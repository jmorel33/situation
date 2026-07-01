# Test Harness — Advanced Fullscreen Font Showcase Plan

**Status:** **Implemented** — OpenGL + Vulkan green (2026-06-27)  
**Scope:** Second test in the `advanced` harness module: a fullscreen, auto-play visual suite that exercises the full font + YPQ + timing story in one polished presentation.  
**Non-scope:** Pixel readback certification (covered by `text_rendering` / `text_retro_builders`), multi-monitor choreography (covered by `all_displays_windowed_fullscreen_cycle`).  
**Primary files:** `tests/harness/test_advanced_font_showcase.c` (new), `tests/harness/test_advanced.c` (registry only), `tests/harness/sit_test_hud.h`, `tests/harness/sit_test_text_helpers.h`, `tests/harness/sit_test_retro_font_helpers.h`  
**Related:** `doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md`, `examples/18_text_showcase`, `examples/demon_hunt` (YPQ title animation)

**Test name:** `font_capabilities_fullscreen_showcase`

---

## How to use this file

- [ ] Execute phases **A0 → A9** in order; do not start segment work until **A0–A3** are green.
- [ ] Tick actionables in **§5** when implemented **and** visually verified on OpenGL (**G1**).
- [ ] Tick **G2** when Vulkan matches.
- [ ] Before closing, run **§8** verification gates.
- [ ] When shipped: update **§2** progress snapshot, tick **§8**, add harness-only `UPDATELOG` note.

---

## 1. Progress snapshot

| Phase | Status | Delivers |
|-------|--------|----------|
| **A0** Scaffold & registration | ✅ complete | `test_advanced_font_showcase.c` registered |
| **A1** Chrome & segment scheduler | ✅ complete | Title upper-left, FPS/VSync upper-right |
| **A2** YPQ color infrastructure | ✅ complete | 8-stop palette + helpers |
| **A3** Font & texture infrastructure | ✅ complete | GPU+CPU Roboto split; retro + outline builders |
| **A4** Segment 1 — Typography Baseline | ✅ complete | |
| **A5** Segment 2 — Retro Atlas Gallery | ✅ complete | |
| **A6** Segment 3 — YPQ Color Field | ✅ complete | 6 scissor bands |
| **A7** Segments 4–7 — Motion / outline / rotation / layout | ✅ complete | |
| **A8** Segment 8 — Composite Finale | ✅ complete | |
| **A9** Backend parity & ship | ✅ complete | GL + VK; both advanced tests back-to-back |

**Matrix coverage (§5):** **38 / 38** actionables ✅ · **2 / 2** backend gates ✅

**Estimated implementation:** ~**850–1,050 lines** new C · **~2.5–4 focused dev days** · **no library changes**

---

## 1b. Scale of work

### What this is (and is not)

| | |
|--|--|
| **Is** | Visual integration test — wiring existing APIs into a polished 30 s fullscreen demo |
| **Is not** | Pixel-readback certification (`text_rendering` / `text_retro_builders` already cover that) |
| **Is not** | Library or shader work — every API in scope is already exported |

### Code size estimate

| Block | Lines (approx.) | Phase |
|-------|-----------------|-------|
| Scaffold, fullscreen, assert, registry | 80–100 | A0 |
| Chrome + segment scheduler | 100–130 | A1 |
| YPQ stops + helpers | 70–90 | A2 |
| Font bundle load/unload + CPU→texture helper | 90–120 | A3 |
| Segment 1 typography | 40–55 | A4 |
| Segment 2 retro grid | 80–100 | A5 |
| Segment 3 YPQ + scissor bands | 90–120 | A6 |
| Segments 4–7 (four draw fns) | 220–280 | A7 |
| Segment 8 finale | 80–100 | A8 |
| **Total new code** | **~850–1,050** | |

`test_advanced.c` is **381 lines** today. **Start in `test_advanced_font_showcase.c`** (see §7) — do not grow the multi-display test file in place.

### Phase effort (implementation time)

| Phase | Estimate | Risk |
|-------|----------|------|
| A0 Scaffold | 3–4 h | Low |
| A1 Chrome + scheduler | 4–5 h | Low |
| A2 YPQ helpers | 3–4 h | Low |
| A3 Font/texture infra | 4–6 h | Low–medium (outline builder helpers) |
| A4 Segment 1 | 2–3 h | Low |
| A5 Segment 2 | 3–4 h | Low (tedious layout) |
| A6 Segment 3 | 5–7 h | **High** (scissor-band polish) |
| A7 Segments 4–7 | 8–12 h | **Medium** (first harness use of `ImageDrawTextEx`) |
| A8 Segment 8 | 4–5 h | Medium |
| A9 GL + VK ship | 3–5 h | Low–medium (texture lifetime if per-frame restamp) |
| **Total** | **~2.5–4 days** | |

Shorter **30 s runtime** does not materially reduce code — only duration constants and animation rates change.

### Complexity hotspots (ranked)

1. **Segment 3 — YPQ scissor bands** — Port a **trimmed** subset of `demon_hunt.c` `title_draw_copper_text_center` (~6 bands, not 56). Do not port `UiLayout`.
2. **Segment 6 — rotation vs skew** — See **§12 locked decision** (hybrid path).
3. **Segment 5 — dual outline** — Follow `examples/other/hello_world.c` (CPU SDF) + retro `WithOutline` builders (GPU).
4. **Segment 2 — retro gallery** — Low risk; reuse `sit_test_retro_font_helpers.h` builders (+ ~30 lines for VCR/VGA outline variants).
5. **A9 Vulkan** — Retro fonts already pass GL+VK in T6; risk is mainly per-frame CPU texture churn in segments 4–6.

### Existing code to reuse

| Need | Source |
|------|--------|
| Timed loop + teardown | `test_advanced.c` (`advanced_animate` pattern; **not** the VD frame path) |
| GPU text draw | `test_text_rendering.c`, `examples/18_text_showcase/main.c` |
| CPU stamp → GPU blit | `test_text_rendering.c` → `stamp_then_gpu_blit` |
| Retro font builders | `sit_test_retro_font_helpers.h` |
| Roboto load (optional) | `sit_text_test_try_load_roboto_baked` / `try_load_roboto_cpu` |
| YPQ scissor-band title | `examples/demon_hunt/demon_hunt.c` (simplify) |
| YPQ GPU grade on texture | `test_graphics.c` → `test_ypq_grade_pass_cpu_parity` |
| HUD bar + FPS | `sit_test_hud.h` (adapt `_sit_hud_fill` / `_sit_hud_text`; custom title placement) |
| CPU `ImageDrawTextEx` | `examples/other/hello_world.c` (**not yet used in harness**) |

### Net-new harness work

- `SituationImageDrawTextEx` / `SituationImageDrawTextFormatted` wrappers (segments 4–6)
- VCR/VGA `WithOutline` builder helpers (~30 lines; APIs exist, helpers do not)
- Segment scheduler + custom chrome (title upper-left, not stock `sit_test_hud_draw`)
- Eight segment draw functions
- Second entry in `g_advanced_tests[]`

### Implementation files (shipped)

| File | Role |
|------|------|
| `tests/harness/test_advanced_font_showcase.c` | ~870 lines — full showcase |
| `tests/harness/test_advanced.c` | Registry + multi-display test |
| `tests/harness/Makefile` | `test_advanced_font_showcase.c` in `HARNESS_SRCS` |

---

## 2. Purpose and success criteria

This is a **visual integration test**, not a pixel-readback certification test. The existing `text_rendering` and `text_retro_builders` modules already certify individual APIs. This advanced test proves they work together under real runtime conditions.

**The test passes when:**

- It runs fullscreen on the primary monitor for the full suite duration without crashing.
- Every sub-test renders visible content for its allotted time.
- FPS is displayed every frame (uncapped, vsync off).
- The current sub-test title is shown in the **upper-left corner**.
- At least one successful frame per elapsed second: `ok_frames > elapsed_seconds`.
- Teardown restores window state (exit fullscreen, unload fonts, shutdown cleanly).

**Non-goals:** pixel readback assertions; multi-monitor choreography; interactive section switching (auto-play only; ESC exits early).

---

## 3. Runtime configuration

| Setting | Value | Rationale |
|---------|-------|-----------|
| Window mode | `SituationToggleFullscreen()` at test start | User requirement |
| VSync | `SituationSetVSync(false)` in test setup | Explicit + shown in chrome |
| Target FPS | `SituationSetTargetFPS(0)` | Uncapped FPS readout |
| Resolution | `SituationGetRenderWidth/Height()` | Layout uses % of screen |
| Duration | **30 s** content (8 segments × 3–5 s) | Full scope; ~2× faster than original draft |
| Wall clock | **~32 s** incl. setup/teardown | Font load + fullscreen toggle |
| Early exit | `sit_test_hud_poll()` — ESC | Consistent with other visual tests |

**Runtime budget (30 s content):**

| # | Segment | Duration | Notes |
|---|---------|----------|-------|
| 1 | Typography Baseline | 3 s | Static layout — no dwell needed |
| 2 | Retro Atlas Gallery | 4 s | Grid scan; all faces visible immediately |
| 3 | YPQ Color Field | 5 s | Longest slot — hue scroll + scissor bands |
| 4 | Motion & Timing | 3 s | **2× animation rates** vs 55 s draft (sine/orbit/breathe) |
| 5 | Outline & Depth | 3 s | Side-by-side static compare |
| 6 | Rotation & Transform | 4 s | Full 360° over segment (~90°/s) |
| 7 | Layout & Clip | 3 s | Static boxed/measure demo |
| 8 | Composite Finale | 5 s | Hero frame + optional metrics flash |
| | **Total** | **30 s** | |

**Pacing rule:** shorter segments do **not** drop features — increase motion speed (phase scroll, orbit rate, rotation) so each effect reads within its window. Optional segment crossfade: **≤0.2 s** (was 0.4 s).

**Module context:** `advanced` module total ≈ **36 s** (existing multi-display test ~6 s + this test ~32 s).

**Chrome layout:**

```
┌─────────────────────────────────────────────────────────────┐
│ [Sub-test title]                          FPS:142  VSync:OFF│  ← top bar (~22px)
│                     (segment content)                       │
│ Segment 3/8 · advanced · ESC:quit                           │  ← bottom bar (optional)
└─────────────────────────────────────────────────────────────┘
```

Adapt helpers from `sit_test_hud.h`; do **not** use `sit_test_hud_draw()` as-is (it puts module name upper-left, not segment title).

---

## 4. Sub-test suite (8 segments)

Each segment shows its **title upper-left** in the chrome bar. Content anchors to `%` of `sw/sh`.

| # | Title | Duration | Phase | Proves |
|---|-------|----------|-------|--------|
| 1 | Typography Baseline | 3 s | A4 | GPU draw, TTF bake, size/spacing, YPQ caption |
| 2 | Retro Atlas Gallery | 4 s | A5 | CP437, terminal, ASCII, packed, outlined, VCR, VGA |
| 3 | YPQ Color Field | 5 s | A6 | YPQ lerp/modulate, per-glyph hue, scissor bands |
| 4 | Motion & Timing | 3 s | A7 | Sine, orbit, zoom breathe, luma pulse, formatted CPU text |
| 5 | Outline & Depth | 3 s | A7 | CPU SDF outline + GPU baked outline side-by-side |
| 6 | Rotation & Transform | 4 s | A7 | CPU rotation/skew → texture blit; optional YPQ grade |
| 7 | Layout & Clip | 3 s | A7 | Boxed wrap/clip, multiline, measure overlay |
| 8 | Composite Finale | 5 s | A8 | All features composed; screenshot-quality frame |

### Segment content reference

**Segment 1 — Typography Baseline**

- Title: `"Situation Typography"` (TTF or default fallback).
- Size ramp: 12 / 18 / 24 / 32 px.
- Spacing row: `"TRACKING"` at −1, 0, +4.
- Caption (YPQ stop 7): `"GPU · CmdDrawTextEx · scale · spacing"`.

**Segment 2 — Retro Atlas Gallery**

- 2×3 panel grid; same pangram fragment in each retro face.
- Panel labels (default font); sample text (retro font); YPQ tint per panel.

**Segment 3 — YPQ Color Field**

- Hero: `"PERCEPTUAL COLOR GRADING"` — per-glyph YPQ phase offset.
- Secondary: animated `SituationYpqAdjustPhase/Chroma/Luma`.
- 4–6 scissor bands with vertical YPQ color scroll (demon_hunt pattern).

**Segment 4 — Motion & Timing**

- Sine baseline, elliptical orbit, zoom breathe, luma pulse — **~2× angular/rate constants** vs original 55 s draft so loops read in 3 s.
- `"t = %.2f s"` via `SituationImageDrawTextFormatted` → texture blit.

**Segment 5 — Outline & Depth**

- Left: CPU stamp via `adv_cpu_text_ex_to_texture` — **shadow pass** outline (offset draw + fill; see §12.9).
- Right: GPU baked outline (VGA/VCR WithOutline).
- String: `"OUTLINE DEPTH TEST"`; optional drop shadow offset.

**Segment 6 — Rotation & Transform** *(see §12 locked decision)*

- `"ROTATION"`: stamp via CPU once (Roboto CPU or default); animate **0°→360°** over 4 s with `SituationCmdDrawTexture(..., rotation, ...)` (~90°/s).
- `"SKEW"`: `SituationImageDrawTextEx` with `sin(t)*0.3` skew → re-stamp small texture each frame (or every N frames).
- Optional: `SituationCmdDrawTextureYpqGrade` on rotation texture (phase shift over time).

**Segment 7 — Layout & Clip**

- `DrawTextBoxed` wrap on/off in centered rect (~60%×30%).
- Multiline `\n` block; faint `MeasureTextEx` bounds quad; `"Lines: N"`.

**Segment 8 — Composite Finale**

- Center: `"SITUATION"` with YPQ copper/gold scissor bands.
- Orbit subtitle; corner retro VGA + stats; optional `SituationDrawMetricsOverlay` flash.

---

## 5. Target matrix (actionables)

Tick when implemented **and** visually verified on **G1** (OpenGL). Note phase in parentheses.

### 5.1 Infrastructure (A0–A3)

- [ ] **I1** (A0) Register `font_capabilities_fullscreen_showcase` in `g_advanced_tests[]`
- [ ] **I2** (A0) `SituationSetVSync(false)` + `SituationSetTargetFPS(0)` at test entry
- [ ] **I3** (A0) Enter/exit fullscreen helpers; restore state on teardown
- [ ] **I4** (A0) Main frame loop: poll → timers → acquire → draw → end; `ok_frames` counter
- [ ] **I5** (A0) Pass assert: `ok_frames > elapsed_seconds`
- [ ] **I6** (A1) Custom top bar: segment title upper-left
- [ ] **I7** (A1) Top bar: `FPS:%d` + `VSync:OFF` upper-right via `SituationGetFPS()`
- [ ] **I8** (A1) Optional bottom bar: segment index + ESC hint
- [ ] **I9** (A1) `AdvFontSegment` table + auto-advance on elapsed duration
- [ ] **I10** (A1) ESC early exit via `sit_test_hud_poll()` without false assert fail
- [ ] **I11** (A2) 8-stop YPQ palette init (`SituationColorToYPQ`)
- [ ] **I12** (A2) `adv_ypq_lerp` / `adv_ypq_sample` / `adv_ypq_modulate`
- [ ] **I13** (A2) `adv_ypq_band_sample` for scissor-band vertical color field
- [ ] **I14** (A2) Background clear color from YPQ stop 0
- [ ] **I15** (A3) Load default (zeroed), Roboto TTF (optional), all retro builders
- [ ] **I16** (A3) `adv_font_unload_all` — no atlas leaks on teardown
- [ ] **I17** (A3) CPU stamp → `SituationCreateTexture` → `CmdDrawTexture` helper
- [ ] **I18** (A3) Roboto-missing fallback: segments use default grid, test does not skip

### 5.2 GPU text features (segments)

- [ ] **G1** (A4) `CmdDrawTextEx` size ramp (12/18/24/32)
- [ ] **G2** (A4) `CmdDrawTextEx` letter spacing (−1, 0, +4)
- [ ] **G3** (A5) CP437 panel draw
- [ ] **G4** (A5) Terminal panel draw
- [ ] **G5** (A5) ASCII / packed panel draw
- [ ] **G6** (A5) Outlined packed + VCR/VGA WithOutline panels
- [ ] **G7** (A6) Per-glyph color loop (YPQ phase offset per char)
- [ ] **G8** (A6) `SituationCmdSetScissor` horizontal bands + YPQ sample per band
- [ ] **G9** (A7) Animated position (sine + orbit) via GPU `DrawTextEx`
- [ ] **G10** (A7) Animated `fontSize` (zoom breathe)
- [ ] **G11** (A7) `DrawTextBoxed` word wrap on
- [ ] **G12** (A7) `DrawTextBoxed` wrap off / clip
- [ ] **G13** (A7) Multiline `\n` GPU draw
- [ ] **G14** (A8) Finale composite: ≥3 techniques visible simultaneously

### 5.3 CPU text / blit features (segments)

- [ ] **C1** (A7) `SituationImageDrawTextEx` SDF outline (segment 5)
- [ ] **C2** (A7) Outline thickness animation (segment 5)
- [ ] **C3** (A7) Rotation 0°→360° via stamped texture + `CmdDrawTexture` rotation param (segment 6)
- [ ] **C4** (A7) CPU skew via `ImageDrawTextEx` → texture blit (segment 6)
- [ ] **C5** (A7) `SituationImageDrawTextFormatted` timer readout (segment 4)
- [ ] **C6** (A7) Optional `SituationCmdDrawTextureYpqGrade` on text texture (segment 6)
- [ ] **C7** (A7) `MeasureTextEx` bounds quad overlay (segment 7)
- [ ] **C8** (A7) `SituationGetTextLineCount` annotation (segment 7)

### 5.4 YPQ & timing (cross-cutting)

- [ ] **Y1** (A2/A6) All animated colors routed through YPQ (no raw RGB sine except convert-at-draw)
- [ ] **Y2** (A6) `SituationYpqAdjustPhase/Chroma/Luma` on live string
- [ ] **Y3** (A8) Finale copper/gold 4-stop YPQ lerp (demon_hunt pattern)
- [ ] **T1** (A1) All motion driven by `SituationTimerGetTime()` after `UpdateTimers`
- [ ] **T2** (A7) Segment boundary crossfade or clean cut (≤0.2 s fade optional)

### 5.5 Backend parity gates

- [ ] **G1** OpenGL — `--module advanced --filter font_capabilities` passes; visual review OK
- [ ] **G2** Vulkan — same command passes; scissor + texture lifetime OK

---

## 6. Phases

Execute **A0 → A9** in order. Each phase lists actionables (§5 IDs) and an **exit gate** — do not start the next phase until the exit is met.

---

### Phase A0 — Scaffold & registration

**Purpose:** Empty test shell that fullscreen-loops, counts frames, and passes/fails correctly.

**Actionables:** I1, I2, I3, I4, I5

- [ ] **A0.1** — Add `test_font_capabilities_fullscreen_showcase` + registry entry in `g_advanced_tests[]`
- [ ] **A0.2** — `adv_font_showcase_enter_fullscreen()` / `exit_fullscreen()`; confirm `SituationIsWindowFullscreen()`
- [ ] **A0.3** — VSync off + target FPS 0 at entry
- [ ] **A0.4** — Frame loop: clear pass, no segment content yet; `ok_frames++` on successful `EndFrame`
- [ ] **A0.5** — Assert `ok_frames > elapsed_seconds`; log frame count to stderr (match existing advanced test style)

**Exit:** Test runs ~5 s fullscreen, black/clear screen, passes on OpenGL; teardown restores windowed state.

---

### Phase A1 — Chrome & segment scheduler

**Purpose:** Visible HUD and timed segment table driving an empty draw callback per segment.

**Actionables:** I6, I7, I8, I9, I10, T1

- [ ] **A1.1** — `adv_chrome_draw(cmd, title, seg_idx, seg_count)` — top bar layout
- [ ] **A1.2** — Segment title upper-left; FPS + `VSync:OFF` upper-right
- [ ] **A1.3** — `AdvFontSegment segments[]` with title + duration + draw fn pointer
- [ ] **A1.4** — Scheduler: advance `seg_idx` when local elapsed ≥ `duration_sec`
- [ ] **A1.5** — Total content duration **30 s** (`adv_segment_total_duration()`); ESC exits without tripping assert (I10)

**Exit:** Run full 30 s; titles cycle 1→8 in upper-left; FPS visible; ESC mid-run still passes assert.

---

### Phase A2 — YPQ color infrastructure

**Purpose:** Shared color helpers used by every segment.

**Actionables:** I11, I12, I13, I14, Y1

- [ ] **A2.1** — Static `ColorYPQA adv_ypq_stops[8]` from RGB anchors (§4 palette table below)
- [ ] **A2.2** — `adv_ypq_lerp`, `adv_ypq_sample(t)`, `adv_ypq_modulate(...)`, `adv_ypq_band_sample(y, scroll)`
- [ ] **A2.3** — Render-pass clear color from stop 0
- [ ] **A2.4** — Smoke segment: solid YPQ-modulated caption proves helpers work

**YPQ palette stops:**

| Stop | Role | RGB anchor |
|------|------|------------|
| 0 | Deep background ink | `{12, 14, 28}` |
| 1 | Shadow / outline base | `{34, 18, 12}` |
| 2 | Primary body (teal) | `{48, 120, 140}` |
| 3 | Secondary (slate) | `{80, 90, 110}` |
| 4 | Accent warm (amber) | `{200, 140, 48}` |
| 5 | Highlight (soft gold) | `{232, 180, 90}` |
| 6 | Specular (near-white) | `{240, 245, 250}` |
| 7 | Muted caption | `{100, 105, 120}` |

**Exit:** Caption text visibly tints through YPQ cycle in scheduler smoke segment.

---

### Phase A3 — Font & texture infrastructure

**Purpose:** Load all faces once; CPU→GPU blit helper for segments 4–6.

**Actionables:** I15, I16, I17, I18

- [ ] **A3.1** — `adv_font_load_all()` / `adv_font_unload_all()` using `sit_test_text_helpers.h` + `sit_test_retro_font_helpers.h`
- [ ] **A3.2** — Roboto: `sit_text_test_try_load_roboto_baked()` — set `g_adv_has_roboto` flag; no whole-test skip
- [ ] **A3.3** — Retro faces: CP437, terminal, ASCII, packed, outlined packed, VCR+outline, VGA+outline (add `WithOutline` builders — ~30 lines; not in retro helpers yet)
- [ ] **A3.4** — `adv_cpu_text_to_texture(...)` — stamp `SituationImageDrawTextEx` → create/destroy texture
- [ ] **A3.5** — Guard TTF/SDF paths with `#if !SITUATION_NO_STB_TRUETYPE`

**Exit:** Load/unload cycle in isolation (or segment 2 stub) shows no crash; Roboto missing → flag false, default used.

---

### Phase A4 — Segment 1: Typography Baseline

**Purpose:** First real content; validates GPU path + chrome together.

**Actionables:** G1, G2

- [ ] **A4.1** — Implement `adv_seg_typography_baseline_draw(...)`
- [ ] **A4.2** — Size ramp + spacing row + YPQ caption (stop 7)
- [ ] **A4.3** — TTF when `g_adv_has_roboto`; else default grid at same layout

**Exit:** Segment 1 readable at 1080p; title cycles correctly; G1/G2 ticked.

---

### Phase A5 — Segment 2: Retro Atlas Gallery

**Purpose:** All retro builder families visible in one grid.

**Actionables:** G3, G4, G5, G6

- [ ] **A5.1** — Implement `adv_seg_retro_gallery_draw(...)`
- [ ] **A5.2** — 2×3 panel layout with labels + pangram fragment per face
- [ ] **A5.3** — Distinct YPQ tint per panel (stops 2–7)

**Exit:** All 7 retro families identifiable on screen; no missing-atlas pink/magenta garbage.

---

### Phase A6 — Segment 3: YPQ Color Field

**Purpose:** Core differentiator — perceptual color as the animation engine.

**Actionables:** G7, G8, Y2

- [ ] **A6.1** — Implement `adv_seg_ypq_color_field_draw(...)`
- [ ] **A6.2** — Per-glyph YPQ phase offset on hero string
- [ ] **A6.3** — Scissor-band vertical color scroll (**6 bands**, simplified from demon_hunt — not 56)
- [ ] **A6.4** — Secondary string with `SituationYpqAdjustPhase/Chroma/Luma`

**Exit:** Color animation clearly YPQ-driven (hue scroll + luma pulse); bands visible on hero text.

---

### Phase A7 — Segments 4–7: Motion, outline, rotation, layout

**Purpose:** CPU transform path + remaining GPU layout APIs.

**Actionables:** G9, G10, G11, G12, G13, C1–C8, T2

- [ ] **A7.1** — Segment 4: `adv_seg_motion_timing_draw` — sine, orbit, breathe, pulse, formatted timer blit
- [ ] **A7.2** — Segment 5: `adv_seg_outline_depth_draw` — CPU SDF vs GPU baked outline
- [ ] **A7.3** — Segment 6: `adv_seg_rotation_transform_draw` — **hybrid rotation** (§12) + CPU skew blit; optional YPQ grade
- [ ] **A7.4** — Segment 7: `adv_seg_layout_clip_draw` — boxed wrap/clip, measure overlay, line count

**Exit:** All four segments render distinct content; CPU blit path stable for full segment duration.

---

### Phase A8 — Segment 8: Composite Finale

**Purpose:** Polished hero frame combining prior techniques.

**Actionables:** G14, Y3

- [ ] **A8.1** — Implement `adv_seg_composite_finale_draw(...)`
- [ ] **A8.2** — Center `"SITUATION"` with YPQ copper/gold scissor bands
- [ ] **A8.3** — Orbit subtitle + corner credits (VGA + default stats)
- [ ] **A8.4** — Optional `SituationDrawMetricsOverlay` flash every 2 s
- [ ] **A8.5** — Visual polish pass: spacing, no overlap clutter at 1080p and 1440p

**Exit:** Finale looks intentional (screenshot-quality); full 30 s run feels cohesive (not rushed).

---

### Phase A9 — Backend parity & ship

**Purpose:** Close gates; split file if needed; update this plan.

**Actionables:** G1, G2 (§5.5)

- [ ] **A9.1** — OpenGL full run + visual review
- [ ] **A9.2** — Vulkan full run; fix scissor/texture lifetime if needed
- [ ] **A9.3** — Run advanced module both tests sequentially (multi-display then font showcase) — no state leak
- [ ] **A9.4** — Confirm `test_advanced_font_showcase.c` in `Makefile`; keep `test_advanced.c` ≤500 lines (registry + multi-display test only)
- [ ] **A9.5** — Tick all §5 checkboxes; set **§1** snapshot to complete; `UPDATELOG` entry

**Exit:** G1 + G2 green; §5 matrix fully ticked; status line reads **A0–A9 complete**.

---

## 7. Implementation structure

### Architecture (locked)

The font showcase **must not** reuse `advanced_draw_frame()` or Virtual Display panels from test 1. Use the simpler path shared by `text_rendering` and `18_text_showcase`:

```
Poll → UpdateTimers → AcquireFrame → single main pass (display_id = -1)
     → clear (YPQ stop 0) → segment draw → chrome → EndFrame
```

**Rationale:** test 1 spans the virtual desktop with VDs; test 2 is primary-monitor fullscreen with native `SituationGetRenderWidth/Height()`. Mixing the two frame paths increases coupling and teardown bugs.

**State isolation:** each test must exit fullscreen and clear undecorated flags before the next runs (`--module advanced` runs both tests sequentially).

### Recommended build order (validate risk early)

1. **A0 + A1** — empty 8-segment loop + chrome (~1 day).
2. **A3 + A4** — font load + segment 1 at fullscreen resolution.
3. **A6 stub** — 6-band YPQ scissor on hero string (hardest visual).
4. **A7 seg 5** — first `ImageDrawTextEx` usage.
5. Fill A5, remaining A7, A8, A9.

### File layout (locked)

```
tests/harness/test_advanced.c
├── existing multi-display test (unchanged)
└── g_advanced_tests[]  (+ extern to font showcase)

tests/harness/test_advanced_font_showcase.c   ← all new code (~850–1,050 lines)
├── adv_font_showcase_enter/exit_fullscreen
├── adv_chrome_draw
├── adv_ypq_* helpers
├── adv_font_load_all / adv_font_unload_all
├── adv_cpu_text_to_texture
├── adv_seg_*_draw  (×8)
├── adv_font_showcase_draw_frame
└── test_font_capabilities_fullscreen_showcase()
```

Update `tests/harness/Makefile`: add `test_advanced_font_showcase.c` to `HARNESS_SRCS`.

**Frame loop skeleton:**

```c
static void test_font_capabilities_fullscreen_showcase(void) {
    adv_font_showcase_enter_fullscreen();
    SituationSetVSync(false);
    SituationSetTargetFPS(0);
    adv_font_load_all();

    const double total = adv_segment_total_duration();
    const double start = SituationTimerGetTime();
    int ok_frames = 0;
    int seg_idx = 0;
    double seg_start = start;

    while (SituationTimerGetTime() - start < total) {
        if (sit_test_hud_poll()) break;
        SituationPollInputEvents();
        SituationUpdateTimers();
        const double now = SituationTimerGetTime();
        while (seg_idx + 1 < ADV_SEG_COUNT &&
               now - seg_start >= segments[seg_idx].duration_sec) {
            seg_idx++;
            seg_start = now;
        }
        if (adv_font_showcase_draw_frame(seg_idx, (float)(now - start),
                                         (float)(now - seg_start)))
            ok_frames++;
    }

    adv_font_unload_all();
    adv_font_showcase_exit_fullscreen();
    const int elapsed = (int)(SituationTimerGetTime() - start);
    SIT_ASSERT(ok_frames > elapsed);
}
```

---

## 8. Verification checklist (ship gates)

- [ ] **V1** OpenGL: `run_tests.bat opengl --module advanced --filter font_capabilities`
- [ ] **V2** Vulkan: `run_tests.bat vulkan --module advanced --filter font_capabilities`
- [ ] **V3** Fullscreen on primary monitor; exits cleanly to windowed
- [ ] **V4** VSync off confirmed in chrome; FPS uncapped and updating
- [ ] **V5** All 8 segment titles appear upper-left during their window
- [ ] **V6** Visual review at 1080p and 1440p — readable, no clutter
- [ ] **V7** ESC at ~10 s — test passes (`ok_frames > elapsed`)
- [ ] **V8** `--module advanced` runs both tests back-to-back without crash
- [ ] **V9** §5 matrix fully ticked; §1 snapshot updated

---

## 9. Font assets and skip policy

| Asset | Required? | Fallback |
|-------|-----------|----------|
| Default grid (zeroed font) | yes | always available |
| Roboto TTF | optional | default grid; `g_adv_has_roboto = false` |
| Retro builder fixtures | yes | `sit_test_retro_font_helpers.h` |
| stb_truetype | yes for TTF/CPU SDF | `#if !SITUATION_NO_STB_TRUETYPE` |

---

## 10. Capability inventory (reference)

### GPU path (`SituationCmdDrawText*`)

Default grid, TTF bake, scale, spacing, tint, multiline, boxed wrap/clip, scissor, retro atlases, baked outline atlases, per-glyph color, metrics overlay. **No GPU rotation or per-draw SDF outline.**

### CPU path (`SituationImageDrawTextEx`)

Rotation, skew, SDF outline, stamp/boxed stamp, formatted draw → blit to GPU.

### Measurement (segment 7)

`SituationMeasureTextEx` bounds quad; `SituationGetTextLineCount`.

---

## 11. Coverage matrix (scope closure)

| Requirement | Segment(s) | Actionable IDs |
|-------------|------------|----------------|
| Fullscreen | A0 | I3 |
| VSync off | A0 | I2, I7 |
| FPS reported | A1 | I7 |
| Sub-test titles upper-left | A1 | I6 |
| YPQ coloring | A2, A6, A8 | I11–I14, Y1–Y3 |
| Timing / animation | A1, A7 | T1, T2, G9–G10 |
| Rotation | A7 seg 6 | C3 (`CmdDrawTexture` rotation on stamped tex), C4 (CPU skew) |
| Zoom / scaling | A4, A7 | G1, G10 |
| Outline | A5, A7 | G6, C1, C2 |
| All font families | A5, A8 | G3–G6, I15 |
| Boxed / wrap / clip | A7 | G11, G12 |
| Scissor | A6, A7 | G8 |
| CPU stamp → GPU | A7 | I17, C1–C6 |
| Professional polish | A8 | G14, A8.5 |

---

## 12. Locked decisions

1. **Auto-play only** — timed segments; ESC exits via `sit_test_hud_poll()`. No `SPACE: next` unless needed during dev debugging.
2. **Separate source file** — implement in `test_advanced_font_showcase.c` from the start; `test_advanced.c` keeps test 1 + registry only.
3. **Frame path** — single main render pass; no Virtual Displays (§7).
4. **Segment 6 rotation (hybrid)** — avoids per-frame CPU restamp at 60 FPS:
   - **Rotation:** stamp text once with `SituationImageDrawTextEx` (or `ImageStampText`); animate with `SituationCmdDrawTexture(..., rotation, ...)` over 4 s.
   - **Skew:** `SituationImageDrawTextEx` with animated `skewFactor` → re-stamp to small texture each frame (or every 2 frames if perf tight on Vulkan).
   - Proves CPU stamp + GPU blit + transform; keeps frame cost bounded.
5. **Segment 3 scissor bands** — **6 horizontal bands** on hero string; simplified port of demon_hunt copper title (no `UiLayout`, no 56-band loop).
6. **Roboto optional** — `g_adv_has_roboto` flag; never `SIT_TEST_SKIP` the whole test.
7. **Library changes** — **none**; if an API gap appears, stop and file a separate library task (out of scope).
8. **Roboto dual load** — `roboto_gpu` (baked, for `CmdDrawTextEx`) and `roboto_cpu` (unbaked, for `ImageDrawTextEx` / CPU stamp). Do not use baked GPU font for CPU SDF/stamp paths.
9. **CPU outline (segment 5)** — `SituationImageDrawTextEx` with `outlineThickness > 0` heap-faults in this harness session; use **shadow pass** (dark offset draw + fill, thickness=0) in `adv_cpu_text_ex_to_texture`. Visual parity with SDF outline; avoids crash.
10. **Scissor lifecycle** — call `adv_scissor_reset()` at **start of every frame** (segment 3 band draws leave narrow scissor otherwise).
11. **Timer readout (segment 4)** — GPU `CmdDrawTextEx` with formatted string (not `ImageDrawTextFormatted` → blit); same visual goal, lower churn.
