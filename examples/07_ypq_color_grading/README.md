# 07 — YPQ Color Grading

**Tier:** Feature Spotlight  
**Backends:** OpenGL + Vulkan  
**Assets:** None — procedural 512×512 color laboratory

## What you see

A split-screen **color laboratory** designed to show off Situation's YPQ perceptual grading — something no other C game library ships out of the box.

| Region | Content |
|--------|---------|
| Top-left | Full **spectrum wheel** — watch phase spin chroma around the YIQ plane |
| Top-right | **Hue bars** with luma ramp — phase shifts bars differently than an HSV hue twist would |
| Bottom-left | **Macbeth-style chips** — skin tones, neutrals, saturated patches |
| Bottom-right | **Sunset gradient** — cinematic "film look" territory |

- **Left half:** original RGB (`SituationCmdDrawTexture`)
- **Right half:** live YPQ grade at 60 FPS (`SituationCmdDrawTextureYpqGrade` — GPU shader, zero CPU re-upload)
- **P–Q wheel:** phase as a vector in the chroma plane (not a hue dial)
- **Presets:** Teal/Orange, Bleach, Noir, Hyperpop

Press **TAB** for an automatic phase sweep — the fastest way to see why YPQ feels like a film grade, not a Photoshop hue slider.

## Controls

| Key | Parameter |
|-----|-----------|
| `Q` / `A` | Phase shift (degrees, hold) |
| `W` / `S` | Chroma scale (0–2.5×) |
| `E` / `D` | Luma scale (0.25–1.75×) |
| `R` / `F` | Mix (0 = original, 1 = full YPQ) |
| `1`–`4` | Cinematic presets |
| `TAB` | Toggle auto phase sweep |
| `SPACE` | Reset all parameters |
| `X` | Run `SituationYpqAnalyzeRgbMapping` (prints 256³ cube stats to console, ~few seconds) |

## What it teaches

| Concept | API |
|---------|-----|
| Procedural CPU image | `SituationCreateImage`, custom fill |
| CPU grade (offline/tools) | `SituationImageAdjustYPQ` |
| GPU grade (real-time) | `SituationCmdDrawTextureYpqGrade` |
| Upload once, grade live | `SituationCreateTexture` |
| Mapping quality diagnostics | `SituationYpqAnalyzeRgbMapping` |

## Why YPQ is not HSV

HSV rotates hue on a color wheel. **YPQ phase rotates chroma in the YIQ (P–Q) plane** while preserving the perceptual luma axis. On neutrals and skin tones the difference is subtle; on saturated patches and split-tones (teal shadows / orange highlights) the look is distinctly cinematic.

Roughly **33% of 8-bit RGB** is reachable from YPQ — the rest are "holes." Press `X` to print exact duplicate/hole counts from the library's full cube scan.

## Build and run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 07_ypq_color_grading
build\examples\07_ypq_color_grading.exe
```

Vulkan: swap `static-vulkan` in both build commands.

Short name: `ypq_color_grading`
