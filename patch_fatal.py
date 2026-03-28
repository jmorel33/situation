import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

# Define `_SituationFatalError` safely
fatal_func = """
static void _SituationFatalError(const char* msg) {
    fprintf(stderr, "FATAL ERROR: %s\\n", msg);
    exit(1);
}
"""

if "_SituationFatalError" not in data:
    data = re.sub(r'#if defined\(SITUATION_USE_VULKAN\)', r'#if defined(SITUATION_USE_VULKAN)\n' + fatal_func, data, count=1)

vulkan_patch_single1 = """
    if (submit_result == VK_ERROR_DEVICE_LOST) {
        _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during vkQueueSubmit. GPU crashed or disconnected. Terminating.");
    }
"""

submit1 = '    VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
if "if (submit_result == VK_ERROR_DEVICE_LOST)" not in data:
    data = data.replace(submit1, submit1 + vulkan_patch_single1)

submit2 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
data = data.replace(submit2, submit2 + vulkan_patch_single1)

vulkan_patch_thread = """
        if (submit_result == VK_ERROR_DEVICE_LOST) {
            _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during Render Thread vkQueueSubmit. GPU crashed or disconnected. Terminating.");
        }
"""
submit3 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[frame_index]);'
data = data.replace(submit3, submit3 + vulkan_patch_thread)

# Remove duplicates if run multiple times
data = re.sub(r'(        _SituationFatalError\("Vulkan Device Lost \(VK_ERROR_DEVICE_LOST\) during vkQueueSubmit\. GPU crashed or disconnected\. Terminating\."\);\n    \}\n)\s*if \(submit_result == VK_ERROR_DEVICE_LOST\) \{.*?\n    \}', r'\1', data, flags=re.DOTALL)
data = re.sub(r'(            _SituationFatalError\("Vulkan Device Lost \(VK_ERROR_DEVICE_LOST\) during Render Thread vkQueueSubmit\. GPU crashed or disconnected\. Terminating\."\);\n        \}\n)\s*if \(submit_result == VK_ERROR_DEVICE_LOST\) \{.*?\n        \}', r'\1', data, flags=re.DOTALL)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)
