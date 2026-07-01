/***************************************************************************************************
*
*   situation_impl_query_pool.h - User GPU query pools (P10.4)
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_QUERY_POOL_H
#define SITUATION_IMPL_QUERY_POOL_H

static inline bool _SitQueryPoolIndexRangeValid(_SituationQueryPoolSlot* slot, uint32_t first, uint32_t count) {
    if (!slot || count == 0u) {
        return false;
    }
    if (first >= slot->query_count) {
        return false;
    }
    if ((uint64_t)first + (uint64_t)count > (uint64_t)slot->query_count) {
        return false;
    }
    return true;
}

#if defined(SITUATION_USE_VULKAN)
static VkPipelineStageFlagBits _SitQueryPoolVkStage(uint32_t stage_flags) {
    if (stage_flags & SITUATION_PIPELINE_STAGE_BOTTOM) {
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT) {
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_DEPTH_STENCIL) {
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) {
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) {
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) {
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if (stage_flags & SITUATION_PIPELINE_STAGE_TRANSFER) {
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}
#endif

static void _SitQueryPoolDestroySlot(_SituationQueryPoolSlot* slot) {
    if (!slot) {
        return;
    }
#if defined(SITUATION_USE_VULKAN)
    if (slot->vk_pool != VK_NULL_HANDLE && sit_render.vk.device != VK_NULL_HANDLE) {
        vkDestroyQueryPool(sit_render.vk.device, slot->vk_pool, NULL);
    }
    slot->vk_pool = VK_NULL_HANDLE;
#elif defined(SITUATION_USE_OPENGL)
    if (slot->gl_queries) {
        for (uint32_t i = 0; i < slot->query_count; ++i) {
            if (slot->gl_queries[i] != 0) {
                glDeleteQueries(1, &slot->gl_queries[i]);
            }
        }
        SIT_FREE(slot->gl_queries);
        slot->gl_queries = NULL;
    }
#endif
    memset(slot, 0, sizeof(*slot));
}

static void _SitQueryPoolShutdownAll(void) {
    for (int i = 0; i < SITUATION_MAX_QUERY_POOLS; ++i) {
        if (sit_render.query_pool_slots_used[i]) {
            _SitQueryPoolDestroySlot(&sit_render.query_pool_slots[i]);
            sit_render.query_pool_slots_used[i] = false;
        }
    }
    sit_render.active_query_pool_count = 0;
    sit_render.active_occlusion_pool_slot = -1;
    sit_render.active_occlusion_query_index = 0u;
}

#if defined(SITUATION_USE_OPENGL)
static void _SitQueryPoolResetGL(_SituationQueryPoolSlot* slot, uint32_t first_query, uint32_t query_count) {
    (void)slot;
    (void)first_query;
    (void)query_count;
    /* GL has no pool reset; results are consumed on read. */
}

static void _SitQueryPoolWriteTimestampGL(_SituationQueryPoolSlot* slot, uint32_t query_index) {
    if (!slot || slot->type != SITUATION_QUERY_TYPE_TIMESTAMP || !GLAD_GL_ARB_timer_query) {
        return;
    }
    if (query_index >= slot->query_count || !slot->gl_queries) {
        return;
    }
    glQueryCounter(slot->gl_queries[query_index], GL_TIMESTAMP);
}

static void _SitQueryPoolBeginOcclusionGL(_SituationQueryPoolSlot* slot, uint32_t query_index) {
    if (!slot || slot->type != SITUATION_QUERY_TYPE_OCCLUSION || !slot->gl_queries) {
        return;
    }
    if (query_index >= slot->query_count) {
        return;
    }
    glBeginQuery(GL_SAMPLES_PASSED, slot->gl_queries[query_index]);
    sit_render.active_occlusion_pool_slot = (int)(slot - sit_render.query_pool_slots);
    sit_render.active_occlusion_query_index = query_index;
}

static void _SitQueryPoolEndOcclusionGL(void) {
    if (sit_render.active_occlusion_pool_slot < 0) {
        return;
    }
    glEndQuery(GL_SAMPLES_PASSED);
    sit_render.active_occlusion_pool_slot = -1;
    sit_render.active_occlusion_query_index = 0u;
}
#endif

#if defined(SITUATION_USE_VULKAN)
static void _SitQueryPoolResetVK(VkCommandBuffer cmd, _SituationQueryPoolSlot* slot, uint32_t first_query, uint32_t query_count) {
    if (!slot || slot->vk_pool == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) {
        return;
    }
    vkCmdResetQueryPool(cmd, slot->vk_pool, first_query, query_count);
}

static void _SitQueryPoolWriteTimestampVK(VkCommandBuffer cmd, _SituationQueryPoolSlot* slot, uint32_t query_index, uint32_t pipeline_stage) {
    if (!slot || slot->type != SITUATION_QUERY_TYPE_TIMESTAMP || slot->vk_pool == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) {
        return;
    }
    if (query_index >= slot->query_count) {
        return;
    }
    vkCmdWriteTimestamp(cmd, _SitQueryPoolVkStage(pipeline_stage), slot->vk_pool, query_index);
}

static void _SitQueryPoolBeginOcclusionVK(VkCommandBuffer cmd, _SituationQueryPoolSlot* slot, uint32_t query_index) {
    if (!slot || slot->type != SITUATION_QUERY_TYPE_OCCLUSION || slot->vk_pool == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) {
        return;
    }
    if (query_index >= slot->query_count) {
        return;
    }
    vkCmdBeginQuery(cmd, slot->vk_pool, query_index, 0);
    sit_render.active_occlusion_pool_slot = (int)(slot - sit_render.query_pool_slots);
    sit_render.active_occlusion_query_index = query_index;
}

static void _SitQueryPoolEndOcclusionVK(VkCommandBuffer cmd) {
    if (sit_render.active_occlusion_pool_slot < 0 || cmd == VK_NULL_HANDLE) {
        return;
    }
    _SituationQueryPoolSlot* slot = &sit_render.query_pool_slots[sit_render.active_occlusion_pool_slot];
    if (!slot->is_active || slot->vk_pool == VK_NULL_HANDLE) {
        sit_render.active_occlusion_pool_slot = -1;
        return;
    }
    vkCmdEndQuery(cmd, slot->vk_pool, sit_render.active_occlusion_query_index);
    sit_render.active_occlusion_pool_slot = -1;
    sit_render.active_occlusion_query_index = 0u;
}
#endif

SITAPI SituationError SituationCreateQueryPool(SituationQueryType type, uint32_t count, SituationQueryPool* out_pool) {
    if (!out_pool) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_pool = SITUATION_NULL_QUERY_POOL;
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (count == 0u || count > SITUATION_MAX_QUERIES_PER_POOL) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCreateQueryPool: count must be 1..SITUATION_MAX_QUERIES_PER_POOL.");
    }
    if (type != SITUATION_QUERY_TYPE_TIMESTAMP && type != SITUATION_QUERY_TYPE_OCCLUSION) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateQueryPool: invalid type.");
    }

#if defined(SITUATION_USE_VULKAN)
    if (type == SITUATION_QUERY_TYPE_TIMESTAMP) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
        if (!props.limits.timestampComputeAndGraphics) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED,
                "SituationCreateQueryPool: timestamp queries unsupported on this device.");
        }
    }
#elif defined(SITUATION_USE_OPENGL)
    if (type == SITUATION_QUERY_TYPE_TIMESTAMP && !GLAD_GL_ARB_timer_query) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED,
            "SituationCreateQueryPool: GL_ARB_timer_query unavailable.");
    }
#endif

    if (sit_render.active_query_pool_count >= SITUATION_MAX_QUERY_POOLS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "SituationCreateQueryPool: max query pools reached.");
    }

    int new_slot = -1;
    for (int i = 0; i < SITUATION_MAX_QUERY_POOLS; ++i) {
        if (!sit_render.query_pool_slots_used[i]) {
            new_slot = i;
            break;
        }
    }
    if (new_slot < 0) {
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }

    _SituationQueryPoolSlot* slot = &sit_render.query_pool_slots[new_slot];
    memset(slot, 0, sizeof(*slot));
    slot->type = type;
    slot->query_count = count;
    slot->generation++;
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }

    bool success = true;
#if defined(SITUATION_USE_VULKAN)
    VkQueryPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    pool_info.queryType = (type == SITUATION_QUERY_TYPE_TIMESTAMP) ? VK_QUERY_TYPE_TIMESTAMP : VK_QUERY_TYPE_OCCLUSION;
    pool_info.queryCount = count;
    if (vkCreateQueryPool(sit_render.vk.device, &pool_info, NULL, &slot->vk_pool) != VK_SUCCESS) {
        success = false;
    }
#elif defined(SITUATION_USE_OPENGL)
    slot->gl_queries = (GLuint*)SIT_CALLOC(count, sizeof(GLuint));
    if (!slot->gl_queries) {
        success = false;
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            glGenQueries(1, &slot->gl_queries[i]);
            if (slot->gl_queries[i] == 0) {
                success = false;
                break;
            }
        }
    }
    SIT_CHECK_GL_ERROR();
    if (sit_render.gl.last_error != GL_NO_ERROR) {
        success = false;
    }
#endif

    if (!success) {
        _SitQueryPoolDestroySlot(slot);
        return _SituationSetErrorFromCode(SITUATION_ERROR_BACKEND_SPECIFIC,
            "SituationCreateQueryPool: GPU query pool creation failed.");
    }

    slot->is_active = true;
    sit_render.query_pool_slots_used[new_slot] = true;
    sit_render.active_query_pool_count++;
    out_pool->slot_index = (uint32_t)new_slot;
    out_pool->generation = slot->generation;
    return SITUATION_SUCCESS;
}

SITAPI void SituationDestroyQueryPool(SituationQueryPool* pool) {
    if (!pool || _SitQueryPoolHandleIsNull(*pool)) {
        return;
    }
    _SituationQueryPoolSlot* slot = _SitGetQueryPoolSlot(*pool);
    if (!slot) {
        *pool = SITUATION_NULL_QUERY_POOL;
        return;
    }
    uint32_t slot_index = pool->slot_index;
    if (sit_render.active_occlusion_pool_slot == (int)slot_index) {
        sit_render.active_occlusion_pool_slot = -1;
        sit_render.active_occlusion_query_index = 0u;
    }
    _SitQueryPoolDestroySlot(slot);
    sit_render.query_pool_slots_used[slot_index] = false;
    if (sit_render.active_query_pool_count > 0) {
        sit_render.active_query_pool_count--;
    }
    *pool = SITUATION_NULL_QUERY_POOL;
}

SITAPI SituationError SituationGetQueryPoolResults(SituationQueryPool pool, uint32_t first_query, uint32_t query_count,
        uint64_t* out_results, uint32_t flags) {
    _SituationFlushRenderThread();
    if (!out_results) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    _SituationQueryPoolSlot* slot = _SitGetQueryPoolSlot(pool);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_POOL_INVALID, "SituationGetQueryPoolResults");
    }
    if (!_SitQueryPoolIndexRangeValid(slot, first_query, query_count)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_INDEX_OUT_OF_RANGE, "SituationGetQueryPoolResults");
    }

    const bool wait = (flags & SITUATION_QUERY_RESULT_WAIT_BIT) != 0u;

#if defined(SITUATION_USE_VULKAN)
    if (slot->vk_pool == VK_NULL_HANDLE) {
        return SITUATION_ERROR_QUERY_POOL_INVALID;
    }
    VkQueryResultFlags vk_flags = VK_QUERY_RESULT_64_BIT;
    if (wait) {
        vk_flags |= VK_QUERY_RESULT_WAIT_BIT;
    }
    VkResult qr = vkGetQueryPoolResults(
        sit_render.vk.device,
        slot->vk_pool,
        first_query,
        query_count,
        (size_t)query_count * sizeof(uint64_t),
        out_results,
        sizeof(uint64_t),
        vk_flags);
    if (qr == VK_NOT_READY) {
        return SITUATION_ERROR_QUERY_RESULT_NOT_READY;
    }
    if (qr != VK_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BACKEND_SPECIFIC, "SituationGetQueryPoolResults: vkGetQueryPoolResults failed.");
    }
    if (slot->type == SITUATION_QUERY_TYPE_TIMESTAMP) {
        const double period = (double)sit_render.vk.gpu_timestamp_period_ns;
        for (uint32_t i = 0; i < query_count; ++i) {
            out_results[i] = (uint64_t)((double)out_results[i] * period);
        }
    }
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_OPENGL)
    if (!slot->gl_queries) {
        return SITUATION_ERROR_QUERY_POOL_INVALID;
    }
    bool release_context = false;
    if (sit_gs.sit_glfw_window && glfwGetCurrentContext() != sit_gs.sit_glfw_window) {
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
        release_context = sit_render.enabled;
    }
    if (wait) {
        glFinish();
    }
    for (uint32_t i = 0; i < query_count; ++i) {
        const uint32_t qi = first_query + i;
        const GLuint q = slot->gl_queries[qi];
        GLuint available = 0;
        if (wait) {
            int spins = 0;
            while (spins < 10000000) {
                glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    break;
                }
                spins++;
            }
        } else {
            glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
        }
        if (!available) {
            if (release_context) {
                glfwMakeContextCurrent(NULL);
            }
            return SITUATION_ERROR_QUERY_RESULT_NOT_READY;
        }
        uint64_t value = 0;
        glGetQueryObjectui64v(q, GL_QUERY_RESULT, &value);
        out_results[i] = value;
    }
    if (release_context) {
        glfwMakeContextCurrent(NULL);
    }
    return SITUATION_SUCCESS;
#else
    (void)wait;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

#endif /* SITUATION_IMPL_QUERY_POOL_H */
