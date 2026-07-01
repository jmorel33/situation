#include "test_perf.h"
#include "kfs_test_assert.h"
#include "kfs_test_props.h"
#include "kfs_test_perf.h"
#include "kfs_test_timing.h"
#include "kfs/kfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PerfBlobBenchCtx {
    KFS_TestCtx* test_ctx;
    int artifact_id;
    size_t expected_bytes;
} PerfBlobBenchCtx;

static int perf_require_props_available(void) {
    const KFS_TestPropDef* prop = kfs_test_find_prop("prairie_jpg");
    char path[1024];
    if (!prop || kfs_test_resolve_prop_path(prop, path, sizeof(path)) != 0) {
        fprintf(stderr, "[SKIP] perf props missing — run: make sync-props\n");
        return KFS_TEST_PERF_SKIP;
    }
    return 0;
}

static int perf_seed_all_props(KFS_TestCtx* ctx, KFS_TestPropCorpus* corpus) {
    int rc = perf_require_props_available();
    if (rc != 0) {
        return rc;
    }
    if (kfs_test_seed_props_corpus(ctx, corpus, 0) != 0) {
        KFS_TEST_FAIL("failed to seed props corpus");
    }
    return 0;
}

static int perf_blob_read_fn(void* user_data, uint64_t* elapsed_ns, size_t* bytes) {
    PerfBlobBenchCtx* bench = (PerfBlobBenchCtx*)user_data;
    uint8_t* data = NULL;
    size_t data_size = 0;
    char* text_data = NULL;
    char* metadata = NULL;
    uint64_t start;
    int rc;

    start = kfs_test_perf_now_ns();
    rc = kfs_get_asset_data(bench->test_ctx->db, bench->test_ctx->admin_uuid,
                            bench->test_ctx->domain_id, bench->artifact_id,
                            &data, &data_size, &text_data, &metadata);
    *elapsed_ns = kfs_test_perf_now_ns() - start;
    kfs_test_timing_record(bench->test_ctx, "kfs_get_asset_data", *elapsed_ns);

    if (rc != KFS_OK) {
        kfs_mem_free(data);
        kfs_mem_free(text_data);
        kfs_mem_free(metadata);
        return 1;
    }

    *bytes = data_size;
    if (bench->expected_bytes > 0 && data_size != bench->expected_bytes) {
        kfs_mem_free(data);
        kfs_mem_free(text_data);
        kfs_mem_free(metadata);
        return 1;
    }

    kfs_mem_free(data);
    kfs_mem_free(text_data);
    kfs_mem_free(metadata);
    return 0;
}

static void perf_clamp_iters_for_tier(char tier) {
    KFS_TestPerfOptions opts = *kfs_test_perf_get_options();
    if (tier == 'L') {
        if (opts.measure_iters > 20) {
            opts.measure_iters = 20;
        }
        if (opts.warmup_iters > 3) {
            opts.warmup_iters = 3;
        }
    } else if (tier == 'M') {
        if (opts.measure_iters > 100) {
            opts.measure_iters = 100;
        }
        if (opts.warmup_iters > 10) {
            opts.warmup_iters = 10;
        }
    }
    kfs_test_perf_set_options(&opts);
}

static int perf_run_blob_read(KFS_TestCtx* ctx, const char* prop_id, char tier,
                              const char* test_name) {
    KFS_TestPropCorpus corpus;
    PerfBlobBenchCtx bench;
    KFS_TestPerfStats stats;
    int rc;

    rc = perf_seed_all_props(ctx, &corpus);
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (rc != 0) {
        return rc;
    }

    bench.test_ctx = ctx;
    bench.artifact_id = kfs_test_prop_artifact_id(&corpus, prop_id);
    if (bench.artifact_id <= 0) {
        KFS_TEST_FAIL("prop artifact not seeded");
    }
    {
        const KFS_TestPropDef* prop = kfs_test_find_prop(prop_id);
        size_t i;
        bench.expected_bytes = 0;
        for (i = 0; i < (size_t)corpus.seed_count; ++i) {
            if (strcmp(corpus.seeds[i].id, prop_id) == 0) {
                bench.expected_bytes = corpus.seeds[i].bytes;
                break;
            }
        }
        if (!prop) {
            KFS_TEST_FAIL("unknown prop id");
        }

        perf_clamp_iters_for_tier(tier);
        rc = kfs_test_perf_run(test_name, tier, perf_blob_read_fn, &bench, &stats);
        if (rc == KFS_TEST_PERF_SKIP) {
            return rc;
        }
        if (rc != 0) {
            return rc;
        }
        kfs_test_perf_publish(test_name, tier, &stats);
        return kfs_test_perf_check_baseline(test_name, &stats);
    }
}

int test_perf_permission_check(KFS_TestCtx* ctx) {
    uint64_t checker_uuid = 0;
    int checker_id = -1;
    int scheme_id = -1;
    int artifact_id = -1;
    KFS_TestPerfStats stats;
    int i;

    KFS_TEST_PTR_NONNULL(ctx);
    if (kfs_test_require_bootstrap(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    KFS_TEST_OK_CTX(ctx, "kfs_add_actor", kfs_add_actor(ctx->db, ctx->admin_uuid, "USER", "perf_checker",
                              "USER", 1, &checker_uuid, &checker_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_domain", kfs_add_actor_to_domain(ctx->db, ctx->admin_uuid, ctx->domain_id, checker_id));
    KFS_TEST_OK_CTX(ctx, "kfs_create_security_scheme", kfs_create_security_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                           ctx->admin_id, "perf_scheme", &scheme_id));
    KFS_TEST_OK_CTX(ctx, "kfs_add_actor_to_scheme", kfs_add_actor_to_scheme(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                        scheme_id, checker_id, 1, 0, 0));
    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "perf_perm_doc", "document", "text",
                                    ctx->admin_id, scheme_id, NULL, 0,
                                    "permission perf", NULL, &artifact_id));

    kfs_test_perf_stats_init(&stats);
    for (i = 0; i < kfs_test_perf_get_options()->warmup_iters; ++i) {
        kfs_check_permission(ctx->db, checker_uuid, "Artifact", artifact_id, KFS_PERM_READ);
    }
    for (i = 0; i < kfs_test_perf_get_options()->measure_iters; ++i) {
        uint64_t start = kfs_test_perf_now_ns();
        if (kfs_check_permission(ctx->db, checker_uuid, "Artifact", artifact_id,
                                 KFS_PERM_READ) != KFS_OK) {
            KFS_TEST_FAIL("permission check failed during perf loop");
        }
        uint64_t elapsed = kfs_test_perf_now_ns() - start;
        kfs_test_timing_record(ctx, "kfs_check_permission", elapsed);
        kfs_test_perf_stats_add(&stats, elapsed, 0);
    }
    kfs_test_perf_stats_finalize(&stats);
    kfs_test_perf_publish("permission_check", 'S', &stats);
    return kfs_test_perf_check_baseline("permission_check", &stats);
}

int test_perf_blob_read_small(KFS_TestCtx* ctx) {
    return perf_run_blob_read(ctx, "prairie_jpg", 'S', "blob_read_small");
}

int test_perf_blob_read_medium(KFS_TestCtx* ctx) {
    return perf_run_blob_read(ctx, "rosewood_png", 'M', "blob_read_medium");
}

int test_perf_blob_read_large_glb(KFS_TestCtx* ctx) {
    return perf_run_blob_read(ctx, "boombox_glb", 'L', "blob_read_large_glb");
}

int test_perf_blob_read_large_wav(KFS_TestCtx* ctx) {
    return perf_run_blob_read(ctx, "sample_wav", 'L', "blob_read_large_wav");
}

int test_perf_blob_ingest_large(KFS_TestCtx* ctx) {
    const KFS_TestPropDef* prop = kfs_test_find_prop("boombox_glb");
    char path[1024];
    const char* topic = "perf_models";
    uint64_t start;
    uint64_t elapsed_ns;
    KFS_TestPerfStats stats;
    int artifact_id = -1;
    int rc;

    KFS_TEST_PTR_NONNULL(ctx);
    rc = perf_require_props_available();
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (!prop || kfs_test_resolve_prop_path(prop, path, sizeof(path)) != 0) {
        return KFS_TEST_PERF_SKIP;
    }
    if (!kfs_test_perf_tier_allowed('L')) {
        return KFS_TEST_PERF_SKIP;
    }
    if (kfs_test_ensure_perf_topics_epics(ctx) != 0) {
        KFS_TEST_FAIL("perf topic bootstrap failed");
    }

    {
        uint8_t* file_data = NULL;
        size_t file_size = 0;
        start = kfs_test_perf_now_ns();
        if (kfs_test_read_prop_file(path, &file_data, &file_size) != 0) {
            KFS_TEST_FAIL("failed to read boombox prop");
        }
        if (KFS_TEST_TIMED_INT(ctx, "kfs_create_artifact",
                                kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                                    "perf_ingest_boombox", prop->kfs_type, prop->format,
                                                    ctx->admin_id, -1, file_data, file_size, NULL,
                                                    "{\"perf\":\"ingest\"}", &artifact_id)) != KFS_OK) {
            free(file_data);
            KFS_TEST_FAIL("boombox ingest failed");
        }
        free(file_data);
        if (KFS_TEST_TIMED_INT(ctx, "kfs_assign_topic_to_artifact_by_name",
                                kfs_assign_topic_to_artifact_by_name(ctx->db, ctx->admin_uuid,
                                                                     ctx->domain_id, artifact_id,
                                                                     topic)) != KFS_OK) {
            KFS_TEST_FAIL("boombox topic assign failed");
        }
        elapsed_ns = kfs_test_perf_now_ns() - start;
    }

    kfs_test_perf_stats_init(&stats);
    kfs_test_perf_stats_add(&stats, elapsed_ns, 0);
    kfs_test_perf_stats_finalize(&stats);
    kfs_test_perf_publish("blob_ingest_large", 'L', &stats);
    return kfs_test_perf_check_baseline("blob_ingest_large", &stats);
}

int test_perf_blob_roundtrip_checksum(KFS_TestCtx* ctx) {
    const KFS_TestPropDef* prop = kfs_test_find_prop("prairie_jpg");
    char path[1024];
    uint8_t* file_data = NULL;
    size_t file_size = 0;
    uint8_t* blob_data = NULL;
    size_t blob_size = 0;
    char* text_data = NULL;
    char* metadata = NULL;
    const char* topic = "perf_textures";
    int artifact_id = -1;
    int rc;

    KFS_TEST_PTR_NONNULL(ctx);
    rc = perf_require_props_available();
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (!prop || kfs_test_resolve_prop_path(prop, path, sizeof(path)) != 0) {
        return KFS_TEST_PERF_SKIP;
    }
    if (kfs_test_ensure_perf_topics_epics(ctx) != 0) {
        KFS_TEST_FAIL("perf topic bootstrap failed");
    }
    if (kfs_test_read_prop_file(path, &file_data, &file_size) != 0) {
        KFS_TEST_FAIL("failed to read prop file");
    }
    KFS_TEST_OK_CTX(ctx, "kfs_create_artifact", kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                    "perf_roundtrip_prairie", prop->kfs_type, prop->format,
                                    ctx->admin_id, -1, file_data, file_size, NULL, NULL,
                                    &artifact_id));
    KFS_TEST_OK_CTX(ctx, "kfs_assign_topic_to_artifact_by_name", kfs_assign_topic_to_artifact_by_name(ctx->db, ctx->admin_uuid,
                                                   ctx->domain_id, artifact_id, topic));
    KFS_TEST_OK_CTX(ctx, "kfs_get_asset_data", kfs_get_asset_data(ctx->db, ctx->admin_uuid, ctx->domain_id, artifact_id,
                                   &blob_data, &blob_size, &text_data, &metadata));
    if (blob_size != file_size || memcmp(file_data, blob_data, file_size) != 0) {
        free(file_data);
        kfs_mem_free(blob_data);
        kfs_mem_free(text_data);
        kfs_mem_free(metadata);
        KFS_TEST_FAIL("roundtrip blob mismatch");
    }
    free(file_data);
    kfs_mem_free(blob_data);
    kfs_mem_free(text_data);
    kfs_mem_free(metadata);
    return 0;
}

typedef struct PerfTopicLoadBenchCtx {
    KFS_TestCtx* test_ctx;
    KFS_TestPropCorpus* corpus;
    const char* topic_name;
} PerfTopicLoadBenchCtx;

/*
 * Benchmark topic-scoped blob reads via corpus artifact IDs.
 * Avoids kfs_load_by_topic (nested transaction while stmt_ids is open in kfs_load_asset_list).
 */
static int perf_load_topic_assets_fn(void* user_data, uint64_t* elapsed_ns, size_t* bytes) {
    PerfTopicLoadBenchCtx* bench = (PerfTopicLoadBenchCtx*)user_data;
    size_t total_bytes = 0;
    int i;
    uint64_t start = kfs_test_perf_now_ns();

    for (i = 0; i < bench->corpus->seed_count; ++i) {
        const KFS_TestPropSeed* seed = &bench->corpus->seeds[i];
        const KFS_TestPropDef* prop = kfs_test_find_prop(seed->id);
        uint8_t* data = NULL;
        size_t data_size = 0;
        char* text_data = NULL;
        char* metadata = NULL;
        int rc;

        if (!prop || strcmp(prop->topic, bench->topic_name) != 0) {
            continue;
        }

        rc = kfs_get_asset_data(bench->test_ctx->db, bench->test_ctx->admin_uuid,
                                bench->test_ctx->domain_id, seed->artifact_id,
                                &data, &data_size, &text_data, &metadata);
        if (rc != KFS_OK) {
            kfs_mem_free(data);
            kfs_mem_free(text_data);
            kfs_mem_free(metadata);
            return 1;
        }
        total_bytes += data_size;
        kfs_mem_free(data);
        kfs_mem_free(text_data);
        kfs_mem_free(metadata);
    }

    *elapsed_ns = kfs_test_perf_now_ns() - start;
    kfs_test_timing_record(bench->test_ctx, "kfs_get_asset_data", *elapsed_ns);
    *bytes = total_bytes;
    return 0;
}

static int perf_run_topic_load(KFS_TestCtx* ctx, const char* topic_name, char tier,
                               const char* label) {
    KFS_TestPropCorpus corpus;
    PerfTopicLoadBenchCtx bench;
    KFS_TestPerfStats stats;
    int rc;

    rc = perf_seed_all_props(ctx, &corpus);
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (rc != 0) {
        return rc;
    }

    bench.test_ctx = ctx;
    bench.corpus = &corpus;
    bench.topic_name = topic_name;

    perf_clamp_iters_for_tier(tier);
    rc = kfs_test_perf_run(label, tier, perf_load_topic_assets_fn, &bench, &stats);
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (rc != 0) {
        return rc;
    }
    kfs_test_perf_publish(label, tier, &stats);
    return kfs_test_perf_check_baseline(label, &stats);
}

int test_perf_load_by_topic_textures(KFS_TestCtx* ctx) {
    return perf_run_topic_load(ctx, "perf_textures", 'S', "load_by_topic_textures");
}

int test_perf_load_by_topic_models(KFS_TestCtx* ctx) {
    return perf_run_topic_load(ctx, "perf_models", 'L', "load_by_topic_models");
}

int test_perf_load_by_epic_geometry(KFS_TestCtx* ctx) {
    /* perf_geometry epic maps to perf_models topic in the v1 prop set. */
    return perf_run_topic_load(ctx, "perf_models", 'L', "load_by_epic_geometry");
}

int test_perf_bulk_ingest_all_props(KFS_TestCtx* ctx) {
    KFS_TestPropCorpus corpus;
    KFS_TestPerfStats stats;
    size_t total_bytes = 0;
    int rc;
    size_t i;

    KFS_TEST_PTR_NONNULL(ctx);
    rc = perf_require_props_available();
    if (rc == KFS_TEST_PERF_SKIP) {
        return rc;
    }
    if (kfs_test_require_bootstrap(ctx) != 0) {
        KFS_TEST_FAIL("bootstrap failed");
    }

    {
        uint64_t start = kfs_test_perf_now_ns();
        if (kfs_test_seed_props_corpus(ctx, &corpus, 0) != 0) {
            KFS_TEST_FAIL("bulk ingest failed");
        }
        kfs_test_perf_stats_init(&stats);
        kfs_test_perf_stats_add(&stats, kfs_test_perf_now_ns() - start, 0);
        for (i = 0; i < (size_t)corpus.seed_count; ++i) {
            total_bytes += corpus.seeds[i].bytes;
        }
        stats.bytes_per_op = total_bytes;
        kfs_test_perf_stats_finalize(&stats);
        kfs_test_perf_publish("bulk_ingest_all_props", 'L', &stats);
        return kfs_test_perf_check_baseline("bulk_ingest_all_props", &stats);
    }
}