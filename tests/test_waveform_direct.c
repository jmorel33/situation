// Direct test of miniaudio waveform generator
// This will generate a 880Hz square wave and print the first 100 samples

#define MINIAUDIO_IMPLEMENTATION
#include "ext/miniaudio.h"
#include <stdio.h>

int main() {
    printf("Testing miniaudio waveform generator directly...\n\n");
    
    // Configure waveform exactly as Situation does
    ma_waveform_config config = ma_waveform_config_init(
        ma_format_f32,           // float32
        1,                       // MONO
        48000,                   // 48kHz sample rate
        ma_waveform_type_square, // Square wave
        880.0,                   // 880Hz (A5)
        1.0                      // Full amplitude
    );
    
    ma_waveform waveform;
    ma_result result = ma_waveform_init(&config, &waveform);
    
    if (result != MA_SUCCESS) {
        printf("ERROR: Failed to initialize waveform! Error code: %d\n", result);
        return 1;
    }
    
    printf("Waveform initialized successfully!\n");
    printf("Type: Square\n");
    printf("Frequency: 880Hz\n");
    printf("Sample Rate: 48000Hz\n");
    printf("Format: float32\n");
    printf("Channels: 1 (mono)\n\n");
    
    // Read samples in batches like we do in the audio callback
    printf("Reading 480 samples (10ms at 48kHz)...\n\n");
    
    float samples[480];
    ma_uint64 frames_read = 0;
    result = ma_waveform_read_pcm_frames(&waveform, samples, 480, &frames_read);
    
    if (result != MA_SUCCESS) {
        printf("ERROR: Failed to read samples! Error code: %d\n", result);
        return 1;
    }
    
    printf("Successfully read %llu frames\n\n", frames_read);
    
    // Print first 100 samples
    printf("First 100 samples:\n");
    for (int i = 0; i < 100 && i < frames_read; i++) {
        printf("[%3d] %.6f", i, samples[i]);
        
        // Show visual representation
        if (samples[i] > 0.5f) printf("  ████████████████\n");
        else if (samples[i] > 0.0f) printf("  ████\n");
        else if (samples[i] > -0.5f) printf("  ▄▄▄▄\n");
        else printf("  ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄\n");
    }
    
    // Check if we got any non-zero samples
    int non_zero_count = 0;
    float min_val = samples[0];
    float max_val = samples[0];
    
    for (int i = 0; i < frames_read; i++) {
        if (samples[i] != 0.0f) non_zero_count++;
        if (samples[i] < min_val) min_val = samples[i];
        if (samples[i] > max_val) max_val = samples[i];
    }
    
    printf("\n=== STATISTICS ===\n");
    printf("Non-zero samples: %d / %llu (%.1f%%)\n", non_zero_count, frames_read, 
           (non_zero_count * 100.0f) / frames_read);
    printf("Min value: %.6f\n", min_val);
    printf("Max value: %.6f\n", max_val);
    printf("Range: %.6f\n", max_val - min_val);
    
    if (non_zero_count == 0) {
        printf("\n❌ ERROR: All samples are ZERO! Waveform is not generating audio!\n");
    } else if (max_val - min_val < 0.1f) {
        printf("\n⚠️  WARNING: Very low amplitude! Waveform might be too quiet.\n");
    } else {
        printf("\n✅ Waveform is generating samples correctly!\n");
    }
    
    ma_waveform_uninit(&waveform);
    
    return 0;
}
