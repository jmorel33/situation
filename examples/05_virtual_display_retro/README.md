# 05 — Virtual Display Retro CRT

**Tier:** Feature Spotlight  
**Backends:** OpenGL + Vulkan  
**Lines of code:** ~250 (excluding shared header)

## What you see

A **320×240** retro scene rendered into a Virtual Display, **integer-scaled** and letterboxed on a pitch-black host window. Inside the CRT:

- Animated horizontal **scanline bands**
- A **bouncing ball** (single quad sprite)
- Chunky **8 px bitmap text**

A second **160×120** Virtual Display powers a **corner minimap** (ball position on a simplified grid). The minimap is composited with **alpha blending** via `SituationGetVirtualDisplayTexture` + `SituationCmdDrawTexture` — a common PiP pattern when the main CRT uses integer centering.

## Controls

| Key | Action |
|-----|--------|
| `M` | Toggle minimap PiP |
| `B` | Toggle minimap frame glow (brighter alpha) |
| `ESC` | Quit |
| `F11` | Fullscreen |
| `F9` | Toggle VSync |
| `P` | Pause |
| `F12` | Screenshot |

## What it teaches

| Concept | API |
|---------|-----|
| Create a render target at fixed resolution | `SituationCreateVirtualDisplayEx` |
| Pixel-perfect upscale (no blur) | `SITUATION_SCALING_INTEGER` |
| Render into the VD | `SituationRenderPassInfo.display_id = vd_id` |
| Composite VDs onto the window | `SituationRenderVirtualDisplays` |
| Opaque CRT composite | `SITUATION_BLEND_NONE` on the main VD |
| Alpha PiP from a second VD | `SituationGetVirtualDisplayTexture`, `SituationCmdDrawTexture` |
| Hide a VD from auto-composite | `SituationConfigureVirtualDisplay(..., visible=false, ...)` |

## Why Virtual Displays?

Most C game libraries give you one window framebuffer. Situation's Virtual Display compositor lets you:

1. **Render at low resolution** (320×240) with crisp integer scaling — no shader hacks.
2. **Run multiple viewports** (game + minimap) without manual FBO plumbing.
3. **Composite with blend modes** (`SITUATION_BLEND_ALPHA`, additive, multiply, …).

SDL and raylib can do render-to-texture, but built-in integer-scale compositing with z-order and blend modes is unique to Situation's VD pipeline.

## Build and run

```bat
REM Prerequisites (once):
build\build_situation.bat static-opengl

REM Build:
build\build_examples.bat static-opengl 05_virtual_display_retro

REM Run:
build\examples\05_virtual_display_retro.exe
```

Vulkan variant:

```bat
build\build_situation.bat static-vulkan
build\build_examples.bat static-vulkan 05_virtual_display_retro
build\examples\05_virtual_display_retro.exe
```

Short name `virtual_display_retro` also works.
