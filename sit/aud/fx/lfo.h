/***************************************************************************************************
*
*   sit/aud/lfo.h - Low Frequency Oscillator
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
***************************************************************************************************/

#ifndef SITUATION_LFO_H
#define SITUATION_LFO_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    SITUATION_LFO_SINE,
    SITUATION_LFO_TRIANGLE,
    SITUATION_LFO_SQUARE,
    SITUATION_LFO_SAW_UP,
    SITUATION_LFO_SAW_DOWN,
    SITUATION_LFO_RANDOM
} LFOWaveform;

typedef struct {
    LFOWaveform waveform;
    float sample_rate;
    float frequency;
    float phase;
    float phase_increment;
    float last_random;
} SituationLFO;

// Initialize LFO
static void lfo_init(SituationLFO* lfo, float sample_rate) {
    lfo->sample_rate = sample_rate;
    lfo->waveform = SITUATION_LFO_SINE;
    lfo->frequency = 1.0f;
    lfo->phase = 0.0f;
    lfo->phase_increment = lfo->frequency / sample_rate;
    lfo->last_random = 0.0f;
}

// Set parameters
static void lfo_set_frequency(SituationLFO* lfo, float freq) {
    lfo->frequency = freq;
    lfo->phase_increment = freq / lfo->sample_rate;
}

static void lfo_set_waveform(SituationLFO* lfo, LFOWaveform waveform) {
    lfo->waveform = waveform;
}

static void lfo_reset_phase(SituationLFO* lfo) {
    lfo->phase = 0.0f;
}

// Generate one sample
static float lfo_process_sample(SituationLFO* lfo) {
    float output = 0.0f;
    
    switch (lfo->waveform) {
        case SITUATION_LFO_SINE:
            output = sinf(lfo->phase * 2.0f * M_PI);
            break;
            
        case SITUATION_LFO_TRIANGLE:
            if (lfo->phase < 0.5f) {
                output = 4.0f * lfo->phase - 1.0f;
            } else {
                output = -4.0f * lfo->phase + 3.0f;
            }
            break;
            
        case SITUATION_LFO_SQUARE:
            output = (lfo->phase < 0.5f) ? 1.0f : -1.0f;
            break;
            
        case SITUATION_LFO_SAW_UP:
            output = 2.0f * lfo->phase - 1.0f;
            break;
            
        case SITUATION_LFO_SAW_DOWN:
            output = 1.0f - 2.0f * lfo->phase;
            break;
            
        case SITUATION_LFO_RANDOM:
            // Sample and hold random
            if (lfo->phase < lfo->phase_increment) {
                lfo->last_random = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            }
            output = lfo->last_random;
            break;
    }
    
    // Advance phase
    lfo->phase += lfo->phase_increment;
    if (lfo->phase >= 1.0f) {
        lfo->phase -= 1.0f;
    }
    
    return output;
}

// Process buffer (generates control signal)
static void lfo_process(SituationLFO* lfo, float* output, int frames) {
    for (int i = 0; i < frames; i++) {
        output[i] = lfo_process_sample(lfo);
    }
}

#endif // SITUATION_LFO_H
