/***************************************************************************************************
*
*   situation_impl_vd.h - Virtual Display Module Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h after all renderer helpers are defined.
*
*   Contains:
*     - Virtual display creation, destruction, configuration
*     - Virtual display compositing (GL + Vulkan backends)
 *     - Virtual display state queries (dirty, size, composite time, content-update info)
 *     - Content-update tracking hooks (called from renderer command recording)
 *     - GL VD renderer initialization
 *
 *   This is an implementation-internal file. Do not include directly.
 *
 ***************************************************************************************************/
#ifndef SITUATION_IMPL_VD_H
#define SITUATION_IMPL_VD_H

//----------------------------------------------------------------------------------
// Content-update tracking (Phase 1) — hooks invoked from situation_impl_renderer.h
//----------------------------------------------------------------------------------

/** Monotonic clock aligned with SituationUpdateTimers / VD frame clock. */
static double _SitVDGetTimeSeconds(void) {
    if (sit_gs.timer_system_instance.is_initialized) {
        return sit_gs.timer_system_instance.current_system_time_seconds;
    }
    return sit_gs.current_time;
}

static void _SitVDGetCompositorIdleState(const SituationVirtualDisplay* vd, int* out_is_idle, double* out_elapsed_idle) {
    double now = _SitVDGetTimeSeconds();
    double since = now - vd->last_content_update_time;
    bool idle = since > vd->idle_threshold_seconds;
    if (out_is_idle) *out_is_idle = idle ? 1 : 0;
    if (out_elapsed_idle) *out_elapsed_idle = idle ? since : 0.0;
}

static void _SitVDFallbackColorNormalized(const SituationVirtualDisplay* vd, float out_rgba[4]) {
    out_rgba[0] = vd->fallback_color.r / 255.0f;
    out_rgba[1] = vd->fallback_color.g / 255.0f;
    out_rgba[2] = vd->fallback_color.b / 255.0f;
    out_rgba[3] = vd->fallback_color.a / 255.0f;
}

#if defined(SITUATION_USE_OPENGL)
static void _SitVDApplyCompositorIdleUniformsGL(GLuint program, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle) {
    float color[4];
    _SitVDFallbackColorNormalized(vd, color);
    glProgramUniform1i(program, SIT_UNIFORM_LOC_VD_IS_IDLE, is_idle);
    glProgramUniform1i(program, SIT_UNIFORM_LOC_VD_FALLBACK_MODE, (int)vd->fallback_mode);
    glProgramUniform1f(program, SIT_UNIFORM_LOC_VD_ELAPSED_IDLE, (float)elapsed_idle);
    glProgramUniform4fv(program, SIT_UNIFORM_LOC_VD_FALLBACK_COLOR, 1, color);
}
#endif

#if defined(SITUATION_USE_VULKAN)
static void _SitVDFillPathBPushConstants(uint8_t* out, const mat4 model_matrix, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle) {
    float color[4];
    _SitVDFallbackColorNormalized(vd, color);
    int fallback_mode = (int)vd->fallback_mode;
    float elapsed = (float)elapsed_idle;
    memcpy(out, model_matrix, sizeof(mat4));
    memcpy(out + 64, &vd->opacity, sizeof(float));
    memcpy(out + 68, &is_idle, sizeof(int));
    memcpy(out + 72, &fallback_mode, sizeof(int));
    memcpy(out + 76, &elapsed, sizeof(float));
    memcpy(out + 80, color, sizeof(float) * 4);
}

static void _SitVDFillPathAPushConstants(uint8_t* out, const mat4 model_matrix, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle) {
    float color[4];
    _SitVDFallbackColorNormalized(vd, color);
    int blend_mode = (int)vd->blend_mode;
    int fallback_mode = (int)vd->fallback_mode;
    float elapsed = (float)elapsed_idle;
    memset(out, 0, SIT_VD_PATH_A_PUSH_CONSTANT_SIZE);
    memcpy(out, model_matrix, sizeof(mat4));
    memcpy(out + 64, &blend_mode, sizeof(int));
    memcpy(out + 68, &vd->opacity, sizeof(float));
    memcpy(out + 72, &is_idle, sizeof(int));
    memcpy(out + 76, &fallback_mode, sizeof(int));
    memcpy(out + 80, &elapsed, sizeof(float));
    memcpy(out + 96, color, sizeof(float) * 4);
}
#endif

static void _SitVDMarkContentUpdated(SituationVirtualDisplay* vd) {
    if (!vd) return;
    vd->last_content_update_time = _SitVDGetTimeSeconds();
    vd->last_content_update_frame = vd->frame_count;
    vd->is_dirty = true;
}

static void _SitVDMarkContentUpdatedFromTextureSlot(int slot_index) {
    if (slot_index < 0) return;
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        if (!sit_render.virtual_display_slots_used[i]) continue;
        SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[i];
        if (vd->texture_slot_index == slot_index) {
            _SitVDMarkContentUpdated(vd);
            return;
        }
    }
}

static void _SitVDMarkComputeBindingsWritten(const int* slots, int slot_count) {
    if (!slots || slot_count <= 0) return;
    for (int b = 0; b < slot_count; ++b) {
        if (slots[b] >= 0) {
            _SitVDMarkContentUpdatedFromTextureSlot(slots[b]);
        }
    }
}

static void _SitVDEndRenderPassCheck(int display_id, bool had_draw) {
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !had_draw) return;
    if (!sit_render.virtual_display_slots_used[display_id]) return;
    _SitVDMarkContentUpdated(&sit_render.virtual_display_slots[display_id]);
}

#if defined(SITUATION_USE_OPENGL)
static void _SitVDResetGLRecordingState(SituationGLSoftCommandBuffer* buf) {
    if (!buf) return;
    buf->recording_pass_display_id = -1;
    buf->recording_pass_had_draw = false;
    for (int i = 0; i < SIT_VD_MAX_COMPUTE_TEXTURE_BINDS; ++i) {
        buf->compute_bound_texture_slots[i] = -1;
    }
}
#endif

static void _SitVDRecordingNoteDrawCmd(SituationCommandBuffer cmd) {
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf && buf->recording_render_pass_active && buf->recording_pass_display_id >= 0) {
        buf->recording_pass_had_draw = true;
    }
#elif defined(SITUATION_USE_VULKAN)
    (void)cmd;
    if (sit_render.vk.inside_render_pass && sit_render.vk.recording_pass_display_id >= 0) {
        sit_render.vk.recording_pass_had_draw = true;
    }
#endif
}

static void _SitVDNoteComputeTextureBind(SituationCommandBuffer cmd, uint32_t binding, int texture_slot_index) {
    if (binding >= SIT_VD_MAX_COMPUTE_TEXTURE_BINDS) return;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf) buf->compute_bound_texture_slots[binding] = texture_slot_index;
#elif defined(SITUATION_USE_VULKAN)
    (void)cmd;
    sit_render.vk.compute_bound_texture_slots[binding] = texture_slot_index;
#endif
}

static void _SitVDNoteComputeDispatch(SituationCommandBuffer cmd) {
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf) {
        _SitVDMarkComputeBindingsWritten(buf->compute_bound_texture_slots, SIT_VD_MAX_COMPUTE_TEXTURE_BINDS);
    }
#elif defined(SITUATION_USE_VULKAN)
    (void)cmd;
    _SitVDMarkComputeBindingsWritten(sit_render.vk.compute_bound_texture_slots, SIT_VD_MAX_COMPUTE_TEXTURE_BINDS);
#endif
}

//----------------------------------------------------------------------------------
// Virtual Display lifecycle & compositing
//----------------------------------------------------------------------------------

/**
 * @brief Creates a new virtual/offscreen display (render target) for multi-pass or composited rendering.
 *
 * @details Allocates and initializes a new virtual display that can be rendered to independently
 *          of the main window/swapchain. Virtual displays are useful for:
 *            - Multi-view rendering (e.g. editor viewports, minimaps, VR eyes, security cameras)
 *            - Post-processing pipelines (render scene apply effects composite)
 *            - Render-to-texture techniques
 *            - Layered UI or debug overlays with independent resolution/scaling
 *
 *          Once created, the virtual display can be rendered into using `SituationRenderVirtualDisplays`
 *          (or manually via its internal framebuffer/command list) and its output can be sampled as
 *          a texture in shaders or blitted/composited into the main framebuffer.
 *
 *          Parameters control rendering behavior, timing, and composition:
 *            - resolution: Internal render resolution (can differ from window size)
 *            - frame_time_mult: Time scaling factor applied to this displays update loop
 *            - z_order: Compositing order when using `SituationRenderVirtualDisplays` (lower = drawn first)
 *            - scaling_mode: How the virtual display output is scaled when composited (fit, fill, stretch, etc.)
 *            - blend_mode: Blending operation used when compositing this display onto others or the main target
 *
 * @param resolution Desired internal resolution of the virtual display (width, height in pixels).
 *                   Both components must be > 0. Fractional values are truncated.
 * @param frame_time_mult Multiplier applied to delta-time for this displays update callbacks.
 *                        1.0 = real-time, 0.5 = half-speed, 2.0 = double-speed, etc.
 *                        Useful for slow-motion effects or independent simulation rates.
 * @param z_order Z-order / layer index for automatic compositing (higher values drawn later/on top).
 *                Use 0 for background, positive for foreground. Negative values are allowed.
 * @param scaling_mode Scaling policy when the displays aspect ratio differs from the target
 *                     (e.g. `SITUATION_SCALING_FIT`, `SITUATION_SCALING_FILL`, `SITUATION_SCALING_STRETCH`).
 * @param blend_mode Blending mode used when this display is composited onto another target
 *                   (e.g. `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`, `SITUATION_BLEND_NONE`).
 * @param out_id Pointer to an integer that receives the unique ID of the new virtual display on success.
 *               On failure, the value is set to -1.
 *
 * @return SITUATION_SUCCESS on successful creation,
 *         SITUATION_ERROR_INVALID_PARAM if resolution or invalid enum values,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if framebuffer/texture allocation failed,
 *         SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT if maximum number of virtual displays reached
 *         (implementation-defined, typically 32),
 *         or other appropriate error codes.
 *
 * @note The returned ID is valid until the display is destroyed with `SituationDestroyVirtualDisplay`.
 *       Virtual displays are automatically rendered when calling `SituationRenderVirtualDisplays`
 *       (unless paused or hidden via separate flags).
 *       Resource cleanup (framebuffer, color/depth textures) is handled on destroy or shutdown.
 *       High-resolution virtual displays consume significant GPU memory monitor allocation failures.
 *
 * @see SituationRenderVirtualDisplays, SituationDestroyVirtualDisplay,
 *      SituationPauseVirtualDisplay, SituationSetVirtualDisplayZOrder,
 *      SituationScalingMode, SituationBlendMode,
 *      SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT, SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID
 */
SITAPI SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id) {
    return SituationCreateVirtualDisplayEx(resolution, frame_time_mult, z_order, scaling_mode, blend_mode, SITUATION_VD_FLAG_NONE, out_id);
}

/**
 * @brief Creates a new virtual display with extended flags controlling resource creation.
 *
 * @details Extended version of SituationCreateVirtualDisplay that accepts creation flags.
 *          When SITUATION_VD_FLAG_COMPUTE_TARGET is set:
 *            - The color texture is created with STORAGE usage (compute shader writable)
 *            - No depth buffer or render pass is created (compute-only, no rasterization)
 *            - The texture can be retrieved via SituationGetVirtualDisplayTexture() for binding
 *              to compute shader dispatches
 *          When flags == SITUATION_VD_FLAG_NONE, behavior is identical to SituationCreateVirtualDisplay.
 *
 * @param resolution Desired internal resolution of the virtual display.
 * @param frame_time_mult Time multiplier for this display's update rate.
 * @param z_order Compositing order (lower = drawn first).
 * @param scaling_mode How the VD is scaled when composited.
 * @param blend_mode Blending mode when compositing.
 * @param flags Creation flags (see SituationVDFlags).
 * @param out_id Pointer that receives the VD's unique ID on success.
 * @return SITUATION_SUCCESS on success, appropriate error code on failure.
 *
 * @see SituationGetVirtualDisplayTexture, SituationCreateVirtualDisplay, SituationVDFlags
 */
SITAPI SituationError SituationCreateVirtualDisplayEx(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, SituationVDFlags flags, int* out_id) {
    if (out_id) *out_id = -1;
    else return SITUATION_ERROR_INVALID_PARAM;

    // --- 1. Validation ---
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot create virtual display");
    }
    if (sit_render.active_virtual_display_count >= SITUATION_MAX_VIRTUAL_DISPLAYS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED, "Maximum virtual displays reached");
    }

    // --- 2. Find Free Slot ---
    int new_id = -1;
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        if (!sit_render.virtual_display_slots_used[i]) {
            new_id = i;
            break;
        }
    }
    if (new_id == -1) return SITUATION_ERROR_UNKNOWN_ERROR; // Should not happen given count check

    // --- 3. Initialize Slot ---
    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[new_id];
    memset(vd, 0, sizeof(SituationVirtualDisplay)); // Crucial: Start with clean slate for safe cleanup

    vd->id = new_id;
    vd->resolution.x = (resolution.x > 0) ? resolution.x : 1.0f;
    vd->resolution.y = (resolution.y > 0) ? resolution.y : 1.0f;
    vd->frame_time_multiplier = frame_time_mult;
    vd->z_order = z_order;
    vd->scaling_mode = scaling_mode;
    vd->blend_mode = blend_mode;
    vd->opacity = 1.0f;
    vd->visible = true;
    vd->is_dirty = true;
    vd->flags = flags;
    vd->texture_slot_index = -1;
    vd->last_update_time_seconds = glfwGetTime();
    vd->last_content_update_time = _SitVDGetTimeSeconds();
    vd->last_content_update_frame = 0;
    vd->idle_threshold_seconds = 1.0;
    vd->fallback_mode = SITUATION_VD_FALLBACK_SOLID;
    vd->fallback_color = (ColorRGBA){13, 38, 102, 255};

    bool is_compute_target = (flags & SITUATION_VD_FLAG_COMPUTE_TARGET) != 0;
    bool success = true; // Master success flag for the chain

#if defined(SITUATION_USE_VULKAN)
    // =================================================================
    // --- VULKAN IMPLEMENTATION ---
    // =================================================================

    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = sit_render.vk.depth_format;

    // --- Step 1: Create Color Image ---
    if (success) {
        VkImageUsageFlags color_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (is_compute_target) {
            // Compute target: writable storage image, transferable for presentation
            color_usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        } else {
            // Rasterization target: color attachment
            color_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (_SituationVulkanCreateImage((uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y, 1, color_format,
                                        VK_IMAGE_TILING_OPTIMAL,
                                        color_usage,
                                        VMA_MEMORY_USAGE_GPU_ONLY,
                                        &vd->vk.image, &vd->vk.image_memory) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "Failed to create VD color image");
            success = false;
        }
    }

    // --- Step 2: Create Color View ---
    if (success) {
        vd->vk.image_view = _SituationVulkanCreateImageView(vd->vk.image, color_format, VK_IMAGE_ASPECT_COLOR_BIT);
        if (vd->vk.image_view == VK_NULL_HANDLE) success = false;
    }

    // --- Step 3: Create Depth Image (skip for compute targets) ---
    if (success && !is_compute_target) {
        if (_SituationVulkanCreateImage((uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y, 1, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VMA_MEMORY_USAGE_GPU_ONLY,
                                        &vd->vk.depth_image, &vd->vk.depth_image_memory) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "Failed to create VD depth image");
            success = false;
        }
    }

    // --- Step 4: Create Depth View (skip for compute targets) ---
    if (success && !is_compute_target) {
        vd->vk.depth_image_view = _SituationVulkanCreateImageView(vd->vk.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
        if (vd->vk.depth_image_view == VK_NULL_HANDLE) success = false;
    }

    // --- Step 5: Create Sampler ---
    if (success) {
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &vd->vk.sampler) != VK_SUCCESS) success = false;
    }

    // --- Step 6: Create Render Pass (skip for compute targets) ---
    if (success && !is_compute_target) {
        VkAttachmentDescription attachments[2] = {0};
        // Color Attachment
        attachments[0].format = color_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Depth Attachment
        attachments[1].format = depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth_ref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pDepthStencilAttachment = &depth_ref;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 2;
        render_pass_info.pAttachments = attachments;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        if (vkCreateRenderPass(sit_render.vk.device, &render_pass_info, NULL, &vd->vk.render_pass) != VK_SUCCESS) success = false;

        // --- LOAD variant: same as above but color LOAD (preserves prior content for multi-pass) ---
        if (success) {
            attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            if (vkCreateRenderPass(sit_render.vk.device, &render_pass_info, NULL, &vd->vk.render_pass_load) != VK_SUCCESS) {
                // Non-fatal: LOAD variant unavailable, fall back to CLEAR pass and log
                vd->vk.render_pass_load = VK_NULL_HANDLE;
                fprintf(stderr, "Situation [VD]: failed to create LOAD render pass variant for VD %d; LOAD_OP will fall back to CLEAR.\n", new_id);
                fflush(stderr);
            }
        }
    }

    // --- Step 7: Create Framebuffer (skip for compute targets) ---
    if (success && !is_compute_target) {
        VkImageView fb_attachments[] = {vd->vk.image_view, vd->vk.depth_image_view};
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vd->vk.render_pass;
        framebuffer_info.attachmentCount = 2;
        framebuffer_info.pAttachments = fb_attachments;
        framebuffer_info.width = (uint32_t)vd->resolution.x;
        framebuffer_info.height = (uint32_t)vd->resolution.y;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(sit_render.vk.device, &framebuffer_info, NULL, &vd->vk.framebuffer) != VK_SUCCESS) success = false;
    }

    // --- Step 8: Allocate Descriptor Set ---
    if (success) {
        // [FIX v2.3.27B] Allocate and capture the pool handle
        vd->vk.descriptor_set = _SituationVulkanAllocateDescriptorSet(
            sit_render.vk.image_sampler_layout,
            &vd->vk.descriptor_pool // Store pool in the VD struct
        );

        if (vd->vk.descriptor_set == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate persistent descriptor set for VD.");
            success = false;
        }
    }

    // --- Step 9: Update Descriptor Set ---
    if (success) {
        VkDescriptorImageInfo image_desc_info = {};
        image_desc_info.sampler = vd->vk.sampler;
        image_desc_info.imageView = vd->vk.image_view;
        image_desc_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_set = {};
        write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_set.dstSet = vd->vk.descriptor_set;
        write_set.dstBinding = SIT_SAMPLER_BINDING_VD_SOURCE;
        write_set.dstArrayElement = 0;
        write_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_set.descriptorCount = 1;
        write_set.pImageInfo = &image_desc_info;

        vkUpdateDescriptorSets(sit_render.vk.device, 1, &write_set, 0, NULL);
    }

    // --- Failure Cleanup (Vulkan) ---
    if (!success) {
        // Check non-null before destroying. VD is zeroed at start, so this is safe.
        if (vd->vk.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(sit_render.vk.device, vd->vk.framebuffer, NULL);
        if (vd->vk.render_pass_load != VK_NULL_HANDLE) vkDestroyRenderPass(sit_render.vk.device, vd->vk.render_pass_load, NULL);
        if (vd->vk.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(sit_render.vk.device, vd->vk.render_pass, NULL);
        if (vd->vk.sampler != VK_NULL_HANDLE) vkDestroySampler(sit_render.vk.device, vd->vk.sampler, NULL);
        if (vd->vk.depth_image_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, vd->vk.depth_image_view, NULL);
        if (vd->vk.depth_image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, vd->vk.depth_image, vd->vk.depth_image_memory);
        if (vd->vk.image_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, vd->vk.image_view, NULL);
        if (vd->vk.image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, vd->vk.image, vd->vk.image_memory);

        if (vd->vk.descriptor_set != VK_NULL_HANDLE && vd->vk.descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, vd->vk.descriptor_pool, 1, &vd->vk.descriptor_set);
        }

        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }

#elif defined(SITUATION_USE_OPENGL)
    // =================================================================
    // --- OPENGL IMPLEMENTATION ---
    // =================================================================

    // Step 1: Create Objects
    if (!is_compute_target) {
        glCreateFramebuffers(1, &vd->gl.fbo_id);
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &vd->gl.texture_id);
    if (!is_compute_target) {
        glCreateRenderbuffers(1, &vd->gl.depth_rbo_id);
    }

    // Step 2: Configure Texture
    if (success) {
        glTextureStorage2D(vd->gl.texture_id, 1, GL_RGBA8, (GLsizei)vd->resolution.x, (GLsizei)vd->resolution.y);

        GLenum filter_mode = GL_NEAREST;
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_MIN_FILTER, filter_mode);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_MAG_FILTER, filter_mode);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_MAX_LEVEL, 0);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Step 3: Configure Renderbuffer & Attach (skip for compute targets)
    if (success && !is_compute_target) {
        glNamedRenderbufferStorage(vd->gl.depth_rbo_id, GL_DEPTH_COMPONENT24, (GLsizei)vd->resolution.x, (GLsizei)vd->resolution.y);
        glNamedFramebufferTexture(vd->gl.fbo_id, GL_COLOR_ATTACHMENT0, vd->gl.texture_id, 0);
        glNamedFramebufferRenderbuffer(vd->gl.fbo_id, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, vd->gl.depth_rbo_id);

        if (glCheckNamedFramebufferStatus(vd->gl.fbo_id, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_FBO_INCOMPLETE, "Incomplete framebuffer");
            success = false;
        }
    }

    SIT_CHECK_GL_ERROR();
    if (sit_render.gl.last_error != GL_NO_ERROR) success = false;

    // --- Failure Cleanup (OpenGL) ---
    if (!success) {
        if (vd->gl.texture_id != 0) glDeleteTextures(1, &vd->gl.texture_id);
        if (vd->gl.depth_rbo_id != 0) glDeleteRenderbuffers(1, &vd->gl.depth_rbo_id);
        if (vd->gl.fbo_id != 0) glDeleteFramebuffers(1, &vd->gl.fbo_id);
        return SITUATION_ERROR_OPENGL_GENERAL;
    }
#endif

    // --- 4. Register texture slot for all VDs (compute targets get STORAGE; raster targets get SAMPLED) ---
    {
        mtx_lock(&sit_render.resource_registry_mutex);
        int slot_idx = -1;
        for (int i = 0; i < SITUATION_MAX_TEXTURES; ++i) {
            if (!sit_render.texture_registry[i].is_active) {
                slot_idx = i;
                break;
            }
        }
        if (slot_idx == -1) {
            mtx_unlock(&sit_render.resource_registry_mutex);
            // Cleanup the VD resources we just created
#if defined(SITUATION_USE_VULKAN)
            if (vd->vk.descriptor_set != VK_NULL_HANDLE && vd->vk.descriptor_pool != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(sit_render.vk.device, vd->vk.descriptor_pool, 1, &vd->vk.descriptor_set);
            }
            if (vd->vk.render_pass_load != VK_NULL_HANDLE) vkDestroyRenderPass(sit_render.vk.device, vd->vk.render_pass_load, NULL);
            if (vd->vk.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(sit_render.vk.device, vd->vk.render_pass, NULL);
            if (vd->vk.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(sit_render.vk.device, vd->vk.framebuffer, NULL);
            if (vd->vk.sampler != VK_NULL_HANDLE) vkDestroySampler(sit_render.vk.device, vd->vk.sampler, NULL);
            if (vd->vk.depth_image_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, vd->vk.depth_image_view, NULL);
            if (vd->vk.depth_image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, vd->vk.depth_image, vd->vk.depth_image_memory);
            if (vd->vk.image_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, vd->vk.image_view, NULL);
            if (vd->vk.image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, vd->vk.image, vd->vk.image_memory);
#elif defined(SITUATION_USE_OPENGL)
            if (vd->gl.texture_id != 0) glDeleteTextures(1, &vd->gl.texture_id);
            if (vd->gl.depth_rbo_id != 0) glDeleteRenderbuffers(1, &vd->gl.depth_rbo_id);
            if (vd->gl.fbo_id != 0) glDeleteFramebuffers(1, &vd->gl.fbo_id);
#endif
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Max texture limit reached while registering VD texture.");
        }

        _SituationTextureSlot* slot = &sit_render.texture_registry[slot_idx];
        slot->generation++;
        if (slot->generation == 0) slot->generation = 1;
        slot->is_active = true;
        mtx_unlock(&sit_render.resource_registry_mutex);

        slot->width = (int)vd->resolution.x;
        slot->height = (int)vd->resolution.y;
        slot->mip_levels = 1;
        slot->format_api = SIT_TEXTURE_FORMAT_RGBA8_UNORM;
        // Compute targets are storage-writable; raster targets are sampled only
        SituationTextureUsageFlags tex_usage = SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_SRC;
        if (is_compute_target) tex_usage |= SITUATION_TEXTURE_USAGE_STORAGE;
        slot->usage_flags = tex_usage;
        slot->wrap_s = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
        slot->wrap_t = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
        slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
        slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
        slot->bindless_handle = 0;
        slot->source_path = NULL;
        slot->mod_time = 0;

#if defined(SITUATION_USE_VULKAN)
        slot->image = vd->vk.image;
        slot->format = VK_FORMAT_R8G8B8A8_UNORM;
        slot->image_view = vd->vk.image_view;
        slot->sampler = vd->vk.sampler;
        slot->allocation = vd->vk.image_memory;
        slot->descriptor_set = vd->vk.descriptor_set;
        slot->descriptor_pool = vd->vk.descriptor_pool;
        slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
        slot->single_sampler_descriptor_pool = VK_NULL_HANDLE;
#elif defined(SITUATION_USE_OPENGL)
        slot->gl_texture_id = vd->gl.texture_id;
        slot->internal_format = GL_RGBA8;
        slot->gl_bindless_handle = 0;
#endif

        vd->texture_slot_index = slot_idx;
    }

    // --- 5. Finalize ---
    sit_render.virtual_display_slots_used[new_id] = true;
    sit_render.active_virtual_display_count++;
    *out_id = new_id;
    return SITUATION_SUCCESS;
}

/**
 * @brief Destroys a virtual display and frees its associated graphics resources.
 *
 * @details Cleans up all backend-specific resources associated with the virtual display and marks its ID as available. This is the only correct way to release a virtual display.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Deletes the FBO, color texture, and depth renderbuffer.
 * - **Vulkan:** Waits for the device to be idle to ensure resources are not in use, then destroys the `VkFramebuffer`, `VkRenderPass`, `VkSampler`, `VkImageView`s, and the underlying `VkImage`s and their memory allocations. It also **frees the persistent `VkDescriptorSet`** back to its pool.
 *
 * @param display_id The ID of the virtual display to destroy.
 * @return SITUATION_SUCCESS on successful destruction.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library isn't initialized.
 * @return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID if the ID is invalid or not in use.
 */
SITAPI SituationError SituationDestroyVirtualDisplay(int display_id) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS) {
        return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
    }
    if (!sit_render.virtual_display_slots_used[display_id]) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND, "SituationDestroyVirtualDisplay: display ID not active");
    }
    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];

    // --- Free the texture registry slot for compute targets (view only, not resource owner) ---
    if (vd->texture_slot_index >= 0 && vd->texture_slot_index < SITUATION_MAX_TEXTURES) {
        mtx_lock(&sit_render.resource_registry_mutex);
        _SituationTextureSlot* slot = &sit_render.texture_registry[vd->texture_slot_index];
        // Zero out the slot references — VD owns the actual GPU resources, we just clear the view
        memset(slot, 0, sizeof(_SituationTextureSlot));
        slot->is_active = false;
        mtx_unlock(&sit_render.resource_registry_mutex);
    }

#if defined(SITUATION_USE_VULKAN)
    // Defer all destruction to the Graveyard to avoid stalling.

    // [FIX v2.3.27B] Pass the specific pool that owns this descriptor set
    if (vd->vk.descriptor_set != VK_NULL_HANDLE) {
        _SituationDeferDestroyDescriptorSet(vd->vk.descriptor_set, vd->vk.descriptor_pool);
    }

    if (vd->vk.framebuffer != VK_NULL_HANDLE) _SituationDeferDestroyFramebuffer(vd->vk.framebuffer);
    if (vd->vk.render_pass != VK_NULL_HANDLE) _SituationDeferDestroyRenderPass(vd->vk.render_pass);
    if (vd->vk.render_pass_load != VK_NULL_HANDLE) _SituationDeferDestroyRenderPass(vd->vk.render_pass_load);

    // Defer images (includes view and sampler for color, just view for depth)
    _SituationDeferDestroyImage(vd->vk.image, vd->vk.image_memory, vd->vk.image_view, vd->vk.sampler);
    if (vd->vk.depth_image != VK_NULL_HANDLE) {
        _SituationDeferDestroyImage(vd->vk.depth_image, vd->vk.depth_image_memory, vd->vk.depth_image_view, VK_NULL_HANDLE);
    }

#elif defined(SITUATION_USE_OPENGL)
    if (vd->gl.texture_id != 0) glDeleteTextures(1, &vd->gl.texture_id);
    if (vd->gl.depth_rbo_id != 0) glDeleteRenderbuffers(1, &vd->gl.depth_rbo_id);
    if (vd->gl.fbo_id != 0) glDeleteFramebuffers(1, &vd->gl.fbo_id);
#endif

    memset(vd, 0, sizeof(SituationVirtualDisplay));
    sit_render.virtual_display_slots_used[display_id] = false;
    sit_render.active_virtual_display_count--;
    return SITUATION_SUCCESS;
}

/**
 * @brief Retrieves the VD's internal color texture as a public SituationTexture handle.
 *
 * @details Returns a SituationTexture handle that can be used to sample the VD's color output
 *          in user shaders (e.g. SituationCmdBindDescriptorSet, bindless reads). Works for all
 *          virtual displays — both raster (SITUATION_VD_FLAG_NONE) and compute targets
 *          (SITUATION_VD_FLAG_COMPUTE_TARGET). Compute targets also have STORAGE usage and
 *          can be written by compute dispatches.
 *
 *          The returned handle references the VD's internal GPU texture — it does not create a
 *          copy. The handle is valid for the lifetime of the virtual display. Destroying the VD
 *          invalidates the handle.
 *
 * @param display_id The ID of the virtual display.
 * @param out_texture Pointer that receives the SituationTexture handle on success.
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library isn't initialized.
 * @return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID if the ID is invalid or not in use.
 * @return SITUATION_ERROR_INVALID_PARAM if out_texture is NULL.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the texture slot is inactive (should not happen after v2.4.257).
 *
 * @see SituationCreateVirtualDisplay, SituationCreateVirtualDisplayEx, SituationVDFlags
 */
SITAPI SituationError SituationGetVirtualDisplayTexture(int display_id, SituationTexture* out_texture) {
    if (!out_texture) return SITUATION_ERROR_INVALID_PARAM;
    *out_texture = (SituationTexture){0};

    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationGetVirtualDisplayTexture: invalid display ID");
    }

    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];

    if (vd->texture_slot_index < 0 || vd->texture_slot_index >= SITUATION_MAX_TEXTURES) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGetVirtualDisplayTexture: VD has no registered texture slot");
    }

    _SituationTextureSlot* slot = &sit_render.texture_registry[vd->texture_slot_index];
    if (!slot->is_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGetVirtualDisplayTexture: texture slot is inactive");
    }

    out_texture->slot_index = (uint32_t)vd->texture_slot_index;
    out_texture->generation = slot->generation;
    out_texture->width = slot->width;
    out_texture->height = slot->height;
    return SITUATION_SUCCESS;
}

/**
 * @brief Changes the scaling and filtering mode for a virtual display at runtime.
 *
 * @details Updates how the virtual display's internal texture is sampled when rendered during compositing. This allows switching between blurry (linear) and sharp (nearest-neighbor) scaling on the fly.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Directly updates the `GL_TEXTURE_MIN_FILTER` and `GL_TEXTURE_MAG_FILTER` parameters of the existing texture object.
 * - **Vulkan:** This is a more involved operation. It destroys the old `VkSampler`, creates a new one with the desired filter mode, and then updates the virtual display's persistent descriptor set to point to the new sampler.
 *
 * @param display_id The ID of the virtual display to configure.
 * @param scaling_mode The new scaling mode to apply (`SITUATION_SCALING_STRETCH`, `SITUATION_SCALING_FIT`, `SITUATION_SCALING_INTEGER`).
 * @return SITUATION_SUCCESS on successful update.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library isn't initialized.
 * @return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID if the ID is invalid or not in use.
 */
SITAPI SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
    }

    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];

    // If the mode isn't changing, do nothing.
    if (vd->scaling_mode == scaling_mode) {
        return SITUATION_SUCCESS;
    }

    // Update the enum value in the struct.
    vd->scaling_mode = scaling_mode;

#if defined(SITUATION_USE_OPENGL)
    // =================================================================
    // --- OPENGL IMPLEMENTATION ---
    // =================================================================

    // For OpenGL, we can simply change the texture parameters on the existing texture object.
    GLenum filter_mode = GL_NEAREST;

    // Bind the texture, change its parameters, and unbind it.
    glBindTexture(GL_TEXTURE_2D, vd->gl.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
    glBindTexture(GL_TEXTURE_2D, 0);

    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // =================================================================
    // --- VULKAN IMPLEMENTATION ---
    // =================================================================

    // For Vulkan, we must destroy the old sampler and create a new one.

    // 1. Defer destruction of the old sampler to avoid stalling.
    _SituationDeferDestroyImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, vd->vk.sampler);

    // 2. Create a new sampler with the correct filter mode.
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &vd->vk.sampler) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to re-create sampler for scaling mode change");
        // The VD is now in a bad state with a destroyed sampler.
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }

    // 3. Update the existing descriptor set to point to the new sampler.
    VkDescriptorImageInfo image_desc_info = {};
    image_desc_info.sampler = vd->vk.sampler; // Use the NEW sampler
    image_desc_info.imageView = vd->vk.image_view;
    image_desc_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write_set = {};
    write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_set.dstSet = vd->vk.descriptor_set; // The existing descriptor set
    write_set.dstBinding = 0;
    write_set.dstArrayElement = 0;
    write_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_set.descriptorCount = 1;
    write_set.pImageInfo = &image_desc_info;

    vkUpdateDescriptorSets(sit_render.vk.device, 1, &write_set, 0, NULL);

    return SITUATION_SUCCESS;

#endif
}

/**
 * @brief [INTERNAL] Sorting predicate for Virtual Displays.
 *
 * @details Used by `qsort` to order the array of visible Virtual Displays based on their `z_order` property.
 *          Ensures that displays with lower Z-values are processed and drawn first (background), while higher Z-values are drawn last (foreground).
 *
 * @param a Pointer to the first `SituationVirtualDisplay*` pointer.
 * @param b Pointer to the second `SituationVirtualDisplay*` pointer.
 * @return An integer less than, equal to, or greater than zero if the Z-order of `a` is respectively less than, equal to, or greater than `b`.
 */
static int _SituationSortVirtualDisplaysCallback(const void* a, const void* b) {
    const SituationVirtualDisplay* vdA = *(const SituationVirtualDisplay**)a; // Array of pointers
    const SituationVirtualDisplay* vdB = *(const SituationVirtualDisplay**)b;
    // Invisible displays should effectively be at the "back" or not considered for sorting if filtered earlier
    if (!vdA->visible && vdB->visible) return 1;  // Non-visible after visible
    if (vdA->visible && !vdB->visible) return -1; // Visible before non-visible
    if (!vdA->visible && !vdB->visible) return 0; // Both non-visible, order doesn't matter
    return vdA->z_order - vdB->z_order; // Sort by Z-order
}

/**
 * @brief Renders all active virtual displays into the provided command buffer.
 *
 * @details This is the central high-level function for rendering content from one or more
 *          virtual/offscreen displays (created via `SituationCreateVirtualDisplay` or similar).
 *          It records draw commands for each active virtual display into the supplied
 *          `SituationCommandBuffer`, allowing the caller to compose virtual display output
 *          into the main framebuffer, a texture, or another render pass.
 *
 *          Typical usage patterns:
 *            - Render all virtual displays to an intermediate texture (e.g. for post-processing)
 *            - Composite virtual displays directly into the final swapchain image
 *            - Render UI overlays or debug views on top of virtual display content
 *
 *          Behavior:
 *            - Iterates over all currently active virtual displays (in creation order or z-order)
 *            - For each display:
 *              - Binds the displays framebuffer / render target
 *              - Records the displays internal command list (scene, shaders, meshes, etc.)
 *              - Applies display-specific viewport, scissor, and clear settings
 *              - Handles layer/compositing order if z-sorting is enabled
 *            - Restores the original render state (viewport, framebuffer, etc.) after all displays
 *            - Records only no actual GPU submission occurs in this function
 *
 *          Thread safety invariants:
 *            - Safe to call from the **main thread** or any thread that owns the command buffer
 *            - The command buffer must not be in a recording state already used by another pass
 *            - Virtual display internal state (textures, framebuffers) is protected against concurrent
 *              modification during recording (via internal locks or deferred updates)
 *            - Actual GPU execution happens later on the render thread (via command buffer submission)
 *
 * @param cmd Valid command buffer handle (must be in recording state, e.g. from `SituationBeginCommandBuffer`).
 *            All draw commands for virtual displays are appended to this buffer.
 *
 * @return SITUATION_SUCCESS if all virtual displays were successfully recorded,
 *         SITUATION_ERROR_INVALID_PARAM if cmd is invalid or not recording,
 *         SITUATION_ERROR_RENDER_LIST_INCOMPLETE if any displays internal command list failed to record,
 *         SITUATION_ERROR_RESOURCE_INVALID if a virtual display is in an inconsistent state,
 *         SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND for orphaned display IDs (rare),
 *         or other appropriate error codes.
 *         Partial failures may still record some displays caller should check return value.
 *
 * @note This function does **not** present or submit it only records commands.
 *       After calling, the caller must end the command buffer, submit it (render thread),
 *       and wait for completion if synchronization is needed.
 *       Virtual displays that are paused, hidden, or have zero size are skipped automatically.
 *       Performance scales with number of active displays and complexity of each displays scene.
 *
 * @see SituationCreateVirtualDisplay, SituationBeginCommandBuffer,
 *      SituationSubmitCommandBuffer (or equivalent), SituationRenderFrame,
 *      SITUATION_ERROR_RENDER_LIST_INCOMPLETE, SITUATION_ERROR_VIRTUAL_DISPLAY_xxx
 */
SITAPI SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd) {
    // --- Initial Checks ---
    if (!SituationIsInitialized() || sit_render.active_virtual_display_count == 0) {
        sit_render.last_vd_composite_time_ms = 0.0;
        return SITUATION_SUCCESS;
    }

    // Start timing for profiling.
    double start_time = glfwGetTime();

    // --- Step 1 & 2: Gather, Filter, and Sort (Backend-Agnostic) ---
    SituationVirtualDisplay* visible_vds_to_render[SITUATION_MAX_VIRTUAL_DISPLAYS];
    int visible_count = 0;
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        if (sit_render.virtual_display_slots_used[i] && sit_render.virtual_display_slots[i].visible &&
            sit_render.virtual_display_slots[i].opacity > 0.001f) {

            // In OpenGL, we must also check that the texture handle is valid.
        #if defined(SITUATION_USE_OPENGL)
            if (sit_render.virtual_display_slots[i].gl.texture_id != 0) {
                 visible_vds_to_render[visible_count++] = &sit_render.virtual_display_slots[i];
            }
        #elif defined(SITUATION_USE_VULKAN)
            // For Vulkan, we would check its image/view handles.
            if (sit_render.virtual_display_slots[i].vk.image_view != VK_NULL_HANDLE) {
                 visible_vds_to_render[visible_count++] = &sit_render.virtual_display_slots[i];
            }
        #endif
        }
    }
    if (visible_count == 0) {
        sit_render.last_vd_composite_time_ms = 0.0;
        return SITUATION_SUCCESS;
    }

    qsort(visible_vds_to_render, visible_count, sizeof(SituationVirtualDisplay*), _SituationSortVirtualDisplaysCallback);

    // --- Backend-Specific Rendering ---
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* _sit_vd_pkt = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_RENDER_VIRTUAL_DISPLAYS, _sit_vd_pkt);
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.vd_compositing_blend_pipelines[SITUATION_BLEND_ALPHA] == VK_NULL_HANDLE) return SITUATION_ERROR_NOT_INITIALIZED;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    /* Composite target matches SituationCmdBeginRenderPass (canvas when fullscreen stretch). */
    bool canvas_stretch = _SituationRenderCanvasStretchActive();
    int composite_fb_w = canvas_stretch
        ? (int)sit_render.vk.canvas_resource_width
        : (int)sit_render.vk.swapchain_extent.width;
    int composite_fb_h = canvas_stretch
        ? (int)sit_render.vk.canvas_resource_height
        : (int)sit_render.vk.swapchain_extent.height;
    if (composite_fb_w < 1) composite_fb_w = 1;
    if (composite_fb_h < 1) composite_fb_h = 1;
    VkFramebuffer composite_target_fb = canvas_stretch
        ? sit_render.vk.canvas_framebuffer
        : sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];

    /* Path A needs vkCmdCopyImage outside a render pass. Path B can draw inside the caller's
     * active main-window pass (OpenGL never ends the pass for SIT_OP_RENDER_VIRTUAL_DISPLAYS). */
    bool any_path_a = false;
    for (int pi = 0; pi < visible_count; ++pi) {
        const SituationVirtualDisplay* pvd = visible_vds_to_render[pi];
        if (pvd->blend_mode >= SITUATION_BLEND_OVERLAY &&
            sit_render.vk.screen_copy_image != VK_NULL_HANDLE &&
            sit_render.vk.screen_copy_view != VK_NULL_HANDLE &&
            sit_render.vk.screen_copy_descriptor_set != VK_NULL_HANDLE) {
            any_path_a = true;
            break;
        }
    }

    bool caller_main_pass_active = sit_render.vk.inside_main_swapchain_render_pass;
    bool vd_resume_swapchain_after_caller_rp = false;
    if (caller_main_pass_active && any_path_a) {
        vkCmdEndRenderPass(vk_cmd);
        sit_render.vk.inside_main_swapchain_render_pass = false;
        sit_render.vk.inside_render_pass = false;
        vd_resume_swapchain_after_caller_rp = true;
    }

    // --- 1. UBO Update (Zero-Stall Persistent Write) ---
    float target_width = (float)composite_fb_w;
    float target_height = (float)composite_fb_h;
    ViewDataUBO ubo_data;
    glm_mat4_identity(ubo_data.view);
    SIT_RETURN_IF_ERR(_SitVulkanFillOrthoProjection2D(target_width, target_height, ubo_data.projection));

    // Write directly to the mapped pointer (NO vmaMapMemory stall!)
    memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));

    // --- 2. Global Setup ---
    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };

    // Track render pass state to minimize switching
    bool is_render_pass_active = caller_main_pass_active && !any_path_a;
    bool composite_started_own_render_pass = false;

    // Pre-fill the RenderPassBeginInfo struct for reuse (must supply clear values: color+depth CLEAR in main pass).
    VkClearValue vd_main_rp_clear_values[2] = {0};
    vd_main_rp_clear_values[1].depthStencil.depth = 1.0f;
    VkRenderPassBeginInfo rp_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp_info.renderPass = sit_render.vk.main_window_render_pass;
    rp_info.framebuffer = composite_target_fb;
    rp_info.renderArea.extent = (VkExtent2D){(uint32_t)composite_fb_w, (uint32_t)composite_fb_h};
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = vd_main_rp_clear_values;

    bool screen_copy_was_sampled = false;
    /* Path A vkCmdBeginRenderPass must use COLOR CLEAR only for the first advanced layer. Each extra
     * non-alpha VD used main_window_render_pass → CLEAR wiped the swapchain between layers → corrupt
     * compositing, GPU faults, and harness cascade (fence timeouts). Subsequent layers need LOAD
     * (main_window_render_pass_resume + main_window_framebuffers_resume). */
    uint32_t path_a_restart_index = 0;

    // Set up standard Viewport and Scissor for the screen (OpenGL-parity 2D)
    VkViewport viewport;
    _SitVulkanFillViewport2DOpenGLParity(target_width, target_height, &viewport);
    VkRect2D scissor = {{0, 0}, {(uint32_t)composite_fb_w, (uint32_t)composite_fb_h}};

    // --- Loop and Draw Each Virtual Display ---
    for (int i = 0; i < visible_count; ++i) {
        const SituationVirtualDisplay* vd = visible_vds_to_render[i];

        // --- Compositing rect (scale + translate in pixel space; Path B push, Path A mat4) ---
        float comp_scale_x = target_width;
        float comp_scale_y = target_height;
        float comp_trans_x = vd->offset.x;
        float comp_trans_y = vd->offset.y;
        mat4 T, S;
        mat4 model_matrix;
        glm_mat4_identity(model_matrix);

        switch (vd->scaling_mode) {
            case SITUATION_SCALING_STRETCH:
                comp_scale_x = target_width;
                comp_scale_y = target_height;
                comp_trans_x = vd->offset.x;
                comp_trans_y = vd->offset.y;
                break;
            case SITUATION_SCALING_FIT: {
                float sx = target_width / vd->resolution.x;
                float sy = target_height / vd->resolution.y;
                float s = fminf(sx, sy);
                comp_scale_x = vd->resolution.x * s;
                comp_scale_y = vd->resolution.y * s;
                comp_trans_x = (target_width - comp_scale_x) / 2.0f;
                comp_trans_y = (target_height - comp_scale_y) / 2.0f;
                break;
            }
            case SITUATION_SCALING_INTEGER: {
                float s = fmaxf(1.0f, floorf(fminf(target_width / vd->resolution.x, target_height / vd->resolution.y)));
                comp_scale_x = vd->resolution.x * s;
                comp_scale_y = vd->resolution.y * s;
                comp_trans_x = (target_width - comp_scale_x) / 2.0f;
                comp_trans_y = (target_height - comp_scale_y) / 2.0f;
                break;
            }
        }
        glm_translate_make(T, (vec3){comp_trans_x, comp_trans_y, 0.0f});
        glm_scale_make(S, (vec3){comp_scale_x, comp_scale_y, 1.0f});
        glm_mat4_mul(T, S, model_matrix);

        /* Path A: Photoshop-style modes need destination sample (screen copy). Path B: simple modes
           match OpenGL (vd_shader + glBlendFunc), not the advanced composite shader. */
        bool use_advanced = (vd->blend_mode >= SITUATION_BLEND_OVERLAY);
        /* Screen copy is torn down on swapchain cleanup and must be recreated with the swapchain.
           If it is missing (should not happen after fix), Path A would vkCmdCopyImage into VK_NULL_HANDLE. */
        if (use_advanced && (sit_render.vk.screen_copy_image == VK_NULL_HANDLE ||
                             sit_render.vk.screen_copy_view == VK_NULL_HANDLE ||
                             sit_render.vk.screen_copy_descriptor_set == VK_NULL_HANDLE)) {
            use_advanced = false;
        }

        if (use_advanced) {
            // ---------------------------------------------------------
            // PATH A: Advanced Blending (Requires Screen Copy)
            // ---------------------------------------------------------

            // 1. Stop Render Pass (Illegal to copy image inside RP)
            bool path_a_preserves_prior_draws = is_render_pass_active;
            if (is_render_pass_active) {
                vkCmdEndRenderPass(vk_cmd);
                is_render_pass_active = false;
            }

            VkImage swapchainImg = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];

            // 2. Barriers: Prepare Swapchain for Read, CopyTarget for Write
            /* Caller ended main pass above (or here via is_render_pass_active): swapchain is PRESENT_SRC_KHR; main_window_render_pass uses finalLayout PRESENT for color. */
            _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImg, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            if (!screen_copy_was_sampled) {
                _SituationVulkanTransitionImageLayout(vk_cmd, sit_render.vk.screen_copy_image, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            } else {
                _SituationVulkanTransitionImageLayout(vk_cmd, sit_render.vk.screen_copy_image, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            }

            // 3. Perform Copy
            VkImageCopy region = { .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}, .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}, .extent = {sit_render.vk.swapchain_extent.width, sit_render.vk.swapchain_extent.height, 1} };
            vkCmdCopyImage(vk_cmd, swapchainImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sit_render.vk.screen_copy_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // 4. Barriers: Restore Swapchain for Drawing, CopyTarget for Reading
            /* Resume pass initialLayout is PRESENT_SRC; default pass expects UNDEFINED before CLEAR. */
            {
                bool use_resume_layout = (path_a_restart_index > 0 || vd_resume_swapchain_after_caller_rp || path_a_preserves_prior_draws) &&
                    sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE &&
                    sit_render.vk.main_window_framebuffers_resume != NULL &&
                    sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count;
                VkImageLayout swapchain_draw_layout = use_resume_layout ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                                                        : VK_IMAGE_LAYOUT_UNDEFINED;
                _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImg, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_draw_layout);
            }
            _SituationVulkanTransitionImageLayout(vk_cmd, sit_render.vk.screen_copy_image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            screen_copy_was_sampled = true;

            // 5. Update Descriptor for Screen Copy (Bind the texture we just filled)
            VkDescriptorImageInfo copy_info = { .sampler = vd->vk.sampler, .imageView = sit_render.vk.screen_copy_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = sit_render.vk.screen_copy_descriptor_set;
            write.dstBinding = SIT_SAMPLER_BINDING_VD_DEST;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &copy_info;
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);

            // 6. Restart Render Pass (Path A): use CLEAR only when the swapchain image has no scene
            //    content yet; otherwise LOAD (resume pass) so we keep caller clears / Path B output.
            {
                bool use_resume = (path_a_restart_index > 0 || vd_resume_swapchain_after_caller_rp || path_a_preserves_prior_draws) &&
                    sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE &&
                    sit_render.vk.main_window_framebuffers_resume != NULL &&
                    sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count;
                rp_info.renderPass = use_resume ? sit_render.vk.main_window_render_pass_resume
                                               : sit_render.vk.main_window_render_pass;
                rp_info.framebuffer = use_resume
                    ? sit_render.vk.main_window_framebuffers_resume[sit_render.vk.current_image_index]
                    : sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
                if (use_resume) {
                    rp_info.clearValueCount = 0;
                    rp_info.pClearValues = NULL;
                } else {
                    rp_info.clearValueCount = 2;
                    rp_info.pClearValues = vd_main_rp_clear_values;
                }
                vkCmdBeginRenderPass(vk_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
                path_a_restart_index++;
                vd_resume_swapchain_after_caller_rp = false;
            }
            is_render_pass_active = true;

            // 7. Draw
            vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline);
            vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

            // Bind Sets: 0=GlobalUBO, 1=VDSampler, 2=ScreenCopy
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 1, 1, &vd->vk.descriptor_set, 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 2, 1, &sit_render.vk.screen_copy_descriptor_set, 0, NULL);

			// Define a layout-compatible byte buffer to avoid anonymous struct issues
            int path_a_is_idle = 0;
            double path_a_elapsed_idle = 0.0;
            _SitVDGetCompositorIdleState(vd, &path_a_is_idle, &path_a_elapsed_idle);
            uint8_t pc[SIT_VD_PATH_A_PUSH_CONSTANT_SIZE];
            _SitVDFillPathAPushConstants(pc, model_matrix, vd, path_a_is_idle, path_a_elapsed_idle);

            vkCmdPushConstants(vk_cmd, sit_render.vk.advanced_compositing_pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                               SIT_VD_PATH_A_PUSH_CONSTANT_SIZE, pc);
            _SitVulkanApplyVDCompositingDynamicState(vk_cmd, target_width, target_height);
            vkCmdDraw(vk_cmd, 4, 1, 0, 0);

        } else {
            // ---------------------------------------------------------
            // PATH B: Standard Blending (Fast Path)
            // ---------------------------------------------------------

            // 1. Ensure Render Pass is active
            if (!is_render_pass_active) {
                bool use_resume_begin = vd_resume_swapchain_after_caller_rp &&
                    sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE &&
                    sit_render.vk.main_window_framebuffers_resume != NULL &&
                    sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count;
                rp_info.renderPass = use_resume_begin ? sit_render.vk.main_window_render_pass_resume
                                                      : sit_render.vk.main_window_render_pass;
                rp_info.framebuffer = use_resume_begin
                    ? sit_render.vk.main_window_framebuffers_resume[sit_render.vk.current_image_index]
                    : sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
                if (use_resume_begin) {
                    rp_info.clearValueCount = 0;
                    rp_info.pClearValues = NULL;
                } else {
                    rp_info.clearValueCount = 2;
                    rp_info.pClearValues = vd_main_rp_clear_values;
                }
                vkCmdBeginRenderPass(vk_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
                vd_resume_swapchain_after_caller_rp = false;
                sit_render.vk.current_render_area.offset = (VkOffset2D){0, 0};
                sit_render.vk.current_render_area.extent = scissor.extent;
                composite_started_own_render_pass = true;

                // [CRITICAL VULKAN FIX] Must push dynamic viewport/scissor state
                // every time a new render pass begins!
                vkCmdSetViewport(vk_cmd, 0, 1, &viewport);
                vkCmdSetScissor(vk_cmd, 0, 1, &scissor);

                is_render_pass_active = true;
            }

            // 2. Draw (per-blend pipeline matches OpenGL glBlendFunc for simple modes)
            {
                int blend_idx = (int)vd->blend_mode;
                VkPipeline blend_pipeline = (blend_idx >= 0 && blend_idx < 5)
                    ? sit_render.vk.vd_compositing_blend_pipelines[blend_idx]
                    : VK_NULL_HANDLE;
                if (blend_pipeline == VK_NULL_HANDLE) {
                    blend_pipeline = sit_render.vk.vd_compositing_blend_pipelines[SITUATION_BLEND_ALPHA];
                }
                vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blend_pipeline);
            }
            vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.vd_compositing_pipeline_layout, 0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.vd_compositing_pipeline_layout, 1, 1, &vd->vk.descriptor_set, 0, NULL);

            int path_b_is_idle = 0;
            double path_b_elapsed_idle = 0.0;
            _SitVDGetCompositorIdleState(vd, &path_b_is_idle, &path_b_elapsed_idle);
            uint8_t vd_pc[SIT_VD_PATH_B_PUSH_CONSTANT_SIZE];
            _SitVDFillPathBPushConstants(vd_pc, model_matrix, vd, path_b_is_idle, path_b_elapsed_idle);
            vkCmdPushConstants(vk_cmd, sit_render.vk.vd_compositing_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               SIT_VD_PATH_B_PUSH_CONSTANT_SIZE, vd_pc);
            _SitVulkanApplyVDCompositingDynamicState(vk_cmd, target_width, target_height);
            vkCmdDraw(vk_cmd, 4, 1, 0, 0);
        }
    }

    // --- Cleanup ---
    if (caller_main_pass_active && !any_path_a) {
        /* Path B drew inside the caller's main-window pass — leave it open for SituationCmdEndRenderPass. */
        _SitVulkanApplyVDCompositingDynamicState(vk_cmd, target_width, target_height);
        sit_render.vk.inside_main_swapchain_render_pass = true;
        sit_render.vk.inside_render_pass = true;
        sit_render.vk.current_render_area.offset = (VkOffset2D){0, 0};
        sit_render.vk.current_render_area.extent = (VkExtent2D){(uint32_t)composite_fb_w, (uint32_t)composite_fb_h};
    } else {
        if (is_render_pass_active) {
            vkCmdEndRenderPass(vk_cmd);
            is_render_pass_active = false;
        }

        /* Restart the main window render pass so the caller can continue recording
         * commands (or call SituationCmdEndRenderPass). Must use the resume render pass:
         * main_window_render_pass clears color on every Begin — that erased VD composite output. */
        {
            VkRenderPassBeginInfo restart_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE &&
                sit_render.vk.main_window_framebuffers_resume != NULL &&
                sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count) {
                restart_info.renderPass = sit_render.vk.main_window_render_pass_resume;
                restart_info.framebuffer = sit_render.vk.main_window_framebuffers_resume[sit_render.vk.current_image_index];
            } else {
                restart_info.renderPass = sit_render.vk.main_window_render_pass;
                restart_info.framebuffer = sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
            }
            restart_info.renderArea.offset = (VkOffset2D){0, 0};
            restart_info.renderArea.extent = (VkExtent2D){(uint32_t)composite_fb_w, (uint32_t)composite_fb_h};
            restart_info.clearValueCount = 0;
            restart_info.pClearValues = NULL;
            vkCmdBeginRenderPass(vk_cmd, &restart_info, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport vp;
            _SitVulkanFillViewport2DOpenGLParity(target_width, target_height, &vp);
            VkRect2D sc = {{0, 0}, {(uint32_t)composite_fb_w, (uint32_t)composite_fb_h}};
            vkCmdSetViewport(vk_cmd, 0, 1, &vp);
            vkCmdSetScissor(vk_cmd, 0, 1, &sc);
            sit_render.vk.inside_main_swapchain_render_pass = true;
            sit_render.vk.inside_render_pass = true;
            sit_render.vk.current_render_area = restart_info.renderArea;
        }
        (void)composite_started_own_render_pass;
    }
#endif
    double end_time = glfwGetTime();
    sit_render.last_vd_composite_time_ms = (end_time - start_time) * 1000.0;
    return SITUATION_SUCCESS;
}

/**
 * @brief Configures multiple properties of an existing virtual display.
 *
 * @details Allows changing common properties like position, visibility, opacity, z-order, frame time multiplier, and blend mode in a single call.
 *
 * @param display_id The ID of the virtual display to configure.
 * @param offset The new top-left offset (in pixels) for rendering this VD onto the target.
 * @param opacity The new opacity (0.0f = transparent, 1.0f = opaque).
 * @param z_order The new rendering order (lower drawn first).
 * @param visible Whether this VD should be rendered during compositing.
 * @param frame_time_mult The new frame time multiplier for internal clock.
 * @param blend_mode The new blend mode for compositing.
 * @return SITUATION_SUCCESS on successful configuration.
 *         Returns SITUATION_ERROR_NOT_INITIALIZED if the library isn't initialized.
 *         Returns SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID if the ID is invalid or not in use.
 */
SituationError SituationConfigureVirtualDisplay(int display_id, Vector2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
    }
    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];

    bool visual_property_changed = false;
    if (vd->offset.x != offset.x || vd->offset.y != offset.y) visual_property_changed = true;
    if (fabsf(vd->opacity - opacity) > 0.001f) visual_property_changed = true; // Compare floats with tolerance
    if (vd->z_order != z_order) visual_property_changed = true;
    if (vd->visible != visible) visual_property_changed = true;
    // Resolution changes would require re-creating the FBO, not just configuring.
    // frame_time_mult doesn't make it visually dirty for compositing.

    vd->offset = offset;
    vd->opacity = (opacity < 0.0f) ? 0.0f : (opacity > 1.0f) ? 1.0f : opacity;
    vd->z_order = z_order;
    vd->visible = visible;
    if (frame_time_mult > 0.0) vd->frame_time_multiplier = frame_time_mult;
    vd->blend_mode = blend_mode;
    if (visual_property_changed) {
        // While compositing will pick up these changes, if the *content* of the VD
        // should react to these, the app might need to mark it dirty.
        // For now, configuring doesn't automatically mark the *content* dirty.
        // The compositing stage will use the new properties.
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Retrieves a pointer to the internal state structure of a virtual display.
 *
 * @details Provides direct access to the `SituationVirtualDisplay` struct for advanced querying of properties not covered by specific getter functions. Use with caution.
 *
 * @param display_id The ID of the virtual display.
 * @return A pointer to the `SituationVirtualDisplay` struct.
 *         Returns NULL if the library isn't initialized or the ID is invalid/not in use.
 *         Check `SituationGetLastErrorMsg()` if NULL is returned unexpectedly.
 */
SITAPI SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id) {
    if (!SituationIsInitialized() || display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "GetVirtualDisplay");
        return NULL;
    }
    return &sit_render.virtual_display_slots[display_id];
}

/**
 * @brief Marks a virtual display as needing to be re-rendered.
 *
 * @details Sets the internal 'dirty' flag. The `SituationRenderVirtualDisplays` function uses this flag to determine if a VD needs its content re-rendered before compositing.
 *          Rendering to a VD's target (e.g., using `SituationBeginVirtualDisplayFrame`/`SituationEndVirtualDisplayFrame`) should automatically mark it dirty, but this function allows manual control.
 *
 * @param display_id The ID of the virtual display.
 * @param is_dirty True to mark as dirty (needs redraw), False to mark as clean.
 */
SITAPI void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetVirtualDisplayDirty"); return; }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationSetVirtualDisplayDirty: invalid display_id");
        return;
    }
    sit_render.virtual_display_slots[display_id].is_dirty = is_dirty;
}

/**
 * @brief Checks if a virtual display is marked as needing to be re-rendered.
 *
 * Queries the internal 'dirty' flag of the virtual display.
 *
 * @param display_id The ID of the virtual display.
 * @return True if the display is marked dirty, False otherwise or if the ID is invalid.
 */
SITAPI bool SituationIsVirtualDisplayDirty(int display_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationIsVirtualDisplayDirty"); return false; }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationIsVirtualDisplayDirty: invalid display_id");
        return false;
    }
    return sit_render.virtual_display_slots[display_id].is_dirty;
}

/**
 * @brief Gets the time taken for the last `SituationRenderVirtualDisplays` call.
 *
 * Useful for profiling the performance impact of the virtual display compositing pass.
 *
 * @return The time in milliseconds for the last composite operation.
 *         Returns 0.0 if no compositing has occurred yet or if there were no visible VDs.
 */
SITAPI double SituationGetLastVDCompositeTimeMS(void) {
    if (!SituationIsInitialized()) return 0.0;
    return sit_render.last_vd_composite_time_ms;
}

/**
 * @brief Query when a virtual display last received new pixel content.
 *
 * Distinct from `last_update_time_seconds` (VD frame clock) and from compositor timing.
 * Optional output pointers may be NULL.
 */
SITAPI SituationError SituationGetVirtualDisplayUpdateInfo(
    int display_id,
    double* out_last_content_update_time,
    uint64_t* out_last_content_update_frame,
    uint64_t* out_frames_since_update,
    double* out_seconds_since_update)
{
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetVirtualDisplayUpdateInfo");
    }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationGetVirtualDisplayUpdateInfo: invalid display_id");
    }

    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];
    double now = _SitVDGetTimeSeconds();

    if (out_last_content_update_time) *out_last_content_update_time = vd->last_content_update_time;
    if (out_last_content_update_frame) *out_last_content_update_frame = vd->last_content_update_frame;
    if (out_frames_since_update) *out_frames_since_update = vd->frame_count - vd->last_content_update_frame;
    if (out_seconds_since_update) *out_seconds_since_update = now - vd->last_content_update_time;

    return SITUATION_SUCCESS;
}

/**
 * @brief Sets how long without a content write before a VD is treated as idle (Phase 2 compositor).
 */
SITAPI void SituationSetVirtualDisplayIdleThreshold(int display_id, double threshold_seconds) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetVirtualDisplayIdleThreshold");
        return;
    }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationSetVirtualDisplayIdleThreshold: invalid display_id");
        return;
    }
    if (threshold_seconds < 0.0) threshold_seconds = 0.0;
    sit_render.virtual_display_slots[display_id].idle_threshold_seconds = threshold_seconds;
}

SITAPI void SituationSetVirtualDisplayFallbackMode(int display_id, SituationVDFallbackMode mode) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetVirtualDisplayFallbackMode");
        return;
    }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationSetVirtualDisplayFallbackMode: invalid display_id");
        return;
    }
    if (mode != SITUATION_VD_FALLBACK_SOLID && mode != SITUATION_VD_FALLBACK_COLORBURST) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationSetVirtualDisplayFallbackMode: invalid mode");
        return;
    }
    sit_render.virtual_display_slots[display_id].fallback_mode = mode;
}

SITAPI void SituationSetVirtualDisplayFallbackColor(int display_id, ColorRGBA color) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetVirtualDisplayFallbackColor");
        return;
    }
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationSetVirtualDisplayFallbackColor: invalid display_id");
        return;
    }
    sit_render.virtual_display_slots[display_id].fallback_color = color;
}

/**
 * @brief Gets the internal resolution of a virtual display.
 *
 * Retrieves the width and height used when the virtual display was created.
 *
 * @param display_id The ID of the virtual display.
 * @param[out] width A pointer to store the width. Ignored if NULL.
 * @param[out] height A pointer to store the height. Ignored if NULL.
 * @return SITUATION_SUCCESS if the ID is valid and values were retrieved.
 *         Returns SITUATION_ERROR_NOT_INITIALIZED if the library isn't initialized.
 *         Returns SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID if the ID is invalid or not in use.
 */
SITAPI void SituationGetVirtualDisplaySize(int display_id, int* width, int* height) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetVirtualDisplaySize"); if (width) *width = 0; if (height) *height = 0; return; }
    if (!width || !height) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetVirtualDisplaySize: width or height is NULL"); return; }
    if (display_id < 0) { *width = sit_gs.main_window_width; *height = sit_gs.main_window_height; return; }
    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(display_id);
    if (vd) { *width = (int)vd->resolution.x; *height = (int)vd->resolution.y; } else { _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationGetVirtualDisplaySize: display not found"); *width = 0; *height = 0; }
}

#endif // SITUATION_IMPL_VD_H
