# Example 21 — Realistic Ocean (Advanced Shader)

Fullscreen procedural seascape: animated clouds, height-field ocean (calm ↔ storm), default camera travel.

**License:** MIT (Situation). **Visual inspiration only** — no Shadertoy source in this repo:

- [Shadertoy 4sXGRM](https://www.shadertoy.com/view/4sXGRM) (primary)
- [Shadertoy 4dSBDt](https://www.shadertoy.com/view/4dSBDt) (Thomas Schander)
- [Shadertoy MdXyzX](https://www.shadertoy.com/view/MdXyzX)

Host scaffolding follows `examples/other/shader_lab_raytrace2.c`.

## Build

```bat
build\build_examples.bat static-opengl 21_ocean_realistic
build\build_examples.bat static-vulkan 21_ocean_realistic
```

Run: `build\examples\21_ocean_realistic.exe`

## Controls

| Input | Action |
| ----- | ------ |
| *(default)* | Slow camera travel over the sea |
| **T** | Toggle travel / stationary orbit |
| **LMB** | Orbit override while held |
| **Wheel** | Travel speed (travel on) or zoom (travel off) |
| **Space** | Pause animation + travel |
| **1 / 2 / 3** | Sea: Calm / Moderate / Storm |
| **, / .** | Cloud coverage down / up |
| **[ / ]** | Fine-tune sea chop |
| **R** | Reset camera to path start |
| **V** | VSync |
| **F12** | PNG screenshot (`ocean_21_*.png`) |

## Shader tiers

This example ships **two fragment shaders** from one source, selected at compile time via `SITUATION_USE_VULKAN`:

| Build | Tier | Constraint |
| ----- | ---- | ---------- |
| `static-opengl` | **Budget** | OpenGL drivers enforce a ~65k **fragment instruction** limit per program — cloud/sea loops are trimmed to link reliably |
| `static-vulkan` | **Megashader** | No equivalent SPIR-V instruction ceiling — heavier cloud fBM, more march steps, GGX specular, sparkle field, subsurface tint |

Use **`static-vulkan`** for the flagship look; use **`static-opengl`** when you need the portable GL path.

## OpenGL note

If the GL build fails to link, the driver hit the fragment instruction budget — trim `CLOUD_MARCH_STEPS` / `SEA_COARSE` in the `#else` branch of `main.c`, or see `doc/plan/SHADERTOY_OCEAN_4dSBDt_PLAN.md` §OpenGL fragment instruction budget.

## Status

**G3 realism pass** — golden-angle radial waves, tiered height-field trace, cloud reflections from surface origin, stronger calm mirror specular, cloudier default sky. Mood-board PNGs still pending (`refs/NOTES.md`).