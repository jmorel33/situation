# AAA Architecture Plan

Tracks the long-horizon GPU architecture and ecosystem goals for the Situation SDK.
Covers bindless rendering, GPU-driven indirect draw, render graphs, asset pipeline, profiling,
async I/O, and platform reach.

Originally `doc/ROADMAP.md`. Renamed and audited against v2.4.262 codebase.

---

## 1. Bindless / Universal Handles ✅ COMPLETE

Every resource addressable by a `uint64_t` handle via generational index registries.

- [x] `SIT_FEATURE_BINDLESS_TEXTURES` and `SIT_FEATURE_BINDLESS_BUFFERS` feature flags defined
- [x] `SituationGetBufferDeviceAddress` — VK: `vkGetBufferDeviceAddress`; GL: NV extension; error set on unsupported hardware
- [x] `SituationGetTextureHandle` — VK: descriptor index into global bindless set; GL: `GL_ARB_bindless_texture` resident handle; stale-handle guard returns 0
- [x] `SituationGetMeshVertexBufferAddress` — BDA for vertex-pull shaders
- [x] `SituationGetMeshIndexBufferAddress` — BDA for GPU-driven index fetch
- [x] `SituationShader`, `SituationBuffer`, `SituationMesh` all use generational slot registries
- [x] `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` flag; `SITUATION_BUFFER_USAGE_STORAGE_COMPUTE` preset includes it
- [x] Old linked-list tracking removed

---

## 2. SSBO-First / Vertex Pulling ✅ COMPLETE

Move geometry away from fixed vertex attributes into SSBOs for shader-side vertex pulling.

- [x] **Mesh vertex buffers include STORAGE + SHADER_DEVICE_ADDRESS** — `SituationCreateMesh` on Vulkan
      creates vertex and index buffers with `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`
      (shipped in v2.4.260). Vertex-pull is now possible: pass the BDA via push constant, read `positions[]` in GLSL.
- [x] **Struct layout defined** — `SituationDrawIndirectCommand` and `SituationDrawIndexedIndirectCommand`
      are STD430-compatible and match `VkDrawIndirectCommand` / GL indirect layouts exactly.
- [x] **Standard GLSL vertex-pull include file** — `sit/gpu/vertex_pull.glslh` provides
      canonical `buffer_reference` structs for all three mesh layouts (`SitVertex_Simple`,
      `SitVertex_Legacy`, `SitVertex_PBR`), `SitIndexBuffer`, and a `sit_bitangent()` helper.
- [x] **`SituationCmdSetVertexAttribute` deprecation** — marked `[Deprecated v2.4]` in
      `situation_api.h` with a migration note pointing to vertex pulling + `vertex_pull.glslh`.
      Full removal is a breaking change; flagged for v2.5 only.

**Note on GL:** GL path creates vertex buffers with `GL_ARRAY_BUFFER` usage. BDA is NVIDIA-only there
(`GL_NV_shader_buffer_load`). The vertex-pull pattern is practically Vulkan-only today.

---

## 3. GPU-Driven Rendering (Indirect Draw) ✅ COMPLETE

CPU submits one command; GPU reads a buffer and executes thousands of draws.

- [x] `SIT_FEATURE_DRAW_INDIRECT_COUNT` and `SIT_FEATURE_MULTI_DRAW_INDIRECT` feature flags defined
- [x] `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER` usage flag defined
- [x] `SituationDrawIndirectCommand` struct — matches `VkDrawIndirectCommand` / `glDrawArraysIndirect` layout
- [x] `SituationDrawIndexedIndirectCommand` struct — matches `VkDrawIndexedIndirectCommand` / `glDrawElementsIndirect` layout
- [x] `SituationDispatchIndirectCommand` struct — matches `VkDispatchIndirectCommand` / `glDispatchComputeIndirect` layout
- [x] `SituationCmdDrawIndirect` — implemented (GL + VK)
- [x] `SituationCmdDrawIndexedIndirect` — implemented (GL + VK)
- [x] `SituationCmdDispatchIndirect` — implemented (GL + VK), with offset alignment validation
- [ ] **"Compute Culling → Indirect Draw" example** — a standalone `examples/gpu_culling.c` showing:
      frustum cull in compute, write surviving draws into an indirect buffer, single `CmdDrawIndexedIndirect`.
      Good showcase for the BDA + indirect stack.

---

## 4. Render Graph (Frame Graph) — NOT STARTED

Declare resource dependencies; library auto-calculates barriers and layout transitions.

- [ ] Design `SituationRenderGraph` and `SituationRenderPassNode` public structs
- [ ] Implement dependency solver (topological sort + barrier inference)
- [ ] Transient Resource Allocator for memory aliasing (Phase 2 — deferred)

**Note:** Manual `SituationCmdPipelineBarrier` remains the current pattern. Render graph is a
significant undertaking; nothing has been started as of v2.4.262.

---

## 5. Asset Baking ("Situation Cooker") — NOT STARTED

Offline conversion of PNG/glTF to GPU-native binary formats for faster loading.

- [ ] Standalone CLI tool `situation-cook`
- [ ] Texture compression: PNG → BC7/ASTC via `ispc_texcomp` or `stb_dxt`
- [ ] Binary mesh format: glTF → flat binary (`.sita` header format)

---

## 6. Advanced Profiling (Tracy Integration) — NOT STARTED

GPU + CPU timeline visibility: see exactly how long the shadow pass took on the GPU,
correlate it with the physics update on the CPU, and identify where the frame budget goes.

**What already exists.** The library has substantial CPU-side timing infrastructure:
- `_SitGetMonotonicTimeNS()` — nanosecond-precision monotonic clock (`QueryPerformanceCounter` on Windows)
- `SituationGetFrameTime()` / `SituationGetFPS()` — frame delta and FPS
- `sit_render.submit_timestamps[]` + `metric_latency_sum_ns/count/max` — per-frame render-thread latency tracking
- `SituationGetThreadPoolMetrics()` — job system stats (submitted, completed, queue depths, steal counters, lock contention)
- `SituationGetThreadPoolSnapshot()` — per-thread slot info (role, CPU affinity, NUMA node, active flag)

**What does not exist yet:** any GPU timestamp query infrastructure (no `VkQueryPool`, no
`glBeginQuery(GL_TIME_ELAPSED)`), no profiler macro layer, no external tool integration.

**Dependency note:** Tracy is the right choice for the CPU zone side — it's MIT-licensed,
header-only on the client, low-overhead, and produces beautiful flame graphs. However it
requires a network connection to `tracy-profiler` or the standalone capture tool at runtime.
The integration is opt-in via `SITUATION_ENABLE_TRACY`. All zones must compile to zero-cost
no-ops when the define is absent — non-negotiable for shipping builds.

---

### 6.0 Architecture Decision: What to Integrate

Two layers are independent and can be done separately:

| Layer | Tool | What it shows |
|-------|------|---------------|
| **CPU zones** | Tracy | Main thread, render thread, audio thread, I/O thread, job workers — labeled regions with nanosecond timestamps |
| **GPU timestamps** | Library-internal `VkQueryPool` / `GL_TIME_ELAPSED` | How long each render pass, compute dispatch, and present took on the GPU — mapped onto the same Tracy timeline |

CPU zones are ~2 days of work. GPU timestamps require new persistent state in `_SituationRenderState` and careful query readback synchronization — closer to a week. They can ship independently.

---

### 6.1 Dependency and Compile Gate

- [ ] **6.1a** Add `ext/tracy/` as an optional submodule or vendored copy of the Tracy client
      headers (`Tracy.hpp`, `TracyC.h`). The client is header-only; no compiled `.c` needed
      unless `TRACY_ENABLE` is defined.
- [ ] **6.1b** Add `SITUATION_ENABLE_TRACY` to the compile-time define table in `situation_api.h`.
      When not defined: all zone macros expand to nothing at zero cost.
      When defined: pull in `TracyC.h` and emit instrumentation.
- [ ] **6.1c** Add `-DSITUATION_ENABLE_TRACY -DTRACY_ENABLE` to an optional flag in
      `build_situation.bat`. Document in `doc/COMPILATION_GUIDE.md`.
- [ ] **6.1d** Decide C vs C++ Tracy client. The C API (`TracyC.h`) avoids forcing C++ into
      the library's C11 build. Prefer it unless profiling throughput is measurably worse.

---

### 6.2 CPU Zone Macro Layer

Define Situation-native macros that wrap Tracy (or expand to nothing). Keep Tracy's names
out of user-facing code entirely:

```c
// sit/situation_impl_trace_prof.h  (new file, included from situation_impl.h)

#ifdef SITUATION_ENABLE_TRACY
  #include <tracy/TracyC.h>
  #define SIT_PROF_ZONE(name)          TracyCZone(___sit_ctx_##__LINE__, 1); \
                                        TracyCZoneName(___sit_ctx_##__LINE__, name, sizeof(name)-1)
  #define SIT_PROF_ZONE_END(name)      TracyCZoneEnd(___sit_ctx_##__LINE__)
  #define SIT_PROF_FRAME_MARK()        TracyCFrameMark
  #define SIT_PROF_ALLOC(ptr, size)    TracyCAllocN(ptr, size, "Situation")
  #define SIT_PROF_FREE(ptr)           TracyCFreeN(ptr, "Situation")
#else
  #define SIT_PROF_ZONE(name)          do {} while(0)
  #define SIT_PROF_ZONE_END(name)      do {} while(0)
  #define SIT_PROF_FRAME_MARK()        do {} while(0)
  #define SIT_PROF_ALLOC(ptr, size)    do {} while(0)
  #define SIT_PROF_FREE(ptr)           do {} while(0)
#endif
```

- [ ] **6.2a** Create `sit/situation_impl_trace_prof.h` with the macro definitions above.
- [ ] **6.2b** Include it from `situation_impl.h` after the Tracy header guard.
- [ ] **6.2c** Expose `SIT_PROFILE_ZONE(name)` / `SIT_PROFILE_ZONE_END(name)` as the
      user-facing aliases in `situation_api.h` (so application code can also place zones
      without directly depending on Tracy).

---

### 6.3 Instrument Library Call Sites (CPU)

Priority order — instrument the hot path first, everything else can follow:

- [ ] **6.3a** `SituationEndFrame` — frame boundary: `SIT_PROF_FRAME_MARK()` at the top.
      This is the single most important instrumentation point; Tracy uses it to delineate frames.
- [ ] **6.3b** `SituationAcquireFrameCommandBuffer` — zone from entry to return.
      Contains the fence wait which is often the CPU idle point.
- [ ] **6.3c** `_SituationRenderThreadEntry` — zone each frame slot execution (dequeue → submit → present).
- [ ] **6.3d** `_SituationSubmitGraphics` / `_SituationSubmitCompute` — zone the `vkQueueSubmit` calls.
      These are where the CPU hands work to the GPU; knowing how long they take is diagnostic.
- [ ] **6.3e** `SituationUpdateTimers` / `SituationPollInputEvents` — short zones; useful for
      detecting OS event stalls.
- [ ] **6.3f** Job system: zone each `SituationSubmitJobEx` dispatch and each worker's job
      execution body in `_SituationWorkerThread`. Label with the job's `func` pointer (or a
      user-provided name if the API gains one).
- [ ] **6.3g** Audio callback: zone the miniaudio device callback and the node graph process tick.
      The audio thread is the most latency-sensitive — any spikes here cause glitches.
- [ ] **6.3h** Hot reload: zone `SituationPollShaderLoad` polling and the compile completion
      handoff. These fire infrequently but can stall the frame when they land.

---

### 6.4 GPU Timestamps — Vulkan

Vulkan timestamp queries use `VkQueryPool` with `VK_QUERY_TYPE_TIMESTAMP`. The pool must
be created at init, queries written into command buffers, and results read back one frame
later (after the fence confirms completion).

**New state required in `_SituationRenderState::vk`:**

```c
VkQueryPool gpu_timestamp_pool;        // TIMESTAMP pool, 2 * FRAMES_IN_FLIGHT * MAX_ZONES queries
uint32_t    gpu_timestamp_slots;       // How many zones are active this frame
uint64_t    gpu_timestamp_results[...];// Readback buffer (heap-allocated)
float       gpu_timestamp_period_ns;  // VkPhysicalDeviceLimits::timestampPeriod
bool        gpu_timestamps_supported; // False on devices where timestampComputeAndGraphics = false
```

- [ ] **6.4a** At VK init (`_SituationInitVulkan`): query `timestampPeriod` from
      `VkPhysicalDeviceLimits`. Check `limits.timestampComputeAndGraphics` — if false, GPU
      timestamps are unsupported on this device; set `gpu_timestamps_supported = false` and
      skip pool creation. Log a diagnostic.
- [ ] **6.4b** Create `VkQueryPool` with type `VK_QUERY_TYPE_TIMESTAMP`,
      `queryCount = 2 * SITUATION_MAX_FRAMES_IN_FLIGHT * SIT_PROF_GPU_MAX_ZONES` where
      `SIT_PROF_GPU_MAX_ZONES` defaults to 16 (overridable by define). Pool is created once,
      never recreated on swapchain resize.
- [ ] **6.4c** At start of each frame (in `SituationAcquireFrameCommandBuffer` after fence wait):
      call `vkCmdResetQueryPool` for this frame's query slot range (avoids `vkResetQueryPool`
      which requires Vulkan 1.2 — `vkCmdResetQueryPool` is 1.0).
- [ ] **6.4d** Add `SituationCmdGPUZoneBegin(cmd, zone_id)` and `SituationCmdGPUZoneEnd(cmd, zone_id)`:
      record `vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, base + zone_id*2)` /
      `vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, base + zone_id*2+1)`.
      `zone_id` is 0..`SIT_PROF_GPU_MAX_ZONES-1`.
      These are no-ops when `SITUATION_ENABLE_TRACY` is not defined.
- [ ] **6.4e** Readback: in `SituationAcquireFrameCommandBuffer`, after the fence wait confirms
      frame N-2 is done, call `vkGetQueryPoolResults` with `VK_QUERY_RESULT_64_BIT` to read
      that frame's timestamps. Convert raw ticks to nanoseconds using `timestampPeriod`.
- [ ] **6.4f** Feed results into Tracy via `TracyCZoneBeginAllocSrc` / `TracyCGpuZone` (or
      the equivalent C API calls) so GPU zones appear on the Tracy GPU track aligned to CPU time.
      Tracy's GPU zone API requires a calibrated GPU–CPU clock offset; compute this once at init
      using `vkGetCalibratedTimestampsEXT` (check for `VK_EXT_calibrated_timestamps` support first —
      fall back to estimated offset if unavailable).
- [ ] **6.4g** On swapchain recreation: do NOT recreate the query pool — it is not swapchain-scoped.
      Reset the per-frame slot counters only.
- [ ] **6.4h** On shutdown: `vkDestroyQueryPool` after the final fence wait.

---

### 6.5 GPU Timestamps — OpenGL

OpenGL uses `GL_TIME_ELAPSED` or `GL_TIMESTAMP` queries. The approach is simpler than Vulkan
but also less precise (no multi-queue).

- [ ] **6.5a** Check `GL_ARB_timer_query` / `GL_EXT_disjoint_timer_query` support at GL init.
      Set `sit_render.gl.gpu_timestamps_supported` accordingly.
- [ ] **6.5b** Create a ring of `2 * SIT_PROF_GPU_MAX_ZONES` `GLuint` query objects per frame
      slot using `glCreateQueries(GL_TIMESTAMP, ...)`.
- [ ] **6.5c** `SituationCmdGPUZoneBegin` / `SituationCmdGPUZoneEnd` call
      `glQueryCounter(query_id, GL_TIMESTAMP)` at the begin/end points.
- [ ] **6.5d** Readback: one frame later (after `GL_SYNC_GPU_COMMANDS_COMPLETE` or equivalent),
      call `glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns_value)`.
- [ ] **6.5e** Feed into Tracy GPU track (same as VK, but offset calibration uses
      `glGetInteger64v(GL_TIMESTAMP, ...)` vs a CPU clock sample taken immediately before/after).

---

### 6.6 `SituationGetProfilingData` — headless/non-Tracy readback

For users who don't want Tracy but still want frame timing data programmatically:

- [ ] **6.6a** Define:
      ```c
      typedef struct {
          uint64_t acquire_ns;      // AcquireFrameCommandBuffer duration
          uint64_t submit_ns;       // vkQueueSubmit / glFlush duration
          uint64_t present_ns;      // vkQueuePresent duration
          uint64_t render_thread_ns;// Full render thread slot duration
          uint64_t gpu_total_ns;    // GPU timestamp: first begin → last end (if queries enabled)
          uint64_t cpu_frame_ns;    // Wall-clock frame duration (EndFrame to EndFrame)
      } SituationFrameProfile;
      ```
- [ ] **6.6b** Implement `SITAPI SituationError SituationGetFrameProfile(SituationFrameProfile* out)`
      — returns data for the most recently completed frame. Thread-safe read from atomics
      that the render thread populates.
- [ ] **6.6c** This struct is populated regardless of `SITUATION_ENABLE_TRACY` — it's always on.
      The CPU fields reuse existing `submit_timestamps` and `metric_latency_*` atomics; only
      the `gpu_*` fields require query pool infrastructure from §6.4/6.5.

---

### 6.7 Error codes

- [ ] `SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED` — device doesn't support timestamp queries
      (`timestampComputeAndGraphics = false` on VK, or timer query extension absent on GL)
- [ ] `SITUATION_ERROR_PROFILING_ZONE_OVERFLOW` — `zone_id >= SIT_PROF_GPU_MAX_ZONES`
- [ ] Add string entries to `SituationGetErrorString()` in `situation_impl_ctrl.h`

---

### 6.8 Ordering summary

```
6.1  Tracy dependency + compile gate  (prerequisite for everything CPU-side)
  └─ 6.2  Macro layer (SIT_PROF_ZONE etc.)
       └─ 6.3  Instrument call sites (CPU)  ← minimum shippable CPU profiling
6.4  GPU timestamps (Vulkan)          ← independent of 6.1–6.3 if not feeding Tracy
6.5  GPU timestamps (OpenGL)          ← parallel to 6.4
6.6  SituationGetFrameProfile         ← depends on 6.4 for gpu_total_ns; CPU fields available sooner
6.7  Error codes                      ← add any time before first public API surface
```

**Minimum shippable scope:** 6.1 + 6.2 + 6.3a (frame mark) + 6.3b + 6.3c + 6.6.
That gives frame-boundary markers for Tracy, acquire/submit/present zones, and a headless
`SituationGetFrameProfile` API that works on every hardware regardless of Tracy or GPU timestamps.
GPU zone readback (6.4/6.5) is the second phase — more impactful for shader optimization work.

---

## 7. Timeline Semaphores — NOT STARTED

Multi-queue async synchronization: compute and graphics run concurrently, synchronized only at
the precise moment data crosses between them.

**Why this looks easy and isn't.** The current frame loop is built on a binary fence +
binary semaphore model that is correct, battle-tested, and deeply interwoven with several
unrelated subsystems. The two plan items originally listed here compress roughly 15 discrete
migration tasks and a full regression test pass.

---

### 7.0 Prerequisites

These must be true before touching any sync code:

- [x] Vulkan 1.2+ required by the library minimum spec — timeline semaphores are core in 1.2,
      no extension load needed. `VkSemaphoreTypeCreateInfo` with `VK_SEMAPHORE_TYPE_TIMELINE`
      and `vkSignalSemaphore` / `vkWaitSemaphores` are unconditionally available.
- [ ] **Dedicated async compute queue** — the library currently uses a single
      `sit_render.vk.compute_queue` that may alias the graphics queue on many GPUs.
      A real timeline payoff requires confirming a distinct `VK_QUEUE_COMPUTE_BIT`-only queue
      family exists and selecting it at device creation. Without a second queue, timeline
      semaphores reduce to bookkeeping overhead with no throughput benefit.
      Action: audit `_SituationInitVulkan` queue family selection; expose
      `SituationGetGraphicsCaps::has_dedicated_compute_queue`.

---

### 7.1 Sync Object Inventory (everything that must change)

The frame sync topology today (per `_SituationRenderState` and `situation_impl_decl.h`):

| Object | Role | Count | Migration target |
|--------|------|-------|-----------------|
| `vk.in_flight_fences[]` | CPU gate: AcquireFrame waits on this before reusing a frame slot | `MAX_FRAMES_IN_FLIGHT` | Remove. Replace with CPU wait on timeline semaphore value `(frame_slot * N)`. |
| `vk.image_available_semaphores[]` | Binary: `vkAcquireNextImageKHR` signals → graphics submit waits | `MAX_FRAMES_IN_FLIGHT` | **Cannot be replaced by timeline.** `vkAcquireNextImageKHR` only signals binary semaphores. Keep as-is. |
| `vk.render_finished_semaphores[]` | Binary: graphics submit signals → `vkQueuePresentKHR` waits | `MAX_FRAMES_IN_FLIGHT` | **Cannot be replaced by timeline.** `vkQueuePresentKHR` only waits on binary semaphores. Keep as-is. |
| `vk.compute_finished_semaphores[]` | Binary: compute submit signals → graphics submit waits (when `frame_has_async_compute`) | `MAX_FRAMES_IN_FLIGHT` | **Replace with timeline.** Graphics submit does `vkWaitSemaphores` on compute timeline value instead. This is the actual timeline payoff. |
| `render.frame_refcounts[]` | Atomic: how many resources reference a frame slot | `MAX_FRAMES_IN_FLIGHT` | Unchanged — these are CPU-side, not GPU sync. |
| `render.submit_timestamps[]` | Latency metrics per slot | `MAX_FRAMES_IN_FLIGHT` | Unchanged. |

**Key constraint:** `image_available` and `render_finished` semaphores are permanently binary
because the Vulkan WSI (windowing system) integration spec requires binary semaphores for
acquire and present. This means a pure timeline-only model is impossible; the architecture is
always a hybrid.

---

### 7.2 `_SituationVulkanCreateSyncObjects` — phased migration

- [ ] **7.2a** Add one `VK_SEMAPHORE_TYPE_TIMELINE` semaphore for the compute queue:
      `vk.compute_timeline_semaphore` (single, not per-frame — timeline value monotonically
      increases). Initial value 0.
- [ ] **7.2b** Keep `compute_finished_semaphores[]` alive during transition (needed for
      rollback). Add `compute_timeline_value` counter to `_SituationRenderState`.
- [ ] **7.2c** In `_SituationVulkanDestroySyncObjects` (and swapchain cleanup / shutdown
      path `_SituationVulkanWaitInFlightFencesPump`): destroy the timeline semaphore alongside
      existing objects.

---

### 7.3 `_SituationSubmitCompute` — use timeline signal

Current code:
```c
// signals binary compute_finished_semaphores[current_frame_index]
vkQueueSubmit(compute_queue, 1, &submit, VK_NULL_HANDLE);
```

- [ ] **7.3a** Migrate to `vkQueueSubmit2` with `VkSemaphoreSubmitInfo`:
      signal `vk.compute_timeline_semaphore` at value `++compute_timeline_value`.
- [ ] **7.3b** Store the signalled timeline value in `vk.compute_frame_timeline_value[current_frame_index]`
      so `_SituationSubmitGraphics` knows exactly which value to wait on for this frame.

---

### 7.4 `_SituationSubmitGraphics` — wait on timeline instead of binary

Current code adds `compute_finished_semaphores[frame]` to `pWaitSemaphores` when
`frame_has_async_compute` is set.

- [ ] **7.4a** Replace that conditional wait with `VkSemaphoreSubmitInfo` waiting on
      `vk.compute_timeline_semaphore` at `vk.compute_frame_timeline_value[current_frame_index]`
      when compute was used this frame.
- [ ] **7.4b** Migrate graphics submit from `vkQueueSubmit` to `vkQueueSubmit2` to accept
      `VkSemaphoreSubmitInfo` with proper `stageMask` (replaces `pWaitDstStageMask`).
      Stage mask for compute→graphics dependency:
      `VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT`
- [ ] **7.4c** Keep signalling `render_finished_semaphores[frame]` (binary) — present still needs it.
- [ ] **7.4d** Keep passing `in_flight_fences[frame]` to `vkQueueSubmit2` fence parameter — the CPU
      gate on frame slot reuse is still needed until 7.5 is complete.

---

### 7.5 Replace `in_flight_fences` CPU gate with timeline wait (optional phase)

The fence array is the CPU-side "don't reuse this frame slot yet" gate. It can be replaced
by a second timeline semaphore on the graphics queue that the CPU polls directly.

- [ ] **7.5a** Add `vk.graphics_timeline_semaphore` (monotonic, initial value 0).
- [ ] **7.5b** Signal it from `_SituationSubmitGraphics` at value `++graphics_timeline_value`.
      Store per-slot value in `vk.graphics_frame_timeline_value[current_frame_index]`.
- [ ] **7.5c** In `SituationAcquireFrameCommandBuffer`, replace:
      ```c
      vkWaitForFences(device, 1, &in_flight_fences[frame], VK_TRUE, timeout)
      vkResetFences(device, 1, &in_flight_fences[frame])
      ```
      with:
      ```c
      // CPU blocks until graphics queue reaches the value submitted for this slot
      VkSemaphoreWaitInfo wi = { ..., .pValues = &graphics_frame_timeline_value[frame] };
      vkWaitSemaphores(device, &wi, timeout_ns);
      ```
      This eliminates `vkResetFences` and the reset-failure error path.
- [ ] **7.5d** Update `_SituationVulkanWaitFencePumpWindowBudget` / `_SituationVulkanWaitInFlightFencesPump`
      to use `vkWaitSemaphores` with the same chunked + `glfwPollEvents` pattern. The
      window-pump logic is load-bearing (prevents "Not Responding" on Windows) and must
      be preserved exactly.
- [ ] **7.5e** Remove `vk.in_flight_fences[]`, `_SituationVulkanCreateSyncObjects` fence
      alloc/init, and all `vkWaitForFences` / `vkResetFences` call sites.

**Note:** 7.5 is lower priority than 7.2–7.4. The fence array works correctly and
this phase only eliminates one `vkResetFences` call per frame. Do 7.2–7.4 first; park 7.5
unless profiling shows fence overhead.

---

### 7.6 Graveyard flush timing

The graveyard (deferred resource destruction) relies on the fence completing to know it's safe
to call `vkDestroy*`. Specifically, in `SituationAcquireFrameCommandBuffer`:

```c
// reset staging cursor and flush graveyard only after fence confirmed GPU is done
sit_render.vk.staging_buffers[current_frame_index].cursor = 0;
_SituationFlushGraveyard(current_frame_index);
```

- [ ] **7.6a** After 7.5, replace fence-based graveyard gate with timeline semaphore query:
      call `vkGetSemaphoreCounterValue` to confirm the graphics timeline has passed
      `graphics_frame_timeline_value[frame]` before flushing. In practice the `vkWaitSemaphores`
      in 7.5c already guarantees this, so the flush can stay in the same location with no
      additional check.

---

### 7.7 Screenshot readback

Screenshot capture uses `screenshot_copy_pending[frame_index]` to defer CPU-side pixel
copy until after the fence confirms GPU completion. Touch points:

- `_SituationVulkanResolveScreenshotAfterSubmit(frame_index)` — called after fence wait
- `screenshot_copy_pending[]` — set in EndFrame command recording, cleared in resolve
- `screenshot_resolved_frame_index` — frame slot whose pixels are valid

- [ ] **7.7a** After 7.5 replaces the fence, confirm `_SituationVulkanResolveScreenshotAfterSubmit`
      is still called from the same location in `AcquireFrame` (after the timeline wait that
      replaced the fence wait). No functional change — just verify the call order is preserved.
- [ ] **7.7b** No structural changes to the screenshot arrays are needed; they remain
      per-frame-slot boolean flags.

---

### 7.8 Render thread and hot reload

The render thread (`_SituationRenderThreadEntry`) processes frame slots from the render queue.
Hot reload (`SituationPollShaderLoad`) fires from the I/O thread. Neither directly touches
fence or semaphore objects — they operate on the command buffer and pipeline handles. No
changes required for either, but both must be confirmed not to call `vkWaitForFences`
directly.

- [ ] **7.8a** Grep `vkWaitForFences` in the full renderer to find any call site outside
      `AcquireFrame` / `WaitInFlightFencesPump`. Each one must be audited: is it an
      in-flight-fence wait (migrate) or a one-shot upload fence (leave alone — single-time
      commands use `vkQueueWaitIdle`, not the per-frame fences).

---

### 7.9 `_SituationVulkanCreateSyncObjects` / `_SituationVulkanDestroySyncObjects` audit

These two functions are the authoritative source of per-frame sync object allocation.
They are called from `_SituationInitVulkan` and from swapchain recreation
(`_SituationVulkanRecreateSwapchain`). Swapchain recreation rebuilds per-frame fences and
binary semaphores but must not reset the timeline counter (the timeline is not swapchain-scoped).

- [ ] **7.9a** Ensure swapchain recreation does not destroy/recreate the timeline semaphores —
      timeline semaphores survive swapchain teardown, their counter must stay monotonic.
- [ ] **7.9b** Ensure `_SituationCleanupVulkan` / `SituationShutdown` path destroys timeline
      semaphores after all submitted work is confirmed complete (the existing
      `_SituationVulkanWaitInFlightFencesPump` call in shutdown covers this if 7.5 is done,
      or via a final `vkWaitSemaphores` on both timeline semaphores if not).

---

### 7.10 Error codes

- [ ] Add `SITUATION_ERROR_VULKAN_TIMELINE_SEMAPHORE_FAILED` to `situation_base_errno.h`
      for `vkCreateSemaphore(VK_SEMAPHORE_TYPE_TIMELINE)` failure and
      `vkWaitSemaphores` timeout paths.
- [ ] Add string entry to `SituationGetErrorString()` table in `situation_impl_ctrl.h`.

---

### 7.11 Tests

- [ ] Add `test_timeline_semaphore_compute_graphics_ordering` to `tests/harness/test_graphics.c`:
      dispatch compute that writes a known value to an SSBO, then draw a pixel that reads
      from it; assert pixel color matches expected value. This test already implicitly passes
      (binary semaphore provides the same ordering guarantee) but becomes the regression
      fixture for the timeline migration.
- [ ] Add `test_graphics_caps_dedicated_compute_queue` to `test_graphics.c`: asserts that
      `SituationGetGraphicsCaps::has_dedicated_compute_queue` returns a valid bool without
      crashing (hardware-agnostic; just validates the capability query path from 7.0).

---

### 7.12 Ordering summary

```
7.0  Prerequisites (dedicated queue detection)
  └─ 7.2  Create timeline semaphore for compute queue
       └─ 7.3  Migrate _SituationSubmitCompute to timeline signal
            └─ 7.4  Migrate _SituationSubmitGraphics to timeline wait (vkQueueSubmit2)
                 ├─ 7.6  Graveyard flush re-check (trivial after 7.5)
                 ├─ 7.7  Screenshot readback audit (verify, no change expected)
                 ├─ 7.8  vkWaitForFences audit across renderer
                 └─ 7.5  [Optional] Replace in_flight_fences with graphics timeline
                      └─ 7.9  Swapchain recreation + shutdown audit
7.10 Error codes  (add any time before first test run)
7.11 Tests        (write before 7.3, run after 7.4)
```

**Minimum shippable scope:** 7.0 + 7.2 + 7.3 + 7.4 + 7.10 + 7.11.
That delivers the actual async compute↔graphics synchronization benefit while leaving
the fence-array CPU gate in place. 7.5 onward is polish.

---

## 8. Async I/O — MOSTLY COMPLETE

Non-blocking file I/O so asset loads don't cause frame spikes.

- [x] **Dedicated I/O thread** — `pool->io_thread` launched at `SituationInit` when threading enabled;
      controlled via `SituationInitInfo::disable_io_thread`
- [x] **`SituationLoadFileAsync`** — binary file load offloaded to I/O thread via callback
- [x] **`SituationLoadFileTextAsync`** — text file load variant
- [x] **`SituationSaveFileAsync`** — async binary write
- [x] **`SituationSaveFileTextAsync`** — async text write
- [x] **`SituationLoadSoundFromFileAsync`** — lock-free audio asset load
- [ ] **`SituationLoadTextureAsync`** — GPU texture upload from background thread; not implemented.
      The raw file load can use `SituationLoadFileAsync`, but the GPU-side decode + upload
      (`SituationCreateTexture`) still blocks the main thread. A proper implementation needs
      a staging buffer + deferred GPU upload handoff.

---

## 9. KFS — Asset & Knowledge Filing System

KFS (`sit/kfs/lib_kfs.h`) is the Situation library's dedicated, self-contained asset management
and structured knowledge system. It is **not** a virtual filesystem abstraction over loose files
or a .zip reader — it is a SQLite-backed, domain-isolated, permission-controlled database for
storing and retrieving any type of game/application asset alongside its semantic metadata.

The original roadmap entry ("Virtual Mounts / VFS") misidentified what KFS is and what its role
is. This section replaces it with an accurate scope and integration plan.

---

### 9.0 What KFS Is

KFS stores assets in three SQLite databases opened simultaneously via `GameDB`:

| Database | Contents |
|----------|----------|
| `artifacts.db` | Binary and text asset blobs (`Assets` table: type, name, format, data, metadata) |
| `architecture.db` | Semantic structure: `Artifacts`, `Topics`, `Epics`, `Notes`, cross-links |
| `registry.db` | Identity and access control: `Actors`, `Domains`, `SecuritySchemes`, `SchemeAllowedActors`, `GroupMembers` |

**Entity model:**
- **Artifact** — a named, typed asset (binary or text) with an optional data blob and linked topics/notes
- **Topic** — a semantic tag/category; artifacts are assigned to topics; topics can form hierarchies (subtopics)
- **Epic** — a higher-level grouping of topics; used to load entire feature sets in one call
- **Note** — free-form annotation attached to any entity
- **Actor** — any identity (User, Group, Company, System); actors own entities and receive permissions
- **Domain** — isolated namespace; all entities are scoped to a domain; the domain firewall enforces cross-domain access control
- **Security Scheme** — an ACL applied to an entity: grants specific Actors Read/Write/Delete on that entity

**Permission model:** `kfs_check_permission` enforces a 7-step check: inactive actor → AdminGroup bypass → domain firewall → ownership (direct + group-transitive) → security scheme ACL.

**Current state:** KFS is fully implemented (14,000+ lines), fully trace-registered in
`situation_base_trace.h` (trace range `10140001–10140121`, 121 functions), and used as a
standalone library. It is **not yet integrated** into the Situation public API (`situation_api.h`)
— callers use `lib_kfs.h` directly.

---

### 9.1 Situation ↔ KFS Integration

The gap between the current state and the AAA vision is the integration layer: Situation's
high-level loaders (`SituationLoadTexture`, `SituationLoadModel`, `SituationLoadSound`, etc.)
should be able to source assets from a KFS database transparently, the same way they today
source from the filesystem.

- [ ] **9.1a — `SituationKFSOpen` / `SituationKFSClose`**: Wrap `kfs_init` / `kfs_close`
      behind `SITAPI` functions so the caller never touches `GameDB*` directly. Store the
      active `GameDB*` in `SituationContext` (or as a user-managed handle returned by the API).
      Decide: single global KFS context (simple) vs. multiple named handles (flexible).

- [ ] **9.1b — `SituationLoadTextureFromKFS`**: Load a texture artifact by name/ID from KFS,
      forwarding the blob to the existing `SituationCreateTexture` upload path. No new GPU code
      needed — just a KFS read + hand-off.

- [ ] **9.1c — `SituationLoadModelFromKFS`**: Same pattern for `.gltf`/`.glb`/`.obj` stored
      as binary blobs in KFS. The existing cgltf/OBJ parsers already accept in-memory buffers.

- [ ] **9.1d — `SituationLoadSoundFromKFS`**: Load audio asset blob, forward to miniaudio
      in-memory decoder. Same pattern.

- [ ] **9.1e — Topic/Epic bulk load helpers**: `SituationLoadAssetsByTopic` and
      `SituationLoadAssetsByEpic` — wrap `kfs_load_by_topic` / `kfs_load_by_epic` and
      dispatch each asset to the appropriate loader based on `KFS_Asset::type`. This is the
      "load entire feature set" call that makes KFS genuinely useful at the game-loop level.

---

### 9.2 Dependency: SQLite

KFS requires `sqlite3`. This is the only new external dependency.

- [ ] **9.2a** Verify SQLite is already present or add it to `ext/`. SQLite's amalgamation
      (`sqlite3.c` + `sqlite3.h`) is a single-file add, MIT-compatible.
- [ ] **9.2b** Add `sqlite3.c` to `build_situation.bat` compilation and confirm it links
      into the static `.a` correctly.
- [ ] **9.2c** Add `-Iext/sqlite3` (or wherever it lands) to `build_tests.bat` and
      `build_examples.bat` include paths.

---

### 9.3 Compile-time gating

KFS is heavyweight (SQLite dep, three database files). It should be opt-in:

- [ ] **9.3a** Guard `lib_kfs.h` inclusion in `situation_impl.h` behind
      `#ifdef SITUATION_ENABLE_KFS`.
- [ ] **9.3b** All `SITAPI` wrapper functions (§9.1) compile to stubs returning
      `SITUATION_ERROR_NOT_IMPLEMENTED` when `SITUATION_ENABLE_KFS` is not defined.
- [ ] **9.3c** Add `-DSITUATION_ENABLE_KFS` to `build_situation.bat` as an optional flag
      (off by default until integration is stable).

---

### 9.4 Error codes

- [ ] Add `SITUATION_ERROR_KFS_NOT_INITIALIZED` — caller tried to use KFS API without `SituationKFSOpen`
- [ ] Add `SITUATION_ERROR_KFS_ARTIFACT_NOT_FOUND` — wraps `KFS_NOTFOUND`
- [ ] Add `SITUATION_ERROR_KFS_PERMISSION_DENIED` — wraps `KFS_PERMISSION_DENIED`
- [ ] Add `SITUATION_ERROR_KFS_DATABASE_ERROR` — wraps SQLite errors from KFS layer
- [ ] Add string entries to `SituationGetErrorString()` in `situation_impl_ctrl.h`

---

### 9.5 What KFS is NOT (scope boundaries)

To avoid scope creep, these are explicitly out of scope:

- **Not a VFS over loose files.** KFS does not mount directories or intercept `fopen`. Loose file
  loading stays in `situation_impl_io.h` via `SituationLoadFileData` / `SituationLoadTexture` etc.
- **Not a .zip/.pak reader.** Packed archive support would be a separate `sit/pak/` module if
  ever needed; KFS's blob storage already covers the "single deployable asset container" use case
  more richly.
- **Not a save-game system.** Although KFS could store save data, that is an application concern.
  Situation's role is to expose the KFS API; how apps use it is up to them.
- **Not a streaming cache.** Hot-reloading (`SituationPollShaderLoad`) stays in the renderer;
  KFS is for offline/startup asset loading, not per-frame streaming.

---

### 9.6 Ordering

```
9.2  SQLite dependency (prerequisite for everything)
  └─ 9.3  Compile-time gating (SITUATION_ENABLE_KFS)
       └─ 9.1a  SituationKFSOpen / SituationKFSClose
            ├─ 9.1b  LoadTextureFromKFS
            ├─ 9.1c  LoadModelFromKFS
            ├─ 9.1d  LoadSoundFromKFS
            └─ 9.1e  LoadAssetsByTopic / LoadAssetsByEpic
9.4  Error codes  (add before any public API surface lands)
```

**Minimum shippable scope:** 9.2 + 9.3 + 9.1a + 9.1b + 9.4.
That gives callers the ability to open a KFS database and load textures from it with proper
error propagation. The remaining loaders and bulk helpers follow the same pattern.

---

## 10. Web & Platform Reach (v2.5+) — NOT STARTED

- [ ] Emscripten WASM compilation target (windowing/input via HTML5 API)
- [ ] WebGPU backend (Dawn or Emscripten WebGPU)

**Prerequisite:** Linux platform layer needs to land first.

---

## 11. Ecosystem Tooling (v3.0) — NOT STARTED

- [ ] Lightweight immediate-mode UI toolkit
- [ ] Visual task graph profiler for `SituationDumpTaskGraph` output

---

## Status Summary (v2.4.262)

| Section | Status |
|---------|--------|
| 1. Bindless / Universal Handles | ✅ Complete |
| 2. SSBO-First / Vertex Pulling | ✅ Complete |
| 3. GPU-Driven Indirect Draw | ✅ Complete (example pending) |
| 4. Render Graph | ❌ Not started |
| 5. Asset Baking | ❌ Not started |
| 6. Tracy Profiling | 📋 Scoped (see §6 detail — CPU zones + GPU timestamps + headless API) |
| 7. Timeline Semaphores | 📋 Scoped (see §7 detail — ~15 tasks, hybrid binary+timeline model) |
| 8. Async I/O | ✅ File I/O done; texture streaming pending |
| 9. Virtual Mounts / VFS | 📋 Scoped — KFS is the implementation; integration layer pending (see §9) |
| 10. Web / Emscripten | ❌ Not started |
| 11. Ecosystem Tooling | ❌ Not started |
