#include "kfs_test_props.h"
#include "kfs_test_assert.h"
#include "kfs_test_timing.h"
#include "kfs/kfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define KFS_TEST_ACCESS(path, mode) _access((path), (mode))
#else
#include <unistd.h>
#define KFS_TEST_ACCESS(path, mode) access((path), (mode))
#endif

static const char* const kfs_test_prop_dir_prefixes[] = {
    "fixtures/props/",
    "../fixtures/props/",
    "../../tests/fixtures/props/",
    NULL
};

static const char* const kfs_test_harness_fallback_prefixes[] = {
    "../../../tests/harness/assets/",
    "../../../../tests/harness/assets/",
    "tests/harness/assets/",
    NULL
};

const KFS_TestPropDef kfs_test_prop_defs[] = {
    { "prairie_jpg",  "textures/prairie.jpg",         "prairie.jpg",           "image",  "jpg",  "perf_textures", "perf_graphics",  'S' },
    { "thoc_jpg",     "textures/thoc.jpg",            "thoc.jpg",              "image",  "jpg",  "perf_textures", "perf_graphics",  'S' },
    { "rosewood_png", "textures/rosewood_veneer1.png","rosewood_veneer1.png",  "image",  "png",  "perf_textures", "perf_graphics",  'M' },
    { "bunny_obj",    "models/stanford-bunny.obj",    "stanford-bunny.obj",    "model",  "obj",  "perf_models",   "perf_geometry",  'M' },
    { "teapot_obj",   "models/utah_teapot.obj",       "utah_teapot.obj",       "model",  "obj",  "perf_models",   "perf_geometry",  'M' },
    { "teapot_stl",   "models/teapot.stl",            "teapot.stl",            "model",  "stl",  "perf_models",   "perf_geometry",  'L' },
    { "boombox_glb",  "models/BoomBox.glb",           "BoomBox.glb",           "model",  "glb",  "perf_models",   "perf_geometry",  'L' },
    { "sample_wav",   "audio/sample.wav",             "sample.wav",            "audio",  "wav",  "perf_audio",    "perf_media",     'L' },
    { "demon_vs",     "shaders/demon_hunt_sky.vs",    "demon_hunt_sky.vs",     "shader", "glsl", "perf_shaders",  "perf_graphics",  'S' },
    { "demon_fs",     "shaders/demon_hunt_sky.fs",    "demon_hunt_sky.fs",     "shader", "glsl", "perf_shaders",  "perf_graphics",  'S' },
    { "roboto_ttf",   "fonts/Roboto-Regular.ttf",     "static/Roboto-Regular.ttf", "font", "ttf", "perf_fonts",    "perf_media",     'M' },
};

const size_t kfs_test_prop_def_count = sizeof(kfs_test_prop_defs) / sizeof(kfs_test_prop_defs[0]);

static int kfs_test_path_exists(const char* path) {
    return path && path[0] && KFS_TEST_ACCESS(path, 0) == 0;
}

static int kfs_test_try_join_path(const char* prefix, const char* leaf,
                                  char* out_path, size_t out_sz) {
    size_t prefix_len;
    size_t leaf_len;
    if (!prefix || !leaf || !out_path || out_sz == 0) {
        return 1;
    }
    prefix_len = strlen(prefix);
    leaf_len = strlen(leaf);
    if (prefix_len + leaf_len >= out_sz) {
        return 1;
    }
    memcpy(out_path, prefix, prefix_len);
    memcpy(out_path + prefix_len, leaf, leaf_len + 1);
    return 0;
}

int kfs_test_resolve_prop_path(const KFS_TestPropDef* prop, char* out_path, size_t out_sz) {
    size_t i;
    if (!prop || !out_path || out_sz == 0) {
        return 1;
    }

    for (i = 0; kfs_test_prop_dir_prefixes[i] != NULL; ++i) {
        if (kfs_test_try_join_path(kfs_test_prop_dir_prefixes[i], prop->rel_path,
                                   out_path, out_sz) != 0) {
            continue;
        }
        if (kfs_test_path_exists(out_path)) {
            return 0;
        }
    }

    for (i = 0; kfs_test_harness_fallback_prefixes[i] != NULL; ++i) {
        if (kfs_test_try_join_path(kfs_test_harness_fallback_prefixes[i], prop->source_name,
                                   out_path, out_sz) != 0) {
            continue;
        }
        if (kfs_test_path_exists(out_path)) {
            return 0;
        }
    }

    out_path[0] = '\0';
    return 1;
}

const KFS_TestPropDef* kfs_test_find_prop(const char* id) {
    size_t i;
    if (!id) {
        return NULL;
    }
    for (i = 0; i < kfs_test_prop_def_count; ++i) {
        if (strcmp(kfs_test_prop_defs[i].id, id) == 0) {
            return &kfs_test_prop_defs[i];
        }
    }
    return NULL;
}

int kfs_test_read_prop_file(const char* path, uint8_t** data, size_t* data_size) {
    FILE* fp;
    long file_size;
    uint8_t* buffer;

    if (!path || !data || !data_size) {
        return 1;
    }
    *data = NULL;
    *data_size = 0;

    fp = fopen(path, "rb");
    if (!fp) {
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return 1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 1;
    }

    buffer = (uint8_t*)malloc((size_t)file_size);
    if (!buffer) {
        fclose(fp);
        return 1;
    }

    if (file_size > 0 &&
        fread(buffer, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(buffer);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    *data = buffer;
    *data_size = (size_t)file_size;
    return 0;
}

static int kfs_test_ensure_topic(KFS_TestCtx* ctx, const char* topic_name, int* topic_id) {
    int rc = kfs_add_topic(ctx->db, ctx->admin_uuid, ctx->admin_id, topic_name,
                           -1, ctx->domain_id, topic_id);
    if (rc == KFS_OK) {
        return 0;
    }
    if (rc == KFS_CONSTRAINT) {
        KFS_Topic topic;
        memset(&topic, 0, sizeof(topic));
        if (kfs_get_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                  topic_name, &topic) == KFS_OK) {
            *topic_id = topic.id;
            kfs_topic_free_contents(&topic);
            return (*topic_id > 0) ? 0 : 1;
        }
    }
    return 1;
}

static int kfs_test_ensure_epic(KFS_TestCtx* ctx, const char* epic_name, int* epic_id) {
    int rc = kfs_add_epic(ctx->db, ctx->admin_uuid, ctx->admin_id, epic_name,
                          "perf epic", -1, ctx->domain_id, epic_id);
    if (rc == KFS_OK) {
        return 0;
    }
    if (rc == KFS_CONSTRAINT) {
        int* ids = NULL;
        char** names = NULL;
        int count = 0;
        size_t i;
        rc = kfs_list_epics(ctx->db, ctx->admin_uuid, ctx->domain_id, &ids, &names, &count);
        if (rc != KFS_OK) {
            return 1;
        }
        for (i = 0; i < (size_t)count; ++i) {
            if (names[i] && strcmp(names[i], epic_name) == 0) {
                *epic_id = ids[i];
                break;
            }
        }
        if (ids) {
            kfs_mem_free(ids);
        }
        if (names) {
            for (i = 0; i < (size_t)count; ++i) {
                kfs_mem_free(names[i]);
            }
            kfs_mem_free(names);
        }
        return (*epic_id > 0) ? 0 : 1;
    }
    return 1;
}

static int kfs_test_link_epic_topics(KFS_TestCtx* ctx) {
    if (kfs_assign_epic_to_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                         "perf_textures", "perf_graphics") != KFS_OK) {
        return 1;
    }
    if (kfs_assign_epic_to_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                         "perf_shaders", "perf_graphics") != KFS_OK) {
        return 1;
    }
    if (kfs_assign_epic_to_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                         "perf_models", "perf_geometry") != KFS_OK) {
        return 1;
    }
    if (kfs_assign_epic_to_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                         "perf_audio", "perf_media") != KFS_OK) {
        return 1;
    }
    if (kfs_assign_epic_to_topic_by_name(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                         "perf_fonts", "perf_media") != KFS_OK) {
        return 1;
    }
    return 0;
}

int kfs_test_ensure_perf_topics_epics(KFS_TestCtx* ctx) {
    static const char* topic_names[] = {
        "perf_textures", "perf_models", "perf_audio", "perf_fonts", "perf_shaders", NULL
    };
    static const char* epic_names[] = {
        "perf_graphics", "perf_geometry", "perf_media", NULL
    };
    size_t i;
    int topic_id = -1;

    if (!ctx) {
        return 1;
    }
    if (kfs_test_require_bootstrap(ctx) != 0) {
        return 1;
    }
    for (i = 0; topic_names[i] != NULL; ++i) {
        if (kfs_test_ensure_topic(ctx, topic_names[i], &topic_id) != 0) {
            return 1;
        }
    }
    for (i = 0; epic_names[i] != NULL; ++i) {
        int epic_id = -1;
        if (kfs_test_ensure_epic(ctx, epic_names[i], &epic_id) != 0) {
            return 1;
        }
    }
    return kfs_test_link_epic_topics(ctx);
}

int kfs_test_seed_props_corpus(KFS_TestCtx* ctx, KFS_TestPropCorpus* corpus, char tier_filter) {
    size_t i;

    if (!ctx || !corpus) {
        return 1;
    }
    memset(corpus, 0, sizeof(*corpus));

    if (kfs_test_ensure_perf_topics_epics(ctx) != 0) {
        return 1;
    }

    for (i = 0; i < kfs_test_prop_def_count; ++i) {
        const KFS_TestPropDef* prop = &kfs_test_prop_defs[i];
        char path[1024];
        uint8_t* file_data = NULL;
        size_t file_size = 0;
        int artifact_id = -1;

        if (tier_filter != 0 && prop->tier != tier_filter) {
            continue;
        }
        if (corpus->seed_count >= KFS_TEST_PROP_CORPUS_MAX) {
            return 1;
        }
        if (kfs_test_resolve_prop_path(prop, path, sizeof(path)) != 0) {
            return 1;
        }
        if (kfs_test_read_prop_file(path, &file_data, &file_size) != 0) {
            return 1;
        }

        if (KFS_TEST_TIMED_INT(ctx, "kfs_create_artifact",
                                kfs_create_artifact(ctx->db, ctx->admin_uuid, ctx->domain_id,
                                                    prop->id, prop->kfs_type, prop->format,
                                                    ctx->admin_id, -1, file_data, file_size, NULL,
                                                    "{\"harness_prop\":true}", &artifact_id)) != KFS_OK) {
            free(file_data);
            return 1;
        }
        free(file_data);

        if (KFS_TEST_TIMED_INT(ctx, "kfs_assign_topic_to_artifact_by_name",
                                kfs_assign_topic_to_artifact_by_name(ctx->db, ctx->admin_uuid,
                                                                     ctx->domain_id, artifact_id,
                                                                     prop->topic)) != KFS_OK) {
            return 1;
        }

        corpus->seeds[corpus->seed_count].id = prop->id;
        corpus->seeds[corpus->seed_count].artifact_id = artifact_id;
        corpus->seeds[corpus->seed_count].bytes = file_size;
        corpus->seed_count++;
    }

    return corpus->seed_count > 0 ? 0 : 1;
}

int kfs_test_prop_artifact_id(const KFS_TestPropCorpus* corpus, const char* id) {
    int i;
    if (!corpus || !id) {
        return -1;
    }
    for (i = 0; i < corpus->seed_count; ++i) {
        if (strcmp(corpus->seeds[i].id, id) == 0) {
            return corpus->seeds[i].artifact_id;
        }
    }
    return -1;
}