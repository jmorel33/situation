#include "test_permissions.h"
#include "kfs_test_assert.h"
#include "kfs/kfs.h"

#include <stdlib.h>
#include <string.h>

static void kfs_test_free_artifact_fields(char* type, char* name, char* format,
                                          char* created_at, char* updated_at) {
    kfs_mem_free(type);
    kfs_mem_free(name);
    kfs_mem_free(format);
    kfs_mem_free(created_at);
    kfs_mem_free(updated_at);
}

static int kfs_test_permissions_setup(KFS_TestCtx* ctx) {
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    return 0;
}

static int kfs_test_permissions_add_domain_user(KFS_TestCtx* ctx, const char* name,
                                                uint64_t* uuid, int* id) {
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", name, "USER", 1, uuid, id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_domain", kfs_add_actor_to_domain(ctx->db, ctx->admin_uuid, ctx->domain_id, *id));
    return 0;
}

int test_permissions_owner_only(KFS_TestCtx* ctx) {
    uint64_t alice_uuid = 0;
    uint64_t bob_uuid = 0;
    int alice_id = -1;
    int bob_id = -1;
    int artifact_id = -1;
    int owner_actor_id = -1;
    char* type = NULL;
    char* name = NULL;
    char* format = NULL;
    int security_scheme_id = -1;
    uint64_t creator_uuid = 0;
    char* created_at = NULL;
    char* updated_at = NULL;
    int has_asset = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    if (kfs_test_permissions_add_domain_user(ctx, "h6_alice", &alice_uuid, &alice_id) != 0) {
        KFS_TEST_FAIL("failed to create alice");
    }
    if (kfs_test_permissions_add_domain_user(ctx, "h6_bob", &bob_uuid, &bob_id) != 0) {
        KFS_TEST_FAIL("failed to create bob");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h6_owner_doc", "document", "text",
                                    alice_id, -1, NULL, 0, "alice owns this", NULL,
                                    &artifact_id));

    KFS_TEST_DENIED(kfs_get_artifact(ctx->db, bob_uuid, ctx->domain_id, artifact_id,
                                   &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                   &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, alice_uuid, ctx->domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_permissions_admin_bypass(KFS_TestCtx* ctx) {
    uint64_t alice_uuid = 0;
    int alice_id = -1;
    int artifact_id = -1;
    int owner_actor_id = -1;
    char* type = NULL;
    char* name = NULL;
    char* format = NULL;
    int security_scheme_id = -1;
    uint64_t creator_uuid = 0;
    char* created_at = NULL;
    char* updated_at = NULL;
    int has_asset = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    if (kfs_test_permissions_add_domain_user(ctx, "h6_admin_bypass_alice", &alice_uuid, &alice_id) != 0) {
        KFS_TEST_FAIL("failed to create alice");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h6_admin_bypass_doc", "document", "text",
                                    alice_id, -1, NULL, 0, "alice-owned", NULL,
                                    &artifact_id));

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_permissions_scheme_group_grant(KFS_TestCtx* ctx) {
    uint64_t bob_uuid = 0;
    uint64_t group_uuid = 0;
    int bob_id = -1;
    int group_id = -1;
    int scheme_id = -1;
    int artifact_id = -1;
    int owner_actor_id = -1;
    char* type = NULL;
    char* name = NULL;
    char* format = NULL;
    int security_scheme_id = -1;
    uint64_t creator_uuid = 0;
    char* created_at = NULL;
    char* updated_at = NULL;
    int has_asset = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "GROUP", "h6_readers",
                              "USER", 1, &group_uuid, &group_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", "h6_scheme_bob",
                              "USER", 1, &bob_uuid, &bob_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_member_to_group", kfs_add_member_to_group(ctx->db, ctx->admin_uuid, group_id, bob_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_domain", kfs_add_actor_to_domain(ctx->db, ctx->admin_uuid, ctx->domain_id, bob_id));

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H6GroupScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_scheme", kfs_add_actor_to_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id, scheme_id,
                                        group_id, 1, 0, 0));

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h6_scheme_doc", "document", "text",
                                    ctx->admin_id, scheme_id, NULL, 0, "scheme gated", NULL,
                                    &artifact_id));

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, bob_uuid, ctx->domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_permissions_scheme_without_grant(KFS_TestCtx* ctx) {
    uint64_t charlie_uuid = 0;
    int charlie_id = -1;
    int scheme_id = -1;
    int artifact_id = -1;
    int owner_actor_id = -1;
    char* type = NULL;
    char* name = NULL;
    char* format = NULL;
    int security_scheme_id = -1;
    uint64_t creator_uuid = 0;
    char* created_at = NULL;
    char* updated_at = NULL;
    int has_asset = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    if (kfs_test_permissions_add_domain_user(ctx, "h6_charlie", &charlie_uuid, &charlie_id) != 0) {
        KFS_TEST_FAIL("failed to create charlie");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H6NoGrantScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h6_no_grant_doc", "document", "text",
                                    ctx->admin_id, scheme_id, NULL, 0, "no grant", NULL,
                                    &artifact_id));

    KFS_TEST_DENIED(kfs_get_artifact(ctx->db, charlie_uuid, ctx->domain_id, artifact_id,
                                   &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                   &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_permissions_write_vs_delete(KFS_TestCtx* ctx) {
    uint64_t dave_uuid = 0;
    int dave_id = -1;
    int scheme_id = -1;
    int artifact_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    if (kfs_test_permissions_add_domain_user(ctx, "h6_dave", &dave_uuid, &dave_id) != 0) {
        KFS_TEST_FAIL("failed to create dave");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H6WriteOnlyScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_scheme", kfs_add_actor_to_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id, scheme_id,
                                        dave_id, 1, 1, 0));

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h6_write_only_doc", "document", "text",
                                    ctx->admin_id, scheme_id, NULL, 0, "write only grant", NULL,
                                    &artifact_id));

    KFS_TEST_OK_CTX(ctx, "kfs_update_artifact_name", kfs_update_artifact_name(ctx->db, dave_uuid, artifact_id, "h6_renamed_by_dave"));
    KFS_TEST_DENIED(kfs_delete_artifact(ctx->db, dave_uuid, ctx->domain_id, artifact_id));
    return 0;
}

int test_permissions_topic_read_gate(KFS_TestCtx* ctx) {
    uint64_t alice_uuid = 0;
    uint64_t bob_uuid = 0;
    int alice_id = -1;
    int bob_id = -1;
    int scheme_id = -1;
    int topic_id = -1;
    KFS_Asset* results = NULL;
    int result_count = 0;
    int rc;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_permissions_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    if (kfs_test_permissions_add_domain_user(ctx, "h6_topic_alice", &alice_uuid, &alice_id) != 0) {
        KFS_TEST_FAIL("failed to create alice");
    }
    if (kfs_test_permissions_add_domain_user(ctx, "h6_topic_bob", &bob_uuid, &bob_id) != 0) {
        KFS_TEST_FAIL("failed to create bob");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "H6TopicScheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_scheme", kfs_add_actor_to_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id, scheme_id,
                                        alice_id, 1, 0, 0));
    KFS_TEST_OK_CTX(ctx, "kfs_add_topic", kfs_add_topic(ctx->db, ctx->admin_uuid, ctx->admin_id, "h6_gated_topic",
                              scheme_id, ctx->domain_id, &topic_id));

    KFS_TEST_DENIED(kfs_load_by_topic(ctx->db, bob_uuid, ctx->domain_id, "h6_gated_topic",
                                      &results, &result_count));
    if (results) {
        kfs_assets_free(results, result_count);
    }

    rc = kfs_load_by_topic(ctx->db, alice_uuid, ctx->domain_id, "h6_gated_topic",
                           &results, &result_count);
    if (rc != KFS_OK && rc != KFS_NOTFOUND) {
        if (results) {
            kfs_assets_free(results, result_count);
        }
        KFS_TEST_FAIL("alice should pass topic read gate (OK or empty NOTFOUND)");
    }
    if (results) {
        kfs_assets_free(results, result_count);
    }
    return 0;
}