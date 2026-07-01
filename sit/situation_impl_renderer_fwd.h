/***************************************************************************************************
*
*   situation_impl_renderer_fwd.h - Forward Declarations for Renderer Internal Functions
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
 *   Forward declarations for static functions defined in renderer slice headers
 *   (situation_impl_renderer_{core,lc,shader,resources,frame_cmd}.h).
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
// --- renderer_core --- (situation_impl_renderer_core.h)
// Shared / Unconditional
//----------------------------------------------------------------------------------

// Uniform map
static _SituationUniformMap* _sit_uniform_map_create(void);
/* HARDENING: void by design — uniform map teardown; idempotent free. */
static void _sit_uniform_map_destroy(_SituationUniformMap* map);
static SituationError _sit_uniform_map_resize(_SituationUniformMap* map);
static SituationError _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value);
static int32_t _sit_uniform_map_get(_SituationUniformMap* map, const char* key);

// Frame lifecycle / backpressure (bodies in renderer_core.h — fwd decls for wdm.h)
static inline bool _SitShouldEngageBackpressure(void);
static inline bool _SitShouldEngageRenderQueueBackpressure(void);
static void _SituationRecomputePacedFramesInFlight(void);
static inline int _SituationEffectiveQueueDepthLimit(void);

#if defined(SITUATION_ENABLE_RENDER_THREAD)
static int _SituationRoundDisplayFps(int raw_fps);
static void _SituationPublishPresentTimingFromRenderThread(void);
#endif
static void _SituationApplyPresentTimingDirect(void);
static void _SituationConsumePresentTimingOnMain(void);
static void _SituationUpdateFpsCounter(void);

//----------------------------------------------------------------------------------
// --- renderer_lc --- (situation_impl_renderer_lc.h)
//----------------------------------------------------------------------------------

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
static SituationError _SituationPerformHotReloadPass(void);

// Render thread worker
/* HARDENING: int by design — thread entry ABI (returns 0 on clean exit). */
static int _SituationRenderThreadEntry(void* arg);

// Frame resource management
static SituationError _SitFlushFrameResources(int frame_index);
static void _SituationFlushRenderThread(void);
#if defined(SITUATION_USE_OPENGL)
static inline int _SituationGLLastSubmittedFrameIndex(void);
#endif

//----------------------------------------------------------------------------------
// --- renderer_shader --- (situation_impl_renderer_shader.h)
//----------------------------------------------------------------------------------

// Core internal GPU shaders (sit/gpu/) — shared loader; GL program helper is backend-specific below
static SituationError _SituationLoadCoreShaderFile(const char* relative_path, char** out_src);

//----------------------------------------------------------------------------------
// --- renderer_frame_cmd --- (situation_impl_renderer_frame_cmd.h)
//----------------------------------------------------------------------------------

// Render thread job dispatch (render-list submit path)
/* HARDENING: void by design — per-frame job dispatch worker. */
static void _SituationRenderJobWorker(void* data, void* unused);

// Render lists
static SituationError _SituationReplayToQueue(SituationRenderList list, int frame_idx);
static SituationError _SituationEnqueueRenderList(SituationRenderList list);

// Indirect draw (shared GL/VK record path)
static SituationError _SituationCmdDrawIndirectRecord(SituationCommandBuffer cmd,
                                                      SituationBuffer indirect_buffer,
                                                      size_t offset,
                                                      size_t command_size,
                                                      bool indexed_draw);

// Shared validation helpers
static SituationError _SituationValidateIndexBufferBind(SituationBuffer buffer, size_t offset,
                                                        SituationIndexType index_type,
                                                        _SituationBufferSlot** out_slot);
static SituationError _SituationValidateIndirectDrawBuffer(SituationBuffer indirect_buffer,
                                                           size_t offset, size_t command_size);
static SituationError _SituationValidateViewportScissorIndex(uint32_t index, const char* caller);
/* HARDENING: bool by design — pure layout category query; no side effects. */
static bool _SituationTextureLayoutIsAttachmentOrPresent(SituationTextureLayout layout);
/* HARDENING: int by design — pure capability query; reads GL/VK state. */
static int _SituationGetMaxViewports(void);
/* HARDENING: size_t by design — pure element-size query. */
static size_t _SitIndexTypeElementSize(SituationIndexType index_type);

//----------------------------------------------------------------------------------
// --- renderer_resources --- (situation_impl_renderer_resources.h)
//----------------------------------------------------------------------------------

// Shutdown leak sweep (called from lc/ctrl; bodies live in resources slice)
/* HARDENING: void by design — best-effort shutdown leak sweep. */
static void _SituationCleanupDanglingResources(void);
/* HARDENING: void by design — default textures/samplers teardown. */
static void _SituationCleanupInternalDefaultResources(void);

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

// Texture / buffer validation helpers
/* HARDENING: bool by design — pure bounds query; no side effects. */
static bool _SituationBufferRegionFits(size_t buffer_size, size_t offset, int width, int height,
                                       size_t row_pitch_bytes);
/* HARDENING: bool by design — pure bounds query; no side effects. */
static bool _SituationTextureRectInBounds(SituationTextureRect rect, int width, int height);
/* HARDENING: int by design — pure query; clamps to >= 1. */
static int _SituationTextureMipExtent(int base_extent, uint32_t mip_level);
/* HARDENING: size_t by design — pure row-pitch helper; no side effects. */
static size_t _SituationTextureBufferRowPitchBytes(size_t row_pitch, int width);

// Mesh layout helpers
/* HARDENING: size_t by design — pure stride query from layout enum. */
static size_t _SitMeshLayoutExpectedStride(SituationMeshVertexLayout layout);
/* HARDENING: bool by design — pure stride recognition query. */
static bool _SitMeshStrideIsKnown(size_t stride);
static SituationMeshVertexLayout _SitInferMeshLayoutFromStride(size_t stride);
static SituationError _SituationCreateMeshInternal(
    const void* vertex_data, int vertex_count, size_t vertex_stride,
    const uint32_t* index_data, int index_count,
    SituationMeshVertexLayout layout, SituationMesh* out_mesh);

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
static void _SituationGLCaptureDisplayedFramebuffer(int sw, int sh, bool from_front_buffer);
static bool _SituationGLShouldCaptureFrame(int frame_index);
static void _SituationGLHandoffScreenshotRequestForSlot(int frame_index);
static void _SituationGLClearScreenshotCaptureFlags(int frame_index);
static bool _SituationGLEnsureScreenshotResources(int sw, int sh);
static void _SituationGLReadPixelsToPackBuffer(int sw, int sh, bool from_front_buffer);
static bool _SituationGLMapPackBufferToScreenshot(int sw, int sh, int frame_index);
static void _SituationGLSyncReadFramebufferToCPU(int sw, int sh, bool from_front_buffer, int frame_index);
static SituationError _SitGLDeferProgramUniform(SituationGLSoftCommandBuffer* buf, GLuint prog, GLint loc, SituationUniformType type, int elem_count, const void* data, size_t payload_bytes);

// GL misc helpers (renderer_resources: _SitGLUploadNamedBuffer)
/* HARDENING: void by design — presentation alpha fixup (best-effort). */
static void _SitGLEnsureDefaultFramebufferOpaqueAlpha(void);
static GLenum _SitGLUploadNamedBuffer(GLuint buffer_id, GLsizeiptr size, const void* initial_data);
static bool _SitGLInitReadbackNamedBuffer(GLuint buffer_id, size_t size, bool* out_persistent_coherent);
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
/* HARDENING: void by design — release host GL context when inside a frame. */
static void _SituationReleaseHostGLContextIfInFrame(void);

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
/** Exclusive FS canvas target; retries creation (Vulkan parity — no default-FBO fail-soft). */
static bool _SituationGLPrepareCanvasStretchTarget(void);
/* HARDENING: void by design — best-effort canvas resource teardown. */
static void _SituationGLDestroyCanvasResources(void);
/* HARDENING: void by design — blits canvas offscreen buffer to the default framebuffer. */
static void _SituationGLBlitCanvasToDisplay(void);

// GL virtual bindless texture table
/* HARDENING: void by design — in-memory virtual texture slot table reset only. */
static void _SituationVirtualBindlessInit(void);
static int _SituationVirtualBindlessBind(GLuint gl_texture_id);
static _SituationTextureSlot* _SituationGLFindTextureSlotByGlId(GLuint gl_tex_id);
static SituationError _SituationGLPrepareStorageTextureForSampling(_SituationTextureSlot* slot);

// GL GLSL source injection helpers
static char* _SituationInjectGLSLDefinesAfterVersion(const char* src, const char* defines_block);
/* HARDENING: const char* by design — pointer into src; no allocation. */
static const char* _SituationGLSLVirtualBindlessInjectionPoint(const char* source);

// GL init / cleanup
static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info);
static GLuint _SituationCreateGLVdCompositorShaderProgram(bool path_b, SituationError* error_code);
/* HARDENING: void by design — best-effort OpenGL teardown. */
static void _SituationCleanupOpenGL(void);

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code);
#endif // SITUATION_ENABLE_SHADER_COMPILER

// GL shader program cache
#if SIT_GL_SHADER_CACHE_ENABLE
static inline uint64_t _SitGLHashBytes64(const void* data, size_t len);
static inline uint32_t _SitGLProgramCacheBucket(uint64_t key);
static inline uint64_t _SitGLLayerAKeyFromSource(const char* vs_code, const char* fs_code);
/* HARDENING: void by design — zeroes cache state and inits mutex. */
static void _SitGLProgramCacheInit(_SitGLProgramCache* c);
/* HARDENING: void by design — destroys all cached programs and releases mutex. */
static void _SitGLProgramCacheShutdown(_SitGLProgramCache* c);
/* HARDENING: bool by design — returns true on hit, false on miss. */
static bool _SitGLProgramCacheTryHit(_SitGLProgramCache* c, uint64_t layer_a_key,
        GLuint* out_program, _SitGLProgramCacheRef* out_ref, uint32_t current_frame);
/* HARDENING: bool by design — returns true on successful insert, false on OOM. */
static bool _SitGLProgramCacheInsert(_SitGLProgramCache* c, uint64_t layer_a_key, GLuint program,
        _SitGLProgramCacheRef* out_ref, uint32_t current_frame);
/* HARDENING: void by design — decrements ref_count; marks EVICT_PENDING at zero. */
static void _SitGLProgramCacheRelease(_SitGLProgramCacheRef* ref, uint32_t current_frame);
/* HARDENING: void by design — per-frame LRU eviction pass. */
static void _SitGLProgramCacheProcessEvictions(_SitGLProgramCache* c, uint32_t current_frame);
static SituationError _SitGLLoadShaderProgramCached(_SituationShaderSlot* slot,
        const char* vs_code, const char* fs_code);
#endif // SIT_GL_SHADER_CACHE_ENABLE

// GL deferred destroy (program — complements buffer/texture above)
/* HARDENING: void by design — enqueue GL program for deferred destroy. */
static void _SitGLDeferDestroyProgram(GLuint id);

// GL output color depth
/* HARDENING: void by design — probes default framebuffer RGB bit depth after GLFW 10-bit hints. */
static void _SituationOpenGLSetOutputColorDepthFromFramebuffer(void);

// GL present / VSync
/* HARDENING: void by design — applies glfwSwapInterval immediately before SwapBuffers. */
static void _SitGLApplySwapIntervalBeforePresent(void);

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

// Shader Cache (Phase 1) — init/shutdown/acquire/release/eviction
/* HARDENING: void by design — cache init; zeroes tables and inits mutex. */
static void _SitVkShaderCacheInit(_SitVkShaderCache* c);
/* HARDENING: void by design — cache shutdown; immediate GPU destroy (device up). */
static void _SitVkShaderCacheShutdown(_SitVkShaderCache* c);
/* Returns Layer A entry (SPIR-V blob); caller must hold cache mutex. */
static _SitVkSpirvBlobEntry* _SitVkShaderCacheLookupOrInsertSpirv(_SitVkShaderCache* c, uint64_t layer_a_key, const uint8_t* vs_data, size_t vs_size, const uint8_t* fs_data, size_t fs_size);
/* Returns Layer B entry (module pair); acquires/creates modules outside mutex. */
static _SitVkModulePairEntry* _SitVkShaderCacheAcquireModules(_SitVkShaderCache* c, uint64_t vs_spirv_hash, uint64_t fs_spirv_hash, const void* vs_data, size_t vs_size, const void* fs_data, size_t fs_size);
/* Returns pipeline bundle (Layer C); creates layout + default_pipeline on miss. */
static _SitVkPipelineBundle* _SitVkShaderCacheAcquireBundle(_SitVkShaderCache* c, const _SitVkShaderCacheKey* key, _SitVkModulePairEntry* modules, uint32_t current_frame);
static inline bool _SitVkShaderCacheBundleRetainedForAcquire(const _SitVkPipelineBundle* b);
static inline _SitVkPipelineBundle* _SitVkShaderCacheRefBundleLocked(
    _SitVkShaderCache* c, _SitVkPipelineBundle* b, uint32_t current_frame);
static inline _SitVkPipelineBundle* _SitVkShaderCacheFindAndRefBundleLocked(
    _SitVkShaderCache* c, const _SitVkShaderCacheKey* key, uint64_t bucket_key, uint32_t current_frame);
static inline void _SitVkStatLegacySlotPipelineBuild(void);
static int _SitVkTryMeshLoadFromLayerA(
    _SituationShaderSlot* slot, const char* vs_code, const char* fs_code,
    uint64_t* out_layer_a_key, SituationError* out_err);
/* Decrements ref_count; marks bundle EVICT_PENDING when ref hits 0. */
static void _SitVkShaderCacheReleaseBundle(_SitVkShaderCache* c, _SitVkPipelineBundleRef* ref);
/* Per-frame LRU eviction pass — call AFTER _SituationFlushGraveyard. */
static void _SitVkShaderCacheProcessEvictions(_SitVkShaderCache* c, uint32_t current_frame);
/* Builds the single Phase 1 default simple pipeline from existing VkShaderModules. */
static VkPipeline _SitVkCreateDefaultSimplePipeline(VkPipelineLayout layout, VkShaderModule vs_module, VkShaderModule fs_module);
/* Post-compile helper: inserts Layer A/B/C and attaches bundle_ref to slot. MESH profile only. */
static void _SitVkTryAttachBundle(_SituationShaderSlot* slot, const void* vs_data, size_t vs_size, const void* fs_data, size_t fs_size, SituationSpirvLayoutProfile layout_profile);
/* CRITICAL SAFETY: the ONLY approved ATTACH path for slot->vk_bundle_ref.bundle. Also sets layout/profile.
 * Two NULL-clear cleanup sites exist (stale-ref branches in UnloadShader + ReloadShader) — those are not
 * attaches and do not violate this rule. See full comment on the definition in situation_impl_renderer.h. */
static inline void _SitVkAttachBundleRef(_SituationShaderSlot* slot, _SitVkPipelineBundle* bundle);
/* Insert compiled SPIR-V into Layer A blob cache (main thread only; idempotent). */
static void _SitVkInsertLayerA(uint64_t layer_a_key, const void* vs_data, size_t vs_size, const void* fs_data, size_t fs_size);

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
static SituationError _SituationVulkanCreateSwapchain(VkSwapchainKHR old_swapchain);
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
static void _SitVulkanGetActive2DTargetSize(float* out_w, float* out_h);
/* HARDENING: bool by design — whether active VD render pass has depth attachment. */
static bool _SitVulkanActiveVDPassHasDepth(void);
/* HARDENING: void by design — destroys per-VD-format quad pipeline variants. */
static void _SitVulkanDestroyQuadVDDynamicPipelines(void);
static SituationError _SitVulkanResolveQuadPipeline(VkPipeline* out_pipeline);
static void _SitVulkanFillQuadPushProjectionForActiveTarget(mat4 out_proj);
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
static VkPipeline _SituationVulkanCreateGraphicsPipelineEx(const void* vs_code, size_t vs_size, const void* fs_code, size_t fs_size, VkPipelineLayout layout, VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings, uint32_t attr_count, const VkVertexInputAttributeDescription* attrs, uint32_t pipeline_flags, VkCullModeFlags cull_mode, VkFrontFace front_face, VkPolygonMode polygon_mode, VkFormat dynamic_color_format, VkFormat dynamic_depth_format, VkSampleCountFlagBits rasterization_samples);
static VkPipeline _SituationVulkanCreateGraphicsPipeline(const void* vs_code, size_t vs_size, const void* fs_code, size_t fs_size, VkPipelineLayout layout, VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings, uint32_t attr_count, const VkVertexInputAttributeDescription* attrs, uint32_t pipeline_flags, VkCullModeFlags cull_mode, VkFrontFace front_face, VkPolygonMode polygon_mode);
static void _SitVkBeginVDDynamicRendering(VkCommandBuffer cmd, SituationVirtualDisplay* vd, const SituationRenderPassInfo* info);
static void _SitVkEndVDDynamicRendering(VkCommandBuffer cmd, int display_id, SituationVirtualDisplay* vd);
static void _SitVkBeginRTDynamicRendering(VkCommandBuffer cmd, _SituationRenderTargetSlot* rts, const SituationRenderPassInfo* info);
static void _SitVkEndRTDynamicRendering(VkCommandBuffer cmd, _SituationRenderTargetSlot* rts);
static void _SitVkPinLastSpirvOnSlot(_SituationShaderSlot* slot, const void* vs, size_t vs_len, const void* fs, size_t fs_len);
static VkPipeline _SitVkCreateVDDynamicPipelineFromModules(VkPipelineLayout layout, VkShaderModule vs_module, VkShaderModule fs_module, VkFormat color_fmt, VkFormat depth_fmt, VkSampleCountFlagBits rasterization_samples);
static void _SitVkDestroyVDDynamicPipelinesOnSlot(_SituationShaderSlot* slot);
static VkPipeline _SitVulkanResolveVDDynamicPipeline(_SituationShaderSlot* slot, VkPipeline rp_pipeline, const SituationVirtualDisplay* vd);

// Command Pool & Buffers
static SituationError _SituationVulkanCreateCommandPool(void);
static SituationError _SituationVulkanCreateCommandBuffers(void);
static SituationError _SituationVulkanCreateSyncObjects(void);
static VkCommandBuffer _SituationVulkanBeginSingleTimeCommands(void);
static SituationError _SituationVulkanEndSingleTimeCommands(VkCommandBuffer command_buffer);

// Image & Buffer Operations (renderer_resources: CreateAndUploadBuffer, ReadBackBuffer)
static SituationError _SituationVulkanCreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkSampleCountFlagBits samples, VkImage* out_image, VmaAllocation* out_allocation);
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
static SituationError _SitVulkanBindGlobalBindlessSet(
    VkCommandBuffer vk_cmd, VkPipelineLayout layout, uint32_t set_index, const char* caller);
static SituationError _SitVulkanWriteSlotToGlobalBindlessSet(
    _SituationTextureSlot* slot, uint32_t slot_idx, const char* caller);

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

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
/* HARDENING: void by design — SPIR-V blob free helper. */
static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob);
static _SituationSpirvBlob _SituationShadercCompileGLSLtoSPIRVWithMacros(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind, const _SituationShadercMacro* macros, size_t macro_count, shaderc_target_env target_env, uint32_t target_env_version, bool gl_auto_bindings);
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRVWithMacros(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind, const _SituationShadercMacro* macros, size_t macro_count);
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind);
static _SituationSpirvBlob _SituationVulkanCompileCoreShaderFile(const char* relative_path, const char* source_name, shaderc_shader_kind shader_kind, const _SituationShadercMacro* extra_macros, size_t extra_macro_count);
static shaderc_include_result* _SituationShaderIncluderResolve(void* user_data, const char* requested_source, int type, const char* requesting_source, size_t include_depth);
/* HARDENING: void by design — shaderc includer release callback. */
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result);
static char* _SituationShaderIncluderTryLoadPath(const char* path, char** out_resolved_path);
static char* _SituationShaderIncluderLoadFile(
    const char* requested_source, const char* requesting_source, char** out_resolved_path);
#endif // SITUATION_ENABLE_SHADER_COMPILER

// Vulkan shader cache + pipeline bundles (hash / key helpers)
static inline uint64_t _SitVkHashBytes64(const void* data, size_t len);
static inline uint64_t _SitVkShadercOptionsFingerprint(void);
static inline VkPipelineCache _SitVkPipelineCacheHandle(void);
static inline void _SitVkFillCacheKey(_SitVkShaderCacheKey* key, uint64_t vs_hash, uint64_t fs_hash, uint8_t profile);
static inline _SitVkPipelineBundle* _SitVkDerefBundle(const _SitVkPipelineBundleRef* ref);
static inline uint32_t _SitVkCacheBucket(uint64_t key);
static inline bool _SitVkCacheKeyEqual(const _SitVkShaderCacheKey* a, const _SitVkShaderCacheKey* b);
#if SIT_VK_SHADER_CACHE_PHASE2
static inline uint32_t _SitVkDynamicStateMask(void);
static inline uint32_t _SitVkCapsFingerprint(void);
#endif

// Vulkan pipeline variant selection
#if SIT_VK_SHADER_CACHE_PHASE2
static VkPipeline _SitVkCreateBundlePipelineForVariant(_SitVkPipelineBundle* bundle, int variant_id);
static VkPipeline _SitVkEnsurePipelineVariant(_SitVkPipelineBundle* bundle, int variant_id);
static int _SitVkPickVariantForDraw(size_t stride, VkCullModeFlags cull, VkFrontFace front, VkPolygonMode poly);
static _SitVkShaderBuildTicket* _SitVkAcquireBuildTicket(_SitVkShaderCache* c, uint64_t layer_a_key, bool* out_leader);
/* HARDENING: void by design — decrements waiter_count; resets ticket when last waiter leaves. */
static void _SitVkReleaseBuildTicket(_SitVkShaderCache* c, _SitVkShaderBuildTicket* ticket);
/* HARDENING: void by design — marks all READY bundles as STALE after render pass change. */
static void _SitVkShaderCacheMarkBundlesStale(_SitVkShaderCache* c);
/* HARDENING: void by design — bumps render_pass_compatibility_id and marks bundles stale. */
static void _SitVkShaderCacheOnMainRenderPassCreated(void);
/* HARDENING: bool by design — spins until ticket phase >= 2 or ticket reset. */
static bool _SitVkWaitBuildTicketLayerA(_SitVkShaderBuildTicket* ticket);
static SituationError _SitVkSyncLoadFromBuildTicketFollower(
    _SituationShaderSlot* slot,
    _SitVkShaderCache* c,
    _SitVkShaderBuildTicket* ticket,
    uint64_t layer_a_key,
    SituationShader handle,
    SituationShader* out_shader);
/* HARDENING: void by design — sets terminal phase and releases build ticket. */
static void _SitVkFinishSyncBuildTicket(_SitVkShaderCache* c, _SitVkShaderBuildTicket* ticket,
        bool leader, int final_phase);
#endif // SIT_VK_SHADER_CACHE_PHASE2

// Vulkan SPIR-V byte lookup (Layer A cache query)
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
/* HARDENING: bool by design — scans Layer A for matching SPIR-V blobs by hash. */
static bool _SitVkLookupSpirvBytesByHash(uint64_t vs_hash, uint64_t fs_hash,
    const void** out_vs, size_t* out_vs_len, const void** out_fs, size_t* out_fs_len);
#endif // SIT_VK_SHADER_CACHE_ENABLE

// Vulkan VD dynamic rendering helpers
static VkAttachmentLoadOp _SitVkMapLoadOp(SituationAttachmentLoadOp op);
static VkAttachmentStoreOp _SitVkMapStoreOp(SituationAttachmentStoreOp op);
/* HARDENING: void by design — transitions VD color image to COLOR_ATTACHMENT_OPTIMAL. */
static void _SitVkTransitionVDColorForRendering(VkCommandBuffer cmd, SituationVirtualDisplay* vd, SituationAttachmentLoadOp color_load);
static VkPipeline _SitVkCreateVDDynamicPipelineClone(
    _SituationShaderSlot* slot, VkPipeline base,
    const void* vs, size_t vs_len, const void* fs, size_t fs_len,
    VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings,
    uint32_t attr_count, const VkVertexInputAttributeDescription* attrs,
    uint32_t pipeline_flags, VkCullModeFlags cull, VkFrontFace front, VkPolygonMode poly,
    VkFormat color_fmt, VkFormat depth_fmt, VkSampleCountFlagBits rasterization_samples);

// Vulkan surface format + render pass management
/* HARDENING: bool by design — scans surface formats for 10-bit SDR support. */
static bool _SituationVulkanSurfaceSupports10BitSdr(const _SituationVulkanSwapchainSupportDetails* support);
/* HARDENING: bool by design — scans surface formats for HDR10 support. */
static bool _SituationVulkanSurfaceSupportsHdr10(const _SituationVulkanSwapchainSupportDetails* support);
static VkSurfaceFormatKHR _SituationVulkanPick8BitSurfaceFormat(const _SituationVulkanSwapchainSupportDetails* support);
static VkSurfaceFormatKHR _SituationVulkanPickSurfaceFormat(
    const _SituationVulkanSwapchainSupportDetails* support,
    SituationOutputColorDepth policy,
    bool* out_using_10bit);
/* HARDENING: void by design — destroys main window render pass + resume variant. */
static void _SituationVulkanDestroyMainWindowRenderPass(void);
static SituationError _SituationVulkanRecreateMainWindowRenderPass(void);

#endif // SITUATION_USE_VULKAN

//----------------------------------------------------------------------------------
// OBJ / STL Model Loading (tinyobj / internal parsers)
//----------------------------------------------------------------------------------
// Forward-declare struct tags for types defined in situation_impl_renderer.h.
typedef struct OBJReaderContext OBJReaderContext;
typedef struct OBJTextureCache OBJTextureCache;
typedef struct VertexMap VertexMap;

// STL loader internals
static SituationError _SitSTLParseASCII(const char* text, float** out_verts, int* out_tri_count);
static SituationError _SitSTLParseBinary(const uint8_t* data, size_t data_size, float** out_verts, int* out_tri_count);
static SituationError _SitSTLSmoothNormals(const float* flat_verts, int flat_v_count,
                                            float** out_verts, int* out_v_count,
                                            uint32_t** out_indices, int* out_i_count);

// OBJ loader internals
/* HARDENING: void by design — zeroes OBJ reader context fields. */
static void _SitOBJReaderContextInit(OBJReaderContext* ctx);
/* HARDENING: void by design — frees OBJ reader context buffers. */
static void _SitOBJReaderContextFree(OBJReaderContext* ctx);
/* HARDENING: void by design — tinyobj file-reader callback; best-effort load. */
static void _SituationOBJFileReader(void* ctx_ptr, const char* filename, int is_mtl, const char* obj_filename, char** buf, size_t* len);
/* HARDENING: void by design — zeroes OBJ texture cache fields. */
static void _SitOBJTextureCacheInit(OBJTextureCache* cache);
static SituationTexture _SitOBJTextureCacheGetOrLoad(OBJTextureCache* cache, const char* filename, const char* obj_filename);
/* HARDENING: void by design — frees OBJ texture cache entries. */
static void _SitOBJTextureCacheFree(OBJTextureCache* cache);
/* HARDENING: bool by design — computes unit face normal; returns false if degenerate. */
static bool _SitOBJTriangleNormal(const float* v0, const float* v1, const float* v2, float out_n[3]);
/* HARDENING: void by design — finalizes accumulated per-vertex normals (normalize pass). */
static void _SitOBJFinalizeMeshNormals(float* vertices, int vertex_count,
                                       const uint32_t* indices, int index_count);

// Vertex map (model loading)
/* HARDENING: void by design — allocates and initializes vertex dedup map. */
static void _SitVertexMapInit(VertexMap* map, uint32_t start_capacity);
static uint32_t _SitVertexHash(tinyobj_vertex_index_t key, uint32_t capacity);
static int _SitVertexMapGet(VertexMap* map, tinyobj_vertex_index_t key, uint32_t* out_val);
/* HARDENING: void by design — inserts or grows vertex map. */
static void _SitVertexMapPut(VertexMap* map, tinyobj_vertex_index_t key, uint32_t val);
/* HARDENING: void by design — frees vertex map entries. */
static void _SitVertexMapFree(VertexMap* map);

//----------------------------------------------------------------------------------
// GLTF Model Loading
//----------------------------------------------------------------------------------
#if defined(CGLTF_IMPLEMENTATION)
static SituationError _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count);
#endif // CGLTF_IMPLEMENTATION

#endif // SITUATION_IMPL_RENDERER_FWD_H
