import re

with open("situation_impl.h", "r") as f:
    content = f.read()

vulkan_code = """
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    // 1. Resolve or Create the Render Pass
    VkRenderPass render_pass = _SituationVulkanGetOrCreateRenderPass(&sit_render.vk, info);
    if (render_pass == VK_NULL_HANDLE) {
        return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
    }

    // 2. Resolve the Target Framebuffer
    VkFramebuffer target_framebuffer = VK_NULL_HANDLE;
    uint32_t target_w = sit_gs.main_window_width;
    uint32_t target_h = sit_gs.main_window_height;

    if (info->display_id == -1) {
        // Target is main window swapchain
        uint32_t current_image_index = sit_render.vk.acquired_image_indices[sit_gs.current_frame];
        target_framebuffer = sit_render.vk.main_window_framebuffers[current_image_index];
    } else if (info->display_id >= 0 && info->display_id < SITUATION_MAX_VIRTUAL_DISPLAYS) {
        // Target is a Virtual Display
        if (sit_render.virtual_display_slots_used[info->display_id]) {
            _SituationVirtualDisplaySlot* vd = &sit_render.virtual_display_slots[info->display_id];
            target_framebuffer = vd->vk.framebuffer;
            target_w = vd->width;
            target_h = vd->height;
        } else {
             _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_DISPLAY_ID, "Target Virtual Display is not allocated.");
             return SITUATION_ERROR_INVALID_DISPLAY_ID;
        }
    } else {
         _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_DISPLAY_ID, "Invalid display ID.");
         return SITUATION_ERROR_INVALID_DISPLAY_ID;
    }

    // 3. Configure Clear Values
    VkClearValue clear_values[2] = {0};

    // Color clear
    clear_values[0].color.float32[0] = info->color_attachment.clear.color.r / 255.0f;
    clear_values[0].color.float32[1] = info->color_attachment.clear.color.g / 255.0f;
    clear_values[0].color.float32[2] = info->color_attachment.clear.color.b / 255.0f;
    clear_values[0].color.float32[3] = info->color_attachment.clear.color.a / 255.0f;

    // Depth/Stencil clear
    clear_values[1].depthStencil.depth = info->depth_attachment.clear.depth;
    clear_values[1].depthStencil.stencil = info->depth_attachment.clear.stencil;

    // 4. Begin the Render Pass
    VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.renderPass = render_pass;
    begin_info.framebuffer = target_framebuffer;
    begin_info.renderArea.offset = (VkOffset2D){0, 0};
    begin_info.renderArea.extent = (VkExtent2D){target_w, target_h};
    begin_info.clearValueCount = 2;
    begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(vk_cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // 5. Inject Viewport and Scissor
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)target_w,
        .height = (float)target_h,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(vk_cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {target_w, target_h}
    };
    vkCmdSetScissor(vk_cmd, 0, 1, &scissor);

    return SITUATION_SUCCESS;
#endif
"""

old_vulkan = """#elif defined(SITUATION_USE_VULKAN)
    // In the current architecture, SituationCmdBeginRenderToDisplay handles the
    // standard "Clear and Render" pass setup using the default RenderPass objects.
    // Supporting LOAD_OP_LOAD would require creating/caching separate VkRenderPass
    // objects configured with VK_ATTACHMENT_LOAD_OP_LOAD.

    if (info->color_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
        // Delegate to the existing helper for the standard case
        return SituationCmdBeginRenderToDisplay(cmd, info->display_id, info->color_attachment.clear.color);
    } else {
        // TODO: Implement a Render Pass Cache to support LOAD_OP_LOAD
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Non-clearing render passes (SIT_LOAD_OP_LOAD) are not yet implemented for the Vulkan backend.");
        return SITUATION_ERROR_NOT_IMPLEMENTED;
    }
#endif"""

if old_vulkan in content:
    content = content.replace(old_vulkan, vulkan_code)

with open("situation_impl.h", "w") as f:
    f.write(content)
