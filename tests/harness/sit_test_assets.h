/**
 * @file sit_test_assets.h
 * @brief Resolve harness asset paths regardless of process working directory.
 *
 * Tests are launched from the repo root via run_tests.bat, but may also be run
 * from build/tests/ or other CWDs. Asset lookups try the usual prefixes before
 * giving up.
 *
 * Requires Situation API (SituationFileExists) — include sit_api_include.h first.
 */

#ifndef SIT_TEST_ASSETS_H
#define SIT_TEST_ASSETS_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#if !defined(_WIN32)
  #include <strings.h>
#endif

/** Relative directories searched for bundled harness assets (filename appended). */
static const char* const g_sit_test_harness_asset_prefixes[] = {
    "tests/harness/assets/",
    "../tests/harness/assets/",
    "../../tests/harness/assets/",
    NULL
};

/**
 * Find the first existing harness asset for `filename` (e.g. "rosewood-veneer.png").
 * @param out_path Buffer receiving the resolved path on success.
 * @return true if SituationFileExists on any prefix+filename combination.
 */
static bool sit_test_resolve_harness_asset(const char* filename, char* out_path, size_t out_path_sz) {
    if (!filename || !filename[0] || !out_path || out_path_sz == 0) {
        return false;
    }

    for (int i = 0; g_sit_test_harness_asset_prefixes[i] != NULL; ++i) {
        snprintf(out_path, out_path_sz, "%s%s", g_sit_test_harness_asset_prefixes[i], filename);
        if (SituationFileExists(out_path)) {
            return true;
        }
    }
    return false;
}

/**
 * Try several filenames under harness asset prefixes; returns the first path that exists.
 * @param filenames NULL-terminated array of basenames (e.g. "rosewood-veneer.png").
 */
static bool sit_test_resolve_harness_asset_any(
    const char* const* filenames, char* out_path, size_t out_path_sz)
{
    if (!filenames || !out_path || out_path_sz == 0) {
        return false;
    }
    for (int f = 0; filenames[f] != NULL; ++f) {
        if (sit_test_resolve_harness_asset(filenames[f], out_path, out_path_sz)) {
            return true;
        }
    }
    return false;
}

/** Case-insensitive: true if `needle` appears in `haystack`. */
static bool sit_test_basename_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle || !needle[0]) {
        return false;
    }
    size_t hay_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    if (needle_len > hay_len) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= hay_len; ++i) {
#if defined(_WIN32)
        if (_strnicmp(haystack + i, needle, needle_len) == 0) {
#else
        if (strncasecmp(haystack + i, needle, needle_len) == 0) {
#endif
            return true;
        }
    }
    return false;
}

/**
 * Scan harness asset directories for the first stb-loadable image whose basename contains `substr`.
 * e.g. substr "rosewood" matches rosewood_veneer1.png, rosewood-veneer.png, etc.
 */
static bool sit_test_resolve_harness_asset_name_contains(
    const char* substr, char* out_path, size_t out_path_sz)
{
    if (!substr || !substr[0] || !out_path || out_path_sz == 0) {
        return false;
    }

    for (int d = 0; g_sit_test_harness_asset_prefixes[d] != NULL; ++d) {
        const char* dir = g_sit_test_harness_asset_prefixes[d];
        int count = 0;
        char** entries = SituationListDirectoryFiles(dir, &count);
        if (!entries || count <= 0) {
            if (entries) {
                SituationFreeDirectoryFileList(entries, count);
            }
            continue;
        }

        for (int i = 0; i < count; ++i) {
            const char* name = entries[i];
            const char* ext = SituationGetFileExtension(name);
            if (!ext || !SituationIsStbImageLoadExtension(ext)) {
                continue;
            }
            if (!sit_test_basename_contains_ci(name, substr)) {
                continue;
            }
            snprintf(out_path, out_path_sz, "%s%s", dir, name);
            if (SituationFileExists(out_path)) {
                SituationFreeDirectoryFileList(entries, count);
                return true;
            }
        }
        SituationFreeDirectoryFileList(entries, count);
    }
    return false;
}

#endif /* SIT_TEST_ASSETS_H */
