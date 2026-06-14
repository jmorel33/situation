# Test Harness — Graphics Upgrade Plan (Phases 23–29)

**Date**: 2026-05-18  
**Parent plan**: [`TEST_HARNESS_PLAN.md`](TEST_HARNESS_PLAN.md) (Phases 1–22 complete)  
**Target**: `tests/harness/test_graphics.c` (+ optional `tests/harness/sit_graphics_test_helpers.h`)  
**Trigger**: Demon Hunt v2.4.81–82 — **315/315 OpenGL harness tests passed** while fragment multi-SSBO + SPIR-V link failed in production.

---

## Summary

The existing graphics harness proves compute SSBO roundtrips and single-set descriptor binding. It does **not** prove that a **fragment** shader loaded via `SituationLoadShaderFromMemory` (SPIR-V on OpenGL) can read **two distinct SSBOs** at `layout(binding = 0)` and `layout(binding = 1)` after `SituationCmdBindDescriptorSet(cmd, 0/1, …)`.

This plan adds **7 small phases** (23–29), one mergeable chunk at a time. Each phase ends with a green `sit_test --module graphics` (or `--filter <name>`) on the reference OpenGL build.

**Non-goals**: Re-implement Demon Hunt raycasting, full maze logic, or 50 duplicate API smoke tests.

---

## Gap analysis (what Phases 1–22 miss)

| Area | Covered today | Missing |
|------|----------------|---------|
| Compute SSBO | `compute_dispatch_*`, `compute_chained_dispatches` (TWO_SSBOS: `set=0` + `set=1`, each `binding=0`) | Fragment stage, **two bindings in one stage** (0 and 1) |
| Shader load path | `load_shader_from_memory`, minimal GLSL | SPIR-V graphics program + multi-SSBO fragment |
| Uniform arrays | float / vec4 / mat4 | `SituationSetShaderUniform1iv` (`int` arrays, e.g. `uRows[32]`) |
| Readback contract | Screen pixels, buffer readback | SSBO tags encoded in `FragColor` (R/G channels) |
| Library link | Implicit via compute TWO_SSBOS name fix | Explicit regression for duplicate `GL_BUFFER_BINDING` on SPIR-V |
| Combined layout | — | Header ints + `vec4[]` tail (Demon Hunt `ShaderScenePack`) |

---

## Prerequisites (all phases)

- Rebuild **`situation_opengl.dll`** when library SSBO binding code changes (`sit/situation_impl_renderer.h`).
- Rebuild harness: `build_tests.bat` (or project equivalent) → `build\sit_test.exe`.
- GPU + visible or offscreen GL context (same as current graphics module).
- `SITUATION_ENABLE_SHADER_COMPILER` enabled (SPIR-V path for `SituationLoadShaderFromMemory` on OpenGL).

**Run commands**

```bat
build\sit_test.exe --module graphics
build\sit_test.exe --filter fragment_dual_ssbo
```

**Reference baseline** (update when phases land): OpenGL full harness **315/315** pass; graphics module ~81 tests in `test_graphics.c`.

---

## Shared implementation (Phase 23 only)

Add reusable pieces once; later phases must not copy-paste fullscreen draw boilerplate.

### 23A — Helpers (`tests/harness/sit_graphics_test_helpers.h` or static section in `test_graphics.c`)

- [x] `graphics_test_fullscreen_mesh()` — cached triangle mesh (same pattern as `test_uniform_float_multiplier`).
- [x] `graphics_test_clear_and_draw(shader, mesh, cmd)` — begin pass, clear black, bind pipeline, draw mesh, end pass, `SituationEndFrame`.
- [x] `graphics_test_read_center_pixel_rgba(SituationImage* out, uint8_t r[4])` — `SituationLoadImageFromScreen`, sample center with tolerance helper (reuse `pixel_approx_eq` if present).
- [x] `graphics_test_skip_if_no_spirv()` — skip (pass) when `!GLAD_GL_ARB_gl_spirv` or `SituationLoadShaderFromMemory` fails for trivial shader (document in test output via `SIT_ASSERT(true)` + comment, matching existing compute skip style).
- [x] `graphics_test_create_ssbo(size, initial, out_buffer)` — thin wrapper over `SituationCreateBuffer` + `STORAGE_BUFFER`.

### 23B — Registry / docs

- [x] Register helper header from `test_graphics.c` if split out.
- [x] Add cross-link in [`TEST_HARNESS_PLAN.md`](TEST_HARNESS_PLAN.md) § “Phase 23+” pointing here.
- [x] Note new tests in harness plan snapshot table when Phase 24 lands.

**Acceptance**: Build clean; no new tests yet (or one noop `test_graphics_helpers_smoke` that draws red tri).  
**Effort**: ~1–2 h

---

## Phase 24 — Fragment dual SSBO readback (critical)

**Purpose**: Catch SPIR-V programs where two SSBO blocks collapse to the same binding — the Demon Hunt failure mode.

### Shader sources (embedded in `test_graphics.c`)

```glsl
// VS: passthrough fullscreen tri (existing g_vs_passthrough)
// FS:
layout(std430, binding = 0) readonly buffer BlockA { uint tagA; };
layout(std430, binding = 1) readonly buffer BlockB { uint tagB; };
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(float(tagA) / 255.0, float(tagB) / 255.0, 0.0, 1.0);
}
```

### Test: `test_fragment_dual_ssbo_readback`

- [x] Create `SituationBuffer` A (4 bytes, `uint32_t tagA = 77`) and B (`tagB = 203`).
- [x] `SituationLoadShaderFromMemory` VS + FS.
- [x] Frame: `SituationCmdBindPipeline` → `SituationCmdBindDescriptorSet(cmd, 0, A)` → `SituationCmdBindDescriptorSet(cmd, 1, B)` → `SituationCmdDrawMesh` (fullscreen tri).
- [x] Readback center pixel: **R ≈ 77**, **G ≈ 203**, B ≈ 0 (tolerance ±10 per channel, match existing tests).
- [x] **Negative check**: bind B to slot 0 only (omit A or bind zeros) → R must **not** be 77 (proves B data is not mistaken for A).
- [x] Register `{"fragment_dual_ssbo_readback", test_fragment_dual_ssbo_readback, true}`.

### Backend matrix

| Backend | Expected |
|---------|----------|
| OpenGL + SPIR-V | **Must pass** after Situation v2.4.82+ |
| OpenGL GLSL-only fallback | Skip if SPIR-V unavailable |
| Vulkan | Deferred — see **[`VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md)** (dated **2026-05-21**, v2.4.93+ track) |

**Acceptance**: Fails on pre-2.4.82 library with aliased bindings; passes after duplicate-binding fix.  
**Effort**: ~2–3 h  
**Depends on**: Phase 23

---

## Phase 25 — Combined header + sprite tail SSBO

**Purpose**: Guard Demon Hunt `ShaderScenePack` layout (map ints + aligned `vec4 spriteData[]`).

### Layout (must match `examples/demon_hunt_sky.fs` / `SkySceneSsboHeader`)

| Field | std430 offset |
|-------|----------------|
| `mapSize[2]` | 0 |
| `wallRows[32]` | 8 |
| `archNsRows[32]` | 136 |
| `archEwRows[32]` | 264 |
| `_alignPad[2]` | 392 |
| `spriteData[]` | **400** (vec4 index **25**) |

### Shader sketch

- `wallRows[0] = 0x0000000F` (bits 0–3 set) → fragment sets **B** channel from popcount or fixed encoding.
- `spriteData[25]` = vec4(0, 0, 1, 0) → **R** channel ≈ 255 (blue tag in vec4.z).

### Test: `test_fragment_combined_scene_ssbo`

- [x] Single SSBO, `SituationCmdBindDescriptorSet(cmd, 1, scene)` only (binding 1, like Demon Hunt).
- [x] Upload header + one vec4 at index 25; readback pixel asserts **both** header-derived and tail-derived channels.
- [x] Register in `graphics_tests[]`.

**Acceptance**: Misaligned header (drop `_alignPad`) causes detectable failure.  
**Effort**: ~2 h  
**Depends on**: Phase 23

---

## Phase 26 — `SituationSetShaderUniform1iv` (int arrays)

**Purpose**: Document SPIR-V behavior for maze-style `uniform int uWallRows[32]` (often stripped or location `-1`).

### Shader

```glsl
uniform int uTags[8];
// FS: fragColor = vec4(float(uTags[0])/255.0, ...);
```

### Test: `test_uniform_1iv_int_array`

- [x] `SituationSetShaderUniform1iv(shader, "uTags[0]", 8, values)` → if `SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND`, **skip** with logged reason (not fail).
- [x] If success: render → readback encodes `values[0]`, `values[1]` in R/G.
- [x] Alternate name: try `"uTags"` if `"uTags[0]"` fails (matches library fallback in `SituationSetShaderUniform1iv`).

**Acceptance**: Never fails CI solely because SPIR-V removed the uniform; passes when GLSL path or SPIR-V retains the array.  
**Effort**: ~1–2 h  
**Depends on**: Phase 23

---

## Phase 27 — SPIR-V SSBO reflection sanity (optional but cheap)

**Purpose**: Fail fast in harness when link leaves two blocks on the same binding **before** draw.

### Test: `test_spirv_ssbo_reflection_unique_bindings`

- [x] After `SituationLoadShaderFromMemory` for Phase 24 shaders, query OpenGL:
  - `glGetProgramInterfaceiv(..., GL_ACTIVE_RESOURCES, &count)`
  - For each `GL_SHADER_STORAGE_BLOCK`, read `GL_BUFFER_BINDING` and block name (`glGetProgramResourceName`).
- [x] Assert block names `BlockA` / `BlockB` (or indices 0/1) map to bindings **0** and **1** respectively.
- [x] Solved via new public `SituationQueryShaderStorageBlocks` API (`situation_api.h` + `situation_impl_renderer.h`); no raw program ID leak needed. OpenGL only; Vulkan returns `NOT_IMPLEMENTED`.

**Acceptance**: Documents expected reflection; catches driver regressions without framebuffer.  
**Effort**: ~1–3 h (depends on program handle access)  
**Depends on**: Phase 24  
**Status**: Optional — Phase 24 readback is the required gate.

---

## Phase 28 — Library regression guard (Situation + harness)

**Purpose**: Keep `_SituationBindGLProgramStorageBlocks()` duplicate-binding fix covered.

### 28A — Library (already in v2.4.82)

- [x] `_SituationBindGLProgramStorageBlocks` assigns unique bindings when SPIR-V reports duplicates (`sit/situation_impl_renderer.h`).

### 28B — Harness

- [x] Re-run Phase 24 test on clean DLL after any edit to `_SituationBindGLProgramStorageBlocks`.
- [x] Add comment in Phase 24 test: `// Regression for v2.4.82 — see doc/plan/TEST_HARNESS_GRAPHICS_UPGRADE.md`
- [x] Add one-line entry to [`doc/UPDATELOG.md`](../UPDATELOG.md) when Phase 24 lands: “harness: fragment_dual_ssbo_readback”.

**Acceptance**: Phase 24 is the permanent guard; no second duplicate test unless reflection phase added.  
**Effort**: ~30 min (mostly verification)  
**Depends on**: Phase 24

---

## Phase 29 — Integration, CI, and plan closure

- [x] Update [`TEST_HARNESS_PLAN.md`](TEST_HARNESS_PLAN.md):
  - Snapshot table: OpenGL test count after new tests.
  - New section “Phase 23–29 — Fragment SSBO / SPIR-V graphics” with link to this file.
  - Coverage gap row marked **addressed** for fragment dual SSBO.
- [x] Update `.kiro/specs/situation-test-harness/` if spec exists (mirror phase list).
- [x] Document in README or `build_tests.bat` comment: run `sit_test --filter fragment` after SSBO binding changes.
- [x] Full OpenGL harness green; record count in this file **Verified** table.
- [x] Vulkan: note “graphics 78/78 unchanged; fragment SSBO phases are OpenGL-SPIR-V only” unless a Vulkan equivalent is added later.
- [x] Vulkan SPIR-V user descriptors: **[`VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md)** — **complete v2.4.94**; **`sit_test_vulkan.exe --filter spirv`** **5/5**; full graphics **86/86**.

**Acceptance**: Single source of truth for graphics SSBO gaps; onboarding dev knows which test to run.  
**Effort**: ~1 h  
**Depends on**: Phases 23–28 (minimum 23 + 24 + 29)

---

## Implementation order

| Phase | Name | New tests (approx.) | Effort | Blocker |
|-------|------|---------------------|--------|---------|
| **23** | Shared helpers | 0–1 smoke | 1–2 h | — |
| **24** | Fragment dual SSBO readback | 1 | 2–3 h | **23** |
| **25** | Combined scene SSBO | 1 | 2 h | 23 |
| **26** | Uniform `1iv` int array | 1 | 1–2 h | 23 |
| **27** | SPIR-V reflection (optional) | 1 | 1–3 h | 24 |
| **28** | Library regression note | 0 | 0.5 h | 24 |
| **29** | Docs + full harness run | 0 | 1 h | 24 |

**Recommended merge order**: 23 → **24** (merge immediately; highest value) → 28 → 29 → 25 → 26 → 27.

**Total effort**: ~9–14 h focused work.

---

## Test registry checklist (append to `graphics_tests[]`)

When implementing, add in order:

```c
// Phase 23
{"graphics_helpers_smoke",            test_graphics_helpers_smoke,          true},  // optional

// Phase 24 — required
{"fragment_dual_ssbo_readback",       test_fragment_dual_ssbo_readback,   true},

// Phase 25
{"fragment_combined_scene_ssbo",      test_fragment_combined_scene_ssbo,  true},

// Phase 26
{"uniform_1iv_int_array",           test_uniform_1iv_int_array,         true},

// Phase 27 (optional)
{"spirv_ssbo_reflection_bindings",    test_spirv_ssbo_reflection_bindings, true},
```

---

## Verified snapshot (fill in when complete)

| Date | OpenGL full `sit_test` | Graphics module | Notes |
|------|------------------------|-----------------|-------|
| 2026-05-20 | **320 / 320** | **89** (+5) | GTX 1070 class GPU; Phases 23–29 + `demon_hunt_sky_shader_link` |
| 2026-05-21 | — | **96** GL / **86** VK | Phase 30 **`test_graphics_spirv.c`**; Vulkan user descriptors **v2.4.94** — [`VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md) |
| 2026-05-21 | — | **98** GL / **88** VK | **v2.4.95** — `screen_readback_corner_layout`, `cmd_draw_text_screen_layout` (GL+VK screen/text parity) |
| 2026-06-12 | — | **102** GL | **v2.4.258** — Phase 27 `spirv_ssbo_reflection_bindings` added; GTX 1070, OpenGL 4.6; `block[0] name='BlockA' binding=0, block[1] name='BlockB' binding=1` ✅ |

---

## Related library / game changes

| Change | Version | Harness phase |
|--------|---------|----------------|
| `glShaderStorageBlockBinding` after link | v2.4.81 | 24 (indirect) |
| Unique bindings when SPIR-V duplicates `GL_BUFFER_BINDING` | v2.4.82 | **24** |
| Demon Hunt `ShaderScenePack` single SSBO @ binding 1 | game | **25** |
| `SituationLoadShaderFromSpirv()` + build-time `.spv` for Demon Hunt (GL) / Vulkan `.spv` parity | v2.4.83–84 | **29** |
| Vulkan SPIR-V user descriptor profiles + harness pixel readback | v2.4.93–94 | **30** (`test_graphics_spirv.c`) |

---

## What this plan does NOT do

- Does not modify `situation_api.h` unless Phase 27 requires a debug program handle (avoid if possible).
- Does not replace compute TWO_SSBOS tests (keep them; different binding model).
- Does not add headless software GL requirement beyond current harness.
- Does not assert audio, window, or filesystem behavior.

---

**Author**: Cursor agent (recovery doc after IDE restart)  
**Status**: **Implemented** (Phases 23–27, 28–29; all phases complete)  
**Next action**: Run full OpenGL harness and fill **Verified** table.
