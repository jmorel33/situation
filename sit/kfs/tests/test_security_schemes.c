#include "test_security_schemes.h"
#include "kfs_test_assert.h"

#include <string.h>

#define KFS_TEST_SCHEME_NAME   "H4TestScheme"
#define KFS_TEST_SCHEME_DELETE "H4DeleteScheme"
#define KFS_TEST_GRANT_USER    "h4_grant_user"
#define KFS_TEST_STRANGER      "h4_stranger"
#define KFS_TEST_SECOND_DOMAIN "H4SecondDomain"

static int kfs_test_schemes_setup(KFS_TestCtx* ctx) {
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    return 0;
}

int test_security_schemes_create(KFS_TestCtx* ctx) {
    int scheme_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_schemes_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, KFS_TEST_SCHEME_NAME, &scheme_id));
    if (scheme_id <= 0) {
        KFS_TEST_FAIL("kfs_create_security_scheme did not populate scheme id");
    }
    return 0;
}

int test_security_schemes_add_actor_grant(KFS_TestCtx* ctx) {
    uint64_t user_uuid = 0;
    int user_id = -1;
    int scheme_id = -1;
    KFS_SecurityScheme scheme;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_schemes_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_GRANT_USER,
                              "USER", 1, &user_uuid, &user_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H4GrantScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_scheme", kfs_add_actor_to_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id, scheme_id,
                                        user_id, 1, 1, 0));

    memset(&scheme, 0, sizeof(scheme));
    KFS_TEST_OK_CTX(ctx, "kfs_get_security_scheme", kfs_get_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                        scheme_id, &scheme));
    if (scheme.allowed_actor_count != 1) {
        kfs_security_scheme_free_contents(&scheme);
        KFS_TEST_FAIL("expected one allowed actor on scheme");
    }
    if (scheme.allowed_actors[0].actor_id != user_id ||
        !scheme.allowed_actors[0].can_read ||
        !scheme.allowed_actors[0].can_write ||
        scheme.allowed_actors[0].can_delete) {
        kfs_security_scheme_free_contents(&scheme);
        KFS_TEST_FAIL("scheme grant flags mismatch (want read+write, no delete)");
    }
    kfs_security_scheme_free_contents(&scheme);
    return 0;
}

int test_security_schemes_wrong_domain(KFS_TestCtx* ctx) {
    int scheme_id = -1;
    int other_domain_id = -1;
    KFS_SecurityScheme scheme;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_schemes_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H4WrongDomainScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_SECOND_DOMAIN,
                               ctx->admin_group_id, "second domain for scheme test",
                               &other_domain_id));

    memset(&scheme, 0, sizeof(scheme));
    KFS_TEST_NOTFOUND(kfs_get_security_scheme(ctx->db, ctx->admin_uuid, other_domain_id,
                                              scheme_id, &scheme));
    kfs_security_scheme_free_contents(&scheme);
    return 0;
}

int test_security_schemes_free_contents(KFS_TestCtx* ctx) {
    int scheme_id = -1;
    KFS_SecurityScheme scheme;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_schemes_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H4FreeScheme", &scheme_id));

    memset(&scheme, 0, sizeof(scheme));
    KFS_TEST_OK_CTX(ctx, "kfs_get_security_scheme", kfs_get_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                        scheme_id, &scheme));
    kfs_security_scheme_free_contents(&scheme);
    memset(&scheme, 0, sizeof(scheme));
    return 0;
}

int test_security_schemes_delete(KFS_TestCtx* ctx) {
    uint64_t stranger_uuid = 0;
    int stranger_id = -1;
    int scheme_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_schemes_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_STRANGER,
                              "USER", 1, &stranger_uuid, &stranger_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, KFS_TEST_SCHEME_DELETE, &scheme_id));

    KFS_TEST_DENIED(kfs_delete_security_scheme(ctx->db, stranger_uuid, ctx->domain_id, scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_delete_security_scheme", kfs_delete_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id, scheme_id));
    return 0;
}