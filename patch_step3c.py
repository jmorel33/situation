import re

with open("situation_impl.h", "r") as f:
    content = f.read()

inject_cleanup = """
    // Destroy Cached Render Passes
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;
"""

# 1. _SituationCleanupVulkan
cleanup_target = "if (sit_render.vk.render_pass != VK_NULL_HANDLE) {"
idx = content.find("static void _SituationCleanupVulkan(void) {")
if idx != -1:
    actual_idx = content.find(cleanup_target, idx)
    if actual_idx != -1:
        content = content[:actual_idx] + inject_cleanup + "\n    " + content[actual_idx:]

# 2. _SituationVulkanRecreateSwapchain
recreate_target = "if (sit_render.vk.render_pass != VK_NULL_HANDLE) {"
idx = content.find("static void _SituationVulkanCleanupSwapchain(void) {")
if idx != -1:
    actual_idx = content.find(recreate_target, idx)
    if actual_idx != -1:
        inject_recreate = inject_cleanup.replace("Destroy Cached Render Passes", "Destroy Cached Render Passes upon swapchain recreation (formats might change)")
        content = content[:actual_idx] + inject_recreate + "\n    " + content[actual_idx:]


with open("situation_impl.h", "w") as f:
    f.write(content)
