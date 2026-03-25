/***************************************************************************************************
*
*   sit/aud/mic_capture.h - Microphone Capture (Audio Input)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
***************************************************************************************************/

#ifndef SITUATION_MIC_CAPTURE_H
#define SITUATION_MIC_CAPTURE_H

#include <string.h>
#include <stdbool.h>

typedef struct {
    float sample_rate;
    int channels;
    bool is_capturing;
    float gain;
    
    // Ring buffer for captured audio (stub - would use actual audio input in real implementation)
    float* ring_buffer;
    int ring_buffer_size;
    int write_position;
    int read_position;
} SituationMicCapture;

// Initialize mic capture
static void mic_capture_init(SituationMicCapture* mic, float sample_rate) {
    mic->sample_rate = sample_rate;
    mic->channels = 2;
    mic->is_capturing = false;
    mic->gain = 1.0f;
    
    // Allocate ring buffer (1 second)
    mic->ring_buffer_size = (int)sample_rate * mic->channels;
    mic->ring_buffer = (float*)calloc(mic->ring_buffer_size, sizeof(float));
    mic->write_position = 0;
    mic->read_position = 0;
}

// Start/stop capture
static void mic_capture_start(SituationMicCapture* mic) {
    mic->is_capturing = true;
}

static void mic_capture_stop(SituationMicCapture* mic) {
    mic->is_capturing = false;
}

// Set parameters
static void mic_capture_set_gain(SituationMicCapture* mic, float gain) {
    mic->gain = gain;
}

// Process audio (read from ring buffer)
static void mic_capture_process(SituationMicCapture* mic, float* output, int frames) {
    if (!mic->is_capturing || !mic->ring_buffer) {
        // Output silence
        memset(output, 0, frames * mic->channels * sizeof(float));
        return;
    }
    
    for (int i = 0; i < frames * mic->channels; i++) {
        output[i] = mic->ring_buffer[mic->read_position] * mic->gain;
        
        mic->read_position++;
        if (mic->read_position >= mic->ring_buffer_size) {
            mic->read_position = 0;
        }
    }
}

// Simulate input (for testing - would be replaced with actual audio input)
static void mic_capture_simulate_input(SituationMicCapture* mic, const float* input, int frames) {
    if (!mic->ring_buffer) return;
    
    for (int i = 0; i < frames * mic->channels; i++) {
        mic->ring_buffer[mic->write_position] = input[i];
        
        mic->write_position++;
        if (mic->write_position >= mic->ring_buffer_size) {
            mic->write_position = 0;
        }
    }
}

// Cleanup
static void mic_capture_cleanup(SituationMicCapture* mic) {
    if (mic->ring_buffer) {
        free(mic->ring_buffer);
        mic->ring_buffer = NULL;
    }
}

#endif // SITUATION_MIC_CAPTURE_H
