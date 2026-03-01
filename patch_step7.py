import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# 1. Fix _SituationVulkanCleanupSwapchain cache destruction location
# Find where it currently is:
wrong_cleanup = """    // Destroy Cached Render Passes
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;"""

# Remove it from wherever it is currently
content = content.replace(wrong_cleanup, "")
content = re.sub(r"\n\s*\n", "\n", content) # clean up empty lines

# Re-inject right after vkDeviceWaitIdle inside _SituationVulkanCleanupSwapchain
idx = content.find("static void _SituationVulkanCleanupSwapchain(void) {")
if idx != -1:
    wait_idle = content.find("vkDeviceWaitIdle(sit_render.vk.device);", idx)
    if wait_idle != -1:
        end_wait_idle = content.find(";", wait_idle) + 1
        content = content[:end_wait_idle] + "\n\n" + wrong_cleanup + "\n" + content[end_wait_idle:]


# 2. Restore UBO update logic inside SituationCmdBeginRenderPass (Vulkan branch)
# We need to insert it right before vkCmdBeginRenderPass
ubo_logic = """
    if (info->display_id == -1) {
        // CRITICAL: Update view/projection UBO for main window rendering
        // This is required for text rendering and any other 2D rendering to work
        ViewDataUBO ubo_data;
        glm_mat4_identity(ubo_data.view);
        glm_ortho(0.0f, (float)target_w, (float)target_h, 0.0f, -1.0f, 1.0f, ubo_data.projection);

        void* mapped_data;
        if (vmaMapMemory(sit_render.vk.vma_allocator, sit_render.vk.view_proj_ubo_memory[sit_render.vk.current_frame_index], &mapped_data) == VK_SUCCESS) {
            memcpy(mapped_data, &ubo_data, sizeof(ViewDataUBO));
            vmaUnmapMemory(sit_render.vk.vma_allocator, sit_render.vk.view_proj_ubo_memory[sit_render.vk.current_frame_index]);
        }
    }
"""

begin_pass_call = "vkCmdBeginRenderPass(vk_cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);"
idx_begin = content.find(begin_pass_call, content.find("SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info) {"))
if idx_begin != -1:
    content = content[:idx_begin] + ubo_logic + "\n    " + content[idx_begin:]


with open("situation_impl.h", "w") as f:
    f.write(content)
