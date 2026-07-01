/**

 * @file test_frame_pacing.c

 * @brief Frame pacing baseline (GAME_LOOP_PERFORMANCE_PLAN Phase 0).

 *

 * Records vsync-paced frame metrics for before/after comparison. Render thread ON

 * is the primary configuration; ST is optional later.

 *

 * Screen-capture regression tests (gl_load_*) live in test_graphics.c (Diagnostics & Readbacks).

 *

 * (c) 2025-2026 Jacques Morel — MIT Licensed

 */



#include "sit_api_include.h"

#include "sit_test_framework.h"

#include "sit_test_window.h"

#include <math.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>



#define SIT_FP_WARMUP_FRAMES   30

#define SIT_FP_SAMPLE_FRAMES   600

#define SIT_FP_MAX_SAMPLES     SIT_FP_SAMPLE_FRAMES



#if defined(SITUATION_USE_VULKAN)

#define SIT_FP_BACKEND_TAG "vulkan"

#elif defined(SITUATION_USE_OPENGL)

#define SIT_FP_BACKEND_TAG "opengl"

#else

#define SIT_FP_BACKEND_TAG "unknown"

#endif



static bool g_fp_init_ok = false;



static int sit_fp_compare_float(const void* a, const void* b) {

    float fa = *(const float*)a;

    float fb = *(const float*)b;

    return (fa > fb) - (fa < fb);

}



static void sit_fp_init_config(SituationInitInfo* config, const char* title) {

    memset(config, 0, sizeof(*config));

    config->window_width = 1280;

    config->window_height = 720;

    config->window_title = title;

    config->main_thread_name = title;

    config->output_color_depth = SIT_OUTPUT_COLOR_8BIT;

    config->initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT;

#if defined(SITUATION_ENABLE_RENDER_THREAD)

    config->render_thread_count = 1;

    config->backpressure_policy = SIT_RENDER_BACKPRESSURE_YIELD;

#endif

}



static void frame_pacing_setup(void) {

    SituationInitInfo config;

    sit_fp_init_config(&config, "SIT_FRAME_PACING");



    SituationError err = SituationInit(0, NULL, &config);

    g_fp_init_ok = (err == SITUATION_SUCCESS);

    if (!g_fp_init_ok) {

        g_sit_current_test_failed = true;

        longjmp(g_sit_test_jmp_buf, 1);

    }



    SituationSetTargetFPS(0);

    SituationSetVSync(true);

}



static void frame_pacing_teardown(void) {

    if (g_fp_init_ok) {

        SituationShutdown();

        g_fp_init_ok = false;

    }

}



static float sit_fp_percentile_ms(float* sorted_ms, int count, double quantile) {

    if (count <= 0) {

        return 0.0f;

    }

    if (count == 1) {

        return sorted_ms[0];

    }

    int idx = (int)((double)(count - 1) * quantile + 0.5);

    if (idx < 0) {

        idx = 0;

    }

    if (idx >= count) {

        idx = count - 1;

    }

    return sorted_ms[idx];

}



static bool sit_fp_draw_empty_frame(void) {

    SituationPollInputEvents();

    SituationUpdateTimers();

    SituationError err = SituationAcquireFrameCommandBuffer();

    if (err != SITUATION_SUCCESS) {

        return false;

    }



    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    if (!cmd) {

        SituationEndFrame();

        return false;

    }



    SituationRenderPassInfo rp = {0};

    rp.display_id = -1;

    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;

    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;

    rp.color_attachment.clear.color = (ColorRGBA){16, 20, 28, 255};

    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;

    rp.depth_attachment.clear.depth = 1.0f;



    err = SituationCmdBeginRenderPass(cmd, &rp);

    if (err != SITUATION_SUCCESS) {

        SituationEndFrame();

        return false;

    }

    err = SituationCmdEndRenderPass(cmd);

    if (err != SITUATION_SUCCESS) {

        SituationEndFrame();

        return false;

    }

    return SituationEndFrame() == SITUATION_SUCCESS;

}



static void sit_fp_write_baseline_json(

        const char* path,

        int refresh_hz,

        float median_ms,

        float p95_ms,

        float max_ms,

        int fps,

        uint32_t spikes,

        uint64_t bp_ns,

        uint64_t fence_ns,

        uint64_t exec_ns,

        uint64_t pres_ns)

{

    FILE* f = fopen(path, "wb");

    if (!f) {

        fprintf(stderr, "[frame_pacing] WARN: could not write baseline JSON: %s\n", path);

        return;

    }



    fprintf(f,

        "{\n"

        "  \"plan\": \"GAME_LOOP_PERFORMANCE_PLAN Phase 0\",\n"

        "  \"backend\": \"%s\",\n"

        "  \"render_thread\": true,\n"

        "  \"window\": \"1280x720\",\n"

        "  \"vsync\": true,\n"

        "  \"target_fps\": 0,\n"

        "  \"warmup_frames\": %d,\n"

        "  \"sample_frames\": %d,\n"

        "  \"max_frames_in_flight\": %d,\n"

        "  \"refresh_hz\": %d,\n"

        "  \"frame_time_median_ms\": %.4f,\n"

        "  \"frame_time_p95_ms\": %.4f,\n"

        "  \"frame_time_max_ms\": %.4f,\n"

        "  \"situation_get_fps\": %d,\n"

        "  \"spike_count\": %u,\n"

        "  \"last_phases_ns\": {\n"

        "    \"backpressure\": %llu,\n"

        "    \"fence\": %llu,\n"

        "    \"execute\": %llu,\n"

        "    \"present\": %llu\n"

        "  }\n"

        "}\n",

        SIT_FP_BACKEND_TAG,

        SIT_FP_WARMUP_FRAMES,

        SIT_FP_SAMPLE_FRAMES,

        (int)SITUATION_MAX_FRAMES_IN_FLIGHT,

        refresh_hz,

        (double)median_ms,

        (double)p95_ms,

        (double)max_ms,

        fps,

        (unsigned)spikes,

        (unsigned long long)bp_ns,

        (unsigned long long)fence_ns,

        (unsigned long long)exec_ns,

        (unsigned long long)pres_ns);

    fclose(f);

    fprintf(stderr, "[frame_pacing] baseline JSON: %s\n", path);

}



/**

 * VSync ON, render thread ON, empty scene — record pacing metrics (Phase 0 gate).

 */

static void test_frame_pacing_vsync_baseline(void) {

    const int refresh_hz = SituationGetMonitorRefreshRate(0);

    float samples_ms[SIT_FP_MAX_SAMPLES];

    int sample_count = 0;



    for (int frame = 0; frame < SIT_FP_WARMUP_FRAMES + SIT_FP_SAMPLE_FRAMES; ++frame) {

        SIT_ASSERT(sit_fp_draw_empty_frame());



        if (frame >= SIT_FP_WARMUP_FRAMES && sample_count < SIT_FP_MAX_SAMPLES) {

            float dt = SituationGetFrameTime();

            SIT_ASSERT(dt > 0.0f);

            samples_ms[sample_count++] = dt * 1000.0f;

        }

    }



    SIT_ASSERT_EQ(sample_count, SIT_FP_SAMPLE_FRAMES);



    float sorted[SIT_FP_MAX_SAMPLES];

    memcpy(sorted, samples_ms, (size_t)sample_count * sizeof(float));

    qsort(sorted, (size_t)sample_count, sizeof(float), sit_fp_compare_float);



    const float median_ms = sit_fp_percentile_ms(sorted, sample_count, 0.5);

    const float p95_ms = sit_fp_percentile_ms(sorted, sample_count, 0.95);

    float max_ms = sorted[sample_count - 1];



    uint64_t bp_ns = 0, fence_ns = 0, exec_ns = 0, pres_ns = 0;

    SituationGetLastFramePhases(&bp_ns, &fence_ns, &exec_ns, &pres_ns);



    const int fps = SituationGetFPS();

    const uint32_t spikes = SituationGetFrameSpikeCount();

    const double max_frame_s = SituationGetMaxFrameTime();



    char json_path[256];

    snprintf(json_path, sizeof(json_path),

             "build/tests/results/frame_pacing_baseline_%s.json", SIT_FP_BACKEND_TAG);

    sit_fp_write_baseline_json(json_path, refresh_hz, median_ms, p95_ms, max_ms, fps, spikes,

                               bp_ns, fence_ns, exec_ns, pres_ns);



    char hist_path[256];

    snprintf(hist_path, sizeof(hist_path),

             "build/tests/results/frame_pacing_histogram_%s.txt", SIT_FP_BACKEND_TAG);

    {

        char histogram[4096];

        memset(histogram, 0, sizeof(histogram));

        SituationExportRenderHistogram(histogram, sizeof(histogram));

        FILE* hf = fopen(hist_path, "wb");

        if (hf) {

            fputs(histogram, hf);

            fclose(hf);

            fprintf(stderr, "[frame_pacing] histogram: %s\n", hist_path);

        }

    }



    fprintf(stderr,

            "[frame_pacing] refresh=%d Hz median=%.3f ms p95=%.3f ms max=%.3f ms "

            "fps=%d spikes=%u phases(ex=%llu pr=%llu bp=%llu) max_frame=%.3f ms\n",

            refresh_hz, (double)median_ms, (double)p95_ms, (double)max_ms,

            fps, (unsigned)spikes,

            (unsigned long long)exec_ns, (unsigned long long)pres_ns, (unsigned long long)bp_ns,

            max_frame_s * 1000.0);



    /* Soft gate @ draft — warn only so non-60 Hz panels still record baseline. */

    const bool near_60hz = (refresh_hz >= 55 && refresh_hz <= 65);

    if (near_60hz) {

        if (median_ms < 15.5f || median_ms > 17.5f) {

            fprintf(stderr,

                    "[frame_pacing] WARN: median frame_time %.3f ms outside [15.5, 17.5] (60 Hz draft band)\n",

                    (double)median_ms);

        }

        if (p95_ms >= 22.0f) {

            fprintf(stderr,

                    "[frame_pacing] WARN: p95 frame_time %.3f ms >= 22 ms (draft budget)\n",

                    (double)p95_ms);

        }

        if (exec_ns < 4000000ull && pres_ns > exec_ns * 2ull) {

            fprintf(stderr,

                    "[frame_pacing] WARN: present phase dominant vs execute (ex=%llu pr=%llu ns)\n",

                    (unsigned long long)exec_ns, (unsigned long long)pres_ns);

        }

    } else {

        fprintf(stderr,

                "[frame_pacing] NOTE: refresh %d Hz — skipping 60 Hz soft band checks\n",

                refresh_hz);

    }



    SIT_ASSERT(median_ms > 1.0f && median_ms < 100.0f);

    SIT_ASSERT(max_ms < 500.0f);

}



static SitTestCase g_frame_pacing_tests[] = {

    {"frame_pacing_vsync_baseline", test_frame_pacing_vsync_baseline, true},

};



const SitTestModule g_module_frame_pacing = {

    .name = "frame_pacing",

    .setup = frame_pacing_setup,

    .teardown = frame_pacing_teardown,

    .tests = g_frame_pacing_tests,

    .test_count = (int)(sizeof(g_frame_pacing_tests) / sizeof(g_frame_pacing_tests[0])),

    .requires_context = true,

    .requires_visible_window = true,

};


