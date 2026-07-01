# Tone Synthesizer Extraction Plan

## Goal
Extract the tone synthesizer code from `situation_impl_audio.h` into a new modular header `sit/aud/tone_synth.h`, following the pattern established by `reverb.h`, `echo.h`, etc.

## Components to Extract

### 1. Constants & Macros
- `#define TONE_INDEX_MASK` (line ~535)
- `#define TONE_GEN_SHIFT` (line ~536)

### 2. Helper Functions (Internal)
- `_MakeToneHandle()` - Creates tone handle from index and generation
- `_IsValidToneHandle()` - Validates tone handle
- `_GetToneFromHandle()` - Retrieves tone pointer from handle

### 3. Public API Functions
- `SituationPlayToneEx()` - Main tone generation function with full ADSR
- `SituationPlayTone()` - Simplified wrapper
- `SituationStopTone()` - Stops a specific tone
- `SituationStopAllTones()` - Panic button, stops all tones
- `SituationPlayMidiNote()` - MIDI note wrapper
- `SituationSetToneReverbEnabled()` - Enable/disable tone reverb
- `SituationSetToneReverbParameters()` - Configure tone reverb

### 4. MIDI Frequency Table
- `g_midi_note_frequencies[128]` - Static const array mapping MIDI notes to Hz

### 5. Tone Rendering/Mixing Code
- The tone processing loop inside `sit_miniaudio_data_callback`
- Envelope state machine (ATTACK, DECAY, SUSTAIN, RELEASE)
- Waveform generation (ma_waveform_read_pcm_frames / ma_noise_read_pcm_frames)
- Pan and volume mixing

## Dependencies

### From situation_impl.h:
- `SituationTone` structure definition
- `SituationEnvelopeState` enum
- `tone_pool[SITUATION_MAX_TONES]` in `_SituationAudioState`
- `tone_generations[SITUATION_MAX_TONES]` in `_SituationAudioState`

### From situation_api.h:
- `SituationToneHandle` typedef
- `SituationWaveType` enum
- `SITUATION_MAX_TONES` constant

### External:
- miniaudio (ma_waveform, ma_noise)
- Threading (mtx_lock/unlock)
- Audio state (`sit_audio` global)

## File Structure Pattern

Following the pattern from `sit/aud/reverb.h` and `sit/aud/echo.h`:

```c
/***************************************************************************************************
*   sit/aud/tone_synth.h - Procedural Tone Synthesis Module
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   DESCRIPTION:
*   64-voice polyphonic tone synthesizer with ADSR envelopes.
*   Supports sine, square, triangle, sawtooth, and noise waveforms.
*   MIDI note mapping and handle-based voice management.
***************************************************************************************************/

#ifndef SITUATION_TONE_SYNTH_H
#define SITUATION_TONE_SYNTH_H

// Constants
// Helper functions
// MIDI table
// Public API implementations
// Tone rendering function (called from audio callback)

#endif // SITUATION_TONE_SYNTH_H
```

## Integration Point

The `#include "sit/aud/tone_synth.h"` should be added in `situation_impl_audio.h` after the other audio effect includes:

```c
#include "sit/aud/reverb.h"
#include "sit/aud/echo.h"
#include "sit/aud/exciter.h"
#include "sit/aud/mastering_amp.h"
#include "sit/aud/studio_reverb.h"
#include "sit/aud/tone_synth.h"  // <-- NEW
```

## Next Steps

1. Identify exact line ranges for each component
2. Create `sit/aud/tone_synth.h` with proper header
3. Move code sections with full documentation
4. Add include directive in `situation_impl_audio.h`
5. Test compilation
6. Verify functionality with test_single_tone.c
