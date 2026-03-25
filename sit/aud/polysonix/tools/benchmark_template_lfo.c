#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

double get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main() {
    PxConfig config = {
        .num_voices = 1,
        .num_lfos = 1,
        .num_voice_adsrs = 1,
        .sample_rate = 44100.0f,
        .samples_per_lfo_update = 1 // update every sample to maximize CPU work
    };

    PxSynth* synth = PX_Create(&config);
    if (!synth) {
        printf("Failed to create synth\n");
        return 1;
    }

    synth->patch.template_lfos[0].enabled = true;
    synth->patch.template_lfos[0].frequency = 1000.0f; // fast

    int num_samples = 44100 * 100; // 100 seconds of processing
    float* dummy_buffer = (float*)malloc(num_samples * 2 * sizeof(float));

    double start = get_time_ns();
    PX_Process(synth, dummy_buffer, num_samples);
    double end = get_time_ns();

    printf("Total time: %.2f ms\n", (end - start) / 1000000.0);

    PX_Destroy(synth);
    free(dummy_buffer);

    return 0;
}
