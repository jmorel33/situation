/***************************************************************************************************
*
*   situation_api_graphics.h - Graphics, Compute, and GPU Resource API
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Frame lifecycle, command-buffer recording, shaders, textures, buffers, compute pipelines,
*   virtual displays, 3D models, screenshots, profiling metrics, and backend accessor functions.
*
*   Requires SITAPI from situation_api.h. GPU types from situation_api_types_gpu.h.
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_GRAPHICS_H
#define SITUATION_API_GRAPHICS_H

#include "situation_api_config.h"
#include "situation_base_types.h"
#include "situation_api_types_system.h"
#include "situation_api_types_gpu.h"

typedef struct GLFWwindow GLFWwindow;

#if defined(SITUATION_USE_VULKAN)
#include <vulkan/vulkan.h>
#endif

SITAPI bool SituationIsFeatureSupported(SituationRenderFeature feature);                 // Check if a graphics feature is supported on current hardware.
//==================================================================================
// Graphics Module: Rendering, Shaders, and GPU Resources
//==================================================================================

// --- Profiling & Diagnostics ---
SITAPI uint32_t SituationGetDrawCallCount(void); 										// Number of draw commands this frame
SITAPI uint64_t SituationGetVRAMUsage(void);     										// Total GPU memory allocated (Bytes)
SITAPI void SituationExportRenderHistogram(char* buf, size_t buf_size);                  // Write a text-based frame time histogram into buf.
SITAPI double SituationGetMaxFrameTime(void);                                           // Highest observed frame delta (general spike debugging)
SITAPI uint32_t SituationGetFrameSpikeCount(void);                                      // Count of detected frame spikes (general debugging aid)

// Last frame phase times (ns) for cause attribution during debug (backpressure, fence wait, execute, present)
SITAPI void SituationGetLastFramePhases(uint64_t* backpressure_ns, uint64_t* fence_wait_ns, uint64_t* execute_ns, uint64_t* present_ns);
SITAPI void SituationGetFrameProfile(SituationFrameProfile* out);                         // P10.1: single snapshot of frame pacing + phase metrics
SITAPI void SituationResetFrameProfileStats(void);                                      // P10.1: reset spike counter, max frame time, histogram
SITAPI SituationError SituationCmdGPUZoneBegin(SituationCommandBuffer cmd, uint32_t zone_id); // P10.3: GPU elapsed zone begin (no-op when unsupported)
SITAPI SituationError SituationCmdGPUZoneEnd(SituationCommandBuffer cmd, uint32_t zone_id);   // P10.3: GPU elapsed zone end
#if defined(SITUATION_ENABLE_RENDER_THREAD)
SITAPI size_t SituationGetRenderQueueDepth(void);                                       // Get the current depth of the render queue
SITAPI void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);         // Get render thread latency metrics
#endif

// [v2.3.37] I/O Metrics
SITAPI size_t SituationGetIOQueueDepth(void);                                           // [v2.3.37] Get the current depth of the IO/Low Priority queue

// [v2.3.23] Debug Overlay
SITAPI void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color); // Draws FPS, Latency, and Memory stats

// --- Frame Lifecycle & Command Buffer --- [Main thread] [GL+VK]
SITAPI SituationError SituationAcquireFrameCommandBuffer(void);                                   // [GL+VK] Prepare the backend for a new frame of rendering commands.
#if defined(SITUATION_ENABLE_THREADING)
SITAPI SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list, void (*func)(void*, void*), void* user_data); // Submit a render list for async recording on a worker thread.
#else
SITAPI void SituationSubmitRenderList(SituationRenderList list, void (*func)(void*, void*), void* user_data); // Submit a render list for immediate recording (single-threaded fallback).
#endif
SITAPI void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list); // Replay a previously recorded render list into a command buffer.
SITAPI void SituationResetRenderList(SituationRenderList list);                         // Reset a render list for reuse next frame.
SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void);                      // [GL+VK] Get the primary command buffer for the current frame.
SITAPI SituationCommandBuffer SituationGetComputeCommandBuffer(void);                   // [VK] Get the compute-specific command buffer (Vulkan only).
SITAPI SituationError SituationEndFrame(void);                                          // [GL+VK] Submit all commands for the frame and present the result.

// --- Command Buffer Recording ---
SITAPI SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);
SITAPI SituationError SituationCmdSetFrontFace(SituationCommandBuffer cmd, SituationFrontFace front_face);
SITAPI SituationError SituationCmdSetPrimitiveTopology(SituationCommandBuffer cmd, SituationPrimitiveTopology topology);
SITAPI SituationError SituationCmdSetPolygonMode(SituationCommandBuffer cmd, SituationPolygonMode mode);
SITAPI SituationError SituationCmdSetDepthBias(SituationCommandBuffer cmd, bool enable, float constant_factor, float clamp, float slope_factor);
SITAPI SituationError SituationCmdSetLineWidth(SituationCommandBuffer cmd, float width);
SITAPI SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a);
SITAPI SituationError SituationCmdSetStencilTest(SituationCommandBuffer cmd, bool enable, const SituationStencilState* front, const SituationStencilState* back);
SITAPI SituationError SituationCmdSetMultisampleState(SituationCommandBuffer cmd, const SituationMultisampleState* state);
SITAPI SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op);
SITAPI SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable);
SITAPI SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable);
SITAPI SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd, SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb, SituationBlendFactor src_a, SituationBlendFactor dst_a);
SITAPI SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
SITAPI SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
SITAPI SituationRendererBehaviorPolicy SituationRendererBehaviorPolicyDefault(void); // Strict defaults on every axis (Phase 14).
SITAPI SituationError SituationCmdSetRendererBehavior(SituationCommandBuffer cmd, const SituationRendererBehaviorPolicy* policy); // Replace active renderer behavior policy.
SITAPI SituationError SituationCmdPushRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id); // Push current policy; mutate with Set inside scope.
SITAPI SituationError SituationCmdPopRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id); // Restore policy saved by PushRendererBehavior.
SITAPI SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
SITAPI SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
SITAPI SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size);

// --- Abstracted Rendering Commands --- [GL+VK]
SITAPI SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);                           // Sets the dynamic viewport and scissor for the current render pass.
SITAPI SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);                                    // Sets the dynamic scissor rectangle to clip rendering.
SITAPI SituationError SituationCmdSetViewportIndexed(SituationCommandBuffer cmd, uint32_t index, float x, float y, float width, float height);   // Sets viewport at index (0 = default viewport).
SITAPI SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height);             // Sets scissor at index (0 = default scissor).
SITAPI SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);                                     // [GL+VK] Binds a graphics pipeline (shader program) for subsequent draws.
SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);                                             // [High-Level] Records a command to draw a complete, pre-configured mesh.
SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);                                                // [High-Level] Record a command to draw a simple, colored 2D quad.
SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, ColorRGBA tint); // [High-Level] Draw a part of a texture defined by a rectangle.
SITAPI SituationError SituationCmdDrawTextureYpqGrade(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, float phase_shift_deg, float chroma_factor, float luma_factor, float mix); // [High-Level] Draw texture with YPQ grade (matches SituationImageAdjustYPQ).
SITAPI SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size);               // [Core] Set a small block of per-draw uniform data (push constant).
SITAPI SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer);            // [GL+VK] [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index.
SITAPI SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset); // [GL+VK] [Core] Binds a dynamic buffer descriptor set with an offset.
SITAPI SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);             // [GL+VK] [Core] Binds a texture's descriptor set (sampler/storage) to a set index.
SITAPI SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture);           // [Core] Binds a texture as a storage image for compute shaders.
SITAPI SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, uint32_t binding, int size, SituationDataType type, bool normalized, size_t offset); // [OpenGL Only, Deprecated v2.4] Attribute format + vertex buffer binding index. Prefer vertex pulling via SituationGetMeshVertexBufferAddress + sit/gpu/vertex_pull.glslh on Vulkan.
SITAPI SituationError SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer, size_t offset, size_t stride); // [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed.
SITAPI SituationError SituationCmdBindIndexBufferEx(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset, SituationIndexType index_type); // [Core] Bind index buffer with 16- or 32-bit element type for subsequent SituationCmdDrawIndexed.
SITAPI SituationError SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset); // [Core] Bind a 32-bit index buffer (SIT_INDEX_UINT32). Pass offset 0 when indices start at the beginning of the buffer.
SITAPI SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance); // [GL+VK] [Core] Record a non-indexed draw call.
SITAPI SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance); // [GL+VK] [Core] Record an indexed draw call.
SITAPI SituationError SituationCmdDrawIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // [Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect buffer (requires active render pass, bound pipeline, and vertex buffers).
SITAPI SituationError SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // [Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firstIndex is relative to SituationCmdBindIndexBuffer offset.
SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);                     // [GL+VK] Begins a render pass with detailed configuration.
SITAPI SituationError SituationCmdClear(SituationCommandBuffer cmd, uint32_t clear_flags, const SituationClearValue* clear_value);       // Mid-pass clear of active render-pass attachments; begin-pass clears use SituationRenderPassInfo loadOp.
SITAPI SituationError SituationCmdClearColor(SituationCommandBuffer cmd, ColorRGBA color);                                               // Mid-pass clear of the active color attachment.
SITAPI SituationError SituationCmdClearDepth(SituationCommandBuffer cmd, float depth);                                                    // Mid-pass clear of the active depth attachment.
SITAPI SituationError SituationCmdClearStencil(SituationCommandBuffer cmd, uint32_t stencil);                                             // Mid-pass clear of the active stencil attachment when supported by backend/attachment state.
SITAPI SituationError SituationCmdClearDepthStencil(SituationCommandBuffer cmd, float depth, uint32_t stencil);                          // Mid-pass clear of active depth and stencil attachments.
SITAPI SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);                                                                      // [GL+VK] Ends the current render pass.
SITAPI SituationRenderPassInfo SituationRenderPassInfoDefault(int display_id, ColorRGBA clear_color); // Clear color+depth; store color; discard depth/stencil.
SITAPI SituationRenderPassInfo SituationRenderPassInfoLoad(int display_id); // Preserve attachment contents (composite / resume pass).
SITAPI SituationRenderPassInfo SituationRenderPassInfoForRenderTarget(SituationRenderTarget render_target, ColorRGBA clear_color); // Clear user RT pass (display_id ignored).
SITAPI uint32_t SituationRenderPassConfigurationKey(const SituationRenderPassInfo* info); // Vulkan render-pass cache key (load/store + target class).
SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color);		// Draws a text string using GPU-accelerated textured quads.
SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color); // Advanced text drawing (scaling/spacing).
SITAPI SituationError SituationCmdDrawTextBoxed(SituationCommandBuffer cmd, SituationFont font, const char* text, SitRectangle bounds, float fontSize, float spacing, ColorRGBA color, bool word_wrap); // Text clipped to a rectangle with optional word wrap.
SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);                                                  // Submits a command to copy a texture to the main window's swapchain (Compute-Only).
SITAPI SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);                // Binds a texture as a sampled image (sampler2D) to a binding point.

// --- Graphics Resource Management ---

SITAPI SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh); // Create a mesh from vertex and index data.
SITAPI SituationError SituationCreateMeshEx(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMeshVertexLayout layout, SituationMesh* out_mesh); // Create a mesh with an explicit vertex layout tag (includes SIT_MESH_LAYOUT_PULL).
SITAPI SituationError SituationGetMeshVertexLayout(SituationMesh mesh, SituationMeshVertexLayout* out_layout); // Query layout tag stored at creation.
SITAPI void SituationDestroyMesh(SituationMesh* mesh);                                  // Unload a mesh from GPU memory.
SITAPI SituationError SituationCmdBindMeshPullBuffers(SituationCommandBuffer cmd, SituationMesh mesh); // [VK+GL] Push mesh vertex/index BDA block for pull shaders (SituationMeshPullPushConstants @ offset 0). Requires SIT_FEATURE_BINDLESS_BUFFERS.
SITAPI uint64_t SituationGetMeshVertexBufferAddress(SituationMesh mesh);                // GPU VA of mesh vertex buffer. [VK] SIT_FEATURE_BINDLESS_BUFFERS; pull draw: SituationCmdBindMeshPullBuffers + buffer_reference VS (see sit/gpu/vertex_pull.glslh).
SITAPI uint64_t SituationGetMeshIndexBufferAddress(SituationMesh mesh);                 // Retrieves the GPU device address of the mesh index buffer. [VK] requires SIT_FEATURE_BINDLESS_BUFFERS; [GL] NVIDIA-only via NV_shader_buffer_load. Returns 0 if unsupported.
SITAPI uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);                // Retrieves the GPU device address of a buffer for bindless access.
SITAPI uint64_t SituationGetTextureHandle(SituationTexture texture);                    // Retrieves the bindless texture handle (OpenGL Only).

// --- Shader Management ---
SITAPI SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader);   // Load a graphics shader pipeline from vertex and fragment files.
SITAPI SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader); // Create a graphics shader pipeline from in-memory GLSL source.
SITAPI SituationError SituationBeginLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader); // Start non-blocking GLSL load: [OpenGL] async compile/link; [Vulkan] shaderc on worker thread, pipelines on next frames. Poll with SituationPollShaderLoad.
SITAPI SituationError SituationBeginLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader); // [Vulkan] Non-blocking pipeline build from in-memory SPIR-V (bytecode copied). [OpenGL] blocking SPIR-V load (unchanged).
SITAPI SituationError SituationBeginLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader); // [Vulkan] Async SPIR-V with layout profile (e.g. UBO_SSBO for Demon Hunt). [OpenGL] profile ignored; same as SituationBeginLoadShaderFromSpirvMemory.
SITAPI SituationError SituationPollShaderLoad(SituationShader shader); // SITUATION_SUCCESS when ready, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS while compiling/linking/building pipelines.
SITAPI SituationError SituationLoadShaderFromSpirv(const char* vs_spv_path, const char* fs_spv_path, SituationShader* out_shader); // Precompiled .spv: OpenGL via GL_ARB_gl_spirv; Vulkan same pipeline contract as SituationLoadShaderFromMemory (no shaderc required).
SITAPI SituationError SituationLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader); // Same as SituationLoadShaderFromSpirv but from in-memory SPIR-V (e.g. build-time embedded .spv). No hot-reload paths (vs/fs file paths left unset).
SITAPI SituationError SituationLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader); // [Vulkan] `layout_profile` selects user SSBO/UBO layouts; [OpenGL] profile ignored.
SITAPI void SituationUnloadShader(SituationShader* shader);                             // Unload a graphics shader pipeline and free its GPU resources.

// --- Shader Interaction & Synchronization ---
SITAPI SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type); // [OpenGL] Set a standalone uniform by name (location cache). While a frame is active, defers to SIT_OP_SET_UNIFORM.
SITAPI SituationError SituationSetShaderUniformLocation(SituationShader shader, int location, const void* data, SituationUniformType type); // [OpenGL] Set uniform by explicit location (SPIR-V layout(location=)); defers during frames like SituationSetShaderUniform.
SITAPI SituationError SituationBindShaderStorageBlock(SituationShader shader, const char* block_name, uint32_t binding_point); // [OpenGL] glShaderStorageBlockBinding for SPIR-V when reflection reports binding 0 for layout(binding=N).
SITAPI SituationError SituationBindUniformBlock(SituationShader shader, const char* block_name, uint32_t binding_point); // [OpenGL] glUniformBlockBinding for std140 UBO blocks (layout(binding=N)).

SITAPI SituationError SituationQueryShaderStorageBlocks(SituationShader shader, SituationShaderStorageBlockInfo* out_blocks, int capacity, int* out_count); // [OpenGL] Enumerate all active SSBO blocks and their assigned binding points. out_count receives actual block count (may exceed capacity). Returns NOT_IMPLEMENTED on Vulkan.
SITAPI SituationError SituationSetShaderUniform1fv(SituationShader shader, const char* uniform_name, int count, const float* values); // [OpenGL] Set float uniform array.
SITAPI SituationError SituationSetShaderUniform1iv(SituationShader shader, const char* uniform_name, int count, const int* values); // [OpenGL] Set int uniform array in one call (e.g. name "uWallRows[0]", count=24). While a frame is active, records SIT_OP_SET_UNIFORM (same as render-thread mode).
SITAPI SituationError SituationSetShaderUniformMatrix4fv(SituationShader shader, const char* uniform_name, int count, const mat4* matrices); // [OpenGL] Set mat4 uniform array.

SITAPI SituationError SituationValidateShaderUniforms(SituationShader shader, const SituationUniformExpectation* table, int table_count, char* error_buf, size_t error_buf_size); // Returns first missing/wrong-type uniform, or SUCCESS if all resolved.

SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags); // Legacy convenience barrier; prefer SituationCmdPipelineBarrierEx, SituationCmdBufferBarrier, or SituationCmdTextureBarrier for new synchronization code.

// --- Texture Management ---
SITAPI SituationError SituationLoadTexture(const char* file_path, bool generate_mipmaps, SituationTexture* out_texture);// Loads a texture from disk and registers the path for hot-reloading.
SITAPI SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture); // Create a texture from a CPU-side image.
SITAPI SituationError SituationCreateTextureEx(SituationImage image, bool generate_mipmaps, SituationTextureUsageFlags flags, SituationTexture* out_texture); // Create a texture with specific usage flags.
SITAPI void SituationDestroyTexture(SituationTexture* texture);                         // Unload a texture from GPU memory.
SITAPI SituationError SituationGetTextureInfo(SituationTexture texture, SituationTextureInfo* out_info); // [Phase 2] Query texture metadata.
SITAPI SituationError SituationSetTextureSamplerParams(SituationTexture texture, SituationTextureFilter min_filter, SituationTextureFilter mag_filter, SituationTextureWrap wrap_s, SituationTextureWrap wrap_t); // [Phase 2] Update sampler state.
SITAPI SituationError SituationCmdBlitTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureBlitRegion* region); // Blit between color 2D textures; caller owns explicit texture barriers.
SITAPI SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureCopyRegion* region); // Exact-size copy between color 2D textures; caller owns explicit texture barriers.
SITAPI SituationError SituationCmdCopyBufferToTexture(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationTexture dst, const SituationTextureCopyRegion* dst_region); // Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller owns texture barriers.
SITAPI SituationError SituationCmdCopyTextureToBuffer(SituationCommandBuffer cmd, SituationTexture src, const SituationTextureCopyRegion* src_region, SituationBuffer dst, size_t dst_offset, size_t dst_row_pitch); // Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller owns texture barriers.
SITAPI SituationError SituationReadTexture(SituationTexture texture, const SituationTextureReadbackDesc* desc, void* dst_pixels, size_t dst_size_bytes); // [Phase 2] Blocking readback of texture pixels.
SITAPI SituationError SituationReadTextureAlloc(SituationTexture texture, const SituationTextureReadbackDesc* desc, SituationImage* out_image); // [Phase 2] Blocking readback into allocated SituationImage.
SITAPI SituationError SituationReadFramebuffer(const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes); // [Phase 2] Blocking readback of framebuffer pixels (RGBA8 or raw RGB10 packed).
SITAPI SituationError SituationReadFramebufferHdr(const SituationReadPixelsDesc* desc, uint32_t* dst_pixels, size_t dst_size_bytes); // [Phase 8] Raw A2R10G10B10 readback when HDR10 swapchain active.

// --- User render targets (Phase 3c — offscreen without VD compositor) ---
SITAPI SituationError SituationCreateRenderTarget(const SituationRenderTargetDesc* desc, SituationRenderTarget* out_rt); // Create color (+ optional depth) offscreen target; msaa_samples must be 1.
SITAPI void SituationDestroyRenderTarget(SituationRenderTarget* rt); // Destroy RT and invalidate handle.
SITAPI SituationError SituationGetRenderTargetTexture(SituationRenderTarget rt, SituationTexture* out_tex); // Registry color texture for sampling / transfer readback.
SITAPI SituationError SituationReadRenderTarget(SituationRenderTarget rt, const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes); // Blocking RGBA8 readback of resolved color.

// --- User query pools (P10.4 — timestamps + occlusion) ---
SITAPI SituationError SituationCreateQueryPool(SituationQueryType type, uint32_t count, SituationQueryPool* out_pool);
SITAPI void SituationDestroyQueryPool(SituationQueryPool* pool);
SITAPI SituationError SituationCmdResetQueryPool(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t first_query, uint32_t query_count);
SITAPI SituationError SituationCmdWriteTimestamp(SituationCommandBuffer cmd, uint32_t pipeline_stage, SituationQueryPool pool, uint32_t query_index);
SITAPI SituationError SituationCmdBeginOcclusionQuery(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t query_index);
SITAPI SituationError SituationCmdEndOcclusionQuery(SituationCommandBuffer cmd);
SITAPI SituationError SituationGetQueryPoolResults(SituationQueryPool pool, uint32_t first_query, uint32_t query_count, uint64_t* out_results, uint32_t flags);

// --- Compute Shader Pipeline ---
SITAPI SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from a shader file.
SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from in-memory GLSL source.
SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);        // Destroy a compute pipeline and free its GPU resources.
SITAPI SituationError SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline); // Bind a compute pipeline for a subsequent dispatch.
SITAPI SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z); // Record a compute dispatch with validation and error reporting.
SITAPI SituationError SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z); // Record a command to dispatch compute shader work groups.
SITAPI SituationError SituationCmdDispatchIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // Record an indirect compute dispatch.
SITAPI void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z); // Query maximum compute work group count per dispatch.
SITAPI SituationError SituationCmdPipelineBarrierEx(SituationCommandBuffer cmd, const SituationPipelineBarrierDesc* desc); // Record an explicit global memory barrier.
SITAPI SituationError SituationCmdBufferBarrier(SituationCommandBuffer cmd, const SituationBufferBarrierDesc* desc); // Record an explicit buffer-range memory barrier.
SITAPI SituationError SituationCmdTextureBarrier(SituationCommandBuffer cmd, SituationTexture texture, const SituationTextureBarrierDesc* desc); // Record an explicit texture layout/memory barrier.

// --- GPU Buffer Management ---
SITAPI SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer); // Create a generic GPU data buffer (e.g., SSBO).
SITAPI SituationError SituationCreateReadbackBuffer(size_t size, SituationBuffer* out_buffer); // [Phase 1] Create an async GPU->CPU staging buffer.
SITAPI void SituationDestroyBuffer(SituationBuffer* buffer);                            // Destroy a GPU buffer.
SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data); // Update data in a GPU buffer.
SITAPI SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data); // Read data from a GPU buffer (blocking).
SITAPI SituationError SituationCmdCopyBufferEx(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t src_offset, size_t dst_offset, size_t size); // Error-returning buffer-copy command with independent source/destination offsets.
SITAPI SituationError SituationCmdCopyBuffer(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t offset, size_t size); // Legacy buffer-copy command (now returns error).
SITAPI SituationError SituationReadBuffer(SituationBuffer readback_buf, void* dst, size_t size); // Read mapped buffer data safely.

// --- Virtual Displays (Render Targets) ---
SITAPI SituationError SituationCreateVirtualDisplayFromDesc(const SituationVirtualDisplayDesc* desc, int* out_id); // Create a virtual display from a full desc (VD-1 attachment config).
SITAPI SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id); // Create an off-screen render target.
SITAPI SituationError SituationCreateVirtualDisplayEx(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, SituationVDFlags flags, int* out_id); // Create a virtual display with extended flags (e.g. COMPUTE_TARGET for compute shader writes).
SITAPI SituationRenderPassInfo SituationRenderPassInfoInherit(int display_id); // Fill pass struct from VD attachment defaults (tier C helper).
SITAPI SituationError SituationSetVirtualDisplayAttachmentDefaults(int display_id, const SituationVirtualDisplayAttachmentDefaults* defaults); // Tier B storage-only attachment defaults.
SITAPI SituationError SituationSetVirtualDisplayClearColor(int display_id, ColorRGBA color); // Tier B: sets attachment_defaults.clear.color only.
SITAPI SituationError SituationSetVirtualDisplaySampler(int display_id, const SituationVirtualDisplaySamplerDesc* sampler); // VD-3 composite sampler (light rebuild).
SITAPI SituationError SituationSetVirtualDisplayMaxAnisotropy(int display_id, float max_anisotropy); // VD-4a aniso on composite sampler.
SITAPI SituationError SituationSetVirtualDisplayMipLevels(int display_id, uint32_t color_mip_levels, uint32_t sampler_max_mip_level); // VD-4a mip LOD clamp; storage mips are create-time only.
SITAPI SituationError SituationSetVirtualDisplayUpdateMode(int display_id, SituationVirtualDisplayUpdateMode mode); // VD-5 dynamic vs static (static: frame clock frozen, dirty-driven).
SITAPI SituationError SituationSetVirtualDisplayMemoryHint(int display_id, SituationVirtualDisplayMemoryHint hint); // VD-5 stored hint (best-effort; no FBO rebuild).
SITAPI SituationError SituationGetVirtualDisplayTexture(int display_id, SituationTexture* out_texture); // Get the VD's internal texture as a SituationTexture handle (valid for compute-target VDs).
SITAPI SituationError SituationDestroyVirtualDisplay(int display_id);                   // Destroy a virtual display.
SITAPI SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd);       // Composite all visible virtual displays to the current target.
SITAPI SituationError SituationConfigureVirtualDisplay(int display_id, Vector2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode); // Configure a virtual display's properties.
SITAPI SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id);             // Get a pointer to a virtual display's state.
SITAPI SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode); // Layout rect only (v2.4.387+); filter via SetVirtualDisplaySampler.
SITAPI void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty);             // Mark a virtual display as needing to be re-rendered.
SITAPI bool SituationIsVirtualDisplayDirty(int display_id);                             // Check if a virtual display is marked as dirty.
SITAPI double SituationGetLastVDCompositeTimeMS(void);                                  // Get the time taken for the last virtual display composite pass.
SITAPI SituationError SituationGetVirtualDisplayUpdateInfo(int display_id, double* out_last_content_update_time, uint64_t* out_last_content_update_frame, uint64_t* out_frames_since_update, double* out_seconds_since_update); // Query last VD content write (not the frame clock).
SITAPI void SituationSetVirtualDisplayIdleThreshold(int display_id, double threshold_seconds); // Set idle threshold for compositor fallback (Phase 2a).
SITAPI void SituationSetVirtualDisplayFallbackMode(int display_id, SituationVDFallbackMode mode); // SOLID, COLORBURST, or PATTERN (create default: PATTERN + zero layers = snow).
SITAPI void SituationSetVirtualDisplayFallbackColor(int display_id, ColorRGBA color); // SOLID idle tint (normalized by compositor).
SITAPI void SituationSetVirtualDisplayPatternLayers(int display_id, int32_t pattern_layers); // Toggle bitmask + PATTERN fallback (plan §3.4).
SITAPI int32_t SituationGetVirtualDisplayPatternLayers(int display_id); // Current standby layer bitmask (0 if invalid id).
SITAPI void SituationSetVirtualDisplayChromaSnow(int display_id, bool enabled); // Bit 16: RGB noise when idle snow (no layers 0–8).
SITAPI bool SituationGetVirtualDisplayChromaSnow(int display_id); // Chroma snow flag on standby bitmask.
SITAPI void SituationSetVirtualDisplayPatternConfig(int display_id, const SitVdStandbyConfig* config); // Full standby tuning + PATTERN mode.
SITAPI void SituationGetVirtualDisplayPatternConfig(int display_id, SitVdStandbyConfig* out_config); // Copy current standby config (no-op if invalid id).
SITAPI void SituationVdStandbyConfigInitDefaults(SitVdStandbyConfig* cfg, int layer_index, float width, float height); // RGL defaults + optional single layer bit.
SITAPI uint32_t SituationVdStandbyLayerBit(int layer_index); // 1u << layer_index (0–8).
SITAPI void SituationVdStandbyToggleLayer(SitVdStandbyConfig* cfg, int layer_index, bool enabled); // Flip layer bit + sync stack.
SITAPI void SituationVdStandbySetLayerOrder(SitVdStandbyConfig* cfg, const uint8_t* stack, uint8_t count); // Replace compose stack.
SITAPI void SituationVdStandbyPackStd140(uint8_t out[SIT_VD_STANDBY_CONFIG_UBO_SIZE], const SitVdStandbyConfig* cfg); // 160 B header UBO (P10).
SITAPI void SituationVdStandbyPackParamsStd430(uint8_t out[SIT_VD_STANDBY_PARAMS_SSBO_SIZE], const SitVdStandbyConfig* cfg); // Layer params SSBO (P10).
SITAPI void SituationGetVirtualDisplaySize(int display_id, int* width, int* height);    // Get the internal resolution of a virtual display.

// --- Camera & Projection Math ---
SITAPI void SituationCameraBuildView(const SituationCameraDesc* desc, mat4 out_view);
SITAPI void SituationCameraBuildProj(const SituationCameraDesc* desc, mat4 out_proj);
SITAPI void SituationCameraBuildViewProj(const SituationCameraDesc* desc, mat4 out_vp);
SITAPI void SituationCameraBuildInvViewProj(const SituationCameraDesc* desc, mat4 out_inv_vp);
SITAPI void SituationCameraUnprojectPixel(const SituationCameraDesc* desc, const mat4 inv_vp, Vector2 pixel, Vector2 framebuffer_px, Vector3* out_ray_origin, Vector3* out_ray_dir);

// --- 3D Model Utilities ---
SITAPI SituationError SituationLoadModel(const char* file_path, SituationModel* out_model); // Loads a complete 3D model and its textures from a GLTF file.
SITAPI SituationError SituationLoadModelFromSTL(const char* file_path, bool smooth_normals, SituationModel* out_model); // Loads a 3D model from a binary or ASCII STL file. UVs are zeroed; normals are flat (per-face) by default, or smooth (averaged per shared vertex) when smooth_normals is true.
SITAPI SituationError SituationLoadModelFromOBJ(const char* file_path, SituationModel* out_model); // Wavefront OBJ: triangulated meshes, MTL/textures; missing/degenerate normals filled from face geometry, authored normals preserved.
SITAPI void SituationUnloadModel(SituationModel* model);                                // Frees all GPU and CPU resources associated with a loaded model.
SITAPI SituationError SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform); // Draws all sub-meshes of a model with a single root transformation.
SITAPI SituationError SituationSaveModelAsGltf(SituationModel model, const char* file_path);      // Exports a model to a human-readable .gltf and a .bin file for debugging.
SITAPI void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count, int* vertex_stride, void** index_data, int* index_count); // Get raw vertex/index data pointers from a mesh (read-only).

// --- Image & Screenshot Utilities ---
/** Arm pre-swap capture for the next SituationEndFrame (pair with SituationLoadImageFromScreen). */
SITAPI void SituationRequestScreenCapture(void);
SITAPI SituationError SituationLoadImageFromScreen(SituationImage* out_image);          // Get a copy of the current screen backbuffer as an image.
SITAPI SituationError SituationTakeScreenshot(const char *fileName);                    // Take a screenshot. fileName is the base name (no extension) or NULL for auto-name. Extension added from format setting.
SITAPI void SituationSetScreenshotFormat(SituationScreenshotFormat format);             // Set the default screenshot file format (default: BMP).
SITAPI SituationScreenshotFormat SituationGetScreenshotFormat(void);                    // Get the current default screenshot format.

// --- Backend-Specific Accessors ---
SITAPI SituationRendererType SituationGetRendererType(void);                            // Legacy — prefer SituationGetGraphicsBackend() + SituationGetGraphicsCaps().
SITAPI GLFWwindow* SituationGetGLFWwindow(void);                                        // Get the raw GLFW window handle.
#ifdef SITUATION_USE_VULKAN
SITAPI VkInstance SituationGetVulkanInstance(void);                                     // Get the raw Vulkan instance handle.
SITAPI VkDevice SituationGetVulkanDevice(void);                                         // Get the raw Vulkan logical device handle.
SITAPI VkPhysicalDevice SituationGetVulkanPhysicalDevice(void);                         // Get the raw Vulkan physical device handle.
SITAPI VkRenderPass SituationGetMainWindowRenderPass(void);                             // Get the render pass for the main window.
#endif

#endif /* SITUATION_API_GRAPHICS_H */
