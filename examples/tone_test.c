/*******************************************************************************
 * Situation Library - TONE TEST
 * ==============================
 * Tests the built-in tone synthesizer (no external audio files needed).
 *
 * Controls:
 *   1-8       - Play notes (C4 through C5)
 *   SPACE     - Play chord
 *   ESC       - Exit
 ******************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <stdio.h>
#include <math.h>

// Note frequencies (A4 = 440Hz tuning)
#define NOTE_C4  261.63f
#define NOTE_D4  293.66f
#define NOTE_E4  329.63f
#define NOTE_F4  349.23f
#define NOTE_G4  392.00f
#define NOTE_A4  440.00f
#define NOTE_B4  493.88f
#define NOTE_C5  523.25f

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Tone Test - Press 1-8 for notes, SPACE for chord",
        .window_width = 640,
        .window_height = 200,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        fprintf(stderr, "Init failed: %s\n", err_msg ? err_msg : "unknown");
        return -1;
    }

    printf("Tone Test Ready!\n");
    printf("  Keys 1-8: Play individual notes (C4 to C5)\n");
    printf("  SPACE:    Play C major chord\n");
    printf("  ESC:      Exit\n");
    fflush(stdout);

    float notes[] = { NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5 };
    int keys[] = { SIT_KEY_1, SIT_KEY_2, SIT_KEY_3, SIT_KEY_4, SIT_KEY_5, SIT_KEY_6, SIT_KEY_7, SIT_KEY_8 };

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;

        // Individual notes
        for (int i = 0; i < 8; i++) {
            if (SituationIsKeyPressed(keys[i])) {
                SituationPlayToneEx(SIT_WAVE_SINE, notes[i], 0.3f, 0.5f, 0.05f, 0.1f, 0.3f, 0.2f, 0.3f);
                printf("  Playing: %.1f Hz\n", notes[i]);
                fflush(stdout);
            }
        }

        // Chord (C major: C4 + E4 + G4)
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            SituationPlayToneEx(SIT_WAVE_SINE, NOTE_C4, 0.2f, 0.5f, 0.05f, 0.2f, 0.4f, 0.3f, 0.5f);
            SituationPlayToneEx(SIT_WAVE_SINE, NOTE_E4, 0.2f, 0.5f, 0.05f, 0.2f, 0.4f, 0.3f, 0.5f);
            SituationPlayToneEx(SIT_WAVE_SINE, NOTE_G4, 0.2f, 0.5f, 0.05f, 0.2f, 0.4f, 0.3f, 0.5f);
            printf("  Playing: C Major chord\n");
            fflush(stdout);
        }

        // Minimal render to keep window alive
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {15, 15, 30, 255} } },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
