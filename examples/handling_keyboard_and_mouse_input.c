/***************************************************************************************************
*   Situation Library - Example: Input Handling
*   -------------------------------------------
*   This example demonstrates real-time input processing for Keyboard and Mouse.
*
*   Key Concepts:
*   1. Continuous Input (IsKeyDown) vs Discrete Input (IsKeyPressed).
*   2. Mouse Position and Buttons.
*   3. Delta Time movement (Framerate independence).
*
*   Controls:
*   - WASD / ARROWS: Move the White Square.
*   - SPACE:         Change Color (Discrete Event).
*   - LEFT CLICK:    Teleport Square to Mouse.
*   - RIGHT CLICK:   Reset to Center.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"
#include <cglm/cglm.h>

// --- State ---
static vec2 g_pos = {0.0f, 0.0f};      // Position (World Space, 0,0 is center)
static vec4 g_color = {1.0f, 1.0f, 1.0f, 1.0f}; // Current Color
static float g_speed = 2.0f;           // Units per second

// --- Logic ---
void update_game() {
    // 1. Get Delta Time
    // This is the time in seconds the last frame took to render.
    // Multiplying movement by this ensures the object moves at the same speed
    // regardless of whether the game is running at 30 FPS or 144 FPS.
    float dt = SituationGetFrameTime();

    // 2. Continuous Input (Movement)
    // We use 'IsKeyDown' because we want movement to happen every frame the key is held.
    if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP))    g_pos[1] += g_speed * dt;
    if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN))  g_pos[1] -= g_speed * dt;
    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT))  g_pos[0] -= g_speed * dt;
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) g_pos[0] += g_speed * dt;

    // 3. Discrete Input (Action)
    // We use 'IsKeyPressed' because we only want this to happen ONCE when the key is tapped.
    if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
        // Randomize Color
        g_color[0] = (float)(rand() % 100) / 100.0f;
        g_color[1] = (float)(rand() % 100) / 100.0f;
        g_color[2] = (float)(rand() % 100) / 100.0f;
        printf("Color Changed!\n");
    }

    // 4. Mouse Input
    // Button 0 = Left, 1 = Right, 2 = Middle
    if (SituationIsMouseButtonPressed(0)) {
        // Get Mouse Position (Pixels, Top-Left is 0,0)
        vec2 mouse_pixels = {0};
        glm_vec2_copy(SituationGetMousePosition(), mouse_pixels);
        
        // Convert Pixel Coordinates to World Coordinates (Approximate for this example)
        // The default Quad shader draws from -1.0 to 1.0.
        // Screen Center is (Width/2, Height/2).
        float screen_w = (float)SituationGetScreenWidth();
        float screen_h = (float)SituationGetScreenHeight();

        // Normalize to [-1, 1]
        g_pos[0] = (mouse_pixels[0] / screen_w) * 2.0f - 1.0f;
        // Flip Y because Screen Y is Down, World Y is Up
        g_pos[1] = -((mouse_pixels[1] / screen_h) * 2.0f - 1.0f); 
    }

    if (SituationIsMouseButtonPressed(1)) {
        // Reset
        g_pos[0] = 0.0f;
        g_pos[1] = 0.0f;
        g_color[0] = 1.0f; g_color[1] = 1.0f; g_color[2] = 1.0f;
    }
}

// --- Rendering ---
void render_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo pass = {
        .display_id = -1,
        .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = { 20, 20, 30, 255 } } }
    };

    SituationCmdBeginRenderPass(cmd, &pass);

    // Draw Square
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){g_pos[0], g_pos[1], 0.0f});
    glm_scale(model, (vec3){0.1f, 0.1f, 1.0f}); // Make it small

    SituationCmdDrawQuad(cmd, model, g_color);

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

// --- Main ---
int main(int argc, char** argv) {
    SituationInitInfo config = { 
        .window_title = "Situation - Input Handling",
        .window_width = 800, 
        .window_height = 600 
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    printf("Controls:\n");
    printf("  [WASD] Move\n");
    printf("  [SPACE] Change Color\n");
    printf("  [L-CLICK] Teleport\n");
    printf("  [R-CLICK] Reset\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME(); // Polls Input + Updates Timers
        update_game();
        render_frame();
    }

    SituationShutdown();
    return 0;
}