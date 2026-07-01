/***************************************************************************************************
 *  Situation — 10: Thread Pool (Parallel Work)
 *
 *  64×64 Game of Life — each frame runs several generations on the CPU, then uploads RGBA8
 *  and draws the grid.  Press T to toggle serial (main thread) vs parallel (thread pool).
 *
 *  Keys:
 *    T       toggle serial / parallel
 *    R       re-seed grid (SubmitJobEx + dependency demo)
 *    SPACE   pause simulation
 *
 *  Build:
 *    build\build_examples.bat static-opengl  10_thread_pool
 *    build\build_examples.bat static-vulkan  10_thread_pool
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <string.h>

#define GW 64
#define GH 64
#define PASSES 24

typedef struct {
    const uint8_t* cur;
    uint8_t*       next;
    int            w, h;
} GolPass;

typedef struct {
    uint8_t* buf;
    int      w, h;
} SeedCtx;

static uint8_t              g_buf[2][GW * GH];
static int                  g_front = 0;
static SituationImage       g_img   = {0};
static SituationTexture     g_tex   = {0};
static SituationThreadPool    g_pool  = {0};
static int                  g_pool_ok = 0;
static int                  g_parallel = 1;
static int                  g_pause = 0;
static uint64_t             g_gen = 0;
static uint64_t             g_dispatches = 0;
static double               g_step_ms = 0.0;

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y,
                float fs, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, fs, 0.0f, c);
}

static int neighbors(const uint8_t* b, int w, int h, int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            n += b[((y + dy + h) % h) * w + ((x + dx + w) % w)];
        }
    }
    return n;
}

static void gol_cell(int idx, void* ud)
{
    GolPass* p = (GolPass*)ud;
    int x = idx % p->w;
    int y = idx / p->w;
    int alive = p->cur[y * p->w + x];
    int n = neighbors(p->cur, p->w, p->h, x, y);
    /* Extra work so serial vs parallel is visible at 64×64 */
    volatile unsigned h = (unsigned)idx * 2654435761u;
    for (int k = 0; k < 96; ++k) {
        h = h * 1664525u + 1013904223u;
    }
    (void)h;
    p->next[y * p->w + x] = (uint8_t)((n == 3 || (n == 2 && alive)) ? 1 : 0);
}

static void step_once(void)
{
    uint8_t* cur = g_buf[g_front];
    uint8_t* nxt = g_buf[g_front ^ 1];
    GolPass pass = {cur, nxt, GW, GH};

    if (g_parallel && g_pool_ok) {
        SituationDispatchParallel(&g_pool, GW * GH, 32, gol_cell, &pass);
        g_dispatches++;
    } else {
        for (int i = 0; i < GW * GH; ++i) {
            gol_cell(i, &pass);
        }
    }
    g_front ^= 1;
    g_gen++;
}

static void upload_texture(void)
{
    const uint8_t* cur = g_buf[g_front];
    for (int y = 0; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            ColorRGBA c = cur[y * GW + x]
                ? (ColorRGBA){100, 220, 255, 255}
                : (ColorRGBA){14, 16, 26, 255};
            SituationSetPixelColor(&g_img, x, y, c);
        }
    }

    SituationTexture new_tex = {0};
    if (SituationCreateTexture(g_img, false, &new_tex) == SITUATION_SUCCESS) {
        if (g_tex.generation > 0) {
            SituationDestroyTexture(&g_tex);
        }
        g_tex = new_tex;
    }
    // on failure we keep the previous texture (if any)
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggressive-loop-optimizations"
#endif
static void job_random(void* data, void* ctx)
{
    (void)ctx;
    SeedCtx* s = (SeedCtx*)data;
    int total = (int)((size_t)s->w * (size_t)s->h);
    for (int i = 0; i < total; ++i) {
        s->buf[i] = (uint8_t)(((i * 1103515245 + 12345) >> 16) & 1);
    }
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void job_gliders(void* data, void* ctx)
{
    (void)ctx;
    SeedCtx* s = (SeedCtx*)data;
    static const int g[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
    int bases[3][2] = {{s->w / 5, s->h / 5}, {s->w / 2, s->h / 3}, {s->w * 3 / 5, s->h / 2}};
    for (int b = 0; b < 3; ++b) {
        for (int i = 0; i < 5; ++i) {
            int x = (bases[b][0] + g[i][0]) % s->w;
            int y = (bases[b][1] + g[i][1]) % s->h;
            s->buf[y * s->w + x] = 1;
        }
    }
}

static void reseed_async(void)
{
    if (!g_pool_ok) return;
    SeedCtx sc = {g_buf[g_front], GW, GH};
    SituationJobId j0 = SituationSubmitJobEx(
        &g_pool, job_random, &sc, sizeof(sc), SIT_SUBMIT_HIGH_PRIORITY);
    SituationJobId j1 = SituationSubmitJobEx(
        &g_pool, job_gliders, &sc, sizeof(sc), SIT_SUBMIT_HIGH_PRIORITY);
    if (j0 && j1) {
        SituationError derr = SituationAddJobDependency(&g_pool, j0, j1);
        if (derr != SITUATION_SUCCESS) {
            fprintf(stderr, "[10] AddJobDependency failed (%d) — gliders may not be ordered after random\n", derr);
        }
    }
    SituationWaitForAllJobs(&g_pool);
    g_gen = 0;
}

static void draw_hud(SituationCommandBuffer cmd, int sw, int sh)
{
    char line[128];
    SituationThreadPoolSnapshot snap = {0};
    if (g_pool_ok) {
        SituationGetThreadPoolSnapshot(&g_pool, &snap);
    }

    snprintf(line, sizeof line, "Gen %llu  |  %s  |  %.2f ms / frame (%d passes)",
             (unsigned long long)g_gen,
             g_parallel ? "PARALLEL (DispatchParallel)" : "SERIAL (main thread)",
             g_step_ms, PASSES);
    txt(cmd, line, 24.0f, 72.0f, 13.0f, (ColorRGBA){255, 230, 140, 255});

    snprintf(line, sizeof line,
             "DispatchParallel calls: %llu  |  workers: %zu  |  active jobs: %d",
             (unsigned long long)g_dispatches,
             snap.worker_count, snap.active_jobs);
    txt(cmd, line, 24.0f, 92.0f, 11.0f, (ColorRGBA){150, 175, 220, 240});

    float side = (float)fminf(sw - 48.0f, sh - 200.0f);
    float ox = ((float)sw - side) * 0.5f;
    float oy = 130.0f;
    SitRectangle tex_src = {0.0f, 0.0f, (float)GW, (float)GH};
    SitRectangle tex_dst = {ox, oy, side, side};
    SituationCmdDrawTexture(cmd, g_tex, tex_src, tex_dst, (Vector2){{0.0f, 0.0f}}, 0.0f,
                            (ColorRGBA){255, 255, 255, 255});

    txt(cmd, "Update CPU buffers -> upload texture -> draw  (update-before-draw)",
        24.0f, (float)sh - 72.0f, 10.0f, (ColorRGBA){120, 130, 160, 210});
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "10 — Thread Pool") != SITUATION_SUCCESS) {
        return -1;
    }

#ifndef SITUATION_ENABLE_THREADING
    fprintf(stderr, "[10] SITUATION_ENABLE_THREADING required — rebuild with build_examples.bat\n");
    SitExample_Shutdown();
    return -1;
#endif

    if (SituationCreateImage(GW, GH, 4, &g_img) != SITUATION_SUCCESS) {
        SitExample_Shutdown();
        return -1;
    }
    g_img.channels = 4;

    if (SituationCreateThreadPool(&g_pool, 0, 256, 0.0, true) == SITUATION_SUCCESS) {
        g_pool_ok = 1;
        reseed_async();
        printf("[10] Thread pool: %zu workers\n", g_pool.thread_count);
    } else {
        fprintf(stderr, "[10] Thread pool init failed — serial mode only\n");
        g_parallel = 0;
        SeedCtx sc = {g_buf[g_front], GW, GH};
        job_random(&sc, NULL);
        job_gliders(&sc, NULL);
    }

    upload_texture();

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        if (SituationIsKeyPressed(SIT_KEY_T) && g_pool_ok) {
            g_parallel = !g_parallel;
            printf("[10] Mode: %s\n", g_parallel ? "parallel" : "serial");
        }
        bool do_step = !g_pause && !SituationIsAppPaused();
        if (SituationIsKeyPressed(SIT_KEY_R) && g_pool_ok) {
            reseed_async();
            upload_texture();
            do_step = false;  /* show the freshly seeded grid this frame instead of immediately advancing 24 gens */
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            g_pause = !g_pause;
        }

        if (do_step) {
            double t0 = SituationTimerGetTime();
            for (int p = 0; p < PASSES; ++p) {
                step_once();
            }
            g_step_ms = (SituationTimerGetTime() - t0) * 1000.0;
            upload_texture();
        }

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass =
            SituationRenderPassInfoDefault(-1, (ColorRGBA){6, 8, 14, 255});
        SituationCmdBeginRenderPass(cmd, &pass);
        draw_hud(cmd, SituationGetRenderWidth(), SituationGetRenderHeight());
        SitExample_DrawHUD(cmd, "10 — Thread Pool",
            "T serial/parallel  R re-seed  SPACE pause  — Game of Life 64x64");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    if (g_tex.generation > 0) SituationDestroyTexture(&g_tex);
    SituationUnloadImage(g_img);
    if (g_pool_ok) SituationDestroyThreadPool(&g_pool);
    SitExample_Shutdown();
    return 0;
}
