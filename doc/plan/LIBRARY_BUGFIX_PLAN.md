# Library Bugfix Plan (Exposed by Test Harness)

**Date**: 2026-05-07  
**Updated**: 2026-05-08 (session 4 — Vulkan backend first pass, v2.4.42)  
**Scope**: Fix library bugs that cause test failures  
**DLL version**: v2.4.42

---

## Current State

**OpenGL Graphics module: 81/81 ✅** — all tests pass individually.

**Vulkan Graphics module: ~55/78** — rendering works visually, remaining failures are pixel readback issues.

**Full sequential suite**: Hangs on second `SituationInit()` due to audio device exclusive mode blocking. GL state is properly cleaned up; the remaining issue is audio-only.

**Other modules**: All pass individually (filesystem, threading, core, window, input, timer, audio, misc).

---

## ✅ Bug 1: Audio Callback Race Condition [FIXED]

**Fix**: `atomic_bool audio_ready` guard in callback + set after init completes.  
**Files**: `sit/situation_impl_decl.h`, `sit/situation_impl_audio.h`, `sit/situation_impl_ctrl.h`

## ✅ Bug 2: Pixel Readback Returns Stale Data [FIXED]

**Fix**: Pre-swap screenshot capture — `SituationEndFrame` reads the back buffer into a CPU buffer before `glfwSwapBuffers`, and `SituationLoadImageFromScreen` uses that buffer. This eliminates DWM/driver-dependent behavior on Windows.  
**Files**: `sit/situation_impl_decl.h` (added `screenshot_buffer` fields), `sit/situation_impl_renderer.h` (pre-swap capture in EndFrame), `sit/situation_impl_image.h` (read from captured buffer)

## ✅ Bug 3: `SituationSaveFileText` Type Mismatch [FIXED]

**Fix**: Test bug — changed `SituationError err = SituationSaveFileText(...)` to `bool save_ok = ...`.  
**File**: `tests/harness/test_graphics.c`

## ✅ Bug 4: DrawMesh Counter Not Incremented [FIXED]

**Fix**: Added `sit_render.frame_draw_calls++` and triangle count to `SituationCmdDrawMesh`.  
**File**: `sit/situation_impl_renderer.h`

## ✅ Bug 5: VD Composite GL_INVALID_ENUM [FIXED]

**Fix**: `_SitGLBackupState()` now queries actual GL blend state via `glGetIntegerv` when shadow state is `GL_NONE` (invalid). Previously `_SitGLRestoreState()` passed 0 to `glBlendFuncSeparate`.  
**File**: `sit/situation_impl_renderer.h`

---

## ✅ Bug 7 — VD Composite Produces Black Output (12 tests) [FIXED]

**Root cause**: Three issues combined:

1. **Sampler uniform not bound**: The VD fragment shaders declare `uniform sampler2D` without `layout(binding=N)` for OpenGL, defaulting to texture unit 0. But the composite code binds textures to units 4 and 5 (`SIT_SAMPLER_BINDING_SOURCE_0/1`). Fixed by calling `glProgramUniform1i` after shader creation to set sampler uniforms to the correct texture units.

2. **VD quad geometry in wrong coordinate space**: The VD quad used NDC vertices [-1,+1] but the vertex shader applies an ortho projection (pixel coords). Changed to a unit quad [0,1] so that model matrix scale/translate maps correctly to pixel coordinates.

3. **SCALING_STRETCH used VD resolution instead of window size**: The model matrix for STRETCH mode scaled by `vd->resolution` (the internal texture size) instead of `target_width/target_height` (the window size). Fixed to scale by window dimensions.

**Files modified**:
- `sit/situation_impl_renderer.h` — sampler uniform setup after shader creation, unit quad vertices, STRETCH scale fix
- `sit/situation_impl_decl.h` — screenshot buffer fields
- `sit/situation_impl_image.h` — pre-swap buffer readback

---

## ✅ Bug 8 — Shader Uniform Data Flow (4 tests) [FIXED]

**Symptom**: Tests render with uniforms (float multiplier, vec4 color, mat4 transform, textured checkerboard) but pixel values don't match expectations. The readback returns non-black pixels but wrong values.

**Root cause**: `SituationSetShaderUniform` deferred uniform uploads to the soft command buffer via `SIT_OP_SET_UNIFORM`. But `SituationAcquireFrameCommandBuffer` resets the buffer (`packet_count=0`, `data_cursor=0`), so uniforms set before frame acquisition were silently lost.

**Fix**: Changed to immediate `glProgramUniform*` calls (DSA). These apply directly to the program object's state and persist until changed, regardless of frame lifecycle.

**File**: `sit/situation_impl_renderer.h` (SituationSetShaderUniform)

---

## ✅ Bug 9 — Texture Roundtrip Format (2 tests) [FIXED]

**Symptom**: `texture_cpu_gpu_cpu_roundtrip` and `texture_format_preservation` — pixel values don't survive upload→render→readback cycle.

**Root cause**: Two issues in `SIT_OP_DRAW_QUAD` execution:
1. `u_uv_rect` (location 5) was never uploaded to the quad shader — the shader computed `v_TexCoord = 0 + aPos * 0 = (0,0)`, always sampling the bottom-left corner
2. `u_use_texture` (location 6) was only set to 0 when no texture was bound, but never set to 1 when a texture WAS bound — the shader skipped texture sampling entirely

**Fix**: Added `glProgramUniform4fv` for UV rect (loc 5) and `glProgramUniform1i` for use_texture (loc 6) in the draw quad batch loop.

**File**: `sit/situation_impl_renderer.h` (SIT_OP_DRAW_QUAD case)

---

## ✅ Bug 10 — Compute State Leak (1 test, flaky) [FIXED]

**Symptom**: `compute_dispatch_write42` passes in isolation but fails when run after other graphics tests in the module.

**Root cause**: Three issues combined:
1. `SIT_OP_BIND_COMPUTE_PIPELINE` had no case in the execution switch — the compute program was never actually bound via `glUseProgram`, so dispatches used whatever program was previously active
2. `SituationCmdBindDescriptorSetDynamic` didn't pass `usage_flags` to the command packet — SSBOs were always bound as `GL_UNIFORM_BUFFER` instead of `GL_SHADER_STORAGE_BUFFER`
3. `SituationCmdBindComputeTexture` used wrong opcode (`SIT_OP_BIND_DESCRIPTOR_SET`) which tried to interpret the texture ID as a buffer handle — storage images were never bound

**Fix**: Added the missing `SIT_OP_BIND_COMPUTE_PIPELINE` case, passed `slot->usage_flags` in the packet, and changed `SituationCmdBindComputeTexture` to use `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING`.

**Files**: `sit/situation_impl_renderer.h`

---

## Remaining: Bug 6 — Full Suite Re-Init Crash [PARTIAL]

**Symptom**: Running all modules sequentially hangs during the second `SituationInit()` call.

**Root cause**: Two issues:
1. ✅ **GL state not zeroed** (FIXED): `_SituationCleanupOpenGL` deleted GL objects but didn't zero state fields. Guard checks like `if (ring_buffer_id != 0) return;` skipped re-creation. Ring buffer and MDI buffer were never cleaned up at all. Fixed by adding proper cleanup and `memset(&sit_render.gl, 0, sizeof(sit_render.gl))`.
2. ❌ **Audio device exclusive mode** (OPEN): `SituationSetAudioDevice(0, NULL)` in the second init blocks indefinitely. The DirectSound exclusive-mode device from the first session may not release cleanly, causing the second `ma_device_init` to block waiting for the device.

**Remaining fix**: Either skip audio device auto-start on re-init, or add a timeout to the audio device initialization, or switch to shared mode for test harness.

**Priority**: P3 — blocks full sequential suite but not individual module testing  
**Effort**: 1 hour (audio-specific investigation)

---

## Resolution Priority

| # | Bug | Tests | Effort | Status |
|---|-----|-------|--------|--------|
| 7 | VD composite (sampler + quad + readback) | 12 | — | ✅ FIXED |
| 8 | Uniform data flow | 4 | 1-2h | ✅ FIXED |
| 9 | Texture format (UV rect + use_texture) | 2 | 30 min | ✅ FIXED |
| 10 | Compute state leak (pipeline bind + SSBO flags) | 2 | 15 min | ✅ FIXED |
| 6 | Re-init crash | (suite) | 2-4h | PARTIAL |

**Graphics module: 81/81 ✅**  
**Full sequential suite**: Still hangs on re-init due to audio device exclusive mode blocking. GL state is now properly zeroed, but audio re-init in exclusive mode blocks indefinitely on some systems. Individual module testing works perfectly.

---

## Fixes Applied (Session 3)

### Bug 8 — Shader Uniform Data Flow [FIXED]
**Root cause**: `SituationSetShaderUniform` deferred uniform uploads to the soft command buffer, but `SituationAcquireFrameCommandBuffer` resets the buffer (packet_count=0, data_cursor=0). Uniforms set before frame acquisition were silently lost.

**Fix**: Changed to immediate `glProgramUniform*` calls (DSA — doesn't require program to be bound). Uniform values persist in the GL program object state until explicitly changed.

**File**: `sit/situation_impl_renderer.h` (SituationSetShaderUniform)

### Bug 9 — Texture Roundtrip / DrawTexture [FIXED]
**Root cause**: Two issues in `SIT_OP_DRAW_QUAD` execution:
1. `u_uv_rect` (location 5) was never uploaded — shader always sampled UV (0,0)
2. `u_use_texture` (location 6) was only set to 0 (disabled) when no texture bound, never set to 1 when a texture WAS bound

**Fix**: Added `glProgramUniform4fv` for UV rect and `glProgramUniform1i` for use_texture flag in the draw quad batch loop.

**File**: `sit/situation_impl_renderer.h` (SIT_OP_DRAW_QUAD case)

### Bug 10 — Compute Pipeline + Buffer Binding [FIXED]
**Root cause**: Three issues:
1. `SIT_OP_BIND_COMPUTE_PIPELINE` had no case in the execution switch — compute program was never bound
2. `SituationCmdBindDescriptorSetDynamic` didn't pass `usage_flags` to the command packet — SSBOs were always bound as UBOs
3. `SituationCmdBindComputeTexture` used wrong opcode (`SIT_OP_BIND_DESCRIPTOR_SET` instead of `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING`) — storage images weren't bound

**Fix**: Added the missing case, passed usage_flags in the packet, and fixed the opcode for compute texture binding.

**Files**: `sit/situation_impl_renderer.h` (execution switch, SituationCmdBindDescriptorSetDynamic, SituationCmdBindComputeTexture)

### Buffer Update Immediacy [FIXED]
**Root cause**: `SituationUpdateBuffer` deferred writes to the soft command buffer, but `SituationGetBufferData` reads immediately via `glGetNamedBufferSubData`. Tests that update+readback without a frame cycle saw stale data.

**Fix**: Changed to immediate `glNamedBufferSubData` (DSA). Works regardless of frame state.

**File**: `sit/situation_impl_renderer.h` (SituationUpdateBuffer)

### Bug 6 — Re-Init GL State [PARTIAL]
**Root cause (GL portion)**: `_SituationCleanupOpenGL` deleted GL objects but didn't zero state fields. On re-init, guard checks like `if (ring_buffer_id != 0) return;` skipped re-creation. Also, ring buffer and MDI buffer were never cleaned up at all.

**Fix**: Added ring buffer/MDI buffer cleanup and `memset(&sit_render.gl, 0, sizeof(sit_render.gl))` at end of cleanup.

**Remaining**: Audio device in exclusive mode blocks on second `SituationSetAudioDevice(0, NULL)` call. This is a miniaudio/driver-level issue separate from GL state.

**File**: `sit/situation_impl_renderer.h` (_SituationCleanupOpenGL)

---

**Author**: Kiro  
**Status**: All graphics tests pass (81/81). Full sequential suite blocked by audio re-init (not GL).

---

## Next Steps — Remaining Work

### Bug 6 Completion: Audio Re-Init Blocking

**Problem**: The second `SituationInit()` call hangs at `SituationSetAudioDevice(0, NULL)` because the DirectSound exclusive-mode device from the previous session doesn't release cleanly. `ma_device_init` blocks waiting for the device.

**Proposed fixes (pick one):**

1. **Skip auto-start on re-init** (simplest): Check if the audio context was previously initialized in the same process. If so, skip the automatic `SituationSetAudioDevice(0, NULL)` call in step 7 of `SituationInit`. Let the user call it manually if needed.
   - File: `sit/situation_impl_ctrl.h` (SituationInit, step 7)
   - Effort: 15 minutes

2. **Use shared mode for re-init**: If the library detects it's being re-initialized (e.g., a static flag), open the audio device in shared mode instead of exclusive mode. Shared mode doesn't block.
   - File: `sit/situation_impl_audio.h` (SituationSetAudioDevice)
   - Effort: 30 minutes

3. **Add timeout to audio device init**: Wrap the `ma_device_init` + `ma_device_start` in a thread with a timeout. If it doesn't complete within 2 seconds, skip audio and log a warning.
   - File: `sit/situation_impl_audio.h` (SituationSetAudioDevice)
   - Effort: 1 hour

4. **Full audio state reset**: `memset(&sit_audio, 0, sizeof(sit_audio))` at the start of `_SituationCleanupSubsystems` (after device uninit), ensuring all flags and pointers are clean for re-init.
   - File: `sit/situation_impl_ctrl.h` (_SituationCleanupSubsystems)
   - Effort: 30 minutes, but needs careful testing to avoid double-free

**Recommendation**: Option 1 is safest and matches real-world usage (games don't re-init mid-process). Option 4 is the most thorough if full re-init support is desired.

### Additional Findings (Not Bugs — Design Notes)

These were discovered during investigation and are worth noting for future work:

1. **Deferred vs Immediate API pattern**: Several APIs (`SituationSetShaderUniform`, `SituationUpdateBuffer`) were deferred to the command buffer but their readback counterparts were immediate. This mismatch is now fixed (both immediate), but if the render thread architecture is ever re-enabled, these need to be revisited with proper synchronization.

2. **`SIT_OP_SET_UNIFORM` is now dead code**: The command buffer replay case for `SIT_OP_SET_UNIFORM` still exists but is never triggered (uniforms are now set immediately). It can be removed in a future cleanup pass, or kept as a fallback if deferred uniforms are ever needed for Vulkan push constants.

3. **`SIT_OP_UPDATE_BUFFER` is now dead code**: Same situation — the OpenGL path of `SituationUpdateBuffer` no longer pushes this opcode. The execution case can be removed or kept for potential future use.

4. **Compute pipeline state tracking**: The new `SIT_OP_BIND_COMPUTE_PIPELINE` case sets `sit_render.gl.current_program_id` which is shared with graphics pipelines. If a compute dispatch is followed by a graphics draw without an explicit `SIT_OP_BIND_PIPELINE`, the graphics draw will use the compute program. This works today because tests always bind a graphics pipeline before drawing, but it's fragile. Consider adding a separate `current_compute_program_id` field.

5. **Quad shader `u_Texture` sampler binding**: The quad fragment shader declares `uniform sampler2D u_Texture` without an explicit `layout(binding=N)`. It defaults to texture unit 0, which is where `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING` binds textures (via `glBindTextureUnit(idx, ...)` with idx=0). This works but is implicit — if the virtual bindless system ever changes the binding point, the quad shader will break silently.

---

## Vulkan Backend — Test Results (First Run, v2.4.41)

**Date**: 2026-05-08  
**Result**: ~43/81 passing (estimated — test crashed before completion)  
**Verdict**: Pre-existing issues, NOT regressions from OpenGL fixes  

All OpenGL fixes are inside `#if defined(SITUATION_USE_OPENGL)` blocks. The Vulkan backend was never previously tested with the harness (TEST_HARNESS_PLAN.md only checked "verify clean build" for Vulkan). These are newly-exposed pre-existing bugs.

### ✅ Vulkan Bug V1 — Shader Compilation Check (was ~20 tests) [FIXED v2.4.42]

**Root cause**: `SituationLoadShaderFromMemory` checked `blob.internal_result != shaderc_compilation_status_success`. Since `internal_result` is a pointer (`shaderc_compilation_result_t = struct*`) and `shaderc_compilation_status_success` is 0, this compared a non-NULL pointer to 0 — always true on success. The function returned -748 without ever compiling the fragment shader.

**Fix**: Changed to `if (!blob.data)` which correctly detects compilation failure (the function returns zeroed blob on error). Same fix applied to compute pipeline creation paths.

**Files**: `sit/situation_impl_renderer.h` (SituationLoadShaderFromMemory, SituationCreateComputePipelineFromMemory)

---

### ✅ Vulkan Bug V2 — Buffer Update Not Visible to Readback (3 tests) [FIXED v2.4.42]

**Root cause**: `SituationUpdateBuffer` Vulkan fallback (when `vmaMapMemory` fails for GPU-only memory) used the current frame's command buffer which may not be in recording state outside a frame.

**Fix**: Changed fallback to use `_SituationVulkanBeginSingleTimeCommands` + `vkCmdUpdateBuffer` + `_SituationVulkanEndSingleTimeCommands`. Also added staging buffer path for updates >64KB. Additionally changed SSBO allocation to `VMA_MEMORY_USAGE_CPU_TO_GPU` so `vmaMapMemory` succeeds directly.

**Files**: `sit/situation_impl_renderer.h` (SituationUpdateBuffer, _SituationVulkanCreateAndUploadBuffer)

---

### Vulkan Bug V3 — DrawQuad / DrawTexture Not Rendering (2 tests) [OPEN — readback issue]

**Symptom**: `draw_quad_red` and `draw_textured_checkerboard` fail — rendering IS visually correct (confirmed) but pixel readback returns black.

**Root cause**: Same as V6 (screenshot readback). The internal quad renderer works correctly on Vulkan. The test failure is purely a readback issue.

**Affected tests**: draw_quad_red, draw_textured_checkerboard

**Blocked by**: V6 (screenshot readback fix)

---

### ✅ Vulkan Bug V4 — VD Composite Crash (SIGSEGV) [FIXED v2.4.42]

**Root cause**: Three issues combined:
1. **VD depth image mipLevels**: `_SituationVulkanCreateImage` call passed `depth_format` (VkFormat enum ~124) as the `mipLevels` parameter instead of `1`. Created an image with 124+ mip levels.
2. **Nested render pass**: `SituationRenderVirtualDisplays` starts its own render pass, but callers already had a render pass active — illegal in Vulkan, caused SIGSEGV.
3. **VD render target render pass mismatch**: `SituationCmdBeginRenderPass` targeting a VD used `_SituationVulkanGetOrCreateRenderPass` which creates a new render pass incompatible with the VD's framebuffer.

**Fix**: 
1. Fixed mipLevels to `1` in VD depth image creation.
2. Added `vkCmdEndRenderPass` at start of composite + restart after composite completes.
3. Changed to use `vd->vk.render_pass` directly for VD render targets.

**Files**: `sit/situation_impl_vd.h`, `sit/situation_impl_renderer.h`

---

### Vulkan Bug V5 — Draw Metrics Overlay (1 test) [OPEN — likely readback issue]

**Symptom**: `draw_metrics_overlay` — no overlay pixels detected.

**Root cause**: Likely the same readback issue as V6. The text renderer may be working but the screenshot returns black. Needs verification after V6 is fixed.

**Effort**: Verify after V6 fix  
**Priority**: P4

---

### Vulkan Bug V6 — Screenshot Readback Returns Black [NEW — blocks ~20 tests]

**Symptom**: All rendering tests that verify pixel values fail. The rendering is visually correct (confirmed by observation — window shows correct colors) but `SituationLoadImageFromScreen` returns all-black data.

**Root cause**: After `SituationEndFrame` presents the swapchain image, the image is owned by the presentation engine. Even with `vkQueueWaitIdle` and `TRANSFER_SRC_BIT` on the swapchain, the readback from the presented image returns zeros on some drivers/configurations.

**Fix needed**: Pre-present screenshot capture (same approach as OpenGL Bug 2 fix). In `SituationEndFrame`, before `vkQueuePresentKHR`, copy the swapchain image content to a persistent staging buffer. `SituationLoadImageFromScreen` reads from that buffer instead of the swapchain image directly.

**Files to modify**: `sit/situation_impl_renderer.h` (SituationEndFrame Vulkan path), `sit/situation_impl_image.h` (SituationLoadImageFromScreen Vulkan path), `sit/situation_impl_decl.h` (add screenshot buffer fields to Vulkan state)

**Affected tests**: draw_pipeline_basic (passes visually), draw_indexed_quad, draw_mesh_triangle, draw_quad_red, draw_textured_checkerboard, draw_metrics_overlay, all VD composite pixel verification tests (~20 total)

**Effort**: 1-2 hours  
**Priority**: P1 (blocks the most tests)

---

### Vulkan Summary Table (Updated v2.4.42)

| Bug | Category | Tests Affected | Priority | Status |
|-----|----------|---------------|----------|--------|
| V1 | Shader compilation check | ~20 | — | ✅ FIXED |
| V2 | Buffer update out-of-frame | 3 | — | ✅ FIXED |
| V3 | Quad/Texture rendering | 2 | — | Blocked by V6 |
| V4 | VD composite crash | 3 | — | ✅ FIXED |
| V5 | Text/overlay rendering | 1 | P4 | Blocked by V6 |
| V6 | Screenshot readback | ~20 | P1 | OPEN |

**Total Vulkan failures**: ~23 (of 78, down from ~29/81)  
**Passing**: ~55 (up from ~43)  
**No crashes** — all SIGSEGV issues resolved.

**Next session**: Fix V6 (pre-present capture) → should flip ~20 tests to passing → target ~75/78.
