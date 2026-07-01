#include "test_actors.h"
#include "kfs_test_assert.h"

#include <string.h>

#define KFS_TEST_ACTOR_ALICE   "h2_alice"
#define KFS_TEST_ACTOR_BOB     "h2_bob"
#define KFS_TEST_GROUP_TEAM    "h2_team"

static int kfs_test_actors_setup(KFS_TestCtx* ctx) {
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    return 0;
}

int test_actors_add_user(KFS_TestCtx* ctx) {
    uint64_t user_uuid = 0;
    int user_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_ACTOR_ALICE,
                              "USER", 1, &user_uuid, &user_id));
    if (user_uuid == 0 || user_id <= 0) {
        KFS_TEST_FAIL("kfs_add_actor did not populate user uuid/id");
    }
    return 0;
}

int test_actors_add_group(KFS_TestCtx* ctx) {
    uint64_t group_uuid = 0;
    uint64_t user_uuid = 0;
    int group_id = -1;
    int user_id = -1;
    int is_member = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "GROUP", KFS_TEST_GROUP_TEAM,
                              "USER", 1, &group_uuid, &group_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_ACTOR_BOB,
                              "USER", 1, &user_uuid, &user_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_member_to_group", kfs_add_member_to_group(ctx->db, ctx->admin_uuid, group_id, user_id));

    KFS_TEST_OK_CTX(ctx, "kfs_is_member_of", kfs_is_member_of(ctx->db, user_id, group_id, &is_member));
    if (!is_member) {
        KFS_TEST_FAIL("expected bob to be a member of h2_team after add");
    }
    return 0;
}

int test_actors_is_member_of(KFS_TestCtx* ctx) {
    uint64_t group_uuid = 0;
    uint64_t user_uuid = 0;
    int group_id = -1;
    int user_id = -1;
    int is_member = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "GROUP", KFS_TEST_GROUP_TEAM,
                              "USER", 1, &group_uuid, &group_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_ACTOR_ALICE,
                              "USER", 1, &user_uuid, &user_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_member_to_group", kfs_add_member_to_group(ctx->db, ctx->admin_uuid, group_id, user_id));

    KFS_TEST_OK_CTX(ctx, "kfs_is_member_of", kfs_is_member_of(ctx->db, user_id, group_id, &is_member));
    if (!is_member) {
        KFS_TEST_FAIL("positive membership check failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_is_member_of", kfs_is_member_of(ctx->db, user_id, ctx->admin_group_id, &is_member));
    if (is_member) {
        KFS_TEST_FAIL("alice should not be in AdminGroup unless explicitly added");
    }
    return 0;
}

int test_actors_deactivate_blocks_action(KFS_TestCtx* ctx) {
    int domain_id = -1;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_set_actor_active", kfs_set_actor_active(ctx->db, ctx->admin_uuid, ctx->admin_uuid, 0));
    KFS_TEST_DENIED(kfs_add_domain(ctx->db, ctx->admin_uuid, "InactiveAdminDomain",
                                   ctx->admin_group_id, "should fail", &domain_id));
    return 0;
}

int test_actors_get_by_name(KFS_TestCtx* ctx) {
    uint64_t user_uuid = 0;
    int user_id = -1;
    KFS_Actor found;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_ACTOR_ALICE,
                              "USER", 1, &user_uuid, &user_id));

    memset(&found, 0, sizeof(found));
    KFS_TEST_OK_CTX(ctx, "kfs_get_actor_by_name", kfs_get_actor_by_name(ctx->db, ctx->admin_uuid, KFS_TEST_ACTOR_ALICE, &found));
    if (found.id != user_id || found.uuid != user_uuid) {
        kfs_actor_free_contents(&found);
        KFS_TEST_FAIL("get_actor_by_name returned wrong actor");
    }
    if (!found.actor_type || strcmp(found.actor_type, "USER") != 0) {
        kfs_actor_free_contents(&found);
        KFS_TEST_FAIL("expected USER actor_type");
    }
    kfs_actor_free_contents(&found);
    return 0;
}

int test_actors_remove_member(KFS_TestCtx* ctx) {
    uint64_t group_uuid = 0;
    uint64_t user_uuid = 0;
    int group_id = -1;
    int user_id = -1;
    int is_member = 0;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_actors_setup(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "GROUP", KFS_TEST_GROUP_TEAM,
                              "USER", 1, &group_uuid, &group_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", KFS_TEST_ACTOR_BOB,
                              "USER", 1, &user_uuid, &user_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_member_to_group", kfs_add_member_to_group(ctx->db, ctx->admin_uuid, group_id, user_id));

    KFS_TEST_OK_CTX(ctx, "kfs_remove_member_from_group", kfs_remove_member_from_group(ctx->db, ctx->admin_uuid, group_id, user_id));
    KFS_TEST_OK_CTX(ctx, "kfs_is_member_of", kfs_is_member_of(ctx->db, user_id, group_id, &is_member));
    if (is_member) {
        KFS_TEST_FAIL("bob should not be a member after remove");
    }
    return 0;
}