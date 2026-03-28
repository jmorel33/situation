/***************************************************************************************************
*
*   sit/aud/tone_synth.h - Procedural Tone Synthesis Module
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   64-voice polyphonic tone synthesizer with ADSR envelope control.
*
*   This module provides real-time procedural audio generation for:
*     • UI feedback sounds (clicks, hovers, notifications)
*     • Retro game audio (8-bit style square/triangle waves)
*     • Procedural music and sound effects
*     • MIDI-based note playback
*
*   Supported waveforms:
*     • Sine - Pure tone, smooth
*     • Square - Retro/8-bit, harsh harmonics
*     • Triangle - Mellow, flute-like
*     • Sawtooth - Bright, string-like
*     • White Noise - Percussion, wind, explosions
*
*   Key Features:
*     • Zero-allocation voice management (fixed 64-voice pool)
*     • Frame-perfect ADSR envelope timing
*     • Handle-based voice control with generation counters
*     • Voice stealing with priority (release phase > newest)
*     • MIDI note number to frequency conversion
*     • Thread-safe voice triggering and control
*     • Routes to mixer for effects processing (reverb via aux buses)
*
*   Architecture:
*     • Built on miniaudio's ma_waveform and ma_noise generators
*     • Integrates with the main audio callback for mixing
*     • Coexists with the mixer architecture for backward compatibility
*
***************************************************************************************************/

#ifndef SITUATION_TONE_SYNTH_H
#define SITUATION_TONE_SYNTH_H

// ================================================================================================
// TONE HANDLE MANAGEMENT
// ================================================================================================

// Handle packing: 16-bit slot index + 16-bit generation counter
#define TONE_INDEX_MASK   0x0000FFFFu
#define TONE_GEN_MASK     0xFFFF0000u
#define TONE_GEN_SHIFT    16

/**
 * @brief [INTERNAL] Creates a tone handle from slot index and generation counter.
 * @details Packs a 16-bit slot index and 16-bit generation into a 32-bit handle.
 *          This allows safe handle validation even after voice reuse.
 *
 * @param index The slot index in the tone pool (0-63)
 * @param gen The generation counter for this slot
 * @return A packed SituationToneHandle
 */
static inline SituationToneHandle _MakeToneHandle(uint16_t index, uint16_t gen) {
    return ((uint32_t)gen << TONE_GEN_SHIFT) | index;
}

/**
 * @brief [INTERNAL] Validates a tone handle.
 * @details Checks if the handle's slot index is in range and if the generation
 *          counter matches the current generation for that slot.
 *
 * @param handle The tone handle to validate
 * @return true if the handle is valid and the tone is active, false otherwise
 */
static inline bool _IsValidToneHandle(SituationToneHandle handle) {
    uint16_t index = handle & TONE_INDEX_MASK;
    if (index >= SITUATION_MAX_TONES) return false;
    uint16_t gen = handle >> TONE_GEN_SHIFT;
    return sit_audio.tone_generations[index] == gen && sit_audio.tone_pool[index].active;
}

/**
 * @brief [INTERNAL] Retrieves a tone pointer from a handle.
 * @details Validates the handle and returns a pointer to the tone structure,
 *          or NULL if the handle is invalid or the tone is no longer active.
 *
 * @param handle The tone handle to resolve
 * @return Pointer to SituationTone if valid, NULL otherwise
 */
static inline SituationTone* _GetToneFromHandle(SituationToneHandle handle) {
    uint16_t index = handle & TONE_INDEX_MASK;
    return (index < SITUATION_MAX_TONES && _IsValidToneHandle(handle))
        ? &sit_audio.tone_pool[index]
        : NULL;
}

// ================================================================================================
// PUBLIC API - TONE CONTROL
// ================================================================================================

/**
 * @brief Stops all active tones immediately by transitioning them to the release phase.
 * @details This is a "panic" function that gracefully fades out all playing tones.
 *          Tones in the release phase are not affected (already fading out).
 *          This is useful for:
 *            • Clearing audio on scene transitions
 *            • Emergency audio reset
 *            • Debugging audio issues
 *
 * @note Thread-safe. Can be called from any thread.
 * @note Tones will fade out according to their release time, not cut abruptly.
 *
 * @see SituationStopTone(), SituationPlayToneEx()
 */
SITAPI void SituationStopAllTones(void) {
    if (!SituationIsInitialized()) return;
    _SitPushToneStopCommand(0); // 0 means all
}

/**
 * @brief Stops a specific tone by transitioning it to the release phase.
 * @details Gracefully fades out the tone according to its release time.
 *          If the tone is already in release or has finished, this has no effect.
 *
 * @param handle The tone handle returned by SituationPlayToneEx()
 *
 * @note Thread-safe. Can be called from any thread.
 * @note Invalid or expired handles are safely ignored.
 *
 * @see SituationPlayToneEx(), SituationStopAllTones()
 */

SITAPI void SituationStopTone(SituationToneHandle handle) {
    if (!SituationIsInitialized() || handle == 0) return;
    _SitPushToneStopCommand(handle);
}


// ================================================================================================
// PUBLIC API - TONE GENERATION
// ================================================================================================

/**
 * @brief Plays a procedural tone with full ADSR envelope and pan control.
 * @details This is the main tone generation function. It allocates a voice from the 64-voice
 *          polyphonic pool and configures it with the specified waveform, frequency, and envelope.
 *
 * Voice Allocation Strategy:
 *   1. First, searches for an empty (inactive) slot
 *   2. If full, steals a voice in the release phase (prioritizes voices furthest in release)
 *   3. If no releasing voices, steals the newest active voice (lowest cursor_frames)
 *
 * @param type Waveform type (SIT_WAVE_SINE, SIT_WAVE_SQUARE, SIT_WAVE_TRIANGLE, SIT_WAVE_SAW, SIT_WAVE_NOISE)
 * @param frequency Frequency in Hz (e.g., 440.0f for A4). Ignored for SIT_WAVE_NOISE. Must be > 0 for other types.
 * @param volume Master volume (0.0 to 1.0). Values outside this range are clamped.
 * @param pan Stereo pan (-1.0 = full left, 0.0 = center, 1.0 = full right). Values outside this range are clamped.
 * @param attack_sec Attack time in seconds (0.0 to fade in instantly). Time for volume to rise from 0.0 to 1.0.
 * @param decay_sec Decay time in seconds. Time for volume to fall from 1.0 to sustain_level.
 * @param sustain_level Sustain volume level (0.0 to 1.0). Volume held during the hold phase.
 * @param release_sec Release time in seconds. Time for volume to fade from sustain_level to 0.0 after hold ends.
 * @param hold_sec Hold time in seconds. Duration to sustain the note. Use -1.0 for infinite hold (manual stop required).
 *
 * @return A SituationToneHandle that can be used with SituationStopTone(), or 0 on failure.
 *
 * @note All time parameters are converted to frame counts internally for frame-perfect timing.
 * @note Negative time values (except hold_sec) are clamped to 0.0.
 * @note Thread-safe. Can be called from any thread.
 * @note If all 64 voices are in use, the function will steal a voice (see allocation strategy above).
 *
 * @see SituationPlayTone(), SituationPlayMidiNote(), SituationStopTone()
 */

SITAPI SituationToneHandle SituationPlayToneEx(SituationWaveType type, float frequency, float volume, float pan, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec) {
    if (!SituationIsInitialized()) return 0;

    static atomic_uint global_tone_id_gen = 0;
    uint32_t new_id = atomic_fetch_add(&global_tone_id_gen, 1) + 1;

    _SitPushTonePlayCommand(type, frequency, volume, pan, attack_sec, decay_sec, sustain_level, release_sec, hold_sec, new_id);

    return new_id;
}

SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec) {
    SituationPlayToneEx(type, frequency, volume, 0.0f, attack_sec, decay_sec, sustain_level, release_sec, hold_sec);
}

/**
 * @brief Plays a tone using a MIDI note number instead of frequency.
 * @details Converts a MIDI note number (0-127) to frequency using the standard MIDI tuning table
 *          (A4 = 440Hz), then calls SituationPlayTone().
 *
 * MIDI Note Reference:
 *   • Middle C (C4) = 60
 *   • A4 (concert pitch) = 69
 *   • C0 (lowest) = 0
 *   • G9 (highest) = 127
 *
 * @param note MIDI note number (0-127). Values outside this range are clamped.
 * @param type Waveform type
 * @param volume Master volume (0.0 to 1.0)
 * @param attack Attack time in seconds
 * @param decay Decay time in seconds
 * @param sustain Sustain volume level (0.0 to 1.0)
 * @param release Release time in seconds
 * @param hold Hold time in seconds (-1.0 for infinite)
 *
 * @note The MIDI frequency table is defined in situation_api.h as SITUATION_MIDI_NOTE_FREQUENCY[].
 * @note This function does not return a handle. Use SituationPlayToneEx() with manual frequency if you need control.
 *
 * @see SituationPlayTone(), SituationPlayToneEx(), SITUATION_MIDI_NOTE_FREQUENCY
 */
SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack, float decay, float sustain, float release, float hold) {
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    float freq = SITUATION_MIDI_NOTE_FREQUENCY[note];
    SituationPlayTone(type, freq, volume, attack, decay, sustain, release, hold);
}

#endif // SITUATION_TONE_SYNTH_H
