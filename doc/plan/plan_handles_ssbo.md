# Plan: Universal Handles & SSBO Standardization — Hardened Roadmap

**Last reviewed:** 2026-06-24 (v2.4.352 D1 audit — solid DrawQuad set-1 bind + B-L6 lifecycle note)
**Goal:** Drive the renderer to a fully bindless internal architecture — no per-draw descriptor set allocation, no fixed-function vertex attribute paths, no legacy binding tables. Every resource is addressable by a GPU-resident 64-bit handle or a stable slot index into a globally-resident descriptor array.

**North star (why these plans matter):** A **powerful scaler sprite system** — atlased sprites, arbitrary scale/9-slice, depth sort, and **thousands of draws/frame** without per-sprite descriptor churn. That system sits **on top of** this roadmap, not beside it. See **Phase G** below.

**Next milestone:** **Phase D0** — prove `global_textures[]` sampling in an **isolated harness test** (reinstate cancelled B′5 as a **hard gate**). **Do not touch internal pipelines until D0 passes.** Root-cause work (D3) runs in parallel only via capture/SPIR-V analysis — not another broad internal migration.

---

## ⚠ Phase D bindless internal migration — tried and failed (v2.4.334 → v2.4.335 backtrack)

**This is a deliberate project mark.** Phase D’s core goal — internal draws sample via `global_bindless_set` + push `texture_id`, no per-draw sampler sets — was **attempted twice** on Vulkan and **failed both times** on the reference machine.

| Attempt | Version | Outcome |
|---------|---------|---------|
| **1** | v2.4.170 → **v2.4.171** | Internal quad `global_textures[]` → **all-black readback**. Fixed by reverting to per-texture `single_sampler_descriptor_set` + dedicated `u_QuadTexture`. |
| **2** | v2.4.334 → **v2.4.335** | Retried bindless after infra fixes (`UPDATE_AFTER_BIND`, variable-count alloc, D5 frame bind). Extended to text / YPQ / VD. **Same failure** — all-black / wrong pixels on GTX 1070. **Full revert** to v2.4.171 sampler model; D5 pre-bind disabled. |

### What v2.4.334 tried

- Pipeline layout set 1 = `bindless_descriptor_layout` on internal quad (then text, YPQ, VD).
- Fragment shaders: `layout(set=1,binding=0) uniform sampler2D global_textures[]` + `nonuniformEXT(pc.texture_id)`.
- D5: `_SitVulkanBindGlobalBindlessSet` once per frame at acquire.
- Per-draw: push `texture_id`; drop `single_sampler_descriptor_set` bind.
- Infra: `descriptorBindingSampledImageUpdateAfterBind`; `VkDescriptorSetVariableDescriptorCountAllocateInfo` on `global_bindless_set` alloc; `_SitVulkanWriteSlotToGlobalBindlessSet` before draw.

### Why we call it failed (not “deferred”)

- Harness readback went **black** across quad, text, YPQ, and VD — not a single missing call site.
- Hardcoded bindless index **0** or **1** in FS still black → sampling the array itself is broken on internal pipelines, not just push-constant plumbing.
- User-shader path binding `single_sampler_descriptor_set` at set 1 **continued to pass** → bindless **pool writes** work; **array sampling in internal FS** does not.
- v2.4.335 **reverted** shaders, pipeline layouts, and draw-time binds. Tests green again only after that backtrack.

### What v2.4.335 kept (scaffolding only)

- Push `texture_id` in all internal push blocks (including VD Path A/B size bump to 120/104).
- `_SitVulkanWriteSlotToGlobalBindlessSet` at texture create / draw (user bindless API; slot table stays populated).
- Bindless pool infra fixes from the attempt (slot updates; not the broken sampling path).

### What remains open

- **Root cause** of `global_textures[]` sampling failure — **unknown**; D0.3 / D0.5 checklist must run before a third broad migration.
- **D0** — no passing harness test has ever sampled the bindless **array** on Vulkan (only `single_sampler` paths pass).
- **D2 goal**: migrate internal draws off `single_sampler_descriptor_set` — **not achieved**, **blocked on D0**.
- **D3**: cannot remove `single_sampler` pools until D2 bindless slices all green.

See **`doc/UPDATELOG.md`** v2.4.334 (failed attempt) and v2.4.335 (backtrack + verification).

### Lesson — why this route failed (and how the plan prevents a third attempt)

Phase **B′** succeeded because it shipped a **minimal harness proof before productization**. Phase **D** failed because it skipped that step:

| B′ (worked) | D (failed twice) |
|-------------|------------------|
| `test_vertex_pull_render` before `CreateMeshEx` / examples | Internal `quad.frag` / text / VD changed **before** any green `global_textures[]` test |
| Pixel-compare gate between VAO and pull path | Infra fixes (`UPDATE_AFTER_BIND`, variable-count alloc) treated as functional proof |
| B′5 bindless draw test **cancelled** as “optional” | `descriptor_bind_sampled_texture` only exercises **`single_sampler`** — never the bindless array |
| One vertical slice, then expand | Slice 2 (text / YPQ / VD) landed while slice 1 was still black |

**Plan fix:** add **Phase D0** (proof gate) and **hard no-go rules** below. D2 internal migration is **forbidden** until D0 exit criteria are met on the reference GTX 1070.

---

## Architectural Summary (What "All Bindless Internally" Means)

### Target state

```
Texture  →  slot_index into global_bindless_set (VK) / ARB_bindless_texture handle (GL)
Buffer   →  GPU virtual address via vkGetBufferDeviceAddress (VK) / NV BDA (GL where supported)
Mesh     →  vertex buffer GPU VA passed as push constant → shader pulls via gl_VertexIndex (VK)
             falls back to VAO/VBO on GL unless NV_shader_buffer_load is available
Shader   →  push constant block carries: texture_id (uint32), model matrix (mat4), BDA (uint64)
             no per-draw BindDescriptorSets except the one-time global_bindless_set bind at frame start
```

### What is already built (through v2.4.330)

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Vulkan global bindless descriptor set | ✅ **Implemented** | `global_bindless_set` / `global_bindless_pool` / `bindless_descriptor_layout`; `PARTIALLY_BOUND + UPDATE_AFTER_BIND`; sampled textures written at `SituationCreateTexture`; storage-only textures mirrored (v2.4.58+); storage images skip ARB residency (v2.4.325) |
| Vulkan descriptorIndexing + bufferDeviceAddress feature gating | ✅ **Implemented** | Queried in `VkPhysicalDeviceVulkan12Features`; `SIT_FEATURE_BINDLESS_TEXTURES` / `SIT_FEATURE_BINDLESS_BUFFERS` on `enabled_features_mask` |
| `SituationGetTextureHandle` (VK) | ✅ **Hardened (A2)** | Validates liveness via `_SitGetTextureSlot`; stale → 0 + `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` |
| `SituationGetTextureHandle` (GL) | ✅ **Implemented** | `glGetTextureHandleARB` + resident handle; 0 when `!GL_ARB_bindless_texture` |
| `SituationGetBufferDeviceAddress` (VK) | ✅ **Implemented** | `vkGetBufferDeviceAddress` for buffers with `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` |
| `SituationGetBufferDeviceAddress` (GL) | ✅ **Hardened (A1)** | `SITUATION_ERROR_OPENGL_UNSUPPORTED` on non-NVIDIA instead of silent 0 |
| `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` flag | ✅ **Implemented** | Maps to `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`; used by SSBO_COMPUTE preset |
| Vulkan mesh buffers (SSBO + BDA) | ✅ **Implemented (B1)** | Vertex + index: `VERTEX \| STORAGE \| TRANSFER_DST \| SHADER_DEVICE_ADDRESS` |
| `SituationGetMeshVertexBufferAddress` / `GetMeshIndexBufferAddress` | ✅ **Implemented (B2–B3)** | VK BDA; GL NV path; stale + unsupported errors (v2.4.260) |
| `sit/gpu/vertex_pull.glslh` | ✅ **Shipped (C3 infra)** | Canonical `buffer_reference` structs for Simple/Legacy/PBR + `SitIndexBuffer` (v2.4.263). See AAA Architecture §2. |
| `SituationCmdSetVertexAttribute` | ✅ **Deprecated v2.4** | Migration note → vertex pull + `vertex_pull.glslh` |
| GL virtual bindless fallback (LRU texture unit cache) | ✅ **Implemented** | `_SituationGLVirtualTextureSlot[32]` |
| Harness: handle / address smoke tests | ✅ **Partial** | `test_get_texture_handle`, `test_buffer_device_address`, `test_mesh_vertex_buffer_address` assert nonzero + stale-handle; **no end-to-end shader proof yet** → **B′** |
| Vertex-pull **draw path** (user or harness) | ❌ **Not implemented** | BDA is query-only; all production draws still use VAO/VBO attribute fetch |
| Internal draws use BDA / bindless uniformly | ❌ **Blocked — tried and failed** | v2.4.171 per-texture sampler restored (v2.4.335); v2.4.334 bindless retry failed — see backtrack section |

---

## Gap Analysis

### Resolved (Phases A–B, v2.4.259–v2.4.263)

| Gap | Was | Fixed in |
|-----|-----|----------|
| **1** — Mesh vertex buffer missing `SHADER_DEVICE_ADDRESS_BIT` | Invalid BDA on mesh buffers | B1 (v2.4.260) |
| **2** — GL `GetBufferDeviceAddress` silent zero | No unsupported signal | A1 (v2.4.259) |
| **3** — Harness handle tests no-op | Always passed | A3/A4 partial — handle validation (v2.4.259); shader e2e → **B′4/B′5** |
| **4** — No mesh BDA API | Couldn't write pull shaders | B2–B3 (v2.4.260) |
| **7** — VK `GetTextureHandle` no slot validation | Stale slot index risk | A2 (v2.4.259) |

### Remaining — blocks full bindless internally

**Gap 5 — Internal draws still per-draw bind (Phase D bindless migration failed)**
Text, quad, YPQ, VD compositor use per-texture `single_sampler_descriptor_set` (v2.4.171 model, restored v2.4.335). v2.4.334 **attempted** frame-level `global_bindless_set` + push `texture_id` → **all-black readback on GTX 1070** → **reverted**. Root cause **unknown**. → **Phase D0** (isolated harness proof) before any D2 retry; **D0.5** for capture/SPIR-V diagnosis.

**Gap 6 — No vertex-pull runtime integration**
`vertex_pull.glslh` exists but nothing in harness or examples renders via BDA. `SituationCreateMeshEx` / `CreateFullscreenTriangleMesh` (v2.5 Phase 5a) still missing for large example migrations. → **Phase B′** (proof), then **Phase C** (productization)

**Gap 8 — BDA addresses not proven in a draw** *(new, v2.4.330 review)*
Phase B closed when query APIs returned nonzero in smoke tests. Users still cannot follow a single documented path from `SituationGetMeshVertexBufferAddress` to pixels on screen. → **Phase B′** (primary next work)

---

## Phase Plan

### Phase A — Hardening & correctness of what's already built
*Target: v2.4.x. Shipped v2.4.259.*

- [x] **A1** — `SituationGetBufferDeviceAddress` GL: emit `SITUATION_ERROR_OPENGL_UNSUPPORTED` when NV/EXT BDA unavailable.
- [x] **A2** — `SituationGetTextureHandle` VK: validate slot liveness via `_SitGetTextureSlot`.
- [x] **A3** — Harness `test_get_texture_handle`: nonzero when supported; stale → 0.
- [x] **A4** — Harness `test_buffer_device_address`: nonzero / unsupported error contract.
- [x] **A5** — `SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED` for GL mesh BDA without NV extension.

---

### Phase B — Mesh vertex buffer device address
*Target: v2.4.x. Shipped v2.4.260. Prerequisite for vertex pulling.*

- [x] **B1** — `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` on all VK mesh vertex/index buffer creation in `SituationCreateMesh` *(and `SituationCreateMeshEx` when v2.5 Phase 5a lands)*.
- [x] **B2** — `SituationGetMeshVertexBufferAddress(SituationMesh) → uint64_t` (VK BDA; GL NV).
- [x] **B3** — `SituationGetMeshIndexBufferAddress(SituationMesh) → uint64_t`.
- [x] **B4** — Harness `test_mesh_vertex_buffer_address`: addresses nonzero + stale → 0. *(Compute read of `vertex[0]` → **B′4**.)*
- [x] **B5** — API docs: BDA invalid after `SituationDestroyMesh` without GPU fence.

---

### Phase B′ — Bridge to C (vertex-pull vertical slice)
*Target: v2.4.x patch release(s). **Active next milestone.** Depends on Phase B.*

Phase B delivered **query APIs**. Phase B′ proves those addresses are **usable in one real draw** before Phase C adds layout enums, bind helpers, and large example migrations.

**In scope:** Vulkan-first harness proof, minimal documented draw recipe, one small example.
**Out of scope:** `SIT_MESH_LAYOUT_PULL`, `CreateMeshEx`, `fps_ray_demo` migration, Phase D bindless migration.

- [x] **B′1** — Harness **`test_vertex_pull_render`**: tiny mesh via `SituationCreateMesh`; render on **Vulkan** twice — standard VAO/`SituationCmdDrawMesh` vs pull shader using `SituationGetMeshVertexBufferAddress` + `sit/gpu/vertex_pull.glslh`; readback pixels; assert match (or tight tolerance). GL: skip pull path or VAO-only reference. *(v2.4.331)*
- [x] **B′2** — Document minimal pull draw recipe (`situation_api.h` + `tests/harness/sit_graphics_test_helpers.h`): BDA → push constant → SPIR-V with `buffer_reference` → draw. **`SituationCmdBindMeshPullBuffers` deferred to C2.** *(v2.4.331)*
- [x] **B′3** — Example **`examples/other/vertex_pull_triangle.c`**: smallest user-facing reference for BDA + pull VS. *(v2.4.331)*
- [x] **B′4** — Extend mesh BDA harness: compute dispatch reads `vertices[0].pos` via vertex BDA, readback `float3`, compare to upload data (completes original **B4** shader-test intent). *(v2.4.331 — `test_mesh_bda_compute_read`)*
- [x] ~~**B′5**~~ — *Was cancelled as required work (v2.4.331) — **mistake**; that gap enabled the Phase D fail route. **Reinstated as mandatory Phase D0.1** (`test_bindless_texture_in_shader`). Do not mark D2 done without it.*

**Exit criteria:** B′1 green on Vulkan CI/reference machine; B′2 published; at least one of B′3/B′4 landed. **Met (v2.4.331).**

---

### Phase C — Vertex pull pipeline (productization)
*Target: v2.4.x / v2.5 Phase 5a. Depends on **Phase B′**.*

- [x] **C1** — `SituationMeshVertexLayout` enum (`POS_NRM_TEX`, `POS_ONLY`, `POS_TEX`, `POS_NRM`, **`SIT_MESH_LAYOUT_PULL`**); `SituationCreateMeshEx`, `SituationGetMeshVertexLayout`. *(v2.4.332)*
- [x] **C2** — `SituationCmdBindMeshPullBuffers(cmd, mesh)` pushes `SituationMeshPullPushConstants` @ offset 0. *(v2.4.332)*
- [x] **C3** — Standard pull-model GLSL include — **`sit/gpu/vertex_pull.glslh`** (v2.4.263). Supersedes the inline snippet below; three mesh layouts + index buffer + `sit_bitangent()`.
  ```glsl
  /* Original plan sketch — see vertex_pull.glslh for canonical structs */
  layout(push_constant) uniform PC { uint64_t vertex_ptr; mat4 mvp; } pc;
  SitVertexBuffer_Legacy verts = SitVertexBuffer_Legacy(pc.vertex_ptr);
  SitVertex_Legacy vert = verts.data[gl_VertexIndex];
  ```
- [x] **C4** — `shader_lab_torus` + `shader_lab_raytrace2`: pull primary on Vulkan when BDA available, VAO fallback on GL / unsupported. *(v2.4.333)*
- [x] **C5** — Re-run B′1 acceptance through **C1/C2** APIs (`CreateMeshEx` + `SituationCmdBindMeshPullBuffers`). Pixel-compare harness stays the gate. *(v2.4.332 — `test_vertex_pull_render`)*

---

### Phase D — Global bindless set as the sole frame-level binding
*Target: v2.5 / v2.6. Internal renderer refactor.*

**Order is mandatory.** Do not reorder D0 → D2 slices. Safe parallel work: **D4** (docs), **D6** (GL LRU) — no internal VK shader changes.

#### D0 — Bindless array proof gate *(new — blocks all D2 bindless retries)*

Prove `global_textures[]` sampling **outside** internal pipelines before touching `sit/gpu/*.frag` again.

- [ ] **D0.1** — Harness **`test_bindless_texture_in_shader`** (reinstated from cancelled B′5):
  - Minimal FS: `layout(set=1,binding=0) uniform sampler2D global_textures[]` + `#extension GL_EXT_nonuniform_qualifier : require`.
  - Pipeline layout set 1 = **`bindless_descriptor_layout`** (not `text_sampler_layout`).
  - Bind **`global_bindless_set`** only — **do not** bind `single_sampler_descriptor_set`.
  - Upload solid-color 4×4 texture; assert readback matches (same bar as `descriptor_bind_sampled_texture`).
  - Subtests (incremental, stop at first failure):
    1. Hardcoded `global_textures[0]` (no push constant).
    2. Push `texture_id = slot_index` + `nonuniformEXT(texture_id)`.
    3. Second texture at slot 1 — index 1 samples correctly.
- [ ] **D0.2** — **`SituationCmdBindTextureSet` bindless branch** — add harness path that forces `global_bindless_set` bind (bypass `single_sampler` shortcut) and passes D0.1 asserts.
- [ ] **D0.3** — Optional diagnostics when D0.* fails (required before declaring “unknown forever”):
  - Validation layers on during test only.
  - One RenderDoc capture: bound descriptor sets vs pipeline layout at draw.
  - SPIR-V dump: array descriptor + `NonUniform` on dynamic index.

**D0 exit criteria (all required on reference GTX 1070 + `build_tests.bat static-vulkan` relink):**

| Gate | Must pass |
|------|-----------|
| `test_bindless_texture_in_shader` | ✅ all subtests |
| `descriptor_bind_sampled_texture` | ✅ still green (no regression) |
| Internal pipelines | **unchanged** — still `single_sampler` |

**No-go:** merging any PR that changes internal FS to `global_textures[]` while D0 is red.

#### D0.5 — Root-cause record *(parallel to D0, does not unblock D2 by itself)*

- [ ] **D0.5.1** — Document findings in this plan + UPDATELOG (even if “driver bug” or “layout flag X missing”).
- [ ] **D0.5.2** — If D0 stays red after diagnostics: file a **minimal repro** (standalone ~100-line Vulkan sample or harness-only) — do not debug via full internal pipeline churn.

#### Hard no-go rules (paliate the obvious fail route)

1. **No internal shader migration** until **`test_bindless_texture_in_shader` is green**.
2. **One pipeline per slice** — quad only after D0; text only after quad slice green; YPQ/VD last.
3. **Pixel gate between slices** — same tests as v2.4.335 backtrack table must pass before expanding scope.
4. **Infra ≠ proof** — pool flags, `UPDATE_AFTER_BIND`, variable-count alloc, slot writes are necessary but **never sufficient** to mark a D2 slice done.
5. **Always relink tests** — `build_tests.bat static-vulkan` after `build_situation.bat`; stale `sit_test_vulkan.exe` invalidates results (confounded v2.4.334).
6. **Keep escape hatch** — internal paths retain `single_sampler` until D3; feature flag or compile-time switch optional for bring-up, not required for ship.
7. **Canceling proof tests is a red flag** — B′5 cancellation directly preceded the D fail route; D0 reinstates it as **required**.

---

- [x] **D1** — Audit internal VK draw sites *(v2.4.334)*.
- [ ] **D2** — Migrate internal draws to `global_bindless_set` + push `texture_id` (**blocked on D0**).
  - [ ] **D2 bindless slice 1** — quad + `SituationCmdDrawTexture` via `global_textures[]` — **attempted v2.4.334, failed, reverted v2.4.335**. *Retry only after D0 green.*
  - [x] **D2 push scaffolding slice 2** — text / YPQ / VD push `texture_id`; draw bind stays `single_sampler` *(v2.4.335)*.
- [ ] **D3** — Remove `single_sampler_descriptor_set` / pool from `_SituationTextureSlot` (**blocked** on D2 bindless slices 1–N all green).
- [ ] **D4** — Document user-shader contract: set 1 = `text_sampler_layout` vs global bindless opt-in. *(Safe to do before D0 — unblocks user-facing clarity.)*
- [ ] **D5** — Bind `global_bindless_set` once per frame in `SituationAcquireFrameCommandBuffer` — **attempted v2.4.334, disabled `#if 0` v2.4.335**. *Enable only after D0.1 subtest 1 passes with frame pre-bind.*
- [ ] **D6** — GL: verify virtual bindless LRU on all internal paths; optional hit/miss stats in caps/diagnostics. *(Independent of VK D0.)*

#### D1 audit — internal Vulkan texture binding (v2.4.352 interim; post-backtrack baseline v2.4.335)

| Draw site | Pipeline layout set 1 | Binding model | Phase D status |
|-----------|----------------------|---------------|----------------|
| `SituationCmdDrawTexture` | `text_sampler_layout` | push `texture_id`; bind `single_sampler_descriptor_set` | 🔲 D2 bindless **failed** — on v2.4.171 sampler model |
| `SituationCmdDrawQuad` (solid) | `text_sampler_layout` | **`use_texture=0` but set 1 required** — bind `quad_solid_texture` `single_sampler_descriptor_set` (interim v352; **B-L6** → internal sampler outside registry) | ✅ validation / draw path; 🔲 lifecycle (**LIBRARY_RECOVERY** §B.5) |
| `SituationCmdDrawTextEx` | `text_sampler_layout` | push `texture_id`; bind font `single_sampler_descriptor_set` | ✅ push scaffolding only |
| `SituationCmdDrawTextureYpqGrade` | `text_sampler_layout` | push `texture_id`; bind texture `single_sampler_descriptor_set` | ✅ push scaffolding only |
| `SituationCmdBindTextureSet` (user shaders) | user layout | `single_sampler` @ set 1/2 or `global_bindless_set` when bindless | 🔲 D4 contract |
| VD compositor (Path A + B) | set 1 = `text_sampler_layout` (+ set 2 screen copy on Path A) | push `texture_id`; bind `vd->vk.descriptor_set` | ✅ push scaffolding only |
| Font atlas init | — | writes both `global_bindless_set[slot]` and `single_sampler` | kept until D3 (**blocked**) |
| Internal quad solid sampler (v352) | `text_sampler_layout` | 1×1 white via `SituationCreateTexture` → **user slot 0** — **not** GL-style “no texture bind” | 🔲 **B-L6** — dedicated internal image or library-owned graveyard teardown |

**Slice 1 shader (current):** `sit/gpu/quad.frag` — Vulkan FS uses `layout(set=1,binding=0) uniform sampler2D u_QuadTexture` + `pc.use_texture`. Solid draws still **bind set 1** (white sampler) even when `use_texture=0`. **Not** `global_textures[]` (bindless attempt reverted).

**Projection (v351):** Internal quad/texture/YPQ push `mat4 projection` per draw — shared view UBO is **not** authoritative across VD → main pass recording order. See **`renderer_bolster_plan.md` Phase 7-bisF**.

---

### Phase E — Buffer device address in push constants ("Mega-Shader" contract)
*Target: v2.6+. Depends on Phase B′, C, and D.*

- [ ] **E1** — `SitBindlessPushConstants` push block (mvp, vertex_ptr, index_ptr, texture_id, material_id, object_id).
- [ ] **E2** — `SIT_SPIRV_LAYOUT_PROFILE_BINDLESS` pipeline layout profile.
- [ ] **E3** — `SituationCmdDrawMeshBindless(cmd, mesh, texture, mvp)`.
- [ ] **E4** — `sit_bindless.glsl` header with `sit_fetch_vertex` / `sit_sample_texture` helpers.
- [ ] **E5** — Example `examples/bindless_mesh.c`: one draw, zero per-draw `BindDescriptorSet`.

---

### Phase F — SSBO-first canonical mesh format (optional / future)
*Parking lot. Depends on Phase C and E.*

- [ ] **F1** — VK: drop `VERTEX_BUFFER_BIT` on `SIT_MESH_LAYOUT_PULL` meshes.
- [ ] **F2** — GL: remove `SituationCmdSetVertexAttribute` (v2.5 breaking change).
- [ ] **F3** — `SituationMeshStd430Vertex` typedefs in `situation_base_types.h`.

---

### Phase G — Scaler sprite system *(north star — depends on C, DU1, D0+, E)*

*Target: v2.6+. **Not started.** This is the product reason the handles/bindless/SSBO plans exist: a library-grade **2D sprite batcher** with scaling, 9-slice, atlas sampling, sort keys, and high draw counts without the v2.4.334 fail route.*

#### What “powerful scaler sprite system” means here

| Capability | Needs from roadmap |
|------------|-------------------|
| **Atlas / multi-texture batching** | Phase **D0+** — `texture_id` → `global_textures[]` (or documented single-atlas + bindless opt-in per **D4**) |
| **Per-sprite transform / UV / color / scale mode** | Phase **DU1** — ring UBO or Phase **E** push/SSBO instance record |
| **Thousands of sprites / frame, few descriptor binds** | **D5** frame bindless + **DU1** master UBO set; optional **E3** `DrawMeshBindless` |
| **Optional GPU sort / indirect batch** | Phase **E** + AAA §3 GPU-driven (future); Demon Hunt SSBO sprite pack is precedent |
| **9-slice / non-uniform scale in shader** | Dedicated sprite VS/FS + instance struct (Phase G proper) |
| **Non-regressive bring-up** | Same gates as D0/DU1 — harness proof before replacing `SituationCmdDrawTexture` internally |

#### Phase G checklist *(actionables — blocked until prerequisites)*

- [ ] **G0** — **`doc/plan/plan_scaler_sprites.md`** — instance struct, sort policy, 9-slice contract, API sketch (`SituationSpriteBatch`, `SituationCmdDrawSpriteEx`, etc.).
- [ ] **G1** — Instance record layout in **`situation_base_types.h`** (std140 or std430): pos, pivot, scale, uv_rect, `texture_id`, color, flags (9-slice, blend, layer).
- [ ] **G2** — **CPU batch builder**: append instances → sort by `(texture_id, layer)` → single SSBO or ring upload per frame.
- [ ] **G3** — **Sprite shader** (VK+GL): sample via bindless or atlas bind; VS expands unit quad with scale + 9-slice; FS tint.
- [ ] **G4** — **Harness `test_scaler_sprite_batch`**: N sprites, mixed scales, 9-slice panel, readback spot-checks; must pass with **D0+DU1** infra, not per-sprite `single_sampler`.
- [ ] **G5** — **Example** `examples/other/scaler_sprites_demo.c` — stress batch (1k+ sprites), VD-style scaling modes where relevant.
- [ ] **G6** — Integrate with text/VD layering (sort key / render pass order doc).

**G blocked until:**

| Prerequisite | Why |
|--------------|-----|
| **D0 green** | Sprites need reliable atlas `texture_id` sampling |
| **DU1 green** | Per-frame instance stream without per-sprite UBO alloc |
| **C ✅** | Pull/SSBO upload patterns for instance buffer |
| *(optional)* **E1–E3** | Unified bindless push + single draw API |

**Today’s building blocks (not yet a sprite system):** internal `SituationCmdDrawTexture` / `DrawQuad`, text dynamic VBO batch, VD compositor scaling, Demon Hunt SSBO sprite pack in user shader space.

---

## Harness Coverage Plan

| Test name | Phase | What it proves | Status |
|-----------|-------|----------------|--------|
| `test_get_texture_handle` | A3 | Nonzero + stale handle | ✅ handle validation |
| `test_buffer_device_address` | A4 | Nonzero / unsupported error | ✅ handle validation |
| `test_mesh_vertex_buffer_address` | B4 | Mesh BDA nonzero + stale | ✅ address smoke |
| **`test_mesh_bda_compute_read`** | **B′4** | Compute reads `vertex[0]` via BDA | ✅ (VK) |
| **`test_vertex_pull_render`** | **B′1 / C5** | Pull render matches VAO render (VK); uses C1/C2 APIs | ✅ (VK) |
| **`test_bindless_texture_in_shader`** | **D0.1** | **`global_bindless_set` + `global_textures[]` sample (no `single_sampler`)** | 🔲 **required gate — blocks D2 + Phase G** |
| `test_bindless_draw_mesh` | E5 | Full `DrawMeshBindless` frame | 🔲 blocked on D0 + D2 |
| **`test_scaler_sprite_batch`** | **G4** | Atlas + scale + 9-slice batch readback | 🔲 blocked on D0 + DU1 + G1–G3 |

---

## Dependency Map

```
Phase A (hardening) ✅
    │
    ├── Phase B (mesh BDA query API) ✅
    │       │
    │       └── Phase B′ (one pull draw proves the stack) ✅ v2.4.331
    │               │
    │               └── Phase C (layout enum, bind helper, example migrations)
    │                       │      ←── v2.5 Phase 5a (CreateMeshEx, CreateFullscreenTriangle)
    │                       └── Phase E (bindless push constant contract)
    │                               └── Phase F (full SSBO-first, IA bypass)
    │
    └── Phase D (global_bindless_set as sole frame binding)
            ├── D0 proof gate (harness `global_textures[]`) ←── **YOU ARE HERE**
            ├── D0.5 root-cause / minimal repro (parallel)
            ├── D2 slices (internal migration — **blocked until D0 green**)
            ├── feeds Phase E
            └── Phase G (scaler sprite system) ←── **north star product**
                    └── needs D0 + DU1 (+ E optional)
```

---

## Cross-references

| Document | Relationship |
|----------|--------------|
| `doc/plan/AAA_ARCHITECTURE_PLAN.md` §1 | Bindless query APIs — ✅ complete |
| `doc/plan/AAA_ARCHITECTURE_PLAN.md` §2 | SSBO-first **infrastructure** (BDA, `vertex_pull.glslh`) — ✅ complete |
| `doc/plan/v2.5-api-expansion.md` Phase 5a | `CreateMeshEx`, `CreateFullscreenTriangleMesh` — blocks **C4**, not **B′** |
| `doc/plan/plan_dynamic_ubo.md` | DU0 ✅ / DU1 UBO ring (orthogonal to Phase D); AAA per-draw uniform versioning |
| `doc/plan/QUEST_RENDERER_SITUATION_API_PLAN.md` | **QSR** — stub gaps + Phase 2 APIs (`CreateTexture3D`, etc.); Phase K readback ✅ |
| `doc/plan/LIBRARY_RECOVERY_PLAN_244.md` §B.5 | Internal VK quad solid sampler lifecycle (**B-L6**); shutdown graveyard-only destroy (**B-L7**) — D1 DrawQuad row interim @ v352 |
| `doc/plan/renderer_bolster_plan.md` Phase 7-bisF | Internal 2D push-constant projection contract (VD UBO overwrite class) |

---

## What This Plan Does Not Do

- Does not change public `SituationTexture` / `SituationBuffer` / `SituationMesh` struct shapes. BDA and bindless handles are *additional* query APIs, not replacements.
- Does not require `ARB_bindless_texture` on OpenGL for the library to function. Virtual bindless LRU is the GL fallback.
- Does not break existing user code. Bindless path is additive.
- Does not target Linux/macOS until those backends land.
- Does **not** allow internal pipeline bindless migration (D2) without a green **`test_bindless_texture_in_shader`** (D0) — explicit guard against the v2.4.334 fail route.

---

## Current Status Summary (v2.4.335)

| Phase | Status |
|-------|--------|
| A — Hardening | ✅ Complete (v2.4.259) |
| B — Mesh BDA API | ✅ Complete (v2.4.260) |
| **B′ — Bridge to C** | **✅ Complete (v2.4.331)** |
| **C — Vertex pull productization** | **✅ Complete (v2.4.333)** — layout enum, bind helper, harness, shader lab examples |
| D — Global bindless as sole binding | **❌ Blocked on D0 proof gate** — v2.4.334→335 backtrack; push `texture_id` scaffolding shipped; **no retry without `test_bindless_texture_in_shader` green** |
| E — Bindless push constant contract | 🔲 Blocked on B′ + C + D |
| F — Full SSBO-first, IA bypass | 🔲 Parking lot |
| **G — Scaler sprite system** | 🔲 **North star** — blocked on **D0 + DU1**; see Phase G checklist |

**C3 (GLSL include):** ✅ shipped as `sit/gpu/vertex_pull.glslh` (v2.4.263); listed under Phase C for traceability, not blocking B′.

**Strategic order for sprites:** **DU1** (instance stream) and **D0** (atlas sampling proof) are the highest-leverage prerequisites; do not start **G2–G5** until both gates are green.
