// --- v1.5 ROM (Populated) ---
// 128 Sequences organized into 16 themed banks of 8.
//
// "Ham Crazy" Edition - Enhanced with complex flag combinations,
// generative probability, and math-based waveforms.

#ifndef PX_WSEQ_ROM_H
#define PX_WSEQ_ROM_H

/**
 * =========================================================================================
 *  POLYSONIX WAVE SEQUENCE REFERENCE
 * =========================================================================================
 *
 *  STRUCT: PxWaveSequence (Global Settings)
 *  ----------------------------------------
 *  const char* name            : Descriptive name of the sequence.
 *  uint8_t  end_action         : Action when sequence finishes (or PX_WSEQ_END flag hit).
 *                                - PX_WSEQ_END_STOP     (0): Stop voice (Release phase).
 *                                - PX_WSEQ_END_HOLD     (1): Hold last step indefinitely.
 *                                - PX_WSEQ_END_LOOP     (2): Loop back to 'seq_loop_start_idx' (step with LOOP_POINT flag, or 0).
 *                                - PX_WSEQ_END_PINGPONG (3): Reverse playback direction.
 *                                - PX_WSEQ_END_REVERSE  (4): Play backwards.
 *
 *  uint8_t  glide_mode         : Pitch transition style.
 *                                - PX_WSEQ_GLIDE_OFF    (0): Stepped pitch changes.
 *                                - PX_WSEQ_GLIDE_STEP   (1): Linear glide within step duration.
 *                                - PX_WSEQ_GLIDE_SMOOTH (2): Continuous RC filter glide (ignores step bounds).
 *
 *  uint8_t  adsr_retrig_phase  : Target ADSR state on PX_WSEQ_RETRIG_ADSR.
 *                                - 1: Attack, 2: Decay, 3: Sustain, 4: Release.
 *
 *  uint8_t  prob_mute_score    : % Chance to mute step if PX_WSEQ_USE_PROB_MUTE is set.
 *                                - Range: 1-100. Value of 0 defaults to 50%.
 *  uint8_t  prob_skip_score    : % Chance to skip step if PX_WSEQ_USE_PROB_SKIP is set.
 *                                - Range: 1-100. Value of 0 defaults to 50%.
 *  uint8_t  rnd_octave_range   : % Chance to shift +/- 1 octave if PX_WSEQ_USE_RND_OCTAVE is set.
 *                                - Range: 0-100.
 *  uint8_t  reset_lfo_pos      : Boolean (1=Yes, 0=No) to reset LFOs on sequence start.
 *
 *  uint16_t rnd_wave_low       : Start index for random wave range (PX_WSEQ_USE_RND_WAVE). Range: 0-65535.
 *  uint16_t rnd_wave_high      : End index for random wave range. Range: 0-65535.
 *
 *  int8_t   [x]_mod_src        : Mod Source (-1 = None/Internal, 0-6 = PxModSource enum).
 *  float    [x]_depth          : Base effect depth (0.0 - 1.0).
 *  int8_t   amp_mod_type       : PxWseqAmpModType (0-7). Shape of amplitude envelope.
 *                                - PX_WSEQ_AMP_RAMP_DOWN (0): Linear fade out (Pluck).
 *                                - PX_WSEQ_AMP_RAMP_UP   (1): Linear fade in (Reverse).
 *                                - PX_WSEQ_AMP_TRIANGLE  (2): Triangle (Swell/Fade).
 *                                - PX_WSEQ_AMP_SINE      (3): Sine Hump.
 *                                - PX_WSEQ_AMP_SQUARE    (4): 50% Gate (Chopper).
 *                                - PX_WSEQ_AMP_PULSE_25  (5): 25% Gate (Short blip).
 *                                - PX_WSEQ_AMP_EXP_DOWN  (6): Exponential Pluck (Percussive).
 *                                - PX_WSEQ_AMP_RANDOM    (7): Random Level (S&H), latched per step.
 *
 *
 *  STRUCT: PxWaveSeqStep (Per-Step Data)
 *  -------------------------------------
 *  uint16_t wave_idx           : Waveform Index (0-65535).
 *  uint16_t duration_cycles    : Duration in oscillator cycles (1-65535).
 *                                - Values < 1 clamped to 1.
 *  int16_t  pitch_offset       : Tuning in cents. 1200 = 1 Octave. Range: -32768 to +32767.
 *  uint16_t flags              : Bitwise logic flags.
 *
 *
 *  BIT FLAGS (16-bit)
 *  ------------------
 *  [Flow Control]
 *  (1<<0) PX_WSEQ_END             : Force sequence end action here.
 *  (1<<1) PX_WSEQ_LOOP_POINT      : Mark this step as the start point for LOOP mode.
 *  (1<<2) PX_WSEQ_JUMP_RANDOM     : Jump to a random step index (0-63).
 *  (1<<3) PX_WSEQ_AMP_MOD         : Enable per-step Amplitude Modulation.
 *
 *  [Reset/Mod]
 *  (1<<4) PX_WSEQ_RESET_LFO       : Reset LFO phase to 0.
 *  (1<<5) PX_WSEQ_RETRIG_ADSR     : Retrigger Voice ADSRs.
 *  (1<<6) PX_WSEQ_LOCK_PHASE      : Hard Sync oscillator phase to 0.
 *  (1<<7) PX_WSEQ_GLIDE           : Enable per-step exponential glide (overrides Linear).
 *
 *  [Generative]
 *  (1<<8) PX_WSEQ_USE_PROB_MUTE   : Randomly mute this step (uses prob_mute_score).
 *  (1<<9) PX_WSEQ_USE_PROB_SKIP   : Randomly skip this step (zero duration) (uses prob_skip_score).
 *  (1<<10) PX_WSEQ_USE_RND_OCTAVE : Randomly offset pitch +/- 1200 cents (uses rnd_octave_range).
 *  (1<<11) PX_WSEQ_USE_RND_WAVE   : Randomly pick wave_idx from [low, high] range.
 *
 *  [Timbre/FX]
 *  (1<<12) PX_WSEQ_REVERSE_PLAY   : Play waveform backwards (negative freq).
 *  (1<<13) PX_WSEQ_BITCRUSH       : Enable Bitcrush effect.
 *  (1<<14) PX_WSEQ_XMOD           : Enable Cross-Mod (FM from prev Osc).
 *  (1<<15) PX_WSEQ_RING_MOD       : Enable Ring Mod (AM from prev Osc).
 *
 * =========================================================================================
 */

extern const PxWaveSequence ROM_WAVE_SEQUENCES[PX_NUM_WSEQ_BANKS];
#endif // PX_WSEQ_ROM_H

#ifdef PX_WSEQ_ROM_IMPLEMENTATION
#ifndef PX_WSEQ_ROM_IMP_INCLUDED
#define PX_WSEQ_ROM_IMP_INCLUDED
const PxWaveSequence ROM_WAVE_SEQUENCES[PX_NUM_WSEQ_BANKS] = {

    // --- Bank 0: Lead (0-7) ---
    // Leads are focused on melodic playback, often with glide or expressive articulation.
    // 0: Classic Saw Lead (Glide enabled, slight detune feeling via sequence)
    {
        .name = "Classic Saw Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .amp_mod_type = PX_WSEQ_AMP_RAMP_DOWN,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_AMP_MOD}, // Saw Rising (Pluck)
            {.wave_idx = 7, .duration_cycles = 100, .pitch_offset = 5, .flags = PX_WSEQ_GLIDE}, // Saw Falling (detune)
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 7, .duration_cycles = 100, .pitch_offset = -5, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 1: Pulse Width Modulation (Simulated by switching Pulse waves)
    {
        .name = "Pulse Width Modulation",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 32, .duration_cycles = 20, .pitch_offset = 0, .flags = 0}, // Pulse 25%
            {.wave_idx = 4,  .duration_cycles = 20, .pitch_offset = 0, .flags = 0}, // Square
            {.wave_idx = 33, .duration_cycles = 20, .pitch_offset = 0, .flags = 0}, // Pulse 75%
            {.wave_idx = 4,  .duration_cycles = 20, .pitch_offset = 0, .flags = 0}, // Square
            {.flags = PX_WSEQ_END}
        }
    },
    // 2: FM Solo (Expressive with XMod)
    {
        .name = "FM Solo",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .xmod_depth = 0.3f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 70, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_XMOD}, // FM Dynamic Lead
            {.wave_idx = 70, .duration_cycles = 50, .pitch_offset = 10, .flags = 0}, // Vibrato-ish
            {.wave_idx = 70, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_XMOD},
            {.wave_idx = 70, .duration_cycles = 50, .pitch_offset = -10, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 3: Sync Lead (Hard Sync effect)
    {
        .name = "Sync Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 116, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_LOCK_PHASE}, // Sync Sweep
            {.wave_idx = 117, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        },
        .lock_phase_mod_src = -1
    },
    // 4: Bitcrushed Lead
    {
        .name = "Bitcrushed Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Sine
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Saw
            {.flags = PX_WSEQ_END}
        }
    },
    // 5: Glitch Arp (Generative Octave Jumps with Bitcrush & Ring Mod)
    {
        .name = "Glitch Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 40,
        .ring_mod_depth = 0.3f,
        .ring_mod_mod_src = -1,
        .amp_mod_type = PX_WSEQ_AMP_RANDOM,
        .steps = {
            {.wave_idx = 4, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD | PX_WSEQ_AMP_MOD},
            {.wave_idx = 4, .duration_cycles = 150, .pitch_offset = 400, .flags = PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_AMP_MOD}, // Stuttering Morph
            {.wave_idx = 4, .duration_cycles = 150, .pitch_offset = 700, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 6: Phase Dist Lead
    {
        .name = "Phase Dist Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 56, .duration_cycles = 300, .pitch_offset = 0, .flags = 0}, // Phase Distortion
            {.wave_idx = 60, .duration_cycles = 300, .pitch_offset = 0, .flags = 0}, // PD Resonant
            {.flags = PX_WSEQ_END}
        }
    },
    // 7: Resonant Lead
    {
        .name = "Resonant Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 114, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, // Reso Filter Sweep
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 1: Pad (8-15) ---
    // Pads are slow-attack, evolving textures suitable for chords and atmosphere.
    // 8: PWM Pad (Slow evolution)
    {
        .name = "PWM Pad",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 32, .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 4,  .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 33, .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 9: Generative Ambient (Replaced Glassy Pad)
    {
        .name = "Generative Ambient",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 80,
        .rnd_wave_high = 100,
        .prob_mute_score = 50,
        .steps = {
            {.wave_idx = 0, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_PROB_MUTE}, // Evolving texture
            {.wave_idx = 0, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_REVERSE_PLAY},
            {.flags = PX_WSEQ_END}
        }
    },
    // 10: Choir Pad
    {
        .name = "Choir Pad",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 121, .duration_cycles = 800, .pitch_offset = 0, .flags = 0}, // Oooh Choir
            {.wave_idx = 113, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Vocal Ah
            {.flags = PX_WSEQ_END}
        }
    },
    // 11: Ring Mod Morph (Updated)
    {
        .name = "Ring Mod Morph",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_skip_score = 30,
        .steps = {
            {.wave_idx = 107, .duration_cycles = 600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 108, .duration_cycles = 600, .pitch_offset = 5, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP}, // Chance to skip detune
            {.wave_idx = 109, .duration_cycles = 600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD}, // Added Ring Mod + Glide
            {.flags = PX_WSEQ_END}
        },
        .ring_mod_depth = 0.2f,
        .ring_mod_mod_src = -1
    },
    // 12: Self-Xmod Drone
    {
        .name = "Self-Xmod Drone",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 81, .duration_cycles = 2000, .pitch_offset = -1200, .flags = PX_WSEQ_XMOD}, // FM Hollow Drone with Self-FM
            {.flags = PX_WSEQ_END}
        },
        .xmod_depth = 0.5f,
        .xmod_mod_src = -1
    },
    // 13: Space Pad
    {
        .name = "Space Pad",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 79, .duration_cycles = 1500, .pitch_offset = 0, .flags = 0}, // FM Sci-Fi Drone
            {.flags = PX_WSEQ_END}
        }
    },
    // 14: Shimmer Pad
    {
        .name = "Shimmer Pad",
        .end_action = PX_WSEQ_END_PINGPONG,
        .steps = {
            {.wave_idx = 102, .duration_cycles = 400, .pitch_offset = 0, .flags = 0}, // Classic Pad
            {.wave_idx = 102, .duration_cycles = 400, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE}, // +1 Octave shimmer
            {.flags = PX_WSEQ_END}
        }
    },
    // 15: Vintage Strings
    {
        .name = "Vintage Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 8, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Saw/Sine
            {.wave_idx = 9, .duration_cycles = 300, .pitch_offset = 5, .flags = PX_WSEQ_GLIDE}, // Beating Detune (+5 cents)
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 2: Strings (16-23) ---
    // Orchestral and synthesized string emulations.
    // 16: Bowed String
    {
        .name = "Bowed String",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 99, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, // Bowed String
            {.flags = PX_WSEQ_END}
        }
    },
    // 17: String Ensemble
    {
        .name = "String Ensemble",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, // Rich String Ensemble
            {.flags = PX_WSEQ_END}
        }
    },
    // 18: Tremolo Strings
    {
        .name = "Tremolo Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 1500, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD}, // Use Ring Mod as Tremolo
            {.flags = PX_WSEQ_END}
        },
        .ring_mod_depth = 0.3f,
        .ring_mod_mod_src = -1
    },
    // 19: Pizzicato
    {
        .name = "Pizzicato",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, // Plucked String
            {.flags = PX_WSEQ_END}
        }
    },
    // 20: Slow Strings
    {
        .name = "Slow Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 147, .duration_cycles = 500, .pitch_offset = 5, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 147, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 147, .duration_cycles = 500, .pitch_offset = -5, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 21: Octave Strings
    {
        .name = "Octave Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 300, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 147, .duration_cycles = 300, .pitch_offset = 1200, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 22: Crushed Bow (Updated from Crushed Strings)
    {
        .name = "Crushed Bow",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 40,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 200, .pitch_offset = -5, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP}, // Skip creates stutter
            {.wave_idx = 147, .duration_cycles = 200, .pitch_offset = 5, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 23: Reverse Swell Strings (Replaced Evolving Strings)
    {
        .name = "Reverse Swell Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE},
            {.wave_idx = 147, .duration_cycles = 800, .pitch_offset = 1200, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 3: Choir (24-31) ---
    // Vocal-like formants and choir textures.
    // 24: Ooh Choir
    {
        .name = "Ooh Choir",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 121, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 25: Aah Choir
    {
        .name = "Aah Choir",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 113, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 26: Realistic Voice (Vowels+Breath)
    {
        .name = "Realistic Voice",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 113, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Ah
            {.wave_idx = 115, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Ee
            {.wave_idx = 112, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Ih
            {.wave_idx = 121, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Oh
            {.wave_idx = 121, .duration_cycles = 250, .pitch_offset = -5, .flags = PX_WSEQ_GLIDE}, // Oo (Detuned)
            {.wave_idx = 113, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Ah Return
            {.wave_idx = 245, .duration_cycles = 80,  .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE}, // Breath Release (Noise)
            {.flags = PX_WSEQ_END}
        }
    },
    // 27: Robot Voice
    {
        .name = "Robot Voice",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 112, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Formantish
            {.flags = PX_WSEQ_END}
        }
    },
    // 28: Alien Choir
    {
        .name = "Alien Choir",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 218, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Alien Voice
    },
    // 29: Whispers
    {
        .name = "Whispers",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 113, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 141, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, // Filtered Static
            {.flags = PX_WSEQ_END}
        }
    },
    // 30: Angelic
    {
        .name = "Angelic",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 121, .duration_cycles = 200, .pitch_offset = 1200, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 31: Monks
    {
        .name = "Monks",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 113, .duration_cycles = 200, .pitch_offset = -1200, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 4: Ensemble (32-39) ---
    // Brass, Wind, and full orchestra hits.
    // 32: Brass Section
    {
        .name = "Brass Section",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 148, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 33: Synth Brass
    {
        .name = "Synth Brass",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 98, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 34: Orchestra Hit
    {
        .name = "Orchestra Hit",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 148, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 148, .duration_cycles = 20, .pitch_offset = 1200, .flags = 0}, // Octave stab
            {.wave_idx = 148, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 35: Wind Section
    {
        .name = "Wind Section",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 125, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Breathy Flute
    },
    // 36: Fanfare
    {
        .name = "Fanfare",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 98, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 98, .duration_cycles = 100, .pitch_offset = 400, .flags = 0}, // Major 3rd
            {.wave_idx = 98, .duration_cycles = 100, .pitch_offset = 700, .flags = 0}, // 5th
            {.wave_idx = 98, .duration_cycles = 400, .pitch_offset = 1200, .flags = 0}, // Octave
            {.flags = PX_WSEQ_END}
        }
    },
    // 37: Big Band
    {
        .name = "Big Band",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 148, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = -1200, .flags = 0}, // Low saw
            {.flags = PX_WSEQ_END}
        }
    },
    // 38: Epic Hit
    {
        .name = "Epic Hit",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 128, .duration_cycles = 10, .pitch_offset = -2400, .flags = 0}, // Kick
            {.wave_idx = 148, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 39: Detuned Saw Stack
    {
        .name = "Detuned Saw Stack",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 7, .duration_cycles = 50, .pitch_offset = 15, .flags = 0},
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = -15, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 5: Pluck (40-47) ---
    // Short, percussive tonal sounds mimicking plucked instruments.
    // 40: Nylon Guitar
    {
        .name = "Nylon Guitar",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = { {.wave_idx = 144, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} }
    },
    // 41: Harp Arp
    {
        .name = "Harp Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .amp_mod_type = PX_WSEQ_AMP_RAMP_DOWN,
        .steps = {
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 400, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 700, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 1200, .flags = PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 42: Koto
    {
        .name = "Koto",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 68, .duration_cycles = 10, .pitch_offset = 200, .flags = PX_WSEQ_AMP_MOD}, // FM Pluck bend
            {.wave_idx = 68, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 43: Banjo
    {
        .name = "Banjo",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = { {.wave_idx = 6, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} },
    },
    // 44: Muted Guitar
    {
        .name = "Muted Guitar",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = { {.wave_idx = 144, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} } // Short duration
    },
    // 45: Electric Pluck
    {
        .name = "Electric Pluck",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_RAMP_DOWN,
        .steps = { {.wave_idx = 6, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} },
    },
    // 46: Bass Pluck
    {
        .name = "Bass Pluck",
        .end_action = PX_WSEQ_END_STOP,
        .amp_mod_type = PX_WSEQ_AMP_RAMP_DOWN,
        .steps = { {.wave_idx = 144, .duration_cycles = 300, .pitch_offset = -1200, .flags = PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} }
    },
    // 47: Random Pluck
    {
        .name = "Random Pluck",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 50,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 144, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 6: Percussive (48-55) ---
    // Drums and hits.
    // 48: Kick
    {
        .name = "Kick",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 128, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 49: Snare
    {
        .name = "Snare",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 129, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 50: Math Hat (Replaced HiHat)
    {
        .name = "Math Hat",
        .end_action = PX_WSEQ_END_STOP,
        .prob_skip_score = 30,
        .steps = {
            {.wave_idx = 242, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP}, // Digital Saw
            {.wave_idx = 245, .duration_cycles = 30, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // White Noise
            {.flags = PX_WSEQ_END}
        }
    },
    // 51: Tom
    {
        .name = "Tom",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 131, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 52: Evolving Glitch Perc (Updated)
    {
        .name = "Evolving Glitch Perc",
        .end_action = PX_WSEQ_END_STOP,
        .prob_skip_score = 40,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 143, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE}, // Max flags
            {.flags = PX_WSEQ_END}
        },
        .adsr_retrig_phase = 1 // Retrigger Attack
    },
    // 53: Industrial Hit
    {
        .name = "Industrial Hit",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 136, .duration_cycles = 150, .pitch_offset = -1200, .flags = 0}, {.flags = PX_WSEQ_END} } // Metallic Perc low
    },
    // 54: Zap
    {
        .name = "Zap",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 211, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Laser Zap
    },
    // 55: Clave
    {
        .name = "Clave",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 142, .duration_cycles = 50, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Wooden Perc
    },

    // --- Bank 7: Oldskool (56-63) ---
    // Retro game and chiptune sounds.
    // 56: Basic Arp
    {
        .name = "Basic Arp",
        .end_action = PX_WSEQ_END_PINGPONG,
        .steps = {
            {.wave_idx = 4, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 4, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 4, .duration_cycles = 100, .pitch_offset = 2400, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 57: 8-bit Run
    {
        .name = "8-bit Run",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 5, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 5, .duration_cycles = 50, .pitch_offset = 200, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 5, .duration_cycles = 50, .pitch_offset = 400, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 5, .duration_cycles = 50, .pitch_offset = 500, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 5, .duration_cycles = 50, .pitch_offset = 700, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 58: C64 Arp
    {
        .name = "C64 Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 4, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Very fast
            {.wave_idx = 4, .duration_cycles = 20, .pitch_offset = 300, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 4, .duration_cycles = 20, .pitch_offset = 700, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 59: Mario Jump
    {
        .name = "Mario Jump",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 4, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH},
            {.wave_idx = 4, .duration_cycles = 200, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 60: Coin
    {
        .name = "Coin",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 32, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 32, .duration_cycles = 200, .pitch_offset = 500, .flags = PX_WSEQ_BITCRUSH}, // 4th/5th up
            {.flags = PX_WSEQ_END}
        }
    },
    // 61: Power Up
    {
        .name = "Power Up",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 400, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 700, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 1600, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 62: Game Over
    {
        .name = "Game Over",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 6, .duration_cycles = 300, .pitch_offset = -100, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 6, .duration_cycles = 400, .pitch_offset = -200, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 63: Chiptune Lead
    {
        .name = "Chiptune Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 32, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Pulse 25
            {.wave_idx = 34, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Staircase
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 8: Arcade (64-71) ---
    // Classic arcade SFX and aggressive digital sounds.
    // 64: Pac-Man
    {
        .name = "Pac-Man",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 176, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 65: Invader
    {
        .name = "Invader",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 181, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 66: Explosion
    {
        .name = "Explosion",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 183, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 67: Laser
    {
        .name = "Laser",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 190, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 68: Jump
    {
        .name = "Jump",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 197, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 69: Collect
    {
        .name = "Collect",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 215, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 70: Enemy
    {
        .name = "Enemy",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 218, .duration_cycles = 300, .pitch_offset = -1200, .flags = PX_WSEQ_XMOD}, // Added growl
            {.flags = PX_WSEQ_END}
        },
        .xmod_depth = 0.3f,
        .xmod_mod_src = -1
    },
    // 71: Level Up
    {
        .name = "Level Up",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 219, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },

    // --- Bank 9: Fun (72-79) ---
    // Novelty and cartoon effects.
    // 72: Bubble
    {
        .name = "Bubble",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 73: Squeak
    {
        .name = "Squeak",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 2, .duration_cycles = 50, .pitch_offset = 2400, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 74: Wobble
    {
        .name = "Wobble",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = 200, .flags = 0},
            {.wave_idx = 2, .duration_cycles = 100, .pitch_offset = -200, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 75: Boing
    {
        .name = "Boing",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 207, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, // Moon Patrol Bounce
            {.flags = PX_WSEQ_END}
        }
    },
    // 76: Slide Whistle
    {
        .name = "Slide Whistle",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 2, .duration_cycles = 500, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 77: Cartoon Fall
    {
        .name = "Cartoon Fall",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 500, .pitch_offset = 2400, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 2, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 78: Toy Piano
    {
        .name = "Toy Piano",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 217, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 79: Kazoo
    {
        .name = "Kazoo",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 10: Natural (80-87) ---
    // Environmental and organic simulations.
    // 80: Wind
    {
        .name = "Wind",
        .end_action = PX_WSEQ_END_LOOP,
        .amp_mod_type = PX_WSEQ_AMP_SINE,
        .steps = { {.wave_idx = 159, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD}, {.flags = PX_WSEQ_END} } // Wind AM
    },
    // 81: Rain
    {
        .name = "Rain",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 156, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP}, {.flags = PX_WSEQ_END} }, // Water Droplet
        .prob_skip_score = 50
    },
    // 82: Thunder
    {
        .name = "Thunder",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 140, .duration_cycles = 500, .pitch_offset = -2400, .flags = 0}, {.flags = PX_WSEQ_END} } // Rumble Noise
    },
    // 83: Bird
    {
        .name = "Bird",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 55, .duration_cycles = 200, .pitch_offset = 2400, .flags = 0}, {.flags = PX_WSEQ_END} } // Bird Call AM
    },
    // 84: Insect
    {
        .name = "Insect",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 3600, .flags = 0}, {.flags = PX_WSEQ_END} } // High saw
    },
    // 85: Water
    {
        .name = "Water",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 156, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 86: Fire
    {
        .name = "Fire",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 143, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE}, {.flags = PX_WSEQ_END} },
        .rnd_octave_range = 80
    },
    // 87: Ocean
    {
        .name = "Ocean",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 245, .duration_cycles = 2000, .pitch_offset = -1200, .flags = PX_WSEQ_GLIDE}, // White noise
            {.wave_idx = 245, .duration_cycles = 2000, .pitch_offset = -2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 11: Enhanced (88-95) ---
    // Modern complex patches using advanced features.
    // 88: Super Saw
    {
        .name = "Super Saw",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 89: Hyper Square
    {
        .name = "Hyper Square",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 4, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 90: Trance Gate
    {
        .name = "Trance Gate",
        .end_action = PX_WSEQ_END_LOOP,
        .amp_mod_type = PX_WSEQ_AMP_SQUARE,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD}, // Gate effect (50% PWM)
            {.flags = PX_WSEQ_END}
        }
    },
    // 91: Complex Arp
    {
        .name = "Complex Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = 1200, .flags = 0},
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = 700, .flags = 0},
            {.wave_idx = 6, .duration_cycles = 50, .pitch_offset = 1900, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 92: Hyper Stutter (Replaced Morphing Lead)
    {
        .name = "Hyper Stutter",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_skip_score = 25,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH},
            {.wave_idx = 4, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.flags = PX_WSEQ_END}
        }
    },
    // 93: Generative Glitch (Replaced Stutter)
    {
        .name = "Generative Glitch",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_mute_score = 30,
        .rnd_wave_low = 0,
        .rnd_wave_high = 64,
        .rnd_octave_range = 60,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_RND_OCTAVE}, // Random wave & octave
            {.wave_idx = 6, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_RETRIG_ADSR}, // Mute & Retrigger
            {.flags = PX_WSEQ_END}
        },
        .adsr_retrig_phase = 1
    },
    // 94: Glitch Hop
    {
        .name = "Glitch Hop",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 143, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RING_MOD}, // Added Ring Mod
            {.wave_idx = 143, .duration_cycles = 50, .pitch_offset = 1200, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        },
        .ring_mod_depth = 0.2f,
        .ring_mod_mod_src = -1
    },
    // 95: Total Chaos Theory
    {
        .name = "Total Chaos Theory",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 248, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_RING_MOD}, // Logistic Chaos + Flag Overflow
            {.flags = PX_WSEQ_END}
        },
        .rnd_octave_range = 75,
        .rnd_wave_low = 0,
        .rnd_wave_high = 255,
        .ring_mod_depth = 0.4f,
        .ring_mod_mod_src = -1
    },

    // --- Bank 12: Deep (96-103) ---
    // Sub-bass and heavy low-end textures.
    // 96: Sub Bass
    {
        .name = "Sub Bass",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 74, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // FM Deep Sub
    },
    // 97: Dub Chord
    {
        .name = "Dub Chord",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 106, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Minor Triad
    },
    // 98: FM Math Drone (Replaced Chaos Drone)
    {
        .name = "FM Math Drone",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.6f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 1000, .pitch_offset = -1200, .flags = PX_WSEQ_XMOD | PX_WSEQ_LOCK_PHASE}, // Laser Malfunction with XMod & Phase Lock
            {.flags = PX_WSEQ_END}
        },
        .lock_phase_mod_src = -1 // Always lock
    },
    // 99: 808 Kick
    {
        .name = "808 Kick",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 128, .duration_cycles = 300, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 100: Dark Ambient
    {
        .name = "Dark Ambient",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 153, .duration_cycles = 800, .pitch_offset = -1200, .flags = 0}, {.flags = PX_WSEQ_END} } // Chaotic Osc
    },
    // 101: Underwater
    {
        .name = "Underwater",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 156, .duration_cycles = 500, .pitch_offset = -1200, .flags = 0}, {.flags = PX_WSEQ_END} } // Water Droplet
    },
    // 102: Heartbeat
    {
        .name = "Heartbeat",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 128, .duration_cycles = 50, .pitch_offset = -2400, .flags = 0},
            {.wave_idx = 128, .duration_cycles = 50, .pitch_offset = -2400, .flags = PX_WSEQ_USE_PROB_MUTE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 103: Rumble
    {
        .name = "Rumble",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 140, .duration_cycles = 500, .pitch_offset = -2400, .flags = 0}, {.flags = PX_WSEQ_END} }
    },

    // --- Bank 13: Futuristic (104-111) ---
    // Sci-fi, cyber, and technological sounds.
    // 104: Robot Talk
    {
        .name = "Robot Talk",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 157, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE}, // Alien Chatter
            {.flags = PX_WSEQ_END}
        },
        .rnd_octave_range = 30
    },
    // 105: Data Stream
    {
        .name = "Data Stream",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 160, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE}, // LFSR Rhythm Gate
            {.flags = PX_WSEQ_END}
        },
        .rnd_octave_range = 80
    },
    // 106: Cyberpunk Bass
    {
        .name = "Cyberpunk Bass",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 210, .duration_cycles = 100, .pitch_offset = -1200, .flags = 0}, {.flags = PX_WSEQ_END} } // POKEY Distorted
    },
    // 107: Math Laser (Replaced Laser Harp)
    {
        .name = "Math Laser",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.4f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_XMOD | PX_WSEQ_GLIDE}, // Laser Malfunction
            {.wave_idx = 50, .duration_cycles = 200, .pitch_offset = 1200, .flags = PX_WSEQ_XMOD | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 108: Teleport
    {
        .name = "Teleport",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 48, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // Warp Speed
            {.wave_idx = 48, .duration_cycles = 300, .pitch_offset = 2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 109: Scanner
    {
        .name = "Scanner",
        .end_action = PX_WSEQ_END_PINGPONG,
        .steps = {
            {.wave_idx = 221, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, // Sweep Down
            {.wave_idx = 221, .duration_cycles = 100, .pitch_offset = 500, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 110: Matrix
    {
        .name = "Matrix",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 175, .duration_cycles = 50, .pitch_offset = 0, .flags = 0}, // LFSR Glitch Matrix
            {.flags = PX_WSEQ_END}
        }
    },
    // 111: Warp Drive
    {
        .name = "Warp Drive",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 48, .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 48, .duration_cycles = 1000, .pitch_offset = 3600, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 14: Emulation (112-119) ---
    // Approximations of acoustic instruments using simple waveforms.
    // 112: Organ
    {
        .name = "Organ",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 110, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 113: Flute
    {
        .name = "Flute",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 125, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 114: Clarinet
    {
        .name = "Clarinet",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 4, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Square
    },
    // 115: Trumpet
    {
        .name = "Trumpet",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 98, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 116: Violin
    {
        .name = "Violin",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 99, .duration_cycles = 100, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 117: Cello
    {
        .name = "Cello",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = { {.wave_idx = 99, .duration_cycles = 100, .pitch_offset = -1200, .flags = 0}, {.flags = PX_WSEQ_END} }
    },
    // 118: Bell
    {
        .name = "Bell",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 88, .duration_cycles = 400, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} } // Metallic Bell
    },
    // 119: Steel Drum
    {
        .name = "Steel Drum",
        .end_action = PX_WSEQ_END_STOP,
        .steps = { {.wave_idx = 136, .duration_cycles = 200, .pitch_offset = 0, .flags = 0}, {.flags = PX_WSEQ_END} }
    },

    // --- Bank 15: Strange (120-127) ---
    // Generative, chaotic, and experimental "Ham Crazy" sequences.
    // 120: Fractal Spiral (Replaced Fibonacci Spiral)
    {
        .name = "Fractal Spiral",
        .end_action = PX_WSEQ_END_LOOP,
        .ring_mod_depth = 0.3f,
        .ring_mod_mod_src = -1,
        .xmod_depth = 0.3f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 247, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD | PX_WSEQ_XMOD}, // Fibonacci with Ring+XMod
            {.wave_idx = 247, .duration_cycles = 200, .pitch_offset = 700, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD | PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 121: Chaos Theory (Replaced Logistic Glitch)
    {
        .name = "Chaos Theory",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 50,
        .prob_skip_score = 20,
        .steps = {
            {.wave_idx = 248, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_REVERSE_PLAY}, // Chaotic!
            {.flags = PX_WSEQ_END}
        }
    },
    // 122: Digital Hurricane (Replaced Bitcrush Storm)
    {
        .name = "Digital Hurricane",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 25,
        .prob_mute_score = 10,
        .steps = {
            {.wave_idx = 245, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 123: Neural Net (Replaced Self-Mod Glitch)
    {
        .name = "Neural Net",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.7f,
        .xmod_mod_src = -1,
        .ring_mod_depth = 0.3f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 95, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_XMOD | PX_WSEQ_RING_MOD | PX_WSEQ_GLIDE}, // Alien Comm
            {.wave_idx = 95, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_XMOD | PX_WSEQ_RING_MOD | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 124: Quantum Leaps (Replaced Stutter Morph)
    {
        .name = "Quantum Leaps",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 50,
        .rnd_wave_high = 60,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 40, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_RETRIG_ADSR}, // Random Phase Dist waves
            {.wave_idx = 50, .duration_cycles = 40, .pitch_offset = 500, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        },
        .adsr_retrig_phase = 1
    },
    // 125: Singularity (Replaced Total Chaos)
    {
        .name = "Singularity",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 0,
        .rnd_wave_high = 255,
        .rnd_octave_range = 90,
        .ring_mod_depth = 0.5f,
        .ring_mod_mod_src = -1,
        .xmod_depth = 0.5f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 0, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_RING_MOD | PX_WSEQ_XMOD | PX_WSEQ_BITCRUSH}, // EVERYTHING
            {.flags = PX_WSEQ_END}
        }
    },
    // 126: Time Travel (Replaced Reverse Tape)
    {
        .name = "Time Travel",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_skip_score = 30,
        .ring_mod_depth = 0.25f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_RING_MOD | PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 2, .duration_cycles = 200, .pitch_offset = -500, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_RING_MOD | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 127: Heat Death (Replaced The End)
    {
        .name = "Heat Death",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 2, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // Degrading
            {.wave_idx = 2, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // ...
            {.wave_idx = 0, .duration_cycles = 0, .pitch_offset = 0, .flags = PX_WSEQ_END} // Silence
        }
    },

    // --- Bank 16: Wavestation (128-135) ---
    // Evolving vector synthesis textures and rhythmic loops.
    // 128: Vector Pad
    {
        .name = "Vector Pad",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .xmod_depth = 0.5f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 102, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD}, // Pad
            {.wave_idx = 8,   .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD}, // Saw/Sine
            {.wave_idx = 115, .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD}, // Vowel
            {.wave_idx = 9,   .duration_cycles = 800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD}, // Sine/Saw
            {.flags = PX_WSEQ_END}
        }
    },
    // 129: Ski Jam
    {
        .name = "Ski Jam",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.3f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 128, .duration_cycles = 50, .pitch_offset = 0, .flags = 0}, // Kick
            {.wave_idx = 132, .duration_cycles = 25, .pitch_offset = 0, .flags = 0}, // Cymbal
            {.wave_idx = 129, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_XMOD}, // Snare (with XMod)
            {.wave_idx = 132, .duration_cycles = 25, .pitch_offset = 0, .flags = 0}, // Cymbal
            {.flags = PX_WSEQ_END}
        }
    },
    // 130: Wave Sweep
    {
        .name = "Wave Sweep",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .xmod_depth = 0.4f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 0,  .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 4,  .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 8,  .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 12, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 131: Ethereal
    {
        .name = "Ethereal",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .ring_mod_depth = 0.2f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 121, .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD}, // Choir
            {.wave_idx = 152, .duration_cycles = 1000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD}, // Breathing
            {.flags = PX_WSEQ_END}
        }
    },
    // 132: Motion Texture
    {
        .name = "Motion Texture",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.5f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 151, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_XMOD},
            {.wave_idx = 151, .duration_cycles = 500, .pitch_offset = 10, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 133: Resonant Sweep
    {
        .name = "Resonant Sweep",
        .end_action = PX_WSEQ_END_PINGPONG,
        .xmod_depth = 0.3f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 114, .duration_cycles = 600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 114, .duration_cycles = 600, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 134: Glass Morph
    {
        .name = "Glass Morph",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .ring_mod_depth = 0.4f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 71, .duration_cycles = 400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD},
            {.wave_idx = 88, .duration_cycles = 400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RING_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 135: Vector Pluck
    {
        .name = "Vector Pluck",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .xmod_depth = 0.6f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 144, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 102, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 17: VS Trickery (136-143) ---
    // Fast scanning and digital artifacts.
    // 136: Prophet Scan
    {
        .name = "Prophet Scan",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 32, .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 33, .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 34, .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 35, .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 137: Digital Choir
    {
        .name = "Digital Choir",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 113, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 115, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 121, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 138: Bell Sweep
    {
        .name = "Bell Sweep",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 88, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 88, .duration_cycles = 300, .pitch_offset = 700, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 139: Horror Scan
    {
        .name = "Horror Scan",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 49, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 49, .duration_cycles = 200, .pitch_offset = -600, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 140: PPG Sweep
    {
        .name = "PPG Sweep",
        .end_action = PX_WSEQ_END_PINGPONG,
        .steps = {
            {.wave_idx = 16, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 18, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 20, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 22, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 141: Hard Sync Sweep
    {
        .name = "Hard Sync Sweep",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 116, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 116, .duration_cycles = 500, .pitch_offset = 2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 142: Metal Morph
    {
        .name = "Metal Morph",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 136, .duration_cycles = 400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 137, .duration_cycles = 400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 143: VS Strings
    {
        .name = "VS Strings",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 147, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 147, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 18: Glitch IDM (144-151) ---
    // Fast, chaotic, and generative sounds.
    // 144: AFX Acid
    {
        .name = "AFX Acid",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 60,
        .steps = {
            {.wave_idx = 29, .duration_cycles = 30, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 29, .duration_cycles = 30, .pitch_offset = 1200, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 145: Drill n Bass
    {
        .name = "Drill n Bass",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 160,
        .rnd_wave_high = 175,
        .steps = {
            {.wave_idx = 160, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 146: Glitch Pad
    {
        .name = "Glitch Pad",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 40,
        .steps = {
            {.wave_idx = 146, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 146, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 147: Brain Dance
    {
        .name = "Brain Dance",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 80,
        .steps = {
            {.wave_idx = 153, .duration_cycles = 40, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 148: Stutter Lead
    {
        .name = "Stutter Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_mute_score = 30,
        .steps = {
            {.wave_idx = 52, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 52, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 149: Granular Cloud
    {
        .name = "Granular Cloud",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 144,
        .rnd_wave_high = 159,
        .steps = {
            {.wave_idx = 165, .duration_cycles = 5, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 150: Chaos Arp
    {
        .name = "Chaos Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 0,
        .rnd_wave_high = 64,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 150, .duration_cycles = 15, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_RND_OCTAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 151: Bitrot
    {
        .name = "Bitrot",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 249, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 249, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 19: Trap (152-159) ---
    // Deep bass, sharp hats, and plucks.
    // 152: 808 Sub Glides
    {
        .name = "808 Sub Glides",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .steps = {
            {.wave_idx = 96, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 96, .duration_cycles = 300, .pitch_offset = -2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 153: HiHat Rolls
    {
        .name = "HiHat Rolls",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 8, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 0,  .duration_cycles = 8, .pitch_offset = 0, .flags = 0}, // Gap
            {.flags = PX_WSEQ_END}
        }
    },
    // 154: Trap Pluck
    {
        .name = "Trap Pluck",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 68, .duration_cycles = 5, .pitch_offset = 1200, .flags = 0}, // Transient
            {.wave_idx = 68, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 155: Dark Bell
    {
        .name = "Dark Bell",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 135, .duration_cycles = 50, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 135, .duration_cycles = 50, .pitch_offset = 300, .flags = 0}, // Minor 3rd
            {.wave_idx = 135, .duration_cycles = 50, .pitch_offset = 700, .flags = 0}, // 5th
            {.flags = PX_WSEQ_END}
        }
    },
    // 156: Snare Rush
    {
        .name = "Snare Rush",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 49, .duration_cycles = 20, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 49, .duration_cycles = 15, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 49, .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 49, .duration_cycles = 5, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 157: Tape Stop FX
    {
        .name = "Tape Stop FX",
        .end_action = PX_WSEQ_END_STOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 6, .duration_cycles = 300, .pitch_offset = -2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 158: Vocal Chop
    {
        .name = "Vocal Chop",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 115, .duration_cycles = 30, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 0,   .duration_cycles = 10, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 115, .duration_cycles = 30, .pitch_offset = 500, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 159: Dirty Brass
    {
        .name = "Dirty Brass",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 33, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 20: Industrial (160-167) ---
    // Distorted, metallic, and mechanical.
    // 160: Reznor Bass
    {
        .name = "Reznor Bass",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 65, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 161: Rusty Metal
    {
        .name = "Rusty Metal",
        .end_action = PX_WSEQ_END_LOOP,
        .ring_mod_depth = 0.5f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 136, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 162: Machine Beat
    {
        .name = "Machine Beat",
        .end_action = PX_WSEQ_END_LOOP,
        .amp_mod_type = PX_WSEQ_AMP_SQUARE,
        .steps = {
            {.wave_idx = 138, .duration_cycles = 50, .pitch_offset = -1200, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 0,   .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 163: Distortion Lead
    {
        .name = "Distortion Lead",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 16, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 164: Broken Circuit
    {
        .name = "Broken Circuit",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 30,
        .steps = {
            {.wave_idx = 210, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP},
            {.flags = PX_WSEQ_END}
        }
    },
    // 165: Power Noise
    {
        .name = "Power Noise",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 140, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 166: Alarm
    {
        .name = "Alarm",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 206, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 206, .duration_cycles = 100, .pitch_offset = 600, .flags = 0}, // Tritone
            {.flags = PX_WSEQ_END}
        }
    },
    // 167: Grinder
    {
        .name = "Grinder",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 139, .duration_cycles = 500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 139, .duration_cycles = 500, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 21: Abyss (168-175) ---
    // Dark ambient and space drones.
    // 168: Deep Drone
    {
        .name = "Deep Drone",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 79, .duration_cycles = 2000, .pitch_offset = -2400, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 169: Void Calls
    {
        .name = "Void Calls",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 49, .duration_cycles = 800, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 0,  .duration_cycles = 1600, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 170: Submarine
    {
        .name = "Submarine",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 156, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.wave_idx = 0,   .duration_cycles = 2000, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 171: Dark Matter
    {
        .name = "Dark Matter",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 100, .duration_cycles = 1000, .pitch_offset = -1200, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 172: Event Horizon
    {
        .name = "Event Horizon",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 48, .duration_cycles = 2000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 48, .duration_cycles = 2000, .pitch_offset = -3600, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 173: Cthulhu
    {
        .name = "Cthulhu",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 157, .duration_cycles = 500, .pitch_offset = -2400, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        },
    },
    // 174: Space Wind
    {
        .name = "Space Wind",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 159, .duration_cycles = 1500, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 175: Black Hole
    {
        .name = "Black Hole",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 248, .duration_cycles = 1000, .pitch_offset = -2400, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 22: Experimental A (176-183) ---
    // Math and complexity.
    // 176: Math Noise
    {
        .name = "Math Noise",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 24, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 177: Fibonacci Spiral
    {
        .name = "Fibonacci Spiral",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 247, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 178: Prime Arp
    {
        .name = "Prime Arp",
        .end_action = PX_WSEQ_END_LOOP,
        .amp_mod_type = PX_WSEQ_AMP_PULSE_25,
        .steps = {
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 200, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 300, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 500, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 700, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 6, .duration_cycles = 10, .pitch_offset = 1100, .flags = PX_WSEQ_AMP_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 179: Chaos Theory II
    {
        .name = "Chaos Theory II",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 21, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 180: Cellular Automata
    {
        .name = "Cellular Automata",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 160, .duration_cycles = 50, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 181: Random Walk
    {
        .name = "Random Walk",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 245, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 182: Fractal Noise
    {
        .name = "Fractal Noise",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 124, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 183: Quantum Foam
    {
        .name = "Quantum Foam",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 50,
        .steps = {
            {.wave_idx = 150, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 23: Experimental B (184-191) ---
    // Pure glitch and mayhem.
    // 184: Data Mosh
    {
        .name = "Data Mosh",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 35, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 185: Spectrum Cycle
    {
        .name = "Spectrum Cycle",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 167, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 186: Phase Smasher
    {
        .name = "Phase Smasher",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 56, .duration_cycles = 200, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 187: FM Matrix
    {
        .name = "FM Matrix",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.8f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 67, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 188: Ring Mod Hell
    {
        .name = "Ring Mod Hell",
        .end_action = PX_WSEQ_END_LOOP,
        .ring_mod_depth = 0.9f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 188, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 189: Bitwise Logic
    {
        .name = "Bitwise Logic",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 45, .duration_cycles = 100, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 190: The Glitch
    {
        .name = "The Glitch",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 0,
        .rnd_wave_high = 255,
        .prob_skip_score = 20,
        .steps = {
            {.wave_idx = 53, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_PROB_SKIP},
            {.flags = PX_WSEQ_END}
        }
    },
    // 191: System Crash
    {
        .name = "System Crash",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 238, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 238, .duration_cycles = 100, .pitch_offset = -1200, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 238, .duration_cycles = 200, .pitch_offset = -2400, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 24: Metallic Textures (192-199) ---
    // Metallic textures use ring mod, bitcrush, and random octave for clangorous, inharmonic sounds.
    // 192: Metallic 1
    {
        .name = "Metallic 1",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 42, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH},
            {.wave_idx = 43, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 44, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 45, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 46, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 47, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 48, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 49, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 193: Metallic 2
    {
        .name = "Metallic 2",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 44, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH},
            {.wave_idx = 45, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 46, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 47, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 48, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 49, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 50, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 51, .duration_cycles = 170, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 194: Metallic 3
    {
        .name = "Metallic 3",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 46, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH},
            {.wave_idx = 47, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 48, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 49, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 50, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 51, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 52, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 53, .duration_cycles = 190, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 195: Metallic 4
    {
        .name = "Metallic 4",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 48, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH},
            {.wave_idx = 49, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 50, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 51, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 52, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 53, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 54, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 55, .duration_cycles = 210, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 196: Metallic 5
    {
        .name = "Metallic 5",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 51, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD},
            {.wave_idx = 52, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 53, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD},
            {.wave_idx = 54, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD},
            {.wave_idx = 55, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD},
            {.wave_idx = 56, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 230, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD}
        }
    },
    // 197: Metallic 6
    {
        .name = "Metallic 6",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 52, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 53, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD},
            {.wave_idx = 54, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 55, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD},
            {.wave_idx = 56, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 250, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD}
        }
    },
    // 198: Metallic 7
    {
        .name = "Metallic 7",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 54, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 55, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD},
            {.wave_idx = 56, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD},
            {.wave_idx = 60, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD},
            {.wave_idx = 61, .duration_cycles = 270, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD}
        }
    },
    // 199: Metallic 8
    {
        .name = "Metallic 8",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 10,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 56, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD},
            {.wave_idx = 60, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD},
            {.wave_idx = 61, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD},
            {.wave_idx = 62, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD},
            {.wave_idx = 63, .duration_cycles = 290, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD}
        }
    },

    // --- Bank 25: Percussive Hits (200-207) ---
    // Percussive hits use short durations, retrigger, and random skip for drum-like patterns.
    // 200: Percussive 1
    {
        .name = "Percussive 1",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_AMP_MOD},
            {.wave_idx = 34, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_AMP_MOD}
        }
    },
    // 201: Percussive 2
    {
        .name = "Percussive 2",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_AMP_MOD},
            {.wave_idx = 35, .duration_cycles = 47, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_AMP_MOD}
        }
    },
    // 202: Percussive 3
    {
        .name = "Percussive 3",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_AMP_MOD},
            {.wave_idx = 36, .duration_cycles = 44, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_AMP_MOD}
        }
    },
    // 203: Percussive 4
    {
        .name = "Percussive 4",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_AMP_MOD},
            {.wave_idx = 37, .duration_cycles = 41, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_AMP_MOD}
        }
    },
    // 204: Percussive 5
    {
        .name = "Percussive 5",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_AMP_MOD},
            {.wave_idx = 38, .duration_cycles = 38, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_AMP_MOD}
        }
    },
    // 205: Percussive 6
    {
        .name = "Percussive 6",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 39, .duration_cycles = 35, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD}
        }
    },
    // 206: Percussive 7
    {
        .name = "Percussive 7",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 40, .duration_cycles = 32, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD}
        }
    },
    // 207: Percussive 8
    {
        .name = "Percussive 8",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_skip_score = 25,
        .amp_mod_type = PX_WSEQ_AMP_EXP_DOWN,
        .steps = {
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD},
            {.wave_idx = 41, .duration_cycles = 29, .pitch_offset = 0, .flags = PX_WSEQ_END | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_AMP_MOD}
        }
    },

    // --- Bank 26: Evolving Pads (208-215) ---
    // Evolving pads use long durations, blending, and random octave for slowly changing textures.
    // 208: Pad 1
    {
        .name = "Pad 1",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 50, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 51, .duration_cycles = 5000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 52, .duration_cycles = 5000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 53, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 54, .duration_cycles = 5000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 55, .duration_cycles = 5000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 56, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 57, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 209: Pad 2
    {
        .name = "Pad 2",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 51, .duration_cycles = 5500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 52, .duration_cycles = 5500, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 53, .duration_cycles = 5500, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 54, .duration_cycles = 5500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 55, .duration_cycles = 5500, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 56, .duration_cycles = 5500, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 57, .duration_cycles = 5500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 58, .duration_cycles = 5500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 210: Pad 3
    {
        .name = "Pad 3",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 52, .duration_cycles = 6000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 53, .duration_cycles = 6000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 54, .duration_cycles = 6000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 55, .duration_cycles = 6000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 56, .duration_cycles = 6000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 57, .duration_cycles = 6000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 58, .duration_cycles = 6000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 59, .duration_cycles = 6000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 211: Pad 4
    {
        .name = "Pad 4",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 53, .duration_cycles = 6500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 54, .duration_cycles = 6500, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 55, .duration_cycles = 6500, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 56, .duration_cycles = 6500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 57, .duration_cycles = 6500, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 58, .duration_cycles = 6500, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 59, .duration_cycles = 6500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 60, .duration_cycles = 6500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 212: Pad 5
    {
        .name = "Pad 5",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 54, .duration_cycles = 7000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 55, .duration_cycles = 7000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 56, .duration_cycles = 7000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 57, .duration_cycles = 7000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 58, .duration_cycles = 7000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 59, .duration_cycles = 7000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 60, .duration_cycles = 7000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 61, .duration_cycles = 7000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 213: Pad 6
    {
        .name = "Pad 6",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 55, .duration_cycles = 7500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 56, .duration_cycles = 7500, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 7500, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 7500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 7500, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 60, .duration_cycles = 7500, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 61, .duration_cycles = 7500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 62, .duration_cycles = 7500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },
    // 214: Pad 7
    {
        .name = "Pad 7",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 56, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 57, .duration_cycles = 8000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 8000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 60, .duration_cycles = 8000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 61, .duration_cycles = 8000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 62, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 63, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },
    // 215: Pad 8
    {
        .name = "Pad 8",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 57, .duration_cycles = 8500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 58, .duration_cycles = 8500, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 59, .duration_cycles = 8500, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 60, .duration_cycles = 8500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 61, .duration_cycles = 8500, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 62, .duration_cycles = 8500, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 63, .duration_cycles = 8500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 64, .duration_cycles = 8500, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },

    // --- Bank 27: Glitch Effects (216-223) ---
    // Glitch effects use bitcrush, random skip, and reverse for broken, chaotic sounds.
    // 216: Glitch 1
    {
        .name = "Glitch 1",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 58, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 59, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 60, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 61, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 62, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 63, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 64, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 65, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 217: Glitch 2
    {
        .name = "Glitch 2",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 59, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 60, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 61, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 62, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 63, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 64, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 65, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 66, .duration_cycles = 60, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 218: Glitch 3
    {
        .name = "Glitch 3",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 60, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 61, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 62, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 63, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 64, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 65, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 66, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 67, .duration_cycles = 70, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 219: Glitch 4
    {
        .name = "Glitch 4",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 61, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 62, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 63, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE},
            {.wave_idx = 64, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 65, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 66, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 67, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR},
            {.wave_idx = 68, .duration_cycles = 80, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 220: Glitch 5
    {
        .name = "Glitch 5",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 62, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 63, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 64, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 65, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 66, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 67, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 68, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 69, .duration_cycles = 90, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY}
        }
    },
    // 221: Glitch 6
    {
        .name = "Glitch 6",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 63, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 64, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 65, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 66, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 67, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 68, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 69, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 70, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY}
        }
    },
    // 222: Glitch 7
    {
        .name = "Glitch 7",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 64, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 65, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 66, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 67, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 68, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 69, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 70, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 71, .duration_cycles = 110, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY}
        }
    },
    // 223: Glitch 8
    {
        .name = "Glitch 8",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .prob_mute_score = 20,
        .prob_skip_score = 50,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 65, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 66, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 67, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_GLIDE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 68, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 69, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 70, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 71, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RETRIG_ADSR | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 72, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_REVERSE_PLAY}
        }
    },

    // --- Bank 28: Noise Sculptures (224-231) ---
    // Noise sculptures use noise waves with bitcrush, random octave, and mute for abstract, noisy art.
    // 224: Noise 1
    {
        .name = "Noise 1",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 66, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 67, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 68, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 69, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 70, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 71, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 72, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 73, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 225: Noise 2
    {
        .name = "Noise 2",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 67, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 68, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 69, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 70, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 71, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 72, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 73, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 74, .duration_cycles = 120, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 226: Noise 3
    {
        .name = "Noise 3",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 68, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 69, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 70, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 71, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 72, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 73, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 74, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 75, .duration_cycles = 140, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 227: Noise 4
    {
        .name = "Noise 4",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 69, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 70, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 71, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 72, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO},
            {.wave_idx = 73, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 74, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 75, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 76, .duration_cycles = 160, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END}
        }
    },
    // 228: Noise 5
    {
        .name = "Noise 5",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 70, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 71, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 72, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 73, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 74, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 75, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 76, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 77, .duration_cycles = 180, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD}
        }
    },
    // 229: Noise 6
    {
        .name = "Noise 6",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 71, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 72, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 73, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 74, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 75, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 76, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 77, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 78, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD}
        }
    },
    // 230: Noise 7
    {
        .name = "Noise 7",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 72, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 73, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 74, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 75, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 76, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 77, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 78, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 79, .duration_cycles = 220, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD}
        }
    },
    // 231: Noise 8
    {
        .name = "Noise 8",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_OFF,
        .prob_mute_score = 30,
        .prob_skip_score = 40,
        .rnd_octave_range = 50,
        .steps = {
            {.wave_idx = 73, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 74, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 75, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 76, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_RESET_LFO | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 77, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 78, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 79, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD},
            {.wave_idx = 80, .duration_cycles = 240, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_END | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD}
        }
    },

    // --- Bank 29: Evolving Pads II (232-239) ---
    // Evolving pads use long durations, blending, and random octave for slowly changing textures.
    // 232: Pad II 1
    {
        .name = "Pad II 1",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 74, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 75, .duration_cycles = 5000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 76, .duration_cycles = 5000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 77, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 78, .duration_cycles = 5000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 79, .duration_cycles = 5000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 80, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 81, .duration_cycles = 5000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 233: Pad II 2
    {
        .name = "Pad II 2",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 75, .duration_cycles = 5600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 76, .duration_cycles = 5600, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 77, .duration_cycles = 5600, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 78, .duration_cycles = 5600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 79, .duration_cycles = 5600, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 80, .duration_cycles = 5600, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 81, .duration_cycles = 5600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 82, .duration_cycles = 5600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 234: Pad II 3
    {
        .name = "Pad II 3",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 76, .duration_cycles = 6200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 77, .duration_cycles = 6200, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 78, .duration_cycles = 6200, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 79, .duration_cycles = 6200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 80, .duration_cycles = 6200, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 81, .duration_cycles = 6200, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 82, .duration_cycles = 6200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 83, .duration_cycles = 6200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 235: Pad II 4
    {
        .name = "Pad II 4",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 77, .duration_cycles = 6800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 78, .duration_cycles = 6800, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 79, .duration_cycles = 6800, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE},
            {.wave_idx = 80, .duration_cycles = 6800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO},
            {.wave_idx = 81, .duration_cycles = 6800, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE},
            {.wave_idx = 82, .duration_cycles = 6800, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 83, .duration_cycles = 6800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY},
            {.wave_idx = 84, .duration_cycles = 6800, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END}
        }
    },
    // 236: Pad II 5
    {
        .name = "Pad II 5",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 78, .duration_cycles = 7400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 79, .duration_cycles = 7400, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 80, .duration_cycles = 7400, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 81, .duration_cycles = 7400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 82, .duration_cycles = 7400, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 83, .duration_cycles = 7400, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 84, .duration_cycles = 7400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 85, .duration_cycles = 7400, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },
    // 237: Pad II 6
    {
        .name = "Pad II 6",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 79, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 80, .duration_cycles = 8000, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 81, .duration_cycles = 8000, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 82, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 83, .duration_cycles = 8000, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 84, .duration_cycles = 8000, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 85, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 86, .duration_cycles = 8000, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },
    // 238: Pad II 7
    {
        .name = "Pad II 7",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 80, .duration_cycles = 8600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 81, .duration_cycles = 8600, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 82, .duration_cycles = 8600, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 83, .duration_cycles = 8600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 84, .duration_cycles = 8600, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 85, .duration_cycles = 8600, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 86, .duration_cycles = 8600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 87, .duration_cycles = 8600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },
    // 239: Pad II 8
    {
        .name = "Pad II 8",
        .end_action = PX_WSEQ_END_HOLD,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .prob_mute_score = 5,
        .prob_skip_score = 10,
        .rnd_octave_range = 30,
        .steps = {
            {.wave_idx = 81, .duration_cycles = 9200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 82, .duration_cycles = 9200, .pitch_offset = 200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 83, .duration_cycles = 9200, .pitch_offset = -200, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_MUTE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 84, .duration_cycles = 9200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_RESET_LFO | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 85, .duration_cycles = 9200, .pitch_offset = 400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_RND_WAVE | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 86, .duration_cycles = 9200, .pitch_offset = -400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_USE_PROB_SKIP | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 87, .duration_cycles = 9200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_REVERSE_PLAY | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD},
            {.wave_idx = 88, .duration_cycles = 9200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_END | PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD}
        }
    },

    // --- Bank 30: Screaming Edges (240-247) ---
    // Wild, funky, and glitchy textures that push the limits.
    // 240: Glitch Funk
    {
        .name = "Glitch Funk",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_skip_score = 40,
        .steps = {
            {.wave_idx = 35, .duration_cycles = 25, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP}, // Bit Crush Bomb
            {.wave_idx = 35, .duration_cycles = 25, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 35, .duration_cycles = 25, .pitch_offset = -1200, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 35, .duration_cycles = 25, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 241: Neuro Tear
    {
        .name = "Neuro Tear",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .xmod_depth = 0.6f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 65, .duration_cycles = 300, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD}, // FM Bass Growl
            {.wave_idx = 65, .duration_cycles = 100, .pitch_offset = 500, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.wave_idx = 65, .duration_cycles = 400, .pitch_offset = -2400, .flags = PX_WSEQ_GLIDE | PX_WSEQ_XMOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 242: Jazz Bot
    {
        .name = "Jazz Bot",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_octave_range = 40,
        .prob_skip_score = 20,
        .steps = {
            {.wave_idx = 110, .duration_cycles = 40, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_OCTAVE}, // Organ
            {.wave_idx = 110, .duration_cycles = 40, .pitch_offset = 300, .flags = PX_WSEQ_USE_RND_OCTAVE | PX_WSEQ_USE_PROB_SKIP},
            {.wave_idx = 110, .duration_cycles = 40, .pitch_offset = 700, .flags = PX_WSEQ_USE_RND_OCTAVE},
            {.wave_idx = 110, .duration_cycles = 40, .pitch_offset = 500, .flags = PX_WSEQ_USE_RND_OCTAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 243: Screaming Sync
    {
        .name = "Screaming Sync",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 116, .duration_cycles = 200, .pitch_offset = 2400, .flags = PX_WSEQ_LOCK_PHASE | PX_WSEQ_GLIDE}, // Sync Sweep
            {.wave_idx = 116, .duration_cycles = 200, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        },
        .lock_phase_mod_src = -1
    },
    // 244: Tape Chew
    {
        .name = "Tape Chew",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 58, .duration_cycles = 350, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE}, // Phase Glitch
            {.wave_idx = 58, .duration_cycles = 50, .pitch_offset = -50, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE},
            {.wave_idx = 58, .duration_cycles = 350, .pitch_offset = 0, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE},
            {.wave_idx = 58, .duration_cycles = 50, .pitch_offset = 1200, .flags = PX_WSEQ_REVERSE_PLAY | PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 245: Machine Gun
    {
        .name = "Machine Gun",
        .end_action = PX_WSEQ_END_LOOP,
        .steps = {
            {.wave_idx = 228, .duration_cycles = 8, .pitch_offset = 0, .flags = PX_WSEQ_RETRIG_ADSR | PX_WSEQ_BITCRUSH}, // POKEY Filtered
            {.wave_idx = 228, .duration_cycles = 8, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        },
        .adsr_retrig_phase = 1
    },
    // 246: Ring Mod City
    {
        .name = "Ring Mod City",
        .end_action = PX_WSEQ_END_LOOP,
        .ring_mod_depth = 1.0f,
        .ring_mod_mod_src = -1,
        .rnd_wave_low = 192,
        .rnd_wave_high = 200,
        .steps = {
            {.wave_idx = 195, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_USE_RND_WAVE}, // Metallic
            {.wave_idx = 195, .duration_cycles = 100, .pitch_offset = 1200, .flags = PX_WSEQ_RING_MOD | PX_WSEQ_USE_RND_WAVE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 247: Hyper Zip
    {
        .name = "Hyper Zip",
        .end_action = PX_WSEQ_END_PINGPONG,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 4, .duration_cycles = 10, .pitch_offset = -2400, .flags = PX_WSEQ_GLIDE}, // Square
            {.wave_idx = 4, .duration_cycles = 10, .pitch_offset = 2400, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },

    // --- Bank 31: Total Meltdown (248-255) ---
    // Pure chaos, noise, and experimental destruction.
    // 248: Feedback Fry
    {
        .name = "Feedback Fry",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.8f,
        .xmod_mod_src = -1,
        .steps = {
            {.wave_idx = 53, .duration_cycles = 150, .pitch_offset = 0, .flags = PX_WSEQ_XMOD | PX_WSEQ_BITCRUSH}, // Glitch Sine
            {.wave_idx = 53, .duration_cycles = 150, .pitch_offset = 1200, .flags = PX_WSEQ_XMOD | PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },
    // 249: Mosh Pit
    {
        .name = "Mosh Pit",
        .end_action = PX_WSEQ_END_LOOP,
        .rnd_wave_low = 176,
        .rnd_wave_high = 239,
        .rnd_octave_range = 60,
        .steps = {
            {.wave_idx = 0, .duration_cycles = 20, .pitch_offset = 0, .flags = PX_WSEQ_USE_RND_WAVE | PX_WSEQ_USE_RND_OCTAVE}, // Arcade/POKEY random
            {.flags = PX_WSEQ_END}
        }
    },
    // 250: Pulsar
    {
        .name = "Pulsar",
        .end_action = PX_WSEQ_END_LOOP,
        .ring_mod_depth = 0.5f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 56, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_RESET_LFO | PX_WSEQ_RING_MOD}, // Phase Distortion
            {.wave_idx = 56, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_RING_MOD},
            {.flags = PX_WSEQ_END}
        }
    },
    // 251: Noise Scrape
    {
        .name = "Noise Scrape",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_SMOOTH,
        .steps = {
            {.wave_idx = 167, .duration_cycles = 600, .pitch_offset = -1200, .flags = PX_WSEQ_GLIDE}, // LFSR Spectral Shift
            {.wave_idx = 167, .duration_cycles = 600, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 252: Static Burst
    {
        .name = "Static Burst",
        .end_action = PX_WSEQ_END_LOOP,
        .prob_mute_score = 60,
        .steps = {
            {.wave_idx = 236, .duration_cycles = 10, .pitch_offset = 0, .flags = PX_WSEQ_USE_PROB_MUTE}, // POKEY 15kHz Noise
            {.wave_idx = 0,   .duration_cycles = 30, .pitch_offset = 0, .flags = 0},
            {.flags = PX_WSEQ_END}
        }
    },
    // 253: Logic Fail
    {
        .name = "Logic Fail",
        .end_action = PX_WSEQ_END_LOOP,
        .xmod_depth = 0.5f,
        .xmod_mod_src = -1,
        .ring_mod_depth = 0.5f,
        .ring_mod_mod_src = -1,
        .steps = {
            {.wave_idx = 240, .duration_cycles = 100, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH | PX_WSEQ_XMOD | PX_WSEQ_RING_MOD}, // PWM Hash
            {.flags = PX_WSEQ_END}
        }
    },
    // 254: Cyber Jazz Solo
    {
        .name = "Cyber Jazz Solo",
        .end_action = PX_WSEQ_END_LOOP,
        .glide_mode = PX_WSEQ_GLIDE_STEP,
        .steps = {
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 0, .flags = PX_WSEQ_GLIDE}, // FM Pitched Grit
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 300, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 500, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 700, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 1000, .flags = PX_WSEQ_GLIDE},
            {.wave_idx = 69, .duration_cycles = 40, .pitch_offset = 1200, .flags = PX_WSEQ_GLIDE},
            {.flags = PX_WSEQ_END}
        }
    },
    // 255: System Meltdown
    {
        .name = "System Meltdown",
        .end_action = PX_WSEQ_END_STOP,
        .steps = {
            {.wave_idx = 251, .duration_cycles = 50, .pitch_offset = 0, .flags = PX_WSEQ_BITCRUSH}, // POKEY T+N
            {.wave_idx = 251, .duration_cycles = 50, .pitch_offset = -500, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 251, .duration_cycles = 100, .pitch_offset = -1200, .flags = PX_WSEQ_BITCRUSH},
            {.wave_idx = 251, .duration_cycles = 200, .pitch_offset = -2400, .flags = PX_WSEQ_BITCRUSH},
            {.flags = PX_WSEQ_END}
        }
    },

};
#endif // PX_WSEQ_ROM_IMP_INCLUDED
#endif // PX_WSEQ_ROM_IMPLEMENTATION
