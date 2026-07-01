/**
 * @file sit_test_scratch.h
 * @brief Writable scratch directory for harness temp files (keeps repo root clean).
 *
 * All disposable `_sit_test_*` artifacts go under `build/tests/scratch/`.
 * Create once via sit_test_scratch_ensure() before running tests.
 *
 * Uses only C stdlib / OS mkdir — no Situation headers (avoids type clashes with situation.h).
 */

#ifndef SIT_TEST_SCRATCH_H
#define SIT_TEST_SCRATCH_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <direct.h>
  #include <sys/stat.h>
#else
  #include <sys/stat.h>
#endif

#define SIT_TEST_SCRATCH_DIR "build/tests/scratch"

static inline bool sit_test_mkdir_one(const char* path) {
    if (!path || !path[0]) {
        return false;
    }
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    return errno == EEXIST;
}

static inline bool sit_test_scratch_ensure(void) {
    if (!sit_test_mkdir_one("build")) {
        return false;
    }
    if (!sit_test_mkdir_one("build/tests")) {
        return false;
    }
    return sit_test_mkdir_one("build/tests/scratch");
}

static inline void sit_test_scratch_path(const char* leaf, char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (!leaf || !leaf[0]) {
        snprintf(out, out_size, "%s", SIT_TEST_SCRATCH_DIR);
        return;
    }
    snprintf(out, out_size, "%s/%s", SIT_TEST_SCRATCH_DIR, leaf);
}

/** Ring of static buffers — safe for up to four paths in one statement (e.g. copy src/dst). */
static inline const char* sit_test_tmp(const char* leaf) {
    static char slots[4][512];
    static int slot;
    char* out = slots[slot++ % 4];
    sit_test_scratch_path(leaf, out, sizeof(slots[0]));
    return out;
}

#endif /* SIT_TEST_SCRATCH_H */
