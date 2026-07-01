# Shader Debug & SPIR-V Analysis Plan

**Status:** Tooling complete. Active gaps: Async hardening Phases B–E, Vulkan shader cache (not started), Demon Hunt Step A/D bisect pending.
**As of:** v2.4.278

---

## Master checklist

- [x] `scripts/spirv_shader_debug.py` — SPIR-V + GLSL offline analysis tool
- [x] `scripts/spirv_desc_spike.py` — Vulkan harness `(set, binding)` layout gate
- [x] `compile_demon_hunt_shaders.bat` — Demon Hunt GL + VK SPIR-V compile + embed regen
- [x] `compile_harness_shaders.bat` / `.ps1` — harness GL + VK SPIR-V compile + embed regen
- [x] `doc/TEST_SPIRV_SHADER_API.md` — async SPIR-V load API test checklist (both backends)
- [x] Async Phase A shipped (v2.4.238) — shared progress driver, tiered unload abandon
- [x] Descriptor parity Phases 0–3 (v2.4.94) — `SituationLoadShaderFromSpirvMemoryEx` + all three layout profiles
- [x] **`UBO_SSBO_SAMPLER` harness coverage (v2.4.278)** — library guard + full pixel readback test — see §1
- [ ] **[Open]** Async hardening Phases B–E — see §2
- [ ] **[Open]** Vulkan shader cache — see §3
- [ ] **[Open]** Demon Hunt Steps A/D — see §4

---

## §1 — `UBO_SSBO_SAMPLER` harness coverage

**Context:** `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` is fully wired in the library —
pipeline layout created at init, all pipeline variants built, bind path routes set 0→UBO /
set 1→SSBO / set 2→`SituationCmdBindTextureSet`. It has never been exercised by a test.

**Full task list in:** `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md` Phase 4.

### 1.1 — Library guard

- [x] Audit `_SituationVulkanResolveBufferDescriptor` set 2 path under `UBO_SSBO_SAMPLER` — the
  current `else` branch now returns `SITUATION_ERROR_INVALID_PARAM` with a clear message when
  `SituationCmdBindDescriptorSet` is called with a buffer on set 2 (must use `CmdBindTextureSet`)

### 1.2 — Harness shader

- [x] Created `tests/harness/shaders/harness_ubo_ssbo_sampler_vk.fs`

### 1.3 — Compile pipeline

- [x] Added `Invoke-GlslcCompile` entry to `compile_harness_shaders.ps1`
- [x] Added `-FsSamplerSpv` optional parameter to `scripts/gen_spirv_embed.ps1`; stub zero emitted when absent
- [x] VK embed invocation passes `-FsSamplerSpv`
- [x] Updated `tests/harness/sit_harness_spirv_embed.h` — extern declarations for both backends

### 1.4 — Descriptor spike gate

- [x] Added `"harness_ubo_ssbo_sampler_vk.fs.spv": [(0, 0), (1, 0), (2, 0)]` to `EXPECTED_VARS`
- [x] Added filename to `main()` loop in `spirv_desc_spike.py`

### 1.5 — Test

- [x] Added `spirv_harness_load_ubo_ssbo_sampler_shader()` helper
- [x] Added `test_spirv_memory_ubo_ssbo_sampler_readback()` — pixel readback R=77, G=255, B=200 ✓
- [x] Added `spirv_harness_sampler_embed_ready()` independent guard
- [x] Registered in `test_graphics.c`

### 1.6 — Verification

- [x] `compile_harness_shaders.bat` → `python scripts\spirv_desc_spike.py` — PASS (3 shaders)
- [x] `build\sit_test_vulkan.exe --module graphics --filter spirv` — **9/9** pass
- [x] `build\sit_test_vulkan.exe --module graphics` — **94/94** pass, 0 new failures
- [x] `build\sit_test.exe --module graphics --filter spirv` — **11/11** pass, OpenGL unaffected
- [ ] Vulkan validation layers — not explicitly checked (GPU: GTX 1070, no layer errors observed)

### 1.7 — Optional cleanup (Phase 4.1, parked)

- [x] `UBO_SSBO` push-constant omission documented in `_SituationVulkanInitGraphicsSpirvLayouts` source comment (deliberate; use `UBO_SSBO_SAMPLER` if push constants needed)

---

## §2 — Async shader hardening (Phases B–E)

**Context:** Phase A shipped v2.4.238. Remaining phases address Vulkan starvation, state
machine tidiness, harness timing coverage, and observability. All are Vulkan + shaderc
specific except Phase D which covers both backends.

**Full task list in:** `doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md`.

### Phase B — Starvation drive (Vulkan + shaderc)

- [ ] Detect unclaimed job after `UNCLAIMED_FAST_NS` (100 ms) in progress driver
- [ ] Retire job via `_SitThreadPoolRetireOrphanedJobMain`; set `ctx->compile_job = 0`
- [ ] Run `_SituationVkAsyncCompileWorker` inline on main after retire
- [ ] Replace bare `thrd_yield` loop in `_SituationVulkanFreeAsyncShaderLoad` with
  `SituationPollInputEvents()` + `_SitThreadSleepMs(1)` per IN_PROGRESS iteration
- [ ] Verify: 20× repeat unload-during-load → poll; p99 < 800 ms, zero `-557` / `-752` / `-99`
- [ ] Verify: `sit_test_vulkan.exe --module graphics` green; `sit_test.exe --module threading` unchanged

### Phase C — State machine cleanup (Vulkan)

- [ ] Add `#define SIT_VK_COMPILE_DONE_PENDING 0`, `_COMPILING -3`, `_SPIRV_READY 1`,
  `_FAILED -1`, `_ABANDONED -2` in `situation_impl_renderer.h`
- [ ] Replace all magic numbers `0`, `-3`, `1`, `-1`, `-2` in poll / unload / worker /
  progress driver with the named constants
- [ ] Remove any remaining duplicated LOST/timeout blocks outside `_SituationVkAsyncCompileProgress`
- [ ] Full graphics module both backends — green

### Phase D — Harness timing coverage (both backends)

- [ ] Add elapsed-ms out-param to `graphics_test_async_poll_shader_ready()` helper
- [ ] `async_shader_poll_after_unload_during_load` — assert elapsed **< 2000 ms** on Vulkan
- [ ] `async_shader_poll_after_unload_during_load` — assert success (no timing assert) on OpenGL
- [ ] `sync_shader_after_async_cycle` — reduce poll cap 3000 → 120 frames
- [ ] New test `async_shader_unload_stress_20x` — 20× unload-during-load → poll; assert pool quiescent after each (Vulkan)
- [ ] New test `async_shader_compile_verify_draw` — poll SUCCESS then draw + red pixel readback
- [ ] Register new tests in `test_graphics.c` suite table
- [ ] Run OpenGL gap checklist: async tests green on `sit_test.exe`, no leak warnings, SPIR-V async paths green
- [ ] Run 3× consecutive full test run on both backends — no flake

### Phase E — Observability

- [ ] Add `stats_async_compile_abandon` counter to `SituationThreadPoolMetrics`
- [ ] Add `stats_async_compile_lost` counter
- [ ] Add `stats_async_compile_unclaimed_steal` counter
- [ ] Expose counters in existing scheduler metrics dump / JSON export
- [ ] Harness: on async test failure, dump pool metrics to stderr (test code only, no library printf)

---

## §3 — Vulkan shader cache

**Context:** Every `SituationLoadShaderFromMemory` cold-builds shaderc output + 9–12
`vkCreateGraphicsPipelines` with zero cross-load reuse. Full Vulkan graphics test suite is
~30+ seconds vs OpenGL's seconds. The code comment in `_SituationVulkanCreateShaderModuleEx`
already acknowledges this: *"The returned module is not cached — caller is responsible for
lifetime management and reuse."*

**Full task list in:** `doc/plan/VULKAN_SHADER_CACHE_PLAN.md`.

**Exit gate (Phase 1):** second load of same minimal shader **< 3 ms** on GTX 1070 and RTX 2080.

### Phase 1 — Layer A + B + one default pipeline

#### 1A — Data structures (`situation_impl_decl.h`)

- [ ] `_SitVkShaderCacheKey` struct — `vs_spirv_hash`, `fs_spirv_hash`, `layout_profile` (Phase 1 only; struct sized for Phase 2 append)
- [ ] `_SitVkBundleState` enum — `READY`, `EVICT_PENDING`, `STALE`, `DESTROYED`
- [ ] `_SitVkPipelineBundle` struct — key, `content_hash`, generation, `ref_count`, `last_used_frame`, state, layout, modules, `default_pipeline`
- [ ] `_SitVkPipelineBundleRef` struct — bundle pointer + generation
- [ ] `_SitVkShaderCache` — hash map + LRU + `shader_cache_mutex`
- [ ] Extend `_SituationShaderSlot` — add `bundle_ref`, `vk_bound_pipeline_cache`
- [ ] Optional: `SIT_VK_SHADER_CACHE_STATS` debug counters (`#if !NDEBUG`)

#### 1B — Cache API (`situation_impl_renderer.h`)

- [ ] `_SitVkShaderCacheInit` / `_SitVkShaderCacheShutdown`
- [ ] `_SitVkShaderCacheLookupOrInsertSpirv` (Layer A)
- [ ] `_SitVkShaderCacheAcquireModules` (Layer B)
- [ ] `_SitVkShaderCacheAcquireBundle` / `_SitVkShaderCacheReleaseBundle`
- [ ] `_SitVkDerefBundle` — **`static inline`** with CRITICAL SAFETY comment; no raw `bundle_ref.bundle` outside this function (grep gate)
- [ ] `_SitVkShaderCacheProcessEvictions` — called after `_SituationFlushGraveyard` each frame
- [ ] Forward-declare all new statics in `situation_impl_renderer_fwd.h` only

#### 1C — Wire load / unload paths

- [ ] `SituationLoadShaderFromMemory` → Phase 1 cache path (Layer A + B + C acquire)
- [ ] `_SituationPollVkAsyncShaderLoad` → same acquire path after SPIR-V ready
- [ ] `SituationUnloadShader` → `ReleaseBundle` (no per-pipeline defer destroy for cached bundles)
- [ ] `SituationLoadShaderFromSpirvMemoryEx` → skip Layer A, start from Layer B

#### 1D — Tests

- [ ] `shader_cache_hit` — load same GLSL twice; second load **< 3 ms**
- [ ] `shader_cache_reuse_after_unload` — load → unload → load; draw pixel test passes
- [ ] `shader_cache_concurrent_loads` — two `BeginLoadShaderFromMemory` same source; both reach SUCCESS
- [ ] `shader_cache_no_memory_growth` — 50× load/unload; `evictions` unchanged after cycle 2; map size ≤ 1 active bundle + modules
- [ ] Full `--module graphics` Vulkan regression — no new failures

#### 1E — Phase 1 exit gate

- [ ] `shader_cache_hit` < 3 ms on **GTX 1070** — record timing in UPDATELOG
- [ ] `shader_cache_hit` < 3 ms on **RTX 2080** — record timing in UPDATELOG
- [ ] `shader_cache_reuse_after_unload` — green + pixel correct
- [ ] `shader_cache_concurrent_loads` — both handles reach SUCCESS
- [ ] 50× load/unload — no memory growth; no leak warnings at shutdown
- [ ] Debug shutdown banner shows sensible cache stats (hits ≫ misses on second full module run)
- [ ] Full graphics module — no regressions vs pre-cache baseline

### Phase 2 — Lazy variants + full key + build tickets

- [ ] Append `render_pass_compatibility_id`, `subpass_index`, `dynamic_state_mask`, `caps_fingerprint` to `_SitVkShaderCacheKey`
- [ ] Enable full-key lookup; `STALE` state + flush on render pass bump
- [ ] Extend bundle: `variants[]`, `variant_ready_mask`; lazy `_SitVkEnsurePipelineVariant`
- [ ] `_SitVkShaderBuildTicket` map — deduplicate concurrent loads of identical source
- [ ] Harden `shader_cache_concurrent_loads` — assert single shaderc job
- [ ] Add `VkPipelineCache` object; pass to all `_SituationVulkanCreateGraphicsPipeline` calls

### Phase 3 — Hot-reload + persistence

- [ ] `_SituationPerformHotReloadPass` — in-place bundle swap on existing slot using `content_hash`
- [ ] Bump compatibility key on swapchain recreate → stale flush
- [ ] Optional: disk `VkPipelineCache` load/save

---

## §4 — Demon Hunt structural refactor

**Context:** Steps B/C/E complete. OOM GPU toggle bisect (Step A) and feature re-enable
(Step D) remain.

**Full task list in:** `doc/plan/DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md`.

### Step A — Bisect on OOM GPU

- [ ] Run toggle bisect on the OOM machine (the one that previously reported `skydome shader failed:`):
  - [ ] Try `DH_ENABLE_PHASE3_SPRITES 0` → rebuild → record pass/fail
  - [ ] Try `DH_ENABLE_BLOOM 0` → rebuild → record pass/fail
  - [ ] Try `ENABLE_SPRITE_RESOLVER 0` → rebuild → record pass/fail (CPU sprites fallback)
  - [ ] `DH_ENABLE_SOFT_SHADOW 0` already default — already off
- [ ] Fill in the execution log in `DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md` with date + result
- [ ] Record the minimal toggle profile that links on the OOM GPU

### Step D — Feature re-enable (after A confirms clean link)

- [ ] `DH_ENABLE_SOFT_SHADOW 1` → rebuild → play-test wall shadow quality
- [ ] `DH_ENABLE_BLOOM 1` (if it was off during bisect) → rebuild
- [ ] `DH_ENABLE_PHASE3_SPRITES 1` (if it was off) → rebuild
- [ ] `DH_SPRITE_LITE_LIGHTING 0` only if link still OK after the above

### Step E verify

- [ ] Confirm CPU sprites still draw when `ENABLE_SPRITE_RESOLVER 0`
  (the `sky_draw_cpu_world_fallback()` fallback — never black void)

### In-game verification

- [ ] Delete `build\examples\demon_hunt_sky.log`, run `demon_hunt.exe`, stay on title screen
- [ ] `demon_hunt_sky.log` contains `shader compile/link OK`
- [ ] `demon_hunt_sky.log` contains `skydome GPU path OK`
- [ ] In play: walls / floor / sky visible (not black void); HUD does not show `World shader FAILED`
- [ ] `build\sit_test.exe --module graphics --filter demon_hunt_sky_spirv_begin_poll` — passes

---

## Reference — `spirv_shader_debug.py` output

### SPIR-V report fields

- Blob size, SPIR-V version, ID bound
- Total instruction count; per-function instruction count + basic block count
- Descriptor resources: `(set, binding, storage class)`
- Top opcodes by frequency
- Warnings: `SPIR_V_INSTR_WARN = 12000`, `SPIR_V_BYTES_WARN = 512 KB`, `NVIDIA_ASM_INSTR_WARN = 65536`

### GLSL report fields

- Function listing sorted by source line count
- Recovery toggles: `DH_ENABLE_*`, `ENABLE_*`, `DH_MAX_*`, `DH_SPRITE_*`

### Devel map (`--devel`)

Production SPIR-V uses `glslc -O` which inlines everything into a single ~77k-instruction
function. `--devel` compiles FS without `-O` → `build/examples/demon_hunt_sky.fs.devel.spv`,
giving per-function instruction counts for `cast_prim`, `shade_sprite_opaque`, `map_dda_occluded`, etc.

---

## Reference — backend comparison

| Topic | OpenGL | Vulkan |
|-------|--------|--------|
| SPIR-V compile target | `glslc --target-env=opengl` | `glslc --target-env=vulkan` |
| Auto-map locations | `-fauto-map-locations -fauto-bind-uniforms` | **not used** — explicit `layout(set=,binding=)` required |
| Typical link failure | `-641` NVIDIA "too many instructions" | `vkCreateShaderModule` / pipeline `-754`..`-756` / `-633` |
| Driver detail in error msg | full `glGetProgramInfoLog` / `glGetShaderInfoLog` | VkResult + stage label |
| Async: compile thread | GL context on main | shaderc on worker thread |
| Async: unload during load | immediate GL delete; no wait | tiered: 500 ms cooperative → 2 s abandon → 5 s deadline |
| Test exe | `sit_test.exe` | `sit_test_vulkan.exe` |

---

## Reference — error codes

**OpenGL SPIR-V:**

| Code | Meaning |
|------|---------|
| -553 | `SHADER_LOAD_IN_PROGRESS` — keep polling |
| -636 | `OPENGL_SPIRV_UNAVAILABLE` — `GL_ARB_gl_spirv` not present |
| -637 | Invalid SPIR-V blob |
| -638 / -639 / -640 | VS / FS / CS specialize failed |
| -641 | Program link failed (NVIDIA: "too many instructions") |

**Vulkan SPIR-V:**

| Code | Meaning |
|------|---------|
| -553 | `SHADER_LOAD_IN_PROGRESS` — keep polling |
| -557 | Compile timeout (shaderc wedged > 5 s) |
| -633 | Pipeline creation failed |
| -753 | `VULKAN_SPIRV_INVALID` |
| -754 / -755 / -756 | VS / FS / CS shader module creation failed |

Retrieve detail: `SituationGetLastErrorCode()` + `SituationGetLastErrorMsg(&msg)` + `SituationFreeString(msg)`.

---

## Reference — layout profiles

| Profile | Set 0 | Set 1 | Set 2 | Push constants |
|---------|-------|-------|-------|----------------|
| `MESH` (default) | dynamic UBO | sampler | — | 128 bytes |
| `DUAL_SSBO` | SSBO | SSBO | — | — |
| `UBO_SSBO` | static UBO | SSBO | — | **none** ⚠ |
| `UBO_SSBO_SAMPLER` | static UBO | SSBO | sampler | 128 bytes |

⚠ `UBO_SSBO` has `pushConstantRangeCount = 0`. Use `UBO_SSBO_SAMPLER` if push constants needed.
On **OpenGL**, `layout_profile` is always ignored.

---

## Related files

- `doc/TEST_SPIRV_SHADER_API.md` — dual-backend API test checklist, error code tables, rebuild sequences
- `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md` — layout profiles, Phase 4 full task list, consumer quick reference
- `doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md` — async contract hardening (full phase specs)
- `doc/plan/VULKAN_SHADER_CACHE_PLAN.md` — pipeline bundle cache (full design + phase specs)
- `doc/plan/DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md` — toggle bisect + execution log
- `scripts/spirv_shader_debug.py` — SPIR-V + GLSL offline analysis
- `scripts/spirv_desc_spike.py` — Vulkan harness descriptor layout gate
- `compile_demon_hunt_shaders.bat` — Demon Hunt SPIR-V compile (GL + VK + devel + embed regen)
- `compile_harness_shaders.bat` / `compile_harness_shaders.ps1` — harness SPIR-V compile + embed regen
