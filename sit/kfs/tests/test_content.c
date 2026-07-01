#include "test_content.h"
#include "kfs_test_assert.h"
#include "kfs/kfs.h"

#include <stdlib.h>
#include <string.h>

#define KFS_TEST_ARTIFACT_NAME "h5_roundtrip_doc"
#define KFS_TEST_TOPIC_NAME    "h5_topic"
#define KFS_TEST_EPIC_NAME     "h5_epic"
#define KFS_TEST_NOTE_TEXT     "h5 note body"

static void kfs_test_free_artifact_fields(char* type, char* name, char* format,
                                          char* created_at, char* updated_at) {
    kfs_mem_free(type);
    kfs_mem_free(name);
    kfs_mem_free(format);
    kfs_mem_free(created_at);
    kfs_mem_free(updated_at);
}

static int kfs_test_content_setup(KFS_TestCtx* ctx) {
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    return 0;
}

int test_content_create_artifact(KFS_TestCtx* ctx) {
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
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    KFS_TEST_ARTIFACT_NAME, "document", "text",
                                    ctx->admin_id, -1, NULL, 0, "hello harness", NULL,
                                    &artifact_id));

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    if (!name || strcmp(name, KFS_TEST_ARTIFACT_NAME) != 0) {
        kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
        KFS_TEST_FAIL("artifact name mismatch on round-trip");
    }
    if (!type || strcmp(type, "document") != 0) {
        kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
        KFS_TEST_FAIL("artifact type mismatch on round-trip");
    }
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_content_link_asset(KFS_TestCtx* ctx) {
    int artifact_id = -1;
    int has_asset = 0;
    int owner_actor_id = -1;
    char* type = NULL;
    char* name = NULL;
    char* format = NULL;
    int security_scheme_id = -1;
    uint64_t creator_uuid = 0;
    char* created_at = NULL;
    char* updated_at = NULL;
    uint8_t* data = NULL;
    size_t data_size = 0;
    char* text_data = NULL;
    char* metadata = NULL;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h5_link_asset", "document", "text",
                                    ctx->admin_id, -1, NULL, 0, NULL, NULL,
                                    &artifact_id));
    KFS_TEST_OK_CTX(ctx, "kfs_link_asset_to_artifact", kfs_link_asset_to_artifact(ctx->db, ctx->admin_uuid, artifact_id,
                                          NULL, 0, "linked text payload", "{\"k\":\"v\"}"));

    KFS_TEST_OK_CTX(ctx, "kfs_get_artifact", kfs_get_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                 &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                 &creator_uuid, &created_at, &updated_at, &has_asset));
    if (!has_asset) {
        kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
        KFS_TEST_FAIL("expected has_asset after link_asset_to_artifact");
    }
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);

    KFS_TEST_OK_CTX(ctx, "kfs_get_asset_data", kfs_get_asset_data(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                   &data, &data_size, &text_data, &metadata));
    if (!text_data || strcmp(text_data, "linked text payload") != 0) {
        kfs_mem_free(data);
        kfs_mem_free(text_data);
        kfs_mem_free(metadata);
        KFS_TEST_FAIL("linked text_data mismatch");
    }
    kfs_mem_free(data);
    kfs_mem_free(text_data);
    kfs_mem_free(metadata);
    return 0;
}

int test_content_topic_assign(KFS_TestCtx* ctx) {
    int topic_id = -1;
    int artifact_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_topic", kfs_add_topic(ctx->db, ctx->admin_uuid, ctx->admin_id, KFS_TEST_TOPIC_NAME,
                              -1, ctx->domain_id, &topic_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h5_topic_doc", "document", "text",
                                    ctx->admin_id, -1, NULL, 0, "topic doc", NULL,
                                    &artifact_id));
    KFS_TEST_OK_CTX(ctx, "kfs_assign_topic_to_artifact_by_name", kfs_assign_topic_to_artifact_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                                     artifact_id, KFS_TEST_TOPIC_NAME));
    return 0;
}

int test_content_epic_topic_link(KFS_TestCtx* ctx) {
    int topic_id = -1;
    int epic_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_topic", kfs_add_topic(ctx->db, ctx->admin_uuid, ctx->admin_id, "h5_epic_topic",
                              -1, ctx->domain_id, &topic_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_epic", kfs_add_epic(ctx->db, ctx->admin_uuid, ctx->admin_id, KFS_TEST_EPIC_NAME,
                             "h5 epic for topic link", -1, ctx->domain_id, &epic_id));
    KFS_TEST_OK_CTX(ctx, "kfs_assign_epic_to_topic", kfs_assign_epic_to_topic(ctx->db, ctx->admin_uuid, topic_id, epic_id));
    return 0;
}

int test_content_note_assign(KFS_TestCtx* ctx) {
    int note_id = -1;
    int artifact_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_note", kfs_add_note(ctx->db, ctx->admin_uuid, ctx->admin_id, KFS_TEST_NOTE_TEXT,
                             -1, ctx->domain_id, &note_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h5_note_doc", "document", "text",
                                    ctx->admin_id, -1, NULL, 0, "note target", NULL,
                                    &artifact_id));
    KFS_TEST_OK_CTX(ctx, "kfs_assign_note", kfs_assign_note(ctx->db, ctx->admin_uuid, "Artifact", artifact_id, note_id));
    return 0;
}

int test_content_delete_artifact(KFS_TestCtx* ctx) {
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
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "h5_delete_me", "document", "text",
                                    ctx->admin_id, -1, NULL, 0, "delete me", NULL,
                                    &artifact_id));
    KFS_TEST_OK_CTX(ctx, "kfs_delete_artifact", kfs_delete_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id));
    KFS_TEST_NOTFOUND(kfs_get_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                       &owner_actor_id, &type, &name, &format, &security_scheme_id,
                                       &creator_uuid, &created_at, &updated_at, &has_asset));
    kfs_test_free_artifact_fields(type, name, format, created_at, updated_at);
    return 0;
}

int test_content_legacy_save_text(KFS_TestCtx* ctx) {
    int artifact_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_content_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    {
        int rc = kfs_save_text(ctx->db, "document", "h5_legacy_text", "text",
                               "legacy save_text path", NULL, NULL, 0,
                               ctx->admin_id, ctx->admin_id, -1, &artifact_id);
        /* Legacy path omits domain_id; schema v2 may reject with CONSTRAINT. */
        if (rc == KFS_OK) {
            if (artifact_id <= 0) {
                KFS_TEST_FAIL("kfs_save_text did not return artifact id");
            }
        } else if (rc != KFS_CONSTRAINT) {
            KFS_TEST_EQ_INT(rc, KFS_OK, "kfs_save_text legacy smoke");
        }
    }
    return 0;
}