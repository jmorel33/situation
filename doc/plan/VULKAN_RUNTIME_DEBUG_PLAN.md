# Vulkan Runtime Debug Plan

**Date**: 2026-05-05  
**Status**: PHASE 3 COMPLETE — Init fixed, frame loop verified  
**Priority**: HIGH — Vulkan was operational before the codebase split, must be restored  
**Context**: The Vulkan renderer passes code audit (all 4 phases), compiles clean, and now initializes fully at runtime. The cycling clear color diagnostic works. Next: verify quad drawing.

---

## Known State

From the runtime log:
1. ✅ GLFW window created
2. ✅ Vulkan instance created
3. ✅ Physical device selected (NVIDIA GeForce GTX 1070, score 1032)
4. ✅ Logical device + queues created
5. ✅ Swapchain created (2 frames in flight)
6. ✅ Render pass created
7. ✅ Descriptor layouts created (compute terminal layout with 4 sets)
8. ✅ Quad shaders compiled (GLSL → SPIR-V via shaderc)
9. ✅ Font atlas texture created (slot 0, usage_flags=0xb)
10. ❌ **CRASH** — somewhere after font texture, before `SituationInit` returns
11. VMA reports 1 leaked allocation (the font atlas image, 65536 bytes)

## Root Cause Hypothesis

The crash occurs between step 9 and the return from `_SituationInitVulkan`. The remaining init steps after font texture are:
- `_SituationVulkanInitInternalRenderers()` (text pipeline, VD compositing pipeline)
- Staging buffer allocation
- Render thread startup (if threading enabled)
- Graveyard initialization

Since the codebase was split into multiple impl headers, a likely cause is a missing initialization step or a struct field that's no longer zeroed properly.

---

## Debug Process

### Phase 1: Locate the Crash

Add `fprintf(stderr, "VK_INIT_STEP: <name>\n"); fflush(stderr);` breadcrumbs at each step inside `_SituationInitVulkan` after the font init:

- [ ] Before `_SituationVulkanInitInternalRenderers()`
- [ ] After `_SituationVulkanInitInternalRenderers()` (or inside it, at each sub-step)
- [ ] Before staging buffer init
- [ ] After staging buffer init
- [ ] Before graveyard init
- [ ] After graveyard init
- [ ] Before render thread startup
- [ ] After render thread startup
- [ ] Before return from `_SituationInitVulkan`

Rebuild with `build_examples.bat vulkan diagnostic_render_vk`, run, capture log. The last breadcrumb printed identifies the crash location.

### Phase 2: Fix the Crash

**ROOT CAUSE FOUND AND FIXED (2026-05-06):**

In `SituationCreateTextureEx`, the "Resource Manager Hook" section at the end of the function checked `strcmp(sit_gs.last_error_msg, "No error")` to detect OpenGL errors. This check was NOT guarded by `#if defined(SITUATION_USE_OPENGL)`, so on the Vulkan path — where `last_error_msg` is never set to "No error" — it always triggered the failure branch, returning `SITUATION_ERROR_TEXTURE_UPLOAD_FAILED` even though the texture was successfully created.

**Fix:** Wrapped the entire error-check-and-cleanup block in `#if defined(SITUATION_USE_OPENGL)` since it's only relevant for deferred GL error detection.

- [x] Identify what's NULL/uninitialized at that point
- [x] Trace back to where it should have been set
- [x] Apply fix
- [x] Rebuild + verify init completes (log shows "Vulkan init OK")

### Phase 3: Verify Frame Loop

- [x] Confirm cycling clear color works (same as OpenGL diagnostic)
- [x] Confirm window stays open and responds to input
- [ ] Confirm clean shutdown (no VMA leaks)

### Phase 4: Verify Quad Draw

- [ ] Add `SituationCmdDrawQuad` to the diagnostic
- [ ] Confirm quad is visible
- [ ] If not visible, debug (same process as OpenGL — likely push constants or pipeline bind issue)

### Phase 5: Performance Test

- [ ] Port `quad_storm.c` to Vulkan
- [ ] Compare FPS with OpenGL at same quad count
- [ ] Vulkan should be significantly faster (command buffers, no driver overhead per draw)

---

## Build Command

Always use the standardized script:
```
build_examples.bat vulkan diagnostic_render_vk
```

Run and capture:
```
build\examples\diagnostic_render_vk.exe > vk_log.txt 2>&1
type vk_log.txt
```

---

## Historical Context

The Vulkan renderer was operational before the codebase was split into multiple `situation_impl_*.h` files. The split may have:
- Broken include order dependencies
- Left struct fields uninitialized that were previously zero-initialized as part of a larger allocation
- Moved init code that depended on ordering guarantees

The fix should be mechanical — find what's missing and restore it.

---

**Author**: Kiro  
**Estimated Effort**: 1-2 sessions  
**Next Action**: Phase 1 — add breadcrumbs, locate crash
