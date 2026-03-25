#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"
#include <stdio.h>
#include <time.h>

#define NUM_FRAMES 256
#define SAMPLE_Rate 44100.0f
#define DURATION_SEC 20.0f

int main() {
    PxConfig config = {
        .num_voices = 16,
        .num_lfos = 3,
        .num_voice_adsrs = 3,
        .sample_rate = SAMPLE_Rate,
        .samples_per_lfo_update = 32,
        .lfo_update_interval_ms = 1.0f,
        .osc_update_mode = PX_OSC_UPDATE_MODE_FIXED_RATE,
        .osc_fixed_update_rate_hz = 35000.0f,
        .use_gpu = false
    };

    PxSynth* synth = PX_Create(&config);
    if (!synth) {
        fprintf(stderr, "Failed to create synth\n");
        return 1;
    }

    // Trigger 16 notes to maximize CPU usage
    for (int i=0; i<16; i++) {
        PX_NoteOn(synth, 40 + i*2, 0, i, 1.0f);
    }

    float buffer[NUM_FRAMES * 2];
    long total_frames = (long)(SAMPLE_Rate * DURATION_SEC);
    int num_blocks = total_frames / NUM_FRAMES;

    clock_t start = clock();

    for (int i=0; i<num_blocks; i++) {
        PX_Process(synth, buffer, NUM_FRAMES);
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Processed %.2f seconds of audio in %.4f seconds (%.2fx realtime)\n",
           DURATION_SEC, time_taken, DURATION_SEC / time_taken);

    PX_Destroy(synth);
    return 0;
}
