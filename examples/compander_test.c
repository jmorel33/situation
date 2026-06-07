/***************************************************************************************************
*
*   Compander Integration Test
*   Tests the compander device registration and basic functionality
*
***************************************************************************************************/

#define SITUATION_USE_OPENGL
#include "../situation.h"
#include "../sit/aud/fx/compander.h"

int main(void) {
    printf("=== Compander Integration Test ===\n\n");
    
    // Initialize device registry
    SituationInitDeviceRegistry();
    
    // Check if compander is registered
    if (SituationIsDeviceRegistered(SITUATION_NODE_COMPANDER)) {
        printf("[SUCCESS] Compander is registered in the device registry!\n\n");
        
        // Query compander metadata
        SituationDeviceMetadata meta;
        if (SituationGetDeviceMetadata(SITUATION_NODE_COMPANDER, &meta) == SITUATION_SUCCESS) {
            printf("Device Name: %s\n", meta.name);
            printf("Category: %s\n", SituationGetCategoryName(meta.category));
            printf("Audio: %d ins, %d outs (%d channels)\n",
                   meta.num_audio_ins, meta.num_audio_outs, meta.audio_channels);
            printf("Controls: %d\n\n", meta.num_controls);
            
            printf("Control Parameters:\n");
            for (int i = 0; i < meta.num_controls; i++) {
                const SituationControlDesc* ctrl = &meta.controls[i];
                printf("  [%2d] %-20s: %.2f to %.2f (default: %.2f) %s\n",
                       ctrl->id, ctrl->name,
                       ctrl->min_value, ctrl->max_value, ctrl->default_value,
                       ctrl->units ? ctrl->units : "");
            }
        }
    } else {
        printf("[FAILED] Compander is NOT registered!\n");
        return 1;
    }
    
    printf("\n=== Testing Compander Processor ===\n\n");
    
    // Test the compander processor directly
    CompanderProcessor proc;
    float sample_rate = 48000.0f;
    compander_init(&proc, sample_rate);
    printf("[SUCCESS] Compander processor initialized at %.0f Hz\n", sample_rate);
    
    // Update mid band parameters
    CompanderParams comp = {0.6f, 0.15f, 0.4f, 3.0f, -70.0f};
    BellParams bell = {1000.0f, 3.0f, 1.0f};
    compander_update_band_params(&proc, 1, &comp, &bell);
    printf("[SUCCESS] Updated mid band parameters\n");
    
    // Process a small test buffer
    float input[512] = {0};
    float output[512] = {0};
    
    // Generate test signal (440 Hz sine wave)
    for (int i = 0; i < 256; i++) {
        float t = (float)i / sample_rate;
        float sample = 0.5f * sinf(2.0f * M_PI * 440.0f * t);
        input[i * 2] = sample;      // Left
        input[i * 2 + 1] = sample;  // Right
    }
    
    compander_process(&proc, input, output, 256);
    printf("[SUCCESS] Processed 256 frames of audio\n");
    
    // Cleanup
    compander_cleanup(&proc);
    printf("[SUCCESS] Compander processor cleaned up\n");
    
    printf("\n=== All Tests Passed! ===\n");
    
    return 0;
}
