# Example 25 — VD Standby / Test Pattern Explorer

Interactive exploration of **`SitVdStandbyConfig`** on a live Virtual Display using the **production PATTERN compositor** — no custom shaders, no manual UBO/SSBO packing.

*Import calibration. Not draw calls.*

Full API reference: [`doc/guide/test_patterns.md`](../../doc/guide/test_patterns.md)

## Build

```bat
build\build_examples.bat static-opengl  25_vd_standby
build\build_examples.bat static-vulkan 25_vd_standby
```

Run: `build\examples\25_vd_standby.exe`

Requires a built library first:

```bat
build\build_situation.bat static-opengl
```

## What you see

A **1280×720** virtual display, letterboxed on a black host window. By default the VD is **idle** (no live content) and shows the **standby pattern compositor** output — animated snow when no calibration layers are enabled.

Status lines under the title bar show fallback mode, layer bitmask, selected layer, stack order, and active parameter readouts.

## Controls

### Layers and compose

| Key | Action |
|-----|--------|
| **0–8** | Toggle calibration layer (SMPTE … cube) |
| **-** | Clear all layers → animated **snow** |
| **C** | Toggle **chroma** RGB snow (when no layers 0–8) |
| **Tab** | Cycle selected layer (0–8) |
| **[ / ]** | Move selected layer down/up in **compose stack** |
| **Shift+1 … Shift+4** | Presets (see below) |
| **R** | Reset config to RGL defaults |

### Per-layer params (selected layer)

Hold **Shift** for coarse steps. **Q/A** and **W/S** nudge values (layer-dependent).

| Layer | Params |
|-------|--------|
| 0 SMPTE | content margins; **O** = overlay circle |
| 1 Checker | tile size X/Y |
| 2 Convergence | stripe width |
| 4 Grid | spacing px (0 = auto) |
| 5 PLUGE | safe margin |
| 6 Crosshatch | grid Nx/Ny |
| 7 Multiburst | band count (0–6) |
| 8 Cube | size |
| Snow | **N/M** = manual noise seed step |

Layer 3 (Gradients) is preset-only in this v1 example.

### Fallback modes and live demo

| Key | Action |
|-----|--------|
| **F** | Cycle **PATTERN → COLORBURST → SOLID → PATTERN** (SOLID = VD create default deep blue) |
| **Space** | Toggle trivial **live quad** on the VD (idle threshold 0.35 s → standby handoff) |

Universal hotkeys: **ESC** quit, **F11** fullscreen, **F9/V** VSync, **P** pause, **F12** screenshot.

## Presets (Shift + number)

| Preset | Effect |
|--------|--------|
| **Shift+1** | Snow only (`pattern_layers = 0`) |
| **Shift+2** | SMPTE bars only |
| **Shift+3** | Checker + SMPTE (default stack) |
| **Shift+4** | All layers 0–8 enabled |

## API demonstrated

- `SituationCreateVirtualDisplayEx`
- `SituationSetVirtualDisplayIdleThreshold`
- `SituationSetVirtualDisplayFallbackMode` / `SituationSetVirtualDisplayFallbackColor`
- `SituationSetVirtualDisplayPatternConfig`
- `SituationVdStandbyConfigInitDefaults`
- `SituationVdStandbyToggleLayer`
- `SituationRenderVirtualDisplays`
