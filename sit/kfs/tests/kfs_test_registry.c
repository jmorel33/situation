#include <stdio.h>
#include <string.h>

#include "kfs/kfs.h"
#include "kfs_test_registry.h"
#include "kfs_test_assert.h"
#include "test_lifecycle.h"
#include "test_actors.h"
#include "test_domains.h"
#include "test_security_schemes.h"
#include "test_content.h"
#include "test_permissions.h"
#include "test_perf.h"

static int test_harness_ping(KFS_TestCtx* ctx) {
    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);
    return 0;
}

static int test_harness_mem_init_idempotent(KFS_TestCtx* ctx) {
    (void)ctx;
    KFS_TEST_EQ_INT(kfs_mem_init(NULL), KFS_OK, "first kfs_mem_init");
    KFS_TEST_EQ_INT(kfs_mem_init(NULL), KFS_OK, "second kfs_mem_init");
    return 0;
}

#define KFS_TEST_ROUND8(n) ((((unsigned)(n)) + 7u) & ~7u)

static int kfs_test_integrity_ok(sqlite3* db) {
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return 1;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 1;
    }
    {
        const unsigned char* result = sqlite3_column_text(stmt, 0);
        int ok = (result != NULL && strcmp((const char*)result, "ok") == 0);
        sqlite3_finalize(stmt);
        return ok ? 0 : 1;
    }
}

static int test_harness_mem_roundtrip(KFS_TestCtx* ctx) {
    size_t baseline;
    void* ptr;
    size_t in_use;

    (void)ctx;
    baseline = kfs_mem_bytes_in_use();

    ptr = kfs_mem_alloc(16);
    if (!ptr) {
        KFS_TEST_FAIL("kfs_mem_alloc(16) returned NULL");
    }
    in_use = kfs_mem_bytes_in_use();
    if (in_use < baseline + 16) {
        fprintf(stderr, "[FAIL] %s:%d: expected in_use >= baseline+16 (got %zu, baseline %zu)\n",
                __FILE__, __LINE__, in_use, baseline);
        kfs_mem_free(ptr);
        return 1;
    }

    ptr = kfs_mem_realloc(ptr, 64);
    if (!ptr) {
        KFS_TEST_FAIL("kfs_mem_realloc grow to 64 returned NULL");
    }
    in_use = kfs_mem_bytes_in_use();
    if (in_use < baseline + 64) {
        fprintf(stderr, "[FAIL] %s:%d: expected in_use >= baseline+64 after grow\n",
                __FILE__, __LINE__);
        kfs_mem_free(ptr);
        return 1;
    }

    ptr = kfs_mem_realloc(ptr, 8);
    if (!ptr) {
        KFS_TEST_FAIL("kfs_mem_realloc shrink to 8 returned NULL");
    }
    in_use = kfs_mem_bytes_in_use();
    if (in_use < baseline + 8) {
        fprintf(stderr, "[FAIL] %s:%d: expected in_use >= baseline+8 after shrink\n",
                __FILE__, __LINE__);
        kfs_mem_free(ptr);
        return 1;
    }

    kfs_mem_free(ptr);
    if (kfs_mem_bytes_in_use() != in_use - 8) {
        fprintf(stderr, "[FAIL] %s:%d: kfs_mem_free did not restore expected in_use\n",
                __FILE__, __LINE__);
        return 1;
    }
    return 0;
}

static int test_harness_mem_roundup_matches_sqlite(KFS_TestCtx* ctx) {
    static const int sizes[] = { 1, 5, 8, 100, 65536 };
    size_t i;

    (void)ctx;
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        int n = sizes[i];
        void* ptr = sqlite3_malloc(n);
        int msize;

        if (!ptr) {
            fprintf(stderr, "[FAIL] %s:%d: sqlite3_malloc(%d) returned NULL\n",
                    __FILE__, __LINE__, n);
            return 1;
        }
        msize = sqlite3_msize(ptr);
        if (msize != (int)KFS_TEST_ROUND8(n)) {
            fprintf(stderr, "[FAIL] %s:%d: sqlite3_msize(%d)=%d, want ROUND8=%u\n",
                    __FILE__, __LINE__, n, msize, KFS_TEST_ROUND8(n));
            sqlite3_free(ptr);
            return 1;
        }
        sqlite3_free(ptr);
    }
    return 0;
}

static int test_harness_integrity_custom_alloc(KFS_TestCtx* ctx) {
    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);

    if (kfs_test_integrity_ok(ctx->db->registry_db) != 0) {
        KFS_TEST_FAIL("registry_db integrity_check failed");
    }
    if (kfs_test_integrity_ok(ctx->db->arch_db) != 0) {
        KFS_TEST_FAIL("arch_db integrity_check failed");
    }
    if (kfs_test_integrity_ok(ctx->db->artifacts_db) != 0) {
        KFS_TEST_FAIL("artifacts_db integrity_check failed");
    }
    return 0;
}

static size_t g_kfs_test_last_oom_request;

static void kfs_test_mem_oom_hook(size_t requested, void* userdata) {
    (void)userdata;
    g_kfs_test_last_oom_request = requested;
}

static int test_harness_mem_limit_triggers_oom(KFS_TestCtx* ctx) {
    size_t baseline;
    void* ptr;
    void* grown;

    (void)ctx;
    g_kfs_test_last_oom_request = 0;
    baseline = kfs_mem_bytes_in_use();

    kfs_mem_set_oom_callback(kfs_test_mem_oom_hook, NULL);
    kfs_mem_set_hard_limit_bytes(baseline + 4);

    ptr = kfs_mem_alloc(8);
    if (ptr != NULL) {
        kfs_mem_free(ptr);
        kfs_mem_set_hard_limit_bytes(0);
        kfs_mem_set_oom_callback(NULL, NULL);
        KFS_TEST_FAIL("kfs_mem_alloc(8) should fail when headroom is 4 bytes");
    }
    if (g_kfs_test_last_oom_request != 8) {
        kfs_mem_set_hard_limit_bytes(0);
        kfs_mem_set_oom_callback(NULL, NULL);
        fprintf(stderr, "[FAIL] %s:%d: OOM callback expected 8, got %zu\n",
                __FILE__, __LINE__, g_kfs_test_last_oom_request);
        return 1;
    }

    ptr = kfs_mem_alloc(4);
    if (!ptr) {
        kfs_mem_set_hard_limit_bytes(0);
        kfs_mem_set_oom_callback(NULL, NULL);
        KFS_TEST_FAIL("kfs_mem_alloc(4) should succeed with 4-byte headroom");
    }
    kfs_mem_set_hard_limit_bytes(kfs_mem_bytes_in_use());
    g_kfs_test_last_oom_request = 0;
    grown = kfs_mem_realloc(ptr, 64);
    if (grown != NULL) {
        kfs_mem_free(grown);
        kfs_mem_set_hard_limit_bytes(0);
        kfs_mem_set_oom_callback(NULL, NULL);
        KFS_TEST_FAIL("kfs_mem_realloc grow should fail at hard_limit");
    }
    if (g_kfs_test_last_oom_request != 64) {
        kfs_mem_free(ptr);
        kfs_mem_set_hard_limit_bytes(0);
        kfs_mem_set_oom_callback(NULL, NULL);
        fprintf(stderr, "[FAIL] %s:%d: realloc OOM callback expected 64, got %zu\n",
                __FILE__, __LINE__, g_kfs_test_last_oom_request);
        return 1;
    }

    kfs_mem_free(ptr);
    kfs_mem_set_hard_limit_bytes(0);
    kfs_mem_set_oom_callback(NULL, NULL);
    return 0;
}

static int test_harness_mem_reset_peak(KFS_TestCtx* ctx) {
    size_t baseline;
    void* ptr;

    (void)ctx;
    baseline = kfs_mem_bytes_in_use();
    ptr = kfs_mem_alloc(128);
    if (!ptr) {
        KFS_TEST_FAIL("kfs_mem_alloc(128) returned NULL");
    }
    if (kfs_mem_peak_bytes() < baseline + 128) {
        kfs_mem_free(ptr);
        fprintf(stderr, "[FAIL] %s:%d: peak should reflect alloc (peak=%zu baseline=%zu)\n",
                __FILE__, __LINE__, kfs_mem_peak_bytes(), baseline);
        return 1;
    }

    kfs_mem_reset_peak();
    if (kfs_mem_peak_bytes() != kfs_mem_bytes_in_use()) {
        kfs_mem_free(ptr);
        fprintf(stderr, "[FAIL] %s:%d: reset_peak mismatch (peak=%zu in_use=%zu)\n",
                __FILE__, __LINE__, kfs_mem_peak_bytes(), kfs_mem_bytes_in_use());
        return 1;
    }

    kfs_mem_free(ptr);
    return 0;
}

static int kfs_test_join_path(char* out, size_t out_sz, const char* dir, const char* leaf) {
    size_t dir_len = strlen(dir);
    size_t leaf_len = strlen(leaf);
    if (dir_len + 1 + leaf_len >= out_sz) {
        return 1;
    }
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len + 1);
    return 0;
}

static int test_harness_open_db_cap(KFS_TestCtx* ctx) {
    GameDB* db_a = NULL;
    GameDB* db_b = NULL;
    char art_a[512];
    char arch_a[512];
    char reg_a[512];
    char art_b[512];
    char arch_b[512];
    char reg_b[512];
    int rc;

    KFS_TEST_PTR_NONNULL(ctx);

    if (kfs_test_join_path(art_a, sizeof(art_a), ctx->tmp_dir, "cap_a_artifacts.db") != 0 ||
        kfs_test_join_path(arch_a, sizeof(arch_a), ctx->tmp_dir, "cap_a_architecture.db") != 0 ||
        kfs_test_join_path(reg_a, sizeof(reg_a), ctx->tmp_dir, "cap_a_registry.db") != 0 ||
        kfs_test_join_path(art_b, sizeof(art_b), ctx->tmp_dir, "cap_b_artifacts.db") != 0 ||
        kfs_test_join_path(arch_b, sizeof(arch_b), ctx->tmp_dir, "cap_b_architecture.db") != 0 ||
        kfs_test_join_path(reg_b, sizeof(reg_b), ctx->tmp_dir, "cap_b_registry.db") != 0) {
        KFS_TEST_FAIL("open_db_cap path buffer too small");
    }

    if (ctx->db) {
        kfs_close(ctx->db);
        ctx->db = NULL;
    }

    kfs_mem_set_max_open_db(1);

    rc = kfs_init(&db_a, art_a, arch_a, reg_a);
    if (rc != KFS_OK) {
        kfs_mem_set_max_open_db(0);
        fprintf(stderr, "[FAIL] %s:%d: first kfs_init under cap=1 returned %d\n",
                __FILE__, __LINE__, rc);
        return 1;
    }

    rc = kfs_init(&db_b, art_b, arch_b, reg_b);
    if (rc != KFS_MISUSE) {
        if (db_b) {
            kfs_close(db_b);
        }
        kfs_close(db_a);
        kfs_mem_set_max_open_db(0);
        fprintf(stderr, "[FAIL] %s:%d: second kfs_init expected KFS_MISUSE, got %d\n",
                __FILE__, __LINE__, rc);
        return 1;
    }

    kfs_close(db_a);
    kfs_mem_set_max_open_db(0);
    return 0;
}

static int test_harness_mem_stats_after_init(KFS_TestCtx* ctx) {
    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);
    if (kfs_mem_bytes_in_use() == 0) {
        fprintf(stderr, "[FAIL] %s:%d: expected kfs_mem_bytes_in_use() > 0 with open DB\n",
                __FILE__, __LINE__);
        return 1;
    }
    if (kfs_sqlite_bytes_in_use() == 0) {
        fprintf(stderr, "[FAIL] %s:%d: expected kfs_sqlite_bytes_in_use() > 0 with open DB\n",
                __FILE__, __LINE__);
        return 1;
    }
    return 0;
}

static int test_harness_version_string(KFS_TestCtx* ctx) {
    (void)ctx;
    const char* ver = kfs_get_version_string();
    char expected[32];

    KFS_TEST_PTR_NONNULL(ver);
    if (ver[0] == '\0') {
        KFS_TEST_FAIL("empty version string");
    }

    snprintf(expected, sizeof(expected), "%d.%d.%d",
             KFS_VERSION_MAJOR, KFS_VERSION_MINOR, KFS_VERSION_PATCH);
    if (strncmp(ver, expected, strlen(expected)) != 0) {
        fprintf(stderr, "[FAIL] %s:%d: version prefix mismatch (got '%s', want prefix '%s')\n",
                __FILE__, __LINE__, ver, expected);
        return 1;
    }
    return 0;
}

const KFS_TestCase kfs_test_cases[] = {
    { "harness", "ping", test_harness_ping },
    { "harness", "mem_init_idempotent", test_harness_mem_init_idempotent },
    { "harness", "mem_stats_after_init", test_harness_mem_stats_after_init },
    { "harness", "mem_roundtrip", test_harness_mem_roundtrip },
    { "harness", "mem_roundup_matches_sqlite", test_harness_mem_roundup_matches_sqlite },
    { "harness", "mem_limit_triggers_oom", test_harness_mem_limit_triggers_oom },
    { "harness", "mem_reset_peak", test_harness_mem_reset_peak },
    { "harness", "open_db_cap", test_harness_open_db_cap },
    { "harness", "integrity_custom_alloc", test_harness_integrity_custom_alloc },
    { "harness", "version_string", test_harness_version_string },
    { "lifecycle", "init_close", test_lifecycle_init_close },
    { "lifecycle", "init_null_paths", test_lifecycle_init_null_paths },
    { "lifecycle", "double_close", test_lifecycle_double_close },
    { "lifecycle", "bootstrap_admin", test_lifecycle_bootstrap_admin },
    { "lifecycle", "create_god_user", test_lifecycle_create_god_user },
    { "actors", "add_user", test_actors_add_user },
    { "actors", "add_group", test_actors_add_group },
    { "actors", "is_member_of", test_actors_is_member_of },
    { "actors", "deactivate_blocks_action", test_actors_deactivate_blocks_action },
    { "actors", "get_by_name", test_actors_get_by_name },
    { "actors", "remove_member", test_actors_remove_member },
    { "domains", "add_and_list", test_domains_add_and_list },
    { "domains", "add_actor_to_domain", test_domains_add_actor_to_domain },
    { "domains", "firewall_deny", test_domains_firewall_deny },
    { "domains", "update_metadata", test_domains_update_metadata },
    { "domains", "delete_requires_admin", test_domains_delete_requires_admin },
    { "security_schemes", "create", test_security_schemes_create },
    { "security_schemes", "add_actor_grant", test_security_schemes_add_actor_grant },
    { "security_schemes", "wrong_domain", test_security_schemes_wrong_domain },
    { "security_schemes", "free_contents", test_security_schemes_free_contents },
    { "security_schemes", "delete", test_security_schemes_delete },
    { "content", "create_artifact", test_content_create_artifact },
    { "content", "link_asset", test_content_link_asset },
    { "content", "topic_assign", test_content_topic_assign },
    { "content", "epic_topic_link", test_content_epic_topic_link },
    { "content", "note_assign", test_content_note_assign },
    { "content", "delete_artifact", test_content_delete_artifact },
    { "content", "legacy_save_text", test_content_legacy_save_text },
    { "permissions", "owner_only", test_permissions_owner_only },
    { "permissions", "admin_bypass", test_permissions_admin_bypass },
    { "permissions", "scheme_group_grant", test_permissions_scheme_group_grant },
    { "permissions", "scheme_without_grant", test_permissions_scheme_without_grant },
    { "permissions", "write_vs_delete", test_permissions_write_vs_delete },
    { "permissions", "topic_read_gate", test_permissions_topic_read_gate },
    { "perf", "permission_check", test_perf_permission_check },
    { "perf", "blob_read_small", test_perf_blob_read_small },
    { "perf", "blob_read_medium", test_perf_blob_read_medium },
    { "perf", "blob_read_large_glb", test_perf_blob_read_large_glb },
    { "perf", "blob_read_large_wav", test_perf_blob_read_large_wav },
    { "perf", "blob_ingest_large", test_perf_blob_ingest_large },
    { "perf", "blob_roundtrip_checksum", test_perf_blob_roundtrip_checksum },
    { "perf", "load_by_topic_textures", test_perf_load_by_topic_textures },
    { "perf", "load_by_topic_models", test_perf_load_by_topic_models },
    { "perf", "load_by_epic_geometry", test_perf_load_by_epic_geometry },
    { "perf", "bulk_ingest_all_props", test_perf_bulk_ingest_all_props },
};

const size_t kfs_test_case_count = sizeof(kfs_test_cases) / sizeof(kfs_test_cases[0]);