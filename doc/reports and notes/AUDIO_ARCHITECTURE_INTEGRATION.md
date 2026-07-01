# Audio Architecture Integration Guide

**Date**: 2026-03-02  
**Library Version**: v2.4.0-alpha  
**Status**: 🔄 TWO SYSTEMS COEXISTING

## Overview

Situation currently has **TWO audio systems** that serve different purposes:

1. **Traditional Mixer System** - Built on miniaudio node graph (channel strips, buses, aux sends)
2. **Modular Node Graph System** - Registry-based device patching (effects, synthesis, routing)

## Current State

### System 1: Traditional Mixer (Production-Ready)

**Location**: `situation_impl_audio.h`  
**Based On**: miniaudio `ma_node_graph`  
**Purpose**: Traditional DAW-style mixing console

**Architecture**:
```
┌─────────────────────────────────────────────────────────────┐
│                    TRADITIONAL MIXER                         │
│                                                              │
│  Track 1 ──┬─→ [EQ] ─→ [Dynamics] ─→ [Pan] ─→ Master      │
│            └─→ Aux Send 1 ─→ [Reverb Bus] ──┘              │
│                                                              │
│  Track 2 ──┬─→ [EQ] ─→ [Dynamics] ─→ [Pan] ─→ Master      │
│            └─→ Aux Send 2 ─→ [Delay Bus] ───┘              │
│                                                              │
│  Master ───→ [Output]                                       │
└─────────────────────────────────────────────────────────────┘
```

**Features**:
- ✅ 16 tracks with channel strips
- ✅ 8 aux buses with FX slots
- ✅ Per-track EQ (4-band parametric)
- ✅ Per-track dynamics (compressor/limiter/gate)
- ✅ Pre/post-fader aux sends
- ✅ Sidechain routing
- ✅ Pan and volume control
- ✅ Metering (peak levels)
- ✅ Solo/mute functionality

**APIs**:
```c
// Mixer management
SituationAudioMixer* mixer = SituationCreateMixer();
SituationDestroyMixer(mixer);

// Track operations
int track_id = SituationAddTrack(mixer, "Vocals");
SituationSetTrackVolume(mixer, track_id, 0.8f);
SituationSetTrackPan(mixer, track_id, 0.0f);
SituationSetTrackEQ(mixer, track_id, &eq_params);

// Aux sends
SituationSetAuxSend(mixer, track_id, aux_bus_id, level, pre_fader);

// Bus operations
int bus_id = SituationAddAuxBus(mixer, "Reverb");
SituationSetBusVolume(mixer, bus_id, 0.7f);
```

**Limitations**:
- ❌ Fixed signal flow (can't reroute)
- ❌ Limited to built-in effects
- ❌ No custom device support
- ❌ No arbitrary patching
- ❌ No modulation routing

---

### System 2: Modular Node Graph (New, 80% Complete)

**Location**: `sit/aud/node_graph*.h`, `sit/aud/device_*.h`  
**Based On**: Custom registry + topological sort  
**Purpose**: Modular synthesis and arbitrary signal routing

**Architecture**:
```
┌─────────────────────────────────────────────────────────────┐
│                  MODULAR NODE GRAPH                          │
│                                                              │
│  [Tone Synth] ──→ [Filter] ──→ [Reverb] ──→ [Output]       │
│       ↓                ↑                                     │
│       └────→ [LFO] ────┘  (modulation)                      │
│                                                              │
│  [Mic Input] ──→ [Overdrive] ──→ [Delay] ──→ [Output]      │
│                                                              │
│  [Sound Source] ──→ [Chorus] ──→ [Maximizer] ──→ [Output]  │
└─────────────────────────────────────────────────────────────┘
```

**Features**:
- ✅ 19 registered devices (effects, sources, modulators)
- ✅ Arbitrary patching (any output → any input)
- ✅ Modulation routing (LFO → filter cutoff, etc.)
- ✅ Topological sort (automatic evaluation order)
- ✅ Cycle detection (prevents feedback loops)
- ✅ Thread-safe operations (lock-free audio processing)
- ✅ JSON serialization (save/load graphs)
- ✅ Custom device registration (extensible)
- ✅ Implicit summing (multiple sources → one input)
- ✅ Implicit splitting (one source → multiple destinations)

**APIs**:
```c
// Graph management
SituationThreadSafeGraph* graph = SituationCreateThreadSafeGraph();
SituationDestroyThreadSafeGraph(graph);

// Node operations
SituationNodeHandle tone_handle;
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, &tone_handle);
SituationDestroyNodeThreadSafe(graph, tone_handle);

// Patching
SituationCreatePatchThreadSafe(graph, src_handle, src_port, dst_handle, dst_port, false);
SituationRemovePatchThreadSafe(graph, src_handle, src_port, dst_handle, dst_port, false);

// Control updates (lock-free)
SituationSetNodeControlThreadSafe(graph, handle, control_id, value);
SituationGetNodeControlThreadSafe(graph, handle, control_id, &value);

// Processing (lock-free)
SituationProcessGraphThreadSafe(graph, output_buffer, frames, device_funcs, num_funcs);

// Serialization
SituationSaveGraphToFile(graph, "my_patch.json");
SituationLoadGraphFromFile(graph, "my_patch.json");
```

**Limitations**:
- ⏳ Not integrated with mixer yet
- ⏳ No built-in master output routing
- ⏳ No metering yet
- ⏳ No UI integration yet

---

## Integration Strategy

### Current Situation

The two systems are **independent** right now:

```
┌──────────────────────┐     ┌──────────────────────┐
│  Traditional Mixer   │     │  Modular Node Graph  │
│  (miniaudio based)   │     │  (registry based)    │
│                      │     │                      │
│  ✅ Production Ready │     │  ✅ 80% Complete     │
│  ❌ Not Extensible   │     │  ⏳ Not Integrated   │
└──────────────────────┘     └──────────────────────┘
```

### Integration Options

#### Option 1: Parallel Systems (Current)

**Use Case**: Use the right tool for the job

```c
// Traditional mixing for music production
SituationAudioMixer* mixer = SituationCreateMixer();
int vocals = SituationAddTrack(mixer, "Vocals");
int reverb_bus = SituationAddAuxBus(mixer, "Reverb");
SituationSetAuxSend(mixer, vocals, reverb_bus, 0.3f, false);

// Modular synthesis for sound design
SituationThreadSafeGraph* synth = SituationCreateThreadSafeGraph();
SituationNodeHandle osc, filter, lfo;
SituationCreateNodeThreadSafe(synth, SITUATION_NODE_TONE_SYNTH, &osc);
SituationCreateNodeThreadSafe(synth, SITUATION_NODE_FILTER, &filter);
SituationCreateNodeThreadSafe(synth, SITUATION_NODE_LFO, &lfo);
SituationCreatePatchThreadSafe(synth, osc, 0, filter, 0, false);
SituationCreatePatchThreadSafe(synth, lfo, 0, filter, 0, true);  // Modulation
```

**Pros**:
- ✅ Both systems work independently
- ✅ No breaking changes
- ✅ Use mixer for traditional workflows
- ✅ Use node graph for modular workflows

**Cons**:
- ❌ Can't mix and match
- ❌ Duplicate functionality
- ❌ Two separate audio paths

---

#### Option 2: Node Graph as Insert Effects (Recommended)

**Use Case**: Use modular devices as track inserts in mixer

```c
// Create mixer
SituationAudioMixer* mixer = SituationCreateMixer();
int track = SituationAddTrack(mixer, "Synth");

// Create modular synth chain
SituationThreadSafeGraph* insert_chain = SituationCreateThreadSafeGraph();
SituationNodeHandle osc, filter, overdrive;
SituationCreateNodeThreadSafe(insert_chain, SITUATION_NODE_TONE_SYNTH, &osc);
SituationCreateNodeThreadSafe(insert_chain, SITUATION_NODE_FILTER, &filter);
SituationCreateNodeThreadSafe(insert_chain, SITUATION_NODE_OVERDRIVE, &overdrive);
SituationCreatePatchThreadSafe(insert_chain, osc, 0, filter, 0, false);
SituationCreatePatchThreadSafe(insert_chain, filter, 0, overdrive, 0, false);

// Attach to mixer track as insert
SituationSetTrackInsert(mixer, track, insert_chain);
```

**Pros**:
- ✅ Best of both worlds
- ✅ Mixer provides structure
- ✅ Node graph provides flexibility
- ✅ Modular devices as track inserts
- ✅ Modular devices as aux bus effects

**Cons**:
- ⏳ Requires integration work
- ⏳ Need to bridge two audio paths

---

#### Option 3: Unified System (Future)

**Use Case**: Replace mixer with node graph entirely

```c
// Everything is a node
SituationThreadSafeGraph* graph = SituationCreateThreadSafeGraph();

// Create "track" as a chain of nodes
SituationNodeHandle input, eq, dynamics, pan, master;
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_MIC_CAPTURE, &input);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_EQ_4BAND, &eq);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_DYNAMICS, &dynamics);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_PANNER, &pan);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_MASTER_OUTPUT, &master);

// Patch like a traditional track
SituationCreatePatchThreadSafe(graph, input, 0, eq, 0, false);
SituationCreatePatchThreadSafe(graph, eq, 0, dynamics, 0, false);
SituationCreatePatchThreadSafe(graph, dynamics, 0, pan, 0, false);
SituationCreatePatchThreadSafe(graph, pan, 0, master, 0, false);

// But also add modular routing
SituationNodeHandle lfo;
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_LFO, &lfo);
SituationCreatePatchThreadSafe(graph, lfo, 0, eq, 0, true);  // Modulate EQ
```

**Pros**:
- ✅ Ultimate flexibility
- ✅ Single unified system
- ✅ No limitations
- ✅ Fully modular

**Cons**:
- ❌ Breaking change
- ❌ More complex for simple use cases
- ❌ Requires complete rewrite of mixer

---

## Current Capabilities

### What You Can Do NOW

#### With Traditional Mixer:

```c
// Create a full mixing session
SituationAudioMixer* mixer = SituationCreateMixer();

// Add tracks
int vocals = SituationAddTrack(mixer, "Vocals");
int guitar = SituationAddTrack(mixer, "Guitar");
int drums = SituationAddTrack(mixer, "Drums");

// Add aux buses
int reverb = SituationAddAuxBus(mixer, "Reverb");
int delay = SituationAddAuxBus(mixer, "Delay");

// Route tracks to aux buses
SituationSetAuxSend(mixer, vocals, reverb, 0.3f, false);  // Post-fader
SituationSetAuxSend(mixer, guitar, delay, 0.2f, true);    // Pre-fader

// Set track parameters
SituationSetTrackVolume(mixer, vocals, 0.8f);
SituationSetTrackPan(mixer, vocals, -0.2f);  // Slightly left

// Configure EQ
SituationEQParams eq = {
    .enabled = true,
    .hpf_freq = 80.0f,
    .ls_freq = 200.0f,
    .ls_gain = 2.0f,
    // ...
};
SituationSetTrackEQ(mixer, vocals, &eq);

// Configure dynamics
SituationDynamicsParams dyn = {
    .threshold = -20.0f,
    .ratio = 4.0f,
    .attack_ms = 5.0f,
    .release_ms = 50.0f,
    // ...
};
SituationSetTrackDynamics(mixer, vocals, &dyn);
```

#### With Modular Node Graph:

```c
// Create a modular synth patch
SituationThreadSafeGraph* graph = SituationCreateThreadSafeGraph();

// Register all devices
SituationRegisterAllDevices();

// Create nodes
SituationNodeHandle osc, filter, lfo, reverb;
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, &osc);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_FILTER, &filter);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_LFO, &lfo);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_REVERB, &reverb);

// Create audio patches
SituationCreatePatchThreadSafe(graph, osc, 0, filter, 0, false);
SituationCreatePatchThreadSafe(graph, filter, 0, reverb, 0, false);

// Create modulation patches
SituationCreatePatchThreadSafe(graph, lfo, 0, filter, 0, true);  // LFO → Filter cutoff

// Set parameters (lock-free, glitch-free)
SituationSetNodeControlThreadSafe(graph, osc, 0, 440.0f);      // Frequency
SituationSetNodeControlThreadSafe(graph, filter, 0, 2000.0f);  // Cutoff
SituationSetNodeControlThreadSafe(graph, lfo, 1, 2.0f);        // LFO rate

// Process audio (in audio callback)
float output[256 * 2];
SituationProcessGraphThreadSafe(graph, output, 256, 
                                g_device_function_table, 
                                g_device_function_table_count);

// Save patch
SituationSaveGraphToFile(graph, "my_synth.json");

// Load patch
SituationLoadGraphFromFile(graph, "my_synth.json");
```

---

## Routing Capabilities

### Traditional Mixer Routing

**Fixed Signal Flow**:
```
Input → EQ → Dynamics → Pre-Fader Split → Pan → Post-Fader Split → Master
                              ↓                        ↓
                         Aux Sends (Pre)         Aux Sends (Post)
```

**Aux Bus Routing**:
```
Aux Bus Input → FX Slot 1 → FX Slot 2 → ... → FX Slot 8 → Master
```

**Sidechain Routing**:
```
Track A (Sidechain Source) → Track B Dynamics (Sidechain Input)
```

**Limitations**:
- ❌ Can't change signal flow order
- ❌ Can't route track output to another track input
- ❌ Can't create feedback loops (by design)
- ❌ Can't modulate parameters from audio signals

---

### Modular Node Graph Routing

**Arbitrary Patching**:
```
ANY Output Port → ANY Input Port (with validation)
```

**Examples**:

1. **Serial Chain**:
   ```
   Source → Effect 1 → Effect 2 → Effect 3 → Output
   ```

2. **Parallel Processing**:
   ```
   Source ──┬→ Effect A ──┐
            └→ Effect B ──┴→ Mixer → Output
   ```

3. **Modulation**:
   ```
   LFO → Filter Cutoff (control patch)
   Envelope → VCA Gain (control patch)
   ```

4. **Feedback** (with cycle detection):
   ```
   Source → Delay → Output
              ↑       ↓
              └───────┘ (feedback loop - DETECTED and PREVENTED)
   ```

5. **Complex Routing**:
   ```
   Mic → Overdrive ──┬→ Reverb → Master
                     └→ Delay ──┘
   
   LFO 1 → Overdrive Drive
   LFO 2 → Delay Time
   ```

**Validation**:
- ✅ Port count checking (can't patch to non-existent port)
- ✅ Channel count matching (stereo → stereo, mono → mono)
- ✅ Cycle detection (prevents infinite loops)
- ✅ Type checking (audio vs control patches)

---

## Recommended Workflow

### For Traditional Mixing (Use Mixer)

**Best For**:
- Music production
- Podcast mixing
- Live sound
- Traditional DAW workflows

**Example**:
```c
SituationAudioMixer* mixer = SituationCreateMixer();

// Add tracks for each instrument
int kick = SituationAddTrack(mixer, "Kick");
int snare = SituationAddTrack(mixer, "Snare");
int bass = SituationAddTrack(mixer, "Bass");
int vocals = SituationAddTrack(mixer, "Vocals");

// Add aux buses for effects
int reverb = SituationAddAuxBus(mixer, "Reverb");
int delay = SituationAddAuxBus(mixer, "Delay");

// Route and mix
SituationSetAuxSend(mixer, vocals, reverb, 0.3f, false);
SituationSetAuxSend(mixer, snare, reverb, 0.2f, false);
SituationSetTrackVolume(mixer, vocals, 0.8f);
```

---

### For Modular Synthesis (Use Node Graph)

**Best For**:
- Sound design
- Modular synthesis
- Custom signal processing
- Experimental audio

**Example**:
```c
SituationThreadSafeGraph* graph = SituationCreateThreadSafeGraph();

// Build a modular synth
SituationNodeHandle osc1, osc2, mixer, filter, lfo, env, reverb;

// Oscillators
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, &osc1);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, &osc2);

// Mixer (sum oscillators)
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_MIXER, &mixer);

// Filter
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_FILTER, &filter);

// Modulators
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_LFO, &lfo);
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_ENVELOPE, &env);

// Effects
SituationCreateNodeThreadSafe(graph, SITUATION_NODE_REVERB, &reverb);

// Audio routing
SituationCreatePatchThreadSafe(graph, osc1, 0, mixer, 0, false);
SituationCreatePatchThreadSafe(graph, osc2, 0, mixer, 1, false);
SituationCreatePatchThreadSafe(graph, mixer, 0, filter, 0, false);
SituationCreatePatchThreadSafe(graph, filter, 0, reverb, 0, false);

// Modulation routing
SituationCreatePatchThreadSafe(graph, lfo, 0, filter, 0, true);  // LFO → Cutoff
SituationCreatePatchThreadSafe(graph, env, 0, filter, 1, true);  // Env → Resonance
```

---

## Future Integration (Phase 6)

### Planned: Hybrid System

**Goal**: Use modular devices as mixer inserts and aux effects

```c
// Create mixer
SituationAudioMixer* mixer = SituationCreateMixer();
int track = SituationAddTrack(mixer, "Synth");

// Create modular insert chain
SituationThreadSafeGraph* insert = SituationCreateThreadSafeGraph();
// ... build modular chain ...

// Attach to track
SituationSetTrackInsertChain(mixer, track, insert);

// Create modular aux effect
SituationThreadSafeGraph* aux_fx = SituationCreateThreadSafeGraph();
// ... build modular effect ...

// Attach to aux bus
int reverb_bus = SituationAddAuxBus(mixer, "Reverb");
SituationSetBusEffectChain(mixer, reverb_bus, aux_fx);
```

**Benefits**:
- ✅ Mixer provides structure and routing
- ✅ Node graph provides unlimited flexibility
- ✅ Best of both worlds
- ✅ Backward compatible

---

## Summary

### Current State

| Feature | Traditional Mixer | Modular Node Graph |
|---------|------------------|-------------------|
| **Status** | ✅ Production Ready | ✅ 80% Complete |
| **Tracks** | ✅ 16 tracks | ⏳ Manual routing |
| **Aux Buses** | ✅ 8 buses | ⏳ Manual routing |
| **EQ** | ✅ Built-in 4-band | ✅ As node |
| **Dynamics** | ✅ Built-in | ✅ As node |
| **Effects** | ✅ 8 slots per bus | ✅ 19 devices |
| **Routing** | ❌ Fixed | ✅ Arbitrary |
| **Modulation** | ❌ None | ✅ Full support |
| **Serialization** | ❌ None | ✅ JSON save/load |
| **Custom Devices** | ❌ Not supported | ✅ Extensible |
| **Thread Safety** | ✅ Yes | ✅ Yes (lock-free) |

### Answer to Your Question

**Yes**, with the modular node graph system you can:

✅ Patch any registered device's output to any other device's input  
✅ Route signals arbitrarily (with validation)  
✅ Create modulation patches (LFO → filter, etc.)  
✅ Implicit summing (multiple sources → one input)  
✅ Implicit splitting (one source → multiple destinations)  
✅ Save and load entire patch configurations  

**However**, the mixer and node graph are currently **separate systems**. To route mixer channels to aux buses, you use the traditional mixer API. To create modular patches, you use the node graph API.

**Future integration** (Phase 6) will allow using modular devices as mixer inserts and aux effects, giving you the best of both worlds!

---

**Maintained By**: Kiro AI Assistant  
**Last Updated**: 2026-03-02  
**Related Docs**: `plan_audio_registry.md`, `AUDIO_DEVICE_INVENTORY.md`
