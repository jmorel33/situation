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
static _SituationUniformMap* _sit_uniform_map_create(void);
/* HARDENING: void by design — uniform map teardown; idempotent free. */
static void _sit_uniform_map_destroy(_SituationUniformMap* map);
static SituationError _sit_uniform_map_resize(_SituationUniformMap* map);
static SituationError _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value);
static int32_t _sit_uniform_map_get(_SituationUniformMap* map, const char* key);

static SituationError _SituationValidateRenderCaps(void);
static SituationError _SituationInitRenderer(const SituationInitInfo* init_info);
static SituationError _SituationInitDefaultFont(void);
static SituationError _SituationInitTextRenderer(void);
static SituationError _SituationInitQuadRenderer(int width, int height);
static SituationError _SituationInitYpqGradeRenderer(int width, int height);
/* HARDENING: void by design — best-effort quad renderer teardown. */
static void _SituationCleanupQuadRenderer(void);
/* HARDENING: void by design — best-effort shutdown leak sweep. */
static void _SituationCleanupDanglingResources(void);
static SituationError _SituationPerformHotReloadPass(void);

// Render Lists
static SituationError _SituationReplayToQueue(SituationRenderList list, int frame_idx);
static SituationError _SituationEnqueueRenderList(SituationRenderList list);

// Resource Slot Access
static _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle);
static _SituationShaderSlot* _SitGetShaderSlot(SituationShader handle);
static _SituationMeshSlot* _SitGetMeshSlot(SituationMesh handle);
static _SituationBufferSlot* _SitGetBufferSlot(SituationBuffer handle);
static _SituationComputePipelineSlot* _SitGetComputePipelineSlot(SituationComputePipeline handle);
static _SituationModelSlot* _SitGetModelSlot(SituationModel handle);

// Resource Slot Allocation
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

//----------------------------------------------------------------------------------
// OpenGL Backend
//----------------------------------------------------------------------------------
#if defined(SITUATION_USE_OPENGL)
/* HARDENING: void by design — GL state backup for scoped restore. */
static void _SitGLBackupState(_SitGLStateBackup* s);
/* HARDENING: void by design — GL state restore after scoped backup. */
static void _SitGLRestoreState(_SitGLStateBackup* s);
/* HARDENING: void by design — invalidate GL shadow state cache. */
static void _SitGLInvalidateShadowState(void);
static GLuint _SitGLGetCachedVAO(SituationMesh mesh);
/* HARDENING: void by design — enqueue GL buffer for deferred destroy. */
static void _SitGLDeferDestroyBuffer(GLuint id);
/* HARDENING: void by design — enqueue GL texture for deferred destroy. */
static void _SitGLDeferDestroyTexture(GLuint id);
/* HARDENING: void by design — enqueue mesh VAO cache clean. */
static void _SitGLDeferCleanMeshVAO(uint64_t mesh_id);
/* HARDENING: void by design — GL deferred destroy flush after fence wait. */
static void _SitGLFlushGraveyard(int frame_index);
/* HARDENING: void by design — debug GL error probe; logs via error channel (plan §3.3). */
static void _SituationCheckGLError(const char* location);
static GLenum _SituationMapDataTypeToGL(SituationDataType type);
static SituationError _SitGLSoftCmdPush(SituationGLSoftCommandBuffer* buf, SitOpCode opcode, SitCommandPacket** out_packet);
static SituationError _SitGLSoftDataPush(SituationGLSoftCommandBuffer* buf, const void* data, size_t size, void** out_ptr);
static SituationError _SituationGLValidateInternalQuadDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass);
static SituationError _SituationGLValidateInternalTextDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass);
static SituationError _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index);
static SituationError _SitGLDeferProgramUniform(SituationGLSoftCommandBuffer* buf, GLuint prog, GLint loc, SituationUniformType type, int elem_count, const void* data, size_t payload_bytes);
/* HARDENING: void by design — presentation alpha fixup (best-effort). */
static void _SitGLEnsureDefaultFramebufferOpaqueAlpha(void);
static GLenum _SitGLUploadNamedBuffer(GLuint buffer_id, GLsizeiptr size, const void* initial_data);
static SituationError _SituationInitGLMDIBuffer(void);
static SituationError _SituationInitGLRingBuffer(void);
static SituationError _SituationInitGLRingFences(void);
/* HARDENING: void by design — GL ring fence wait (blocking sync point). */
static void _SituationGLRingWait(void);
/* HARDENING: void by design — GL context handoff to host thread. */
static void _SituationMakeGLContextCurrentForHostThread(void);
/* HARDENING: void by design — GL context handoff to render thread. */
static void _SituationReleaseHostGLContextForRenderThread(void);
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
static SituationError _SituationInitGLVirtualDisplayRenderer(void);
static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info);
/* HARDENING: void by design — best-effort OpenGL teardown. */
static void _SituationCleanupOpenGL(void);
/* HARDENING: void by design — in-memory virtual texture slot table reset only. */
static void _SituationVirtualBindlessInit(void);
static int _SituationVirtualBindlessBind(GLuint gl_texture_id);
static GLuint _SituationCompileGLShaderEx(const char* source, GLenum type, SituationError* error_code, bool wait_for_compile);
static GLuint _SituationCompileGLShader(const char* source, GLenum type, SituationError* error_code);
static int _SituationPollGLShaderCompile(GLuint shader, GLenum type, SituationError* error_code);
static SituationError _SituationGLAsyncLoadFail(_SituationShaderSlot* slot);
static SituationError _SituationPollGLAsyncShaderLoad(_SituationShaderSlot* slot);
static SituationError _SituationPollGLPendingProgramLink(_SituationShaderSlot* slot);
static GLuint _SituationCreateGLShaderProgramAsync(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgram(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramFromSource(const char* cs_src, SituationError* error_code);
static GLuint _SituationCreateGLComputeProgram(const void* source_data, SituationGLShaderSourceType source_type, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramFromSpirv(const SituationSpirvBinary* vs_blob, const SituationSpirvBinary* fs_blob, SituationError* error_code);
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_OPENGL

#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER)
static SituationError _SituationVulkanBuildGraphicsPipelinesOnSlot(_SituationShaderSlot* slot, const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile);
static SituationError _SituationVulkanBuildMeshPipelinesOnSlot(_SituationShaderSlot* slot, const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len);
/* HARDENING: void by design — async teardown; poll path sets terminal error before free. */
static void _SituationVulkanFreeAsyncShaderLoad(_SituationShaderSlot* slot);
static SituationError _SituationPollVkAsyncShaderLoad(_SituationShaderSlot* slot);
/* HARDENING: void by design — shader compile worker ABI. */
static void _SituationVkAsyncCompileWorker(void* payload, void* unused);
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
static SituationError _SitVulkanEnsureGraphicsPipelineBound(VkCommandBuffer vk_cmd, _SituationShaderSlot* shader_slot, size_t stride);
static SituationError _SitVulkanValidateInternalQuadDrawReady(VkCommandBuffer vk_cmd, const char* caller);
static SituationError _SitVulkanValidateInternalTextDrawReady(VkCommandBuffer vk_cmd, const char* caller);
/* HARDENING: void by design — fills VkDynamicState array from caps; no alloc (pipeline create). */
static void _SitVulkanFillGraphicsDynamicStates(VkDynamicState* states, uint32_t* out_count);

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
/* HARDENING: void by design — record-only mipmap generation. */
static void _SituationVulkanGenerateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t width, int32_t height, uint32_t mip_levels);
static SituationError _SituationVulkanCreateAndUploadBuffer(VkCommandBuffer cmd, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* out_buffer, VmaAllocation* out_allocation);
static SituationError _SituationVulkanReadBackBuffer(VkBuffer src_buffer, VmaAllocation src_alloc, size_t size, size_t offset, void* out_data);
/* HARDENING: void by design — immediate or deferred image destroy helper. */
static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation);
/* HARDENING: void by design — immediate or deferred buffer destroy helper. */
static void _SituationVulkanDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

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
/* HARDENING: bool by design — shutdown-state query for immediate vs deferred destroy. */
static bool _SituationVulkanImmediateDestroyDuringShutdown(void);
static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height);
/* HARDENING: void by design — screenshot resource teardown. */
static void _SituationVulkanDestroyScreenshotResources(void);
/* HARDENING: void by design — record-only screenshot blit. */
static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height);
/* HARDENING: void by design — post-submit screenshot resolve hook. */
static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index);
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
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind);
static shaderc_include_result* _SituationShaderIncluderResolve(void* user_data, const char* requested_source, int type, const char* requesting_source, size_t include_depth);
/* HARDENING: void by design — shaderc includer release callback. */
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_VULKAN

//----------------------------------------------------------------------------------
// GLTF Model Loading
//----------------------------------------------------------------------------------
#if defined(CGLTF_IMPLEMENTATION)
static SituationError _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count);
#endif // CGLTF_IMPLEMENTATION

#endif // SITUATION_IMPL_RENDERER_FWD_H
