import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# there is a logic error:
#     } else if (info->display_id >= 0 && info->display_id < SITUATION_MAX_VIRTUAL_DISPLAYS) { ... }
#     else { return ... }
# But wait, there is a missing closing brace or extra else in the generated string.
# Wait, let's fix the logic for resolving framebuffer.

replacement = """
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
"""

with open("situation_impl.h", "w") as f:
    f.write(content)
