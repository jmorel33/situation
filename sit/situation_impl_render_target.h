/***************************************************************************************************
*
*   situation_impl_render_target.h - User offscreen render targets (Phase 3c)
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDER_TARGET_H
#define SITUATION_IMPL_RENDER_TARGET_H

static void _SitRenderPassSetRenderTargetLayoutHint(SituationRenderTarget rt, SituationTextureLayout layout) {
    _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(rt);
    if (!rts || rts->texture_slot_index < 0 || rts->texture_slot_index >= SITUATION_MAX_TEXTURES) {
        return;
    }
    _SituationTextureSlot* tex = &sit_render.texture_registry[rts->texture_slot_index];
    if (tex->is_active) {
        _SitTextureSetLayoutHint(tex, layout);
    }
}

SITAPI SituationRenderPassInfo SituationRenderPassInfoForRenderTarget(SituationRenderTarget render_target, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = -1;
    info.render_target = render_target;
    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;
    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    info.depth_attachment.clear.depth = 1.0f;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_DONT_CARE;
    info.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    return info;
}

static SituationError _SitRenderTargetRegisterColorTexture(_SituationRenderTargetSlot* rts, SituationRenderTarget* out_rt) {
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
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "SituationCreateRenderTarget: texture registry full.");
    }

    _SituationTextureSlot* slot = &sit_render.texture_registry[slot_idx];
    slot->generation++;
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }
    slot->is_active = true;
    mtx_unlock(&sit_render.resource_registry_mutex);

    slot->width = rts->width;
    slot->height = rts->height;
    slot->mip_levels = 1;
    slot->format_api = SIT_TEXTURE_FORMAT_RGBA8_UNORM;
    slot->usage_flags = (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_SRC);
    slot->wrap_s = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
    slot->wrap_t = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
    slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
    slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
    slot->layout_hint = SITUATION_TEXTURE_LAYOUT_UNDEFINED;
    slot->bindless_handle = 0;
    slot->source_path = NULL;
    slot->mod_time = 0;

#if defined(SITUATION_USE_VULKAN)
    slot->image = rts->color_image;
    slot->format = VK_FORMAT_R8G8B8A8_UNORM;
    slot->image_view = rts->color_view;
    slot->allocation = rts->color_memory;
    slot->descriptor_set = VK_NULL_HANDLE;
    slot->descriptor_pool = VK_NULL_HANDLE;
    slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
    slot->single_sampler_descriptor_pool = VK_NULL_HANDLE;

    VkSamplerCreateInfo sampler_info = {0};
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
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &slot->sampler) != VK_SUCCESS) {
        slot->is_active = false;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED,
            "SituationCreateRenderTarget: vkCreateSampler failed.");
    }

    {
        SituationError bindless_err = _SitVulkanWriteSlotToGlobalBindlessSet(
            slot, (uint32_t)slot_idx, "SituationCreateRenderTarget");
        if (bindless_err != SITUATION_SUCCESS) {
            vkDestroySampler(sit_render.vk.device, slot->sampler, NULL);
            slot->sampler = VK_NULL_HANDLE;
            slot->is_active = false;
            return bindless_err;
        }
    }
#elif defined(SITUATION_USE_OPENGL)
    slot->gl_texture_id = rts->color_texture_id;
    slot->internal_format = GL_RGBA8;
    slot->gl_bindless_handle = 0;
    slot->gl_image_binding_unit = -1;
#endif

    rts->texture_slot_index = slot_idx;
    out_rt->slot_index = (uint32_t)(rts - sit_render.render_target_slots);
    out_rt->generation = rts->generation;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCreateRenderTarget(const SituationRenderTargetDesc* desc, SituationRenderTarget* out_rt) {
    if (!desc || !out_rt) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_rt = SITUATION_NULL_RENDER_TARGET;
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (desc->width < 1 || desc->height < 1) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateRenderTarget: width/height must be >= 1.");
    }
    if (desc->msaa_samples > 1) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCreateRenderTarget: msaa_samples > 1 deferred to MSAA slice.");
    }
    if (desc->want_stencil) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCreateRenderTarget: stencil attachments not implemented yet.");
    }
    if (sit_render.active_render_target_count >= SITUATION_MAX_RENDER_TARGETS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationCreateRenderTarget: max render targets reached.");
    }

    int new_slot = -1;
    for (int i = 0; i < SITUATION_MAX_RENDER_TARGETS; ++i) {
        if (!sit_render.render_target_slots_used[i]) {
            new_slot = i;
            break;
        }
    }
    if (new_slot < 0) {
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }

    _SituationRenderTargetSlot* rts = &sit_render.render_target_slots[new_slot];
    memset(rts, 0, sizeof(*rts));
    rts->width = desc->width;
    rts->height = desc->height;
    rts->has_depth = desc->want_depth;
    rts->texture_slot_index = -1;
    rts->generation++;
    if (rts->generation == 0u) {
        rts->generation = 1u;
    }

    bool success = true;
#if defined(SITUATION_USE_VULKAN)
    rts->color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageUsageFlags color_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (_SituationVulkanCreateImage((uint32_t)desc->width, (uint32_t)desc->height, 1, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_TILING_OPTIMAL, color_usage, VMA_MEMORY_USAGE_GPU_ONLY,
                                    VK_SAMPLE_COUNT_1_BIT,
                                    &rts->color_image, &rts->color_memory) != SITUATION_SUCCESS) {
        success = false;
    }
    if (success) {
        rts->color_view = _SituationVulkanCreateImageView(rts->color_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        if (rts->color_view == VK_NULL_HANDLE) {
            success = false;
        }
    }
    if (success && rts->has_depth) {
        if (_SituationVulkanCreateImage((uint32_t)desc->width, (uint32_t)desc->height, 1, VK_FORMAT_D24_UNORM_S8_UINT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VMA_MEMORY_USAGE_GPU_ONLY, VK_SAMPLE_COUNT_1_BIT,
                                        &rts->depth_image, &rts->depth_memory) != SITUATION_SUCCESS) {
            success = false;
        }
    }
    if (success && rts->has_depth) {
        rts->depth_view = _SituationVulkanCreateImageView(rts->depth_image, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
        if (rts->depth_view == VK_NULL_HANDLE) {
            success = false;
        }
    }
#elif defined(SITUATION_USE_OPENGL)
    glCreateFramebuffers(1, &rts->fbo_id);
    glCreateTextures(GL_TEXTURE_2D, 1, &rts->color_texture_id);
    if (rts->has_depth) {
        glCreateRenderbuffers(1, &rts->depth_rbo_id);
    }
    if (success) {
        glTextureStorage2D(rts->color_texture_id, 1, GL_RGBA8, desc->width, desc->height);
        glNamedFramebufferTexture(rts->fbo_id, GL_COLOR_ATTACHMENT0, rts->color_texture_id, 0);
        if (rts->has_depth) {
            glNamedRenderbufferStorage(rts->depth_rbo_id, GL_DEPTH_COMPONENT24, desc->width, desc->height);
            glNamedFramebufferRenderbuffer(rts->fbo_id, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rts->depth_rbo_id);
        }
        if (glCheckNamedFramebufferStatus(rts->fbo_id, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            success = false;
        }
    }
    SIT_CHECK_GL_ERROR();
    if (sit_render.gl.last_error != GL_NO_ERROR) {
        success = false;
    }
#endif

    if (!success) {
#if defined(SITUATION_USE_VULKAN)
        if (rts->depth_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, rts->depth_view, NULL);
        if (rts->depth_image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, rts->depth_image, rts->depth_memory);
        if (rts->color_view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, rts->color_view, NULL);
        if (rts->color_image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, rts->color_image, rts->color_memory);
#elif defined(SITUATION_USE_OPENGL)
        if (rts->color_texture_id != 0) glDeleteTextures(1, &rts->color_texture_id);
        if (rts->depth_rbo_id != 0) glDeleteRenderbuffers(1, &rts->depth_rbo_id);
        if (rts->fbo_id != 0) glDeleteFramebuffers(1, &rts->fbo_id);
#endif
        memset(rts, 0, sizeof(*rts));
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_TARGET_CREATE_FAILED, "SituationCreateRenderTarget: GPU allocation failed.");
    }

    SituationError reg_err = _SitRenderTargetRegisterColorTexture(rts, out_rt);
    if (reg_err != SITUATION_SUCCESS) {
        SituationDestroyRenderTarget(out_rt);
        return reg_err;
    }

    rts->is_active = true;
    sit_render.render_target_slots_used[new_slot] = true;
    sit_render.active_render_target_count++;
    return SITUATION_SUCCESS;
}

SITAPI void SituationDestroyRenderTarget(SituationRenderTarget* rt) {
    if (!rt || _SitRenderTargetHandleIsNull(*rt)) {
        return;
    }
    _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(*rt);
    if (!rts) {
        *rt = SITUATION_NULL_RENDER_TARGET;
        return;
    }
    uint32_t slot_index = rt->slot_index;

    if (rts->texture_slot_index >= 0 && rts->texture_slot_index < SITUATION_MAX_TEXTURES) {
        mtx_lock(&sit_render.resource_registry_mutex);
        _SituationTextureSlot* tex = &sit_render.texture_registry[rts->texture_slot_index];
#if defined(SITUATION_USE_VULKAN)
        VkSampler deferred_sampler = tex->sampler;
        _SituationDeferDestroyImage(rts->color_image, rts->color_memory, rts->color_view, deferred_sampler);
#else
        (void)tex;
#endif
        memset(tex, 0, sizeof(*tex));
        tex->is_active = false;
        mtx_unlock(&sit_render.resource_registry_mutex);
    }

#if defined(SITUATION_USE_VULKAN)
    if (rts->texture_slot_index < 0 || rts->texture_slot_index >= SITUATION_MAX_TEXTURES) {
        _SituationDeferDestroyImage(rts->color_image, rts->color_memory, rts->color_view, VK_NULL_HANDLE);
    }
    if (rts->depth_image != VK_NULL_HANDLE) {
        _SituationDeferDestroyImage(rts->depth_image, rts->depth_memory, rts->depth_view, VK_NULL_HANDLE);
    }
#elif defined(SITUATION_USE_OPENGL)
    if (rts->color_texture_id != 0) glDeleteTextures(1, &rts->color_texture_id);
    if (rts->depth_rbo_id != 0) glDeleteRenderbuffers(1, &rts->depth_rbo_id);
    if (rts->fbo_id != 0) glDeleteFramebuffers(1, &rts->fbo_id);
#endif

    memset(rts, 0, sizeof(*rts));
    sit_render.render_target_slots_used[slot_index] = false;
    if (sit_render.active_render_target_count > 0) {
        sit_render.active_render_target_count--;
    }
    *rt = SITUATION_NULL_RENDER_TARGET;
}

SITAPI SituationError SituationGetRenderTargetTexture(SituationRenderTarget rt, SituationTexture* out_tex) {
    if (!out_tex) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_tex = (SituationTexture){0};
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(rt);
    if (!rts) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_TARGET_INVALID, "SituationGetRenderTargetTexture: invalid render target.");
    }
    if (rts->texture_slot_index < 0 || rts->texture_slot_index >= SITUATION_MAX_TEXTURES) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGetRenderTargetTexture: texture slot inactive.");
    }
    _SituationTextureSlot* slot = &sit_render.texture_registry[rts->texture_slot_index];
    if (!slot->is_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGetRenderTargetTexture: texture slot inactive.");
    }
    out_tex->slot_index = (uint32_t)rts->texture_slot_index;
    out_tex->generation = slot->generation;
    out_tex->width = slot->width;
    out_tex->height = slot->height;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationReadRenderTarget(SituationRenderTarget rt, const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes) {
    _SituationFlushRenderThread();
    if (!dst_pixels) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!_SitGetRenderTargetSlot(rt)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_TARGET_INVALID, "SituationReadRenderTarget: invalid render target.");
    }

    SituationTexture tex = {0};
    SIT_RETURN_IF_ERR(SituationGetRenderTargetTexture(rt, &tex));

    SituationTextureReadbackDesc tex_desc = {0};
    tex_desc.format = SIT_TEXTURE_READ_RGBA8;
    if (desc) {
        tex_desc.region.x = desc->x;
        tex_desc.region.y = desc->y;
        tex_desc.region.width = desc->width;
        tex_desc.region.height = desc->height;
        tex_desc.dst_row_pitch_bytes = desc->dst_row_pitch_bytes;
    }
    _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(rt);
    if (tex_desc.region.width <= 0) {
        tex_desc.region.width = rts->width;
    }
    if (tex_desc.region.height <= 0) {
        tex_desc.region.height = rts->height;
    }
    if (tex_desc.dst_row_pitch_bytes == 0) {
        tex_desc.dst_row_pitch_bytes = (size_t)tex_desc.region.width * 4u;
    }
    return SituationReadTexture(tex, &tex_desc, dst_pixels, dst_size_bytes);
}

#endif /* SITUATION_IMPL_RENDER_TARGET_H */
