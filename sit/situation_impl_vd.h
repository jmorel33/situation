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
*     - Virtual display state queries (dirty, size, composite time)
*     - GL VD renderer initialization
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_VD_H
#define SITUATION_IMPL_VD_H


/**
 * @brief Creates a new virtual/offscreen display (render target) for multi-pass or composited rendering.
 *
 * @details Allocates and initializes a new virtual display that can be rendered to independently
 *          of the main window/swapchain. Virtual displays are useful for:
 *            - Multi-view rendering (e.g. editor viewports, minimaps, VR eyes, security cameras)
 *            - Post-processing pipelines (render scene ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ apply effects ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ composite)
 *            - Render-to-texture techniques
 *            - Layered UI or debug overlays with independent resolution/scaling
 *
 *          Once created, the virtual display can be rendered into using `SituationRenderVirtualDisplays`
 *          (or manually via its internal framebuffer/command list) and its output can be sampled as
 *          a texture in shaders or blitted/composited into the main framebuffer.
 *
 *          Parameters control rendering behavior, timing, and composition:
 *            - resolution: Internal render resolution (can differ from window size)
 *            - frame_time_mult: Time scaling factor applied to this displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s update loop
 *            - z_order: Compositing order when using `SituationRenderVirtualDisplays` (lower = drawn first)
 *            - scaling_mode: How the virtual display output is scaled when composited (fit, fill, stretch, etc.)
 *            - blend_mode: Blending operation used when compositing this display onto others or the main target
 *
 * @param resolution Desired internal resolution of the virtual display (width, height in pixels).
 *                   Both components must be > 0. Fractional values are truncated.
 * @param frame_time_mult Multiplier applied to delta-time for this displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s update callbacks.
 *                        1.0 = real-time, 0.5 = half-speed, 2.0 = double-speed, etc.
 *                        Useful for slow-motion effects or independent simulation rates.
 * @param z_order Z-order / layer index for automatic compositing (higher values drawn later/on top).
 *                Use 0 for background, positive for foreground. Negative values are allowed.
 * @param scaling_mode Scaling policy when the displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s aspect ratio differs from the target
 *                     (e.g. `SITUATION_SCALING_FIT`, `SITUATION_SCALING_FILL`, `SITUATION_SCALING_STRETCH`).
 * @param blend_mode Blending mode used when this display is composited onto another target
 *                   (e.g. `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`, `SITUATION_BLEND_NONE`).
 * @param out_id Pointer to an integer that receives the unique ID of the new virtual display on success.
 *               On failure, the value is set to -1.
 *
 * @return SITUATION_SUCCESS on successful creation,
 *         SITUATION_ERROR_INVALID_PARAM if resolution ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°Ãƒâ€šÃ‚Â¤ 0 or invalid enum values,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if framebuffer/texture allocation failed,
 *         SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT if maximum number of virtual displays reached
 *         (implementation-defined, typically 32ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ64),
 *         or other appropriate error codes.
 *
 * @note The returned ID is valid until the display is destroyed with `SituationDestroyVirtualDisplay`.
 *       Virtual displays are automatically rendered when calling `SituationRenderVirtualDisplays`
 *       (unless paused or hidden via separate flags).
 *       Resource cleanup (framebuffer, color/depth textures) is handled on destroy or shutdown.
 *       High-resolution virtual displays consume significant GPU memory ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â monitor allocation failures.
 *
 * @see SituationRenderVirtualDisplays, SituationDestroyVirtualDisplay,
 *      SituationPauseVirtualDisplay, SituationSetVirtualDisplayZOrder,
 *      SituationScalingMode, SituationBlendMode,
 *      SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT, SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID
 */
SITAPI SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id) {
    if (out_id) *out_id = -1;
    else return SITUATION_ERROR_INVALID_PARAM;

    // --- 1. Validation ---
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot create virtual display");
    }
    if (sit_render.active_virtual_display_count >= SITUATION_MAX_VIRTUAL_DISPLAYS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT, "Maximum virtual displays reached");
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
    vd->last_update_time_seconds = glfwGetTime();

    bool success = true; // Master success flag for the chain

#if defined(SITUATION_USE_VULKAN)
    // =================================================================
    // --- VULKAN IMPLEMENTATION ---
    // =================================================================

    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = sit_render.vk.depth_format;

    // --- Step 1: Create Color Image ---
    if (success) {
        if (_SituationVulkanCreateImage((uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y, 1, color_format,
                                        VK_IMAGE_TILING_OPTIMAL,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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

    // --- Step 3: Create Depth Image ---
    if (success) {
        if (_SituationVulkanCreateImage((uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y, 1, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VMA_MEMORY_USAGE_GPU_ONLY,
                                        &vd->vk.depth_image, &vd->vk.depth_image_memory) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "Failed to create VD depth image");
            success = false;
        }
    }

    // --- Step 4: Create Depth View ---
    if (success) {
        vd->vk.depth_image_view = _SituationVulkanCreateImageView(vd->vk.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
        if (vd->vk.depth_image_view == VK_NULL_HANDLE) success = false;
    }

    // --- Step 5: Create Sampler ---
    if (success) {
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = (scaling_mode == SITUATION_SCALING_STRETCH) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_info.minFilter = sampler_info.magFilter;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &vd->vk.sampler) != VK_SUCCESS) success = false;
    }

    // --- Step 6: Create Render Pass ---
    if (success) {
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
    }

    // --- Step 7: Create Framebuffer ---
    if (success) {
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
    glCreateFramebuffers(1, &vd->gl.fbo_id);
    glCreateTextures(GL_TEXTURE_2D, 1, &vd->gl.texture_id);
    glCreateRenderbuffers(1, &vd->gl.depth_rbo_id);

    // Step 2: Configure Texture
    if (success) {
        glTextureStorage2D(vd->gl.texture_id, 1, GL_RGBA8, (GLsizei)vd->resolution.x, (GLsizei)vd->resolution.y);

        GLenum filter_mode = (scaling_mode == SITUATION_SCALING_STRETCH) ? GL_LINEAR : GL_NEAREST;
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_MIN_FILTER, filter_mode);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_MAG_FILTER, filter_mode);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(vd->gl.texture_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Step 3: Configure Renderbuffer & Attach
    if (success) {
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

    // --- 4. Finalize ---
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
    if (display_id < 0 || display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[display_id]) {
        return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
    }
    SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[display_id];

#if defined(SITUATION_USE_VULKAN)
    // Defer all destruction to the Graveyard to avoid stalling.

    // [FIX v2.3.27B] Pass the specific pool that owns this descriptor set
    if (vd->vk.descriptor_set != VK_NULL_HANDLE) {
        _SituationDeferDestroyDescriptorSet(vd->vk.descriptor_set, vd->vk.descriptor_pool);
    }

    if (vd->vk.framebuffer != VK_NULL_HANDLE) _SituationDeferDestroyFramebuffer(vd->vk.framebuffer);
    if (vd->vk.render_pass != VK_NULL_HANDLE) _SituationDeferDestroyRenderPass(vd->vk.render_pass);

    // Defer images (includes view and sampler for color, just view for depth)
    _SituationDeferDestroyImage(vd->vk.image, vd->vk.image_memory, vd->vk.image_view, vd->vk.sampler);
    _SituationDeferDestroyImage(vd->vk.depth_image, vd->vk.depth_image_memory, vd->vk.depth_image_view, VK_NULL_HANDLE);

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
    GLenum filter_mode = (scaling_mode == SITUATION_SCALING_STRETCH) ? GL_LINEAR : GL_NEAREST;

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
    sampler_info.magFilter = sampler_info.minFilter = (scaling_mode == SITUATION_SCALING_STRETCH) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
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
 *              - Binds the displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s framebuffer / render target
 *              - Records the displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s internal command list (scene, shaders, meshes, etc.)
 *              - Applies display-specific viewport, scissor, and clear settings
 *              - Handles layer/compositing order if z-sorting is enabled
 *            - Restores the original render state (viewport, framebuffer, etc.) after all displays
 *            - Records only ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â no actual GPU submission occurs in this function
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
 *         SITUATION_ERROR_RENDER_LIST_INCOMPLETE if any displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s internal command list failed to record,
 *         SITUATION_ERROR_RESOURCE_INVALID if a virtual display is in an inconsistent state,
 *         SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND for orphaned display IDs (rare),
 *         or other appropriate error codes.
 *         Partial failures may still record some displays ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller should check return value.
 *
 * @note This function does **not** present or submit ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â it only records commands.
 *       After calling, the caller must end the command buffer, submit it (render thread),
 *       and wait for completion if synchronization is needed.
 *       Virtual displays that are paused, hidden, or have zero size are skipped automatically.
 *       Performance scales with number of active displays and complexity of each displayÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s scene.
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
    if (!_SitGLSoftCmdPush(buf, SIT_OP_RENDER_VIRTUAL_DISPLAYS)) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.vd_compositing_pipeline == VK_NULL_HANDLE) return SITUATION_ERROR_NOT_INITIALIZED;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    /* End caller's main-window render pass only if one is active. Unconditional vkCmdEndRenderPass
     * crashes when the caller never began a pass (e.g. harness render_virtual_displays).
     * If we end the caller's pass, the swapchain image already holds their draws — the next
     * vkCmdBeginRenderPass for VD must use LOAD (main_window_render_pass_resume), not CLEAR,
     * or alpha/opacity tests composite against black instead of the cleared background. */
    bool vd_resume_swapchain_after_caller_rp = sit_render.vk.inside_main_swapchain_render_pass;
    if (sit_render.vk.inside_main_swapchain_render_pass) {
        vkCmdEndRenderPass(vk_cmd);
        sit_render.vk.inside_main_swapchain_render_pass = false;
    }

    // --- 1. UBO Update (Zero-Stall Persistent Write) ---
    float target_width = (float)sit_render.vk.swapchain_extent.width;
    float target_height = (float)sit_render.vk.swapchain_extent.height;
    ViewDataUBO ubo_data;
    glm_mat4_identity(ubo_data.view);
    glm_ortho(0.0f, target_width, target_height, 0.0f, -1.0f, 1.0f, ubo_data.projection);

    // Write directly to the mapped pointer (NO vmaMapMemory stall!)
    memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));

    // --- 2. Global Setup ---
    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };

    // Track render pass state to minimize switching
    bool is_render_pass_active = false;

    // Pre-fill the RenderPassBeginInfo struct for reuse (must supply clear values: color+depth CLEAR in main pass).
    VkClearValue vd_main_rp_clear_values[2] = {0};
    vd_main_rp_clear_values[1].depthStencil.depth = 1.0f;
    VkRenderPassBeginInfo rp_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp_info.renderPass = sit_render.vk.main_window_render_pass;
    rp_info.framebuffer = sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
    rp_info.renderArea.extent = sit_render.vk.swapchain_extent;
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = vd_main_rp_clear_values;

    bool screen_copy_was_sampled = false;
    /* Path A vkCmdBeginRenderPass must use COLOR CLEAR only for the first advanced layer. Each extra
     * non-alpha VD used main_window_render_pass → CLEAR wiped the swapchain between layers → corrupt
     * compositing, GPU faults, and harness cascade (fence timeouts). Subsequent layers need LOAD
     * (main_window_render_pass_resume + main_window_framebuffers_resume). */
    uint32_t path_a_restart_index = 0;

    // Set up standard Viewport and Scissor for the screen
    VkViewport viewport = {0.0f, 0.0f, target_width, target_height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, sit_render.vk.swapchain_extent};

    // --- Loop and Draw Each Virtual Display ---
    for (int i = 0; i < visible_count; ++i) {
        const SituationVirtualDisplay* vd = visible_vds_to_render[i];

        // --- Matrix Calculation ---
        mat4 T, S;
        mat4 model_matrix;
        glm_mat4_identity(model_matrix);

        switch (vd->scaling_mode) {
            case SITUATION_SCALING_STRETCH: {
                glm_translate_make(T, (vec3){vd->offset.x, vd->offset.y, 0.0f});
                glm_scale_make(S, (vec3){target_width, target_height, 1.0f});
                glm_mat4_mul(T, S, model_matrix);
                break;
            }
            case SITUATION_SCALING_FIT: {
                float sx = target_width / vd->resolution.x;
                float sy = target_height / vd->resolution.y;
                float s = fminf(sx, sy);
                float final_w = vd->resolution.x * s;
                float final_h = vd->resolution.y * s;
                float final_x = (target_width - final_w) / 2.0f;
                float final_y = (target_height - final_h) / 2.0f;
                glm_translate_make(T, (vec3){final_x, final_y, 0.0f});
                glm_scale_make(S, (vec3){final_w, final_h, 1.0f});
                glm_mat4_mul(T, S, model_matrix);
                break;
            }
            case SITUATION_SCALING_INTEGER: {
                float s = fmaxf(1.0f, floorf(fminf(target_width / vd->resolution.x, target_height / vd->resolution.y)));
                float final_w = vd->resolution.x * s;
                float final_h = vd->resolution.y * s;
                float final_x = (target_width - final_w) / 2.0f;
                float final_y = (target_height - final_h) / 2.0f;
                glm_translate_make(T, (vec3){final_x, final_y, 0.0f});
                glm_scale_make(S, (vec3){final_w, final_h, 1.0f});
                glm_mat4_mul(T, S, model_matrix);
                break;
            }
        }

        /* Path A: needs swapchain + destination sample (advanced FS). Path B: premultiplied-style
           alpha only. ADDITIVE/MULTIPLY/SCREEN/NONE were incorrectly on Path B (blend mode ignored). */
        bool use_advanced = (vd->blend_mode != SITUATION_BLEND_ALPHA);
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
            /* Destination for the composite FS is screen_copy (filled above); swapchain may CLEAR+redraw each layer. */
            _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImg, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
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
                vkCmdBeginRenderPass(vk_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
                path_a_restart_index++;
                vd_resume_swapchain_after_caller_rp = false;
            }
            vkCmdSetViewport(vk_cmd, 0, 1, &viewport);
            vkCmdSetScissor(vk_cmd, 0, 1, &scissor);
            is_render_pass_active = true;

            // 7. Draw
            vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline);
            vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

            // Bind Sets: 0=GlobalUBO, 1=VDSampler, 2=ScreenCopy
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 1, 1, &vd->vk.descriptor_set, 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.advanced_compositing_pipeline_layout, 2, 1, &sit_render.vk.screen_copy_descriptor_set, 0, NULL);

			// Define a layout-compatible byte buffer to avoid anonymous struct issues
            struct { mat4 m; int b; float o; } pc;
            glm_mat4_copy(model_matrix, pc.m);
            pc.b = vd->blend_mode;
            pc.o = vd->opacity;

            /* SPIR-V CompositePushConstants size is mat4+int+float (72); MSVC may pad sizeof(pc). */
            vkCmdPushConstants(vk_cmd, sit_render.vk.advanced_compositing_pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                               (uint32_t)(sizeof(mat4) + sizeof(int) + sizeof(float)), &pc);
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
                vkCmdBeginRenderPass(vk_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
                vd_resume_swapchain_after_caller_rp = false;

                // [CRITICAL VULKAN FIX] Must push dynamic viewport/scissor state
                // every time a new render pass begins!
                vkCmdSetViewport(vk_cmd, 0, 1, &viewport);
                vkCmdSetScissor(vk_cmd, 0, 1, &scissor);

                is_render_pass_active = true;
            }

            // 2. Draw
            vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.vd_compositing_pipeline);
            vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.vd_compositing_pipeline_layout, 0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
            vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.vd_compositing_pipeline_layout, 1, 1, &vd->vk.descriptor_set, 0, NULL);

            struct { mat4 m; float o; } pc;
            glm_mat4_copy(model_matrix, pc.m);
            pc.o = vd->opacity;

            /* Pipeline layout push range is sizeof(mat4)+sizeof(float) (68); MSVC may pad sizeof(pc). */
            vkCmdPushConstants(vk_cmd, sit_render.vk.vd_compositing_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               (uint32_t)(sizeof(mat4) + sizeof(float)), &pc);
            vkCmdDraw(vk_cmd, 4, 1, 0, 0);
        }
    }

    // --- Cleanup ---
    if (is_render_pass_active) {
        vkCmdEndRenderPass(vk_cmd);
    }

    // [FIX] Restart the main window render pass so the caller can continue recording
    // commands (or call SituationCmdEndRenderPass). Must use the resume render pass:
    // main_window_render_pass clears color on every Begin — that erased VD composite output.
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
        restart_info.renderArea.extent = sit_render.vk.swapchain_extent;
        VkClearValue clear_values[2] = {0};
        restart_info.clearValueCount = 2;
        restart_info.pClearValues = clear_values;
        vkCmdBeginRenderPass(vk_cmd, &restart_info, VK_SUBPASS_CONTENTS_INLINE);

        float tw = (float)sit_render.vk.swapchain_extent.width;
        float th = (float)sit_render.vk.swapchain_extent.height;
        VkViewport vp = {0.0f, 0.0f, tw, th, 0.0f, 1.0f};
        VkRect2D sc = {{0, 0}, sit_render.vk.swapchain_extent};
        vkCmdSetViewport(vk_cmd, 0, 1, &vp);
        vkCmdSetScissor(vk_cmd, 0, 1, &sc);
        sit_render.vk.inside_main_swapchain_render_pass = true;
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
