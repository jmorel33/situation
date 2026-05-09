// Minimal test to verify ma_waveform behavior
#define MINIAUDIO_IMPLEMENTATION
#include "ext/miniaudio.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("Testing ma_waveform sample-by-sample vs batch reading...\n\n");
    
    // Create a waveform
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
    
    // Test 1: Read samples one at a time (current broken method)
    printf("Test 1: Reading 480 samples ONE AT A TIME\n");
    float samples_one_by_one[480];
    for (int i = 0; i < 480; i++) {
        ma_waveform_read_pcm_frames(&waveform, &samples_one_by_one[i], 1, NULL);
    }
    printf("  First 10 samples: ");
    for (int i = 0; i < 10; i++) {
        printf("%.3f ", samples_one_by_one[i]);
    }
    printf("\n");
    
    // Reset waveform
    ma_waveform_uninit(&waveform);
    ma_waveform_init(&config, &waveform);
    
    // Test 2: Read samples in batch (correct method)
    printf("\nTest 2: Reading 480 samples IN BATCH\n");
    float samples_batch[480];
    ma_waveform_read_pcm_frames(&waveform, samples_batch, 480, NULL);
    printf("  First 10 samples: ");
    for (int i = 0; i < 10; i++) {
        printf("%.3f ", samples_batch[i]);
    }
    printf("\n");
    
    // Compare
    printf("\nComparison:\n");
    int differences = 0;
    for (int i = 0; i < 480; i++) {
        if (fabsf(samples_one_by_one[i] - samples_batch[i]) > 0.001f) {
            differences++;
        }
    }
    printf("  Differences found: %d out of 480 samples\n", differences);
    
    if (differences > 0) {
        printf("\n❌ PROBLEM CONFIRMED: One-at-a-time reading produces different results!\n");
    } else {
        printf("\n✅ Both methods produce identical results.\n");
    }
    
    ma_waveform_uninit(&waveform);
    return 0;
}
