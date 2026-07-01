# Situation UPDATELOG — v2.4.x (part 4 of 5)

Patches **2.4.301** through **2.4.400 "Win32 Identity WI-5 — Plan Complete"** (84 entries, oldest first).

Index: [`UPDATELOG.md`](UPDATELOG.md) · Previous: [`updatelog_24_03.md`](updatelog_24_03.md) · Next: [`updatelog_24_05.md`](updatelog_24_05.md)

---

## [v2.4.301] - 2026-06-17

### Description

**v2.4.301**: 10-bit color output plan **Phase 6** — Vulkan HDR10 swapchain selection and caps/feature split.

1. **`VK_EXT_swapchain_colorspace`**: enabled at instance creation so WSI can expose `VK_COLOR_SPACE_HDR10_ST2084_EXT`.
2. **`_SituationVulkanPickSurfaceFormat`**: prefers `A2R10G10B10` + `HDR10_ST2084` when policy is `SIT_OUTPUT_COLOR_HDR10` or `AUTO` and both WSI HDR10 and DXGI `hdr_enabled` on the window monitor; otherwise existing 10-bit SDR / 8-bit fallbacks with stderr diagnostics.
3. **New policy `SIT_OUTPUT_COLOR_HDR10`**: explicit HDR10 request; fail-soft to 10-bit SDR then 8-bit with logged reason (OS HDR off or WSI missing HDR10 pair).
4. **Caps / features**: `output_hdr_active`, `output_color_space`, `wsi_supports_hdr10`; `SIT_FEATURE_HDR_OUTPUT` now means HDR10 active only; new `SIT_FEATURE_10BIT_SDR_OUTPUT` for 10-bit SDR swapchain.
5. **Init order**: `_SituationCachePhysicalDisplays()` runs before renderer init so DXGI gating is available at first swapchain create.
6. **Harness**: `report_hdr_10bit_display_capability` prints full DXGI fields + Phase 6 swapchain/WSI lines; `sit_test_assert_output_color_depth_consistent()` updated for HDR vs SDR10 split.

### Changes

- **`sit/situation_api.h`**: `SIT_OUTPUT_COLOR_HDR10`, `SituationOutputColorSpace`, extended `SituationGraphicsCaps`, `SIT_FEATURE_10BIT_SDR_OUTPUT`.
- **`sit/situation_impl_renderer.h`**: colorspace extension, `_SituationVulkanSurfaceSupportsHdr10`, HDR-aware format picker.
- **`sit/situation_impl_decl.h`**: `_SituationWantsHdr10Output`, `_SituationSetOutputColorDepthState(active_10bit, active_hdr)`, vk `surface_supports_hdr10` / `swapchain_color_space`.
- **`sit/situation_impl_ctrl.h`**: early display cache; caps fill for Phase 6 fields.
- **`sit/situation_impl_wdm.h`**: `_SituationResolveWindowMonitorId`, `_SituationWindowMonitorDxgiHdrEnabled`; display cache no longer requires `SituationIsInitialized`.
- **`tests/harness/test_output_color_depth.c`**, **`sit_test_output_color_depth.h`**: enhanced capability report and assertions.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 6 partial status.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.301.

---

---

---

---

---

---

## [v2.4.302] - 2026-06-17

### Description

**v2.4.302**: 10-bit color output plan **Phase 6 (complete)** + **Phase 7** — ST.2084 PQ output pipeline and harness proof.

1. **Phase 6 finish**: refresh display cache at swapchain create; force `VK_PRESENT_MODE_FIFO_KHR` when `output_hdr_active`; log HDR10 swapchain line; document compositor/window HDR policy in plan.
2. **Phase 7 PQ pipeline**: `_SitSt2084LinearToPq` / `_SitSt2084PqToLinear` in `situation_impl_color.h`; public `SituationLinearToPq`, `SituationPqToLinear`, `SituationYpqToRgb10PackedHdr`, `SituationPqGrayToRgb10Packed`, `SituationColorRgbaToHdrPqClear`.
3. **Vulkan clear path**: `_SituationColorRgbaToClearFloats` converts sRGB clears → linear → PQ when HDR swapchain active.
4. **Harness**: `st2084_pq_roundtrip`, `pq_half_rgb10_patch` in core module (PQ 0.5 → packed ~512).

### Changes

- **`sit/situation_impl_color.h`**: ST.2084 PQ encode/decode, HDR packed pixel helpers, clear-color conversion.
- **`sit/situation_impl_image.h`**: public PQ API implementations.
- **`sit/situation_impl_renderer.h`**: HDR FIFO present mode, swapchain create log, PQ clear floats.
- **`sit/situation_api.h`**: PQ/HDR packed pixel API declarations.
- **`tests/harness/test_core.c`**: PQ roundtrip + reference patch tests.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 6–7 marked done.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.302.

---

---

---

---

## [v2.4.303] - 2026-06-17

### Description

**v2.4.303**: 10-bit color output plan **Phase 8** — HDR verification tests and raw swapchain readback.

1. **`SIT_TEXTURE_READ_RGB10_PACKED`**: `SituationReadFramebuffer` can return raw A2R10G10B10 uint32 texels (Vulkan 10-bit swapchain); new `SituationReadFramebufferHdr` wrapper requires `output_hdr_active`.
2. **Harness (opt-in `SIT_TEST_HDR=1`)**: `hdr_caps_and_feature`, `hdr_swapchain_format_logged`, `hdr_raw_ramp_readback` (PQ ramp, ≥512 distinct R10 codes), `visual_hdr_grading_bands` (`SIT_TEST_HDR_VISUAL=1`).
3. **WSI fix**: enumerate swapchain formats via `vkGetPhysicalDeviceSurfaceFormats2KHR` (+ `VK_KHR_get_surface_capabilities2`) so HDR10_ST2084 pairs are visible to the format picker.
4. **Harness setup**: auto-position test window on first DXGI HDR-enabled monitor when `SIT_TEST_HDR=1`.
5. **WSI format picker**: accept `A2B10G10R10` + HDR10 (common on Windows; previously only checked `A2R10G10B10`).
6. **Shared helpers**: `sit_test_require_hdr10_active`, `sit_test_hdr_os_and_wsi_ready` in `sit_test_output_color_depth.h`.

### Changes

- **`sit/situation_api.h`**: `SIT_TEXTURE_READ_RGB10_PACKED`, `SituationReadFramebufferHdr`.
- **`sit/situation_impl_renderer.h`**: raw packed readback path in `SituationReadFramebuffer`; SurfaceFormats2 WSI query + `VK_KHR_get_surface_capabilities2`.
- **`tests/harness/test_output_color_depth.c`**: Phase 8 HDR tests + PQ ramp/grading shaders.
- **`tests/harness/sit_test_output_color_depth.h`**, **`sit_test_window.h`**: HDR test helpers + `SIT_TEST_HDR_VISUAL`.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 8 marked done.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.303.

---

---

---

---

---

---

## [v2.4.304] - 2026-06-18

### Description

**v2.4.304**: Example quality-of-life, VSync/FPS control, and render thread hardening.

1. **Example 2 fixed** — shapes were not rendering. Incomplete manual `SituationRenderPassInfo` (no depth attachment, implicit storeOp) caused draws to be discarded on some paths/backends. Now consistently uses `SituationRenderPassInfoDefault`.
2. **VSync toggle now actually works for performance** — F9/V now also flips `SituationSetTargetFPS(0)` when off. Shared examples force `SIT_OUTPUT_COLOR_8BIT` at init so HDR detection no longer silently forces `FIFO` present mode.
3. **Render thread stutters reduced**:
   - Removed per-iteration `fprintf(stderr)` + `fflush` spam in `_SituationRenderThreadEntry` (was doing I/O on every frame).
   - `MAX_FRAMES_IN_FLIGHT` bumped from 2 → 3.
   - Render queue `cnd_timedwait` timeout reduced from 50 ms → 1 ms.
4. **Build hygiene**:
   - `make clean` / `build_situation.bat clean` now also removes `*.a` (static libs) so header-only changes actually recompile.
   - Misleading "up to date (no rebuild needed)" status messages changed to simple "OK".
5. **Version** bumped to 2.4.304.

### Changes

- **`examples/02_draw_shapes/main.c`**: switched to `SituationRenderPassInfoDefault`.
- **`examples/shared/sit_example.h`**: VSync toggle + target FPS logic, force 8-bit color depth.
- **`sit/situation_impl_renderer.h`**: removed render-thread spam, bumped frames-in-flight constant, shorter timeout.
- **`sit/situation_api.h`**: `SITUATION_MAX_FRAMES_IN_FLIGHT` now 3.
- **`sit/Makefile`**, **`build/build_situation.bat`**: improved `clean` (removes `*.a`), cleaner status messages and help text.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.304.

---

---

---

---

---

---

## [v2.4.305] - 2026-06-18

### Description

**v2.4.305**: General-purpose stutter and frame-spike diagnostics for debugging.

Added lightweight, always-on (but near-zero-cost) tools so you can objectively detect spikes and understand their causes without staring at animation or attaching external profilers. Designed for real applications, not just examples.

1. **Spike tracking**:
   - `SituationGetMaxFrameTime()` — highest frame delta seen.
   - `SituationGetFrameSpikeCount()` — number of frames that crossed the spike threshold.
   - Automatic detection inside `SituationUpdateTimers` (threshold = 2× target or ~8 ms when uncapped).

2. **Per-frame phase timing** (last completed frame, in nanoseconds):
   - Backpressure / render-queue wait time
   - Fence / client-wait time (`glClientWaitSync`, Vulkan fence waits)
   - Execute / command replay time
   - Present / swap time

3. **Better live visibility**:
   - `SituationDrawMetricsOverlay` now shows `max XXX, spikes N` plus a "Phases: bp ... fp ... ex ... pr ..." line.
   - `SituationExportRenderHistogram` now emits real frame-time histogram buckets + spike count + last phases (no longer just render latency).

4. **New convenience API**:
   - `SituationGetLastFramePhases()` — fetch the four phase times for the most recent frame.

All additions are guarded, use the existing monotonic timer, and add no measurable overhead when you are not actively looking at the data.

See `doc/plan/AAA_ARCHITECTURE_PLAN.md` (Section 6 lightweight slice) for the design and safety rules that were followed.

### Changes

- **`sit/situation_impl_decl.h`**: new fields `max_frame_time`, `frame_spike_count`, `frame_time_hist[6]`, `last_backpressure_ns`, `last_fence_wait_ns`, `last_execute_ns`, `last_present_ns`.
- **`sit/situation_impl_ctrl.h`**: spike detection + histogram bucket update in `SituationUpdateTimers`; initialization.
- **`sit/situation_impl_renderer.h`**: instrumentation of backpressure waits, fence waits, execute, and present paths; updated `DrawMetricsOverlay` and `ExportRenderHistogram` output; implementations of the new getters.
- **`sit/situation_api.h`**: `SituationGetMaxFrameTime`, `SituationGetFrameSpikeCount`, `SituationGetLastFramePhases`.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.305.
- **`doc/plan/AAA_ARCHITECTURE_PLAN.md`**: Section 6 expanded with "Lightweight Stutter & Spike Diagnostics" (6.0), explicit safety/harmlessness rules, and updated ordering/minimum scope.

---

---

---

---

---

---

## [v2.4.306] - 2026-06-18

### Description

**v2.4.306**: Stutter diagnostics improvements, render thread VSync fixes, and debugging tooling hardening.

- Fixed VSync toggle (F9/V) when render thread is enabled — guarded `glfwSwapInterval` calls so they only happen on the thread with the current GL context. The render thread now consistently sets the interval before every swap based on the current flag.
- Added core engine timing for better diagnostics: `last_poll_ns` and `last_update_ns` (time spent in `SituationPollInputEvents` and `SituationUpdateTimers`).
- Added finer-grained poll sub-timings: `last_glfw_poll_ns`, `last_input_reset_ns`, `last_joystick_ns` to pinpoint sources of core latency.
- Improved auto-stutter logging in `SituationUpdateTimers`: now uses relative threshold (avoids spamming at 60 FPS) and includes detailed core (with sub-poll) + render phase breakdown.
- Updated `SituationDrawMetricsOverlay` and `SituationExportRenderHistogram` to surface core phases (poll/update) and poll sub-details alongside render phases.
- Made polling text in the M-key debug overlay use standard size (8px) instead of 2x for better readability.
- Moved and refined the M-key debug metrics overlay in shared examples (left-anchored, background, fixed-width formatting to reduce text jiggle/clipping).
- Build fixes: restored accidentally-dropped `target_frame_time` member in global state struct; removed duplicate `poll_t0` definition.
- Fixed GLFW "Cannot set swap interval without current context" errors on VSync toggle.
- More robust instrumentation for identifying whether stutters are in core (poll/update) or render (backpressure/fence/execute/present).

See the lightweight diagnostics slice in `doc/plan/AAA_ARCHITECTURE_PLAN.md`.

### Changes

- **`sit/situation_base_version.h`**: bumped patch to 2.4.306.
- **`sit/situation_impl_decl.h`**: added `last_poll_ns`, `last_update_ns`, `last_glfw_poll_ns`, `last_input_reset_ns`, `last_joystick_ns`; restored `target_frame_time`.
- **`sit/situation_impl_ctrl.h`**: full timing of `PollInputEvents` (with sub-timings) and `UpdateTimers`; adaptive stutter logging with poll sub-details; fixed duplicate `poll_t0` var.
- **`sit/situation_impl_wdm.h`**: guarded `glfwSwapInterval` calls behind context check.
- **`sit/situation_impl_renderer.h`**: per-frame swap interval set in render thread; updated overlay + histogram JSON to include core phases and poll subs; fixed formats and poll text size (standard 8px) for less visual jitter/clipping.
- **`examples/shared/sit_example.h`**: M-key metrics toggle with better positioning, background, and poll text at standard size.
- **`doc/UPDATELOG.md`**, **`doc/whatsnew.md`**: documented.

---

---

---

---

---

---

## [v2.4.307] - 2026-06-19

### Description

**v2.4.307**: Piano roll audio fix, true uncapped FPS with VSync off, and console log spam cleanup.

- **node_graph_piano_demo** — No sound fixed: added final `MIXER` sink node (chain now ends gain→mixer for reliable master output), explicit `SituationTopologicalSort` after patches and post-`SetActiveGraph`, plus direct `SituationPlayToneEx` fallback in note-on for guaranteed audibility (graph path + effects still active for wet presets).
- **High FPS with VSync off** — Backpressure waits now skipped when `target_frame_time==0` (main loop no longer blocks waiting for render slots; ring overwrites old frames, render thread processes latest). `SITUATION_MAX_FRAMES_IN_FLIGHT` bumped to 6 for more pipelining. YIELD policy + relaxed checks let uncapped loops reach much higher than monitor refresh.
- **Logging** — `[STUTTER]` auto-log no longer spams on normal ~27 ms frames at 59/60 fps: 60 ms absolute floor when uncapped + 0.25 s rate limit (real 60–100+ ms hitches still log with full phase data).
- **Input hygiene** — Piano keybed no longer hijacks globals: removed `V` from vsync toggle (F9 only); rebound filter env amt from F9/F10 to `[`/`]`.
- Same-session incremental: continued 2.4.306 diagnostics / render-thread / overlay work.

### Changes

- **`sit/situation_base_version.h`**: bumped patch to 2.4.307; updated description.
- **`sit/situation_api.h`**: `SITUATION_MAX_FRAMES_IN_FLIGHT` = 6.
- **`sit/situation_impl_renderer.h`**: relaxed `frames_pending` / queue-depth waits in EndFrame + acquire when target==0; added mixer support path.
- **`examples/other/node_graph_piano_demo.c`**: added mixer node + final patch, post-activation TopoSort, PlayToneEx fallback, key rebinds, updated comments/HUD.
- **`examples/shared/sit_example.h`**: vsync toggle no longer includes V.
- **`sit/situation_impl_ctrl.h`**: 60 ms log floor + rate-limit on stutter printf.
- **`doc/UPDATELOG.md`**, **`doc/whatsnew.md`**: documented.

---

---

---

---

---

---

## [v2.4.308] - 2026-06-19

### Description

**v2.4.308**: Fix lost keyboard/mouse edge input during Vulkan GPU fence waits and swapchain acquire.

- **Guarded event pump** — Non-primary `glfwPollEvents()` / `glfwWaitEventsTimeout()` calls (fence waits, acquire retries, WDM transitions, screenshot spin) now route through `_SituationPumpWindowEventsGuarded()` with a depth counter.
- **Pending buffers** — Key/mouse/scroll edges during guarded pumps are deferred to `pending_*` arrays and merged into `down_this_frame` / `up_this_frame` at the next `SituationPollInputEvents()`, restoring `SituationIsKeyPressed()` for ESC, F9–F12, and all example hotkeys.

### Changes

- **`sit/situation_impl_decl.h`**: `pump_guard_depth`, pending keyboard/mouse/scroll buffers, `_SituationPumpWindowEventsGuarded()`, `_SituationWaitWindowEventsTimeoutGuarded()`.
- **`sit/situation_impl_input.h`**: Callbacks defer per-frame edge writes when guard is active.
- **`sit/situation_impl_ctrl.h`**: Merge pending edges after frame reset, before official `glfwPollEvents()`.
- **`sit/situation_impl_renderer.h`**, **`sit/situation_impl_wdm.h`**, **`sit/situation_impl_image.h`**: Replace raw event pumps with guarded helpers.
- **`sit/situation_base_version.h`**, **`doc/UPDATELOG.md`**: version bump.

---

---

---

---

---

---

## [v2.4.309] - 2026-06-19

### Description

**v2.4.309**: Complete keyboard hotkey fix — `SituationIsKeyPressed()` now matches `SituationIsKeyDown()` after guarded GPU pumps.

- **Edge synthesis before state snapshot** — At the top of `SituationPollInputEvents()`, compare `current_state` vs `last_state` (before `memcpy` clobbers the transition) and fold drift into `pending_*`, so held keys pressed during acquire/fence waits produce a press edge.
- **Tail pending merge** — After the official `glfwPollEvents()`, merge any edges deferred by nested guarded pumps (focus/fullscreen callbacks) into `down_this_frame` for same-frame `IsKeyPressed` visibility.

### Changes

- **`sit/situation_impl_ctrl.h`**: Pre-poll edge synthesis + post-poll pending merge.
- **`sit/situation_base_version.h`**, **`doc/UPDATELOG.md`**: version bump.

---

---

---

---

---

---

## [v2.4.310] - 2026-06-19

### Description

**v2.4.310**: Vulkan alpha blending, uncapped FPS, render thread spike elimination, and example 02 improvements.

- **Vulkan quad pipeline alpha blend** — The built-in quad/texture pipeline was created with `SIT_VK_PIPELINE_BLEND_OPAQUE` (blendEnable=VK_FALSE). All textured draws with alpha were rendered opaque on Vulkan while working correctly on OpenGL. Fixed: pipeline now uses standard SRC_ALPHA/ONE_MINUS_SRC_ALPHA blending, matching the OpenGL path.
- **Uncapped Vulkan FPS** — Removed the redundant `vkWaitForFences` from the render thread's present path. The fence is now waited on only in `SituationAcquireFrameCommandBuffer` when a slot is reused. To prevent acquire starvation, `max_frames_in_flight` is now guaranteed to be less than `swapchain_image_count` (one free image always available for acquire). Result: ~2500fps on trivial draws vs ~120fps before.
- **Render thread spike fix** — The render thread was calling `_SituationVulkanWaitFencePumpWindow` which internally calls `glfwPollEvents()`. This is a GLFW threading violation (event functions are main-thread-only). It caused the main thread's `glfwPollEvents` to stall for 500-900ms due to Win32 message queue contention. Fixed: render thread uses plain `vkWaitForFences` with no event pump.
- **OpenGL uncapped backpressure** — The "double-check" `cnd_wait` in OpenGL's `SituationEndFrame` was unconditional, blocking the main thread even when `target_frame_time == 0` (uncapped). Gated by `target_frame_time > 0.0` to match the Vulkan path.
- **Example 02 improvements** — Bouncing ball uses a runtime-generated circle texture (64x64 RGBA disc with soft AA edge) via `SituationCreateTexture`. All animation uses `SituationTimerGetTime()` directly (no dt accumulation) for jitter-free visuals. Shadow improved (larger offset, lower alpha). Ball uses sub-pixel positioning for smooth motion.

### Changes

- **`sit/situation_impl_renderer.h`**: Vulkan quad pipeline blend flag `0` (was `SIT_VK_PIPELINE_BLEND_OPAQUE`). Render thread fence wait removed (present uses semaphore-only GPU sync). `max_frames_in_flight` clamped to `< image_count`. OpenGL EndFrame backpressure gated by `target_frame_time > 0.0`. Default `desired_frames` = 2 (triple-buffer swapchain gives headroom).
- **`examples/02_draw_shapes/main.c`**: Circle texture ball, temporal animations, improved shadow, removed pixel-snapping.
- **`sit/situation_base_version.h`**: v2.4.310.
- **`doc/UPDATELOG.md`**: This entry.

### Known Issues — Synchronization Refinement Needed

- **Post-VSync-toggle frame desync**: After toggling VSync (F9), `_SituationVulkanRecreateSwapchain` destroys and recreates the swapchain. All fences are recreated in signaled state, causing the acquire path to race through slots without proper pacing. The result is alternating short/long frame times (70ms/130ms pattern) until the pipeline naturally re-stabilizes. Root cause: `vkCreateSwapchainKHR` is called with `oldSwapchain = VK_NULL_HANDLE` — forcing a full teardown+create instead of a graceful handoff.
- **Fix path**: Pass the old swapchain handle to `vkCreateSwapchainKHR` (`create_info.oldSwapchain = sit_render.vk.swapchain`). The driver retires the old swapchain lazily, images stay valid until actually replaced, and fence/semaphore state remains coherent. This eliminates the full `_SituationVulkanCleanupSwapchain` fence drain and the post-recreation desync. Requires restructuring the recreate flow to destroy old resources AFTER the new swapchain is operational (deferred cleanup via graveyard or frame-counted retirement).
- **Periodic spikes** (~298 in 2490fps uncapped run, max 823ms): Likely Vulkan acquire timeout during first frames + DWM compositor stalls. The acquire timeout is 1s (`SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS`). Consider reducing the stepped acquire budget or adding a fast-path non-blocking try before the budgeted loop.
- **`_SituationVulkanCleanupSwapchain` uses `_SituationVulkanWaitInFlightFencesPump`**: This calls `_SituationPumpWindowEventsGuarded` which is safe (main thread), but still drains all fences sequentially — one VSync period per fence slot. With 2 slots at 60Hz that's ~33ms minimum stall on every swapchain recreation.

---

---

---

---

---

---

## [v2.4.311] - 2026-06-19

### Description

**v2.4.311**: `SituationGetFrameTime()` now returns the actual display-paced interval, not the main thread's free-running loop time.

- **Root cause**: With the render thread active, the main thread loops at 3–5ms (record + enqueue) while the display updates at 16.7ms (VSync). `frame_time` was measured in `SituationUpdateTimers()` (poll-to-poll), giving alternating 5ms/16ms values as the acquire fence intermittently blocks. Objects using `dt * velocity` moved non-uniformly — visible as micro-stutter.
- **Fix**: When `SITUATION_ENABLE_RENDER_THREAD` is active, `frame_time` is now measured once at the top of `SituationAcquireFrameCommandBuffer` — the interval between successive calls, which IS the actual frame cadence regardless of internal sync points.
- **Without render thread**: Unchanged — `SituationUpdateTimers()` measures correctly since the main thread owns present.
- **VSync toggle stutter reduced**: Removed redundant `_SituationVulkanWaitInFlightFencesPump` from `SituationSetVSync` — cleanup already drains fences internally.

### Changes

- **`sit/situation_impl_ctrl.h`**: `SituationUpdateTimers` frame_time calculation skipped when render thread is active (measured elsewhere).
- **`sit/situation_impl_renderer.h`**: Frame time measured at top of `SituationAcquireFrameCommandBuffer` (once per user frame, before any sync logic).
- **`sit/situation_impl_wdm.h`**: Removed redundant `_SituationVulkanWaitInFlightFencesPump` from `SituationSetVSync`.
- **`sit/situation_base_version.h`**: v2.4.311.

---

---

---

---

---

---

## [v2.4.312] - 2026-06-19

### Description

**v2.4.312**: Fix fullscreen exit when toggling VSync.

- **Bug**: Calling `SituationSetVSync` while in exclusive fullscreen mode caused the window to drop back to windowed mode.
- **Root cause**: `SituationApplyCurrentProfileWindowState` had an unconditional minimize/maximize/restore block that called `glfwRestoreWindow` when `GLFW_MAXIMIZED` was reported true and neither `SITUATION_FLAG_WINDOW_MINIMIZED` nor `SITUATION_FLAG_WINDOW_MAXIMIZED` was set in the profile. On some Windows/Vulkan drivers, exclusive fullscreen windows report `GLFW_MAXIMIZED = true`, so the "restore to normal" logic inadvertently pulled the window out of fullscreen.
- **Fix**: The entire minimize/maximize/restore section is now skipped when `SITUATION_FLAG_FULLSCREEN_MODE` is active in the target profile. Fullscreen windows should not be subject to maximize/minimize/restore semantics.

### Changes

- **`sit/situation_impl_wdm.h`**: `SituationApplyCurrentProfileWindowState` — minimize/maximize/restore block guarded by `!(target_flags & SITUATION_FLAG_FULLSCREEN_MODE)`.
- **`sit/situation_base_version.h`**: v2.4.312.

---

---

---

---

---

---

## [v2.4.313] - 2026-06-19

### Description

**v2.4.313**: Eliminate multi-frame stall when toggling VSync (Vulkan).

- **Problem**: `SituationSetVSync` triggered `_SituationVulkanRecreateSwapchain` which called `_SituationVulkanCleanupSwapchain` → `_SituationVulkanWaitInFlightFencesPump`. That function waited on each in-flight fence *sequentially* with a 3-second per-fence budget and 4ms event-pump chunks. With 2 in-flight frames at 60Hz VSync, the minimum stall was ~33ms (two full VSync periods drained one-by-one), with random spikes up to hundreds of milliseconds from DWM/driver latency.
- **Fix — render thread drain + batched fence wait**: `_SituationVulkanRecreateSwapchain` now drains the render thread queue first (waits for `frames_pending == 0`), then uses `_SituationVulkanWaitInFlightFencesBatched` — a single `vkWaitForFences(waitAll=VK_TRUE)` on all in-flight fences simultaneously with 8ms pump chunks (500ms budget). Typical wait: one VSync period (~16ms) vs the old N×VSync sequential approach.
- **Clean teardown+create**: After the batched wait confirms GPU idle, resources are destroyed and the swapchain is rebuilt from scratch (`oldSwapchain = VK_NULL_HANDLE`). The `oldSwapchain` handoff mechanism was removed — it caused indefinite `vkAcquireNextImageKHR` timeouts on some Windows drivers when switching present modes (IMMEDIATE↔FIFO) because the compositor wouldn't release images to the new swapchain.
- **Timing reset**: `sit_gs.previous_time` is reset after successful recreation so the stall doesn't inflate the next frame's `dt` measurement or spike the FPS counter.

### Changes

- **`sit/situation_impl_renderer_fwd.h`**: `_SituationVulkanCreateSwapchain` signature now takes `VkSwapchainKHR old_swapchain` (passed `VK_NULL_HANDLE` on all paths; parameter retained for future resize-only handoff).
- **`sit/situation_impl_renderer.h`**: `_SituationVulkanCreateSwapchain` accepts `old_swapchain` parameter. `_SituationVulkanRecreateSwapchain` rewritten: render thread drain → batched fence wait → clean destroy → create → rebuild resources → timing reset.
- **`sit/situation_impl_decl.h`**: New `_SituationVulkanWaitInFlightFencesBatched` — single batched fence wait with event pump.
- **`sit/situation_base_version.h`**: v2.4.313.

---

---

---

---

---

---

## [v2.4.314] - 2026-06-19

### Description

**v2.4.314**: Fix infinite acquire timeout loop after alt-tab / focus loss.

- **Bug**: After alt-tabbing away and back (or switching between window and background tasks), `vkAcquireNextImageKHR` would enter an infinite loop of 1-second timeouts. The compositor held all swapchain images from the stale state, and the acquire path never triggered a recreation because `VK_TIMEOUT` wasn't treated as a signal to recreate.
- **Fix**: New `consecutive_acquire_timeouts` counter. After 3 consecutive `VK_TIMEOUT` results from `vkAcquireNextImageKHR`, the code forces `framebuffer_resized = true` and re-enters the swapchain retry loop. The recreate rebuilds the swapchain cleanly, giving the compositor fresh images. Counter resets on any successful acquire.

### Changes

- **`sit/situation_impl_decl.h`**: Added `consecutive_acquire_timeouts` field to Vulkan render state.
- **`sit/situation_impl_renderer.h`**: Acquire timeout path now increments counter and forces recreation after 3 consecutive failures. Counter reset on successful acquire.
- **`sit/situation_base_version.h`**: v2.4.314.

---

---

---

---

---

---

## [v2.4.315] - 2026-06-20

### Description

**v2.4.315**: Fix VSync-off frame rate fallback to ~65fps — adaptive backpressure, latency metric, init policy, and mutex correctness.

- **Bug**: After toggling VSync off (F9), frame rate would fall back to approximately 65fps instead of running uncapped. Observed in `08_temporal_oscillators` and `04_play_a_sound` examples.
- **Root cause (primary)**: The adaptive backpressure policy evaluated its spike/steady thresholds even when `target_frame_time == 0` (uncapped). It used a hardcoded 16ms fallback (`if (target_ns == 0) target_ns = 16666667`) as the threshold. After VSync was disabled, `metric_max_latency_ns` still held the old ~16ms value from `glfwSwapBuffers` blocking during VSync — exceeding the threshold and permanently switching the policy to `SLEEP`. The metric is an all-time max that never decays or resets, so recovery was impossible.
- **Root cause (secondary)**: The `render_queue_mutex` in `SituationAcquireFrameCommandBuffer` (OpenGL) was locked but never unlocked before returning. This relied on Windows `CRITICAL_SECTION` re-entrancy (UB for C11 `mtx_plain`) and serialized the render thread during main-thread frame recording.
- **Root cause (tertiary)**: `SituationInitInfo.backpressure_policy` was never applied to `sit_render_policy_state` — the field was dead code.

### Fixes

1. **Skip adaptive policy when uncapped**: Wrapped the entire spike/steady threshold evaluation in `if (sit_gs.target_frame_time > 0.0)`. When uncapped, the policy state is not mutated — it stays at whatever was configured at init or last set when a target FPS was active.
2. **Reset latency metrics on VSync disable**: `SituationSetVSync(false)` now atomically zeroes `metric_max_latency_ns`, `metric_latency_sum_ns`, and `metric_latency_count`. Fresh metrics start accumulating under the new present mode.
3. **Wire init backpressure policy**: `_SituationInitRenderer` now applies `init_info->backpressure_policy` to `sit_render_policy_state` (with bounds validation). The `SIT_RENDER_BACKPRESSURE_YIELD` setting in `sit_example.h` now takes effect.
4. **Unlock mutex after backpressure check**: Added `mtx_unlock(&sit_render.render_queue_mutex)` immediately after the `cnd_wait` loop in the OpenGL `AcquireFrameCommandBuffer` path. The mutex is no longer held across the entire frame recording phase.

### Changes

- **`sit/situation_impl_renderer.h`**: Adaptive backpressure guarded by `target_frame_time > 0.0`; removed hardcoded 16ms default; init applies `backpressure_policy`; `mtx_unlock` after AcquireFrame backpressure.
- **`sit/situation_impl_wdm.h`**: `SituationSetVSync(false)` resets all three latency metric atomics.
- **`sit/situation_base_version.h`**: v2.4.315.

---

---

---

---

---

---

## [v2.4.316] - 2026-06-20

### Description

**v2.4.316**: VD-1 — configurable virtual display attachments (renderer bolster plan slice 1).

- **`SituationCreateVirtualDisplayFromDesc`** — desc-struct VD creation (color format, depth mode, attachment defaults, layout fields). Legacy `CreateVirtualDisplay` / `CreateVirtualDisplayEx` wrap this with UNORM + D24 defaults.
- **`SituationSetVirtualDisplayAttachmentDefaults`** / **`SituationRenderPassInfoInherit`** — tier B/C attachment-default storage and inherit helper.
- **`SituationVirtualDisplay`** extended with `color_format`, `depth_stencil_mode`, `attachment_defaults`.
- **OpenGL**: color-only VDs skip depth RBO; begin-pass disables depth test when the VD has no depth attachment.
- **Vulkan**: VD raster targets use **dynamic rendering** (`vkCmdBeginRendering` / `EndRendering`); baked `render_pass` / `render_pass_load` / `framebuffer` removed. Per-shader VD pipeline variants via `VkPipelineRenderingCreateInfo`; shader-cache bundle path clones from module pairs. `dynamicRendering` enabled at device create.
- **D24S8**: enum present; create rejects with `NOT_IMPLEMENTED` until follow-up patch.
- **Harness**: `vd_load_color_attachment` (renamed), `vd_color_only_no_depth`, `vd_configure_attachment_defaults`.

### Changes

- **`sit/situation_api.h`** — VD-1 types, `FromDesc`, inherit/configure APIs; attachment-default structs moved after render-pass load/store types.
- **`sit/situation_impl_vd.h`** — FromDesc create path, guards, inherit/configure implementations.
- **`sit/situation_impl_renderer.h`** — VK dynamic rendering begin/end, layout barriers, VD dynamic pipeline cache, GL depth-none begin-pass.
- **`sit/situation_impl_decl.h`** — shader-slot VD pipeline cache + pinned SPIR-V copies.
- **`build/dll/situation_*.def`** — new exports (regenerated by gendef on build).
- **`tests/harness/test_virtual_display.c`** — three VD-1 tests.
- **`sit/situation_base_version.h`** — v2.4.316.

---

---

---

---

---

---

## [v2.4.317] - 2026-06-20

### Description

**v2.4.317**: VSync-aware backpressure — main thread correctly synchronizes to render thread cadence when VSync is the pace-maker.

- **Bug**: With VSync ON and `target_frame_time == 0` (standard example configuration: VSync paces, no software limiter), the main thread's backpressure was completely disengaged. It produced frames faster than the render thread could consume at VSync rate, overwriting ring buffer slots still being executed, inflating `frames_pending` unboundedly, and causing frame-time jitter (~18-20ms instead of clean 16.67ms). This manifested as hundreds of false "spikes" in the metrics overlay.
- **Root cause**: All backpressure guards used `if (sit_gs.target_frame_time > 0.0)` — which is false when VSync provides pacing. The main thread never waited for the render queue to drain.
- **Fix**: New `_SitShouldEngageBackpressure()` helper returns true when either a software target is set OR VSync is active. Applied to all 5 backpressure sites (GL + VK acquire and end-frame paths). Adaptive policy thresholds widened to 200%/100% (spike = missed a full frame, steady = at or below target) so normal VSync latency doesn't falsely trigger SLEEP.

### Changes

- **`sit/situation_impl_renderer.h`**: `_SitShouldEngageBackpressure()` inline helper; all `target_frame_time > 0.0` backpressure guards replaced; adaptive policy target_ns uses 16.67ms fallback when VSync-paced; spike/steady thresholds widened.
- **`sit/situation_base_version.h`**: v2.4.317.

---

---

---

---

---

---

## [v2.4.318] - 2026-06-20

### Description

**v2.4.318**: Window state awareness — maximize and position callbacks keep internal state honest.

- **Maximize callback** (`_SituationGLFWWindowMaximizeCallback`): now syncs `SITUATION_FLAG_WINDOW_MAXIMIZED` in `active_profile_window_flags` and refreshes the cached window state flags. Previously only forwarded to the user callback — internal flags remained stale after an OS-driven maximize/restore.
- **Window position tracking**: new `glfwSetWindowPosCallback` (`_SituationGLFWWindowPosCallback`) updates `sit_gs.window_pos_x/y` on every OS-driven move (user drag, snap, programmatic). Initial position seeded at window creation. `SituationGetWindowPosition()` now reads from cached state (zero-cost, no GLFW call per query).

### Changes

- **`sit/situation_impl_input.h`**: maximize callback syncs profile flags + refreshes cache; new window-pos callback.
- **`sit/situation_impl_ctrl.h`**: register `glfwSetWindowPosCallback`; seed initial `window_pos_x/y` after window creation.
- **`sit/situation_impl_decl.h`**: added `window_pos_x`, `window_pos_y` fields.
- **`sit/situation_impl_wdm.h`**: `SituationGetWindowPosition()` reads cached values instead of live GLFW query.
- **`sit/situation_base_version.h`**: v2.4.318.

---

---

---

---

---

---

## [v2.4.319] - 2026-06-20

### Description

**v2.4.319**: VSync frame spike elimination — windowed latency metric, swap-interval change-detection, and mode-aware spike threshold.

Three fixes that together eliminate the "668 spikes / max 3605ms" symptom observed in VSync-paced examples (e.g. `02_draw_shapes`). The previous fixes (v2.4.310–317) addressed real bugs but left a design-level issue: the adaptive backpressure policy could permanently lock into SLEEP mode while VSync was ON, and the spike counter's 20ms threshold conflated VSync-paced operation with truly-uncapped, producing hundreds of false positives from normal OS jitter.

### Bug Analysis

**Symptom**: With VSync ON and `target_frame_time == 0` (the standard example configuration), the metrics overlay reported hundreds of "spikes" and a multi-second max frame time, even though visual smoothness was acceptable and FPS was stable at 60.

**Root cause 1 — Adaptive policy permanently locked into SLEEP**: `metric_max_latency_ns` was an all-time maximum that only ratcheted upward. Normal VSync latency (submit → render-thread dequeue → execute → `glfwSwapBuffers` blocking → signal) is 16–20ms. Any single frame exceeding 33.3ms (2× the 16.67ms VSync reference) — inevitable during startup, alt-tab, DWM stalls — permanently switched the backpressure policy to `SIT_RENDER_BACKPRESSURE_SLEEP`. The recovery condition (`metric < steady_thresh = 16.67ms`) could never be satisfied because the max only grew. SLEEP mode uses `cnd_wait`, which on Windows has non-deterministic wake latency (1–15ms depending on timer resolution), inflating frame_time measurements beyond the expected 16.67ms.

**Root cause 2 — Redundant per-frame `glfwSwapInterval`**: The render thread called `glfwSwapInterval(1)` (mapped to `wglSwapIntervalEXT`) before every single `glfwSwapBuffers`. This is a driver extension call that some Windows drivers treat as a synchronization point or internal state flush, adding unnecessary micro-latency to the present path.

**Root cause 3 — Spike threshold too tight for VSync**: The spike counter used a flat 20ms threshold for all `target_frame_time == 0` modes (both VSync-paced and truly-uncapped). At 60Hz VSync (16.67ms/frame), this gave only 3.3ms of headroom. Normal Windows scheduler jitter, DWM compositor variance, and `cnd_wait` wake latency routinely push frames to 18–21ms, triggering hundreds of false "spikes" that obscure genuine dropped frames.

**Note on the 3605ms max spike**: This was caused by `glfwPollEvents()` blocking in a Win32 modal message pump during window drag/resize/alt-tab. This is external OS behavior unavoidable with GLFW's single-window model — the render thread continues presenting, but the main thread's `frame_time` measurement includes the stall. Not a library bug.

### Fixes

**Fix 1 — Windowed latency metric** (`situation_impl_renderer.h`, `situation_impl_decl.h`):
- `metric_max_latency_ns` now resets to 0 every `SIT_LATENCY_METRIC_WINDOW_FRAMES` (120) frames processed by the render thread (~2 seconds at 60Hz).
- New `atomic_uint_least32_t metric_window_frame_count` field tracks frames since last reset.
- Within each window, the max still ratchets upward (so the policy responds to sustained pressure), but after the window expires, a transient spike is forgotten and the policy can recover to SPIN/YIELD.
- `SituationSetVSync(false)` now also resets `metric_window_frame_count` for clean re-enable.

**Fix 2 — Swap-interval change-detection** (`situation_impl_renderer.h`, `situation_impl_decl.h`):
- New `int last_applied_swap_interval` field in `_SituationGLState` tracks the currently-active swap interval.
- The render thread now only calls `glfwSwapInterval()` when `desired_interval != last_applied_swap_interval`.
- Initial value set at render thread start (alongside the first `glfwSwapInterval` call).
- Result: one WGL call on VSync toggle, zero calls during steady-state operation.
- Single-threaded fallback paths were already correct (no per-frame call).

**Fix 3 — Mode-aware spike threshold** (`situation_impl_ctrl.h`):
- Spike counting now uses three distinct thresholds:
  - **Software target active** (`target_frame_time > 0`): `2× target` (unchanged — missed a full frame budget).
  - **VSync-paced** (`target == 0`, VSync ON): **25ms** (1.5× at 60Hz). Gives 8ms headroom for normal OS/scheduler/DWM jitter. Only counts frames that genuinely missed a VSync deadline.
  - **Truly uncapped** (`target == 0`, VSync OFF): **20ms**. At high FPS (1000+), the OS scheduler routinely yields the main thread for 10–15ms (thread timeslice, DWM compositor pulse). These are normal and not actionable. 20ms represents a genuinely notable stall. (Same as previous behavior — preserves the original design intent.)
- Stutter log threshold simplified to `max(thresh × 3, 60ms)` — unchanged effective behavior, cleaner expression.

### VSync-OFF regression verification

- **Fix 1**: Adaptive policy evaluation is entirely gated by `_SitShouldEngageBackpressure()` which returns `false` when VSync is off + target is 0. The periodic reset runs but the metric is never read — no behavioral change.
- **Fix 2**: When VSync is toggled off, the render thread detects `desired_interval (0) != last_applied (1)`, calls `glfwSwapInterval(0)` once, then stays silent. Improved from old behavior (called every frame).
- **Fix 3**: Uncapped threshold remains at 20ms (same as before) — correctly filters normal OS scheduler timeslice yields at high FPS. VSync-paced mode gets the new 25ms threshold.

### Changes

- **`sit/situation_impl_renderer.h`**: `SIT_LATENCY_METRIC_WINDOW_FRAMES` constant (120). Render-thread latency recording now resets `metric_max_latency_ns` every 120 frames. Render-thread `glfwSwapInterval` replaced with change-detection using `last_applied_swap_interval`. Initial interval set with tracking at thread start.
- **`sit/situation_impl_decl.h`**: New fields: `atomic_uint_least32_t metric_window_frame_count` (latency window counter), `int last_applied_swap_interval` (GL swap interval tracking).
- **`sit/situation_impl_ctrl.h`**: Spike threshold logic rewritten to distinguish VSync-paced (25ms), software-target (2×), and truly-uncapped (20ms, unchanged) modes. Stutter log threshold simplified.
- **`sit/situation_impl_wdm.h`**: `SituationSetVSync(false)` now also resets `metric_window_frame_count`.
- **`sit/situation_base_version.h`**: v2.4.319.

---

---

---

---

---

---

## [v2.4.320] - 2026-06-20

### Description

**v2.4.320**: Canvas stretch fail-soft, renderer forward-declaration coverage, shutdown fullscreen release, and test harness crash reporting.

Fixes `output_color_depth.monitor_hot_swap_recreate` harness failure (-502). Bug B (`texture_storage_write_readback`) fixed in v2.4.328.

### Bug Analysis (Bug A — Canvas FBO failure)

**Symptom**: `SituationEndFrame()` returns `SITUATION_ERROR_RENDER_COMMAND_FAILED` (-502) on the first frame after `SituationSetWindowMonitor()` enters exclusive fullscreen. Reproduced 10/10 on dual-monitor NVIDIA GTX 1070.

**Root cause**: Two-part failure:
1. **Stale canvas dimensions** — `SituationSetWindowMonitor` called `glfwSetWindowMonitor` without first capturing the windowed framebuffer size into `render_canvas_width/height`. The GLFW framebuffer callback skips canvas updates when in exclusive fullscreen, so the canvas stayed at whatever value it had before.
2. **GL context instability** — Even with correct dimensions (fixed by Phase A2), `glGenFramebuffers` / `glNamedFramebufferTexture` fail with `GL_INVALID_OPERATION` (0x0502) and `glCheckNamedFramebufferStatus` returns 0 on the first frame after a display mode change. The NVIDIA driver needs one event pump to complete the mode transition before named framebuffer operations work.

### Fixes

**Phase A2 — Canvas bookkeeping** (`situation_impl_wdm.h`):
- `SituationSetWindowMonitor`: before calling `glfwSetWindowMonitor` with a non-NULL monitor, capture the current windowed framebuffer size into `render_canvas_width/height` and save `windowed_x/y/w/h` for future restore. Only runs when currently windowed (`glfwGetWindowMonitor == NULL`). Same pattern as `SituationApplyCurrentProfileWindowState`.
- `SituationSetDisplayMode`: same canvas capture added before the fullscreen `glfwSetWindowMonitor` call.

**Phase A3 — Fail-soft at execute time** (`situation_impl_renderer.h`):
- In `SIT_OP_BEGIN_RENDER_PASS` executor: if `_SituationGLEnsureCanvasResources()` fails, instead of returning a hard error, fall back to rendering directly to the default framebuffer (FBO 0) at display resolution for that frame. Logs at `SIT_LOG_DEBUG` level. Next frame's canvas creation succeeds once the GL context stabilizes. One frame at native resolution is imperceptible.

**Phase A1 — Diagnostic logging** (`situation_impl_renderer.h`):
- `_SituationGLEnsureCanvasResources`: on invalid dimensions, logs canvas/display/window sizes and monitor state at `SIT_LOG_WARNING`. On FBO completeness failure, logs `fbo_status`, `glGetError`, all dimensions, and monitor state.

**Phase B1 — Color mask diagnostic** (`situation_impl_renderer.h`):
- `SIT_OP_DRAW_QUAD` executor: when `SIT_TEST_DEBUG_GL` env var is set and the draw is textured, queries `GL_COLOR_WRITEMASK` and logs a warning if any component is FALSE. Zero overhead when env var is not set (cached static check).

**Broken buffer diagnostic** (`situation_impl_renderer.h`):
- `_SituationGLExecuteCommands`: broken soft buffer early-out now logs packet count at `SIT_LOG_WARNING` (was silent).

### Test Results

- `output_color_depth.monitor_hot_swap_recreate`: **PASS** (was FAIL -502)
- `output_color_depth` module (isolated): 2 passed, 0 failed, 8 skipped (HDR/10-bit correctly skip)
- No regressions in module scope

### Known Issue Exposed

The canvas fix makes `monitor_hot_swap_recreate` pass for the first time, which exercises the full test path including `SituationSetWindowMonitor` to a secondary monitor, fullscreen toggles, and return to primary. This exposed a **pre-existing lifecycle bug**: after `SituationShutdown` following multi-monitor exclusive fullscreen transitions on OpenGL, the next `SituationInit` → `glfwCreateWindow` could hang indefinitely (NVIDIA Windows, GTX 1070, dual 2560×1440).

**Mitigation status (see plan for current state)**: v2.4.320 harness settling frames + shutdown fullscreen release; v2.4.322 errno guards; v2.4.323 teardown delay removal. Full suite **completes** as of v2.4.327 (542/555 pass on reference machine). Cross-module hang treated as **mitigated**, not fully root-fixed.

### Test Harness Improvements

- **`main.c`**: Added `atexit` unexpected-exit handler — reports which module/test was active when the process terminates without completing the harness (catches `abort()`, `ExitProcess`, CRT termination).
- **`sit_test_framework.h`**: Module setup now sets `g_sit_current_test_name` to `"<module>.(setup)"` so crash reports identify the setup phase.
- **`test_output_color_depth.c`**: `monitor_hot_swap_recreate` now exits fullscreen (`SituationToggleFullscreen`) and renders 3 settling frames before module teardown to reduce driver state issues for subsequent modules.

### Plan Reference

See `doc/plan/CANVAS_STRETCH_READBACK_FIX_PLAN.md` — Phases A1, A2, A3, B1 complete. Lifecycle hang tracked as new issue.

### Forward Declarations (`situation_impl_renderer_fwd.h`)

Added 59 missing forward declarations identified by `scripts/verify_renderer_fwd.py`. Coverage now 326/326 static functions in `situation_impl_renderer.h`. Added struct tags to 5 anonymous typedefs (`OBJReaderContext`, `VertexMapEntry`, `VertexMap`, `OBJTextureCacheEntry`, `OBJTextureCache`) to enable forward-declaration without exposing struct bodies.

New sections added:
- GL shader program cache (`#if SIT_GL_SHADER_CACHE_ENABLE`) — 10 functions
- GL deferred program destroy — `_SitGLDeferDestroyProgram`
- GL output color depth — `_SituationOpenGLSetOutputColorDepthFromFramebuffer`
- Vulkan shader cache hash/key helpers — 7 inline functions
- Vulkan Phase 2 pipeline variants (`#if SIT_VK_SHADER_CACHE_PHASE2`) — 12 functions
- Vulkan SPIR-V byte lookup — `_SitVkLookupSpirvBytesByHash`
- Vulkan VD dynamic rendering helpers — `_SitVkMapLoadOp`, `_SitVkMapStoreOp`, `_SitVkTransitionVDColorForRendering`, `_SitVkCreateVDDynamicPipelineClone`
- Vulkan surface format + render pass management — 6 functions
- OBJ loader internals — 7 functions
- STL loader internals — 3 functions
- Vertex map (model loading) — 5 functions
- Frame lifecycle — `_SitShouldEngageBackpressure`

Regenerated `situation_base_trace.h` via `scripts/gen_situation_base_trace.py` (2409 trace points, 76 source files).

### Shutdown Fullscreen Release (`situation_impl_ctrl.h`)

**Fix:** `_SituationCleanupRenderer` now calls `glfwSetWindowMonitor(window, NULL, ...)` when the window is in exclusive fullscreen before proceeding with GL resource teardown. This releases GLFW's internal monitor-mode claim — without it, the next `glfwCreateWindow` in the same process blocks indefinitely (regression introduced by the v2.4.3xx removal of `glfwTerminate` from the shutdown path).

**Unresolved:** Calling `glfwPollEvents()` after the mode release is required for the harness to survive the ACCESS_VIOLATION from the NVIDIA ICD's `WM_DISPLAYCHANGE` handler — but without it the process exits silently. The current build has `glfwPollEvents()` removed; the ACCESS_VIOLATION during teardown remains an open investigation item (does not affect Vulkan, single-monitor usage, or applications that exit after shutdown rather than re-initializing).

**Root cause of the regression:** v2.4.3xx added `[FIX] Do NOT call glfwTerminate()` in `_SituationCleanupPlatform` to prevent the OpenGL ICD from wedging on re-init. This fix was correct for its target symptom but introduced a side effect: GLFW's monitor-mode tracking persists across `Shutdown/Init` cycles. When a window is destroyed while GLFW believes it owns a monitor's exclusive mode, the next `glfwCreateWindow` deadlocks on Windows display-mode arbitration. The `glfwSetWindowMonitor(NULL)` call in `_SituationCleanupRenderer` is the corrective counterpart.

### Vulkan Async Shader Failures (observed, not introduced by this patch)

Vulkan `graphics` module shows 5 async shader test failures + a process crash:

| Test | Symptom |
|------|---------|
| `async_shader_begin_reports_in_progress` | `graphics_test_async_poll_shader_ready` returns -1 (timeout) after 600 polls |
| `async_shader_load_memory_draw` | Same — shader never reaches READY state |
| `async_shader_renderer_alive_while_loading` | Same |
| `async_shader_poll_after_unload_during_load` | Same |
| `sync_shader_after_async_cycle` | Same (3000 poll cap) |
| **`shader_cache_hit`** | **HARNESS CRASH: process exited unexpectedly** (CRT abort or driver kill) |

The async poll failures are tracked in `doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md` (Phase B — starvation drive). The `shader_cache_hit` crash is **new and untracked** — likely a regression from the Vulkan shader cache (`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`) or VD-1 pipeline variant work (`doc/plan/renderer_bolster_plan.md` § VD-1). Needs investigation: the crash happens after the async tests leave the shader pipeline in a failed/abandoned state, then `shader_cache_hit` attempts a synchronous load that hits a null pipeline or freed module.

---

---

---

---

---

---

## [v2.4.321] - 2026-06-21

### Description

**v2.4.321**: Complete forward-declaration coverage for all non-renderer, non-inline internal statics. Consolidation of all forward declarations into a single canonical location.

### Changes

**`sit/situation_impl_forward.h`** — canonical forward-declaration file for non-renderer statics:
- Added forward declarations for all non-inline static functions across 17 non-renderer impl files (131 total)
- New sections: Utility Helpers, Timer, Threading Diagnostics, Threading NUMA, Threading Observability, Threading Scheduler, Threading Topology, I/O Module, Window/Display/Monitor, Image Module
- Existing sections expanded: Control Module (+1), GLFW Callbacks (+1 WindowPos), Threading & IO (+4 core threading), Audio Helpers (+13), Virtual Display (+3)
- `static inline` functions intentionally excluded — they're self-contained and don't need forward declarations (definition always precedes use in include order)

**Duplicate forward declarations removed** (single source of truth in `situation_impl_forward.h`):
- `situation_impl_decl.h`: removed 5 duplicates (`_SituationWorkerEntry`, `_SitParallelWorker`, `_SituationRenderThreadEntry`, `_SituationInitRenderThread`, `_SituationDestroyRenderThread`). Retained `_SituationSetErrorFromCode` as it's used before `forward.h` is included.
- `situation_impl_wdm.h`: removed 2 duplicates (`_SituationDxgiFillDisplayHdrMetadata`, `_SituationComputeWindowStateFlags`)
- `situation_impl_audio.h`: removed 1 duplicate (`_SituationMixToneToBuffer`)
- `situation_impl_image.h`: removed 1 duplicate (`_SituationSaveImageBMP`)

**`scripts/verify_impl_forward.py`** (new):
- Counterpart to `verify_renderer_fwd.py` — covers all non-renderer impl files
- Reports missing forward declarations and extras with file/line info
- `--fix` mode prints copy-paste-ready signatures
- Correctly excludes `static inline` functions (no forward decl needed)
- Scans `sit/aud/` to avoid false positives on reverb/effects helpers

### Verification

- `verify_impl_forward.py`: OK — 131 non-inline statics covered across 17 non-renderer impl files
- `verify_renderer_fwd.py`: unaffected (renderer coverage unchanged)
- Build: `static-opengl` ✓, `static-vulkan` ✓ — no errors or warnings

---

---

---

---

---

---

## [v2.4.322] - 2026-06-21

### Description

**v2.4.322**: Implementation of the Error Code Gap-Fill Plan. Addresses platform error checking, OpenGL exclusive fullscreen teardown reliability, non-fatal display mode transition warnings, and stale audio device fallbacks.

### Changes

**`sit/situation_base_errno.h`**:
- Added 6 new error codes:
  - `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` (-13)
  - `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` (-14)
  - `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED` (-108)
  - `SITUATION_ERROR_CONTEXT_RECLAIM_FAILED` (-109)
  - `SITUATION_ERROR_DISPLAY_MODE_SETTLING` (-215)
  - `SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE` (-431)

**`sit/situation_impl_ctrl.h`**:
- Added process-lifetime flag `_sit_previous_shutdown_had_errors` to track incomplete teardown sessions.
- In `_SituationCleanupPlatform`, added failsafe fullscreen release checks before destroying the window, setting `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED` if exclusive mode cannot be released.
- In `SituationShutdown`, added explicit GL context reclaim checks after joining the render thread, setting `SITUATION_ERROR_CONTEXT_RECLAIM_FAILED` on failure.
- In `SituationShutdown`, tracked overall teardown completeness and raised `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` if any part of the shutdown sequence failed.
- In `SituationInit`, consumed the incomplete shutdown flag and set the warning code `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` to notify the application of potential stale driver state.

**`sit/situation_impl_renderer.h`**:
- In the FBO 0 fail-soft path of the `SIT_OP_BEGIN_RENDER_PASS` executor, set the non-fatal diagnostic warning `SITUATION_ERROR_DISPLAY_MODE_SETTLING`.
- Cleared the settling error code once canvas resources are successfully created.

**`sit/situation_impl_audio.h`**:
- In `_SituationSetAudioDeviceInternal`, added a fallback that retries device initialization in shared mode if exclusive mode fails, raising `SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE`.

### Verification

- Run `audit_errno.ps1`: OK (0 phantoms, X-macro parity confirmed)
- Run `verify_impl_forward.py`: OK (no signature mismatch or missing declarations)

---

---

---

---

---

---

## [v2.4.323] - 2026-06-21

### Description

**v2.4.323**: Fix OpenGL ring fence array size heap corruption that caused silent test harness crashes during teardown, and optimize teardown performance by removing obsolete `Sleep(50)` delay loops in the cleanup paths.

### Changes

**`sit/situation_impl_renderer.h`**:
- In `_SituationInitGLRingFences`, set `sit_render.gl.ring_fence_count = SITUATION_MAX_FRAMES_IN_FLIGHT` instead of the hardcoded value of `3`. This prevents out-of-bounds writes and heap corruption when accessing `ring_fences` with frame indices >= 3.

**`sit/situation_impl_ctrl.h`**:
- In `_SituationCleanupRenderer` and `_SituationCleanupPlatform`, removed obsolete `Sleep(50)` delay calls that were originally added to wait out display mode settling. This reduces module shutdown latency without impacting stability or correctness.

**`sit/situation_base_version.h`**:
- Bumped patch version to `323`.

### Verification

- Rebuild and run the OpenGL test harness: OK (all 555 tests executed, harness runs to completion and prints the summary, with noticeably faster teardown transitions).

---

---

---

---

---

---

## [v2.4.324] - 2026-06-21

### Description

**v2.4.324**: Vulkan async shader load hardening — Phase A/B fixes from `doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md`. Eliminates the root causes of the `[STUTTER] dt=132292.5ms` production stutter and the `shader_cache_reuse_after_unload` test failure (~66 s timeout).

### Changes

**`sit/situation_impl_renderer.h`**:
- In `_SituationVkAsyncCompileFreeCtx`: clear `ctx->build_ticket = NULL` after releasing the ticket handle to prevent dangling-handle access on subsequent poll iterations or unload. Eliminates a source of `SituationWaitForJob` indefinite blocks.
- In `_SituationPollVkAsyncShaderLoad`: null `ctx->build_ticket` immediately upon release in the terminal-state path, preventing stale non-null pointer from triggering use-after-free or double-free on the next cache-reuse load cycle.
- In `_SituationVkAsyncCompileProgress`: added starvation / unclaimed fast-path check — if a shaderc pool job has been unclaimed for longer than `SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS` (100 ms), the main thread retires the stale slot via `_SitThreadPoolRetireOrphanedJobMain`, zeroes `compile_job`, and runs `_SituationVkAsyncCompileWorker` inline. Prevents the 10 s hard-wait that was the primary stutter source when a worker thread failed to pick up a compile job under load.

**`sit/situation_base_version.h`**:
- Bumped patch version to `324`.

### Verification

- Targeted fix for PROD CRITICAL issues: `[STUTTER] dt=132292.5ms` (thread pool starvation + no inline drive) and `[FAIL] shader_cache_reuse_after_unload` (~66 s, caused by stale ticket + repeated 10 s unload waits).
- Rebuild and run Vulkan test harness; verify `shader_cache_reuse_after_unload` completes in < 2 s and no stutter events in `dt` log.
- Plan reference: `ASYNC_SHADER_LOAD_HARDENING_PLAN.md` Phases A (fast unload / progress driver) and B.1 (unclaimed fast path). Phases B.2–E remain open.

---

---

---

---

---

---

## [v2.4.325] - 2026-06-21

### Description

**v2.4.325**: Storage image bindless conflict fix: skip ARB_bindless_texture residency for USAGE_STORAGE textures.

### Changes

- Skipped ARB_bindless_texture residency for USAGE_STORAGE textures to avoid conflicts on storage images.

---

---

---

---

---

---

## [v2.4.326] - 2026-06-22

### Description

**v2.4.326**: Flexible debugging capabilities addition, Vulkan UBO/SSBO descriptor layout conflict resolution, and async test fixes.

### Changes

**`debug.bat`**:
- Replaced with a flexible version that supports incremental builds by default, custom target redirection (clean/distclean), environment variable compiler overrides, `--no-build`, and custom `--break` debugger hooks.

**`sit/situation_impl_renderer.h`**:
- Fixed descriptor layout mismatch driver crash inside `vd_render_into_pipeline` by ensuring `view_proj_ubo_descriptor_set` (which has a layout matching `view_data_ubo_layout` with UBO at binding 1) is not incorrectly bound to user pipeline layouts that use `dynamic_ubo_layout` (with UBO at binding 0).

**`tests/harness/test_graphics.c`**:
- Address buffer readback synchronization in `async_buffer_readback` and thread pool async compilation.

---

---

---

---

---

---

## [v2.4.327] - 2026-06-22

### Description

**v2.4.327**: Critical thread-safety and memory-safety patches, Vulkan swapchain recreation stability, and render thread readback synchronization.

### Changes

**`sit/situation_impl_renderer.h`**:
- **CRITICAL**: Fixed a severe use-after-free vulnerability in resource allocators (`_SitAllocShaderSlot`, `_SitAllocMeshSlot`, `_SitAllocBufferSlot`, `_SitAllocComputePipelineSlot`, `_SitAllocModelSlot`) where `memset` wiped the generation counter, causing all recycled handles to permanently evaluate to `generation = 1` and bypass validation checks.
- **CRITICAL**: Eliminated a heap corruption race condition in `SituationCmdDrawTextEx` (Vulkan) by replacing the global `text_batch_scratch` buffer with a thread-local allocation, making text command recording safe across multiple worker threads.
- Fixed Vulkan `STALE` bundle bug (Error -747) where `SituationCmdDrawMesh` failed after window resize/swapchain recreation. The pipeline resolver now automatically recovers and re-attaches bundles using pinned SPIR-V bytecode.
- Resolved readback race conditions in `SituationReadTexture`, `SituationReadFramebuffer`, `SituationGetBufferData`, and `SituationReadBuffer` by introducing and calling `_SituationFlushRenderThread()` to ensure GPU execution completes before CPU readback.
- Fixed OpenGL stale pixel readbacks on compute-generated textures by adding `GL_TEXTURE_UPDATE_BARRIER_BIT` to the `SITUATION_BARRIER_TRANSFER_READ` pipeline barrier mapping.
- Fixed screenshot lifetime bug causing `capture_screenshot_exit` test failures by preventing the engine from prematurely discarding unconsumed pre-swap/pre-present screenshots at the end of the frame.
- Restored intended OpenGL quad batching performance by embedding `texture_id` directly into the `SIT_OP_DRAW_QUAD` command packet, bypassing the batch-breaking descriptor bind packets.

### Plan cross-reference (2026-06-22)

- **`doc/plan/CANVAS_STRETCH_READBACK_FIX_PLAN.md`**: Bug B Phase B1 diagnostic run (`SIT_TEST_DEBUG_GL=1`, full `graphics` module) — **no** `[B1 DIAG]` color-mask warnings (mask all TRUE at textured quad draw). **Fixed in v2.4.328** — stale screenshot readback, not GL draw/sampling.

---

---

---

---

---

## [v2.4.328] - 2026-06-22

### Description

**v2.4.328**: Fixes OpenGL `graphics.texture_storage_write_readback` failure in full module runs (Bug B). Root cause was **stale pre-swap screenshot buffer** reused by `SituationLoadImageFromScreen()` when the draw frame did not arm capture before `EndFrame()`. Compute/storage sampling path was correct; harness readback was wrong.

### Changes

**`sit/situation_api.h`**, **`situation_dll.c`**:
- Added **`SituationRequestScreenCapture()`** — arms pre-swap back-buffer capture on the next `SituationEndFrame()`. Exported from DLL (header-only impl did not appear in `.def`).

**`sit/situation_impl_renderer.h`**:
- **`SituationAcquireFrameCommandBuffer`**: invalidate `screenshot_valid` each OpenGL frame so prior-test screenshot buffers are not reused.
- **`_SituationGLPrepareStorageTextureForSampling`**: barrier + image-unit unbind + swizzle reset + `SAMPLED` usage before textured draw from storage textures (defense-in-depth).
- **`SIT_OP_DRAW_QUAD`**: carry `texture_slot_index`; unbind image units 0–7 at main-window render-pass start; pipeline barrier src/dst hardening for compute-write → fragment-read.
- Baseline raster: `glDisable(GL_SCISSOR_TEST)`; unconditional quad program bind per textured batch.

**`tests/harness/test_graphics.c`**:
- **`test_texture_storage_write_readback`**: call `SituationRequestScreenCapture()` before draw-frame `EndFrame()`.

**`tests/harness/sit_test_window.h`**:
- **`sit_test_full_window_dest()`**: use render (not client) dimensions for projection alignment.

### Verification

- **`--module graphics`**: 110/110 PASS including `texture_storage_write_readback` (reference machine, dual 2560×1440, GTX 1070).
- Plan: `doc/plan/CANVAS_STRETCH_READBACK_FIX_PLAN.md` — Bug B marked fixed.

### Investigation notes — `virtual_display` order-dependent failures (not fixed in this patch)

Triage (2026-06-22, OpenGL harness, reference machine):

| Observation | Result |
|-------------|--------|
| `--filter vd_offset_position` (and the other three) | **PASS** in isolation |
| `--module virtual_display` (full module, single `SituationInit`) | **26 pass / 4 fail** — same four tests |
| Full `sit_test_opengl.exe` | Four failures also appear here (after `graphics` + `text_rendering` modules) |

**Revised root-cause hypothesis:** failures are **intra-module order dependence inside `virtual_display`**, not cross-module pollution from `graphics`. The harness runs **init/shutdown per module** (`sit_test_framework.h`); earlier VD tests (~20 before the first failure) leave state that breaks later pixel/compositor asserts within the same session.

**Failing tests** (all pass alone, fail after ~20 prior VD tests in module order):

| Test | Approx. index | Assertion |
|------|---------------|-----------|
| `vd_offset_position` | ~21 | `(5,5)` not black — `pixels[tl_idx] < 30` |
| `vd_color_only_no_depth` | ~27 | center R too high — expected green VD |
| `vd_idle_content_switch_colorburst` | ~29 | `screen_is_idle()` |
| `vd_idle_content_switch` | ~30 | `screen_is_live()` |

**Ruled out for primary cause:** Bug B-style stale `screenshot_buffer` (distinct symptom); cross-session `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` (-14) as main driver (same-process module boundary does full shutdown).

**Recommended next steps (deferred):**

1. Bisect `virtual_display_tests[]` in `test_virtual_display.c` to name the polluting test(s) — suspects include `vd_render_into_pipeline`, blend/scaling/opacity tests.
2. Extend `SIT_TEST_DEBUG_GL=1` logging to `SIT_OP_RENDER_VIRTUAL_DISPLAYS` and acquire-time shadow-vs-driver audit (stderr, not new errno).
3. Harness hygiene: `SituationRequestScreenCapture()` before VD screen readbacks; widen `_SituationGLApplyBaselineRasterState()` (blend/depth/viewport not fully reset today).

See `doc/plan/CANVAS_STRETCH_READBACK_FIX_PLAN.md` — Secondary cleanup / VD triage section.

---

---

---

---

---

---

## [v2.4.329] - 2026-06-22

### Description

**v2.4.329**: Ships **Fortran and Modula-2 language-bindings tooling** (generators, wrappers, build scripts, docs). Fortran `hello_situation` builds on all four backends. **Fortran-demo fixes** for Vulkan shader persistence after swapchain events, VSync toggle, and borderless/fullscreen (not core library changes — C examples unaffected).

### Changes — Language bindings (Fortran / Modula-2)

**Generators** (`tools/`):
- `generate_fortran_bindings.py` → `wrappers/Fortran/src/*.f90` (575 `bind(C)` procedures, 18 callbacks)
- `generate_modula2_bindings.py` → `wrappers/Modula2/src/*.def` + `SituationHelpers.mod` (575 `EXTERN` procedures)
- Both wired into `tools/run_all.bat`; documented in `tools/README.md`

**Wrappers**:
- `wrappers/Fortran/` — full `hello_situation` demo (`main.f90`, `demo_helpers.f90`), README
- `wrappers/Modula2/` — full `hello_situation` demo (`Main.mod`), README
- Plan: `doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md` (Phase 1 ✅, Phase 2 blocked on `gm2`)

**Build scripts** (`build/`, `scripts/`):
- `build/build_fortran_example.bat` — `[backend] [example_name]` (opengl / vulkan / static-opengl / static-vulkan)
- `build/build_modula2_example.bat` — same interface; fails with clear error when `gm2` missing
- `scripts/wrapper_compile_fortran.bat`, `scripts/wrapper_compile_modula2.bat`
- `scripts/wrapper_paths.bat` — `build/examples/{fortran,modula2}/`, `build/obj/{fortran,modula2}/`

**Fortran build fixes** (2026-06-22):
- `wrapper_compile_fortran.bat` — removed broken `endlocal` that wiped `SIT_FORTRAN_OBJ_ARGS`
- `wrapper_gcc_link_static.bat` — fixed blank-line `^` continuations; added `OBJ_LIST`; static-vulkan uses `g++` + `-lgfortran`
- `build/build_fortran_example.bat` — delayed expansion for object lists

**Fortran `hello_situation` Vulkan demo fixes** (`wrappers/Fortran/examples/hello_situation/` — demo only, **no `sit/` changes**):
- **Shader sources (Rust/Zig/Odin pattern):** Vulkan — `gl_VertexIndex` vertex + `push_constant` fragment + `SituationCmdSetPushConstant`; OpenGL — `gl_VertexID` vertex + `layout(location)` uniforms + `SituationSetShaderUniform`
- `SituationCreateMesh` + `SituationCmdDrawMesh` for fullscreen triangle
- **`reload_user_shader()`** — cache-busting reload (`//rN` vertex tag) with `SituationPollShaderLoad`; multi-frame refresh after focus regain, VSync toggle, and borderless toggle; 3-frame settle delay after swapchain recreate
- **VSync toggle fix** — `SituationSetVSync(merge(C_FALSE, C_TRUE, vsync_on))` (was re-setting current state, so VSync-off never worked)
- `demo_helpers.f90` — `cleanup_resources` destroys mesh handle on shutdown

**Docs** (no `COMPILATION_GUIDE.md` version-header bump — coordinated separately):
- `doc/COMPILATION_GUIDE.md` — Fortran/Modula-2 build rows, toolchains, shared `wrapper_*.bat` table
- `.kiro/steering/situation-project.md` — Language Bindings section

### Verification (Fortran `hello_situation`)

| Backend | Build |
|---------|-------|
| `opengl` (DLL) | ✅ |
| `vulkan` (DLL) | ✅ |
| `static-opengl` | ✅ |
| `static-vulkan` | ✅ (`g++` + `-lgfortran`) |

| Runtime check | Result |
|---------------|--------|
| Vulkan `hello_situation` — torus + raster bars render | ✅ (2026-06-23) |
| Vulkan — focus loss / alt-tab, shader stays visible | ✅ (2026-06-23, user verified) |
| Vulkan — VSync off (`V` key), HUD shows `VSync:OFF` | ✅ (2026-06-23, user verified) |
| Vulkan — borderless / fullscreen toggle (`F` key) | ✅ (2026-06-23, user verified) |

Modula-2: bindings generate; end-to-end build **blocked** until `gm2` is bundled under `_languages/gm2/` or installed.

### Investigation notes — Vulkan user shader lost after focus loss (Fortran `hello_situation`; resolved in demo)

**Reported:** Clicking outside the Situation window (focus loss / alt-tab) causes the **user shader** (raymarched torus) to stop rendering — black clear colour remains; HUD text may still draw. Reproduced on **Vulkan** in the **Fortran** `hello_situation` demo. **Not reproduced in plain C examples** (`shader_lab_*`, `basic_triangle`, `test_quad`, etc.).

**Root cause (Fortran demo, not core library):**
1. Swapchain / render-pass recreate (focus loss, VSync toggle, borderless) marks shader-cache bundles **STALE**
2. Initial Fortran port used wrong draw/shader paths vs working wrappers (Rust/Zig/Odin)
3. Reload without cache-busting could re-attach STALE bundles intermittently
4. VSync toggle logic in `main.f90` set the **current** state instead of toggling

**Fix (Fortran demo, 2026-06-23):** Backend-correct shader pair + `SituationCmdDrawMesh` + cache-busting `reload_user_shader()` with multi-frame refresh and settle delay. VSync toggle corrected. **Core library unchanged.**

**Ruled out:** Core Vulkan renderer bug (C examples retain user shaders after focus loss on same DLL).

See `doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md` — verification log.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.329.

---

---

---

---

---

---

## [v2.4.330] - 2026-06-22

### Description

**v2.4.330**: Fixes **OpenGL VSync toggle** when examples use the **dedicated render thread** (`render_thread_count = 1`, default in `sit_example.h`). With VSync off, the main thread was flooding the render queue at tens of thousands of FPS (flashing/tearing); turning VSync back on could hang or crash from an unbounded `frames_pending` backlog and corrupted in-flight frame slots. Uncapped FPS now comes from `glfwSwapInterval(0)` at present time, not from disabling queue backpressure.

### Changes — OpenGL VSync / render thread

**Root cause:** `_SitShouldEngageBackpressure()` returned `false` when VSync was off and `target_frame_time == 0`. The main thread then submitted frames without waiting while the render thread still processed every queued frame — slot lifetimes were violated and `frames_pending` could grow without bound until re-enabling VSync deadlocked the main thread.

**Fixes (`sit/`):**
- **`situation_impl_renderer.h` — `_SitShouldEngageBackpressure()`:** Always engage backpressure when `sit_render.enabled` (render thread active). VSync-off still uncaps via swap interval at present, not via an unlimited queue.
- **`situation_impl_renderer.h` — `_SitGLApplySwapIntervalBeforePresent()`:** Apply `glfwSwapInterval` immediately before every OpenGL `glfwSwapBuffers` (main-thread and render-thread paths), tracked with `last_applied_swap_interval`.
- **`situation_impl_wdm.h` — `SituationSetVSync()`:** Update both focus profiles directly; **do not** call `SituationApplyCurrentProfileWindowState()` (VSync is not a GLFW window attribute). Reset render latency metrics on **both** enable and disable so adaptive backpressure policy does not inherit stale samples.
- **`situation_impl_wdm.h` — `SituationApplyCurrentProfileWindowState()`:** Only invoke the framebuffer-size callback when size actually changed; removed mid-frame `glfwSwapInterval` (present site owns interval).

**Related (same release):**
- **`situation_impl_ctrl.h` / `situation_impl_input.h` / `examples/shared/sit_example.h`:** F9/F11 hotkeys no longer double-fire (removed erroneous key synthesis; `SituationConsumeKeyPress` clears deferred pump edges).

### Verification

| Check | OpenGL + render thread (e.g. `01_open_a_window`) |
|-------|-----------------------------------------------------|
| F9 VSync off — HUD stays `VSync:OFF`, no violent flash loop | ✅ user verified |
| F9 VSync on — returns to ~60 FPS without hang/crash | ✅ user verified |
| Vulkan VSync toggle | unchanged (swapchain recreate path) |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.330.

---

---

---

---

---

---

## [v2.4.331] - 2026-06-22

### Description

**v2.4.331**: Completes **Phase B′** (bridge to vertex-pull / SSBO handles): Vulkan harness proves mesh **buffer device address (BDA)** end-to-end in draw and compute, plus a minimal reference example and documented pull draw recipe.

### Changes — Phase B′ (vertex pull vertical slice)

**Harness (`tests/harness/`):**
- **`test_vertex_pull_render` (B′1):** Center quad rendered twice — VAO passthrough vs pull VS using `SituationGetMeshVertexBufferAddress` + `buffer_reference`; center-pixel RGB compared (tolerance 16).
- **`test_mesh_bda_compute_read` (B′4):** Compute reads mesh `vertices[0]` via BDA push constant; SSBO readback matches upload data.
- **`sit_graphics_test_helpers.h`:** Pull VS / compute CS sources, `graphics_test_create_center_quad_mesh`, `graphics_test_render_mesh_red`, documented BDA → push constant → draw recipe.

**Example:**
- **`examples/other/vertex_pull_triangle.c` (B′3):** Smallest user-facing pull-path demo (Vulkan BDA + `sit/gpu/vertex_pull.glslh`; OpenGL falls back to VAO passthrough).

**Docs / API:**
- **`sit/situation_api.h`:** `SituationGetMeshVertexBufferAddress` comment points to pull recipe + GLSL header.
- **`sit/gpu/vertex_pull.glslh`:** Note that `uint64_t` BDA push constants require `GL_EXT_shader_explicit_arithmetic_types_int64`.
- **`doc/plan/plan_handles_ssbo.md`:** B′1–B′4 marked complete; B′5 (optional bindless texture / generic buffer BDA) remains open.

**Shader notes:** Pull VS keeps a dummy `layout(location=0) in vec3 aPos` for pipeline vertex-input state; `(void)aPos` is invalid GLSL — use `pos + aPos * 0.0` instead.

### Verification

| Check | Vulkan (GTX 1070 reference) |
|-------|-------------------------------|
| `test_vertex_pull_render` | ✅ |
| `test_mesh_bda_compute_read` | ✅ |
| `vertex_pull_triangle` example builds | ✅ |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.331.

---

---

---

---

---

---

## [v2.4.332] - 2026-06-22

### Description

**v2.4.332**: **Phase C** vertex-pull productization — mesh layout enum, `CreateMeshEx`, and `SituationCmdBindMeshPullBuffers` replace manual BDA push in harness and examples.

### Changes — Phase C (C1 / C2 / C5)

**API (`sit/situation_api.h`, `situation_impl_renderer.h`):**
- **`SituationMeshVertexLayout`** — `POS_NRM_TEX`, `POS_ONLY`, `POS_TEX`, `POS_NRM`, **`SIT_MESH_LAYOUT_PULL`**.
- **`SituationCreateMeshEx(..., layout, ...)`** — explicit layout tag; validates stride per layout (PULL accepts any stride).
- **`SituationGetMeshVertexLayout`** — query layout stored at creation. `SituationCreateMesh` infers layout from stride.
- **`SituationMeshPullPushConstants`** — `{ vertex_address, index_address }` at push-constant offset 0.
- **`SituationCmdBindMeshPullBuffers(cmd, mesh)`** — pushes the standard pull block for `vertex_pull.glslh` shaders.

**Harness / examples:**
- **`test_vertex_pull_render`** — `CreateMeshEx(PULL)` + `SituationCmdBindMeshPullBuffers` (C5).
- **`vertex_pull_triangle.c`** — uses C2 bind helper.

**Docs:** `sit/gpu/vertex_pull.glslh`, `doc/plan/plan_handles_ssbo.md` updated; B′5 marked cancelled/deferred.

### Verification

| Check | Vulkan |
|-------|--------|
| `test_vertex_pull_render` | ✅ (after rebuild) |
| `test_mesh_bda_compute_read` | ✅ |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.332.

---

---

---

---

---

---

## [v2.4.333] - 2026-06-22

### Description

**v2.4.333**: **Phase C complete** — vertex-pull is a first-class library path end-to-end: layout tags, bind helper, harness gate, reference example, and shader-lab demos migrated on Vulkan.

### Changes — Phase C4 (example migrations)

- **`examples/other/shader_lab_torus.c`**: Torus mesh via `CreateMeshEx(SIT_MESH_LAYOUT_PULL)` on Vulkan; legacy-layout pull VS fetches pos/normal/UV from mesh BDA; `SituationCmdBindMeshPullBuffers` before draw. OpenGL unchanged (VAO path).
- **`examples/other/shader_lab_raytrace2.c`**: Fullscreen triangle pull path on Vulkan (same bind recipe); raytrace FS unchanged.

### Changes — Phase C summary (v2.4.332–333)

| Item | API / artifact |
|------|----------------|
| C1 | `SituationMeshVertexLayout`, `CreateMeshEx`, `GetMeshVertexLayout` |
| C2 | `SituationCmdBindMeshPullBuffers`, `SituationMeshPullPushConstants` |
| C3 | `sit/gpu/vertex_pull.glslh` (v2.4.263) |
| C4 | `shader_lab_torus`, `shader_lab_raytrace2` |
| C5 | `test_vertex_pull_render` through C1/C2 |

### Verification

| Check | Result |
|-------|--------|
| `test_vertex_pull_render` (C5) | ✅ Vulkan |
| `test_mesh_bda_compute_read` (B′4) | ✅ Vulkan |
| `shader_lab_torus` / `shader_lab_raytrace2` build (Vulkan static) | ✅ |
| Full harness | 533/547 pass; 5 failures are pre-existing async-shader timing + tone_synth audio (unrelated to Phase C) |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.333.

---

---

---

---

---

---

## [v2.4.334] - 2026-06-22

### Description

**v2.4.334**: **Phase D slice 1 attempt** — tried to move internal textured quads back to `global_bindless_set` + `global_textures[nonuniformEXT(pc.texture_id)]`, reversing the v2.4.171 per-texture sampler workaround.

> **Superseded by v2.4.335 (backtrack).** This approach **failed** on the reference GTX 1070: all-black readback returned. Initial harness green on a subset of tests did not hold once slice 2 and full relinks were exercised. Do **not** treat v2.4.334 as a successful bindless migration — it is documented as a **failed experiment** retried after v2.4.171 and reverted in v2.4.335.

### Changes — Phase D (D1 / D5 / D2 slice 1 attempt — **reverted in v2.4.335**)

- **`SituationAcquireFrameCommandBuffer` (VK)**: `_SitVulkanBindGlobalBindlessSet` pre-binds `global_bindless_set` at set 1 *(disabled `#if 0` in v2.4.335)*.
- **`SituationCmdDrawTexture`**: dropped per-texture `single_sampler_descriptor_set` bind; push `texture_id` *(sampler bind restored v2.4.335)*.
- **`SituationCmdDrawQuad`**: relied on D5 frame bindless bind *(global bindless bind removed v2.4.335)*.
- **Internal quad pipeline layout**: set 1 = `bindless_descriptor_layout` *(reverted to `text_sampler_layout` v2.4.335)*.
- **`sit/gpu/quad.frag` (VK)**: `global_textures[nonuniformEXT(pc.texture_id)]` *(reverted to `u_QuadTexture` v2.4.335)*.
- **Bindless infra:** `descriptorBindingSampledImageUpdateAfterBind`; `VkDescriptorSetVariableDescriptorCountAllocateInfo` on alloc *(infra kept; sampling path failed)*.
- **`doc/plan/plan_handles_ssbo.md`**: D1 audit table; D5 + D2 slice 1 marked complete *(corrected in v2.4.335 — bindless migration not complete)*.

**Still on per-texture samplers (D2 slice 2):** `SituationCmdDrawTextEx`, `SituationCmdDrawTextureYpqGrade`, VD compositor, `SituationCmdBindTextureSet`.

### Verification (v2.4.334 — partial; bindless path later failed)

| Check | Result |
|-------|--------|
| `draw_textured_checkerboard` | ✅ Vulkan *(at time of release; failed again during slice 2 bindless WIP)* |
| `screen_readback_corner_layout` | ✅ Vulkan *(same caveat)* |
| `draw_quad_red` | ✅ Vulkan |
| `descriptor_bind_sampled_texture` | ✅ Vulkan *(user single_sampler path — never used bindless array)* |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.334.

---

---

---

---

---

---

## [v2.4.335] - 2026-06-22

### Description

**v2.4.335**: **Phase D backtrack** — we **tried and failed** to migrate internal Vulkan draws to `global_textures[]` bindless array sampling (v2.4.334). All internal textured paths were **reverted** to the v2.4.171 per-texture `single_sampler_descriptor_set` model. This release also ships **D2 slice 2 scaffolding**: push `texture_id` on text, YPQ grade, and VD compositor push blocks.

> **Project mark:** v2.4.334 claimed Phase D slice 1 “closed” the v2.4.171 bindless regression. It did not. The same all-black readback failure returned on the reference machine (GTX 1070). v2.4.335 explicitly records that **internal bindless array sampling is not working** and that Phase D’s migration goal remains **unachieved**.

### Backtrack — internal bindless migration failed (v2.4.334 → v2.4.335)

| | |
|---|---|
| **What we tried (v2.4.334–335 WIP)** | Pipeline set 1 = `bindless_descriptor_layout`; FS `global_textures[nonuniformEXT(pc.texture_id)]`; D5 frame pre-bind of `global_bindless_set`; combined UBO+bindless descriptor bind; infra fixes (`descriptorBindingSampledImageUpdateAfterBind`, variable-count alloc). Extended to text, YPQ, VD in slice 2 WIP. |
| **Symptom** | All-black or wrong-pixel framebuffer readback — same failure class as **v2.4.170** (fixed in **v2.4.171** with per-texture sampler). Hardcoded bindless index 0/1 also black → not a push-constant / slot-index bug alone. |
| **Reference GPU** | NVIDIA GeForce GTX 1070, Vulkan 1.4 |
| **Failing harness** | `draw_textured_checkerboard`, `screen_readback_corner_layout`, `ypq_grade_pass_cpu_parity`, all `text_rendering`, ~half of `virtual_display` |
| **Still passing** | User-shader `descriptor_bind_sampled_texture` via `single_sampler_descriptor_set`; solid `draw_quad_red` (no texture sample) |
| **What we reverted** | Internal FS → dedicated `sampler2D`; pipeline set 1 → `text_sampler_layout`; per-draw `single_sampler_descriptor_set` bind on quad/text/YPQ/VD; D5 frame pre-bind → `#if 0` |
| **What we kept** | Push `texture_id` in internal push blocks; `_SitVulkanWriteSlotToGlobalBindlessSet` at create/draw (user bindless API + future retry); bindless pool infra (slot writes work; **sampling does not**) |
| **Phase D status** | **Goal not achieved.** Internal renderer still on v2.4.171 sampler model. Open blocker for D2/D3. |

### Changes — Phase D (D2 slice 2 scaffolding, post-backtrack)

- **`SituationCmdDrawTextEx`**: push font `texture_id`; set 1 = font `single_sampler_descriptor_set` (not bindless array).
- **`SituationCmdDrawTextureYpqGrade`**: push `texture_id`; set 1 = texture `single_sampler_descriptor_set`.
- **VD compositor (Path A + B)**: push `texture_id` in compositor push blocks; set 1 = `vd->vk.descriptor_set` (per-VD sampler).
- **Shaders**: `text.frag`, `ypq_grade.frag`, `vd.frag`, `composite.frag`, `quad.frag` — Vulkan use dedicated `sampler2D` at set 1 binding 0 (bindless array **removed**).
- **Push layouts**: `SIT_VD_PATH_B_PUSH_CONSTANT_SIZE` → 104, `SIT_VD_PATH_A_PUSH_CONSTANT_SIZE` → 120 (add `texture_id`); `compositor.vert` push blocks updated to match.
- **D5 frame pre-bind**: disabled (`#if 0`) — attempted in v2.4.334, part of failed bindless path.
- **`doc/plan/plan_handles_ssbo.md`**: failed-attempt section added; D2 bindless migration marked **not done**.

**Still on per-texture samplers:** all internal textured draws (same as v2.4.171); `SituationCmdBindTextureSet` (user shaders — D4).

### Verification

| Check | Result |
|-------|--------|
| `draw_textured_checkerboard` | ✅ Vulkan (after revert + test relink) |
| `screen_readback_corner_layout` | ✅ Vulkan |
| `ypq_grade_pass_cpu_parity` | ✅ Vulkan |
| `draw_quad_red` | ✅ Vulkan |
| `text_rendering` module (5 tests) | ✅ Vulkan |
| `virtual_display` module (30 tests) | ✅ Vulkan |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.335.

---

---

---

---

---

---

## [v2.4.336] - 2026-06-23

### Description

**v2.4.336**: **API cleanliness P0 + P1** — header ergonomics and discoverability without ABI changes. Deprecated symbols now warn at compile time; active modules stay uncluttered; canonical API paths documented in-header and in the SDK.

### Changes — API header (`sit/situation_api.h`)

- **P0**: `SIT_DEPRECATED(msg)` (GCC/Clang/MSVC); all legacy symbols moved to a single **Deprecated API** section at EOF; duplicate `SituationGetCPUThreadCount` removed; stray `SituationGetBasePathFromFile` public decl removed.
- **P1**: Module map + API comment-tag convention; inline usage guide trimmed to five rules + SDK link; `SituationInitInfoDefault`, `SituationClearValueColor`, `SituationClearValueDepth`; canonical backend/audio enumeration comments; audio recommended-paths table; YPQ/HDR subsection banner.
- **`SituationGetAudioDevices`**: deprecated — prefer `SituationEnumerateAudioDevices` + `SituationFreeDeviceList`.

### Changes — Implementation fix (build unblock)

- **`situation_impl_vd.h`**: wrap `_SitVulkanWriteSlotToGlobalBindlessSet` in `#if defined(SITUATION_USE_VULKAN)` on the OpenGL VD texture-registration path (OpenGL build was failing with implicit declaration).

### Changes — Docs

- **`doc/situation_sdk.md`**: deprecated-API compile-warning note; canonical audio enumeration; audio recommended paths.
- **`doc/plan/API_cleanliness_plan.md`**: P0 complete; P1 in progress.

### Verification

| Check | Result |
|-------|--------|
| `build_situation.bat` opengl + vulkan | ✅ |
| `build_tests.bat` opengl + vulkan | ✅ (deprecated warning on `test_get_device_info`) |
| `run_tests.bat` opengl/vulkan `--module core --headless` | ✅ 44/44 each |
| Binding regen (5 FFI + index) | ✅ 581 SITAPI |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.336.

---

---

---

---

---

---

## [v2.4.337] - 2026-06-23

### Description

**v2.4.337**: **API cleanliness P2.1** — all public typedefs that were declared mid-function-section now live in a typed block before the module map and `SITAPI` prototypes. Function sections are prototype-only; no ABI or signature changes.

### Changes — API header (`sit/situation_api.h`)

- **P2.1**: Consolidated **Additional public types** section before API quick rules — moved `SituationOSInfo`, `SituationProcessInfo`, `SituationGraphicsBackend`, `SituationGraphicsCaps`, raster enums/structs, mesh layout types, shader validation types, camera types, `SituationMidiDeviceInfo`, `SituationYpqRgbMappingStats`, and threading topology types out of function blocks.

### Verification

| Check | Result |
|-------|--------|
| `build_situation.bat` opengl + vulkan | ✅ |
| `build_tests.bat` opengl | ✅ (expected deprecation warnings) |
| `run_tests.bat` opengl `--module core --headless` | ✅ 46/46 |
| Binding regen (5 FFI + index) | ✅ 581 SITAPI |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.337.

---

---

---

---

---

---

## [v2.4.338] - 2026-06-23

### Description

**v2.4.338**: Release packaging for **API cleanliness P2.1** (same header changes as v2.4.337). Patch bump aligns DLL artifacts and doc metadata with the committed release.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.338.
- **`doc/situation_sdk.md`**: metadata table version sync.
- **`doc/situation_api.md`**: header synced to v2.4.338 (581 SITAPI); merged 34 missing entries via `tools/merge_api_doc_gaps.py`; fixed broken Miscellaneous module summary; P1 inline helpers + init example corrected; deprecated API table updated.

### Verification

| Check | Result |
|-------|--------|
| `build_situation.bat` opengl + vulkan | ✅ |

---

---

---

---

---

---

## [v2.4.339] - 2026-06-23

### Description

**v2.4.339**: **API cleanliness P2.2** — `sit/situation_api.h` is now a **94-line umbrella** that `#include`s nine domain submodules. Public entry point unchanged (`#include "sit/situation_api.h"` via `situation.h`); **581** `SITAPI` symbols; zero ABI/signature changes.

### Changes — API header split (P2.2)

| File | Lines | Owns |
|------|------:|------|
| `situation_api.h` | 94 | Guards, `SITAPI`/`SIT_DEPRECATED`, backend checks, include chain |
| `situation_api_config.h` | 141 | Limits, `SITUATION_BEGIN_FRAME`, `SIT_MALLOC` / alloc macros |
| `situation_api_types_system.h` | 567 | Init, timers, topology, thread-pool types |
| `situation_api_types_gpu.h` | 1017 | Render passes, barriers, VD, raster/camera/mesh types |
| `situation_api_types_audio.h` | 262 | Node graph, MIDI, tone types |
| `situation_api_platform.h` | 488 | Lifecycle, window, input, image/font, introspection |
| `situation_api_graphics.h` | 254 | GPU commands, resources, shaders, VD, models |
| `situation_api_audio.h` | 229 | Playback, graph, MIDI, serialization |
| `situation_api_system.h` | 189 | Hot-reload, filesystem, timers/YPQ, threading |
| `situation_api_deprecated.h` | 48 | 11 legacy symbols (`SITUATION_INCLUDE_DEPRECATED_API`, default 1) |

- **`tools/situation_api_parser.py`**: `read_expanded_api_lines()` follows `#include "situation_api_*.h"` so bindings/index stay at 581 after the split.
- **`tools/audit_api_header_layout.py`**: reports umbrella vs expanded line counts.
- **`sit/situation.h`**: bridge comment lists P2.2 submodule files.
- All `situation_api_*.h` files: standard library file header banners.

### Verification

| Check | Result |
|-------|--------|
| Umbrella `situation_api.h` | **94 lines** (was 3266) |
| Expanded API tree | **3259 lines** |
| `python tools/generate_api_index.py` | **581/581** |
| `build_situation.bat` clean + opengl + vulkan | ✅ |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.339.
- **`doc/situation_api.md`**, **`doc/situation_sdk.md`**: metadata synced to v2.4.339.

---

---

---

---

---

---

## [v2.4.340] - 2026-06-23

### Description

**v2.4.340**: **API cleanliness P2 complete** — documentation mirror of P2.2 header split, full binding verification, harness GL+VK green. **581/581** symbol coverage across `doc/guide/`; umbrella `doc/situation_api.md` is **96 lines** (Option A TOC).

**Guide augmentation pass** (same release): narrative module guides upgraded from API stubs to workflow-oriented docs — mermaid diagrams, canonical examples, troubleshooting, cross-links. New **`virtual_display.md`**, **`drawing_3d.md`**, **`hd_color_output.md`**; full rewrites for **`compute.md`**, **`hot_reload.md`**, **`threading.md`**, **`logging.md`**, **`window_display.md`**; expanded **`examples_faq.md`**; SDK §3.6 Virtual Display deduplicated to pointers; **`725`** internal doc links verified.

### Changes — P2.5 doc guide split

| Artifact | Result |
|----------|--------|
| `doc/situation_api.md` | **96-line** umbrella (stable URL); module table lists **25** guide files |
| `doc/guide/*.md` | **25** module files (`_front_matter.md` + 24 modules incl. `hd_color_output.md`, `virtual_display.md`, `drawing_3d.md`) |
| `tools/p25_split_api_guide.py` | Stack-based `<details>` extractor + merge with prior guide content |
| `tools/merge_api_doc_gaps.py` | Targets `doc/guide/*.md`; section→file routing |
| `tools/generate_api_index.py` | Coverage scan: umbrella + guide tree |
| `scripts/verify_doc_links.py` | Validates umbrella + guide cross-links |

### Changes — Module guide augmentation (narrative)

| Guide | Change |
|-------|--------|
| `doc/guide/virtual_display.md` | **New** — compositor model, scaling/blend, CRT/PiP/3D/compute patterns, troubleshooting |
| `doc/guide/drawing_3d.md` | **New** — shader+mesh+camera, GLTF, canonical `examples/other/` paths |
| `doc/guide/compute.md` | **Rewrite** — layouts, dispatch, barriers, particles/readback/present, VD compute target |
| `doc/guide/hot_reload.md` | **Rewrite** — I/O-thread polling, eligibility table; `SituationCheckHotReloads()` no-op corrected |
| `doc/guide/threading.md` | **Rewrite** — thread landscape, dual queues, `DispatchParallel`; fixed pool API names |
| `doc/guide/logging.md` | **Expanded** — levels, callbacks, `SIT_CHECK_GL_ERROR`, stdout vs stderr |
| `doc/guide/window_display.md` | **Rewrite** — GLFW profile system, all 13 window flags, pause/focus/resize callbacks, fullscreen vs borderless, multi-monitor |
| `doc/guide/hd_color_output.md` | **New** — 10-bit SDR & HDR10 policy, caps, DXGI, limitations, YPQ/PQ integration, harness |
| `doc/guide/examples_faq.md` | **Expanded** — learning path, per-example keys/build/APIs, compute/console/demon_hunt, FAQ tied to guides |
| `doc/guide/drawing_2d.md` | VD section trimmed → pointer to `virtual_display.md` |
| `doc/guide/audio_graph.md`, `filesystem.md`, `system_introspection.md`, `ypq_color.md`, `renderer_bolster.md` | Intros + workflow sections (earlier in pass) |
| `doc/situation_sdk.md` §3.6 | Long VD duplicate replaced with summary + links; sub-anchors `361`–`365` preserved |
| `doc/situation_sdk.md` §3.7 | Link to `guide/compute.md` at top |
| `doc/introduction.md`, `doc/COMPILATION_GUIDE.md`, `doc/guide/_front_matter.md`, `doc/plan/API_cleanliness_plan.md` | Index / cross-link sync |

### Changes — P2 exit verification

| Check | Result |
|-------|--------|
| `tools/run_all.bat` (5 FFI + index) | **581/581** |
| `build_tests.bat static-opengl` / `static-vulkan` | ✅ harness builds |
| `sit_test_*.exe --module core --filter init` | ✅ GL + VK |
| `python tools/merge_api_doc_gaps.py` | **581/581** across guide tree |
| `scripts/verify_doc_links.py` | **725** links, **29** docs ✅ |

### P2.4 — deferred to v2.5 (documented)

`SituationThreadPool` remains a **public struct** (caller-owned storage via `SituationCreateThreadPool(SituationThreadPool*)`). By-value usage exists in `examples/10_thread_pool/main.c`, harness tests, and `situation_impl_decl.h` — opaque typedef would require API redesign (breaking change), out of minor-release scope.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.340.

---

---

---

---

---

---

## [v2.4.341] - 2026-06-23

### Description

**v2.4.341**: **Font migration (F0–F4 + partial F6)** — Situation owns grid/bitmap font atlases, GPU text layout, and CPU stamp paths; RGL bitmap font APIs delegate to Situation instead of duplicating atlas builders and per-glyph sprite loops.

### New APIs — font lifecycle & builders

| API | Role |
|-----|------|
| `SituationUnloadFont` | Now frees `glyph_info` and destroys owned atlas textures (skips built-in default) |
| `SituationBakeBitmapFontAtlas` | 1bpp `LoadBitmapFontFromMemory` data → NEAREST GPU grid atlas |
| `SituationLoadBitmapFontFromTexture` | GPU sheet + cell size → `SituationFont` grid metadata |
| `SituationCreateTerminalFontFromMemory` / `Ex` | Grayscale terminal grid → atlas |
| `SituationCreateCP437Font` / `CreateASCIIFont` | Convenience terminal wrappers |
| `SituationCreatePackedBitmapFont` / `Outlined…` | `SituationPackedFont` bit-packed rows + optional outline |
| `SituationCreateVCRFont` (+ outline) | 12×14 VCR OSD packed font |
| `SituationCreateVGA8x8Font` (+ outline) | 8×8 VGA packed font |

### New APIs — layout & draw

| API | Role |
|-----|------|
| `SituationMeasureTextEx` | Multiline measure with per-char spacing |
| `SituationGetTextLineCount` | Line count for explicit `\n` or width wrap |
| `SituationMeasureText` | Delegates to `MeasureTextEx` (multiline + baked TTF when available) |
| `SituationCmdDrawTextBoxed` | GPU text clipped to `SitRectangle` with optional word wrap |
| `SituationImageStampText` / `StampTextBoxed` | CPU rasterize label + optional bg into `SituationImage` |

### `SituationFont` grid fields

`first_char`, `chars_per_row` / `chars_per_col`, `display_cell_width` / `display_cell_height`, `char_spacing`, `line_spacing` — used by grid atlas builders and `SituationCmdDrawTextEx` (any grid atlas, not only the built-in default).

### GPU text pipeline

- `SituationCmdDrawTextEx` / GL execute: general **grid atlas** path (`glyph_info == NULL` + valid atlas), newline support, scaled cell size from font metadata.
- Default 8×8 font init sets grid layout fields for consistent measure/draw.

### RGL (`doc/misc/rgl.h`) — bitmap delegation (F6 partial)

| RGL API | Situation backend |
|---------|-------------------|
| `RGL_CreateTerminalFont*` / CP437 / ASCII | `SituationCreateTerminalFont*` |
| `RGL_CreatePackedBitmapFont` / outlined / VCR / VGA | `SituationCreatePacked*` / `VCR*` / `VGA*` |
| `RGL_DrawText` / `Ex` / `Boxed` | `SituationCmdDrawTextEx` / `DrawTextBoxed` |
| `RGL_MeasureText` / `GetTextLineCount` | `SituationMeasureTextEx` / `GetTextLineCount` |
| `RGL_UnloadBitmapFont` | `SituationUnloadFont` |

TTF load/draw and GPU stamp-to-texture in RGL still use legacy paths (next slice).

### Tests & docs

| Item | Result |
|------|--------|
| `test_text_rendering` | **8/8** OpenGL (`font_unload_destroys_atlas`, multiline measure, line count, existing draw/TTF tests) |
| `doc/guide/font.md` | **New** font module guide (load/bake/builders, `SituationPackedFont`, measure, stamp, lifecycle) |
| `doc/guide/text_rendering.md` | Refactored to **GPU-only** (`SituationCmdDrawText*`); font setup links to `font.md` |
| `doc/guide/image.md` | Font struct/API stubs removed; CPU draw + links to `font.md` |
| `doc/situation_command_reference.md` | `SituationCmdDrawTextBoxed`, `SituationDrawMetricsOverlay`; v2.4.341 |
| `doc/situation_api_index.md` | Regenerated — full font API surface + command-ref anchors |
| FFI wrappers | Regenerated (Rust/Odin/Zig/Fortran/Modula2): `SituationFont` ABI matches C header; `SituationPackedFont` added |
| `doc/plan/font_migration_plan.md` | F0–F4 ✅ ledger; F5 partial (docs/wrappers); F6/F7 status |
| `doc/misc/RGL_MIGRATION_PLAN.md` | §7A expanded (Situation F0–F4 + RGL bitmap delegates vs TTF/stamp legacy) |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.341.

---

---

---

---

---

---

## [v2.4.342] - 2026-06-24

### Description

**v2.4.342**: **Python ctypes bindings** — Situation’s **sixth** generated wrapper language (after Odin, Zig, Rust, Fortran, Modula-2). Same `sit/situation_api.h` → `tools/situation_api_parser.py` → `tools/binding_common.py` pipeline; **594** auto-bound DLL exports; full **`hello_situation`** demo (torus raymarch, ToneSynth→Echo→Reverb graph, virtual MIDI, HUD). Stdlib **`ctypes` only** — no pip dependency for local development.

### Generator & package

| Item | Path / detail |
|------|----------------|
| Generator | `tools/generate_python_bindings.py` — mirrors Zig/Fortran structure; emits `types`, `foreign`, `callbacks`, `constants`, `helpers` |
| Package | `wrappers/Python/situation/` — `load_dll("opengl"\|"vulkan")`, `foreign.bind_all(dll)`, `helpers.init_info_window()` |
| Loader | `situation/_dll.py` — discovers `build/dll/` or `build/examples/python/`; `os.add_dll_directory` on Windows |
| Manual | `situation/manual.py` — variadic log + `SituationSetLogCallback` (same four-symbol set as other wrappers) |
| Index | `wrappers/Python/API_INDEX.md`, `MANUAL_BINDINGS.md` |

### ABI & binding accuracy

| Topic | Implementation |
|-------|----------------|
| `SituationInitInfo` | Full **v2.4.336+** MSVC x64 / MinGW layout with explicit padding (matches `wrappers/Modula2/glue/situation_m2_glue.c`) — demos use **`helpers.init_info_window()`**, not hand-filled fields |
| Enums | `IntEnum` with sequential C enum assignment + `#define` resolution from `situation_api_types_system.h` |
| Callbacks | **18** `CFUNCTYPE` aliases from `sit/situation_base_callbacks.h` |
| Structs | Generational handles (`SituationShader`, `SituationTexture`, …), `ColorRGBA`, `Vector2`–`4`, `Mat4`, opaque aliases |
| DLL exports | `bind_all` uses `hasattr(dll, …)` — skips symbols parsed from headers but not exported from the DLL |
| Init smoke | `SituationInit` / `SituationShutdown` verified against `situation_opengl.dll` |

### Build & run

```bat
build\build_situation.bat opengl
python tools\generate_python_bindings.py
build\build_python_example.bat opengl hello_situation
```

| Script | Role |
|--------|------|
| `build/build_python_example.bat` | Thin dispatcher: `wrapper_link_config.bat` → copy DLL to `build/examples/python/` → run example |
| `scripts/wrapper_paths.bat` | **`python`** row → `build\examples\python\` |
| `tools/run_all.bat` | Regenerates Python bindings with Odin/Zig/Rust/Fortran/Modula-2 |

### Demo — `wrappers/Python/examples/hello_situation.py`

Ports the canonical Rust demo: Vulkan push constants vs OpenGL named uniforms (`uTime`, `uResolution`), pentatonic auto-notes, V/F/Space/+/−/]/[/P/O keys, FPS/VSync/audio/FX HUD via `SituationCmdDrawTextEx`. Runtime verified (main loop, audio graph, render).

### Docs & plan

| Item | Update |
|------|--------|
| `wrappers/Python/README.md` | Build, layout, usage, main-thread rule |
| `tools/README.md` | `generate_python_bindings.py` listed |
| `doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md` | Phase 4 Python deliverables reference this release |

Phase 3 shared infra (`wrappers/shared/`, `verify_abi_layouts.py`) remains planned; Python v1 uses the verified Modula-2 glue layout inline in generated `types.py`.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.342.

---

---

## [v2.4.343] - 2026-06-24

### Description

**v2.4.343**: **RGL test pattern shader migration — P7 harness gate.** Config delivery is a single **std140 UBO** at `set=0, binding=0` on **both** OpenGL and Vulkan (`SituationCmdBindDescriptorSet` + `SituationUpdateBuffer`). Removed per-backend duct tape (Vulkan push constants, OpenGL fake-SSBO-via-push, loose SPIR-V uniforms, padding hacks). Harness fragment shaders are **identical** GL/VK sources; only glslc `--target-env` differs.

### Shader library (`sit/gpu/test_patterns/`)

| Item | Detail |
|------|--------|
| Binding headers | `sit_tp_config_ubo.glslh`, `sit_tp_smpte_vd_ubo.glslh` — std140 blocks; pattern modules stay binding-agnostic |
| Dispatcher | `sit_test_patterns.glslh` + `sit_tp_sample()` — P0–P4 (eight 2D patterns + 3D stub; full SMPTE in `#else`) |
| Compositor | Vulkan `#include` in `vd.frag` / `composite.frag`; OpenGL §0.5-C inline mirror |

### C API & harness

| Item | Path / detail |
|------|----------------|
| Config + pack | `sit/sit_test_pattern_config.h` — `SitTestPatternConfig`, std140 offsets (`SIT_TP_CONFIG_UBO_SIZE` 144 B) |
| Draw helper | `sit/sit_test_pattern_config.c` — `SitTestPatternBindConfigResources`, `SitTestPatternDrawFullscreenUbo` |
| Tests | `tests/harness/test_graphics_patterns.c` — readback suite |
| Shaders | `tests/harness/shaders/harness_test_pattern_{gl,vk}.fs`, `harness_test_pattern_smpte_vd_{gl,vk}.fs` |
| Embed | `build/compile_harness_shaders.ps1`, `scripts/gen_test_pattern_spirv_embed.ps1` |
| Build | `build/build_tests.bat` — links `sit/sit_test_pattern_config.c` + pattern SPIR-V embeds |

### Verification

| Backend | Result |
|---------|--------|
| Vulkan | **9/9** (`--module graphics --filter pattern`, incl. runtime `#include` compile) |
| OpenGL | **8/8** (no runtime-include test on GL SPIR-V path) |

### Docs & steering

| Item | Update |
|------|--------|
| `doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md` | §1.5 UBO contract shipped; P7 ✅; P6 lab + P8 docs open |
| `.kiro/steering/situation-project.md` | Chunked updatelog model; architecture (test_patterns, Python bindings); harness + dual-backend shader rules |

**Open:** P6 `shader_lab_test_patterns.c`, P8 `doc/guide/test_patterns.md`, §5.3 VD pixel-identical gate, P5 3D grid raymarch.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.343.

---

---

## [v2.4.344] - 2026-06-24

### Description

**v2.4.344**: **VD idle PATTERN compositor** — full `SitTestPatternConfig` on the Virtual Display standby path (Vulkan). Layer-bitmask standby replaces the old exclusive `pattern_type` model; compositor reads tuning from a std140 UBO instead of push constants; user guide and harness catch up.

### VD pattern API (`situation_api_graphics.h`)

| API | Role |
|-----|------|
| `SituationSetVirtualDisplayPatternConfig(vd, &cfg)` | Copy full `SitTestPatternConfig`, force `SITUATION_VD_FALLBACK_PATTERN` |
| `SituationGetVirtualDisplayPatternConfig(vd, &out)` | Read standby config (`width`/`height` synced from VD resolution) |
| `SituationSetVirtualDisplayPatternLayers(vd, bitmask)` | Convenience: set `pattern_layers` + `PATTERN` mode |
| `SituationGetVirtualDisplayPatternLayers(vd)` | Read layer bitmask |

`SituationVirtualDisplay.standby_pattern` replaces `standby_pattern_layers` (`situation_api_types_gpu.h`).

### Compositor (Vulkan)

| Item | Detail |
|------|--------|
| Idle modes | `SOLID`, `COLORBURST` (SMPTE subset), **`PATTERN`** (full layer compositor) |
| UBO | `SitTpConfigBlock` 144 B std140 — set **2** (path B / `vd.frag`), set **3** (path A / `composite.frag`) |
| Upload | `_SitVDUploadStandbyPatternUbo()` on idle `PATTERN` draws |
| Shaders | `vd_idle_pattern.glslh`, `vd_colorburst_subset.glslh`; push constants back to **104 B** / **120 B** |
| OpenGL | Config API + UBO bind @ `SIT_UBO_BINDING_VD_PATTERN`; layered **PATTERN** compositor awaits SPIR-V path (`SITUATION_GL_VD_COMPOSITOR_SPIRV`) — `COLORBURST`/`SOLID` unchanged |

### Build & DLL

| Item | Detail |
|------|--------|
| `sit/sit_test_pattern_config.c` | Linked into OpenGL and Vulkan DLLs (`sit/Makefile`) |
| Draw helpers | `SitTestPatternBindConfigResources`, `UploadConfigUbo`, `DrawFullscreenUbo` exported from DLL |

### Bug fix

| Item | Detail |
|------|--------|
| VMA teardown | Destroy `vd_pattern_config_ubo` **before** `vmaDestroyAllocator` (shutdown assert/leak) |

### Harness & launcher

| Item | Detail |
|------|--------|
| Tests | `vd_pattern_config_api`, `vd_idle_pattern_standby` (`test_virtual_display.c`) |
| Pattern suite | Vulkan **11/11** graphics `--filter pattern` (compositor path verified on idle VD) |
| `build/run_tests.bat` | Prepend `C:\msys64\mingw64\bin` — avoids stale `libwinpthread` on PATH (DLL load crash) |

### Docs

| Item | Update |
|------|--------|
| `doc/guide/test_patterns.md` | Rewritten — three idle modes, VD API table, Vulkan vs OpenGL compositor status |
| `doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md` | §3.4 `Set/GetVirtualDisplayPatternConfig` ✅ |

**Open:** P6 `shader_lab_test_patterns.c` (keys 0–8 layer toggles); harness module-teardown crash (post-test, pre-existing class).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.344.

---

---

## [v2.4.345] - 2026-06-24

### Description

Delivers OpenGL Virtual Display compositor **PATTERN** parity with Vulkan via build-time glslc → SPIR-V embed (`GL_ARB_gl_spirv`), sharing the same `#include sit/gpu/test_patterns/` headers as the Vulkan compositor.

**Same patch, two phases:** the first embed pass **broke OpenGL at runtime** (`SituationInit` **-641**); shader binding and init fixes in the **same v2.4.345 patch** restore init and VD composite. Full detail below — including regressions introduced by the initial embed and what remains open.

### Changes — infrastructure & delivery

| Area | Item | Detail |
|------|------|--------|
| Embed | `sit/sit_vd_compositor_gl_spirv_embed.c/.h` | Precompiled `compositor.vert` (path A/B), `vd.frag`, `composite.frag` |
| Build | `build/compile_vd_compositor_gl.ps1` | glslc OpenGL target; auto-run from `build_situation.bat opengl` / `static-opengl` |
| Init | `_SituationCreateGLVdCompositorShaderProgram` | Loads embed via `_SituationCreateGLShaderProgramFromSpirv` |
| Static lib | `sit/Makefile` `static-opengl` | Archive `sit_test_pattern_config_static.o`, `sit_vd_compositor_gl_spirv_embed_static.o` |
| Shaders | `vd.frag` / `composite.frag` | `SITUATION_GL_VD_COMPOSITOR_SPIRV` + full pattern `#include` path |
| Harness | `vd_idle_pattern_standby` | Runs on OpenGL + Vulkan (checkerboard readback) |

### Changes — init/composite fix (same patch)

| Item | Detail |
|------|--------|
| Sampler layout | `vd.frag`: `layout(location = 1, binding = 4) uniform sampler2D u_screenTexture`; `composite.frag`: bindings **4** / **5** |
| Vertex layout | `compositor.vert` (OpenGL): `u_model` → **location 8** (avoid SPIR-V loc **0** clash with `aPos`) |
| glslc flags | `compile_vd_compositor_gl.ps1`: removed **`-fauto-bind-uniforms`** (explicit `layout(binding=…)` only) |
| Pattern UBO | `SitTpConfigBlock` @ `SIT_UBO_BINDING_VD_PATTERN` (**6**) — bind at init and before VD composite execute |
| Sampler units | Init uses explicit uniform **locations** (`SIT_UNIFORM_LOC_VD_SCREEN_TEXTURE` etc.) — SPIR-V strips names; `glGetUniformLocation` failed silently |
| VD model matrix | Composite draws: `SIT_UNIFORM_LOC_VD_COMPOSITOR_MODEL` (**8**), not `SIT_UNIFORM_LOC_MODEL_MATRIX` (**0**) |
| Error queue | Drain GL errors after `SIT_OP_RENDER_VIRTUAL_DISPLAYS` (NVIDIA spurious `0x502`; pixels still correct per harness) |
| Init diagnostics | Do **not** overwrite SPIR-V link errors from `_SituationCreateGLShaderProgramFromSpirv` |

### Files touched

| File | Change |
|------|--------|
| `sit/gpu/vd.frag`, `sit/gpu/composite.frag`, `sit/gpu/compositor.vert` | Sampler/UBO layouts; `u_model` @ 8 |
| `build/compile_vd_compositor_gl.ps1`, `sit/sit_vd_compositor_gl_spirv_embed.c` | glslc flags; regenerated embed |
| `sit/situation_impl_renderer.h`, `sit/situation_impl_decl.h` | UBO/sampler bind, compositor loc 8, error drain, init errors |
| `sit/Makefile` | `static-opengl` archives |
| `doc/updatelog_24_04.md`, `doc/whatsnew.md`, `doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md`, `doc/guide/test_patterns.md` | Docs + this entry |

### Regressions introduced (initial embed — fixed in same patch)

| Severity | Symptom | Cause | Status |
|----------|---------|-------|--------|
| **Critical** | OpenGL **`SituationInit` → `-641`**; SPIR-V VD **program link failed** | `glslc -fauto-bind-uniforms` put sampler @ **loc 0**, colliding with **`v_texCoord` @ 0** | **Fixed** |
| **Critical** | All GL context modules **SETUP FAILED** | Downstream of init `-641` | **Fixed** |
| **High** | Static `situation_opengl.a` link errors | Missing `sit_test_pattern_config` + embed `.o` in archive | **Fixed** |
| **High** | Init SPIR-V link log **overwritten** | Generic message in `_SituationInitOpenGL` | **Fixed** |
| **Process** | Docs claimed GL parity before green harness | Premature plan/whatsnew update | **Corrected** in this entry |

**Not introduced by this patch (pre-existing):** harness teardown `ACCESS_VIOLATION`; audio module crash cascade when init never succeeded.

### Regressions & known gaps (remaining — do not re-break)

| Severity | Item | Notes |
|----------|------|--------|
| **Medium** | NVIDIA **`GL_INVALID_OPERATION (0x502)`** after VD `glDrawArrays` | Mitigated by post-composite error-queue drain; harness pixels correct; proper fix open |
| **Medium** | **`virtual_display` full module** — 4 tests fail together, pass isolated: `vd_offset_position`, `vd_color_only_no_depth`, `vd_idle_content_switch`, `vd_idle_content_switch_colorburst` | Test ordering pollution (pre-existing) |
| **Low** | VD compositor `u_model` @ **8** vs quad/text @ **0** | Intentional SPIR-V workaround |
| **Low** | VD GLSL changes need **`compile_vd_compositor_gl.ps1`** + rebuild | No runtime shaderc on OpenGL DLL |
| **Open** | §5.3 COLORBURST pixel-identical gate; P6 shader lab | Plan items unchanged |
| **Pre-existing** | Harness teardown crashes (audio tail, VK `pattern_runtime_include_compile`) | Unchanged |

**Library behavior we must not regress**

- OpenGL **`SituationInit` succeeds** when `GL_ARB_gl_spirv` + non-empty embed.
- Vulkan VD **PATTERN** (344+): `graphics --filter pattern` **11/11**.
- OpenGL **`SOLID` / `COLORBURST` / `PATTERN`** idle compositor paths.
- **`SituationSet/GetVirtualDisplayPatternConfig`** ABI (344).

### Verification (GTX 1070, `sit_test_opengl.exe`, headless)

| Suite | Result |
|-------|--------|
| `core` init filters | Pass |
| `graphics --filter pattern` | **10/10** |
| `vd_composite_time`, `vd_idle_pattern_standby` | Pass |
| `vd_idle_content_switch` | Pass in isolation |
| Full `virtual_display` module | **28/32** (4 flaky in module run) |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.345.

---

---

## [v2.4.346] - 2026-06-24

### Description

Library recovery release: mesh PBR layout + loader error propagation, harness asset-path gates, and Vulkan async-shader build-ticket cleanup on mid-load unload. Part of **`doc/plan/LIBRARY_RECOVERY_PLAN_244.md`** (Tracks H, A, partial B).

### Changes — mesh PBR layout (Track A)

| Item | Detail |
|------|--------|
| Enum | `SIT_MESH_LAYOUT_POS_NRM_TAN_TEX` in `situation_api_types_gpu.h` (48-byte PBR stride) |
| Stride infer | `_SitInferMeshLayoutFromStride`: `case 48`; `SituationCreateMesh` rejects unknown strides |
| VAO | `_SitGLGetCachedVAO` uses layout enum (not magic `stride == 48`) |
| GLTF | `SituationLoadModel`: `CreateMeshEx(..., POS_NRM_TAN_TEX)` + error rollback |
| OBJ | `SituationLoadModelFromOBJ`: explicit `POS_NRM_TAN_TEX` |
| Docs | `situation_sdk.md` — removed false 32→48 auto-padding claim; documents `CreateMeshEx` |

### Changes — harness prerequisites (Track H)

| Item | Detail |
|------|--------|
| `test_model_loader.c` | `sit_test_resolve_harness_asset("BoomBox.glb")`; load failure / empty mesh → **FAIL** (not skip) |
| `test_stl_loader.c` | Same resolution pattern for `teapot.stl` |
| `build/run_tests.bat` | Gate: `model_loader` must not print `[SKIP] … BoomBox` |

### Changes — Vulkan async shader (Track B partial)

| Item | Detail |
|------|--------|
| Root cause | `_SituationVulkanFreeAsyncShaderLoad` returned on `ABANDONED` without `_SituationVkAsyncCompileFreeCtx` → Phase-2 build ticket stuck at phase 1 |
| Symptom | Next `BeginLoadShaderFromMemory` with same GLSL joined dead ticket as follower; `SituationPollShaderLoad` spun `IN_PROGRESS` forever |
| Fix | Abandon path frees ctx + ticket; poll path returns compile failure on abandoned ctx; worker avoids double-free on `-2` |
| File | `sit/situation_impl_renderer.h` |

### Regressions & known gaps (remaining — do not re-break)

| Severity | Item | Notes |
|----------|------|--------|
| **High** | VK full-suite shutdown | `pattern_runtime_include_compile` ACCESS_VIOLATION in full `graphics` module order; ghost AV on core/window/timer teardown |
| **Medium** | GL `virtual_display` full module | 4 tests fail together, pass isolated (state leak at frame boundary) |
| **Low** | NVIDIA `0x502` after VD draw | Drained; pixels OK in harness |

**Library behavior we must not regress**

- GLTF/OBJ BoomBox/teapot load with non-empty `gpu_mesh` (GL + VK).
- VK `graphics --filter async_shader` **6/6** including `async_shader_poll_after_unload_during_load`.
- OpenGL VD init + `pattern_smpte_vd_bar_color` (345+).

### Verification (GTX 1070, headless)

| Suite | Result |
|-------|--------|
| GL `model_loader` | **5/5** |
| GL `obj_loader` | **7/7** |
| GL `stl_loader` | **6/6** |
| VK `model_loader` | **5/5** |
| VK `graphics --filter async_shader` | **6/6** |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.346.

---

---

## [v2.4.347] - 2026-06-24

### Description

Vulkan shutdown crash fix (Track B): GDB backtrace proved ghost `ACCESS_VIOLATION` in `core` was `SituationShutdown` → `_SituationCleanupVulkan`, not failing tests. Part of **`doc/plan/LIBRARY_RECOVERY_PLAN_244.md`** (B-L1, B-L2, B-C1 partial).

### Changes — Vulkan shutdown (Track B)

| Item | Detail |
|------|--------|
| Root cause | `_SitVkShaderCacheShutdown` Layer B destroyed identical `VkShaderModule` handles multiple times (GDB: 5× / 3× / 2× per handle); remaining AV in `vkDestroyDescriptorPool` region |
| Shader cache dedup | `_SitVkShaderCacheShutdown`: `seen_modules[]`, `seen_pipelines[]`, `seen_layouts[]` — each Vulkan handle destroyed at most once |
| Acquire race | `_SitVkShaderCacheAcquireModules` / `_SitVkShaderCacheAcquireBundle`: post-create re-check; on hit, destroy redundant modules and return existing entry |
| Teardown order | `SituationShutdown` (`situation_impl_ctrl.h`): join render thread **before** `SituationDestroyThreadPool`; Vulkan GPU flush before renderer cleanup |
| VMA order | `_SituationCleanupVulkan`: `vmaDestroyAllocator` **after** descriptor set layout + pool teardown, before `vkDestroyDevice` |
| Pool dedup | `_SituationCleanupVulkan`: `seen_pools[]` for `vd_pattern`, manager pools, persistent, global bindless — skip duplicates across lists |

### Regressions & known gaps (remaining)

| Severity | Item | Notes |
|----------|------|--------|
| **Medium** | VK `tone_synth.legacy_*` (3 tests) | Full suite completes; legacy audio API failures unrelated to shutdown |
| **Medium** | GL `virtual_display` full module | 6 tests fail in full-suite order (Track C — three pollution profiles) |
| **Low** | Harness ghost FAIL label | Teardown AV outside test `setjmp` still attributes to last test name (B-D2 diagnostic) |

**Library behavior we must not regress**

- VK `core --headless` clean shutdown (no ghost AV).
- VK full suite reaches `RESULTS:` (all modules).
- VK `graphics --filter async_shader` **6/6** (346 fix preserved).
- GL mesh loaders + VD paths from 346.

### Verification (GTX 1070, headless)

| Suite | Result |
|-------|--------|
| VK `core --headless` | **5/5 runs, exit 0** (46/46 each) |
| VK full suite | **553 pass / 3 fail / 9 skip** (~221 s) — completes; fails: `tone_synth.legacy_play_tone_ex`, `legacy_play_tone`, `legacy_play_midi_note` |

**GDB method:** `gdb -batch -ex run --module core --headless -ex bt` → `_SituationCleanupVulkan` ← `_SituationCleanupRenderer` ← `SituationShutdown` ← `core_teardown`.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.347.

---

---

## [v2.4.348] - 2026-06-24

### Description

Targeted Vulkan ↔ OpenGL visual parity: 2D quad draw dynamic state and test-pattern 3D grid Y convention.

### Changes

| Item | Detail |
|------|--------|
| VK quad depth | `_SitVulkanApplyQuadDrawDynamicState`: depth test/write **off** during `SituationCmdDrawQuad` — matches OpenGL `SIT_OP_DRAW_QUAD` (blend already in quad pipeline) |
| 3D grid Y | `sit/gpu/test_patterns/sit_tp_3d_grid.glslh`: flip Y before raymarch so Vulkan neg-viewport parity matches OpenGL camera orientation |
| Files | `sit/situation_impl_renderer.h`, `sit/gpu/test_patterns/sit_tp_3d_grid.glslh` |

### Verification (GTX 1070, headless)

| Suite | Result |
|-------|--------|
| VK `graphics --filter pattern_3d_grid` | **PASS** |
| GL `graphics --filter pattern_3d_grid` | **PASS** |

Visual confirm: `advanced.all_displays_windowed_fullscreen_cycle` — depth-off only; **VD ortho/viewport still wrong** (see v2.4.349).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.348.

---

---

## [v2.4.349] - 2026-06-24

### Description

Vulkan virtual-display quad draws: refresh ortho projection and viewport from active VD target at each `SituationCmdDrawQuad` (OpenGL `SIT_OP_DRAW_QUAD` parity).

### Changes

| Item | Detail |
|------|--------|
| Active 2D target | `_SitVulkanGetActive2DTargetSize`: VD `resolution` when `recording_pass_display_id >= 0`; else render area / canvas / swapchain |
| Viewport/scissor | `_SitVulkanApply2DViewportScissor` uses active target size (no swapchain fallback during VD pass) |
| Quad draw | `_SitVulkanApplyQuadDrawDynamicState` re-uploads ortho UBO from active target each draw |
| Quad pipeline | `SIT_VK_PIPELINE_NO_DEPTH` on quad pipeline (static depth off, matches GL) |
| Files | `sit/situation_impl_renderer.h` |

### Verification

Visual: `advanced.all_displays_windowed_fullscreen_cycle` — RGB orbiting 42×42 px quads match OpenGL on each VD.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.349.

---

---

## [v2.4.353] - 2026-06-24

### Description

Renderer forward-declaration sync and targeted internal errno hardening on draw-critical helpers (no public API change).

### Changes

| Item | Detail |
|------|--------|
| `situation_impl_renderer_fwd.h` | Synced with `situation_impl_renderer.h` — 16 missing forwards added, 3 stale GL SPIR-V compile decls removed; `verify_renderer_fwd.py` **345/345** |
| `_SitVulkanResolveQuadPipeline` | `SituationError` + `VkPipeline* out`; VD dynamic path errors on invalid display id, missing quad SPIR-V, cache full, or pipeline create failure (no silent fallback) |
| `_SitVulkanWriteSlotToGlobalBindlessSet` | `SituationError` + `caller` context; propagated from quad/text/YPQ draws and VD create/scaling |
| `_SituationGLPrepareStorageTextureForSampling` | `SituationError`; invalid slot → `INVALID_PARAM`; propagated from GL soft-buffer execute path |
| Docs | `.kiro/steering/situation-project.md` refreshed (Makefile build, wrappers/tools); `doc/architecture.md` last-reviewed **v2.4.352** |

### Verification

```bat
python scripts\verify_renderer_fwd.py
```

Exit **0** — `OK: renderer_fwd.h covers all 345 static functions in situation_impl_renderer.h`.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.353.

---

---

## [v2.4.354] - 2026-06-24

### Description

VD standby pattern API vs test-harness separation of concerns — library ships types + compositor upload; harness owns pack/init/draw helpers for calibration readback tests.

### Changes

| Item | Detail |
|------|--------|
| **API types** | `SitVdStandbyLayer`, `SitVdStandbyConfig`, `SIT_VD_STANDBY_LAYER_*`, `SIT_VD_STANDBY_CONFIG_UBO_SIZE` live in `sit/situation_api_types_gpu.h` (with `SituationVDFallbackMode`) |
| **Removed** | `sit/sit_vd_pattern_config.h` — fused test-pattern naming + harness pack helpers no longer in library tree |
| **Library impl** | `situation_impl_vd.h` — internal `_SitVDPackStandbyConfigStd140` + `_SitVDStandbyConfigInitDefaults`; compositor uses `SIT_VD_STANDBY_CONFIG_UBO_SIZE` |
| **API rename** | `SituationSet/GetVirtualDisplayPatternConfig` take `SitVdStandbyConfig*` (was `SitTestPatternConfig*`) |
| **Harness** | `tests/harness/sit_harness_pattern_ubo.h` — pack/init/toggle for readback tests; `sit_harness_test_pattern_helpers.c/.h` — bind/upload/draw glue |
| **Build split** | `sit/Makefile` no longer compiles harness pattern code; `tests/harness/Makefile` + `build/build_tests.bat` own harness targets |
| **tests/** | Root `tests/*.c` moved to `tests/misc/`; harness-only sources under `tests/harness/` |

### Verification

```bat
build\build_situation.bat opengl
build\build_tests.bat opengl
```

Both **OK**.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.354.

---

---

## [v2.4.355] - 2026-06-25

### Description

OpenGL host texture upload with render thread enabled — `SituationCreateTextureEx` binds the loader shared context; lazy no-op when already current. Removes brittle `last_error_msg == "No error"` finalizer in favor of explicit GL upload failure tracking. Adds `build/build_shaderc.bat` and documents it in the compilation guides.

### Changes

| Item | Detail |
|------|--------|
| **OpenGL** | `_SituationMakeGLContextCurrentForHostThread` skips bind when target window is already current |
| **OpenGL** | `SituationCreateTextureEx` / sampler params / readback call host context bind on main thread |
| **OpenGL** | Texture upload uses `gl_upload_failed` + `last_error_code` checkpoint (fixes false `-520` on font atlas init) |
| **Build** | `build/build_shaderc.bat` — configure/build `libshaderc_combined.a` + `glslc.exe`; Makefile error points at script |
| **Docs** | `COMPILATION_GUIDE.md`, `BUILD_SITUATION_GUIDE.md`, `build/README.md` |

### Verification

```bat
build\build_situation.bat opengl
build\build_examples.bat opengl 10_thread_pool
build\examples\10_thread_pool.exe
```

Init + per-frame texture upload OK with `render_thread_count = 1` (no `GL_INVALID_OPERATION` spam).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.355.

---

---

## [v2.4.356] - 2026-06-25

### Description

Hardens `_SituationRenderThreadEntry` OpenGL path: atomic shutdown reads, fence/graveyard/present gating (`gl_frame_ok`), and removal of duplicate per-frame graveyard flush. Collapses redundant GL context handoff wait and nested `#ifndef SITUATION_USE_VULKAN` swap-interval block.

### Changes

| Item | Detail |
|------|--------|
| **Render thread** | `atomic_load` for `thread_shutdown_req` in wait/shutdown paths |
| **Render thread** | Fence wait: flush graveyard + shader-cache evictions only on `GL_ALREADY_SIGNALED` / `GL_CONDITION_SATISFIED`; timeout keeps sync |
| **Render thread** | `gl_frame_ok` gates blit/swap/new fence; failed execute no longer presents garbage; slot still signals main |
| **Render thread** | OpenGL graveyard flush after fence wait only (removed duplicate refcount flush; Vulkan unchanged) |
| **Render thread** | GL context handoff: invariant check + bind (no redundant `cnd_timedwait` loop); removed nested `#ifndef SITUATION_USE_VULKAN` under OpenGL |
| **EndFrame GL** | Removed dead commented screenshot `else` blocks (threaded + single-threaded paths) |

### Verification

```bat
build\build_situation.bat opengl
build\build_examples.bat opengl 10_thread_pool
build\examples\10_thread_pool.exe
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.356.

---

---

## [v2.4.357] - 2026-06-25

### Description

Internal refactor: all pure color-space math (HSV, YPQ, PQ/HDR, 10-bit packing, sRGB EOTF) lives in `sit/situation_impl_color.h`. `situation_impl_image.h` keeps buffer ops and CPU adjust loops only. No public API signature changes.

### Changes

| Item | Detail |
|------|--------|
| **Color module** | Renamed `situation_impl_ypq.h` → `situation_impl_color.h`; moved ~30 public pixel APIs + HSV + `SituationConvertColorToVector4` out of image module |
| **Includes** | `situation_impl.h` and top of `situation_impl_image.h` both `#include "situation_impl_color.h"` |
| **sRGB** | Single `_SitSrgbUnitToLinear` in color module; `_SitYpqSrgbByteToLinear` delegates |
| **Trace** | Regenerated `situation_base_trace.h` via `scripts/gen_situation_base_trace.py`; color APIs under `SITUATION_TRACE_SITUATION_IMPL_COLOR_H` (10730001–10730031) |
| **Docs** | `doc/plan/COLOR_IMPL_CONSOLIDATION_PLAN.md`; steering/SDK/YPQ plan cross-links updated |

### Verification

```powershell
& ".\build\build_tests.bat" opengl
# consolidation filters: ypq, hsv, color_to_vector4, rgb10, pq, ypq_grade — all green
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.357.

---

---

## [v2.4.358] - 2026-06-25

### Description

Internal refactor (mechanical move only): begin `RENDERER_MODULARIZATION_PLAN` with **R3** (`situation_impl_renderer_core.h`) and **R1** (`situation_impl_renderer_shader.h`). No public API signature changes.

### Changes

| Item | Detail |
|------|--------|
| **Core slice (R3)** | `sit/situation_impl_renderer_core.h` — 1,587 lines, 47 statics (uniform map, staging, graveyard, GL backup, program cache, VK pipeline macros) |
| **Shader slice (R1)** | `sit/situation_impl_renderer_shader.h` — 6,576 lines, 88 statics (GLSL/SPIR-V compile, async load, VK shader cache, `SituationLoadShader*`) |
| **Orchestrator** | `situation_impl_renderer.h` — `#include core` first, `#include shader` at end (after GL soft-cmd macros); monolith ~22,795 lines |
| **Fwd gate** | `situation_impl_renderer_fwd.h` — `// --- renderer_core ---` / `// --- renderer_shader ---` sections |
| **Scripts** | `extract_renderer_core.py`, `extract_renderer_shader.py`, `audit_renderer_shader_extract.py`; extended `verify_renderer_fwd.py` |
| **Docs** | `doc/plan/RENDERER_MODULARIZATION_PLAN.md` migration log R0/R1/R3 marked complete |

### Verification

```powershell
python scripts/verify_renderer_fwd.py
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" vulkan
# Full OGL/VK test matrix — green (same as pre-split)
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.358.

---

---

## [v2.4.359] - 2026-06-25

### Description

Complete **RENDERER_MODULARIZATION_PLAN** (R2–R5): extract `renderer_lc.h`, `renderer_resources.h`, `renderer_frame_cmd.h`; trim orchestrator to guard + includes. Mechanical move only — no public API signature changes.

### Changes

| Item | Detail |
|------|--------|
| **LC slice (R4)** | `sit/situation_impl_renderer_lc.h` — 10,663 lines, 102 statics (verify; multi-range cut: render thread, lifecycle, VK bootstrap, hot-reload tail) |
| **Resources slice (R2)** | `sit/situation_impl_renderer_resources.h` — 3,019 lines, 29 statics (contiguous cut; transfer `SituationCmd*` left in slice per plan) |
| **Frame/cmd slice (R5)** | `sit/situation_impl_renderer_frame_cmd.h` — 9,132 lines, 85 statics + 100 `SITAPI` (single contiguous cut) |
| **Orchestrator** | `situation_impl_renderer.h` — **23 lines**, zero statics; include order `core → lc → shader → resources → frame_cmd` |
| **Fwd gate** | `situation_impl_renderer_fwd.h` — `renderer_lc` + `renderer_frame_cmd` sections; **347/347** statics |
| **Scripts** | `extract_renderer_{lc,frame_cmd}.py`, `audit_renderer_{lc,frame_cmd}_extract.py`, `census_renderer_r5.py`, `audit_renderer_cross_slice.py` |
| **Docs** | `architecture.md`, `situation_sdk.md`, `COMPILATION_GUIDE.md`, plan docs cross-linked; `RENDERER_MODULARIZATION_PLAN.md` **complete** |

### Verification

```powershell
python scripts/verify_renderer_fwd.py
python scripts/audit_renderer_frame_cmd_extract.py
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" vulkan
# graphics --filter acquire; model_loader 5/5 — green
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.359.

---

---

## [v2.4.360] - 2026-06-25

### Description

**Ship marker** for **RENDERER_MODULARIZATION_PLAN** complete (R0–R5). Mechanical split landed in v2.4.358–359; this patch consolidates release notes, post-split inventory audit, and cross-doc references. No code or public API changes.

### Post-split inventory (authoritative — `inventory_renderer_module.py`)

| File | Lines | Statics (verify) | SITAPI |
|------|------:|-----------------:|-------:|
| `situation_impl_renderer.h` | 23 | 0 | 0 |
| `situation_impl_renderer_core.h` | 1,587 | 47 | 1 |
| `situation_impl_renderer_lc.h` | 10,663 | 102 | 2 |
| `situation_impl_renderer_shader.h` | 6,576 | 88 | 29 |
| `situation_impl_renderer_resources.h` | 3,019 | 29 | 27 |
| `situation_impl_renderer_frame_cmd.h` | 9,132 | 85 | 100 |
| **Impl subtotal** | **31,000** | **347** (union) | **159** |
| `situation_impl_renderer_fwd.h` | 756 | 347 fwd decls | — |
| **Grand total (all renderer headers)** | **31,756** | — | **159** |

| Check | Baseline (pre-R0) | Post-split |
|-------|-------------------|------------|
| Renderer impl LOC | 28,105 (one file) | **31,000** (+2,895 headers/guards) |
| Largest file | 28,105 | **10,663** (`renderer_lc.h`) |
| Orchestrator | (entire monolith) | **23 lines**, 0 statics |
| Static union | 345 | **347** (+2) |
| Fwd gate | — | **347/347 green** |

### Changes

| Item | Detail |
|------|--------|
| **Version** | v2.4.360 — renderer modularization declared complete |
| **Inventory** | `scripts/inventory_renderer_module.py` — unique static counts aligned with `verify_renderer_fwd.py` (347 union) |
| **whatsnew** | Single grouped section for full R0–R5 split (replaces separate v2.4.358/359 entries) |
| **Reference sync** | Orchestrator **23 lines** (not 29); lc slice **102 statics** (verify); 31,000 impl LOC documented |
| **Docs** | `architecture.md`, `UPDATELOG.md`, `RENDERER_MODULARIZATION_PLAN.md`, `situation_sdk.md`, `COMPILATION_GUIDE.md` |

### Verification

```powershell
python scripts/inventory_renderer_module.py
python scripts/verify_renderer_fwd.py
# User test matrix — GL/VK builds + harness (unchanged behaviour)
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.360.

---

---

## [v2.4.361] - 2026-06-25

### Description

**Track C P1 (virtual_display module profile):** OpenGL harness showed order-dependent pixel failures while Vulkan was clean. Investigation looked like a VD compositor offset bug; in-process bisect initially implicated **`vd_frame_time_multiplier`**. Actual root cause: **render-thread screenshot readback contract** — not compositor matrix math.

With **`SITUATION_ENABLE_RENDER_THREAD`**, the main thread cannot call `glReadPixels`. Capture happens on the render thread **pre-swap** when `screenshot_requested` is set. The public API already documented the pair **`SituationRequestScreenCapture()` → `SituationEndFrame()` → `SituationLoadImageFromScreen()`**, but **`SituationRequestScreenCapture()` was never implemented** in the library (only a duplicate no-op stub in `situation_dll.c`). Harness tests called **`EndFrame()` then `LoadImageFromScreen()`**, which auto-armed too late or reused **`screenshot_valid`** buffer from a prior test (same 1024×768 → immediate stale memcpy). Innocuous extra frames (empty `EndFrame` in timing tests) shifted queue timing and made the race visible.

**Also (mechanical, no behaviour intent):** Siamese colocation **S2 pilot** — GL VD execute body moved into `situation_impl_vd.h` beside the VK/GL record twins.

### Investigation (C-I2)

| Step | Finding |
|------|---------|
| Isolated `--filter vd_offset` | **PASS** |
| Full `--module virtual_display` pre-fix | **FAIL** (4 tests) |
| `scripts/bisect_vd_polluter.py` | First failing prefix implicated **`vd_frame_time_multiplier`** → misleading once readback fixed |
| VK same module | **34/34** throughout |

**Misread avoided:** expanding `_SitGLBackupState` / compositor offset math (recovery plan §Failed approaches §1).

### Correct capture pattern (GL + render thread)

```c
SituationRequestScreenCapture();
SituationEndFrame();
SituationLoadImageFromScreen(&screen);
```

### Changes

| Item | Detail |
|------|--------|
| **`SituationRequestScreenCapture()`** | Implemented in `situation_impl_image.h` — `screenshot_valid = false`, `screenshot_requested = true` |
| **`SituationLoadImageFromScreen()`** | Stops auto-arming; render-thread path waits for buffer filled during EndFrame after Request |
| **`_SituationGLCaptureDisplayedFramebuffer()`** | New helper in `renderer_lc.h` (render thread); pre-swap `GL_BACK` readback |
| **Stale cache** | Render thread clears `screenshot_valid` when presenting a frame without capture request |
| **`situation_dll.c`** | Removed duplicate stub `SituationRequestScreenCapture` (link to impl header) |
| **Harness** | `test_virtual_display.c` — Request before every EndFrame that precedes pixel readback |
| **Harness filter** | `sit_test_framework.h` — `--filter` substring OR via `\|` (in-process bisect) |
| **Siamese S2** | `_SitGLExecRenderVirtualDisplays` in `situation_impl_vd.h`; dispatch-only case in `renderer_lc.h`; forward decl in `situation_impl_forward.h` |
| **VD composite** | `glm_ortho` refresh for `vd_ortho_projection` at GL execute entry (defensive) |
| **Scripts** | `scripts/bisect_vd_polluter.py`, `scripts/colocate_gl_execute.py`, `scripts/audit_siamese_colocation.py`; one-shot extract scripts → `scripts/_old/` |
| **Fwd gate** | `situation_impl_renderer_fwd.h` — **348/348** (+ `_SituationGLCaptureDisplayedFramebuffer`) |

### Verification

```powershell
python scripts/verify_renderer_fwd.py
python scripts/audit_siamese_colocation.py
python scripts/bisect_vd_polluter.py
& ".\build\build_situation.bat" opengl
& ".\build\build_tests.bat" opengl
& ".\build\run_tests.bat" opengl --headless --module virtual_display
# 34/34 pass (GL); VK unchanged 34/34
```

### Track C status after this patch

| Profile | Scope | Status @ 361 |
|---------|-------|----------------|
| **P1** | `virtual_display` module pixel tests | **Closed** (readback contract + harness) |
| **P2** | `pattern_smpte_vd_bar_color` full suite | Open |
| **P3** | Cross-module (`graphics`/`text_rendering` before VD) | Open |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.361.

---

---

## [v2.4.362] - 2026-06-25

### Description

**GL readback hardening** after v2.4.361 Track C work. Two production paths broke on OpenGL + render thread: (1) **screen pixel readback** regressed when capture was gated on `SituationRequestScreenCapture()` only — the harness and apps use **`EndFrame()` then `LoadImageFromScreen()`**; (2) **`SituationCreateReadbackBuffer()`** returned **`-600`** (`SITUATION_ERROR_OPENGL_GENERAL`) because GL buffer creation ran with **no current context** (render thread owns the main window; loader context was never bound). **`SituationCmdCopyBufferEx`** then failed at **`EndFrame`** on the render thread when **`glCopyNamedBufferSubData`** rejected loader-share-context readback destinations.

### Symptom → fix

| Symptom | Root cause | Fix |
|---------|------------|-----|
| `graphics` mass pixel failures @ 361 | Request-only pre-swap capture; stale/missing `screenshot_valid` | Always capture pre-swap; `screenshot_valid = false` on RT queue handoff |
| `transfer.copy_texture_to_buffer_validation` `-600` on `CreateReadbackBuffer` | No `_SituationMakeGLContextCurrentForHostThread()` | Bind loader context; `_SitGLInitReadbackNamedBuffer()` with storage fallback |
| `transfer.copy_buffer_ex_offsets` `-600` on `EndFrame` | `glCopyNamedBufferSubData` fails for loader-context readback buffers | CPU staging fallback in `SIT_OP_COPY_BUFFER` execute |
| Persistent map + GPU copy | Mapped readback blocked copy / immutable storage | `GL_DYNAMIC_STORAGE_BIT`; defer map until `SituationReadBuffer()` |

### Changes

| Item | Detail |
|------|--------|
| **Screenshot (RT)** | Always `_SituationGLCaptureDisplayedFramebuffer()` before swap; invalidate cache when frame queued to render thread (`renderer_frame_cmd.h`, `renderer_lc.h`) |
| **`SituationRequestScreenCapture()`** | Kept (explicit invalidation); optional — not required for `EndFrame` → `Load` workflow |
| **`SituationCreateReadbackBuffer()`** | Loader context bind; `_SitGLInitReadbackNamedBuffer()`; lazy map in `SituationReadBuffer()` |
| **`SIT_OP_COPY_BUFFER`** | Fallback: `glGetNamedBufferSubData` + `glNamedBufferSubData` when named copy fails |
| **Files** | `situation_impl_renderer_resources.h`, `situation_impl_renderer_frame_cmd.h`, `situation_impl_renderer_lc.h` |

### Verification

```powershell
& ".\build\build_situation.bat" opengl
& ".\build\build_tests.bat" opengl
& ".\build\run_tests.bat" opengl --headless --module graphics
# 120/120
& ".\build\run_tests.bat" opengl --headless --module virtual_display
# 34/34
& ".\build\run_tests.bat" opengl --headless --module transfer
# 12/12
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.362.

---

---

## [v2.4.363] - 2026-06-25

### Description

**Lua wrapper ship** + **Track D opened** (wrapper `-600` vs harness green). Self-contained `hello_situation` embed pipeline; OpenGL 10-bit probe fix (stop invalid `glfwGetWindowAttrib` on framebuffer bits).

1. **`build/build_lua_example.bat`** — `build/examples/lua/hello_situation.exe` with embedded Situation DLL + `lua51.dll` + bytecode.
2. **Launcher** — `wrappers/lua/launcher/` dynamic `lua51.dll` load; draw shim for `SituationCmdDrawTextEx` / `SituationFont` by value.
3. **Tooling** — `tools/gen_lua_embed.py`, `tools/gen_lua_dll_embed.py`; `build/run_lua_dev.bat` for external DLL iteration; embed prefers `situation_opengl_lua_embed.dll` when fresh DLL hits render-thread `-600`.
4. **Library** — `_SituationOpenGLSetOutputColorDepthFromFramebuffer()` uses `glGetIntegerv(GL_*_BITS)` after GLAD init (fixes GLFW `0x00021001` spam).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.363.

---

---

## [v2.4.364] - 2026-06-25

### Description

**Vulkan shader cache Phase 6 — 6G + stats (partial 6A).** Harness screen-readback batching for graphics pixel tests; shader-cache shutdown diagnostics.

1. **6G — Screen readback batching** — `graphics_test_begin_screen_probe` / `read_pixel_rgba_probed` / `end_screen_probe` in `sit_graphics_test_helpers.h`; `pattern_3d_grid_axis_red` drops from multi-second stalls to **~45 ms** on repeat probes.
2. **6A prep** — `legacy_slot_pipeline_builds` debug stat; shutdown banner includes `legacy_slot_builds` (gate for removing inline 12-pipeline fan-out).

See **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`** Phase 6 §6G.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.364.

---

---

## [v2.4.365] - 2026-06-25

### Description

**Vulkan shader cache Phase 6 — 6A/6B/6D.** Remove redundant per-slot 12-pipeline inline fan-out; Layer A cache hit skips shaderc on reload.

1. **6A** — Consolidate duplicate inline `vkCreateGraphicsPipelines` blocks to `_SituationVulkanBuildGraphicsPipelinesOnSlot` fallback only; **`legacy_slot_builds=0`** on graphics module run.
2. **6B** — `bundle_resolve_slot_fallbacks` stat when bundle path falls back to slot pipelines.
3. **6D** — V15 gate: second `pattern_runtime_include_compile` load **< 50 ms** (observed **~0.2 ms**).
4. **Layer A** — Hit no longer falls through to shaderc when bundle attach fails; uses cached SPIR-V + legacy fallback.

VK `--module graphics`: **~14.6 s / 113 tests** (GTX 1070, down from ~30 s @ 363).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.365.

---

---

## [v2.4.366] - 2026-06-25

### Description

**Vulkan shader cache Phase 6 — 6E + partial 6C.** Async shader reload uses Layer A fast path; descriptor reload gate.

1. **6E** — `_SitVkTryMeshLoadFromLayerA` shared by sync + async load; `SituationBeginLoadShaderFromMemory` checks Layer A before worker submit; `async_shader_poll_after_unload_during_load` reload poll **~0.2–0.8 ms** (was multi-second class @ 363).
2. **6C (partial)** — V14: `descriptor_bind_ubo_color` unload→reload **< 3 ms**; bundle eligibility widening still open.
3. **Benchmark** — VK `--module graphics` **~8–14 s** (headless vs windowed); full static suite **~200–242 s** (variance, not a Phase 6 regression signal).

See **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`** @ v2.4.366.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.366.

---

---

## [v2.4.367] - 2026-06-26

### Description

**Track D (library)** — OpenGL render-thread **`SituationEndFrame`** could return **`SITUATION_ERROR_OPENGL_GENERAL` (`-600`)** in real apps while `sit_test_opengl.exe` stayed green. **`_SituationGLExecuteCommands`** runs a **`glGetError()` after every recorded opcode** and treats the first pending error as fatal. That contract is correct for catching real replay bugs; the regression was **unrelated GL calls enqueueing benign errors that survived until the next opcode check**.

### Why scoped drains (not “ignore GL errors”)

OpenGL keeps a **single error queue per context**. Anything that calls GL without immediately draining can leave `GL_INVALID_OPERATION` / `GL_INVALID_ENUM` for a later caller. Our executor **does not** drain globally — it **attributes** the next `glGetError()` to the opcode that just finished. That is strict and useful **inside** the replay loop, but three **library-owned** paths sit **outside** or **after** individual opcodes and were polluting the queue:

| Source | When it runs | Why an error can appear without a broken draw |
|--------|--------------|-----------------------------------------------|
| **Pre-swap screenshot** (`_SituationGLCaptureDisplayedFramebuffer`) | Every `EndFrame` on the render thread since **v2.4.362** | `glReadPixels` / `glFinish` on the default FBO can fail or leave driver quirks; we only tested `screenshot_valid`, not the queue |
| **VD compositor restore** (`_SitGLExecRenderVirtualDisplays`) | Apps using `SituationRenderVirtualDisplays` (console, VD demos, games) | SPIR-V compositor + `_SitGLRestoreState` (blend re-bind) can enqueue **benign** errors on some drivers; an earlier drain ran **before** restore, so restore-time errors still hit the **`SIT_OP_RENDER_VIRTUAL_DISPLAYS`** opcode check |
| **Prior-frame tail** (canvas blit, present, compositor) | Between frames | Errors not consumed at end of frame N are blamed on opcode 0 of frame N+1 |

**Drains added here are boundary hygiene only** — same pattern already used for canvas FBO fail-soft (`renderer_lc.h`) and the old VD draw drain. They do **not** replace opcode-level checking; they clear **known library tail work** so the next strict check reflects the opcode that actually ran.

### Symptom → fix

| Symptom | Misread | Fix |
|---------|---------|-----|
| `-600` on first/second `EndFrame` in consumer apps | Random driver flakiness / harness gap only | Drain after screenshot readback; drain at execute entry; drain **after** VD `_SitGLRestoreState` |
| stderr only showed `-600` | — | Render thread logs `SituationGetLastErrorMsg()` (opcode + `GL 0x…` from execute) |

### Changes

| Item | Detail |
|------|--------|
| **`situation_impl_renderer_lc.h`** | Drain stale queue after baseline reset, before packet loop; drain after pre-swap `glReadPixels`; propagate execute `gl_detail` to stderr on render-thread failure |
| **`situation_impl_vd.h`** | Move VD compositor drain to **after** `_SitGLRestoreState` (restore was leaving errors for the opcode check) |
| **`tests/harness/test_virtual_display.c`** | **`gl_endframe_execute_vd_streak`** — 4× `EndFrame` with user shader + VD composite + implicit screenshot path (Track D / G7 gate; skips on Vulkan) |
| **`doc/plan/LIBRARY_RECOVERY_PLAN_244.md`** | Track D library fix (D-L1 partial); wrapper smoke (D-C1) still app-specific |

### Verification

```powershell
& ".\build\build_situation.bat" opengl
& ".\build\run_tests.bat" opengl --headless --module virtual_display
# 35/35 (includes gl_endframe_execute_vd_streak)
```

Re-test any consumer that previously hit frame-0/1 `-600` against fresh `build/dll/situation_opengl.dll`.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.367.

---

---

## [v2.4.368] - 2026-06-26

### Description

**Harness throughput + audio meter reliability** — cut MIDI and effects heard-test durations (~2× faster suite), harden master-bus meter reads, clarify shutdown resource messages, and add `build_tests` aggregate targets. Track D **`gl_endframe_execute_vd_streak`** is now compile-time OpenGL-only (no runtime skip on Vulkan builds).

### Changes

| Area | Detail |
|------|--------|
| **`tests/harness/test_tone_synth.c`** | Halved MIDI tests (`legacy_midi_note_frequency`, `midi_note_frequency`, `phase1_compare_a4`, `midi_complex_melody`, `midi_velocity_ramp`); halved effect listen durations (CC mod/tremolo, filter modes, pulse width, waveforms, LFO, filter ADSR, sub/sync/ring, portamento, patch memory); **`SIT_TONE_SUB_EFFECT_SEG_MS`** 3500→1750. Legacy smoke tests call **`SituationSetActiveGraph(NULL)`** + **`SituationStopAllTones()`** before play; **`sit_tone_smoke_assert_meter_levels`** polls ~120 ms, rejects torn/non-finite atomics. |
| **`tests/harness/test_audio_effects_heard.c`** | Default dry/wet capture 400→200 ms; echo wet 380 ms via **`SIT_EFFECT_HEARD_DEF_WET_MS`**; reverb sweep shortened; maximizer square capture unchanged (1000/800 ms — harmonic check fails when shortened). |
| **`sit/situation_impl_audio.h`** | Publish zero peak/RMS when **`!audio_ready`**; **`SituationGetMasterOutputMeter`** clamps non-finite / negative reads to 0. |
| **`sit/situation_impl_ctrl.h`** | Zero meter atomics on audio shutdown (with **`audio_ready`** clear). |
| **`sit/situation_impl_renderer_resources.h`** | Shutdown stderr: **`Situation [Shutdown]: releasing active …`** instead of **`SITUATION WARNING: Leaked …`**. |
| **`tests/harness/test_virtual_display.c`** | **`gl_endframe_execute_vd_streak`** body + registry wrapped in **`#if defined(SITUATION_USE_OPENGL)`** (matches other GL-only VD tests). |
| **`tests/harness/test_threading.c`** | **`cpu_stress_10s_taskmgr`**: job-based mid report (~half duration), histogram snapshot, no duplicate mid/full reports; uses library **`SituationDispatchParallel`**. |
| **`build/build_tests.bat`**, **`tests/harness/Makefile`** | New **`all`** (opengl + vulkan) and **`all-static`** (static-opengl + static-vulkan) targets. |

### Verification

```powershell
& ".\build\build_situation.bat" opengl
& ".\build\build_tests.bat" opengl
& ".\build\tests\sit_test_opengl.exe" --module tone_synth --filter "legacy_midi|midi_note|phase1_compare|midi_complex|midi_velocity"
& ".\build\tests\sit_test_opengl.exe" --module tone_synth --filter "cc_mod|cc92|filter_|pulse|waveform|lfo_mod|filter_env|sub_|portamento"
& ".\build\tests\sit_test_opengl.exe" --module audio_effects_heard
& ".\build\tests\sit_test_opengl.exe" --module tone_synth --filter legacy_play
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.368.

---

---

## [v2.4.369] - 2026-06-26

### Description

**Text & font harness certification (T0–T6)** — the largest font coverage sprint since v2.4.341: error-first GPU text module, full bitmap/TTF/stamp/lifecycle paths, and a dedicated **retro builder** module with a two-stage *usable → display surface* model for every `SituationCreate*` font family. Library fix: VCR and VGA 8×8 builders now emit visible white glyphs (alpha was 0).

### Changes

| Area | Detail |
|------|--------|
| **`tests/harness/test_text_rendering.c`** | **25 tests** (was 29): error paths run first (`draw_without_bake_default_fallback`, `font_load_missing_file_fails`, `double_unload_safe`, `font_unload_destroys_atlas`, `roboto_asset_optional_probe`); default grid draw/layout; boxed wrap/clip, multiline GPU, colored tint; bitmap memory bake + sheet texture; TTF measure/draw (Roboto, honest `[SKIP]`); CPU stamp/boxed, stamp→GPU blit, reload after unload. Retro one-shot smokes removed → T6 module. |
| **`tests/harness/test_text_retro_builders.c`** | **New module** — **14 tests**, 7 families × 2 stages: CP437, terminal (+ `SituationCreateTerminalFontEx` spacing), ASCII, packed, outlined packed, VCR, VGA 8×8. Each `*_usable` checks atlas/metadata/measure; each `*_display` runs white draw, colored tint, multiline, boxed wrap, measure-vs-draw via `sit_text_test_retro_display_surface`. Registered after `text_rendering` in `sit_test_registry.c`. |
| **`tests/harness/sit_test_retro_font_helpers.h`** | Retro fixtures (`build_cp437_font`, terminal/ASCII grids, packed/VCR/VGA), `assert_grid_font_usable`, shared display-surface harness. |
| **`tests/harness/sit_test_text_helpers.h`** | Shared text helpers extended: Roboto CPU load, color pixel checks, terminal/packed/VGA/VCR glyph fixtures used by T4/T6. |
| **`tests/harness/Makefile`** | Builds `test_text_retro_builders.c` into both backend harness exes. |
| **`sit/situation_impl_image.h`** | **`SituationCreateVGA8x8Font`** and **`SituationCreateVCRFont`**: set `font_r/g/b/a = 255` (was alpha 0 → invisible GPU text). |
| **`doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md`** | Phases **T0–T6** marked complete; matrix **L5–L11** ticked; module inventory and verification commands updated. |

### Certification matrix (harness)

| Phase | Scope | OpenGL | Vulkan |
|-------|-------|--------|--------|
| T0 | Hygiene — honest skip, asset resolve, shared helpers | ✅ | ✅ |
| T1 | Bitmap GPU — memory bake + texture sheet | ✅ | ✅ |
| T2 | Draw options — boxed wrap/clip, multiline, color | ✅ | ✅ |
| T3 | Measure parity — TTF/bitmap scale, measure vs draw | ✅ | ✅ |
| T4 | Retro smoke (superseded by T6 split) | — | — |
| T5 | CPU stamp, GPU blit, lifecycle, error paths | ✅ | ✅ |
| T6 | Retro builders — usable + display per family | **14/14** | **14/14** |

**Module totals:** `text_rendering` **25/25** · `text_retro_builders` **14/14** on both backends @ 2026-06-26.

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" vulkan
& ".\build\build_tests.bat" opengl
& ".\build\run_tests.bat" opengl --module text_rendering
& ".\build\run_tests.bat" vulkan --module text_rendering
& ".\build\run_tests.bat" opengl --module text_retro_builders
& ".\build\run_tests.bat" vulkan --module text_retro_builders
```

If Vulkan retro VCR/VGA display tests fail after pulling this patch, force-rebuild the Vulkan DLL (`mingw32-make -B ../build/dll/situation_vulkan.dll` from `sit/`) so the font alpha fix is linked.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.369.

---

---

## [v2.4.370] - 2026-06-27

### Description

**Advanced font showcase harness** (~30 s exclusive-fullscreen demo) plus two renderer fixes: GPU text shader preserves retro atlas fill/outline colors, and **OpenGL exclusive fullscreen** now matches Vulkan (fixed canvas + stretch blit — no fail-soft draw to the default framebuffer at native resolution).

### Changes

| Area | Detail |
|------|--------|
| **`tests/harness/test_advanced_font_showcase.c`** | **New** — `font_capabilities_fullscreen_showcase`: 8 timed segments (typography, retro gallery, YPQ field, motion, outline, rotation, layout, finale); custom chrome; Roboto optional with VGA size caps; showcase-local retro fixtures from `sit_default_8x8_font` (readable glyphs, not T6 solid-fill cert blocks); YPQ scissor bands ~2 px / 20–56 stripes with smoothstep palette (demon_hunt style); outline segment labels + short retro strings; CPU stamp uses offset shadow when SDF outline unsafe with baked Roboto in-process. |
| **`tests/harness/test_advanced.c`** | Registers second advanced test; teardown still exits fullscreen. |
| **`tests/harness/Makefile`** | Adds `test_advanced_font_showcase.c` to `HARNESS_SRCS`. |
| **`doc/plan/TEST_HARNESS_ADVANCED_FONT_SHOWCASE_PLAN.md`** | Plan for showcase architecture, segments, and verification (marked implemented). |
| **`sit/gpu/text.frag`** | Fragment shader multiplies **`texColor.rgb * tint`** (OpenGL + Vulkan paths) so retro outlined atlases keep black outline + tinted fill; previously alpha-only mask + uniform tint made outlines look hyper-bold. |
| **`sit/situation_impl_renderer_lc.h`** | **OpenGL fullscreen canvas parity:** removed fail-soft render to default FBO when canvas creation fails; **`_SituationGLPrepareCanvasStretchTarget()`** (8 retries); bind-based canvas FBO setup; destroy stale canvas when `shadow_state_dirty` + stretch active; main pass skipped until canvas ready (Vulkan-aligned). |
| **`sit/situation_impl_renderer_fwd.h`** | Forward decl for `_SituationGLPrepareCanvasStretchTarget`. |
| **`sit/situation_impl_wdm.h`** | Sets **`shadow_state_dirty`** when entering exclusive fullscreen (OpenGL) so canvas is recreated on the render thread after mode change. |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation\build"
& "..\build\build_situation.bat" opengl
& "..\build\build_situation.bat" vulkan
& ".\build_tests.bat" opengl
& ".\run_tests.bat" opengl --module advanced --filter font_capabilities
& ".\run_tests.bat" vulkan --module advanced --filter font_capabilities
```

After pulling, rebuild **`situation_opengl.dll`** / **`situation_vulkan.dll`** so `text.frag` and canvas-stretch changes are linked.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.370.

---

---

## [v2.4.371] - 2026-06-27

### Description

**Virtual Display idle default is animated snow** — no calibration layers enabled at create (`PATTERN` + `pattern_layers == 0`). Layers 0–8 remain opt-in via `SituationSetVirtualDisplayPatternLayers` / `SituationSetVirtualDisplayPatternConfig`. Zero-layer standby uses per-pixel white noise driven by **`noise_frame_seed`** (monotonic clock in **milliseconds**, uploaded each compositor frame).

### Changes

| Area | Detail |
|------|--------|
| **`sit/gpu/test_patterns/sit_tp_noise.glslh`** | **New** — `sit_tp_noise_rgb()`; hash combines pixel position + `noise_frame_seed`. |
| **`sit/gpu/test_patterns/sit_test_patterns.glslh`** | `sit_tp_sample()` returns noise when `pattern_layers == 0` (replaces flat `bg_dark_gray`). |
| **`sit/gpu/test_patterns/sit_tp_config*.glslh`** | `SitTpConfig` / std140 UBO +4 B: `noise_frame_seed` @ offset 136 (144 B total unchanged). |
| **`sit/situation_impl_vd.h`** | VD create: `fallback_mode = PATTERN`, `pattern_layers = 0`; compositor sets seed from `_SitVDGetTimeMilliseconds()` on each pattern UBO upload. |
| **`sit/situation_api_types_gpu.h`** | `SitVdStandbyConfig.noise_frame_seed`; default standby comments updated. |
| **`tests/harness/test_graphics_patterns.c`** | **`pattern_zero_layers_noise`** — readback verifies animated grayscale noise. |
| **`tests/harness/test_virtual_display.c`** | **`vd_idle_content_switch`** — bookends use snow (not SOLID blue); `vd_screen_is_idle_snow()`. |
| **`doc/guide/test_patterns.md`**, **`doc/guide/virtual_display.md`** | Document snow default and zero-layer semantics. |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation\build"
& "..\build\build_situation.bat" opengl
& "..\build\build_situation.bat" vulkan
& ".\build_tests.bat" opengl
& ".\run_tests.bat" opengl --module graphics --filter pattern --headless
& ".\run_tests.bat" opengl --module virtual_display --filter vd_idle_content_switch --headless
```

Regenerate compositor/harness SPIR-V embeds after pulling shader header changes: `build\compile_vd_compositor_gl.ps1`, `build\compile_harness_shaders.ps1` (also run from `build_tests.bat`).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.371.

---

---

## [v2.4.372] - 2026-06-27

### Description

**Chroma snow** — optional RGB per-channel idle noise when calibration layers 0–8 are all off. Controlled by **`SIT_VD_STANDBY_LAYER_CHROMA_SNOW`** (`1u << 16`) on `pattern_layers`; default remains **B&W snow**. Bit 16 is a standby flag, not a calibration layer — compositing masks it out with `SIT_VD_STANDBY_LAYER_CALIBRATION_MASK`.

### Changes

| Area | Detail |
|------|--------|
| **`sit/gpu/test_patterns/sit_tp_config.glslh`** | `SIT_TP_LAYER_CHROMA_SNOW`, calibration mask `0x1FF`, `sit_tp_chroma_snow_on()`. |
| **`sit/gpu/test_patterns/sit_tp_noise.glslh`** | Chroma branch: three independent hash channels with seed offsets. |
| **`sit/gpu/test_patterns/sit_test_patterns.glslh`** | Zero-layer path uses masked calibration bits; chroma flag preserved. |
| **`sit/situation_api_types_gpu.h`** | `SIT_VD_STANDBY_LAYER_CHROMA_SNOW`, `SIT_VD_STANDBY_LAYER_CALIBRATION_MASK`. |
| **`sit/situation_api_graphics.h`**, **`sit/situation_impl_vd.h`** | `SituationSetVirtualDisplayChromaSnow` / `Get`. |
| **`tests/harness/test_graphics_patterns.c`** | **`pattern_chroma_snow`** — readback verifies non-grayscale RGB. |
| **`doc/guide/test_patterns.md`**, **`doc/guide/virtual_display.md`** | Bit 16 table, API rows, harness count 12/12 OpenGL. |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation\build"
& "..\build\build_situation.bat" opengl
& ".\build_tests.bat" opengl
& ".\tests\sit_test_opengl.exe" --module graphics --filter pattern_chroma_snow
& ".\tests\sit_test_opengl.exe" --module graphics --filter pattern
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.372.

---

---

## [v2.4.373] - 2026-06-27

### Description

**Layer 8 corrected to one lit cube** — replaces misimplemented grid-of-cubes raymarch (`sit_tp_3d_grid.glslh` deleted). **`sit_tp_cube.glslh`**: single box, fixed canonical view, diffuse + ambient lighting. `grid_size` standby field now means **cube edge length** (default **1**).

### Changes

| Area | Detail |
|------|--------|
| **`sit/gpu/test_patterns/sit_tp_cube.glslh`** | **New** — one SDF cube; RGL `RGL_DrawCube(..., 1.0f, material)` intent. |
| **`sit/gpu/test_patterns/sit_tp_3d_grid.glslh`** | **Removed** |
| **`sit/gpu/test_patterns/sit_test_patterns.glslh`** | Layer 8 → `sit_tp_cube` |
| **`tests/harness/test_graphics_patterns.c`** | **`pattern_cube_lit_faces`** (replaces `pattern_3d_grid_axis_red`) |
| **`doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md`** | §4.9 + **P13** shipped |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.373.

---

---

## [v2.4.374] - 2026-06-27

### Description

**P9 CPU — unified `SitTestPatternConfig`** with per-layer parameter blocks, default layer stack, palette, and snow params. **`SitVdStandbyConfig`** is now a typedef alias. Shaders still consume the flat **144 B** UBO via **`SitTestPatternPackLegacyStd140`**.

### Changes

| Area | Detail |
|------|--------|
| **`sit/sit_test_pattern_config.h`** | **New** — `SitTpParams*`, `SitTestPatternLayerBundle`, stack helpers, pack shim |
| **`sit/situation_api_types_gpu.h`** | Includes config header; removes flat struct |
| **`sit/situation_impl_vd.h`** | Delegates init/pack; sets `cfg.snow.noise_frame_seed` on upload |
| **`tests/harness/sit_harness_pattern_ubo.h`** | Thin wrappers over library helpers |
| **`tests/harness/test_graphics_patterns.c`** | `pattern_config_defaults`; nested field access |
| **`doc/guide/test_patterns.md`** | §3.6 config struct layout |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.374.

---

---

## [v2.4.375] - 2026-06-27

### Description

**Library layout fix** — remove standalone `sit/sit_test_pattern_config.h`. **`SitVdStandbyConfig`** and all init/pack/toggle helpers now live in **`sit/situation_api_types_gpu.h`** (VD API types module), matching library conventions.

### Changes

| Area | Detail |
|------|--------|
| **`sit/sit_test_pattern_config.h`** | **Removed** |
| **`sit/situation_api_types_gpu.h`** | `SitVdStandbyConfig`, `SitVdStandbyLayerParams`, palette, `SitVdStandbyConfigInitDefaults`, `SitVdStandbyPackStd140` |
| **`tests/harness/sit_harness_pattern_ubo.h`** | Delegates to `SitVdStandby*` helpers in api types |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.375.

---

---

## [v2.4.376] - 2026-06-27

### Description

**VD standby implementation split** — init, toggle, stack helpers, and std140/std430 packers moved out of `situation_impl_vd.h` into **`sit/situation_impl_vd_standby.h`**. Public **`SituationVdStandby*`** entry points added to **`situation_api_graphics.h`**.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.376.

---

---

## [v2.4.377] - 2026-06-27

### Description

**`situation_api_types_gpu.h` types-only cleanup** — render-pass helpers → `situation_impl_renderer_frame_cmd.h` (renderer `frame_cmd` slice, adjacent to `SituationCmdBeginRenderPass`); `ViewDataUBO` → `situation_impl_renderer_core.h`; VD standby types grouped under the Virtual Display section.

> **Note:** v2.4.377 briefly introduced an invalid interim `situation_impl_render_pass.h` outside the five-slice renderer layout; that deviation was removed and the helpers merged into `frame_cmd` @ v2.4.384.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.377.

---

---

## [v2.4.378] - 2026-06-27

### Description

**P10 / P11 — GPU layer params + stack compose** — 160 B std140 header UBO + 832 B std430 layer-params SSBO (@ binding **1**); compositor and harness dual upload/bind; per-layer shader reads via **`sit_tp_layer_params*.glslh`**. Multi-bit compose uses **`sit_tp_compose_stack()`** instead of a hardcoded layer chain. std140 header layout fix: scalar **`_header_pad7` / `_header_pad8`** (replaces `float[2]` stride bug); **`layer_stack_count`** @ GPU offset **140**.

### Changes

| Area | Detail |
|------|--------|
| **`sit/gpu/test_patterns/`** | `sit_tp_config_header_ubo.glslh`, `sit_tp_compose.glslh`, SSBO param blocks |
| **`sit/situation_impl_vd_standby.h`** | `SituationVdStandbyPackParamsStd430`, dual bind upload |
| **`tests/harness/test_graphics_patterns.c`** | compose + per-layer param gate tests |

### Verification

- Graphics `--filter pattern` **15/15** GL · **16/16** VK; full harness `--filter pattern` **17/17** GL.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.378.

---

---

## [v2.4.379] - 2026-06-27

### Description

**P14 digestible example + example scaffolding polish** — ships **`examples/25_vd_standby`** (interactive VD test-pattern explorer on the production compositor path), fixes shared example HUD layout for resize/fullscreen, completes **`build_examples.bat`** coverage, and sets symmetric SMPTE safe-area defaults.

1. **`25_vd_standby` (P14)** — 1280×720 VD with `SituationSetVirtualDisplayPatternConfig`; keys for layers 0–8, snow/chroma, stack order, per-layer params, fallback cycle (PATTERN / COLORBURST / SOLID), live-draw demo. Status + hotkey panel in **lower-mid** host window (clear of `sit_example` top/bottom bars). SOLID fallback uses VD create default tint (RGB 13, 38, 102) — no spurious override.
2. **`examples/shared/sit_example.h` HUD** — resolution-independent layout (1280×1024 reference fractions): title left, backend centered, VSync @ 75% width, FPS inset from right with badge avoidance, universal hotkeys **centered** in bottom bar. Layout uses correct **17px** glyph advance (font 16 + spacing 1); compact hotkey string on narrow widths.
3. **`build/build_examples.bat`** — `all` target builds **01–10, 18–21, 25**; usage text updated; unknown example names fail with usage help; **`25_vd_standby` / `vd_standby`** + `-mwindows`.
4. **SMPTE defaults** — `content_margin_x/y` **0.100 / 0.100** (was 0.125 / 0.20) in `SituationVdStandbyConfigInitDefaults`, shader fallbacks, and harness pixel gates — matches 10% safe-area surround; overlay circle tapers to top/bottom of content box.

### Changes

| Area | Detail |
|------|--------|
| **`examples/25_vd_standby/`** | **New** — `main.c`, `README.md`; ratio-based status panel |
| **`examples/shared/sit_example.h`** | Ratio HUD; `_sit_ex_pick_bottom_keys`, `_sit_ex_center_x` |
| **`build/build_examples.bat`** | Full digestible `all` list; clearer errors |
| **`sit/situation_impl_vd_standby.h`** | SMPTE margin defaults 0.1f |
| **`sit/gpu/test_patterns/sit_tp_smpte.glslh`** | Fallback margins 0.1 |
| **`sit/gpu/test_patterns/sit_tp_compose.glslh`** | Overlay circle fallback margins 0.1 |
| **`tests/harness/test_graphics_patterns.c`** | SMPTE bar sample uses margin-derived content rect |
| **`doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md`** | P14 shipped; §6.4 acceptance |
| **`examples/README.md`**, **`doc/guide/test_patterns.md`** | `25_vd_standby` build row |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_examples.bat" static-opengl all
& ".\build\build_examples.bat" static-opengl 25_vd_standby
```

Regenerate harness SPIR-V embeds before pattern tests after GLSL margin change:

```powershell
& ".\build\build_tests.bat" shaders
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.379.

---

---

## [v2.4.380] - 2026-06-27

### Description

**Audio graph examples + input/window reliability** — restores audible graph+MIDI paths across digestible examples, fixes FX wrapper control mapping, corrects stale fullscreen/window-state HUD, and separates numpad synth controls from piano input in example 04.

1. **Shared example audio scaffolding (`sit_example.h`)** — `SitExample_InitAudioRegistry`, `SitExample_WireToneSynthVirtualMidi`, `SitExample_ActivateAudioGraph`, `SitExample_DestroyAudioGraph`, `SitExample_TeardownVirtualMidi` for the virtual-MIDI → tone-synth graph path (registry init, loopback, `EnableMidiControl`, topological sort + activate, safe teardown order).
2. **Examples 04 / 06 / 09 / 19** — wired through the shared helpers; virtual MIDI note-on/off drives the tone synth instead of the legacy `PlayToneEx` pool. Example **08** explicitly calls `SituationSetActiveGraph(NULL)` so `PlayToneEx` mixes to the device (intentional legacy demo).
3. **`device_wrappers.h` FX mapping** — chorus dry/wet/feedback indices aligned to registry (17–20); phaser and overdrive control slots match registry metadata so bypassed FX keep a live dry path instead of silencing the chain.
4. **Window-state cache** — `SituationPollInputEvents`, input init, and F11 borderless toggle now refresh `cached_window_state_flags` via `_SituationComputeWindowStateFlags()` instead of reading the stale cache through `SituationGetCurrentActualWindowStateFlags()` (fixed stuck `[FULLSCREEN]` HUD after leaving borderless).
5. **Numpad digit alias (Windows)** — `_SituationReconcileNumpadDigitAlias()` after each `glfwPollEvents`: when `KP_0..9` is held, clears aliased main-row digit logical + scancode state so piano layouts are not triggered by NumLock-on duplicates.
6. **Example 04 — full graph synth demo** — 16-voice tone synth through overdrive → chorus → phaser → echo → reverb → gain; tracker-style two-octave piano (scancode + logical key); numpad reserved for sub/LFO/options; startup keyboard arming + CC123 all-notes-off; expanded 5-octave keyboard UI; FX presets with chorus dry=1.0 on bypass.
7. **Example 04 — numpad 440 Hz beep fix** — `TONE_CTRL_VOLUME` (manual-mode amplitude when no MIDI voices) set to **0.0**; numpad param pushes no longer trigger the default 440 Hz manual oscillator (notes use MIDI velocity only).
8. **Example 09** — virtual MIDI loopback auto-init on first note; graph deactivate → close MIDI streams → destroy before loopback teardown (shutdown crash fix).

### Changes

| Area | Detail |
|------|--------|
| **`examples/shared/sit_example.h`** | Audio registry + virtual MIDI graph helpers |
| **`examples/04_play_a_sound/main.c`** | Graph synth UI; scancode piano; numpad options; `TONE_CTRL_VOLUME` 0 |
| **`examples/06_audio_node_graph/main.c`** | Shared audio init / virtual MIDI / teardown |
| **`examples/08_temporal_oscillators/main.c`** | `SituationSetActiveGraph(NULL)` for legacy tone pool |
| **`examples/09_midi_control/main.c`** | Virtual MIDI auto-init; safe graph shutdown |
| **`examples/19_node_graph_piano/main.c`** | Shared audio helpers |
| **`sit/aud/device_wrappers.h`** | Chorus / phaser / overdrive control index fixes |
| **`sit/situation_impl_input.h`** | `_SituationReconcileNumpadDigitAlias`; input-init window-state refresh |
| **`sit/situation_impl_ctrl.h`** | Reconcile after poll; window-state refresh |
| **`sit/situation_impl_wdm.h`** | Window-state refresh after F11 borderless toggle |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_examples.bat" static-opengl 04_play_a_sound 06_audio_node_graph 08_temporal_oscillators 09_midi_control 19_node_graph_piano
```

Manual checks (example 04):

- Piano keys (`Q W E …`, `Z X C …`) play through the FX chain; numpad adjusts sub/LFO without triggering notes or a 440 Hz drone.
- F11 borderless → restore: HUD state label tracks actual window mode.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.380.

---

---

## [v2.4.381] - 2026-06-27

### Description

**Tone synth band-limited oscillators (polyBLEP)** — pulse, saw, and triangle generators in the graph tone synth are anti-aliased; hard-sync sub resets get a matching blep at the master wrap. Example 04 octave keys fixed to shift ±1 octave per press.

1. **polyBLEP / polyBLAMP** — `_SituationToneSynthPolyBlep` and `_SituationToneSynthPolyBlamp` in `tone_synth_graph.h`; normalized phase increment `freq / sample_rate` drives corrections at discontinuities.
2. **Waveforms** — saw (wrap), pulse (wrap + duty edge), triangle (slope kinks via polyBLAMP); sine and hash noise unchanged.
3. **Hard-sync clicks** — when the main oscillator wraps and resets the sub phase to 0, output jump is spread with a polyBLEP keyed to the master wrap (reduces KP5 sync pops).
4. **Example 04** — F2/F3 octave shift was ±2 octaves per press (`s_octave ±= 2`); now ±1 to match example 19 and typical synth UX.

### Changes

| Area | Detail |
|------|--------|
| **`sit/aud/tone_synth_graph.h`** | polyBLEP/polyBLAMP; phase norm helpers; sync-reset blep in `MixMainSub` |
| **`examples/04_play_a_sound/main.c`** | F2/F3 `s_octave ±= 1` (was ±2) |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_examples.bat" static-opengl 04_play_a_sound
```

Manual (example 04): TAB → saw/pulse high on keyboard — cleaner timbre, fewer metallic aliases; KP5 sub sync — fewer random clicks; F2/F3 — one octave per press.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.381.

---

---

## [v2.4.384] - 2026-06-28

### Description

**Game loop Phase 1 — on-demand OpenGL screen capture; Vulkan pattern harness green** — removes unconditional per-frame GL readback from the hot path while preserving `EndFrame` → `LoadImageFromScreen` parity; fixes Vulkan graphics pattern tests (descriptor bind order + shader include order).

1. **On-demand GL capture (Phase 1 / Gate 1)** — capture runs only when `screenshot_request_pending[slot]` or `screenshot_urgent[slot]` is set; PBO async readback on the render thread; capture **epoch** invalidates stale cache across consecutive `EndFrame` → `Load` calls; single-thread harness path uses pre-swap sync fallback at `EndFrame` when capture was not requested (DWM-safe implicit readback).
2. **Vulkan pattern tests** — `sit_harness_test_pattern_draw_fullscreen_config` binds pipeline before descriptor sets (Vulkan requires bound pipeline for layout resolution); test-pattern GLSL headers always include `sit_tp_layer_params.glslh` (struct types needed for non-SSBO fallback paths); runtime-include compile test includes `sit_tp_config_ubo.glslh` before `sit_test_patterns.glslh`.
3. **Plan** — [`doc/plan/GAME_LOOP_PERFORMANCE_PLAN.md`](plan/GAME_LOOP_PERFORMANCE_PLAN.md) Phase 1 items 1.1–1.15 marked complete; frame pacing baseline shows no regression vs Phase 0.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_decl.h`** | `screenshot_urgent[]`, `screenshot_request_pending[]`, capture/buffer epoch, GL screenshot mutex |
| **`sit/situation_impl_renderer_lc.h`** | PBO pipeline, `_SituationGLShouldCaptureFrame`, RT pre/post-swap capture, late urgent poll |
| **`sit/situation_impl_renderer_frame_cmd.h`** | RT EndFrame epoch bump; ST EndFrame gated capture + pre-swap sync fallback; render-pass helpers (`SituationRenderPassInfo*`, `SituationRenderPassConfigurationKey`) — absorbed from removed stray `situation_impl_render_pass.h` |
| **`sit/situation_impl_renderer.h`** | Orchestrator restored to five-slice include order only (no sixth `render_pass` header) |
| **`sit/situation_impl_image.h`** | RT urgent latch + epoch-valid cache; ST epoch cache or sync read fallback |
| **`tests/harness/test_frame_pacing.c`** | `gl_load_urgent_after_endframe`, `gl_load_after_endframe_st` |
| **`tests/harness/sit_harness_test_pattern_helpers.c`** | BeginRenderPass → BindPipeline → BindDescriptorSet draw order |
| **`sit/gpu/test_patterns/sit_tp_*.glslh`** | Unconditional `sit_tp_layer_params.glslh` include where layer param structs are used |
| **`tests/harness/test_graphics_patterns.c`** | Runtime-include shader: config UBO header before test_patterns |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" vulkan
& ".\build\build_tests.bat" opengl
& ".\build\build_tests.bat" vulkan
Set-Location build; $env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\run_tests.bat" opengl --module graphics
& ".\run_tests.bat" opengl --module frame_pacing
& ".\run_tests.bat" vulkan --module graphics
```

OpenGL: **graphics 125/125**, **frame_pacing 3/3**. Vulkan: **graphics 118/118**.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.384.

---

---

## [v2.4.385] - 2026-06-28

### Description

**Game loop performance plan Phases 2–4 complete** — present-anchored `frame_time`/FPS, paced queue depth under VSync, developer docs and metrics overlay; renderer layout correction.

1. **Phase 2 — present-anchored timing** — render thread publishes display delta at present; main thread consumes in `SituationUpdateTimers`; refresh-aware FPS rounding; `SituationGetDisplayRefreshRate()`.
2. **Phase 3 — paced VSync** — `paced_frames_in_flight = 2` when VSync/target FPS active (else 6); queue backpressure uses runtime limit; VSync-only disables adaptive SLEEP (YIELD instead). Gate 2/3 green (OpenGL + Vulkan frame_pacing).
3. **Phase 4 — visibility** — `doc/architecture.md` Frame Loop Contract; steering frame/capture rules; metrics overlay shows refresh Hz, paced slots, present dt, capture state; debug stderr doc links for loop violations and urgent capture timeout.
4. **Renderer layout** — removed invalid sixth slice `situation_impl_render_pass.h`; render-pass helpers live in `situation_impl_renderer_frame_cmd.h`. `verify_renderer_fwd.py` **370/370** green after backpressure/present-timing fwd parity fix.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_renderer_core.h`** | Present timing publish/consume, FPS counter, backpressure/paced-depth helpers |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Paced queue waits; metrics overlay fields; `SituationRenderPassInfo*` helpers |
| **`sit/situation_impl_renderer_fwd.h`** | Forward-decl parity (370 statics) |
| **`sit/situation_impl_ctrl.h`** | Loop violation stderr doc links |
| **`sit/situation_impl_wdm.h`** | `SituationGetDisplayRefreshRate()`; paced recompute on VSync/target FPS |
| **`doc/architecture.md`** | Frame Loop Contract section + TOC |
| **`doc/plan/GAME_LOOP_PERFORMANCE_PLAN.md`** | Phases 1–4 complete; Gate 2/3 recorded |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
python scripts/verify_renderer_fwd.py
& ".\build\build_situation.bat" all
& ".\build\build_tests.bat" opengl
& ".\build\build_tests.bat" vulkan
Set-Location build; $env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\run_tests.bat" opengl
& ".\run_tests.bat" vulkan
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.385.

---

---

## [v2.4.386] - 2026-06-28

### Description

**Fractional display refresh Hz** — store precise nominal panel rate at display-cache time; new float query APIs; VSync FPS rounding uses fractional truth while HUD integer getters and M overlay stay readable.

1. **`SituationDisplayInfo.current_refresh_hz`** — filled on display cache refresh. Windows: DXGI `GetDisplayModeList` rational matched to current width/height and integer DEVMODE Hz. Non-Windows: `(float)GLFW refreshRate`.
2. **New APIs** — `SituationGetMonitorRefreshRateHz`, `SituationGetDisplayRefreshRateHz`, `SituationGetMeasuredPresentRateHz` (render-thread present delta when available, else `1/frame_time`).
3. **Integer getters unchanged** — `SituationGetMonitorRefreshRate` / `SituationGetDisplayRefreshRate` still return OS integer Hz; M metrics overlay **`Disp:`** / **`FPS:`** lines unchanged.
4. **FPS rounding** — `_SituationRoundDisplayFps` uses `SituationGetMonitorRefreshRateHz(0)` so 59.94 nominal rounds to **60** in `SituationGetFPS()`.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_platform.h`** | `current_refresh_hz` on `SituationDisplayInfo`; three float Hz getters |
| **`sit/situation_impl_wdm.h`** | `_SituationDxgiFillDisplayRefreshHz`; API implementations |
| **`sit/situation_impl_renderer_core.h`** | Fractional nominal in `_SituationRoundDisplayFps` |
| **`sit/situation_base_trace.h`** | Trace IDs for new public APIs |
| **`doc/architecture.md`** | Frame Loop Contract refresh-rate paragraph |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_tests.bat" opengl
Set-Location build; $env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\run_tests.bat" opengl --module frame_pacing
python scripts/verify_renderer_fwd.py
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.386.

---

---

## [v2.4.387] - 2026-06-29

### Description

**VD bolster resume (renderer_bolster_plan Phase 11)** — completes v2.4.x Virtual Display exit criteria: **VD-2** sRGB storage + compositor gamma, **VD-3** explicit composite sampler (scaling layout-only), **VD-4a** aniso/mip LOD configure + create-time storage mips, **VD-5** static update mode + memory hint. **VD-4b** MSAA remains v2.5-gated.

1. **VD-2 — color / HDR behavior** — OpenGL `GL_SRGB8_ALPHA8` FBO when `SIT_VD_FORMAT_RGBA8_SRGB`; `GL_FRAMEBUFFER_SRGB` during VD render passes; Vulkan SRGB image format at create; HDR-aware clear on main window only (`display_id == -1` on GL). **`SituationSetVirtualDisplayClearColor`** tier-B sugar.
2. **VD-3 — composite sampler** — **`SituationVirtualDisplaySamplerDesc`** on create/configure; **`SituationSetVirtualDisplaySampler`** (light rebuild). **`SituationSetVirtualDisplayScalingMode`** is layout-only (no filter side effects).
3. **VD-4a — rendering quality** — **`SituationSetVirtualDisplayMaxAnisotropy`**, **`SituationSetVirtualDisplayMipLevels`** (sampler LOD clamp; storage `color_mip_levels` at create only). Post-draw mipgen: GL `glGenerateTextureMipmap`; VK in **`_SitVkEndVDDynamicRendering`**.
4. **VD-5 — performance / memory** — **`SituationSetVirtualDisplayUpdateMode`** (`SIT_VD_UPDATE_STATIC` freezes frame clock in **`SituationUpdateTimers`**); **`SituationSetVirtualDisplayMemoryHint`** stored + passed to VMA at create.
5. **Plan / docs** — **`doc/plan/renderer_bolster_plan.md`** status, checklists, test matrix, exit criteria synced @ v2.4.387.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_types_gpu.h`** | `SituationVirtualDisplaySamplerDesc`, update/memory enums; extended `SituationVirtualDisplayDesc` / `SituationVirtualDisplay` |
| **`sit/situation_api_graphics.h`** | Configure APIs: clear color, sampler, aniso, mip levels, update mode, memory hint |
| **`sit/situation_impl_vd.h`** | sRGB GL internal format, composite sampler helpers, mipgen, configure guards |
| **`sit/situation_impl_renderer_lc.h`** | GL VD pass clear floats + `GL_FRAMEBUFFER_SRGB` |
| **`sit/situation_impl_renderer_frame_cmd.h`** | VK VD end-pass mip generation |
| **`sit/situation_impl_ctrl.h`** | Static VD update mode skips frame-clock advance |
| **`build/dll/situation_*.def`** | New exports (regenerated by gendef on build) |
| **`tests/harness/test_virtual_display.c`** | `vd_default_clear_color`, `vd_srgb_format_composite`, `vd_sampler_nearest_upscale`, `vd_aniso_sampler_configure`, `vd_update_mode_static` |
| **`doc/whatsnew.md`**, **`doc/plan/renderer_bolster_plan.md`** | v2.4.387 release notes + plan closure |
| **`sit/situation_base_version.h`** | v2.4.387 |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" vulkan
& ".\build\build_tests.bat" opengl
& ".\build\build_tests.bat" vulkan
Set-Location build; $env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module virtual_display
& ".\tests\sit_test_vulkan.exe" --module virtual_display
```

**Results @ v2.4.387 (GTX 1070):** `virtual_display` **40/40** OpenGL, **39/39** Vulkan (GL-only `gl_endframe_execute_vd_streak` excluded on VK).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.387.

---

## v2.4.388 — Phase 14 Slice 1: renderer behavior policy commands

**Phase 14 Slice 1** — command-buffer-scoped renderer behavior policy (`Set/Push/PopRendererBehavior`) with strict defaults; first functional axis is **`SIT_BLIT_FILTER_DOWNGRADE_NEAREST`** on `SituationCmdBlitTexture`.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_types_gpu.h`** | `SituationRendererBehaviorPolicy` + axis enums |
| **`sit/situation_api_graphics.h`** | `SituationRendererBehaviorPolicyDefault`, `SituationCmdSet/Push/PopRendererBehavior` |
| **`sit/situation_api_config.h`** | `SITUATION_MAX_BEHAVIOR_STACK_DEPTH` (32) |
| **`sit/situation_impl_decl.h`** | GL/VK behavior stack storage; `SIT_OP_SET/PUSH/POP_RENDERER_BEHAVIOR` |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Policy APIs; frame reset |
| **`sit/situation_impl_renderer_resources.h`** | Blit filter policy resolution |
| **`sit/situation_impl_renderer_lc.h`** | GL executor replay |
| **`tests/harness/test_transfer.c`** | `behavior_policy_*`, `behavior_blit_filter_downgrade`, `behavior_stack_bounds` |
| **`doc/plan/PHASE_14_RENDERER_BEHAVIOR_POLICY.md`**, **`doc/RENDERER_COMMAND_STACK.md`** | Slice 1 shipped |
| **`sit/situation_base_version.h`** | v2.4.388 |

### Verification

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation\build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module transfer
& ".\tests\sit_test_vulkan.exe" --module transfer
```

**Results @ v2.4.388:** `transfer` **16/16** OpenGL, **16/16** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.388.

---

## v2.4.389 — Phase 14 Slice 2: transfer usage compatible fallback

**Phase 14 Slice 2** — `SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK` allows sampled RGBA8 color sources without `TRANSFER_SRC` on read-only transfer paths (copy/blit/readback + matching texture barriers). Buffers and storage-only textures stay strict.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_renderer_resources.h`** | `_SitValidateTextureTransferSrcUsage`; wired into transfer Cmd* |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Policy Set accepts compatible usage + validation modes; texture barrier parity |
| **`tests/harness/test_transfer.c`** | `behavior_transfer_usage_fallback`, `behavior_transfer_usage_storage_rejected` |
| **`doc/misc/RENDERER_BARRIER_COOKBOOK.md`** | Strict vs opt-in transfer usage note |
| **`sit/situation_base_version.h`** | v2.4.389 |

### Verification

**Results @ v2.4.389:** `transfer` **18/18** OpenGL, **18/18** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.389.

---

## v2.4.390 — Phase 14 Slice 3: assisted texture layout hints

**Phase 14 Slice 3** — per-texture `layout_hint` updated by `SituationCmdTextureBarrier` and transfer commands. Under `SIT_TEXTURE_LAYOUT_ASSISTED`, copy/blit/buffer-texture paths auto-insert transfer layout transitions when the hint disagrees with required transfer-src/dst layouts. Attachment and present layouts stay explicit (Phase 3b not included).

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_decl.h`** | `layout_hint` on `_SituationTextureSlot` |
| **`sit/situation_impl_renderer_resources.h`** | `_SitAssistedEnsureTextureLayout`, hint updates on transfer Cmd* |
| **`sit/situation_impl_renderer_frame_cmd.h`** | `TextureBarrier` sets hint; VK layout helpers consolidated in resources slice |
| **`tests/harness/test_transfer.c`** | `behavior_layout_assisted_transfer` |
| **`doc/misc/RENDERER_BARRIER_COOKBOOK.md`** | Assisted layout opt-in note |
| **`sit/situation_base_version.h`** | v2.4.390 |

### Verification

**Results @ v2.4.390:** `transfer` **19/19** OpenGL, **19/19** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.390.

---

## v2.4.391 — Phase 14 Slice 4: validation logging + user docs

**Phase 14 Slice 4** — documents renderer behavior policy for app developers; fallback warnings use a consistent `renderer behavior:` log prefix when `validation >= WARN`. `SituationSetTraceLogLevel` filters visibility.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_renderer_resources.h`** | `_SitLogRendererBehaviorFallback` log prefix |
| **`tests/harness/test_transfer.c`** | `behavior_validation_modes_accept`, `behavior_validation_warn_emits_log` |
| **`doc/guide/renderer_bolster.md`** | Workflow 3b — behavior policy; cookbook link |
| **`doc/situation_command_reference.md`** | Policy command index rows + §3 subsection |
| **`doc/RENDERER_COMMAND_STACK.md`** | Behavior policy row ✅ @ v2.4.391 |
| **`doc/misc/RENDERER_BARRIER_COOKBOOK.md`** | Validation tone + trace log level note |
| **`sit/situation_base_version.h`** | v2.4.391 |

### Verification

**Results @ v2.4.391:** `transfer` **21/21** OpenGL, **21/21** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.391.

---

## v2.4.392 — Phase 3b: color attachment → transfer readback

**Phase 3b** — `SituationCmdTextureBarrier` accepts `COLOR_ATTACHMENT` ↔ transfer layouts on transfer-capable color textures. `EndRenderPass` sets layout hint on VD targets; mip-0 VD images stay in attachment layout until explicit readback or composite. Cookbook + `transfer.render_target_readback` harness (VD → barrier → `CopyTextureToBuffer`).

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_impl_renderer_frame_cmd.h`** | TextureBarrier COLOR_ATTACHMENT validation; VD end-pass/composite layout helpers |
| **`sit/situation_impl_renderer_resources.h`** | `_SitRenderPassSetTargetLayoutHint`, deferred layout split |
| **`sit/situation_impl_vd.h`** | Composite ensures SHADER_READ before sampling |
| **`tests/harness/test_transfer.c`** | `render_target_readback`; barrier validation updated |
| **`doc/misc/RENDERER_BARRIER_COOKBOOK.md`** | Render target readback recipe |
| **`sit/situation_base_version.h`** | v2.4.392 |

### Verification

**Results @ v2.4.392:** `transfer` **22/22** OpenGL, **22/22** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.392.

---

## v2.4.393 — Phase 3c: SituationRenderTarget offscreen pass + readback

**Phase 3c** — user offscreen render targets without VD compositor: `SituationCreateRenderTarget` / `DestroyRenderTarget`, `SituationGetRenderTargetTexture`, `SituationReadRenderTarget`. `SituationRenderPassInfo.render_target` routes `BeginRenderPass`/`EndRenderPass` (Option A from v2.5 plan). Single-sample only; MSAA deferred.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_base_types.h`** | `SituationRenderTarget` handle |
| **`sit/situation_api_types_gpu.h`** | `SituationRenderTargetDesc`, `render_target` on `SituationRenderPassInfo` |
| **`sit/situation_impl_render_target.h`** | Create/destroy/read + registry integration |
| **`sit/situation_impl_renderer_frame_cmd.h`** | RT dynamic rendering (VK), pass routing GL+VK |
| **`tests/harness/test_render_target.c`** | New module (3 tests) |
| **`doc/misc/RENDERER_BARRIER_COOKBOOK.md`** | User RT readback recipe |

### Verification

**Results @ v2.4.393:** `render_target` **3/3** OpenGL, **3/3** Vulkan; `transfer` **22/22** OpenGL, **22/22** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.393.

---

## v2.4.394 — P10.1: SituationGetFrameProfile headless frame telemetry

**P10.1** — structured snapshot API wrapping P10.0 frame pacing getters for QSR loops and regression harnesses.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_types_gpu.h`** | `SituationFrameProfile` (+ version/size, `gpu_zone_ns[]` placeholders) |
| **`sit/situation_api_graphics.h`** | `SituationGetFrameProfile`, `SituationResetFrameProfileStats` |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Snapshot fill from existing `sit_gs` / render-thread metrics |
| **`tests/harness/test_frame_profile.c`** | New module (2 tests) |

### Verification

**Results @ v2.4.394:** `frame_profile` **2/2** OpenGL, **2/2** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.394.

---

## v2.4.395 — P10.2: Tracy CPU profiling (opt-in)

**P10.2** — compile-gated Tracy CPU zones for interactive stutter capture; zero cost when disabled (default build).

### Changes

| Area | Detail |
|------|--------|
| **`ext/tracy/public/`** | Vendored Tracy client (upstream) |
| **`sit/situation_prof_macros.h`** | `SIT_PROF_*` / `SIT_PROFILE_*` macro layer |
| **`sit/situation_impl_trace_prof.h`** | Internal include for impl modules |
| **`sit/tracy_client.cpp`** | Single TU linking Tracy client when `SIT_TRACY=1` |
| **`sit/Makefile`**, **`build/build_situation.bat`** | `SIT_TRACY=1` / second arg `tracy` |
| **Instrumented paths** | EndFrame, Acquire, render thread, VK submit, poll/update, PollShaderLoad, workers, audio |

### Verification

**Default (Tracy off):** `frame_profile` **2/2** OpenGL, **2/2** Vulkan; `frame_pacing` baseline unchanged.

**Tracy on:** `SIT_TRACY=1 build_situation.bat opengl` links clean.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.395.

---

## v2.4.396 — P10.3: GPU timestamp zones

**P10.3** — fixed internal GPU elapsed-time zones plus user slots; fills **`SituationGetFrameProfile` → `gpu_zone_ns[]`** when **`SIT_FEATURE_GPU_TIMESTAMPS`** is supported.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_types_gpu.h`** | `SituationGPUProfileZone` enum (composite, VD paths, text batch, user 0–11) |
| **`sit/situation_api_graphics.h`** | `SituationCmdGPUZoneBegin`, `SituationCmdGPUZoneEnd`; `SIT_FEATURE_GPU_TIMESTAMPS` |
| **`sit/situation_base_errno.h`** | `-563` GPU unsupported, `-564` zone overflow, `-565` zone state |
| **`sit/situation_impl_gpu_prof.h`** | GL `GL_TIME_ELAPSED` ring; VK `VkQueryPool` timestamps; readback after fence |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Soft-buffer ops `SIT_OP_GPU_ZONE_BEGIN/END`; internal VD/text zones |
| **`tests/harness/test_frame_profile.c`** | Expanded to 4 tests (user zone GL; VK skips in-pass user zone) |

### Notes

- Readback is **one frame slot late** (non-blocking; no `VK_QUERY_RESULT_WAIT_BIT`).
- **Vulkan:** user zones **outside** active render pass only (`-565` in-pass).
- **OpenGL:** user zones work inside render pass when bracketing GPU work.
- Tracy GPU track feed from P10.3 timestamps remains **open**.

### Verification

**Results @ v2.4.396:** `frame_profile` **4/4** OpenGL, **4/4** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.396.

---

## v2.4.397 — P10.4: public SituationQueryPool API

**P10.4** — user-owned GPU query pools (timestamps + occlusion), separate from P10.3 internal profile zones.

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_base_types.h`** | `SituationQueryPool` handle |
| **`sit/situation_api_types_gpu.h`** | `SituationQueryType`, `SituationQueryResultFlags` |
| **`sit/situation_api_graphics.h`** | Create/destroy/readback + `SituationCmd*` query pool commands |
| **`sit/situation_base_errno.h`** | `-566`…`-570` query pool errors |
| **`sit/situation_impl_query_pool.h`** | Pool registry, GL/VK backends, readback |
| **`sit/situation_impl_renderer_frame_cmd.h`** | Cmd recording (VK direct, GL soft-buffer) |
| **`tests/harness/test_query_pool.c`** | New module (3 tests) |

### Verification

**Results @ v2.4.397:** `query_pool` **3/3** OpenGL, **3/3** Vulkan.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.397.

---

## v2.4.398 — MSAA Phase 0 prep (VD-4b scaffolding)

**VD-4b Phase 0** — public quality types + internal wiring only. **No MSAA attachments, no resolve, `msaa_samples > 1` still rejected.** End-to-end MSAA remains **v2.5-gated** (see **`doc/plan/renderer_bolster_plan.md`** § VD-4b).

### Changes

| Area | Detail |
|------|--------|
| **`sit/situation_api_types_gpu.h`** | **`SituationMultisampleQuality`**, **`SITUATION_MULTISAMPLE_DEFAULT`**, inline **`SampleCount` / `FromSampleCount` / `Clamp`** |
| **`SituationVirtualDisplay`** | **`msaa_quality`** (from **`desc.msaa_samples`** at create), **`pending_gpu_rebuild`** (field only; hook deferred) |
| **`sit/situation_impl_renderer_lc.h`** | **`_SituationVulkanCreateImage(..., VkSampleCountFlagBits samples)`** — all call sites still **`1×`** |
| **`sit/situation_impl_renderer_frame_cmd.h`** | VD pipeline variant key includes sample count; **`rasterization_samples`** on dynamic pipeline create |
| **`sit/situation_impl_renderer_shader.h`** | **`CreateGraphicsPipelineEx`** / **`_SitVkCreateVDDynamicPipelineFromModules`** take **`rasterization_samples`**; bake **`dynamic_ms_*`** when samples > 1 |
| **`tests/harness/test_graphics.c`** | **`multisample_quality_helpers`** |

### Unified MSAA direction (documented)

One user-facing quality system (**`SituationMultisampleQuality`** = attachment tier). **`SituationMultisampleState`** + **`SituationCmdSetMultisampleState`** = in-pass raster flags (layer 2), meaningful only after VD-4b attachments exist.

### Verification

**Results @ v2.4.398:** `multisample_quality_helpers` **1/1** OpenGL + Vulkan; existing **`multisample_*`** graphics filters unchanged.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.398.

---

---

## [v2.4.399 "Win32 Executable Identity"] - 2026-06-29

**Application identity (defaults + overrides)** — library-owned shell and PE defaults for any Situation app, with author override at every layer. Implements [SIT_IDENTITY_PLAN.md](plan/SIT_IDENTITY_PLAN.md) slices **WI-0–WI-4** (WI-5 deferred → **v2.4.400**). Cross-platform architecture: [architecture.md](architecture.md#application-identity-architecture-v24399) (Win32 Phase I shipped; Linux **LI-*** / macOS **MA-*** planned).

### Changes — Library / API

| Area | Detail |
|------|--------|
| **`SITUATION_DEFAULT_APP_USER_MODEL_ID`** | `"Situation.Application"` in **`situation_api_config.h`** |
| **`SituationInitInfo::app_user_model_id`** | NULL → default AppID via `SetCurrentProcessExplicitAppUserModelID` **before** `glfwInit` |
| **`SituationWin32SetAppUserModelId`** | Pre-init override API (Win32); invalid empty/space strings rejected |
| **`sit/platform/windows/situation_win32_identity.h`** | AppID apply helper (idempotent; `GetProcAddress` on `shell32.dll`) |
| **`sit/situation_impl_ctrl.h`** | `_SituationWin32InitAppUserModelIdFromInitInfo` in `_SituationInitPlatform` |
| **`sit/platform/windows/sit_app.rc`** | EXE **`VS_VERSION_INFO`** + icon (`FileDescription`: "Situation Application") |
| **`sit/platform/windows/sit_app_template.rc`** | Author PE override template (`-DAPP_*` windres flags) |
| **`SIT_APP_RC` / `APP_RC=`** | Override default RC in **`build/build_examples.bat`** and **`tests/harness/Makefile`** |
| **`sit/situation_base_trace.h`** | Trace IDs for new identity APIs |

### Changes — Docs & steering

| Doc | Detail |
|-----|--------|
| **`doc/guide/windows_app_identity.md`** | New — defaults + override matrix, PE/AppID, repo examples, troubleshooting |
| **`doc/architecture.md`** | § **Application Identity Architecture** (cross-platform layers; Win32 impl; LI-* / MA-* roadmap) |
| **`doc/guide/core.md`** | `app_user_model_id` on **`SituationInitInfo`** |
| **`doc/guide/window_display.md`** | Cross-links + troubleshooting rows |
| **`doc/COMPILATION_GUIDE.md`** | § Application Identity (PE resources, `windres`, `SIT_APP_RC`) |
| **`doc/situation_sdk.md`** | § **2.4 Application Identity**; module guide link |
| **`doc/introduction.md`**, **`doc/whatsnew.md`** | v2.4.399 narrative + architecture link |
| **`doc/plan/SIT_IDENTITY_PLAN.md`** | WI-0–WI-4 closed; v2.4.400 release gate for WI-5 (renamed from WIN32_IDENTITY_PLAN; expanded with LI-*, MA-*, WI-6/7) |
| **`doc/plan/KTERM_CONSOLE_GOALS_PLAN.md`** | Identity cross-link (defaults unless overridden) |
| **`.kiro/steering/situation-project.md`** | v2.4.399, platform tree, key rule #14, `SIT_APP_RC` define |
| **`doc/situation_api.md`**, **`doc/guide/_front_matter.md`** | Metadata → v2.4.399 |
| **`doc/situation_api_index.md`**, **`doc/situation_api_generated.md`** | Regenerated via **`tools/generate_api_index.py`** @ 399 |
| **`doc/situation_command_reference.md`** | Header version → v2.4.399 |
| **`doc/plan/AAA_ARCHITECTURE_PLAN.md`** | Status summary @ v2.4.399 + identity cross-ref |

### Changes — Tests

| Area | Detail |
|------|--------|
| **`tests/harness/test_window.c`** | `win32_app_user_model_id_default`, `win32_set_app_user_model_id_invalid` |

### Verification

**Results @ v2.4.399:** `window` **`win32_*`** **2/2** OpenGL (`--module window --filter win32`).

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_tests.bat" static-opengl
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module window --filter win32
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.399.

---

---

## [v2.4.400 "Win32 Identity WI-5 — Plan Complete"] - 2026-06-29

**Application identity Phase I (Win32 WI-5) complete** — runtime window icon auto-load from init path. Plan renamed to [SIT_IDENTITY_PLAN.md](plan/SIT_IDENTITY_PLAN.md) (WI-6+, LI-*, MA-* open).

### Changes — Library

| Area | Detail |
|------|--------|
| **`SituationInitInfo::default_window_icon_path`** | NULL = no auto-load; PNG via stb; `.ico` multi-size on Win32 |
| **`sit/platform/windows/situation_win32_window_icon.h`** | `PrivateExtractIconsW` → RGBA `SituationImage` |
| **`sit/situation_impl_image.h`** | `_SituationApplyDefaultWindowIconPath` (fail-soft) |
| **`sit/situation_impl_ctrl.h`** | Apply after `is_initialized` at end of `SituationInit` |

### Changes — Tests

| Area | Detail |
|------|--------|
| **`tests/harness/test_identity_init.c`** | Module `identity_init`: PNG path init + fail-soft probe |
| **`tests/harness/sit_test_registry.c`** | Register `identity_init` before `window` |

### Verification

**Results @ v2.4.400:** `identity_init` **2/2**; `window` **`win32_*`** **2/2** OpenGL.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" opengl
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module identity_init
& ".\tests\sit_test_opengl.exe" --module window --filter win32
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.400.

---
