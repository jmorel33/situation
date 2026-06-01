/**
 * @file test_advanced.c
 * @brief Advanced harness tests — multi-display window modes via Situation API only.
 *
 * Uses the normal Situation frame loop (AcquireFrameCommandBuffer → render passes → EndFrame).
 * One host window spans the virtual desktop; one Virtual Display per monitor carries content.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>

#define SIT_ADV_MAX_DISPLAYS 8

typedef struct SitAdvDesktop {
    int x;
    int y;
    int w;
    int h;
} SitAdvDesktop;

typedef struct SitAdvDisplayPanel {
    int vd_id;
    int monitor_id;
    float draw_w;
    float draw_h;
} SitAdvDisplayPanel;

static bool g_advanced_init_ok = false;
static SitAdvDisplayPanel g_adv_panels[SIT_ADV_MAX_DISPLAYS];
static int g_adv_panel_count = 0;
static int g_adv_primary_monitor = 0;

static void advanced_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_ADVANCED");

    SituationError err = SituationInit(0, NULL, &config);
    g_advanced_init_ok = (err == SITUATION_SUCCESS);
    if (!g_advanced_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void advanced_destroy_panels(void) {
    for (int i = 0; i < g_adv_panel_count; i++) {
        if (g_adv_panels[i].vd_id >= 0) {
            SituationDestroyVirtualDisplay(g_adv_panels[i].vd_id);
            g_adv_panels[i].vd_id = -1;
        }
    }
    g_adv_panel_count = 0;
}

static void advanced_teardown(void) {
    if (g_advanced_init_ok) {
        advanced_destroy_panels();
        if (SituationIsWindowFullscreen()) {
            SituationToggleFullscreen();
            SituationPollInputEvents();
        }
        SituationClearWindowState(SITUATION_FLAG_WINDOW_UNDECORATED);
        SituationPollInputEvents();
        SituationDisplayMode mode = {0};
        mode.width = SIT_TEST_WINDOW_WIDTH;
        mode.height = SIT_TEST_WINDOW_HEIGHT;
        SituationSetDisplayMode(g_adv_primary_monitor, &mode, false);
        SituationPollInputEvents();
        SituationShutdown();
        g_advanced_init_ok = false;
    }
}

static SitAdvDesktop advanced_desktop_bounds(void) {
    SitAdvDesktop desktop = {0};
    const int count = SituationGetMonitorCount();
    if (count <= 0) {
        desktop.w = SIT_TEST_WINDOW_WIDTH;
        desktop.h = SIT_TEST_WINDOW_HEIGHT;
        return desktop;
    }

    int min_x = INT_MAX;
    int min_y = INT_MAX;
    int max_x = INT_MIN;
    int max_y = INT_MIN;

    for (int i = 0; i < count; i++) {
        Vector2 pos = SituationGetMonitorPosition(i);
        const int mw = SituationGetMonitorWidth(i);
        const int mh = SituationGetMonitorHeight(i);
        const int mx = (int)pos.x;
        const int my = (int)pos.y;
        if (mx < min_x) {
            min_x = mx;
        }
        if (my < min_y) {
            min_y = my;
        }
        if (mx + mw > max_x) {
            max_x = mx + mw;
        }
        if (my + mh > max_y) {
            max_y = my + mh;
        }
    }

    desktop.x = min_x;
    desktop.y = min_y;
    desktop.w = max_x - min_x;
    desktop.h = max_y - min_y;
    if (desktop.w < SIT_TEST_WINDOW_WIDTH) {
        desktop.w = SIT_TEST_WINDOW_WIDTH;
    }
    if (desktop.h < SIT_TEST_WINDOW_HEIGHT) {
        desktop.h = SIT_TEST_WINDOW_HEIGHT;
    }
    return desktop;
}

static int advanced_primary_monitor_id(const SituationDisplayInfo* displays, int count) {
    for (int i = 0; i < count; i++) {
        if (displays[i].is_primary) {
            return displays[i].situation_monitor_id;
        }
    }
    return (count > 0) ? displays[0].situation_monitor_id : 0;
}

static void advanced_apply_host_window(const SitAdvDesktop* desktop, bool undecorated) {
    SIT_ASSERT_NOT_NULL(desktop);

    if (SituationIsWindowFullscreen()) {
        SituationToggleFullscreen();
        SituationPollInputEvents();
    }

    if (undecorated) {
        SituationSetWindowState(SITUATION_FLAG_WINDOW_UNDECORATED);
    } else {
        SituationClearWindowState(SITUATION_FLAG_WINDOW_UNDECORATED);
    }

    SituationSetWindowMaxSize(desktop->w + 64, desktop->h + 64);
    SituationSetWindowSize(desktop->w, desktop->h);
    SituationSetWindowPosition(desktop->x, desktop->y);
    SituationPollInputEvents();
}

static bool advanced_create_panels(
    const SituationDisplayInfo* displays,
    int display_count,
    const SitAdvDesktop* desktop,
    bool fullscreen_panels)
{
    advanced_destroy_panels();
    if (!displays || display_count <= 0 || !desktop) {
        return false;
    }

    for (int i = 0; i < display_count && g_adv_panel_count < SIT_ADV_MAX_DISPLAYS; i++) {
        const int mid = displays[i].situation_monitor_id;
        Vector2 pos = SituationGetMonitorPosition(mid);
        const int mw = SituationGetMonitorWidth(mid);
        const int mh = SituationGetMonitorHeight(mid);

        float panel_w = (float)SIT_TEST_WINDOW_WIDTH;
        float panel_h = (float)SIT_TEST_WINDOW_HEIGHT;
        float ox = pos.x - (float)desktop->x + ((float)mw - panel_w) * 0.5f;
        float oy = pos.y - (float)desktop->y + ((float)mh - panel_h) * 0.5f;

        if (fullscreen_panels) {
            panel_w = (float)mw;
            panel_h = (float)mh;
            ox = pos.x - (float)desktop->x;
            oy = pos.y - (float)desktop->y;
        }

        Vector2 resolution = {panel_w, panel_h};
        int vd_id = -1;
        SituationError err = SituationCreateVirtualDisplay(
            resolution,
            1.0,
            i,
            SITUATION_SCALING_STRETCH,
            SITUATION_BLEND_NONE,
            &vd_id);
        if (err != SITUATION_SUCCESS || vd_id < 0) {
            return false;
        }

        Vector2 offset = {{ox, oy}};
        err = SituationConfigureVirtualDisplay(
            vd_id, offset, 1.0f, i, true, 1.0, SITUATION_BLEND_NONE);
        if (err != SITUATION_SUCCESS) {
            SituationDestroyVirtualDisplay(vd_id);
            return false;
        }

        g_adv_panels[g_adv_panel_count].vd_id = vd_id;
        g_adv_panels[g_adv_panel_count].monitor_id = mid;
        g_adv_panels[g_adv_panel_count].draw_w = panel_w;
        g_adv_panels[g_adv_panel_count].draw_h = panel_h;
        g_adv_panel_count++;
    }

    return g_adv_panel_count == display_count;
}

static void advanced_draw_triangles_in_panel(
    SituationCommandBuffer cmd,
    const SitAdvDisplayPanel* panel)
{
    const float w = panel->draw_w;
    const float h = panel->draw_h;
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float t = (float)SituationTimerGetTime();
    static const Vector4 colors[3] = {
        {{1.0f, 0.25f, 0.2f, 1.0f}},
        {{0.25f, 1.0f, 0.35f, 1.0f}},
        {{0.35f, 0.55f, 1.0f, 1.0f}},
    };

    for (int i = 0; i < 3; i++) {
        const float angle = t * 0.35f + (float)i * (6.2831853f / 3.0f);
        const float orbit = 95.0f + 22.0f * sinf(t * 0.42f);
        const float px = cx + cosf(angle) * orbit;
        const float py = cy + sinf(angle) * orbit;

        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){px, py, 0.0f});
        glm_rotate(model, t * 0.22f + (float)i, (vec3){0.0f, 0.0f, 1.0f});
        glm_scale(model, (vec3){42.0f, 42.0f, 1.0f});
        glm_translate(model, (vec3){-0.5f, -0.5f, 0.0f});
        SituationCmdDrawQuad(cmd, model, colors[i]);
    }
}

static bool advanced_draw_frame(void) {
    if (g_adv_panel_count <= 0) {
        return false;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    if (!SituationAcquireFrameCommandBuffer()) {
        return false;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return false;
    }

    for (int i = 0; i < g_adv_panel_count; i++) {
        SituationRenderPassInfo rp = {0};
        rp.display_id = g_adv_panels[i].vd_id;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = (ColorRGBA){12, 14, 28, 255};
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.clear.depth = 1.0f;

        if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
            return false;
        }
        advanced_draw_triangles_in_panel(cmd, &g_adv_panels[i]);
        if (SituationCmdEndRenderPass(cmd) != SITUATION_SUCCESS) {
            return false;
        }
    }

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;

    if (SituationCmdBeginRenderPass(cmd, &main_rp) != SITUATION_SUCCESS) {
        return false;
    }
    if (SituationRenderVirtualDisplays(cmd) != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return false;
    }
    if (SituationCmdEndRenderPass(cmd) != SITUATION_SUCCESS) {
        return false;
    }
    return SituationEndFrame() == SITUATION_SUCCESS;
}

static int advanced_animate(double seconds) {
    const double start = SituationTimerGetTime();
    int ok_frames = 0;
    while (SituationTimerGetTime() - start < seconds) {
        if (advanced_draw_frame()) {
            ok_frames++;
        }
    }
    return ok_frames;
}

/**
 * All monitors show 1024×768 panels at once (2 s), desktop-span fullscreen (2 s), windowed again (2 s).
 */
static void test_all_displays_windowed_fullscreen_cycle(void) {
    SituationDisplayInfo* displays = NULL;
    int display_count = 0;
    SituationError err = SituationGetDisplays(&displays, &display_count);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(displays);
    SIT_ASSERT(display_count >= 1);

    g_adv_primary_monitor = advanced_primary_monitor_id(displays, display_count);
    const SitAdvDesktop desktop = advanced_desktop_bounds();

    /* Phase 1 — decorated host spans the virtual desktop; one VD per monitor (1024×768). */
    advanced_apply_host_window(&desktop, false);
    SIT_ASSERT(advanced_create_panels(displays, display_count, &desktop, false));
    SIT_ASSERT_EQ(g_adv_panel_count, display_count);
    SIT_ASSERT(!SituationIsWindowFullscreen());

    int ok_frames = advanced_animate(2.0);

    /* Phase 2 — borderless host + panels fill each monitor (desktop-wide fullscreen). */
    advanced_apply_host_window(&desktop, true);
    SIT_ASSERT(advanced_create_panels(displays, display_count, &desktop, true));
    SIT_ASSERT_EQ(g_adv_panel_count, display_count);
    ok_frames += advanced_animate(2.0);

    /* Phase 3 — back to windowed panels on every monitor. */
    advanced_apply_host_window(&desktop, false);
    SIT_ASSERT(advanced_create_panels(displays, display_count, &desktop, false));
    SIT_ASSERT_EQ(g_adv_panel_count, display_count);
    SIT_ASSERT(!SituationIsWindowFullscreen());
    ok_frames += advanced_animate(2.0);

    advanced_destroy_panels();
    SituationDisplayMode mode = {0};
    mode.width = SIT_TEST_WINDOW_WIDTH;
    mode.height = SIT_TEST_WINDOW_HEIGHT;
    SIT_ASSERT_EQ(SituationSetDisplayMode(g_adv_primary_monitor, &mode, false), SITUATION_SUCCESS);
    SituationPollInputEvents();

    SituationFreeDisplays(displays, display_count);

    SIT_ASSERT(ok_frames > 24);
    fprintf(stderr,
            "  [advanced] %d monitor panel(s), host %dx%d@%d,%d, %d frames\n",
            display_count,
            desktop.w,
            desktop.h,
            desktop.x,
            desktop.y,
            ok_frames);
}

static SitTestCase g_advanced_tests[] = {
    {"all_displays_windowed_fullscreen_cycle", test_all_displays_windowed_fullscreen_cycle, true},
};

const SitTestModule g_module_advanced = {
    .name = "advanced",
    .setup = advanced_setup,
    .teardown = advanced_teardown,
    .tests = g_advanced_tests,
    .test_count = (int)(sizeof(g_advanced_tests) / sizeof(g_advanced_tests[0])),
    .requires_context = true,
    .requires_visible_window = true,
};
