# RGL → Situation Migration Plan

**Source:** `misc/rgl.h` (~10k lines, single-header, `#define RGL_IMPLEMENTATION`)  
**Target:** Full compatibility with `sit/situation_api.h` (OpenGL + Vulkan, command-buffer rendering)  
**Strategy:** Bottom-up — core renderer and resources first, then draw layers, then world systems, then optional/advanced features.

---

## 1. Goals and non-goals

### Goals

- **Zero direct OpenGL calls** in RGL (no `gl*`, no `GLuint`/`GLint` in `RGLState`).
- **Command-buffer rendering** aligned with Situation’s update-before-draw contract.
- **Backend parity:** behavior must be valid on Vulkan, not only OpenGL immediate mode.
- **Preserve the public RGL API** where possible (game code should not need rewrites).

### Non-goals (initially)

- Rewriting RGL into multiple `.c` files (can happen later; plan assumes incremental edits to `rgl.h`).
- Replacing RGL’s CPU-side path/level logic (Phases 5–6 are mostly already CPU-only).
- Feature parity for every debug/test-pattern path on day one.

---

## 2. Current state (audit summary)

| Area | Situation today | RGL today (2026-05-24) | Gap |
|------|-----------------|------------------------|-----|
| Shaders | `SituationLoadShaderFromMemory` → `SituationShader` handle | Main + shadow shaders loaded/unloaded via Situation API | Debug `wireframe_shader` still `GLuint` in `RGLState`; shadow path stubbed at draw time |
| Index bind | `SituationCmdBindIndexBuffer` (v2.4.124) | RGL batch uses `Draw` only today; mesh path uses `DrawMesh` | Wire explicit IBO bind when RGL draws indexed meshes manually |
| Uniforms | `SituationSetShaderUniform*`, `SituationBindUniformBlock` | Flush sets uniforms by name; UBOs via `SituationUpdateBuffer` + `SituationCmdBindDescriptorSet` | No `loc_*` on main path |
| Dynamic geometry | `SituationCreateBuffer` + `SituationUpdateBuffer` + `SituationCmdDraw` | Batch VBO + `_RGL_FlushBatch` records pipeline/draw | Vertex layout configured per pass |
| Textures | `SituationLoadTexture` / `SituationCreateTexture` | `RGL_LoadTexture` + bitmap font atlases via Situation builders ✅; TTF atlases still `_RGL_UploadTextureFromPixels` | Stamp-text RGL wrappers legacy; see §7A |
| Frame | `SituationAcquireFrameCommandBuffer`, `SituationCmdBeginRenderPass` | `RGL_Begin`/`End` begin/end pass + `SituationCmdSetViewport` | Host must acquire CB (see §8) |
| Render targets | `SituationCreateVirtualDisplay` + render passes | VD fields exist; `RGL_DrawPathAsMap` stubbed | Finish VD wiring |
| Stencil shadows | `SituationRenderPassInfo.stencil_attachment` only | `RGL_CastStencilShadowFromMesh` / wireframe debug **stub** (`RGL_STUB_LEGACY_GL_`) | **No `SituationCmdSetStencil*` API** |
| Math / color utils | Pure CPU | Pure CPU | **No migration** |
| Dependencies | `situation_api.h`, `ext/tinyobj_loader_c.h`, `ext/par_shapes.h` | `misc/dynamo.h` stub; `stb_truetype` guarded (`#ifndef STB_TRUETYPE_H`) | `tinyobj_parse_obj` uses memory `file_reader` callback |

Approximate **~138** active `gl*` tokens remain outside `#if 0` / stub-only paths; **~499** total matches in `misc/rgl.h` (legacy blocks, commented stencil, path-map code). **CI grep (Phase 0.2) not enabled yet.**

### 2.1 Compile / API hygiene (in progress)

Public headers use **`Vector2` / `Vector3`** (from `situation_api.h`); many implementations still used **`vec2` / `vec3`** (cglm), causing conflicting prototypes under `-Werror`. Ongoing fix:

- Align impl signatures with public API (`RGL_DrawText`, `RGL_DrawGrid`, `RGL_DrawBillboard`, lights, etc.).
- Add `_RGL_V3` / `_RGL_V2` helpers for internal cglm → `Vector2`/`Vector3` calls.
- Resolve **`_RGL_Draw3DQuad`** forward-decl vs implementation signature mismatch (`up_vec` parameter).
- **`build_rgl_smoke.bat`**: added; requires `-Isit` for `situation_api.h`.

---

## 3. Layer model (build order)

RGL is organized in dependency layers. **Lower layers must be Situation-clean before upper layers can be validated.**

```
Layer 0 — Pure CPU (no GPU)
  Math, color, YPQ helpers, path/level queries

Layer 1 — Core renderer substrate
  RGL_Init / Shutdown, RGLState, shaders, buffers, _RGL_FlushBatch

Layer 2 — Frame contract
  RGL_Begin / End, camera matrices, viewport, render targets

Layer 3 — Resource management
  Textures, meshes (load/gen/destroy), bindless handles (optional)

Layer 4 — Immediate-mode drawing (batched)
  All RGL_Draw* that queue into RGLInternalDraw → _RGL_FlushBatch

Layer 5 — Lighting
  CPU light pool → UBO upload in flush (no glBuffer*)

Layer 6 — World drawing
  Path draw, level draw, scenery, RGL_DrawWorld

Layer 7 — Secondary systems
  Particles, fonts/text, shadows, debug overlays, test patterns
```

---

## 4. Situation API cheat sheet (for RGL)

Use these mappings consistently; avoid reintroducing GL types in `RGLState`.

| RGL need | Situation API |
|----------|----------------|
| Load main/shadow GLSL | `SituationLoadShaderFromMemory(vs, fs, &shader)` |
| Set scalar/vec/mat uniform | `SituationSetShaderUniform`, `SituationSetShaderUniform1fv`, `SituationSetShaderUniformMatrix4fv` |
| Bind LightBlock UBO | `SituationCreateBuffer` + `SituationBindUniformBlock` + `SituationCmdBindDescriptorSet` |
| Dynamic vertex stream | `SituationCreateBuffer(..., SITUATION_BUFFER_USAGE_DYNAMIC_VERTEX)` + `SituationUpdateBuffer` |
| Draw triangles | `SituationCmdBindPipeline`, `SituationCmdBindVertexBuffer`, `SituationCmdBindIndexBuffer`, `SituationCmdBindTextureSet`, `SituationCmdDraw` / `SituationCmdDrawIndexed` |
| Viewport / scissor | `SituationCmdSetViewport`, `SituationCmdSetScissor` |
| Blend / depth | `SituationCmdSetBlendEnable`, `SituationCmdSetDepthTest`, `SituationCmdSetDepthWrite` |
| Begin frame | `SITUATION_BEGIN_FRAME()` then `SituationAcquireFrameCommandBuffer()` |
| Get recorder | `SituationGetMainCommandBuffer()` |
| Begin pass to screen / VD | `SituationCmdBeginRenderPass(cmd, &info)` with `display_id` |
| End frame | `SituationCmdEndRenderPass` → `SituationEndFrame()` |
| Texture from file | `SituationLoadTexture` (already used in `RGL_LoadTexture`) |
| Texture from CPU image | `SituationCreateImage` → `SituationCreateTexture` |
| Off-screen target | `SituationCreateVirtualDisplay` + render pass to `display_id` |
| Static mesh | `SituationCreateMesh` / `SituationDestroyMesh` |

**Contract (critical):** Per frame, **update** buffers and uniforms **before** recording draw commands that consume them (`situation_api.h` usage guide).

---

## 5. Phased migration

Each phase has **actionables**, **exit criteria**, and **suggested validation**. Phases are sequential unless noted.

---

### Phase 0 — Housekeeping (no GPU behavior change)

**Purpose:** Make the codebase ready to migrate without fixing rendering yet.

| ID | Actionable | Owner / notes |
|----|------------|----------------|
| 0.1 | Add `misc/RGL_MIGRATION_PLAN.md` (this file) and link from project README or `doc/` if desired | ✅ |
| 0.2 | Add grep/CI guard: fail if `misc/rgl.h` contains `\bgl[A-Z]` outside a clearly marked `// RGL_GL_LEGACY` block (optional, enable after Phase 1) | ⬜ |
| 0.3 | Document minimal host loop for RGL tests (see §8) | ✅ §8 |
| 0.4 | Confirm `rgl.h` includes only `situation_api.h` / `misc/dynamo.h` — no `#include <GL/gl.h>` | ✅ (`misc/dynamo.h` stub; `miniaudio.h` for light mutex) |

**Exit criteria:** Plan agreed; no functional change required.

**Layers touched:** None (documentation only).

---

### Phase 1 — Core substrate (Layer 1)

**Purpose:** Replace invalid Situation usage and GL-owned GPU objects with Situation buffers and shaders. This is the **highest priority** phase.

#### 1.1 `RGLState` cleanup

| ID | Actionable |
|----|------------|
| 1.1a | Remove `GLuint batch_vao`, `batch_vbo`, `light_ubo`, `default_fbo`, `fullscreen_quad_vao` | ✅ removed from active init (legacy `fullscreen_quad_vao` field may remain unused) |
| 1.1b | Remove `GLint loc_*` — store `SituationShader` only; set uniforms by **name** each flush | ✅ on main batch path |
| 1.1c | Add `SituationBuffer batch_vertex_buffer`, `light_ubo_buffer`, `view_ubo_buffer` | ✅ |
| 1.1d | Fetch command buffer via `_RGL_GetCmd()` → `SituationGetMainCommandBuffer()` | ✅ |
| 1.1e | Add `bool render_pass_active` / batch layout reset on `RGL_Begin` | ✅ |

#### 1.2 `RGL_Init` / `RGL_Shutdown`

| ID | Actionable |
|----|------------|
| 1.2a | `SituationLoadShaderFromMemory` for main shader — check `SituationError` / `_RGL_ShaderValid` | ✅ |
| 1.2b | Shadow shaders via `SituationLoadShaderFromMemory`; unload in shutdown | ✅ load optional (warn on fail) |
| 1.2c | `SituationBindUniformBlock` for `"LightBlock"` (0) and `"ViewData"` (1) | ✅ |
| 1.2d | `SituationCreateBuffer` for light/view UBOs + batch vertex buffer | ✅ |
| 1.2e | No `glGen*` / `glDelete*` in init/shutdown on main path | ✅ debug wireframe GL VAO removed |

#### 1.3 `_RGL_FlushBatch` (critical path)

| ID | Actionable |
|----|------------|
| 1.3a | **Update:** `SituationUpdateBuffer(batch_vertex_buffer, ...)` | ✅ |
| 1.3b | **Update:** light + view UBO via `SituationUpdateBuffer` | ✅ |
| 1.3c | **Update:** `SituationSetShaderUniform*` + view matrix upload | ✅ |
| 1.3d | **Record:** `SituationCmdBindPipeline`, descriptor sets, raster state | ✅ |
| 1.3e | **Record:** `SituationCmdBindTextureSet` + `SituationCmdDraw` | ✅ |
| 1.3f | No `glUseProgram` / `glDrawArrays` on flush path | ✅ |
| 1.3g | `SituationCmdBindVertexBuffer` + `SituationCmdSetVertexAttribute` (13-float layout) | ✅; public `SituationCmdBindVertexBuffer` added in `situation_api.h` |

**Exit criteria:**

- `rgrep '\bgl[A-Z]' misc/rgl.h` shows **no hits** in `_RGL_FlushBatch`, `RGL_Init`, `RGL_Shutdown`.
- Minimal test: `RGL_Init` → `RGL_Begin(-1)` → `RGL_DrawRectangle` → `RGL_End` does not crash (may need Phase 2 for full frame).

**Depends on:** Host app calls `SituationInit` before `RGL_Init`.

---

### Phase 2 — Frame contract (Layer 2)

**Purpose:** Integrate RGL with Situation’s per-frame command buffer and render passes.

| ID | Actionable |
|----|------------|
| 2.1 | **Host acquires** frame CB; RGL only records (see §8) | ✅ documented + smoke test |
| 2.2 | `RGL_Begin`: `SituationCmdBeginRenderPass` + clear color/depth | ✅ |
| 2.3 | `SituationCmdSetViewport` from `RGL.viewport` | ✅ |
| 2.4 | `RGL_End`: flush + `SituationCmdEndRenderPass` | ✅ |
| 2.5 | `RGL_SetRenderTarget` / `RGL_ResetRenderTarget` | 🟡 needs validation |
| 2.6 | `RGL_CreateRenderTexture` / VD texture handle | ⬜ |

**Exit criteria:**

- Sample loop in §8 renders a solid rect + textured sprite to main window on **OpenGL and Vulkan** backends.
- No `glViewport` / `glBindFramebuffer` in `RGL_Begin`/`End`/target functions.

---

### Phase 3 — Resources (Layer 3)

**Purpose:** All GPU images and meshes go through Situation.

| ID | Actionable |
|----|------------|
| 3.1 | `RGL_LoadTexture` / `RGL_UnloadTexture` — already mostly correct; add error logging via `SituationGetLastErrorMsg` |
| 3.2 | `RGL_GetTextureRect` — use `SituationGetTextureInfo` or cached `SituationTexture.width/height` |
| 3.3 | `RGL_LoadMeshFromFile` / `RGL_GenMesh*` — ensure GPU path uses `SituationCreateMesh` only (audit for `gl*` in mesh upload) |
| 3.4 | `RGL_DestroyMesh` — `SituationDestroyMesh(&mesh.gpu_mesh)` |
| 3.5 | Font/bitmap atlases — bitmap: `SituationCreate*` builders ✅; TTF: still `_RGL_UploadTextureFromPixels` in `RGL_LoadTrueTypeFont` (see §7A.4) | 🟡 |
| 3.6 | `RGL_LoadMeshFromFile`: use `tinyobj_loader_c.h` with `_RGL_ObjFileReader` memory callback (not legacy 8-arg `tinyobj_parse_obj`) | ✅ |
| 3.7 | Include path: `ext/tinyobj_loader_c.h` (not `tiny_obj_loader_c.h`) | ✅ |

**Exit criteria:** Resource create/destroy functions contain no `gl*` calls.

---

### Phase 4 — Batched 2D/3D primitives (Layer 4)

**Purpose:** All draw entry points only **enqueue** `RGLInternalDraw`; flush uses Phase 1 path only.

| ID | Actionable |
|----|------------|
| 4.1 | Audit every `RGL_Draw*` — must not call GL directly (grep per function) |
| 4.2 | Fix `_RGL_Draw3DQuad`, `_RGL_DrawLineQuad`, `_RGL_DrawCubeFaces` — queue only |
| 4.3 | `RGL_DrawMesh` / indexed 3D — if any immediate `glDrawElements`, route through `SituationCmdDrawIndexed` + owned mesh buffer |
| 4.4 | Implement texture bind in flush (complete TODO at ~line 1684) |
| 4.5 | Apply `RGL.transform` in shader (UBO or push constants) — add `ViewData` / model matrix upload in flush |

**Exit criteria:**

- Draw 2D shapes, `RGL_DrawSprite`, `RGL_DrawBillboard`, `RGL_DrawCube` in test scene.
- Batch stats (`RGL.stats`) still increment correctly.

---

### Phase 5 — Lighting (Layer 5)

**Purpose:** Light management stays CPU-side; GPU upload only via Situation buffers.

| ID | Actionable |
|----|------------|
| 5.1 | Keep `RGLLight` pool and mutex on CPU (no change) |
| 5.2 | Frustum cull + sort in `_RGL_FlushBatch` (already present) — upload via `SituationUpdateBuffer` only |
| 5.3 | Verify main shader `LightBlock` layout matches packed `light_block_data` on **Vulkan** (may need `SituationValidateShaderUniforms` or SPIR-V layout profile) |
| 5.4 | `RGL_SetAmbientLight` — CPU only until flush |

**Exit criteria:** Multiple point lights visible on mesh/quads; no GL in light path.

---

### Phase 6 — World systems (Layer 6)

**Purpose:** Path, level, scenery drawing — all ultimately call Layer 4 queue functions.

| ID | Actionable |
|----|------------|
| 6.1 | **Queries** (`RGL_GetPathPropertiesAt`, `RGL_GetGroundAt`, junctions, markers) — **no GPU**; mark ✅ complete |
| 6.2 | `RGL_DrawPath` / `_RGL_DrawPathScene_Road` — remove `_RGL_DrawMapPolygon` GL path; use batched quads |
| 6.3 | `RGL_DrawLevel` — walls/flats/things via `_RGL_Draw3DQuad` / billboards only |
| 6.4 | `RGL_DrawWorld` — integration test with one path + one level |
| 6.5 | `RGL_DrawPathAsMap` — render to VD via Phase 2 render target |

**Exit criteria:** Drive-style scene (road + walls + sprites) renders without `gl*` outside flush.

---

### Phase 7 — Fonts, particles, shadows, debug (Layer 7)

**Purpose:** Port or gate advanced features.

#### 7A — Fonts and text 🟡

**Canonical detail:** [`doc/plan/font_migration_plan.md`](../../plan/font_migration_plan.md) — Situation-first migration, phases F0–F7.

**v2.4.341 summary:** Situation **F0–F4 complete** (builders, measure, boxed/stamp, grid GPU draw, `SituationPackedFont`). Docs: `doc/guide/font.md`. FFI wrappers regen. RGL **bitmap** create/draw/measure/unload/boxed delegate to Situation. **Still legacy in `rgl.h`:** TTF load/draw (`RGL_LoadTrueTypeFont`, `RGL_DrawTextTTF`), stamp-to-texture (`RGL_StampTextToTexture*`), styled GPU text (shadow/outline/gradient/wave), duplicate atlas upload in TTF path.

| ID | Actionable | Status |
|----|------------|--------|
| 7A.0 | Situation core: F0–F4 APIs + OpenGL harness 8/8 | ✅ v2.4.341 |
| 7A.0b | `doc/guide/font.md` + command ref + FFI bindings | ✅ v2.4.341 |
| 7A.1 | Bake atlases via Situation builders (`SituationCreate*` / `BakeBitmapFontAtlas`) | ✅ RGL `Create*` wrappers call Situation |
| 7A.2 | `RGL_StampTextToTexture*` → `SituationImageStamp*` + texture upload | 🟡 Situation APIs shipped; RGL wrappers **legacy** |
| 7A.3 | Delegate `RGL_DrawText*` to `SituationCmdDrawText*` | 🟡 Bitmap `DrawText` / `Ex` / `Boxed` ✅; TTF **legacy sprite path** |
| 7A.4 | `RGL_LoadTrueTypeFont` → `SituationLoadFont` + bake | ⬜ Legacy `stbtt` + `_RGL_UploadTextureFromPixels` in `rgl.h` |
| 7A.5 | Delete duplicate font atlas builders from `rgl.h` (keep thin wrappers) | ⬜ After 7A.4 + 7A.2 |
| 7A.6 | `RGL_MeasureTextTTF` / unload TTF → Situation | 🟡 Bitmap measure/unload ✅ |
| 7A.7 | Vulkan `text_rendering` harness + expanded font tests (boxed, stamp, builders) | ⬜ OpenGL green; Vulkan env issue on some machines |

**Exit criteria (7A):** All `RGL_DrawText*` and font create paths delegate to Situation; no font-section `_RGL_UploadTextureFromPixels`; stamp wrappers use `SituationImageStamp*`; §7A marked ✅ in progress tracker.

#### 7B — Particles

| ID | Actionable |
|----|------------|
| 7B.1 | `RGL_DrawParticles` — billboards via `RGL_DrawBillboard` queue (Dynamo already CPU) |

#### 7C — Shadows

| ID | Actionable |
|----|------------|
| 7C.1 | **Short term:** `RGL_DrawSpriteDownwardShadow` → `RGL_DrawBillboardCylindricalY` on ground | ✅ |
| 7C.2 | **Stencil path:** stub + `#if 0` legacy GL; needs Situation stencil commands or alternate technique | 🟡 stubbed |
| 7C.3 | `RGL_CastStencilShadowFromMesh` — calls stub; blocked on 7C.2 | 🟡 |

#### 7D — Debug / calibration

| ID | Actionable |
|----|------------|
| 7D.1 | `_RGL_InitDebugRendering` — Situation wireframe shader only (GL VAO removed) | 🟡 |
| 7D.2 | `RGL_DrawWireframeBounds` — stub (`RGL_STUB_LEGACY_GL_`) | 🟡 |
| 7D.3 | Test patterns — low priority; can remain CPU queue into batch |

**Exit criteria:** Text + particles work; shadow strategy documented; debug draws without GL.

---

### Phase 8 — Integration and polish

| ID | Actionable |
|----|------------|
| 8.1 | `examples/rgl_smoke_test.c` + `build_rgl_smoke.bat` (`-Isit`, links like `build_examples.bat`) | ✅ added |
| 8.2 | Run smoke test on **OpenGL** and **Vulkan** | 🟡 OpenGL build not green yet (Vector2/vec2 + `_RGL_Draw3DQuad` fixes in flight) |
| 8.3 | Remove dead code: `gl_program_id`, FBO comments, `LTBindTexture` TODOs |
| 8.4 | Optional: split `rgl.h` → `rgl.h` + `rgl.c` for faster iteration |
| 8.5 | Enable CI grep guard from Phase 0.2 |

**Exit criteria:** Full library builds; zero `gl*` in `misc/rgl.h`; smoke test green on both backends.

---

## 6. What stays unchanged (Phase 0 CPU layers)

These modules need **no Situation GPU migration** (verify they contain no `gl*` — today they should not):

- **Math & color** (`RGL_Lerp`, `RGL_ColorFromHSV`, YPQ helpers, palettes)
- **Path management** (`RGL_CreatePath`, `RGL_AddPathPoint`, …)
- **Level editing** (`RGL_AddWall`, `RGL_AddFlat`, …)
- **World queries** (`RGL_QueryJunction`, `RGL_FindMarkersInRange`, coordinate transforms)

Continue to use `SituationConvertColorToVec4` / `SituationColorToYPQ` where already integrated.

---

## 7. Situation API gaps (track in Situation repo)

| Gap | Impact on RGL | Suggested Situation work |
|-----|---------------|---------------------------|
| No `SituationCmdSetStencilFunc/Op/Mask` | Stencil shadow volumes | Add raster/stencil commands + harness tests |
| No public `SituationGetShaderLocation` | RGL used `GLint` caches | Prefer uniform-by-name APIs; or restore thin wrapper |
| Raw `SituationCmdDraw` + dynamic VBO vertex layout | Batch path | Document canonical pattern in graphics harness |
| VD → `SituationTexture` handle | `RGL_CreateRenderTexture` incomplete | Expose texture handle from virtual display API |

---

## 8. Recommended host loop (for tests)

RGL must run inside Situation’s frame contract:

```c
SituationInit(argc, argv, &init_info);
RGL_Init();

while (!SituationWindowShouldClose()) {
    SITUATION_BEGIN_FRAME();

    if (SituationAcquireFrameCommandBuffer()) {
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        RGL_Begin(-1);   /* begins render pass + sets viewport/camera */
        /* ... game update ... */
        RGL_DrawRectangle(...);
        RGL_End();       /* flush batch + end render pass */

        SituationEndFrame();  /* submit + present */
    }
}

RGL_Shutdown();
SituationShutdown();
```

**Open question (decide in Phase 2):** Should `RGL_Begin` call `SituationAcquireFrameCommandBuffer` itself, or require the host to acquire first? **Recommendation:** Host acquires; RGL only records — matches `situation_api.h` guide and keeps a single submit per frame.

---

## 9. Progress tracker (manual)

Copy to PR descriptions or project board:

| Phase | Status | Notes |
|-------|--------|-------|
| 0 Housekeeping | ✅ | Plan; `misc/dynamo.h` stub; §8 host loop; no CI `gl*` guard yet |
| 1 Core substrate | 🟡 | Main path on Situation; exit criteria (zero `gl*` in init/flush/shutdown) met for **active** code |
| 2 Frame contract | 🟡 | `RGL_Begin`/`End` + viewport; host-acquire contract documented |
| 3 Resources | 🟡 | Textures + fonts + OBJ loader callback; mesh gen uses par_shapes (capsule uses `create_hemisphere`) |
| 4 Batched drawing | 🟡 | Queue → flush; align remaining `Vector2`/`Vector3` impl signatures |
| 5 Lighting | 🟡 | UBO + uniforms in flush; `RGL_SetLightPosition` uses `Vector3` |
| 6 World systems | ⬜ | Path map stubbed; `_RGL_Draw3DQuad` decl/impl mismatch blocks full compile |
| 7 Fonts/particles/shadows/debug | 🟡 | 7A Situation F0–F4 ✅ + RGL bitmap delegates ✅; TTF/stamp/legacy cleanup ⬜; 7C/7D stubbed |
| 8 Integration | 🟡 | **OpenGL smoke build green** (`build_rgl_smoke.bat`); run `build\examples\rgl_smoke_test.exe` for red rectangle |

Legend: ⬜ not started · 🟡 in progress · ✅ done

### 9.1 Next actions (recommended order)

1. ~~**Green OpenGL smoke build**~~ ✅ (`build_rgl_smoke.bat` + `-Iexamples` for `font_data.h`).
2. **Run / verify visually:** `build\examples\rgl_smoke_test.exe` — red rectangle on dark gray.
3. Finish `Vector2`/`Vector3` alignment; fix `_RGL_Draw3DQuad` prototype vs body (`up_vec`).
4. **Vulkan** smoke build (mirror `build_examples.bat` vulkan path).
5. Wrap or delete `#if 0` GL blocks; enable Phase **0.2** grep guard on non-legacy regions.
6. Phase **6** path/level draw audit once smoke test passes.

**Harness reference (Situation):** `bind_index_buffer_low_level` + `draw_indexed_quad` in `tests/harness/test_graphics.c` — both pass OpenGL + Vulkan on v2.4.124.

---

## 10. Suggested first PR (minimal vertical slice)

Keep the first merge small but end-to-end:

1. Phase **1.1–1.3** + **2.1–2.4** (largely implemented).
2. Compile hygiene: public `Vector2`/`Vector3` matches implementations (no `vec2`/`vec3` in `SITAPI` functions).
3. One test: `examples/rgl_smoke_test.c` — clear + red `RGL_DrawRectangle` (texture optional in follow-up).
4. Builds on OpenGL; Vulkan in immediate next PR.

Defer path/level/stencil until smoke test is green. Bitmap font atlases use Situation builders (v2.4.341); TTF/stamp cleanup tracked in §7A.

---

## 11. Related files

| File | Role |
|------|------|
| `misc/rgl.h` | Library under migration |
| `misc/dynamo.h` | Minimal `DynamoBody` stub for particles |
| `misc/RGL_MIGRATION_PLAN.md` | This plan |
| `doc/plan/font_migration_plan.md` | Situation font migration (F0–F7); canonical for §7A |
| `doc/guide/font.md` | Situation font module guide |
| `sit/situation_api.h` | Target API (`SituationCmdBindVertexBuffer` / `SituationCmdBindIndexBuffer` — v2.4.124) |
| `ext/tinyobj_loader_c.h` | Wavefront OBJ loader (existing in repo) |
| `examples/rgl_smoke_test.c` | Phase 8 smoke test |
| `build_rgl_smoke.bat` | Build smoke test (OpenGL, `-Isit`) |
| `build_examples.bat` | Reference for flags/link pattern |
| `tests/harness/test_graphics.c` | Reference for cmd buffer, meshes, shaders |

---

*Last updated: 2026-06-23 (v2.4.341 — Situation font F0–F4 complete; RGL bitmap text delegates; §7A expanded)*
