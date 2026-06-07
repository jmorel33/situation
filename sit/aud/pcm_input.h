/***************************************************************************************************
*
*   sit/aud/pcm_input.h - PCM Input Node (Lock-Free Ring Buffer Source)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   A source node that reads from a lock-free ring buffer fed by the user from any thread.
*   Participates in the audio graph like any other node — mixable, patchable, effects-chainable.
*
*   Producer: user thread (via SituationPushNodePCM)
*   Consumer: audio callback thread (node process function)
*   On underrun: output silence (no glitch — just quiet)
*
*   Controls:
*     0 = Gain   (0.0–2.0, default 1.0)
*     1 = Pan    (-1.0 left, +1.0 right, default 0.0)
*     2 = Mute   (0/1 toggle, default 0.0)
*
***************************************************************************************************/

#ifndef SITUATION_PCM_INPUT_H
#define SITUATION_PCM_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef SIT_MALLOC
#define SIT_MALLOC(s) malloc(s)
#endif
#ifndef SIT_CALLOC
#define SIT_CALLOC(n,s) calloc(n,s)
#endif
#ifndef SIT_FREE
#define SIT_FREE(p) free(p)
#endif

// ================================================================================================
// CONFIGURATION
// ================================================================================================

/** Default ring buffer size in frames (power of 2). */
#ifndef SIT_PCM_INPUT_RING_FRAMES
#define SIT_PCM_INPUT_RING_FRAMES 4096
#endif

/** Maximum supported channels. */
#ifndef SIT_PCM_INPUT_MAX_CHANNELS
#define SIT_PCM_INPUT_MAX_CHANNELS 2
#endif

// Control indices
#define SIT_PCM_INPUT_CTRL_GAIN  0
#define SIT_PCM_INPUT_CTRL_PAN   1
#define SIT_PCM_INPUT_CTRL_MUTE  2

// ================================================================================================
// STATE STRUCTURE
// ================================================================================================

/**
 * @brief PCM Input node state.
 * @details Lock-free SPSC ring buffer. Producer writes via SituationPushNodePCM (any thread),
 *          consumer reads in the audio callback (process function).
 *          Buffer stores interleaved samples: ring_size = SIT_PCM_INPUT_RING_FRAMES * channels.
 */
typedef struct SituationPCMInputState {
    float* ring_buffer;                     // Interleaved sample storage
    uint32_t ring_frames;                   // Capacity in frames (power of 2)
    uint32_t ring_mask;                     // ring_frames - 1 (for fast modulo)
    uint32_t channels;                      // Channel count (1 or 2)

    // Lock-free SPSC indices (in samples, not frames)
    atomic_uint_fast32_t write_pos;         // Producer write position (samples)
    atomic_uint_fast32_t read_pos;          // Consumer read position (samples)
} SituationPCMInputState;

// ================================================================================================
// INTERNAL HELPERS
// ================================================================================================

/**
 * @brief Compute available samples to read.
 */
static inline uint32_t _SitPCMInputAvailable(const SituationPCMInputState* state) {
    uint32_t w = (uint32_t)atomic_load_explicit(&state->write_pos, memory_order_acquire);
    uint32_t r = (uint32_t)atomic_load_explicit(&state->read_pos, memory_order_relaxed);
    uint32_t ring_samples = state->ring_frames * state->channels;
    if (w >= r) return w - r;
    return ring_samples - (r - w);
}

/**
 * @brief Compute free samples available for writing.
 */
static inline uint32_t _SitPCMInputFree(const SituationPCMInputState* state) {
    uint32_t ring_samples = state->ring_frames * state->channels;
    return ring_samples - _SitPCMInputAvailable(state) - 1;
}

// ================================================================================================
// CREATE / DESTROY
// ================================================================================================

/**
 * @brief Allocate and initialize a PCM input node state.
 */
static inline SituationPCMInputState* _SitPCMInputCreate(uint32_t channels) {
    if (channels == 0 || channels > SIT_PCM_INPUT_MAX_CHANNELS) {
        channels = 2;
    }

    SituationPCMInputState* state = (SituationPCMInputState*)SIT_CALLOC(1, sizeof(SituationPCMInputState));
    if (!state) return NULL;

    state->ring_frames = SIT_PCM_INPUT_RING_FRAMES;
    state->ring_mask = SIT_PCM_INPUT_RING_FRAMES - 1;
    state->channels = channels;

    uint32_t ring_samples = state->ring_frames * channels;
    state->ring_buffer = (float*)SIT_CALLOC(ring_samples, sizeof(float));
    if (!state->ring_buffer) {
        SIT_FREE(state);
        return NULL;
    }

    atomic_init(&state->write_pos, 0);
    atomic_init(&state->read_pos, 0);

    return state;
}

/**
 * @brief Free PCM input node state.
 */
static inline void _SitPCMInputDestroy(SituationPCMInputState* state) {
    if (!state) return;
    if (state->ring_buffer) {
        SIT_FREE(state->ring_buffer);
        state->ring_buffer = NULL;
    }
    SIT_FREE(state);
}

// ================================================================================================
// PRODUCER (any thread)
// ================================================================================================

/**
 * @brief Push interleaved PCM frames into the ring buffer.
 * @param state PCM input state.
 * @param samples Interleaved float PCM data.
 * @param frame_count Number of frames to push.
 * @param channels Must match state->channels.
 * @return Number of frames actually written (may be less if buffer full).
 */
static inline uint32_t _SitPCMInputPush(
    SituationPCMInputState* state,
    const float* samples,
    uint32_t frame_count,
    uint32_t channels
) {
    if (!state || !samples || frame_count == 0) return 0;
    if (channels != state->channels) return 0;

    uint32_t ring_samples = state->ring_frames * state->channels;
    uint32_t samples_to_write = frame_count * channels;

    // Check free space
    uint32_t free_samples = _SitPCMInputFree(state);
    if (samples_to_write > free_samples) {
        samples_to_write = free_samples;
        frame_count = samples_to_write / channels;
        samples_to_write = frame_count * channels; // re-align
    }

    if (frame_count == 0) return 0;

    uint32_t w = (uint32_t)atomic_load_explicit(&state->write_pos, memory_order_relaxed);

    // Write in up to two chunks (wrap-around)
    uint32_t chunk1 = ring_samples - w;
    if (samples_to_write <= chunk1) {
        memcpy(&state->ring_buffer[w], samples, samples_to_write * sizeof(float));
    } else {
        memcpy(&state->ring_buffer[w], samples, chunk1 * sizeof(float));
        memcpy(&state->ring_buffer[0], samples + chunk1, (samples_to_write - chunk1) * sizeof(float));
    }

    uint32_t new_w = (w + samples_to_write) % ring_samples;
    atomic_store_explicit(&state->write_pos, new_w, memory_order_release);

    return frame_count;
}

/**
 * @brief Query free frames available for writing.
 */
static inline uint32_t _SitPCMInputGetFreeFrames(const SituationPCMInputState* state) {
    if (!state || state->channels == 0) return 0;
    return _SitPCMInputFree(state) / state->channels;
}

// ================================================================================================
// CONSUMER (audio callback thread — process function)
// ================================================================================================

/**
 * @brief Process function for PCM input node.
 * @details Reads from ring buffer into output. On underrun, zero-fills remainder.
 *          Applies gain and pan from controls.
 */
static void _SituationProcessPCMInputNode(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
) {
    (void)inputs; // PCM input is a source — no audio inputs

    if (!device_data || !outputs) return;

    SituationPCMInputState* state = (SituationPCMInputState*)device_data;
    float* out_buf = outputs[0].buffer;
    int out_channels = outputs[0].channels;

    if (!out_buf || frames <= 0) return;

    // Check mute
    float mute = (controls && controls[SIT_PCM_INPUT_CTRL_MUTE] > 0.5f) ? 1.0f : 0.0f;
    if (mute > 0.5f) {
        memset(out_buf, 0, (size_t)frames * (size_t)out_channels * sizeof(float));
        return;
    }

    // Read gain and pan
    float gain = controls ? controls[SIT_PCM_INPUT_CTRL_GAIN] : 1.0f;
    float pan = controls ? controls[SIT_PCM_INPUT_CTRL_PAN] : 0.0f;
    if (gain < 0.0f) gain = 0.0f;
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // Constant-power pan law
    float pan_l = cosf((pan + 1.0f) * 0.25f * 3.14159265f);
    float pan_r = sinf((pan + 1.0f) * 0.25f * 3.14159265f);

    uint32_t ring_samples = state->ring_frames * state->channels;
    uint32_t r = (uint32_t)atomic_load_explicit(&state->read_pos, memory_order_relaxed);
    uint32_t w = (uint32_t)atomic_load_explicit(&state->write_pos, memory_order_acquire);

    // Compute available samples
    uint32_t available;
    if (w >= r) available = w - r;
    else available = ring_samples - (r - w);

    uint32_t samples_needed = (uint32_t)frames * state->channels;
    uint32_t samples_to_read = (samples_needed <= available) ? samples_needed : available;
    uint32_t frames_to_read = samples_to_read / state->channels;

    // Read from ring buffer and apply gain/pan
    if (state->channels == 2 && out_channels == 2) {
        // Stereo → Stereo
        for (uint32_t f = 0; f < frames_to_read; f++) {
            uint32_t idx = (r + f * 2) % ring_samples;
            uint32_t idx2 = (idx + 1) % ring_samples;
            float l = state->ring_buffer[idx] * gain * pan_l;
            float rv = state->ring_buffer[idx2] * gain * pan_r;
            out_buf[f * 2] = l;
            out_buf[f * 2 + 1] = rv;
        }
    } else if (state->channels == 1 && out_channels == 2) {
        // Mono → Stereo
        for (uint32_t f = 0; f < frames_to_read; f++) {
            uint32_t idx = (r + f) % ring_samples;
            float s = state->ring_buffer[idx] * gain;
            out_buf[f * 2] = s * pan_l;
            out_buf[f * 2 + 1] = s * pan_r;
        }
    } else if (state->channels == 1 && out_channels == 1) {
        // Mono → Mono
        for (uint32_t f = 0; f < frames_to_read; f++) {
            uint32_t idx = (r + f) % ring_samples;
            out_buf[f] = state->ring_buffer[idx] * gain;
        }
    } else {
        // Fallback: stereo source → mono out (sum and halve)
        for (uint32_t f = 0; f < frames_to_read; f++) {
            uint32_t idx = (r + f * 2) % ring_samples;
            uint32_t idx2 = (idx + 1) % ring_samples;
            out_buf[f] = (state->ring_buffer[idx] + state->ring_buffer[idx2]) * 0.5f * gain;
        }
    }

    // Zero-fill remainder on underrun
    uint32_t frames_remaining = (uint32_t)frames - frames_to_read;
    if (frames_remaining > 0) {
        uint32_t offset = frames_to_read * (uint32_t)out_channels;
        memset(&out_buf[offset], 0, (size_t)frames_remaining * (size_t)out_channels * sizeof(float));
    }

    // Advance read position
    uint32_t new_r = (r + samples_to_read) % ring_samples;
    atomic_store_explicit(&state->read_pos, new_r, memory_order_release);
}

// ================================================================================================
// DEVICE WRAPPER FUNCTIONS (for g_device_function_table)
// ================================================================================================

/**
 * @brief Create PCM input device state.
 */
static void* _SituationCreatePCMInput(const SituationDeviceMetadata* metadata) {
    (void)metadata;
    // Default to stereo
    return _SitPCMInputCreate(2);
}

/**
 * @brief Destroy PCM input device state.
 */
static void _SituationDestroyPCMInput(void* device_data) {
    if (device_data) {
        _SitPCMInputDestroy((SituationPCMInputState*)device_data);
    }
}

#endif // SITUATION_PCM_INPUT_H
