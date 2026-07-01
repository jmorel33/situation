# Audio Node Graph & Device Registry — System Plan

> **Reality check performed: June 2026 (v2.4.263)**
> Cross-referenced against `sit/situation_api.h`, `sit/aud/*.h`, `sit/aud/fx/*.h`,
> `sit/aud/polysonix/`, `examples/`, and `tests/harness/`.
> Checked boxes = verified in source. Unchecked = confirmed missing or incomplete.

---

## What This Is

This plan covers the design and implementation of the **audio node graph system** in Situation —
a registry-driven, directed-graph audio processing architecture where all sound-generating and
sound-processing components exist as typed, patchable nodes. It replaces the older
channel/bus/insert model and serves as the canonical audio API going forward.

**Key files:**
- `sit/aud/device_registry.h` — registry storage and query functions
- `sit/aud/registry_init.h` — built-in device registration at startup
- `sit/aud/node_graph.h` — internal node/graph struct definitions
- `sit/aud/node_graph_impl.h` — graph/node/patch lifecycle, topological sort
- `sit/aud/node_graph_process.h` — real-time processing loop
- `sit/aud/device_wrappers.h` — create/process/destroy adapters for every device type + `g_device_function_table`
- `sit/aud/node_graph_serialization.h` / `node_graph_serialization_impl.h` — JSON persistence
- `sit/aud/midi.h` — PortMidi-compatible hybrid hardware+virtual MIDI layer
- `sit/aud/midi_device.h` / `midi_device_callbacks.h` — per-node MIDI CC routing
- `sit/aud/midi_learn.h` — runtime MIDI Learn (CC → parameter mapping)
- `sit/aud/tone_synth.h` / `tone_synth_graph.h` — polyphonic tone synthesizer
- `sit/aud/polysonix/` — Polysonix polyphonic synthesis engine (separate sublibrary)
- `sit/aud/sound_source.h` / `mic_capture.h` / `pcm_input.h` — source/capture nodes
- `sit/aud/fx/` — 23 DSP effect modules

**Public surface:** All `SITAPI`-prefixed graph functions are declared in `sit/situation_api.h`.

---

## Design Principles (Unchanged)

- **Registry-Driven:** All nodes must be registered to be instantiable
- **Node-Graph Topology:** Directed graph with topological evaluation order
- **Protected Scope:** Unregistered types return error at creation time
- **Implicit Summing / Splitting:** Input ports sum multiple sources; output fans out
- **Extensibility:** Custom device types registerable via the public API
- **Persistence:** Graph topology + control values serializable to/from JSON
- **Real-Time Safety:** Audio callback path is lock-free (no alloc, no syscall)

---

## Phase 1 — Structures and Registry API ✅ COMPLETE

**Goal:** Core type definitions and registry infrastructure.

- [x] **Enums** defined in `situation_api.h`:
  - `SituationDeviceCategory`: EFFECT, SOURCE, CAPTURE, UTILITY, MODULATOR, ANALYZER, CUSTOM
  - `SituationControlType`: FLOAT, INT, BOOL, ENUM
  - `SituationNodeType`: 30+ built-ins plus `SITUATION_NODE_CUSTOM = 1000`
- [x] **Structs** defined in `situation_api.h`:
  - `SituationControlDesc` (id, name, type, min/max/default, enum_labels, units, is_logarithmic)
  - `SituationDeviceMetadata` (type, name, category, num_audio_ins/outs, num_ctrl_ins/outs,
    num_controls, controls array, audio_channels, version, description, author)
  - `SituationPatch` (src/dst node handles, src/dst port indices, is_control flag)
  - `SituationAudioPort` (buffer ptr, channels, frames)
  - `SituationControlPort` (value, is_modulated)
  - `SituationDeviceFunctions` (type, create, process, destroy function pointers)
  - `SituationNodeHandle` = `uint32_t` (generational: generation<<16 | index)
  - `SITUATION_INVALID_NODE_HANDLE` = 0xFFFFFFFF (in `situation_base_types.h`)
- [x] **Registry storage** in `device_registry.h`:
  - `g_device_registry[SITUATION_MAX_DEVICES]` static array
  - `g_device_registry_count`, `g_registry_initialized`
  - `_SituationInitRegistry()` (idempotent)
- [x] **Registry functions** (public API in `situation_api.h`):
  - `SituationInitDeviceRegistry()` — populates all built-ins (idempotent)
  - `SituationRegisterDeviceType()` — validates, deduplication-checks, copies metadata
  - `SituationGetDeviceMetadata()` — query by type, returns copy
  - `SituationIsDeviceRegistered()` — boolean check
  - `SituationGetRegisteredDeviceCount()` — count
  - `SituationGetCategoryName()` — display string for category enum
  - `SituationGetDeviceMetadataPtr()` — internal, returns stable pointer (not in public API)
  - `SituationGetDeviceMetadataByIndex()` — internal only (not in public API)
  - `SituationIterateRegistry()` — internal only (not in public API)
- [x] **Validation** in `SituationValidateDeviceMetadata()`:
  - Rejects empty names, invalid categories, bad port counts, control range errors

---

## Phase 2 — Built-in Device Population ✅ COMPLETE (30 devices)

**Goal:** Register all built-in device types at init time.

Original plan called for 7 devices. Actual count: **30 registered devices** across 6 categories,
all registered by `SituationInitDeviceRegistry()` in `registry_init.h`, with full
create/process/destroy wrappers in `g_device_function_table` in `device_wrappers.h`.

### Effects (17 registered)
- [x] **Reverb** (`SITUATION_NODE_REVERB`) — Freeverb algorithm, 5 controls
- [x] **Echo** (`SITUATION_NODE_ECHO`) — Delay line, 3 controls: delay_time, feedback, wet_mix
- [x] **Chorus** (`SITUATION_NODE_CHORUS`) — 4-stage, 21 controls (4 stages × 4 params + 5 global)
- [x] **Phaser** (`SITUATION_NODE_PHASER`) — 6 controls: rate, depth, feedback, stages, stereo_phase, mix
- [x] **Overdrive** (`SITUATION_NODE_OVERDRIVE`) — 10 controls
- [x] **Exciter** (`SITUATION_NODE_EXCITER`) — 7 controls
- [x] **Maximizer** (`SITUATION_NODE_MAXIMIZER`) — FFTW3-based spectral maximizer
- [x] **Spring Reverb** (`SITUATION_NODE_SPRING_REVERB`) — Physical spring reverb model
- [x] **Studio Reverb** (`SITUATION_NODE_STUDIO_REVERB`) — 9 controls
- [x] **SST-282** (`SITUATION_NODE_SST282`) — Vintage tape saturation / harmonic distortion
- [x] **Mastering Amp** (`SITUATION_NODE_MASTERING_AMP`) — SSE-optimized mastering processor
- [x] **DeafMax** (`SITUATION_NODE_DEAFMAX`) — Zero-alloc surgical peak maximizer
- [x] **Dynamics** (`SITUATION_NODE_DYNAMICS`) — Compressor/limiter/gate
- [x] **Compander** (`SITUATION_NODE_COMPANDER`) — 3-band multiband compander with EQ
- [x] **EQ 4-Band** (`SITUATION_NODE_EQ_4BAND`) — Parametric EQ
- [x] **Filter** (`SITUATION_NODE_FILTER`) — 5 controls: cutoff, resonance, type (LP/HP/BP), gain_db, order
- [x] **ISA110** (`SITUATION_NODE_ISA110`) — Focusrite ISA 110 preamp + 4-band inductor EQ; registered, wrapper complete, MIDI CC map present (v2.4.261)

### Sources (3 registered)
- [x] **Tone Synth** (`SITUATION_NODE_TONE_SYNTH`) — Full polyphonic synthesizer with voices, ADSR,
  sub-oscillator, LFO, filter, portamento, pitch bend, MIDI control, 39 controls
- [x] **Sound Source** (`SITUATION_NODE_SOUND_SOURCE`) — PCM buffer playback node. `sound_source.h`
  provides `sound_source_load_buffer()` (copies a float buffer in) and
  `sound_source_feed_interleaved_frames()` (push a block for single-shot playback). The node
  is functional but not integrated with `SituationLoadSound()` / `SituationPlaySound()` —
  audio data must be pushed manually; there is no bridge from the high-level sound API.
- [x] **PCM Input** (`SITUATION_NODE_PCM_INPUT`) — Lock-free ring buffer; push PCM from any thread
  via `SituationPushNodePCM()` / `SituationGetNodePCMFreeFrames()`

### Capture (1 registered)
- [x] **Mic Capture** (`SITUATION_NODE_MIC_CAPTURE`) — Allocates a 1-second ring buffer and reads
  from it in the process function. The ring buffer is an isolated stub — it is **not wired** to
  the WASAPI capture pipeline (`SituationStartAudioCaptureEx()`). Audio data reaches the node
  only via `mic_capture_simulate_input()` (a test helper), not from real microphone hardware.

### Utilities (3 registered)
- [x] **Panner** (`SITUATION_NODE_PANNER`) — Constant-power stereo panner
- [x] **Gain** (`SITUATION_NODE_GAIN`) — Simple gain node
- [x] **Mixer** (`SITUATION_NODE_MIXER`) — Bus summing node (16 inputs → 1 stereo output)

### Modulators (2 registered)
- [x] **LFO** (`SITUATION_NODE_LFO`) — 2 controls: frequency, waveform; outputs control signal
- [x] **Envelope Follower** (`SITUATION_NODE_ENVELOPE_FOLLOWER`) — Audio-to-control modulator

### Analyzers (2 registered)
- [x] **Peak Meter** (`SITUATION_NODE_PEAK_METER`) — Per-channel peak/RMS metering
- [x] **Spectrum Analyzer** (`SITUATION_NODE_SPECTRUM_ANALYZER`) — FFT-based spectrum output

### Not Yet Registered
- [ ] **Polysonix** (`sit/aud/polysonix/`) — Complete polyphonic synthesis sublibrary with ROM
  patches, wavetable sequencer, and a bytecode VM (`px_vm.h`). No `SITUATION_NODE_POLYSONIX`
  type, no wrapper, no registration — not reachable from the graph API. This is the biggest
  unconnected capability in the audio subsystem.

---

## Phase 3 — Node Creation, Patching, and Control API ✅ COMPLETE

**Goal:** Allow graph construction and parameter control from the main thread.

All functions declared in `situation_api.h` and implemented in `node_graph_impl.h`:

- [x] `SituationCreateGraph()` — allocates graph + patch array (cap 256) + sorted_nodes array
- [x] `SituationDestroyGraph()` — two-phase detach (waits for RT callback idle, nulls
  active/default graph pointers, waits again, frees all nodes/ports/patches)
- [x] `SituationCreateNode()` — registry lookup, alloc + init ports + control defaults, calls
  device create func, triggers sort immediately
- [x] `SituationDestroyNode()` — closes MIDI hardware stream, frees MIDI Learn state, cleans up
  all graph-level and per-node patch list entries (fixed v2.4.261), calls device destroy,
  increments handle generation, re-sorts
- [x] `SituationGetNode()` — handle validation (index + generation check)
- [x] `SituationCreatePatch()` — port validation, channel mismatch check, DFS cycle detection,
  adds to graph and per-node patch lists, marks ctrl port `is_modulated`, re-sorts
- [x] `SituationRemovePatch()` — removes from graph patch list and from both per-node patch lists
  (fixed v2.4.261 via `_SituationRemovePatchFromArray`); clears `is_modulated` if last ctrl patch
- [x] `SituationDestroyPatch()` — legacy audio-only wrapper for `SituationRemovePatch`
- [x] `SituationTopologicalSort()` — public SITAPI; Kahn's algorithm; audio patches drive order
  (control patches excluded); cached in `sorted_nodes[]`
- [x] `SituationSetControl()` — range validation, type coercion (bool/int/enum), writes
  `control_values[]`; special-cases tone synth patch slot/store controls
- [x] `SituationGetControl()` — reads `control_values[]`
- [x] `SituationSetActiveGraph()` / `SituationGetActiveGraph()` — live graph switching
- [x] **Generational handles** — 16-bit index + 16-bit generation; stale handles return NULL
- [x] **Cycle detection** — DFS from dst toward src in `SituationWouldCreateCycle()`

### Known Gap: Thread-Safe Graph API Called by Examples but Does Not Exist

Five example files (`threading_raw.c`, `threading_stress_test.c`, `threading_minimal_test.c`,
`threading_diagnostic_test.c`, `node_graph_threading_test.c`) use a `SituationThreadSafeGraph*`
type and the following functions:

- `SituationCreateThreadSafeGraph()`
- `SituationDestroyThreadSafeGraph()`
- `SituationCreateNodeThreadSafe()`
- `SituationCreatePatchThreadSafe()`
- `SituationProcessGraphThreadSafe()`
- `SituationSetNodeControlThreadSafe()`

**None of these exist anywhere in the library.** Not in `situation_api.h`, not in any `sit/` header.
The examples would fail to link against the public library. They are dead code referencing a
planned but unimplemented thread-safe graph wrapper.

**Fix required:** Either implement the thread-safe graph wrapper (mutex-protected or copy-on-write
topology, lock-free control writes via atomics), declare it in `situation_api.h`, and expose the
`SituationThreadSafeGraph` type — or remove/rewrite the examples to use the current API with the
understanding that graph mutation is main-thread-only. The current `SituationProcessGraph()` is
already designed to be called from the RT audio thread; only topology mutation (create/destroy
node/patch) is not thread-safe.

---

## Phase 4 — Real-Time Graph Evaluation ✅ COMPLETE

**Goal:** Audio callback processes the graph in topological order.

Implemented in `node_graph_process.h`:

- [x] `SituationProcessGraph()` — main entry point; called from RT audio thread:
  1. If `needs_resort || sorted_count == 0`: output silence, return
  2. For each node in sorted order: zero inputs → sum sources → apply ctrl patches → clear
     output buffers → dispatch `ProcessFunc`
  3. Sum unpatched outputs to master buffer (mono→stereo upmix; skip port >0 for multi-out nodes)
- [x] **Sort caching** — `needs_resort` flag; sorts on main thread at all topology mutations;
  callback outputs silence while flag is set (safe; avoids reading a partially-sorted list)
- [x] **Implicit summing** — `_SituationSumBuffers()` accumulates from all connected sources
- [x] **MIDI dispatch in callback** — per MIDI-enabled node: `Pm_Read()` batch → note-on/off/CC/
  pitch-bend/program-change dispatch; MIDI Learn intercepts CC first
- [x] **`g_device_function_table[]`** — all 26 device types wired; dispatched per-node per-block
- [x] **`SituationSetActiveGraph()` / `SituationGetActiveGraph()`** — atomic graph switching

### Known Minor Issue: Mixer Node Port Iteration

`_SituationProcessMixerNodeNode()` in `device_wrappers.h` uses `break` (not `continue`) on the
first null input buffer. Continuously-allocated ports are never sparse in practice, so this is
benign today — but would silently drop inputs from a gap onward if sparse port assignment were
ever used. `situation_mixer_node_process()` in `fx/mixer_node.h` correctly uses `continue`.

---

## Phase 5 — Persistence and Extensibility ✅ MOSTLY COMPLETE

**Goal:** Save/load graphs; support custom device types.

### Serialization (all declared in `situation_api.h`)
- [x] `SituationSaveGraphToFile()` — serializes to JSON file
- [x] `SituationLoadGraphFromFile()` — parses JSON, re-creates nodes via device funcs table
- [x] `SituationSerializeGraphToJSON()` — returns allocated JSON string (caller frees)
- [x] `SituationDeserializeGraphFromJSON()` — parses JSON string
- [x] `SituationFreeJSONString()` — frees serialization output
- [x] `SituationGetSerializationVersion()` — returns `"2.4.0"` constant string
- [x] `SituationIsVersionCompatible()` — checks version field from JSON
- [x] **Sample rate** — calls `SituationGetAudioPlaybackSampleRate()` at serialize time; falls back
  to 48000 if audio not initialized (fixed v2.4.262 — no longer a hardcoded constant)
- [x] Format: human-readable JSON with version header, `"nodes"` array (id, type_name, active flag,
  controls map), `"patches"` array (src/dst id+port, type string)

### Remaining Gaps in Serialization
- [ ] **`SituationIsVersionCompatible()` uses exact string match only** — `strcmp` against
  `"2.4.0"`. Any format change will break all saved files with no migration path.
  `// TODO: Implement semantic versioning comparison` is in the source.
- [ ] **MIDI Learn presets not included in graph JSON** — `SituationSaveMidiPreset()` writes a
  separate file. A full session save requires saving graph JSON + one preset file per MIDI-
  enabled node. No combined session format exists.
- [ ] **Custom node round-trip**: loading resolves type by name string — works if custom types
  are re-registered before load, but there is no error message when a custom type is missing
  at load time (silently fails the `_JSONParseNode` step).

### Custom Type Registration
- [x] `SituationRegisterDeviceType()` is public — callers fill `SituationDeviceMetadata` with
  any type value ≥ `SITUATION_NODE_CUSTOM = 1000`
- [ ] No `SituationRegisterCustomType()` convenience wrapper — callers must fill the full
  metadata struct manually. Not a blocker; the API is sufficient.
- [ ] No runtime plugin/DLL hot-load path for custom device types

### MIDI Learn (post-original-plan addition — fully implemented)
All declared in `situation_api.h`:
- [x] `SituationEnableMidiLearn()` / `SituationDisableMidiLearn()` — per-node opt-in
- [x] `SituationStartMidiLearn()` — begin 5s learn window for a specific control index
- [x] `SituationCancelMidiLearn()` — abort learn
- [x] `SituationIsLearning()` / `SituationIsMidiLearnEnabled()` — status queries
- [x] `SituationClearMidiMapping()` / `SituationClearAllMidiMappings()` — mapping removal
- [x] `SituationSaveMidiPreset()` / `SituationLoadMidiPreset()` — JSON preset files
- [x] 7-bit and 14-bit CC detection (MSB+LSB within 100ms window)
- [x] Channel filter (omni or specific channel)
- [x] Conflict callback, learn-complete callback, learn-timeout callback

---

## Phase 6 — Testing, Optimization, and Documentation ⚠️ PARTIAL

### Testing
- [x] Test harness modules: `tests/harness/test_audio.c`, `test_audio_effects_heard.c`
- [x] Example programs: `node_graph_demo.c`, `node_graph_piano_demo.c`,
  `simple_process_test.c`, `mixer_insert_demo.c`, `mixer_aux_demo.c`
- [x] Threading example files exist: `threading_raw.c`, `threading_stress_test.c`,
  `threading_minimal_test.c`, `threading_diagnostic_test.c`, `node_graph_threading_test.c`
  — **but all reference the unimplemented thread-safe API** (see Phase 3 gap)
- [ ] **No fuzz/stress test for graph topology** (random create/destroy/patch cycles)
- [ ] **No automated save→load round-trip test** — serialization is untested end-to-end
  in the harness

### Optimization
- [x] SIMD in process functions: `mastering_amp.h` uses SSE intrinsics
- [ ] **No silent-node pruning** — all `is_active` nodes process every block regardless of
  whether their inputs are silent; `needs_processing` flag is set but never read in the
  audio callback path
- [ ] **No per-block timing/telemetry** — no CPU measurement in `SituationProcessGraph()`;
  impossible to profile individual node cost at runtime

### Documentation
- [x] `doc/midi_api.md` — MIDI API reference
- [x] `doc/audio_analysis.md` — architecture overview
- [ ] **No node graph usage guide** — no tutorial or walkthrough in `doc/`; only scattered
  example files
- [ ] **`situation_api.md` node graph section may be stale** relative to v2.6 MIDI Learn
  additions and the ISA110 / PCM Input additions

---

## Phase 7 — Post-Original-Plan Additions ✅ COMPLETE

Work that landed organically after the original plan ended.

### MIDI Hardware Layer (`sit/aud/midi.h`)
- [x] PortMidi-compatible API (`Pm_Initialize`, `Pm_OpenInput/Output`, `Pm_Read/Write`, etc.)
- [x] Windows WinMM backend for hardware MIDI I/O
- [x] Virtual MIDI devices with lock-free SPSC ring buffers
- [x] MIDI routing matrix and per-connection filter (note-on/off, CC, PC, PB, aftertouch, channel mask)
- [x] MIDI transform (transpose, velocity curve, channel remap)
- [x] SysEx input/output
- [x] Hotplug detection (`Pm_HasDeviceListChanged()`, `Pm_SetDeviceChangeCallback()`)
- [x] MIDI recording/playback (`PmRecording` struct)
- [ ] **ALSA backend (Linux)**: `PM_HAS_HARDWARE_MIDI 0` — TODO stub only
- [ ] **CoreMIDI backend (macOS)**: `PM_HAS_HARDWARE_MIDI 0` — TODO stub only

### Per-Node MIDI CC Routing (`sit/aud/midi_device.h`, `midi_device_callbacks.h`)
All declared in `situation_api.h`:
- [x] `SituationEnableMidiControl()` / `SituationDisableMidiControl()` / `SituationAutoConnectMidi()`
- [x] `SituationIsMidiEnabled()` / `SituationSetNodeMidiChannel()`
- [x] `SituationListMidiDevices()` / `SituationGetMidiDeviceName()`
- [x] Virtual MIDI loopback API: `SituationSetupVirtualMidiLoopback()`, `SituationVirtualMidiNoteOnEx()`, etc.
- [x] Hardcoded CC maps for all registered node types
- [x] Tone Synth: full note-on/off, pitch bend, program change, portamento, CC for filter/LFO/env

### PCM Input Node (`sit/aud/pcm_input.h`)
- [x] `SituationPushNodePCM()` / `SituationGetNodePCMFreeFrames()` — public API
- [x] Lock-free ring buffer push from any thread; correct interleaved float handling

### Tone Synth Polyphony (`sit/aud/tone_synth.h`, `tone_synth_graph.h`)
- [x] Polyphonic voices with per-voice ADSR, main + sub oscillator, 5 waveforms
- [x] Pulse-width modulation, LFO vibrato + tremolo + filter modulation
- [x] Per-voice filter with envelope + LFO cutoff offset; portamento; mono mode
- [x] Sum limiter on voice stack, MIDI note-to-frequency table
- [x] Patch preset save/load (TONE_CTRL_PATCH_SLOT / PATCH_STORE controls)
- [x] Pitch bend 14-bit, program change dispatch

### SFX Routing Bridge
- [x] `SituationSetToneRouting()` — route a procedural tone handle to the active graph's
  SFX sound source node
- [x] `SituationSetGraphSFXSource()` — designate a Sound Source node as the SFX receiver
  (partially bridges the gap between high-level tone API and node graph)

### Polysonix (`sit/aud/polysonix/`)
- [x] Complete polyphonic engine with ROM patches (`px_patches_rom.h`), wavetable ROM
  (`px_wave_rom.h`, `px_wave_native.h`), wavetable sequencer (`px_wseq_rom.h`),
  bytecode VM (`px_vm.h`, `px_vm.comp`), patch bank system (`px_patching.h`)
- [ ] **Not connected to node graph** — no enum value, no wrapper, no registration (see Phase 2)

---

## Open Items Summary

| # | Area | Description | Status | Severity |
|---|------|-------------|--------|----------|
| 1 | Phase 3 | `SituationRemovePatch` did not clean per-node patch lists | ✅ Fixed v2.4.261 | — |
| 2 | Phase 3 | `SituationDestroyNode` did not clean peer nodes' patch lists | ✅ Fixed v2.4.261 | — |
| 3 | Phase 2 | ISA110 not registered | ✅ Fixed v2.4.261 | — |
| 4 | Phase 5 | Serialization hardcoded 48000 sample rate | ✅ Fixed v2.4.262 | — |
| 5 | Phase 3 | `SituationCreateNodeThreadSafe` / `SituationCreatePatchThreadSafe` etc. called by 5 examples but not declared in public API, not found anywhere in the library | **OPEN** | Bug / Missing API |
| 6 | Phase 2 | Polysonix is a complete synthesis engine with no node graph exposure | **OPEN** | Missing integration |
| 7 | Phase 2 | `SITUATION_NODE_SOUND_SOURCE` not bridged to `SituationLoadSound()` / `SituationPlaySound()` — audio data must be pushed manually | **OPEN** | Design gap |
| 8 | Phase 2 | `SITUATION_NODE_MIC_CAPTURE` ring buffer is isolated stub — not wired to WASAPI capture pipeline | **OPEN** | Design gap |
| 9 | Phase 5 | `SituationIsVersionCompatible()` uses exact string match only — no semver, breaks on any format version increment | ✅ Fixed v2.4.264 | — |
| 10 | Phase 5 | No combined session format (graph JSON + MIDI Learn presets) — multi-file round-trip required | **OPEN** | Missing feature |
| 11 | Phase 5 | Custom node type deserialization silently fails if type not re-registered before load | ✅ Fixed v2.4.264 | — |
| 12 | Phase 6 | `needs_processing` flag set but never checked in `SituationProcessGraph()` — no silent-node pruning | **OPEN** | Optimization |
| 13 | Phase 6 | No per-block CPU/latency telemetry in graph evaluation | **OPEN** | Missing feature |
| 14 | Phase 6 | No automated save→load serialization round-trip test | **OPEN** | Testing gap |
| 15 | Phase 6 | Node graph usage guide missing from `doc/` | **OPEN** | Docs gap |
| 16 | Phase 4 | Mixer node process uses `break` on null buffer instead of `continue` — sparse port assignment would drop inputs | ✅ Fixed v2.4.264 | — |
| 17 | Phase 7 | MIDI: ALSA backend (Linux) not implemented | **OPEN** | Platform gap |
| 18 | Phase 7 | MIDI: CoreMIDI backend (macOS) not implemented | **OPEN** | Platform gap |

---

## Priority Recommendations

**Fix first (correctness / broken code):**
- **Item 5** — The five threading example files reference a `SituationThreadSafeGraph` type and
  six functions that do not exist. They are dead code and would cause link errors if built.
  Decision needed: implement the thread-safe wrapper (highest value), or rewrite/remove the
  examples. The examples have been in this state since at least v2.4.262; they are misleading to
  anyone reading the codebase.
- **Item 11** — Silent failure on missing custom type at load time. Should log + return an
  appropriate error code rather than returning `SITUATION_ERROR_NODE_DESERIALIZATION_FAILED`
  with no diagnostic about which type name was unresolvable.

**High value (capability):**
- **Item 6** — Polysonix to node graph. The engine is complete and standalone; a thin wrapper
  (create/process/destroy) + registration + `SITUATION_NODE_POLYSONIX` enum value is all that
  is needed. This would expose ROM-patch polyphonic synthesis through the same API as everything
  else.
- **Item 7** — Sound Source bridge. `SituationSetGraphSFXSource()` is a partial step; extending
  it so that `SituationPlaySound()` can feed a designated Sound Source node would unify the two
  audio playback paths.
- **Item 8** — Mic Capture wiring. The node needs to pull from the real WASAPI capture ring
  buffer rather than a private stub buffer. Design: add a pointer to the global capture state
  in `SituationMicCapture` and read from it in `mic_capture_process()`.

**Small fixes (worth doing opportunistically):**
- **Item 9** — Replace `strcmp` in `SituationIsVersionCompatible()` with a major.minor
  compatibility check (same major = compatible; patch changes are always backwards compatible).
- **Item 16** — Change `break` to `continue` in `_SituationProcessMixerNodeNode()` for
  defensive correctness.
- **Item 12** — Wire `needs_processing` into `SituationProcessGraph()` as an early-out for
  nodes with all-zero inputs and no active MIDI. Low-effort perf win for sparse graphs.
