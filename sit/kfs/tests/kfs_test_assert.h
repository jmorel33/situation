#ifndef KFS_TEST_ASSERT_H
#define KFS_TEST_ASSERT_H

#include <stdio.h>
#include "kfs/kfs.h"
#include "kfs_test_timing.h"

#define KFS_TEST_FAIL(msg) \
    do { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
        return 1; \
    } while (0)

#define KFS_TEST_EQ_INT(actual, expected, msg) \
    do { \
        int _a = (int)(actual); \
        int _e = (int)(expected); \
        if (_a != _e) { \
            fprintf(stderr, "[FAIL] %s:%d: %s (got %d, want %d)\n", \
                    __FILE__, __LINE__, (msg), _a, _e); \
            return 1; \
        } \
    } while (0)

#define KFS_TEST_OK(rc) KFS_TEST_EQ_INT((rc), KFS_OK, "expected KFS_OK")

/* Time a KFS API call and record it on ctx (shown in harness output). */
#define KFS_TEST_OK_CTX(ctx, label, call) \
    do { \
        int _kfs_timed_rc = KFS_TEST_TIMED_INT((ctx), (label), (call)); \
        KFS_TEST_EQ_INT(_kfs_timed_rc, KFS_OK, "expected KFS_OK"); \
    } while (0)

#define KFS_TEST_DENIED(rc) \
    KFS_TEST_EQ_INT((rc), KFS_PERMISSION_DENIED, "expected KFS_PERMISSION_DENIED")

#define KFS_TEST_NOTFOUND(rc) \
    KFS_TEST_EQ_INT((rc), KFS_NOTFOUND, "expected KFS_NOTFOUND")

#define KFS_TEST_PTR_NONNULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "[FAIL] %s:%d: null pointer: %s\n", \
                    __FILE__, __LINE__, #ptr); \
            return 1; \
        } \
    } while (0)

#define KFS_TEST_PTR_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "[FAIL] %s:%d: expected null pointer: %s\n", \
                    __FILE__, __LINE__, #ptr); \
            return 1; \
        } \
    } while (0)

#endif /* KFS_TEST_ASSERT_H */