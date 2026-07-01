# Voxel / Block-World Rendering Plan — STB Bootstrap → Situation Native

| Field | Value |
|-------|--------|
| **Status** | 🟡 **DRAFT** — `ext/stb_voxel_render.h` present (v0.89); zero Situation integration |
| **Goal** | Core: **(A)** STB bootstrap, **(B)** block spec + wall slant, **(C)** native mesher + API, **(D)** retire STB. Stretch: **(C+)** compute remesh, **(E)** dynamic lighting pass (G-buffer / Quest crossover) |
| **Asset** | [`ext/stb_voxel_render.h`](../../ext/stb_voxel_render.h) — **MIT or public domain** (dual license, Sean Barrett; use MIT + notice for Situation) |
| **Primary files (eventual)** | `sit/stb_voxel_render_bridge.c`, `sit/stb_voxel_to_sit_mesh.h`, `sit/sit_voxel*.h`, `sit/gpu/voxel*.vert/frag`, `examples/26_voxel_demo/`, `tests/harness/test_voxel.c`, `.kiro/specs/situation-voxel-slants/` |
| **Pairs with** | [`DIGESTIBLE_EXAMPLES_PLAN.md`](DIGESTIBLE_EXAMPLES_PLAN.md), [`drawing_3d.md`](../guide/drawing_3d.md), [`AAA_ARCHITECTURE_PLAN.md`](AAA_ARCHITECTURE_PLAN.md) §3, [`QUEST_RENDERER_SITUATION_API_PLAN.md`](QUEST_RENDERER_SITUATION_API_PLAN.md), [`RENDERER_SIAMESE_COLOCATION_PLAN.md`](RENDERER_SIAMESE_COLOCATION_PLAN.md), [`plan_handles_ssbo.md`](plan_handles_ssbo.md), [`renderer_bolster_plan.md`](renderer_bolster_plan.md), [`GAME_LOOP_PERFORMANCE_PLAN.md`](GAME_LOOP_PERFORMANCE_PLAN.md) |
| **Explicit non-goal** | Minecraft protocol / server; infinite MMO; macOS OpenGL voxel path (VK-first for new subsystems if GL parity slips) |

---

## Phase overview

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ Phase A — STB bootstrap (GL + VK)                                       │
│   STB mesher → quad converter → SituationMesh → draw                    │
│   Deliverable: example 26 + harness smoke; bridge OFF in default DLL    │
└───────────────────────────────┬─────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ Phase B — Spec + adapter + ONE native slant (wall)                      │
│   .kiro spec, sit_voxel_spec.h, STB adapter, wall_slant mesher stub     │
│   Deliverable: spec frozen; wall slant beside STB cubes in example 26   │
└───────────────────────────────┬─────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ Phase C — Native voxel engine + public API                              │
│   CPU mesher, geometric vheight normals, dual-backend forward shaders   │
│   Deliverable: 26 runs with STB legacy flag OFF; doc/guide/voxel.md     │
└───────────────────────────────┬─────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ Phase D — STB retirement                                                │
│   Bridge out of default build; ext/ retained for reference / import     │
└───────────────────────────────┬─────────────────────────────────────────┘
                                ↓
        ┌───────────────────────┴───────────────────────┐
        ↓ (optional, after C reference mesher)          ↓
┌───────────────────────────────┐   ┌───────────────────────────────────┐
│ Phase C+ — Compute remesh     │   │ Phase E — Dynamic lighting pass   │
│ SituationCmdDispatch → SSBO   │   │ G-buffer + compute / NRC / ReSTIR │
│ Compare to CPU; indirect draw │   │ style pass on top of voxel geometry │
└───────────────────────────────┘   └───────────────────────────────────┘
```

**Recommended order:** complete **Phase A tasks A.2.1–A.2.8** (visible mesh) before **Phase B spec writing** — concrete output validates slant design assumptions.

---

## Locked design decisions (review 2026-06-29)

| Topic | Decision |
|-------|----------|
| **STB → mesh converter** | Reusable **`sit/stb_voxel_to_sit_mesh.h`** (+ `.c` TU) — **not** buried in bridge; harness, example, and C+ hash tests all call the same API |
| **Triangulation** | Document **once** in `stb_voxel_to_sit_mesh.h`: quad corners `0–3` → tris `(0,1,2)` + `(0,2,3)`; **CCW when viewed from outside** the solid; STB axes X right, Y forward, Z up |
| **Phase A output layout** | **`SIT_MESH_LAYOUT_POS_NRM_TEX`** (32 B) — sufficient for bootstrap draw |
| **Phase C native layout** | **`SIT_MESH_LAYOUT_POS_NRM_TAN_TEX`** (48 B) default — PBR-ready; axis-aligned faces use tangent `(1,0,0,1)` until normal maps; compact 24/32 B = later optimization flag |
| **Slant enum** | Cardinal names for walls/ramps (see Phase B table); **`SIT_SLANT_VHEIGHT4`** stays one kind with corner payload |
| **Chunk mesh lifetime** | **Double-buffer from Phase C** (`mesh_active` / `mesh_building`); swap on remesh complete; graveyard old handle — C+ reuses same pattern for GPU buffers |
| **Example folder** | **`examples/26_voxel_demo/`** — user-facing showcase; harness module remains **`voxel`** |
| **C+ barriers** | Reuse SSBO/barrier **recipe** from harness **`mesh_bda_compute_read`** + vertex-pull work — not the same kernel (BDA read ≠ mesh append) |

| STB strength | STB limitation vs Situation |
|--------------|----------------------------|
| Fast path to textured block meshes | GLSL only — VK needs SPIR-V embed like other `sit/gpu/` assets |
| Compact quad format, AO, dual texture layers | **Quads, no index buffer** — not `SIT_MESH_LAYOUT_*`; needs conversion TU |
| Slabs, cardinal slopes, vheight terrain | **Wall slopes `UNIMPLEMENTED`**; slant vocabulary not extensible |
| MIT / public domain, battle-tested | Author tolerates slope/lighting bugs until a shipping project needs fixes |
| No GPU API coupling | No chunk cache, streaming, or graveyard — all host responsibility |

**End state:** generational handles, `SituationCmd*` rendering, GL/VK Siamese shaders, graveyard-safe mesh lifecycle, harness black-box tests, digestible example — same bar as other subsystems.

---

## Related examples (avoid duplicate work)

| Example / plan | Relationship to voxel |
|----------------|----------------------|
| **`26_voxel_demo`** (this plan) | **3D block-world showcase** — one chunk, orbit camera; short name **`load_voxel_demo`** in `build_examples.bat` |
| `20_load_and_draw_model` | Arbitrary mesh load — complementary; link from 26 README |
| `12_procedural_world` ([DIGESTIBLE](DIGESTIBLE_EXAMPLES_PLAN.md)) | **2D tilemap on VD** — not competing; different tier |
| `13_shader_lab_3d` (planned promotion) | Custom GLSL / torus — shader lab, not blocks |
| [`drawing_3d.md`](../guide/drawing_3d.md) | Camera, mesh, depth pass patterns for Phase A draw code |

---

## Phase A — STB bootstrap (GL + VK)

**Objective:** One static chunk meshed with unmodified STB logic, converted to `SituationMesh`, drawn with STB-derived shaders — **OpenGL and Vulkan green**.

### A.1 Scope

| In | Out (defer to B/C) |
|----|---------------------|
| Single static chunk (≤32×32×16 recommended; STB limits x/y &lt; 127, z &lt; 255) | Streaming, world save/load |
| `stbvox_make_mesh` → bridge → `SituationCreateMesh` → `SituationCmdDrawMesh` | Greedy meshing |
| Mode **20/21** (untextured) first, then Mode **0/1** (textured + array atlas) | Multiplayer |
| Example `26_voxel_demo` + harness `voxel` module | Public `SituationCreateVoxel*` API |
| Bridge TU gated **`SITUATION_ENABLE_STB_VOXEL`** (default **off** in DLL) | STB in default `situation_dll.c` |

### A.2 Coordinate system

STB preferred axes: **X right, Y forward, Z up** (see header “MESHING” section). Document in bridge README how chunk origin maps to Situation `SituationCameraDesc` / cglm (Y-up world). Example 26 should use `SituationCameraBuildViewProj` per [drawing_3d.md](../guide/drawing_3d.md).

### A.3 Critical path: STB quads → SituationMesh

STB output is **quad lists** (`stbvox_get_quad_count`) — **no index buffer**. `SituationCreateMesh` expects **indexed triangles** in a standard layout. This conversion is the highest-risk Phase A task.

| STB mode | Quad vertex size | Notes |
|----------|------------------|-------|
| 0 / 20 | 32 B (attr_vertex + attr_face interleaved) | Easiest to decode for first converter |
| 1 / 21 | 20 B vertex + face data in texture buffer | Second; needs face-buffer decode |

**Converter output target (Phase A):** `SIT_MESH_LAYOUT_POS_NRM_TEX` (32 B) with procedural UVs — **not** a permanent 20 B STB quad layout in the library. Phase C native mesher defaults to **`SIT_MESH_LAYOUT_POS_NRM_TAN_TEX`** (48 B) — see [Locked design decisions](#locked-design-decisions-review-2026-06-29).

**Triangulation & winding:** canonical rules live in **`sit/stb_voxel_to_sit_mesh.h`** (single source of truth):

```text
STB quad vertex order (per face): 0—1
                                    | |
                                    3—2
Triangle A: (0, 1, 2)
Triangle B: (0, 2, 3)
Winding: CCW when viewed from outside the solid (outward normal points at camera for front faces).
Axis: STB X+ = east, Y+ = north, Z+ = up — document mapping to SituationCameraDesc in bridge README.
```

All harness tests and C+.1.2 hash comparisons **must** use `SitStbVoxQuadsToSitMesh()` — no duplicated triangulation logic.

### A.4 Tasks

#### A.4.1 Bridge & mesher (CPU only)

- [ ] **A.2.1** Add `sit/stb_voxel_render_bridge.c` — single TU, `#define STB_VOXEL_RENDER_IMPLEMENTATION`; **never** paste STB into `situation_impl*.h` (same discipline as `ext/glad.c`).
- [ ] **A.2.2** Add `sit/stb_voxel_bridge.h` — public bridge API: init mesh maker, fill `stbvox_input_description`, run `stbvox_make_mesh`, expose quad count + raw buffers (opaque to examples).
- [ ] **A.2.3** Harness unit test **`stb_chunk_quad_count`** — solid 4×4×4 core → `stbvox_get_quad_count` &gt; 0 (no GPU).

#### A.4.2 Quad → SituationMesh converter (reusable utility)

- [ ] **A.2.4** Add **`sit/stb_voxel_to_sit_mesh.h`** + **`sit/stb_voxel_to_sit_mesh.c`** — public to bridge/harness/example (not DLL export):
  ```c
  SituationError SitStbVoxQuadsToSitMesh(
      const stbvox_mesh_maker* mm, int mesh_slot,
      SituationMeshVertexLayout out_layout,  /* Phase A: POS_NRM_TEX */
      SituationMesh* out);
  ```
  Decode Mode **20** first; Mode **1** face-buffer path in A.2.4b if needed.
- [ ] **A.2.4b** (optional Phase A) Mode **1/21** face texture-buffer decode path in same TU.
- [ ] **A.2.5** Harness test **`stb_chunk_sit_mesh_nonempty`** — calls **`SitStbVoxQuadsToSitMesh`** only; yields `vertex_count > 0`, `index_count > 0`, valid handle.
- [ ] **A.2.6** Triangulation + winding + axis mapping documented in **`stb_voxel_to_sit_mesh.h`** header comment (see A.3); bridge README links to it.

#### A.4.3 Shaders & draw (GL + VK)

- [ ] **A.2.7** **Untextured first:** draw converted mesh with a **minimal Situation lit shader** (same pattern as example 20) — proves pipeline before STB GLSL complexity.
- [ ] **A.2.8** Extract STB GLSL (`stbvox_get_vertex_shader`, `stbvox_get_fragment_shader`) → `sit/gpu/voxel_stb.vert` / `voxel_stb.frag`; SPIR-V embed via existing scripts; **`LoadShaderFromMemory` / SPIR-V on both backends**.
- [ ] **A.2.9** Map STB uniforms (`stbvox_get_uniform_info`) → `SituationSetShaderUniform` / UBO; document enum → name table in `stb_voxel_bridge.h`.
- [ ] **A.2.10** Texture path: STB expects **2D array** samplers. Use `SituationCreateTexture` + **`SituationCmdBindTextureSet`**; start with procedural 8×8 debug atlas in `tests/harness/assets/` or Mode 20 until array path validated.

#### A.4.4 Example & harness

- [ ] **A.2.11** Create **`examples/26_voxel_demo/`** — flat/cube world (one 32³ chunk), orbit camera (`SituationCameraDesc`), `sit_example.h` scaffold; README: *“Phase A — STB bootstrap; APIs will change.”* Link [drawing_3d.md](../guide/drawing_3d.md). Register short name **`load_voxel_demo`** in **`build/build_examples.bat`** (same pattern as example 20).
- [ ] **A.2.12** Add **`tests/harness/test_voxel.c`** module:
  - [ ] `stb_chunk_quad_count` (CPU)
  - [ ] `stb_chunk_sit_mesh_nonempty` (CPU)
  - [ ] `stb_chunk_render_center_pixel` — draw + **`SituationRequestScreenCapture()`** before **`SituationEndFrame()`** → **`SituationLoadImageFromScreen()`** → center pixel ≠ clear color (use `graphics_test_screen_any_non_black`; **GL + VK**)
- [ ] **A.2.13** Wire **`build/build_examples.bat`** + harness Makefile for example 26 and `voxel` module; link bridge TU **only** in example/harness (or `SITUATION_ENABLE_STB_VOXEL=1`).

### A.5 STB geometry available in Phase A (baseline)

| STB `STBVOX_GEOM_*` | Meaning |
|---------------------|---------|
| `solid`, `transp`, `force` | Cube faces |
| `slab_upper`, `slab_lower` | Half blocks |
| `floor_slope_*`, `ceil_slope_*` | Cardinal diagonal slopes |
| `floor_vheight_03/12`, `ceil_vheight_*` | Variable corner heights (terrain) |
| `crossed_pair` | Corner pairs |
| `*_as_wall_UNIMPLEMENTED` | **Not in STB** — Phase B native target |

### A.6 Phase A exit criteria (all required)

- [ ] Converter tests pass (CPU): quad count + SituationMesh nonempty.
- [ ] **`26_voxel_demo`** runs **static-opengl** and **static-vulkan** — blocks visible, no cracks at chunk interior.
- [ ] Harness **`--module voxel`** green **GL + VK**.
- [ ] Full harness regression: no failures in unrelated modules.
- [ ] No `SITUATION_IMPLEMENTATION` in example/harness.
- [ ] STB bridge **not** linked into default DLL (`SITUATION_ENABLE_STB_VOXEL` off).
- [ ] Example README states Phase A / STB temporary.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_situation.bat" static-vulkan
& ".\build\build_examples.bat" static-opengl 26_voxel_demo
& ".\build\build_examples.bat" static-vulkan 26_voxel_demo
& ".\build\build_tests.bat" static-opengl
& ".\build\build_tests.bat" static-vulkan
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\run_tests.bat" opengl --module voxel
& ".\run_tests.bat" vulkan --module voxel
```

**Phase A merge:** patch bump + append **`doc/updatelog_24_05.md`** (“experimental STB voxel example 26”).

---

## Phase B — Block spec + adapter + wall slant (prototype)

**Objective:** Freeze a **Situation-owned block specification** (full slant enum on paper) with **STB adapter** for mappable cells, plus **one native mesher implementation: one `SIT_SLANT_WALL_*` cardinal** (e.g. `_NORTH`). Remaining slant kinds are **specified but implemented in Phase C**.

### B.1 Scope

| In (Phase B ships) | Out (Phase C) |
|--------------------|---------------|
| `.kiro/specs/situation-voxel-slants/` (requirements, design, tasks) | Full native mesher for all slant kinds |
| `sit/sit_voxel_spec.h` (draft types) | Public `SituationCreateVoxel*` API |
| `sit_stbvox_adapter.c` — STB roundtrip for mappable cells | Streaming / `SitVoxelWorld` |
| Native CPU mesher stub: **`SIT_SLANT_WALL_*`** (cardinal) only | `SIT_SLANT_FACE`, `SIT_SLANT_CORNER` meshing |
| Example 26: STB cubes + one wall-slant column side-by-side | Retire STB |

### B.2 Motivation (gaps STB leaves)

- Wall-aligned slopes (`*_as_wall_UNIMPLEMENTED`)
- Neighbor-aware culling when slant face normals ≠ axis-aligned blocks
- Extensible slant enum not tied to `STBVOX_GEOM_*`

*(Face tilt 0–90° and corner wedges are **specified** in B, **implemented** in C.)*

### B.3 Slant enum (draft — refine in `.kiro/spec`)

**Naming rule:** use **cardinal direction** for walls and 45° ramps (neighbor culling table reads naturally). Keep **`SIT_SLANT_VHEIGHT4`** as one kind with SW/SE/NW/NE corner payload (do not explode into 256 enum values).

| `SitSlantKind` | Intent | STB mapping | Phase |
|----------------|--------|-------------|-------|
| `SIT_SLANT_NONE` | Full block | `STBVOX_GEOM_solid` | A (STB) |
| `SIT_SLANT_HALF_LOWER` | Bottom half slab | `slab_lower` | A (STB) |
| `SIT_SLANT_HALF_UPPER` | Top half slab | `slab_upper` | A (STB) |
| `SIT_SLANT_RAMP_FLOOR_NORTH` | 45° floor ramp, high side north | `floor_slope_*` + rot | A (STB) |
| `SIT_SLANT_RAMP_FLOOR_SOUTH` | 45° floor ramp, high side south | `floor_slope_*` + rot | A (STB) |
| `SIT_SLANT_RAMP_FLOOR_EAST` | 45° floor ramp, high side east | `floor_slope_*` + rot | A (STB) |
| `SIT_SLANT_RAMP_FLOOR_WEST` | 45° floor ramp, high side west | `floor_slope_*` + rot | A (STB) |
| `SIT_SLANT_RAMP_CEIL_NORTH` … `_WEST` | 45° ceiling ramps (four cardinals) | `ceil_slope_*` + rot | A (STB) |
| `SIT_SLANT_VHEIGHT4` | Corner heights SW,SE,NW,NE in cell payload | `floor_vheight_*` / `ceil_vheight_*` | A (STB) / C native |
| `SIT_SLANT_WALL_NORTH` | Vertical ramp on north face | **Native only** | **B implements** (pick one cardinal first) |
| `SIT_SLANT_WALL_EAST` | Vertical ramp on east face | **Native only** | B → C |
| `SIT_SLANT_WALL_SOUTH` | Vertical ramp on south face | **Native only** | C |
| `SIT_SLANT_WALL_WEST` | Vertical ramp on west face | **Native only** | C |
| `SIT_SLANT_FACE` | Partial face tilt (quantized) | **Native only** | C |
| `SIT_SLANT_CORNER` | Interior/exterior wedge | **Native only** | C |

**Phase B implements one wall cardinal** (e.g. `SIT_SLANT_WALL_NORTH`); spec defines all four for culling tables.

**Neighbor rules:** slant faces use opacity + slant-compatible adjacency table (design.md). Lighting: STB-style per-vertex AO initially; Situation-native lighting in Phase C.

### B.4 Tasks

#### B.4.1 Specification

- [ ] **B.2.1** Create **`.kiro/specs/situation-voxel-slants/requirements.md`** — glossary: `SitVoxelCell`, `SitBlockSpec`, `SitSlantKind`, chunk, palette, face mask.
- [ ] **B.2.2** Create **`design.md`** — byte encoding per cell, neighbor table, STB mapping table, `SIT_VOXEL_NEEDS_NATIVE` flag rules.
- [ ] **B.2.3** Create **`tasks.md`** — checklist synced with this plan.

#### B.4.2 Headers & adapter

- [ ] **B.2.4** Add **`sit/sit_voxel_spec.h`** (header-only, no mesher):
  - `SitVoxelCell`, `SitSlantKind`, `SitVoxelChunkDesc`, `SitVoxelPalette`
- [ ] **B.2.5** Add **`sit/sit_stbvox_adapter.c`**: `SitVoxelChunkDesc` → `stbvox_input_description` when mappable; set `SIT_VOXEL_NEEDS_NATIVE` for wall/face/corner slants.
- [ ] **B.2.6** Add **`sit/sit_voxel_slant_wall.c`** (minimal): emit **`SIT_SLANT_WALL_NORTH`** (or chosen cardinal) tris → `SituationMesh` bucket separate from STB mesh.

#### B.4.3 Tests & example

- [ ] **B.2.7** Harness **`voxel_spec_stb_roundtrip`** — solid cube field → adapter → STB mesh; assert quad/mesh hash (vertex count + bounds) stable.
- [ ] **B.2.8** Harness **`voxel_slant_wall_visible`** — single wall-slant cell → native mesh nonempty; pixel probe on slant face (**GL + VK**).
- [ ] **B.2.9** Update **`26_voxel_demo`**: render STB cube region + wall-slant prototype column; README notes Phase B.
- [ ] **B.2.10** Add spec overview paragraph to **`doc/guide/drawing_3d.md`** (link to this plan; full **`doc/guide/voxel.md`** waits for Phase C).

### B.5 Phase B exit criteria

- [ ] `.kiro/specs/situation-voxel-slants/` reviewed (requirements + design + tasks).
- [ ] Adapter roundtrip test green.
- [ ] Wall slant visible in example 26 (**GL + VK**).
- [ ] Harness `voxel` module green **GL + VK**.
- [ ] No STB fork — adapter + native stub only.

**Phase B merge:** patch bump if `sit_voxel_spec.h` added; spec-only doc commits need no API index regen.

---

## Phase C — Native Situation voxel engine

**Objective:** Replace STB mesher and STB shaders with Situation-native pipeline; implement **remaining `SitSlantKind`** values; ship public API.

### C.1 Architecture target

```text
sit/vox/  (or sit_voxel_*.h slices — P2.2 umbrella when public)
├── sit_voxel_api.h          ← public SITAPI
├── sit_voxel_spec.h         ← Phase B spec (frozen)
├── sit_voxel_mesher.h       ← CPU mesher (naive → optional greedy)
├── sit_voxel_chunk.h        ← chunk cache, dirty flags, mesh slots
├── sit_voxel_palette.h      ← block registry (generational handles)
└── sit/gpu/
    ├── voxel.vert / voxel.frag
    └── voxel_slant.glslh
```

**Rendering path:**

```text
SituationVoxelChunk (handle)
  → worker-thread remesh (optional) → CPU vertex/index (48 B POS_NRM_TAN_TEX)
  → write into mesh_building; on complete swap mesh_building ↔ mesh_active
  → SituationCreateMeshEx(..., SIT_MESH_LAYOUT_POS_NRM_TAN_TEX)
  → SituationCmdBindPipeline(voxel_shader)
  → SituationCmdDrawMesh(mesh_active)
  → graveyard previous mesh_active on swap (fence-safe)
```

**Double-buffer (required in Phase C):** each chunk holds **`SituationMesh mesh_active`** + **`mesh_building`**. `SituationVoxelRemesh` fills `mesh_building` off the render thread; frame draw uses only `mesh_active`. C+ GPU path writes into the same `mesh_building` slot before swap.

**Shader UBO (align with library contract):**

| Binding | Constant | Content |
|---------|----------|---------|
| 0 | `SIT_UBO_BINDING_FRAME_DATA` | time, resolution (if needed) |
| 1 | `SIT_UBO_BINDING_VIEW_DATA` | `ViewDataUBO` { view, projection } |

Vulkan: `layout(std140, set = 0, binding = 1) uniform ViewData { ... }`.  
OpenGL: `layout(std140, binding = 1) uniform ViewData { ... }`.  
See [drawing_3d.md](../guide/drawing_3d.md) Tier 3 binding table.

### C.2 Design spike (C.0 — resolve before C.1 coding)

- [ ] **C.0.1** Vertex layout: **locked default `SIT_MESH_LAYOUT_POS_NRM_TAN_TEX` (48 B)** — pos + normal + tangent + uv; tangents `(1,0,0,1)` on axis-aligned faces until normal maps. Optional compact 32/24 B layout = **optimization flag later**, not v1.
- [ ] **C.0.2** Texturing: bindless `global_textures[]` **or** 2D array texture — pick one (start atlas-friendly).
- [ ] **C.0.3** Chunk default **32³**; document max extent per mesh (STB precision heritage).
- [ ] **C.0.4** Draw strategy: one draw per chunk vs per material bucket (STB can multiply texture binds).
- [ ] **C.0.5** Threading: remesh on worker pool; main thread records `SituationCmd*` only ([GAME_LOOP_PERFORMANCE_PLAN](GAME_LOOP_PERFORMANCE_PLAN.md)).
- [ ] **C.0.6** Double-buffer policy: `mesh_active` / `mesh_building` swap + graveyard documented in `sit_voxel_chunk.h`.

| Topic | Choice |
|-------|--------|
| Vertex format | **`SIT_MESH_LAYOUT_POS_NRM_TAN_TEX` (48 B)** — locked |
| Texturing | _TBD at C.0.2_ |
| Chunk size | 32³ default |
| Mesh swap | **Double-buffer from day one** |
| Streaming | `SitVoxelWorld` — Phase C core or C+ stretch |
| GPU meshing | **C+ optional** — compute + `CmdDrawIndexedIndirect` ([AAA §3](AAA_ARCHITECTURE_PLAN.md)) |

### C.3 Tasks

#### C.3.1 Mesher & shaders

- [ ] **C.1.1** CPU mesher: axis blocks + **all** `SitSlantKind` — **no STB calls**; output **`SIT_MESH_LAYOUT_POS_NRM_TAN_TEX`**.
- [ ] **C.1.2** Shaders: rewrite from scratch; `sit/gpu/` conventions; SPIR-V embed; GL parity.
- [ ] **C.1.3** Implement slant kinds deferred from B: `SIT_SLANT_FACE`, `SIT_SLANT_CORNER`, plus any vheight/slab paths not covered by STB adapter.
- [ ] **C.1.4** **Geometric normals (required):** per-triangle `normalize(cross(e1,e2))` from emitted vertex positions — **do not** copy STB `normal_table[32]` into native shaders. Non-planar vheight tops: split `_03` / `_12` diagonals; one normal per triangle.
- [ ] **C.1.5** Optional: `scripts/gen_vheight_normal_table.py` — generates lookup for **STB adapter parity tests only** (not native draw path); documents delta vs geometric ground truth.

#### C.3.2 Public API (draft — prefer `SituationCmd*` for draw)

- [ ] **C.2.1** `SituationCreateVoxelPalette` / `SituationDestroyVoxelPalette`
- [ ] **C.2.2** `SituationCreateVoxelChunk` / `SituationDestroyVoxelChunk`
- [ ] **C.2.3** `SituationSetVoxelCell` (or bulk fill helper)
- [ ] **C.2.4** `SituationVoxelRemesh` (CPU; async on worker) → fills **`mesh_building`**; swap to **`mesh_active`** when complete
- [ ] **C.2.4b** **`sit_voxel_chunk.h`**: `mesh_active`, `mesh_building`, swap + graveyard on remesh (required — not deferred to C+)
- [ ] **C.2.5** `SituationCmdDrawVoxelChunk(cmd, chunk)` — records draw(s) into command buffer
- [ ] **C.2.6** Include from `situation_api_graphics.h` or umbrella; trace IDs; `generate_*_bindings.py` regen.

#### C.3.3 Integration, tests, docs

- [ ] **C.3.1** Migrate **`26_voxel_demo`** off STB; gate legacy behind **`SITUATION_ENABLE_STB_VOXEL_LEGACY`**.
- [ ] **C.3.2** Harness: native mesh hash tests replace STB equivalents; **≥ 6 slant kinds** pixel/regression tests (**GL + VK**).
- [ ] **C.3.3** Siamese colocation: GL record + execute adjacent to VK in draw path ([RENDERER_SIAMESE_COLOCATION_PLAN](RENDERER_SIAMESE_COLOCATION_PLAN.md)).
- [ ] **C.3.4** Docs: **`doc/guide/voxel.md`**, `whatsnew.md`, **`doc/updatelog_24_05.md`**, steering pointer; cross-link [drawing_3d.md](../guide/drawing_3d.md).
- [ ] **C.3.5** Stretch: VD offscreen mini-map chunk ([virtual_display.md](../guide/virtual_display.md)).
- [ ] **C.3.6** Performance smoke (manual appendix, not CI): 16× 32³ chunks @ 60 Hz VSync, GTX 1070 class — log results in plan changelog.

### C.4 Phase C exit criteria

- [ ] **`26_voxel_demo`** runs with **`SITUATION_ENABLE_STB_VOXEL_LEGACY=0`** (**GL + VK**).
- [ ] Harness `voxel` module green **GL + VK**; slant tests ≥ 6 kinds.
- [ ] **Vheight/slope lighting:** harness or visual check that terrain normals match geometric expectation (not STB quantized `normal_table` banding).
- [ ] **Double-buffer remesh:** edit chunk during draw — no stall; `mesh_active` stable for duration of frame.
- [ ] Public API in DLL + bindings regen.
- [ ] No default mesh layout ABI break.

**Phase C merge:** minor **v2.5** candidate per [`LIBRARY_BUGFIX_PLAN.md`](LIBRARY_BUGFIX_PLAN.md) — coordinate before shipping `SituationCreateVoxel*`.

---

## Phase C+ — Compute remesh & GPU-driven chunks (optional)

**Objective:** Accelerate chunk rebuild and streaming using **`SituationCmdDispatch`** — **after** Phase C CPU mesher is the correctness reference. Does **not** block Phases A–D.

**Prerequisite:** Phase C CPU mesher + harness hash tests green (**GL + VK**).

### C+.1 Why compute (not STB wrap)

| Approach | Verdict |
|----------|---------|
| GPU-wrap STB quad output | ❌ Throwaway — do not invest |
| Compute on **`SitVoxelCell` grid** | ✅ Same rules as CPU mesher; validate against C.1 reference |

Situation already ships dispatch, barriers, SSBO bindings, and indirect draw ([`renderer_bolster_plan.md`](renderer_bolster_plan.md)); AAA §3 targets compute cull → indirect draw ([`AAA_ARCHITECTURE_PLAN.md`](AAA_ARCHITECTURE_PLAN.md)).

**Reuse existing harness patterns for barriers / SSBO wiring** (not the kernel logic):

- **`mesh_bda_compute_read`** ([`test_graphics.c`](../../tests/harness/test_graphics.c)) — dispatch + SSBO + barrier + readback validation
- **Vertex-pull / BDA** ([`test_vertex_pull_render.c`](../../tests/harness/test_vertex_pull_render.c)) — buffer binding discipline

C+ mesh append is a **new** `voxel_mesher.comp` kernel; follow the same **`SituationCmdPipelineBarrierEx` / `SituationCmdBufferBarrier`** sequencing as those tests.

### C+.2 Architecture

```text
SitVoxelChunk grid (+ 1-cell halo in SSBO)
  → SituationCmdDispatch (face emit kernel)
  → vertex/index SSBO (atomic append OR prefix-sum sized)
  → SituationCmdPipelineBarrierEx / SituationCmdBufferBarrier
  → SituationBuffer (device-local) OR mapped readback → SituationCreateMeshEx
  → SituationCmdDrawMesh / SituationCmdDrawIndexedIndirect (multi-chunk)
  → write GPU result into chunk mesh_building; swap with mesh_active; graveyard old mesh
```

### C+.3 Tasks

#### C+.3.1 Reference & validation

- [ ] **C+.1.1** Document CPU vs GPU mesher contract in `.kiro/specs/situation-voxel-slants/design.md` appendix (same neighbor/slant rules).
- [ ] **C+.1.2** Harness **`voxel_compute_mesh_hash`** — 16³ solid chunk: GPU output matches CPU vertex/index count + bounds (**GL + VK**).

#### C+.3.2 Compute pipeline

- [ ] **C+.2.1** Add `sit/gpu/voxel_mesher.comp` — read cell SSBO, write face/vertex SSBO; include 1-voxel halo for chunk seams.
- [ ] **C+.2.2** Variable output sizing: pick **atomic append** (v1) or **prefix sum** (v2); document max verts/chunk.
- [ ] **C+.2.3** Wire `SituationCreateComputePipeline` + `SituationCmdBindComputePipeline` + `SituationCmdDispatch` in remesh path (worker thread submits or main records once per dirty frame).
- [ ] **C+.2.4** Barriers: cell SSBO → compute → vertex SSBO → vertex buffer upload — mirror **`mesh_bda_compute_read`** barrier ordering ([`renderer_bolster_plan.md`](renderer_bolster_plan.md) Phase 3).

#### C+.3.3 Integration & perf

- [ ] **C+.3.1** `SituationVoxelRemesh` fast path: flag **`SIT_VOXEL_REMESH_GPU`** when compute available.
- [ ] **C+.3.2** Optional: multi-chunk **`SituationCmdDrawIndexedIndirect`** ([`AAA_ARCHITECTURE_PLAN.md`](AAA_ARCHITECTURE_PLAN.md) §3 showcase — `examples/gpu_culling.c` pattern).
- [ ] **C+.3.3** Example **`26_voxel_demo`**: toggle CPU vs GPU remesh in README; no default behavior change until hash tests stable.
- [ ] **C+.3.4** Performance log (manual): remesh 32³ dirty chunk CPU vs GPU ms — appendix in this plan.

### C+.4 Phase C+ exit criteria

- [ ] **`voxel_compute_mesh_hash`** green **GL + VK**.
- [ ] Example 26 remesh on edit without render-thread stall (worker + double-buffer).
- [ ] No regression when GPU path disabled (CPU fallback unchanged).

**Phase C+ merge:** patch bump + updatelog note; no new public API required if folded into `SituationVoxelRemesh` flags.

---

## Phase E — Dynamic lighting pass (optional)

**Objective:** Second-pass lighting on top of voxel geometry — from simple additive lights through **G-buffer compositing** toward Quest / NRC / ReSTIR-style techniques. **Not** part of STB bootstrap or Phase C forward shader minimum.

**Prerequisite:** Phase C with **geometric normals (C.1.4)**; Phase E tier 2+ needs **MRT G-buffer** from voxel pass.

**Pairs with:** [`QUEST_RENDERER_SITUATION_API_PLAN.md`](QUEST_RENDERER_SITUATION_API_PLAN.md) (NRC volumes, `SituationCreateTexture3D`, compute SSBO gaps).

### E.1 Lighting tiers (incremental)

| Tier | Technique | Depends on |
|------|-----------|------------|
| **E0** (can ship in C) | Directional + vertex AO in forward voxel shader; `SIT_UBO_BINDING_LIGHTING` point lights | C.1.2 shaders |
| **E1** | Additive second pass: fullscreen / volume with depth + normal RT | C + offscreen targets |
| **E2** | Compute light accumulation (tiled or per-pixel) | E1 G-buffer + dispatch |
| **E3** | Quest crossover: 3D radiance cache / NRC-style sampling | [`QUEST_RENDERER_SITUATION_API_PLAN`](QUEST_RENDERER_SITUATION_API_PLAN.md) Phase 2 API |
| **E4** | ReSTIR-DI / advanced resampling (research-grade) | E2–E3 stable; VK-first |

**Note:** ReSTIR is **not** in Situation today. Treat E3–E4 as Quest/product integration, not voxel-core.

### E.2 Tasks

#### E.1 Forward baseline (may land in Phase C)

- [ ] **E.0.1** `SitVoxelLightingUBO` at `SIT_UBO_BINDING_LIGHTING` — directional + N point lights (std140, documented in `doc/guide/voxel.md`).
- [ ] **E.0.2** Emissive / fullbright block flag in `SitVoxelPalette`.

#### E.2 G-buffer pass

- [ ] **E.1.1** Extend voxel forward pass or add MRT pass: **albedo**, **octahedron/world normal**, **depth** (Situation render targets / VD-compatible).
- [ ] **E.1.2** Harness: G-buffer normal on vheight slope ≠ axis-aligned face normal (proves C.1.4 carries to lighting).
- [ ] **E.1.3** Second composite pass: sample G-buffer + apply simple Lambert or half-Lambert.

#### E.3 Compute / Quest crossover (stretch)

- [ ] **E.2.1** Compute shader: tiled light cull → light list SSBO → accumulation into HDR color RT.
- [ ] **E.2.2** Track Quest API blockers: `SituationCreateTexture3D`, `SituationCmdFillBuffer`, mapped pointer — implement per QSR plan before NRC-style paths.
- [ ] **E.2.3** Example **`examples/27_voxel_lighting/`** (or extend 26): toggle E0 vs E1 vs E2; document keys in README.

#### E.4 ReSTIR / NRC (future)

- [ ] **E.3.1** Spike doc: Situation render-graph / barrier requirements for multi-pass resampling (defer until [`AAA_ARCHITECTURE_PLAN.md`](AAA_ARCHITECTURE_PLAN.md) render graph or manual barrier recipe proven).
- [ ] **E.3.2** Prototype only when Quest stub + Situation Texture3D path green — **not a CI gate**.

### E.3 Phase E exit criteria (minimum for “E1 shipped”)

- [ ] Vheight terrain reads lit correctly in second pass (visual + one harness pixel probe).
- [ ] **GL + VK** for E0 or E1 tier documented in `doc/guide/voxel.md`.
- [ ] No per-frame readback on hot path (capture tests use `SituationRequestScreenCapture` only).

**Phase E merge:** patch or minor depending on new API (`Texture3D` etc.); coordinate with QSR plan version bumps.

---

## Phase D — STB retirement

- [ ] **D.1** Default build excludes `stb_voxel_render_bridge.c` from all targets except optional legacy flag.
- [ ] **D.2** Retain **`ext/stb_voxel_render.h`** for reference; optional stretch: `SituationImportStbVoxelChunk` import tool.
- [ ] **D.3** Example README + **`doc/guide/voxel.md`**: STB path deprecated.
- [ ] **D.4** Remove or `#if 0` harness tests that require STB when legacy flag off.

---

## Regression matrix (every phase merge)

| Gate | Command |
|------|---------|
| Library DLL | `build\build_situation.bat` opengl + vulkan |
| Full harness | `build\run_tests.bat` opengl / vulkan |
| Voxel module | `build\run_tests.bat` opengl --module voxel (and vulkan) |
| Example 26 | `build\build_examples.bat static-opengl 26_voxel_demo` (alias `load_voxel_demo`; static-vulkan too) |
| Bindings | `tools\run_all.bat` (Phase C only) |
| Compute mesh (C+) | `build\run_tests.bat` opengl --module voxel --filter compute_mesh` (when added) |
| G-buffer / lighting (E) | `build\run_tests.bat` opengl --module voxel --filter gbuffer` (when added) |

**No regression rules:** no default mesh layout ABI change; STB bridge off in default DLL; no `SITUATION_IMPLEMENTATION` in tests.

---

## Risk register

| Risk | Mitigation |
|------|------------|
| STB quads ≠ `SituationCreateMesh` | **`SitStbVoxQuadsToSitMesh`** in `stb_voxel_to_sit_mesh.h` — single triangulation source; test before GPU |
| STB GLSL ≠ Situation pipeline | **A.2.8** SPIR-V embed; **VK required in Phase A exit** |
| STB quad format lock-in | Phase A throwaway; Phase C new layout |
| Slant spec explosion | B ships **wall only**; C incremental per kind |
| Mesh rebuild stalls frame | Worker remesh + double-buffer; graveyard old meshes |
| Texture array portability | Mode 20 first; array atlas in A.2.10; bindless decision in C.0 |
| Scope creep | A/B static chunk; streaming = C/C+ |
| Wrong slope lighting in Phase A STB | Expected; do not polish STB `normal_table` — fix in C.1.4 |
| GPU mesher ≠ CPU mesher | C+.1.2 hash gate before defaulting GPU path |
| ReSTIR scope explosion | E tiers; E3+ gated on Quest API + G-buffer; not Phase C |

---

## Scheduling DAG

```text
A.2.1–A.2.6  CPU mesher + converter ──→ A.2.7  minimal draw
       └──────────────────────────────→ A.2.8–A.2.10  STB shaders + atlas
                                          └→ A.2.11–A.2.13  example + harness
                                                    ↓
B.2.1–B.2.3  .kiro spec (can start after A.2.3 validates quads)
B.2.4–B.2.10 adapter + wall slant + tests
                                                    ↓
C.0 spike → C.1 mesher (+ C.1.4 geometric normals) → C.2 API → C.3 example + docs
                                                    ↓
D retire STB
                                                    ↓
        ┌───────────────────────┬───────────────────────┐
        ↓                       ↓                       ↓
C+.1 CPU reference      E.0 forward lights      (optional parallel)
C+.2 compute mesher     E.1 G-buffer + composite
C+.3 indirect draw      E.2+ Quest / ReSTIR spike
```

**Parallel OK:** B.2.1 spec writing after **A.2.3** (quad count proven), while A.2.7–A.2.13 finish GPU path.

---

## Version / changelog policy

| Phase merge | Version doc |
|-------------|-------------|
| **A** | Patch + append **`doc/updatelog_24_05.md`** — experimental example 26 |
| **B** | Patch if `sit_voxel_spec.h` lands; else docs-only |
| **C** | Minor v2.5 candidate + **`doc/guide/voxel.md`** + whatsnew |
| **D** | Patch — deprecation note |
| **C+** | Patch — compute remesh flag / internal path; updatelog note |
| **E** | Patch (E0–E1) or minor (E2+ with new RT/light API); sync with QSR plan |

Active updatelog band: **`updatelog_24_05.md`** (401+). Extend `_06` at 2.4.500 per [`UPDATELOG.md`](../UPDATELOG.md).

---

## Plan document changelog

| Date | Revision |
|------|----------|
| 2026-06-28 | Initial draft — STB bootstrap → slant spec → native engine |
| 2026-06-29 | Review pass — quad converter path, GL+VK Phase A exit, B scoped to wall slant, UBO bindings, task renumbering, checkbox layout |
| 2026-06-29 | Phase **C+** (compute remesh) + Phase **E** (lighting tiers E0–E4); C.1.4 geometric vheight normals; Quest plan cross-link |
| 2026-06-29 | Locked decisions: `stb_voxel_to_sit_mesh.h`, 48 B Phase C layout, cardinal slant names, double-buffer in C, `26_voxel_demo` + `load_voxel_demo` |
