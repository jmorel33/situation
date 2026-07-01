/***************************************************************************************************
 *  Situation — 01: Open a Window
 *
 *  The simplest possible Situation application. Opens a 1280×1024 window, clears it to a
 *  deep-space gradient-like solid colour, and prints GPU / OS info to the console on startup.
 *
 *  What this example teaches:
 *    - SituationInit / SituationShutdown lifecycle
 *    - SITUATION_BEGIN_FRAME macro (polls input + updates timers in one call)
 *    - SituationAcquireFrameCommandBuffer / SituationEndFrame render loop
 *    - SituationCmdBeginRenderPass with a clear colour
 *    - SituationWindowShouldClose main-loop predicate
 *    - SituationGetGPUName / SituationGetGPUInfo / SituationGetOSInfo
 *
 *  Universal hotkeys (from sit_example.h):
 *    ESC   — quit          F11 — fullscreen       F9 — toggle VSync
 *    P     — pause         F12 — screenshot
 *
 *  Build:
 *    build\build_examples.bat static-opengl  01_open_a_window
 *    build\build_examples.bat static-vulkan  01_open_a_window
 ***************************************************************************************************/

/* Select a backend.  The build script passes -DSITUATION_USE_OPENGL or _VULKAN.
   This guard lets you also open the file directly in an editor / compile by hand. */
#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"   /* includes situation.h + font + hotkey scaffolding */
#include <cglm/cglm.h>

/* ─── Application ──────────────────────────────────────────────────────────────────────────── */

int main(int argc, char** argv)
{
    /* 1. Initialise — 1280×1024, VSync on, resizable */
    if (SitExample_Init(argc, argv, "01 — Open a Window") != SITUATION_SUCCESS) {
        return -1;
    }

    /* 2. Print hardware info — this is the "system awareness" part of Situation */
    {
        SituationGPUInfo  gpu  = {0};
        SituationCPUInfo  cpu  = {0};
        SituationMemoryInfo mem = {0};
        SituationOSInfo   os   = SituationGetOSInfo();
        SituationGetGPUInfo(&gpu);
        SituationGetCPUInfo(&cpu);
        SituationGetMemoryInfo(&mem);

        printf("\n╔══════════════════════════════════════════╗\n");
        printf("║  Situation %s\n", SituationGetVersionString());
        printf("║  Backend : %s\n", SituationGetGraphicsBackendName());
        printf("║  OS      : %s %s (build %u)\n",
               os.name, os.version, os.build_number);
        printf("║  CPU     : %s (%u cores / %u threads @ %.1f GHz)\n",
               cpu.name, cpu.core_count, cpu.thread_count, cpu.clock_speed_ghz);
        printf("║  GPU     : %s (%.1f GB VRAM)\n",
               gpu.name,
               (double)gpu.dedicated_memory_bytes / (1024.0 * 1024.0 * 1024.0));
        printf("║  RAM     : %.1f GB total / %.1f GB free\n",
               (double)mem.total_bytes     / (1024.0 * 1024.0 * 1024.0),
               (double)mem.available_bytes / (1024.0 * 1024.0 * 1024.0));
        printf("╚══════════════════════════════════════════╝\n\n");
    }

    /* Background clear colour — a calm dark-blue */
    const ColorRGBA BG = {12, 18, 38, 255};

    /* 3. Main loop */
    while (!SituationWindowShouldClose())
    {
        /* SitExample_BeginFrame polls input and timers.
           Returns 1 when the user presses ESC (handled by the hotkey layer). */
        if (SitExample_BeginFrame()) {
            break;
        }

        /* Pause: skip rendering but keep the loop alive */
        if (SituationIsAppPaused()) {
            continue;
        }

        /* ── Render ─────────────────────────────────────────────────────── */
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            continue;
        }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        /* Begin the only render pass — clear to our background colour */
        SituationRenderPassInfo pass = {
            .display_id       = -1,  /* -1 = main window */
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .clear  = { .color = BG }
            }
        };
        SituationCmdBeginRenderPass(cmd, &pass);

        /* Draw the shared HUD (title, FPS, hotkey hints) */
        SitExample_DrawHUD(cmd,
            "01 — Open a Window",
            "Just a window — no geometry, no assets.  ESC to quit.");

        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    /* 4. Cleanup */
    SitExample_Shutdown();
    return 0;
}
