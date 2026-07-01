#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

int main() {
    printf("Testing Global Voice Pan...\n");

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

    // 1. Check default
    float pan = PX_GetGlobalVoicePan(synth);
    printf("Default Pan: %f\n", pan);
    assert(fabsf(pan - 0.0f) < 0.0001f);

    // 2. Set Pan to 0.5
    PX_SetGlobalVoicePan(synth, 0.5f);

    // Check immediately (should likely be unchanged if queue not processed, but implementation detail)
    // Actually, PX_SetGlobalVoicePan pushes a command. The state isn't updated until process.
    pan = PX_GetGlobalVoicePan(synth);
    printf("Pan after set (before process): %f\n", pan);
    // Depending on if we want to enforce this "lag", we can assert it's still 0.0f
    assert(fabsf(pan - 0.0f) < 0.0001f);

    // Process to apply command
    float buffer[256];
    PX_Process(synth, buffer, 128); // Process a block

    pan = PX_GetGlobalVoicePan(synth);
    printf("Pan after process: %f\n", pan);
    assert(fabsf(pan - 0.5f) < 0.0001f);

    // 3. Test Clamping Low (-1.5 -> -1.0)
    PX_SetGlobalVoicePan(synth, -1.5f);
    PX_Process(synth, buffer, 128);
    pan = PX_GetGlobalVoicePan(synth);
    printf("Pan clamped low: %f\n", pan);
    assert(fabsf(pan - -1.0f) < 0.0001f);

    // 4. Test Clamping High (1.5 -> 1.0)
    PX_SetGlobalVoicePan(synth, 1.5f);
    PX_Process(synth, buffer, 128);
    pan = PX_GetGlobalVoicePan(synth);
    printf("Pan clamped high: %f\n", pan);
    assert(fabsf(pan - 1.0f) < 0.0001f);

    PX_Destroy(synth);
    printf("Global Voice Pan Test Passed!\n");
    return 0;
}
