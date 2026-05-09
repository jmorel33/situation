/*******************************************************************************
 * Situation Library - QUAD STORM
 * ==============================
 * Stress test: Thousands of animated quads with physics.
 * Demonstrates raw rendering throughput.
 *
 * Controls:
 *   UP/DOWN    - Add/Remove 500 quads
 *   V          - Toggle VSync (capped vs uncapped FPS)
 *   SPACE      - Randomize all velocities
 *   ESC        - Exit
 *
 * Window title shows: quad count, FPS, frame time
 ******************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// --- Configuration ---
#define MAX_QUADS      50000
#define INITIAL_QUADS  2000
#define WINDOW_W       1280
#define WINDOW_H       720

// --- Quad State ---
typedef struct {
    float x, y;
    float vx, vy;
    float size;
    Vector4 color;
} Quad;

static Quad quads[MAX_QUADS];
static int quad_count = INITIAL_QUADS;
static int vsync_on = 1;

// --- Helpers ---
static float randf(void) { return (float)rand() / (float)RAND_MAX; }
static float randf_range(float lo, float hi) { return lo + randf() * (hi - lo); }

static void init_quad(Quad* q) {
    q->x = randf_range(50.0f, WINDOW_W - 50.0f);
    q->y = randf_range(50.0f, WINDOW_H - 50.0f);
    q->vx = randf_range(-300.0f, 300.0f);
    q->vy = randf_range(-300.0f, 300.0f);
    q->size = randf_range(3.0f, 10.0f);
    q->color = (Vector4){{randf() * 0.8f + 0.2f, randf() * 0.8f + 0.2f, randf() * 0.8f + 0.2f, 1.0f}};
}

int main(int argc, char** argv) {
    srand(42);

    SituationInitInfo config = {
        .window_title = "Quad Storm",
        .window_width = WINDOW_W,
        .window_height = WINDOW_H,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        fprintf(stderr, "Init failed!\n");
        return -1;
    }

    // Initialize quads
    for (int i = 0; i < MAX_QUADS; i++) init_quad(&quads[i]);

    // FPS tracking
    double fps_timer = 0.0;
    int frame_count = 0;
    double last_fps = 0.0;
    double last_frametime = 0.0;
    char title_buf[256];

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        float dt = SituationGetFrameTime();
        if (dt > 0.1f) dt = 0.016f; // Clamp on first frame

        // --- Input ---
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;

        if (SituationIsKeyPressed(SIT_KEY_UP)) {
            quad_count += 500;
            if (quad_count > MAX_QUADS) quad_count = MAX_QUADS;
        }
        if (SituationIsKeyPressed(SIT_KEY_DOWN)) {
            quad_count -= 500;
            if (quad_count < 100) quad_count = 100;
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            for (int i = 0; i < quad_count; i++) {
                quads[i].vx = randf_range(-300.0f, 300.0f);
                quads[i].vy = randf_range(-300.0f, 300.0f);
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            vsync_on = !vsync_on;
            SituationSetVSync(vsync_on);
        }

        // --- Physics ---
        for (int i = 0; i < quad_count; i++) {
            Quad* q = &quads[i];
            q->x += q->vx * dt;
            q->y += q->vy * dt;

            if (q->x < 0.0f)              { q->x = 0.0f; q->vx = fabsf(q->vx); }
            if (q->x > WINDOW_W - q->size) { q->x = WINDOW_W - q->size; q->vx = -fabsf(q->vx); }
            if (q->y < 0.0f)              { q->y = 0.0f; q->vy = fabsf(q->vy); }
            if (q->y > WINDOW_H - q->size) { q->y = WINDOW_H - q->size; q->vy = -fabsf(q->vy); }
        }

        // --- Render ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {5, 5, 12, 255} } },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            // Draw all quads
            for (int i = 0; i < quad_count; i++) {
                mat4 model;
                glm_mat4_identity(model);
                glm_translate(model, (vec3){quads[i].x, quads[i].y, 0.0f});
                glm_scale(model, (vec3){quads[i].size, quads[i].size, 1.0f});
                SituationCmdDrawQuad(cmd, model, quads[i].color);
            }

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }

        // --- FPS Counter ---
        frame_count++;
        fps_timer += dt;
        if (fps_timer >= 0.5) {
            last_fps = frame_count / fps_timer;
            last_frametime = (fps_timer / frame_count) * 1000.0;
            frame_count = 0;
            fps_timer = 0.0;

            snprintf(title_buf, sizeof(title_buf),
                "QUAD STORM | %d quads | %.0f FPS | %.2f ms | VSync: %s | [UP/DOWN] +/-500 [V] vsync [SPACE] shuffle",
                quad_count, last_fps, last_frametime, vsync_on ? "ON" : "OFF");
            SituationSetWindowTitle(title_buf);
        }
    }

    SituationShutdown();
    return 0;
}
