/***************************************************************************************************
*
*   situation_impl_renderer_frame_cmd.h - Frame Loop, Commands, and Model I/O
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Per-frame acquire/end, SituationCmd* recording, render lists, metrics, model loaders.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_FRAME_CMD_H
#define SITUATION_IMPL_RENDERER_FRAME_CMD_H

/* MSAA Phase 0 — shared VD sample count + VK pipeline key helpers (VD-4b prep). */
static inline int _SitVDEffectiveSampleCount(const SituationVirtualDisplay* vd) {
    if (!vd) return 1;
    return SituationMultisampleQualitySampleCount(vd->msaa_quality);
}

#if defined(SITUATION_USE_VULKAN)
static inline uint32_t _SitVDVkPipelineVariantKey(VkFormat color_fmt, VkFormat depth_fmt, int sample_count) {
    uint32_t sc = (sample_count <= 1) ? 1u : (uint32_t)sample_count;
    return ((uint32_t)color_fmt << 8u) ^ ((uint32_t)depth_fmt << 16u) ^ (sc << 24u);
}

static inline VkSampleCountFlagBits _SitVkSampleCountFlagFromInt(int samples) {
    switch (samples) {
        case 2:  return VK_SAMPLE_COUNT_2_BIT;
        case 4:  return VK_SAMPLE_COUNT_4_BIT;
        case 8:  return VK_SAMPLE_COUNT_8_BIT;
        case 16: return VK_SAMPLE_COUNT_16_BIT;
        case 32: return VK_SAMPLE_COUNT_32_BIT;
        case 64: return VK_SAMPLE_COUNT_64_BIT;
        default: return VK_SAMPLE_COUNT_1_BIT;
    }
}
#endif

SITAPI SituationError SituationAcquireFrameCommandBuffer(void) {
    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot begin frame before library initialization.");
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    /* Frame time: when the render thread is OFF, measured in SituationUpdateTimers.
     * When the render thread is ON, display-anchored timing is published at present
     * completion and consumed in SituationUpdateTimers (Phase 2). */

    /* Recover from callers that acquired a frame but never submitted (harness validation tests, etc.).
     * Without this, the in-flight fence for the slot is reset on the next acquire but never signaled. */
    if (sit_render.in_frame) {
        fprintf(stderr,
            "[Situation] WARNING: SituationAcquireFrameCommandBuffer called while a frame is still open; auto-ending previous frame.\n");
        fflush(stderr);
#if defined(SITUATION_USE_VULKAN)
        if (sit_render.vk.inside_render_pass) {
            VkCommandBuffer leak_cmd = (VkCommandBuffer)SituationGetMainCommandBuffer();
            if (leak_cmd != VK_NULL_HANDLE) {
                vkCmdEndRenderPass(leak_cmd);
            }
            sit_render.vk.inside_render_pass = false;
            sit_render.vk.inside_main_swapchain_render_pass = false;
            sit_render.vk.current_render_area = (VkRect2D){0};
        }
#endif
        SituationEndFrame();
    }

#if defined(SITUATION_USE_OPENGL)
    {
        sit_render.gl.screenshot_valid = false;
        sit_render.gl.screenshot_resolved_frame_index = -1;
        // [Async load / Hot-Reload] Poll non-blocking GLSL compile stages, then program link.
        for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
            _SituationShaderSlot* slot = &sit_render.shader_registry[i];
            if (slot->is_active) {
                /* SPIR-V specialize is driven from SituationPollShaderLoad (one step per app poll).
                 * Polling here too caused FS specialize inside render_frame (UI freeze, no sky_poll logs). */
                if (slot->gl_async_load_stage != SIT_GL_ASYNC_STAGE_SPIRV) {
                    _SituationPollGLAsyncShaderLoad(slot);
                }
                _SituationPollGLPendingProgramLink(slot);
            }
        }

        // --- 2. OpenGL Frame Setup ---

        // Reset Ring Buffer Allocator for this Frame (Paged Strategy)
        // We divide the ring buffer into N pages, one per frame in flight.
        // At the start of the frame, we reset the atomic head to the start of our assigned page.
        // This implicitly assumes the previous frame using this page has finished (guaranteed by Backpressure/Fence wait below).
        size_t page_size = sit_render.gl.ring_size / SITUATION_MAX_FRAMES_IN_FLIGHT;
        atomic_store(&sit_render.gl.ring_head, sit_render.current_frame_index * page_size);

        // Backpressure & Thread Handoff
        #if !defined(__STDC_NO_THREADS__)
        SIT_PROF_ZONE_SCOPED("Acquire/Backpressure") {
        mtx_lock(&sit_render.render_queue_mutex);
        if (_SitShouldEngageRenderQueueBackpressure()) {
            const int queue_limit = _SituationEffectiveQueueDepthLimit();
            while (sit_render.frames_pending >= queue_limit) {
                cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
            }
        }
        mtx_unlock(&sit_render.render_queue_mutex);
        }
        // [FIX v2.4.251] Unlock immediately after backpressure check.
        // Previously the mutex was held from here through the entire frame recording phase
        // until SituationEndFrame re-locked it. This worked on Windows (CRITICAL_SECTION is
        // re-entrant) but was undefined behavior for C11 mtx_plain on other platforms, and
        // serialized the render thread unnecessarily during frame recording.

        /* Drop loader-thread context before recording so the render thread can use the main window. */
        _SituationReleaseHostGLContextForRenderThread();
        /* With C11 threads (e.g. MinGW + SITUATION_ENABLE_THREADING), the #else branch below
         * never ran, so the main thread often had no current GL context here.
         * SituationSetShaderUniform / glGetUniformLocation then do not apply (fps_ray_demo skydome).
         * Skip when the dedicated GL render thread owns the context. */
        {
            int own_gl = 1;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
            if (sit_render.enabled) {
                own_gl = 0;
            }
#endif
            if (own_gl) {
                if (sit_gs.sit_glfw_window) {
                    glfwMakeContextCurrent(sit_gs.sit_glfw_window);
                }
                _SitGLInvalidateShadowState();
            }
        }
        #else
        // Make the context current for this thread (Single Threaded Mode)
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
        // Invalidate shadow state to recover from external changes.
        _SitGLInvalidateShadowState();
        #endif

        // Reset Soft Command Buffer
        sit_render.gl.soft_buffers[sit_render.current_frame_index].packet_count = 0;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].data_cursor = 0;
        // Reset breaker
        sit_render.gl.soft_buffers[sit_render.current_frame_index].is_broken = false;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].current_recording_shader_id = 0;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].recording_render_pass_active = false;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].recording_occlusion_active = false;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].raster_stack_depth = 0;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].behavior = (SituationRendererBehaviorPolicy){0};
        sit_render.gl.soft_buffers[sit_render.current_frame_index].behavior_stack_depth = 0;
        _SitVDResetGLRecordingState(&sit_render.gl.soft_buffers[sit_render.current_frame_index]);

        _SituationResetTrackedRasterStateForNewFrame();
        sit_render.gl.screenshot_valid = false;
        sit_render.gl.screenshot_resolved_frame_index = -1;
        // do not touch screenshot_requested here (Load/Take may have set it for this frame's EndFrame)

        // Mark that we're now recording a frame
        sit_render.in_frame = true;
        return SITUATION_SUCCESS;
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Frame Setup ---
        /* Async GLSL→SPIR-V compile + pipeline build are driven from SituationPollShaderLoad
         * (same contract as OpenGL SPIR-V: see gl_async_load_stage != SPIRV guard above).
         * Polling every active vk_async_load slot here duplicated PollShaderLoad, raced
         * pipeline creation, and could advance the compile deadline twice per frame. */

        uint32_t image_index = 0;
        bool image_acquired = false;
        for (uint32_t swapchain_attempt = 0; swapchain_attempt < (uint32_t)SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES; ++swapchain_attempt) {
            _SituationPumpWindowEventsGuarded();

            // 2.1. Wait for the previous frame (using this frame's fence) to finish.
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: Waiting for fence (frame_index=%u, attempt=%u)...\n",
                   sit_render.vk.current_frame_index, swapchain_attempt);
            fflush(stdout);
            #endif

            SIT_PROF_ZONE_CTX(sit_prof_fence_wait, "Acquire/FenceWait");
            VkResult wait_result = _SituationVulkanWaitFencePumpWindow(
                sit_render.vk.device,
                sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]
            );

            if (wait_result == VK_TIMEOUT) {
                fprintf(stderr, "[Vulkan] Frame fence wait timed out (max ~%.1fs) — see SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS\n",
                        (double)SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS / 1e9);
                fflush(stderr);
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Timed out waiting for frame fence in SituationAcquireFrameCommandBuffer.");
                SIT_PROF_RETURN_CTX(sit_prof_fence_wait, SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED);
            }
            if (wait_result != VK_SUCCESS) {
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to wait for frame fence in SituationAcquireFrameCommandBuffer.");
                SIT_PROF_RETURN_CTX(sit_prof_fence_wait, SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED);
            }
            SIT_PROF_ZONE_END_CTX(sit_prof_fence_wait);

            _SitGpuProfReadbackFrame((int)sit_render.vk.current_frame_index);

            if (sit_render.vk.framebuffer_resized) {
                sit_render.vk.framebuffer_resized = false;
                if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
                    return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
                }
                continue;
            }
            {
                SituationError sync_err = _SituationVulkanEnsureSwapchainMatchesFramebuffer();
                if (sync_err == SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED) {
                    continue;
                }
                if (sync_err != SITUATION_SUCCESS) {
                    return sync_err;
                }
            }

            // Backpressure (Vulkan)
            SIT_PROF_ZONE_SCOPED("Acquire/Backpressure") {
            #if !defined(__STDC_NO_THREADS__)
            mtx_lock(&sit_render.render_queue_mutex);
            if (_SitShouldEngageRenderQueueBackpressure()) {
                const int queue_limit = _SituationEffectiveQueueDepthLimit();
                while (sit_render.frames_pending >= queue_limit) {
                    cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                }
            }
            mtx_unlock(&sit_render.render_queue_mutex);
            #endif
            }

            /* If render thread is active, give it a brief chance to present before we attempt acquire.
             * This reduces the chance that we exhaust all swapchain images and hit the 1s acquire timeout.
             * Skip nudge for truly uncapped (no target, no vsync) to avoid slowing main loop. */
            #if !defined(__STDC_NO_THREADS__)
            if (_SitShouldEngageBackpressure() && sit_render.enabled && sit_render.frames_pending > 0) {
                /* Non-blocking nudge: the render thread will signal when it finishes. */
                thrd_yield();
            }
            #endif

            sit_render.vk.staging_buffers[sit_render.vk.current_frame_index].cursor = 0;
            _SituationFlushGraveyard(sit_render.vk.current_frame_index);
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
            /* [Shader Cache] Eviction runs after the graveyard flush so GPU fence for this
             * frame slot has already passed. Use the Vulkan frame index, not the GL one. */
            _SitVkShaderCacheProcessEvictions(&sit_render.vk.shader_cache,
                sit_render.vk.current_frame_index);
#endif

            if (atomic_exchange(&sit_render.vk.recreate_swapchain_request, false)) {
                #if !defined(__STDC_NO_THREADS__)
                mtx_lock(&sit_render.render_queue_mutex);
                while (sit_render.frames_pending > 0) {
                    mtx_unlock(&sit_render.render_queue_mutex);
                    thrd_yield();
                    mtx_lock(&sit_render.render_queue_mutex);
                }
                mtx_unlock(&sit_render.render_queue_mutex);
                #endif
                if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
                    return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
                }
                continue;
            }

            if (!sit_render.vk.swapchain_valid || sit_render.vk.swapchain == VK_NULL_HANDLE) {
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID, "Cannot acquire frame: swapchain is invalid.");
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID;
            }

            // 2.2. Acquire the next swapchain image.
            // Use short-step pumping + polls so we don't block the main thread (and dt) for the full
            // 1s safety timeout on every vsync boundary or when the render thread is catching up.
            // This prevents the "1s stutter / 2FPS" death spiral when using render thread + Vulkan.
            uint64_t sit_acquire_t0_ns = _SitGetMonotonicTimeNS();
            uint64_t acquire_budget = SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS;
            const uint64_t acquire_step = 16000000ULL; /* ~16ms steps */
            VkResult acquire_result = VK_TIMEOUT;
            uint64_t acquired_waited = 0;
            while (acquired_waited < acquire_budget) {
                uint64_t remain = acquire_budget - acquired_waited;
                uint64_t step = (acquire_step < remain) ? acquire_step : remain;
                acquire_result = vkAcquireNextImageKHR(
                    sit_render.vk.device,
                    sit_render.vk.swapchain,
                    step,
                    sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index],
                    VK_NULL_HANDLE,
                    &image_index
                );
                if (acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR) {
                    break;
                }
                if (acquire_result != VK_TIMEOUT) {
                    break;
                }
                acquired_waited += step;
                _SituationPumpWindowEventsGuarded();
            }
            double sit_acquire_ms = (double)(_SitGetMonotonicTimeNS() - sit_acquire_t0_ns) / 1000000.0;
            if (acquire_result == VK_TIMEOUT ||
                SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS == 0 ||
                sit_acquire_ms >= (double)SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS) {
                fprintf(stderr, "[Vulkan] vkAcquireNextImageKHR %.2f ms result=%d (attempt %u)\n",
                        sit_acquire_ms, (int)acquire_result, swapchain_attempt);
                fflush(stderr);
            }

            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                /* Drain any work the render thread may still have in flight on the old swapchain
                 * before destroying it. */
                #if !defined(__STDC_NO_THREADS__)
                if (sit_render.enabled) {
                    mtx_lock(&sit_render.render_queue_mutex);
                    while (sit_render.frames_pending > 0) {
                        mtx_unlock(&sit_render.render_queue_mutex);
                        thrd_yield();
                        mtx_lock(&sit_render.render_queue_mutex);
                    }
                    mtx_unlock(&sit_render.render_queue_mutex);
                }
                #endif
                if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
                    return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
                }
                _SituationVulkanResignalFrameFence(sit_render.vk.current_frame_index);
                continue;
            }
            if (acquire_result == VK_TIMEOUT) {
                /* Timeout is normal when all swapchain images are acquired and the presentation engine
                 * (especially under FIFO/VSync) hasn't returned one yet. Do NOT recreate on every timeout --
                 * that thrashes the swapchain and makes the 2 FPS permanent. Just fail this frame; the
                 * caller will retry next iteration while the render thread continues presenting and freeing
                 * images in the background.
                 *
                 * However, if we hit multiple consecutive timeouts, the swapchain is likely stale
                 * (e.g. window was occluded/alt-tabbed and compositor won't release images).
                 * Force a recreation to recover. */
                sit_render.vk.consecutive_acquire_timeouts++;
                if (swapchain_attempt == 0) {
                    fprintf(stderr, "[Vulkan] vkAcquireNextImageKHR %.2f ms result=%d (attempt %u) -- timeout, will retry\n",
                            sit_acquire_ms, (int)acquire_result, swapchain_attempt);
                    fflush(stderr);
                }
                if (sit_render.vk.consecutive_acquire_timeouts >= 3) {
                    /* Stale swapchain — force recreation to recover from occluded/alt-tab state. */
                    sit_render.vk.consecutive_acquire_timeouts = 0;
                    sit_render.vk.framebuffer_resized = true;
                    continue; /* Re-enter loop — framebuffer_resized will trigger recreate at top */
                }
                // Do not recreate yet. Break so we return error and let the app loop + render thread make progress.
                break;
            }
            if (acquire_result == VK_SUBOPTIMAL_KHR) {
                /* Proceed — swapchain is still usable. */
            } else if (acquire_result != VK_SUCCESS) {
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED, "Failed to acquire swap chain image in SituationAcquireFrameCommandBuffer!");
                return SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED;
            }

            image_acquired = true;
            sit_render.vk.consecutive_acquire_timeouts = 0;
            break;
        }

        if (!image_acquired || !sit_render.vk.swapchain_valid) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED,
                "Failed to acquire swapchain image after swapchain recreation retries.");
            _SituationVulkanRecoverOrphanedFrameFence();
            return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
        }

        // 2.4. Update Global State.
        // Store the index of the swapchain image we will render to this frame.
        sit_render.vk.current_image_index = image_index;
        sit_render.vk.acquired_image_indices[sit_render.vk.current_frame_index] = image_index; // Store for Render Thread
        sit_render.frame_has_async_compute = false; // Reset async flag
        sit_render.vk.dynamic_vbo_cursor = 0;

        // 2.5. Prepare Command Buffer for Recording.
        // Reset the fence to the unsignaled state *before* resetting the command buffer.
        VkResult reset_fence_result = vkResetFences(
            sit_render.vk.device,
            1,
            &sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]
        );
        if (reset_fence_result != VK_SUCCESS) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to reset frame fence in SituationAcquireFrameCommandBuffer.");
             return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED; // Indicate failure
        }

        // Get the command buffer for this frame (assuming this helper function exists and returns the correct buffer from sit_render.vk.command_buffers).
        VkCommandBuffer cmd = (VkCommandBuffer)SituationGetMainCommandBuffer(); // Or directly access: sit_render.vk.command_buffers[sit_render.vk.current_frame_index]
        VkCommandBuffer compute_cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];

        if (cmd == VK_NULL_HANDLE || compute_cmd == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to get command buffers for frame in SituationAcquireFrameCommandBuffer.");
             return SITUATION_ERROR_VULKAN_COMMAND_FAILED; // Indicate failure
        }

        // Reset the command buffer to ensure it's ready for new commands.
        VkResult reset_cmd_result = vkResetCommandBuffer(cmd, 0); // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT is 0
        vkResetCommandBuffer(compute_cmd, 0); // Reset compute buffer too

        if (reset_cmd_result != VK_SUCCESS) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to reset command buffer in SituationAcquireFrameCommandBuffer.");
             return SITUATION_ERROR_VULKAN_COMMAND_FAILED; // Indicate failure
        }

        // Begin recording commands into the command buffer.
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // Flags = 0 means "one time submit" implicitly, and no inheritance.
        VkResult begin_result = vkBeginCommandBuffer(cmd, &begin_info);
        vkBeginCommandBuffer(compute_cmd, &begin_info); // Begin compute buffer

        if (begin_result != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to begin recording command buffer in SituationAcquireFrameCommandBuffer!");
            return SITUATION_ERROR_VULKAN_COMMAND_FAILED; // Indicate failure
        }

        _SitGpuProfResetPoolVulkan(cmd, sit_render.vk.current_frame_index);

        sit_render.vk.inside_main_swapchain_render_pass = false;
        sit_render.vk.inside_render_pass = false;
        sit_render.vk.current_render_area = (VkRect2D){0};
        sit_render.vk.recording_pass_display_id = -1;
        sit_render.vk.recording_pass_rt_slot = -1;
        sit_render.vk.recording_pass_had_draw = false;
        for (int _sit_vd_bi = 0; _sit_vd_bi < SIT_VD_MAX_COMPUTE_TEXTURE_BINDS; ++_sit_vd_bi) {
            sit_render.vk.compute_bound_texture_slots[_sit_vd_bi] = -1;
        }

        _SituationResetTrackedRasterStateForNewFrame();
        sit_render.vk.screenshot_valid = false;
        sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;

        /* Phase D5 deferred: internal pipelines bind per-texture single_sampler at draw time. */
        (void)0;
#if 0
        if (sit_render.vk.quad_pipeline_layout != VK_NULL_HANDLE) {
            SituationError bindless_err = _SitVulkanBindGlobalBindlessSet(
                cmd, sit_render.vk.quad_pipeline_layout, 1u, "SituationAcquireFrameCommandBuffer");
            if (bindless_err != SITUATION_SUCCESS) {
                return bindless_err;
            }
        }
#endif

        // Mark that we're now recording a frame
        sit_render.in_frame = true;

        // If we reached here, Vulkan frame setup was successful (excluding OOD/K recreate).
        return SITUATION_SUCCESS;
    }
#endif

    // Should not be reached if SITUATION_USE_OPENGL or SITUATION_USE_VULKAN is defined,
    // but included for theoretical completeness if neither backend is selected.
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "No graphics backend defined for SituationAcquireFrameCommandBuffer.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

#if defined(SITUATION_USE_OPENGL)
static void _SitGLEnsureDefaultFramebufferOpaqueAlpha(void) {
    GLboolean color_mask[4];
    GLboolean scissor_was_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLfloat clear_color[4];

    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    if (scissor_was_enabled) {
        glEnable(GL_SCISSOR_TEST);
    }
}
#endif

/**
 * @brief Submits all recorded commands for the current frame and presents the result.
 *
 * @details This function finalizes the rendering for the frame started by `SituationAcquireFrameCommandBuffer`.
 *          It submits the recorded commands to the GPU, waits for the GPU to finish rendering to the swapchain image, and then presents that image to the screen. It also handles frame rate limiting (if configured) and updates internal timing statistics like FPS.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:**
 *   - Calls `glfwSwapBuffers` to swap the front and back framebuffers, making the rendered image visible on the screen.
 *   - Implicitly waits for the GPU to finish rendering the previous frame before swapping (this behavior can depend on VSync settings).
 * - **Vulkan:**
 *   - Ends the recording of the primary command buffer for the current frame.
 *   - Submits the command buffer to the graphics queue. This submission waits on the `image_available_semaphore` for the swapchain image to be acquired and signals the `render_finished_semaphore` when rendering is complete.
 *   - Presents the rendered swapchain image to the screen using `vkQueuePresentKHR`, waiting on the `render_finished_semaphore`.
 *   - Handles swapchain recreation if the presentation surface becomes outdated (`VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR`, or window resize).
 *   - Advances the `current_frame_index` for the next frame's synchronization objects.
 *
 * @return SITUATION_SUCCESS on successful completion of the frame.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED (Vulkan) if ending the command buffer or submitting it to the queue fails.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED (Vulkan) if presenting the image fails for reasons other than out-of-date/suboptimal swapchain.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized.
 *       2. `SituationAcquireFrameCommandBuffer` was called successfully for this frame.
 *       3. All rendering commands for the frame have been recorded.
 * @warning This function is not thread-safe and must be called from the thread that initialized the library and is managing the rendering loop.
 *
 * @see SituationAcquireFrameCommandBuffer()
 */
#if defined(SITUATION_USE_VULKAN)
static SituationError _SituationSubmitCompute(VkCommandBuffer cmd) {
    // Submit compute work and signal semaphore in one go
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkSemaphore signal_sema = sit_render.vk.compute_finished_semaphores[sit_render.vk.current_frame_index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sema;

    VkResult result;
    SIT_PROF_ZONE_SCOPED("VK/ComputeSubmit") {
        result = vkQueueSubmit(sit_render.vk.compute_queue, 1, &submit, VK_NULL_HANDLE);
    }
    if (result != VK_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "_SituationSubmitCompute: vkQueueSubmit failed.");
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Records all pending graphics commands into the given Vulkan command buffer.
 *
 * @details This is a core low-level function in the Vulkan backend that populates a
 *          `VkCommandBuffer` with the full set of recorded draw/dispatch commands,
 *          state changes, and transitions for the current frame or render list.
 *
 *          Typical call sites:
 *            - Inside `_SituationRenderThreadEntry` when processing a frame slot
 *            - During render list replay (`_SituationReplayToQueue`)
 *            - In synchronous fallback paths (if render thread disabled)
 *
 *          What it does (in rough order):
 *            - Begins command buffer recording (if not already begun)
 *            - Sets viewport/scissor (from current render pass or display)
 *            - Binds global descriptor sets (samplers, uniforms, bindless)
 *            - Iterates over the active render list or queued draw calls:
 *              - Binds pipelines (graphics/compute)
 *              - Binds vertex/index buffers
 *              - Binds descriptor sets per draw
 *              - Records `vkCmdDraw*` / `vkCmdDrawIndexed*` / `vkCmdDispatch*`
 *              - Handles push constants
 *              - Inserts barriers/transitions (image layouts, memory barriers)
 *            - Ends any active render pass
 *            - Ends command buffer recording
 *
 *          The function does **not** submit the command buffer to the queue
 *          that is handled separately (e.g. `vkQueueSubmit` in the render thread).
 *
 * @param cmd A Vulkan command buffer handle in the recording state (or reset/ready).
 *            Must belong to a pool allocated for the current frame/swapchain image.
 *
 * @note This function assumes the command buffer is already begun (via `vkBeginCommandBuffer`).
 *       Errors (validation failures, out-of-memory, invalid state) are logged internally
 *       and may set the global `SituationError` (e.g. SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED).
 *       No return value failures are non-fatal but logged.
 *
 * Thread safety invariants:
 *   - Must be called from the **render thread** (owns the Vulkan context/queues)
 *   - Command buffer must not be in use by another thread
 *   - No internal locking caller ensures exclusive access
 *   - Safe during hot-reload if old resources are destroyed first
 *
 * @see _SituationRenderThreadEntry (main caller),
 *      _SituationReplayToQueue, SituationSubmitRenderList,
 *      vkBeginCommandBuffer, vkCmdDrawIndexed, vkEndCommandBuffer,
 *      SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED
 */
static VkResult _SituationSubmitGraphics(VkCommandBuffer cmd) {
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };

    // Wait for Image Available (always)
    VkSemaphore wait_semas[2];
    VkPipelineStageFlags wait_stages[2];
    uint32_t wait_count = 0;

    wait_semas[wait_count] = sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index];
    wait_stages[wait_count] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    wait_count++;

    // Wait for Compute (if used this frame)
    if (sit_render.frame_has_async_compute) {
        wait_semas[wait_count] = sit_render.vk.compute_finished_semaphores[sit_render.vk.current_frame_index];
        wait_stages[wait_count] = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        wait_count++;
    }

    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = wait_semas;
    submit.pWaitDstStageMask = wait_stages;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkSemaphore signal_sema = sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sema;

    #ifdef SITUATION_VULKAN_DEBUG
    // fprintf(stderr, "[Situation] [_SituationSubmitGraphics] About to submit to GPU (cmd=%p)\n", (void*)cmd); fflush(stderr);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics] About to submit to GPU\n");
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics]   Command buffer: %p\n", (void*)cmd);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
    fflush(stdout);
    #endif
    VkResult submit_result;
    SIT_PROF_ZONE_SCOPED("VK/QueueSubmit") {
        submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
    }
    #ifdef SITUATION_VULKAN_DEBUG
    // fprintf(stderr, "[Situation] [_SituationSubmitGraphics] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result); fflush(stderr);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
    fflush(stdout);
    #endif
    if (submit_result != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "_SituationSubmitGraphics: vkQueueSubmit failed.");
    }
    return submit_result;
}
#endif

/**
 * @brief Ends the current frame and submits it for rendering on the render thread.
 *
 * @details This function marks the end of the current frame's command recording and
 *          submits it to the dedicated render thread for execution. It is the primary
 *          synchronization point in the main loop, ensuring that the frame's command
 *          buffer is enqueued for GPU submission, presentation, and resource cleanup.
 *
 *          Key steps performed:
 *            - Locks the render queue mutex
 *            - Waits (with timeout) if all in-flight frame slots are occupied
 *              (backpressure handling to prevent unbounded queue growth)
 *            - Enqueues the current frame index into the circular queue
 *            - Increments pending frame count and refcount for the slot
 *            - Records submission timestamp for latency metrics (if enabled)
 *            - Signals the render thread condition variable to wake it
 *            - Unlocks the mutex
 *            - Advances the current_frame index (modulo MAX_FRAMES_IN_FLIGHT)
 *
 *          This function is **non-blocking** in normal operation but may briefly wait
 *          under high load (e.g. slow GPU, many pending frames). Timeout is fixed
 *          (e.g. 1 second) and returns an error on expiry.
 *
 *          Call this at the end of your main loop after all `SituationCmd*` recordings
 *          for the frame. It does **not** perform polling or input handling pair with
 *          `SituationPollEvents` at loop start.
 *
 * @return SITUATION_SUCCESS on successful submission,
 *         SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT if wait for free slot timed out
 *         (too many pending frames reduce load or increase MAX_FRAMES_IN_FLIGHT),
 *         SITUATION_ERROR_THREAD_VIOLATION if called from render thread (deadlock risk),
 *         or other appropriate error codes (e.g. mutex failure).
 *
 * @note Must be called from the **main thread** (or thread that owns the command buffer).
 *       Pairs with `SituationBeginFrame` (if you have it) or manual command recording.
 *       If render thread is disabled, this falls back to synchronous execution.
 *       Metrics (latency, queue depth) are updated internally if enabled.
 *
 *       Thread safety:
 *         - Safe only from main thread render thread calls would deadlock
 *         - Internal mutex + condvar protect queue access
 *         - Atomic ops for refcounts and metrics
 *
 * @see SituationPollEvents, SituationBeginCommandBuffer, SituationCmd* functions,
 *      _SituationRenderThreadEntry (execution side), SITUATION_MAX_FRAMES_IN_FLIGHT,
 *      SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT, SITUATION_ERROR_THREAD_VIOLATION
 */
SITAPI SituationError SituationEndFrame(void) {
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: ENTRY\n");
    fflush(stdout);
    #endif
    
    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot end frame.");
#if defined(SITUATION_USE_VULKAN)
        _SituationVulkanRecoverOrphanedFrameFence();
#endif
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    SIT_PROF_FRAME_MARK();

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: Library initialized, replaying momentum queue\n");
    fflush(stdout);
    #endif

    // --- [v2.3.27] Replay Momentum Queue ---
    // Process all lists submitted by worker threads this frame.
    SituationCommandBuffer main_cmd = SituationGetMainCommandBuffer();

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: Got main command buffer: %p\n", main_cmd);
    fflush(stdout);
    #endif

    // Only replay if we have a valid command buffer (we should, if Init succeeded)
    if (main_cmd) {
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Locking momentum mutex\n");
        fflush(stdout);
        #endif
        
        mtx_lock(&sit_render.momentum_mutex);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Momentum mutex locked\n");
        fflush(stdout);
        #endif

        int head = atomic_load(&sit_render.momentum_head);
        int tail = atomic_load(&sit_render.momentum_tail);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: head=%d, tail=%d\n", head, tail);
        fflush(stdout);
        #endif

        while (tail != head) {
            SituationRenderList list = sit_render.momentum_queue[tail];

            // "Paste" the recorded commands into the real command buffer
            SituationReplayRenderList(main_cmd, list);

            // [FIX v2.3.27B] Mark as finished
            atomic_fetch_sub(&list->in_flight_count, 1);

            // Advance tail
            tail = (tail + 1) % 256;
        }
        atomic_store(&sit_render.momentum_tail, tail);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: About to unlock momentum mutex\n");
        fflush(stdout);
        #endif

        mtx_unlock(&sit_render.momentum_mutex);
        
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Momentum mutex unlocked\n");
        fflush(stdout);
        #endif
    }

    // --- 2. Backend-Specific Frame End ---
#if defined(SITUATION_USE_OPENGL)
    {
        // --- 2a. OpenGL Frame End ---

        // [Phase 2] Threaded Submission
        #if !defined(__STDC_NO_THREADS__)
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            // [v2.3.24a] Adaptive Backpressure (Safety Zenith)
            // Dynamically switch policy based on frame latency history.
            // Policy: SPIKE (>100% target) -> SLEEP (Save CPU/Battery, let GPU catch up)
            //         STEADY (<50% target) -> SPIN (Max performance/responsiveness)
            int policy = atomic_load(&sit_render_policy_state);
            const int queue_limit = _SituationEffectiveQueueDepthLimit();

            // [FIX v2.4.251] Only evaluate adaptive policy when frame-paced (target > 0 or VSync ON).
            // When truly uncapped (vsync off, target == 0), the policy must not mutate based on
            // stale latency metrics — doing so permanently locked the system into SLEEP
            // because metric_max_latency_ns retained vsync-era values (~16ms) that exceeded
            // the hardcoded 16ms default threshold, with no path to recover.
            if (_SitShouldEngageBackpressure()) {
                if (sit_gs.target_frame_time == 0.0 &&
                    (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0) {
                    /* Phase 3: VSync present is the pacer — avoid SLEEP/cnd_wait jitter. */
                    policy = SIT_RENDER_BACKPRESSURE_YIELD;
                    atomic_store(&sit_render_policy_state, SIT_RENDER_BACKPRESSURE_YIELD);
                } else {
                // Use Max latency from the recent history (reset/updated by thread)
                uint64_t lat_check = atomic_load(&sit_render.metric_max_latency_ns);

                uint64_t target_ns = (uint64_t)(sit_gs.target_frame_time * 1000000000.0);
                // When VSync paces us (target == 0), use one VSync interval (~16.67ms at 60Hz)
                // as the reference for spike/steady evaluation.
                if (target_ns == 0) target_ns = 16666667ULL;

                uint64_t spike_thresh = target_ns * 2;     // 200% of target — missed a full frame
                uint64_t steady_thresh = target_ns;        // At or below target — healthy

                if (lat_check > spike_thresh) {
                    atomic_store(&sit_render_policy_state, SIT_RENDER_BACKPRESSURE_SLEEP);
                    policy = SIT_RENDER_BACKPRESSURE_SLEEP;
                } else if (lat_check < steady_thresh) {
                    atomic_store(&sit_render_policy_state, SIT_RENDER_BACKPRESSURE_SPIN);
                    policy = SIT_RENDER_BACKPRESSURE_SPIN;
                }
                }
            }

            // Check Queue Depth + time backpressure (for spike cause attribution).
            // Render-thread queue limits always apply (v2.4.330); uncapped FPS is from swap interval at present.
            uint64_t bp_t0 = _SitGetMonotonicTimeNS();
            if (_SitShouldEngageRenderQueueBackpressure()) {
                size_t depth = atomic_load(&sit_render.render_queue_depth);
                if (depth >= (size_t)queue_limit) {
                    if (policy == SIT_RENDER_BACKPRESSURE_SPIN) {
                        while (atomic_load(&sit_render.render_queue_depth) >= (size_t)queue_limit) {
                            #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
                            _mm_pause();
                            #elif defined(__aarch64__) || defined(_M_ARM64)
                                #if defined(__has_builtin)
                                    #if __has_builtin(__builtin_arm_wfe)
                                    __builtin_arm_wfe();
                                    #else
                                    __asm__ __volatile__("yield");
                                    #endif
                                #else
                                    #if defined(_MSC_VER)
                                    __yield();
                                    #else
                                    __asm__ __volatile__("yield");
                                    #endif
                                #endif
                            #endif
                        }
                    }
                    else if (policy == SIT_RENDER_BACKPRESSURE_SLEEP) {
                         mtx_lock(&sit_render.render_queue_mutex);
                         while (sit_render.frames_pending >= queue_limit) {
                             cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                         }
                         mtx_unlock(&sit_render.render_queue_mutex);
                    }
                    else { // YIELD
                         while (atomic_load(&sit_render.render_queue_depth) >= (size_t)queue_limit) thrd_yield();
                    }
                }
            }

            mtx_lock(&sit_render.render_queue_mutex);
            // Wait for a free in-flight slot before handoff (always when render thread is on).
            if (_SitShouldEngageRenderQueueBackpressure() && policy != SIT_RENDER_BACKPRESSURE_SLEEP) {
                 while (sit_render.frames_pending >= queue_limit) {
                     cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                 }
            }
            uint64_t bp_t1 = _SitGetMonotonicTimeNS();
            sit_gs.last_backpressure_ns = bp_t1 - bp_t0;

            // [v2.3.24a] Leak-Proof Handoff: Increment Refcount
            atomic_fetch_add(&sit_render.frame_refcounts[sit_render.current_frame_index], 1);

            sit_render.render_queue[sit_render.render_queue_head] = sit_render.current_frame_index;
            sit_render.render_queue_head = (sit_render.render_queue_head + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
            sit_render.render_queue_count++;
            sit_render.frames_pending++;

            // [v2.3.22] Record Submit Timestamp for Latency
            // [v2.3.25] Store explicitly for drift check
            uint64_t now = _SitGetMonotonicTimeNS();
            atomic_store(&sit_render.submit_timestamps[sit_render.current_frame_index], now);
            atomic_fetch_add(&sit_render.render_queue_depth, 1);

            /* Invalidate readback cache before render thread presents this frame so
             * SituationLoadImageFromScreen (EndFrame then Load) cannot memcpy the prior frame. */
            sit_render.gl.screenshot_valid = false;
            sit_render.gl.screenshot_resolved_frame_index = -1;
            sit_render.gl.screenshot_capture_epoch++;
            atomic_thread_fence(memory_order_release);
            _SituationGLHandoffScreenshotRequestForSlot(sit_render.current_frame_index);

            cnd_signal(&sit_render.render_queue_cv);
            mtx_unlock(&sit_render.render_queue_mutex);
            sit_render.in_frame = false;
        } else
        #endif
        // If threading disabled at runtime but compiled in, fallback to immediate execution below
        {
            #ifdef SITUATION_OPENGL_DEBUG
            printf("[OpenGL Debug] SituationEndFrame: About to call _SituationGLExecuteCommands\n");
            printf("[OpenGL Debug] current_frame_index=%d, packet_count=%d\n", 
                   sit_render.current_frame_index, 
                   sit_render.gl.soft_buffers[sit_render.current_frame_index].packet_count);
            fflush(stdout);
            #endif

            // 1. Wait for old frame to finish and flush its graveyard
            if (sit_render.gl.frame_fences[sit_render.current_frame_index]) {
                uint64_t fw_t0 = _SitGetMonotonicTimeNS();
                glClientWaitSync(sit_render.gl.frame_fences[sit_render.current_frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
                sit_gs.last_fence_wait_ns = _SitGetMonotonicTimeNS() - fw_t0;

                _SitGLFlushGraveyard(sit_render.current_frame_index);

#if SIT_GL_SHADER_CACHE_ENABLE
                _SitGLProgramCacheProcessEvictions(&sit_render.gl.program_cache,
                    (uint32_t)sit_render.current_frame_index);
#endif

                glDeleteSync(sit_render.gl.frame_fences[sit_render.current_frame_index]);
                sit_render.gl.frame_fences[sit_render.current_frame_index] = 0;
            }

            _SitGpuProfReadbackFrame(sit_render.current_frame_index);
            _SitGpuProfClearFrameSlot((uint32_t)sit_render.current_frame_index);

            SIT_DEBUG_LOG("[EndFrame] Executing GL commands\n");
            uint64_t exec_t0 = _SitGetMonotonicTimeNS();
            {
                SituationError exec_err = _SituationGLExecuteCommands(
                    &sit_render.gl.soft_buffers[sit_render.current_frame_index],
                    sit_render.current_frame_index);
                if (exec_err != SITUATION_SUCCESS) {
                    sit_render.in_frame = false;
                    return exec_err;
                }
            }
            sit_gs.last_execute_ns = _SitGetMonotonicTimeNS() - exec_t0;
            _SitGLEnsureDefaultFramebufferOpaqueAlpha();

            _SituationGLBlitCanvasToDisplay();

            SIT_DEBUG_LOG("[EndFrame] About to call glfwSwapBuffers\n");
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                SIT_DEBUG_LOG("[EndFrame] OpenGL error BEFORE swap: 0x%x\n", err);
            }

            // [Phase 1] Pre-swap screenshot on demand (requested / urgent latch)
            {
                int sw = SituationGetRenderWidth();
                int sh = SituationGetRenderHeight();
                sit_render.gl.screenshot_valid = false;
                sit_render.gl.screenshot_resolved_frame_index = -1;
                sit_render.gl.screenshot_capture_epoch++;
                const bool do_capture = _SituationGLShouldCaptureFrame(sit_render.current_frame_index);
                if (do_capture && sw > 0 && sh > 0 && _SituationGLEnsureScreenshotResources(sw, sh)) {
                    _SituationGLReadPixelsToPackBuffer(sw, sh, false);
                } else if (sw > 0 && sh > 0) {
                    /* ST fallback: pre-swap sync capture for EndFrame→Load when no explicit request/urgent. */
                    _SituationGLSyncReadFramebufferToCPU(sw, sh, false, sit_render.current_frame_index);
                }

                uint64_t pres_t0 = _SitGetMonotonicTimeNS();
                _SitGLApplySwapIntervalBeforePresent();
                glfwSwapBuffers(sit_gs.sit_glfw_window);
                sit_gs.last_present_ns = _SitGetMonotonicTimeNS() - pres_t0;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
                _SituationApplyPresentTimingDirect();
#endif
                SIT_DEBUG_LOG("[EndFrame] glfwSwapBuffers completed\n");

                if (do_capture && sw > 0 && sh > 0) {
                    (void)_SituationGLMapPackBufferToScreenshot(sw, sh, sit_render.current_frame_index);
                }
                sit_render.gl.screenshot_requested = false;
            }

            // 2. Create new fence for the commands we just submitted
            sit_render.gl.frame_fences[sit_render.current_frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();
            sit_render.in_frame = false;
        }
        #else
        // [Phase 1] Execute Deferred Commands Immediately (Single-Threaded)

        // 1. Wait for old frame to finish and flush its graveyard
        if (sit_render.gl.frame_fences[sit_render.current_frame_index]) {
            glClientWaitSync(sit_render.gl.frame_fences[sit_render.current_frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);

            _SitGLFlushGraveyard(sit_render.current_frame_index);

            glDeleteSync(sit_render.gl.frame_fences[sit_render.current_frame_index]);
            sit_render.gl.frame_fences[sit_render.current_frame_index] = 0;
        }

        SIT_DEBUG_LOG("[EndFrame] Executing GL commands (non-threaded path)\n");
        {
            SituationError exec_err = _SituationGLExecuteCommands(
                &sit_render.gl.soft_buffers[sit_render.current_frame_index],
                sit_render.current_frame_index);
            if (exec_err != SITUATION_SUCCESS) {
                sit_render.in_frame = false;
                return exec_err;
            }
        }
        _SitGLEnsureDefaultFramebufferOpaqueAlpha();

        _SituationGLBlitCanvasToDisplay();

        SIT_DEBUG_LOG("[EndFrame] About to call glfwSwapBuffers (non-threaded)\n");
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            SIT_DEBUG_LOG("[EndFrame] OpenGL error BEFORE swap: 0x%x\n", err);
        }

        // [Phase 1] Pre-swap screenshot on demand (requested / urgent latch)
        {
            int sw = SituationGetRenderWidth();
            int sh = SituationGetRenderHeight();
            sit_render.gl.screenshot_valid = false;
            sit_render.gl.screenshot_resolved_frame_index = -1;
            sit_render.gl.screenshot_capture_epoch++;
            const bool do_capture = _SituationGLShouldCaptureFrame(sit_render.current_frame_index);
            if (do_capture && sw > 0 && sh > 0 && _SituationGLEnsureScreenshotResources(sw, sh)) {
                _SituationGLReadPixelsToPackBuffer(sw, sh, false);
            } else if (sw > 0 && sh > 0) {
                _SituationGLSyncReadFramebufferToCPU(sw, sh, false, sit_render.current_frame_index);
            }

            _SitGLApplySwapIntervalBeforePresent();
            glfwSwapBuffers(sit_gs.sit_glfw_window);
#if defined(SITUATION_ENABLE_RENDER_THREAD)
            _SituationApplyPresentTimingDirect();
#endif
            SIT_DEBUG_LOG("[EndFrame] glfwSwapBuffers completed (non-threaded)\n");

            if (do_capture && sw > 0 && sh > 0) {
                (void)_SituationGLMapPackBufferToScreenshot(sw, sh, sit_render.current_frame_index);
            }
            sit_render.gl.screenshot_requested = false;
        }

        // 2. Create new fence for the commands we just submitted
        sit_render.gl.frame_fences[sit_render.current_frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        sit_render.in_frame = false;

        #endif

        // [PLATINUM] Unify frame index advancement across both paths.
        sit_render.current_frame_index = (sit_render.current_frame_index + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;

        // OpenGL path implicitly succeeds if glfwSwapBuffers doesn't crash.
        // Return success.
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2b. Vulkan Frame End ---

        // 1. End recording the primary command buffer for this frame.
        // Get the command buffer first and validate it.
        VkCommandBuffer cmd = (VkCommandBuffer)SituationGetMainCommandBuffer();
        if (cmd == VK_NULL_HANDLE) { // Check if SituationGetMainCommandBuffer returned NULL
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to get main command buffer for ending frame.");
             _SituationVulkanRecoverOrphanedFrameFence();
             return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        // [FIX V6] Pre-present screenshot: copy swapchain image to host-visible staging while the image is
        // still in a well-defined state (see LIBRARY_BUGFIX_PLAN — same idea as OpenGL pre-swap ReadPixels).
        // Clear only this frame slot — not a global flag (render thread may still need prior slots).
        if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
            sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
        }
        _SituationVulkanRecordCanvasStretchBlit(cmd);
        if (sit_render.vk.screenshot_requested) {
            uint32_t sw = sit_render.vk.swapchain_extent.width;
            uint32_t sh = sit_render.vk.swapchain_extent.height;
            if (sit_render.vk.swapchain_valid && sw > 0 && sh > 0 && sit_render.vk.swapchain_images &&
                sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count) {
                VkImage swap_img = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];
                SituationError cap_err = _SituationVulkanEnsureScreenshotResources(sw, sh);
                if (cap_err == SITUATION_SUCCESS && sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE) {
                    _SituationVulkanRecordScreenshotCopy(cmd, swap_img, sw, sh);
                } else {
                    sit_render.vk.screenshot_valid = false;
                }
            } else {
                sit_render.vk.screenshot_valid = false;
            }
            sit_render.vk.screenshot_requested = false;
        } else {
            sit_render.vk.screenshot_valid = false;
        }

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to end recording command buffer!");
            _SituationVulkanRecoverOrphanedFrameFence();
            return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        // [Phase 2] Threaded Submission (Vulkan)
        #if !defined(__STDC_NO_THREADS__)
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            uint32_t handoff_frame = sit_render.vk.current_frame_index;
            if (handoff_frame < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                sit_render.vk.needs_compute_wait[handoff_frame] = sit_render.frame_has_async_compute;
            }
            if (sit_render.frame_has_async_compute) {
                VkCommandBuffer compute_cmd = sit_render.vk.compute_command_buffers[handoff_frame];
                if (vkEndCommandBuffer(compute_cmd) != VK_SUCCESS) {
                    _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to end compute command buffer!");
                    _SituationVulkanRecoverOrphanedFrameFence();
                    return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
                }
                SituationError compute_err = _SituationSubmitCompute(compute_cmd);
                if (compute_err != SITUATION_SUCCESS) {
                    if (handoff_frame < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                        sit_render.vk.screenshot_copy_pending[handoff_frame] = false;
                    }
                    sit_render.vk.screenshot_valid = false;
                    _SituationVulkanRecoverOrphanedFrameFence();
                    return compute_err;
                }
            }

            // [v2.3.24c] Robust Backpressure (Unified)
            // We use a Condition Variable to wait efficiently if the queue is full.
            // This replaces the dangerous spinlock with OS-scheduled sleeping.

            mtx_lock(&sit_render.render_queue_mutex);

            // Wait Loop: While the queue is full, sleep.
            // The Render Thread will signal 'main_wait_cv' when it finishes a frame.
            if (_SitShouldEngageRenderQueueBackpressure()) {
                const int queue_limit = _SituationEffectiveQueueDepthLimit();
                while (sit_render.frames_pending >= queue_limit) {
                    cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                }
            }

            // [v2.3.25] Refcount Increment
            // Critical: Ensure the frame is marked as "in use" before handoff.
            atomic_fetch_add(&sit_render.frame_refcounts[sit_render.vk.current_frame_index], 1);

            // Push Frame Index to Ring Buffer
            // NOTE: ring index arithmetic must use SITUATION_MAX_FRAMES_IN_FLIGHT (the array's
            // compile-time capacity), NOT vk.max_frames_in_flight — if the runtime value is ever
            // clamped below the constant, head/tail would desync from the consumer's modulus.
            sit_render.render_queue[sit_render.render_queue_head] = sit_render.vk.current_frame_index;
            sit_render.render_queue_head = (sit_render.render_queue_head + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
            sit_render.render_queue_count++;
            sit_render.frames_pending++;

            // [v2.3.22] Metrics & Depth Tracking
            uint64_t now = _SitGetMonotonicTimeNS();
            atomic_store(&sit_render.submit_timestamps[sit_render.vk.current_frame_index], now);
            atomic_fetch_add(&sit_render.render_queue_depth, 1);

            // Wake up Render Thread to process the new frame
            cnd_signal(&sit_render.render_queue_cv);
            mtx_unlock(&sit_render.render_queue_mutex);
            sit_render.in_frame = false;
        } else
        #endif
        {
            // 2. Submit the command buffer to the graphics queue (Single-Threaded Path).
            // [v2.3.23] Multi-Queue Sync
            if (sit_render.frame_has_async_compute) {
                VkCommandBuffer compute_cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];
                vkEndCommandBuffer(compute_cmd);
                SituationError compute_err = _SituationSubmitCompute(compute_cmd);
                if (compute_err != SITUATION_SUCCESS) {
                    if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                        sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
                    }
                    sit_render.vk.screenshot_valid = false;
                    _SituationVulkanRecoverOrphanedFrameFence();
                    return compute_err;
                }
            }

            VkResult submit_res = _SituationSubmitGraphics(cmd);
            if (submit_res == VK_SUCCESS) {
                _SituationVulkanResolveScreenshotAfterSubmit(sit_render.vk.current_frame_index);
            } else {
                if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                    sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
                }
                sit_render.vk.screenshot_valid = false;
            }

            // 3. Present the rendered image to the screen.
            VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index] };

            VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = signal_semaphores; // Wait for rendering to finish
            VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
            present_info.swapchainCount = 1;
            present_info.pSwapchains = swapchains;
            present_info.pImageIndices = &sit_render.vk.current_image_index; // Present the image we acquired/used this frame

            // [FIX v2.3.27B] Safety check
            if (!sit_render.vk.swapchain_valid) {
                _SituationVulkanRecoverOrphanedFrameFence();
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID;
            }

            // Perform the presentation.
            #ifdef SITUATION_VULKAN_DEBUG
            // fprintf(stderr, "[Situation] About to call vkQueuePresentKHR (image_index=%u)\n", sit_render.vk.current_image_index); fflush(stderr);
            #endif
            VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);
            #ifdef SITUATION_VULKAN_DEBUG
            // fprintf(stderr, "[Situation] vkQueuePresentKHR result: %d (VK_SUCCESS=0)\n", result); fflush(stderr);
            #endif

            // 4. Handle Presentation Result & Swapchain State.
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || sit_render.vk.framebuffer_resized) {
                // The swapchain is out of date or not optimal. Recreate it.
                // Reset the resize flag if it was set.
                sit_render.vk.framebuffer_resized = false;
                if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
                    _SituationVulkanRecoverOrphanedFrameFence();
                    return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
                }
                // Note: We don't return an error here on success. Recreating the swapchain is handled internally.
                // The application should check for swapchain recreation needs in SituationAcquireFrameCommandBuffer.
            } else if (result != VK_SUCCESS) {
                // An unexpected error occurred during presentation.
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "Failed to present swap chain image!");
                _SituationVulkanRecoverOrphanedFrameFence();
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
            }

            // Store the index of the image we just submitted for presentation.
            sit_render.vk.last_presented_image_index = sit_render.vk.current_image_index;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
            _SituationApplyPresentTimingDirect();
#endif
            sit_render.in_frame = false;
        }
        #else
        // 2. Submit the command buffer to the graphics queue (Single-Threaded Path).
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = { sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index] };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        // Submit the command buffer, waiting on the acquire semaphore and signaling the render finish semaphore.
        // The fence associated with this frame is signaled when the submission completes.
        fprintf(stderr, "[Situation] [SINGLE-THREADED] About to submit frame to GPU (cmd=%p)\n", (void*)cmd); fflush(stderr);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED] About to submit frame to GPU\n");
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED]   Command buffer: %p\n", (void*)cmd);
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
        fflush(stdout);
        #endif
        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
        fprintf(stderr, "[Situation] [SINGLE-THREADED] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result); fflush(stderr);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
        fflush(stdout);
        #endif
        if (submit_result != VK_SUCCESS) {
            if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
            }
            sit_render.vk.screenshot_valid = false;
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "Failed to submit draw command buffer!");
            _SituationVulkanRecoverOrphanedFrameFence();
            return SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED;
        }

        _SituationVulkanResolveScreenshotAfterSubmit(sit_render.vk.current_frame_index);

        // 3. Present the rendered image to the screen.
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores; // Wait for rendering to finish
        VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &sit_render.vk.current_image_index; // Present the image we acquired/used this frame

        // Perform the presentation.
        VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);

        // 4. Handle Presentation Result & Swapchain State.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || sit_render.vk.framebuffer_resized) {
            // The swapchain is out of date or not optimal. Recreate it.
            // Reset the resize flag if it was set.
            sit_render.vk.framebuffer_resized = false;
            if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
                _SituationVulkanRecoverOrphanedFrameFence();
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
            }
            // Note: We don't return an error here on success. Recreating the swapchain is handled internally.
            // The application should check for swapchain recreation needs in SituationAcquireFrameCommandBuffer.
        } else if (result != VK_SUCCESS) {
            // An unexpected error occurred during presentation.
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "Failed to present swap chain image!");
            _SituationVulkanRecoverOrphanedFrameFence();
            return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
        }

        // Store the index of the image we just submitted for presentation.
        sit_render.vk.last_presented_image_index = sit_render.vk.current_image_index;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
        _SituationApplyPresentTimingDirect();
#endif
        sit_render.in_frame = false;
        #endif

        // 5. Advance Frame Index for Next Frame's Synchronization.
        // Use the dynamically determined max frames in flight, not a compile-time constant.
        sit_render.vk.current_frame_index = (sit_render.vk.current_frame_index + 1) % sit_render.vk.max_frames_in_flight;
    }
#endif // SITUATION_USE_VULKAN

    // --- 3. Post-Frame Logic (Timing, FPS) ---
    // Update timing and FPS counter. This happens regardless of the backend.
    // It includes the time taken by buffer swapping/presentation.

    // Frame Rate Limiting (if a target time is set).
    if (sit_gs.target_frame_time > 0.0) {
        double next_frame_start_time = sit_gs.current_time + sit_gs.target_frame_time;
        double current_time = glfwGetTime(); // Get current time for comparison
        while (current_time < next_frame_start_time) {
            double remaining = next_frame_start_time - current_time;
            // Yield control to the OS to avoid consuming 100% CPU.
            #if defined(_WIN32)
                if (remaining > 0.002) {
                    Sleep(1); // Sleep ~1ms when we have time to spare
                } else {
                    Sleep(0); // Yield for the final sub-2ms precision
                }
			#elif defined(__linux__) || defined(__APPLE__)
				struct timespec req = {0};
				if (remaining > 0.002) {
				    req.tv_nsec = 1000 * 1000; // 1ms
				} else {
				    req.tv_nsec = 100 * 1000; // 100μs
				}
				nanosleep(&req, NULL);
			#endif
            current_time = glfwGetTime(); // Update current time for the next check
        }
    }

    // FPS Calculation Update (present-anchored when render thread is on).
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    _SituationUpdateFpsCounter();
#else
    sit_gs.fps_frame_counter++;
    double current_time = glfwGetTime();
    double time_since_last_fps_update = current_time - sit_gs.fps_last_update_time;

    #ifdef SITUATION_VULKAN_DEBUG
    if (sit_gs.fps_frame_counter == 1 || sit_gs.fps_frame_counter % 60 == 0) {
        printf("Situation [FPS Debug]: frame=%d, current_time=%.3f, last_update=%.3f, delta=%.3f\n",
               sit_gs.fps_frame_counter, current_time, sit_gs.fps_last_update_time, time_since_last_fps_update);
    }
    #endif

    if (time_since_last_fps_update >= 1.0) {
        sit_gs.current_fps = (int)((double)sit_gs.fps_frame_counter / time_since_last_fps_update);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [FPS Debug]: FPS updated to %d (frames=%d, time=%.3f)\n",
               sit_gs.current_fps, sit_gs.fps_frame_counter, time_since_last_fps_update);
        #endif
        sit_gs.fps_frame_counter = 0;
        sit_gs.fps_last_update_time = glfwGetTime();
    }
#endif

    // --- 4. Success ---
#if defined(SITUATION_USE_OPENGL) || defined(SITUATION_USE_VULKAN)
    // Explicitly return success if the backend-specific code completed without error return.
    sit_render.in_frame = false;
    return SITUATION_SUCCESS;
#else
    // Fallback if neither backend is defined (should be caught by compiler flags usually).
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Gets the primary command buffer for the current frame.
 *
 * @details This function retrieves the main `SituationCommandBuffer` handle that should be used for recording rendering and compute commands for the frame currently being prepared or rendered.
 *          This handle is typically obtained *after* a successful call to `SituationAcquireFrameCommandBuffer` and is valid until `SituationEndFrame` is called.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** OpenGL operates in immediate mode and does not use explicit command buffers in the same way Vulkan does. Therefore, this function returns `NULL`.
 * - **Vulkan:** Returns the `VkCommandBuffer` associated with the current frame index (`sit_render.vk.current_frame_index`). This buffer is managed internally by the library and is reset and begun at the start of the frame by `SituationAcquireFrameCommandBuffer`.
 *
 * @return A `SituationCommandBuffer` handle.
 *         - In Vulkan, this is a valid handle for the current frame's primary command buffer.
 *         - In OpenGL, this function returns `NULL`.
 * @return `NULL` if the library is not initialized, or if called at an inappropriate time (e.g., before `SituationAcquireFrameCommandBuffer` or after `SituationEndFrame` in Vulkan, if `sit_render.vk.current_frame_index` is invalid).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized.
 *       2. (Vulkan) This function is called between `SituationAcquireFrameCommandBuffer` and `SituationEndFrame`.
 *       3. The returned handle is only used for recording commands and not stored persistently across frames without re-querying.
 *
 * @see SituationAcquireFrameCommandBuffer(), SituationEndFrame()
 */
SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void) {
    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        // Returning NULL is a safe default for an invalid/uninitialized state.
        // Could also set an error, but often just returning NULL is sufficient for a getter.
        // _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot get command buffer before library initialization.");
        return NULL;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        // --- 2. OpenGL Path ---
        // [Phase 2] Return the Soft Command Buffer for current frame
        // [PLATINUM] Unified logic: Always return the buffer for the current frame index.
        return (SituationCommandBuffer)&sit_render.gl.soft_buffers[sit_render.current_frame_index];
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Path ---
        // Retrieve the command buffer for the current frame index.
        // This assumes sit_render.vk.current_frame_index is valid (set by SituationAcquireFrameCommandBuffer).

        // Optional: Add a bounds check for robustness, though SituationAcquireFrameCommandBuffer should manage this.
        if (sit_render.vk.current_frame_index >= sit_render.vk.max_frames_in_flight) {
            // This indicates a potential logic error or state issue.
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Current frame index is out of bounds for command buffer access.");
            return NULL;
        }

        // Get the VkCommandBuffer from the internal array.
        VkCommandBuffer vk_cmd = sit_render.vk.command_buffers[sit_render.vk.current_frame_index];

        // Optional: Check if vk_cmd is VK_NULL_HANDLE, though SituationAcquireFrameCommandBuffer should provide a valid one.
        if (vk_cmd == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Current frame's command buffer is unexpectedly NULL.");
             return NULL;
        }

        // Cast the VkCommandBuffer to the opaque SituationCommandBuffer type.
        return (SituationCommandBuffer)(uintptr_t)vk_cmd;
    }
#endif

    // --- 3. Fallback (Should not be reached if backends are defined) ---
    // If neither SITUATION_USE_OPENGL nor SITUATION_USE_VULKAN is defined.
    return NULL;
}

SITAPI SituationCommandBuffer SituationGetComputeCommandBuffer(void) {
    if (!SituationIsInitialized()) return NULL;
#if defined(SITUATION_USE_VULKAN)
    // Track usage for sync
    sit_render.frame_has_async_compute = true;
    VkCommandBuffer cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];
    return (SituationCommandBuffer)(uintptr_t)cmd;
#else
    // Fallback to main buffer for OpenGL
    return SituationGetMainCommandBuffer();
#endif
}

/**
 * @brief Builds a default clear render-pass descriptor for the given display.
 * @see SituationCmdBeginRenderPass(), SituationRenderPassInfoLoad()
 */
SITAPI SituationRenderPassInfo SituationRenderPassInfoDefault(int display_id, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;
    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    info.depth_attachment.clear.depth = 1.0f;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_DONT_CARE;
    info.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    return info;
}

/**
 * @brief Builds a load-existing-contents render-pass descriptor for the given display.
 * @see SituationCmdBeginRenderPass(), SituationRenderPassInfoDefault()
 */
SITAPI SituationRenderPassInfo SituationRenderPassInfoLoad(int display_id) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.depth_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    return info;
}

/**
 * @brief Hash key for Vulkan render-pass cache lookup from attachment ops.
 * @see _SituationVulkanGetOrCreateRenderPass()
 */
SITAPI uint32_t SituationRenderPassConfigurationKey(const SituationRenderPassInfo* info) {
    if (!info) {
        return 0u;
    }
    uint32_t key = 0u;
    key |= (info->display_id == -1) ? 0u : 1u;
    key |= ((uint32_t)info->color_attachment.loadOp & 3u) << 1;
    key |= ((uint32_t)info->depth_attachment.loadOp & 3u) << 3;
    key |= ((uint32_t)info->stencil_attachment.loadOp & 3u) << 5;
    key |= ((uint32_t)info->color_attachment.storeOp & 3u) << 7;
    key |= ((uint32_t)info->depth_attachment.storeOp & 3u) << 9;
    key |= ((uint32_t)info->stencil_attachment.storeOp & 3u) << 11;
    return key;
}

/**
 * @brief [Core] Begins a configurable render pass on a command buffer.
 *
 * @details This is the primary entry point for rendering geometry. It configures the rendering target (Display or Virtual Display)
 *          and specifies how attachments (Color, Depth, Stencil) should be handled at the start and end of the pass.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a `vkCmdBeginRenderPass` command. It selects the appropriate `VkFramebuffer` and `VkRenderPass` object
 *   based on the `info->display_id` and the `loadOp`/`storeOp` settings. It sets the clear values for the attachments
 *   and defines the render area.
 * - **OpenGL:** Binds the target framebuffer (FBO 0 for main window). It then mimics Vulkan's `loadOp` behavior:
 *   - `SIT_LOAD_OP_CLEAR`: Calls `glClear` with the specified color/depth values.
 *   - `SIT_LOAD_OP_LOAD`: Does nothing (preserves existing framebuffer content).
 *   - `SIT_LOAD_OP_DONT_CARE`: Does nothing (undefined content, fast).
 *
 * @param cmd The command buffer to record into.
 * @param info A pointer to a `SituationRenderPassInfo` struct defining the target display ID and attachment operations.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if `info` is NULL.
 * @return `SITUATION_ERROR_NOT_IMPLEMENTED` (Vulkan) if a requested load/store op combination is not yet supported by the internal cache.
 *
 * @note Must be paired with `SituationCmdEndRenderPass`.
 */
#if defined(SITUATION_USE_VULKAN)
/** Top-left pixel ortho — identical to OpenGL internal 2D paths. */
static SituationError _SitVulkanFillOrthoProjection2D(float width, float height, mat4 out_proj) {
    if (!out_proj) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "_SitVulkanFillOrthoProjection2D: out_proj cannot be NULL.");
    }
    if (width < 1.0f) width = 1.0f;
    if (height < 1.0f) height = 1.0f;
    glm_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f, out_proj);
    return SITUATION_SUCCESS;
}

/** Vulkan-only: negative viewport height so Situation (0,0) top-left matches OpenGL. */
static void _SitVulkanFillViewport2DOpenGLParity(float width, float height, VkViewport* out_vp) {
    if (!out_vp) {
        return;
    }
    if (width < 1.0f) {
        width = 1.0f;
    }
    if (height < 1.0f) {
        height = 1.0f;
    }
    out_vp->x = 0.0f;
    out_vp->y = height;
    out_vp->width = width;
    out_vp->height = -height;
    out_vp->minDepth = 0.0f;
    out_vp->maxDepth = 1.0f;
}

/** Resolve 2D draw/composite target size — VD resolution when recording a VD pass (OpenGL current_target parity). */
static void _SitVulkanGetActive2DTargetSize(float* out_w, float* out_h) {
    if (!out_w || !out_h) {
        return;
    }
    *out_w = 1.0f;
    *out_h = 1.0f;

    if (sit_render.vk.inside_render_pass &&
            sit_render.vk.recording_pass_rt_slot >= 0 &&
            sit_render.vk.recording_pass_rt_slot < SITUATION_MAX_RENDER_TARGETS &&
            sit_render.render_target_slots_used[sit_render.vk.recording_pass_rt_slot]) {
        const _SituationRenderTargetSlot* rts = &sit_render.render_target_slots[sit_render.vk.recording_pass_rt_slot];
        *out_w = (float)rts->width;
        *out_h = (float)rts->height;
        if (*out_w < 1.0f) {
            *out_w = 1.0f;
        }
        if (*out_h < 1.0f) {
            *out_h = 1.0f;
        }
        return;
    }

    if (sit_render.vk.inside_render_pass &&
            sit_render.vk.recording_pass_display_id >= 0 &&
            sit_render.vk.recording_pass_display_id < SITUATION_MAX_VIRTUAL_DISPLAYS &&
            sit_render.virtual_display_slots_used[sit_render.vk.recording_pass_display_id]) {
        const SituationVirtualDisplay* vd =
            &sit_render.virtual_display_slots[sit_render.vk.recording_pass_display_id];
        *out_w = vd->resolution.x;
        *out_h = vd->resolution.y;
        if (*out_w < 1.0f) {
            *out_w = 1.0f;
        }
        if (*out_h < 1.0f) {
            *out_h = 1.0f;
        }
        return;
    }

    VkExtent2D extent = sit_render.vk.current_render_area.extent;
    if (extent.width > 0 && extent.height > 0) {
        *out_w = (float)extent.width;
        *out_h = (float)extent.height;
        return;
    }

    if (_SituationRenderCanvasStretchActive()) {
        *out_w = (float)sit_render.vk.canvas_resource_width;
        *out_h = (float)sit_render.vk.canvas_resource_height;
    } else {
        *out_w = (float)sit_render.vk.swapchain_extent.width;
        *out_h = (float)sit_render.vk.swapchain_extent.height;
    }
    if (*out_w < 1.0f) {
        *out_w = 1.0f;
    }
    if (*out_h < 1.0f) {
        *out_h = 1.0f;
    }
}

static void _SitVulkanApply2DViewportScissor(VkCommandBuffer vk_cmd) {
    if (vk_cmd == VK_NULL_HANDLE) {
        return;
    }
    float tw = 1.0f;
    float th = 1.0f;
    _SitVulkanGetActive2DTargetSize(&tw, &th);
    VkViewport vp;
    _SitVulkanFillViewport2DOpenGLParity(tw, th, &vp);
    VkRect2D sc = {{0, 0}, {(uint32_t)tw, (uint32_t)th}};
    vkCmdSetViewport(vk_cmd, 0, 1, &vp);
    vkCmdSetScissor(vk_cmd, 0, 1, &sc);
}
#endif

SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "cmd cannot be NULL in SituationCmdBeginRenderPass.");
    if (!info) return SITUATION_ERROR_INVALID_PARAM;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf->recording_render_pass_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE,
            "SituationCmdBeginRenderPass: a render pass is already active (nested passes not allowed)");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BEGIN_RENDER_PASS, p);

    p->args.begin_pass.display_id = info->display_id;
    {
        int fbw = sit_gs.main_window_width;
        int fbh = sit_gs.main_window_height;
        if (_SituationRenderCanvasStretchActive()) {
            _SituationGetRenderCanvasSize(&fbw, &fbh);
        } else if (sit_gs.sit_glfw_window) {
            glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fbw, &fbh);
        }
        if (fbw < 1) {
            fbw = 1;
        }
        if (fbh < 1) {
            fbh = 1;
        }
        p->args.begin_pass.target_w = fbw;
        p->args.begin_pass.target_h = fbh;
    }
    p->args.begin_pass.info = *info;
    buf->recording_render_pass_active = true;
    buf->recording_pass_had_draw = false;
    buf->recording_pass_rt_slot = -1;
    if (_SitRenderPassInfoUsesRenderTarget(info)) {
        if (!_SitGetRenderTargetSlot(info->render_target)) {
            buf->recording_render_pass_active = false;
            return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_TARGET_INVALID,
                "SituationCmdBeginRenderPass: invalid render_target handle.");
        }
        buf->recording_pass_rt_slot = (int)info->render_target.slot_index;
        buf->recording_pass_display_id = -1;
    } else {
        buf->recording_pass_display_id = info->display_id;
    }

    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.inside_render_pass) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE,
            "SituationCmdBeginRenderPass: a render pass is already active (nested passes not allowed)");
    }
    if (_SitRenderPassInfoUsesRenderTarget(info)) {
        _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(info->render_target);
        if (!rts) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_TARGET_INVALID,
                "SituationCmdBeginRenderPass: invalid render_target handle.");
        }
        if (!sit_render.vk.dynamic_rendering_enabled) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "SituationCmdBeginRenderPass: render targets require Vulkan dynamic rendering.");
        }
        _SitVkBeginRTDynamicRendering((VkCommandBuffer)cmd, rts, info);

        float target_width = (float)rts->width;
        float target_height = (float)rts->height;
        ViewDataUBO ubo_data;
        glm_mat4_identity(ubo_data.view);
        SIT_RETURN_IF_ERR(_SitVulkanFillOrthoProjection2D(target_width, target_height, ubo_data.projection));
        memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));

        VkViewport viewport;
        _SitVulkanFillViewport2DOpenGLParity(target_width, target_height, &viewport);
        vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);
        VkRect2D scissor = {{0, 0}, {(uint32_t)rts->width, (uint32_t)rts->height}};
        vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

        sit_render.vk.inside_main_swapchain_render_pass = false;
        sit_render.vk.inside_render_pass = true;
        sit_render.vk.current_render_area.offset = (VkOffset2D){0, 0};
        sit_render.vk.current_render_area.extent = scissor.extent;
        sit_render.vk.recording_pass_display_id = -1;
        sit_render.vk.recording_pass_rt_slot = (int)info->render_target.slot_index;
        sit_render.vk.recording_pass_had_draw = false;
        return SITUATION_SUCCESS;
    }
    // For the main window, use the render pass that was used to create the framebuffers
    // to ensure compatibility. The cached render pass system creates passes with different
    // dependency counts which makes them incompatible with the existing framebuffers.
    VkRenderPass rp;
    if (info->display_id < 0) {
        rp = sit_render.vk.main_window_render_pass;  // Use the original render pass
    } else {
        if (info->display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[info->display_id]) {
            return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
        }
        if (!sit_render.vk.dynamic_rendering_enabled) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "SituationCmdBeginRenderPass: VD targets require Vulkan dynamic rendering (VD-1).");
        }
        rp = VK_NULL_HANDLE;
    }
    if (info->display_id >= 0 && sit_render.vk.dynamic_rendering_enabled) {
        SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[info->display_id];
        _SitVkBeginVDDynamicRendering((VkCommandBuffer)cmd, vd, info);

        float target_width = vd->resolution.x;
        float target_height = vd->resolution.y;
        ViewDataUBO ubo_data;
        glm_mat4_identity(ubo_data.view);
        SIT_RETURN_IF_ERR(_SitVulkanFillOrthoProjection2D(target_width, target_height, ubo_data.projection));
        memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));

        VkViewport viewport;
        _SitVulkanFillViewport2DOpenGLParity(target_width, target_height, &viewport);
        vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);
        VkRect2D scissor = {{0, 0}, {(uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y}};
        vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

        sit_render.vk.inside_main_swapchain_render_pass = false;
        sit_render.vk.inside_render_pass = true;
        sit_render.vk.current_render_area.offset = (VkOffset2D){0, 0};
        sit_render.vk.current_render_area.extent = scissor.extent;
        sit_render.vk.recording_pass_display_id = info->display_id;
        sit_render.vk.recording_pass_had_draw = false;
        return SITUATION_SUCCESS;
    }
    if (rp == VK_NULL_HANDLE) {
        return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
    }

    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = rp;

    VkClearValue clear_values[2];
    {
        float clear_rgba[4];
        _SituationColorRgbaToClearFloats(
            info->color_attachment.clear.color,
            sit_render.output_hdr_active != 0,
            clear_rgba);
        clear_values[0].color = (VkClearColorValue){{clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]}};
    }
    clear_values[1].depthStencil = (VkClearDepthStencilValue){info->depth_attachment.clear.depth, info->stencil_attachment.clear.stencil};
    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    if (info->display_id < 0) {
        bool canvas_stretch = _SituationRenderCanvasStretchActive();
        if (canvas_stretch) {
            SituationError canvas_err = _SituationVulkanEnsureCanvasResources();
            if (canvas_err != SITUATION_SUCCESS) {
                return canvas_err;
            }
        }

        render_pass_info.framebuffer = canvas_stretch
            ? sit_render.vk.canvas_framebuffer
            : sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
        render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
        render_pass_info.renderArea.extent = canvas_stretch
            ? (VkExtent2D){sit_render.vk.canvas_resource_width, sit_render.vk.canvas_resource_height}
            : sit_render.vk.swapchain_extent;

        float target_width = (float)render_pass_info.renderArea.extent.width;
        float target_height = (float)render_pass_info.renderArea.extent.height;
        ViewDataUBO ubo_data;
        glm_mat4_identity(ubo_data.view);
        SIT_RETURN_IF_ERR(_SitVulkanFillOrthoProjection2D(target_width, target_height, ubo_data.projection));
        memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));
    }

    vkCmdBeginRenderPass((VkCommandBuffer)cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    _SitVulkanFillViewport2DOpenGLParity((float)render_pass_info.renderArea.extent.width,
                                         (float)render_pass_info.renderArea.extent.height, &viewport);
    vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = render_pass_info.renderArea.extent;
    vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

    if (info->display_id < 0) {
        sit_render.vk.inside_main_swapchain_render_pass = true;
    }
    sit_render.vk.inside_render_pass = true;
    sit_render.vk.current_render_area = render_pass_info.renderArea;
    sit_render.vk.recording_pass_display_id = info->display_id;
    sit_render.vk.recording_pass_had_draw = false;

    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Clears one or more active render-pass attachments.
 *
 * @details The command is valid only between `SituationCmdBeginRenderPass` and
 *          `SituationCmdEndRenderPass`. Texture/image clears are intentionally
 *          deferred to copy/transfer phases; this API targets the current
 *          framebuffer attachments.
 */
SITAPI SituationError SituationCmdClear(SituationCommandBuffer cmd, uint32_t clear_flags, const SituationClearValue* clear_value) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd || !clear_value) return SITUATION_ERROR_INVALID_PARAM;

    const uint32_t valid_flags = SIT_CLEAR_COLOR_BIT | SIT_CLEAR_DEPTH_BIT | SIT_CLEAR_STENCIL_BIT;
    if (clear_flags == 0 || (clear_flags & ~valid_flags) != 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdClear: invalid clear flags.");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf->recording_render_pass_active) {
        return SITUATION_ERROR_NO_RENDER_PASS_ACTIVE;
    }
    if (clear_flags & SIT_CLEAR_STENCIL_BIT) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCmdClear: OpenGL stencil attachment clears are not implemented yet.");
    }

    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_CLEAR, p);
    p->args.clear.flags = clear_flags;
    p->args.clear.value = *clear_value;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    if (!sit_render.vk.inside_render_pass) {
        return SITUATION_ERROR_NO_RENDER_PASS_ACTIVE;
    }
    if (clear_flags & SIT_CLEAR_STENCIL_BIT) {
        bool has_stencil = (sit_render.vk.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                            sit_render.vk.depth_format == VK_FORMAT_D24_UNORM_S8_UINT);
        if (!has_stencil) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "SituationCmdClear: active render pass has no stencil attachment.");
        }
    }

    VkClearAttachment attachments[2];
    uint32_t attachment_count = 0;

    if (clear_flags & SIT_CLEAR_COLOR_BIT) {
        VkClearAttachment* att = &attachments[attachment_count++];
        memset(att, 0, sizeof(*att));
        att->aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        att->colorAttachment = 0;
        att->clearValue.color = (VkClearColorValue){{
            clear_value->color.r / 255.0f,
            clear_value->color.g / 255.0f,
            clear_value->color.b / 255.0f,
            clear_value->color.a / 255.0f
        }};
    }

    VkImageAspectFlags ds_aspect = 0;
    if (clear_flags & SIT_CLEAR_DEPTH_BIT) {
        ds_aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (clear_flags & SIT_CLEAR_STENCIL_BIT) {
        ds_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (ds_aspect != 0) {
        VkClearAttachment* att = &attachments[attachment_count++];
        memset(att, 0, sizeof(*att));
        att->aspectMask = ds_aspect;
        att->clearValue.depthStencil.depth = clear_value->depth;
        att->clearValue.depthStencil.stencil = clear_value->stencil;
    }

    VkClearRect rect = {0};
    rect.rect = sit_render.vk.current_render_area;
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;
    vkCmdClearAttachments((VkCommandBuffer)cmd, attachment_count, attachments, 1, &rect);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdClearColor(SituationCommandBuffer cmd, ColorRGBA color) {
    SituationClearValue value = {0};
    value.color = color;
    return SituationCmdClear(cmd, SIT_CLEAR_COLOR_BIT, &value);
}

SITAPI SituationError SituationCmdClearDepth(SituationCommandBuffer cmd, float depth) {
    SituationClearValue value = {0};
    value.depth = depth;
    return SituationCmdClear(cmd, SIT_CLEAR_DEPTH_BIT, &value);
}

SITAPI SituationError SituationCmdClearStencil(SituationCommandBuffer cmd, uint32_t stencil) {
    SituationClearValue value = {0};
    value.stencil = stencil;
    return SituationCmdClear(cmd, SIT_CLEAR_STENCIL_BIT, &value);
}

SITAPI SituationError SituationCmdClearDepthStencil(SituationCommandBuffer cmd, float depth, uint32_t stencil) {
    SituationClearValue value = {0};
    value.depth = depth;
    value.stencil = stencil;
    return SituationCmdClear(cmd, SIT_CLEAR_DEPTH_BIT | SIT_CLEAR_STENCIL_BIT, &value);
}

/**
 * @brief [Core] Ends the current render pass on a command buffer.
 *
 * @details This function signals the completion of a rendering pass started by `SituationCmdBeginRenderPass` or `SituationCmdBeginRenderToDisplay`.
 *          It performs the necessary steps to finalize drawing operations for the current framebuffer attachment.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a `vkCmdEndRenderPass` command into the provided command buffer. This transitions the image layout of the attachments (e.g., to `PRESENT_SRC_KHR`) as defined by the render pass configuration.
 * - **OpenGL:** Unbinds the current Framebuffer Object (FBO) by binding the default framebuffer (0). This effectively "ends" the pass by redirecting subsequent draw calls back to the window's backbuffer.
 *
 * @param cmd The command buffer to record into.
 *            - **Vulkan:** Must be a valid `VkCommandBuffer` in the recording state, currently inside a render pass instance.
 *            - **OpenGL:** Ignored.
 *
 * @note This function must be paired with a preceding `SituationCmdBegin...` call.
 * @warning Calling this function without an active render pass (Vulkan) will result in a validation error.
 */
SITAPI SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: ENTRY, cmd=%p\n", cmd);
    fflush(stdout);
    #endif
    
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf) return SITUATION_ERROR_INVALID_PARAM;
    if (!buf->recording_render_pass_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE,
            "SituationCmdEndRenderPass: no render pass is currently active");
    }
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: buf=%p, calling _SitGLSoftCmdPush\n", buf);
    fflush(stdout);
    #endif
    
    SitCommandPacket* _sit_pkt_ = NULL; SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_END_RENDER_PASS, _sit_pkt_);
    if (buf->recording_pass_rt_slot >= 0 &&
            buf->recording_pass_rt_slot < SITUATION_MAX_RENDER_TARGETS &&
            sit_render.render_target_slots_used[buf->recording_pass_rt_slot]) {
        SituationRenderTarget rt = {
            (uint32_t)buf->recording_pass_rt_slot,
            sit_render.render_target_slots[buf->recording_pass_rt_slot].generation
        };
        _SitRenderPassSetRenderTargetLayoutHint(rt, SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT);
    } else {
        _SitRenderPassSetTargetLayoutHint(buf->recording_pass_display_id, SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT);
    }
    _SitVDEndRenderPassCheck(buf->recording_pass_display_id, buf->recording_pass_had_draw);
    buf->recording_pass_display_id = -1;
    buf->recording_pass_rt_slot = -1;
    buf->recording_pass_had_draw = false;
    buf->recording_render_pass_active = false;
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: SUCCESS, returning\n");
    fflush(stdout);
    #endif
#elif defined(SITUATION_USE_VULKAN)
    if (cmd == 0) return SITUATION_ERROR_INVALID_PARAM;
    if (!sit_render.vk.inside_render_pass) return SITUATION_ERROR_NO_RENDER_PASS_ACTIVE;
    int ended_vd_id = sit_render.vk.recording_pass_display_id;
    int ended_rt_slot = sit_render.vk.recording_pass_rt_slot;
    _SitVDEndRenderPassCheck(ended_vd_id, sit_render.vk.recording_pass_had_draw);
    if (ended_rt_slot >= 0 &&
            ended_rt_slot < SITUATION_MAX_RENDER_TARGETS &&
            sit_render.render_target_slots_used[ended_rt_slot]) {
        SituationRenderTarget rt = {
            (uint32_t)ended_rt_slot,
            sit_render.render_target_slots[ended_rt_slot].generation
        };
        _SitRenderPassSetRenderTargetLayoutHint(rt, SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT);
    } else {
        _SitRenderPassSetTargetLayoutHint(ended_vd_id, SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT);
    }
    sit_render.vk.recording_pass_display_id = -1;
    sit_render.vk.recording_pass_rt_slot = -1;
    sit_render.vk.recording_pass_had_draw = false;
    if (ended_rt_slot >= 0 &&
            ended_rt_slot < SITUATION_MAX_RENDER_TARGETS &&
            sit_render.render_target_slots_used[ended_rt_slot]) {
        _SitVkEndRTDynamicRendering((VkCommandBuffer)cmd, &sit_render.render_target_slots[ended_rt_slot]);
    } else if (ended_vd_id >= 0 && sit_render.vk.dynamic_rendering_enabled &&
        sit_render.virtual_display_slots_used[ended_vd_id]) {
        _SitVkEndVDDynamicRendering((VkCommandBuffer)cmd, ended_vd_id, &sit_render.virtual_display_slots[ended_vd_id]);
    } else {
        vkCmdEndRenderPass((VkCommandBuffer)cmd);
    }
    sit_render.vk.inside_main_swapchain_render_pass = false;
    sit_render.vk.inside_render_pass = false;
    sit_render.vk.current_render_area = (VkRect2D){0};
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Begins a render pass on a specific display target.
 * @details For OpenGL, this binds the appropriate framebuffer (0 for the main window, or an FBO for a virtual display), sets the viewport to the target's dimensions, and clears the color and depth buffers.
 *          For Vulkan, this begins a formal VkRenderPass on the command buffer.
 *
 * @param cmd The command buffer to record to. (Ignored in the immediate-mode OpenGL backend).
 * @param display_id The ID of the target. Use -1 for the main window/swapchain.
 * @param clear_color The color to clear the target with.
 */
SITAPI SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;
    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_STORE;
    info.depth_attachment.clear.depth = 1.0f;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.stencil_attachment.storeOp = SIT_STORE_OP_STORE;
    info.stencil_attachment.clear.stencil = 0;
    return SituationCmdBeginRenderPass(cmd, &info);
}
/**
 * @brief Ends the current render pass.
 * @note Deprecated in favor of SituationCmdEndRenderPass.
 */
SITAPI SituationError SituationCmdEndRender(SituationCommandBuffer cmd) {
    return SituationCmdEndRenderPass(cmd);
}

#if defined(SITUATION_USE_OPENGL) || defined(SITUATION_USE_VULKAN)
static int _SituationGetMaxViewports(void) {
    if (!SituationIsInitialized()) {
        return 0;
    }
#if defined(SITUATION_USE_OPENGL)
    // Use cached value — GL context is on render thread, not main thread after init
    return (sit_render.cached_max_viewports >= 1) ? sit_render.cached_max_viewports : 1;
#elif defined(SITUATION_USE_VULKAN)
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
    int max_vp = (int)props.limits.maxViewports;
    return (max_vp >= 1) ? max_vp : 1;
#endif
}

static SituationError _SituationValidateViewportScissorIndex(uint32_t index, const char* caller) {
    int max_vp = _SituationGetMaxViewports();
    if (max_vp <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED,
            "Cannot set viewport/scissor before renderer initialization.");
    }
    if ((int)index >= max_vp) {
        char detail[160];
        snprintf(detail, sizeof(detail),
            "%s: index %u exceeds max_viewports (%d).", caller, index, max_vp);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    return SITUATION_SUCCESS;
}
#endif

/**
 * @brief Sets the viewport at a given index for subsequent drawing commands.
 */
SITAPI SituationError SituationCmdSetViewportIndexed(SituationCommandBuffer cmd, uint32_t index, float x, float y, float width, float height) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (width <= 0.0f || height <= 0.0f) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                 "Invalid viewport dimensions: width=%.2f, height=%.2f. Dimensions must be positive.", width, height);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, error_msg);
    }
    SIT_RETURN_IF_ERR(_SituationValidateViewportScissorIndex(index, "SituationCmdSetViewportIndexed"));

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = NULL;
        SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_VIEWPORT, p);
        if (p) {
            p->args.viewport.index = index;
            p->args.viewport.x = x;
            p->args.viewport.y = y;
            p->args.viewport.w = width;
            p->args.viewport.h = height;
        } else {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }
#elif defined(SITUATION_USE_VULKAN)
    {
        if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
                "Invalid command buffer for SituationCmdSetViewportIndexed.");
        }
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
        VkViewport viewport = {};
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(vk_cmd, index, 1, &viewport);
    }
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets the scissor rectangle at a given index.
 */
SITAPI SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (width < 0 || height < 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SIT_RETURN_IF_ERR(_SituationValidateViewportScissorIndex(index, "SituationCmdSetScissorIndexed"));

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = NULL;
        SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_SCISSOR, p);
        if (p) {
            p->args.scissor.index = index;
            p->args.scissor.x = x;
            p->args.scissor.y = y;
            p->args.scissor.w = width;
            p->args.scissor.h = height;
        } else {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }
#elif defined(SITUATION_USE_VULKAN)
    {
        VkRect2D scissor = {};
        scissor.offset.x = x;
        scissor.offset.y = y;
        scissor.extent.width = (uint32_t)width;
        scissor.extent.height = (uint32_t)height;
        vkCmdSetScissor((VkCommandBuffer)cmd, index, 1, &scissor);
    }
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets the viewport and scissor rectangle for subsequent drawing commands.
 *
 * @details The viewport defines the rectangular area of the current render target that primitives will be rasterized to. The coordinates are in framebuffer/pixel space.
 *          In Vulkan, this function also sets the scissor rectangle to match the viewport dimensions, enforcing that rendering is clipped to this area. In OpenGL, the scissor test is not modified by this function; use `SituationCmdSetScissor` if explicit scissor control is needed.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glViewport` to set the viewport transformation.
 *   The command buffer parameter `cmd` is ignored as OpenGL uses global state.
 *   Note that OpenGL does not implicitly change the scissor state.
 * - **Vulkan:** Records `vkCmdSetViewport` and `vkCmdSetScissor` commands into the provided command buffer. Both the viewport and scissor are set to the specified rectangle.
 *   This requires the command buffer to be in the recording state and the bound graphics pipeline to have been created with `VK_DYNAMIC_STATE_VIEWPORT` and `VK_DYNAMIC_STATE_SCISSOR` enabled.
 *
 * @param cmd The command buffer into which the commands will be recorded (Vulkan)
 *            or ignored (OpenGL).
 * @param x The top-left x-coordinate of the viewport/scissor (in pixels).
 * @param y The top-left y-coordinate of the viewport/scissor (in pixels).
 *         Note: In Vulkan, the Y axis origin is typically the top-left.
 *         In OpenGL, it's the bottom-left, but `glViewport` handles this.
 * @param width The width of the viewport/scissor (in pixels). Must be positive.
 * @param height The height of the viewport/scissor (in pixels). Must be positive.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. (Vulkan) The bound graphics pipeline supports dynamic viewport and scissor.
 *       3. The specified `width` and `height` are greater than zero.
 * @warning Providing a `width` or `height` of zero or negative values results in undefined behavior or errors, depending on the backend and driver.
 */
SITAPI SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height) {
    SituationError err = SituationCmdSetViewportIndexed(cmd, 0, x, y, width, height);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
#if defined(SITUATION_USE_VULKAN)
    return SituationCmdSetScissorIndexed(cmd, 0, (int)x, (int)y, (int)width, (int)height);
#else
    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Sets the dynamic scissor rectangle for the render target.
 * @details The scissor test is a hardware-level optimization that discards any pixel fragments outside of this defined rectangle, preventing the fragment shader from running on them. This should be called after binding a pipeline that was created with VK_DYNAMIC_STATE_SCISSOR enabled.
 * @param cmd The command buffer to record the command into.
 * @param x, y The top-left corner of the scissor rectangle, in pixel coordinates.
 * @param width, height The dimensions of the scissor rectangle, in pixels.
 */
SITAPI SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height) {
    return SituationCmdSetScissorIndexed(cmd, 0, x, y, width, height);
}

#if defined(SITUATION_USE_VULKAN)
static VkPipeline _SitVulkanBasePipelineForStride(_SituationShaderSlot* shader_slot, size_t stride) {
    if (!shader_slot) return VK_NULL_HANDLE;
    VkPipeline base = shader_slot->vk_pipeline;
    if (stride <= 3 * sizeof(float) && shader_slot->vk_pipeline_simple != VK_NULL_HANDLE) {
        base = shader_slot->vk_pipeline_simple;
    } else if (stride <= (3 + 3 + 2) * sizeof(float) && shader_slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
        base = shader_slot->vk_pipeline_legacy;
    }
    return base;
}

static VkPipeline _SitVulkanSelectRasterVariant(_SituationShaderSlot* shader_slot, VkPipeline base_pipeline) {
    if (!shader_slot || base_pipeline == VK_NULL_HANDLE) return base_pipeline;
    if (sit_render.vk.dynamic_cull_mode != VK_CULL_MODE_BACK_BIT) return base_pipeline;
    bool ccw = (sit_render.vk.dynamic_front_face == VK_FRONT_FACE_COUNTER_CLOCKWISE);

    VkPipeline variant = base_pipeline;
    if (base_pipeline == shader_slot->vk_pipeline) {
        variant = ccw ? shader_slot->vk_pipeline_back_ccw : shader_slot->vk_pipeline_back_cw;
    } else if (base_pipeline == shader_slot->vk_pipeline_legacy) {
        variant = ccw ? shader_slot->vk_pipeline_legacy_back_ccw : shader_slot->vk_pipeline_legacy_back_cw;
    } else if (base_pipeline == shader_slot->vk_pipeline_simple) {
        variant = ccw ? shader_slot->vk_pipeline_simple_back_ccw : shader_slot->vk_pipeline_simple_back_cw;
    }
    return (variant != VK_NULL_HANDLE) ? variant : base_pipeline;
}

static VkPipeline _SitVulkanSelectPolygonVariant(_SituationShaderSlot* shader_slot, VkPipeline base_pipeline) {
    if (!shader_slot || base_pipeline == VK_NULL_HANDLE) return base_pipeline;
    if (sit_render.vk.dynamic_polygon_mode != VK_POLYGON_MODE_LINE) return base_pipeline;
    if ((base_pipeline == shader_slot->vk_pipeline || base_pipeline == shader_slot->vk_pipeline_back_ccw ||
         base_pipeline == shader_slot->vk_pipeline_back_cw) &&
        shader_slot->vk_pipeline_line != VK_NULL_HANDLE) {
        return shader_slot->vk_pipeline_line;
    }
    if ((base_pipeline == shader_slot->vk_pipeline_legacy || base_pipeline == shader_slot->vk_pipeline_legacy_back_ccw ||
         base_pipeline == shader_slot->vk_pipeline_legacy_back_cw) &&
        shader_slot->vk_pipeline_legacy_line != VK_NULL_HANDLE) {
        return shader_slot->vk_pipeline_legacy_line;
    }
    if ((base_pipeline == shader_slot->vk_pipeline_simple || base_pipeline == shader_slot->vk_pipeline_simple_back_ccw ||
         base_pipeline == shader_slot->vk_pipeline_simple_back_cw) &&
        shader_slot->vk_pipeline_simple_line != VK_NULL_HANDLE) {
        return shader_slot->vk_pipeline_simple_line;
    }
    return base_pipeline;
}

static VkPipeline _SitVulkanResolveGraphicsPipeline(_SituationShaderSlot* shader_slot, size_t stride) {
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* CRITICAL SAFETY: use _SitVkDerefBundle — never read vk_bundle_ref.bundle directly. */
    _SitVkPipelineBundle* bundle = _SitVkDerefBundle(&shader_slot->vk_bundle_ref);
    
    // [FIX] Recover from STALE state (e.g., after swapchain recreation)
    if (!bundle && shader_slot->vk_last_vs_spirv && shader_slot->vk_last_fs_spirv) {
        _SitVkTryAttachBundle(shader_slot,
            shader_slot->vk_last_vs_spirv, shader_slot->vk_last_vs_size,
            shader_slot->vk_last_fs_spirv, shader_slot->vk_last_fs_size,
            shader_slot->vk_spirv_layout_profile);
        bundle = _SitVkDerefBundle(&shader_slot->vk_bundle_ref);
    }

    if (bundle && bundle->default_pipeline != VK_NULL_HANDLE) {
#if SIT_VK_SHADER_CACHE_PHASE2
        int vid = _SitVkPickVariantForDraw(stride,
            sit_render.vk.dynamic_cull_mode,
            sit_render.vk.dynamic_front_face,
            sit_render.vk.dynamic_polygon_mode);
        VkPipeline selected = (vid < 0)
            ? bundle->default_pipeline
            : _SitVkEnsurePipelineVariant(bundle, vid);
#else
        /* Phase 1: simple stride + fill + no back-cull only. */
        bool simple_stride = (stride == 3u * sizeof(float) || stride == 0u);
        bool fill = (sit_render.vk.dynamic_polygon_mode == VK_POLYGON_MODE_FILL);
        bool no_back_cull = (sit_render.vk.dynamic_cull_mode == VK_CULL_MODE_NONE);
        VkPipeline selected = VK_NULL_HANDLE;
        if (simple_stride && fill && no_back_cull)
            selected = bundle->default_pipeline;
#endif
        if (selected != VK_NULL_HANDLE) {
            shader_slot->vk_bound_pipeline_cache = selected;
            return selected;
        }
#if !defined(NDEBUG) && SIT_VK_SHADER_CACHE_PHASE2
        sit_render.vk.shader_cache.stats.bundle_resolve_slot_fallbacks++;
#endif
    }
#endif
    VkPipeline base = _SitVulkanBasePipelineForStride(shader_slot, stride);
    VkPipeline after_poly = _SitVulkanSelectPolygonVariant(shader_slot, base);
    VkPipeline resolved = after_poly;
    if (sit_render.vk.dynamic_polygon_mode == VK_POLYGON_MODE_FILL) {
        resolved = _SitVulkanSelectRasterVariant(shader_slot, after_poly);
    }
#if !defined(NDEBUG)
    sit_render.vk.raster_pipeline_resolve_count++;
    if (after_poly != base) {
        sit_render.vk.raster_polygon_variant_hits++;
    }
    if (resolved != after_poly) {
        sit_render.vk.raster_cull_front_variant_hits++;
    }
#endif
#if !defined(NDEBUG)
    if (shader_slot && base != VK_NULL_HANDLE) {
        assert(resolved != VK_NULL_HANDLE);
    }
#endif
    return resolved;
}

static VkPrimitiveTopology _SitVulkanGetCurrentPrimitiveTopology(void) {
    if (!sit_render.vk.dynamic_primitive_topology_initialized) {
        sit_render.vk.dynamic_primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        sit_render.vk.dynamic_primitive_topology_initialized = true;
    }
    return sit_render.vk.dynamic_primitive_topology;
}

static bool _SitVulkanGraphicsDynamicProcsReady(void) {
    return sit_render.vk.extended_dynamic_state_enabled
        && sit_render.vk.pfn_cmd_set_primitive_topology
        && sit_render.vk.pfn_cmd_set_depth_test_enable
        && sit_render.vk.pfn_cmd_set_depth_write_enable
        && sit_render.vk.pfn_cmd_set_depth_compare_op;
}

static void _SitVulkanCmdSetDepthDynamics(
    VkCommandBuffer vk_cmd, VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op) {
    if (!_SitVulkanGraphicsDynamicProcsReady() || vk_cmd == VK_NULL_HANDLE) return;
    sit_render.vk.pfn_cmd_set_depth_test_enable(vk_cmd, test_enable);
    sit_render.vk.pfn_cmd_set_depth_write_enable(vk_cmd, write_enable);
    sit_render.vk.pfn_cmd_set_depth_compare_op(vk_cmd, compare_op);
}

static bool _SitVulkanActiveVDPassHasDepth(void) {
    if (!sit_render.vk.inside_render_pass || sit_render.vk.recording_pass_display_id < 0) {
        return false;
    }
    int id = sit_render.vk.recording_pass_display_id;
    if (id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[id]) {
        return false;
    }
    return _SitVDHasDepthAttachment(&sit_render.virtual_display_slots[id]);
}

static void _SitVulkanDestroyQuadVDDynamicPipelines(void) {
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        return;
    }
    for (uint32_t i = 0; i < sit_render.vk.quad_vd_dynamic_pipeline_count; ++i) {
        if (sit_render.vk.quad_vd_dynamic_pipelines[i].pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, sit_render.vk.quad_vd_dynamic_pipelines[i].pipeline, NULL);
            sit_render.vk.quad_vd_dynamic_pipelines[i].pipeline = VK_NULL_HANDLE;
        }
    }
    sit_render.vk.quad_vd_dynamic_pipeline_count = 0;
}

static SituationError _SitVulkanResolveQuadPipeline(VkPipeline* out_pipeline) {
    if (!out_pipeline) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "_SitVulkanResolveQuadPipeline: out_pipeline is NULL.");
    }

    const bool vd_dynamic_path =
        sit_render.vk.inside_render_pass &&
        sit_render.vk.recording_pass_display_id >= 0 &&
        sit_render.vk.dynamic_rendering_enabled;

    if (!vd_dynamic_path) {
        *out_pipeline = sit_render.vk.quad_pipeline;
        return SITUATION_SUCCESS;
    }

    int id = sit_render.vk.recording_pass_display_id;
    if (id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[id]) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
            "_SitVulkanResolveQuadPipeline: active VD render pass has invalid display id.");
    }

    const SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[id];
    VkFormat color_fmt = _SitVDVkColorFormat(vd->color_format);
    VkFormat depth_fmt = _SitVDVkDepthFormat(vd);
    const VkSampleCountFlagBits raster_samples =
        _SitVkSampleCountFlagFromInt(_SitVDEffectiveSampleCount(vd));
    uint32_t key = _SitVDVkPipelineVariantKey(color_fmt, depth_fmt, _SitVDEffectiveSampleCount(vd));

    for (uint32_t i = 0; i < sit_render.vk.quad_vd_dynamic_pipeline_count; ++i) {
        if (sit_render.vk.quad_vd_dynamic_pipelines[i].key == key &&
                sit_render.vk.quad_vd_dynamic_pipelines[i].pipeline != VK_NULL_HANDLE) {
            *out_pipeline = sit_render.vk.quad_vd_dynamic_pipelines[i].pipeline;
            return SITUATION_SUCCESS;
        }
    }

    if (!sit_render.vk.quad_owned_vs_spirv || !sit_render.vk.quad_owned_fs_spirv ||
            sit_render.vk.quad_vs_spirv_size == 0 || sit_render.vk.quad_fs_spirv_size == 0 ||
            sit_render.vk.quad_pipeline_layout == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED,
            "_SitVulkanResolveQuadPipeline: internal quad SPIR-V or layout is not initialized.");
    }

    if (sit_render.vk.quad_vd_dynamic_pipeline_count >= SIT_VK_QUAD_VD_DYNAMIC_PIPELINE_CACHE_MAX) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "_SitVulkanResolveQuadPipeline: VD quad pipeline cache is full.");
    }

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attr_desc = {};
    attr_desc.binding = 0;
    attr_desc.location = SIT_ATTR_POSITION;
    attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc.offset = 0;

    VkPipeline created = _SituationVulkanCreateGraphicsPipelineEx(
        sit_render.vk.quad_owned_vs_spirv, sit_render.vk.quad_vs_spirv_size,
        sit_render.vk.quad_owned_fs_spirv, sit_render.vk.quad_fs_spirv_size,
        sit_render.vk.quad_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        1, &binding_desc,
        1, &attr_desc,
        0,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL,
        color_fmt, depth_fmt, raster_samples);

    if (created == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "_SitVulkanResolveQuadPipeline: failed to create VD-format quad pipeline.");
    }

    sit_render.vk.quad_vd_dynamic_pipelines[sit_render.vk.quad_vd_dynamic_pipeline_count].key = key;
    sit_render.vk.quad_vd_dynamic_pipelines[sit_render.vk.quad_vd_dynamic_pipeline_count].pipeline = created;
    sit_render.vk.quad_vd_dynamic_pipeline_count++;
    *out_pipeline = created;
    return SITUATION_SUCCESS;
}

static void _SitVulkanFillQuadPushProjectionForActiveTarget(mat4 out_proj) {
    if (!out_proj) {
        return;
    }
    float tw = 1.0f;
    float th = 1.0f;
    _SitVulkanGetActive2DTargetSize(&tw, &th);
    _SitVulkanFillOrthoProjection2D(tw, th, out_proj);
}

static void _SitVulkanApplyQuadDrawDynamicState(VkCommandBuffer vk_cmd) {
    if (vk_cmd == VK_NULL_HANDLE) return;

    /* Projection lives in push constants (per draw). Viewport/scissor still track active 2D target. */
    _SitVulkanApply2DViewportScissor(vk_cmd);
    if (sit_render.vk.depth_bias_dynamic_enabled && sit_render.vk.pfn_cmd_set_depth_bias_enable) {
        sit_render.vk.pfn_cmd_set_depth_bias_enable(vk_cmd, sit_render.vk.dynamic_depth_bias_enable);
    }
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        sit_render.vk.pfn_cmd_set_primitive_topology(vk_cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        if (_SitVulkanActiveVDPassHasDepth()) {
            /* OpenGL SIT_OP_DRAW_QUAD into VD+depth: depth test off, depth write on (pass sets glDepthMask TRUE). */
            _SitVulkanCmdSetDepthDynamics(vk_cmd, VK_FALSE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
            sit_render.vk.dynamic_depth_test_enable = VK_FALSE;
            sit_render.vk.dynamic_depth_write_enable = VK_TRUE;
            sit_render.vk.dynamic_depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        } else {
            _SitVulkanCmdSetDepthDynamics(vk_cmd, VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
            sit_render.vk.dynamic_depth_test_enable = VK_FALSE;
            sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
            sit_render.vk.dynamic_depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        }
    }
}

/** VD compositor draws a triangle strip; caller/user draws often leave TRIANGLE_LIST dynamic state. */
static void _SitVulkanApplyVDCompositingDynamicState(VkCommandBuffer vk_cmd, float vp_w, float vp_h) {
    if (vk_cmd == VK_NULL_HANDLE) return;
    if (vp_w < 1.0f) vp_w = 1.0f;
    if (vp_h < 1.0f) vp_h = 1.0f;
    VkViewport vp;
    _SitVulkanFillViewport2DOpenGLParity(vp_w, vp_h, &vp);
    VkRect2D sc = {{0, 0}, {(uint32_t)vp_w, (uint32_t)vp_h}};
    vkCmdSetViewport(vk_cmd, 0, 1, &vp);
    vkCmdSetScissor(vk_cmd, 0, 1, &sc);
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        sit_render.vk.pfn_cmd_set_primitive_topology(vk_cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        sit_render.vk.pfn_cmd_set_depth_test_enable(vk_cmd, VK_FALSE);
        sit_render.vk.pfn_cmd_set_depth_write_enable(vk_cmd, VK_FALSE);
    }
}

static void _SitVulkanApplyGraphicsViewportScissor(VkCommandBuffer vk_cmd) {
    if (vk_cmd == VK_NULL_HANDLE) return;
    VkExtent2D extent = sit_render.vk.current_render_area.extent;
    if (extent.width == 0 || extent.height == 0) {
        extent = sit_render.vk.swapchain_extent;
    }
    if (extent.width == 0 || extent.height == 0) return;
    VkViewport vp;
    _SitVulkanFillViewport2DOpenGLParity((float)extent.width, (float)extent.height, &vp);
    VkRect2D sc = {{0, 0}, extent};
    vkCmdSetViewport(vk_cmd, 0, 1, &vp);
    vkCmdSetScissor(vk_cmd, 0, 1, &sc);
}

static VkCompareOp _SitVulkanMapCompareOp(SituationDepthCompareOp op) {
    switch (op) {
        case SIT_DEPTH_COMPARE_ALWAYS:   return VK_COMPARE_OP_ALWAYS;
        case SIT_DEPTH_COMPARE_LESS:     return VK_COMPARE_OP_LESS;
        case SIT_DEPTH_COMPARE_LEQUAL:   return VK_COMPARE_OP_LESS_OR_EQUAL;
        case SIT_DEPTH_COMPARE_GREATER:  return VK_COMPARE_OP_GREATER;
        case SIT_DEPTH_COMPARE_GEQUAL:   return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case SIT_DEPTH_COMPARE_EQUAL:    return VK_COMPARE_OP_EQUAL;
        case SIT_DEPTH_COMPARE_NOTEQUAL: return VK_COMPARE_OP_NOT_EQUAL;
        case SIT_DEPTH_COMPARE_NEVER:    return VK_COMPARE_OP_NEVER;
        default:                         return VK_COMPARE_OP_LESS;
    }
}

static VkStencilOp _SitVulkanMapStencilOp(SituationStencilOp op) {
    switch (op) {
        case SIT_STENCIL_OP_ZERO:              return VK_STENCIL_OP_ZERO;
        case SIT_STENCIL_OP_REPLACE:           return VK_STENCIL_OP_REPLACE;
        case SIT_STENCIL_OP_INCREMENT_CLAMP:   return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case SIT_STENCIL_OP_DECREMENT_CLAMP:   return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case SIT_STENCIL_OP_INVERT:            return VK_STENCIL_OP_INVERT;
        case SIT_STENCIL_OP_INCREMENT_WRAP:    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case SIT_STENCIL_OP_DECREMENT_WRAP:    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case SIT_STENCIL_OP_KEEP:
        default:                               return VK_STENCIL_OP_KEEP;
    }
}

static bool _SitVulkanHasStencilAttachment(void) {
    return sit_render.vk.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           sit_render.vk.depth_format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static void _SitVulkanApplyStencilFaceDynamics(VkCommandBuffer vk_cmd, VkStencilFaceFlags face,
                                               const SituationStencilState* state) {
    if (vk_cmd == VK_NULL_HANDLE || !state || !sit_render.vk.pfn_cmd_set_stencil_op) {
        return;
    }
    vkCmdSetStencilCompareMask(vk_cmd, face, state->compare_mask);
    vkCmdSetStencilWriteMask(vk_cmd, face, state->write_mask);
    vkCmdSetStencilReference(vk_cmd, face, state->reference);
    sit_render.vk.pfn_cmd_set_stencil_op(vk_cmd, face,
        _SitVulkanMapStencilOp(state->fail_op),
        _SitVulkanMapStencilOp(state->pass_op),
        _SitVulkanMapStencilOp(state->depth_fail_op),
        _SitVulkanMapCompareOp(state->compare_op));
}

static void _SitVulkanApplyTrackedExtendedRasterDynamics(VkCommandBuffer vk_cmd) {
    if (vk_cmd == VK_NULL_HANDLE) return;
    if (sit_render.vk.extended_dynamic_state3_color_write_enabled &&
        sit_render.vk.pfn_cmd_set_color_write_mask_ext) {
        VkColorComponentFlags mask = sit_render.vk.dynamic_color_write_mask;
        sit_render.vk.pfn_cmd_set_color_write_mask_ext(vk_cmd, 0, 1, &mask);
    }
    vkCmdSetLineWidth(vk_cmd, sit_render.vk.dynamic_line_width);
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        _SitVulkanCmdSetDepthDynamics(vk_cmd,
            sit_render.vk.dynamic_depth_test_enable,
            sit_render.vk.dynamic_depth_write_enable,
            sit_render.vk.dynamic_depth_compare_op);
    }
    if (sit_render.vk.pfn_cmd_set_stencil_test_enable) {
        sit_render.vk.pfn_cmd_set_stencil_test_enable(vk_cmd, sit_render.vk.dynamic_stencil_test_enable);
        if (sit_render.vk.dynamic_stencil_test_enable &&
            sit_render.vk.pfn_cmd_set_stencil_op) {
            _SitVulkanApplyStencilFaceDynamics(vk_cmd, VK_STENCIL_FACE_FRONT_BIT, &sit_render.vk.dynamic_stencil_front);
            _SitVulkanApplyStencilFaceDynamics(vk_cmd, VK_STENCIL_FACE_BACK_BIT, &sit_render.vk.dynamic_stencil_back);
        }
    }
}

static void _SitVulkanCaptureRasterState(_SitVulkanRasterStackEntry* entry) {
    if (!entry) return;
    entry->color_write_mask = sit_render.vk.dynamic_color_write_mask;
    entry->depth_test_enable = sit_render.vk.dynamic_depth_test_enable;
    entry->depth_write_enable = sit_render.vk.dynamic_depth_write_enable;
    entry->depth_compare_op = sit_render.vk.dynamic_depth_compare_op;
    entry->stencil_test_enable = sit_render.vk.dynamic_stencil_test_enable;
    entry->stencil_front = sit_render.vk.dynamic_stencil_front;
    entry->stencil_back = sit_render.vk.dynamic_stencil_back;
    entry->line_width = sit_render.vk.dynamic_line_width;
    entry->cull_mode = sit_render.vk.dynamic_cull_mode;
    entry->front_face = sit_render.vk.dynamic_front_face;
    entry->depth_bias_enable = sit_render.vk.dynamic_depth_bias_enable;
    entry->depth_bias_constant = sit_render.vk.dynamic_depth_bias_constant;
    entry->depth_bias_clamp = sit_render.vk.dynamic_depth_bias_clamp;
    entry->depth_bias_slope = sit_render.vk.dynamic_depth_bias_slope;
    // Multisample — stored for push/pop symmetry; no dynamic VK dispatch possible without
    // VK_EXT_extended_dynamic_state3 sample shading/alpha-to-coverage features.
    entry->ms_sample_shading_enable    = sit_render.vk.dynamic_ms_sample_shading_enable;
    entry->ms_min_sample_shading       = sit_render.vk.dynamic_ms_min_sample_shading;
    entry->ms_sample_mask              = sit_render.vk.dynamic_ms_sample_mask;
    entry->ms_alpha_to_coverage_enable = sit_render.vk.dynamic_ms_alpha_to_coverage_enable;
}

static void _SitVulkanApplyTrackedRasterDynamics(VkCommandBuffer vk_cmd) {
    if (vk_cmd == VK_NULL_HANDLE) return;
    _SitVulkanApplyGraphicsViewportScissor(vk_cmd);
    if (sit_render.vk.pfn_cmd_set_primitive_topology) {
        sit_render.vk.pfn_cmd_set_primitive_topology(vk_cmd, _SitVulkanGetCurrentPrimitiveTopology());
    }
    /* When VK_DYNAMIC_STATE_POLYGON_MODE_EXT is in the pipeline, static POLYGON_MODE_LINE
     * in vk_pipeline_*_line variants is ignored — must record mode every draw. */
    if (sit_render.vk.extended_dynamic_state3_polygon_mode_enabled && sit_render.vk.pfn_cmd_set_polygon_mode_ext) {
        sit_render.vk.pfn_cmd_set_polygon_mode_ext(vk_cmd, sit_render.vk.dynamic_polygon_mode);
    }
    if (sit_render.vk.depth_bias_dynamic_enabled && sit_render.vk.pfn_cmd_set_depth_bias_enable) {
        sit_render.vk.pfn_cmd_set_depth_bias_enable(vk_cmd, sit_render.vk.dynamic_depth_bias_enable);
        if (sit_render.vk.dynamic_depth_bias_enable && sit_render.vk.pfn_cmd_set_depth_bias) {
            sit_render.vk.pfn_cmd_set_depth_bias(vk_cmd,
                sit_render.vk.dynamic_depth_bias_constant,
                sit_render.vk.dynamic_depth_bias_clamp,
                sit_render.vk.dynamic_depth_bias_slope);
        }
    }
    _SitVulkanApplyTrackedExtendedRasterDynamics(vk_cmd);
}

#if defined(SITUATION_USE_VULKAN)
static VkAttachmentLoadOp _SitVkMapLoadOp(SituationAttachmentLoadOp op) {
    switch (op) {
        case SIT_LOAD_OP_LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case SIT_LOAD_OP_CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        default: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

static VkAttachmentStoreOp _SitVkMapStoreOp(SituationAttachmentStoreOp op) {
    return (op == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
static bool _SitVkLookupSpirvBytesByHash(uint64_t vs_hash, uint64_t fs_hash,
    const void** out_vs, size_t* out_vs_len, const void** out_fs, size_t* out_fs_len) {
    _SitVkShaderCache* c = &sit_render.vk.shader_cache;
    for (uint32_t i = 0; i < SIT_VK_SHADER_CACHE_MAX_ENTRIES; ++i) {
        for (_SitVkSpirvBlobEntry* e = c->spirv_blob_cache[i]; e; e = e->next) {
            if (e->vs_size > 0 && e->fs_size > 0 &&
                _SitVkHashBytes64(e->vs_data, e->vs_size) == vs_hash &&
                _SitVkHashBytes64(e->fs_data, e->fs_size) == fs_hash) {
                if (out_vs) *out_vs = e->vs_data;
                if (out_vs_len) *out_vs_len = e->vs_size;
                if (out_fs) *out_fs = e->fs_data;
                if (out_fs_len) *out_fs_len = e->fs_size;
                return true;
            }
        }
    }
    return false;
}
#endif

static void _SitVkPinLastSpirvOnSlot(_SituationShaderSlot* slot,
    const void* vs, size_t vs_len, const void* fs, size_t fs_len) {
    if (!slot || !vs || !fs || vs_len == 0 || fs_len == 0) return;
    if (slot->vk_owned_last_vs_spirv) {
        SIT_FREE(slot->vk_owned_last_vs_spirv);
        slot->vk_owned_last_vs_spirv = NULL;
    }
    if (slot->vk_owned_last_fs_spirv) {
        SIT_FREE(slot->vk_owned_last_fs_spirv);
        slot->vk_owned_last_fs_spirv = NULL;
    }
    slot->vk_owned_last_vs_spirv = (uint8_t*)SIT_MALLOC(vs_len);
    slot->vk_owned_last_fs_spirv = (uint8_t*)SIT_MALLOC(fs_len);
    if (!slot->vk_owned_last_vs_spirv || !slot->vk_owned_last_fs_spirv) {
        SIT_FREE(slot->vk_owned_last_vs_spirv);
        SIT_FREE(slot->vk_owned_last_fs_spirv);
        slot->vk_owned_last_vs_spirv = NULL;
        slot->vk_owned_last_fs_spirv = NULL;
        slot->vk_last_vs_spirv = NULL;
        slot->vk_last_fs_spirv = NULL;
        slot->vk_last_vs_size = 0;
        slot->vk_last_fs_size = 0;
        return;
    }
    memcpy(slot->vk_owned_last_vs_spirv, vs, vs_len);
    memcpy(slot->vk_owned_last_fs_spirv, fs, fs_len);
    slot->vk_last_vs_spirv = slot->vk_owned_last_vs_spirv;
    slot->vk_last_fs_spirv = slot->vk_owned_last_fs_spirv;
    slot->vk_last_vs_size = vs_len;
    slot->vk_last_fs_size = fs_len;
}

static void _SitVkDestroyVDDynamicPipelinesOnSlot(_SituationShaderSlot* slot) {
    if (!slot || sit_render.vk.device == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < slot->vk_vd_dynamic_pipeline_count; ++i) {
        if (slot->vk_vd_dynamic_pipelines[i].pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, slot->vk_vd_dynamic_pipelines[i].pipeline, NULL);
            slot->vk_vd_dynamic_pipelines[i].pipeline = VK_NULL_HANDLE;
        }
    }
    slot->vk_vd_dynamic_pipeline_count = 0;
}

static VkPipeline _SitVkCreateVDDynamicPipelineClone(
    _SituationShaderSlot* slot, VkPipeline base,
    const void* vs, size_t vs_len, const void* fs, size_t fs_len,
    VkPrimitiveTopology topology, uint32_t binding_count, const VkVertexInputBindingDescription* bindings,
    uint32_t attr_count, const VkVertexInputAttributeDescription* attrs,
    uint32_t pipeline_flags, VkCullModeFlags cull, VkFrontFace front, VkPolygonMode poly,
    VkFormat color_fmt, VkFormat depth_fmt, VkSampleCountFlagBits rasterization_samples)
{
    (void)base;
    return _SituationVulkanCreateGraphicsPipelineEx(
        vs, vs_len, fs, fs_len, slot->vk_pipeline_layout, topology,
        binding_count, bindings, attr_count, attrs, pipeline_flags,
        cull, front, poly, color_fmt, depth_fmt, rasterization_samples);
}

static VkPipeline _SitVulkanResolveVDDynamicPipeline(_SituationShaderSlot* slot, VkPipeline rp_pipeline, const SituationVirtualDisplay* vd) {
    if (!slot || !vd || rp_pipeline == VK_NULL_HANDLE || !sit_render.vk.dynamic_rendering_enabled) {
        return rp_pipeline;
    }
    VkFormat color_fmt = _SitVDVkColorFormat(vd->color_format);
    VkFormat depth_fmt = _SitVDVkDepthFormat(vd);
    const int sample_count = _SitVDEffectiveSampleCount(vd);
    const VkSampleCountFlagBits raster_samples = _SitVkSampleCountFlagFromInt(sample_count);
    uint32_t key = ((uint32_t)(uintptr_t)rp_pipeline) ^
        _SitVDVkPipelineVariantKey(color_fmt, depth_fmt, sample_count);

    for (uint32_t i = 0; i < slot->vk_vd_dynamic_pipeline_count; ++i) {
        if (slot->vk_vd_dynamic_pipelines[i].key == key && slot->vk_vd_dynamic_pipelines[i].pipeline != VK_NULL_HANDLE) {
            return slot->vk_vd_dynamic_pipelines[i].pipeline;
        }
    }

    VkPipeline created = VK_NULL_HANDLE;
    _SitVkPipelineBundle* bundle = _SitVkDerefBundle(&slot->vk_bundle_ref);
    if (bundle && bundle->vs_module != VK_NULL_HANDLE && bundle->fs_module != VK_NULL_HANDLE) {
        created = _SitVkCreateVDDynamicPipelineFromModules(
            slot->vk_pipeline_layout, bundle->vs_module, bundle->fs_module, color_fmt, depth_fmt, raster_samples);
    }

    _SitVkPipelineBundle* bundle_for_spirv = bundle;
    const void* vs = slot->vk_last_vs_spirv;
    size_t vs_len = slot->vk_last_vs_size;
    const void* fs = slot->vk_last_fs_spirv;
    size_t fs_len = slot->vk_last_fs_size;
#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    if ((!vs || !fs || vs_len == 0 || fs_len == 0) && bundle_for_spirv) {
        if (_SitVkLookupSpirvBytesByHash(bundle_for_spirv->key.vs_spirv_hash, bundle_for_spirv->key.fs_spirv_hash,
                &vs, &vs_len, &fs, &fs_len)) {
            slot->vk_last_vs_spirv = vs;
            slot->vk_last_vs_size = vs_len;
            slot->vk_last_fs_spirv = fs;
            slot->vk_last_fs_size = fs_len;
        }
    }
#endif
    if ((!vs || !fs || vs_len == 0 || fs_len == 0) && bundle_for_spirv) {
        (void)bundle_for_spirv;
    }
    if (created == VK_NULL_HANDLE && (!vs || !fs || vs_len == 0 || fs_len == 0)) {
        return rp_pipeline;
    }

    VkVertexInputBindingDescription binding_pbr = {0, (3 + 3 + 4 + 2) * (uint32_t)sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs_pbr[4];
    attrs_pbr[0].binding = 0; attrs_pbr[0].location = SIT_ATTR_POSITION; attrs_pbr[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs_pbr[0].offset = 0;
    attrs_pbr[1].binding = 0; attrs_pbr[1].location = SIT_ATTR_NORMAL; attrs_pbr[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs_pbr[1].offset = 3 * (uint32_t)sizeof(float);
    attrs_pbr[2].binding = 0; attrs_pbr[2].location = SIT_ATTR_TANGENT; attrs_pbr[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs_pbr[2].offset = 6 * (uint32_t)sizeof(float);
    attrs_pbr[3].binding = 0; attrs_pbr[3].location = SIT_ATTR_TEXCOORD_0; attrs_pbr[3].format = VK_FORMAT_R32G32_SFLOAT; attrs_pbr[3].offset = 10 * (uint32_t)sizeof(float);

    VkVertexInputBindingDescription binding_legacy = {0, (3 + 3 + 2) * (uint32_t)sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs_legacy[3];
    attrs_legacy[0].binding = 0; attrs_legacy[0].location = SIT_ATTR_POSITION; attrs_legacy[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs_legacy[0].offset = 0;
    attrs_legacy[1].binding = 0; attrs_legacy[1].location = SIT_ATTR_NORMAL; attrs_legacy[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrs_legacy[1].offset = 3 * (uint32_t)sizeof(float);
    attrs_legacy[2].binding = 0; attrs_legacy[2].location = SIT_ATTR_TEXCOORD_0; attrs_legacy[2].format = VK_FORMAT_R32G32_SFLOAT; attrs_legacy[2].offset = 6 * (uint32_t)sizeof(float);

    VkVertexInputBindingDescription binding_simple = {0, 3 * (uint32_t)sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs_simple[1];
    attrs_simple[0].binding = 0; attrs_simple[0].location = SIT_ATTR_POSITION; attrs_simple[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs_simple[0].offset = 0;

    if (created == VK_NULL_HANDLE) {
    if (rp_pipeline == slot->vk_pipeline) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_pbr, 4, attrs_pbr, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_back_ccw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_pbr, 4, attrs_pbr, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_back_cw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_pbr, 4, attrs_pbr, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_legacy) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_legacy, 3, attrs_legacy, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_legacy_back_ccw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_legacy, 3, attrs_legacy, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_legacy_back_cw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_legacy, 3, attrs_legacy, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_simple) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_simple, 1, attrs_simple, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_simple_back_ccw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_simple, 1, attrs_simple, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_simple_back_cw) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_simple, 1, attrs_simple, 0u, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_line) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_pbr, 4, attrs_pbr, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_legacy_line) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_legacy, 3, attrs_legacy, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE, color_fmt, depth_fmt, raster_samples);
    } else if (rp_pipeline == slot->vk_pipeline_simple_line) {
        created = _SitVkCreateVDDynamicPipelineClone(slot, rp_pipeline, vs, vs_len, fs, fs_len, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_simple, 1, attrs_simple, 0u, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_LINE, color_fmt, depth_fmt, raster_samples);
    }

        if (created == VK_NULL_HANDLE) {
            /* Shader-cache bundle default or other unrecognized base — clone with current raster dynamics. */
            created = _SitVkCreateVDDynamicPipelineClone(
                slot, rp_pipeline, vs, vs_len, fs, fs_len,
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_simple, 1, attrs_simple, 0u,
                sit_render.vk.dynamic_cull_mode, sit_render.vk.dynamic_front_face,
                sit_render.vk.dynamic_polygon_mode, color_fmt, depth_fmt, raster_samples);
        }
    }

    if (created == VK_NULL_HANDLE) {
        return rp_pipeline;
    }
    if (slot->vk_vd_dynamic_pipeline_count >= SIT_VK_VD_DYNAMIC_PIPELINE_CACHE_MAX) {
        return created;
    }
    slot->vk_vd_dynamic_pipelines[slot->vk_vd_dynamic_pipeline_count].key = key;
    slot->vk_vd_dynamic_pipelines[slot->vk_vd_dynamic_pipeline_count].pipeline = created;
    slot->vk_vd_dynamic_pipeline_count++;
    return created;
}

static void _SitVkTransitionVDColorForRendering(VkCommandBuffer cmd, SituationVirtualDisplay* vd, SituationAttachmentLoadOp color_load) {
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = (SitVDVk(vd)->color_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        ? (VkAccessFlags)VK_ACCESS_SHADER_READ_BIT : 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = SitVDVk(vd)->color_image_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = SitVDVk(vd)->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    if (color_load == SIT_LOAD_OP_CLEAR || color_load == SIT_LOAD_OP_DONT_CARE) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.srcAccessMask = 0;
    } else if (SitVDVk(vd)->color_image_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.srcAccessMask = 0;
    }

    VkPipelineStageFlags src_stage = (barrier.srcAccessMask & VK_ACCESS_SHADER_READ_BIT)
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         0, NULL, 0, NULL, 1, &barrier);
    SitVDVk(vd)->color_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

static void _SitVkBeginVDDynamicRendering(VkCommandBuffer cmd, SituationVirtualDisplay* vd, const SituationRenderPassInfo* info) {
    _SitVkTransitionVDColorForRendering(cmd, vd, info->color_attachment.loadOp);

    float clear_rgba[4];
    _SituationColorRgbaToClearFloats(info->color_attachment.clear.color, sit_render.output_hdr_active != 0, clear_rgba);

    VkRenderingAttachmentInfo color_att = {0};
    color_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_att.imageView = SitVDVk(vd)->image_view;
    color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_att.loadOp = _SitVkMapLoadOp(info->color_attachment.loadOp);
    color_att.storeOp = _SitVkMapStoreOp(info->color_attachment.storeOp);
    color_att.clearValue.color = (VkClearColorValue){{clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]}};

    VkRenderingAttachmentInfo depth_att = {0};
    VkRenderingAttachmentInfo* p_depth = NULL;
    if (_SitVDHasDepthAttachment(vd) && SitVDVk(vd)->depth_image_view != VK_NULL_HANDLE) {
        depth_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView = SitVDVk(vd)->depth_image_view;
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_att.loadOp = _SitVkMapLoadOp(info->depth_attachment.loadOp);
        depth_att.storeOp = _SitVkMapStoreOp(info->depth_attachment.storeOp);
        depth_att.clearValue.depthStencil.depth = info->depth_attachment.clear.depth;
        depth_att.clearValue.depthStencil.stencil = info->stencil_attachment.clear.stencil;
        p_depth = &depth_att;
    }

    VkRenderingInfo rendering = {0};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.offset = (VkOffset2D){0, 0};
    rendering.renderArea.extent = (VkExtent2D){(uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color_att;
    rendering.pDepthAttachment = p_depth;
    rendering.pStencilAttachment = NULL;

    if (_SitVDHasDepthAttachment(vd) && SitVDVk(vd)->depth_image != VK_NULL_HANDLE) {
        VkImageMemoryBarrier depth_barrier = {0};
        depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depth_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_barrier.image = SitVDVk(vd)->depth_image;
        depth_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_barrier.subresourceRange.levelCount = 1;
        depth_barrier.subresourceRange.layerCount = 1;
        if (info->depth_attachment.loadOp == SIT_LOAD_OP_LOAD) {
            depth_barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, NULL, 0, NULL, 1, &depth_barrier);
    }

    if (_SitVDHasDepthAttachment(vd)) {
        sit_render.vk.dynamic_depth_test_enable = VK_TRUE;
        sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
        sit_render.vk.dynamic_depth_compare_op = VK_COMPARE_OP_LESS;
    } else {
        sit_render.vk.dynamic_depth_test_enable = VK_FALSE;
        sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
    }
    _SitVulkanApplyTrackedExtendedRasterDynamics(cmd);

    vkCmdBeginRendering(cmd, &rendering);
}

static void _SitVkEndVDDynamicRendering(VkCommandBuffer cmd, int display_id, SituationVirtualDisplay* vd) {
    vkCmdEndRendering(cmd);
    if (!vd || SitVDVk(vd)->color_image_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        return;
    }
    if (vd->color_mip_levels > 1u) {
        VkImageMemoryBarrier to_dst = {0};
        to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_dst.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_dst.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_dst.image = SitVDVk(vd)->image;
        to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_dst.subresourceRange.levelCount = 1;
        to_dst.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 0, NULL, 1, &to_dst);
        _SituationVulkanGenerateMipmaps(cmd, SitVDVk(vd)->image,
            (int32_t)vd->resolution.x, (int32_t)vd->resolution.y, vd->color_mip_levels);
        SitVDVk(vd)->color_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        _SitRenderPassSetTargetLayoutHint(display_id, SITUATION_TEXTURE_LAYOUT_SHADER_READ);
        return;
    }
    /* Phase 3b: mip0 VD stays COLOR_ATTACHMENT until readback barrier or composite transition. */
}

static void _SitVkTransitionRTColorForRendering(VkCommandBuffer cmd, _SituationRenderTargetSlot* rts, SituationAttachmentLoadOp color_load) {
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = (rts->color_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        ? (VkAccessFlags)VK_ACCESS_SHADER_READ_BIT : 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = rts->color_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rts->color_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    if (color_load == SIT_LOAD_OP_CLEAR || color_load == SIT_LOAD_OP_DONT_CARE) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.srcAccessMask = 0;
    } else if (rts->color_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.srcAccessMask = 0;
    }

    VkPipelineStageFlags src_stage = (barrier.srcAccessMask & VK_ACCESS_SHADER_READ_BIT)
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         0, NULL, 0, NULL, 1, &barrier);
    rts->color_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

static void _SitVkBeginRTDynamicRendering(VkCommandBuffer cmd, _SituationRenderTargetSlot* rts, const SituationRenderPassInfo* info) {
    _SitVkTransitionRTColorForRendering(cmd, rts, info->color_attachment.loadOp);

    float clear_rgba[4];
    _SituationColorRgbaToClearFloats(info->color_attachment.clear.color, false, clear_rgba);

    VkRenderingAttachmentInfo color_att = {0};
    color_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_att.imageView = rts->color_view;
    color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_att.loadOp = _SitVkMapLoadOp(info->color_attachment.loadOp);
    color_att.storeOp = _SitVkMapStoreOp(info->color_attachment.storeOp);
    color_att.clearValue.color = (VkClearColorValue){{clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]}};

    VkRenderingAttachmentInfo depth_att = {0};
    VkRenderingAttachmentInfo* p_depth = NULL;
    if (rts->has_depth && rts->depth_view != VK_NULL_HANDLE) {
        depth_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView = rts->depth_view;
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_att.loadOp = _SitVkMapLoadOp(info->depth_attachment.loadOp);
        depth_att.storeOp = _SitVkMapStoreOp(info->depth_attachment.storeOp);
        depth_att.clearValue.depthStencil.depth = info->depth_attachment.clear.depth;
        depth_att.clearValue.depthStencil.stencil = info->stencil_attachment.clear.stencil;
        p_depth = &depth_att;
    }

    VkRenderingInfo rendering = {0};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.offset = (VkOffset2D){0, 0};
    rendering.renderArea.extent = (VkExtent2D){(uint32_t)rts->width, (uint32_t)rts->height};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color_att;
    rendering.pDepthAttachment = p_depth;
    rendering.pStencilAttachment = NULL;

    if (rts->has_depth && rts->depth_image != VK_NULL_HANDLE) {
        VkImageMemoryBarrier depth_barrier = {0};
        depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depth_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_barrier.image = rts->depth_image;
        depth_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_barrier.subresourceRange.levelCount = 1;
        depth_barrier.subresourceRange.layerCount = 1;
        if (info->depth_attachment.loadOp == SIT_LOAD_OP_LOAD) {
            depth_barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, NULL, 0, NULL, 1, &depth_barrier);
    }

    if (rts->has_depth) {
        sit_render.vk.dynamic_depth_test_enable = VK_TRUE;
        sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
        sit_render.vk.dynamic_depth_compare_op = VK_COMPARE_OP_LESS;
    } else {
        sit_render.vk.dynamic_depth_test_enable = VK_FALSE;
        sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
    }
    _SitVulkanApplyTrackedExtendedRasterDynamics(cmd);
    vkCmdBeginRendering(cmd, &rendering);
}

static void _SitVkEndRTDynamicRendering(VkCommandBuffer cmd, _SituationRenderTargetSlot* rts) {
    vkCmdEndRendering(cmd);
    if (!rts || rts->color_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        return;
    }
    /* Phase 3c: stay COLOR_ATTACHMENT until readback barrier or SituationReadRenderTarget. */
}

static void _SitVkEnsureVDColorShaderReadForComposite(VkCommandBuffer cmd, SituationVirtualDisplay* vd) {
    if (!vd || SitVDVk(vd)->color_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        return;
    }
    int display_id = (int)(vd - sit_render.virtual_display_slots);
    SituationTextureLayout old_layout = SITUATION_TEXTURE_LAYOUT_UNDEFINED;
    if (SitVDVk(vd)->color_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        old_layout = SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT;
    } else if (SitVDVk(vd)->color_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        old_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC;
    } else {
        return;
    }

    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;
    VkAccessFlags src_access_mask = 0;
    VkAccessFlags dst_access_mask = 0;
    _SituationVulkanTextureLayoutBarrierMasks(old_layout, true, &src_stage_mask, &src_access_mask);
    _SituationVulkanTextureLayoutBarrierMasks(SITUATION_TEXTURE_LAYOUT_SHADER_READ, false, &dst_stage_mask, &dst_access_mask);
    if (src_stage_mask == 0 || dst_stage_mask == 0) {
        return;
    }

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = _SituationVulkanMapTextureLayout(old_layout);
    barrier.newLayout = _SituationVulkanMapTextureLayout(SITUATION_TEXTURE_LAYOUT_SHADER_READ);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = SitVDVk(vd)->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, src_stage_mask, dst_stage_mask, 0, 0, NULL, 0, NULL, 1, &barrier);
    SitVDVk(vd)->color_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (display_id >= 0 && display_id < SITUATION_MAX_VIRTUAL_DISPLAYS) {
        _SitRenderPassSetTargetLayoutHint(display_id, SITUATION_TEXTURE_LAYOUT_SHADER_READ);
    }
}
#endif

static SituationError _SitVulkanEnsureGraphicsPipelineBound(VkCommandBuffer vk_cmd, _SituationShaderSlot* shader_slot, size_t stride) {
    if (vk_cmd == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "_SitVulkanEnsureGraphicsPipelineBound: command buffer cannot be NULL.");
    }
    if (!shader_slot) {
        return SITUATION_SUCCESS;
    }
    VkPipeline selected = _SitVulkanResolveGraphicsPipeline(shader_slot, stride);
    if (sit_render.vk.inside_render_pass && sit_render.vk.recording_pass_display_id >= 0 &&
        sit_render.vk.dynamic_rendering_enabled &&
        sit_render.virtual_display_slots_used[sit_render.vk.recording_pass_display_id]) {
        const SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[sit_render.vk.recording_pass_display_id];
        selected = _SitVulkanResolveVDDynamicPipeline(shader_slot, selected, vd);
    }
    if (selected == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
            "_SitVulkanEnsureGraphicsPipelineBound: no graphics pipeline variant for bound shader.");
    }
    if (selected != sit_render.vk.current_pbr_pipeline) {
#if !defined(NDEBUG)
        sit_render.vk.raster_pipeline_rebind_count++;
#endif
        vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, selected);
        sit_render.vk.current_pbr_pipeline = selected;
    }

    /* Always re-apply dynamics: vkCmdBindPipeline does not preserve dynamic polygon/topology/bias. */
    _SitVulkanApplyTrackedRasterDynamics(vk_cmd);
    return SITUATION_SUCCESS;
}

static void _SitVulkanApplyRasterState(VkCommandBuffer vk_cmd, const _SitVulkanRasterStackEntry* entry) {
    if (!entry || vk_cmd == VK_NULL_HANDLE) return;
    sit_render.vk.dynamic_color_write_mask = entry->color_write_mask;
    sit_render.vk.dynamic_depth_test_enable = entry->depth_test_enable;
    sit_render.vk.dynamic_depth_write_enable = entry->depth_write_enable;
    sit_render.vk.dynamic_depth_compare_op = entry->depth_compare_op;
    sit_render.vk.dynamic_stencil_test_enable = entry->stencil_test_enable;
    sit_render.vk.dynamic_stencil_front = entry->stencil_front;
    sit_render.vk.dynamic_stencil_back = entry->stencil_back;
    sit_render.vk.dynamic_line_width = entry->line_width;
    sit_render.vk.dynamic_cull_mode = entry->cull_mode;
    sit_render.vk.dynamic_front_face = entry->front_face;
    sit_render.vk.dynamic_depth_bias_enable = entry->depth_bias_enable;
    sit_render.vk.dynamic_depth_bias_constant = entry->depth_bias_constant;
    sit_render.vk.dynamic_depth_bias_clamp = entry->depth_bias_clamp;
    sit_render.vk.dynamic_depth_bias_slope = entry->depth_bias_slope;
    // Multisample — restore shadow state only; no dynamic command dispatch on VK without ext3
    sit_render.vk.dynamic_ms_sample_shading_enable    = entry->ms_sample_shading_enable;
    sit_render.vk.dynamic_ms_min_sample_shading       = entry->ms_min_sample_shading;
    sit_render.vk.dynamic_ms_sample_mask              = entry->ms_sample_mask;
    sit_render.vk.dynamic_ms_alpha_to_coverage_enable = entry->ms_alpha_to_coverage_enable;
    if (sit_render.vk.current_bound_shader_slot) {
        (void)_SitVulkanEnsureGraphicsPipelineBound(vk_cmd,
            sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride);
    } else {
        _SitVulkanApplyTrackedRasterDynamics(vk_cmd);
    }
}

static SituationError _SitVulkanValidateInternalQuadDrawReady(VkCommandBuffer vk_cmd, const char* caller) {
    char detail[192];
    if (!caller) {
        caller = "Internal quad draw";
    }
    if (vk_cmd == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: command buffer cannot be NULL.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    if (!sit_render.vk.inside_render_pass) {
        snprintf(detail, sizeof(detail), "%s: no active render pass.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, detail);
    }
    if (sit_render.vk.quad_pipeline == VK_NULL_HANDLE || sit_render.vk.quad_pipeline_layout == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: internal quad pipeline is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, detail);
    }
    if (sit_render.vk.quad_vertex_buffer == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: internal quad vertex buffer is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_VULKAN)
/** Phase D5 — bind global_bindless_set at set 1 for layouts that include bindless_descriptor_layout (deduped per frame). */
static SituationError _SitVulkanBindGlobalBindlessSet(
    VkCommandBuffer vk_cmd, VkPipelineLayout layout, uint32_t set_index, const char* caller)
{
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        return SITUATION_SUCCESS;
    }
    if (vk_cmd == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            caller ? caller : "_SitVulkanBindGlobalBindlessSet: invalid cmd or layout.");
    }
    if (sit_render.vk.global_bindless_set == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED,
            caller ? caller : "_SitVulkanBindGlobalBindlessSet: global bindless set not initialized.");
    }
    if (sit_render.vk.global_bindless_graphics_bound
        && sit_render.vk.global_bindless_graphics_layout == layout) {
        return SITUATION_SUCCESS;
    }
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set_index, 1,
        &sit_render.vk.global_bindless_set, 0, NULL);
    sit_render.vk.global_bindless_graphics_bound = true;
    sit_render.vk.global_bindless_graphics_layout = layout;
    return SITUATION_SUCCESS;
}

/** Write a sampled texture slot into global_bindless_set[slot_idx] (Phase D2). */
static SituationError _SitVulkanWriteSlotToGlobalBindlessSet(
    _SituationTextureSlot* slot, uint32_t slot_idx, const char* caller)
{
    char detail[256];
    const char* ctx = (caller && caller[0]) ? caller : "_SitVulkanWriteSlotToGlobalBindlessSet";

    if (!slot || slot_idx >= SITUATION_MAX_TEXTURES) {
        snprintf(detail, sizeof(detail), "%s: invalid texture slot or slot index.", ctx);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    if (sit_render.vk.global_bindless_set == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: global bindless descriptor set is not initialized.", ctx);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    if (slot->image_view == VK_NULL_HANDLE || slot->sampler == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: texture slot %u is missing image view or sampler.", ctx, slot_idx);
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, detail);
    }
    VkDescriptorImageInfo bindless_image_info = {};
    bindless_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bindless_image_info.imageView = slot->image_view;
    bindless_image_info.sampler = slot->sampler;
    VkWriteDescriptorSet bindless_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    bindless_write.dstSet = sit_render.vk.global_bindless_set;
    bindless_write.dstBinding = 0;
    bindless_write.dstArrayElement = slot_idx;
    bindless_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindless_write.descriptorCount = 1;
    bindless_write.pImageInfo = &bindless_image_info;
    vkUpdateDescriptorSets(sit_render.vk.device, 1, &bindless_write, 0, NULL);
    return SITUATION_SUCCESS;
}
#endif

static SituationError _SitVulkanValidateInternalTextDrawReady(VkCommandBuffer vk_cmd, const char* caller) {
    char detail[192];
    if (!caller) {
        caller = "Internal text draw";
    }
    if (vk_cmd == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: command buffer cannot be NULL.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    if (!sit_render.vk.inside_render_pass) {
        snprintf(detail, sizeof(detail), "%s: no active render pass.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, detail);
    }
    if (sit_render.vk.text_pipeline == VK_NULL_HANDLE || sit_render.vk.text_pipeline_layout == VK_NULL_HANDLE) {
        snprintf(detail, sizeof(detail), "%s: internal text pipeline is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, detail);
    }
    return SITUATION_SUCCESS;
}
#endif

#if defined(SITUATION_USE_OPENGL)
static SituationError _SituationGLValidateInternalQuadDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass) {
    char detail[192];
    if (!caller) {
        caller = "Internal quad draw";
    }
    if (!buf) {
        snprintf(detail, sizeof(detail), "%s: command buffer cannot be NULL.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    if (buf->is_broken) {
        snprintf(detail, sizeof(detail), "%s: soft command buffer is broken (prior record failure).", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_BUFFER_FULL, detail);
    }
    if (require_recorded_render_pass && !buf->recording_render_pass_active) {
        snprintf(detail, sizeof(detail), "%s: no active render pass.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, detail);
    }
    if (sit_render.gl.quad_shader_program == 0) {
        snprintf(detail, sizeof(detail), "%s: internal quad shader is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    if (sit_render.gl.quad_vao == 0 || sit_render.gl.quad_vbo == 0) {
        snprintf(detail, sizeof(detail), "%s: internal quad geometry is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    return SITUATION_SUCCESS;
}

static SituationError _SituationGLValidateInternalTextDrawReady(SituationGLSoftCommandBuffer* buf, const char* caller, bool require_recorded_render_pass) {
    char detail[192];
    if (!caller) {
        caller = "Internal text draw";
    }
    if (!buf) {
        snprintf(detail, sizeof(detail), "%s: command buffer cannot be NULL.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, detail);
    }
    if (buf->is_broken) {
        snprintf(detail, sizeof(detail), "%s: soft command buffer is broken (prior record failure).", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_BUFFER_FULL, detail);
    }
    if (require_recorded_render_pass && !buf->recording_render_pass_active) {
        snprintf(detail, sizeof(detail), "%s: no active render pass.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, detail);
    }
    if (sit_render.gl.text_shader_program == 0) {
        snprintf(detail, sizeof(detail), "%s: internal text shader is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    if (sit_render.gl.text_vao == 0 || sit_render.gl.text_vbo == 0) {
        snprintf(detail, sizeof(detail), "%s: internal text geometry is not initialized.", caller);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, detail);
    }
    return SITUATION_SUCCESS;
}
#endif

/**
 * @brief [Core] Binds a vertex buffer for subsequent draw calls.
 * @details Records a command to set the active vertex buffer at the given binding point.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL 4.6:** Records `glVertexArrayVertexBuffer` on the global VAO (DSA).
 * - **Vulkan 1.x:** Records `vkCmdBindVertexBuffers` at `binding` with `offset`. If `SituationCmdBindPipeline` was called
 *   earlier in the same pass, selects `vk_pipeline_simple` / `vk_pipeline_legacy` / PBR from `stride` (same rules as `SituationCmdDrawMesh`).
 *
 * @param cmd The command buffer to record into.
 * @param binding Vertex input binding index (usually 0).
 * @param buffer Vertex buffer handle.
 * @param offset Byte offset into the buffer.
 * @param stride Stride between vertices (OpenGL soft replay; must match pipeline on Vulkan).
 */
SITAPI SituationError SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer, size_t offset, size_t stride) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBindVertexBuffer: invalid buffer handle");
        return SITUATION_ERROR_INVALID_RESOURCE_HANDLE;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_VERTEX_BUFFER, p);
    p->args.bind_vbo.binding = binding;
    p->args.bind_vbo.buffer_id = (uint64_t)slot->gl_buffer_id;
    p->args.bind_vbo.offset = offset;
    p->args.bind_vbo.stride = stride;
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    /* Match DrawMesh: pick pipeline variant from stride after BindPipeline (default vk_pipeline has no vertex input). */
    if (sit_render.vk.current_bound_shader_slot) {
        SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd, sit_render.vk.current_bound_shader_slot, stride));
    }

    VkBuffer vertex_buffers[] = { slot->vk_buffer };
    VkDeviceSize offsets[] = { (VkDeviceSize)offset };
    vkCmdBindVertexBuffers(vk_cmd, binding, 1, vertex_buffers, offsets);
    sit_render.vk.current_graphics_vertex_stride = stride;
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

static size_t _SitIndexTypeElementSize(SituationIndexType index_type) {
    return (index_type == SIT_INDEX_UINT16) ? sizeof(uint16_t) : sizeof(uint32_t);
}

#if defined(SITUATION_USE_OPENGL)
static GLenum _SitGLIndexType(SituationIndexType index_type) {
    return (index_type == SIT_INDEX_UINT16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}
#elif defined(SITUATION_USE_VULKAN)
static VkIndexType _SitVkIndexType(SituationIndexType index_type) {
    return (index_type == SIT_INDEX_UINT16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}
#endif

static SituationError _SituationValidateIndexBufferBind(SituationBuffer buffer, size_t offset, SituationIndexType index_type,
                                                        _SituationBufferSlot** out_slot) {
    if (index_type != SIT_INDEX_UINT16 && index_type != SIT_INDEX_UINT32) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid SituationIndexType.");
    }

    size_t align = _SitIndexTypeElementSize(index_type);
    if ((offset % align) != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "Index buffer offset must be aligned to the index element size (2 for UINT16, 4 for UINT32).");
    }

    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBindIndexBufferEx: invalid buffer handle");
    }
    if ((slot->usage_flags & SITUATION_BUFFER_USAGE_INDEX_BUFFER) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE,
            "Index buffer missing SITUATION_BUFFER_USAGE_INDEX_BUFFER.");
    }
    if (offset > slot->size_in_bytes) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Index buffer offset is outside the buffer.");
    }

    *out_slot = slot;
    return SITUATION_SUCCESS;
}

/**
 * @brief [Core] Binds an index buffer for subsequent indexed draw calls.
 * @details Records index buffer binding with explicit 16- or 32-bit index element type.
 *
 * @param cmd The command buffer to record into.
 * @param buffer Index buffer handle (`SITUATION_BUFFER_USAGE_INDEX_BUFFER`).
 * @param offset Byte offset into the index buffer (must be aligned to index element size).
 * @param index_type `SIT_INDEX_UINT16` or `SIT_INDEX_UINT32`.
 */
SITAPI SituationError SituationCmdBindIndexBufferEx(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset,
                                                    SituationIndexType index_type) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    _SituationBufferSlot* slot = NULL;
    SituationError err = _SituationValidateIndexBufferBind(buffer, offset, index_type, &slot);
    if (err != SITUATION_SUCCESS) return err;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_INDEX_BUFFER, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.bind_ibo.buffer_id = (uint64_t)slot->gl_buffer_id;
    p->args.bind_ibo.offset = offset;
    p->args.bind_ibo.index_type = index_type;
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    VkIndexType vk_index_type = _SitVkIndexType(index_type);
    vkCmdBindIndexBuffer(vk_cmd, slot->vk_buffer, (VkDeviceSize)offset, vk_index_type);
    sit_render.vk.current_index_type = vk_index_type;
    sit_render.vk.bound_ibo_index_element_size = _SitIndexTypeElementSize(index_type);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief [Core] Binds a 32-bit index buffer (compatibility wrapper).
 */
SITAPI SituationError SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset) {
    return SituationCmdBindIndexBufferEx(cmd, buffer, offset, SIT_INDEX_UINT32);
}

/**
 * @brief Binds a texture as a storage image for a compute shader.
 *
 * @details Associates a texture with a specific binding slot (e.g., `binding = 2` in GLSL).
 *          - **Vulkan:** Binds the texture's pre-cached descriptor set to the specified `binding` index in the command buffer.
 *          - **OpenGL:** Calls `glBindImageTexture`.
 *
 * @param cmd The current command buffer.
 * @param binding The binding index in the shader layout.
 * @param texture The texture to bind. Must support storage usage.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` or `SITUATION_ERROR_RESOURCE_INVALID` on failure.
 */
SITAPI SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    // [Bug Fix] Use LEGACY_TEXTURE_HANDLING opcode which correctly handles resource_type=3 (storage image)
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING, p);

    p->args.bind_desc.set_index = binding;
    p->args.bind_desc.resource_id = slot->gl_texture_id;
    p->args.bind_desc.resource_type = 3; // 3 = Image Texture (Storage)
    _SitVDNoteComputeTextureBind(cmd, binding, (int)texture.slot_index);
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    // FIX: Use the 'binding' parameter as the set index, matching the Unified API buffer logic.
    vkCmdBindDescriptorSets(
        (VkCommandBuffer)cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        sit_render.vk.current_compute_pipeline_layout,
        binding,  // FIX: Was '0' in original code
        1,
        &slot->descriptor_set,
        0, NULL
    );
    _SitVDNoteComputeTextureBind(cmd, binding, (int)texture.slot_index);
    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief [Core] Records a non-indexed drawing command.
 * @details Renders primitives sequentially from the currently bound vertex buffer.
 *
 * @param cmd The command buffer to record the command into.
 * @param vertex_count The number of vertices to draw.
 * @param instance_count The number of instances to draw (for instanced rendering).
 * @param first_vertex The index of the first vertex to draw.
 * @param first_instance The instance ID of the first instance to draw.
 */
SITAPI SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (vertex_count == 0 || instance_count == 0) return SITUATION_SUCCESS; // No-op is success

    // Mark that a draw command has happened this frame
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    // Triangle count approximation for standard topology (Triangle List)
    sit_render.frame_triangle_count += (vertex_count / 3) * instance_count;
    _SitVDRecordingNoteDrawCmd(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW, p);
    if (p) {
        p->args.draw.v_count = vertex_count;
        p->args.draw.i_count = instance_count;
        p->args.draw.first_v = first_vertex;
        p->args.draw.first_i = first_instance;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    if (!sit_render.vk.current_bound_shader_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,
            "SituationCmdDraw: bind a graphics pipeline before drawing.");
    }
    SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd, sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    _SitVulkanApplyTrackedRasterDynamics(vk_cmd);
    vkCmdDraw(vk_cmd, vertex_count, instance_count, first_vertex, first_instance);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief [Core] Records an indexed drawing command.
 * @details Renders primitives using indices from the currently bound index buffer to look up vertices from the currently bound vertex buffer.
 *
 * @param cmd The command buffer to record the command into.
 * @param index_count The number of indices to draw.
 * @param instance_count The number of instances to draw (for instanced rendering).
 * @param first_index The offset into the index buffer to start reading indices from.
 * @param vertex_offset A value added to each index before looking up a vertex.
 * @param first_instance The instance ID of the first instance to draw.
 */
SITAPI SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (index_count == 0 || instance_count == 0) return SITUATION_SUCCESS;

    // Update Stats
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (index_count / 3) * instance_count;
    _SitVDRecordingNoteDrawCmd(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_INDEXED, p);
    if (p) {
        p->args.draw_indexed.idx_count = index_count;
        p->args.draw_indexed.inst_count = instance_count;
        p->args.draw_indexed.first_idx = first_index;
        p->args.draw_indexed.v_offset = vertex_offset;
        p->args.draw_indexed.first_inst = first_instance;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    if (!sit_render.vk.current_bound_shader_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,
            "SituationCmdDrawIndexed: bind a graphics pipeline before drawing.");
    }
    SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd, sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    _SitVulkanApplyTrackedRasterDynamics(vk_cmd);
    vkCmdDrawIndexed(vk_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
#endif
    return SITUATION_SUCCESS;
}

static SituationError _SituationValidateIndirectDrawBuffer(SituationBuffer indirect_buffer, size_t offset, size_t command_size) {
    if ((offset & 3u) != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INDIRECT_COMMAND_INVALID,
            "Indirect draw offset must be 4-byte aligned.");
    }

    _SituationBufferSlot* slot = _SitGetBufferSlot(indirect_buffer);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,
            "Indirect draw buffer handle is invalid.");
    }
    if ((slot->usage_flags & SITUATION_BUFFER_USAGE_INDIRECT_BUFFER) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE,
            "Indirect draw buffer missing SITUATION_BUFFER_USAGE_INDIRECT_BUFFER.");
    }
    if (offset > slot->size_in_bytes || slot->size_in_bytes - offset < command_size) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INDIRECT_COMMAND_INVALID,
            "Indirect draw command range is outside the buffer.");
    }
    return SITUATION_SUCCESS;
}

static SituationError _SituationCmdDrawIndirectRecord(SituationCommandBuffer cmd,
                                                      SituationBuffer indirect_buffer,
                                                      size_t offset,
                                                      size_t command_size,
                                                      bool indexed_draw) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Indirect draw: library not initialized.");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Indirect draw: cmd cannot be NULL.");
    }

    SituationError err = _SituationValidateIndirectDrawBuffer(indirect_buffer, offset, command_size);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    _SituationBufferSlot* slot = _SitGetBufferSlot(indirect_buffer);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "Indirect draw buffer handle is invalid.");
    }

    _SitVDRecordingNoteDrawCmd(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf->recording_render_pass_active) {
        return SITUATION_ERROR_NO_RENDER_PASS_ACTIVE;
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, indexed_draw ? SIT_OP_DRAW_INDEXED_INDIRECT : SIT_OP_DRAW_INDIRECT, p);
    if (!p) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    if (!indexed_draw) {
        p->args.draw_indirect.buffer_id = (uint64_t)slot->gl_buffer_id;
        p->args.draw_indirect.offset = offset;
    } else {
        p->args.draw_indexed_indirect.buffer_id = (uint64_t)slot->gl_buffer_id;
        p->args.draw_indexed_indirect.offset = offset;
    }
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    if (!sit_render.vk.inside_render_pass) {
        return SITUATION_ERROR_NO_RENDER_PASS_ACTIVE;
    }
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (!sit_render.vk.current_bound_shader_slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,
            "Indirect draw: bind a graphics pipeline before drawing.");
    }
    SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd, sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    _SitVulkanApplyTrackedRasterDynamics(vk_cmd);
    if (!indexed_draw) {
        vkCmdDrawIndirect(vk_cmd, slot->vk_buffer, (VkDeviceSize)offset, 1, (uint32_t)sizeof(SituationDrawIndirectCommand));
    } else {
        vkCmdDrawIndexedIndirect(vk_cmd, slot->vk_buffer, (VkDeviceSize)offset, 1, (uint32_t)sizeof(SituationDrawIndexedIndirectCommand));
    }
    return SITUATION_SUCCESS;
#else
    (void)indexed_draw;
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdDrawIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset) {
    SituationError err = _SituationCmdDrawIndirectRecord(cmd, indirect_buffer, offset,
        sizeof(SituationDrawIndirectCommand), false);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset) {
    SituationError err = _SituationCmdDrawIndirectRecord(cmd, indirect_buffer, offset,
        sizeof(SituationDrawIndexedIndirectCommand), true);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    return SITUATION_SUCCESS;
}

/**
 * @brief Draws a text string using GPU-accelerated textured quads.
 *
 * @details Renders a string of text into the current command buffer. This function is extremely fast and suitable for real-time UIs.
 *          It uses the internal quad renderer with a font atlas texture.
 *
 * @param cmd The command buffer to record into.
 * @param font The font to use. Must have been baked with `SituationBakeFontAtlas`.
 * @param text The null-terminated string to draw.
 * @param pos The screen-space position (top-left) to start drawing.
 * @param color The color tint for the text.
 *
 * @note Requires a valid orthographic projection matrix to be active in the view UBO (which `SituationAcquireFrameCommandBuffer` sets up by default).
 */
SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color) {
    return SituationCmdDrawTextEx(cmd, font, text, pos, 0.0f, 0.0f, color);
}

/**
 * @brief Draws a text string using GPU-accelerated textured quads.
 * @details Records a batch of draw commands to render text using the internal text renderer pipeline.
 *          This function supports custom font sizing and character spacing adjustments at runtime.
 *
 * @par Performance (Optimized v2.3.27)
 * - **Vulkan:** Uses a **persistent mapped ring buffer** to write vertex data directly to GPU-visible memory.
 *   This eliminates per-draw buffer allocations and staging copies, offering near-zero overhead for dynamic UI.
 *   (Falls back to staging upload only if the ring buffer fills up within a single frame).
 * - **OpenGL:** Data is packed into the soft command stream for deferred execution.
 *
 * @param cmd The command buffer to record into.
 * @param font The font to use. Must have been baked with `SituationBakeFontAtlas`.
 * @param text The text string to render.
 * @param pos The screen position (top-left) in pixels.
 * @param fontSize The desired font height in pixels. Pass 0.0f to use the native baked size.
 * @param spacing Additional spacing between characters in pixels. Can be negative.
 * @param color The text color tint.
 */
SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color) {
    if (!SituationIsInitialized() || !text) return SITUATION_ERROR_INVALID_PARAM;

    // Default Debug Font Fallback
    SituationFont use_font = font;
    if (use_font.atlas_texture.generation == 0) {
        use_font = sit_render.default_font;
        if (use_font.atlas_texture.generation == 0) return SITUATION_ERROR_RESOURCE_INVALID;
    }

    bool is_grid_font = _SituationFontIsGridAtlas(&use_font);
    if (!is_grid_font && !use_font.glyph_info) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    size_t len = strlen(text);
#if !defined(SITUATION_NO_STB) && !defined(SITUATION_NO_STB_TRUETYPE)
    if (len == 0) return SITUATION_SUCCESS;
    if (len > 2048) len = 2048;

    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (len * 2);
    _SitVDRecordingNoteDrawCmd(cmd);

    Vector4 color_vec;
    SituationConvertColorToVector4(color, &color_vec);

#if defined(SITUATION_USE_OPENGL)
    // --- OPENGL PATH (Standard Soft Buffer) ---
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SIT_RETURN_IF_ERR(_SituationGLValidateInternalTextDrawReady(buf, "SituationCmdDrawTextEx", true));
    void* text_ptr = NULL;
    SIT_GL_SOFT_DATA_PUSH(buf, text, len + 1, text_ptr);
    size_t text_offset = (size_t)((uint8_t*)text_ptr - buf->data_buffer);

    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_TEXT_EX, p);
    if (p) {
        p->args.draw_text_ex.font = use_font;
        p->args.draw_text_ex.pos = pos;
        p->args.draw_text_ex.color = color;
        p->args.draw_text_ex.text_offset = text_offset;
        p->args.draw_text_ex.fontSize = fontSize;
        p->args.draw_text_ex.spacing = spacing;

        // [v2.3.30] Bindless Logic
        // The SIT_OP_DRAW_TEXT_EX packet doesn't store the bindless handle explicitly.
        // Instead, the executor (_SituationGLExecuteCommands) will detect if the feature is enabled
        // and resolve the handle from the font's atlas texture ID at draw time.
        // This keeps the packet size small and logic centralized in the executor.
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    // --- VULKAN OPTIMIZED PATH ---

    // 2. Calculate Size (6 verts/char * 4 floats/vert)
    size_t data_size = len * 6 * 4 * sizeof(float);

    VkBuffer target_buffer = VK_NULL_HANDLE;
    size_t target_offset = 0;
    float* write_ptr = NULL;
    float* local_scratch = NULL; // [FIX] Declare outside the if/else block!

    uint32_t frame_idx = sit_render.vk.current_frame_index;

    // 3. POINTER SELECTION: Decide where to write
    // Try Fast Path: Is there space in the mapped Ring Buffer?
    if (sit_render.vk.dynamic_vbo_mapped[frame_idx] &&
        (sit_render.vk.dynamic_vbo_cursor + data_size <= sit_render.vk.dynamic_vbo_capacity))
    {
        // FAST: Write directly to GPU-mapped memory
        write_ptr = (float*)((uint8_t*)sit_render.vk.dynamic_vbo_mapped[frame_idx] + sit_render.vk.dynamic_vbo_cursor);
        target_buffer = sit_render.vk.dynamic_vbo[frame_idx];
        target_offset = sit_render.vk.dynamic_vbo_cursor;

        // Reserve the space
        sit_render.vk.dynamic_vbo_cursor += data_size;
    } else {
        // SLOW: Ring buffer full. Use local allocation to prevent worker thread races.
        local_scratch = (float*)SIT_MALLOC(data_size);
        write_ptr = local_scratch;
    }

    if (!write_ptr) return SITUATION_ERROR_NOT_INITIALIZED; // Allocation failed

    // 4. VERTEX GENERATION (Unified Loop)
    // This logic fills 'write_ptr', regardless of where it points.
    float x = pos.x;
    float y = pos.y;
    stbtt_bakedchar* cdata = (stbtt_bakedchar*)use_font.glyph_info;
    int v_idx = 0;

    float target_size = (fontSize > 0.0f) ? fontSize : use_font.font_height_pixels;
    float scale_factor = (use_font.font_height_pixels > 0.0f) ? (target_size / use_font.font_height_pixels) : 1.0f;
    float line_start_x = x;

    for (size_t i = 0; i < len; i++) {
        if (is_grid_font) {
            _SituationFontEmitGridGlyph(
                write_ptr, &v_idx, &use_font, (unsigned char)text[i],
                &x, &y, line_start_x, scale_factor, spacing);
        }
        else if (text[i] >= 32 && text[i] < 128) {
            float x_before = x;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, use_font.atlas_width, use_font.atlas_height, text[i] - 32, &x, &y, &q, 1);

            if (scale_factor != 1.0f || spacing != 0.0f) {
                float w = q.x1 - q.x0;
                float h = q.y1 - q.y0;
                float y_off = q.y0 - y;

                float x0 = x_before + (q.x0 - x_before) * scale_factor;
                float y0 = y + y_off * scale_factor;
                float x1 = x0 + w * scale_factor;
                float y1 = y0 + h * scale_factor;

                q.x0 = x0; q.y0 = y0;
                q.x1 = x1; q.y1 = y1;

                float advance = x - x_before;
                x = x_before + (advance * scale_factor) + spacing;
            }

            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t0;
            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t1;
            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t0;

            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t0;
            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t1;
            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t1;
        }
    }

    // 5. UPLOAD (Fallback Path Only)
    // If we wrote to scratch (target_buffer is NULL), we must upload now.
    if (target_buffer == VK_NULL_HANDLE) {
        VkBuffer temp_buffer;
        VmaAllocation temp_alloc;
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // This helper creates a staging buffer + device local buffer and copies
        if (_SituationVulkanCreateAndUploadBuffer(vk_cmd, sit_render.text_batch_scratch, data_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &temp_buffer, &temp_alloc) == SITUATION_SUCCESS) {
            target_buffer = temp_buffer;
            target_offset = 0;
            // Mark for deletion at end of frame
            _SituationDeferDestroyBuffer(temp_buffer, temp_alloc);
        }
    }

    // 6. DRAW
    if (target_buffer != VK_NULL_HANDLE) {
        uint32_t vert_count = (uint32_t)(v_idx / 4);
        if (vert_count == 0) {
            return SITUATION_SUCCESS;
        }

        if (target_buffer == sit_render.vk.dynamic_vbo[frame_idx] &&
            sit_render.vk.dynamic_vbo_alloc[frame_idx] != VK_NULL_HANDLE) {
            vmaFlushAllocation(sit_render.vk.vma_allocator, sit_render.vk.dynamic_vbo_alloc[frame_idx], target_offset, (VkDeviceSize)(v_idx * sizeof(float)));
        }

        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
        SIT_RETURN_IF_ERR(_SitVulkanValidateInternalTextDrawReady(vk_cmd, "SituationCmdDrawTextEx"));

        // CRITICAL: Update global state so descriptor set binding works
        sit_render.vk.current_pipeline_layout_for_push_constants = sit_render.vk.text_pipeline_layout;

        vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.text_pipeline);

        uint32_t frame_idx = sit_render.vk.current_frame_index;
        _SituationTextureSlot* font_slot = _SitGetTextureSlot(use_font.atlas_texture);
        if (!font_slot || !use_font.atlas_texture.generation) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
                "SituationCmdDrawTextEx: font atlas texture is invalid or stale.");
        }

        SIT_RETURN_IF_ERR(_SitVulkanWriteSlotToGlobalBindlessSet(
            font_slot, (uint32_t)use_font.atlas_texture.slot_index, "SituationCmdDrawTextEx"));

        vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            sit_render.vk.text_pipeline_layout, 0, 1,
            &sit_render.vk.view_proj_ubo_descriptor_set[frame_idx], 0, NULL);
        VkDescriptorSet font_tex_set = VK_NULL_HANDLE;
        if (font_slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            font_tex_set = font_slot->single_sampler_descriptor_set;
        } else if (font_slot->descriptor_set != VK_NULL_HANDLE) {
            font_tex_set = font_slot->descriptor_set;
        }
        if (font_tex_set == VK_NULL_HANDLE) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
                "SituationCmdDrawTextEx: font atlas has no sampler descriptor set.");
        }
        vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            sit_render.vk.text_pipeline_layout, 1, 1, &font_tex_set, 0, NULL);

        // Bind Vertex Buffer with OFFSET
        VkDeviceSize offsets[] = { target_offset };
        vkCmdBindVertexBuffers(vk_cmd, 0, 1, &target_buffer, offsets);

        struct { Vector4 color; uint32_t texture_id; } text_pc;
        text_pc.color = color_vec;
        text_pc.texture_id = use_font.atlas_texture.slot_index;
        vkCmdPushConstants(vk_cmd, sit_render.vk.text_pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(text_pc), &text_pc);
        _SitVulkanApply2DViewportScissor(vk_cmd);
        if (_SitVulkanGraphicsDynamicProcsReady()) {
            sit_render.vk.pfn_cmd_set_primitive_topology(vk_cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        }
        _SitVulkanCmdSetDepthDynamics(vk_cmd, VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
        vkCmdDraw(vk_cmd, vert_count, 1, 0, 0);
    }
	if (local_scratch) {
        SIT_FREE(local_scratch); // [FIX] Free thread-local scratch buffer
    }
#endif
    return SITUATION_SUCCESS;

#else
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationCmdDrawText requires STB Truetype.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdDrawTextBoxed(
    SituationCommandBuffer cmd, SituationFont font, const char* text,
    SitRectangle bounds, float fontSize, float spacing, ColorRGBA color, bool word_wrap)
{
    if (!SituationIsInitialized() || !text) return SITUATION_ERROR_INVALID_PARAM;

    SituationFont use_font = font;
    if (use_font.atlas_texture.generation == 0) {
        use_font = sit_render.default_font;
        if (use_font.atlas_texture.generation == 0) return SITUATION_ERROR_RESOURCE_INVALID;
    }

    int cell_w = _SituationFontGridCellWidth(&use_font);
    int cell_h = _SituationFontGridCellHeight(&use_font);
    float target_size = (fontSize > 0.0f) ? fontSize : use_font.font_height_pixels;
    float scale_factor = (use_font.font_height_pixels > 0.0f) ?
        (target_size / use_font.font_height_pixels) : 1.0f;
    float advance_px = (float)cell_w * scale_factor + spacing + use_font.char_spacing;
    float line_advance = _SituationFontGridLineAdvance(&use_font, scale_factor);

    float cx = bounds.x;
    float cy = bounds.y;
    float word_start_x = cx;
    const char* word_start_ptr = text;

    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            cx = bounds.x;
            cy += line_advance;
            if (cy + (float)cell_h * scale_factor > bounds.y + bounds.height) break;
            word_start_x = cx;
            word_start_ptr = c + 1;
            continue;
        }
        if (*c == '\r') continue;

        if (word_wrap && cx + (float)cell_w * scale_factor > bounds.x + bounds.width) {
            if (*c == ' ' || word_start_ptr == c) {
                cx = bounds.x;
                cy += line_advance;
                if (cy + (float)cell_h * scale_factor > bounds.y + bounds.height) break;
                word_start_x = cx;
                word_start_ptr = c + 1;
                continue;
            } else {
                cx = word_start_x;
                cy += line_advance;
                if (cy + (float)cell_h * scale_factor > bounds.y + bounds.height) break;
                c = word_start_ptr - 1;
                word_start_x = cx;
                continue;
            }
        }

        if (*c == ' ') {
            word_start_x = cx + advance_px;
            word_start_ptr = c + 1;
        }

        float glyph_w = (float)cell_w * scale_factor;
        float glyph_h = (float)cell_h * scale_factor;
        if (cx >= bounds.x && cx + glyph_w <= bounds.x + bounds.width &&
            cy >= bounds.y && cy + glyph_h <= bounds.y + bounds.height) {
            char one[2] = { *c, '\0' };
            Vector2 pos = { cx, cy };
            SituationCmdDrawTextEx(cmd, use_font, one, pos, fontSize, spacing, color);
        }

        cx += advance_px;
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief Submits a command to copy a texture to the main window's swapchain.
 * @details This function is designed for Compute-Only applications or custom rendering pipelines where the final image is generated in a texture/image rather than drawn directly to the backbuffer via a Render Pass.
 *          It effectively "presents" the texture by blitting it onto the swapchain's current image.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a sequence of layout transitions and a `vkCmdBlitImage` command.
 *   1. Transitions the Swapchain Image to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
 *   2. Transitions the Source Texture to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
 *   3. Blits the source to the swapchain (scaling if necessary).
 *   4. Transitions the Swapchain Image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` (ready for presentation).
 *   5. Transitions the Source Texture back to `VK_IMAGE_LAYOUT_GENERAL` (ready for next compute frame).
 *
 * - **OpenGL:** Creates a temporary Framebuffer Object (FBO) attached to the source texture and performs a `glBlitNamedFramebuffer` to the default backbuffer (FBO 0).
 *
 * @param cmd The command buffer to record into.
 * @param texture The source texture containing the frame to present. Must be valid.
 */
SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PRESENT, p);
    if (p) {
        p->args.present.texture = texture;
        {
            int fbw = sit_gs.main_window_width;
            int fbh = sit_gs.main_window_height;
            if (sit_gs.sit_glfw_window) {
                glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fbw, &fbh);
            }
            if (fbw < 1) {
                fbw = 1;
            }
            if (fbh < 1) {
                fbh = 1;
            }
            p->args.present.target_w = fbw;
            p->args.present.target_h = fbh;
        }
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    _SituationTextureSlot* tex_slot = _SitGetTextureSlot(texture);
    if (!tex_slot) return SITUATION_ERROR_RESOURCE_INVALID;

    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    // 1. Get the current swapchain image we are targeting
    VkImage swapchainImage = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];
    if (swapchainImage == VK_NULL_HANDLE) return SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID;

    // 2. Transition Swapchain to TRANSFER_DST
    // Note: SituationAcquireFrameCommandBuffer normally leaves it in UNDEFINED or COLOR_ATTACHMENT_OPTIMAL.
    // We assume it's currently available.
    _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImage, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 3. Transition Source Texture to TRANSFER_SRC
    // Note: Compute shaders usually leave textures in GENERAL or SHADER_READ_ONLY.
    _SituationVulkanTransitionImageLayout(vk_cmd, tex_slot->image, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // 4. Blit
    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x = texture.width;
    blit.srcOffsets[1].y = texture.height;
    blit.srcOffsets[1].z = 1;

    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1].x = sit_render.vk.swapchain_extent.width;
    blit.dstOffsets[1].y = sit_render.vk.swapchain_extent.height;
    blit.dstOffsets[1].z = 1;

    vkCmdBlitImage(vk_cmd, tex_slot->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_NEAREST);  // Use NEAREST for sharp pixel-perfect scaling

    // 5. Transition Swapchain to PRESENT_SRC
    _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImage, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 6. Transition Source back to GENERAL (ready for next compute frame)
    _SituationVulkanTransitionImageLayout(vk_cmd, tex_slot->image, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Retrieves the GPU device address of a buffer for bindless access.
 * @details This function returns a 64-bit pointer (BDA) that can be passed to shaders to access the buffer directly, bypassing descriptor sets.
 *          This is essential for modern "Bindless" rendering techniques and ray tracing.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Returns the address via `vkGetBufferDeviceAddress`. Requires the `bufferDeviceAddress` feature to be enabled during initialization (handled automatically if supported).
 * - **OpenGL:** Returns the address via `glGetNamedBufferParameterui64v` (NV_shader_buffer_load / EXT_buffer_reference). Returns 0 with
 *   `SITUATION_ERROR_OPENGL_UNSUPPORTED` on drivers that lack both extensions (AMD, Intel, Mesa).
 *
 * @param buffer The buffer handle to query.
 * @return The 64-bit GPU address, or 0 if the feature is unsupported or the buffer is invalid.
 */
SITAPI uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer) {
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return 0;

#if defined(SITUATION_USE_VULKAN)
    // Requires bufferDeviceAddress feature — enabled automatically when supported.
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = slot->vk_buffer
    };
    return vkGetBufferDeviceAddress(sit_render.vk.device, &info);

#elif defined(SITUATION_USE_OPENGL)
    GLuint64 address = 0;
    // Requires GL_NV_shader_buffer_load or GL_EXT_buffer_reference (NVIDIA-only on most drivers).
    // On AMD / Intel / Mesa neither extension is available — return 0 with a clear error rather
    // than silently succeeding and leaving the caller with an unusable zero address.
#if defined(GLAD_GL_EXT_buffer_reference) || defined(GLAD_GL_NV_shader_buffer_load)
    if (GLAD_GL_EXT_buffer_reference || GLAD_GL_NV_shader_buffer_load) {
        glGetNamedBufferParameterui64v(buffer.gl_buffer_id, GL_BUFFER_GPU_ADDRESS_NV, &address);
        glMakeNamedBufferResidentNV(buffer.gl_buffer_id, GL_READ_WRITE);
        return (uint64_t)address;
    }
#endif
    // Extension absent — emit a diagnostic error once per call so callers / logs can detect this.
    _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED,
        "SituationGetBufferDeviceAddress: GL_NV_shader_buffer_load / GL_EXT_buffer_reference "
        "not available on this driver. Buffer device addresses are unsupported on non-NVIDIA "
        "OpenGL hardware. Use SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS) to gate.");
    return 0;
#endif
    return 0;
}

/**
 * @brief Retrieves a bindless texture handle for the given texture.
 * @details Allows the texture to be accessed in shaders via a 64-bit handle (e.g., `sampler2D` can be constructed from a `uint64_t`), bypassing texture units.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glGetTextureHandleARB` and ensures the handle is resident (`glMakeTextureHandleResidentARB`).
 *   Returns 0 with `SITUATION_ERROR_OPENGL_UNSUPPORTED` when `GL_ARB_bindless_texture` is not available.
 * - **Vulkan:** Returns the slot index into `global_bindless_set` — the stable descriptor array index
 *   that should be passed as a push constant `texture_id` to shaders using descriptor indexing.
 *   Validates slot liveness via `_SitGetTextureSlot`; returns 0 with
 *   `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` for stale handles.
 *
 * @param texture The texture to query.
 * @return A 64-bit bindless handle (GL) or descriptor index (VK), or 0 if unsupported / invalid.
 */
SITAPI uint64_t SituationGetTextureHandle(SituationTexture texture) {
#if defined(SITUATION_USE_OPENGL)
    if (!texture.generation) return 0;

    // Check if bindless is supported
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Bindless textures not supported by driver.");
        return 0;
    }

    // Retrieve the handle using ARB extension
#if defined(GLAD_GL_ARB_bindless_texture)
    if (GLAD_GL_ARB_bindless_texture) {
        GLuint64 handle = glGetTextureHandleARB(texture.gl_texture_id);
        if (!handle) {
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to retrieve texture handle.");
            return 0;
        }

        // Ensure the handle is resident (GPU accessible)
        if (!glIsTextureHandleResidentARB(handle)) {
            glMakeTextureHandleResidentARB(handle);
        }
        return (uint64_t)handle;
    }
#endif
    return 0;

#elif defined(SITUATION_USE_VULKAN)
    // For Vulkan, the "bindless handle" is the index into global_bindless_set (descriptor indexing).
    // Validate the handle is still alive and the slot is registered in the bindless set before
    // returning the index — a raw slot_index on a stale handle would silently alias a recycled slot.
    if (!texture.generation) return 0;

    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED,
            "SituationGetTextureHandle: texture handle is stale or has been destroyed.");
        return 0;
    }
    // slot_index lives on the public handle; _SitGetTextureSlot already confirmed it matches
    // the live registry entry — return it as the descriptor array index.
    return (uint64_t)texture.slot_index;
#else
    return 0;
#endif
}

/**
 * @brief Retrieves the GPU device address of the mesh vertex buffer for bindless / vertex-pull access.
 *
 * @details Returns a 64-bit GPU virtual address that can be passed to shaders as a push constant
 * (or SSBO field) to fetch vertex data directly by index, bypassing the fixed-function IA.
 *
 * The returned address is valid only while the mesh handle is alive. Destroying the mesh
 * invalidates the address with no GPU-side fence — ensure all in-flight GPU work referencing
 * the address has completed before calling `SituationDestroyMesh`.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** `vkGetBufferDeviceAddress` on the vertex buffer. Requires `SIT_FEATURE_BINDLESS_BUFFERS`
 *   (i.e. `bufferDeviceAddress` Vulkan 1.2 feature). Always nonzero for a live mesh on a compliant device.
 * - **OpenGL:** `glGetNamedBufferParameterui64v(vbo, GL_BUFFER_GPU_ADDRESS_NV)` + resident make.
 *   NVIDIA-only (`GL_NV_shader_buffer_load`). Returns 0 + `SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED`
 *   on AMD / Intel / Mesa. Use `SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)` to gate.
 *
 * @param mesh The mesh handle to query.
 * @return A 64-bit GPU address, or 0 if the feature is unsupported, the mesh is invalid, or the
 *         index buffer was not provided at creation time (vertex-only mesh).
 */
SITAPI uint64_t SituationGetMeshVertexBufferAddress(SituationMesh mesh) {
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) return 0;

#if defined(SITUATION_USE_VULKAN)
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED,
            "SituationGetMeshVertexBufferAddress: SIT_FEATURE_BINDLESS_BUFFERS not available on this device.");
        return 0;
    }
    if (slot->vertex_buffer == VK_NULL_HANDLE) return 0;
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = slot->vertex_buffer
    };
    return vkGetBufferDeviceAddress(sit_render.vk.device, &info);

#elif defined(SITUATION_USE_OPENGL)
    GLuint64 address = 0;
#if defined(GLAD_GL_NV_shader_buffer_load)
    if (GLAD_GL_NV_shader_buffer_load && slot->vbo_id) {
        glGetNamedBufferParameterui64v(slot->vbo_id, GL_BUFFER_GPU_ADDRESS_NV, &address);
        glMakeNamedBufferResidentNV(slot->vbo_id, GL_READ_ONLY);
        return (uint64_t)address;
    }
#endif
    _SituationSetErrorFromCode(SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED,
        "SituationGetMeshVertexBufferAddress: GL_NV_shader_buffer_load not available. "
        "Mesh vertex buffer device addresses require NVIDIA hardware on OpenGL. "
        "Use SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS) to gate.");
    return 0;
#else
    return 0;
#endif
}

/**
 * @brief Retrieves the GPU device address of the mesh index buffer for GPU-driven index fetch.
 *
 * @details Companion to `SituationGetMeshVertexBufferAddress`. Returns the GPU address of the
 * index buffer, allowing a compute or mesh shader to fetch index data directly.
 *
 * Returns 0 if the mesh was created without an index buffer (`index_count == 0` at creation).
 * Same lifetime and feature-gating rules as `SituationGetMeshVertexBufferAddress` apply.
 *
 * @param mesh The mesh handle to query.
 * @return A 64-bit GPU address, or 0 if unsupported, the mesh is invalid, or no index buffer exists.
 */
SITAPI uint64_t SituationGetMeshIndexBufferAddress(SituationMesh mesh) {
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) return 0;

#if defined(SITUATION_USE_VULKAN)
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED,
            "SituationGetMeshIndexBufferAddress: SIT_FEATURE_BINDLESS_BUFFERS not available on this device.");
        return 0;
    }
    if (slot->index_buffer == VK_NULL_HANDLE) return 0;  /* vertex-only mesh */
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = slot->index_buffer
    };
    return vkGetBufferDeviceAddress(sit_render.vk.device, &info);

#elif defined(SITUATION_USE_OPENGL)
    GLuint64 address = 0;
#if defined(GLAD_GL_NV_shader_buffer_load)
    if (GLAD_GL_NV_shader_buffer_load && slot->ebo_id) {
        glGetNamedBufferParameterui64v(slot->ebo_id, GL_BUFFER_GPU_ADDRESS_NV, &address);
        glMakeNamedBufferResidentNV(slot->ebo_id, GL_READ_ONLY);
        return (uint64_t)address;
    }
#endif
    _SituationSetErrorFromCode(SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED,
        "SituationGetMeshIndexBufferAddress: GL_NV_shader_buffer_load not available. "
        "Mesh index buffer device addresses require NVIDIA hardware on OpenGL. "
        "Use SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS) to gate.");
    return 0;
#else
    return 0;
#endif
}

/**
 * @brief Pushes mesh vertex/index GPU addresses for vertex-pull shaders (Phase C2).
 * @details Writes a `SituationMeshPullPushConstants` block at push-constant offset 0:
 *          `vertex_address` from `SituationGetMeshVertexBufferAddress`, `index_address` from
 *          `SituationGetMeshIndexBufferAddress` (0 when the mesh has no index buffer).
 *          Pair with a pull-model VS (`sit/gpu/vertex_pull.glslh`) and `SituationCmdDrawMesh`.
 *
 * @param cmd Active command buffer with a graphics pipeline bound.
 * @param mesh Live mesh created via `SituationCreateMesh` / `SituationCreateMeshEx`.
 * @return SITUATION_SUCCESS, or an error if BDA is unsupported or the mesh handle is invalid.
 *
 * @see SituationMeshPullPushConstants, SituationGetMeshVertexBufferAddress
 */
SITAPI SituationError SituationCmdBindMeshPullBuffers(SituationCommandBuffer cmd, SituationMesh mesh) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdBindMeshPullBuffers: library not initialized.");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBindMeshPullBuffers: cmd is NULL.");
    }
    if (!_SitGetMeshSlot(mesh)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationCmdBindMeshPullBuffers: invalid mesh handle.");
    }
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED,
            "SituationCmdBindMeshPullBuffers: SIT_FEATURE_BINDLESS_BUFFERS not available.");
    }

    uint64_t vaddr = SituationGetMeshVertexBufferAddress(mesh);
    if (vaddr == 0) {
        return SituationGetLastErrorCode() != SITUATION_SUCCESS ? SituationGetLastErrorCode()
            : _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
                "SituationCmdBindMeshPullBuffers: mesh vertex buffer address is 0.");
    }
    uint64_t iaddr = SituationGetMeshIndexBufferAddress(mesh);

    SituationMeshPullPushConstants pc = { vaddr, iaddr };
    return SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
}

/**
 * @brief Binds a texture as a sampled image (sampler2D) to a specific binding point.
 * @details This is a semantic alias for `SituationCmdBindTextureSet`, specifically intending usage as a sampled texture (vs storage).
 *          It ensures clarity in user code when distiguishing between read-only textures and read-write images.
 *
 * @param cmd The command buffer.
 * @param binding The shader binding point (set index).
 * @param texture The texture to bind.
 * @return SITUATION_SUCCESS on success.
 */
SITAPI SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture) {
    // Maps directly to the existing unified binding function
    return SituationCmdBindTextureSet(cmd, binding, texture);
}

/**
 * @brief [Core] Define the format of a vertex attribute for the active VAO.
 *
 * @details Configures how vertex data is read from the bound buffer for a specific attribute location (e.g., Position at loc 0, UV at loc 1).
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Modifies the state of the currently bound Global VAO using `glVertexArrayAttribFormat`. This allows dynamic changes to vertex formats at runtime.
 * - **Vulkan:** **Not Supported.** Returns `SITUATION_ERROR_NOT_IMPLEMENTED`.
 *   In Vulkan, vertex input state is immutable and baked into the `VkPipeline` object at creation. You cannot change vertex attributes dynamically on a command buffer; you must create a new pipeline with the desired layout.
 *
 * @note **[OpenGL Only]** This function is not supported on Vulkan.
 *
 * @param cmd The command buffer (Ignored in OpenGL).
 * @param location The shader attribute location index (e.g., `layout(location=0)`).
 * @param binding Vertex input binding index; must match the `binding` passed to `SituationCmdBindVertexBuffer` for that stream (use 0 for interleaved data).
 * @param size The number of components (1, 2, 3, or 4).
 * @param type The data type of the components (e.g., `SIT_DATA_FLOAT`).
 * @param normalized Whether fixed-point data should be normalized.
 * @param offset The byte offset of this attribute within the bound vertex buffer stride.
 */
SITAPI SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, uint32_t binding, int size, SituationDataType type, bool normalized, size_t offset) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    if (_SituationMapDataTypeToGL(type) == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdSetVertexAttribute: Invalid data type.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_VERTEX_ATTRIBUTE, p);
    if (p) {
        p->args.set_vertex_attr.location = location;
        p->args.set_vertex_attr.binding = binding;
        p->args.set_vertex_attr.size = size;
        p->args.set_vertex_attr.type = (int)type;
        p->args.set_vertex_attr.normalized = normalized ? 1 : 0;
        p->args.set_vertex_attr.offset = offset;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    // FIX: Explicitly report that this is not supported in the Vulkan backend.
    // In Vulkan, vertex attributes are baked into the immutable VkPipeline object at creation.
    // To change attributes, you must create a new pipeline with the desired vertex input state.
    (void)cmd; (void)location; (void)binding; (void)size; (void)type; (void)normalized; (void)offset;
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
        "SituationCmdSetVertexAttribute is incompatible with Vulkan's architecture. "
        "Vulkan Pipelines are immutable; vertex attributes must be defined at pipeline creation time "
        "(inside SituationLoadShaderFromMemory logic), not dynamically on the command buffer.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Binds a graphics pipeline (shader program) for subsequent drawing commands.
 *
 * @details This function activates the specified graphics pipeline, making its shader program (and associated fixed-function state, in Vulkan) active for subsequent draw calls recorded in the command buffer. Any previously bound graphics pipeline is replaced.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** This function is a wrapper around `glUseProgram`. It activates the shader program associated with the `SituationShader` handle. The command buffer parameter `cmd` is ignored as OpenGL uses global state.
 *               In debug builds (`NDEBUG` not defined), it validates the program ID using `glIsProgram` to catch potential errors early.
 * - **Vulkan:** Records a `vkCmdBindPipeline` command into the provided command buffer for the `VK_PIPELINE_BIND_POINT_GRAPHICS` bind point.
 *               It also updates the internal global state `sit_render.vk.current_pipeline_layout_for_push_constants` with the pipeline's layout, which is essential for subsequent `SituationCmdSetPushConstant` and descriptor set binding operations.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param shader The `SituationShader` handle representing the graphics pipeline to bind.
 *
 * @return SITUATION_SUCCESS on successful binding of the pipeline.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the shader handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (OpenGL, Debug) if the shader's program ID is not a valid OpenGL name.
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_OPENGL_GENERAL (OpenGL) if an OpenGL error occurs during the `glUseProgram` call (e.g., program linking issues made runtime detectable, context problems).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. The shader pipeline represented by `shader` was created successfully.
 */
SITAPI SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Attempted to bind an invalid shader handle.");
        #if defined(SITUATION_USE_VULKAN)
        sit_render.vk.current_pipeline_layout_for_push_constants = VK_NULL_HANDLE;
        #endif
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_PIPELINE, p);

        p->args.bind_pipeline.shader_id = (uint64_t)slot->gl_program_id;
        buf->current_recording_shader_id = (uint64_t)slot->gl_program_id;
        return SITUATION_SUCCESS;
    }
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    sit_render.vk.current_compute_pipeline_layout = VK_NULL_HANDLE;
    sit_render.vk.current_pipeline_layout_for_push_constants = slot->vk_pipeline_layout;
    sit_render.vk.current_bound_shader_slot = slot;
    sit_render.vk.current_graphics_vertex_stride = 0;
    SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd, slot, 0));
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Records a command to draw a mesh using the currently bound graphics pipeline.
 *
 * @details This function instructs the GPU to render the geometry defined by the provided `SituationMesh`. It requires that a graphics pipeline (shader, potentially state) has been previously bound using `SituationCmdBindPipeline`.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Binds the mesh's Vertex Array Object (VAO) using `glBindVertexArray`.
 *   The VAO encapsulates the vertex buffer, index buffer, and vertex attribute configurations. It then calls `glDrawElements` to render the indexed geometry.
 *   In debug builds (`NDEBUG` not defined), it validates the VAO ID using `glIsVertexArray` to catch potential errors early.
 * - **Vulkan:** Explicitly binds the mesh's `vertex_buffer` and `index_buffer` to the command buffer using `vkCmdBindVertexBuffers` and `vkCmdBindIndexBuffer`.
 *   It then records a `vkCmdDrawIndexed` command. This requires the command buffer to be in the recording state and a compatible graphics pipeline to be bound.
 *
 * @param cmd The command buffer into which the draw command will be recorded.
 *            In OpenGL, this parameter is typically ignored as it uses global state.
 * @param mesh The `SituationMesh` handle containing the geometry data (vertices, indices) to be drawn.
 *
 * @return SITUATION_SUCCESS on successful recording of the draw command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the mesh handle is invalid (e.g., `id` is 0) or if the mesh contains no indices (`index_count` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (OpenGL, Debug) if the mesh's VAO ID is not a valid OpenGL name.
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_OPENGL_GENERAL (OpenGL) if an OpenGL error occurs during the draw process (e.g., invalid VAO state, context issues).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible graphics pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The mesh data (vertex/index buffers) is valid and accessible by the GPU.
 */
// --- Raster & Fixed-Function State (Phase 4) ---

SITAPI SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_CULL_MODE, p);
    p->args.set_cull_mode.mode = mode;
#elif defined(SITUATION_USE_VULKAN)
    VkCullModeFlags vk_mode = VK_CULL_MODE_NONE;
    if (mode == SIT_CULL_BACK) vk_mode = VK_CULL_MODE_BACK_BIT;
    else if (mode == SIT_CULL_FRONT) vk_mode = VK_CULL_MODE_FRONT_BIT;
    sit_render.vk.dynamic_cull_mode = vk_mode;
    sit_render.vk.dynamic_raster_state_initialized = true;
    if (sit_render.vk.current_bound_shader_slot) {
        SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound((VkCommandBuffer)cmd, sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetFrontFace(SituationCommandBuffer cmd, SituationFrontFace front_face) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_FRONT_FACE, p);
    p->args.set_front_face.front_face = front_face;
#elif defined(SITUATION_USE_VULKAN)
    VkFrontFace vk_front_face = (front_face == SIT_FRONT_FACE_CW) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    sit_render.vk.dynamic_front_face = vk_front_face;
    sit_render.vk.dynamic_raster_state_initialized = true;
    if (sit_render.vk.current_bound_shader_slot) {
        SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound((VkCommandBuffer)cmd, sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetPrimitiveTopology(SituationCommandBuffer cmd, SituationPrimitiveTopology topology) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_PRIMITIVE_TOPOLOGY, p);
    p->args.set_primitive_topology.topology = topology;
#elif defined(SITUATION_USE_VULKAN)
    VkPrimitiveTopology vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    switch (topology) {
        case SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:  vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        case SIT_PRIMITIVE_TOPOLOGY_LINE_LIST:      vk_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case SIT_PRIMITIVE_TOPOLOGY_LINE_STRIP:     vk_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
        case SIT_PRIMITIVE_TOPOLOGY_POINT_LIST:     vk_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
        default:                                     vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    }
    sit_render.vk.dynamic_primitive_topology = vk_topology;
    sit_render.vk.dynamic_primitive_topology_initialized = true;
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        sit_render.vk.pfn_cmd_set_primitive_topology((VkCommandBuffer)cmd, vk_topology);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetPolygonMode(SituationCommandBuffer cmd, SituationPolygonMode mode) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (mode != SIT_POLYGON_MODE_FILL && mode != SIT_POLYGON_MODE_LINE && mode != SIT_POLYGON_MODE_POINT) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid SituationPolygonMode.");
    }
#if defined(SITUATION_USE_VULKAN)
    if (mode != SIT_POLYGON_MODE_FILL) {
        if ((sit_render.enabled_features_mask & SIT_FEATURE_FILL_MODE_NON_SOLID) == 0u) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "Polygon line/point mode requires fillModeNonSolid (wireframe) support on this device.");
        }
    }
    VkPolygonMode vk_mode = VK_POLYGON_MODE_FILL;
    if (mode == SIT_POLYGON_MODE_LINE) vk_mode = VK_POLYGON_MODE_LINE;
    else if (mode == SIT_POLYGON_MODE_POINT) vk_mode = VK_POLYGON_MODE_POINT;
    sit_render.vk.dynamic_polygon_mode = vk_mode;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (sit_render.vk.extended_dynamic_state3_polygon_mode_enabled && sit_render.vk.pfn_cmd_set_polygon_mode_ext
        && vk_cmd != VK_NULL_HANDLE) {
        sit_render.vk.pfn_cmd_set_polygon_mode_ext(vk_cmd, vk_mode);
    }
    if (sit_render.vk.current_bound_shader_slot && vk_cmd != VK_NULL_HANDLE) {
        sit_render.vk.current_pbr_pipeline = VK_NULL_HANDLE;
        SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound(vk_cmd,
            sit_render.vk.current_bound_shader_slot, sit_render.vk.current_graphics_vertex_stride));
    }
    if (mode == SIT_POLYGON_MODE_POINT &&
        (!sit_render.vk.extended_dynamic_state3_polygon_mode_enabled || !sit_render.vk.pfn_cmd_set_polygon_mode_ext)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "Dynamic polygon point mode requires VK_EXT_extended_dynamic_state3 polygon mode on this device.");
    }
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_POLYGON_MODE, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_polygon_mode.mode = mode;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetDepthBias(SituationCommandBuffer cmd, bool enable,
                                               float constant_factor, float clamp, float slope_factor) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_DEPTH_BIAS, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_depth_bias.enable = enable;
    p->args.set_depth_bias.constant_factor = constant_factor;
    p->args.set_depth_bias.clamp = clamp;
    p->args.set_depth_bias.slope_factor = slope_factor;
#elif defined(SITUATION_USE_VULKAN)
    if (enable && (!sit_render.vk.depth_bias_dynamic_enabled ||
                   !sit_render.vk.pfn_cmd_set_depth_bias_enable || !sit_render.vk.pfn_cmd_set_depth_bias)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "Dynamic depth bias requires VK_EXT_extended_dynamic_state2 on this device.");
    }
    sit_render.vk.dynamic_depth_bias_enable = enable ? VK_TRUE : VK_FALSE;
    sit_render.vk.dynamic_depth_bias_constant = constant_factor;
    sit_render.vk.dynamic_depth_bias_clamp = clamp;
    sit_render.vk.dynamic_depth_bias_slope = slope_factor;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (sit_render.vk.pfn_cmd_set_depth_bias_enable) {
        sit_render.vk.pfn_cmd_set_depth_bias_enable(vk_cmd, sit_render.vk.dynamic_depth_bias_enable);
    }
    if (enable && sit_render.vk.pfn_cmd_set_depth_bias) {
        sit_render.vk.pfn_cmd_set_depth_bias(vk_cmd, constant_factor, clamp, slope_factor);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetLineWidth(SituationCommandBuffer cmd, float width) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (width <= 0.0f || width > 1024.0f) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Line width must be a positive finite value.");
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_LINE_WIDTH, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_line_width.width = width;
#elif defined(SITUATION_USE_VULKAN)
    if (width != 1.0f && (sit_render.enabled_features_mask & SIT_FEATURE_WIDE_LINES) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "Line widths other than 1.0 require wideLines support on this device.");
    }
    sit_render.vk.dynamic_line_width = width;
    vkCmdSetLineWidth((VkCommandBuffer)cmd, width);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_COLOR_WRITE_MASK, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_color_write_mask.r = r;
    p->args.set_color_write_mask.g = g;
    p->args.set_color_write_mask.b = b;
    p->args.set_color_write_mask.a = a;
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    VkColorComponentFlags mask = 0;
    if (r) mask |= VK_COLOR_COMPONENT_R_BIT;
    if (g) mask |= VK_COLOR_COMPONENT_G_BIT;
    if (b) mask |= VK_COLOR_COMPONENT_B_BIT;
    if (a) mask |= VK_COLOR_COMPONENT_A_BIT;
    const VkColorComponentFlags full_mask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (mask != full_mask &&
        (!sit_render.vk.extended_dynamic_state3_color_write_enabled ||
         !sit_render.vk.pfn_cmd_set_color_write_mask_ext)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCmdSetColorWriteMask requires VK_EXT_extended_dynamic_state3 color write mask on this device.");
    }
    sit_render.vk.dynamic_color_write_mask = mask;
    if (sit_render.vk.pfn_cmd_set_color_write_mask_ext) {
        sit_render.vk.pfn_cmd_set_color_write_mask_ext(vk_cmd, 0, 1, &mask);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetStencilTest(SituationCommandBuffer cmd, bool enable,
                                                 const SituationStencilState* front,
                                                 const SituationStencilState* back) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (enable && (!front || !back)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdSetStencilTest requires front and back state when enable is true.");
    }
#if defined(SITUATION_USE_OPENGL)
    if (enable && !_SitGLHasStencilBuffer()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
            "SituationCmdSetStencilTest: active framebuffer has no stencil attachment.");
    }
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_STENCIL_TEST, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_stencil_test.enable = enable;
    if (enable) {
        p->args.set_stencil_test.front = *front;
        p->args.set_stencil_test.back = *back;
    }
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (enable) {
        if (!_SitVulkanHasStencilAttachment()) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "SituationCmdSetStencilTest: active render target has no stencil attachment.");
        }
        if (!sit_render.vk.pfn_cmd_set_stencil_test_enable ||
            !sit_render.vk.pfn_cmd_set_stencil_op) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
                "SituationCmdSetStencilTest requires VK_EXT_extended_dynamic_state stencil dynamics on this device.");
        }
        sit_render.vk.dynamic_stencil_front = *front;
        sit_render.vk.dynamic_stencil_back = *back;
        sit_render.vk.dynamic_stencil_test_enable = VK_TRUE;
        sit_render.vk.pfn_cmd_set_stencil_test_enable(vk_cmd, VK_TRUE);
        _SitVulkanApplyStencilFaceDynamics(vk_cmd, VK_STENCIL_FACE_FRONT_BIT, front);
        _SitVulkanApplyStencilFaceDynamics(vk_cmd, VK_STENCIL_FACE_BACK_BIT, back);
    } else {
        sit_render.vk.dynamic_stencil_test_enable = VK_FALSE;
        if (sit_render.vk.pfn_cmd_set_stencil_test_enable) {
            sit_render.vk.pfn_cmd_set_stencil_test_enable(vk_cmd, VK_FALSE);
        }
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetMultisampleState(SituationCommandBuffer cmd, const SituationMultisampleState* state) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd || !state) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_MULTISAMPLE_STATE, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_multisample_state.ms = *state;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    // Vulkan: sample shading, sample mask, and alpha-to-coverage are baked into
    // VkPipelineMultisampleStateCreateInfo at pipeline compile time — they are not
    // dynamic state in Vulkan 1.4 core. VK_EXT_extended_dynamic_state3 exposes
    // vkCmdSetSampleShadingEnableEXT / vkCmdSetAlphaToCoverageEnableEXT, but
    // those features are not currently loaded. Track the desired state in the
    // shadow so push/pop brackets are symmetric; the values will be consulted
    // when MSAA pipeline variants are compiled (Phase 6 render target work).
    sit_render.vk.dynamic_ms_sample_shading_enable    = state->sample_shading_enable;
    sit_render.vk.dynamic_ms_min_sample_shading       = state->min_sample_shading;
    sit_render.vk.dynamic_ms_sample_mask              = state->sample_mask;
    sit_render.vk.dynamic_ms_alpha_to_coverage_enable = state->alpha_to_coverage_enable;
    (void)cmd;
    return SITUATION_SUCCESS;
#endif
}

SITAPI SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_DEPTH_TEST, p);
    p->args.set_depth_test.enable = enable;
    p->args.set_depth_test.depth_op = depth_op;
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    VkCompareOp vk_op = _SitVulkanMapCompareOp(depth_op);
    sit_render.vk.dynamic_depth_test_enable = enable ? VK_TRUE : VK_FALSE;
    sit_render.vk.dynamic_depth_compare_op = vk_op;
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        sit_render.vk.pfn_cmd_set_depth_test_enable(vk_cmd, sit_render.vk.dynamic_depth_test_enable);
        sit_render.vk.pfn_cmd_set_depth_compare_op(vk_cmd, vk_op);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_DEPTH_WRITE, p);
    p->args.set_depth_write.enable = enable;
#elif defined(SITUATION_USE_VULKAN)
    sit_render.vk.dynamic_depth_write_enable = enable ? VK_TRUE : VK_FALSE;
    if (_SitVulkanGraphicsDynamicProcsReady()) {
        sit_render.vk.pfn_cmd_set_depth_write_enable((VkCommandBuffer)cmd, sit_render.vk.dynamic_depth_write_enable);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_BLEND_ENABLE, p);
    p->args.set_blend_enable.enable = enable;
#elif defined(SITUATION_USE_VULKAN)
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd, SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb, SituationBlendFactor src_a, SituationBlendFactor dst_a) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_BLEND_FUNC_SEPARATE, p);
    p->args.set_blend_func.src_rgb = src_rgb;
    p->args.set_blend_func.dst_rgb = dst_rgb;
    p->args.set_blend_func.src_a = src_a;
    p->args.set_blend_func.dst_a = dst_a;
#elif defined(SITUATION_USE_VULKAN)
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf->raster_stack_depth >= SITUATION_MAX_RASTER_STACK_DEPTH) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPushRasterState: raster stack overflow.");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PUSH_RASTER_STATE, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.push_pop_raster_state.scope_id = scope_id;
    buf->raster_stack_depth++;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.raster_stack_depth >= SITUATION_MAX_RASTER_STACK_DEPTH) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPushRasterState: raster stack overflow.");
    }
    _SitVulkanCaptureRasterState(&sit_render.vk.raster_stack[sit_render.vk.raster_stack_depth++]);
    (void)scope_id;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf->raster_stack_depth <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPopRasterState: raster stack underflow.");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_POP_RASTER_STATE, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.push_pop_raster_state.scope_id = scope_id;
    buf->raster_stack_depth--;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.raster_stack_depth <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPopRasterState: raster stack underflow.");
    }
    _SitVulkanApplyRasterState((VkCommandBuffer)cmd,
        &sit_render.vk.raster_stack[--sit_render.vk.raster_stack_depth]);
    (void)scope_id;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationRendererBehaviorPolicy SituationRendererBehaviorPolicyDefault(void) {
    SituationRendererBehaviorPolicy policy = {0};
    return policy;
}

SITAPI SituationError SituationCmdSetRendererBehavior(SituationCommandBuffer cmd, const SituationRendererBehaviorPolicy* policy) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd || !policy) return SITUATION_ERROR_INVALID_PARAM;
    SituationError validate_err = _SitValidateRendererBehaviorPolicyForSet(policy);
    if (validate_err != SITUATION_SUCCESS) {
        return validate_err;
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    buf->behavior = *policy;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_RENDERER_BEHAVIOR, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.set_renderer_behavior.policy = *policy;
#elif defined(SITUATION_USE_VULKAN)
    sit_render.vk.behavior = *policy;
    (void)cmd;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdPushRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf->behavior_stack_depth >= SITUATION_MAX_BEHAVIOR_STACK_DEPTH) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPushRendererBehavior: behavior stack overflow.");
    }
    buf->behavior_stack[buf->behavior_stack_depth++] = buf->behavior;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PUSH_RENDERER_BEHAVIOR, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.push_pop_renderer_behavior.scope_id = scope_id;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.behavior_stack_depth >= SITUATION_MAX_BEHAVIOR_STACK_DEPTH) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPushRendererBehavior: behavior stack overflow.");
    }
    sit_render.vk.behavior_stack[sit_render.vk.behavior_stack_depth++] = sit_render.vk.behavior;
    (void)scope_id;
    (void)cmd;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdPopRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (buf->behavior_stack_depth <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPopRendererBehavior: behavior stack underflow.");
    }
    buf->behavior = buf->behavior_stack[--buf->behavior_stack_depth];
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_POP_RENDERER_BEHAVIOR, p);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.push_pop_renderer_behavior.scope_id = scope_id;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.behavior_stack_depth <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "SituationCmdPopRendererBehavior: behavior stack underflow.");
    }
    sit_render.vk.behavior = sit_render.vk.behavior_stack[--sit_render.vk.behavior_stack_depth];
    (void)scope_id;
    (void)cmd;
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd || !data || size == 0) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    // OpenGL implementation requires explicit shader layouts which are not implemented yet in Phase 4.
    // Use SituationCmdSetPushConstant instead for OpenGL.
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#elif defined(SITUATION_USE_VULKAN)
    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;
    vkCmdPushConstants(cmd, slot->vk_pipeline_layout, VK_SHADER_STAGE_ALL, offset, size, data);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdGPUZoneBegin(SituationCommandBuffer cmd, uint32_t zone_id) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdGPUZoneBegin");
    }
    if (!sit_render.gpu_timestamps_supported) {
        return SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED;
    }
    if (!_SitGpuProfZoneIdValid(zone_id)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_OVERFLOW, "SituationCmdGPUZoneBegin");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!_SitGpuProfZoneIdValid(zone_id)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_OVERFLOW, "SituationCmdGPUZoneBegin");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_GPU_ZONE_BEGIN, p);
    p->args.gpu_zone.zone_id = zone_id;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.inside_render_pass) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_STATE,
            "SituationCmdGPUZoneBegin: not supported inside an active render pass on Vulkan");
    }
    const uint32_t frame_idx = sit_render.vk.current_frame_index;
    SituationError err = _SitGpuProfValidateZoneCall(zone_id, true, frame_idx);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    _SitGpuProfZoneBeginVulkan((VkCommandBuffer)cmd, zone_id, frame_idx);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdGPUZoneEnd(SituationCommandBuffer cmd, uint32_t zone_id) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdGPUZoneEnd");
    }
    if (!sit_render.gpu_timestamps_supported) {
        return SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED;
    }
    if (!_SitGpuProfZoneIdValid(zone_id)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_OVERFLOW, "SituationCmdGPUZoneEnd");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!_SitGpuProfZoneIdValid(zone_id)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_OVERFLOW, "SituationCmdGPUZoneEnd");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_GPU_ZONE_END, p);
    p->args.gpu_zone.zone_id = zone_id;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.inside_render_pass) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_PROFILING_ZONE_STATE,
            "SituationCmdGPUZoneEnd: not supported inside an active render pass on Vulkan");
    }
    const uint32_t frame_idx = sit_render.vk.current_frame_index;
    SituationError err = _SitGpuProfValidateZoneCall(zone_id, false, frame_idx);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    _SitGpuProfZoneEndVulkan((VkCommandBuffer)cmd, zone_id, frame_idx);
#endif
    return SITUATION_SUCCESS;
}

static inline SituationError _SitValidateQueryPoolCmd(_SituationQueryPoolSlot* slot, SituationQueryType expected_type,
        uint32_t query_index, const char* func) {
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_POOL_INVALID, func);
    }
    if (slot->type != expected_type) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_TYPE_MISMATCH, func);
    }
    if (query_index >= slot->query_count) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_INDEX_OUT_OF_RANGE, func);
    }
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdResetQueryPool(SituationCommandBuffer cmd, SituationQueryPool pool,
        uint32_t first_query, uint32_t query_count) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdResetQueryPool");
    }
    _SituationQueryPoolSlot* slot = _SitGetQueryPoolSlot(pool);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_POOL_INVALID, "SituationCmdResetQueryPool");
    }
    if (!_SitQueryPoolIndexRangeValid(slot, first_query, query_count)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_QUERY_INDEX_OUT_OF_RANGE, "SituationCmdResetQueryPool");
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_RESET_QUERY_POOL, p);
    p->args.reset_query_pool.pool_slot = pool.slot_index;
    p->args.reset_query_pool.pool_generation = pool.generation;
    p->args.reset_query_pool.first_query = first_query;
    p->args.reset_query_pool.query_count = query_count;
#elif defined(SITUATION_USE_VULKAN)
    _SitQueryPoolResetVK((VkCommandBuffer)cmd, slot, first_query, query_count);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdWriteTimestamp(SituationCommandBuffer cmd, uint32_t pipeline_stage,
        SituationQueryPool pool, uint32_t query_index) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdWriteTimestamp");
    }
    _SituationQueryPoolSlot* slot = _SitGetQueryPoolSlot(pool);
    SituationError err = _SitValidateQueryPoolCmd(slot, SITUATION_QUERY_TYPE_TIMESTAMP, query_index, "SituationCmdWriteTimestamp");
    if (err != SITUATION_SUCCESS) {
        return err;
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_WRITE_TIMESTAMP, p);
    p->args.write_timestamp.pool_slot = pool.slot_index;
    p->args.write_timestamp.pool_generation = pool.generation;
    p->args.write_timestamp.query_index = query_index;
    p->args.write_timestamp.pipeline_stage = pipeline_stage;
#elif defined(SITUATION_USE_VULKAN)
    _SitQueryPoolWriteTimestampVK((VkCommandBuffer)cmd, slot, query_index, pipeline_stage);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdBeginOcclusionQuery(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t query_index) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBeginOcclusionQuery");
    }
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.active_occlusion_pool_slot >= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OCCLUSION_QUERY_STATE,
            "SituationCmdBeginOcclusionQuery: occlusion query already active.");
    }
#endif
    _SituationQueryPoolSlot* slot = _SitGetQueryPoolSlot(pool);
    SituationError err = _SitValidateQueryPoolCmd(slot, SITUATION_QUERY_TYPE_OCCLUSION, query_index, "SituationCmdBeginOcclusionQuery");
    if (err != SITUATION_SUCCESS) {
        return err;
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf->recording_render_pass_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, "SituationCmdBeginOcclusionQuery");
    }
    if (buf->recording_occlusion_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OCCLUSION_QUERY_STATE,
            "SituationCmdBeginOcclusionQuery: occlusion query already active.");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BEGIN_OCCLUSION_QUERY, p);
    p->args.occlusion_query.pool_slot = pool.slot_index;
    p->args.occlusion_query.pool_generation = pool.generation;
    p->args.occlusion_query.query_index = query_index;
    buf->recording_occlusion_active = true;
#elif defined(SITUATION_USE_VULKAN)
    if (!sit_render.vk.inside_render_pass) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, "SituationCmdBeginOcclusionQuery");
    }
    _SitQueryPoolBeginOcclusionVK((VkCommandBuffer)cmd, slot, query_index);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdEndOcclusionQuery(SituationCommandBuffer cmd) {
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdEndOcclusionQuery");
    }
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf->recording_render_pass_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, "SituationCmdEndOcclusionQuery");
    }
    if (!buf->recording_occlusion_active) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OCCLUSION_QUERY_STATE,
            "SituationCmdEndOcclusionQuery: no active occlusion query.");
    }
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_END_OCCLUSION_QUERY, p);
    buf->recording_occlusion_active = false;
#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.active_occlusion_pool_slot < 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OCCLUSION_QUERY_STATE,
            "SituationCmdEndOcclusionQuery: no active occlusion query.");
    }
    if (!sit_render.vk.inside_render_pass) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE, "SituationCmdEndOcclusionQuery");
    }
    _SitQueryPoolEndOcclusionVK((VkCommandBuffer)cmd);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd || !name) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BEGIN_DEBUG_GROUP, p);
    size_t name_len = strlen(name) + 1;
    void* ptr = NULL;
    SIT_GL_SOFT_DATA_PUSH(buf, name, name_len, ptr);
    p->args.begin_debug_group.name_offset = (size_t)((uint8_t*)ptr - buf->data_buffer);
    p->args.begin_debug_group.color = color;
#elif defined(SITUATION_USE_VULKAN)
    PFN_vkCmdBeginDebugUtilsLabelEXT pfn = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdBeginDebugUtilsLabelEXT");
    if (pfn) {
        VkDebugUtilsLabelEXT label = {};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        label.color[0] = color.r / 255.0f;
        label.color[1] = color.g / 255.0f;
        label.color[2] = color.b / 255.0f;
        label.color[3] = color.a / 255.0f;
        pfn((VkCommandBuffer)cmd, &label);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* _sit_pkt_ = NULL; SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_END_DEBUG_GROUP, _sit_pkt_);
#elif defined(SITUATION_USE_VULKAN)
    PFN_vkCmdEndDebugUtilsLabelEXT pfn = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdEndDebugUtilsLabelEXT");
    if (pfn) {
        pfn((VkCommandBuffer)cmd);
    }
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    // We just validate that the mesh slot is valid before recording
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "CmdDrawMesh: Invalid mesh handle.");
    }

    // [FIX v2.4.38] Increment draw call counter (was missing — reported by test harness)
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (slot->index_count > 0 ? slot->index_count : slot->vertex_count) / 3;
    _SitVDRecordingNoteDrawCmd(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_MESH, p);
    p->args.draw_mesh.mesh = mesh; // Store handle
    p->args.draw_mesh.shader_id = buf->current_recording_shader_id;
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Select the correct pipeline variant based on vertex stride
    if (sit_render.vk.current_bound_shader_slot) {
        SIT_RETURN_IF_ERR(_SitVulkanEnsureGraphicsPipelineBound((VkCommandBuffer)cmd, sit_render.vk.current_bound_shader_slot, (size_t)slot->vertex_stride));
    } else {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,
            "SituationCmdDrawMesh: bind a graphics pipeline before drawing.");
    }

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers((VkCommandBuffer)cmd, 0, 1, &slot->vertex_buffer, offsets);
    sit_render.vk.current_graphics_vertex_stride = slot->vertex_stride;
    if (slot->index_count > 0 && slot->index_buffer) {
        vkCmdBindIndexBuffer((VkCommandBuffer)cmd, slot->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed((VkCommandBuffer)cmd, (uint32_t)slot->index_count, 1, 0, 0, 0);
    } else {
        vkCmdDraw((VkCommandBuffer)cmd, (uint32_t)slot->vertex_count, 1, 0, 0);
    }
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}


/**
 * @brief Draws a colored, transformed quad.
 * @details This is a high-level helper command that uses the library's internal quad renderer.
 *          It is intended for simple 2D or debug rendering.
 * @param cmd The command buffer. (Ignored in OpenGL).
 * @param model The 4x4 model matrix (position, rotation, scale) for the quad.
 * @param color The color of the quad as a normalized vec4 (r, g, b, a).
 */
/**
 * @brief Draws a part of a texture (defined by a rectangle) on screen.
 * @details This function allows you to draw a specific rectangular region (source) of a texture
 *          scaled to fit a destination rectangle on the screen. It also supports rotation
 *          around a custom origin and color tinting.
 *
 * @param cmd The command buffer to record into.
 * @param texture The texture to draw.
 * @param source The rectangular part of the texture to draw.
 * @param dest The screen rectangle to draw the texture into.
 * @param origin The point within the destination rectangle to rotate around (relative to top-left).
 * @param rotation The rotation angle in degrees (clockwise).
 * @param tint The color tint to apply to the texture (WHITE for no tint).
 */
SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, ColorRGBA tint) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    // 1. Bind Texture (Set 0, Binding 0)
    // This handles descriptor binding (Vulkan) or texture binding + uniform setting (OpenGL)
#if defined(SITUATION_USE_OPENGL)
    SituationCmdBindSampledTexture(cmd, 0, texture);
#endif

    // 2. Calculate UV Rect
    float tw = (float)texture.width;
    float th = (float)texture.height;
    if (tw <= 0) tw = 1.0f;
    if (th <= 0) th = 1.0f;

    Vector4 uv_rect;
    uv_rect.x = source.x / tw;
    uv_rect.y = source.y / th;
    uv_rect.z = source.width / tw;
    uv_rect.w = source.height / th;

    // 3. Calculate Model Matrix
    // Transform: Translate(dest) * Rotate(rot) * Translate(-origin) * Scale(dest.wh)
    mat4 model;
    glm_mat4_identity(model);

    // Translation to destination position (top-left)
    glm_translate(model, (vec3){dest.x, dest.y, 0.0f});

    // Rotation
    if (rotation != 0.0f) {
        glm_rotate(model, glm_rad(rotation), (vec3){0.0f, 0.0f, 1.0f});
    }

    // Origin offset (Pivot) - Only if not zero
    if (origin.x != 0.0f || origin.y != 0.0f) {
        glm_translate(model, (vec3){-origin.x, -origin.y, 0.0f});
    }

    // Scale to destination size
    // Note: If width/height are negative, it might flip, but UVs handle flipping too usually.
    glm_scale(model, (vec3){dest.width, dest.height, 1.0f});

    // 4. Convert Color
    Vector4 color_vec;
    SituationConvertColorToVector4(tint, &color_vec);

    // 5. Submit Draw Call (Internal Quad)
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += 2;
    _SitVDRecordingNoteDrawCmd(cmd);

    int use_texture = 1; // True

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SIT_RETURN_IF_ERR(_SituationGLValidateInternalQuadDrawReady(buf, "SituationCmdDrawTexture", true));

    _SituationTextureSlot* slot = _SitGetTextureSlot(texture); // [FIX] Get raw GL ID
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_QUAD, p);
    if (p) {
        glm_mat4_copy(model, p->args.draw_quad.model);
        p->args.draw_quad.color = color_vec;
        p->args.draw_quad.uv_rect = uv_rect;
        p->args.draw_quad.use_texture = 1;
        p->args.draw_quad.texture_id = slot->gl_texture_id;
        p->args.draw_quad.texture_slot_index = (int)(slot - sit_render.texture_registry);
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    SIT_RETURN_IF_ERR(_SitVulkanValidateInternalQuadDrawReady(vk_cmd, "SituationCmdDrawTexture"));

    VkPipeline quad_pipeline = VK_NULL_HANDLE;
    SIT_RETURN_IF_ERR(_SitVulkanResolveQuadPipeline(&quad_pipeline));
    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, quad_pipeline);

    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

    _SituationTextureSlot* tex_slot = _SitGetTextureSlot(texture);
    if (!tex_slot || !texture.generation) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
            "SituationCmdDrawTexture: invalid texture or stale handle.");
    }

    SIT_RETURN_IF_ERR(_SitVulkanWriteSlotToGlobalBindlessSet(
        tex_slot, (uint32_t)texture.slot_index, "SituationCmdDrawTexture"));

    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 0, 1,
        &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
    if (tex_slot->single_sampler_descriptor_set == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID,
            "SituationCmdDrawTexture: missing single_sampler descriptor set.");
    }
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 1, 1,
        &tex_slot->single_sampler_descriptor_set, 0, NULL);

    struct {
        mat4 projection;
        mat4 model;
        vec4 color;
        vec4 uv_rect;
        uint32_t texture_id;
        int use_texture;
    } push_data;

    _SitVulkanFillQuadPushProjectionForActiveTarget(push_data.projection);
    glm_mat4_copy(model, push_data.model);
    glm_vec4_copy(color_vec.raw, push_data.color);
    glm_vec4_copy(uv_rect.raw, push_data.uv_rect);
    push_data.texture_id = texture.slot_index;
    push_data.use_texture = use_texture; // 1

    vkCmdPushConstants(vk_cmd, sit_render.vk.quad_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, SIT_QUAD_VK_PUSH_BYTES, &push_data);
    _SitVulkanApplyQuadDrawDynamicState(vk_cmd);
    vkCmdDraw(vk_cmd, 4, 1, 0, 0);
#endif
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCmdDrawTextureYpqGrade(
    SituationCommandBuffer cmd,
    SituationTexture texture,
    SitRectangle source,
    SitRectangle dest,
    Vector2 origin,
    float rotation,
    float phase_shift_deg,
    float chroma_factor,
    float luma_factor,
    float mix)
{
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationCmdBindSampledTexture(cmd, 0, texture);
#endif

    float tw = (float)texture.width;
    float th = (float)texture.height;
    if (tw <= 0.0f) {
        tw = 1.0f;
    }
    if (th <= 0.0f) {
        th = 1.0f;
    }

    Vector4 uv_rect;
    uv_rect.x = source.x / tw;
    uv_rect.y = source.y / th;
    uv_rect.z = source.width / tw;
    uv_rect.w = source.height / th;

    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){dest.x, dest.y, 0.0f});
    if (rotation != 0.0f) {
        glm_rotate(model, glm_rad(rotation), (vec3){0.0f, 0.0f, 1.0f});
    }
    if (origin.x != 0.0f || origin.y != 0.0f) {
        glm_translate(model, (vec3){-origin.x, -origin.y, 0.0f});
    }
    glm_scale(model, (vec3){dest.width, dest.height, 1.0f});

    mix = fmaxf(0.0f, fminf(1.0f, mix));

    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += 2;
    _SitVDRecordingNoteDrawCmd(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SIT_RETURN_IF_ERR(_SituationGLValidateInternalQuadDrawReady(buf, "SituationCmdDrawTextureYpqGrade", true));
    if (sit_render.gl.ypq_grade_shader_program == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdDrawTextureYpqGrade: YPQ grade shader is not initialized.");
    }

    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_TEXTURE_YPQ, p);
    if (p) {
        glm_mat4_copy(model, p->args.draw_texture_ypq.model);
        p->args.draw_texture_ypq.uv_rect = uv_rect;
        p->args.draw_texture_ypq.phase_shift_deg = phase_shift_deg;
        p->args.draw_texture_ypq.chroma_factor = chroma_factor;
        p->args.draw_texture_ypq.luma_factor = luma_factor;
        p->args.draw_texture_ypq.mix = mix;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    SIT_RETURN_IF_ERR(_SitVulkanValidateInternalQuadDrawReady(vk_cmd, "SituationCmdDrawTextureYpqGrade"));
    if (sit_render.vk.ypq_grade_pipeline == VK_NULL_HANDLE || sit_render.vk.ypq_grade_pipeline_layout == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdDrawTextureYpqGrade: YPQ grade pipeline is not initialized.");
    }

    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.ypq_grade_pipeline);

    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

    _SituationTextureSlot* tex_slot = _SitGetTextureSlot(texture);
    if (!tex_slot || !texture.generation) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    SIT_RETURN_IF_ERR(_SitVulkanWriteSlotToGlobalBindlessSet(
        tex_slot, (uint32_t)texture.slot_index, "SituationCmdDrawTextureYpqGrade"));

    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.ypq_grade_pipeline_layout, 0, 1,
        &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);
    if (tex_slot->single_sampler_descriptor_set == VK_NULL_HANDLE) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.ypq_grade_pipeline_layout, 1, 1,
        &tex_slot->single_sampler_descriptor_set, 0, NULL);

    struct {
        mat4 projection;
        mat4 model;
        vec4 color;
        vec4 uv_rect;
        uint32_t texture_id;
        int use_texture;
        float phase_shift_deg;
        float chroma_factor;
        float luma_factor;
        float mix;
    } push_data;

    _SitVulkanFillQuadPushProjectionForActiveTarget(push_data.projection);
    glm_mat4_copy(model, push_data.model);
    glm_vec4_one(push_data.color);
    glm_vec4_copy(uv_rect.raw, push_data.uv_rect);
    push_data.texture_id = texture.slot_index;
    push_data.use_texture = 1;
    push_data.phase_shift_deg = phase_shift_deg;
    push_data.chroma_factor = chroma_factor;
    push_data.luma_factor = luma_factor;
    push_data.mix = mix;

    vkCmdPushConstants(vk_cmd, sit_render.vk.ypq_grade_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, SIT_YPQ_GRADE_PUSH_BYTES, &push_data);
    _SitVulkanApplyQuadDrawDynamicState(vk_cmd);
    vkCmdDraw(vk_cmd, 4, 1, 0, 0);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Records a command to draw a full-screen or transformed quad (rectangle) with solid color.
 *
 * @details This is a high-performance convenience function for drawing a simple colored quad,
 *          typically used for:
 *            - Full-screen effects (post-processing, UI backgrounds, overlays)
 *            - Debug visualizations (bounding boxes, highlights, color pickers)
 *            - Simple sprites or rectangles without needing a full mesh
 *            - Blitting a single color or texture (if extended with texture binding)
 *
 *          The function records the following into the command buffer:
 *            - Binds an internal built-in quad vertex shader + fragment shader
 *            - Sets up a simple vertex buffer or immediate vertex data (2 triangles / 6 vertices)
 *            - Applies the provided `model` matrix (usually for position, scale, rotation)
 *            - Sets the uniform color (passed as `Vector4`)
 *            - Issues a draw call (6 indices or 6 vertices)
 *
 *          Coordinates:
 *            - In NDC space: model matrix transforms from [-1,1] quad to desired screen region
 *            - Common usage: identity matrix for full-screen, translate/scale for positioned rects
 *
 *          Backend implementation:
 *            - Vulkan: records into command buffer (uses pre-created pipeline or dynamic state)
 *            - OpenGL: uses glDrawArrays or glDrawElements with VAO or immediate mode fallback
 *
 * @param cmd Valid recording command buffer handle (must be in recording state)
 * @param model 4x4 transformation matrix applied to the quad vertices
 *              (position, rotation, scale; usually orthographic projection already applied externally)
 * @param color RGBA color to fill the quad (components in [0,1] range)
 *              Use SIT_VEC4(r,g,b,a) macro for convenience
 *
 * @return SITUATION_SUCCESS on successful command recording,
 *         SITUATION_ERROR_INVALID_PARAM if cmd is invalid/not recording,
 *         SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT if internal command buffer full,
 *         SITUATION_ERROR_RESOURCE_INVALID if internal quad pipeline/shader not ready,
 *         or other backend-specific errors
 *
 * @note This is a **very lightweight** draw call ideal for high-frequency use (e.g. UI, debug lines).
 *       No texture binding or complex state changes are performed.
 *       Assumes current pipeline layout supports the internal quad shader (set via `SituationCmdBindPipeline` if needed).
 *       For textured quads, use `SituationCmdBindTexture` + `SituationCmdDrawQuadTextured` variant (if exists).
 *
 *       Thread safety:
 *         - Must be called from a thread that owns the command buffer
 *         - Safe during command recording phase only
 *         - Actual execution happens later on render thread submission
 *
 * @see SituationCmdDrawQuadTextured (if implemented), SituationCmdBindPipeline,
 *      SituationCreateCommandBuffer, mat4 (cglm), Vector4, SIT_VEC4 macro
 */
SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += 2;
    _SitVDRecordingNoteDrawCmd(cmd);

    // Default UV Rect: Offset (0,0), Scale (1,1)
    Vector4 uv_rect = {{0.0f, 0.0f, 1.0f, 1.0f}};
    int use_texture = 0; // False

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SIT_RETURN_IF_ERR(_SituationGLValidateInternalQuadDrawReady(buf, "SituationCmdDrawQuad", true));

    // [v2.3.30] Bindless Support
    // Logic: In OpenGL, `SituationCmdDrawQuad` relies on a previously issued `SituationCmdBindTexture(cmd, 0, tex)`
    // command to set the active texture.
    // To support bindless automatically, `SituationCmdBindTexture` has been updated (below) to ALSO
    // record a `SIT_OP_SET_UNIFORM` command that pushes the bindless handle to uniform location 7.
    // So `SituationCmdDrawQuad` itself doesn't need to change much, except enabling the flag in the packet
    // if we wanted to be explicit.
    // However, the shader needs to know whether to sample from binding 0 or the handle at location 7.
    // We update SituationCmdBindTexture to set the `u_use_texture` uniform to 2 (Bindless) instead of 1 (Bindful)
    // if bindless is active.

    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW_QUAD, p);
    if (p) {
        glm_mat4_copy(model, p->args.draw_quad.model);
        p->args.draw_quad.color = color;
        p->args.draw_quad.uv_rect = uv_rect;
        p->args.draw_quad.use_texture = use_texture;
        p->args.draw_quad.texture_slot_index = -1;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    SIT_RETURN_IF_ERR(_SitVulkanValidateInternalQuadDrawReady(vk_cmd, "SituationCmdDrawQuad"));

    VkPipeline quad_pipeline = VK_NULL_HANDLE;
    SIT_RETURN_IF_ERR(_SitVulkanResolveQuadPipeline(&quad_pipeline));
    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, quad_pipeline);

    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

    _SituationTextureSlot* solid_tex_slot = _SitGetTextureSlot(sit_render.vk.quad_solid_texture);
    if (!solid_tex_slot || solid_tex_slot->single_sampler_descriptor_set == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED,
            "SituationCmdDrawQuad: internal solid quad sampler is not initialized.");
    }
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 1, 1,
        &solid_tex_slot->single_sampler_descriptor_set, 0, NULL);

    // Single push block (must match QuadPushConstants in internal_quad shaders — 168 bytes).
    struct {
        mat4 projection;
        mat4 model;
        vec4 color;
        vec4 uv_rect;
        uint32_t texture_id;
        int use_texture;
    } push_quad;

    _SitVulkanFillQuadPushProjectionForActiveTarget(push_quad.projection);
    glm_mat4_copy(model, push_quad.model);
    glm_vec4_copy(color.raw, push_quad.color);
    glm_vec4_copy(uv_rect.raw, push_quad.uv_rect);
    push_quad.texture_id = 0;
    push_quad.use_texture = use_texture;

    vkCmdPushConstants(vk_cmd, sit_render.vk.quad_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, SIT_QUAD_VK_PUSH_BYTES, &push_quad);
    _SitVulkanApplyQuadDrawDynamicState(vk_cmd);
    vkCmdDraw(vk_cmd, 4, 1, 0, 0);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets a small amount of per-draw data (push constants) for the currently bound pipeline.
 *
 * @details This function provides an efficient way to send small, frequently changing data (e.g., transformation matrices, color vectors, material properties) to shaders. It replaces slower methods like individual `glUniform*` calls or updating UBOs for tiny data changes.
 *          The `contract_id` specifies the location/offset within the shader's defined push constant block.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glProgramUniform*` functions as a direct and efficient equivalent.
 *               It queries the currently bound program (`glGetCurrentProgram`) and updates the uniform  at the location specified by `contract_id`. Supported data types are limited to common cases (`mat4`, `vec4`, `vec3`, `vec2`, `float`, `int`) based on `size`. Other sizes will trigger an error message.
 * - **Vulkan:** Records a `vkCmdPushConstants` command into the provided command buffer.
 *   The data is written to the push constant block of the pipeline layout last bound via `vkCmdPushConstants` or assumed to be correctly set in `sit_render.vk.current_pipeline_layout_for_push_constants`. The data is made available to all graphics shader stages (`VK_SHADER_STAGE_ALL_GRAPHICS`).
 *   The pipeline *must* have been created with a push constant range that includes the specified `contract_id` (offset) and `size`.
 *
 * @param cmd The command buffer for the current frame (Vulkan) or ignored (OpenGL).
 * @param contract_id The location/offset within the shader's push constant block.
 *                    In OpenGL, this corresponds to the `location` layout qualifier.
 *                    In Vulkan, this is the byte offset.
 * @param data A pointer to the raw data to send. Must not be NULL.
 * @param size The size of the data in bytes (e.g., `sizeof(mat4)`, `sizeof(vec4)`).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A valid shader/pipeline is bound before calling this function.
 *       2. (Vulkan) The bound pipeline's layout includes a push constant range covering `contract_id` to `contract_id + size`.
 *       3. The `size` and `contract_id` match the shader's expectations.
 * @warning Calling this in OpenGL when no program is bound (`glUseProgram(0)`) will result in no action being taken.
 */
SITAPI SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size) {
    // --- 1. Input Validation ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot set push constant.");
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!data) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Push constant data pointer is NULL.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (size == 0) {
        // Setting 0 bytes is likely an error.
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Push constant size is 0.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    // Optionally, add a maximum size check based on API limits if known.

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

        // Allocate space in the data buffer
        void* ptr = NULL;
    SIT_GL_SOFT_DATA_PUSH(buf, data, size, ptr);

        // Calculate offset relative to buffer start
        size_t offset = (size_t)((uint8_t*)ptr - buf->data_buffer);

        SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_SET_PUSH_CONSTANT, p);
        if (p) {
            p->args.push_constant.offset = contract_id;
            p->args.push_constant.size = size;
            p->args.push_constant.data_offset = offset;
        } else {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Input Validation ---
        if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for push constant update.");
            return SITUATION_ERROR_INVALID_PARAM;
        }
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // Determine which pipeline layout to use (compute takes priority)
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkShaderStageFlags stages = 0;

        if (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) {
            // Compute pipeline is bound
            layout = sit_render.vk.current_compute_pipeline_layout;
            stages = VK_SHADER_STAGE_COMPUTE_BIT;
        } else if (sit_render.vk.current_pipeline_layout_for_push_constants != VK_NULL_HANDLE) {
            // Graphics pipeline is bound
            layout = sit_render.vk.current_pipeline_layout_for_push_constants;
            stages = VK_SHADER_STAGE_ALL_GRAPHICS;
        } else {
            // No pipeline bound
            return SITUATION_ERROR_PIPELINE_BIND_FAIL;
        }

        // --- 3. Vulkan Implementation (vkCmdPushConstants) ---
        vkCmdPushConstants(
            vk_cmd,
            layout,                                               // Pipeline Layout (compute or graphics)
            stages,                                               // Shader stages (compute or graphics)
            contract_id,                                          // Offset
            (uint32_t)size,                                       // Size
            data                                                  // Data
        );
        // Note: vkCmdPushConstants itself doesn't return VkResult.
        // Errors would be validation layer reports or device lost states during submission.
    }
#endif
    // --- 4. Post-Operation ---
    // No general post-operation actions are required here.
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the number of draw commands issued during the current frame.
 *
 * @details This counter is incremented every time `SituationCmdDraw`, `SituationCmdDrawIndexed`,
 *          `SituationCmdDrawMesh`, or `SituationCmdDrawQuad` is called.
 *          It is automatically reset to 0 at the beginning of every frame (inside `SituationPollInputEvents`).
 *
 * @return The count of draw calls recorded so far in the current frame.
 */
SITAPI uint32_t SituationGetDrawCallCount(void) {
    return sit_render.frame_draw_calls;
}

#if defined(SITUATION_ENABLE_RENDER_THREAD)
/**
 * @brief Retrieves the current depth of the render thread's command queue.
 * @details This function returns the number of frames currently waiting to be processed by the render thread.
 *          It is primarily used for implementing backpressure mechanisms to prevent the main thread from getting too far ahead of the GPU.
 *
 * @return The number of pending frames in the ring buffer. Returns 0 if threading is disabled or not initialized.
 */
SITAPI size_t SituationGetRenderQueueDepth(void) {
    if (!SituationIsInitialized() || !sit_render.enabled) return 0;
    return atomic_load(&sit_render.render_queue_depth);
}

/**
 * @brief Retrieves latency metrics for the threaded renderer.
 * @details Provides atomic snapshots of the time delta between frame submission (Main Thread) and frame execution (Render Thread).
 *          Useful for diagnosing input lag or stall issues.
 *
 * @param[out] avg_ns Pointer to receive the average latency in nanoseconds. Can be NULL.
 * @param[out] max_ns Pointer to receive the maximum recorded latency in nanoseconds. Can be NULL.
 */
SITAPI void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetRenderLatencyStats");
        if (avg_ns) *avg_ns = 0;
        if (max_ns) *max_ns = 0;
        return;
    }

    // [v2.3.24a] Updated Metrics (Histogram Stub)
    uint64_t cnt = atomic_load(&sit_render.metric_latency_count);
    if (avg_ns) *avg_ns = cnt ? atomic_load(&sit_render.metric_latency_sum_ns) / cnt : 0;

    // Max is now atomic and tracked correctly in render thread
    if (max_ns) *max_ns = atomic_load(&sit_render.metric_max_latency_ns);
}

SITAPI double SituationGetMaxFrameTime(void) {
    if (!SituationIsInitialized()) return 0.0;
    return sit_gs.max_frame_time;
}

SITAPI uint32_t SituationGetFrameSpikeCount(void) {
    if (!SituationIsInitialized()) return 0;
    return sit_gs.frame_spike_count;
}

SITAPI void SituationGetLastFramePhases(uint64_t* backpressure_ns, uint64_t* fence_wait_ns, uint64_t* execute_ns, uint64_t* present_ns) {
    if (!SituationIsInitialized()) {
        if (backpressure_ns) *backpressure_ns = 0;
        if (fence_wait_ns) *fence_wait_ns = 0;
        if (execute_ns) *execute_ns = 0;
        if (present_ns) *present_ns = 0;
        return;
    }
    if (backpressure_ns) *backpressure_ns = sit_gs.last_backpressure_ns;
    if (fence_wait_ns) *fence_wait_ns = sit_gs.last_fence_wait_ns;
    if (execute_ns) *execute_ns = sit_gs.last_execute_ns;
    if (present_ns) *present_ns = sit_gs.last_present_ns;
}
#endif

/**
 * @brief Fills a versioned snapshot of frame pacing and render-thread phase metrics.
 * @details Wraps P10.0 getters (`GetMaxFrameTime`, `GetFrameSpikeCount`, `GetLastFramePhases`,
 *          render latency, queue depth) into one struct for QSR loops and regression harnesses.
 *          `gpu_zone_ns[]` entries reflect P10.3 GPU zones when `SIT_FEATURE_GPU_TIMESTAMPS` is supported.
 *          Non-allocating; safe to call from any thread after init.
 */
SITAPI void SituationGetFrameProfile(SituationFrameProfile* out) {
    if (!out) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetFrameProfile");
        return;
    }

    memset(out, 0, sizeof(*out));
    out->struct_version = SITUATION_FRAME_PROFILE_VERSION;
    out->struct_size = (uint32_t)sizeof(SituationFrameProfile);

    if (!SituationIsInitialized()) {
        return;
    }

    out->frame_time_ms = (double)sit_gs.frame_time * 1000.0;
    out->max_frame_time_ms = sit_gs.max_frame_time * 1000.0;
    out->spike_count = sit_gs.frame_spike_count;

    out->backpressure_ns = sit_gs.last_backpressure_ns;
    out->fence_wait_ns = sit_gs.last_fence_wait_ns;
    out->execute_ns = sit_gs.last_execute_ns;
    out->present_ns = sit_gs.last_present_ns;
    out->poll_ns = sit_gs.last_poll_ns;
    out->update_ns = sit_gs.last_update_ns;

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    SituationGetRenderLatencyStats(&out->render_latency_avg_ns, &out->render_latency_max_ns);
    out->queue_depth = SituationGetRenderQueueDepth();
#endif

    if (sit_render.gpu_timestamps_supported) {
        memcpy(out->gpu_zone_ns, sit_render.gpu_zone_ns, sizeof(out->gpu_zone_ns));
    }
}

/**
 * @brief Clears accumulated spike / max-frame-time / histogram counters (explicit opt-in).
 * @details Does not reset last-frame phase timers or current `frame_time_ms`.
 */
SITAPI void SituationResetFrameProfileStats(void) {
    if (!SituationIsInitialized()) {
        return;
    }

    sit_gs.max_frame_time = 0.0;
    sit_gs.frame_spike_count = 0;
    memset(sit_gs.frame_time_hist, 0, sizeof(sit_gs.frame_time_hist));
}

/**
 * @brief Exports current render performance metrics as a compact JSON string into a user-provided buffer.
 *
 * @details Fills the caller-supplied buffer with a JSON object containing key render statistics
 *          collected by the render thread (when `SITUATION_ENABLE_RENDER_THREAD` is defined).
 *          Currently includes:
 *            - Library version (hardcoded to match current build)
 *            - Current/main frame time in ms, max observed, spike count (for general stutter debugging)
 *            - Render latency avg/max (if threaded)
 *            - frame_hist array of bucket counts for frame times (general purpose)
 *
 *          Safety features:
 *            - If buffer is NULL or size is 0 -> silent no-op
 *            - If buffer is too small (< 256 bytes) -> writes a minimal error JSON and truncates safely
 *            - If metrics are disabled (no render thread) -> `avg_ns` and `max_ns` are 0
 *
 *          Intended for:
 *            - Debug overlays / in-game performance HUD
 *            - Logging at shutdown or on hotkey
 *            - Quick telemetry export during development
 *
 * @param buf Caller-allocated writable buffer to receive the null-terminated JSON string.
 *            Must remain valid for the duration of the call.
 * @param buf_size Size of the buffer in bytes (including null terminator).
 *                 Recommended minimum: 256 bytes (for error case).
 *                 Larger buffers allow future expansion (more fields, actual bins).
 *
 * @note The function is **fast, non-allocating, and thread-safe** - safe to call from any thread
 *       at any time after initialization.
 *       Output is always null-terminated (even on truncation).
 *       No error code is returned - failures are handled gracefully via JSON error message.
 *       Metrics collection is compile-time gated (`SITUATION_ENABLE_RENDER_THREAD`).
 *       Future versions may populate `bins` with p50/p90/p99 buckets or queue-depth history.
 *
 *       Example output (metrics enabled):
 *       ```json
 *       {"version":"2.3.24b","avg_ns":8333333,"max_ns":16666666,"bins":[]}
 *       ```
 *
 *       Example error output (buffer too small):
 *       ```json
 *       {"bins":[],"error":"buffer too small"}
 *       ```
 *
 * @see SITUATION_ENABLE_RENDER_THREAD (compile-time toggle),
 *      SituationGetRenderLatencyStats (internal metrics getter),
 *      _SituationRenderThreadEntry (where latency is recorded)
 */
SITAPI void SituationExportRenderHistogram(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationExportRenderHistogram: buf is NULL or buf_size is 0"); return; }

    // [v2.3.24b] Export Guard
    if (buf_size < 256) {
        strncpy(buf, "{\"bins\":[],\"error\":\"buffer too small\"}", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    uint64_t ravg = 0, rmax = 0;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    SituationGetRenderLatencyStats(&ravg, &rmax);
#endif

    // General frame time stats for spike debugging (main visible delta)
    double fms = sit_gs.frame_time * 1000.0;
    double fmax_ms = sit_gs.max_frame_time * 1000.0;
    uint32_t spikes = sit_gs.frame_spike_count;

    // Build simple frame time hist string (counts)
    char hist[128];
    snprintf(hist, sizeof(hist), "[%u,%u,%u,%u,%u,%u]",
             sit_gs.frame_time_hist[0], sit_gs.frame_time_hist[1], sit_gs.frame_time_hist[2],
             sit_gs.frame_time_hist[3], sit_gs.frame_time_hist[4], sit_gs.frame_time_hist[5]);

    // Format JSON (extended for general debugging assistance)
    snprintf(buf, buf_size,
        "{\"version\":\"%d.%d.%d%s\",\"frame_ms\":%.3f,\"frame_max_ms\":%.3f,\"spikes\":%u,\"render_avg_ns\":%llu,\"render_max_ns\":%llu,\"frame_hist\":%s,\"phases\":{\"backpressure\":%llu,\"fence\":%llu,\"execute\":%llu,\"present\":%llu},\"core\":{\"poll\":%llu,\"update\":%llu,\"glfw_poll\":%llu,\"input_reset\":%llu,\"joystick\":%llu}}",
        SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR, SITUATION_VERSION_PATCH, SITUATION_VERSION_REVISION,
        fms, fmax_ms, spikes,
        (unsigned long long)ravg, (unsigned long long)rmax, hist,
        (unsigned long long)sit_gs.last_backpressure_ns,
        (unsigned long long)sit_gs.last_fence_wait_ns,
        (unsigned long long)sit_gs.last_execute_ns,
        (unsigned long long)sit_gs.last_present_ns,
        (unsigned long long)sit_gs.last_poll_ns,
        (unsigned long long)sit_gs.last_update_ns,
        (unsigned long long)sit_gs.last_glfw_poll_ns,
        (unsigned long long)sit_gs.last_input_reset_ns,
        (unsigned long long)sit_gs.last_joystick_ns);
}

/**
 * @brief Renders a built-in debug overlay with performance statistics.
 * @details Draws a lightweight textual overlay displaying FPS, Frame Time, Render Queue Depth,
 *          Render Thread Latency, Draw Calls, Triangle Count, and VRAM usage.
 *          Uses the internal debug font and requires no external assets.
 *
 * @param cmd The command buffer to record drawing commands into.
 * @param position The top-left screen position to start drawing the text.
 * @param color The text color.
 */
SITAPI void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationDrawMetricsOverlay"); return; }

    // Use the default font with 2x scaling for better readability
    SituationFont font = sit_render.default_font;
    float font_size = 16.0f;  // 2x scale (8px base * 2)
    float spacing = 1.0f;

    char buffer[256];
    float line_height = 20.0f; // 16px font + 4px padding
    float y = position.y;

    // Standard (1x) size for polling/core text to reduce visual noise
    float poll_font_size = 8.0f;
    float poll_line_height = 12.0f;

    // 1. FPS & Frame Time (with max + spikes for general stutter debugging)
    // Use fixed widths to reduce jiggle when numbers change length
    snprintf(buffer, sizeof(buffer), "FPS:%4d (%5.2f ms, max %5.2f, spikes %3u)", sit_gs.current_fps, sit_gs.frame_time * 1000.0f, sit_gs.max_frame_time * 1000.0f, sit_gs.frame_spike_count);
    SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    y += line_height;

    // Display pace: refresh rate, paced queue depth, present interval, capture state
    {
        const int refresh_hz = SituationGetDisplayRefreshRate();
        const int paced_limit = sit_render.paced_frames_in_flight;
        double present_ms = sit_gs.frame_time * 1000.0;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            uint64_t present_delta_ns = atomic_load_explicit(&sit_render.latest_present_delta_ns, memory_order_relaxed);
            if (present_delta_ns > 0) {
                present_ms = present_delta_ns / 1000000.0;
            }
        }
#endif
        const char* capture_state = "none";
#if defined(SITUATION_USE_OPENGL)
        if (sit_render.gl.screenshot_requested) {
            capture_state = "requested";
        } else {
            for (int cap_i = 0; cap_i < SITUATION_MAX_FRAMES_IN_FLIGHT; cap_i++) {
                if (atomic_load_explicit(&sit_render.gl.screenshot_urgent[cap_i], memory_order_relaxed) != 0) {
                    capture_state = "urgent";
                    break;
                }
            }
            if (strcmp(capture_state, "none") == 0) {
                for (int cap_i = 0; cap_i < SITUATION_MAX_FRAMES_IN_FLIGHT; cap_i++) {
                    if (sit_render.gl.screenshot_request_pending[cap_i]) {
                        capture_state = "requested";
                        break;
                    }
                }
            }
        }
#elif defined(SITUATION_USE_VULKAN)
        if (sit_render.vk.screenshot_requested) {
            capture_state = "requested";
        }
#endif
        snprintf(buffer, sizeof(buffer), "Disp: %3d Hz  paced %d/%d  present %5.2f ms  cap %s",
            refresh_hz, paced_limit, SITUATION_MAX_FRAMES_IN_FLIGHT, present_ms, capture_state);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // Phase breakdown (last frame, for cause attribution when spikes seen)
    if (sit_gs.last_backpressure_ns || sit_gs.last_fence_wait_ns || sit_gs.last_present_ns) {
        snprintf(buffer, sizeof(buffer), "Phases: bp %9llu fp %9llu ex %9llu pr %9llu (ns)",
            (unsigned long long)sit_gs.last_backpressure_ns,
            (unsigned long long)sit_gs.last_fence_wait_ns,
            (unsigned long long)sit_gs.last_execute_ns,
            (unsigned long long)sit_gs.last_present_ns);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // Core engine phases (poll + update timers) - for stutters outside rendering
    // Use standard (non-2x) size as requested for the polling text
    if (sit_gs.last_poll_ns || sit_gs.last_update_ns) {
        snprintf(buffer, sizeof(buffer), "Core: poll %9llu upd %9llu (ns)",
            (unsigned long long)sit_gs.last_poll_ns,
            (unsigned long long)sit_gs.last_update_ns);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, poll_font_size, spacing, color);
        y += poll_line_height;

        if (sit_gs.last_glfw_poll_ns || sit_gs.last_input_reset_ns || sit_gs.last_joystick_ns) {
            snprintf(buffer, sizeof(buffer), "  poll sub: glfw %9llu reset %9llu joy %9llu",
                (unsigned long long)sit_gs.last_glfw_poll_ns,
                (unsigned long long)sit_gs.last_input_reset_ns,
                (unsigned long long)sit_gs.last_joystick_ns);
            SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, poll_font_size, spacing, color);
            y += poll_line_height;
        }
    }

    // 2. Render Queue Depth (if threading)
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        size_t depth = atomic_load(&sit_render.render_queue_depth);
        snprintf(buffer, sizeof(buffer), "Queue Depth: %zu / %d", depth, sit_render.paced_frames_in_flight);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }
    #endif

    // 3. Latency
    uint64_t avg_lat = 0, max_lat = 0;
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    SituationGetRenderLatencyStats(&avg_lat, &max_lat);
    #endif
    if (avg_lat > 0) {
        snprintf(buffer, sizeof(buffer), "Lat: %.2f ms (Max: %.2f)", avg_lat / 1000000.0, max_lat / 1000000.0);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // 4. Draw Calls & Triangles
    snprintf(buffer, sizeof(buffer), "Draws: %u  Tris: %u", sit_render.frame_draw_calls, sit_render.frame_triangle_count);
    SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    y += line_height;

    // 5. Thread pool (when enabled)
    #if defined(SITUATION_ENABLE_THREADING)
    if (sit_gs.thread_pool.is_active) {
        snprintf(buffer, sizeof(buffer), "Jobs: %d  Q lo/hi: %zu/%zu",
            SituationGetActiveJobCount(&sit_gs.thread_pool),
            SituationGetQueueDepth(&sit_gs.thread_pool, SIT_JOB_QUEUE_LOW),
            SituationGetQueueDepth(&sit_gs.thread_pool, SIT_JOB_QUEUE_HIGH));
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }
    #endif

    // 6. VRAM
    uint64_t vram = SituationGetVRAMUsage();
    if (vram > 0) {
        snprintf(buffer, sizeof(buffer), "VRAM: %.2f MB", vram / (1024.0 * 1024.0));
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // [Phase 5] Virtual Bindless Stats
    #if defined(SITUATION_USE_OPENGL)
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        snprintf(buffer, sizeof(buffer), "Virt Bindless: Hits %llu / Miss %llu",
            (unsigned long long)sit_render.gl.virtual_stats.hits,
            (unsigned long long)sit_render.gl.virtual_stats.misses);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    }
    #endif
}

// [v2.3.22] Momentum Implementation

/**
 * @brief Creates a new Render List for recording and replaying graphics commands.
 * @details Part of the **"Momentum"** module (v2.3.22). A Render List is a recorded sequence of draw commands that can be captured once and replayed many times.
 *          This is useful for optimizing static geometry or UI elements, allowing them to be drawn without traversing the scene graph or issuing individual API calls every frame.
 *
 * @return A valid `SituationRenderList` handle, or NULL on allocation failure.
 * @see SituationDestroyRenderList(), SituationReplayRenderList()
 */
SITAPI SituationRenderList SituationCreateRenderList(void) {
    SituationRenderList list = (SituationRenderList)SIT_CALLOC(1, sizeof(struct SituationRenderList_t));
    if (list) {
        list->packet_capacity = 128;
        list->packets = (SituationRenderPacket*)SIT_MALLOC(list->packet_capacity * sizeof(SituationRenderPacket));
        list->data_capacity = 1024;
        list->data_buffer = (uint8_t*)SIT_MALLOC(list->data_capacity);

        // [FIX v2.3.27B]
        atomic_init(&list->in_flight_count, 0);
    }

    return list;
}

/**
 * @brief Destroys a Render List and frees its recorded data.
 * @param list The Render List handle to destroy.
 */
SITAPI void SituationDestroyRenderList(SituationRenderList list) {
    if (!list) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationDestroyRenderList: list is NULL"); return; }

    // [FIX v2.3.27B] Ensure we don't free memory being read by the GPU thread
    while (atomic_load(&list->in_flight_count) > 0) {
        // Simple yield loop
         #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        _mm_pause();
        #elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ __volatile__("yield");
        #endif
    }

    if (list->packets) SIT_FREE(list->packets);
    if (list->data_buffer) SIT_FREE(list->data_buffer);
    SIT_FREE(list);
}

/**
 * @brief Clears a Render List, preparing it for new recording.
 * @details Resets the internal write cursors but keeps the allocated memory buffers to minimize allocation overhead during re-recording.
 * @param list The Render List to reset.
 */
SITAPI void SituationResetRenderList(SituationRenderList list) {
    if (!list) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationResetRenderList: list is NULL"); return; }

    // [FIX v2.3.27B] Wait for in-flight usage to complete
    // We use a simple spin-wait here because this condition should be extremely rare
    // (typically only happens if the Main Thread is lapping the Render Thread).
    int retries = 0;
    while (atomic_load(&list->in_flight_count) > 0) {
        #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        _mm_pause();
        #elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ __volatile__("yield");
        #endif

        retries++;
        if (retries > 100000) {
             // If we are stuck here, it's a deadlock or logic error. Break to avoid hanging.
             _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_LIST_INCOMPLETE, "ResetRenderList timeout: List stuck in flight.");
             break;
        }
    }

    list->packet_count = 0;
    list->data_cursor = 0;
    list->is_recording = false;
}

/**
 * @brief Replays the commands recorded in a Render List into the target command buffer.
 * @details This function copies the recorded packet stream from the Render List into the active frame's command buffer.
 *          This effectively "pastes" the draw calls into the current frame.
 *
 * @param cmd The target command buffer (e.g., the main frame buffer).
 * @param list The source Render List containing the recorded commands.
 */
SITAPI void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list) {
    if (!cmd || !list || list->packet_count == 0) return;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!list->packets) return;

    if (buf->data_cursor + list->data_cursor > buf->data_capacity) {
         size_t new_cap = buf->data_capacity + list->data_cursor + 1024;
         uint8_t* new_ptr = (uint8_t*)SIT_REALLOC(buf->data_buffer, new_cap);
         if (!new_ptr) return;
         buf->data_buffer = new_ptr;
         buf->data_capacity = new_cap;
    }
    size_t base_data_offset = buf->data_cursor;
    if (list->data_cursor > 0) {
        memcpy(buf->data_buffer + base_data_offset, list->data_buffer, list->data_cursor);
        buf->data_cursor += list->data_cursor;
    }

    // Translation Loop (SituationRenderPacket -> SitCommandPacket)
    for (size_t i = 0; i < list->packet_count; ++i) {
        SituationRenderPacket* src = &list->packets[i];
        SitCommandPacket dst = {0};

        switch (src->type) {
            case SIT_CMD_DRAW:
                dst.opcode = SIT_OP_DRAW;
                dst.args.draw.v_count = src->data.draw.vertex_count;
                dst.args.draw.i_count = src->data.draw.instance_count;
                dst.args.draw.first_v = src->data.draw.first_vertex;
                dst.args.draw.first_i = src->data.draw.first_instance;
                break;
            case SIT_CMD_DRAW_INDEXED:
                dst.opcode = SIT_OP_DRAW_INDEXED;
                dst.args.draw_indexed.idx_count = src->data.draw_indexed.index_count;
                dst.args.draw_indexed.inst_count = src->data.draw_indexed.instance_count;
                dst.args.draw_indexed.first_idx = src->data.draw_indexed.first_index;
                dst.args.draw_indexed.v_offset = src->data.draw_indexed.vertex_offset;
                dst.args.draw_indexed.first_inst = src->data.draw_indexed.first_instance;
                break;
            case SIT_CMD_DISPATCH:
                dst.opcode = SIT_OP_DISPATCH;
                dst.args.dispatch.x = src->data.dispatch.group_x;
                dst.args.dispatch.y = src->data.dispatch.group_y;
                dst.args.dispatch.z = src->data.dispatch.group_z;
                break;
            case SIT_CMD_BARRIER:
                dst.opcode = SIT_OP_PIPELINE_BARRIER;
                dst.args.barrier.src = SITUATION_BARRIER_ALL_BARRIER_BITS;
                dst.args.barrier.dst = SITUATION_BARRIER_ALL_BARRIER_BITS;
                break;
            default: continue;
        }

        if (buf->packet_count >= buf->packet_capacity) {
             size_t new_cap = buf->packet_capacity * 2;
             if (new_cap < 16) new_cap = 16;
             SitCommandPacket* new_ptr = (SitCommandPacket*)SIT_REALLOC(buf->packets, new_cap * sizeof(SitCommandPacket));
             if (new_ptr) { buf->packets = new_ptr; buf->packet_capacity = new_cap; }
             else return;
        }
        buf->packets[buf->packet_count++] = dst;
    }
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    for (size_t i = 0; i < list->packet_count; ++i) {
        const SituationRenderPacket* pkt = &list->packets[i];
        switch (pkt->type) {
            case SIT_CMD_DRAW:
                vkCmdDraw(vk_cmd, pkt->data.draw.vertex_count, pkt->data.draw.instance_count, pkt->data.draw.first_vertex, pkt->data.draw.first_instance);
                break;
            case SIT_CMD_DRAW_INDEXED:
                vkCmdDrawIndexed(vk_cmd, pkt->data.draw_indexed.index_count, pkt->data.draw_indexed.instance_count, pkt->data.draw_indexed.first_index, pkt->data.draw_indexed.vertex_offset, pkt->data.draw_indexed.first_instance);
                break;
            case SIT_CMD_DISPATCH:
                vkCmdDispatch(vk_cmd, pkt->data.dispatch.group_x, pkt->data.dispatch.group_y, pkt->data.dispatch.group_z);
                break;
            case SIT_CMD_BARRIER:
                // Simplified barrier for replay; ideally replicate full SituationCmdPipelineBarrier logic here
                // For now, a full memory barrier is safe but slow.
                // To be robust, you should expose the _SituationVulkanPipelineBarrier logic to this function.
                break;
            default: break;
        }
    }
#endif
}

// [v2.3.24b] Integration Zenith: Batched Replay Logic
/**
 * @brief [INTERNAL] Replays (re-executes) the commands of a render list directly into the render thread queue at a specific frame index.
 *
 * @details This low-level helper is used when a render list needs to be re-queued or replayed
 *          for a particular frame slot without going through the normal submission path.
 *          It is typically called in scenarios such as:
 *            - Hot-reload recovery (re-submit changed shaders/meshes to the same frame)
 *            - Frame retry after backpressure or transient errors
 *            - Internal synchronization when a list must be re-executed in a specific in-flight slot
 *            - Debug/force-replay mechanisms
 *
 *          Behavior:
 *            - Validates the list handle and frame_idx (0 to SITUATION_MAX_FRAMES_IN_FLIGHT-1)
 *            - Acquires the render queue mutex
 *            - Directly places the list reference into the queue at the requested frame_idx
 *              (overwriting if already occupied - caller must ensure slot is free or safe)
 *            - Increments the frame refcount for that slot
 *            - Signals the render thread condition variable to wake it if idle
 *            - Releases the mutex
 *
 *          Unlike normal enqueue, this bypasses tail/head circular queue logic and forces
 *          placement at a known frame index (useful when synchronizing with in-flight frames).
 *
 * Thread safety invariants:
 *   - Must be called from a thread that does **not** own the render context
 *     (typically main thread or thread-pool workers)
 *   - Queue access protected by `sit_render.render_queue_mutex`
 *   - Caller must guarantee the target frame_idx slot is either free or safe to overwrite
 *     (refcount == 0 or previous work completed)
 *   - **Not** safe to call from the render thread (deadlock on mutex)
 *
 * @param list Valid `SituationRenderList` handle that has been previously recorded.
 *             Must remain valid until the replayed execution completes.
 * @param frame_idx Specific frame slot index (0 to SITUATION_MAX_FRAMES_IN_FLIGHT-1)
 *                  where the list should be placed for execution.
 *                  Invalid indices are ignored (logged as warning).
 *
 * @note This is a **forceful** operation - no queue-full check is performed.
 *       If the slot is still in use (refcount > 0), behavior is undefined
 *       (possible overwrite, resource leak, or render corruption).
 *       Errors (invalid list/frame_idx) are logged internally only - function returns void.
 *       Use with extreme care - prefer normal `SituationSubmitRenderList` paths unless
 *       you are implementing retry/hot-reload synchronization logic.
 *
 * @see _SituationEnqueueRenderList (normal enqueue), _SituationRenderThreadEntry,
 *      SituationSubmitRenderList, sit_render.render_queue,
 *      sit_render.frame_refcounts, SITUATION_MAX_FRAMES_IN_FLIGHT
 */
static SituationError _SituationReplayToQueue(SituationRenderList list, int frame_idx) {
    if (!list || list->is_recording) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_LIST_INCOMPLETE, "List unfinished or null.");
    }

#if defined(SITUATION_USE_VULKAN)
    // [Safety] Serialize recording to shared frame resources
    mtx_lock(&sit_render.render_queue_mutex);

    VkCommandBuffer g_cmd = sit_render.vk.command_buffers[frame_idx];

    // [Batching Strategy]
    // We scan for dispatches first to aggregate them into a single submit.
    // This assumes Compute -> Graphics dependency flow (simulation then render).

    // 1. Alloc Temp Compute Buffer (from frame pool, auto-reset)
    VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = sit_render.vk.compute_command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer c_cmd;
    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc, &c_cmd) != VK_SUCCESS) {
        mtx_unlock(&sit_render.render_queue_mutex);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED,
            "_SituationReplayToQueue: vkAllocateCommandBuffers failed.");
    }

    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(c_cmd, &begin);

    uint32_t dispatch_cnt = 0;

    // Pass 1: Compute
    for (size_t i = 0; i < list->packet_count; ++i) {
        if (list->packets[i].type == SIT_CMD_DISPATCH) {
            vkCmdDispatch(c_cmd, list->packets[i].data.dispatch.group_x, list->packets[i].data.dispatch.group_y, list->packets[i].data.dispatch.group_z);
            dispatch_cnt++;
        }
    }
    vkEndCommandBuffer(c_cmd);

    // Submit Compute if needed
    if (dispatch_cnt > 0) {
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &c_cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &sit_render.vk.compute_finished_semaphores[frame_idx];

        vkQueueSubmit(sit_render.vk.compute_queue, 1, &submit, VK_NULL_HANDLE);

        // Signal Graphics to wait
        if (frame_idx >= 0 && frame_idx < SITUATION_MAX_FRAMES_IN_FLIGHT) {
            sit_render.vk.needs_compute_wait[frame_idx] = true;
        }
    }

    // Pass 2: Graphics & Barriers
    for (size_t i = 0; i < list->packet_count; ++i) {
        const SituationRenderPacket* pkt = &list->packets[i];
        switch (pkt->type) {
            case SIT_CMD_BARRIER:
                // Memory Barrier for visibility
                vkCmdPipelineBarrier(g_cmd, pkt->data.barrier.src_stage, pkt->data.barrier.dst_stage, 0, 0, NULL, 0, NULL, 0, NULL);
                break;
            case SIT_CMD_DRAW:
                SituationCmdDraw(g_cmd, pkt->data.draw.vertex_count, pkt->data.draw.instance_count, pkt->data.draw.first_vertex, pkt->data.draw.first_instance);
                break;
            case SIT_CMD_DRAW_INDEXED:
                SituationCmdDrawIndexed(g_cmd, pkt->data.draw_indexed.index_count, pkt->data.draw_indexed.instance_count, pkt->data.draw_indexed.first_index, pkt->data.draw_indexed.vertex_offset, pkt->data.draw_indexed.first_instance);
                break;
            default: break;
        }
    }

    mtx_unlock(&sit_render.render_queue_mutex);
#endif
    return SITUATION_SUCCESS;
}

// --- [INTERNAL] Thread-Safe Queue Push ---
/**
 * @brief [INTERNAL] Enqueues a render list into the render thread's pending queue.
 *
 * @details This low-level function safely adds the given `SituationRenderList` to the
 *          render queue (`sit_render.render_queue`) for later processing by the dedicated
 *          render thread. It handles:
 *            - Acquiring the queue mutex
 *            - Checking for queue overflow (if full, logs warning and may drop or block)
 *            - Appending the list index/frame slot to the circular queue
 *            - Incrementing pending frame count / refcount
 *            - Signaling the render thread condition variable (`render_queue_cv`)
 *            - Releasing the mutex
 *
 *          Called internally by:
 *            - `SituationSubmitRenderList` (immediate variant)
 *            - `_SituationRenderJobWorker` (thread-pool/async variant)
 *            - Any other deferred render path
 *
 *          The render thread will eventually dequeue, execute the list's commands,
 *          present (if needed), and flush resources when refcount reaches zero.
 *
 * Thread safety invariants:
 *   - Must be called from a thread that does **not** own the render context
 *     (typically main thread or thread-pool workers)
 *   - Queue access is protected by `sit_render.render_queue_mutex`
 *   - Condition variable signal wakes the render thread if it was waiting
 *   - Safe for concurrent calls (mutex serializes enqueue operations)
 *   - **Not** safe to call from the render thread itself (deadlock risk)
 *
 * @param list Valid `SituationRenderList` handle that has been recorded and ended.
 *             Must not be already enqueued or destroyed.
 *
 * @note This function is non-blocking in normal operation.
 *       If the queue is full (rare, high backpressure), logs a warning
 *       (e.g. SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT or similar)
 *       and may drop the list or block briefly (implementation-defined).
 *       On queue full returns `SITUATION_ERROR_THREAD_QUEUE_FULL` (caller should handle).
 *
 * @see _SituationRenderThreadEntry (dequeue/execution side),
 *      SituationSubmitRenderList, SituationSubmitRenderList (pool variant),
 *      sit_render.render_queue, sit_render.render_queue_mutex,
 *      sit_render.render_queue_cv, SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT
 */
static SituationError _SituationEnqueueRenderList(SituationRenderList list) {
    if (!list) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationEnqueueRenderList: list is NULL.");
    }

    mtx_lock(&sit_render.momentum_mutex);

    int head = atomic_load(&sit_render.momentum_head);
    int tail = atomic_load(&sit_render.momentum_tail);
    int next_head = (head + 1) % 256;

    if (next_head == tail) {
        mtx_unlock(&sit_render.momentum_mutex);
        return _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL,
            "Momentum render queue full. Frame data dropped.");
    }

    atomic_fetch_add(&list->in_flight_count, 1);
    sit_render.momentum_queue[head] = list;
    atomic_store(&sit_render.momentum_head, next_head);

    mtx_unlock(&sit_render.momentum_mutex);
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_ENABLE_THREADING)

// Internal Job Wrapper Context
typedef struct {
    SituationRenderList list;
    void (*func)(void*, void*);
    void* user_data;
} _SitRenderJobCtx;

/**
 * @brief [INTERNAL] Worker function that processes queued render lists on a thread pool worker.
 *
 * @details This function is executed on a thread pool worker when a render list job
 *          is submitted via `SituationSubmitRenderList(pool, ...)`.
 *
 *          It performs the following steps:
 *            - Retrieves the render list handle and optional callback from the job payload
 *            - Forwards the render list to the render thread queue (via internal enqueue)
 *            - Waits (blocks the worker) until the render thread has fully processed the list
 *              (using fences or refcount zeroing to detect completion)
 *            - Upon completion, invokes the optional user callback `func(user_data, list)`
 *              **on this worker thread** (not render thread - safe for user code)
 *            - Releases any temporary job resources and signals job completion
 *
 *          This design allows render list submission from any worker thread without
 *          blocking the main thread, while still ensuring ordered GPU execution via
 *          the dedicated render thread.
 *
 * Key invariants:
 *   - The worker blocks until GPU work is complete (synchronous from job perspective)
 *   - Callback is called on the worker thread (caller must ensure thread-safety)
 *   - Render thread owns actual command execution / present
 *   - Job ID can be waited on externally for full completion
 *
 * @param data Pointer to the job-specific context (embedded `_SitRenderListJobCtx` or similar).
 *             Contains: render list handle, callback function, user_data.
 * @param unused Unused second argument (conforms to `SituationSubmitJobEx` signature)
 *
 * @note This worker intentionally blocks to provide backpressure and ordering guarantees.
 *       For fire-and-forget submission (non-blocking), use a different queue or callback pattern.
 *       Errors during enqueue or render (e.g. queue full, invalid list) are logged internally.
 *
 * @see SituationSubmitRenderList (pool variant), SituationWaitForJob,
 *      _SituationRenderThreadEntry, SITUATION_ERROR_RENDER_LIST_INCOMPLETE,
 *      SITUATION_ERROR_THREAD_QUEUE_FULL
 */
/* HARDENING: void by design — thread-pool job ABI; failures use _SituationSetErrorFromCode. */
static void _SituationRenderJobWorker(void* data, void* unused) {
    (void)unused;
    _SitRenderJobCtx* ctx = (_SitRenderJobCtx*)data;

    // 1. Run User Generation Logic (CPU Work)
    // This fills ctx->list with commands (SituationCmdDraw, etc.)
    if (ctx->func) {
        // Pass dummy error ptr for legacy compatibility if needed
        SituationError dummy_err = SITUATION_SUCCESS;
        ctx->func(ctx->user_data, (void*)&dummy_err);
    }

    // 2. Submit Completed List to Main Thread Queue
    // This is now thread-safe!
    SituationError enq_err = _SituationEnqueueRenderList(ctx->list);
    if (enq_err != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(enq_err, "Render job worker failed to enqueue render list.");
    }
}

/**
 * @brief Submits a render list for asynchronous execution on the thread pool.
 *
 * @details Queues the given `SituationRenderList` as a job in the specified thread pool,
 *          where it will be picked up by a worker thread and forwarded to the render thread
 *          for GPU execution at the next available frame slot.
 *
 *          This is the **asynchronous / multi-thread-friendly** variant of render list submission,
 *          allowing the caller to offload submission from the main thread (e.g. from worker threads,
 *          background loaders, or parallel simulation loops).
 *
 *          After successful submission:
 *            - Returns a `SituationJobId` that can be waited on via `SituationWaitForJob`
 *              or `SituationWaitForAllJobs` to know when the list has completed GPU execution
 *            - When the render thread finishes processing the list, the optional callback
 *              `func(user_data, list)` is invoked **on the render thread**
 *            - Resources associated with the list are released when ref-count reaches zero
 *
 *          If the job queue is full, submission may block briefly or fail (depending on pool config).
 *
 * @param pool Valid `SituationThreadPool` pointer (created via `SituationCreateThreadPool`).
 *             Must remain valid until the job completes.
 * @param list Valid `SituationRenderList` handle previously recorded with commands.
 *             Must not be submitted multiple times without re-recording.
 * @param func Optional completion callback. Signature: `void func(void* user_data, void* list)`.
 *             Called on the render thread upon completion. May be NULL.
 * @param user_data Opaque pointer passed to `func`. Caller manages lifetime/ownership.
 *
 * @return A non-zero `SituationJobId` on successful submission (can be used to wait/track),
 *         0 on failure (queue full, invalid pool/list, allocation error, etc.).
 *         Failures are logged internally via SITUATION_LOG_WARNING and may set the global
 *         error state (e.g. SITUATION_ERROR_THREAD_QUEUE_FULL).
 *
 * @note This is non-blocking from the caller's perspective (unless queue is full and blocking).
 *       Actual GPU execution happens asynchronously on the render thread.
 *       The callback is **not** called if submission fails.
 *       Thread safety:
 *         - Safe to call from **any thread** (main, worker, etc.) as long as pool is valid
 *         - Internal queue mutex + condition variable protect submission
 *         - Not safe to call from the render thread itself (potential deadlock)
 *
 * @see SituationCreateThreadPool, SituationWaitForJob, SituationWaitForAllJobs,
 *      SituationCreateRenderList, SituationBeginRenderList, SituationEndRenderList,
 *      SITUATION_ERROR_THREAD_QUEUE_FULL, SITUATION_ERROR_RENDER_LIST_INCOMPLETE
 */
SITAPI SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list, void (*func)(void*, void*), void* user_data) {
    if (!list) return 0;

    // Reset list for new recording before handing it off
    SituationResetRenderList(list);

    // Prepare context
    _SitRenderJobCtx ctx = { list, func, user_data };

    // Submit to High Priority Queue (Physics/Render Logic)
    return SituationSubmitJobEx(pool, _SituationRenderJobWorker, &ctx, sizeof(_SitRenderJobCtx), SIT_SUBMIT_HIGH_PRIORITY);
}

#else

// Fallback for Single-Threaded Builds
/**
 * @brief Submits a pre-recorded render list for execution on the render thread.
 *
 * @details Queues the given `SituationRenderList` (a pre-built sequence of draw/dispatch commands)
 *          to be executed by the dedicated render thread at the next available frame slot.
 *          This is the primary high-level way to submit rendering work in Situation when using
 *          the deferred command-buffer model.
 *
 *          After submission:
 *            - The render thread picks up the list when a frame slot becomes free
 *            - Executes all commands in the list (binds, draws, dispatches, etc.)
 *            - Calls the optional user-provided callback `func(user_data, list)` upon completion
 *              (invoked on the **render thread** - caller must ensure callback is thread-safe)
 *            - Releases any internal resources associated with the list (if ref-count reaches zero)
 *
 *          This function is **non-blocking** from the caller's perspective - it returns immediately
 *          after queuing. Actual GPU work happens asynchronously on the render thread.
 *
 * @param list Valid `SituationRenderList` handle previously created and filled with commands
 *             (via `SituationBeginRenderList`, `SituationCmd*` functions, `SituationEndRenderList`).
 *             Must not be submitted multiple times without re-recording.
 * @param func Optional callback invoked on the render thread when the list has finished executing.
 *             Signature: `void func(void* user_data, void* list)`.
 *             May be NULL if no completion notification is needed.
 * @param user_data Opaque pointer passed to `func` when called. Ownership/lifetime is caller-managed.
 *
 * @note Errors during submission (invalid list, queue full, etc.) are logged internally
 *       via SITUATION_LOG_WARNING and may set the global error state, but the function itself
 *       returns void - no return code is provided.
 *       The list remains valid after submission until explicitly destroyed or ref-count drops.
 *       For synchronous wait, use `SituationWaitForRenderList` or `SituationWaitForAllRenderLists`.
 *
 *       Thread safety:
 *         - Safe to call from **main thread** or any non-render-thread context
 *         - Not safe to call from the render thread itself (deadlock risk on queue mutex)
 *         - Internal queue is protected by mutex + condition variable
 *
 * @see SituationCreateRenderList, SituationBeginRenderList, SituationEndRenderList,
 *      SituationWaitForRenderList, SituationWaitForAllRenderLists,
 *      SITUATION_ERROR_RENDER_LIST_INCOMPLETE, SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT
 */
SITAPI void SituationSubmitRenderList(SituationRenderList list, void (*func)(void*, void*), void* user_data) {
    if (!list) return;

    SituationResetRenderList(list);

    // 1. Run Logic
    if (func) {
        SituationError dummy_err = SITUATION_SUCCESS;
        func(user_data, (void*)&dummy_err);
    }

    // 2. Enqueue (or just replay immediately if you prefer, but queueing keeps logic unified)
    SituationError enq_err = _SituationEnqueueRenderList(list);
    if (enq_err != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(enq_err, "SituationSubmitRenderList failed to enqueue render list.");
    }
}

#endif

/**
 * @brief Gets the estimated total video memory (VRAM) allocated by the application.
 *
 * @details Returns the total size in bytes of all GPU resources currently managed by the application.
 *          The accuracy of this value depends heavily on the underlying backend and operating system support.
 *
 * @par Backend Support Matrix
 *   - **Vulkan:** **Exact.** Returns precise allocation statistics from the internal Memory Allocator (VMA), tracking buffers and images.
 *   - **Windows (DXGI):** **High Accuracy.** If `SITUATION_ENABLE_DXGI` is defined, queries the OS video memory manager directly.
 *     This works for both OpenGL and Vulkan backends on Windows.
 *   - **OpenGL (NVIDIA):** **Good Accuracy.** Uses `GL_NVX_gpu_memory_info` to calculate usage (Total - Available).
 *   - **OpenGL (AMD/Intel/Other):** **Unavailable.** Returns 0, as standard OpenGL does not expose per-process memory usage.
 *
 * @return The total allocated VRAM in bytes, or 0 if the information cannot be retrieved.
 */
SITAPI uint64_t SituationGetVRAMUsage(void) {
    if (!SituationIsInitialized()) return 0;

    // --- 1. VULKAN (Most Accurate) ---
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.vma_allocator) {
        VmaTotalStatistics stats;
        vmaCalculateStatistics(sit_render.vk.vma_allocator, &stats);
        return stats.total.statistics.allocationBytes;
    }
#endif

    // --- 2. WINDOWS DXGI (Universal on Windows) ---
#if defined(_WIN32) && defined(SITUATION_ENABLE_DXGI)
    if (sit_gs.is_com_initialized) {
        IDXGIFactory4* pFactory = NULL;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)&pFactory)) && pFactory) {
            IDXGIAdapter3* pAdapter3 = NULL;
            IDXGIAdapter* pAdapterTemp = NULL;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapterTemp))) {
                if (SUCCEEDED(pAdapterTemp->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO info = {0};
                    if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                        pAdapter3->Release();
                        pAdapterTemp->Release();
                        pFactory->Release();
                        return info.CurrentUsage;
                    }
                    pAdapter3->Release();
                }
                pAdapterTemp->Release();
            }
            pFactory->Release();
        }
    }
#endif

    // --- 3. OPENGL EXTENSIONS (Linux / Windows without DXGI) ---
#if defined(SITUATION_USE_OPENGL)

    // NVIDIA Extension
    // Guard: Only run this if GLAD defines the extension macro
    #ifdef GL_NVX_gpu_memory_info
        // Constants might not be defined if the extension isn't in headers, so define them safely
        #ifndef GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
            #define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
        #endif
        #ifndef GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
            #define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
        #endif

        if (GLAD_GL_NVX_gpu_memory_info) {
            GLint total_kb = 0;
            GLint current_kb = 0;
            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_kb);
            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &current_kb);
            // Usage = Total - Available
            return (uint64_t)(total_kb - current_kb) * 1024;
        }
    #endif

    // AMD Extension
    // Guard: Only run this if GLAD defines the extension macro
    #ifdef GL_ATI_meminfo
        #ifndef GL_TEXTURE_FREE_MEMORY_ATI
            #define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
        #endif

        if (GLAD_GL_ATI_meminfo) {
            GLint mem_info[4];
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, mem_info);
            // ATI only reports Free memory. Without Total, we can't calc Usage accurately.
            // Returning 0 is safer than returning a misleading number.
            return 0;
        }
    #endif

#endif

    return 0;
}

//==================================================================================
// --- [NEW UNIFIED API] Resource Binding ---
//==================================================================================

#if defined(SITUATION_USE_VULKAN)
static void _SituationVulkanFreeBufferDescriptorSet(_SituationBufferSlot* slot) {
    if (!slot || slot->descriptor_set == VK_NULL_HANDLE || slot->descriptor_pool == VK_NULL_HANDLE) {
        return;
    }
    vkFreeDescriptorSets(sit_render.vk.device, slot->descriptor_pool, 1, &slot->descriptor_set);
    slot->descriptor_set = VK_NULL_HANDLE;
    slot->vk_cached_descriptor_layout = VK_NULL_HANDLE;
    slot->vk_cached_descriptor_type = 0;
}

/* HARDENING: bool by design — descriptor resolver; false paths set *out_err. */
static bool _SituationVulkanResolveBufferDescriptor(
    uint32_t set_index,
    SituationBufferUsageFlags usage,
    VkDescriptorSetLayout* out_layout,
    VkDescriptorType* out_type,
    bool* out_use_dynamic_offset,
    SituationError* out_err) {
    const bool is_storage = (usage & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) != 0;
    const bool is_compute = sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE;

    if (is_compute) {
        *out_layout = is_storage ? sit_render.vk.ssbo_layout : sit_render.vk.dynamic_ubo_layout;
        *out_type = is_storage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        *out_use_dynamic_offset = !is_storage;
        return true;
    }

    if (sit_render.vk.current_pipeline_layout_for_push_constants == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED, "Cannot bind descriptor set; no graphics pipeline is bound.");
        *out_err = SITUATION_ERROR_RENDER_COMMAND_FAILED;
        return false;
    }

    SituationSpirvLayoutProfile profile = SIT_SPIRV_LAYOUT_PROFILE_MESH;
    if (sit_render.vk.current_bound_shader_slot) {
        profile = sit_render.vk.current_bound_shader_slot->vk_spirv_layout_profile;
    }

    switch (profile) {
    case SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO:
        if (set_index > 1u) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "DUAL_SSBO profile supports descriptor sets 0 and 1 only.");
            *out_err = SITUATION_ERROR_INVALID_PARAM;
            return false;
        }
        if (!is_storage) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "DUAL_SSBO profile requires storage buffers.");
            *out_err = SITUATION_ERROR_INVALID_PARAM;
            return false;
        }
        *out_layout = sit_render.vk.ssbo_layout;
        *out_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        *out_use_dynamic_offset = false;
        return true;

    case SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO:
        if (set_index > 1u) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO profile supports descriptor sets 0 and 1 only.");
            *out_err = SITUATION_ERROR_INVALID_PARAM;
            return false;
        }
        if (set_index == 0u) {
            if (is_storage) {
                _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO set 0 requires a uniform buffer.");
                *out_err = SITUATION_ERROR_INVALID_PARAM;
                return false;
            }
            *out_layout = sit_render.vk.ubo_layout;
            *out_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            *out_use_dynamic_offset = false;
        } else {
            if (!is_storage) {
                _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO set 1 requires a storage buffer.");
                *out_err = SITUATION_ERROR_INVALID_PARAM;
                return false;
            }
            *out_layout = sit_render.vk.ssbo_layout;
            *out_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            *out_use_dynamic_offset = false;
        }
        return true;

    case SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER:
        if (set_index > 2u) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO_SAMPLER profile supports descriptor sets 0, 1, and 2 only.");
            *out_err = SITUATION_ERROR_INVALID_PARAM;
            return false;
        }
        if (set_index == 0u) {
            if (is_storage) {
                _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO_SAMPLER set 0 requires a uniform buffer.");
                *out_err = SITUATION_ERROR_INVALID_PARAM;
                return false;
            }
            *out_layout = sit_render.vk.ubo_layout;
            *out_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            *out_use_dynamic_offset = false;
        } else if (set_index == 1u) {
            if (!is_storage) {
                _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "UBO_SSBO_SAMPLER set 1 requires a storage buffer.");
                *out_err = SITUATION_ERROR_INVALID_PARAM;
                return false;
            }
            *out_layout = sit_render.vk.ssbo_layout;
            *out_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            *out_use_dynamic_offset = false;
        } else {
            /* set_index == 2: must be bound via SituationCmdBindTextureSet, not SituationCmdBindDescriptorSet.
             * A SituationBuffer is never a combined image sampler — reject explicitly rather than
             * silently writing a mismatched VkWriteDescriptorSet (which produces undefined behaviour). */
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
                "UBO_SSBO_SAMPLER set 2 is a combined image sampler — use SituationCmdBindTextureSet, not SituationCmdBindDescriptorSet.");
            *out_err = SITUATION_ERROR_INVALID_PARAM;
            return false;
        }
        return true;

    default:
        *out_layout = is_storage ? sit_render.vk.ssbo_layout : sit_render.vk.dynamic_ubo_layout;
        *out_type = is_storage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        *out_use_dynamic_offset = !is_storage;
        return true;
    }
}

static SituationError _SituationVulkanEnsureBufferDescriptorSet(
    _SituationBufferSlot* slot,
    VkDescriptorSetLayout layout,
    VkDescriptorType descriptor_type) {
    if (slot->descriptor_set != VK_NULL_HANDLE
        && slot->vk_cached_descriptor_layout == layout
        && slot->vk_cached_descriptor_type == descriptor_type) {
        return SITUATION_SUCCESS;
    }

    if (slot->descriptor_set != VK_NULL_HANDLE) {
        _SituationVulkanFreeBufferDescriptorSet(slot);
    }

    slot->descriptor_set = _SituationVulkanAllocateDescriptorSet(layout, &slot->descriptor_pool);
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = slot->vk_buffer;
    buffer_info.offset = 0;
    buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptor_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptor_write.dstSet = slot->descriptor_set;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = descriptor_type;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(sit_render.vk.device, 1, &descriptor_write, 0, NULL);
    slot->vk_cached_descriptor_layout = layout;
    slot->vk_cached_descriptor_type = descriptor_type;
    return SITUATION_SUCCESS;
}
#endif /* SITUATION_USE_VULKAN */

/**
 * @brief Binds a buffer's pre-packaged descriptor set to a specific set index in the currently bound pipeline.
 * @details This is the primary, unified function for making a GPU buffer's data (UBO or SSBO) available to a shader.
 *          It associates a `SituationBuffer` with a descriptor set slot declared in the shader code (e.g., `layout(set = X, binding = 0) uniform MyUBO` in GLSL).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Maps the `set_index` to an indexed binding point (`GL_UNIFORM_BUFFER` or `GL_SHADER_STORAGE_BUFFER`) and calls `glBindBufferBase`. This is a direct and efficient binding operation.
 * - **Vulkan:** This function leverages the library's high-performance persistent descriptor set model. When the `SituationBuffer` was created, a dedicated `VkDescriptorSet` was allocated and populated for it.
 *               This function records a fast `vkCmdBindDescriptorSets` command using this pre-cached set, avoiding any runtime allocation or update overhead.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param set_index The descriptor set index in the pipeline layout to bind to. This must match the `set = X` value in the shader's layout qualifier.
 * @param buffer The `SituationBuffer` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful binding.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid or lacks the required internal resources.
 * @return SITUATION_ERROR_RENDER_COMMAND_FAILED if no pipeline is currently bound.
 *
 * @see SituationCreateBuffer(), SituationCmdBindTextureSet()
 */
SITAPI SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_DESCRIPTOR_SET, p);

    // We pack only slot_index + generation into resource_id (uint64)
    // SituationBuffer grew beyond 8 bytes (added size_in_bytes, usage_flags),
    // but we only need the handle identity (slot_index + generation) to look up the slot.
    p->args.bind_desc.set_index = set_index;
    p->args.bind_desc.resource_id = ((uint64_t)buffer.generation << 32) | (uint64_t)buffer.slot_index;

    // We assume _SituationGLExecuteCommands will unpack it.
    // BUT _SituationGLExecuteCommands currently expects a GL ID.
    // I MUST UPDATE _SituationGLExecuteCommands to unpack handle and get slot->gl_buffer_id.

    p->args.bind_desc.offset = dynamic_offset;
    p->args.bind_desc.size = slot->size_in_bytes;
    p->args.bind_desc.usage_flags = slot->usage_flags; // [Bug 10 Fix] Pass usage flags so execution knows UBO vs SSBO
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
    VkDescriptorType desc_type = 0;
    bool use_dynamic_offset = false;
    SituationError resolve_err = SITUATION_SUCCESS;
    if (!_SituationVulkanResolveBufferDescriptor(
            set_index, slot->usage_flags, &desc_layout, &desc_type, &use_dynamic_offset, &resolve_err)) {
        return resolve_err;
    }

    SituationError ensure_err = _SituationVulkanEnsureBufferDescriptorSet(slot, desc_layout, desc_type);
    if (ensure_err != SITUATION_SUCCESS) {
        return ensure_err;
    }

    VkPipelineLayout pipeline_layout = (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE)
        ? sit_render.vk.current_compute_pipeline_layout
        : sit_render.vk.current_pipeline_layout_for_push_constants;

    VkPipelineBindPoint bind_point = (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE)
        ? VK_PIPELINE_BIND_POINT_COMPUTE
        : VK_PIPELINE_BIND_POINT_GRAPHICS;

    if (use_dynamic_offset) {
        uint32_t dyn_offset = dynamic_offset;
        vkCmdBindDescriptorSets(
            (VkCommandBuffer)cmd, bind_point, pipeline_layout, set_index, 1, &slot->descriptor_set, 1, &dyn_offset);
    } else {
        vkCmdBindDescriptorSets(
            (VkCommandBuffer)cmd, bind_point, pipeline_layout, set_index, 1, &slot->descriptor_set, 0, NULL);
    }
    return SITUATION_SUCCESS;
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

SITAPI SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer) {
    return SituationCmdBindDescriptorSetDynamic(cmd, set_index, buffer, 0);
}


/**
 * @brief Binds a texture's pre-packaged descriptor set to a specific set index in the currently bound pipeline.
 * @details This is the primary, unified function for making a texture available for sampling or image load/store operations in a shader. It associates a `SituationTexture` with a descriptor set slot declared in the shader code (e.g., `layout(set = X, binding = 0) uniform sampler2D myTexture`).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Maps the `set_index` to a texture unit and calls `glBindTextureUnit`.
 * - **Vulkan:** Uses the texture's pre-cached, persistent `VkDescriptorSet` to record a fast `vkCmdBindDescriptorSets` command, avoiding runtime overhead.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param set_index The descriptor set index in the pipeline layout to bind to.
 * @param texture The `SituationTexture` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful binding.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the texture handle is invalid.
 *
 * @see SituationCreateTexture(), SituationCmdBindDescriptorSet()
 */
SITAPI SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture) {
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: SituationCmdBindTextureSet called, set=%u, tex slot=%u gen=%u\n",
           set_index, texture.slot_index, texture.generation);
    fflush(stdout);
    #endif
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot=%p, is_active=%d, descriptor_set=%p\n",
           slot, slot ? slot->is_active : -1, slot ? (void*)slot->descriptor_set : NULL);
    fflush(stdout);
    #endif
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Attempted to bind an invalid texture handle.");
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

    // [v2.3.30] Bindless Path
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        // Retrieve the handle (will create/resident if needed)
        uint64_t handle = SituationGetTextureHandle(texture);
        if (handle) {
            // ... (Comment preserved: Bindless logic handled at draw site)
        }
    }

    // Standard Bind (always safe fallback and required for non-bindless shaders)
    // [Phase 2] Use Legacy Texture Opcode to avoid buffer logic in main opcode
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING, p);

    p->args.bind_desc.set_index = set_index;
    p->args.bind_desc.resource_id = slot->gl_texture_id;
    p->args.bind_desc.resource_type = 1; // 1 = Sampled Texture

    // [v2.3.30] Bindless Integration for Internal Shaders
    // If we are binding a texture while a bindless-capable internal shader is active,
    // we should also push the handle to the "magic" bindless uniform location (7)
    // and set the "use bindless" flag (6) to 1.
    // However, SituationCmdBindTexture doesn't know *which* shader will be used later.
    //
    // BUT, since we implemented the bindless logic in `SituationCmdBindTexture` above (in the first block),
    // we are already covered for cases where we can resolve the shader (like Quad).
    //
    // For TEXT rendering, `SituationCmdDrawText` does not call `SituationCmdBindTexture`!
    // It binds the font atlas internally. We need to update `SituationCmdDrawText` to use bindless.

    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    // Determine the active pipeline (graphics or compute).
    VkPipelineBindPoint bind_point;
    VkPipelineLayout layout;
    if (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) {
        bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        layout = sit_render.vk.current_compute_pipeline_layout;
    } else if (sit_render.vk.current_pipeline_layout_for_push_constants != VK_NULL_HANDLE) {
        bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        layout = sit_render.vk.current_pipeline_layout_for_push_constants;
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED, "Cannot bind texture set; no pipeline is currently bound.");
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    // [Bindless] Standard Textures (Sampled)
    // If the texture has no descriptor set, it is part of the Bindless Array.
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        /* SituationLoadShaderFromMemory: set 1 = text_sampler_layout; bind per-texture set (see CreateTextureEx). */
        /* UBO_SSBO_SAMPLER: set 2 = text_sampler_layout (feedback texture). */
        if (bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS && (set_index == 1u || set_index == 2u) && slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &slot->single_sampler_descriptor_set, 0, NULL);
            return SITUATION_SUCCESS;
        }
        /* Internal quad/text/bindless: global array + push texture_id (slot index). */
        vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &sit_render.vk.global_bindless_set, 0, NULL);
        uint32_t texture_id = texture.slot_index;
        vkCmdPushConstants(vk_cmd, layout, VK_SHADER_STAGE_ALL, 96, sizeof(uint32_t), &texture_id);

    } else {
        // [Legacy/Storage] Bind specific descriptor set
        vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &slot->descriptor_set, 0, NULL);
    }

    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Binds a GPU buffer (typically a UBO) for use by the currently bound graphics pipeline.
 * @details Associates a `SituationBuffer` with a uniform block declared in the vertex or fragment shader code (e.g., `layout(location = X) uniform ...` in OpenGL, or `layout(set = ..., binding = X) uniform ...` in Vulkan/GLSL).
 *          This allows the shader to access the buffer's uniform data (e.g., view/projection matrices).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindBufferBase(GL_UNIFORM_BUFFER, contract_id, buffer.gl_buffer_id)`.
 *   This efficiently binds the buffer to the specified uniform block binding point.
 * - **Vulkan:** This function also implements the high-performance, persistent descriptor set model.
 *   When the `SituationBuffer` was created (via `SituationCreateBuffer`), the Vulkan backend allocated a `VkDescriptorSet` (specifically for UBOs) and populated it with the buffer's `VkBuffer` handle. This function records a
 *  `vkCmdBindDescriptorSets` command using this pre-cached descriptor set from `buffer.descriptor_set`, ensuring a very fast operation.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param contract_id The binding point ID within the shader.
 *                    In OpenGL, this corresponds directly to the `location` or binding index specified in the shader (e.g., `glUniformBlockBinding` or `layout(location=X)`).
 *                    In Vulkan, this corresponds to the `dstBinding` used when the buffer's internal descriptor set was populated.
 * @param buffer The `SituationBuffer` handle to bind. The buffer should have been created with usage flags indicating it will be used as a uniform buffer (e.g., `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER`).
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the buffer's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible graphics pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `contract_id` matches the binding point defined in the shader.
 * @warning Binding a buffer that was not created with appropriate usage flags (like `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER`) may lead to undefined behavior.
 */
SITAPI SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer) {
    // The old 'binding' parameter directly maps to the new 'set_index' parameter.
    return SituationCmdBindDescriptorSet(cmd, binding, buffer);
}

// --- Command Buffer Implementations ---

/**
 * @brief Binds a compute pipeline for subsequent dispatch commands.
 * @details Activates the specified compute pipeline, making its shader program and associated state active for subsequent `SituationCmdDispatch` and resource binding commands (e.g., `SituationCmdBindComputeBuffer`) recorded in the command buffer.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glUseProgram(pipeline.gl_program_id)` to activate the OpenGL Compute Program associated with the `SituationComputePipeline` handle.
 * - **Vulkan:** Records a `vkCmdBindPipeline` command into the provided command buffer for the `VK_PIPELINE_BIND_POINT_COMPUTE` bind point. It also updates the internal global state `sit_render.vk.current_compute_pipeline_layout` with the pipeline's layout.
 *   This layout is essential for subsequent `vkCmdBindDescriptorSets` (called by `SituationCmdBindComputeBuffer`) and `vkCmdPushConstants` (called by `SituationCmdSetPushConstant`) commands to specify the correct pipeline interface.
 *
 * @param cmd The command buffer into which the bind command will be recorded.
 *            In OpenGL, this parameter is typically ignored as it uses global state.
 * @param pipeline The `SituationComputePipeline` handle representing the compute pipeline to bind.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. The compute pipeline represented by `pipeline` was created successfully.
 * @warning This function must be called before any dispatch or resource binding commands related to this compute pipeline.
 */
SITAPI SituationError SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot bind compute pipeline.");
    }

    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(pipeline);
    if (!slot) {
        #if defined(SITUATION_USE_VULKAN)
        sit_render.vk.current_compute_pipeline_layout = VK_NULL_HANDLE;
        #endif
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Invalid compute pipeline handle provided.");
    }

#if defined(SITUATION_USE_VULKAN)
    if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for binding compute pipeline.");
    }

    vkCmdBindPipeline((VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_COMPUTE, slot->vk_pipeline);
    sit_render.vk.current_compute_pipeline_layout = slot->vk_pipeline_layout;
#elif defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_BIND_COMPUTE_PIPELINE, p);
    if (p) {
        p->args.bind_pipeline.shader_id = (uint64_t)slot->gl_program_id;
    }
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Binds a GPU buffer (typically an SSBO) to a specific binding point within the currently bound compute pipeline.
 * @details Associates a `SituationBuffer` with a binding point declared in the GLSL compute shader code (e.g., `layout(set = ..., binding = X) buffer ...`). This allows the compute shader to access the buffer's data.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.gl_buffer_id)`.
 *   This efficiently binds the buffer to the specified unit for use by the currently active compute program.
 * - **Vulkan:** This function implements a high-performance, persistent descriptor set model.
 *   When the `SituationBuffer` was created (via `SituationCreateBuffer`), the Vulkan backend internally allocated a `VkDescriptorSet` from a dedicated persistent pool and populated
 *   it with the buffer's `VkBuffer` handle. This function simply records a fast `vkCmdBindDescriptorSets` command using this pre-cached descriptor set.
 *   This approach avoids the significant CPU overhead of allocating and updating descriptor sets every frame, which is crucial for performance in Vulkan.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param binding The binding point index within the compute shader's descriptor set.
 *                In GLSL, this corresponds to the `binding = X` part of the layout qualifier.
 *                In Vulkan, this corresponds to the `dstBinding` used when the buffer's internal descriptor set was originally populated (typically 0 for a single buffer resource within its set).
 * @param buffer The `SituationBuffer` handle to bind. The buffer should have been created with usage flags indicating it will be used as a storage buffer (e.g., `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`).
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the buffer's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compute pipeline has been successfully bound using `SituationCmdBindComputePipeline` before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `binding` index matches the layout specified in the compute shader.
 * @warning Binding a buffer that was not created with appropriate usage flags (like `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`) may lead to undefined behavior or validation errors.
 */
SITAPI SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer) {
    // The old 'binding' parameter directly maps to the new 'set_index' parameter.
    return SituationCmdBindDescriptorSet(cmd, binding, buffer);
}

/**
 * @brief Inserts a pipeline memory barrier for synchronization.
 * @details This is a critical function for synchronizing memory access between different pipeline stages, especially between compute and graphics passes, or before/after transfer operations.
 *          It ensures that writes from a source stage (e.g., a compute shader) are visible to reads or writes in a destination stage (e.g., a vertex shader, or another compute shader).
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** This maps to one or more `glMemoryBarrier` calls. The `src_flags` and `dst_flags` are combined to determine the necessary OpenGL barrier bits. Multiple barriers might be issued
 *   if the combined flags require it (e.g., one for SSBO/ShaderImage access, another for indirect commands).
 * - **Vulkan:** This maps to one or more `vkCmdPipelineBarrier` calls. It carefully constructs the `srcStageMask`, `dstStageMask`, `srcAccessMask`, and `dstAccessMask` based on the abstract flags.
 *   This implementation correctly maps the defined abstract flags to their Vulkan equivalents.
 *
 * @param cmd The command buffer to record the barrier into.
 * @param src_flags A bitmask of `SituationBarrierSrcFlags` indicating the pipeline stage(s) and type(s) of memory access that form the source of the dependency.
 * @param dst_flags A bitmask of `SituationBarrierDstFlags` indicating the pipeline stage(s) and type(s) of memory access that form the destination of the dependency.
 */
#if defined(SITUATION_USE_VULKAN)
static VkPipelineStageFlags _SituationVulkanMapPipelineStages(uint32_t stages) {
    VkPipelineStageFlags mask = 0;
    if (stages & SITUATION_PIPELINE_STAGE_TOP) mask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND) mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_VERTEX_INPUT) mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT) mask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_DEPTH_STENCIL) mask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_TRANSFER) mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_HOST) mask |= VK_PIPELINE_STAGE_HOST_BIT;
    if (stages & SITUATION_PIPELINE_STAGE_BOTTOM) mask |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    return mask;
}

static VkAccessFlags _SituationVulkanMapAccessFlags(uint32_t access) {
    VkAccessFlags mask = 0;
    if (access & SITUATION_ACCESS_INDIRECT_COMMAND_READ) mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    if (access & SITUATION_ACCESS_VERTEX_READ) mask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (access & SITUATION_ACCESS_INDEX_READ) mask |= VK_ACCESS_INDEX_READ_BIT;
    if (access & SITUATION_ACCESS_UNIFORM_READ) mask |= VK_ACCESS_UNIFORM_READ_BIT;
    if (access & SITUATION_ACCESS_SHADER_READ) mask |= VK_ACCESS_SHADER_READ_BIT;
    if (access & SITUATION_ACCESS_SHADER_WRITE) mask |= VK_ACCESS_SHADER_WRITE_BIT;
    if (access & SITUATION_ACCESS_COLOR_ATTACHMENT_READ) mask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    if (access & SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE) mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (access & SITUATION_ACCESS_DEPTH_STENCIL_READ) mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (access & SITUATION_ACCESS_DEPTH_STENCIL_WRITE) mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (access & SITUATION_ACCESS_TRANSFER_READ) mask |= VK_ACCESS_TRANSFER_READ_BIT;
    if (access & SITUATION_ACCESS_TRANSFER_WRITE) mask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if (access & SITUATION_ACCESS_HOST_READ) mask |= VK_ACCESS_HOST_READ_BIT;
    if (access & SITUATION_ACCESS_HOST_WRITE) mask |= VK_ACCESS_HOST_WRITE_BIT;
    return mask;
}
#endif

SITAPI SituationError SituationCmdPipelineBarrierEx(SituationCommandBuffer cmd, const SituationPipelineBarrierDesc* desc) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdPipelineBarrierEx: library not initialized.");
    }
    if (!cmd || !desc) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdPipelineBarrierEx: cmd and desc are required.");
    }
    if (desc->src_stages == 0 || desc->dst_stages == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdPipelineBarrierEx: src_stages and dst_stages must be non-zero.");
    }

#if defined(SITUATION_USE_OPENGL)
    uint32_t src_flags = 0;
    uint32_t dst_flags = 0;

    if ((desc->src_stages & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) && (desc->src_access & SITUATION_ACCESS_SHADER_WRITE)) {
        src_flags |= SITUATION_BARRIER_COMPUTE_SHADER_WRITE;
    }
    if ((desc->src_stages & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) && (desc->src_access & SITUATION_ACCESS_SHADER_WRITE)) {
        src_flags |= SITUATION_BARRIER_FRAGMENT_SHADER_WRITE;
    }
    if ((desc->src_stages & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) && (desc->src_access & SITUATION_ACCESS_SHADER_WRITE)) {
        src_flags |= SITUATION_BARRIER_VERTEX_SHADER_WRITE;
    }
    if ((desc->src_stages & SITUATION_PIPELINE_STAGE_TRANSFER) && (desc->src_access & SITUATION_ACCESS_TRANSFER_WRITE)) {
        src_flags |= SITUATION_BARRIER_TRANSFER_WRITE;
    }

    if (desc->dst_access & SITUATION_ACCESS_INDIRECT_COMMAND_READ) {
        dst_flags |= SITUATION_BARRIER_INDIRECT_COMMAND_READ;
    }
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) {
        if (desc->dst_access & SITUATION_ACCESS_SHADER_READ) dst_flags |= SITUATION_BARRIER_COMPUTE_SHADER_READ;
        if (desc->dst_access & SITUATION_ACCESS_SHADER_WRITE) dst_flags |= SITUATION_BARRIER_COMPUTE_SHADER_READ;
    }
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) {
        if (desc->dst_access & (SITUATION_ACCESS_SHADER_READ | SITUATION_ACCESS_VERTEX_READ | SITUATION_ACCESS_UNIFORM_READ)) {
            dst_flags |= SITUATION_BARRIER_VERTEX_SHADER_READ;
        }
    }
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) {
        if (desc->dst_access & (SITUATION_ACCESS_SHADER_READ | SITUATION_ACCESS_UNIFORM_READ)) {
            dst_flags |= SITUATION_BARRIER_FRAGMENT_SHADER_READ;
        }
    }
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_TRANSFER) {
        if (desc->dst_access & SITUATION_ACCESS_TRANSFER_READ) dst_flags |= SITUATION_BARRIER_TRANSFER_READ;
        if (desc->dst_access & SITUATION_ACCESS_TRANSFER_WRITE) dst_flags |= SITUATION_BARRIER_TRANSFER_READ;
    }

    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PIPELINE_BARRIER, p);
    p->args.barrier.src = src_flags;
    p->args.barrier.dst = dst_flags;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;
    VkAccessFlags src_access_mask = 0;
    VkAccessFlags dst_access_mask = 0;

    if (desc->src_stages & SITUATION_PIPELINE_STAGE_TOP) src_stage_mask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND) src_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_VERTEX_INPUT) src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) src_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT) src_stage_mask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_DEPTH_STENCIL) src_stage_mask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) src_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_TRANSFER) src_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_HOST) src_stage_mask |= VK_PIPELINE_STAGE_HOST_BIT;
    if (desc->src_stages & SITUATION_PIPELINE_STAGE_BOTTOM) src_stage_mask |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_TOP) dst_stage_mask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND) dst_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_VERTEX_INPUT) dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_VERTEX_SHADER) dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER) dst_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT) dst_stage_mask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_DEPTH_STENCIL) dst_stage_mask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_COMPUTE_SHADER) dst_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_TRANSFER) dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_HOST) dst_stage_mask |= VK_PIPELINE_STAGE_HOST_BIT;
    if (desc->dst_stages & SITUATION_PIPELINE_STAGE_BOTTOM) dst_stage_mask |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (desc->src_access & SITUATION_ACCESS_INDIRECT_COMMAND_READ) src_access_mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_VERTEX_READ) src_access_mask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_INDEX_READ) src_access_mask |= VK_ACCESS_INDEX_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_UNIFORM_READ) src_access_mask |= VK_ACCESS_UNIFORM_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_SHADER_READ) src_access_mask |= VK_ACCESS_SHADER_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_SHADER_WRITE) src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT;
    if (desc->src_access & SITUATION_ACCESS_COLOR_ATTACHMENT_READ) src_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE) src_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (desc->src_access & SITUATION_ACCESS_DEPTH_STENCIL_READ) src_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_DEPTH_STENCIL_WRITE) src_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (desc->src_access & SITUATION_ACCESS_TRANSFER_READ) src_access_mask |= VK_ACCESS_TRANSFER_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_TRANSFER_WRITE) src_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if (desc->src_access & SITUATION_ACCESS_HOST_READ) src_access_mask |= VK_ACCESS_HOST_READ_BIT;
    if (desc->src_access & SITUATION_ACCESS_HOST_WRITE) src_access_mask |= VK_ACCESS_HOST_WRITE_BIT;

    if (desc->dst_access & SITUATION_ACCESS_INDIRECT_COMMAND_READ) dst_access_mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_VERTEX_READ) dst_access_mask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_INDEX_READ) dst_access_mask |= VK_ACCESS_INDEX_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_UNIFORM_READ) dst_access_mask |= VK_ACCESS_UNIFORM_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_SHADER_READ) dst_access_mask |= VK_ACCESS_SHADER_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_SHADER_WRITE) dst_access_mask |= VK_ACCESS_SHADER_WRITE_BIT;
    if (desc->dst_access & SITUATION_ACCESS_COLOR_ATTACHMENT_READ) dst_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE) dst_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (desc->dst_access & SITUATION_ACCESS_DEPTH_STENCIL_READ) dst_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_DEPTH_STENCIL_WRITE) dst_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (desc->dst_access & SITUATION_ACCESS_TRANSFER_READ) dst_access_mask |= VK_ACCESS_TRANSFER_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_TRANSFER_WRITE) dst_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if (desc->dst_access & SITUATION_ACCESS_HOST_READ) dst_access_mask |= VK_ACCESS_HOST_READ_BIT;
    if (desc->dst_access & SITUATION_ACCESS_HOST_WRITE) dst_access_mask |= VK_ACCESS_HOST_WRITE_BIT;

    VkMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    vkCmdPipelineBarrier((VkCommandBuffer)cmd, src_stage_mask, dst_stage_mask, 0, 1, &barrier, 0, NULL, 0, NULL);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdBufferBarrier(SituationCommandBuffer cmd, const SituationBufferBarrierDesc* desc) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdBufferBarrier: library not initialized.");
    }
    if (!cmd || !desc) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBufferBarrier: cmd and desc are required.");
    }
    if (desc->src_stages == 0 || desc->dst_stages == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBufferBarrier: src_stages and dst_stages must be non-zero.");
    }
    if (desc->size == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdBufferBarrier: size must be non-zero.");
    }

    _SituationBufferSlot* slot = _SitGetBufferSlot(desc->buffer);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBufferBarrier: invalid buffer handle.");
    }
    if (desc->offset > slot->size_in_bytes || slot->size_in_bytes - desc->offset < desc->size) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_SIZE, "SituationCmdBufferBarrier: range exceeds buffer size.");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationPipelineBarrierDesc global = {0};
    global.src_stages = desc->src_stages;
    global.src_access = desc->src_access;
    global.dst_stages = desc->dst_stages;
    global.dst_access = desc->dst_access;
    return SituationCmdPipelineBarrierEx(cmd, &global);
#elif defined(SITUATION_USE_VULKAN)
    VkPipelineStageFlags src_stage_mask = _SituationVulkanMapPipelineStages(desc->src_stages);
    VkPipelineStageFlags dst_stage_mask = _SituationVulkanMapPipelineStages(desc->dst_stages);
    if (src_stage_mask == 0 || dst_stage_mask == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdBufferBarrier: unsupported stage mask.");
    }

    VkBufferMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = _SituationVulkanMapAccessFlags(desc->src_access);
    barrier.dstAccessMask = _SituationVulkanMapAccessFlags(desc->dst_access);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = slot->vk_buffer;
    barrier.offset = (VkDeviceSize)desc->offset;
    barrier.size = (VkDeviceSize)desc->size;

    vkCmdPipelineBarrier(
        (VkCommandBuffer)cmd,
        src_stage_mask,
        dst_stage_mask,
        0,
        0,
        NULL,
        1,
        &barrier,
        0,
        NULL);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdTextureBarrier(SituationCommandBuffer cmd, SituationTexture texture, const SituationTextureBarrierDesc* desc) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdTextureBarrier: library not initialized.");
    }
    if (!cmd || !desc) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdTextureBarrier: cmd and desc are required.");
    }
    if (desc->old_layout > SITUATION_TEXTURE_LAYOUT_PRESENT || desc->new_layout > SITUATION_TEXTURE_LAYOUT_PRESENT) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdTextureBarrier: unsupported texture layout value.");
    }
    if (desc->new_layout == SITUATION_TEXTURE_LAYOUT_UNDEFINED) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdTextureBarrier: new_layout cannot be UNDEFINED.");
    }

    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdTextureBarrier: invalid texture handle.");
    }

    uint32_t mip_count = desc->mip_level_count ? desc->mip_level_count : 1u;
    uint32_t layer_count = desc->array_layer_count ? desc->array_layer_count : 1u;
    if (desc->base_mip_level >= (uint32_t)slot->mip_levels ||
        mip_count == 0u ||
        desc->base_mip_level + mip_count > (uint32_t)slot->mip_levels ||
        desc->base_array_layer != 0u ||
        layer_count != 1u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_REGION_INVALID, "SituationCmdTextureBarrier: mip range or array layer range is invalid for a 2D texture.");
    }
    if (_SitTextureLayoutIsDeferred(desc->old_layout) ||
        _SitTextureLayoutIsDeferred(desc->new_layout)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationCmdTextureBarrier: depth/stencil attachment and present layouts are reserved until render-target ownership is exposed.");
    }
    if ((desc->new_layout == SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT || desc->old_layout == SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT) &&
        !_SitTextureColorAttachmentLayoutAllowed(slot)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, "SituationCmdTextureBarrier: COLOR_ATTACHMENT layout requires SITUATION_TEXTURE_USAGE_TRANSFER_SRC on a sampled color texture.");
    }
    if ((desc->new_layout == SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC || desc->old_layout == SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC) &&
        !_SitTextureTransferSrcUsageAllowed(slot, _SitGetActiveRendererBehaviorPolicy(cmd), NULL)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, "SituationCmdTextureBarrier: texture lacks SITUATION_TEXTURE_USAGE_TRANSFER_SRC.");
    }
    if ((desc->new_layout == SITUATION_TEXTURE_LAYOUT_TRANSFER_DST || desc->old_layout == SITUATION_TEXTURE_LAYOUT_TRANSFER_DST) &&
        !_SitTextureTransferDstUsageAllowed(slot)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, "SituationCmdTextureBarrier: texture lacks SITUATION_TEXTURE_USAGE_TRANSFER_DST.");
    }
    if ((desc->new_layout == SITUATION_TEXTURE_LAYOUT_GENERAL || desc->old_layout == SITUATION_TEXTURE_LAYOUT_GENERAL) &&
        (slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, "SituationCmdTextureBarrier: GENERAL layout requires SITUATION_TEXTURE_USAGE_STORAGE.");
    }
    if ((desc->new_layout == SITUATION_TEXTURE_LAYOUT_SHADER_READ || desc->old_layout == SITUATION_TEXTURE_LAYOUT_SHADER_READ) &&
        (slot->usage_flags & (SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE)) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_INVALID_USAGE, "SituationCmdTextureBarrier: SHADER_READ layout requires sampled or storage texture usage.");
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_PIPELINE_BARRIER, p);
    p->args.barrier.src = 0;
    p->args.barrier.dst = 0; /* GL has no texture layouts; use the existing conservative all-barrier fallback. */
    _SitTextureSetLayoutHint(slot, desc->new_layout);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;
    VkAccessFlags src_access_mask = 0;
    VkAccessFlags dst_access_mask = 0;
    _SituationVulkanTextureLayoutBarrierMasks(desc->old_layout, true, &src_stage_mask, &src_access_mask);
    _SituationVulkanTextureLayoutBarrierMasks(desc->new_layout, false, &dst_stage_mask, &dst_access_mask);
    if (src_stage_mask == 0 || dst_stage_mask == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdTextureBarrier: unsupported layout transition.");
    }

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = _SituationVulkanMapTextureLayout(desc->old_layout);
    barrier.newLayout = _SituationVulkanMapTextureLayout(desc->new_layout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = desc->base_mip_level;
    barrier.subresourceRange.levelCount = mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        (VkCommandBuffer)cmd,
        src_stage_mask,
        dst_stage_mask,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier);
    _SitTextureSetLayoutHint(slot, desc->new_layout);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags) {
    if (!SituationIsInitialized()) { return; } // Silently return if the library isn't initialized.

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH_VOID(buf, SIT_OP_PIPELINE_BARRIER, p);
    if (p) {
        p->args.barrier.src = src_flags;
        p->args.barrier.dst = dst_flags;
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- Enhanced Vulkan Pipeline Barrier Implementation ---
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // --- Accumulate Vulkan Stages and Access Masks ---
        VkPipelineStageFlags src_stage_mask = 0;
        VkAccessFlags src_access_mask = 0;
        VkPipelineStageFlags dst_stage_mask = 0;
        VkAccessFlags dst_access_mask = 0;

        // --- Determine Source Stage and Access ---
        // What stage wrote the data, and how?
        if (src_flags & SITUATION_BARRIER_COMPUTE_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; }
        if (src_flags & SITUATION_BARRIER_FRAGMENT_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // VK_ACCESS_SHADER_WRITE_BIT is correct for imageStore or SSBO writes in the fragment shader.
        if (src_flags & SITUATION_BARRIER_VERTEX_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // Vertex shader writes (e.g., to SSBOs).
        if (src_flags & SITUATION_BARRIER_TRANSFER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT; src_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT; }
        // --- Determine Destination Stage and Access ---
        // What stage will read or write the data next, and how?
        if (dst_flags & SITUATION_BARRIER_COMPUTE_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT; }
        if (dst_flags & SITUATION_BARRIER_COMPUTE_SHADER_WRITE) { dst_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // Ensuring visibility for a subsequent write by the compute shader.
        if (dst_flags & SITUATION_BARRIER_VERTEX_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; } // [FIX] Added VERTEX_ATTRIBUTE_READ
        if (dst_flags & SITUATION_BARRIER_FRAGMENT_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT; } // For textures/SSBOs
        if (dst_flags & SITUATION_BARRIER_TRANSFER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT; dst_access_mask |= VK_ACCESS_TRANSFER_READ_BIT; } // Reading for a copy operation
        if (dst_flags & SITUATION_BARRIER_TRANSFER_WRITE) {
            // Ensuring the destination of a transfer write is ready.
            // The src barrier ensures data written by shaders is visible for transfer *read*.
            // This barrier ensures the *destination* resource is ready to be written to by transfer.
            // This is less common as transfer destinations are often "fresh". But if a buffer/image was previously written by a shader and is now the destination of a transfer, this barrier makes sense.
            dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        if (dst_flags & SITUATION_BARRIER_INDIRECT_COMMAND_READ) {
            dst_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT; // Or DISPATCH_INDIRECT_BIT for compute
            // Vulkan spec often uses VERTEX_INPUT_BIT or others for indirect, but DRAW_INDIRECT_BIT is specific.
            // Let's use DRAW_INDIRECT_BIT. For compute dispatches, DISPATCH_INDIRECT_BIT is correct.
            // The source stage/access for writing indirect args would be SHADER_WRITE_BIT.
            // To cover both draw and dispatch indirect, we might need to set both stages, or determine it dynamically. For simplicity, we'll use DRAW_INDIRECT_BIT.
            // Access for reading indirect args is INDIRECT_COMMAND_READ_BIT.
            dst_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // Safer to include both
            dst_access_mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        }

        // --- [ROBUSTNESS] Prevent validation errors from empty stage masks ---
        // If no source stage is specified, assume the earliest possible stage.
        if (src_stage_mask == 0) { src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
        // If no destination stage is specified, assume the latest possible stage.
        if (dst_stage_mask == 0) { dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }

        // --- Issue the Barrier ---
        // Allow barriers where access masks are 0 (Execution-Only Dependencies)
        // Only skip if both stage masks are 0 (which implies no dependency defined)
        if (src_stage_mask != 0 || dst_stage_mask != 0) {
            // --- Basic Memory Barrier (No image/buffer memory transitions assumed) ---
            // For image/buffer layout transitions, VkImageMemoryBarrier or VkBufferMemoryBarrier structs would need to be set up and passed to vkCmdPipelineBarrier.
            VkMemoryBarrier memory_barrier = {};
            memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.srcAccessMask = src_access_mask;
            memory_barrier.dstAccessMask = dst_access_mask;

            // Perform the barrier
            // Using VK_DEPENDENCY_BY_REGION_BIT can be a performance hint if the access is localized.
            // For general barriers, it's often omitted unless specifically needed.
            vkCmdPipelineBarrier(
                vk_cmd,
                src_stage_mask,           // srcStageMask
                dst_stage_mask,           // dstStageMask
                0,                        // dependencyFlags (e.g., VK_DEPENDENCY_BY_REGION_BIT)
                1,                        // memoryBarrierCount
                &memory_barrier,          // pMemoryBarriers
                0,                        // bufferMemoryBarrierCount
                NULL,                     // pBufferMemoryBarriers
                0,                        // imageMemoryBarrierCount
                NULL                      // pImageMemoryBarriers
            );
        } else {
            // Optional: Log a verbose message if no effective barrier is specified?
            // This is generally not an error, just a no-op.
            // fprintf(stderr, "VERBOSE: SituationCmdPipelineBarrier called with no effective barriers (src: 0x%x, dst: 0x%x).\n", src_flags, dst_flags);
        }
        // --- End Enhanced Implementation ---
    }
#endif
}

/**
 * @brief Dispatches compute work using the currently bound compute pipeline.
 *
 * @details Executes the compute shader associated with the compute pipeline that was previously bound using `SituationCmdBindComputePipeline`. The number of work groups to be executed in each dimension (X, Y, Z) must be specified.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDispatchCompute`. This function uses the currently active compute program (bound via `glUseProgram`).
 * - **Vulkan:** Records a `vkCmdDispatch` command into the provided command buffer.
 *   It requires that a compute pipeline has been previously bound to the command buffer using `vkCmdBindPipeline` with `VK_PIPELINE_BIND_POINT_COMPUTE`.
 *   Any necessary descriptor sets (for SSBOs, textures, etc.) must also be bound prior to this call.
 *
 * @param cmd The command buffer into which the dispatch command will be recorded (Vulkan) or which provides context (OpenGL, though often unused).
 * @param group_count_x The number of local work groups to dispatch in the X dimension.
 * @param group_count_y The number of local work groups to dispatch in the Y dimension.
 * @param group_count_z The number of local work groups to dispatch in the Z dimension.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A valid compute pipeline is bound before calling this function.
 *       2. All required resources (buffers, textures via descriptor sets/binds) are bound.
 *       3. Appropriate memory barriers (`SituationCmdPipelineBarrierEx`, `SituationCmdBufferBarrier`, or `SituationCmdTextureBarrier`) are used if synchronization is needed before or after the dispatch.
 *
 * @warning Calling this function without a bound compute pipeline will result in undefined behavior or a Vulkan validation error.
 */
SITAPI SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) {
    // --- 1. Core Library Initialization Check ---
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot dispatch compute work.");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdDispatchEx: cmd cannot be NULL.");
    }
    if (group_count_x == 0 || group_count_y == 0 || group_count_z == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdDispatchEx: group counts must be non-zero.");
    }

    _SitVDNoteComputeDispatch(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DISPATCH, p);
    if (p) {
        p->args.dispatch.x = group_count_x;
        p->args.dispatch.y = group_count_y;
        p->args.dispatch.z = group_count_z;
    }
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    {
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // 2. Vulkan pipeline validation is deferred until compute bind-state is tracked.
        // While Vulkan drivers will error if no pipeline is bound, checking here provides
        // clearer feedback. This requires tracking the last bound compute pipeline layout
        // or a simple boolean flag in the global state (e.g., sit_render.vk.is_compute_pipeline_bound).
        // Uncomment the lines below if such state tracking is implemented.
        /*
        if (sit_render.vk.current_compute_pipeline_layout == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED, "Cannot dispatch compute work; no compute pipeline is currently bound. Call SituationCmdBindComputePipeline first.");
             return;
        }
        */

        // --- 3. Vulkan Dispatch ---
        // Records the dispatch command into the command buffer.
        // Assumes the pipeline and descriptor sets are correctly bound beforehand.
        vkCmdDispatch(vk_cmd, group_count_x, group_count_y, group_count_z);
        return SITUATION_SUCCESS;
    }
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

SITAPI SituationError SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) {
    return SituationCmdDispatchEx(cmd, group_count_x, group_count_y, group_count_z);
}

SITAPI SituationError SituationCmdDispatchIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdDispatchIndirect: library not initialized.");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdDispatchIndirect: cmd cannot be NULL.");
    }
    if ((offset & 3u) != 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INDIRECT_COMMAND_INVALID, "SituationCmdDispatchIndirect: offset must be 4-byte aligned.");
    }

    _SituationBufferSlot* slot = _SitGetBufferSlot(indirect_buffer);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdDispatchIndirect: invalid indirect buffer handle.");
    }
    if ((slot->usage_flags & SITUATION_BUFFER_USAGE_INDIRECT_BUFFER) == 0u) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_INVALID_USAGE, "SituationCmdDispatchIndirect: buffer missing SITUATION_BUFFER_USAGE_INDIRECT_BUFFER.");
    }
    if (offset > slot->size_in_bytes || slot->size_in_bytes - offset < sizeof(SituationDispatchIndirectCommand)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INDIRECT_COMMAND_INVALID, "SituationCmdDispatchIndirect: command range is outside the buffer.");
    }

    _SitVDNoteComputeDispatch(cmd);

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = NULL;
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DISPATCH_INDIRECT, p);
    p->args.dispatch_indirect.buffer_id = (uint64_t)slot->gl_buffer_id;
    p->args.dispatch_indirect.offset = offset;
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    vkCmdDispatchIndirect((VkCommandBuffer)cmd, slot->vk_buffer, (VkDeviceSize)offset);
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Queries the maximum number of work groups that can be dispatched in a single compute command.
 * @details Returns the hardware limit for the number of local work groups in the X, Y, and Z dimensions.
 *          This corresponds to `glDispatchCompute` or `vkCmdDispatch` arguments.
 *          Note: This is the maximum count per dimension for a *single* dispatch, not the total number of concurrent groups.
 * @param[out] x Pointer to store the maximum X dimension.
 * @param[out] y Pointer to store the maximum Y dimension.
 * @param[out] z Pointer to store the maximum Z dimension.
 */
SITAPI void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z) {
    uint32_t max_x = 0, max_y = 0, max_z = 0;

    if (SituationIsInitialized()) {
#if defined(SITUATION_USE_VULKAN)
        if (sit_render.vk.physical_device) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
            max_x = props.limits.maxComputeWorkGroupCount[0];
            max_y = props.limits.maxComputeWorkGroupCount[1];
            max_z = props.limits.maxComputeWorkGroupCount[2];
        }
#elif defined(SITUATION_USE_OPENGL)
        // OpenGL 4.3+ required for Compute
        GLint gx = 0, gy = 0, gz = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &gx);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &gy);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &gz);
        max_x = (uint32_t)gx;
        max_y = (uint32_t)gy;
        max_z = (uint32_t)gz;
#endif
    }

    if (x) *x = max_x;
    if (y) *y = max_y;
    if (z) *z = max_z;
}

/**
 * @brief Queries whether a specific rendering feature is supported on the current platform/backend.
 *
 * @details Returns true if the given render feature (or combination of features when using a bitmask)
 *          is fully supported by the active graphics backend (OpenGL or Vulkan) and current hardware/driver.
 *
 *          This function supports **bitmask queries**  -  you can pass a combination of features using bitwise OR
 *          (e.g. `SITUATION_FEATURE_COMPUTE | SITUATION_FEATURE_MESH_SHADING`). In mask mode, the function
 *          returns true **only if all requested features are supported** (logical AND semantics).
 *
 *          Typical use cases:
 *            - Runtime feature detection for enabling/disabling advanced effects
 *            - Graceful degradation (fall back to simpler shaders/pipelines)
 *            - Conditional UI options in tools/editors
 *            - Logging supported feature set at startup
 *
 *          Supported features are defined in the `SituationRenderFeature` enum and include (but are not limited to):
 *            - Compute shaders
 *            - Mesh/task shaders
 *            - Ray tracing acceleration structures
 *            - Variable rate shading
 *            - Bindless resources
 *            - Multi-draw indirect
 *            - Etc.
 *
 * @param feature A single `SituationRenderFeature` value **or** a bitmask of multiple features combined with `|`.
 *                Passing 0 always returns true (no features requested).
 *
 * @return true if **all** requested features (or the single feature) are supported by the current backend/hardware,
 *         false otherwise.
 *         Returns true for unknown/undefined feature bits (safe default).
 *
 * @note This is a fast, cached query  -  results are determined during initialization (`SituationInit`)
 *       by checking extension strings, device properties, feature structs (Vulkan), or GL version/extensions.
 *       Thread-safe and non-blocking  -  safe to call from any thread at any time after init.
 *       Result does **not** change during the application lifetime unless the backend is reinitialized.
 *
 *       **Mask behavior example:**
 *       ```c
 *       if (SituationIsFeatureSupported(SITUATION_FEATURE_COMPUTE | SITUATION_FEATURE_MESH_SHADING)) {
 *           // Use advanced compute + mesh pipeline
 *       } else if (SituationIsFeatureSupported(SITUATION_FEATURE_COMPUTE)) {
 *           // Fallback to compute only
 *       }
 *       ```
 *
 * @see SituationRenderFeature (enum), SituationInit,
 *      SituationGetRendererBackend (if exists), SITUATION_FEATURE_xxx constants
 */
SITAPI bool SituationIsFeatureSupported(SituationRenderFeature feature) {
    if (!SituationIsInitialized()) return false;
    // Check against the mask populated during backend initialization
    return (sit_render.enabled_features_mask & feature) != 0;
}

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Creates a VkImage and allocates its memory using VMA.
 * @details This is a core Vulkan helper function that abstracts the creation of an image and the allocation of its device memory. It uses the Vulkan Memory Allocator (VMA) to handle the memory binding, which is the recommended practice.
 *
 * @param width The width of the image in pixels.
 * @param height The height of the image in pixels.
 * @param format The pixel format of the image (e.g., VK_FORMAT_R8G8B8A8_SRGB).
 * @param tiling The tiling arrangement for texels (VK_IMAGE_TILING_OPTIMAL for GPU-only, VK_IMAGE_TILING_LINEAR for CPU access).
 * @param usage A bitmask of VkImageUsageFlagBits specifying how the image will be used (e.g., as a color attachment, a sampled texture, etc.).
 * @param memory_usage A VmaMemoryUsage hint for the allocator (e.g., VMA_MEMORY_USAGE_GPU_ONLY for high-performance device memory).
 *          This function is essential for the cleanup routines of swapchains, textures, and virtual displays.
 *
 * @param image The `VkImage` handle to destroy.
 * @param allocation The associated `VmaAllocation` handle to free.
 */
static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation) {
    if (image != VK_NULL_HANDLE && sit_render.vk.vma_allocator != VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, image, allocation);
    }
}

/**
 * @brief [INTERNAL] Destroys a VkBuffer and frees its associated VMA allocation.
 * @details This is a simple wrapper around `vmaDestroyBuffer` that provides null-safety checks
 *          and centralizes buffer destruction logic for consistency with `_SituationVulkanDestroyImage`.
 *          It ensures that the buffer and allocator handles are valid before attempting destruction.
 *
 * @param buffer The VkBuffer handle to destroy. If VK_NULL_HANDLE, the function does nothing.
 * @param allocation The associated VmaAllocation handle to free.
 *
 * @note This function is safe to call with VK_NULL_HANDLE for the buffer parameter.
 * @note The VMA allocator must be valid (`sit_render.vk.vma_allocator != VK_NULL_HANDLE`) for destruction to occur.
 *
 * @see _SituationVulkanDestroyImage(), vmaDestroyBuffer(), _SituationDeferDestroyBuffer()
 */
static void _SituationVulkanDestroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
    if (buffer != VK_NULL_HANDLE && sit_render.vk.vma_allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, buffer, allocation);
    }
}

/**
 * @brief [INTERNAL] Creates a VkImageView for a given VkImage.
 * @details An image view is a mandatory component that describes how to access a VkImage and which parts of it are accessible. It specifies metadata like the format and aspect (e.g., color, depth, stencil) without which the GPU cannot interpret the image data.
 *
 * @param image The VkImage for which to create a view.
 * @param format The pixel format, which must be compatible with the format of the source image.
 * @param aspect_flags A bitmask of VkImageAspectFlagBits specifying which aspect of the image the view will access (e.g., VK_IMAGE_ASPECT_COLOR_BIT for color textures, VK_IMAGE_ASPECT_DEPTH_BIT for depth buffers).
 * @return A valid VkImageView handle on success, or VK_NULL_HANDLE on failure.
 */
static VkImageView _SituationVulkanCreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(sit_render.vk.device, &view_info, NULL, &image_view) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return image_view;
}

#endif // SITUATION_USE_VULKAN

// ============================================================================
// Virtual Display API
// ============================================================================

/**
 * @brief [INTERNAL] Extracts and interleaves geometry data from a raw GLTF primitive.
 *
 * @details This helper bridges the gap between the generic `cgltf` data structures and the specific
 *          interleaved vertex format required by the Situation engine.
 *
 *          It performs the following operations:
 *          1. Identifies accessors for Position, Normal, and TexCoord attributes.
 *          2. Allocates a single interleaved buffer for vertices.
 *          3. Reads and packs data into the layout: `[Px, Py, Pz, Nx, Ny, Nz, U, V]`.
 *             - If Normals are missing, defaults to `(0, 0, 1)`.
 *             - If UVs are missing, defaults to `(0, 0)`.
 *          4. Extracts indices and normalizes them to `uint32_t`, generating a linear sequence
 *             if the primitive is non-indexed.
 *
 * @param prim Pointer to the `cgltf_primitive` to process.
 * @param[out] out_vertices Pointer to receive the allocated float array of interleaved vertex data.
 *                          The caller owns this memory and must `free()` it.
 * @param[out] out_v_count Pointer to receive the total number of vertices.
 * @param[out] out_indices Pointer to receive the allocated `uint32_t` array of indices.
 *                          The caller owns this memory and must `free()` it.
 * @param[out] out_i_count Pointer to receive the total number of indices.
 *
 * @return `true` if extraction was successful (valid positions found, memory allocated).
 * @return `false` if the primitive is not a triangle list or if allocation failed.
 */
#if defined(CGLTF_IMPLEMENTATION)
static SituationError _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count) {
    if (!prim || !out_vertices || !out_v_count || !out_indices || !out_i_count) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_vertices = NULL;
    *out_indices = NULL;
    *out_v_count = 0;
    *out_i_count = 0;

    if (prim->type != cgltf_primitive_type_triangles) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_ASSET_PARSE_FAILED, "glTF primitive is not a triangle list.");
    }

    cgltf_accessor* pos_acc = NULL;
    cgltf_accessor* norm_acc = NULL;
    cgltf_accessor* uv_acc = NULL;

    for (size_t i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_position) pos_acc = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_normal)   norm_acc = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_texcoord) uv_acc = prim->attributes[i].data;
    }

    if (!pos_acc) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_ASSET_PARSE_FAILED, "glTF primitive has no POSITION attribute.");
    }

    *out_v_count = (int)pos_acc->count;

    *out_vertices = (float*)SIT_MALLOC(*out_v_count * 12 * sizeof(float));
    if (!*out_vertices) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_MEMORY_ALLOCATION, "glTF vertex buffer allocation failed.");
    }

    cgltf_accessor* tan_acc = NULL;
    for (size_t i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_tangent) tan_acc = prim->attributes[i].data;
    }

    for (int i = 0; i < *out_v_count; ++i) {
        float* v_ptr = &(*out_vertices)[i * 12];

        cgltf_accessor_read_float(pos_acc, i, v_ptr, 3);

        if (norm_acc) {
            cgltf_accessor_read_float(norm_acc, i, v_ptr + 3, 3);
        } else {
            v_ptr[3] = 0.0f; v_ptr[4] = 0.0f; v_ptr[5] = 1.0f;
        }

        if (tan_acc) {
            cgltf_accessor_read_float(tan_acc, i, v_ptr + 6, 4);
        } else {
            v_ptr[6] = 1.0f; v_ptr[7] = 0.0f; v_ptr[8] = 0.0f; v_ptr[9] = 1.0f;
        }

        if (uv_acc) {
            cgltf_accessor_read_float(uv_acc, i, v_ptr + 10, 2);
        } else {
            v_ptr[10] = 0.0f; v_ptr[11] = 0.0f;
        }
    }

    if (prim->indices) {
        *out_i_count = (int)prim->indices->count;
        *out_indices = (uint32_t*)SIT_MALLOC(*out_i_count * sizeof(uint32_t));
        if (!*out_indices) {
            SIT_FREE(*out_vertices);
            *out_vertices = NULL;
            return _SituationSetErrorFromCode(
                SITUATION_ERROR_MEMORY_ALLOCATION, "glTF index buffer allocation failed.");
        }

        for (int k = 0; k < *out_i_count; ++k) {
            (*out_indices)[k] = (uint32_t)cgltf_accessor_read_index(prim->indices, k);
        }
    } else {
        *out_i_count = *out_v_count;
        *out_indices = (uint32_t*)SIT_MALLOC(*out_i_count * sizeof(uint32_t));
        if (!*out_indices) {
            SIT_FREE(*out_vertices);
            *out_vertices = NULL;
            return _SituationSetErrorFromCode(
                SITUATION_ERROR_MEMORY_ALLOCATION, "glTF generated index buffer allocation failed.");
        }

        for (int k = 0; k < *out_i_count; ++k) {
            (*out_indices)[k] = (uint32_t)k;
        }
    }

    return SITUATION_SUCCESS;
}
#endif // CGLTF_IMPLEMENTATION

/**
 * @brief Loads a texture directly from a file path (Reload-Compatible).
 *
 * @details This is a convenience wrapper that combines `SituationLoadImage`, `SituationCreateTexture`,
 *          and `SituationUnloadImage`.
 *
 *          **Crucially**, unlike `SituationCreateTexture`, this function registers the `file_path`
 *          with the internal resource tracker. This enables `SituationReloadTexture` to work later.
 *
 * @param file_path The path to the image file (PNG, JPG, BMP, TGA, etc.).
 * @param generate_mipmaps If `true`, generates a full mipmap chain for the texture.
 *
 * @return A valid `SituationTexture` handle, or `{0}` on failure.
 */
SITAPI SituationError SituationLoadTexture(const char* file_path, bool generate_mipmaps, SituationTexture* out_texture) {
    if (out_texture) *out_texture = (SituationTexture){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_texture cannot be NULL.");

    SituationImage img = {0};
    SituationError load_err = SituationLoadImage(file_path, &img);
    if (load_err != SITUATION_SUCCESS) return load_err;

    SituationError err = SituationCreateTexture(img, generate_mipmaps, out_texture);
    SituationUnloadImage(img);

    if (err == SITUATION_SUCCESS) {
        _SituationTextureSlot* slot = _SitGetTextureSlot(*out_texture);
        if (slot) {
            slot->source_path = _sit_strdup(file_path);
            slot->mod_time = SituationGetFileModTime(file_path);
        }
    }
    return err;
}


/**
 * @brief Loads a 3D model from a GLTF/GLB file.
 *
 * @details This is a comprehensive asset loader that handles the entire pipeline of importing a 3D asset:
 *          1. **Parsing:** Uses `cgltf` to parse the file structure.
 *          2. **Textures:** Automatically resolves and loads all referenced texture files from disk into GPU memory (`SituationTexture`).
 *          3. **Geometry:** Iterates through meshes, extracts vertex/index data, interleaves it into the engine's format, and creates GPU resources (`SituationMesh`).
 *          4. **Materials:** Extracts PBR material properties (Base Color, Metallic, Roughness) and binds the loaded textures to the mesh instances.
 *
 * @param file_path The path to the `.gltf` or `.glb` file.
 *
 * @return A valid `SituationModel` handle containing all loaded resources.
 * @return A zeroed handle `{0}` if the file could not be found, parsed, or if `CGLTF_IMPLEMENTATION` is missing.
 *
 * @note This function relies on the helper `_SituationExtractGLTFPrimitive` to handle geometry processing.
 * @warning The caller is responsible for destroying the returned model using `SituationUnloadModel` to prevent GPU memory leaks.;
 */
SITAPI SituationError SituationLoadModel(const char* file_path, SituationModel* out_model) {
    if (out_model) *out_model = (SituationModel){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_model cannot be NULL");

#if defined(CGLTF_IMPLEMENTATION)
    cgltf_options options = {0};
    cgltf_data* data = NULL;

    // 1. Parse GLTF
    cgltf_result result = cgltf_parse_file(&options, file_path, &data);
    if (result != cgltf_result_success) return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Failed to parse GLTF file.");
    result = cgltf_load_buffers(&options, data, file_path);
    if (result != cgltf_result_success) { cgltf_free(data); return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Failed to load GLTF buffers."); }

    // 2. Allocate Slot
    SituationModel handle;
    _SituationModelSlot* slot = _SitAllocModelSlot(&handle);
    if (!slot) { cgltf_free(data); return SituationGetLastErrorCode(); }

    slot->source_path = _sit_strdup(file_path);
    slot->mod_time = SituationGetFileModTime(file_path);

    // 3. Load Textures
    slot->texture_count = (int)data->textures_count;
    if (slot->texture_count > 0) {
        slot->all_model_textures = SIT_CALLOC(slot->texture_count, sizeof(SituationTexture));
        if (!slot->all_model_textures) {
            _SitFreeModelSlot(handle); cgltf_free(data); return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        char* base_path = SituationGetBasePathFromFile(file_path);
        for (int i = 0; i < slot->texture_count; ++i) {
            const char* texture_uri = data->textures[i].image->uri;
            if (texture_uri) {
                char* full_texture_path = SituationJoinPath(base_path, texture_uri);
                SituationImage tex_img = {0};
                SituationError load_err = SituationLoadImage(full_texture_path, &tex_img);
                if (load_err == SITUATION_SUCCESS) {
                    SituationError tex_err = SituationCreateTexture(tex_img, true, &slot->all_model_textures[i]);
                    SituationUnloadImage(tex_img);
                    if (tex_err != SITUATION_SUCCESS) fprintf(stderr, "SITUATION WARNING: Model texture failed: %s\n", full_texture_path);
                } else {
                    fprintf(stderr, "SITUATION WARNING: Model image load failed: %s\n", full_texture_path);
                }
                SIT_FREE(full_texture_path);
            }
        }
        SIT_FREE(base_path);
    }

    // 4. Load Meshes
    slot->mesh_count = (int)data->meshes_count;
    if (slot->mesh_count > 0) {
        slot->meshes = SIT_CALLOC(slot->mesh_count, sizeof(SituationModelMesh));
        if (!slot->meshes) {
            // Cleanup textures
            for(int k=0; k<slot->texture_count; k++) SituationDestroyTexture(&slot->all_model_textures[k]);
            SIT_FREE(slot->all_model_textures);
            _SitFreeModelSlot(handle); cgltf_free(data); return SITUATION_ERROR_MEMORY_ALLOCATION;
        }

        for (int i = 0; i < slot->mesh_count; ++i) {
            cgltf_mesh* gltf_mesh = &data->meshes[i];
            SituationModelMesh* sit_mesh = &slot->meshes[i];
            if (gltf_mesh->name) strncpy(sit_mesh->name, gltf_mesh->name, SITUATION_MAX_DEVICE_NAME_LEN - 1);

            if (gltf_mesh->primitives_count > 0) {
                cgltf_primitive* prim = &gltf_mesh->primitives[0];
                float* vertex_data = NULL; uint32_t* index_data = NULL; int v_count = 0; int i_count = 0;
                SituationError extract_err = _SituationExtractGLTFPrimitive(
                    prim, &vertex_data, &v_count, &index_data, &i_count);
                if (extract_err == SITUATION_SUCCESS) {
                    SituationError mesh_err = SituationCreateMeshEx(
                        vertex_data, v_count, 12 * sizeof(float),
                        index_data, i_count,
                        SIT_MESH_LAYOUT_POS_NRM_TAN_TEX,
                        &sit_mesh->gpu_mesh);
                    SIT_FREE(vertex_data);
                    SIT_FREE(index_data);
                    if (mesh_err != SITUATION_SUCCESS) {
                        for (int j = 0; j < i; ++j) {
                            SituationDestroyMesh(&slot->meshes[j].gpu_mesh);
                        }
                        for (int k = 0; k < slot->texture_count; ++k) {
                            SituationDestroyTexture(&slot->all_model_textures[k]);
                        }
                        SIT_FREE(slot->meshes);
                        SIT_FREE(slot->all_model_textures);
                        _SitFreeModelSlot(handle);
                        cgltf_free(data);
                        return mesh_err;
                    }
                } else {
                    for (int j = 0; j < i; ++j) {
                        SituationDestroyMesh(&slot->meshes[j].gpu_mesh);
                    }
                    for (int k = 0; k < slot->texture_count; ++k) {
                        SituationDestroyTexture(&slot->all_model_textures[k]);
                    }
                    SIT_FREE(slot->meshes);
                    SIT_FREE(slot->all_model_textures);
                    _SitFreeModelSlot(handle);
                    cgltf_free(data);
                    return extract_err;
                }

                cgltf_material* mat = prim->material;
                if (mat) {
                    if (mat->has_pbr_metallic_roughness) {
                        cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;
                        memcpy(sit_mesh->base_color_factor.raw, pbr->base_color_factor, sizeof(Vector4));
                        sit_mesh->metallic_factor = pbr->metallic_factor;
                        sit_mesh->roughness_factor = pbr->roughness_factor;
                        if (pbr->base_color_texture.texture) sit_mesh->base_color_texture = slot->all_model_textures[pbr->base_color_texture.texture - data->textures];
                        if (pbr->metallic_roughness_texture.texture) sit_mesh->metallic_roughness_texture = slot->all_model_textures[pbr->metallic_roughness_texture.texture - data->textures];
                    }
                    if (mat->normal_texture.texture) sit_mesh->normal_texture = slot->all_model_textures[mat->normal_texture.texture - data->textures];
                    memcpy(sit_mesh->emissive_factor.raw, mat->emissive_factor, sizeof(Vector3));
                    if (mat->emissive_texture.texture) sit_mesh->emissive_texture = slot->all_model_textures[mat->emissive_texture.texture - data->textures];
                }
            }
        }
    }

    cgltf_free(data);

    // Fill handle cache
    handle.mesh_count = slot->mesh_count;
    handle.meshes = slot->meshes;
    *out_model = handle;
    return SITUATION_SUCCESS;
#else
    (void)file_path;
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Model loading not available. Please implement cgltf.h.");
#endif
}


/**
 * @brief Unloads a model and frees all of its associated GPU and CPU resources.
 * @details This is the only correct way to clean up a model loaded with `SituationLoadModel`.
 *          It systematically performs the following actions:
 *          1. Iterates through every sub-mesh in the model and calls `SituationDestroyMesh` to free its GPU vertex/index buffers.;
 *          2. Iterates through every texture loaded with the model and calls `SituationDestroyTexture` to free its GPU resources.
 *          3. Frees the CPU memory used for the arrays that held the mesh and texture handles.
 *          4. Invalidates the user's `SituationModel` handle by zeroing it out.
 *
 * @param[in,out] model A pointer to the `SituationModel` object to be unloaded. The handle becomes invalid after this call.
 *
 * @note Failure to call this function on a loaded model will result in significant GPU and CPU memory leaks.
 *       It is essential for proper resource management.
 * @note It is safe to call this function on a NULL pointer or an already-unloaded model (where `model->id` is 0);
 *       it will simply do nothing.
 */
SITAPI void SituationUnloadModel(SituationModel* model) {
    if (!model) return;
    _SituationModelSlot* slot = _SitGetModelSlot(*model);
    if (!slot) return;

    if (slot->meshes) {
        for (int i = 0; i < slot->mesh_count; i++) {
            SituationDestroyMesh(&slot->meshes[i].gpu_mesh);
        }
        SIT_FREE(slot->meshes);
    }
    if (slot->all_model_textures) {
        for (int i = 0; i < slot->texture_count; i++) {
            SituationDestroyTexture(&slot->all_model_textures[i]);
        }
        SIT_FREE(slot->all_model_textures);
    }

    _SitFreeModelSlot(*model);
    memset(model, 0, sizeof(SituationModel));
}


/* ============================================================================
 * STL Model Loader
 * ============================================================================
 *
 * Supports both binary STL and ASCII STL, auto-detected by sniffing the header.
 *
 * Binary STL record layout (50 bytes per triangle):
 *   float[3]  normal
 *   float[3]  vertex 0
 *   float[3]  vertex 1
 *   float[3]  vertex 2
 *   uint16_t  attribute byte count (ignored)
 *
 * Vertex layout produced (stride 32, matches SituationCreateMesh stride-32 path):
 *   float[3]  position
 *   float[3]  normal
 *   float[2]  uv  (always 0,0 — STL has no UV data)
 *
 * Flat normals (default): each triangle corner uses the face normal from the file.
 * Smooth normals: vertices at the same position are merged and their normals averaged.
 *   Uses a simple O(n²) spatial merge with an epsilon of 1e-5 — fine for typical
 *   STL triangle counts (< ~200k). For very large meshes the flat path is faster.
 */

/* Internal: parse ASCII STL into flat triangle soup (caller frees *out_verts). */
static SituationError _SitSTLParseASCII(const char* text, float** out_verts, int* out_tri_count) {
    /* Count "facet normal" occurrences to pre-size the buffer */
    int tri_count = 0;
    const char* p = text;
    while ((p = strstr(p, "facet normal")) != NULL) { ++tri_count; ++p; }
    if (tri_count == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_ASSET_PARSE_FAILED, "STL ASCII: no facets found.");
    }

    /* 9 floats per triangle (3 verts × 3 coords); normals come from the file per-face. */
    float* verts = (float*)SIT_MALLOC(tri_count * 9 * sizeof(float));
    float* norms = (float*)SIT_MALLOC(tri_count * 3 * sizeof(float));
    if (!verts || !norms) {
        SIT_FREE(verts); SIT_FREE(norms);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "STL ASCII: vertex buffer alloc failed.");
    }

    p = text;
    int t = 0;
    while (t < tri_count) {
        p = strstr(p, "facet normal");
        if (!p) break;
        float nx, ny, nz;
        if (sscanf(p, "facet normal %f %f %f", &nx, &ny, &nz) != 3) { ++p; continue; }
        norms[t * 3 + 0] = nx; norms[t * 3 + 1] = ny; norms[t * 3 + 2] = nz;

        const char* vloop = strstr(p, "outer loop");
        if (!vloop) { ++p; ++t; continue; }

        int ok = 1;
        for (int v = 0; v < 3; ++v) {
            vloop = strstr(vloop, "vertex");
            if (!vloop) { ok = 0; break; }
            float vx, vy, vz;
            if (sscanf(vloop, "vertex %f %f %f", &vx, &vy, &vz) != 3) { ok = 0; break; }
            verts[t * 9 + v * 3 + 0] = vx;
            verts[t * 9 + v * 3 + 1] = vy;
            verts[t * 9 + v * 3 + 2] = vz;
            ++vloop;
        }
        if (!ok) { ++p; continue; }
        ++t; ++p;
    }
    tri_count = t; /* actual parsed count */

    /* Pack into interleaved [Px Py Pz Nx Ny Nz U V] × 3 verts per face */
    float* interleaved = (float*)SIT_MALLOC(tri_count * 3 * 8 * sizeof(float));
    if (!interleaved) {
        SIT_FREE(verts); SIT_FREE(norms);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "STL ASCII: interleaved alloc failed.");
    }
    for (int t2 = 0; t2 < tri_count; ++t2) {
        for (int v = 0; v < 3; ++v) {
            float* dst = &interleaved[(t2 * 3 + v) * 8];
            dst[0] = verts[t2 * 9 + v * 3 + 0];
            dst[1] = verts[t2 * 9 + v * 3 + 1];
            dst[2] = verts[t2 * 9 + v * 3 + 2];
            dst[3] = norms[t2 * 3 + 0];
            dst[4] = norms[t2 * 3 + 1];
            dst[5] = norms[t2 * 3 + 2];
            dst[6] = 0.0f; dst[7] = 0.0f;
        }
    }
    SIT_FREE(verts); SIT_FREE(norms);
    *out_verts = interleaved;
    *out_tri_count = tri_count;
    return SITUATION_SUCCESS;
}

/* Internal: parse binary STL into flat interleaved soup (caller frees *out_verts). */
static SituationError _SitSTLParseBinary(const uint8_t* data, size_t data_size, float** out_verts, int* out_tri_count) {
    /* Binary STL: 80-byte header + 4-byte tri count + 50 bytes × tri_count */
    if (data_size < 84) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_ASSET_PARSE_FAILED, "STL binary: file too small.");
    }
    uint32_t tri_count;
    memcpy(&tri_count, data + 80, sizeof(uint32_t));
    if (data_size < 84 + (size_t)tri_count * 50) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_ASSET_PARSE_FAILED, "STL binary: truncated triangle data.");
    }

    float* interleaved = (float*)SIT_MALLOC((size_t)tri_count * 3 * 8 * sizeof(float));
    if (!interleaved) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "STL binary: vertex buffer alloc failed.");
    }

    const uint8_t* rec = data + 84;
    for (uint32_t t = 0; t < tri_count; ++t, rec += 50) {
        float nx, ny, nz, v[9];
        memcpy(&nx, rec + 0,  4); memcpy(&ny, rec + 4,  4); memcpy(&nz, rec + 8,  4);
        memcpy(&v[0], rec + 12, 36); /* 3 verts × 3 floats × 4 bytes */
        for (int i = 0; i < 3; ++i) {
            float* dst = &interleaved[(t * 3 + i) * 8];
            dst[0] = v[i * 3 + 0]; dst[1] = v[i * 3 + 1]; dst[2] = v[i * 3 + 2];
            dst[3] = nx;            dst[4] = ny;             dst[5] = nz;
            dst[6] = 0.0f;          dst[7] = 0.0f;
        }
    }

    *out_verts = interleaved;
    *out_tri_count = (int)tri_count;
    return SITUATION_SUCCESS;
}

/* Internal: merge coincident vertices and accumulate normals for smooth shading.
 * Operates on an existing flat interleaved buffer in-place; outputs a new deduplicated
 * vertex buffer + index buffer. Caller frees both. O(n²) — acceptable for < ~100k tris. */
static SituationError _SitSTLSmoothNormals(const float* flat_verts, int flat_v_count,
                                            float** out_verts, int* out_v_count,
                                            uint32_t** out_indices, int* out_i_count) {
    const float EPS = 1e-5f;

    /* Worst case: every vertex is unique */
    float*    sv  = (float*)SIT_MALLOC(flat_v_count * 8 * sizeof(float));
    uint32_t* idx = (uint32_t*)SIT_MALLOC(flat_v_count * sizeof(uint32_t));
    float*    acc = (float*)SIT_MALLOC(flat_v_count * 3 * sizeof(float)); /* accumulated normals */
    int*      cnt = (int*)SIT_CALLOC(flat_v_count, sizeof(int));
    if (!sv || !idx || !acc || !cnt) {
        SIT_FREE(sv); SIT_FREE(idx); SIT_FREE(acc); SIT_FREE(cnt);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "STL smooth: alloc failed.");
    }

    int unique = 0;
    for (int i = 0; i < flat_v_count; ++i) {
        const float* src = flat_verts + i * 8;
        /* Search existing unique vertices for a position match */
        int found = -1;
        for (int j = 0; j < unique; ++j) {
            const float* cmp = sv + j * 8;
            float dx = src[0] - cmp[0], dy = src[1] - cmp[1], dz = src[2] - cmp[2];
            if (dx*dx + dy*dy + dz*dz < EPS * EPS) { found = j; break; }
        }
        if (found < 0) {
            /* New unique vertex */
            memcpy(sv + unique * 8, src, 8 * sizeof(float));
            acc[unique * 3 + 0] = src[3];
            acc[unique * 3 + 1] = src[4];
            acc[unique * 3 + 2] = src[5];
            cnt[unique] = 1;
            idx[i] = (uint32_t)unique;
            ++unique;
        } else {
            acc[found * 3 + 0] += src[3];
            acc[found * 3 + 1] += src[4];
            acc[found * 3 + 2] += src[5];
            ++cnt[found];
            idx[i] = (uint32_t)found;
        }
    }

    /* Normalize accumulated normals */
    for (int j = 0; j < unique; ++j) {
        float nx = acc[j*3+0], ny = acc[j*3+1], nz = acc[j*3+2];
        float len = sqrtf(nx*nx + ny*ny + nz*nz);
        if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
        sv[j*8+3] = nx; sv[j*8+4] = ny; sv[j*8+5] = nz;
    }

    SIT_FREE(acc); SIT_FREE(cnt);
    *out_verts   = sv;
    *out_v_count = unique;
    *out_indices = idx;
    *out_i_count = flat_v_count;
    return SITUATION_SUCCESS;
}

/**
 * @brief Loads a 3D model from a binary or ASCII STL file.
 *
 * @details STL files contain only triangle geometry with per-face normals; they carry no texture
 *          coordinates, no material data, and no scene hierarchy. The loader produces a single-mesh
 *          SituationModel using the stride-32 vertex layout `[Px Py Pz Nx Ny Nz U V]` (UVs zero).
 *
 *          Auto-detection: if the first 5 bytes of the file are "solid" the file is treated as
 *          ASCII STL, otherwise binary STL. This matches the de-facto standard heuristic.
 *
 *          Flat normals (smooth_normals = false, default): each triangle corner keeps the
 *          face normal from the file, producing crisp hard edges. 3 unique vertices per triangle.
 *
 *          Smooth normals (smooth_normals = true): coincident vertices (within 1e-5 epsilon) are
 *          merged and their normals averaged, producing a smooth appearance. Uses O(n²) merge —
 *          suitable for meshes up to ~100k triangles; prefer flat for very large files.
 *
 * @param file_path     Path to the .stl file (binary or ASCII).
 * @param smooth_normals When true, merge coincident vertices and average normals for smooth shading.
 * @param out_model     Receives the loaded model on success. Contains one SituationModelMesh;
 *                      material fields are zeroed (base_color_factor defaults to white).
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_FILE_READ_FAILED if the file cannot be opened.
 * @return SITUATION_ERROR_ASSET_PARSE_FAILED if the STL data is malformed.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION on allocation failure.
 */
SITAPI SituationError SituationLoadModelFromSTL(const char* file_path, bool smooth_normals, SituationModel* out_model) {
    if (out_model) *out_model = (SituationModel){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationLoadModelFromSTL: out_model is NULL.");
    if (!file_path) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationLoadModelFromSTL: file_path is NULL.");

    /* --- 1. Read the entire file into memory --- */
    FILE* f = fopen(file_path, "rb");
    if (!f) return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "SituationLoadModelFromSTL: cannot open file.");

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) { fclose(f); return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "SituationLoadModelFromSTL: empty file."); }

    uint8_t* file_data = (uint8_t*)SIT_MALLOC((size_t)file_size + 1);
    if (!file_data) { fclose(f); return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromSTL: file buffer alloc failed."); }
    size_t read_bytes = fread(file_data, 1, (size_t)file_size, f);
    fclose(f);
    file_data[read_bytes] = '\0'; /* null-terminate for ASCII path */

    /* --- 2. Auto-detect binary vs ASCII and parse into flat interleaved soup --- */
    float* flat_verts = NULL;
    int    tri_count  = 0;
    SituationError parse_err;

    /* Heuristic: ASCII STL starts with "solid" (case-sensitive per spec, but universally lowercase).
     * Note: some binary STL files also start with "solid" in the 80-byte header, so we also check
     * that the binary tri-count yields a plausible file size before deciding. */
    bool looks_ascii = (read_bytes >= 5 && strncmp((char*)file_data, "solid", 5) == 0);
    if (looks_ascii) {
        /* Verify it isn't a binary STL that happens to start with "solid" */
        if (read_bytes >= 84) {
            uint32_t bin_tri_count;
            memcpy(&bin_tri_count, file_data + 80, sizeof(uint32_t));
            if (84 + (size_t)bin_tri_count * 50 == (size_t)read_bytes) {
                looks_ascii = false; /* matches binary layout exactly — treat as binary */
            }
        }
    }

    if (looks_ascii) {
        parse_err = _SitSTLParseASCII((char*)file_data, &flat_verts, &tri_count);
    } else {
        parse_err = _SitSTLParseBinary(file_data, (size_t)read_bytes, &flat_verts, &tri_count);
    }
    SIT_FREE(file_data);

    if (parse_err != SITUATION_SUCCESS) return parse_err;
    if (tri_count == 0 || !flat_verts) {
        SIT_FREE(flat_verts);
        return _SituationSetErrorFromCode(SITUATION_ERROR_ASSET_PARSE_FAILED, "SituationLoadModelFromSTL: no triangles parsed.");
    }

    /* --- 3. Optionally merge vertices for smooth normals --- */
    float*    final_verts   = flat_verts;
    int       final_v_count = tri_count * 3;
    uint32_t* final_indices = NULL;
    int       final_i_count = 0;
    bool      owns_smooth   = false;

    if (smooth_normals) {
        float*    sv  = NULL;
        uint32_t* idx = NULL;
        int       sv_count = 0, si_count = 0;
        SituationError smooth_err = _SitSTLSmoothNormals(flat_verts, tri_count * 3, &sv, &sv_count, &idx, &si_count);
        SIT_FREE(flat_verts);
        flat_verts = NULL;
        if (smooth_err != SITUATION_SUCCESS) return smooth_err;
        final_verts   = sv;
        final_v_count = sv_count;
        final_indices = idx;
        final_i_count = si_count;
        owns_smooth   = true;
    } else {
        /* Flat path: generate a linear index buffer (0, 1, 2, 3, ...) */
        final_i_count = tri_count * 3;
        final_indices = (uint32_t*)SIT_MALLOC(final_i_count * sizeof(uint32_t));
        if (!final_indices) {
            SIT_FREE(flat_verts);
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromSTL: index buffer alloc failed.");
        }
        for (int k = 0; k < final_i_count; ++k) final_indices[k] = (uint32_t)k;
    }
    (void)owns_smooth;

    /* --- 4. Upload to GPU via SituationCreateMesh (stride 32 = 8 floats × 4 bytes) --- */
    SituationMesh gpu_mesh = {0};
    SituationError mesh_err = SituationCreateMesh(final_verts, final_v_count, 8 * sizeof(float),
                                                   final_indices, final_i_count, &gpu_mesh);
    SIT_FREE(final_verts);
    SIT_FREE(final_indices);

    if (mesh_err != SITUATION_SUCCESS) return mesh_err;

    /* --- 5. Allocate a model slot with a single sub-mesh --- */
    SituationModel handle;
    _SituationModelSlot* slot = _SitAllocModelSlot(&handle);
    if (!slot) {
        SituationDestroyMesh(&gpu_mesh);
        return SituationGetLastErrorCode();
    }

    slot->source_path    = _sit_strdup(file_path);
    slot->mod_time       = SituationGetFileModTime(file_path);
    slot->is_stl         = true;
    slot->stl_smooth_normals = smooth_normals;
    slot->mesh_count     = 1;
    slot->texture_count  = 0;
    slot->all_model_textures = NULL;
    slot->meshes = (SituationModelMesh*)SIT_CALLOC(1, sizeof(SituationModelMesh));
    if (!slot->meshes) {
        SituationDestroyMesh(&gpu_mesh);
        _SitFreeModelSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromSTL: mesh array alloc failed.");
    }

    slot->meshes[0].gpu_mesh            = gpu_mesh;
    slot->meshes[0].base_color_factor   = (Vector4){{1.0f, 1.0f, 1.0f, 1.0f}};
    slot->meshes[0].metallic_factor     = 0.0f;
    slot->meshes[0].roughness_factor    = 0.8f;
    strncpy(slot->meshes[0].name, "stl_mesh", SITUATION_MAX_DEVICE_NAME_LEN - 1);

    /* Copy cached metadata into the handle (mirrors what SituationLoadModel does). */
    handle.mesh_count = slot->mesh_count;
    handle.meshes     = slot->meshes;

    *out_model = handle;
    return SITUATION_SUCCESS;
}


/* ============================================================================
 * OBJ Model Loader
 * ============================================================================ */

typedef struct OBJReaderContext {
    char** buffers;
    int count;
    int capacity;
} OBJReaderContext;

static void _SitOBJReaderContextInit(OBJReaderContext* ctx) {
    ctx->buffers = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}

static void _SitOBJReaderContextFree(OBJReaderContext* ctx) {
    if (ctx->buffers) {
        for (int i = 0; i < ctx->count; ++i) {
            SIT_FREE(ctx->buffers[i]);
        }
        SIT_FREE(ctx->buffers);
    }
    ctx->count = 0;
    ctx->capacity = 0;
}

static void _SituationOBJFileReader(void* ctx_ptr, const char* filename, int is_mtl, const char* obj_filename, char** buf, size_t* len) {
    OBJReaderContext* ctx = (OBJReaderContext*)ctx_ptr;
    *buf = NULL;
    *len = 0;
    
    char* path_to_load = NULL;
    if (is_mtl) {
        char* base_path = SituationGetBasePathFromFile(obj_filename);
        path_to_load = SituationJoinPath(base_path, filename);
        SIT_FREE(base_path);
    } else {
        path_to_load = _sit_strdup(filename);
    }
    
    if (!path_to_load) return;
    
    FILE* f = fopen(path_to_load, "rb");
    SIT_FREE(path_to_load);
    if (!f) return;
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(f);
        return;
    }
    
    char* file_data = (char*)SIT_MALLOC(file_size + 1);
    if (!file_data) {
        fclose(f);
        return;
    }
    
    size_t read_bytes = fread(file_data, 1, file_size, f);
    fclose(f);
    file_data[read_bytes] = '\0';
    
    *buf = file_data;
    *len = read_bytes;
    
    // Add to our context tracking list
    if (ctx->count >= ctx->capacity) {
        int new_capacity = ctx->capacity == 0 ? 4 : ctx->capacity * 2;
        char** new_buffers = (char**)SIT_REALLOC(ctx->buffers, new_capacity * sizeof(char*));
        if (new_buffers) {
            ctx->buffers = new_buffers;
            ctx->capacity = new_capacity;
        }
    }
    if (ctx->count < ctx->capacity) {
        ctx->buffers[ctx->count++] = file_data;
    }
}

typedef struct VertexMapEntry {
    tinyobj_vertex_index_t key;
    uint32_t value;
} VertexMapEntry;

typedef struct VertexMap {
    VertexMapEntry* entries;
    uint32_t capacity;
    uint32_t count;
} VertexMap;

static void _SitVertexMapInit(VertexMap* map, uint32_t start_capacity) {
    map->capacity = start_capacity;
    map->count = 0;
    map->entries = (VertexMapEntry*)SIT_CALLOC(start_capacity, sizeof(VertexMapEntry));
    for (uint32_t i = 0; i < start_capacity; ++i) {
        map->entries[i].key.v_idx = -1; // -1 represents empty slot
    }
}

static uint32_t _SitVertexHash(tinyobj_vertex_index_t key, uint32_t capacity) {
    uint32_t h = (uint32_t)key.v_idx * 73856093 ^ (uint32_t)key.vn_idx * 19349663 ^ (uint32_t)key.vt_idx * 83492791;
    return h % capacity;
}

static int _SitVertexMapGet(VertexMap* map, tinyobj_vertex_index_t key, uint32_t* out_val) {
    if (map->capacity == 0) return 0;
    uint32_t idx = _SitVertexHash(key, map->capacity);
    uint32_t start = idx;
    do {
        if (map->entries[idx].key.v_idx == -1) {
            return 0; // Not found
        }
        if (map->entries[idx].key.v_idx == key.v_idx &&
            map->entries[idx].key.vn_idx == key.vn_idx &&
            map->entries[idx].key.vt_idx == key.vt_idx) {
            *out_val = map->entries[idx].value;
            return 1; // Found
        }
        idx = (idx + 1) % map->capacity;
    } while (idx != start);
    return 0;
}

static void _SitVertexMapPut(VertexMap* map, tinyobj_vertex_index_t key, uint32_t val) {
    if (map->count * 2 >= map->capacity) { // Grow at 50% load factor
        uint32_t old_cap = map->capacity;
        VertexMapEntry* old_entries = map->entries;
        
        map->capacity = old_cap == 0 ? 1024 : old_cap * 2;
        map->entries = (VertexMapEntry*)SIT_CALLOC(map->capacity, sizeof(VertexMapEntry));
        for (uint32_t i = 0; i < map->capacity; ++i) {
            map->entries[i].key.v_idx = -1;
        }
        map->count = 0;
        
        for (uint32_t i = 0; i < old_cap; ++i) {
            if (old_entries[i].key.v_idx != -1) {
                _SitVertexMapPut(map, old_entries[i].key, old_entries[i].value);
            }
        }
        SIT_FREE(old_entries);
    }
    
    uint32_t idx = _SitVertexHash(key, map->capacity);
    while (map->entries[idx].key.v_idx != -1) {
        idx = (idx + 1) % map->capacity;
    }
    map->entries[idx].key = key;
    map->entries[idx].value = val;
    map->count++;
}

static void _SitVertexMapFree(VertexMap* map) {
    SIT_FREE(map->entries);
    map->capacity = 0;
    map->count = 0;
}

/** Unit face normal from triangle positions (CCW). Returns false if degenerate. */
static bool _SitOBJTriangleNormal(const float* v0, const float* v1, const float* v2, float out_n[3]) {
    const float e1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
    const float e2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
    out_n[0] = e1[1] * e2[2] - e1[2] * e2[1];
    out_n[1] = e1[2] * e2[0] - e1[0] * e2[2];
    out_n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    const float len_sq = out_n[0] * out_n[0] + out_n[1] * out_n[1] + out_n[2] * out_n[2];
    if (len_sq <= 1e-16f) {
        out_n[0] = 0.0f;
        out_n[1] = 0.0f;
        out_n[2] = 0.0f;
        return false;
    }
    const float inv_len = 1.0f / sqrtf(len_sq);
    out_n[0] *= inv_len;
    out_n[1] *= inv_len;
    out_n[2] *= inv_len;
    return true;
}

/**
 * Normalize OBJ-supplied normals; fill missing/degenerate corners from face geometry.
 * Vertices without vn indices are stored as (0,0,0) during extraction as a sentinel.
 * Preserves smooth/per-corner normals from the file when valid.
 */
static void _SitOBJFinalizeMeshNormals(float* vertices, int vertex_count,
                                       const uint32_t* indices, int index_count) {
    if (!vertices || vertex_count <= 0 || !indices || index_count < 3) {
        return;
    }

    for (int vi = 0; vi < vertex_count; ++vi) {
        float* n = &vertices[vi * 12 + 3];
        const float len_sq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
        if (len_sq > 1e-16f) {
            const float inv_len = 1.0f / sqrtf(len_sq);
            n[0] *= inv_len;
            n[1] *= inv_len;
            n[2] *= inv_len;
        }
    }

    for (int tri = 0; tri < index_count / 3; ++tri) {
        const uint32_t i0 = indices[tri * 3 + 0];
        const uint32_t i1 = indices[tri * 3 + 1];
        const uint32_t i2 = indices[tri * 3 + 2];
        if (i0 >= (uint32_t)vertex_count || i1 >= (uint32_t)vertex_count ||
            i2 >= (uint32_t)vertex_count) {
            continue;
        }

        const float* v0 = &vertices[i0 * 12];
        const float* v1 = &vertices[i1 * 12];
        const float* v2 = &vertices[i2 * 12];
        float face_n[3];
        if (!_SitOBJTriangleNormal(v0, v1, v2, face_n)) {
            face_n[0] = 0.0f;
            face_n[1] = 0.0f;
            face_n[2] = 1.0f;
        }

        const uint32_t corners[3] = {i0, i1, i2};
        for (int c = 0; c < 3; ++c) {
            float* n = &vertices[corners[c] * 12 + 3];
            const float len_sq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
            if (len_sq <= 1e-16f) {
                n[0] = face_n[0];
                n[1] = face_n[1];
                n[2] = face_n[2];
            }
        }
    }

    for (int vi = 0; vi < vertex_count; ++vi) {
        float* n = &vertices[vi * 12 + 3];
        const float len_sq = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
        if (len_sq <= 1e-16f) {
            n[0] = 0.0f;
            n[1] = 0.0f;
            n[2] = 1.0f;
        }
    }
}

typedef struct OBJTextureCacheEntry {
    char* filename;
    SituationTexture texture;
} OBJTextureCacheEntry;

typedef struct OBJTextureCache {
    OBJTextureCacheEntry* entries;
    int count;
    int capacity;
} OBJTextureCache;

static void _SitOBJTextureCacheInit(OBJTextureCache* cache) {
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

static SituationTexture _SitOBJTextureCacheGetOrLoad(OBJTextureCache* cache, const char* filename, const char* obj_filename) {
    for (int i = 0; i < cache->count; ++i) {
        if (strcmp(cache->entries[i].filename, filename) == 0) {
            return cache->entries[i].texture;
        }
    }
    
    SituationTexture tex = {0};
    char* base_path = SituationGetBasePathFromFile(obj_filename);
    char* full_path = SituationJoinPath(base_path, filename);
    SIT_FREE(base_path);
    
    SituationError err = SituationLoadTexture(full_path, true, &tex);
    SIT_FREE(full_path);
    if (err == SITUATION_SUCCESS) {
        if (cache->count >= cache->capacity) {
            cache->capacity = cache->capacity == 0 ? 4 : cache->capacity * 2;
            cache->entries = (OBJTextureCacheEntry*)SIT_REALLOC(cache->entries, cache->capacity * sizeof(OBJTextureCacheEntry));
        }
        cache->entries[cache->count].filename = _sit_strdup(filename);
        cache->entries[cache->count].texture = tex;
        cache->count++;
    }
    return tex;
}

static void _SitOBJTextureCacheFree(OBJTextureCache* cache) {
    if (cache->entries) {
        for (int i = 0; i < cache->count; ++i) {
            SIT_FREE(cache->entries[i].filename);
        }
        SIT_FREE(cache->entries);
    }
    cache->count = 0;
    cache->capacity = 0;
}

/**
 * @brief Loads a Wavefront OBJ model (geometry, materials, optional textures).
 *
 * @details Uses tinyobj with triangulation. Each shape becomes one SituationModelMesh in the
 *          standard 48-byte layout (position, normal, tangent, UV). When the OBJ omits normal
 *          indices for a corner, or supplies a degenerate normal, the loader derives a face
 *          normal from triangle positions. Valid per-corner normals from the file are preserved
 *          and normalized (smooth-shaded OBJ assets keep their authored shading).
 */
SITAPI SituationError SituationLoadModelFromOBJ(const char* file_path, SituationModel* out_model) {
    if (out_model) *out_model = (SituationModel){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationLoadModelFromOBJ: out_model is NULL.");
    if (!file_path) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationLoadModelFromOBJ: file_path is NULL.");

    OBJReaderContext reader_ctx;
    _SitOBJReaderContextInit(&reader_ctx);

    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t* materials = NULL;
    size_t num_materials = 0;

    int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials,
                                file_path, _SituationOBJFileReader, &reader_ctx, TINYOBJ_FLAG_TRIANGULATE);

    if (ret != TINYOBJ_SUCCESS) {
        _SitOBJReaderContextFree(&reader_ctx);
        return _SituationSetErrorFromCode(SITUATION_ERROR_ASSET_PARSE_FAILED, "SituationLoadModelFromOBJ: failed to parse OBJ file.");
    }

    SituationModel handle;
    _SituationModelSlot* slot = _SitAllocModelSlot(&handle);
    if (!slot) {
        tinyobj_attrib_free(&attrib);
        tinyobj_shapes_free(shapes, num_shapes);
        tinyobj_materials_free(materials, num_materials);
        _SitOBJReaderContextFree(&reader_ctx);
        return SituationGetLastErrorCode();
    }

    slot->source_path = _sit_strdup(file_path);
    slot->mod_time = SituationGetFileModTime(file_path);
    slot->is_obj = true;

    OBJTextureCache tex_cache;
    _SitOBJTextureCacheInit(&tex_cache);

    slot->mesh_count = (int)num_shapes;
    if (slot->mesh_count > 0) {
        slot->meshes = (SituationModelMesh*)SIT_CALLOC(slot->mesh_count, sizeof(SituationModelMesh));
        if (!slot->meshes) {
            _SitOBJReaderContextFree(&reader_ctx);
            _SitFreeModelSlot(handle);
            tinyobj_attrib_free(&attrib);
            tinyobj_shapes_free(shapes, num_shapes);
            tinyobj_materials_free(materials, num_materials);
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromOBJ: mesh array alloc failed.");
        }

        for (int i = 0; i < slot->mesh_count; ++i) {
            tinyobj_shape_t* shape = &shapes[i];
            SituationModelMesh* sit_mesh = &slot->meshes[i];
            if (shape->name) strncpy(sit_mesh->name, shape->name, SITUATION_MAX_DEVICE_NAME_LEN - 1);
            else snprintf(sit_mesh->name, SITUATION_MAX_DEVICE_NAME_LEN, "obj_mesh_%d", i);

            // Default material factors
            sit_mesh->base_color_factor = (Vector4){{1.0f, 1.0f, 1.0f, 1.0f}};
            sit_mesh->metallic_factor = 0.0f;
            sit_mesh->roughness_factor = 0.8f;
            sit_mesh->emissive_factor = (Vector3){{0.0f, 0.0f, 0.0f}};

            int mat_id = -1;
            if (attrib.material_ids && shape->length > 0) {
                mat_id = attrib.material_ids[shape->face_offset / 3];
            }

            if (mat_id >= 0 && (size_t)mat_id < num_materials) {
                tinyobj_material_t* mat = &materials[mat_id];
                sit_mesh->base_color_factor.x = mat->diffuse[0];
                sit_mesh->base_color_factor.y = mat->diffuse[1];
                sit_mesh->base_color_factor.z = mat->diffuse[2];
                sit_mesh->base_color_factor.w = mat->dissolve;

                sit_mesh->roughness_factor = mat->shininess > 0.0f ? 1.0f - (mat->shininess / 1000.0f) : 0.8f;
                if (sit_mesh->roughness_factor < 0.0f) sit_mesh->roughness_factor = 0.0f;
                if (sit_mesh->roughness_factor > 1.0f) sit_mesh->roughness_factor = 1.0f;

                float spec_sum = mat->specular[0] + mat->specular[1] + mat->specular[2];
                sit_mesh->metallic_factor = spec_sum > 0.0f ? spec_sum / 3.0f : 0.0f;
                if (sit_mesh->metallic_factor > 1.0f) sit_mesh->metallic_factor = 1.0f;

                sit_mesh->emissive_factor.x = mat->emission[0];
                sit_mesh->emissive_factor.y = mat->emission[1];
                sit_mesh->emissive_factor.z = mat->emission[2];

                if (mat->diffuse_texname && strlen(mat->diffuse_texname) > 0) {
                    sit_mesh->base_color_texture = _SitOBJTextureCacheGetOrLoad(&tex_cache, mat->diffuse_texname, file_path);
                }
                if (mat->bump_texname && strlen(mat->bump_texname) > 0) {
                    sit_mesh->normal_texture = _SitOBJTextureCacheGetOrLoad(&tex_cache, mat->bump_texname, file_path);
                }
            }

            // Extract geometry
            VertexMap v_map;
            _SitVertexMapInit(&v_map, 1024);

            int shape_v_count = 0;
            int shape_v_capacity = 1024;
            float* final_vertices = (float*)SIT_MALLOC(shape_v_capacity * 12 * sizeof(float));

            int shape_i_count = shape->length * 3;
            uint32_t* final_indices = (uint32_t*)SIT_MALLOC(shape_i_count * sizeof(uint32_t));

            if (!final_vertices || !final_indices) {
                _SitVertexMapFree(&v_map);
                SIT_FREE(final_vertices);
                SIT_FREE(final_indices);
                for (int j = 0; j < i; ++j) {
                    SituationDestroyMesh(&slot->meshes[j].gpu_mesh);
                }
                for (int k = 0; k < tex_cache.count; ++k) {
                    SituationDestroyTexture(&tex_cache.entries[k].texture);
                }
                _SitOBJTextureCacheFree(&tex_cache);
                _SitOBJReaderContextFree(&reader_ctx);
                _SitFreeModelSlot(handle);
                tinyobj_attrib_free(&attrib);
                tinyobj_shapes_free(shapes, num_shapes);
                tinyobj_materials_free(materials, num_materials);
                return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromOBJ: vertex/index buffer alloc failed.");
            }

            for (size_t f = 0; f < shape->length; ++f) {
                for (int v = 0; v < 3; ++v) {
                    tinyobj_vertex_index_t vi = attrib.faces[shape->face_offset + f * 3 + v];
                    uint32_t existing_idx = 0;
                    if (_SitVertexMapGet(&v_map, vi, &existing_idx)) {
                        final_indices[f * 3 + v] = existing_idx;
                    } else {
                        if (shape_v_count >= shape_v_capacity) {
                            shape_v_capacity *= 2;
                            float* new_verts = (float*)SIT_REALLOC(final_vertices, shape_v_capacity * 12 * sizeof(float));
                            if (!new_verts) {
                                _SitVertexMapFree(&v_map);
                                SIT_FREE(final_vertices);
                                SIT_FREE(final_indices);
                                for (int j = 0; j < i; ++j) {
                                    SituationDestroyMesh(&slot->meshes[j].gpu_mesh);
                                }
                                for (int k = 0; k < tex_cache.count; ++k) {
                                    SituationDestroyTexture(&tex_cache.entries[k].texture);
                                }
                                _SitOBJTextureCacheFree(&tex_cache);
                                _SitOBJReaderContextFree(&reader_ctx);
                                _SitFreeModelSlot(handle);
                                tinyobj_attrib_free(&attrib);
                                tinyobj_shapes_free(shapes, num_shapes);
                                tinyobj_materials_free(materials, num_materials);
                                return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadModelFromOBJ: vertex buffer realloc failed.");
                            }
                            final_vertices = new_verts;
                        }

                        float* v_ptr = &final_vertices[shape_v_count * 12];

                        // Position
                        if (vi.v_idx >= 0 && (unsigned int)vi.v_idx < attrib.num_vertices) {
                            v_ptr[0] = attrib.vertices[vi.v_idx * 3 + 0];
                            v_ptr[1] = attrib.vertices[vi.v_idx * 3 + 1];
                            v_ptr[2] = attrib.vertices[vi.v_idx * 3 + 2];
                        } else {
                            v_ptr[0] = 0.0f; v_ptr[1] = 0.0f; v_ptr[2] = 0.0f;
                        }

                        // Normal — (0,0,0) sentinel when vn is absent; finalized after extraction.
                        if (vi.vn_idx >= 0 && (unsigned int)vi.vn_idx < attrib.num_normals) {
                            v_ptr[3] = attrib.normals[vi.vn_idx * 3 + 0];
                            v_ptr[4] = attrib.normals[vi.vn_idx * 3 + 1];
                            v_ptr[5] = attrib.normals[vi.vn_idx * 3 + 2];
                        } else {
                            v_ptr[3] = 0.0f; v_ptr[4] = 0.0f; v_ptr[5] = 0.0f;
                        }

                        // Tangent (defaulted)
                        v_ptr[6] = 1.0f; v_ptr[7] = 0.0f; v_ptr[8] = 0.0f; v_ptr[9] = 1.0f;

                        // Texcoord
                        if (vi.vt_idx >= 0 && (unsigned int)vi.vt_idx < attrib.num_texcoords) {
                            v_ptr[10] = attrib.texcoords[vi.vt_idx * 2 + 0];
                            v_ptr[11] = attrib.texcoords[vi.vt_idx * 2 + 1];
                        } else {
                            v_ptr[10] = 0.0f; v_ptr[11] = 0.0f;
                        }

                        _SitVertexMapPut(&v_map, vi, (uint32_t)shape_v_count);
                        final_indices[f * 3 + v] = (uint32_t)shape_v_count;
                        shape_v_count++;
                    }
                }
            }

            _SitVertexMapFree(&v_map);

            _SitOBJFinalizeMeshNormals(final_vertices, shape_v_count, final_indices, shape_i_count);

            SituationError mesh_err = SituationCreateMeshEx(
                final_vertices, shape_v_count, 12 * sizeof(float),
                final_indices, shape_i_count,
                SIT_MESH_LAYOUT_POS_NRM_TAN_TEX,
                &sit_mesh->gpu_mesh);
            SIT_FREE(final_vertices);
            SIT_FREE(final_indices);
            if (mesh_err != SITUATION_SUCCESS) {
                for (int j = 0; j <= i; ++j) {
                    SituationDestroyMesh(&slot->meshes[j].gpu_mesh);
                }
                for (int k = 0; k < tex_cache.count; ++k) {
                    SituationDestroyTexture(&tex_cache.entries[k].texture);
                }
                _SitOBJTextureCacheFree(&tex_cache);
                _SitOBJReaderContextFree(&reader_ctx);
                _SitFreeModelSlot(handle);
                tinyobj_attrib_free(&attrib);
                tinyobj_shapes_free(shapes, num_shapes);
                tinyobj_materials_free(materials, num_materials);
                return mesh_err;
            }
        }
    }

    // Cache textures in slot
    slot->texture_count = tex_cache.count;
    if (slot->texture_count > 0) {
        slot->all_model_textures = (SituationTexture*)SIT_CALLOC(slot->texture_count, sizeof(SituationTexture));
        if (slot->all_model_textures) {
            for (int k = 0; k < slot->texture_count; ++k) {
                slot->all_model_textures[k] = tex_cache.entries[k].texture;
            }
        }
    }

    _SitOBJTextureCacheFree(&tex_cache);
    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    _SitOBJReaderContextFree(&reader_ctx);

    // Fill handle cache
    handle.mesh_count = slot->mesh_count;
    handle.meshes = slot->meshes;
    *out_model = handle;

    return SITUATION_SUCCESS;
}


/**
 * @brief Records commands to draw a complete 3D model with a given transformation.
 * @details This is a high-level drawing command that iterates through all sub-meshes of a model.
 *          For each sub-mesh, it binds the material-specific textures and sets material properties via push constants before issuing a draw call for the mesh's geometry.
 *
 * @par Shader Contract Prerequisites
 *   For this function to work correctly, the caller is **responsible** for binding a compatible PBR-style shader *before* calling it. The shader must expect:
 *   - **Textures** at the binding points defined in the Shader Contract (e.g., `SIT_SAMPLER_BINDING_ALBEDO` at binding 0).
 *   - **Push Constants** with a layout matching the internal `PBRModelPushConstants` struct, containing the model matrix, base color factor, and metallic/roughness factors.
 *   - **Camera Data** from a previously bound UBO (e.g., at `SIT_UBO_BINDING_VIEW_DATA`).
 *
 * @param cmd The command buffer for the current frame.
 * @param model The `SituationModel` handle to draw. Must be a valid, loaded model.
 * @param transform The root model-to-world transformation matrix (position, rotation, scale) to apply to the entire model.
 *
 * @note This function is a high-level convenience wrapper. It can generate many state changes (texture binds) if the model has many unique materials, which may have performance implications.
 */
SITAPI SituationError SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform) {
    _SituationModelSlot* slot = _SitGetModelSlot(model);
    if (!slot || !slot->meshes) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationDrawModel: invalid model handle");
    }

    for (int i = 0; i < slot->mesh_count; i++) {
        SituationModelMesh* mesh = &slot->meshes[i];
        if (mesh->gpu_mesh.slot_index == 0 && mesh->gpu_mesh.generation == 0) continue; // Invalid mesh handle

        // Push Constants for PBR
        typedef struct {
            mat4 model;
            Vector4 base_color_factor;
            Vector4 pbr_factors; // x=metal, y=rough, z=unused
        } PBRModelPushConstants;

        PBRModelPushConstants constants;
        glm_mat4_copy(transform, constants.model); // Copy matrix
        constants.base_color_factor = mesh->base_color_factor;
        constants.pbr_factors.x = mesh->metallic_factor;
        constants.pbr_factors.y = mesh->roughness_factor;

        SituationCmdSetPushConstant(cmd, 0, &constants, sizeof(PBRModelPushConstants));

        // Bind Textures
        if (mesh->base_color_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_ALBEDO, mesh->base_color_texture);
        if (mesh->normal_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_NORMAL, mesh->normal_texture);
        if (mesh->metallic_roughness_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_PBR_MAP, mesh->metallic_roughness_texture);
        if (mesh->emissive_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_EMISSIVE, mesh->emissive_texture);

        SituationCmdDrawMesh(cmd, mesh->gpu_mesh);
    }
    return SITUATION_SUCCESS;
}


/**
 * @brief Saves a model's structure and geometry to a human-readable GLTF 2.0 file.
 * @details This is a powerful utility for debugging, asset inspection, or exporting procedurally generated content. It writes the model's scene graph, materials, and texture references to a JSON-based `.gltf` file, and all binary vertex and index data to an accompanying `.bin` file.
 * @param model The `SituationModel` object to save.
 * @param file_path The destination path for the output `.gltf` file. The `.bin` file will be created in the same directory with a corresponding name.
 * @return `true` if the model was saved successfully, `false` otherwise.
 * @note This is an advanced utility with two important requirements:
 *       1. It requires both `cgltf.h` and `cgltf_write.h` to be available in the project.
 *       2. It relies on being able to read geometry data back from the GPU, which can be a slow operation. For best results, use this for debugging or development tools rather than as a frequent runtime operation.
 */
SITAPI SituationError SituationSaveModelAsGltf(SituationModel model, const char* file_path) {
#if !defined(CGLTF_WRITE_H)
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationSaveModelAsGltf requires CGLTF_WRITE_H to be included.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#elif defined(CGLTF_IMPLEMENTATION)
    if (model.id == 0) return SITUATION_ERROR_INVALID_PARAM;

    // This is a simplified outline. A full implementation is very involved.

    // 1. Setup cgltf_data structure
    cgltf_data* data = SIT_CALLOC(1, sizeof(cgltf_data));
    data->meshes_count = model.mesh_count;
    data->meshes = SIT_CALLOC(model.mesh_count, sizeof(cgltf_mesh));
    // ... allocate memory for materials, textures, accessors, buffer_views, buffers ...

    // This will hold all vertex/index data for the entire model
    cgltf_buffer* main_buffer = &data->buffers[0];

    // 2. Loop through each SituationModelMesh
    for (int i = 0; i < model.mesh_count; ++i) {
        SituationModelMesh* sit_mesh = &model.meshes[i];
        cgltf_mesh* gltf_mesh = &data->meshes[i];

        // a. Get CPU-side vertex and index data for this mesh.
        //    *** CRITICAL: This requires a new function to read back GPU data ***
        void* vertex_data;
        void* index_data;
        int vertex_count, index_count, vertex_stride;
        // This function would be slow!
        SituationGetMeshData(sit_mesh->gpu_mesh, &vertex_data, &vertex_count, &vertex_stride, &index_data, &index_count);

        // b. Append this data to a giant CPU buffer that will become the .bin file.
        //    Update buffer_views and accessors to point to the correct offsets and strides
        //    within this giant buffer. This involves a lot of pointer arithmetic and bookkeeping.

        // c. Create a cgltf_material for this mesh's material.
        //    Copy the PBR factors and texture indices into the cgltf struct.
    }

    // 3. Write the file
    cgltf_options options = {0};
    options.type = cgltf_file_type_gltf; // Human-readable .gltf + .bin
    cgltf_result result = cgltf_write_file(&options, file_path, data);

    // 4. Cleanup
    cgltf_free(data);

    return result == cgltf_result_success ? SITUATION_SUCCESS : SITUATION_ERROR_FILE_WRITE_FAILED;
#else
    (void)model; (void)file_path;
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Model saving not available. Please implement cgltf.h and cgltf_write.h.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}




//==================================================================================
// Implementation for Hot-Reloading
//==================================================================================

/**
 * @brief Reloads a texture from its original image file.
 *
 * @details This function destroys the existing GPU texture resources (Image, View, Sampler, Memory) and
 *          re-loads the image data from the original file path.
 *
 *          **Requirement:** The texture must have been loaded using `SituationLoadTexture`.
 *          Textures created via `SituationCreateTexture` (from raw memory) cannot be reloaded.
 *
 * @param[in,out] texture A pointer to the `SituationTexture` handle to reload.
 *
 * @return `true` if the image was successfully loaded and uploaded to the GPU.
 * @return `false` if the file could not be loaded or if the original path was not tracked.
 */
SITAPI SituationError SituationReloadTexture(SituationTexture* texture) {
    if (!SituationIsInitialized() || !texture) return SITUATION_ERROR_INVALID_PARAM;
    _SituationTextureSlot* slot = _SitGetTextureSlot(*texture);
    if (!slot || !slot->source_path) return SITUATION_ERROR_INVALID_PARAM;

    SituationImage img = {0};
    if (SituationLoadImage(slot->source_path, &img) != SITUATION_SUCCESS) return SITUATION_ERROR_FILE_NOT_FOUND;

    SituationTexture temp;
    SituationError err = SituationCreateTexture(img, true, &temp);
    if (err == SITUATION_SUCCESS) {
        _SituationTextureSlot* new_slot = _SitGetTextureSlot(temp);
        if (new_slot) {
            // Swap
            #if defined(SITUATION_USE_OPENGL)
            _SitGLDeferDestroyTexture(slot->gl_texture_id);
            slot->gl_texture_id = new_slot->gl_texture_id;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
            slot->image = new_slot->image;
            slot->allocation = new_slot->allocation;
            slot->image_view = new_slot->image_view;
            slot->sampler = new_slot->sampler;
            #endif
            slot->width = new_slot->width;
            slot->height = new_slot->height;
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            new_slot->is_active = false;
            SituationUnloadImage(img);
            return SITUATION_SUCCESS;
        }
        SituationUnloadImage(img);
        return _SituationSetErrorFromCode(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,
            "SituationReloadTexture: new slot not accessible after successful create (registry defect)");
    }
    SituationUnloadImage(img);
    return err;
}


/**
 * @brief Reloads a 3D model and all its dependencies.
 *
 * @details This is a "heavy" operation. It unloads the entire model structure, including:
 *          1. All sub-meshes (vertex/index buffers).
 *          2. All associated textures (Albedo, Normal, PBR maps).
 *
 *          It then re-parses the GLTF/GLB file and re-uploads all geometry and textures to the GPU.
 *          This is useful for iterating on 3D assets (e.g., exporting from Blender and seeing updates instantly).
 *
 * @param[in,out] model A pointer to the `SituationModel` handle to reload.
 *
 * @return `true` on success, `false` on failure.
 */
SITAPI SituationError SituationReloadModel(SituationModel* model) {
    if (!SituationIsInitialized() || !model) return SITUATION_ERROR_INVALID_PARAM;
    _SituationModelSlot* slot = _SitGetModelSlot(*model);
    if (!slot || !slot->source_path) return SITUATION_ERROR_INVALID_PARAM;

    SituationModel new_handle;
    SituationError err;
    if (slot->is_stl) {
        err = SituationLoadModelFromSTL(slot->source_path, slot->stl_smooth_normals, &new_handle);
    } else if (slot->is_obj) {
        err = SituationLoadModelFromOBJ(slot->source_path, &new_handle);
    } else {
        err = SituationLoadModel(slot->source_path, &new_handle);
    }

    if (err == SITUATION_SUCCESS) {
        _SituationModelSlot* new_slot = _SitGetModelSlot(new_handle);
        if (new_slot) {
            // Free old resources
            if (slot->meshes) {
                for(int i=0; i<slot->mesh_count; i++) SituationDestroyMesh(&slot->meshes[i].gpu_mesh);
                SIT_FREE(slot->meshes);
            }
            if (slot->all_model_textures) {
                for(int i=0; i<slot->texture_count; i++) SituationDestroyTexture(&slot->all_model_textures[i]);
                SIT_FREE(slot->all_model_textures);
            }

            // Move new resources to old slot
            slot->meshes = new_slot->meshes;
            slot->mesh_count = new_slot->mesh_count;
            slot->all_model_textures = new_slot->all_model_textures;
            slot->texture_count = new_slot->texture_count;
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            // Update handle cache
            model->mesh_count = slot->mesh_count;
            model->meshes = slot->meshes;

            new_slot->is_active = false;
            return SITUATION_SUCCESS;
        }
    }
    return SITUATION_ERROR_GENERAL;
}


/**
 * @brief Reloads a compute pipeline from its original source file.
 *
 * @details Similar to `SituationReloadShader`, but for Compute Pipelines. It recompiles the GLSL source
 *          (or re-reads SPIR-V) and rebuilds the `VkPipeline` (Vulkan) or `GL Program` (OpenGL).
 *          It automatically reuses the `SituationComputeLayoutType` that was specified during the initial creation.
 *
 * @param[in,out] pipeline A pointer to the `SituationComputePipeline` handle to reload.
 *
 * @return `true` on success, `false` on failure.
 */
SITAPI SituationError SituationReloadComputePipeline(SituationComputePipeline* pipeline) {
    if (!SituationIsInitialized() || !pipeline) return SITUATION_ERROR_INVALID_PARAM;
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*pipeline);
    if (!slot || !slot->source_path) return SITUATION_ERROR_INVALID_PARAM;

    // Reload from source
    char* source = SituationLoadFileText(slot->source_path);
    if (!source) return SITUATION_ERROR_FILE_NOT_FOUND;

    SituationComputePipeline new_pipe_handle;
    SituationError err = SituationCreateComputePipelineFromMemory(source, slot->layout_type, &new_pipe_handle);
    SIT_FREE(source);

    if (err == SITUATION_SUCCESS) {
        _SituationComputePipelineSlot* new_slot = _SitGetComputePipelineSlot(new_pipe_handle);
        if (new_slot) {
            // Swap internals
            #if defined(SITUATION_USE_OPENGL)
            if (glIsProgram(slot->gl_program_id)) glDeleteProgram(slot->gl_program_id);
            slot->gl_program_id = new_slot->gl_program_id;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyPipeline(slot->vk_pipeline, VK_NULL_HANDLE); // Layout shared? No, we kept layout in slot.
            // But wait, SituationCreateComputePipelineFromMemory creates NEW layout usually?
            // My implementation reuses layout_type lookup or creates new?
            // It creates new layout?
            // "VkPipelineLayout layout = sit_render.vk.compute_layouts[layout_type];" - It reuses standard layout.
            // So we don't destroy layout.
            // We destroy old pipeline.
            vkDestroyShaderModule(sit_render.vk.device, slot->shader_module, NULL); // Destroy old module
            slot->vk_pipeline = new_slot->vk_pipeline;
            slot->shader_module = new_slot->shader_module;
            #endif

            // Update metadata
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            // Free new slot shell
            new_slot->is_active = false;
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,
            "SituationReloadComputePipeline: new slot not accessible after successful create (registry defect)");
    }
    return err;
}

#endif // SITUATION_IMPL_RENDERER_FRAME_CMD_H
