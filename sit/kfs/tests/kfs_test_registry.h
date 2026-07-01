#ifndef KFS_TEST_REGISTRY_H
#define KFS_TEST_REGISTRY_H

#include <stddef.h>
#include "kfs_test_fixture.h"

typedef struct KFS_TestCase {
    const char* module;
    const char* name;
    int (*fn)(KFS_TestCtx* ctx);
} KFS_TestCase;

extern const KFS_TestCase kfs_test_cases[];
extern const size_t kfs_test_case_count;

#endif /* KFS_TEST_REGISTRY_H */