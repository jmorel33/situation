/***************************************************************************************************
*
*   sit/aud/registry_init.h - Device Registry Initialization
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 1 implementation: Register all built-in audio devices.
*   
*   This file contains the initialization code that registers all 18+ built-in audio devices
*   with the device registry. It should be called once during audio subsystem initialization.
*   
*   Registered Devices:
*     • Effects: Reverb, Echo, Chorus, Phaser, Overdrive, Exciter, Maximizer, Spring Reverb,
*                Studio Reverb, SST-282
*     • Dynamics: Dynamics Processor, EQ (4-Band), Filter
*     • Utilities: Panner, Gain, Mixer
*     • Sources: Sound Source, Tone Synth
*     • Capture: Mic Capture
*     • Modulators: LFO, Envelope Follower
*   
***************************************************************************************************/

#ifndef SITUATION_REGISTRY_INIT_H
#define SITUATION_REGISTRY_INIT_H

#include "device_registry.h"

// ================================================================================================
// DEVICE REGISTRATION FUNCTIONS
// ================================================================================================

/**
 * @brief Register the Reverb device (Freeverb-style).
 * @details First device registered - demonstrates the complete registration process.
 */
static void _SituationRegisterReverb(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_REVERB;
    strncpy(meta.name, "Reverb", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    // Audio ports
    meta.num_audio_ins = 2;     // Stereo in
    meta.num_audio_outs = 2;    // Stereo out
    meta.audio_channels = 2;    // Stereo
    
    // No control ports (modulation not yet implemented)
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // Controls
    meta.num_controls = 5;
    
    // Control 0: room_size
    strncpy(meta.controls[0].name, "room_size", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 1.0f;
    meta.controls[0].default_value = 0.5f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // Control 1: damping
    strncpy(meta.controls[1].name, "damping", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 1.0f;
    meta.controls[1].default_value = 0.5f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // Control 2: wet_level
    strncpy(meta.controls[2].name, "wet_level", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.3f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // Control 3: dry_level
    strncpy(meta.controls[3].name, "dry_level", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.0f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 0.7f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // Control 4: width
    strncpy(meta.controls[4].name, "width", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 1.0f;
    meta.controls[4].default_value = 1.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // Metadata
    meta.latency_samples = 0;
    meta.description = "Freeverb-style reverb with room size and damping controls";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;  // 1.0.0
    
    // Function pointers (Phase 3 - not yet implemented)
    meta.create_func = NULL;
    meta.destroy_func = NULL;
    meta.process_func = NULL;
    meta.set_control_func = NULL;
    meta.get_control_func = NULL;
    
    // Register
    SituationError err = SituationRegisterDeviceType(&meta);
    if (err != SITUATION_SUCCESS) {
        // Log error (use actual logging system)
        // SituationLogError("Failed to register Reverb: %s", SituationGetRegistryErrorMessage(err));
    }
}

/**
 * @brief Register the Echo/Delay device.
 */
static void _SituationRegisterEcho(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_ECHO;
    strncpy(meta.name, "Echo", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 3;
    
    // Control 0: time
    strncpy(meta.controls[0].name, "time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.01f;
    meta.controls[0].max_value = 2.0f;
    meta.controls[0].default_value = 0.5f;
    meta.controls[0].units = "s";
    meta.controls[0].is_logarithmic = false;
    
    // Control 1: feedback
    strncpy(meta.controls[1].name, "feedback", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 1.0f;
    meta.controls[1].default_value = 0.3f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // Control 2: wet_mix
    strncpy(meta.controls[2].name, "wet_mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.5f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Stereo delay/echo with feedback control";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Tone Synthesizer device.
 */
static void _SituationRegisterToneSynth(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_TONE_SYNTH;
    strncpy(meta.name, "Tone Synth", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_SOURCE;
    
    meta.num_audio_ins = 0;     // Pure generator
    meta.num_audio_outs = 2;    // Stereo out
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // Note: Tone synth has per-voice controls, not global controls
    // This is a special case - the controls here represent the voice template
    meta.num_controls = 39;
    
    // Control 0: frequency
    strncpy(meta.controls[0].name, "frequency", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 20.0f;
    meta.controls[0].max_value = 20000.0f;
    meta.controls[0].default_value = 440.0f;
    meta.controls[0].units = "Hz";
    meta.controls[0].is_logarithmic = true;
    
    // Control 1: waveform (enum)
    strncpy(meta.controls[1].name, "waveform", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_ENUM;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 4.0f;
    meta.controls[1].default_value = 0.0f;
    meta.controls[1].enum_count = 5;
    static const char* waveform_labels[] = {"Sine", "Pulse", "Triangle", "Saw", "Noise", NULL};
    meta.controls[1].enum_labels = waveform_labels;
    
    // Control 2: volume
    strncpy(meta.controls[2].name, "volume", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.5f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // Control 3: pan
    strncpy(meta.controls[3].name, "pan", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = -1.0f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 0.0f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // Control 4: attack
    strncpy(meta.controls[4].name, "attack", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 2.0f;
    meta.controls[4].default_value = 0.01f;
    meta.controls[4].units = "s";
    meta.controls[4].is_logarithmic = true;
    
    // Control 5: decay
    strncpy(meta.controls[5].name, "decay", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 0.0f;
    meta.controls[5].max_value = 2.0f;
    meta.controls[5].default_value = 0.1f;
    meta.controls[5].units = "s";
    meta.controls[5].is_logarithmic = true;
    
    // Control 6: sustain
    strncpy(meta.controls[6].name, "sustain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = 0.0f;
    meta.controls[6].max_value = 1.0f;
    meta.controls[6].default_value = 0.7f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    // Control 7: release
    strncpy(meta.controls[7].name, "release", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = 0.0f;
    meta.controls[7].max_value = 5.0f;
    meta.controls[7].default_value = 0.2f;
    meta.controls[7].units = "s";
    meta.controls[7].is_logarithmic = true;
    
    // Control 8: hold
    strncpy(meta.controls[8].name, "hold", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[8].id = 8;
    meta.controls[8].type = SITUATION_CONTROL_FLOAT;
    meta.controls[8].min_value = -1.0f;  // -1 = infinite
    meta.controls[8].max_value = 10.0f;
    meta.controls[8].default_value = 1.0f;
    meta.controls[8].units = "s";
    meta.controls[8].is_logarithmic = false;
    
    // Control 9: filter mode (PxFilterMode 0=OFF .. 8=BP+HP)
    strncpy(meta.controls[9].name, "filter_mode", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[9].id = 9;
    meta.controls[9].type = SITUATION_CONTROL_ENUM;
    meta.controls[9].min_value = 0.0f;
    meta.controls[9].max_value = 8.0f;
    meta.controls[9].default_value = 0.0f;
    meta.controls[9].enum_count = 9;
    static const char* filter_mode_labels[] = {
        "Off", "LP", "HP", "BP", "Notch", "Allpass", "LP+BP", "LP+HP", "BP+HP", NULL
    };
    meta.controls[9].enum_labels = filter_mode_labels;
    
    // Control 10: filter cutoff
    strncpy(meta.controls[10].name, "filter_cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[10].id = 10;
    meta.controls[10].type = SITUATION_CONTROL_FLOAT;
    meta.controls[10].min_value = 20.0f;
    meta.controls[10].max_value = 20000.0f;
    meta.controls[10].default_value = 2000.0f;
    meta.controls[10].units = "Hz";
    meta.controls[10].is_logarithmic = true;
    
    // Control 11: filter resonance (Q)
    strncpy(meta.controls[11].name, "filter_resonance", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[11].id = 11;
    meta.controls[11].type = SITUATION_CONTROL_FLOAT;
    meta.controls[11].min_value = 0.5f;
    meta.controls[11].max_value = 20.0f;
    meta.controls[11].default_value = 0.707f;
    meta.controls[11].units = NULL;
    meta.controls[11].is_logarithmic = false;
    
    // Control 12: filter poles
    strncpy(meta.controls[12].name, "filter_poles", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[12].id = 12;
    meta.controls[12].type = SITUATION_CONTROL_INT;
    meta.controls[12].min_value = 1.0f;
    meta.controls[12].max_value = 4.0f;
    meta.controls[12].default_value = 3.0f;
    meta.controls[12].units = NULL;
    meta.controls[12].is_logarithmic = false;
    
    // Control 13: filter drive
    strncpy(meta.controls[13].name, "filter_drive", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[13].id = 13;
    meta.controls[13].type = SITUATION_CONTROL_FLOAT;
    meta.controls[13].min_value = 1.0f;
    meta.controls[13].max_value = 10.0f;
    meta.controls[13].default_value = 1.0f;
    meta.controls[13].units = NULL;
    meta.controls[13].is_logarithmic = false;
    
    // Control 14: filter keytrack
    strncpy(meta.controls[14].name, "filter_keytrack", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[14].id = 14;
    meta.controls[14].type = SITUATION_CONTROL_FLOAT;
    meta.controls[14].min_value = 0.0f;
    meta.controls[14].max_value = 1.0f;
    meta.controls[14].default_value = 0.0f;
    meta.controls[14].units = NULL;
    meta.controls[14].is_logarithmic = false;
    
    // Control 15: filter oversampling (0=off, 1=2x, 2=2x forced)
    strncpy(meta.controls[15].name, "filter_oversample", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[15].id = 15;
    meta.controls[15].type = SITUATION_CONTROL_INT;
    meta.controls[15].min_value = 0.0f;
    meta.controls[15].max_value = 2.0f;
    meta.controls[15].default_value = 1.0f;
    meta.controls[15].units = NULL;
    meta.controls[15].is_logarithmic = false;
    
    // Control 16: voice mode (0=poly, 1=mono)
    strncpy(meta.controls[16].name, "voice_mode", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[16].id = 16;
    meta.controls[16].type = SITUATION_CONTROL_ENUM;
    meta.controls[16].min_value = 0.0f;
    meta.controls[16].max_value = 1.0f;
    meta.controls[16].default_value = 0.0f;
    meta.controls[16].enum_count = 2;
    static const char* voice_mode_labels[] = {"Poly", "Mono", NULL};
    meta.controls[16].enum_labels = voice_mode_labels;
    
    // Control 17: pulse width (duty cycle for waveform 1)
    strncpy(meta.controls[17].name, "pulse_width", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[17].id = 17;
    meta.controls[17].type = SITUATION_CONTROL_FLOAT;
    meta.controls[17].min_value = 0.05f;
    meta.controls[17].max_value = 0.95f;
    meta.controls[17].default_value = 0.5f;
    meta.controls[17].units = NULL;
    meta.controls[17].is_logarithmic = false;
    
    // Control 18: LFO rate (Hz)
    strncpy(meta.controls[18].name, "lfo_rate", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[18].id = 18;
    meta.controls[18].type = SITUATION_CONTROL_FLOAT;
    meta.controls[18].min_value = 0.0f;
    meta.controls[18].max_value = 20.0f;
    meta.controls[18].default_value = 0.0f;
    meta.controls[18].units = "Hz";
    meta.controls[18].is_logarithmic = true;
    
    // Control 19: LFO waveform
    strncpy(meta.controls[19].name, "lfo_waveform", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[19].id = 19;
    meta.controls[19].type = SITUATION_CONTROL_ENUM;
    meta.controls[19].min_value = 0.0f;
    meta.controls[19].max_value = 2.0f;
    meta.controls[19].default_value = 0.0f;
    meta.controls[19].enum_count = 3;
    static const char* lfo_waveform_labels[] = {"Triangle", "Square", "Random", NULL};
    meta.controls[19].enum_labels = lfo_waveform_labels;
    
    // Control 20: LFO pitch amount
    strncpy(meta.controls[20].name, "lfo_pitch_amount", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[20].id = 20;
    meta.controls[20].type = SITUATION_CONTROL_FLOAT;
    meta.controls[20].min_value = 0.0f;
    meta.controls[20].max_value = 1.0f;
    meta.controls[20].default_value = 0.0f;
    meta.controls[20].units = NULL;
    meta.controls[20].is_logarithmic = false;
    
    // Control 21: LFO pitch range (semitones)
    strncpy(meta.controls[21].name, "lfo_pitch_range", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[21].id = 21;
    meta.controls[21].type = SITUATION_CONTROL_FLOAT;
    meta.controls[21].min_value = 0.0f;
    meta.controls[21].max_value = 12.0f;
    meta.controls[21].default_value = 2.0f;
    meta.controls[21].units = "st";
    meta.controls[21].is_logarithmic = false;
    
    // Control 22: LFO PWM amount
    strncpy(meta.controls[22].name, "lfo_pwm_amount", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[22].id = 22;
    meta.controls[22].type = SITUATION_CONTROL_FLOAT;
    meta.controls[22].min_value = 0.0f;
    meta.controls[22].max_value = 1.0f;
    meta.controls[22].default_value = 0.0f;
    meta.controls[22].units = NULL;
    meta.controls[22].is_logarithmic = false;
    
    // Control 23: LFO PWM range (duty excursion)
    strncpy(meta.controls[23].name, "lfo_pwm_range", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[23].id = 23;
    meta.controls[23].type = SITUATION_CONTROL_FLOAT;
    meta.controls[23].min_value = 0.0f;
    meta.controls[23].max_value = 0.45f;
    meta.controls[23].default_value = 0.2f;
    meta.controls[23].units = NULL;
    meta.controls[23].is_logarithmic = false;
    
    // Control 24: LFO filter amount
    strncpy(meta.controls[24].name, "lfo_filter_amount", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[24].id = 24;
    meta.controls[24].type = SITUATION_CONTROL_FLOAT;
    meta.controls[24].min_value = 0.0f;
    meta.controls[24].max_value = 1.0f;
    meta.controls[24].default_value = 0.0f;
    meta.controls[24].units = NULL;
    meta.controls[24].is_logarithmic = false;
    
    // Control 25: LFO filter range (Hz span)
    strncpy(meta.controls[25].name, "lfo_filter_range", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[25].id = 25;
    meta.controls[25].type = SITUATION_CONTROL_FLOAT;
    meta.controls[25].min_value = 20.0f;
    meta.controls[25].max_value = 8000.0f;
    meta.controls[25].default_value = 1000.0f;
    meta.controls[25].units = "Hz";
    meta.controls[25].is_logarithmic = true;
    
    // Control 26: ADSR envelope filter amount
    strncpy(meta.controls[26].name, "filter_env_amount", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[26].id = 26;
    meta.controls[26].type = SITUATION_CONTROL_FLOAT;
    meta.controls[26].min_value = 0.0f;
    meta.controls[26].max_value = 1.0f;
    meta.controls[26].default_value = 0.0f;
    meta.controls[26].units = NULL;
    meta.controls[26].is_logarithmic = false;
    
    // Control 27: ADSR envelope filter range (Hz span)
    strncpy(meta.controls[27].name, "filter_env_range", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[27].id = 27;
    meta.controls[27].type = SITUATION_CONTROL_FLOAT;
    meta.controls[27].min_value = 20.0f;
    meta.controls[27].max_value = 8000.0f;
    meta.controls[27].default_value = 4000.0f;
    meta.controls[27].units = "Hz";
    meta.controls[27].is_logarithmic = true;
    
    // Control 28: portamento time (mono RC glide tau; 0 = use speed only / instant)
    strncpy(meta.controls[28].name, "portamento_time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[28].id = 28;
    meta.controls[28].type = SITUATION_CONTROL_FLOAT;
    meta.controls[28].min_value = 0.0f;
    meta.controls[28].max_value = 2.0f;
    meta.controls[28].default_value = 0.0f;
    meta.controls[28].units = "s";
    meta.controls[28].is_logarithmic = true;
    
    // Control 29: portamento speed (semitones per second; 0 = use time only / instant)
    strncpy(meta.controls[29].name, "portamento_speed", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[29].id = 29;
    meta.controls[29].type = SITUATION_CONTROL_FLOAT;
    meta.controls[29].min_value = 0.0f;
    meta.controls[29].max_value = 48.0f;
    meta.controls[29].default_value = 0.0f;
    meta.controls[29].units = "st/s";
    meta.controls[29].is_logarithmic = false;
    
    // Control 30: sub-oscillator mix level
    strncpy(meta.controls[30].name, "sub_level", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[30].id = 30;
    meta.controls[30].type = SITUATION_CONTROL_FLOAT;
    meta.controls[30].min_value = 0.0f;
    meta.controls[30].max_value = 1.0f;
    meta.controls[30].default_value = 0.0f;
    meta.controls[30].units = NULL;
    meta.controls[30].is_logarithmic = false;
    
    // Control 31: sub-oscillator waveform (same set as main)
    strncpy(meta.controls[31].name, "sub_waveform", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[31].id = 31;
    meta.controls[31].type = SITUATION_CONTROL_ENUM;
    meta.controls[31].min_value = 0.0f;
    meta.controls[31].max_value = 4.0f;
    meta.controls[31].default_value = 0.0f;
    meta.controls[31].enum_count = 5;
    static const char* sub_waveform_labels[] = {"Sine", "Pulse", "Triangle", "Saw", "Noise", NULL};
    meta.controls[31].enum_labels = sub_waveform_labels;
    
    // Control 32: sub-oscillator octave (0 = unison, 1 = −1 oct, 2 = −2 oct)
    strncpy(meta.controls[32].name, "sub_octave", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[32].id = 32;
    meta.controls[32].type = SITUATION_CONTROL_ENUM;
    meta.controls[32].min_value = 0.0f;
    meta.controls[32].max_value = 2.0f;
    meta.controls[32].default_value = 1.0f;
    meta.controls[32].enum_count = 3;
    static const char* sub_octave_labels[] = {"Unison", "Oct -1", "Oct -2", NULL};
    meta.controls[32].enum_labels = sub_octave_labels;
    
    // Control 33: sub-oscillator fine tune (semitones)
    strncpy(meta.controls[33].name, "sub_fine", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[33].id = 33;
    meta.controls[33].type = SITUATION_CONTROL_FLOAT;
    meta.controls[33].min_value = -1.0f;
    meta.controls[33].max_value = 1.0f;
    meta.controls[33].default_value = 0.0f;
    meta.controls[33].units = "st";
    meta.controls[33].is_logarithmic = false;
    
    strncpy(meta.controls[34].name, "sub_coarse", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[34].id = 34;
    meta.controls[34].type = SITUATION_CONTROL_FLOAT;
    meta.controls[34].min_value = -12.0f;
    meta.controls[34].max_value = 12.0f;
    meta.controls[34].default_value = 0.0f;
    meta.controls[34].units = "st";
    meta.controls[34].is_logarithmic = false;
    
    strncpy(meta.controls[35].name, "sub_sync", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[35].id = 35;
    meta.controls[35].type = SITUATION_CONTROL_BOOL;
    meta.controls[35].min_value = 0.0f;
    meta.controls[35].max_value = 1.0f;
    meta.controls[35].default_value = 0.0f;
    meta.controls[35].units = NULL;
    meta.controls[35].is_logarithmic = false;
    
    strncpy(meta.controls[36].name, "sub_ring_mod", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[36].id = 36;
    meta.controls[36].type = SITUATION_CONTROL_BOOL;
    meta.controls[36].min_value = 0.0f;
    meta.controls[36].max_value = 1.0f;
    meta.controls[36].default_value = 0.0f;
    meta.controls[36].units = NULL;
    meta.controls[36].is_logarithmic = false;

    strncpy(meta.controls[37].name, "patch_slot", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[37].id = 37;
    meta.controls[37].type = SITUATION_CONTROL_INT;
    meta.controls[37].min_value = 0.0f;
    meta.controls[37].max_value = 15.0f;
    meta.controls[37].default_value = 0.0f;
    meta.controls[37].units = NULL;
    meta.controls[37].is_logarithmic = false;

    strncpy(meta.controls[38].name, "patch_store", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[38].id = 38;
    meta.controls[38].type = SITUATION_CONTROL_BOOL;
    meta.controls[38].min_value = 0.0f;
    meta.controls[38].max_value = 1.0f;
    meta.controls[38].default_value = 0.0f;
    meta.controls[38].units = NULL;
    meta.controls[38].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Graph tone synth: ADSR, SVF, pulse, mono/poly, portamento, mod LFO, sub-osc";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Panner utility device.
 */
static void _SituationRegisterPanner(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_PANNER;
    strncpy(meta.name, "Panner", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_UTILITY;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 1;      // Can be modulated by LFO
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 1;
    
    // Control 0: pan
    strncpy(meta.controls[0].name, "pan", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = -1.0f;
    meta.controls[0].max_value = 1.0f;
    meta.controls[0].default_value = 0.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Constant power stereo panner";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Chorus (4-Stage) device.
 */
static void _SituationRegisterChorus(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_CHORUS;
    strncpy(meta.name, "Chorus 4-Stage", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // 4 stages × 4 params + 5 global = 21 controls
    meta.num_controls = 21;
    
    int ctrl_idx = 0;
    
    // Per-stage controls (4 stages)
    for (int stage = 0; stage < 4; stage++) {
        char name_buf[SITUATION_MAX_CONTROL_NAME];
        
        // base_delay_ms
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "stage%d_base_delay_ms", stage);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 1.0f;
        meta.controls[ctrl_idx].max_value = 50.0f;
        meta.controls[ctrl_idx].default_value = 10.0f + stage * 5.0f;
        meta.controls[ctrl_idx].units = "ms";
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // lfo_freq
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "stage%d_lfo_freq", stage);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.1f;
        meta.controls[ctrl_idx].max_value = 10.0f;
        meta.controls[ctrl_idx].default_value = 0.5f + stage * 0.2f;
        meta.controls[ctrl_idx].units = "Hz";
        meta.controls[ctrl_idx].is_logarithmic = true;
        ctrl_idx++;
        
        // lfo_depth_ms
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "stage%d_lfo_depth_ms", stage);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.1f;
        meta.controls[ctrl_idx].max_value = 20.0f;
        meta.controls[ctrl_idx].default_value = 2.0f;
        meta.controls[ctrl_idx].units = "ms";
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // pan
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "stage%d_pan", stage);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = -1.0f;
        meta.controls[ctrl_idx].max_value = 1.0f;
        meta.controls[ctrl_idx].default_value = (stage % 2 == 0) ? -0.5f : 0.5f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
    }
    
    // Global controls
    // width
    strncpy(meta.controls[ctrl_idx].name, "width", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 1.0f;
    meta.controls[ctrl_idx].default_value = 0.5f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // dry_gain
    strncpy(meta.controls[ctrl_idx].name, "dry_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 2.0f;
    meta.controls[ctrl_idx].default_value = 1.0f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // wet_gain
    strncpy(meta.controls[ctrl_idx].name, "wet_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 2.0f;
    meta.controls[ctrl_idx].default_value = 0.5f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // feedback
    strncpy(meta.controls[ctrl_idx].name, "feedback", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 0.99f;
    meta.controls[ctrl_idx].default_value = 0.0f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // stereo_enhance
    strncpy(meta.controls[ctrl_idx].name, "stereo_enhance", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 2.0f;
    meta.controls[ctrl_idx].default_value = 1.0f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "4-stage chorus with per-stage LFO control and stereo enhancement";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Phaser device.
 */
static void _SituationRegisterPhaser(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_PHASER;
    strncpy(meta.name, "Phaser", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 6;
    
    // lfo_freq
    strncpy(meta.controls[0].name, "lfo_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.1f;
    meta.controls[0].max_value = 10.0f;
    meta.controls[0].default_value = 0.5f;
    meta.controls[0].units = "Hz";
    meta.controls[0].is_logarithmic = true;
    
    // feedback
    strncpy(meta.controls[1].name, "feedback", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 0.99f;
    meta.controls[1].default_value = 0.5f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // mix
    strncpy(meta.controls[2].name, "mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.5f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // pan_depth
    strncpy(meta.controls[3].name, "pan_depth", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.0f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 0.5f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // stereo_width
    strncpy(meta.controls[4].name, "stereo_width", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 2.0f;
    meta.controls[4].default_value = 1.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // feedback_delay_ms
    strncpy(meta.controls[5].name, "feedback_delay_ms", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 0.0f;
    meta.controls[5].max_value = 100.0f;
    meta.controls[5].default_value = 10.0f;
    meta.controls[5].units = "ms";
    meta.controls[5].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "All-pass filter phaser with feedback delay and stereo widening";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Overdrive device.
 */
static void _SituationRegisterOverdrive(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_OVERDRIVE;
    strncpy(meta.name, "Overdrive", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 10;
    
    // mode (enum)
    strncpy(meta.controls[0].name, "mode", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_ENUM;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 3.0f;
    meta.controls[0].default_value = 0.0f;
    meta.controls[0].enum_count = 4;
    static const char* overdrive_mode_labels[] = {"Soft", "Hard", "Tube", "Fold", NULL};
    meta.controls[0].enum_labels = overdrive_mode_labels;
    
    // drive
    strncpy(meta.controls[1].name, "drive", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 200.0f;
    meta.controls[1].default_value = 40.0f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // input_gain
    strncpy(meta.controls[2].name, "input_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 4.0f;
    meta.controls[2].default_value = 1.0f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // output_gain
    strncpy(meta.controls[3].name, "output_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.0f;
    meta.controls[3].max_value = 2.0f;
    meta.controls[3].default_value = 1.0f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // mix
    strncpy(meta.controls[4].name, "mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 1.0f;
    meta.controls[4].default_value = 1.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // filter_cutoff
    strncpy(meta.controls[5].name, "filter_cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 20.0f;
    meta.controls[5].max_value = 22050.0f;
    meta.controls[5].default_value = 4500.0f;
    meta.controls[5].units = "Hz";
    meta.controls[5].is_logarithmic = true;
    
    // filter_res
    strncpy(meta.controls[6].name, "filter_res", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = 0.0f;
    meta.controls[6].max_value = 1.0f;
    meta.controls[6].default_value = 0.4f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    // low_shelf_db
    strncpy(meta.controls[7].name, "low_shelf_db", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = -24.0f;
    meta.controls[7].max_value = 24.0f;
    meta.controls[7].default_value = 3.0f;
    meta.controls[7].units = "dB";
    meta.controls[7].is_logarithmic = false;
    
    // high_shelf_db
    strncpy(meta.controls[8].name, "high_shelf_db", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[8].id = 8;
    meta.controls[8].type = SITUATION_CONTROL_FLOAT;
    meta.controls[8].min_value = -24.0f;
    meta.controls[8].max_value = 24.0f;
    meta.controls[8].default_value = -3.0f;
    meta.controls[8].units = "dB";
    meta.controls[8].is_logarithmic = false;
    
    // asymmetry
    strncpy(meta.controls[9].name, "asymmetry", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[9].id = 9;
    meta.controls[9].type = SITUATION_CONTROL_FLOAT;
    meta.controls[9].min_value = -1.0f;
    meta.controls[9].max_value = 1.0f;
    meta.controls[9].default_value = 0.0f;
    meta.controls[9].units = NULL;
    meta.controls[9].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Multi-mode overdrive with post-distortion filter and EQ";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Maximizer device.
 */
static void _SituationRegisterMaximizer(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_MAXIMIZER;
    strncpy(meta.name, "Maximizer", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 1;     // Mono per instance
    meta.num_audio_outs = 1;
    meta.audio_channels = 1;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // Dynamic bands (4 max) + 2 filter controls = variable, using max 14 controls
    // 4 bands × 4 params (center_freq, Q, enhancement_factor, D) + hpf_cutoff + lpf_cutoff
    meta.num_controls = 18;
    
    int ctrl_idx = 0;
    
    // Per-band controls (4 bands max)
    for (int band = 0; band < 4; band++) {
        char name_buf[SITUATION_MAX_CONTROL_NAME];
        
        // center_freq
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "band%d_center_freq", band);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 100.0f;
        meta.controls[ctrl_idx].max_value = 16000.0f;
        meta.controls[ctrl_idx].default_value = 1000.0f * (band + 1);
        meta.controls[ctrl_idx].units = "Hz";
        meta.controls[ctrl_idx].is_logarithmic = true;
        ctrl_idx++;
        
        // Q
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "band%d_Q", band);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.5f;
        meta.controls[ctrl_idx].max_value = 10.0f;
        meta.controls[ctrl_idx].default_value = 2.0f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // enhancement_factor
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "band%d_enhancement", band);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.0f;
        meta.controls[ctrl_idx].max_value = 10.0f;
        meta.controls[ctrl_idx].default_value = 1.0f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // D (number of overtones)
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "band%d_overtones", band);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_INT;
        meta.controls[ctrl_idx].min_value = 1.0f;
        meta.controls[ctrl_idx].max_value = 10.0f;
        meta.controls[ctrl_idx].default_value = 3.0f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
    }
    
    // Global filter controls
    // hpf_cutoff
    strncpy(meta.controls[ctrl_idx].name, "hpf_cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 20.0f;
    meta.controls[ctrl_idx].max_value = 500.0f;
    meta.controls[ctrl_idx].default_value = 30.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    ctrl_idx++;
    
    // lpf_cutoff
    strncpy(meta.controls[ctrl_idx].name, "lpf_cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 8000.0f;
    meta.controls[ctrl_idx].max_value = 20000.0f;
    meta.controls[ctrl_idx].default_value = 16000.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    
    meta.latency_samples = 256;  // Hop size dependent
    meta.description = "FFT-based spectral multiband maximizer with harmonic enhancement";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Spring Reverb device.
 */
static void _SituationRegisterSpringReverb(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_SPRING_REVERB;
    strncpy(meta.name, "Spring Reverb", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 10;
    
    // input_level
    strncpy(meta.controls[0].name, "input_level", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 10.0f;
    meta.controls[0].default_value = 5.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // threshold
    strncpy(meta.controls[1].name, "threshold", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 10.0f;
    meta.controls[1].default_value = 1.0f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // decay_time
    strncpy(meta.controls[2].name, "decay_time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.1f;
    meta.controls[2].max_value = 10.0f;
    meta.controls[2].default_value = 1.0f;
    meta.controls[2].units = "s";
    meta.controls[2].is_logarithmic = true;
    
    // gate_enabled
    strncpy(meta.controls[3].name, "gate_enabled", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_BOOL;
    meta.controls[3].min_value = 0.0f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 1.0f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // bass
    strncpy(meta.controls[4].name, "bass", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = -10.0f;
    meta.controls[4].max_value = 10.0f;
    meta.controls[4].default_value = 0.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // middle
    strncpy(meta.controls[5].name, "middle", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = -10.0f;
    meta.controls[5].max_value = 10.0f;
    meta.controls[5].default_value = 0.0f;
    meta.controls[5].units = NULL;
    meta.controls[5].is_logarithmic = false;
    
    // treble
    strncpy(meta.controls[6].name, "treble", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = -10.0f;
    meta.controls[6].max_value = 10.0f;
    meta.controls[6].default_value = 0.0f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    // direct
    strncpy(meta.controls[7].name, "direct", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = 0.0f;
    meta.controls[7].max_value = 10.0f;
    meta.controls[7].default_value = 5.0f;
    meta.controls[7].units = NULL;
    meta.controls[7].is_logarithmic = false;
    
    // reverb
    strncpy(meta.controls[8].name, "reverb", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[8].id = 8;
    meta.controls[8].type = SITUATION_CONTROL_FLOAT;
    meta.controls[8].min_value = 0.0f;
    meta.controls[8].max_value = 10.0f;
    meta.controls[8].default_value = 5.0f;
    meta.controls[8].units = NULL;
    meta.controls[8].is_logarithmic = false;
    
    // cross_mix
    strncpy(meta.controls[9].name, "cross_mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[9].id = 9;
    meta.controls[9].type = SITUATION_CONTROL_FLOAT;
    meta.controls[9].min_value = 0.0f;
    meta.controls[9].max_value = 10.0f;
    meta.controls[9].default_value = 2.0f;
    meta.controls[9].units = NULL;
    meta.controls[9].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Physical modeling spring reverb with gate and 3-band EQ";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Studio Reverb device.
 */
static void _SituationRegisterStudioReverb(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_STUDIO_REVERB;
    strncpy(meta.name, "Studio Reverb", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 10;
    
    // size (1-13 mapped to volume in m³)
    strncpy(meta.controls[0].name, "size", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_INT;
    meta.controls[0].min_value = 1.0f;
    meta.controls[0].max_value = 13.0f;
    meta.controls[0].default_value = 9.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // decay_time
    strncpy(meta.controls[1].name, "decay_time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.1f;
    meta.controls[1].max_value = 200.0f;
    meta.controls[1].default_value = 2.1f;
    meta.controls[1].units = "s";
    meta.controls[1].is_logarithmic = true;
    
    // bass_coef
    strncpy(meta.controls[2].name, "bass_coef", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.25f;
    meta.controls[2].max_value = 4.0f;
    meta.controls[2].default_value = 1.0f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // treble_coef
    strncpy(meta.controls[3].name, "treble_coef", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.25f;
    meta.controls[3].max_value = 4.0f;
    meta.controls[3].default_value = 0.45f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // pre_delay_ms
    strncpy(meta.controls[4].name, "pre_delay_ms", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 999.0f;
    meta.controls[4].default_value = 21.0f;
    meta.controls[4].units = "ms";
    meta.controls[4].is_logarithmic = false;
    
    // reverb_atten_db
    strncpy(meta.controls[5].name, "reverb_atten_db", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 0.0f;
    meta.controls[5].max_value = 99.0f;
    meta.controls[5].default_value = 14.0f;
    meta.controls[5].units = "dB";
    meta.controls[5].is_logarithmic = false;
    
    // stereo_discorrelator
    strncpy(meta.controls[6].name, "stereo_discorrelator", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = -30.0f;
    meta.controls[6].max_value = 30.0f;
    meta.controls[6].default_value = 10.0f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    // diffusion_db
    strncpy(meta.controls[7].name, "diffusion_db", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = 30.0f;
    meta.controls[7].max_value = 100.0f;
    meta.controls[7].default_value = 50.0f;
    meta.controls[7].units = "dB";
    meta.controls[7].is_logarithmic = false;
    
    // wet_mix
    strncpy(meta.controls[8].name, "wet_mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[8].id = 8;
    meta.controls[8].type = SITUATION_CONTROL_FLOAT;
    meta.controls[8].min_value = 0.0f;
    meta.controls[8].max_value = 1.0f;
    meta.controls[8].default_value = 0.5f;
    meta.controls[8].units = NULL;
    meta.controls[8].is_logarithmic = false;
    
    // preset_index
    strncpy(meta.controls[9].name, "preset_index", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[9].id = 9;
    meta.controls[9].type = SITUATION_CONTROL_INT;
    meta.controls[9].min_value = 0.0f;
    meta.controls[9].max_value = 51.0f;
    meta.controls[9].default_value = 0.0f;
    meta.controls[9].units = NULL;
    meta.controls[9].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Professional algorithmic reverb with early reflections, FDN, and diffusion";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the SST-282 device.
 */
static void _SituationRegisterSST282(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_SST282;
    strncpy(meta.name, "SST-282", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // 4 tap levels + 8 main controls = 12 controls
    meta.num_controls = 12;
    
    // input_gain
    strncpy(meta.controls[0].name, "input_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 10.0f;
    meta.controls[0].default_value = 5.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // lf_cut_dB
    strncpy(meta.controls[1].name, "lf_cut_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 10.0f;
    meta.controls[1].default_value = 0.0f;
    meta.controls[1].units = "dB";
    meta.controls[1].is_logarithmic = false;
    
    // hf_cut_dB
    strncpy(meta.controls[2].name, "hf_cut_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 10.0f;
    meta.controls[2].default_value = 0.0f;
    meta.controls[2].units = "dB";
    meta.controls[2].is_logarithmic = false;
    
    // echo_delay_ms
    strncpy(meta.controls[3].name, "echo_delay_ms", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 30.0f;
    meta.controls[3].max_value = 256.0f;
    meta.controls[3].default_value = 100.0f;
    meta.controls[3].units = "ms";
    meta.controls[3].is_logarithmic = false;
    
    // feedback
    strncpy(meta.controls[4].name, "feedback", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 10.0f;
    meta.controls[4].default_value = 5.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // dry_dB
    strncpy(meta.controls[5].name, "dry_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 0.0f;
    meta.controls[5].max_value = 12.0f;
    meta.controls[5].default_value = 6.0f;
    meta.controls[5].units = "dB";
    meta.controls[5].is_logarithmic = false;
    
    // echo_dB
    strncpy(meta.controls[6].name, "echo_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = 0.0f;
    meta.controls[6].max_value = 12.0f;
    meta.controls[6].default_value = 6.0f;
    meta.controls[6].units = "dB";
    meta.controls[6].is_logarithmic = false;
    
    // direct_dB
    strncpy(meta.controls[7].name, "direct_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = 0.0f;
    meta.controls[7].max_value = 12.0f;
    meta.controls[7].default_value = 6.0f;
    meta.controls[7].units = "dB";
    meta.controls[7].is_logarithmic = false;
    
    // Tap levels (4 pairs)
    for (int i = 0; i < 4; i++) {
        char name_buf[SITUATION_MAX_CONTROL_NAME];
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "tap_level_%d", i);
        strncpy(meta.controls[8 + i].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[8 + i].id = 8 + i;
        meta.controls[8 + i].type = SITUATION_CONTROL_FLOAT;
        meta.controls[8 + i].min_value = 0.0f;
        meta.controls[8 + i].max_value = 12.0f;
        meta.controls[8 + i].default_value = 6.0f;
        meta.controls[8 + i].units = "dB";
        meta.controls[8 + i].is_logarithmic = false;
    }
    
    meta.latency_samples = 6;  // Downsample factor dependent
    meta.description = "Hardware emulation reverb/delay with audition taps and preset system";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Mastering Amp device.
 */
static void _SituationRegisterMasteringAmp(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_MASTERING_AMP;
    strncpy(meta.name, "Mastering Amp", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 15;
    
    // amp_type (enum)
    strncpy(meta.controls[0].name, "amp_type", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_ENUM;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 2.0f;
    meta.controls[0].default_value = 0.0f;
    meta.controls[0].enum_count = 3;
    static const char* amp_type_labels[] = {"Type A", "Type N", "Type C", NULL};
    meta.controls[0].enum_labels = amp_type_labels;
    
    // drive
    strncpy(meta.controls[1].name, "drive", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.0f;
    meta.controls[1].max_value = 1.0f;
    meta.controls[1].default_value = 0.5f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // low_freq
    strncpy(meta.controls[2].name, "low_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 20.0f;
    meta.controls[2].max_value = 200.0f;
    meta.controls[2].default_value = 100.0f;
    meta.controls[2].units = "Hz";
    meta.controls[2].is_logarithmic = true;
    
    // low_gain
    strncpy(meta.controls[3].name, "low_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = -1.0f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 0.0f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // mid_freq
    strncpy(meta.controls[4].name, "mid_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 200.0f;
    meta.controls[4].max_value = 5000.0f;
    meta.controls[4].default_value = 1000.0f;
    meta.controls[4].units = "Hz";
    meta.controls[4].is_logarithmic = true;
    
    // mid_gain
    strncpy(meta.controls[5].name, "mid_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = -1.0f;
    meta.controls[5].max_value = 1.0f;
    meta.controls[5].default_value = 0.0f;
    meta.controls[5].units = NULL;
    meta.controls[5].is_logarithmic = false;
    
    // mid_Q
    strncpy(meta.controls[6].name, "mid_Q", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = 0.5f;
    meta.controls[6].max_value = 5.0f;
    meta.controls[6].default_value = 1.0f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    // high_freq
    strncpy(meta.controls[7].name, "high_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[7].id = 7;
    meta.controls[7].type = SITUATION_CONTROL_FLOAT;
    meta.controls[7].min_value = 5000.0f;
    meta.controls[7].max_value = 20000.0f;
    meta.controls[7].default_value = 10000.0f;
    meta.controls[7].units = "Hz";
    meta.controls[7].is_logarithmic = true;
    
    // high_gain
    strncpy(meta.controls[8].name, "high_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[8].id = 8;
    meta.controls[8].type = SITUATION_CONTROL_FLOAT;
    meta.controls[8].min_value = -1.0f;
    meta.controls[8].max_value = 1.0f;
    meta.controls[8].default_value = 0.0f;
    meta.controls[8].units = NULL;
    meta.controls[8].is_logarithmic = false;
    
    // air_freq
    strncpy(meta.controls[9].name, "air_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[9].id = 9;
    meta.controls[9].type = SITUATION_CONTROL_FLOAT;
    meta.controls[9].min_value = 10000.0f;
    meta.controls[9].max_value = 20000.0f;
    meta.controls[9].default_value = 15000.0f;
    meta.controls[9].units = "Hz";
    meta.controls[9].is_logarithmic = true;
    
    // air_gain
    strncpy(meta.controls[10].name, "air_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[10].id = 10;
    meta.controls[10].type = SITUATION_CONTROL_FLOAT;
    meta.controls[10].min_value = 0.0f;
    meta.controls[10].max_value = 1.0f;
    meta.controls[10].default_value = 0.0f;
    meta.controls[10].units = NULL;
    meta.controls[10].is_logarithmic = false;
    
    // tight_cutoff
    strncpy(meta.controls[11].name, "tight_cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[11].id = 11;
    meta.controls[11].type = SITUATION_CONTROL_FLOAT;
    meta.controls[11].min_value = 20.0f;
    meta.controls[11].max_value = 200.0f;
    meta.controls[11].default_value = 20.0f;
    meta.controls[11].units = "Hz";
    meta.controls[11].is_logarithmic = true;
    
    // aspect_ratio (stereo width)
    strncpy(meta.controls[12].name, "aspect_ratio", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[12].id = 12;
    meta.controls[12].type = SITUATION_CONTROL_FLOAT;
    meta.controls[12].min_value = 0.0f;
    meta.controls[12].max_value = 2.0f;
    meta.controls[12].default_value = 1.0f;
    meta.controls[12].units = NULL;
    meta.controls[12].is_logarithmic = false;
    
    // vintage
    strncpy(meta.controls[13].name, "vintage", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[13].id = 13;
    meta.controls[13].type = SITUATION_CONTROL_BOOL;
    meta.controls[13].min_value = 0.0f;
    meta.controls[13].max_value = 1.0f;
    meta.controls[13].default_value = 0.0f;
    meta.controls[13].units = NULL;
    meta.controls[13].is_logarithmic = false;
    
    // circuit_bending
    strncpy(meta.controls[14].name, "circuit_bending", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[14].id = 14;
    meta.controls[14].type = SITUATION_CONTROL_BOOL;
    meta.controls[14].min_value = 0.0f;
    meta.controls[14].max_value = 1.0f;
    meta.controls[14].default_value = 0.0f;
    meta.controls[14].units = NULL;
    meta.controls[14].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Mastering console processor with Color Amp simulation and 3-band EQ with Air";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the DeafMax device.
 */
static void _SituationRegisterDeafMax(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_DEAFMAX;
    strncpy(meta.name, "DeafMax", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // 5 controls: drive, release, ceiling, makeup, linked
    meta.num_controls = 5;
    
    // drive
    strncpy(meta.controls[0].name, "drive", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 100.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // release
    strncpy(meta.controls[1].name, "release", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.00005f;
    meta.controls[1].max_value = 0.05f;
    meta.controls[1].default_value = 0.0008f;
    meta.controls[1].units = "s";
    meta.controls[1].is_logarithmic = true;
    
    // ceiling
    strncpy(meta.controls[2].name, "ceiling", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = -1.0f;
    meta.controls[2].max_value = 0.0f;
    meta.controls[2].default_value = -0.1f;
    meta.controls[2].units = "dBFS";
    meta.controls[2].is_logarithmic = false;
    
    // makeup
    strncpy(meta.controls[3].name, "makeup", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.0f;
    meta.controls[3].max_value = 10.0f;
    meta.controls[3].default_value = 1.0f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // linked
    strncpy(meta.controls[4].name, "linked", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_BOOL;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 1.0f;
    meta.controls[4].default_value = 1.0f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Zero-latency surgical peak maximizer with extreme loudness and modulatable aggression";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the ISA 110 device (Focusrite ISA 110 preamp + 4-band inductor EQ).
 *
 * Controls (14 total):
 *   0  drive          0.0 – 10.0      transformer saturation
 *   1  hpf_cutoff     20 – 400 Hz     high-pass filter frequency
 *   2  hpf_enabled    0/1             high-pass filter on/off
 *   3  band0_freq     20 – 500 Hz     low shelf frequency
 *   4  band0_gain    -24 – +24 dB     low shelf gain
 *   5  band1_freq    100 – 2000 Hz    low-mid bell frequency
 *   6  band1_gain    -24 – +24 dB     low-mid bell gain
 *   7  band1_q        0.1 – 20.0      low-mid bell Q
 *   8  band2_freq    500 – 10000 Hz   high-mid bell frequency
 *   9  band2_gain    -24 – +24 dB     high-mid bell gain
 *   10 band2_q        0.1 – 20.0      high-mid bell Q
 *   11 band3_freq   2000 – 20000 Hz   high shelf frequency
 *   12 band3_gain   -24 – +24 dB      high shelf gain
 *   13 output_gain   0.0 – 2.0        post-EQ output level
 */
static void _SituationRegisterISA110(void) {
    SituationDeviceMetadata meta = {0};

    meta.type = SITUATION_NODE_ISA110;
    strncpy(meta.name, "ISA 110", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;

    meta.num_audio_ins  = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins   = 0;
    meta.num_ctrl_outs  = 0;
    meta.num_controls   = 14;

#define _ISA110_CTRL(idx, cname, ctype, lo, hi, def, cunit, clog) \
    do { \
        strncpy(meta.controls[(idx)].name, (cname), SITUATION_MAX_CONTROL_NAME - 1); \
        meta.controls[(idx)].id              = (idx); \
        meta.controls[(idx)].type            = (ctype); \
        meta.controls[(idx)].min_value       = (lo); \
        meta.controls[(idx)].max_value       = (hi); \
        meta.controls[(idx)].default_value   = (def); \
        meta.controls[(idx)].units           = (cunit); \
        meta.controls[(idx)].is_logarithmic  = (clog); \
    } while (0)

    _ISA110_CTRL( 0, "drive",        SITUATION_CONTROL_FLOAT, 0.0f,    10.0f,    1.0f,  NULL,  false);
    _ISA110_CTRL( 1, "hpf_cutoff",   SITUATION_CONTROL_FLOAT, 20.0f,  400.0f,   75.0f, "Hz",  true);
    _ISA110_CTRL( 2, "hpf_enabled",  SITUATION_CONTROL_BOOL,  0.0f,    1.0f,    1.0f,  NULL,  false);
    _ISA110_CTRL( 3, "band0_freq",   SITUATION_CONTROL_FLOAT, 20.0f,  500.0f,   80.0f, "Hz",  true);
    _ISA110_CTRL( 4, "band0_gain",   SITUATION_CONTROL_FLOAT, -24.0f,  24.0f,    0.0f, "dB",  false);
    _ISA110_CTRL( 5, "band1_freq",   SITUATION_CONTROL_FLOAT, 100.0f, 2000.0f, 220.0f, "Hz",  true);
    _ISA110_CTRL( 6, "band1_gain",   SITUATION_CONTROL_FLOAT, -24.0f,  24.0f,    0.0f, "dB",  false);
    _ISA110_CTRL( 7, "band1_q",      SITUATION_CONTROL_FLOAT, 0.1f,   20.0f,    1.4f,  NULL,  false);
    _ISA110_CTRL( 8, "band2_freq",   SITUATION_CONTROL_FLOAT, 500.0f, 10000.0f,2200.0f,"Hz",  true);
    _ISA110_CTRL( 9, "band2_gain",   SITUATION_CONTROL_FLOAT, -24.0f,  24.0f,    0.0f, "dB",  false);
    _ISA110_CTRL(10, "band2_q",      SITUATION_CONTROL_FLOAT, 0.1f,   20.0f,    1.4f,  NULL,  false);
    _ISA110_CTRL(11, "band3_freq",   SITUATION_CONTROL_FLOAT, 2000.0f,20000.0f,12000.0f,"Hz", true);
    _ISA110_CTRL(12, "band3_gain",   SITUATION_CONTROL_FLOAT, -24.0f,  24.0f,    0.0f, "dB",  false);
    _ISA110_CTRL(13, "output_gain",  SITUATION_CONTROL_FLOAT, 0.0f,    2.0f,    1.0f,  NULL,  false);

#undef _ISA110_CTRL

    meta.latency_samples = 0;
    meta.description = "Focusrite ISA 110 preamp + 4-band inductor EQ emulation. "
                       "Transformer saturation, switchable 18dB/oct HPF, "
                       "2 shelves + 2 bells with FMA-optimised coefficient smoothing.";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;

    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Exciter device.
 */
static void _SituationRegisterExciter(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_EXCITER;
    strncpy(meta.name, "Exciter", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 7;
    
    // fc (cutoff)
    strncpy(meta.controls[0].name, "fc", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 1000.0f;
    meta.controls[0].max_value = 20000.0f;
    meta.controls[0].default_value = 5000.0f;
    meta.controls[0].units = "Hz";
    meta.controls[0].is_logarithmic = true;
    
    // drive
    strncpy(meta.controls[1].name, "drive", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 1.0f;
    meta.controls[1].max_value = 50.0f;
    meta.controls[1].default_value = 10.0f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // mix
    strncpy(meta.controls[2].name, "mix", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.5f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    // clip_k (softness)
    strncpy(meta.controls[3].name, "clip_k", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.01f;
    meta.controls[3].max_value = 1.0f;
    meta.controls[3].default_value = 0.5f;
    meta.controls[3].units = NULL;
    meta.controls[3].is_logarithmic = false;
    
    // trans_sens (transient sensitivity)
    strncpy(meta.controls[4].name, "trans_sens", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 5.0f;
    meta.controls[4].default_value = 1.5f;
    meta.controls[4].units = NULL;
    meta.controls[4].is_logarithmic = false;
    
    // trans_attack
    strncpy(meta.controls[5].name, "trans_attack", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_FLOAT;
    meta.controls[5].min_value = 0.001f;
    meta.controls[5].max_value = 0.1f;
    meta.controls[5].default_value = 0.005f;
    meta.controls[5].units = "s";
    meta.controls[5].is_logarithmic = true;
    
    // trans_release
    strncpy(meta.controls[6].name, "trans_release", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_FLOAT;
    meta.controls[6].min_value = 0.01f;
    meta.controls[6].max_value = 1.0f;
    meta.controls[6].default_value = 0.05f;
    meta.controls[6].units = "s";
    meta.controls[6].is_logarithmic = true;
    
    meta.latency_samples = 0;
    meta.description = "Harmonic exciter with transient detection and asymmetric soft clipping";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Dynamics device (Compressor/Limiter/Gate).
 */
static void _SituationRegisterDynamics(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_DYNAMICS;
    strncpy(meta.name, "Dynamics", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 4;     // 2 main + 2 sidechain
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 7;
    
    // threshold_dB
    strncpy(meta.controls[0].name, "threshold_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = -60.0f;
    meta.controls[0].max_value = 0.0f;
    meta.controls[0].default_value = -20.0f;
    meta.controls[0].units = "dB";
    meta.controls[0].is_logarithmic = false;
    
    // ratio
    strncpy(meta.controls[1].name, "ratio", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 1.0f;
    meta.controls[1].max_value = 20.0f;
    meta.controls[1].default_value = 4.0f;
    meta.controls[1].units = ":1";
    meta.controls[1].is_logarithmic = false;
    
    // attack_time
    strncpy(meta.controls[2].name, "attack_time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.001f;
    meta.controls[2].max_value = 0.5f;
    meta.controls[2].default_value = 0.01f;
    meta.controls[2].units = "s";
    meta.controls[2].is_logarithmic = true;
    
    // release_time
    strncpy(meta.controls[3].name, "release_time", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[3].id = 3;
    meta.controls[3].type = SITUATION_CONTROL_FLOAT;
    meta.controls[3].min_value = 0.01f;
    meta.controls[3].max_value = 2.0f;
    meta.controls[3].default_value = 0.1f;
    meta.controls[3].units = "s";
    meta.controls[3].is_logarithmic = true;
    
    // makeup_gain_dB
    strncpy(meta.controls[4].name, "makeup_gain_dB", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[4].id = 4;
    meta.controls[4].type = SITUATION_CONTROL_FLOAT;
    meta.controls[4].min_value = 0.0f;
    meta.controls[4].max_value = 24.0f;
    meta.controls[4].default_value = 0.0f;
    meta.controls[4].units = "dB";
    meta.controls[4].is_logarithmic = false;
    
    // is_gate
    strncpy(meta.controls[5].name, "is_gate", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[5].id = 5;
    meta.controls[5].type = SITUATION_CONTROL_BOOL;
    meta.controls[5].min_value = 0.0f;
    meta.controls[5].max_value = 1.0f;
    meta.controls[5].default_value = 0.0f;
    meta.controls[5].units = NULL;
    meta.controls[5].is_logarithmic = false;
    
    // sidechain_enabled
    strncpy(meta.controls[6].name, "sidechain_enabled", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[6].id = 6;
    meta.controls[6].type = SITUATION_CONTROL_BOOL;
    meta.controls[6].min_value = 0.0f;
    meta.controls[6].max_value = 1.0f;
    meta.controls[6].default_value = 0.0f;
    meta.controls[6].units = NULL;
    meta.controls[6].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Compressor/Limiter/Gate with sidechain support";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the 4-Band EQ device.
 */
static void _SituationRegisterEQ4Band(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_EQ_4BAND;
    strncpy(meta.name, "EQ 4-Band", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // 4 bands: HPF (2 params) + Low Shelf (3) + Peak (3) + High Shelf (3) = 11 controls
    meta.num_controls = 11;
    
    int ctrl_idx = 0;
    
    // High-pass filter
    strncpy(meta.controls[ctrl_idx].name, "hpf_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 20.0f;
    meta.controls[ctrl_idx].max_value = 500.0f;
    meta.controls[ctrl_idx].default_value = 20.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "hpf_enabled", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_BOOL;
    meta.controls[ctrl_idx].min_value = 0.0f;
    meta.controls[ctrl_idx].max_value = 1.0f;
    meta.controls[ctrl_idx].default_value = 0.0f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // Low shelf
    strncpy(meta.controls[ctrl_idx].name, "low_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 20.0f;
    meta.controls[ctrl_idx].max_value = 500.0f;
    meta.controls[ctrl_idx].default_value = 100.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "low_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = -24.0f;
    meta.controls[ctrl_idx].max_value = 24.0f;
    meta.controls[ctrl_idx].default_value = 0.0f;
    meta.controls[ctrl_idx].units = "dB";
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "low_Q", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.1f;
    meta.controls[ctrl_idx].max_value = 10.0f;
    meta.controls[ctrl_idx].default_value = 0.707f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // Peaking (mid)
    strncpy(meta.controls[ctrl_idx].name, "peak_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 200.0f;
    meta.controls[ctrl_idx].max_value = 8000.0f;
    meta.controls[ctrl_idx].default_value = 1000.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "peak_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = -24.0f;
    meta.controls[ctrl_idx].max_value = 24.0f;
    meta.controls[ctrl_idx].default_value = 0.0f;
    meta.controls[ctrl_idx].units = "dB";
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "peak_Q", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.1f;
    meta.controls[ctrl_idx].max_value = 10.0f;
    meta.controls[ctrl_idx].default_value = 1.0f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    // High shelf
    strncpy(meta.controls[ctrl_idx].name, "high_freq", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 2000.0f;
    meta.controls[ctrl_idx].max_value = 20000.0f;
    meta.controls[ctrl_idx].default_value = 10000.0f;
    meta.controls[ctrl_idx].units = "Hz";
    meta.controls[ctrl_idx].is_logarithmic = true;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "high_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = -24.0f;
    meta.controls[ctrl_idx].max_value = 24.0f;
    meta.controls[ctrl_idx].default_value = 0.0f;
    meta.controls[ctrl_idx].units = "dB";
    meta.controls[ctrl_idx].is_logarithmic = false;
    ctrl_idx++;
    
    strncpy(meta.controls[ctrl_idx].name, "high_Q", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[ctrl_idx].id = ctrl_idx;
    meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
    meta.controls[ctrl_idx].min_value = 0.1f;
    meta.controls[ctrl_idx].max_value = 10.0f;
    meta.controls[ctrl_idx].default_value = 0.707f;
    meta.controls[ctrl_idx].units = NULL;
    meta.controls[ctrl_idx].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "4-band parametric EQ with high-pass, low shelf, peaking, and high shelf";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Filter device.
 */
static void _SituationRegisterFilter(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_FILTER;
    strncpy(meta.name, "Filter", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 3;
    
    // cutoff
    strncpy(meta.controls[0].name, "cutoff", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 20.0f;
    meta.controls[0].max_value = 20000.0f;
    meta.controls[0].default_value = 1000.0f;
    meta.controls[0].units = "Hz";
    meta.controls[0].is_logarithmic = true;
    
    // resonance
    strncpy(meta.controls[1].name, "resonance", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.1f;
    meta.controls[1].max_value = 10.0f;
    meta.controls[1].default_value = 0.707f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // type (enum)
    strncpy(meta.controls[2].name, "type", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_ENUM;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 2.0f;
    meta.controls[2].default_value = 0.0f;
    meta.controls[2].enum_count = 3;
    static const char* filter_type_labels[] = {"Lowpass", "Highpass", "Bandpass", NULL};
    meta.controls[2].enum_labels = filter_type_labels;
    
    meta.latency_samples = 0;
    meta.description = "Biquad filter with lowpass, highpass, and bandpass modes";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Sound Source device.
 */
static void _SituationRegisterSoundSource(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_SOUND_SOURCE;
    strncpy(meta.name, "Sound Source", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_SOURCE;
    
    meta.num_audio_ins = 0;     // Pure source
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 3;
    
    // volume
    strncpy(meta.controls[0].name, "volume", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 2.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // pitch
    strncpy(meta.controls[1].name, "pitch", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.5f;
    meta.controls[1].max_value = 2.0f;
    meta.controls[1].default_value = 1.0f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // play_state
    strncpy(meta.controls[2].name, "play_state", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_BOOL;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 1.0f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Sample playback source with volume and pitch control";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Mic Capture device.
 */
static void _SituationRegisterMicCapture(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_MIC_CAPTURE;
    strncpy(meta.name, "Mic Capture", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_CAPTURE;
    
    meta.num_audio_ins = 0;     // Pure capture
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 1;
    
    // gain
    strncpy(meta.controls[0].name, "gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 2.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Microphone/line input capture with gain control";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the PCM Input device (user-fed ring buffer source).
 */
static void _SituationRegisterPCMInput(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_PCM_INPUT;
    strncpy(meta.name, "PCM Input", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_SOURCE;
    
    meta.num_audio_ins = 0;     // Pure source (reads from ring buffer)
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 3;
    
    // gain
    strncpy(meta.controls[0].name, "gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 2.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    // pan
    strncpy(meta.controls[1].name, "pan", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = -1.0f;
    meta.controls[1].max_value = 1.0f;
    meta.controls[1].default_value = 0.0f;
    meta.controls[1].units = NULL;
    meta.controls[1].is_logarithmic = false;
    
    // mute
    strncpy(meta.controls[2].name, "mute", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_BOOL;
    meta.controls[2].min_value = 0.0f;
    meta.controls[2].max_value = 1.0f;
    meta.controls[2].default_value = 0.0f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "User-fed PCM source with lock-free ring buffer (any-thread push)";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

/**
 * @brief Register the Compander device (3-band multiband compander with EQ).
 */
static void _SituationRegisterCompander(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_COMPANDER;
    strncpy(meta.name, "Compander", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_EFFECT;
    
    meta.num_audio_ins = 2;
    meta.num_audio_outs = 2;
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    // 3 bands × 8 params (5 compander + 3 bell EQ) = 24 controls
    meta.num_controls = 24;
    
    int ctrl_idx = 0;
    
    // Per-band controls (3 bands: low, mid, high)
    const char* band_names[] = {"low", "mid", "high"};
    for (int band = 0; band < 3; band++) {
        char name_buf[SITUATION_MAX_CONTROL_NAME];
        
        // loud_threshold
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_loud_threshold", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.0f;
        meta.controls[ctrl_idx].max_value = 1.0f;
        meta.controls[ctrl_idx].default_value = 0.5f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // quiet_threshold
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_quiet_threshold", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.0f;
        meta.controls[ctrl_idx].max_value = 1.0f;
        meta.controls[ctrl_idx].default_value = 0.1f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // comp_slope
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_comp_slope", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.1f;
        meta.controls[ctrl_idx].max_value = 1.0f;
        meta.controls[ctrl_idx].default_value = 0.5f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // exp_slope
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_exp_slope", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 1.0f;
        meta.controls[ctrl_idx].max_value = 5.0f;
        meta.controls[ctrl_idx].default_value = 2.0f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // noise_gate
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_noise_gate", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = -90.0f;
        meta.controls[ctrl_idx].max_value = 0.0f;
        meta.controls[ctrl_idx].default_value = -60.0f;
        meta.controls[ctrl_idx].units = "dB";
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // bell_center_freq
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_bell_freq", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 20.0f;
        meta.controls[ctrl_idx].max_value = 20000.0f;
        meta.controls[ctrl_idx].default_value = (band == 0) ? 100.0f : (band == 1) ? 1000.0f : 8000.0f;
        meta.controls[ctrl_idx].units = "Hz";
        meta.controls[ctrl_idx].is_logarithmic = true;
        ctrl_idx++;
        
        // bell_gain
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_bell_gain", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = -12.0f;
        meta.controls[ctrl_idx].max_value = 12.0f;
        meta.controls[ctrl_idx].default_value = 0.0f;
        meta.controls[ctrl_idx].units = "dB";
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
        
        // bell_Q
        snprintf(name_buf, SITUATION_MAX_CONTROL_NAME, "%s_bell_Q", band_names[band]);
        strncpy(meta.controls[ctrl_idx].name, name_buf, SITUATION_MAX_CONTROL_NAME - 1);
        meta.controls[ctrl_idx].id = ctrl_idx;
        meta.controls[ctrl_idx].type = SITUATION_CONTROL_FLOAT;
        meta.controls[ctrl_idx].min_value = 0.1f;
        meta.controls[ctrl_idx].max_value = 10.0f;
        meta.controls[ctrl_idx].default_value = 1.0f;
        meta.controls[ctrl_idx].units = NULL;
        meta.controls[ctrl_idx].is_logarithmic = false;
        ctrl_idx++;
    }
    
    meta.latency_samples = 0;
    meta.description = "Three-band multiband compander with per-band bell curve EQ and noise gate";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// LFO REGISTRATION
// ================================================================================================

/**
 * @brief Register the LFO (Low Frequency Oscillator) modulator device.
 */
static void _SituationRegisterLFO(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_LFO;
    strncpy(meta.name, "LFO", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_MODULATOR;
    
    meta.num_audio_ins = 0;     // No audio input (pure generator)
    meta.num_audio_outs = 1;    // Control signal output (as audio buffer)
    meta.audio_channels = 1;    // Mono control signal
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 1;     // Control output port
    
    meta.num_controls = 2;
    
    // Control 0: waveform (enum)
    strncpy(meta.controls[0].name, "waveform", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_ENUM;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 4.0f;
    meta.controls[0].default_value = 0.0f;
    meta.controls[0].enum_count = 5;
    static const char* lfo_waveform_labels[] = {"Sine", "Triangle", "Square", "Saw Up", "Saw Down", NULL};
    meta.controls[0].enum_labels = lfo_waveform_labels;
    
    // Control 1: frequency
    strncpy(meta.controls[1].name, "frequency", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.01f;
    meta.controls[1].max_value = 20.0f;
    meta.controls[1].default_value = 1.0f;
    meta.controls[1].units = "Hz";
    meta.controls[1].is_logarithmic = true;
    
    meta.latency_samples = 0;
    meta.description = "Low frequency oscillator for parameter modulation";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// GAIN REGISTRATION
// ================================================================================================

/**
 * @brief Register the Gain utility device.
 */
static void _SituationRegisterGain(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_GAIN;
    strncpy(meta.name, "Gain", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_UTILITY;
    
    meta.num_audio_ins = 2;     // Stereo in
    meta.num_audio_outs = 2;    // Stereo out
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 1;
    
    // Control 0: gain
    strncpy(meta.controls[0].name, "gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 4.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Simple gain stage with click-free smoothing";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// MIXER NODE REGISTRATION
// ================================================================================================

/**
 * @brief Register the Mixer (bus summing) utility device.
 */
static void _SituationRegisterMixer(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_MIXER;
    strncpy(meta.name, "Mixer", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_UTILITY;
    
    meta.num_audio_ins = 16;    // Up to 16 stereo inputs
    meta.num_audio_outs = 2;    // Stereo out (summed)
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 1;
    
    // Control 0: master_gain
    strncpy(meta.controls[0].name, "master_gain", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.0f;
    meta.controls[0].max_value = 4.0f;
    meta.controls[0].default_value = 1.0f;
    meta.controls[0].units = NULL;
    meta.controls[0].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Bus summing mixer — sums up to 16 stereo inputs to one stereo output";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// ENVELOPE FOLLOWER REGISTRATION
// ================================================================================================

/**
 * @brief Register the Envelope Follower modulator device.
 */
static void _SituationRegisterEnvelopeFollower(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_ENVELOPE_FOLLOWER;
    strncpy(meta.name, "Envelope Follower", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_MODULATOR;
    
    meta.num_audio_ins = 2;     // Stereo input (tap)
    meta.num_audio_outs = 1;    // Control signal output (mono)
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 1;     // Envelope value output
    
    meta.num_controls = 3;
    
    // Control 0: attack
    strncpy(meta.controls[0].name, "attack", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_FLOAT;
    meta.controls[0].min_value = 0.001f;
    meta.controls[0].max_value = 0.5f;
    meta.controls[0].default_value = 0.01f;
    meta.controls[0].units = "s";
    meta.controls[0].is_logarithmic = true;
    
    // Control 1: release
    strncpy(meta.controls[1].name, "release", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[1].id = 1;
    meta.controls[1].type = SITUATION_CONTROL_FLOAT;
    meta.controls[1].min_value = 0.01f;
    meta.controls[1].max_value = 2.0f;
    meta.controls[1].default_value = 0.1f;
    meta.controls[1].units = "s";
    meta.controls[1].is_logarithmic = true;
    
    // Control 2: sensitivity
    strncpy(meta.controls[2].name, "sensitivity", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[2].id = 2;
    meta.controls[2].type = SITUATION_CONTROL_FLOAT;
    meta.controls[2].min_value = 0.1f;
    meta.controls[2].max_value = 10.0f;
    meta.controls[2].default_value = 1.0f;
    meta.controls[2].units = NULL;
    meta.controls[2].is_logarithmic = false;
    
    meta.latency_samples = 0;
    meta.description = "Envelope follower — outputs control signal tracking input amplitude";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// PEAK METER REGISTRATION
// ================================================================================================

/**
 * @brief Register the Peak Meter analyzer device.
 */
static void _SituationRegisterPeakMeter(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_PEAK_METER;
    strncpy(meta.name, "Peak Meter", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_ANALYZER;
    
    meta.num_audio_ins = 2;     // Stereo in (passthrough)
    meta.num_audio_outs = 2;    // Stereo out (passthrough)
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 0;      // Read-only — no user controls
    
    meta.latency_samples = 0;
    meta.description = "Peak and RMS level meter with ballistic decay (passthrough)";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// SPECTRUM ANALYZER REGISTRATION
// ================================================================================================

/**
 * @brief Register the Spectrum Analyzer device.
 */
static void _SituationRegisterSpectrumAnalyzer(void) {
    SituationDeviceMetadata meta = {0};
    
    meta.type = SITUATION_NODE_SPECTRUM_ANALYZER;
    strncpy(meta.name, "Spectrum Analyzer", SITUATION_MAX_DEVICE_NAME - 1);
    meta.category = SITUATION_DEVICE_ANALYZER;
    
    meta.num_audio_ins = 2;     // Stereo in (passthrough)
    meta.num_audio_outs = 2;    // Stereo out (passthrough)
    meta.audio_channels = 2;
    meta.num_ctrl_ins = 0;
    meta.num_ctrl_outs = 0;
    
    meta.num_controls = 1;
    
    // Control 0: fft_size
    strncpy(meta.controls[0].name, "fft_size", SITUATION_MAX_CONTROL_NAME - 1);
    meta.controls[0].id = 0;
    meta.controls[0].type = SITUATION_CONTROL_ENUM;
    meta.controls[0].min_value = 256.0f;
    meta.controls[0].max_value = 1024.0f;
    meta.controls[0].default_value = 512.0f;
    meta.controls[0].enum_count = 3;
    static const char* fft_size_labels[] = {"256", "512", "1024", NULL};
    meta.controls[0].enum_labels = fft_size_labels;
    
    meta.latency_samples = 0;
    meta.description = "FFT-based spectrum analyzer for frequency display (passthrough)";
    meta.author = "Situation Audio";
    meta.version = 0x00010000;
    
    SituationRegisterDeviceType(&meta);
}

// ================================================================================================
// MASTER INITIALIZATION FUNCTION
// ================================================================================================

/**
 * @brief Initialize the device registry with all built-in devices.
 * @details Call this once during audio subsystem initialization.
 *          Registers all 18 built-in audio devices.
 * 
 * @note This function is NOT thread-safe. Call during init only.
 */
void SituationInitDeviceRegistry(void) {
    static bool registry_populated = false;
    if (registry_populated) {
        return;
    }

    // Effects (16 devices)
    _SituationRegisterReverb();
    _SituationRegisterEcho();
    _SituationRegisterChorus();
    _SituationRegisterPhaser();
    _SituationRegisterOverdrive();
    _SituationRegisterExciter();
    _SituationRegisterMaximizer();
    _SituationRegisterSpringReverb();
    _SituationRegisterStudioReverb();
    _SituationRegisterSST282();
    _SituationRegisterMasteringAmp();
    _SituationRegisterDeafMax();
    _SituationRegisterISA110();
    _SituationRegisterDynamics();
    _SituationRegisterCompander();
    _SituationRegisterEQ4Band();
    _SituationRegisterFilter();
    
    // Sources (3 devices)
    _SituationRegisterToneSynth();
    _SituationRegisterSoundSource();
    _SituationRegisterPCMInput();
    
    // Capture (1 device)
    _SituationRegisterMicCapture();
    
    // Utilities (3 devices)
    _SituationRegisterPanner();
    _SituationRegisterGain();
    _SituationRegisterMixer();
    
    // Modulators (1 device)
    _SituationRegisterLFO();
    _SituationRegisterEnvelopeFollower();
    
    // Analyzers (2 devices)
    _SituationRegisterPeakMeter();
    _SituationRegisterSpectrumAnalyzer();

    registry_populated = true;
    
    // Debug: Print registry contents
    #ifdef SITUATION_DEBUG_REGISTRY
    int count = SituationGetRegisteredDeviceCount();
    printf("[Registry] Initialized with %d devices:\n", count);
    for (int i = 0; i < count; i++) {
        SituationDeviceMetadata meta;
        if (SituationGetDeviceMetadataByIndex(i, &meta) == SITUATION_SUCCESS) {
            printf("  [%d] %s (%s) - %d ins, %d outs, %d controls\n",
                   i, meta.name, SituationGetCategoryName(meta.category),
                   meta.num_audio_ins, meta.num_audio_outs, meta.num_controls);
        }
    }
    #endif
}

/**
 * @brief Demo function showing registry query usage.
 * @details Example code demonstrating how to query the registry.
 */
static void SituationRegistryDemo(void) {
    // Query Reverb metadata
    SituationDeviceMetadata reverb_meta;
    if (SituationGetDeviceMetadata(SITUATION_NODE_REVERB, &reverb_meta) == SITUATION_SUCCESS) {
        printf("Found device: %s\n", reverb_meta.name);
        printf("  Category: %s\n", SituationGetCategoryName(reverb_meta.category));
        printf("  Audio: %d ins, %d outs (%d channels)\n",
               reverb_meta.num_audio_ins, reverb_meta.num_audio_outs, reverb_meta.audio_channels);
        printf("  Controls: %d\n", reverb_meta.num_controls);
        
        for (int i = 0; i < reverb_meta.num_controls; i++) {
            const SituationControlDesc* ctrl = &reverb_meta.controls[i];
            printf("    [%d] %s (%s): %.2f to %.2f (default: %.2f)\n",
                   ctrl->id, ctrl->name, SituationGetControlTypeName(ctrl->type),
                   ctrl->min_value, ctrl->max_value, ctrl->default_value);
        }
    }
    
    // Check if device is registered
    if (SituationIsDeviceRegistered(SITUATION_NODE_TONE_SYNTH)) {
        printf("Tone Synth is registered!\n");
    }
    
    // Iterate all devices
    printf("\nAll registered devices:\n");
    int count = SituationGetRegisteredDeviceCount();
    for (int i = 0; i < count; i++) {
        SituationDeviceMetadata meta;
        if (SituationGetDeviceMetadataByIndex(i, &meta) == SITUATION_SUCCESS) {
            printf("  %s\n", meta.name);
        }
    }
}

#endif // SITUATION_REGISTRY_INIT_H

