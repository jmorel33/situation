# Situation Audio — Mixer Node

**Status:** Superseded & Archived — original DM2000-style mixer console was not implemented.
The sole surviving artifact is `SITUATION_NODE_MIXER`, a bus summing node in the audio node graph.
This document reflects reality as of v2.4.263.

**Original Version:** 1.1 (Expanded Features) — 2026-02-17
**Updated:** 2026-06-13 — rewritten to reflect what actually exists

---

## What Happened

The original plan described a fixed-function 16-track digital mixer console inspired by the
Yamaha DM2000, with a bespoke `SituationAudioMixer` struct, per-track channel strips, aux
buses, sidechain routing, and a binary session format (SMX2). That entire design was abandoned
in favour of the registry-driven audio node graph system (see `doc/plan/plan_audio_registry.md`).

The node graph architecture is strictly more capable: any topology is possible, all devices
are composable, and the mixer concept is expressed by simply instantiating one or more
`SITUATION_NODE_MIXER` nodes and patching sources into their inputs.

---

## What Exists: `SITUATION_NODE_MIXER`

A bus-summing utility node. Sums up to 16 stereo inputs to one stereo output, with a single
master gain control. Used to build sub-group buses, aux returns, and master mix points within
a `SituationAudioGraph`.

### Source files

| File | Role |
|------|------|
| `sit/aud/fx/mixer_node.h` | DSP core: `SituationMixerNodeState`, `situation_mixer_node_init()`, `situation_mixer_node_process()` |
| `sit/aud/registry_init.h` | Registers `SITUATION_NODE_MIXER` into the device registry (`_SituationRegisterMixer`) |
| `sit/aud/device_wrappers.h` | Wrapper triad: `_SituationCreateMixerNode`, `_SituationProcessMixerNodeNode`, `_SituationDestroyMixerNode`; entry in `g_device_function_table` |

### Node specification

| Property | Value |
|----------|-------|
| Type enum | `SITUATION_NODE_MIXER` |
| Category | `SITUATION_DEVICE_UTILITY` |
| Audio inputs | 16 stereo (each 2-channel) |
| Audio outputs | 1 stereo |
| Control inputs | 0 (but `master_gain` is patchable via control patch) |
| Control outputs | 0 |

### Controls

| Index | Name | Type | Range | Default | Description |
|-------|------|------|-------|---------|-------------|
| 0 | `master_gain` | Float | 0.0 – 4.0 | 1.0 | Overall bus level after summing |

### Signal flow

```
[Input port 0] ─┐
[Input port 1] ─┤
[Input port 2] ─┤  sum all connected ports  →  × master_gain  →  [Output port 0]
      ...        ┤
[Input port 15]─┘
```

Ports with no patch connected contribute nothing (their buffers are zeroed by the graph
before each block). The summing loop breaks on the first null buffer, so continuous port
assignment is assumed.

### Known quirk

`_SituationProcessMixerNodeNode` (in `device_wrappers.h`) uses `break` on a null input
buffer rather than `continue`. This means a gap in connected ports (e.g. inputs 0, 2 but
not 1) would silently drop inputs from port 2 onward. In practice the graph fills ports
from index 0 upward so this is benign today, but worth fixing if sparse patching is ever
needed.

---

## How to Use

Create a mixer node in a graph to sum multiple sources:

```c
// Create graph and nodes
SituationAudioGraph* graph = SituationCreateGraph();

SituationNodeHandle synth_a, synth_b, bus_mixer;
SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth_a);
SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth_b);
SituationCreateNode(graph, SITUATION_NODE_MIXER,      &bus_mixer);

// Patch both synths into the mixer
SituationCreatePatch(graph, synth_a, 0, bus_mixer, 0, false);  // synth A → input 0
SituationCreatePatch(graph, synth_b, 0, bus_mixer, 1, false);  // synth B → input 1

// Optional: set master gain
SituationSetControl(graph, bus_mixer, 0, 0.8f);

// Mixer output is unpatched → automatically routed to master output by the graph
```

Sub-group hierarchies work by chaining mixers:

```c
// Drums sub-bus → master mix bus
SituationNodeHandle drums_bus, master_bus;
SituationCreateNode(graph, SITUATION_NODE_MIXER, &drums_bus);
SituationCreateNode(graph, SITUATION_NODE_MIXER, &master_bus);

// ... patch drum sources into drums_bus inputs ...
SituationCreatePatch(graph, drums_bus, 0, master_bus, 0, false);  // sub-bus → master
```

### Session persistence

Graph state (including mixer nodes and their `master_gain` values) is persisted via the
node graph JSON serializer:

```c
// Save
SituationSaveGraphToFile(graph, "session.json");

// Load
SituationLoadGraphFromFile(graph, "session.json", device_funcs, num_device_funcs);
```

Format: JSON with `"version"`, `"sample_rate"`, `"nodes"` array (type + controls), and
`"patches"` array. Replaces the abandoned binary SMX2 format from the original plan.

---

## What Was Abandoned (for the record)

The following were designed but never implemented:

- `SituationAudioMixer` / `SituationAudioTrack` / `SituationAudioBus` structs
- `SituationCreateMixer` / `SituationDestroyMixer`
- `SituationAddTrack` / `SituationRemoveTrack`
- Per-track channel strip API (`SituationSetTrackEQ`, `SituationSetTrackDynamics`,
  `SituationSetTrackSideChain`, `SituationSetTrackVolume`, `SituationSetTrackPan`,
  `SituationSetTrackMute`, `SituationSetTrackSolo`)
- Aux send/routing API (`SituationSetTrackSend`, `SituationSetTrackOutput`)
- FX insert/remove API (`SituationInsertEffect`, `SituationRemoveEffect`)
- `SituationSaveMixerSession` / `SituationLoadMixerSession` (binary SMX2)
- Device enumeration/binding API (`SituationEnumerateAudioDevices`, etc.)
- `miniaudio` `ma_node_graph` backend

All of the above functionality is achievable today using the node graph API directly, with
greater flexibility. The channel-strip pattern (EQ → Dynamics → Panner → Gain → Mixer) maps
to a chain of `SITUATION_NODE_EQ_4BAND`, `SITUATION_NODE_DYNAMICS`, `SITUATION_NODE_PANNER`,
`SITUATION_NODE_GAIN`, and `SITUATION_NODE_MIXER` nodes connected with `SituationCreatePatch`.

The test file `tests/test_mixer_persistence.c` references the abandoned API and will not
compile. It can be deleted or rewritten against the node graph API.

---

## Related Files

- `doc/plan/plan_audio_registry.md` — the architecture that replaced this plan
- `sit/aud/node_graph.h` / `node_graph_impl.h` — graph container and node management
- `sit/aud/node_graph_process.h` — real-time processing loop
- `sit/aud/node_graph_serialization.h` / `node_graph_serialization_impl.h` — JSON persistence
- `sit/aud/device_registry.h` — device type registration
- `sit/aud/registry_init.h` — all 26 built-in device registrations
- `sit/aud/device_wrappers.h` — create/process/destroy wrappers and `g_device_function_table`
