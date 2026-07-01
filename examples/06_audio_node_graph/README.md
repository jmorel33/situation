# 06 — Audio Node Graph

**Tier:** Feature Spotlight  
**Backends:** OpenGL + Vulkan  
**Lines of code:** ~280 (excluding shared header)

## What you see

A live **ASCII signal-flow diagram** of a DAW-style processing chain:

```
Tone Synth → EQ 4-Band → Reverb → Mixer → OUTPUT
```

Parameter values update every frame. A pulsing arrow shows audio flow. The **topological sort order** is listed below the diagram.

You hear a continuous test tone routed through the full chain (EQ + reverb + master gain).

## Controls

| Key | Action |
|-----|--------|
| `Q` / `W` / `E` | Waveform: sine / square / saw |
| `1` / `2` / `3` | Frequency preset: 220 / 440 / 880 Hz |
| `UP` / `DOWN` | Reverb room size (live) |
| `SPACE` | Gate tone on / off |
| `S` | Save graph to `graph_session.json` |
| `L` | Load graph from `graph_session.json` |
| `C` | Try illegal feedback patch (cycle detection) |
| `ESC` | Quit (plus universal hotkeys from `sit_example.h`) |

## What it teaches

| Concept | API |
|---------|-----|
| Device registry | `SituationInitDeviceRegistry` |
| Create graph + nodes | `SituationCreateGraph`, `SituationCreateNode` |
| Wire nodes together | `SituationCreatePatch` |
| Live parameter changes | `SituationSetControl`, `SituationGetControl` |
| Route to speakers | `SituationSetActiveGraph`, `SituationResumeAudioDevice` |
| Processing order | `SituationTopologicalSort` |
| Persist session | `SituationSaveGraphToFile`, `SituationLoadGraphFromFile` |
| Cycle safety | `SituationCreatePatch` → `SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED` |
| Drive tone synth | Virtual MIDI: `SituationSetupVirtualMidiLoopback`, `SituationVirtualMidiNoteOn` |

## Why node graphs?

Most game audio APIs play clips or fire one-shot synth notes. Situation exposes a **patchable node graph** — the same model used in DAWs and modular synths — so you can build EQ → reverb → mixer chains, save them to JSON, and hot-reload at runtime.

miniaudio has a node graph, but Situation's is fully exposed: create nodes, patch ports, serialize, and inspect topology from C without a separate tool.

## Build and run

```bat
REM Prerequisites (once):
build\build_situation.bat static-opengl

REM Build:
build\build_examples.bat static-opengl 06_audio_node_graph

REM Run (writes graph_session.json next to the exe):
build\examples\06_audio_node_graph.exe
```

Vulkan variant:

```bat
build\build_situation.bat static-vulkan
build\build_examples.bat static-vulkan 06_audio_node_graph
```

Short name `audio_node_graph` also works.
