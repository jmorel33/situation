# 09 — MIDI Control

**Tier 2 — Feature Spotlight** | OpenGL + Vulkan | **Best with MIDI hardware**

Lists available MIDI ports, routes note-on/off to a tone synth (or `SituationPlayToneEx` when no graph is connected), draws CC values as bars, and demonstrates MIDI Learn on reverb room size.

## Build & run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 09_midi_control
build\examples\09_midi_control.exe
```

Vulkan: replace `static-opengl` with `static-vulkan`.

## What you see

- **With a MIDI keyboard/controller:** notes play through `Tone Synth → Reverb`; CC16–19 move the reverb bars (default mapping).
- **Without hardware:** the window lists ports from `SituationListMidiDevices` and the on-screen piano still plays via `SituationPlayToneEx` + `SITUATION_MIDI_NOTE_FREQUENCY[]`.
- **Press `V`** to create a virtual MIDI loopback (same path tests use) for the full graph + learn demo without physical gear.

## Keys

| Key | Action |
|-----|--------|
| `Z`–`M`, `Q`–`U` row | On-screen piano (key glow + MIDI when connected) |
| `L` | Start MIDI Learn — wiggle a knob to assign CC → reverb **room size** |
| `V` | Virtual MIDI loopback (no hardware). **Also piano key C4 (MIDI 53)** — first press builds the graph and fires one C4; subsequent presses play normally through the graph. |
| Universal | `ESC`, `F11`, `F9`, `P`, `F12` via `sit_example.h` |

## APIs demonstrated

| API | Role |
|-----|------|
| `SituationListMidiDevices` | Enumerate PortMidi input/output ports |
| `SituationEnableMidiControl` | Attach a MIDI input to tone + reverb nodes |
| `SituationEnableMidiLearn` / `SituationStartMidiLearn` | Runtime CC → parameter mapping |
| `SituationIsLearning` | UI feedback during learn mode |
| `SITUATION_MIDI_NOTE_FREQUENCY[]` | MIDI note → Hz lookup (128 entries) |
| `SituationPlayToneEx` | Fallback tones when no graph/MIDI device |
| `SituationSetupVirtualMidiLoopback` | Software MIDI for CI and keyboard-only demos |

## Notes

- Plan docs may say `SituationEnumerateMidiDevices` / `SituationOpenMidiDevice` — the public API uses **`SituationListMidiDevices`** + **`SituationEnableMidiControl(graph, node, device_id)`** (or `-1` for auto-select).
- **14-bit CC:** some controllers send high-resolution CC; the engine normalizes 7-bit values. High-res devices often emit paired MSB/LSB messages — see PortMidi device docs if bars look stepped.
- **CC bar labels** show the control name (`room`, `damp`, `wet`, `dry`) rather than the CC number. The default mapping is CC16–19; after a MIDI Learn the CC assignment changes but the bar label stays accurate.
- Hardware note-on does not drive the on-screen key glow (MIDI is processed on the audio thread); the piano row is for local keyboard + virtual MIDI testing.
