/**
 * @file sit_test_visual_layout.h
 * @brief Render-pixel layout for harness overlays (listen tests, scope, spectrum).
 *
 * Harness overlay code must use SituationGetRenderWidth/Height — not logical
 * SituationGetScreenWidth/Height — so panel positions match the framebuffer on Hi-DPI.
 * This is a test-harness layout rule, not a renderer API requirement.
 */

#ifndef SIT_TEST_VISUAL_LAYOUT_H
#define SIT_TEST_VISUAL_LAYOUT_H

#include "sit_api_include.h"
#include "sit_test_stereo_scope.h"
#include "sit_test_window.h"

typedef struct SitTestVisualLayout {
    float w;
    float h;
    float scale; /* render / logical, 1.0 when no Hi-DPI */
    float header_h;
    float scope_y;
    float scope_h;
    float spec_y;
    float spec_h;
    float margin;
} SitTestVisualLayout;

static inline SitTestVisualLayout sit_test_visual_layout_compute(float header_logical_px) {
    SitTestVisualLayout L = {0};
    const float sw = (float)SituationGetScreenWidth();
    const float sh = (float)SituationGetScreenHeight();
    float rw = (float)SituationGetRenderWidth();
    float rh = (float)SituationGetRenderHeight();
    if (rw < 1.0f) {
        rw = sw > 0.0f ? sw : (float)SIT_TEST_WINDOW_WIDTH;
    }
    if (rh < 1.0f) {
        rh = sh > 0.0f ? sh : (float)SIT_TEST_WINDOW_HEIGHT;
    }
    L.w = rw;
    L.h = rh;
    L.scale = (sh > 0.0f) ? (rh / sh) : 1.0f;
    L.margin = 12.0f * L.scale;
    L.header_h = header_logical_px * L.scale;
    L.spec_h = SIT_TEST_SPECTRUM_PANEL_H * L.scale;
    L.spec_y = rh - L.spec_h - 8.0f * L.scale;
    L.scope_y = L.header_h;
    L.scope_h = L.spec_y - L.scope_y - 8.0f * L.scale;
    if (L.scope_h < 64.0f * L.scale) {
        L.scope_h = 64.0f * L.scale;
    }
    return L;
}

/** Scale a logical-pixel Y (authored for 768p window) into render pixels. */
static inline float sit_test_visual_y(float logical_y, float scale) {
    return logical_y * scale;
}

#endif /* SIT_TEST_VISUAL_LAYOUT_H */
