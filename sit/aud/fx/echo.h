/***************************************************************************************************
*
*   sit/aud/echo.h - Echo/Delay Module (v1.1 2026/04/19)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of the delay/echo effect for the Situation Audio Engine.
*   Wraps miniaudio's ma_delay logic with state management and parameter smoothing.
*
*   ================================================================================================
*   PERFORMANCE NOTE
*   ================================================================================================
*   This module uses miniaudio's ma_delay implementation, which contains unoptimized multiply-add
*   operations in its feedback path (miniaudio.h lines 48465 & 48471):
*   
*       pDelay->pBuffer[iBuffer] = (pDelay->pBuffer[iBuffer] * decay) + (input * dry);
*   
*   These operations could benefit from FMA (Fused Multiply-Add) instructions for ~33% speedup,
*   but we do not modify third-party libraries. If performance is critical, consider implementing
*   a custom delay with FMA optimization similar to other Situation FX modules.
*
*   Mix semantics (parallel dry / wet):
*   miniaudio's ma_delay output is (delay_line * config.wet); "dry" scales input INTO the line only.
*   We use config.wet = 1 and config.dry = 1 so the tap is full-scale; graph/UI wet w is applied once:
*   out = (1 - w) * dry_input + w * delay_tap  (linear blend; w in [0,1]).
*
***************************************************************************************************/
#ifndef SIT_AUX_ECHO_H
#define SIT_AUX_ECHO_H

#include <math.h> // For fabsf

// Note: miniaudio.h must be included before this file.

typedef struct {
    ma_delay delay;
    uint32_t channels;
    bool is_initialized;

    /* Copy of dry signal before delay; sized for SIT_ECHO_MAX_SCRATCH_FRAMES * channels. */
    float* dry_scratch;

    // Current values for smoothing
    float current_feedback;
    float current_wet;

    // Target values from UI / modulators
    float target_feedback;
    float target_wet;
} sit_echo_t;

#ifndef SIT_ECHO_MAX_SCRATCH_FRAMES
#define SIT_ECHO_MAX_SCRATCH_FRAMES 2048
#endif

static ma_result _SituationConfigEcho(sit_echo_t* echo, uint32_t sample_rate, uint32_t channels, float delay_sec, float feedback, float wet, float dry) {
    if (!echo) return MA_INVALID_ARGS;

    // Clamp parameters
    if (delay_sec < 0.001f) delay_sec = 0.001f;
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    if (dry < 0.0f) dry = 0.0f;
    if (dry > 1.0f) dry = 1.0f;
    (void)dry; /* API keeps dry for callers; miniaudio line feed is always 1.0f (see header). */

    // Set targets (Process loop will glide towards these)
    echo->target_feedback = feedback;
    echo->target_wet = wet;

    if (!echo->is_initialized) {
        echo->channels = channels;
        
        uint32_t delay_frames = (uint32_t)(sample_rate * delay_sec);
        ma_delay_config config = ma_delay_config_init(channels, sample_rate, delay_frames, feedback);
        config.wet = 1.0f; /* Unity tap; UI wet is applied only in the parallel mix below */
        config.dry = 1.0f; /* Full input into delay line */

        ma_result result = ma_delay_init(&config, NULL, &echo->delay);
        if (result == MA_SUCCESS) {
            echo->is_initialized = true;
            
            // Snap to initial values immediately to prevent gliding on instantiation
            echo->current_feedback = feedback;
            echo->current_wet = wet;

            if (!echo->dry_scratch) {
                size_t cap = (size_t)SIT_ECHO_MAX_SCRATCH_FRAMES * (size_t)channels;
                echo->dry_scratch = (float*)SIT_CALLOC(cap, sizeof(float));
                if (!echo->dry_scratch) {
                    ma_delay_uninit(&echo->delay, NULL);
                    echo->is_initialized = false;
                    return MA_OUT_OF_MEMORY;
                }
            }
        }
        return result;
    } else {
        // Note: Delay time is fixed at init.
        // For dynamic delay times (e.g., tap-tempo), we would need to uninit and reinit, 
        // or refactor to the "max-size ring buffer + read pointer offset" pattern.
        return MA_SUCCESS;
    }
}

static void _SituationUninitEcho(sit_echo_t* echo) {
    if (echo && echo->is_initialized) {
        ma_delay_uninit(&echo->delay, NULL);
        echo->is_initialized = false;
    }
    if (echo && echo->dry_scratch) {
        SIT_FREE(echo->dry_scratch);
        echo->dry_scratch = NULL;
    }
}

static void _SituationProcessEcho(sit_echo_t* echo, float* frames, uint32_t frame_count) {
    if (!echo || !echo->is_initialized || frame_count == 0) return;
    if (frame_count > SIT_ECHO_MAX_SCRATCH_FRAMES) return;

    if (!echo->dry_scratch) {
        size_t cap = (size_t)SIT_ECHO_MAX_SCRATCH_FRAMES * (size_t)echo->channels;
        echo->dry_scratch = (float*)SIT_CALLOC(cap, sizeof(float));
        if (!echo->dry_scratch) return;
    }

    const uint32_t ch = echo->channels;
    const size_t nbytes = (size_t)frame_count * (size_t)ch * sizeof(float);
    memcpy(echo->dry_scratch, frames, nbytes);

    // Tolerance for parameter smoothing
    const float epsilon = 0.0001f;
    bool needs_smoothing = (fabsf(echo->current_feedback - echo->target_feedback) > epsilon) ||
                           (fabsf(echo->current_wet - echo->target_wet) > epsilon);

    if (needs_smoothing) {
        // One-pole smoothing coefficient (~10-15ms at 44.1kHz / 48kHz)
        const float smooth_coeff = 0.002f; 
        
        uint32_t frames_left = frame_count;
        float* wet_out = frames;
        const float* dry_in = echo->dry_scratch;
        
        while (frames_left > 0) {
            // Process in small chunks (16 frames) to apply smoothing without massive per-sample overhead
            uint32_t chunk = (frames_left < 16) ? frames_left : 16;
            
            // Interpolate (linear approximation of exponential smoothing for the chunk)
            echo->current_feedback += (echo->target_feedback - echo->current_feedback) * smooth_coeff * chunk;
            echo->current_wet      += (echo->target_wet - echo->current_wet) * smooth_coeff * chunk;
            
            ma_delay_set_decay(&echo->delay, echo->current_feedback);
            ma_delay_set_wet(&echo->delay, 1.0f);
            ma_delay_set_dry(&echo->delay, 1.0f);
            
            ma_delay_process_pcm_frames(&echo->delay, wet_out, dry_in, chunk);
            
            const float w = echo->current_wet;
            const float dry_mix = 1.0f - w;
            const uint32_t samp = chunk * ch;
            for (uint32_t s = 0; s < samp; s++) {
                float out = dry_mix * dry_in[s] + w * wet_out[s];
                if (out > 2.0f) out = 2.0f;
                else if (out < -2.0f) out = -2.0f;
                wet_out[s] = out;
            }
            
            wet_out += samp;
            dry_in += samp;
            frames_left -= chunk;
        }
    } else {
        // Snap to exact targets to prevent float drift over long periods
        if (echo->current_feedback != echo->target_feedback ||
            echo->current_wet != echo->target_wet) {
            
            echo->current_feedback = echo->target_feedback;
            echo->current_wet      = echo->target_wet;
            
            ma_delay_set_decay(&echo->delay, echo->current_feedback);
            ma_delay_set_wet(&echo->delay, 1.0f);
            ma_delay_set_dry(&echo->delay, 1.0f);
        }

        ma_delay_process_pcm_frames(&echo->delay, frames, echo->dry_scratch, frame_count);
        const float w = echo->current_wet;
        const float dry_mix = 1.0f - w;
        const uint32_t samp = frame_count * ch;
        for (uint32_t s = 0; s < samp; s++) {
            float out = dry_mix * echo->dry_scratch[s] + w * frames[s];
            if (out > 2.0f) out = 2.0f;
            else if (out < -2.0f) out = -2.0f;
            frames[s] = out;
        }
    }
}

#endif // SIT_AUX_ECHO_H