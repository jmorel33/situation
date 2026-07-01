/***************************************************************************************************
*
*   situation_impl_decl.h - Internal Declarations, Types, Globals & Data
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file contains all internal type definitions, struct declarations, static globals,
*   macros, forward declarations, and embedded data for the Situation library.
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

// Propagate SituationError from internal helpers (internal hardening plan Phase 0).
#ifndef SIT_RETURN_IF_ERR
#define SIT_RETURN_IF_ERR(expr) do { \
    SituationError _sit_err = (expr); \
    if (_sit_err != SITUATION_SUCCESS) return _sit_err; \
} while(0)
#endif

// =================================================================================
// Internal Globals & Utility Helpers
// =================================================================================

// Forward declaration (implementation in error handling section) — needed here before situation_impl_forward.h
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

/* HARDENING: void by design — debug assert only; sets THREAD_VIOLATION on mismatch. */
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

#if defined(SITUATION_USE_VULKAN)
#define SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS      16
#define SITUATION_VULKAN_UNIFORM_BUFFER_SIZE          256
#define SITUATION_VULKAN_STORAGE_BUFFER_SIZE          256
#define SITUATION_VULKAN_COMBINED_IMAGE_SAMPLER_SIZE  512
#define SITUATION_VULKAN_DEFAULT_USER_STORAGE_IMAGES  128
#endif // SITUATION_USE_VULKAN

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

/** Raw SPIR-V bytes (file or memory). Used for precompiled shader load without runtime GLSL compile. */
typedef struct SituationSpirvBinary {
    const void* data;
    size_t size;
} SituationSpirvBinary;

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SituationShadercMacro {
    const char* name;
    const char* value;
} _SituationShadercMacro;
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
#ifndef GL_STENCIL_BITS
#define GL_STENCIL_BITS 0x0D57
#endif
#ifndef GL_STENCIL
#define GL_STENCIL 0x1802
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
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
    SIT_OP_DRAW_TEXTURE_YPQ,
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
    SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING, // [Phase 2] Temporary
    SIT_OP_COPY_BUFFER, // [Phase 1] Async readback
    SIT_OP_SET_CULL_MODE, // [Phase 4]
    SIT_OP_SET_FRONT_FACE, // [Phase 6]
    SIT_OP_SET_PRIMITIVE_TOPOLOGY, // [Phase 6]
    SIT_OP_SET_POLYGON_MODE, // [Phase 6B]
    SIT_OP_SET_DEPTH_BIAS, // [Phase 6B]
    SIT_OP_SET_LINE_WIDTH, // [Phase 6B]
    SIT_OP_SET_COLOR_WRITE_MASK, // [Phase 6B]
    SIT_OP_SET_STENCIL_TEST, // [Phase 6B]
    SIT_OP_SET_MULTISAMPLE_STATE, // [Phase 4 / v2.5] GL: sample shading, mask, alpha-to-coverage. VK: pipeline-bake only (see notes).
    SIT_OP_SET_DEPTH_TEST,
    SIT_OP_SET_DEPTH_WRITE,
    SIT_OP_SET_BLEND_ENABLE,
    SIT_OP_SET_BLEND_FUNC_SEPARATE,
    SIT_OP_PUSH_RASTER_STATE,
    SIT_OP_POP_RASTER_STATE,
    SIT_OP_SET_PUSH_CONSTANT_DATA,
    SIT_OP_BEGIN_DEBUG_GROUP,
    SIT_OP_END_DEBUG_GROUP,
    SIT_OP_CLEAR,
    SIT_OP_DISPATCH_INDIRECT,
    SIT_OP_DRAW_INDIRECT,
    SIT_OP_DRAW_INDEXED_INDIRECT,
    SIT_OP_BLIT_TEXTURE,
    SIT_OP_COPY_TEXTURE,
    SIT_OP_COPY_BUFFER_TO_TEXTURE,
    SIT_OP_COPY_TEXTURE_TO_BUFFER,
    SIT_OP_SET_RENDERER_BEHAVIOR,
    SIT_OP_PUSH_RENDERER_BEHAVIOR,
    SIT_OP_POP_RENDERER_BEHAVIOR,
    SIT_OP_GPU_ZONE_BEGIN,
    SIT_OP_GPU_ZONE_END,
    SIT_OP_RESET_QUERY_POOL,
    SIT_OP_WRITE_TIMESTAMP,
    SIT_OP_BEGIN_OCCLUSION_QUERY,
    SIT_OP_END_OCCLUSION_QUERY
} SitOpCode;

typedef struct {
    SitOpCode opcode;
    union {
        struct { int display_id; int target_w; int target_h; SituationRenderPassInfo info; } begin_pass;
        struct { uint32_t index; float x, y, w, h; } viewport;
        struct { uint32_t index; int x, y, w, h; } scissor;
        struct { uint64_t shader_id; } bind_pipeline;
        struct { SituationMesh mesh; uint64_t shader_id; } draw_mesh;
        struct { mat4 model; Vector4 color; Vector4 uv_rect; int use_texture; uint64_t texture_id; int texture_slot_index; } draw_quad;
        struct { mat4 model; Vector4 uv_rect; float phase_shift_deg; float chroma_factor; float luma_factor; float mix; } draw_texture_ypq;
        struct { uint32_t offset; size_t size; size_t data_offset; } push_constant;
        struct { uint32_t set_index; uint64_t resource_id; int resource_type; size_t offset; size_t size; uint32_t usage_flags; } bind_desc; // [Phase 2] Added size and usage_flags for Ring Buffer
        struct { uint32_t binding; uint64_t buffer_id; size_t offset; size_t stride; } bind_vbo;
        struct { uint64_t buffer_id; size_t offset; SituationIndexType index_type; } bind_ibo;
        struct { uint32_t v_count, i_count, first_v, first_i; } draw;
        struct { uint32_t idx_count, inst_count, first_idx; int32_t v_offset; uint32_t first_inst; } draw_indexed;
        struct { uint32_t src, dst; } barrier;
        struct { uint32_t x, y, z; } dispatch;
        struct { uint64_t buffer_id; size_t offset; } dispatch_indirect;
        struct { uint64_t buffer_id; size_t offset; } draw_indirect;
        struct { uint64_t buffer_id; size_t offset; } draw_indexed_indirect;
        struct { SituationTexture texture; int target_w; int target_h; } present;
        struct { SituationFont font; Vector2 pos; ColorRGBA color; size_t text_offset; } draw_text; // Store text in data_buffer
        struct { SituationFont font; Vector2 pos; float fontSize; float spacing; ColorRGBA color; size_t text_offset; } draw_text_ex; // [v2.3.23]
        struct { uint64_t buffer_id; size_t offset; size_t size; size_t data_offset; } update_buffer;
        struct { uint32_t location; uint32_t binding; int size; int type; int normalized; size_t offset; } set_vertex_attr;
        struct { uint64_t shader_id; GLint location; int type; int elem_count; size_t data_offset; } set_uniform;
        struct { uint64_t src_id; uint64_t dst_id; size_t src_offset; size_t dst_offset; size_t size; } copy_buffer; // [Phase 4A]
        struct { SituationTexture src; SituationTexture dst; SituationTextureBlitRegion region; } blit_texture;
        struct { SituationTexture src; SituationTexture dst; SituationTextureCopyRegion region; } copy_texture;
        struct { uint64_t buffer_id; size_t buffer_offset; SituationTexture dst; SituationTextureCopyRegion region; } copy_buffer_to_texture;
        struct { SituationTexture src; SituationTextureCopyRegion region; uint64_t buffer_id; size_t buffer_offset; size_t buffer_row_pitch; } copy_texture_to_buffer;
        struct { SituationCullMode mode; } set_cull_mode; // [Phase 4]
        struct { SituationFrontFace front_face; } set_front_face; // [Phase 6]
        struct { SituationPrimitiveTopology topology; } set_primitive_topology; // [Phase 6]
        struct { SituationPolygonMode mode; } set_polygon_mode; // [Phase 6B]
        struct { bool enable; float constant_factor; float clamp; float slope_factor; } set_depth_bias; // [Phase 6B]
        struct { float width; } set_line_width; // [Phase 6B]
        struct { bool r; bool g; bool b; bool a; } set_color_write_mask; // [Phase 6B]
        struct { bool enable; SituationStencilState front; SituationStencilState back; } set_stencil_test; // [Phase 6B]
        struct { SituationMultisampleState ms; } set_multisample_state; // [Phase 4 / v2.5]
        struct { bool enable; SituationDepthCompareOp depth_op; } set_depth_test;
        struct { bool enable; } set_depth_write;
        struct { bool enable; } set_blend_enable;
        struct { SituationBlendFactor src_rgb; SituationBlendFactor dst_rgb; SituationBlendFactor src_a; SituationBlendFactor dst_a; } set_blend_func;
        struct { uint32_t scope_id; } push_pop_raster_state;
        struct { SituationRendererBehaviorPolicy policy; } set_renderer_behavior;
        struct { uint32_t scope_id; } push_pop_renderer_behavior;
        struct { SituationShader shader; uint32_t offset; size_t size; size_t data_offset; } set_push_constant_data;
        struct { size_t name_offset; ColorRGBA color; } begin_debug_group;
        struct { uint32_t flags; SituationClearValue value; } clear;
        struct { uint32_t zone_id; } gpu_zone;
        struct { uint32_t pool_slot; uint32_t pool_generation; uint32_t first_query; uint32_t query_count; } reset_query_pool;
        struct { uint32_t pool_slot; uint32_t pool_generation; uint32_t query_index; uint32_t pipeline_stage; } write_timestamp;
        struct { uint32_t pool_slot; uint32_t pool_generation; uint32_t query_index; } occlusion_query;
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
    bool recording_render_pass_active;
    int raster_stack_depth; // Push/pop balance during soft-buffer recording
    SituationRendererBehaviorPolicy behavior;
    SituationRendererBehaviorPolicy behavior_stack[SITUATION_MAX_BEHAVIOR_STACK_DEPTH];
    int behavior_stack_depth;
    int recording_pass_display_id;   // -1 main window, >= 0 VD target while pass open
    int recording_pass_rt_slot;      // >= 0 user render target while pass open; -1 otherwise
    bool recording_pass_had_draw;    // true after a draw recorded in the current pass
    bool recording_occlusion_active; // P10.4: occlusion query open during soft-buffer recording
    int compute_bound_texture_slots[SIT_VD_MAX_COMPUTE_TEXTURE_BINDS]; // registry slot indices, -1 unset
} SituationGLSoftCommandBuffer;

typedef struct {
    GLboolean blend;
    GLboolean depth_test;
    GLboolean cull_face;
    GLboolean scissor_test;
    GLboolean stencil_test;
    GLboolean color_mask[4];
    GLboolean depth_mask;
    GLint blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
    GLint blend_equ_rgb, blend_equ_alpha;
    GLenum depth_func;
    GLenum cull_face_mode;
    GLenum front_face;
    GLenum polygon_mode;
    GLboolean polygon_offset_fill;
    GLfloat polygon_offset_factor;
    GLfloat polygon_offset_units;
    GLfloat line_width;
    GLenum primitive_mode;
    bool primitive_mode_set;
    GLint stencil_func_front, stencil_ref_front, stencil_value_mask_front, stencil_writemask_front;
    GLint stencil_fail_front, stencil_depth_fail_front, stencil_pass_front;
    GLint stencil_func_back, stencil_ref_back, stencil_value_mask_back, stencil_writemask_back;
    GLint stencil_fail_back, stencil_depth_fail_back, stencil_pass_back;
    // Multisample state (GL 3.3+ core, no-ops on single-sample FBOs)
    GLboolean multisample_sample_shading;   // GL_SAMPLE_SHADING
    GLfloat   multisample_min_shading;      // glMinSampleShading value
    GLuint    multisample_sample_mask;      // glSampleMaski(0, mask) — only index 0 tracked
    GLboolean multisample_alpha_to_coverage; // GL_SAMPLE_ALPHA_TO_COVERAGE
} _SitGLRasterStackEntry;
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

// =============================================================================
// --- OpenGL Shader Program Cache (Phase 4) ---
// See: doc/plan/VULKAN_SHADER_CACHE_PLAN.md Phase 4
// =============================================================================
#ifndef SIT_GL_SHADER_CACHE_ENABLE
#define SIT_GL_SHADER_CACHE_ENABLE 1
#endif

#define SIT_GL_SHADER_CACHE_MAX_ENTRIES          256
#define SIT_GL_SHADER_CACHE_EVICT_DELAY_FRAMES     2

typedef enum _SitGLProgramCacheState {
    SIT_GL_PROG_READY = 0,
    SIT_GL_PROG_EVICT_PENDING,
} _SitGLProgramCacheState;

typedef struct _SitGLProgramCacheEntry {
    uint64_t layer_a_key;
    GLuint program_id;
    atomic_uint ref_count;
    uint32_t generation;
    uint32_t last_used_frame;
    _SitGLProgramCacheState state;
    struct _SitGLProgramCacheEntry* next;
} _SitGLProgramCacheEntry;

typedef struct _SitGLProgramCacheRef {
    _SitGLProgramCacheEntry* entry;
    uint32_t generation;
} _SitGLProgramCacheRef;

typedef struct _SitGLProgramCache {
    _SitGLProgramCacheEntry* buckets[SIT_GL_SHADER_CACHE_MAX_ENTRIES];
    mtx_t mutex;
    bool mutex_initialized;
#if !defined(NDEBUG)
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
#endif
} _SitGLProgramCache;
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

// =============================================================================
// --- Vulkan Shader Cache (Phase 1) ---
// See: doc/plan/VULKAN_SHADER_CACHE_PLAN.md
// =============================================================================

/* Compile-time kill switch — set to 0 to fall back to uncached path for bisect. */
#ifndef SIT_VK_SHADER_CACHE_ENABLE
#define SIT_VK_SHADER_CACHE_ENABLE 1
#endif

#define SIT_VK_SHADER_CACHE_MAX_ENTRIES         256
#define SIT_VK_SHADER_CACHE_EVICT_DELAY_FRAMES    2

#ifndef SIT_VK_SHADER_CACHE_PHASE2
#define SIT_VK_SHADER_CACHE_PHASE2 1
#endif

#define SIT_VK_SHADER_CACHE_HOT_PIN_FRAMES       60
#define SIT_VK_SHADER_CACHE_HOT_PIN_MAX             8
#define SIT_VK_PIPE_VARIANT_COUNT                  11
#define SIT_VK_SHADER_BUILD_TICKET_MAX           32

/* Lazy pipeline variant indices — default_pipeline covers SIMPLE/none/fill. */
typedef enum _SitVkPipeVariantId {
    SIT_VK_VAR_PBR_NONE = 0,
    SIT_VK_VAR_PBR_BACK_CCW,
    SIT_VK_VAR_PBR_BACK_CW,
    SIT_VK_VAR_PBR_LINE,
    SIT_VK_VAR_LEGACY_NONE,
    SIT_VK_VAR_LEGACY_BACK_CCW,
    SIT_VK_VAR_LEGACY_BACK_CW,
    SIT_VK_VAR_LEGACY_LINE,
    SIT_VK_VAR_SIMPLE_BACK_CCW,
    SIT_VK_VAR_SIMPLE_BACK_CW,
    SIT_VK_VAR_SIMPLE_LINE,
} _SitVkPipeVariantId;

/* Three-field key (Phase 1). Phase 2 appends render_pass_compatibility_id,
 * subpass_index, dynamic_state_mask, caps_fingerprint at the bottom — do NOT
 * reorder Phase 1 fields. */
typedef struct _SitVkShaderCacheKey {
    uint64_t vs_spirv_hash;
    uint64_t fs_spirv_hash;
    uint8_t  layout_profile;   /* SituationSpirvLayoutProfile */
#if SIT_VK_SHADER_CACHE_PHASE2
    uint32_t render_pass_compatibility_id;
    uint8_t  subpass_index;
    uint32_t dynamic_state_mask;
    uint32_t caps_fingerprint;
#endif
} _SitVkShaderCacheKey;

typedef enum _SitVkBundleState {
    SIT_VK_BUNDLE_READY = 0,
    SIT_VK_BUNDLE_EVICT_PENDING,
    SIT_VK_BUNDLE_STALE,        /* Phase 2: compatibility key bump */
    SIT_VK_BUNDLE_DESTROYED
} _SitVkBundleState;

/* Forward-declare bundle so BundleRef can hold a pointer to it. */
typedef struct _SitVkPipelineBundle _SitVkPipelineBundle;

/* Held per shader slot — zero locking on draw path. */
typedef struct _SitVkPipelineBundleRef {
    _SitVkPipelineBundle* bundle;     /* NULL when no cached bundle is attached */
    uint32_t              generation; /* bundle->generation at acquire time */
} _SitVkPipelineBundleRef;

/* Phase 1 minimal bundle.  Phase 2 appends pin_count / variants[] at the bottom
 * — do NOT reorder Phase 1 fields. */
struct _SitVkPipelineBundle {
    _SitVkShaderCacheKey  key;
    /* vs_spirv_hash ^ fs_spirv_hash — fast hot-reload equality check (Phase 3). */
    uint64_t              content_hash;
    uint32_t              generation;   /* bumped on evict/stale; invalidates live BundleRefs */
    atomic_uint           ref_count;    /* decremented by ReleaseBundle; destroy when 0 + delay */
    uint32_t              last_used_frame;
    _SitVkBundleState     state;

    VkPipelineLayout      layout;
    bool                  owns_layout;  /* false when layout is a global shared entry */
    VkShaderModule        vs_module;
    VkShaderModule        fs_module;
    /* Phase 1: one default pipeline (simple stride, fill, no back-face cull).
     * Phase 2: lazy variants[] appended below. */
    VkPipeline            default_pipeline;
#if SIT_VK_SHADER_CACHE_PHASE2
    atomic_uint           pin_count;
    VkPipeline            variants[SIT_VK_PIPE_VARIANT_COUNT];
    atomic_uint           variant_ready_mask;
#endif
};

/* --- Layer A: SPIR-V blob cache (CPU, worker-safe) --- */
typedef struct _SitVkSpirvBlobEntry {
    uint64_t              layer_a_key;  /* hash(vs_src) ^ (hash(fs_src) << 1) ^ shaderc_fp */
    uint8_t*              vs_data;
    size_t                vs_size;
    uint8_t*              fs_data;
    size_t                fs_size;
    atomic_uint           ref_count;
    struct _SitVkSpirvBlobEntry* next;  /* collision chain */
} _SitVkSpirvBlobEntry;

/* --- Layer B: shader module pair cache (GPU, main thread) --- */
typedef struct _SitVkModulePairEntry {
    uint64_t              vs_spirv_hash;
    uint64_t              fs_spirv_hash;
    VkShaderModule        vs_module;
    VkShaderModule        fs_module;
    atomic_uint           ref_count;
    struct _SitVkModulePairEntry* next; /* collision chain */
} _SitVkModulePairEntry;

/* --- Layer C: pipeline bundle cache (GPU, main thread) --- */
typedef struct _SitVkShaderCacheEntry {
    _SitVkShaderCacheKey  key;
    _SitVkPipelineBundle* bundle;
    struct _SitVkShaderCacheEntry* next; /* collision chain */
} _SitVkShaderCacheEntry;

#if SIT_VK_SHADER_CACHE_PHASE2
typedef struct _SitVkShaderBuildTicket {
    uint64_t              layer_a_key;
    atomic_int            phase; /* 0=idle, 1=shaderc, 2=shaderc done, 3=bundle ready */
    _SitVkPipelineBundle* result_bundle;
    atomic_uint           waiter_count;
    uint64_t              leader_submit_ns; /* wall-clock time when leader was acquired (0=idle) */
} _SitVkShaderBuildTicket;
#endif

/* Three parallel bucket arrays — mirrors the GL vao_cache[256] shape.
 * Held inline on sit_render.vk via _SitVkShaderCache shader_cache. */
typedef struct _SitVkShaderCache {
    _SitVkSpirvBlobEntry*   spirv_blob_cache[SIT_VK_SHADER_CACHE_MAX_ENTRIES];
    _SitVkModulePairEntry*  module_pair_cache[SIT_VK_SHADER_CACHE_MAX_ENTRIES];
    _SitVkShaderCacheEntry* pipeline_bundle_cache[SIT_VK_SHADER_CACHE_MAX_ENTRIES];
    mtx_t                   mutex;          /* protects all three maps + ref-counts */
    bool                    mutex_initialized;
#if SIT_VK_SHADER_CACHE_PHASE2
    _SitVkShaderBuildTicket build_tickets[SIT_VK_SHADER_BUILD_TICKET_MAX];
#endif
#if !defined(NDEBUG)
    /* Debug counters — printed at shutdown in [Vulkan Debug] banner. */
    struct {
        uint64_t hits;               /* Layer C bundle acquire hit */
        uint64_t misses;             /* new bundle built (includes pipeline create) */
        uint64_t evictions;
        uint64_t stale_derefs;       /* _SitVkDerefBundle returned NULL due to stale gen */
        uint64_t total_build_time_ns;
#if SIT_VK_SHADER_CACHE_PHASE2
        uint64_t variant_lazies;
        uint64_t compile_dedup_joins;
        uint64_t legacy_slot_pipeline_builds; /* Phase 6.0: MESH loads that fell through to 12-pipeline slot path */
        uint64_t bundle_resolve_slot_fallbacks; /* Phase 6B: bundle active but resolve used slot pipelines */
#endif
    } stats;
#endif
} _SitVkShaderCache;

static inline uint32_t _SituationHashRenderPassKey(const SituationRenderPassInfo* info, bool is_main_window) {
    (void)is_main_window;
    return SituationRenderPassConfigurationKey(info);
}


/**
 * @brief [INTERNAL] Vulkan backend state container.
 * @details Holds the core Vulkan handles (Instance, Device, Queue) and the memory allocator (VMA).
 *          It also manages the swapchain, per-frame synchronization objects (Semaphores, Fences),
 *          and the dynamic descriptor pool manager.
 */
typedef struct {
    VkColorComponentFlags color_write_mask;
    VkBool32 depth_test_enable;
    VkBool32 depth_write_enable;
    VkCompareOp depth_compare_op;
    VkBool32 stencil_test_enable;
    SituationStencilState stencil_front;
    SituationStencilState stencil_back;
    float line_width;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    VkBool32 depth_bias_enable;
    float depth_bias_constant;
    float depth_bias_clamp;
    float depth_bias_slope;
    // Multisample state — stored for push/pop completeness; apply requires pipeline rebuild
    bool ms_sample_shading_enable;
    float ms_min_sample_shading;
    uint32_t ms_sample_mask;
    bool ms_alpha_to_coverage_enable;
} _SitVulkanRasterStackEntry;

 typedef struct {
    // -------------------------------------------------------------------------
    // Core API Objects
    // -------------------------------------------------------------------------
    VkInstance instance;                        // The Vulkan instance handle
    VkDebugUtilsMessengerEXT debug_messenger;   // Handle for the debug callback (validation layers)
    VkSurfaceKHR surface;                       // The window surface handle
    VkPhysicalDevice physical_device;           // Handle to the selected physical GPU
    uint32_t physical_device_api_version;       // VkPhysicalDeviceProperties::apiVersion (raw Vulkan version dword)
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
    /** Same attachments as main window FBs but color loadOp LOAD — required after VD composite
     *  so vkCmdBeginRenderPass does not CLEAR the swapchain (wiping composite output). */
    VkRenderPass main_window_render_pass_resume;
    VkFramebuffer* main_window_framebuffers_resume;

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
    bool needs_compute_wait[SITUATION_MAX_FRAMES_IN_FLIGHT]; // [v2.3.24b] Per-slot: graphics must wait for compute

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
    VkDescriptorSetLayout composite_dest_sampler_layout;   // Set 2 in advanced VD FS: binding 5 (framebuffer copy / destination)

    // Compute Pipeline Layout Cache
    VkPipelineLayout current_pipeline_layout_for_push_constants; // Last bound graphics layout
    VkPipelineLayout current_compute_pipeline_layout;            // Last bound compute layout
    VkPipelineLayout compute_layouts[8];                         // Pre-created standard layouts
    VkPipelineLayout graphics_spirv_layout_ubo_ssbo;             // Graphics SPIR-V: set 0 UBO, set 1 SSBO (harness)
    VkPipelineLayout graphics_spirv_layout_ubo_ssbo_sampler;     // Graphics SPIR-V: set 0 UBO, set 1 SSBO, set 2 sampler (demon_hunt feedback)
    VkCullModeFlags dynamic_cull_mode;                           // Last requested dynamic cull mode
    VkFrontFace dynamic_front_face;                              // Last requested dynamic front-face
    bool dynamic_raster_state_initialized;                       // True after first explicit cull/front-face command
    VkPrimitiveTopology dynamic_primitive_topology;              // Last requested dynamic primitive topology
    bool dynamic_primitive_topology_initialized;                 // True after first explicit/implicit topology setup
    VkIndexType current_index_type;                              // Last bound index type for indexed draws
    size_t bound_ibo_index_element_size;                         // 2 or 4 bytes per index (Vulkan bind offset validation)

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
    /* VD-1: lazy dynamic-rendering quad pipeline variants (VD color + depth formats). */
    #define SIT_VK_QUAD_VD_DYNAMIC_PIPELINE_CACHE_MAX 8
    struct {
        uint32_t key;
        VkPipeline pipeline;
    } quad_vd_dynamic_pipelines[SIT_VK_QUAD_VD_DYNAMIC_PIPELINE_CACHE_MAX];
    uint32_t quad_vd_dynamic_pipeline_count;
    uint8_t* quad_owned_vs_spirv;
    uint8_t* quad_owned_fs_spirv;
    size_t quad_vs_spirv_size;
    size_t quad_fs_spirv_size;
    VkPipeline ypq_grade_pipeline;                               // Pipeline for YPQ grade textured draws
    VkPipelineLayout ypq_grade_pipeline_layout;                  // Layout for YPQ grade pipeline
    VkBuffer quad_vertex_buffer;                                 // Vertex buffer for unit quad
    VmaAllocation quad_vertex_buffer_memory;                     // Memory for quad vertex buffer
    SituationTexture quad_solid_texture;                         // 1x1 white — set 1 sampler for solid DrawQuad

    VkPipeline text_pipeline;                                    // Pipeline for Batched Text renderer
    VkPipelineLayout text_pipeline_layout;                       // Layout for Batched Text renderer

    VkPipeline vd_compositing_pipeline;                          // Alpha-blend VD pipeline (alias of blend_pipelines[ALPHA])
    VkPipeline vd_compositing_blend_pipelines[5];                // ALPHA..NONE simple compositor variants
    VkPipelineLayout vd_compositing_pipeline_layout;             // Layout for simple VD composition
    VkPipeline advanced_compositing_pipeline;                    // Pipeline for advanced blend modes
    VkPipelineLayout advanced_compositing_pipeline_layout;       // Layout for advanced blend modes

    VkDescriptorSetLayout vd_pattern_config_ubo_layout;          // Set 2/3: SitTpConfigBlock UBO + layer-params SSBO
    VkBuffer vd_pattern_config_ubo;
    VmaAllocation vd_pattern_config_ubo_memory;
    VkBuffer vd_pattern_config_ssbo;
    VmaAllocation vd_pattern_config_ssbo_memory;
    VkDescriptorSet vd_pattern_config_descriptor_set;
    VkDescriptorPool vd_pattern_config_descriptor_pool;

    // [PIPELINE STATE]
    VkPipeline current_legacy_pipeline;
    VkPipeline current_pbr_pipeline;
    struct _SituationShaderSlot* current_bound_shader_slot;      // For stride-based pipeline selection at draw time
    size_t current_graphics_vertex_stride;                       // Last stride from SituationCmdBindVertexBuffer

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

    /* Offscreen canvas for exclusive fullscreen (render at windowed res, stretch to swapchain). */
    VkImage canvas_color_image;
    VmaAllocation canvas_color_memory;
    VkImageView canvas_color_view;
    VkImage canvas_depth_image;
    VmaAllocation canvas_depth_memory;
    VkImageView canvas_depth_view;
    VkFramebuffer canvas_framebuffer;
    uint32_t canvas_resource_width;
    uint32_t canvas_resource_height;

    VkDescriptorPool screen_copy_descriptor_pool;                // [FIX v2.3.27B] Track the pool that owns the screen copy set

    // [Bindless] Global Descriptor Set
    VkDescriptorSet global_bindless_set;
    VkDescriptorPool global_bindless_pool;
    /** Phase D5: dedupe vkCmdBindDescriptorSets for global bindless (per frame, per pipeline layout). */
    bool global_bindless_graphics_bound;
    VkPipelineLayout global_bindless_graphics_layout;

    _SituationStagingBuffer staging_buffers[SITUATION_MAX_FRAMES_IN_FLIGHT];     // [NEW] Per-Frame Staging Buffers

    // --- Graveyard (Deferred Deletion Queue) ---
    struct _SituationVKGraveyard* graveyards;                    // Array of deletion queues (one per frame in flight)

    // --- Threading Signals ---
    atomic_bool recreate_swapchain_request;                      // Signal from Render Thread to Main Thread
    bool swapchain_valid; // [FIX v2.3.27B]
    uint32_t consecutive_acquire_timeouts;                       // Consecutive VK_TIMEOUT from vkAcquireNextImageKHR (force recreate after threshold)
    bool surface_supports_10bit_sdr;                             // WSI lists A2R10G10B10 + SRGB_NONLINEAR (Phase 1)
    bool surface_supports_hdr10;                                   // WSI lists A2R10G10B10 + HDR10_ST2084 (Phase 6)
    uint32_t swapchain_color_space;                              // VkColorSpaceKHR of active swapchain
    uint32_t acquired_image_indices[SITUATION_MAX_FRAMES_IN_FLIGHT]; // Image index for each frame slot

    uint64_t staging_buffer_size; // Configured staging buffer size

    /* P10.3 GPU timestamp query pool (not swapchain-scoped). */
    VkQueryPool gpu_timestamp_pool;
    float gpu_timestamp_period_ns;

    // Render Pass Cache
    _SituationCachedRenderPass render_pass_cache[32];
    uint32_t render_pass_cache_count;

    // Pre-present screenshot (parity with OpenGL `screenshot_buffer` — see LIBRARY_BUGFIX_PLAN V6)
    VkBuffer screenshot_staging_buffer;
    VmaAllocation screenshot_staging_allocation;
    uint8_t* screenshot_buffer;
    int screenshot_width;
    int screenshot_height;
    bool screenshot_valid;
    bool screenshot_requested;  // request capture in this frame's EndFrame
    /** Frame slot whose pixels are in screenshot_buffer (UINT32_MAX when stale). */
    uint32_t screenshot_resolved_frame_index;
    /** Per frame-in-flight slot: copy command recorded for that slot's submission (required when render thread may lag main). */
    bool screenshot_copy_pending[SITUATION_MAX_FRAMES_IN_FLIGHT];
    mtx_t screenshot_mutex;
    bool screenshot_mutex_initialized;

    /** Recording state: main swapchain render pass open (SituationCmdBeginRenderPass with display_id == -1). Prevents stray vkCmdEndRenderPass in VD composite. */
    bool inside_main_swapchain_render_pass;
    bool inside_render_pass;
    VkRect2D current_render_area;
    int recording_pass_display_id;   // -1 main window, >= 0 VD while render pass recording
    int recording_pass_rt_slot;        // >= 0 user render target while render pass recording; -1 otherwise
    bool recording_pass_had_draw;
    int compute_bound_texture_slots[SIT_VD_MAX_COMPUTE_TEXTURE_BINDS]; // -1 unset

    // [Phase 6B] Raster dynamics tracked at end of struct (avoid shifting screenshot/pipeline field layout).
    VkPolygonMode dynamic_polygon_mode;                          // Last requested polygon mode (default FILL)
    VkBool32 dynamic_depth_bias_enable;                          // Last requested depth bias enable
    float dynamic_depth_bias_constant;                           // Vulkan depthBiasConstantFactor / GL units
    float dynamic_depth_bias_clamp;                              // Vulkan depthBiasClamp
    float dynamic_depth_bias_slope;                              // Vulkan depthBiasSlopeFactor / GL factor
    bool extended_dynamic_state_enabled;                         // VK_EXT_extended_dynamic_state (topology, depth)
    bool extended_dynamic_state3_polygon_mode_enabled;           // VK_EXT_extended_dynamic_state3 polygon mode
    bool extended_dynamic_state3_color_write_enabled;              // VK_EXT_extended_dynamic_state3 color write mask
    bool dynamic_rendering_enabled;                                  // Vulkan 1.3+ dynamic rendering (VD-1)
    bool depth_bias_dynamic_enabled;                             // VK_EXT_extended_dynamic_state2 depth bias dynamics
    PFN_vkCmdSetPolygonModeEXT pfn_cmd_set_polygon_mode_ext;
    PFN_vkCmdSetPrimitiveTopology pfn_cmd_set_primitive_topology;
    PFN_vkCmdSetDepthTestEnable pfn_cmd_set_depth_test_enable;
    PFN_vkCmdSetDepthWriteEnable pfn_cmd_set_depth_write_enable;
    PFN_vkCmdSetDepthCompareOp pfn_cmd_set_depth_compare_op;
    PFN_vkCmdSetDepthBiasEnable pfn_cmd_set_depth_bias_enable;
    PFN_vkCmdSetDepthBias pfn_cmd_set_depth_bias;
    PFN_vkCmdSetColorWriteMaskEXT pfn_cmd_set_color_write_mask_ext;
    PFN_vkCmdSetStencilTestEnable pfn_cmd_set_stencil_test_enable;
    PFN_vkCmdSetStencilOp pfn_cmd_set_stencil_op;

    VkColorComponentFlags dynamic_color_write_mask;
    VkBool32 dynamic_depth_test_enable;
    VkBool32 dynamic_depth_write_enable;
    VkCompareOp dynamic_depth_compare_op;
    VkBool32 dynamic_stencil_test_enable;
    SituationStencilState dynamic_stencil_front;
    SituationStencilState dynamic_stencil_back;
    float dynamic_line_width;
    // Multisample shadow state (tracking only; no dynamic VK path without ext_dynamic_state3)
    bool dynamic_ms_sample_shading_enable;
    float dynamic_ms_min_sample_shading;
    uint32_t dynamic_ms_sample_mask;
    bool dynamic_ms_alpha_to_coverage_enable;

    _SitVulkanRasterStackEntry raster_stack[SITUATION_MAX_RASTER_STACK_DEPTH];
    int raster_stack_depth;
    SituationRendererBehaviorPolicy behavior;
    SituationRendererBehaviorPolicy behavior_stack[SITUATION_MAX_BEHAVIOR_STACK_DEPTH];
    int behavior_stack_depth;

#if SIT_VK_SHADER_CACHE_PHASE2
    VkPipelineCache       pipeline_cache;              /* driver pipeline cache (Phase 2C) */
    uint32_t              render_pass_compatibility_id; /* bumped on render-pass recreate */
#endif
    /* [Shader Cache Phase 1] Vulkan shader cache — SPIR-V blobs, module pairs, pipeline bundles. */
    _SitVkShaderCache shader_cache;

#if !defined(NDEBUG)
    /** [6-bisC] Debug-only pipeline variant selection counters (Vulkan raster fallback). */
    uint64_t raster_pipeline_resolve_count;
    uint64_t raster_polygon_variant_hits;
    uint64_t raster_cull_front_variant_hits;
    uint64_t raster_pipeline_rebind_count;
#endif

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

    GLuint* programs_to_delete;     // Shader program IDs (Phase 4 program cache eviction)
    size_t program_count;
    size_t program_capacity;
} _SituationGLGraveyard;

// OpenGL-only declarations (inside SITUATION_USE_OPENGL guard above).
// [Phase 4] Multi-Draw Indirect Structures for glMultiDraw*Indirect.
// Vulkan uses VkDrawIndirectCommand / VkDrawIndexedIndirectCommand directly.
typedef struct {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t first;
    uint32_t baseInstance;
} _SituationGLDrawArraysIndirectCommand;

typedef struct {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t baseInstance;
} _SituationGLDrawElementsIndirectCommand;

/**
 * @brief [INTERNAL] OpenGL backend state container.
 * @details Holds all global OpenGL objects and state variables managed by the library.
 *          This includes internal shaders (Quad, Virtual Display), global buffers (UBOs),
 *          and caching variables for optimizing state changes.
 */
// [Phase 5] OpenGL virtual bindless fallback (texture-unit LRU cache).
// Not related to SituationVirtualDisplay; it is the number of GL texture units
// reserved for the fallback when true bindless texture handles are unavailable.
#define SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS 32

typedef struct _SituationGLVirtualTextureSlot {
    uint32_t texture_slot_index;     // The actual GL texture unit (0-31)
    GLuint gl_texture_id;            // The GL ID of the texture currently bound here
    uint64_t last_used_counter;      // For LRU eviction
    bool is_active;                  // Is this slot currently holding a valid texture?
} _SituationGLVirtualTextureSlot;

typedef struct _SituationGLVirtualBindlessStats {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
} _SituationGLVirtualBindlessStats;

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

    /* Offscreen canvas for exclusive fullscreen (render at windowed res, stretch to backbuffer). */
    GLuint canvas_fbo;
    GLuint canvas_color_tex;
    GLuint canvas_depth_rbo;
    int canvas_resource_width;
    int canvas_resource_height;

    // Screenshot capture (pre-swap readback for reliable pixel capture)
    GLuint screenshot_pbo;                      // PBO for async readback before swap
    uint8_t* screenshot_buffer;                 // CPU-side copy of last frame's pixels
    int screenshot_width;                       // Width of captured frame
    int screenshot_height;                      // Height of captured frame
    bool screenshot_valid;                      // True if screenshot_buffer has valid data
    bool screenshot_requested;                  // If true, EndFrame will do pre-swap capture (for TakeScreenshot / LoadFromScreen)
    /** Per-slot capture at EndFrame handoff (RT) or pre-swap (ST); cleared after capture. */
    bool screenshot_request_pending[SITUATION_MAX_FRAMES_IN_FLIGHT];
    /** Set by LoadImageFromScreen on cache miss (RT only); render thread checks before pre-swap. */
    atomic_uint_least8_t screenshot_urgent[SITUATION_MAX_FRAMES_IN_FLIGHT];
    int screenshot_resolved_frame_index;        // Slot whose pixels are in screenshot_buffer (-1 when stale)
    /** Monotonic epoch bumped at each RT EndFrame invalidation; buffer_epoch must match for cache hit. */
    uint64_t screenshot_capture_epoch;
    uint64_t screenshot_buffer_epoch;
    mtx_t screenshot_mutex;
    bool screenshot_mutex_initialized;

    GLuint quad_shader_program;                 // Shader program for the 2D Quad/Text renderer
    GLuint ypq_grade_shader_program;            // Shader program for YPQ texture grade draws
    GLuint quad_vao;                            // Private VAO for 2D quads
    GLuint quad_vbo;                            // Private VBO for 2D quads

    GLuint text_shader_program;                 // Shader program for Batched Text renderer
    GLuint text_vao;                            // Private VAO for Batched Text
    GLuint text_vbo;                            // Private dynamic VBO for Batched Text

    // -------------------------------------------------------------------------
    // Global Resources & State
    // -------------------------------------------------------------------------
    GLuint view_data_ubo_id;                    // Handle to the global View/Projection UBO
    GLuint vd_pattern_config_ubo_id;           // std140 SitTpConfigBlock header for compositor PATTERN idle
    GLuint vd_pattern_config_ssbo_id;          // std430 SitTpLayerParamsBlock (P10)
    GLuint global_vao_id;                       // The "Public" VAO active during user rendering commands
    size_t bound_ibo_byte_offset;               // Byte offset from last SIT_OP_BIND_INDEX_BUFFER (added to indexed draw indices)
    GLenum current_index_type;                  // GL_UNSIGNED_SHORT or GL_UNSIGNED_INT from last bind
    size_t bound_ibo_index_element_size;        // 2 or 4 bytes per index for draw offset math
    GLuint mesh_vao_id;                         // [2.3.19] Shared VAO for standard meshes (PBR layout)
    GLuint current_program_id;                  // Cache of the currently bound shader program ID

    // Shadow State (Tracks what we *think* the driver state is)
    GLuint current_bound_texture_id; // [v2.3.31] Track bound texture for legacy/quad draws
    GLuint current_vao_id;
    GLuint current_fbo_id;
    int current_target_width;
    int current_target_height;
    int    blend_enabled;
    GLenum blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
    GLenum blend_eq_rgb, blend_eq_alpha;
    int    depth_test_enabled;
    int    cull_face_enabled;
    int    scissor_test_enabled;
    GLenum current_primitive_mode;              // Draw topology mode when current_primitive_mode_set
    bool current_primitive_mode_set;            // False => default GL_TRIANGLES (GL_POINTS is 0; do not use truthiness on mode)
    GLenum current_polygon_mode;                // GL_FILL / GL_LINE / GL_POINT (default GL_FILL)
    bool polygon_offset_enabled;                // GL_POLYGON_OFFSET_FILL enabled for depth bias

    #if defined(SITUATION_ENABLE_SHADER_COMPILER)
    bool arb_spirv_available;                   // True if GL_ARB_gl_spirv extension is supported
    #endif
    bool parallel_shader_compile_available;     // GL_KHR/ARB_parallel_shader_compile
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
    _SitGLProgramCache program_cache;    /* Phase 4: Layer A source hash → GLuint program */
    _SituationGLGraveyard graveyards[SITUATION_MAX_FRAMES_IN_FLIGHT];
    GLsync frame_fences[SITUATION_MAX_FRAMES_IN_FLIGHT];
    int last_applied_swap_interval; // [v2.4.319] Tracks current glfwSwapInterval to avoid redundant per-frame calls

    // [Phase 4] Multi-Draw Indirect
    GLuint mdi_buffer_id;
    void* mdi_data_ptr;
    size_t mdi_ring_size;
    atomic_size_t mdi_ring_head;

    // [Phase 5] Virtual Bindless Fallback
    _SituationGLVirtualTextureSlot virtual_texture_slots[SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS];
    _SituationGLVirtualBindlessStats virtual_stats;
    uint64_t virtual_lru_counter;
    GLint current_virtual_loc; // [Phase 5] Cache for virtual bindless uniform location

    /* P10.3 GL_TIME_ELAPSED queries — one per zone per frame slot. */
    GLuint gpu_elapsed_queries[SITUATION_MAX_FRAMES_IN_FLIGHT][SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT];
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

    // --- Guarded pump state (defer per-frame edges during non-primary glfwPollEvents) ---
    int pump_guard_depth;                                   // >0 inside _SituationPumpWindowEventsGuarded
    bool pending_down[GLFW_KEY_LAST + 1];                   // Key presses captured during guarded pumps
    bool pending_up[GLFW_KEY_LAST + 1];                     // Key releases captured during guarded pumps
    bool pending_mouse_down[GLFW_MOUSE_BUTTON_LAST + 1];    // Mouse presses captured during guarded pumps
    bool pending_mouse_up[GLFW_MOUSE_BUTTON_LAST + 1];      // Mouse releases captured during guarded pumps
    float pending_scroll_x;                                 // Scroll X accumulated during guarded pumps
    float pending_scroll_y;                                 // Scroll Y accumulated during guarded pumps
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
        /* Must match sit_echo_t in sit/aud/fx/echo.h (cast in situation_impl_audio). */
        struct {
            ma_delay delay;
            uint32_t channels;
            bool is_initialized;
            float* dry_scratch;
            float current_feedback;
            float current_wet;
            float target_feedback;
            float target_wet;
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

    // [Errno Adoption Phase 8] Non-fatal status for main-thread polling
    atomic_int last_status; // Set by audio thread (e.g., SITUATION_ERROR_AUDIO_STREAM_ENDED), polled by main thread
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
    bool route_to_graph;                // Route this tone to the graph's SFX sound source instead of mixing directly to pOut
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

    /** True for the body of sit_miniaudio_data_callback (Phase 15 graph teardown). */
    atomic_bool is_in_audio_callback;

    // [FIX v2.4.38] Audio callback race condition guard.
    // Set to true at the END of audio init, after all state (device registry, default graph) is ready.
    // The audio callback returns silence if this is false.
    atomic_bool audio_ready;

    // Resonance Module State
    SituationTone tone_pool[SITUATION_MAX_TONES];
    uint32_t tone_generations[SITUATION_MAX_TONES];

    // Audio Output Monitoring (for visualization)
    _Atomic(void*) output_monitor_callback;
    _Atomic(void*) output_monitor_user_data;

    /* Last playback block levels (written from audio callback; read from main/UI via SituationGetMasterOutputMeter). */
    _Atomic float audio_meter_peak;
    _Atomic float audio_meter_rms;

    // [Phase H] Node Graph Integration
    SituationAudioGraph*    active_graph;    // Currently active processing graph (NULL = legacy path)
    SituationAudioGraph*    default_graph;   // Auto-created minimal graph (Sound Source + Tone Synth → Mixer)
    void*                   default_graph_voice_source; // SituationSoundSource* — default graph SITUATION_NODE_SOUND_SOURCE (Policy B voice bus)
    void*                   sfx_graph_voice_source;     // SituationSoundSource* — target node for routed procedural SFX tones
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
    bool gl_pending_link_spirv; /* false = GLSL hot-reload (build uniform map) */
    /* Non-blocking GLSL load: VS compile -> FS compile -> async link (polled each frame). */
    uint8_t gl_async_load_stage; /* 0=idle, 1=GLSL compile poll, 2=SPIR-V specialize poll */
    GLuint gl_async_vs_shader;
    GLuint gl_async_fs_shader;
    uint8_t* gl_spirv_vs_copy;
    uint8_t* gl_spirv_fs_copy;
    size_t gl_spirv_vs_len;
    size_t gl_spirv_fs_len;
    uint8_t gl_spirv_substage; /* 0=specialize VS, 1=specialize FS, 2=link dispatched */
    /* Phase 4: non-NULL when gl_program_id is borrowed from the shared program cache. */
    _SitGLProgramCacheRef gl_program_cache_ref;
#elif defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipeline vk_pipeline_legacy;
    VkPipeline vk_pipeline_simple;       // Position-only vertex layout (stride = 3*float)
    VkPipeline vk_pipeline_back_ccw;
    VkPipeline vk_pipeline_back_cw;
    VkPipeline vk_pipeline_legacy_back_ccw;
    VkPipeline vk_pipeline_legacy_back_cw;
    VkPipeline vk_pipeline_simple_back_ccw;
    VkPipeline vk_pipeline_simple_back_cw;
    VkPipeline vk_pipeline_line;                                 // Static wireframe (polygonMode LINE, no cull)
    VkPipeline vk_pipeline_legacy_line;
    VkPipeline vk_pipeline_simple_line;
    VkPipelineLayout vk_pipeline_layout;
    SituationSpirvLayoutProfile vk_spirv_layout_profile; // For descriptor bind path (Phase 2+)
    bool vk_owns_pipeline_layout;        // false when vk_pipeline_layout is a global cache entry
    void* vk_async_load;                 /* _SituationVkAsyncShaderLoad* while compile/pipeline build pending */
    /* [Shader Cache Phase 1] Non-NULL when a cached bundle is attached to this slot.
     * CRITICAL SAFETY: never read bundle_ref.bundle directly — always call _SitVkDerefBundle(). */
    _SitVkPipelineBundleRef vk_bundle_ref;
    /* Hot-path draw cache: last resolved pipeline from bundle (avoids repeat deref per draw). */
    VkPipeline vk_bound_pipeline_cache;
    /* VD-1: SPIR-V source used for last pipeline build (slot-owned copy when pinned). */
    const void* vk_last_vs_spirv;
    size_t vk_last_vs_size;
    const void* vk_last_fs_spirv;
    size_t vk_last_fs_size;
    uint8_t* vk_owned_last_vs_spirv;
    uint8_t* vk_owned_last_fs_spirv;
    /* VD-1: lazy dynamic-rendering pipeline variants (mirrors active RP pipeline family + VD formats). */
    #define SIT_VK_VD_DYNAMIC_PIPELINE_CACHE_MAX 32
    struct {
        uint32_t key;
        VkPipeline pipeline;
    } vk_vd_dynamic_pipelines[SIT_VK_VD_DYNAMIC_PIPELINE_CACHE_MAX];
    uint32_t vk_vd_dynamic_pipeline_count;
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
    SituationMeshVertexLayout vertex_layout;
    /** CPU copy of upload data (CreateMesh); GetMeshData prefers this over GPU readback. */
    uint8_t* cpu_vertices;
    size_t cpu_vertices_size;
    uint8_t* cpu_indices;
    size_t cpu_indices_size;
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
    VkDescriptorSetLayout vk_cached_descriptor_layout; /* layout used for descriptor_set (VK_NULL = none) */
    VkDescriptorType vk_cached_descriptor_type;
#endif

    // [Phase 1] Async Readback Metadata
    bool is_readback;
    void* mapped_ptr;
    size_t mapped_size;
    bool readback_is_host_coherent;
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
    bool is_stl;
    bool is_obj;
    bool stl_smooth_normals;
} _SituationModelSlot;

// --- Internal Texture Slot Definition ---
typedef struct _SituationTextureSlot {
    bool is_active;
    uint32_t generation; // Increments every time this slot is recycled
    int width;
    int height;
    int mip_levels;
    SituationTextureFormat format_api;
    SituationTextureUsageFlags usage_flags;
    SituationTextureLayout layout_hint; // Phase 14 Slice 3: last known layout (hint only; not validation truth).
    SituationTextureFilter min_filter;
    SituationTextureFilter mag_filter;
    SituationTextureWrap wrap_s;
    SituationTextureWrap wrap_t;
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
    /** Extra set: single combined image sampler at binding 0 (`text_sampler_layout`) for
     *  SituationLoadShaderFromMemory pipelines (set 1). Bindless textures only populate the
     *  global array; this set lets SituationCmdBindTextureSet(cmd, 1, tex) work with harness shaders. */
    VkDescriptorSet single_sampler_descriptor_set;
    VkDescriptorPool single_sampler_descriptor_pool;
#elif defined(SITUATION_USE_OPENGL)
    GLuint gl_texture_id;
    GLenum internal_format;             // Texture internal format (GL_RGBA8 or GL_SRGB8_ALPHA8)
    uint64_t gl_bindless_handle; // [Phase 3] Bindless Handle
    int gl_image_binding_unit;          // Image unit when bound via glBindImageTexture (-1 if none)
#endif
} _SituationTextureSlot;

typedef struct _SituationRenderTargetSlot {
    bool is_active;
    uint32_t generation;
    int width;
    int height;
    bool has_depth;
    int texture_slot_index;
#if defined(SITUATION_USE_VULKAN)
    VkImage color_image;
    VmaAllocation color_memory;
    VkImageView color_view;
    VkImage depth_image;
    VmaAllocation depth_memory;
    VkImageView depth_view;
    VkImageLayout color_layout;
#elif defined(SITUATION_USE_OPENGL)
    GLuint fbo_id;
    GLuint color_texture_id;
    GLuint depth_rbo_id;
#endif
} _SituationRenderTargetSlot;

typedef struct _SituationQueryPoolSlot {
    bool is_active;
    uint32_t generation;
    SituationQueryType type;
    uint32_t query_count;
#if defined(SITUATION_USE_VULKAN)
    VkQueryPool vk_pool;
#elif defined(SITUATION_USE_OPENGL)
    GLuint* gl_queries;
#endif
} _SituationQueryPoolSlot;

/* SituationVirtualDisplay::backend[] overlay — public type is opaque uint64_t[N] in situation_api_types_gpu.h */
#if defined(SITUATION_USE_VULKAN)
typedef struct SitVirtualDisplayVkBackend {
    VkImage          image;
    VmaAllocation    image_memory;
    VkImageView      image_view;
    VkImage          depth_image;
    VmaAllocation    depth_image_memory;
    VkImageView      depth_image_view;
    VkSampler        sampler;
    VkDescriptorSet  descriptor_set;
    VkDescriptorPool descriptor_pool;
    VkImageLayout    color_image_layout;
} SitVirtualDisplayVkBackend;

_Static_assert(sizeof(SitVirtualDisplayVkBackend) <= SIT_VD_BACKEND_STORAGE_U64_COUNT * sizeof(uint64_t),
               "SitVirtualDisplayVkBackend exceeds SituationVirtualDisplay.backend storage");

static inline SitVirtualDisplayVkBackend* SitVDVk(SituationVirtualDisplay* vd) {
    return (SitVirtualDisplayVkBackend*)(void*)vd->backend;
}

static inline const SitVirtualDisplayVkBackend* SitVDVkConst(const SituationVirtualDisplay* vd) {
    return (const SitVirtualDisplayVkBackend*)(const void*)vd->backend;
}

#elif defined(SITUATION_USE_OPENGL)
typedef struct SitVirtualDisplayGlBackend {
    GLuint fbo_id;
    GLuint texture_id;
    GLuint depth_rbo_id;
} SitVirtualDisplayGlBackend;

_Static_assert(sizeof(SitVirtualDisplayGlBackend) <= SIT_VD_BACKEND_STORAGE_U64_COUNT * sizeof(uint64_t),
               "SitVirtualDisplayGlBackend exceeds SituationVirtualDisplay.backend storage");

static inline SitVirtualDisplayGlBackend* SitVDGl(SituationVirtualDisplay* vd) {
    return (SitVirtualDisplayGlBackend*)(void*)vd->backend;
}

static inline const SitVirtualDisplayGlBackend* SitVDGlConst(const SituationVirtualDisplay* vd) {
    return (const SitVirtualDisplayGlBackend*)(const void*)vd->backend;
}
#endif

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

    _SituationRenderTargetSlot render_target_slots[SITUATION_MAX_RENDER_TARGETS];
    bool render_target_slots_used[SITUATION_MAX_RENDER_TARGETS];
    int active_render_target_count;

    _SituationQueryPoolSlot query_pool_slots[SITUATION_MAX_QUERY_POOLS];
    bool query_pool_slots_used[SITUATION_MAX_QUERY_POOLS];
    int active_query_pool_count;
    int active_occlusion_pool_slot;       // P10.4: -1 when no occlusion query open
    uint32_t active_occlusion_query_index;

    // -------------------------------------------------------------------------
    // Profiling
    // -------------------------------------------------------------------------
    uint32_t frame_draw_calls;                                // Counter for draw commands issued this frame
    uint32_t frame_triangle_count;                            // Estimate of triangles drawn this frame
    double last_vd_composite_time_ms;                         // Profiling timer for the composition pass
    bool gpu_timestamps_supported;                            // P10.3: ARB_timer_query / VK timestamp queries
    uint64_t gpu_zone_ns[SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT]; // P10.3: last readback per zone (ns)
    uint32_t gpu_zone_completed[SITUATION_MAX_FRAMES_IN_FLIGHT]; // bitmask: zone had begin+end this slot
    uint16_t gpu_zone_open[SITUATION_MAX_FRAMES_IN_FLIGHT];      // bitmask: zone begin without end yet

    // -------------------------------------------------------------------------
    // Internal Resource Tracking (Linked Lists)
    // -------------------------------------------------------------------------
    // Legacy linked lists replaced by registries                          // Head of the model tracking list

    // -------------------------------------------------------------------------
    // Feature Capabilities
    // -------------------------------------------------------------------------
    uint64_t enabled_features_mask;                           // Bitmask of SituationRenderFeature flags enabled on the current backend
    uint8_t output_bits_per_channel;                          // 8 or 10 after init (0 treated as 8 in caps queries)
    uint8_t output_color_depth_active;                        // 1 when 10-bit swapchain / default FB is active
    uint8_t output_hdr_active;                                // 1 when HDR10 ST2084 swapchain is active (Phase 6)
    uint8_t output_color_space;                               // SituationOutputColorSpace

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
    /* Occupancy count (guarded by render_queue_mutex). Required because the ring may be
     * completely full (head == tail with MAX_FRAMES_IN_FLIGHT queued), which is otherwise
     * indistinguishable from empty — relying on head == tail alone wedges the render thread. */
    int render_queue_count;
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
    atomic_uint_least64_t metric_max_latency_ns; // Windowed max for adaptive backpressure (reset every SIT_LATENCY_METRIC_WINDOW_FRAMES)
    atomic_uint_least32_t metric_window_frame_count; // Frames since last metric_max_latency_ns reset
    atomic_int frame_refcounts[SITUATION_MAX_FRAMES_IN_FLIGHT];
    atomic_bool drift_warned;

    /* [Phase 2] Present-anchored display timing (RT publish / ST direct / main consume). */
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    atomic_uint_least64_t last_present_complete_time_ns;
    atomic_uint_least64_t latest_present_delta_ns;
    atomic_uint_least32_t present_timing_seq;
    uint32_t last_consumed_present_seq;
    atomic_uint_least32_t fps_present_counter;
    uint64_t st_last_present_complete_time_ns;
    uint64_t fps_monotonic_window_start_ns;
#endif

    /* [Phase 3] Runtime in-flight cap when VSync or target FPS is active (else MAX). */
    int paced_frames_in_flight;

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
    bool momentum_mutex_initialized;

    // [v2.3.34] Thread Safety for Resource Lists (Hot-Reload offload)
    mtx_t resource_registry_mutex;
    bool resource_registry_mutex_initialized;

    // [v2.3.40] Initialization state for safe multi-threaded resource creation
    atomic_int init_state;  // SituationInitState

    // [v2.4.206] Cached GL limits for main-thread access (GL context may not be current)
    int cached_max_viewports;

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
    SituationError last_error_code;                           // Enum from the most recent _SituationSetErrorFromCode call
    ma_mutex error_mutex;                                     // Mutex protecting concurrent access to the error buffer
    atomic_bool is_initialized;                               // Flag indicating if SituationInit() has completed successfully
    bool is_com_initialized;                                  // Flag indicating if Windows COM was initialized by this library
    bool input_mutexes_initialized;                           // Flag indicating if input subsystem mutexes were initialized

    // -------------------------------------------------------------------------
    // Window State Management
    // -------------------------------------------------------------------------
    GLFWwindow* sit_glfw_window;                              // The primary GLFW window handle
    int main_window_width;                                    // Current framebuffer/render width in pixels
    int main_window_height;                                   // Current framebuffer/render height in pixels
    int window_pos_x;                                         // Current window position X (updated by OS move callback)
    int window_pos_y;                                         // Current window position Y (updated by OS move callback)
    int windowed_x;                                           // Saved X position before entering fullscreen/borderless
    int windowed_y;                                           // Saved Y position before entering fullscreen/borderless
    int windowed_w;                                           // Saved width before entering fullscreen/borderless
    int windowed_h;                                           // Saved height before entering fullscreen/borderless
    int render_canvas_width;                                  // Fixed render coordinate width (windowed size; preserved in FS)
    int render_canvas_height;                                 // Fixed render coordinate height (windowed size; preserved in FS)
    int fullscreen_w;                                         // Native monitor width when in exclusive fullscreen
    int fullscreen_h;                                         // Native monitor height when in exclusive fullscreen

    bool current_window_focus_state;                          // True if the window currently has input focus
    bool was_minimized_last_frame;                            // State tracker for detecting minimize/restore transitions
    bool is_app_internally_paused;                            // Master pause flag for suspending audio/updates
    bool was_window_resized_last_frame;                       // Event flag indicating a resize occurred this frame
    bool is_borderless_active;                                // True if "fake fullscreen" borderless mode is active

    uint32_t active_profile_window_flags;                     // Target window flags to apply when focused
    uint32_t inactive_profile_window_flags;                   // Target window flags to apply when unfocused
    uint32_t cached_window_state_flags;                       // Result of last GetCurrentActualWindowStateFlags — refreshed by SituationPollInputEvents

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
    double target_frame_time;                                 // Target duration per frame (1.0 / target_fps)  -- restored after accidental removal during spike monitoring field additions
    double max_frame_time;                                    // Highest observed frame delta (for spike detection, general debugging)
    uint32_t frame_spike_count;                               // Count of frames exceeding spike threshold (general debug aid)
    uint32_t frame_time_hist[6];                              // Basic histogram buckets for frame times (debug aid, general use): [0]<1ms [1]1-2 [2]2-4 [3]4-8 [4]8-16 [5]>16

    // Phase timing for last frame (ns) - for spike cause attribution in debugging
    uint64_t last_backpressure_ns;
    uint64_t last_fence_wait_ns;
    uint64_t last_execute_ns;
    uint64_t last_present_ns;

    // Core engine timing for general debugging (not just rendering)
    uint64_t last_poll_ns;
    uint64_t last_update_ns;

    // Finer-grained core timings for debugging poll stutters
    uint64_t last_glfw_poll_ns;
    uint64_t last_input_reset_ns;
    uint64_t last_joystick_ns;
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
    SituationMaximizeCallback maximize_callback_fn;           // Callback invoked on window maximize / restore
    void* maximize_callback_user_ptr;                         // User context for maximize callback
    SituationFileDropCallback file_drop_callback;             // Callback invoked on file drop
    void* file_drop_user_data;                                // User context for file drop callback
    void (*log_callback)(SituationLogLevel, const char*, void*); // Custom log callback
    void* log_user_data;                                      // User context for custom log callback
    SituationScreenshotFormat screenshot_format;              // Default screenshot format (BMP)

    // -------------------------------------------------------------------------
    // Environment & Filesystem
    // -------------------------------------------------------------------------
    int    argc;                                              // Command-line argument count
    char** argv;                                              // Command-line argument vector
    char** dropped_file_paths;                                // Array of paths dropped this frame (polling API)
    int    dropped_file_count;                                // Number of paths dropped this frame
    bool   file_was_dropped_this_frame;                       // Flag indicating if a drop event occurred

    // [Threading Bolstering] OS-visible main thread name (copied from SituationInitInfo; default "Sit Main")
    char main_thread_name[SITUATION_MAX_THREAD_NAME_LEN];

    // [Threading Bolstering] Optional affinity overrides (0 = built-in defaults at thread entry)
    uint64_t thread_affinity_main;
    uint64_t thread_affinity_render;
    uint64_t thread_affinity_audio;
    bool  numa_prefer_local;
    bool  worker_numa_spread;
    int32_t io_thread_numa_node;
    bool thread_pool_use_physical_cores;
    uint32_t thread_pool_reserved_threads;

    /** Main-window output color depth policy from SituationInitInfo. */
    SituationOutputColorDepth output_color_depth_policy;

#if defined(SITUATION_ENABLE_THREADING)
    SituationThreadPool thread_pool;
#endif

} _SituationGlobalStateContainer;

#ifdef SITUATION_ENABLE_THREADING
// --- Threading Internal Helpers (forward-declared in situation_impl_forward.h) ---
#endif

#if !defined(__STDC_NO_THREADS__)
// [v2.3.21] Render Thread Lifecycle Helpers (forward-declared in situation_impl_forward.h)
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

static inline bool _SitRenderTargetHandleIsNull(SituationRenderTarget rt) {
    return rt.generation == 0u;
}

static inline bool _SitRenderPassInfoUsesRenderTarget(const SituationRenderPassInfo* info) {
    return info && info->render_target.generation != 0u;
}

static inline _SituationRenderTargetSlot* _SitGetRenderTargetSlot(SituationRenderTarget rt) {
    if (_SitRenderTargetHandleIsNull(rt)) {
        return NULL;
    }
    if (rt.slot_index >= SITUATION_MAX_RENDER_TARGETS) {
        return NULL;
    }
    _SituationRenderTargetSlot* slot = &sit_render.render_target_slots[rt.slot_index];
    if (!sit_render.render_target_slots_used[rt.slot_index] || !slot->is_active || slot->generation != rt.generation) {
        return NULL;
    }
    return slot;
}

static inline bool _SitQueryPoolHandleIsNull(SituationQueryPool pool) {
    return pool.generation == 0u;
}

static inline _SituationQueryPoolSlot* _SitGetQueryPoolSlot(SituationQueryPool pool) {
    if (_SitQueryPoolHandleIsNull(pool)) {
        return NULL;
    }
    if (pool.slot_index >= SITUATION_MAX_QUERY_POOLS) {
        return NULL;
    }
    _SituationQueryPoolSlot* slot = &sit_render.query_pool_slots[pool.slot_index];
    if (!sit_render.query_pool_slots_used[pool.slot_index] || !slot->is_active || slot->generation != pool.generation) {
        return NULL;
    }
    return slot;
}

/**
 * Pump OS window events without corrupting per-frame input state.
 * Used during GPU fence waits and swapchain operations to prevent
 * Windows "Not Responding" while keeping edge flags owned by SituationPollInputEvents().
 * Key presses/releases during this pump are deferred to pending_* and merged at the next poll.
 */
static inline void _SituationPumpWindowEventsGuarded(void) {
    if (!_sit_current_context) {
        glfwPollEvents();
        return;
    }
    sit_input.pump_guard_depth++;
    glfwPollEvents();
    sit_input.pump_guard_depth--;
}

/** Same guard semantics as _SituationPumpWindowEventsGuarded for glfwWaitEventsTimeout. */
static inline void _SituationWaitWindowEventsTimeoutGuarded(double timeout_seconds) {
    if (!_sit_current_context) {
        glfwWaitEventsTimeout(timeout_seconds);
        return;
    }
    sit_input.pump_guard_depth++;
    glfwWaitEventsTimeout(timeout_seconds);
    sit_input.pump_guard_depth--;
}

/** True when init policy requests 10-bit container (AUTO, 10BIT, or HDR10). */
static inline bool _SituationWants10BitOutput(SituationOutputColorDepth policy) {
    switch (policy) {
    case SIT_OUTPUT_COLOR_8BIT:
        return false;
    case SIT_OUTPUT_COLOR_10BIT:
    case SIT_OUTPUT_COLOR_HDR10:
    case SIT_OUTPUT_COLOR_AUTO:
    default:
        return true;
    }
}

/** True when policy requests HDR10 swapchain (explicit or AUTO with OS HDR on window monitor). */
static inline bool _SituationWantsHdr10Output(SituationOutputColorDepth policy, bool os_hdr_enabled_on_window) {
    switch (policy) {
    case SIT_OUTPUT_COLOR_HDR10:
        return true;
    case SIT_OUTPUT_COLOR_AUTO:
        return os_hdr_enabled_on_window;
    default:
        return false;
    }
}

/** Sync output color state and feature flags (10-bit SDR vs HDR10 are separate). */
static inline void _SituationSetOutputColorDepthState(bool active_10bit, bool active_hdr) {
    sit_render.output_color_depth_active = active_10bit ? 1u : 0u;
    sit_render.output_bits_per_channel = active_10bit ? 10u : 8u;
    sit_render.output_hdr_active = active_hdr ? 1u : 0u;
    if (active_hdr) {
        sit_render.output_color_space = (uint8_t)SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084;
        sit_render.enabled_features_mask |= (uint64_t)SIT_FEATURE_HDR_OUTPUT;
        sit_render.enabled_features_mask &= ~(uint64_t)SIT_FEATURE_10BIT_SDR_OUTPUT;
    } else if (active_10bit) {
        sit_render.output_color_space = (uint8_t)SIT_OUTPUT_COLOR_SPACE_SDR_SRGB;
        sit_render.enabled_features_mask &= ~(uint64_t)SIT_FEATURE_HDR_OUTPUT;
        sit_render.enabled_features_mask |= (uint64_t)SIT_FEATURE_10BIT_SDR_OUTPUT;
    } else {
        sit_render.output_color_space = (uint8_t)SIT_OUTPUT_COLOR_SPACE_SDR_SRGB;
        sit_render.enabled_features_mask &= ~((uint64_t)SIT_FEATURE_HDR_OUTPUT | (uint64_t)SIT_FEATURE_10BIT_SDR_OUTPUT);
    }
}

#if defined(SITUATION_USE_VULKAN)
/**
 * Wait on a fence without freezing the window: try non-blocking first (happy path is ~free),
 * then short timeouts + glfwPollEvents only while the GPU is still busy or wedged.
 * @param max_ns Total wait budget for the chunked loop (not per-iteration).
 */
static VkResult _SituationVulkanWaitFencePumpWindowBudget(VkDevice device, VkFence fence, uint64_t max_ns) {
    VkResult imm = vkWaitForFences(device, 1, &fence, VK_TRUE, 0);
    if (imm == VK_SUCCESS) return VK_SUCCESS;
    if (imm != VK_TIMEOUT) return imm;

    const uint64_t chunk_ns = 4000000ULL; /* 4 ms chunks for finer timing measurement and more responsive polling while waiting on GPU */
    uint64_t waited = 0;
    while (waited < max_ns) {
        uint64_t remain = max_ns - waited;
        uint64_t step = chunk_ns < remain ? chunk_ns : remain;
        VkResult r = vkWaitForFences(device, 1, &fence, VK_TRUE, step);
        if (r == VK_SUCCESS) return VK_SUCCESS;
        if (r != VK_TIMEOUT) return r;
        waited += step;
        _SituationPumpWindowEventsGuarded();
    }
    return VK_TIMEOUT;
}

static VkResult _SituationVulkanWaitFencePumpWindow(VkDevice device, VkFence fence) {
    return _SituationVulkanWaitFencePumpWindowBudget(device, fence, SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS);
}

/**
 * Bounded substitute for vkDeviceWaitIdle: wait each in-flight frame fence with short timeouts + glfwPollEvents.
 * Used on shutdown, swapchain teardown/recreate, VSync/full cleanup, and init error paths — anywhere indefinite idle would freeze the OS window.
 */
/* HARDENING: void by design — bounded shutdown wait with window pump; warnings only. */
static void _SituationVulkanWaitInFlightFencesPump(const char* context_label) {
    VkDevice device = sit_render.vk.device;
    if (device == VK_NULL_HANDLE) return;
    VkFence* fences = sit_render.vk.in_flight_fences;
    uint32_t mf = sit_render.vk.max_frames_in_flight;
    if (fences == NULL || mf == 0) {
        fprintf(stderr, "[Situation] WARNING: Vulkan (%s): no in-flight fences tracked; skipping bounded GPU idle wait.\n",
                context_label ? context_label : "unknown");
        return;
    }
    const uint64_t budget = SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS;
    for (uint32_t i = 0; i < mf; ++i) {
        VkFence f = fences[i];
        if (f == VK_NULL_HANDLE) continue;
        VkResult r = _SituationVulkanWaitFencePumpWindowBudget(device, f, budget);
        if (r == VK_TIMEOUT) {
            fprintf(stderr, "[Situation] WARNING: Vulkan (%s): in_flight_fences[%u] timed out (~%.1fs, SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS); continuing.\n",
                    context_label ? context_label : "unknown", i, (double)budget / 1e9);
        } else if (r != VK_SUCCESS) {
            fprintf(stderr, "[Situation] WARNING: Vulkan (%s): in_flight_fences[%u] vkWaitForFences returned %d; continuing.\n",
                    context_label ? context_label : "unknown", i, (int)r);
        }
    }
}

/* HARDENING: void by design — shutdown GPU idle pump wrapper. */
static void _SituationVulkanShutdownWaitGpuPump(void) { _SituationVulkanWaitInFlightFencesPump("SituationShutdown"); }

/**
 * Batched fence wait for swapchain recreation: waits ALL in-flight fences simultaneously
 * in a single vkWaitForFences call with event pump. Returns in one VSync period max rather
 * than N sequential periods (the old per-fence pump pattern).
 * Used by _SituationVulkanRecreateSwapchain after the new swapchain is created with oldSwapchain
 * handoff — GPU work referencing old resources must complete before destruction.
 */
static void _SituationVulkanWaitInFlightFencesBatched(void) {
    VkDevice device = sit_render.vk.device;
    if (device == VK_NULL_HANDLE) return;
    VkFence* fences = sit_render.vk.in_flight_fences;
    uint32_t mf = sit_render.vk.max_frames_in_flight;
    if (fences == NULL || mf == 0) return;

    /* Collect valid (non-null) fences into a contiguous array for the batched wait. */
    VkFence valid_fences[8]; /* max_frames_in_flight is typically 2-3, 8 is generous */
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < mf && valid_count < 8; ++i) {
        if (fences[i] != VK_NULL_HANDLE) {
            valid_fences[valid_count++] = fences[i];
        }
    }
    if (valid_count == 0) return;

    /* Single batched wait with short-chunk event pump to keep the window responsive.
     * Total budget: 500ms — far less than the old 3s-per-fence sequential approach,
     * but plenty for any reasonable GPU workload to retire (typically < 33ms). */
    const uint64_t chunk_ns = 8000000ULL; /* 8ms chunks — responsive pump, not too chatty */
    const uint64_t budget_ns = 500000000ULL; /* 500ms total budget */
    uint64_t waited = 0;
    while (waited < budget_ns) {
        uint64_t remain = budget_ns - waited;
        uint64_t step = chunk_ns < remain ? chunk_ns : remain;
        VkResult r = vkWaitForFences(device, valid_count, valid_fences, VK_TRUE, step);
        if (r == VK_SUCCESS) return;
        if (r != VK_TIMEOUT) return; /* error — bail, best-effort */
        waited += step;
        _SituationPumpWindowEventsGuarded();
    }
    /* Timeout — continue anyway (best-effort, same policy as shutdown path). */
    fprintf(stderr, "[Situation] WARNING: _SituationVulkanWaitInFlightFencesBatched timed out (500ms); continuing.\n");
}

/** Recreate a frame fence in the signaled state (used after failed submit / orphaned acquire). */
static void _SituationVulkanResignalFrameFence(uint32_t frame_index) {
    VkDevice device = sit_render.vk.device;
    if (device == VK_NULL_HANDLE || sit_render.vk.in_flight_fences == NULL) {
        return;
    }
    if (frame_index >= sit_render.vk.max_frames_in_flight) {
        return;
    }

    VkFence* slot = &sit_render.vk.in_flight_fences[frame_index];
    if (*slot != VK_NULL_HANDLE) {
        vkDestroyFence(device, *slot, NULL);
        *slot = VK_NULL_HANDLE;
    }

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    if (vkCreateFence(device, &fence_info, NULL, slot) != VK_SUCCESS) {
        *slot = VK_NULL_HANDLE;
    }
}

/** Undo an acquired-but-not-submitted frame so the next acquire does not hang on a dead fence. */
static void _SituationVulkanRecoverOrphanedFrameFence(void) {
    if (!sit_render.in_frame) {
        return;
    }
    sit_render.in_frame = false;
    sit_render.vk.inside_render_pass = false;
    sit_render.vk.inside_main_swapchain_render_pass = false;
    sit_render.vk.current_render_area = (VkRect2D){0};
    _SituationVulkanResignalFrameFence(sit_render.vk.current_frame_index);
}
#endif

// =================================================================================
// Shader Contract
// =================================================================================
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
#define SIT_UBO_BINDING_VD_PATTERN        6 // std140 SitTpConfigBlock header for compositor PATTERN idle (OpenGL).
#define SIT_SSBO_BINDING_VD_PATTERN       7 // std430 SitTpLayerParamsBlock (OpenGL, P10).

#define SIT_UNIFORM_LOC_VD_IS_IDLE          9 // int: 1 when compositor idle fallback is active (Phase 2a).
#define SIT_UNIFORM_LOC_VD_FALLBACK_MODE   10 // int: SituationVDFallbackMode when idle.
#define SIT_UNIFORM_LOC_VD_ELAPSED_IDLE    11 // float: seconds since last content write when idle.
#define SIT_UNIFORM_LOC_VD_FALLBACK_COLOR  12 // vec4: normalized SOLID idle RGBA.
#define SIT_UNIFORM_LOC_VD_COMPOSITOR_MODEL 8 // mat4: compositor.vert OpenGL SPIR-V (u_model; not loc 0 — aPos is 0).
#define SIT_UNIFORM_LOC_VD_SCREEN_TEXTURE   1 // sampler2D: vd.frag (OpenGL SPIR-V embed; layout(location=1)).
#define SIT_UNIFORM_LOC_COMPOSITE_SOURCE    1 // sampler2D: composite.frag source (layout(location=1)).
#define SIT_UNIFORM_LOC_COMPOSITE_DEST      7 // sampler2D: composite.frag destination (layout(location=7)).

/** Vulkan push-constant byte sizes (std430 layout, must match GLSL compositor shaders). */
#define SIT_VD_PATH_B_PUSH_CONSTANT_SIZE   104u
#define SIT_VD_PATH_A_PUSH_CONSTANT_SIZE  120u

/** Core internal GPU shader paths (authoritative sources under sit/gpu/). */
#define SIT_GPU_PATH_COMPOSITOR_VERT   "sit/gpu/compositor.vert"
#define SIT_GPU_PATH_VD_FRAG           "sit/gpu/vd.frag"
#define SIT_GPU_PATH_COMPOSITE_FRAG    "sit/gpu/composite.frag"
#define SIT_GPU_PATH_QUAD_VERT         "sit/gpu/quad.vert"
#define SIT_GPU_PATH_QUAD_FRAG         "sit/gpu/quad.frag"
#define SIT_GPU_PATH_YPQ_GRADE_FRAG    "sit/gpu/ypq_grade.frag"
#define SIT_GPU_PATH_TEXT_VERT         "sit/gpu/text.vert"
#define SIT_GPU_PATH_TEXT_FRAG         "sit/gpu/text.frag"

// Canonical GLSL for internal renderer pipelines: sit/gpu/ (see SIT_GPU_PATH_* and doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md).


#endif // SITUATION_IMPL_DECL_H