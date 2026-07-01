#define POLYSONIX_IMPLEMENTATION
#include "polysonix.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

int main() {
    printf("Testing Phase Wrapping Logic...\n");

    PxConfig config = {
        .num_voices = 1,
        .num_lfos = 0,
        .num_voice_adsrs = 1,
        .sample_rate = 48000.0f,
        .samples_per_lfo_update = 32,
        .lfo_update_interval_ms = 1.0f,
        .osc_update_mode = PX_OSC_UPDATE_MODE_PER_SAMPLE,
        .osc_fixed_update_rate_hz = 35000.0f
    };

    PxSynth* s = PX_Create(&config);
    if (!s) return 1;

    PX_NoteOn(s, 60, 0, 1, 1.0f);
    float buf[2];
    PX_Process(s, buf, 1);
    Voice* v = &s->voices[0];

    // --- Test Case 1: Large Positive Increment (Forward) ---
    v->frequency = 2.3f * 48000.0f;
    v->original_frequency = v->frequency; // Critical fix
    v->osc_phase[0] = 0.0f;

    PX_Process(s, buf, 1);
    float p1 = v->osc_phase[0];
    printf("Forward (2.3): %f\n", p1);
    if (fabsf(p1 - 0.3f) > 1e-5f) { fprintf(stderr, "FAIL 1\n"); return 1; }

    // --- Test Case 2: Extreme Positive Increment ---
    v->frequency = 105.7f * 48000.0f;
    v->original_frequency = v->frequency; // Critical fix
    v->osc_phase[0] = 0.0f;
    PX_Process(s, buf, 1);
    float p2 = v->osc_phase[0];
    printf("Forward (105.7): %f\n", p2);
    if (fabsf(p2 - 0.7f) > 1e-4f) { fprintf(stderr, "FAIL 2\n"); return 1; }

    // --- Test Case 3: Reverse Play (Explicit Flag) ---
    PxWaveSequence mock_seq = {0};
    mock_seq.steps[0].flags = PX_WSEQ_REVERSE_PLAY;
    v->seq_states[0].current_sequence = &mock_seq;
    v->seq_states[0].step_flags = PX_WSEQ_REVERSE_PLAY;
    v->seq_states[0].finished = false;

    v->frequency = 1.4f * 48000.0f;
    v->original_frequency = v->frequency;
    v->osc_phase[0] = 0.5f;

    PX_Process(s, buf, 1);
    float p3 = v->osc_phase[0];
    printf("Reverse (1.4, start 0.5): %f\n", p3);
    // 0.5 - 1.4 = -0.9 -> 0.1
    if (fabsf(p3 - 0.1f) > 1e-5f) { fprintf(stderr, "FAIL 3\n"); return 1; }

    // --- Test Case 4: Negative Frequency in Forward Mode (Simulating extreme FM) ---
    // Disable Reverse Flag
    v->seq_states[0].current_sequence = NULL;

    // Set Negative Frequency manually
    v->frequency = -1.4f * 48000.0f;
    v->original_frequency = v->frequency;
    v->osc_phase[0] = 0.5f;

    // In current implementation, this should FAIL to wrap (becomes -0.9)
    // In fixed implementation, this should WRAP to 0.1

    PX_Process(s, buf, 1);
    float p4 = v->osc_phase[0];
    printf("Forward Negative (-1.4, start 0.5): %f\n", p4);

    if (p4 < 0.0f) {
        printf("NOTE: Forward Negative failed to wrap (Current Behavior Confirmed)\n");
    } else {
        printf("NOTE: Forward Negative wrapped correctly.\n");
    }

    PX_Destroy(s);
    return 0;
}
