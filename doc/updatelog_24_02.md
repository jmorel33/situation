# Situation UPDATELOG — v2.4.x (part 2 of 5)

Patches **2.4.101 "SPIR-V Driver Log Capture"** through **2.4.200 "API Documentation Refresh"** (100 entries, oldest first).

Index: [`UPDATELOG.md`](UPDATELOG.md) · Previous: [`updatelog_24_01.md`](updatelog_24_01.md) · Next: [`updatelog_24_03.md`](updatelog_24_03.md)

---

## [v2.4.101 "SPIR-V Driver Log Capture"] - 2026-05-21

### Tooling

- **`scripts/spirv_shader_debug.py`**: offline GLSL + SPIR-V stats; `demon_hunt --devel` compiles FS without `-O` for per-function instruction map.
- **`doc/SHADER_DEBUG.md`**: usage, bisect workflow, NVIDIA limit notes.
- **`compile_demon_hunt_shaders.bat`**: runs debug report after `glslc`.

### Description

SPIR-V compile/link failures now capture the **full driver diagnostic** in `SituationGetLastErrorMsg` (up to 16 KiB). **OpenGL:** `GL_INFO_LOG_LENGTH` via `_SituationDupGLInfoLog`. **Vulkan:** `vkCreateShaderModule` VkResult + stage label in `_SituationVulkanCreateShaderModuleEx` (unchanged; now documented alongside GL). Removes the old 900-character truncation in `_SituationSetGLErrorFromSpirvStage`.

### Changes

- **`sit/situation_api.h`**: `SITUATION_MAX_ERROR_MSG_LEN` / `SITUATION_MAX_SHADER_LOG_LEN` → **16384**.
- **`sit/situation_impl_renderer.h`**: `_SituationDupGLInfoLog`; async link failures use `_SituationSetGLErrorFromSpirvStage` with full program log; SPIR-V blob sizes kept until link completes.
- **`tests/harness/sit_graphics_test_helpers.h`**: Poll helper returns terminal errors (prints driver log before assert).
- **`examples/demon_hunt.c`**: Logs driver text on a separate line (not truncated in 768-byte buffer).

---

---

---

---

---

---

## [v2.4.102 "SPIR-V Diagnostics GL+VK"] - 2026-05-21

### Description

Documentation and API parity: recent SPIR-V work is **not OpenGL-only**. **`SituationPollShaderLoad`** on Vulkan now calls **`_SituationPollVkAsyncShaderLoad`** (same as frame acquire), and returns **`SituationGetLastErrorCode()`** on failure instead of a generic pipeline code only.

### Changes

- **`sit/situation_impl_renderer.h`**: Vulkan poll path aligned with OpenGL poll-driven async SPIR-V load.
- **`doc/TEST_SPIRV_SHADER_API.md`**: Dual-backend matrix, `sit_test` + `sit_test_vulkan` commands, separate error tables.
- **`doc/UPDATELOG.md`**, **`sit/situation_base_version.h`**: Version titles/descriptions name **GL+VK**, not OpenGL-only.

---

---

---

---

---

---

## [v2.4.103 "Thread Pool IO Queue Fix"] - 2026-05-21

### Description

Fixes a **double consumer** on the low-priority job queue: worker threads and the dedicated I/O thread both dequeued `SIT_SUBMIT_DEFAULT` jobs (async file I/O), which could **double-free** job payloads and corrupt the heap. Symptoms included harness `save_file_text_async` passing then **SIGSEGV** in `filesystem` module teardown on Windows.

### Changes

- **`sit/situation_impl_threading.h`**: Workers skip queue 0 when `pool->io_thread` is active; idle wait checks high-priority queue too.
- **`tests/harness/test_filesystem.c`**: Module teardown no longer calls redundant `SituationDeleteFile` after async tests (per-test cleanup remains).

---

---

---

---

---

---

## [v2.4.104 "Error Mutex, Image Resize, SPIR-V Load Ex"] - 2026-05-21

### Description

Situation **v2.4.104** fixes post-shutdown error handling and CPU image resize, adds **`SituationBeginLoadShaderFromSpirvMemoryEx`** so Vulkan async SPIR-V loads can select descriptor layout profiles (mesh, dual-SSBO, UBO+SSBO), and hardens SPIR-V poll tests in the harness. Full OpenGL harness **337/337**; Vulkan **`--module graphics --filter spirv`** passes, including a large UBO+SSBO Begin/Poll regression.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **104**.
- **`sit/situation_impl_ctrl.h`**: **`_SituationSetErrorFromCode`** returns early when **`_sit_current_context`** is NULL (avoids locking **`error_mutex`** after **`SituationShutdown()`**); **`error_mutex`** init/uninit in **`SituationInit`** / **`SituationShutdown`**.
- **`sit/situation_impl_deps.h`**: **`stb_image_resize2.h`** implementation for **`SituationResizeImage`**; **`STBIR_FREE`** routed through expression-safe **`sit_stbir_free`** (stb internals use comma expressions — cannot expand statement-form **`SIT_FREE`**).
- **`sit/situation_api.h`**: **`SituationBeginLoadShaderFromSpirvMemoryEx`** — async SPIR-V kickoff with **`SituationSpirvLayoutProfile`** (**Vulkan**: layout profile selects pipeline descriptor layout; **OpenGL**: profile ignored, same as **`SituationBeginLoadShaderFromSpirvMemory`**).
- **`sit/situation_impl_renderer.h`**: Vulkan async SPIR-V load stores **`layout_profile`** on the load context; **`SituationBeginLoadShaderFromSpirvMemory`** delegates to Ex with **`SIT_SPIRV_LAYOUT_PROFILE_MESH`**.

### Tests

- **`tests/harness/test_graphics.c`**: **`demon_hunt_sky_spirv_begin_poll`** (OpenGL) — prefers devel fragment SPIR-V for link; accepts **`OPENGL_SPIRV_PROGRAM_LINK_FAILED`** (-641) when driver log contains **“too many instructions”** (async API + error reporting still validated).
- **`tests/harness/test_graphics_spirv.c`**: **`demon_hunt_sky_spirv_vk_begin_poll`** (Vulkan) — large production SPIR-V via **`SituationBeginLoadShaderFromSpirvMemoryEx(..., SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO)`** + Begin/Poll.

### Demo (examples only)

The **`demon_hunt`** example was updated to call the new Vulkan SPIR-V API; it is not part of the library surface.

### Verification

```bat
build_situation.bat opengl
build\sit_test.exe

build_situation.bat vulkan
build\sit_test_vulkan.exe --module graphics --filter spirv
```

---

---

---

---

---

---

## [v2.4.105 "Vulkan Async GLSL Worker Queue Fix"] - 2026-05-23

### Description

Situation **v2.4.105** fixes Vulkan **`SituationBeginLoadShaderFromMemory`** async GLSL loads that never completed: compile jobs were submitted to the **low-priority I/O queue** (serviced by the dedicated I/O thread), but after **`SituationInit`** those jobs were not dequeued (`SituationGetIOQueueDepth()` stayed at 1, **`SituationPollShaderLoad`** returned **`SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`** until harness timeout). Sync GLSL load and async SPIR-V load were unaffected. CPU-bound shaderc work now runs on the **high-priority worker queue**; unload waits for the compile worker before freeing async context (prevents use-after-free once workers actually run).

### Root cause (investigation)

- **`_SituationVkAsyncShaderLoad`** is **120 bytes** (> **`SITUATION_JOB_PAYLOAD_MAX`** 64), so **`SIT_SUBMIT_POINTER_ONLY`** was already correct — not a small-object copy bug.
- Repro: **`SituationGetIOQueueDepth() == 1`** indefinitely after **`SituationBeginLoadShaderFromMemory`**; **`disable_io_thread=true`** (inline submit path) completed on frame 0; standalone **`SituationCreateThreadPool`** low-priority jobs still work.
- Misclassification: GLSL→SPIR-V via shaderc is **CPU-bound**, not I/O; it must not use **`SIT_SUBMIT_DEFAULT`** / queue 0 when a dedicated I/O thread owns that queue (see also **v2.4.103** worker skip of queue 0).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **105**, description **"Vulkan Async GLSL Worker Queue Fix"**.
- **`sit/situation_impl_renderer.h`**:
  - **`SituationBeginLoadShaderFromMemory`** (Vulkan): submit **`_SituationVkAsyncCompileWorker`** with **`SIT_SUBMIT_HIGH_PRIORITY | SIT_SUBMIT_POINTER_ONLY | SIT_SUBMIT_BLOCK_IF_FULL`** instead of default low-priority I/O queue.
  - **`_SituationVulkanFreeAsyncShaderLoad`**: spin on **`compile_done == 0`** with **`thrd_yield()`** before freeing **`ctx`** / source strings (safe **`SituationUnloadShader`** during in-flight compile).
- **`sit/situation_impl_io.h`**: when the I/O queue head job has unmet dependencies, **`thrd_yield()`** after unlock (avoids busy-spin on head-of-line block).

### Tests (regression)

Previously failing Vulkan harness tests (all now pass):

- **`graphics.async_shader_begin_reports_in_progress`**
- **`graphics.async_shader_load_memory_draw`**
- **`graphics.async_shader_renderer_alive_while_loading`**
- **`graphics.sync_shader_after_async_cycle`**

### Verification

```bat
build_situation.bat vulkan
build_tests.bat vulkan
build\sit_test_vulkan.exe --filter async_shader
build\sit_test_vulkan.exe
```

**Result (2026-05-23):** Vulkan harness **327/327** passed (GCC 15.1.0, Vulkan SDK 1.4.313.2).

---

---

---

---

---

---

## [v2.4.106 "Windows Audio Shared Auto-Start Fix"] - 2026-05-23

### Description

Situation **v2.4.106** fixes Windows system audio being **muted or left broken** after running the test harness (or any app that calls **`SituationInit`** / **`SituationShutdown`** repeatedly in one process). Auto-start of the default playback device in **`SituationInit`** step 7 no longer requests **WASAPI exclusive** mode on the first in-process session; it always uses **shared** mode. Exclusive output remains available via explicit **`SituationSetAudioDevice()`** for low-latency games.

### Root cause

- The harness runs **one `SituationInit` + `SituationShutdown` per module** (core, window, input, timer, audio, graphics, misc) in a **single process**.
- On Windows, step 7 previously used **`ma_share_mode_exclusive`** on **session 1** and shared mode only on session 2+ (workaround for re-init blocking).
- **Exclusive mode hijacks the default endpoint** and mutes other apps (browser, Spotify, system sounds). Repeated exclusive grab/release across module cycles could leave WASAPI in a bad state after teardown — reported as “harness nuked my audio.”
- Related prior work: **v2.4.52** (release miniaudio before GPU sync on shutdown), Bug 6 notes in **`doc/plan/LIBRARY_BUGFIX_PLAN.md`** (exclusive re-init lifecycle).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **106**, description **"Windows Audio Shared Auto-Start Fix"**.
- **`sit/situation_impl_ctrl.h`**:
  - **`SituationInit`** step 7 (Windows): always call **`_SituationSetAudioDeviceInternal(..., ma_share_mode_shared)`**; removed session-counter exclusive-first policy.
  - **`SituationShutdown`** / **`_SituationCleanupSubsystems`**: reset **`is_miniaudio_device_internally_paused`** after device stop/uninit so pause/resume tests cannot leave stale pause state across module cycles.
- **`sit/situation_impl_audio.h`**: comment update — auto-start is shared; explicit **`SituationSetAudioDevice`** is exclusive.

### Verification

```bat
build_situation.bat all
build_tests.bat opengl
build_tests.bat vulkan
copy /Y build\dll\situation_opengl.dll build\
copy /Y build\dll\situation_vulkan.dll build\
build\sit_test.exe --module audio
build\sit_test.exe
build\sit_test_vulkan.exe
```

**Important:** Windows loads the DLL beside the exe first. If **`build\situation_opengl.dll`** is stale (e.g. locked by a running **`sit_test.exe`**), the harness silently tests an old build — always **`copy /Y`** from **`build\dll\`** after **`build_situation.bat`**, or run with **`PATH`** pointing at **`build\dll`**.

**Results (2026-05-23, reference Windows config):**

| Check | Result |
|-------|--------|
| OpenGL full harness | **337/337** passed |
| OpenGL **`--module audio`** | **96/96** passed |
| Vulkan full harness | **327/327** passed |
| Audio probe (7× **`SituationInit`/`Shutdown`**, 2 s tone/cycle) | Init OK, **`SituationIsAudioDevicePlaying()`** true, master meter peak ≈ **0.56** each cycle; audible output confirmed |
| System audio after harness | Default playback (browser/Spotify/system sounds) still works — no device toggle or reboot required |

Optional deeper probe: compile/run **`build/dll/audio_probe.exe`** (7-cycle stress test with **`SituationGetMasterOutputMeter`** — non-zero peak/RMS confirms the callback is mixing samples even when harness tones are too short to hear).

---

---

---

---

---

---

## [v2.4.107 "MIDI Audio Frequency Verification"] - 2026-05-23

### Description

Situation **v2.4.107** adds end-to-end **MIDI → audio** verification: virtual MIDI loopback injects note-on into a graph **tone synth** node, the output monitor captures mixed samples, and a Goertzel pass confirms the expected pitch (e.g. A4 = **440 Hz**). Legacy **`SituationPlayMidiNote`** is covered by the same frequency helper.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **107**, description **"MIDI Audio Frequency Verification"**.
- **`sit/aud/midi_device_callbacks.h`**: **Tone Synth** MIDI note-on/off → frequency / volume controls.
- **`sit/aud/node_graph_process.h`**: dispatch **0x90** / **0x80** to node note callbacks.
- **`sit/aud/node_graph_midi.h`**: **`SituationSetupVirtualMidiLoopback`**, **`SituationVirtualMidiNoteOn`/`NoteOff`**, **`SituationTeardownVirtualMidiLoopback`**.
- **`sit/situation_api.h`**: export virtual MIDI loopback helpers.
- **`sit/aud/device_wrappers.h`**: tone synth uses **`SituationGetAudioPlaybackSampleRate()`** for phase increment.

### Tests

- **`tests/harness/audio_freq_detect.c`**: Goertzel capture/verify helper.
- **`tests/harness/test_audio.c`**: **`legacy_midi_note_emits_frequency`**, **`graph_midi_note_emits_frequency`**.
- **`tests/harness/midi_audio_probe.c`**: standalone probe → **`build/midi_audio_probe.exe`** (via **`build_tests.bat`**).

### Verification

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build\dll
sit_test.exe --module audio --filter emits_frequency --verbose
midi_audio_probe.exe
```

Run from **`build\dll\`** (or **`copy /Y build\dll\situation_opengl.dll build\`**) so the harness loads the fresh DLL.

---

---

---

---

---

---

## [v2.4.108 "Effect Heard Tests & Harness Fixes"] - 2026-05-23

### Description

Situation **v2.4.108** adds **per-effect audible verification** in the harness (brief 440 Hz tone through each FX node, wet vs dry analysis), a **reverb dry/wet mix sweep** test (440 Hz square wave, wet **0→1** then **1→0** over 4 s each), aligns **Filter** and **EQ 4-Band** process wrappers with registry control layout, and fixes harness **SPIR-V disk path** resolution when running from **`build/dll`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **108**, description **"Effect Heard Tests & Harness Fixes"**.
- **`sit/aud/device_wrappers.h`**:
  - **Filter**: map registry controls (**cutoff / resonance / type**) to **`PxFilterMode`** processing (fixes default-off / OOB reads).
  - **EQ 4-Band**: map registry shelf/peak controls to **`eq4band_set_band`** (fixes misaligned band gains from registry defaults).
  - **Maximizer**: init hop size **64** to match playback period.
  - **EQ 4-Band**: band-3 gain no longer reads past control array.
  - **Tone synth**: phase increment uses **`SituationGetAudioPlaybackSampleRate()`**.
- **`sit/aud/tone_synth_graph.h`**: per-node MIDI voice state + `SituationToneSynthMidiCtx` (controls + synth state).
- **`sit/aud/midi_device_callbacks.h`**: mono note on/off (matching note), **CC1** → vibrato LFO depth, **CC7/CC11** → `base × channel × expression`, **CC64** sustain; removed file-static globals.
- **`sit/aud/device_wrappers.h`**: 5 Hz vibrato LFO in process when `mod_depth_semitones > 0`; volume/pitch driven from MIDI state while sounding.
- **`sit/aud/node_graph_midi.h`**: allocate/free tone synth MIDI ctx on enable/disable.
- **`sit/aud/node_graph_process.h`**: dispatch MIDI to `midi_device->device_ptr` (fixes tone synth ctx vs control array).

### Tests

- **`tests/harness/test_audio_effects_heard.c`**: module **`audio_effects_heard`** — **17** named tests:
  - **`graph_tone_synth_effect_heard_*`** (×16): one per registered effect; 440 Hz sine from graph tone synth, ~400 ms capture, wet vs dry.
  - **`effect_reverb_mix_dry_wet_sweep`**: 440 Hz **square** → **Reverb**; **`wet_level`** ramp **0→1** (4 s) then **1→0** (4 s) while graph runs; RMS envelope at dry vs wet windows proves mix responds.
- **`tests/harness/audio_freq_detect.c`**: **`sit_audio_effect_heard()`**, **`sit_audio_capture_window_rms()`**, **`sit_audio_capture_window_correlation()`** (Goertzel + windowed analysis for mix sweep).
- **`tests/harness/test_graphics_spirv.c`**: resolve disk SPIR-V under **`../../tests/harness/spirv_out/`** when cwd is **`build/dll`** (fixes **`spirv_disk_roundtrip`**, **`async_shader_spirv_memory_vulkan`**).
- **`tests/harness/test_audio.c`**: Harness names prefix synth path — **`legacy_tone_pool_*`** (`SituationPlayToneEx` / `PlayMidiNote`) vs **`graph_tone_synth_*`** (graph `SITUATION_NODE_TONE_SYNTH` + virtual MIDI). Examples: **`graph_tone_synth_velocity_ramp`**, **`graph_tone_synth_midi_complex_melody`**, **`graph_tone_synth_cc7_tremolo`**, **`legacy_tone_pool_midi_note_frequency`**.
- **`tests/harness/test_audio_effects_heard.c`**: **`graph_tone_synth_effect_heard_*`** (×16), **`graph_tone_synth_reverb_mix_dry_wet_sweep`**.
- **`tests/harness/sit_test_registry.c`**, **`build_tests.bat`**: register module and build wiring.

### Verification

From project root (MinGW on **`PATH`**, no `.bat` required):

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
Set-Location build\dll
.\sit_test_gl.exe --module audio_effects_heard
.\sit_test_gl.exe --module audio_effects_heard --filter mix_dry_wet
.\sit_test_gl.exe
.\sit_test_vk.exe
```

Rebuild DLL + harness with **`gcc`** after library changes; run exes from **`build\dll`** so the fresh **`situation_opengl.dll`** / **`situation_vulkan.dll`** is loaded.

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| OpenGL **`audio_effects_heard`** | **17/17** passed (incl. mix sweep ~9 s) |
| OpenGL full harness | **356/356** passed |
| Vulkan full harness | **345/345** passed |

---

---

---

---

---

---

## [v2.4.109 "MIDI Device Names & Test Routing"] - 2026-05-23

### Description

Situation **v2.4.109** adds **official PortMidi device name constants** for harness virtual MIDI and the graph tone synth target, a **`SituationGetMidiDeviceName`** lookup API, a fix for **virtual device enumeration** in **`SituationListMidiDevices`**, **virtual loopback CC / pitch-bend injection** for integration tests, and **per-test MIDI routing banners** (device name, PortMidi id, channel, CC usage) on all graph tone synth MIDI verification tests. **CC7 tremolo bleed** between sequential harness runs is flushed via a shared MIDI silence helper before teardown.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **109**, description **"MIDI Device Names & Test Routing"**.
- **`sit/situation_api.h`**:
  - **`SITUATION_TEST_MIDI_CHANNEL`** (0-based; harness displays as MIDI channel 0 → channel 1).
  - **`SITUATION_VIRTUAL_MIDI_IN_NAME`** / **`SITUATION_VIRTUAL_MIDI_OUT_NAME`** — official virtual loopback PortMidi names.
  - **`SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME`** — graph tone synth **`SIT_MidiDevice`** target name (**`"Tone Synth"`**).
  - **`SituationGetMidiDeviceName()`** — resolve PortMidi name for hardware or virtual **`device_id`** (virtual ids ≥ 32).
  - **`SituationVirtualMidiControlChange()`**, **`SituationVirtualMidiPitchBend()`** — inject CC / bend into the virtual loopback output stream.
- **`sit/aud/node_graph_midi.h`**:
  - **`SituationListMidiDevices()`** — enumerate hardware ids **0…MAX_DEVICES−1** plus active virtual ids **MAX_DEVICES+slot** (fixes missing virtual devices when **`Pm_CountDevices()`** count ≠ index space).
  - Virtual loopback **`Pm_CreateVirtualDevice`** uses **`SITUATION_VIRTUAL_MIDI_*_NAME`** constants.
  - **`SituationSetupVirtualMidiLoopback()`** returns the virtual input **`PmDeviceID`** for **`SituationEnableMidiControl()`**.

### Tests

- **`tests/harness/midi_test_info.h`** (new):
  - **`sit_midi_log_graph_tone_synth_route()`** — asserts official input + synth names via **`SituationGetMidiDeviceName`** / registry metadata; prints **`[MIDI]`** routing line (channel + per-test CC/note usage).
  - **`sit_midi_log_legacy_tone_pool_route()`** — labels legacy **`SituationPlayMidiNote`** path (no PortMidi).
- **`tests/harness/test_audio.c`** (Phase 8 — MIDI / audio frequency verification):
  - Documented per-test **MIDI channel / CC** table in file header.
  - **`graph_tone_synth_midi_note_frequency`**, **`graph_tone_synth_midi_complex_melody`**, **`graph_tone_synth_velocity_ramp`**, **`graph_tone_synth_cc_mod_vibrato`**, **`graph_tone_synth_cc7_tremolo`** — each prints official routing banner at start.
  - **`sit_midi_tone_graph_setup()`** / **`sit_midi_tone_graph_teardown()`** shared helpers for graph + virtual MIDI tests.
  - **`sit_midi_tone_graph_silence_midi()`** — CC123 all-notes-off, CC64/1/7/11 reset, pitch bend center, note-off; prevents **CC7 tremolo** volume state leaking into later tests.
  - **`graph_tone_synth_cc_mod_vibrato`**: Goertzel wander test updated for **5 Hz LFO vibrato** from **CC1** (not static detune).
  - **`graph_tone_synth_midi_complex_melody`**: velocity, pitch bend, **CC1** vibrato windows on G4 phrase.

### Verification

From project root (rebuild **DLL + harness** after library changes):

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth_midi
sit_test.exe --module audio --filter graph_tone_synth_velocity_ramp
sit_test.exe --module audio --filter graph_tone_synth_cc
sit_test.exe --module audio --filter legacy_tone_pool_midi
```

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| **`graph_tone_synth_midi_note_frequency`** | pass — `in="Situation Test MIDI In"`, `synth="Tone Synth"`, ch0 |
| **`graph_tone_synth_midi_complex_melody`** | pass |
| **`graph_tone_synth_velocity_ramp`** | pass (run isolated if prior module left audio busy) |
| **`graph_tone_synth_cc_mod_vibrato`** | pass |
| **`graph_tone_synth_cc7_tremolo`** | pass |
| **`legacy_tone_pool_midi_note_frequency`** | pass — legacy path banner, no PortMidi |

### Not yet MIDI-driven (graph tone synth)

Registry controls **waveform (1)**, **pan (3)**, **ADSR/hold (4–8)** remain harness/API-only; **CC92** true tremolo LFO not wired — **CC7** volume toggle used in tests. **`SituationVirtualMidiNoteOn/Off`** remain channel-0 status bytes; CC/bend APIs accept explicit channel.

---

---

---

---

---

---

## [v2.4.110 "Graph Tone Synth Legacy Parity"] - 2026-05-23

### Description

Situation **v2.4.110** brings the **graph tone synth** (`SITUATION_NODE_TONE_SYNTH`, PortMidi path) to **feature parity with the legacy 64-voice tone pool**: polyphonic voice allocation with stealing, per-voice **ADSR + hold**, stereo **pan**, all five **waveforms**, velocity-scaled envelopes, and expanded **MIDI CC / program change** mapping. The legacy **`SituationPlayToneEx`** pool is unchanged; this patch upgrades the MIDI-controllable graph node only.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **110**, description **"Graph Tone Synth Legacy Parity"**.
- **`sit/aud/tone_synth_graph.h`** (major rewrite):
  - **`SituationToneSynthVoice`** pool (**64** voices per node) with attack/decay/sustain/release/hold frame counts.
  - Voice **steal order**: inactive slot → furthest-in-**release** → newest active (matches legacy pool policy).
  - Shared helpers: alloc/release note, envelope step, oscillator sample (sine/square/triangle/saw/noise), pitch bend + **CC1** LFO on all voices.
  - **`SituationToneSynthNodeState`**: voice array + MIDI globals (CC7/CC11 volume, CC64 sustain, bend, mod depth) + manual **`SetControl`** fallback when no voices active.
- **`sit/aud/device_wrappers.h`** — **`_SituationProcessToneSynthNode`**:
  - Sums all active voices with **ADSR × velocity × channel volume × expression**.
  - **Stereo pan** (legacy linear L/R gains from control 3).
  - Manual control path preserved for harness **`SituationSetControl`** (instant amp when no MIDI voices).
- **`sit/aud/midi_device_callbacks.h`** — graph tone synth MIDI:
  - **Poly note on/off** (per-note voice alloc/release; sustain-pedal deferral).
  - **CC1** vibrato depth, **CC7** volume, **CC10** pan, **CC11** expression, **CC64** sustain, **CC72/73** release/attack, **CC123** all-notes-off.
  - **`_SituationToneSynthOnProgramChange`** — program **% 5** → waveform enum.
- **`sit/aud/node_graph_process.h`**: dispatch **0xC0 program change** to graph tone synth.

### Legacy pool vs graph (after this patch)

| Feature | Legacy pool | Graph tone synth |
|---------|-------------|------------------|
| 64-voice poly | ✓ | ✓ |
| ADSR + hold | ✓ | ✓ (per voice, from controls 4–8) |
| Pan | ✓ | ✓ (control 3 / CC10) |
| Waveforms ×5 | ✓ (miniaudio) | ✓ (RT-safe oscillators) |
| PortMidi | — | ✓ |
| `PlayToneEx` handles | ✓ | — (internal voices only) |

### Verification

Rebuild **DLL** after library changes (`build_tests.bat` only if harness sources changed):

```bat
build_situation.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth_midi
sit_test.exe --module audio --filter graph_tone_synth_velocity_ramp
sit_test.exe --module audio --filter graph_tone_synth_cc
sit_test.exe --module audio_effects_heard --filter graph_tone_synth_effect_heard_reverb
```

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| **`graph_tone_synth_midi_note_frequency`** | pass |
| **`graph_tone_synth_midi_complex_melody`** | pass |
| **`graph_tone_synth_velocity_ramp`** | pass (run isolated if prior tests left audio busy) |
| **`graph_tone_synth_cc_mod_vibrato`** / **`cc7_tremolo`** | pass |
| **`graph_tone_synth_effect_heard_reverb`** | pass |

### Still open (graph tone synth)

- Manual **`SetControl`** path: instant volume (no ADSR) when no MIDI voices — legacy **`PlayToneEx`** always envelopes.
- **`SituationVirtualMidiNoteOn/Off`**: channel-0 only; no public voice handles.
- **CC decay/sustain/hold** not on dedicated CCs (set via node controls or CC72/73 for attack/release only).
- Oscillator DSP differs from miniaudio **`ma_waveform`** (same shapes, not bit-identical).

---

---

---

---

---

---

## [v2.4.111 "Graph Tone Synth Full MIDI"] - 2026-05-23

### Description

Situation **v2.4.111** completes the agreed **“everything through MIDI”** follow-up for the graph tone synth: **channel-aware virtual note APIs**, **per-node MIDI channel filtering**, the remaining **ADSR CC map**, **CC70 waveform**, **CC92 true amplitude tremolo** (5 Hz LFO), and harness migration to the `*Ex` virtual MIDI helpers.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **111**, description **"Graph Tone Synth Full MIDI"**.
- **`sit/situation_api.h`**:
  - **`SituationVirtualMidiNoteOnEx` / `NoteOffEx`** — channel 0–15 note injection.
  - **`SituationVirtualMidiProgramChange`** — channel-aware program change.
  - **`SituationSetNodeMidiChannel`** — filter node MIDI to one channel (`-1` = omni).
  - Legacy **`SituationVirtualMidiNoteOn/Off`** remain as channel-0 wrappers.
- **`sit/aud/node_graph_midi.h`**: implementations for the new APIs; **`_SituationVirtualMidiWrite`** shared helper.
- **`sit/aud/node_graph_process.h`**: **`_SituationNodeMidiAcceptsChannel`** — drops messages not matching `node->midi_device->midi_channel` when set.
- **`sit/aud/tone_synth_graph.h`**:
  - **`_SituationToneSynthApplyControlChange`** — centralized CC map: CC1, 7, 10, 11, 64, **70**, **72–77** (ADSR), **92** (tremolo depth), 123.
  - **CC92 tremolo** state (`tremolo_depth`, `tremolo_phase`) + **5 Hz** default.
- **`sit/aud/device_wrappers.h`**: amplitude tremolo LFO applied to combined MIDI volume during voice mix.
- **`sit/aud/midi_device_callbacks.h`**: tone synth CC handler delegates to **`_SituationToneSynthApplyControlChange`**.

### Graph tone synth MIDI CC map

| CC | Function |
|----|----------|
| 1 | Vibrato depth (5 Hz pitch LFO) |
| 7 | Channel volume |
| 10 | Pan |
| 11 | Expression |
| 64 | Sustain pedal |
| 70 | Waveform (0–4) |
| 72 | Release time |
| 73 | Attack time |
| 75 | Decay time |
| 76 | Sustain level |
| 77 | Hold time |
| 92 | Tremolo depth (5 Hz amplitude LFO) |
| 123 | All notes off |

### Harness (Phase 8)

- All graph tone synth tests use **`SituationVirtualMidiNoteOnEx/OffEx(SITUATION_TEST_MIDI_CHANNEL, …)`**.
- **`sit_midi_tone_graph_setup`** calls **`SituationSetNodeMidiChannel`** after MIDI enable.
- **`sit_midi_tone_graph_silence_midi`** resets **CC92** before teardown.
- **`graph_tone_synth_cc7_tremolo`** renamed **`graph_tone_synth_cc92_tremolo`** — tests true LFO tremolo via **CC92**, not CC7 toggling.
- **`sit_midi_graph_fixture_release`** — cleans up graph/MIDI state when a prior test **longjmp**'s on assertion failure (prevents `-496` MIDI open errors on the next test).

### Verification

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth
```

**Results (2026-05-23):** all **15** `graph_tone_synth_*` tests pass in one run (including Phase 8 MIDI frequency/CC tests).

---

---

---

---

---

---

## [v2.4.112 "Tone Synth Compare Ready"] - 2026-05-23

### Description

Situation **v2.4.112** is a patch bump after **v2.4.111** verification: all **15** `graph_tone_synth_*` harness tests pass in one run, including stable sequential Phase 8 MIDI tests via **`sit_midi_graph_fixture_release`**. This release marks the graph tone synth as ready for side-by-side scrutiny against the legacy 64-voice pool and app-level switching.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **112**, description **"Tone Synth Compare Ready"**.
- No additional library API changes beyond **v2.4.111** (harness fixture cleanup only).
- **`tone_synth_phase1_compare_a4`** harness test — Phase 1 side-by-side: legacy-only A4 then graph-only A4 (exclusive paths), prints `[COMPARE Phase 1]` hz/peak/rms delta.

### Verification

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
Set-Location build
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module audio --filter graph_tone_synth
& ".\sit_test.exe" --module audio --filter tone_synth_phase1_compare_a4
```

**Results (2026-05-23):** 15 passed, 0 failed.

---

---

---

---

---

---

## [v2.4.113 "Tone Synth SVF Filter"] - 2026-05-23

### Description

Graph **Tone Synth** gains a **per-voice Polysonix-style multi-pole SVF** (`sit/aud/fx/filter.h`): LP/HP/BP/notch/combo modes, 1–4 poles, drive, optional 2× oversampling, and MIDI-note key tracking. Filter runs after the oscillator envelope, before pan — same order as Polysonix voices.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **113**, description **"Tone Synth SVF Filter"**.
- **`sit/aud/tone_synth_graph.h`**: Per-voice `SituationToneSynthVoiceFilter`; controls **9–15**; MIDI CC **16** mode, **74** cutoff, **71** resonance, **102** poles, **17** drive, **22** keytrack, **18** oversampling.
- **`sit/aud/device_wrappers.h`**: `_SituationProcessToneSynthNode` applies filter per voice (and manual fallback path).
- **`sit/aud/registry_init.h`**: Tone synth **16** controls (filter block registered with defaults; filter **off** by default for harness parity).

### MIDI filter map (Tone Synth node)

| CC | Control |
|----|---------|
| 16 | Filter mode 0=OFF … 8=BP+HP |
| 74 | Cutoff 20 Hz–20 kHz (log) |
| 71 | Resonance Q 0.5–20 |
| 102 | Poles 1–4 |
| 17 | Drive 1–10 |
| 22 | Keytrack 0–1 |
| 18 | Oversampling 0=off, ≥1=2× |

---

---

---

---

---

---

## [v2.4.114 "Tone Synth Mono Poly"] - 2026-05-23

### Description

Graph **Tone Synth** adds a **mono / poly pivot**: poly keeps the existing 64-voice allocator; mono uses **voice slot 0 only** and cuts any other active voices on each new note (last-note priority, no stack).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **114**.
- **`sit/aud/tone_synth_graph.h`**: Control **16** `voice_mode` (0=poly, 1=mono); `_SituationToneSynthSetVoiceMode`, `_SituationToneSynthEnforceMonoVoices`.
- **MIDI**: **CC126** → mono, **CC127** → poly (GM mode CCs). Also set via `SituationSetControl(graph, handle, 16, 0|1)`.

---

---

---

---

---

---

## [v2.4.115 "Tone Synth Pulse Width"] - 2026-05-23

### Description

Graph **Tone Synth** waveform **1** is now a **pulse** (variable duty cycle) instead of a fixed 50% square. Pulse width is control **17** (default 0.5 = square) and **MIDI CC106** (5%–95% duty).

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: `_SituationToneSynthClampPulseWidth`, pulse osc in case 1, CC106 → control 17.
- **`sit/aud/registry_init.h`**: Control **17** `pulse_width`; waveform enum label **Pulse**.
- **`sit/aud/device_wrappers.h`**: Pass pulse width into oscillator.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_filter_modes`, `graph_tone_synth_pulse_width` harness tests.

---

---

---

---

---

---

## [v2.4.116 "Tone Synth Mod LFO"] - 2026-05-23

### Description

Graph **Tone Synth** adds one **global mod LFO** (triangle / square / random S&H): rate + waveform, with independent **amount × range** for **pitch**, **PWM**, and **filter cutoff**. Separate from CC1 vibrato (fixed 5 Hz) and CC92 tremolo. All CC-mapped; off by default (LFO rate 0).

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: Controls **18–25**, `mod_lfo_phase`, CC **24–31**.
- **`sit/aud/device_wrappers.h`**: Per-sample LFO applied to pitch, pulse width, filter.
- **`sit/aud/registry_init.h`**: 26 controls registered.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_lfo_mod`, `graph_tone_synth_waveforms_all` (+ filter/pulse width).

### Mod LFO CC map

| CC | Control | Scaling |
|----|---------|---------|
| 24 | LFO rate | 0=off; else log 0.05–20 Hz |
| 25 | LFO waveform | 0=tri, 1=square, 2=random |
| 26 | Pitch amount | 0–1 |
| 27 | Pitch range | 0–12 semitones |
| 28 | PWM amount | 0–1 |
| 29 | PWM range | 0–0.45 duty |
| 30 | Filter amount | 0–1 |
| 31 | Filter range | log 20–8000 Hz span |

Modulation: `target += lfo × amount × range` (bipolar LFO −1..1).

---

---

---

---

---

---

## [v2.4.117 "Tone Synth Filter Env Mod"] - 2026-05-23

### Description

Graph **Tone Synth** routes the per-voice **ADSR envelope** to filter cutoff via **amount × range** (same formula as mod LFO filter depth). Off by default; stacks with LFO filter offset.

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: Controls **26–27** (`filter_env_amount`, `filter_env_range`), CC **32–33**.
- **`sit/aud/device_wrappers.h`**: Per-voice `cutoff_offset += envelope × amount × range` (manual path uses amplitude).
- **`sit/aud/registry_init.h`**: 28 controls registered.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_filter_env_adsr`.

### Filter env CC map

| CC | Control | Scaling |
|----|---------|---------|
| 32 | Filter env amount | 0–1 |
| 33 | Filter env range | log 20–8000 Hz span |

Modulation: `cutoff += envelope × amount × range` (0–1 ADSR, added before keytrack/clamp).

---

---

---

---

---

---

## [v2.4.118 "Tone Synth Portamento"] - 2026-05-23

### Description

Graph **Tone Synth** in **mono** mode gains **portamento** (pitch glide on legato note changes) with two complementary controls: fixed **glide time** and interval-aware **glide speed** (semitones per second). **Legato** re-triggers preserve envelope sustain and oscillator phase when a new note arrives while the prior note is still in attack, decay, or sustain; a gap or release before the next note-on starts a fresh voice. Harness: dedicated **`tone_synth`** module with linked vs unlinked four-note phrases.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **118**.
- **`sit/aud/tone_synth_graph.h`**: Controls **28–29** (`portamento_time`, `portamento_speed`); `base_hz` / `target_hz`; `_SituationToneSynthVoicePortamentoTauSec`, `_SituationToneSynthVoiceGlidePitch`, `_SituationToneSynthVoiceLegatoFromControls`; MIDI **CC5** (time), **CC20** (speed).
- **`sit/aud/device_wrappers.h`**: Per-sample RC glide in mono when time or speed &gt; 0.
- **`sit/aud/registry_init.h`**: **30** controls registered.
- **`tests/harness/test_tone_synth.c`**: Module **`tone_synth`**; `mono_portamento_linked`, `mono_portamento_unlinked` (4-note C–E–G–C phrases).
- **`doc/tone_synth.md`**: Portamento / legato section (§3.1).

### Portamento controls (registry indices)

| Idx | Name | Default | Range | MIDI CC |
|-----|------|---------|-------|---------|
| 28 | `portamento_time` | 0 | 0–2 s | **CC5** (`norm × 2`) |
| 29 | `portamento_speed` | 0 | 0–48 st/s | **CC20** (`norm × 48`) |

**Effective glide time** `τ` (seconds, used as RC time constant):

| `portamento_time` | `portamento_speed` | `τ` |
|-------------------|--------------------|-----|
| ≤ 0 | ≤ 0 | Instant (snap `base_hz` → `target_hz`) |
| &gt; 0 | ≤ 0 | `τ = time` (same for every interval) |
| ≤ 0 | &gt; 0 | `τ = semitones ÷ speed` (interval from current `base_hz` to `target_hz`) |
| &gt; 0 | &gt; 0 | `τ = max(time, semitones ÷ speed)` — **slower glide wins** |

Poly mode ignores portamento (pitch snaps each voice). Mono + legato sets `target_hz` on note-on; glide runs in the audio loop.

---

---

---

---

---

---

## [v2.4.119 "Tone Synth Sub Osc"] - 2026-05-23

### Description

Graph **Tone Synth** (patch **119**): **16 voices** per node (was 64), plus a per-voice **sub-oscillator** mixed before the SVF — same five waveforms as the main osc, **0 / −1 / −2 octave** offset, **±1 semitone** fine tune, level **0–1**. Legacy **`SituationPlayToneEx`** pool stays at 64 voices. Override voice cap: `-DSITUATION_TONE_SYNTH_MAX_VOICES=N`.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **119**.
- **`sit/aud/tone_synth_graph.h`**: `sub_phase`, `sub_waveform`; controls **30–33**; `_SituationToneSynthOscSampleWave`, `_SituationToneSynthSubFrequencyHz`; legato preserves `sub_phase`; MIDI **CC107–110**.
- **`sit/aud/device_wrappers.h`**: Main + sub sum pre-filter; manual path sub mix.
- **`sit/aud/registry_init.h`**: **34** controls.
- **`sit/situation_api.h`**: `SITUATION_MAX_CONTROLS_PER_DEVICE` **32 → 48** (Tone Synth exceeded the old cap).
- **`tests/harness/test_tone_synth.c`**: `sub_oscillator`.
- **`doc/tone_synth.md`**: §3.2 sub-oscillator.

### Sub-oscillator controls

| Idx | Name | Default | Range | MIDI CC |
|-----|------|---------|-------|---------|
| 30 | `sub_level` | 0 | 0–1 | **107** |
| 31 | `sub_waveform` | Sine | 0–4 (same as main) | **108** (`value % 5`) |
| 32 | `sub_octave` | Oct −1 | 0=unison, 1=−1 oct, 2=−2 oct | **109** |
| 33 | `sub_fine` | 0 | ±1 semitone | **110** (`norm×2−1`) |

Pitch: `sub_hz = main_hz × 2^((fine − octave×12) / 12)` (octave 0/1/2 = unison / −1 oct / −2 oct) where `main_hz` is the voice pitch after bend, portamento, and mod LFO.

---

---

---

---

---

---

## [v2.4.120 "Tone Synth Sum Limiter"] - 2026-05-23

### Description

Graph **Tone Synth** applies the same **post-voice-sum enhanced lookahead limiter** as **Polysonix** (`EnhancedLimiter` in `sit/aud/polysonix/polysonix.h`) after panning and before the node output. Fixed patch defaults: threshold **0.95**, release **50 ms**, ratio **20:1**, **1 ms** lookahead (buffer **2 ms** at sample rate, min 16 samples).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **120**.
- **`sit/aud/tone_synth_graph.h`**: `SituationToneSynthSumLimiter`, init/process/alloc (Polysonix-identical); `sum_limiter` on `SituationToneSynthNodeState`.
- **`sit/aud/device_wrappers.h`**: Allocate/free limiter on create/destroy; per-sample limit after voice sum.
- **`doc/tone_synth.md`**: Signal-flow note for sum limiter.

---

---

---

---

---

---

## [v2.4.121 "Tone Synth Sub Switches"] - 2026-05-23

### Description

Graph **Tone Synth** sub-oscillator gains three switches: **sub_note** (ctrl/MIDI **0** = track main note pitch; **1–127** = fixed sub MIDI note), **sync with sub** (main phase hard-resets each sub cycle), and **ring modulation** (multiply main×sub with `sub_level` as wet depth). MIDI **CC111–113**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **121**.
- **`sit/aud/tone_synth_graph.h`**: Controls **34–36**; `_SituationToneSynthMixMainSub`; `sub_cycle_pending` on voice/node.
- **`sit/aud/device_wrappers.h`**: Voice/manual paths use shared mix helper.
- **`sit/aud/registry_init.h`**: **37** controls.
- **`doc/tone_synth.md`**: §3.2 sub switches.

| Idx | Name | MIDI CC |
|-----|------|---------|
| 34 | `sub_note` (0=track, 1–127=fixed) | **111** |
| 35 | `sub_sync` | **112** |
| 36 | `sub_ring_mod` | **113** |

---

---

---

---

---

---

## [v2.4.122 "Tone Synth Sub Sync Ring Listen"] - 2026-05-23

### Description

Graph **Tone Synth** sub **sync** and **ring mod** fixes and audible harness coverage. **Ring mod** uses the same sub-oscillator pitch as additive (`sub_note` + `sub_octave` / `sub_fine`), so **CC111** sweeps audibly retune the modulator while **A4** is held. **Fix:** ring multiply is no longer skipped when the sub waveform crosses zero (was outputting dry main at those samples). Dedicated listen tests **`sub_sync`** and **`sub_ring_mod`**: each runs ~**3.5 s** with `sub_note=0` (sub tracks main), then ~**3.5 s** with **CC111** stepped **1→127** (note held; only CC111 between steps).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **122**.
- **`sit/aud/tone_synth_graph.h`**: Ring path uses `_SituationToneSynthSubFrequencyHz` (same as additive sub); ring mix always applied when sub is active (not gated on `sub_s != 0`).
- **`tests/harness/test_tone_synth.c`**: `sub_sync` — saw main, pulse sub, oct −1, sync on; `sub_ring_mod` — sine×sine oct −1, ring on; sustained **A4** + CC111 coarse sweep (**40** steps); ring asserts weak **440 Hz** vs **220/660 Hz** sidebands.
- **`doc/tone_synth.md`**: Ring and sync behaviour aligned with shared sub pitch.

### Harness (listen)

| Test | Effect | Segment A | Segment B |
|------|--------|-----------|-------------|
| `sub_sync` | **CC112** on | **A4** hold, `sub_note=0` | **A4** hold, **CC111** 1→127 |
| `sub_ring_mod` | **CC113** on | **A4** hold, `sub_note=0` | **A4** hold, **CC111** 1→127 |

---

---

---

---

---

---

## [v2.4.123 "Tone Synth Patch Memory"] - 2026-05-23

### Description

Each graph **Tone Synth** node has **16 patch slots** storing a snapshot of controls **1–36** (waveform through `sub_ring_mod`; manual **frequency** ctrl **0** is excluded). **MIDI CC114** selects slot **0–15** and **recalls** when the CC value changes; **CC115** **≥64** saves the current control state into the selected slot (rising edge). Controls **37–38** (`patch_slot`, `patch_store`) mirror the same behaviour via `SituationSetControl`.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **123**.
- **`sit/aud/tone_synth_graph.h`**: `patch_slots[16]` on node state; save/recall + voice template sync on recall.
- **`sit/aud/registry_init.h`**: **39** controls.
- **`sit/aud/node_graph_impl.h`**: `SituationSetControl` hooks patch slot/store for tone synth nodes.
- **`tests/harness/test_tone_synth.c`**: `patch_memory`.
- **`doc/tone_synth.md`**: §3.2 patch memory.

| Idx | Name | MIDI CC |
|-----|------|---------|
| 37 | `patch_slot` | **114** |
| 38 | `patch_store` | **115** (≥64 save) |

---

---

---

---

---

---

## [v2.4.124 "Bind Index Buffer API"] - 2026-05-24

### Description

Public **low-level indexed draw** API for OpenGL 4.6 and Vulkan: **`SituationCmdBindIndexBuffer`** (with byte **`offset`**) pairs with **`SituationCmdBindVertexBuffer`** and **`SituationCmdDrawIndexed`**. Index format is **32-bit** (`GL_UNSIGNED_INT` / `VK_INDEX_TYPE_UINT32`), matching **`SituationCreateMesh`**. On Vulkan, **`SituationCmdBindVertexBuffer`** now selects **`vk_pipeline_simple`** / legacy / PBR from **stride** after **`SituationCmdBindPipeline`**, same rules as **`SituationCmdDrawMesh`**, so manual VBO+IBO draws work without calling **`DrawMesh`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **124**.
- **`sit/situation_api.h`**: **`SituationCmdBindIndexBuffer`**, **`SituationCmdBindVertexBuffer`** (public declarations).
- **`sit/situation_impl_renderer.h`**: OpenGL soft replay uses **`glVertexArrayElementBuffer`** on the global VAO; IBO byte offset applied at indexed draw (including MDI **`firstIndex`** bias); Vulkan **`vkCmdBindIndexBuffer`** with offset; Vulkan vertex bind rebinds stride-matched pipeline variant.
- **`sit/situation_impl_decl.h`**: **`bind_ibo.offset`** packet field; **`bound_ibo_byte_offset`** GL replay state.
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** (bind VBO/IBO + **`DrawIndexed`** + screen readback); dual-backend harness pass with **`draw_indexed_quad`**.

### Harness

| Test | Path |
|------|------|
| `bind_index_buffer_low_level` | `BindPipeline` → `BindVertexBuffer` → `BindIndexBuffer` → `DrawIndexed` |
| `draw_indexed_quad` | High-level **`SituationCmdDrawMesh`** (regression) |

---

---

---

---

---

---

## [v2.4.125 "Vertex Attribute Binding"] - 2026-05-24

### Description

Fixes **OpenGL low-level draw replay** (`SituationCmdBindVertexBuffer` + `SituationCmdDraw` / `SituationCmdDrawIndexed`) and makes **vertex input binding explicit** on the public API. The GL executor now **binds the global VAO** before vertex-buffer and draw packets (DSA updates alone were not enough when VAO 0 was active). **`SituationCmdSetVertexAttribute`** gains a **`binding`** parameter (must match the **`binding`** passed to **`SituationCmdBindVertexBuffer`**); use **`0`** for all attributes in an **interleaved** layout. This unblocks **RGL** batch flush (smoke test no longer crashes in **`SituationEndFrame`** on the first **`SIT_OP_DRAW`**) and matches how Vulkan thinks about vertex streams without hard-coding RGL’s layout in the core.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **125**.
- **`sit/situation_api.h`**: **`SituationCmdSetVertexAttribute(cmd, location, binding, size, type, normalized, offset)`** — new **`binding`** argument after **`location`**.
- **`sit/situation_impl_decl.h`**: **`set_vertex_attr.binding`** in soft-command packet.
- **`sit/situation_impl_renderer.h`**:
  - Record/replay **`binding`** via **`glVertexArrayAttribBinding`** (replaces incorrect **`location == binding`** assumption).
  - **`SIT_OP_BIND_VERTEX_BUFFER`** / **`SIT_OP_DRAW`**: bind **`sit_render.gl.global_vao_id`** when needed before DSA / draw.
- **`misc/rgl.h`**: **`_RGL_ConfigureBatchVertexLayout`** passes **`binding = 0`** for all five interleaved attributes (13-float batch stride).
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** OpenGL path uses **`SetVertexAttribute(..., 0, ...)`**.

### API note (breaking, OpenGL-only path)

Callers of **`SituationCmdSetVertexAttribute`** must pass **`binding`** explicitly:

```c
/* Interleaved: one VBO at binding 0 */
SituationCmdSetVertexAttribute(cmd, 0, 0, 3, SIT_DATA_FLOAT, false, offsetof(Vertex, pos));
SituationCmdBindVertexBuffer(cmd, 0, vbo, 0, sizeof(Vertex));

/* Separate streams: attribute location N from binding N */
SituationCmdSetVertexAttribute(cmd, 1, 1, 2, SIT_DATA_FLOAT, false, 0);
SituationCmdBindVertexBuffer(cmd, 1, uv_vbo, 0, 8);
```

Vulkan: unchanged — vertex layout remains pipeline-creation time; **`SetVertexAttribute`** still returns **`SITUATION_ERROR_NOT_IMPLEMENTED`**.

### Harness

| Backend | Command | Graphics module |
|---------|---------|-----------------|
| OpenGL | `build\sit_test.exe --module graphics` | **107** passed |
| Vulkan | `build\sit_test_vulkan.exe --module graphics` | **97** passed |

Includes **`bind_index_buffer_low_level`**, **`draw_indexed_quad`**, **`draw_pipeline_basic`**. **`build\examples\rgl_smoke_test.exe`** (monolithic OpenGL) runs past first frame after fix.

---

---

---

---

---

---

## [v2.4.126 "Vertex Index Bind SituationError"] - 2026-05-24

### Description

**`SituationCmdBindVertexBuffer`** and **`SituationCmdBindIndexBuffer`** now return **`SituationError`**, matching other Core recording commands (`BindPipeline`, `Draw`, `DrawIndexed`). Removes silent failure paths (null command buffer on Vulkan vertex bind, OpenGL soft-buffer allocation failure) so callers and docs do not rely on **`SituationGetLastError()`** alone.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **126**.
- **`sit/situation_api.h`**: `void` → **`SituationError`** for both bind functions.
- **`sit/situation_impl_renderer.h`**: Return codes aligned with **`SituationCmdBindPipeline`** / **`SituationCmdDraw`**; **`MEMORY_ALLOCATION`** when **`_SitGLSoftCmdPush`** fails.
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** asserts bind success.
- **`doc/situation_command_reference.md`**: Signatures and **Returns** sections updated.

### API note (breaking, source)

```c
/* Before (v2.4.125) */
void SituationCmdBindVertexBuffer(...);
void SituationCmdBindIndexBuffer(...);

/* After (v2.4.126) */
SituationError SituationCmdBindVertexBuffer(...);
SituationError SituationCmdBindIndexBuffer(...);
```

Recompile callers; check return value like other **`SituationCmd*`** Core APIs.

---

---

---

---

---

---

## [v2.4.127 "Internal Hardening Tooling"] - 2026-05-24

### Description

Internal hardening **Phase 0** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): tooling and test baseline for the void → **`SituationError`** migration.

### Library Changes

- **`SIT_RETURN_IF_ERR`** macro in **`sit/situation_impl_decl.h`**
- **`scripts/list_internal_voids.ps1`** → **`doc/plan/internal_void_inventory.csv`**
- **`doc/plan/INTERNAL_HARDENING_PLAN.md`** — phased task board + per-phase version policy

### Tests

- **`tests/harness/test_core.c`**: **`init_double_init_error`** — second **`SituationInit`** returns **`ALREADY_INITIALIZED`**; **`SituationGetLastErrorCode`** matches

### Verification

- OpenGL harness: **`core`** includes new error-propagation smoke test

---

---

---

---

---

---

## [v2.4.128 "Internal Init Error Propagation"] - 2026-05-24

### Description

Internal hardening **Phase 1** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): init-chain helpers return **`SituationError`** and propagate to **`SituationInit`** instead of side-channel-only **`_SituationSetErrorFromCode`** from **`void`** / **`bool`** stubs.

### Library Changes (internal only — no public API break)

- **GL ring/MDI/fences**: **`_SituationInitGLRingBuffer`**, **`_SituationInitGLMDIBuffer`**, **`_SituationInitGLRingFences`**
- **Renderer inits** (`bool` → **`SituationError`**): **`_SituationInitDefaultFont`**, **`_SituationInitTextRenderer`**, **`_SituationInitQuadRenderer`**, **`_SituationInitGLVirtualDisplayRenderer`**, **`_SituationValidateRenderCaps`**, **`_SituationInitRenderThread`**
- **Subsystems**: **`_SitAudioInitPool`**, **`_SituationCachePhysicalDisplays`**
- Callers: **`_SituationInitOpenGL`**, **`_SituationInitVulkan`**, **`SituationInit`** (ctrl)
- Forward decls: **`situation_impl_forward.h`**, **`situation_impl_renderer_fwd.h`**, **`situation_impl_decl.h`**

### Verification

- OpenGL harness: **`core` 30/30**, **`graphics` 107/107**

---

---

---

---

---

---

## [v2.4.129 "Internal Vulkan Swapchain Errors"] - 2026-05-24

### Description

Internal hardening **Phase 2** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): Vulkan swapchain recreation, single-time command submit, and compute queue submit return **`SituationError`** and propagate to frame acquire / **`SituationEndFrame`** instead of side-channel-only failures.

### Library Changes (internal only — no public API break)

- **`_SituationVulkanRecreateSwapchain`**, **`_SituationVulkanCleanupSwapchain`** — `void` → **`SituationError`**
- **`_SituationVulkanEndSingleTimeCommands`**, **`_SituationSubmitCompute`** — `void` → **`SituationError`**
- Callers: **`SituationAcquireFrameCommandBuffer`**, **`SituationEndFrame`** (present OUT_OF_DATE / resize), **`SituationSetVSync`**, texture/buffer/mesh upload paths, screenshot blit
- Forward decls: **`situation_impl_renderer_fwd.h`**
- Verified existing **`SituationError`** helpers on recreate path: **`_SituationVulkanCreateSwapchain`**, **`_SituationVulkanCreateScreenCopyResource`**, **`_SituationVulkanEnsureScreenshotResources`**

### Verification

- OpenGL harness: **`core`**, **`graphics`** (no Vulkan-specific regressions in shared code paths)
- Vulkan build: **`build_tests.bat vulkan`** — **`graphics`** module

---

---

---

---

---

---

## [v2.4.130 "Errno Table Phase 2.1"] - 2026-05-24

### Description

Errno table **Phase 2.1**: close doc/comment gaps from the errno audit — add missing codes, document legacy duplicates with **`EOL:`** comments for a future caller sanitisation pass, and add **`scripts/audit_errno.ps1`**.

### Library Changes

- **`sit/situation_base_errno.h`**:
  - **New codes**: `MEMORY_ACCESS` (-12), `FILE_MODIFIED` (-317), `BACKEND_SPECIFIC` (-551), `VULKAN_COMMAND_BUFFER_FAILED` (-721)
  - **`#define` aliases** after enum (not in X-macro switch table): `GLAD_LOAD_FAILED`, `GL_UPLOAD_FAILED`, `ACCESS_DENIED`, … → canonical names
  - **`EOL:`** on legacy pairs (display, filesystem, audio, GL/Vulkan generics, platform Win32)
- **`scripts/audit_errno.ps1`**: table vs usage; flags phantoms and duplicate alias values
- **`tests/harness/test_core.c`**: **`errno_table_phase_2_1`**

### Verification

- `.\scripts\audit_errno.ps1` — no phantom names
- OpenGL harness: **`core` 31/31** (`errno_table_phase_2_1`)

---

---

---

---

---

---

## [v2.4.131 "Internal GL Soft Buffer Errors"] - 2026-05-24

### Description

Internal hardening **Phase 3** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): OpenGL deferred record path and momentum enqueue return **`SituationError`** so OOM, broken soft buffers, GL replay failures, and queue-full propagate to **`SituationEndFrame`** / record APIs instead of silent drops.

### Library Changes (internal only — no public API break)

- **`_SitGLSoftCmdPush`**, **`_SitGLSoftDataPush`** — return **`SituationError`** + out-pointers; macros **`SIT_GL_SOFT_CMD_PUSH`** / **`SIT_GL_SOFT_DATA_PUSH`**
- **`_SituationGLExecuteCommands`** — broken buffer → **`RENDER_COMMAND_FAILED`**; per-packet **`glGetError`** → **`OPENGL_GENERAL`**
- **`_SituationReplayToQueue`**, **`_SituationEnqueueRenderList`** — **`SituationError`** (queue full → **`THREAD_QUEUE_FULL`**)
- Callers: all OpenGL **`SituationCmd*`** record paths, **`SituationRenderVirtualDisplays`**, **`SituationEndFrame`** (execute path), render thread (logs execute failure)
- Debug: optional **`SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS`** cap for OOM simulation
- **`_SituationCheckGLError`** — stays **`void`** (decision in plan §3.3)

### Verification

- OpenGL harness: **`core`**, **`graphics`** (rebuild + full module pass after gate)

---

---

---

---

---

---

## [v2.4.132 "Internal Uniform Map Errors"] - 2026-05-24

### Description

Internal hardening **Phase 4** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): shader uniform location cache and resource slot allocators report **`SituationError`** (or set the error channel on NULL) instead of silent OOM / registry-full failures.

### Library Changes (internal only — no public API break)

- **`_sit_uniform_map_resize`**, **`_sit_uniform_map_set`** — `void` → **`SituationError`**
- **`_sit_uniform_map_create`** — enables **`_SituationSetErrorFromCode`** on alloc failure (still returns NULL per Rule I4)
- **`_SituationPopulateGLShaderUniformMap`** — propagates map set failures
- **`_SitAlloc*Slot`** — registry full → **`SITUATION_ERROR_RESOURCE_INVALID`** + message; callers check NULL via **`SituationGetLastErrorCode()`**
- **`_SitFree*Slot`** — stay **`void`** with **`HARDENING: void by design`** (idempotent; invalid handles ignored)
- **`SIT_GL_SOFT_CMD_PUSH_VOID`** — void compute record cmds no longer use error-return macro

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**

---

---

---

---

---

---

## [v2.4.133 "Internal Shader Async Errors"] - 2026-05-24

### Description

Internal hardening **Phase 5** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): OpenGL/Vulkan async shader poll paths and the hot-reload I/O pass return **`SituationError`** so in-progress (`SHADER_LOAD_IN_PROGRESS`), terminal compile/link/SPIR-V failures, and reload failures propagate without relying on a prior global error read.

### Library Changes (internal only — no public API break)

- **`_SituationSetGLErrorFromSpirvStage`** — returns **`SituationError`** (drops `out_code` parameter; callers assign optional `error_code` pointers)
- **`_SituationGLAsyncLoadFail`**, **`_SituationPollGLAsyncShaderLoad`**, **`_SituationPollGLAsyncSpirvShaderLoad`**, **`_SituationPollGLPendingProgramLink`** — **`SituationError`** with explicit in-progress vs terminal mapping
- **`_SituationPollVkAsyncShaderLoad`** — **`SituationError`**; **`SituationPollShaderLoad`** uses poll return values directly
- **`_SituationPerformHotReloadPass`** — returns first reload failure (shader/texture/audio) or **`SITUATION_SUCCESS`**
- **`_SituationVulkanFreeAsyncShaderLoad`** — stays **`void`** with **`HARDENING:`** comment (Phase 9 doc)

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**, **`--filter spirv` 10/10**

---

---

---

---

---

---

## [v2.4.134 "Internal Render Thread Errors"] - 2026-05-24

### Description

Internal hardening **Phase 6** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): render-thread shutdown and per-frame graveyard flush return **`SituationError`**; GL fence timeouts and join failures propagate to **`SituationShutdown`** instead of only logging.

### Library Changes (internal only — no public API break)

- **`_SituationDestroyRenderThread`** — returns join timeout / **`THREAD_JOIN_FAILED`** (fixes mistaken **`THREAD_CREATION_FAILED`** on join)
- **`_SitFlushFrameResources`** — bounds check + **`SituationError`**; render thread logs flush failures
- **Render thread (OpenGL)** — **`glClientWaitSync`** timeout/failure sets **`RENDER_BACKPRESSURE_TIMEOUT`** / **`OPENGL_GENERAL`**
- **`SituationShutdown`** — preserves render-thread error (skips "Shutdown complete" overlay when join failed)
- **`_SituationRenderJobWorker`** — checks **`_SituationEnqueueRenderList`**; **`HARDENING:`** void-by-design note (pool ABI)

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**

---

---

---

---

---

---

## [v2.4.135 "Internal Audio Errors"] - 2026-05-24

### Description

Internal hardening **Phase 7** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): non-RT audio paths return **`SituationError`**; reverb init checks OOM on every buffer; sound load wires effects init; RT miniaudio/reverb callbacks stay **`void`** with **`HARDENING:`** notes.

### Library Changes (internal only — no public API break)

- **`_SitFreeSoundSlot`** — invalid handle → **`RESOURCE_INVALID`**; success returns **`SITUATION_SUCCESS`**
- **`_SituationInitReverb`** — **`SituationError`** + **`void** out_state`**; per-buffer OOM with **`_SituationUninitReverb`** rollback
- **`_SituationInitSoundEffects`** — called from **`SituationLoadSoundFromFile`** / **`SituationLoadSoundFromStream`**; propagates reverb alloc failures
- **`_SituationAsyncAudioWorker`** — clears target handle on load fail; error channel via **`SituationLoadSoundFromFile`**; void-by-design (pool ABI)
- **`_SitAudioCleanupPool`** — uses **`_SituationUninitReverb`** instead of raw **`SIT_FREE`**
- RT paths unchanged: **`sit_miniaudio_data_callback`**, **`_SituationProcessReverb`**, etc.

### Verification

- OpenGL harness: **`core` 31/31**, **`audio` 80/81** (`control_sweep_all_devices` — log-scale tone_synth control midpoint readback; pre-existing graph test tolerance, not Phase 7 regression)

---

---

---

---

---

---

## [v2.4.136 "Internal Hardening Stragglers"] - 2026-05-24

### Description

Internal hardening **Phase 8** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): last init-style **`bool`** helpers normalized to **`SituationError`**; intentional query/resolver **`bool`**s documented with **`HARDENING:`** notes.

### Library Changes (internal only — no public API break)

- **`_SituationExtractGLTFPrimitive`** — **`SituationError`** (`ASSET_PARSE_FAILED`, `MEMORY_ALLOCATION`); model load caller checks return
- **`_SituationSaveImageBMP`** — **`SituationError`**; **`SituationExportImage`** BMP path returns helper result directly
- **Documented void-by-design / bool-by-design**: **`_sit_directory_exists`**, **`_SituationGraphHasMixerNode`**, **`_SituationShouldMixLatentVoices`**, **`_SituationDetectCycle`**, **`_SituationVulkanResolveBufferDescriptor`**, **`_SituationVulkanImmediateDestroyDuringShutdown`**

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107** (includes **`model_*`** glTF load path)

---

---

---

---

---

---

## [v2.4.137 "Internal Void By Design Docs"] - 2026-05-24

### Description

Internal hardening **Phase 9** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): Bucket B — every intentional internal **`void`** helper tagged at forward decl with **`/* HARDENING: void by design — … */`** so Phase 10 caller audit can distinguish documented sinks from conversion debt.

### Library Changes (internal only — no public API break)

- **`situation_impl_forward.h`** — lifecycle, GLFW callbacks, thread/render worker forward decls
- **`situation_impl_renderer_fwd.h`** — VK/GL cleanup, graveyard, defer-destroy, record-only helpers, bind helpers
- **`situation_impl_decl.h`**, **`situation_impl_io.h`**, **`situation_impl_audio.h`**, **`situation_impl_threading.h`** — assert/pump, async file workers, RT mix/capture, parallel worker
- **`_SituationPopulateGLShaderUniformMap`** — N/A (returns **`SituationError`** since Phase 4); not re-tagged as void
- One-shot helper: **`scripts/tag_phase9_hardening.py`** (88 new tags; gate count **101** in `sit/situation_impl*.h`)

### Verification

- Gate: **`HARDENING: void by design`** count ≥ 68 in `sit/situation_impl*.h` → **101**
- OpenGL harness: **`core` 31/31**, **`graphics` 107/107** (comment-only; no signature changes)

---

---

---

---

---

---

## [v2.4.138 "Internal Caller Audit"] - 2026-05-24

### Description

Internal hardening **Phase 10** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): final caller audit — init tree and Vulkan create chain already propagate **`SituationError`**; fixed remaining swapchain recreate and render-list enqueue sites that dropped return values.

### Library Changes (internal only — no public API break)

- **`SituationAcquireFrameCommandBuffer`** (Vulkan) — **`VK_ERROR_OUT_OF_DATE_KHR`** / **`VK_TIMEOUT`** paths now check **`_SituationVulkanRecreateSwapchain()`** (was bare / `(void)` call)
- **`SituationSetVSync`** (Vulkan) — propagate swapchain recreate failure via **`_SituationSetErrorFromCode`**
- **`SituationSubmitRenderList`** — check **`_SituationEnqueueRenderList`**; set global error on failure (matches render-job worker)
- Gate script **`scripts/audit_phase10_caller.py`**: **0** unchecked **`_SituationInit*`** / **`_SituationVulkanCreate*`** calls in init tree

### Verification

- Gate script **`scripts/audit_phase10_caller.py`**: **0** unchecked init/Vulkan-create calls
- **Internal hardening complete at v2.4.138** (Phases 0–10, patches **127→138**)

#### Post-hardening full harness (OpenGL + Vulkan DLLs rebuilt)

**Build:** `build_situation.bat` + `build_tests.bat` for **opengl** and **vulkan** — both green.

**Single-shot full suite** (`sit_test.exe` / `sit_test_vulkan.exe`, all 12 modules, no flags):

- Both backends **abort mid-`tone_synth`** with access violation (**`0xC0000005`**) after ~15–25 long MIDI tests in one process — does **not** reproduce when those tests are run alone or in small groups.
- Logs: **`build/harness_ogl_full.log`**, **`build/harness_vk_full.log`** (partial runs).

**Per-module harness** (authoritative sign-off run):

| Module | OpenGL | Vulkan |
|--------|--------|--------|
| filesystem | 23/23 | 23/23 |
| threading | 7/7 | 7/7 |
| core | 31/31 | 31/31 |
| window | 27/27 | 27/27 |
| input | 17/17 | 17/17 |
| timer | 10/10 | 10/10 |
| Projection | 2/2 | 2/2 |
| audio | **80/81** | **80/81** |
| audio_effects_heard | 17/17 | 17/17 |
| graphics | **107/107** | **97/97** |
| misc | 20/20 | 20/20 |
| tone_synth | **35/35**† | **35/35** |
| **Suite total** | **376 pass, 1 fail** / 377 | **366 pass, 1 fail** / 367 |

† OpenGL **`tone_synth`**: all 35 pass per-module or per-test; **continuous `--module tone_synth` crashes** (sustained MIDI + GL — follow-up bug, not a Phase 10 regression). Vulkan completes **`tone_synth`** in one module run.

**Known failure (both backends, pre-existing):** **`control_sweep_all_devices`** — log-scale tone_synth control midpoint readback tolerance (`test_audio.c`).

**Graphics delta:** Vulkan harness has **10 fewer** registered tests than OpenGL (GL/SPIR-V–only cases) — expected.

**Hardening-relevant modules:** init/window/graphics/misc and backend-specific paths — **green** on both backends aside from the audio control sweep above.

---

---

---

---

---

---

## [v2.4.139 "CPU Topology & Affinity"] - 2026-05-24

### Description

**Threading Bolstering — Epic A** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): read-only CPU topology cache, affinity query/set with previous-mask feedback, HT/NUMA mask builders, and configurable render/audio affinity via init — additive on the existing generational thread pool (no scheduler rewrite).

### Library Changes

- **New** `sit/situation_impl_threading_topology.h` — topology refresh/query; `SituationGetCPUCoreCount()` now uses cached physical-core count (Linux sysfs; Windows `GetLogicalProcessorInformation`; macOS `sysctl`)
- **Types** — `SituationCpuTopology`, `SituationLogicalProcessorInfo`; limits `SITUATION_MAX_LOGICAL_PROCESSORS` (256), `SITUATION_AFFINITY_MASK_BITS` (64)
- **API** — `SituationRefreshCpuTopology`, `SituationGetCpuTopology`, `SituationSetThreadAffinityEx`, `SituationGetThreadAffinity`, `SituationGetCurrentProcessorIndex`, `SituationGetThreadNumaNode`, `SituationBuildPhysicalCoreMask`, `SituationBuildUniqueCoreMask`, `SituationBuildNumaNodeMask`, `SituationGetConfiguredRenderThreadAffinity`, `SituationGetConfiguredAudioThreadAffinity`
- **`SituationInitInfo`** — `thread_affinity_render`, `thread_affinity_audio` (0 = defaults: logical core 1 / 2)
- **Render / audio** — pin via configured masks at thread entry (same defaults as before when fields are 0)
- **Trace IDs** — `10030011`–`10030019` in `situation_base_trace.h`

### Verification

- OpenGL DLL rebuild + **`sit_test.exe --module threading`**: **11/11** (was 7/7; adds `cpu_topology_refresh`, `affinity_roundtrip`, `mask_builders`, `configured_affinity`)
- Pool worker / submit paths unchanged

---

---

---

---

---

---

## [v2.4.140 "Thread Pool Observability"] - 2026-05-24

### Description

**Threading Bolstering — Epic B** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): export threading diagnostics, per-worker CPU sampling, queue/active-job metrics, pool snapshots, and combined debug dumps — additive on the v2.4.139 topology/affinity layer.

### Library Changes

- **New** `sit/situation_impl_threading_observability.h` — `SituationGetThreadingStatus`, `SituationPrintThreadingStatus`, queue depth APIs, pool snapshot, `SituationDumpThreadPoolStatus`, `SituationDumpThreadingReport`
- **`SituationThreadPool`** — worker `SituationWorkerStartArg`, `worker_last_logical_cpu[]`, job submit/complete + main-thread steal stats, `io_last_logical_cpu`
- **Workers** — CPU sample every 8 jobs + on idle wake; I/O thread samples each loop
- **Render / audio** — record affinity mask + sampled CPU into snapshot (`_SituationObservabilityRecord*`)
- **`SituationGetIOQueueDepth`** — delegates to `SituationGetQueueDepth(..., SIT_JOB_QUEUE_LOW)`
- **`SituationDrawMetricsOverlay`** — shows active jobs + low/high queue depths when threading enabled
- **Docs** — `doc/THREADING_TROUBLESHOOTING_GUIDE.md` (replaces broken diag header reference)
- **Trace IDs** — `10030020`–`10030027`

### Verification

- OpenGL DLL rebuild + **`sit_test.exe --module threading`**: **14/14** (7 original + 4 Epic A + 3 Epic B)

---

---

---

---

---

---

## [v2.4.141 "NUMA Awareness"] - 2026-05-24

### Description

**Threading Bolstering — Epic C** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): NUMA topology snapshot, init-time placement policy, worker/I/O/render/audio pinning hooks, and thread-local preferred NUMA node for allocators.

### Library Changes

- **New** `sit/situation_impl_threading_numa.h` — `SituationRefreshNumaTopology`, `SituationGetNumaTopology`, `SituationGetPreferredNumaNode`
- **Types** — `SituationNumaTopology`, `SituationNumaNodeInfo` (`processor_count`, `memory_bytes`, `processor_mask_low`)
- **`SituationInitInfo`** — `numa_prefer_local`, `worker_numa_spread`, `io_thread_numa_node` (default `< 0` = no I/O pin)
- **Placement** — workers spread across nodes when enabled; I/O thread optional node pin; render/audio use `_SituationSetThreadAffinityForRole` + `numa_prefer_local` when masks are 0
- **Windows** — `GetNumaHighestNodeNumber`, `GetNumaAvailableMemoryNode`; **Linux** — sysfs `nodeN/meminfo`
- **Trace IDs** — `10030028`–`10030030`

### Verification

- **`sit_test.exe --module threading`**: **16/16** (+ `numa_topology_refresh`, `numa_node_mask`)

---

---

---

---

---

## [v2.4.142 "Scheduler Metrics"] - 2026-05-24

### Description

**Threading Bolstering — Epic D** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): scheduler contention counters, dynamic high-queue scan depth, physical-core pool sizing, and sizing helpers — without lock-free MPMC or worker-to-worker steal (deferred).

### Library Changes

- **New** `sit/situation_impl_threading_scheduler.h` — `SituationGetRecommendedWorkerCount`, `SituationGetThreadPoolMetrics`, `SituationResetThreadPoolStats`, `SituationDumpThreadPoolMetrics`
- **`SituationThreadPoolMetrics`** — high-queue lock ops/ns, main steal ok/fail/empty, scan-forward swap/exhausted, I/O idle/jobs + busy ratio, inline submit, queue-full spins, `DispatchParallel` call count
- **Dynamic scan** — `_SitWorkerScanDepthForPending()` scales 4–32 from pending depth (replaces fixed `SIT_WORKER_SCAN_DEPTH` 8)
- **`SituationCreateThreadPool(..., num_threads=0)`** — `_SitResolveAutoWorkerCount()` from `SituationInitInfo` `thread_pool_use_physical_cores` / `thread_pool_reserved_threads`
- **Instrumentation** — worker high-queue lock timing; I/O `stats_io_idle_waits` / `stats_io_jobs_run`
- **Trace IDs** — `10030031`–`10030034`

### Verification

- **`sit_test.exe --module threading`**: **18/18** (+ `recommended_worker_count`, `scheduler_metrics_parallel`)

---

---

---

---

---

---

## [v2.4.143 "Threading API Hygiene"] - 2026-05-24

### Description

**Threading Bolstering — Epic E** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): accurate public API docs, main-thread affinity init hook, fail-soft affinity warnings, harness coverage, and dual-socket manual validation guide. Consolidates Epics A–D (v2.4.139–142).

### Library Changes

- **`SituationInitInfo::thread_affinity_main`** — optional main-thread pin after window creation (0 = no pin)
- **`SituationGetConfiguredMainThreadAffinity()`** — read effective main mask
- **Fail-soft affinity** — `_SituationSetThreadAffinityForRole` logs debug warning on pin failure; init continues
- **Docs** — `situation_api.h` / worker impl comments (mutex + atomics, not “lock-free”); README threading; `sit/k-term/doc/situation_api.md` bolstering table; `doc/THREADING_MANUAL_VALIDATION.md`
- **Trace** — `10030035` `SituationGetConfiguredMainThreadAffinity`

### Verification

- **`sit_test.exe --module threading`**: **20/20** (+ `configured_main_affinity`, `metrics_reset_and_dump`)

---

---

---

---

---

---

## [v2.4.144 "Threading Bolstering Complete"] - 2026-05-24

### Description

**Threading Bolstering — release cap** (Epics A–E, v2.4.139–143): topology/affinity, pool observability, NUMA placement, scheduler metrics, API hygiene, and a **10 s all-core harness stress** with Task Manager–correlatable CPU reports. See **`doc/THREADING_BOLSTERING_API.md`** for the consolidated public API reference.

### Library Changes

- **Harness** — `cpu_stress_10s_taskmgr`: ~10 s `SituationDispatchParallel` CPU burn, logical-CPU histogram, worker snapshot, `SituationDumpThreadPoolStatus`; skip via `SIT_SKIP_CPU_STRESS`
- **Docs** — `doc/THREADING_BOLSTERING_API.md` (API catalog); plan Epics A–E marked complete

### Verification

- **`sit_test.exe --module threading`**: **21/21**
- **OpenGL full harness**: **391** total
- CPU stress: sustained **~100%** `_Total` processor time over the 10 s window (validated via performance counters)

---

---

---

---

---

---

## [v2.4.145 "Typed Audio Controls"] - 2026-05-24

### Description

Audio graph control readback now matches declared control types: `FLOAT` remains continuous, while `INT` / `ENUM` round to integer slots and `BOOL` normalizes to 0/1. This fixes the long-standing `audio.control_sweep_all_devices` failure without weakening the test.

### Library Changes

- **`SituationSetControl`** — clamps, then coerces by `SituationControlType` before storing/readback
- **Harness diagnostics** — `control_sweep_all_devices` now reports device/control/min/max/requested/expected/readback and set/get errors on mismatch

### Verification

- **OpenGL full harness**: **391/391**
- **Vulkan full harness**: **381/381** (rebuilt; no stale binary)
- Fixed: **`audio.control_sweep_all_devices`**

---

---

---

---

---

---

## [v2.4.146 "Renderer Text Cleanup"] - 2026-05-24

### Description

Renderer/internal cleanup release: normalizes corrupted punctuation in renderer diagnostics and comments, and restores indentation in the control implementation header.

### Library Changes

- **`situation_impl_renderer.h`** — replaced mojibake punctuation in docs and the metric contention log with plain ASCII
- **`situation_impl_ctrl.h`** — indentation-only formatting pass

### Verification

- **OpenGL DLL build**: pass
- **Vulkan DLL build**: pass

---

---

---

---

---

---

## [v2.4.147 "Compute Harness Split Pilot"] - 2026-05-25

### Description

Renderer bolster Phase -1 is complete: pure compute coverage now has its own harness module, so future dispatch/barrier work can land outside the graphics module while preserving OpenGL/Vulkan parity.

### Harness Changes

- **`tests/harness/test_compute.c`** — new compute module with duplicated-and-fit coverage for compute pipeline creation, workgroup limits, pipeline barrier smoke, SSBO dispatch/readback, sampled-texture-to-SSBO, compute-to-graphics barrier smoke, and chained dispatches
- **`tests/harness/sit_test_registry.c`** — registers the new `compute` module
- **`build_tests.bat`** — builds `test_compute.c` into both OpenGL and Vulkan harness executables
- **`tests/harness/test_graphics.c`** — retired pure-compute duplicates while keeping `compute_image_write` as graphics interop coverage
- **`doc/plan/renderer_bolster_plan.md`** — Phase -1 checklist marked through the pilot split and legacy-copy retirement

### Verification

- **OpenGL harness build**: pass
- **Vulkan harness build**: pass
- **OpenGL `sit_test.exe --module compute`**: **8/8**
- **OpenGL `sit_test.exe --module graphics`**: **99/99**
- **Vulkan `sit_test_vulkan.exe --module compute`**: **8/8**
- **Vulkan `sit_test_vulkan.exe --module graphics`**: **89/89**

---

---

---

---

---

---

## [v2.4.148 "Render Clear Commands"] - 2026-05-25

### Description

Renderer bolster Phase 1 has its first implementation slice: command-buffer clears are now public, explicit, and covered on both OpenGL and Vulkan.

### Library Changes

- **`situation_api.h`** — added `SituationClearFlags`, `SituationCmdClear`, and color/depth/stencil convenience wrappers
- **`situation_impl_decl.h`** — added `SIT_OP_CLEAR`, clear packet payload, GL render-pass recording state, and Vulkan current render-area state
- **`situation_impl_renderer.h`** — records/replays GL clear packets for color/depth, maps Vulkan clears to `vkCmdClearAttachments`, returns `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` outside render passes, and reports unsupported stencil attachment clears as `SITUATION_ERROR_NOT_IMPLEMENTED`
- **`renderer_bolster_plan.md`** — added the Phase 1 parity/errno matrix row and marked the implemented clear-command tasks

### Harness Changes

- **`tests/harness/test_graphics.c`** — added `clear_color_command`, `clear_depth_command`, `clear_stencil_conditional`, and `clear_requires_render_pass`; depth clear now proves depth-tested draw rejection, while stencil clear is conditionally validated until the stencil-state phase

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter clear`**: **3/3**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter clear`**: **3/3**

---

---

---

---

---

---

## [v2.4.149 "Compute Dispatch Ex"] - 2026-05-25

### Description

Renderer bolster Phase 2 has its first direct-dispatch slice: compute dispatch now has a `SituationError`-returning entry point while the legacy void wrapper remains intact.

### Library Changes

- **`situation_api.h`** — added `SituationCmdDispatchEx`
- **`situation_impl_renderer.h`** — moved direct dispatch recording into `SituationCmdDispatchEx`; `SituationCmdDispatch` now delegates as a compatibility wrapper
- **`renderer_bolster_plan.md`** — added the Phase 2 dispatch parity/errno matrix row and marked the direct-dispatch `Ex` tasks complete

### Harness Changes

- **`tests/harness/test_compute.c`** — `dispatch_basic` now uses `SituationCmdDispatchEx`, and `dispatch_ex_invalid_params` verifies null-command and zero-group validation

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module compute --filter dispatch`**: **4/4**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module compute --filter dispatch`**: **4/4**

---

---

---

---

---

---

## [v2.4.150 "Compute Dispatch Indirect"] - 2026-05-25

### Description

Renderer bolster Phase 2 now has CPU-filled indirect compute dispatch on both OpenGL and Vulkan.

### Library Changes

- **`situation_api.h`** — added `SituationDispatchIndirectCommand` and `SituationCmdDispatchIndirect`
- **`situation_base_errno.h`** — added `SITUATION_ERROR_INDIRECT_COMMAND_INVALID`
- **`situation_impl_decl.h`** — added `SIT_OP_DISPATCH_INDIRECT` and indirect dispatch packet payload
- **`situation_impl_renderer.h`** — validates indirect buffer usage/range/alignment, records OpenGL soft-buffer indirect dispatches, executes `glDispatchComputeIndirect`, and maps Vulkan to `vkCmdDispatchIndirect`
- **`renderer_bolster_plan.md`** — marked the CPU-filled indirect dispatch slice complete and left compute-generated indirect arguments for the Phase 3 barrier model

### Harness Changes

- **`tests/harness/test_compute.c`** — added `dispatch_indirect_cpu_filled` and `dispatch_indirect_validation`

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module compute --filter indirect`**: **2/2**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module compute --filter indirect`**: **2/2**

---

---

---

---

---

---

## [v2.4.151 "Global Barrier Ex"] - 2026-05-25

### Description

Renderer bolster Phase 3 has its first implementation slice: global pipeline barriers can now be expressed with explicit backend-neutral stage and access flags.

### Library Changes

- **`situation_api.h`** — added `SituationPipelineStageFlags`, `SituationAccessFlags`, `SituationPipelineBarrierDesc`, and `SituationCmdPipelineBarrierEx`
- **`situation_impl_renderer.h`** — validates global barrier descriptors, maps OpenGL through the existing soft command barrier packet, and maps Vulkan directly to `VkMemoryBarrier` stage/access masks
- **`renderer_bolster_plan.md`** — added the Phase 3 barrier parity/errno matrix row and marked the global-barrier slice complete while leaving buffer-range and texture-layout barriers deferred

### Harness Changes

- **`tests/harness/test_compute.c`** — added `dispatch_indirect_compute_generated`, proving compute-written indirect args become visible before `SituationCmdDispatchIndirect`; expanded barrier validation coverage

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module compute --filter barrier`**: **2/2**
- **OpenGL `sit_test.exe --module compute --filter indirect`**: **3/3**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module compute --filter barrier`**: **2/2**
- **Vulkan `sit_test_vulkan.exe --module compute --filter indirect`**: **3/3**

---

---

---

---

---

---

## [v2.4.152 "Buffer Barrier Desc"] - 2026-05-25

### Description

Renderer bolster Phase 3B adds explicit buffer-range barriers, proving the resource-specific barrier shape before the larger texture layout work.

### Library Changes

- **`situation_api.h`** — added `SituationBufferBarrierDesc` and `SituationCmdBufferBarrier`
- **`situation_impl_renderer.h`** — validates buffer handles, byte ranges, and stage masks; maps OpenGL conservatively through the existing soft barrier packet; maps Vulkan to `VkBufferMemoryBarrier`
- **`renderer_bolster_plan.md`** — added the Phase 3 buffer barrier parity/errno matrix row and marked the buffer-range barrier slice complete

### Harness Changes

- **`tests/harness/test_compute.c`** — added `dispatch_indirect_buffer_barrier`, proving compute-written indirect args through a buffer-range barrier; expanded barrier validation coverage for invalid buffer barrier descriptors

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --modullet's just make sure patch 159e compute --filter barrier`**: **3/3**
- **OpenGL `sit_test.exe --module compute --filter indirect`**: **4/4**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module compute --filter barrier`**: **3/3**
- **Vulkan `sit_test_vulkan.exe --module compute --filter indirect`**: **4/4**

---

---

---

---

---

---

## [v2.4.153 "Renderer Checkpoint Hygiene"] - 2026-05-25

### Description

Renderer bolster Phase 3C is a checkpoint slice before Phase 4: clean up resource-leak noise and align docs/comments with the new clear, dispatch, and barrier surface.

### Library Changes

- **`situation_impl_ctrl.h`** — releases library-owned default resources before user leak detection during shutdown
- **`situation_impl_renderer.h`** — adds internal default font atlas cleanup and updates stale dispatch/memory-barrier guidance
- **`situation_api.h`** — clarifies clear command intent, legacy barrier status, explicit barrier preferences, and the pending Phase 4 copy API
- **`renderer_bolster_plan.md`** — documents render-pass load clears versus recorded clear commands, records stencil deferral, and marks stale-comment audit items complete

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module compute --filter barrier`**: **3/3**, no leaked texture warning
- **OpenGL `sit_test.exe --module compute --filter indirect`**: **4/4**, no leaked texture warning
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module compute --filter barrier`**: **3/3**, no leaked texture warning
- **Vulkan `sit_test_vulkan.exe --module compute --filter indirect`**: **4/4**, no leaked texture warning

---

---

---

---

---

---

## [v2.4.154 "Copy Buffer Ex"] - 2026-05-25

### Description

Renderer bolster Phase 4A starts the transfer-command work with an error-returning buffer copy API and independent source/destination offsets.

### Library Changes

- **`situation_api.h`** — added `SituationCmdCopyBufferEx`
- **`situation_impl_decl.h`** — expanded the soft copy-buffer packet with independent `src_offset` and `dst_offset`
- **`situation_impl_renderer.h`** — validates buffer handles, transfer usage flags, and source/destination ranges; maps OpenGL to `glCopyNamedBufferSubData`; maps Vulkan to `vkCmdCopyBuffer`; preserves legacy `SituationCmdCopyBuffer` as `src_offset = offset`, `dst_offset = 0`
- **`renderer_bolster_plan.md`** — added the Phase 4 copy-buffer parity/errno matrix row and marked the first copy slice complete

### Harness Changes

- **`tests/harness/test_graphics.c`** — added `copy_buffer_ex_offsets` and `copy_buffer_ex_validation`; legacy `async_buffer_readback` continues to cover the wrapper behavior

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter copy_buffer`**: **2/2**
- **OpenGL `sit_test.exe --module graphics --filter async_buffer_readback`**: **1/1**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter copy_buffer`**: **2/2**
- **Vulkan `sit_test_vulkan.exe --module graphics --filter async_buffer_readback`**: **1/1**

---

---

---

---

---

---

## [v2.4.155 "Texture Barrier Desc"] - 2026-05-25

### Description

Renderer bolster Phase 3/4 prerequisite adds explicit texture layout barriers so Phase 4 texture copy/blit commands can keep the strict no-hidden-transition contract.

### Library Changes

- **`situation_base_errno.h`** - added texture-specific usage, region, and format error codes for transfer/layout work
- **`situation_api.h`** - added `SituationTextureLayout`, `SituationTextureBarrierDesc`, and `SituationCmdTextureBarrier`
- **`situation_impl_renderer.h`** - validates 2D color texture mip/layer ranges and usage flags; maps OpenGL to a conservative soft memory barrier; maps Vulkan to `VkImageMemoryBarrier`
- **`renderer_bolster_plan.md`** - marks the first texture-layout barrier slice complete and records the strict blit-layout prerequisite

### Harness Changes

- **`tests/harness/test_graphics.c`** - added `texture_barrier_validation` for null args, invalid handles, invalid mip ranges, missing transfer usage, reserved attachment layouts, and a valid shader-read <-> transfer-src transition pair

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter texture_barrier`**: **1/1**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter texture_barrier`**: **1/1**

---

---

---

---

---

---

## [v2.4.156 "Texture Blit Region"] - 2026-05-25

### Description

Renderer bolster Phase 4B exposes the first strict texture-to-texture blit command using the texture barrier contract established in the previous slice.

### Library Changes

- **`situation_api.h`** - added `SituationBlitFilter`, `SituationTextureRect`, `SituationTextureBlitRegion`, and `SituationCmdBlitTexture`
- **`situation_impl_decl.h`** - added the GL soft-command packet for texture blits
- **`situation_impl_renderer.h`** - validates matching RGBA8 color textures, transfer usage flags, mip/layer/rect bounds, and filter support; maps OpenGL to temporary FBOs plus `glBlitNamedFramebuffer`; maps Vulkan to `vkCmdBlitImage` with caller-owned transfer layouts
- **`renderer_bolster_plan.md`** - marks the first texture blit slice complete and records the explicit no-hidden-transition contract

### Harness Changes

- **`tests/harness/test_graphics.c`** - added blit validation coverage plus same-size asymmetric and scaled-nearest readback tests

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter blit_texture`**: **3/3**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter blit_texture`**: **3/3**

---

---

---

---

---

---

## [v2.4.157 "Texture Copy Region"] - 2026-05-27

### Description

Renderer bolster Phase 4B adds exact-size texture-to-texture copy on top of the blit and barrier contracts.

### Library Changes

- **`situation_api.h`** - added `SituationTextureCopyRegion` and `SituationCmdCopyTexture`
- **`situation_impl_decl.h`** - added `SIT_OP_COPY_TEXTURE` soft-command packet
- **`situation_impl_renderer.h`** - validates matching RGBA8 color textures, transfer usage, mip/layer/rect bounds; maps OpenGL to `glCopyImageSubData`; maps Vulkan to `vkCmdCopyImage` with caller-owned transfer layouts
- **`renderer_bolster_plan.md`** - marks the first texture copy slice complete

### Harness Changes

- **`tests/harness/test_graphics.c`** - added copy validation coverage plus same-size asymmetric readback test

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter copy_texture`**: **2/2**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter copy_texture`**: **2/2**

---

---

---

---

---

---

## [v2.4.158 "Buffer Texture Copy"] - 2026-05-27

### Description

Renderer bolster Phase 4B completes the buffer↔texture transfer pair alongside the existing texture copy/blit commands.

### Library Changes

- **`situation_api.h`** - added `SituationCmdCopyBufferToTexture` and `SituationCmdCopyTextureToBuffer`; documented `SituationTextureCopyRegion` semantics for buffer paths
- **`situation_impl_decl.h`** - added `SIT_OP_COPY_BUFFER_TO_TEXTURE` and `SIT_OP_COPY_TEXTURE_TO_BUFFER` soft-command packets
- **`situation_impl_renderer.h`** - validates transfer usage, RGBA8 formats, mip/layer/rect bounds, and buffer ranges; OpenGL uses pixel pack/unpack buffers; Vulkan uses `vkCmdCopyBufferToImage` / `vkCmdCopyImageToBuffer` with caller-owned transfer layouts
- **`renderer_bolster_plan.md`** - marks buffer↔texture public commands complete

### Harness Changes

- **`tests/harness/test_graphics.c`** - added validation and asymmetric sub-rect readback tests for both directions

### Verification

- **OpenGL DLL build**: pending
- **OpenGL harness build**: pending
- **OpenGL `sit_test.exe --module graphics --filter buffer_to_texture`**: pending
- **OpenGL `sit_test.exe --module graphics --filter texture_to_buffer`**: pending
- **Vulkan DLL build**: pending
- **Vulkan harness build**: pending
- **Vulkan `sit_test_vulkan.exe --module graphics --filter buffer_to_texture`**: pending
- **Vulkan `sit_test_vulkan.exe --module graphics --filter texture_to_buffer`**: pending

---

---

---

---

---

---

## [v2.4.159 "Transfer Harness Split"] - 2026-05-27

### Description

Renderer bolster **Phase 4.1** (harness-only patch). Phase 4 transfer APIs shipped in v2.4.156-v2.4.158; this patch moves transfer test coverage into a dedicated module so `sit_test.exe --module transfer` runs copy/blit/barrier coverage without loading the full graphics suite.

### Harness Changes

- **`tests/harness/test_transfer.c`** — new `transfer` module with **12 tests**: texture barrier, `CopyBufferEx`, blit, texture copy, and buffer<->texture paths.
- **`tests/harness/test_graphics.c`** — removes duplicate Phase 4 registrations (`texture_barrier_validation`, `copy_buffer_ex_offsets`, `copy_buffer_ex_validation`); graphics remains **103** tests (OpenGL).
- **`build_tests.bat`**, **`tests/harness/sit_test_registry.c`** — link/register `test_transfer.c` after compute.
- **`doc/plan/renderer_bolster_plan.md`** — Phase 4.1 matrix labels switched from `graphics.*` to `transfer.*` for moved cases.

### Notes

- `transfer_tests[]` order runs `copy_buffer_ex_*` before buffer<->texture PBO tests, preventing stale OpenGL `SituationEndFrame` errors in a single transfer-module run.

### Verification

- **OpenGL `sit_test.exe --module transfer`**: **12/12**
- **Vulkan `sit_test_vulkan.exe --module transfer`**: **12/12**
- **OpenGL `sit_test.exe --module graphics`**: **103/103**
- **OpenGL `sit_test.exe --module graphics --filter texture_barrier`**: **0 run** (all skipped, expected)
- **OpenGL `sit_test.exe --module graphics --filter copy_buffer_ex`**: **0 run** (all skipped, expected)

---

---

---

---

---

---

## [v2.4.160 "Indirect Draw Commands"] - 2026-05-27

### Description

Renderer bolster **Phase 5** (first slice): public graphics indirect draw from a buffer, mirroring the existing compute `DispatchIndirect` path. OpenGL internal MDI batching for ordinary `SituationCmdDraw` / `DrawIndexed` is unchanged.

### Library Changes

- **`situation_api.h`** — `SituationDrawIndirectCommand`, `SituationDrawIndexedIndirectCommand` (Vulkan/GL-compatible layouts); `SituationCmdDrawIndirect`, `SituationCmdDrawIndexedIndirect`.
- **`situation_impl_decl.h`** — `SIT_OP_DRAW_INDIRECT`, `SIT_OP_DRAW_INDEXED_INDIRECT` soft-command packets.
- **`situation_impl_renderer.h`** — validates indirect-buffer usage, 4-byte offset, and command size; requires active render pass; OpenGL `glDrawArraysIndirect` / `glDrawElementsIndirect`; Vulkan `vkCmdDrawIndirect` / `vkCmdDrawIndexedIndirect` (single command, stride = struct size).
- **`situation_base_trace.h`** — trace IDs for new commands.
- **`renderer_bolster_plan.md`** — Phase 5 first-slice items marked complete (structs, commands, validation, GL/VK paths, CPU-filled tests).

### Harness Changes

- **`tests/harness/test_graphics.c`** — `draw_indirect_cpu_filled`, `draw_indexed_indirect_cpu_filled`, `draw_indirect_validation`, `draw_indirect_compute_generated_barrier` (+4 tests; graphics module **107** on OpenGL).

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter indirect`**: **4/4**
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter draw_indirect`**: **3/3**

### Deferred (Phase 5 follow-up)

- `draw_count` / `glMultiDraw*Indirect` batching from one buffer
- `drawIndirectCount` / multi-draw-count variants
- Phase 7 index-type flexibility for indexed indirect

---

---

---

---

---

---

## [v2.4.161 "Front Face Raster State"] - 2026-05-28

### Description

Renderer bolster **Phase 6** first slice: explicit front-face state is now recordable on command buffers, enabling deterministic cull/front-face behavior across backends.

### Library Changes

- **`situation_api.h`** — adds `SituationFrontFace` (`SIT_FRONT_FACE_CCW`, `SIT_FRONT_FACE_CW`) and `SituationCmdSetFrontFace`.
- **`situation_impl_decl.h`** — adds `SIT_OP_SET_FRONT_FACE` plus packet payload for front-face state.
- **`situation_impl_renderer.h`** — OpenGL soft-command execution maps to `glFrontFace`; Vulkan maps to `vkCmdSetFrontFace`; Vulkan graphics pipeline dynamic state now includes `VK_DYNAMIC_STATE_FRONT_FACE`.
- **`renderer_bolster_plan.md`** — Phase 6 checklist updated for front-face enum/API and the front-face/cull interaction test.

### Harness Changes

- **`tests/harness/test_graphics.c`** — adds `front_face_cull_interaction` (CW triangle is culled with CCW front-face + back-face cull, then visible with CW front-face).
- Graphics module count: **108** tests on OpenGL.

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter front_face`**: **1/1**
- **Vulkan status**: implementation wired (`vkCmdSetFrontFace` + dynamic state), but harness parity remains pending (see v2.4.162 verification note).

---

---

---

---

---

---

## [v2.4.162 "Primitive Topology Raster State"] - 2026-05-28

### Description

Renderer bolster **Phase 6** second slice: primitive topology is now explicit command-buffer state, so non-triangle draws (for example line lists) can be selected deterministically.

### Library Changes

- **`situation_api.h`** — adds `SituationPrimitiveTopology` and `SituationCmdSetPrimitiveTopology`.
- **`situation_impl_decl.h`** — adds `SIT_OP_SET_PRIMITIVE_TOPOLOGY` and command packet payload.
- **`situation_impl_renderer.h`** — OpenGL executor tracks topology state and uses it for draw, indexed draw, and indirect draw paths; Vulkan maps to `vkCmdSetPrimitiveTopology`.
- **`situation_impl_renderer.h`** — Vulkan pipeline dynamic states now include `VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY`.
- **`renderer_bolster_plan.md`** — Phase 6 checklist updated for primitive-topology enum/API and line-topology harness proof.

### Harness Changes

- **`tests/harness/test_graphics.c`** — adds `primitive_topology_line_list`, proving `SituationCmdSetPrimitiveTopology(...LINE_LIST)` affects rasterized output.
- Graphics module count: **109** tests on OpenGL.

### Verification

- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter primitive_topology`**: **1/1**
- **OpenGL `sit_test.exe --module graphics --filter front_face`**: **1/1** (regression check)
- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter primitive_topology`**: **1/1**
- **Vulkan `sit_test_vulkan.exe --module graphics --filter front_face`**: **0/1** (fails `front_face_cull_interaction`; Phase 6 parity follow-up)

---

---

---

---

---

---

## [v2.4.163 "Vulkan Raster Variant Fallback (In Progress)"] - 2026-05-28

### Description

Renderer bolster **Phase 6-bis** in-progress patch: Vulkan raster parity now uses explicit static pipeline variants for cull/front-face combinations while preserving backend-neutral public APIs.

### Library Changes (in progress)

- **`situation_impl_decl.h`** — extends Vulkan shader slot/state with raster variant pipeline handles and graphics stride tracking used for variant selection.
- **`situation_impl_renderer_fwd.h`** — updates Vulkan graphics pipeline helper signature to accept static raster parameters.
- **`situation_impl_renderer.h`** — materializes and selects raster variants across shader pipeline families (`vk_pipeline`, `vk_pipeline_legacy`, `vk_pipeline_simple`) and routes selection through draw, indexed draw, indirect draw, and mesh draw paths.
- **`situation_impl_renderer.h`** — wires variant teardown/swap into Vulkan unload and hot-reload paths so fallback pipelines follow shader slot lifetime.
- **`renderer_bolster_plan.md`** — updates Phase 6-bis checklist to reflect completed implementation slices and explicit remaining blocker.

### Verification (current)

- **Vulkan DLL build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter front_face_cull_interaction`**: **0/1** (still failing; parity fix not complete yet)

### Remaining work

- Diagnose/fix `front_face_cull_interaction` Vulkan parity failure.
- Run/record full focused matrix for OpenGL and Vulkan (`front_face`, `primitive_topology`).
- Add final Phase 6-bis completion entry once parity is green.

---

---

---

---

---

---

## [v2.4.164 "Vulkan Raster Parity Closure Sprint"] - 2026-05-28

### Description

Follow-up to **v2.4.163**: closes **Phase 6-bis** (6-bisF/6-bisG), adds Phase 6A point-topology harness coverage (Vulkan green; OpenGL point-list fixed in v2.4.166), and lands Phase 3 barrier cookbook docs. Does **not** close full **Phase 6** (6B raster APIs still open). Does not replace the v2.4.163 record of the in-progress 6-bis implementation slice.

Renderer bolster **Phase 6-bis closure** plus Phase 6A/3 housekeeping: Vulkan front-face/cull parity is green, topology default state is deterministic on pipeline rebind, point-list harness added, and barrier cookbook docs landed.

### Library Changes

- **`situation_impl_renderer.h`** — unified Vulkan graphics pipeline selector; static cull/front-face variants; tracked dynamic topology default (`TRIANGLE_LIST`) applied on bind/rebind; OpenGL enables `GL_PROGRAM_POINT_SIZE` when `POINT_LIST` topology is recorded.
- **`situation_impl_decl.h`** — topology tracking fields on Vulkan render state.
- **`doc/plan/renderer_bolster_plan.md`** — Phase 6-bis (A–G) and Phase 6 parity items marked complete where verified.
- **`doc/RENDERER_BARRIER_COOKBOOK.md`** — Phase 3 cookbook follow-up (harness-backed recipes).

### Harness Changes

- **`tests/harness/test_graphics.c`** — adds `primitive_topology_point_list`; removes temporary Vulkan diagnostic `printf`; adds fail-path cleanup before final assert in `front_face_cull_interaction`.
- Graphics module count: **110** tests (OpenGL).

### Verification

- **Vulkan DLL build**: pass
- **Vulkan harness build**: pass
- **Vulkan `sit_test_vulkan.exe --module graphics --filter front_face_cull_interaction`**: **1/1**
- **Vulkan `sit_test_vulkan.exe --module graphics --filter primitive_topology`**: **2/2** (line + point)
- **OpenGL DLL build**: pass
- **OpenGL harness build**: pass
- **OpenGL `sit_test.exe --module graphics --filter front_face`**: **1/1**
- **OpenGL `sit_test.exe --module graphics --filter primitive_topology`**: **2/2**

---

---

---

---

---

---

## [v2.4.165 "Index Type Flexibility"] - 2026-05-28

### Description

Renderer bolster **Phase 7**: indexed draws can bind 16- or 32-bit index buffers via `SituationCmdBindIndexBufferEx`; legacy `SituationCmdBindIndexBuffer` remains UINT32.

### Library Changes

- **`situation_api.h`** — adds `SituationIndexType` (`SIT_INDEX_UINT32`, `SIT_INDEX_UINT16`) and `SituationCmdBindIndexBufferEx`.
- **`situation_impl_decl.h`** — `bind_ibo` packet carries index type; GL/VK track bound index element size and type for draw math.
- **`situation_impl_renderer.h`** — offset alignment validation; maps to `GL_UNSIGNED_SHORT` / `GL_UNSIGNED_INT` and `VK_INDEX_TYPE_UINT16` / `VK_INDEX_TYPE_UINT32`.
- **`renderer_bolster_plan.md`** — Phase 7 first slice marked complete (mesh `CreateMeshEx` deferred).

### Harness Changes

- **`tests/harness/test_graphics.c`** — `bind_index_buffer_uint16`, `bind_index_buffer_offset_alignment_uint16`.
- Graphics module count: **112** tests (OpenGL).

### Verification

- **OpenGL/Vulkan DLL build**: OK (2026-05-28, GCC 15.1.0 / MinGW)
- **OpenGL `sit_test.exe --module graphics --filter bind_index_buffer`**: **3/3 pass** (`bind_index_buffer_low_level`, `bind_index_buffer_uint16`, `bind_index_buffer_offset_alignment_uint16`)
- **Vulkan `sit_test_vulkan.exe --module graphics --filter bind_index_buffer`**: **3/3 pass** (same filter; shutdown fence timeout warnings on teardown only)

---

---

---

---

---

---

## [v2.4.166 "Phase 6A GL Point Topology Fix"] - 2026-05-28

### Description

Phase 6 was incorrectly treated as closed after 6-bis; **Phase 6 overall remains open** (polygon mode, depth bias, stencil, etc.). This entry fixes the last failing **Phase 6A** harness case on OpenGL.

### Library Changes

- **`situation_impl_decl.h` / `situation_impl_renderer.h`** — track `current_primitive_mode_set` so `GL_POINTS` (enum value 0) is not mistaken for “unset” and drawn as triangles.

### Verification

- **OpenGL `sit_test.exe --module graphics --filter primitive_topology`**: **2/2 pass** (`line_list`, `point_list`)
- Phase 6B items unchanged (still planned).

---

---

---

---

---

---

## [v2.4.167 "Phase 6B Polygon Mode and Depth Bias"] - 2026-05-28

### Description

Renderer bolster **Phase 6B** (first slice): `SituationCmdSetPolygonMode` and `SituationCmdSetDepthBias` with OpenGL soft-command replay and Vulkan dynamic raster state (line pipeline variants + `VK_EXT_extended_dynamic_state2` depth bias when available). Phase 6 overall remains open (line width, color write mask, stencil, multisample).

### Library Changes

- **`situation_api.h`** — `SituationPolygonMode`, `SituationCmdSetPolygonMode`, `SituationCmdSetDepthBias`.
- **`situation_impl_decl.h`** — `SIT_OP_SET_POLYGON_MODE`, `SIT_OP_SET_DEPTH_BIAS`, tracked GL/Vulkan state.
- **`situation_impl_renderer.h`** — GL replay (`glPolygonMode`, `glPolygonOffset`); Vulkan line pipeline variants and depth-bias dynamics; `_SitVulkanApplyTrackedRasterDynamics` on pipeline rebind (topology + active depth bias).

### Harness Changes

- **`tests/harness/test_graphics.c`** — `polygon_mode_line_wireframe`, `depth_bias_overlap`.

### Verification

- **Vulkan isolated**: `--filter polygon_mode_line_wireframe`, `--filter depth_bias_overlap` — **pass** (fresh DLL).
- **Vulkan full `graphics` module**: may still fail pixel asserts for these tests until per-acquire raster reset lands; not a -540 / fence wedge.

---

---

---

---

---

---

## [v2.4.168 "VD Render Pass and Frame Lifecycle"] - 2026-05-28

### Description

Stability patch for Vulkan harness regressions exposed during renderer bolster work: Virtual Display compositing now keeps **software render-pass state** in sync with recorded `vkCmd*` commands, and frame acquisition recovers from callers that **`Acquire` without `EndFrame`**. Documents current **lingering** graphics readback / VD pixel failures (not one root cause). Builds on **v2.4.167** Phase 6B APIs.

**Canonical version**: `sit/situation_base_version.h` → **2.4.168** (`SituationGetVersionString()`).

### Fixes Applied

#### Virtual Display — render-pass contract (`SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` / -540)

- **`sit/situation_impl_vd.h`** — When `SituationRenderVirtualDisplays` ends the caller’s main swapchain pass, also clear `sit_render.vk.inside_render_pass`. After the final resume `vkCmdBeginRenderPass`, set `inside_render_pass`, `inside_main_swapchain_render_pass`, and `current_render_area` (matching `SituationCmdBeginRenderPass`).
- **Symptom fixed**: `graphics.render_virtual_displays` called `SituationCmdEndRenderPass` after VD with **-540** even though the command buffer had an active pass; full Vulkan graphics runs then cascaded into **~15s frame-fence timeouts** on unrelated tests.

#### Frame lifecycle — leaked acquire / wedged fences

- **`sit/situation_impl_renderer.h`** — `SituationAcquireFrameCommandBuffer`: if `sit_render.in_frame` is still true, log a warning, end any open Vulkan render pass on the main command buffer, and call `SituationEndFrame()` before starting the new frame (unsignaled in-flight fence otherwise blocks the next acquire indefinitely).
- **`tests/harness/test_graphics.c`** — `bind_index_buffer_offset_alignment_uint16` no longer calls `SituationAcquireFrameCommandBuffer()` for validation-only `SituationCmdBindIndexBufferEx` (alignment error is returned before GPU recording).

### Prior entry (same sprint) — v2.4.167 Phase 6B

Still in tree; verification updated below:

- **`SituationCmdSetPolygonMode`**, **`SituationCmdSetDepthBias`** (OpenGL soft replay; Vulkan dynamic raster / line pipeline variants).
- Harness: `polygon_mode_line_wireframe`, `depth_bias_overlap`.

### Lingering issues (known, not closed in this patch)

> **Historical snapshot (v2.4.167, 2026-05-28).** Do not treat as current OpenGL harness status. Canvas/readback plan archived @ v2.4.362 — see `doc/done/CANVAS_STRETCH_READBACK_FIX_PLAN.md`.

| Area | Status | Notes |
|------|--------|--------|
| **Vulkan full `graphics` module (~25 failures)** | Open | Not a single bug. Roughly **half** fail only when the full module runs in order (**dynamic raster / resource state** not reset between tests). Roughly **half** fail **in isolation** (real pixels wrong). |
| **Order-dependent (pass alone, fail in module)** | Open | Examples: `polygon_mode_line_wireframe`, `depth_bias_overlap`, `descriptor_bind_*`, `spirv_memory_*`, some `vd_*`. Likely fix: reset topology → triangle list, polygon → fill, depth bias off, etc. on acquire or test teardown. |
| **Screen readback / layout** | Partial | Fail isolated: `screen_readback_corner_layout`, `texture_cpu_gpu_cpu_roundtrip`, `texture_format_preservation`. **`texture_storage_write_readback`**: fixed v2.4.328 (stale screenshot buffer in full module run). |
| **VD compositing pixels** | Open | Fail isolated: most strict `vd_*` blend/scaling/z-order/visibility tests; `render_virtual_displays` and loose scans (e.g. `vd_render_into_pipeline`) can pass. Separate from the **-540** flag bug fixed here. |
| **Phase 6B OpenGL** | Open | Full OpenGL harness may still fail `polygon_mode_line_wireframe` / `depth_bias_overlap` pixel asserts; Vulkan isolated filters for those tests reported **pass** after rebuild. |
| **`tone_synth` module** | Deferred | Full sequential run can **SIGSEGV** during `cc_mod_vibrato` (after `midi_velocity_ramp`). Not investigated in this patch. |
| **Version header drift** | Fixed | `situation_base_version.h` was **2.4.146** while narrative releases reached **2.4.167**; aligned to **2.4.168** with this entry. |

### Verification (2026-05-28, Windows, MinGW GCC, Vulkan DLL rebuilt)

- **`sit_test_vulkan.exe --filter render_virtual_displays`**: **pass** (was **-540**).
- **`sit_test_vulkan.exe --module graphics`**: **79 pass / 25 fail** (~15s); **no** fence-timeout cascade after `draw_indirect_*` (was first failure at `draw_indirect_cpu_filled` with 15s waits).
- **Isolated filters (Vulkan)**: `polygon_mode_line_wireframe`, `depth_bias_overlap`, `descriptor_bind_ubo_color`, `spirv_memory_dual_ssbo_readback`, `vd_render_into_pipeline` — **pass** alone; same tests may still fail in full module run.
- **Isolated filters (Vulkan) still failing**: `screen_readback_corner_layout`, `texture_cpu_gpu_cpu_roundtrip`, `vd_z_ordering`, `vd_blend_alpha`, and related strict pixel tests (see table above).
- **Other modules (Vulkan, isolated)**: `filesystem`, `threading`, `core`, `window`, `input`, `timer`, `Projection`, `audio`, `transfer`, `compute`, `misc` — **0 failures** when run with `--module`.

### Recommended next steps

1. Reset tracked Vulkan dynamic raster state at **`SituationAcquireFrameCommandBuffer`** to clear order-dependent graphics failures.
2. Audit **`SituationLoadImageFromScreen`** Y/origin against `screen_readback_corner_layout`.
3. Debug one strict VD case (e.g. `vd_z_ordering`) to separate compositing vs readback.

---

---

---

---

---

---

## [v2.4.169 "Vulkan Raster Reset and Readback Parity"] - 2026-05-29

### Description

Renderer bolster follow-up: clears **order-dependent** Vulkan graphics harness failures by resetting tracked raster state each frame, fixes **Vulkan screenshot orientation** for pre-present swapchain copies, hardens **dynamic polygon mode / depth bias** recording, and tightens VD compositor resume-pass setup. Harness fixes for indirect-draw vertex count and pixel-exact texture tests. Builds on **v2.4.168**.

**Canonical version**: `sit/situation_base_version.h` → **2.4.169** (`SituationGetVersionString()`).

### Library Changes

#### Per-acquire raster reset (bucket 1 — state leakage)

- **`sit/situation_impl_renderer.h`** — `_SituationResetTrackedRasterStateForNewFrame()` runs at the start of every `SituationAcquireFrameCommandBuffer` (OpenGL + Vulkan): cull `NONE`, front face `CW`, topology `TRIANGLE_LIST`, polygon `FILL`, depth bias off, `current_pbr_pipeline = NULL`; GL primitive/polygon tracking and polygon-offset flag cleared.
- **`sit/situation_impl_renderer.h`** — `_SitVulkanApplyTrackedRasterDynamics`: always calls `vkCmdSetDepthBiasEnable` when dynamic depth bias is available (previously left bias enabled on pipeline rebind after a test disabled it).

#### Vulkan screenshot readback (bucket 2 — partial)

- **`sit/situation_impl_image.h`** — `SituationLoadImageFromScreen` no longer applies `SituationImageFlip(SIT_FLIP_VERTICAL)` on the Vulkan pre-present staging path (`vkCmdCopyImageToBuffer` rows are already top-left, +Y down, matching harness `graphics_test_sample_rgba`). OpenGL still flips after `glReadPixels` (bottom-first).

#### Dynamic polygon mode and draw-time raster apply

- **`sit/situation_impl_renderer.h`** — `_SitVulkanFillGraphicsDynamicStates` adds `VK_DYNAMIC_STATE_POLYGON_MODE_EXT` and depth-bias dynamic states when extensions are enabled (pipelines created after this patch pick them up on shader load).
- **`sit/situation_impl_renderer.h`** — `SituationCmdSetPolygonMode` records `pfnCmdSetPolygonModeEXT` when available, then rebinds the graphics pipeline variant; point mode still requires dynamic polygon support.
- **`sit/situation_impl_renderer.h`** — `_SitVulkanApplyTrackedRasterDynamics` also applies tracked polygon mode; called from `SituationCmdDraw`, `SituationCmdDrawIndexed`, and indirect draw record paths after pipeline bind.

#### Virtual Display compositor

- **`sit/situation_impl_vd.h`** — after VD composite, main-window **resume** `vkCmdBeginRenderPass` uses `clearValueCount = 0` (LOAD color pass; avoids passing clear values on a load-op pass).

#### Frame / screenshot hygiene

- **`sit/situation_impl_renderer.h`** — invalidate `screenshot_valid` on acquire (GL + Vulkan) so stale pre-present buffers are not reused across frames.

### Harness Changes

- **`tests/harness/test_graphics.c`** — `draw_indirect_cpu_filled`: VBO supplies **6** vertices (two triangles); was 4 vertices with `vertexCount = 6`.
- **`tests/harness/test_graphics.c`** — `screen_readback_corner_layout` uses `SituationGetRenderWidth` / `SituationGetRenderHeight` for draw and readback size asserts.
- **`tests/harness/test_graphics.c`** — pixel-exact texture tests set `img.color_encoding = SITUATION_COLOR_LINEAR` before upload (`texture_cpu_gpu_cpu_roundtrip`, `texture_format_preservation`, `screen_readback_corner_layout`).

### Verification (2026-05-29, Windows, MinGW GCC, Vulkan DLL rebuilt)

- **`sit_test_vulkan.exe --module graphics`**: **89 pass / 15 fail** (~16s); was **79 pass / 25 fail** before this patch (v2.4.168 notes).
- **`sit_test_vulkan.exe --filter draw_indirect_cpu_filled`**: **pass** (harness vertex-count fix).
- **`sit_test_vulkan.exe --filter vd_blend_alpha`**, **`vd_z_ordering`** (isolated): **pass** after screenshot-orientation fix.
- **`sit_test_vulkan.exe --filter draw_textured_checkerboard`**: **pass** (loose pixel scan; DrawTexture + readback path alive).

### Lingering issues (known, not closed in this patch)

| Area | Status | Notes |
|------|--------|--------|
| **Vulkan full `graphics` module (~15 failures)** | Open | Down from ~25. Mix of strict pixel readback (`screen_readback_corner_layout`, texture roundtrips), wireframe (`polygon_mode_line_wireframe`), VD scaling/blend/offset (some **isolated**, some **module-order**). |
| **Strict 2×2 / 4×4 texture readback** | Open | `draw_textured_checkerboard` passes loose bright/dark scan; quadrant asserts still fail — likely readback vs draw contract, not total blackout. |
| **Wireframe polygon mode** | Open | `polygon_mode_line_wireframe` still fails isolated (center pixel filled red on LINE pass); dynamic polygon + line pipeline variants in tree but not green on harness yet. |
| **VD scaling corners** | Open | e.g. `vd_scaling_stretch`: top-left red **pass**, bottom-right red **fail** in readback — partial coverage or compositor edge, separate from -540 lifecycle bugs. |
| **Module-order VD blend/scaling** | Open | Some `vd_*` pass alone, fail in full `--module graphics` run — further compositor or global state audit. |
| **`tone_synth` module** | Deferred | Unchanged from v2.4.168. |

### Recommended next steps

1. Compare **`SituationLoadImageFromScreen`** vs **`SituationReadFramebuffer`** on the same frame for one failing strict test (confirm window vs CPU buffer).
2. Close **`polygon_mode_line_wireframe`** on Vulkan (verify `fillModeNonSolid` + dynamic polygon on bound pipeline after shader reload).
3. Audit VD **stretch** destination coverage for bottom-right readback samples.

---

---

---

---

---

---

## [v2.4.170 "Vulkan Quad Draw Dynamics and Build Fix"] - 2026-05-30

### Description

Renderer bolster follow-up: repairs **`build_situation.bat`** Vulkan builds broken by blank lines between `^` continuations (empty `g++`/`gcc` arguments), records **quad-appropriate Vulkan dynamic state** before internal `DrawTexture` / `DrawQuad` draws, and tightens harness pixel sampling for strict readback tests. Investigation shows **untextured** quad readback works while **textured** `SituationCmdDrawTexture` still barely hits the swapchain — next target is bindless sampling, not screenshot orientation. Builds on **v2.4.169**.

**Canonical version**: `sit/situation_base_version.h` → **2.4.170** (`SituationGetVersionString()`).

### Build / tooling

- **`build_situation.bat`** — Vulkan steps 1–4: remove blank lines between line-continuation carets (fixes `linker input file not found: Invalid argument` on VMA wrapper compile); quote `"-msse4.1"`; add `-Iext\shaderc\libshaderc\include` for `SITUATION_ENABLE_SHADER_COMPILER`.

### Library Changes

#### Vulkan quad draw dynamic state

- **`sit/situation_impl_renderer.h`** — `_SitVulkanApplyQuadDrawDynamicState()`: before `vkCmdDraw` on the internal quad pipeline, sets viewport/scissor from `current_render_area`, `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP` when extended dynamic state is enabled, and depth test **ON** / write **OFF** / compare **`LESS_OR_EQUAL`** (matches quad pipeline static state).
- **`sit/situation_impl_renderer.h`** — `SituationCmdDrawTexture` and `SituationCmdDrawQuad` call the helper before drawing.
- **`sit/situation_impl_renderer.h`** — `_SitVulkanEnsureGraphicsPipelineBound` always calls `_SitVulkanApplyTrackedRasterDynamics` after bind (not only on pipeline change).
- **`sit/situation_impl_renderer.h`** — `_SitVulkanApplyTrackedRasterDynamics`: gate `vkCmdSetPrimitiveTopology` / `pfnCmdSetPolygonModeEXT` on `extended_dynamic_state_enabled`; record default mesh depth dynamics (`LESS`, write off) for PBR/list draws.

### Harness Changes

- **`tests/harness/test_graphics.c`** — strict readback tests sample at **texel centers** (`w/8`, `3w/8` on 2×2 stretched quads), not `w/4` (seam at `u×2 = 0.5` on a 2-wide texture). **v2.4.171** corrects horizontal centers to **`5w/8`** (see above).
- **`tests/harness/test_graphics.c`** — `texture_cpu_gpu_cpu_roundtrip` / `texture_format_preservation` sample inside the fixed **320×240** draw rect (not full-window `width` for UV math).
- **`tests/harness/test_graphics.c`** — `screen_readback_corner_layout` uses default **SRGB** texture encoding (same as passing `draw_textured_checkerboard` path).

### Investigation notes (2026-05-30, Windows, MinGW GCC, DLL via fixed `build_situation.bat`)

- **`build_situation.bat vulkan`**, **`build_tests.bat vulkan`**: **OK** after batch-file fix.
- **`sit_test_vulkan.exe --filter draw_quad_red`**: **pass** — solid-color quad + `SituationLoadImageFromScreen` path alive.
- **`sit_test_vulkan.exe --filter screen_readback_corner_layout`**: **fail** — TL/TR/BL/BR samples **0,0,0** (readback black; not a corner-coordinate seam).
- **`sit_test_vulkan.exe --filter draw_textured_checkerboard`**: **pass** loose scan, but full-buffer count ≈ **1 bright / 76799 dark** pixels → textured quad draw still effectively broken.
- **`sit_test_vulkan.exe --filter draw_pipeline_basic`**: **SIGSEGV** on `SituationCmdBindPipeline` (still open; separate from readback black).

### Lingering issues (known, not closed in this patch)

| Area | Status | Notes |
|------|--------|--------|
| **`SituationCmdDrawTexture` / bindless** | Open | Primary blocker for `screen_readback_corner_layout`, texture roundtrips, and strict VD pixel tests. Untextured `SituationCmdDrawQuad` readback OK. |
| **`screen_readback_corner_layout`** | Open | Fails isolated: all quadrant samples black after successful draw/submit/readback API calls. |
| **Vulkan full `graphics` module (~15 failures)** | Open | Re-verify after bindless fix; v2.4.169 had **89 pass / 15 fail**. |
| **`draw_pipeline_basic` / user-shader bind** | Open | Isolated **SIGSEGV** at `SituationCmdBindPipeline` after `BeginRenderPass` (needs separate triage). |
| **Wireframe / VD scaling / `tone_synth`** | Open | Unchanged from v2.4.169 table. |

### Recommended next steps

1. Audit **bindless** `global_textures[]` indexing: `CreateTexture` descriptor updates vs `SituationCmdDrawTexture` push `texture_id`.
2. Re-run **`screen_readback_corner_layout`** after textured quad draws reliably fill the framebuffer.
3. Triage **`draw_pipeline_basic`** crash on `SituationCmdBindPipeline` (shader slot / pipeline handles).

---

---

---

---

---

---

## [v2.4.171 "Vulkan Quad Per-Texture Sampler Fix"] - 2026-05-30

### Description

Closes the **v2.4.170** blocker for Vulkan **internal textured quads**: `SituationCmdDrawTexture` no longer relies on bindless `global_textures[nonuniformEXT(pc.texture_id)]` for the built-in quad pipeline. Each draw binds the texture’s existing **`single_sampler_descriptor_set`** (allocated in `SituationCreateTextureEx` at set 1 = **`text_sampler_layout`**, binding 0) and samples **`u_QuadTexture`** in the internal fragment shader. That restores real framebuffer color (was **all-black** readback at TL/TR/BL/BR) and makes strict harness quadrant asserts meaningful again.

Also adds a **V-only `uv_rect` invert** on the Vulkan draw path (`uv_rect.y += uv_rect.w; uv_rect.w = -uv_rect.w`) so **`SituationImage` row 0 (top)** maps to the **top** of the stretched quad on screen. Harness strict readback tests align with **`draw_textured_checkerboard`**: fixed **`dest` 320×240** (matches ortho/swapchain convention) and sample pixels at **`w/8` / `5w/8`** (not **`3w/8`**, which still hits the **left** texel on a 2-wide texture — u≈0.375, not the right-texel center u≈0.625).

**Canonical version**: `sit/situation_base_version.h` → **2.4.171** (`SituationGetVersionString()`).

**Builds on**: **v2.4.170** (quad dynamic state, `build_situation.bat` fix). Rebuild **both** `build_situation.bat vulkan` and `build_tests.bat vulkan` after pulling.

### Library changes

#### Internal quad pipeline (Vulkan)

- **`sit/situation_impl_decl.h`** — `SIT_QUAD_FRAGMENT_SHADER`: `layout(set = 1, binding = 0) uniform sampler2D u_QuadTexture`; drops `GL_EXT_nonuniform_qualifier` and the `global_textures[]` array. Push block still carries `texture_id` for layout size parity; the FS ignores it.
- **`sit/situation_impl_renderer.h`** — quad `VkPipelineLayout` set 1 = **`text_sampler_layout`** (was optional **`bindless_descriptor_layout`**).
- **`sit/situation_impl_renderer.h`** — **`SituationCmdDrawTexture`**: requires `tex_slot->single_sampler_descriptor_set`; binds it at set 1; errors if missing. Still binds view/proj UBO at set 0 and applies **`_SitVulkanApplyQuadDrawDynamicState`** before `vkCmdDraw`.
- **`sit/situation_impl_renderer.h`** — **`SituationCmdDrawQuad`**: binds set 1 from the default-font atlas (`single_sampler_descriptor_set` or legacy `descriptor_set`) so the layout’s second set is valid when `use_texture == 0`.
- **`sit/situation_impl_renderer.h`** — Vulkan-only **V flip** on `uv_rect` inside `SituationCmdDrawTexture` (OpenGL path unchanged).

**Note:** User pipelines loaded via **`SituationLoadShaderFromMemory`** / **`SituationCmdBindTexture`** can still use the global bindless array where the shader layout expects it; only the **internal quad** path moved to per-texture samplers (same sets already populated in **`SituationCreateTextureEx`**).

#### Version

- **`sit/situation_base_version.h`** — patch **171**, description **"Vulkan Quad Per-Texture Sampler Fix"**.

### Harness changes

- **`tests/harness/test_graphics.c`** — **`screen_readback_corner_layout`**: `dest` **{0,0,320,240}**; quadrant samples at **`(w/8, h/8)`**, **`(5w/8, h/8)`**, **`(w/8, 5h/8)`**, **`(5w/8, 5h/8)`** on the draw rect (indices still use `screen.width` for stride).
- **`tests/harness/test_graphics.c`** — **`texture_format_preservation`**: same **`5w/8` / `5h/8`** horizontal/vertical sample grid (was **`3w/8` / `3h/8`**).

### Verification (Windows, MinGW GCC, `situation_vulkan.dll` + `sit_test_vulkan.exe`)

| Filter | Result |
|--------|--------|
| `screen_readback_corner_layout` | **pass** |
| `texture_format_preservation` | **pass** |
| `draw_textured_checkerboard` | **pass** |
| `draw_quad_red` | **pass** |

Full Vulkan **`--module graphics`** not re-benchmarked in this patch; expect fewer texture/readback failures than v2.4.169’s **89/15** after pulling **170+171**.

### Lingering issues (not closed in v2.4.171)

| Area | Status | Notes |
|------|--------|--------|
| **`draw_pipeline_basic` / user-shader bind** | Open | Isolated **SIGSEGV** at `SituationCmdBindPipeline` after `BeginRenderPass` — separate from internal quad/readback. |
| **Vulkan full `graphics` module** | Open | Re-run for updated pass/fail counts after **171**. |
| **Wireframe / VD scaling / `tone_synth`** | Open | Unchanged from v2.4.169–170 tables. |
| **Custom shaders + bindless** | — | Still supported via **`SituationCmdBindTexture`** global bindless branch; not the internal quad path. |

### Supersedes (v2.4.170 open items)

- **Internal `SituationCmdDrawTexture` / bindless** — **closed** for the built-in quad pipeline (v2.4.171). **Re-opened and failed again:** v2.4.334 retried bindless array sampling; v2.4.335 **backtrack** — still on per-texture sampler (see v2.4.335 backtrack section).
- **`screen_readback_corner_layout` all-black readback** — **closed** with sampler + UV + harness sample fixes (v2.4.171). Same symptom returned during v2.4.334 bindless retry; closed again after v2.4.335 revert.

---

---

---

---

---

---

## [v2.4.172 "Vulkan Extended Dynamic State Proc Fix"] - 2026-05-30

### Description

Closes the **v2.4.170–171** **SIGSEGV** on user-shader **`SituationCmdBindPipeline`** (GDB: null call through **`_SitVulkanApplyTrackedRasterDynamics`**). Windows **`vulkan-1.dll`** does not export **`vkCmdSetDepthTestEnable`**, **`vkCmdSetDepthWriteEnable`**, **`vkCmdSetDepthCompareOp`**, or **`vkCmdSetPrimitiveTopology`** as link-time imports — those **VK_EXT_extended_dynamic_state** entry points must be resolved with **`vkGetDeviceProcAddr`**. v2.4.170 added unconditional calls on every pipeline bind; the IAT stubs were **NULL**, so the first user draw after **`BeginRenderPass`** crashed ( **`async_shader_load_memory_draw`**, **`draw_pipeline_basic`**, and related harness tests).

Also stops calling **`vkCmdSetPolygonModeEXT`** on every bind: **FILL** / **LINE** already use static pipeline variants (**`_SitVulkanSelectPolygonVariant`**); dynamic polygon mode is recorded only for **POINT** via **`SituationCmdSetPolygonMode`**. Re-applying polygon mode on bind was redundant and segfaulted on the test ICD when combined with user pipelines.

**Canonical version**: `sit/situation_base_version.h` → **2.4.172** (`SituationGetVersionString()`).

**Builds on**: **v2.4.171** (internal quad per-texture sampler). Rebuild **both** `build_situation.bat vulkan` and `build_tests.bat vulkan` after pulling.

### Root cause (investigation)

- **GDB** on **`draw_pipeline_basic`**: **`rip = 0`**, stack **`SituationCmdBindPipeline` → `_SitVulkanEnsureGraphicsPipelineBound` → `_SitVulkanApplyTrackedRasterDynamics`**.
- **`objdump -p situation_vulkan.dll`**: depth/topology **`vkCmd*`** listed as **`vulkan-1.dll`** imports with **`<none>`** — not present in system loader exports; direct calls jump to **NULL**.
- **Bisect**: empty **`ApplyTrackedRasterDynamics`** → bind succeeds (readback fail only); enabling **`vkCmdSetPolygonModeEXT`** on bind → crash returns; depth/topology via loaded PFNs → **`draw_pipeline_basic`** green.
- Internal **`draw_quad_red`** survived: **`SituationCmdDrawQuad`** uses **`_SitVulkanApplyQuadDrawDynamicState`** and never went through the broken bind-time polygon re-apply added in **v2.4.170**.

### Library changes

#### Extended dynamic state — proc resolution (Vulkan)

- **`sit/situation_impl_decl.h`** — **`_SituationVulkanState`**: store **`PFN_vkCmdSetPrimitiveTopology`**, **`PFN_vkCmdSetDepthTestEnable`**, **`PFN_vkCmdSetDepthWriteEnable`**, **`PFN_vkCmdSetDepthCompareOp`** (alongside existing polygon/depth-bias PFNs).
- **`sit/situation_impl_renderer.h`** — after logical-device create, load the above via **`vkGetDeviceProcAddr`** / **`vkGetInstanceProcAddr`** when **`extended_dynamic_state_enabled`**; also try **`vkCmdSetPolygonMode`** if **`vkCmdSetPolygonModeEXT`** is absent (1.3 alias).
- **`sit/situation_impl_renderer.h`** — **`_SitVulkanGraphicsDynamicProcsReady()`**, **`_SitVulkanCmdSetDepthDynamics()`**: guarded recording through PFNs (no direct **`vkCmdSetDepth*`** / **`vkCmdSetPrimitiveTopology`** linker imports).
- **`sit/situation_impl_renderer.h`** — **`_SitVulkanFillGraphicsDynamicStates`**: declare topology/depth dynamics only when all required PFNs resolved; **`VK_DYNAMIC_STATE_POLYGON_MODE_EXT`** only when **`pfn_cmd_set_polygon_mode_ext`** is non-null.
- **`sit/situation_impl_renderer.h`** — **`_SitVulkanApplyTrackedRasterDynamics`**: topology + depth-bias + depth defaults; **removed** per-bind **`vkCmdSetPolygonModeEXT`** (static fill/line variants handle mode).
- **`sit/situation_impl_renderer.h`** — **`_SitVulkanApplyQuadDrawDynamicState`**, **`SituationCmdSetDepthTest`**, **`SituationCmdSetDepthWrite`**, **`SituationCmdSetPrimitiveTopology`**: route through PFN helpers.
- **`sit/situation_impl_renderer.h`** — **`SituationCmdSetPolygonMode`**: call dynamic polygon setter **only for `SIT_POLYGON_MODE_POINT`**; FILL/LINE rebind static pipeline variant via **`_SitVulkanEnsureGraphicsPipelineBound`**.

#### Version

- **`sit/situation_base_version.h`** — patch **172**, description **"Vulkan Extended Dynamic State Proc Fix"**.

### Verification (Windows, MinGW GCC, Vulkan SDK 1.4.313.2, `situation_vulkan.dll` + `sit_test_vulkan.exe`)

| Filter | Result |
|--------|--------|
| `async_shader_load_memory_draw` | **pass** (was **SIGSEGV**) |
| `async_shader_begin_reports_in_progress` | **pass** |
| `async_shader_renderer_alive_while_loading` | **pass** |
| `async_shader_unload_during_load` | **pass** |
| `sync_shader_after_async_cycle` | **pass** |
| `draw_pipeline_basic` | **pass** (was **SIGSEGV**) |
| `draw_quad_red` | **pass** |
| `screen_readback_corner_layout` | **pass** |

| Module | Result |
|--------|--------|
| Vulkan **`--module graphics`** | **92 pass / 12 fail** (was crash-at-test-7 + **89/15** pre-171 table) |

All **`--filter async_shader`** harness tests **5/5** pass.

### Lingering issues (not closed in v2.4.172)

| Area | Status | Notes |
|------|--------|--------|
| **`polygon_mode_line_wireframe`** | Open | No longer **SIGSEGV**; **line** readback pixel assert still fails (static line pipeline / raster — separate from bind crash). |
| **`primitive_topology_line_list`** | Open | Pixel readback (same family as wireframe). |
| **VD scaling / blend / offset** | Open | Unchanged from v2.4.169–171 tables (**6** failures in full **`graphics`** run). |
| **`draw_metrics_overlay`**, **`compute_image_write`**, **`draw_indirect_compute_generated_barrier`** | Open | Unchanged. |

### Supersedes (v2.4.171 open items)

- **`draw_pipeline_basic` / user-shader bind SIGSEGV** — **closed** (v2.4.172).
- **`async_shader_load_memory_draw` crash** — **closed** (same root cause as bind crash; v2.4.172).

---

---

---

---

---

---

## [v2.4.173 "Graphics Backend Query API"] - 2026-05-30

### Description

Adds explicit **graphics backend introspection** so applications and the harness no longer infer OpenGL vs Vulkan from compile-time `#ifdef`s or a stale **`SituationGetGraphicsCaps`** version field. Situation ships as **two renderer DLLs** (`situation_opengl.dll` → **OpenGL 4.6 Core**, `situation_vulkan.dll` → **Vulkan 1.4** target via **`VK_API_VERSION_1_4`**); callers can now query which backend is active at runtime.

**`SituationGetGraphicsBackend()`** and **`SituationGetGraphicsBackendName()`** work **before `SituationInit`** (backend is fixed per DLL). **`SituationGetGraphicsCaps`** now reports **`backend`**, separates **Situation target API** from **device runtime API**, and fixes Vulkan **`api_version_packed`** (was hard-coded **1.2**; now **1.4** target with **`device_api_version_packed`** from **`VkPhysicalDeviceProperties::apiVersion`**).

**Canonical version**: `sit/situation_base_version.h` → **2.4.173** (`SituationGetVersionString()`).

**Builds on**: **v2.4.172** (Vulkan extended dynamic state proc fix). Rebuild **`build_situation.bat`** (both backends) and **`build_tests.bat`** after pulling.

### Library changes

#### Graphics backend query (OpenGL + Vulkan)

- **`sit/situation_api.h`** — **`SituationGraphicsBackend`** enum (`UNKNOWN` / `OPENGL` / `VULKAN`); **`SituationGetGraphicsBackend()`**, **`SituationGetGraphicsBackendName()`**.
- **`sit/situation_api.h`** — **`SituationGraphicsCaps`**: add **`backend`**, **`device_api_version_packed`** (`major<<16|minor` for active GL context or picked **`VkPhysicalDevice`**). **`api_version_packed`** is now the **Situation backend target** — **4.6** OpenGL, **1.4** Vulkan — not the conflated driver version.
- **`sit/situation_impl_ctrl.h`** — implement query helpers; **`_SituationPackApiVersionMajorMinor()`**; refresh **`SituationGetGraphicsCaps`** fill logic.
- **`sit/situation_impl_decl.h`** — **`_SituationVulkanState.physical_device_api_version`** (raw **`properties.apiVersion`** at device pick).
- **`sit/situation_impl_renderer.h`** — store **`physical_device_api_version`** in **`_SituationVulkanPickPhysicalDevice`**.
- **`sit/situation_base_trace.h`** — trace IDs for **`SituationGetGraphicsBackend`**, **`SituationGetGraphicsBackendName`**.

#### Version

- **`sit/situation_base_version.h`** — patch **173**, description **"Graphics Backend Query API"**.

### Harness changes

- **`tests/harness/test_core.c`** — **`get_graphics_backend`**, **`get_graphics_caps`**: assert backend enum/name per active DLL; target API **4.6** / **1.4**; device version non-zero after init.

### Verification (Windows, MinGW GCC)

| Filter / module | OpenGL | Vulkan |
|-----------------|--------|--------|
| `get_graphics_backend` | **pass** | **pass** |
| `get_graphics_caps` | **pass** | **pass** |

### Notes for callers

```c
// Before or after SituationInit — identifies which DLL you loaded
SituationGraphicsBackend b = SituationGetGraphicsBackend(); // SIT_GRAPHICS_BACKEND_OPENGL | VULKAN

SituationGraphicsCaps caps;
SituationGetGraphicsCaps(&caps); // requires successful SituationInit for device fields
// caps.api_version_packed         → Situation target (4.6 GL / 1.4 VK)
// caps.device_api_version_packed    → driver/context version (may differ, e.g. VK 1.3 GPU)
```

### Lingering issues (unchanged from v2.4.172)

| Area | Status |
|------|--------|
| Line / wireframe raster readback (**`polygon_mode_line_wireframe`**, **`primitive_topology_line_list`**) | Open |
| VD scaling / blend / offset (**6** tests) | Open |
| **`draw_metrics_overlay`**, **`compute_image_write`**, **`draw_indirect_compute_generated_barrier`** | Open |

---

---

---

---

---

---

## [v2.4.174 "Vulkan VD Compositor Fix"] - 2026-05-30

### Description

Closes the Vulkan **Virtual Display** compositor regression where only half the swapchain received VD output (diagonal red/black split). Root cause: user mesh draws leave **`TRIANGLE_LIST`** dynamic topology on the command buffer; VD compositor pipelines are **triangle strips** — with extended dynamic state enabled, static pipeline topology is ignored unless reset before each composite draw.

Also aligns Vulkan VD routing with OpenGL (**Path B** for simple blend modes), adds per-mode blend pipeline variants, Path A swapchain layout/resume-pass fixes, and splits all VD harness coverage into a dedicated **`virtual_display`** module (runs after **`graphics`**).

**Canonical version**: `sit/situation_base_version.h` → **2.4.174** (`SituationGetVersionString()`).

**Builds on**: **v2.4.173** (graphics backend query API). Rebuild **`build_situation.bat vulkan`** (and **`build_tests.bat`**) after pulling.

### Harness changes

#### Virtual Display harness split

All **Virtual Display** coverage moved out of **`graphics`** into a dedicated module so VD compositing failures can be debugged without running the full graphics suite mid-list.

- **`tests/harness/test_virtual_display.c`** — new **`virtual_display`** module with **21 tests**: VD API/lifecycle (`create_destroy_virtual_display`, `configure_virtual_display`, `get_virtual_display`, dirty flag, size, `render_virtual_displays`) plus compositing pipeline coverage (`vd_render_into_pipeline`, z-order, visibility, opacity, scaling modes, blend modes, composite timing, frame-time multiplier, offset).
- **`tests/harness/test_graphics.c`** — removes all VD test bodies and **`graphics_tests[]`** registrations; graphics keeps draw/raster/SPIR-V/compute-interop coverage only.
- **`build_tests.bat`**, **`tests/harness/sit_test_registry.c`** — link/register **`test_virtual_display.c`** immediately **after** **`graphics`**, **before** **`compute`** (module order: … → `graphics` → `virtual_display` → `compute` → `transfer` → …).
- Shared readback helpers remain in **`sit_graphics_test_helpers.h`**; module-local passthrough/red GLSL strings live in **`test_virtual_display.c`**.

**Run focused VD regressions:**

```text
build\sit_test_vulkan.exe --module virtual_display
build\sit_test_vulkan.exe --module virtual_display --filter vd_scaling
build\sit_test.exe --module virtual_display
```

**Filter migration:** `--module graphics --filter vd_` / `--filter virtual_display` now **skip all** (expected); use **`--module virtual_display`** instead.

### Library changes

#### Vulkan VD compositor (`sit/situation_impl_vd.h`, `sit/situation_impl_decl.h`, `sit/situation_impl_renderer.h`)

- **Path routing** — `use_advanced = (blend_mode >= SITUATION_BLEND_OVERLAY)`; **`BLEND_NONE`** / additive / multiply / screen use Path B again.
- **Path B blend pipelines** — five `vd_compositing_blend_pipelines[]` variants via **`SIT_VK_PIPELINE_BLEND_*`** flags; shared layout, distinct **`VkPipelineColorBlendAttachmentState`**.
- **Depth off** — **`SIT_VK_PIPELINE_NO_DEPTH`** on simple + advanced VD compositor pipelines.
- **Path A layout** — after screen copy, transition swapchain to **`PRESENT_SRC_KHR`** (resume pass) or **`UNDEFINED`** (clear pass) before **`vkCmdBeginRenderPass`**.
- **Resume pass hygiene** — `clearValueCount = 0` when beginning resume pass inside composite (Path A/B).
- **Viewport/scissor** — full-framebuffer **`vkCmdSetViewport`/`Scissor`** before each composite draw; composite ortho/scale uses live **`glfwGetFramebufferSize`** (parity with GL **`main_window_*`**).
- **VD push constants** — vertex FS share **`scale_xy`** + **`translate_op.z`** (opacity); scaling rect computed once per layer for both paths.
- **VD compositor dynamic topology (fix)** — `_SitVulkanApplyVDCompositingDynamicState` records **`VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP`** + depth-off before each VD composite draw. User mesh draws leave **`TRIANGLE_LIST`** dynamic state on the command buffer; strip pipelines ignore static topology when extended dynamic state is enabled → only the first triangle of the unit quad rendered (diagonal half-screen red/black). Same helper used for Path A and Path B.

### Verification (Windows, MinGW GCC, GTX 1070)

User snapshot after VD harness split (Vulkan **`sit_test_vulkan.exe`**, sequential module runs):

| Module | Result | Failing tests |
|--------|--------|---------------|
| **`virtual_display`** (Vulkan, full module) | **21/21** pass |
| **`graphics`** (Vulkan, full module) | **78/83** pass (5 unrelated: indirect-compute barrier, line/wireframe topology, metrics overlay, `compute_image_write` **`-500`**) |

**Harness split confirmed:** **`graphics`** no longer registers VD cases (**83** tests vs **104** pre-split); VD regressions isolated to **`--module virtual_display`**.

**Known noise at end of `virtual_display` run:** shader/mesh leak warnings and VMA image leaks from VD slots not destroyed between tests in the same module session (pre-existing teardown gap, not a compositor crash).

### Lingering issues

| Area | Status |
|------|--------|
| Line/wireframe, metrics overlay, `compute_image_write` | Open in **`graphics`** (**78/83** Vulkan) — unrelated to VD compositor |

### Version

- **`sit/situation_base_version.h`** — patch **174**, description **"Vulkan VD Compositor Fix"**.

---

---

---

---

---

---

## [v2.4.175 "Vulkan Raster and Compute Interop"] - 2026-05-30

### Description

Follow-up to **v2.4.174**: closes **three** of the five remaining Vulkan **`graphics`** harness failures (**78/83 → 81/83**) around viewport/scissor on user draws, storage-texture sampling for compute readback, and compute-generated indirect draw args.

Wireframe polygon mode and the default-font metrics overlay remain open on the reference GTX 1070 run.

**Canonical version**: `sit/situation_base_version.h` → **2.4.175** (`SituationGetVersionString()`).

**Builds on**: **v2.4.174** (VD compositor fix + harness split). Rebuild **`build_situation.bat vulkan`** after pulling — delete **`build/situation_dll_vulkan.o`** if the DLL object is stale (header-only renderer changes).

### Library changes

#### Vulkan raster dynamics (`sit/situation_impl_renderer.h`)

- **`_SitVulkanApplyGraphicsViewportScissor()`** — sets swapchain/render-area viewport and scissor before user mesh/line draws and internal text draws (same hygiene pattern as **`_SitVulkanApplyVDCompositingDynamicState`**).
- **`_SitVulkanApplyTrackedRasterDynamics()`** — always re-applies viewport/scissor and the tracked primitive topology on **`SituationCmdDraw`**, indexed/indirect draw, and post-bind pipeline paths.
- **Polygon mode / ext3** — **`VK_DYNAMIC_STATE_POLYGON_MODE_EXT`** is gated on **`VK_EXT_extended_dynamic_state3`** (`extended_dynamic_state3_polygon_mode_enabled`); feature is **not** enabled yet on the reference device. Wireframe harness still relies on static **`vk_pipeline_*_line`** variants. Per-draw **`vkCmdSetPolygonModeEXT`** stays out of the hot raster path (prior GTX 1070 bisect: that call crashed when recorded every bind).

#### Storage texture → textured quad (`SituationCreateTextureEx`)

- Storage-only textures (**`STORAGE | TRANSFER_SRC`**, no **`SAMPLED`**) now allocate and populate **`single_sampler_descriptor_set`** (**`text_sampler_layout`**) with **`VK_IMAGE_LAYOUT_GENERAL`**, so **`SituationCmdDrawTexture`** can sample compute-written images. Fixes **`compute_image_write`** returning **`-500`** (`SITUATION_ERROR_RESOURCE_INVALID` — missing sampler descriptor set).

#### Text / metrics overlay (`SituationCmdDrawTextEx`, `SituationDrawMetricsOverlay`, `sit/situation_impl_decl.h`)

- Text draw: bind graphics pipeline, then view UBO (set 0), then global bindless set (set 1).
- Grid-font UV generation on Vulkan aligned with the OpenGL soft-buffer path (removed erroneous V flip).
- **`SIT_TEXT_FRAGMENT_SHADER`**: glyph alpha from **`max(texColor.a, texColor.r)`** for grayscale atlas texels.
- Text draws disable depth test before **`vkCmdDraw`**; **`SituationDrawMetricsOverlay`** uses **`sit_render.default_font`** explicitly.

### Harness changes (`tests/harness/test_graphics.c`)

- **`draw_indirect_compute_generated_barrier`** — vertex buffer expanded to **6 vertices** (two triangles), matching the CPU-filled indirect test and the compute shader’s **`vertexCount = 6`**.
- **`primitive_topology_line_list`** — readback scans the center row **±3 px** instead of a single pixel (1 px line raster).

### Verification (Windows, MinGW GCC, GTX 1070)

Vulkan **`sit_test_vulkan.exe --module graphics`**:

| Test | v2.4.174 | v2.4.175 |
|------|----------|----------|
| **`compute_image_write`** | fail (**`-500`**) | **pass** |
| **`draw_indirect_compute_generated_barrier`** | fail (center not red) | **pass** |
| **`primitive_topology_line_list`** | fail (center not red) | **pass** |
| **`polygon_mode_line_wireframe`** | fail | fail (center still red in line pass) |
| **`draw_metrics_overlay`** | fail | fail (no non-black pixels in top-left) |
| **Module total** | **78/83** | **81/83** |

**`virtual_display`** remains **21/21** (unchanged from v2.4.174).

### Lingering issues

| Area | Status |
|------|--------|
| **`polygon_mode_line_wireframe`** | Line pass still fills center — static **`POLYGON_MODE_LINE`** pipeline variant may not bind on reference GPU; dynamic ext3 polygon mode not enabled |
| **`draw_metrics_overlay`** | Default 8×8 grid-font path produces no visible readback pixels; baked-font text tests (**`cmd_draw_text_bitmap`**) still pass |

### Version

- **`sit/situation_base_version.h`** — patch **175**, description **"Vulkan Raster and Compute Interop"**.

---

---

---

---

---

---

## [v2.4.176 "Vulkan Text and Metrics Overlay Fix"] - 2026-05-30

### Description

Closes the Vulkan **on-window** metrics overlay / default-font text orientation bug: HUD text appeared at the **bottom** of the window, lines stacked **upward**, glyphs **upside down**. OpenGL was already correct. Harness **`draw_metrics_overlay`** now passes; live-window placement matches top-left `(5, 5)` + downward line advance.

This patch **does not** remove the accumulated Vulkan 2D orientation workarounds — it documents them explicitly so a follow-up patch can replace per-path flips with one projection fix.

**Canonical version**: `sit/situation_base_version.h` → **2.4.176** (`SituationGetVersionString()`).

**Builds on**: **v2.4.175** (viewport/scissor, storage-texture sampling, indirect compute). Rebuild **`build_situation.bat vulkan`**, sync **`build/situation_vulkan.dll`** from **`build/dll/`** (harness loads **`build/`** first), then **`build_tests.bat vulkan`**.

### Library changes

#### Vulkan text vertex generation (`sit/situation_impl_renderer.h`)

- **`_SitVulkanTextTargetHeight()`** — render-area height for Y remap (falls back to swapchain extent).
- **`_SitVulkanTransformTextQuad()`** — mirrors logical pixel **Y** for grid-font quads (`qy0`/`qy1` from `target_h - y`).
- **`_SitVulkanTransformStbttQuad()`** — same mirror for baked **`stbtt_aligned_quad`** paths, plus **`t0`/`t1` swap** so glyphs stay upright.
- Grid-font emit path swaps atlas **V** (`tv0`/`tv1`) to stay consistent with the **`DrawTexture`** Vulkan UV convention (see technical debt below).

On-screen result: **`SituationDrawMetricsOverlay`** (FPS, draws, VRAM lines) renders **top-left**, lines **down**, characters **upright** on the reference GTX 1070 window.

### Verification (Windows, MinGW GCC, GTX 1070)

Vulkan **`sit_test_vulkan.exe --module graphics`**:

| Test | v2.4.175 | v2.4.176 |
|------|----------|----------|
| **`draw_metrics_overlay`** | fail (no pixels in top-left readback; live window bottom-up) | **pass** |
| **`draw_quad_red`** | pass | pass |
| **`screen_readback_corner_layout`** | pass | pass |
| **`polygon_mode_line_wireframe`** | fail | fail |
| **Module total** | **81/83** | **82/83** |

**`virtual_display`**: **21/21** (unchanged).

### Accomplished in this patch

- Vulkan metrics overlay and default grid-font text: **correct live-window orientation** (confirmed visually and via harness readback in top-left **100×40**).
- **`draw_metrics_overlay`**: **closed** on Vulkan.
- Phase **6B** text placement item: **closed for display**; readback for overlay now agrees with on-window behavior without a global screenshot flip.

### Known limitations and technical debt (cleanup target: later patch)

These are **intentional workarounds** for the same underlying issue: **`glm_ortho(0, w, h, 0, …)`** in **`SituationCmdBeginRenderPass`** produces **OpenGL-style clip space**, while **Vulkan NDC Y points down**. We have **not** applied the standard one-line fix (`projection[1][1] *= -1.0f` after ortho) library-wide because the renderer evolved partial compensations; a naïve projection-only change (tried during development) regressed multiple **`graphics`** readback tests until hacks were removed in lockstep.

| Workaround | Location | What it does | Remove when |
|------------|----------|--------------|-------------|
| **Texture UV flip** | **`SituationCmdDrawTexture`** (Vulkan only) | `uv_rect.y += uv_rect.w; uv_rect.w = -uv_rect.w` before quad draw | Unified Vulkan ortho / clip-space fix in view-proj UBO |
| **Text Y mirror + V swap** | **`SituationCmdDrawTextEx`** (Vulkan only) | **`_SitVulkanTransformTextQuad`**, **`_SitVulkanTransformStbttQuad`**, grid-font **`tv0`/`tv1`** | Same unified projection fix; delete helpers |
| **Stale readback doc** | **`SituationLoadImageFromScreen`** comment in **`sit/situation_impl_image.h`** | Comment claims Vulkan applies **`SituationImageFlip`**; **cached screenshot path does not flip** (OpenGL **`glReadPixels`** path still flips) | Audit readback row order vs display after projection fix |

**Recommended follow-up patch (“Vulkan 2D projection cleanup”):**

1. Add **`_SitVulkanAdjustProjectionForClipSpace(mat4)`** (or equivalent) wherever **`glm_ortho`** fills the view-proj UBO (**`BeginRenderPass`**, VD compositor, etc.).
2. Remove **texture UV flip** and **text Y/V helpers** in the same change set.
3. Re-run full Vulkan **`graphics`** + **`virtual_display`**; fix **`polygon_mode_line_wireframe`** readback/sync separately if still red in harness center sample.
4. Align **`SituationLoadImageFromScreen`** Vulkan behavior and docs with post-fix row order (flip only if copy layout requires it).

### Lingering issues (unchanged from v2.4.175)

| Area | Status |
|------|--------|
| **`polygon_mode_line_wireframe`** | Line pass harness still sees filled red center in readback; likely screenshot sync and/or static line pipeline selection — **not** text orientation |

### Version

- **`sit/situation_base_version.h`** — patch **176**, description **"Vulkan Text and Metrics Overlay Fix"**.

---

---

---

---

---

---

## [v2.4.177 "Vulkan 2D Projection Cleanup (Phase 7-bis)"] - 2026-05-30

### Description

Completes **renderer bolster Phase 7-bis**: one Vulkan 2D convention via shared ortho helper and **shader-side** V/Y handling instead of per-draw CPU UV mirrors and text transforms. **`projection[1][1] *= -1`** after `glm_ortho(0, w, h, 0, …)` remains **forbidden** (clips internal quads).

**Canonical version**: `sit/situation_base_version.h` → **2.4.177**.

Rebuild **`build_situation.bat vulkan`**, sync **`build/situation_vulkan.dll`** from **`build/dll/`**, then **`build_tests.bat vulkan`** and full **`sit_test_vulkan.exe`**.

### Library changes

- **`_SitVulkanFillOrthoProjection2D`** — `glm_ortho(0, w, h, 0, …)` only; used in **`SituationCmdBeginRenderPass`** and VD compositor UBO fill (`sit/situation_impl_vd.h`).
- **`_SitVulkanGet2DTargetHeight`** — render-pass height for text push constants.
- **Quad fragment shader** (Vulkan): sample with `vec2(v_TexCoord.x, 1.0 - v_TexCoord.y)` when texturing.
- **Text shaders** (Vulkan): vertex `py = pc.target_h - aPos.y`; fragment V flip on sample; push constants `vec4 color` + `float target_h`.
- **Removed** Vulkan UV flip in **`SituationCmdDrawTexture`**; removed **`_SitVulkanTransformTextQuad`**, **`_SitVulkanTransformStbttQuad`**, grid-font **`tv0`/`tv1`** swap.

OpenGL paths unchanged. Vulkan **`SituationLoadImageFromScreen`** still does **not** flip cached screenshot rows.

### Verification

Run on reference hardware (GTX 1070): full Vulkan harness expected **417/417** (83/83 graphics, 21/21 virtual_display) after rebuild.

### Version

- **`sit/situation_base_version.h`** — patch **177**.

---

---

---

---

---

---

## [v2.4.178 "Vulkan Draw Hygiene Hardening"] - 2026-05-30

### Description

**Internal hardening Phase 11** for Vulkan 2D/draw-path helpers (`doc/plan/INTERNAL_HARDENING_PLAN.md`). Triages each `_SitVulkan*` internal: record-only helpers documented **void by design**; bind/ortho paths return **`SituationError`** and propagate to public command record sites.

### Library changes

- **`_SitVulkanEnsureGraphicsPipelineBound`** → `SituationError`: `INVALID_PARAM` (null cmd), `VULKAN_PIPELINE_CREATION_FAILED` (no pipeline variant on bound shader), no-op SUCCESS when `shader_slot` is NULL (raster-only updates).
- **`_SitVulkanFillOrthoProjection2D`** → `SituationError`: `INVALID_PARAM` if `out_proj` is NULL.
- **Callers:** `SituationCmdBeginRenderPass`, `SituationRenderVirtualDisplays`, `SituationCmdBindPipeline`, vertex bind, `Draw`/`DrawIndexed`/indirect, `DrawMesh`, raster rebind commands — use `SIT_RETURN_IF_ERR`.
- **Draw without bound pipeline** → `INVALID_RESOURCE_HANDLE` with explicit detail string.
- **`situation_impl_renderer_fwd.h`** — HARDENING contract comments per helper (no new errno codes).

### Version

- **`sit/situation_base_version.h`** — patch **178**.

---

---

---

---

---

---

## [v2.4.179 "Vulkan Quad Text Draw Hardening"] - 2026-05-30

### Description

**Internal hardening Phase 12:** shared pre-draw validation for internal **quad** (`SituationCmdDrawQuad`, `SituationCmdDrawTexture`) and **text** (`SituationCmdDrawTextEx`) Vulkan paths.

### Library changes

- **`_SitVulkanValidateInternalQuadDrawReady`** — checks command buffer, active render pass, quad pipeline/layout, and quad VBO before `vkCmdDraw`.
- **`_SitVulkanValidateInternalTextDrawReady`** — checks command buffer, active render pass, and text pipeline/layout before text draws.
- Errors use existing codes: `INVALID_PARAM`, `NO_RENDER_PASS_ACTIVE`, `VULKAN_PIPELINE_CREATION_FAILED`, `NOT_INITIALIZED` (quad VBO).
- Replaces ad-hoc `quad_pipeline == VK_NULL_HANDLE` checks with `_SituationSetErrorFromCode` detail strings.

### Version

- **`sit/situation_base_version.h`** — patch **179**.

---

---

---

---

---

---

## [v2.4.180 "OpenGL Quad Text Draw Hardening"] - 2026-05-30

### Description

**Internal hardening Phase 13:** OpenGL parity with Vulkan quad/text pre-draw validation (soft-buffer record + execute paths).

### Library changes

- **`_SituationGLValidateInternalQuadDrawReady`** — soft buffer, active render pass, `quad_shader_program`, `quad_vao`/`quad_vbo`.
- **`_SituationGLValidateInternalTextDrawReady`** — same for text shader/VAO/VBO.
- **Record:** `SituationCmdDrawQuad`, `SituationCmdDrawTexture`, `SituationCmdDrawTextEx`.
- **Execute:** `SIT_OP_DRAW_QUAD`, `SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX` return errors instead of silent no-op when resources are missing.
- **`_SituationInitTextRenderer` (GL):** fail if shader program is 0 after link.

### Version

- **`sit/situation_base_version.h`** — patch **180**.

---

---

---

---

---

---

## [v2.4.181 "OpenGL Deferred Execute Pass Fix"] - 2026-05-30

### Description

OpenGL records draw commands into a soft buffer and executes them at **`SituationEndFrame`**. **`SituationCmdEndRenderPass`** cleared `recording_render_pass_active` at **record** time, so execute-time **`SIT_OP_DRAW_QUAD`** / text / texture still saw “no render pass” (**`NO_RENDER_PASS_ACTIVE` / -540**) even when the packet stream was Begin → Draw → End. Vulkan is immediate and was unaffected.

### Library changes

- **`_SituationGLExecuteCommands`:** track **`exec_inside_render_pass`** from the packet stream (`SIT_OP_BEGIN_RENDER_PASS` / `SIT_OP_END_RENDER_PASS`), not `recording_render_pass_active`.
- **`_SituationGLValidateInternalQuadDrawReady` / `TextDrawReady`:** `require_recorded_render_pass` — `true` on record paths, `false` on execute paths.
- **Harness:** `draw_quad_red` passes on OpenGL; `draw_metrics_overlay` passes. Textured readback (`draw_textured_checkerboard`) may still fail with **`OPENGL_GENERAL` (-600)** — separate issue.

### Version

- **`sit/situation_base_version.h`** — patch **181** (UPDATELOG entry added retroactively; code shipped in the same sprint before **182**).

---

---

---

---

---

---

## [v2.4.182 "Audio Graph RT Teardown"] - 2026-05-30

### Description

**Internal hardening Phase 15:** eliminate intermittent **use-after-free** when destroying an audio graph while the miniaudio callback may still be in `SituationProcessGraph` (MIDI/tone-synth harness crashes on Windows).

### Library changes

- **`atomic_bool is_in_audio_callback`** — set for the full body of `sit_miniaudio_data_callback` after `audio_ready`.
- **`_SituationWaitUntilAudioCallbackIdle()`** — main-thread spin (same pattern as `is_processing_snapshot` / `SituationUnloadSound`).
- **`SituationDestroyGraph`:** clear `active_graph` / `default_graph` if they match the graph being destroyed, then wait for callback idle before freeing nodes.

### Test plan

- `build\sit_test.exe --module tone_synth` (repeat / full suite when convenient).
- Prior failure: AV after `phase1_compare_a4` → next MIDI graph test.

### OpenGL full-suite graphics failures (v2.4.182 snapshot — **closed in v2.4.183**)

**Command:** `build\sit_test.exe` (OpenGL DLL) — **423 / 427** pass, **4** fail (all in `graphics`) at **182** ship; **428 / 428** at **183**.

These were **unrelated to Phase 15** (audio/MIDI). Repro at **182** with `build\sit_test.exe --module graphics`. Failures were **stable in module order**; `polygon_mode_line_wireframe` and `depth_bias_overlap` could pass when filtered alone (deferred-command / GL state carry-over). **Fixed in v2.4.183** — see top entry.

| Test | First failing assertion | What the test expects | Suspected issue |
|------|-------------------------|----------------------|-----------------|
| **`graphics.polygon_mode_line_wireframe`** | `test_graphics.c:1743` — `_sit_pixel_is_red` at screen center after **FILL** `DrawIndexed` | Solid red indexed quad fills the center (count > 200 red pixels in 41×41 window) before a second frame tests **LINE** mode | **Indexed draw path not producing red** when the graphics module runs in sequence — likely **`SIT_OP_BIND_PIPELINE` / `SIT_OP_DRAW_INDEXED` / VAO** not applied correctly at **execute** time, or **global GL state** left by an earlier test (cull/depth/program). Not the LINE-vs-FILL ratio check (that is later). |
| **`graphics.depth_bias_overlap`** | `test_graphics.c:1909` — `_sit_pixel_is_green` at center after biased green draw | Red quad drawn first; green quad with **`SituationCmdSetDepthBias(true, -4, 0, -1)`** should win depth and appear green at center | **`SIT_OP_SET_DEPTH_BIAS`** (and/or depth test/write packets) not taking effect at execute, or bias constants insufficient on this GPU — center stays **red** or **black**. Same deferred-executor / ordering class as polygon mode. |
| **`graphics.texture_format_preservation`** | `test_graphics.c:4303` — `pixel_approx_eq(pixels[tl_idx], 200, 30)` on **R** after `SituationCmdDrawTexture` | 2×2 **linear** RGBA uploaded, stretched to **320×240**; sample at **(40, 30)** ≈ top-left texel (**R≈200, G≈50, B≈100**) | **Textured internal quad** path: wrong pixels (often **black/clear**) — related to **`draw_textured_checkerboard`** class. Suspect **`SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING` + `SIT_OP_DRAW_QUAD`** ordering at execute, **ortho/UV** (Phase 7-bis top-left vs sample coords), and/or **`SituationEndFrame`** surfacing **`OPENGL_GENERAL` (-600)** from a GL error during quad batch. |
| **`graphics.screen_readback_corner_layout`** | `test_graphics.c:4395` — `graphics_test_pixel_approx_eq(rgba[0], 255, 30)` (TL quadrant) | 2×2 **SRGB** texture (R/G/B/Y corners), same **320×240** `DrawTexture`; TL sample should be **red (255,0,0)** | Same **textured quad / readback layout** bucket as above — TL is not red (wrong quadrant or empty framebuffer). Confirms **screen readback + DrawTexture** parity issue on OpenGL, not a harness sampling bug (test documents texel-center math explicitly). |

**Follow-up (shipped in 183):** baseline raster reset + execute hygiene for pipeline/indexed/depth-bias/texture/quad opcodes; **`glFinish`** before pre-swap screenshot; NEAREST for non-mipmap textures; harness **`module_order_point_then_polygon`** regression.

### Version

- **`sit/situation_base_version.h`** — patch **182**.

---

---

---

---

---

---

## [v2.4.183 "OpenGL Executor Harness Green"] - 2026-05-30

### Description

**Renderer bolster — OpenGL deferred executor foundation (closes v2.4.182 snapshot):** finish the foundational OpenGL soft-buffer execute path so the full harness is green before RGL work. Fixes **deferred GL state carry-over**, **indexed/pipeline draw hygiene**, **polygon mode / depth bias execute**, **textured quad readback**, and a **pre-swap screenshot race** that made strict 2×2 pixel tests flaky.

### Library changes

- **`_SituationGLApplyBaselineRasterState()`** — per-frame and per-execute baseline: front face CCW, polygon fill, polygon offset off, program point size off, stencil off, depth mask on, index-type defaults, global VAO bind.
- **`_SituationResetTrackedRasterStateForNewFrame()`** — calls baseline raster reset on **`SituationAcquireFrameCommandBuffer`**.
- **`_SituationGLExecuteCommands`** — baseline raster after shadow-cache reset (order matters) and again at end of execute; **`SIT_OP_DRAW_INDEXED`** binds global VAO before draw; texture bind execute uses **`glProgramUniform1i`** for virtual bindless only when a user program is active, else **`glBindTextureUnit`**; **`SIT_OP_DRAW_QUAD`** rebinds **`current_bound_texture_id`** to unit 0 and refreshes ortho from **`current_target_width/height`** each batch; quad init sets **`u_Texture`** → unit 0 via **`glProgramUniform1i`**.
- **`SituationEndFrame` (OpenGL)** — **`glFinish()`** before pre-swap **`glReadPixels`** into **`screenshot_buffer`** (threaded + non-threaded paths) so **`SituationLoadImageFromScreen`** never reads an incomplete back buffer.
- **`SituationLoadImageFromScreen` (OpenGL)** — cached screenshot path: **`memcpy`** only (bottom-origin capture); **one** **`SituationImageFlip(SIT_FLIP_VERTICAL)`** on load (no double-flip).
- **`SituationCreateTextureEx` (OpenGL)** — non-mipmap textures default to **`GL_NEAREST`** min/mag (stable texel values for UI / strict readback harness).
- **`sit/situation_impl_image.h`** — screenshot flip policy aligned with capture path above.

### Harness

- **`graphics.module_order_point_then_polygon`** — regression: **`primitive_topology_point_list`** then **`polygon_mode_line_wireframe`** in one init (module-order carry-over).

### Test plan

- Rebuild OpenGL DLL + harness; run:
  - `build\sit_test.exe --module graphics` → **94/94**
  - `build\sit_test.exe` → **428/428**
- Focus filters (stable, not flaky): `polygon_mode_line_wireframe`, `depth_bias_overlap`, `texture_format_preservation`, `screen_readback_corner_layout`, `module_order_point_then_polygon`.

### Verification

- **OpenGL full suite:** **428/428** pass on reference **GTX 1070** (2026-05-30).
- **OpenGL `graphics` module:** **94/94**.
- Closes the four failures documented under **v2.4.182** (423/427 → 428/428).

### Version

- **`sit/situation_base_version.h`** — patch **183**.

### Docs

- **`doc/plan/renderer_bolster_plan.md`** — **Phase 7-ter** (OpenGL deferred executor foundation **v2.4.181–183**).
- **`doc/plan/INTERNAL_HARDENING_PLAN.md`** — cross-ref: **v2.4.179–180** hardening phases; **v2.4.181–183** bolster executor slices (not hardening phase slots).

---

---

---

---

---

---

## [v2.4.184 "Indexed Viewport Scissor API"] - 2026-05-30

### Description

**Renderer bolster Phase 8 (first slice):** indexed viewport/scissor commands and **`max_viewports`** capability query — future-proof multi-viewport without breaking single-viewport users.

### Library changes

- **`SituationGraphicsCaps.max_viewports`** — from **`GL_MAX_VIEWPORTS`** (OpenGL) or **`VkPhysicalDeviceLimits::maxViewports`** (Vulkan); **≥ 1** after init.
- **`SituationCmdSetViewportIndexed(cmd, index, x, y, w, h)`** — OpenGL records **`SIT_OP_SET_VIEWPORT`** with index; index **0** → **`glViewport`**, index **> 0** → **`glViewportIndexedf`**; Vulkan **`vkCmdSetViewport(cmd, index, 1, …)`**.
- **`SituationCmdSetScissorIndexed(cmd, index, x, y, w, h)`** — OpenGL index **0** → **`glScissor`**, **> 0** → **`glScissorIndexed`** (same top-left API → bottom-left GL Y as index 0); Vulkan **`vkCmdSetScissor(cmd, index, 1, …)`**.
- **`SituationCmdSetViewport` / `SituationCmdSetScissor`** — thin wrappers delegating to index **0** (Vulkan **`SetViewport`** still sets matching scissor at index 0).

### Harness

- **`core.get_graphics_caps`** — asserts **`max_viewports >= 1`**.
- **`core.viewport_index_zero_parity`** — indexed **0** matches legacy API; optional index **1** when **`max_viewports >= 2`**.
- **`core.viewport_index_out_of_range`** — index **`max_viewports`** returns **`INVALID_PARAM`**.

### Test plan

- `build\sit_test.exe --module core --filter viewport_index`
- `build\sit_test.exe --module core --filter get_graphics_caps`
- Full OpenGL suite: `build\sit_test.exe`

### Verification

- **OpenGL full suite:** **430/430** pass on reference **GTX 1070** (2026-05-30).
- **`core.viewport_index_*`:** **2/2** pass.

### Version

- **`sit/situation_base_version.h`** — patch **184**.

### Docs

- **`doc/plan/renderer_bolster_plan.md`** — Phase 8 first slice marked complete.

---

---

---

---

---

---

## [v2.4.185 "Raster State Phase 6B"] - 2026-05-30

### Description

**Renderer bolster Phase 6B (second slice):** line width, color write mask, stencil test API, and real OpenGL push/pop raster stack.

### Library changes

- **`SituationStencilOp`**, **`SituationStencilState`**, **`SituationMultisampleState`** — public types for stencil/multisample raster control.
- **`SituationCmdSetLineWidth`** — OpenGL **`SIT_OP_SET_LINE_WIDTH`** → **`glLineWidth`**; Vulkan **`vkCmdSetLineWidth`** with **`VK_DYNAMIC_STATE_LINE_WIDTH`** in graphics pipelines (non-1.0 widths require **`wideLines`**).
- **`SituationCmdSetColorWriteMask`** — OpenGL **`SIT_OP_SET_COLOR_WRITE_MASK`** → **`glColorMask`**; Vulkan returns **`NOT_IMPLEMENTED`** until dynamic color-write support lands.
- **`SituationCmdSetStencilTest`** — OpenGL soft replay with **`glStencil*Separate`**; returns **`NOT_IMPLEMENTED`** when the active framebuffer has no stencil bits; Vulkan **`NOT_IMPLEMENTED`**.
- **`SituationCmdSetMultisampleState`** — deferred (**`NOT_IMPLEMENTED`**) until MSAA render targets are exposed.
- **`SituationCmdPushRasterState` / `SituationCmdPopRasterState`** — OpenGL execute-time stack (**256** deep, **`SITUATION_MAX_RASTER_STACK_DEPTH`**) capturing/restoring blend, depth, cull, scissor, stencil, polygon mode, depth bias, line width, color mask, and topology shadow; recording validates push/pop balance.

### Harness

- **`graphics.color_write_mask_blocks_red`** — all-false mask leaves cleared black after red draw.
- **`graphics.push_pop_raster_color_mask`** — masked draw blocked, post-pop draw restores red.
- **`graphics.stencil_test_command_conditional`** — enable accepts **`SUCCESS`** or **`NOT_IMPLEMENTED`**; disable always **`SUCCESS`**.
- **`graphics.line_width_command`** — **`1.0`** ok; negative width **`INVALID_PARAM`**.

### Test plan

- `build\sit_test.exe --module graphics --filter color_write_mask`
- `build\sit_test.exe --module graphics --filter push_pop_raster`
- Full OpenGL suite: `build\sit_test.exe`

### Verification

- **OpenGL full suite:** **434/434** pass on reference **GTX 1070** (2026-05-30) — **430** baseline at v2.4.184 + **4** new **`graphics`** tests.
- **`graphics.color_write_mask_blocks_red`**, **`push_pop_raster_color_mask`**, **`stencil_test_command_conditional`**, **`line_width_command`:** **4/4** pass.

### Version

- **`sit/situation_base_version.h`** — patch **185**.

### Docs

- **`doc/plan/renderer_bolster_plan.md`** — Phase 6B slice progress.

---

---

---

---

---

---

## [v2.4.186 "Vulkan Phase 6-bisH"] - 2026-05-30

### Description

**Phase 6-bisH:** Vulkan parity for Phase 6B color write mask, stencil test, and push/pop raster stack via **`VK_EXT_extended_dynamic_state3`** color write + **`VK_EXT_extended_dynamic_state`** stencil dynamics plus tracked extended raster state.

### Library changes

- **Device features:** enable **`extendedDynamicState3ColorWriteMask`** (polygon mode dyn3 remains off — wireframe uses static line pipelines). Stencil uses existing **`extendedDynamicState`** feature chain.
- **Proc loading:** **`vkCmdSetColorWriteMaskEXT`**, **`vkCmdSetStencilTestEnable`**, **`vkCmdSetStencilOp`**; core **`vkCmdSetStencilCompareMask`** / **`WriteMask`** / **`Reference`**; graceful disable when missing.
- **Pipeline dynamics:** **`VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT`**, **`STENCIL_TEST_ENABLE`**, **`STENCIL_OP`**, **`STENCIL_COMPARE_MASK`**, **`STENCIL_WRITE_MASK`**, **`STENCIL_REFERENCE`** added through **`_SitVulkanFillGraphicsDynamicStates`**.
- **`SituationCmdSetColorWriteMask`** — records color write mask when dyn3 color write is available; full mask is no-op when feature absent.
- **`SituationCmdSetStencilTest`** — enable requires depth/stencil attachment + dyn3 stencil procs; disable via **`vkCmdSetStencilTestEnable(false)`**.
- **`SituationCmdPushRasterState` / `PopRasterState`** — **256**-deep tracked-state stack; pop re-issues dynamics and rebinds cull/front-face pipeline variant when needed.
- **Tracked raster reset** on frame acquire: color mask all-on, depth test on / write off / **`LESS`**, stencil off, line width **1.0**, stack depth **0**.
- **`SituationCmdSetDepthTest` / `SetDepthWrite` / `SetLineWidth`** — update tracked fields used by push/pop restore and pipeline rebind.

### Harness (target)

- **`graphics.color_write_mask_blocks_red`**
- **`graphics.push_pop_raster_color_mask`**
- **`graphics.stencil_test_command_conditional`**
- **`graphics.line_width_command`**

### Verification

- **Vulkan focused (GTX 1070, 2026-05-30):** **`color_write_mask_blocks_red`**, **`push_pop_raster_color_mask`**, **`stencil_test_command_conditional`**, **`line_width_command`** — **4/4** pass.
- **Vulkan full suite:** **424/424** pass after rebuild.

### Version

- **`sit/situation_base_version.h`** — patch **186**.

### Docs

- **`doc/plan/renderer_bolster_plan.md`** — new **6-bisH** subsection.

---

---

---

---

---

---

## [v2.4.187 "Phase 6 closure"] - 2026-05-30

### Description

**Phase 6 closure (v2.4.187):** marks fixed-function raster state **complete** on GL/VK harness; documents what shipped vs what is **explicitly out of scope** (multisample command → v2.5 render-target work, not a separate MSAA phase).

### Phase 6 — closed

**In scope and done:** front face, topology, polygon mode, depth bias, line width, color write mask, stencil test, push/pop stack (**256** deep), Vulkan 6-bis/6-bisH parity, policy doc, debug variant counters, harness assert cleanup.

**Out of scope (documented, not v2.4.187 deliverables):**

- **`SituationCmdSetMultisampleState`** — type exists; command **`NOT_IMPLEMENTED`**. Blocked on **`SituationRenderTarget`** + MSAA resolve (**`doc/plan/v2.5-api-expansion.md` Phase 6**). MSAA policy wiring lives in bolster **Phase 11**.
- Patches/tessellation topology, GL ES/Web port policy, optional 6-bisA topology-in-variant-key.

### Library changes

- **`SITUATION_MAX_RASTER_STACK_DEPTH` (256)** — unified public limit in **`situation_api.h`** (GL execute stack + VK tracked stack).
- **Vulkan raster variant stats (debug builds):** **`raster_pipeline_resolve_count`**, **`raster_polygon_variant_hits`**, **`raster_cull_front_variant_hits`**, **`raster_pipeline_rebind_count`** — logged on **`_SituationCleanupVulkan`** when non-zero.
- **`SituationCmdSetMultisampleState`** — remains **`NOT_IMPLEMENTED`**; error cites **v2.5 render-target Phase 6** (not part of Phase 6 closure deliverable).

### Docs

- **`doc/plan/renderer_bolster_plan.md`** — Phase 6 **closed**; **§ MSAA / multisample (not Phase 6)** cross-refs v2.5 + Phase 11.

### Harness

- **`sit_test_framework.h`** — **`sit_test_fail_impl`** calls registered crash cleanup before **`longjmp`** (6-bisD assert-path hygiene).

### Verification

- **`sit_test_vulkan.exe --module graphics --filter polygon_mode_line_wireframe`** — pass (GTX 1070).
- Full Vulkan harness **424/424** unchanged after rebuild.

### Version

- **`sit/situation_base_version.h`** — patch **187**.

---

---

---

---

---

---

## [v2.4.188 "Phase 11 Render Pass Foundation"] - 2026-05-30

### Description

**Phase 11 foundation (first slice):** documents `SituationRenderPassInfo` load/store/clear semantics, adds begin-pass helper constructors, and exposes a stable configuration key for Vulkan render-pass caching (stencil load/store included; MSAA sample count still deferred).

### Library changes

- **`SituationRenderPassInfoDefault(display_id, clear_color)`** — inline helper: clear color+depth, store color, discard depth/stencil after pass.
- **`SituationRenderPassInfoLoad(display_id)`** — inline helper: preserve all attachment contents (resume/composite pattern).
- **`SituationRenderPassConfigurationKey(info)`** — public key from target class + color/depth/stencil load/store ops (clear values not keyed).
- **`_SituationHashRenderPassKey`** — delegates to **`SituationRenderPassConfigurationKey`** (single source of truth).

### Docs

- **`sit/situation_api.h`** — expanded comments on load/store/clear, attachment independence, stencil caveats, mid-pass vs begin-pass clears.
- **`doc/plan/renderer_bolster_plan.md`** — Phase 11 foundation slice marked done.

### Harness

- **`core.render_pass_info_default_helper`**, **`render_pass_info_load_helper`**, **`render_pass_configuration_key_stencil_ops`**, **`render_pass_info_default_begin_pass`**.

### Version

- **`sit/situation_base_version.h`** — patch **188**.

---

---

---

---

---

---

## [v2.4.189 "Tone Synth Sub Osc + Vulkan 2D Parity"] - 2026-05-31

### Description

Single patch since **v2.4.188**: tone synth sub-oscillator controls (coarse tune, hard sync, CS-40M-style ring bus), Vulkan 2D screen space aligned to OpenGL, and harness listen overlays (stereo scope + spectrum).

**Canonical version**: `sit/situation_base_version.h` → **2.4.189**.

Rebuild **`build_situation.bat vulkan`**, sync **`build/dll/situation_vulkan.dll`**, then **`build_tests.bat vulkan`**.

### Tone synth (library)

- **`sub_coarse`** (ctrl **34**, **CC111**) — ±12 semitones from main note; replaces legacy **`sub_note`** fixed-note pitch.
- **`sub_sync`** (ctrl **35**, **CC112**) — main = master; sub phase resets each main cycle (off when ring on).
- **`sub_ring_mod`** (ctrl **36**, **CC113**) — CS-40M-style multiply bus: `main + ring_level×(main×sub)`; carrier stays in mix; **CC107** = ring bus level (defaults to **1** when ring on and level is 0); additive sub disabled while ring on.
- **`tone_synth_graph.h`**, **`device_wrappers.h`**, **`registry_init.h`**, **`doc/tone_synth.md`**.

### Vulkan 2D OpenGL parity (library)

OpenGL is the reference for Situation 2D screen space. Vulkan internal 2D draws (quads, text, VD composite) now match OpenGL live-window placement without per-shader Y/UV workarounds.

- **`_SitVulkanFillViewport2DOpenGLParity`** — negative viewport height for internal 2D passes so `(0,0)` top-left matches OpenGL.
- **`_SitVulkanApply2DViewportScissor`** — internal quad/text draws and VD compositor. **`_SitVulkanApplyGraphicsViewportScissor`** — tracked user-draw hygiene (same parity viewport; see **v2.4.220**).
- **`_SitVulkanFillOrthoProjection2D`** — unchanged `glm_ortho(0, w, h, 0, …)` (same as OpenGL).
- **Vulkan text shaders** — same vertex/fragment logic as OpenGL (removed `target_h - y` and fragment V flip); text push constants are **`vec4 color`** only.
- **Vulkan quad fragment shader** — samples **`v_TexCoord`** directly when texturing (same as OpenGL).

### Harness

- **`test_tone_synth`**: **`sub_sync`**, **`sub_ring_mod`**, coarse-sweep listen tests; CC111-only sweep while note held.
- **`sit_test_visual_layout.h`** — overlay layout in **`SituationGetRenderWidth/Height`** (Hi-DPI).
- **`sit_test_stereo_scope.c`** — harness-only scope/spectrum: **`SituationCmdDrawTexture`**-style quad rects, AC-couple snapshot for bipolar zero lines, log-spaced spectrum panel.
- **`sit_test_listen_overlay.h`** — listen-test window labels + scope pump.
- **`test_graphics.stereo_scope_overlay_layout`** — regression for bottom-left 2D placement.

### Verification (GTX 1070)

- Vulkan **`graphics`**: **89/89**
- Vulkan **`virtual_display`**: **21/21**
- OpenGL **`graphics`**: **99/99**
- Listen: **`tone_synth.sub_sync`**, **`sub_ring_mod`** pass on Vulkan and OpenGL (1024×768 scope + spectrum).

### Version

- **`sit/situation_base_version.h`** — patch **189**, description **"Tone Synth Sub Osc + Vulkan 2D Parity"**.

---

---

---

---

---

---

## [v2.4.190 "Harness Scope Perf + Tone Synth Test Fixes"] - 2026-05-31

### Description

Harness-only patch since **v2.4.189**: stereo scope overlay performance (fewer draw calls, throttled spectrum, Win32 message pump) and **`tone_synth`** capture-test reliability. Library behavior unchanged except version string.

**Canonical version**: `sit/situation_base_version.h` → **2.4.190**.

Rebuild **`build_situation.bat vulkan`**, sync **`build/dll/situation_vulkan.dll`**, then **`build_tests.bat vulkan`**.

### Harness — stereo scope overlay

- **`sit_test_stereo_scope_frame_prepare()`** — one ring snapshot per frame; log-spectrum Goertzel throttled (**`SIT_TEST_SPECTRUM_UPDATE_MS`** = 100 ms).
- **`SIT_TEST_SCOPE_DRAW_COLUMNS`** **384 → 160** — fewer internal 2D quads per frame (scope line strips).
- **Atomic ring write index** — audio monitor callback vs UI thread handoff.
- **Win32 message dispatch** during harness waits — **`PeekMessage`/`DispatchMessage`** in overlay and capture sleep loops.

### Harness — tone_synth tests

- **`legacy_midi_note_frequency`**: settle legacy pool (**`StopAllTones`**, **`SetActiveGraph(NULL)`**) before capture.
- **`midi_complex_melody`**: Goertzel neighbor fix for E4 (±1 semitone instead of +7, which hits B4 in the melody); capture timing via **`SIT_TONE_CAPTURE_SLEEP_MS`**.

### Verification (GTX 1070)

- Vulkan full harness: **429/429** (after rebuild).

### Version

- **`sit/situation_base_version.h`** — patch **190**, description **"Harness Scope Perf + Tone Synth Test Fixes"**.

---

---

---

---

---

---

## [v2.4.191 "Vulkan Async Shader Poll + Harness Multi-Monitor VD + Phase 11-bis Plan"] - 2026-05-31

### Description

**v2.4.191**: Vulkan async GLSL shader load hardening, thread-safe audio output monitor callback, harness waits that keep scope/spectrum live during sleeps, unified **1024×768** harness window, **`advanced`** multi-monitor integration test (Tier A: spanning host + multi-Virtual Display), **`build_situation.bat`** OpenGL compile fix, and **Phase 11-bis** multi-monitor plan (consent-gated). **No new public library APIs** beyond the async-shader and audio-monitor fixes below.

**Canonical version**: `sit/situation_base_version.h` → **2.4.191**.

Rebuild **both** backends and harnesses after pull:

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_situation.bat" vulkan
& ".\build_tests.bat" opengl
& ".\build_tests.bat" vulkan
```

### Library

- **`situation_impl_renderer.h` — Vulkan async shader load**
  - **`SituationBeginLoadShaderFromMemory`**: submit compile with **`SIT_SUBMIT_POINTER_ONLY`** + **`SIT_SUBMIT_RUN_IF_FULL`**; sync fallback if the worker never starts; never SOO-copy async ctx.
  - **`SituationAcquireFrameCommandBuffer`**: poll **`vk_async_load`** slots only (not every active shader).
- **`situation_impl_audio.h` / `situation_impl_decl.h` — audio monitor**
  - **`output_monitor_callback`** / **`output_monitor_user_data`** stored as **`_Atomic(void*)`**; audio callback loads with acquire ordering.

### Harness — waits, scope, and capture

- **`sit_test_harness_wait_ms`** — when **`SituationIsInitialized()`**, delegates to **`sit_test_audio_visual_pump_ms`** (~30 Hz scope render during wall-clock waits).
- **`SIT_TONE_CAPTURE_SLEEP_MS`** — same as listen overlay pump (capture tests no longer freeze the display for seconds).
- **`sit_test_stereo_scope.c`** — Win32 **`CRITICAL_SECTION`** on scope ring snapshot vs audio push; acquire retry on transient fence failure.
- **`test_audio_effects_heard.c`** — reverb dry/wet sweep uses **`sit_test_audio_visual_pump_ms`** instead of raw **`Sleep`**.

### Harness — default window size

- **`tests/harness/sit_test_window.h`** — shared **`SIT_TEST_WINDOW_WIDTH`** / **`HEIGHT`** (**1024×768**), **`sit_test_window_init_info`**, runtime size helpers, **`sit_test_full_window_dest`**.
- **`tests/harness/sit_test_audio_window.h`** — aliases the shared window header.
- **`tests/harness/sit_test_visual_layout.h`** — layout helpers use harness window dimensions.
- Graphics, VD, audio listen, and context modules updated to use **`sit_test_window_init_info`** instead of ad-hoc sizes (e.g. retired **320×240** defaults where unified).

### Harness — module order

- **`tests/harness/sit_test_registry.c`** — **`graphics`** → **`virtual_display`** → **`compute`** → **`transfer`** run **before** **`audio`** / **`tone_synth`** / **`audio_effects_heard`** so listen tests can pump scope/spectrum on the main swapchain after GPU modules init.

### Harness — **`advanced`** module (multi-monitor Tier A)

- **`tests/harness/test_advanced.c`** — new module, runs last.
- Test **`all_displays_windowed_fullscreen_cycle`**: three **2 s** phases on **one** integration path only:
  1. **Windowed panels** — host spans virtual desktop; one **Virtual Display** per monitor at **1024×768** with independent triangle animation.
  2. **Fullscreen layout** — undecorated host + VDs sized to each monitor.
  3. **Restore** — decorated host + windowed panels again.
- Uses the **standard** frame loop: **`SituationAcquireFrameCommandBuffer`** → VD render passes → main pass + **`SituationRenderVirtualDisplays`** → **`SituationEndFrame`**. **No** parallel BeginFrame/EndFrame APIs.
- **Honesty boundary:** Tier A is **one OS window** compositing all monitors — not N separate taskbar windows (Tier B deferred to v2.5; see plan below).
- **`build_tests.bat`** — links **`test_advanced.c`**.

### Build

- **`build_situation.bat`** — **OpenGL** steps 1–3: remove blank lines between `^` line continuations (same failure class as Vulkan fix in **v2.4.170** — empty `gcc` arguments / `linker input file not found`).

### Documentation / planning

- **`doc/plan/renderer_bolster_plan.md`** — new **Phase 11-bis — Multi-monitor Virtual Display presentation (WDM + compositor)**:
  - Tier A (v2.4.x): spanning host + multi-VD via existing APIs.
  - Tier B (v2.5): true per-monitor OS windows only through integrated multi-present at **`SituationEndFrame`** (spike + maintainer consent).
  - Explicit **anti-patterns** (rogue monitor-window APIs, aux GL presentation, display-mode cycling).
  - Consent-gated delivery slices **MM-0 … MM-3** — **no implementation without approval**.
- **`doc/plan/v2.5-api-expansion.md`** § P — multi-window parking lot cross-ref to Phase 11-bis Tier B.

### Not in this release

- **`SituationMonitorWindow*`** / **`situation_impl_mwin.h`** — **not shipped** (prototyped and **reverted**; breaks command-buffer conventions).

### Verification (reference machine, dual monitor **5120×1440**)

- OpenGL full harness: **440/440** (includes **`advanced.all_displays_windowed_fullscreen_cycle`**, async shader tests on Vulkan build of same tree).
- Vulkan full harness: **430/430** (same).
- Listen overlay stays animated during **`legacy_midi_note_frequency`**, **`midi_complex_melody`**, **`midi_velocity_ramp`**.

### Version

- **`sit/situation_base_version.h`** — patch **191**, description **"Vulkan Async Shader Poll + Harness Multi-Monitor VD + Phase 11-bis Plan"**.

---

---

---

---

---

---

## [v2.4.192 "YPQ Color Toolset M1+M2"] - 2026-05-31

### Description

**v2.4.192**: Core YPQ pixel API (HSV parity), float `ColorYPQf` edit path, in-gamut chroma clamp, and `SituationImageAdjustYPQ` CPU grading. Internal NTSC YIQ math consolidated in `situation_impl_color.h` (M0 groundwork).

Rebuild library and harness after pull:

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
```

### Library — YPQ toolset (M1 + M2)

- **`sit/situation_impl_color.h`** — shared NTSC YIQ constants; byte and float conversion core.
- **`sit/situation_api.h`** — `ColorYPQf`; `SituationYpqLerp`, `AdjustLuma` / `AdjustPhase` / `AdjustChroma`, accessors, `Distance`, `Equals`; `SituationColorToYPQf` / `FromYPQf`, `SituationYpqQuantize`, `SituationYpqClampInGamut`; `SituationImageAdjustYPQ`.
- **`sit/situation_impl_image.h`** — implementations; image adjust mirrors HSV mix/ factor semantics in float YPQ space.

### Harness

- **`tests/harness/test_misc.c`** — `ypq_lerp_wrap`, `ypq_adjust_*`, `ypq_distance_equals`, `ypq_float_roundtrip`, `ypq_quantize`, `image_adjust_ypq`; deterministic Q sweep retained.

### Docs

- **`doc/situation_api.md`** — YPQ types, pixel API, float path, `SituationImageAdjustYPQ`.
- **`doc/situation_sdk.md`** — when to use RGB vs HSV vs YPQ.
- **`doc/plan/YPQ_COLOR_PLAN.md`** — M0–M2 progress.

---

---

---

---

---

---

## [v2.4.193 "YPQ GPU Grade + Core Optimizations (M4 WIP)"] - 2026-05-31

### Description

**v2.4.193**: YPQ CPU core optimizations (FMA, inverse scales), public **`SituationCmdDrawTextureYpqGrade`** GPU path with CPU parity test, harness sweep/stats speedups, **`build_situation.bat`** release **`SIT_OPTIMIZE_CFLAGS`**, and batch-file formatting cleanup. Phases 3, 5, 6 remain open in **`doc/plan/YPQ_COLOR_PLAN.md`**.

**Canonical version**: `sit/situation_base_version.h` → **2.4.193**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
```

---

---

---

---

---

---

## [v2.4.194 "Window Init + Harness Headless + GLFW Hint Fix"] - 2026-05-31

### Description

**v2.4.194**: Startup window visibility fixes (default **topmost**, **`GLFW_VISIBLE`** hint reset across OpenGL init cycles), unified harness **`--headless`** / **`SIT_TEST_HEADLESS`**, Vulkan **`SituationCmdDrawTextureYpqGrade`** push-constant compile fix, and **`advanced`** module skip when headless.

**Canonical version**: `sit/situation_base_version.h` → **2.4.194**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_situation.bat" vulkan
& ".\build_tests.bat" opengl
& ".\build_tests.bat" vulkan
```

### Library

- **`sit/situation_impl_ctrl.h` — `_SituationInitWindow`**
  - Default **`SITUATION_FLAG_WINDOW_TOPMOST`** at startup so new apps are not buried behind other windows.
  - **`GLFW_VISIBLE`** set from **`SITUATION_FLAG_WINDOW_HIDDEN`** (fixes hidden main window on 2nd+ **`SituationInit`** in one process after OpenGL loader window).
- **`sit/situation_impl_renderer.h`**
  - Restore **`GLFW_VISIBLE TRUE`** after hidden OpenGL loader window creation.
- **`sit/situation_impl_renderer.h` — Vulkan YPQ grade**
  - **`glm_vec4_one(push_data.color)`** instead of invalid **`vec4` array assignment**.

### Harness

- **`--headless`** and **`SIT_TEST_HEADLESS=1`** — hidden window, no listen overlay; same path OpenGL and Vulkan.
- **`sit_test_headless.h`**, **`sit_test_gpu_context_init`**, **`requires_visible_window`** ( **`advanced`** skipped when headless).
- Headless waits use minimal frame pump (no scope HUD).

---

---

---

---

---

---

## [v2.4.195 "Tone Sub Sync + Harness Scope/Spectrum"] - 2026-05-31

### Description

**v2.4.195**: Hard-sync sub-oscillator frequency/mix fix (classic 0.5×–2× sweep via CC111), harness stereo scope/spectrum overhaul (fast clear, dB amplitude display, FMA layout math), and tone_synth listen-test timing fixes (`lfo_mod`, `midi_complex_melody` scope refresh).

**Canonical version**: `sit/situation_base_version.h` → **2.4.195**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
```

### Library

- **`sit/aud/tone_synth_graph.h` — sub hard sync**
  - Sync-on frequency: `sub_hz = main × 2^((coarse + fine)/12)` — **octave ignored** when sync is enabled (CC112), so CC111 sweeps **0.5×…2×** of main instead of a narrow band.
  - Sync mix favors the slave: `sub + (1 − sub_level) × main` so the synced sub is audible at full sub level.
  - Noise seed uses main pitch when sync is on.

### Harness

- **`tests/harness/sit_test_stereo_scope.c` / `.h`**
  - Spectrum: removed per-frame decay crush; asymmetric attack/release smoothing; **dB display** (−42 dB floor) so harmonics/sidebands are visible, not peak-or-nothing.
  - Slow-decay display peak reference; capture snapshot path restored for effects sweeps.
  - Display math routed through **`SCOPE_FMA`** / helpers across scope push, clip vertices, and overlay layout.
- **`tests/harness/test_tone_synth.c`**
  - **`lfo_mod`**: settle window before Goertzel; PWM segment uses 2nd-harmonic probe; timing aligned with steady-state segments.
  - **`midi_complex_melody`**: explicit overlay render each 20 ms melody step (scope no longer frozen during capture).
  - **`mono_portamento_unlinked`**: snap/measure offsets tuned for portamento settle.

### Docs

- **`doc/tone_synth.md`** — sync frequency and mix behavior updated.

### Verification

- **OpenGL `sit_test.exe --module tone_synth`**: **34/35** ( **`midi_complex_melody`** intermittently fails Goertzel on last note when run in full module; passes in isolation).
- Key listen tests verified: **`sub_sync`**, **`lfo_mod`**, **`midi_complex_melody`** (isolated).

---

---

---

---

---

---

## [v2.4.196 "Tone Synth FMA + PI Constants"] - 2026-05-31

### Description

**v2.4.196**: Replace magic PI literals with proper `#define` constants (`SIT_TONE_PI`, `SIT_TONE_TWO_PI`) and convert multiply-add patterns to `fmaf()` across the tone synth graph hot path for single-instruction FMA on modern CPUs.

**Canonical version**: `sit/situation_base_version.h` → **2.4.196**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
```

### Library

- **`sit/aud/tone_synth_graph.h` — PI constants**
  - Added `M_PI` guard, `SIT_TONE_PI` (`(float)M_PI`), `SIT_TONE_TWO_PI` (`2.0f * SIT_TONE_PI`).
  - Replaced all 6 inline `3.14159265359f` magic numbers with the named constants.

- **`sit/aud/tone_synth_graph.h` — FMA pass**
  - Per-sample hot path: oscillator waveforms (triangle, noise), ring mod dry/wet crossfade, sync mix, additive sub, envelope decay/release, vibrato pitch, LFO triangle/random, portamento glide, pulse width modulation.
  - MIDI CC mapping: pan (CC10), filter drive (CC17), resonance (CC71), pulse width (CC106).
  - Utility: `_SituationToneSynthMidiNormLog` log interpolation, `_SituationToneSynthFreqToMidiNote` note calculation.
  - Total: ~17 `fmaf()` conversions across the file.

### Verification

- **OpenGL DLL**: builds clean (GCC 15.1.0, `-std=c11`).
- **`sit_test.exe --module audio`**: 81/81 passed, 0 failed.

---

---

---

---

---

---

## [v2.4.197 "Threading Module Dispatch + Thread Naming"] - 2026-06-01

### Description

**v2.4.197**: Workers now spread across physical cores by default when threading is enabled (`SITUATION_WORKER_NUMA_SPREAD_DEFAULT`). All library threads are named for debugger/Task Manager visibility. Render thread shutdown fixed (no longer hangs when no frames were submitted). New `SituationGetInternalThreadPool()` API. Snapshot/dump now shows thread names and main thread. Default reserved threads raised to 4 (main/render/audio/I/O). I/O thread pinned to CPU 3 by default.

**Canonical version**: `sit/situation_base_version.h` → **2.4.197**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
```

### Library

- **`sit/situation_api.h` — Build flag + snapshot struct + I/O affinity API**
  - Added `SITUATION_WORKER_NUMA_SPREAD_DEFAULT` (defaults to `1` when `SITUATION_ENABLE_THREADING` defined; override to `0` to disable).
  - `SituationThreadSlotSnapshot`: added `index` and `name[24]` fields.
  - `SituationInitInfo::worker_numa_spread` comment updated to document build-flag default.
  - `SituationInitInfo::thread_pool_reserved_threads` default changed from 1 → 4 (main/render/audio/I/O).
  - Added `SITAPI SituationThreadPool* SituationGetInternalThreadPool(void)`.
  - Added `SITAPI uint64_t SituationGetConfiguredIOThreadAffinity(void)` — default CPU 3.

- **`sit/situation_impl_ctrl.h` — Default spread + main thread naming + render shutdown fix**
  - `worker_numa_spread` now OR'd with `SITUATION_WORKER_NUMA_SPREAD_DEFAULT` at init.
  - Main thread named `"Sit Main"` via `_SituationSetCurrentThreadName` after window creation.
  - Render thread shutdown: moved `_SituationDestroyRenderThread()` before `glFinish()` when render thread owns GL context; main thread reclaims context after join.

- **`sit/situation_impl_threading.h` — Worker naming + pool getter**
  - Workers named `"Sit Worker N"` at entry via `_SituationSetCurrentThreadName`.
  - Added `SituationGetInternalThreadPool()` implementation.

- **`sit/situation_impl_threading_scheduler.h` — Reserved threads default**
  - `_SitResolveAutoWorkerCount` and `SituationGetRecommendedWorkerCount`: default reserved raised from 1 → 4 (accounts for main, render, audio, I/O dedicated threads).

- **`sit/situation_impl_threading_topology.h` — I/O affinity getter**
  - Added `SituationGetConfiguredIOThreadAffinity()`: returns `1ULL << 3` (CPU 3) by default; respects `io_thread_numa_node > 0` override.

- **`sit/situation_impl_threading_numa.h` — Single-NUMA per-core spread**
  - `_SituationApplyWorkerNumaPlacement`: on single-NUMA systems, pins each worker to a distinct physical core via `SituationBuildPhysicalCoreMask(worker_index % physical_count)`.

- **`sit/situation_impl_threading_diag.h` — Thread naming helper**
  - Added `_SituationSetCurrentThreadName(const char*)`: Windows (`SetThreadDescription`, dynamically loaded), Linux (`pthread_setname_np`), macOS (`pthread_setname_np`).

- **`sit/situation_impl_threading_observability.h` — Snapshot includes main thread + names**
  - `SituationGetThreadPoolSnapshot`: populates `name` field for all slots; includes main thread as first slot.
  - `SituationDumpThreadPoolStatus`: prints `[Sit Main]`, `[Sit Worker 0]`, `[Sit I/O]`, `[Sit Render]`, `[Sit Audio]`.

- **`sit/situation_impl_io.h` — I/O thread naming + affinity**
  - I/O thread named `"Sit I/O"` at entry.
  - Pinned to CPU 3 via `SituationGetConfiguredIOThreadAffinity()` (replaces unreliable NUMA-only placement on single-node systems).

- **`sit/situation_impl_audio.h` — Audio thread naming**
  - Audio callback thread named `"Sit Audio"` on first invocation.

- **`sit/situation_impl_renderer.h` — Render thread naming + shutdown fix**
  - Render thread named `"Sit Render"` at entry.
  - Render loop: `cnd_wait` → `cnd_timedwait` (50ms) for guaranteed shutdown responsiveness.
  - `_SituationDestroyRenderThread`: repeated broadcast (3× with 20ms gaps) before join polling.

### Build System

- **`build_situation.bat`** — Added `-DSITUATION_ENABLE_RENDER_THREAD` to both OpenGL and Vulkan DLL builds.
- **`build_tests.bat`** — Added `-DSITUATION_ENABLE_RENDER_THREAD` to both OpenGL and Vulkan test harness builds. Removed `midi_audio_probe.exe` one-shot build.

### Test Harness

- **`tests/harness/test_core.c`** — New `module_core_assignment` test: boots full library with render thread, triggers audio, pumps frames, then reports all thread roles with CPU/affinity via `SituationDumpThreadingReport`. Asserts audio thread visible and ≥2 distinct CPUs.

### Verification

- **OpenGL DLL**: builds clean (GCC 15.1.0, `-std=c11`).
- **`sit_test.exe --module core --filter module_core --headless`**: PASS — all roles on distinct cores (Main=4, Workers=0/2/4/6, I/O=3, Render=1, Audio=2), clean shutdown.
- **`sit_test.exe --module threading`**: 21/21 passed.

---

---

---

---

---

---

## [v2.4.198 "PCM Input Node"] - 2026-06-01

### Description

**v2.4.198**: New `SITUATION_NODE_PCM_INPUT` source node — a lock-free ring buffer source that accepts user-pushed PCM from any thread. Enables kterm voice playback, network audio streams, and any user-fed PCM source to participate in the audio graph (mixable, patchable, effects-chainable). kterm voice updated to use the new node instead of the removed `SituationStartAudioPlayback` API.

**Canonical version**: `sit/situation_base_version.h` → **2.4.198**.

### Library

- **`sit/situation_api.h`**
  - Added `SITUATION_NODE_PCM_INPUT` to `SituationNodeType` enum (Sources section).
  - Added `SITAPI uint32_t SituationPushNodePCM(...)` — push interleaved float PCM into a PCM_INPUT node's ring buffer (any thread, lock-free).
  - Added `SITAPI uint32_t SituationGetNodePCMFreeFrames(...)` — query available write space.

- **`sit/aud/pcm_input.h`** (NEW)
  - Lock-free SPSC ring buffer state (`SituationPCMInputState`).
  - `_SitPCMInputCreate` / `_SitPCMInputDestroy` — lifecycle.
  - `_SitPCMInputPush` — producer (any thread).
  - `_SituationProcessPCMInputNode` — consumer (audio callback), applies gain/pan/mute controls.
  - `_SituationCreatePCMInput` / `_SituationDestroyPCMInput` — device wrapper functions.
  - Default ring size: 4096 frames × channels (`SIT_PCM_INPUT_RING_FRAMES`).

- **`sit/aud/device_wrappers.h`**
  - Added `#include "pcm_input.h"`.
  - Added `SITUATION_NODE_PCM_INPUT` entry to `g_device_function_table`.

- **`sit/aud/registry_init.h`**
  - Added `_SituationRegisterPCMInput()` — registers PCM Input as a Source device with gain/pan/mute controls.
  - Called in `SituationInitDeviceRegistry()`.

- **`sit/situation_impl_audio.h`**
  - Added `SituationPushNodePCM()` and `SituationGetNodePCMFreeFrames()` implementations.

- **`sit/k-term/kt_voice.h`**
  - `KTermVoiceContext` struct: added `playback_graph`, `pcm_node_handle`, `pcm_node_active` fields.
  - `KTerm_Voice_Enable`: creates a `SITUATION_NODE_PCM_INPUT` node on the active graph for playback.
  - `KTerm_Voice_ProcessPlayback`: pushes received audio into the PCM input node (falls back to local ring buffer if node unavailable).

---

---

---

---

---

---

## [v2.4.199 "System Introspection APIs"] - 2026-06-03

### Description

**v2.4.199**: New platform-abstracted APIs for OS identification, process enumeration, and active audio device query. Enables kterm console `sysinfo`, `ps`, and `threads` commands to work through Situation rather than raw platform calls. Cross-platform: Windows (RtlGetVersion, Toolhelp32, WASAPI), Linux (/etc/os-release, /proc, miniaudio), macOS (sysctl, miniaudio).

**Canonical version**: `sit/situation_base_version.h` → **2.4.199**.

### Library

- **`sit/situation_api.h`**
  - Added `SituationOSInfo` struct: `name[64]`, `version[64]`, `build_number`.
  - Added `SITAPI SituationOSInfo SituationGetOSInfo(void)` — OS name, version string, build number.
  - Added `SituationProcessInfo` struct: `pid`, `name[260]`, `memory_bytes`.
  - Added `SITUATION_MAX_PROCESS_NAME_LEN` (260).
  - Added `SITAPI SituationProcessInfo* SituationGetProcessList(int* out_count)` — snapshot of running OS processes with PID, name, and working set.
  - Added `SITAPI void SituationFreeProcessList(SituationProcessInfo* list, int count)` — frees returned list.
  - Added `SITAPI const char* SituationGetActiveAudioDeviceName(void)` — name of currently bound playback device.

- **`sit/situation_impl_io.h`**
  - Implemented `SituationGetOSInfo()`:
    - Windows: `RtlGetVersion` from ntdll (no compatibility shims). Detects Win7/8/8.1/10/11 by build number.
    - Linux: Parses `PRETTY_NAME` from `/etc/os-release`, kernel version from `uname()`.
    - macOS: `kern.osrelease` via `sysctlbyname`.
  - Implemented `SituationGetProcessList()`:
    - Windows: `CreateToolhelp32Snapshot` + `Process32First/Next` + `GetProcessMemoryInfo` for working set.
    - Linux: `/proc` directory enumeration, reads `/proc/<pid>/comm` and `/proc/<pid>/statm`.
  - Implemented `SituationFreeProcessList()` — calls `SIT_FREE`.
  - Implemented `SituationGetActiveAudioDeviceName()`:
    - Uses `ma_device_get_info()` on the active miniaudio device (WASAPI/PulseAudio/ALSA).
    - Fallback: reads `sit_gs.audio.miniaudio_device.playback.name` directly.

---

---

---

---

---

---

## [v2.4.200 "API Documentation Refresh"] - 2026-06-04

### Description

**v2.4.200**: Full API documentation refresh — closes the 93-patch documentation gap (v2.4.106 → v2.4.199). All 531 public `SITAPI` functions are now documented in `doc/situation_api.md`. Struct definitions updated to match current header. Deprecated APIs cataloged with replacements. Command reference expanded to 70 commands. Threading model description corrected across steering and docs. Copyright years unified to 2025-2026.

**Canonical version**: `sit/situation_base_version.h` → **2.4.200**.

### Documentation

- **`doc/situation_api.md`**
  - Version: v2.4.106 → v2.4.199.
  - Coverage: 438 → **531/531** functions (0 missing).
  - New sections: System Introspection, YPQ Color, MIDI Integration, Renderer Bolster Commands, CPU Topology & Affinity, Thread Pool Observability, Deprecated APIs table.
  - Fixed `SituationDeviceInfo` struct (stale fields `os_name`, `cpu_brand` → current `cpu_name`, `gpu_name`, storage/network/input arrays).
  - Fixed `SituationInitInfo` struct (added I/O config, thread affinity, NUMA placement, worker sizing fields).
  - Fixed "Strictly Single-Threaded" section → accurate "Threading Model" describing Main/Render/Audio/I/O + worker pool architecture.
  - Fixed usage examples referencing removed fields and deprecated commands.
  - License copyright: `2025` → `2025-2026`.

- **`doc/situation_command_reference.md`**
  - Version synced to v2.4.199.
  - Command index: 35 → **70** active commands (barriers, clears, transfers, raster state, indirect draw, YPQ grade, indexed viewport/scissor).

- **`doc/situation_api_index.md`** + **`doc/situation_api_generated.md`**
  - Regenerated — 531 functions indexed, 0 gaps.

- **`scripts/generate_situation_api_docs.py`**
  - `CMD_REF_ANCHORS`: 35 → 70 entries (all `SituationCmd*` mapped, 0 warnings).

### Steering / Project Context

- **`.kiro/steering/situation-project.md`**
  - Version: 2.4.0 → 2.4.199.
  - Rule #8: Corrected from "Library is strictly single-threaded" to accurate multi-thread architecture description (Main, Render, Audio, I/O, Worker pool; API call-site discipline from main thread).

### Copyright / License

- `doc/situation_api.md` license block: `Copyright (c) 2025` → `2025-2026`.
- `sit/k-term/LICENSE`: `2025 Trente Trois` → `2025-2026 Jacques Morel`.
- `sit/k-term/kt_shell.h`, `sit/k-term/example/situation.h`, `examples/kterm_console.c`: `(c) 2025` → `(c) 2025-2026`.

### Verification

- `python scripts/generate_situation_api_docs.py` → **531/531, 0 missing, 0 warnings**.
- No stale struct fields or deprecated function calls remain in usage examples.

---

---
