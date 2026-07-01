#ifndef KFS_TEST_FIXTURE_H
#define KFS_TEST_FIXTURE_H

#include <stdint.h>
#include "kfs/kfs.h"
#include "kfs_test_timing.h"

#define KFS_TEST_ADMIN_GROUP_NAME "AdminGroup"
#define KFS_TEST_ADMIN_USER_NAME  "kfs_test_admin"
#define KFS_TEST_PRIMARY_DOMAIN   "PrimaryDomain"

typedef struct KFS_TestCtx {
    GameDB* db;
    char tmp_dir[512];
    char artifacts_path[512];
    char arch_path[512];
    char registry_path[512];
    uint64_t admin_uuid;
    int admin_id;
    int admin_group_id;
    int domain_id;
    KFS_TestTimingEntry timings[KFS_TEST_TIMING_MAX_OPS];
    int timing_count;
} KFS_TestCtx;

/* Create shared suite temp dir once; DB files are removed in suite_end. */
int kfs_test_fixture_suite_begin(void);
void kfs_test_fixture_suite_end(void);

/* Returns 0 on success, non-zero on harness error. */
int kfs_test_ctx_create(KFS_TestCtx* ctx);

/* Returns 0 on success. Idempotent if ctx is already cleared. */
int kfs_test_ctx_destroy(KFS_TestCtx* ctx);

/* H1+: bootstrap AdminGroup, admin user, primary domain. H0: no-op success. */
int kfs_test_bootstrap_admin(KFS_TestCtx* ctx);

/* Idempotent: bootstrap only when ctx has no admin yet. */
int kfs_test_require_bootstrap(KFS_TestCtx* ctx);

#endif /* KFS_TEST_FIXTURE_H */