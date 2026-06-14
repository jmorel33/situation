/***************************************************************************************************
*
*   situation_impl_renderer_fwd.h - Forward Declarations for Renderer Internal Functions
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Forward declarations for static functions defined in situation_impl_renderer.h.
*   Preprocessor guards mirror the renderer so GL/Vulkan types appear only when active.
*
*   Public API: sit/situation_api.h (SITAPI). Other implementation statics:
*   situation_impl_forward.h (lifecycle, I/O, audio, render thread) and their .h units
*   (e.g. situation_impl_wdm.h, situation_impl_vd.h, situation_impl_audio.h).
*
*   Do not include this file directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_FWD_H
#define SITUATION_IMPL_RENDERER_FWD_H

//----------------------------------------------------------------------------------
// Shared / Unconditional
//----------------------------------------------------------------------------------

// Uniform map
static _SituationUniformMap* _sit_uniform_map_create(void);
/* HARDENING: void by design — uniform map teardown; idempotent free. */
static void _sit_uniform_map_destroy(_SituationUniformMap* map);
static SituationError _sit_uniform_map_resize(_SituationUniformMap* map);
static SituationError _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value);
static int32_t _sit_uniform_map_get(_SituationUniformMap* map, const char* key);

// Renderer lifecycle
static SituationError _SituationValidateRenderCaps(void);
static SituationError _SituationInitRenderer(const SituationInitInfo* init_info);
static SituationError _SituationInitRenderThread(const SituationInitInfo* info);
static SituationError _SituationDestroyRenderThread(void);
static SituationError _SituationInitDefaultFont(void);
static SituationError _SituationInitTextRenderer(void);
static SituationError _SituationInitQuadRenderer(int width, int height);
static SituationError _SituationInitYpqGradeRenderer(int width, int height);
/* HARDENING: void by design — best-effort quad renderer teardown. */
static void _SituationCleanupQuadRenderer(void);
/* HARDENING: void by design — best-effort shutdown leak sweep. */
static void _SituationCleanupDanglingResources(void);
/* HARDENING: void by design — default textures/samplers teardown. */
static void _SituationCleanupInternalDefaultResources(void);
static SituationError _SituationPerformHotReloadPass(void);

// Render thread worker
/* HARDENING: int by design — thread entry ABI (returns 0 on clean exit). */
static int _SituationRenderThreadEntry(void* arg);
/* HARDENING: void by design — per-frame job dispatch worker. */
static void _SituationRenderJobWorker(void* data, void* unused);

// Frame resource management
static SituationError _SitFlushFrameResources(int frame_index);

// Core internal GPU shaders (sit/gpu/) — shared loader; GL program helper is backend-specific below
static SituationError _SituationLoadCoreShaderFile(const char* relative_path, char** out_src);

// Render lists
static SituationError _SituationReplayToQueue(SituationRenderList list, int frame_idx);
static SituationError _SituationEnqueueRenderList(SituationRenderList list);

// Indirect draw (shared GL/VK record path)
static SituationError _SituationCmdDrawIndirectRecord(SituationCommandBuffer cmd,
                                                      SituationBuffer indirect_buffer,
                                                      size_t offset,
                                                      size_t command_size,
                                                      bool indexed_draw);

// Resource slot access
static _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle);
static _SituationShaderSlot* _SitGetShaderSlot(SituationShader handle);
static _SituationMeshSlot* _SitGetMeshSlot(SituationMesh handle);
static _SituationBufferSlot* _SitGetBufferSlot(SituationBuffer handle);
static _SituationComputePipelineSlot* _SitGetComputePipelineSlot(SituationComputePipeline handle);
static _SituationModelSlot* _SitGetModelSlot(SituationModel handle);

// Resource slot allocation
static _SituationShaderSlot* _SitAllocShaderSlot(SituationShader* out_handle);
/* HARDENING: void by design — idempotent slot release; invalid handles ignored. */
static void _SitFreeShaderSlot(SituationShader handle);
static _SituationComputePipelineSlot* _SitAllocComputePipelineSlot(SituationComputePipeline* out_handle);
/* HARDENING: void by design — idempotent slot release; invalid handles ignored. */
static void _SitFreeComputePipelineSlot(SituationComputePipeline handle);
static _SituationMeshSlot* _SitAllocMeshSlot(SituationMesh* out_handle);
/* HARDENING: void by design — idempotent slot release; invalid handles ignored. */
static void _SitFreeMeshSlot(SituationMesh handle);
static _SituationBufferSlot* _SitAllocBufferSlot(SituationBuffer* out_handle);
/* HARDENING: void by design — idempotent slot release; invalid handles ignored. */
static void _SitFreeBufferSlot(SituationBuffer handle);
static _SituationModelSlot* _SitAllocModelSlot(SituationModel* out_handle);
/* HARDENING: void by design — idempotent slot release; invalid handles ignored. */
static void _SitFreeModelSlot(SituationModel handle);

// Shared validation helpers
static SituationError _SituationValidateIndexBufferBind(SituationBuffer buffer, size_t offset,
                                                        SituationIndexType index_type,
                                                        _SituationBufferSlot** out_slot);
static SituationError _SituationValidateIndirectDrawBuffer(SituationBuffer indirect_buffer,
                                                           size_t offset, size_t command_size);
static SituationError _SituationValidateViewportScissorIndex(uint32_t index, const char* caller);
/* HARDENING: bool by design — pure bounds query; no side effects. */
static bool _SituationBufferRegionFits(size_t buffer_size, size_t offset, int width, int height,
                                       size_t row_pitch_bytes);
/* HARDENING: bool by design — pure layout category query; no side effects. */
static bool _SituationTextureLayoutIsAttachmentOrPresent(SituationTextureLayout layout);
/* HARDENING: bool by design — pure bounds query; no side effects. */
static bool _SituationTextureRectInBounds(SituationTextureRect rect, int width, int height);
/* HARDENING: int by design — pure query; clamps to >= 1. */
static int _SituationTextureMipExtent(int base_extent, uint32_t mip_level);
/* HARDENING: size_t by design — pure row-pitch helper; no side effects. */
static size_t _SituationTextureBufferRowPitchBytes(size_t row_pitch, int width);
/* HARDENING: int by design — pure capability query; reads GL/VK state. */
static int _SituationGetMaxViewports(void);
/* HARDENING: size_t by design — pure element-size query. */
static size_t _SitIndexTypeElementSize(SituationIndexType index_type);

//----------------------------------------------------------------------------------
// OpenGL Backend
//----------------------------------------------------------------------------------
#if defined(SITUATION_USE_OPENGL)

// GL state save/restore
/* HARDENING: void by design — GL state backup for scoped restore. */
static void _SitGLBackupState(_SitGLStateBackup* s);
/* HARDENING: void by design — GL state restore after scoped backup. */
static void _SitGLRestoreState(_SitGLStateBackup* s);
/* HARDENING: void by design — invalidate GL shadow state cache. */
static void _SitGLInvalidateShadowState(void);

// GL raster state stack
/* HARDENING: void by design — captures current GL raster state into entry. */
static void _SitGLCaptureRasterState(_SitGLRasterStackEntry* e);
/* HARDENING: void by design — applies raster state entry to GL. */
static void _SitGLApplyRasterState(const _SitGLRasterStackEntry* e);
/* HARDENING: void by design — applies GL baseline raster defaults. */
static void _SituationGLApplyBaselineRasterState(void);
/* HARDENING: void by design — resets per-frame raster tracking. */
static void _SituationResetTrackedRasterStateForNewFrame(void);

// GL stencil helpers
/* HARDENING: void by design — applies stencil state for one face. */
static void _SitGLApplyStencilFace(GLenum face, const SituationStencilState* state);
/* HARDENING: bool by design — queries whether the current framebuffer has a stencil attachment. */
static bool _SitGLHasStencilBuffer(void);

// GL format / type mapping
static GLenum _SituationMapDataTypeToGL(SituationDataType type);
static GLenum _SitGLIndexType(SituationIndexType index_type);
static GLenum _SitGLMapDepthCompare(SituationDepthCompareOp op);
static GLenum _SitGLMapStencilOp(SituationStencilOp op);

// GL VAO cache
static GLuint _SitGLGetCachedVAO(SituationMesh mesh);

// GL deferred destroy (graveyard)
/* HARDENING: void by design — enqueue GL buffer for deferred destroy. */
static void _SitGLDeferDestroyBuffer(GLuint id);
/* HARDENING: void by design — enqueue GL texture for deferred destroy. */
static void _SitGLDeferDestroyTexture(GLuint id);
/* HARDENING: void by design — enqueue mesh VAO cache clean. */
static void _SitGLDeferCleanMeshVAO(uint64_t mesh_id);
/* HARDENING: void by design — GL deferred destroy flush after fence wait. */
static void _SitGLFlushGraveyard(int frame_index);

// GL error / debug
/* HARDENING: void by design — debug GL error probe; logs via error channel. */
static void _SituationCheckGLError(const char* location);

// GL soft command buffer
static SituationError _SitGLSoftCmdPush(SituationGLSoftCommandBuffer* buf, SitOpCode opcode, SitCommandPacket** out_packet);
static SituationError _SitGLSoftDataPush(SituationGLSoftCommandBuffer* buf, const void* data, size_t size, void** out_ptr);
static SituationError _SituationGLValidateInternalQuadDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass);
static SituationError _SituationGLValidateInternalTextDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass);
static SituationError _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index);
static SituationError _SitGLDeferProgramUniform(SituationGLSoftCommandBuffer* buf, GLuint prog, GLint loc, SituationUniformType type, int elem_count, const void* data, size_t payload_bytes);

// GL misc helpers
/* HARDENING: void by design — presentation alpha fixup (best-effort). */
static void _SitGLEnsureDefaultFramebufferOpaqueAlpha(void);
static GLenum _SitGLUploadNamedBuffer(GLuint buffer_id, GLsizeiptr size, const void* initial_data);
/* HARDENING: void by design — flips screenshot pixel rows to top-left origin. */
static void _SituationGLFlipScreenshotRowsTopLeft(uint8_t* rgba, int width, int height);

// GL ring buffer / fences
static SituationError _SituationInitGLMDIBuffer(void);
static SituationError _SituationInitGLRingBuffer(void);
static SituationError _SituationInitGLRingFences(void);
/* HARDENING: void by design — GL ring fence wait (blocking sync point). */
static void _SituationGLRingWait(void);

// GL context handoff
/* HARDENING: void by design — GL context handoff to host thread. */
static void _SituationMakeGLContextCurrentForHostThread(void);
/* HARDENING: void by design — GL context handoff to render thread. */
static void _SituationReleaseHostGLContextForRenderThread(void);

// GL shader compilation helpers
static char* _SituationDupGLInfoLog(GLuint name, int is_program);
/* HARDENING: void by design — GL UBO block bind best-effort. */
static void _SituationBindGLProgramUniformBlocks(GLuint program);
/* HARDENING: void by design — GL SSBO block bind best-effort. */
static void _SituationBindGLProgramStorageBlocks(GLuint program);
static GLuint _SituationGLFindProgramResourceIndex(GLuint program, GLenum resource_interface, const char* name);
static GLuint _SituationGLFindBlockIndexByBinding(GLuint program, GLenum resource_interface, uint32_t binding_point);
static SituationError _SituationPopulateGLShaderUniformMap(GLuint program, _SituationUniformMap* map);
static SituationError _SituationSetShaderUniformLocationImpl(_SituationShaderSlot* slot, GLint location, const void* data, SituationUniformType type);
static int _SituationFinalizeGLPendingProgramLink(_SituationShaderSlot* slot);
static SituationError _SituationValidateSpirvBinary(const void* data, size_t size, const char* label);
static SituationError _SituationSetGLErrorFromSpirvStage(SituationError code, const char* stage, size_t blob_bytes, const char* driver_log);
static int _SituationGLSpecializeSpirvShader(GLuint shader, GLenum type, size_t blob_bytes, SituationError fail_code, SituationError* out_err);
static SituationError _SituationBeginGLSpirvShaderLoadAsync(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader);
static SituationError _SituationPollGLAsyncSpirvShaderLoad(_SituationShaderSlot* slot);
/* HARDENING: void by design — async SPIR-V copy buffer free. */
static void _SituationGLFreeSpirvAsyncCopies(_SituationShaderSlot* slot);
static size_t _sit_uniform_scalar_payload_bytes(SituationUniformType type);
static GLuint _SituationCompileGLShaderEx(const char* source, GLenum type, SituationError* error_code, bool wait_for_compile);
static GLuint _SituationCompileGLShader(const char* source, GLenum type, SituationError* error_code);
static int _SituationPollGLShaderCompile(GLuint shader, GLenum type, SituationError* error_code);
static SituationError _SituationGLAsyncLoadFail(_SituationShaderSlot* slot);
static SituationError _SituationPollGLAsyncShaderLoad(_SituationShaderSlot* slot);
static SituationError _SituationPollGLPendingProgramLink(_SituationShaderSlot* slot);
static GLuint _SituationCreateGLShaderProgramAsync(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgram(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLCoreShaderProgram(const char* vs_path, const char* fs_path, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramFromSource(const char* cs_src, SituationError* error_code);
static GLuint _SituationCreateGLComputeProgram(const void* source_data, SituationGLShaderSourceType source_type, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramFromSpirv(const SituationSpirvBinary* vs_blob, const SituationSpirvBinary* fs_blob, SituationError* error_code);

// GL virtual display renderer init
static SituationError _SituationInitGLVirtualDisplayRenderer(void);

// GL canvas resources
static SituationError _SituationGLEnsureCanvasResources(void);
/* HARDENING: void by design — best-effort canvas resource teardown. */
static void _SituationGLDestroyCanvasResources(void);
/* HARDENING: void by design — blits canvas offscreen buffer to the default framebuffer. */
static void _SituationGLBlitCanvasToDisplay(void);

// GL virtual bindless texture table
/* HARDENING: void by design — in-memory virtual texture slot table reset only. */
static void _SituationVirtualBindlessInit(void);
static int _SituationVirtualBindlessBind(GLuint gl_texture_id);

// GL GLSL source injection helpers
static char* _SituationInjectGLSLDefinesAfterVersion(const char* src, const char* defines_block);
/* HARDENING: const char* by design — pointer into src; no allocation. */
static const char* _SituationGLSLVirtualBindlessInjectionPoint(const char* source);

// GL init / cleanup
static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info);
/* HARDENING: void by design — best-effort OpenGL teardown. */
static void _SituationCleanupOpenGL(void);

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_OPENGL

#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SituationVkAsyncShaderLoad _SituationVkAsyncShaderLoad;
typedef enum {
    SIT_VK_ASYNC_PROGRESS_IN_PROGRESS,
    SIT_VK_ASYNC_PROGRESS_SPIRV_READY,
    SIT_VK_ASYNC_PROGRESS_FAILED,
    SIT_VK_ASYNC_PROGRESS_LOST,
    SIT_VK_ASYNC_PROGRESS_TIMEOUT,
    SIT_VK_ASYNC_PROGRESS_ABANDONED
} _SitVkAsyncCompileProgressResult;
#define SIT_VK_ASYNC_PROGRESS_POLL     1u
#define SIT_VK_ASYNC_PROGRESS_UNLOAD   2u
#define SIT_VK_ASYNC_PROGRESS_SHUTDOWN 4u
static SituationError _SituationVulkanBuildGraphicsPipelinesOnSlot(_SituationShaderSlot* slot, const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile);
static SituationError _SituationVulkanBuildMeshPipelinesOnSlot(_SituationShaderSlot* slot, const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len);
/* HARDENING: void by design — single free path for async compile ctx (src, spirv blobs, ctx). */
static void _SituationVkAsyncCompileFreeCtx(_SituationVkAsyncShaderLoad* ctx);
static _SitVkAsyncCompileProgressResult _SituationVkAsyncCompileProgress(
    _SituationVkAsyncShaderLoad* ctx, _SituationShaderSlot* slot, uint32_t mode_flags, uint64_t elapsed_ns);
/* HARDENING: bool by design — returns true if abandon CAS succeeded; false if already terminal or busy. */
static bool _SituationVkAsyncCompileAbandon(_SituationVkAsyncShaderLoad* ctx, _SituationShaderSlot* slot, bool detach_slot);
/* HARDENING: void by design — async teardown; poll path sets terminal error before free. */
static void _SituationVulkanFreeAsyncShaderLoad(_SituationShaderSlot* slot);
static SituationError _SituationPollVkAsyncShaderLoad(_SituationShaderSlot* slot);
/* HARDENING: void by design — shader compile worker ABI. */
static void _SituationVkAsyncCompileWorker(void* payload, void* unused);
/* HARDENING: void by design — cooperative async compile progress from poll/unload (non-blocking). */
static void _SituationVkAsyncShaderCompilePump(_SituationVkAsyncShaderLoad* ctx);
#endif // VULKAN + SHADER_COMPILER

//----------------------------------------------------------------------------------
// Vulkan Backend
//----------------------------------------------------------------------------------
#if defined(SITUATION_USE_VULKAN)
static VKAPI_ATTR VkBool32 VKAPI_CALL _SituationVulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

// Staging & Graveyard
static SituationError _SituationInitStagingBuffers(void);
/* HARDENING: void by design — best-effort staging buffer teardown. */
static void _SituationCleanupStagingBuffers(void);
/* HARDENING: void by design — graveyard slot init (no failure paths). */
static void _SituationInitGraveyard(_SituationVKGraveyard* gy);
/* HARDENING: void by design — graveyard slot teardown. */
static void _SituationCleanupGraveyard(_SituationVKGraveyard* gy);
/* HARDENING: void by design — deferred destroy flush after fence wait. */
static void _SituationFlushGraveyard(uint32_t frame_index);

// Deferred Destroy
/* HARDENING: void by design — enqueue buffer for deferred destroy. */
static void _SituationDeferDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
/* HARDENING: void by design — enqueue image for deferred destroy. */
static void _SituationDeferDestroyImage(VkImage image, VmaAllocation allocation, VkImageView view, VkSampler sampler);
/* HARDENING: void by design — enqueue descriptor set for deferred destroy. */
static void _SituationDeferDestroyDescriptorSet(VkDescriptorSet set, VkDescriptorPool pool);
/* HARDENING: void by design — enqueue pipeline for deferred destroy. */
static void _SituationDeferDestroyPipeline(VkPipeline pipeline, VkPipelineLayout layout);
/* HARDENING: void by design — enqueue framebuffer for deferred destroy. */
static void _SituationDeferDestroyFramebuffer(VkFramebuffer framebuffer);
/* HARDENING: void by design — enqueue render pass for deferred destroy. */
static void _SituationDeferDestroyRenderPass(VkRenderPass render_pass);

// Instance & Device
static const char** _SituationVulkanGetRequiredExtensions(uint32_t* out_extension_count, bool enable_validation);
static SituationError _SituationVulkanCreateInstance(const SituationInitInfo* init_info);
static SituationError _SituationVulkanSetupDebugMessenger(const SituationInitInfo* init_info);
static SituationError _SituationVulkanCreateSurface(void);
static SituationError _SituationVulkanPickPhysicalDevice(void);
static SituationError _SituationVulkanCreateLogicalDevice(const SituationInitInfo* init_info);
static SituationError _SituationVulkanCreateAllocator(void);
/* HARDENING: int by design — returns 1 if suitable, 0 if not. */
static int _SituationIsDeviceSuitable(VkPhysicalDevice device);
static _SituationQueueFamilyIndices _SituationVulkanFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

// Swapchain
/* HARDENING: void by design — swapchain capability query (fills out struct). */
static void _SituationVulkanQuerySwapchainSupport(VkPhysicalDevice device, _SituationVulkanSwapchainSupportDetails* out_details);
/* HARDENING: void by design — free swapchain query scratch. */
static void _SituationVulkanFreeSwapchainSupportDetails(_SituationVulkanSwapchainSupportDetails* details);
static SituationError _SituationVulkanCreateSwapchain(void);
static SituationError _SituationVulkanCreateImageViews(void);
static SituationError _SituationVulkanCreateDepthResources(void);
static SituationError _SituationVulkanCreateFramebuffers(void);
static SituationError _SituationVulkanCleanupSwapchain(void);
static SituationError _SituationVulkanRecreateSwapchain(void);
static SituationError _SituationVulkanRecreateSurfaceKHR(void);
static VkExtent2D _SituationVulkanPickSwapchainExtent(const VkSurfaceCapabilitiesKHR* caps, int fb_w, int fb_h);
static SituationError _SituationVulkanEnsureSwapchainMatchesFramebuffer(void);
/* HARDENING: void by design — syncs cached window dimensions from GLFW into Vulkan state. */
static void _SituationVulkanSyncMainWindowFromGLFW(void);

// Vulkan 2D ortho + draw-path hygiene (BeginRenderPass, VD compositor, quads, text, user draws)
/* HARDENING: float by design — active render-area height query; clamps to >= 1.0f; no failure paths. */
static SituationError _SitVulkanFillOrthoProjection2D(float width, float height, mat4 out_proj);
static void _SitVulkanFillViewport2DOpenGLParity(float width, float height, VkViewport* out_vp);
static void _SitVulkanApply2DViewportScissor(VkCommandBuffer vk_cmd);
/* HARDENING: VkPipeline by design — internal variant resolver; VK_NULL_HANDLE means no pipeline on slot. */
static VkPipeline _SitVulkanBasePipelineForStride(_SituationShaderSlot* shader_slot, size_t stride);
static VkPipeline _SitVulkanSelectRasterVariant(_SituationShaderSlot* shader_slot, VkPipeline base_pipeline);
static VkPipeline _SitVulkanSelectPolygonVariant(_SituationShaderSlot* shader_slot, VkPipeline base_pipeline);
static VkPipeline _SitVulkanResolveGraphicsPipeline(_SituationShaderSlot* shader_slot, size_t stride);
/* HARDENING: enum by design — lazy-init tracked topology state. */
static VkPrimitiveTopology _SitVulkanGetCurrentPrimitiveTopology(void);
/* HARDENING: bool by design — capability query for extended dynamic state PFNs. */
static bool _SitVulkanGraphicsDynamicProcsReady(void);
/* HARDENING: void by design — record-only; skips when cmd/PFNs unavailable (9.8). */
static void _SitVulkanCmdSetDepthDynamics(VkCommandBuffer vk_cmd, VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op);
static void _SitVulkanApplyQuadDrawDynamicState(VkCommandBuffer vk_cmd);
static void _SitVulkanApplyVDCompositingDynamicState(VkCommandBuffer vk_cmd, float vp_w, float vp_h);
static void _SitVulkanApplyGraphicsViewportScissor(VkCommandBuffer vk_cmd);
static void _SitVulkanApplyTrackedRasterDynamics(VkCommandBuffer vk_cmd);
/* HARDENING: void by design — applies extended dynamic raster state (polygon mode, depth bias, stencil). */
static void _SitVulkanApplyTrackedExtendedRasterDynamics(VkCommandBuffer vk_cmd);
static SituationError _SitVulkanEnsureGraphicsPipelineBound(VkCommandBuffer vk_cmd, _SituationShaderSlot* shader_slot, size_t stride);
static SituationError _SitVulkanValidateInternalQuadDrawReady(VkCommandBuffer vk_cmd, const char* caller);
static SituationError _SitVulkanValidateInternalTextDrawReady(VkCommandBuffer vk_cmd, const char* caller);
/* HARDENING: void by design — fills VkDynamicState array from caps; no alloc (pipeline create). */
static void _SitVulkanFillGraphicsDynamicStates(VkDynamicState* states, uint32_t* out_count);

// Vulkan raster state stack
/* HARDENING: void by design — captures current Vulkan dynamic raster state into entry. */
static void _SitVulkanCaptureRasterState(_SitVulkanRasterStackEntry* entry);
/* HARDENING: void by design — applies raster state entry as Vulkan dynamic state. */
static void _SitVulkanApplyRasterState(VkCommandBuffer vk_cmd, const _SitVulkanRasterStackEntry* entry);

// Vulkan stencil helpers
/* HARDENING: void by design — applies stencil dynamic state for one face. */
static void _SitVulkanApplyStencilFaceDynamics(VkCommandBuffer vk_cmd, VkStencilFaceFlags face,
                                               const SituationStencilState* state);
/* HARDENING: bool by design — queries whether the active renderpass has a stencil attachment. */
static bool _SitVulkanHasStencilAttachment(void);

// Vulkan format / type mapping
static VkIndexType _SitVkIndexType(SituationIndexType index_type);
static VkCompareOp _SitVulkanMapCompareOp(SituationDepthCompareOp op);
static VkStencilOp _SitVulkanMapStencilOp(SituationStencilOp op);
static VkAccessFlags _SituationVulkanMapAccessFlags(uint32_t access);
static VkPipelineStageFlags _SituationVulkanMapPipelineStages(uint32_t stages);
static VkImageLayout _SituationVulkanMapTextureLayout(SituationTextureLayout layout);
/* HARDENING: void by design — fills stage+access masks from layout for pipeline barriers. */
static void _SituationVulkanTextureLayoutBarrierMasks(SituationTextureLayout layout, bool is_source,
                                                      VkPipelineStageFlags* stage_mask,
                                                      VkAccessFlags* access_mask);

// Render Pass & Pipeline
static SituationError _SituationVulkanCreateRenderPass(void);
static VkRenderPass _SituationVulkanGetOrCreateRenderPass(_SituationVulkanState* vk_state, const SituationRenderPassInfo* info);
static VkFormat _SituationVulkanFindSupportedFormat(const VkFormat* candidates, uint32_t candidate_count, VkImageTiling tiling, VkFormatFeatureFlags features);
static VkPipeline _SituationVulkanCreateGraphicsPipeline(const void* vs_code, size_t vs_size, const void* fs_code, size_t fs_size, VkPipelineLayout layout, VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings, uint32_t attr_count, const VkVertexInputAttributeDescription* attrs, uint32_t pipeline_flags, VkCullModeFlags cull_mode, VkFrontFace front_face, VkPolygonMode polygon_mode);

// Command Pool & Buffers
static SituationError _SituationVulkanCreateCommandPool(void);
static SituationError _SituationVulkanCreateCommandBuffers(void);
static SituationError _SituationVulkanCreateSyncObjects(void);
static VkCommandBuffer _SituationVulkanBeginSingleTimeCommands(void);
static SituationError _SituationVulkanEndSingleTimeCommands(VkCommandBuffer command_buffer);

// Image & Buffer Operations
static SituationError _SituationVulkanCreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkImage* out_image, VmaAllocation* out_allocation);
static VkImageView _SituationVulkanCreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
/* HARDENING: void by design — record-only image layout barrier. */
static void _SituationVulkanTransitionImageLayout(VkCommandBuffer cmd, VkImage image, uint32_t mip_levels, VkImageLayout old_layout, VkImageLayout new_layout);
/* HARDENING: void by design — record-only buffer-to-image copy. */
static void _SituationVulkanCopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
static void* _SituationVulkanBlitImageToHostVisibleBuffer(VkImage srcImage, VkImageLayout srcImageLayout, uint32_t width, uint32_t height);
static void _SituationVulkanEnsureScreenshotResolvedForFrame(uint32_t frame_index);
/* HARDENING: void by design — record-only mipmap generation. */
static void _SituationVulkanGenerateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t width, int32_t height, uint32_t mip_levels);
static SituationError _SituationVulkanCreateAndUploadBuffer(VkCommandBuffer cmd, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* out_buffer, VmaAllocation* out_allocation);
static SituationError _SituationVulkanReadBackBuffer(VkBuffer src_buffer, VmaAllocation src_alloc, size_t size, size_t offset, void* out_data);
/* HARDENING: void by design — immediate or deferred image destroy helper. */
static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation);
/* HARDENING: void by design — immediate or deferred buffer destroy helper. */
static void _SituationVulkanDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
/* HARDENING: bool by design — shutdown-state query for immediate vs deferred destroy. */
static bool _SituationVulkanImmediateDestroyDuringShutdown(void);

// Canvas resources
static SituationError _SituationVulkanEnsureCanvasResources(void);
/* HARDENING: void by design — best-effort Vulkan canvas resource teardown. */
static void _SituationVulkanDestroyCanvasResources(void);
/* HARDENING: void by design — records canvas-to-swapchain blit commands. */
static void _SituationVulkanRecordCanvasStretchBlit(VkCommandBuffer cmd);

// Screenshot
static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height);
/* HARDENING: void by design — screenshot resource teardown. */
static void _SituationVulkanDestroyScreenshotResources(void);
/* HARDENING: void by design — record-only screenshot blit. */
static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height);
/* HARDENING: void by design — post-submit screenshot resolve hook. */
static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index);

// Shader Modules
static VkShaderModule _SituationCreateVulkanShaderModule(const char* code, size_t code_size);
static VkShaderModule _SituationVulkanCreateShaderModule(const void* code, size_t code_size);
static VkShaderModule _SituationVulkanCreateShaderModuleEx(const void* code, size_t code_size, const char* stage_label, SituationError fail_code);
static SituationError _SituationVulkanLoadShaderFromSpirvMemoryWithProfile(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader);
/* HARDENING: bool by design — descriptor resolver; false paths set *out_err. */
static bool _SituationVulkanResolveBufferDescriptor(uint32_t set_index, SituationBufferUsageFlags usage, VkDescriptorSetLayout* out_layout, VkDescriptorType* out_type, bool* out_use_dynamic_offset, SituationError* out_err);
static SituationError _SituationVulkanEnsureBufferDescriptorSet(_SituationBufferSlot* slot, VkDescriptorSetLayout layout, VkDescriptorType descriptor_type);
/* HARDENING: void by design — descriptor set cache release. */
static void _SituationVulkanFreeBufferDescriptorSet(_SituationBufferSlot* slot);
/* HARDENING: void by design — CPU color format swizzle helper. */
static void _SituationVulkanCopyMappedColorToRGBA(uint8_t* dst, const void* mapped, size_t nbytes, VkFormat fmt);
static SituationShader _SituationCreateVulkanPipeline(const char* vs_path, const char* fs_path);
static VkDescriptorSet _SituationVulkanAllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool* out_pool);

// Screen Copy
static SituationError _SituationVulkanCreateScreenCopyResource(void);
/* HARDENING: void by design — screen copy resource teardown. */
static void _SituationVulkanDestroyScreenCopyResource(void);

// Submit
static SituationError _SituationSubmitCompute(VkCommandBuffer cmd);
static VkResult _SituationSubmitGraphics(VkCommandBuffer cmd);

// Init & Cleanup
static SituationError _SituationInitVulkan(const SituationInitInfo* init_info);
static SituationError _SituationVulkanInitInternalRenderers(void);
static SituationError _SituationVulkanInitComputeLayouts(void);
static SituationError _SituationVulkanInitGraphicsSpirvLayouts(void);
/* HARDENING: void by design — best-effort Vulkan teardown. */
static void _SituationCleanupVulkan(void);

// SPIR-V
static char* _SituationReadSpirvFile(const char* filename, size_t* out_size);
/* HARDENING: void by design — SPIR-V blob free helper. */
static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob);

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
typedef struct _SituationShadercMacro {
    const char* name;
    const char* value;
} _SituationShadercMacro;
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRVWithMacros(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind, const _SituationShadercMacro* macros, size_t macro_count);
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind);
static _SituationSpirvBlob _SituationVulkanCompileCoreShaderFile(const char* relative_path, const char* source_name, shaderc_shader_kind shader_kind, const _SituationShadercMacro* extra_macros, size_t extra_macro_count);
static shaderc_include_result* _SituationShaderIncluderResolve(void* user_data, const char* requested_source, int type, const char* requesting_source, size_t include_depth);
/* HARDENING: void by design — shaderc includer release callback. */
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_VULKAN

//----------------------------------------------------------------------------------
// OBJ / STL Model Loading (tinyobj / internal parsers)
//----------------------------------------------------------------------------------
// NOTE: OBJReaderContext, OBJTextureCache, VertexMap and tinyobj_vertex_index_t are
// typedef'd locally inside situation_impl_renderer.h after tinyobj is included.
// The functions below are only called from within that file — no external forward
// declaration is required or possible at this inclusion point.
//
// If a cross-file caller ever needs these functions, the typedefs must first be
// promoted to situation_impl_decl.h. Until then, this section is intentionally empty.

//----------------------------------------------------------------------------------
// GLTF Model Loading
//----------------------------------------------------------------------------------
#if defined(CGLTF_IMPLEMENTATION)
static SituationError _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count);
#endif // CGLTF_IMPLEMENTATION

#endif // SITUATION_IMPL_RENDERER_FWD_H
