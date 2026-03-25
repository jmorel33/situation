/***************************************************************************************************
*
*   sit/aud/sound_source.h - Simple Sound Source (Audio Playback)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
***************************************************************************************************/

#ifndef SITUATION_SOUND_SOURCE_H
#define SITUATION_SOUND_SOURCE_H

#include <string.h>
#include <stdbool.h>

#define MAX_SOUND_BUFFER_SIZE 48000 * 10  // 10 seconds at 48kHz

typedef struct {
    float* buffer;
    int buffer_size;
    int channels;
    int sample_rate;
    int playback_position;
    bool is_playing;
    bool loop;
    float volume;
} SituationSoundSource;

// Initialize sound source
static void sound_source_init(SituationSoundSource* src, float sample_rate) {
    src->buffer = NULL;
    src->buffer_size = 0;
    src->channels = 2;
    src->sample_rate = (int)sample_rate;
    src->playback_position = 0;
    src->is_playing = false;
    src->loop = false;
    src->volume = 1.0f;
}

// Load audio data (stub - would load from file in real implementation)
static void sound_source_load_buffer(SituationSoundSource* src, const float* data, int frames, int channels) {
    if (src->buffer) {
        free(src->buffer);
    }
    
    src->channels = channels;
    src->buffer_size = frames;
    src->buffer = (float*)malloc(frames * channels * sizeof(float));
    
    if (src->buffer) {
        memcpy(src->buffer, data, frames * channels * sizeof(float));
    }
}

// Playback control
static void sound_source_play(SituationSoundSource* src) {
    src->is_playing = true;
}

static void sound_source_stop(SituationSoundSource* src) {
    src->is_playing = false;
    src->playback_position = 0;
}

static void sound_source_pause(SituationSoundSource* src) {
    src->is_playing = false;
}

static void sound_source_set_loop(SituationSoundSource* src, bool loop) {
    src->loop = loop;
}

static void sound_source_set_volume(SituationSoundSource* src, float volume) {
    src->volume = volume;
}

// Process audio
static void sound_source_process(SituationSoundSource* src, float* output, int frames, int output_channels) {
    if (!src->is_playing || !src->buffer) {
        // Output silence
        memset(output, 0, frames * output_channels * sizeof(float));
        return;
    }
    
    for (int i = 0; i < frames; i++) {
        if (src->playback_position >= src->buffer_size) {
            if (src->loop) {
                src->playback_position = 0;
            } else {
                src->is_playing = false;
                // Fill rest with silence
                memset(&output[i * output_channels], 0, (frames - i) * output_channels * sizeof(float));
                break;
            }
        }
        
        // Read from buffer
        if (src->channels == 1 && output_channels == 2) {
            // Mono to stereo
            float sample = src->buffer[src->playback_position] * src->volume;
            output[i * 2] = sample;
            output[i * 2 + 1] = sample;
        } else if (src->channels == 2 && output_channels == 2) {
            // Stereo to stereo
            output[i * 2] = src->buffer[src->playback_position * 2] * src->volume;
            output[i * 2 + 1] = src->buffer[src->playback_position * 2 + 1] * src->volume;
        } else if (src->channels == 2 && output_channels == 1) {
            // Stereo to mono
            float l = src->buffer[src->playback_position * 2];
            float r = src->buffer[src->playback_position * 2 + 1];
            output[i] = (l + r) * 0.5f * src->volume;
        } else {
            // Mono to mono
            output[i] = src->buffer[src->playback_position] * src->volume;
        }
        
        src->playback_position++;
    }
}

// Cleanup
static void sound_source_cleanup(SituationSoundSource* src) {
    if (src->buffer) {
        free(src->buffer);
        src->buffer = NULL;
    }
}

#endif // SITUATION_SOUND_SOURCE_H
