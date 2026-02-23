# Situation Audio Mixer — Design Specification & Implementation Plan

**Target:** A professional-grade, low-level 16-stereo-track mixer with aux sends, sub-groups, flexible routing, and per-channel processing.
**Philosophy:** Inspired by the Yamaha DM2000 — maximum control and flexibility with zero hand-holding.
We provide the infrastructure and tooling. The calling application decides presets, UI, automation, and workflow.

**Version:** 1.1 (Expanded Features) — 2026-02-17
**Author:** Jacques Morel (Architecture), Jules (Technical Implementation)

---

## 1. Goals

- **Professional Core:** Deliver a clean, powerful, and extensible mixer core similar in spirit to high-end digital consoles (Yamaha DM2000 / SSL / Neve).
- **Fixed & Flexible:** Support up to 16 stereo tracks + 8 aux/send buses + insert FX chains.
- **Per-Channel Processing:** Built-in EQ and Dynamics (Compressor/Limiter/Gate) on every channel.
- **Runtime Dynamism:** Allow complete dynamic re-routing at runtime (Track → Bus → FX → Master).
- **Minimal API:** Keep the API minimal and low-level — no built-in presets, no "factory effects", no profiles. The user builds the graph; we manage the signal flow.
- **Session Persistence:** Provide robust binary session save/load functionality to serialize the entire mixer state.
- **Integration:** Stay lightweight and integrate seamlessly with the existing `miniaudio` backend and `Situation` generational task system.

## 2. Feature Overview

### 2.1. 16 Stereo Tracks
Each track is a fully-featured processing channel strip.
- **Identification:** User-assignable Label Name per channel.
- **Controls:** Volume (Fader), Pan (Stereo Balance), Mute, Solo.
- **EQ:** 4-Band Parametric EQ per channel (High-Pass, Low-Shelf, Peaking, High-Shelf).
- **Dynamics:** Compressor / Limiter / Noise Gate per channel.
  - **Side-Chain:** Supports external side-chain inputs (e.g., Kick drum triggering Bass compression).
- **Sends:** Up to 8 sends per track (configurable Pre-Fader or Post-Fader) routing to Aux Buses.
- **Routing:** Direct Output assignable to Master or any Sub-Group.

### 2.2. Aux / Send Buses (8 Slots)
- **Function:** Standard summing buses for effects (Reverb, Delay) or monitoring.
- **Inputs:** Can receive sends from any of the 16 tracks.
- **Inserts:** Supports a chain of FX nodes (up to 8 slots per bus).

### 2.3. Sub-Groups
- **Definition:** User-definable grouping buses.
- **Usage:** Route Drum tracks to a "Drums" group for single-fader control before hitting Master.

### 2.4. Master Bus
- **Function:** Final stereo summing point.
- **Inserts:** Supports Master FX chain (Limiter, Compressor, EQ).
- **Output:** Routes directly to the audio device endpoint.

### 2.5. FX Insert System
- **Architecture:** "Plug-in / Plug-out" nodes.
- **Extensibility:** Effects are user-provided `ma_node` implementations (we only provide the container/routing).
- **Flexibility:** Insert FX at any point: Track Insert, Bus Insert, or Master Insert.

### 2.6. Session Management
- **Persistence:** Save/Load complete mixer state to a compact binary format.
- **Data:** Serializes topology, fader levels, pan settings, send amounts, EQ settings, Dynamics parameters, and FX parameters (via opaque blobs).

### 2.7. Monitoring
- **Metering:** Real-time Peak and RMS metering per Track, Bus, and Master.
- **Access:** Exposed via thread-safe atomic accessors for UI overlays.

---

## 3. Core Data Structures

The mixer is built on top of `miniaudio`'s `ma_node_graph` API.

```c
#define SIT_MAX_TRACKS          16
#define SIT_MAX_AUX_BUSES        8
#define SIT_MAX_FX_SLOTS         8

// Forward declarations
typedef struct SituationAudioMixer SituationAudioMixer;
typedef struct SituationAudioTrack SituationAudioTrack;
typedef struct SituationAudioBus SituationAudioBus;

// --------------------------------------------------------------------------------------
// Track Definition
// --------------------------------------------------------------------------------------
typedef struct {
    char name[64];                   // User-assignable label
    int id;                          // Internal index (0-15)
    bool is_active;

    // -- State --
    float volume;                    // 0.0 - 1.0 (Linear gain)
    float pan;                       // -1.0 left → +1.0 right
    bool mute;
    bool solo;

    // -- EQ State --
    struct {
        bool enabled;
        float high_pass_freq;
        float low_shelf_gain;
        float low_shelf_freq;
        float peaking_gain;
        float peaking_freq;
        float peaking_q;
        float high_shelf_gain;
        float high_shelf_freq;
    } eq;

    // -- Dynamics State --
    struct {
        bool enabled;
        float threshold_db;
        float ratio;
        float attack_ms;
        float release_ms;
        float makeup_gain_db;
        bool is_limiter_mode;        // Ratio = Inf
        bool is_gate_mode;           // Acts as expander/gate
        int side_chain_source_id;    // Track ID to use as key input (-1 = None/Internal)
    } dynamics;

    // -- Sends --
    float send_level[SIT_MAX_AUX_BUSES];
    bool send_pre_fader[SIT_MAX_AUX_BUSES];

    // -- Miniaudio Node Graph --
    // A track is a composite of nodes:
    // [Input] -> [EQ] -> [Dynamics] -> [Splitter(Sends)] -> [Gain/Pan] -> [Output]
    ma_node_base* input_node;        // Where audio sources connect

    // Internal Processing Nodes
    ma_peaking_eq_node eq_node_peak; // Chain of filters...
    ma_lpf_node eq_node_lp;          // ...simplified for this spec struct

    // Dynamics Node (Custom or wrapped)
    // Supports 2 inputs: [0] = Signal, [1] = Sidechain Key
    ma_compressor_node dynamics_node;

    ma_splitter_node splitter;       // Splits signal for Sends vs Main
    ma_fader_node fader;             // Handles Volume/Mute (custom node wrapper)
    ma_panner_node panner;           // Handles Pan

    // -- Metering --
    _Atomic float peak_L;
    _Atomic float peak_R;
    _Atomic float gain_reduction;    // Dynamics gain reduction (dB)
} SituationAudioTrack;

// --------------------------------------------------------------------------------------
// Bus Definition (Aux / Group / Master)
// --------------------------------------------------------------------------------------
typedef struct {
    char name[64];
    int id;

    // -- FX Chain --
    void* fx[SIT_MAX_FX_SLOTS];      // User-provided effect nodes (opaque handles)
    int fx_count;

    // -- Miniaudio Node Graph --
    // [Input Sum] -> [FX Chain] -> [Fader] -> [Output]
    ma_node_base* input_node;        // Summing point
    ma_node_base* output_node;       // Post-processing output

    // -- Metering --
    _Atomic float peak_L;
    _Atomic float peak_R;
} SituationAudioBus;

// --------------------------------------------------------------------------------------
// Mixer Container
// --------------------------------------------------------------------------------------
struct SituationAudioMixer {
    ma_node_graph graph;             // The root miniaudio node graph
    ma_device* device;               // Reference to the main audio device

    // -- Tracks --
    SituationAudioTrack tracks[SIT_MAX_TRACKS];
    int track_count;

    // -- Buses --
    SituationAudioBus aux_buses[SIT_MAX_AUX_BUSES];
    int aux_count;

    SituationAudioBus master;

    // -- Session Data --
    char session_name[256];
    uint64_t last_save_time;

    // -- Thread Safety --
    // Protects graph topology changes (connecting/disconnecting nodes)
    // Parameter changes (volume/pan) are atomic/thread-safe by design
    mtx_t topology_mutex;
};
```

---

## 4. Public API (User-Facing)

The API is low-level, C-style, and designed for integration.

### 4.1. Lifecycle
```c
// Creates a new mixer instance attached to the global audio device
SituationAudioMixer* SituationCreateMixer(void);

// Destroys the mixer and all internal nodes. Does NOT destroy external FX nodes.
void SituationDestroyMixer(SituationAudioMixer* mixer);
```

### 4.2. Track Management
```c
// Activates a track slot and configures its internal nodes
SituationAudioTrack* SituationAddTrack(SituationAudioMixer* mixer, const char* name);

// Deactivates a track and disconnects it
void SituationRemoveTrack(SituationAudioTrack* track);

// Sets the label name for a track (useful for UI/Session)
void SituationSetTrackName(SituationAudioTrack* track, const char* name);

// Connects a playing sound (source) to a specific track input
SituationError SituationRouteSoundToTrack(SituationSoundHandle sound, SituationAudioTrack* track);
```

### 4.3. Channel Strip Processing
```c
// Configure EQ
// bands: [0]=HighPass, [1]=LowShelf, [2]=Peaking, [3]=HighShelf
void SituationSetTrackEQ(SituationAudioTrack* track, bool enabled, float* freqs, float* gains, float* Qs);

// Configure Dynamics (Compressor/Limiter)
// mode: 0=Compressor, 1=Limiter, 2=Gate
void SituationSetTrackDynamics(SituationAudioTrack* track, bool enabled, int mode, float threshold_db, float ratio, float attack_ms, float release_ms, float makeup_gain);

// Set Side-Chain Source
// source_track: The track to use as the key trigger. NULL = Internal side-chain.
void SituationSetTrackSideChain(SituationAudioTrack* target_track, SituationAudioTrack* source_track);
```

### 4.4. Routing & Sends
```c
// Configures a send from a track to an Aux bus
SituationError SituationSetTrackSend(SituationAudioTrack* track, int aux_bus_index, float level, bool pre_fader);

// Sets the main output of a track (defaults to Master, can be a Sub-Group)
SituationError SituationSetTrackOutput(SituationAudioTrack* track, SituationAudioBus* destination);
```

### 4.5. Parameter Control
```c
void SituationSetTrackVolume(SituationAudioTrack* track, float volume);
void SituationSetTrackPan(SituationAudioTrack* track, float pan);
void SituationSetTrackMute(SituationAudioTrack* track, bool mute);
void SituationSetTrackSolo(SituationAudioTrack* track, bool solo);
```

### 4.6. FX Inserts
```c
// Inserts a custom miniaudio node into a bus's FX chain
SituationError SituationInsertEffect(SituationAudioBus* bus, int slot, ma_node* effect_node);

// Removes an effect and bridges the gap
void* SituationRemoveEffect(SituationAudioBus* bus, int slot);
```

### 4.7. Master & Monitoring
```c
void SituationSetMasterVolume(SituationAudioMixer* mixer, float volume);
void SituationGetTrackMeter(SituationAudioTrack* track, float* left_peak, float* right_peak, float* gain_reduction);
```

### 4.8. Session Persistence
```c
bool SituationSaveMixerSession(SituationAudioMixer* mixer, const char* filepath);
bool SituationLoadMixerSession(SituationAudioMixer* mixer, const char* filepath);
```

---

## 5. Internal Architecture

### 5.1. The Node Graph
- **Graph Root:** The `SituationAudioMixer` owns a `ma_node_graph`.
- **Endpoint:** The graph's endpoint connects directly to the `ma_device` playback callback.
- **Execution:** The graph is processed in the audio thread. All API calls modifying topology (add/remove track/FX) must acquire `topology_mutex`.

### 5.2. Signal Flow (Per Track)
1.  **Source:** `ma_data_source_node` (The playing sound).
2.  **Input:** Connected to `track->input_node`.
3.  **EQ Chain:** Series of 4 filter nodes (HPF -> LS -> Peak -> HS).
4.  **Dynamics:** `ma_compressor_node` (or custom wrapper).
    *   **Input 0:** Main Signal (from EQ).
    *   **Input 1:** Side-chain Signal (routed from another track's pre-fader tap).
5.  **Splitter:** `ma_splitter_node` divides signal:
    *   **Output 0:** To Fader/Pan chain (Main Path).
    *   **Output 1..N:** To Send Gains (Aux Paths).
6.  **Send Gains:** If Post-Fader, signal is tapped *after* the fader. If Pre-Fader, tapped *before*.
7.  **Processing:** `ma_fader_node` (Volume) -> `ma_panner_node` (Pan).
8.  **Summing:** Output connects to `master.input_node` or `subgroup.input_node`.

### 5.3. FX Chaining
- An `FX Chain` is a linked list of `ma_node`s.
- `SituationInsertEffect` breaks the link between `Node A` and `Node B`, and inserts `Node FX`: `A -> FX -> B`.
- FX nodes are "borrowed". The Mixer does not own the memory of the FX node struct, only the routing.

---

## 6. Save/Load Format (Binary)

The format is a simple, versioned binary stream for maximum read/write speed.

### 6.1. File Header
```c
struct MixerFileHeader {
    char magic[4];          // "SMX2" (Updated version)
    uint32_t version;       // 2
    uint64_t timestamp;
    uint32_t track_count;
    uint32_t bus_count;
};
```

### 6.2. Track Block (Repeated `track_count` times)
```c
struct MixerTrackData {
    int id;
    char name[64];
    float volume;
    float pan;
    uint8_t flags;          // Bit 0: Mute, Bit 1: Solo, Bit 2: Active

    // -- EQ --
    uint8_t eq_enabled;
    float eq_params[16];    // Serialized EQ bands (Freq/Gain/Q)

    // -- Dynamics --
    uint8_t dyn_enabled;
    uint8_t dyn_mode;
    float dyn_params[8];    // Thresh, Ratio, Attack, Release, Makeup, etc.
    int side_chain_source;  // ID of sidechain source track

    float sends[8];         // Levels for all 8 aux slots
    uint8_t send_pre[8];    // Pre/Post flags
};
```

### 6.3. Bus Block (Repeated `bus_count` times)
```c
struct MixerBusData {
    int id;
    char name[64];
    float volume;
    int fx_count;
    // Note: We cannot serialize generic FX pointers.
    // FX serialization requires a specific "FX ID" registry which is out of scope for Phase 1.
    // For v1.0, FX chains are NOT persisted, only levels and routing.
};
```

---

## 7. Implementation Phases

### **Phase 1: Core Infrastructure**
*   [x] Implement `SituationAudioMixer` struct and initialization.
*   [x] Set up `ma_node_graph` attached to the main `Situation` audio device.
*   [x] Implement `SituationAudioTrack` with Volume and Pan nodes.
*   [x] Implement routing: Track -> Master.
*   [x] Verify basic audio playback through the mixer.

### **Phase 2: Channel Strip (EQ & Dynamics)**
*   [x] Implement 4-band EQ chain using miniaudio filters.
*   [x] Implement Dynamics node (Comp/Lim/Gate) wrapper.
*   [x] Implement Side-chain routing logic (wiring Input 1 of Dynamics node).

### **Phase 3: Routing & Aux**
*   [x] Implement `Aux` buses.
*   [x] Implement `Sends` (Pre/Post fader logic using splitters).
*   [x] Implement `Mute` and `Solo` logic (Solo requires muting all non-soloed tracks).

### **Phase 4: FX & Metering**
*   [ ] Implement FX Insert logic (graph surgery).
*   [ ] Add atomic peak metering to the audio callback / node process.
*   [ ] Create a test with a Reverb node on an Aux bus.

### **Phase 5: Persistence & Polish**
*   [ ] Implement `SituationSaveMixerSession` / `SituationLoadMixerSession` with new V2 format.
*   [ ] Optimize thread locking (minimize stalling the audio thread).
*   [ ] Final integration test with the `Situation` hot-reload system.


---

## 8. Device Enumeration & Routing Policy

One of the core responsibilities of the Situation Audio Reactor is to provide **reliable, validated device management**. We must know exactly what devices are available, what they are capable of, and enforce safe routing rules before any audio flows.

This prevents common pitfalls such as:
- Trying to route a stereo track to a mono device
- Using a capture-only device for output
- Channel count mismatches
- Silent failures when devices are disconnected

### 8.1. Device Types
```c
typedef enum {
    SIT_AUDIO_DEVICE_TYPE_UNKNOWN     = 0,
    SIT_AUDIO_DEVICE_TYPE_PLAYBACK    = 1,   // Output only (speakers, headphones)
    SIT_AUDIO_DEVICE_TYPE_CAPTURE     = 2,   // Input only (microphones, line-in)
    SIT_AUDIO_DEVICE_TYPE_DUPLEX      = 3,   // Both input and output (most sound cards)
    SIT_AUDIO_DEVICE_TYPE_LOOPBACK    = 4    // System loopback (for desktop audio capture)
} SituationAudioDeviceType;
```

### 8.2. Device Information Structure
```c
typedef struct {
    char name[256];                    // Human-readable name (e.g. "Focusrite Scarlett 2i2")
    char id[128];                      // Backend-specific unique identifier (miniaudio ID)

    SituationAudioDeviceType type;

    uint32_t min_channels_in;          // Minimum supported input channels
    uint32_t max_channels_in;          // Maximum supported input channels
    uint32_t min_channels_out;         // Minimum supported output channels
    uint32_t max_channels_out;         // Maximum supported output channels

    uint32_t preferred_sample_rate;    // Preferred / native sample rate
    uint32_t native_format;            // Preferred miniaudio format (f32, s16, etc.)

    bool is_default_playback;          // Is this the system's default output?
    bool is_default_capture;           // Is this the system's default input?

    // Future extension points
    uint32_t latency_us;               // Estimated latency in microseconds
} SituationAudioDeviceInfo;
```

### 8.3. Public Enumeration API
```c
// Returns a dynamically allocated array of available devices. Caller must free with SituationFreeDeviceList().
SituationAudioDeviceInfo* SituationEnumerateAudioDevices(int* out_count);

// Free the list returned by SituationEnumerateAudioDevices()
void SituationFreeDeviceList(SituationAudioDeviceInfo* devices, int count);

// Returns the best matching device for a given requirement (e.g. minimum 2 output channels)
SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type,
                                                  uint32_t min_channels_out,
                                                  uint32_t min_channels_in);
```

### 8.4. Mixer Device Binding
```c
// Binds the mixer to a specific output device (validation happens here)
SituationError SituationBindMixerToDevice(SituationAudioMixer* mixer,
                                          const char* device_id,
                                          uint32_t requested_channels_out);

// For capture (mic/line input) — can be bound independently
SituationError SituationBindCaptureDevice(SituationAudioMixer* mixer,
                                          const char* device_id,
                                          uint32_t requested_channels_in);
```

### 8.5. Validation & Routing Rules (Enforced Internally)
When binding a device, we validate:
1. The device exists
2. It supports the requested channel count (within min/max)
3. Type compatibility (cannot use capture-only device for output)
4. Channel routing is validated at connection time (e.g. stereo track cannot route to mono output without explicit downmix policy)
5. Default fallback: If no device is specified, we automatically select the system default with best channel match.
