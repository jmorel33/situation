/**
 * @file sit_test_output_color_depth.h
 * @brief Shared output color depth harness assertions (Phase 4–6).
 */

#ifndef SIT_TEST_OUTPUT_COLOR_DEPTH_H
#define SIT_TEST_OUTPUT_COLOR_DEPTH_H

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"

/** Caps + feature flags must agree (HDR10 vs 10-bit SDR vs 8-bit). */
static inline void sit_test_assert_output_color_depth_consistent(void) {
    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    const bool hdr_active = caps.output_hdr_active != 0u;
    const bool sdr10_active = caps.output_color_depth_active != 0u && !hdr_active;
    const bool hdr_feature = SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT) != 0;
    const bool sdr10_feature = SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT) != 0;

    SIT_ASSERT(hdr_active == hdr_feature);
    SIT_ASSERT(sdr10_active == sdr10_feature);

    if (hdr_active) {
        SIT_ASSERT(caps.output_bits_per_channel == 10u);
        SIT_ASSERT(caps.output_color_space == (uint8_t)SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084);
        SIT_ASSERT(!sdr10_feature);
    } else if (sdr10_active) {
        SIT_ASSERT(caps.output_bits_per_channel == 10u);
        SIT_ASSERT(caps.output_color_space == (uint8_t)SIT_OUTPUT_COLOR_SPACE_SDR_SRGB);
        SIT_ASSERT(!hdr_feature);
    } else {
        SIT_ASSERT(caps.output_bits_per_channel == 8u);
        SIT_ASSERT(!hdr_feature);
        SIT_ASSERT(!sdr10_feature);
    }
}

static inline const char* sit_test_output_color_policy_name(SituationOutputColorDepth policy) {
    switch (policy) {
    case SIT_OUTPUT_COLOR_8BIT: return "SIT_OUTPUT_COLOR_8BIT";
    case SIT_OUTPUT_COLOR_10BIT: return "SIT_OUTPUT_COLOR_10BIT";
    case SIT_OUTPUT_COLOR_HDR10: return "SIT_OUTPUT_COLOR_HDR10";
    case SIT_OUTPUT_COLOR_AUTO: return "SIT_OUTPUT_COLOR_AUTO";
    default: return "SIT_OUTPUT_COLOR_UNKNOWN";
    }
}

static inline const char* sit_test_output_color_space_name(uint8_t space) {
    switch ((SituationOutputColorSpace)space) {
    case SIT_OUTPUT_COLOR_SPACE_SDR_SRGB: return "SDR_SRGB";
    case SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084: return "HDR10_ST2084";
    default: return "UNKNOWN";
    }
}

/** True when DXGI reports HDR enabled on the window monitor and WSI exposes HDR10. */
static inline bool sit_test_hdr_os_and_wsi_ready(void) {
    SituationDisplayInfo* displays = NULL;
    int display_count = 0;
    if (SituationGetDisplays(&displays, &display_count) != SITUATION_SUCCESS || !displays) {
        return false;
    }

    bool os_hdr = false;
    const int mid = SituationGetCurrentMonitor();
    if (mid >= 0 && mid < display_count) {
        const SituationDisplayInfo* wm = &displays[mid];
        os_hdr = wm->dxgi_metadata_valid && wm->hdr_enabled;
    }
    SituationFreeDisplays(displays, display_count);

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    return os_hdr && caps.wsi_supports_hdr10;
}

static inline void sit_test_skip_unless_hdr_opt_in(void) {
    if (!sit_test_hdr_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_HDR=1 for HDR verification tests");
    }
}

/** Skip when OS/WSI cannot do HDR10; hard-fail when prerequisites met but swapchain is not HDR. */
static inline void sit_test_require_hdr10_active(void) {
    sit_test_skip_unless_hdr_opt_in();
    if (!sit_test_hdr_os_and_wsi_ready()) {
        SIT_TEST_SKIP("enable Windows Use HDR on the window monitor and ensure WSI HDR10 pair (re-run with SIT_TEST_HDR=1)");
    }

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    sit_test_assert_output_color_depth_consistent();

    if (!caps.output_hdr_active) {
        fprintf(stderr,
            "[output_color_depth] HDR10 prerequisites met but output_hdr_active=0 — check init policy / driver\n");
    }
    SIT_ASSERT(caps.output_hdr_active);
    SIT_ASSERT(caps.output_color_space == (uint8_t)SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084);
    SIT_ASSERT(SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT));
    SIT_ASSERT(caps.output_bits_per_channel == 10u);
    SIT_ASSERT(caps.output_color_depth_active);
}

/** Move the test window onto the first DXGI HDR-enabled monitor and nudge resize (swapchain format re-pick). */
static inline void sit_test_position_window_on_hdr_monitor(void) {
#if defined(_WIN32)
    SituationDisplayInfo* displays = NULL;
    int count = 0;
    if (SituationGetDisplays(&displays, &count) != SITUATION_SUCCESS || !displays) {
        return;
    }

    int target = -1;
    for (int i = 0; i < count; ++i) {
        if (displays[i].dxgi_metadata_valid && displays[i].hdr_enabled) {
            target = i;
            break;
        }
    }
    if (target < 0) {
        SituationFreeDisplays(displays, count);
        return;
    }

    if (SituationGetCurrentMonitor() != target) {
        SituationSetWindowMonitor(target);
        SituationPollInputEvents();
    } else {
        Vector2 pos = SituationGetMonitorPosition(target);
        const int mw = SituationGetMonitorWidth(target);
        const int mh = SituationGetMonitorHeight(target);
        SituationSetWindowSize(SIT_TEST_WINDOW_WIDTH, SIT_TEST_WINDOW_HEIGHT);
        SituationSetWindowPosition((int)pos.x + mw / 4, (int)pos.y + mh / 4);
        SituationPollInputEvents();
    }

    /* Recreate swapchain so HDR format pick sees the window monitor's DXGI state. */
    SituationSetWindowSize(SIT_TEST_WINDOW_WIDTH + 2, SIT_TEST_WINDOW_HEIGHT);
    SituationPollInputEvents();
    SituationSetWindowSize(SIT_TEST_WINDOW_WIDTH, SIT_TEST_WINDOW_HEIGHT);
    SituationPollInputEvents();

    fprintf(stderr,
        "[output_color_depth] HDR test window on monitor[%d] \"%s\" (SituationGetCurrentMonitor=%d)\n",
        target,
        displays[target].name,
        SituationGetCurrentMonitor());

    SituationFreeDisplays(displays, count);
#else
    (void)0;
#endif
}

#endif /* SIT_TEST_OUTPUT_COLOR_DEPTH_H */
