# Vulkan SPIR-V User Descriptor Parity Plan

**Document date**: 2026-05-21  
**Phase 0 completed**: 2026-05-21  
**Phase 0.1 completed**: 2026-05-21  
**Status**: **Complete (v2.4.94)** — Phases 0–3 shipped; test gates **G1–G3** met (**2026-05-21**); Phase 4–5 deferred  
**Related harness (v2.4.95)**: `screen_readback_corner_layout`, `cmd_draw_text_screen_layout` — same GL/VK pixel contract for framebuffer layout + text placement.  
**Library baseline**: **v2.4.95** (`SITUATION_VERSION_PATCH` 95 — *GL VK Screen Text Parity Tests*)  
**Shipped band**: **v2.4.93 – v2.4.94** (patch releases only; **not** a v2.5 minor bump)

---

## Position in the development cycle

This plan sits in the **late v2.4.x stabilization track**, **after** the OpenGL-focused SPIR-V harness work and **before** the **v2.5** minor milestone defined in [`LIBRARY_BUGFIX_PLAN.md`](LIBRARY_BUGFIX_PLAN.md).

| Cycle stage | State | This plan |
|-------------|--------|-----------|
| **v2.4.83–84** | Shipped | SPIR-V **load** on Vulkan for the **fixed mesh contract** (set 0 dynamic UBO, set 1 sampler, 128-byte push constants, three vertex-input pipelines) — same as `SituationLoadShaderFromMemory`. |
| **v2.4.91–92** | Shipped | OpenGL SPIR-V **block bind** fallbacks; harness **Phase 30** (`test_graphics_spirv.c`); Vulkan load/disk/post-link tests pass; **pixel/SSBO readback skipped** on Vulkan. |
| **v2.4.93–94 (this plan)** | **Shipped** | Vulkan **user** SPIR-V **layout profiles** (`MESH`, `DUAL_SSBO`, `UBO_SSBO`) + profile-aware **`SituationCmdBindDescriptorSet`**; harness pixel readback on Vulkan. |
| **v2.5 minor gate** | Blocked until | Full sequential harness green, Bug 6 audio, audio pipeline review — per bugfix plan. **This plan is a contributor**, not sufficient alone, for v2.5. |

**Parent / sibling plans**

- [`TEST_HARNESS_GRAPHICS_UPGRADE.md`](TEST_HARNESS_GRAPHICS_UPGRADE.md) — Phases 23–29 (OpenGL fragment SSBO); Phase 24 explicitly deferred Vulkan.
- [`TEST_HARNESS_PLAN.md`](TEST_HARNESS_PLAN.md) — Graphics module umbrella.
- [`LIBRARY_BUGFIX_PLAN.md`](LIBRARY_BUGFIX_PLAN.md) — Patch policy, v2.5 gates, Vulkan **78/78** historical scorecard (pre–Phase 30 SPIR-V tests).
- [`v2.5-api-expansion.md`](v2.5-api-expansion.md) — Shipping bar: **OpenGL + Vulkan parity** unless tagged `[GL only]` / `[VK only]`.

**Constraint from v2.5 roadmap**: Any API added here should default to **`[GL+VK]`** behavior or be explicitly tagged if intentionally one-sided.

---

## Summary

**v2.4.84 “SPIR-V Load Vulkan Parity”** means the SPIR-V **file/memory load APIs exist** on Vulkan with the **same pipeline contract as textured mesh shaders**. It does **not** mean every SPIR-V program can use custom descriptor sets like OpenGL programs after link + `SituationBindShaderStorageBlock`.

**v2.4.92 gap (closed in v2.4.93–94):** Vulkan SPIR-V load used only the **mesh** descriptor layout; harness dual-SSBO / UBO+SSBO needed **set 0 + set 1** user layouts.

**Outcome (verified 2026-05-21):**

- **OpenGL** `build\sit_test.exe --module graphics --filter spirv` — **7/7** SPIR-V tests pass (includes GL-only explicit-bind tests).
- **Vulkan** `build\sit_test_vulkan.exe --module graphics --filter spirv` — **5/5** pass; **`spirv_memory_dual_ssbo_readback`** and **`spirv_memory_ubo_ssbo_readback`** pixel readback green (no deferral).
- **Vulkan** full graphics module — **86/86** pass on reference **GTX 1070**.

**Shipped goal:** Profile-based Vulkan SPIR-V pipelines + binds for harness **set 0 + set 1** buffers without breaking default **`SituationLoadShaderFromSpirvMemory`** (mesh contract).

**Non-goals**

- Porting Demon Hunt skydome to Vulkan (OpenGL-target `.spv`, large FS, separate game effort).
- Replacing `SituationLoadShaderFromMemory` for all internal pipelines.
- Full SPIR-V reflection compiler for arbitrary games (optional long-term phase only).

---

## Gap analysis (historical — v2.4.92 → closed v2.4.94)

| Capability | OpenGL (v2.4.94) | Vulkan (v2.4.94) |
|------------|------------------|------------------|
| `SituationLoadShaderFromSpirvMemory` | GL program + optional `SituationBind*Block` | Default **MESH** profile: `dynamic_ubo_layout` + `text_sampler_layout` (unchanged) |
| `SituationLoadShaderFromSpirvMemoryEx` | Profile ignored | **DUAL_SSBO** / **UBO_SSBO** cached pipeline layouts |
| `SituationCmdBindDescriptorSet(cmd, 0/1, buffer)` | Binding points after link | Profile-aware layout/type; static UBO on **UBO_SSBO** set 0 |
| Harness `harness_dual_ssbo_vk.fs` | GL embed | **set 0 + set 1** SSBO — pixel readback **PASS** |
| Harness `harness_ubo_ssbo_vk.fs` | GL embed | **set 0** UBO + **set 1** SSBO — pixel readback **PASS** (literal GLSL sets; see Phase 0.1) |
| Demon Hunt sky (GL `.spv`) | Production path | **Out of scope** (Phase 5) |

**Original root cause:** Vulkan SPIR-V load always used mesh layouts only. **Fix:** `SituationSpirvLayoutProfile` + `SituationLoadShaderFromSpirvMemoryEx` + `_SituationVulkanResolveBufferDescriptor` (v2.4.93–94).

---

## Feasibility and approach selection

Three implementation strategies were evaluated for **realization feasibility** on this codebase (Windows, GTX 1070 class reference, existing `SituationCmdBindDescriptorSet` contract).

| ID | Approach | Feasibility | Effort (est.) | Risk | Recommendation |
|----|----------|-------------|---------------|------|----------------|
| **A1** | **Layout profiles** baked into library: `DUAL_SSBO`, `UBO_PLUS_SSBO`, keep existing `MESH_PBR` as default for memory SPIR-V when no profile flag | **High** | **3–5 dev-days** lib + **0.5 day** harness | Low–medium: API surface grows slightly; wrong profile → pipeline creation fail (testable) | **Ship first (Phases 1–3)** |
| **A2** | Parse SPIR-V (minimal reflection: descriptor sets, bindings, types) and build `VkDescriptorSetLayout` per shader | **Medium** | **3–6 weeks** | Medium–high: dynamic arrays, combined samplers, push constant overlap with existing 128-byte global contract | Phase 4+ only if A1 insufficient |
| **A3** | Caller supplies `SituationDescriptorLayoutDesc` alongside SPIR-V blobs | **High** | **2–4 dev-days** + API churn | Medium: every consumer must describe layout; Demon Hunt must adopt on GL+VK | **Hybrid**: optional desc alongside A1 default profiles |

**Decision (v1 of this plan)**: Implement **A1** for harness closure; design structs so **A3** can be added without breaking A1 callers. Defer **A2** until a second consumer needs layouts not covered by profiles.

**SPIR-V reflection spike (Phase 0)** — go/no-go for A2 later:

- Input: `tests/harness/sit_harness_spirv_vk_embed.c` blobs.
- Tooling already in tree: `glslc --target-env=vulkan1.3` via `compile_harness_shaders.ps1`.
- Spike deliverable: table of `(set, binding, type)` for dual-SSBO and UBO+SSBO FS; confirm it matches GLSL sources in `tests/harness/shaders/`.
- **Exit**: If spike matches hand-authored tables, A1 is sufficient for harness; schedule A2 only for Demon Hunt Vulkan or third-party arbitrary SPIR-V.

---

## Phased actionables

Each phase ends with a recorded harness scorecard and a **`doc/UPDATELOG.md`** entry (patch bump per [`LIBRARY_BUGFIX_PLAN.md`](LIBRARY_BUGFIX_PLAN.md)).

### Phase 0 — SPIR-V layout spike (investigation)

**Duration**: 1–2 days  
**Owner**: Library  
**Depends on**: v2.4.92 tree

- [x] Document descriptor sets/bindings for:
  - `tests/harness/shaders/harness_dual_ssbo_vk.fs`
  - `tests/harness/shaders/harness_ubo_ssbo_vk.fs`
- [x] Confirm `SituationCmdBindDescriptorSet(cmd, set_index, buffer)` semantics on Vulkan today (which pool, which layout on bind).
- [x] Identify where `_SituationShaderSlot` stores `vk_pipeline_layout` and whether per-shader layouts already exist for special internal pipelines (VD compositor precedent: multi-set layouts in v2.4.60).
- [x] Write spike notes at bottom of this file under **Phase 0 notes** (date + author).

**Acceptance**: Written binding tables; no harness change required.  
**Feasibility**: **Certain** (read-only) — **confirmed**; several plan assumptions **revised** (see Phase 0 notes).

---

### Phase 1 — Layout profiles and pipeline layout creation

**Duration**: 3–4 dev-days  
**Target version**: **v2.4.93** — **shipped**  
**Depends on**: Phase 0

**Library tasks**

- [x] Extend shader slot / load path so `SituationLoadShaderFromSpirvMemory` on Vulkan can create a **per-shader** `VkPipelineLayout` with **2 sets** (prefer **reusing** global `sit_render.vk.ssbo_layout` / `sit_render.vk.ubo_layout` — see Phase 0):
  - **Profile `SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO`**: `compute_layouts[SIT_COMPUTE_LAYOUT_TWO_SSBOS]`.
  - **Profile `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO`**: `graphics_spirv_layout_ubo_ssbo` (`ubo_layout` + `ssbo_layout`).
- [x] Preserve **default** behavior: `SituationLoadShaderFromSpirvMemory` → **`SIT_SPIRV_LAYOUT_PROFILE_MESH`**.
- [x] API: **`SituationLoadShaderFromSpirvMemoryEx`** + **`SituationSpirvLayoutProfile`** in **`sit/situation_api.h`**.
- [x] Descriptor pool / buffer lazy alloc (Phase 2): static **`ubo_layout`** for UBO profile binds.
- [x] Triple graphics pipelines (PBR / legacy / simple) per profile; harness draw uses **simple** via stride.

**Files (expected touch)**

- `sit/situation_impl_renderer.h` — Vulkan `SituationLoadShaderFromSpirvMemory` / new Ex variant
- `sit/situation_impl_decl.h` — `_SituationShaderSlot` fields if layout/pool per slot
- `sit/situation_api.h` — public profile enum + declaration

**Acceptance**

- Vulkan pipeline creation succeeds for harness SPIR-V when correct profile is selected.
- Existing mesh SPIR-V/memory loads unchanged (regression: internal + any game using mesh contract).

**Feasibility**: **High** (A1).  
**Risk**: Descriptor pool exhaustion if every shader gets a unique pool — mitigate with shared pool per profile type.

---

### Phase 2 — Bind path and command recording

**Duration**: 1–2 dev-days (may shrink — see Phase 0)  
**Target version**: **v2.4.94** (tentative)  
**Depends on**: Phase 1

- [x] When `SituationCmdBindPipeline` binds a user SPIR-V shader with profile layout, subsequent `SituationCmdBindDescriptorSet` uses **`current_pipeline_layout_for_push_constants`** + profile-aware descriptor layout/type.
- [x] Static UBO profile: **`ubo_layout`** + **`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`** for **`UBO_SSBO`** set 0.
- [x] Buffer descriptor cache rebuild when layout/type changes (`vk_cached_descriptor_*`).
- [x] **`INVALID_PARAM`** for wrong set index / buffer usage on profile shaders.
- [x] Compute path unchanged (still uses dynamic UBO / SSBO by usage).

**Acceptance**

- Manual smoke: bind dual SSBO, draw fullscreen tri, no validation-layer ERROR on GTX 1070.
- No regression on `build\sit_test_vulkan.exe --module graphics` tests that do **not** use user SPIR-V profiles.

**Feasibility**: **High**.  
**Risk**: **Medium** if global `sit_render.vk` caches assume one layout for all user shaders — audit bind cache invalidation on `SituationUnloadShader`.

---

### Phase 3 — Harness closure (remove Vulkan skip)

**Duration**: 0.5–1 dev-day  
**Target version**: **v2.4.94** — **shipped**  
**Depends on**: Phase 2

**Harness tasks**

- [x] Removed `spirv_harness_skip_vulkan_pixel_tests()`.
- [x] Harness loads with **`SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO`** / **`UBO_SSBO`** via **`SituationLoadShaderFromSpirvMemoryEx`** on Vulkan.
- [x] Bind order unchanged: set 0 = UBO, set 1 = SSBO.
- [x] Re-run (2026-05-21, GTX 1070 reference):
  ```bat
  compile_harness_shaders.bat
  python scripts\spirv_desc_spike.py
  build_situation.bat vulkan && build_tests.bat vulkan
  build\sit_test_vulkan.exe --module graphics
  build\sit_test_vulkan.exe --module graphics --filter spirv
  build\sit_test.exe --module graphics --filter spirv
  ```

**Scorecard targets**

| Build | Before (v2.4.92) | After (v2.4.94) |
|-------|------------------|------------------|
| Vulkan graphics module | **86/86** pass, **2 tests skip** pixel asserts | **86/86** pass, **0 skips** for SPIR-V pixel tests |
| OpenGL graphics module | SPIR-V filter green | **`--filter spirv`** **7/7** pass |

- [x] Update [`TEST_HARNESS_GRAPHICS_UPGRADE.md`](TEST_HARNESS_GRAPHICS_UPGRADE.md) verified table with Vulkan SPIR-V row.
- [x] Update [`LIBRARY_BUGFIX_PLAN.md`](LIBRARY_BUGFIX_PLAN.md) Vulkan graphics subsection (**86** tests on **`sit_test_vulkan.exe`**).

**Acceptance**: `spirv_memory_dual_ssbo_readback` and `spirv_memory_ubo_ssbo_readback` pass on Vulkan with same pixel tolerances as OpenGL (±10 per channel).  
**Feasibility**: **Certain** once Phases 1–2 land.

---

### Phase 4 — Optional generic reflection (deferred)

**Duration**: 3–6 weeks  
**Status**: **Deferred** until a consumer needs layouts outside A1 profiles  
**Depends on**: Phase 0 spike + product request

- [ ] Integrate minimal SPIR-V parser or shaderc reflection for descriptor sets.
- [ ] Auto-build `VkDescriptorSetLayout` + pool policy.
- [ ] Consider unifying with `SituationLoadShaderFromMemory` shaderc path.

**Feasibility**: **Medium**. Not required for harness or v2.5 gate.

---

### Phase 5 — Demon Hunt Vulkan skydome (explicitly separate)

**Not part of this plan’s closure criteria.**

- Requires Vulkan-target GLSL/SPIR-V, shader size limits, and game-side descriptor contract.
- Track separately when game port starts; may consume Phase 4.

---

## Test gates (definition of done)

| Gate | Command | Pass criterion |
|------|---------|----------------|
| **G1** | `build\sit_test_vulkan.exe --module graphics --filter spirv` | All SPIR-V tests pass; **no** deferred skip messages for pixel readback |
| **G2** | `build\sit_test_vulkan.exe --module graphics` | Module scorecard: **0 failed** (count per harness printout) |
| **G3** | `build\sit_test.exe --module graphics --filter spirv` | OpenGL SPIR-V regression unchanged |
| **G4** | Validation layers optional | No new ERROR on dual-SSBO / UBO+SSBO draw during G1 |

**Plan status = Complete** — G1–G3 met and documented in the **Verified snapshot** table below (**2026-05-21**).

---

## Verified snapshot (fill when phases land)

| Date | Library | Vulkan `sit_test_vulkan` graphics | OpenGL `sit_test` SPIR-V filter | Notes |
|------|---------|-----------------------------------|--------------------------------|-------|
| 2026-05-21 | v2.4.92 | 86/86 (2 pixel tests skip) | Green | Plan authored; work not started |
| 2026-05-21 | v2.4.92 | (unchanged) | Green | **Phase 0 spike complete** — plan revised; implementation not started |
| 2026-05-21 | v2.4.92 | (unchanged) | Green | **Phase 0.1** — vk `glslc` flags + UBO/SSBO GLSL recipe; `spirv_desc_spike.py` PASS |
| 2026-05-21 | v2.4.94 | **86/86**, spirv **5/5** | spirv **7/7** | **Plan complete** — Phases 0–3; harness `harness_ubo_ssbo_vk.fs` literal sets; `spirv_desc_spike.py` gate |

---

## Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking v2.4.84 mesh SPIR-V contract | Low if default profile preserved | High | Keep default layout; add harness-only Ex/profile path; regression `spirv_disk_roundtrip` + existing Vulkan graphics suite |
| Descriptor pool / set leaks per shader | Medium | Medium | Unload path destroys layout + pool; extend teardown tests |
| Wrong profile → silent wrong pixels | Medium | High | Pipeline creation failure or validation-layer error; harness pixel asserts |
| ABI break on `SituationLoadShaderFromSpirvMemory` | Low | High | Add `…Ex` or new enum parameter with default profile |
| Scope creep into A2 reflection | Medium | Schedule slip | Phase 4 explicitly deferred; Phase 0 only informs future |

---

## Related changelog entries

| Version | Topic |
|---------|--------|
| v2.4.83 | OpenGL SPIR-V load + harness Phases 23–29 (GL) |
| v2.4.84 | Vulkan SPIR-V **disk** load — mesh layout |
| v2.4.92 | SPIR-V harness Phase 30; OpenGL block bind; Vulkan skip documented |
| v2.4.93 | Vulkan SPIR-V layout profiles (`SituationLoadShaderFromSpirvMemoryEx`) |
| v2.4.94 | Profile-aware descriptor bind; harness Vulkan pixel readback |

---

## Phase 0 notes

**Spike date**: 2026-05-21  
**Author**: Cursor agent  
**Tooling**: `scripts/spirv_desc_spike.py` on `tests/harness/spirv_out/*.spv` (regenerate via `compile_harness_shaders.ps1`).

### Binding tables (canonical for Vulkan implementation)

Source GLSL lives in `tests/harness/shaders/*_vk.fs`. **Runtime truth for Vulkan is the `.spv` on disk / embed**, not the GLSL file when they disagree.

#### `harness_dual_ssbo_vk.fs` — **matches GLSL `set` intent**

| Resource | GLSL | SPIR-V (`OpDecorate` / `OpVariable`) | Harness bind today |
|----------|------|--------------------------------------|-------------------|
| BlockA | `set=0, binding=0` SSBO | set **0**, binding **0** | `SituationCmdBindDescriptorSet(cmd, 0, ssbo_a)` |
| BlockB | `set=1, binding=0` SSBO | set **1**, binding **0** | `SituationCmdBindDescriptorSet(cmd, 1, ssbo_b)` |

**Pipeline layout (Phase 1):** `{ ssbo_layout, ssbo_layout }` — same as internal `SIT_COMPUTE_LAYOUT_TWO_SSBOS` (`sit/situation_impl_renderer.h` ~9310).

#### `harness_ubo_ssbo_vk.fs` — **Phase 0.1** (after compile fix)

| Resource | SPIR-V (canonical) | Harness bind |
|----------|-------------------|--------------|
| Frame UBO | set **0**, binding **0** | `cmd, 0, ubo` |
| TagBlock SSBO | set **1**, binding **0** | `cmd, 1, ssbo` |

**Pipeline layout (Phase 1):** `{ ubo_layout, ssbo_layout }`.

**Compile (Phase 0.1 + fix):** Vulkan `glslc` **without** `-fauto-map-locations`; use **literal** `set = 0` on Frame / `set = 1` on TagBlock in `harness_ubo_ssbo_vk.fs` (an earlier inverted “recipe” emitted TagBlock@0 / Frame@1 and broke Vulkan pixel readback until corrected). Verify with `python scripts/spirv_desc_spike.py` after every harness SPIR-V regen.

---

### `SituationCmdBindDescriptorSet` — Vulkan (v2.4.94)

| Step | Behavior |
|------|----------|
| Lazy alloc | `_SituationVulkanEnsureBufferDescriptorSet` allocates/updates when layout or type changes |
| Layout pick | **`_SituationVulkanResolveBufferDescriptor`** — profile from `current_bound_shader_slot->vk_spirv_layout_profile`; compute path unchanged |
| **UBO_SSBO** | Set 0 → `ubo_layout` + `UNIFORM_BUFFER` (static); set 1 → `ssbo_layout` + `STORAGE_BUFFER` |
| **DUAL_SSBO** | Both sets → `ssbo_layout` + `STORAGE_BUFFER` |
| **MESH** (default) | Storage → `ssbo_layout`; else → `dynamic_ubo_layout` + `UNIFORM_BUFFER_DYNAMIC` |
| Record | `vkCmdBindDescriptorSets` with `current_pipeline_layout_for_push_constants` or compute layout |

**OpenGL difference (harness-relevant):** `set_index` → `glBindBufferBase` **binding point**, not Vulkan descriptor set. Vulkan harness uses **`SituationLoadShaderFromSpirvMemoryEx`** + explicit set indices; OpenGL may use **`SituationBindUniformBlock`** / **`SituationBindShaderStorageBlock`** after load.

---

### `_SituationShaderSlot` / layout precedent

| Item | Location | Notes |
|------|----------|-------|
| Per-shader `vk_pipeline_layout` | `sit/situation_impl_decl.h` `_SituationShaderSlot` | Created per `SituationLoadShaderFromSpirvMemory` / `SituationLoadShaderFromMemory`; destroyed in `SituationUnloadShader` |
| Global layouts | `sit_render.vk.ssbo_layout`, `ubo_layout`, `dynamic_ubo_layout`, `text_sampler_layout`, … | Init ~5702–5748 `situation_impl_renderer.h` |
| Multi-set **graphics** precedent | VD advanced compositor (v2.4.60) | Three-set **internal** pipeline — not user SPIR-V |
| Multi-set **compute** precedent | `SIT_COMPUTE_LAYOUT_TWO_SSBOS` | **Two× `ssbo_layout`** — direct template for `DUAL_SSBO` graphics profile |

**Phase 1 need not invent new `VkDescriptorSetLayout` objects** for harness closure — only new **`VkPipelineLayout` handles** (cached globals) + profile tag on shader slot / `…Ex` loader.

**Push constants:** Harness FS has none; user SPIR-V loader still adds 128-byte range (same as mesh). Valid if unused in SPIR-V; keep for mesh-profile default.

**Draw path:** `SituationCmdDrawMesh` selects `vk_pipeline_simple` when `vertex_stride <= 12` — harness fullscreen mesh OK.

---

## Plan revisions after Phase 0

| Original actionable | Phase 0 verdict | Revised action |
|--------------------|-----------------|----------------|
| Phase 1: new descriptor set layouts per profile | Assumed new layouts | **Reuse** `ssbo_layout` / `ubo_layout`; add cached **pipeline layouts** only |
| Phase 1: UBO+SSBO = set0 UBO, set1 SSBO | Phase 0 thought inverted | **Confirmed Phase 0.1:** `{ ubo_layout, ssbo_layout }`; harness binds unchanged |
| Phase 2: mostly pipeline-layout routing | “High feasibility, bind path” | **Required:** static UBO descriptor alloc/write path (not dynamic-only) |
| Phase 2: full bind rewrite | Implied large change | **Narrow:** layout handle already per-shader; fix **ubo vs dynamic** selection + optional profile-aware layout for lazy alloc |
| Phase 3: same bind order as OpenGL | Assumed VK swap | **Keep** `set 0` UBO / `set 1` SSBO (Phase 0.1 bytecode) |
| A1 effort 3–5 days | — | Still plausible; Phase 2 UBO fix is the unknown slice (0.5–1.5 d) |
| Phase 4 reflection | Deferred | Unchanged — bytecode ≠ GLSL under `-fauto-map-locations` reinforces deferral |

**A1 remains recommended.** Phase 0 does **not** justify jumping to A2 for harness-only closure.

## Phase 0.1 — Harness compile hygiene (2026-05-21)

**Agreed / done:**

- [x] Remove `-fauto-map-locations` from Vulkan harness `glslc` flags (`compile_harness_shaders.ps1`).
- [x] Regenerate `tests/harness/spirv_out/*.spv` and `sit_harness_spirv_vk_embed.c`.
- [x] Adjust `harness_ubo_ssbo_vk.fs` qualifiers so **emitted** SPIR-V is set **0** = Frame UBO, set **1** = TagBlock SSBO (literal `set = 0` / `set = 1`; inverted recipe was wrong).
- [x] Add `scripts/spirv_desc_spike.py` gate (`EXPECTED_VARS`) — run after every harness SPIR-V regen.

**Findings:**

| Hypothesis | Result |
|------------|--------|
| `-fauto-map-locations` caused UBO/SSBO set swap | **Rejected** — bytecode unchanged for set assignment when only that flag was removed (Phase 0 vs 0.1). |
| Inverted “recipe” (`set=1` Frame / `set=0` Tag) emits Frame@0 Tag@1 | **Rejected** — actually emitted **TagBlock@0 / Frame@1**; broke pixel readback until corrected. |
| Literal `set = 0` Frame / `set = 1` TagBlock | **Accepted** — matches harness binds and `{ ubo_layout, ssbo_layout }`; verify with `spirv_desc_spike.py`. |
| Phase 3 needs inverted Vulkan binds | **Rejected** — keep `set 0` = UBO, `set 1` = SSBO. |

**Dual SSBO:** unchanged; still set **0** + set **1**, `{ ssbo_layout, ssbo_layout }`.

---

**Author**: Cursor agent (user-requested plan artifact)  
**Next action**: None for Phases 0–3 (complete). **Phase 4** (SPIR-V reflection / arbitrary layouts) or **Phase 5** (Demon Hunt Vulkan skydome) only when a product owner requests them.

### Consumer quick reference (v2.4.94)

```c
// Default mesh SPIR-V (unchanged)
SituationLoadShaderFromSpirvMemory(vs, vs_len, fs, fs_len, &shader);

// Harness / custom two-set SPIR-V (Vulkan)
SituationLoadShaderFromSpirvMemoryEx(vs, vs_len, fs, fs_len,
    SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO, &shader);
SituationLoadShaderFromSpirvMemoryEx(vs, vs_len, fs, fs_len,
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO, &shader);

SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, buffer_a); // set 0
SituationCmdBindDescriptorSet(cmd, 1, buffer_b); // set 1
```

After harness GLSL changes: `compile_harness_shaders.bat` then `python scripts\spirv_desc_spike.py`.
