/***************************************************************************************************
*   Situation Library - Example: Interactive Quad
*   -------------------------------------------
*   This example demonstrates how to connect Input to Rendering using the High-Level API.
*
*   Controls:
*   - ARROW KEYS / WASD: Move the square.
*   - LEFT CLICK:        Change color.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h> // Matrix math library

// --- Game State ---
// We keep track of the player's position and color here.
static vec3 player_pos = {0.0f, 0.0f, 0.0f};
static vec4 player_color = {0.0f, 1.0f, 1.0f, 1.0f}; // Start Cyan

int main(void) {
    // 1. Initialize
    SituationInitInfo config = {
        .window_title = "Situation - Interactive Quad",
        .window_width = 800, .window_height = 600
    };
    SituationContext ctx = SituationCreateContext(&config);
    if (!ctx) return -1;

    // Optional: Set this context as the "current" one for this thread.
    // This allows you to call API functions with NULL instead of 'ctx'.
    // For clarity, this example will pass 'ctx' explicitly.
    SituationSetCurrentContext(ctx);

    printf("Controls:\n  [WASD/Arrows] Move\n  [Left Click]  Randomize Color\n");

    // 2. Main Loop
    while (!SituationWindowShouldClose(ctx)) {
        SituationPollInputEvents(ctx);
        SituationUpdateTimers(ctx);

        // --- UPDATE LOGIC ---

        // A. Movement
        // We multiply speed by SituationGetFrameTime() (Delta Time)
        // to ensure movement speed is the same on 60 FPS and 144 FPS.
        float speed = 2.0f * SituationGetFrameTime(ctx);

        if (SituationIsKeyDown(ctx, SIT_KEY_UP)    || SituationIsKeyDown(ctx, SIT_KEY_W)) player_pos[1] += speed;
        if (SituationIsKeyDown(ctx, SIT_KEY_DOWN)  || SituationIsKeyDown(ctx, SIT_KEY_S)) player_pos[1] -= speed;
        if (SituationIsKeyDown(ctx, SIT_KEY_LEFT)  || SituationIsKeyDown(ctx, SIT_KEY_A)) player_pos[0] -= speed;
        if (SituationIsKeyDown(ctx, SIT_KEY_RIGHT) || SituationIsKeyDown(ctx, SIT_KEY_D)) player_pos[0] += speed;

        // B. Mouse Interaction
        if (SituationIsMouseButtonPressed(ctx, 0)) { // 0 is Left Click
            // Randomize RGB, keep Alpha at 1.0
            player_color[0] = (float)(rand() % 100) / 100.0f;
            player_color[1] = (float)(rand() % 100) / 100.0f;
            player_color[2] = (float)(rand() % 100) / 100.0f;
        }

        // --- RENDER LOGIC ---
        if (SituationAcquireFrameCommandBuffer(ctx)) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer(ctx);

            // Setup Render Pass (Clear to Dark Gray)
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {30, 30, 30, 255} } }
            };

            SituationCmdBeginRenderPass(ctx, cmd, &pass);

            // Calculate Matrix
            mat4 model;
            glm_mat4_identity(model);
            // Apply our game state position to the matrix
            glm_translate(model, player_pos);
            // Scale it down slightly (default quad is -1 to 1, which fills screen)
            glm_scale(model, (vec3){0.2f, 0.2f, 1.0f});

            // Draw using the internal renderer
            SituationCmdDrawQuad(ctx, cmd, model, player_color);

            SituationCmdEndRenderPass(ctx, cmd);
            SituationEndFrame(ctx);
        }
    }

    // 3. Shutdown
    SituationDestroyContext(ctx);
    return 0;
}