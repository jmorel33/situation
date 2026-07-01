# AAA Architecture Plan

Tracks the long-horizon GPU architecture and ecosystem goals for the Situation SDK.
Covers bindless rendering, GPU-driven indirect draw, render graphs, asset pipeline, profiling,
async I/O, and platform reach.

Originally `doc/ROADMAP.md`. Renamed and audited through **v2.4.399** (renderer bolster Phase 10 + MSAA Phase 0 cross-refs; **application identity** WI-0–WI-4 @ v2.4.399). Canonical detail for Phase 10 slices: **`doc/plan/renderer_bolster_plan.md`** § Phase 10. Identity: **`doc/architecture.md`** § Application Identity, **`doc/plan/SIT_IDENTITY_PLAN.md`**.

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
significant undertaking; nothing has been started as of v2.4.398.

---

## 5. Asset Baking ("Situation Cooker") — NOT STARTED

Offline conversion of PNG/glTF to GPU-native binary formats for faster loading.

- [ ] Standalone CLI tool `situation-cook`
- [ ] Texture compression: PNG → BC7/ASTC via `ispc_texcomp` or `stb_dxt`
- [ ] Binary mesh format: glTF → flat binary (`.sita` header format)

---

## 6. Advanced Profiling (Tracy Integration) — Phase 10 complete @ v2.4.397; Tracy GPU track feed still open

**Phase 10 status (renderer bolster):** **P10.0–P10.4 ✅ @ v2.4.397** — lightweight diagnostics, `SituationGetFrameProfile`, Tracy CPU zones (opt-in @ v2.4.395), internal GPU timestamp zones (P10.3 @ v2.4.396), **public `SituationQueryPool`** (P10.4 @ v2.4.397). **Still open in this §:** Tracy GPU track feed — **6.4f** (VK: extend `_SitGpuProfReadbackVulkan`, raw timestamp ticks + calibration) and **6.5e** (GL: separate absolute-timestamp path; not elapsed-only).

**Primary use case (driving this work):** When debugging random frame-time spikes / micro-stutters (invisible in average FPS but causing visible hitches or timing issues), we need objective detection and cause attribution. This must work for any application using the library (not just examples), e.g. to identify backpressure waits, fence/clientwait times, present/swap overhead, user code, render-thread scheduling, etc. without subjective observation or requiring external tools. Ex02 (and other examples) serve only as convenient test cases. **Shipped:** `renderer_bolster_plan.md` **Phase 10 complete @ v2.4.397** (see §6 below). MSAA / VD work is **not** in this plan — see bolster Phase 11 / VD-4b.

GPU + CPU timeline visibility: see exactly how long the shadow pass took on the GPU,
correlate it with the physics update on the CPU, and identify where the frame budget goes.

**What already exists.** The library has substantial CPU-side timing infrastructure:
- `_SitGetMonotonicTimeNS()` — nanosecond-precision monotonic clock (`QueryPerformanceCounter` on Windows)
- `SituationGetFrameTime()` / `SituationGetFPS()` — frame delta and FPS (primary signal for visible stutters)
- `sit_render.submit_timestamps[]` + `metric_latency_sum_ns/count/max` — per-frame render-thread latency tracking (used for adaptive backpressure)
- `SituationDrawMetricsOverlay`, `SituationExportRenderHistogram`, `SituationGetRenderLatencyStats`, `SituationGetRenderQueueDepth`, `SituationGetDrawCallCount` — high-level diagnostics (**P10.0 ✅**)
- `SituationGetFrameProfile`, `SituationResetFrameProfileStats` — headless snapshot (**P10.1 ✅ v2.4.394**)
- `SIT_PROFILE_ZONE_*` / Tracy opt-in — CPU zones (**P10.2 ✅ v2.4.395**)
- `SituationCmdGPUZoneBegin/End` → `gpu_zone_ns[]` — internal GPU zones (**P10.3 ✅ v2.4.396**)
- `SituationCreateQueryPool` / user occlusion + timestamp queries — **separate from internal profile pool** (**P10.4 ✅ v2.4.397**)
- `SituationGetThreadPoolMetrics()` — job system stats (submitted, completed, queue depths, steal counters, lock contention)
- `SituationGetThreadPoolSnapshot()` — per-thread slot info (role, CPU affinity, NUMA node, active flag)

**Safety & Harmlessness Principles (non-negotiable for any implementation):**
- All new timing/profiling must be **zero-cost or near-zero when disabled** (compile-time or cheap runtime guard).
- Never introduce new waits, syscalls, I/O, or allocations on the hot path.
- Timing calls themselves must be extremely cheap and not perturb the very jitter we are measuring (use existing monotonic clock; avoid heavy atomics on every frame unless already present).
- Changes must not regress the recent render-thread stutter mitigations (3× in-flight, 1 ms timedwait, conditional screenshot capture, removal of per-frame debug spam).
- When render thread is enabled (now default for examples), any new data must be thread-safe via atomics or protected handoff.
- Prefer extending existing `metric_*` / `submit_timestamps` / `frame_time` rather than duplicating.
- All new public APIs must have clear "when off / not threaded / no GPU support" behavior.
- No changes that could cause new backpressure or present-mode side-effects.

**Dependency note:** Tracy is the right choice for the CPU zone side — it's MIT-licensed,
header-only on the client, low-overhead, and produces beautiful flame graphs. However it
requires a network connection to `tracy-profiler` or the standalone capture tool at runtime.
The integration is opt-in via `SITUATION_ENABLE_TRACY`. All zones must compile to zero-cost
no-ops when the define is absent — non-negotiable for shipping builds.

---

### 6.0 Lightweight Stutter & Spike Diagnostics (Immediate, Proactive Layer — Start Here)

This slice delivers objective spike detection and basic cause attribution *now*, using/enhancing only existing infrastructure. It is the safe on-ramp for the full Tracy work. It must be harmless (no measurable jitter, no new waits) and proactive (easy to see "yes there were spikes and why" in ex02 or any example).

**Goals for this slice (general library debugging assistance):**
- Detect frame_time spikes (the dt that affects animation, physics, timing) even when avg FPS looks good.
- Attribute spikes to causes: backpressure/cnd_wait, fence/clientwait, present/swap, user logic, render-thread work, etc.
- Provide easy-to-use APIs and overlays that any app can call (programmatic access, HUD overlay, histogram export).
- Support dump/logging for post-run analysis.
- Zero or near-zero cost when not actively observing (guarded, no hot-path impact).

**Concrete safe steps (do in this order; each must pass regression checks on representative workloads including examples run uncapped/vsync-off):**

- [x] **6.0.1** Surface & harden existing high-level tools (general use)
  - `SituationDrawMetricsOverlay` is easy to call from any app.
  - Extended overlay to show "Frame: X.XX ms (max Y.YY, spikes: N)" + phases line.
  - `ExportRenderHistogram` now populates real bins from `frame_time` + spikes + phases.
  - New dedicated getters added (`GetMaxFrameTime`, `GetFrameSpikeCount`, `GetLastFramePhases`).

- [x] **6.0.2** Add lightweight spike tracking (general, low cost)
  - Added to core: `max_frame_time`, `frame_spike_count`, `frame_time_hist[]`.
  - Updated in `SituationUpdateTimers`.
  - Cheap, always-on, no allocation/I/O.

- [x] **6.0.3** Cheap phase timing for cause attribution (general)
  - Instrumented key sections (backpressure waits, fence/clientwait, execute/replay, present/swap) using `_SitGetMonotonicTimeNS`.
  - Stored in `last_backpressure_ns`, `last_fence_wait_ns`, `last_execute_ns`, `last_present_ns`.
  - Exposed via `GetLastFramePhases()` and overlay/histogram.

- [x] **6.0.4** General dump / logging / programmatic access
  - Data available via overlay, `ExportRenderHistogram` (JSON with phases/hist), and new getters.
  - Any app can query at any time. (Dedicated `DumpFrameStats` can be added later if needed.)

**Exit criteria for lightweight slice:** [x] Any application (using public APIs) can run for minutes under load and programmatically or via HUD obtain "number of frame spikes, max duration, and dominant cause phase for recent outliers" with no subjective watching and no measurable regression in FPS, stutter rate, or CPU usage vs baseline.

---

### 6.2 Architecture Decision: What to Integrate (Full Path)

The Lightweight slice (6.0) must land first. The Tracy + GPU layers are independent of each other and of the lightweight layer.

| Layer | Tool | What it shows | Harmless when off? |
|-------|------|---------------|--------------------|
| **Lightweight** | Enhanced existing + `GetFrameProfile` | Spikes, phase breakdowns, frame-time histograms | Yes (cheap guards) |
| **CPU zones** | Tracy (opt-in) | Labeled regions on main/render/audio/IO/job threads | Yes (compile-time no-op) |
| **GPU timestamps** | `VkQueryPool` / `GL_TIME_ELAPSED` | Per-pass / present GPU time | Yes (device capability guard) |

CPU zones + lightweight are highest priority for stutter hunting. GPU timestamps are heavier and mostly for shader work.

---

### 6.3 Dependency and Compile Gate (Tracy)

- [x] **6.1a** Add `ext/tracy/` as an optional submodule or vendored copy of the Tracy client
      headers (`Tracy.hpp`, `TracyC.h`). The client is header-only; no compiled `.c` needed
      unless `TRACY_ENABLE` is defined. **Shipped @ v2.4.395** (`ext/tracy/public/` sparse clone).
- [x] **6.1b** Add `SITUATION_ENABLE_TRACY` to the compile-time define table in `situation_api.h`.
      When not defined: all zone macros expand to nothing at zero cost.
      When defined: pull in `TracyC.h` and emit instrumentation.
- [x] **6.1c** Add `-DSITUATION_ENABLE_TRACY -DTRACY_ENABLE` to an optional flag in
      `build_situation.bat`. Document in `doc/COMPILATION_GUIDE.md`.
- [x] **6.1d** Decide C vs C++ Tracy client. The C API (`TracyC.h`) avoids forcing C++ into
      the library's C11 build. Prefer it unless profiling throughput is measurably worse. **Using TracyC.h + `build/tracy_client.cpp` TU.**

---

### 6.4 CPU Zone Macro Layer (Tracy)

Define Situation-native macros that wrap Tracy (or expand to nothing). Keep Tracy's names
out of user-facing code entirely:

```c
// sit/situation_profiling.h  (included from situation.h, not situation_api.h)

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

- [x] **6.2a** Create `sit/situation_profiling.h` with the macro definitions above.
- [x] **6.2b** Include it from `situation.h` after `situation_api.h` (instrumentation, not API surface).
- [x] **6.2c** Expose `SIT_PROFILE_ZONE(name)` / `SIT_PROFILE_ZONE_END(name)` as the
      user-facing aliases in `situation_api.h` (so application code can also place zones
      without directly depending on Tracy).

---

### 6.5 Instrument Library Call Sites (CPU) (Tracy)

Priority order — instrument the hot path first, everything else can follow:

- [x] **6.3a** `SituationEndFrame` — frame boundary: `SIT_PROF_FRAME_MARK()` at the top.
- [x] **6.3b** `SituationAcquireFrameCommandBuffer` — zone backpressure + fence wait sections.
- [x] **6.3c** `_SituationRenderThreadEntry` — zone each frame slot execution (dequeue → submit → present).
- [x] **6.3d** `_SituationSubmitGraphics` / `_SituationSubmitCompute` — zone the `vkQueueSubmit` calls.
- [x] **6.3e** `SituationUpdateTimers` / `SituationPollInputEvents` — short zones; useful for
      detecting OS event stalls.
- [x] **6.3f** Job system: zone each worker job execution body in `_SituationWorkerEntry`.
- [x] **6.3g** Audio callback: zone the miniaudio device callback graph/mix tick.
- [x] **6.3h** Hot reload: zone `SituationPollShaderLoad` polling.

---

### 6.6 GPU Timestamps — Vulkan (can be deferred)

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

- [x] **6.4a** At VK init (`_SituationInitVulkan`): query `timestampPeriod` from
      `VkPhysicalDeviceLimits`. Check `limits.timestampComputeAndGraphics` — if false, GPU
      timestamps are unsupported on this device; set `gpu_timestamps_supported = false` and
      skip pool creation. Log a diagnostic. **Shipped @ v2.4.396 (P10.3).**
- [x] **6.4b** Create `VkQueryPool` with type `VK_QUERY_TYPE_TIMESTAMP`,
      `queryCount = 2 * SITUATION_MAX_FRAMES_IN_FLIGHT * SIT_PROF_GPU_MAX_ZONES` where
      `SIT_PROF_GPU_MAX_ZONES` defaults to 16 (overridable by define). Pool is created once,
      never recreated on swapchain resize. **Shipped @ v2.4.396.**
- [x] **6.4c** At start of each frame (in `SituationAcquireFrameCommandBuffer` after fence wait):
      call `vkCmdResetQueryPool` for this frame's query slot range (avoids `vkResetQueryPool`
      which requires Vulkan 1.2 — `vkCmdResetQueryPool` is 1.0). **Shipped @ v2.4.396.**
- [x] **6.4d** Add `SituationCmdGPUZoneBegin(cmd, zone_id)` and `SituationCmdGPUZoneEnd(cmd, zone_id)`:
      record `vkCmdWriteTimestamp` at begin/end query indices for `zone_id` (0..15).
      Independent of Tracy (`SITUATION_ENABLE_TRACY`); no-op / `-563` when unsupported.
      **VK user zones outside active render pass only** (`-565` in-pass). **Shipped @ v2.4.396.**
- [x] **6.4e** Readback: in `SituationAcquireFrameCommandBuffer`, after the fence wait confirms
      the prior frame slot is done, call `vkGetQueryPoolResults` with `VK_QUERY_RESULT_64_BIT`
      (non-blocking — no `WAIT_BIT`). Convert raw ticks to nanoseconds using `timestampPeriod`.
      Stores **elapsed** duration in `gpu_zone_ns[]` (absolute ticks discarded after delta).
      **Shipped @ v2.4.396.**

**Architecture (6.4f — Tracy GPU track, Vulkan):** Prefer **extending `_SitGpuProfReadbackVulkan`** when
`SITUATION_ENABLE_TRACY` is defined — **not** a second `TracyVkContext` query pool and **not** feeding
`gpu_zone_ns[]` alone. Tracy's GPU track needs **absolute** begin/end GPU times on a calibrated timeline;
P10.3 readback already has raw `t0`/`t1` timestamp ticks before the elapsed conversion — retain those
for Tracy emit. Official Tracy path (`TracyVkContext` / `TracyVkZone` / `TracyVkCollect` in
`ext/tracy/public/tracy/TracyVulkan.hpp`) is reference only; duplicating instrumentation would split
`SituationCmdGPUZoneBegin/End` from headless P10.3.

- [ ] **6.4f** After P10.3 VK readback (same fence hook), when `SITUATION_ENABLE_TRACY`:
      - [ ] **6.4f.1** At VK init: emit `___tracy_emit_gpu_new_context` (period = `timestampPeriod` ns/tick).
            Calibrate GPU↔CPU offset via `vkGetCalibratedTimestampsEXT` when
            `VK_EXT_calibrated_timestamps` is supported; emit `___tracy_emit_gpu_calibration` at init and
            periodically thereafter. Fall back to estimated offset if the extension is unavailable
            (same strategy as `TracyVkContextCalibrated`).
      - [ ] **6.4f.2** For each completed zone in the readback slot: map `zone_id` → Tracy `queryId`
            (begin = `zone_id * 2`, end = `zone_id * 2 + 1` within the frame slot, or equivalent stable
            mapping); emit `___tracy_emit_gpu_time` for raw `t0`/`t1` ticks, then
            `___tracy_emit_gpu_zone_begin` / `___tracy_emit_gpu_zone_end` with matching `context` +
            `srcloc` (C emit API in `ext/tracy/public/tracy/TracyC.h` — **not** `TracyCZone*`, which is
            CPU-only).
      - [ ] **6.4f.3** Static **zone name table** — `SituationGPUProfileZone` → Tracy srcloc strings
            (registered once via `___tracy_emit_zone_begin_alloc` / cached `srcloc` handles):

            | `zone_id` | Enum | Tracy label |
            |-----------|------|-------------|
            | 0 | `SITUATION_GPU_ZONE_COMPOSITE` | `"GPU VD Composite"` |
            | 1 | `SITUATION_GPU_ZONE_VD_PATH_A` | `"GPU VD Path A"` |
            | 2 | `SITUATION_GPU_ZONE_VD_PATH_B` | `"GPU VD Path B"` |
            | 3 | `SITUATION_GPU_ZONE_TEXT_BATCH` | `"GPU Text Batch"` |
            | 4–15 | `SITUATION_GPU_ZONE_USER_0` … `_USER_11` | `"GPU User 0"` … `"GPU User 11"` |

      - [ ] **6.4f.4** Guard: no-op when Tracy disabled; render-thread only; no new waits on hot path
            (emit after existing fence readback only). User zones recorded in-pass on VK remain unsupported
            for P10.3 (`-565`); Tracy feed inherits that constraint.

- [x] **6.4g** On swapchain recreation: do NOT recreate the query pool — it is not swapchain-scoped.
      Reset the per-frame slot counters only. **Shipped @ v2.4.396.**
- [x] **6.4h** On shutdown: `vkDestroyQueryPool` after the final fence wait. **Shipped @ v2.4.396.**

---

### 6.7 GPU Timestamps — OpenGL (can be deferred)

OpenGL P10.3 uses `GL_TIME_ELAPSED` for headless elapsed zones. Tracy GPU track integration (**6.5e**)
is a **separate concern** from Vulkan (**6.4f**): elapsed-only results cannot be aligned to Tracy's GPU
timeline without absolute timestamp queries.

- [x] **6.5a** Check `GL_ARB_timer_query` / `GL_EXT_disjoint_timer_query` support at GL init.
      Set `sit_render.gl.gpu_timestamps_supported` accordingly. **Shipped @ v2.4.396.**
- [x] **6.5b** Create a ring of `SIT_PROF_GPU_MAX_ZONES` `GLuint` query objects per frame
      slot using `glCreateQueries(GL_TIME_ELAPSED, ...)`. **Shipped @ v2.4.396.**
- [x] **6.5c** `SituationCmdGPUZoneBegin` / `SituationCmdGPUZoneEnd` record elapsed queries
      at begin/end points (soft-buffer replay on render thread). **Shipped @ v2.4.396.**
- [x] **6.5d** Readback: one frame later (after fence on render thread),
      call `glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns_value)` → `gpu_zone_ns[]`.
      **Shipped @ v2.4.396.**

**Architecture (6.5e — Tracy GPU track, OpenGL):** Extend **`_SitGpuProfReadbackOpenGL`** when
`SITUATION_ENABLE_TRACY` — same single-instrumentation rule as 6.4f. **`gpu_zone_ns[]` alone is
insufficient.** P10.3's `GL_TIME_ELAPSED` queries yield duration only; Tracy needs **absolute**
`GL_TIMESTAMP` samples (`glQueryCounter` / `GL_TIMESTAMP` per `TracyOpenGL.hpp`). Options:

1. **Tracy-build parallel queries** — keep `GL_TIME_ELAPSED` for headless P10.3; add per-zone
   `GL_TIMESTAMP` begin/end query objects used only when Tracy is enabled; read back absolute values
   in `_SitGpuProfReadbackOpenGL` and emit `___tracy_emit_gpu_*` (same zone name table as 6.4f.3).
2. **Replace GL query type on Tracy builds only** — higher churn; prefer (1).

Calibration: `TracyGpuContext` auto-calibration (`GL_TIMESTAMP` vs CPU sample) or manual
`___tracy_emit_gpu_calibration` — **not** merely subtracting elapsed from a CPU timestamp.
**Apple:** Tracy disables OpenGL GPU timestamps (`TRACY_OPENGL_DISABLE` in `TracyOpenGL.hpp`); 6.5e
is best-effort / skip on Apple GL.

- [ ] **6.5e** GL Tracy GPU track (independent checklist from 6.4f):
      - [ ] **6.5e.1** Add absolute `GL_TIMESTAMP` query path for Tracy builds (see options above).
      - [ ] **6.5e.2** After existing fence readback, emit `___tracy_emit_gpu_new_context`,
            `___tracy_emit_gpu_calibration`, `___tracy_emit_gpu_time`, `___tracy_emit_gpu_zone_begin/end`
            (or `TracyGpuContext` / `TracyGpuCollect` as reference — same zone name table as **6.4f.3**).
      - [ ] **6.5e.3** Keep headless `gpu_zone_ns[]` on `GL_TIME_ELAPSED` unchanged when Tracy is off.
      - [ ] **6.5e.4** Document Apple GL skip; no regression to P10.3 harness when Tracy disabled.

---

### 6.8 `SituationGetFrameProfile` — Headless / Non-Tracy Readback (Lightweight first)

This is the key proactive API for "we don't know if it stutters". Implement a version using only existing metrics + the new phase timers from 6.0 as soon as the lightweight slice is stable. Full GPU fields can come later.

For users who don't want Tracy but still want frame timing data programmatically:

- [x] **6.8a** (lightweight) Define practical phase data. We implemented direct getters instead of full struct for immediate usefulness:
      - `SituationGetLastFramePhases(backpressure, fence, execute, present)`
      - Plus `GetMaxFrameTime()` + `GetFrameSpikeCount()`
- [x] **6.8b** (lightweight) Implemented core via `SituationGetLastFramePhases` + spike helpers. Full `SituationFrameProfile` struct wraps this @ v2.4.394 (P10.1).
- [x] **6.8c** Lightweight fields (phases + spikes) are populated always-on via existing monotonic timer. `SituationGetFrameProfile` ships @ v2.4.394 (**P10.1**). Tracy CPU zones ship @ v2.4.395 (**P10.2**).

---

### 6.8d Public user query pools (P10.4 — shipped @ v2.4.397)

Separate from the **internal** P10.3 profile timestamp pool. Implemented in bolster Phase 10.4 — canonical API doc: **`doc/guide/profiling.md`**, **`doc/situation_command_reference.md`** §10.

- [x] **`SituationQueryPool`** handle + **`SituationQueryType`** (timestamp, occlusion)
- [x] **`SituationCreateQueryPool` / `DestroyQueryPool` / `GetQueryPoolResults`**
- [x] **`SituationCmdResetQueryPool`**, **`WriteTimestamp`**, **`BeginOcclusionQuery`**, **`EndOcclusionQuery`**
- [x] errno **`-566`…`-570`** (query pool; **`-566`** not-ready readback)
- [x] Harness **`query_pool`** 3/3 GL + VK

**Out of AAA §6 scope:** pipeline-statistics query type; Tracy export of user pool results.

### 6.9 Error codes

- [x] `SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED` (-563) — device doesn't support timestamp queries
- [x] `SITUATION_ERROR_PROFILING_ZONE_OVERFLOW` (-564) — `zone_id >= SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT`
- [x] `SITUATION_ERROR_PROFILING_ZONE_STATE` (-565) — begin/end mismatch; VK in-pass user zones
- [x] String entries in `SituationGetErrorString()` — **shipped @ v2.4.396**
- [x] Query pool errno **`-566`…`-570`** — **shipped @ v2.4.397** (P10.4; see **`renderer_bolster_plan.md`**)

---

### 6.10 Ordering summary

```
6.0  Lightweight Stutter Diagnostics (IMMEDIATE — start here for ex02 spikes)
     (enhance overlay/histogram, spike counters, cheap phase timers, surface in examples)
6.1  (old)  → now 6.3  Tracy dependency + compile gate
       └─ 6.4  Macro layer
            └─ 6.5  Instrument call sites (CPU)  ← Tracy minimum
6.6  GPU timestamps (Vulkan)          ← independent, deferrable
6.7  GPU timestamps (OpenGL)          ← parallel to 6.6
6.8  SituationGetFrameProfile + user QueryPool (P10.1 / P10.4)
6.9  Error codes
```

**Safe minimum shippable scope for stutter work:** [x] 6.0 (lightweight slice) + 6.8a/b (headless phases via direct getters). **Phase 10 bolster exit met @ v2.4.397.**

This gives:
- Objective spike detection (max + count + hist).
- Basic cause attribution (backpressure / fence / execute / present phases).
- Headless **`SituationGetFrameProfile`** + optional Tracy CPU zones.
- Optional user **`SituationQueryPool`** for custom GPU timing/occlusion.
- Clean foundation for Tracy GPU track feed (**6.4f** VK / **6.5e** GL — still open; separate paths).

GPU zone readback ships in P10.3 @ v2.4.396 (`gpu_zone_ns[]`). **P10.4 user query pools @ v2.4.397.**
Tracy GPU track (**6.4f** Vulkan emit-from-readback; **6.5e** GL absolute-timestamp path) remains follow-on polish, gated on `SITUATION_ENABLE_TRACY`.

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

### 7.6 Graveyard flush timing — current model shipped; timeline migration note only

The **per-frame graveyard + flush-after-GPU-completion** approach is **not outdated** — it remains the
core deferred-destruction model (see **`sit/situation_impl_renderer_core.h`** § Vulkan Graveyard). What
*is* outdated in the old one-paragraph §7.6 was treating **`SituationAcquireFrameCommandBuffer`** as the
only flush site and framing the whole section as a §7.5 footnote.

**Invariant (all backends):** `_SituationFlushGraveyard` / `_SitGLFlushGraveyard` may call real
`vkDestroy*` / `glDelete*` only after the GPU has finished using resources from **that frame slot's prior
submission cycle**. Never flush on the defer path itself.

#### Current flush sites (shipped)

| Path | GPU-done gate | Flush + follow-on |
|------|----------------|-------------------|
| **VK `AcquireFrame`** (canonical) | `_SituationVulkanWaitFencePumpWindow(in_flight_fences[slot])` | `_SitGpuProfReadbackFrame` → staging cursor reset → **`_SituationFlushGraveyard(slot)`** → **`_SitVkShaderCacheProcessEvictions`** (when enabled) → later **`vkResetFences`** before cmd recording |
| **GL render thread** (frame start) | `glClientWaitSync(frame_fences[slot])` | **`_SitFlushFrameResources(slot)`** → execute → present → new fence |
| **GL single-threaded `EndFrame`** | same fence wait at slot reuse | flush → optional program-cache eviction → execute |
| **Render thread tail** (both backends) | **none** (refcount bookkeeping) | when `frame_refcounts[slot]` hits 0 → **`_SitFlushFrameResources(slot)`** — on **VK**, the **safe** destroy pass is still the **next `AcquireFrame` fence wait** for that slot; tail flush must not become the primary gate |
| **Shutdown / swapchain recreate** | `vkDeviceWaitIdle` or drain all slots | flush all graveyards before teardown |

Helper: **`_SitFlushFrameResources(frame_index)`** in `situation_impl_renderer_lc.h` (GL + VK wrapper).

**Ordering dependency (shipped):** shader/program cache LRU eviction runs **after** graveyard flush for
the same slot (`_SitVkShaderCacheProcessEvictions` / `_SitGLProgramCacheProcessEvictions`) — evicted
bundles queue GPU objects into the graveyard assuming the fence for that slot has already passed.

**Resource coverage (beyond original plan):** VK graveyards hold buffers, images, pipelines, framebuffers,
render passes, descriptor sets (sets intentionally not individually freed at flush — bump-pool strategy).
GL graveyards hold meshes, buffers, textures, programs, FBOs, RBOs.

```c
/* VK AcquireFrame — primary safe flush (situation_impl_renderer_frame_cmd.h) */
_SituationVulkanWaitFencePumpWindow(..., in_flight_fences[current_frame_index]);
_SitGpuProfReadbackFrame((int)current_frame_index);
sit_render.vk.staging_buffers[current_frame_index].cursor = 0;
_SituationFlushGraveyard(current_frame_index);
/* shader cache eviction immediately after */
```

#### Timeline-semaphore migration (§7.5 follow-on — not started)

When §7.5 replaces the in-flight **fence wait** in `AcquireFrame` with a **graphics timeline wait**:

- [ ] **7.6a** Keep graveyard flush in the **same `AcquireFrame` location** — immediately after the
      GPU-done wait and before resetting/recording the slot. The `vkWaitSemaphores` in §7.5c already
      implies the graphics timeline has reached `graphics_frame_timeline_value[slot]`; no extra
      `vkGetSemaphoreCounterValue` check is required unless implementing a non-blocking poll path.
- [ ] **7.6b** Audit render-thread **`frame_refcounts` tail flush** on VK: ensure it does not become
      the primary destroy gate when fences are removed (should remain empty/no-op if §7.6a is correct).
- [ ] **7.6c** Preserve **shader-cache-after-graveyard** ordering and shutdown drain paths unchanged.

**Not graveyard scope:** P10.4 user `SituationQueryPool` lifecycle is separate from internal graveyards.

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

## 9. KFS — Kaizen Filing System

KFS (`sit/kfs/kfs/kfs.h`, include as `kfs/kfs.h` with `-I sit/kfs`) is the Situation library's dedicated, self-contained asset management
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
— callers use `kfs/kfs.h` directly (`#define KFS_IMPLEMENTATION` in one TU).

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

- [ ] **9.3a** Guard `kfs/kfs.h` inclusion in `situation_impl.h` behind
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

## Status Summary (v2.4.399)

| Section | Status |
|---------|--------|
| 1. Bindless / Universal Handles | ✅ Complete |
| 2. SSBO-First / Vertex Pulling | ✅ Complete |
| 3. GPU-Driven Indirect Draw | ✅ Complete (example pending) |
| 4. Render Graph | ❌ Not started |
| 5. Asset Baking | ❌ Not started |
| 6. Tracy / Profiling | ⚠️ **Phase 10 complete @ v2.4.397** (P10.0–P10.4); **open:** Tracy GPU track (**6.4f** VK readback emit, **6.5e** GL absolute timestamps) |
| 7. Timeline Semaphores | 📋 Scoped (see §7 detail — ~15 tasks, hybrid binary+timeline model) |
| 8. Async I/O | ✅ File I/O done; texture streaming pending |
| 9. Virtual Mounts / VFS | 📋 Scoped — KFS is the implementation; integration layer pending (see §9) |
| 10. Web / Emscripten | ❌ Not started |
| 11. Ecosystem Tooling | ❌ Not started |

**Application identity @ v2.4.399 (not a numbered § here):** Win32 defaults + overrides — **`doc/architecture.md`** § Application Identity, **`doc/plan/SIT_IDENTITY_PLAN.md`** (Phase I WI-0–WI-5 @ v2.4.400; WI-6+, LI-*, MA-* open).

**Not tracked in this plan (see `renderer_bolster_plan.md`):** VD bolster (VD-1…VD-5 @ v2.4.387), **`SituationRenderTarget`** (3c @ v2.4.393), Phase 14 behavior policy, **MSAA Phase 0 prep @ v2.4.398** (VD-4b attachments still v2.5-gated).
