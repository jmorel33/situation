/**
 * @file kfs_mem.h
 * @brief KFS unified memory layer — macros and public allocator API.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 * All KFS heap allocations should use KFS_MALLOC / KFS_FREE (etc.).
 * SQLite is routed through the same backend via sqlite3_mem_methods installed
 * in kfs_mem_init() (xMalloc, xFree, xRealloc, xSize, xRoundup, xInit, xShutdown).
 *
 * Bootstrap order (required):
 *   1. kfs_mem_init(cfg)  — before any KFS or SQLite use
 *   2. kfs_init(...)      — may call kfs_mem_init(NULL) if not done yet
 *
 * Policy defaults (cfg NULL or zero fields): L1 unified allocator, hard_limit_bytes=0,
 * max_open_db=0, MEMSTATUS=1 (required, set inside kfs_mem_init).
 *
 * Standalone: KFS does not use Situation SIT_MALLOC (see doc/memory_alloc_plan.md D7).
 *
 * Optional MyBuddy backend (M7): compile the KFS TU with -DKFS_MEM_USE_MYBUDDY and link
 * pthread (+ -lbcrypt on Windows). Instantiates MyBuddy in kfs.c; heap island in
 * kfs_impl_core.h uses mbd_alloc / mbd_free / mbd_realloc. Default build stays CRT.
 * MyBuddy init uses production profile C: MBD_FLAG_BUDDY_LARGE + 256 MiB pool (M7 bench).
 *
 * Macro overrides (embedder / alternate backend):
 *   Define KFS_MALLOC, KFS_CALLOC, KFS_REALLOC, KFS_FREE, and/or KFS_STRDUP
 *   **before** including this header. Each macro is wrapped in #ifndef, so the
 *   first definition wins. Impl code (kfs_impl_*.h) uses KFS_* exclusively.
 *
 *   Overrides change impl allocation only. SQLite heap still flows through the
 *   kfs_mem_* functions installed via sqlite3_mem_methods in kfs_mem_init().
 *   To route SQLite through MyBuddy (mbd_*), replace the CRT calls inside the
 *   KFS_MEM_CRT_BACKEND region in kfs_impl_core.h (or swap that backend at
 *   link time) — do not depend on Situation headers.
 *
 *   API-returned pointers must be released with kfs_mem_free() regardless of
 *   macro overrides (same heap as KFS_MALLOC).
 *
 * Heap policy (D3/D6):
 *   hard_limit_bytes > 0 — KFS alloc/realloc reject when in_use + request would
 *   exceed the cap; SQLite soft heap limit is set to the same value.
 *   max_open_db > 0 — kfs_init returns KFS_MISUSE when open handle count >= cap.
 *
 * Not routed through kfs_mem (see doc/memory_alloc_plan.md §2.2):
 *   per-connection lookaside slots, mmap I/O, static page-cache pools.
 */
#ifndef KFS_MEM_H
#define KFS_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t hard_limit_bytes; /* 0 = unlimited heap cap (default) */
    int    max_open_db;      /* 0 = unlimited concurrent GameDB handles (default) */
    int    track_stats;      /* 1 = maintain in-use / peak counters (default) */
} kfs_mem_config_t;

typedef void (*kfs_mem_oom_fn)(size_t requested, void* userdata);

int    kfs_mem_init(const kfs_mem_config_t* cfg);
void   kfs_mem_shutdown(void);

void*  kfs_mem_alloc(size_t size);
void*  kfs_mem_calloc(size_t count, size_t size);
void*  kfs_mem_realloc(void* ptr, size_t size);
void   kfs_mem_free(void* ptr);
char*  kfs_mem_strdup(const char* str);

size_t kfs_mem_bytes_in_use(void);
size_t kfs_mem_peak_bytes(void);
size_t kfs_sqlite_bytes_in_use(void);

/** Reset peak counter to current in-use (does not free memory). */
void   kfs_mem_reset_peak(void);

void   kfs_mem_set_oom_callback(kfs_mem_oom_fn fn, void* userdata);

/** Runtime policy (safe after kfs_mem_init). 0 = unlimited for limit/cap fields. */
void   kfs_mem_set_hard_limit_bytes(size_t nbytes);
void   kfs_mem_set_max_open_db(int max);

#ifndef KFS_MALLOC
#define KFS_MALLOC(sz)        kfs_mem_alloc(sz)
#endif
#ifndef KFS_CALLOC
#define KFS_CALLOC(n, sz)     kfs_mem_calloc((n), (sz))
#endif
#ifndef KFS_REALLOC
#define KFS_REALLOC(p, sz)    kfs_mem_realloc((p), (sz))
#endif
#ifndef KFS_FREE
#define KFS_FREE(p)           do { if (p) { kfs_mem_free(p); (p) = NULL; } } while(0)
#endif
#ifndef KFS_STRDUP
#define KFS_STRDUP(s)         kfs_mem_strdup(s)
#endif

#ifdef __cplusplus
}
#endif

#endif /* KFS_MEM_H */