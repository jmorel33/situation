/***************************************************************************************************
 *   RGL smoke test — verifies Situation-backed batch draw (Phase 1/8).
 *
 *   Expected: dark gray window with a red rectangle centered on screen.
 ***************************************************************************************************/

#define SITUATION_USE_OPENGL
#include "situation.h"

#define RGL_IMPLEMENTATION
#include "misc/rgl.h"

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "RGL Smoke Test",
        .window_width = 800,
        .window_height = 600
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        return 1;
    }

    if (!RGL_Init()) {
        char* err_msg = NULL;
        if (SituationGetLastErrorMsg(&err_msg) == SITUATION_SUCCESS && err_msg) {
            fprintf(stderr, "RGL_Init failed: %s\n", err_msg);
        } else {
            fprintf(stderr, "RGL_Init failed\n");
        }
        SituationShutdown();
        return 1;
    }

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            RGL_Begin(-1);
            RGL_DrawRectangle((SitRectangle){200.0f, 150.0f, 400.0f, 300.0f}, 0.0f, RED);
            RGL_End();
        }

        SituationEndFrame();
    }

    RGL_Shutdown();
    SituationShutdown();
    return 0;
}
