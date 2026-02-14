# Wave Sequencer Amplitude Modulation Plan

## Overview
This document analyzes and proposes the addition of Amplitude Modulation (Amp Mod) capabilities to the Polysonix Wave Sequencer. This feature allows per-step amplitude shaping (e.g., ramps, gates, random levels) driven by the sequence execution, enabling rhythmic gating, tremolo, and pluck effects tied to the step duration.

## Data Structures

### 1. Global Setting: `amp_mod_type`
We will utilize the existing `_padding` field in the `PxWaveSequence` structure to store the Amplitude Modulation Type. This ensures no change in structure size or alignment.

**Location:** `polysonix.h` - `PxWaveSequence` struct.

```c
typedef struct {
    // ...
    int8_t   lock_phase_mod_src;
    int8_t   xmod_mod_src;
    int8_t   ring_mod_mod_src;
    int8_t   amp_mod_type;        // Previously _padding

    // ...
} PxWaveSequence;
```

**Proposed Types (3-bit, 8 values):**
The `amp_mod_type` field (int8_t) will store one of the following values. These define the *shape* of the amplitude envelope applied over the duration of a step.

| Value | Name | Description | Formula (t = 0.0 to 1.0) |
| :--- | :--- | :--- | :--- |
| 0 | `PX_WSEQ_AMP_RAMP_DOWN` | Linear fade out (Pluck) | `1.0 - t` |
| 1 | `PX_WSEQ_AMP_RAMP_UP` | Linear fade in (Reverse) | `t` |
| 2 | `PX_WSEQ_AMP_TRIANGLE` | Triangle (Swell/Fade) | `1.0 - abs(2*t - 1)` |
| 3 | `PX_WSEQ_AMP_SINE` | Sine Hump | `sin(t * PI)` |
| 4 | `PX_WSEQ_AMP_SQUARE` | 50% Gate (Chopper) | `t < 0.5 ? 1.0 : 0.0` |
| 5 | `PX_WSEQ_AMP_PULSE_25` | 25% Gate (Short blip) | `t < 0.25 ? 1.0 : 0.0` |
| 6 | `PX_WSEQ_AMP_EXP_DOWN` | Exponential Pluck (Percussive) | `pow(1.0 - t, 4.0)` |
| 7 | `PX_WSEQ_AMP_RANDOM` | Random Level (S&H) | `rnd()` (Held for step) |

### 2. Per-Step Flag: `PX_WSEQ_AMP_MOD`
We will use the currently unused Bit 3 in the `PxWaveSeqStep` flags to enable this feature on a per-step basis.

**Location:** `polysonix.h` - Bitwise Flags.

```c
#define PX_WSEQ_AMP_MOD         (1 << 3)  // Value: 8
```

## Implementation Logic

### Audio Loop (`PX_Process`)
In `polysonix.h`, inside the voice/oscillator processing loop:

1.  **Check Flag:** Verify if `sq->step_flags & PX_WSEQ_AMP_MOD` is true.
2.  **Calculate Phase:** Determine the normalized progress of the current step (`t`).
    ```c
    float t = ((float)sq->cycles_counter + v->osc_phase[o]) / effective_duration;
    // Clamp t to [0.0, 1.0]
    ```
3.  **Calculate Scalar:** Apply the formula corresponding to `sq->current_sequence->amp_mod_type`.
    *   *Optimization:* For `PX_WSEQ_AMP_RANDOM`, the value should be latched in `PxSeqState` when the step is loaded to ensure it remains constant for the duration of the step.
4.  **Apply:** Multiply the oscillator's `raw_sample` (or `level`) by this scalar.

### Random Value Handling
To support `PX_WSEQ_AMP_RANDOM` correctly, the `PxSeqState` struct may need a new field (e.g., `float step_amp_mod_val`) to store the random value generated at the start of the step. Alternatively, we can reuse the PRNG deterministically if seeded by `step_idx` and `trigger_count`, but storing a latched float is cleaner.

## Backward Compatibility
*   **Struct Size:** Unchanged.
*   **Existing Presets:**
    *   The `_padding` byte is typically zero-initialized in existing ROM data. This maps to `PX_WSEQ_AMP_RAMP_DOWN` (Type 0).
    *   The `(1 << 3)` flag is not used in existing ROM data (values are 0).
    *   Since the effect is only applied if the **Flag** is set, existing sequences will remain unaffected regardless of the Type value (0).

## Work Checklist
- [x] Modify `PxWaveSequence` struct in `polysonix.h` to rename `_padding` to `amp_mod_type`.
- [x] Add `PxWseqAmpModType` enum/defines in `polysonix.h`.
- [x] Add `PX_WSEQ_AMP_MOD` flag definition.
- [x] Update `PxSeqState` in `polysonix.h` to include `float step_random_amp` (if needed for Random type).
- [x] Implement amplitude scaling logic in `PX_Process`.
- [x] (Optional) edit existing wseq entries who require this like percussives or pads and others that would benefit `px_wseq_rom.h` demonstrating these features.
