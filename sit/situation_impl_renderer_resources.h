/***************************************************************************************************
*
*   situation_impl_renderer_resources.h - Buffer, Texture, and Mesh Resource Management
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Resource allocation, upload, readback, slot registry helpers (GL + VK inline).
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_RESOURCES_H
#define SITUATION_IMPL_RENDERER_RESOURCES_H

/**
 * @brief Binds a texture to a specific binding point for use by the currently bound pipeline.
 * @details Associates a `SituationTexture` with a sampler or image unit declared in the shader code (e.g., `layout(binding = X) uniform sampler2D ...`).
 *          This allows the shader to sample or read from the texture.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindTextureUnit(contract_id, texture.gl_texture_id)`.
 *   This efficiently binds the texture to the specified texture unit.
 * - **Vulkan:** This function leverages the persistent descriptor set model for textures.
 *   When the `SituationTexture` was created (e.g., via `SituationLoadTexture`), the Vulkan backend allocated a `VkDescriptorSet` (for combined image samplers) and
 *   populated it with the texture's `VkImageView` and `VkSampler`. This function records a `vkCmdBindDescriptorSets` command using this pre-cached descriptor set stored in `texture.descriptor_set`. This is a very fast operation, avoiding runtime allocation and updates of descriptor sets.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param contract_id The binding point ID within the shader.
 *                    In OpenGL, this corresponds to the texture unit index.
 *                    In Vulkan, this corresponds to the `dstBinding` used when the
 *                    texture's internal descriptor set was populated.
 * @param texture The `SituationTexture` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the texture handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the texture's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `contract_id` matches the binding point defined in the shader.
 */
// [INTERNAL] Resolves a public handle to a pointer to the internal slot.
// Returns NULL if the handle is stale (generation mismatch) or invalid.
static _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle) {
    if (handle.slot_index >= SITUATION_MAX_TEXTURES) return NULL;

    _SituationTextureSlot* slot = &sit_render.texture_registry[handle.slot_index];

    // Generation Check: Prevents Use-After-Free
    if (!slot->is_active || slot->generation != handle.generation) {
        return NULL;
    }
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal shader slot for a given handle, with validation.
 *
 * @details Performs bounds checking and generation validation on the `SituationShader` handle.
 *          Returns the corresponding slot pointer if the handle is valid and active,
 *          otherwise returns NULL (invalid, stale, or freed handle).
 *
 *          This is the canonical safe way to access shader data from a public handle
 *          throughout the library (e.g. in bind, destroy, hot-reload paths).
 *
 * @param handle The SituationShader handle to resolve
 * @return Valid _SituationShaderSlot* if handle is active and matches generation,
 *         NULL otherwise (invalid handle, out of range, or already freed)
 *
 * @see _SitAllocShaderSlot, _SitFreeShaderSlot, SituationShader
 */
static _SituationShaderSlot* _SitGetShaderSlot(SituationShader handle) {
    if (handle.slot_index >= SITUATION_MAX_SHADERS) return NULL;
    _SituationShaderSlot* slot = &sit_render.shader_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal mesh slot for a given handle, with validation.
 *
 * @details Checks slot index bounds and generation match.
 *          Returns the slot pointer if the handle is currently active,
 *          otherwise NULL (invalid, stale generation, or freed).
 *
 * @param handle The SituationMesh handle to resolve
 * @return Valid _SituationMeshSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocMeshSlot, _SitFreeMeshSlot, SituationMesh
 */
static _SituationMeshSlot* _SitGetMeshSlot(SituationMesh handle) {
    if (handle.slot_index >= SITUATION_MAX_MESHES) return NULL;
    _SituationMeshSlot* slot = &sit_render.mesh_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal buffer slot for a given handle, with validation.
 *
 * @details Validates slot index and generation counter.
 *          Returns the slot pointer only if the buffer is still active.
 *
 * @param handle The SituationBuffer handle to resolve
 * @return Valid _SituationBufferSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocBufferSlot, _SitFreeBufferSlot, SituationBuffer
 */
static _SituationBufferSlot* _SitGetBufferSlot(SituationBuffer handle) {
    if (handle.slot_index >= SITUATION_MAX_BUFFERS) return NULL;
    _SituationBufferSlot* slot = &sit_render.buffer_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal compute pipeline slot for a given handle, with validation.
 *
 * @details Checks bounds and generation match.
 *          Returns the slot pointer if the pipeline is active,
 *          otherwise NULL.
 *
 * @param handle The SituationComputePipeline handle to resolve
 * @return Valid _SituationComputePipelineSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocComputePipelineSlot, _SitFreeComputePipelineSlot, SituationComputePipeline
 */
static _SituationComputePipelineSlot* _SitGetComputePipelineSlot(SituationComputePipeline handle) {
    if (handle.slot_index >= SITUATION_MAX_COMPUTE_PIPELINES) return NULL;
    _SituationComputePipelineSlot* slot = &sit_render.compute_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal model slot for a given handle, with validation.
 *
 * @details Validates slot index and generation.
 *          Returns the slot pointer only if the model is still active.
 *
 * @param handle The SituationModel handle to resolve
 * @return Valid _SituationModelSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocModelSlot, _SitFreeModelSlot, SituationModel
 */
static _SituationModelSlot* _SitGetModelSlot(SituationModel handle) {
    if (handle.slot_index >= SITUATION_MAX_MODELS) return NULL;
    _SituationModelSlot* slot = &sit_render.model_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}


// --- Resource Allocation Helpers ---

/**
 * @brief [INTERNAL] Allocates a free shader slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationShader handle on success
 * @return Pointer to the allocated _SituationShaderSlot, or NULL on failure
 *
 * @see _SitFreeShaderSlot, SituationCreateShader, SituationCreateShaderFromSpirv
 */
static _SituationShaderSlot* _SitAllocShaderSlot(SituationShader* out_handle) {
    if (!out_handle) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitAllocShaderSlot: out_handle is NULL.");
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
        if (!sit_render.shader_registry[i].is_active) {
            _SituationShaderSlot* slot = &sit_render.shader_registry[i];
			uint32_t preserved_gen = slot->generation; // [FIX] Preserve generation
            memset(slot, 0, sizeof(_SituationShaderSlot));
            slot->is_active = true;
			slot->generation = preserved_gen + 1;      // [FIX] Increment preserved
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
        "_SitAllocShaderSlot: shader registry full (SITUATION_MAX_SHADERS).");
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a shader slot and releases associated shader modules/programs.
 *
 * @param handle The SituationShader handle to free (invalid handles are ignored)
 *
 * @see _SitAllocShaderSlot, SituationDestroyShader
 */
static void _SitFreeShaderSlot(SituationShader handle) {
    /* HARDENING: void by design — idempotent slot release; invalid/stale handles ignored. */
    _SituationShaderSlot* slot = _SitGetShaderSlot(handle);
    if (!slot) return;

    if (slot->vs_path) SIT_FREE(slot->vs_path);
    if (slot->fs_path) SIT_FREE(slot->fs_path);

    slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free compute pipeline slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationComputePipeline handle on success
 * @return Pointer to the allocated _SituationComputePipelineSlot, or NULL on failure
 *
 * @see _SitFreeComputePipelineSlot, SituationCreateComputePipeline
 */
static _SituationComputePipelineSlot* _SitAllocComputePipelineSlot(SituationComputePipeline* out_handle) {
    if (!out_handle) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitAllocComputePipelineSlot: out_handle is NULL.");
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_COMPUTE_PIPELINES; i++) {
        if (!sit_render.compute_registry[i].is_active) {
            _SituationComputePipelineSlot* slot = &sit_render.compute_registry[i];
			uint32_t preserved_gen = slot->generation; // [FIX] Preserve generation
            memset(slot, 0, sizeof(_SituationComputePipelineSlot));
            slot->is_active = true;
			slot->generation = preserved_gen + 1;      // [FIX] Increment preserved
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
        "_SitAllocComputePipelineSlot: compute pipeline registry full.");
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a compute pipeline slot and releases GPU resources.
 *
 * @param handle The SituationComputePipeline handle to free (invalid handles are ignored)
 *
 * @see _SitAllocComputePipelineSlot, SituationDestroyComputePipeline
 */
static void _SitFreeComputePipelineSlot(SituationComputePipeline handle) {
    /* HARDENING: void by design — idempotent slot release; invalid/stale handles ignored. */
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(handle);
    if (!slot) return;

    if (slot->source_path) SIT_FREE(slot->source_path);

    slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free mesh slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationMesh handle on success
 * @return Pointer to the allocated _SituationMeshSlot, or NULL on failure
 *
 * @see _SitFreeMeshSlot, SituationCreateMesh
 */
static _SituationMeshSlot* _SitAllocMeshSlot(SituationMesh* out_handle) {
    if (!out_handle) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitAllocMeshSlot: out_handle is NULL.");
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_MESHES; i++) {
        if (!sit_render.mesh_registry[i].is_active) {
            _SituationMeshSlot* slot = &sit_render.mesh_registry[i];
			uint32_t preserved_gen = slot->generation; // [FIX] Preserve generation
            memset(slot, 0, sizeof(_SituationMeshSlot));
            slot->is_active = true;
            slot->generation = preserved_gen + 1;      // [FIX] Increment preserved
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
        "_SitAllocMeshSlot: mesh registry full (SITUATION_MAX_MESHES).");
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a mesh slot and releases associated resources.
 *
 * @param handle The SituationMesh handle to free (invalid handles are ignored)
 *
 * @see _SitAllocMeshSlot, SituationDestroyMesh
 */
static void _SitFreeMeshSlot(SituationMesh handle) {
    /* HARDENING: void by design — idempotent slot release; invalid/stale handles ignored. */
    _SituationMeshSlot* slot = _SitGetMeshSlot(handle);
    if (slot) slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free buffer slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationBuffer handle on success
 * @return Pointer to the allocated _SituationBufferSlot, or NULL on failure
 *
 * @see _SitFreeBufferSlot, SituationCreateBuffer
 */
static _SituationBufferSlot* _SitAllocBufferSlot(SituationBuffer* out_handle) {
    if (!out_handle) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitAllocBufferSlot: out_handle is NULL.");
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_BUFFERS; i++) {
        if (!sit_render.buffer_registry[i].is_active) {
            _SituationBufferSlot* slot = &sit_render.buffer_registry[i];
			uint32_t preserved_gen = slot->generation; // [FIX] Preserve generation
            memset(slot, 0, sizeof(_SituationBufferSlot));
            slot->is_active = true;
            slot->generation = preserved_gen + 1;      // [FIX] Increment preserved
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
        "_SitAllocBufferSlot: buffer registry full (SITUATION_MAX_BUFFERS).");
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a buffer slot and releases GPU/CPU resources.
 *
 * @param handle The SituationBuffer handle to free (invalid handles are ignored)
 *
 * @see _SitAllocBufferSlot, SituationDestroyBuffer
 */
static void _SitFreeBufferSlot(SituationBuffer handle) {
    /* HARDENING: void by design — idempotent slot release; invalid/stale handles ignored. */
    _SituationBufferSlot* slot = _SitGetBufferSlot(handle);
    if (slot) slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free model slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationModel handle on success
 * @return Pointer to the allocated _SituationModelSlot, or NULL on failure
 *         (pool full or allocation error)
 *
 * @see _SitFreeModelSlot, SituationCreateModel
 */
static _SituationModelSlot* _SitAllocModelSlot(SituationModel* out_handle) {
    if (!out_handle) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitAllocModelSlot: out_handle is NULL.");
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_MODELS; i++) {
        if (!sit_render.model_registry[i].is_active) {
            _SituationModelSlot* slot = &sit_render.model_registry[i];
			uint32_t preserved_gen = slot->generation; // [FIX] Preserve generation
            memset(slot, 0, sizeof(_SituationModelSlot));
            slot->is_active = true;
            slot->generation = preserved_gen + 1;      // [FIX] Increment preserved
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
        "_SitAllocModelSlot: model registry full (SITUATION_MAX_MODELS).");
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a model slot and releases associated resources.
 *
 * @param handle The SituationModel handle to free (invalid handles are ignored)
 *
 * @see _SitAllocModelSlot, SituationDestroyModel (public API)
 */
static void _SitFreeModelSlot(SituationModel handle) {
    /* HARDENING: void by design — idempotent slot release; invalid/stale handles ignored. */
    _SituationModelSlot* slot = _SitGetModelSlot(handle);
    if (!slot) return;
    if (slot->source_path) SIT_FREE(slot->source_path);
    // Note: Model data (meshes array) should be freed by SituationUnloadModel before calling this;
    slot->is_active = false;
}

/**
 * @brief Binds a texture to a specific descriptor set slot in a command buffer.
 *
 * @details Records a command that binds the given `SituationTexture` to the specified
 *          descriptor set index (`set_index`) in the active pipeline layout.
 *          The texture becomes available for sampling/storage in subsequent draw/dispatch
 *          commands within the same command buffer.
 *
 *          Equivalent to:
 *            - Vulkan: `vkCmdBindDescriptorSets` (with sampler + image view)
 *            - OpenGL: `glBindTextureUnit` or legacy `glActiveTexture` + `glBindTexture`
 *
 * @param cmd Valid recording command buffer handle
 * @param set_index Descriptor set index (0-based) in the current pipeline layout
 * @param texture Valid `SituationTexture` handle to bind
 *
 * @return SITUATION_SUCCESS on success,
 *         SITUATION_ERROR_INVALID_PARAM if cmd not recording or texture invalid,
 *         SITUATION_ERROR_RESOURCE_INVALID if texture not created/compatible,
 *         or other backend-specific errors
 *
 * @note Must be called after binding the pipeline that uses the layout containing set_index.
 *       Thread-safe if cmd is thread-owned; otherwise use render thread submission.
 *
 * @see SituationCmdBindPipeline, SituationCreateTexture, SituationTexture
 */
SITAPI SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture) {
    // This function was already correctly named, so it's a simple wrapper.
    return SituationCmdBindTextureSet(cmd, set_index, texture);
}

/**
 * @brief Creates a GPU texture from a CPU-side image with full control over usage flags.
 *
 * @details The extended version of `SituationCreateTexture`, allowing the caller to explicitly
 *          specify the `SituationTextureUsageFlags` bitmask that defines how the texture will
 *          be used during its lifetime. This is essential for performance and correctness when
 *          the texture will be used in compute shaders, render targets, transfer operations,
 *          or other non-default scenarios.
 *
 *          Core behavior:
 *            - Uploads the base-level pixel data from the `SituationImage` to GPU memory
 *            - Optionally generates a full mipmap chain if `generate_mipmaps` is true
 *            - Creates the texture with exactly the requested usage flags (Vulkan: usage bits;
 *              OpenGL: inferred from usage to set appropriate storage/texture parameters)
 *            - Returns a `SituationTexture` handle ready for binding/sampling
 *
 *          Required usage flags (caller responsibility):
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_DST` - almost always needed for initial upload
 *            - `SITUATION_TEXTURE_USAGE_SAMPLED` - if the texture will be sampled in shaders
 *            - `SITUATION_TEXTURE_USAGE_STORAGE` - if used as storage image in compute
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` - required when `generate_mipmaps` is true
 *              (for internal blit operations during mipmap generation)
 *            - `SITUATION_TEXTURE_USAGE_COLOR_ATTACHMENT` - if used as render target
 *            - `SITUATION_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT` - for depth/stencil textures
 *
 *          Invalid or insufficient flags result in error (e.g. mipmaps requested without TRANSFER_SRC).
 *
 * @param image Valid `SituationImage` handle with source pixel data.
 *              Dimensions, format, and channels must be compatible with texture creation.
 * @param generate_mipmaps If true, generates a complete mipmap pyramid after base-level upload.
 *                         Requires `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` in `usage_flags`.
 * @param usage_flags Bitmask of `SituationTextureUsageFlags` values (OR-ed) specifying all
 *                    intended usages of the texture. Must include at least TRANSFER_DST for upload.
 * @param out_texture Pointer to a `SituationTexture` variable that receives the new handle on success.
 *                    On failure, set to `SITUATION_NULL_TEXTURE`.
 *
 * @return SITUATION_SUCCESS on successful creation and upload,
 *         SITUATION_ERROR_INVALID_PARAM if image invalid, flags inconsistent, or out_texture NULL,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format unsupported or mipmaps requested
 *         without TRANSFER_SRC,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if GPU memory allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL texture creation/upload/blit failed,
 *         or other appropriate error codes.
 *
 * @note This is the low-level, flexible entry point - use `SituationCreateTexture` for the
 *       common case with automatic/default flags.
 *
 *       Performance considerations:
 *         - Mipmap generation uses hardware blit (fast on modern GPUs) but still adds cost
 *         - Including unnecessary flags may increase memory usage or prevent optimal tiling
 *         - Upload is synchronous from caller perspective; actual GPU work may be queued
 *
 *       Thread safety:
 *         - Safe from the main thread between frames (uses loader shared context when
 *           `render_thread_count > 0`; see `_SituationMakeGLContextCurrentForHostThread`)
 *         - Do not call from worker threads or during active command recording that
 *           uses the texture on the same frame without synchronization
 *
 *       Caller must destroy the texture with `SituationDestroyTexture` when done.
 *
 * @see SituationCreateTexture (convenience wrapper), SituationCreateImage,
 *      SituationDestroyTexture, SituationSetTextureSamplerParams,
 *      SituationTextureUsageFlags, SITUATION_TEXTURE_USAGE_xxx constants,
 *      SITUATION_NULL_TEXTURE, SITUATION_ERROR_RESOURCE_INVALID
 */
SITAPI SituationError SituationCreateTextureEx(SituationImage image, bool generate_mipmaps, SituationTextureUsageFlags usage_flags, SituationTexture* out_texture) {
    if (out_texture) *out_texture = (SituationTexture){0};

    if (!SituationIsImageValid(image)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Cannot create texture from invalid image.");
    }
    if (!out_texture) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_texture cannot be NULL.");
    }

    // 1. Find Free Slot
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    int slot_idx = -1;
    for (int i = 0; i < SITUATION_MAX_TEXTURES; ++i) {
        if (!sit_render.texture_registry[i].is_active) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx == -1) {
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Max texture limit reached (SITUATION_MAX_TEXTURES).");
    }

    _SituationTextureSlot* slot = &sit_render.texture_registry[slot_idx];

    // 2. Prepare Slot (Increment generation to invalidate old handles to this slot)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot %d: generation before=%u\n", slot_idx, slot->generation);
    fflush(stdout);
    #endif
    slot->generation++;
    if (slot->generation == 0) slot->generation = 1; // Wrap-around safety
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot %d: generation after=%u\n", slot_idx, slot->generation);
    fflush(stdout);
    #endif
    slot->is_active = true;
    mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    slot->width = image.width;
    slot->height = image.height;
    slot->mip_levels = generate_mipmaps ? (int)floor(log2(fmax(image.width, image.height))) + 1 : 1;
    slot->format_api = (image.color_encoding == SITUATION_COLOR_SRGB) ? SIT_TEXTURE_FORMAT_RGBA8_SRGB : SIT_TEXTURE_FORMAT_RGBA8_UNORM;
    slot->usage_flags = usage_flags;
    slot->layout_hint = SITUATION_TEXTURE_LAYOUT_UNDEFINED;
    slot->wrap_s = SIT_TEXTURE_WRAP_REPEAT;
    slot->wrap_t = SIT_TEXTURE_WRAP_REPEAT;
    slot->bindless_handle = 0;

#if defined(SITUATION_USE_OPENGL)
    bool gl_upload_failed = false;
    slot->gl_image_binding_unit = -1;
    _SituationMakeGLContextCurrentForHostThread();
    sit_gs.last_error_code = SITUATION_SUCCESS;
    glCreateTextures(GL_TEXTURE_2D, 1, &slot->gl_texture_id);
    SIT_CHECK_GL_ERROR();
    if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;

    // --- Select OpenGL Format Based on Color Encoding ---
    GLenum gl_internal_format;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        // Storage images MUST use LINEAR format
        gl_internal_format = GL_RGBA8;
    } else {
        // Use the image's color encoding preference
        gl_internal_format = (image.color_encoding == SITUATION_COLOR_SRGB)
                            ? GL_SRGB8_ALPHA8
                            : GL_RGBA8;
    }

    // Store format in slot
    slot->internal_format = gl_internal_format;


    // If the texture ID is 0 here, it means the context is likely invalid.
    if (slot->gl_texture_id == 0) {
        gl_upload_failed = true;
    }

    int levels = 1;
    if (generate_mipmaps) {
        levels = (int)floor(log2(fmax(image.width, image.height))) + 1;
    }

    // Allocate immutable storage. This can fail if texture is too large.
    if (!gl_upload_failed) {
        glTextureStorage2D(slot->gl_texture_id, levels, gl_internal_format, image.width, image.height);
        SIT_CHECK_GL_ERROR();
        if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;
    }

    // Upload the base level pixel data.
    if (!gl_upload_failed) {
        glTextureSubImage2D(slot->gl_texture_id, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
        SIT_CHECK_GL_ERROR();
        if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;
    }

    if (!gl_upload_failed && generate_mipmaps) {
        glGenerateTextureMipmap(slot->gl_texture_id);
        SIT_CHECK_GL_ERROR();
        if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;
    }

    // Set texture parameters.
    if (!gl_upload_failed) {
        glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (generate_mipmaps) {
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            slot->min_filter = SIT_TEXTURE_FILTER_LINEAR;
            slot->mag_filter = SIT_TEXTURE_FILTER_LINEAR;
        } else {
            /* No mips: NEAREST preserves texel values for UI / readback-style draws (harness parity). */
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
            slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
        }
        SIT_CHECK_GL_ERROR();
        if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;
    }

    // [Phase 3] Bindless Texture: Make Resident immediately.
    // [Bug Fix] Storage images MUST NOT be made bindless-resident. The OpenGL spec
    // forbids a texture from being simultaneously resident as a bindless handle AND
    // bound to an image unit (via glBindImageTexture). If a storage texture is made
    // resident here, glBindImageTexture silently no-ops on the affected drivers, so
    // the compute shader never writes to it — producing black pixels on readback.
    // Only make non-storage textures resident via ARB_bindless_texture.
    if (!gl_upload_failed &&
        SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES) &&
        !(usage_flags & SITUATION_TEXTURE_USAGE_STORAGE)) {
        #if defined(GLAD_GL_ARB_bindless_texture)
        if (GLAD_GL_ARB_bindless_texture) {
            slot->gl_bindless_handle = glGetTextureHandleARB(slot->gl_texture_id);
            if (slot->gl_bindless_handle) {
                glMakeTextureHandleResidentARB(slot->gl_bindless_handle);
                SIT_CHECK_GL_ERROR();
                if (sit_gs.last_error_code != SITUATION_SUCCESS) gl_upload_failed = true;
            }
        }
        #endif
    }

    if (gl_upload_failed) {
        if (slot->gl_texture_id) {
            glDeleteTextures(1, &slot->gl_texture_id);
            slot->gl_texture_id = 0;
        }
        slot->is_active = false;
        _SituationReleaseHostGLContextIfInFrame();
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_UPLOAD_FAILED, "OpenGL texture upload failed.");
    }

    out_texture->slot_index = slot_idx;
    out_texture->generation = slot->generation;
    out_texture->width = slot->width;
    out_texture->height = slot->height;

    _SituationReleaseHostGLContextIfInFrame();

#elif defined(SITUATION_USE_VULKAN)
    // --- Step 0: Calculate Mipmap Levels ---
    uint32_t mip_levels = slot->mip_levels;

    VkDeviceSize image_size = (VkDeviceSize)image.width * image.height * 4;
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;

    // Check if we can use async transfer (inside frame)
    // Only use main command buffer if we're actively recording a frame
    VkCommandBuffer cmd = (sit_render.in_frame)
        ? (VkCommandBuffer)SituationGetMainCommandBuffer()
        : VK_NULL_HANDLE;

    if (_SituationVulkanCreateAndUploadBuffer(cmd, image.data, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging_buffer, &staging_allocation) != SITUATION_SUCCESS) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // FIX: Map abstract flags to Vulkan flags
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_DST) vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_SRC) vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Storage textures need this to upload initial data
    }

    // --- Step 1: Select Format Based on Color Encoding ---
    VkFormat vk_format;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        // Storage images MUST use LINEAR format (SRGB doesn't support storage on most GPUs)
        vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    } else {
        // Use the image's color encoding preference
        vk_format = (image.color_encoding == SITUATION_COLOR_SRGB)
                    ? VK_FORMAT_R8G8B8A8_SRGB
                    : VK_FORMAT_R8G8B8A8_UNORM;
    }

    // Store format in slot for later use
    slot->format = vk_format;

    if (_SituationVulkanCreateImage(image.width, image.height, mip_levels, vk_format, VK_IMAGE_TILING_OPTIMAL,
                                  vk_usage, VMA_MEMORY_USAGE_GPU_ONLY, VK_SAMPLE_COUNT_1_BIT,
                                  &slot->image, &slot->allocation) != SITUATION_SUCCESS) {
        if (cmd == VK_NULL_HANDLE) vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        else _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // --- Step 3: Copy and Generate Mipmaps ---
    VkCommandBuffer command_buffer = (cmd != VK_NULL_HANDLE) ? cmd : _SituationVulkanBeginSingleTimeCommands();
    // If async, we assume _SituationVulkanCreateAndUploadBuffer already put the staging buffer in a state ready for transfer (it does, it doesn't transition it, but buffer is host visible).
    // But we need to transition the *image* to TransferDst.

    // a. Transition the entire image (all mip levels) to be ready for writing.
    // For async, we need a pipeline barrier to ensure the layout transition happens before the copy.
    _SituationVulkanTransitionImageLayout(command_buffer, slot->image, mip_levels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // b. Copy the staging buffer to the first mip level (level 0).
    _SituationVulkanCopyBufferToImage(command_buffer, staging_buffer, slot->image, (uint32_t)image.width, (uint32_t)image.height);

    // c. Generate the mipmaps by blitting from one level to the next.
    if (generate_mipmaps) {
        _SituationVulkanGenerateMipmaps(command_buffer, slot->image, image.width, image.height, mip_levels);
    } else {
        // Pure storage (imageLoad/imageStore) uses GENERAL. Textures that are also SAMPLED
        // (default SituationCreateTexture) go through the bindless path and must end in
        // SHADER_READ_ONLY_OPTIMAL to match global_textures[] descriptor layout.
        VkImageLayout target_layout = ((usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
                                      && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u)
            ? VK_IMAGE_LAYOUT_GENERAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        _SituationVulkanTransitionImageLayout(command_buffer, slot->image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, target_layout);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Vulkan] Transitioned texture to layout: %s\n",
                target_layout == VK_IMAGE_LAYOUT_GENERAL ? "GENERAL" : "SHADER_READ_ONLY");
#endif
    }

    if (cmd == VK_NULL_HANDLE) {
        SituationError end_err = _SituationVulkanEndSingleTimeCommands(command_buffer);
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        if (end_err != SITUATION_SUCCESS) {
            _SituationDeferDestroyImage(slot->image, slot->allocation, VK_NULL_HANDLE, VK_NULL_HANDLE);
            slot->is_active = false;
            return end_err;
        }
    } else {
        // --- Step 4: Defer Cleanup Staging Buffer (Asynchronous) ---
        _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
    }

    // --- Step 5: Create Image View and Sampler ---
    // The image view must now be aware of all the mip levels.
    slot->image_view = _SituationVulkanCreateImageView(slot->image, slot->format, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;  // Pixel-perfect for bitmap fonts
    sampler_info.minFilter = VK_FILTER_NEAREST;  // No blurring when scaling
    slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
    slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    // CRITICAL: Set the mipmap mode and LOD bias for the sampler.
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = (float)mip_levels; // Use all available mip levels

    if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &slot->sampler) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "vkCreateSampler failed in SituationCreateTextureEx.");
        _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, VK_NULL_HANDLE);
        slot->is_active = false;
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

	// --- Step 6: Create and Cache the Persistent Descriptor Set [PATCH 1] ---
    VkDescriptorType descriptor_type;
    VkDescriptorSetLayout layout_to_use;

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[SituationCreateTextureEx] Selecting layout: usage_flags=0x%x, STORAGE=%d, COMPUTE_SAMPLED=%d\n",
            usage_flags,
            !!(usage_flags & SITUATION_TEXTURE_USAGE_STORAGE),
            !!(usage_flags & SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED));
#endif

    /* Storage-only images use a dedicated storage descriptor set. If SAMPLED is also set,
       use bindless COMBINED_IMAGE_SAMPLER so fragment shaders (internal quad, VD compositor)
       see vkUpdateDescriptorSets on global_bindless_set — STORAGE alone skipped that write. */
    if ((usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
        && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u) {
        layout_to_use = sit_render.vk.storage_image_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using STORAGE layout\n");
#endif
    } else if (usage_flags & SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED) {
        // Textures that will be sampled in compute shaders need binding 0
        layout_to_use = sit_render.vk.compute_sampler_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using COMPUTE_SAMPLER layout\n");
#endif
    } else {
        // Regular graphics pipeline textures use bindless layout (binding 0)
        layout_to_use = sit_render.vk.bindless_descriptor_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using BINDLESS layout\n");
#endif
    }

    // [FIX v2.3.27B] Capture the pool
    VkDescriptorPool used_pool = VK_NULL_HANDLE;

    // [Bindless] Use Global Descriptor Set for standard textures
    if (layout_to_use == sit_render.vk.bindless_descriptor_layout) {
        // We do NOT allocate a new set. We write to the global set.
        slot->descriptor_set = VK_NULL_HANDLE; // Bindless textures don't own a set
        slot->descriptor_pool = VK_NULL_HANDLE;

        VkDescriptorImageInfo bindless_image_info = {};
        bindless_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindless_image_info.imageView = slot->image_view;
        bindless_image_info.sampler = slot->sampler;

        VkWriteDescriptorSet bindless_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        bindless_write.dstSet = sit_render.vk.global_bindless_set;
        bindless_write.dstBinding = 0;
        bindless_write.dstArrayElement = (uint32_t)slot_idx; // Use the slot index!
        bindless_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindless_write.descriptorCount = 1;
        bindless_write.pImageInfo = &bindless_image_info;

        mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &bindless_write, 0, NULL);
        /* LoadShaderFromMemory / harness: pipeline set 1 is text_sampler_layout (binding 0), not the bindless array. */
        slot->single_sampler_descriptor_set = _SituationVulkanAllocateDescriptorSet(sit_render.vk.text_sampler_layout, &slot->single_sampler_descriptor_pool);
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            VkWriteDescriptorSet sw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            sw.dstSet = slot->single_sampler_descriptor_set;
            sw.dstBinding = SIT_SAMPLER_BINDING_ALBEDO;
            sw.dstArrayElement = 0;
            sw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sw.descriptorCount = 1;
            sw.pImageInfo = &bindless_image_info;
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &sw, 0, NULL);
        }
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    } else {
        // Fallback for Storage/Compute layouts (until they are bindless-ready)
        slot->descriptor_set = _SituationVulkanAllocateDescriptorSet(layout_to_use, &used_pool);
        slot->descriptor_pool = used_pool; // Assign pool for proper cleanup

        if (slot->descriptor_set == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate persistent descriptor set for texture.");
            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
            return SITUATION_ERROR_UNKNOWN_ERROR;
        }
    }

/*    // Use the Asset Pool
    VkDescriptorSetAllocateInfo asset_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = sit_render.vk.asset_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout_to_use
    };
    vkAllocateDescriptorSets(sit_render.vk.device, &asset_alloc_info, &tex_slot->descriptor_set);

    if (slot->descriptor_set == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate persistent descriptor set for texture.");

        // CRITICAL: Defer cleanup to Graveyard. Immediate destruction is unsafe if async upload commands are pending.
        _SituationDeferDestroyImage(slot->image, texture.allocation, slot->image_view, texture.sampler);

        // Note: Staging buffer (if used) was already deferred to graveyard or destroyed synchronously above.
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }*/

    VkDescriptorImageInfo image_info = {};
    // The layout for storage images is different. It's often GENERAL or TRANSFER_DST_OPTIMAL before the compute shader runs, and the shader itself might transition it.
    // For simplicity, let's assume it should be in GENERAL layout for read/write access.
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else {
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    image_info.imageView = slot->image_view;
    image_info.sampler = (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? slot->sampler : VK_NULL_HANDLE;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = slot->descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = descriptor_type; // Use the chosen type
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    if (slot->descriptor_set != VK_NULL_HANDLE) {
        mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
        /* SituationCmdDrawTexture binds global_bindless_set only (internal quad FS).
           Storage-only textures skip the bindless branch above — mirror into global_bindless_set
           so textured draws sample the same image (descriptor layout = GENERAL, matching slot layout). */
        if (sit_render.vk.global_bindless_set != VK_NULL_HANDLE
            && (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
            && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u) {
            VkDescriptorImageInfo bindless_mirror = {};
            bindless_mirror.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            bindless_mirror.imageView = slot->image_view;
            bindless_mirror.sampler = slot->sampler;
            VkWriteDescriptorSet bw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            bw.dstSet = sit_render.vk.global_bindless_set;
            bw.dstBinding = 0;
            bw.dstArrayElement = (uint32_t)slot_idx;
            bw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bw.descriptorCount = 1;
            bw.pImageInfo = &bindless_mirror;
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &bw, 0, NULL);
        }
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    }

    /* Per-texture single_sampler still used by text, YPQ grade, and SituationCmdBindTextureSet (D2 slice 2 / D4). */
    if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE
        && slot->single_sampler_descriptor_set == VK_NULL_HANDLE
        && slot->image_view != VK_NULL_HANDLE
        && slot->sampler != VK_NULL_HANDLE) {
        slot->single_sampler_descriptor_set = _SituationVulkanAllocateDescriptorSet(
            sit_render.vk.text_sampler_layout, &slot->single_sampler_descriptor_pool);
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            VkDescriptorImageInfo sample_info = {};
            sample_info.imageLayout = (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE)
                ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            sample_info.imageView = slot->image_view;
            sample_info.sampler = slot->sampler;
            VkWriteDescriptorSet sw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            sw.dstSet = slot->single_sampler_descriptor_set;
            sw.dstBinding = SIT_SAMPLER_BINDING_ALBEDO;
            sw.dstArrayElement = 0;
            sw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sw.descriptorCount = 1;
            sw.pImageInfo = &sample_info;
            mtx_lock(&sit_render.resource_registry_mutex);
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &sw, 0, NULL);
            mtx_unlock(&sit_render.resource_registry_mutex);
        }
    }

    // --- Final: Set Output Texture Handle ---
    out_texture->slot_index = slot_idx;
    out_texture->generation = slot->generation;
    out_texture->width = slot->width;
    out_texture->height = slot->height;
#endif

    return SITUATION_SUCCESS;
}

/**
 * @brief Creates a GPU texture from an existing CPU-side image, with optional mipmap generation.
 *
 * @details Convenience wrapper around `SituationCreateTextureEx` that automatically computes
 *          appropriate `SituationTextureUsageFlags` based on the requested mipmap generation.
 *
 *          Default usage flags always include:
 *            - `SITUATION_TEXTURE_USAGE_SAMPLED` (for texture sampling in shaders)
 *            - `SITUATION_TEXTURE_USAGE_STORAGE` (for potential compute/storage access)
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_DST` (required for upload from CPU/image)
 *
 *          If `generate_mipmaps` is true, also adds:
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` (needed for internal blit operations
 *              during mipmap chain generation)
 *
 *          The function performs the full GPU upload of the base image level and, if requested,
 *          generates the complete mipmap pyramid using hardware-accelerated blit commands
 *          (glGenerateMipmap on OpenGL, vkCmdBlitImage on Vulkan).
 *
 *          After success, the resulting texture is immediately usable for sampling or binding.
 *          The source `SituationImage` can be destroyed afterward if no longer needed on CPU.
 *
 * @param image Valid `SituationImage` handle containing the source pixel data.
 *              Must have valid dimensions, channels, and format compatible with texture creation.
 * @param generate_mipmaps If true, generates a full mipmap chain (recommended for most textures
 *                         that will be minified). If false, only the base level is uploaded.
 * @param out_texture Pointer to a `SituationTexture` variable that receives the new texture handle
 *                    on success. On failure, set to `SITUATION_NULL_TEXTURE`.
 *
 * @return SITUATION_SUCCESS on successful upload and texture creation,
 *         SITUATION_ERROR_INVALID_PARAM if image is invalid or out_texture is NULL,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format/channels are unsupported,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if GPU memory allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL texture creation/upload failed,
 *         or other appropriate error codes propagated from `SituationCreateTextureEx`.
 *
 * @note This is the most commonly used texture creation entry point  -  it provides sensible
 *       defaults while still allowing full control via the `Ex` variant when needed.
 *       Mipmap generation adds a small GPU cost (especially for large textures) but greatly
 *       improves quality when minification occurs.
 *
 *       Thread safety:
 *       - Safe to call from the main thread or any thread that does not own the render context
 *       - Internal synchronization ensures safe concurrent creation
 *       - Actual GPU upload may be deferred to render thread or staging queue
 *
 *       Caller is responsible for destroying the texture with `SituationDestroyTexture`.
 *
 * @see SituationCreateTextureEx, SituationCreateImage, SituationCreateImageFromMemory,
 *      SituationDestroyTexture, SituationSetTextureSamplerParams,
 *      SITUATION_TEXTURE_USAGE_SAMPLED, SITUATION_TEXTURE_USAGE_STORAGE,
 *      SITUATION_TEXTURE_USAGE_TRANSFER_DST, SITUATION_TEXTURE_USAGE_TRANSFER_SRC,
 *      SITUATION_NULL_TEXTURE
 */
SITAPI SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture) {
    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE | SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    if (generate_mipmaps) flags = (SituationTextureUsageFlags)(flags | SITUATION_TEXTURE_USAGE_TRANSFER_SRC);
    return SituationCreateTextureEx(image, generate_mipmaps, flags, out_texture);
}

/**
 * @brief Destroys a GPU texture and frees its associated resources.
 *
 * @details Cleans up all backend-specific resources associated with the texture (e.g., OpenGL texture name, Vulkan image, view, sampler, VMA allocation, cached descriptor set). The texture handle is invalidated after this call.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDeleteTextures` to delete the texture name. The OpenGL driver manages the underlying memory and GPU resource lifecycle.
 * - **Vulkan:** Waits for the device to be idle to ensure the texture is no longer in use. Then, it destroys the `VkSampler`, `VkImageView`, and uses `vmaDestroyImage` for the `VkImage` and `VmaAllocation`.
 *   Crucially, if a persistent descriptor set was allocated for this texture during creation, it is also freed back to the dedicated persistent descriptor pool.
 *   This function uses the internal `_SituationVulkanDestroyTexture` helper if available for centralized Vulkan cleanup logic.
 *
 * @param[in,out] texture A pointer to the `SituationTexture` handle to destroy.
 *                        The `texture->id` field will be set to 0 upon successful destruction. The contents of the struct pointed to by `texture` will be zeroed.
 *
 * @note It's safe to call this function on an already destroyed or invalid texture (where `texture->id` is 0); it will simply do nothing.
 * @note This function internally removes the texture from the library's resource tracking list.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationDestroyTexture(SituationTexture* texture) {
    if (!texture || texture->generation == 0) return;

    _SituationTextureSlot* slot = _SitGetTextureSlot(*texture);
    if (!slot) {
        texture->generation = 0;
        return;
    }

    if (slot->source_path) {
        SIT_FREE(slot->source_path);
        slot->source_path = NULL;
    }

#if defined(SITUATION_USE_OPENGL)
    _SitGLDeferDestroyTexture(slot->gl_texture_id);
    // Erase from Virtual Bindless Cache to prevent ID-recycle collisions
    for (int i = 0; i < SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        if (sit_render.gl.virtual_texture_slots[i].gl_texture_id == slot->gl_texture_id) {
            sit_render.gl.virtual_texture_slots[i].is_active = false;
            sit_render.gl.virtual_texture_slots[i].gl_texture_id = 0;
        }
    }
    slot->gl_texture_id = 0;
#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(sit_render.vk.device, slot->sampler, NULL);
            slot->sampler = VK_NULL_HANDLE;
        }
        if (slot->image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(sit_render.vk.device, slot->image_view, NULL);
            slot->image_view = VK_NULL_HANDLE;
        }
        if (slot->image != VK_NULL_HANDLE) {
            vmaDestroyImage(sit_render.vk.vma_allocator, slot->image, slot->allocation);
            slot->image = VK_NULL_HANDLE;
            slot->allocation = VK_NULL_HANDLE;
        }
        if (slot->descriptor_set != VK_NULL_HANDLE && slot->descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->descriptor_pool, 1, &slot->descriptor_set);
            slot->descriptor_set = VK_NULL_HANDLE;
        }
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE && slot->single_sampler_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->single_sampler_descriptor_pool, 1, &slot->single_sampler_descriptor_set);
            slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
        if (slot->descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->descriptor_set, slot->descriptor_pool);
        }
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->single_sampler_descriptor_set, slot->single_sampler_descriptor_pool);
            slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
        }
    }
#endif

    slot->is_active = false;
    texture->generation = 0;
}

SITAPI SituationError SituationGetTextureInfo(SituationTexture texture, SituationTextureInfo* out_info) {
    if (!out_info) return SITUATION_ERROR_INVALID_PARAM;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot || !slot->is_active) return SITUATION_ERROR_RESOURCE_INVALID;

    out_info->width = slot->width;
    out_info->height = slot->height;
    out_info->mip_levels = slot->mip_levels;
    out_info->format = slot->format_api;
    out_info->usage_flags = slot->usage_flags;
    out_info->min_filter = slot->min_filter;
    out_info->mag_filter = slot->mag_filter;
    out_info->wrap_s = slot->wrap_s;
    out_info->wrap_t = slot->wrap_t;

    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationSetTextureSamplerParams(SituationTexture texture, SituationTextureFilter min_filter, SituationTextureFilter mag_filter, SituationTextureWrap wrap_s, SituationTextureWrap wrap_t) {
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot || !slot->is_active) return SITUATION_ERROR_RESOURCE_INVALID;

    slot->min_filter = min_filter;
    slot->mag_filter = mag_filter;
    slot->wrap_s = wrap_s;
    slot->wrap_t = wrap_t;

#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    GLint gl_min = (min_filter == SIT_TEXTURE_FILTER_LINEAR) ? ((slot->mip_levels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) : GL_NEAREST;
    GLint gl_mag = (mag_filter == SIT_TEXTURE_FILTER_LINEAR) ? GL_LINEAR : GL_NEAREST;
    GLint gl_wrap_s = (wrap_s == SIT_TEXTURE_WRAP_REPEAT) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    GLint gl_wrap_t = (wrap_t == SIT_TEXTURE_WRAP_REPEAT) ? GL_REPEAT : GL_CLAMP_TO_EDGE;

    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, gl_min);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, gl_mag);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_S, gl_wrap_s);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_T, gl_wrap_t);
    SIT_CHECK_GL_ERROR();
    _SituationReleaseHostGLContextIfInFrame();
#elif defined(SITUATION_USE_VULKAN)
    // Destroy the old sampler
    if (slot->sampler != VK_NULL_HANDLE) {
        _SituationDeferDestroyImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, slot->sampler); // Queue for cleanup
    }

    // Create a new sampler
    VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.minFilter = (min_filter == SIT_TEXTURE_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler_info.magFilter = (mag_filter == SIT_TEXTURE_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler_info.addressModeU = (wrap_s == SIT_TEXTURE_WRAP_REPEAT) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = (wrap_t == SIT_TEXTURE_WRAP_REPEAT) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = sampler_info.addressModeU;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = (min_filter == SIT_TEXTURE_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = (float)slot->mip_levels;

    if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &slot->sampler) != VK_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "vkCreateSampler failed.");
    }

    // Update descriptor sets
    VkDescriptorImageInfo image_info = {};
    if (slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else {
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    image_info.imageView = slot->image_view;
    image_info.sampler = slot->sampler;

    VkWriteDescriptorSet writes[2] = {0};
    int write_count = 0;

    if (slot->descriptor_set == VK_NULL_HANDLE && sit_render.vk.global_bindless_set != VK_NULL_HANDLE) {
        // It's in the bindless set!
        writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[write_count].dstSet = sit_render.vk.global_bindless_set;
        writes[write_count].dstBinding = 0;
        writes[write_count].dstArrayElement = (uint32_t)texture.slot_index;
        writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[write_count].descriptorCount = 1;
        writes[write_count].pImageInfo = &image_info;
        write_count++;
    } else if (slot->descriptor_set != VK_NULL_HANDLE) {
        writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[write_count].dstSet = slot->descriptor_set;
        writes[write_count].dstBinding = 0;
        writes[write_count].dstArrayElement = 0;
        writes[write_count].descriptorType = ((slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) && !(slot->usage_flags & SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED)) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[write_count].descriptorCount = 1;
        writes[write_count].pImageInfo = &image_info;
        write_count++;
    }

    if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
        writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[write_count].dstSet = slot->single_sampler_descriptor_set;
        writes[write_count].dstBinding = SIT_SAMPLER_BINDING_ALBEDO; // Usually 0
        writes[write_count].dstArrayElement = 0;
        writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[write_count].descriptorCount = 1;
        writes[write_count].pImageInfo = &image_info;
        write_count++;
    }

    if (write_count > 0) {
        mtx_lock(&sit_render.resource_registry_mutex);
        vkUpdateDescriptorSets(sit_render.vk.device, write_count, writes, 0, NULL);
        mtx_unlock(&sit_render.resource_registry_mutex);
    }
#endif

    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationReadTexture(SituationTexture texture, const SituationTextureReadbackDesc* desc, void* dst_pixels, size_t dst_size_bytes) {
	_SituationFlushRenderThread();
    if (!dst_pixels) return SITUATION_ERROR_INVALID_PARAM;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot || !slot->is_active) return SITUATION_ERROR_RESOURCE_INVALID;

    SituationTextureReadbackDesc local_desc = {0};
    if (!desc) {
        local_desc.region.width = slot->width;
        local_desc.region.height = slot->height;
        local_desc.region.mip_level = 0;
        local_desc.format = SIT_TEXTURE_READ_RGBA8;
        local_desc.dst_row_pitch_bytes = slot->width * 4;
        desc = &local_desc;
    }

    if (desc->format != SIT_TEXTURE_READ_RGBA8) return SITUATION_ERROR_INVALID_PARAM;

    int read_w = desc->region.width ? desc->region.width : slot->width;
    int read_h = desc->region.height ? desc->region.height : slot->height;
    size_t pitch = desc->dst_row_pitch_bytes ? desc->dst_row_pitch_bytes : (size_t)(read_w * 4);

    if (dst_size_bytes < pitch * read_h) return SITUATION_ERROR_BUFFER_OVERFLOW;

#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(pitch / 4));
    glGetTextureSubImage(slot->gl_texture_id, desc->region.mip_level, desc->region.x, desc->region.y, 0, read_w, read_h, 1, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)dst_size_bytes, dst_pixels);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0); // Restore
    SIT_CHECK_GL_ERROR();
    _SituationReleaseHostGLContextIfInFrame();
#elif defined(SITUATION_USE_VULKAN)
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VkDeviceSize size = pitch * read_h;

    VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    
    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        old_layout = VK_IMAGE_LAYOUT_GENERAL;
    } else {
        switch (slot->layout_hint) {
            case SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT:
                old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;
            case SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC:
                old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                break;
            case SITUATION_TEXTURE_LAYOUT_TRANSFER_DST:
                old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                break;
            default:
                break;
        }
    }
    _SituationVulkanTransitionImageLayout(cmd, slot->image, slot->mip_levels, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = (uint32_t)(pitch / 4);
    region.bufferImageHeight = read_h;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = desc->region.mip_level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){desc->region.x, desc->region.y, 0};
    region.imageExtent = (VkExtent3D){(uint32_t)read_w, (uint32_t)read_h, 1};

    vkCmdCopyImageToBuffer(cmd, slot->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer, 1, &region);

    _SituationVulkanTransitionImageLayout(cmd, slot->image, slot->mip_levels, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);
    SIT_RETURN_IF_ERR(_SituationVulkanEndSingleTimeCommands(cmd));

    void* mapped = NULL;
    vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &mapped);
    memcpy(dst_pixels, mapped, size);
    vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);
    vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
#endif

    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationReadTextureAlloc(SituationTexture texture, const SituationTextureReadbackDesc* desc, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot || !slot->is_active) return SITUATION_ERROR_RESOURCE_INVALID;

    SituationTextureReadbackDesc local_desc = {0};
    if (desc) {
        local_desc = *desc;
    } else {
        local_desc.region.width = slot->width;
        local_desc.region.height = slot->height;
        local_desc.region.mip_level = 0;
        local_desc.format = SIT_TEXTURE_READ_RGBA8;
    }

    int read_w = local_desc.region.width ? local_desc.region.width : slot->width;
    int read_h = local_desc.region.height ? local_desc.region.height : slot->height;
    local_desc.dst_row_pitch_bytes = read_w * 4;

    size_t required_bytes = local_desc.dst_row_pitch_bytes * read_h;
    void* pixels = SIT_MALLOC(required_bytes);

    SituationError err = SituationReadTexture(texture, &local_desc, pixels, required_bytes);
    if (err != SITUATION_SUCCESS) {
        SIT_FREE(pixels);
        return err;
    }

    out_image->width = read_w;
    out_image->height = read_h;
    out_image->data = (uint8_t*)pixels;
    out_image->color_encoding = SITUATION_COLOR_SRGB;
    
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationReadFramebuffer(const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes) {
	_SituationFlushRenderThread();

    if (!dst_pixels) return SITUATION_ERROR_INVALID_PARAM;

    SituationReadPixelsDesc default_desc = {0};
    if (!desc) {
        default_desc.width = sit_gs.main_window_width;
        default_desc.height = sit_gs.main_window_height;
        default_desc.format = SIT_TEXTURE_READ_RGBA8;
        default_desc.dst_row_pitch_bytes = sit_gs.main_window_width * 4;
        desc = &default_desc;
    }

    int read_w = desc->width ? desc->width : sit_gs.main_window_width;
    int read_h = desc->height ? desc->height : sit_gs.main_window_height;
    size_t pitch = desc->dst_row_pitch_bytes ? desc->dst_row_pitch_bytes : (size_t)(read_w * 4);

    if (dst_size_bytes < pitch * read_h) return SITUATION_ERROR_BUFFER_OVERFLOW;

    const bool read_rgb10_packed = (desc->format == SIT_TEXTURE_READ_RGB10_PACKED);

#if defined(SITUATION_USE_OPENGL)
    if (read_rgb10_packed) {
        return SITUATION_ERROR_NOT_IMPLEMENTED;
    }
    glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(pitch / 4));
    glReadPixels(desc->x, desc->y, read_w, read_h, GL_RGBA, GL_UNSIGNED_BYTE, dst_pixels);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0); // Restore
    SIT_CHECK_GL_ERROR();
#elif defined(SITUATION_USE_VULKAN)
    if (read_rgb10_packed &&
        sit_render.vk.swapchain_image_format != VK_FORMAT_A2R10G10B10_UNORM_PACK32 &&
        sit_render.vk.swapchain_image_format != VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
        return SITUATION_ERROR_NOT_IMPLEMENTED;
    }
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VkDeviceSize size = pitch * read_h;

    VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    
    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
    VkImage swap_image = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];

    _SituationVulkanTransitionImageLayout(cmd, swap_image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = (uint32_t)(pitch / 4);
    region.bufferImageHeight = read_h;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){desc->x, desc->y, 0};
    region.imageExtent = (VkExtent3D){(uint32_t)read_w, (uint32_t)read_h, 1};

    vkCmdCopyImageToBuffer(cmd, swap_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer, 1, &region);

    _SituationVulkanTransitionImageLayout(cmd, swap_image, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    SituationError end_err = _SituationVulkanEndSingleTimeCommands(cmd);
    if (end_err != SITUATION_SUCCESS) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        return end_err;
    }

    void* mapped = NULL;
    vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &mapped);
    {
        const uint8_t* src_rows = (const uint8_t*)mapped;
        uint8_t* dst_rows = (uint8_t*)dst_pixels;
        VkFormat fmt = sit_render.vk.swapchain_image_format;
        for (int y = 0; y < read_h; y++) {
            if (read_rgb10_packed) {
                memcpy(dst_rows + (size_t)y * pitch, src_rows + (size_t)y * pitch, (size_t)read_w * 4u);
            } else {
                _SituationVulkanCopyMappedColorToRGBA(
                    dst_rows + (size_t)y * pitch,
                    src_rows + (size_t)y * pitch,
                    (size_t)read_w,
                    fmt);
            }
        }
    }
    vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);
    vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
#endif

    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationReadFramebufferHdr(const SituationReadPixelsDesc* desc, uint32_t* dst_pixels, size_t dst_size_bytes) {
    if (!dst_pixels) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!sit_render.output_hdr_active) {
        return SITUATION_ERROR_NOT_IMPLEMENTED;
    }

    SituationReadPixelsDesc local_desc = {0};
    if (desc) {
        local_desc = *desc;
    } else {
        local_desc.width = sit_gs.main_window_width;
        local_desc.height = sit_gs.main_window_height;
    }
    local_desc.format = SIT_TEXTURE_READ_RGB10_PACKED;
    if (!local_desc.dst_row_pitch_bytes) {
        const int w = local_desc.width ? local_desc.width : sit_gs.main_window_width;
        local_desc.dst_row_pitch_bytes = (size_t)w * 4u;
    }
    return SituationReadFramebuffer(&local_desc, dst_pixels, dst_size_bytes);
}

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Creates a device-local GPU buffer and uploads data to it, using an asynchronous path when possible.
 *
 * @details This is the core data upload utility for the Vulkan backend. It correctly handles the creation of high-performance, device-local buffers by using a temporary, host-visible "staging" buffer for the data transfer.
 *
 * @par Asynchronous Upload Path (The "Velocity" Solution)
 *   This function implements a dual-path mechanism to solve the "Synchronous Transfers" bottleneck:
 *   - **If `cmd` is a valid command buffer (not NULL):** This is the **asynchronous path**, used during the main render loop. The function records a `vkCmdCopyBuffer` command into the provided `cmd` and places the staging buffer into the graveyard for deferred deletion using `_SituationDeferDestroyBuffer`. This is a non-blocking operation that allows dozens of assets to be uploaded in a single frame without stalling the CPU.
 *   - **If `cmd` is NULL:** This is the **synchronous path**, used during initialization or outside the main render loop. The function creates its own temporary command buffer, submits the copy, and stalls the CPU by waiting for the transfer to complete (`vkQueueWaitIdle`). This is necessary when a frame is not in flight but is avoided at all costs during runtime.
 *
 * @param cmd The main command buffer for the current frame, or NULL to force a synchronous upload.
 * @param data Pointer to the data to upload.
 * @param size The size of the data in bytes.
 * @param usage The final usage flags for the destination buffer (e.g., `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`).
 * @param[out] out_buffer Pointer to store the handle of the final, device-local buffer.
 * @param[out] out_allocation Pointer to store the VMA allocation for the final buffer.
 *
 * @return `SITUATION_SUCCESS` on success.
 */
static SituationError _SituationVulkanCreateAndUploadBuffer(VkCommandBuffer cmd, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* out_buffer, VmaAllocation* out_allocation) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanCreateAndUploadBuffer called (size=%llu, data=%p, cmd=%p)\n", (unsigned long long)size, data, (void*)cmd); fflush(stdout);
    #endif
    // --- 1. Input Validation ---
    if (size == 0 || !out_buffer || !out_allocation) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: ERROR: Invalid parameters!\n"); fflush(stdout);
        #endif
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_buffer = VK_NULL_HANDLE;
    *out_allocation = VK_NULL_HANDLE;

    // If data is NULL, create an empty buffer without staging
    if (data == NULL) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;

        VmaAllocationCreateInfo alloc_info = {0};
        // CRITICAL: Use CPU_TO_GPU for uniform buffers and storage buffers so they can be
        // mapped/updated. GPU_ONLY is not mappable and will cause failures when we try to
        // update UBOs or read back SSBOs.
        if ((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
            alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        } else {
            alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }

        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, out_buffer, out_allocation, NULL) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create device-local buffer.");
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
        }
        return SITUATION_SUCCESS;
    }

    // --- 2. Create Staging Buffer ---
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = VK_NULL_HANDLE;

    VkBufferCreateInfo staging_buffer_info = {};
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.size = size;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {0};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &staging_buffer_info, &staging_alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create staging buffer.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }

    // --- 3. Upload Data to Staging Buffer ---
    void* mapped_data = NULL;
    if (vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &mapped_data) != VK_SUCCESS) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        return SITUATION_ERROR_BUFFER_MAP_FAILED;
    }
    memcpy(mapped_data, data, (size_t)size);
    vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);

    // --- 4. Create Final GPU-Local Buffer ---
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;

    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, out_buffer, out_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create device-local buffer.");
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }

    // --- 5. Copy Data ---
    VkBufferCopy copy_region = {};
    copy_region.size = size;

    if (cmd != VK_NULL_HANDLE) {
        // === ASYNCHRONOUS PATH ===
        // Use the provided command buffer. We must defer the destruction of the staging buffer
        // until the frame is done.

        // Barrier: Ensure copy happens before shader reads
        // Note: Only need barrier if we use it in the same frame, but robust to add.
        // Actually, standard practice is to barrier destination.
        VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = *out_buffer;
        barrier.offset = 0;
        barrier.size = size;

        vkCmdCopyBuffer(cmd, staging_buffer, *out_buffer, 1, &copy_region);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

        _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        return SITUATION_SUCCESS;

    } else {
        // === SYNCHRONOUS PATH (Legacy/Init) ===
        VkCommandBuffer temp_cmd = _SituationVulkanBeginSingleTimeCommands();
        if (temp_cmd == VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, *out_buffer, *out_allocation);
            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
            *out_buffer = VK_NULL_HANDLE;
            return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        vkCmdCopyBuffer(temp_cmd, staging_buffer, *out_buffer, 1, &copy_region);

        // Execute and Wait
        SIT_RETURN_IF_ERR(_SituationVulkanEndSingleTimeCommands(temp_cmd));

        // SAFE CLEANUP [2.3.14A]:
        // If we are initializing, Graveyards might not be ready or flushed yet.
        // Explicit destroy is fine here because we waited on the queue via EndSingleTimeCommands.
        if (sit_render.vk.graveyards) {
            _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        } else {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        }

        return SITUATION_SUCCESS;
    }
}

/**
 * @brief [INTERNAL] Reads data from a Vulkan buffer back to host memory.
 *
 * @details This helper abstracts the complexity of reading GPU memory. It intelligently selects the optimal path based on the buffer's memory type:
 *          1. **Direct Map:** If the buffer's memory is `HOST_VISIBLE` and `HOST_COHERENT` (e.g., a CPU-to-GPU buffer), it maps the memory directly and copies the data.
 *          2. **Staging Transfer:** If the buffer is `DEVICE_LOCAL` (e.g., a high-performance SSBO), it allocates a temporary host-visible staging buffer, records a GPU copy command, submits it, and waits for completion.
 *
 * @par Synchronization Logic
 * This function performs critical synchronization to ensure data integrity:
 * - **Pre-Copy Barrier:** Inserts a `vkCmdPipelineBarrier` to ensure that all previous GPU writes (from Compute Shaders, Vertex Shaders, or Transfers) are finished and flushed from cache before the copy operation begins.
 * - **Host-Read Barrier:** Ensures the transfer write to the staging buffer is visible to the host before mapping.
 *
 * @param src_buffer The source `VkBuffer` handle to read from.
 * @param src_alloc The VMA allocation handle associated with the source buffer (required to query memory flags).
 * @param size The number of bytes to read.
 * @param offset The byte offset within the source buffer to start reading from.
 * @param[out] out_data Pointer to the destination CPU memory buffer. Must be pre-allocated by the caller.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED` if the temporary staging buffer cannot be created.
 * @return `SITUATION_ERROR_BUFFER_MAP_FAILED` if memory mapping fails.
 *
 * @warning This is a **synchronous** operation. It allocates a command buffer, submits it, and stalls the CPU (`vkQueueWaitIdle`) until the GPU transfer is complete.
 */
static SituationError _SituationVulkanReadBackBuffer(VkBuffer src_buffer, VmaAllocation src_alloc, size_t size, size_t offset, void* out_data) {
    VkDevice device = sit_render.vk.device;
    VmaAllocator allocator = sit_render.vk.vma_allocator;

    // 1. Check if directly mappable
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(allocator, src_alloc, &alloc_info);

    if (alloc_info.pMappedData != NULL) {  // VMA already mapped it
        void* mapped_data;
        if (vmaMapMemory(allocator, src_alloc, &mapped_data) != VK_SUCCESS) return SITUATION_ERROR_BUFFER_MAP_FAILED;
        memcpy(out_data, (char*)mapped_data + offset, size);
        vmaUnmapMemory(allocator, src_alloc);
        return SITUATION_SUCCESS;
    }

    // 2. Use Staging Buffer
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    staging_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (vmaCreateBuffer(allocator, &staging_info, &staging_alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // Sync barrier: Wait for vertex/transfer stages to finish reading/writing before we copy
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.buffer = src_buffer;
    barrier.offset = offset;
    barrier.size = size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = offset;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, src_buffer, staging_buffer, 1, &copy_region);

    // Memory barrier for host read
    VkBufferMemoryBarrier host_barrier = {};
    host_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    host_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    host_barrier.buffer = staging_buffer;
    host_barrier.offset = 0;
    host_barrier.size = size;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host_barrier, 0, NULL);

    SituationError submit_err = _SituationVulkanEndSingleTimeCommands(cmd);
    if (submit_err != SITUATION_SUCCESS) {
        vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);
        return submit_err;
    }

    // 3. Map and Copy
    void* mapped_data;
    SituationError result = SITUATION_SUCCESS;
    if (vmaMapMemory(allocator, staging_allocation, &mapped_data) == VK_SUCCESS) {
        memcpy(out_data, mapped_data, size);
        vmaUnmapMemory(allocator, staging_allocation);
    } else {
        result = SITUATION_ERROR_BUFFER_MAP_FAILED;
    }

    vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);
    return result;
}
#endif

// ============================================================================
// Buffer Implementation
// ============================================================================

/**
 * @brief Creates a general-purpose GPU data buffer for storing vertices, indices, uniforms (UBOs), or shader storage data (SSBOs).
 * @details This is the primary function for allocating memory on the GPU. It abstracts the backend-specific complexities, such as OpenGL's buffer storage mechanisms or Vulkan's staging buffer transfers, into a single, unified call.
 *          The `usage_flags` parameter is critical, as it provides a hint to the driver about how the buffer will be used, which heavily influences performance and memory placement.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Uses modern Direct State Access (`glCreateBuffers`, `glNamedBufferStorage`). Buffers intended for updates (e.g., UBOs, SSBOs) are automatically created with `GL_DYNAMIC_STORAGE_BIT` to allow modification via `SituationUpdateBuffer`.
 * - **Vulkan:** This function implements a high-performance workflow.
 *   - If `initial_data` is provided, it automatically creates a temporary staging buffer, copies the data to it, and then records a GPU command to transfer the data to a fast, device-local (`VMA_MEMORY_USAGE_GPU_ONLY`) final buffer.
 *   - If the buffer is created with `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER` or `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`, it also **pre-allocates and caches a persistent `VkDescriptorSet`** within the `SituationBuffer` handle. This
 *     makes subsequent binding with `SituationCmdBindUniformBuffer` or `SituationCmdBindComputeBuffer` an extremely fast operation, avoiding runtime descriptor allocation overhead.
 *
 * @param size The total size of the buffer to allocate, in bytes. Must be greater than zero.
 * @param initial_data A pointer to the initial data to upload to the buffer. If `NULL`, the buffer is allocated, but its contents are undefined until written to.
 * @param usage_flags A bitmask of `SituationBufferUsageFlags` that tells the driver how the buffer will be used.
 *                    This is a critical performance hint. Flags can be combined using the bitwise OR operator (e.g., `SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC`).
 *
 * @return A `SituationBuffer` handle.
 *         - On success, the `id` member of the returned struct will be non-zero, and the handle is ready for use.
 *         - On failure, the `id` member will be 0. Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *           Failure can occur due to invalid parameters, running out of GPU memory, or other API errors.
 *
 * @note The caller is **responsible** for destroying the buffer using `SituationDestroyBuffer()` to prevent GPU memory leaks.;
 *
 * @warning Providing incorrect or overly broad `usage_flags` can lead to suboptimal performance. For example, creating a static vertex buffer without `SITUATION_BUFFER_USAGE_TRANSFER_DST` might prevent it from being updated efficiently later.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationDestroyBuffer();
 * @see SituationUpdateBuffer()
 * @see SituationGetBufferData()
 * @see SituationBufferUsageFlags
 */
#if defined(SITUATION_USE_OPENGL)
static GLenum _SitGLUploadNamedBuffer(GLuint buffer_id, GLsizeiptr size, const void* initial_data) {
    while (glGetError() != GL_NO_ERROR) {}

    glNamedBufferStorage(buffer_id, size, initial_data, GL_DYNAMIC_STORAGE_BIT);
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) {
        return GL_NO_ERROR;
    }

    /* Many Windows GL drivers reject glNamedBufferStorage; glNamedBufferData still works. */
    glNamedBufferData(buffer_id, size, initial_data, GL_DYNAMIC_DRAW);
    return glGetError();
}

static bool _SitGLInitReadbackNamedBuffer(GLuint buffer_id, size_t size, bool* out_persistent_coherent) {
    *out_persistent_coherent = false;
    while (glGetError() != GL_NO_ERROR) {}

    /* GL_DYNAMIC_STORAGE_BIT required: GPU transfer ops (CopyBuffer, pack readback) write the buffer. */
    GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(buffer_id, (GLsizeiptr)size, NULL, storage_flags);
    if (glGetError() == GL_NO_ERROR) {
        *out_persistent_coherent = true;
        return true;
    }
    while (glGetError() != GL_NO_ERROR) {}

    glNamedBufferData(buffer_id, (GLsizeiptr)size, NULL, GL_STREAM_READ);
    return glGetError() == GL_NO_ERROR;
}
#endif

SITAPI SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer) {
    if (!out_buffer) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_buffer, 0, sizeof(SituationBuffer));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (size == 0) return SITUATION_ERROR_INVALID_PARAM;

    SituationBuffer handle;
    _SituationBufferSlot* slot = _SitAllocBufferSlot(&handle);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    slot->size_in_bytes = size;
    slot->usage_flags = usage_flags;
    handle.size_in_bytes = size;
    handle.usage_flags = usage_flags;

#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    glCreateBuffers(1, &slot->gl_buffer_id);
    if (slot->gl_buffer_id == 0) {
        _SitFreeBufferSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for SituationCreateBuffer.");
    }
    GLenum storage_err = _SitGLUploadNamedBuffer(slot->gl_buffer_id, (GLsizeiptr)size, initial_data);
    if (storage_err != GL_NO_ERROR) {
        glDeleteBuffers(1, &slot->gl_buffer_id);
        _SitFreeBufferSlot(handle);
        char detail[96];
        snprintf(detail, sizeof(detail), "buffer upload failed (GL error 0x%X)", (unsigned int)storage_err);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, detail);
    }

#elif defined(SITUATION_USE_VULKAN)
    VkBufferUsageFlags vk_usage = 0;
    if (usage_flags & SITUATION_BUFFER_USAGE_VERTEX_BUFFER) vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_INDEX_BUFFER) vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_UNIFORM_BUFFER) vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_INDIRECT_BUFFER) vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_SRC) vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_DST) vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_DEVICE_ADDRESS) vk_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    slot->vk_usage_flags = vk_usage;

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
    SituationError err = _SituationVulkanCreateAndUploadBuffer(cmd, initial_data, size, vk_usage, &slot->vk_buffer, &slot->vma_allocation);
    SituationError end_err = _SituationVulkanEndSingleTimeCommands(cmd);

    if (end_err != SITUATION_SUCCESS) {
        _SitFreeBufferSlot(handle);
        return end_err;
    }
    if (err != SITUATION_SUCCESS) {
        _SitFreeBufferSlot(handle);
        return err;
    }
#endif

    *out_buffer = handle;
    return SITUATION_SUCCESS;
}

/**
 * @brief [Phase 1] Creates an asynchronous GPU->CPU staging buffer for readback.
 */
SITAPI SituationError SituationCreateReadbackBuffer(size_t size, SituationBuffer* out_buffer) {
    if (!out_buffer) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_buffer, 0, sizeof(SituationBuffer));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (size == 0) return SITUATION_ERROR_INVALID_PARAM;

    SituationBuffer handle;
    _SituationBufferSlot* slot = _SitAllocBufferSlot(&handle);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    slot->size_in_bytes = size;
    slot->usage_flags = SITUATION_BUFFER_USAGE_TRANSFER_DST;
    slot->is_readback = true;
    handle.size_in_bytes = size;
    handle.usage_flags = SITUATION_BUFFER_USAGE_TRANSFER_DST;

#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    glCreateBuffers(1, &slot->gl_buffer_id);
    if (slot->gl_buffer_id == 0) {
        _SitFreeBufferSlot(handle);
        _SituationReleaseHostGLContextIfInFrame();
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for readback.");
    }
    if (!_SitGLInitReadbackNamedBuffer(slot->gl_buffer_id, size, &slot->readback_is_host_coherent)) {
        glDeleteBuffers(1, &slot->gl_buffer_id);
        _SitFreeBufferSlot(handle);
        _SituationReleaseHostGLContextIfInFrame();
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "OpenGL readback buffer storage failed.");
    }
    slot->mapped_ptr = NULL;
    slot->mapped_size = size;
    _SituationReleaseHostGLContextIfInFrame();
#elif defined(SITUATION_USE_VULKAN)
    slot->vk_usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo vma_info;
    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, &slot->vk_buffer, &slot->vma_allocation, &vma_info) != VK_SUCCESS) {
        _SitFreeBufferSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create readback buffer.");
    }
    slot->mapped_ptr = vma_info.pMappedData;
    slot->mapped_size = size;
    VkMemoryPropertyFlags mem_prop_flags = 0;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(sit_render.vk.physical_device, &memProperties);
    mem_prop_flags = memProperties.memoryTypes[vma_info.memoryType].propertyFlags;
    slot->readback_is_host_coherent = (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
#endif

    *out_buffer = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Destroys a GPU buffer and frees all associated resources.
 * @details This is the only correct way to release a buffer created with SituationCreateBuffer.
 *          It handles backend-specific cleanup (OpenGL buffer names, Vulkan buffers, VMA allocations, and cached descriptor sets) and removes the buffer from the library's internal resource tracking list to prevent shutdown warnings.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDeleteBuffers` to release the buffer object.
 * - **Vulkan:** Waits for the GPU to become idle to ensure the buffer is not in use, then frees the pre-allocated persistent descriptor set, and finally destroys the `VkBuffer` and its `VmaAllocation`.
 *
 * @param[in,out] buffer A pointer to the `SituationBuffer` handle to destroy. The handle is invalidated (zeroed out) after this call.
 *
 * @note It is safe to call this function on a NULL pointer or an already-destroyed (zeroed) buffer handle; it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationDestroyBuffer(SituationBuffer* buffer) {
    if (!buffer) return;
    _SituationBufferSlot* slot = _SitGetBufferSlot(*buffer);
    if (!slot) return;

#if defined(SITUATION_USE_OPENGL)
    if (slot->is_readback && slot->mapped_ptr) {
        glUnmapNamedBuffer(slot->gl_buffer_id);
        slot->mapped_ptr = NULL;
    }
    _SitGLDeferDestroyBuffer(slot->gl_buffer_id);
#elif defined(SITUATION_USE_VULKAN)
    if (slot->is_readback) {
        slot->mapped_ptr = NULL; // VMA unmaps automatically
    }
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->descriptor_set != VK_NULL_HANDLE && slot->descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->descriptor_pool, 1, &slot->descriptor_set);
            slot->descriptor_set = VK_NULL_HANDLE;
        }
        if (slot->vk_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->vk_buffer, slot->vma_allocation);
            slot->vk_buffer = VK_NULL_HANDLE;
            slot->vma_allocation = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyBuffer(slot->vk_buffer, slot->vma_allocation);
        if (slot->descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->descriptor_set, slot->descriptor_pool);
        }
    }
#endif

    _SitFreeBufferSlot(*buffer);
    memset(buffer, 0, sizeof(SituationBuffer));
}


/**
 * @brief Creates a self-contained GPU mesh from vertex and index data.
 * @details This function allocates all necessary GPU resources for a renderable mesh and configures its vertex attribute layout according to the library's shader contract.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Creates a dedicated Vertex Array Object (VAO) for this mesh. It also creates a Vertex Buffer Object (VBO) and an Element Buffer Object (EBO), uploads the provided data,
 *   and configures the VAO's vertex attributes (position, normal, texcoord) to point to the correct data within the VBO. The mesh is a fully self-contained, ready-to-draw object.
 * - **Vulkan:** Creates two device-local `VkBuffer` objects (one for vertices, one for indices) and uses a staging buffer process to upload the provided data to them for optimal performance.
 *
 * @param vertex_data Pointer to the raw, interleaved vertex data.
 * @param vertex_count The total number of vertices in the buffer.
 * @param vertex_stride The size of a single vertex struct in bytes (e.g., `sizeof(MyVertex)`).
 * @param index_data Pointer to the index data (must be `uint32_t`).
 * @param index_count The total number of indices.
 *
 * @return A `SituationMesh` handle.
 *         - On success, the `id` will be non-zero.
 *         - On failure, the `id` will be 0. Use `SituationGetLastErrorMsg()` for details.
 *
 * @note The provided vertex data **must** conform to the attribute layout defined in the Shader Contract (e.g., Position `vec3`, Normal `vec3`, TexCoord `vec2`).
 * @note The caller is **responsible** for destroying the mesh using `SituationDestroyMesh()`.;
 */
static size_t _SitMeshLayoutExpectedStride(SituationMeshVertexLayout layout) {
    switch (layout) {
        case SIT_MESH_LAYOUT_POS_ONLY: return 12;
        case SIT_MESH_LAYOUT_POS_TEX: return 20;
        case SIT_MESH_LAYOUT_POS_NRM: return 24;
        case SIT_MESH_LAYOUT_POS_NRM_TEX: return 32;
        case SIT_MESH_LAYOUT_POS_NRM_TAN_TEX: return 48;
        case SIT_MESH_LAYOUT_PULL: return 0;
        default: return 0;
    }
}

static bool _SitMeshStrideIsKnown(size_t stride) {
    switch (stride) {
        case 12: case 20: case 24: case 32: case 48: return true;
        default: return false;
    }
}

static SituationMeshVertexLayout _SitInferMeshLayoutFromStride(size_t stride) {
    switch (stride) {
        case 12: return SIT_MESH_LAYOUT_POS_ONLY;
        case 20: return SIT_MESH_LAYOUT_POS_TEX;
        case 24: return SIT_MESH_LAYOUT_POS_NRM;
        case 32: return SIT_MESH_LAYOUT_POS_NRM_TEX;
        case 48: return SIT_MESH_LAYOUT_POS_NRM_TAN_TEX;
        default: return SIT_MESH_LAYOUT_POS_NRM_TEX;
    }
}

static SituationError _SituationCreateMeshInternal(
    const void* vertex_data, int vertex_count, size_t vertex_stride,
    const uint32_t* index_data, int index_count,
    SituationMeshVertexLayout layout, SituationMesh* out_mesh)
{
    if (!out_mesh) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_mesh, 0, sizeof(SituationMesh));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (vertex_count < 0 || index_count < 0) return SITUATION_ERROR_INVALID_PARAM;
    if (vertex_count > 0 && (!vertex_data || vertex_stride == 0)) return SITUATION_ERROR_INVALID_PARAM;
    if (layout != SIT_MESH_LAYOUT_PULL) {
        size_t expected = _SitMeshLayoutExpectedStride(layout);
        if (expected != 0 && vertex_stride != expected) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
                "SituationCreateMeshEx: vertex_stride does not match SituationMeshVertexLayout.");
        }
    }

    SituationMesh handle;
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    _SituationMeshSlot* slot = _SitAllocMeshSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex);
    if (!slot) {
        return SituationGetLastErrorCode();
    }

    slot->vertex_count = vertex_count;
    slot->index_count = index_count;
    slot->vertex_stride = vertex_stride;
    slot->vertex_layout = layout;

    // Cache in handle for fast access
    handle.vertex_count = vertex_count;
    handle.index_count = index_count;
    handle.vertex_stride = vertex_stride;

#if defined(SITUATION_USE_OPENGL)
    _SituationMakeGLContextCurrentForHostThread();
    // Create VAO/VBO/EBO
    glCreateBuffers(1, &slot->vbo_id);
    if (slot->vbo_id == 0) {
        _SitFreeMeshSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for mesh VBO.");
    }
    glNamedBufferData(slot->vbo_id, vertex_count * vertex_stride, vertex_data, GL_STATIC_DRAW);

    if (index_count > 0 && index_data) {
        glCreateBuffers(1, &slot->ebo_id);
        if (slot->ebo_id == 0) {
            glDeleteBuffers(1, &slot->vbo_id);
            _SitFreeMeshSlot(handle);
            return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for mesh EBO.");
        }
        glNamedBufferData(slot->ebo_id, index_count * sizeof(uint32_t), index_data, GL_STATIC_DRAW);
    }
    SIT_CHECK_GL_ERROR();

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // Create Vertex Buffer
    VkDeviceSize vSize = vertex_count * vertex_stride;
    if (_SituationVulkanCreateAndUploadBuffer(cmd, vertex_data, vSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,  /* B1: required for BDA / vertex pulling */
            &slot->vertex_buffer, &slot->vertex_buffer_memory) != SITUATION_SUCCESS) {
        SituationError end_err = _SituationVulkanEndSingleTimeCommands(cmd);
        _SitFreeMeshSlot(handle);
        return (end_err != SITUATION_SUCCESS) ? end_err : SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // Create Index Buffer
    if (index_count > 0 && index_data) {
        VkDeviceSize iSize = index_count * sizeof(uint32_t);
        if (_SituationVulkanCreateAndUploadBuffer(cmd, index_data, iSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,  /* B3: required for GPU-driven index fetch */
                &slot->index_buffer, &slot->index_buffer_memory) != SITUATION_SUCCESS) {
            SituationError end_err = _SituationVulkanEndSingleTimeCommands(cmd);
            _SituationVulkanDestroyBuffer(slot->vertex_buffer, slot->vertex_buffer_memory); // Clean up VBO
            _SitFreeMeshSlot(handle);
            return (end_err != SITUATION_SUCCESS) ? end_err : SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }
    }
    SIT_RETURN_IF_ERR(_SituationVulkanEndSingleTimeCommands(cmd));
#endif

    if (vertex_count > 0 && vertex_data && vertex_stride > 0) {
        size_t cpu_v_size = (size_t)vertex_count * vertex_stride;
        slot->cpu_vertices = (uint8_t*)SIT_MALLOC(cpu_v_size);
        if (slot->cpu_vertices) {
            memcpy(slot->cpu_vertices, vertex_data, cpu_v_size);
            slot->cpu_vertices_size = cpu_v_size;
        }
    }
    if (index_count > 0 && index_data) {
        size_t cpu_i_size = (size_t)index_count * sizeof(uint32_t);
        slot->cpu_indices = (uint8_t*)SIT_MALLOC(cpu_i_size);
        if (slot->cpu_indices) {
            memcpy(slot->cpu_indices, index_data, cpu_i_size);
            slot->cpu_indices_size = cpu_i_size;
        }
    }

    // Track for leaks? No need, registry handles it.
    *out_mesh = handle;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh) {
    if (!_SitMeshStrideIsKnown(vertex_stride)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCreateMesh: unsupported vertex_stride (use SituationCreateMeshEx with explicit layout).");
    }
    return _SituationCreateMeshInternal(vertex_data, vertex_count, vertex_stride, index_data, index_count,
        _SitInferMeshLayoutFromStride(vertex_stride), out_mesh);
}

SITAPI SituationError SituationCreateMeshEx(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMeshVertexLayout layout, SituationMesh* out_mesh) {
    return _SituationCreateMeshInternal(vertex_data, vertex_count, vertex_stride, index_data, index_count, layout, out_mesh);
}

SITAPI SituationError SituationGetMeshVertexLayout(SituationMesh mesh, SituationMeshVertexLayout* out_layout) {
    if (!out_layout) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMeshVertexLayout: out_layout is NULL.");
    }
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGetMeshVertexLayout: invalid mesh handle.");
    }
    *out_layout = slot->vertex_layout;
    return SITUATION_SUCCESS;
}


/**
 * @brief Destroys a GPU mesh and frees all of its associated resources.
 * @details This is the only correct way to release a mesh created with `SituationCreateMesh`. It handles the full cleanup process, ensuring that all backend-specific GPU objects are deleted and that the mesh is removed from the library's internal resource tracking list.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Deletes the Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO) associated with the mesh using `glDelete*` functions.
 * - **Vulkan:** Waits for the GPU to become idle to ensure the buffers are not in use, then destroys the `VkBuffer` and frees the `VmaAllocation` for both the vertex and index buffers.
 *
 * @param[in,out] mesh A pointer to the `SituationMesh` handle to destroy. The handle is invalidated by being zeroed out after this call, preventing accidental reuse.
 *
 * @note It is safe to call this function on a NULL pointer or an already-destroyed (zeroed) mesh handle; it will simply do nothing.
 * @note Failure to call this function on a created mesh will result in a GPU memory leak and a warning message upon application shutdown.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 *
 * @see SituationCreateMesh()
 */
SITAPI void SituationDestroyMesh(SituationMesh* mesh) {
    if (!mesh) return;
    _SituationMeshSlot* slot = _SitGetMeshSlot(*mesh);
    if (!slot) return;

#if defined(SITUATION_USE_OPENGL)
    _SitGLDeferDestroyBuffer(slot->vbo_id);
    if (slot->ebo_id) _SitGLDeferDestroyBuffer(slot->ebo_id);
    // Also clean VAO cache for this mesh ID (which we don't have a unique ID for anymore,
    // but the slot index is unique enough for the session, or we use generation?
    // The VAO cache used mesh.id. We can use slot index or just clear all?
    // Or we use the VBO ID as the key (which is what we did before: id was VBO ID).
    // Let's rely on VBO ID for cache key.
    _SitGLDeferCleanMeshVAO(slot->vbo_id); // Assuming VBO ID is unique key
#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->vertex_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->vertex_buffer, slot->vertex_buffer_memory);
            slot->vertex_buffer = VK_NULL_HANDLE;
            slot->vertex_buffer_memory = VK_NULL_HANDLE;
        }
        if (slot->index_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->index_buffer, slot->index_buffer_memory);
            slot->index_buffer = VK_NULL_HANDLE;
            slot->index_buffer_memory = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyBuffer(slot->vertex_buffer, slot->vertex_buffer_memory);
        if (slot->index_buffer) {
            _SituationDeferDestroyBuffer(slot->index_buffer, slot->index_buffer_memory);
        }
    }
#endif

    SIT_FREE(slot->cpu_vertices);
    slot->cpu_vertices = NULL;
    slot->cpu_vertices_size = 0;
    SIT_FREE(slot->cpu_indices);
    slot->cpu_indices = NULL;
    slot->cpu_indices_size = 0;

    _SitFreeMeshSlot(*mesh);
    memset(mesh, 0, sizeof(SituationMesh));
}


/**
 * @brief Reads geometry data back from a GPU mesh into CPU memory.
 *
 * @details This function performs a synchronous readback from VRAM. It allocates memory for the vertex and index buffers which **the caller must free**.
 *
 * @param mesh The mesh handle.
 * @param[out] vertex_data Pointer to receive the array of vertices. Caller must free.
 * @param[out] vertex_count Pointer to receive the number of vertices.
 * @param[out] vertex_stride Pointer to receive the size of a single vertex in bytes.
 * @param[out] index_data Pointer to receive the array of indices. Caller must free.
 * @param[out] index_count Pointer to receive the number of indices.
 */
SITAPI void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count, int* vertex_stride, void** index_data, int* index_count) {
    // Initialize outputs to 0/NULL
    if (vertex_data) *vertex_data = NULL;
    if (vertex_count) *vertex_count = 0;
    if (vertex_stride) *vertex_stride = 0;
    if (index_data) *index_data = NULL;
    if (index_count) *index_count = 0;

    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) return;

    // Set count info
    if (vertex_count) *vertex_count = slot->vertex_count;
    if (vertex_stride) *vertex_stride = (int)slot->vertex_stride;
    if (index_count) *index_count = slot->index_count;

    size_t v_size = slot->vertex_count * slot->vertex_stride;
    size_t i_size = slot->index_count * sizeof(uint32_t);

    // Allocate CPU memory
    void* v_ptr = NULL;
    void* i_ptr = NULL;

    if (vertex_data && v_size > 0) {
        v_ptr = SIT_MALLOC(v_size);
        if (!v_ptr) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Mesh readback vertex buffer"); return; }
        *vertex_data = v_ptr;
    }

    if (index_data && i_size > 0) {
        i_ptr = SIT_MALLOC(i_size);
        if (!i_ptr) {
            if (v_ptr) SIT_FREE(v_ptr);
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Mesh readback index buffer");
            return;
        }
        *index_data = i_ptr;
    }

    if (v_ptr && slot->cpu_vertices && slot->cpu_vertices_size >= v_size) {
        memcpy(v_ptr, slot->cpu_vertices, v_size);
    } else if (v_ptr) {
#if defined(SITUATION_USE_OPENGL)
        glGetNamedBufferSubData(slot->vbo_id, 0, v_size, v_ptr);
        SIT_CHECK_GL_ERROR();
#elif defined(SITUATION_USE_VULKAN)
        if (_SituationVulkanReadBackBuffer(slot->vertex_buffer, slot->vertex_buffer_memory, v_size, 0, v_ptr) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to read vertex buffer from Vulkan mesh");
        }
#endif
    }

    if (i_ptr && slot->cpu_indices && slot->cpu_indices_size >= i_size) {
        memcpy(i_ptr, slot->cpu_indices, i_size);
    } else if (i_ptr) {
#if defined(SITUATION_USE_OPENGL)
        glGetNamedBufferSubData(slot->ebo_id, 0, i_size, i_ptr);
        SIT_CHECK_GL_ERROR();
#elif defined(SITUATION_USE_VULKAN)
        if (_SituationVulkanReadBackBuffer(slot->index_buffer, slot->index_buffer_memory, i_size, 0, i_ptr) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to read index buffer from Vulkan mesh");
        }
#endif
    }
}



/**
 * @brief [INTERNAL] Releases GPU registry slots still active at SituationShutdown.
 *
 * @details Called from `SituationShutdown` after library-owned defaults are cleared.
 *          Walks texture/shader/mesh/buffer/compute/model registries; any slot still
 *          marked active is destroyed so shutdown does not leave VRAM behind.
 *
 *          Logs each reclamation to stderr (slot + generation where applicable) so
 *          developers can add explicit Destroy/Unload calls if they want quiet shutdown.
 *
 * @note Explicit destroy before shutdown is still preferred; this path is the safety net.
 */
static void _SituationCleanupInternalDefaultResources(void) {
    if (sit_render.default_font_atlas.generation != 0) {
        SituationTexture atlas = sit_render.default_font_atlas;
        SituationDestroyTexture(&atlas);
        memset(&sit_render.default_font_atlas, 0, sizeof(sit_render.default_font_atlas));
    }
    memset(&sit_render.default_font, 0, sizeof(sit_render.default_font));
}

static void _SituationCleanupDanglingResources(void) {
    // 1. Textures
    for(int i=0; i<SITUATION_MAX_TEXTURES; i++) {
        if(sit_render.texture_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Texture (slot %d, gen %u)\n", i, sit_render.texture_registry[i].generation);
            SituationTexture t = { (uint32_t)i, sit_render.texture_registry[i].generation };
            SituationDestroyTexture(&t);
        }
    }
    // 2. Shaders
    for(int i=0; i<SITUATION_MAX_SHADERS; i++) {
        if(sit_render.shader_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Shader (slot %d)\n", i);
            SituationShader s = { (uint32_t)i, sit_render.shader_registry[i].generation };
            SituationUnloadShader(&s);
        }
    }
    // 3. Meshes
    for(int i=0; i<SITUATION_MAX_MESHES; i++) {
        if(sit_render.mesh_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Mesh (slot %d)\n", i);
            SituationMesh m = { (uint32_t)i, sit_render.mesh_registry[i].generation, 0,0,0 };
            SituationDestroyMesh(&m);
        }
    }
    // 4. Buffers
    for(int i=0; i<SITUATION_MAX_BUFFERS; i++) {
        if(sit_render.buffer_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Buffer (slot %d)\n", i);
            SituationBuffer b = { (uint32_t)i, sit_render.buffer_registry[i].generation, 0, 0 };
            SituationDestroyBuffer(&b);
        }
    }
    // 5. Compute
    for(int i=0; i<SITUATION_MAX_COMPUTE_PIPELINES; i++) {
        if(sit_render.compute_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Compute pipeline (slot %d)\n", i);
            SituationComputePipeline p = { (uint32_t)i, sit_render.compute_registry[i].generation };
            SituationDestroyComputePipeline(&p);
        }
    }
    // 6. Models
    for(int i=0; i<SITUATION_MAX_MODELS; i++) {
        if(sit_render.model_registry[i].is_active) {
            fprintf(stderr, "Situation [Shutdown]: releasing active Model (slot %d)\n", i);
            SituationModel m = { (uint32_t)i, sit_render.model_registry[i].generation, 0, NULL };
            SituationUnloadModel(&m);
        }
    }
}

/**
 * @brief Updates a portion (or all) of an existing buffer's contents with new data.
 *
 * @details Copies `size` bytes from the provided `data` pointer into the specified buffer,
 *          starting at byte offset `offset`. This is the primary way to update dynamic
 *          vertex buffers, index buffers, uniform buffers, storage buffers, staging buffers,
 *          or any other GPU-accessible buffer managed by the Situation library.
 *
 *          Supports partial updates (sub-region only) and full-buffer overwrites
 *          (when `offset == 0` and `size == buffer_size`).
 *
 *          Behavior depends on buffer usage flags and backend:
 *            - **Staging / CPU-writable buffers**: direct memcpy to mapped memory (fast)
 *            - **Device-local / GPU-only buffers**: may trigger staging copy via internal
 *              transfer queue or immediate command recording (slower, may block)
 *            - **Persistent mapped buffers**: direct write to persistent mapping
 *
 *          The update is **synchronous** from the caller's perspective  -  the data is guaranteed
 *          to be visible to subsequent GPU commands after this function returns (subject to
 *          proper pipeline barriers/sync inserted by the library).
 *
 * @param buffer Valid `SituationBuffer` handle created via `SituationCreateBuffer` or similar.
 *               Must support writing (not read-only).
 * @param offset Byte offset into the buffer where the update begins (0 = start of buffer).
 *               Must be < buffer size.
 * @param size Number of bytes to copy from `data`. Must satisfy `offset + size <= buffer size`.
 * @param data Pointer to the source data to copy into the buffer.
 *             Must remain valid for the duration of the call.
 *             Ownership is **not** transferred  -  caller retains the source buffer.
 *
 * @return SITUATION_SUCCESS on successful update,
 *         SITUATION_ERROR_INVALID_PARAM if buffer is invalid, offset+size out of bounds,
 *         or data is NULL when size > 0,
 *         SITUATION_ERROR_RESOURCE_INVALID if buffer is not writable (read-only usage),
 *         SITUATION_ERROR_MEMORY_ACCESS if mapping failed or staging allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL operation failed (e.g. out of device memory),
 *         or other appropriate error codes.
 *
 * @note Performance:
 *       - Fastest for persistently mapped or staging buffers
 *       - Slower for device-local buffers (implicit staging/transfer)
 *       - Avoid frequent small updates on device-local buffers  -  batch changes or use persistent mapping
 *
 *       Thread safety:
 *       - Safe to call from **main thread** or any thread that does not hold the render context
 *       - Internal synchronization ensures safe concurrent updates (if buffer allows)
 *       - Not safe to call from the render thread while recording commands that use the buffer
 *
 *       After update, the new data is visible to shaders/pipelines in subsequent command buffers.
 *       No explicit barrier is required unless you are reading back or using cross-queue access.
 *
 * @see SituationCreateBuffer, SituationMapBuffer, SituationUnmapBuffer,
 *      SituationCreateBufferFromData (for initial upload),
 *      SITUATION_ERROR_INVALID_PARAM, SITUATION_ERROR_RESOURCE_INVALID
 */
/**
 * @brief [Phase 4A] Record an async copy between buffers with independent offsets.
 */
SITAPI SituationError SituationCmdCopyBufferEx(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t src_offset, size_t dst_offset, size_t size) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdCopyBufferEx: library not initialized.");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdCopyBufferEx: cmd cannot be NULL.");
    }
    if (size == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyBufferEx: size must be non-zero.");
    }

    _SituationBufferSlot* slot_src = _SitGetBufferSlot(src);
    _SituationBufferSlot* slot_dst = _SitGetBufferSlot(dst);
    if (!slot_src || !slot_dst) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyBufferEx: invalid source or destination buffer handle.");
    }
    if ((slot_src->usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_SRC) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE, "SituationCmdCopyBufferEx: source buffer missing SITUATION_BUFFER_USAGE_TRANSFER_SRC.");
    }
    if ((slot_dst->usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_DST) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE, "SituationCmdCopyBufferEx: destination buffer missing SITUATION_BUFFER_USAGE_TRANSFER_DST.");
    }
    if (src_offset > slot_src->size_in_bytes || slot_src->size_in_bytes - src_offset < size) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyBufferEx: source range exceeds buffer size.");
    }
    if (dst_offset > slot_dst->size_in_bytes || slot_dst->size_in_bytes - dst_offset < size) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyBufferEx: destination range exceeds buffer size.");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* s_cmd = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(s_cmd, SIT_OP_COPY_BUFFER, p);
    p->args.copy_buffer.src_id = slot_src->gl_buffer_id;
    p->args.copy_buffer.dst_id = slot_dst->gl_buffer_id;
    p->args.copy_buffer.src_offset = src_offset;
    p->args.copy_buffer.dst_offset = dst_offset;
    p->args.copy_buffer.size = size;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    VkBufferCopy region = {0};
    region.srcOffset = src_offset;
    region.dstOffset = dst_offset;
    region.size = size;
    vkCmdCopyBuffer(vk_cmd, slot_src->vk_buffer, slot_dst->vk_buffer, 1, &region);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Legacy void wrapper for buffer copies.
 */
SITAPI SituationError SituationCmdCopyBuffer(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t offset, size_t size) {
    return SituationCmdCopyBufferEx(cmd, src, dst, offset, 0, size);
}

static int _SituationTextureMipExtent(int base_extent, uint32_t mip_level) {
    int extent = base_extent >> mip_level;
    return extent > 0 ? extent : 1;
}

static bool _SituationTextureRectInBounds(SituationTextureRect rect, int width, int height) {
    if (rect.width <= 0 || rect.height <= 0) return false;
    if (rect.x < 0 || rect.y < 0) return false;
    if (rect.x > width || rect.y > height) return false;
    if (rect.width > width - rect.x || rect.height > height - rect.y) return false;
    return true;
}

static void _SitTextureSetLayoutHint(_SituationTextureSlot* slot, SituationTextureLayout layout) {
    if (slot) {
        slot->layout_hint = layout;
    }
}

static bool _SitTextureLayoutIsDeferred(SituationTextureLayout layout) {
    return layout == SITUATION_TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT ||
           layout == SITUATION_TEXTURE_LAYOUT_PRESENT;
}

static bool _SitTextureLayoutIsAttachmentOrPresent(SituationTextureLayout layout) {
    return layout == SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT ||
           _SitTextureLayoutIsDeferred(layout);
}

static bool _SitTextureColorAttachmentLayoutAllowed(const _SituationTextureSlot* slot) {
    if (!slot) {
        return false;
    }
    return (slot->usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_SRC) != 0u;
}

static void _SitRenderPassSetTargetLayoutHint(int display_id, SituationTextureLayout layout) {
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS) {
        return;
    }
    if (!sit_render.virtual_display_slots_used[display_id]) {
        return;
    }
    int slot_idx = sit_render.virtual_display_slots[display_id].texture_slot_index;
    if (slot_idx < 0 || slot_idx >= SITUATION_MAX_TEXTURES) {
        return;
    }
    _SituationTextureSlot* slot = &sit_render.texture_registry[slot_idx];
    if (slot->is_active) {
        _SitTextureSetLayoutHint(slot, layout);
    }
}

#if defined(SITUATION_USE_VULKAN)
static VkImageLayout _SituationVulkanMapTextureLayout(SituationTextureLayout layout) {
    switch (layout) {
        case SITUATION_TEXTURE_LAYOUT_UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
        case SITUATION_TEXTURE_LAYOUT_GENERAL: return VK_IMAGE_LAYOUT_GENERAL;
        case SITUATION_TEXTURE_LAYOUT_SHADER_READ: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case SITUATION_TEXTURE_LAYOUT_TRANSFER_DST: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case SITUATION_TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case SITUATION_TEXTURE_LAYOUT_PRESENT: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static void _SituationVulkanTextureLayoutBarrierMasks(
    SituationTextureLayout layout,
    bool is_source,
    VkPipelineStageFlags* stage_mask,
    VkAccessFlags* access_mask) {
    switch (layout) {
        case SITUATION_TEXTURE_LAYOUT_UNDEFINED:
            *stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            *access_mask = 0;
            break;
        case SITUATION_TEXTURE_LAYOUT_GENERAL:
            *stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_SHADER_READ:
            *stage_mask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            *access_mask = VK_ACCESS_SHADER_READ_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC:
            *stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *access_mask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_TRANSFER_DST:
            *stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT:
            *stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            *access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT:
            *stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            *access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case SITUATION_TEXTURE_LAYOUT_PRESENT:
            (void)is_source;
            *stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            *access_mask = 0;
            break;
        default:
            *stage_mask = 0;
            *access_mask = 0;
            break;
    }
}

static SituationError _SitVulkanEmitTextureLayoutTransition(
    SituationCommandBuffer cmd,
    _SituationTextureSlot* slot,
    SituationTextureLayout old_layout,
    SituationTextureLayout new_layout,
    uint32_t base_mip_level) {
    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;
    VkAccessFlags src_access_mask = 0;
    VkAccessFlags dst_access_mask = 0;
    _SituationVulkanTextureLayoutBarrierMasks(old_layout, true, &src_stage_mask, &src_access_mask);
    _SituationVulkanTextureLayoutBarrierMasks(new_layout, false, &dst_stage_mask, &dst_access_mask);
    if (src_stage_mask == 0 || dst_stage_mask == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "assisted layout transition uses unsupported layout value.");
    }

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = _SituationVulkanMapTextureLayout(old_layout);
    barrier.newLayout = _SituationVulkanMapTextureLayout(new_layout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = base_mip_level;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        (VkCommandBuffer)cmd,
        src_stage_mask,
        dst_stage_mask,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier);
    return SITUATION_SUCCESS;
}
#endif

static const SituationRendererBehaviorPolicy* _SitGetActiveRendererBehaviorPolicy(SituationCommandBuffer cmd) {
#if defined(SITUATION_USE_OPENGL)
    if (cmd) {
        return &((SituationGLSoftCommandBuffer*)cmd)->behavior;
    }
#elif defined(SITUATION_USE_VULKAN)
    (void)cmd;
    return &sit_render.vk.behavior;
#endif
    static const SituationRendererBehaviorPolicy k_strict_default = {0};
    return &k_strict_default;
}

static SituationError _SitValidateRendererBehaviorPolicyForSet(const SituationRendererBehaviorPolicy* policy) {
    if (!policy) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (policy->transfer_usage != SIT_TRANSFER_USAGE_STRICT &&
        policy->transfer_usage != SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdSetRendererBehavior: unsupported transfer_usage policy value.");
    }
    if (policy->texture_layout != SIT_TEXTURE_LAYOUT_EXPLICIT &&
        policy->texture_layout != SIT_TEXTURE_LAYOUT_ASSISTED) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCmdSetRendererBehavior: tracked texture layout policy is not implemented yet.");
    }
    if (policy->coordinate != SIT_COORDINATE_STRICT) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdSetRendererBehavior: unsupported coordinate policy value.");
    }
    if (policy->validation > SIT_RENDERER_VALIDATION_COMPAT) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdSetRendererBehavior: unsupported validation policy value.");
    }
    if (policy->blit_filter != SIT_BLIT_FILTER_STRICT &&
        policy->blit_filter != SIT_BLIT_FILTER_DOWNGRADE_NEAREST) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdSetRendererBehavior: unsupported blit_filter policy value.");
    }
    return SITUATION_SUCCESS;
}

static bool _SitTextureIsCompatibleSampledColorSource(const _SituationTextureSlot* slot) {
    if (!slot) return false;
    if ((slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u &&
        (slot->usage_flags & (SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED)) == 0u) {
        return false;
    }
    if ((slot->usage_flags & (SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED)) == 0u) {
        return false;
    }
    return slot->format_api == SIT_TEXTURE_FORMAT_RGBA8_UNORM || slot->format_api == SIT_TEXTURE_FORMAT_RGBA8_SRGB;
}

static bool _SitTextureTransferSrcUsageAllowed(
    const _SituationTextureSlot* slot,
    const SituationRendererBehaviorPolicy* policy,
    bool* out_used_compatible_fallback) {
    if (out_used_compatible_fallback) {
        *out_used_compatible_fallback = false;
    }
    if (!slot || !policy) return false;
    if ((slot->usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_SRC) != 0u) {
        return true;
    }
    if (policy->transfer_usage != SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK) {
        return false;
    }
    if (!_SitTextureIsCompatibleSampledColorSource(slot)) {
        return false;
    }
    if (out_used_compatible_fallback) {
        *out_used_compatible_fallback = true;
    }
    return true;
}

static bool _SitTextureTransferDstUsageAllowed(const _SituationTextureSlot* slot) {
    return slot && (slot->usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_DST) != 0u;
}

static void _SitLogRendererBehaviorFallback(
    const SituationRendererBehaviorPolicy* policy,
    const char* message) {
    if (policy && policy->validation >= SIT_RENDERER_VALIDATION_WARN) {
        SituationLog(SIT_LOG_WARNING, "renderer behavior: %s", message);
    }
}

static SituationError _SitAssistedEnsureTextureLayout(
    SituationCommandBuffer cmd,
    _SituationTextureSlot* slot,
    SituationTextureLayout required_layout,
    uint32_t base_mip_level,
    const SituationRendererBehaviorPolicy* policy,
    const char* api_name) {
    if (!slot || !policy || !cmd) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (policy->texture_layout != SIT_TEXTURE_LAYOUT_ASSISTED) {
        return SITUATION_SUCCESS;
    }
    if (_SitTextureLayoutIsAttachmentOrPresent(required_layout)) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_NOT_IMPLEMENTED,
            "assisted layout policy does not synthesize attachment or present layout transitions.");
    }
    if (slot->layout_hint == required_layout) {
        return SITUATION_SUCCESS;
    }

    if (policy->validation >= SIT_RENDERER_VALIDATION_WARN) {
        char msg[256];
        snprintf(
            msg,
            sizeof(msg),
            "%s: assisted layout transition %d -> %d.",
            api_name,
            (int)slot->layout_hint,
            (int)required_layout);
        _SitLogRendererBehaviorFallback(policy, msg);
    }

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = NULL;
        SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PIPELINE_BARRIER, p);
        if (!p) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        p->args.barrier.src = 0;
        p->args.barrier.dst = 0;
    }
#elif defined(SITUATION_USE_VULKAN)
    {
        SituationError transition_err = _SitVulkanEmitTextureLayoutTransition(
            cmd,
            slot,
            slot->layout_hint,
            required_layout,
            base_mip_level);
        if (transition_err != SITUATION_SUCCESS) {
            return transition_err;
        }
    }
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif

    _SitTextureSetLayoutHint(slot, required_layout);
    return SITUATION_SUCCESS;
}

static SituationError _SitValidateTextureTransferSrcUsage(
    SituationCommandBuffer cmd,
    const _SituationTextureSlot* slot,
    const char* api_name) {
    const SituationRendererBehaviorPolicy* policy = _SitGetActiveRendererBehaviorPolicy(cmd);
    bool used_compatible_fallback = false;
    if (_SitTextureTransferSrcUsageAllowed(slot, policy, &used_compatible_fallback)) {
        if (used_compatible_fallback) {
            char msg[256];
            snprintf(
                msg,
                sizeof(msg),
                "%s: allowing sampled color source without TRANSFER_SRC under COMPATIBLE_FALLBACK policy.",
                api_name);
            _SitLogRendererBehaviorFallback(policy, msg);
        }
        return SITUATION_SUCCESS;
    }
    char detail[256];
    snprintf(
        detail,
        sizeof(detail),
        "%s: source texture missing SITUATION_TEXTURE_USAGE_TRANSFER_SRC.",
        api_name);
    return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, detail);
}

static SituationError _SitValidateTextureTransferDstUsage(
    const _SituationTextureSlot* slot,
    const char* api_name) {
    if (_SitTextureTransferDstUsageAllowed(slot)) {
        return SITUATION_SUCCESS;
    }
    char detail[256];
    snprintf(
        detail,
        sizeof(detail),
        "%s: destination texture missing SITUATION_TEXTURE_USAGE_TRANSFER_DST.",
        api_name);
    return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, detail);
}

static SituationError _SitResolveBlitRegionForPolicy(
    const SituationTextureBlitRegion* region,
    const _SituationTextureSlot* src_slot,
    const SituationRendererBehaviorPolicy* policy,
    SituationTextureBlitRegion* out_region) {
    if (!region || !src_slot || !policy || !out_region) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    *out_region = *region;
    if (region->filter != SITUATION_BLIT_FILTER_LINEAR) {
        return SITUATION_SUCCESS;
    }
    if ((src_slot->usage_flags & (SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED)) != 0u) {
        return SITUATION_SUCCESS;
    }
    if (policy->blit_filter == SIT_BLIT_FILTER_STRICT) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED,
            "SituationCmdBlitTexture: linear filtering requires a sampled color source texture.");
    }
    if (policy->blit_filter == SIT_BLIT_FILTER_DOWNGRADE_NEAREST) {
        out_region->filter = SITUATION_BLIT_FILTER_NEAREST;
        if (policy->validation >= SIT_RENDERER_VALIDATION_WARN) {
            _SitLogRendererBehaviorFallback(
                policy,
                "SituationCmdBlitTexture: linear filter downgraded to nearest (source lacks sampled usage).");
        }
        return SITUATION_SUCCESS;
    }
    return _SituationSetErrorFromCode(
        SITUATION_ERROR_INVALID_PARAM,
        "SituationCmdBlitTexture: unsupported blit_filter policy value.");
}

/**
 * @brief [Phase 4B] Record a strict color texture blit.
 */
SITAPI SituationError SituationCmdBlitTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureBlitRegion* region) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdBlitTexture: library not initialized.");
    }
    if (!cmd || !region) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBlitTexture: cmd and region are required.");
    }
    if (region->src_array_layer != 0u || region->dst_array_layer != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdBlitTexture: only array layer 0 is supported in the first slice.");
    }
    if (region->filter != SITUATION_BLIT_FILTER_NEAREST && region->filter != SITUATION_BLIT_FILTER_LINEAR) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBlitTexture: unsupported filter.");
    }

    _SituationTextureSlot* src_slot = _SitGetTextureSlot(src);
    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(dst);
    if (!src_slot || !dst_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBlitTexture: invalid source or destination texture handle.");
    }
    SituationError src_usage_err = _SitValidateTextureTransferSrcUsage(cmd, src_slot, "SituationCmdBlitTexture");
    if (src_usage_err != SITUATION_SUCCESS) {
        return src_usage_err;
    }
    SituationError dst_usage_err = _SitValidateTextureTransferDstUsage(dst_slot, "SituationCmdBlitTexture");
    if (dst_usage_err != SITUATION_SUCCESS) {
        return dst_usage_err;
    }
    if (src_slot->format_api != dst_slot->format_api ||
        (src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_UNORM && src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_SRGB)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED, "SituationCmdBlitTexture: first slice requires matching RGBA8 color formats.");
    }
    if (region->src_mip_level >= (uint32_t)src_slot->mip_levels ||
        region->dst_mip_level >= (uint32_t)dst_slot->mip_levels) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdBlitTexture: mip level is out of range.");
    }

    int src_w = _SituationTextureMipExtent(src_slot->width, region->src_mip_level);
    int src_h = _SituationTextureMipExtent(src_slot->height, region->src_mip_level);
    int dst_w = _SituationTextureMipExtent(dst_slot->width, region->dst_mip_level);
    int dst_h = _SituationTextureMipExtent(dst_slot->height, region->dst_mip_level);
    if (!_SituationTextureRectInBounds(region->src_rect, src_w, src_h) ||
        !_SituationTextureRectInBounds(region->dst_rect, dst_w, dst_h)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdBlitTexture: source or destination rectangle is out of bounds.");
    }

    const SituationRendererBehaviorPolicy* policy = _SitGetActiveRendererBehaviorPolicy(cmd);
    SituationTextureBlitRegion effective_region = *region;
    SituationError resolve_err = _SitResolveBlitRegionForPolicy(region, src_slot, policy, &effective_region);
    if (resolve_err != SITUATION_SUCCESS) {
        return resolve_err;
    }

    SituationError assisted_src_err = _SitAssistedEnsureTextureLayout(
        cmd,
        src_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC,
        region->src_mip_level,
        policy,
        "SituationCmdBlitTexture");
    if (assisted_src_err != SITUATION_SUCCESS) {
        return assisted_src_err;
    }
    SituationError assisted_dst_err = _SitAssistedEnsureTextureLayout(
        cmd,
        dst_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
        region->dst_mip_level,
        policy,
        "SituationCmdBlitTexture");
    if (assisted_dst_err != SITUATION_SUCCESS) {
        return assisted_dst_err;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* s_cmd = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(s_cmd, SIT_OP_BLIT_TEXTURE, p);
    p->args.blit_texture.src = src;
    p->args.blit_texture.dst = dst;
    p->args.blit_texture.region = effective_region;
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkImageBlit blit = {0};
    blit.srcOffsets[0] = (VkOffset3D){effective_region.src_rect.x, effective_region.src_rect.y, 0};
    blit.srcOffsets[1] = (VkOffset3D){effective_region.src_rect.x + effective_region.src_rect.width, effective_region.src_rect.y + effective_region.src_rect.height, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = effective_region.src_mip_level;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = (VkOffset3D){effective_region.dst_rect.x, effective_region.dst_rect.y, 0};
    blit.dstOffsets[1] = (VkOffset3D){effective_region.dst_rect.x + effective_region.dst_rect.width, effective_region.dst_rect.y + effective_region.dst_rect.height, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = effective_region.dst_mip_level;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    VkFilter vk_filter = (effective_region.filter == SITUATION_BLIT_FILTER_LINEAR) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    vkCmdBlitImage(
        (VkCommandBuffer)cmd,
        src_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        vk_filter);
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief [Phase 4B] Record a strict color texture copy (exact size, no scaling).
 */
SITAPI SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureCopyRegion* region) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdCopyTexture: library not initialized.");
    }
    if (!cmd || !region) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdCopyTexture: cmd and region are required.");
    }
    if (region->src_array_layer != 0u || region->dst_array_layer != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTexture: only array layer 0 is supported in the first slice.");
    }

    _SituationTextureSlot* src_slot = _SitGetTextureSlot(src);
    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(dst);
    if (!src_slot || !dst_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyTexture: invalid source or destination texture handle.");
    }
    SituationError src_usage_err = _SitValidateTextureTransferSrcUsage(cmd, src_slot, "SituationCmdCopyTexture");
    if (src_usage_err != SITUATION_SUCCESS) {
        return src_usage_err;
    }
    SituationError dst_usage_err = _SitValidateTextureTransferDstUsage(dst_slot, "SituationCmdCopyTexture");
    if (dst_usage_err != SITUATION_SUCCESS) {
        return dst_usage_err;
    }
    if (src_slot->format_api != dst_slot->format_api ||
        (src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_UNORM && src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_SRGB)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED, "SituationCmdCopyTexture: first slice requires matching RGBA8 color formats.");
    }
    if (region->src_mip_level >= (uint32_t)src_slot->mip_levels ||
        region->dst_mip_level >= (uint32_t)dst_slot->mip_levels) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTexture: mip level is out of range.");
    }

    int src_w = _SituationTextureMipExtent(src_slot->width, region->src_mip_level);
    int src_h = _SituationTextureMipExtent(src_slot->height, region->src_mip_level);
    int dst_w = _SituationTextureMipExtent(dst_slot->width, region->dst_mip_level);
    int dst_h = _SituationTextureMipExtent(dst_slot->height, region->dst_mip_level);
    SituationTextureRect dst_rect = {
        region->dst_x,
        region->dst_y,
        region->src_rect.width,
        region->src_rect.height
    };
    if (!_SituationTextureRectInBounds(region->src_rect, src_w, src_h) ||
        !_SituationTextureRectInBounds(dst_rect, dst_w, dst_h)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTexture: source or destination region is out of bounds.");
    }

    const SituationRendererBehaviorPolicy* policy = _SitGetActiveRendererBehaviorPolicy(cmd);
    SituationError assisted_src_err = _SitAssistedEnsureTextureLayout(
        cmd,
        src_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC,
        region->src_mip_level,
        policy,
        "SituationCmdCopyTexture");
    if (assisted_src_err != SITUATION_SUCCESS) {
        return assisted_src_err;
    }
    SituationError assisted_dst_err = _SitAssistedEnsureTextureLayout(
        cmd,
        dst_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
        region->dst_mip_level,
        policy,
        "SituationCmdCopyTexture");
    if (assisted_dst_err != SITUATION_SUCCESS) {
        return assisted_dst_err;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* s_cmd = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(s_cmd, SIT_OP_COPY_TEXTURE, p);
    p->args.copy_texture.src = src;
    p->args.copy_texture.dst = dst;
    p->args.copy_texture.region = *region;
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    _SitVDMarkContentUpdatedFromTextureSlot((int)dst.slot_index);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkImageCopy copy = {0};
    copy.srcOffset = (VkOffset3D){region->src_rect.x, region->src_rect.y, 0};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.mipLevel = region->src_mip_level;
    copy.srcSubresource.baseArrayLayer = 0;
    copy.srcSubresource.layerCount = 1;
    copy.dstOffset = (VkOffset3D){region->dst_x, region->dst_y, 0};
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.mipLevel = region->dst_mip_level;
    copy.dstSubresource.baseArrayLayer = 0;
    copy.dstSubresource.layerCount = 1;
    copy.extent = (VkExtent3D){
        (uint32_t)region->src_rect.width,
        (uint32_t)region->src_rect.height,
        1u
    };
    vkCmdCopyImage(
        (VkCommandBuffer)cmd,
        src_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy);
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    _SitVDMarkContentUpdatedFromTextureSlot((int)dst.slot_index);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

static size_t _SituationTextureBufferRowPitchBytes(size_t row_pitch, int width) {
    return row_pitch ? row_pitch : (size_t)width * 4u;
}

static bool _SituationBufferRegionFits(size_t buffer_size, size_t offset, int width, int height, size_t row_pitch_bytes) {
    if (width <= 0 || height <= 0) return false;
    if (offset > buffer_size) return false;
    size_t last_row = offset + row_pitch_bytes * (size_t)(height - 1);
    size_t end = last_row + (size_t)width * 4u;
    return end <= buffer_size;
}

/**
 * @brief [Phase 4B] Upload tightly packed RGBA8 rows from a buffer into a texture subregion.
 */
SITAPI SituationError SituationCmdCopyBufferToTexture(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationTexture dst, const SituationTextureCopyRegion* dst_region) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdCopyBufferToTexture: library not initialized.");
    }
    if (!cmd || !dst_region) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdCopyBufferToTexture: cmd and dst_region are required.");
    }
    if (dst_region->src_array_layer != 0u || dst_region->dst_array_layer != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyBufferToTexture: only array layer 0 is supported in the first slice.");
    }
    if (dst_region->src_rect.x != 0 || dst_region->src_rect.y != 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyBufferToTexture: src_rect x/y must be 0; use dst_x/dst_y for texture placement.");
    }

    _SituationBufferSlot* buf_slot = _SitGetBufferSlot(src);
    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(dst);
    if (!buf_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyBufferToTexture: invalid buffer handle.");
    }
    if (!dst_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyBufferToTexture: invalid texture handle.");
    }
    SituationError dst_usage_err = _SitValidateTextureTransferDstUsage(dst_slot, "SituationCmdCopyBufferToTexture");
    if (dst_usage_err != SITUATION_SUCCESS) {
        return dst_usage_err;
    }
    if (dst_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_UNORM && dst_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_SRGB) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED, "SituationCmdCopyBufferToTexture: first slice requires RGBA8 color formats.");
    }
    if ((buf_slot->usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_SRC) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE, "SituationCmdCopyBufferToTexture: buffer missing SITUATION_BUFFER_USAGE_TRANSFER_SRC.");
    }
    if (dst_region->dst_mip_level >= (uint32_t)dst_slot->mip_levels) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyBufferToTexture: mip level is out of range.");
    }

    int dst_w = _SituationTextureMipExtent(dst_slot->width, dst_region->dst_mip_level);
    int dst_h = _SituationTextureMipExtent(dst_slot->height, dst_region->dst_mip_level);
    SituationTextureRect dst_rect = {
        dst_region->dst_x,
        dst_region->dst_y,
        dst_region->src_rect.width,
        dst_region->src_rect.height
    };
    if (!_SituationTextureRectInBounds(dst_rect, dst_w, dst_h)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyBufferToTexture: destination region is out of bounds.");
    }
    if (!_SituationBufferRegionFits(buf_slot->size_in_bytes, src_offset, dst_region->src_rect.width, dst_region->src_rect.height, (size_t)dst_region->src_rect.width * 4u)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyBufferToTexture: source range exceeds buffer size.");
    }

    const SituationRendererBehaviorPolicy* policy = _SitGetActiveRendererBehaviorPolicy(cmd);
    SituationError assisted_dst_err = _SitAssistedEnsureTextureLayout(
        cmd,
        dst_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
        dst_region->dst_mip_level,
        policy,
        "SituationCmdCopyBufferToTexture");
    if (assisted_dst_err != SITUATION_SUCCESS) {
        return assisted_dst_err;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* s_cmd = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(s_cmd, SIT_OP_COPY_BUFFER_TO_TEXTURE, p);
    p->args.copy_buffer_to_texture.buffer_id = buf_slot->gl_buffer_id;
    p->args.copy_buffer_to_texture.buffer_offset = src_offset;
    p->args.copy_buffer_to_texture.dst = dst;
    p->args.copy_buffer_to_texture.region = *dst_region;
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    _SitVDMarkContentUpdatedFromTextureSlot((int)dst.slot_index);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkBufferImageCopy region = {0};
    region.bufferOffset = src_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = dst_region->dst_mip_level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){dst_region->dst_x, dst_region->dst_y, 0};
    region.imageExtent = (VkExtent3D){
        (uint32_t)dst_region->src_rect.width,
        (uint32_t)dst_region->src_rect.height,
        1u
    };
    vkCmdCopyBufferToImage(
        (VkCommandBuffer)cmd,
        buf_slot->vk_buffer,
        dst_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
    _SitTextureSetLayoutHint(dst_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
    _SitVDMarkContentUpdatedFromTextureSlot((int)dst.slot_index);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief [Phase 4B] Copy a texture subregion into a buffer with optional row pitch.
 */
SITAPI SituationError SituationCmdCopyTextureToBuffer(SituationCommandBuffer cmd, SituationTexture src, const SituationTextureCopyRegion* src_region, SituationBuffer dst, size_t dst_offset, size_t dst_row_pitch) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdCopyTextureToBuffer: library not initialized.");
    }
    if (!cmd || !src_region) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdCopyTextureToBuffer: cmd and src_region are required.");
    }
    if (src_region->src_array_layer != 0u || src_region->dst_array_layer != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTextureToBuffer: only array layer 0 is supported in the first slice.");
    }

    _SituationTextureSlot* src_slot = _SitGetTextureSlot(src);
    _SituationBufferSlot* buf_slot = _SitGetBufferSlot(dst);
    if (!src_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyTextureToBuffer: invalid texture handle.");
    }
    if (!buf_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdCopyTextureToBuffer: invalid buffer handle.");
    }
    SituationError src_usage_err = _SitValidateTextureTransferSrcUsage(cmd, src_slot, "SituationCmdCopyTextureToBuffer");
    if (src_usage_err != SITUATION_SUCCESS) {
        return src_usage_err;
    }
    if (src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_UNORM && src_slot->format_api != SIT_TEXTURE_FORMAT_RGBA8_SRGB) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED, "SituationCmdCopyTextureToBuffer: first slice requires RGBA8 color formats.");
    }
    if ((buf_slot->usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_DST) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE, "SituationCmdCopyTextureToBuffer: buffer missing SITUATION_BUFFER_USAGE_TRANSFER_DST.");
    }
    if (src_region->src_mip_level >= (uint32_t)src_slot->mip_levels) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTextureToBuffer: mip level is out of range.");
    }

    int src_w = _SituationTextureMipExtent(src_slot->width, src_region->src_mip_level);
    int src_h = _SituationTextureMipExtent(src_slot->height, src_region->src_mip_level);
    if (!_SituationTextureRectInBounds(src_region->src_rect, src_w, src_h)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdCopyTextureToBuffer: source region is out of bounds.");
    }

    size_t row_pitch_bytes = _SituationTextureBufferRowPitchBytes(dst_row_pitch, src_region->src_rect.width);
    if (dst_row_pitch != 0 && dst_row_pitch < (size_t)src_region->src_rect.width * 4u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyTextureToBuffer: dst_row_pitch is smaller than the row width.");
    }
    if (!_SituationBufferRegionFits(buf_slot->size_in_bytes, dst_offset, src_region->src_rect.width, src_region->src_rect.height, row_pitch_bytes)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdCopyTextureToBuffer: destination range exceeds buffer size.");
    }

    const SituationRendererBehaviorPolicy* policy = _SitGetActiveRendererBehaviorPolicy(cmd);
    SituationError assisted_src_err = _SitAssistedEnsureTextureLayout(
        cmd,
        src_slot,
        SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC,
        src_region->src_mip_level,
        policy,
        "SituationCmdCopyTextureToBuffer");
    if (assisted_src_err != SITUATION_SUCCESS) {
        return assisted_src_err;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* s_cmd = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(s_cmd, SIT_OP_COPY_TEXTURE_TO_BUFFER, p);
    p->args.copy_texture_to_buffer.src = src;
    p->args.copy_texture_to_buffer.region = *src_region;
    p->args.copy_texture_to_buffer.buffer_id = buf_slot->gl_buffer_id;
    p->args.copy_texture_to_buffer.buffer_offset = dst_offset;
    p->args.copy_texture_to_buffer.buffer_row_pitch = dst_row_pitch;
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkBufferImageCopy region = {0};
    region.bufferOffset = dst_offset;
    region.bufferRowLength = dst_row_pitch ? (uint32_t)(dst_row_pitch / 4u) : 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = src_region->src_mip_level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){src_region->src_rect.x, src_region->src_rect.y, 0};
    region.imageExtent = (VkExtent3D){
        (uint32_t)src_region->src_rect.width,
        (uint32_t)src_region->src_rect.height,
        1u
    };
    vkCmdCopyImageToBuffer(
        (VkCommandBuffer)cmd,
        src_slot->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buf_slot->vk_buffer,
        1,
        &region);
    _SitTextureSetLayoutHint(src_slot, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief [Phase 1] Read mapped buffer data safely.
 */
SITAPI SituationError SituationReadBuffer(SituationBuffer readback_buf, void* dst, size_t size) {
	_SituationFlushRenderThread();
    _SituationBufferSlot* slot = _SitGetBufferSlot(readback_buf);
    if (!slot || !slot->is_readback) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationReadBuffer: invalid readback buffer");
    }

    size_t copy_size = size > slot->mapped_size ? slot->mapped_size : size;

#if defined(SITUATION_USE_OPENGL)
    if (!slot->mapped_ptr) {
        _SituationMakeGLContextCurrentForHostThread();
        GLbitfield map_flags = slot->readback_is_host_coherent
            ? (GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)
            : GL_MAP_READ_BIT;
        slot->mapped_ptr = glMapNamedBufferRange(
            slot->gl_buffer_id, 0, (GLsizeiptr)slot->mapped_size, map_flags);
        if (!slot->mapped_ptr) {
            _SituationReleaseHostGLContextIfInFrame();
            return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "SituationReadBuffer: glMapNamedBufferRange failed.");
        }
        _SituationReleaseHostGLContextIfInFrame();
    }
    if (!slot->readback_is_host_coherent) {
        glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    }
#elif defined(SITUATION_USE_VULKAN)
    if (!slot->mapped_ptr) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationReadBuffer: invalid or unmapped readback buffer");
    }
    if (!slot->readback_is_host_coherent) {
        VmaAllocationInfo vma_info;
        vmaGetAllocationInfo(sit_render.vk.vma_allocator, slot->vma_allocation, &vma_info);
        vmaInvalidateAllocation(sit_render.vk.vma_allocator, slot->vma_allocation, 0, copy_size);
    }
#endif

    // The driver has synchronized this mapped_ptr up to the last frame.
    memcpy(dst, slot->mapped_ptr, copy_size);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data) {
    if (!data) return SITUATION_ERROR_INVALID_PARAM;
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    if (offset + size > slot->size_in_bytes) return SITUATION_ERROR_BUFFER_OVERFLOW;

#if defined(SITUATION_USE_OPENGL)
    // Immediate DSA upload. With render_thread_count > 0, buffer objects live in the
    // loader shared context — must make that current before glNamedBufferSubData.
    _SituationMakeGLContextCurrentForHostThread();
    glNamedBufferSubData(slot->gl_buffer_id, (GLintptr)offset, (GLsizeiptr)size, data);
    SIT_CHECK_GL_ERROR();
    _SituationReleaseHostGLContextIfInFrame();
    if (sit_render.gl.last_error != GL_NO_ERROR) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "SituationUpdateBuffer: glNamedBufferSubData failed.");
    }
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan: Try direct map first (works for host-visible buffers like UBOs).
    // If that fails (GPU-only memory), use a single-time command buffer with
    // vkCmdUpdateBuffer for small updates or staging for large ones.
    void* mapped;
    if (vmaMapMemory(sit_render.vk.vma_allocator, slot->vma_allocation, &mapped) == VK_SUCCESS) {
        memcpy((uint8_t*)mapped + offset, data, size);
        vmaUnmapMemory(sit_render.vk.vma_allocator, slot->vma_allocation);
        return SITUATION_SUCCESS;
    } else {
        // GPU-only memory: use single-time command buffer so this works outside of a frame.
        // vkCmdUpdateBuffer has a 65536-byte limit per spec.
        if (size > 65536) {
            // Large update: use staging buffer
            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;

            VkBufferCreateInfo staging_info = {};
            staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_info.size = size;
            staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo staging_alloc = {0};
            staging_alloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            if (vmaCreateBuffer(sit_render.vk.vma_allocator, &staging_info, &staging_alloc, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
                return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
            }

            void* staging_mapped;
            if (vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &staging_mapped) != VK_SUCCESS) {
                vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
                return SITUATION_ERROR_BUFFER_MAP_FAILED;
            }
            memcpy(staging_mapped, data, size);
            vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);

            VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
            if (cmd == VK_NULL_HANDLE) {
                vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
                return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
            }

            VkBufferCopy region = { .srcOffset = 0, .dstOffset = offset, .size = size };
            vkCmdCopyBuffer(cmd, staging_buffer, slot->vk_buffer, 1, &region);
            SituationError end_err = _SituationVulkanEndSingleTimeCommands(cmd);
            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
            return end_err;
        } else {
            // Small update: use vkCmdUpdateBuffer in a single-time command buffer
            VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
            if (cmd == VK_NULL_HANDLE) {
                return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
            }
            vkCmdUpdateBuffer(cmd, slot->vk_buffer, offset, size, data);
            return _SituationVulkanEndSingleTimeCommands(cmd);
        }
    }
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}



/**
 * @brief Reads data back from a GPU buffer to host memory.
 *
 * @details Copies a specified range of data from the GPU buffer into a user-provided host memory buffer. This is useful for debugging, reading results from compute shaders, or retrieving data generated on the GPU.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glGetNamedBufferSubData` to read data directly from the buffer object into host memory, provided the buffer was created with appropriate flags (e.g., `GL_DYNAMIC_STORAGE_BIT` or `GL_MAP_READ_BIT` implicitly via usage).
 * - **Vulkan:** Reading from GPU-local memory (`VMA_MEMORY_USAGE_GPU_ONLY`) requires a staging buffer. This function internally allocates a temporary staging buffer, copies the data from the source buffer to the staging buffer using a command,
 *   and then maps the staging buffer to copy the data to the user's `out_data` pointer.
 *   This process is asynchronous and requires waiting for the GPU to finish the copy.
 *
 * @param buffer The `SituationBuffer` handle to read data from.
 * @param offset The byte offset within the GPU buffer to start reading from.
 * @param size The number of bytes to read.
 * @param[out] out_data A pointer to the host memory buffer where the data will be written.
 *                      This buffer must be allocated by the caller and large enough to hold `size` bytes.
 *
 * @return SITUATION_SUCCESS on successful read.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid.
 * @return SITUATION_ERROR_INVALID_PARAM if `out_data` is NULL.
 * @return SITUATION_ERROR_BUFFER_INVALID_SIZE if `offset + size` exceeds the buffer's size.
 * @return SITUATION_ERROR_BUFFER_MAP_FAILED if mapping memory fails (Vulkan) or reading fails (OpenGL).
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if creating a staging buffer fails (Vulkan).
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED if recording or submitting the copy command fails (Vulkan).
 */
SITAPI SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data) {
	_SituationFlushRenderThread();
    if (!out_data) return SITUATION_ERROR_INVALID_PARAM;
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    // Synchronous read (stalls)
    glGetNamedBufferSubData(slot->gl_buffer_id, offset, size, out_data);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    // Use helper
    return _SituationVulkanReadBackBuffer(slot->vk_buffer, slot->vma_allocation, size, offset, out_data);
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}


#endif // SITUATION_IMPL_RENDERER_RESOURCES_H
