#ifndef TEST_PERF_H
#define TEST_PERF_H

#include "kfs_test_fixture.h"

int test_perf_permission_check(KFS_TestCtx* ctx);
int test_perf_blob_read_small(KFS_TestCtx* ctx);
int test_perf_blob_read_medium(KFS_TestCtx* ctx);
int test_perf_blob_read_large_glb(KFS_TestCtx* ctx);
int test_perf_blob_read_large_wav(KFS_TestCtx* ctx);
int test_perf_blob_ingest_large(KFS_TestCtx* ctx);
int test_perf_blob_roundtrip_checksum(KFS_TestCtx* ctx);
int test_perf_load_by_topic_textures(KFS_TestCtx* ctx);
int test_perf_load_by_topic_models(KFS_TestCtx* ctx);
int test_perf_load_by_epic_geometry(KFS_TestCtx* ctx);
int test_perf_bulk_ingest_all_props(KFS_TestCtx* ctx);

#endif /* TEST_PERF_H */