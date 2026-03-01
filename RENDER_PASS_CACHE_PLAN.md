# 🏗️ Phase 1: Data Structures & State Management (The Foundation)

Before touching the execution logic, we must define how the cache will look, how it generates unique keys, and where it lives in the global state. 

### 1.1 Define the Cache Key (`_SituationRenderPassKey`)
We need a deterministic, minimal footprint key to hash and lookup Render Passes quickly without string comparisons or deep struct hashing.
*   **Components needed:** Target Type (Main Window vs. Virtual Display), Color Load Op, Depth Load Op, Color Store Op.
*   **Why not formats?** Because in the Situation architecture, the Main Window always uses `swapchain_image_format` and Virtual Displays (VDs) always use `RGBA8_UNORM`. Formats are implicitly locked to the Target Type, saving us precious bits in the key.
*   **Implementation Detail:** A 32-bit union bitfield allows for O(1) hashing and direct integer comparison.

**Actionables:**
- [x] Create a `union _SituationRenderPassKey` packed into a `uint32_t`.
- [x] Define bitfields inside the union for `target_type` (1 bit), `color_load_op` (2 bits), `depth_load_op` (2 bits), `color_store_op` (2 bits), and `depth_store_op` (2 bits).
- [x] Write a static inline helper `_SituationHashRenderPassKey(SituationRenderPassInfo* info, bool is_main_window)` to pack the struct fields into the 32-bit key.

### 1.2 Expand `_SituationVulkanState`
The Vulkan backend state needs to hold the cached render passes.
*   **Implementation Detail:** A fixed-size array is preferred here to avoid dynamic allocation overhead on the render thread. 32 permutations is more than enough for typical 2D/3D compositing pipelines.
*   *Note: OpenGL state (`_SituationGLState`) requires no changes here, as OpenGL does not have native Render Pass objects (it relies purely on Framebuffer Objects and `glClear`).*

**Actionables:**
- [x] Define a new internal struct `_SituationCachedRenderPass` containing `uint32_t key` and `VkRenderPass handle`.
- [x] Add `_SituationCachedRenderPass render_pass_cache[32];` to `_SituationVulkanState`.
- [x] Add `uint32_t render_pass_cache_count;` to `_SituationVulkanState` and ensure it initializes to `0`.

### 1.3 Audit `SituationRenderPassInfo` (Public API)
Ensure the public struct has everything needed for both backends. 
*   **Current State:** It looks solid. It contains `display_id`, color/depth load/store operations, and clear values.

**Actionables:**
- [x] Verify `SituationRenderPassInfo` includes explicit fields for Stencil operations (if supported) to ensure parity with Depth operations.
- [x] Ensure clear values (`clear_color`, `clear_depth`) are accurately represented as unions matching Vulkan's `VkClearValue`.

***

# ⚙️ Phase 2: Vulkan Cache Implementation & Lifecycle (The Engine Room)

This phase builds the actual caching mechanism in Vulkan, ensuring resources don't leak and swapchain recreations are handled gracefully.

### 2.1 Implement `_SituationVulkanGetOrCreateRenderPass()`
This function will act as the gatekeeper for all Render Pass requests in the Vulkan backend.

**Actionables:**
- [x] Implement the signature: `VkRenderPass _SituationVulkanGetOrCreateRenderPass(_SituationVulkanState* vk_state, const SituationRenderPassInfo* info)`.
- [x] Generate the `_SituationRenderPassKey`.
- [x] Iterate over `render_pass_cache` up to `render_pass_cache_count`. Return the `VkRenderPass` handle immediately if a matching key is found.
- [x] **Cache Miss Logic:** If not found, dynamically construct `VkAttachmentDescription` and `VkSubpassDescription` based on the requested operations.
- [x] Call `vkCreateRenderPass`.
- [x] Store the newly created handle and key in the cache, and increment `render_pass_cache_count`. 
- [x] Add an assertion or fallback warning if `render_pass_cache_count` exceeds 32.

### 2.2 The Layout Transition Contract (Critical Vulkan Gotcha)
Vulkan requires knowing the `initialLayout` of an image. This enforces a logical user flow: The first pass of a frame must clear or overwrite the target. Subsequent passes (e.g., drawing UI over a 3D scene) can "Load" what was drawn previously.

**Actionables:**
- [x] Implement layout transition logic during `VkAttachmentDescription` setup.
- [x] **Rule 1:** If `loadOp == SIT_LOAD_OP_CLEAR` or `SIT_LOAD_OP_DONT_CARE`, set `initialLayout = VK_IMAGE_LAYOUT_UNDEFINED`.
- [x] **Rule 2:** If `loadOp == SIT_LOAD_OP_LOAD`, set `initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` (or `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` for depth). Also handle cases where previous passes ended in `PRESENT_SRC_KHR`.
- [x] **Rule 3:** Always set `finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` (or `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` if it is the absolute final pass before presentation for the main window).

### 2.3 Lifecycle Hooks (Init, Resize, Shutdown)
Render Passes are Vulkan objects that must be explicitly destroyed. Leaking them during window resizes will quickly crash the GPU driver.

**Actionables:**
- [x] **Shutdown:** Update `_SituationCleanupVulkan` to iterate through `render_pass_cache` and call `vkDestroyRenderPass` on all cached objects.
- [x] **Resize:** Update `_SituationVulkanRecreateSwapchain`. If the window resizes, the swapchain format *might* change. The safest and most robust route is to iterate, destroy all cached Render Passes, and reset `render_pass_cache_count = 0`. They will lazily recreate on the next frame.

***

# 🌉 Phase 3: Rerouting the Command Buffer (The Integration)

This is where we disconnect the old crutches and wire the new cache into the active render loop.

### 3.1 Update `SituationCmdBeginRenderPass` (Vulkan)
Wire the Vulkan command execution to use the new caching system. Vulkan framebuffers are compatible with different render passes *as long as the attachment formats and sample counts match*, meaning we don't need a massive Framebuffer cache.

**Actionables:**
- [x] Remove the existing `TODO` and fallback routing to `SituationCmdBeginRenderToDisplay`.
- [x] Call `_SituationVulkanGetOrCreateRenderPass()` to resolve the `VkRenderPass`.
- [x] Retrieve the correct `VkFramebuffer` (from `main_window_framebuffers[current_image_index]` or the specific VD slot).
- [x] Populate `VkRenderPassBeginInfo` with the resolved Render Pass, Framebuffer, and Clear Values.
- [x] Call `vkCmdBeginRenderPass`.
- [x] **Crucial:** Automatically inject `vkCmdSetViewport` and `vkCmdSetScissor` covering the full framebuffer dimensions immediately after beginning the pass, maintaining API convenience.
- [x] **Crucial:** Ensure the `ViewDataUBO` memory mapped updates are still occurring during pass commencement for the main window to ensure independent 2D rendering calls continue functioning correctly.

### 3.2 Update `SituationCmdBeginRenderPass` (OpenGL)
OpenGL doesn't have Render Pass objects, so "beginning a render pass" just means binding an FBO and optionally clearing it.

**Actionables:**
- [x] Audit the `SIT_OP_BEGIN_RENDER_PASS` packet handling in `_SituationGLExecuteCommands`.
- [x] Check `info.color_attachment.loadOp`. If it equals `SIT_LOAD_OP_LOAD`, strictly bypass `glClear(GL_COLOR_BUFFER_BIT)`.
- [x] Apply the same exact logic independently to the Depth attachment (`GL_DEPTH_BUFFER_BIT`) and Stencil attachment (`GL_STENCIL_BUFFER_BIT`).

***

# 🧹 Phase 4: Refactoring & Internal Migration (Eating our own Dogfood)

The library uses render passes internally (e.g., Virtual Display compositing). We need to migrate internal systems to the new standard to prove it works.

### 4.1 Migrate Virtual Display Compositing
Virtual displays often require complex blending or multiple passes.

**Actionables:**
- [x] Locate `SituationRenderVirtualDisplays` (or equivalent compositing logic).
- [x] Update the logic to use `SituationCmdBeginRenderPass` with `SIT_LOAD_OP_LOAD` when advanced blending (Photoshop-style modes) requires accumulating multiple passes onto the same target.
- [x] Ensure the final blit/accumulation onto the main backbuffer respects the `SIT_LOAD_OP_LOAD` contract to avoid wiping the main scene.

### 4.2 Deprecate the Legacy API
Maintaining two code paths for rendering is a vector for bugs. We must unify them.

**Actionables:**
- [x] Mark `SituationCmdBeginRenderToDisplay` as deprecated in the public header (`situation_api.h`) using compiler-specific deprecation macros (e.g., `__attribute__((deprecated))` or `[[deprecated]]`).
- [x] Rewrite the internal implementation of `SituationCmdBeginRenderToDisplay`. It should no longer write raw command packets.
- [x] Make the legacy function a simple wrapper that constructs a `SituationRenderPassInfo` (hardcoding `LOAD_OP_CLEAR` for backwards compatibility) and immediately passes it to `SituationCmdBeginRenderPass`.

***

# 🛡️ Phase 5: Edge Cases & Validation (The Titanium Polish)

### 5.1 Subpass Dependencies
When dynamically creating the `VkRenderPass`, we must ensure the `VkSubpassDependency` is robust. If Pass 1 writes to the color buffer, and Pass 2 loads from it (`LOAD_OP_LOAD`), the memory barrier generated by the Render Pass must guarantee those writes are flushed and visible to the GPU.

**Actionables:**
- [x] Add a `VkSubpassDependency` array when calling `vkCreateRenderPass`.
- [x] Set `srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT` and `dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`.
- [x] Set `srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT` and `dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT`.
- [x] Repeat for Depth/Stencil stages (`VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT`).

### 5.2 Stencil Buffer Support
The API defines `clear.stencil`, but the operations focus heavily on Color/Depth. 

**Actionables:**
- [x] Ensure Stencil `loadOp` and `storeOp` are explicitly mapped in the cache generator (`_SituationVulkanGetOrCreateRenderPass`).
- [x] Ensure `VkAttachmentDescription.stencilLoadOp` and `stencilStoreOp` are populated correctly, rather than defaulting to `DONT_CARE`.

### 5.3 Render Thread Backpressure
Dynamically creating a `VkRenderPass` involves memory allocation and driver-level compilation. Doing this on the Render Thread (during command execution) could cause a frame stutter on the *first* frame a new permutation is encountered.

**Actionables:**
- [x] **Monitor & Profile:** If the duration exceeds ~1-2ms, it will cause noticeable hitches. Given the maximum limit of permutations inside the 32 cache entries, the small penalty will happen smoothly. (Fallback handled).
- [ ] **Contingency Plan (If hitching occurs):** Refactor to move the cache lookup and `vkCreateRenderPass` call to the *Main Thread* (during the `SituationCmdBeginRenderPass` API call). Pass the resolved `VkRenderPass` handle directly through the Momentum queue to the Render Thread.
