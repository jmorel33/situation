/**********************************************************************************************
 *
 * @file console_host_app.c
 *   (c) 2025-2026 Jacques Morel
 * @brief KaOS Terminal host — owns Situation init/shutdown and the main loop.
 *
 * The console CLI lives in console.h (define CONSOLE_IMPLEMENTATION in exactly one TU).
 **********************************************************************************************/
#if defined(_WIN32)
  #define NOMINMAX
#endif

#ifndef SITUATION_USE_OPENGL
#define SITUATION_USE_OPENGL
#endif
#define SITUATION_IMPLEMENTATION
#include "situation.h"

#define CONSOLE_IMPLEMENTATION
#include "console.h"

int main(int argc, char** argv) {
    const char* window_title = "KaOS - Kaizen Operating System v0.1 (Situation-Aware)";
    const int target_fps = 60;

    SituationInitInfo init_info = {0};
    init_info.enable_vulkan_validation = true;
    init_info.window_width = 80 * 8 * 2;
    init_info.window_height = 50 * 8 * 2;
    init_info.window_title = window_title;
    init_info.initial_active_window_flags =
        SITUATION_WINDOW_STATE_RESIZABLE | SITUATION_WINDOW_STATE_VSYNC_HINT | SITUATION_WINDOW_STATE_ALWAYS_RUN;
    init_info.initial_inactive_window_flags = SITUATION_WINDOW_STATE_ALWAYS_RUN;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    /* Match sit_example.h: ST path (render_thread_count=0) sync-reads the full framebuffer
     * every EndFrame for LoadImageFromScreen cache — ~1 FPS at 1280×800. */
    init_info.render_thread_count = 1;
    init_info.backpressure_policy = SIT_RENDER_BACKPRESSURE_YIELD;
#endif

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        fprintf(stderr, "FATAL: SituationInit failed: %s\n", err_msg ? err_msg : "Unknown error");
        if (err_msg) {
            free(err_msg);
        }
        return 1;
    }

    SituationSetTraceLogLevel(SIT_LOG_NONE);
    SituationSetTargetFPS(target_fps);

    if (SituationGetInitState() != SITUATION_STATE_READY) {
        fprintf(stderr, "FATAL: Situation not in READY state after init\n");
        SituationShutdown();
        return 1;
    }

    ConsoleConfig cfg = CONSOLE_CONFIG_DEFAULT;
    if (!Console_Init(&cfg)) {
        fprintf(stderr, "FATAL: Console_Init failed\n");
        SituationShutdown();
        return 1;
    }

    while (!SituationWindowShouldClose() && !Console_ShouldExit()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        Console_Update();
    }

    Console_Shutdown();
    SituationShutdown();
    return 0;
}
