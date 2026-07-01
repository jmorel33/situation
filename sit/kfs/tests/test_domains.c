#include "test_domains.h"
#include "kfs_test_assert.h"
#include "kfs/kfs.h"

#include <stdlib.h>
#include <string.h>

#define KFS_TEST_DOMAIN_EXTRA     "H3ExtraDomain"
#define KFS_TEST_DOMAIN_WALLED    "H3WalledGarden"
#define KFS_TEST_DOMAIN_RENAME    "H3RenameMe"
#define KFS_TEST_DOMAIN_RENAMED   "H3Renamed"
#define KFS_TEST_DOMAIN_DELETE    "H3DeleteMe"
#define KFS_TEST_MEMBER           "h3_member"
#define KFS_TEST_OUTSIDER         "h3_outsider"
#define KFS_TEST_REGULAR          "h3_regular"
#define KFS_TEST_ARTIFACT         "h3_firewall_doc"

static void kfs_test_free_domain_list(int* domain_ids, char** domain_names, int domain_count) {
    int i;
    if (domain_names) {
        for (i = 0; i < domain_count; i++) {
            kfs_mem_free(domain_names[i]);
        }
        kfs_mem_free(domain_names);
    }
    kfs_mem_free(domain_ids);
}

static void kfs_test_free_artifact_fields(char* type, char* name, char* format,
                                          char* created_at, char* updated_at) {
    kfs_mem_free(type);
    kfs_mem_free(name);
    kfs_mem_free(format);
    kfs_mem_free(created_at);
    kfs_mem_free(updated_at);
}

static int kfs_test_domains_setup(KFS_TestCtx* ctx) {
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    return 0;
}

static int kfs_test_domains_find_name(char** domain_names, int domain_count, const char* name) {
    int i;
    for (i = 0; i < domain_count; i++) {
        if (domain_names[i] && strcmp(domain_names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

int test_domains_add_and_list(KFS_TestCtx* ctx) {
    int extra_domain_id = -1;
    int* domain_ids = NULL;
    char** domain_names = NULL;
    int domain_count = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_domains_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_DOMAIN_EXTRA,
                               ctx->admin_group_id, "extra harness domain", &extra_domain_id));
    if (extra_domain_id <= 0) {
        KFS_TEST_FAIL("kfs_add_domain did not populate domain id");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_list_domains", kfs_list_domains(ctx->db, ctx->admin_uuid,
                                 &domain_ids, &domain_names, &domain_count));
    if (domain_count < 2) {
        kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
        KFS_TEST_FAIL("expected at least two domains");
    }
    if (!kfs_test_domains_find_name(domain_names, domain_count, KFS_TEST_DOMAIN_EXTRA)) {
        kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
        KFS_TEST_FAIL("new domain missing from kfs_list_domains");
    }

    kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
    return 0;
}

int test_domains_add_actor_to_domain(KFS_TestCtx* ctx) {
    uint64_t member_uuid = 0;
    int walled_domain_id = -1;
    int member_id = -1;
    int artifact_id = -1;
    int* domain_ids = NULL;
    char** domain_names = NULL;
    int domain_count = 0;
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
    if (kfs_test_domains_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_DOMAIN_WALLED,
                               ctx->admin_group_id, "walled test domain", &walled_domain_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_MEMBER,
                              "USER", 1, &member_uuid, &member_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_domain", kfs_add_actor_to_domain(ctx->db, ctx->admin_uuid, walled_domain_id, member_id));

    KFS_TEST_OK_CTX(ctx, "kfs_list_domains", kfs_list_domains(ctx->db, member_uuid, &domain_ids, &domain_names, &domain_count));
    if (!kfs_test_domains_find_name(domain_names, domain_count, KFS_TEST_DOMAIN_WALLED)) {
        kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
        KFS_TEST_FAIL("member should see walled domain after add_actor_to_domain");
    }
    kfs_test_free_domain_list(domain_ids, domain_names, domain_count);

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, walled_domain_id,
                                    KFS_TEST_ARTIFACT, "document", "text",
                                    member_id, -1, NULL, 0, "member-owned", NULL,
                                    &artifact_id));

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, member_uuid, walled_domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    if (owner_actor_id != member_id) {
        kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
        KFS_TEST_FAIL("member should read domain-scoped artifact after membership grant");
    }
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_domains_firewall_deny(KFS_TestCtx* ctx) {
    uint64_t outsider_uuid = 0;
    int walled_domain_id = -1;
    int outsider_id = -1;
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
    if (kfs_test_domains_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, "H3FirewallDomain",
                               ctx->admin_group_id, "firewall deny domain", &walled_domain_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_OUTSIDER,
                              "USER", 1, &outsider_uuid, &outsider_id));

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, walled_domain_id,
                                    "h3_secret_doc", "document", "text",
                                    ctx->admin_id, -1, NULL, 0, "secret", NULL,
                                    &artifact_id));

    KFS_TEST_DENIED(kfs_get_artifact(ctx->db, outsider_uuid, walled_domain_id, artifact_id,
                                   &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                   &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_domains_update_metadata(KFS_TestCtx* ctx) {
    int domain_id = -1;
    int* domain_ids = NULL;
    char** domain_names = NULL;
    int domain_count = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_domains_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_DOMAIN_RENAME,
                               ctx->admin_group_id, "rename me", &domain_id));
    KFS_TEST_OK_CTX(ctx, "kfs_update_domain", kfs_update_domain(ctx->db, ctx->admin_uuid, domain_id,
                                  KFS_TEST_DOMAIN_RENAMED, -1, "renamed domain"));

    KFS_TEST_OK_CTX(ctx, "kfs_list_domains", kfs_list_domains(ctx->db, ctx->admin_uuid,
                                 &domain_ids, &domain_names, &domain_count));
    if (!kfs_test_domains_find_name(domain_names, domain_count, KFS_TEST_DOMAIN_RENAMED)) {
        kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
        KFS_TEST_FAIL("renamed domain not visible in kfs_list_domains");
    }
    if (kfs_test_domains_find_name(domain_names, domain_count, KFS_TEST_DOMAIN_RENAME)) {
        kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
        KFS_TEST_FAIL("old domain name still listed after update");
    }

    kfs_test_free_domain_list(domain_ids, domain_names, domain_count);
    return 0;
}

int test_domains_delete_requires_admin(KFS_TestCtx* ctx) {
    uint64_t regular_uuid = 0;
    int domain_id = -1;
    int regular_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_domains_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_domain", kfs_add_domain(ctx->db, ctx->admin_uuid, KFS_TEST_DOMAIN_DELETE,
                               ctx->admin_group_id, "delete me", &domain_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_REGULAR,
                              "USER", 1, &regular_uuid, &regular_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_domain", kfs_add_actor_to_domain(ctx->db, ctx->admin_uuid, domain_id, regular_id));

    KFS_TEST_DENIED(kfs_delete_domain(ctx->db, regular_uuid, domain_id));
    KFS_TEST_OK_CTX(ctx, "kfs_delete_domain", kfs_delete_domain(ctx->db, ctx->admin_uuid, domain_id));
    return 0;
}