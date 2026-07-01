#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

int main() {
    printf("Testing polysonix.h compilation and basic initialization...\n");

    PxConfig config = {
        .num_voices = 4,
        .num_lfos = 2,
        .num_voice_adsrs = 2,
        .sample_rate = 44100.0f,
        .osc_update_mode = PX_OSC_UPDATE_MODE_FIXED_RATE,
        .osc_fixed_update_rate_hz = 44100.0f
    };

    PxSynth* synth = PX_Create(&config);
    if (!synth) {
        fprintf(stderr, "PX_Create failed.\n");
        return 1;
    }

    printf("Synth created successfully.\n");
    printf("RNG State: %u\n", synth->rng_state);

    // Verify pointers
    for(int i=0; i<config.num_voices; ++i) {
        if (synth->voices[i].osc[0].vm_params.rng_state_ptr != &synth->voices[i].rng_state) {
             fprintf(stderr, "Voice %d osc 0 rng_state_ptr mismatch.\n", i);
             return 1;
        }
    }
    printf("Voice pointers verified.\n");

    // Trigger a note (to check PX_NoteOn_internal compilation and execution)
    PX_NoteOn(synth, 60, 0, 1, 1.0f);

    // Process some audio (to check execution path)
    float buffer[256];
    PX_Process(synth, buffer, 128);

    printf("Audio processed.\n");

    PX_Destroy(synth);
    printf("Test Passed!\n");
    return 0;
}
