// Test ma_waveform over long duration
#define MINIAUDIO_IMPLEMENTATION
#include "ext/miniaudio.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("Testing ma_waveform over 20 seconds...\n\n");
    
    ma_waveform_config config = ma_waveform_config_init(
        ma_format_f32,
        1,      // mono
        48000,  // sample rate
        ma_waveform_type_sine,
        440.0,  // frequency
        1.0     // amplitude
    );
    
    ma_waveform waveform;
    ma_waveform_init(&config, &waveform);
    
    // Simulate 20 seconds of audio (20 * 48000 = 960000 samples)
    // Read in chunks of 480 samples (10ms at 48kHz)
    int total_samples = 20 * 48000;
    int chunk_size = 480;
    int num_chunks = total_samples / chunk_size;
    
    printf("Reading %d samples in %d chunks of %d samples each\n", 
           total_samples, num_chunks, chunk_size);
    printf("This simulates 20 seconds of audio at 48kHz\n\n");
    
    float samples[480];
    int discontinuities = 0;
    float prev_sample = 0.0f;
    
    for (int chunk = 0; chunk < num_chunks; chunk++) {
        ma_waveform_read_pcm_frames(&waveform, samples, chunk_size, NULL);
        
        // Check for discontinuities (sudden jumps in value)
        for (int i = 0; i < chunk_size; i++) {
            float diff = fabsf(samples[i] - prev_sample);
            // For a 440Hz sine wave at 48kHz, max change between samples is ~0.058
            if (diff > 0.1f && chunk > 0) {  // Allow first chunk to settle
                discontinuities++;
                if (discontinuities < 10) {  // Only print first few
                    printf("Discontinuity at chunk %d, sample %d: %.3f -> %.3f (diff: %.3f)\n",
                           chunk, i, prev_sample, samples[i], diff);
                }
            }
            prev_sample = samples[i];
        }
        
        // Print progress every second
        if (chunk % 100 == 0) {
            printf("Progress: %.1f seconds, sample value: %.3f\n", 
                   (float)chunk * chunk_size / 48000.0f, samples[0]);
        }
    }
    
    printf("\nResults:\n");
    printf("  Total discontinuities: %d\n", discontinuities);
    
    if (discontinuities > 0) {
        printf("\n❌ PROBLEM: Waveform has discontinuities over long duration!\n");
    } else {
        printf("\n✅ Waveform is smooth over 20 seconds.\n");
    }
    
    ma_waveform_uninit(&waveform);
    return 0;
}
