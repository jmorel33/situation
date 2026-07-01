# Situation Examples

This folder contains the canonical user-facing examples for the Situation library. Each example is self-contained, builds in a single command, and is designed to teach a specific concept clearly.

## Prerequisites

Build the static library once before building any example:

```bat
build\build_situation.bat static-opengl
REM or for Vulkan:
build\build_situation.bat static-vulkan
```

## Quick-build table

All examples support both backends unless noted. Replace `static-opengl` with `static-vulkan` for the Vulkan backend.

| # | Folder | Build command | What you see |
|---|--------|---------------|--------------|
| 1 | `01_open_a_window` | `build\build_examples.bat static-opengl 01_open_a_window` | A plain window; GPU/CPU/OS info printed to console |
| 2 | `02_draw_shapes` | `build\build_examples.bat static-opengl 02_draw_shapes` | Animated coloured quads and a bouncing ball |
| 3 | `03_keyboard_and_mouse` | `build\build_examples.bat static-opengl 03_keyboard_and_mouse` | A movable square with full keyboard + mouse control |
| 4 | `04_play_a_sound` | `build\build_examples.bat static-opengl 04_play_a_sound` | On-screen piano — procedural synth, no audio files |
| 5 | `05_virtual_display_retro` | `build\build_examples.bat static-opengl 05_virtual_display_retro` | Integer-scaled 320×240 CRT + corner minimap PiP |
| 6 | `06_audio_node_graph` | `build\build_examples.bat static-opengl 06_audio_node_graph` | Live ASCII audio chain — Tone → EQ → Reverb → Mixer |
| 7 | `07_ypq_color_grading` | `build\build_examples.bat static-opengl 07_ypq_color_grading` | Split-screen YPQ grade — color lab + GPU live preview |
| 8 | `08_temporal_oscillators` | `build\build_examples.bat static-opengl 08_temporal_oscillators` | 8-beat polyrhythm pulse machine — flash + tick on trigger |
| 9 | `09_midi_control` | `build\build_examples.bat static-opengl 09_midi_control` | MIDI port list, piano + CC bars, learn reverb room size (**MIDI hardware optional**) |
| 10 | `10_thread_pool` | `build\build_examples.bat static-opengl 10_thread_pool` | 64×64 Game of Life — serial vs `SituationDispatchParallel` speed compare |
| 18 | `18_text_showcase` | `build\build_examples.bat static-opengl 18_text_showcase` | Text API catalog — colors, sizes, spacing, animation |
| 19 | `19_node_graph_piano` | `build\build_examples.bat static-opengl 19_node_graph_piano` | Full synth piano — 8-node FX graph, QWERTY keyboard |
| 20 | `20_load_and_draw_model` | `build\build_examples.bat static-opengl 20_load_and_draw_model` | Utah teapot — OBJ/STL load, orbit camera, lit mesh |
| 21 | `21_ocean_realistic` | `build\build_examples.bat static-opengl 21_ocean_realistic` | **Advanced Shader** — clouds, calm/choppy ocean, camera travel *(G1 — see `examples/21_ocean_realistic/README.md`)* |
| 25 | `25_vd_standby` | `build\build_examples.bat static-opengl 25_vd_standby` | VD test-pattern explorer — snow, layers 0–8, stack order, per-layer params |
| 27 | `27_grid_playfield` | `build\build_examples.bat static-opengl grid_playfield` | Stacked grid playfield — Kenney tiles, scrolling BG + HUD FG on compute VD |

## Universal hotkeys

Every example includes `examples/shared/sit_example.h` which handles these keys automatically:

| Key | Action |
|-----|--------|
| `ESC` | Quit |
| `F11` | Toggle fullscreen / windowed |
| `F9` | Toggle VSync on / off |
| `P` | Toggle pause |
| `F12` | Save screenshot (`screenshot_NNNN.bmp`) |

## Tier structure

| Tier | Range | Purpose |
|------|-------|---------|
| 1 — Fundamental | 01–04 | Open a window, draw shapes, handle input, play audio |
| 2 — Feature Spotlight | 05–10 | Virtual display, audio node graph, YPQ color, oscillators, MIDI, threads |
| 3 — Showcase | 11–12 | Combined demos (planned) |
| 4 — Graphics & Compute | 13–17 | 3D, compute, hot-reload (planned) |
| 5 — Capstone | 18–20 | Text, piano synth, model loading |
| **Advanced Shader** | **21** | Fullscreen fragment flagship (ocean + clouds + travel) |
| **VD calibration** | **25** | Interactive test-pattern / standby explorer on a live VD |
| 6 — Full Demo | `demon_hunt`, `console` | Complete applications |

## Legacy examples

`examples/other/` contains ~85 development and integration test files. They are not user-facing documentation. The numbered examples above are the canonical starting point.

## Shared scaffolding

`examples/shared/sit_example.h` provides a thin wrapper that every numbered example uses:
- `SitExample_Init(argc, argv, title)` — 1024×768 window, VSync on, font loaded
- `SitExample_BeginFrame()` — polls input + timers + handles universal hotkeys
- `SitExample_DrawHUD(cmd, title, hint)` — draws the info bar at top and hint bar at bottom
- `SitExample_EndFrame()` — calls `SituationEndFrame()`
- `SitExample_Shutdown()` — unloads font, calls `SituationShutdown()`
- `_sit_ex_font` — the loaded IBM 8×8 bitmap font (available in all examples)
