# Externalize GPU Compute Plan

**Date:** 2026-06-10  
**Status:** complete — Phases 0–1C verified (2026-06-12); shipped in v2.4.254; Phase 2 decision deferred  
**Scope:** Extract **Situation core library** internal GPU shaders from `sit/situation_impl_decl.h` into `sit/gpu/`, and make that folder the source of truth for core renderer pipeline compilation.  
**Out of scope:** K-Term (`sit/k-term/`), Polysonix (`sit/aud/polysonix/`), harness shaders (`tests/harness/shaders/`), and examples (`examples/demon_hunt/`). Those modules keep their own shader trees and load paths.  
**Primary files:** `sit/gpu/**`, `sit/situation_impl_decl.h`, `sit/situation_impl_renderer.h`, `scripts/gen_spirv_embed.ps1`  
**Related plans:** `doc/plan/IMPL_DECL_SPLIT_PLAN.md`, `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`, `doc/plan/renderer_bolster_plan.md`  
**Constraint:** preserve OpenGL 4.6 / Vulkan 1.4 dual-backend parity unless explicitly tagged `[GL only]` / `[VK only]`.  
**Compile model:** **one GLSL source per stage**, with `#if defined(SITUATION_USE_VULKAN)` / `SITUATION_USE_OPENGL` branches — same as today's embedded strings. **No separate `gl/` and `vk/` source trees.** Backend selection is a **compile-time** decision (shaderc macros + `glslc --target-env`), not a file-layout decision.

---

## How to use this file

1. Execute phases in order: **0 → 1A → 1B → 1C** is the required path; **2** optional; **3** on ship; **4** optional follow-up (shared `.glslh` contracts — explore much later).
2. Treat `sit/gpu/` as the **only** authoritative location for **core library** internal GLSL after **Phase 1C**.
3. Every moved shader must be covered by existing harness renderer tests on **both** backends (or a documented `[backend-specific]` tag).
4. When a phase ships, update `doc/UPDATELOG.md`, `doc/whatsnew.md`, and `doc/COMPILATION_GUIDE.md`.
5. **Do not touch** K-Term or Polysonix shader paths, preambles, or `.comp` files as part of this plan.

---

## Executive summary

The **core library** keeps all internal renderer shaders as embedded C strings in `sit/situation_impl_decl.h` (~lines 1880–2330). At init, OpenGL compiles them directly; Vulkan runs them through shaderc at runtime (when `SITUATION_ENABLE_SHADER_COMPILER` is enabled).

Other modules already externalize their own shaders independently:

| Module | Location | This plan |
|--------|----------|-----------|
| Core renderer (VD, composite, quad, text, YPQ) | `situation_impl_decl.h` embedded strings | **In scope** |
| K-Term compute | `sit/k-term/shaders/*.comp` + preambles in `kterm_api.h` | **Out of scope** |
| Polysonix compute | `sit/aud/polysonix/px_vm.comp` | **Out of scope** |
| Harness / examples | `tests/harness/shaders/`, `examples/demon_hunt/` | **Out of scope** |

**`sit/gpu/` is the sole core shader source** (Phases 0–1C, 2026-06-10): renderer init loads disk GLSL; embedded strings removed from `situation_impl_decl.h`.

---

## Goal

1. **Extract** embedded core renderer shader strings from `situation_impl_decl.h` into `sit/gpu/`.
2. **Streamline** duplicate vertex shaders while extracting (sanitisation): merge VD + composite into one `compositor.vert`; keep quad + YPQ on `quad.vert`; leave `text.vert` separate.
3. **Preserve** the **CPU–GPU interface** — binding contract, push-constant layouts, descriptor sets, and GL uniform locations that C draw code depends on.
4. **Wire** `_SituationInitOpenGL` / `_SituationVulkanInitInternalRenderers` to load from `sit/gpu/` (runtime GLSL or build-time SPIR-V embed).
5. **Support** two consumption modes for core shaders:
   - **Runtime GLSL** (development)
   - **Build-time SPIR-V embed** (release builds without shaderc)
6. **Solve** the core-library blockers below without changing K-Term, Polysonix, or public user-shader APIs.

---

## CPU–GPU interface (the missing half)

Externalizing shaders is not just moving GLSL text. The core library has a **two-sided contract**: C code in `situation_impl_renderer.h` and `situation_impl_vd.h` packs data and binds resources; the shaders declare matching `layout(...)` interfaces. Today both sides are maintained by convention in `situation_impl_decl.h` via stringified macros and hand-counted byte offsets.

### Shader contract (shared numbering)

Defined in `situation_impl_decl.h` (~1760–1805). These numbers must stay aligned across C vertex setup, pipeline layout creation, and external GLSL:

| Category | C macros | GLSL (Vulkan) | GLSL (OpenGL) |
|----------|----------|---------------|---------------|
| Vertex attributes | `SIT_ATTR_POSITION`, `SIT_ATTR_TEXCOORD_0`, … | `layout(location = N) in` | same |
| Per-view UBO | `SIT_UBO_BINDING_VIEW_DATA` (= 1) | `set = 0, binding = 1` | `binding = 1` uniform block |
| Samplers | `SIT_SAMPLER_BINDING_VD_SOURCE`, `_VD_DEST`, `_ALBEDO`, … | `set = 1/2, binding = N` | `binding = N` or bindless |
| Per-draw data | `SIT_UNIFORM_LOC_*` | **push constants** | `layout(location = N) uniform` |

**Key asymmetry:** Vulkan passes model matrix, opacity, blend mode, and VD idle state via **push constants**; OpenGL passes the same fields as **standalone uniform locations** (`glProgramUniform*` in the draw path). External GLSL preserves this split with **`#if` branches in a single file** per stage — identical to the embedded-string pattern today. The C draw code will not change in early phases.

### Per-pipeline interface map (in scope)

| Pipeline | C data path | Vulkan interface | OpenGL interface | Push size (VK) |
|----------|-------------|------------------|------------------|----------------|
| `vd.frag` + `compositor.vert` | `_SitVDFillPathBPushConstants` in `situation_impl_vd.h` | set 0: view UBO; set 1: source sampler; push: `VDPushConstants` | `u_projection`, `u_model`, `u_opacity`, `u_isIdle`, … via `_SitVDApplyCompositorIdleUniformsGL` | `SIT_VD_PATH_B_PUSH_CONSTANT_SIZE` (96) |
| `composite.frag` + `compositor.vert` | `_SitVDFillPathAPushConstants` | set 0: view UBO; set 1+2: source + dest samplers; push: `CompositePushConstants` (+ `blendMode`) | same uniform-loc pattern + `u_blendMode` | `SIT_VD_PATH_A_PUSH_CONSTANT_SIZE` (112) |
| `quad.frag` + `quad.vert` | anonymous struct in `SituationCmdDrawQuad` / `DrawTexture` | set 0: view UBO; set 1: albedo sampler; push: `QuadPushConstants` | `u_projection`, `u_model`, `u_objectColor`, loc 5 `uv_rect`, loc 6 `use_texture` | 104 bytes (hand-counted) |
| `ypq_grade.frag` + `quad.vert` | anonymous struct in `SituationCmdDrawTextureYpqGrade` | reuses quad VS; push: `YpqGradePushConstants` (+ grade floats) | reuses quad GL uniform pattern | `SIT_YPQ_GRADE_PUSH_BYTES` |
| `text.*` | `{ Vector4 color } text_pc` in text draw | set 0: view UBO; set 1: albedo; push: `TextPushConstants { vec4 color }` | `u_projection`, `u_color`, bindless loc 6/7 | `sizeof(vec4)` |

### Manual packing (high-risk area)

VD/composite push constants are **not** passed as named C structs matching GLSL. `situation_impl_vd.h` uses `memcpy` into byte buffers at fixed offsets:

```c
// Path B (VD): model @ 0, opacity @ 64, is_idle @ 68, fallback_mode @ 72, ...
_SitVDFillPathBPushConstants(uint8_t* out, ...);

// Path A (composite): adds blendMode @ 64, shifts idle fields
_SitVDFillPathAPushConstants(uint8_t* out, ...);
```

These offsets must match `VDPushConstants` / `CompositePushConstants` in the external `.frag` / `.vert` files **exactly**. A shader move that reorders GLSL fields without updating `_SitVDFillPath*PushConstants` will silently corrupt compositing.

### Pipeline layout coupling (Vulkan init)

`_SituationVulkanInitInternalRenderers` (~5404) creates `VkPipelineLayout` objects whose descriptor set count, binding indices, and `VkPushConstantRange.size` are derived from the **same** contract macros. Example — VD layout:

- `setLayoutCount = 2`: `view_data_ubo_layout` + `image_sampler_layout`
- `pushConstantRange.size = SIT_VD_PATH_B_PUSH_CONSTANT_SIZE`

If external shaders change `set`/`binding` or push block size, **both** the GLSL files and the `VkPipelineLayoutCreateInfo` in init must be updated together. SPIR-V embed does not remove this coupling — it only removes runtime shaderc.

### Vertex input coupling

Internal VAO / `VkVertexInputAttributeDescription` setup uses `SIT_ATTR_POSITION` and `SIT_ATTR_TEXCOORD_0` with strides matching the quad/VD VBO layout (e.g. VD: `vec2 pos + vec2 uv`). External vertex shaders must keep the same `location` assignments.

### What stays in C (not moved to `sit/gpu/`)

| Stays in codebase | Why |
|-------------------|-----|
| `SIT_ATTR_*`, `SIT_UBO_BINDING_*`, `SIT_UNIFORM_LOC_*` in `situation_impl_decl.h` | Vertex setup, pipeline layouts, GL uniform locations |
| `_SitVDFillPath*PushConstants`, draw-path packing in `situation_impl_vd.h` / `situation_impl_renderer.h` | CPU-side data the shaders consume |
| `VkPipelineLayoutCreateInfo` in init | Must match shader `layout` / push sizes |

External shader files must remain **byte-compatible** with this C code. Harness tests on GL + VK are the primary guard; a separate machine-readable interface file is optional follow-up, not part of the folder layout.

---

## Compile model (unified sources, compile-time backend)

Today each shader is one C string with `#if defined(SITUATION_USE_VULKAN)` / `SITUATION_USE_OPENGL` inside it. Externalization **keeps that model**:

| Step | OpenGL build | Vulkan build |
|------|--------------|--------------|
| Source file | `sit/gpu/vd.frag` (single file) | **same** `sit/gpu/vd.frag` |
| Preprocessor | `-DSITUATION_USE_OPENGL` (or equivalent shaderc define) | `-DSITUATION_USE_VULKAN` |
| Compiler target | runtime GL compile or `glslc --target-env=opengl` | shaderc / `glslc --target-env=vulkan` |
| SPIR-V output (optional embed step) | build tree, same as harness | build tree, same as harness |

Shared logic (SMPTE idle helpers, color math) lives once in the source or in `#include`d `.glslh` files **without** backend branches. Backend-specific `layout(set,binding)` vs `layout(location=N) uniform` stays inside `#if` blocks in the same file — not in duplicate files.

**Non-goal:** maintaining parallel `graphics/gl/` and `graphics/vk/` trees (harness-style split is out of scope for core shaders).

---

## Target layout (`sit/gpu/`)

**Minimum deliverable** — seven shader files (three vertex, four fragment), lifted and streamlined from `situation_impl_decl.h`:

```
sit/gpu/
├── compositor.vert     # VD + advanced composite (merged; was two duplicate verts)
├── vd.frag
├── composite.frag
├── quad.vert           # quad + YPQ grade (YPQ already shared VS today)
├── quad.frag
├── ypq_grade.frag
├── text.vert           # separate — no model matrix, batched screen-space verts
└── text.frag
```

### Vertex shader sanitisation

| File | Serves | Rationale |
|------|--------|-----------|
| `compositor.vert` | `vd.frag`, `composite.frag` | `SIT_VD_VERTEX_SHADER_SRC` and `SIT_COMPOSITE_VERTEX_SHADER_SRC` are the same `main()` logic; share `vd_quad_vao` |
| `quad.vert` | `quad.frag`, `ypq_grade.frag` | Already shared in code via `SIT_QUAD_VERTEX_SHADER` |
| `text.vert` | `text.frag` only | Different transform (projection only, no model) and push layout — do not merge |

**`compositor.vert` push-constant note:** VD path B (96 bytes) and composite path A (112 bytes) differ in the **fragment** block. The shared vert only reads `model`. Declare the push struct in `compositor.vert` to match the **pipeline it is linked with** — use a compile-time define when one SPIR-V module cannot serve both layouts (e.g. `-DSIT_COMPOSITOR_PATH_A` vs `-DSIT_COMPOSITOR_PATH_B`), still from the **same** `.vert` file on disk.

Each file keeps today's `#if defined(SITUATION_USE_VULKAN)` / `SITUATION_USE_OPENGL` branches. Renderer init loads by **fixed path** (e.g. `sit/gpu/vd.frag` + `sit/gpu/compositor.vert`) — same idea as today's string constants, but from disk.

### Shader header documentation (required)

Every file under `sit/gpu/` must carry a **Situation-style Doxygen header** at the top (block comment **before** `#version`, which GLSL allows). Headers move **with** the shader out of `situation_impl_decl.h` — they are not left behind in C.

**Required tags** (match existing internal shaders in `situation_impl_decl.h`):

| Tag | Purpose |
|-----|---------|
| `@internal` | Core library shader; not public API |
| `@shader SIT_*` | Stable logical id (used in docs / cross-references) |
| `@brief` | One-line summary |
| `@details` | What the stage does, which C API drives it, backend notes, coupling to other files |
| `@var name (in\|out\|uniform, GL\|VK)` | Inputs, outputs, push/UBO/sampler — per backend where they differ |

**Optional but encouraged:** `@see` (C draw/init functions), `@note` (push layout size, path A/B), `@warning` (packing / sync constraints).

**Example** (`sit/gpu/vd.frag`):

```glsl
/**
 * @internal
 * @shader SIT_VD_FRAGMENT_SHADER
 * @brief Fragment stage for simple Virtual Display compositing.
 * @details Samples the VD offscreen texture (or idle SMPTE/solid fallback) and applies per-draw opacity.
 *          Driven by SituationRenderVirtualDisplays path B. Pairs with compositor.vert.
 *
 * @var v_texCoord (in) Interpolated UV; VK uses position-as-UV from vert, GL uses atlas coords.
 * @var u_screenTexture (uniform) VD source sampler (set 1 / binding SIT_SAMPLER_BINDING_VD_SOURCE on VK).
 * @var pc / u_opacity (uniform, VK / GL) Per-draw opacity and idle fallback fields.
 *
 * @see situation_impl_vd.h _SitVDFillPathBPushConstants
 * @see SIT_VD_PATH_B_PUSH_CONSTANT_SIZE
 */
#version 450 core
```

**Per-file `@shader` ids and documentation scope:**

| File | `@shader` id | Must document in `@details` |
|------|--------------|-----------------------------|
| `compositor.vert` | `SIT_COMPOSITOR_VERTEX_SHADER` | Shared VS for VD + composite; `vd_quad_vao`; path A/B push struct compile defines; transform + UV rules per backend |
| `vd.frag` | `SIT_VD_FRAGMENT_SHADER` | Simple VD blend; idle COLORBURST/SOLID; path B push layout (96 B); single source texture |
| `composite.frag` | `SIT_COMPOSITE_FRAGMENT_SHADER` | Photoshop-style blend modes; reads source + destination; path A push (112 B); idle fallback |
| `quad.vert` | `SIT_QUAD_VERTEX_SHADER` | Unit quad 0..1; `uv_rect` UV mapping; pairs with quad + ypq frags |
| `quad.frag` | `SIT_QUAD_FRAGMENT_SHADER` | Textured/colored quad; bindless GL path; `SituationCmdDrawQuad` / `DrawTexture` |
| `ypq_grade.frag` | `SIT_YPQ_GRADE_SHADER` | NTSC YPQ grade; sync with `situation_impl_ypq.h`; reuses quad.vert; extended push floats |
| `text.vert` | `SIT_TEXT_VERTEX_SHADER` | Screen-space batched verts; **no model matrix**; 6 verts/glyph built on CPU |
| `text.frag` | `SIT_TEXT_FRAGMENT_SHADER` | Glyph atlas sample; alpha from R/A; per-string color; bindless optional on GL |

**Gaps today:** `SIT_QUAD_*` and `SIT_TEXT_*` only have brief `//` comments in `situation_impl_decl.h`. Extraction **must** add full headers (lift + expand from VD/composite/YPQ examples).

**C-side:** Remove duplicated `@shader` comment blocks from `situation_impl_decl.h` once the canonical copy lives in `sit/gpu/`. Keep a one-line `// see sit/gpu/vd.frag` pointer if useful for grep.

**Deferred to Phase 4** (optional, much later):

- Shared `.glslh` contract headers (`sit_contract.glslh`, optional `sit_vd_idle.glslh`) for push structs, VK bindings, and duplicated helpers — see **Phase 4** below. Phase 1A inlined these intentionally to keep 1B/1C simple.

**Optional build output** (Phase 2, not under `sit/gpu/`):

- SPIR-V embed — lives in the **existing** build/scripts flow (`scripts/gen_spirv_embed.ps1`, `build/`, etc.), not under `sit/gpu/`.

**Explicitly not part of this layout:** `manifest.json`, `sit_core_interface.json`, `spirv/`, `embed/`, `contract/`, `graphics/` subfolders, or logical `core.*` naming indirection.

---

## Current state audit

### In-scope shader inventory

| File(s) in `sit/gpu/` | Current location | Notes |
|-----------------------|------------------|-------|
| `compositor.vert`, `vd.frag` | `SIT_VD_*` | Vert merged at extract; dual `#if` GL/VK |
| `compositor.vert`, `composite.frag` | `SIT_COMPOSITE_*` | Same vert as VD; blend modes in frag |
| `quad.vert`, `quad.frag` | `SIT_QUAD_*` | Bindless on GL |
| `quad.vert`, `ypq_grade.frag` | `SIT_YPQ_GRADE_*` | FS only; VS already shared |
| `text.vert`, `text.frag` | `SIT_TEXT_*` | Own vert; batched glyphs |

### Loader touchpoints (in scope only)

| Consumer | API today | Change |
|----------|-----------|--------|
| Internal renderer init (GL) | `SIT_*_SHADER` strings → `_SituationCreateGLShaderProgram` | Read `sit/gpu/*.vert|*.frag`; compile with `SITUATION_USE_OPENGL` (matches C build) |
| Internal renderer init (VK) | `SIT_*_SHADER_SRC` → `_SituationVulkanCompileGLSLtoSPIRV` | Same files; shaderc with `SITUATION_USE_VULKAN`; optional prebuilt SPIR-V later |
| User shaders | `SituationLoadShader` / `SituationLoadShaderFromSpirv*` | **Unchanged** |

### CPU-side draw / bind touchpoints (must stay in sync with external GLSL)

| Consumer | File | What it sends to GPU |
|----------|------|----------------------|
| VD compositing draw | `situation_impl_vd.h` | `_SitVDFillPathBPushConstants`, `_SitVDApplyCompositorIdleUniformsGL`, `vkCmdPushConstants`, descriptor binds for VD texture |
| Advanced composite draw | `situation_impl_vd.h` | `_SitVDFillPathAPushConstants` (+ `blendMode` at offset 64) |
| Quad / texture draw | `situation_impl_renderer.h` | `glProgramUniform*` (GL) or `vkCmdPushConstants` with quad anonymous struct |
| YPQ grade draw | `situation_impl_renderer.h` | Extends quad push block with grade floats |
| Text draw | `situation_impl_renderer.h` | `text_pc.color` push or `u_color` uniform |
| Vulkan pipeline layout | `situation_impl_renderer.h` `_SituationVulkanInitInternalRenderers` | `VkDescriptorSetLayout` + `VkPushConstantRange.size` per pipeline |

**This plan does not rewrite draw paths.** C packing and bind code stay as-is; external shaders must remain compatible. Harness tests are the gate.

### Explicitly unchanged

| Consumer | Location | Notes |
|----------|----------|-------|
| K-Term | `kterm_api.h` preambles + `sit/k-term/shaders/*.comp` | Own load path; no migration |
| Polysonix | `sit/aud/polysonix/px_vm.comp` | Own load path; no migration |
| Harness SPIR-V tests | `tests/harness/shaders/` + `compile_harness_shaders.ps1` | Separate build script |
| Demon Hunt | `examples/demon_hunt/` | Separate example pipeline |

---

## Blockers (must solve)

### B1 — Compile-time backend selection in unified sources

**Problem:** Moving shaders to disk must preserve today's model: one source, `#if defined(SITUATION_USE_VULKAN)` / `SITUATION_USE_OPENGL` branches, selected when the **library** is built — not by choosing different files per API.

**Solution:**

1. **One** `.vert` / `.frag` per pipeline directly under `sit/gpu/`, lifted verbatim from embedded strings (including `#if` blocks).
2. Runtime GL load and runtime shaderc compile pass the **same macro** the C translation unit uses (`SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`). Mechanism:
   - **OpenGL:** `_SituationInjectGLSLDefinesAfterVersion` string-prepends `#define SITUATION_USE_OPENGL 1\n` (or `_VULKAN`) immediately after the `#version` line (GLSL requires `#version` to be first; injection goes on the very next line). Source string is then passed to `glShaderSource`.
   - **Vulkan/shaderc:** `_SituationVulkanCompileCoreShaderFile` passes the define via shaderc's `shaderc_compile_options_add_macro_definition` API before calling `shaderc_compile_into_spv`. Additional per-pipeline macros (e.g. `SIT_COMPOSITOR_PATH_B`) are passed as a caller-supplied `_SituationShadercMacro` array.
3. Optional SPIR-V embed (Phase 3): compile the same files twice with different defines/`--target-env`; output stays in the **build** tree via existing embed scripts — not a prescribed `sit/gpu/spirv/` layout.
4. Shared chunks may use `#include` only when needed during extraction; backend forks stay as `#if` in the stage file.

**Exit criteria:** `vd.frag` loads on GL and VK from `sit/gpu/vd.frag`; `test_virtual_display.c` unchanged.

---

### B2 — Stringified contract macros in embedded GLSL

**Problem:** Embedded shaders use `SIT_STRINGIFY(SIT_ATTR_POSITION)` etc. External `.frag` / `.vert` files cannot use C stringification.

**Solution:**

1. **Keep** `SIT_*` binding/attribute `#define`s in `situation_impl_decl.h` for C code.
2. When lifting each shader, replace stringified macros with **numeric literals** matching those defines (or a single optional `.glslh` if many files share the same literals).
3. Wire shaderc include path only if an include file is actually added.

**Exit criteria:** External shaders compile; binding numbers still match C `VkPipelineLayout` / VAO setup.

---

### B7 — Push-constant layout must stay aligned with C packing

**Problem:** `_SitVDFillPath*PushConstants` uses fixed `memcpy` offsets. Reordering fields in external GLSL silently breaks compositing.

**Solution:** Lift shaders **without** changing push struct field order. Verify with existing harness tests (`test_virtual_display.c`, `test_graphics.c`). No new JSON schema required for v1.

**Exit criteria:** Harness green on GL + VK after extraction.

---

### B8 — Shared `compositor.vert` across two push layouts

**Problem:** VD and composite fragment pipelines use different `VkPushConstantRange` sizes (96 vs 112). A single vertex SPIR-V module must declare a push block compatible with the pipeline it pairs with.

**Solution:**

1. One `compositor.vert` on disk; `main()` is identical for both paths.
2. When compiling SPIR-V for the VD pipeline, define `SIT_COMPOSITOR_PATH_B` so the vert declares `VDPushConstants`; for composite, define `SIT_COMPOSITOR_PATH_A` for `CompositePushConstants`.
3. Runtime GLSL path: same single file; init links the correct vert+frag pair per pipeline (as today).

**Exit criteria:** Both VD and composite pipelines init and pass harness on GL + VK.

---

### B3 — Vulkan internal init requires runtime shaderc

**Problem:** `_SituationVulkanInitInternalRenderers` compiles embedded GLSL at init when `SITUATION_ENABLE_SHADER_COMPILER` is set. Without shaderc, internal pipelines stay NULL (comment ~L5431: precompiled SPIR-V is future work).

**Solution:**

1. Extend existing embed workflow (`scripts/gen_spirv_embed.ps1` or a thin `compile_core_gpu_shaders.ps1` wrapper) to compile `sit/gpu/*` sources.
2. `_SituationVulkanInitInternalRenderers` tries embed first when present; optional runtime GLSL fallback for dev.

**Exit criteria:** Vulkan init succeeds with `SITUATION_ENABLE_SHADER_COMPILER` **off** when embed is present.

---

### B4 — SPIR-V target non-portability

**Problem:** OpenGL-target and Vulkan-target `.spv` are incompatible (`doc/plan/SHADER_DEBUG_PLAN.md`).

**Solution:** Same source file, two compile passes (GL vs VK defines + `--target-env`). Embed table keyed by `(shader_path, backend)` in generated C — same pattern as harness.

**Exit criteria:** Core internal pipelines init correctly on both backends from embed.

---

### B5 — Dev vs release asset resolution

**Problem:** Runtime GLSL load needs a stable path to `sit/gpu/` when CWD varies (`build/` vs repo root).

**Solution (internal only):**

1. Fixed paths such as `"sit/gpu/vd.frag"` (or compile-time string constants in `situation_impl_decl.h` / renderer init) — no manifest indirection.
2. `_SituationLoadCoreShaderFile` search order (implemented in `situation_impl_renderer.h`):
   - Pass 1: CWD-relative path as-is (works when running from repo root).
   - Pass 2: Relative path prefixed with `../`, `../../`, `../../../`, `../../../../` (covers `build/`, `build/tests/`, etc.).
   - Pass 3: Exe-base-relative path via `SituationGetBasePath()` (handles relocated binaries).
   - Pass 4: Exe-base + each of the four `../` prefixes applied to the relative path.
   - On all misses: `SITUATION_ERROR_FILE_NOT_FOUND` with full detail string listing the original path.
3. No public `SituationSetGpuShaderRoot` required unless we later promote it; keep internal for v1.

**Exit criteria:** Renderer init works when test binary runs from `build/`.

---

### B6 — Doc drift (`doc/COMPILATION_GUIDE.md` references missing `shaders/`)

**Problem:** Root `shaders/` folder documented but absent.

**Solution:** Update guide to document `sit/gpu/` as the **core library** shader root.

---

## Loader change (minimal)

Replace reading `SIT_*_SHADER_SRC` string constants with **read file → same compile path** as today. No new public API; at most a small internal `_SituationLoadCoreShaderFile(const char* path)` used by existing init functions.

Public `SituationLoadShader` and user shader APIs are **unchanged**.

---

## Phases

Each phase ends with a **Gate** (objective pass/fail) and **Verify** (concrete commands). Check boxes as work lands; do not skip Verify.

---

### Phase 0 — Scaffold (no behavior change)

**Purpose:** create the folder and docs; runtime still uses embedded strings.

- [x] Create `sit/gpu/` directory
- [x] Update `doc/COMPILATION_GUIDE.md` — `sit/gpu/` is the core internal shader root (replaces absent root `shaders/` mention)

**Gate:** Folder exists; full build unchanged; harness green (embedded shaders still active).

**Verify:**

- [x] `build_situation.bat` (or project default build) succeeds
- [x] `sit_test.exe` / `sit_test_vulkan.exe` graphics + VD modules unchanged

---

### Phase 1A — Extract shader files (blockers B1, B2, B7)

**Purpose:** all seven GLSL files exist on disk with headers; C code still compiles embedded strings (dual path OK briefly).

- [x] `compositor.vert` — merge `SIT_VD_VERTEX_SHADER_SRC` + `SIT_COMPOSITE_VERTEX_SHADER_SRC`; `#if` for path A/B push struct names (B8)
- [x] `vd.frag` — from `SIT_VD_*`; include `SIT_VD_IDLE_*` body; full `@shader` header
- [x] `composite.frag` — from `SIT_COMPOSITE_*`; full header
- [x] `quad.vert` + `quad.frag` — from `SIT_QUAD_*`; **new** full headers (gap today)
- [x] `ypq_grade.frag` — from `SIT_YPQ_GRADE_*`; note `quad.vert` pairing in header
- [x] `text.vert` + `text.frag` — from `SIT_TEXT_*`; **new** full headers (document 6 verts/glyph, no model matrix)
- [x] Replace all `SIT_STRINGIFY(...)` with numeric literals matching `SIT_*` defines in decl.h
- [x] Optional: extract `SIT_VD_IDLE_*` macro strings into `.glslh` include if cleaner than inline — **skipped** (inlined in `vd.frag` / `composite.frag`)

**Gate:** Seven files under `sit/gpu/`; each has `@internal` + `@shader` + `@brief` + `@details` + `@var`; manual `glslc`/shaderc smoke-compile one GL and one VK file with correct defines.

**Verify:**

- [x] All seven paths present: `sit/gpu/{compositor.vert,vd.frag,composite.frag,quad.vert,quad.frag,ypq_grade.frag,text.vert,text.frag}`

---

### Phase 1B — Wire file loader (blockers B1, B5, B8)

**Purpose:** renderer init reads `sit/gpu/` instead of string constants; embedded strings may remain until 1C.

- [x] Add internal `_SituationLoadCoreShaderFile(const char* path, char** out_src)` (or equivalent) with dev path fallbacks (`sit/gpu/`, `../sit/gpu/`, …)
- [x] OpenGL: `_SituationCreateGLShaderProgram` call sites for VD, composite, quad, YPQ, text — load file pairs from `sit/gpu/`
- [x] Vulkan: `_SituationVulkanInitInternalRenderers` + `_SituationInitQuadRenderer` + `_SituationInitYpqGradeRenderer` + text init — load same paths; pass `SITUATION_USE_*` define to shaderc
- [x] `compositor.vert` linked twice: VD program (path B defines) + composite program (path A defines)
- [x] Register shaderc include directory only if `.glslh` exists (deferred to **Phase 4** unless added early)

**Gate:** Renderer uses disk shaders; harness passes on GL + VK running from repo root **and** from `build/`.

**Verify:**

- [x] `sit_test.exe --module graphics` (or project VD/quad/text filters) — pass (GL, repo root)
- [x] `sit_test_vulkan.exe --module graphics` — pass (VK, repo root)
- [x] `sit_test.exe --module virtual_display` — pass (26/26, GL, repo root)
- [x] `sit_test_vulkan.exe --module virtual_display` — pass (26/26, VK, repo root)
- [x] Repeat at least one run with CWD = `build/` — GL 26/26 virtual_display, graphics pass; VK 26/26 virtual_display from `build/`

---

### Phase 1C — Retire embedded strings (completes main deliverable)

**Purpose:** remove duplicate source from `situation_impl_decl.h`.

- [x] Delete `SIT_*_SHADER_SRC` / `SIT_*_SHADER` string constants from `situation_impl_decl.h`
- [x] Delete C-side `@shader` comment blocks for moved shaders (canonical copy is in `sit/gpu/`)
- [x] Keep `SIT_*` contract `#define`s, push size macros, and all draw/pack code
- [x] Grep decl.h: no `#version`, no embedded GLSL string bodies

**Gate:** Phase 1 complete — `sit/gpu/` is sole shader source; harness green both backends; K-Term/Polysonix untouched.

**Verify:**

- [x] `rg '#version' sit/situation_impl_decl.h` — no matches in shader section
- [x] `rg 'SIT_VD_VERTEX_SHADER_SRC|SIT_QUAD_VERTEX_SHADER' sit/` — no definitions (two stale `@par` doc-comment references in `situation_impl_renderer.h` updated to `sit/gpu/` paths in v2.4.254)
- [x] K-Term / Polysonix test suites unchanged (scope guard)

---

### Phase 2 — SPIR-V embed (optional; blockers B3, B4)

**Status: PARKED** — see decision below.

**Purpose:** Vulkan internal pipelines without runtime shaderc.

**Decision (2026-06-12):** Phase 2 is not required for the current shipping target. `build_situation.bat static-vulkan` always defines `SITUATION_ENABLE_SHADER_COMPILER`, so no distribution scenario hits the no-shaderc path today. Phase 2 becomes relevant only if a shaderc-free Vulkan distribution target is explicitly needed.

**Prerequisite fix shipped (v2.4.255):** The prior silent-success lie (`_SituationVulkanInitInternalRenderers` returning `SITUATION_SUCCESS` with NULL pipelines when shaderc is off) has been replaced with a hard `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` (-757) error and a descriptive message. The no-shaderc path now fails loudly rather than silently corrupting rendering.

**Remaining tasks (when Phase 2 is picked up):**

- [ ] Add `compile_core_gpu_shaders.ps1` (or extend `gen_spirv_embed.ps1`) — compile each `sit/gpu/*` stage twice (`SITUATION_USE_OPENGL` + `opengl` env, `SITUATION_USE_VULKAN` + `vulkan` env) using `%VULKAN_SDK%\Bin\glslc.exe`
- [ ] `compositor.vert`: separate SPIR-V for path A and path B defines
- [ ] Generate embed C (e.g. `sit_core_spirv_embed.c`) into build tree; hook from `build_situation.bat` or document pre-step
- [ ] `_SituationVulkanInitInternalRenderers`: embed-first; runtime GLSL fallback when `SITUATION_GPU_RUNTIME_COMPILE` or shaderc enabled
- [ ] Remove/replace `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` early-exit with embed path

**Gate:** Library links and renders internal pipelines with `SITUATION_ENABLE_SHADER_COMPILER` off when embed is built.

**Verify:**

- [ ] Build without shaderc dep — success
- [ ] `sit_test_vulkan.exe` VD + quad + text paths — pass

---

### Phase 3 — Ship docs

**Purpose:** changelog and plan closure.

- [x] Update `doc/UPDATELOG.md`, `doc/whatsnew.md`
- [x] Mark this plan **Status:** complete with version shipped (v2.4.254, 2026-06-12)
- [x] Close open questions (embed committed vs build-only — build-only, deferred to Phase 2; Phase 2 in same release or follow-up — **follow-up**)

**Gate:** Docs reflect externalized `sit/gpu/` layout and three-vert streamlining.

**Verify:**

- [x] Plan checkboxes for Phases 0–1C checked
- [x] Success criteria §1–8 satisfied

---

### Phase 4 — Shared shader contract headers (optional; explore much later)

**Purpose:** reduce duplication and drift risk by extracting repeated GLSL contracts into `#include`d `.glslh` files. **No behavior change** — refactor only, after **1C** (and ideally after **2** if SPIR-V embed is in use) so `sit/gpu/` is stable and disk-loaded.

**Why defer:** Phase 1A intentionally inlined SMPTE helpers and numeric literals to keep 1B/1C focused on migration. Eight stage files are manageable; harness tests already guard push-layout mistakes. Includes add loader/include-path work (`-I sit/gpu/` for shaderc and GL runtime compile) without unblocking the main deliverable.

**Suggested layout (exploratory — not prescriptive until someone picks this up):**

```
sit/gpu/
├── sit_contract.glslh   # push struct definitions, VK set/binding #defines, vertex attr loc #defines
├── sit_vd_idle.glslh    # SMPTE idle helpers (optional split from sit_contract)
├── compositor.vert
├── vd.frag
└── …
```

**What belongs in shared headers (high ROI):**

| Content | Rationale |
|---------|-----------|
| `VDPushConstants`, `CompositePushConstants`, `QuadPushConstants`, `YpqGradePushConstants`, `TextPushConstants` | Must match `memcpy` packers in `situation_impl_vd.h` / renderer draw paths — highest drift risk |
| Vulkan `SIT_UBO_BINDING_*`, `SIT_SAMPLER_BINDING_*` as GLSL `#define`s | Same numbers as `situation_impl_decl.h`; used across all VK branches |
| Vertex attribute loc `#define`s (`SIT_ATTR_POSITION`, `SIT_ATTR_TEXCOORD_0`, …) | Mirror C `SIT_ATTR_*`; vert files use names instead of bare `0`, `2` |
| SMPTE / shared math (`_sit_smpte_*`, YPQ constants if duplicated) | Today duplicated in `vd.frag` + `composite.frag` |

**What stays per-stage (do not centralize blindly):**

| Content | Rationale |
|---------|-----------|
| OpenGL `layout(location = N) uniform` blocks | Per-pipeline — text lacks `u_blendMode`, VD lacks `u_uv_rect`; a monolithic “all GL locs” header is easy to misuse |
| `main()` and stage-specific logic | Stage files remain the unit of compilation |
| C contract macros in `situation_impl_decl.h` | **Stay in C** unless a later codegen step generates `sit_contract.glslh` from the same source |

**Cross-file comment (manual sync until codegen):**

```c
/* Must match sit/gpu/sit_contract.glslh */
```

```glsl
// Must match situation_impl_decl.h SIT_* contract macros (~1760–1805)
```

**Tasks (when pursued):**

- [ ] Add `sit_contract.glslh` (and optionally `sit_vd_idle.glslh`) with push structs + VK binding/attr `#define`s
- [ ] Refactor stage files to `#include "sit_contract.glslh"` (Doxygen headers stay **above** `#version`; include typically immediately after `#version 450 core`)
- [ ] Extend `_SituationLoadCoreShaderFile` / shaderc init: register `sit/gpu/` as include directory; verify from repo root **and** `build/` CWD
- [ ] If Phase 2 embed exists: update `compile_core_gpu_shaders.ps1` with same `-I` path; re-embed; confirm SPIR-V unchanged aside from debug names
- [ ] Document in `doc/COMPILATION_GUIDE.md`
- [ ] *(Optional follow-up)* codegen `sit_contract.glslh` from `situation_impl_decl.h` to eliminate manual dual maintenance

**Gate:** Harness green on GL + VK; no change to push byte sizes, pipeline layouts, or rendered output; grep shows struct definitions live in one `.glslh` file.

**Verify:**

- [ ] `sit_test.exe` + `sit_test_vulkan.exe` — graphics + virtual_display modules pass
- [ ] `rg 'layout\\(push_constant\\)' sit/gpu/*.frag sit/gpu/*.vert` — declarations reference included struct names, not duplicate struct bodies
- [ ] `rg '_sit_smpte_' sit/gpu/*.frag` — helpers appear only in `sit_vd_idle.glslh` (or `sit_contract.glslh`), not duplicated across `vd.frag` / `composite.frag`

**Not in scope for Phase 4:** replacing C macros with GLSL as sole authority; merging OpenGL uniform declarations into one header; changing K-Term/Polysonix shader trees.

---

## Test plan

| Area | Command / module | Backends |
|------|-------------------|----------|
| Internal renderer | `tests/harness/test_virtual_display.c` | GL + VK |
| Draw / raster | `tests/harness/test_graphics.c` (VD, quad, text paths) | GL + VK |
| Path resolution | Harness from `build/` and repo root (existing tests) | both |
| No shaderc build | Phase 2 only — build without `SITUATION_ENABLE_SHADER_COMPILER` | VK |

**Regression guard (unchanged modules):** K-Term and Polysonix test suites should pass without modification — if they fail, scope has leaked.

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| GL/VK branch drift inside `#if` | Single source file per stage; contract test + harness on both backends; review `#if` blocks when editing |
| Push-constant layout silent corruption | Do not reorder GLSL push fields; harness VD/composite tests catch drift; **Phase 4** consolidates struct defs when duplication becomes painful |
| Contract drift between C macros and GLSL literals | Manual comment cross-refs until **Phase 4**; optional codegen from `situation_impl_decl.h` as follow-up |
| Pipeline layout / SPIR-V mismatch | `compositor.vert` compiled per path A/B; init keeps existing `SIT_VD_PATH_*` sizes |
| Shared vert / mismatched push layout | B8: compile-time `SIT_COMPOSITOR_PATH_*` defines from same `.vert` file |
| Embed size in static lib | Core set is small (7 files); acceptable |
| Accidental scope creep into K-Term/Polysonix | Explicit out-of-scope list; no changes under `sit/k-term/` or `sit/aud/polysonix/` |
| Vulkan init regression without shaderc | Embed-first init with clear build-time error if embed missing |

---

## Success criteria

1. **Core shaders only** under `sit/gpu/` — no K-Term or Polysonix files moved.
2. **No embedded GLSL** in `situation_impl_decl.h` for internal renderer pipelines.
3. **C draw/bind code** unchanged; external shaders remain compatible with existing packing.
4. **Harness renderer tests** green on OpenGL and Vulkan.
5. **K-Term and Polysonix** unchanged.
6. **One source per stage** under `sit/gpu/`; backend via compile-time `#if` only.
7. **Streamlined verts:** three vertex files (`compositor.vert`, `quad.vert`, `text.vert`) — not five.
8. **Documented shaders:** each `sit/gpu/*` file has a complete `@internal` / `@shader` header describing purpose, CPU driver, and resource interface.

---

## Open questions (maintainer)

1. ~~Is Phase 2 (SPIR-V embed / no shaderc) required for the first ship, or a follow-up?~~ **Resolved (v2.4.254):** follow-up. Phases 0–1C ship without it. Vulkan builds still require `SITUATION_ENABLE_SHADER_COMPILER` for internal pipelines. The silent-success lie fixed in v2.4.255 — now returns `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` (-757).
2. Commit generated SPIR-V embed in repo or build-only? **Deferred to Phase 2.**
3. If Phase 4 lands, is manual `sit_contract.glslh` sync enough, or is codegen from `situation_impl_decl.h` warranted? **Deferred to Phase 4.**

---

## References

- `sit/situation_impl_decl.h` — shader contract macros (~1760–1805), embedded shaders (~1880–2330)
- `sit/situation_impl_vd.h` — `_SitVDFillPathAPushConstants`, `_SitVDFillPathBPushConstants`, GL idle uniforms
- `sit/situation_impl_renderer.h` — draw-path push packing, `_SituationVulkanInitInternalRenderers` (~5404)
- `scripts/gen_spirv_embed.ps1`
- `compile_harness_shaders.ps1` — reference pattern only; not in scope
