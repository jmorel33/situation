/**
 * @file build/mybuddy_smoke_main.c
 * @brief M7 smoke — KFS + SQLite heap through MyBuddy backend (libkfs_mybuddy.a).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "mybuddy/mybuddy.h"
#include "kfs/kfs.h"

#define SMOKE_FAIL(msg) do { fprintf(stderr, "[mybuddy_smoke] FAIL: %s\n", (msg)); return 1; } while (0)

static int smoke_sqlite_roundtrip(void) {
    void* p = sqlite3_malloc(64);
    int sz;
    if (!p) {
        SMOKE_FAIL("sqlite3_malloc(64)");
    }
    sz = sqlite3_msize(p);
    if (sz < 64) {
        sqlite3_free(p);
        fprintf(stderr, "[mybuddy_smoke] FAIL: sqlite3_msize=%d\n", sz);
        return 1;
    }
    sqlite3_free(p);
    return 0;
}

int main(void) {
    void* kfs_ptr;
    void* grown;
    char art_path[256];
    char arch_path[256];
    char reg_path[256];
    GameDB* db = NULL;
    int rc;
    int pid = (int)getpid();

    if (kfs_mem_init(NULL) != KFS_OK) {
        SMOKE_FAIL("kfs_mem_init");
    }

    kfs_ptr = kfs_mem_alloc(128);
    if (!kfs_ptr) {
        SMOKE_FAIL("kfs_mem_alloc(128)");
    }
    grown = kfs_mem_realloc(kfs_ptr, 256);
    if (!grown) {
        kfs_mem_free(kfs_ptr);
        SMOKE_FAIL("kfs_mem_realloc grow");
    }
    kfs_mem_free(grown);

    if (smoke_sqlite_roundtrip() != 0) {
        return 1;
    }

    snprintf(art_path, sizeof(art_path), "kfs_mb_smoke_art_%d.db", pid);
    snprintf(arch_path, sizeof(arch_path), "kfs_mb_smoke_arch_%d.db", pid);
    snprintf(reg_path, sizeof(reg_path), "kfs_mb_smoke_reg_%d.db", pid);
    rc = kfs_init(&db, art_path, arch_path, reg_path);
    if (rc != KFS_OK) {
        fprintf(stderr, "[mybuddy_smoke] FAIL: kfs_init rc=%d\n", rc);
        return 1;
    }
    kfs_close(db);
    remove(art_path);
    remove(arch_path);
    remove(reg_path);

    printf("[mybuddy_smoke] OK — KFS mem + SQLite vtable on MyBuddy (%s / %s)\n",
           kfs_get_version_string(), MbdGetVersionString());
    return 0;
}