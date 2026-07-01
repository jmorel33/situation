## Audio Node Graph

**What this is:** A **patchable audio signal chain** in plain C — the same mental model as a DAW track rack or modular synth, but without a visual editor. You create **nodes** (reverb, EQ, synth, mixer…), connect **ports** with **patches**, turn **knobs** with `SituationSetControl`, and the audio device runs the graph every buffer automatically.

**You do not need this for simple playback.** Loading a WAV and pressing play stays in the [Audio Module](audio.md). Use the graph when you want **routing you can rewire**, **effects in series**, **live parameters**, **MIDI control**, or **JSON save/load** of a mix.

**Canonical examples:**
- `examples/06_audio_node_graph/` — ASCII signal-flow diagram, JSON session, cycle detection
- `examples/04_play_a_sound/` — full synth + FX chain driven by keyboard via virtual MIDI
- `examples/09_midi_control/` — hardware CC, MIDI Learn, preset files

**Also see:** [MIDI Integration](midi.md) (virtual loopback, learn mode) · [Audio Module](audio.md) (file playback, default graph)

---

### Coming from another audio graph?

If you've used one of these, here's the translation:

| You're used to… | Situation equivalent |
|-----------------|----------------------|
| **Web Audio** `AudioNode.connect()` | `SituationCreatePatch(graph, src, srcPort, dst, dstPort, false)` |
| **JUCE** `AudioProcessorGraph` | Same idea — nodes + audio wires; controls are explicit IDs |
| **Pure Data / Max** objects + cables | Code-first patching; no canvas (build your own UI if needed) |
| **miniaudio** node graph | Situation wraps a higher layer: typed devices, registry, MIDI, JSON |
| **FMOD/Wwise** event buses | Not the same — Situation exposes **raw** patch wires, not authored events |
| **VST hosts** | Built-in FX are native nodes (`SITUATION_NODE_REVERB`, etc.), not VST plugins |

**Situation-specific perks** (easy to miss):

| Perk | What it means for you |
|------|------------------------|
| **Default graph on init** | After audio starts, a minimal `Sound Source → Mixer` graph already runs — loaded sounds can flow through the mixer without you building anything |
| **No OUTPUT node** | Chain ends at any node whose output isn't patched elsewhere — that signal **automatically sums to the speakers** |
| **Auto sort on patch** | `CreatePatch` / `CreateNode` / `DestroyNode` call `TopologicalSort` for you — no manual reorder unless you want to verify |
| **Cycle rejection** | Feedback patches return `SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED` instead of blowing up at runtime |
| **Generational handles** | Stale node handles (after destroy) safely fail instead of corrupting the graph |
| **Device registry** | Every node type publishes port counts, control names, min/max, units, and defaults — introspect before wiring |
| **Implicit fan-in** | Multiple sources patched to one input **sum automatically** (like a mini-mixer on that port) |
| **Audio vs control wires** | Same patch API, `is_control=true` for modulation (LFO → cutoff) without touching audio buffers |
| **JSON sessions** | Save topology **and** knob values; reload at runtime (`graph_session.json` in example 06) |
| **Virtual MIDI built-in** | Drive the synth from C code or a keyboard — no loopMIDI / IAC driver install ([MIDI](midi.md)) |
| **PCM push from any thread** | `SituationPushNodePCM` is lock-free into a `PCM_INPUT` node |
| **~28 built-in devices** | Reverb, EQ, dynamics, tone synth (16-voice), meters, LFO, etc. — one registry |

---

### Mental model — five concepts

```
  ┌─────────────┐     audio patch      ┌─────────────┐
  │  Tone Synth │ ───────────────────► │   Reverb    │
  │  (source)   │                      │  (effect)   │
  └─────────────┘                      └──────┬──────┘
        │ control_values[]                    │ unpatched out
        │ (knobs + MIDI)                      ▼
        │                              ┌──────────────┐
        └──────────────────────────────│  Speakers    │
                                       │ (implicit)   │
                                       └──────────────┘
```

1. **Graph** — `SituationAudioGraph*` container holding nodes and patches.
2. **Node** — One device instance (synth, reverb, mixer…). Identified by a **generational handle**.
3. **Port** — Audio inputs/outputs (stereo buffers) and optional control inputs/outputs (floats).
4. **Patch** — Directed wire: `(src node, src port) → (dst node, dst port)`. Audio or control.
5. **Control** — Named parameter (room size, wet/dry, frequency…). Set from C, MIDI, or control patches.

Each audio callback, the engine:

1. Reads MIDI (if enabled per node) into `control_values[]`
2. Walks nodes in **topological order** (sources before effects)
3. **Sums** incoming audio patches into each node's input buffers
4. Calls each device's `process` function
5. **Sums unpatched node outputs** to the device buffer (stereo interleaved float)

---

### Two paths: default graph vs custom graph

#### Path A — Zero setup (most games)

When the audio device starts, Situation creates a **default graph**:

```
Sound Source ──► Mixer ──► (device output)
```

`SituationPlayLoadedSound` voices feed the **Sound Source** node. You never call `CreateGraph` unless you want custom routing. Details in [Audio Module — default graph](audio.md).

#### Path B — Custom chain (DAW-style)

You build your own graph, activate it, and optionally replace the default:

```c
SituationInitDeviceRegistry();   /* idempotent; also runs on first CreateNode */

SituationAudioGraph* graph = SituationCreateGraph();

SituationNodeHandle synth, reverb, gain;
SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
SituationCreateNode(graph, SITUATION_NODE_REVERB,     &reverb);
SituationCreateNode(graph, SITUATION_NODE_GAIN,       &gain);

SituationCreatePatch(graph, synth,  0, reverb, 0, false);  /* audio */
SituationCreatePatch(graph, reverb, 0, gain,   0, false);

SituationSetControl(graph, reverb, 0, 0.7f);   /* room size */
SituationSetControl(graph, gain,   0, 0.85f);  /* master */

SituationSetActiveGraph(graph);
SituationResumeAudioDevice();   /* if you paused audio during setup */
```

**No OUTPUT node exists.** Leave the last node's output unpatched — here `gain` feeds the speakers.

---

### Audio patches vs control patches

| | Audio patch `is_control=false` | Control patch `is_control=true` |
|---|-------------------------------|--------------------------------|
| **Carries** | Stereo PCM buffers | Single float (modulation) |
| **Example** | Synth → Reverb | LFO → Filter cutoff |
| **Fan-in** | Sums into input buffer | Overwrites control port value |

```c
/* Audio chain */
SituationCreatePatch(graph, synth, 0, filter, 0, false);

/* Modulation: LFO output 0 → filter control input 0 */
SituationCreatePatch(graph, lfo, 0, filter, 0, true);
```

---

### Built-in node types (registry)

All types are registered by `SituationInitDeviceRegistry()`. Query any type with `SituationGetDeviceMetadata()`.

| Category | Node types (examples) |
|----------|------------------------|
| **Sources** | `TONE_SYNTH` (16-voice poly synth), `SOUND_SOURCE` (sample voices), `PCM_INPUT` (streamed PCM) |
| **Effects** | `REVERB`, `ECHO`, `CHORUS`, `PHASER`, `OVERDRIVE`, `EQ_4BAND`, `FILTER`, `DYNAMICS`, `COMPANDER`, `STUDIO_REVERB`, `ISA110`, … |
| **Utility** | `GAIN`, `PANNER`, `MIXER` |
| **Modulators** | `LFO`, `ENVELOPE_FOLLOWER` |
| **Analyzers** | `SPECTRUM_ANALYZER`, `PEAK_METER` |
| **Capture** | `MIC_CAPTURE` |
| **Custom** | `SITUATION_NODE_CUSTOM = 1000` + `SituationRegisterDeviceType` |

Use `SituationGetRegisteredDeviceCount()` to see how many types are available at runtime.

---

### Discovering ports and knobs (don't guess IDs)

Every device publishes metadata when registered:

```c
SituationDeviceMetadata meta = {0};
if (SituationGetDeviceMetadata(SITUATION_NODE_REVERB, &meta) == SITUATION_SUCCESS) {
    printf("%s: %d audio in, %d audio out, %d controls\n",
           meta.name, meta.num_audio_ins, meta.num_audio_outs, meta.num_controls);
    for (int i = 0; i < meta.num_controls; i++) {
        printf("  [%u] %s  %.2f..%.2f %s  default=%.2f\n",
               meta.controls[i].id,
               meta.controls[i].name,
               meta.controls[i].min_value,
               meta.controls[i].max_value,
               meta.controls[i].units ? meta.controls[i].units : "",
               meta.controls[i].default_value);
    }
}
```

New nodes start with **defaults from metadata** — you only `SetControl` what you want to change.

Control IDs are **per device type** (e.g. reverb room size = `0`). Examples hard-code IDs as `#define` for readability; production code can read names from metadata or shared headers.

---

### How to get sound into the graph

| Goal | Approach |
|------|----------|
| **Polyphonic synth through FX** | `TONE_SYNTH` + virtual/hardware MIDI (`SituationVirtualMidiNoteOn`) — see example 04 |
| **One-shot samples** | Default graph `SOUND_SOURCE`, or custom graph with mixer |
| **Stream from another thread** | `PCM_INPUT` node + `SituationPushNodePCM` |
| **Live knob tweaks** | `SituationSetControl` from main thread each frame |
| **Hardware knobs** | `SituationEnableMidiControl` + optional [MIDI Learn](midi.md) |
| **Legacy tone API into graph** | `SituationSetToneRouting` + `SituationSetGraphSFXSource` |

**Important (example 04 lesson):** For the **tone synth through effects**, drive notes with **MIDI** (`VirtualMidiNoteOn/Off`), not `SituationPlayToneEx` alone — MIDI is what the synth node expects for polyphony, sustain pedal, and pitch bend.

```c
SituationSetupVirtualMidiLoopback(&midi_in);
SituationEnableMidiControl(graph, synth_node, midi_in);
SituationVirtualMidiNoteOn(60, 100);   /* middle C */
/* … later … */
SituationVirtualMidiNoteOff(60);
```

---

### One buffer through the graph (what happens at runtime)

```
  miniaudio callback (audio thread)
       │
       ▼
  SituationProcessGraph(active_graph, output, frameCount, …)
       │
       ├─ For each node in sorted order:
       │     Pm_Read (MIDI) → control_values[]
       │     zero inputs → sum audio patches → apply control patches
       │     device process_func(inputs, outputs, controls, frames)
       │
       └─ Sum all unpatched audio outputs → stereo output buffer
       │
       ▼
  speakers
```

**Silence until sorted:** If the graph has never been sorted (`sorted_count == 0`), output is silence until the first successful `TopologicalSort`. In practice, the first `CreateNode` / `CreatePatch` sorts for you.

**Threading:** Never call `CreateNode`, `DestroyNode`, `CreatePatch`, or `RemovePatch` from the audio thread. Only `SituationPushNodePCM` and virtual MIDI injection are designed for cross-thread use.

---

### Example 06 walkthrough — effect chain lab

Signal path:

```
Tone Synth → EQ 4-Band → Reverb → Mixer → (speakers)
```

| Key | Action |
|-----|--------|
| Q / W / E | Waveform: sine / square / saw |
| 1 / 2 / 3 | Frequency presets |
| UP / DOWN | Reverb room size (live) |
| SPACE | Gate tone on/off |
| S / L | Save / load `graph_session.json` |
| C | Try illegal feedback patch (cycle blocked) |

The on-screen diagram shows **topological order** — the same order the audio thread processes nodes.

---

### Example 04 walkthrough — synth + FX rack

Signal path:

```
Tone Synth → Overdrive → Chorus → Phaser → Echo → Reverb → Gain → (speakers)
```

Every piano key sends MIDI note-on/off through the **full chain**. Effects are bypassed via wet/dry controls (`mix = 0`), then enabled with presets. This is the reference for **musical** graph usage.

---

### Saving and loading graphs (JSON)

Persistence includes **node types**, **patches**, and **control values**:

```c
SituationSaveGraphToFile(graph, "my_mix.json");

/* Later — same process, new graph object or after edits */
SituationLoadGraphFromFile(graph, "my_mix.json", device_funcs, num_funcs);
```

`SituationSerializeGraphToJSON` / `DeserializeGraphFromJSON` for in-memory round-trips. Check compatibility with `SituationIsVersionCompatible`.

Custom device types need a `SituationDeviceFunctions` table passed to deserialize so the loader can recreate your nodes.

---

### PCM streaming node

For procedural audio, network streams, or decoder output from a worker thread:

```c
SituationNodeHandle pcm;
SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &pcm);
SituationCreatePatch(graph, pcm, 0, mixer, 1, false);

/* From any thread: */
uint32_t free_frames = SituationGetNodePCMFreeFrames(graph, pcm);
if (free_frames >= chunk_frames) {
    SituationPushNodePCM(graph, pcm, interleaved_floats, chunk_frames, 2);
}
```

On underrun the node outputs silence — use `GetNodePCMFreeFrames` for back-pressure.

---

### MIDI on the graph (summary)

Full detail in [MIDI Integration](midi.md). Minimal setup:

```c
int midi_in;
SituationSetupVirtualMidiLoopback(&midi_in);
SituationEnableMidiControl(graph, reverb_node, midi_in);
/* or */ SituationAutoConnectMidi(graph, reverb_node);

SituationEnableMidiLearn(graph, reverb_node);
SituationStartMidiLearn(graph, reverb_node, control_id, "Room Size", 0.0f, 1.0f, 0);
/* wiggle a hardware knob — mapping saved with SituationSaveMidiPreset */
```

MIDI is processed **inside** `SituationProcessGraph` on the audio thread — no manual polling in your game loop.

---

### Custom devices (extension point)

Register a new node type for your own DSP:

```c
SituationDeviceMetadata meta = {0};
meta.type = SITUATION_NODE_CUSTOM;
meta.category = SITUATION_DEVICE_EFFECT;
/* fill ports, controls, create/process/destroy function pointers … */
SituationRegisterDeviceType(&meta);
```

Then `SituationCreateNode(graph, SITUATION_NODE_CUSTOM, &handle)` works like built-ins. Serialization requires supplying `SituationDeviceFunctions` on load.

---

### Common recipes

#### A — Master bus with reverb send

```
Synth ──► Gain ──► (speakers)
   └──► Reverb ──► Mixer ──► (speakers)
```

Patch synth to both dry gain and reverb; mixer sums wet return.

#### B — Live parameter animation

```c
float t = (float)SituationTimerGetTime();
SituationSetControl(graph, filter, CUTOFF_ID, 200.0f + 1800.0f * (0.5f + 0.5f * sinf(t)));
```

Safe from main thread — single-float writes are atomic for the audio thread.

#### C — Swap entire mix at level load

```c
SituationAudioGraph* level_graph = SituationCreateGraph();
build_level_chain(level_graph);
SituationSetActiveGraph(level_graph);
/* previous graph stays allocated; destroy when unused */
```

Only **one** active graph mixes at a time.

#### D — Inspect before patching

Always check `num_audio_ins/outs` in metadata before assuming port `0` exists. Some devices are stereo-in/stereo-out; others mono.

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Silence | No `SetActiveGraph`, or graph unsorted | Activate graph; ensure at least one `CreatePatch` succeeded |
| Silence after custom build | Audio paused | `SituationResumeAudioDevice()` |
| Patch fails with cycle error | Feedback loop | Intended — rewire; see example 06 key `C` |
| Patch fails port invalid | Wrong port index | Query `SituationGetDeviceMetadata` |
| Stale node after destroy | Generational handle | Store new handle from `CreateNode`; old handle returns NULL from `GetNode` |
| Synth works but no FX | Playing via legacy tone API | Use MIDI into `TONE_SYNTH`, or route through graph sound source |
| MIDI no effect | MIDI not enabled on node | `SituationEnableMidiControl` + correct device id |
| JSON load fails | Version mismatch or missing custom device funcs | Check `SituationIsVersionCompatible`; pass device table |
| PCM clicks/gaps | Ring underrun/overrun | Monitor `GetNodePCMFreeFrames`; match push rate to callback |
| Controls snap wrong | Wrong control ID | Read metadata; IDs are per device type, not global |

---

### API reference

#### Graph lifecycle

```c
SituationAudioGraph* SituationCreateGraph(void);
void SituationDestroyGraph(SituationAudioGraph* graph);
SituationError SituationCreateNode(SituationAudioGraph* g, SituationNodeType type, SituationNodeHandle* out);
SituationError SituationDestroyNode(SituationAudioGraph* g, SituationNodeHandle h);
SituationNode* SituationGetNode(SituationAudioGraph* g, SituationNodeHandle h);
SituationError SituationTopologicalSort(SituationAudioGraph* g);  /* usually automatic */
```

#### Patching

```c
SituationError SituationCreatePatch(SituationAudioGraph* g,
    SituationNodeHandle src, int src_port,
    SituationNodeHandle dst, int dst_port,
    bool is_control);
SituationError SituationRemovePatch(/* same args */);
```

#### Controls & active graph

```c
SituationError SituationSetControl(SituationAudioGraph* g, SituationNodeHandle h, uint32_t id, float val);
SituationError SituationGetControl(SituationAudioGraph* g, SituationNodeHandle h, uint32_t id, float* out);
SituationError SituationSetActiveGraph(SituationAudioGraph* g);
SituationAudioGraph* SituationGetActiveGraph(void);
```

#### PCM input

```c
uint32_t SituationPushNodePCM(SituationAudioGraph* g, SituationNodeHandle node,
    const float* samples, uint32_t frame_count, uint32_t channels);
uint32_t SituationGetNodePCMFreeFrames(SituationAudioGraph* g, SituationNodeHandle node);
```

#### Registry & metadata

```c
void SituationInitDeviceRegistry(void);
SituationError SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out);
int SituationGetRegisteredDeviceCount(void);
SituationError SituationRegisterDeviceType(const SituationDeviceMetadata* meta);
char* SituationGetCategoryName(SituationDeviceCategory cat);  /* caller frees */
```

#### Serialization

```c
SituationError SituationSaveGraphToFile(const SituationAudioGraph* g, const char* path);
SituationError SituationLoadGraphFromFile(SituationAudioGraph* g, const char* path,
    const SituationDeviceFunctions* funcs, int n);
char* SituationSerializeGraphToJSON(const SituationAudioGraph* g);
void SituationFreeJSONString(char* json);
```

#### Tone / SFX routing (legacy bridge)

```c
SituationError SituationSetToneRouting(SituationToneHandle h, bool route_to_graph);
SituationError SituationSetGraphSFXSource(SituationNodeHandle sound_source);
```

#### MIDI (see [midi.md](midi.md))

`SituationEnableMidiControl`, `SituationAutoConnectMidi`, `SituationSetupVirtualMidiLoopback`, `SituationEnableMidiLearn`, `SituationStartMidiLearn`, `SituationSaveMidiPreset`, …

---

### Related documentation

- [Audio Module](audio.md) — file playback, device selection, default graph policy
- [MIDI Integration](midi.md) — virtual MIDI, learn mode, CC maps, threading
- `examples/06_audio_node_graph/README.md` — build/run for the lab demo
- `examples/04_play_a_sound/main.c` — full synth control ID reference in comments
