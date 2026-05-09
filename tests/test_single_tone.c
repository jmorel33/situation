// Absolute minimal test - ONE tone, nothing else
#ifndef SITUATION_USE_SHARED
    #define SITUATION_IMPLEMENTATION
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER
#include "situation.h"

#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  MINIMAL TONE TEST\n");
    printf("========================================\n\n");

    SituationInitInfo init_info = {
        .window_width = 640,
        .window_height = 480,
        .window_title = "Tone Test",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
        .enable_vulkan_validation = false
    };

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        printf("Failed to initialize!\n");
        return -1;
    }

    // Set audio device
    SituationAudioFormat audio_fmt = {
        .channels = 2,
        .sample_rate = 48000,
        .bit_depth = 32
    };
    
    // Find and use Anker speakers
    int device_count = 0;
    SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
    int selected_device = 0;
    if (devices && device_count > 0) {
        for (int i = 0; i < device_count; i++) {
            if (strstr(devices[i].name, "Anker") != NULL || 
                strstr(devices[i].name, "Speakers") != NULL) {
                selected_device = i;
                break;
            }
        }
        SIT_FREE(devices);
    }
    
    SituationSetAudioDevice(selected_device, &audio_fmt);
    printf("Audio device set\n");
    printf("Press SPACE to play tone, ESC to exit\n\n");

    bool tone_playing = false;
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();

        // SPACE: Play tone (same parameters as working SPACE key)
        if (SituationIsKeyPressed(SIT_KEY_SPACE) && !tone_playing) {
            printf("Playing 880Hz square wave...\n");
            SituationPlayTone(SIT_WAVE_SQUARE, 880.0f, 1.0f, 
                0.01f,  // attack
                0.05f,  // decay
                0.8f,   // sustain
                0.2f,   // release
                0.3f);  // hold
            tone_playing = true;
        }

        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }

        // Minimal rendering - just clear screen
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {0, 0, 0, 255} }
                }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
