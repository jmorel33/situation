/***************************************************************************************************
*
*   situation_impl_decl.h - Internal Declarations, Types, Globals & Data
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file contains all internal type definitions, struct declarations, static globals,
*   macros, forward declarations, and embedded data (shaders) for the Situation library.
*   It is included by situation_impl.h after miniaudio and backend (glad/vulkan) are available.
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_DECL_H
#define SITUATION_IMPL_DECL_H

// =================================================================================
// Internal Macros
// =================================================================================

// Debug logging macro - writes to file for crash diagnosis
#ifdef SITUATION_DEBUG_LOG_ENABLED
#define SIT_DEBUG_LOG(msg, ...) do { \
    FILE* _log = fopen("situation_debug.log", "a"); \
    if (_log) { \
        fprintf(_log, msg "\n", ##__VA_ARGS__); \
        fclose(_log); \
    } \
} while(0)
#else
#define SIT_DEBUG_LOG(msg, ...) do {} while(0)
#endif

// Internal hash map initial capacity
#define SIT_UNIFORM_MAP_INITIAL_CAPACITY 16

// =================================================================================
// Internal Globals & Utility Helpers
// =================================================================================

// Forward declaration (implementation in error handling section)
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail);

#ifdef SITUATION_ENABLE_THREADING
static thrd_t sit_gs_main_thread_id;
static bool sit_gs_thread_id_set = false;
#endif

// [v2.3.24a] Safety Zenith: Atomic Refcounts & Adaptive Policy
static atomic_int sit_render_policy_state = ATOMIC_VAR_INIT(SIT_RENDER_BACKPRESSURE_SPIN);

static uint64_t _SitGetMonotonicTimeNS(void) {
    #if defined(_WIN32)
    static LARGE_INTEGER freq;
    static bool init = false;
    if (!init) { QueryPerformanceFrequency(&freq); init = true; }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000000ULL) / freq.QuadPart);
    #else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    #endif
    return 0;
}

static void _SituationAssertMainThread(const char* file, int line) {
#ifndef NDEBUG
#ifdef SITUATION_ENABLE_THREADING
    if (sit_gs_thread_id_set && !thrd_equal(thrd_current(), sit_gs_main_thread_id)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_VIOLATION, "API call from non-main thread");
        fprintf(stderr, "[Situation DEBUG] Thread violation: %s:%d\n", file, line);
    }
#else
    (void)file; (void)line;
#endif
#endif
}

// =================================================================================
// Internal Struct & Typedef Definitions
// =================================================================================

typedef struct _SituationUniformMapEntry {
    char* key;          // Uniform name
    int32_t value;        // Uniform location
    struct _SituationUniformMapEntry* next; // For handling hash collisions
} _SituationUniformMapEntry;

// The hash map itself
typedef struct _SituationUniformMap {
    _SituationUniformMapEntry** buckets;
    int capacity;
    int count;
} _SituationUniformMap;

typedef struct _SituationComputePipeline {
    uint32_t id; // Public facing ID
#if defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipelineLayout vk_pipeline_layout;
    // --- NEW: Persistent Descriptor Set for this pipeline's layout ---
    // This is useful if the pipeline layout itself needs a descriptor set bound (e.g., for resources specific to the pipeline, not push constants).
    // If the pipeline layout is only for push constants, this might not be needed. Let's assume for now it's needed for general resource binding associated with the pipeline.
    // However, typically, resources like SSBOs/UBOs are bound per-draw/dispatch using their own sets.
    // Binding the pipeline's *layout* itself doesn't usually require a descriptor set.
    // Let's re-evaluate. The pipeline layout defines the interface. Descriptor sets are bound *to* that interface.
    // So, the pipeline layout itself doesn't need a descriptor set.
    // But, if the pipeline uses descriptor sets (which it does), and we want to pre-allocate sets for *those bindings* (like SSBOs/UBOs used by shaders in this pipeline),
    // those sets are owned by the *resources* (SituationBuffer), not the pipeline.
    // The pipeline just defines the layout that those resource sets must conform to.
    // Therefore, SituationComputePipeline likely does NOT need its own descriptor_set member for this fix.
    // The fix is for buffers/images that are *used by* the pipeline.
    // The pipeline layout is used in vkCmdBindDescriptorSets to specify *which* layout the bound sets conform to.
    // Conclusion: No change needed for SituationComputePipeline struct for this specific buffer binding performance fix.
    // The fix is in SituationBuffer and how it's used in binding calls.
    VkShaderModule shader_module; // Keep module handle for cleanup (baked into pipeline, but good practice to track)
#endif
    // Add fields for descriptor set layouts, reflection info if needed for more complex systems
} _SituationComputePipeline;

// Helper struct to track memory allocated during include resolution
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SitIncludeResult {
    shaderc_include_result result;
    char* full_path; // We own this
    char* content;   // We own this
} _SitIncludeResult;
#endif

// --- Internal Joystick State Structure ---
typedef struct {
    bool is_present;
    bool is_gamepad;
    char name[SITUATION_MAX_DEVICE_NAME_LEN];

    // Gamepad-specific state
    unsigned char current_button_state[SITUATION_MAX_JOYSTICK_BUTTONS]; // GLFW_PRESS or GLFW_RELEASE
    unsigned char last_button_state[SITUATION_MAX_JOYSTICK_BUTTONS];
    float axis_state[SITUATION_MAX_JOYSTICK_AXES];
    int axis_count;

} _SituationJoystickState;

typedef struct {
    int jid;
    int event; // GLFW_CONNECTED or GLFW_DISCONNECTED
} _SituationJoystickEvent;


// [v2.3.24b] Integration Zenith: Cross-Backend Render Packet
typedef enum {
    SIT_CMD_DRAW,
    SIT_CMD_DRAW_INDEXED,
    SIT_CMD_DISPATCH,
    SIT_CMD_BARRIER,
    // Add others as needed for generic replay
} SituationRenderCommand;

typedef struct {
    SituationRenderCommand type;
    union {
        struct { uint32_t vertex_count, instance_count, first_vertex, first_instance; } draw;
        struct { uint32_t index_count, instance_count, first_index, vertex_offset, first_instance; } draw_indexed;
        struct { uint32_t group_x, group_y, group_z; } dispatch;
        struct { uint32_t src_stage, dst_stage; } barrier;
    } data;
} SituationRenderPacket;

// [v2.3.22] Momentum Render List (Opaque Impl)
struct SituationRenderList_t {
    SituationRenderPacket* packets; // [v2.3.24b] Typed packets
    size_t packet_count;
    size_t packet_capacity;
    uint8_t* data_buffer;
    size_t data_cursor;
    size_t data_capacity;
    bool is_recording;
    atomic_int in_flight_count;     // [FIX v2.3.27B] Track active usage to prevent reset-while-reading race
};

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SituationSpirvBlob {
    const uint8_t* data;
    size_t size;
    shaderc_compilation_result_t internal_result;
	uint64_t source_hash;  // For hot-reload compare
} _SituationSpirvBlob;
#endif // SITUATION_ENABLE_SHADER_COMPILER

#if defined(SITUATION_USE_OPENGL)
// --- OpenGL State Hardening Helpers ---

#ifndef GL_COMPLETION_STATUS_KHR
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif

// Soft Command Buffer Definitions
typedef enum {
    SIT_OP_BEGIN_RENDER_PASS,
    SIT_OP_END_RENDER_PASS,
    SIT_OP_SET_VIEWPORT,
    SIT_OP_SET_SCISSOR,
    SIT_OP_BIND_PIPELINE,
    SIT_OP_DRAW_MESH,
    SIT_OP_DRAW_QUAD,
    SIT_OP_SET_PUSH_CONSTANT,
    SIT_OP_BIND_DESCRIPTOR_SET,
    SIT_OP_BIND_VERTEX_BUFFER,
    SIT_OP_BIND_INDEX_BUFFER,
    SIT_OP_DRAW,
    SIT_OP_DRAW_INDEXED,
    SIT_OP_PIPELINE_BARRIER,
    SIT_OP_DISPATCH,
    SIT_OP_BIND_COMPUTE_PIPELINE,
    SIT_OP_PRESENT,
    SIT_OP_RENDER_VIRTUAL_DISPLAYS,
    SIT_OP_DRAW_TEXT, // Special op for deferred text drawing
    SIT_OP_DRAW_TEXT_EX, // [v2.3.23] Extended text op
    SIT_OP_UPDATE_BUFFER,
    SIT_OP_SET_VERTEX_ATTRIBUTE,
    SIT_OP_SET_UNIFORM,
    SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING // [Phase 2] Temporary
} SitOpCode;

typedef struct {
    SitOpCode opcode;
    union {
        struct { int display_id; int target_w; int target_h; SituationRenderPassInfo info; } begin_pass;
        struct { float x, y, w, h; } viewport;
        struct { int x, y, w, h; } scissor;
        struct { uint64_t shader_id; } bind_pipeline;
        struct { SituationMesh mesh; uint64_t shader_id; } draw_mesh;
        struct { mat4 model; Vector4 color; Vector4 uv_rect; } draw_quad;
        struct { uint32_t offset; size_t size; size_t data_offset; } push_constant;
        struct { uint32_t set_index; uint64_t resource_id; int resource_type; size_t offset; size_t size; uint32_t usage_flags; } bind_desc; // [Phase 2] Added size and usage_flags for Ring Buffer
        struct { uint32_t binding; uint64_t buffer_id; size_t offset; size_t stride; } bind_vbo;
        struct { uint64_t buffer_id; } bind_ibo;
        struct { uint32_t v_count, i_count, first_v, first_i; } draw;
        struct { uint32_t idx_count, inst_count, first_idx; int32_t v_offset; uint32_t first_inst; } draw_indexed;
        struct { uint32_t src, dst; } barrier;
        struct { uint32_t x, y, z; } dispatch;
        struct { SituationTexture texture; int target_w; int target_h; } present;
        struct { SituationFont font; Vector2 pos; ColorRGBA color; size_t text_offset; } draw_text; // Store text in data_buffer
        struct { SituationFont font; Vector2 pos; float fontSize; float spacing; ColorRGBA color; size_t text_offset; } draw_text_ex; // [v2.3.23]
        struct { uint64_t buffer_id; size_t offset; size_t size; size_t data_offset; } update_buffer;
        struct { uint32_t location; int size; int type; int normalized; size_t offset; } set_vertex_attr;
        struct { uint64_t shader_id; GLint location; int type; size_t data_offset; } set_uniform;
    } args;
} SitCommandPacket;

typedef struct {
    SitCommandPacket* packets;
    size_t packet_count;
    size_t packet_capacity;
    uint8_t* data_buffer;
    size_t data_cursor;
    size_t data_capacity;

    uint64_t current_recording_shader_id; // [Critical Fix] Track current shader for MDI consistency

    // [FIX v2.3.27B] Circuit breaker for OOM handling
    bool is_broken;
} SituationGLSoftCommandBuffer;

typedef struct {
    GLint program;
    GLint vao;
    GLint fbo;
    GLboolean blend;
    GLint blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
    GLint blend_equ_rgb, blend_equ_alpha;
    GLboolean depth_test;
    GLboolean cull_face;
    GLboolean scissor_test;
} _SitGLStateBackup;

// [Phase 2.5] Lazy VAO Cache Entry
typedef struct _SitGLVaoCacheEntry {
    uint64_t mesh_id;               // Key: The mesh ID (which matches VBO ID)
    GLuint vao_id;                  // Value: The VAO configured for this mesh
    struct _SitGLVaoCacheEntry* next; // Chaining for collisions
} _SitGLVaoCacheEntry;
#endif

#if defined(SITUATION_USE_VULKAN)
// --- VULKAN IMPLEMENTATION SECTION ---

// --- Internal Vulkan Helper Data Structures ---
typedef struct {
    uint32_t graphics_family;
    uint32_t present_family;
    uint32_t compute_family;
    bool graphics_family_has_value;
    bool present_family_has_value;
    bool compute_family_has_value;
} _SituationQueueFamilyIndices;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    uint32_t format_count;
    VkPresentModeKHR* present_modes;
    uint32_t present_mode_count;
} _SituationVulkanSwapchainSupportDetails;

// --- Vulkan Graveyard Definition ---
typedef struct _SituationVKGraveyard {
    VkBuffer* buffers;
    VmaAllocation* buffer_allocations;
    int buffer_count;
    int buffer_capacity;

    VkImage* images;
    VmaAllocation* image_allocations;
    VkImageView* image_views;
    VkSampler* samplers;
    int image_count;
    int image_capacity;

    VkDescriptorSet* descriptor_sets;
    VkDescriptorPool* descriptor_pools; // [NEW] Track pool for freeing
    int descriptor_set_count;
    int descriptor_set_capacity;

    VkPipeline* pipelines;
    VkPipelineLayout* pipeline_layouts;
    int pipeline_count;
    int pipeline_capacity;

    VkFramebuffer* framebuffers;
    int framebuffer_count;
    int framebuffer_capacity;

    VkRenderPass* render_passes;
    int render_pass_count;
    int render_pass_capacity;
} _SituationVKGraveyard;

typedef struct {
    VkBuffer buffer;
    VmaAllocation allocation;
    uint8_t* mapped_data;
    size_t capacity;
    size_t cursor;
} _SituationStagingBuffer;

/**
 * @brief Deterministic key for the Vulkan Render Pass cache.
 */
typedef union {
    uint32_t key;
    struct {
        uint32_t target_type      : 1;
        uint32_t color_load_op    : 2;
        uint32_t depth_load_op    : 2;
        uint32_t stencil_load_op  : 2;
        uint32_t color_store_op   : 2;
        uint32_t depth_store_op   : 2;
        uint32_t stencil_store_op : 2;
        uint32_t _padding         : 19;
    } bits;
} _SituationRenderPassKey;

typedef struct {
    uint32_t key;
    VkRenderPass handle;
} _SituationCachedRenderPass;

static inline uint32_t _SituationHashRenderPassKey(const SituationRenderPassInfo* info, bool is_main_window) {
    _SituationRenderPassKey key = {0};
    key.bits.target_type = is_main_window ? 0 : 1;
    key.bits.color_load_op = info->color_attachment.loadOp;
    key.bits.depth_load_op = info->depth_attachment.loadOp;
    key.bits.stencil_load_op = info->stencil_attachment.loadOp;
    key.bits.color_store_op = info->color_attachment.storeOp;
    key.bits.depth_store_op = info->depth_attachment.storeOp;
    key.bits.stencil_store_op = info->stencil_attachment.storeOp;
    return key.key;
}


/**
 * @brief [INTERNAL] Vulkan backend state container.
 * @details Holds the core Vulkan handles (Instance, Device, Queue) and the memory allocator (VMA).
 *          It also manages the swapchain, per-frame synchronization objects (Semaphores, Fences),
 *          and the dynamic descriptor pool manager.
 */
 typedef struct {
    // -------------------------------------------------------------------------
    // Core API Objects
    // -------------------------------------------------------------------------
    VkInstance instance;                        // The Vulkan instance handle
    VkDebugUtilsMessengerEXT debug_messenger;   // Handle for the debug callback (validation layers)
    VkSurfaceKHR surface;                       // The window surface handle
    VkPhysicalDevice physical_device;           // Handle to the selected physical GPU
    VkDevice device;                            // The logical device handle
    VmaAllocator vma_allocator;                 // The VMA memory allocator instance

    // -------------------------------------------------------------------------
    // Queues
    // -------------------------------------------------------------------------
    VkQueue graphics_queue;                     // Queue handle for graphics operations
    VkQueue compute_queue;                      // Queue handle for compute operations
    VkQueue present_queue;                      // Queue handle for presentation operations
    uint32_t graphics_family_index;             // Family index of the graphics queue
    uint32_t compute_family_index;              // Family index of the compute queue
    uint32_t present_family_index;              // Family index of the present queue

    // -------------------------------------------------------------------------
    // Swapchain & Presentation
    // -------------------------------------------------------------------------
    VkSwapchainKHR swapchain;                   // The active swapchain handle
    VkFormat swapchain_image_format;            // The pixel format of swapchain images
    VkExtent2D swapchain_extent;                // The resolution of swapchain images
    VkImage* swapchain_images;                  // Array of handles to swapchain images
    VkImageView* swapchain_image_views;         // Array of views for swapchain images
    uint32_t swapchain_image_count;             // Number of images in the swapchain

    VkRenderPass main_window_render_pass;       // The default render pass for the window
    VkFramebuffer* main_window_framebuffers;    // Array of framebuffers for the swapchain

    VkImage depth_image;                        // The depth buffer image
    VmaAllocation depth_image_memory;           // Memory allocation for the depth buffer
    VkImageView depth_image_view;               // View for the depth buffer
    VkFormat depth_format;                      // Format of the depth buffer

    // -------------------------------------------------------------------------
    // Per-Frame Synchronization & State
    // -------------------------------------------------------------------------
    VkCommandPool command_pool;                 // Main pool for allocating command buffers
    VkCommandPool compute_command_pool;         // Pool for compute command buffers [NEW]
    uint32_t max_frames_in_flight;              // Number of frames processed concurrently (e.g., 2)
    VkCommandBuffer* command_buffers;           // Array of per-frame command buffers
    VkCommandBuffer* compute_command_buffers;   // Array of per-frame compute command buffers [NEW]
    VkSemaphore* image_available_semaphores;    // Semaphores signaled when image is acquired
    VkSemaphore* render_finished_semaphores;    // Semaphores signaled when rendering completes
    VkSemaphore* compute_finished_semaphores;   // Semaphores signaled when compute queue completes
    VkFence* in_flight_fences;                  // Fences signaled when frame execution finishes
    uint32_t current_frame_index;               // Index of the frame currently being recorded (0..max-1)
    uint32_t current_image_index;               // Index of the swapchain image currently acquired
    uint32_t last_presented_image_index;        // Index of the last presented image
    bool framebuffer_resized;                   // Flag indicating window resize occurred
    bool needs_compute_wait;                    // [v2.3.24b] Sync flag: Graphics must wait for Compute

    // -------------------------------------------------------------------------
    // Descriptor Management
    // -------------------------------------------------------------------------
    VkDescriptorPool persistent_descriptor_pool;// Initial pool for long-lived resources
    //VkDescriptorPool asset_descriptor_pool;     // [VULKAN] Separate pool for assets (Textures/Models)
    VkDescriptorPool descriptor_pool;           // Current active pool for allocations

    // Dynamic Pool Manager
    struct {
        VkDescriptorPool* pools;                // Dynamic array of descriptor pools
        int count;                              // Number of pools created
        int capacity;                           // Capacity of the pools array
        int current_index;                      // Index of the currently active pool
    } descriptor_manager;

    // Standard Layouts
    VkDescriptorSetLayout ubo_layout;           // Layout for generic UBOs
    VkDescriptorSetLayout dynamic_ubo_layout;   // Layout for dynamic UBOs
    VkDescriptorSetLayout ssbo_layout;          // Layout for generic SSBOs
    VkDescriptorSetLayout view_data_ubo_layout; // Layout for the global View UBO
    VkDescriptorSetLayout image_sampler_layout; // Layout for combined image samplers
    VkDescriptorSetLayout text_sampler_layout;  // Layout for text textures (binding 0)
    VkDescriptorSetLayout bindless_descriptor_layout; // [Bindless] Layout for global texture array (binding 0)
    VkDescriptorSetLayout storage_buffer_layout;// Layout for storage buffers (redundant with ssbo?)
    VkDescriptorSetLayout storage_image_layout; // Layout for storage images
    VkDescriptorSetLayout compute_sampler_layout;
    VkDescriptorSetLayout composite_dual_sampler_layout;  // Bindings 4 and 5 for composite shader // Layout for compute shader samplers (binding 0, compute stage)

    // Compute Pipeline Layout Cache
    VkPipelineLayout current_pipeline_layout_for_push_constants; // Last bound graphics layout
    VkPipelineLayout current_compute_pipeline_layout;            // Last bound compute layout
    VkPipelineLayout compute_layouts[8];                         // Pre-created standard layouts

    // -------------------------------------------------------------------------
    // Internal Renderers Resources
    // -------------------------------------------------------------------------

    // Dynamic Vertex Buffers (Optimization for Text/UI)
    VkBuffer dynamic_vbo[SITUATION_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation dynamic_vbo_alloc[SITUATION_MAX_FRAMES_IN_FLIGHT];
    void* dynamic_vbo_mapped[SITUATION_MAX_FRAMES_IN_FLIGHT];
    size_t dynamic_vbo_cursor;                                   // Current byte offset for the current frame
    size_t dynamic_vbo_capacity;                                 // Total size in bytes (e.g., 512KB)

    VkPipeline quad_pipeline;                                    // Pipeline for 2D Quad renderer
    VkPipelineLayout quad_pipeline_layout;                       // Layout for 2D Quad renderer
    VkBuffer quad_vertex_buffer;                                 // Vertex buffer for unit quad
    VmaAllocation quad_vertex_buffer_memory;                     // Memory for quad vertex buffer

    VkPipeline text_pipeline;                                    // Pipeline for Batched Text renderer
    VkPipelineLayout text_pipeline_layout;                       // Layout for Batched Text renderer

    VkPipeline vd_compositing_pipeline;                          // Pipeline for simple VD composition
    VkPipelineLayout vd_compositing_pipeline_layout;             // Layout for simple VD composition
    VkPipeline advanced_compositing_pipeline;                    // Pipeline for advanced blend modes
    VkPipelineLayout advanced_compositing_pipeline_layout;       // Layout for advanced blend modes

    // [PIPELINE STATE]
    VkPipeline current_legacy_pipeline;
    VkPipeline current_pbr_pipeline;
    struct _SituationShaderSlot* current_bound_shader_slot;      // For stride-based pipeline selection at draw time

    // Global UBOs (Per-Frame)
    VkBuffer* view_proj_ubo_buffer;                              // Array of View UBOs
    VmaAllocation* view_proj_ubo_memory;                         // Array of View UBO memory
    void** view_proj_ubo_mapped;                                 // Array of mapped View UBO memory
    VkDescriptorSet* view_proj_ubo_descriptor_set;               // Array of View UBO descriptor sets

    // Screen Copy Resources (for advanced blending)
    VkImage screen_copy_image;                                   // Image for copying framebuffer
    VmaAllocation screen_copy_memory;                            // Memory for screen copy
    VkImageView screen_copy_view;                                // View for screen copy
    VkDescriptorSet screen_copy_descriptor_set;                  // Descriptor set for reading screen copy

    VkDescriptorPool screen_copy_descriptor_pool;                // [FIX v2.3.27B] Track the pool that owns the screen copy set

    // [Bindless] Global Descriptor Set
    VkDescriptorSet global_bindless_set;
    VkDescriptorPool global_bindless_pool;

    _SituationStagingBuffer staging_buffers[SITUATION_MAX_FRAMES_IN_FLIGHT];     // [NEW] Per-Frame Staging Buffers

    // --- Graveyard (Deferred Deletion Queue) ---
    struct _SituationVKGraveyard* graveyards;                    // Array of deletion queues (one per frame in flight)

    // --- Threading Signals ---
    atomic_bool recreate_swapchain_request;                      // Signal from Render Thread to Main Thread
    bool swapchain_valid; // [FIX v2.3.27B]
    uint32_t acquired_image_indices[SITUATION_MAX_FRAMES_IN_FLIGHT]; // Image index for each frame slot

    uint64_t staging_buffer_size; // Configured staging buffer size

    // Render Pass Cache
    _SituationCachedRenderPass render_pass_cache[32];
    uint32_t render_pass_cache_count;

} _SituationVulkanState;

#elif defined(SITUATION_USE_OPENGL)
// [Phase 2.5] Deferred Deletion Queue (Graveyard)
typedef struct {
    ma_mutex lock;                  // Thread safety for Main (Push) vs Render (Pop)

    uint64_t* mesh_ids_to_clean;    // Mesh IDs (to clean VAO cache)
    size_t mesh_count;
    size_t mesh_capacity;

    GLuint* buffers_to_delete;      // VBO/EBO/UBO IDs
    size_t buffer_count;
    size_t buffer_capacity;

    GLuint* textures_to_delete;     // Texture IDs
    size_t texture_count;
    size_t texture_capacity;

    // We can add shaders/programs if needed later
} _SituationGLGraveyard;

// [Phase 4] Multi-Draw Indirect Structures
typedef struct {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t first;
    uint32_t baseInstance;
} SitDrawArraysIndirectCommand;

typedef struct {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t baseInstance;
} SitDrawElementsIndirectCommand;

/**
 * @brief [INTERNAL] OpenGL backend state container.
 * @details Holds all global OpenGL objects and state variables managed by the library.
 *          This includes internal shaders (Quad, Virtual Display), global buffers (UBOs),
 *          and caching variables for optimizing state changes.
 */
#if defined(SITUATION_USE_OPENGL)

#define SITUATION_MAX_VIRTUAL_TEXTURE_UNITS 32

typedef struct _SituationVirtualTextureSlot {
    uint32_t texture_slot_index;     // The actual GL texture unit (0-31)
    GLuint gl_texture_id;            // The GL ID of the texture currently bound here
    uint64_t last_used_counter;      // For LRU eviction
    bool is_active;                  // Is this slot currently holding a valid texture?
} _SituationVirtualTextureSlot;

typedef struct _SituationVirtualBindlessStats {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
} _SituationVirtualBindlessStats;

#endif // SITUATION_USE_OPENGL

 typedef struct {
    // -------------------------------------------------------------------------
    // Internal Renderers (Virtual Display & Quad)
    // -------------------------------------------------------------------------
    GLuint vd_shader_program_id;                // Shader program for basic Virtual Display composition
    GLuint vd_quad_vao;                         // Private VAO for full-screen quad rendering
    GLuint vd_quad_vbo;                         // Private VBO for full-screen quad geometry
    mat4 vd_ortho_projection;                   // Orthographic projection matrix matching the window size

    GLuint composite_shader_program_id;         // Shader program for advanced blending modes
    GLuint composite_copy_texture_id;           // Texture used to copy the framebuffer for advanced blending

    // Screenshot capture (pre-swap readback for reliable pixel capture)
    GLuint screenshot_pbo;                      // PBO for async readback before swap
    uint8_t* screenshot_buffer;                 // CPU-side copy of last frame's pixels
    int screenshot_width;                       // Width of captured frame
    int screenshot_height;                      // Height of captured frame
    bool screenshot_valid;                      // True if screenshot_buffer has valid data

    GLuint quad_shader_program;                 // Shader program for the 2D Quad/Text renderer
    GLuint quad_vao;                            // Private VAO for 2D quads
    GLuint quad_vbo;                            // Private VBO for 2D quads

    GLuint text_shader_program;                 // Shader program for Batched Text renderer
    GLuint text_vao;                            // Private VAO for Batched Text
    GLuint text_vbo;                            // Private dynamic VBO for Batched Text

    // -------------------------------------------------------------------------
    // Global Resources & State
    // -------------------------------------------------------------------------
    GLuint view_data_ubo_id;                    // Handle to the global View/Projection UBO
    GLuint global_vao_id;                       // The "Public" VAO active during user rendering commands
    GLuint mesh_vao_id;                         // [2.3.19] Shared VAO for standard meshes (PBR layout)
    GLuint current_program_id;                  // Cache of the currently bound shader program ID

    // Shadow State (Tracks what we *think* the driver state is)
    GLuint current_bound_texture_id; // [v2.3.31] Track bound texture for legacy/quad draws
    GLuint current_vao_id;
    GLuint current_fbo_id;
    int    blend_enabled;
    GLenum blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
    GLenum blend_eq_rgb, blend_eq_alpha;
    int    depth_test_enabled;
    int    cull_face_enabled;
    int    scissor_test_enabled;

    #if defined(SITUATION_ENABLE_SHADER_COMPILER)
    bool arb_spirv_available;                   // True if GL_ARB_gl_spirv extension is supported
    #endif
    GLenum last_error;                          // Cached result of last glGetError()
    bool shadow_state_dirty;                    // [2.3.14A] Flag to indicate external state changes

    // [Phase 1] Soft Command Buffer for Deferred Rendering
    SituationGLSoftCommandBuffer soft_buffers[SITUATION_MAX_FRAMES_IN_FLIGHT];

    // [Phase 1.5] Persistent Ring Buffer (Zero-Copy)
    GLuint ring_buffer_id;
    void* ring_data_ptr;
    size_t ring_size;
    atomic_size_t ring_head;
    GLsync* ring_fences;        // Array of fence objects (Triple buffering usually)
    size_t ring_fence_count;
    uint32_t current_fence_index;

    // [Phase 2] Context Sharing for Threaded Rendering
    GLFWwindow* loader_window; // Invisible window for main-thread resource loading

    // [Phase 2.5] Lazy VAO Cache & Graveyard
    _SitGLVaoCacheEntry* vao_cache[256]; // Simple hash table
    _SituationGLGraveyard graveyards[SITUATION_MAX_FRAMES_IN_FLIGHT];
    GLsync frame_fences[SITUATION_MAX_FRAMES_IN_FLIGHT];

    // [Phase 4] Multi-Draw Indirect
    GLuint mdi_buffer_id;
    void* mdi_data_ptr;
    size_t mdi_ring_size;
    atomic_size_t mdi_ring_head;

    // [Phase 5] Virtual Bindless Fallback
    _SituationVirtualTextureSlot virtual_texture_slots[SITUATION_MAX_VIRTUAL_TEXTURE_UNITS];
    _SituationVirtualBindlessStats virtual_stats;
    uint64_t virtual_lru_counter;
    GLint current_virtual_loc; // [Phase 5] Cache for virtual bindless uniform location
} _SituationGLState;
#endif // SITUATION_USE_OPENGL

/**
 * @brief [INTERNAL] Keyboard state container.
 * @details Tracks the current/previous state of all keys, handles the event-driven queue
 *          for polling APIs (GetKeyPressed), and manages modifier/lock key flags.
 */
 typedef struct {
    // -------------------------------------------------------------------------
    // State Tracking
    // -------------------------------------------------------------------------
    bool current_state[GLFW_KEY_LAST + 1];      // State of each key in the current frame (true = held)
    bool last_state[GLFW_KEY_LAST + 1];         // State of each key in the previous frame
    bool down_this_frame[GLFW_KEY_LAST + 1];    // Flag set if key was pressed down during this frame
    bool up_this_frame[GLFW_KEY_LAST + 1];      // Flag set if key was released during this frame
    bool scancode_state[SITUATION_MAX_SCANCODES]; // State of each scancode (true = held)

    // -------------------------------------------------------------------------
    // Event Queues (Thread-Safe Ring Buffers)
    // -------------------------------------------------------------------------
    int pressed_queue[SITUATION_KEY_QUEUE_MAX];             // Queue of raw key codes pressed
    int scancode_queue[SITUATION_KEY_QUEUE_MAX];            // Parallel queue of scancodes
    uint32_t pressed_head;                                  // Write index for key queue
    uint32_t pressed_tail;                                  // Read index for key queue

    unsigned int char_queue[SITUATION_CHAR_QUEUE_MAX];      // Queue of Unicode codepoints (text input)
    uint32_t char_head;                                     // Write index for char queue
    uint32_t char_tail;                                     // Read index for char queue

    ma_mutex event_queue_mutex;                             // Mutex protecting concurrent access to ring buffers

    // -------------------------------------------------------------------------
    // Modifiers & Callbacks
    // -------------------------------------------------------------------------
    int modifier_state;                         // Bitmask of currently active modifier keys (Shift, Ctrl, Alt)
    int lock_key_state;                         // Bitmask of currently active lock keys (Caps, Num)
    bool is_scroll_lock_on;                     // Toggle state of the Scroll Lock key
    SituationKeyCallback key_callback;          // User-defined callback invoked on key events
    void* key_callback_user_data;               // User context pointer for the key callback
} _SituationKeyboardState;

/**
 * @brief [INTERNAL] Mouse state container.
 * @details Tracks cursor position (with virtual scaling/offset), button states, and scroll wheel delta.
 *          Includes a ring buffer for buffering rapid click events.
 */
 typedef struct {
    // -------------------------------------------------------------------------
    // Position & Movement
    // -------------------------------------------------------------------------
    vec2 current_pos;                           // Current cursor position in window client coordinates
    vec2 last_pos;                              // Cursor position from the previous frame
    vec2 offset;                                // Virtual offset applied to raw coordinates
    vec2 scale;                                 // Virtual scale factor applied to raw coordinates
    float wheel_move_y;                         // Vertical scroll delta for this frame
    float wheel_move_x;                         // Horizontal scroll delta for this frame

    // -------------------------------------------------------------------------
    // Button State
    // -------------------------------------------------------------------------
    bool current_button_state[GLFW_MOUSE_BUTTON_LAST + 1];  // State of each button (true = held)
    bool last_button_state[GLFW_MOUSE_BUTTON_LAST + 1];     // State of each button in the previous frame
    bool button_down_this_frame[GLFW_MOUSE_BUTTON_LAST + 1];// Flag set if button was pressed this frame
    bool button_up_this_frame[GLFW_MOUSE_BUTTON_LAST + 1];  // Flag set if button was released this frame

    // -------------------------------------------------------------------------
    // Event Queue (Thread-Safe Ring Buffer)
    // -------------------------------------------------------------------------
    int button_queue[SITUATION_KEY_QUEUE_MAX];  // Queue of mouse button press events
    uint32_t button_head;                       // Write index for button queue
    uint32_t button_tail;                       // Read index for button queue

    ma_mutex mutex;                             // Mutex protecting shared state (position, buttons, queue)

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------
    SituationMouseButtonCallback button_callback;           // Callback for button press/release
    void* button_callback_user_data;                        // Context for button callback
    SituationCursorPosCallback cursor_pos_callback;         // Callback for cursor movement
    void* cursor_pos_callback_user_data;                    // Context for cursor callback
    SituationScrollCallback scroll_callback;                // Callback for scroll wheel events
    void* scroll_callback_user_data;                        // Context for scroll callback
} _SituationMouseState;

/**
 * @brief [INTERNAL] Joystick and Gamepad manager.
 * @details Manages the connection state, axis values, and button states for up to 16 controllers.
 *          Handles both raw joystick input and mapped Gamepad input (SDL2 style).
 */
 typedef struct {
    // -------------------------------------------------------------------------
    // Device State
    // -------------------------------------------------------------------------
    _SituationJoystickState state[SITUATION_MAX_JOYSTICKS]; // Array of state structs for each slot

    // -------------------------------------------------------------------------
    // Connection Event Queue (Thread-Safe)
    // -------------------------------------------------------------------------
    _SituationJoystickEvent event_queue[SITUATION_MAX_JOYSTICKS]; // Queue of connect/disconnect events
    int event_queue_count;                                        // Number of pending events in the queue
    ma_mutex event_queue_mutex;                                   // Mutex protecting the event queue

    // -------------------------------------------------------------------------
    // Button Press Queue (Ring Buffer)
    // -------------------------------------------------------------------------
    int button_pressed_queue[SITUATION_KEY_QUEUE_MAX];      // Queue of gamepad button press events
    uint32_t button_head;                                   // Write index for button queue
    uint32_t button_tail;                                   // Read index for button queue

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------
    SituationJoystickCallback callback;                     // User callback for connection events
    void* callback_user_data;                               // User context for connection callback
} _SituationJoystickManager;

// [NEW] Dedicated Input State Container
typedef struct {
    _SituationKeyboardState keyboard;
    _SituationMouseState mouse;
    _SituationJoystickManager joysticks;
    GLFWcursor* cursors[16];
    int cursor_count;
} _SituationInputState;

// --- Internal Sound Data (Moved from API) ---
typedef struct _SituationSound {
    ma_decoder decoder;
    void* preloaded_data;
    bool is_preloaded;
    ma_data_converter converter;
    bool is_initialized;
    bool converter_initialized;
    bool is_looping;
    bool is_streamed;
    uint64_t cursor_frames;
    uint64_t total_frames;
    atomic_float volume;
    atomic_float pan;
    atomic_float pitch;
    float _internal_pitch_tracker;
    SituationStreamReadCallback stream_read_cb;
    SituationStreamSeekCallback stream_seek_cb;
    void* stream_user_data;
    struct {
        bool filter_enabled;
        ma_biquad biquad;
        SituationFilterType filter_type;
        float filter_cutoff_hz;
        float filter_q;
        bool echo_enabled;
        struct {
            ma_delay delay;
            uint32_t channels;
            bool is_initialized;
            float current_feedback;
            float current_wet;
            float current_dry;
            float target_feedback;
            float target_wet;
            float target_dry;
        } echo;
        float echo_delay_sec;
        float echo_feedback;
        float echo_wet_mix;
        bool reverb_enabled;
        void* reverb_state;
        float reverb_room_size;
        float reverb_damping;
        float reverb_wet_mix;
        float reverb_dry_mix;
    } effects;
    SituationAudioProcessorCallback* processors;
    void** processor_user_data;
    int processor_count;

    // [Phase 1] Graph Support
    bool is_graph_managed;
    ma_data_source_node graph_node;
} _SituationSound;


// Redefine Sound Slot
typedef struct _SituationSoundSlot {
    bool is_active;
    uint32_t generation;
    _SituationSound sound_data; // Embedded data
    char* source_path; // For hot-reload
    long mod_time;
} _SituationSoundSlot;

typedef enum {
    SIT_ENV_IDLE = 0,
    SIT_ENV_ATTACK,     // Volume rising 0.0 -> 1.0
    SIT_ENV_DECAY,      // Volume falling 1.0 -> Sustain Level
    SIT_ENV_SUSTAIN,    // Volume holding at Sustain Level
    SIT_ENV_RELEASE     // Volume falling Sustain Level -> 0.0
} SituationEnvelopeState;


typedef struct {
    bool active;
    uint32_t generation;                // For handle validation (prevents reuse bugs)

    union {
        ma_waveform waveform;
        ma_noise    noise;
    };

    ma_format format;                   // Shared: f32
    uint32_t channels;                  // Shared: 2 (stereo)

    float volume_peak;
    float pan;

    SituationEnvelopeState state;
    uint64_t cursor_frames;

    uint64_t t_attack;
    uint64_t t_decay;
    uint64_t t_hold;                    // UINT64_MAX if infinite sustain
    uint64_t t_release;

    float level_sustain;

    SituationWaveType wave_type;        // Needed to know if we're using noise or waveform
    double trigger_timestamp_ms;        // [LATENCY] When this tone was triggered
} SituationTone;


 typedef struct {
    // -------------------------------------------------------------------------
    // Audio Subsystem (MiniAudio)
    // -------------------------------------------------------------------------
    // Handle Pool
    _SituationSoundSlot sound_pool[SITUATION_MAX_LOADED_SOUNDS];
    mtx_t pool_mutex;       // Protects allocation/deallocation of slots

    ma_context miniaudio_context;                         // The main MiniAudio context
    ma_device miniaudio_device;                           // The primary playback device
    bool is_miniaudio_context_initialized;                // True if the context was successfully created
    bool is_miniaudio_device_active;                      // True if the playback device is initialized
    bool is_miniaudio_device_internally_paused;           // True if playback is temporarily suspended (e.g. minimized)
    int current_miniaudio_device_audioinfo_id;         // ID of the currently selected output device

    // [v2.4] Dynamic Mixing Queue
    // Replaces fixed `queued_sounds[32]` with resizable array.
    _SituationSound** active_voices;                    // Dynamic array of active sounds being mixed
    int active_voice_count;                            // Number of active sounds
    int active_voice_capacity;                         // Current capacity of the dynamic array
    uint32_t config_max_voices;                        // Configured hard limit (0 = Unlimited)

    // [v2.4] Audio Thread Snapshot Buffer
    // Persistent scratch buffer to avoid stack allocation or malloc on the audio thread.
    _SituationSound** snapshot_buffer;                  // Dynamic array for mixer snapshot
    int snapshot_buffer_capacity;                      // Capacity of the snapshot buffer

    // [FIX v2.3.27B] Use C11 Recursive Mutex to prevent deadlocks when
    // API functions are called from within audio callbacks/processors.
    mtx_t audio_queue_mutex;                                       // Mutex protecting the sound queue

    // Pre-allocated temp buffers for the audio callback (avoids SIT_MALLOC on audio thread)
    float* audio_callback_decoder_temp_buffer;            // Scratch buffer for decoding PCM
    float* audio_callback_effects_temp_buffer;            // Scratch buffer for processing effects
    float* audio_callback_converter_temp_buffer;          // Scratch buffer for sample rate conversion
    uint32_t audio_callback_temp_buffer_frames_capacity;  // Size of the scratch buffers in frames

    // -------------------------------------------------------------------------
    // Audio Capture (Recording)
    // -------------------------------------------------------------------------
    ma_device capture_device;                                 // The primary recording device
    bool is_capture_device_active;                            // True if recording is currently active
    SituationAudioCaptureCallback capture_callback;           // User callback for processing recorded audio
    void* capture_user_data;                                  // User context pointer for the capture callback

    bool audio_capture_on_main_thread;                        // Configuration flag for main-thread dispatch

    // [Phase 5] Capture Device Binding
    ma_device_id active_capture_device_id;
    bool active_capture_device_id_set;

    float* audio_capture_queue;                               // Ring buffer for transferring audio to main thread
    size_t audio_capture_write_head;                          // Write index for the capture ring buffer
    size_t audio_capture_read_head;                           // Read index for the capture ring buffer
    size_t audio_capture_queue_capacity;                      // Total size of the capture ring buffer
    ma_mutex audio_capture_mutex;                             // Mutex protecting the capture ring buffer

    float* audio_capture_temp_buffer;                         // Permanent scratch buffer for main thread audio capture polling
    size_t audio_capture_temp_buffer_capacity;                // Capacity of the permanent scratch buffer in floats

    // [NEW] Safety flag for Snapshotting
    atomic_bool is_processing_snapshot;

    // [FIX v2.4.38] Audio callback race condition guard.
    // Set to true at the END of audio init, after all state (device registry, default graph) is ready.
    // The audio callback returns silence if this is false.
    atomic_bool audio_ready;

    // Resonance Module State
    SituationTone tone_pool[SITUATION_MAX_TONES];
    uint32_t tone_generations[SITUATION_MAX_TONES];

    // Audio Output Monitoring (for visualization)
    void (*output_monitor_callback)(const float* samples, uint32_t frame_count, void* user_data);
    void* output_monitor_user_data;

    // [Phase H] Node Graph Integration
    SituationAudioGraph*    active_graph;    // Currently active processing graph (NULL = legacy path)
    SituationAudioGraph*    default_graph;   // Auto-created minimal graph (Sound Source + Tone Synth → Mixer)
} _SituationAudioState;

//==================================================================================
// --- Internal Resource Slots ---
//==================================================================================
typedef struct _SituationShaderSlot {
    bool is_active;
    uint32_t generation;
#if defined(SITUATION_USE_OPENGL)
    GLuint gl_program_id;
    struct _SituationUniformMap* uniform_map;
    GLuint gl_pending_program_id;
    bool gl_is_linking;
#elif defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipeline vk_pipeline_legacy;
    VkPipeline vk_pipeline_simple;       // Position-only vertex layout (stride = 3*float)
    VkPipelineLayout vk_pipeline_layout;
#endif
    // Hot-Reload Metadata
    char* vs_path;
    char* fs_path;
    long vs_mod_time;
    long fs_mod_time;
} _SituationShaderSlot;

typedef struct _SituationMeshSlot {
    bool is_active;
    uint32_t generation;
    int index_count;
    int vertex_count;
    size_t vertex_stride;
#if defined(SITUATION_USE_OPENGL)
    GLuint vbo_id;
    GLuint ebo_id;
#elif defined(SITUATION_USE_VULKAN)
    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_memory;
    VkBuffer index_buffer;
    VmaAllocation index_buffer_memory;
#endif
} _SituationMeshSlot;

typedef struct _SituationBufferSlot {
    bool is_active;
    uint32_t generation;
    size_t size_in_bytes;
    SituationBufferUsageFlags usage_flags;
#if defined(SITUATION_USE_OPENGL)
    GLuint gl_buffer_id;
    uint64_t dynamic_offset;
    uint32_t dynamic_frame_index;
#elif defined(SITUATION_USE_VULKAN)
    VkBuffer vk_buffer;
    VmaAllocation vma_allocation;
    VkBufferUsageFlags vk_usage_flags;
    VkDescriptorSet descriptor_set;
    VkDescriptorPool descriptor_pool;
#endif
} _SituationBufferSlot;

typedef struct _SituationComputePipelineSlot {
    bool is_active;
    uint32_t generation;
#if defined(SITUATION_USE_OPENGL)
    GLuint gl_program_id;
#elif defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipelineLayout vk_pipeline_layout;
    VkShaderModule shader_module;
#endif
    // Hot-Reload Metadata
    char* source_path;
    SituationComputeLayoutType layout_type;
    long mod_time;
} _SituationComputePipelineSlot;

typedef struct _SituationModelSlot {
    bool is_active;
    uint32_t generation;
    int mesh_count;
    SituationModelMesh* meshes;
    int texture_count;
    SituationTexture* all_model_textures;
    char* source_path;
    long mod_time;
} _SituationModelSlot;

// --- Internal Texture Slot Definition ---
typedef struct _SituationTextureSlot {
    bool is_active;
    uint32_t generation; // Increments every time this slot is recycled
    int width;
    int height;
    uint64_t bindless_handle;

    char* source_path;
    long mod_time;

    // Backend Resources
#if defined(SITUATION_USE_VULKAN)
    VkImage image;
    VkFormat format;                    // Texture format (UNORM or SRGB)
    VkImageView image_view;
    VkSampler sampler;
    VmaAllocation allocation;
    VkDescriptorSet descriptor_set;
    VkDescriptorPool descriptor_pool; // Owner pool
#elif defined(SITUATION_USE_OPENGL)
    GLuint gl_texture_id;
    GLenum internal_format;             // Texture internal format (GL_RGBA8 or GL_SRGB8_ALPHA8)
    uint64_t gl_bindless_handle; // [Phase 3] Bindless Handle
#endif
} _SituationTextureSlot;


// Render State Container
typedef struct {
    // -------------------------------------------------------------------------
    // Graphics Backend State
    // -------------------------------------------------------------------------
    SituationRendererType renderer_type;                      // The active rendering API (OpenGL or Vulkan)
    bool debug_draw_command_issued_this_frame;                // Debug flag to detect illegal state changes during a frame

#if defined(SITUATION_USE_VULKAN)
    _SituationVulkanState vk;                                 // Encapsulated Vulkan-specific state handles
#elif defined(SITUATION_USE_OPENGL)
    _SituationGLState gl;                                     // Encapsulated OpenGL-specific state handles
#endif

    // -------------------------------------------------------------------------
    // Virtual Display Subsystem
    // -------------------------------------------------------------------------
    SituationVirtualDisplay virtual_display_slots[SITUATION_MAX_VIRTUAL_DISPLAYS]; // Static pool for virtual displays
    bool virtual_display_slots_used[SITUATION_MAX_VIRTUAL_DISPLAYS];               // Allocation map for the virtual display pool
    int active_virtual_display_count;                         // Count of currently allocated virtual displays

    // -------------------------------------------------------------------------
    // Profiling
    // -------------------------------------------------------------------------
    uint32_t frame_draw_calls;                                // Counter for draw commands issued this frame
    uint32_t frame_triangle_count;                            // Estimate of triangles drawn this frame
    double last_vd_composite_time_ms;                         // Profiling timer for the composition pass

    // -------------------------------------------------------------------------
    // Internal Resource Tracking (Linked Lists)
    // -------------------------------------------------------------------------
    // Legacy linked lists replaced by registries                          // Head of the model tracking list

    // -------------------------------------------------------------------------
    // Feature Capabilities
    // -------------------------------------------------------------------------
    uint64_t enabled_features_mask;                           // Bitmask of SituationRenderFeature flags enabled on the current backend

    // [PERF] Text Batch Scratch Buffer
    float* text_batch_scratch;
    size_t text_batch_capacity;

#if !defined(__STDC_NO_THREADS__)
    // Threading
    thrd_t render_thread;
    mtx_t render_queue_mutex;
    cnd_t render_queue_cv;
    cnd_t main_wait_cv; // For backpressure

    // Internal Frame Structure (Definition local to _SituationRenderThreadEntry usually,
    // but queue stores indices or pointers)
    // We store indices into the per-frame buffers (soft_buffers or command_buffers)
    int render_queue[SITUATION_MAX_FRAMES_IN_FLIGHT]; // Ring buffer of frame indices to render
    int render_queue_head;
    int render_queue_tail;
    int frames_pending;

    atomic_bool thread_active;
    atomic_bool thread_shutdown_req;
    atomic_bool gl_context_released;  // Signal from Main Thread to Render Thread (OpenGL context handoff)

    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    bool enabled;
    atomic_size_t render_queue_depth;  // [NEW] Tracks head-tail diff
    #endif
#endif

    // Tracks current frame index for the MAIN thread (producing)
    // [PLATINUM] Moved outside #ifdef to ensure consistent frame tracking in both threaded and non-threaded modes.
    int current_frame_index;

    // [v2.3.42] Frame Recording State Flag
    // Tracks whether we're currently between AcquireFrame and EndFrame (command buffer is recording)
    bool in_frame;

    // [v2.3.23] Async Compute Flag (for current recording frame)
    bool frame_has_async_compute;

    // [v2.3.25] Metrics & Tracking (Moved from globals)
    atomic_uint_least64_t submit_timestamps[SITUATION_MAX_FRAMES_IN_FLIGHT];
    atomic_uint_least64_t metric_latency_sum_ns;
    atomic_uint_least64_t metric_latency_count;
    atomic_uint_least64_t metric_max_latency_ns; // For histogram/max tracking
    atomic_int frame_refcounts[SITUATION_MAX_FRAMES_IN_FLIGHT];
    atomic_bool drift_warned;

    // --- Texture Registry ---
    _SituationTextureSlot texture_registry[SITUATION_MAX_TEXTURES];
    _SituationShaderSlot shader_registry[SITUATION_MAX_SHADERS];
    _SituationMeshSlot mesh_registry[SITUATION_MAX_MESHES];
    _SituationBufferSlot buffer_registry[SITUATION_MAX_BUFFERS];
    _SituationComputePipelineSlot compute_registry[SITUATION_MAX_COMPUTE_PIPELINES];
    _SituationModelSlot model_registry[SITUATION_MAX_MODELS];

    // [v2.3.23] Default Debug Font
    SituationTexture default_font_atlas;
    SituationFont    default_font;

    // [v2.3.27] Momentum Deferred Queue
    SituationRenderList momentum_queue[256]; // Max 256 lists per frame
    atomic_int momentum_head;
    atomic_int momentum_tail;
    mtx_t momentum_mutex; // Protects the queue

    // [v2.3.34] Thread Safety for Resource Lists (Hot-Reload offload)
    mtx_t resource_registry_mutex;

    // [v2.3.40] Initialization state for safe multi-threaded resource creation
    atomic_int init_state;  // SituationInitState

} _SituationRenderState;

/**
 * @brief [INTERNAL] The central monolithic state container for the entire library.
 *
 * @details This structure holds all global state variables required by the engine, organized by subsystem.
 *          It is instantiated as a single static variable `sit_gs`.
 *
 *          The container is designed to be zero-initialized at startup (`memset` to 0), ensuring a
 *          safe default state for all pointers and flags. Backend-specific state is segregated
 *          into `vk` (Vulkan) and `gl` (OpenGL) substructures to keep the namespace clean.
 */

typedef struct {
    // -------------------------------------------------------------------------
    // Core Lifecycle & Error Handling
    // -------------------------------------------------------------------------
    char last_error_msg[SITUATION_MAX_ERROR_MSG_LEN];         // Buffer for the last reported error message
    ma_mutex error_mutex;                                     // Mutex protecting concurrent access to the error buffer
    atomic_bool is_initialized;                               // Flag indicating if SituationInit() has completed successfully
    bool is_com_initialized;                                  // Flag indicating if Windows COM was initialized by this library
    bool input_mutexes_initialized;                           // Flag indicating if input subsystem mutexes were initialized

    // -------------------------------------------------------------------------
    // Window State Management
    // -------------------------------------------------------------------------
    GLFWwindow* sit_glfw_window;                              // The primary GLFW window handle
    int main_window_width;                                    // Current width of the window's client area
    int main_window_height;                                   // Current height of the window's client area
    int windowed_x;                                           // Saved X position before entering fullscreen/borderless
    int windowed_y;                                           // Saved Y position before entering fullscreen/borderless
    int windowed_w;                                           // Saved width before entering fullscreen/borderless
    int windowed_h;                                           // Saved height before entering fullscreen/borderless

    bool current_window_focus_state;                          // True if the window currently has input focus
    bool was_minimized_last_frame;                            // State tracker for detecting minimize/restore transitions
    bool is_app_internally_paused;                            // Master pause flag for suspending audio/updates
    bool was_window_resized_last_frame;                       // Event flag indicating a resize occurred this frame
    bool is_borderless_active;                                // True if "fake fullscreen" borderless mode is active

    uint32_t active_profile_window_flags;                     // Target window flags to apply when focused
    uint32_t inactive_profile_window_flags;                   // Target window flags to apply when unfocused

    // -------------------------------------------------------------------------
    // Display & Virtual Display Subsystems
    // -------------------------------------------------------------------------
    SituationDisplayInfo* cached_physical_displays_array;     // Array of detected physical monitors
    int cached_physical_display_count;                        // Number of valid entries in the display array

    // -------------------------------------------------------------------------
    // Timing & Profiling
    // -------------------------------------------------------------------------
    SituationTimerSystem timer_system_instance;               // The Temporal Oscillator system state
    double current_time;                                      // Timestamp at the start of the current frame
    double previous_time;                                     // Timestamp at the start of the previous frame
    double frame_time;                                        // Calculated delta time (dt) for the last frame
    double target_frame_time;                                 // Target duration per frame (1.0 / target_fps)
    int    fps_frame_counter;                                 // Accumulator for frames rendered this second
    double fps_last_update_time;                              // Timestamp of the last FPS calculation
    int    current_fps;                                       // The most recently calculated FPS value

    // -------------------------------------------------------------------------
    // Application Callbacks
    // -------------------------------------------------------------------------
    void (*exit_callback)(void*);                             // Callback invoked just before shutdown
    void* exit_callback_user_data;                            // User context for exit callback
    void (*resize_callback)(int, int, void*);                 // Callback invoked on window resize
    void* resize_callback_user_data;                          // User context for resize callback
    SituationFocusCallback focus_callback_fn;                 // Callback invoked on window focus change
    void* focus_callback_user_ptr;                            // User context for focus callback
    SituationFileDropCallback file_drop_callback;             // Callback invoked on file drop
    void* file_drop_user_data;                                // User context for file drop callback

    // -------------------------------------------------------------------------
    // Environment & Filesystem
    // -------------------------------------------------------------------------
    int    argc;                                              // Command-line argument count
    char** argv;                                              // Command-line argument vector
    char** dropped_file_paths;                                // Array of paths dropped this frame (polling API)
    int    dropped_file_count;                                // Number of paths dropped this frame
    bool   file_was_dropped_this_frame;                       // Flag indicating if a drop event occurred

#if defined(SITUATION_ENABLE_THREADING)
    SituationThreadPool thread_pool;
#endif

} _SituationGlobalStateContainer;

#ifdef SITUATION_ENABLE_THREADING
// --- Forward Declarations for Threading Internal Helpers ---
static int _SituationWorkerEntry(void* arg);                                                    // [THREAD] Internal worker thread loop
static void _SitParallelWorker(void* data, void* ctx);                                          // [THREAD] Internal helper for parallel dispatch
#endif

#if !defined(__STDC_NO_THREADS__)
static int _SituationRenderThreadEntry(void* arg);                                              // [THREAD] Render thread loop

// [v2.3.21] Render Thread Lifecycle Helpers
static bool _SituationInitRenderThread(const SituationInitInfo* info);
static void _SituationDestroyRenderThread(void);
#endif

// --- Context Architecture (v2.3.7+) ---
// Moving away from static globals to a heap-allocated context allows for better control over
// initialization order, memory lifetime, and future multi-context support.
typedef struct SituationContext {
    _SituationGlobalStateContainer gs;
    _SituationRenderState render; // [NEW]
    _SituationAudioState audio;
    _SituationInputState input; // [NEW]

    // [v2.3.40] Initialization state for safe multi-threaded resource creation
    atomic_int init_state;  // SituationInitState

} SituationContext;

static SituationContext* _sit_current_context = NULL;

// Macros to maintain backward compatibility with existing internal code that uses sit_gs/sit_audio.
// These resolve to the current active context.
#define sit_gs (_sit_current_context->gs)
#define sit_render (_sit_current_context->render)
#define sit_audio (_sit_current_context->audio)
#define sit_input (_sit_current_context->input) // [NEW]


// =================================================================================
// Shader Contract & Embedded Shader Sources
// =================================================================================
// Shader Contract
//==================================================================================
#define SIT_STRINGIFY_HELPER(x) #x
#define SIT_STRINGIFY(x) SIT_STRINGIFY_HELPER(x)
/**
 * @section Shader Contract
 * @brief Defines the standard resource binding locations for all shaders used with this library.
 * @details To ensure that the engine can correctly and consistently bind resources (vertex data, uniforms, textures) to any shader, all GLSL shaders must adhere to this predefined contract.
 *          The contract uses a unified set of constants that map to the appropriate backend-specific syntax (`layout(location=...)` for OpenGL attributes, `layout(set=..., binding=...)` for Vulkan resources, etc.).
 *
 * @par Vulkan Descriptor Set Layout
 *   In Vulkan, resources are organized into Descriptor Sets. This contract defines a standard layout:
 *   - **Set 0: Per-View/Frame Resources:** Contains UBOs with data that is updated once per frame or per camera (e.g., projection matrix, time).
 *   - **Set 1: Per-Material Resources:** Contains samplers for material-specific textures (e.g., albedo, normal maps).
 *   - **Set 2+: Per-Object/Dynamic Resources:** Reserved for more dynamic, per-draw-call resources if needed.
 *   - **Push Constants:** Used for small, extremely high-frequency data (e.g., model matrix, object color), passed without a descriptor set.
 */
// --- Category 1: Vertex Attributes ---
// Mapped to `layout(location = N) in ...;` in both OpenGL and Vulkan.
#define SIT_ATTR_POSITION                   0 // vec3: Vertex position (xyz)
#define SIT_ATTR_NORMAL                     1 // vec3: Vertex normal (xyz)
#define SIT_ATTR_TEXCOORD_0                 2 // Vector2: Primary texture coordinates (uv)
#define SIT_ATTR_COLOR                      3 // vec4: Vertex color (rgba)
#define SIT_ATTR_TANGENT                    4 // vec4: Vertex tangent for normal mapping (xyz + handedness w)
#define SIT_ATTR_TEXCOORD_1                 5 // Vector2: Secondary texture coordinates (e.g., for lightmaps)

// --- Category 2: Per-View/Frame Resources (Vulkan Descriptor Set 0) ---
// Mapped to `layout(std140, set = 0, binding = N) uniform ...` in Vulkan.
// Mapped to `layout(std140, binding = N) uniform ...` in OpenGL.
#define SIT_UBO_BINDING_FRAME_DATA          0 // Contains per-frame data (Time, Screen Resolution, etc.)
#define SIT_UBO_BINDING_VIEW_DATA           1 // Contains per-camera data (View/Projection matrices)
#define SIT_UBO_BINDING_LIGHTING            2 // Contains scene lighting information
#define SIT_UBO_BINDING_SKINNING            3 // Contains bone matrices for skeletal animation

// --- Category 3: Per-Material Resources (Vulkan Descriptor Set 1) ---
// Mapped to `layout(set = 1, binding = N) uniform sampler...` in Vulkan.
// Mapped to `layout(binding = N) uniform sampler...` in OpenGL.
#define SIT_SAMPLER_BINDING_ALBEDO          0 // sampler2D: Base ColorRGBA / Diffuse Map (sRGB)
#define SIT_SAMPLER_BINDING_NORMAL          1 // sampler2D: Normal Map (Linear)
#define SIT_SAMPLER_BINDING_PBR_MAP         2 // sampler2D: Packed PBR Map (e.g., R=Metallic, G=Roughness, B=AO) (Linear)
#define SIT_SAMPLER_BINDING_EMISSIVE        3 // sampler2D: Emissive/Glow Map (sRGB)
#define SIT_SAMPLER_BINDING_SOURCE_0        4 // sampler2D: Primary generic source texture (e.g., a Virtual Display)
#define SIT_SAMPLER_BINDING_SOURCE_1        5 // sampler2D: Secondary generic source texture (e.g., framebuffer copy)
#define SIT_SAMPLER_BINDING_DEPTH           6 // sampler2D: Depth buffer texture for post-processing effects.
#define SIT_SAMPLER_BINDING_SHADOWMAP       7 // sampler2DShadow: Shadow map from a light source.

// Aliases for Virtual Display compositing shaders for clarity
#define SIT_SAMPLER_BINDING_VD_SOURCE       SIT_SAMPLER_BINDING_SOURCE_0
#define SIT_SAMPLER_BINDING_VD_DEST         SIT_SAMPLER_BINDING_SOURCE_1

// --- Category 4: OpenGL-Specific Standalone Uniforms ---
// These are primarily for the OpenGL backend, where they are more common and efficient
// than creating single-purpose UBOs. In Vulkan, this data is passed via Push Constants.
// Mapped to `layout(location = N) uniform ...;` in OpenGL.
#define SIT_UNIFORM_LOC_MODEL_MATRIX        0 // mat4: The object-to-world transformation matrix.
#define SIT_UNIFORM_LOC_OBJECT_COLOR        1 // vec4: A general-purpose color tint for the object.
#define SIT_UNIFORM_LOC_OPACITY             2 // float: A general-purpose opacity/alpha multiplier.
#define SIT_UNIFORM_LOC_BLEND_MODE          3 // int: An integer representing a blend mode for shader-based blending.
#define SIT_UNIFORM_LOC_PROJECTION_MATRIX   4 // mat4: A projection matrix, for simple shaders not using the View UBO.

// =================================================================================
// --- Agnostic Internal Shader Sources ---
// =================================================================================
// The following GLSL shaders are embedded directly into the library. They use C preprocessor directives to compile into different, highly optimized versions for the OpenGL and Vulkan
// backends, allowing the core logic to be shared while respecting the unique data-passing conventions of each API. All resource locations are defined by the library's Shader Contract.

/**
 * @internal
 * @shader SIT_VD_SHADER
 * @brief The standard shader for compositing Virtual Displays.
 * @details This is the primary, high-performance shader used for drawing virtual displays with simple blend modes (Alpha, Additive, Multiply, etc.). It draws a simple textured quad.
 *
 * @var aPos (in) The 2D vertex position of the quad's corners.
 * @var aTexCoords (in) The UV coordinates for sampling the virtual display's texture.
 * @var v_texCoord (out) The interpolated texture coordinates passed to the fragment shader.
 *
 * @var ubo (uniform, Vulkan) The per-frame Uniform Buffer Object containing projection and view matrices.
 * @var pc (uniform, Vulkan) The Push Constant block containing the per-draw model matrix and opacity.
 * @var u_projection (uniform, OpenGL) The orthographic projection matrix.
 * @var u_model (uniform, OpenGL) The per-draw model matrix.
 * @var u_screenTexture (uniform) The sampler for the virtual display's texture content.
 * @var u_opacity (uniform, OpenGL) The per-draw opacity value.
 */
// --- Shader for simple Virtual Display compositing (Alpha, Add, Multiply, etc.) ---
static const char* SIT_VD_VERTEX_SHADER_SRC =
    "#version 450 core\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_POSITION) ") in vec2 aPos;\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_TEXCOORD_0) ") in vec2 aTexCoords;\n"
    "layout(location = 0) out vec2 v_texCoord;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 0, binding = " SIT_STRINGIFY(SIT_UBO_BINDING_VIEW_DATA) ") uniform UboView { mat4 view; mat4 projection; } ubo;\n"
    "layout(push_constant) uniform VDPushConstants { mat4 model; float opacity; } pc;\n"
    "void main() {\n"
    "    gl_Position = ubo.projection * pc.model * vec4(aPos, 0.0, 1.0);\n"
    "    v_texCoord = aTexCoords;\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_PROJECTION_MATRIX) ") uniform mat4 u_projection;\n"
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_MODEL_MATRIX) ") uniform mat4 u_model;\n"
    "void main() {\n"
    "    gl_Position = u_projection * u_model * vec4(aPos, 0.0, 1.0);\n"
    "    v_texCoord = aTexCoords;\n"
    "}\n"
#endif
;

static const char* SIT_VD_FRAGMENT_SHADER_SRC =
    "#version 450 core\n"
    "layout(location = 0) in vec2 v_texCoord;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 1, binding = " SIT_STRINGIFY(SIT_SAMPLER_BINDING_VD_SOURCE) ") uniform sampler2D u_screenTexture;\n"
#elif defined(SITUATION_USE_OPENGL)
    "uniform sampler2D u_screenTexture;\n"
#endif
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(push_constant) uniform VDPushConstants { mat4 model; float opacity; } pc;\n"
    "void main() {\n"
    "    vec4 texColor = texture(u_screenTexture, v_texCoord);\n"
    "    outColor = vec4(texColor.rgb, texColor.a * pc.opacity);\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_OPACITY) ") uniform float u_opacity;\n"
    "void main() {\n"
    "    vec4 texColor = texture(u_screenTexture, v_texCoord);\n"
    "    outColor = vec4(texColor.rgb, texColor.a * u_opacity);\n"
    "}\n"
#endif
;

/**
 * @internal
 * @shader SIT_COMPOSITE_SHADER
 * @brief An advanced shader for compositing with Photoshop-style blend modes.
 * @details This shader is used when a Virtual Display is configured with a blend mode that requires knowledge of the destination color (e.g., Overlay, Soft Light).
 *          It reads from both the source texture (the VD) and the destination framebuffer (copied to a texture) to calculate the final blended color.
 *
 * @var u_sourceTexture (uniform) The sampler for the source Virtual Display's texture.
 * @var u_destinationTexture (uniform) A sampler containing a copy of the framebuffer content that is *behind* the current virtual display.
 * @var pc.blendMode (uniform, Vulkan) An integer representing the blend mode to apply.
 * @var u_blendMode (uniform, OpenGL) An integer representing the blend mode to apply.
 */
// --- Shader for Advanced Photoshop-style compositing ---
static const char* SIT_COMPOSITE_VERTEX_SHADER_SRC =
    "#version 450 core\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_POSITION) ") in vec2 aPos;\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_TEXCOORD_0) ") in vec2 aTexCoords;\n"
    "layout(location = 0) out vec2 v_texCoord;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 0, binding = " SIT_STRINGIFY(SIT_UBO_BINDING_VIEW_DATA) ") uniform UboView { mat4 view; mat4 projection; } ubo;\n"
    "layout(push_constant) uniform CompositePushConstants { mat4 model; int blendMode; float opacity; } pc;\n"
    "void main() {\n"
    "    gl_Position = ubo.projection * pc.model * vec4(aPos, 0.0, 1.0);\n"
    "    v_texCoord = aTexCoords;\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_PROJECTION_MATRIX) ") uniform mat4 u_projection;\n"
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_MODEL_MATRIX) ") uniform mat4 u_model;\n"
    "void main() {\n"
    "    gl_Position = u_projection * u_model * vec4(aPos, 0.0, 1.0);\n"
    "    v_texCoord = aTexCoords;\n"
    "}\n"
#endif
;

static const char* SIT_COMPOSITE_FRAGMENT_SHADER_SRC =
    "#version 450 core\n"
    "layout(location = 0) in vec2 v_texCoord;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    // Sampler bindings: Set 1 = VD source (binding 4), Set 2 = screen copy (binding 5)\n"
    "layout(set = 1, binding = " SIT_STRINGIFY(SIT_SAMPLER_BINDING_VD_SOURCE) ") uniform sampler2D u_sourceTexture;\n"
    "layout(set = 2, binding = " SIT_STRINGIFY(SIT_SAMPLER_BINDING_VD_DEST) ") uniform sampler2D u_destinationTexture;\n"
#elif defined(SITUATION_USE_OPENGL)
    "uniform sampler2D u_sourceTexture;\n"
    "uniform sampler2D u_destinationTexture;\n"
#endif
    "\n"
    // --- Blend Mode Helpers ---\n"
    "float overlay(float b, float l) { return (b < 0.5) ? (2.0*b*l) : (1.0 - 2.0*(1.0-b)*(1.0-l)); }\n"
    "float softlight(float b, float l) { return (l < 0.5) ? (b - (1.0 - 2.0 * l) * b * (1.0 - b)) : (b + (2.0 * l - 1.0) * (((b <= 0.25) ? (((16.0 * b - 12.0) * b + 4.0) * b) : sqrt(b)) - b)); }\n"
    "\n"
    // --- Backend-Specific Uniform/Push Constant Declarations ---\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(push_constant) uniform CompositePushConstants { mat4 model; int blendMode; float opacity; } pc;\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_BLEND_MODE) ") uniform int u_blendMode;\n"
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_OPACITY) ") uniform float u_opacity;\n"
#endif
    "\n"
    "void main() {\n"
    "    // --- Select correct uniform source based on backend ---\n"
    "    int blendMode;\n"
    "    float opacity;\n"
#if defined(SITUATION_USE_VULKAN)
    "    blendMode = pc.blendMode;\n"
    "    opacity = pc.opacity;\n"
#else // OpenGL
    "    blendMode = u_blendMode;\n"
    "    opacity = u_opacity;\n"
#endif
    "\n"
    "    // Fetch source (VD) and destination (framebuffer) colors\n"
    "    vec4 src = texture(u_sourceTexture, v_texCoord);\n"
    "    vec3 dst = texture(u_destinationTexture, gl_FragCoord.xy / textureSize(u_destinationTexture, 0)).rgb;\n"
    "    vec3 res;\n"
    "\n"
    "    // --- FULL BLEND MODE IMPLEMENTATION ---\n"
    "    switch (blendMode) {\n"
    "        // These modes are handled by the simple shader, but included as fallbacks.\n"
    "        case 0:  /* ALPHA */ res = src.rgb; break;\n"
    "        case 1:  /* ADDITIVE */ res = src.rgb + dst; break;\n"
    "        case 2:  /* MULTIPLY */ res = src.rgb * dst; break;\n"
    "        // --- Advanced Photoshop-style modes ---\n"
    "        case 3:  /* SCREEN */ res = 1.0 - (1.0 - src.rgb) * (1.0 - dst); break;\n"
    "        case 5:  /* OVERLAY */ res = vec3(overlay(dst.r, src.r), overlay(dst.g, src.g), overlay(dst.b, src.b)); break;\n"
    "        case 6:  /* SOFT_LIGHT */ res = vec3(softlight(dst.r, src.r), softlight(dst.g, src.g), softlight(dst.b, src.b)); break;\n"
    "        case 7:  /* HARD_LIGHT */ res = vec3(overlay(src.r, dst.r), overlay(src.g, dst.g), overlay(src.b, dst.b)); break;\n"
    "        case 8:  /* COLOR_DODGE */ res = dst / (1.0 - min(vec3(0.9999), src.rgb)); break;\n"
    "        case 9:  /* COLOR_BURN */ res = 1.0 - (1.0 - dst) / max(vec3(0.0001), src.rgb); break;\n"
    "        case 10: /* DARKEN */ res = min(dst, src.rgb); break;\n"
    "        case 11: /* LIGHTEN */ res = max(dst, src.rgb); break;\n"
    "        case 12: /* DIFFERENCE */ res = abs(dst - src.rgb); break;\n"
    "        case 13: /* EXCLUSION */ res = dst + src.rgb - 2.0 * dst * src.rgb; break;\n"
    "        default: res = src.rgb; break;\n"
    "    }\n"
    "\n"
    "    // --- Final Composition ---\n"
    "    // Linearly interpolate (mix) between the original destination color (dst)\n"
    "    // and the blended result (res) based on the source's alpha and overall opacity.\n"
    "    float finalAlpha = src.a * opacity;\n"
    "    outColor = vec4(mix(dst.rgb, res, finalAlpha), 1.0);\n"
    "}\n";

// Draws a simple, colored, transformed quad with dynamic UV support.
static const char* SIT_QUAD_VERTEX_SHADER =
    "#version 450 core\n"
    // Shader Contract: Vertex Position Attribute (Standard Quad is 0..1)
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_POSITION) ") in vec2 aPos;\n"
    // Output UVs to fragment shader
    "layout(location = 0) out vec2 v_TexCoord;\n"
    "\n"
    // --- Backend-Agnostic Uniform Block --- \n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 0, binding = " SIT_STRINGIFY(SIT_UBO_BINDING_VIEW_DATA) ") uniform UboView { mat4 view; mat4 projection; } ubo;\n"
    // Added uv_rect to push constants
    "layout(push_constant) uniform QuadPushConstants { mat4 model; vec4 color; vec4 uv_rect; uint texture_id; int use_texture; } pc;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = ubo.projection * pc.model * vec4(aPos, 0.0, 1.0);\n"
    "    // Calculate UV: aPos is 0..1. uv_rect is (u_off, v_off, u_scale, v_scale)\n"
    "    v_TexCoord = pc.uv_rect.xy + (aPos * pc.uv_rect.zw);\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_PROJECTION_MATRIX) ") uniform mat4 u_projection;\n"
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_MODEL_MATRIX) ") uniform mat4 u_model;\n"
    // Add a standalone uniform location for UV rect (Location 5)
    "layout(location = 5) uniform vec4 u_uv_rect;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = u_projection * u_model * vec4(aPos, 0.0, 1.0);\n"
    "    v_TexCoord = u_uv_rect.xy + (aPos * u_uv_rect.zw);\n"
    "}\n"
#endif
;

static const char* SIT_QUAD_FRAGMENT_SHADER =
    "#version 450 core\n"
#if defined(SITUATION_USE_OPENGL)
    "#extension GL_ARB_bindless_texture : enable\n"
    "#extension GL_ARB_gpu_shader_int64 : enable\n"
#endif
    "layout(location = 0) in vec2 v_TexCoord;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "#extension GL_EXT_nonuniform_qualifier : require\n"
    "layout(set = 1, binding = 0) uniform sampler2D global_textures[];\n"
    "layout(push_constant) uniform QuadPushConstants { mat4 model; vec4 color; vec4 uv_rect; uint texture_id; int use_texture; } pc;\n"
    "void main() {\n"
    "    vec4 texColor = vec4(1.0);\n"
    "    if (pc.use_texture == 1) {\n"
    "        texColor = texture(global_textures[nonuniformEXT(pc.texture_id)], v_TexCoord);\n"
    "    }\n"
    "    // For SDF fonts, we might need special handling, but for baked bitmap fonts, simple sampling works.\n"
    "    // If it's a 1-channel bitmap font, it comes as alpha (0,0,0,A) or (1,1,1,A). \n"
    "    // Our baker creates RGBA white with alpha.\n"
    "    outColor = texColor * pc.color;\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    // OpenGL uniforms
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_OBJECT_COLOR) ") uniform vec4 u_objectColor;\n"
    "layout(location = 6) uniform int u_use_texture;\n"
    // Standard texture sampler (binding 0 by default)
    "uniform sampler2D u_Texture;\n"
    // [v2.3.30] Bindless Handle Uniform (Location 7)
    // Using uvec2 to pass 64-bit handle safely as 2x32-bit ints if int64 support is flaky,
    // but here we use GL_ARB_gpu_shader_int64 for simplicity with extension check.
    "#if defined(GL_ARB_bindless_texture)\n"
    "layout(bindless_sampler, location = 7) uniform sampler2D u_TextureHandle;\n"
    "#endif\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = vec4(1.0);\n"
    "    if (u_use_texture == 1) {\n"
    "#if defined(GL_ARB_bindless_texture)\n"
    // If handle is valid (non-zero), use it. We assume init sets it to 0 if unused.
    // However, checking sampler handle validity in shader is tricky.
    // We rely on the CPU side setting u_use_texture = 2 for bindless.
    "        if (u_use_texture == 2) texColor = texture(u_TextureHandle, v_TexCoord);\n"
    "        else texColor = texture(u_Texture, v_TexCoord);\n"
    "#else\n"
    "        texColor = texture(u_Texture, v_TexCoord);\n"
    "#endif\n"
    "    }\n"
    "    outColor = texColor * u_objectColor;\n"
    "}\n"
#endif
;

// Draws batched text quads.
static const char* SIT_TEXT_VERTEX_SHADER =
    "#version 450 core\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_POSITION) ") in vec2 aPos;\n"
    "layout(location = " SIT_STRINGIFY(SIT_ATTR_TEXCOORD_0) ") in vec2 aTexCoord;\n"
    "layout(location = 0) out vec2 v_TexCoord;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 0, binding = " SIT_STRINGIFY(SIT_UBO_BINDING_VIEW_DATA) ") uniform UboView { mat4 view; mat4 projection; } ubo;\n"
    "layout(push_constant) uniform TextPushConstants { vec4 color; uint texture_id; } pc;\n"
    "void main() {\n"
    "    gl_Position = ubo.projection * vec4(aPos, 0.0, 1.0);\n"
    "    v_TexCoord = aTexCoord;\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_PROJECTION_MATRIX) ") uniform mat4 u_projection;\n"
    "void main() {\n"
    "    gl_Position = u_projection * vec4(aPos, 0.0, 1.0);\n"
    "    v_TexCoord = aTexCoord;\n"
    "}\n"
#endif
;

static const char* SIT_TEXT_FRAGMENT_SHADER =
    "#version 450 core\n"
#if defined(SITUATION_USE_OPENGL)
    "#extension GL_ARB_bindless_texture : enable\n"
    "#extension GL_ARB_gpu_shader_int64 : enable\n"
#elif defined(SITUATION_USE_VULKAN)
    "#extension GL_EXT_nonuniform_qualifier : require\n"
#endif
    "layout(location = 0) in vec2 v_TexCoord;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
#if defined(SITUATION_USE_VULKAN)
    "layout(set = 1, binding = 0) uniform sampler2D global_textures[];\n"
    // Note: Added texture_id to Push Constants
    "layout(push_constant) uniform TextPushConstants { vec4 color; uint texture_id; } pc;\n"
    "void main() {\n"
    "    // Sample from global array using texture_id\n"
    "    vec4 texColor = texture(global_textures[nonuniformEXT(pc.texture_id)], v_TexCoord);\n"
    "    outColor = vec4(pc.color.rgb, pc.color.a * texColor.a);\n"
    "}\n"
#elif defined(SITUATION_USE_OPENGL)
    "layout(binding = " SIT_STRINGIFY(SIT_SAMPLER_BINDING_ALBEDO) ") uniform sampler2D u_Texture;\n"
    "\n"
    "layout(location = " SIT_STRINGIFY(SIT_UNIFORM_LOC_OBJECT_COLOR) ") uniform vec4 u_color;\n"
    // Bindless Handle support
    "layout(location = 6) uniform int u_use_bindless;\n"
    "#if defined(GL_ARB_bindless_texture)\n"
    "layout(bindless_sampler, location = 7) uniform sampler2D u_TextureHandle;\n"
    "#endif\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor;\n"
    "#if defined(GL_ARB_bindless_texture)\n"
    "    if (u_use_bindless == 1) texColor = texture(u_TextureHandle, v_TexCoord);\n"
    "    else texColor = texture(u_Texture, v_TexCoord);\n"
    "#else\n"
    "    texColor = texture(u_Texture, v_TexCoord);\n"
    "#endif\n"
    "    outColor = texColor * u_color;\n"
    "}\n"
#endif
;


#endif // SITUATION_IMPL_DECL_H