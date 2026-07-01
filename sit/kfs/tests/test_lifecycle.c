#include "test_lifecycle.h"
#include "kfs_test_assert.h"
#include "kfs/kfs.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#define KFS_TEST_FILE_EXISTS(path) (_access((path), 0) == 0)
#else
#include <unistd.h>
#define KFS_TEST_FILE_EXISTS(path) (access((path), F_OK) == 0)
#endif

int test_lifecycle_init_close(KFS_TestCtx* ctx) {
    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);

    if (!KFS_TEST_FILE_EXISTS(ctx->artifacts_path)) {
        KFS_TEST_FAIL("artifacts.db missing after kfs_init");
    }
    if (!KFS_TEST_FILE_EXISTS(ctx->arch_path)) {
        KFS_TEST_FAIL("architecture.db missing after kfs_init");
    }
    if (!KFS_TEST_FILE_EXISTS(ctx->registry_path)) {
        KFS_TEST_FAIL("registry.db missing after kfs_init");
    }
    return 0;
}

int test_lifecycle_init_null_paths(KFS_TestCtx* ctx) {
    GameDB* db = NULL;

    (void)ctx;

    KFS_TEST_EQ_INT(kfs_init(&db, NULL, "artifacts.db", "registry.db"),
                    KFS_INVALID_ARGUMENT, "NULL artifacts path");
    KFS_TEST_PTR_NULL(db);

    KFS_TEST_EQ_INT(kfs_init(&db, "artifacts.db", "", "registry.db"),
                    KFS_INVALID_ARGUMENT, "empty architecture path");
    KFS_TEST_PTR_NULL(db);

    KFS_TEST_EQ_INT(kfs_init(NULL, "artifacts.db", "architecture.db", "registry.db"),
                    KFS_INVALID_ARGUMENT, "NULL db handle out-param");
    return 0;
}

int test_lifecycle_double_close(KFS_TestCtx* ctx) {
    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);

    GameDB* db = ctx->db;
    KFS_TEST_OK_CTX(ctx, "kfs_close", kfs_close(db));
    ctx->db = NULL;

    KFS_TEST_OK_CTX(ctx, "kfs_close", kfs_close(NULL));
    return 0;
}

int test_lifecycle_bootstrap_admin(KFS_TestCtx* ctx) {
    int is_member = 0;
    int extra_domain_id = -1;
    int* domain_ids = NULL;
    char** domain_names = NULL;
    int domain_count = 0;
    size_t i;

    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);

    if (kfs_test_bootstrap_admin(ctx) != 0) {
        KFS_TEST_FAIL("kfs_test_bootstrap_admin failed");
    }

    if (ctx->admin_uuid == 0 || ctx->admin_id <= 0 ||
        ctx->admin_group_id <= 0 || ctx->domain_id <= 0) {
        KFS_TEST_FAIL("bootstrap did not populate admin/domain ids");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_is_member_of", kfs_is_member_of(ctx->db, ctx->admin_id, ctx->admin_group_id, &is_member));
    if (!is_member) {
        KFS_TEST_FAIL("admin user is not a member of AdminGroup");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, "HarnessSecondary",
                               ctx->admin_group_id, "Second harness domain", &extra_domain_id));
    if (extra_domain_id <= 0) {
        KFS_TEST_FAIL("admin failed to add secondary domain");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_list_domains", kfs_list_domains(ctx->db, ctx->admin_uuid,
                                 &domain_ids, &domain_names, &domain_count));
    if (domain_count < 2) {
        KFS_TEST_FAIL("expected at least two domains after bootstrap");
    }

    for (i = 0; i < (size_t)domain_count; i++) {
        kfs_mem_free(domain_names[i]);
    }
    kfs_mem_free(domain_ids);
    kfs_mem_free(domain_names);
    return 0;
}

int test_lifecycle_create_god_user(KFS_TestCtx* ctx) {
    uint64_t god_uuid = 0;
    int god_id = -1;
    int god_domain_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    KFS_TEST_PTR_NONNULL(ctx->db);

    KFS_TEST_OK_CTX(ctx, "kfs_create_god_user", kfs_create_god_user(ctx->db, 0, "god_api_user", 1, &god_uuid, &god_id));
    if (god_uuid == 0 || god_id <= 0) {
        KFS_TEST_FAIL("kfs_create_god_user did not populate actor outputs");
    }

    /* AdminGroup membership is implied if the new god user can add a domain. */
    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, god_uuid, "GodApiDomain", god_id,
                               "domain created by kfs_create_god_user admin", &god_domain_id));
    if (god_domain_id <= 0) {
        KFS_TEST_FAIL("god user could not create a domain after kfs_create_god_user");
    }

    KFS_TEST_EQ_INT(kfs_create_god_user(ctx->db, 0, "god_api_user_2", 1, &god_uuid, &god_id),
                    KFS_CONSTRAINT, "second god user should be rejected");
    return 0;
}