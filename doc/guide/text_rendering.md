## Text Rendering Module

**Overview:** GPU text rendering via the command buffer — `SituationCmdDrawText*` batches textured quads through the internal text pipeline for HUDs, debug overlays, and in-game UI at 60+ FPS. Font **loading, baking, measurement, and lifecycle** live in the dedicated [Fonts Module](font.md).

CPU text (`SituationImageDrawText*`, stamp APIs) rasterizes into a `SituationImage` for texture baking and offline labels. See [Image Module — Text onto images](image.md#text-onto-images) and [Fonts Module](font.md).

**Canonical pattern:** Numbered examples use `examples/shared/sit_example.h`, which draws HUD text with a zeroed `SituationFont` (built-in 8×8 VGA font). See `SitExample_DrawHUD()`.

**Related docs:**
- [Fonts Module](font.md) — `SituationFont`, load/bake/builders, measure, unload, stamp
- [2D Rendering & Drawing](drawing_2d.md) — coordinate system, scissor, quads
- [Image Module](image.md) — CPU-side text onto images
- [situation_sdk.md §3.9](../situation_sdk.md#39-text-rendering) — SDK workflow summary
- [situation_command_reference.md §6](../situation_command_reference.md#6-high-level-draw-helpers) — command catalog

---

### Two Rendering Paths

| Path | Functions | Output | Best for |
|------|-----------|--------|----------|
| **GPU (this guide)** | `SituationCmdDrawText`, `SituationCmdDrawTextEx`, `SituationCmdDrawTextBoxed` | Screen pixels via internal text pipeline | Real-time HUD, scores, chat, debug overlays |
| **CPU (image buffer)** | `SituationImageDrawText*`, `SituationImageStampText*` | Pixels in a `SituationImage` | Texture atlases, screenshots, offline labels |
| **Diagnostics** | `SituationDrawMetricsOverlay` | Multi-line FPS/latency stats | Dev builds (`M` key in `sit_example.h`) |

```
  Font setup (see font.md)
           │
           ▼
    SituationFont handle
           │
     ┌─────┴─────┐
     ▼           ▼
 GPU path       CPU path
 (this file)    (image.md + font.md)
     │
     ▼
 SituationCmdDrawText*
```

Before any GPU draw: load and bake (or use builders) as described in [Fonts Module](font.md). A **zeroed** `SituationFont` skips setup and uses the built-in default atlas.

---

### Architecture — GPU Text Pipeline

GPU text piggybacks on the **internal quad/text pipeline** initialized during `SituationInit()`.

#### Internal components

1. **Default font atlas** — At init, `_SituationInitDefaultFont()` expands the embedded 8×8 CP437 font into a 128×128 RGBA texture (16×16 grid, **NEAREST** filtering).

2. **Font atlas texture** — TTF fonts use `SituationBakeFontAtlas` (`stbtt_BakeFontBitmap`, ASCII 32–126, 512×512 alpha). Grid/bitmap fonts use NEAREST grid atlases from builders or `SituationBakeBitmapFontAtlas`.

3. **Text shader pipeline** — Dedicated quad shaders sample the font atlas with alpha blending. State is isolated so text draws do not corrupt user-bound shaders.

4. **Vertex batching** — Each character is two triangles (6 vertices). An entire string is one draw packet, not one draw per glyph.

#### Backend behavior

| Backend | Vertex data | Notes |
|---------|-------------|-------|
| **Vulkan** | Persistent mapped **ring buffer** per frame-in-flight | Near-zero allocation; staging fallback if ring fills. |
| **OpenGL** | **Soft command buffer** (`SIT_OP_DRAW_TEXT_EX`) | Deferred execute at `SituationEndFrame()`. |

#### Default font fallback

When `font.atlas_texture.generation == 0`, `SituationCmdDrawTextEx` substitutes `sit_render.default_font`:

```c
SituationFont font = {0};
SituationCmdDrawTextEx(cmd, font, "FPS: 60", (Vector2){10, 10}, 16.0f, 1.0f, WHITE);
```

Details: [Fonts Module — Built-in default](font.md#built-in-default-font).

#### Character set limits

- **Default font:** Grid layout for ASCII 0–127; printable subset renders correctly.
- **Baked TTF atlas:** ASCII 32–126. Other codepoints skipped silently.
- **Max string length:** 2048 characters per draw (truncated internally).

For full Unicode on CPU, use `SituationImageDrawCodepoint` per codepoint ([font.md](font.md)).

---

### Quick Start

#### A — Debug text with zero setup

```c
SituationFont font = {0};

while (!SituationWindowShouldClose()) {
    SituationPollInputEvents();
    SituationUpdateTimers();

    if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
        SituationCmdBeginRenderPass(cmd, &pass);
        SituationCmdDrawTextEx(cmd, font, "Hello", (Vector2){10, 10}, 16.0f, 1.0f, WHITE);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }
}
```

#### B — TTF font for UI

Load and bake per [Fonts Module — TTF workflow](font.md#1-truetype--opentype-ttfotf):

```c
SituationFont ui_font = {0};
SituationLoadFont("assets/Roboto-Regular.ttf", &ui_font);
SituationBakeFontAtlas(&ui_font, 20.0f);

SituationCmdDrawText(cmd, ui_font, "Score: 42", (Vector2){100, 50}, GOLD);
SituationUnloadFont(ui_font);
```

#### C — HUD bar (`sit_example.h`)

```c
#include "shared/sit_example.h"

SitExample_Init(argc, argv, "My Example");
while (!SitExample_BeginFrame()) {
    if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
        SituationCmdBeginRenderPass(cmd, &pass);
        SitExample_DrawHUD(cmd, "06 — Audio Node Graph", "Q/W/E: waveform");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }
}
SitExample_Shutdown();
```

#### D — Centered title (measure first)

```c
SitRectangle bounds = SituationMeasureText(font, "Game Over", 32.0f);
float cx = (screen_w - bounds.width) * 0.5f;
SituationCmdDrawTextEx(cmd, font, "Game Over", (Vector2){cx, 200.0f}, 32.0f, 0.0f, RED);
```

Use `SituationMeasureTextEx` when `spacing` is non-zero ([font.md](font.md#measurement-and-layout)).

---

### Layout and Clipping

**Multi-line:** One line per `DrawTextEx` call unless using `SituationCmdDrawTextBoxed`. Manual multi-line: split on `\n` and advance `y` by line height (typically `fontSize * 1.25`).

**Clipping:** `SituationCmdSetScissor` before text draws to clip to a panel ([drawing_2d.md](drawing_2d.md)).

**Boxed wrap:** `SituationCmdDrawTextBoxed` clips and optionally word-wraps inside a `SitRectangle`.

---

### API Reference — GPU Drawing

Record **inside an active render pass** on a command buffer from `SituationAcquireFrameCommandBuffer()`.

---
#### `SituationCmdDrawText`
Draws at the baked atlas size (or default font scale).

```c
SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font,
    const char* text, Vector2 pos, ColorRGBA color);
```

Equivalent to `SituationCmdDrawTextEx(cmd, font, text, pos, 0.0f, 0.0f, color)`.

---
#### `SituationCmdDrawTextEx`
Primary GPU text function — custom size and letter spacing.

```c
SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font,
    const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color);
```

**Parameters:**
- `fontSize` — height in pixels. `0.0f` uses `font.font_height_pixels` (baked size).
- `spacing` — extra pixels between characters (can be negative).

```c
SituationCmdDrawTextEx(cmd, font, "SITUATION", (Vector2){40, 20}, 48.0f, 2.0f, CYAN);

SituationFont dbg = {0};
SituationCmdDrawTextEx(cmd, dbg, "v2.4.341", (Vector2){4, 4}, 8.0f, 0.0f, GRAY);
```

---
#### `SituationCmdDrawTextBoxed`
Text clipped to a rectangle with optional word wrap.

```c
SituationError SituationCmdDrawTextBoxed(SituationCommandBuffer cmd, SituationFont font,
    const char* text, SitRectangle bounds, float fontSize, float spacing, ColorRGBA color,
    bool word_wrap);
```

---
#### `SituationDrawMetricsOverlay`
Multi-line debug panel: FPS, frame time, spikes, phase breakdown, queue depth, latency, draw counts, memory.

```c
void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color);
```

Toggle in examples with the `M` key (`sit_example.h`). Uses the default font at 16px with 2× scaling.

---

### Requirements and Build Flags

| Requirement | Detail |
|-------------|--------|
| **stb_truetype** | TTF GPU path requires STB. Do not define `SITUATION_NO_STB_TRUETYPE`. |
| **Init** | Text pipeline initializes with `SituationInit()`. Draw before init returns `SITUATION_ERROR_NOT_INITIALIZED`. |
| **Render pass** | Text between `SituationCmdBeginRenderPass` and `SituationCmdEndRenderPass`. |
| **Projection** | `SituationAcquireFrameCommandBuffer()` sets 2D orthographic projection (top-left origin, Y down). |

Font loading requirements: [Fonts Module — Requirements](font.md#requirements-and-build-flags).

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| No text visible | Draw outside render pass or before init | Wrap in `BeginRenderPass` / `EndRenderPass` |
| `SITUATION_ERROR_RESOURCE_INVALID` | Font not baked | See [font.md — TTF workflow](font.md#1-truetype--opentype-ttfotf) |
| `SITUATION_ERROR_FONT_ATLAS_FULL` | Too many/large glyphs | Reduce bake size or simpler font |
| Blocky scaled TTF | Small bake, large draw size | Bake at or above display size |
| Default works, custom doesn't | Missing bake or STB disabled | [font.md troubleshooting](font.md#troubleshooting) |
| Vulkan text invisible (historical) | Stale descriptor/sampler | Fixed v2.3.39+ — use current library |
| `NOT_IMPLEMENTED` | STB stripped | Remove `SITUATION_NO_STB_TRUETYPE` |
| Wrong alignment | Spacing not in `MeasureText` | Use `SituationMeasureTextEx` |

**Legacy API note:** `SituationDrawTextSimple` and `SituationDrawTextStyled` do **not** exist. Use `SituationCmdDrawTextEx` with a zeroed font or a baked TTF font.

---

### Performance Tips

- One `SituationCmdDrawTextEx` per logical line beats many single-character calls.
- Reuse baked fonts — baking is expensive ([font.md — performance](font.md#performance-tips)).
- Default font + `fontSize=16` is fastest (no file I/O, NEAREST sampling).
- Static labels: CPU bake once, then `SituationCmdDrawTexture`.
- Avoid strings longer than a few hundred characters per call; split HUD panels.
