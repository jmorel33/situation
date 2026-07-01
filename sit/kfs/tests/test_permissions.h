#ifndef KFS_TEST_PERMISSIONS_H
#define KFS_TEST_PERMISSIONS_H

#include "kfs_test_fixture.h"

int test_permissions_owner_only(KFS_TestCtx* ctx);
int test_permissions_admin_bypass(KFS_TestCtx* ctx);
int test_permissions_scheme_group_grant(KFS_TestCtx* ctx);
int test_permissions_scheme_without_grant(KFS_TestCtx* ctx);
int test_permissions_write_vs_delete(KFS_TestCtx* ctx);
int test_permissions_topic_read_gate(KFS_TestCtx* ctx);

#endif /* KFS_TEST_PERMISSIONS_H */