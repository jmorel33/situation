# Polysonix v1.8: Time-Locked Wave Sequencing Plan

## 1. Objective
Implement "Time-Locked" mode for the Wave Sequencer. In this mode, step durations (defined in the ROM as "cycles at Reference Frequency") are dynamically scaled based on the oscillator's playback frequency to maintain a constant time duration.

## 2. Core Requirements
*   **Constant Duration**: A step with duration `N` should last the same amount of time regardless of whether the oscillator is playing C3 or C5.
*   **Cycle-Based Precision**: The implementation must rely on `cycles_counter` for zero-overhead tracking during the loop, calculating the `target_cycles` threshold only *once* per step transition.
*   **Correct Integration**: The calculation must account for the pitch offset of the *new* step to prevent duration drift during intervals.

## 3. Data Structures

### `polysonix.h`

#### `PxPatch` (Global Settings)
- [x] `bool wseq_fixed_time;` (Enable Time-Locked mode)
- [x] `float wseq_ref_freq;` (Reference frequency, default 440.0 Hz)

#### `PxSeqState` (Internal State)
- [x] `uint32_t current_step_target_cycles;` (Cached target for the active step)

### `PxCommandType`
- [x] `PX_CMD_SET_WSEQ_FIXED_TIME`
- [x] `PX_CMD_SET_WSEQ_REF_FREQ`

## 4. Implementation Logic

### Initialization (`PX_NoteOn_internal`)
When a sequence starts:
- [x] Determine `base_freq` (Oscillator's starting frequency).
- [x] Determine `step_ratio` (Pitch ratio of Step 0).
- [x] Calculate `projected_freq = base_freq * step_ratio`.
- [x] If `wseq_fixed_time`: `target = step.duration * (projected_freq / wseq_ref_freq)`
- [x] Else: `target = step.duration`
- [x] Clamp `target` to `max(1)`.
- [x] Store in `sq->current_step_target_cycles`.

### Step Transition (`PX_Process`)
Inside the `cycle_completed` block, when advancing to a new step:
- [x] Retrieve `current_osc_freq` (Effective frequency of the *current* cycle).
- [x] Retrieve `old_step_ratio` (Pitch ratio of the *finished* step).
- [x] Retrieve `new_step_ratio` (Pitch ratio of the *next* step).
- [x] Calculate `projected_freq = current_osc_freq * (new_step_ratio / old_step_ratio)`.
    *   *Note: This adjusts the current frequency to what it WILL be in the next step, assuming no other modulations change instantly.*
- [x] If `wseq_fixed_time`: `target = next_step.duration * (projected_freq / wseq_ref_freq)`
- [x] Else: `target = next_step.duration`
- [x] Clamp `target` to `max(1)`.
- [x] Store in `sq->current_step_target_cycles`.

### Execution Loop (`PX_Process`)
Inside the oscillator loop:
- [x] Check: `if (sq->cycles_counter >= sq->current_step_target_cycles)` to advance step.

## 5. API Updates
- [x] `PX_SetWSeqFixedTime(PxSynth* s, bool enabled)`
- [x] `PX_GetWSeqFixedTime(PxSynth* s)`
- [x] `PX_SetWSeqRefFreq(PxSynth* s, float freq)`
- [x] `PX_GetWSeqRefFreq(PxSynth* s)`

## 6. Serialization
- [x] Add `wseq_fixed_time` and `wseq_ref_freq` to `px_serialize_patch_impl` in `px_patching.h`.
- [x] Add `wseq_fixed_time` and `wseq_ref_freq` to `px_deserialize_patch_impl` in `px_patching.h`.
- [x] Update `UISnapshot` copying logic in `polysonix.h`.

## 7. Rationale: Why Cycle-Based Timing is Essential for Bass

You're absolutely right — the cycle-based step duration becomes **even more beneficial** (and the alternative more problematic) at **lower frequencies**, especially below ~250 Hz.

Here's why it gets noticeably worse with time-based (fixed ms) timing on bass notes:

### The problem at low frequencies

Take a typical low note, e.g. A1 = 55 Hz (very common bass range):
- One full cycle = 1 / 55 ≈ **18.18 ms**

If you use **fixed time-based steps** (e.g. 20 ms per step):
- At 55 Hz → step change happens after ~1.1 cycles → often **mid-waveform**
- Result: The waveform gets cut at a random point (not at zero or peak) → **big voltage jump** → **loud click or thump**

Worse examples:
- C1 = 32.7 Hz → cycle ≈ 30.6 ms
  → A 20 ms step = only ~0.65 cycles → almost guaranteed mid-cycle cut → very audible glitch
- Sub-bass (e.g. 40–60 Hz) → same issue, but even more noticeable because low frequencies carry more energy

The click is **proportional to the amplitude at the cut point** — and low notes tend to have longer, more sustained waveforms with higher peak energy → the discontinuity is **much more obvious** than on high notes.

### Cycle-based timing shines here

With your current cycle-based system:
- Step duration = N cycles (N = duration_cycles)
- Transition always happens **after a whole (or nearly whole) number of cycles**
- Waveform continuity is preserved → phase is aligned (or reset safely)
- Even at 40 Hz, a step of 10 cycles = 250 ms → clean handoff, no mid-cycle cut
- Small timing variance (e.g. 1–2 ms off from "perfect" rhythm) is **inaudible** at low frequencies — human perception of timing jitter drops dramatically below ~200 Hz (we rely more on amplitude envelope than precise phase)

### Quick comparison table (at low freq)

| Frequency | Cycle time | Fixed 20 ms step → cycles per step | Risk of mid-cycle cut | Click severity | Cycle-based behavior |
|-----------|------------|------------------------------------|-----------------------|----------------|----------------------|
| 1000 Hz   | 1 ms       | 20 cycles                          | Low (many cycles)     | Minor          | Fast, energetic seq  |
| 250 Hz    | 4 ms       | 5 cycles                           | Medium                | Noticeable     | Still clean          |
| 100 Hz    | 10 ms      | 2 cycles                           | High                  | Loud           | Very clean           |
| 55 Hz     | 18 ms      | ~1.1 cycles                        | Very high             | Very loud      | Perfect continuity   |
| 40 Hz     | 25 ms      | 0.8 cycles                         | Extreme               | Thump/click    | Still safe           |

**Conclusion**
At low frequencies, time-based stepping turns into a **glitch machine** — especially on sustained bass tones where clicks are most exposed.
Your cycle-locked design is **not just better** — it's **essential** for clean, professional-sounding bass and sub-bass sequencing.

The slight non-uniform timing across octaves is a **tiny price** compared to the alternative: audible artifacts that ruin the low end.

So yes — stick with it.
It's one of those cases where "musical correctness" (no clicks) trumps "mathematical metronomic perfection".
Great call protecting the waveform integrity. 🎛️
