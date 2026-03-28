import re

# 1. sit/situation_impl.h
with open("sit/situation_impl.h", "r") as f:
    data = f.read()

struct_def = """
typedef enum {
    SIT_AUDIO_CMD_PLAY,
    SIT_AUDIO_CMD_STOP,
    SIT_AUDIO_CMD_VOLUME,
    SIT_AUDIO_CMD_PAN,
    SIT_AUDIO_CMD_PITCH
} SituationAudioCommandType;

typedef struct {
    SituationAudioCommandType type;
    struct _SituationSound* sound;
    float value;
} SituationAudioCommand;

#define SIT_AUDIO_CMD_QUEUE_SIZE 256
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

data = re.sub(r'SIT_DEBUG_LOG\("\[GLExecute\] START: packet_count=%d\\n", buf->packet_count\);',
              'SIT_DEBUG_LOG("[GLExecute] START: packet_count=%zu\\n", buf->packet_count);', data)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)

# 2. sit/situation_impl_audio.h
with open("sit/situation_impl_audio.h", "r") as f:
    data = f.read()

push_cmd = """
static void _SitPushAudioCommand(SituationAudioCommandType type, _SituationSound* sound, float value) {
    size_t head = atomic_load_explicit(&sit_audio.audio_command_head, memory_order_relaxed);
    size_t next_head = (head + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;

    // We overwrite if full (could also drop, but let's assume queue is large enough)
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

    _SitPushAudioCommand(SIT_AUDIO_CMD_PLAY, data, 0.0f);

    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*// Add to active voices.*?mtx_unlock\(&sit_audio\.audio_queue_mutex\);\s*return SITUATION_SUCCESS;\s*}', play_replace, data, flags=re.DOTALL)

stop_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_STOP, data, 0.0f);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*mtx_lock\(&sit_audio\.audio_queue_mutex\);.*?mtx_unlock\(&sit_audio\.audio_queue_mutex\);\s*return SITUATION_SUCCESS;\s*}', stop_replace, data, flags=re.DOTALL)

vol_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_VOLUME, data, volume);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*data->volume = volume;\s*return SITUATION_SUCCESS;\s*}', vol_replace, data, flags=re.DOTALL)

pan_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_PAN, data, pan);
    return SITUATION_SUCCESS;
}
"""
data = re.sub(r'    _SituationSound\* data = &slot->sound_data;\s*data->pan = pan;\s*return SITUATION_SUCCESS;\s*}', pan_replace, data, flags=re.DOTALL)

pitch_replace = """
    _SituationSound* data = &slot->sound_data;
    _SitPushAudioCommand(SIT_AUDIO_CMD_PITCH, data, pitch);
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

        if (cmd.type == SIT_AUDIO_CMD_PLAY) {
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
        } else if (cmd.type == SIT_AUDIO_CMD_STOP) {
            for (int i = 0; i < pGs->active_voice_count; i++) {
                if (pGs->active_voices[i] == cmd.sound) {
                    pGs->active_voices[i] = pGs->active_voices[pGs->active_voice_count - 1];
                    pGs->active_voice_count--;
                    break;
                }
            }
        } else if (cmd.type == SIT_AUDIO_CMD_VOLUME) {
            cmd.sound->volume = cmd.value;
        } else if (cmd.type == SIT_AUDIO_CMD_PAN) {
            cmd.sound->pan = cmd.value;
        } else if (cmd.type == SIT_AUDIO_CMD_PITCH) {
            cmd.sound->pitch = cmd.value;
        }

        tail = (tail + 1) % SIT_AUDIO_CMD_QUEUE_SIZE;
    }
    atomic_store_explicit(&pGs->audio_command_tail, tail, memory_order_release);
"""

data = re.sub(r'    mtx_lock\(&pGs->audio_queue_mutex\);', process_cmds, data, count=1)
data = re.sub(r'        mtx_unlock\(&pGs->audio_queue_mutex\);', '', data)
data = re.sub(r'    // Note: audio_queue_mutex is ALREADY LOCKED here.\n    // We snapshot the active voices to a local buffer to minimize lock duration.', '', data)
data = re.sub(r'    mtx_unlock\(&pGs->audio_queue_mutex\);', '', data)

data = re.sub(r'    // Acquire lock to safely check/use active_mixer', process_cmds + '\n    // Acquire lock to safely check/use active_mixer', data)

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

data = re.sub(r'mtx_lock\(&sit_audio\.audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_unlock\(&sit_audio\.audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_lock\(&pGs->audio_queue_mutex\);', '', data)
data = re.sub(r'mtx_unlock\(&pGs->audio_queue_mutex\);', '', data)

with open("sit/situation_impl_audio.h", "w") as f:
    f.write(data)

# 3. sit/aud/tone_synth.h
with open("sit/aud/tone_synth.h", "r") as f:
    tonedata = f.read()

tonedata = re.sub(r'mtx_lock\(&sit_audio\.audio_queue_mutex\);', '', tonedata)
tonedata = re.sub(r'mtx_unlock\(&sit_audio\.audio_queue_mutex\);', '', tonedata)

with open("sit/aud/tone_synth.h", "w") as f:
    f.write(tonedata)
