import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# Make SituationCmdBeginRenderToDisplay a wrapper around SituationCmdBeginRenderPass
old_wrapper = """SITAPI __attribute__((deprecated("Use SituationCmdBeginRenderPass instead"))) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "CmdBeginRenderToDisplay");
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BEGIN_RENDER_PASS);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

    p->args.begin_pass.display_id = display_id;
    // Capture resolution for thread safety
    p->args.begin_pass.target_w = sit_gs.main_window_width;
    p->args.begin_pass.target_h = sit_gs.main_window_height;

    // Construct info
    memset(&p->args.begin_pass.info, 0, sizeof(SituationRenderPassInfo));
    p->args.begin_pass.info.display_id = display_id;
    p->args.begin_pass.info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    p->args.begin_pass.info.color_attachment.clear.color = clear_color;
    p->args.begin_pass.info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    p->args.begin_pass.info.depth_attachment.clear.depth = 1.0f;

#elif defined(SITUATION_USE_VULKAN)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: CmdBeginRenderToDisplay called\\n");
    printf("Situation [Vulkan Debug]:   display_id=%d, clear_color=(%d,%d,%d,%d)\\n",
           display_id, clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    fflush(stdout);
    #endif

    // Verify parameter
    if (cmd == NULL) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "CmdBeginRenderToDisplay: null command buffer");
        return SITUATION_ERROR_INVALID_PARAM;
    }

    VkFramebuffer target_framebuffer = VK_NULL_HANDLE;
    uint32_t target_w = sit_gs.main_window_width;
    uint32_t target_h = sit_gs.main_window_height;

    // --- 1. Target Resolution ---
    if (display_id == -1) {
        // Main window target.
        uint32_t current_image_index = sit_render.vk.acquired_image_indices[sit_gs.current_frame];
        target_framebuffer = sit_render.vk.main_window_framebuffers[current_image_index];

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   Target: Main Window (w=%d, h=%d), Image Index: %d, FB: %p\\n",
               target_w, target_h, current_image_index, (void*)target_framebuffer);
        fflush(stdout);
        #endif

    } else if (display_id >= 0 && display_id < SITUATION_MAX_VIRTUAL_DISPLAYS) {
        // Virtual display target.
        if (sit_render.virtual_display_slots_used[display_id]) {
            _SituationVirtualDisplaySlot* vd = &sit_render.virtual_display_slots[display_id];
            target_framebuffer = vd->vk.framebuffer;
            target_w = vd->width;
            target_h = vd->height;

            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:   Target: Virtual Display %d (w=%d, h=%d), FB: %p\\n",
                   display_id, target_w, target_h, (void*)target_framebuffer);
            fflush(stdout);
            #endif
        } else {
             _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_DISPLAY_ID, "CmdBeginRenderToDisplay: Invalid or unallocated Virtual Display ID.");
             return SITUATION_ERROR_INVALID_DISPLAY_ID;
        }
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_DISPLAY_ID, "CmdBeginRenderToDisplay: Invalid Virtual Display ID (out of bounds).");
        return SITUATION_ERROR_INVALID_DISPLAY_ID;
    }

    // --- 2. Configure Render Pass Begin Info ---
    VkClearValue clear_values[2];
    // Convert 0-255 ColorRGBA to 0.0-1.0 floats.
    clear_values[0].color.float32[0] = clear_color.r / 255.0f;
    clear_values[0].color.float32[1] = clear_color.g / 255.0f;
    clear_values[0].color.float32[2] = clear_color.b / 255.0f;
    clear_values[0].color.float32[3] = clear_color.a / 255.0f;
    // Clear depth to 1.0, stencil to 0.
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = sit_render.vk.main_window_render_pass; // In our current architecture, the same render pass object is compatible with VD framebuffers as long as formats match.
    render_pass_info.framebuffer = target_framebuffer;
    render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
    render_pass_info.renderArea.extent = (VkExtent2D){target_w, target_h};
    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    // --- 3. Execute Command ---
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Calling vkCmdBeginRenderPass...\\n");
    fflush(stdout);
    #endif

    vkCmdBeginRenderPass((VkCommandBuffer)cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   vkCmdBeginRenderPass completed\\n");
    fflush(stdout);
    #endif

    // --- 4. Setup Dynamic Viewport/Scissor ---
    // The viewport size is dynamic based on the render target.
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)target_w,
        .height = (float)target_h,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);

    // Set a default full-screen scissor rectangle matching the viewport.
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {target_w, target_h}
    };
    vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

#endif

    return SITUATION_SUCCESS;
}"""

new_wrapper = """SITAPI __attribute__((deprecated("Use SituationCmdBeginRenderPass instead"))) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;

    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;

    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_STORE;
    info.depth_attachment.clear.depth = 1.0f;
    info.depth_attachment.clear.stencil = 0;

    return SituationCmdBeginRenderPass(cmd, &info);
}"""

# Do a smart replacement since formatting might differ slightly
start_idx = content.find("SITAPI __attribute__((deprecated(\"Use SituationCmdBeginRenderPass instead\"))) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color) {")
end_idx = content.find("}", content.find("return SITUATION_SUCCESS;", start_idx)) + 1

if start_idx != -1 and end_idx != -1:
    content = content[:start_idx] + new_wrapper + content[end_idx:]

with open("situation_impl.h", "w") as f:
    f.write(content)
