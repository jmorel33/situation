# Polysonix v1.5: Wave Sequencing Implementation Plan

> **Note:** This plan has been fully implemented in v1.5 and subsequently integrated into the v1.6 Triple Oscillator Architecture. All features described below are preserved in the v1.6 engine.

## Objective
Implement a per-voice, bytecode-driven Wave Sequencer with 8-byte steps, microtonal precision, and comprehensive global sequence settings. The implementation must be zero-allocation at runtime and fit within a 64KB static ROM budget.

## Philosophy
"Per-Cycle" logic for phase-perfect transitions.

## Phase 1: Data Structures & Constants (polysonix.h)

**Goal:** Define the storage format, logic flags, and global sequence settings.

**Actionables:**
- [x] **Modify `polysonix.h` (Public Enums and Structs section)**:
    - [x] Add 16-Bit Logic Flags macros.
    - [x] Define Enums: `PxWSeqEndAction`, `PxWSeqGlideMode`.
    - [x] Define `PxWaveSeqStep` struct (strictly 8-byte aligned).
    - [x] Define `PxWaveSequence` struct including the new Settings Header.
    - [x] Define `PX_NUM_WSEQ_BANKS` (128) and `PX_MAX_WSEQ_STEPS` (64).

**Detail:**

```c
// --- v1.5 Wave Sequencing Definitions ---

// Global Sequence Enums
typedef enum {
    PX_WSEQ_END_STOP = 0,    // Stop voice (Note Off behavior)
    PX_WSEQ_END_HOLD,        // Hold last step indefinitely
    PX_WSEQ_END_LOOP,        // Loop back to start
    PX_WSEQ_END_PINGPONG,    // Reverse direction
    PX_WSEQ_END_REVERSE      // Reverse walk
} PxWSeqEndAction;

typedef enum {
    PX_WSEQ_GLIDE_OFF = 0,
    PX_WSEQ_GLIDE_STEP,      // Glide between steps
    PX_WSEQ_GLIDE_SMOOTH     // Continuous glide
} PxWSeqGlideMode;

// 16-Bit Logic Flags (Per Step)
// --- FLOW CONTROL (Bits 0-3) ---
#define PX_WSEQ_END             (1 << 0)  // Force Sequence End Action here
#define PX_WSEQ_LOOP_POINT      (1 << 1)  // Marker for Loop Start
#define PX_WSEQ_JUMP_RANDOM     (1 << 2)  // Jump to random step

// --- MODULATION / RESET (Bits 4-7) ---
#define PX_WSEQ_RESET_LFO       (1 << 4)  // Reset LFO phases (if global flag enables it?)
#define PX_WSEQ_RETRIG_ADSR     (1 << 5)  // Retrigger ADSR (uses global phase setting)
#define PX_WSEQ_LOCK_PHASE      (1 << 6)  // Lock phase (Hard Sync)
// Bit 7 Reserved

// --- GENERATIVE (Bits 8-11) ---
#define PX_WSEQ_USE_PROB_MUTE   (1 << 8)  // Use Global Prob Mute Score
#define PX_WSEQ_USE_PROB_SKIP   (1 << 9)  // Use Global Prob Skip Score
#define PX_WSEQ_USE_RND_OCTAVE  (1 << 10) // Use Global Rnd Octave Range
#define PX_WSEQ_USE_RND_WAVE    (1 << 11) // Use Global Rnd Wave Range

// --- GLITCH / TIMBRE (Bits 12-15) ---
#define PX_WSEQ_REVERSE_PLAY    (1 << 12) // Negative Freq
#define PX_WSEQ_BITCRUSH        (1 << 13) // Enable Bitcrush (Global Depth)
#define PX_WSEQ_XMOD            (1 << 14) // Enable XMod (Global Depth/Src)
#define PX_WSEQ_RING_MOD        (1 << 15) // Enable Ring Mod (Global Depth/Src)

// 8-Byte Step Structure (Aligned)
typedef struct {
    uint16_t wave_idx;        // 0-65535
    uint16_t duration_cycles; // 0-65535
    int16_t  pitch_offset;    // Cents
    uint16_t flags;           // Bitfield
} PxWaveSeqStep;

#define PX_MAX_WSEQ_STEPS 64
#define PX_NUM_WSEQ_BANKS 128

// Sequence Structure with Header
typedef struct {
    // --- Global Settings Header ---
    uint8_t  end_action;          // PxWSeqEndAction
    uint8_t  glide_mode;          // PxWSeqGlideMode
    uint8_t  bitcrush_bits;       // 1-8
    uint8_t  adsr_retrig_phase;   // PxADSRState to jump to (e.g., ATTACK)

    uint8_t  prob_mute_score;     // 0-100%
    uint8_t  prob_skip_score;     // 0-100%
    uint8_t  rnd_octave_range;    // 0-100%
    uint8_t  reset_lfo_pos;       // Boolean (1=Yes, 0=No)

    uint16_t rnd_wave_low;        // Index
    uint16_t rnd_wave_high;       // Index

    // Modulation Sources (PxModSource cast to int8_t, -1 for None)
    int8_t   lock_phase_mod_src;  // 0 = no mod, otherwise MOD Matrix choice
    int8_t   xmod_mod_src;
    int8_t   ring_mod_mod_src;
    int8_t   _padding;

    float    xmod_depth;          // Base amount
    float    ring_mod_depth;      // Base amount

    // --- Steps ---
    PxWaveSeqStep steps[PX_MAX_WSEQ_STEPS];
} PxWaveSequence;
```

**Memory Storage:**
*   In `polysonix.h` (Implementation section):
    ```c
    // Global ROM (Static Const to reside in Flash/RO)
    static const PxWaveSequence ROM_WAVE_SEQUENCES[PX_NUM_WSEQ_BANKS];
    ```

## Phase 2: Core Engine State (polysonix.h)

**Goal:** Update `Voice` and `PxPatch` structs to track sequence state.

**Actionables:**
- [x] **Modify `PxPatch` struct:**
    - [x] Add `int selected_sequence_id;` (Default: -1, Range: -1 to 127).
- [x] **Modify `Voice` struct (Internal Data Structures):**
    - [x] Add sequencer state fields.
    - [x] Add per-step optimization cache fields.
    - [x] Add pointer/index to current sequence.

**Detail (Voice Struct Additions):**
```c
typedef struct Voice {
    // ... existing fields ...

    // --- v1.5 Sequencer State ---
    int  seq_id;                // -1 = Off
    int  seq_step_idx;          // Current step (0-63)
    int  seq_direction;         // 1 (Forward) or -1 (Backward)
    int  seq_cycles_counter;    // How many cycles played in this step
    bool seq_finished;          // True if END flag hit (and action is STOP/HOLD)

    // --- Per-Step Cached Values (Optimization) ---
    uint16_t step_flags;        // Current flags
    float    step_pitch_ratio;  // Pre-calculated frequency multiplier (from cents)
    float    target_pitch_ratio;// For glide logic
    float    step_bitcrush_scale; // Pre-calculated pow(2, bits) for bitcrush
    bool     step_mute_state;   // Latch for Mute probability
    uint32_t rng_state;         // Context-aware PRNG state

    // ... existing fields ...
} Voice;
```

## Phase 3: The Audio Loop (The Meat)

**Goal:** Inject logic into `PX_Process` and `PX_NoteOn_internal` using the new Header settings.

**Actionables:**
- [x] **Update `PX_NoteOn_internal`:**
    - [x] Initialize `v->seq_id`.
    - [x] Check `reset_lfo_pos` from header.
    - [x] Load Step 0.

- [x] **Update `PX_Process` (Inside Voice Loop):**

    - [x] **Logic Injection Point 1: Frequency Calc**
        - [x] Apply pitch ratio.
        - [x] **Glide:** Implement `glide_mode` logic here (Step vs Smooth).

    - [x] **Logic Injection Point 2: Audio FX (Generative & Glitch)**
        - [x] **Prob Mute:** Use `prob_mute_score` from header if `PX_WSEQ_USE_PROB_MUTE` flag is set.
        - [x] **Prob Skip:** Use `prob_skip_score` logic during step transition.
        - [x] **Bitcrush:** `val = floor(val * pow(2, bits)) / pow(2, bits)` using `bitcrush_bits` if `PX_WSEQ_BITCRUSH`.
        - [x] **XMod/RingMod:**
            - [x] Calculate `amount = depth + (mod_src_val * depth)`.
            - [x] Apply FM/RingMod.
        - [x] **Lock Phase:**
            - [x] If `PX_WSEQ_LOCK_PHASE`:
                - [x] If `lock_phase_mod_src > 0`: check threshold? Or use value as phase reset point? "0 = no mod, Otherwise MOD Matrix choice".

    - [x] **Logic Injection Point 3: Phase & Step Advancement**
        - [x] **End Action:**
            - [x] `STOP`: Silence voice.
            - [x] `HOLD`: Keep playing last step.
            - [x] `LOOP`: `step_idx = 0`.
            - [x] `PINGPONG`: `direction *= -1`.
        - [x] **Retrigger:**
            - [x] `PX_WSEQ_RETRIG_ADSR`: Use `adsr_retrig_phase` from header to determine where to reset ADSR (e.g. to ATTACK start).

## Phase 4: API & Control (polysonix.h)

**Goal:** Allow user control.

**Actionables:**
- [x] **Update `PxCommandType`:** Add `PX_CMD_SET_SEQUENCE_ID`.
- [x] **Add API Functions:**
    - [x] `PX_API void PX_SetSequenceID(PxSynth* s, int seq_id);`
    - [x] `PX_API int PX_GetSequenceID(PxSynth* s);`
- [x] **Implement Command Handling.**

## Phase 5: Content (ROM)

**Goal:** Populate `ROM_WAVE_SEQUENCES`.

**Presets:**
- [x] **Seq 0 (Basic):** 4 steps, Sine/Tri/Saw/Square.
- [x] **Seq 1 (Arp):** Major triad arpeggio.
- [x] **Seq 2 (Rhythmic):** Uses `PX_WSEQ_USE_PROB_MUTE` with `prob_mute_score`.
- [x] **Seq 3 (FX):** Uses Bitcrush and RingMod with header settings.

## Phase 6: Verification Strategy

**Actionables:**
- [x] **Compile Check.**
- [x] **Manual Test Harness:** `test_seq.c`
