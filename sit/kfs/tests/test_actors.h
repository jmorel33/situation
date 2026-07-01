#ifndef KFS_TEST_ACTORS_H
#define KFS_TEST_ACTORS_H

#include "kfs_test_fixture.h"

int test_actors_add_user(KFS_TestCtx* ctx);
int test_actors_add_group(KFS_TestCtx* ctx);
int test_actors_is_member_of(KFS_TestCtx* ctx);
int test_actors_deactivate_blocks_action(KFS_TestCtx* ctx);
int test_actors_get_by_name(KFS_TestCtx* ctx);
int test_actors_remove_member(KFS_TestCtx* ctx);

#endif /* KFS_TEST_ACTORS_H */