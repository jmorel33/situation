/***************************************************************************************************
*
*   situation_impl_renderer_fwd.h - Forward Declarations for Renderer Internal Functions
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file contains forward declarations for all static functions defined in
*   situation_impl_renderer.h. Preprocessor guards mirror the renderer's structure
*   so that Vulkan/OpenGL-specific types are only referenced when the corresponding
*   backend is active.
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_FWD_H
#define SITUATION_IMPL_RENDERER_FWD_H

//----------------------------------------------------------------------------------
// Shared / Unconditional
//----------------------------------------------------------------------------------
static _SituationUniformMap* _sit_uniform_map_create(void);
static void _sit_uniform_map_destroy(_SituationUniformMap* map);
static void _sit_uniform_map_resize(_SituationUniformMap* map);
static void _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value);
static int32_t _sit_uniform_map_get(_SituationUniformMap* map, const char* key);

static bool _SituationValidateRenderCaps(void);
static SituationError _SituationInitRenderer(const SituationInitInfo* init_info);
static bool _SituationInitDefaultFont(void);
static bool _SituationInitTextRenderer(void);
static bool _SituationInitQuadRenderer(int width, int height);
static void _SituationCleanupQuadRenderer(void);
static void _SituationCleanupDanglingResources(void);
static void _SituationPerformHotReloadPass(void);

// Display & Virtual Display
static void _SituationCachePhysicalDisplays(void);
static int _SituationSortVirtualDisplaysCallback(const void* a, const void* b);

// Render Lists
static void _SituationReplayToQueue(SituationRenderList list, int frame_idx);
static void _SituationEnqueueRenderList(SituationRenderList list);

// Resource Slot Access
static _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle);
static _SituationShaderSlot* _SitGetShaderSlot(SituationShader handle);
static _SituationMeshSlot* _SitGetMeshSlot(SituationMesh handle);
static _SituationBufferSlot* _SitGetBufferSlot(SituationBuffer handle);
static _SituationComputePipelineSlot* _SitGetComputePipelineSlot(SituationComputePipeline handle);
static _SituationModelSlot* _SitGetModelSlot(SituationModel handle);
static _SituationSoundSlot* _SitGetSoundSlot(SituationSound handle);

// Resource Slot Allocation
static _SituationShaderSlot* _SitAllocShaderSlot(SituationShader* out_handle);
static void _SitFreeShaderSlot(SituationShader handle);
static _SituationComputePipelineSlot* _SitAllocComputePipelineSlot(SituationComputePipeline* out_handle);
static void _SitFreeComputePipelineSlot(SituationComputePipeline handle);
static _SituationMeshSlot* _SitAllocMeshSlot(SituationMesh* out_handle);
static void _SitFreeMeshSlot(SituationMesh handle);
static _SituationBufferSlot* _SitAllocBufferSlot(SituationBuffer* out_handle);
static void _SitFreeBufferSlot(SituationBuffer handle);
static _SituationModelSlot* _SitAllocModelSlot(SituationModel* out_handle);
static void _SitFreeModelSlot(SituationModel handle);

//----------------------------------------------------------------------------------
// OpenGL Backend
//----------------------------------------------------------------------------------
#if defined(SITUATION_USE_OPENGL)
static void _SitGLBackupState(_SitGLStateBackup* s);
static void _SitGLRestoreState(_SitGLStateBackup* s);
static void _SitGLInvalidateShadowState(void);
static GLuint _SitGLGetCachedVAO(SituationMesh mesh);
static void _SitGLDeferDestroyBuffer(GLuint id);
static void _SitGLDeferDestroyTexture(GLuint id);
static void _SitGLDeferCleanMeshVAO(uint64_t mesh_id);
static void _SitGLFlushGraveyard(int frame_index);
static void _SituationCheckGLError(const char* location);
static GLenum _SituationMapDataTypeToGL(SituationDataType type);
static SitCommandPacket* _SitGLSoftCmdPush(SituationGLSoftCommandBuffer* buf, SitOpCode opcode);
static void* _SitGLSoftDataPush(SituationGLSoftCommandBuffer* buf, const void* data, size_t size);
static void _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index);
static bool _SituationInitGLVirtualDisplayRenderer(void);
static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info);
static void _SituationCleanupOpenGL(void);
static void _SituationVirtualBindlessInit(void);
static int _SituationVirtualBindlessBind(GLuint gl_texture_id);
static GLuint _SituationCompileGLShader(const char* source, GLenum type, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramAsync(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgram(const char* vs_src, const char* fs_src, SituationError* error_code);
static GLuint _SituationCreateGLShaderProgramFromSource(const char* cs_src, SituationError* error_code);
static GLuint _SituationCreateGLComputeProgram(const void* source_data, SituationGLShaderSourceType source_type, SituationError* error_code);
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static GLuint _SituationCreateGLShaderProgramFromSpirv(const _SituationSpirvBlob* vs_blob, const _SituationSpirvBlob* fs_blob, SituationError* error_code);
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_OPENGL

//----------------------------------------------------------------------------------
// Vulkan Backend
//----------------------------------------------------------------------------------
#if defined(SITUATION_USE_VULKAN)
// Staging & Graveyard
static SituationError _SituationInitStagingBuffers(void);
static void _SituationCleanupStagingBuffers(void);
static void _SituationInitGraveyard(_SituationVKGraveyard* gy);
static void _SituationCleanupGraveyard(_SituationVKGraveyard* gy);
static void _SituationFlushGraveyard(uint32_t frame_index);

// Deferred Destroy
static void _SituationDeferDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
static void _SituationDeferDestroyImage(VkImage image, VmaAllocation allocation, VkImageView view, VkSampler sampler);
static void _SituationDeferDestroyDescriptorSet(VkDescriptorSet set, VkDescriptorPool pool);
static void _SituationDeferDestroyPipeline(VkPipeline pipeline, VkPipelineLayout layout);
static void _SituationDeferDestroyFramebuffer(VkFramebuffer framebuffer);
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
static void _SituationVulkanQuerySwapchainSupport(VkPhysicalDevice device, _SituationVulkanSwapchainSupportDetails* out_details);
static void _SituationVulkanFreeSwapchainSupportDetails(_SituationVulkanSwapchainSupportDetails* details);
static SituationError _SituationVulkanCreateSwapchain(void);
static SituationError _SituationVulkanCreateImageViews(void);
static SituationError _SituationVulkanCreateDepthResources(void);
static SituationError _SituationVulkanCreateFramebuffers(void);
static void _SituationVulkanCleanupSwapchain(void);
static void _SituationVulkanRecreateSwapchain(void);

// Render Pass & Pipeline
static SituationError _SituationVulkanCreateRenderPass(void);
static VkRenderPass _SituationVulkanGetOrCreateRenderPass(_SituationVulkanState* vk_state, const SituationRenderPassInfo* info);
static VkFormat _SituationVulkanFindSupportedFormat(const VkFormat* candidates, uint32_t candidate_count, VkImageTiling tiling, VkFormatFeatureFlags features);
static VkPipeline _SituationVulkanCreateGraphicsPipeline(const void* vs_code, size_t vs_size, const void* fs_code, size_t fs_size, VkPipelineLayout layout, VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings, uint32_t attr_count, const VkVertexInputAttributeDescription* attrs, uint32_t pipeline_flags);

// Command Pool & Buffers
static SituationError _SituationVulkanCreateCommandPool(void);
static SituationError _SituationVulkanCreateCommandBuffers(void);
static SituationError _SituationVulkanCreateSyncObjects(void);
static VkCommandBuffer _SituationVulkanBeginSingleTimeCommands(void);
static void _SituationVulkanEndSingleTimeCommands(VkCommandBuffer command_buffer);

// Image & Buffer Operations
static SituationError _SituationVulkanCreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkImage* out_image, VmaAllocation* out_allocation);
static VkImageView _SituationVulkanCreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
static void _SituationVulkanTransitionImageLayout(VkCommandBuffer cmd, VkImage image, uint32_t mip_levels, VkImageLayout old_layout, VkImageLayout new_layout);
static void _SituationVulkanCopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
static void* _SituationVulkanBlitImageToHostVisibleBuffer(VkImage srcImage, VkImageLayout srcImageLayout, uint32_t width, uint32_t height);
static void _SituationVulkanGenerateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t width, int32_t height, uint32_t mip_levels);
static SituationError _SituationVulkanCreateAndUploadBuffer(VkCommandBuffer cmd, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* out_buffer, VmaAllocation* out_allocation);
static SituationError _SituationVulkanReadBackBuffer(VkBuffer src_buffer, VmaAllocation src_alloc, size_t size, size_t offset, void* out_data);
static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation);
static void _SituationVulkanDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

// Shader Modules
static VkShaderModule _SituationCreateVulkanShaderModule(const char* code, size_t code_size);
static VkShaderModule _SituationVulkanCreateShaderModule(const void* code, size_t code_size);
static SituationShader _SituationCreateVulkanPipeline(const char* vs_path, const char* fs_path);
static VkDescriptorSet _SituationVulkanAllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool* out_pool);

// Screen Copy
static SituationError _SituationVulkanCreateScreenCopyResource(void);
static void _SituationVulkanDestroyScreenCopyResource(void);

// Submit
static void _SituationSubmitCompute(VkCommandBuffer cmd);
static VkResult _SituationSubmitGraphics(VkCommandBuffer cmd);

// Init & Cleanup
static SituationError _SituationInitVulkan(const SituationInitInfo* init_info);
static SituationError _SituationVulkanInitInternalRenderers(void);
static SituationError _SituationVulkanInitComputeLayouts(void);
static void _SituationCleanupVulkan(void);

// SPIR-V
static char* _SituationReadSpirvFile(const char* filename, size_t* out_size);
static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob);

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(const char* glsl_source, const char* source_name, shaderc_shader_kind shader_kind);
static shaderc_include_result* _SituationShaderIncluderResolve(void* user_data, const char* requested_source, int type, const char* requesting_source, size_t include_depth);
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result);
#endif // SITUATION_ENABLE_SHADER_COMPILER
#endif // SITUATION_USE_VULKAN

//----------------------------------------------------------------------------------
// GLTF Model Loading
//----------------------------------------------------------------------------------
#if defined(CGLTF_IMPLEMENTATION)
static bool _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count);
#endif // CGLTF_IMPLEMENTATION

#endif // SITUATION_IMPL_RENDERER_FWD_H
