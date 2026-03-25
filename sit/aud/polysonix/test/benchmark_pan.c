#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define POLYSONIX_IMPLEMENTATION
#include "polysonix.h"

int main() {
    printf("Benchmarking Polysonix Pan Performance...\n");

    // Configure for high load
    PxConfig config = {
        .num_voices = 16,
        .num_lfos = 3,
        .num_voice_adsrs = 2,
        .sample_rate = 44100.0f,
        .osc_update_mode = PX_OSC_UPDATE_MODE_FIXED_RATE,
        .osc_fixed_update_rate_hz = 35000.0f
    };

    PxSynth* synth = PX_Create(&config);
    if (!synth) {
        fprintf(stderr, "PX_Create failed.\n");
        return 1;
    }

    // Set up LFO routing to Pan to ensure pan calculations are varying
    // Route LFO 0 to Pan
    // LFO 0 is enabled by default in PX_Create
    // Reduce depth to 0 for half of the test to measure static improvement
    PX_SetLFOModAmount(synth, 0, PX_LFO_DEST_PAN, 0.8f);
    PX_SetLFOParam(synth, 0, PX_LFO_PARAM_FREQUENCY, 5.0f); // 5Hz LFO

    // Trigger all voices
    for (int i = 0; i < 16; i++) {
        PX_NoteOn(synth, 60 + i, 0, i, 1.0f);
    }

    // Buffer size
    int block_size = 256;
    float* buffer = (float*)calloc(block_size * 2, sizeof(float));

    // Warm up
    for(int i=0; i<10; i++) {
        PX_Process(synth, buffer, block_size);
    }

    // Measure
    clock_t start = clock();
    int iterations = 2000;
    for(int i=0; i<iterations; i++) {
        PX_Process(synth, buffer, block_size);
    }
    clock_t end = clock();

    double elapsed_seconds = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Time taken for %d iterations of %d frames: %.4f seconds\n", iterations, block_size, elapsed_seconds);

    // Calculate samples per second
    double total_samples = (double)iterations * block_size * 2; // Stereo
    printf("Performance: %.2f M samples/sec\n", (total_samples / elapsed_seconds) / 1000000.0);

    PX_Destroy(synth);
    free(buffer);
    return 0;
}
