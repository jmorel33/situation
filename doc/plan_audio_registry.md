# Implementation Plan for Device Registry and Patching System in Situation Audio Mixer

This plan outlines a comprehensive, phased approach to integrating a **device registry** and **programmatic patching system** into the Situation library's audio mixer (v2.3.x+). The goal is to create a modular, extensible "audio apparatus" where all components (effects, sources, mixers) are treated as registerable devices (nodes) with defined ins/outs/controls. This enforces a protected scope: The mixer **only processes registered devices**, preventing ad-hoc additions and ensuring thread-safety, persistence, and optimization.

We'll use **existing effects (Reverb, Delay, Filter)** as primary examples to demonstrate the full solution. We'll extend to **Mic Input (capture)**, **On-Board Tone Generator** (a simple built-in synth), and **Low Frequency Oscillator (LFO)**, resulting in **7 default registered devices** out-of-the-box.

The system builds on miniaudio callbacks, snapshot-and-unlock concurrency, and generational handles. New effects/plugs (written in C) can be registered dynamically, but the mixer topology is graph-based and only evaluates registered types.

**Key Principles**:
- **Registry-Driven**: All devices must be registered (built-in at init, customs via API) to be instantiable/patchable.
- **Node-Graph Topology**: Mixer becomes a directed graph of nodes (devices); evaluation happens in the real-time callback.
- **Protected Scope**: Unregistered types throw errors; graph validation prevents invalid patches.
- **Implicit Summing/Splitting**: Input ports implicitly sum multiple signals; Output ports implicitly fan-out to multiple targets.
- **Extensibility**: Users add C plugs via registration hooks (create/process funcs).
- **Persistence**: Serialize graph (nodes + patches) to JSON-like structure for session save/load.
- **Optimization**: Topological sort with caching for eval order; skip unchanged nodes.

## Phase 1: Define Structures and Registry API (Foundation Setup)
**Goal**: Establish metadata structs and registry mechanics. Demonstrate with existing Reverb as the first example.

**Actionables**:
- [ ] **Define Enums and Structs** (in `situation_impl_audio.h`):
   - Enums:
     - `SituationDeviceCategory` (EFFECT, SOURCE, MIXER, MODULATOR, CAPTURE).
     - `SituationControlType` (FLOAT, INT, BOOL).
     - `SituationNodeType` (SITUATION_NODE_REVERB, SITUATION_NODE_DELAY, SITUATION_NODE_FILTER, SITUATION_NODE_SOUND_SOURCE, SITUATION_NODE_MIC_CAPTURE, SITUATION_NODE_TONE_GENERATOR, SITUATION_NODE_LFO, SITUATION_NODE_CUSTOM).
     - `SituationError` (SITUATION_ERR_INVALID_PATCH, SITUATION_ERR_UNREGISTERED_TYPE, etc.).
   - Structs:
     - `SituationControlDesc` (id, name, type, min/max/default).
     - `SituationDeviceMetadata` (type, name, category, num_audio_ins/outs, num_ctrl_ins/outs, controls array, num_controls).
     - `SituationNode` (type, data ptr, handle, connections array/list for patches).
     - `SituationPort` (channels, buffer ptr for audio; value for controls).
     - **Note**: Most audio devices default to **Stereo (2 channels)** unless specified otherwise. Control ports are typically Mono.

- [ ] **Implement Registry**:
   - Static array: `static SituationDeviceMetadata g_registry[SITUATION_MAX_DEVICES = 64]; static int g_registry_count = 0;`.
   - Registration Func: `void SituationRegisterDeviceType(const SituationDeviceMetadata* meta);` – Copies meta to registry, checks for duplicates.
   - Query Funcs: `bool SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out);`, `void SituationIterateRegistry(void (*callback)(const SituationDeviceMetadata* meta, void* user_data), void* user_data);`.

- [ ] **Register First Example (Reverb)**:
   - In library init (`SituationInitAudio()`): Define and register Reverb metadata (name="Reverb", category=EFFECT, audio_ins=2, outs=2, controls: decay (float 0.1-10, def 2.0), damping (0-1, def 0.5), wet_mix (0-1, def 0.3)).
   - Demo: Simple test code snippet in comments showing query: Get metadata, print name/ins/outs/controls.

**Risks / Dependencies**: Minimal risk. Depends on `situation_impl_audio.h` structure.

**Milestone**: Registry populated with Reverb; basic queries work. Estimated Time: 1-2 days.

## Phase 2: Register All Default Devices (Built-In Population)
**Goal**: Add the 7 defaults, using existing effects/code as templates. Ensure Mic and Tone Gen integrate seamlessly. Detailed API integration required.

**Actionables**:

- [ ] **Register Existing Effects (Delay, Filter)**:
   - **Delay**:
     - **Metadata**: name="Delay", category=EFFECT, ins=2, outs=2.
     - **Controls**: `time` (0.01-2.0, def 0.5), `feedback` (0-1.0, def 0.3), `wet_mix` (0-1.0, def 0.5).
     - **Implementation Details**: Uses `ma_delay` internally. Must allocate `ma_delay` and wrap in `SituationNode`. State updates (`feedback`, `wet_mix`) mapped to `ma_delay_set_decay`, `ma_delay_set_wet`. Re-initialization required if `time` changes dynamically.
   - **Filter**:
     - **Metadata**: name="Filter", category=EFFECT, ins=2, outs=2.
     - **Controls**: `cutoff` (20-20000, def 1000), `resonance` (0.1-10.0, def 1.0), `type` (0-2: lowpass, highpass, bandpass, def 0).
     - **Implementation Details**: Uses miniaudio's biquad nodes (`ma_biquad_node`). Map `type` directly to miniaudio biquad configuration formulas.

- [ ] **Register Sound Source**:
   - **Metadata**: name="SoundSource", category=SOURCE, ins=0, outs=2.
   - **Controls**: `volume` (0-2.0, def 1.0), `pitch` (0.5-2.0, def 1.0), `play_state` (0-1: stop/play).
   - **Implementation Details**: Wrap existing `SituationSound` / `ma_sound` handles. Node processing pulls samples via `ma_sound_read_pcm_frames`.

- [ ] **Register Mic Capture**:
   - **Metadata**: name="MicCapture", category=CAPTURE, ins=0, outs=2.
   - **Controls**: `gain` (0-2.0, def 1.0).
   - **Implementation Details**: Tied to `SituationStartAudioCaptureEx`. The node pulls frames from the capture device's lock-free ring buffer (e.g., `rb_capture`). If capture is inactive, output silence.

- [ ] **Implement and Register Tone Generator** (New Synth):
   - **Metadata**: name="ToneGenerator", category=SOURCE, ins=1 (Freq Mod), outs=2.
   - **Controls**: `frequency` (20-20000, def 440), `waveform` (0-3: sine, square, tri, saw, def 0), `amplitude` (0-1.0, def 0.5), `trigger` (bool 0/1).
   - **State Struct**:
     ```c
     typedef struct {
         float phase;
         float phase_increment;
         uint32_t sample_rate;
     } SituationToneGenState;
     ```
   - **Implementation Details**: Provide per-sample DSP loop calculating oscillators. Input port 0 modulates frequency. Handle phase reset if `trigger` control transitions from 0 to 1.

- [ ] **Implement and Register LFO** (Modulator):
   - **Metadata**: name="LFO", category=MODULATOR, ins=0, outs=1 (Control Signal).
   - **Controls**: `frequency` (0.1-20.0, def 1.0), `waveform` (0-3: sine, square, tri, saw, def 0), `depth` (0-1.0, def 1.0).
   - **State Struct**:
     ```c
     typedef struct {
         float phase;
         float phase_increment;
     } SituationLFOState;
     ```
   - **Implementation Details**: Outputs a single-channel control signal (floating point buffer) in its `ProcessFunc` instead of audio. Range [-1.0, 1.0] scaled by `depth`.

- [ ] **Init Hook**:
   - In `SituationInitAudio()`, explicitly register all 7 defaults (Reverb, Delay, Filter, SoundSource, MicCapture, ToneGenerator, LFO) via `SituationRegisterDeviceType`.
   - Add debug iteration macro to print registry manifest on startup.

**Risks / Dependencies**: Tone Gen DSP needs efficiency. `ma_delay` runtime length changes may require buffering strategies.

**Milestone**: All 7 devices registered at startup; metadata fully queryable. Structural definitions clear for phase 3 patching. Estimated Time: 3 days.

## Phase 3: Node Creation and Patching API (Graph Building)
**Goal**: Allow instantiating/patching nodes from registry only. Demonstrate full chain with examples (e.g., Source → Reverb → Delay).

**Actionables**:
- [ ] **Node Creation**:
   - Func: `SituationNode* SituationCreateNode(SituationNodeType type);` – Lookup metadata, alloc node, init data (call type-specific create fn if custom), assign generational handle.
   - Enforce Registry: If type not in registry, return NULL/error.
   - For customs: Hook `typedef SituationNode* (*CreateFunc)(const SituationDeviceMetadata* meta);`.

- [ ] **Patching & Implicit Logic**:
   - Funcs: `bool SituationPatch(SituationNode* src, int src_port, SituationNode* dst, int dst_port, bool is_control);`, `bool SituationUnpatch(...)`.
   - **Implicit Summing**: If multiple sources patch to `dst->port[0]`, the mixer sums them before processing.
   - **Implicit Splitting**: If `src->port[0]` patches to multiple destinations, the buffer is copied/read by all.
   - Validation: Check port bounds, types. Store edges in `src->outputs` list and `dst->inputs` list.
   - Cycle Detection: Simple DFS on patch. Optional "allow_feedback" flag for delay lines.

- [ ] **Control Access & Modulation**:
   - Funcs: `bool SituationSetControl(SituationNode* node, uint32_t control_id, float value);`.
   - Modulation: Allow patching LFO output to control inputs.
   - **Smoothing**: Implement per-sample or block-based interpolation/smoothing in process funcs for modulated controls to prevent zipper noise.

- [ ] **Demo Topology**:
   - Example Code: Create Source (load sound), Reverb, Delay; Patch Source out → Reverb in → Delay in (parallel? Use mult if needed); Set Reverb decay=3.0.
   - Full Chain: Add MicCapture → Filter → master (implicit output node).

**Risks / Dependencies**: Circular dependency detection logic. Control rate smoothing implementation complexity.

**Milestone**: Nodes creatable/patchable only from registry; simple graphs buildable. Estimated Time: 3-4 days.

## Phase 4: Integrate with Mixer Callback (Real-Time Evaluation)
**Goal**: Refactor mixer to evaluate the graph topology in miniaudio callback, using registered devices only.

**Actionables**:
- [ ] **Graph Management & Topological Sort**:
   - Global or per-mixer: `SituationAudioGraph` struct (nodes list, roots/sources array).
   - **Sort Caching**: Recompute topological sort order only when topology changes (add/remove/patch). Store linear list of nodes in `sorted_nodes`.
   - **Master Output Node**: Explicitly define a hidden "Master Output" node. All unpatched output ports (or signals specifically routed to Master) are summed into this node, which feeds the device buffer.

- [ ] **Evaluation Loop**:
   - In callback: Snapshot params (existing), use `sorted_nodes`.
   - Per Node:
     - 1. Zero input buffers.
     - 2. Sum all connected sources into input buffers.
     - 3. Call `ProcessFunc(node, ins, outs, frames)`.
     - 4. Output buffers are now ready for next nodes.
   - Output: Route "Master Output" node buffer to miniaudio device buffer.

- [ ] **Thread-Safety**: Patches/sets from non-RT threads; snapshot copies graph state for callback.
- [ ] **Migration**: Convert current channels/buses/inserts to nodes (e.g., channel strip = implicit EQ+Dynamics node).

**Risks / Dependencies**: Real-time safety (no malloc in callback). Thread contention during graph updates.

**Milestone**: Mixer runs graph-based; test with Source → Reverb → Output chain (play sound with effect). Estimated Time: 4-5 days.

## Phase 5: Persistence, Validation, and Extensibility (Polish and Protection)
**Goal**: Ensure saves/loads, robust errors, and custom plug support. Full demo with all devices.

**Actionables**:
- [ ] **Persistence**:
   - Format: JSON-like binary structure or actual JSON (if dev environment allows, else custom binary).
   - Structure:
     - `nodes`: list of { id, type_string, controls: {id: val} }
     - `patches`: list of { src_id, src_port, dst_id, dst_port, type }
   - Example Format:
     ```json
     {
       "nodes": [
         {"id": 1, "type": "SoundSource", "controls": {"volume": 0.8}},
         {"id": 2, "type": "Reverb", "controls": {"decay": 3.0}}
       ],
       "patches": [
         {"src_id": 1, "src_port": 0, "dst_id": 2, "dst_port": 0, "type": "audio"}
       ]
     }
     ```
   - Funcs: `void SituationSaveAudioGraph(const SituationAudioGraph* graph, const char* file);`.
   - **Custom Types**: Store type as string name to resolve dynamically against registry on load.

- [ ] **Validation and Protection**:
   - Runtime Checks: Invalid patch/control → log via centralized error callback (`SituationSetErrorCallback`).
   - Hotplug: For MicCapture, handle device changes via miniaudio events.

- [ ] **Custom Registration**:
   - Func: `void SituationRegisterCustomType(const char* name, SituationDeviceCategory cat, int ins/outs, SituationControlDesc* controls, CreateFunc create, ProcessFunc process, DestroyFunc destroy);`.

- [ ] **Full Demo Example**:
   - In examples/: Build graph with all 7 – e.g., ToneGen + MicCapture → Filter (Modulated by LFO) → Reverb → Output. Save/Load session.

**Risks / Dependencies**: Versioning of serialized data. Handling missing custom types on load.

**Milestone**: Persistent, extensible mixer; customs addable. Estimated Time: 2-3 days.

## Phase 6: Testing, Optimization, and Documentation (Deployment Readiness)
**Goal**: Harden for production; document for dev team/users.

**Actionables**:
- [ ] **Testing**:
   - **Fuzz Testing**: Randomly create nodes and patch them to stress-test topological sort and cycle detection.
   - Unit: Registry queries, node create/patch, graph eval (mock callback).
   - Integration: Stress with 20+ nodes (multi Reverbs), latency checks.

- [ ] **Optimization**:
   - SIMD in process funcs (e.g., vectorized Reverb).
   - Graph Pruning: Skip nodes if inputs unchanged/silent (needs "is_silent" flag propagation).
   - **Metrics**: Add peak CPU/latency logging. Compare "Before vs After" refactor.

- [ ] **Documentation**:
   - README Update: Section on registry/patching, with examples using defaults.
   - API Docs: Doxygen-style for new funcs.
   - MIXER_PLAN.MD: Update with implemented status.
   - **Versioning**: Suggest version bump to v2.4.0-alpha upon completion.

**Risks / Dependencies**: Performance regression if topological sort is slow or graph is too deep.

**Milestone**: Production-ready system. Estimated Time: 2 days.
