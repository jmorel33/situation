#ifndef KFS_TEST_SECURITY_SCHEMES_H
#define KFS_TEST_SECURITY_SCHEMES_H

#include "kfs_test_fixture.h"

int test_security_schemes_create(KFS_TestCtx* ctx);
int test_security_schemes_add_actor_grant(KFS_TestCtx* ctx);
int test_security_schemes_wrong_domain(KFS_TestCtx* ctx);
int test_security_schemes_free_contents(KFS_TestCtx* ctx);
int test_security_schemes_delete(KFS_TestCtx* ctx);

#endif /* KFS_TEST_SECURITY_SCHEMES_H */