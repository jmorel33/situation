## 2D Rendering & Drawing Module

**Overview:** Situation is a 3D-capable renderer, but its high-level 2D path is a first-class workflow: colored rectangles, textured sprites, text, scissor clipping, and Virtual Display compositing — all through a small set of `SituationCmdDraw*` commands recorded into a command buffer. You do not bind shaders, upload vertex buffers, or set up an orthographic camera for basic 2D. The library's internal **quad renderer** (dedicated pipeline + shared unit-square mesh) handles transforms, alpha blending, and texture sampling.

**When to use this vs 3D:** Use the 2D commands for UI panels, HUDs, debug overlays, sprite games, and retro pixel-art scenes. For **fixed-cell tile playfields** with stacked scrolling layers, see **[2D Grid](grid.md)**. For meshes, cameras, depth, and custom GLSL, see [3D Drawing](drawing_3d.md).

**Canonical examples:**
- `examples/02_draw_shapes/` — quads, rotation, procedural circle texture, labels
- `examples/05_virtual_display_retro/` — 320×240 Virtual Display, integer scaling, PiP compositing
- `examples/shared/sit_example.h` — HUD bars (`SituationCmdDrawQuad` + `SituationCmdDrawTextEx`)

**Related:** [Virtual Display](virtual_display.md) · [2D Grid](grid.md) · [3D Drawing](drawing_3d.md) · [Fonts](font.md) · [Text Rendering](text_rendering.md) · [Image Module](image.md) · [Graphics — API detail](graphics.md#virtual-displays) · [situation_command_reference.md §6](../situation_command_reference.md#6-high-level-draw-helpers)

---

### Architecture — The Internal Quad Renderer

All high-level 2D draws share one backend pipeline initialized at `SituationInit()`:

```
  Your C code
    SituationCmdDrawQuad / DrawTexture / DrawTextEx
           │
           ▼
  Internal quad pipeline (sit/gpu/quad shaders)
    • Unit square mesh (4 verts, triangle strip / 2 tris)
    • Orthographic view-proj UBO (set each frame by AcquireFrameCommandBuffer)
    • Push constants: model matrix, color, UV rect, texture slot
           │
           ▼
  OpenGL soft-buffer replay  OR  Vulkan immediate draw
```

**Key properties:**
- Each `SituationCmdDrawQuad` = **one draw call**, 2 triangles.
- Each character in `SituationCmdDrawTextEx` = 2 triangles (batched into one draw).
- `SituationCmdDrawTexture` builds a model matrix from `dest` + `origin` + `rotation`, then submits an internal textured quad draw.
- State is isolated from your custom pipelines — you can interleave 2D HUD draws with 3D passes in the same frame.

**Requirements:** Must be inside an active render pass (`SituationCmdBeginRenderPass` … `SituationCmdEndRenderPass`). Calling before init or outside a pass returns an error.

---

### Coordinate System

| Space | API | Use for |
|-------|-----|---------|
| **Render pixels** | `SituationGetRenderWidth()` / `GetRenderHeight()` | Drawing commands — this is what 2D coords use |
| **Logical window** | `SituationGetScreenWidth()` / `GetScreenHeight()` | Window placement, input hit areas on HiDPI displays |
| **Virtual Display** | VD's own resolution (e.g. 320×240) | Scene drawn into off-screen target; independent of window size |

**Origin:** Top-left `(0, 0)`. X increases right, Y increases **down**. Matches screen/UI conventions.

**Projection:** Set automatically when you call `SituationAcquireFrameCommandBuffer()`. All `SituationCmdDraw*` 2D functions assume this orthographic space — no `glm_ortho` needed.

**HiDPI:** On Retina/high-DPI monitors, render pixels ≠ logical window size. Always use `SituationGetRenderWidth()` for layout math in draw code.

---

### Frame Loop Pattern

Every 2D frame follows the same skeleton:

```c
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents();
    SituationUpdateTimers();

    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo pass = SituationRenderPassInfoDefault(-1, (ColorRGBA){18, 18, 28, 255});
    SituationCmdBeginRenderPass(cmd, &pass);

    // --- all 2D draws here ---

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}
```

`SituationRenderPassInfoDefault(display_id, clear_color)` clears color and depth. Pass `display_id = -1` for the main window, or a Virtual Display ID to render into an off-screen target.

---

### Drawing Solid Rectangles — `SituationCmdDrawQuad`

The universal 2D primitive. A unit square `(0,0)–(1,1)` transformed by a `mat4` model matrix.

```c
SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);
```

- `model` — translate + scale (+ optional rotate) via **cglm**
- `color` — normalized RGBA `Vector4` in `[0, 1]` per channel; alpha `< 1` enables transparency

**Axis-aligned rectangle helper** (from `02_draw_shapes`):

```c
static void draw_rect(SituationCommandBuffer cmd,
                      float x, float y, float w, float h, Vector4 colour)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, colour);
}
```

**Rotated rectangle** (pivot at center):

```c
static void draw_rect_rot(SituationCommandBuffer cmd,
                          float cx, float cy, float w, float h,
                          float angle_rad, Vector4 colour)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){cx, cy, 0.0f});
    glm_rotate_z(m, angle_rad, m);
    glm_translate(m, (vec3){-w * 0.5f, -h * 0.5f, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, colour);
}
```

**Common uses:**
- Panel backgrounds (semi-transparent black: `{0, 0, 0, 0.8f}`)
- Progress bars, health bars
- Drop shadows (offset dark quad behind a sprite)
- Debug bounding boxes
- Scanline bands (example 05 — thin horizontal quads in a loop)

**HUD bar pattern** (`sit_example.h`):

```c
SituationCmdDrawQuad(cmd, m, (Vector4){{0.0f, 0.0f, 0.0f, 0.80f}});  // 80% opaque top bar
```

---

### Drawing Textured Sprites — `SituationCmdDrawTexture`

Draws a region of a GPU texture into a destination rectangle, with optional rotation and color tint.

```c
SituationError SituationCmdDrawTexture(
    SituationCommandBuffer cmd,
    SituationTexture texture,
    SitRectangle source,    // UV region in texels
    SitRectangle dest,      // screen rect (position + size)
    Vector2 origin,         // rotation pivot relative to dest top-left
    float rotation,         // degrees, clockwise
    ColorRGBA tint);        // multiply color; white = no tint
```

#### Loading a sprite

```c
SituationImage img = {0};
if (SituationLoadImage("assets/player.png", &img) != SITUATION_SUCCESS) { /* handle error */ }

SituationTexture tex = {0};
SituationCreateTexture(img, true, &tex);   // true = generate mipmaps
SituationUnloadImage(img);
```

Or generate procedurally on CPU (circle in example 02), then `SituationCreateTexture`.

#### Full texture blit

```c
SitRectangle src = {0, 0, (float)tex.width, (float)tex.height};
SitRectangle dst = {100, 200, 64, 64};
ColorRGBA no_tint = {255, 255, 255, 255};
SituationCmdDrawTexture(cmd, tex, src, dst, (Vector2){{0, 0}}, 0.0f, no_tint);
```

#### Sprite sheet slice

```c
SitRectangle src = {32, 0, 32, 32};   // second 32×32 tile in a sheet
SitRectangle dst = {x, y, 32, 32};
SituationCmdDrawTexture(cmd, sheet, src, dst, (Vector2){{0,0}}, 0.0f, (ColorRGBA){255, 255, 255, 255});
```

#### Rotation about center

```c
SitRectangle dst = {cx - 40, cy - 40, 80, 80};
Vector2 pivot = {40, 40};   // center of dest rect
SituationCmdDrawTexture(cmd, tex, src, dst, pivot, 45.0f, (ColorRGBA){255, 255, 255, 255});
```

#### Alpha tint / fade

```c
ColorRGBA fade = {255, 255, 255, 128};   // 50% opacity
SituationCmdDrawTexture(cmd, tex, src, dst, (Vector2){{0,0}}, 0.0f, fade);
```

**Transform order internally:** `Translate(dest) → Rotate → Translate(-origin) → Scale(dest.size)`. Same convention as most 2D engines.

**YPQ graded draw:** `SituationCmdDrawTextureYpqGrade` applies live color grading (phase/chroma/luma) in the fragment shader — see [YPQ Color](ypq_color.md) and example 07.

---

### Shapes Beyond Quads

There is no `SituationCmdDrawLine` or `SituationCmdDrawCircle` today. Use these patterns instead:

| Shape | Approach |
|-------|----------|
| **Circle / soft disc** | Generate alpha circle on `SituationImage`, upload once as texture (see `02_draw_shapes`) |
| **Line** | Thin `SituationCmdDrawQuad` rotated with `draw_rect_rot`, or CPU-draw to image |
| **Polygon** | Triangle fan via manual vertices + `SituationCmdDraw`, or CPU bake to texture |
| **Pixel-art circle** | Axis-aligned quad sprite (example 05 minimap ball) |

Example 02 builds a 64×64 anti-aliased circle at startup:

```c
SituationCreateImage(64, 64, 4, &circle_img);
// ... fill pixels with distance test + soft edge ...
SituationCreateTexture(circle_img, false, &circle_tex);
```

Then draws it each frame with `SituationCmdDrawTexture` and a color tint animation.

---

### Text Overlay

GPU text uses the same render pass and coordinate space. See [Fonts](font.md) for load/bake/measurement and [Text Rendering](text_rendering.md) for GPU draw commands and metrics.

Quick HUD label:

```c
SituationFont font = {0};   // built-in 8×8 default
SituationCmdDrawTextEx(cmd, font, "Score: 42",
    (Vector2){{10, 10}}, 16.0f, 1.0f, (ColorRGBA){255, 255, 255, 255});
```

Use `SitExample_DrawHUD()` from `sit_example.h` for a complete top/bottom bar layout.

---

### Scissor Clipping — `SituationCmdSetScissor`

Restricts subsequent draws to a rectangular region. Essential for scrollable panels, clipped text areas, and split-screen UI.

```c
SituationError SituationCmdSetScissor(SituationCommandBuffer cmd,
    int x, int y, int width, int height);
```

**Scroll panel workflow:**

```c
const int panel_x = 40, panel_y = 80, panel_w = 300, panel_h = 200;

SituationCmdSetScissor(cmd, panel_x, panel_y, panel_w, panel_h);

// Draw content offset by scroll position
draw_rect(cmd, panel_x, panel_y - scroll_y, panel_w, content_h, bg);
SituationCmdDrawTextEx(cmd, font, "Item 1", (Vector2){{panel_x + 8, panel_y + 8 - scroll_y}}, ...);

// Reset scissor to full framebuffer when done
SituationCmdSetScissor(cmd, 0, 0, SituationGetRenderWidth(), SituationGetRenderHeight());
```

**Notes:**
- Scissor is in **render pixel** coordinates, same as draw commands.
- For multiple independent clip regions, `SituationCmdSetScissorIndexed(cmd, index, …)` supports indexed scissors (Vulkan multi-scissor path).
- On Vulkan, `SituationCmdSetViewport` also sets scissor to match — use explicit `SetScissor` when you need a clip rect smaller than the viewport.

---

### Virtual Displays — see dedicated guide

Off-screen layers (320×240 retro CRT, PiP minimap, multi-layer compositing) are a **core Situation feature**, not a 2D afterthought.

**Full guide:** **[Virtual Display Module](virtual_display.md)** — architecture diagrams, scaling/blend modes, patterns (retro, PiP, split-screen, 3D layer, compute target), and example 05 walkthrough.

Quick reminder for 2D authors:

```c
rp.display_id = g_vd_id;              /* draw in VD pixel space */
SituationRenderVirtualDisplays(cmd);  /* composite to window */
```

Example: `examples/05_virtual_display_retro/`.

---

### Layering and Draw Order

2D has no z-buffer sorting for quads — **later draws appear on top**. Plan your call order:

1. Background clear (render pass load op)
2. World / game layer (VD or main pass)
3. Shadows (semi-transparent quads)
4. Sprites (back to front if overlapping)
5. UI panels
6. Text labels
7. HUD overlay (`SitExample_DrawHUD`)

For complex UI with many overlapping panels, either sort by explicit z or use separate Virtual Displays per layer.

---

### Alpha and Blending

- **Solid quads:** Set alpha in `Vector4 color`. `{1, 1, 1, 0.5f}` = 50% white overlay.
- **Textured sprites:** Set alpha in `ColorRGBA tint.a`. RGB channels also multiply the texture color.
- **VD compositing:** Controlled by `SituationBlendMode` at create/configure time.
- **Per-draw blend override:** Use [Advanced GPU commands](renderer_bolster.md) (`SituationCmdPushRasterState`, `SituationCmdSetBlendEnable`) for advanced cases — rarely needed for basic 2D.

Default 2D pipeline has alpha blending enabled for textured and colored quads.

---

### Quick Start Recipes

#### A — Animated color swatches + spinning square (`02_draw_shapes`)

See `examples/02_draw_shapes/main.c` for pulsing `draw_rect` swatches, `draw_rect_rot` with `SituationTimerGetTime()`, and labels.

#### B — Bouncing sprite

```c
SitRectangle src = {0, 0, 64, 64};
SitRectangle dst = {ball_x, ball_y, radius * 2, radius * 2};
SituationCmdDrawTexture(cmd, circle_tex, src, dst, (Vector2){{0,0}}, 0.0f, ball_tint);
```

Update `ball_x/y` with `SituationGetFrameTime()` and wall bounce logic.

#### C — Semi-transparent panel + clipped content

```c
draw_rect(cmd, 20, 20, 400, 300, (Vector4){{0.1f, 0.1f, 0.15f, 0.9f}});
SituationCmdSetScissor(cmd, 20, 20, 400, 300);
// ... draw list items ...
SituationCmdSetScissor(cmd, 0, 0, SituationGetRenderWidth(), SituationGetRenderHeight());
```

#### D — Retro CRT (`05_virtual_display_retro`)

Render all game art in 320×240 VD coordinates. Composite with `SituationRenderVirtualDisplays`. HUD drawn in window pass after composite.

---

### API Reference

---
#### `SituationCmdDrawQuad`
Solid colored rectangle (transformed unit quad).

```c
SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);
```

---
#### `SituationCmdDrawTexture`
Textured sprite with source rect, destination rect, pivot, rotation, and tint.

```c
SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd,
    SituationTexture texture, SitRectangle source, SitRectangle dest,
    Vector2 origin, float rotation, ColorRGBA tint);
```

---
#### `SituationCmdDrawTextureYpqGrade`
Textured draw with YPQ color grading in the fragment shader.

```c
SituationError SituationCmdDrawTextureYpqGrade(SituationCommandBuffer cmd,
    SituationTexture texture, SitRectangle source, SitRectangle dest,
    Vector2 origin, float rotation,
    float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

---
#### `SituationCmdSetScissor` / `SituationCmdSetScissorIndexed`
Clip subsequent draws to a rectangle.

```c
SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);
SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height);
```

---
#### `SituationCmdDrawText` / `SituationCmdDrawTextEx`
GPU text overlay. See [Fonts](font.md) and [Text Rendering](text_rendering.md).

---
#### `SituationDrawMetricsOverlay`
Multi-line FPS/latency/debug stats using the default font.

```c
void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color);
```

---
#### Virtual Display (2D layers)

| Function | Purpose |
|----------|---------|
| `SituationCreateVirtualDisplay` / `Ex` | Create off-screen target |
| `SituationRenderPassInfo.display_id` | Target a VD in begin render pass |
| `SituationRenderVirtualDisplays` | Composite all visible VDs to window |
| `SituationGetVirtualDisplayTexture` | Manual PiP / shader sampling |
| `SituationConfigureVirtualDisplay` | Offset, opacity, visibility, blend |
| `SituationSetVirtualDisplayScalingMode` | Change FIT / INTEGER / STRETCH at runtime |
| `SituationDestroyVirtualDisplay` | Cleanup |

---
#### `SitRectangle`

```c
typedef struct SitRectangle { float x, y, width, height; } SitRectangle;
```

Used by `SituationCmdDrawTexture`. Distinct from `Rectangle` in the Image module.

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Nothing draws | Outside render pass | Wrap in `BeginRenderPass` / `EndRenderPass` |
| Quads invisible | Zero-size matrix or alpha 0 | Check scale and `Vector4` alpha |
| Texture invisible | Invalid/stale handle | Verify `SituationCreateTexture` succeeded; check `generation != 0` |
| Blurry pixel art | LINEAR filtering or STRETCH scaling | Use `SITUATION_SCALING_INTEGER` for VDs; NEAREST on textures |
| Wrong position on HiDPI | Used logical screen size | Use `SituationGetRenderWidth/Height` |
| Clipped unexpectedly | Scissor not reset | Set scissor back to full screen after panel |
| Text works, quads don't | Init failure on quad renderer | Check `SituationInit` errors; need STB for text only |
| VD shows black | Wrong `display_id` in pass | Set `rp.display_id = vd_id` before begin pass |
| Rotation looks wrong | Pivot not set | Pass center offset in `origin` for `DrawTexture` |
| Draw order wrong | Later = on top | Reorder calls; draw background first |

**Legacy note:** Older docs reference `Rectangle` for `SituationCmdDrawTexture`. The API type is **`SitRectangle`**.

---

### Performance Tips

- Batch static UI: fewer large `DrawTexture` calls beat many tiny `DrawQuad` calls when possible.
- Reuse textures — do not `SituationCreateTexture` per frame.
- Virtual Displays with `frame_time_mult < 1.0` skip re-rendering when content is static (`is_dirty` flag).
- Default font + quads are the cheapest draw path (no texture bind for solid rects).
- For static HUD panels that never change, bake once to a texture and draw a single quad.
