# Test Harness Plan — Full SITAPI Coverage

**Date**: 2026-05-06  
**Last aligned with library**: 2026-05-10 (**v2.4.63**); verification snapshot below; root **`situation.h`** version macros; **`LIBRARY_BUGFIX_PLAN.md`** (Bug 6 open for Vulkan full-chain / audio lifecycle)  
**Target Directory**: `tests/harness/`  
**Scope**: Custom C11 test framework exercising every public `SITAPI` function.  
**Risk Level**: Low — additive only, no changes to library code.  
**Spec Reference**: `.kiro/specs/situation-test-harness/`

---

## Summary

Build a self-contained test harness that calls every public API function in `situation_api.h`.
The harness is a single executable with 9 test modules (one per subsystem), a minimal header-only
framework, CLI filtering, and crash recovery. No external test framework dependencies.

The goal is regression detection: if a refactor breaks an API contract, the harness catches it
immediately.

**Graphics** (Phases 1–11): Framework, core modules, rendering pipeline, virtual displays, compute, data flow, descriptors, model loading. **89 tests** in `test_graphics.c` (includes Phases 23–29).

**Graphics upgrade** (Phases 23–29): Fragment multi-SSBO + SPIR-V regression tests — see [`doc/plan/TEST_HARNESS_GRAPHICS_UPGRADE.md`](TEST_HARNESS_GRAPHICS_UPGRADE.md). **+5 OpenGL-only tests** (`graphics_helpers_smoke`, `fragment_dual_ssbo_readback`, `fragment_combined_scene_ssbo`, `uniform_1iv_int_array`, `demon_hunt_sky_shader_link`). Motivation: Demon Hunt SSBO binding bug passed full OpenGL harness (v2.4.81–82).

**Audio** (Phases 12–18): Device registry, node graph lifecycle, control parameters (dials/buttons), all 16 registered effects modules, mixer advanced features (routing, EQ, dynamics, metering), graph serialization roundtrip, MIDI integration & learn. 86 new tests in `test_audio.c`.

**Coverage Gaps** (Phases 19–22): Window state/display modes, system utilities/logging, filesystem extended ops, unregistered audio node types. ~30 new tests across `test_window.c`, `test_core.c`, `test_filesystem.c`, `test_audio.c`.

---

## Verified on reference hardware (snapshot)

Use this table so **“confirmed working”** is explicit; it is not a substitute for re-running after every change.

| Scope | Command / artifact | Result | Notes |
|-------|-------------------|--------|--------|
| **OpenGL — full sequential harness** | `build\sit_test.exe` (no `--module`; OpenGL DLL build) | **320 / 320** pass | **v2.4.84+**, Windows reference; +5 fragment SSBO / SPIR-V tests |
| **Vulkan — full sequential harness** | `build\sit_test_vulkan.exe` (Vulkan DLL build; see `build_tests.bat vulkan`) | **312 / 312** pass | **v2.4.84+**, Windows reference |
| **OpenGL — chained compute (SSBO bindings)** | Covered inside full OpenGL run | pass | **v2.4.62** — `SIT_COMPUTE_LAYOUT_TWO_SSBOS` + `glShaderStorageBlockBinding` |
| **Audio — echo dry/wet (manual)** | `examples/node_graph_piano_demo.c` via `build_examples.bat opengl node_graph_piano_demo` | listen OK | **v2.4.63** — parallel dry + linear wet; **not** an automated `sit_test` assertion yet |
| **Vulkan — full sequential harness** | Same pattern as OpenGL, all modules | **not baseline’d green** | Track under **`LIBRARY_BUGFIX_PLAN.md`** Bug 6; re-run after audio init/shutdown/re-init work |

---

## Architecture

```
tests/harness/
├── sit_test_framework.h      ← Header-only framework (assertions, runner, output)
├── sit_test_registry.c       ← Module registration (up to 32 modules)
├── main.c                    ← Entry point, CLI parser, signal handlers
├── test_core.c               ← Core/Lifecycle (init, shutdown, state, FPS, callbacks)
├── test_window.c             ← Window/Display (state, properties, monitors, clipboard)
├── test_input.c              ← Input (keyboard, mouse, gamepad)
├── test_graphics.c           ← Graphics (meshes, shaders, textures, buffers, compute, VDs)
├── test_audio.c              ← Audio (device, playback, tones, effects, capture, mixer)
├── test_filesystem.c         ← Filesystem (paths, file I/O, directories) — NO GPU
├── test_threading.c          ← Threading (pool, jobs, dependencies) — NO GPU
├── test_timer.c              ← Timer/Oscillators
└── test_misc.c               ← Image CPU ops, fonts, color conversions
```

**Build output**: `build\sit_test.exe` (OpenGL / default) or `build\sit_test_vulkan.exe` (`build_tests.bat vulkan`) — same sources, mutually exclusive DLL link; separate names avoid overwriting a locked exe.

---

## Phase 1 — Framework Infrastructure

- [x] Create `tests/harness/sit_test_framework.h`
  - Assertion macros: `SIT_ASSERT`, `SIT_ASSERT_EQ`, `SIT_ASSERT_NEQ`, `SIT_ASSERT_NULL`, `SIT_ASSERT_NOT_NULL`, `SIT_ASSERT_STR_EQ`, `SIT_ASSERT_MEM_EQ`
  - `setjmp`/`longjmp` recovery (non-fatal failures)
  - Result tracking: total/passed/failed/skipped + per-test timing
  - ANSI colored output (with `--no-color` fallback)
  - Module descriptor structs (`SitTestModule`, `SitTestCase`)

- [x] Create `tests/harness/sit_test_registry.c`
  - Static array of up to 32 module pointers
  - `sit_test_register_all()` — registers modules in execution order
  - `sit_test_find_module(name)` — case-insensitive lookup

- [x] Create `tests/harness/main.c`
  - CLI parsing: `--module <name>`, `--filter <substr>`, `--list`, `--stop-on-fail`, `--verbose`, `--no-color`
  - Signal handlers: SIGSEGV, SIGABRT → record crash, attempt teardown
  - Timeout: 10s per test (thread-based on Windows, SIGALRM on POSIX)
  - Exit code: 0 = all pass, 1 = any failure

- [x] Create `build_tests.bat`
  - Compiles all `tests/harness/*.c` into single binary
  - Flags: `-std=c11 -I. -I./ext -DSITUATION_IMPLEMENTATION`
  - Backend switch: `-DSITUATION_USE_VULKAN` or `-DSITUATION_USE_OPENGL`
  - Links: same libs as `build_examples.bat`

**Estimated effort**: 2–3 hours  
**Dependency**: None

---

## Phase 2 — Context-Free Modules (No GPU Required)

These modules run without `SituationInit()` — pure logic tests.

### 2A — Filesystem Module (`test_filesystem.c`)

- [x] Path utilities: `SituationGetBasePath`, `SituationGetAppSavePath`, `SituationJoinPath`, `SituationGetFileName`, `SituationGetFileExtension`, `SituationFreeString`
- [x] File queries: `SituationFileExists`, `SituationDirectoryExists`, `SituationGetFileModTime`
- [x] File I/O roundtrips: `SituationSaveFileText`/`SituationLoadFileText`, `SituationSaveFileData`/`SituationLoadFileData`
- [x] File ops: `SituationCopyFile`, `SituationDeleteFile`, `SituationMoveFile`
- [x] Directory ops: `SituationCreateDirectory`, `SituationDeleteDirectory`, `SituationListDirectoryFiles`, `SituationFreeDirectoryFileList`
- [x] Cleanup: all temp files use `_sit_test_` prefix, teardown removes them

### 2B — Threading Module (`test_threading.c`)

- [x] Pool lifecycle: `SituationCreateThreadPool` (4 threads), verify `is_active`, `SituationDestroyThreadPool`
- [x] Job submission: `SituationSubmitJobEx` (increment counter), `SituationWaitForJob`, verify counter
- [x] Batch: `SituationDispatchParallel` over 100 items, verify all processed
- [x] Wait all: `SituationWaitForAllJobs`
- [x] Dependencies: `SituationAddJobDependency`, verify execution order
- [x] Diagnostics: `SituationDumpTaskGraph` (just verify no crash)

**Estimated effort**: 1–2 hours  
**Dependency**: Phase 1

---

## Phase 3 — Core Context Modules

These require `SituationInit()` / `SituationShutdown()` in setup/teardown.

### 3A — Core/Lifecycle (`test_core.c`)

- [x] Version: `SituationGetVersionString` → non-NULL, non-empty
- [x] Init/Shutdown cycle: verify `SituationIsInitialized`, `SituationGetInitState`
- [x] Frame loop: `SituationPollInputEvents`, `SituationUpdateTimers`, `SituationGetFrameTime`, `SituationGetFPS`
- [x] FPS control: `SituationSetTargetFPS`
- [x] App state: `SituationWindowShouldClose` (false after init), `SituationPauseApp`/`SituationResumeApp`/`SituationIsAppPaused`
- [x] Callbacks: `SituationSetExitCallback`, `SituationSetResizeCallback`, `SituationSetFocusCallback`, `SituationSetFileDropCallback` (set + set NULL)
- [x] System info: `SituationGetDeviceInfo`, `SituationGetCPUThreadCount` (>0), `SituationGetGPUName`, `SituationGetUserDirectory`
- [x] Error: `SituationGetLastErrorMsg`
- [x] Args: `SituationIsArgumentPresent`, `SituationGetArgumentValue`

### 3B — Window/Display (`test_window.c`)

- [x] State: `SituationSetWindowState`, `SituationClearWindowState`, `SituationIsWindowState`
- [x] Fullscreen: `SituationToggleFullscreen`, `SituationToggleBorderlessWindowed` (toggle + restore)
- [x] Min/Max/Restore: `SituationMaximizeWindow`, `SituationMinimizeWindow`, `SituationRestoreWindow`
- [x] VSync: `SituationSetVSync`
- [x] Properties: `SituationSetWindowTitle`, `SituationSetWindowPosition`, `SituationSetWindowSize`, `SituationSetWindowMinSize`, `SituationSetWindowMaxSize`, `SituationSetWindowOpacity`
- [x] Getters: `SituationGetScreenWidth`/`Height` (>0), `SituationGetRenderWidth`/`Height`, `SituationGetWindowSize`, `SituationGetWindowPosition`, `SituationGetWindowScaleDPI`
- [x] Monitors: `SituationGetMonitorCount` (≥1), `SituationGetCurrentMonitor`, `SituationGetDisplays`/`SituationFreeDisplays`, `SituationGetMonitorName`, `SituationGetMonitorWidth`/`Height`, `SituationGetMonitorRefreshRate`, `SituationGetMonitorPosition`
- [x] Cursor: `SituationSetCursor`, `SituationShowCursor`, `SituationHideCursor`, `SituationDisableCursor`
- [x] Clipboard: `SituationSetClipboardText` → `SituationGetClipboardText` roundtrip

### 3C — Input (`test_input.c`)

- [x] Keyboard: `SituationIsKeyDown(SIT_KEY_A)` → false (no input), `SituationIsKeyUp`, `SituationIsKeyPressed`, `SituationIsKeyReleased`, `SituationGetKeyPressed`, `SituationGetCharPressed`, `SituationGetKeyScancode`, `SituationIsModifierPressed`, `SituationSetKeyCallback`
- [x] Mouse: `SituationGetMousePosition`, `SituationGetMouseDelta`, `SituationGetMouseWheelMove`, `SituationIsMouseButtonDown`/`Pressed`/`Released`, `SituationSetMousePosition`, `SituationSetMouseOffset`, `SituationSetMouseScale`, callbacks
- [x] Gamepad: `SituationIsJoystickPresent` (graceful false), `SituationIsGamepad`, `SituationGetJoystickName`, `SituationGetGamepadAxisCount`/`Value`, `SituationIsGamepadButtonDown`, `SituationSetJoystickCallback`, `SituationSetGamepadMappings`

### 3D — Timer/Oscillators (`test_timer.c`)

- [x] Time: `SituationTimerGetTime` → >0.0 after init+update
- [x] Oscillator queries: `SituationTimerGetOscillatorState`, `SituationTimerGetPreviousOscillatorState`, `SituationTimerHasOscillatorUpdated`, `SituationTimerPingOscillator`, `SituationTimerGetOscillatorTriggerCount`, `SituationTimerGetOscillatorPeriod`, `SituationSetTimerOscillatorPeriod`, `SituationTimerGetPingProgress`

**Estimated effort**: 3–4 hours  
**Dependency**: Phase 1

---

## Phase 4 — Graphics Module (`test_graphics.c`)

Requires `SituationInit()` + shader compiler for some tests.

- [x] Meshes: `SituationCreateMesh` (triangle, 3 verts/3 idx), verify valid handle, `SituationGetMeshData`, `SituationDestroyMesh`
- [x] Shaders: `SituationLoadShaderFromMemory` (minimal vert+frag), verify handle, `SituationSetShaderUniform`, `SituationUnloadShader`
- [x] Textures: `SituationCreateImage` (2×2 RGBA) → `SituationCreateTexture`, verify handle, `SituationCreateTextureEx` (storage), `SituationGetTextureHandle`, `SituationDestroyTexture`
- [x] Buffers: `SituationCreateBuffer` (1KB storage), `SituationUpdateBuffer`, `SituationGetBufferData` roundtrip, `SituationGetBufferDeviceAddress`, `SituationDestroyBuffer`
- [x] Compute: `SituationCreateComputePipelineFromMemory`, verify handle, `SituationGetMaxComputeWorkGroups`, `SituationDestroyComputePipeline`
- [x] Command buffer: `SituationAcquireFrameCommandBuffer`, `SituationGetMainCommandBuffer`, `SituationCmdBeginRenderPass`/`EndRenderPass`, `SituationCmdSetViewport`, `SituationCmdSetScissor`, `SituationCmdPipelineBarrier`, `SituationEndFrame`
- [x] Virtual displays: `SituationCreateVirtualDisplay` (320×240), `SituationConfigureVirtualDisplay`, `SituationGetVirtualDisplay`, `SituationSetVirtualDisplayDirty`/`IsVirtualDisplayDirty`, `SituationRenderVirtualDisplays`, `SituationGetVirtualDisplaySize`, `SituationDestroyVirtualDisplay`
- [x] Models/Diagnostics: `SituationGetRendererType` (returns valid enum), `SituationIsFeatureSupported`, `SituationGetDrawCallCount`, `SituationGetVRAMUsage`, `SituationTakeScreenshot`

**Estimated effort**: 3–4 hours  
**Dependency**: Phase 1, Phase 3A (init pattern)

---

## Phase 5 — Audio Module (`test_audio.c`)

- [x] Device: `SituationGetAudioDevices`, `SituationGetAudioPlaybackSampleRate`, `SituationGetAudioMasterVolume`/`SetAudioMasterVolume`, `SituationIsAudioDevicePlaying`, `SituationPauseAudioDevice`/`ResumeAudioDevice`
- [x] Playback: `SituationLoadSoundFromFile` (need a test .wav), `SituationPlayLoadedSound`, `SituationStopLoadedSound`, `SituationStopAllLoadedSounds`, `SituationUnloadSound`
- [x] Streaming: `SituationLoadAudio`, `SituationPlayAudio`, `SituationUnloadAudio`
- [x] Tones: `SituationPlayToneEx` → non-zero handle, `SituationPlayTone`, `SituationStopTone`, `SituationStopAllTones`, `SituationPlayMidiNote`
- [x] Effects: `SituationSetSoundVolume`/`Get`, `SituationSetSoundPan`/`Get`, `SituationSetSoundPitch`/`Get`, `SituationSetSoundFilter`, `SituationSetSoundEcho`, `SituationSetSoundReverb`
- [x] Processors: `SituationAttachAudioProcessor`, `SituationDetachAudioProcessor`
- [x] Capture: `SituationStartAudioCapture`/`Stop` (graceful even without mic), `SituationSetAudioOutputMonitor`
- [x] Mixer: `SituationCreateMixer`/`DestroyMixer`, `SituationAddTrack`/`RemoveTrack`, `SituationSetTrackVolume`/`Pan`
- [x] Device enum: `SituationEnumerateAudioDevices`, `SituationFreeDeviceList`
- [x] Graph: `SituationSaveGraphToFile`, `SituationSerializeGraphToJSON`, `SituationFreeJSONString`

**Estimated effort**: 2–3 hours  
**Dependency**: Phase 1, Phase 3A, test audio asset (generate a 1-second sine .wav)

---

## Phase 6 — Miscellaneous Module (`test_misc.c`)

- [x] Image CPU ops: `SituationCreateImage` (4×4), `SituationSetPixelColor`, verify pixel, `SituationImageCopy`, `SituationGenImageColor`, `SituationImageCrop`, `SituationImageResize`, `SituationImageFlip`, `SituationExportImage`, `SituationLoadImage`/`FromMemory`, `SituationUnloadImage`, `SituationIsImageValid`
- [x] Fonts: `SituationLoadBitmapFontFromMemory`, `SituationMeasureText` (non-zero for non-empty string), `SituationUnloadFont`
- [x] Color: `SituationRgbToHsv` → `SituationHsvToRgb` roundtrip (within ±1), `SituationColorToYPQ`/`FromYPQ`, `SituationConvertColorToVector4`

**Estimated effort**: 1–2 hours  
**Dependency**: Phase 1 (some tests need context for font GPU upload)

---

## Phase 7 — Integration & Verification

- [x] Wire all modules into `sit_test_registry.c` in order: filesystem → threading → core → window → input → timer → graphics → audio → misc
- [x] Compile with `-DSITUATION_USE_OPENGL` — verify clean build
- [x] Run `sit_test --module filesystem` — passes without GPU
- [x] Run `sit_test --module threading` — passes without GPU
- [x] Run `sit_test --module misc` — passes without GPU
- [x] Verify `--list`, `--filter`, `--stop-on-fail` work correctly
- [x] Verify no `_sit_test_*` artifacts remain after run
- [x] Compile with `-DSITUATION_USE_VULKAN` — verify clean build
- [x] **OpenGL** — full sequential `sit_test.exe` (all modules): **310/310** pass (**v2.4.62+**, reference Windows; requires GPU)
- [x] **Vulkan** — `sit_test_vulkan.exe --module graphics`: **78/78** pass (**v2.4.61+**, reference **GTX 1070**)
- [x] **Vulkan** — full sequential `sit_test_vulkan.exe` (all modules): **312/312** pass (**v2.4.84+**, Windows reference; graphics **81** tests on Vulkan — no OpenGL-only fragment SSBO suite)

**Estimated effort**: 1 hour  
**Dependency**: All previous phases

---

## Implementation Order

| # | Phase | Effort | Priority | Status |
|---|-------|--------|----------|--------|
| 1 | Framework infrastructure | 2–3h | Critical | ☑ |
| 2 | Context-free modules (FS, Threading) | 1–2h | High | ☑ |
| 3 | Core context modules (Core, Window, Input, Timer) | 3–4h | High | ☑ |
| 4 | Graphics module | 3–4h | Medium | ☑ |
| 5 | Audio module | 2–3h | Medium | ☑ |
| 6 | Miscellaneous module | 1–2h | Low | ☑ |
| 7 | Integration & verification | 1h | High | ☑ |
| 8 | Rendering pipeline tests | 3–4h | High | ☑ |
| 9 | Virtual display deep tests | 2–3h | High | ☑ |
| 10 | Compute shader roundtrip | 2h | Medium | ☑ |
| 11 | Data flow & descriptor binding | 2–3h | Medium | ☑ |
| 12 | Audio: Device registry & metadata | 1h | Critical | ☑ |
| 13 | Audio: Node graph lifecycle & patching | 2–3h | Critical | ☑ |
| 14 | Audio: Control parameters (dials & buttons) | 2h | High | ☑ |
| 15 | Audio: Effects module instantiation | 2–3h | High | ☑ |
| 16 | Audio: Mixer advanced features | 3–4h | High | ☑ |
| 17 | Audio: Graph serialization roundtrip | 1–2h | Medium | ☑ |
| 18 | Audio: MIDI integration & learn | 2h | Medium | ☑ |
| 19 | Window state & display modes | 1–2h | Medium | ☑ |
| 20 | System utilities & logging | 1h | Low | ☑ |
| 21 | Filesystem extended ops | 1h | Low | ☑ (async only; path utils/watchers not in API) |
| 22 | Audio: unregistered node types (post-DLL rebuild) | 1h | Blocked | ☐ |
| 23 | Graphics helpers (fullscreen draw / pixel readback) | 1–2h | High | ☑ |
| 24 | Fragment dual SSBO readback (SPIR-V) | 2–3h | **Critical** | ☑ |
| 25 | Combined scene SSBO (map header + vec4 tail) | 2h | Medium | ☑ |
| 26 | `SituationSetShaderUniform1iv` int arrays | 1–2h | Low | ☑ |
| 27 | SPIR-V SSBO reflection sanity (optional) | 1–3h | Low | ☐ (optional) |
| 28 | Library regression guard (v2.4.82) | 0.5h | High | ☑ |
| 29 | Docs + full harness verification | 1h | High | ☑ (run full suite locally) |

**Total estimated effort**: ~45–55 hours (Phases 1–22) + ~9–14 hours (Phases 23–29). Detail: [`TEST_HARNESS_GRAPHICS_UPGRADE.md`](TEST_HARNESS_GRAPHICS_UPGRADE.md).

---

## Phase 23–29 — Fragment SSBO / SPIR-V graphics (planned)

> **Full checklist, shaders, and acceptance criteria**: [`doc/plan/TEST_HARNESS_GRAPHICS_UPGRADE.md`](TEST_HARNESS_GRAPHICS_UPGRADE.md)

- [x] **23** — Shared test helpers (fullscreen mesh, center-pixel readback, SPIR-V skip).
- [x] **24** — `test_fragment_dual_ssbo_readback` — **required** regression for v2.4.82.
- [x] **25** — `test_fragment_combined_scene_ssbo` — Demon Hunt `ShaderScenePack` layout.
- [x] **26** — `test_uniform_1iv_int_array` — pass/skip when SPIR-V strips uniforms.
- [ ] **27** — (Optional) `glGetProgramResourceiv` binding uniqueness.
- [x] **28** — Wire regression comments + UPDATELOG when Phase 24 lands.
- [x] **29** — Update harness snapshot table and CI notes (Vulkan graphics unchanged; +4 OpenGL-only tests).

---

## Phase 8 — Rendering Pipeline Tests (Visual Verification)

These tests exercise the actual draw path: bind shader → set state → draw → verify output.
All require GPU context. Verification uses `SituationLoadImageFromScreen()` or buffer readback.

### 8A — Draw Commands (`test_graphics.c` additions)

- [x] Full draw pipeline: `SituationCmdBindPipeline` → `SituationCmdDraw` (3 verts) → `SituationEndFrame` → `SituationLoadImageFromScreen` → verify non-black pixels
- [x] Indexed draw: `SituationCmdDrawIndexed` with a quad (4 verts, 6 indices) → verify rendered area
- [x] High-level mesh draw: `SituationCmdDrawMesh` with a triangle mesh → verify pixels
- [x] Quad draw: `SituationCmdDrawQuad` with red color → verify red pixels in output
- [x] Textured draw: Create 2×2 checkerboard texture → `SituationCmdDrawTexture` → verify pattern in output

### 8B — Shader Uniform Data Flow

- [x] Set float uniform: `SituationSetShaderUniform` (float) → render → verify shader uses value (e.g., color multiplier)
- [x] Set vec4 uniform: pass color via uniform → render → verify output color matches
- [x] Set mat4 uniform: pass transform → render → verify vertex positions shifted
- [x] Push constants: `SituationCmdSetPushConstant` → render → verify data reaches shader

### 8C — Text Rendering

- [x] `SituationCmdDrawText` with bitmap font → verify non-empty pixels in text region
- [x] `SituationCmdDrawTextEx` with custom size/spacing → verify bounds differ from default

### 8D — Metrics & Diagnostics

- [x] `SituationDrawMetricsOverlay` → no crash, verify pixels drawn in overlay region
- [x] `SituationGetDrawCallCount` → issue N draws → verify count ≥ N
- [x] `SituationExportRenderHistogram` → non-empty buffer after a few frames
- [x] `SituationLoadImageFromScreen` → valid image with correct dimensions

**Estimated effort**: 3–4 hours  
**Dependency**: Phase 3A (init), Phase 4 (shader/mesh creation)

---

## Phase 9 — Virtual Display Deep Tests

Current VD tests only verify creation/destruction and property getters. These tests exercise
the full compositing pipeline: render into a VD, configure it, composite to main, verify output.

### 9A — Render-to-VD Pipeline

- [x] Create VD (64×64) → begin render pass targeting VD → draw solid red quad → end pass → mark dirty → composite → screenshot → verify red region in output
- [x] Multiple VDs with z-ordering: VD1 (z=0, blue) behind VD2 (z=1, red) → composite → verify VD2 on top
- [x] VD visibility toggle: create VD, set `visible=false` → composite → verify VD not in output → set `visible=true` → composite → verify VD appears
- [x] VD opacity: create VD (opacity=0.5) over solid background → composite → verify blended color

### 9B — Scaling Modes

- [x] `SITUATION_SCALING_STRETCH`: 32×32 VD stretched to 64×64 region → verify fills entire region (no black bars)
- [x] `SITUATION_SCALING_FIT`: non-square VD (32×64) fit into square region → verify aspect ratio preserved (letterbox/pillarbox)
- [x] `SITUATION_SCALING_INTEGER`: 32×32 VD in 100×100 region → verify integer scale factor (1x or 2x or 3x, no fractional)
- [x] `SituationSetVirtualDisplayScalingMode` runtime switch → verify mode change takes effect

### 9C — Blend Modes

- [x] `SITUATION_BLEND_ALPHA`: semi-transparent VD over white → verify alpha-blended result
- [x] `SITUATION_BLEND_ADDITIVE`: dark VD over dark background → verify brightening
- [x] `SITUATION_BLEND_MULTIPLY`: white VD over colored background → verify no change; colored VD over white → verify darkening
- [x] `SITUATION_BLEND_NONE`: VD with alpha → verify alpha ignored, full overwrite

### 9D — VD Timing & Performance

- [x] `SituationGetLastVDCompositeTimeMS` → composite 4 VDs → verify time > 0.0
- [x] Frame-time multiplier: VD with `frame_time_mult=0.5` → verify dirty flag behavior respects timing
- [x] Offset/position: create VD with offset (10,10) → composite → verify content shifted

**Estimated effort**: 2–3 hours  
**Dependency**: Phase 3A, Phase 4 (shaders for rendering into VD)

---

## Phase 10 — Compute Shader Roundtrip

End-to-end compute tests: create pipeline → bind resources → dispatch → read back → verify.

- [x] Basic dispatch: compute shader writes `42.0` to SSBO → `SituationCmdBindComputePipeline` → `SituationCmdDispatch(1,1,1)` → `SituationGetBufferData` → verify `42.0`
- [x] Multi-element dispatch: compute shader writes `gl_GlobalInvocationID.x` to buffer[i] → dispatch(64,1,1) → readback → verify sequential values 0..63
- [x] Compute with texture output: compute shader writes to storage image → read back image → verify pixel values
- [x] `SituationCmdBindComputeTexture`: bind input texture → compute reads it → writes to SSBO → verify values match texture data
- [x] Pipeline barrier correctness: compute writes → barrier → graphics reads → verify no corruption
- [x] Multiple dispatches: dispatch A writes to buffer → barrier → dispatch B reads buffer and writes to buffer2 → readback buffer2 → verify chained computation

**Estimated effort**: 2 hours  
**Dependency**: Phase 4 (compute pipeline creation, buffer management)

---

## Phase 11 — Data Flow & Descriptor Binding

Tests that verify data actually travels correctly between CPU and GPU, and that binding
points work as expected.

### 11A — Buffer Data Integrity

- [x] Partial buffer update: create 1KB buffer → update bytes [256..512] → readback full buffer → verify only updated region changed
- [x] Large buffer: create 1MB buffer → fill with pattern → readback → verify byte-for-byte match
- [x] Zero-offset update: update from offset 0 → readback → verify
- [x] Multiple sequential updates: update region A, then region B → readback → verify both

### 11B — Descriptor Set Binding

- [x] `SituationCmdBindDescriptorSet`: bind UBO with known data → render → verify shader reads correct values (output to SSBO or color)
- [x] `SituationCmdBindDescriptorSetDynamic`: bind with offset 0, render → bind with offset 256, render → verify different data used
- [x] `SituationCmdBindTextureSet`: bind texture to set → sample in shader → verify sampled color
- [x] `SituationCmdBindSampledTexture`: bind to specific binding point → verify correct texture sampled
- [x] Multiple sets: bind set 0 (UBO) + set 1 (texture) simultaneously → render → verify both active

### 11C — Texture Data Roundtrip

- [x] CPU→GPU→CPU: create image with known pixels → `SituationCreateTexture` → render textured quad to VD → `SituationLoadImageFromScreen` → verify pixels match (within tolerance for filtering)
- [x] Storage texture write: compute shader writes to storage texture → read back as image → verify values
- [x] Texture format preservation: create RGBA image → upload → download → verify channels preserved

### 11D — Model Loading (if test assets available)

- [x] `SituationLoadModel` with embedded test GLTF (minimal triangle) → verify non-null handle
- [x] `SituationDrawModel` → render → verify pixels
- [x] `SituationSaveModelAsGltf` → export → verify file exists and is valid JSON
- [x] `SituationUnloadModel` → verify no crash, handle invalidated

**Estimated effort**: 2–3 hours  
**Dependency**: Phase 4, Phase 8 (draw verification pattern), Phase 10 (compute)

---

## Phase 12 — Audio: Device Registry & Metadata

Tests that the audio device type system works: registration, querying, categories.
All 26 built-in device types should be registered after `SituationInitDeviceRegistry`.

- [x] `SituationInitDeviceRegistry` — call succeeds (no crash)
- [x] `SituationGetRegisteredDeviceCount` — returns > 0 after init (built-in devices registered)
- [x] `SituationIsDeviceRegistered(SITUATION_NODE_REVERB)` — returns true
- [x] `SituationIsDeviceRegistered(SITUATION_NODE_CUSTOM + 999)` — returns false (unregistered)
- [x] `SituationGetDeviceMetadata(SITUATION_NODE_REVERB)` — returns SUCCESS, name non-empty, category = EFFECT
- [x] `SituationGetDeviceMetadata(SITUATION_NODE_TONE_SYNTH)` — category = SOURCE, num_audio_outs > 0
- [x] `SituationGetDeviceMetadata(SITUATION_NODE_LFO)` — category = MODULATOR
- [x] `SituationGetDeviceMetadata(SITUATION_NODE_PEAK_METER)` — category = ANALYZER
- [x] `SituationGetCategoryName(SITUATION_DEVICE_EFFECT)` — returns non-NULL string
- [x] `SituationGetCategoryName(SITUATION_DEVICE_SOURCE)` — returns non-NULL string
- [x] `SituationGetCategoryName(SITUATION_DEVICE_UTILITY)` — returns non-NULL string
- [x] `SituationRegisterDeviceType` — register a custom device, verify `IsDeviceRegistered` returns true
- [x] Verify all 26 built-in node types are registered (loop through enum, check each)

**Estimated effort**: 1 hour  
**Dependency**: Phase 5 (audio module init)

---

## Phase 13 — Audio: Node Graph Lifecycle & Patching

Core graph operations: create/destroy graphs, create/destroy nodes, connect/disconnect patches, cycle detection.

### 13A — Graph & Node Lifecycle

- [x] `SituationCreateGraph` — returns non-NULL
- [x] `SituationDestroyGraph` — no crash on valid graph
- [x] `SituationDestroyGraph(NULL)` — no crash (graceful)
- [x] `SituationCreateNode(graph, SITUATION_NODE_REVERB)` — returns SUCCESS, handle != INVALID
- [x] `SituationCreateNode(graph, SITUATION_NODE_GAIN)` — returns SUCCESS
- [x] `SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH)` — returns SUCCESS
- [x] Create 16 nodes in one graph — all succeed (stress test)
- [x] `SituationDestroyNode` — returns SUCCESS for valid handle
- [x] `SituationDestroyNode` with invalid handle — returns error, no crash
- [x] `SituationGetNode` — returns non-NULL for valid handle
- [x] `SituationGetNode` with invalid handle — returns NULL

### 13B — Patching (Audio Connections)

- [x] `SituationCreatePatch(src, 0, dst, 0, false)` — connect two nodes, returns SUCCESS
- [x] Verify patch between TONE_SYNTH output → REVERB input succeeds
- [x] Verify patch between REVERB output → GAIN input succeeds (chain)
- [x] `SituationDestroyPatch` — disconnect, returns SUCCESS
- [x] Double-connect same ports — returns error (duplicate patch)
- [x] Connect to invalid node handle — returns error
- [x] Connect with out-of-range port index — returns error

### 13C — Patching (Control Connections)

- [x] `SituationCreatePatch(lfo, 0, reverb, 0, true)` — control patch, returns SUCCESS
- [x] Verify LFO → effect control connection works
- [x] `SituationDestroyPatch` on control patch — returns SUCCESS

### 13D — Cycle Detection

- [x] Create A→B→C chain — succeeds
- [x] Attempt C→A patch (creates cycle) — returns error
- [x] Verify A→B, B→A is rejected (simple 2-node cycle)
- [x] Verify self-loop (A→A) is rejected

**Estimated effort**: 2–3 hours  
**Dependency**: Phase 12 (registry must be initialized for node creation)

---

## Phase 14 — Audio: Control Parameters (Dials & Buttons)

Every node has controls (knobs, switches, sliders). Test get/set for various device types.

### 14A — Basic Control Get/Set

- [x] `SituationSetControl(graph, reverb_node, 0, 0.5)` — returns SUCCESS
- [x] `SituationGetControl(graph, reverb_node, 0)` — returns 0.5
- [x] Set control to min value (0.0) — succeeds, readback matches
- [x] Set control to max value (1.0) — succeeds, readback matches
- [x] Set control on invalid node — returns error
- [x] Set control with invalid control_id — returns error
- [x] Get control with invalid control_id — returns error

### 14B — Control Metadata Verification

- [x] Get metadata for REVERB — verify num_controls > 0
- [x] Verify each control has non-empty name
- [x] Verify min_value < max_value for all controls
- [x] Verify default_value is within [min, max] range
- [x] Verify control types are valid enum values

### 14C — Per-Device Control Sweep

For each of the 26 built-in device types:
- [x] Create node → get metadata → for each control: set to default, get back, verify match
- [x] Set each control to midpoint ((min+max)/2) → verify readback

This is a parametric test that iterates all device types and all their controls.

**Estimated effort**: 2 hours  
**Dependency**: Phase 13A (need nodes in a graph)

---

## Phase 15 — Audio: Effects Module Instantiation & Verification

Verify every effects processor can be instantiated as a node, has valid metadata, and its controls respond.

### 15A — Effect Node Creation (all 18 effects + modulators)

For each effect type:
- [x] SITUATION_NODE_REVERB — create, verify handle valid
- [x] SITUATION_NODE_ECHO — create, verify handle valid
- [x] SITUATION_NODE_CHORUS — create, verify handle valid
- [x] SITUATION_NODE_PHASER — create, verify handle valid
- [x] SITUATION_NODE_OVERDRIVE — create, verify handle valid
- [x] SITUATION_NODE_EXCITER — create, verify handle valid
- [x] SITUATION_NODE_MAXIMIZER — create, verify handle valid
- [x] SITUATION_NODE_SPRING_REVERB — create, verify handle valid
- [x] SITUATION_NODE_STUDIO_REVERB — create, verify handle valid
- [x] SITUATION_NODE_SST282 — create, verify handle valid
- [x] SITUATION_NODE_DYNAMICS — create, verify handle valid
- [x] SITUATION_NODE_COMPANDER — create, verify handle valid
- [x] SITUATION_NODE_EQ_4BAND — create, verify handle valid
- [x] SITUATION_NODE_FILTER — create, verify handle valid
- [x] SITUATION_NODE_MASTERING_AMP — create, verify handle valid
- [x] SITUATION_NODE_DEAFMAX — create, verify handle valid
- [x] SITUATION_NODE_LFO — create, verify handle valid (modulator)
- [x] SITUATION_NODE_ENVELOPE_FOLLOWER — create, verify handle valid (modulator)

### 15B — Effect Metadata Validation

For each effect:
- [x] Verify category is SITUATION_DEVICE_EFFECT (or MODULATOR for LFO/envelope)
- [x] Verify num_audio_ins >= 1 (effects need input)
- [x] Verify num_audio_outs >= 1 (effects produce output)
- [x] Verify num_controls >= 1 (every effect has at least one knob)
- [x] Verify name is non-empty and descriptive

### 15C — Effect Control Roundtrip

For each effect:
- [x] Set all controls to their default values → readback → verify match
- [x] Set first control to min → readback → verify
- [x] Set first control to max → readback → verify
- [x] Verify no crash when sweeping all controls from min to max

### 15D — Effect Chain Test

- [x] Create chain: TONE_SYNTH → FILTER → REVERB → GAIN → output
- [x] Verify all patches succeed
- [x] Destroy chain — no crash, all resources freed

**Estimated effort**: 2–3 hours  
**Dependency**: Phase 13 (graph/node creation), Phase 14 (control get/set)

---

## Phase 16 — Audio: Mixer Advanced Features

Expand beyond the basic create/track/volume tests (Phase 5) to cover routing, sends, EQ, dynamics, metering, and session persistence.

### 16A — Track Routing & Sends

- [x] `SituationGetAuxBus(mixer, 0)` — returns non-NULL bus pointer
- [x] `SituationSetTrackSend(track, 0, 0.5, false)` — post-fader send, returns SUCCESS
- [x] `SituationSetTrackSend(track, 0, 0.7, true)` — pre-fader send, returns SUCCESS
- [x] `SituationSetTrackOutput(track, aux_bus)` — reroute track output, returns SUCCESS
- [x] `SituationRouteSoundToTrack(sound, track)` — route sound through track, returns SUCCESS

### 16B — Track EQ

- [x] `SituationSetTrackEQ(track, true, freqs, gains, Qs)` — enable 4-band EQ, no crash
- [x] `SituationSetTrackEQ(track, false, NULL, NULL, NULL)` — disable EQ, no crash
- [x] Verify EQ with valid frequency/gain/Q arrays doesn't crash

### 16C — Track Dynamics

- [x] `SituationSetTrackDynamics(track, true, 0, -20.0, 4.0, 10.0, 100.0, 0.0)` — compressor mode
- [x] `SituationSetTrackDynamics(track, true, 1, -10.0, 10.0, 1.0, 50.0, 3.0)` — limiter mode
- [x] `SituationSetTrackDynamics(track, true, 2, -40.0, 2.0, 5.0, 200.0, 0.0)` — gate mode
- [x] `SituationSetTrackDynamics(track, false, 0, 0, 0, 0, 0, 0)` — disable dynamics
- [x] `SituationSetTrackSideChain(target, source)` — set sidechain source, no crash

### 16D — Metering

- [x] `SituationGetTrackMeter(track, &left, &right, &gr)` — returns values, no crash
- [x] Verify meter values are in reasonable range (0.0 to 1.0 for peaks, <= 0 for GR)

### 16E — Effect Insert/Remove on Bus

- [ ] `SituationInsertEffect(bus, 0, effect_node)` — insert effect at slot 0, returns SUCCESS
- [ ] `SituationRemoveEffect(bus, 0)` — remove effect, returns non-NULL pointer
- [ ] `SituationRemoveEffect(bus, 0)` on empty slot — returns NULL (graceful)
- [x] `SituationGetMixerGraph(mixer)` — returns non-NULL ma_node_graph pointer

### 16F — Session Save/Load

- [x] `SituationSaveMixerSession(mixer, "_sit_test_session.json")` — returns true
- [x] Verify file exists after save
- [x] `SituationLoadMixerSession(mixer, "_sit_test_session.json")` — returns true
- [x] Cleanup: delete temp file

### 16G — Device Binding

- [x] `SituationBindMixerToDevice(mixer, NULL, 2)` — bind to default device, returns SUCCESS or graceful error
- [x] `SituationFindBestDevice(0, 2, 0)` — returns non-NULL or NULL (depends on hardware)

**Estimated effort**: 3–4 hours  
**Dependency**: Phase 5 (existing mixer basics already work)

---

## Phase 17 — Audio: Graph Serialization Roundtrip

Full save/load cycle: create a graph with nodes and patches → serialize → deserialize → verify structure matches.

- [x] Create graph with 3 nodes (synth → reverb → gain) and 2 patches
- [x] `SituationSerializeGraphToJSON` — returns non-NULL string
- [x] Verify JSON string starts with `{` and contains "nodes" key
- [x] `SituationFreeJSONString` — no crash
- [x] `SituationSaveGraphToFile(graph, "_sit_test_graph.json")` — returns SUCCESS
- [x] Verify file exists
- [x] Create new empty graph → `SituationLoadGraphFromFile` — returns SUCCESS
- [x] Verify loaded graph has same node count (3)
- [x] Verify loaded graph has same patch count (2)
- [x] `SituationDeserializeGraphFromJSON` — roundtrip from string, verify node count
- [x] `SituationGetSerializationVersion` — returns non-NULL string
- [x] `SituationIsVersionCompatible(version)` — returns true for current version
- [x] Cleanup: delete temp file

**Estimated effort**: 1–2 hours  
**Dependency**: Phase 13 (graph/node/patch creation)

---

## Phase 18 — Audio: MIDI Integration & Learn

MIDI control, learn mode, mapping management, and preset persistence.
All tests handle absent MIDI hardware gracefully.

### 18A — MIDI Device Control

- [x] `SituationListMidiDevices(devices, 16)` — returns >= 0 (may be 0 if no MIDI hardware)
- [x] `SituationEnableMidiControl(graph, node, -1)` — auto-select, returns SUCCESS or graceful error
- [x] `SituationIsMidiEnabled(graph, node)` — returns 1 after enable (or 0 if no device)
- [x] `SituationDisableMidiControl(graph, node)` — returns SUCCESS
- [x] `SituationIsMidiEnabled(graph, node)` — returns 0 after disable
- [x] `SituationAutoConnectMidi(graph, node)` — equivalent to EnableMidiControl(-1)

### 18B — MIDI Learn Lifecycle

- [x] `SituationEnableMidiLearn(graph, node)` — returns SUCCESS (requires MIDI enabled first)
- [x] `SituationIsMidiLearnEnabled(graph, node)` — returns 1
- [x] `SituationStartMidiLearn(graph, node, 0, "Volume", 0.0, 1.0, 0)` — returns SUCCESS
- [x] `SituationIsLearning(graph, node)` — returns 1
- [x] `SituationCancelMidiLearn(graph, node)` — returns SUCCESS
- [x] `SituationIsLearning(graph, node)` — returns 0 after cancel
- [x] `SituationDisableMidiLearn(graph, node)` — returns SUCCESS
- [x] `SituationIsMidiLearnEnabled(graph, node)` — returns 0

### 18C — Mapping Management

- [x] `SituationClearMidiMapping(graph, node, 0)` — returns SUCCESS (even if no mapping)
- [x] `SituationClearAllMidiMappings(graph, node)` — returns SUCCESS

### 18D — MIDI Preset Persistence

- [x] `SituationSaveMidiPreset(graph, node, "_sit_test_midi.json")` — returns SUCCESS
- [x] Verify file exists
- [x] `SituationLoadMidiPreset(graph, node, "_sit_test_midi.json")` — returns SUCCESS
- [x] Cleanup: delete temp file

**Estimated effort**: 2 hours  
**Dependency**: Phase 13 (need graph + node), MIDI hardware optional (tests gracefully handle absence)

---

## Built-in Audio Device Types (26 total)

| Type | Category | Expected Controls |
|------|----------|-------------------|
| REVERB | Effect | decay, mix, damping, room size |
| ECHO | Effect | delay time, feedback, mix |
| CHORUS | Effect | rate, depth, mix, voices |
| PHASER | Effect | rate, depth, stages, feedback |
| OVERDRIVE | Effect | drive, tone, level |
| EXCITER | Effect | amount, frequency, mix |
| MAXIMIZER | Effect | threshold, ceiling, release |
| SPRING_REVERB | Effect | decay, tone, mix |
| STUDIO_REVERB | Effect | room, damping, width, mix |
| SST282 | Effect | (tape saturation emulation) |
| DYNAMICS | Effect | threshold, ratio, attack, release |
| COMPANDER | Effect | threshold, ratio, knee |
| EQ_4BAND | Effect | freq/gain/Q x 4 bands |
| FILTER | Effect | cutoff, resonance, type |
| MASTERING_AMP | Effect | gain, ceiling, character |
| DEAFMAX | Effect | (loudness maximizer) |
| PANNER | Utility | pan position |
| GAIN | Utility | gain level |
| MIXER | Utility | (internal mixing node) |
| SOUND_SOURCE | Source | volume, pitch, loop |
| TONE_SYNTH | Source | frequency, waveform, volume |
| MIC_CAPTURE | Capture | gain, monitoring |
| LFO | Modulator | rate, depth, waveform |
| ENVELOPE_FOLLOWER | Modulator | attack, release, sensitivity |
| SPECTRUM_ANALYZER | Analyzer | (read-only analysis) |
| PEAK_METER | Analyzer | (read-only metering) |

---

## Phase 19 — Window State & Display Modes

Window state management, fullscreen/borderless toggling, icon setting, and focus control.
These tests may alter the display — use caution on CI. Some may need to be `userTriggered` only.

### 19A — Window State Flags

- [x] `SituationSetWindowState(SITUATION_FLAG_WINDOW_TOPMOST)` — set topmost, no crash
- [x] `SituationClearWindowState(SITUATION_FLAG_WINDOW_TOPMOST)` — clear topmost, no crash
- [x] `SituationSetWindowState(SITUATION_FLAG_WINDOW_UNDECORATED)` — remove decorations, no crash
- [x] `SituationClearWindowState(SITUATION_FLAG_WINDOW_UNDECORATED)` — restore decorations, no crash
- [x] `SituationSetWindowFocused()` — request focus, no crash

### 19B — Fullscreen & Borderless

- [x] `SituationToggleFullscreen()` — toggle on, verify no crash, toggle off to restore
- [x] `SituationToggleBorderlessWindowed()` — toggle on, verify no crash, toggle off to restore
- [x] Verify window dimensions change after fullscreen toggle (or remain valid)

### 19C — Window Icons

- [x] `SituationSetWindowIcon(NULL)` — clear icon, no crash (graceful NULL)
- [x] `SituationSetWindowIcon(valid_image)` — set 32×32 RGBA icon, no crash
- [x] `SituationSetWindowIcons(images, count)` — set multiple icon sizes, no crash

**Estimated effort**: 1–2 hours  
**Dependency**: Phase 3 (window module init)  
**Risk**: Fullscreen toggle may disrupt display on CI — consider skipping or making opt-in.

---

## Phase 20 — System Utilities & Logging

OS interaction, logging, string management, and platform queries.

### 20A — Logging

- [x] `SituationLog(SIT_LOG_INFO, "test message")` — no crash, message logged
- [x] `SituationLogWarning("warning message")` — no crash
- [x] `SituationSetTraceLogLevel(SIT_LOG_ERROR)` — set level, no crash
- [x] `SituationSetTraceLogLevel(SIT_LOG_NONE)` — suppress all, no crash
- [x] Restore original log level after test

### 20B — String Management

- [x] `SituationFreeString(NULL)` — no crash (graceful NULL)
- [x] `SituationFreeString(valid_ptr)` — free a string returned by API, no crash
- [x] Verify `SituationGetBasePath()` result can be freed with `SituationFreeString()`

### 20C — OS Interaction (Windows-specific)

- [x] `SituationGetCurrentDriveLetter()` — returns valid letter (A-Z) or 0
- [x] `SituationGetDriveInfo()` — returns non-NULL or NULL, no crash
- [ ] `SituationOpenFile(".")` — opens current directory (may spawn explorer), no crash
- [x] `SituationExecuteCommand("echo test")` — executes, returns success or graceful error

### 20D — Error System

- [x] `SituationGetLastErrorMsg()` — returns non-NULL string or graceful NULL after init

**Estimated effort**: 1 hour  
**Dependency**: Phase 1 (framework)  
**Note**: OpenFile/ExecuteCommand may have side effects — consider making them opt-in or `userTriggered`.

---

## Phase 21 — Filesystem Extended Operations

Additional filesystem operations not covered in Phase 2.

### 21A — Async File I/O

- [x] `SituationLoadFileAsync(pool, path, callback, userdata)` — starts async load, callback invoked with data
- [x] `SituationLoadFileTextAsync(pool, path, callback, userdata)` — async text load, callback invoked
- [x] `SituationSaveFileAsync(pool, path, data, size, callback, userdata)` — async binary save, file created
- [x] `SituationSaveFileTextAsync(pool, path, text, callback, userdata)` — async text save, content verified

### 21B — File Watching

- [ ] `SituationWatchFile(path, callback)` — register watcher, no crash (NOT IN API — deferred)
- [ ] `SituationUnwatchFile(path)` — unregister, no crash (NOT IN API — deferred)
- [ ] Modify watched file → verify callback fires (NOT IN API — deferred)

### 21C — Path Utilities (Extended)

- [ ] `SituationGetParentDirectory("a/b/c.txt")` — returns "a/b" (NOT IN API — deferred)
- [ ] `SituationNormalizePath("a//b/../c")` — returns normalized path (NOT IN API — deferred)
- [ ] `SituationIsAbsolutePath("/foo")` — returns true (NOT IN API — deferred)
- [ ] `SituationIsAbsolutePath("relative/path")` — returns false (NOT IN API — deferred)
- [ ] `SituationGetFileSize(path)` — returns correct size for known file (NOT IN API — deferred)

**Estimated effort**: 1 hour  
**Dependency**: Phase 2 (filesystem basics)  
**Note**: Async I/O tests may need polling or short timeouts. File watching may not be testable in all environments.

---

## Phase 22 — Audio: Unregistered Node Types (Post-DLL Rebuild)

Once the library registers GAIN, MIXER, LFO, ENVELOPE_FOLLOWER, SPECTRUM_ANALYZER, and PEAK_METER in the device registry, these tests can be enabled.

**Blocked by**: Library TODO in `sit/aud/registry_init.h` — modulators, analyzers, and remaining utilities not yet registered.

### 22A — Missing Utility Nodes

- [ ] `SituationCreateNode(graph, SITUATION_NODE_GAIN)` — returns SUCCESS after registration
- [ ] `SituationCreateNode(graph, SITUATION_NODE_MIXER)` — returns SUCCESS after registration
- [ ] Verify GAIN metadata: category = UTILITY, num_controls >= 1

### 22B — Modulator Nodes

- [ ] `SituationCreateNode(graph, SITUATION_NODE_LFO)` — returns SUCCESS
- [ ] `SituationCreateNode(graph, SITUATION_NODE_ENVELOPE_FOLLOWER)` — returns SUCCESS
- [ ] Verify LFO metadata: category = MODULATOR, has rate/depth/waveform controls
- [ ] Verify ENVELOPE_FOLLOWER metadata: category = MODULATOR, has attack/release controls
- [ ] Create LFO → REVERB control patch — verify control modulation works

### 22C — Analyzer Nodes

- [ ] `SituationCreateNode(graph, SITUATION_NODE_SPECTRUM_ANALYZER)` — returns SUCCESS
- [ ] `SituationCreateNode(graph, SITUATION_NODE_PEAK_METER)` — returns SUCCESS
- [ ] Verify SPECTRUM_ANALYZER metadata: category = ANALYZER
- [ ] Verify PEAK_METER metadata: category = ANALYZER

### 22D — Patch Disconnect (Post-Export Fix)

- [ ] `SituationRemovePatch(graph, src, 0, dst, 0, false)` — disconnect audio patch, returns SUCCESS
- [ ] `SituationRemovePatch(graph, src, 0, dst, 0, true)` — disconnect control patch, returns SUCCESS
- [ ] Verify node is disconnected after remove (re-patch succeeds)

### 22E — Bus Effect Insert/Remove

- [ ] `SituationInsertEffect(bus, 0, effect_node)` — insert effect at slot 0, returns SUCCESS
- [ ] `SituationRemoveEffect(bus, 0)` — remove effect, returns non-NULL pointer
- [ ] `SituationRemoveEffect(bus, 0)` on empty slot — returns NULL (graceful)

**Estimated effort**: 1 hour (once library changes are made)  
**Dependency**: Library DLL rebuild with registered modulators/analyzers + exported SituationRemovePatch  
**Status**: Blocked — waiting on library-side implementation.

---

## Design Decisions

1. **No external framework** — Zero dependencies beyond C11 stdlib. Matches library philosophy.
2. **Single binary** — All modules compile into one executable. Simple CI integration.
3. **Module isolation** — Each module owns its init/shutdown. A GPU failure skips graphics tests but filesystem still runs.
4. **setjmp recovery** — One failing assertion doesn't abort the suite. Every test gets a chance.
5. **10s timeout** — Catches deadlocks in threading tests without blocking CI forever.
6. **`_sit_test_` prefix** — All temp artifacts are namespaced for safe cleanup.
7. **No audio output verification** — Unlike graphics (where we readback pixels), audio tests verify API contracts (return codes, state changes, no crashes). We can't easily verify audio output in an automated test.
8. **Graceful hardware absence** — MIDI tests handle "no MIDI device" gracefully. Audio device tests handle "no audio device" gracefully. Tests pass on headless CI.
9. **Parametric effect sweep** — Phase 15 uses a loop over all 26 device types rather than 26 individual test functions. One test function, many iterations.
10. **Holistic platform** — Graphics and audio share one plan, one harness, one binary. No duplication, no drift between subsystems.

---

## What This Does NOT Change

- [x] No modifications to `situation_api.h` or any library source *(the harness itself stays additive; library and examples evolve separately.)*
- [x] No new public API functions *required* by the harness
- [x] No new external dependencies in the harness
- [x] Harness build stays **`build_tests.bat`** + `tests/harness/*.c`
- [ ] No duplication of existing 33 audio tests (Phases 12–18 cover only untested API surface)

Examples / `build_examples.bat` / library code may change for demos or fixes; that does not change the harness layout above.

---

## Test Assets Required

- [x] `tests/harness/assets/test_sine.wav` — 1-second 440Hz sine wave (16-bit, 44100Hz, mono). Generate with script or embed as raw bytes.
- [x] Minimal shader source strings (embedded in `test_graphics.c` as `const char*`)
- [x] No external model files needed (mesh tests use programmatic vertex data)
- [x] Minimal GLTF test model (embedded or generated) — single triangle with vertex colors (Phase 11D)
- [x] Compute shader source strings for roundtrip tests (embedded in `test_graphics.c`, Phase 10)
- [x] Checkerboard texture data (programmatic 4×4 RGBA, Phase 8A)

---

**Author**: Kiro  
**Status**: Phases 1–21 complete (Phase 21 partial — async I/O only, path utils/watchers not in API). Phase 22 blocked on library-side registration. **Harness**: OpenGL full **310/310** ✅; Vulkan graphics **78/78** ✅; Vulkan full-chain + OpenGL re-check after audio — see snapshot + Bug 6.  
**Spec**: `.kiro/specs/situation-test-harness/`
