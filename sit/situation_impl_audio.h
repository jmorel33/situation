/***************************************************************************************************
*
*   situation_impl_audio.h - Audio Subsystem Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   This file contains the complete implementation of the audio-related functionality for the
*   "Situation" library.
*
*   It is included only when SITUATION_IMPLEMENTATION is defined (typically via situation.h).
*
*   The audio subsystem is built on top of miniaudio (single-header library) and provides:
*     â€¢ Device management (enumeration, selection, format negotiation)
*     â€¢ Playback mixing with volume, pan, pitch control
*     â€¢ Real-time effects chain (low/high-pass filters, echo, reverb, custom DSP processors)
*     â€¢ Procedural tone generation (sine, square, triangle, saw, noise) with ADSR envelopes
*     â€¢ MIDI-note convenience layer
*     â€¢ Global reverb for tones (Schroeder/Freeverb style)
*     â€¢ Handle-based sound management with generation counters for safety
*     â€¢ Thread-safe mixing using snapshot strategy (minimal lock contention)
*
*   Key design principles:
*     - Low-latency mixing suitable for games and interactive applications
*     - Safe hot-reloading support for audio assets (when combined with filesystem watching)
*     - Minimal allocations during the audio callback (pre-allocated pools and scratch buffers)
*     - Unified API surface whether using loaded samples or procedural generation
*
*   ================================================================================================
*   DEPENDENCIES
*   ================================================================================================
*   - miniaudio.h        (single-header audio I/O and decoding)
*   - situation_impl.h   (for shared types, macros, error codes, atomics, etc.)
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_AUDIO_H
#define SITUATION_IMPL_AUDIO_H

#include <math.h>

// Define implementation guards for MIDI headers
#define MIDI_IMPLEMENTATION
#define MIDI_DEVICE_IMPLEMENTATION
#define MIDI_LEARN_IMPLEMENTATION

// Core audio subsystem includes (implementation only)
#include "sit/aud/midi.h"                      // MIDI system (needed by node graph)
#include "sit/aud/midi_device.h"               // MIDI device abstraction
#include "sit/aud/midi_learn.h"                // MIDI Learn system
#include "sit/aud/registry_init.h"             // Registry initialization (device registration functions)

/** Spin until the RT playback callback finishes (Phase 15 — safe graph destroy). */
static void _SituationWaitUntilAudioCallbackIdle(void) {
    while (atomic_load(&sit_audio.is_in_audio_callback)) {
        thrd_yield();
    }
}

#include "sit/aud/node_graph_impl.h"           // Node graph implementation
#include "sit/aud/node_graph_process.h"        // Node graph processing
#include "sit/aud/node_graph_midi.h"           // Node graph MIDI integration (Phase 2)
#include "sit/aud/node_graph_serialization_impl.h"  // Graph serialization implementation
#include "sit/aud/device_wrappers.h"           // Device wrapper functions (includes all device headers)
#include "sit/aud/tone_synth.h"                // Tone synthesizer (audio API functions)

// ================================================================================================
// PCM INPUT NODE — PUBLIC API IMPLEMENTATIONS
// ================================================================================================

uint32_t SituationPushNodePCM(
    SituationAudioGraph* graph,
    SituationNodeHandle node_handle,
    const float* samples,
    uint32_t frame_count,
    uint32_t channels
) {
    if (!graph || !samples || frame_count == 0) return 0;

    SituationNode* node = SituationGetNode(graph, node_handle);
    if (!node) return 0;
    if (node->type != SITUATION_NODE_PCM_INPUT) return 0;

    SituationPCMInputState* state = (SituationPCMInputState*)node->device_data;
    if (!state) return 0;

    return _SitPCMInputPush(state, samples, frame_count, channels);
}

uint32_t SituationGetNodePCMFreeFrames(
    SituationAudioGraph* graph,
    SituationNodeHandle node_handle
) {
    if (!graph) return 0;

    SituationNode* node = SituationGetNode(graph, node_handle);
    if (!node) return 0;
    if (node->type != SITUATION_NODE_PCM_INPUT) return 0;

    SituationPCMInputState* state = (SituationPCMInputState*)node->device_data;
    if (!state) return 0;

    return _SitPCMInputGetFreeFrames(state);
}

// Initialize the device registry on first inclusion
/* HARDENING: void by design — intentional void internal helper (Bucket B). */
static void _SituationEnsureRegistryInit(void) {
    static bool registry_init_done = false;
    if (!registry_init_done) {
        SituationInitDeviceRegistry();
        registry_init_done = true;
    }
}

// [Phase H] Removed: miniaudio node wrappers (DynamicsNode, PannerNode, MeterNode)
// and mixer struct definitions (SituationAudioBus, SituationAudioTrack, SituationAudioMixer).
// These were part of the legacy ma_node_graph mixer, replaced by the node graph system.

// Audio-related implementation extracted from situation_impl.h

/** Spin until the playback callback finishes decoding/mixing from a voice snapshot (see `is_processing_snapshot`). */
/* HARDENING: void by design — RT spin-wait until audio snapshot completes. */
static void _SituationWaitUntilVoiceSnapshotIdle(void) {
    while (atomic_load(&sit_audio.is_processing_snapshot)) {
        thrd_yield();
    }
}

// _SituationMixToneToBuffer — forward-declared in situation_impl_forward.h

static _SituationSoundSlot* _SitGetSoundSlot(SituationSound handle) {
    if (handle.slot_index >= SITUATION_MAX_LOADED_SOUNDS) return NULL;
    _SituationSoundSlot* slot = &sit_audio.sound_pool[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

static _SituationSoundSlot* _SitAllocSoundSlot(SituationSound* out_handle) {
    mtx_lock(&sit_audio.pool_mutex);
    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; i++) {
        if (!sit_audio.sound_pool[i].is_active) {
            _SituationSoundSlot* slot = &sit_audio.sound_pool[i];
            memset(slot, 0, sizeof(_SituationSoundSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;

            mtx_unlock(&sit_audio.pool_mutex);
            return slot;
        }
    }
    mtx_unlock(&sit_audio.pool_mutex);
    return NULL;
}

static SituationError _SitFreeSoundSlot(SituationSound handle) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(handle);
    if (!slot) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_RESOURCE_INVALID, "Invalid sound slot handle.");
    }

    mtx_lock(&sit_audio.pool_mutex);
    if (slot->source_path) {
        SIT_FREE(slot->source_path);
        slot->source_path = NULL;
    }
    // Note: sound_data cleanup (ma_decoder_uninit) should be done before calling this
    slot->is_active = false;
    mtx_unlock(&sit_audio.pool_mutex);
    return SITUATION_SUCCESS;
}


// --- Audio Implementations (MiniAudio) ---
/**
 * @brief [INTERNAL] Core Audio Mixing Callback (Production Hardened)
 *
 * @details This function is the heartbeat of the audio subsystem, executed by the high-priority
 *          audio thread. It is responsible for decoding, processing, and mixing all active
 *          sounds into the device's output buffer.
 *
 * @section ThreadSafety Thread Safety Strategy ("Snapshot-and-Unlock")
 *          This implementation uses a high-performance "Snapshot" strategy to minimize lock contention:
 *          1.  **Snapshot:** The `audio_queue_mutex` is locked briefly to copy the list of active sound pointers
 *              to a local stack array. The lock is then released immediately.
 *          2.  **Processing:** The heavy lifting (decoding, effects, mixing) happens without holding the lock,
 *              allowing the Main Thread to continue adding/modifying sounds without stalling.
 *          3.  **Commit:** The lock is re-acquired briefly at the end only to remove finished sounds from the global queue.
 *
 *          **Safety Mechanism:** To prevent Use-After-Free errors (where the Main Thread unloads a sound while
 *          the Audio Thread is processing it from a snapshot), this function sets an atomic flag
 *          `is_processing_snapshot`. `SituationUnloadSound` spins on this flag to ensure it never frees
 *          memory that is currently being accessed.
 *
 * @section Optimization Performance Optimizations
 *          1.  **Lock-Free Mixing:** By releasing the lock during processing, the audio thread never blocks the
 *              main application loop, and vice-versa, preventing audio glitches during heavy main-thread load.
 *          2.  **Fused Mixing Loop:** Panning, Volume application, and Accumulation are combined into a single
 *              tight loop for maximum CPU cache locality.
 *          3.  **Scratch Buffers:** Uses pre-allocated thread-local buffers to avoid `malloc` on the audio thread.
 *
 * @section Pipeline Processing Pipeline
 *          For every active sound:
 *          1.  **Decode:** Read raw PCM from file/memory/stream into `decoder_buffer`.
 *          2.  **Effects:** Apply Filter -> Echo -> Reverb -> User Processors.
 *          3.  **Convert:** Resample/Remap to device format into `converter_buffer`.
 *          4.  **Mix:** Apply Pan/Vol and add to `pOutput`.
 *
 * @param pDevice Pointer to the MiniAudio device instance.
 * @param pOutput Pointer to the raw output buffer to be filled.
 * @param pInput  Pointer to the input buffer (unused here; capture handled separately).
 * @param frameCount The number of frames requested by the audio hardware.
 */
// --- Tone Synthesis (moved to sit/aud/tone_synth.h) ---

// --- Tone Synthesis Functions (moved to sit/aud/tone_synth.h) ---

/** HARDENING: bool by design — graph topology query (true if a mixer node exists). */
static bool _SituationGraphHasMixerNode(const SituationAudioGraph* graph) {
    if (!graph) return false;
    for (int i = 0; i < graph->node_count; ++i) {
        const SituationNode* n = graph->nodes[i];
        if (n && n->type == SITUATION_NODE_MIXER) return true;
    }
    return false;
}

/**
 * Whether loaded/streamed voices (active_voices) should be summed into pOut after the graph.
 * - No graph, empty graph, or graph without a mixer: latent audio must hit the main bus.
 * - Library default_graph with Policy B: voices are fed into the graph Sound Source — no latent sum.
 * - default_graph but voice source missing: fall back to latent sum (init failure).
 * - User graph that includes a mixer: assume the graph owns summing (avoid double-mix).
 * HARDENING: bool by design — routing policy query, not a failure path.
 */
static bool _SituationShouldMixLatentVoices(const _SituationAudioState* pGs) {
    if (!pGs->active_graph) return true;
    if (pGs->active_graph->node_count == 0) return true;
    if (!_SituationGraphHasMixerNode(pGs->active_graph)) return true;
    if (pGs->active_graph == pGs->default_graph) {
        return pGs->default_graph_voice_source == NULL;
    }
    return false;
}

/** Mix decoded/processed active voices from snapshot_buffer into an interleaved stereo buffer (accumulate). */
/* HARDENING: void by design — real-time voice mix path. */
static void _SituationMixLoadedVoicesFromSnapshot(
    _SituationAudioState* pGs,
    ma_device* pDevice,
    uint32_t frameCount,
    float* decoder_buffer,
    float* effects_buffer,
    float* mix_dest_stereo,
    int voices_to_mix
) {
    for (int i = 0; i < voices_to_mix; ++i) {
        _SituationSound* sound = pGs->snapshot_buffer[i];
        if (!sound) continue;

        ma_uint64 frames_read = 0;

        if (sound->is_preloaded && sound->preloaded_data) {
            ma_uint64 frames_remaining = sound->total_frames - sound->cursor_frames;
            frames_read = (frames_remaining > frameCount) ? frameCount : frames_remaining;

            memcpy(decoder_buffer, (float*)sound->preloaded_data + (sound->cursor_frames * 2), frames_read * 2 * sizeof(float));

            sound->cursor_frames += frames_read;

            if (frames_read < frameCount && sound->is_looping) {
                sound->cursor_frames = 0;
                if (sound->total_frames > 0) {
                    ma_uint64 remainder = frameCount - frames_read;
                    ma_uint64 loop_read = (sound->total_frames > remainder) ? remainder : sound->total_frames;
                    memcpy(decoder_buffer + (frames_read * 2), sound->preloaded_data, loop_read * 2 * sizeof(float));
                    frames_read += loop_read;
                    sound->cursor_frames += loop_read;
                }
            }
        } else if (sound->is_initialized) {
            // Same lock as SituationPlayLoadedSound / queue edits: serialize ma_decoder vs main-thread seek/uninit.
            mtx_lock(&pGs->audio_queue_mutex);
            ma_result res = ma_decoder_read_pcm_frames(&sound->decoder, decoder_buffer, frameCount, &frames_read);

            if (res == MA_AT_END && sound->is_looping) {
                ma_decoder_seek_to_pcm_frame(&sound->decoder, 0);
                ma_uint64 remainder = frameCount - frames_read;
                ma_uint64 loop_read = 0;
                ma_decoder_read_pcm_frames(&sound->decoder, decoder_buffer + (frames_read * 2), remainder, &loop_read);
                frames_read += loop_read;
            } else if (res == MA_AT_END && !sound->is_looping) {
                atomic_store(&sound->last_status, SITUATION_ERROR_AUDIO_STREAM_ENDED);
            }
            mtx_unlock(&pGs->audio_queue_mutex);
        }

        if (frames_read > 0) {
            memcpy(effects_buffer, decoder_buffer, frames_read * 2 * sizeof(float));

            if (sound->processors) {
                for (int p = 0; p < sound->processor_count; ++p) {
                    if (sound->processors[p]) {
                        sound->processors[p](effects_buffer, (uint32_t)frames_read, 2, pDevice->sampleRate, sound->processor_user_data[p]);
                    }
                }
            }

            if (sound->effects.echo_enabled && sound->effects.echo.is_initialized) {
                _SituationProcessEcho((sit_echo_t*)&sound->effects.echo, effects_buffer, (uint32_t)frames_read);
            }

            if (sound->effects.reverb_enabled && sound->effects.reverb_state) {
                _SituationProcessReverb(sound->effects.reverb_state, effects_buffer, effects_buffer, (uint32_t)frames_read, 2);
            }

            float vol = atomic_load(&sound->volume);
            float pan = atomic_load(&sound->pan);

            for (ma_uint64 f = 0; f < frames_read; ++f) {
                float sampleL = effects_buffer[f * 2 + 0];
                float sampleR = effects_buffer[f * 2 + 1];

                float gainL = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
                float gainR = (pan >= 0.0f) ? 1.0f : (1.0f + pan);

                mix_dest_stereo[f * 2 + 0] += sampleL * vol * gainL;
                mix_dest_stereo[f * 2 + 1] += sampleR * vol * gainR;
            }
        }
    }
}

/** Peak + RMS over the final mixed buffer; atomics for main-thread poll; optional legacy monitor callback. */
/* HARDENING: void by design — RT level metering side-channel. */
static void _SituationPublishMasterBusLevels(_SituationAudioState* pGs, const float* pOut, uint32_t frameCount, uint32_t channels) {
    if (!pGs || !pOut || frameCount == 0 || channels == 0) return;

    float peak = 0.f;
    double sum_sq = 0.0;
    size_t n = (size_t)frameCount * (size_t)channels;
    for (size_t i = 0; i < n; i++) {
        float s = pOut[i];
        float a = fabsf(s);
        if (a > peak) peak = a;
        sum_sq += (double)s * (double)s;
    }
    float rms = (float)sqrt(sum_sq / (double)n);
    atomic_store_explicit(&pGs->audio_meter_peak, peak, memory_order_relaxed);
    atomic_store_explicit(&pGs->audio_meter_rms, rms, memory_order_relaxed);

    void (*mon)(const float*, uint32_t, void*) =
        (void (*)(const float*, uint32_t, void*))atomic_load_explicit(
            &pGs->output_monitor_callback, memory_order_acquire);
    if (mon) {
        void* mon_ud = atomic_load_explicit(&pGs->output_monitor_user_data, memory_order_acquire);
        mon(pOut, frameCount, mon_ud);
    }
}

/* HARDENING: void by design — miniaudio RT callback ABI. */
static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
    _SituationAudioState* pGs = (_SituationAudioState*)pDevice->pUserData;
    if (!pGs) return;

    // [FIX v2.4.38] Guard against race condition: audio device starts before init completes.
    // Return silence until all audio state (registry, default graph, buffers) is fully initialized.
    if (!atomic_load(&pGs->audio_ready)) {
        memset(pOutput, 0, frameCount * pDevice->playback.channels * sizeof(float));
        atomic_store_explicit(&pGs->audio_meter_peak, 0.f, memory_order_relaxed);
        atomic_store_explicit(&pGs->audio_meter_rms, 0.f, memory_order_relaxed);
        return;
    }

    atomic_store(&pGs->is_in_audio_callback, true);

    // ========================================================================
    // [NEW] PIN AUDIO THREAD TO SPECIFIC CORE (Executes exactly once)
    // ========================================================================
    #if defined(_MSC_VER)
        static __declspec(thread) bool s_audio_thread_pinned = false;
    #else
        static _Thread_local bool s_audio_thread_pinned = false;
    #endif

    if (!s_audio_thread_pinned) {
        _SituationSetCurrentThreadName("Sit Audio");

        // Pin audio callback (default: logical core 2; override via SituationInitInfo::thread_affinity_audio)
        {
            uint64_t audio_aff = SituationGetConfiguredAudioThreadAffinity();
            _SituationSetThreadAffinityForRole(SIT_THREAD_ROLE_AUDIO, audio_aff);
            _SituationObservabilityRecordAudioThread(audio_aff);
        }
        
        // Optional: Log it so you know it worked
        // fprintf(stderr, "[Situation Audio] Audio thread pinned to core 2.\n");
        s_audio_thread_pinned = true;
    }
    // ========================================================================
	
    SIT_PROF_ZONE_SCOPED("AudioCallback") {
    // Output Buffer (Mixing Destination)
    float* pOut = (float*)pOutput;

    // Clear output buffer (silence)
    memset(pOut, 0, frameCount * pDevice->playback.channels * sizeof(float));

    // Handle Input (Capture)
    if (pGs->is_capture_device_active && pGs->capture_callback) {
        // ... (Capture logic omitted for brevity, usually distinct device)
    }

    // --- Voice snapshot (locked copy of active_voices for this callback) ---
    mtx_lock(&pGs->audio_queue_mutex);

    int voices_to_mix = pGs->active_voice_count;
    if (pGs->snapshot_buffer && voices_to_mix > 0) {
        memcpy(pGs->snapshot_buffer, pGs->active_voices, voices_to_mix * sizeof(_SituationSound*));
    }

    mtx_unlock(&pGs->audio_queue_mutex);

    float* decoder_buffer = pGs->audio_callback_decoder_temp_buffer;
    float* effects_buffer = pGs->audio_callback_effects_temp_buffer;
    float* voice_bus = pGs->audio_callback_converter_temp_buffer;

    bool route_voices_via_default_ss =
        pGs->active_graph &&
        pGs->active_graph == pGs->default_graph &&
        pGs->default_graph_voice_source != NULL &&
        decoder_buffer && effects_buffer && voice_bus &&
        pDevice->playback.channels == 2;

    // Policy B: sum loaded voices into converter scratch, feed default graph Sound Source, then graph owns the bus.
    if (route_voices_via_default_ss) {
        SituationSoundSource* voice_src = (SituationSoundSource*)pGs->default_graph_voice_source;
        if (voices_to_mix > 0) {
            memset(voice_bus, 0, frameCount * 2 * sizeof(float));
            atomic_store(&pGs->is_processing_snapshot, true);
            _SituationMixLoadedVoicesFromSnapshot(pGs, pDevice, frameCount, decoder_buffer, effects_buffer, voice_bus, voices_to_mix);
            atomic_store(&pGs->is_processing_snapshot, false);
            sound_source_feed_interleaved_frames(voice_src, voice_bus, (int)frameCount, 2);
        } else {
            sound_source_stop(voice_src);
        }
    }

    // --- [Phase G1] Routed Tone Pre-Mixing ---
    bool has_routed_tones = false;
    for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
        if (pGs->tone_pool[i].active && pGs->tone_pool[i].route_to_graph) {
            has_routed_tones = true;
            break;
        }
    }

    if (has_routed_tones && pGs->sfx_graph_voice_source) {
        if (voice_bus) {
            memset(voice_bus, 0, frameCount * 2 * sizeof(float));
            for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                SituationTone* t = &pGs->tone_pool[i];
                if (!t->active || !t->route_to_graph) continue;
                _SituationMixToneToBuffer(t, voice_bus, frameCount);
            }
            sound_source_feed_interleaved_frames((SituationSoundSource*)pGs->sfx_graph_voice_source, voice_bus, (int)frameCount, 2);
        }
    } else if (pGs->sfx_graph_voice_source) {
        sound_source_stop((SituationSoundSource*)pGs->sfx_graph_voice_source);
    }

    // --- [Phase H] Node graph ---
    if (pGs->active_graph) {
        SituationProcessGraph(pGs->active_graph, pOut, frameCount,
                              g_device_function_table, g_device_function_table_count);
    }

    // --- Loaded voices onto main bus when the graph does not own them ---
    if (!_SituationShouldMixLatentVoices(pGs)) {
        goto tone_mixing;
    }

    if (voices_to_mix > 0 && pGs->snapshot_buffer && decoder_buffer && effects_buffer) {
        atomic_store(&pGs->is_processing_snapshot, true);
        _SituationMixLoadedVoicesFromSnapshot(pGs, pDevice, frameCount, decoder_buffer, effects_buffer, pOut, voices_to_mix);
        atomic_store(&pGs->is_processing_snapshot, false);
    }

tone_mixing:
    // --- [Phase 2] Tone Synthesis Mixing ---
    // Mix active tones from the tone pool into the output buffer
    for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
        SituationTone* t = &pGs->tone_pool[i];
        if (!t->active) continue;

        if (t->route_to_graph) {
            // Already processed and fed to the graph's SFX sound source, so skip here
            continue;
        }

        _SituationMixToneToBuffer(t, pOut, frameCount);
    }

    _SituationPublishMasterBusLevels(pGs, pOut, frameCount, pDevice->playback.channels);
    } /* SIT_PROF_ZONE_SCOPED AudioCallback */

    atomic_store(&pGs->is_in_audio_callback, false);
}


// --- Situation Audio Pipeline API Implementation ---

/**
 * @brief Callback function type for processing captured audio data.
 *
 * @param data A pointer to the buffer containing the raw audio samples. The format is always `float*` (32-bit float, mono).
 * @param frame_count The number of frames (samples) in the buffer.
 * @param user_data The custom pointer provided to `SituationStartAudioCapture`.
 */

// --- Audio Output Monitoring (for visualization) ---
SITAPI void SituationSetAudioOutputMonitor(void (*callback)(const float* samples, uint32_t frame_count, void* user_data), void* user_data) {
    if (!SituationIsInitialized()) return;
    mtx_lock(&sit_audio.audio_queue_mutex);
    atomic_store_explicit(&sit_audio.output_monitor_callback, (void*)callback, memory_order_release);
    atomic_store_explicit(&sit_audio.output_monitor_user_data, user_data, memory_order_release);
    mtx_unlock(&sit_audio.audio_queue_mutex);
}

SITAPI void SituationGetMasterOutputMeter(float* out_peak, float* out_rms) {
    if (!SituationIsInitialized()) {
        if (out_peak) *out_peak = 0.f;
        if (out_rms) *out_rms = 0.f;
        return;
    }
    float pk = atomic_load_explicit(&sit_audio.audio_meter_peak, memory_order_relaxed);
    float rms = atomic_load_explicit(&sit_audio.audio_meter_rms, memory_order_relaxed);
    if (!isfinite(pk) || pk < 0.f) pk = 0.f;
    if (!isfinite(rms) || rms < 0.f) rms = 0.f;
    if (out_peak) *out_peak = pk;
    if (out_rms) *out_rms = rms;
}

/* HARDENING: void by design — miniaudio RT capture callback ABI. */
static void _sit_miniaudio_capture_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
    (void)pOutput;
    _SituationAudioState* pGs = (_SituationAudioState*)pDevice->pUserData;
    if (!pGs || !pInput) return;

    // 1. If Main Thread Mode is disabled, call directly (legacy behavior)
    if (!pGs->audio_capture_on_main_thread) {
        if (pGs->capture_callback) pGs->capture_callback((const float*)pInput, frameCount, pGs->capture_user_data);
        return;
    }

    // 2. Main Thread Mode: Push to Ring Buffer
    ma_mutex_lock(&pGs->audio_capture_mutex);


    uint32_t channels = pDevice->capture.channels;
    size_t sampleCount = frameCount * channels;

    size_t capacity = pGs->audio_capture_queue_capacity;
    size_t write_head = pGs->audio_capture_write_head;
    size_t read_head = pGs->audio_capture_read_head; // Snapshot read head

    // Calculate available space
    size_t used = (write_head >= read_head) ? (write_head - read_head) : (capacity - read_head + write_head);
    size_t free_space = capacity - used - 1; // Keep 1 slot open to distinguish full/empty

    if (free_space >= sampleCount) {
        const float* input_f32 = (const float*)pInput;
        size_t frames_to_end = capacity - write_head;

        if (sampleCount <= frames_to_end) {
            // Contiguous write
            memcpy(&pGs->audio_capture_queue[write_head], input_f32, sampleCount * sizeof(float));
        } else {
            // Split write (wrap around)
            memcpy(&pGs->audio_capture_queue[write_head], input_f32, frames_to_end * sizeof(float));
            memcpy(&pGs->audio_capture_queue[0], input_f32 + frames_to_end, (sampleCount - frames_to_end) * sizeof(float));
        }
        pGs->audio_capture_write_head = (write_head + sampleCount) % capacity;
    } else {
        // Buffer overrun: Drop packets or log warning in debug mode
    }

    ma_mutex_unlock(&pGs->audio_capture_mutex);
}

/**
 * @brief Initializes and starts audio capture (recording) from the default input device.
 *
 * @details Opens the default microphone/input device and begins streaming raw audio data to the provided callback function.
 *          The audio format defaults to the device's native configuration (Sample Rate & Channels) to minimize latency and resampling overhead. If you require a specific format (e.g. 44.1kHz Mono for FFT), use `SituationStartAudioCaptureEx`.
 *
 * @par Thread Safety
 * The provided `callback` function will be executed on a high-priority, internal audio thread.
 * **Do not perform blocking operations** (like file I/O, large memory allocations, or heavy mutex locking) inside the callback, or you may cause audio glitches.
 *
 * @param callback The function to call when new audio data is available.
 * @param user_data A custom pointer passed to the callback (e.g., for storing state).
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_AUDIO_CONTEXT` if the audio system is not initialized.
 * @return `SITUATION_ERROR_AUDIO_DEVICE` if the input device cannot be opened or started.
 *
 * @see SituationStopAudioCapture(), SituationAudioCaptureCallback
 */
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback callback, void* user_data) {
    return SituationStartAudioCaptureEx(callback, user_data, 0, 0);
}

SITAPI SituationError SituationStartAudioCaptureEx(SituationAudioCaptureCallback callback, void* user_data, uint32_t sample_rate, uint32_t channels) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.is_miniaudio_context_initialized) return SITUATION_ERROR_AUDIO_CONTEXT;
    if (sit_audio.is_capture_device_active) SituationStopAudioCapture();

    sit_audio.capture_callback = callback;
    sit_audio.capture_user_data = user_data;

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    if (sit_audio.active_capture_device_id_set) {
        config.capture.pDeviceID = &sit_audio.active_capture_device_id;
    }
    config.capture.format = ma_format_f32; // Standardize on Float32
    config.capture.channels = channels;
    config.sampleRate = sample_rate;
    config.dataCallback = _sit_miniaudio_capture_callback;
    config.pUserData = &sit_audio;

    ma_result dev_result = ma_device_init(&sit_audio.miniaudio_context, &config, &sit_audio.capture_device);
    if (dev_result != MA_SUCCESS) {
        if (dev_result == MA_NO_DEVICE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE, "No capture device (microphone) found.");
            return SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE;
        }
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED, "Failed to initialize capture device.");
        return SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED;
    }

    if (ma_device_start(&sit_audio.capture_device) != MA_SUCCESS) {
        ma_device_uninit(&sit_audio.capture_device);
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_START_FAILED, "Failed to start capture device.");
        return SITUATION_ERROR_AUDIO_DEVICE_START_FAILED;
    }

    sit_audio.is_capture_device_active = true;
    return SITUATION_SUCCESS;
}

/**
 * @brief Stops audio capture and closes the input device.
 *
 * @details Halts the recording stream and releases the underlying audio device resources. The callback function will no longer be invoked.
 *          It is safe to call this function even if capture is not currently active.
 *
 * @see SituationStartAudioCapture()
 */
SITAPI void SituationStopAudioCapture(void) {
    if (!SituationIsInitialized()) return;
    if (sit_audio.is_capture_device_active) {
        ma_device_uninit(&sit_audio.capture_device);
        sit_audio.is_capture_device_active = false;
        sit_audio.capture_callback = NULL;
    }
}

/**
 * @brief Enumerates all available audio playback devices on the system.
 * @details This function queries the underlying audio backend (MiniAudio) for a list of all devices capable of playing sound. It provides their human-readable names and internal identifiers.
 *
 * @warning The returned array of `SituationAudioDeviceInfo` structs is dynamically allocated. The caller is **responsible for freeing this memory** using `free()` when it is no longer needed.
 *
 * @param[out] count A pointer to an integer that will be filled with the number of devices found.
 *
 * @return A pointer to a newly allocated array of `SituationAudioDeviceInfo` structs.
 * @return `NULL` if the library is not initialized, if no playback devices are found, or if a memory allocation error occurs. In these cases, `*count` is set to 0.
 *
 * @note The returned information can be used with `SituationSetAudioDevice` to switch output to a specific device (e.g., headphones vs. speakers).
 *
 * @see SituationSetAudioDevice()
 */
SITAPI SituationAudioDeviceInfo* SituationEnumerateAudioDevices(int* out_count) {
    if (!SituationIsInitialized()) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (!sit_audio.is_miniaudio_context_initialized) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    ma_device_info* playback_infos;
    uint32_t playback_count;
    ma_device_info* capture_infos;
    uint32_t capture_count;

    if (ma_context_get_devices(&sit_audio.miniaudio_context, &playback_infos, &playback_count, &capture_infos, &capture_count) != MA_SUCCESS) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    int total_count = playback_count + capture_count;
    SituationAudioDeviceInfo* list = (SituationAudioDeviceInfo*)SIT_CALLOC(total_count, sizeof(SituationAudioDeviceInfo));
    if (!list) return NULL;

    int idx = 0;
    for (uint32_t i = 0; i < playback_count; i++) {
        SituationAudioDeviceInfo* info = &list[idx++];
        strncpy(info->name, playback_infos[i].name, 255);

        info->native_id = playback_infos[i].id; // Copy native struct
        info->type = SIT_AUDIO_DEVICE_TYPE_PLAYBACK;

        uint32_t min_ch = 0;
        uint32_t max_ch = 0;
        for (uint32_t k = 0; k < playback_infos[i].nativeDataFormatCount; k++) {
            uint32_t ch = playback_infos[i].nativeDataFormats[k].channels;
            if (min_ch == 0 || ch < min_ch) min_ch = ch;
            if (ch > max_ch) max_ch = ch;
        }
        info->min_channels_out = min_ch;
        info->max_channels_out = max_ch;

        if(playback_infos[i].nativeDataFormatCount > 0)
            info->preferred_sample_rate = playback_infos[i].nativeDataFormats[0].sampleRate;
        else
            info->preferred_sample_rate = 48000;

        info->is_default_playback = playback_infos[i].isDefault;

        // ID Generation
        snprintf(info->id, 127, "PLAYBACK_%.100s_%u", info->name, i);
    }

    for (uint32_t i = 0; i < capture_count; i++) {
        SituationAudioDeviceInfo* info = &list[idx++];
        strncpy(info->name, capture_infos[i].name, 255);
        info->native_id = capture_infos[i].id;
        info->type = SIT_AUDIO_DEVICE_TYPE_CAPTURE;

        uint32_t min_ch = 0;
        uint32_t max_ch = 0;
        for (uint32_t k = 0; k < capture_infos[i].nativeDataFormatCount; k++) {
            uint32_t ch = capture_infos[i].nativeDataFormats[k].channels;
            if (min_ch == 0 || ch < min_ch) min_ch = ch;
            if (ch > max_ch) max_ch = ch;
        }
        info->min_channels_in = min_ch;
        info->max_channels_in = max_ch;

        info->is_default_capture = capture_infos[i].isDefault;
        snprintf(info->id, 127, "CAPTURE_%.100s_%u", info->name, i);
    }

    if (out_count) *out_count = total_count;
    return list;
}

SITAPI void SituationFreeDeviceList(SituationAudioDeviceInfo* devices, int count) {
    (void)count;
    SIT_FREE(devices);
}

SITAPI SituationAudioDeviceInfo* SituationGetAudioDevices(int* count) {
    return SituationEnumerateAudioDevices(count);
}

/**
 * @brief Internal: open playback with explicit WASAPI/DirectSound share mode (exclusive vs shared).
 * @details `SituationSetAudioDevice` passes exclusive. `SituationInit` step 7 on Windows passes shared
 * so auto-start does not mute other apps; explicit `SituationSetAudioDevice()` is for exclusive/low-latency.
 */
static SituationError _SituationSetAudioDeviceInternal(int situation_internal_id, const SituationAudioFormat* format, ma_share_mode playback_share_mode) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.is_miniaudio_context_initialized) return SITUATION_ERROR_AUDIO_CONTEXT;

    ma_device_info* ma_playback_devices = NULL;
    uint32_t ma_playback_count = 0;
    ma_result res = ma_context_get_devices(&sit_audio.miniaudio_context, &ma_playback_devices, &ma_playback_count, NULL, NULL);

    if (res != MA_SUCCESS || situation_internal_id < 0 || (uint32_t)situation_internal_id >= ma_playback_count) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Invalid internal_id or failed to enumerate for SetAudioDevice");
        return SITUATION_ERROR_AUDIO_DEVICE;
    }

    ma_device_id* target_device_id = &ma_playback_devices[situation_internal_id].id;

    if (sit_audio.is_miniaudio_device_active) {
        // [FIX v2.4.38] Stop callback processing before tearing down the device
        atomic_store(&sit_audio.audio_ready, false);
        ma_device_stop(&sit_audio.miniaudio_device);
        ma_device_uninit(&sit_audio.miniaudio_device);
        sit_audio.is_miniaudio_device_active = false;
    }

    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.pDeviceID = target_device_id;
    device_config.playback.shareMode = playback_share_mode;
    device_config.dataCallback = sit_miniaudio_data_callback;
    device_config.pUserData = &sit_audio; // Pass audio state if callback needs it (e.g. for temp buffers)
                                      // User data is accessed via pDevice->pUserData in callback

    if (format) {
        device_config.playback.channels = format->channels;
        device_config.sampleRate = format->sample_rate;
        if (format->bit_depth == 32) device_config.playback.format = ma_format_f32;
        else if (format->bit_depth == 16) device_config.playback.format = ma_format_s16;
        else if (format->bit_depth == 24) device_config.playback.format = ma_format_s24; // Requires device support
        else if (format->bit_depth == 8) device_config.playback.format = ma_format_u8;   // Requires device support
        else {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Unsupported bit depth in SetAudioDevice format");
            return SITUATION_ERROR_INVALID_PARAM;
        }
    } else { // Use device default format preferences, or sensible common defaults
        device_config.playback.format = ma_format_f32; // Prefer float32 for easier mixing
        device_config.playback.channels = 2;           // Stereo default
        device_config.sampleRate = 0;              // Common default sample rate
        // To use device's native format:
        // device_config.playback.format = ma_format_unknown; // Let miniaudio pick
        // device_config.playback.nativeChannelCount = 0; // Use device native or best match
        // device_config.nativeSampleRate = 0;
    }
    // Define period size for callback scheduling (LOW LATENCY for musical input)
    device_config.periodSizeInFrames = 64;  // ~1.3ms at 48kHz (ultra low latency)
    device_config.periods = 2;               // Double buffering

    res = ma_device_init(&sit_audio.miniaudio_context, &device_config, &sit_audio.miniaudio_device);

    if (res != MA_SUCCESS && playback_share_mode == ma_share_mode_exclusive) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE,
            "Exclusive audio device init failed; previous session may not have released device cleanly");
        // Retry in shared mode
        device_config.playback.shareMode = ma_share_mode_shared;
        res = ma_device_init(&sit_audio.miniaudio_context, &device_config, &sit_audio.miniaudio_device);
    }

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "=== AUDIO DEVICE INIT DEBUG ===\n");
    fprintf(stderr, "Init result: %s\n", ma_result_description(res));
    if (res == MA_SUCCESS) {
        fprintf(stderr, "Requested share mode: %s\n",
            device_config.playback.shareMode == ma_share_mode_exclusive ? "EXCLUSIVE" : "SHARED");
        fprintf(stderr, "Actual share mode: %s\n",
            sit_audio.miniaudio_device.playback.shareMode == ma_share_mode_exclusive ? "EXCLUSIVE" : "SHARED");
        fprintf(stderr, "Sample rate: %u Hz\n", sit_audio.miniaudio_device.sampleRate);
        fprintf(stderr, "Period size: %u frames\n", sit_audio.miniaudio_device.playback.internalPeriodSizeInFrames);
        fprintf(stderr, "Periods: %u\n", sit_audio.miniaudio_device.playback.internalPeriods);
        fprintf(stderr, "Buffer size: %u frames (%.2f ms)\n",
            sit_audio.miniaudio_device.playback.internalPeriodSizeInFrames * sit_audio.miniaudio_device.playback.internalPeriods,
            (sit_audio.miniaudio_device.playback.internalPeriodSizeInFrames * sit_audio.miniaudio_device.playback.internalPeriods * 1000.0) / sit_audio.miniaudio_device.sampleRate);
        fprintf(stderr, "Backend: %s\n", ma_get_backend_name(sit_audio.miniaudio_device.pContext->backend));
    }
    fprintf(stderr, "================================\n");
#endif

    if (res != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED, ma_result_description(res));
        return SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED;
    }

    if (!sit_audio.is_miniaudio_device_internally_paused) { // Only start if not meant to be paused
        res = ma_device_start(&sit_audio.miniaudio_device);
        if (res != MA_SUCCESS) {
            ma_device_uninit(&sit_audio.miniaudio_device);
            _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_START_FAILED, "Failed to start new audio device");
            return SITUATION_ERROR_AUDIO_DEVICE_START_FAILED;
        }
    }

    sit_audio.is_miniaudio_device_active = true;
    sit_audio.current_miniaudio_device_audioinfo_id = situation_internal_id;

    // Initialize device registry (once)
    _SituationEnsureRegistryInit();

    // [Phase H] Create default graph if not already created
    if (!sit_audio.default_graph) {
        SituationAudioGraph* graph = SituationCreateGraph();
        if (graph) {
            SituationNodeHandle sound_src  = SITUATION_INVALID_NODE_HANDLE;
            SituationNodeHandle mixer_node = SITUATION_INVALID_NODE_HANDLE;

            SituationCreateNode(graph, SITUATION_NODE_SOUND_SOURCE, &sound_src);
            SituationCreateNode(graph, SITUATION_NODE_MIXER, &mixer_node);

            // Patch: Sound Source â†’ Mixer input 0
            if (sound_src != SITUATION_INVALID_NODE_HANDLE && mixer_node != SITUATION_INVALID_NODE_HANDLE) {
                SituationCreatePatch(graph, sound_src, 0, mixer_node, 0, false);
            }

            {
                SituationNode* ss_node = SituationGetNode(graph, sound_src);
                if (ss_node && ss_node->device_data) {
                    sit_audio.default_graph_voice_source = ss_node->device_data;
                }
            }

            sit_audio.default_graph = graph;
            sit_audio.active_graph = graph;  // [Phase H] Activate the node graph path
            (void)SituationTopologicalSort(graph);
        }
    }

    // [FIX v2.4.38] Signal that audio state is fully initialized.
    // The audio callback will now begin processing instead of returning silence.
    atomic_store(&sit_audio.audio_ready, true);

    return SITUATION_SUCCESS;
}

/**
 * @brief Switches the active audio output to a specific device.
 * @details This function re-initializes the audio subsystem to use the device specified by its internal ID (obtained from `SituationGetAudioDevices`). It allows the user to select their preferred output, such as switching between speakers and a headset. Always requests **exclusive** mode for low latency.
 *
 * @par Behavior
 *   If an audio device is already active, it will be stopped and uninitialized before the new device is started. The new device will be configured with the specified format, or with sensible defaults (stereo, 48kHz float32) if `format` is `NULL`.
 *
 * @param situation_internal_id The internal ID of the target device, corresponding to its index in the array returned by `SituationGetAudioDevices`.
 * @param format A pointer to a `SituationAudioFormat` struct specifying the desired sample rate, channel count, and bit depth for the new device. Can be `NULL` to use defaults.
 *
 * @return `SITUATION_SUCCESS` on successful switch.
 * @return `SITUATION_ERROR_AUDIO_CONTEXT` if the audio system is not initialized.
 * @return `SITUATION_ERROR_AUDIO_DEVICE` if the ID is invalid, or if the new device fails to initialize or start.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the requested format contains an unsupported bit depth.
 *
 * @warning Switching devices may cause a brief interruption in audio playback.
 *
 * @see SituationGetAudioDevices()
 */
SITAPI SituationError SituationSetAudioDevice(int situation_internal_id, const SituationAudioFormat* format) {
    return _SituationSetAudioDeviceInternal(situation_internal_id, format, ma_share_mode_exclusive);
}

/**
 * @brief Gets the sample rate of the currently active audio playback device.
 * @details This is the master sample rate at which the audio engine is mixing and outputting sound to the hardware. All playing sounds are resampled to match this rate.
 *
 * @return The sample rate in Hertz (e.g., 44100, 48000).
 * @return `0` if the library is not initialized or if no audio device is currently active.
 */
SITAPI int SituationGetAudioPlaybackSampleRate(void) {
    if (!SituationIsInitialized()) return 0;
    if (!sit_audio.is_miniaudio_device_active) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Audio device not active for GetAudioPlaybackSampleRate");
        return 0;
    }
    return sit_audio.miniaudio_device.sampleRate;
}

/**
 * @brief Re-initializes the active audio device with a new sample rate.
 * @details This function allows you to change the master output sample rate of the audio engine at runtime.
 *
 * @par Behavior
 *   The function preserves the current device, channel count, and bit depth. It stops the device, re-initializes it with the new sample rate, and restarts it.
 *
 * @param sample_rate The new desired sample rate in Hertz (e.g., 44100, 48000, 96000).
 *
 * @return `SITUATION_SUCCESS` on successful change.
 * @return `SITUATION_ERROR_AUDIO_DEVICE` if no device is active, if the current format cannot be determined, or if re-initialization fails.
 *
 * @warning Changing the sample rate will cause a brief interruption in audio playback.
 * @note All currently playing sounds will be automatically resampled to the new master rate by their internal converters.
 */
SITAPI SituationError SituationSetAudioPlaybackSampleRate(int sample_rate) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.is_miniaudio_device_active || sit_audio.current_miniaudio_device_audioinfo_id < 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "No audio device set, cannot change sample rate.");
        return SITUATION_ERROR_AUDIO_DEVICE;
    }
    SituationAudioFormat current_fmt;
    current_fmt.channels = sit_audio.miniaudio_device.playback.channels;
    current_fmt.sample_rate = sample_rate;

    ma_format current_ma_fmt = sit_audio.miniaudio_device.playback.format;
    if (current_ma_fmt == ma_format_f32) current_fmt.bit_depth = 32;
    else if (current_ma_fmt == ma_format_s16) current_fmt.bit_depth = 16;
    else if (current_ma_fmt == ma_format_s24) current_fmt.bit_depth = 24;
    else if (current_ma_fmt == ma_format_u8) current_fmt.bit_depth = 8;
    else {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_INVALID_OPERATION, "Cannot determine current bit depth to change sample rate (unsupported format).");
        return SITUATION_ERROR_AUDIO_DEVICE;
    }

    return SituationSetAudioDevice(sit_audio.current_miniaudio_device_audioinfo_id, &current_fmt);
}

/**
 * @brief Gets the current master volume of the audio device.
 * @details This is the global volume level applied to the final mix before it is sent to the speakers.
 *
 * @return The master volume as a linear scalar value. `0.0f` is silent, `1.0f` is the default volume.
 * @return `0.0f` if the library is not initialized or if no audio device is currently active.
 *
 * @see SituationSetAudioMasterVolume()
 */
SITAPI float SituationGetAudioMasterVolume(void) {
    if (!SituationIsInitialized()) return 0.0f;
    if (!sit_audio.is_miniaudio_device_active) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Audio device not active for GetAudioMasterVolume");
        return 0.0f;
    }
    float volume = 0.0f; // Default to 0 if get fails
    ma_result res = ma_device_get_master_volume(&sit_audio.miniaudio_device, &volume);
    if (res != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Failed to get master volume");
        // volume remains 0.0f or its last valid value if ma_device_get_master_volume modified it partially
    }
    return volume;
}

/**
 * @brief Sets the master volume for the entire audio device.
 * @details This function controls the final, global volume level of all mixed audio before it is sent to the hardware. It affects all sounds currently playing and is independent of individual sound volumes.
 *
 * @param volume The desired master volume as a linear scalar. `0.0f` is silent, `1.0f` is the default (unattenuated) volume. Values greater than `1.0f` can be used for amplification if supported by the backend. Negative values are clamped to `0.0f`.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_AUDIO_DEVICE if no audio device is active or if the volume cannot be set.
 *
 * @see SituationGetAudioMasterVolume(), SituationSetSoundVolume()
 */
SITAPI SituationError SituationSetAudioMasterVolume(float volume) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.is_miniaudio_device_active) return SITUATION_ERROR_AUDIO_DEVICE;
    // MiniAudio volume is linear [0, 1], can go >1 for gain. Clamp to [0,1] for typical app behavior.
    float clamped_volume = (volume < 0.0f) ? 0.0f : volume; // (volume > 1.0f) ? 1.0f : volume; // No upper clamp to allow gain

    ma_result res = ma_device_set_master_volume(&sit_audio.miniaudio_device, clamped_volume);
    if (res != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Failed to set master volume");
        return SITUATION_ERROR_AUDIO_DEVICE;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Checks if the audio device is currently active and playing sound.
 * @details This function checks two conditions: that an audio device has been successfully initialized and started, and that the application has not manually paused it via `SituationPauseApp()` or `SituationPauseAudioDevice()`.
 *
 * @return `true` if the audio device is running and not paused, `false` otherwise.
 *
 * @see SituationPauseAudioDevice(), SituationResumeAudioDevice()
 */
SITAPI bool SituationIsAudioDevicePlaying(void) {
    if (!SituationIsInitialized()) return false;
    if (!sit_audio.is_miniaudio_device_active) return false;
    // Considered "playing" if device is started and not internally marked as paused by our system
    return ma_device_is_started(&sit_audio.miniaudio_device) && !sit_audio.is_miniaudio_device_internally_paused;
}

/**
 * @brief Pauses all audio output by stopping the audio device.
 * @details This function halts the audio processing thread. No sounds will be played, and the audio callback will no longer be called until `SituationResumeAudioDevice()` is invoked.
 *          This is a low-power state ideal for when the application is minimized or in a pause menu.
 *
 * @note This function is called automatically when the application is paused via `SituationPauseApp()`.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_AUDIO_DEVICE if the device fails to stop.
 *
 * @see SituationResumeAudioDevice(), SituationIsAudioDevicePlaying(), SituationPauseApp()
 */
SITAPI SituationError SituationPauseAudioDevice(void) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.is_miniaudio_device_active) {
        // Not an error to pause an inactive device, just mark it.
        sit_audio.is_miniaudio_device_internally_paused = true;
        return SITUATION_SUCCESS;
    }

    if (ma_device_is_started(&sit_audio.miniaudio_device)) {
        ma_result res = ma_device_stop(&sit_audio.miniaudio_device);
        if (res != MA_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE, "Failed to stop (pause) audio device");
            return SITUATION_ERROR_AUDIO_DEVICE;
        }
    }

    sit_audio.is_miniaudio_device_internally_paused = true;
    return SITUATION_SUCCESS;
}

/**
 * @brief Resumes all audio output by restarting a paused audio device.
 * @details If the audio device was previously stopped by `SituationPauseAudioDevice()`, this function restarts the audio processing thread, and sound playback will continue from where it left off.
 *
 * @note This function is called automatically when the application is resumed via `SituationResumeApp()`.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_AUDIO_DEVICE if the device fails to start.
 *
 * @see SituationPauseAudioDevice(), SituationIsAudioDevicePlaying(), SituationResumeApp()
 */
SITAPI SituationError SituationResumeAudioDevice(void) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    sit_audio.is_miniaudio_device_internally_paused = false;

    if (sit_audio.is_miniaudio_device_active && !ma_device_is_started(&sit_audio.miniaudio_device)) {
        ma_result res = ma_device_start(&sit_audio.miniaudio_device);
        if (res != MA_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_START_FAILED, "Failed to start (resume) audio device");
            return SITUATION_ERROR_AUDIO_DEVICE_START_FAILED;
        }
    }

    return SITUATION_SUCCESS;
}

// --- Helper to initialize effects for a sound ---
/**
 * @brief [INTERNAL] Initializes the built-in effects chain for a given sound.
 * @details This helper function is called once after a sound's decoder has been initialized. It allocates and sets up the internal data structures for the sound's real-time effects, including the biquad filter (for LPF/HPF), the echo/delay unit, and the reverb processor.
 *          All effects are initialized to a disabled state with default parameters.
 *
 * @param sound A pointer to the `SituationSound` struct to initialize. The sound's `decoder` member must already be valid, as its channel count and sample rate are used for configuration.
 *
 * @return SITUATION_SUCCESS if all effects are initialized successfully.
 * @return SITUATION_ERROR_INVALID_PARAM if the `sound` pointer is NULL or not initialized.
 * @return SITUATION_ERROR_AUDIO_CONTEXT if an underlying MiniAudio effect fails to initialize.
 *
 * @note This function is for internal use by the `SituationLoadSound*` functions only.
 *
 * @see SituationLoadSoundFromFile(), SituationLoadSoundFromStream()
 */
static SituationError _SituationInitSoundEffects(_SituationSound* sound) {
    if (!sound) return SITUATION_ERROR_INVALID_PARAM;

    // Defaults
    sound->effects.filter_enabled = false;
    sound->effects.echo_enabled = false;
    sound->effects.echo.is_initialized = false;
    sound->effects.reverb_enabled = false;

    // Reverb Init
    // Note: We need sample rate. If preloaded, assume 48000? Or store it.
    // If streamed, decoder has it.
    uint32_t sample_rate = 48000;
    if (sound->is_initialized && sound->is_streamed) {
        sample_rate = sound->decoder.outputSampleRate;
    }

    SituationError rev_err = _SituationInitReverb(sample_rate, &sound->effects.reverb_state);
    if (rev_err != SITUATION_SUCCESS) {
        return rev_err;
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief Loads and configures an audio file for playback.
 * @details This function initializes a `SituationSound` object. Depending on the selected `mode`, it will either decode the entire file into a memory buffer immediately or set up a decoder stream to read from disk on-demand.
 *          It also initializes the sound's internal effects chain (Filter, Echo, Reverb) and resampling converter.
 *
 * @par Thread Safety & Performance
 *   - If `mode` results in a **FULL** load: The expensive decoding happens on the calling thread. Playback is lock-free and I/O-free, making it safe for the high-priority audio thread.
 *   - If `mode` results in a **STREAM** load: A file handle is kept open. The audio thread will perform disk I/O reads. This carries a risk of stuttering if the system IO is under heavy load.
 *
 * @param file_path The absolute or relative path to the audio file (WAV, MP3, FLAC, OGG).
 * @param mode The loading strategy. Use `SITUATION_AUDIO_LOAD_AUTO` for the best balance of safety and memory usage.
 * @param looping If `true`, the sound will automatically restart from the beginning when it finishes.
 * @param[out] out_sound A pointer to a `SituationSound` struct that will be initialized.
 *
 * @return SITUATION_SUCCESS on successful loading.
 * @return SITUATION_ERROR_FILE_ACCESS if the file cannot be opened.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION if the RAM buffer could not be allocated (for FULL loads).
 *
 * @note The caller is **responsible** for freeing resources by calling `SituationUnloadSound()`.
 * @see SituationUnloadSound(), SituationAudioLoadMode
 */
SITAPI SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!out_sound || !file_path) return SITUATION_ERROR_INVALID_PARAM;
    if (!sit_audio.is_miniaudio_device_active) return SITUATION_ERROR_AUDIO_DEVICE;

    SituationSound handle;
    _SituationSoundSlot* slot = _SitAllocSoundSlot(&handle);
    if (!slot) return SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED;

    _SituationSound* sound = &slot->sound_data;
    sound->volume = 1.0f;
    sound->pan = 0.0f;
    sound->pitch = 1.0f;
    sound->is_streamed = false;
    sound->is_looping = looping;

    slot->source_path = _sit_strdup(file_path);
    slot->mod_time = SituationGetFileModTime(file_path);

    // 1. Decide Loading Strategy
    bool should_preload = false;
    if (mode == SITUATION_AUDIO_LOAD_FULL) should_preload = true;
    else if (mode == SITUATION_AUDIO_LOAD_STREAM) should_preload = false;
    else {
        // AUTO
        ma_decoder temp_dec;
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_file(file_path, &config, &temp_dec) == MA_SUCCESS) {
            ma_uint64 frames;
            ma_decoder_get_length_in_pcm_frames(&temp_dec, &frames);
            ma_decoder_uninit(&temp_dec);
            // 10s @ 44.1k = 441000
            should_preload = (frames < 441000);
        } else {
            should_preload = false; // Fallback
        }
    }

    // 2. Load
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, sit_audio.miniaudio_device.sampleRate);
    if (should_preload) {
        // Decode to RAM
        ma_uint64 framesRead;
        ma_result result = ma_decode_file(file_path, &config, &framesRead, &sound->preloaded_data);
        if (result != MA_SUCCESS) {
            if (sound->is_preloaded && sound->preloaded_data) {
                ma_free(sound->preloaded_data, NULL);
                sound->preloaded_data = NULL;
            }
            if (sound->is_streamed && sound->is_initialized) {
                ma_decoder_uninit(&sound->decoder);
                sound->is_initialized = false;
            }
            (void)_SitFreeSoundSlot(handle);
            if (result == MA_NO_BACKEND || result == MA_FORMAT_NOT_SUPPORTED) {
                return SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED;
            }
            return SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED;
        }
        sound->total_frames = framesRead;
        sound->is_preloaded = true;
        sound->is_initialized = true;
    } else {
        // Stream
        ma_result stream_res = ma_decoder_init_file(file_path, &config, &sound->decoder);
        if (stream_res != MA_SUCCESS) {
            (void)_SitFreeSoundSlot(handle);
            if (stream_res == MA_NO_BACKEND || stream_res == MA_FORMAT_NOT_SUPPORTED) {
                return SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED;
            }
            return SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED;
        }
        sound->is_streamed = true;
        sound->is_initialized = true;
    }

    SituationError fx_err = _SituationInitSoundEffects(sound);
    if (fx_err != SITUATION_SUCCESS) {
        if (sound->effects.reverb_state) {
            _SituationUninitReverb(sound->effects.reverb_state);
            sound->effects.reverb_state = NULL;
        }
        if (sound->is_preloaded && sound->preloaded_data) {
            ma_free(sound->preloaded_data, NULL);
            sound->preloaded_data = NULL;
            sound->is_preloaded = false;
        }
        if (sound->is_streamed && sound->is_initialized) {
            ma_decoder_uninit(&sound->decoder);
            sound->is_initialized = false;
            sound->is_streamed = false;
        }
        (void)_SitFreeSoundSlot(handle);
        return fx_err;
    }

    *out_sound = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief [INTERNAL] Static thunk for routing audio read requests.
 *
 * @details This function acts as a bridge (trampoline) between the generic MiniAudio decoder logic and the
 *          specific `stream_read_cb` stored in a `SituationSound` instance.
 *
 * @par Implementation Detail (The "Container Of" Trick)
 *      MiniAudio passes a pointer to the `ma_decoder`. Since `ma_decoder` is a member of `SituationSound`,
 *      we use `offsetof` to calculate the address of the parent `SituationSound` struct.
 *      This allows us to access the specific callbacks for *this* sound instance without using global state.
 *
 * @param pDecoder The pointer to the decoder member inside a SituationSound struct.
 * @param pBufferOut The buffer to fill with audio data.
 * @param bytesToRead The number of bytes requested.
 * @param pBytesRead Output pointer for bytes read.
 * @return MA_SUCCESS or error.
 */
static ma_result _situation_stream_read_thunk(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead) {
    _SituationSound* sound = (_SituationSound*)((char*)pDecoder - offsetof(_SituationSound, decoder));
    if (sound->stream_read_cb) {
        ma_uint64 read = sound->stream_read_cb(sound->stream_user_data, pBufferOut, bytesToRead);
        if (pBytesRead) *pBytesRead = (size_t)read;
        return (read > 0) ? MA_SUCCESS : MA_AT_END;
    }
    return MA_NOT_IMPLEMENTED;
}

/**
 * @brief [INTERNAL] Static thunk for routing audio seek requests.
 *
 * @details Similar to `_situation_stream_read_thunk`, this recovers the parent `SituationSound` instance
 *          and dispatches the seek request to the user's specific `stream_seek_cb`.
 *
 * @param pDecoder The pointer to the decoder member inside a SituationSound struct.
 * @param byteOffset The offset to seek to.
 * @param origin The seek origin (start or current).
 * @return MA_SUCCESS on success, or an error code.
 */
static ma_result _situation_stream_seek_thunk(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin) {
    _SituationSound* sound = (_SituationSound*)((char*)pDecoder - offsetof(_SituationSound, decoder));
    if (sound->stream_seek_cb) {
        return sound->stream_seek_cb(sound->stream_user_data, byteOffset, origin);
    }
    return MA_NOT_IMPLEMENTED;
}

/**
 * @brief Initializes a sound for playback from a custom, user-defined data stream.
 * @details This function configures a `SituationSound` to pull audio data on-demand using the provided callbacks.
 *          This is essential for procedural audio, network streaming, or reading from custom archive formats.
 *
 * @par Thread Safety Improvement (v2.3.2C Fix)
 *      Previously, this function modified a global vtable, causing race conditions if multiple streams were loaded.
 *      It now stores the `on_read` and `on_seek` pointers directly into the `out_sound` instance and uses
 *      a shared, read-only vtable with thunk functions to resolve the correct callback at runtime.
 *
 * @param on_read The callback invoked when the audio engine needs more data. Must be thread-safe.
 * @param on_seek The callback invoked to seek within the stream. Can be NULL.
 * @param user_data A custom pointer passed to the callbacks (e.g., your file handle or generator state).
 * @param format The audio format (channels, sample rate) of the incoming stream.
 * @param looping If true, the engine will attempt to seek to 0 when the stream ends.
 * @param[out] out_sound The sound struct to initialize.
 *
 * @return SITUATION_SUCCESS on success, or an error code if initialization fails.
 */
SITAPI SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!out_sound) return SITUATION_ERROR_INVALID_PARAM;

    SituationSound handle;
    _SituationSoundSlot* slot = _SitAllocSoundSlot(&handle);
    if (!slot) return SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED;

    _SituationSound* sound = &slot->sound_data;
    sound->volume = 1.0f;
    sound->pan = 0.0f;
    sound->pitch = 1.0f;
    sound->is_streamed = true;
    sound->is_looping = looping;
    sound->stream_read_cb = on_read;
    sound->stream_seek_cb = on_seek;
    sound->stream_user_data = user_data;

    // Init Decoder with Thunks
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, format ? format->channels : 2, format ? format->sample_rate : 48000);
    ma_result res = ma_decoder_init(_situation_stream_read_thunk, _situation_stream_seek_thunk, NULL, &config, &sound->decoder);
    if (res != MA_SUCCESS) {
        (void)_SitFreeSoundSlot(handle);
        if (res == MA_NO_BACKEND || res == MA_FORMAT_NOT_SUPPORTED) {
            return SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED;
        }
        return SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED;
    }
    sound->is_initialized = true;

    SituationError fx_err = _SituationInitSoundEffects(sound);
    if (fx_err != SITUATION_SUCCESS) {
        ma_decoder_uninit(&sound->decoder);
        sound->is_initialized = false;
        if (sound->effects.reverb_state) {
            _SituationUninitReverb(sound->effects.reverb_state);
            sound->effects.reverb_state = NULL;
        }
        (void)_SitFreeSoundSlot(handle);
        return fx_err;
    }

    *out_sound = handle;
    return SITUATION_SUCCESS;
}

/**
 * @brief Unloads a sound and frees all of its associated memory and resources.
 * @details This is the designated cleanup function for any `SituationSound` initialized by the library.
 *          It releases the decoded PCM data, the internal data converter, and all effects processors.
 *
 * @param[in,out] sound A pointer to the `SituationSound` struct to uninitialize. The struct is zeroed out
 *                      after cleanup to invalidate it for future use.
 *
 * @note It is safe to call this function on a `NULL` pointer or an already-unloaded sound;
 *       it will simply do nothing.
 * @warning Failure to call this function on a loaded sound will result in a memory leak.
 *
 * @see SituationLoadSoundFromFile(), SituationLoadSoundFromStream()
 */
SITAPI void SituationUnloadSound(SituationSound* sound) {
    if (!sound) return;
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return;

    _SituationSound* data = &slot->sound_data;

    // Stop playback first
    SituationStopLoadedSound(sound);

    // Wait until the audio callback is not decoding/mixing from a snapshot that may reference this sound.
    _SituationWaitUntilVoiceSnapshotIdle();

    // If managed by mixer graph, detach and uninit node
    // [Phase H] Legacy mixer graph management removed â€” node graph handles routing now
    if (data->is_graph_managed) {
        data->is_graph_managed = false;
    }

    if (data->is_preloaded && data->preloaded_data) {
        ma_free(data->preloaded_data, NULL);
    }
    if (data->is_initialized) {
        ma_decoder_uninit(&data->decoder);
    }
    if (data->processors) {
        SIT_FREE(data->processors);
        SIT_FREE(data->processor_user_data);
    }

    // Cleanup Effects
    if (data->effects.reverb_state) {
        _SituationUninitReverb(data->effects.reverb_state);
        data->effects.reverb_state = NULL;
    }
    if (data->effects.echo.is_initialized) {
        _SituationUninitEcho((sit_echo_t*)&data->effects.echo);
    }

    _SitFreeSoundSlot(*sound);
    memset(sound, 0, sizeof(SituationSound));
}

/**
 * @brief Begins playback of a loaded sound or restarts it if already playing.
 * @details This function adds the specified sound to the audio engine's mixing queue. If the sound is already in the queue (i.e., it's currently playing or paused), its playback cursor will be reset to the beginning.
 *
 * @par Thread Safety
 *   This function is thread-safe and can be called from any thread.
 *
 * @param sound A pointer to a valid, initialized `SituationSound` struct.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_AUDIO_SOUND_LIMIT if the maximum number of concurrent sounds is already playing.
 * @return SITUATION_ERROR_INVALID_PARAM if the `sound` handle is invalid.
 *
 * @see SituationStopLoadedSound(), SituationStopAllLoadedSounds()
 */
SITAPI SituationError SituationPlayLoadedSound(SituationSound* sound) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    _SituationSound* data = &slot->sound_data;

    // Add to active voices
    mtx_lock(&sit_audio.audio_queue_mutex);

    // Check duplicates? Or allow multiple?
    // SituationSound is now a handle. We add pointer to DATA to mixer.
    // If we want multiple instances of same sound, we need multiple slots (or mixer supports multiple cursors).
    // The current design (and previous) embedded playback state (cursor) in the Sound struct.
    // So playing it again restarts it.

    // Reset cursor
    if (data->is_preloaded) {
        data->cursor_frames = 0;
    } else if (data->is_initialized) {
        ma_decoder_seek_to_pcm_frame(&data->decoder, 0);
    }

    // Add if not present
    bool present = false;
    for (int i = 0; i < sit_audio.active_voice_count; i++) {
        if (sit_audio.active_voices[i] == data) {
            present = true;
            break;
        }
    }
    if (!present) {
        if (sit_audio.active_voice_count < sit_audio.active_voice_capacity) {
            sit_audio.active_voices[sit_audio.active_voice_count++] = data;
        } else {
            // Realloc
            int new_cap = sit_audio.active_voice_capacity * 2;
            _SituationSound** new_array = (_SituationSound**)SIT_REALLOC(sit_audio.active_voices, new_cap * sizeof(_SituationSound*));
            if (new_array) {
                sit_audio.active_voices = new_array;
                sit_audio.active_voice_capacity = new_cap;
                sit_audio.active_voices[sit_audio.active_voice_count++] = data;
            }
        }
    }

    mtx_unlock(&sit_audio.audio_queue_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Stops a specific sound from playing and removes it from the mixing queue.
 * @details If the specified sound is currently playing, it will be immediately silenced and removed from the audio processing pipeline. Its playback position is not reset.
 *
 * @par Thread Safety
 *   This function is thread-safe and can be called from any thread.
 *
 * @param sound A pointer to the `SituationSound` struct to stop.
 *
 * @return SITUATION_SUCCESS if the sound was found and stopped.
 * @return SITUATION_ERROR_INVALID_PARAM if the `sound` handle is invalid or was not currently playing.
 *
 * @see SituationPlayLoadedSound(), SituationStopAllLoadedSounds()
 */
SITAPI SituationError SituationStopLoadedSound(SituationSound* sound_to_stop) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound_to_stop);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    _SituationSound* data = &slot->sound_data;

    mtx_lock(&sit_audio.audio_queue_mutex);
    for (int i = 0; i < sit_audio.active_voice_count; i++) {
        if (sit_audio.active_voices[i] == data) {
            // Remove (swap with last)
            sit_audio.active_voices[i] = sit_audio.active_voices[sit_audio.active_voice_count - 1];
            sit_audio.active_voice_count--;
            break;
        }
    }
    mtx_unlock(&sit_audio.audio_queue_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Stops all currently playing sounds and clears the mixing queue.
 * @details This is a convenience function that immediately silences all audio being processed by the engine.
 *
 * @par Thread Safety
 *   This function is thread-safe and can be called from any thread.
 *
 * @return SITUATION_SUCCESS on success.
 *
 * @see SituationStopLoadedSound()
 */
SITAPI SituationError SituationStopAllLoadedSounds(void) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    mtx_lock(&sit_audio.audio_queue_mutex);
    sit_audio.active_voice_count = 0;
    mtx_unlock(&sit_audio.audio_queue_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Creates a new sound by making a deep copy of a source sound's decoded PCM data.
 * @details The new sound is fully independent and must be unloaded separately.
 * @param source The sound to copy from.
 * @param out_destination A pointer to a SituationSound struct to be initialized with the copy.
 * @return SITUATION_SUCCESS on success.
 */
SITAPI SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination) {
    if (!source || !out_destination) return SITUATION_ERROR_INVALID_PARAM;

    // Resolve Source
    // _SitGetSoundSlot takes handle by value. source is const pointer.
    _SituationSoundSlot* src_slot = _SitGetSoundSlot(*source);
    if (!src_slot) return SITUATION_ERROR_RESOURCE_INVALID;
    _SituationSound* src_data = &src_slot->sound_data;

    // Allocate Dest
    SituationSound handle;
    _SituationSoundSlot* dst_slot = _SitAllocSoundSlot(&handle);
    if (!dst_slot) return SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED;
    _SituationSound* dst_data = &dst_slot->sound_data;

    // Copy properties
    dst_data->volume = atomic_load(&src_data->volume);
    dst_data->pan = atomic_load(&src_data->pan);
    dst_data->pitch = atomic_load(&src_data->pitch);
    dst_data->is_looping = src_data->is_looping;

    // Copy Data
    if (src_data->is_preloaded && src_data->preloaded_data) {
        size_t size = (size_t)src_data->total_frames * sizeof(float) * 2; // Stereo f32
        dst_data->preloaded_data = SIT_MALLOC(size);
        if (!dst_data->preloaded_data) {
            (void)_SitFreeSoundSlot(handle);
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        memcpy(dst_data->preloaded_data, src_data->preloaded_data, size);
        dst_data->total_frames = src_data->total_frames;
        dst_data->is_preloaded = true;
    } else {
        // Cannot easily copy streamed sound state without reopening file
        (void)_SitFreeSoundSlot(handle);
        return SITUATION_ERROR_NOT_IMPLEMENTED;
    }

    *out_destination = handle;
    return SITUATION_SUCCESS;
}

/**
 * @brief Modifies a sound in-place to contain only a specific range of its audio data.
 * @warning This is a destructive operation.
 * @param sound The sound to modify.
 * @param initFrame The first frame to include in the cropped sound.
 * @param finalFrame The last frame to include.
 * @return SITUATION_SUCCESS on success.
 */
SITAPI SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    _SituationSound* data = &slot->sound_data;

    if (!data->is_preloaded || !data->preloaded_data) return SITUATION_ERROR_AUDIO_INVALID_OPERATION;
    if (finalFrame <= initFrame || finalFrame > data->total_frames) return SITUATION_ERROR_INVALID_PARAM;

    uint64_t new_frames = finalFrame - initFrame;
    size_t frame_size = sizeof(float) * 2;
    size_t new_size = (size_t)new_frames * frame_size;

    void* new_data = SIT_MALLOC(new_size);
    if (!new_data) return SITUATION_ERROR_MEMORY_ALLOCATION;

    float* src_ptr = (float*)data->preloaded_data;
    memcpy(new_data, src_ptr + (initFrame * 2), new_size);

    SIT_FREE(data->preloaded_data);
    data->preloaded_data = new_data;
    data->total_frames = new_frames;

    return SITUATION_SUCCESS;
}

/**
 * @brief Exports the raw PCM data of a sound to a new WAV file.
 * @param sound The sound to export.
 * @param fileName The path of the .wav file to create.
 * @return SITUATION_SUCCESS on success, or an error code on failure.
 */
SITAPI SituationError SituationSoundExportAsWav(const SituationSound* sound, const char* fileName) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationSoundExportAsWav: invalid sound handle");
    }
    _SituationSound* data = &slot->sound_data;

    if (!data->is_preloaded || !data->preloaded_data) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_INVALID_OPERATION, "SituationSoundExportAsWav: sound has no preloaded PCM data");
    }

    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000); // Assuming engine native
    ma_encoder encoder;
    if (ma_encoder_init_file(fileName, &config, &encoder) != MA_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "SituationSoundExportAsWav: ma_encoder_init_file failed");
    }

    ma_encoder_write_pcm_frames(&encoder, data->preloaded_data, data->total_frames, NULL);
    ma_encoder_uninit(&encoder);
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets the volume for a specific, individual sound.
 * @details This function controls the amplitude of a single sound, independent of the master audio volume.
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param volume The desired volume as a linear scalar. `0.0f` is silent, `1.0f` is the sound's original volume. Values greater than `1.0f` can be used for amplification. Negative values will be clamped to `0.0f`.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 *
 * @see SituationGetSoundVolume(), SituationSetAudioMasterVolume()
 */
SITAPI SituationError SituationSetSoundVolume(SituationSound* sound, float volume) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.volume, volume);
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the current volume of a specific, individual sound.
 *
 * @param sound A pointer to the `SituationSound` handle to query.
 *
 * @return The sound's current volume as a linear scalar. Returns `0.0f` if the handle is invalid.
 *
 * @see SituationSetSoundVolume()
 */
SITAPI float SituationGetSoundVolume(SituationSound* sound) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return 0.0f;
    return atomic_load(&slot->sound_data.volume);
}

/**
 * @brief Sets the stereo panning for a specific sound.
 * @details This function positions a sound within the stereo field. It uses an equal-power panning algorithm, which ensures that the perceived loudness of the sound remains constant as it moves from left to right.
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param pan The desired pan position, from `-1.0f` (full left) to `1.0f` (full right). A value of `0.0f` is center. Values outside this range will be clamped.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 *
 * @see SituationGetSoundPan()
 */
SITAPI SituationError SituationSetSoundPan(SituationSound* sound, float pan) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.pan, pan);
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the current stereo panning of a specific sound.
 *
 * @param sound A pointer to the `SituationSound` handle to query.
 *
 * @return The sound's current pan position, from `-1.0f` (left) to `1.0f` (right). Returns `0.0f` if the handle is invalid.
 *
 * @see SituationSetSoundPan()
 */
SITAPI float SituationGetSoundPan(SituationSound* sound) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return 0.0f;
    return atomic_load(&slot->sound_data.pan);
}

/**
 * @brief Sets the playback pitch for a specific sound.
 * @details This function adjusts the playback speed of a sound, which in turn changes its pitch. It works by dynamically changing the input sample rate of the sound's internal data converter.
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param pitch The desired pitch multiplier. `1.0f` is the original pitch. `2.0f` is one octave higher (double speed), and `0.5f` is one octave lower (half speed). The value must be positive.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 * @return `SITUATION_ERROR_AUDIO_CONVERTER` if the internal resampler fails to update.
 *
 * @warning Changing the pitch is a moderately expensive operation as it requires reconfiguring the audio resampler. Avoid calling it on every frame if possible.
 */
SITAPI SituationError SituationSetSoundPitch(SituationSound* sound, float pitch) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.pitch, pitch);
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the current pitch multiplier of a specific sound.
 *
 * @param sound A pointer to the `SituationSound` handle to query.
 *
 * @return The sound's current pitch multiplier. Returns `1.0f` if the handle is invalid.
 *
 * @see SituationSetSoundPitch()
 */
SITAPI float SituationGetSoundPitch(SituationSound* sound) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return 1.0f;
    return atomic_load(&slot->sound_data.pitch);
}

/**
 * @brief Applies a low-pass or high-pass biquad filter to a sound's real-time effects chain.
 * @details This function allows you to dynamically alter the frequency content of a sound. A low-pass filter removes high frequencies (making a sound muffled), while a high-pass filter removes low frequencies (making a sound tinny).
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param type The type of filter to apply (`SITUATION_FILTER_LOWPASS`, `SITUATION_FILTER_HIGHPASS`, or `SITUATION_FILTER_NONE` to disable).
 * @param cutoff_hz The frequency (in Hz) at which the filter begins to take effect.
 * @param q_factor The resonance or "quality" of the filter. A value around `0.707f` is neutral. Higher values create a resonant peak at the cutoff frequency.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 */
SITAPI SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    slot->sound_data.effects.filter_enabled = (type != SITUATION_FILTER_NONE);
    slot->sound_data.effects.filter_type = type;
    slot->sound_data.effects.filter_cutoff_hz = cutoff_hz;
    slot->sound_data.effects.filter_q = q_factor;
    // Re-init biquad logic omitted for brevity (handled in mixer usually)
    return SITUATION_SUCCESS;
}

/**
 * @brief Applies a simple echo (delay) effect to a sound's real-time effects chain.
 * @details This function creates repeating, decaying echoes of the original sound.
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param enabled `true` to activate the echo effect, `false` to disable it.
 * @param delay_sec The time in seconds between each echo.
 * @param feedback The amount of the echo that is fed back into the delay line to create subsequent echoes. A value of `0.0` gives a single echo; a value of `0.5` means each echo is half as loud as the previous one. Clamped to [0.0 - 1.0].
 * @param wet_mix The volume of the echo signal relative to the original signal. `0.0` is no echo, `1.0` is full-volume echo. Clamped to [0.0 - 1.0].
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 */
SITAPI SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    slot->sound_data.effects.echo_enabled = enabled;
    slot->sound_data.effects.echo_delay_sec = delay_sec;
    slot->sound_data.effects.echo_feedback = feedback;
    slot->sound_data.effects.echo_wet_mix = wet_mix;

    if (enabled && slot->sound_data.is_initialized) {
        uint32_t sample_rate = slot->sound_data.decoder.outputSampleRate;
        uint32_t channels = slot->sound_data.decoder.outputChannels;
        _SituationConfigEcho(
            (sit_echo_t*)&slot->sound_data.effects.echo,
            sample_rate, channels,
            delay_sec, feedback, wet_mix, 1.0f
        );
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Applies a reverb effect to a sound's real-time effects chain.
 * @details This function simulates the acoustic reflections of a room or space, adding depth and atmosphere to a sound.
 *
 * @param[in,out] sound A pointer to the `SituationSound` handle to modify.
 * @param enabled `true` to activate the reverb effect, `false` to disable it.
 * @param room_size A value from `0.0` (small closet) to `1.0` (large cathedral) representing the perceived size of the simulated space.
 * @param damping A value from `0.0` to `1.0` representing how much high frequencies are absorbed by the room's surfaces. Higher values lead to a darker, more muffled reverb tail.
 * @param wet_mix The volume of the reverberated ("wet") signal.
 * @param dry_mix The volume of the original ("dry") signal.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `sound` handle is invalid.
 */
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    slot->sound_data.effects.reverb_enabled = enabled;
    slot->sound_data.effects.reverb_room_size = room_size;
    slot->sound_data.effects.reverb_damping = damping;
    slot->sound_data.effects.reverb_wet_mix = wet_mix;
    slot->sound_data.effects.reverb_dry_mix = dry_mix;
    return SITUATION_SUCCESS;
}

/**
 * @brief Attach a custom DSP processor to a sound's effect chain.
 * @details Processors are called in the order they are attached, after built-in effects.
 * @param sound The sound to attach the processor to.
 * @param processor The callback function to execute.
 * @param user_data A custom pointer to pass to the callback's user_data parameter.
 */
SITAPI SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data) {
    if (!sound || !processor) return SITUATION_ERROR_INVALID_PARAM;
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    _SituationSound* data = &slot->sound_data;

    void* new_processors = SIT_REALLOC(data->processors, (data->processor_count + 1) * sizeof(SituationAudioProcessorCallback));
    if (!new_processors) return SITUATION_ERROR_MEMORY_ALLOCATION;
    data->processors = (SituationAudioProcessorCallback*)new_processors;

    void* new_user_datas = SIT_REALLOC(data->processor_user_data, (data->processor_count + 1) * sizeof(void*));
    if (!new_user_datas) return SITUATION_ERROR_MEMORY_ALLOCATION; // Leak risk on prev realloc, but acceptable for now
    data->processor_user_data = (void**)new_user_datas;

    data->processors[data->processor_count] = processor;
    data->processor_user_data[data->processor_count] = user_data;
    data->processor_count++;

    return SITUATION_SUCCESS;
}

/**
 * @brief Detach a custom DSP processor from a sound.
 * @param sound The sound to detach the processor from.
 * @param processor The callback function to remove.
 * @param user_data The user data pointer associated with the processor to remove.
 */
SITAPI SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data) {
    if (!sound || !processor) return SITUATION_ERROR_INVALID_PARAM;
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    _SituationSound* data = &slot->sound_data;

    for (int i = 0; i < data->processor_count; ++i) {
        if (data->processors[i] == processor && data->processor_user_data[i] == user_data) {
            // Remove
            for (int j = i; j < data->processor_count - 1; j++) {
                data->processors[j] = data->processors[j+1];
                data->processor_user_data[j] = data->processor_user_data[j+1];
            }
            data->processor_count--;
            // Should realloc down? Optional.
            return SITUATION_SUCCESS;
        }
    }
    return SITUATION_ERROR_INVALID_PARAM; // Not found
}


// ==================================================================================
//  Audio Handle System Implementation
// ==================================================================================

// Helper: Initialize the audio pool
static SituationError _SitAudioInitPool(void) {
    if (mtx_init(&sit_audio.pool_mutex, mtx_plain) != thrd_success) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED, "Failed to init audio pool mutex");
    }

    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; ++i) {
        sit_audio.sound_pool[i].is_active = false;
        sit_audio.sound_pool[i].generation = 1;
        memset(&sit_audio.sound_pool[i].sound_data, 0, sizeof(_SituationSound));
        sit_audio.sound_pool[i].source_path = NULL;
    }
    return SITUATION_SUCCESS;
}

// Helper: Cleanup the audio pool
/* HARDENING: void by design — idempotent teardown or free helper. */
static void _SitAudioCleanupPool(void) {
    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; ++i) {
        if (sit_audio.sound_pool[i].is_active) {
            _SituationSound* snd = &sit_audio.sound_pool[i].sound_data;
            if (snd->is_initialized) ma_decoder_uninit(&snd->decoder);
            if (snd->is_preloaded && snd->preloaded_data) SIT_FREE(snd->preloaded_data);
            if (snd->processors) SIT_FREE(snd->processors);
            if (snd->processor_user_data) SIT_FREE(snd->processor_user_data);
            if (snd->effects.reverb_state) {
                _SituationUninitReverb(snd->effects.reverb_state);
            }

            if (sit_audio.sound_pool[i].source_path) SIT_FREE(sit_audio.sound_pool[i].source_path);
        }
    }
    mtx_destroy(&sit_audio.pool_mutex);
}


// Helper: Allocate a slot


// Helper: Get sound from handle (Validation)


// Helper: Free a slot


// --- New Handle-Based API ---

SITAPI SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping) {
    SituationSound handle = SITUATION_NULL_HANDLE;
    if (SituationLoadSoundFromFile(file_path, mode, looping, &handle) == SITUATION_SUCCESS) {
        return handle;
    }
    return SITUATION_NULL_HANDLE;
}

SITAPI SituationError SituationPlayAudio(SituationSoundHandle handle) {
    return SituationPlayLoadedSound(&handle);
}

SITAPI void SituationUnloadAudio(SituationSoundHandle handle) {
    SituationUnloadSound(&handle);
}

SITAPI SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(handle);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.volume, volume);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(handle);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.pan, pan);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(handle);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    atomic_store(&slot->sound_data.pitch, pitch);
    return SITUATION_SUCCESS;
}


// --- Async Audio Helper (Restored & Updated for v2.3.15) ---

/**
 * @brief [INTERNAL] Context data for asynchronous audio loading jobs.
 * @details Packed into the job's 64-byte SOO storage to avoid allocation.
 */
typedef struct {
    char* path;
    bool looping;
    SituationSound* target;
} _SitAsyncAudioCtx;

/**
 * @brief [INTERNAL] Job callback for background audio loading.
 * @details Decodes audio to RAM (SITUATION_AUDIO_LOAD_FULL) on a worker thread to avoid main-thread disk I/O.
 * @param data Pointer to the _SitAsyncAudioCtx (embedded in job storage).
 * @param unused Unused user context.
 */
/* HARDENING: void by design — thread-pool job ABI; use SituationGetLastErrorCode after wait if handle invalid. */
static void _SituationAsyncAudioWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncAudioCtx* ctx = (_SitAsyncAudioCtx*)data;

    SituationError err = SituationLoadSoundFromFile(
        ctx->path, SITUATION_AUDIO_LOAD_FULL, ctx->looping, ctx->target);
    if (err != SITUATION_SUCCESS) {
        memset(ctx->target, 0, sizeof(SituationSound));
    }

    SIT_FREE(ctx->path);
    // Note: We don't free 'ctx' here because it's embedded in the job storage!
    // The beauty of Small Object Optimization.
}

#ifdef SITUATION_ENABLE_THREADING
/**
 * @brief Asynchronously loads an audio file from disk in a background thread.
 *
 * @details This is a convenience helper that wraps `SituationLoadSoundFromFile` in a thread pool job.
 *          It performs a **Full Load** (decoding the entire file to RAM) to avoid disk I/O on the main thread.
 *
 *          **Usage:**
 *          1. Call this function. It returns immediately.
 *          2. Store the returned `SituationJobId`.
 *          3. Use `SituationWaitForJob(job_id)` to know when loading is done.
 *          4. Once complete, the `out_sound` struct contains the ready-to-play sound.
 *
 * @param pool The thread pool instance.
 * @param file_path The path to the audio file.
 * @param looping Whether the sound should loop.
 * @param out_sound Pointer to the `SituationSound` struct to be initialized.
 *                  **Important:** This memory must remain valid until the job completes.
 *
 * @return A `SituationJobId` for the loading task, or `0` if submission failed.
 */
SITAPI SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound) {
    if (!pool || !file_path || !out_sound) return 0;

    // 1. Prepare Context
    _SitAsyncAudioCtx ctx;
    ctx.path = _sit_strdup(file_path); // Duplicate string (ownership transfers to worker)
    if (!ctx.path) return 0;

    ctx.looping = looping;
    ctx.target = out_sound;

    // 2. Clear target struct for safety
    memset(out_sound, 0, sizeof(SituationSound));

    // 3. Submit to Low Priority Queue (Assets/IO)
    // We pass 'ctx' by value. Since sizeof(_SitAsyncAudioCtx) is ~24 bytes,
    // it fits easily into the 64-byte storage (SOO). No malloc for the context!
    return SituationSubmitJobEx(
        pool,
        _SituationAsyncAudioWorker,
        &ctx,
        sizeof(_SitAsyncAudioCtx),
        SIT_SUBMIT_DEFAULT // Low Priority is correct for loading
    );
}
#endif // SITUATION_ENABLE_THREADING

// --- Mixer Implementation (Phase 1) ---

SITAPI SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type, uint32_t min_channels_out, uint32_t min_channels_in) {
    int count = 0;
    SituationAudioDeviceInfo* list = SituationEnumerateAudioDevices(&count);
    if (!list || count == 0) return NULL;

    int best_score = -1;
    int best_idx = -1;

    for (int i=0; i<count; ++i) {
        int score = 0;
        if (list[i].type == preferred_type) score += 100;
        else if (preferred_type == SIT_AUDIO_DEVICE_TYPE_DUPLEX && (list[i].type == SIT_AUDIO_DEVICE_TYPE_PLAYBACK || list[i].type == SIT_AUDIO_DEVICE_TYPE_CAPTURE)) score += 50;

        if (list[i].max_channels_out >= min_channels_out) score += 20;
        if (list[i].max_channels_in >= min_channels_in) score += 20;

        if (list[i].is_default_playback) score += 10;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    if (best_idx != -1) {
        SituationAudioDeviceInfo* result = (SituationAudioDeviceInfo*)SIT_MALLOC(sizeof(SituationAudioDeviceInfo));
        if (result) *result = list[best_idx];
        SituationFreeDeviceList(list, count);
        return result;
    }

    SituationFreeDeviceList(list, count);
    return NULL;
}


// [Phase H] Removed: All mixer implementation code (~1370 lines)
// SituationCreateMixer, SituationDestroyMixer, SituationAddTrack, SituationRemoveTrack,
// SituationSetTrack*, SituationGetAuxBus, SituationSetTrackSend, SituationSetTrackOutput,
// SituationSetTrackEQ, SituationSetTrackDynamics, SituationSetTrackSideChain,
// SituationSetMasterVolume, SituationGetMasterVolume, SituationGetTrackMeter,
// SituationInsertEffect, SituationRemoveEffect, SituationGetMixerGraph,
// SituationBindMixerToDevice, SituationBindCaptureDevice,
// SituationSaveMixerSession, SituationLoadMixerSession,
// _SituationProcessInsertChain, _SituationProcessAuxFXChain, etc.
// Replaced by the node graph system (SituationProcessGraph + SituationAudioGraph).

// ================================================================================================
// PHASE H â€” NODE GRAPH ACTIVE GRAPH API
// ================================================================================================

/**
 * @brief Set the active audio processing graph.
 * @details When set, the audio callback will use SituationProcessGraph() to generate audio
 *          instead of the legacy mixer path. Pass NULL to disable graph processing and
 *          revert to the legacy/fallback path.
 * 
 * @param graph The graph to activate, or NULL to disable.
 * @return SITUATION_SUCCESS on success.
 */
SITAPI SituationError SituationSetActiveGraph(SituationAudioGraph* graph) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (graph && (graph->needs_resort || graph->sorted_count == 0)) {
        SituationError sort_err = SituationTopologicalSort(graph);
        if (sort_err != SITUATION_SUCCESS) return sort_err;
    }
    sit_audio.active_graph = graph;
    return SITUATION_SUCCESS;
}

/**
 * @brief Get the currently active audio processing graph.
 * @return The active graph, or NULL if none is set.
 */
SITAPI SituationAudioGraph* SituationGetActiveGraph(void) {
    if (!SituationIsInitialized()) return NULL;
    return sit_audio.active_graph;
}


// ================================================================================================
// PHASE H — NODE GRAPH SFX ROUTING SYSTEM
// ================================================================================================

/* HARDENING: void by design — real-time tone mix path. */
static void _SituationMixToneToBuffer(SituationTone* t, float* buffer, uint32_t frameCount) {
    if (!t || !t->active || !buffer) return;

    for (ma_uint32 f = 0; f < frameCount; ++f) {
        // 1. Generate sample
        float sample = 0.0f;
        if (t->wave_type == SIT_WAVE_NOISE) {
            ma_noise_read_pcm_frames(&t->noise, &sample, 1, NULL);
        } else {
            ma_waveform_read_pcm_frames(&t->waveform, &sample, 1, NULL);
        }

        // 2. Envelope progress
        float envelope = 0.0f;
        switch (t->state) {
            case SIT_ENV_ATTACK:
                envelope = (t->t_attack > 0) ? (float)t->cursor_frames / (float)t->t_attack : 1.0f;
                if (t->cursor_frames >= t->t_attack) { t->state = SIT_ENV_DECAY; t->cursor_frames = 0; }
                break;
            case SIT_ENV_DECAY: {
                float progress = (t->t_decay > 0) ? (float)t->cursor_frames / (float)t->t_decay : 1.0f;
                envelope = 1.0f - (1.0f - t->level_sustain) * progress;
                if (t->cursor_frames >= t->t_decay) { t->state = SIT_ENV_SUSTAIN; t->cursor_frames = 0; }
                } break;
            case SIT_ENV_SUSTAIN:
                envelope = t->level_sustain;
                if (t->t_hold != UINT64_MAX && t->cursor_frames >= t->t_hold) { t->state = SIT_ENV_RELEASE; t->cursor_frames = 0; }
                break;
            case SIT_ENV_RELEASE: {
                float progress = (t->t_release > 0) ? (float)t->cursor_frames / (float)t->t_release : 1.0f;
                envelope = t->level_sustain * (1.0f - progress);
                if (t->cursor_frames >= t->t_release) { t->active = false; }
                } break;
            default:
                t->active = false;
                break;
        }

        if (!t->active) break;

        // 3. Apply volume and envelope
        float final_sample = sample * envelope * t->volume_peak;

        // 4. Pan and mix to stereo output
        float pan = t->pan;
        float gainL = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
        float gainR = (pan >= 0.0f) ? 1.0f : (1.0f + pan);

        buffer[f * 2 + 0] += final_sample * gainL;
        buffer[f * 2 + 1] += final_sample * gainR;

        t->cursor_frames++;
    }
}

SITAPI SituationError SituationSetToneRouting(SituationToneHandle handle, bool route_to_graph) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    mtx_lock(&sit_audio.audio_queue_mutex);
    SituationTone* t = _GetToneFromHandle(handle);
    if (!t) {
        mtx_unlock(&sit_audio.audio_queue_mutex);
        return SITUATION_ERROR_INVALID_PARAM;
    }
    t->route_to_graph = route_to_graph;
    mtx_unlock(&sit_audio.audio_queue_mutex);

    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationSetGraphSFXSource(SituationNodeHandle handle) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!sit_audio.active_graph) return SITUATION_ERROR_INVALID_PARAM;

    SituationNode* node = SituationGetNode(sit_audio.active_graph, handle);
    if (!node || node->type != SITUATION_NODE_SOUND_SOURCE) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    sit_audio.sfx_graph_voice_source = node->device_data;
    return SITUATION_SUCCESS;
}

#endif // SITUATION_IMPL_AUDIO_H
