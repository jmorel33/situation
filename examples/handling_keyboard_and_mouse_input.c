/*
 * Handling Keyboard and Mouse Input
 *
 * This example demonstrates how to process real-time input from the keyboard
 * and mouse to manipulate an object on the screen.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"

// Standard C libraries and cglm for vector math.
#include <stdio.h>
#include <cglm/cglm.h>

// --- Simple State for an On-Screen Element ---
// We use global variables to store the position and speed of a movable square.
static vec2 g_element_position = {400.0f, 300.0f}; // Initial position at the center of the window
static float g_element_speed = 200.0f;             // Speed in pixels per second

// --- Input Handling Logic ---
// This function is called every frame to check the input state and update the element's position.
void process_input(float delta_time) {
    // Calculate the distance to move this frame based on speed and delta_time.
    // This makes the movement speed independent of the frame rate.
    const float velocity = g_element_speed * delta_time;

    // --- Keyboard Input for Continuous Movement ---
    // `SituationIsKeyDown` checks if a key is currently being held down.
    // This is suitable for actions that should repeat every frame, like movement.
    if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP))    { g_element_position[1] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN))  { g_element_position[1] += velocity; }
    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT))  { g_element_position[0] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) { g_element_position[0] += velocity; }

    // --- Mouse Input for Discrete Actions ---
    // `SituationIsMouseButtonPressed` checks for a single click event. It returns true
    // only on the frame the button is first pressed.
    if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
        // `SituationGetMousePosition` retrieves the current cursor coordinates.
        Vector2 mouse_pos = SituationGetMousePosition();
        // Teleport the element to the mouse cursor's position.
        g_element_position[0] = mouse_pos.x;
        g_element_position[1] = mouse_pos.y;
    }

    // For other types of input, you can also use:
    // - SituationIsKeyPressed(SIT_KEY_...): For single-press events (like jumping).
    // - SituationGetMouseScroll(): To get the amount the mouse wheel was scrolled this frame.
}

// --- Rendering the Element ---
// This function draws a simple white square at the element's current position.
void render_element(SituationCommandBuffer cmd) {
    // Use cglm to create a model matrix for the square.
    mat4 model;
    glm_mat4_identity(model);
    // The position is translated by -25px because the quad is drawn from its center.
    glm_translate(model, (vec3){g_element_position[0] - 25.0f, g_element_position[1] - 25.0f, 0.0f});
    // Scale the quad to be 50x50 pixels.
    glm_scale(model, (vec3){50.0f, 50.0f, 1.0f});

    vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // White color

    // `SituationCmdDrawQuad` is a helper for drawing simple textured or colored quads.
    SituationCmdDrawQuad(cmd, model, color);
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Input Handling";
    init_info.window_width = 800;
    init_info.window_height = 600;

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    printf("Use WASD/Arrow Keys to move. Click to teleport. Close window to quit.\n");

    // Main application loop.
    while (!SituationWindowShouldClose()) {
        // CRITICAL: Poll for new events at the start of each frame.
        // This updates the internal state for functions like `SituationIsKeyDown`.
        SituationPollInputEvents();

        // Update timers to get the delta time for the last frame.
        SituationUpdateTimers();
        float delta_time = SituationGetFrameTime();

        // Process all input based on the new events and delta time.
        process_input(delta_time);

        // Standard rendering loop.
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });
            render_element(cmd); // Draw our movable square.
            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
