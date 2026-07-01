# Digestible Examples Plan

**Version:** 2.4.340 (rev. 2)  
**Status:** **Tier 1–2 complete** · Tier 3–5 planned · `examples/other/` audit catalogued  
**Goal:** Curated first-class examples — one folder per example, screenshot-worthy output, 15-minute readability — covering **Situation-only strengths** *and* the **baseline coverage** users expect from raylib/SDL-style libraries.

**Revision 2 (2026-06):** Tier 1 (01–04) and Tier 2 (05–10) shipped. This revision maps **raylib example categories** to gaps, expands the roadmap through **Tier 5**, and lists **`examples/other/` promotion candidates** so new numbered examples reuse proven code instead of reinventing it.

---

## Progress snapshot

| Tier | Range | Status |
|------|-------|--------|
| 1 — Fundamental | 01–04 | ✅ Shipped |
| 2 — Feature spotlight | 05–10 | ✅ Shipped (VD, graph, YPQ, oscillators, MIDI, threads) |
| 3 — Combined showcase | 11–12 | ⬜ Specified, not built |
| 4 — Graphics & compute | 13–17 | ⬜ New — fills raylib-like gaps with Situation flavor |
| 5 — Capstone apps | 18–20 | ✅ Shipped (text, piano, model) |
| Full demo | `demon_hunt`, `console` | ✅ Exist; stay Tier 6 |

**Still missing vs raylib:** 3D camera/mesh, shader labs, compute, texture/sprite depth, streaming audio, post-FX — plus Situation-only: **hot-reload workflow**, **HDR/10-bit output**, **K-Term** (console host only today).

---

## The Problem

`examples/other/` contains ~85 `.c` files with no hierarchy, no consistent structure, and no README. Most are dev/debug stubs (`dll_test.c`, `test_clear.c`, `window_debug_test.c`) that leak internal thinking rather than demonstrating the library. The handful of real examples (`demon_hunt`, `platformer_plumber`, `node_graph_piano_demo`) are buried alongside 70 files that have no business being in a user-facing examples folder.

New users landing on this project see either `hello_world.c` (which demonstrates text rotation on a dark blue background — mildly interesting) or `demon_hunt` (a full 1000-LOC Vulkan shooter they can't learn from). There is nothing in between.

---

## Design Principles

1. **One folder per example.** Each example gets `examples/<name>/`, a `<name>.c` source file, and a `README.md` explaining what it shows, how to build it, and what API calls are highlighted.
2. **Graduated complexity.** Six tiers: Fundamental → Feature Spotlight → Combined Showcase → Graphics & Compute → Capstone → Full Demo. Users enter at tier 1 and self-select upward.
3. **Genuinely interesting output.** Nothing that looks like a unit test. Every example should produce something worth screenshotting or recording.
4. **Lean on library strengths.** The library's unique differentiators — YPQ color, audio node graph, temporal oscillators, virtual display compositor, MIDI, NUMA threading — should each have a dedicated showcase that you won't find in SDL or raylib's example set.
5. **Self-contained assets.** All examples use procedurally generated data or embed their assets in headers. No external file dependencies. `tests/harness/assets/` images (`prairie.jpg`, `rosewood_veneer1.png`) may be used for image-processing examples where noted.
6. **Both backends.** Each example must compile with both `-DSITUATION_USE_OPENGL` and `-DSITUATION_USE_VULKAN` unless the example is explicitly demonstrating a Vulkan-specific feature (and says so prominently).
7. **Build script integration.** The `build/build_examples.bat` lookup must resolve each example by name (no path guessing required from the caller).
8. **Promote before rewrite.** If `examples/other/` already proves a concept, refactor into a numbered folder + `sit_example.h` rather than writing from scratch.

---

## Raylib coverage map (what users expect)

Raylib organizes examples by **core**, **textures**, **text**, **models**, **shaders**, **audio**, **others**. Situation should not clone that tree folder-for-folder, but **numbered examples should cover the same learning arcs** while leaning on differentiators.

| Raylib area | Typical examples | Situation today | Planned |
|-------------|------------------|-----------------|---------|
| **Core** — window, loop, input | `core_basic_window`, input tests | **01–03** ✅ | — |
| **Core** — 3D camera, picking | `core_3d_camera_*`, world/screen | `shader_lab_torus` in `other/` | **13** |
| **Shapes / 2D** | `shapes`, drawing | **02** ✅ | — |
| **Text** | font metrics, formatting | partial in 02; `text_showcase` in `other/` | **18** |
| **Textures** — sprites, painting | `textures_*` | guides only; tile quads in **12** | optional sprite add-on later |
| **Models** — load, animate | `models_*` | `loading_and_rendering_a_3d_model.c` | **20** |
| **Shaders** — lighting, post | `shaders_*` | `shader_lab_torus`, `shader_lab_raytrace2` | **13**, **15**; raytrace = Tier 6 |
| **Audio** — sound, stream | `audio_*` | **04, 06, 09** ✅ | **11**; stream note in **19** |
| **Audio graph / MIDI** | *(not in raylib)* | **06, 09** ✅; `node_graph_piano_demo.c` | **19** |
| **Threading** | *(minimal)* | **10** ✅ | — |
| **Compute / GPGPU** | *(not in raylib)* | `compute_shader_*`, `gpu_particle_*`, `digital_rain` | **14, 15, 17** |
| **Compositing / retro** | *(not in raylib)* | **05** ✅ | **12** dual-VD world; **25** VD pattern explorer *(planned P14)* |
| **YPQ / HDR color** | *(not in raylib)* | **07** ✅ | **16** |
| **Hot-reload** | *(not in raylib)* | — | **16** |
| **Terminal** | *(not in raylib)* | `console/`, `kterm_*` | Tier 6 only |

**Takeaway:** Tiers 1–2 nail Situation's personality. Tiers 3–5 add credibility (“normal 3D/compute/game stuff works too”) without chasing raylib's example count.

---

## Situation-only matrix (must stay prominent)

| Strength | Example | Status |
|----------|---------|--------|
| Virtual Display compositor | 05, 12, **25** *(planned)* | 05 ✅ |
| Audio node graph | 06, 11, 19 | 06 ✅ |
| YPQ color science | 07, 11, 12, 16 | 07 ✅ |
| Temporal oscillators | 08, 11, 17 | 08 ✅ |
| MIDI + learn | 09 | 09 ✅ |
| NUMA / thread pool | 10 | 10 ✅ |
| Vulkan + GL parity | all numbered | ✅ |
| Compute + barriers | 14, 15 | planned |
| HDR10 / 10-bit output | 16 | planned |
| Hot-reload (I/O thread) | 16 | planned |
| K-Term + compute terminal | `console/` | Tier 6 |

---

## Folder Layout (Target State)

```
examples/
├── README.md                        ← Index / quick-start guide
├── demon_hunt/                      ← Existing (keep, already well-structured)
│   ├── demon_hunt.c
│   └── ...
├── console/                         ← Existing K-Term console app (keep)
│   └── ...
│
│   ─── TIER 1: FUNDAMENTAL ──────────────────────────────────────────────────
├── 01_open_a_window/
│   ├── main.c
│   └── README.md
├── 02_draw_shapes/
│   ├── main.c
│   └── README.md
├── 03_keyboard_and_mouse/
│   ├── main.c
│   └── README.md
├── 04_play_a_sound/
│   ├── main.c
│   └── README.md
│
│   ─── TIER 2: FEATURE SPOTLIGHT ─────────────────────────────────────────
├── 05_virtual_display_retro/
│   ├── main.c
│   └── README.md
├── 06_audio_node_graph/
│   ├── main.c
│   └── README.md
├── 07_ypq_color_grading/
│   ├── main.c
│   └── README.md
├── 08_temporal_oscillators/
│   ├── main.c
│   └── README.md
├── 09_midi_control/
│   ├── main.c
│   └── README.md
├── 10_thread_pool/
│   ├── main.c
│   └── README.md
│
│   ─── TIER 3: SHOWCASE (combined personality) ─────────────────────────────
├── 11_music_visualizer/
│   ├── main.c
│   └── README.md
├── 12_procedural_world/
│   ├── main.c
│   └── README.md
│
│   ─── TIER 4: GRAPHICS & COMPUTE (raylib-parity + Situation depth) ───────
├── 13_shader_lab_3d/
│   ├── main.c                      ← refactor from other/shader_lab_torus.c
│   └── README.md
├── 14_gpu_particles/
│   ├── main.c                      ← refactor from other/gpu_particle_simulation.c
│   └── README.md
├── 15_compute_image_fx/
│   ├── main.c                      ← refactor from other/compute_shader_image_processing.c
│   └── README.md
├── 16_hot_reload_and_hd_color/
│   ├── main.c                      ← new (shader hot-reload + optional HDR ramp)
│   └── README.md
├── 17_digital_rain/
│   ├── main.c                      ← refactor from other/digital_rain.c
│   └── README.md
│
│   ─── TIER 5: CAPSTONE (small complete experiences) ─────────────────────
├── 18_text_showcase/
│   ├── main.c                      ← refactor from other/text_showcase.c
│   └── README.md
├── 19_node_graph_piano/
│   ├── main.c                      ← refactor from other/node_graph_piano_demo.c
│   └── README.md
├── 20_load_and_draw_model/
│   ├── main.c                      ← refactor from other/loading_and_rendering_a_3d_model.c
│   └── README.md
│
│   ─── TIER 6: FULL DEMO (existing, retained) ────────────────────────────
├── demon_hunt/
├── console/
└── other/                          ← legacy lab; see promotion catalog below
    ├── shader_lab_raytrace2.c        ← flagship shader demo (build by name)
    ├── platformer_plumber.c          ← optional future 21 or Tier 6 link
    └── … (~85 files, dev stubs + promoted sources)
```

---

## Example Specifications

Each spec covers: what the user sees/hears, which API features are demonstrated, complexity tier, backend compatibility, and any assets needed.

---

### 01 — Open a Window

**Folder:** `examples/01_open_a_window/`  
**Tier:** 1 — Fundamental  
**Backends:** OpenGL + Vulkan

**Output:** A 1280×720 window with a solid dark-slate background. Title bar shows `"Situation 2.4 — Hello"`. Window is resizable. Console prints GPU name and VRAM on startup.

**What it teaches:**
- `SituationInit` / `SituationShutdown` lifecycle
- `SITUATION_BEGIN_FRAME()` macro
- `SituationAcquireFrameCommandBuffer` + `SituationEndFrame`
- `SituationCmdBeginRenderPass` with a clear color
- `SituationWindowShouldClose` loop
- `SituationGetGpuName` / `SituationGetVideoMemoryUsed`

**Source length target:** ~60 lines. No math deps. Every line commented.

---

### 02 — Draw Shapes

**Folder:** `examples/02_draw_shapes/`  
**Tier:** 1 — Fundamental  
**Backends:** OpenGL + Vulkan

**Output:** Animated frame. Several colored quads and text labels laid out on screen. One quad pulses in scale using `SituationGetFrameTime`. Text labels identify each shape using the embedded VGA bitmap font.

**What it teaches:**
- `SituationCmdDrawQuad` / `SituationCmdDrawText`
- `SituationGetFrameTime` for animation
- `SituationLoadBitmapFontFromMemory` with the embedded `ibm_font_8x8`
- `ColorRGBA` / `Vector2` / `Rectangle` types
- Render pass `SIT_LOAD_OP_CLEAR`

**Source length target:** ~120 lines. Includes `font_data.h` (already exists).

---

### 03 — Keyboard and Mouse

**Folder:** `examples/03_keyboard_and_mouse/`  
**Tier:** 1 — Fundamental  
**Backends:** OpenGL + Vulkan

**Output:** A colored square that the user moves with WASD. Mouse position shown as a crosshair cursor drawn with quads. Left-click toggles the square's color between two presets. The window title updates every frame with current cursor coordinates.

**What it teaches:**
- `SituationIsKeyDown` / `SituationIsKeyPressed`
- `SituationGetMousePosition` / `SituationIsMouseButtonDown`
- `SituationSetWindowTitle` with `snprintf`
- Frame-rate-independent movement via `SituationGetFrameTime`
- The input ring-buffer guarantees (mention in README: no drops even under frame spikes)

**Source length target:** ~130 lines.

---

### 04 — Play a Sound

**Folder:** `examples/04_play_a_sound/`  
**Tier:** 1 — Fundamental  
**Backends:** OpenGL + Vulkan

**Output:** Window with instructions printed to screen. Press `1`–`5` to trigger tones of different pitches (C, E, G, high-C, high-E). Press `R` to play a reverb-wet chord (all three notes simultaneously). Tones fade out via ADSR hold/release.

**What it teaches:**
- `SituationPlayToneEx` — wave type, frequency, volume, pan, ADSR parameters
- `SITUATION_MIDI_NOTE_FREQUENCY` lookup table
- `SituationStopAllTones`
- Audio routing: tone pool → output
- No audio files needed — 100% procedural

**Source length target:** ~100 lines.  
**Note:** This is arguably the best "first audio example" for a library with this much audio power — purely procedural, zero assets, immediately satisfying.

---

### 05 — Virtual Display: Retro CRT

**Folder:** `examples/05_virtual_display_retro/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** A 320×240 virtual display letterboxed inside the 1280×720 window (integer scaling). On the virtual display: a procedurally animated scanline-style scene (moving horizontal bands of color, a simple bouncing ball sprite drawn with quads, pixel-art-style text). The host window background is pitch black, making the crisp integer-scaled VD the entire visual focus. A second small 160×120 VD in the corner shows a "mini-map" version with different blend mode.

**What it teaches:**
- `SituationCreateVirtualDisplayEx` with `SITUATION_VD_SCALE_INTEGER`
- Rendering to a VD (`display_id` in `SituationRenderPassInfo`)
- Compositing VDs back to the main window
- `SITUATION_VD_BLEND_ALPHA`
- Why VDs matter: pixel-perfect retro rendering, multi-viewport, render-to-texture compositing patterns

**Source length target:** ~200 lines.  
**Uniqueness:** No other C game library has this as a built-in compositing primitive. This example should be in the README hero section.

---

### 06 — Audio Node Graph

**Folder:** `examples/06_audio_node_graph/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** A window showing a live ASCII/text visualization of the audio processing chain drawn with `SituationCmdDrawText`. The chain: Tone Synth → EQ 4-Band → Reverb → Mixer → output. Press keys to:
- `Q/W/E` — change tone synth waveform (sine/square/sawtooth)
- `↑↓` — adjust reverb room size in real time via `SituationSetControl`
- `1/2/3` — switch between preset frequencies (220Hz, 440Hz, 880Hz)
- `S` — save the current graph to `graph_session.json` (`SituationSerializeGraph`)
- `L` — load it back (`SituationDeserializeGraph`)

The ASCII diagram updates to reflect current control values each frame.

**What it teaches:**
- `SituationInitDeviceRegistry`
- `SituationCreateGraph` / `SituationCreateNodeWithDevice`
- `SituationCreatePatch` — connecting nodes
- `SituationSetControl` — live parameter adjustment
- `SituationSetActiveGraph` — routing to audio output
- `SituationTopologicalSort`
- Graph JSON serialization/deserialization
- Cycle detection (try patching a loop — prints error)

**Source length target:** ~280 lines.  
**Uniqueness:** A DAW-style patching system in a game library. Rivals miniaudio's node graph but is more exposed and more controllable.

---

### 07 — YPQ Color Grading

**Folder:** `examples/07_ypq_color_grading/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** A split-screen. Left half: a procedurally generated gradient image (a 512×512 CPU image with smooth RGB bands). Right half: the same image processed through `SituationImageAdjustYPQ` with user-controllable parameters. Four sliders controlled by keyboard:
- `Q/A` — phase shift (+/−)
- `W/S` — chroma scale (+/−)
- `E/D` — luma scale (+/−)
- `R/F` — mix (0=original, 1=full YPQ)

Current YPQ parameter values printed on screen. Press `X` to call `SituationYpqAnalyzeRgbMapping` and print the diagnostics to the console.

**What it teaches:**
- `SituationCreateImage` / `SituationGenImageGradient` (CPU image generation)
- `SituationImageAdjustYPQ` — what phase/chroma/luma do visually
- `SituationCreateTexture` from a CPU image + `SituationUpdateTexture` for live changes
- `SituationYpqAnalyzeRgbMapping` diagnostics
- The YIQ perceptual model (explain in README: why chroma phase ≠ hue shift)

**Source length target:** ~220 lines.  
**Uniqueness:** This doesn't exist anywhere else. YPQ is a novel perceptual color encoding. The example turns it into something tangible and interactive.

---

### 08 — Temporal Oscillators: Beat-Sync Events

**Folder:** `examples/08_temporal_oscillators/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** A visual metronome / beat-sync demo. 8 colored circles on screen, each driven by a different oscillator at a different period (1/4 beat, 1/2 beat, 1 beat, 2 beats, etc. relative to 120 BPM). When an oscillator triggers, its circle flashes white and plays a short tone. The "feel" is a polyrhythmic pulse machine. Press `+/-` to adjust BPM (recalculates all oscillator periods). Press `1`–`8` to mute individual oscillators.

**What it teaches:**
- `SituationSetTimerOscillatorPeriod`
- `SituationTimerGetOscillatorTriggerCount` — edge detection pattern
- `SituationGetFrameTime` + `SituationUpdateTimers`
- `SituationPlayToneEx` for percussive clicks triggered by oscillators
- The oscillator pattern for music-reactive game logic (explain in README)

**Source length target:** ~180 lines.  
**Uniqueness:** 256 independent metronomes is unusual even in audio middleware. Most game libraries give you "delta time". This example makes the oscillator system legible.

---

### 09 — MIDI Control

**Folder:** `examples/09_midi_control/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** Requires a connected MIDI device (keyboard or controller). If no device is found, prints available ports and exits gracefully with instructions. When connected: incoming MIDI notes trigger `SituationPlayToneEx` tones (frequency from `SITUATION_MIDI_NOTE_FREQUENCY[]`), visualized as piano keys lighting up on screen (drawn with quads). CC messages draw their value as horizontal bars. Press `L` to activate MIDI learn mode — "wiggle a knob to assign it to reverb room size".

**What it teaches:**
- `SituationEnumerateMidiDevices` / `SituationOpenMidiDevice`
- MIDI note-on/off callback pattern
- `SITUATION_MIDI_NOTE_FREQUENCY[]` lookup
- `SituationSetMidiLearnCallback` — the learn workflow
- 14-bit CC (if device supports it — mention in README)
- Graceful fallback when no device present

**Source length target:** ~250 lines.  
**Note:** Mark as "requires MIDI hardware" in the README index. Still builds and runs without one — just shows the device list.

---

### 10 — Thread Pool: Parallel Work

**Folder:** `examples/10_thread_pool/`  
**Tier:** 2 — Feature Spotlight  
**Backends:** OpenGL + Vulkan

**Output:** A 64×64 grid of cells on screen. Each frame, a "Game of Life" or reaction-diffusion update is dispatched in parallel across the grid using `SituationDispatchParallel`. The result is uploaded as a texture and drawn. A frame counter and "jobs dispatched" count is shown. Pressing `T` toggles between serial (main thread) and parallel (thread pool) to show the speedup on a CPU-heavy update.

**What it teaches:**
- `SituationCreateThreadPool` / `SituationDestroyThreadPool`
- `SituationDispatchParallel` — fork-join over a grid
- `SituationSubmitJobEx` with dependencies
- Thread safety: update buffers before draw commands (the "update-before-draw" rule)
- `SituationGetThreadPoolSnapshot` — print core utilization

**Source length target:** ~220 lines.  
**Note:** Game of Life is the classic parallel CPU demo — visually obvious when parallelism is on vs off.

---

### 11 — Music Visualizer

**Folder:** `examples/11_music_visualizer/`  
**Tier:** 3 — Showcase  
**Backends:** OpenGL + Vulkan

**Output:** A fullscreen music visualizer. The audio chain: Tone Synth → Spectrum Analyzer node → Peak Meter node → Mastering Amp → output. A procedural melody (Pentatonic scale, stepped by oscillator at 120 BPM) drives the tone synth. The spectrum analyzer's frequency bins are read each frame and drawn as vertical bars with YPQ-colored gradients (bars shift hue based on amplitude). The peak meter drives a luma pulse on the background. Result: a pulsing, colorful audio visualizer with zero external files.

**What it teaches (combined demo of):**
- Full audio node graph setup with multiple nodes
- `SITUATION_NODE_SPECTRUM_ANALYZER` — reading frequency bins
- `SITUATION_NODE_PEAK_METER` — reading peak levels
- YPQ color: mapping audio amplitude to chroma/luma
- Temporal oscillators for note triggering
- `SituationCmdDrawQuad` for bar visualization

**Source length target:** ~350 lines.  
**Uniqueness:** Spectrum analysis + YPQ color + temporal oscillators + node graph — all four unique features in one demo. This is the "wow" example that shows the library's personality.

---

### 12 — Procedural World

**Folder:** `examples/12_procedural_world/`  
**Tier:** 3 — Showcase  
**Backends:** OpenGL + Vulkan

**Output:** A top-down 2D "world view" rendered to a virtual display (320×240, integer scaled). The world is a procedurally generated tilemap (using a simple noise function, no external lib). Tiles are drawn as colored quads on the VD. The player character is a small square moved with WASD. A minimap in the corner (second VD, 80×60) shows a zoomed-out view. Day/night cycle: a global YPQ phase shift applied to the VD's luma simulates time-of-day color grading. Footstep sounds triggered by oscillator when moving.

**What it teaches (combined showcase):**
- Virtual display as a rendering target for a game viewport
- Dual VD compositing (main view + minimap)
- `SituationImageAdjustYPQ` on a texture (or GPU YPQ via the grade shader) for day/night
- Input + `SituationGetFrameTime` for movement
- Oscillator-triggered audio for footsteps
- Tilemap rendering with quads

**Source length target:** ~400 lines.  
**Note:** No 3D, no models, no external assets. Pure API showcase of what you'd actually build a 2D game with.

---

### 13 — Shader Lab 3D

**Folder:** `examples/13_shader_lab_3d/`  
**Tier:** 4 — Graphics & Compute  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/shader_lab_torus.c`

**Output:** A lit, rotating torus (or cube fallback) with orbit camera (mouse drag + scroll). Phong-style lighting in fragment shader; wireframe toggle. Window title shows backend and FPS.

**What it teaches:**
- 3D projection + view matrix (cglm)
- Vertex/index buffers, depth test
- Runtime GLSL compile (`SITUATION_ENABLE_SHADER_COMPILER`)
- First-person / orbit camera pattern (raylib `core_3d_camera_*` arc)

**Refactor notes:** Strip dev cruft; adopt `sit_example.h`; keep under ~250 lines if possible.

---

### 14 — GPU Particles

**Folder:** `examples/14_gpu_particles/`  
**Tier:** 4  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/gpu_particle_simulation.c`

**Output:** ~100k particles: compute updates physics, instanced quads render. Gravity well at cursor. Obvious GPU vs CPU contrast when pausing compute.

**What it teaches:**
- SSBO shared as VBO
- Compute dispatch + pipeline barrier before draw
- Instanced rendering
- See `doc/guide/compute.md`

**Refactor notes:** Already well-commented; add README with barrier diagram.

---

### 15 — Compute Image FX

**Folder:** `examples/15_compute_image_fx/`  
**Tier:** 4  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/compute_shader_image_processing.c`

**Output:** Load `tests/harness/assets/prairie.jpg` (or embedded fallback), run compute blur/tonemap pass, display result fullscreen. Toggle kernel size with keys.

**What it teaches:**
- SSBO / storage image workflow
- Dispatch 2D grid
- Optional readback for validation
- Parallels raylib `shaders_postprocessing` but compute-first

**Asset policy:** First numbered example allowed to use harness JPEG (document path in README).

---

### 16 — Hot Reload & HD Color

**Folder:** `examples/16_hot_reload_and_hd_color/`  
**Tier:** 4  
**Backends:** OpenGL + Vulkan  
**Promote from:** *(new — no `other/` source)*

**Output:** Fullscreen gradient driven by a fragment shader. **F5** hot-reloads `shader.frag` from disk (via hot-reload API). **H** toggles HDR10 request when caps allow (`SituationSetOutputColorDepth` / env). On-screen banner shows reload count and active color depth.

**What it teaches:**
- Hot-reload thread + file watch workflow (`doc/guide/hot_reload.md`)
- HDR / 10-bit policy (`doc/guide/hd_color_output.md`)
- Shader recompile without restart

**Source length target:** ~200 lines.

---

### 17 — Digital Rain

**Folder:** `examples/17_digital_rain/`  
**Tier:** 4  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/digital_rain.c`

**Output:** Matrix-style falling glyphs on VD or fullscreen; optional oscillator-driven “modem” bed in background (keep subtle). YPQ tint on columns by speed.

**What it teaches:**
- Compute or CPU glyph field + texture upload
- VD optional (integer scale for retro look)
- Combining **08** oscillators with visual demo

**Refactor notes:** `digital_rain.c` may be long — split sim vs render; trim duplicate window setup.

---

### 18 — Text Showcase

**Folder:** `examples/18_text_showcase/`  
**Tier:** 5 — Capstone  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/text_showcase.c`

**Output:** Font sizes, alignment, wrapped paragraphs, colored runs, rotation — one screen catalog.

**What it teaches:**
- `SituationDrawText*` family
- Font loading / default font
- Layout patterns (raylib text examples arc)

---

### 19 — Node Graph Piano

**Folder:** `examples/19_node_graph_piano/`  
**Tier:** 5  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/node_graph_piano_demo.c`

**Output:** On-screen piano keyboard; click keys or use QWERTY map. Richer graph than **06**: multiple oscillators, filters, optional reverb node. Visual graph debug overlay optional.

**What it teaches:**
- Advanced audio graph wiring
- Real-time parameter changes
- Bridges **06** → **11** for users who want “instrument” not “beep”

**Refactor notes:** Largest promotion candidate; may stay ~400+ lines — that's OK for Tier 5.

---

### 20 — Load and Draw Model

**Folder:** `examples/20_load_and_draw_model/`  
**Tier:** 5  
**Backends:** OpenGL + Vulkan  
**Promote from:** `examples/other/loading_and_rendering_a_3d_model.c`

**Output:** Load embedded or bundled `.obj` (minimal teapot/plane), lit draw loop, rotate model.

**What it teaches:**
- Model load API
- Mesh + material basics
- Raylib `models_*` learning arc

**Asset policy:** Embed mesh as header or ship `model.obj` inside example folder (exception to “no external files” for one small OBJ).

---

### 21 — Ocean Realistic (Advanced Shader)

**Folder:** `examples/21_ocean_realistic/`  
**Category:** **Advanced Shader**  
**Tier:** Advanced Shader *(numbered extension after Tier 5)*  
**Backends:** OpenGL + Vulkan (`static-opengl` / `static-vulkan`)  
**Plan:** [`SHADERTOY_OCEAN_4dSBDt_PLAN.md`](SHADERTOY_OCEAN_4dSBDt_PLAN.md) — original MIT GLSL; Shadertoy inspiration only (`4sXGRM`, `4dSBDt`, `MdXyzX`)

**Output:** Fullscreen ocean + **clouds**, **calm ↔ choppy** sea presets, default **camera travel**; orbit override and pause.

**What it teaches:**
- Large single-pass fragment programs in Situation (load from memory, dual GL/VK uniform path)
- Ray / height-field water + sky–sea composition
- **OpenGL ~65k fragment instruction budget** — design for link success on consumer drivers
- Contrasts with **13** (mesh Phong) and Tier 6 `shader_lab_raytrace2` (jewel-box scene in `other/`)

**Build:**

```bat
build\build_examples.bat static-opengl 21_ocean_realistic
build\build_examples.bat static-vulkan 21_ocean_realistic
```

**Slot:** Reserves **21** for this demo; `platformer_plumber` deferred to Tier 6 or a later number.

---

### 25 — VD Standby / Test Pattern Explorer

**Folder:** `examples/25_vd_standby/` *(planned — **P14**, spec in `doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md` §6.4)*  
**Tier:** 2 — Feature spotlight (Virtual Display + calibration)  
**Backends:** OpenGL + Vulkan  
**Status:** 📋 **Shipped** — `examples/25_vd_standby/` (P14)

**Output:** A **1280×720 Virtual Display** integer-scaled on a black host window, always showing the **built-in PATTERN standby compositor**. No custom shaders — the example drives **`SitVdStandbyConfig`** through the public VD API and updates the image in real time.

**What the user explores (one session):**

| Section | Interaction |
|---------|-------------|
| **Snow** | Clear all layers (`-`); toggle chroma (`C`); animated `noise_frame_seed` |
| **Layers 0–8** | Keys `0`–`8` toggle SMPTE, checker, convergence, gradients, grid, PLUGE, crosshatch, multiburst, cube |
| **Stack** | `[` / `]` reorder selected layer; default stack matches harness compose tests |
| **Params** | `Tab` selects layer; `Q`/`A`/`W`/`S` nudge active layer fields (tile size, margins, stripe width, …) |
| **Fallbacks** | `F` cycles PATTERN / COLORBURST / SOLID (contrast with full compositor path) |
| **Presets** | Shift+`1`–`4` — snow, SMPTE-only, checker+SMPTE, full stack |

**What it teaches:**

- Production integration: `SituationSetVirtualDisplayPatternConfig` — not harness UBO packing
- `SitVdStandbyConfig` unified struct (stack + nine layer param blocks + snow)
- Difference between **zero-layer snow**, **single-bit preset**, and **multi-bit stack compose**
- How VD idle standby is the product path for calibration cards (*Import calibration. Not draw calls.*)

**Build (once implemented):**

```bat
build\build_examples.bat static-opengl  25_vd_standby
build\build_examples.bat static-vulkan 25_vd_standby
```

**Supersedes:** `examples/other/vd_idle_standby_demo.c` (idle-only SOLID/COLORBURST slice) and the user-facing goals of P6 `shader_lab_test_patterns.c`.

**Source length target:** ~400 lines in `main.c` + README key map.

---

## Appendix A — `examples/other/` promotion catalog

| File | Verdict | Target | Notes |
|------|---------|--------|-------|
| `shader_lab_torus.c` | **Promote** | 13 | Primary 3D shader lab |
| `gpu_particle_simulation.c` | **Promote** | 14 | Compute flagship |
| `compute_shader_image_processing.c` | **Promote** | 15 | Image compute |
| `digital_rain.c` | **Promote** | 17 | Visual + optional audio |
| `text_showcase.c` | **Promote** | 18 | Text API catalog |
| `node_graph_piano_demo.c` | **Promote** | 19 | Rich audio graph |
| `loading_and_rendering_a_3d_model.c` | **Promote** | 20 | Model loading |
| `shader_lab_raytrace2.c` | **Tier 6 link** | README only | Jewel-box raytrace; build by name |
| *(new)* ocean realistic | **Promote** | **21** | Advanced Shader — see `SHADERTOY_OCEAN_4dSBDt_PLAN.md` |
| `platformer_plumber.c` | **Defer** | Tier 6 or later # | Was candidate for 21; slot taken by ocean |
| `playing_background_music.c` | **Merge** | 04 README or 19 | Streaming API snippet |
| `vd_idle_standby_demo.c` | **Promote → 25** | **25_vd_standby** | P14 expands idle demo into full pattern explorer |
| `graph_load_demo.c`, `graph_save_demo.c` | **Merge** | 06 README | Persistence sidecar |
| `kterm_showcase.c`, `kterm_simple_test.c` | **Tier 6** | `console/` docs | Terminal host |
| `basic_triangle.c`, `vertex_pull_triangle.c` | **Stay** | `other/` | Low-level GL/VK tests |
| `mandelbrot.c` | **Stay** | `other/` | CPU classic; low priority |
| `dll_test.c`, `test_clear.c`, `window_debug_test.c` | **Stay** | dev only | Not user-facing |
| MIDI test harnesses (`midi_*`) | **Stay** | covered by 09 | |
| `hello_window.c`, `handling_keyboard...` | **Superseded** | 01–03 | Keep for grep reference |

**After promotion:** Leave original `.c` in `other/` with one-line header comment: `/* Superseded by examples/NN_name/ — kept for dev reference */`

---

## Appendix B — Tier 6 (full demos, not numbered)

| Path | Role |
|------|------|
| `examples/demon_hunt/` | Vulkan-heavy game; README warns “read 01–10 first” |
| `examples/console/` | K-Term host; links `kterm_showcase.c` patterns |
| `examples/other/shader_lab_raytrace2.c` | Flagship raymarch; `-mwindows`; screenshot hero |

**21_ocean_realistic** — Advanced Shader flagship (planned). `platformer_plumber` → Tier 6 or later number.

---

## Build Script Changes Required

`build/build_examples.bat` currently does a flat lookup: `examples/other/<name>.c` for everything except `demon_hunt` and `kterm_console`. The new numbered-folder structure requires updating the path resolution logic.

### Change: Source Path Resolution

Replace the current flat-file lookup section with a folder-aware lookup:

```bat
REM --- Resolve source file path ---
set "EXAMPLE_SRC=examples\other\%EXAMPLE%.c"

REM Numbered examples (new structure)
for /d %%d in (examples\[0-9][0-9]_*) do (
    for %%f in ("%%d\main.c") do (
        set "DIR_NAME=%%~nxd"
        set "SHORT_NAME=!DIR_NAME:~3!"
        if /i "!SHORT_NAME!"=="%EXAMPLE%" set "EXAMPLE_SRC=%%d\main.c"
        REM Also support full folder name: e.g. "01_open_a_window"
        if /i "!DIR_NAME!"=="%EXAMPLE%" set "EXAMPLE_SRC=%%d\main.c"
    )
)

REM Named subdirectory examples (demon_hunt, console, etc.)
if /i "%EXAMPLE%"=="demon_hunt"   set "EXAMPLE_SRC=examples\demon_hunt\demon_hunt.c"
if /i "%EXAMPLE%"=="kterm_console" set "EXAMPLE_SRC=examples\console\console_host_app.c"
```

This means examples can be invoked by short name or full folder name:
```bat
build_examples.bat opengl open_a_window
build_examples.bat opengl 01_open_a_window
```

### Change: `-mwindows` Flag

The new showcase examples (11–20) should also get `-mwindows` since they are real windowed apps. Add them to the EXTRA_LDFLAGS section alongside `platformer_plumber`.

---

## Examples Index README

`examples/README.md` — a one-page index with build commands for each example, what it demonstrates, and the tier. This is the first file a new user reads.

**Sections:**
1. Prerequisites (one-liner: build the static lib)
2. Quick-build table (name → `build_examples.bat static-opengl <name>` → what you see)
3. Tier description (1 → 6)
4. Note about `examples/other/` (legacy + promotion sources; not indexed)
5. Link to Tier 6 demos (`demon_hunt`, `console`, `shader_lab_raytrace2`)

---

## Migration of Existing Examples

The 85 files in `examples/other/` are not deleted. The plan is:

- **Keep as-is.** `examples/other/` stays. It is not linked from the README index. It's available for devs who need to reference implementation patterns.
- **The new numbered folders are the canonical user-facing examples.** The README and `doc/introduction.md` link only to the numbered set.
- **Eventually** (separate plan), `examples/other/` can be audited and pruned. That is not in scope here.

---

## Task Checklist

### Infrastructure
- [x] Update `build/build_examples.bat` — source path resolution for numbered folders
- [x] Update `build/build_examples.bat` — add `-Iexamples/shared` and `-Iexamples/other` to all four build modes
- [x] Update `build/build_examples.bat` — add numbered showcase examples to `-mwindows` list
- [x] Create `examples/shared/sit_example.h` — common scaffolding header
- [x] Create `examples/README.md` — index page

### Tier 1 — Fundamental
- [x] `examples/01_open_a_window/main.c` + `README.md`
- [x] `examples/02_draw_shapes/main.c` + `README.md`
- [x] `examples/03_keyboard_and_mouse/main.c` + `README.md`
- [x] `examples/04_play_a_sound/main.c` + `README.md`

### Tier 2 — Feature Spotlight
- [x] `examples/05_virtual_display_retro/main.c` + `README.md`
- [x] `examples/06_audio_node_graph/main.c` + `README.md`
- [x] `examples/07_ypq_color_grading/main.c` + `README.md`
- [x] `examples/08_temporal_oscillators/main.c` + `README.md`
- [x] `examples/09_midi_control/main.c` + `README.md`
- [x] `examples/10_thread_pool/main.c` + `README.md`

### Tier 3 — Combined Showcase
- [ ] `examples/11_music_visualizer/main.c` + `README.md`
- [ ] `examples/12_procedural_world/main.c` + `README.md`

### Tier 4 — Graphics & Compute
- [ ] `examples/13_shader_lab_3d/main.c` + `README.md` ← from `shader_lab_torus.c`
- [ ] `examples/14_gpu_particles/main.c` + `README.md`
- [ ] `examples/15_compute_image_fx/main.c` + `README.md`
- [ ] `examples/16_hot_reload_and_hd_color/main.c` + `README.md` (new)
- [ ] `examples/17_digital_rain/main.c` + `README.md`

### Tier 5 — Capstone
- [x] `examples/18_text_showcase/main.c` + `README.md`
- [x] `examples/19_node_graph_piano/main.c` + `README.md`
- [x] `examples/20_load_and_draw_model/main.c` + `README.md`

### Advanced Shader
- [ ] `examples/21_ocean_realistic/main.c` + `README.md` — see [`SHADERTOY_OCEAN_4dSBDt_PLAN.md`](SHADERTOY_OCEAN_4dSBDt_PLAN.md); verify OpenGL FS link ≤~65k instructions

### Documentation Updates
- [x] Update `examples/README.md` — tiers 3–6 + promotion note
- [ ] Update `doc/guide/examples_faq.md` — learning path through 20
- [ ] Update `doc/introduction.md` — section 4 "Examples & Tutorials"
- [ ] Update root `README.md` — link to `examples/README.md`

---

## Priority Order (rev. 2)

Recommended build order after Tier 1–2 completion:

1. **11 — Music Visualizer** — highest “wow” / combines 4 unique features; good for README hero video
2. **13 — Shader Lab 3D** — fast win via promotion; answers “can I do 3D?”
3. **14 — GPU Particles** — compute credibility; pairs with new `compute.md`
4. **12 — Procedural World** — dual-VD game-shaped demo
5. **16 — Hot Reload & HD Color** — new code but small; docs already exist
6. **15, 17** — image compute + digital rain (promotions)
7. **18–20** — capstone promotions (text, piano, model)
8. **Doc/index pass** — `examples/README.md`, `examples_faq.md`, introduction

**Defer:** `platformer_plumber` → Tier 6 or future 21 until numbered pipeline is stable.

---

## Notes on Quality Bar

- Every example compiles clean with `-Wall -Wextra -Wpedantic` (no warnings)
- Every example runs without crashing on first launch, no setup required beyond building the static lib
- Every `main.c` opens with a 10-line block comment: what it shows, what keys to press, what to look for
- Every `README.md` includes: description, build command, key bindings, APIs highlighted, what makes this feature unique vs SDL/raylib/miniaudio
- No TODOs left in source — if something isn't done, it isn't committed
