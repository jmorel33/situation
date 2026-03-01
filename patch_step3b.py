import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# I inserted into the wrong vkDeviceWaitIdle(sit_render.vk.device); let me fix that
content = re.sub(r"\s*// Destroy Cached Render Passes upon swapchain recreation \(formats might change\).*?sit_render\.vk\.render_pass_cache_count = 0;\n", "", content, flags=re.DOTALL)
content = re.sub(r"\s*// Destroy Cached Render Passes\n\s*for \(uint32_t i = 0; i < sit_render\.vk\.render_pass_cache_count; \+\+i\) \{\n\s*if \(sit_render\.vk\.render_pass_cache\[i\]\.handle != VK_NULL_HANDLE\) \{\n\s*vkDestroyRenderPass\(sit_render\.vk\.device, sit_render\.vk\.render_pass_cache\[i\]\.handle, NULL\);\n\s*\}\n\s*\}\n\s*sit_render\.vk\.render_pass_cache_count = 0;\n\s*", "", content, flags=re.DOTALL)

# Re-apply correctly
def inject_at_string(s_to_find, to_inject):
    global content
    idx = content.find(s_to_find)
    if idx != -1:
        content = content[:idx] + to_inject + content[idx:]

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
# Make sure we insert in the context of _SituationCleanupVulkan
idx = content.find("static void _SituationCleanupVulkan(void) {")
if idx != -1:
    actual_idx = content.find(cleanup_target, idx)
    if actual_idx != -1:
        content = content[:actual_idx] + inject_cleanup + "\n    " + content[actual_idx:]

# 2. _SituationVulkanRecreateSwapchain
recreate_target = "if (sit_render.vk.render_pass != VK_NULL_HANDLE) {"
idx = content.find("static void _SituationVulkanRecreateSwapchain(void) {")
if idx != -1:
    actual_idx = content.find(recreate_target, idx)
    if actual_idx != -1:
        inject_recreate = inject_cleanup.replace("Destroy Cached Render Passes", "Destroy Cached Render Passes upon swapchain recreation (formats might change)")
        content = content[:actual_idx] + inject_recreate + "\n    " + content[actual_idx:]

with open("situation_impl.h", "w") as f:
    f.write(content)
