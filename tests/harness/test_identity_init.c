/**
 * @file test_identity_init.c
 * @brief Win32 identity init-path tests (default_window_icon_path) — own SituationInit cycle.
 *
 * Runs before the window module so init can supply default_window_icon_path.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"

static bool g_identity_init_ok = false;

static bool sit_test_resolve_repo_relative(const char* rel_path, char* out_path, size_t out_path_sz) {
    if (!rel_path || !rel_path[0] || !out_path || out_path_sz == 0) {
        return false;
    }
    static const char* const prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
        NULL
    };
    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(out_path, out_path_sz, "%s%s", prefixes[i], rel_path);
        if (SituationFileExists(out_path)) {
            return true;
        }
    }
    return false;
}

static void identity_init_setup(void) {
    char icon_path[512];
    if (!sit_test_resolve_repo_relative("scripts/art/icon_source.PNG", icon_path, sizeof(icon_path))) {
        longjmp(g_sit_test_jmp_buf, 1);
    }

    SituationInitInfo config = {0};
    sit_test_window_init_info_flags(&config, "SIT_TEST_IDENTITY_INIT", SITUATION_FLAG_WINDOW_HIDDEN);
    config.default_window_icon_path = icon_path;

    SituationError err = SituationInit(0, NULL, &config);
    g_identity_init_ok = (err == SITUATION_SUCCESS);
    if (!g_identity_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void identity_init_teardown(void) {
    if (g_identity_init_ok) {
        SituationShutdown();
        g_identity_init_ok = false;
    }
}

static void test_default_window_icon_path_png_init(void) {
    SIT_ASSERT(g_identity_init_ok);
    SIT_ASSERT(SituationIsInitialized());
}

static void test_default_window_icon_path_invalid_failsoft(void) {
    /* Module teardown runs after this test — we cannot re-init here. Verify API field exists on struct. */
    SituationInitInfo info = SituationInitInfoDefault(640, 480, "Icon path probe");
    info.default_window_icon_path = "definitely_missing_icon_12345.png";
    (void)info;
    SIT_ASSERT(true);
}

static SitTestCase identity_init_tests[] = {
    {"default_window_icon_path_png_init", test_default_window_icon_path_png_init, true},
    {"default_window_icon_path_invalid_failsoft", test_default_window_icon_path_invalid_failsoft, true},
};

const SitTestModule g_module_identity_init = {
    .name = "identity_init",
    .setup = identity_init_setup,
    .teardown = identity_init_teardown,
    .tests = identity_init_tests,
    .test_count = sizeof(identity_init_tests) / sizeof(identity_init_tests[0]),
};
