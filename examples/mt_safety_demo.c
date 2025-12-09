#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "situation.h"

// Define implementation only in one file
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Use OpenGL for this demo
#include "situation.h"

// Dummy main loop for demonstration
int main(int argc, char** argv) {
    printf("Situation v2.3.24a Safety Demo\n");

    SituationInitInfo info = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Safety Zenith Demo",
        .render_thread_count = 1, // Enable MT
        .backpressure_policy = SIT_RENDER_BACKPRESSURE_SPIN
    };

    if (SituationInit(argc, argv, &info) != SITUATION_SUCCESS) {
        fprintf(stderr, "Init failed: %s\n", SituationGetLastErrorMsg());
        return 1;
    }

    printf("Running 10k frame push simulation...\n");

    SituationSetTargetFPS(144); // 6.9ms target

    // Simulation loop
    for (int i = 0; i < 10000; i++) {
        SITUATION_BEGIN_FRAME();

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            // In a real app, we'd record commands here
            // SituationCmdDrawQuad(cmd, GLM_MAT4_IDENTITY, (Vector4){{1,0,1,1}});

            // Simulate variable load to trigger adaptive policy
            if (i % 1000 == 0) {
                 // Spike!
                 struct timespec ts = {0, 10000000}; // 10ms spike
                 thrd_sleep(&ts, NULL);
            }

            SituationEndFrame();
        }

        // Print stats occasionally
        if (i % 2000 == 0) {
            uint64_t avg, max;
            SituationGetRenderLatencyStats(&avg, &max);
            printf("Frame %d | Avg Latency: %lu ns | Max Latency: %lu ns\n", i, avg, max);
        }
    }

    printf("Shutting down... (Leak check should be silent)\n");
    SituationShutdown();
    printf("Done.\n");
    return 0;
}
