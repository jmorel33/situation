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
***************************************************************************************************/
#ifndef SIT_AUX_ECHO_H
#define SIT_AUX_ECHO_H

#include <math.h> // For fabsf

// Note: miniaudio.h must be included before this file.

typedef struct {
    ma_delay delay;
    uint32_t channels;
    bool is_initialized;

    // Current values for smoothing
    float current_feedback;
    float current_wet;
    float current_dry;

    // Target values from UI / modulators
    float target_feedback;
    float target_wet;
    float target_dry;
} sit_echo_t;

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

    // Set targets (Process loop will glide towards these)
    echo->target_feedback = feedback;
    echo->target_wet = wet;
    echo->target_dry = dry;

    if (!echo->is_initialized) {
        echo->channels = channels;
        
        uint32_t delay_frames = (uint32_t)(sample_rate * delay_sec);
        ma_delay_config config = ma_delay_config_init(channels, sample_rate, delay_frames, feedback);
        config.wet = wet;
        config.dry = dry;

        ma_result result = ma_delay_init(&config, NULL, &echo->delay);
        if (result == MA_SUCCESS) {
            echo->is_initialized = true;
            
            // Snap to initial values immediately to prevent gliding on instantiation
            echo->current_feedback = feedback;
            echo->current_wet = wet;
            echo->current_dry = dry;
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
}

static void _SituationProcessEcho(sit_echo_t* echo, float* frames, uint32_t frame_count) {
    if (!echo || !echo->is_initialized || frame_count == 0) return;

    // Tolerance for parameter smoothing
    const float epsilon = 0.0001f;
    bool needs_smoothing = 
        (fabsf(echo->current_feedback - echo->target_feedback) > epsilon) ||
        (fabsf(echo->current_wet - echo->target_wet) > epsilon) ||
        (fabsf(echo->current_dry - echo->target_dry) > epsilon);

    if (needs_smoothing) {
        // One-pole smoothing coefficient (~10-15ms at 44.1kHz / 48kHz)
        const float smooth_coeff = 0.002f; 
        
        uint32_t frames_left = frame_count;
        float* ptr = frames;
        
        while (frames_left > 0) {
            // Process in small chunks (16 frames) to apply smoothing without massive per-sample overhead
            uint32_t chunk = (frames_left < 16) ? frames_left : 16;
            
            // Interpolate (linear approximation of exponential smoothing for the chunk)
            echo->current_feedback += (echo->target_feedback - echo->current_feedback) * smooth_coeff * chunk;
            echo->current_wet      += (echo->target_wet - echo->current_wet) * smooth_coeff * chunk;
            echo->current_dry      += (echo->target_dry - echo->current_dry) * smooth_coeff * chunk;
            
            ma_delay_set_decay(&echo->delay, echo->current_feedback);
            ma_delay_set_wet(&echo->delay, echo->current_wet);
            ma_delay_set_dry(&echo->delay, echo->current_dry);
            
            ma_delay_process_pcm_frames(&echo->delay, ptr, ptr, chunk);
            
            ptr += chunk * echo->channels;
            frames_left -= chunk;
        }
    } else {
        // Snap to exact targets to prevent float drift over long periods
        if (echo->current_feedback != echo->target_feedback ||
            echo->current_wet != echo->target_wet ||
            echo->current_dry != echo->target_dry) {
            
            echo->current_feedback = echo->target_feedback;
            echo->current_wet      = echo->target_wet;
            echo->current_dry      = echo->target_dry;
            
            ma_delay_set_decay(&echo->delay, echo->current_feedback);
            ma_delay_set_wet(&echo->delay, echo->current_wet);
            ma_delay_set_dry(&echo->delay, echo->current_dry);
        }

        // Fast path: no parameter changes
        ma_delay_process_pcm_frames(&echo->delay, frames, frames, frame_count);
    }
}

#endif // SIT_AUX_ECHO_H