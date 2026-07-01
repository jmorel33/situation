# 02 — Draw Shapes

**Tier:** Fundamental  
**Backends:** OpenGL + Vulkan

## What you see

- Six colour swatches, each breathing/pulsing at a different phase
- A spinning square in the centre of the screen, slowly cycling hue
- A bouncing ball that wraps between the screen edges
- Text labels on every shape

All animation is frame-rate-independent — it runs identically at 30 FPS, 60 FPS, or 144 FPS.

## What it teaches

| Concept | Function |
|---------|----------|
| Drawing a solid quad | `SituationCmdDrawQuad(cmd, mat4, Vector4)` |
| Drawing rotated geometry | Build a `mat4` with `glm_translate` + `glm_rotate_z` + `glm_scale` |
| Drawing text | `SituationCmdDrawTextEx(cmd, font, text, pos, size, spacing, colour)` |
| Frame-rate-independent time | `SituationGetFrameTime()` → `dt` |
| Continuous time for animation | `SituationTimerGetTime()` |
| `ColorRGBA` vs `Vector4` | `ColorRGBA` is 4×`uint8_t` (0–255); `Vector4` is 4×`float` (0–1) |

## Key API notes

**`SituationCmdDrawQuad`** is the universal 2D primitive. It takes a `mat4` transform that defines size and position in screen pixels. A 1×1 quad at `(0, 0)` becomes a 1-pixel dot; scale to `(width, height, 1)` after translating to position.

**`SituationCmdDrawTextEx`** vs **`SituationCmdDrawText`**: The `Ex` variant gives you size control and per-character spacing. `size` is the target glyph height in pixels; with the IBM 8×8 font, `size=8` is 1:1 pixel, `size=16` is double-scale.

The `_sit_ex_font` variable is the IBM 8×8 font loaded by `SitExample_Init`. Reference it directly in your example code.

## Build and run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat  static-opengl 02_draw_shapes
build\examples\02_draw_shapes.exe
```
