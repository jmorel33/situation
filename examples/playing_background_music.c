#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or VULKAN
#include "../situation.h"
#include <stdio.h>

// --- Global Sound Handle ---
static SituationSound g_background_music = {0};

// --- NOTE ---
// This example requires an audio file named "background.ogg" in an "assets/audio" directory.
// Since this file is not provided, the loading will fail, but the example demonstrates the API.

// --- Initialization ---
int init_audio() {
    const char* music_path = "assets/audio/background.ogg";

    // The function signature in the README was slightly different from common practice.
    // A more likely signature is shown here, loading into a handle.
    g_background_music = SituationLoadSoundFromFile(music_path);

    if (g_background_music.id == 0) {
        fprintf(stderr, "Warning: Failed to load background music (%s): %s\n", music_path, SituationGetLastErrorMsg());
    } else {
        printf("Background music loaded successfully.\n");
        SituationSetSoundLooping(g_background_music, true);
        SituationPlaySound(g_background_music);
    }
    return 0;
}

// --- Cleanup ---
void cleanup_audio() {
    if (g_background_music.id != 0) {
        SituationStopSound(g_background_music);
        SituationDestroySound(&g_background_music);
    }
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Audio Test";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    init_audio();

    printf("Audio playing (if loaded). Running... Close window to stop.\n");

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 40, 40, 60, 255 });
            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
    }

    cleanup_audio();
    SituationShutdown();
    return 0;
}
