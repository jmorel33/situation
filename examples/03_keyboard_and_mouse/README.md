# 03 — Keyboard and Mouse

**Tier:** Fundamental  
**Backends:** OpenGL + Vulkan

## What you see

A coloured square on a grid background. You control it with keyboard and mouse. The window title bar updates every frame with the square's position and current cursor coordinates.

## Controls

| Input | Action |
|-------|--------|
| WASD / Arrow keys | Move the square (frame-rate independent) |
| Mouse wheel | Resize the square |
| Left click | Teleport the square to the cursor |
| Right click (hold) | Drag the square with the mouse |
| Tab | Cycle through 5 preset colours |
| Space | Reset square to screen centre |

## What it teaches

| Concept | Function |
|---------|----------|
| Continuous key hold | `SituationIsKeyDown(SIT_KEY_W)` |
| Single-frame press | `SituationIsKeyPressed(SIT_KEY_TAB)` |
| Mouse position | `SituationGetMousePosition()` → `Vector2` |
| Mouse button hold | `SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_RIGHT)` |
| Mouse button click | `SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)` |
| Scroll wheel | `SituationGetMouseWheelMove()` → `float` (+up, −down) |
| Dynamic title bar | `SituationSetWindowTitle(char*)` |
| Frame-rate-independent movement | `SituationGetFrameTime()` × speed |

## Key concepts

**IsKeyDown vs IsKeyPressed:** `IsKeyDown` returns `true` every frame the key is physically held. `IsKeyPressed` returns `true` only on the single frame the key transitions from up to down. Use `IsKeyDown` for smooth movement, `IsKeyPressed` for discrete one-shot actions.

**O(1) ring-buffered input:** Situation's input system is ring-buffered — no event is ever dropped even during frame-rate spikes. `SituationIsKeyPressed` uses the per-frame snapshot so you never miss a short keypress.

**Frame-rate-independent movement:** Always multiply speed by `SituationGetFrameTime()`. At 30 FPS the square moves the same distance per second as at 144 FPS.

## Build

```bat
build\build_situation.bat static-opengl
build\build_examples.bat  static-opengl 03_keyboard_and_mouse
build\examples\03_keyboard_and_mouse.exe
```
