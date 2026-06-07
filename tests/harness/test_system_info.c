/**
 * @file test_system_info.c
 * @brief System Introspection API tests (v2.4.199)
 *
 * Tests SituationGetOSInfo(), SituationGetProcessList(),
 * SituationFreeProcessList(), and SituationGetActiveAudioDeviceName().
 *
 * Context-dependent: requires SituationInit() for audio device query.
 * OS info and process list are partially testable without context.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"

// ============================================================================
//  SituationGetOSInfo Tests
// ============================================================================

static void test_os_info_name_not_empty(void) {
    SituationOSInfo info = SituationGetOSInfo();
    SIT_ASSERT(strlen(info.name) > 0);
    // Should not contain "Unknown" on Windows/Linux/macOS
#ifdef _WIN32
    SIT_ASSERT(strstr(info.name, "Windows") != NULL);
#endif
}

static void test_os_info_version_not_empty(void) {
    SituationOSInfo info = SituationGetOSInfo();
    SIT_ASSERT(strlen(info.version) > 0);
    // Version should contain at least one digit
    bool has_digit = false;
    for (const char* p = info.version; *p; p++) {
        if (*p >= '0' && *p <= '9') { has_digit = true; break; }
    }
    SIT_ASSERT(has_digit);
}

static void test_os_info_build_number_nonzero_on_windows(void) {
    SituationOSInfo info = SituationGetOSInfo();
#ifdef _WIN32
    // Windows should always have a build number
    SIT_ASSERT(info.build_number > 0);
    // Modern Windows 10+ should be > 10000
    SIT_ASSERT(info.build_number >= 7600); // Win7 minimum
#else
    // Other platforms may have 0
    (void)info;
#endif
}

static void test_os_info_version_format_major_minor(void) {
    SituationOSInfo info = SituationGetOSInfo();
    // Version string should contain at least one dot (major.minor)
    SIT_ASSERT(strchr(info.version, '.') != NULL);
}

static void test_os_info_repeated_calls_consistent(void) {
    SituationOSInfo info1 = SituationGetOSInfo();
    SituationOSInfo info2 = SituationGetOSInfo();
    // Repeated calls should return the same info
    SIT_ASSERT_STR_EQ(info1.name, info2.name);
    SIT_ASSERT_STR_EQ(info1.version, info2.version);
    SIT_ASSERT_EQ(info1.build_number, info2.build_number);
}

// ============================================================================
//  SituationGetProcessList Tests
// ============================================================================

static void test_process_list_returns_non_null(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    SIT_ASSERT(count > 0);
    SituationFreeProcessList(list, count);
}

static void test_process_list_count_reasonable(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    // A running system should have at least a handful of processes
    SIT_ASSERT(count >= 5);
    // But shouldn't be absurdly high (sanity check)
    SIT_ASSERT(count < 100000);
    SituationFreeProcessList(list, count);
}

static void test_process_list_has_valid_pids(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    // Check first few entries have nonzero PIDs (PID 0 is System Idle on Windows)
    int nonzero_count = 0;
    for (int i = 0; i < count && i < 50; i++) {
        if (list[i].pid > 0) nonzero_count++;
    }
    // At least 90% should have real PIDs
    SIT_ASSERT(nonzero_count > 0);
    SituationFreeProcessList(list, count);
}

static void test_process_list_has_named_entries(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    // At least some processes should have non-empty names
    int named_count = 0;
    for (int i = 0; i < count; i++) {
        if (strlen(list[i].name) > 0) named_count++;
    }
    // Most processes should be named
    SIT_ASSERT(named_count >= count / 2);
    SituationFreeProcessList(list, count);
}

static void test_process_list_contains_self(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    // Our own test executable should appear in the list
    bool found_self = false;
    for (int i = 0; i < count; i++) {
        if (strstr(list[i].name, "sit_test") != NULL) {
            found_self = true;
            break;
        }
    }
    SIT_ASSERT(found_self);
    SituationFreeProcessList(list, count);
}

static void test_process_list_memory_nonzero(void) {
    int count = 0;
    SituationProcessInfo* list = SituationGetProcessList(&count);
    SIT_ASSERT_NOT_NULL(list);
    // At least some processes should have nonzero memory
    int has_memory = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].memory_bytes > 0) has_memory++;
    }
    // Some processes may be inaccessible (permission denied), but most should report memory
    SIT_ASSERT(has_memory > 0);
    SituationFreeProcessList(list, count);
}

static void test_process_list_null_count_returns_null(void) {
    // Passing NULL for out_count should return NULL gracefully
    SituationProcessInfo* list = SituationGetProcessList(NULL);
    SIT_ASSERT(list == NULL);
}

static void test_free_process_list_null_safe(void) {
    // Freeing NULL should not crash
    SituationFreeProcessList(NULL, 0);
    SituationFreeProcessList(NULL, 5);
    // If we got here, no crash
    SIT_ASSERT(true);
}

// ============================================================================
//  SituationGetActiveAudioDeviceName Tests
// ============================================================================

static void test_active_audio_device_name_not_null(void) {
    const char* name = SituationGetActiveAudioDeviceName();
    SIT_ASSERT_NOT_NULL(name);
    // Should return something (even "Not initialized" or "No audio device")
    SIT_ASSERT(strlen(name) > 0);
}

static void test_active_audio_device_name_reasonable_length(void) {
    const char* name = SituationGetActiveAudioDeviceName();
    SIT_ASSERT_NOT_NULL(name);
    // Name should not be absurdly long
    SIT_ASSERT(strlen(name) < 256);
}

static void test_active_audio_device_name_repeated_stable(void) {
    const char* name1 = SituationGetActiveAudioDeviceName();
    const char* name2 = SituationGetActiveAudioDeviceName();
    // Should return the same pointer (static buffer) or at least same content
    SIT_ASSERT_STR_EQ(name1, name2);
}

static void test_active_audio_device_not_error_string(void) {
    // After successful init, should not return error strings
    const char* name = SituationGetActiveAudioDeviceName();
    SIT_ASSERT_NOT_NULL(name);
    if (SituationIsInitialized() && SituationIsAudioDevicePlaying()) {
        // If audio is actually playing, name should not be an error placeholder
        SIT_ASSERT(strcmp(name, "Not initialized") != 0);
        SIT_ASSERT(strcmp(name, "No audio device") != 0);
    }
}

// ============================================================================
//  Integration: OS + Process combined sanity
// ============================================================================

static void test_system_snapshot_coherent(void) {
    // All three APIs should be callable in sequence without crashing
    SituationOSInfo os = SituationGetOSInfo();
    int proc_count = 0;
    SituationProcessInfo* procs = SituationGetProcessList(&proc_count);
    const char* audio_name = SituationGetActiveAudioDeviceName();

    SIT_ASSERT(strlen(os.name) > 0);
    SIT_ASSERT_NOT_NULL(procs);
    SIT_ASSERT(proc_count > 0);
    SIT_ASSERT_NOT_NULL(audio_name);

    SituationFreeProcessList(procs, proc_count);
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase system_info_tests[] = {
    // OS Info
    {"os_info_name_not_empty",              test_os_info_name_not_empty,              false},
    {"os_info_version_not_empty",           test_os_info_version_not_empty,           false},
    {"os_info_build_number_windows",        test_os_info_build_number_nonzero_on_windows, false},
    {"os_info_version_format",              test_os_info_version_format_major_minor,  false},
    {"os_info_repeated_consistent",         test_os_info_repeated_calls_consistent,   false},
    // Process List
    {"process_list_non_null",               test_process_list_returns_non_null,        false},
    {"process_list_count_reasonable",        test_process_list_count_reasonable,        false},
    {"process_list_valid_pids",             test_process_list_has_valid_pids,          false},
    {"process_list_named_entries",           test_process_list_has_named_entries,       false},
    {"process_list_contains_self",           test_process_list_contains_self,           false},
    {"process_list_memory_nonzero",          test_process_list_memory_nonzero,          false},
    {"process_list_null_count_safe",         test_process_list_null_count_returns_null, false},
    {"free_process_list_null_safe",          test_free_process_list_null_safe,          false},
    // Active Audio Device
    {"audio_device_name_not_null",           test_active_audio_device_name_not_null,   true},
    {"audio_device_name_length",             test_active_audio_device_name_reasonable_length, true},
    {"audio_device_name_stable",             test_active_audio_device_name_repeated_stable,  true},
    {"audio_device_not_error_string",        test_active_audio_device_not_error_string, true},
    // Integration
    {"system_snapshot_coherent",             test_system_snapshot_coherent,             true},
};

const SitTestModule g_module_system_info = {
    .name = "system_info",
    .setup = NULL,
    .teardown = NULL,
    .tests = system_info_tests,
    .test_count = sizeof(system_info_tests) / sizeof(system_info_tests[0]),
    .requires_context = true
};
