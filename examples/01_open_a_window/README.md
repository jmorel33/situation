# 01 — Open a Window

**Tier:** Fundamental  
**Backends:** OpenGL + Vulkan  
**Lines of code:** ~70 (excluding the shared header)

## What you see

A 1024×768 window with a dark-blue background. Nothing else — intentionally. The terminal/console prints a hardware summary at startup:

```
╔══════════════════════════════════════════╗
║  Situation 2.4.265
║  Backend : OpenGL
║  OS      : Windows 11 10.0.22631 (build 22631)
║  CPU     : AMD Ryzen 9 7950X (16 cores / 32 threads @ 4.5 GHz)
║  GPU     : NVIDIA GeForce RTX 4090 (24.0 GB VRAM)
║  RAM     : 64.0 GB total / 48.2 GB free
╚══════════════════════════════════════════╝
```

## What it teaches

| Concept | Function |
|---------|----------|
| Initialise the library | `SituationInit` with `SituationInitInfo` |
| Main loop predicate | `SituationWindowShouldClose` |
| Mandatory frame phases | `SITUATION_BEGIN_FRAME()`, `SituationAcquireFrameCommandBuffer`, `SituationEndFrame` |
| Clear the framebuffer | `SituationCmdBeginRenderPass` with `SIT_LOAD_OP_CLEAR` |
| Query hardware | `SituationGetGPUInfo`, `SituationGetCPUInfo`, `SituationGetMemoryInfo`, `SituationGetOSInfo` |
| Graceful exit | `SituationShutdown` |

## Build and run

```bat
REM Prerequisites (once per session):
build\build_situation.bat static-opengl

REM Build:
build\build_examples.bat static-opengl 01_open_a_window

REM Run:
build\examples\01_open_a_window.exe
```

Vulkan variant:
```bat
build\build_situation.bat static-vulkan
build\build_examples.bat static-vulkan 01_open_a_window
build\examples\01_open_a_window.exe
```

## Universal hotkeys

All numbered examples share these hotkeys (from `examples/shared/sit_example.h`):

| Key | Action |
|-----|--------|
| ESC | Quit |
| F11 | Toggle fullscreen / windowed |
| F9  | Toggle VSync on / off |
| P   | Toggle pause |
| F12 | Save screenshot (`screenshot_NNNN.bmp`) |

## Architecture note

The three-phase frame structure is mandatory in Situation:

```
1. SituationPollInputEvents()          ← gather OS events
2. SituationUpdateTimers()             ← advance delta-time + oscillators
3. SituationAcquireFrameCommandBuffer  ← begin GPU work
   SituationCmdBeginRenderPass         ← start a render pass
   ... draw commands ...
   SituationCmdEndRenderPass           ← end the pass
   SituationEndFrame()                 ← submit to GPU
```

`SITUATION_BEGIN_FRAME()` is a convenience macro that wraps steps 1 + 2 together.  
The shared header `sit_example.h` wraps this macro plus common hotkey handling.
