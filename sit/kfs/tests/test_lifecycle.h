#ifndef KFS_TEST_LIFECYCLE_H
#define KFS_TEST_LIFECYCLE_H

#include "kfs_test_fixture.h"

int test_lifecycle_init_close(KFS_TestCtx* ctx);
int test_lifecycle_init_null_paths(KFS_TestCtx* ctx);
int test_lifecycle_double_close(KFS_TestCtx* ctx);
int test_lifecycle_bootstrap_admin(KFS_TestCtx* ctx);
int test_lifecycle_create_god_user(KFS_TestCtx* ctx);

#endif /* KFS_TEST_LIFECYCLE_H */