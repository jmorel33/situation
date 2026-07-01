#ifndef KFS_TEST_DOMAINS_H
#define KFS_TEST_DOMAINS_H

#include "kfs_test_fixture.h"

int test_domains_add_and_list(KFS_TestCtx* ctx);
int test_domains_add_actor_to_domain(KFS_TestCtx* ctx);
int test_domains_firewall_deny(KFS_TestCtx* ctx);
int test_domains_update_metadata(KFS_TestCtx* ctx);
int test_domains_delete_requires_admin(KFS_TestCtx* ctx);

#endif /* KFS_TEST_DOMAINS_H */