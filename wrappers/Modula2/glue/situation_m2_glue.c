#include "situation_m2_glue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Mirror sit/situation_api.h SituationInitInfo (MSVC x64 / MinGW, v2.4.336+). */
typedef enum {
    SIT_OUTPUT_COLOR_AUTO = 0
} SituationOutputColorDepth;

struct SituationInitInfo {
    int32_t  window_width;
    int32_t  window_height;
    const char* window_title;
    uint32_t initial_active_window_flags;
    uint32_t initial_inactive_window_flags;
    uint8_t  enable_vulkan_validation;
    uint8_t  force_single_queue;
    uint8_t  _pad_after_vulkan_bools[2];
    uint32_t max_frames_in_flight;
    const char** required_vulkan_extensions;
    uint32_t required_vulkan_extension_count;
    uint32_t flags;
    SituationOutputColorDepth output_color_depth;
    uint32_t max_audio_voices;
    int32_t  render_thread_count;
    int32_t  backpressure_policy;
    uint32_t io_queue_capacity;
    uint8_t  disable_io_thread;
    uint8_t  _pad_before_hot_reload[7];
    double   hot_reload_poll_rate;
    uint64_t staging_buffer_size;
    const char* main_thread_name;
    uint64_t thread_affinity_main;
    uint64_t thread_affinity_render;
    uint64_t thread_affinity_audio;
    uint8_t  numa_prefer_local;
    uint8_t  worker_numa_spread;
    uint8_t  _pad_before_io_numa[2];
    int32_t  io_thread_numa_node;
    uint8_t  thread_pool_use_physical_cores;
    uint8_t  _pad_before_pool_reserved[3];
    uint32_t thread_pool_reserved_threads;
};

void SituationM2InitInfoZero(SituationInitInfo* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->output_color_depth = SIT_OUTPUT_COLOR_AUTO;
}

void SituationM2InitInfoWindow(SituationInitInfo* out, int width, int height, const char* title) {
    if (!out) return;
    SituationM2InitInfoZero(out);
    out->window_width = width;
    out->window_height = height;
    out->window_title = title;
    /* render_thread_count left at 0 — matches Rust/C hello_situation defaults (main-thread GL). */
}