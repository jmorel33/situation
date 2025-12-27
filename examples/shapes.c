/***************************************************************************************************
*   Situation Library - Example: 2D Shapes Loop
*   -------------------------------------------
*   Demonstrates drawing a loop of 2D shapes (quads) with varying colors and transformations.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Situation - Shapes Loop",
        .window_width = 800, .window_height = 600
    };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        float time = (float)SituationTimerGetTime();

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {20, 20, 20, 255} } }
            };

            SituationCmdBeginRenderPass(cmd, &pass);

            // Loop to draw shapes
            int count = 10;
            for (int i = 0; i < count; i++) {
                float t = (float)i / count;
                float angle = t * 2.0f * 3.14159f + time;
                float radius = 0.5f;

                mat4 model;
                glm_mat4_identity(model);

                // Orbit
                vec3 pos = { cosf(angle) * radius, sinf(angle) * radius, 0.0f };
                glm_translate(model, pos);

                // Spin
                glm_rotate(model, time * 2.0f + t * 5.0f, (vec3){0.0f, 0.0f, 1.0f});

                // Scale
                float s = 0.1f + 0.05f * sinf(time * 5.0f + i);
                glm_scale(model, (vec3){s, s, 1.0f});

                // Color (HSL-ish)
                Vector4 color = {{
                    0.5f + 0.5f * cosf(t * 6.28f + time), // R
                    0.5f + 0.5f * sinf(t * 6.28f + time), // G
                    0.5f + 0.5f * sinf(t * 6.28f + time * 0.5f), // B
                    1.0f
                }};

                SituationCmdDrawQuad(cmd, model, color);
            }

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
