## MIDI Integration Module _(v2.4.150+)_

**Overview:** Situation ships a hybrid MIDI stack — real hardware (WinMM on Windows) plus a fully user-space **virtual MIDI router** that needs no OS loopback driver. The public API wires MIDI into the [Audio Node Graph](audio_graph.md): one call enables CC and note control on any supported effect or the 16-voice tone synth, with optional **MIDI Learn** for runtime mapping and JSON preset save/load.

**Why the virtual system matters:** Most platforms either require installing a third-party virtual cable (loopMIDI, IAC Driver) or cannot loop back at all in CI/headless builds. Situation's virtual layer lives entirely inside `sit/aud/midi.h`: lock-free ring buffers, a connection router, and non-blocking writes safe for the audio thread. Your game, test harness, or on-screen piano can inject note/CC events programmatically and they arrive at the same `Pm_Read()` path hardware uses — no special-case code in the graph processor.

**Canonical examples:**
- `examples/09_midi_control/` — hardware or virtual loopback, CC bars, MIDI Learn
- `examples/04_play_a_sound/` — on-screen piano via virtual MIDI → tone synth
- `examples/06_audio_node_graph/` — virtual note-on drives the signal chain

**Deep-dive references** (implementation detail beyond this guide):
- [`doc/midi_api.md`](../midi_api.md) — full API reference and timing analysis
- [`doc/misc/MIDI_SYSTEM_OVERVIEW.md`](../misc/MIDI_SYSTEM_OVERVIEW.md) — layer diagram and CC allocation map
- [`doc/misc/MIDI_CC_REFERENCE.md`](../misc/MIDI_CC_REFERENCE.md) — per-device CC tables
- [`doc/misc/MIDI_14BIT_SUPPORT.md`](../misc/MIDI_14BIT_SUPPORT.md) — MSB/LSB high-resolution CC

---

### Architecture

MIDI flows through five layers. The virtual router sits at layer 2 alongside hardware PortMidi streams.

```
  Hardware keyboard          Your C code (VirtualMidi*)
         │                            │
         └──────────┬─────────────────┘
                    ▼
         ┌──────────────────────┐
         │  sit/aud/midi.h      │  Hybrid PortMidi API
         │  • WinMM hardware    │  • Virtual devices (always on)
         │  • SPSC ring buffers │  • Router (many-to-many)
         └──────────┬───────────┘
                    │ PmEvent stream
                    ▼
         ┌──────────────────────┐
         │  Node graph (audio    │  SituationProcessGraph()
         │  callback thread)    │  Pm_Read → dispatch callbacks
         └──────────┬───────────┘
                    │ writes control_values[]
                    ▼
         ┌──────────────────────┐
         │  midi_device_        │  CC → parameter mapping
         │  callbacks.h         │  (17 device types)
         └──────────┬───────────┘
                    ▼
         ┌──────────────────────┐
         │  FX DSP (reverb,     │  Pure audio — no MIDI
         │  filter, synth…)     │  knowledge
         └──────────────────────┘
```

**Thread model:** `SituationEnableMidiControl()` opens a `PmStream` on the main thread. Each audio buffer, `SituationProcessGraph()` calls `Pm_Read()` on that stream and dispatches events. Callbacks write into the node's `control_values[]` float array; the node's `process` function reads those values on the same audio thread. Single-float writes are atomic — no locks on the hot path.

**Graph topology** (`CreateNode`, `CreatePatch`, etc.) must stay on the **main thread**. Only `SituationPushNodePCM` and virtual MIDI injection are safe from other threads (virtual writes are lock-free).

---

### Virtual MIDI System

Situation's virtual MIDI is not a thin wrapper around OS virtual cables. It is a complete in-process routing fabric built into `midi.h`.

#### What gets created

`SituationSetupVirtualMidiLoopback()` performs these steps atomically:

1. **`Pm_Initialize()`** — lazy-init on first use.
2. **`Pm_CreateVirtualDevice("Situation Test MIDI Out", output)`** — a virtual output port.
3. **`Pm_CreateVirtualDevice("Situation Test MIDI In", input)`** — a virtual input port.
4. **`Pm_ConnectVirtualDevices(out, in)`** — registers a router entry so writes to Out appear on In.
5. **`Pm_OpenOutput(out_stream)`** — holds the stream used by `SituationVirtualMidi*()`.

The returned **input device ID** is a real PortMidi device index — pass it to `SituationEnableMidiControl(graph, node, device_id)` exactly as you would a physical keyboard.

Device names are defined as `SITUATION_VIRTUAL_MIDI_OUT_NAME` / `SITUATION_VIRTUAL_MIDI_IN_NAME` in the API header. `SituationListMidiDevices()` will show them alongside hardware ports.

#### How messages travel (the loopback trick)

When you call `SituationVirtualMidiNoteOn(60, 100)`:

```
SituationVirtualMidiNoteOnEx()
  → _SituationVirtualMidiWrite(Pm_Message(0x90|ch, note, vel))
    → Pm_Write(out_stream, &event, 1)          // virtual output stream
      → Pm_WriteVirtual()                       // lock-free SPSC ring (8192 slots)
      → Router: for each active connection where source == out_dev:
           → write directly into destination virtual input buffer
             (optional filter + transform per connection)
```

Later, on the **audio thread** inside `SituationProcessGraph()`:

```
Pm_Read(node->midi_input, events, 32)           // reads from virtual In buffer
  → channel filter (SituationSetNodeMidiChannel)
  → MIDI Learn intercept (if active)
  → device callback (e.g. _SituationReverbOnControlChange)
    → node->control_values[i] = normalized_value
  → node's process() reads control_values → DSP
```

Hardware MIDI follows the identical `Pm_Read` → callback path; only the producer differs (WinMM callback vs virtual router).

#### Lock-free SPSC buffers

Virtual devices use **single-producer, single-consumer** ring buffers:

- **8192 events** per device (`VIRTUAL_BUFFER_SIZE`, power-of-two for fast modulo).
- **`_Atomic uint32_t`** read/write positions with cache-line padding.
- **No mutex** on read/write — safe between a main-thread producer and audio-thread consumer.
- Overflow returns `pmBufferOverflow`; the harness tests stay well under capacity.

This is the core "trickery" that makes internal routing work without stalling the audio callback.

#### Router, filters, and transforms

`Pm_ConnectVirtualDevices()` registers entries in a global `PmRouter` (up to 128 connections). Each connection can optionally attach:

| Feature | Purpose |
|---------|---------|
| `PmFilter` | Block/pass message types (note on/off, CC, pitch bend…) per channel bitmask |
| `PmTransform` | Transpose semitones, remap channels, scale velocity with linear/exp/log curves |

The public loopback API uses a plain connection (no filter). Advanced routing is available at the `midi.h` layer for sequencers, arpeggiators, or multi-target fan-out.

#### Hardware vs virtual timing (critical difference)

| | Hardware MIDI | Virtual MIDI |
|---|---------------|--------------|
| **Write behavior** | May block with `Sleep()` to honor timestamps | Returns immediately (non-blocking) |
| **Real-time safe** | No — do not write from audio thread | Yes — designed for audio-thread routing |
| **Use case** | External keyboards, hardware synths | In-app piano, tests, sequencers, CI |

Virtual MIDI **preserves timestamps** in the event struct but delivers immediately. The audio callback is responsible for scheduling — which matches how `SituationProcessGraph` already batches MIDI per buffer.

#### Lifecycle

```c
int midi_in = -1;

// Setup once (idempotent — second call returns same device ID)
SituationSetupVirtualMidiLoopback(&midi_in);

// Attach to nodes like any hardware port
SituationEnableMidiControl(graph, reverb_node, midi_in);
SituationEnableMidiControl(graph, synth_node, midi_in);

// Inject from main thread, UI, or tests
SituationVirtualMidiNoteOnEx(0, 60, 100);
SituationVirtualMidiControlChange(0, 91, 80);   // reverb level CC
SituationVirtualMidiNoteOffEx(0, 60);

// Shutdown
SituationTeardownVirtualMidiLoopback();  // also call from test harness teardown
```

`SituationTeardownVirtualMidiLoopback()` closes the output stream, disconnects the router entry, and destroys both virtual devices. Always pair setup/teardown in tests to avoid leaking device slots.

---

### Hardware MIDI Path

On Windows, `midi.h` enumerates WinMM devices and receives input via `midiInOpen` callbacks into a mutex-protected ring buffer. `SituationListMidiDevices()` wraps `Pm_CountDevices()` / `Pm_GetDeviceInfo()` and returns human-readable names for UI port lists.

**Auto-select:** `SituationEnableMidiControl(graph, node, -1)` or `SituationAutoConnectMidi()` picks the first available input that is not already opened.

**Channel filtering:** `SituationSetNodeMidiChannel(graph, node, ch)` restricts input to one channel (0–15). Pass `-1` for omni (all channels).

---

### Node Graph Integration

Enabling MIDI on a node is a one-shot setup — the library handles callbacks, device identity, and cleanup:

```c
SituationAudioGraph* graph = SituationCreateGraph();
SituationNodeHandle reverb;
SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
SituationSetActiveGraph(graph);

// Hardware: device_id from SituationListMidiDevices, or -1 for auto
SituationEnableMidiControl(graph, reverb, -1);

// Or virtual loopback (see above)
int virt_in;
SituationSetupVirtualMidiLoopback(&virt_in);
SituationEnableMidiControl(graph, reverb, virt_in);
```

**What `SituationEnableMidiControl` does internally:**
1. Looks up the node type in `SIT_GetMidiCallbackForDevice()` (`midi_device_callbacks.h`).
2. Creates an `SIT_MidiDevice` with the correct note/CC callbacks.
3. For `SITUATION_NODE_TONE_SYNTH`, allocates a `SituationToneSynthMidiCtx` (polyphonic voice state separate from the control array).
4. Opens `Pm_OpenInput(device_id)` and stores the stream on the node.
5. Sets Universal Device Inquiry identity (SysEx `F0 7E … 06 02 …`) so external tools can identify Situation nodes.

**During playback:** Each call to `SituationProcessGraph()` reads pending MIDI for every enabled node before running that node's DSP. Learn mode intercepts CC messages before the hardcoded device callback runs.

**Cleanup:** `SituationDestroyNode()` and `SituationDisableMidiControl()` close the PortMidi stream and free synth MIDI context automatically.

---

### Built-in CC Mappings

Every FX node type ships with pre-defined CC → control mappings in `midi_device_callbacks.h`. Examples:

| Device | CC range | Parameters |
|--------|----------|------------|
| Reverb | 91–95 | room, damp, wet, dry, width |
| Filter | 40–45 | type, cutoff, Q, gain, drive |
| EQ 4-Band | 46–57 | per-band freq/gain/Q |
| Dynamics | 70–76 | threshold, ratio, attack, release… |
| Tone Synth | Notes + CC | 16-voice polyphony, pitch bend, program change |

Full tables: [`doc/misc/MIDI_CC_REFERENCE.md`](../misc/MIDI_CC_REFERENCE.md).

**14-bit CC:** Controllers that send MSB (CC 0–31) + LSB (CC 32–63) pairs are merged into 14-bit values (0–16383) before normalization — smooth sweeps without zipper noise. See [`doc/misc/MIDI_14BIT_SUPPORT.md`](../misc/MIDI_14BIT_SUPPORT.md).

**Note frequencies:** `SITUATION_MIDI_NOTE_FREQUENCY[128]` in the API provides MIDI note → Hz lookup for procedural tones when no graph is connected.

---

### MIDI Learn

Runtime mapping overrides the hardcoded CC table for a specific control:

```c
SituationEnableMidiControl(graph, reverb, midi_in);
SituationEnableMidiLearn(graph, reverb);

// User wiggles a knob — next CC captured (5 s timeout)
SituationStartMidiLearn(graph, reverb,
    REVERB_ROOM_SIZE, "room", 0.0f, 1.0f, 0);  // scaling: 0=linear

if (SituationIsLearning(graph, reverb)) {
    // show "listening…" UI
}

SituationSaveMidiPreset(graph, reverb, "reverb_mappings.json");
SituationLoadMidiPreset(graph, reverb, "reverb_mappings.json");
```

**Scaling modes:** `0` linear, `1` logarithmic, `2` decibel, `3` discrete.

Learn requires MIDI to already be enabled. While learning, incoming CC is captured by `SIT_MidiLearn_ProcessCC()` before the device-specific callback runs.

---

### Quick Start Recipes

#### A — Hardware controller → reverb

```c
SituationInitDeviceRegistry();
SituationAudioGraph* g = SituationCreateGraph();
SituationNodeHandle reverb, out;
SituationCreateNode(g, SITUATION_NODE_REVERB, &reverb);
SituationCreateNode(g, SITUATION_NODE_OUTPUT, &out);
SituationCreatePatch(g, reverb, 0, out, 0, false);
SituationSetActiveGraph(g);
SituationResumeAudioDevice();

SituationEnableMidiControl(g, reverb, -1);  // first available input
// Turn CC 91 on your controller → reverb room size moves
```

#### B — On-screen piano without hardware (virtual loopback)

```c
int midi_in;
SituationSetupVirtualMidiLoopback(&midi_in);

SituationNodeHandle synth;
SituationCreateNode(g, SITUATION_NODE_TONE_SYNTH, &synth);
SituationEnableMidiControl(g, synth, midi_in);

// UI key press:
SituationVirtualMidiNoteOnEx(0, 60, 100);
// key release:
SituationVirtualMidiNoteOffEx(0, 60);
```

#### C — Test harness / CI (same path as production)

The audio test harness (`tests/harness/test_tone_synth.c`) uses virtual loopback exclusively — hundreds of CC/note assertions without physical MIDI. Pattern:

```c
int midi_in;
SIT_ASSERT_EQ(SituationSetupVirtualMidiLoopback(&midi_in), SITUATION_SUCCESS);
SIT_ASSERT_EQ(SituationEnableMidiControl(graph, node, midi_in), SITUATION_SUCCESS);
SituationVirtualMidiNoteOnEx(0, 69, 100);
// ... assert audio output ...
SituationTeardownVirtualMidiLoopback();
```

---

### Structs

#### `SituationMidiDeviceInfo`
```c
typedef struct {
    int device_id;              // Pass to SituationEnableMidiControl
    char device_name[128];      // Port name (includes virtual devices)
    int is_input;               // 1 if input device
    int is_output;              // 1 if output device
} SituationMidiDeviceInfo;
```

---

### API Reference — Device Control

---
#### `SituationListMidiDevices`
Enumerates all PortMidi devices — hardware **and** virtual loopback ports.

```c
int SituationListMidiDevices(SituationMidiDeviceInfo* devices, int max_count);
```

**Returns:** Number of devices found (0 if none).

**Usage Example:**
```c
SituationMidiDeviceInfo ports[16];
int n = SituationListMidiDevices(ports, 16);
for (int i = 0; i < n; i++) {
    if (ports[i].is_input)
        printf("[%d] IN  %s\n", ports[i].device_id, ports[i].device_name);
}
```

---
#### `SituationEnableMidiControl`
Attaches a MIDI input to a graph node. Auto-selects callbacks from node type. Pass `device_id = -1` for first available input.

```c
SituationError SituationEnableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle, int device_id);
```

---
#### `SituationDisableMidiControl`
Closes the PortMidi stream and destroys the node's MIDI device wrapper.

```c
SituationError SituationDisableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationAutoConnectMidi`
Shorthand for `SituationEnableMidiControl(graph, handle, -1)`.

```c
SituationError SituationAutoConnectMidi(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationIsMidiEnabled`
Returns 1 if the node has an active MIDI input stream.

```c
int SituationIsMidiEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationSetNodeMidiChannel`
Restricts input to one MIDI channel (0–15), or `-1` for omni.

```c
SituationError SituationSetNodeMidiChannel(SituationAudioGraph* graph, SituationNodeHandle handle, int channel);
```

---
#### `SituationGetMidiDeviceName`
Gets the PortMidi name string for a device ID.

```c
SituationError SituationGetMidiDeviceName(int device_id, char* out_name, size_t out_name_size);
```

---

### API Reference — Virtual MIDI Loopback

These functions create and drive the in-process virtual cable. Messages injected here traverse the same router → `Pm_Read` → callback path as hardware.

---
#### `SituationSetupVirtualMidiLoopback`
Creates `"Situation Test MIDI Out"` → `"Situation Test MIDI In"`, connects them in the router, and opens the output stream for injection.

```c
SituationError SituationSetupVirtualMidiLoopback(int* out_input_device_id);
```

**Returns:** Input device ID suitable for `SituationEnableMidiControl()`. Idempotent — safe to call twice.

---
#### `SituationVirtualMidiNoteOnEx` / `SituationVirtualMidiNoteOffEx`
Channel-aware note on/off (channel 0–15). Note-on with velocity 0 is coerced to velocity 1.

```c
SituationError SituationVirtualMidiNoteOnEx(uint8_t channel, uint8_t note, uint8_t velocity);
SituationError SituationVirtualMidiNoteOffEx(uint8_t channel, uint8_t note);
```

---
#### `SituationVirtualMidiNoteOn` / `SituationVirtualMidiNoteOff`
Legacy wrappers for channel 0.

```c
SituationError SituationVirtualMidiNoteOn(uint8_t note, uint8_t velocity);
SituationError SituationVirtualMidiNoteOff(uint8_t note);
```

---
#### `SituationVirtualMidiControlChange`
Injects a CC message (mod wheel, expression, device-mapped CCs, etc.).

```c
SituationError SituationVirtualMidiControlChange(uint8_t channel, uint8_t controller, uint8_t value);
```

---
#### `SituationVirtualMidiPitchBend`
Pitch bend: range 0–16383, center = 8192. Routed to tone synth pitch-bend handler when node type is `SITUATION_NODE_TONE_SYNTH`.

```c
SituationError SituationVirtualMidiPitchBend(uint8_t channel, int16_t bend);
```

---
#### `SituationVirtualMidiProgramChange`
Program change on a channel (tone synth preset selection).

```c
SituationError SituationVirtualMidiProgramChange(uint8_t channel, uint8_t program);
```

---
#### `SituationTeardownVirtualMidiLoopback`
Closes the output stream, disconnects router, destroys virtual devices. Call at shutdown and in test teardown.

```c
void SituationTeardownVirtualMidiLoopback(void);
```

**Complete virtual MIDI example:**
```c
int midi_in = -1;
SituationSetupVirtualMidiLoopback(&midi_in);
SituationEnableMidiControl(graph, synth, midi_in);

SituationVirtualMidiNoteOnEx(0, 60, 100);
SituationSleep(500);
SituationVirtualMidiControlChange(0, 1, 64);    // mod wheel
SituationVirtualMidiNoteOffEx(0, 60);

SituationTeardownVirtualMidiLoopback();
```

---

### API Reference — MIDI Learn

---
#### `SituationEnableMidiLearn` / `SituationDisableMidiLearn`
Enable or disable learn capability on a node (MIDI must already be enabled).

```c
SituationError SituationEnableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
SituationError SituationDisableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationStartMidiLearn`
Arms learn mode: the next CC received maps to the specified control. Times out after 5 seconds.

```c
SituationError SituationStartMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle,
    int control_index, const char* param_name,
    float min_value, float max_value, int scaling);
```

**Scaling:** `0` linear, `1` log, `2` dB, `3` discrete.

---
#### `SituationCancelMidiLearn`
Cancels an active learn operation.

```c
SituationError SituationCancelMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationIsLearning` / `SituationIsMidiLearnEnabled`
Query learn state for UI feedback.

```c
int SituationIsLearning(SituationAudioGraph* graph, SituationNodeHandle handle);
int SituationIsMidiLearnEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationClearMidiMapping` / `SituationClearAllMidiMappings`
Remove learned mappings.

```c
SituationError SituationClearMidiMapping(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index);
SituationError SituationClearAllMidiMappings(SituationAudioGraph* graph, SituationNodeHandle handle);
```

---
#### `SituationSaveMidiPreset` / `SituationLoadMidiPreset`
Persist learned CC mappings to/from JSON.

```c
SituationError SituationSaveMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);
SituationError SituationLoadMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);
```

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `SITUATION_ERROR_MIDI_NO_DEVICES` | No hardware and loopback not set up | Call `SituationSetupVirtualMidiLoopback()` first |
| `SITUATION_ERROR_MIDI_NOT_SUPPORTED` | Node type has no callback entry | Check `SIT_DeviceSupportsMidi(type)` |
| CC moves wrong parameter | Hardcoded mapping vs Learn conflict | Clear mappings or use Learn to override |
| Notes silent with virtual MIDI | Graph not active or synth not patched to output | `SituationSetActiveGraph`, verify patches to `OUTPUT` |
| Stepped CC bars | 7-bit controller | Normal; 14-bit MSB/LSB pairs give smoother resolution |
| Virtual device missing from list | Loopback not created yet | `SituationSetupVirtualMidiLoopback()` before `ListMidiDevices` |

**API naming note:** Older plan docs may reference `SituationEnumerateMidiDevices` / `SituationOpenMidiDevice`. The shipping API uses **`SituationListMidiDevices`** + **`SituationEnableMidiControl(graph, node, device_id)`**.
