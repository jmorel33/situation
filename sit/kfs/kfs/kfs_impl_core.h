/**
 * @file kfs_impl_core.h
 * @brief KFS implementation — platform shims, DB lifecycle, utilities, memory frees.
 *
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 * No business permission rules (kfs_check_permission lives in kfs_impl_auth.h).
 *
 * Split phase: S3 (extracted from kfs_impl.h).
 */
#ifndef KFS_IMPL_CORE_H
#define KFS_IMPL_CORE_H

#ifdef KFS_IMPLEMENTATION

/* SECTION: platform + CRT — S3.1 */
/* SECTION: static helpers — S3.2 */
/* SECTION: kfs_init / kfs_close — S3.3 */
/* SECTION: memory frees — S3.4 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#include <windows.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <sys/time.h>
#else
struct timeval {
    long tv_sec;
    long tv_usec;
};
static int kfs_gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    if (!tv) return -1;
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uint64_t t = (uli.QuadPart - 116444736000000000ULL) / 10ULL;
    tv->tv_sec = (long)(t / 1000000ULL);
    tv->tv_usec = (long)(t % 1000000ULL);
    return 0;
}
#define gettimeofday kfs_gettimeofday
#endif
#else
#include <sys/time.h>
#endif

#include <limits.h>
#include <stdatomic.h>

/* ============================================================================== */
/* ==                     UNIFIED MEMORY (KFS + SQLite)                      == */
/* ============================================================================== */

#define KFS_MEM_ALIGN       8u
#define KFS_MEM_HEADER_SIZE sizeof(size_t)

typedef struct {
    size_t            hard_limit_bytes;
    int               max_open_db;
    int               track_stats;
    int               installed;
    int               sqlite_layer_active;
    int               memstatus_required;
    _Atomic size_t    bytes_in_use;
    _Atomic size_t    peak_bytes;
    int               open_db_count;
    kfs_mem_oom_fn    oom_fn;
    void*             oom_userdata;
} kfs_mem_state_t;

static kfs_mem_state_t g_kfs_mem = {
    0, 0, 1, 0, 0, 1, 0, 0, 0, NULL, NULL
};

static void* kfs_mem_block_from_user(void* user_ptr) {
    if (!user_ptr) {
        return NULL;
    }
    return (char*)user_ptr - KFS_MEM_HEADER_SIZE;
}

static void* kfs_mem_user_from_block(void* block) {
    if (!block) {
        return NULL;
    }
    return (char*)block + KFS_MEM_HEADER_SIZE;
}

static size_t kfs_mem_user_size(void* user_ptr) {
    void* block = kfs_mem_block_from_user(user_ptr);
    if (!block) {
        return 0;
    }
    return *(size_t*)block;
}

static void kfs_mem_store_user_size(void* block, size_t user_size) {
    *(size_t*)block = user_size;
}

static int kfs_mem_roundup_size(int n) {
    if (n <= 0) {
        return 0;
    }
    unsigned int size = (unsigned int)n;
    unsigned int align = (unsigned int)KFS_MEM_ALIGN;
    return (int)((size + align - 1u) & ~(align - 1u));
}

static void kfs_mem_stats_add(size_t user_size) {
    if (!g_kfs_mem.track_stats || user_size == 0) {
        return;
    }
    size_t prev = atomic_fetch_add(&g_kfs_mem.bytes_in_use, user_size) + user_size;
    size_t peak = atomic_load(&g_kfs_mem.peak_bytes);
    while (prev > peak && !atomic_compare_exchange_weak(&g_kfs_mem.peak_bytes, &peak, prev)) {
        /* retry */
    }
}

static void kfs_mem_stats_remove(size_t user_size) {
    if (!g_kfs_mem.track_stats || user_size == 0) {
        return;
    }
    atomic_fetch_sub(&g_kfs_mem.bytes_in_use, user_size);
}

static int kfs_mem_would_exceed_limit(size_t user_size) {
    if (g_kfs_mem.hard_limit_bytes == 0) {
        return 0;
    }
    size_t in_use = atomic_load(&g_kfs_mem.bytes_in_use);
    return (in_use + user_size) > g_kfs_mem.hard_limit_bytes;
}

static void kfs_mem_sync_sqlite_soft_limit(void) {
    if (g_kfs_mem.hard_limit_bytes > 0) {
        sqlite3_soft_heap_limit64((sqlite3_int64)g_kfs_mem.hard_limit_bytes);
    } else {
        sqlite3_soft_heap_limit64(0);
    }
}

/* BEGIN KFS_MEM_CRT_BACKEND — sole heap backend island; allowed by scripts/check_kfs_mem.py */
#ifdef KFS_MEM_USE_MYBUDDY
#include "mybuddy/mybuddy.h"
#define KFS_MEM_BACKEND_ALLOC(total)        mbd_alloc(total)
#define KFS_MEM_BACKEND_FREE(block)         mbd_free(block)
#define KFS_MEM_BACKEND_REALLOC(block, tot) mbd_realloc((block), (tot))
#else
#define KFS_MEM_BACKEND_ALLOC(total)        malloc(total)
#define KFS_MEM_BACKEND_FREE(block)         free(block)
#define KFS_MEM_BACKEND_REALLOC(block, tot) realloc((block), (tot))
#endif

static void* kfs_mem_alloc_bytes(size_t user_size) {
    if (user_size == 0) {
        return NULL;
    }
    if (kfs_mem_would_exceed_limit(user_size)) {
        if (g_kfs_mem.oom_fn) {
            g_kfs_mem.oom_fn(user_size, g_kfs_mem.oom_userdata);
        }
        return NULL;
    }

    size_t total = user_size + KFS_MEM_HEADER_SIZE;
    void* block = KFS_MEM_BACKEND_ALLOC(total);
    if (!block) {
        if (g_kfs_mem.oom_fn) {
            g_kfs_mem.oom_fn(user_size, g_kfs_mem.oom_userdata);
        }
        return NULL;
    }

    kfs_mem_store_user_size(block, user_size);
    kfs_mem_stats_add(user_size);
    return kfs_mem_user_from_block(block);
}

static void kfs_mem_free_bytes(void* user_ptr) {
    if (!user_ptr) {
        return;
    }
    size_t user_size = kfs_mem_user_size(user_ptr);
    void* block = kfs_mem_block_from_user(user_ptr);
    kfs_mem_stats_remove(user_size);
    KFS_MEM_BACKEND_FREE(block);
}

static void* kfs_mem_realloc_bytes(void* user_ptr, size_t new_user_size) {
    if (new_user_size == 0) {
        kfs_mem_free_bytes(user_ptr);
        return NULL;
    }
    if (!user_ptr) {
        return kfs_mem_alloc_bytes(new_user_size);
    }
    size_t old_size = kfs_mem_user_size(user_ptr);
    if (g_kfs_mem.hard_limit_bytes > 0) {
        size_t in_use = atomic_load(&g_kfs_mem.bytes_in_use);
        if (in_use - old_size + new_user_size > g_kfs_mem.hard_limit_bytes) {
            if (g_kfs_mem.oom_fn) {
                g_kfs_mem.oom_fn(new_user_size, g_kfs_mem.oom_userdata);
            }
            return NULL;
        }
    }
    void* block = kfs_mem_block_from_user(user_ptr);
    size_t new_total = new_user_size + KFS_MEM_HEADER_SIZE;
    void* new_block = KFS_MEM_BACKEND_REALLOC(block, new_total);
    if (!new_block) {
        if (g_kfs_mem.oom_fn) {
            g_kfs_mem.oom_fn(new_user_size, g_kfs_mem.oom_userdata);
        }
        return NULL;
    }

    kfs_mem_store_user_size(new_block, new_user_size);
    if (g_kfs_mem.track_stats) {
        if (new_user_size >= old_size) {
            kfs_mem_stats_add(new_user_size - old_size);
        } else {
            kfs_mem_stats_remove(old_size - new_user_size);
        }
    }
    return kfs_mem_user_from_block(new_block);
}
/* END KFS_MEM_CRT_BACKEND */

/* --- SQLite sqlite3_mem_methods (full vtable) ----------------------------- */

static void* kfs_sqlite_mem_malloc(int n) {
    if (n < 0) {
        return NULL;
    }
    return kfs_mem_alloc_bytes((size_t)n);
}

static void kfs_sqlite_mem_free(void* p) {
    kfs_mem_free_bytes(p);
}

static void* kfs_sqlite_mem_realloc(void* p, int n) {
    if (n < 0) {
        return NULL;
    }
    return kfs_mem_realloc_bytes(p, (size_t)n);
}

static int kfs_sqlite_mem_size(void* p) {
    if (!p) {
        return 0;
    }
    size_t sz = kfs_mem_user_size(p);
    if (sz > (size_t)INT_MAX) {
        return INT_MAX;
    }
    return (int)sz;
}

static int kfs_sqlite_mem_roundup(int n) {
    return kfs_mem_roundup_size(n);
}

static int kfs_sqlite_mem_init(void* app_data) {
    (void)app_data;
    g_kfs_mem.sqlite_layer_active = 1;
    return SQLITE_OK;
}

static void kfs_sqlite_mem_shutdown(void* app_data) {
    (void)app_data;
    g_kfs_mem.sqlite_layer_active = 0;
}

static sqlite3_mem_methods g_kfs_sqlite_mem_methods = {
    kfs_sqlite_mem_malloc,
    kfs_sqlite_mem_free,
    kfs_sqlite_mem_realloc,
    kfs_sqlite_mem_size,
    kfs_sqlite_mem_roundup,
    kfs_sqlite_mem_init,
    kfs_sqlite_mem_shutdown,
    NULL
};

#ifdef KFS_MEM_USE_MYBUDDY
static void kfs_mybuddy_backend_init(void) {
    /* Production profile C (M7 bench winner): BUDDY_LARGE + 256 MiB pool */
    mbd_config_t mbd_cfg = {0};
    mbd_cfg.flags = MBD_FLAG_BUDDY_LARGE;
    mbd_cfg.pool_size = (1ULL << 28);
    mbd_init(&mbd_cfg);
}
#endif

/* --- Public memory API ---------------------------------------------------- */

int kfs_mem_init(const kfs_mem_config_t* cfg) {
    if (g_kfs_mem.installed) {
        return KFS_OK;
    }

#ifdef KFS_MEM_USE_MYBUDDY
    kfs_mybuddy_backend_init();
#endif

    if (cfg) {
        g_kfs_mem.hard_limit_bytes = cfg->hard_limit_bytes;
        g_kfs_mem.max_open_db = cfg->max_open_db;
        g_kfs_mem.track_stats = cfg->track_stats ? 1 : 0;
    } else {
        g_kfs_mem.hard_limit_bytes = 0;
        g_kfs_mem.max_open_db = 0;
        g_kfs_mem.track_stats = 1;
    }

    int rc = sqlite3_config(SQLITE_CONFIG_MALLOC, &g_kfs_sqlite_mem_methods);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_mem_init: sqlite3_config(SQLITE_CONFIG_MALLOC) failed (%d)\n", rc);
        return KFS_MISUSE;
    }

    rc = sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 1);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_mem_init: sqlite3_config(SQLITE_CONFIG_MEMSTATUS) required but failed (%d)\n", rc);
        return KFS_MISUSE;
    }
    g_kfs_mem.memstatus_required = 1;
    kfs_mem_sync_sqlite_soft_limit();

    g_kfs_mem.installed = 1;
    return KFS_OK;
}

void kfs_mem_shutdown(void) {
    if (!g_kfs_mem.installed) {
        return;
    }
    if (g_kfs_mem.open_db_count > 0) {
        fprintf(stderr, "[WARN] kfs_mem_shutdown: %d open GameDB handle(s); skipping bookkeeping reset\n",
                g_kfs_mem.open_db_count);
        return;
    }
    g_kfs_mem.installed = 0;
    g_kfs_mem.sqlite_layer_active = 0;
    atomic_store(&g_kfs_mem.bytes_in_use, 0);
    atomic_store(&g_kfs_mem.peak_bytes, 0);
}

void* kfs_mem_alloc(size_t size) {
    return kfs_mem_alloc_bytes(size);
}

void* kfs_mem_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    if (count > (size_t)-1 / size) {
        return NULL;
    }
    size_t total = count * size;
    void* ptr = kfs_mem_alloc_bytes(total);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0, total);
    return ptr;
}

void* kfs_mem_realloc(void* ptr, size_t size) {
    return kfs_mem_realloc_bytes(ptr, size);
}

void kfs_mem_free(void* ptr) {
    kfs_mem_free_bytes(ptr);
}

char* kfs_mem_strdup(const char* str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char* dup = (char*)kfs_mem_alloc_bytes(len);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, str, len);
    return dup;
}

size_t kfs_mem_bytes_in_use(void) {
    return atomic_load(&g_kfs_mem.bytes_in_use);
}

size_t kfs_mem_peak_bytes(void) {
    return atomic_load(&g_kfs_mem.peak_bytes);
}

size_t kfs_sqlite_bytes_in_use(void) {
    sqlite3_int64 used = sqlite3_memory_used();
    if (used < 0) {
        return 0;
    }
    return (size_t)used;
}

void kfs_mem_reset_peak(void) {
    size_t current = atomic_load(&g_kfs_mem.bytes_in_use);
    atomic_store(&g_kfs_mem.peak_bytes, current);
}

void kfs_mem_set_oom_callback(kfs_mem_oom_fn fn, void* userdata) {
    g_kfs_mem.oom_fn = fn;
    g_kfs_mem.oom_userdata = userdata;
}

void kfs_mem_set_hard_limit_bytes(size_t nbytes) {
    g_kfs_mem.hard_limit_bytes = nbytes;
    kfs_mem_sync_sqlite_soft_limit();
}

void kfs_mem_set_max_open_db(int max) {
    g_kfs_mem.max_open_db = (max > 0) ? max : 0;
}

/* ── Version query (same pattern as MyBuddy MbdGetVersionString) ─────────── */

#define _KFS_STR_HELPER(x) #x
#define _KFS_STR(x) _KFS_STR_HELPER(x)

const char* kfs_get_version_string(void) {
    return _KFS_STR(KFS_VERSION_MAJOR) "."
           _KFS_STR(KFS_VERSION_MINOR) "."
           _KFS_STR(KFS_VERSION_PATCH)
           KFS_VERSION_REVISION
           " (" KFS_VERSION_DESCRIPTION ")";
}

// Simple string hash function (djb2) - Returns 32 bits
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    if (!str) return hash; // Handle NULL input
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

/**
 * @brief Generates a 64-bit KFS UUID based on milliseconds and username hash.
 * Uses approximately 42 bits for time and 22 bits for hash.
 * WARNING: Collisions are still possible, especially if users are created
 *          with the same name within the same millisecond.
 *
 * @param username The username to incorporate.
 * @param output_uuid Pointer to store the resulting 64-bit unsigned integer.
 * @return KFS_OK on success, KFS_ERROR on timer failure, KFS_INVALID_ARGUMENT.
 */
static int generate_kfs_uuid_64(const char* username, uint64_t* output_uuid) {
    if (!username || !output_uuid) {
        return KFS_INVALID_ARGUMENT;
    }

    uint64_t time_ms = 0;
    uint32_t name_hash = 0;
    const int hash_bits = 22; // Number of bits reserved for hash
    const uint64_t hash_mask = (1ULL << hash_bits) - 1; // Mask for lower 22 bits (e.g., 0x3FFFFF)

    // 1. Get Time (Milliseconds since epoch)
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("generate_kfs_uuid_64: gettimeofday failed");
        return KFS_ERROR; // Timer error
    }
    time_ms = ((uint64_t)tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);

    // 2. Hash Username (32 bits)
    name_hash = hash_string(username);

    // 3. Combine
    // Shift time left to make space for hash bits
    uint64_t time_shifted = time_ms << hash_bits;

    // Mask the hash to take only the lower 'hash_bits' bits
    uint64_t hash_masked_64 = (uint64_t)(name_hash & hash_mask);

    // Combine using bitwise OR
    *output_uuid = time_shifted | hash_masked_64;

    // Optional: Debug print
    // printf("Time (ms): %llu (0x%llx)\n", (unsigned long long)time_ms, (unsigned long long)time_ms);
    // printf("Name Hash: %u (0x%x)\n", name_hash, name_hash);
    // printf("Shifted T: 0x%016llx\n", (unsigned long long)time_shifted);
    // printf("Masked H:  0x%016llx\n", (unsigned long long)hash_masked_64);
    // printf("Combined:  0x%016llx (%llu)\n", (unsigned long long)*output_uuid, (unsigned long long)*output_uuid);

    return KFS_OK;
}


/**
 * @brief Ensures a database file exists at the specified path.
 */
int kfs_ensure_db_file_exists(const char* db_path) {
    if (!db_path || strlen(db_path) == 0) {
        return KFS_INVALID_ARGUMENT;
    }

    sqlite3* temp_db = NULL;
    // SQLITE_OPEN_CREATE is default, but be explicit.
    // SQLITE_OPEN_READWRITE is also default.
    int rc = sqlite3_open_v2(db_path, &temp_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_ensure_db_file_exists: Failed to open/create '%s': %s\n", db_path, sqlite3_errmsg(temp_db));
        // Close if open somehow succeeded partially but returned error
        if (temp_db) sqlite3_close(temp_db);
        // Map common errors if desired, otherwise return SQLite code
        if (rc == SQLITE_CANTOPEN) return KFS_CANTOPEN;
        return rc; // Return the specific SQLite error
    }

    // Successfully opened (and possibly created). Close it immediately.
    rc = sqlite3_close(temp_db);
    if (rc != SQLITE_OK) {
         // This usually indicates unfinalized statements, which shouldn't happen here.
         fprintf(stderr, "[WARN] kfs_ensure_db_file_exists: Error closing temporary handle for '%s': %s\n", db_path, sqlite3_errstr(rc));
         // Still return OK as the file likely exists.
    }

    return KFS_OK;
}

/**
 * @brief Deletes a database file from the filesystem.
 */
int kfs_delete_db_file(const char* db_path) {
     if (!db_path || strlen(db_path) == 0) {
        return KFS_INVALID_ARGUMENT;
    }

    // Optional: Check if file exists first to avoid error return from remove() if not found
    // struct stat buffer;
    // if (stat(db_path, &buffer) != 0) {
    //     // File doesn't exist (or other stat error) - consider this success for deletion.
    //     // Could check errno specifically for ENOENT.
    //     return KFS_OK;
    // }

    // Attempt to remove the file
    int remove_rc = remove(db_path);

    if (remove_rc == 0) {
        // Deletion successful
        return KFS_OK;
    } else {
        // Deletion failed. Check if it was because the file didn't exist.
        // Note: Checking errno after remove() can be unreliable across platforms/libraries.
        // A common pattern is to try deleting and only report error if it wasn't ENOENT (File not found).
        // However, a simpler approach for now is: if remove() fails, return an error.
        // The caller might retry or ignore based on context.
        // perror("kfs_delete_db_file"); // Print system error message (e.g., "Permission denied")
        fprintf(stderr, "[ERROR] kfs_delete_db_file: Failed to remove file '%s'. Check permissions or if file is in use.\n", db_path);
        return KFS_ERROR; // Generic error for deletion failure
    }
}

/* Close all database connections */
int kfs_close(GameDB* db) {
    if (!db) {
        return KFS_OK; // Nothing to close
    }
    int rc1 = sqlite3_close(db->artifacts_db);
    int rc2 = sqlite3_close(db->arch_db);
    int rc3 = sqlite3_close(db->registry_db);
    KFS_FREE(db);
    g_kfs_mem.open_db_count--;

    // Check for errors during close (usually indicates unfinalized statements)
    if (rc1 != SQLITE_OK) fprintf(stderr, "[WARN] Error closing artifacts_db: %s\n", sqlite3_errstr(rc1));
    if (rc2 != SQLITE_OK) fprintf(stderr, "[WARN] Error closing arch_db: %s\n", sqlite3_errstr(rc2));
    if (rc3 != SQLITE_OK) fprintf(stderr, "[WARN] Error closing registry_db: %s\n", sqlite3_errstr(rc3));

    // Return OK if all closed successfully, else return a generic error
    return (rc1 == KFS_OK && rc2 == KFS_OK && rc3 == KFS_OK) ? KFS_OK : KFS_ERROR;
}

/********************** DATABASE USAGE **********************

kfs_delete_db_file("architecture.db"); // Ignore error if it didn't exist
kfs_delete_db_file("registry.db");
kfs_delete_db_file("artifacts.db");

kfs_ensure_db_file_exists("architecture.db");
kfs_ensure_db_file_exists("registry.db");
kfs_ensure_db_file_exists("artifacts.db");

kfs_delete_db_file("architecture.db");
kfs_delete_db_file("registry.db");
kfs_delete_db_file("artifacts.db");

GameDB* db;
int rc = kfs_init(&db, "artifacts.db", "architecture.db", "registry.db")

kfs_close(db);
db = NULL; // Important!

*/


/* Helper function to get user_id by username */
static int get_user_id(GameDB* db, const char* username, int* user_id) {
    const char* sql = "SELECT id FROM Users WHERE username = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->registry_db, sql, -1, &stmt, NULL);
    if (rc != KFS_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db->registry_db));
        return rc;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id = sqlite3_column_int(stmt, 0);
    } else {
        sqlite3_finalize(stmt);
        return KFS_NOTFOUND;
    }
    sqlite3_finalize(stmt);
    return KFS_OK;
}


/**
 * @brief Helper function to get user_id by username. Internal use.
 * @param db GameDB handle (needs registry_db connection).
 * @param username The username to look up.
 * @param user_id Output parameter for the user's ID.
 * @return KFS_OK if found, KFS_NOTFOUND if not found, SQLite error code otherwise.
 */
static int get_user_id_by_name(GameDB* db, const char* username, int* user_id) {
    if (!db || !db->registry_db || !username || !user_id) {
        return KFS_INVALID_ARGUMENT; // Or KFS_INTERNAL if called improperly
    }
    *user_id = -1; // Initialize output

    const char* sql = "SELECT id FROM Users WHERE username = ?;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->registry_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] get_user_id_by_name - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        return rc;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *user_id = sqlite3_column_int(stmt, 0);
        rc = KFS_OK; // Found
    } else if (rc == SQLITE_DONE) {
        // Username not found
        rc = KFS_NOTFOUND;
    } else {
        // DB error during step
        fprintf(stderr, "[ERROR] get_user_id_by_name - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        // rc holds the error code
    }

    sqlite3_finalize(stmt);
    return rc;
}


/* ============================================================================== */
/* ==                       STATIC HELPER FUNCTIONS                          == */
/* ============================================================================== */

/* Get current ISO 8601 timestamp */
static char* get_current_timestamp() {
    // Keep the existing implementation using gmtime and strftime
    time_t now = time(NULL);
    // Use UTC time for consistency
    struct tm* tm_info = gmtime(&now); // Use gmtime for UTC
    if (tm_info == NULL) {
        return NULL; // time() or gmtime() failed
    }
    // Allocate enough space for "YYYY-MM-DDTHH:MM:SSZ\0"
    char* buf = (char*)KFS_MALLOC(21);
    if (buf == NULL) {
        return NULL;
    }
    strftime(buf, 21, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return buf;
}

/* Helper function to execute SQL and handle errors */
static int exec_sql(sqlite3* db, const char* sql, const char* db_name) {
    char* errMsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] SQL error in %s database (%s): %s\n", db_name, sql, errMsg);
        sqlite3_free(errMsg);
    }
    return rc;
}

// --- NEW Helper: Check group membership (single level for now) ---
static int is_user_in_group(GameDB* db, int user_actor_id, int group_actor_id) {
    if (!db || !db->registry_db || user_actor_id <= 0 || group_actor_id <= 0) {
        return 0; // Indicate false or error
    }
    const char* sql = "SELECT 1 FROM GroupMembers WHERE group_actor_id = ? AND member_actor_id = ? LIMIT 1;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->registry_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] is_user_in_group - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        return 0; // Indicate error / false
    }
    sqlite3_bind_int(stmt, 1, group_actor_id);
    sqlite3_bind_int(stmt, 2, user_actor_id);

    int is_member = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        is_member = 1; // Found a row, user is in the group
    } else if (rc != SQLITE_DONE) {
        // Error during step
        fprintf(stderr, "[ERROR] is_user_in_group - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        // Fall through to return 0
    }
    // If SQLITE_DONE, is_member remains 0 (not found)

    sqlite3_finalize(stmt);
    return is_member;
}

// --- NEW Helper: Get Actor ID and check activity by UUID ---
static int get_active_actor_id_by_uuid(GameDB* db, uint64_t actor_uuid, int* actor_id) {
     if (!db || !db->registry_db || actor_uuid == 0 || !actor_id) {
        return KFS_INVALID_ARGUMENT;
    }
    *actor_id = -1;
    const char* sql = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->registry_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_finalize(stmt); return rc; }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)actor_uuid);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int is_active = sqlite3_column_int(stmt, 1);
        if (is_active) {
            *actor_id = sqlite3_column_int(stmt, 0);
            rc = KFS_OK;
        } else {
            // Actor found but is inactive
            rc = KFS_PERMISSION_DENIED; // Treat inactive user as permission denied
        }
    } else if (rc == SQLITE_DONE) {
        rc = KFS_NOTFOUND; // UUID not found
    }
    // else: rc holds the SQLite error

    sqlite3_finalize(stmt);
    return rc;
}
/* ============================================================================== */
/* ==                       DATABASE INITIALIZATION                          == */
/* ============================================================================== */

/**
 * @brief Initializes the KFS databases (Registry, Architecture, Artifacts).
 * Creates tables and indexes according to the v2.0 Actor/Domain security model if they don't exist.
 * Opens connections to the three database files.
 *
 * @param db_handle Output parameter, pointer to the GameDB struct pointer. Will be allocated by the function.
 * @param artifacts_path Path to the artifacts data database file.
 * @param arch_path Path to the architecture/metadata database file.
 * @param registry_path Path to the actor/security registry database file.
 * @return KFS_OK on success, KFS_NOMEM on allocation failure, KFS_INVALID_ARGUMENT,
 *         or SQLite error code (e.g., KFS_CANTOPEN, KFS_ERROR) on failure.
 */
int kfs_init(GameDB** db_handle, const char* artifacts_path, const char* arch_path, const char* registry_path) {
    int mem_rc = kfs_mem_init(NULL);
    if (mem_rc != KFS_OK) {
        return mem_rc;
    }

    // --- Input Validation ---
    if (!db_handle || !artifacts_path || strlen(artifacts_path) == 0 ||
        !arch_path || strlen(arch_path) == 0 ||
        !registry_path || strlen(registry_path) == 0) {
        fprintf(stderr, "[ERROR] kfs_init: Invalid NULL or empty database path provided.\n");
        return KFS_INVALID_ARGUMENT;
    }
    *db_handle = NULL; // Initialize output parameter

    if (g_kfs_mem.max_open_db > 0 && g_kfs_mem.open_db_count >= g_kfs_mem.max_open_db) {
        fprintf(stderr, "[ERROR] kfs_init: open GameDB cap reached (%d)\n", g_kfs_mem.max_open_db);
        return KFS_MISUSE;
    }

    // --- Allocate GameDB Struct ---
    GameDB* db = (GameDB*)KFS_MALLOC(sizeof(GameDB));
    if (!db) {
        fprintf(stderr, "[ERROR] kfs_init: Failed to allocate memory for GameDB handle.\n");
        return KFS_NOMEM;
    }
    db->artifacts_db = NULL;
    db->arch_db = NULL;
    db->registry_db = NULL;

    int rc = KFS_OK; // Overall status

    // --- Open Database Connections ---
    rc = sqlite3_open_v2(artifacts_path, &db->artifacts_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) { /* Handle error & cleanup */ goto init_error; }
    rc = sqlite3_open_v2(arch_path, &db->arch_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) { /* Handle error & cleanup */ goto init_error; }
    rc = sqlite3_open_v2(registry_path, &db->registry_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) { /* Handle error & cleanup */ goto init_error; }

    // --- Enable Foreign Key Constraints ---
    if (exec_sql(db->registry_db, "PRAGMA foreign_keys = ON;", "registry") != KFS_OK) goto init_error;
    if (exec_sql(db->arch_db, "PRAGMA foreign_keys = ON;", "architecture") != KFS_OK) goto init_error;

    // --- Create Tables (IF NOT EXISTS) ---

    // == REGISTRY DATABASE ==
    const char* actors_sql =
        "CREATE TABLE IF NOT EXISTS Actors ("
        "id INTEGER PRIMARY KEY, "
        "uuid INTEGER UNIQUE NOT NULL, "
        "actor_type TEXT NOT NULL CHECK(actor_type IN ('USER', 'GROUP', 'COMPANY', 'SYSTEM')), "
        "name TEXT NOT NULL, "
        "role TEXT, "
        "is_active INTEGER DEFAULT 1 NOT NULL"
        ");";
    if (exec_sql(db->registry_db, actors_sql, "registry") != KFS_OK) goto init_error;

    const char* group_members_sql =
        "CREATE TABLE IF NOT EXISTS GroupMembers ("
        "group_actor_id INTEGER NOT NULL, "
        "member_actor_id INTEGER NOT NULL, "
        "PRIMARY KEY (group_actor_id, member_actor_id), "
        "FOREIGN KEY(group_actor_id) REFERENCES Actors(id) ON DELETE CASCADE, "
        "FOREIGN KEY(member_actor_id) REFERENCES Actors(id) ON DELETE CASCADE"
        ");";
     if (exec_sql(db->registry_db, group_members_sql, "registry") != KFS_OK) goto init_error;

    const char* domains_sql =
        "CREATE TABLE IF NOT EXISTS Domains ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT UNIQUE NOT NULL, "
        "owner_actor_id INTEGER NOT NULL, "
        "creator_uuid INTEGER NOT NULL, "
        "created_at TEXT, "
        "description TEXT, "
        "FOREIGN KEY(owner_actor_id) REFERENCES Actors(id) ON DELETE RESTRICT"
        ");";
    if (exec_sql(db->registry_db, domains_sql, "registry") != KFS_OK) goto init_error;

    const char* domain_actors_sql =
        "CREATE TABLE IF NOT EXISTS DomainActors ("
        "domain_id INTEGER NOT NULL, "
        "actor_id INTEGER NOT NULL, "
        "PRIMARY KEY (domain_id, actor_id), "
        "FOREIGN KEY(domain_id) REFERENCES Domains(id) ON DELETE CASCADE, "
        "FOREIGN KEY(actor_id) REFERENCES Actors(id) ON DELETE CASCADE"
        ");";
    if (exec_sql(db->registry_db, domain_actors_sql, "registry") != KFS_OK) goto init_error;

    const char* security_schemes_sql =
        "CREATE TABLE IF NOT EXISTS SecuritySchemes ("
        "id INTEGER PRIMARY KEY, "
        "domain_id INTEGER NOT NULL, " // ADDED
        "name TEXT NOT NULL, "         // Consider UNIQUE(name, domain_id)? For now, name is unique globally.
        "owner_actor_id INTEGER NOT NULL, "
        "creator_uuid INTEGER NOT NULL, "
        "created_at TEXT, "
        "UNIQUE(name, domain_id), " // Ensure scheme name is unique within a domain
        "FOREIGN KEY(owner_actor_id) REFERENCES Actors(id) ON DELETE RESTRICT, "
        "FOREIGN KEY(domain_id) REFERENCES Domains(id) ON DELETE CASCADE" // ADDED
        ");";
    if (exec_sql(db->registry_db, security_schemes_sql, "registry") != KFS_OK) goto init_error;

    const char* scheme_allowed_actors_sql =
        "CREATE TABLE IF NOT EXISTS SchemeAllowedActors ("
        "security_scheme_id INTEGER NOT NULL, " // Renamed from scheme_id for clarity
        "actor_id INTEGER NOT NULL, "          // Renamed from allowed_actor_id
        "can_read INTEGER DEFAULT 0 NOT NULL, "
        "can_write INTEGER DEFAULT 0 NOT NULL, "
        "can_delete INTEGER DEFAULT 0 NOT NULL, "
        "PRIMARY KEY(security_scheme_id, actor_id), "
        "FOREIGN KEY(security_scheme_id) REFERENCES SecuritySchemes(id) ON DELETE CASCADE, "
        "FOREIGN KEY(actor_id) REFERENCES Actors(id) ON DELETE CASCADE"
        ");";
    if (exec_sql(db->registry_db, scheme_allowed_actors_sql, "registry") != KFS_OK) goto init_error;


    // == ARCHITECTURE DATABASE ==
    const char* artifacts_meta_sql =
        "CREATE TABLE IF NOT EXISTS Artifacts ("
        "id INTEGER PRIMARY KEY, "
        "domain_id INTEGER NOT NULL, "      // ADDED
        "type TEXT NOT NULL, "
        "name TEXT NOT NULL, "
        "format TEXT, "
        "creator_uuid INTEGER NOT NULL, "
        "owner_actor_id INTEGER NOT NULL, "
        "security_scheme_id INTEGER, "
        "created_at TEXT, "
        "updated_at TEXT"
        ");";
    if (exec_sql(db->arch_db, artifacts_meta_sql, "architecture") != KFS_OK) goto init_error;

    const char* notes_sql =
        "CREATE TABLE IF NOT EXISTS Notes ("
        "id INTEGER PRIMARY KEY, "
        "domain_id INTEGER NOT NULL, "      // ADDED
        "creator_uuid INTEGER NOT NULL, "
        "owner_actor_id INTEGER NOT NULL, "
        "security_scheme_id INTEGER, "
        "content TEXT, "
        "created_at TEXT, "
        "updated_at TEXT"
        ");";
    if (exec_sql(db->arch_db, notes_sql, "architecture") != KFS_OK) goto init_error;

    const char* topics_sql =
        "CREATE TABLE IF NOT EXISTS Topics ("
        "id INTEGER PRIMARY KEY, "
        "domain_id INTEGER NOT NULL, "      // ADDED
        "creator_uuid INTEGER NOT NULL, "
        "owner_actor_id INTEGER NOT NULL, "
        "security_scheme_id INTEGER, "
        "name TEXT NOT NULL, "              // Consider UNIQUE(name, domain_id)
        "created_at TEXT, "                 // ADDED for consistency
        "updated_at TEXT, "                 // ADDED for consistency
        "UNIQUE(name, domain_id)"           // ADDED
        ");";
    if (exec_sql(db->arch_db, topics_sql, "architecture") != KFS_OK) goto init_error;

    const char* epics_sql =
        "CREATE TABLE IF NOT EXISTS Epics ("
        "id INTEGER PRIMARY KEY, "
        "domain_id INTEGER NOT NULL, "      // ADDED
        "creator_uuid INTEGER NOT NULL, "
        "owner_actor_id INTEGER NOT NULL, "
        "security_scheme_id INTEGER, "
        "name TEXT NOT NULL, "              // Consider UNIQUE(name, domain_id)
        "description TEXT, "                // ADDED description column
        "created_at TEXT, "                 // ADDED for consistency
        "updated_at TEXT, "                 // ADDED for consistency
        "UNIQUE(name, domain_id)"           // ADDED
        ");";
    if (exec_sql(db->arch_db, epics_sql, "architecture") != KFS_OK) goto init_error;

    const char* entity_notes_sql =
        "CREATE TABLE IF NOT EXISTS EntityNotes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "entity_type TEXT NOT NULL CHECK(entity_type IN ('Artifact', 'Topic', 'Epic')), "
        "entity_id INTEGER NOT NULL, "
        "note_id INTEGER NOT NULL, "
        "UNIQUE(entity_type, entity_id, note_id), "
        "FOREIGN KEY(note_id) REFERENCES Notes(id) ON DELETE CASCADE"
        ");";
    if (exec_sql(db->arch_db, entity_notes_sql, "architecture") != KFS_OK) goto init_error;

    const char* related_topics_sql =
        "CREATE TABLE IF NOT EXISTS RelatedTopics ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "topic_id INTEGER NOT NULL, "
        "related_topic_id INTEGER NOT NULL, "
        "is_subtopic INTEGER DEFAULT 0 NOT NULL, "
        "UNIQUE(topic_id, related_topic_id), "
        "FOREIGN KEY(topic_id) REFERENCES Topics(id) ON DELETE CASCADE, "
        "FOREIGN KEY(related_topic_id) REFERENCES Topics(id) ON DELETE CASCADE"
        ");";
    if (exec_sql(db->arch_db, related_topics_sql, "architecture") != KFS_OK) goto init_error;

    const char* topic_assignments_sql =
        "CREATE TABLE IF NOT EXISTS TopicAssignments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "artifact_id INTEGER NOT NULL, "
        "topic_id INTEGER NOT NULL, "
        "UNIQUE(artifact_id, topic_id), "
        "FOREIGN KEY(artifact_id) REFERENCES Artifacts(id) ON DELETE CASCADE, " // Cascade when artifact deleted
        "FOREIGN KEY(topic_id) REFERENCES Topics(id) ON DELETE CASCADE"       // Cascade when topic deleted
        ");";
    if (exec_sql(db->arch_db, topic_assignments_sql, "architecture") != KFS_OK) goto init_error;

    const char* epic_assignments_sql =
        "CREATE TABLE IF NOT EXISTS EpicAssignments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "topic_id INTEGER NOT NULL, "
        "epic_id INTEGER NOT NULL, "
        "UNIQUE(topic_id, epic_id), "
        "FOREIGN KEY(topic_id) REFERENCES Topics(id) ON DELETE CASCADE, " // Cascade when topic deleted
        "FOREIGN KEY(epic_id) REFERENCES Epics(id) ON DELETE CASCADE"     // Cascade when epic deleted
        ");";
    if (exec_sql(db->arch_db, epic_assignments_sql, "architecture") != KFS_OK) goto init_error;

    const char* related_epics_sql = // Added table for User File linking
        "CREATE TABLE IF NOT EXISTS RelatedEpics ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "epic_id1 INTEGER NOT NULL, "
        "epic_id2 INTEGER NOT NULL, "
        "UNIQUE(epic_id1, epic_id2), "
        "FOREIGN KEY(epic_id1) REFERENCES Epics(id) ON DELETE CASCADE, "
        "FOREIGN KEY(epic_id2) REFERENCES Epics(id) ON DELETE CASCADE"
        ");";
    if (exec_sql(db->arch_db, related_epics_sql, "architecture") != KFS_OK) goto init_error;


    // == ARTIFACTS DATABASE ==
    const char* assets_sql =
        "CREATE TABLE IF NOT EXISTS Assets ("
        "id INTEGER PRIMARY KEY, " // Must match architecture.db.Artifacts.id
        "data BLOB, "
        "text_data TEXT, "
        "metadata TEXT "
        ");";
    if (exec_sql(db->artifacts_db, assets_sql, "artifacts") != KFS_OK) goto init_error;


    // --- Create Indexes ---

    // == REGISTRY INDEXES ==
    const char* registry_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_actors_uuid ON Actors(uuid);"
        "CREATE INDEX IF NOT EXISTS idx_actors_type ON Actors(actor_type);"
        "CREATE INDEX IF NOT EXISTS idx_actors_name ON Actors(name);"
        "CREATE INDEX IF NOT EXISTS idx_groupmembers_group ON GroupMembers(group_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_groupmembers_member ON GroupMembers(member_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_domains_owner ON Domains(owner_actor_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_domains_name ON Domains(name);"           // ADDED
        "CREATE INDEX IF NOT EXISTS idx_domainactors_domain ON DomainActors(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_domainactors_actor ON DomainActors(actor_id);"   // ADDED
        "CREATE INDEX IF NOT EXISTS idx_securityschemes_domain ON SecuritySchemes(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_securityschemes_owner ON SecuritySchemes(owner_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_securityschemes_creator_uuid ON SecuritySchemes(creator_uuid);"
        "CREATE INDEX IF NOT EXISTS idx_securityschemes_name ON SecuritySchemes(name, domain_id);" // Updated for UNIQUE constraint
        "CREATE INDEX IF NOT EXISTS idx_schemeallowed_scheme ON SchemeAllowedActors(security_scheme_id);"
        "CREATE INDEX IF NOT EXISTS idx_schemeallowed_actor ON SchemeAllowedActors(actor_id);";
    if (exec_sql(db->registry_db, registry_index_sql, "registry") != KFS_OK) goto init_error;

    // == ARCHITECTURE INDEXES ==
    const char* arch_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_artifacts_domain ON Artifacts(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_artifacts_owner ON Artifacts(owner_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_artifacts_scheme ON Artifacts(security_scheme_id);"
        "CREATE INDEX IF NOT EXISTS idx_artifacts_creator_uuid ON Artifacts(creator_uuid);"
        "CREATE INDEX IF NOT EXISTS idx_artifacts_name ON Artifacts(name);"
        "CREATE INDEX IF NOT EXISTS idx_artifacts_type ON Artifacts(type);"
        "CREATE INDEX IF NOT EXISTS idx_notes_domain ON Notes(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_notes_owner ON Notes(owner_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_notes_scheme ON Notes(security_scheme_id);"
        "CREATE INDEX IF NOT EXISTS idx_notes_creator_uuid ON Notes(creator_uuid);"
        "CREATE INDEX IF NOT EXISTS idx_topics_domain ON Topics(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_topics_owner ON Topics(owner_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_topics_scheme ON Topics(security_scheme_id);"
        "CREATE INDEX IF NOT EXISTS idx_topics_creator_uuid ON Topics(creator_uuid);"
        "CREATE INDEX IF NOT EXISTS idx_topics_name ON Topics(name, domain_id);" // Updated
        "CREATE INDEX IF NOT EXISTS idx_epics_domain ON Epics(domain_id);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_epics_owner ON Epics(owner_actor_id);"
        "CREATE INDEX IF NOT EXISTS idx_epics_scheme ON Epics(security_scheme_id);"
        "CREATE INDEX IF NOT EXISTS idx_epics_creator_uuid ON Epics(creator_uuid);"
        "CREATE INDEX IF NOT EXISTS idx_epics_name ON Epics(name, domain_id);" // Updated
        "CREATE INDEX IF NOT EXISTS idx_entity_notes_lookup ON EntityNotes(entity_type, entity_id);"
        "CREATE INDEX IF NOT EXISTS idx_entity_notes_note_id ON EntityNotes(note_id);"
        "CREATE INDEX IF NOT EXISTS idx_related_topics_topic ON RelatedTopics(topic_id);"
        "CREATE INDEX IF NOT EXISTS idx_related_topics_related ON RelatedTopics(related_topic_id);"
        "CREATE INDEX IF NOT EXISTS idx_topic_assignments_artifact ON TopicAssignments(artifact_id);"
        "CREATE INDEX IF NOT EXISTS idx_topic_assignments_topic ON TopicAssignments(topic_id);"
        "CREATE INDEX IF NOT EXISTS idx_epic_assignments_topic ON EpicAssignments(topic_id);"
        "CREATE INDEX IF NOT EXISTS idx_epic_assignments_epic ON EpicAssignments(epic_id);"
        "CREATE INDEX IF NOT EXISTS idx_related_epics1 ON RelatedEpics(epic_id1);" // ADDED
        "CREATE INDEX IF NOT EXISTS idx_related_epics2 ON RelatedEpics(epic_id2);"; // ADDED
    if (exec_sql(db->arch_db, arch_index_sql, "architecture") != KFS_OK) goto init_error;

    // == ARTIFACTS INDEXES ==
    // No additional indexes needed currently

    // --- Success ---
    *db_handle = db;
    g_kfs_mem.open_db_count++;
    fprintf(stdout, "[INFO] kfs_init: Database initialization successful (Schema v2.0).\n");
    return KFS_OK;

// --- Error Handling ---
init_error:
    fprintf(stderr, "[ERROR] kfs_init failed during schema creation or connection opening (rc=%d).\n", rc);
    if (db) {
        if(db->artifacts_db) sqlite3_close(db->artifacts_db);
        if(db->arch_db) sqlite3_close(db->arch_db);
        if(db->registry_db) sqlite3_close(db->registry_db);
        KFS_FREE(db);
    }
    *db_handle = NULL;
    return (rc == KFS_OK) ? KFS_ERROR : rc;
}
/* ============================================================================== */
/* ==                        MEMORY MANAGEMENT (User)                        == */
/* ============================================================================== */

/**
 * @brief Frees a pointer to a KFS entity struct and all of its dynamically allocated contents.
 * This is the recommended, unified function for cleaning up memory for any KFS struct
 * returned by the library (e.g., KFS_Actor, KFS_Topic, KFS_Asset).
 *
 * @param entity A void pointer to the KFS struct to be freed (e.g., a KFS_Topic*).
 * @param entity_type A string literal identifying the type of the struct, which MUST match
 *        one of the supported types: "KFS_Actor", "KFS_Note", "KFS_SecurityScheme",
 *        "KFS_Asset", "KFS_Topic", "KFS_Epic", "KFS_UserInfo". The function will print a
 *        warning if the type is unknown.
 */
void kfs_entity_free(void* entity, const char* entity_type) {
    if (!entity || !entity_type) {
        return; // Nothing to do
    }

    if (strcmp(entity_type, "KFS_Actor") == 0) {
        kfs_actor_free((KFS_Actor*)entity);
    } else if (strcmp(entity_type, "KFS_Note") == 0) {
        kfs_note_free((KFS_Note*)entity);
    } else if (strcmp(entity_type, "KFS_SecurityScheme") == 0) {
        kfs_security_scheme_free((KFS_SecurityScheme*)entity);
    } else if (strcmp(entity_type, "KFS_Asset") == 0) {
        kfs_asset_free((KFS_Asset*)entity);
    } else if (strcmp(entity_type, "KFS_Topic") == 0) {
        kfs_topic_free((KFS_Topic*)entity);
    } else if (strcmp(entity_type, "KFS_Epic") == 0) {
        kfs_epic_free((KFS_Epic*)entity);
    } else if (strcmp(entity_type, "KFS_UserInfo") == 0) {
        // kfs_user_info_free already takes a KFS_UserInfo*, so no cast needed inside the call.
        // It also handles both contents and the struct itself if it were allocated.
        // For consistency, let's assume it should free the pointer.
        kfs_user_info_free((KFS_UserInfo*)entity);
        kfs_mem_free(entity); // kfs_user_info_free only frees contents, so we free the struct ptr.
    }
    // Add other entity types here as they are created.
    else {
        fprintf(stderr, "[WARN] kfs_entity_free: Unknown entity type '%s'. Memory for the pointer was not freed.\n", entity_type);
        // We cannot safely free the pointer itself without knowing its type and how it was allocated.
    }
}

/**
 * @brief Frees memory allocated within a KFS_User struct (strings).
 * Does not free the struct pointer itself.
 * DEPRECATED if KFS_User struct is fully replaced by KFS_Actor.
 *
 * @param user Pointer to the KFS_User struct whose contents are to be freed.
 */
void kfs_user_free_contents(KFS_User* user) {
     if (!user) return;
     // user->uuid is uint64_t, no free needed
     kfs_mem_free(user->username); user->username = NULL;
     kfs_mem_free(user->role); user->role = NULL;
     // Reset other fields
     user->id = 0;
     user->uuid = 0;
     user->is_active = 0;
}

/**
 * @brief Frees memory allocated within a KFS_User struct (strings) AND the struct pointer itself.
 * DEPRECATED if KFS_User struct is fully replaced by KFS_Actor.
 *
 * @param user Pointer to the KFS_User struct to free. If NULL, the function does nothing.
 */
void kfs_user_free(KFS_User* user) {
    if (!user) return;
    kfs_user_free_contents(user);
    kfs_mem_free(user);
}

/**
 * @brief Frees memory allocated for the contents of a KFS_SecurityScheme struct,
 * including the name and the array of allowed actors with their internal strings.
 * Does not free the struct pointer itself.
 *
 * @param scheme Pointer to the KFS_SecurityScheme struct whose contents are to be freed.
 */
void kfs_security_scheme_free_contents(KFS_SecurityScheme* scheme) {
    if (!scheme) return;

    kfs_mem_free(scheme->name); scheme->name = NULL;
    kfs_mem_free(scheme->created_at); scheme->created_at = NULL;
    kfs_mem_free(scheme->updated_at); scheme->updated_at = NULL;

    if (scheme->allowed_actors) {
        for (int i = 0; i < scheme->allowed_actor_count; i++) {
            // Free strings inside each allowed_actor struct
            kfs_mem_free(scheme->allowed_actors[i].actor_name); scheme->allowed_actors[i].actor_name = NULL;
            kfs_mem_free(scheme->allowed_actors[i].actor_type); scheme->allowed_actors[i].actor_type = NULL;
            // Reset other fields (optional)
            scheme->allowed_actors[i].actor_id = 0;
            scheme->allowed_actors[i].actor_uuid = 0;
            scheme->allowed_actors[i].can_read = 0;
            scheme->allowed_actors[i].can_write = 0;
            scheme->allowed_actors[i].can_delete = 0;
        }
        kfs_mem_free(scheme->allowed_actors); // Free the array of structs itself
        scheme->allowed_actors = NULL;
    }
    scheme->allowed_actor_count = 0;

    // Reset other non-pointer fields (optional)
    scheme->id = 0;
    scheme->domain_id = 0;
    scheme->creator_uuid = 0;
    scheme->owner_actor_id = 0;
}

/**
 * @brief Frees memory allocated within a KFS_SecurityScheme struct (strings, arrays)
 * AND the struct pointer itself.
 *
 * @param scheme Pointer to the KFS_SecurityScheme struct to free. If NULL, does nothing.
 */
void kfs_security_scheme_free(KFS_SecurityScheme* scheme) {
    if (!scheme) return;
    kfs_security_scheme_free_contents(scheme);
    kfs_mem_free(scheme);
}


/**
 * @brief Frees memory allocated within a KFS_Note struct (strings).
 * Does not free the struct pointer itself.
 *
 * @param note Pointer to the KFS_Note struct whose contents are to be freed.
 */
void kfs_note_free_contents(KFS_Note* note) {
    if (!note) return;
    kfs_mem_free(note->content); note->content = NULL;
    kfs_mem_free(note->created_at); note->created_at = NULL;
    kfs_mem_free(note->updated_at); note->updated_at = NULL;
    // Reset other fields (optional)
    note->id = 0;
    note->domain_id = 0;
    note->creator_uuid = 0;
    note->owner_actor_id = 0;
    note->security_scheme_id = 0;
}

/**
 * @brief Frees memory allocated within a KFS_Note struct (strings)
 * AND the struct pointer itself.
 *
 * @param note Pointer to the KFS_Note struct to free. If NULL, the function does nothing.
 */
void kfs_note_free(KFS_Note* note) {
    if (!note) return;
    kfs_note_free_contents(note);
    kfs_mem_free(note);
}

/**
 * @brief Frees memory allocated within a KFS_Asset struct (strings, blob, arrays).
 * Handles the dynamically allocated list of notes (using kfs_note_free) and topics (strings).
 * Does not free the struct pointer itself.
 *
 * @param asset Pointer to the KFS_Asset struct whose contents are to be freed.
 */
void kfs_asset_free_contents(KFS_Asset* asset) {
    if (!asset) return;
    kfs_mem_free(asset->type); asset->type = NULL;
    kfs_mem_free(asset->name); asset->name = NULL;
    kfs_mem_free(asset->format); asset->format = NULL;
    kfs_mem_free(asset->data); asset->data = NULL; asset->data_size = 0;
    kfs_mem_free(asset->text_data); asset->text_data = NULL;
    kfs_mem_free(asset->metadata); asset->metadata = NULL;

    // Free array of topic name strings
    if (asset->topics) {
        for (int i = 0; i < asset->topic_count; i++) {
             kfs_mem_free(asset->topics[i]);
        }
        kfs_mem_free(asset->topics); // Free the array of pointers
        asset->topics = NULL;
    }
    asset->topic_count = 0;

    // Free array of note structs
    if (asset->notes) {
        for (int i = 0; i < asset->note_count; i++) {
            kfs_note_free(asset->notes[i]); // Use the full free for notes
        }
        kfs_mem_free(asset->notes); // Free the array of pointers
        asset->notes = NULL;
    }
    asset->note_count = 0;

    // Reset other non-pointer fields (optional)
    asset->id = 0;
    asset->creator_uuid = 0;
    asset->owner_actor_id = 0;
    asset->security_scheme_id = 0;
}

/**
 * @brief Frees memory allocated within a KFS_Asset struct (strings, blob, arrays)
 * AND the struct pointer itself.
 *
 * @param asset Pointer to the KFS_Asset struct to free. If NULL, the function does nothing.
 */
void kfs_asset_free(KFS_Asset* asset) {
    if (!asset) return;
    kfs_asset_free_contents(asset);
    kfs_mem_free(asset);
}

/**
 * @brief Frees an array of KFS_Asset structs and all memory allocated within each struct.
 *
 * @param assets Pointer to the array of KFS_Asset structs.
 * @param count The number of elements in the array.
 */
void kfs_assets_free(KFS_Asset* assets, int count) {
    if (!assets || count <= 0) return;
    for (int i = 0; i < count; i++) {
        kfs_asset_free_contents(&assets[i]); // Free contents of struct within array
    }
    kfs_mem_free(assets); // Free the array block itself
}

/**
 * @brief Frees memory allocated within a KFS_Topic struct (strings, arrays, notes).
 * Does not free the struct pointer itself.
 *
 * @param topic Pointer to the KFS_Topic struct whose contents are to be freed.
 */
void kfs_topic_free_contents(KFS_Topic* topic) {
     if (!topic) return;

    kfs_mem_free(topic->name); topic->name = NULL;
    kfs_mem_free(topic->created_at); topic->created_at = NULL; // If added
    kfs_mem_free(topic->updated_at); topic->updated_at = NULL; // If added

    // Free array of epic names (strings)
    if (topic->epics) {
        for (int i = 0; i < topic->epic_count; i++) {
            kfs_mem_free(topic->epics[i]);
        }
        kfs_mem_free(topic->epics); // Free the array of pointers
        topic->epics = NULL;
    }
    topic->epic_count = 0;

    // Free array of related topic names (strings)
    if (topic->related_topics) {
        for (int i = 0; i < topic->related_count; i++) {
            kfs_mem_free(topic->related_topics[i]);
        }
        kfs_mem_free(topic->related_topics); // Free the array of pointers
        topic->related_topics = NULL;
    }
    // Free the integer array for flags
    kfs_mem_free(topic->is_subtopic); topic->is_subtopic = NULL;
    topic->related_count = 0;


    // Free array of notes
    if (topic->notes) {
        for (int i = 0; i < topic->note_count; i++) {
            kfs_note_free(topic->notes[i]); // Use note-specific free function
        }
        kfs_mem_free(topic->notes); // Free the array of pointers
        topic->notes = NULL;
    }
    topic->note_count = 0;

    // Reset non-pointer fields (optional)
    topic->id = 0;
    topic->domain_id = 0;
    topic->creator_uuid = 0;
    topic->owner_actor_id = 0;
    topic->security_scheme_id = 0;
}

/**
 * @brief Frees memory allocated within a KFS_Topic struct (strings, arrays)
 * AND the struct pointer itself.
 *
 * @param topic Pointer to the KFS_Topic struct to free. If NULL, the function does nothing.
 */
void kfs_topic_free(KFS_Topic* topic) {
    if (!topic) return;
    kfs_topic_free_contents(topic); // Free the contents first
    kfs_mem_free(topic);                    // Then free the struct allocation
}

/**
 * @brief Frees an array of KFS_Topic structs and all memory allocated within each struct.
 *
 * @param topics Pointer to the array of KFS_Topic structs.
 * @param count The number of elements in the array.
 */
void kfs_topics_free(KFS_Topic* topics, int count) {
     if (!topics || count <= 0) return;
    for (int i = 0; i < count; i++) {
        kfs_topic_free_contents(&topics[i]); // Free contents of struct within array
    }
    kfs_mem_free(topics); // Free the array block itself
}

/**
 * @brief Frees memory allocated within a KFS_Epic struct (strings, notes array).
 * Does not free the struct pointer itself.
 *
 * @param epic Pointer to the KFS_Epic struct whose contents are to be freed.
 */
void kfs_epic_free_contents(KFS_Epic* epic) {
     if (!epic) return;

    kfs_mem_free(epic->name); epic->name = NULL;
    kfs_mem_free(epic->description); epic->description = NULL; // Free description if added
    kfs_mem_free(epic->created_at); epic->created_at = NULL; // Free created_at if added
    kfs_mem_free(epic->updated_at); epic->updated_at = NULL; // Free updated_at if added

    if (epic->notes) {
        for (int i = 0; i < epic->note_count; i++) {
            kfs_note_free(epic->notes[i]); // Use the specific free function for notes
        }
        kfs_mem_free(epic->notes); // Free the array of pointers
        epic->notes = NULL;
    }
    epic->note_count = 0;

    // Reset non-pointer fields (optional but good practice)
    epic->id = 0;
    epic->domain_id = 0;
    epic->creator_uuid = 0;
    epic->owner_actor_id = 0;
    epic->security_scheme_id = 0;
}

/**
 * @brief Frees memory allocated within a KFS_Epic struct (strings, arrays)
 * AND the struct pointer itself.
 *
 * @param epic Pointer to the KFS_Epic struct to free. If NULL, the function does nothing.
 */
void kfs_epic_free(KFS_Epic* epic) {
    if (!epic) return;
    kfs_epic_free_contents(epic); // Free the contents first
    kfs_mem_free(epic);                   // Then free the struct allocation
}

/**
 * @brief Frees an array of KFS_Epic structs and all memory allocated within each struct.
 *
 * @param epics Pointer to the array of KFS_Epic structs.
 * @param count The number of elements in the array.
 */
void kfs_epics_free(KFS_Epic* epics, int count) {
     if (!epics || count <= 0) return;
    for (int i = 0; i < count; i++) {
        kfs_epic_free_contents(&epics[i]); // Free contents of struct within array
    }
    kfs_mem_free(epics); // Free the array block itself
}

#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_CORE_H */
