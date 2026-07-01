# Example 27 — Grid Playfield

**Tier:** Feature Spotlight  
**Backends:** OpenGL + Vulkan  
**Phase:** Grid render plan C.3 — stacked grids on a compute Virtual Display

## What you see

A **1280×960** playfield (20×15 cells at 64 px) built from **two stacked grids**:

1. **Background** — Kenney platformer tile atlas, horizontally scrolling (`SituationGridSetScroll`).
2. **Foreground** — static HUD labels (built-in VGA font) and a coin icon, alpha-blended on top.

The stack is rendered with `SituationGridStackPresent` into a **compute-target** VD, then **integer-scaled** to the host window via `SituationRenderVirtualDisplays`.

## Assets

Uses CC0 tiles from `examples/assets/kenney_new-platformer-pack-1.1/` ([Kenney.nl](https://kenney.nl)). Run the exe from the repo root (or one level up from `build/examples/`) so the relative asset path resolves.

## Controls

| Key | Action |
|-----|--------|
| `A` / `D` or `←` / `→` | Scroll background (hold Shift for faster) |
| `Space` | Toggle auto-scroll |
| `ESC` | Quit |
| `F11` | Fullscreen (playfield scales to fit; windowed = integer 1:1) |
| `F9` / `V` | Toggle VSync |
| `P` | Pause |
| `F12` | Screenshot |

## What it teaches

| Concept | API |
|---------|-----|
| Grid cell model (`code` + `fg` + `bg`) | `SitGridCell`, `SituationGridSetCell` |
| Tile atlas as grid font | `SituationLoadTexture`, `SituationLoadBitmapFontFromTexture`, `SituationGridSetFont` |
| Per-grid scroll | `SituationGridSetScroll` |
| Stack composite (bottom → top) | `SituationGridStackCreate`, `SituationGridStackAddGrid`, `SituationGridStackPresent` |
| Compute VD sized to grid pixels | `SituationCreateVirtualDisplayEx` + `SITUATION_VD_FLAG_COMPUTE_TARGET` |
| Integer scale to window | `SITUATION_SCALING_INTEGER`, `SituationRenderVirtualDisplays` |

Full guide: [`doc/guide/grid.md`](../../doc/guide/grid.md)

## Build and run

```bat
REM Prerequisites (once):
build\build_situation.bat static-opengl

REM Build:
build\build_examples.bat static-opengl grid_playfield

REM Run (from repo root):
build\examples\27_grid_playfield.exe
```

Vulkan:

```bat
build\build_situation.bat static-vulkan
build\build_examples.bat static-vulkan grid_playfield
```
