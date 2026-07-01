/***************************************************************************************************
*
*   situation_impl_gpu_prof.h - P10.3 internal GPU timestamp / elapsed zones
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Populates SituationFrameProfile.gpu_zone_ns[] via VkQueryPool (VK) or GL_TIME_ELAPSED (GL).
*   Do not include directly — pulled in from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_GPU_PROF_H
#define SITUATION_IMPL_GPU_PROF_H

#ifndef SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT
#define SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT 16
#endif

#define SIT_GPU_PROF_QUERIES_PER_FRAME (SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT * 2u)
#define SIT_GPU_PROF_TOTAL_QUERIES (SITUATION_MAX_FRAMES_IN_FLIGHT * SIT_GPU_PROF_QUERIES_PER_FRAME)

static inline bool _SitGpuProfZoneIdValid(uint32_t zone_id) {
    return zone_id < (uint32_t)SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT;
}

static inline uint32_t _SitGpuProfVkQueryIndex(uint32_t frame_idx, uint32_t zone_id, int end) {
    return frame_idx * SIT_GPU_PROF_QUERIES_PER_FRAME + zone_id * 2u + (end ? 1u : 0u);
}

static inline void _SitGpuProfClearFrameSlot(uint32_t frame_idx) {
    if (frame_idx >= (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }
    sit_render.gpu_zone_open[frame_idx] = 0;
    sit_render.gpu_zone_completed[frame_idx] = 0;
}

static inline SituationError _SitGpuProfValidateZoneCall(uint32_t zone_id, bool begin, uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported) {
        return SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED;
    }
    if (!_SitGpuProfZoneIdValid(zone_id)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_OVERFLOW,
            begin ? "SituationCmdGPUZoneBegin" : "SituationCmdGPUZoneEnd");
    }
    if (frame_idx >= (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    const uint16_t bit = (uint16_t)(1u << zone_id);
    if (begin) {
        if (sit_render.gpu_zone_open[frame_idx] & bit) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_STATE,
                "SituationCmdGPUZoneBegin: zone already open");
        }
    } else {
        if (!(sit_render.gpu_zone_open[frame_idx] & bit)) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_STATE,
                "SituationCmdGPUZoneEnd: zone not open");
        }
    }
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_VULKAN)

static SituationError _SitGpuProfInitVulkan(void) {
    sit_render.gpu_timestamps_supported = false;
    sit_render.vk.gpu_timestamp_pool = VK_NULL_HANDLE;
    sit_render.vk.gpu_timestamp_period_ns = 0.0f;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
    sit_render.vk.gpu_timestamp_period_ns = props.limits.timestampPeriod;

    if (!props.limits.timestampComputeAndGraphics) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Situation] GPU timestamps: timestampComputeAndGraphics=false — zones disabled.\n");
        fflush(stderr);
#endif
        return SITUATION_SUCCESS;
    }

    VkQueryPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = SIT_GPU_PROF_TOTAL_QUERIES;

    VkResult cr = vkCreateQueryPool(sit_render.vk.device, &pool_info, NULL, &sit_render.vk.gpu_timestamp_pool);
    if (cr != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED,
            "_SitGpuProfInitVulkan: vkCreateQueryPool failed");
        return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED;
    }

    sit_render.gpu_timestamps_supported = true;
    sit_render.enabled_features_mask |= SIT_FEATURE_GPU_TIMESTAMPS;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] GPU timestamps enabled (VK, period=%.3f ns/tick).\n",
            (double)sit_render.vk.gpu_timestamp_period_ns);
    fflush(stderr);
#endif
    return SITUATION_SUCCESS;
}

static void _SitGpuProfShutdownVulkan(void) {
    if (sit_render.vk.gpu_timestamp_pool != VK_NULL_HANDLE && sit_render.vk.device != VK_NULL_HANDLE) {
        vkDestroyQueryPool(sit_render.vk.device, sit_render.vk.gpu_timestamp_pool, NULL);
        sit_render.vk.gpu_timestamp_pool = VK_NULL_HANDLE;
    }
}

static void _SitGpuProfReadbackVulkan(uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported || sit_render.vk.gpu_timestamp_pool == VK_NULL_HANDLE) {
        return;
    }
    if (frame_idx >= (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }

    const uint32_t completed = sit_render.gpu_zone_completed[frame_idx];
    if (completed == 0u) {
        return;
    }

    const uint32_t base = frame_idx * SIT_GPU_PROF_QUERIES_PER_FRAME;
    const uint32_t count = SIT_GPU_PROF_QUERIES_PER_FRAME;

    uint64_t ticks[SIT_GPU_PROF_QUERIES_PER_FRAME];
    memset(ticks, 0, sizeof(ticks));

    VkResult qr = vkGetQueryPoolResults(
        sit_render.vk.device,
        sit_render.vk.gpu_timestamp_pool,
        base,
        count,
        sizeof(ticks),
        ticks,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT);

    if (qr == VK_NOT_READY) {
        return;
    }
    if (qr != VK_SUCCESS) {
        return;
    }

    const double period = (double)sit_render.vk.gpu_timestamp_period_ns;
    for (uint32_t z = 0; z < (uint32_t)SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT; ++z) {
        if ((completed & (1u << z)) == 0u) {
            continue;
        }
        const uint64_t t0 = ticks[z * 2u];
        const uint64_t t1 = ticks[z * 2u + 1u];
        if (t1 > t0) {
            sit_render.gpu_zone_ns[z] = (uint64_t)((double)(t1 - t0) * period);
        } else {
            sit_render.gpu_zone_ns[z] = 0ull;
        }
    }
}

static void _SitGpuProfResetPoolVulkan(VkCommandBuffer cmd, uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported || sit_render.vk.gpu_timestamp_pool == VK_NULL_HANDLE) {
        return;
    }
    if (cmd == VK_NULL_HANDLE || frame_idx >= (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }
    const uint32_t base = frame_idx * SIT_GPU_PROF_QUERIES_PER_FRAME;
    vkCmdResetQueryPool(cmd, sit_render.vk.gpu_timestamp_pool, base, SIT_GPU_PROF_QUERIES_PER_FRAME);
    _SitGpuProfClearFrameSlot(frame_idx);
}

static void _SitGpuProfZoneBeginVulkan(VkCommandBuffer cmd, uint32_t zone_id, uint32_t frame_idx) {
    if (_SitGpuProfValidateZoneCall(zone_id, true, frame_idx) != SITUATION_SUCCESS) {
        return;
    }
    const uint32_t q = _SitGpuProfVkQueryIndex(frame_idx, zone_id, 0);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, sit_render.vk.gpu_timestamp_pool, q);
    sit_render.gpu_zone_open[frame_idx] |= (uint16_t)(1u << zone_id);
}

static void _SitGpuProfZoneEndVulkan(VkCommandBuffer cmd, uint32_t zone_id, uint32_t frame_idx) {
    if (_SitGpuProfValidateZoneCall(zone_id, false, frame_idx) != SITUATION_SUCCESS) {
        return;
    }
    const uint32_t q = _SitGpuProfVkQueryIndex(frame_idx, zone_id, 1);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, sit_render.vk.gpu_timestamp_pool, q);
    sit_render.gpu_zone_open[frame_idx] &= (uint16_t)~(1u << zone_id);
    sit_render.gpu_zone_completed[frame_idx] |= (1u << zone_id);
}

#endif /* SITUATION_USE_VULKAN */

#if defined(SITUATION_USE_OPENGL)

static SituationError _SitGpuProfInitOpenGL(void) {
    sit_render.gpu_timestamps_supported = false;

    if (!GLAD_GL_ARB_timer_query) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Situation] GPU timestamps: GL_ARB_timer_query unavailable — zones disabled.\n");
        fflush(stderr);
#endif
        return SITUATION_SUCCESS;
    }

    for (int f = 0; f < SITUATION_MAX_FRAMES_IN_FLIGHT; ++f) {
        for (uint32_t z = 0; z < (uint32_t)SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT; ++z) {
            sit_render.gl.gpu_elapsed_queries[f][z] = 0;
            glGenQueries(1, &sit_render.gl.gpu_elapsed_queries[f][z]);
            if (sit_render.gl.gpu_elapsed_queries[f][z] == 0) {
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL,
                    "_SitGpuProfInitOpenGL: glGenQueries failed");
                return SITUATION_ERROR_OPENGL_GENERAL;
            }
        }
    }

    sit_render.gpu_timestamps_supported = true;
    sit_render.enabled_features_mask |= SIT_FEATURE_GPU_TIMESTAMPS;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] GPU timestamps enabled (GL TIME_ELAPSED).\n");
    fflush(stderr);
#endif
    return SITUATION_SUCCESS;
}

static void _SitGpuProfShutdownOpenGL(void) {
    if (!GLAD_GL_ARB_timer_query) {
        return;
    }
    for (int f = 0; f < SITUATION_MAX_FRAMES_IN_FLIGHT; ++f) {
        for (uint32_t z = 0; z < (uint32_t)SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT; ++z) {
            if (sit_render.gl.gpu_elapsed_queries[f][z] != 0) {
                glDeleteQueries(1, &sit_render.gl.gpu_elapsed_queries[f][z]);
                sit_render.gl.gpu_elapsed_queries[f][z] = 0;
            }
        }
    }
}

static void _SitGpuProfReadbackOpenGL(uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported || !GLAD_GL_ARB_timer_query) {
        return;
    }
    if (frame_idx >= (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }

    const uint32_t completed = sit_render.gpu_zone_completed[frame_idx];
    if (completed == 0u) {
        return;
    }

    for (uint32_t z = 0; z < (uint32_t)SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT; ++z) {
        if ((completed & (1u << z)) == 0u) {
            continue;
        }
        const GLuint q = sit_render.gl.gpu_elapsed_queries[frame_idx][z];
        if (q == 0) {
            continue;
        }
        GLuint available = 0;
        glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) {
            sit_render.gpu_zone_ns[z] = 0ull;
            continue;
        }
        uint64_t ns = 0;
        glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
        sit_render.gpu_zone_ns[z] = ns;
    }
}

static void _SitGpuProfZoneBeginGL(uint32_t zone_id, uint32_t frame_idx) {
    if (_SitGpuProfValidateZoneCall(zone_id, true, frame_idx) != SITUATION_SUCCESS) {
        return;
    }
    const GLuint q = sit_render.gl.gpu_elapsed_queries[frame_idx][zone_id];
    glBeginQuery(GL_TIME_ELAPSED, q);
    sit_render.gpu_zone_open[frame_idx] |= (uint16_t)(1u << zone_id);
}

static void _SitGpuProfZoneEndGL(uint32_t zone_id, uint32_t frame_idx) {
    if (_SitGpuProfValidateZoneCall(zone_id, false, frame_idx) != SITUATION_SUCCESS) {
        return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    sit_render.gpu_zone_open[frame_idx] &= (uint16_t)~(1u << zone_id);
    sit_render.gpu_zone_completed[frame_idx] |= (1u << zone_id);
}

#endif /* SITUATION_USE_OPENGL */

static inline SituationError _SitGpuProfInit(void) {
    memset(sit_render.gpu_zone_ns, 0, sizeof(sit_render.gpu_zone_ns));
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; ++i) {
        _SitGpuProfClearFrameSlot((uint32_t)i);
    }
#if defined(SITUATION_USE_VULKAN)
    return _SitGpuProfInitVulkan();
#elif defined(SITUATION_USE_OPENGL)
    return _SitGpuProfInitOpenGL();
#else
    return SITUATION_SUCCESS;
#endif
}

static inline void _SitGpuProfShutdown(void) {
#if defined(SITUATION_USE_VULKAN)
    _SitGpuProfShutdownVulkan();
#elif defined(SITUATION_USE_OPENGL)
    _SitGpuProfShutdownOpenGL();
#endif
    sit_render.gpu_timestamps_supported = false;
}

static inline void _SitGpuProfReadbackFrame(int frame_index) {
    if (frame_index < 0 || frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }
#if defined(SITUATION_USE_VULKAN)
    _SitGpuProfReadbackVulkan((uint32_t)frame_index);
#elif defined(SITUATION_USE_OPENGL)
    _SitGpuProfReadbackOpenGL((uint32_t)frame_index);
#endif
}

/** Internal library zones (no SituationCommandBuffer on GL; pass vk cmd on Vulkan). */
#if defined(SITUATION_USE_OPENGL)
static inline void _SitGpuProfInternalZoneBeginGL(uint32_t zone_id, int frame_index) {
    if (!sit_render.gpu_timestamps_supported || frame_index < 0) {
        return;
    }
    _SitGpuProfZoneBeginGL(zone_id, (uint32_t)frame_index);
}

static inline void _SitGpuProfInternalZoneEndGL(uint32_t zone_id, int frame_index) {
    if (!sit_render.gpu_timestamps_supported || frame_index < 0) {
        return;
    }
    _SitGpuProfZoneEndGL(zone_id, (uint32_t)frame_index);
}
#endif

#if defined(SITUATION_USE_VULKAN)
static inline void _SitGpuProfInternalZoneBeginVK(VkCommandBuffer cmd, uint32_t zone_id, uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported || cmd == VK_NULL_HANDLE) {
        return;
    }
    _SitGpuProfZoneBeginVulkan(cmd, zone_id, frame_idx);
}

static inline void _SitGpuProfInternalZoneEndVK(VkCommandBuffer cmd, uint32_t zone_id, uint32_t frame_idx) {
    if (!sit_render.gpu_timestamps_supported || cmd == VK_NULL_HANDLE) {
        return;
    }
    _SitGpuProfZoneEndVulkan(cmd, zone_id, frame_idx);
}
#endif

#endif /* SITUATION_IMPL_GPU_PROF_H */
