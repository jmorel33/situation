# RGL Test Pattern → Situation Shader Migration Plan

**Status:** In progress — **P0–P14 shipped** (v2.4.378 + `25_vd_standby`); §5.3 gate, §8 shim remaining  
**As of:** Situation v2.4.378  
**Date:** June 23, 2026 (design review + first implementation slice: June 23, 2026)  
**Scope:** Migrate RGL test-pattern technology into reusable GLSL headers under `sit/gpu/test_patterns/`. **Authoritative plan for layer stack: §3.5 (P11). User-facing explorer: §6.4 / P14 (`25_vd_standby`).**  
**Related:** `doc/misc/RGL_MIGRATION_PLAN.md` §7D.3, `doc/plan/SHADER_DEBUG_PLAN.md`, `doc/misc/TEST_SPIRV_SHADER_API.md`, `doc/done/EXTERNALIZE_GPU_COMPUTE_PLAN.md` Phase 4, `sit/gpu/vd.frag`, `sit/gpu/composite.frag`, `sit/gpu/vertex_pull.glslh`

### Progress snapshot (June 27, 2026 — v2.4.378)

| Deliverable | State |
|---|---|
| `sit/gpu/test_patterns/sit_tp_config.glslh` | ✅ Shipped — enums, `SitTpConfig`, calibration mask, chroma snow bit **16** |
| `sit/gpu/test_patterns/sit_tp_noise.glslh` | ✅ Shipped — B&W + chroma idle snow (`noise_frame_seed`) |
| `sit/gpu/test_patterns/sit_tp_colors.glslh` | ✅ Shipped — `SitTpPalette`, `sit_tp_default_palette()` |
| `sit/gpu/test_patterns/sit_tp_primitives.glslh` | ✅ Shipped — grid, checkerboard, stripes, safe area, crosshair, gradient, rect, ring |
| `sit/gpu/test_patterns/sit_tp_smpte.glslh` | ✅ VD subset + **full RGL SMPTE** (`sit_tp_smpte_bars`) |
| `sit/gpu/test_patterns/sit_tp_*.glslh` (6 modules) | ✅ convergence, gradients, pluge, multiburst, crosshatch |
| `sit/gpu/test_patterns/sit_tp_cube.glslh` | ✅ **One lit cube** (P13 — replaces grid raymarch) |
| `sit/gpu/test_patterns/sit_test_patterns.glslh` | ✅ **`sit_tp_sample()`** — snow, preset, **stack compose** (`sit_tp_compose_stack`) |
| `sit/gpu/test_patterns/sit_tp_compose.glslh` | ✅ **P11** — stack loop + `sit_tp_apply_layer` |
| `sit/gpu/test_patterns/sit_tp_config_header_ubo.glslh` | ✅ **P10** — 160 B std140 header (stack + seed) |
| `sit/gpu/test_patterns/sit_tp_layer_params*.glslh` | ✅ **P10** — std430 SSBO mirrors + palette |
| `_SituationShaderIncluderResolve` multi-path search | ✅ Shipped — fingerprint `^= 0x2` |
| `vd.frag` / `composite.frag` Vulkan `#include` | ✅ Shipped — header UBO + SSBO, idle **PATTERN** |
| `vd.frag` / `composite.frag` OpenGL SPIR-V compositor | ✅ Embed + init/composite fix |
| VD C API (`Set/GetVirtualDisplayPatternConfig`, layers, ChromaSnow) | ✅ Shipped — **`SitVdStandbyConfig`** (P9) |
| **`sit/situation_api_types_gpu.h`** | ✅ **P9/P10** — struct, stack, palette, SSBO offsets, pack APIs |
| Harness P7 (`--filter pattern`) | ✅ graphics **15/15** GL · **16/16** VK; full harness **17/17** GL (incl. VD standby) |
| **§3.5 Layer stack** (user compose order) | ✅ **P11** — shader stack loop + default stack |
| **§3.6 Per-layer params + unified struct** | ✅ **P9 CPU + P10 SSBO + shader reads** |
| **P14** `25_vd_standby` digestible example | ✅ **Shipped** — `examples/25_vd_standby/` |
| P6 lab (`shader_lab_test_patterns.c`, keys 0–8) | ⏸ **Optional dev lab** — superseded for users by **P14** |
| P8 guide | ✅ `doc/guide/test_patterns.md` rewritten (snow, chroma, idle flow) |
| §5.3 pixel-identical gate | ⏳ Pending formal COLORBURST gate |
| §0.5 A/B OpenGL unified headers (non-SPIR-V stages) | ⏳ Quad/text still raw GLSL; compositor on SPIR-V path |
| §8 optional RGL shim | ⏳ Not started |

**345 regression (must not repeat):** glslc `-fauto-bind-uniforms` + implicit sampler locations → SPIR-V link failure; docs must not claim GL parity before green `sit_test_opengl.exe` init + VD composite.

**Build verified:** `build_situation.bat vulkan` + `opengl` OK (OpenGL runs `compile_vd_compositor_gl.ps1` before link; requires glslc).

### Implementation touchpoints (shipped files)

| File | Role |
|---|---|
| `sit/situation_impl_renderer.h` | `_SituationShaderIncluderLoadFile`, `_SitVkShadercOptionsFingerprint` `^= 0x2` |
| `sit/gpu/test_patterns/sit_tp_config.glslh` | Enums, `SitTpConfig`, UV helpers |
| `sit/gpu/test_patterns/sit_tp_colors.glslh` | RGL palette |
| `sit/gpu/test_patterns/sit_tp_primitives.glslh` | Analytic drawing primitives |
| `sit/gpu/test_patterns/sit_tp_smpte.glslh` | VD subset SMPTE (`SIT_TP_SMPTE_VD_SUBSET`) |
| `sit/gpu/vd.frag` | Vulkan `#include`; OpenGL `#else` inline mirror |
| `sit/gpu/composite.frag` | Same as `vd.frag` |

**Compositor pattern (Vulkan):**

```glsl
#if defined(SITUATION_USE_VULKAN)
#define SIT_TP_SMPTE_VD_SUBSET 1
#include "sit/gpu/test_patterns/sit_tp_smpte.glslh"
#else
/* §0.5-C: inline mirror — must stay synced with sit_tp_smpte.glslh */
#endif
```

---

## Focus — Modular GPU libraries via `#include`

**This plan is not just RGL parity.** It is the first **showcase consumer** of Situation's runtime modular GLSL (shaderc `#include` on Vulkan). **All pattern logic lives in `sit/gpu/test_patterns/*.glslh`** — that is the product. We do not inline tooling into `vd.frag` as a second source of truth.

### Authoring vs delivery (do not conflate)

| Layer | What it is | This plan |
|---|---|---|
| **Authoring (source of truth)** | `sit/gpu/test_patterns/*.glslh` — primitives, SMPTE, dispatcher | **Always headers.** Single place to edit pattern math. |
| **User consumption** | `#include "sit/gpu/test_patterns/sit_test_patterns.glslh"` in app/lab/harness shaders | Vulkan: runtime shaderc. OpenGL apps: glslc embed (`-I sit/gpu`) or flatten — headers unchanged. |
| **Built-in compositor consumption** | `vd.frag` / `composite.frag` pull in SMPTE subset | **Vulkan:** `#include` at `SituationInit()`. **OpenGL core:** §0.5 *delivery* shim (flatten or temporary sync) — not a fork of the math. |

Option C (OpenGL keeps inline SMPTE temporarily) means **stale duplicate until synced from the header** — a maintenance shim only, not an alternate design. North star: both backends consume the same `.glslh` files (option A or B).

### How built-in shaders compile today (not DLL build time)

Core renderer shaders are **library-internal** but **not baked into the DLL** at link time (SPIR-V embed for core: deferred per `EXTERNALIZE_GPU_COMPUTE_PLAN.md` Phase 2).

```
build situation_*.dll     →  C code + shaderc link (Vulkan only);  sit/gpu/*.frag NOT compiled here
        │
        ▼
SituationInit()           →  _SituationLoadCoreShaderFile("sit/gpu/vd.frag", …)  // disk read + path fallbacks
        │
        ├─ Vulkan  →  shaderc (+ #include)  →  SPIR-V  →  VkPipeline
        └─ OpenGL  →  glCompileShader (raw GLSL, no #include)  →  GL program
```

Apps need `sit/gpu/` on disk (or path fallbacks to repo root) unless/until core SPIR-V embed lands. This plan does **not** change that model unless §0.5 option A explicitly unifies OpenGL core init.

### Library behavior contract (default slice)

| Area | Changes? |
|---|---|
| VD idle SMPTE / compositor output | **No** — refactor only; §5.3 pixel-identical gate |
| `SituationInit` flow (disk load at startup) | **No** — unless §0.5 option A |
| Public C API | **No** new required APIs in v1 |
| User shaders | **Additive** — optional `#include` library |
| RGL `RGL_DrawTestPattern` | **No** — unless optional §8 shim is implemented |

### What the library already gives you

Shipped since **Situation 2.3.x** (`doc/updatelog_23.md` — *Shader #include Support*):

- `_SituationShaderIncluderResolve` wired into every **Vulkan shaderc** compile — user shaders (`SituationLoadShaderFromMemory`) **and** internal pipelines (`vd.frag`, `composite.frag`, compositor, quad, text, … via `_SituationVulkanCompileCoreShaderFile`).
- Shared headers under `sit/gpu/` — `vertex_pull.glslh` (no consumer yet); **`sit/gpu/test_patterns/*.glslh`** — **first shipped `#include` consumer** (compositor SMPTE on Vulkan).
- One authoring model: write GLSL, `#include` shared contracts, load from memory or disk — SPIR-V cache, hot-reload, harness gates.

**Verified (June 2026):** glslc and the runtime resolver both resolve `#include "sit/gpu/test_patterns/sit_tp_smpte.glslh"` when the repo root is on the include path (CWD or `-I`). Compositor stages consume it at `SituationInit()` on Vulkan.

### What test patterns unlock

| RGL (legacy) | Situation (this plan) |
|---|---|
| CPU batch queue: hundreds of `DrawRectangle` / `DrawLine` calls | One `sit_tp_sample(uv, cfg, pal)` in fragment shader |
| Pattern logic locked in C | Pattern logic in **reusable `.glslh` headers** — import like a library |
| Separate code paths for debug vs compositor | Same SMPTE header: VD idle subset + full calibration mode |
| No user extensibility | Any user shader: `#include "sit/gpu/test_patterns/sit_test_patterns.glslh"` |

Nine broadcast-grade calibration patterns (SMPTE, PLUGE, multiburst, convergence, …) become a **GPU standard library** — zero new draw APIs, zero batch overhead, hot-reload friendly, readable in RenderDoc as procedural color.

### Strategic outcomes (why we pursue this)

1. **Establish `sit/gpu/` as the shader SDK** — test patterns join `vertex_pull.glslh`; Phase 4 of `EXTERNALIZE_GPU_COMPUTE_PLAN.md` (`sit_contract.glslh`) follows the same pattern.
2. **Kill duplication** — `_sit_smpte_*` in `vd.frag` + `composite.frag` → one **`sit_tp_smpte.glslh`** — **done on Vulkan**; OpenGL keeps §0.5-C mirror until A/B.
3. **Prove dual consumption** — compositor and `shader_lab_test_patterns.c` share the **same header source**; delivery path may differ by backend.
4. **First CI gate for `#include`** — harness shaders + runtime compile test; no longer "wired but untested."
5. **RGL migration story** — §7D.3 moves from "low priority CPU queue" to "shader library; RGL shim optional."

**Tagline for docs/examples:** *Import calibration. Not draw calls.*

### Core shader dual pipeline — delivery gap, not authoring gap

Built-in `sit/gpu/` stages use **two runtime compile contracts** at `SituationInit()`. Test patterns expose that split because compositor stages are the first built-ins to **consume** shared `.glslh` headers.

| | **Vulkan core** | **OpenGL core** |
|---|---|---|
| **When** | `SituationInit()` | `SituationInit()` |
| **Entry** | `_SituationVulkanCompileCoreShaderFile` | `_SituationCreateGLCoreShaderProgram` |
| **Compiler** | shaderc → SPIR-V → `vkCreateShaderModule` | `glCompileShader` / `glLinkProgram` on raw GLSL |
| **DLL flag** | `SITUATION_ENABLE_SHADER_COMPILER` | *not defined* (`sit/Makefile`) |
| **Can `#include` `.glslh` at init?** | ✅ yes | ❌ no (driver path) |
| **How to consume headers anyway** | `#include` in `vd.frag` | Option **B** flatten into stage file; option **A** unify init on shaderc/SPIR-V; option **C** temporary inline copy synced from header |

**Headers are backend-neutral.** `#if SITUATION_USE_VULKAN` / `SITUATION_USE_OPENGL` in stage files (unchanged). The dual pipeline is **how the library loads built-ins**, not how test patterns should be written.

**Implication for this plan:**

- **P1–P4:** build the `.glslh` library (authoring — backend agnostic).
- **Vulkan P2:** compositor `#include`s `sit_tp_smpte.glslh` directly.
- **OpenGL P2:** pick §0.5 **delivery** option — do not fork pattern logic into `vd.frag` as the long-term design.
- **Unification (option A):** optional follow-up; closes dual pipeline for all `sit/gpu/` built-ins, not only test patterns.

### Focus checklist (ship with P8 docs)

- [x] `doc/guide/test_patterns.md` — `#include` library, VD summon, fullscreen draw, FAQ
- [ ] `shader_lab_test_patterns.c` demonstrates one-line pattern import on keypress 0–8
- [ ] `doc/guide/graphics.md` or `doc/COMPILATION_GUIDE.md` cross-link: `sit/gpu/` shared headers + CWD convention
- [ ] Close `EXTERNALIZE_GPU_COMPUTE_PLAN.md` Phase 4 item: "first real `.glslh` consumer" → **test patterns** — **partial:** compositor SMPTE on Vulkan; full library pending P4
- [ ] Optional: short `doc/whatsnew.md` blurb — modular shader libraries land

---

## Master checklist

- [x] §0 — RGL inventory sign-off (design review; enum order verified)
- [x] §0.5 — **OpenGL core delivery strategy** — **option C** recorded (temporary GL inline mirror)
- [ ] **Focus** — first real `#include` consumer **shipped**; docs tagline still open (P8)
- [x] §1 — Header tree (partial) + include resolver + cache fingerprint
- [x] §2 — Primitive layer (`sit_tp_primitives.glslh`)
- [x] §3 — Palette + config headers + **std140 UBO contract** (`sit_tp_config_ubo.glslh`, `sit/sit_test_pattern_config.h`)
- [x] §3.6 — **Unified `SitTestPatternConfig` + per-layer parameters (CPU)** — **P9** v2.4.374
- [x] §3.5 — **Layer stack shader loop** (user-controlled compose order) — **P11 shipped** v2.4.378
- [x] §4 — All nine RGL pattern types ported to GLSL (**8 complete + 3D stub**)
- [x] §4.11 — Dispatcher (`sit_test_patterns.glslh`)
- [x] §5 — Compositor SMPTE consumes `sit_tp_smpte.glslh` (**Vulkan `#include`**; **GL §0.5-C mirror**)
- [ ] §5.3 — Pixel-identical gate (harness/manual readback pending)
- [ ] §6 — Example shader + CPU config mirror (**C API shipped**; `shader_lab_test_patterns.c` pending)
- [x] §7 — Harness readback gate (std140 UBO + SSBO; graphics **15/15** GL · **16/16** VK; full harness `--filter pattern` **17/17** GL)
- [ ] §8 — RGL shim / migration notes in `RGL_MIGRATION_PLAN.md` §7D.3 (optional)
- [x] §9 — Documentation — **`doc/guide/test_patterns.md`** (cross-links: `virtual_display.md`, `situation_api.md`)

---

## §0 — Inventory & gap analysis

**Context:** RGL draws test patterns by queuing CPU 2D primitives (`RGL_DrawRectangle`, `RGL_DrawLineEx`, …) inside `RGL_Begin`/`RGL_End`. Situation user code uses **fragment shaders** loaded via `SituationLoadShaderFromMemory` / SPIR-V embed. The migration replaces the CPU queue with a **procedural UV function** library.

### 0.1 — RGL surface area (source of truth: `doc/misc/rgl.h`)

| RGL symbol | Lines (approx.) | Shader equivalent |
|---|---|---|
| `RGLTestPatternType` (9 values) | 442–452 | `SIT_TP_*` enum in `sit_tp_config.glslh` |
| `RGLTestPatternColors` | 424–438 | `SitTpPalette` struct (normalized `vec3`/`vec4`) |
| `RGLTestPatternConfig` | 454–472 | `SitTpConfig` uniform / push-constant block |
| `RGL_GetDefaultTestPatternConfig` | 8856–8871 | `sit_tp_default_config(int type)` GLSL helper + C mirror |
| `RGL_DrawTestPattern` | 9296–9384 | `sit_tp_sample(vec2 uv, SitTpConfig cfg) → vec3` |
| `RGL_DEFAULT_TEST_COLORS` | 8839–8854 | `SIT_TP_DEFAULT_PALETTE` constant |
| `_RGL_DrawSmpteBars` | 8945–9069 | `sit_tp_smpte_bars(uv, cfg)` |
| `_RGL_DrawPluge` | 9072–9133 | `sit_tp_pluge(uv, cfg)` |
| `_RGL_DrawMultiburst` | 9166–9222 | `sit_tp_multiburst(uv, cfg)` |
| `_RGL_DrawCrosshatch` | 9135–9164 | `sit_tp_crosshatch(uv, cfg)` |
| `_RGL_Draw3DGrid` | 9239–9294 | `sit_tp_3d_grid(uv, cfg)` — **see §4.9** |
| `RGL_DrawGrid` | 6117–6127 | `sit_tp_grid(uv, spacing, offset, color, thickness)` |
| `RGL_DrawCheckerboard` | 6315–6333 | `sit_tp_checkerboard(uv, rect, tile_size, c1, c2)` |
| `RGL_DrawStripes` | 6344–6360 | `sit_tp_stripes(uv, rect, stripe_width, vertical, c1, c2)` |
| `RGL_DrawSafeArea` | (calibration module) | `sit_tp_safe_area(uv, overscan_pct, line_color, thickness)` |
| `RGL_DrawCrosshair` | (calibration module) | `sit_tp_crosshair(uv, center, size, thickness, color)` |
| `RGL_DrawRectangleGradient` | 6279–6291 | `sit_tp_rect_gradient(uv, rect, tl, tr, bl, br)` |
| `RGL_DrawRuler` / `RGL_DrawArrow` / `RGL_DrawLabeledRectangle` | 6137+ | **Out of scope v1** — no RGL test-pattern dispatcher calls them; defer to §9 optional |

### 0.2 — Situation existing overlap

| Location | What exists | Action |
|---|---|---|
| `sit/gpu/vd.frag` | Vulkan: `#include sit_tp_smpte.glslh` (`SIT_TP_SMPTE_VD_SUBSET`); OpenGL: inline mirror (§0.5-C) | **Done** — pixel gate §5.3 pending |
| `sit/gpu/composite.frag` | Same as `vd.frag` | **Done** — pixel gate §5.3 pending |
| `sit/gpu/test_patterns/` | 4 headers (config, colors, primitives, smpte VD subset) | **In progress** — dispatcher + remaining patterns |
| `tests/harness/test_graphics.c` | 4×4 CPU checkerboard texture | Keep; add shader-pattern readback tests separately |
| `doc/misc/RGL_MIGRATION_PLAN.md` §7D.3 | "low priority; CPU queue" | **Supersede** after §8 |

### 0.3 — Known RGL quirks to preserve or fix

- [ ] Document: `_RGL_DrawMultiburst` **ignores** `config->frequencies` / `num_frequencies` — uses hardcoded `{0.5,1,2,3,4,5}`. Shader port reads uniform array `cfg.frequencies[6]` with same default; fix noted in migration notes.
- [ ] Document: RGL text labels (`RGL_DrawText`) are **conditional** on debug font atlas. Shader port **omits text in v1**; optional CPU text overlay documented in §4.10.
- [ ] Document: RGL `Color` is 0–255 `uint8`; Situation VD SMPTE uses 0–1 `float` with **different bar constants** than RGL (e.g. VD top gray `0.706` ≈ 180/255 vs RGL `bar_light_gray` `192/255` ≈ `0.753`). §3.1 holds RGL calibration palette; §5.3 holds VD subset constants — do not conflate.
- [ ] Document: RGL `grid_white` uses **α=100**; procedural grid must composite with `100/255` alpha over background, not opaque white lines.

### 0.4 — Sign-off checklist

- [x] Verify nine `RGL_TESTPATTERN_*` enum values map 1:1 to `SIT_TP_*` constants (**order matters** — see §3.2; crosshatch=6, multiburst=7) — implemented in `sit_tp_config.glslh`
- [ ] Confirm default config fields: `width=640`, `height=480`, `checker_size=(32,32)`, `stripe_width=16`, `grid_size=5`, `show_overlay_circle=true` (SMPTE only) — matches `RGL_GetDefaultTestPatternConfig` — **C mirror not yet shipped**
- [x] Approve 3D grid strategy: **raymarched scene** in fragment shader (not mesh batch) — see §4.9; **P5 deferred**

### 0.5 — OpenGL core **delivery** strategy (§0.5 — not an authoring fork)

**Problem statement:** OpenGL built-in init cannot expand `#include` today. **Solution space is delivery only** — the authoritative SMPTE/pattern code remains in `sit_tp_smpte.glslh` / `sit_test_patterns.glslh`.

**Clarification:** Runtime `#include` **works on Vulkan** at `SituationInit()` and for user shaders. Vulkan P2 needs no special case beyond writing the header and adding `#include` to `vd.frag` / `composite.frag`.

| Path | Compile route | Consumes `.glslh` how |
|---|---|---|
| **Vulkan** built-ins | `_SituationVulkanCompileCoreShaderFile` → shaderc | **`#include` in stage file** |
| **Vulkan** user shaders | `SituationLoadShaderFromMemory` → shaderc | **`#include` in shader source** |
| **OpenGL** built-ins | `_SituationCreateGLCoreShaderProgram` → `glCompileShader` | §0.5 option below |
| **OpenGL** user / harness | `SituationLoadShaderFromSpirvMemory` or glslc embed | **Headers compiled in via glslc `-I`** (unchanged pattern) |

**P2 on Vulkan:** `#include "sit/gpu/test_patterns/sit_tp_smpte.glslh"` in compositor stages — **no library behavior change** if §5.3 passes.

**P2 on OpenGL built-ins** — pick **delivery** (math stays in header):

| Option | Delivery mechanism | Authoring | Library behavior change? |
|---|---|---|---|
| **A — shaderc/SPIR-V for GL core init** | Same as Vulkan at init | `.glslh` only | **Yes** — init path changes |
| **B — build-time flatten** | Script expands `#include` into `vd.frag` before init (or generates `vd_flat.frag`) | `.glslh` only | **No** if output matches today |
| **C — temporary GL inline mirror** | Keep inline SMPTE in GL `vd.frag` until A/B; **must sync from header** | `.glslh` + manual/regen sync | **No** pixels; **yes** maintenance debt |

- [x] **Decision recorded:** **Option C** — temporary GL inline SMPTE mirror synced from header; Vulkan uses `#include` (June 23, 2026)
- [ ] **A:** extend `_SituationCreateGLCoreShaderProgram` to shaderc→SPIR-V when available
- [ ] **B:** `scripts/flatten_glsl_includes.py` (or glslc `-E`) in build; document that flattened files are **generated**, not hand-edited
- [x] **C:** explicit debt item — inline GL copy is **not** source of truth; regenerate or diff-check against header — **active in `vd.frag` / `composite.frag` `#else` branches**

---

## §1 — Header file architecture

**Context:** Situation's runtime `#include` stack is **production on Vulkan** (see **Focus** above). `_SituationShaderIncluderResolve` searches CWD, `requesting_source` parent dir, `../` prefix fallbacks, and exe-relative paths (mirrors `_SituationLoadCoreShaderFile`). Test patterns are the **first shipped consumer** of shared `.glslh` headers.

**Verified build flags (`sit/Makefile`):**
- Vulkan DLL: `-DSITUATION_ENABLE_SHADER_COMPILER` + shaderc linked
- OpenGL DLL: **no** `SITUATION_ENABLE_SHADER_COMPILER` — see §0.5

### 1.1 — Directory layout (authoritative sources — edit here only)

Pattern logic is **never** authored inline in `vd.frag` except transient §0.5-C mirrors pending sync.

```
sit/gpu/test_patterns/
  sit_tp_config.glslh       # ✅ enums, SitTpConfig, SIT_TP_FLIP_Y, sit_tp_uv/px
  sit_tp_colors.glslh       # ✅ RGL_DEFAULT_TEST_COLORS → SitTpPalette
  sit_tp_primitives.glslh   # ✅ grid, checkerboard, stripes, safe area, crosshair, gradients
  sit_tp_smpte.glslh        # ✅ VD subset + full RGL SMPTE (sit_tp_smpte_bars)
  sit_tp_pluge.glslh        # ✅
  sit_tp_multiburst.glslh   # ✅ (reads cfg.frequencies when num_frequencies > 0)
  sit_tp_convergence.glslh  # ✅
  sit_tp_gradients.glslh    # ✅
  sit_tp_crosshatch.glslh   # ✅
  sit_tp_3d_grid.glslh      # ⏸ stub (P5 deferred)
  sit_test_patterns.glslh   # ✅ umbrella + sit_tp_sample()
```

### 1.2 — Public include for user shaders

User fragment shaders add:

```glsl
#include "sit/gpu/test_patterns/sit_test_patterns.glslh"

// In main():
vec2 uv = /* normalized 0..1, origin per SIT_TP_FLIP_Y macro */;
vec3 rgb = sit_tp_sample(uv, u_tpConfig, sit_tp_default_palette());
```

### 1.3 — Include path resolver (P0)

- [x] Extend `_SituationShaderIncluderResolve` to search, in order:
  1. `requested_source` as-is (CWD)
  2. Directory of `requesting_source` (relative includes)
  3. `../`, `../../`, `../../../`, `../../../../` prefixes on `requested_source`
  4. Executable-relative `SituationJoinPath(exe_base, relative_path)` (match core shader loader)
- [x] **Update `_SitVkShadercOptionsFingerprint`** — `fp ^= 0x2ULL` (include resolver v2)
- [ ] **Runtime include test (Vulkan):** compile via `SituationLoadShaderFromMemory` from CWD `examples/other/` (glslc smoke test from repo root passes; runtime CWD sweep pending)
- [ ] **Build-time include test (both backends):** glslc `-I` repo root in `compile_harness_shaders.ps1` — see §7.1
- [x] Document required CWD / `-I` convention in `doc/guide/test_patterns.md`

### 1.4 — GL / Vulkan parity macros

All headers must support both backends (mirror `vd.frag`):

```glsl
#ifndef SIT_TP_FLIP_Y
#if defined(SITUATION_USE_OPENGL)
#define SIT_TP_FLIP_Y 1   /* GL framebuffer origin bottom-left */
#else
#define SIT_TP_FLIP_Y 0   /* VK compositor path: no flip in pattern UV */
#endif
#endif

vec2 sit_tp_uv(vec2 raw_uv) {
#if SIT_TP_FLIP_Y
    return vec2(raw_uv.x, 1.0 - raw_uv.y);
#else
    return raw_uv;
#endif
}
```

- [x] `sit_tp_config.glslh` defines `SIT_TP_FLIP_Y` override hook
- [ ] Harness + lab shaders compile with both `--target-env=opengl` and `--target-env=vulkan` (glslc); runtime flip validated per §1.4 / §7.1

### 1.5 — `SitTpConfig` uniform / UBO layout (P0 design note)

Mixed `int` / `float` / `float[6]` in one block needs explicit **std140** (UBO) packing — alignment differs between Vulkan and SPIR-V layout profiles (`SIT_SPIRV_LAYOUT_PROFILE_*`).

**Decision (June 23, 2026):** Shared harness + C API use **one std140 UBO** at `set=0, binding=0` on **both** OpenGL and Vulkan (`SituationCmdBindDescriptorSet` + `SituationUpdateBuffer`). Pattern library headers stay binding-agnostic; binding lives in `sit_tp_config_ubo.glslh` / `sit_tp_smpte_vd_ubo.glslh`. P6 lab may optionally use Vulkan push constants for zero descriptor churn — separate from harness.

- [x] Define `SitTpConfigBlock` std140 GLSL block (`sit/gpu/test_patterns/sit_tp_config_ubo.glslh`)
- [x] Document `SIT_TP_CONFIG_UBO_SIZE` (144 B) and offset table in `sit/sit_test_pattern_config.h`
- [x] `SitTestPatternUploadConfigUbo` / `SitTestPatternDrawFullscreenUbo` — no loose SPIR-V uniforms or push/SSBO split per backend
- [x] OpenGL: `SituationBindUniformBlock(shader, "SitTpConfigBlock", 0)` after SPIR-V load; Vulkan: explicit `layout(set=0,binding=0)`

---

## §2 — Primitive layer (`sit_tp_primitives.glslh`)

**Context:** RGL calibration helpers become **signed-distance / analytic** UV functions. Normalized UV (0..1) for most entry points; pixel-space where noted. Functions that operate on pixel rects take an explicit `vec2 resolution` (implementation detail — required to convert UV → px correctly).

### 2.1 — Core helpers

| Function | RGL source | GLSL signature (as shipped) |
|---|---|---|
| Grid | `RGL_DrawGrid` | `float sit_tp_grid_line(vec2 uv, vec2 resolution, vec2 spacing, vec2 offset, float thickness)` |
| Checkerboard | `RGL_DrawCheckerboard` | `vec3 sit_tp_checkerboard(vec2 uv, vec2 resolution, vec4 rect_px, vec2 tile_px, vec3 c1, vec3 c2)` |
| Stripes | `RGL_DrawStripes` | `vec3 sit_tp_stripes(vec2 uv, vec2 resolution, vec4 rect_px, float stripe_w, bool vertical, vec3 c1, vec3 c2)` |
| Safe area | `RGL_DrawSafeArea` | `float sit_tp_safe_area_border(vec2 uv, vec2 resolution, float overscan, float thickness)` |
| Crosshair | `RGL_DrawCrosshair` | `float sit_tp_crosshair_mask(vec2 uv_px, vec2 center_px, float size, float thickness)` |
| Rect gradient | `RGL_DrawRectangleGradient` | `vec3 sit_tp_bilinear_gradient(vec2 uv, vec2 resolution, vec4 rect_px, vec3 tl, vec3 tr, vec3 bl, vec3 br)` |
| Filled rect | `RGL_DrawRectangle` | `float sit_tp_rect_mask(vec2 uv_px, vec4 rect_px)` |
| Circle outline | `RGL_DrawCircleOutline` (SMPTE overlay) | `float sit_tp_ring(vec2 uv_px, vec2 center, float radius, float thickness)` |

Also: `sit_tp_rect_outline_mask` (internal helper for safe-area border).

### 2.2 — Coordinate helpers

- [x] `vec2 sit_tp_px(vec2 uv, vec2 resolution)` — in `sit_tp_config.glslh`
- [x] `vec2 sit_tp_uv_from_px(vec2 px, vec2 resolution)` — in `sit_tp_primitives.glslh`
- [x] `vec4 sit_tp_content_area(vec2 resolution, float margin_x, float margin_y)` — in `sit_tp_primitives.glslh`

### 2.3 — Implementation tasks

- [x] Implement `sit_tp_primitives.glslh` with no external includes (self-contained math)
- [ ] Golden UV fixtures: 8 reference `(uv, resolution, cfg) → rgb` pairs checked offline via `scripts/spirv_shader_debug.py` or a tiny Python reimplementation of RGL layout math
- [x] Anti-aliasing: 1-pixel `fwidth`-based edge soften on grid lines, safe-area border, crosshair, and ring (RGL had hard pixel edges; shader version targets ±1/255)

---

## §3 — Palette & config (`sit_tp_config.glslh`, `sit_tp_colors.glslh`)

### 3.1 — Palette conversion (RGL 0–255 → shader 0–1)

| RGL field | RGB (8-bit) | Shader `vec3` |
|---|---|---|
| `bg_dark_gray` | (45, 45, 45) | `vec3(45.0/255.0)` |
| `grid_white` | (255, 255, 255, **α=100**) | `vec3(1.0)` × alpha 100/255 for compositing |
| `bar_light_gray` | (192, 192, 192) | `vec3(0.753)` |
| `bar_yellow` | (192, 192, 0) | `vec3(0.753, 0.753, 0.0)` |
| `bar_cyan` | (0, 192, 192) | `vec3(0.0, 0.753, 0.753)` |
| `bar_green` | (0, 192, 0) | `vec3(0.0, 0.753, 0.0)` |
| `bar_magenta` | (192, 0, 192) | `vec3(0.753, 0.0, 0.753)` |
| `bar_red` | (192, 0, 0) | `vec3(0.753, 0.0, 0.0)` |
| `bar_blue` | (0, 0, 192) | `vec3(0.0, 0.0, 0.753)` |
| `bar_black` | (0, 0, 0) | `vec3(0.0)` |
| `bar_white` | (255, 255, 255) | `vec3(1.0)` |
| `bar_mid_gray` | (128, 128, 128) | `vec3(0.502)` |
| `bar_dark_gray` | (64, 64, 64) | `vec3(0.251)` |
| `bar_orange` | (208, 132, 45) | `vec3(0.816, 0.518, 0.176)` |
| PLUGE −4 IRE | (10, 10, 10) | `vec3(10.0/255.0)` |
| PLUGE +4 IRE | (20, 20, 20) | `vec3(20.0/255.0)` |
| PLUGE +7.5 IRE | (30, 30, 30) | `vec3(30.0/255.0)` |

- [x] `sit_tp_colors.glslh` exports `SitTpPalette sit_tp_default_palette()` (RGL calibration values)
- [ ] VD SMPTE subset uses **separate constants** — see §5.3; **implemented** in header; **pixel-identical gate pending** at 1920×1080 (tolerance ΔRGB < 1/255)

### 3.2 — Layer index & bitmask (keys 0–8 toggle bits, not exclusive enum)

Source: `doc/misc/rgl.h` lines 442–451 (RGL preset order preserved as **layer index**).

**GLSL** (`sit_tp_config.glslh`):

```glsl
#define SIT_TP_LAYER_SMPTE        (1u << 0)
#define SIT_TP_LAYER_CHECKERBOARD (1u << 1)
/* … */
#define SIT_TP_SMPTE_BARS      0   /* layer index alias — same as RGL_TESTPATTERN_* order */
#define SIT_TP_CHECKERBOARD    1
/* … */
bool sit_tp_layer_on(uint layers, uint layer_index);  /* bitfieldExtract */
```

- [x] GLSL `SIT_TP_LAYER_*` bitmask + layer index aliases (keys 0–8 toggle bits, not exclusive enum)
- [x] C mirror `SitTestPatternConfig.pattern_layers` + `SitTestPatternToggleLayer()` (harness defaults use single-bit presets)

**Architecture correction (v2.4+):** RGL exposed nine convenience **presets** via `RGLTestPatternType`, but the Situation product model is **build-your-own VD fields** — layers compose via `pattern_layers` bitmask, `sit_tp_layer_on()`, `sit_tp_preset_layer()` (single bit), and `sit_tp_compose_layers()` (multi bit). Single-bit configs preserve RGL preset pixel parity; multi-bit stacks content layers then shared grid / SMPTE circle overlay.

### 3.3 — Config struct (mirrors `RGLTestPatternConfig` fields; `pattern_layers` not `pattern_type`)

```glsl
struct SitTpConfig {
    int   pattern_layers;      // bitmask of SIT_TP_LAYER_*
    float width;               // logical framebuffer width (px)
    float height;
    int   show_overlay_circle; // SMPTE circle overlay when SMPTE layer enabled
    vec2  checker_size;        // px
    float stripe_width;        // px
    float frequencies[6];      // MHz bands for multiburst
    int   num_frequencies;
    int   grid_size;           // 3D grid extent
    // Palette passed separately or embedded — see layout profile
};
```

- [x] Define `SIT_TP_CONFIG_UBO_SIZE` and document alignment for `SituationLoadShaderFromSpirvMemoryEx` layout profiles
- [x] C header `sit/sit_test_pattern_config.h` — `SitTestPatternConfig` + toggle helpers

### 3.4 — C API quick reference (authoritative user layer list)

**Header:** `sit/sit_test_pattern_config.h`  
**User guide mirror:** `doc/guide/test_patterns.md` § API quick reference (C) — must stay in sync with this section  
**UBO:** `SIT_TP_CONFIG_UBO_SIZE` = 144 bytes, std140 `@ set=0, binding=0` (`SitTpConfigBlock` in `sit_tp_config_ubo.glslh`)

#### Layer index → toggle bit

| Key | `SitTestPatternLayer` | C bitmask `SIT_TEST_PATTERN_LAYER_*` | Hex |
|-----|------------------------|--------------------------------------|-----|
| 0 | `SIT_TEST_PATTERN_SMPTE_BARS` | `SIT_TEST_PATTERN_LAYER_SMPTE` | `0x001` |
| 1 | `SIT_TEST_PATTERN_CHECKERBOARD` | `SIT_TEST_PATTERN_LAYER_CHECKERBOARD` | `0x002` |
| 2 | `SIT_TEST_PATTERN_CONVERGENCE` | `SIT_TEST_PATTERN_LAYER_CONVERGENCE` | `0x004` |
| 3 | `SIT_TEST_PATTERN_GRADIENTS` | `SIT_TEST_PATTERN_LAYER_GRADIENTS` | `0x008` |
| 4 | `SIT_TEST_PATTERN_GRID_ONLY` | `SIT_TEST_PATTERN_LAYER_GRID` | `0x010` |
| 5 | `SIT_TEST_PATTERN_PLUGE` | `SIT_TEST_PATTERN_LAYER_PLUGE` | `0x020` |
| 6 | `SIT_TEST_PATTERN_CROSSHATCH` | `SIT_TEST_PATTERN_LAYER_CROSSHATCH` | `0x040` |
| 7 | `SIT_TEST_PATTERN_MULTIBURST` | `SIT_TEST_PATTERN_LAYER_MULTIBURST` | `0x080` |
| 8 | `SIT_TEST_PATTERN_3D_GRID` | `SIT_TEST_PATTERN_LAYER_3D_GRID` | `0x100` |

GLSL bitmask names: `SIT_TP_LAYER_*` (same index order). Shader tests: `sit_tp_layer_on(layers, index)`.

#### `SitTestPatternConfig` fields

| Field | Affects layers |
|-------|----------------|
| `pattern_layers` | Which layers are on (OR of macros above) |
| `width`, `height` | All |
| `show_overlay_circle` | SMPTE when SMPTE bit set |
| `checker_size_x`, `checker_size_y` | Checkerboard |
| `stripe_width` | Convergence |
| `frequencies[6]`, `num_frequencies` | Multiburst |
| `grid_size` | 3D grid |

#### Inline helpers (shipped)

| Function | Purpose |
|----------|---------|
| `SitTestPatternLayerBit(layer)` | `1u << layer` |
| `SitTestPatternConfigInitDefaults(cfg, layer_index, w, h)` | Zero struct + one layer bit + RGL-like defaults |
| `SitTestPatternToggleLayer(cfg, layer, enabled)` | Set/clear one bit |
| `SitTestPatternSetSingleLayer(cfg, layer)` | Exactly one bit |
| `SitTestPatternPackConfigStd140(out, cfg)` | Pack 144-byte UBO blob |
| `SitTestPatternBindConfigResources(shader, false)` | GL: bind `SitTpConfigBlock` |
| `SitTestPatternUploadConfigUbo` / `SitTestPatternDrawFullscreenUbo` | Upload + draw (harness / lab / custom shader) |

#### Minimal C usage

```c
SitTestPatternConfig cfg;
SitTestPatternConfigInitDefaults(&cfg, SIT_TEST_PATTERN_SMPTE_BARS, 640.f, 480.f);
SitTestPatternToggleLayer(&cfg, SIT_TEST_PATTERN_CHECKERBOARD, true);

uint8_t ubo[SIT_TP_CONFIG_UBO_SIZE];
SitTestPatternPackConfigStd140(ubo, &cfg);
SitTestPatternDrawFullscreenUbo(cmd, shader, mesh, buf, ubo, sizeof(ubo));
```

**Delivery scope today:**

| Path | Reads `pattern_layers`? |
|------|-------------------------|
| Custom FS + `#include sit_test_patterns.glslh` + UBO | ✅ Yes |
| Harness (`test_graphics_patterns.c`) | ✅ Yes |
| Built-in VD idle compositor (`vd.frag` / `composite.frag`) | ✅ **PATTERN**: full `SitTestPatternConfig` via UBO; **COLORBURST**: SMPTE subset (§5.3) |

- [x] §3.4 table + helpers documented (this section)
- [x] `SituationSetVirtualDisplayPatternLayers()` / `SituationGetVirtualDisplayPatternLayers()`
- [x] `SituationSetVirtualDisplayPatternConfig()` / `SituationGetVirtualDisplayPatternConfig()` — compositor UBO (v2.4.344)
- [ ] P6 lab: keypress **0–8 toggles** layer bits per §3.4 — **optional dev lab**; user-facing exploration → **P14 §6.4**

---

### 3.5 — Layer stack (user-controlled compose order) — **P9–P11**

**Problem (June 2026):** `pattern_layers` is an **on/off bitmask only**. Multi-bit compose uses a **fixed pipeline** hardcoded in `sit_tp_compose_layers()` — not user-configurable z-order. Key indices **0–8** name layers; they are **not** draw order.

**Current fixed pipeline** (legacy default when stack is empty — must remain pixel-identical):

```
3D_CUBE (exclusive full-frame when sole layer — one lit cube, §4.9)
  → bg_dark_gray
  → CHECKER / CONVERGENCE / GRADIENTS (replace — last in code wins)
  → PLUGE / MULTIBURST / CROSSHATCH / SMPTE (*_on blend onto base)
  → GRID overlay
  → SMPTE circle (show_overlay_circle)
```

**Decision (June 27, 2026):** **Option A — explicit layer stack** (bottom → top draw list). Keep `pattern_layers` as enable mask; add stack fields to config. **Library always initializes a default stack** (legacy draw order as data — see §3.5.1). **Per-layer parameters are first-class** — see **§3.6** (this is the main struct regroup).

#### 3.5.1 — Data model

| Field | Type | Role |
|-------|------|------|
| `pattern_layers` | `int32` bitmask | Which layers are **enabled** (unchanged; bits 0–8 + flag bit 16 chroma snow) |
| `layer_stack_count` | `int32` | Active stack length (**default 8** — full legacy compose order; see below) |
| `layer_stack[9]` | `uint8` × 9 | Draw order bottom → top; each byte = layer index **0–8** |

**Default stack** (initialized by `SitTestPatternConfigInitDefaults` — always populated, not “empty means legacy”):

```
{ 1, 2, 3, 5, 7, 6, 0, 4, 0xFF }   /* Checker → … → Grid; slot 8 unused */
 count = 8
```

Shader **always** iterates the stack; legacy hardcoded `if` chain is removed once P9 lands. Single-bit preset path still uses full RGL wrappers until migrated to stack + per-layer params.

**Semantics:**

- **Enable mask** and **stack** are orthogonal: a layer must be **on in the bitmask** to draw even if listed in the stack (shader skips disabled layers).
- **Toggle on:** append layer index to stack (if not already present) — natural lab behavior for keys 0–8.
- **Toggle off:** remove layer from stack; clear its bit in `pattern_layers`.
- **Explicit reorder:** `SitTestPatternSetLayerOrder` / `SituationSetVirtualDisplayPatternLayerOrder` replaces stack without changing enable bits.
- **Live edit:** mutate any field in `SitTestPatternConfig` (stack, params, palette) and call `SituationSetVirtualDisplayPatternConfig` — compositor re-uploads on next idle frame.
- **Zero calibration layers** → snow path; uses **`params.snow`** (§3.6); stack ignored.

#### 3.5.2 — Per-layer apply mode (`sit_tp_apply_layer`)

Refactor compose into one dispatcher used by the stack loop:

| Layer index | Apply mode | Function |
|-------------|------------|----------|
| 0 SMPTE | **Blend** | `sit_tp_smpte_bars_on(base, …)` |
| 1 Checker | **Replace** | `sit_tp_pattern_checkerboard` |
| 2 Convergence | **Replace** | `sit_tp_convergence` |
| 3 Gradients | **Replace** | `sit_tp_gradients` |
| 4 Grid | **Blend** | `sit_tp_blend_grid(base, …)` |
| 5 PLUGE | **Blend** | `sit_tp_pluge_on` |
| 6 Crosshatch | **Blend** | `sit_tp_crosshatch_on` |
| 7 Multiburst | **Blend** | `sit_tp_multiburst_on` |
| 8 3D cube | **Replace** | `sit_tp_cube` — **one** lit cube, fixed canonical view (§4.9) |

Post-loop: SMPTE **overlay circle** when `show_overlay_circle != 0` and SMPTE layer enabled (same as today — always after stack, not a stack slot).

Legacy `sit_tp_compose_layers()` is **deleted** after P9; default stack reproduces today’s pixels when the same layers are enabled.

#### 3.5.3 — UBO header (stack + frame) — binding 0

First UBO block (**`SitTpConfigHeaderBlock`**, binding 0): frame, enable mask, stack, snow seed. Per-layer tunables move to **§3.6 SSBO** (binding 1) — the 144 B flat layout is **retired**, not extended ad hoc.

| Offset | Field | Notes |
|--------|-------|-------|
| 0 | `pattern_layers` | enable mask + flags |
| 4–12 | `width`, `height`, `show_overlay_circle` | frame |
| 16–136 | *(snow seed @ 136)* | `noise_frame_seed` — compositor may overwrite live |
| 140 | `layer_stack_count` | default **8** |
| 144 | `layer_stack_packed` | `uvec4` byte unpack (9 indices) |

**Size:** **160 B** binding 0. Full parameter payload is **not** squeezed into this block.

#### 3.5.4 — C / public API (P11)

| API | Phase | Purpose |
|-----|-------|---------|
| `SitTestPatternSetLayerOrder(cfg, stack, count)` | P11 | Replace stack |
| `SitTestPatternAppendLayer` / `RemoveLayer` | P11 | Toggle + stack sync |
| `SituationSetVirtualDisplayPatternLayerOrder(vd, …)` | P11 | VD standby stack |

Layer **parameter** getters/setters: **§3.6.5** (`SitTestPatternGet/SetLayerParams`).

#### 3.5.5 — Shader files (P9 stack loop)

| File | Change |
|------|--------|
| `sit_tp_compose.glslh` *(new)* | `sit_tp_apply_layer`, stack loop |
| `sit_test_patterns.glslh` | Multi-bit → stack compose; reads **§3.6** param SSBO |
| `sit_tp_config_ubo.glslh` | Header block only @ binding 0 |

#### 3.5.6 — Verification (P9 / P11 / P14)

| Test / demo | Assert |
|------|--------|
| `pattern_compose_checker_plus_smpte` | Default stack + default params → same pixels as today |
| `pattern_compose_order_smpte_under_checker` | Reordered stack readback |
| `pattern_layer_params_checker_tile` *(§3.6)* | Live `checker.tile_size` change visible |
| **`25_vd_standby`** *(P14 §6.4)* | Manual: snow, layer toggles, stack `[`/`]`, param nudges on live VD |

- [x] §3.5 stack design signed off
- [x] P9 — default stack init + stack loop shader
- [x] P11 — stack C/VD API + compose harness
- [x] P14 — **`25_vd_standby`** interactive example (replaces P12 lab goals)

**User guide mirror:** `doc/guide/test_patterns.md` — “Layer stack” + “Per-layer parameters” → §3.5 / §3.6.

---

### 3.6 — Unified `SitTestPatternConfig` + **per-layer parameters** — **P9–P12**

**Problem:** Today `SitVdStandbyConfig` is a **flat** struct — `checker_size` and `frequencies[]` live at the top level even though they only affect one layer. Most layers use **hardcoded** shader constants (grid spacing 32, crosshatch 16×12, PLUGE margins, gradient corner colors from palette only). There is **no** public `sit/sit_test_pattern_config.h`; harness duplicates pack helpers.

**Decision (June 27, 2026):** One authoritative CPU struct — **`SitTestPatternConfig`** — regroups **stack + frame + palette + per-layer parameter blocks**. Every layer index **0–8** (plus snow) has its own param sub-struct with **RGL defaults** filled in by `SitTestPatternConfigInitDefaults`. Live testing edits any sub-struct and re-uploads.

#### 3.6.1 — Canonical C struct (authoritative)

Ship as **`sit/sit_test_pattern_config.h`**. **`SitVdStandbyConfig`** becomes a typedef alias (or deprecated name) for the same layout.

```c
typedef struct SitTestPatternConfig {
    /* --- composition --- */
    int32_t  pattern_layers;
    uint8_t  layer_stack[9];
    uint8_t  layer_stack_count;

    /* --- frame (shared) --- */
    float    width;
    float    height;

    /* --- shared palette (all layers may reference) --- */
    SitTestPatternPalette palette;

    /* --- one param block per layer TYPE (index 0–8) --- */
    struct {
        SitTpParamsSmpte       smpte;        /* 0 */
        SitTpParamsChecker     checker;      /* 1 */
        SitTpParamsConvergence convergence;  /* 2 */
        SitTpParamsGradients   gradients;    /* 3 */
        SitTpParamsGrid        grid;         /* 4 */
        SitTpParamsPluge       pluge;        /* 5 */
        SitTpParamsCrosshatch  crosshatch;   /* 6 */
        SitTpParamsMultiburst  multiburst;   /* 7 */
        SitTpParamsCube         cube;          /* 8 — one 3D cube, not a scene */
    } layer;

    /* --- snow (no layer bit; zero calibration mask) --- */
    SitTpParamsSnow snow;
} SitTestPatternConfig;
```

VD slot: `standby_pattern` holds this struct. **`fallback_mode` / `fallback_color`** stay on the VD object (SOLID / COLORBURST — not part of pattern compositor).

#### 3.6.2 — Per-layer parameter catalog

Each sub-struct holds **everything** that layer’s shader reads — migrate hardcoded literals into fields with RGL defaults.

| Layer | C type | Parameters (initial set) | Today |
|-------|--------|---------------------------|--------|
| **0 SMPTE** | `SitTpParamsSmpte` | `content_margin_x/y`, `show_overlay_circle`, `overlay_circle_radius` (optional) | margin 0.125/0.2 hardcoded; circle flag in flat struct |
| **1 Checker** | `SitTpParamsChecker` | `tile_size_x/y`, `color_a`, `color_b` (RGBA or palette indices) | tile in flat struct; colors from palette |
| **2 Convergence** | `SitTpParamsConvergence` | `stripe_width`, `central_inset_x/y`, `central_size_w/h`, `color_a`, `color_b` | stripe in flat struct; central 25%/50% hardcoded |
| **3 Gradients** | `SitTpParamsGradients` | `quad_colors[4][4]` (four corners × four quadrants) or 16× `ColorRGBA` | palette corners hardcoded in shader |
| **4 Grid** | `SitTpParamsGrid` | `spacing_px` (or `divisions`), `line_alpha`, `line_color` | spacing `width/32`, α=100/255 hardcoded |
| **5 PLUGE** | `SitTpParamsPluge` | `safe_margin`, `bar_count`, `bar_height_frac`, pluge/center colors | layout hardcoded |
| **6 Crosshatch** | `SitTpParamsCrosshatch` | `grid_nx`, `grid_ny`, `crosshair_size`, `crosshair_thickness`, `safe_margin` | 16×12, 20, 2 hardcoded |
| **7 Multiburst** | `SitTpParamsMultiburst` | `frequencies[6]`, `num_frequencies`, `safe_margin`, stripe colors | frequencies in flat struct |
| **8 Cube** | `SitTpParamsCube` | `size`, `diffuse`, `ambient` (lit faces); **no camera** — canonical view baked in shader | **Misimplemented** as grid scene + `grid_size`; see §4.9 |
| **Snow** | `SitTpParamsSnow` | `noise_frame_seed`, `chroma` (bool / flag) | seed live; chroma bit 16 |

**Shader rule:** `sit_tp_apply_layer(index, base, uv, header, layer_params, palette)` reads **`config.layer.<name>`** for that index — never global flat `cfg.checker_size`.

#### 3.6.3 — GPU delivery (header UBO + params SSBO)

std140 UBO cannot hold 16 gradient colors + full palette + nine layer blocks. **Two binding model:**

| Binding | Block | Contents |
|---------|-------|----------|
| **0** | `SitTpConfigHeaderBlock` | `pattern_layers`, frame, stack, `noise_frame_seed` (160 B) |
| **1** | `SitTpLayerParamsBlock` (**std430 SSBO**) | Mirror of `SitTestPatternConfig.layer` + `snow` + optional inline palette |

```glsl
layout(std430, binding = 1) readonly buffer SitTpLayerParamsBlock {
    SitTpParamsSmpte       smpte;
    SitTpParamsChecker     checker;
    /* … all nine + snow … */
    SitTestPatternPalette  palette;
} u_sit_tp_layers;
```

**Why SSBO:** variable-sized friendly, live `UpdateBuffer` whole struct or sub-range for lab sliders, no std140 stride waste. Custom shaders `#include` the same layouts from `sit_tp_layer_params.glslh`.

**Backward compat (one release):** P9 may ship a **flatten mapper** (`SitTestPatternPackLegacyFlat144`) so old harness paths stay green until SSBO bind lands in P10 — then remove flat pack.

#### 3.6.4 — Init defaults

`SitTestPatternConfigInitDefaults(cfg, layer_index, w, h)` must:

1. Zero struct; set `width`/`height`.
2. Populate **default stack** (§3.5.1).
3. Call **`SitTestPatternInitLayerParamsDefaults(&cfg->layer)`** — every sub-struct gets RGL-aligned defaults (even layers not enabled).
4. If `layer_index` 0–8: set single enable bit; else zero layers (snow).
5. Init **`palette`** from `SitTestPatternPaletteRglDefault()`.

Enabling a layer via toggle **does not zero** its param block — params persist while off (good for lab).

#### 3.6.5 — C API (live edit)

| API | Purpose |
|-----|---------|
| `SitTestPatternConfigInitDefaults(cfg, layer, w, h)` | Full struct + all layer defaults |
| `SitTestPatternGetLayerParams(cfg, layer, out)` | Typed read (returns union/view) |
| `SitTestPatternSetLayerParams(cfg, layer, const void* params)` | Typed write |
| `SituationSetVirtualDisplayPatternConfig(vd, cfg)` | **Already exists** — copies whole struct once expanded |
| `SituationGetVirtualDisplayPatternConfig(vd, out)` | Read back for lab UI |

Optional sugar (P12): `SituationSetVirtualDisplayCheckerSize(vd, x, y)` → mutates `cfg.layer.checker` + upload — avoid for v1; prefer whole-config or generic layer setter.

#### 3.6.6 — Shader / header files (P10)

| File | Role |
|------|------|
| `sit/sit_test_pattern_config.h` | C struct + init/pack/upload helpers |
| `sit/gpu/test_patterns/sit_tp_layer_params.glslh` | GLSL mirrors of each `SitTpParams*` (std430) |
| `sit/gpu/test_patterns/sit_tp_layer_params_ssbo.glslh` | SSBO block + `sit_tp_layer_params_fetch(index)` |
| `sit/gpu/test_patterns/sit_tp_*.glslh` | Refactor signatures to take typed param struct, not flat `SitTpConfig` |

Remove duplicated pack logic from `tests/harness/sit_harness_pattern_ubo.h` — harness includes library header.

#### 3.6.7 — Verification

| Test | Proves |
|------|--------|
| `pattern_layer_params_checker_tile` | `checker.tile_size` 8 vs 32 readback differs |
| `pattern_layer_params_convergence_stripe` | `stripe_width` change visible |
| `pattern_layer_params_multiburst_freq` | Custom `frequencies[]` vs default |
| `pattern_compose_checker_plus_smpte` | Default params + stack → legacy pixels |

- [ ] §3.6 per-layer param design signed off
- [ ] P9 — `sit_test_pattern_config.h` + all `SitTpParams*` types + init defaults (CPU only)
- [x] P10 — SSBO + shader refactor (each layer reads its param block) — v2.4.378
- [x] P11 — stack API + VD upload binds header + SSBO — v2.4.378
- [x] P14 — **`25_vd_standby`**: live keys for every layer param group (§6.4.3)

**Migration note:** Flat fields in current `SitVdStandbyConfig` (`checker_size_x`, `frequencies[]`, …) map 1:1 into `layer.*` for one release via inline accessors or deprecated field aliases — then remove flat fields in v2.5.

---

## §4 — Pattern implementations (RGL → GLSL)

Each pattern module exports `vec3 sit_tp_<name>(vec2 uv, SitTpConfig cfg, SitTpPalette pal)`.

### 4.1 — `SIT_TP_SMPTE_BARS` (`sit_tp_smpte.glslh`)

**RGL reference:** `_RGL_DrawSmpteBars` + grid background + optional overlay circle.

Layout (fractions of `content_area` = 75%×60% centered):

| Region | Fraction of content_area | Content |
|---|---|---|
| Top 7 bars | height × 0.45 | gray, yellow, cyan, green, magenta, red, blue |
| Middle 7 bars | height × 0.15 | mid-gray, black×5, mid-gray |
| Freq + PLUGE band | height × 0.20 | white bg; PLUGE triplets L/R; 12-stripe bursts; gray/orange refs; triangle |
| Bottom gradient | height × 0.20 | magenta→black / black→blue bilinear; dark-gray + black bars |
| Safe area | full screen | 10% overscan outline |
| Overlay circle | optional | radius = content_height/2 |

- [x] Port full layout (RGL `_RGL_DrawSmpteBars`) — `sit_tp_smpte_bars()` in `#else` branch
- [x] Add `sit_tp_smpte_bars(uv, cfg, pal)` for calibration / dispatcher use
- [x] Add VD subset mode: `#define SIT_TP_SMPTE_VD_SUBSET 1` exposes `_sit_smpte_color_bars(vec2 uv)` for compositor drop-in
- [x] Background: `bg_dark_gray` + 32×32 grid in `sit_tp_smpte_bars` (dispatcher path)

### 4.2 — `SIT_TP_CHECKERBOARD` (`sit_tp_pattern_checkerboard` in dispatcher)

- [x] Tile size from `cfg.checker_size`; colors `bar_white` / `bar_black`
- [x] No text label in shader v1

### 4.3 — `SIT_TP_CONVERGENCE` (`sit_tp_convergence.glslh`)

- [x] Full-screen vertical stripes (`stripe_width`, white/black)
- [x] Central 50%×50% horizontal stripes (RGL `central_rect` at 25% inset)

### 4.4 — `SIT_TP_GRADIENTS` (`sit_tp_gradients.glslh`)

- [x] Four quadrants with corner colors per RGL `RGL_DrawRectangleGradient` calls

### 4.5 — `SIT_TP_GRID_ONLY`

- [x] `bg_dark_gray` fill + grid spacing `width/32` (`sit_tp_pattern_grid_only`)

### 4.6 — `SIT_TP_PLUGE` (`sit_tp_pluge.glslh`)

- [x] 10-bar layout: 4 PLUGE pairs (−4/0/+4/+7.5 IRE) + 3 center bars (mid-gray, white, dark-gray)
- [x] Safe area 10% + 32×32 grid overlay
- [x] Text labels deferred (§4.10)

### 4.7 — `SIT_TP_MULTIBURST` (`sit_tp_multiburst.glslh`)

- [x] 6 bands, default MHz `{0.5, 1, 2, 3, 4, 5}`
- [x] Stripe width formula: `10.0 / (freq + 0.5)` px (port RGL line 9187)
- [x] Read `cfg.frequencies[]` when `num_frequencies > 0` (**fix** RGL hardcode)
- [x] Safe area + grid overlay

### 4.8 — `SIT_TP_CROSSHATCH` (`sit_tp_crosshatch.glslh`)

- [x] 16×12 line grid (`nx=16`, `ny=12`)
- [x] Center crosshair: size 20 px, thickness 2 px
- [x] Safe area 10%

### 4.9 — Layer 8: **one 3D cube** (`sit_tp_cube.glslh`) — **not a 3D scene**

#### History / intent (corrected June 2026)

Broadcast test patterns (layers **0–7**) are **2D calibration cards** on a flat field — SMPTE, PLUGE, multiburst, etc.

Layer **8** is the **only** layer that exercises **3D presentation**, and its purpose is narrow:

> Show **exactly one lit cube** so an operator can verify the **3D path** (projection, depth, face normals, basic lighting) on a monitor feed — the same way other layers verify color, geometry, or frequency response in 2D.

It is **not**:

- A general 3D scene or mini engine inside the test-pattern library  
- A grid of cubes, floor grid, or RGB axis gizmo  
- A configurable camera rig (`camera_position` is **out of scope** for test patterns)

**RGL drift (do not treat as spec):** `doc/misc/rgl.h` `_RGL_Draw3DGrid` (June-era) draws a **5×5 cube field**, floor lines, and RGB axes — that was a **misinterpretation / demo expansion**, not the original test-pattern intent. Situation **P5** raymarched that drift in `sit_tp_3d_grid.glslh`. **That implementation is wrong** and must be replaced.

**Naming:** Logical name **CUBE** / `SIT_TP_LAYER_CUBE`. C symbols may remain `SIT_TP_3D_GRID` / bit 8 until a breaking rename; docs and new code use **cube**.

#### Correct behavior

| Aspect | Spec |
|--------|------|
| Geometry | **One** axis-aligned cube, centered (world origin or fixed offset) |
| View | **Fixed canonical camera** baked into shader (or fixed compositor draw) — **not** a user param |
| Lighting | Simple diffuse + ambient (match RGL `RGLMaterial` intent: verify faces read differently) |
| Tunables | `size`, `diffuse` color, `ambient` — optional per-face tints later; **no** `grid_size`, **no** camera |
| Compositing | When layer 8 is the sole calibration layer: full-frame **replace** (like today’s early-out). When stacked (future): treat as replace or blend per §3.5 apply mode — TBD in P11 |
| Delivery | Prefer **minimal single-box SDF** in `sit_tp_cube.glslh` (fullscreen procedural VD path) **or** compositor special-case: idle + cube layer → one `DrawCube` into VD (uses real 3D pipeline). **Do not** grow a scene graph in `.glslh` |

#### Implementation status

- [x] ~~Raymarched grid scene~~ — `sit_tp_3d_grid.glslh` (**incorrect — retire**)
- [x] **`sit_tp_cube.glslh`** — one box, fixed view, RGL-lit-cube intent
- [x] Remove `grid_size` default 5 → **1** (cube edge); field name legacy until P9 struct
- [x] Harness: **`pattern_cube_lit_faces`** replaces `pattern_3d_grid_axis_red`
- [ ] Optional: fix `_RGL_Draw3DGrid` in RGL to one cube (§8 shim / separate RGL note)

**Risk (old):** Raymarch grid parity was subjective. **New scope:** single cube is bounded; fixed camera removes tuning surface.

### 4.10 — Text labels (RGL `RGL_DrawText` parity)

| RGL label | Pattern |
|---|---|
| "SMPTE Color Bars" | SMPTE |
| "Checkerboard" | CHECKERBOARD |
| "Convergence Test" | CONVERGENCE |
| "Gradient Test" | GRADIENTS |
| "Grid Overlay" | GRID_ONLY |
| "PLUGE Pattern" + IRE labels | PLUGE |
| "Multiburst Pattern" + MHz labels | MULTIBURST |
| "Crosshatch Pattern" | CROSSHATCH |
| "Cube Pattern" (was "3D Grid Pattern") | CUBE (layer 8) |

- [ ] **v1 decision:** Labels **not rendered in shader** (no bitmap font in include)
- [ ] Document optional overlay: draw labels with Situation debug text API **after** fullscreen pattern pass (example in §7)
- [ ] **v2 optional:** `sit_tp_labels.glslh` with precomputed 8×8 bitmap font atlas uniform (deferred)

### 4.11 — Compositor (`sit_test_patterns.glslh`)

```glsl
vec3 sit_tp_sample(vec2 uv, SitTpConfig cfg, SitTpPalette pal) {
    /* single-bit → RGL preset parity; multi-bit → sit_tp_compose_layers() */
}
```

- [x] `sit_tp_sample` entry point (RGL `RGL_DrawTestPattern` equivalent)
- [x] **Layer toggles:** `pattern_layers` bitmask + `sit_tp_compose_layers()` — keys 0–8 flip bits, not exclusive enum
- [x] `*_on(vec3 base, …)` content variants for PLUGE, multiburst, crosshatch, SMPTE (multi-layer stack)
- [x] Harness `pattern_compose_checker_plus_smpte` — multi-layer readback gate (**legacy fixed order**)
- [x] **P11:** Multi-bit compose uses `sit_tp_compose_stack()` — §3.5 shipped v2.4.378

---

## §5 — Consolidate existing Situation SMPTE (`vd.frag`, `composite.frag`)

**Context:** Duplicated `_sit_smpte_*` in two compositor stages. **Extract to `sit_tp_smpte.glslh`** — do not maintain a second copy of the math in stage files.

**Behavior:** Pixel-identical VD idle SMPTE (§5.3). Refactor only — not a feature change.

**Vulkan:** `#include` the header in `vd.frag` / `composite.frag`.

**OpenGL built-ins:** use §0.5 delivery (flatten generated stage, or temporary synced mirror — header remains authoritative).

### 5.1 — Refactor steps

- [x] Implement SMPTE subset in `sit/gpu/test_patterns/sit_tp_smpte.glslh` (**single source of truth**)
- [x] Add `#define SIT_TP_SMPTE_VD_SUBSET 1` mode: `_sit_smpte_color_bars(vec2 uv)` for compositor drop-in
- [x] **Vulkan** — replace inline bodies in `vd.frag` / `composite.frag` with:
  ```glsl
  #define SIT_TP_SMPTE_VD_SUBSET 1
  #include "sit/gpu/test_patterns/sit_tp_smpte.glslh"
  ```
- [x] **OpenGL built-ins** — §0.5-C inline mirror in `#else` branches (sync debt — not long-term design)
- [ ] Verify `SITUATION_VD_FALLBACK_COLORBURST` pixel-identical ±1/255 (harness or manual)
- [ ] Run `sit_test_vulkan --module virtual_display` and OpenGL equivalent

### 5.2 — Full SMPTE vs VD subset

| Mode | Used by | Content |
|---|---|---|
| `SIT_TP_SMPTE_VD_SUBSET` | `vd.frag`, `composite.frag` | 7 top + castellation + bottom (current behavior) |
| `SIT_TP_SMPTE_FULL` | User calibration shaders | Complete RGL `_RGL_DrawSmpteBars` layout |

- [x] Both modes live in one header; select via `SIT_TP_SMPTE_VD_SUBSET` macro (full RGL branch stubbed in `#else`)

### 5.3 — VD subset palette constants (not RGL calibration values)

Existing `vd.frag` / `composite.frag` use **VD-specific** floats. Shared header **must preserve these** in subset mode — do not substitute §3.1 RGL `bar_light_gray` (`0.753`):

| VD helper | Key constant | Notes |
|---|---|---|
| `_sit_smpte75` bar 0 | `0.706` | ≠ RGL light gray `192/255` |
| `_sit_smpte_castellation` | `0.063`, `0.706`, … | Castellation row |
| `_sit_smpte_bottom` | `0.063`, `0.922`, … | Bottom gradient band |

- [x] `#ifdef SIT_TP_SMPTE_VD_SUBSET` branch in `sit_tp_smpte.glslh` hardcodes current VD literals
- [ ] `#else` branch uses `SitTpPalette` from §3.1 for full RGL parity — **stub only**
- [ ] Dedicated harness or VD module readback asserts subset pixels unchanged pre/post refactor

---

## §6 — Example shader & CPU config mirror

### 6.1 — Example: `examples/other/shader_lab_test_patterns.c`

**Loading strategy per backend** (existing `shader_lab_*.c` examples embed GLSL as C strings — they do not use runtime `#include`):

| Backend | Recommended load path |
|---|---|
| **Vulkan** | `SituationLoadShaderFromMemory` with `#include` in source (shaderc) **or** SPIR-V embed from `build/compile_test_pattern_shaders.bat` |
| **OpenGL DLL** | SPIR-V embed via `SituationLoadShaderFromSpirvMemory` (`GL_ARB_gl_spirv`) **or** embed **flattened** GLSL strings (no `#include` in C literals) |
| **OpenGL static + compiler** | Same as Vulkan if build enables `SITUATION_ENABLE_SHADER_COMPILER` |

- [ ] Fullscreen triangle VS (reuse `shader_lab_torus.c` / `sit/gpu/quad.vert` pattern)
- [ ] FS (authoring form — flattened or SPIR-V for shipping):
  ```glsl
  #version 450 core
  #include "sit/gpu/test_patterns/sit_test_patterns.glslh"
  layout(location = 0) in vec2 v_uv;
  layout(location = 0) out vec4 outColor;
  // Uniform binding: use push constants or std140 UBO per §1.5 — not raw struct at location=3 on Vulkan
  uniform vec2 u_resolution;
  // SitTpConfig via agreed layout (push/UBO)
  void main() {
      vec3 rgb = sit_tp_sample(v_uv, u_tpConfig, sit_tp_default_palette());
      outColor = vec4(rgb, 1.0);
  }
  ```
- [ ] Keypress **0–8 toggles** layer bits per §3.4 (not cycle exclusive enum)
- [ ] Add to `build/build_examples.bat` (mirror `shader_lab_torus`)
- [ ] Add entry to `examples/other/CMakeLists.txt` if applicable

### 6.2 — C config mirror (`sit/sit_test_pattern_config.h`)

**Authoritative spec:** §3.4 (layer indices) + **§3.6** (unified struct + per-layer params).

- [ ] **`SitTestPatternConfig`** — full struct per §3.6.1 (replaces flat `SitVdStandbyConfig` layout)
- [ ] **`SitTpParams*`** sub-structs — one per layer with RGL defaults (§3.6.2)
- [ ] `SitTestPatternConfigInitDefaults` — frame + **default stack** + **all layer param defaults**
- [ ] `SitTestPatternPackHeaderStd140` + `SitTestPatternPackLayerParamsSsbo` — dual upload (§3.6.3)
- [ ] Remove duplicate helpers from `tests/harness/sit_harness_pattern_ubo.h`
- [x] ~~Flat~~ `SitTestPatternUploadConfigUbo` — **to be replaced** by header + SSBO upload (P10)

### 6.3 — Build script (embed recommended for OpenGL parity)

- [ ] `build/compile_test_pattern_shaders.bat` (+ `.ps1`) — glslc with `-I sit/gpu` → OpenGL + Vulkan SPIR-V → embed C arrays (mirror `compile_demon_hunt_shaders.bat` / `compile_harness_shaders.ps1`)
- [ ] Optional: runtime shaderc compile with includes for Vulkan-only fast iteration

### 6.4 — Example: `examples/25_vd_standby` (**P14 — planned, realization next**)

**Purpose:** Canonical **user-runnable** exploration of the full **`SitVdStandbyConfig`** surface on a live Virtual Display — from **zero-layer animated snow** (noise seed + chroma) through **all nine calibration layers**, **stack compose order**, and **per-layer parameter blocks** — with **real-time HUD edits** and immediate compositor feedback.

**Tagline:** *Import calibration. Not draw calls.* — this example makes that tangible without writing shaders or packing UBO bytes manually.

**Supersedes for users:** P6 `shader_lab_test_patterns.c` (optional dev-only fullscreen pass in `examples/other/`) and the idle-only slice of `vd_idle_standby_demo.c`. **`25_vd_standby`** is the digestible entry point referenced from `doc/guide/test_patterns.md` and `examples/README.md`.

#### 6.4.1 — Architecture

| Concern | Choice |
|---------|--------|
| **Integration path** | **Production VD idle compositor** — `SITUATION_VD_FALLBACK_PATTERN` + `SituationSetVirtualDisplayPatternConfig()` (not a custom fullscreen shader) |
| **Scaffolding** | `examples/shared/sit_example.h` — 1024×768 host window, universal hotkeys, HUD font |
| **VD layout** | One primary VD (recommended **1280×720** or match host aspect); **integer-scaled FIT** on black letterbox (same visual language as **05**) |
| **Idle behaviour** | **Never write live content** to the VD (or **SPACE** toggles a trivial live quad so users can see idle threshold → standby handoff). Default: `idle_threshold = 0` so pattern is **always visible** on first frame |
| **Upload path** | App mutates `SitVdStandbyConfig` in RAM; on dirty flag calls `SituationSetVirtualDisplayPatternConfig(vd, &cfg)` — compositor re-packs **160 B header + 832 B SSBO** internally |
| **Labels** | CPU text HUD only (§4.10 — no shader bitmap font in v1) |
| **Backends** | OpenGL + Vulkan via `build\build_examples.bat static-opengl 25_vd_standby` |

#### 6.4.2 — Exploration surfaces (must all be reachable in one session)

| Section | What the user explores | API / fields |
|---------|------------------------|--------------|
| **A — Snow (no signal)** | Animated B&W snow; optional **chroma** RGB snow | `pattern_layers = 0`, `snow.noise_frame_seed` (auto-advance each frame), `snow.chroma` / bit **16** |
| **B — Layer mask** | Toggle calibration layers **0–8** independently (multi-bit compose) | `SituationVdStandbyToggleLayer`, `pattern_layers` bitmask |
| **C — Stack order** | Bottom → top compose order; compare e.g. checker-under-SMPTE vs reversed | `layer_stack[]`, `layer_stack_count`, `SituationVdStandbySetLayerOrder` |
| **D — Active layer params** | Context panel edits the **selected layer's** param sub-struct (§3.6.2) | `cfg.layer.smpte`, `.checker`, `.convergence`, … `.cube` |
| **E — Fallback modes** | Contrast **PATTERN** vs legacy **COLORBURST** vs **SOLID** | `SituationSetVirtualDisplayFallbackMode`, `SituationSetVirtualDisplayFallbackColor` |
| **F — Presets** | One-key “test cards” (SMPTE only, PLUGE+grid, full stack default) | `SituationVdStandbyConfigInitDefaults` + toggles |

#### 6.4.3 — Input map (v1 — keyboard; mouse sliders deferred)

**Global**

| Key | Action |
|-----|--------|
| `0`–`8` | Toggle layer **0–8** on/off (`SituationVdStandbyToggleLayer`) |
| `-` | Clear all calibration layers → **snow** (`pattern_layers = 0`) |
| `C` | Toggle **chroma snow** (when no layers 0–8) |
| `[` / `]` | Move **selected** layer **down/up** one slot in stack (swap neighbours) |
| `Tab` | Cycle **selected layer** highlight (0–8, wrap) |
| `R` | Reset current section to RGL defaults (`SituationVdStandbyConfigInitDefaults` preserving VD size) |
| `F` | Cycle fallback: **PATTERN → COLORBURST → SOLID → PATTERN** |
| `SPACE` | Optional: toggle trivial live draw (demonstrate idle threshold) |
| `1`–`4` (with **Shift**) | Load preset **1–4** (see §6.4.4) |

**Selected-layer param nudges** (repeat while held; shift = coarse step)

| Layer | Keys | Param |
|-------|------|-------|
| **0 SMPTE** | `Q`/`A`, `W`/`S` | `content_margin_x`, `content_margin_y` |
| | `O` | Toggle `show_overlay_circle` |
| **1 Checker** | `Q`/`A`, `W`/`S` | `tile_size_x`, `tile_size_y` |
| **2 Convergence** | `Q`/`A` | `stripe_width` |
| **4 Grid** | `Q`/`A` | `spacing_px` (0 = auto width/32) |
| **5 PLUGE** | `Q`/`A` | `safe_margin` |
| **6 Crosshatch** | `Q`/`A`, `W`/`S` | `grid_nx`, `grid_ny` |
| **7 Multiburst** | `Q`/`A` | `num_frequencies` (0–6) |
| **8 Cube** | `Q`/`A` | `size` |
| **Snow** (no layers) | `N`/`M` | manual `noise_frame_seed` step (else auto from frame time) |

*Gradients (layer 3) and full palette editing:* preset-only in v1; document as **P14.1** stretch if HUD grows past ~400 lines.

#### 6.4.4 — Presets (Shift+1 … Shift+4)

| Preset | `pattern_layers` | Notes |
|--------|------------------|-------|
| **1 — Snow** | `0` | Default create behaviour; seed animates |
| **2 — SMPTE card** | bit **0** only | Full RGL SMPTE bars |
| **3 — Checker + SMPTE** | bits **0\|1** | Default stack; harness parity test |
| **4 — Full stack** | all bits **0–8** | Default stack + all layers enabled |

#### 6.4.5 — HUD layout

Two-band overlay on the **host** window (not burned into the VD pattern):

```
┌─────────────────────────────────────────────────────────────┐
│ 25_vd_standby  │  mode: PATTERN  │  layers: 0x003  │  sel: 1 │
├─────────────────────────────────────────────────────────────┤
│ stack: [1,2,3,5,7,6,0,4]  │  checker tile: 32×32  │  …     │
├─────────────────────────────────────────────────────────────┤
│                        VD (integer scaled)                   │
├─────────────────────────────────────────────────────────────┤
│ 0–8 toggle │ - snow │ C chroma │ [/] stack │ Tab select │ … │
└─────────────────────────────────────────────────────────────┘
```

Use `SitExample_DrawHUD` for title/hint bars; layer/stack readout on a second text line updated only when `cfg` changes.

#### 6.4.6 — File layout & build

```
examples/25_vd_standby/
├── main.c          ← single file target ≤ ~450 lines; no external assets
└── README.md       ← build one-liner, key map, link to doc/guide/test_patterns.md
```

```bat
build\build_examples.bat static-opengl  25_vd_standby
build\build_examples.bat static-vulkan 25_vd_standby
```

- [x] Add lookup entry to `build/build_examples.bat`
- [x] Add row to `examples/README.md` quick-build table
- [x] Add spec to `doc/plan/DIGESTIBLE_EXAMPLES_PLAN.md` (§25)
- [x] Cross-link from `doc/guide/test_patterns.md` (replace `vd_idle_standby_demo.c` as primary demo pointer)
- [ ] Optional: one screenshot in `doc/guide/` assets at 1280×720 for docs

#### 6.4.7 — Acceptance criteria (P14 gate)

- [x] User can reach **snow**, **single-layer preset**, **multi-layer compose**, and **stack reorder** without recompiling
- [x] Changing checker tile size / convergence stripe / SMPTE margins updates the **VD compositor** output within one frame
- [x] **OpenGL** builds run (`static-opengl` + DLL); no manual UBO/SSBO packing in example code
- [x] README fits **15-minute read** rule from `DIGESTIBLE_EXAMPLES_PLAN.md`
- [x] Example source cites **`SituationVdStandby*`** and **`SituationSetVirtualDisplayPatternConfig`** only — no harness internals

#### 6.4.8 — Relationship to other phases

| Phase | Relationship |
|-------|--------------|
| **P6** | Optional `shader_lab_test_patterns.c` — custom FS fullscreen path for shader authors; **not** the user demo |
| **P12** | Live-edit goals **fold into P14**; close P12 when P14 ships |
| **P11** | **Hard dependency** — stack compose must match harness `pattern_compose_checker_plus_smpte` |
| **P10** | **Hard dependency** — per-layer SSBO reads (checker tile, convergence stripe, etc.) |

---

## §7 — Harness & verification gates

**Checklist style:** extend `doc/misc/TEST_SPIRV_SHADER_API.md` with a test-pattern section (mirror `doc/plan/SHADER_DEBUG_PLAN.md` §1).

### 7.0 — Two include-validation paths (do not conflate)

| Mechanism | What it validates | When |
|---|---|---|
| **Build-time glslc** `-I sit/gpu` in `compile_harness_shaders.ps1` | Headers compile; GL + VK SPIR-V embed | Every harness shader rebuild |
| **Runtime shaderc** via `SituationLoadShaderFromMemory` | `_SituationShaderIncluderResolve` search paths | Vulkan P0 gate; optional OpenGL if §0.5 option A |

Harness embed **does not** exercise the runtime resolver — both are required.

### 7.1 — Harness shaders (GL **and** VK pairs)

Follow existing pattern: `harness_solid_red_gl.fs` / `harness_solid_red_vk.fs` → `sit_harness_spirv_{gl,vk}_embed.c`.

| File | Purpose |
|---|---|
| `tests/harness/shaders/harness_test_pattern_smpte_gl.fs` | SMPTE **VD subset** readback (OpenGL-target SPIR-V) |
| `tests/harness/shaders/harness_test_pattern_smpte_vk.fs` | Same (Vulkan-target SPIR-V) |
| `tests/harness/shaders/harness_test_pattern_checker_gl.fs` | Checkerboard corner pixel |
| `tests/harness/shaders/harness_test_pattern_checker_vk.fs` | Same |
| `tests/harness/shaders/harness_test_pattern_grid_gl.fs` | Grid-only line intersection |
| `tests/harness/shaders/harness_test_pattern_grid_vk.fs` | Same |

- [x] Each FS `#include`s `sit/gpu/test_patterns/sit_test_patterns.glslh` + `sit_tp_config_ubo.glslh` (SMPTE VD: `sit_tp_smpte_vd_ubo.glslh`)
- [x] Extend `build/compile_harness_shaders.ps1` — pattern FS compiled for GL + VK targets
- [x] `scripts/gen_test_pattern_spirv_embed.ps1` — separate pattern embed slots
- [ ] Update `scripts/spirv_desc_spike.py` `EXPECTED_VARS` for new descriptor/uniform layout
- [ ] OpenGL harness requires `GL_ARB_gl_spirv` (same as existing SPIR-V harness tests in `test_graphics_spirv.c`)

### 7.2 — Readback tests (`tests/harness/test_graphics.c`)

| Test | Assert |
|---|---|
| `test_pattern_smpte_vd_bar_color` | VD subset: pixel at top-bar yellow ≈ `(0.706, 0.706, 0.0)` ±1/255 (**not** RGL `0.753`) |
| `test_pattern_smpte_full_bar_color` | Full RGL SMPTE (optional): yellow ≈ `(0.753, 0.753, 0.0)` ±1/255 |
| `test_pattern_checkerboard_corner` | (0,0) white, (tile, tile) black for 32×32 tiles |
| `test_pattern_grid_line` | Grid intersection pixel brightness > background |
| `test_pattern_convergence_moire_zone` | Central horizontal stripe region differs from vertical-only outer |
| `test_pattern_pluge_black_bar` | 0 IRE bar `(0,0,0)` |
| `test_pattern_multiburst_band_count` | 6 frequency regions detectable via column sampling |
| `test_pattern_crosshatch_center` | Crosshair center pixel white on gray |
| `test_pattern_compose_checker_plus_smpte` | Default stack + params → checker under SMPTE (§3.5 / §3.6) |
| `test_pattern_compose_order_smpte_under_checker` | **P11** — reversed stack readback |
| `test_pattern_layer_params_checker_tile` | **P10** — live checker tile size |
| `test_pattern_layer_params_convergence_stripe` | **P10** — stripe width |
| `test_pattern_zero_layers_noise` | Animated B&W snow |
| `test_pattern_chroma_snow` | RGB snow with bit 16 |

- [x] Implement in `tests/harness/test_graphics_patterns.c` (SPIR-V embed + runtime include on VK)
- [x] Register tests in suite table (`test_graphics.c`)
- [x] Run `build\tests\sit_test_vulkan.exe --module graphics --filter pattern` — **16/16** (v2.4.378)
- [x] Run `build\tests\sit_test_opengl.exe --module graphics --filter pattern` — **15/15** (v2.4.378)

### 7.3 — Visual regression (manual)

- [ ] Capture 9 PNGs (one per pattern) at 640×480 from `shader_lab_test_patterns.c`
- [ ] Side-by-side with RGL reference renders (one-time capture script using `RGL_DrawTestPattern` before RGL deprecation)
- [ ] Store references under `tests/harness/assets/test_patterns/` (optional; manual gate)

### 7.4 — Acceptance criteria

- [x] All §7.2 automated tests green on Vulkan + OpenGL (3D test deferred with P5)
- [ ] `vd.frag` / `composite.frag` SMPTE **VD subset** pixel-identical pre/post refactor (§5.3)
- [ ] No regression in full `graphics` + `virtual_display` module counts
- [ ] Runtime include path works from at least two CWDs: repo root and `examples/other/` (**Vulkan** P0 gate)
- [ ] Build-time glslc `-I sit/gpu` succeeds on CI/dev machine without glslc stub (see `compile_harness_shaders.ps1` WARN path)

---

## §8 — RGL migration plan update

- [ ] Update `doc/misc/RGL_MIGRATION_PLAN.md` §7D.3:

  **Before:** "Test patterns — low priority; can remain CPU queue into batch"

  **After:** "Test patterns — **migrated** to `sit/gpu/test_patterns/` shader includes. `RGL_DrawTestPattern` → thin wrapper calling Situation shader fullscreen pass, or deprecated."

- [ ] Add `RGL_DrawTestPattern` shim (optional PR):
  ```c
  // rgl.h — delegates to Situation shader blit when SIT_TEST_PATTERNS_SHIM defined
  void RGL_DrawTestPattern(const RGLTestPatternConfig* config) {
      // RGL_DrawTestPattern today requires RGL.is_batching == true (rgl.h ~9297)
      if (!RGL.is_batching) { RGL_Begin(-1); /* draw */ RGL_End(); return; }
      _SitTestPatternBlitFullscreen(config);  // uses SitTestPatternGetDefault + lab shader
  }
  ```
- [ ] Shim must wrap `RGL_Begin`/`RGL_End` if not already batching — not a one-line delegate
- [ ] Mark shim **optional** — primary consumer path is direct shader include

---

## §9 — Documentation

- [x] Create `doc/guide/test_patterns.md` (why, layer toggles, VD summon, fullscreen draw, FAQ)
  - **C API layer list:** plan **§3.4** (authoritative); user guide § API quick reference (mirror)
  - Quick start `#include` snippet
  - Layer index / bitmask table (`SitTestPatternLayer` ↔ `SIT_TEST_PATTERN_LAYER_*`)
  - Uniform layout for Vulkan vs OpenGL
  - UV origin / `SIT_TP_FLIP_Y` explanation
  - Text label overlay recipe
  - 3D grid raymarch limitations
- [x] Add cross-link from `doc/guide/virtual_display.md` (VD SMPTE now lives in shared header)
- [x] Add entry to `DIGESTIBLE_EXAMPLES_PLAN.md` for **`25_vd_standby`** (§6.4 / **P14**)

---

## §10 — Phased delivery (PR order)

| Phase | PR title | Deliverable | Depends | Status |
|---|---|---|---|---|
| **P0** | Include resolver + config/colors + cache fingerprint | §1.3, §1.5, §3 | — | ✅ **Shipped** |
| **P1** | `sit_tp_primitives.glslh` | §2 | P0 | ✅ **Shipped** |
| **P2** | `sit_tp_smpte.glslh` + compositor SMPTE dedup | §5 | P1, §0.5 for GL delivery | ✅ **Vulkan shipped**; GL §0.5-C mirror; §5.3 gate open |
| **P3** | 2D patterns (checker, convergence, gradients, grid, pluge, multiburst, crosshatch) | §4.2–4.8 | P1 | ✅ **Shipped** |
| **P4** | `sit_test_patterns.glslh` dispatcher | §4.11 | P2 full SMPTE, P3 | ✅ **Shipped** |
| **P5** | `sit_tp_3d_grid.glslh` raymarch | §4.9 | P1 | ⏸ **Stub only** |
| **P6** | `sit_test_pattern_config.h` + `shader_lab_test_patterns.c` + embed script | §6.1 | P4 | ⏸ **Optional dev lab** — C API shipped; users → **P14** |
| **P7** | Harness readback tests (GL+VK embed) | §7 | P4 | ✅ **Shipped** (graphics **15/15** GL · **16/16** VK) |
| **P8** | Docs + RGL migration plan update | §8, §9 | P7 | ⏳ **Guide shipped**; RGL_MIGRATION_PLAN §7D.3 + digestible examples open |
| **P9** | **`SitTestPatternConfig` + all `SitTpParams*` types + default stack/defaults (CPU)** | §3.6.1–§3.6.4 | P4 | ✅ **Shipped** v2.4.374 |
| **P10** | **Per-layer params SSBO + shader refactor** | §3.6.3, §3.6.6 | P9 | ✅ **Shipped** v2.4.378 |
| **P11** | **Stack loop + VD dual upload (header UBO + SSBO)** | §3.5, §3.6.5 | P10 | ✅ **Shipped** v2.4.378 |
| **P12** | **Lab live-edit** (stack reorder + per-layer param keys) | §3.5.6, §3.6.7, §6.1 | P11 | ⏸ **Folded into P14** — close when P14 ships |
| **P13** | **Replace layer 8 — single cube (`sit_tp_cube`)** | §4.9 | P4 | ✅ **Shipped** v2.4.373 |
| **P14** | **`25_vd_standby` digestible example** — real-time pattern exploration on VD | §6.4 | P10, P11 | ✅ **Shipped** |

**Completed slice (June 27, 2026):** P0–P14 + harness pattern suite green. **Next:** §5.3 COLORBURST gate, optional `pattern_compose_order_smpte_under_checker`, §8 RGL shim.

### P9 implementation checklist (CPU struct PR) — ✅ v2.4.374

1. [x] **`SitVdStandbyConfig`** in **`sit/situation_api_types_gpu.h`** — per-layer param sub-structs + snow.
2. [x] **`SitTestPatternInitLayerParamsDefaults`** — fill all layer blocks (§3.6.2 catalog); migrate hardcoded shader literals into default values.
3. [x] **`SitTestPatternConfigInitDefaults`** — frame + **default stack** (8 entries) + all layer defaults + palette.
4. [x] Single public type **`SitVdStandbyConfig`** (no parallel `SitTestPatternConfig` header).
5. [x] Harness + VD init use **`situation_api_types_gpu.h`**; **keep** 144 B pack via **`SitVdStandbyPackStd140`**.
6. [x] Unit tests: **`pattern_config_defaults`** — default stack, checker tile 32, cube size 1, pack shim.

### P10 implementation checklist (GPU params PR) — ✅ v2.4.378

1. [x] Add **`sit_tp_layer_params.glslh`** + **`sit_tp_layer_params_ssbo.glslh`** (std430, binding **1**).
2. [x] Refactor each **`sit_tp_*.glslh`** to read its param struct (not flat `SitTpConfig`).
3. [x] Header UBO **160 B** @ binding **0** (stack + frame + seed); std140 scalar tail fix.
4. [x] Compositor + harness bind SSBO; dual upload header + layer block.
5. [x] Harness: `pattern_layer_params_checker_tile`, `pattern_layer_params_convergence_stripe`.

### P11 implementation checklist (stack compose PR) — ✅ v2.4.378

1. [x] **`sit_tp_compose.glslh`** — stack loop + `sit_tp_apply_layer(index, …, layer_params)`.
2. [x] **`sit_test_patterns.glslh`** delegates multi-bit path to **`sit_tp_compose_stack`**.
3. [x] Stack API: `SituationVdStandbySetLayerOrder`, toggle append/remove.
4. [ ] **`pattern_compose_order_smpte_under_checker`** — optional follow-up readback test.

### P12 implementation checklist (lab PR) — ⏸ folded into P14

1. ~~**`shader_lab_test_patterns.c`**~~ — optional dev lab in `examples/other/`; **not** the user demo.
2. Live-edit UX → **`25_vd_standby`** via `SituationSetVirtualDisplayPatternConfig` (§6.4).
3. Cross-link in `DIGESTIBLE_EXAMPLES_PLAN.md` → **P14**.

### P14 implementation checklist (`25_vd_standby` example PR) — ✅ shipped

1. [x] **`examples/25_vd_standby/main.c`** — VD + `SitVdStandbyConfig` + HUD; §6.4 input map.
2. [x] **`examples/25_vd_standby/README.md`** — build, keys, exploration sections A–F.
3. [x] **`build/build_examples.bat`** lookup + `examples/README.md` table row.
4. [x] **`doc/plan/DIGESTIBLE_EXAMPLES_PLAN.md`** §25 spec.
5. [x] **`doc/guide/test_patterns.md`** — primary demo pointer → `25_vd_standby`.
6. [ ] Manual smoke on Vulkan static (blocked here by stale `situation_vulkan.a` rebuild — OpenGL static + DLL verified).

### P13 implementation checklist (cube correction PR)

1. Add **`sit_tp_cube.glslh`** — one SDF box, fixed view/light; params: `size`, `diffuse`, `ambient` only.
2. Wire layer 8 in dispatcher; retire **`sit_tp_3d_grid.glslh`**.
3. Deprecate **`grid_size`** in standby API (ignore in pack until removed).
4. Harness **`pattern_cube_lit_faces`** replaces `pattern_3d_grid_axis_red`.
5. Update guides: layer 8 = **Cube**, not “3D grid”.

---

## §11 — Readiness summary (updated June 23, 2026)

| Area | Verdict |
|---|---|
| **Strategic focus** | **High value** — `.glslh` library; compositor + users share authoring |
| **Library behavior (default slice)** | **Unchanged** — refactor + additive `#include` API |
| Overall direction | **On track** — first slice shipped; headers authoritative |
| Vulkan compositor | ✅ **`#include` at init** — live in `vd.frag` / `composite.frag` |
| OpenGL compositor | ✅ **§0.5-C mirror** — inline duplicate; sync debt documented |
| RGL inventory / layout math | **Accurate** — verified against `rgl.h` |
| Enum parity | ✅ **In GLSL + C** — `sit/sit_test_pattern_config.h` |
| Built-in compile model | **Runtime disk load at init** — not DLL build; see Focus |
| Include resolver | ✅ **Multi-path search** — fingerprint bumped |
| OpenGL core delivery | **§0.5-C active** — flatten/unify (A/B) optional follow-up |
| Harness dual-backend | ✅ **Shipped** — std140 UBO @ set=0; identical GL/VK FS sources |
| VD SMPTE dedup | ✅ **Vulkan done** — §5.3 pixel gate pending |
| Primitives layer | ✅ **Shipped** — `sit_tp_primitives.glslh` |
| 3D cube (layer 8) | ✅ **P13** — `sit_tp_cube.glslh` one lit cube |
| Example app / user docs | **`25_vd_standby` shipped** (P14) + `doc/guide/test_patterns.md` |
| **Layer compose order** | ✅ **P11** — user stack in shader + C API |
| **Per-layer parameters** | ✅ **P10** — SSBO + shader reads; live edit via VD API in **P14** |

### Known unplanned-work triggers

1. OpenGL core shader compile path change (§0.5 A/B) — largest hidden cost; **C defers this**
2. ~~`SitTpConfig` std140 / push-constant layout + SPIR-V profile alignment (§1.5)~~ — ✅ **std140 UBO shipped**
3. ~~`_SitVkShadercOptionsFingerprint` bump when resolver changes (§1.3)~~ — ✅ done (`^= 0x2`)
4. `gen_spirv_embed.ps1` extension for new harness shaders (§7.1)
5. RGL shim batching wrapper (§8)
6. Grid alpha compositing for `grid_white` (§0.3) — **not yet implemented in pattern modules**
7. **OpenGL SMPTE sync drift** — inline mirror in `vd.frag` / `composite.frag` must stay aligned with `sit_tp_smpte.glslh` until A/B
8. ~~**UBO 144 → 160 B** (§3.5.3)~~ — ✅ **Shipped v2.4.378** — header UBO + 832 B SSBO; compositor + harness dual bind

---

## Build & verify

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" vulkan
& ".\build\build_situation.bat" opengl

# glslc smoke (repo root on include path):
$glslc = ".\ext\shaderc\build\glslc\glslc.exe"
& $glslc -fshader-stage=fragment -DSITUATION_USE_VULKAN -DSIT_TP_SMPTE_VD_SUBSET=1 `
  -I "." ".\sit\gpu\vd.frag" -o "$env:TEMP\vd_test.spv"

# After P7:
& ".\build\compile_harness_shaders.bat"
python scripts\spirv_desc_spike.py
& ".\build\tests\sit_test_vulkan.exe" --module graphics --filter pattern
& ".\build\tests\sit_test_opengl.exe" --module graphics --filter pattern
# After P6:
# build\build_examples.bat opengl shader_lab_test_patterns
```

### Success (check all that apply)

- [x] **Focus:** `sit/gpu/test_patterns/` is documented as the first modular shader library; `#include` story in `doc/guide/test_patterns.md`
- [x] `sit/gpu/test_patterns/sit_test_patterns.glslh` exists and compiles (glslc `-I` repo root; Vulkan runtime via resolver)
- [ ] Nine `SIT_TP_*` patterns render correctly in `shader_lab_test_patterns.c`
- [x] `vd.frag` / `composite.frag` consume `sit_tp_smpte.glslh` (Vulkan `#include`; OpenGL §0.5-C mirror)
- [ ] VD colorburst pixel-identical ±1/255 (§5.3 gate)
- [x] Harness pattern tests pass — graphics **15/15** GL · **16/16** VK; full harness `--filter pattern` **17/17** GL (v2.4.378)
- [x] `doc/guide/test_patterns.md` published
- [ ] `RGL_MIGRATION_PLAN.md` §7D.3 updated

---

## Execution log

Chronological record of shipped work and open gates. **Current head:** P14 `25_vd_standby` shipped · **Next:** §5.3 gate, `pattern_compose_order_smpte_under_checker`, §8 RGL shim.

### June 23, 2026 — P0–P4 core library

- [x] **§0.5 OpenGL strategy** — decision: **C** (temporary GL inline mirror; Vulkan `#include` first)
- [x] **P0 include resolver** — multi-path search + `requesting_source` relative; fingerprint `^= 0x2`; `sit_tp_config.glslh` + `sit_tp_colors.glslh` added
- [x] **P1 primitives** — `sit_tp_primitives.glslh` (grid, checkerboard, stripes, safe area, crosshair, gradient, rect, ring); glslc smoke OK
- [x] **P2 SMPTE dedup (Vulkan)** — `#include` in `vd.frag` / `composite.frag`; VD subset in `sit_tp_smpte.glslh`; GL inline mirror (§0.5-C); builds OK
- [x] **P3 2D patterns** — pluge, multiburst, convergence, gradients, crosshatch; grid/checker via dispatcher
- [x] **P4 dispatcher** — `sit_test_patterns.glslh` + `sit_tp_sample()`; full SMPTE in `sit_tp_smpte_bars`; glslc smoke OK
- [x] **P7 harness (initial)** — Vulkan **9/9**, OpenGL **8/8** (`--filter pattern`; std140 UBO; fixed-order multi-bit compose)

### June 24, 2026 — VD compositor PATTERN path (v2.4.344–347)

- [x] **v2.4.344 · VD idle PATTERN compositor (Vulkan)** — full layer-bitmask standby; `SituationSet/GetVirtualDisplayPatternConfig`; std140 `SitTpConfigBlock`; push constants retired on compositor path
- [x] **v2.4.345 · OpenGL compositor SPIR-V embed** — `compile_vd_compositor_gl.ps1` + `sit_vd_compositor_gl_spirv_embed.c`; init/composite binding fixes (345 regression: glslc auto-bind + embed before green GL init documented)
- [x] **v2.4.346 · OpenGL VD PATTERN parity** — UBO bind @ `SIT_UBO_BINDING_VD_PATTERN`; `pattern_smpte_vd_bar_color` green on GL
- [x] **v2.4.347 · Harness / async stability** — mesh loader + VD paths preserved; VK `async_shader` 6/6
- [x] **P8 (partial) · User guide** — `doc/guide/test_patterns.md` rewritten (idle flow, three fallback modes, VD API table)

### June 26–27, 2026 — Snow, chroma, cube, CPU struct (v2.4.371–375)

- [x] **v2.4.371 · Snow default** — `sit_tp_noise.glslh`; zero-layer animated noise; VD create `PATTERN` + `pattern_layers = 0`; harness `pattern_zero_layers_noise`; `vd_idle_content_switch` snow bookends
- [x] **v2.4.372 · Chroma snow** — bit **16** + `SituationSetVirtualDisplayChromaSnow`; harness `pattern_chroma_snow`; GL pattern suite **12/12**
- [x] **v2.4.373 · P13 cube layer** — `sit_tp_cube.glslh`; removed `sit_tp_3d_grid.glslh` raymarch; harness `pattern_cube_lit_faces`
- [x] **2026-06-27 · §3.5 + §3.6 design** — default stack + unified config + per-layer `SitTpParams*` + SSBO binding model signed off
- [x] **v2.4.374 · P9 CPU struct** — `SitTestPatternConfig` / `SitVdStandbyConfig`; stack + palette + nine layer param blocks; flat **144 B** pack shim; harness `pattern_config_defaults`
- [x] **v2.4.375 · Library layout fix** — removed standalone `sit_test_pattern_config.h`; types + `SitVdStandby*` helpers consolidated in `situation_api_types_gpu.h`; harness delegates to library pack APIs

### June 27, 2026 — Impl separation + GPU params + stack compose (v2.4.376–378)

- [x] **v2.4.376 · VD standby impl split** — init/toggle/stack/pack moved to `sit/situation_impl_vd_standby.h`; public **`SituationVdStandby*`** in `situation_api_graphics.h`
- [x] **v2.4.377 · `api_types_gpu.h` types-only cleanup** — render-pass helpers → `situation_impl_renderer_frame_cmd.h`; `ViewDataUBO` → `situation_impl_renderer_core.h`; VD standby types grouped under Virtual Display section; **4/4** `render_pass_*` tests green *(interim `situation_impl_render_pass.h` was a layout deviation — absorbed into `frame_cmd` @ v2.4.384)*
- [x] **v2.4.378 · P10 SSBO + 160 B header UBO** — `sit_tp_layer_params*.glslh`, `sit_tp_config_header_ubo.glslh` (160 B); 832 B std430 SSBO @ binding **1**; compositor + harness dual upload/bind; per-layer shader reads; **`SituationVdStandbyPackParamsStd430`**
- [x] **v2.4.378 · P11 stack compose** — `sit_tp_compose.glslh` + `sit_tp_compose_stack()`; multi-bit path replaces hardcoded compose chain; default stack `{1,2,3,5,7,6,0,4}` pixel-parity via harness
- [x] **v2.4.378 · Harness expansion** — `pattern_compose_checker_plus_smpte`, `pattern_layer_params_checker_tile`, `pattern_layer_params_convergence_stripe`; graphics `--filter pattern` **15/15** GL, **16/16** VK (+ runtime `#include`); full harness `--filter pattern` **17/17** GL (incl. 2× `virtual_display`)
- [x] **v2.4.378 · std140 header layout fix** — replaced `float _header_pad_tail[2]` (16 B stride bug) with scalar `_header_pad7` / `_header_pad8`; `layer_stack_count` @ GPU offset **140** matches CPU packer
- [x] **P14 · `25_vd_standby` example** — `examples/25_vd_standby/`; live VD pattern explorer via `SituationSetVirtualDisplayPatternConfig`; OpenGL static + DLL build OK

### Deferred / superseded

- [x] **P5 3D grid raymarch** — **Superseded by P13** (one lit cube); `sit_tp_3d_grid.glslh` removed
- [ ] **P6 `shader_lab_test_patterns.c`** — optional dev lab in `examples/other/`; user-facing demo → **P14** ✅
- [ ] **P2 §5.3 pixel gate** — VD COLORBURST readback max Δ vs pre-refactor baseline: _______________
- [ ] **`pattern_compose_order_smpte_under_checker`** — reversed-stack harness readback (P11 follow-up)
- [ ] **§8 RGL shim** — optional `RGL_DrawTestPattern` → Situation shader blit
- [ ] **§7.3 reference PNGs** — nine pattern captures @ 640×480 (manual; can use `25_vd_standby`)
- [ ] **Full graphics module regression** — record pass counts on release tags: sit_test_opengl ___/___ , sit_test_vulkan ___/___

---

## Open questions

1. ~~**§0.5 OpenGL delivery:** A / B / C?~~ **Decided: C** (June 23, 2026). Revisit A/B when GL compositor sync debt becomes painful.
2. ~~**SMPTE full vs VD subset default:**~~ **Decided:** VD subset behind `SIT_TP_SMPTE_VD_SUBSET`; full RGL via `#else` + `SitTpPalette` (not yet implemented).
3. ~~**UBO vs push constant for `SitTpConfig`:**~~ **Decided:** harness + shared C API use **std140 UBO** on both backends; P6 lab may add optional VK push-constant fast path.
4. **RGL shim:** Implement `RGL_DrawTestPattern` → shader blit in `rgl.h`, or document-only deprecation? If shim: requires batching wrapper (§8).
5. **Reference PNG capture:** Automate RGL reference renders before any RGL removal, or accept manual one-time capture?
6. **P5 deferral:** Ship v1 with eight 2D patterns only and stub `SIT_TP_3D_GRID` with solid `bg_dark_gray` + "deferred" label via CPU text? **Likely yes** — P5 deferred. *(P5 raymarch shipped later.)*
7. ~~**Layer compose order:**~~ **Decided:** Option A explicit stack — §3.5; default stack always populated; **P11** shader loop.
8. ~~**Per-layer parameters:**~~ **Decided:** Unified **`SitTestPatternConfig`** with nine `SitTpParams*` blocks + snow — §3.6; **SSBO binding 1**; **P9** CPU struct, **P10** GPU.
9. ~~**Layer 8 identity:**~~ **Decided (June 27, 2026):** **One lit cube** — not grid/scene/camera params; §4.9; **P13** replaces `sit_tp_3d_grid.glslh`. RGL `_RGL_Draw3DGrid` grid-of-cubes is **drift**, not spec.