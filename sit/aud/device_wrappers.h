/***************************************************************************************************
*
*   sit/aud/device_wrappers.h - Device Wrapper Functions for Node Graph
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 4: Device wrapper functions that connect existing device implementations to the
*   node graph system. Each device gets create/process/destroy functions that match the
*   SituationDeviceFunctions interface.
*   
*   This file provides the glue layer between the node graph and the actual DSP implementations.
*   
***************************************************************************************************/

#ifndef SITUATION_DEVICE_WRAPPERS_H
#define SITUATION_DEVICE_WRAPPERS_H

#include "node_graph_process.h"
#include "fx/reverb.h"
#include "fx/echo.h"
#include "fx/chorus_4stage.h"
#include "fx/phaseshifter.h"

// Define implementation guards for header-only libraries
#define SIT_OVERDRIVE_IMPLEMENTATION
#include "fx/overdrive.h"

#include "fx/exciter.h"
#include "fx/maximizer.h"      // FFTW3-based spectral maximizer (Windows-compatible)
#include "fx/studio_reverb.h"
#include "fx/spring_reverb.h"
#include "fx/sst282.h"
#include "fx/mastering_amp.h"  // SSE-optimized mastering processor

#define SIT_DEAFMAX_IMPLEMENTATION
#include "fx/deafmax.h"        // Zero-alloc surgical peak maximizer

// New simple implementations
#include "fx/filter.h"
#include "fx/eq_4band.h"
#include "fx/dynamics.h"
#include "fx/compander.h"
#include "fx/lfo.h"
#include "sound_source.h"
#include "mic_capture.h"

// ================================================================================================
// REVERB WRAPPER
// ================================================================================================

/**
 * @brief Create reverb device state.
 * @param metadata Device metadata from registry.
 * @return Opaque pointer to SituationReverbState.
 */
static void* _SituationCreateReverb(const SituationDeviceMetadata* metadata) {
    (void)metadata;  // Unused for now
    
    // Initialize reverb at 48kHz (should get from audio context in real impl)
    return _SituationInitReverb(48000);
}

/**
 * @brief Process audio through reverb.
 * @param device_data Opaque pointer to SituationReverbState.
 * @param inputs Array of input audio ports.
 * @param outputs Array of output audio ports.
 * @param controls Array of control values.
 * @param frames Number of frames to process.
 */
static void _SituationProcessReverbNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationReverbState* rev = (SituationReverbState*)device_data;
    
    // Update parameters from controls
    // Control indices from registry: 0=room_size, 1=damp, 2=wet, 3=dry, 4=width
    if (controls) {
        rev->room_size = controls[0];
        rev->damp = controls[1];
        rev->wet = controls[2];
        rev->dry = controls[3];
        rev->width = controls[4];
    }
    
    // Process audio
    _SituationProcessReverb(
        device_data,
        outputs[0].buffer,
        inputs[0].buffer,
        (uint32_t)frames,
        inputs[0].channels
    );
}

/**
 * @brief Destroy reverb device state.
 * @param device_data Opaque pointer to SituationReverbState.
 */
static void _SituationDestroyReverb(void* device_data) {
    _SituationUninitReverb(device_data);
}

// ================================================================================================
// ECHO WRAPPER
// ================================================================================================

/**
 * @brief Echo device state wrapper.
 */
typedef struct {
    ma_delay delay;
    bool is_initialized;
    uint32_t sample_rate;
    uint32_t channels;
} SituationEchoNodeState;

/**
 * @brief Create echo device state.
 * @param metadata Device metadata from registry.
 * @return Opaque pointer to SituationEchoNodeState.
 */
static void* _SituationCreateEcho(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationEchoNodeState* state = (SituationEchoNodeState*)SIT_CALLOC(1, sizeof(SituationEchoNodeState));
    if (!state) return NULL;
    
    state->sample_rate = 48000;  // Should get from audio context
    state->channels = 2;         // Stereo
    state->is_initialized = false;
    
    return state;
}

/**
 * @brief Process audio through echo.
 * @param device_data Opaque pointer to SituationEchoNodeState.
 * @param inputs Array of input audio ports.
 * @param outputs Array of output audio ports.
 * @param controls Array of control values.
 * @param frames Number of frames to process.
 */
static void _SituationProcessEchoNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationEchoNodeState* state = (SituationEchoNodeState*)device_data;
    
    // Update parameters from controls
    // Control indices: 0=delay_time, 1=feedback, 2=wet_level
    float delay_time = controls ? controls[0] : 0.5f;
    float feedback = controls ? controls[1] : 0.5f;
    float wet = controls ? controls[2] : 0.5f;
    float dry = 1.0f - wet;  // Complementary dry level
    
    // Configure echo (lazy init on first process)
    _SituationConfigEcho(
        &state->delay,
        &state->is_initialized,
        state->sample_rate,
        state->channels,
        delay_time,
        feedback,
        wet,
        dry
    );
    
    // Copy input to output first
    memcpy(outputs[0].buffer, inputs[0].buffer, frames * inputs[0].channels * sizeof(float));
    
    // Process echo in-place
    _SituationProcessEcho(&state->delay, outputs[0].buffer, (uint32_t)frames);
}

/**
 * @brief Destroy echo device state.
 * @param device_data Opaque pointer to SituationEchoNodeState.
 */
static void _SituationDestroyEcho(void* device_data) {
    if (!device_data) return;
    
    SituationEchoNodeState* state = (SituationEchoNodeState*)device_data;
    
    if (state->is_initialized) {
        _SituationUninitEcho(&state->delay);
    }
    
    SIT_FREE(state);
}

// ================================================================================================
// TONE SYNTH WRAPPER (Stub - needs full implementation)
// ================================================================================================

/**
 * @brief Tone synth device state (stub).
 */
typedef struct {
    float frequency;
    float amplitude;
    float phase;
    int waveform;  // 0=sine, 1=square, 2=saw, 3=triangle
} SituationToneSynthNodeState;

/**
 * @brief Create tone synth device state.
 * @param metadata Device metadata from registry.
 * @return Opaque pointer to SituationToneSynthNodeState.
 */
static void* _SituationCreateToneSynth(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationToneSynthNodeState* state = (SituationToneSynthNodeState*)SIT_CALLOC(1, sizeof(SituationToneSynthNodeState));
    if (!state) return NULL;
    
    state->frequency = 440.0f;  // A4
    state->amplitude = 0.3f;
    state->phase = 0.0f;
    state->waveform = 0;  // Sine
    
    return state;
}

/**
 * @brief Process audio through tone synth (simple sine wave generator).
 * @param device_data Opaque pointer to SituationToneSynthNodeState.
 * @param inputs Array of input audio ports (unused - pure source).
 * @param outputs Array of output audio ports.
 * @param controls Array of control values.
 * @param frames Number of frames to process.
 */
static void _SituationProcessToneSynthNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)inputs;  // Pure source - no inputs
    
    if (!device_data || !outputs) return;
    
    SituationToneSynthNodeState* state = (SituationToneSynthNodeState*)device_data;
    
    // Update parameters from controls
    // Control indices: 0=frequency, 1=amplitude, 2=waveform, ...
    if (controls) {
        state->frequency = controls[0];
        state->amplitude = controls[1];
        state->waveform = (int)controls[2];
    }
    
    float* buffer = outputs[0].buffer;
    int channels = outputs[0].channels;
    float sample_rate = 48000.0f;  // Should get from audio context
    float phase_increment = (2.0f * 3.14159265359f * state->frequency) / sample_rate;
    
    // Generate waveform
    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;
        
        switch (state->waveform) {
            case 0:  // Sine
                sample = sinf(state->phase) * state->amplitude;
                break;
            case 1:  // Square
                sample = (state->phase < 3.14159265359f ? 1.0f : -1.0f) * state->amplitude;
                break;
            case 2:  // Sawtooth
                sample = ((state->phase / 3.14159265359f) - 1.0f) * state->amplitude;
                break;
            case 3:  // Triangle
                sample = (2.0f * fabsf((state->phase / 3.14159265359f) - 1.0f) - 1.0f) * state->amplitude;
                break;
        }
        
        // Write to all channels (mono source)
        for (int c = 0; c < channels; c++) {
            buffer[i * channels + c] = sample;
        }
        
        // Advance phase
        state->phase += phase_increment;
        if (state->phase >= 2.0f * 3.14159265359f) {
            state->phase -= 2.0f * 3.14159265359f;
        }
    }
}

/**
 * @brief Destroy tone synth device state.
 * @param device_data Opaque pointer to SituationToneSynthNodeState.
 */
static void _SituationDestroyToneSynth(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// PASSTHROUGH WRAPPER (for testing)
// ================================================================================================

/**
 * @brief Process audio passthrough (copy input to output).
 */
static void _SituationProcessPassthrough(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)device_data;
    (void)controls;
    
    if (!inputs || !outputs) return;
    
    float gain = 1.0f;
    if (controls) gain = controls[0];
    
    int samples = frames * inputs[0].channels;
    for (int i = 0; i < samples; i++) {
        outputs[0].buffer[i] = inputs[0].buffer[i] * gain;
    }
}

// ================================================================================================
// CHORUS WRAPPER
// ================================================================================================

/**
 * @brief Create chorus device state.
 */
static void* _SituationCreateChorus(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationChorus4Stage* chorus = (SituationChorus4Stage*)SIT_CALLOC(1, sizeof(SituationChorus4Stage));
    if (!chorus) return NULL;
    
    // Initialize with 48kHz sample rate and 100ms max delay
    int max_delay_samples = (int)(48000 * 0.1f);  // 100ms at 48kHz
    SituationChorus4Stage_Init(chorus, 48000, max_delay_samples);
    
    return chorus;
}

/**
 * @brief Process audio through chorus.
 */
static void _SituationProcessChorusNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationChorus4Stage* chorus = (SituationChorus4Stage*)device_data;
    
    // Update parameters from controls (21 controls total)
    if (controls) {
        // Set stage parameters (4 stages × 4 params each)
        for (int stage = 0; stage < 4; stage++) {
            int base = stage * 4;
            SituationChorus4Stage_SetStageParams(
                chorus, stage,
                controls[base + 0],  // base_delay_ms
                controls[base + 1],  // lfo_freq
                controls[base + 2],  // lfo_depth_ms
                controls[base + 3]   // pan
            );
        }
        
        // Set global parameters
        SituationChorus4Stage_SetDryGain(chorus, controls[16]);
        SituationChorus4Stage_SetWetGain(chorus, controls[17]);
        SituationChorus4Stage_SetFeedback(chorus, controls[18]);
        SituationChorus4Stage_SetStereoEnhance(chorus, controls[19]);
        SituationChorus4Stage_SetWidth(chorus, controls[20]);
    }
    
    // Process audio (interleaved stereo)
    float* in = inputs[0].buffer;
    float* out = outputs[0].buffer;
    
    // Deinterleave, process, reinterleave
    for (int i = 0; i < frames; i++) {
        float in_l = in[i * 2];
        float in_r = in[i * 2 + 1];
        float out_l, out_r;
        
        SituationChorus4Stage_Process(chorus, &in_l, &in_r, &out_l, &out_r, 1);
        
        out[i * 2] = out_l;
        out[i * 2 + 1] = out_r;
    }
}

/**
 * @brief Destroy chorus device state.
 */
static void _SituationDestroyChorus(void* device_data) {
    if (device_data) {
        SituationChorus4Stage_Free((SituationChorus4Stage*)device_data);
    }
}

// ================================================================================================
// PHASER WRAPPER
// ================================================================================================

/**
 * @brief Create phaser device state.
 */
static void* _SituationCreatePhaser(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    PhaseShifter* phaser = (PhaseShifter*)SIT_CALLOC(1, sizeof(PhaseShifter));
    if (!phaser) return NULL;
    
    // Initialize with default parameters
    initPhaseShifter(phaser, 48000, 0.5f, 0.7f, 0.5f, 0.0f, 1.0f, 5.0f);
    
    return phaser;
}

/**
 * @brief Process audio through phaser.
 */
static void _SituationProcessPhaserNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    PhaseShifter* phaser = (PhaseShifter*)device_data;
    
    // Update parameters from controls
    // 0=rate, 1=depth, 2=feedback, 3=stages, 4=stereo_phase, 5=mix
    if (controls) {
        setRate(phaser, controls[0]);
        // Note: depth, stages, stereo_phase not directly settable in this API
        setFeedback(phaser, controls[2]);
        setMix(phaser, controls[5]);
    }
    
    // Copy input to output
    memcpy(outputs[0].buffer, inputs[0].buffer, frames * inputs[0].channels * sizeof(float));
    
    // Process in-place
    processBuffer(phaser, outputs[0].buffer, frames, inputs[0].channels);
}

/**
 * @brief Destroy phaser device state.
 */
static void _SituationDestroyPhaser(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// OVERDRIVE WRAPPER
// ================================================================================================

/**
 * @brief Create overdrive device state.
 */
static void* _SituationCreateOverdrive(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    sit_overdrive* overdrive = (sit_overdrive*)SIT_CALLOC(1, sizeof(sit_overdrive));
    if (!overdrive) return NULL;
    
    sit_overdrive_init(overdrive, 48000.0f);
    
    return overdrive;
}

/**
 * @brief Process audio through overdrive.
 */
static void _SituationProcessOverdriveNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    sit_overdrive* overdrive = (sit_overdrive*)device_data;
    
    // Update parameters from controls
    // 0=drive, 1=input_gain, 2=output_gain, 3=mix, 4=mode, 5=filter_cutoff, 
    // 6=filter_res, 7=low_shelf_db, 8=high_shelf_db, 9=asymmetry
    if (controls) {
        sit_overdrive_set_drive(overdrive, controls[0]);
        sit_overdrive_set_input_gain(overdrive, controls[1]);
        sit_overdrive_set_output_gain(overdrive, controls[2]);
        sit_overdrive_set_mix(overdrive, controls[3]);
        sit_overdrive_set_mode(overdrive, (sit_overdrive_mode)((int)controls[4]));
        sit_overdrive_set_filter_cutoff(overdrive, controls[5]);
        sit_overdrive_set_filter_res(overdrive, controls[6]);
        sit_overdrive_set_low_shelf(overdrive, controls[7]);
        sit_overdrive_set_high_shelf(overdrive, controls[8]);
        sit_overdrive_set_asymmetry(overdrive, controls[9]);
    }
    
    // Process audio
    sit_overdrive_process(overdrive, inputs[0].buffer, outputs[0].buffer, frames);
}

/**
 * @brief Destroy overdrive device state.
 */
static void _SituationDestroyOverdrive(void* device_data) {
    if (device_data) {
        sit_overdrive* overdrive = (sit_overdrive*)device_data;
        sit_overdrive_reset(overdrive);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// EXCITER WRAPPER
// ================================================================================================

/**
 * @brief Create exciter device state.
 */
static void* _SituationCreateExciter(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    ExciterState* exciter = (ExciterState*)SIT_CALLOC(1, sizeof(ExciterState));
    if (!exciter) return NULL;
    
    init_exciter(exciter, NULL, 48000.0f);  // NULL = use default params
    
    return exciter;
}

/**
 * @brief Process audio through exciter.
 */
static void _SituationProcessExciterNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    ExciterState* exciter = (ExciterState*)device_data;
    
    // Update parameters from controls
    // 0=cutoff, 1=drive, 2=mix, 3=clip_softness, 4=trans_sensitivity, 5=trans_attack, 6=trans_release
    if (controls) {
        set_cutoff(exciter, controls[0]);
        set_drive(exciter, controls[1]);
        set_mix(exciter, controls[2]);
        set_clip_softness(exciter, controls[3]);
        set_trans_sensitivity(exciter, controls[4]);
        set_trans_attack(exciter, controls[5]);
        set_trans_release(exciter, controls[6]);
    }
    
    // Process audio
    process_exciter(exciter, inputs[0].buffer, outputs[0].buffer, frames, inputs[0].channels);
}

/**
 * @brief Destroy exciter device state.
 */
static void _SituationDestroyExciter(void* device_data) {
    if (device_data) {
        ExciterState* exciter = (ExciterState*)device_data;
        deinit_exciter(exciter);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// PANNER WRAPPER
// ================================================================================================

/**
 * @brief Panner device state (simple stereo panner).
 */
typedef struct {
    float pan_position;  // -1.0 (left) to 1.0 (right)
} SituationPannerNodeState;

/**
 * @brief Create panner device state.
 */
static void* _SituationCreatePanner(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationPannerNodeState* state = (SituationPannerNodeState*)SIT_CALLOC(1, sizeof(SituationPannerNodeState));
    if (!state) return NULL;
    
    state->pan_position = 0.0f;  // Center
    return state;
}

/**
 * @brief Process audio through panner.
 */
static void _SituationProcessPannerNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationPannerNodeState* state = (SituationPannerNodeState*)device_data;
    
    // Update pan position from controls
    if (controls) {
        state->pan_position = controls[0];  // -1.0 to 1.0
    }
    
    // Clamp pan position
    if (state->pan_position < -1.0f) state->pan_position = -1.0f;
    if (state->pan_position > 1.0f) state->pan_position = 1.0f;
    
    // Calculate pan gains (constant power panning)
    float angle = (state->pan_position + 1.0f) * 0.25f * 3.14159265359f;  // 0 to pi/2
    float left_gain = cosf(angle);
    float right_gain = sinf(angle);
    
    float* in = inputs[0].buffer;
    float* out = outputs[0].buffer;
    int channels = inputs[0].channels;
    
    if (channels == 2) {
        // Stereo input
        for (int i = 0; i < frames; i++) {
            float in_l = in[i * 2];
            float in_r = in[i * 2 + 1];
            float mono = (in_l + in_r) * 0.5f;  // Downmix to mono first
            
            out[i * 2] = mono * left_gain;
            out[i * 2 + 1] = mono * right_gain;
        }
    } else {
        // Mono input
        for (int i = 0; i < frames; i++) {
            float mono = in[i];
            out[i * 2] = mono * left_gain;
            out[i * 2 + 1] = mono * right_gain;
        }
    }
}

/**
 * @brief Destroy panner device state.
 */
static void _SituationDestroyPanner(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// STUDIO REVERB WRAPPER
// ================================================================================================

/**
 * @brief Create studio reverb device state.
 */
static void* _SituationCreateStudioReverb(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationStudioReverb* reverb = _SituationStudioReverbCreate(48000);
    return reverb;
}

/**
 * @brief Process audio through studio reverb.
 */
static void _SituationProcessStudioReverbNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationStudioReverb* reverb = (SituationStudioReverb*)device_data;
    
    // Update parameters from controls
    // 0=size, 1=decay_time, 2=bass_coef, 3=treble_coef, 4=pre_delay_ms, 
    // 5=reverb_atten_db, 6=stereo_discorrelator, 7=diffusion_db, 8=wet_mix
    if (controls) {
        SituationStudioReverbParams params = reverb->params;
        params.size = (int)controls[0];
        params.decay_time = controls[1];
        params.bass_coef = controls[2];
        params.treble_coef = controls[3];
        params.pre_delay_ms = controls[4];
        params.reverb_atten_db = controls[5];
        params.stereo_discorrelator = controls[6];
        params.diffusion_db = controls[7];
        params.wet_mix = controls[8];
        _SituationStudioReverbSetParams(reverb, params);
    }
    
    // Process audio (stereo)
    float* in_l = inputs[0].buffer;
    float* in_r = inputs[0].channels == 2 ? inputs[0].buffer + 1 : inputs[0].buffer;
    float* out_l = outputs[0].buffer;
    float* out_r = outputs[0].buffer + 1;
    
    _SituationStudioReverbProcess(reverb, in_l, in_r, out_l, out_r, frames);
}

/**
 * @brief Destroy studio reverb device state.
 */
static void _SituationDestroyStudioReverb(void* device_data) {
    if (device_data) {
        _SituationStudioReverbDestroy((SituationStudioReverb*)device_data);
    }
}

// ================================================================================================
// SPRING REVERB WRAPPER
// ================================================================================================

/**
 * @brief Create spring reverb device state.
 */
static void* _SituationCreateSpringReverb(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SpringReverb* reverb = (SpringReverb*)SIT_CALLOC(1, sizeof(SpringReverb));
    if (!reverb) return NULL;
    
    SpringReverb_init(reverb, 48000);
    
    return reverb;
}

/**
 * @brief Process audio through spring reverb.
 */
static void _SituationProcessSpringReverbNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SpringReverb* reverb = (SpringReverb*)device_data;
    
    // Update parameters from controls
    // 0=input_level, 1=threshold, 2=decay_time, 3=gate_enabled, 4=bass, 5=middle, 6=treble, 7=direct, 8=reverb_level, 9=cross_mix
    if (controls) {
        SpringReverb_set_params(reverb, controls[0], controls[1], controls[2], (int)controls[3],
                               controls[4], controls[5], controls[6], controls[7], controls[8], controls[9]);
    }
    
    // Process audio (stereo)
    float* in_l = inputs[0].buffer;
    float* in_r = inputs[0].channels == 2 ? inputs[0].buffer + 1 : inputs[0].buffer;
    float* out_l = outputs[0].buffer;
    float* out_r = outputs[0].buffer + 1;
    
    SpringReverb_process(reverb, in_l, in_r, out_l, out_r, frames);
}

/**
 * @brief Destroy spring reverb device state.
 */
static void _SituationDestroySpringReverb(void* device_data) {
    if (device_data) {
        SpringReverb* reverb = (SpringReverb*)device_data;
        SpringReverb_cleanup(reverb);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// SST-282 WRAPPER
// ================================================================================================

/**
 * @brief Create SST-282 device state.
 */
static void* _SituationCreateSST282(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SST282State* sst = (SST282State*)SIT_CALLOC(1, sizeof(SST282State));
    if (!sst) return NULL;
    
    sst282_init(sst, 48000.0f);
    
    return sst;
}

/**
 * @brief Process audio through SST-282.
 */
static void _SituationProcessSST282Node(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SST282State* sst = (SST282State*)device_data;
    
    // Update parameters from controls
    // 0=input_gain, 1=lf_cut_dB, 2=hf_cut_dB, 3-6=tap_levels[4], 7=feedback, 8=dry_dB, 9=echo_dB, 10=direct_dB, 11=mode, 12=echo_delay
    if (controls) {
        sst282_set_input_gain(sst, controls[0]);
        sst282_set_eq(sst, controls[1], controls[2]);
        sst282_set_tap_level(sst, 0, controls[3]);
        sst282_set_tap_level(sst, 1, controls[4]);
        sst282_set_tap_level(sst, 2, controls[5]);
        sst282_set_tap_level(sst, 3, controls[6]);
        sst282_set_feedback(sst, controls[7]);
        sst282_set_mix(sst, controls[8], controls[9], controls[10]);
        sst282_set_mode(sst, (bool)controls[11]);
        sst282_set_echo_delay(sst, controls[12]);
    }
    
    // Process audio (mono input, stereo output)
    for (int i = 0; i < frames; i++) {
        float input = inputs[0].buffer[i * inputs[0].channels];
        float left_out, right_out;
        sst282_process(sst, input, &left_out, &right_out);
        outputs[0].buffer[i * 2] = left_out;
        outputs[0].buffer[i * 2 + 1] = right_out;
    }
}

/**
 * @brief Destroy SST-282 device state.
 */
static void _SituationDestroySST282(void* device_data) {
    if (device_data) {
        SST282State* sst = (SST282State*)device_data;
        sst282_destroy(sst);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// FILTER WRAPPER
// ================================================================================================

/**
 * @brief Create filter device state.
 */
static void* _SituationCreateFilter(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationFilter* filter = (SituationFilter*)SIT_CALLOC(1, sizeof(SituationFilter));
    if (!filter) return NULL;
    
    filter_init(filter, 48000.0f);
    
    return filter;
}

/**
 * @brief Process audio through filter.
 */
static void _SituationProcessFilterNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationFilter* filter = (SituationFilter*)device_data;
    
    // Update parameters from controls
    // 0=mode, 1=frequency, 2=resonance_q, 3=poles, 4=drive, 5=oversampling
    if (controls) {
        PxFilterMode mode = (PxFilterMode)((int)controls[0]);
        float frequency = controls[1];
        float resonance_q = controls[2];
        int poles = (int)controls[3];
        float drive = controls[4];
        bool oversampling = controls[5] > 0.5f;
        
        // Set coefficients with all parameters
        filter_set_coefficients(filter, frequency, resonance_q, mode, poles);
        filter_set_drive(filter, drive);
        filter_set_oversampling(filter, oversampling);
    }
    
    // Process audio
    filter_process(filter, inputs[0].buffer, outputs[0].buffer, frames, inputs[0].channels);
}

/**
 * @brief Destroy filter device state.
 */
static void _SituationDestroyFilter(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// EQ 4-BAND WRAPPER
// ================================================================================================

/**
 * @brief Create EQ device state.
 */
static void* _SituationCreateEQ4Band(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationEQ4Band* eq = (SituationEQ4Band*)SIT_CALLOC(1, sizeof(SituationEQ4Band));
    if (!eq) return NULL;
    
    eq4band_init(eq, 48000.0f);
    
    return eq;
}

/**
 * @brief Process audio through EQ.
 */
static void _SituationProcessEQ4BandNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationEQ4Band* eq = (SituationEQ4Band*)device_data;
    
    // Update parameters from controls
    // 0-2=band0 (freq, q, gain), 3-5=band1, 6-8=band2, 9-11=band3
    if (controls) {
        eq4band_set_band(eq, 0, controls[0], controls[1], controls[2]);
        eq4band_set_band(eq, 1, controls[3], controls[4], controls[5]);
        eq4band_set_band(eq, 2, controls[6], controls[7], controls[8]);
        eq4band_set_band(eq, 3, controls[9], controls[10], controls[11]);
    }
    
    // Process audio
    eq4band_process(eq, inputs[0].buffer, outputs[0].buffer, frames, inputs[0].channels);
}

/**
 * @brief Destroy EQ device state.
 */
static void _SituationDestroyEQ4Band(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// DYNAMICS WRAPPER
// ================================================================================================

/**
 * @brief Create dynamics device state.
 */
static void* _SituationCreateDynamics(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationDynamics* dyn = (SituationDynamics*)SIT_CALLOC(1, sizeof(SituationDynamics));
    if (!dyn) return NULL;
    
    dynamics_init(dyn, 48000.0f);
    
    return dyn;
}

/**
 * @brief Process audio through dynamics.
 */
static void _SituationProcessDynamicsNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationDynamics* dyn = (SituationDynamics*)device_data;
    
    // Update parameters from controls
    // 0=mode, 1=threshold_db, 2=ratio, 3=attack_ms, 4=release_ms, 5=knee_db, 6=makeup_gain_db
    if (controls) {
        dynamics_set_mode(dyn, (DynamicsMode)((int)controls[0]));
        dynamics_set_threshold(dyn, controls[1]);
        dynamics_set_ratio(dyn, controls[2]);
        dynamics_set_attack(dyn, controls[3]);
        dynamics_set_release(dyn, controls[4]);
        dynamics_set_knee(dyn, controls[5]);
        dynamics_set_makeup_gain(dyn, controls[6]);
    }
    
    // Process audio
    dynamics_process(dyn, inputs[0].buffer, outputs[0].buffer, frames, inputs[0].channels);
}

/**
 * @brief Destroy dynamics device state.
 */
static void _SituationDestroyDynamics(void* device_data) {
    if (device_data) {
        SituationDynamics* dyn = (SituationDynamics*)device_data;
        dynamics_cleanup(dyn);  // Free lookahead buffers
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// COMPANDER WRAPPER
// ================================================================================================

/**
 * @brief Create compander device state.
 */
static void* _SituationCreateCompander(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    CompanderProcessor* comp = (CompanderProcessor*)SIT_CALLOC(1, sizeof(CompanderProcessor));
    if (!comp) return NULL;
    
    compander_init(comp, 48000.0f);
    
    return comp;
}

/**
 * @brief Process audio through compander.
 */
static void _SituationProcessCompanderNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    CompanderProcessor* comp = (CompanderProcessor*)device_data;
    
    // Update parameters from controls
    // 3 bands × 8 params (5 compander + 3 bell EQ) = 24 controls
    // Band 0 (Low): 0-7, Band 1 (Mid): 8-15, Band 2 (High): 16-23
    if (controls) {
        for (int band = 0; band < 3; band++) {
            int base = band * 8;
            
            // Compander params: loud_threshold, quiet_threshold, comp_slope, exp_slope, noise_gate
            CompanderParams comp_params = {
                .loud_threshold = controls[base + 0],
                .quiet_threshold = controls[base + 1],
                .comp_slope = controls[base + 2],
                .exp_slope = controls[base + 3],
                .noise_gate = controls[base + 4]
            };
            
            // Bell EQ params: center_freq, gain, Q
            BellParams bell_params = {
                .center_freq = controls[base + 5],
                .gain = controls[base + 6],
                .Q = controls[base + 7]
            };
            
            compander_update_band_params(comp, band, &comp_params, &bell_params);
        }
    }
    
    // Process audio (compander expects interleaved stereo)
    compander_process(comp, inputs[0].buffer, outputs[0].buffer, frames);
}

/**
 * @brief Destroy compander device state.
 */
static void _SituationDestroyCompander(void* device_data) {
    if (device_data) {
        CompanderProcessor* comp = (CompanderProcessor*)device_data;
        compander_cleanup(comp);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// LFO WRAPPER
// ================================================================================================

/**
 * @brief Create LFO device state.
 */
static void* _SituationCreateLFO(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationLFO* lfo = (SituationLFO*)SIT_CALLOC(1, sizeof(SituationLFO));
    if (!lfo) return NULL;
    
    lfo_init(lfo, 48000.0f);
    
    return lfo;
}

/**
 * @brief Process LFO (generates control signal).
 */
static void _SituationProcessLFONode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)inputs;  // LFO has no audio input
    if (!device_data || !outputs) return;
    
    SituationLFO* lfo = (SituationLFO*)device_data;
    
    // Update parameters from controls
    // 0=waveform, 1=frequency
    if (controls) {
        lfo_set_waveform(lfo, (LFOWaveform)((int)controls[0]));
        lfo_set_frequency(lfo, controls[1]);
    }
    
    // Generate control signal (output as audio for now)
    lfo_process(lfo, outputs[0].buffer, frames);
    
    // Duplicate to stereo if needed
    if (outputs[0].channels == 2) {
        for (int i = frames - 1; i >= 0; i--) {
            outputs[0].buffer[i * 2] = outputs[0].buffer[i];
            outputs[0].buffer[i * 2 + 1] = outputs[0].buffer[i];
        }
    }
}

/**
 * @brief Destroy LFO device state.
 */
static void _SituationDestroyLFO(void* device_data) {
    if (device_data) {
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// SOUND SOURCE WRAPPER
// ================================================================================================

/**
 * @brief Create sound source device state.
 */
static void* _SituationCreateSoundSource(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationSoundSource* src = (SituationSoundSource*)SIT_CALLOC(1, sizeof(SituationSoundSource));
    if (!src) return NULL;
    
    sound_source_init(src, 48000.0f);
    
    return src;
}

/**
 * @brief Process sound source (audio playback).
 */
static void _SituationProcessSoundSourceNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)inputs;  // Sound source has no audio input
    if (!device_data || !outputs) return;
    
    SituationSoundSource* src = (SituationSoundSource*)device_data;
    
    // Update parameters from controls
    // 0=play/stop, 1=loop, 2=volume
    if (controls) {
        if (controls[0] > 0.5f) {
            sound_source_play(src);
        } else {
            sound_source_stop(src);
        }
        sound_source_set_loop(src, controls[1] > 0.5f);
        sound_source_set_volume(src, controls[2]);
    }
    
    // Process audio
    sound_source_process(src, outputs[0].buffer, frames, outputs[0].channels);
}

/**
 * @brief Destroy sound source device state.
 */
static void _SituationDestroySoundSource(void* device_data) {
    if (device_data) {
        SituationSoundSource* src = (SituationSoundSource*)device_data;
        sound_source_cleanup(src);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// DEAFMAX WRAPPER
// ================================================================================================

/**
 * @brief Create DeafMax device state.
 */
static void* _SituationCreateDeafMax(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    return deafmax_create();
}

/**
 * @brief Process audio through DeafMax.
 */
static void _SituationProcessDeafMaxNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;

    DeafMax* m = (DeafMax*)device_data;

    // Update parameters from controls
    // 0=drive, 1=release, 2=ceiling, 3=makeup, 4=linked
    if (controls) {
        deafmax_set_drive(m, controls[0]);
        deafmax_set_release(m, controls[1]);
        deafmax_set_ceiling(m, controls[2]);
        deafmax_set_makeup(m, controls[3]);
        deafmax_set_linked(m, controls[4] >= 0.5f);
    }

    // Stereo processing: deinterleave → process → interleave
    if (inputs[0].channels == 2) {
        float* tmpInL  = (float*)alloca(frames * sizeof(float));
        float* tmpInR  = (float*)alloca(frames * sizeof(float));
        float* tmpOutL = (float*)alloca(frames * sizeof(float));
        float* tmpOutR = (float*)alloca(frames * sizeof(float));

        for (int i = 0; i < frames; i++) {
            tmpInL[i] = inputs[0].buffer[i * 2];
            tmpInR[i] = inputs[0].buffer[i * 2 + 1];
        }

        deafmax_process_stereo(m, tmpInL, tmpInR, tmpOutL, tmpOutR, frames);

        for (int i = 0; i < frames; i++) {
            outputs[0].buffer[i * 2]     = tmpOutL[i];
            outputs[0].buffer[i * 2 + 1] = tmpOutR[i];
        }
    } else {
        deafmax_process_mono(m, inputs[0].buffer, outputs[0].buffer, frames);
    }
}

/**
 * @brief Destroy DeafMax device state.
 */
static void _SituationDestroyDeafMax(void* device_data) {
    if (device_data) {
        deafmax_destroy((DeafMax*)device_data);
    }
}

// ================================================================================================
// MAXIMIZER WRAPPER
// ================================================================================================

static void* _SituationCreateMaximizer(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    MaximizerState* max = (MaximizerState*)SIT_CALLOC(1, sizeof(MaximizerState));
    if (!max) return NULL;
    
    init_maximizer(max, 48000, 512, 10, 20000.0f, 4);
    
    set_band_params(max, 0, 100.0f, 1.0f, 1.5f, 3);
    set_band_params(max, 1, 500.0f, 1.0f, 1.5f, 3);
    set_band_params(max, 2, 2000.0f, 1.0f, 1.5f, 3);
    set_band_params(max, 3, 8000.0f, 1.0f, 1.5f, 3);
    
    return max;
}

static void _SituationProcessMaximizerNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    MaximizerState* max = (MaximizerState*)device_data;
    
    if (controls) {
        for (int band = 0; band < 4; band++) {
            int base = band * 4;
            set_band_params(max, band, 
                controls[base + 0],
                controls[base + 1],
                controls[base + 2],
                (int)controls[base + 3]
            );
        }
        set_hpf_cutoff(max, controls[16]);
        set_lpf_cutoff(max, controls[17]);
    }
    
    if (inputs[0].channels == 2) {
        float* temp_in = (float*)alloca(frames * sizeof(float));
        float* temp_out = (float*)alloca(frames * sizeof(float));
        
        for (int i = 0; i < frames; i++) {
            temp_in[i] = inputs[0].buffer[i * 2];
        }
        maximizer_processor(max, temp_in, temp_out, frames);
        for (int i = 0; i < frames; i++) {
            outputs[0].buffer[i * 2] = temp_out[i];
        }
        
        for (int i = 0; i < frames; i++) {
            temp_in[i] = inputs[0].buffer[i * 2 + 1];
        }
        maximizer_processor(max, temp_in, temp_out, frames);
        for (int i = 0; i < frames; i++) {
            outputs[0].buffer[i * 2 + 1] = temp_out[i];
        }
    } else {
        maximizer_processor(max, inputs[0].buffer, outputs[0].buffer, frames);
    }
}

static void _SituationDestroyMaximizer(void* device_data) {
    if (device_data) {
        MaximizerState* max = (MaximizerState*)device_data;
        free_maximizer(max);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// MASTERING AMP WRAPPER
// ================================================================================================

/**
 * @brief Create mastering amp device state.
 */
static void* _SituationCreateMasteringAmp(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationMasteringAmp* amp = (SituationMasteringAmp*)SIT_CALLOC(1, sizeof(SituationMasteringAmp));
    if (!amp) return NULL;
    
    _SituationMasteringAmpInit(amp, 48000.0f);
    
    return amp;
}

/**
 * @brief Process audio through mastering amp.
 */
static void _SituationProcessMasteringAmpNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    if (!device_data || !inputs || !outputs) return;
    
    SituationMasteringAmp* amp = (SituationMasteringAmp*)device_data;
    
    // Update parameters from controls
    // 0=amp_type, 1=drive, 2=low_freq, 3=low_gain, 4=mid_freq, 5=mid_gain, 6=mid_q,
    // 7=high_freq, 8=high_gain, 9=air_freq, 10=air_gain, 11=tight_cutoff,
    // 12=aspect_ratio, 13=vintage, 14=circuit_bending
    if (controls) {
        _SituationMasteringAmpSetAmpType(amp, (SituationMasteringAmpType)((int)controls[0]));
        _SituationMasteringAmpSetDrive(amp, controls[1]);
        _SituationMasteringAmpSetLowFreq(amp, controls[2]);
        _SituationMasteringAmpSetLowGain(amp, controls[3]);
        _SituationMasteringAmpSetMidFreq(amp, controls[4]);
        _SituationMasteringAmpSetMidGain(amp, controls[5]);
        _SituationMasteringAmpSetMidQ(amp, controls[6]);
        _SituationMasteringAmpSetHighFreq(amp, controls[7]);
        _SituationMasteringAmpSetHighGain(amp, controls[8]);
        _SituationMasteringAmpSetAirFreq(amp, controls[9]);
        _SituationMasteringAmpSetAirGain(amp, controls[10]);
        _SituationMasteringAmpSetTightCutoff(amp, controls[11]);
        _SituationMasteringAmpSetAspectRatio(amp, controls[12]);
        _SituationMasteringAmpSetVintage(amp, controls[13] > 0.5f);
        _SituationMasteringAmpSetCircuitBending(amp, controls[14] > 0.5f);
    }
    
    // Process audio (stereo)
    _SituationMasteringAmpProcessAudio(amp, inputs[0].buffer, outputs[0].buffer, frames);
}

/**
 * @brief Destroy mastering amp device state.
 */
static void _SituationDestroyMasteringAmp(void* device_data) {
    if (device_data) {
        SituationMasteringAmp* amp = (SituationMasteringAmp*)device_data;
        _SituationMasteringAmpDestroy(amp);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// MIC CAPTURE WRAPPER
// ================================================================================================

/**
 * @brief Create mic capture device state.
 */
static void* _SituationCreateMicCapture(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    
    SituationMicCapture* mic = (SituationMicCapture*)SIT_CALLOC(1, sizeof(SituationMicCapture));
    if (!mic) return NULL;
    
    mic_capture_init(mic, 48000.0f);
    
    return mic;
}

/**
 * @brief Process mic capture (audio input).
 */
static void _SituationProcessMicCaptureNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)inputs;  // Mic capture has no audio input
    if (!device_data || !outputs) return;
    
    SituationMicCapture* mic = (SituationMicCapture*)device_data;
    
    // Update parameters from controls
    // 0=start/stop, 1=gain
    if (controls) {
        if (controls[0] > 0.5f) {
            mic_capture_start(mic);
        } else {
            mic_capture_stop(mic);
        }
        mic_capture_set_gain(mic, controls[1]);
    }
    
    // Process audio
    mic_capture_process(mic, outputs[0].buffer, frames);
}

/**
 * @brief Destroy mic capture device state.
 */
static void _SituationDestroyMicCapture(void* device_data) {
    if (device_data) {
        SituationMicCapture* mic = (SituationMicCapture*)device_data;
        mic_capture_cleanup(mic);
        SIT_FREE(device_data);
    }
}

// ================================================================================================
// DEVICE FUNCTION TABLE
// ================================================================================================

/**
 * @brief Global device function table.
 * @details Maps device types to their create/process/destroy functions.
 * 
 * @note All 20 devices implemented (100%)
 *       - Reverb (Freeverb algorithm)
 *       - Echo (miniaudio ma_delay)
 *       - Tone Synth (waveform generator)
 *       - Chorus (4-stage with oversampling)
 *       - Phaser (all-pass filter)
 *       - Overdrive (multi-mode distortion)
 *       - Exciter (harmonic enhancer)
 *       - Panner (stereo panner)
 *       - Studio Reverb (professional algorithmic)
 *       - Spring Reverb (physical modeling)
 *       - SST-282 (hardware emulation)
 *       - Filter (biquad filter)
 *       - EQ 4-Band (parametric EQ)
 *       - Dynamics (compressor/limiter/gate)
 *       - Compander (3-band multiband compander with EQ)
 *       - LFO (low frequency oscillator)
 *       - Sound Source (audio playback)
 *       - Mic Capture (audio input)
 *       - Mastering Amp (SSE-optimized console processor)
 *       - Maximizer (FFTW3-based spectral enhancer)
 */
const SituationDeviceFunctions g_device_function_table[] = {
    // Reverb
    {
        .type = SITUATION_NODE_REVERB,
        .create = _SituationCreateReverb,
        .process = _SituationProcessReverbNode,
        .destroy = _SituationDestroyReverb
    },
    
    // Echo
    {
        .type = SITUATION_NODE_ECHO,
        .create = _SituationCreateEcho,
        .process = _SituationProcessEchoNode,
        .destroy = _SituationDestroyEcho
    },
    
    // Tone Synth
    {
        .type = SITUATION_NODE_TONE_SYNTH,
        .create = _SituationCreateToneSynth,
        .process = _SituationProcessToneSynthNode,
        .destroy = _SituationDestroyToneSynth
    },
    
    // Chorus
    {
        .type = SITUATION_NODE_CHORUS,
        .create = _SituationCreateChorus,
        .process = _SituationProcessChorusNode,
        .destroy = _SituationDestroyChorus
    },
    
    // Phaser
    {
        .type = SITUATION_NODE_PHASER,
        .create = _SituationCreatePhaser,
        .process = _SituationProcessPhaserNode,
        .destroy = _SituationDestroyPhaser
    },
    
    // Overdrive
    {
        .type = SITUATION_NODE_OVERDRIVE,
        .create = _SituationCreateOverdrive,
        .process = _SituationProcessOverdriveNode,
        .destroy = _SituationDestroyOverdrive
    },
    
    // Exciter
    {
        .type = SITUATION_NODE_EXCITER,
        .create = _SituationCreateExciter,
        .process = _SituationProcessExciterNode,
        .destroy = _SituationDestroyExciter
    },
    
    // Panner
    {
        .type = SITUATION_NODE_PANNER,
        .create = _SituationCreatePanner,
        .process = _SituationProcessPannerNode,
        .destroy = _SituationDestroyPanner
    },
    
    // Studio Reverb
    {
        .type = SITUATION_NODE_STUDIO_REVERB,
        .create = _SituationCreateStudioReverb,
        .process = _SituationProcessStudioReverbNode,
        .destroy = _SituationDestroyStudioReverb
    },
    
    // Spring Reverb
    {
        .type = SITUATION_NODE_SPRING_REVERB,
        .create = _SituationCreateSpringReverb,
        .process = _SituationProcessSpringReverbNode,
        .destroy = _SituationDestroySpringReverb
    },
    
    // SST-282
    {
        .type = SITUATION_NODE_SST282,
        .create = _SituationCreateSST282,
        .process = _SituationProcessSST282Node,
        .destroy = _SituationDestroySST282
    },
    
    // Filter
    {
        .type = SITUATION_NODE_FILTER,
        .create = _SituationCreateFilter,
        .process = _SituationProcessFilterNode,
        .destroy = _SituationDestroyFilter
    },
    
    // EQ 4-Band
    {
        .type = SITUATION_NODE_EQ_4BAND,
        .create = _SituationCreateEQ4Band,
        .process = _SituationProcessEQ4BandNode,
        .destroy = _SituationDestroyEQ4Band
    },
    
    // Dynamics
    {
        .type = SITUATION_NODE_DYNAMICS,
        .create = _SituationCreateDynamics,
        .process = _SituationProcessDynamicsNode,
        .destroy = _SituationDestroyDynamics
    },
    
    // Compander
    {
        .type = SITUATION_NODE_COMPANDER,
        .create = _SituationCreateCompander,
        .process = _SituationProcessCompanderNode,
        .destroy = _SituationDestroyCompander
    },
    
    // LFO
    {
        .type = SITUATION_NODE_LFO,
        .create = _SituationCreateLFO,
        .process = _SituationProcessLFONode,
        .destroy = _SituationDestroyLFO
    },
    
    // Sound Source
    {
        .type = SITUATION_NODE_SOUND_SOURCE,
        .create = _SituationCreateSoundSource,
        .process = _SituationProcessSoundSourceNode,
        .destroy = _SituationDestroySoundSource
    },
    
    // Mic Capture
    {
        .type = SITUATION_NODE_MIC_CAPTURE,
        .create = _SituationCreateMicCapture,
        .process = _SituationProcessMicCaptureNode,
        .destroy = _SituationDestroyMicCapture
    },
    
    // Mastering Amp
    {
        .type = SITUATION_NODE_MASTERING_AMP,
        .create = _SituationCreateMasteringAmp,
        .process = _SituationProcessMasteringAmpNode,
        .destroy = _SituationDestroyMasteringAmp
    },
    
    // Maximizer
    {
        .type = SITUATION_NODE_MAXIMIZER,
        .create = _SituationCreateMaximizer,
        .process = _SituationProcessMaximizerNode,
        .destroy = _SituationDestroyMaximizer
    },
    
    // DeafMax
    {
        .type = SITUATION_NODE_DEAFMAX,
        .create = _SituationCreateDeafMax,
        .process = _SituationProcessDeafMaxNode,
        .destroy = _SituationDestroyDeafMax
    }
};

const int g_device_function_table_count = sizeof(g_device_function_table) / sizeof(g_device_function_table[0]);

#endif // SITUATION_DEVICE_WRAPPERS_H
