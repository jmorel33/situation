#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or VULKAN
#include "../situation.h"
#include <stdio.h>
#include <cglm/cglm.h> // Requires cglm for vector math

// --- Simple State for an On-Screen Element ---
static vec2 g_element_position = {400.0f, 300.0f};
static float g_element_speed = 200.0f;

// --- Input Handling Logic ---
void process_input(float delta_time) {
    const float velocity = g_element_speed * delta_time;

    if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP))    { g_element_position[1] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN))  { g_element_position[1] += velocity; }
    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT))  { g_element_position[0] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) { g_element_position[0] += velocity; }

    if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = SituationGetMousePosition();
        g_element_position[0] = mouse_pos.x;
        g_element_position[1] = mouse_pos.y;
    }
}

// --- Rendering the Element (Simple Quad) ---
void render_element(SituationCommandBuffer cmd) {
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){g_element_position[0] - 25.0f, g_element_position[1] - 25.0f, 0.0f});
    glm_scale(model, (vec3){50.0f, 50.0f, 1.0f});

    vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

    SituationCmdDrawQuad(cmd, model, color);
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Input Handling";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    printf("Use WASD/Arrow Keys to move. Click to teleport. Close window to quit.\n");

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();

        SituationUpdateTimers();
        float delta_time = SituationGetFrameTime();

        process_input(delta_time);

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });
            render_element(cmd);
            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
