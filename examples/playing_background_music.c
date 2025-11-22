/***************************************************************************************************
*   Situation Library - Example: Background Music & DSP
*   ---------------------------------------------------
*   This example demonstrates the Audio Subsystem.
*   
*   Key Concepts:
*   1. Streaming vs Preloading: Using SITUATION_AUDIO_LOAD_STREAM for long music files.
*   2. Playback Control: Play, Stop, Volume, Pitch.
*   3. Real-Time DSP: Applying Reverb to the music.
*
*   Prerequisites:
*   - "assets/audio/music.mp3" (or .wav/.ogg)
*
*   Controls:
*   - UP/DOWN:   Volume
*   - LEFT/RIGHT: Pitch (Speed)
*   - SPACE:     Toggle Reverb
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"

// --- Configuration ---
#define MUSIC_PATH "assets/audio/music.mp3"

// --- Global State ---
static SituationSound g_music = {0};
static bool g_reverb_enabled = false;

// --- Initialization ---
int init_audio() {
    // Load Sound
    // Mode: SITUATION_AUDIO_LOAD_STREAM
    //       This tells the engine NOT to decode the whole file to RAM.
    //       Instead, it reads small chunks from disk as needed.
    //       This is vital for long music tracks (saves ~50MB RAM per track).
    // Looping: true
    SituationError err = SituationLoadSoundFromFile(
        MUSIC_PATH, 
        SITUATION_AUDIO_LOAD_STREAM, 
        true, // Loop
        &g_music
    );

    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to load music: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Start Playback
    SituationPlayLoadedSound(&g_music);
    
    printf("Playing: %s\n", MUSIC_PATH);
    return 0;
}

// --- Update Logic ---
void update_audio_controls() {
    // Volume Control
    float vol = SituationGetSoundVolume(&g_music);
    if (SituationIsKeyDown(SIT_KEY_UP))   vol += 0.01f;
    if (SituationIsKeyDown(SIT_KEY_DOWN)) vol -= 0.01f;
    
    // Clamp 0.0 to 1.0
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    SituationSetSoundVolume(&g_music, vol);

    // Pitch Control
    float pitch = SituationGetSoundPitch(&g_music);
    if (SituationIsKeyDown(SIT_KEY_RIGHT)) pitch += 0.01f;
    if (SituationIsKeyDown(SIT_KEY_LEFT))  pitch -= 0.01f;
    SituationSetSoundPitch(&g_music, pitch);

    // Reverb Toggle
    if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
        g_reverb_enabled = !g_reverb_enabled;
        
        // Apply Reverb Effect
        // Room Size: 0.8 (Large Hall)
        // Damping:   0.5 (Medium Wall Absorption)
        // Wet Mix:   0.5 (50% Reverb sound)
        // Dry Mix:   0.8 (80% Original sound)
        SituationSetSoundReverb(&g_music, g_reverb_enabled, 0.8f, 0.5f, 0.5f, 0.8f);
        
        printf("Reverb: %s\n", g_reverb_enabled ? "ON" : "OFF");
    }
}

// --- Cleanup ---
void cleanup_audio() {
    if (g_music.is_initialized) {
        SituationStopLoadedSound(&g_music);
        SituationUnloadSound(&g_music);
    }
}

// --- Main ---
int main(int argc, char** argv) {
    SituationInitInfo config = { 
        .window_title = "Situation - Audio Player",
        .window_width = 600, 
        .window_height = 400 
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    if (init_audio() != 0) {
        // Don't exit, just print error, so we can see the window
        printf("Audio failed to load. Ensure '%s' exists.\n", MUSIC_PATH);
    }

    printf("Controls:\n [UP/DOWN] Volume\n [L/R] Pitch\n [SPACE] Reverb\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        update_audio_controls();

        // Minimal Render to keep window alive
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {20, 20, 20, 255} } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    cleanup_audio();
    SituationShutdown();
    return 0;
}