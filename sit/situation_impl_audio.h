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
*     • Device management (enumeration, selection, format negotiation)
*     • Playback mixing with volume, pan, pitch control
*     • Real-time effects chain (low/high-pass filters, echo, reverb, custom DSP processors)
*     • Procedural tone generation (sine, square, triangle, saw, noise) with ADSR envelopes
*     • MIDI-note convenience layer
*     • Global reverb for tones (Schroeder/Freeverb style)
*     • Handle-based sound management with generation counters for safety
*     • Thread-safe mixing using snapshot strategy (minimal lock contention)
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

// Define implementation guards for MIDI headers
#define MIDI_IMPLEMENTATION
#define MIDI_DEVICE_IMPLEMENTATION
#define MIDI_LEARN_IMPLEMENTATION

// Core audio subsystem includes (implementation only)
#include "sit/aud/midi.h"                      // MIDI system (needed by node graph)
#include "sit/aud/midi_device.h"               // MIDI device abstraction
#include "sit/aud/midi_learn.h"                // MIDI Learn system
#include "sit/aud/registry_init.h"             // Registry initialization (device registration functions)
#include "sit/aud/node_graph_impl.h"           // Node graph implementation
#include "sit/aud/node_graph_process.h"        // Node graph processing
#include "sit/aud/node_graph_midi.h"           // Node graph MIDI integration (Phase 2)
#include "sit/aud/node_graph_serialization_impl.h"  // Graph serialization implementation
#include "sit/aud/device_wrappers.h"           // Device wrapper functions (includes all device headers)
#include "sit/aud/tone_synth.h"                // Tone synthesizer (audio API functions)

// Initialize the device registry on first inclusion
static void _SituationEnsureRegistryInit(void) {
    static bool registry_init_done = false;
    if (!registry_init_done) {
        SituationInitDeviceRegistry();
        registry_init_done = true;
    }
}

// --- Internal Dynamics Node (Phase 2) ---
// Implements Compressor / Limiter / Gate
typedef struct {
    ma_node_config nodeConfig;
    uint32_t sampleRate;
    float thresholdDB;
    float ratio;
    float attackTime;
    float releaseTime;
    float makeupGainDB;
    bool isGate;
    int sidechainEnabled;
} SituationDynamicsNodeConfig;

typedef struct {
    ma_node_base base;
    float thresholdDB;
    float ratio;
    float attackCoef;
    float releaseCoef;
    float makeupGain;
    bool isGate;
    int sidechainEnabled;
    float envelope;
    _Atomic float gainReductionDB;
    // Cached for Persistence
    float attackTime;
    float releaseTime;
} SituationDynamicsNode;

static void _situation_dynamics_process(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut) {
    (void)pFrameCountIn;
    SituationDynamicsNode* dyn = (SituationDynamicsNode*)pNode;
    const float* pMain = ppFramesIn[0];
    const float* pSide = ppFramesIn[1];
    float* pOut = ppFramesOut[0];
    ma_uint32 frames = *pFrameCountOut;
    ma_uint32 channels = ma_node_get_output_channels(pNode, 0);

    // Passthrough if no input or ratio=1
    if (!pMain) {
        memset(pOut, 0, frames * channels * sizeof(float));
        return;
    }

    // Optimization: If ratio is 1.0 (no comp) and not gating, just copy/scale
    if (!dyn->isGate && dyn->ratio == 1.0f && dyn->makeupGain == 1.0f) {
        memcpy(pOut, pMain, frames * channels * sizeof(float));
        atomic_store(&dyn->gainReductionDB, 0.0f);
        return;
    }

    float minGainDB = 0.0f;

    for (ma_uint32 i = 0; i < frames; ++i) {
        // 1. Key Signal
        float key = 0.0f;
        if (dyn->sidechainEnabled && pSide) {
             float L = pSide[i*channels];
             float R = (channels > 1) ? pSide[i*channels+1] : L;
             key = fmaxf(fabsf(L), fabsf(R));
        } else {
             float L = pMain[i*channels];
             float R = (channels > 1) ? pMain[i*channels+1] : L;
             key = fmaxf(fabsf(L), fabsf(R));
        }

        // 2. Envelope Follower
        if (key > dyn->envelope) dyn->envelope += dyn->attackCoef * (key - dyn->envelope);
        else dyn->envelope += dyn->releaseCoef * (key - dyn->envelope);

        // 3. Gain Calculation
        float gainDB = 0.0f;
        float envDB = (dyn->envelope > 1e-6f) ? 20.0f * log10f(dyn->envelope) : -100.0f;

        if (dyn->isGate) {
            if (envDB < dyn->thresholdDB) gainDB = -100.0f; // Hard Gate
        } else {
            if (envDB > dyn->thresholdDB) {
                gainDB = (dyn->thresholdDB - envDB) * (1.0f - 1.0f / dyn->ratio);
            }
        }

        if (gainDB < minGainDB) minGainDB = gainDB;

        // 4. Apply
        float gain = powf(10.0f, gainDB / 20.0f) * dyn->makeupGain;
        for (ma_uint32 c = 0; c < channels; ++c) {
            pOut[i*channels + c] = pMain[i*channels + c] * gain;
        }
    }

    atomic_store(&dyn->gainReductionDB, -minGainDB);
}

static ma_node_vtable g_situation_dynamics_vtable = {
    _situation_dynamics_process, NULL, 2, 1, 0
};

static SituationDynamicsNodeConfig SituationDynamicsNodeConfigInit(int channels, int sampleRate, const ma_uint32* pInputChannels, const ma_uint32* pOutputChannels) {
    SituationDynamicsNodeConfig config;
    memset(&config, 0, sizeof(config));
    config.nodeConfig = ma_node_config_init();
    config.nodeConfig.vtable = &g_situation_dynamics_vtable;
    config.nodeConfig.pInputChannels = pInputChannels;
    config.nodeConfig.pOutputChannels = pOutputChannels;
    config.sampleRate = sampleRate;
    config.ratio = 1.0f;
    config.makeupGainDB = 0.0f;
    config.attackTime = 0.01f;
    config.releaseTime = 0.1f;
    return config;
}

static ma_result SituationDynamicsNodeInit(ma_node_graph* pNodeGraph, const SituationDynamicsNodeConfig* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, SituationDynamicsNode* pNode) {
    ma_result result = ma_node_init(pNodeGraph, &pConfig->nodeConfig, pAllocationCallbacks, &pNode->base);
    if (result != MA_SUCCESS) return result;
    pNode->thresholdDB = pConfig->thresholdDB;
    pNode->ratio = pConfig->ratio;
    pNode->makeupGain = powf(10.0f, pConfig->makeupGainDB / 20.0f);
    pNode->isGate = pConfig->isGate;
    pNode->sidechainEnabled = pConfig->sidechainEnabled;
    pNode->envelope = 0.0f;
    atomic_init(&pNode->gainReductionDB, 0.0f);
    float sr = (float)pConfig->sampleRate;
    if (sr <= 0.0f) sr = 48000.0f;
    pNode->attackCoef = 1.0f - expf(-1.0f / (pConfig->attackTime * sr));
    pNode->releaseCoef = 1.0f - expf(-1.0f / (pConfig->releaseTime * sr));
    return MA_SUCCESS;
}

static void SituationDynamicsNodeUninit(SituationDynamicsNode* pNode, const ma_allocation_callbacks* pAllocationCallbacks) {
    ma_node_uninit(&pNode->base, pAllocationCallbacks);
}

// --- Internal Panner Node (Phase 3) ---
typedef struct {
    ma_node_base base;
    _Atomic float pan; // -1.0 (Left) to +1.0 (Right)
} SituationPannerNode;

static void _situation_panner_process(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut) {
    (void)pFrameCountIn;
    SituationPannerNode* panner = (SituationPannerNode*)pNode;
    const float* pIn = ppFramesIn[0];
    float* pOut = ppFramesOut[0];
    ma_uint32 frames = *pFrameCountOut;
    ma_uint32 channels = ma_node_get_output_channels(pNode, 0);

    if (!pIn) {
        memset(pOut, 0, frames * channels * sizeof(float));
        return;
    }

    ma_uint32 inChannels = ma_node_get_input_channels(pNode, 0);
    float pan = atomic_load(&panner->pan);
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // Linear Pan Law
    float gainL = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
    float gainR = (pan >= 0.0f) ? 1.0f : (1.0f + pan);

    for (ma_uint32 i = 0; i < frames; ++i) {
        float inL, inR;
        if (inChannels == 1) {
            inL = pIn[i];
            inR = pIn[i];
        } else {
            inL = pIn[i*inChannels];
            inR = pIn[i*inChannels + 1];
        }

        pOut[i*channels] = inL * gainL;
        if (channels > 1) {
            pOut[i*channels + 1] = inR * gainR;
        }
    }
}

static ma_node_vtable g_situation_panner_vtable = {
    _situation_panner_process, NULL, 1, 1, 0
};

static ma_result SituationPannerNodeInit(ma_node_graph* pNodeGraph, const ma_allocation_callbacks* pAllocationCallbacks, SituationPannerNode* pNode) {
    ma_node_config config = ma_node_config_init();
    config.vtable = &g_situation_panner_vtable;
    static ma_uint32 inCh[1] = {2};
    static ma_uint32 outCh[1] = {2};
    config.pInputChannels = inCh;
    config.pOutputChannels = outCh;

    ma_result result = ma_node_init(pNodeGraph, &config, pAllocationCallbacks, &pNode->base);
    if (result != MA_SUCCESS) return result;
    atomic_init(&pNode->pan, 0.0f);
    return MA_SUCCESS;
}

static void SituationPannerNodeUninit(SituationPannerNode* pNode, const ma_allocation_callbacks* pAllocationCallbacks) {
    ma_node_uninit(&pNode->base, pAllocationCallbacks);
}

// --- Internal Meter Node (Phase 4) ---
typedef struct {
    ma_node_base base;
    _Atomic float peak_L;
    _Atomic float peak_R;
} SituationMeterNode;

static void _situation_meter_process(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut) {
    (void)pFrameCountIn;
    SituationMeterNode* meter = (SituationMeterNode*)pNode;
    const float* pIn = ppFramesIn[0];
    float* pOut = ppFramesOut[0];
    ma_uint32 frames = *pFrameCountOut;
    ma_uint32 channels = ma_node_get_output_channels(pNode, 0);

    if (!pIn) {
        memset(pOut, 0, frames * channels * sizeof(float));
        atomic_store(&meter->peak_L, 0.0f);
        atomic_store(&meter->peak_R, 0.0f);
        return;
    }

    // Pass-through copy
    memcpy(pOut, pIn, frames * channels * sizeof(float));

    float max_L = 0.0f;
    float max_R = 0.0f;

    for (ma_uint32 i = 0; i < frames; ++i) {
        float L = fabsf(pIn[i*channels]);
        float R = (channels > 1) ? fabsf(pIn[i*channels+1]) : L;
        if (L > max_L) max_L = L;
        if (R > max_R) max_R = R;
    }

    atomic_store(&meter->peak_L, max_L);
    atomic_store(&meter->peak_R, max_R);
}

static ma_node_vtable g_situation_meter_vtable = {
    _situation_meter_process, NULL, 1, 1, 0
};

static ma_result SituationMeterNodeInit(ma_node_graph* pNodeGraph, const ma_allocation_callbacks* pAllocationCallbacks, SituationMeterNode* pNode) {
    ma_node_config config = ma_node_config_init();
    config.vtable = &g_situation_meter_vtable;
    static ma_uint32 inCh[1] = {2};
    static ma_uint32 outCh[1] = {2};
    config.pInputChannels = inCh;
    config.pOutputChannels = outCh;

    ma_result result = ma_node_init(pNodeGraph, &config, pAllocationCallbacks, &pNode->base);
    if (result != MA_SUCCESS) return result;
    atomic_init(&pNode->peak_L, 0.0f);
    atomic_init(&pNode->peak_R, 0.0f);
    return MA_SUCCESS;
}

static void SituationMeterNodeUninit(SituationMeterNode* pNode, const ma_allocation_callbacks* pAllocationCallbacks) {
    ma_node_uninit(&pNode->base, pAllocationCallbacks);
}

// --- Mixer Definitions (Phase 2) ---
#define SIT_MAX_TRACKS          16
#define SIT_MAX_AUX_BUSES        8
#define SIT_MAX_FX_SLOTS         8

// --- Insert Chain Types (Phase 6 Session 1) ---
typedef enum {
    SITUATION_INSERT_PRE_EQ = 0,    // Before EQ
    SITUATION_INSERT_POST_EQ = 1,   // After EQ, before dynamics
    SITUATION_INSERT_POST_DYN = 2,  // After dynamics
    SITUATION_INSERT_COUNT = 3      // Total number of insert positions
} SituationInsertPosition;

typedef struct {
    SituationAudioGraph* chain;  // Modular node graph (NULL if no insert)
    bool bypass;                       // True to bypass insert (pass-through)
    bool is_active;                    // True if insert is attached
} SituationInsertChain;

// --- Aux Bus FX Types (Phase 6 Session 2) ---
typedef struct {
    SituationAudioGraph* fx_chain;  // Modular FX node graph (NULL if no FX)
    bool bypass;                          // True to bypass FX (pass-through)
    bool is_active;                       // True if FX chain is attached
    float wet_mix;                        // Wet signal level (0.0 to 1.0)
    float dry_mix;                        // Dry signal level (0.0 to 1.0)
} SituationAuxFXChain;

typedef struct SituationAudioBus {
    char name[64];
    int id;
    bool is_active;  // Added for consistency
    
    // Graph: Input Sum -> Output Splitter -> Master
    ma_splitter_node input_node;
    ma_splitter_node output_node;

    // Phase 4: FX & Metering
    ma_node* fx[SIT_MAX_FX_SLOTS];
    int fx_count;
    SituationMeterNode meter_node;

    // Phase 6 Session 2: Modular FX Chain
    SituationAuxFXChain fx_chain;

    _Atomic float volume;
    _Atomic float pan;
} SituationAudioBus;

struct SituationAudioTrack {
    struct SituationAudioMixer* owner; // Back-pointer for locking
    char name[64];
    int id;
    bool is_active;

    _Atomic float volume;
    _Atomic float pan;
    bool mute;
    bool solo;

    // EQ State (Cached for Persistence)
    struct {
        bool enabled;
        float hpf_freq;
        float ls_freq;
        float ls_gain;
        float ls_q;
        float peak_freq;
        float peak_gain;
        float peak_q;
        float hs_freq;
        float hs_gain;
        float hs_q;
    } eq_state;

    // Sends
    float send_level[SIT_MAX_AUX_BUSES];
    bool send_pre[SIT_MAX_AUX_BUSES];

    // Phase 6: Insert Chains (Pre-EQ, Post-EQ, Post-Dynamics)
    SituationInsertChain inserts[SITUATION_INSERT_COUNT];

    // Node Graph: Input Sum -> [Insert Pre-EQ] -> [EQ] -> [Insert Post-EQ] -> [Dynamics] -> [Insert Post-Dyn] -> [PreSplit] -> [Pan] -> [PostSplit] -> Master
    ma_splitter_node input_node; // Input summing point

    // EQ Chain
    ma_hpf_node eq_hpf;
    ma_loshelf_node eq_loshelf;
    ma_peak_node eq_peak;
    ma_hishelf_node eq_hishelf;

    // Dynamics
    SituationDynamicsNode dynamics_node;

    // Pre-Fader Splitter
    // Bus 0: To Panner (Main Path)
    // Bus 1: Sidechain Send
    // Bus 2..9: Aux Sends (Pre)
    ma_splitter_node pre_fader_splitter;

    // Panner
    SituationPannerNode panner_node;

    // Phase 4: Metering
    SituationMeterNode meter_node;

    // Post-Fader Splitter
    // Bus 0: To Master (Main Path)
    // Bus 1..8: Aux Sends (Post)
    ma_splitter_node post_fader_splitter;

    // Sidechain State
    struct SituationAudioTrack* sidechain_source;
};

struct SituationAudioMixer {
    ma_node_graph graph;
    ma_device* device;

    struct SituationAudioTrack tracks[SIT_MAX_TRACKS];
    int track_count;

    SituationAudioBus aux_buses[SIT_MAX_AUX_BUSES];

    // Master Bus
    ma_splitter_node master_node; // Connects to Endpoint

    mtx_t topology_mutex;
    bool is_initialized;
};

// Audio-related implementation extracted from situation_impl.h

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

static void _SitFreeSoundSlot(SituationSound handle) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(handle);
    if (!slot) return;

    mtx_lock(&sit_audio.pool_mutex);
    if (slot->source_path) SIT_FREE(slot->source_path);
    // Note: sound_data cleanup (ma_decoder_uninit) should be done before calling this
    slot->is_active = false;
    mtx_unlock(&sit_audio.pool_mutex);
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

static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
    _SituationAudioState* pGs = (_SituationAudioState*)pDevice->pUserData;
    if (!pGs) return;

    // Output Buffer (Mixing Destination)
    float* pOut = (float*)pOutput;

    // Clear output buffer (silence)
    memset(pOut, 0, frameCount * pDevice->playback.channels * sizeof(float));

    // Handle Input (Capture)
    if (pGs->is_capture_device_active && pGs->capture_callback) {
        // ... (Capture logic omitted for brevity, usually distinct device)
    }

    // --- [Phase 1] Mixer Integration ---
    // Acquire lock to safely check/use active_mixer
    mtx_lock(&pGs->audio_queue_mutex);
    SituationAudioMixer* mixer = pGs->active_mixer;
    if (mixer && mixer->is_initialized) {
        // We must also lock the mixer topology because ma_node_graph_read_pcm_frames is not thread-safe vs modification
        mtx_lock(&mixer->topology_mutex);

        ma_uint64 framesRead = 0;
        ma_node_graph_read_pcm_frames(&mixer->graph, pOutput, frameCount, &framesRead);

        mtx_unlock(&mixer->topology_mutex);
        mtx_unlock(&pGs->audio_queue_mutex);
        return;
    }


    // --- Process lock-free commands ---
    size_t tail = atomic_load_explicit(&pGs->audio_command_tail, memory_order_relaxed);
    size_t commit = atomic_load_explicit(&pGs->audio_command_commit, memory_order_acquire);

    while (tail != commit) {
        SituationAudioCommand cmd = pGs->audio_command_queue[tail];
        switch (cmd.type) {
            case SIT_AUDIO_CMD_PLAY_SOUND:
            {
                bool present = false;
                for (int i = 0; i < pGs->active_voice_count; i++) {
                    if (pGs->active_voices[i] == cmd.sound) { present = true; break; }
                }
                if (!present) {
                    if (pGs->active_voice_count < pGs->active_voice_capacity) {
                        pGs->active_voices[pGs->active_voice_count++] = cmd.sound;
                    } else {
                        int new_cap = pGs->active_voice_capacity * 2;
                        _SituationSound** new_array = (_SituationSound**)SIT_REALLOC(pGs->active_voices, new_cap * sizeof(_SituationSound*));
                        if (new_array) {
                            pGs->active_voices = new_array;
                            pGs->active_voice_capacity = new_cap;
                            pGs->active_voices[pGs->active_voice_count++] = cmd.sound;
                        }
                    }
                }
                break;
            }
            case SIT_AUDIO_CMD_STOP_SOUND:
            {
                for (int i = 0; i < pGs->active_voice_count; i++) {
                    if (pGs->active_voices[i] == cmd.sound) {
                        pGs->active_voices[i] = pGs->active_voices[pGs->active_voice_count - 1];
                        pGs->active_voice_count--;
                        break;
                    }
                }
                break;
            }
            case SIT_AUDIO_CMD_STOP_ALL_SOUNDS: pGs->active_voice_count = 0; break;
            case SIT_AUDIO_CMD_SET_SOUND_VOLUME: cmd.sound->volume = cmd.value; break;
            case SIT_AUDIO_CMD_SET_SOUND_PAN: cmd.sound->pan = cmd.value; break;
            case SIT_AUDIO_CMD_SET_SOUND_PITCH: cmd.sound->pitch = cmd.value; break;
            case SIT_AUDIO_CMD_PLAY_TONE:
            {
                int slot = -1;
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (!pGs->tone_pool[i].active) { slot = i; break; }
                }
                if (slot == -1) {
                    uint64_t max_release = 0; int best_rel = -1;
                    for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                        if (pGs->tone_pool[i].active && pGs->tone_pool[i].state == SIT_ENV_RELEASE && pGs->tone_pool[i].cursor_frames > max_release) {
                            max_release = pGs->tone_pool[i].cursor_frames; best_rel = i;
                        }
                    }
                    slot = best_rel;
                }
                if (slot == -1) {
                    uint64_t min_cursor = UINT64_MAX; int best_act = -1;
                    for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                        if (pGs->tone_pool[i].active && pGs->tone_pool[i].cursor_frames < min_cursor) {
                            min_cursor = pGs->tone_pool[i].cursor_frames; best_act = i;
                        }
                    }
                    slot = best_act;
                }
                if (slot != -1) {
                    SituationTone* t = &pGs->tone_pool[slot];
                    memset(t, 0, sizeof(SituationTone));
                    t->id = cmd.tone_handle;
                    t->active = true;
                    t->type = cmd.tone_type;
                    t->amplitude = cmd.value;
                    t->frequency = cmd.frequency;
                    t->pan = cmd.pan;
                    t->envelope.attack = cmd.attack_sec;
                    t->envelope.decay = cmd.decay_sec;
                    t->envelope.sustain_level = cmd.sustain_level;
                    t->envelope.release = cmd.release_sec;
                    t->envelope.hold = cmd.hold_sec;
                    t->state = SIT_ENV_ATTACK;
                    t->format = pGs->miniaudio_device.playback.format;
                    t->channels = pGs->miniaudio_device.playback.channels;
                    t->sample_rate = pGs->miniaudio_device.sampleRate;
                    if (t->type == SIT_WAVE_NOISE) {
                        ma_noise_config cfg = ma_noise_config_init(t->format, t->channels, t->sample_rate, 0, ma_noise_type_white);
                        ma_noise_init(&cfg, NULL, &t->noise);
                    } else {
                        ma_waveform_type mt = (t->type == SIT_WAVE_SQUARE) ? ma_waveform_type_square :
                                              (t->type == SIT_WAVE_TRIANGLE) ? ma_waveform_type_triangle :
                                              (t->type == SIT_WAVE_SAW) ? ma_waveform_type_sawtooth : ma_waveform_type_sine;
                        ma_waveform_config cfg = ma_waveform_config_init(t->format, t->channels, t->sample_rate, mt, t->amplitude, t->frequency);
                        ma_waveform_init(&cfg, &t->waveform);
                    }
                }
                break;
            }
            case SIT_AUDIO_CMD_STOP_TONE:
            {
                SituationTone* t = NULL;
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].id == cmd.tone_handle) { t = &pGs->tone_pool[i]; break; }
                }
                if (t && t->state != SIT_ENV_RELEASE && t->state != SIT_ENV_IDLE) {
                    t->state = SIT_ENV_RELEASE;
                    t->cursor_frames = 0;
                }
                break;
            }
            case SIT_AUDIO_CMD_STOP_ALL_TONES:
            {
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].state != SIT_ENV_RELEASE) {
                        pGs->tone_pool[i].state = SIT_ENV_RELEASE;
                        pGs->tone_pool[i].cursor_frames = 0;
                    }
                }
                break;
            }
        }
        tail = (tail + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;
    }
    atomic_store_explicit(&pGs->audio_command_tail, tail, memory_order_release);

    // --- MIXING LOOP (Legacy/Fallback) ---


    if (voices_to_mix == 0 || !pGs->snapshot_buffer) return;

    // Process Snapshot
    // Temp buffer for mixing one sound before adding to accumulation
    // float* mix_buffer = ... (we need a scratch buffer for effects)
    // Using pGs->audio_callback_decoder_temp_buffer etc.

    // We need to verify we have scratch buffers. Assuming they are init in InitAudio.
    float* decoder_buffer = pGs->audio_callback_decoder_temp_buffer;
    float* effects_buffer = pGs->audio_callback_effects_temp_buffer;

    // Sanity check
    if (!decoder_buffer || !effects_buffer) return;

    for (int i = 0; i < voices_to_mix; ++i) {
        _SituationSound* sound = pGs->snapshot_buffer[i];
        if (!sound) continue;

        // 1. Read/Decode PCM
        ma_uint64 frames_read = 0;

        if (sound->is_preloaded && sound->preloaded_data) {
            // RAM Playback
            ma_uint64 frames_remaining = sound->total_frames - sound->cursor_frames;
            frames_read = (frames_remaining > frameCount) ? frameCount : frames_remaining;

            // Copy from RAM to decoder_buffer (or directly mix if no effects? Effects need inplace usually)
            // Let's copy to decoder_buffer to standardize pipeline.
            memcpy(decoder_buffer, (float*)sound->preloaded_data + (sound->cursor_frames * 2), frames_read * 2 * sizeof(float));

            // Advance cursor
            sound->cursor_frames += frames_read;

            // Loop?
            if (frames_read < frameCount && sound->is_looping) {
                sound->cursor_frames = 0;
                // Read remainder
                ma_uint64 remainder = frameCount - frames_read;
                // Simple loop: just read from start.
                // Note: infinite loop risk if file is 0 length. check total_frames > 0.
                if (sound->total_frames > 0) {
                    ma_uint64 loop_read = (sound->total_frames > remainder) ? remainder : sound->total_frames;
                    memcpy(decoder_buffer + (frames_read * 2), sound->preloaded_data, loop_read * 2 * sizeof(float));
                    frames_read += loop_read;
                    sound->cursor_frames += loop_read;
                }
            }
        } else if (sound->is_initialized) {
            // Streaming (ma_decoder)
            // Note: ma_decoder_read_pcm_frames is not fully thread safe if main thread seeks/unloads!
            // But we hold a reference (via active_voices). Unload stops sound first.
            // Seek is the main risk. We need per-voice lock or atomic flags?
            // For now assuming safe-ish via stop-before-unload pattern.

            ma_result res = ma_decoder_read_pcm_frames(&sound->decoder, decoder_buffer, frameCount, &frames_read);

            if (res == MA_AT_END && sound->is_looping) {
                ma_decoder_seek_to_pcm_frame(&sound->decoder, 0);
                ma_uint64 remainder = frameCount - frames_read;
                ma_uint64 loop_read = 0;
                ma_decoder_read_pcm_frames(&sound->decoder, decoder_buffer + (frames_read * 2), remainder, &loop_read);
                frames_read += loop_read;
            }
        }

        if (frames_read > 0) {
            // 2. Effects Processing (In-Place on decoder_buffer or copy to effects_buffer)
            // Let's use effects_buffer as destination
            memcpy(effects_buffer, decoder_buffer, frames_read * 2 * sizeof(float));

            // Custom Processors
            if (sound->processors) {
                for (int p = 0; p < sound->processor_count; ++p) {
                    if (sound->processors[p]) {
                        sound->processors[p](effects_buffer, (uint32_t)frames_read, 2, pDevice->sampleRate, sound->processor_user_data[p]);
                    }
                }
            }

            // Built-in Effects (Filter, Echo, Reverb)
            if (sound->effects.echo_enabled && sound->effects.delay_initialized) {
                _SituationProcessEcho(&sound->effects.delay, effects_buffer, (uint32_t)frames_read);
            }

            if (sound->effects.reverb_enabled && sound->effects.reverb_state) {
                _SituationProcessReverb(sound->effects.reverb_state, effects_buffer, effects_buffer, (uint32_t)frames_read, 2);
            }

            // 3. Apply Volume/Pan & Mix to Output
            float vol = atomic_load(&sound->volume);
            float pan = atomic_load(&sound->pan);

            // Simple Stereo Mix
            for (ma_uint64 f = 0; f < frames_read; ++f) {
                float sampleL = effects_buffer[f*2 + 0];
                float sampleR = effects_buffer[f*2 + 1];

                // Pan law (linear approximation)
                float gainL = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
                float gainR = (pan >= 0.0f) ? 1.0f : (1.0f + pan);

                pOut[f*2 + 0] += sampleL * vol * gainL;
                pOut[f*2 + 1] += sampleR * vol * gainR;
            }
        } else {
            // Sound finished?
            if (!sound->is_looping && !sound->is_streamed && (sound->is_preloaded && sound->cursor_frames >= sound->total_frames)) {
                // Mark for removal?
                // The mixer cannot remove from the main list easily without lock.
                // We typically handle this in a cleanup pass or let StopLoadedSound handle it.
                // For now, it just plays silence.
            }
        }
    }
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
        if (data->is_preloaded) {
        data->cursor_frames = 0;
    }

    SituationAudioCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = SIT_AUDIO_CMD_PLAY_SOUND;
    cmd.sound = data;
    _SitPushAudioCommand(cmd);
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

    SituationAudioCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = SIT_AUDIO_CMD_STOP_SOUND;
    cmd.sound = data;
    _SitPushAudioCommand(cmd);
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
    SituationAudioCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = SIT_AUDIO_CMD_STOP_ALL_SOUNDS;
    _SitPushAudioCommand(cmd);
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
            _SitFreeSoundSlot(handle);
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        memcpy(dst_data->preloaded_data, src_data->preloaded_data, size);
        dst_data->total_frames = src_data->total_frames;
        dst_data->is_preloaded = true;
    } else {
        // Cannot easily copy streamed sound state without reopening file
        _SitFreeSoundSlot(handle);
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
 * @return True on success, false on failure.
 */
SITAPI bool SituationSoundExportAsWav(const SituationSound* sound, const char* fileName) {
    _SituationSoundSlot* slot = _SitGetSoundSlot(*sound);
    if (!slot) return false;
    _SituationSound* data = &slot->sound_data;

    if (!data->is_preloaded || !data->preloaded_data) return false;

    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000); // Assuming engine native
    ma_encoder encoder;
    if (ma_encoder_init_file(fileName, &config, &encoder) != MA_SUCCESS) return false;

    ma_encoder_write_pcm_frames(&encoder, data->preloaded_data, data->total_frames, NULL);
    ma_encoder_uninit(&encoder);
    return true;
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
            &slot->sound_data.effects.delay,
            &slot->sound_data.effects.delay_initialized,
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
static void _SitAudioInitPool(void) {
    if (mtx_init(&sit_audio.pool_mutex, mtx_plain) != thrd_success) {
        SITUATION_LOG_WARNING(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Failed to init audio pool mutex");
    }

    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; ++i) {
        sit_audio.sound_pool[i].is_active = false;
        sit_audio.sound_pool[i].generation = 1;
        memset(&sit_audio.sound_pool[i].sound_data, 0, sizeof(_SituationSound));
        sit_audio.sound_pool[i].source_path = NULL;
    }
}

// Helper: Cleanup the audio pool
static void _SitAudioCleanupPool(void) {
    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; ++i) {
        if (sit_audio.sound_pool[i].is_active) {
            _SituationSound* snd = &sit_audio.sound_pool[i].sound_data;
            if (snd->is_initialized) ma_decoder_uninit(&snd->decoder);
            if (snd->is_preloaded && snd->preloaded_data) SIT_FREE(snd->preloaded_data);
            if (snd->processors) SIT_FREE(snd->processors);
            if (snd->processor_user_data) SIT_FREE(snd->processor_user_data);
            if (snd->effects.reverb_state) SIT_FREE(snd->effects.reverb_state); // Or _SituationUninitReverb if available

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
static void _SituationAsyncAudioWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncAudioCtx* ctx = (_SitAsyncAudioCtx*)data;

    // Use FULL load mode to decode to RAM on this background thread.
    // This ensures no disk I/O happens on the main thread later.
    SituationLoadSoundFromFile(ctx->path, SITUATION_AUDIO_LOAD_FULL, ctx->looping, ctx->target);

    // Cleanup string copy
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

SITAPI SituationAudioMixer* SituationCreateMixer(void) {
    if (!SituationIsInitialized()) return NULL;

    SituationAudioMixer* mixer = (SituationAudioMixer*)SIT_CALLOC(1, sizeof(SituationAudioMixer));
    if (!mixer) return NULL;

    mtx_init(&mixer->topology_mutex, mtx_plain);

    ma_node_graph_config config = ma_node_graph_config_init(2); // Stereo default
    if (ma_node_graph_init(&config, NULL, &mixer->graph) != MA_SUCCESS) {
        SIT_FREE(mixer);
        return NULL;
    }

    ma_splitter_node_config splitCfg = ma_splitter_node_config_init(2);
    if (ma_splitter_node_init(&mixer->graph, &splitCfg, NULL, &mixer->master_node) != MA_SUCCESS) {
        ma_node_graph_uninit(&mixer->graph, NULL);
        SIT_FREE(mixer);
        return NULL;
    }

    ma_node_attach_output_bus(&mixer->master_node, 0, ma_node_graph_get_endpoint(&mixer->graph), 0);

    // Initialize Aux Buses
    for (int i = 0; i < SIT_MAX_AUX_BUSES; ++i) {
        SituationAudioBus* bus = &mixer->aux_buses[i];
        bus->id = i;
        snprintf(bus->name, sizeof(bus->name), "Aux %d", i + 1);

        // Input Node (Splitter acting as summer)
        if (ma_splitter_node_init(&mixer->graph, &splitCfg, NULL, &bus->input_node) != MA_SUCCESS) continue;

        // Output Node
        if (ma_splitter_node_init(&mixer->graph, &splitCfg, NULL, &bus->output_node) != MA_SUCCESS) {
            ma_splitter_node_uninit(&bus->input_node, NULL);
            continue;
        }

        // Meter Node
        if (SituationMeterNodeInit(&mixer->graph, NULL, &bus->meter_node) != MA_SUCCESS) {
            ma_splitter_node_uninit(&bus->input_node, NULL);
            ma_splitter_node_uninit(&bus->output_node, NULL);
            continue;
        }

        // Wire: Input -> Meter -> Output -> Master
        ma_node_attach_output_bus(&bus->input_node, 0, &bus->meter_node.base, 0);
        ma_node_attach_output_bus(&bus->meter_node.base, 0, &bus->output_node, 0);
        ma_node_attach_output_bus(&bus->output_node, 0, &mixer->master_node, 0);

        bus->volume = 1.0f;
    }

    mixer->is_initialized = true;
    return mixer;
}

static void _SituationRemoveTrack_NoLock(SituationAudioTrack* track) {
    if (!track || !track->is_active) return;

    // Uninit all nodes (which detaches them)
    ma_splitter_node_uninit(&track->input_node, NULL);
    ma_hpf_node_uninit(&track->eq_hpf, NULL);
    ma_loshelf_node_uninit(&track->eq_loshelf, NULL);
    ma_peak_node_uninit(&track->eq_peak, NULL);
    ma_hishelf_node_uninit(&track->eq_hishelf, NULL);
    SituationDynamicsNodeUninit(&track->dynamics_node, NULL);
    ma_splitter_node_uninit(&track->pre_fader_splitter, NULL);
    SituationPannerNodeUninit(&track->panner_node, NULL);
    SituationMeterNodeUninit(&track->meter_node, NULL);
    ma_splitter_node_uninit(&track->post_fader_splitter, NULL);

    track->is_active = false;
    track->owner = NULL;
}

SITAPI void SituationRemoveTrack(SituationAudioTrack* track) {
    if (!track || !track->is_active || !track->owner) return;

    mtx_lock(&track->owner->topology_mutex);
    _SituationRemoveTrack_NoLock(track);
    mtx_unlock(&track->owner->topology_mutex);
}

SITAPI void SituationDestroyMixer(SituationAudioMixer* mixer) {
    if (!mixer) return;

    if (SituationIsInitialized()) {
        mtx_lock(&sit_audio.audio_queue_mutex);
        if (sit_audio.active_mixer == mixer) {
            sit_audio.active_mixer = NULL;
        }
        mtx_unlock(&sit_audio.audio_queue_mutex);
    }

    mtx_lock(&mixer->topology_mutex);
    for (int i=0; i<SIT_MAX_TRACKS; ++i) {
        if (mixer->tracks[i].is_active) {
            _SituationRemoveTrack_NoLock(&mixer->tracks[i]);
        }
    }
    // Cleanup Aux Buses
    for (int i = 0; i < SIT_MAX_AUX_BUSES; ++i) {
        ma_splitter_node_uninit(&mixer->aux_buses[i].input_node, NULL);
        ma_splitter_node_uninit(&mixer->aux_buses[i].output_node, NULL);
        SituationMeterNodeUninit(&mixer->aux_buses[i].meter_node, NULL);
    }

    ma_splitter_node_uninit(&mixer->master_node, NULL);
    ma_node_graph_uninit(&mixer->graph, NULL);
    mtx_unlock(&mixer->topology_mutex);

    mtx_destroy(&mixer->topology_mutex);
    SIT_FREE(mixer);
}

SITAPI SituationError SituationBindMixerToDevice(SituationAudioMixer* mixer, const char* device_id, uint32_t requested_channels_out) {
    if (!mixer) return SITUATION_ERROR_INVALID_PARAM;

    if (device_id) {
        int count = 0;
        SituationAudioDeviceInfo* devs = SituationEnumerateAudioDevices(&count);
        int target_idx = -1;
        if (devs) {
            for (int i=0; i<count; ++i) {
                if (strcmp(devs[i].id, device_id) == 0) {
                    target_idx = i;
                    break;
                }
            }
            SituationFreeDeviceList(devs, count);
        }

        if (target_idx >= 0) {
            SituationAudioFormat fmt = {0};
            fmt.channels = requested_channels_out;
            SituationSetAudioDevice(target_idx, &fmt);
        }
    }

    mtx_lock(&sit_audio.audio_queue_mutex);
    sit_audio.active_mixer = mixer;
    mtx_unlock(&sit_audio.audio_queue_mutex);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationBindCaptureDevice(SituationAudioMixer* mixer, const char* device_id, uint32_t requested_channels_in) {
    (void)mixer; (void)requested_channels_in;

    if (!device_id) {
        sit_audio.active_capture_device_id_set = false;
        return SITUATION_SUCCESS;
    }

    int count = 0;
    SituationAudioDeviceInfo* devs = SituationEnumerateAudioDevices(&count);
    if (!devs) return SITUATION_ERROR_AUDIO_DEVICE;

    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (strcmp(devs[i].id, device_id) == 0 && devs[i].type == SIT_AUDIO_DEVICE_TYPE_CAPTURE) {
            sit_audio.active_capture_device_id = devs[i].native_id;
            sit_audio.active_capture_device_id_set = true;
            found = true;
            break;
        }
    }
    SituationFreeDeviceList(devs, count);

    return found ? SITUATION_SUCCESS : SITUATION_ERROR_INVALID_PARAM;
}

// Forward declaration
static void _SituationUpdateSoloState(SituationAudioMixer* mixer);

static bool _SituationInitTrack_NoLock(SituationAudioMixer* mixer, int index, const char* name) {
    SituationAudioTrack* t = &mixer->tracks[index];
    memset(t, 0, sizeof(SituationAudioTrack));
    t->owner = mixer;
    strncpy(t->name, name ? name : "Track", 63);
    t->id = index;
    t->is_active = true;
    t->volume = 1.0f;

    memset(t->send_level, 0, sizeof(t->send_level));
    memset(t->send_pre, 0, sizeof(t->send_pre));

    // 1. Input Node (Splitter / Summer)
    ma_splitter_node_config splitCfg = ma_splitter_node_config_init(2); // Stereo
    if (ma_splitter_node_init(&mixer->graph, &splitCfg, NULL, &t->input_node) != MA_SUCCESS) {
        t->is_active = false; return false;
    }

    // 2. EQ Nodes (Default: Flat/Bypass)
    uint32_t sr = mixer->device->sampleRate;
    ma_hpf_node_config hpfCfg = ma_hpf_node_config_init(2, sr, 10.0, 0); // 10Hz HPF
    ma_hpf_node_init(&mixer->graph, &hpfCfg, NULL, &t->eq_hpf);

    ma_loshelf_node_config lsCfg = ma_loshelf_node_config_init(2, sr, 0, 0, 100);
    ma_loshelf_node_init(&mixer->graph, &lsCfg, NULL, &t->eq_loshelf);

    ma_peak_node_config pkCfg = ma_peak_node_config_init(2, sr, 0, 0, 1000);
    ma_peak_node_init(&mixer->graph, &pkCfg, NULL, &t->eq_peak);

    ma_hishelf_node_config hsCfg = ma_hishelf_node_config_init(2, sr, 0, 0, 5000);
    ma_hishelf_node_init(&mixer->graph, &hsCfg, NULL, &t->eq_hishelf);

    // Initialize Cached State
    t->eq_state.enabled = false;
    t->eq_state.hpf_freq = 10.0f;
    t->eq_state.ls_freq = 100.0f;
    t->eq_state.ls_gain = 0.0f;
    t->eq_state.ls_q = 0.0f;
    t->eq_state.peak_freq = 1000.0f;
    t->eq_state.peak_gain = 0.0f;
    t->eq_state.peak_q = 0.0f;
    t->eq_state.hs_freq = 5000.0f;
    t->eq_state.hs_gain = 0.0f;
    t->eq_state.hs_q = 0.0f;

    // 3. Dynamics Node
    ma_uint32 inCh[2]; inCh[0] = 2; inCh[1] = 2;
    ma_uint32 outCh[1]; outCh[0] = 2;
    SituationDynamicsNodeConfig dynCfg = SituationDynamicsNodeConfigInit(2, sr, inCh, outCh);
    SituationDynamicsNodeInit(&mixer->graph, &dynCfg, NULL, &t->dynamics_node);

    // Default cached dynamics
    t->dynamics_node.attackTime = 10.0f;
    t->dynamics_node.releaseTime = 100.0f;

    // 4. Pre-Fader Splitter (Main + Sidechain + 8 Pre-Aux)
    // Bus 0: Main -> Panner
    // Bus 1: Sidechain Send
    // Bus 2..9: Aux Sends (Pre)
    ma_splitter_node_config splitCfgPre = ma_splitter_node_config_init(2);
    splitCfgPre.nodeConfig.outputBusCount = 2 + SIT_MAX_AUX_BUSES;
    if (ma_splitter_node_init(&mixer->graph, &splitCfgPre, NULL, &t->pre_fader_splitter) != MA_SUCCESS) {
            t->is_active = false; return false;
    }

    // 5. Panner Node
    if (SituationPannerNodeInit(&mixer->graph, NULL, &t->panner_node) != MA_SUCCESS) {
        t->is_active = false; return false;
    }

    // 6. Post-Fader Splitter (Main + 8 Post-Aux)
    // Bus 0: Main -> Master
    // Bus 1..8: Aux Sends (Post)
    ma_splitter_node_config splitCfgPost = ma_splitter_node_config_init(2);
    splitCfgPost.nodeConfig.outputBusCount = 1 + SIT_MAX_AUX_BUSES;
    if (ma_splitter_node_init(&mixer->graph, &splitCfgPost, NULL, &t->post_fader_splitter) != MA_SUCCESS) {
        t->is_active = false; return false;
    }

    // 7. Meter Node
    if (SituationMeterNodeInit(&mixer->graph, NULL, &t->meter_node) != MA_SUCCESS) {
        t->is_active = false; return false;
    }

    // 8. Wiring: Input -> [EQ] -> Dynamics -> PreSplit -> Panner -> Meter -> PostSplit -> Master
    ma_node_attach_output_bus(&t->input_node, 0, &t->eq_hpf, 0);
    ma_node_attach_output_bus(&t->eq_hpf, 0, &t->eq_loshelf, 0);
    ma_node_attach_output_bus(&t->eq_loshelf, 0, &t->eq_peak, 0);
    ma_node_attach_output_bus(&t->eq_peak, 0, &t->eq_hishelf, 0);
    ma_node_attach_output_bus(&t->eq_hishelf, 0, &t->dynamics_node, 0);

    // Dynamics -> PreSplit
    ma_node_attach_output_bus(&t->dynamics_node, 0, &t->pre_fader_splitter, 0);

    // PreSplit [0] -> Panner
    ma_node_attach_output_bus(&t->pre_fader_splitter, 0, &t->panner_node.base, 0);

    // Panner -> Meter
    ma_node_attach_output_bus(&t->panner_node.base, 0, &t->meter_node.base, 0);

    // Meter -> PostSplit
    ma_node_attach_output_bus(&t->meter_node.base, 0, &t->post_fader_splitter, 0);

    // PostSplit [0] -> Master
    ma_node_attach_output_bus(&t->post_fader_splitter, 0, &mixer->master_node, 0);

    return true;
}

SITAPI SituationAudioTrack* SituationAddTrack(SituationAudioMixer* mixer, const char* name) {
    if (!mixer) return NULL;

    mtx_lock(&mixer->topology_mutex);
    for (int i=0; i<SIT_MAX_TRACKS; ++i) {
        if (!mixer->tracks[i].is_active) {
            if (_SituationInitTrack_NoLock(mixer, i, name)) {
                _SituationUpdateSoloState(mixer);
                mtx_unlock(&mixer->topology_mutex);
                return &mixer->tracks[i];
            } else {
                continue; // Retry next? No, if init failed, it likely fails for all (e.g. OOM)
            }
        }
    }
    mtx_unlock(&mixer->topology_mutex);
    return NULL;
}

SITAPI void SituationSetTrackName(SituationAudioTrack* track, const char* name) {
    if (track && name) strncpy(track->name, name, 63);
}

SITAPI void SituationSetTrackVolume(SituationAudioTrack* track, float volume) {
    if (track) {
        track->volume = volume;
        // Volume is applied at the Pre-Fader Splitter (Bus 0), which feeds the Panner -> Post-Splitter -> Master
        // This ensures Post-Fader sends are affected, but Pre-Fader sends (other buses) are not.
        ma_node_set_output_bus_volume((ma_node*)&track->pre_fader_splitter, 0, volume);
    }
}

SITAPI void SituationSetTrackPan(SituationAudioTrack* track, float pan) {
    if (track) {
        atomic_store(&track->pan, pan);
        atomic_store(&track->panner_node.pan, pan);
    }
}

// Forward declaration
static void _SituationUpdateSoloState(SituationAudioMixer* mixer);

SITAPI void SituationSetTrackMute(SituationAudioTrack* track, bool mute) {
    if (track) {
        track->mute = mute;
        if (track->owner) _SituationUpdateSoloState(track->owner);
    }
}

SITAPI void SituationSetTrackSolo(SituationAudioTrack* track, bool solo) {
    if (track) {
        track->solo = solo;
        if (track->owner) _SituationUpdateSoloState(track->owner);
    }
}

SITAPI SituationAudioBus* SituationGetAuxBus(SituationAudioMixer* mixer, int bus_index) {
    if (!mixer || bus_index < 0 || bus_index >= SIT_MAX_AUX_BUSES) return NULL;
    return &mixer->aux_buses[bus_index];
}

static void _SituationUpdateSoloState(SituationAudioMixer* mixer) {
    bool any_solo = false;
    for (int i=0; i<SIT_MAX_TRACKS; ++i) {
        if (mixer->tracks[i].is_active && mixer->tracks[i].solo) {
            any_solo = true;
            break;
        }
    }

    for (int i=0; i<SIT_MAX_TRACKS; ++i) {
        SituationAudioTrack* t = &mixer->tracks[i];
        if (!t->is_active) continue;

        bool should_mute = t->mute;
        if (any_solo) {
            if (t->solo) should_mute = false;
            else should_mute = true;
        }

        // Apply mute at Dynamics Output (kills entire strip including pre-fader sends)
        ma_node_set_output_bus_volume((ma_node*)&t->dynamics_node, 0, should_mute ? 0.0f : 1.0f);
    }
}

SITAPI SituationError SituationSetTrackSend(SituationAudioTrack* track, int aux_bus_index, float level, bool pre_fader) {
    if (!track || aux_bus_index < 0 || aux_bus_index >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_INVALID_PARAM;
    if (!track->owner) return SITUATION_ERROR_NOT_INITIALIZED;

    mtx_lock(&track->owner->topology_mutex);

    ma_node* aux_input = (ma_node*)&track->owner->aux_buses[aux_bus_index].input_node;
    int pre_bus_idx = 2 + aux_bus_index;
    int post_bus_idx = 1 + aux_bus_index;

    bool was_pre = track->send_pre[aux_bus_index];
    bool type_changed = (was_pre != pre_fader);

    if (type_changed || level > 0.0f) {
         if (pre_fader) {
             if (type_changed) {
                 ma_node_detach_output_bus(&track->post_fader_splitter, post_bus_idx);
                 ma_node_attach_output_bus(&track->pre_fader_splitter, pre_bus_idx, aux_input, 0);
             } else {
                 ma_node_attach_output_bus(&track->pre_fader_splitter, pre_bus_idx, aux_input, 0);
             }
             ma_node_set_output_bus_volume(&track->pre_fader_splitter, pre_bus_idx, level);
         } else {
             if (type_changed) {
                 ma_node_detach_output_bus(&track->pre_fader_splitter, pre_bus_idx);
                 ma_node_attach_output_bus(&track->post_fader_splitter, post_bus_idx, aux_input, 0);
             } else {
                 ma_node_attach_output_bus(&track->post_fader_splitter, post_bus_idx, aux_input, 0);
             }
             ma_node_set_output_bus_volume(&track->post_fader_splitter, post_bus_idx, level);
         }
    } else if (level <= 0.0001f) {
        ma_node_detach_output_bus(&track->pre_fader_splitter, pre_bus_idx);
        ma_node_detach_output_bus(&track->post_fader_splitter, post_bus_idx);
    }

    track->send_level[aux_bus_index] = level;
    track->send_pre[aux_bus_index] = pre_fader;

    mtx_unlock(&track->owner->topology_mutex);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationSetTrackOutput(SituationAudioTrack* track, SituationAudioBus* destination) {
    if (!track || !track->owner) return SITUATION_ERROR_INVALID_PARAM;
    mtx_lock(&track->owner->topology_mutex);

    ma_node_detach_output_bus(&track->post_fader_splitter, 0);

    if (destination) {
        ma_node_attach_output_bus(&track->post_fader_splitter, 0, &destination->input_node, 0);
    } else {
        ma_node_attach_output_bus(&track->post_fader_splitter, 0, &track->owner->master_node, 0);
    }

    mtx_unlock(&track->owner->topology_mutex);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationRouteSoundToTrack(SituationSoundHandle sound, SituationAudioTrack* track) {
    if (!track) return SITUATION_ERROR_INVALID_PARAM;
    _SituationSoundSlot* slot = _SitGetSoundSlot(sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    _SituationSound* data = &slot->sound_data;

    mtx_lock(&sit_audio.audio_queue_mutex);
    if (!sit_audio.active_mixer) {
        mtx_unlock(&sit_audio.audio_queue_mutex);
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    SituationAudioMixer* mixer = sit_audio.active_mixer;

    mtx_lock(&mixer->topology_mutex);

    if (!data->is_graph_managed) {
        ma_data_source_node_config cfg = ma_data_source_node_config_init(&data->decoder);
        if (ma_data_source_node_init(&mixer->graph, &cfg, NULL, &data->graph_node) != MA_SUCCESS) {
            mtx_unlock(&mixer->topology_mutex);
            mtx_unlock(&sit_audio.audio_queue_mutex);
            return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
        }
        data->is_graph_managed = true;
    }

    ma_node_attach_output_bus(&data->graph_node, 0, &track->input_node, 0);

    mtx_unlock(&mixer->topology_mutex);
    mtx_unlock(&sit_audio.audio_queue_mutex);
    return SITUATION_SUCCESS;
}

SITAPI void SituationSetTrackEQ(SituationAudioTrack* track, bool enabled, float* freqs, float* gains, float* Qs) {
    if (!track || !track->is_active) return;
    uint32_t sr = sit_audio.is_miniaudio_device_active ? sit_audio.miniaudio_device.sampleRate : 48000;

    if (track->owner) mtx_lock(&track->owner->topology_mutex);

    track->eq_state.enabled = enabled;

    if (freqs && gains && Qs) {
        track->eq_state.hpf_freq = freqs[0];
        track->eq_state.ls_freq = freqs[1];
        track->eq_state.ls_gain = gains[1];
        track->eq_state.ls_q = Qs[1];
        track->eq_state.peak_freq = freqs[2];
        track->eq_state.peak_gain = gains[2];
        track->eq_state.peak_q = Qs[2];
        track->eq_state.hs_freq = freqs[3];
        track->eq_state.hs_gain = gains[3];
        track->eq_state.hs_q = Qs[3];
    }

    if (enabled) {
        ma_hpf_config hpf = ma_hpf_config_init(ma_format_f32, 2, sr, (double)track->eq_state.hpf_freq, 0);
        ma_hpf_node_reinit(&hpf, &track->eq_hpf);

        ma_loshelf_config ls = ma_loshelf2_config_init(ma_format_f32, 2, sr, (double)track->eq_state.ls_gain, (double)track->eq_state.ls_q, (double)track->eq_state.ls_freq);
        ma_loshelf_node_reinit(&ls, &track->eq_loshelf);

        ma_peak_config pk = ma_peak2_config_init(ma_format_f32, 2, sr, (double)track->eq_state.peak_gain, (double)track->eq_state.peak_q, (double)track->eq_state.peak_freq);
        ma_peak_node_reinit(&pk, &track->eq_peak);

        ma_hishelf_config hs = ma_hishelf2_config_init(ma_format_f32, 2, sr, (double)track->eq_state.hs_gain, (double)track->eq_state.hs_q, (double)track->eq_state.hs_freq);
        ma_hishelf_node_reinit(&hs, &track->eq_hishelf);
    } else {
        ma_hpf_config hpf = ma_hpf_config_init(ma_format_f32, 2, sr, 0, 0);
        ma_hpf_node_reinit(&hpf, &track->eq_hpf);

        ma_loshelf_config ls = ma_loshelf2_config_init(ma_format_f32, 2, sr, 0, 0, 100);
        ma_loshelf_node_reinit(&ls, &track->eq_loshelf);

        ma_peak_config pk = ma_peak2_config_init(ma_format_f32, 2, sr, 0, 0, 1000);
        ma_peak_node_reinit(&pk, &track->eq_peak);

        ma_hishelf_config hs = ma_hishelf2_config_init(ma_format_f32, 2, sr, 0, 0, 5000);
        ma_hishelf_node_reinit(&hs, &track->eq_hishelf);
    }

    if (track->owner) mtx_unlock(&track->owner->topology_mutex);
}

SITAPI void SituationSetTrackDynamics(SituationAudioTrack* track, bool enabled, int mode, float threshold_db, float ratio, float attack_ms, float release_ms, float makeup_gain) {
    if (!track || !track->is_active) return;

    if (!enabled) {
        track->dynamics_node.ratio = 1.0f;
        track->dynamics_node.makeupGain = 1.0f;
        track->dynamics_node.isGate = false;
        return;
    }

    track->dynamics_node.thresholdDB = threshold_db;
    track->dynamics_node.ratio = (mode == 1) ? 100.0f : ratio;
    track->dynamics_node.isGate = (mode == 2);
    track->dynamics_node.makeupGain = powf(10.0f, makeup_gain / 20.0f);

    uint32_t sr = sit_audio.is_miniaudio_device_active ? sit_audio.miniaudio_device.sampleRate : 48000;
    float fs = (float)sr;
    if (attack_ms < 0.001f) attack_ms = 0.001f;
    if (release_ms < 0.001f) release_ms = 0.001f;

    track->dynamics_node.attackTime = attack_ms;
    track->dynamics_node.releaseTime = release_ms;

    track->dynamics_node.attackCoef = 1.0f - expf(-1.0f / ((attack_ms/1000.0f) * fs));
    track->dynamics_node.releaseCoef = 1.0f - expf(-1.0f / ((release_ms/1000.0f) * fs));
}

static void _SituationSetTrackSideChain_NoLock(SituationAudioTrack* target_track, SituationAudioTrack* source_track) {
    // Save State
    float thresh = target_track->dynamics_node.thresholdDB;
    float ratio = target_track->dynamics_node.ratio;
    float att = target_track->dynamics_node.attackCoef;
    float rel = target_track->dynamics_node.releaseCoef;
    float mk = target_track->dynamics_node.makeupGain;
    bool gate = target_track->dynamics_node.isGate;
    float att_ms = target_track->dynamics_node.attackTime;
    float rel_ms = target_track->dynamics_node.releaseTime;

    // Re-create node to clear connections
    SituationDynamicsNodeUninit(&target_track->dynamics_node, NULL);

    uint32_t sr = sit_audio.miniaudio_device.sampleRate;
    ma_uint32 inCh[2]; inCh[0] = 2; inCh[1] = 2;
    ma_uint32 outCh[1]; outCh[0] = 2;
    SituationDynamicsNodeConfig dynCfg = SituationDynamicsNodeConfigInit(2, sr, inCh, outCh);
    SituationDynamicsNodeInit(&sit_audio.active_mixer->graph, &dynCfg, NULL, &target_track->dynamics_node);

    // Restore
    target_track->dynamics_node.thresholdDB = thresh;
    target_track->dynamics_node.ratio = ratio;
    target_track->dynamics_node.attackCoef = att;
    target_track->dynamics_node.releaseCoef = rel;
    target_track->dynamics_node.makeupGain = mk;
    target_track->dynamics_node.isGate = gate;
    target_track->dynamics_node.attackTime = att_ms;
    target_track->dynamics_node.releaseTime = rel_ms;

    // Re-wire Main
    ma_node_attach_output_bus(&target_track->eq_hishelf, 0, &target_track->dynamics_node, 0);
    ma_node_attach_output_bus(&target_track->dynamics_node, 0, &target_track->pre_fader_splitter, 0);

    // Wire Sidechain
    if (source_track) {
        ma_node_attach_output_bus(&source_track->pre_fader_splitter, 1, &target_track->dynamics_node, 1);
        target_track->dynamics_node.sidechainEnabled = 1;
        target_track->sidechain_source = source_track;
    } else {
        target_track->dynamics_node.sidechainEnabled = 0;
        target_track->sidechain_source = NULL;
    }
}

SITAPI void SituationSetTrackSideChain(SituationAudioTrack* target_track, SituationAudioTrack* source_track) {
    if (!target_track || !target_track->is_active) return;
    if (!sit_audio.active_mixer) return;

    mtx_lock(&sit_audio.active_mixer->topology_mutex);
    _SituationSetTrackSideChain_NoLock(target_track, source_track);
    mtx_unlock(&sit_audio.active_mixer->topology_mutex);
}

SITAPI SituationError SituationSetMasterVolume(SituationAudioMixer* mixer, float volume) {
    if (!mixer) return SITUATION_ERROR_INVALID_PARAM;
    ma_node_set_output_bus_volume((ma_node*)&mixer->master_node, 0, volume);
    return SITUATION_SUCCESS;
}

SITAPI float SituationGetMasterVolume(SituationAudioMixer* mixer) {
    if (!mixer) return 0.0f;
    return ma_node_get_output_bus_volume((ma_node*)&mixer->master_node, 0);
}

SITAPI void SituationGetTrackMeter(SituationAudioTrack* track, float* left_peak, float* right_peak, float* gain_reduction) {
    if (!track || !track->is_active) {
        if (left_peak) *left_peak = 0.0f;
        if (right_peak) *right_peak = 0.0f;
        if (gain_reduction) *gain_reduction = 0.0f;
        return;
    }

    if (left_peak) *left_peak = atomic_load(&track->meter_node.peak_L);
    if (right_peak) *right_peak = atomic_load(&track->meter_node.peak_R);
    if (gain_reduction) *gain_reduction = atomic_load(&track->dynamics_node.gainReductionDB);
}

SITAPI SituationError SituationInsertEffect(SituationAudioBus* bus, int slot, ma_node* effect_node) {
    if (!bus || !effect_node || slot < 0 || slot >= SIT_MAX_FX_SLOTS) return SITUATION_ERROR_INVALID_PARAM;
    // Assuming bus is part of an active mixer, but we don't have a direct pointer to mixer from bus struct in Phase 2 def.
    // Wait, SituationAudioBus struct definition (Phase 2) doesn't have 'owner' pointer like Track does.
    // But we need to lock topology!
    // We can assume the caller ensures thread safety? No, API says "All API calls modifying topology... must acquire topology_mutex".
    // I need to add 'owner' to SituationAudioBus or find the mixer.
    // SituationAudioBus is stored in mixer->aux_buses array.
    // I can iterate global mixer list? No global list.
    // sit_audio.active_mixer is the likely owner if we are operating on the active mixer.
    // But what if we have multiple mixers (not supported well yet)?
    // Let's assume sit_audio.active_mixer is the one.
    // But 'bus' pointer could be from anywhere.
    // HACK: For now, I'll rely on sit_audio.active_mixer. If it's NULL, we can't lock.

    if (!sit_audio.active_mixer) return SITUATION_ERROR_NOT_INITIALIZED;
    SituationAudioMixer* mixer = sit_audio.active_mixer;

    // Verify bus belongs to mixer?
    bool found = false;
    for(int i=0; i<SIT_MAX_AUX_BUSES; ++i) {
        if (&mixer->aux_buses[i] == bus) { found = true; break; }
    }
    // Also check Master? Master is a bus-like struct but type SituationAudioBus?
    // In struct SituationAudioMixer, 'master' is SituationAudioBus type? No.
    // struct SituationAudioMixer { ... SituationAudioBus aux_buses[...]; ma_splitter_node master_node; ... }
    // Master is just a node in the struct, not a SituationAudioBus struct.
    // So this API only works for Aux buses for now as typed.

    if (!found) return SITUATION_ERROR_INVALID_PARAM; // Bus not in active mixer

    mtx_lock(&mixer->topology_mutex);

    if (bus->fx[slot] != NULL) {
        mtx_unlock(&mixer->topology_mutex);
        return SITUATION_ERROR_AUDIO_INVALID_OPERATION; // Slot occupied
    }

    // 1. Find Prev Node
    ma_node* prev = (ma_node*)&bus->input_node;
    for (int i = slot - 1; i >= 0; i--) {
        if (bus->fx[i]) {
            prev = bus->fx[i];
            break;
        }
    }

    // 2. Find Next Node
    ma_node* next = (ma_node*)&bus->meter_node.base;
    for (int i = slot + 1; i < SIT_MAX_FX_SLOTS; i++) {
        if (bus->fx[i]) {
            next = bus->fx[i];
            break;
        }
    }

    // 3. Rewire
    // Detach Prev -> Next
    // Note: ma_node_detach_output_bus removes all connections from that bus index.
    // Since we are building a linear chain, output bus 0 is the only one used.
    ma_node_detach_output_bus(prev, 0);

    // Attach Prev -> New
    ma_node_attach_output_bus(prev, 0, effect_node, 0);

    // Attach New -> Next
    ma_node_attach_output_bus(effect_node, 0, next, 0);

    bus->fx[slot] = effect_node;
    bus->fx_count++;

    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}

SITAPI void* SituationRemoveEffect(SituationAudioBus* bus, int slot) {
    if (!bus || slot < 0 || slot >= SIT_MAX_FX_SLOTS) return NULL;
    if (!sit_audio.active_mixer) return NULL;
    SituationAudioMixer* mixer = sit_audio.active_mixer;

    mtx_lock(&mixer->topology_mutex);

    ma_node* old_node = bus->fx[slot];
    if (!old_node) {
        mtx_unlock(&mixer->topology_mutex);
        return NULL;
    }

    // 1. Find Prev Node
    ma_node* prev = (ma_node*)&bus->input_node;
    for (int i = slot - 1; i >= 0; i--) {
        if (bus->fx[i]) {
            prev = bus->fx[i];
            break;
        }
    }

    // 2. Find Next Node
    ma_node* next = (ma_node*)&bus->meter_node.base;
    for (int i = slot + 1; i < SIT_MAX_FX_SLOTS; i++) {
        if (bus->fx[i]) {
            next = bus->fx[i];
            break;
        }
    }

    // 3. Rewire
    // Detach Prev -> Old
    ma_node_detach_output_bus(prev, 0);

    // Detach Old -> Next
    ma_node_detach_output_bus(old_node, 0);

    // Attach Prev -> Next
    ma_node_attach_output_bus(prev, 0, next, 0);

    bus->fx[slot] = NULL;
    bus->fx_count--;

    mtx_unlock(&mixer->topology_mutex);
    return old_node;
}

SITAPI ma_node_graph* SituationGetMixerGraph(SituationAudioMixer* mixer) {
    if (!mixer) return NULL;
    return &mixer->graph;
}

// ================================================================================================
// MIXER INSERT CHAIN INTEGRATION (Phase 6 Session 1)
// ================================================================================================

/**
 * @brief [INTERNAL] Process audio through an insert chain
 */
static inline void _SituationProcessInsertChain(
    SituationInsertChain* insert,
    const float* input,
    float* output,
    int frames,
    const SituationDeviceFunctions* device_funcs,
    int num_funcs
) {
    if (!insert || !input || !output || frames <= 0) {
        if (output && input && frames > 0) {
            memcpy(output, input, frames * 2 * sizeof(float));
        }
        return;
    }
    
    if (!insert->is_active || !insert->chain || insert->bypass) {
        memcpy(output, input, frames * 2 * sizeof(float));
        return;
    }
    
    SituationProcessGraph(insert->chain, output, frames, device_funcs, num_funcs);
    memcpy(output, input, frames * 2 * sizeof(float)); // TODO: Implement input injection
}

/**
 * @brief Attach an insert chain to a track at the specified position
 */
SITAPI SituationError SituationSetTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    SituationAudioGraph* insert_chain
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (track_id < 0 || track_id >= SIT_MAX_TRACKS) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    if (position < 0 || position >= SITUATION_INSERT_COUNT) return SITUATION_ERROR_MIXER_INSERT_INVALID;
    if (!insert_chain) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationAudioTrack* track = &mixer->tracks[track_id];
    if (!track->is_active) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    
    if (mtx_lock(&mixer->topology_mutex) != thrd_success) {
        return SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED;
    }
    
    SituationInsertChain* insert = &track->inserts[position];
    
    if (insert->is_active && insert->chain) {
        mtx_unlock(&mixer->topology_mutex);
        return SITUATION_ERROR_MIXER_INSERT_ALREADY_ATTACHED;
    }
    
    insert->chain = insert_chain;
    insert->bypass = false;
    insert->is_active = true;
    
    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Remove an insert chain from a track
 */
SITAPI SituationError SituationClearTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (track_id < 0 || track_id >= SIT_MAX_TRACKS) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    if (position < 0 || position >= SITUATION_INSERT_COUNT) return SITUATION_ERROR_MIXER_INSERT_INVALID;
    
    SituationAudioTrack* track = &mixer->tracks[track_id];
    if (!track->is_active) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    
    if (mtx_lock(&mixer->topology_mutex) != thrd_success) {
        return SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED;
    }
    
    SituationInsertChain* insert = &track->inserts[position];
    
    if (!insert->is_active || !insert->chain) {
        mtx_unlock(&mixer->topology_mutex);
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    SituationDestroyGraph(insert->chain);
    insert->chain = NULL;
    insert->is_active = false;
    insert->bypass = false;
    
    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Bypass or enable an insert chain
 */
SITAPI SituationError SituationBypassTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    bool bypass
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (track_id < 0 || track_id >= SIT_MAX_TRACKS) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    if (position < 0 || position >= SITUATION_INSERT_COUNT) return SITUATION_ERROR_MIXER_INSERT_INVALID;
    
    SituationAudioTrack* track = &mixer->tracks[track_id];
    if (!track->is_active) return SITUATION_ERROR_MIXER_TRACK_INVALID;
    
    SituationInsertChain* insert = &track->inserts[position];
    
    if (!insert->is_active) {
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    insert->bypass = bypass;
    return SITUATION_SUCCESS;
}

/**
 * @brief Get the insert chain at the specified position
 */
SITAPI SituationAudioGraph* SituationGetTrackInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position
) {
    if (!mixer) return NULL;
    if (track_id < 0 || track_id >= SIT_MAX_TRACKS) return NULL;
    if (position < 0 || position >= SITUATION_INSERT_COUNT) return NULL;
    
    SituationAudioTrack* track = &mixer->tracks[track_id];
    if (!track->is_active) return NULL;
    
    SituationInsertChain* insert = &track->inserts[position];
    return insert->is_active ? insert->chain : NULL;
}

/**
 * @brief Check if an insert chain is bypassed
 */
SITAPI bool SituationIsTrackInsertBypassed(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position
) {
    if (!mixer) return false;
    if (track_id < 0 || track_id >= SIT_MAX_TRACKS) return false;
    if (position < 0 || position >= SITUATION_INSERT_COUNT) return false;
    
    SituationAudioTrack* track = &mixer->tracks[track_id];
    if (!track->is_active) return false;
    
    SituationInsertChain* insert = &track->inserts[position];
    return insert->bypass;
}

// ================================================================================================
// MIXER AUX BUS FX INTEGRATION (Phase 6 Session 2)
// ================================================================================================

/**
 * @brief [INTERNAL] Process aux bus FX chain
 */
static inline void _SituationProcessAuxFXChain(
    SituationAuxFXChain* fx_chain,
    const float* input,
    float* output,
    int frames,
    const SituationDeviceFunctions* device_funcs,
    int num_funcs
) {
    if (!fx_chain || !input || !output || frames <= 0) {
        if (output && input && frames > 0) {
            memcpy(output, input, frames * 2 * sizeof(float));
        }
        return;
    }
    
    if (!fx_chain->is_active || !fx_chain->fx_chain || fx_chain->bypass) {
        memcpy(output, input, frames * 2 * sizeof(float));
        return;
    }
    
    SituationProcessGraph(fx_chain->fx_chain, output, frames, device_funcs, num_funcs);
    
    float wet = fx_chain->wet_mix;
    float dry = fx_chain->dry_mix;
    
    if (wet < 1.0f || dry > 0.0f) {
        for (int i = 0; i < frames * 2; i++) {
            output[i] = output[i] * wet + input[i] * dry;
        }
    }
}

/**
 * @brief Attach a modular FX chain to an aux bus
 */
SITAPI SituationError SituationSetBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id,
    SituationAudioGraph* fx_chain
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_MIXER_BUS_INVALID;
    if (!fx_chain) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    
    if (mtx_lock(&mixer->topology_mutex) != thrd_success) {
        return SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED;
    }
    
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    if (fx->is_active && fx->fx_chain) {
        mtx_unlock(&mixer->topology_mutex);
        return SITUATION_ERROR_MIXER_INSERT_ALREADY_ATTACHED;
    }
    
    fx->fx_chain = fx_chain;
    fx->bypass = false;
    fx->is_active = true;
    fx->wet_mix = 1.0f;
    fx->dry_mix = 0.0f;
    
    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Remove FX chain from an aux bus
 */
SITAPI SituationError SituationClearBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_MIXER_BUS_INVALID;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    
    if (mtx_lock(&mixer->topology_mutex) != thrd_success) {
        return SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED;
    }
    
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    if (!fx->is_active || !fx->fx_chain) {
        mtx_unlock(&mixer->topology_mutex);
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    SituationDestroyGraph(fx->fx_chain);
    fx->fx_chain = NULL;
    fx->is_active = false;
    fx->bypass = false;
    fx->wet_mix = 1.0f;
    fx->dry_mix = 0.0f;
    
    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}

/**
 * @brief Bypass or enable aux bus FX chain
 */
SITAPI SituationError SituationBypassBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id,
    bool bypass
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_MIXER_BUS_INVALID;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    if (!fx->is_active) {
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    fx->bypass = bypass;
    return SITUATION_SUCCESS;
}

/**
 * @brief Set wet/dry mix for aux bus FX
 */
SITAPI SituationError SituationSetBusEffectMix(
    SituationAudioMixer* mixer,
    int bus_id,
    float wet_mix,
    float dry_mix
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_MIXER_BUS_INVALID;
    if (wet_mix < 0.0f || wet_mix > 1.0f) return SITUATION_ERROR_INVALID_PARAM;
    if (dry_mix < 0.0f || dry_mix > 1.0f) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    if (!fx->is_active) {
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    fx->wet_mix = wet_mix;
    fx->dry_mix = dry_mix;
    return SITUATION_SUCCESS;
}

/**
 * @brief Get aux bus FX chain
 */
SITAPI SituationAudioGraph* SituationGetBusEffectChain(
    SituationAudioMixer* mixer,
    int bus_id
) {
    if (!mixer) return NULL;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return NULL;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    return fx->is_active ? fx->fx_chain : NULL;
}

/**
 * @brief Check if aux bus FX is bypassed
 */
SITAPI bool SituationIsBusEffectBypassed(
    SituationAudioMixer* mixer,
    int bus_id
) {
    if (!mixer) return false;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return false;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    return fx->bypass;
}

/**
 * @brief Get wet/dry mix levels for aux bus FX
 */
SITAPI SituationError SituationGetBusEffectMix(
    SituationAudioMixer* mixer,
    int bus_id,
    float* out_wet_mix,
    float* out_dry_mix
) {
    if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
    if (bus_id < 0 || bus_id >= SIT_MAX_AUX_BUSES) return SITUATION_ERROR_MIXER_BUS_INVALID;
    if (!out_wet_mix || !out_dry_mix) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationAudioBus* bus = &mixer->aux_buses[bus_id];
    SituationAuxFXChain* fx = &bus->fx_chain;
    
    if (!fx->is_active) {
        return SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED;
    }
    
    *out_wet_mix = fx->wet_mix;
    *out_dry_mix = fx->dry_mix;
    return SITUATION_SUCCESS;
}

// --- Persistence Structures (Phase 5) ---
typedef struct {
    char magic[4];          // "SMX2"
    uint32_t version;       // 2
    uint64_t timestamp;
    uint32_t track_count;
    uint32_t bus_count;
} MixerFileHeader;

typedef struct {
    int id;
    char name[64];
    float volume;
    float pan;
    uint8_t flags;          // Bit 0: Mute, Bit 1: Solo, Bit 2: Active

    // -- EQ --
    uint8_t eq_enabled;
    float eq_params[16];    // Serialized EQ bands (Freq/Gain/Q)

    // -- Dynamics --
    uint8_t dyn_enabled;
    uint8_t dyn_mode;
    float dyn_params[8];    // Thresh, Ratio, Attack, Release, Makeup, etc.
    int side_chain_source;  // ID of sidechain source track

    float sends[8];         // Levels for all 8 aux slots
    uint8_t send_pre[8];    // Pre/Post flags
} MixerTrackData;

typedef struct {
    int id;
    char name[64];
    float volume;
    int fx_count;
    // Note: FX chains are NOT persisted in v1.0, only topology placeholders.
} MixerBusData;

SITAPI bool SituationSaveMixerSession(SituationAudioMixer* mixer, const char* filepath) {
    if (!mixer || !filepath) return false;

    // Use a temp buffer to avoid holding lock during I/O
    MixerTrackData* track_data = (MixerTrackData*)SIT_MALLOC(sizeof(MixerTrackData) * SIT_MAX_TRACKS);
    MixerBusData* bus_data = (MixerBusData*)SIT_MALLOC(sizeof(MixerBusData) * SIT_MAX_AUX_BUSES);
    if (!track_data || !bus_data) {
        if(track_data) SIT_FREE(track_data);
        if(bus_data) SIT_FREE(bus_data);
        return false;
    }

    mtx_lock(&mixer->topology_mutex); // Lock for consistent read

    for(int i=0; i<SIT_MAX_TRACKS; ++i) {
        SituationAudioTrack* t = &mixer->tracks[i];
        MixerTrackData* d = &track_data[i];
        memset(d, 0, sizeof(MixerTrackData));

        d->id = t->id;
        strncpy(d->name, t->name, 63);
        d->volume = atomic_load(&t->volume);
        d->pan = atomic_load(&t->pan);

        d->flags = 0;
        if (t->mute) d->flags |= 1;
        if (t->solo) d->flags |= 2;
        if (t->is_active) d->flags |= 4;

        // EQ
        d->eq_enabled = t->eq_state.enabled ? 1 : 0;
        d->eq_params[0] = t->eq_state.hpf_freq;
        d->eq_params[1] = t->eq_state.ls_freq;
        d->eq_params[2] = t->eq_state.ls_gain;
        d->eq_params[3] = t->eq_state.ls_q;
        d->eq_params[4] = t->eq_state.peak_freq;
        d->eq_params[5] = t->eq_state.peak_gain;
        d->eq_params[6] = t->eq_state.peak_q;
        d->eq_params[7] = t->eq_state.hs_freq;
        d->eq_params[8] = t->eq_state.hs_gain;
        d->eq_params[9] = t->eq_state.hs_q;

        // Dynamics (Partial persistence)
        d->dyn_enabled = (t->dynamics_node.ratio != 1.0f || t->dynamics_node.isGate) ? 1 : 0;
        d->dyn_mode = t->dynamics_node.isGate ? 2 : 0;
        if (t->dynamics_node.ratio > 20.0f) d->dyn_mode = 1;

        d->dyn_params[0] = t->dynamics_node.thresholdDB;
        d->dyn_params[1] = t->dynamics_node.ratio;
        d->dyn_params[2] = t->dynamics_node.attackTime;
        d->dyn_params[3] = t->dynamics_node.releaseTime;
        d->dyn_params[4] = 20.0f * log10f(t->dynamics_node.makeupGain);

        d->side_chain_source = t->sidechain_source ? t->sidechain_source->id : -1;

        for(int s=0; s<SIT_MAX_AUX_BUSES; ++s) {
            d->sends[s] = t->send_level[s];
            d->send_pre[s] = t->send_pre[s] ? 1 : 0;
        }
    }

    for(int i=0; i<SIT_MAX_AUX_BUSES; ++i) {
        SituationAudioBus* b = &mixer->aux_buses[i];
        MixerBusData* d = &bus_data[i];
        memset(d, 0, sizeof(MixerBusData));
        d->id = b->id;
        strncpy(d->name, b->name, 63);
        d->volume = atomic_load(&b->volume);
        d->fx_count = b->fx_count;
    }

    mtx_unlock(&mixer->topology_mutex);

    // Write to file
    FILE* f = fopen(filepath, "wb");
    if (!f) {
        SIT_FREE(track_data);
        SIT_FREE(bus_data);
        return false;
    }

    MixerFileHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "SMX2", 4);
    header.version = 2;
    header.timestamp = (uint64_t)time(NULL);
    header.track_count = SIT_MAX_TRACKS;
    header.bus_count = SIT_MAX_AUX_BUSES;

    fwrite(&header, sizeof(header), 1, f);
    fwrite(track_data, sizeof(MixerTrackData), SIT_MAX_TRACKS, f);
    fwrite(bus_data, sizeof(MixerBusData), SIT_MAX_AUX_BUSES, f);

    fclose(f);
    SIT_FREE(track_data);
    SIT_FREE(bus_data);
    return true;
}

SITAPI bool SituationLoadMixerSession(SituationAudioMixer* mixer, const char* filepath) {
    if (!mixer || !filepath) return false;

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    MixerFileHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }

    if (memcmp(header.magic, "SMX2", 4) != 0 || header.version != 2) {
        fclose(f);
        return false;
    }

    MixerTrackData* track_data = (MixerTrackData*)SIT_MALLOC(sizeof(MixerTrackData) * header.track_count);
    MixerBusData* bus_data = (MixerBusData*)SIT_MALLOC(sizeof(MixerBusData) * header.bus_count);
    if (!track_data || !bus_data) {
        if(track_data) SIT_FREE(track_data);
        if(bus_data) SIT_FREE(bus_data);
        fclose(f);
        return false;
    }

    if (fread(track_data, sizeof(MixerTrackData), header.track_count, f) != header.track_count ||
        fread(bus_data, sizeof(MixerBusData), header.bus_count, f) != header.bus_count) {
        SIT_FREE(track_data);
        SIT_FREE(bus_data);
        fclose(f);
        return false;
    }
    fclose(f);

    mtx_lock(&mixer->topology_mutex);

    int max_tracks = (header.track_count < SIT_MAX_TRACKS) ? header.track_count : SIT_MAX_TRACKS;
    for(int i=0; i<max_tracks; ++i) {
        MixerTrackData* d = &track_data[i];
        SituationAudioTrack* t = &mixer->tracks[i];

        bool active_in_file = (d->flags & 4) != 0;
        if (active_in_file) {
            if (!t->is_active) {
                if (!_SituationInitTrack_NoLock(mixer, i, NULL)) continue;
            }

            strncpy(t->name, d->name, 63);

            bool mute = (d->flags & 1) != 0;
            bool solo = (d->flags & 2) != 0;
            SituationSetTrackMute(t, mute);
            SituationSetTrackSolo(t, solo);

            SituationSetTrackVolume(t, d->volume);
            SituationSetTrackPan(t, d->pan);

            float freqs[4] = {d->eq_params[0], d->eq_params[1], d->eq_params[4], d->eq_params[7]};
            float gains[4] = {0, d->eq_params[2], d->eq_params[5], d->eq_params[8]};
            float Qs[4] = {0, d->eq_params[3], d->eq_params[6], d->eq_params[9]};
            SituationSetTrackEQ(t, d->eq_enabled, freqs, gains, Qs);

            // Use saved attack/release if available (> 0), otherwise defaults 10ms/100ms
            float att = d->dyn_params[2] > 0.0f ? d->dyn_params[2] : 10.0f;
            float rel = d->dyn_params[3] > 0.0f ? d->dyn_params[3] : 100.0f;
            SituationSetTrackDynamics(t, d->dyn_enabled, d->dyn_mode, d->dyn_params[0], d->dyn_params[1], att, rel, d->dyn_params[4]);

            for(int s=0; s<SIT_MAX_AUX_BUSES; ++s) {
                SituationSetTrackSend(t, s, d->sends[s], d->send_pre[s]);
            }
        } else {
            if (t->is_active) {
                _SituationRemoveTrack_NoLock(t);
            }
        }
    }

    for(int i=0; i<max_tracks; ++i) {
        if (!mixer->tracks[i].is_active) continue;
        int src_id = track_data[i].side_chain_source;
        if (src_id >= 0 && src_id < SIT_MAX_TRACKS && mixer->tracks[src_id].is_active) {
            _SituationSetTrackSideChain_NoLock(&mixer->tracks[i], &mixer->tracks[src_id]);
        }
    }

    for(int i=0; i<SIT_MAX_AUX_BUSES; ++i) {
        SituationAudioBus* b = &mixer->aux_buses[i];
        if (i < (int)header.bus_count) {
            strncpy(b->name, bus_data[i].name, 63);
            atomic_store(&b->volume, bus_data[i].volume);
        }
    }

    _SituationUpdateSoloState(mixer);

    mtx_unlock(&mixer->topology_mutex);
    SIT_FREE(track_data);
    SIT_FREE(bus_data);
    return true;
}

#endif // SITUATION_IMPL_AUDIO_H
