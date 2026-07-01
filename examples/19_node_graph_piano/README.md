# 19 — Node Graph Piano

**Tier 5 — Capstone** | OpenGL + Vulkan | Promoted from `examples/other/node_graph_piano_demo.c`

A **polyphonic software synthesizer** wired through a full audio node graph:

**Tone Synth → Overdrive → Chorus → Phaser → Echo → Reverb → Gain → Mixer**

Play with the QWERTY tracker layout (two octaves on Z-row + Q-row). Tweak ADSR, filter, sub-oscillator, LFO, and FX presets live.

## Build & run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 19_node_graph_piano
build\examples\19_node_graph_piano.exe
```

Short name: `node_graph_piano`

## Piano layout

| Row | Keys |
|-----|------|
| Lower | `Z S X D C V G B H N J M` (C3–B3 + C4) |
| Upper | `Q 2 W 3 E R 5 T 6 Y 7 U 8 I 9 O 0 P [ ]` |

## Key controls (summary)

| Key | Action |
|-----|--------|
| `F2` / `F3` | Octave down / up |
| `SPACE` | Sustain pedal |
| `TAB` | Cycle waveform |
| `F4`–`F8` | Filter mode / cutoff / resonance |
| `F11` | Cycle FX preset (Dry → All-in) |
| `KP_*` | Sub-osc + LFO controls |
| Arrows, `;`/`'`, `+`/`-` | ADSR |
| Universal | `ESC`, `F11`, `F9`, `P`, `F12` |

See the on-screen HUD for the full list.

## APIs demonstrated

| API | Role |
|-----|------|
| `SituationCreateGraph` / `SituationCreateNode` | Multi-node FX chain |
| `SituationCreatePatch` / `SituationTopologicalSort` | Wiring + execution order |
| `SituationSetupVirtualMidiLoopback` | Keyboard → tone synth |
| `SituationVirtualMidiNoteOn/Off` | Polyphonic note lifecycle |
| `SituationSetControl` | Live parameter updates |

## vs example 06

**06** teaches the graph API with a minimal chain. **19** is the instrument-grade capstone — same API, real-time performance use case.
