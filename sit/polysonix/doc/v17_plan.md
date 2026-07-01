# Polysonix v1.7 – Cross-Mod, Phase Distortion, Osc Sync, & Ring Mod Support

This release introduces the "secret sauce" analog/digital hybrid features to Polysonix. Leveraging the Triple Oscillator Architecture from v1.6, v1.7 adds deep interaction between oscillators.

## Features Overview

*   **Cross-Mod (FM/AM):** Oscillator $N$ modulates Oscillator $N-1$ frequency. Yamaha DX-style FM behavior but with custom bytecode waves.
*   **Phase Distortion (PD):** Casio CZ-style phase warping per oscillator for metallic, resonant tones.
*   **Oscillator Sync:** Hard/Soft sync where Oscillator $N$ resets to Oscillator $N-1$'s phase.
*   **Ring Modulation:** Oscillator $N$ amplitude modulates Oscillator $N-1$.

All features are per-oscillator, fully modulatable via the Matrix, and backward compatible (defaulting to off).

---

## Implementation Roadmap

### 1. Data Structures Updates (`polysonix.h`)

*   [ ] **Update `PxOscillator` Struct**
    *   Add flags and depth parameters for interactions.
    ```c
    typedef struct {
        // ... existing osc params ...
        bool cross_mod_enabled;    // Osc n modulates n-1
        float cross_mod_depth;     // 0.0–1.0 (matrix modulatable)

        bool phase_dist_enabled;   // Phase Distortion on/off
        float phase_dist_amount;   // 0.0–1.0 (warp factor)

        bool osc_sync_enabled;     // Sync to previous osc
        float osc_sync_softness;   // 0.0 (hard) to 1.0 (soft)

        bool ring_mod_enabled;     // Ring mod with previous osc
        float ring_mod_depth;      // 0.0–1.0
    } PxOscillator;
    ```

*   [ ] **Update `Voice` Struct**
    *   Add `float osc_output[PX_MAX_OSC_PER_VOICE]` to cache the output sample of each oscillator for use by the next oscillator (Ring Mod / Cross Mod) in the same cycle.
    *   Add `float osc_phase_prev_step[PX_MAX_OSC_PER_VOICE]` or logic to track phase wrapping for Sync detection.

*   [ ] **Update `PxModDestination` Enum**
    *   Add 12 new destinations (4 features * 3 oscillators).
    *   `PX_MOD_DEST_OSC[1-3]_CROSS_MOD_DEPTH`
    *   `PX_MOD_DEST_OSC[1-3]_PHASE_DIST`
    *   `PX_MOD_DEST_OSC[1-3]_SYNC_SOFTNESS`
    *   `PX_MOD_DEST_OSC[1-3]_RING_MOD_DEPTH`

### 2. API & Command Interface (`polysonix.h`)

*   [ ] **Add New Commands (`PxCommandType`)**
    *   `PX_CMD_SET_OSC_CROSS_MOD` (enabled/depth)
    *   `PX_CMD_SET_OSC_PHASE_DIST` (enabled/amount)
    *   `PX_CMD_SET_OSC_SYNC` (enabled/softness)
    *   `PX_CMD_SET_OSC_RING_MOD` (enabled/depth)

*   [ ] **Implement Public API Accessors**
    *   `PX_SetOscCrossMod(PxSynth* s, int osc_idx, bool enabled, float depth)`
    *   `PX_SetOscPhaseDist(PxSynth* s, int osc_idx, bool enabled, float amount)`
    *   `PX_SetOscSync(PxSynth* s, int osc_idx, bool enabled, float softness)`
    *   `PX_SetOscRingMod(PxSynth* s, int osc_idx, bool enabled, float depth)`
    *   Corresponding getters.

*   [ ] **Update `PX_ProcessCommands`**
    *   Handler logic to update `PxPatch` from the new commands.

### 3. Core Audio Engine (`PX_Process`)

*   [ ] **Initialization**
    *   Update `PX_Create` to set defaults (false/0.0) for new fields.
    *   Update `PX_UpdateUISnapshot` to copy new fields.

*   [ ] **DSP Implementation (Per-Voice, Per-Oscillator Loop)**
    *   **Phase Distortion:**
        *   Calculate effective amount (Base + Mod Matrix).
        *   Apply non-linear transform to `base_phase` *before* passing to VM:
            `vm_phase = base_phase + (sinf(base_phase * TWO_PI) * pd_amount)`
    *   **Oscillator Sync:**
        *   Logic: If `o > 0` and Sync enabled, check if `osc[o-1]` completed a cycle this frame.
        *   If triggered: Reset `osc_phase[o]`.
        *   Apply `osc_sync_softness` to blend the reset (Anti-aliased/Soft Sync).
    *   **VM Execution:**
        *   Pass transformed `vm_phase` to `execute_bytecode`.
    *   **Ring Modulation:**
        *   If `o > 0` and Ring Mod enabled:
        *   `sample *= v->osc_output[o-1] * effective_depth` (Blend wet/dry based on depth).
    *   **Cross-Modulation (FM):**
        *   If `o > 0` and Cross Mod enabled:
        *   Modulate `osc[o-1]`'s frequency for the *next* sample frame? Or modulate current oscillator `o` using `o-1` output?
        *   *Correction per design:* The prompt specifies "Osc2 modulates Osc1 frequency". This implies feedback or modifying the *previous* oscillator.
        *   Implementation: `v->osc_freq[o-1] += sample * depth * CONST`.
    *   **Output Caching:**
        *   Store final `sample` in `v->osc_output[o]` for the next oscillator to use.

### 4. Documentation & Validation

*   [ ] **Update `README.md`**
    *   Document the v1.7 features.
    *   Add examples of configuring FM or Sync sounds.
*   [ ] **Verification**
    *   Ensure CPU usage remains within real-time limits (~20-30% increase expected).
    *   Verify backward compatibility (patches without these features sound identical).
