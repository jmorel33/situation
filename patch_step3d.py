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

# Insert into _SituationVulkanCleanupSwapchain since that's what both _SituationCleanupVulkan and _SituationVulkanRecreateSwapchain call!
cleanup_target = "vkDestroyImageView(sit_render.vk.device, sit_render.vk.depth_image_view, NULL);"
idx = content.find("static void _SituationVulkanCleanupSwapchain(void) {")
if idx != -1:
    actual_idx = content.find(cleanup_target, idx)
    if actual_idx != -1:
        content = content[:actual_idx] + inject_cleanup + "\n    " + content[actual_idx:]

with open("situation_impl.h", "w") as f:
    f.write(content)
