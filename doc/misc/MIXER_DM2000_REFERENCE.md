# Yamaha DM2000V2 Reference: Mapping to Situation Mixer

**Date**: 2026-03-02  
**Library Version**: v2.4.0-alpha  
**Status**: 🎯 Phase 6 Planning Document

## Overview

This document maps the Yamaha DM2000V2 professional digital mixing console architecture to the Situation mixer implementation, identifying what features exist, what's missing, and how Phase 6 will bridge the gap.

The DM2000V2 is a 96-channel digital console with sophisticated routing, dynamics, EQ, and effects processing. It represents the gold standard for professional mixing workflows that Situation aims to support.

---

## DM2000V2 Architecture Overview

### Signal Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    YAMAHA DM2000V2 SIGNAL FLOW                          │
│                                                                         │
│  Input ──→ [HPF] ──→ [Gate] ──→ [Dynamics] ──→ [4-Band EQ] ──→       │
│                                                                         │
│  ──→ [Insert 1] ──→ [Insert 2] ──→ [Pre-Fader Split] ──→              │
│                                          ↓                              │
│                                    Aux Sends (Pre)                      │
│                                                                         │
│  ──→ [Fader] ──→ [Pan] ──→ [Post-Fader Split] ──→ [Mix Bus]          │
│                                    ↓                                    │
│                              Aux Sends (Post)                           │
│                                                                         │
│  Mix Bus ──→ [Insert] ──→ [Dynamics] ──→ [EQ] ──→ [Master]           │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key Features

1. **96 Input Channels** (48 mono + 24 stereo)
2. **8 Mix Buses** (submix groups)
3. **8 Matrix Outputs** (flexible routing)
4. **4 Stereo Aux Buses** (effects sends)
5. **2 Insert Points per Channel** (external processing)
6. **4-Band Parametric EQ** (per channel)
7. **Dynamics** (Gate + Compressor per channel)
8. **Flexible Routing Matrix** (any source → any destination)
9. **Scene Memory** (instant recall of all settings)
10. **Automation** (fader, mute, pan)

---

## Feature Comparison: DM2000V2 vs Situation Mixer

### Channel Strip Architecture

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Input Channels** | 96 (48 mono + 24 stereo) | 16 tracks | 16 tracks (expandable) |
| **High-Pass Filter** | ✅ 20-600 Hz, 12 dB/oct | ✅ Configurable | ✅ Exists |
| **Gate** | ✅ Threshold, Range, Attack, Release | ✅ Via Dynamics Node | ✅ Exists |
| **Compressor** | ✅ Threshold, Ratio, Attack, Release, Knee | ✅ Via Dynamics Node | ✅ Exists |
| **4-Band EQ** | ✅ HPF + Low Shelf + Mid Peak + High Shelf | ✅ Implemented | ✅ Exists |
| **Insert Points** | ✅ 2 per channel (pre-EQ, post-EQ) | ❌ Not implemented | 🎯 Phase 6 Session 1 |
| **Fader** | ✅ Motorized, 100mm | ✅ Volume control | ✅ Exists |
| **Pan** | ✅ Stereo pan law | ✅ Linear pan | ✅ Exists |
| **Aux Sends** | ✅ 8 sends (pre/post) | ✅ 8 sends (pre/post) | ✅ Exists |
| **Direct Out** | ✅ Pre/post-fader | ❌ Not implemented | ⏳ Future |
| **Phase Invert** | ✅ Per channel | ❌ Not implemented | ⏳ Future |
| **Delay** | ✅ 0-500ms per channel | ❌ Not implemented | ⏳ Future |

---

### Mix Bus Architecture

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Mix Buses** | ✅ 8 stereo buses | ❌ Not implemented | 🎯 Phase 6 Session 3 |
| **Bus Insert** | ✅ 1 per bus | ❌ Not implemented | 🎯 Phase 6 Session 2 |
| **Bus Dynamics** | ✅ Compressor/Limiter | ❌ Not implemented | 🎯 Phase 6 Session 2 |
| **Bus EQ** | ✅ 4-band parametric | ❌ Not implemented | 🎯 Phase 6 Session 2 |
| **Bus Fader** | ✅ Motorized | ✅ Volume control | ✅ Exists |
| **Bus to Master** | ✅ Assignable | ✅ Fixed routing | 🎯 Phase 6 Session 3 |
| **Bus to Matrix** | ✅ Flexible routing | ❌ Not implemented | ⏳ Future |

---

### Aux Bus Architecture

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Aux Buses** | ✅ 8 stereo buses | ✅ 8 aux buses | ✅ Exists |
| **Aux FX Slots** | ✅ 4 internal FX | ✅ 8 FX slots | ✅ Exists |
| **Aux Insert** | ✅ 1 per bus | ❌ Not implemented | 🎯 Phase 6 Session 2 |
| **Aux Return** | ✅ To Master/Buses | ✅ To Master only | 🎯 Phase 6 Session 3 |
| **Aux Fader** | ✅ Motorized | ✅ Volume control | ✅ Exists |

---

### Routing Matrix

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Channel → Master** | ✅ Default routing | ✅ Implemented | ✅ Exists |
| **Channel → Mix Bus** | ✅ Assignable | ❌ Not implemented | 🎯 Phase 6 Session 3 |
| **Channel → Aux** | ✅ Send level control | ✅ Implemented | ✅ Exists |
| **Bus → Master** | ✅ Assignable | ✅ Fixed routing | 🎯 Phase 6 Session 3 |
| **Bus → Matrix** | ✅ Flexible routing | ❌ Not implemented | ⏳ Future |
| **Aux → Master** | ✅ Return routing | ✅ Implemented | ✅ Exists |
| **Aux → Bus** | ✅ Flexible routing | ❌ Not implemented | 🎯 Phase 6 Session 3 |
| **Direct Outs** | ✅ Per channel | ❌ Not implemented | ⏳ Future |

---

### Effects Processing

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Internal FX** | ✅ 4 engines (Reverb, Delay, etc.) | ✅ 19 modular devices | ✅ Exists |
| **Insert FX** | ✅ 2 per channel | ❌ Not implemented | 🎯 Phase 6 Session 1 |
| **Aux FX** | ✅ 4 per aux bus | ✅ 8 slots per bus | ✅ Exists |
| **FX Library** | ✅ Yamaha algorithms | ✅ Custom + Registry | ✅ Exists |
| **FX Bypass** | ✅ Per effect | ⏳ Partial | 🎯 Phase 6 |
| **FX Automation** | ✅ Parameter recall | ❌ Not implemented | ⏳ Future |

---

### Scene Management

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Scene Memory** | ✅ 300 scenes | ❌ Not implemented | 🎯 Phase 6 Session 4 |
| **Scene Recall** | ✅ Instant recall | ❌ Not implemented | 🎯 Phase 6 Session 4 |
| **Scene Fade Time** | ✅ Configurable | ❌ Not implemented | ⏳ Future |
| **Partial Recall** | ✅ Selective parameters | ❌ Not implemented | ⏳ Future |
| **Scene Naming** | ✅ Custom names | ❌ Not implemented | 🎯 Phase 6 Session 4 |

---

### Metering & Monitoring

| Feature | DM2000V2 | Situation Current | Phase 6 Target |
|---------|----------|-------------------|----------------|
| **Channel Meters** | ✅ Peak + RMS | ✅ Peak only | ✅ Exists |
| **Bus Meters** | ✅ Peak + RMS | ❌ Not implemented | 🎯 Phase 6 Session 3 |
| **Master Meters** | ✅ Peak + RMS + Phase | ✅ Peak only | ⏳ Future |
| **Gain Reduction** | ✅ Per dynamics | ✅ Implemented | ✅ Exists |
| **Spectrum Analyzer** | ✅ Real-time FFT | ❌ Not implemented | ⏳ Future |

---

## Phase 6 Implementation Plan

### Session 1: Insert Chain Integration (2 days)

**Goal**: Match DM2000V2's insert point functionality

**DM2000V2 Insert Points**:
- Insert 1: Pre-EQ (for external preamps, mic processors)
- Insert 2: Post-EQ (for external compressors, effects)

**Situation Implementation**:
```c
// API Design
typedef enum {
    SITUATION_INSERT_PRE_EQ,    // Before EQ (like DM2000 Insert 1)
    SITUATION_INSERT_POST_EQ,   // After EQ, before dynamics
    SITUATION_INSERT_POST_DYN,  // After dynamics (like DM2000 Insert 2)
} SituationInsertPosition;

// Attach modular chain as insert
SituationError SituationSetTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    SituationThreadSafeGraph* insert_chain
);

// Remove insert
SituationError SituationClearTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position
);

// Bypass insert (like DM2000 bypass button)
SituationError SituationBypassTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    bool bypass
);
```

**Signal Flow**:
```
Input ──→ [Insert Pre-EQ] ──→ [EQ] ──→ [Insert Post-EQ] ──→ 
         (Node Graph)                   (Node Graph)

──→ [Dynamics] ──→ [Insert Post-Dyn] ──→ [Pan] ──→ Master
                   (Node Graph)
```

**Implementation Details**:
1. Modify `SituationAudioTrack` structure to hold 3 insert chain pointers
2. Insert node graph processing between existing miniaudio nodes
3. Handle bypass by routing around insert chain
4. Ensure thread-safe attach/detach operations
5. Maintain zero-latency switching (no clicks/pops)

**Testing**:
- Simple insert: Tone Synth → Overdrive (Pre-EQ)
- Complex insert: Filter → Chorus → Reverb (Post-EQ)
- Bypass functionality during playback
- Multiple inserts on same track

---

### Session 2: Aux Bus FX Integration (2 days)

**Goal**: Match DM2000V2's aux bus processing

**DM2000V2 Aux Bus**:
- 8 stereo aux buses
- 4 FX slots per bus (Situation has 8)
- Insert point per bus
- Return to master or mix buses

**Situation Implementation**:
```c
// Attach modular FX chain to aux bus
SituationError SituationSetBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id,
    SituationThreadSafeGraph* fx_chain
);

// Clear FX chain
SituationError SituationClearBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id
);

// Bypass FX chain
SituationError SituationBypassBusEffects(
    SituationAudioMixer* mixer,
    int bus_id,
    bool bypass
);

// Wet/Dry mix (like DM2000 effect balance)
SituationError SituationSetBusEffectMix(
    SituationAudioMixer* mixer,
    int bus_id,
    float wet_dry  // 0.0 = dry, 1.0 = wet
);
```

**Signal Flow**:
```
Aux Bus Input ──→ [Node Graph FX Chain] ──→ [Wet/Dry Mix] ──→ Master
                  (Modular Effects)
```

**Implementation Details**:
1. Replace fixed FX slots with modular node graph
2. Add wet/dry mix control (parallel processing)
3. Maintain pre/post-fader send routing
4. Support multiple tracks sending to same aux
5. Efficient processing (skip silent buses)

**Testing**:
- Reverb bus with modular reverb chain
- Delay bus with modular delay + filter
- Multiple tracks sending to same aux
- Wet/dry mix control
- Bypass functionality

---

### Session 3: Flexible Signal Flow Control (2-3 days)

**Goal**: Match DM2000V2's routing matrix

**DM2000V2 Routing**:
- Channel → Master (default)
- Channel → Mix Bus 1-8 (assignable)
- Mix Bus → Master (default)
- Mix Bus → Matrix 1-8 (assignable)
- Aux → Master (default)
- Aux → Mix Bus (assignable)

**Situation Implementation**:
```c
// Routing destination types
typedef enum {
    SITUATION_ROUTE_MASTER,        // To master bus
    SITUATION_ROUTE_MIX_BUS,       // To mix bus (submix)
    SITUATION_ROUTE_AUX_BUS,       // To aux bus
    SITUATION_ROUTE_TRACK,         // To another track
    SITUATION_ROUTE_NODE_GRAPH,    // To external node graph
    SITUATION_ROUTE_NONE,          // Mute (no output)
} SituationRoutingDestination;

// Routing source types
typedef enum {
    SITUATION_SOURCE_TRACK,
    SITUATION_SOURCE_MIX_BUS,
    SITUATION_SOURCE_AUX_BUS,
} SituationRoutingSource;

// Route track output
SituationError SituationRouteTrackOutput(
    SituationAudioMixer* mixer,
    int track_id,
    SituationRoutingDestination dest_type,
    int dest_id,  // Bus/track ID if applicable
    float gain    // Routing gain (0.0-1.0)
);

// Create mix bus (like DM2000 Mix Bus 1-8)
SituationError SituationCreateMixBus(
    SituationAudioMixer* mixer,
    const char* name,
    int* out_bus_id
);

// Route mix bus output
SituationError SituationRouteMixBusOutput(
    SituationAudioMixer* mixer,
    int bus_id,
    SituationRoutingDestination dest_type,
    int dest_id,
    float gain
);

// Assign track to mix bus (like DM2000 bus assign buttons)
SituationError SituationAssignTrackToMixBus(
    SituationAudioMixer* mixer,
    int track_id,
    int bus_id,
    bool assigned
);
```

**Signal Flow Examples**:

1. **Drum Submix** (like DM2000 Mix Bus):
```
Kick Track ──┐
Snare Track ─┼──→ [Drum Mix Bus] ──→ [Bus Dynamics] ──→ Master
Hats Track ──┘
```

2. **Parallel Compression** (like DM2000 parallel routing):
```
Vocal Track ──┬──→ Master (Direct)
              └──→ [Mix Bus] ──→ [Heavy Comp] ──→ Master (Parallel)
```

3. **Complex Routing** (like DM2000 matrix):
```
Track 1 ──→ Mix Bus 1 ──→ Aux 1 ──→ Master
Track 2 ──→ Mix Bus 1 ──┘
Track 3 ──→ Mix Bus 2 ──→ Master
```

**Implementation Details**:
1. Add mix bus support (8 buses like DM2000)
2. Implement routing matrix with cycle detection
3. Support multiple destinations per source
4. Add bus dynamics and EQ (like DM2000)
5. Validate routing changes before applying
6. Ensure no audio dropouts during routing changes

**Testing**:
- Drum submix: Kick + Snare + Hats → Drum Bus → Master
- Parallel compression: Track → [Direct + Compressed] → Master
- Complex routing: Track → Mix Bus 1 → Aux 1 → Master
- Routing cycle detection
- Multiple tracks to same mix bus

---

### Session 4: Mixer Serialization (1 day)

**Goal**: Match DM2000V2's scene memory system

**DM2000V2 Scene Memory**:
- 300 scene slots
- Instant recall
- Scene naming
- Partial recall (selective parameters)
- Scene fade time

**Situation Implementation**:
```c
// Save mixer configuration (like DM2000 scene store)
SituationError SituationSaveMixerScene(
    SituationAudioMixer* mixer,
    const char* filepath,
    const char* scene_name
);

// Load mixer configuration (like DM2000 scene recall)
SituationError SituationLoadMixerScene(
    SituationAudioMixer* mixer,
    const char* filepath
);

// Scene metadata
typedef struct {
    char name[64];
    char description[256];
    uint32_t version;
    uint64_t timestamp;
} SituationSceneMetadata;

// Get scene info without loading
SituationError SituationGetSceneMetadata(
    const char* filepath,
    SituationSceneMetadata* out_metadata
);
```

**JSON Format** (inspired by DM2000 scene structure):
```json
{
  "version": "2.4.0",
  "scene": {
    "name": "Rock Mix - Verse",
    "description": "Verse section with light compression",
    "timestamp": 1709395200,
    "tracks": [
      {
        "id": 0,
        "name": "Vocals",
        "volume": 0.8,
        "pan": 0.0,
        "mute": false,
        "solo": false,
        "eq": {
          "enabled": true,
          "hpf_freq": 80.0,
          "ls_freq": 200.0,
          "ls_gain": 2.0,
          "ls_q": 0.7,
          "peak_freq": 3000.0,
          "peak_gain": 3.0,
          "peak_q": 1.0,
          "hs_freq": 8000.0,
          "hs_gain": 2.0,
          "hs_q": 0.7
        },
        "dynamics": {
          "threshold_db": -20.0,
          "ratio": 4.0,
          "attack_ms": 5.0,
          "release_ms": 50.0,
          "makeup_gain_db": 3.0,
          "is_gate": false,
          "sidechain_enabled": false
        },
        "inserts": [
          {
            "position": "pre_eq",
            "chain_file": "vocal_preamp.json",
            "bypass": false
          },
          {
            "position": "post_eq",
            "chain_file": "vocal_deesser.json",
            "bypass": false
          }
        ],
        "aux_sends": [
          {
            "bus": 0,
            "level": 0.3,
            "pre_fader": false
          },
          {
            "bus": 1,
            "level": 0.1,
            "pre_fader": false
          }
        ],
        "routing": {
          "destination": "master",
          "gain": 1.0
        }
      }
    ],
    "mix_buses": [
      {
        "id": 0,
        "name": "Drums",
        "volume": 0.9,
        "pan": 0.0,
        "dynamics": {
          "threshold_db": -10.0,
          "ratio": 2.0,
          "attack_ms": 10.0,
          "release_ms": 100.0,
          "makeup_gain_db": 2.0
        },
        "routing": {
          "destination": "master",
          "gain": 1.0
        }
      }
    ],
    "aux_buses": [
      {
        "id": 0,
        "name": "Reverb",
        "volume": 0.7,
        "pan": 0.0,
        "fx_chain": "plate_reverb.json",
        "wet_dry": 1.0,
        "bypass": false
      },
      {
        "id": 1,
        "name": "Delay",
        "volume": 0.6,
        "pan": 0.0,
        "fx_chain": "stereo_delay.json",
        "wet_dry": 0.8,
        "bypass": false
      }
    ],
    "master": {
      "volume": 0.85,
      "limiter_enabled": true
    }
  }
}
```

**Implementation Details**:
1. Serialize all mixer state to JSON
2. Include references to insert/FX chain files
3. Support scene naming and metadata
4. Validate scene version compatibility
5. Handle missing insert/FX chain files gracefully
6. Atomic scene loading (all or nothing)

**Testing**:
- Save complex mixer scene
- Load scene and verify all parameters
- Scene with missing insert chains (graceful degradation)
- Version compatibility handling
- Round-trip integrity test

---

## Summary: DM2000V2 Feature Parity

### ✅ Already Implemented (Current Situation Mixer)

- Channel strip basics (volume, pan, mute, solo)
- 4-band parametric EQ (HPF + Low Shelf + Peak + High Shelf)
- Dynamics (Gate + Compressor with sidechain)
- Aux sends (8 buses, pre/post-fader)
- Metering (peak levels per track)
- Thread-safe parameter updates

### 🎯 Phase 6 Targets (DM2000V2 Parity)

- **Session 1**: Insert points (2 per channel, like DM2000)
- **Session 2**: Aux bus FX chains (modular effects, like DM2000 internal FX)
- **Session 3**: Mix buses and routing matrix (8 buses, like DM2000)
- **Session 4**: Scene memory (save/recall, like DM2000 scenes)

### ⏳ Future Enhancements (Beyond DM2000V2)

- Matrix outputs (flexible routing)
- Direct outs (per channel)
- Phase invert (per channel)
- Delay compensation (per channel)
- Spectrum analyzer (real-time FFT)
- Automation (fader, mute, pan)
- RMS metering (in addition to peak)
- Scene fade time (smooth transitions)
- Partial scene recall (selective parameters)

---

## Conclusion

Phase 6 will bring Situation's mixer to **DM2000V2 feature parity** in the areas that matter most for professional mixing workflows:

1. **Insert Points** - External processing integration
2. **Modular FX** - Flexible effects routing
3. **Mix Buses** - Submixing and grouping
4. **Scene Memory** - Instant recall of complex setups

After Phase 6, Situation will have a mixer that rivals professional digital consoles while maintaining the flexibility of a modular node graph system underneath.

The hybrid architecture (traditional mixer + modular node graph) provides the best of both worlds: the structure and workflow of a DM2000V2 with the flexibility and extensibility of a modular synthesis environment.

---

**Next Steps**: Begin Phase 6 Session 1 - Insert Chain Integration
