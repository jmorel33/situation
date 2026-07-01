#include "kfs_test_fixture.h"
#include "kfs_test_assert.h"
#include "kfs_test_perf.h"
#include "kfs_test_timing.h"

#include <stdio.h>
#include <string.h>


#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define KFS_TEST_MKDIR(path) _mkdir(path)
#define KFS_TEST_UNLINK(path) _unlink(path)
#define KFS_TEST_RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define KFS_TEST_MKDIR(path) mkdir(path, 0700)
#define KFS_TEST_UNLINK(path) unlink(path)
#define KFS_TEST_RMDIR(path) rmdir(path)
#endif

#define KFS_TEST_TMP_ROOT "../tests/tmp"

static char g_kfs_test_suite_dir[512];
static unsigned g_kfs_test_db_serial = 0;
static int g_kfs_test_suite_active = 0;

static void kfs_test_ctx_clear(KFS_TestCtx* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

static int kfs_test_path_join(char* out, size_t out_sz, const char* dir, const char* leaf) {
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

static int kfs_test_path_join3(char* out, size_t out_sz, const char* dir,
                               const char* stem, unsigned serial, const char* suffix) {
    char leaf[128];
    snprintf(leaf, sizeof(leaf), "%s_%u%s", stem, serial, suffix);
    return kfs_test_path_join(out, out_sz, dir, leaf);
}

int kfs_test_fixture_suite_begin(void) {
    if (g_kfs_test_suite_active) {
        return 0;
    }

    if (KFS_TEST_MKDIR(KFS_TEST_TMP_ROOT) != 0) {
        /* May already exist — ignore. */
    }

#ifdef _WIN32
    snprintf(g_kfs_test_suite_dir, sizeof(g_kfs_test_suite_dir),
             KFS_TEST_TMP_ROOT "/kfs_%u", (unsigned)_getpid());
#else
    snprintf(g_kfs_test_suite_dir, sizeof(g_kfs_test_suite_dir),
             KFS_TEST_TMP_ROOT "/kfs_%d", (int)getpid());
#endif

    if (KFS_TEST_MKDIR(g_kfs_test_suite_dir) != 0) {
        fprintf(stderr, "[HARNESS] failed to create suite temp dir: %s\n", g_kfs_test_suite_dir);
        g_kfs_test_suite_dir[0] = '\0';
        return 1;
    }

    g_kfs_test_db_serial = 0;
    g_kfs_test_suite_active = 1;
    return 0;
}

void kfs_test_fixture_suite_end(void) {
    unsigned i;

    if (!g_kfs_test_suite_active) {
        return;
    }

    for (i = 1; i <= g_kfs_test_db_serial; ++i) {
        char path[512];
        if (kfs_test_path_join3(path, sizeof(path), g_kfs_test_suite_dir,
                                "artifacts", i, ".db") == 0) {
            KFS_TEST_UNLINK(path);
        }
        if (kfs_test_path_join3(path, sizeof(path), g_kfs_test_suite_dir,
                                "architecture", i, ".db") == 0) {
            KFS_TEST_UNLINK(path);
        }
        if (kfs_test_path_join3(path, sizeof(path), g_kfs_test_suite_dir,
                                "registry", i, ".db") == 0) {
            KFS_TEST_UNLINK(path);
        }
    }

    KFS_TEST_RMDIR(g_kfs_test_suite_dir);
    g_kfs_test_suite_dir[0] = '\0';
    g_kfs_test_db_serial = 0;
    g_kfs_test_suite_active = 0;
}

int kfs_test_ctx_create(KFS_TestCtx* ctx) {
    unsigned serial;

    if (!ctx) return 1;

    if (!g_kfs_test_suite_active) {
        if (kfs_test_fixture_suite_begin() != 0) {
            return 1;
        }
    }

    kfs_test_ctx_clear(ctx);
    serial = g_kfs_test_db_serial + 1;

    snprintf(ctx->tmp_dir, sizeof(ctx->tmp_dir), "%s", g_kfs_test_suite_dir);

    if (kfs_test_path_join3(ctx->artifacts_path, sizeof(ctx->artifacts_path),
                            ctx->tmp_dir, "artifacts", serial, ".db") != 0 ||
        kfs_test_path_join3(ctx->arch_path, sizeof(ctx->arch_path),
                            ctx->tmp_dir, "architecture", serial, ".db") != 0 ||
        kfs_test_path_join3(ctx->registry_path, sizeof(ctx->registry_path),
                            ctx->tmp_dir, "registry", serial, ".db") != 0) {
        fprintf(stderr, "[HARNESS] temp dir path too long: %s\n", ctx->tmp_dir);
        return 1;
    }

    g_kfs_test_db_serial = serial;

    {
        int rc = KFS_TEST_TIMED_INT(ctx, "kfs_init",
                                    kfs_init(&ctx->db, ctx->artifacts_path, ctx->arch_path,
                                             ctx->registry_path));
        if (rc != KFS_OK) {
            fprintf(stderr, "[HARNESS] kfs_init failed: %d\n", rc);
            kfs_test_ctx_destroy(ctx);
            return 1;
        }
    }

    return 0;
}

int kfs_test_ctx_destroy(KFS_TestCtx* ctx) {
    if (!ctx) return 0;

    if (ctx->db) {
        KFS_TEST_TIMED_INT(ctx, "kfs_close", kfs_close(ctx->db));
        ctx->db = NULL;
    }

    return 0;
}

int kfs_test_require_bootstrap(KFS_TestCtx* ctx) {
    if (!ctx) return 1;
    if (ctx->admin_id > 0 && ctx->admin_uuid != 0) {
        return 0;
    }
    return kfs_test_bootstrap_admin(ctx);
}

int kfs_test_bootstrap_admin(KFS_TestCtx* ctx) {
    if (!ctx || !ctx->db) {
        return 1;
    }

    uint64_t admin_group_uuid = 0;
    int domain_id = -1;
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor",
                    kfs_add_actor(ctx->db, 0, "GROUP", KFS_TEST_ADMIN_GROUP_NAME,
                                  "SYSTEM", 1, &admin_group_uuid, &ctx->admin_group_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor",
                    kfs_add_actor(ctx->db, 0, "USER", KFS_TEST_ADMIN_USER_NAME,
                                  "USER", 1, &ctx->admin_uuid, &ctx->admin_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_member_to_group",
                    kfs_add_member_to_group(ctx->db, ctx->admin_uuid,
                                            ctx->admin_group_id, ctx->admin_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_domain",
                    kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_PRIMARY_DOMAIN,
                                   ctx->admin_group_id, "Harness primary domain", &domain_id));
    ctx->domain_id = domain_id;
    return 0;
}