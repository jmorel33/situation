/***************************************************************************************************
*
*   sit/aud/echo.h - Echo/Delay Module
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of the delay/echo effect for the Situation Audio Engine.
*   Wraps miniaudio's ma_delay logic with state management.
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

// Note: miniaudio.h must be included before this file.

static void _SituationConfigEcho(ma_delay* delay, bool* is_initialized, uint32_t sample_rate, uint32_t channels, float delay_sec, float feedback, float wet, float dry) {
    if (!delay) return;

    // Clamp parameters
    if (delay_sec < 0.001f) delay_sec = 0.001f;
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;

    uint32_t delay_frames = (uint32_t)(sample_rate * delay_sec);

    if (!*is_initialized) {
        ma_delay_config config = ma_delay_config_init(channels, sample_rate, delay_frames, feedback);
        config.wet = wet;
        config.dry = dry;

        if (ma_delay_init(&config, NULL, delay) == MA_SUCCESS) {
            *is_initialized = true;
        }
    } else {
        ma_delay_set_wet(delay, wet);
        ma_delay_set_dry(delay, dry);
        ma_delay_set_decay(delay, feedback);
        // Note: Resizing the delay buffer at runtime is not supported without re-initialization.
        // For dynamic delay times, we would need to uninit and reinit, or use a ring buffer with max capacity.
    }
}

static void _SituationUninitEcho(ma_delay* delay) {
    if (delay) {
        ma_delay_uninit(delay, NULL);
    }
}

static void _SituationProcessEcho(ma_delay* delay, float* frames, uint32_t frame_count) {
    if (delay) {
        // Process in-place (out=in)
        ma_delay_process_pcm_frames(delay, frames, frames, frame_count);
    }
}

#endif // SIT_AUX_ECHO_H
