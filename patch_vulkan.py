import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

# Define `_SituationFatalError`
fatal_func = """
static void _SituationFatalError(const char* msg) {
    fprintf(stderr, "FATAL ERROR: %s\\n", msg);
    // On systems with GUI, you could add OS-specific dialogs here without including headers (e.g. system("zenity ..."))
    // But since we are restricted from including windows.h to keep single-header clean, stderr is safest cross-platform.
    exit(1);
}
"""

if "_SituationFatalError" not in data:
    # Insert before `_SituationSubmitGraphics` (or near the top of vulkan utils)
    data = re.sub(r'#if defined\(SITUATION_USE_VULKAN\)', r'#if defined(SITUATION_USE_VULKAN)\n' + fatal_func, data, count=1)

# Now find `vkQueueSubmit` in `_SituationSubmitGraphics`
# It looks like:
#    VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
#    #ifdef SITUATION_VULKAN_DEBUG
#    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
#    fflush(stdout);
#    #endif
#    if (submit_result == VK_ERROR_DEVICE_LOST) { ... }

vulkan_patch_single1 = """
    if (submit_result == VK_ERROR_DEVICE_LOST) {
        _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during vkQueueSubmit. GPU crashed or disconnected. Terminating.");
    }
"""

# Let's use string replace for `_SituationSubmitGraphics`
submit1 = '    VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
if submit1 in data:
    data = data.replace(submit1, submit1 + vulkan_patch_single1)

# `SituationEndFrame` fallback (around 14409)
submit2 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
if submit2 in data:
    data = data.replace(submit2, submit2 + vulkan_patch_single1)

# `_SituationRenderThreadEntry` (around 31307)
vulkan_patch_thread = """
        if (submit_result == VK_ERROR_DEVICE_LOST) {
            _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during Render Thread vkQueueSubmit. GPU crashed or disconnected. Terminating.");
        }
"""
submit3 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[frame_index]);'
if submit3 in data:
    data = data.replace(submit3, submit3 + vulkan_patch_thread)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)
