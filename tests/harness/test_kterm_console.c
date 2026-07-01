/**
 * @file test_kterm_console.c
 * @brief KaOS Terminal (kterm_console) integration smoke tests
 *
 * Spawns build/examples/kterm_console.exe with KTERM_CAPTURE_* env vars.
 * Skips gracefully when the example binary is not built.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_test_framework.h"
#include "sit_api_include.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

static bool kterm_file_exists(const char* path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(path, F_OK) == 0;
#endif
}

static bool kterm_resolve_console_exe(char* out, size_t out_size) {
    const char* env = getenv("KTERM_CONSOLE_EXE");
    if (env && env[0] && kterm_file_exists(env)) {
        strncpy(out, env, out_size - 1);
        out[out_size - 1] = '\0';
        return true;
    }

    static const char* candidates[] = {
        "build/examples/kterm_console.exe",
        "build\\examples\\kterm_console.exe",
        "../build/examples/kterm_console.exe",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (kterm_file_exists(candidates[i])) {
            strncpy(out, candidates[i], out_size - 1);
            out[out_size - 1] = '\0';
            return true;
        }
    }
    return false;
}

static long kterm_file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    fclose(f);
    return size;
}

static void test_kterm_capture_screenshot_exit(void) {
#ifndef _WIN32
    fprintf(stderr, "[kterm_console] capture test skipped (Windows spawn only)\n");
    SIT_ASSERT(true);
    return;
#else
    char exe_path[512];
    if (!kterm_resolve_console_exe(exe_path, sizeof(exe_path))) {
        fprintf(stderr, "[kterm_console] capture test skipped (kterm_console.exe not found; build with build_examples.bat opengl kterm_console)\n");
        SIT_ASSERT(true);
        return;
    }

    const char* capture_path = "build/kterm_capture_harness";
    _mkdir("build");

    // Clean up any leftover file from previous run
    char cleanup_path[600];
    _snprintf_s(cleanup_path, sizeof(cleanup_path), _TRUNCATE, "%s%s",
                capture_path, sit_screenshot_format_ext[SIT_SCREENSHOT_BMP]);
    DeleteFileA(cleanup_path);

    char prev_capture[1024] = {0};
    char prev_exit[16] = {0};
    char* old_capture = getenv("KTERM_CAPTURE_SCREENSHOT");
    char* old_exit = getenv("KTERM_CAPTURE_EXIT");
    if (old_capture) {
        strncpy(prev_capture, old_capture, sizeof(prev_capture) - 1);
    }
    if (old_exit) {
        strncpy(prev_exit, old_exit, sizeof(prev_exit) - 1);
    }

    _putenv_s("KTERM_CAPTURE_SCREENSHOT", capture_path);
    _putenv_s("KTERM_CAPTURE_EXIT", "1");

    char dll_path[512];
    _snprintf_s(dll_path, sizeof(dll_path), _TRUNCATE, "build\\dll;%s", getenv("PATH") ? getenv("PATH") : "");
    _putenv_s("PATH", dll_path);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    char cmdline[640];
    _snprintf_s(cmdline, sizeof(cmdline), _TRUNCATE, "\"%s\"", exe_path);

    BOOL created = CreateProcessA(
        exe_path,
        cmdline,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi);

    if (!created) {
        _putenv_s("KTERM_CAPTURE_SCREENSHOT", prev_capture[0] ? prev_capture : "");
        _putenv_s("KTERM_CAPTURE_EXIT", prev_exit[0] ? prev_exit : "");
        fprintf(stderr, "[kterm_console] CreateProcess failed (%lu)\n", GetLastError());
        SIT_ASSERT(false);
        return;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, 60000);
    DWORD exit_code = 1;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    _putenv_s("KTERM_CAPTURE_SCREENSHOT", prev_capture[0] ? prev_capture : "");
    _putenv_s("KTERM_CAPTURE_EXIT", prev_exit[0] ? prev_exit : "");

    SIT_ASSERT(wait == WAIT_OBJECT_0);
    SIT_ASSERT(exit_code == 0);

    // The file gets the format extension appended (default: .bmp)
    char output_path[600];
    _snprintf_s(output_path, sizeof(output_path), _TRUNCATE, "%s%s",
                capture_path, sit_screenshot_format_ext[SIT_SCREENSHOT_BMP]);
    SIT_ASSERT(kterm_file_exists(output_path));

    long file_size = kterm_file_size(output_path);
    SIT_ASSERT(file_size > 64);

    DeleteFileA(output_path);
#endif
}

static SitTestCase kterm_console_tests[] = {
    {"capture_screenshot_exit", test_kterm_capture_screenshot_exit, false},
};

const SitTestModule g_module_kterm_console = {
    .name = "kterm_console",
    .setup = NULL,
    .teardown = NULL,
    .tests = kterm_console_tests,
    .test_count = sizeof(kterm_console_tests) / sizeof(kterm_console_tests[0]),
    .requires_context = false,
};
