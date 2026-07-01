#ifndef KFS_TEST_PROPS_H
#define KFS_TEST_PROPS_H

#include <stddef.h>
#include <stdint.h>
#include "kfs_test_fixture.h"

typedef enum KFS_TestPropTier {
    KFS_PROP_TIER_S = 'S',
    KFS_PROP_TIER_M = 'M',
    KFS_PROP_TIER_L = 'L'
} KFS_TestPropTier;

typedef struct KFS_TestPropDef {
    const char* id;
    const char* rel_path;
    const char* source_name;
    const char* kfs_type;
    const char* format;
    const char* topic;
    const char* epic;
    char tier;
} KFS_TestPropDef;

typedef struct KFS_TestPropSeed {
    const char* id;
    int artifact_id;
    size_t bytes;
} KFS_TestPropSeed;

#define KFS_TEST_PROP_CORPUS_MAX 16

typedef struct KFS_TestPropCorpus {
    KFS_TestPropSeed seeds[KFS_TEST_PROP_CORPUS_MAX];
    int seed_count;
} KFS_TestPropCorpus;

extern const KFS_TestPropDef kfs_test_prop_defs[];
extern const size_t kfs_test_prop_def_count;

/* Resolve vendored prop or fall back to Situation harness assets/. */
int kfs_test_resolve_prop_path(const KFS_TestPropDef* prop, char* out_path, size_t out_sz);

const KFS_TestPropDef* kfs_test_find_prop(const char* id);

/* Load entire prop file into heap buffer (caller frees). */
int kfs_test_read_prop_file(const char* path, uint8_t** data, size_t* data_size);

/*
 * Bootstrap topics/epics and ingest all props (or tier-filtered subset).
 * tier_filter: 0 = all, or 'S'/'M'/'L'.
 */
int kfs_test_ensure_perf_topics_epics(KFS_TestCtx* ctx);

int kfs_test_seed_props_corpus(KFS_TestCtx* ctx, KFS_TestPropCorpus* corpus, char tier_filter);

int kfs_test_prop_artifact_id(const KFS_TestPropCorpus* corpus, const char* id);

#endif /* KFS_TEST_PROPS_H */