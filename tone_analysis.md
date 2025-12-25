# Situation Library: Resonance Module Analysis

**Target Version:** v2.3.38
**Module Name:** Resonance (Procedural Synthesis)
**Goal:** Integrate a high-performance, zero-allocation, thread-safe software synthesizer into the Situation library.

## [x] 1. Architectural Philosophy
The Resonance module is designed for procedural audio generation. Unlike the existing audio system which plays pre-recorded samples, Resonance generates waveforms in real-time.

**Core Tenets**
*   **Zero Allocation:** All voices are pre-allocated in a fixed pool (SITUATION_MAX_TONES). Triggering a sound never calls malloc.
*   **Frame-Perfect Timing:** Time is tracked in audio frames (integers), not seconds (floats), preventing phase drift and envelope desynchronization over long uptimes.
*   **Fire-and-Forget:** The API is designed for game feel (UI blips, loot drops, procedural music). You define the envelope (ADSR) and duration upfront, and the engine handles the lifecycle.
*   **MiniAudio Integration:** Leverages ma_waveform for band-limited waveform generation (preventing aliasing artifacts on square/saw waves).

## [x] 2. Data Structures

### [x] 2.1. Enumerations (Public API)
We define the standard waveform shapes.

```c
typedef enum {
    SIT_WAVE_SINE,      // Pure tone
    SIT_WAVE_SQUARE,    // Retro/8-bit sound (has harmonics)
    SIT_WAVE_TRIANGLE,  // Mellow, flute-like
    SIT_WAVE_SAW,       // Harsh, string-like
    SIT_WAVE_NOISE      // White noise (for percussion/explosions) - Optional extension
} SituationWaveType;
```

### [x] 2.2. Internal Envelope State
The synth uses a Finite State Machine (FSM) for the ADSR envelope.

```c
typedef enum {
    SIT_ENV_IDLE = 0,
    SIT_ENV_ATTACK,     // Volume rising 0.0 -> 1.0
    SIT_ENV_DECAY,      // Volume falling 1.0 -> Sustain Level
    SIT_ENV_SUSTAIN,    // Volume holding at Sustain Level
    SIT_ENV_RELEASE     // Volume falling Sustain Level -> 0.0
} SituationEnvelopeState;
```

### [x] 2.3. The Voice Structure (Internal)
This struct represents a single "voice" of polyphony. It contains the waveform generator and the timing logic for the envelope.

```c
typedef struct {
    bool active;                // Is this slot in use?
    ma_waveform waveform;       // MiniAudio waveform generator state

    // Mix Parameters
    float volume_peak;          // Master volume for this note (0.0 - 1.0)
    float pan;                  // Stereo pan (-1.0 to 1.0)

    // Envelope State Machine
    SituationEnvelopeState state;
    uint64_t cursor_frames;     // How many frames have passed since trigger

    // Envelope Timings (in Frames) derived from input seconds
    uint64_t t_attack;          // Duration of Attack
    uint64_t t_decay;           // Duration of Decay
    uint64_t t_hold;            // Duration of Sustain (Hold time)
    uint64_t t_release;         // Duration of Release

    // Levels
    float level_sustain;        // Volume level during hold phase (0.0 - 1.0)
} SituationTone;
```

### [x] 2.4. Global State Update
Update `_SituationAudioState` to include the pool and the MIDI table.

```c
#define SITUATION_MAX_TONES 64 // 64-voice polyphony

typedef struct {
    // ... existing audio state ...

    // Resonance Module State
    SituationTone tone_pool[SITUATION_MAX_TONES];
    // Note: The MIDI table is static const data, doesn't need to be in the struct instance
} _SituationAudioState;
```

## [x] 3. The MIDI Frequency Table (Immutable Data)
This table replaces runtime `pow()` calculations. It maps MIDI note numbers (0-127) to frequency (Hz).

```c
/* Implementation Section */
static const float g_midi_note_frequencies[128] = {
    8.1758f,   8.66196f,  9.17702f,  9.72272f,  10.3009f,  10.9134f,  11.5623f,  12.2499f,  12.9783f,  13.75f,    14.5676f,  15.4339f,
    16.3516f,  17.3239f,  18.3540f,  19.4454f,  20.6017f,  21.8268f,  23.1247f,  24.4997f,  25.9565f,  27.5f,     29.1352f,  30.8677f,
    32.7032f,  34.6478f,  36.7081f,  38.8909f,  41.2034f,  43.6535f,  46.2493f,  48.9994f,  51.9131f,  55.0f,     58.2705f,  61.7354f,
    65.4064f,  69.2957f,  73.4162f,  77.7817f,  82.4069f,  87.3071f,  92.4986f,  97.9989f,  103.826f,  110.0f,    116.541f,  123.471f,
    130.813f,  138.591f,  146.832f,  155.563f,  164.814f,  174.614f,  184.997f,  195.998f,  207.652f,  220.0f,    233.082f,  246.942f,
    261.626f,  277.183f,  293.665f,  311.127f,  329.628f,  349.228f,  369.994f,  391.995f,  415.305f,  440.0f,    466.164f,  493.883f,
    523.251f,  554.365f,  587.330f,  622.254f,  659.255f,  698.456f,  739.989f,  783.991f,  830.609f,  880.0f,    932.328f,  987.767f,
    1046.50f,  1108.73f,  1174.66f,  1244.51f,  1318.51f,  1396.91f,  1479.98f,  1567.98f,  1661.22f,  1760.0f,   1864.66f,  1975.53f,
    2093.00f,  2217.46f,  2349.32f,  2489.02f,  2637.02f,  2793.83f,  2959.96f,  3135.96f,  3322.44f,  3520.0f,   3729.31f,  3951.07f,
    4186.01f,  4434.92f,  4698.64f,  4978.03f,  5274.04f,  5587.65f,  5919.91f,  6271.93f,  6644.88f,  7040.0f,   7458.62f,  7902.13f,
    8372.02f,  8869.84f,  9397.27f,  9956.06f,  10548.1f,  11175.3f,  11839.8f,  12543.9f
};
```

## [ ] 4. Public API Definitions

### [ ] 4.1. Core Generator

```c
/**
 * @brief Plays a procedural tone with an ADSR envelope.
 * @param type Waveform type (Sine, Square, etc.)
 * @param frequency Hz (e.g., 440.0f)
 * @param volume 0.0 to 1.0
 * @param attack_sec Time to fade in (seconds)
 * @param decay_sec Time to drop to sustain level (seconds)
 * @param sustain_level Volume during hold (0.0 to 1.0)
 * @param release_sec Time to fade out (seconds)
 * @param hold_sec Duration to hold the note before releasing (seconds)
 */
SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume,
                              float attack_sec, float decay_sec, float sustain_level,
                              float release_sec, float hold_sec);
```

### [ ] 4.2. MIDI Wrapper

```c
/**
 * @brief Plays a tone defined by a MIDI note number.
 * @param note MIDI note index (0-127). E.g., 60 is Middle C.
 * @param ... (Same ADSR params as above)
 */
SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume,
                                  float attack, float decay, float sustain, float release, float hold);
```

### [ ] 4.3. Panic

```c
/**
 * @brief Immediately stops all procedural tones.
 */
SITAPI void SituationStopAllTones(void);
```

## [ ] 5. Implementation Logic & Hardening

### [ ] 5.1. Input Validation (Idiot-Proofing)
The API must reject invalid inputs to prevent undefined behavior or audio glitches.

*   **Initialization Check:** Ensure `SituationIsInitialized()` returns true.
*   **Frequency:** Must be positive (`frequency > 0`). If <= 0, log a warning and return.
*   **Volume:** Clamp between 0.0f and 1.0f.
*   **Time Durations:**
    *   `attack`, `decay`, `release`, `hold` must be non-negative. If negative, clamp to 0.0f.
*   **Waveform Type:** Validate enum range.
*   **MIDI Note:** For `SituationPlayMidiNote`, clamp `note` between 0 and 127.

### [ ] 5.2. Allocation Strategy (Voice Stealing)
When `SituationPlayTone` is called:
1.  **Thread Safety:** Lock `sit_audio.audio_queue_mutex` to prevent race conditions with the audio callback.
2.  **Pool Iteration:** Iterate `tone_pool`. Look for `!active`.
3.  **Voice Stealing:** If all 64 voices are full:
    *   **Priority 1:** Steal voices in `SIT_ENV_RELEASE` phase (already fading out). Pick the one closest to finishing.
    *   **Priority 2:** Steal the oldest voice (highest `cursor_frames`).
    *   **Click Prevention:** Ideally, stealing should cross-fade, but for a simple implementation, stealing implies an abrupt cut. *Mitigation:* The ADSR envelope logic will naturally start the new note at `env_amp = 0.0` (Attack phase), which prevents a click at the *start* of the new note. The cut of the old note might still click, but this is acceptable for a "Panic" steal scenario.
4.  **Configuration:**
    *   Map `SituationWaveType` to `ma_waveform_type` (e.g., `ma_waveform_type_sine`).
    *   **Re-Initialize Waveform:** Use `ma_waveform_init(&config, &t->waveform)` to reset the generator state cleanly.
    *   **Time conversion:** `frames = (uint64_t)(seconds * device_sample_rate)`.
    *   **Numerical Stability:** Check for overflow when converting seconds to frames? (uint64 max is huge, practically impossible to overflow with reasonable seconds).
    *   Set `state = SIT_ENV_ATTACK`.
    *   Set `active = true`.
5.  **Unlock:** Release `sit_audio.audio_queue_mutex`.

### [ ] 5.3. The Mixing Pipeline (Audio Thread)
Inside `sit_miniaudio_data_callback`, after the standard sound mixing loop:

**CRITICAL:** This runs on the audio thread. It must be fast.
**LOCKING:** We must lock `sit_audio.audio_queue_mutex` before iterating the tone pool to ensure we don't process a half-initialized voice.

```c
// --- RESONANCE MIXER ---
// Note: pOutput already contains mixed audio from standard sounds. We ADD to it.
float* out_ptr = (float*)pOutput; // Assuming f32 format

// Lock mutex for thread safety
mtx_lock(&sit_audio.audio_queue_mutex);

for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
    SituationTone* t = &sit_audio.tone_pool[i];
    if (!t->active) continue;

    // Process in chunks (e.g., per frame) for envelope precision
    for (uint32_t f = 0; f < frameCount; ++f) {

        // --- 1. Envelope Logic ---
        float env_amp = 0.0f;

        switch (t->state) {
            case SIT_ENV_ATTACK:
                if (t->cursor_frames >= t->t_attack) {
                    t->state = SIT_ENV_DECAY;
                    t->cursor_frames = 0; // Reset local phase timer
                    env_amp = 1.0f;
                } else {
                    // Prevent divide by zero if attack is 0
                    env_amp = (t->t_attack > 0) ? ((float)t->cursor_frames / (float)t->t_attack) : 1.0f;
                }
                break;

            case SIT_ENV_DECAY:
                if (t->cursor_frames >= t->t_decay) {
                    t->state = SIT_ENV_SUSTAIN;
                    t->cursor_frames = 0;
                    env_amp = t->level_sustain;
                } else {
                    float progress = (t->t_decay > 0) ? ((float)t->cursor_frames / (float)t->t_decay) : 1.0f;
                    env_amp = 1.0f - (progress * (1.0f - t->level_sustain));
                }
                break;

            case SIT_ENV_SUSTAIN:
                if (t->cursor_frames >= t->t_hold) {
                    t->state = SIT_ENV_RELEASE;
                    t->cursor_frames = 0;
                    env_amp = t->level_sustain;
                } else {
                    env_amp = t->level_sustain;
                }
                break;

            case SIT_ENV_RELEASE:
                if (t->cursor_frames >= t->t_release) {
                    t->active = false; // Note finished
                    env_amp = 0.0f;
                } else {
                    float progress = (t->t_release > 0) ? ((float)t->cursor_frames / (float)t->t_release) : 1.0f;
                    env_amp = t->level_sustain * (1.0f - progress);
                }
                break;

            default: env_amp = 0.0f; break;
        }

        if (!t->active) break; // Optimization: Stop processing if release finished mid-buffer

        // --- 2. Waveform Generation ---
        float sample;
        ma_waveform_read_pcm_frames(&t->waveform, &sample, 1, NULL);

        // --- 3. Mix & Pan ---
        // Simple stereo panning (Equal Power approximation or Linear)
        // Assuming 2 output channels for simplicity in this snippet
        float final_amp = sample * env_amp * t->volume_peak;

        // Left Channel
        out_ptr[f * 2 + 0] += final_amp * (t->pan <= 0 ? 1.0f : 1.0f - t->pan);
        // Right Channel
        out_ptr[f * 2 + 1] += final_amp * (t->pan >= 0 ? 1.0f : 1.0f + t->pan);

        t->cursor_frames++;
    }
}

mtx_unlock(&sit_audio.audio_queue_mutex);
```

### [ ] 5.4. Cleanup (Avoid Leaks)
`ma_waveform` generally doesn't hold heavy resources, but `ma_waveform_uninit` exists.
In `_SituationCleanupSubsystems` (and `SituationStopAllTones`):
1. Lock mutex.
2. Iterate pool.
3. If `active`, call `ma_waveform_uninit(&t->waveform)`.
4. Set `active = false`.
5. Unlock mutex.

## [ ] 6. Lifecycle Integration

### [ ] 6.1. Init
In `_SituationInitSubsystems`:

```c
memset(sit_audio.tone_pool, 0, sizeof(sit_audio.tone_pool));
// No special init needed for pool items until played, as they are POD + ma_waveform
```

### [ ] 6.2. Shutdown
In `_SituationCleanupSubsystems`:

```c
// ma_waveform doesn't usually hold heap memory, but good to be explicit
mtx_lock(&sit_audio.audio_queue_mutex);
for(int i=0; i<SITUATION_MAX_TONES; ++i) {
    if(sit_audio.tone_pool[i].active) {
        ma_waveform_uninit(&sit_audio.tone_pool[i].waveform);
        sit_audio.tone_pool[i].active = false;
    }
}
mtx_unlock(&sit_audio.audio_queue_mutex);
```

## [ ] 7. Use Case Examples (for Documentation)

**1. UI Hover Sound (Short, High Pitch, Sine)**

```c
SituationPlayTone(SIT_WAVE_SINE, 880.0f, 0.5f, 0.01f, 0.05f, 0.0f, 0.0f, 0.0f);
// Attack 0.01s, Decay 0.05s to 0 vol. Just a "blip".
```

**2. Retro Jump Sound (Square Wave, Pitch Slide not supported yet, but Envelope helps)**

```c
SituationPlayTone(SIT_WAVE_SQUARE, 440.0f, 0.3f, 0.05f, 0.2f, 0.0f, 0.1f, 0.0f);
```

**3. Procedural Music (Arpeggio)**

```c
// Called every 0.2 seconds by a timer
static int note_index = 0;
int notes[] = {60, 64, 67, 72}; // C Major
SituationPlayMidiNote(notes[note_index], SIT_WAVE_TRIANGLE, 0.4f, 0.01f, 0.1f, 0.8f, 0.2f, 0.1f);
note_index = (note_index + 1) % 4;
```
