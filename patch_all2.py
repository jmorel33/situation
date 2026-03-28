import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

struct_def = """
typedef enum {
    SIT_AUDIO_CMD_PLAY_SOUND,
    SIT_AUDIO_CMD_STOP_SOUND,
    SIT_AUDIO_CMD_STOP_ALL_SOUNDS,
    SIT_AUDIO_CMD_SET_SOUND_VOLUME,
    SIT_AUDIO_CMD_SET_SOUND_PAN,
    SIT_AUDIO_CMD_SET_SOUND_PITCH,
    SIT_AUDIO_CMD_PLAY_TONE,
    SIT_AUDIO_CMD_STOP_TONE
} SituationAudioCommandType;

typedef struct {
    SituationAudioCommandType type;
    struct _SituationSound* sound;
    SituationWaveType tone_type;
    float frequency;
    float pan;
    float attack_sec;
    float decay_sec;
    float sustain_level;
    float release_sec;
    float hold_sec;
    uint32_t tone_id;
    float value;
} SituationAudioCommand;

#define SIT_AUDIO_CMD_QUEUE_SIZE 512
"""

match = re.search(r'typedef struct \{\n    // -------------------------------------------------------------------------\n    // Audio Subsystem \(MiniAudio\)', data)
if match:
    data = data[:match.start()] + struct_def + "\n" + data[match.start():]

data = re.sub(r'mtx_t audio_queue_mutex;[^\n]*',
    '''SituationAudioCommand audio_command_queue[SIT_AUDIO_CMD_QUEUE_SIZE];
    atomic_size_t audio_command_head;
    atomic_size_t audio_command_tail;''', data)

data = re.sub(r'mtx_destroy\(&sit_audio\.audio_queue_mutex\);', '// mtx_destroy(&sit_audio.audio_queue_mutex); // Removed', data)
data = re.sub(r'if \(mtx_init\(&sit_audio\.audio_queue_mutex, mtx_recursive\) != thrd_success\) \{[^\}]+\}', '// audio_queue_mutex removed in favor of lock-free queue', data)

data = re.sub(r'sit_audio\.active_voice_count = 0;',
    '''sit_audio.active_voice_count = 0;
    atomic_init(&sit_audio.audio_command_head, 0);
    atomic_init(&sit_audio.audio_command_tail, 0);''', data)

# Thread affinity
affinity_func = """
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

static void _SituationSetThreadAffinity(bool high_perf) {
#if defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = high_perf ? 1 : 2; // Extremely simplified, ideally you query cores. Let's just set to 1 for high perf, and 2 for low perf.
    SetThreadAffinityMask(thread, mask);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (high_perf) {
        CPU_SET(0, &cpuset); // Assume core 0 is P-core
    } else {
        CPU_SET(1, &cpuset); // Assume core 1 is E-core
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
    #include <mach/mach_init.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
    thread_port_t mach_thread = mach_thread_self();
    thread_affinity_policy_data_t policyData = { high_perf ? 1 : 2 };
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policyData, THREAD_AFFINITY_POLICY_COUNT);
#endif
}
"""
if "_SituationSetThreadAffinity" not in data:
    data = re.sub(r'SITAPI SituationError SituationInit\(', affinity_func + '\nSITAPI SituationError SituationInit(', data)
data = re.sub(r'_SituationSetError\("SituationInit: No error. Initialization successful."\);\n\n    // --- 7. Return Success ---\n    return SITUATION_SUCCESS;', '_SituationSetError("SituationInit: No error. Initialization successful.");\n    _SituationSetThreadAffinity(true);\n\n    // --- 7. Return Success ---\n    return SITUATION_SUCCESS;', data)
data = re.sub(r'static int _SituationRenderThreadEntry\(void\* arg\) \{\n', r'static int _SituationRenderThreadEntry(void* arg) {\n    _SituationSetThreadAffinity(true);\n', data)
data = re.sub(r'static int _SituationWorkerEntry\(void\* arg\) \{\n', r'static int _SituationWorkerEntry(void* arg) {\n    _SituationSetThreadAffinity(false);\n', data)

# Error handling
fatal_func = """
static void _SituationFatalError(const char* msg) {
    fprintf(stderr, "FATAL ERROR: %s\\n", msg);
    exit(1);
}
"""
if "_SituationFatalError" not in data:
    data = re.sub(r'#if defined\(SITUATION_USE_VULKAN\)', r'#if defined(SITUATION_USE_VULKAN)\n' + fatal_func, data, count=1)
vulkan_patch_single1 = """
    if (submit_result == VK_ERROR_DEVICE_LOST) {
        _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during vkQueueSubmit. GPU crashed or disconnected. Terminating.");
    }
"""
submit1 = '    VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
if submit1 in data:
    data = data.replace(submit1, submit1 + vulkan_patch_single1)
submit2 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);'
data = data.replace(submit2, submit2 + vulkan_patch_single1)

vulkan_patch_thread = """
        if (submit_result == VK_ERROR_DEVICE_LOST) {
            _SituationFatalError("Vulkan Device Lost (VK_ERROR_DEVICE_LOST) during Render Thread vkQueueSubmit. GPU crashed or disconnected. Terminating.");
        }
"""
submit3 = '        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[frame_index]);'
data = data.replace(submit3, submit3 + vulkan_patch_thread)
data = re.sub(r'(        _SituationFatalError\("Vulkan Device Lost \(VK_ERROR_DEVICE_LOST\) during vkQueueSubmit\. GPU crashed or disconnected\. Terminating\."\);\n    \}\n)\s*if \(submit_result == VK_ERROR_DEVICE_LOST\) \{.*?\n    \}', r'\1', data, flags=re.DOTALL)
data = re.sub(r'(            _SituationFatalError\("Vulkan Device Lost \(VK_ERROR_DEVICE_LOST\) during Render Thread vkQueueSubmit\. GPU crashed or disconnected\. Terminating\."\);\n        \}\n)\s*if \(submit_result == VK_ERROR_DEVICE_LOST\) \{.*?\n        \}', r'\1', data, flags=re.DOTALL)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)

# 2. sit/situation_impl_audio.h
with open("sit/situation_impl_audio.h", "r") as f:
    data = f.read()

push_cmd = """
static void _SitPushAudioCommand(SituationAudioCommandType type, _SituationSound* sound, float value) {
    size_t head = atomic_load_explicit(&sit_audio.audio_command_head, memory_order_relaxed);
    size_t next_head = (head + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;
    sit_audio.audio_command_queue[head].type = type;
    sit_audio.audio_command_queue[head].sound = sound;
    sit_audio.audio_command_queue[head].value = value;
    atomic_store_explicit(&sit_audio.audio_command_head, next_head, memory_order_release);
}
"""
data = re.sub(r'SITAPI SituationError SituationPlayLoadedSound', push_cmd + '\nSITAPI SituationError SituationPlayLoadedSound', data)

play_replace = """
    _SituationSound* data = &slot->sound_data;

    if (data->is_preloaded) {
        data->cursor_frames = 0;
    } else if (data->is_initialized) {
        ma_decoder_seek_to_pcm_frame(&data->decoder, 0);
    }

    _SitPushAudioCommand(SIT_AUDIO_CMD_PLAY_SOUND, data, 0.0f);

    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*// Add to active voices.*?mtx_unlock\(&sit_audio\.audio_queue_mutex\);\s*return SITUATION_SUCCESS;\s*}', play_replace, data, flags=re.DOTALL)

stop_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_STOP_SOUND, data, 0.0f);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*mtx_lock\(&sit_audio\.audio_queue_mutex\);.*?mtx_unlock\(&sit_audio\.audio_queue_mutex\);\s*return SITUATION_SUCCESS;\s*}', stop_replace, data, flags=re.DOTALL)

stop_all_replace = """
SITAPI SituationError SituationStopAllLoadedSounds(void) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SitPushAudioCommand(SIT_AUDIO_CMD_STOP_ALL_SOUNDS, NULL, 0.0f);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'SITAPI SituationError SituationStopAllLoadedSounds\(void\) \{.*?\n\}', stop_all_replace, data, flags=re.DOTALL)


vol_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_SET_SOUND_VOLUME, data, volume);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*data->volume = volume;\s*return SITUATION_SUCCESS;\s*}', vol_replace, data, flags=re.DOTALL)

pan_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_SET_SOUND_PAN, data, pan);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*data->pan = pan;\s*return SITUATION_SUCCESS;\s*}', pan_replace, data, flags=re.DOTALL)

pitch_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_SET_SOUND_PITCH, data, pitch);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*data->pitch = pitch;\s*return SITUATION_SUCCESS;\s*}', pitch_replace, data, flags=re.DOTALL)


process_cmds = """
    // Process audio commands lock-free
    size_t tail = atomic_load_explicit(&pGs->audio_command_tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&pGs->audio_command_head, memory_order_acquire);
    while (tail != head) {
        SituationAudioCommand cmd = pGs->audio_command_queue[tail];

        if (cmd.type == SIT_AUDIO_CMD_PLAY_SOUND) {
            bool present = false;
            for (int i = 0; i < pGs->active_voice_count; i++) {
                if (pGs->active_voices[i] == cmd.sound) {
                    present = true;
                    break;
                }
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
        } else if (cmd.type == SIT_AUDIO_CMD_STOP_SOUND) {
            for (int i = 0; i < pGs->active_voice_count; i++) {
                if (pGs->active_voices[i] == cmd.sound) {
                    pGs->active_voices[i] = pGs->active_voices[pGs->active_voice_count - 1];
                    pGs->active_voice_count--;
                    break;
                }
            }
        } else if (cmd.type == SIT_AUDIO_CMD_STOP_ALL_SOUNDS) {
            pGs->active_voice_count = 0;
        } else if (cmd.type == SIT_AUDIO_CMD_SET_SOUND_VOLUME) {
            cmd.sound->volume = cmd.value;
        } else if (cmd.type == SIT_AUDIO_CMD_SET_SOUND_PAN) {
            cmd.sound->pan = cmd.value;
        } else if (cmd.type == SIT_AUDIO_CMD_SET_SOUND_PITCH) {
            cmd.sound->pitch = cmd.value;
        } else if (cmd.type == SIT_AUDIO_CMD_PLAY_TONE) {
            int slot = -1;
            for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                if (!pGs->tone_pool[i].active) {
                    slot = i;
                    break;
                }
            }
            if (slot == -1) {
                uint64_t max_release_cursor = 0;
                int best_release_slot = -1;
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].state == SIT_ENV_RELEASE) {
                        if (pGs->tone_pool[i].cursor_frames > max_release_cursor) {
                            max_release_cursor = pGs->tone_pool[i].cursor_frames;
                            best_release_slot = i;
                        }
                    }
                }
                slot = best_release_slot;
            }
            if (slot == -1) {
                uint64_t min_cursor = UINT64_MAX;
                int best_active_slot = -1;
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].cursor_frames < min_cursor) {
                        min_cursor = pGs->tone_pool[i].cursor_frames;
                        best_active_slot = i;
                    }
                }
                slot = best_active_slot;
            }

            if (slot != -1) {
                SituationTone* t = &pGs->tone_pool[slot];
                memset(t, 0, sizeof(SituationTone));
                t->id = cmd.tone_id;
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
                    ma_noise_config noiseConfig = ma_noise_config_init(t->format, t->channels, t->sample_rate, 0, ma_noise_type_white);
                    ma_noise_init(&noiseConfig, NULL, &t->noise);
                } else {
                    ma_waveform_type mtype = ma_waveform_type_sine;
                    if (t->type == SIT_WAVE_SQUARE) mtype = ma_waveform_type_square;
                    if (t->type == SIT_WAVE_TRIANGLE) mtype = ma_waveform_type_triangle;
                    if (t->type == SIT_WAVE_SAW) mtype = ma_waveform_type_sawtooth;
                    ma_waveform_config waveConfig = ma_waveform_config_init(t->format, t->channels, t->sample_rate, mtype, t->amplitude, t->frequency);
                    ma_waveform_init(&waveConfig, &t->waveform);
                }
            }
        } else if (cmd.type == SIT_AUDIO_CMD_STOP_TONE) {
            if (cmd.tone_id == 0) {
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].state != SIT_ENV_RELEASE) {
                        pGs->tone_pool[i].state = SIT_ENV_RELEASE;
                        pGs->tone_pool[i].cursor_frames = 0;
                    }
                }
            } else {
                for (int i = 0; i < SITUATION_MAX_TONES; ++i) {
                    if (pGs->tone_pool[i].active && pGs->tone_pool[i].id == cmd.tone_id) {
                        pGs->tone_pool[i].state = SIT_ENV_RELEASE;
                        pGs->tone_pool[i].cursor_frames = 0;
                        break;
                    }
                }
            }
        }

        tail = (tail + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;
    }
    atomic_store_explicit(&pGs->audio_command_tail, tail, memory_order_release);
"""

# Replace in callback ONCE and only ONCE!
# Before I replaced two places (one for graph, one for normal). But they are inside the SAME function!
# We should just insert it at the very top of `sit_miniaudio_data_callback` right after `float* pOut = (float*)pOutput;`
data = re.sub(r'static void sit_miniaudio_data_callback\(ma_device\* pDevice, void\* pOutput, const void\* pInput, uint32_t frameCount\) \{.*?float\* pOut = \(float\*\)pOutput;\n', r'static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {\n    _SituationAudioState* pGs = (_SituationAudioState*)pDevice->pUserData;\n    if (!pGs) return;\n\n    float* pOut = (float*)pOutput;\n' + process_cmds, data, count=1, flags=re.DOTALL)

# Strip all remaining mtx_lock audio_queue_mutex
data = re.sub(r'mtx_lock\(&sit_audio\.audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_unlock\(&sit_audio\.audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_lock\(&pGs->audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_unlock\(&pGs->audio_queue_mutex\);', '', data)
data = re.sub(r'    // Note: audio_queue_mutex is ALREADY LOCKED here.\n    // We snapshot the active voices to a local buffer to minimize lock duration.', '', data)

route_replace = """
SITAPI SituationError SituationRouteSoundToTrack(SituationSoundHandle sound, SituationAudioTrack* track) {
    if (!SituationIsInitialized() || !track || !track->is_active) return SITUATION_ERROR_INVALID_PARAM;
    _SituationSoundSlot* slot = _SitGetSoundSlot(sound);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    _SituationSound* data = &slot->sound_data;

    SituationAudioMixer* mixer = sit_audio.active_mixer;
    if (!mixer) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    mtx_lock(&mixer->topology_mutex);

    if (!data->is_graph_managed) {
        ma_data_source_node_config cfg = ma_data_source_node_config_init(&data->decoder);
        if (ma_data_source_node_init(&mixer->graph, &cfg, NULL, &data->graph_node) != MA_SUCCESS) {
            mtx_unlock(&mixer->topology_mutex);
            return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
        }
        data->is_graph_managed = true;
    }

    ma_node_attach_output_bus(&data->graph_node, 0, &track->input_node, 0);

    mtx_unlock(&mixer->topology_mutex);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'SITAPI SituationError SituationRouteSoundToTrack\(SituationSoundHandle sound, SituationAudioTrack\* track\) \{.*?return SITUATION_SUCCESS;\n\}\n\n\nSITAPI void SituationSetTrackEQ', route_replace + '\nSITAPI void SituationSetTrackEQ', data, flags=re.DOTALL)

with open("sit/situation_impl_audio.h", "w") as f:
    f.write(data)

# 3. sit/aud/tone_synth.h
with open("sit/aud/tone_synth.h", "r") as f:
    tonedata = f.read()

tone_push_cmd = """
static void _SitPushTonePlayCommand(SituationWaveType type, float frequency, float volume, float pan, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec, uint32_t id) {
    size_t head = atomic_load_explicit(&sit_audio.audio_command_head, memory_order_relaxed);
    size_t next_head = (head + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;

    sit_audio.audio_command_queue[head].type = SIT_AUDIO_CMD_PLAY_TONE;
    sit_audio.audio_command_queue[head].tone_type = type;
    sit_audio.audio_command_queue[head].frequency = frequency;
    sit_audio.audio_command_queue[head].value = volume; // reuse value for volume
    sit_audio.audio_command_queue[head].pan = pan;
    sit_audio.audio_command_queue[head].attack_sec = attack_sec;
    sit_audio.audio_command_queue[head].decay_sec = decay_sec;
    sit_audio.audio_command_queue[head].sustain_level = sustain_level;
    sit_audio.audio_command_queue[head].release_sec = release_sec;
    sit_audio.audio_command_queue[head].hold_sec = hold_sec;
    sit_audio.audio_command_queue[head].tone_id = id;

    atomic_store_explicit(&sit_audio.audio_command_head, next_head, memory_order_release);
}

static void _SitPushToneStopCommand(uint32_t tone_id) {
    size_t head = atomic_load_explicit(&sit_audio.audio_command_head, memory_order_relaxed);
    size_t next_head = (head + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;

    sit_audio.audio_command_queue[head].type = SIT_AUDIO_CMD_STOP_TONE;
    sit_audio.audio_command_queue[head].tone_id = tone_id;

    atomic_store_explicit(&sit_audio.audio_command_head, next_head, memory_order_release);
}
"""

tonedata = re.sub(r'SITAPI uint32_t SituationPlayToneEx', tone_push_cmd + '\nSITAPI uint32_t SituationPlayToneEx', tonedata)

play_tone_replace = """
SITAPI SituationToneHandle SituationPlayToneEx(SituationWaveType type, float frequency, float volume, float pan, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec) {
    if (!SituationIsInitialized()) return 0;

    static atomic_uint global_tone_id_gen = 0;
    uint32_t new_id = atomic_fetch_add(&global_tone_id_gen, 1) + 1;

    _SitPushTonePlayCommand(type, frequency, volume, pan, attack_sec, decay_sec, sustain_level, release_sec, hold_sec, new_id);

    return new_id;
}
"""

tonedata = re.sub(r'SITAPI SituationToneHandle SituationPlayToneEx\(SituationWaveType type, float frequency, float volume, float pan, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec\) \{.*?(?=SITAPI void SituationPlayTone)', play_tone_replace + '\n', tonedata, flags=re.DOTALL)

new_stop_tone = """
SITAPI void SituationStopTone(SituationToneHandle handle) {
    if (!SituationIsInitialized() || handle == 0) return;
    _SitPushToneStopCommand(handle);
}
"""
tonedata = re.sub(r'SITAPI void SituationStopTone\(SituationToneHandle handle\) \{.*?\n\}', new_stop_tone, tonedata, flags=re.DOTALL)

data_stop_all = """SITAPI void SituationStopAllTones(void) {
    if (!SituationIsInitialized()) return;
    _SitPushToneStopCommand(0); // 0 means all
}"""
tonedata = re.sub(r'SITAPI void SituationStopAllTones\(void\) \{.*?\n\}', data_stop_all, tonedata, flags=re.DOTALL)

tonedata = re.sub(r'mtx_lock\(&sit_audio\.audio_queue_mutex\);', '', tonedata)
tonedata = re.sub(r'mtx_unlock\(&sit_audio\.audio_queue_mutex\);', '', tonedata)

with open("sit/aud/tone_synth.h", "w") as f:
    f.write(tonedata)
