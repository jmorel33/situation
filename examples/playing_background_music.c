/*
 * Playing Background Music
 *
 * This example demonstrates how to load and play a sound file for background music.
 * It is designed to fail gracefully if the specified audio file is not found.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>

// --- Global Sound Handle ---
// We store the handle to our sound in a global variable.
static SituationSound g_background_music = {0};

// --- NOTE ---
// This example requires an audio file (e.g., in .ogg, .wav, or .mp3 format)
// located at "assets/audio/background.ogg".
// You will need to provide this file yourself for the audio to actually play.
// The underlying audio engine is miniaudio, which supports many common formats.

// --- Initialization ---
// Loads and configures the background music.
int init_audio() {
    const char* music_path = "assets/audio/background.ogg";

    // Load the sound from the specified file path.
    // This function will allocate resources and decode the audio data.
    g_background_music = SituationLoadSoundFromFile(music_path);

    // Check if loading was successful. The handle's ID will be non-zero on success.
    if (g_background_music.id == 0) {
        fprintf(stderr, "Warning: Failed to load background music (%s): %s\n", music_path, SituationGetLastErrorMsg());
    } else {
        // If loaded successfully, we can configure and play it.
        printf("Background music loaded successfully.\n");

        // Set the sound to loop. This is common for background music.
        SituationSetSoundLooping(g_background_music, true);

        // Start playing the sound.
        SituationPlaySound(g_background_music);
    }
    return 0;
}

// --- Cleanup ---
// Stops and destroys the sound resources to prevent memory leaks.
void cleanup_audio() {
    // Only try to clean up if the sound was loaded successfully.
    if (g_background_music.id != 0) {
        // Stop playback before destroying the sound.
        SituationStopSound(g_background_music);

        // Release all resources associated with the sound.
        SituationDestroySound(&g_background_music);
    }
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Audio Test";
    init_info.window_width = 800;
    init_info.window_height = 600;
    // Initializing the library also initializes the audio engine.
    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Load and play our audio.
    init_audio();

    printf("Audio playing (if loaded). Running... Close window to stop.\n");

    // The main loop does not need any specific audio calls.
    // The sound plays in the background on a separate thread.
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        // A minimal render loop to keep the window open and responsive.
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 40, 40, 60, 255 }); // Dark purple background
            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
    }

    // Clean up audio resources before shutting down.
    cleanup_audio();
    SituationShutdown();
    return 0;
}
