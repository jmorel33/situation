import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# I messed up inserting inside _SituationCleanupVulkan/RecreateSwapchain. Let's clean up those rogue inserts first.
content = re.sub(r"// Destroy Cached Render Passes upon swapchain recreation.*?\n\s*sit_render\.vk\.render_pass_cache_count = 0;\n", "", content, flags=re.DOTALL)
content = re.sub(r"// Destroy Cached Render Passes\n\s*for \(uint32_t i = 0; i < sit_render\.vk\.render_pass_cache_count; \+\+i\) \{\n\s*if \(sit_render\.vk\.render_pass_cache\[i\]\.handle != VK_NULL_HANDLE\) \{\n\s*vkDestroyRenderPass\(sit_render\.vk\.device, sit_render\.vk\.render_pass_cache\[i\]\.handle, NULL\);\n\s*\}\n\s*\}\n\s*sit_render\.vk\.render_pass_cache_count = 0;\n\s*", "", content, flags=re.DOTALL)


# Proper insertion in _SituationCleanupVulkan
cleanup_vulkan_pos = content.find("if (sit_render.vk.render_pass != VK_NULL_HANDLE) {")
if cleanup_vulkan_pos != -1:
    inject_cleanup = """
    // Destroy Cached Render Passes
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;

"""
    content = content[:cleanup_vulkan_pos] + inject_cleanup + content[cleanup_vulkan_pos:]

# Proper insertion in _SituationVulkanRecreateSwapchain
recreate_swapchain_pos = content.find("vkDeviceWaitIdle(sit_render.vk.device);", content.find("_SituationVulkanRecreateSwapchain(void)"))
if recreate_swapchain_pos != -1:
    inject_recreate = """
    // Destroy Cached Render Passes upon swapchain recreation (formats might change)
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;
"""
    eol = content.find("\n", recreate_swapchain_pos)
    content = content[:eol+1] + inject_recreate + content[eol+1:]


with open("situation_impl.h", "w") as f:
    f.write(content)
