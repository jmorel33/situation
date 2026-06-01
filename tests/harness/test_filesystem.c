/**
 * @file test_filesystem.c
 * @brief Filesystem module tests — Path utilities, File I/O, Directory operations
 *
 * Context-free: does NOT require SituationInit().
 * All temp files use the "_sit_test_" prefix for safe cleanup.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"

// ============================================================================
//  Cleanup helper
// ============================================================================

static void filesystem_teardown(void) {
    (void)0; /* Tests delete their own artifacts; module teardown was crashing after async I/O (v2.4.103 IO-queue fix). */
}

// ============================================================================
//  Path Utility Tests
// ============================================================================

static void test_get_base_path(void) {
    char* path = SituationGetBasePath();
    SIT_ASSERT_NOT_NULL(path);
    // Base path should be non-empty
    SIT_ASSERT(strlen(path) > 0);
    SituationFreeString(path);
}

static void test_get_file_name(void) {
    const char* name = SituationGetFileName("some/dir/file.txt");
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT_STR_EQ(name, "file.txt");
}

static void test_get_file_name_no_dir(void) {
    const char* name = SituationGetFileName("file.txt");
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT_STR_EQ(name, "file.txt");
}

static void test_get_file_extension(void) {
    const char* ext = SituationGetFileExtension("file.txt");
    SIT_ASSERT_NOT_NULL(ext);
    // Could be ".txt" or "txt" depending on implementation
    SIT_ASSERT(strstr(ext, "txt") != NULL);
}

static void test_get_file_extension_none(void) {
    const char* ext = SituationGetFileExtension("noextension");
    // Should return NULL or empty string for no extension
    if (ext != NULL) {
        SIT_ASSERT(strlen(ext) == 0 || ext[0] == '\0');
    }
}

static void test_join_path(void) {
    char* joined = SituationJoinPath("base", "file.txt");
    SIT_ASSERT_NOT_NULL(joined);
    // Should contain both components
    SIT_ASSERT(strstr(joined, "base") != NULL);
    SIT_ASSERT(strstr(joined, "file.txt") != NULL);
    SituationFreeString(joined);
}

static void test_get_app_save_path(void) {
    char* path = SituationGetAppSavePath("sit_test_app");
    SIT_ASSERT_NOT_NULL(path);
    SIT_ASSERT(strlen(path) > 0);
    SituationFreeString(path);
}

// ============================================================================
//  File Query Tests
// ============================================================================

static void test_file_exists_positive(void) {
    // Create a temp file first
    bool ok = SituationSaveFileText("_sit_test_text.txt", "hello");
    SIT_ASSERT(ok);
    SIT_ASSERT(SituationFileExists("_sit_test_text.txt"));
    SituationDeleteFile("_sit_test_text.txt");
}

static void test_file_exists_negative(void) {
    SIT_ASSERT(!SituationFileExists("_sit_test_nonexistent_xyz_12345.txt"));
}

static void test_directory_exists(void) {
    bool ok = SituationCreateDirectory("_sit_test_dir", false);
    SIT_ASSERT(ok);
    SIT_ASSERT(SituationDirectoryExists("_sit_test_dir"));
    SituationDeleteDirectory("_sit_test_dir", false);
    SIT_ASSERT(!SituationDirectoryExists("_sit_test_dir"));
}

static void test_get_file_mod_time(void) {
    SituationSaveFileText("_sit_test_text.txt", "modtime test");
    long mod_time = SituationGetFileModTime("_sit_test_text.txt");
    SIT_ASSERT(mod_time > 0);
    SituationDeleteFile("_sit_test_text.txt");
}

// ============================================================================
//  File I/O Tests
// ============================================================================

static void test_save_load_text_roundtrip(void) {
    const char* content = "Hello, Situation Test Harness!\nLine 2.";
    bool ok = SituationSaveFileText("_sit_test_text.txt", content);
    SIT_ASSERT(ok);

    char* loaded = SituationLoadFileText("_sit_test_text.txt");
    SIT_ASSERT_NOT_NULL(loaded);
    SIT_ASSERT_STR_EQ(loaded, content);
    free(loaded);

    SituationDeleteFile("_sit_test_text.txt");
}

static void test_save_load_binary_roundtrip(void) {
    unsigned char data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42, 0xFF};
    unsigned int size = sizeof(data);

    SituationError err = SituationSaveFileData("_sit_test_binary.bin", data, size);
    SIT_ASSERT_EQ(err, 0); // SITUATION_SUCCESS

    unsigned int bytes_read = 0;
    unsigned char* loaded = NULL;
    err = SituationLoadFileData("_sit_test_binary.bin", &bytes_read, &loaded);
    SIT_ASSERT_EQ(err, 0);
    SIT_ASSERT_NOT_NULL(loaded);
    SIT_ASSERT_EQ(bytes_read, size);
    SIT_ASSERT_MEM_EQ(loaded, data, size);
    free(loaded);

    SituationDeleteFile("_sit_test_binary.bin");
}

static void test_copy_file(void) {
    SituationSaveFileText("_sit_test_text.txt", "copy me");
    bool ok = SituationCopyFile("_sit_test_text.txt", "_sit_test_copy.txt");
    SIT_ASSERT(ok);
    SIT_ASSERT(SituationFileExists("_sit_test_copy.txt"));

    char* content = SituationLoadFileText("_sit_test_copy.txt");
    SIT_ASSERT_NOT_NULL(content);
    SIT_ASSERT_STR_EQ(content, "copy me");
    free(content);

    SituationDeleteFile("_sit_test_text.txt");
    SituationDeleteFile("_sit_test_copy.txt");
}

static void test_move_file(void) {
    SituationSaveFileText("_sit_test_text.txt", "move me");
    bool ok = SituationMoveFile("_sit_test_text.txt", "_sit_test_moved.txt");
    SIT_ASSERT(ok);
    SIT_ASSERT(!SituationFileExists("_sit_test_text.txt"));
    SIT_ASSERT(SituationFileExists("_sit_test_moved.txt"));

    char* content = SituationLoadFileText("_sit_test_moved.txt");
    SIT_ASSERT_NOT_NULL(content);
    SIT_ASSERT_STR_EQ(content, "move me");
    free(content);

    SituationDeleteFile("_sit_test_moved.txt");
}

static void test_delete_file(void) {
    SituationSaveFileText("_sit_test_text.txt", "delete me");
    SIT_ASSERT(SituationFileExists("_sit_test_text.txt"));
    bool ok = SituationDeleteFile("_sit_test_text.txt");
    SIT_ASSERT(ok);
    SIT_ASSERT(!SituationFileExists("_sit_test_text.txt"));
}

// ============================================================================
//  Directory Operation Tests
// ============================================================================

static void test_create_delete_directory(void) {
    bool ok = SituationCreateDirectory("_sit_test_dir", false);
    SIT_ASSERT(ok);
    SIT_ASSERT(SituationDirectoryExists("_sit_test_dir"));

    ok = SituationDeleteDirectory("_sit_test_dir", false);
    SIT_ASSERT(ok);
    SIT_ASSERT(!SituationDirectoryExists("_sit_test_dir"));
}

static void test_create_directory_with_parents(void) {
    bool ok = SituationCreateDirectory("_sit_test_dir/sub/deep", true);
    SIT_ASSERT(ok);
    SIT_ASSERT(SituationDirectoryExists("_sit_test_dir/sub/deep"));

    // Recursive delete
    ok = SituationDeleteDirectory("_sit_test_dir", true);
    SIT_ASSERT(ok);
    SIT_ASSERT(!SituationDirectoryExists("_sit_test_dir"));
}

static void test_list_directory_files(void) {
    SituationCreateDirectory("_sit_test_dir", false);
    SituationSaveFileText("_sit_test_dir/_sit_test_a.txt", "a");
    SituationSaveFileText("_sit_test_dir/_sit_test_b.txt", "b");

    int count = 0;
    char** files = SituationListDirectoryFiles("_sit_test_dir", &count);
    SIT_ASSERT_NOT_NULL(files);
    SIT_ASSERT(count >= 2);

    SituationFreeDirectoryFileList(files, count);
    SituationDeleteDirectory("_sit_test_dir", true);
}

// ============================================================================
//  Phase 21 — Async File I/O (requires SITUATION_ENABLE_THREADING)
// ============================================================================

#ifdef SITUATION_ENABLE_THREADING

static volatile bool g_async_load_done = false;
static volatile bool g_async_load_success = false;
static volatile size_t g_async_load_size = 0;

static void async_load_callback(void* data, size_t size, void* user_data) {
    (void)user_data;
    g_async_load_done = true;
    g_async_load_success = (data != NULL && size > 0);
    g_async_load_size = size;
    if (data) free(data);
}

static void test_load_file_async(void) {
    // Create a test file first
    const char* content = "async load test content 12345";
    bool ok = SituationSaveFileText("_sit_test_async.txt", content);
    SIT_ASSERT(ok);

    // Create a thread pool for async ops
    SituationThreadPool pool;
    memset(&pool, 0, sizeof(pool));
    ok = SituationCreateThreadPool(&pool, 2, 64, 0.0, false);
    SIT_ASSERT(ok);

    g_async_load_done = false;
    g_async_load_success = false;
    g_async_load_size = 0;

    SituationJobId job = SituationLoadFileAsync(&pool, "_sit_test_async.txt", async_load_callback, NULL);
    SIT_ASSERT_NEQ(job, 0);

    // Wait for the job to complete
    SituationWaitForJob(&pool, job);

    // Give a brief moment for callback to fire
    SituationWaitForAllJobs(&pool);

    SIT_ASSERT(g_async_load_done);
    SIT_ASSERT(g_async_load_success);
    SIT_ASSERT(g_async_load_size > 0);

    SituationDestroyThreadPool(&pool);
    SituationDeleteFile("_sit_test_async.txt");
}

static volatile bool g_async_text_load_done = false;
static volatile bool g_async_text_load_success = false;

static void async_text_load_callback(char* text, void* user_data) {
    (void)user_data;
    g_async_text_load_done = true;
    g_async_text_load_success = (text != NULL && strlen(text) > 0);
    if (text) free(text);
}

static void test_load_file_text_async(void) {
    const char* content = "async text load test";
    bool ok = SituationSaveFileText("_sit_test_async_text.txt", content);
    SIT_ASSERT(ok);

    SituationThreadPool pool;
    memset(&pool, 0, sizeof(pool));
    ok = SituationCreateThreadPool(&pool, 2, 64, 0.0, false);
    SIT_ASSERT(ok);

    g_async_text_load_done = false;
    g_async_text_load_success = false;

    SituationJobId job = SituationLoadFileTextAsync(&pool, "_sit_test_async_text.txt", async_text_load_callback, NULL);
    SIT_ASSERT_NEQ(job, 0);

    SituationWaitForJob(&pool, job);
    SituationWaitForAllJobs(&pool);

    SIT_ASSERT(g_async_text_load_done);
    SIT_ASSERT(g_async_text_load_success);

    SituationDestroyThreadPool(&pool);
    SituationDeleteFile("_sit_test_async_text.txt");
}

static volatile bool g_async_save_done = false;
static volatile bool g_async_save_success = false;

static void async_save_callback(bool success, void* user_data) {
    (void)user_data;
    g_async_save_done = true;
    g_async_save_success = success;
}

static void test_save_file_async(void) {
    SituationThreadPool pool;
    memset(&pool, 0, sizeof(pool));
    bool ok = SituationCreateThreadPool(&pool, 2, 64, 0.0, false);
    SIT_ASSERT(ok);

    const char data[] = "async save binary data 0xDEAD";
    g_async_save_done = false;
    g_async_save_success = false;

    SituationJobId job = SituationSaveFileAsync(&pool, "_sit_test_async_save.bin", data, sizeof(data), async_save_callback, NULL);
    SIT_ASSERT_NEQ(job, 0);

    SituationWaitForJob(&pool, job);
    SituationWaitForAllJobs(&pool);

    SIT_ASSERT(g_async_save_done);
    SIT_ASSERT(g_async_save_success);
    SIT_ASSERT(SituationFileExists("_sit_test_async_save.bin"));

    SituationDestroyThreadPool(&pool);
    SituationDeleteFile("_sit_test_async_save.bin");
}

static void test_save_file_text_async(void) {
    SituationThreadPool pool;
    memset(&pool, 0, sizeof(pool));
    bool ok = SituationCreateThreadPool(&pool, 2, 64, 0.0, false);
    SIT_ASSERT(ok);

    g_async_save_done = false;
    g_async_save_success = false;

    SituationJobId job = SituationSaveFileTextAsync(&pool, "_sit_test_async_save_text.txt", "hello async world", async_save_callback, NULL);
    SIT_ASSERT_NEQ(job, 0);

    SituationWaitForJob(&pool, job);
    SituationWaitForAllJobs(&pool);

    SIT_ASSERT(g_async_save_done);
    SIT_ASSERT(g_async_save_success);
    SIT_ASSERT(SituationFileExists("_sit_test_async_save_text.txt"));

    // Verify content
    char* loaded = SituationLoadFileText("_sit_test_async_save_text.txt");
    SIT_ASSERT_NOT_NULL(loaded);
    SIT_ASSERT_STR_EQ(loaded, "hello async world");
    free(loaded);

    SituationDestroyThreadPool(&pool);
    SituationDeleteFile("_sit_test_async_save_text.txt");
}

#endif // SITUATION_ENABLE_THREADING

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase filesystem_tests[] = {
    // Path utilities
    {"get_base_path",               test_get_base_path,               false},
    {"get_file_name",               test_get_file_name,               false},
    {"get_file_name_no_dir",        test_get_file_name_no_dir,        false},
    {"get_file_extension",          test_get_file_extension,          false},
    {"get_file_extension_none",     test_get_file_extension_none,     false},
    {"join_path",                   test_join_path,                   false},
    {"get_app_save_path",           test_get_app_save_path,           false},
    // File queries
    {"file_exists_positive",        test_file_exists_positive,        false},
    {"file_exists_negative",        test_file_exists_negative,        false},
    {"directory_exists",            test_directory_exists,            false},
    {"get_file_mod_time",           test_get_file_mod_time,           false},
    // File I/O
    {"save_load_text_roundtrip",    test_save_load_text_roundtrip,    false},
    {"save_load_binary_roundtrip",  test_save_load_binary_roundtrip,  false},
    {"copy_file",                   test_copy_file,                   false},
    {"move_file",                   test_move_file,                   false},
    {"delete_file",                 test_delete_file,                 false},
    // Directory operations
    {"create_delete_directory",     test_create_delete_directory,     false},
    {"create_directory_parents",    test_create_directory_with_parents, false},
    {"list_directory_files",        test_list_directory_files,        false},

    // --- Phase 21: Async File I/O (4) ---
#ifdef SITUATION_ENABLE_THREADING
    {"load_file_async",             test_load_file_async,             false},
    {"load_file_text_async",        test_load_file_text_async,        false},
    {"save_file_async",             test_save_file_async,             false},
    {"save_file_text_async",        test_save_file_text_async,        false},
#endif
};

const SitTestModule g_module_filesystem = {
    .name = "filesystem",
    .setup = NULL,
    .teardown = filesystem_teardown,
    .tests = filesystem_tests,
    .test_count = sizeof(filesystem_tests) / sizeof(filesystem_tests[0]),
    .requires_context = false
};
