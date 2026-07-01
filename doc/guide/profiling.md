# Profiling & Diagnostics

Situation provides layered frame telemetry (P10.0–P10.4). Full SDK walkthrough: **[situation_sdk.md](../situation_sdk.md) §3.8**.

**File layout @ v2.4.401:** runtime metrics API in `situation_api_graphics.h`; Tracy CPU zones in **`sit/situation_profiling.h`** via **`situation.h`**; Tracy link glue in **`build/tracy_client.cpp`**. See **[architecture.md](../architecture.md#profiling-instrumentation-layout-v24401)**.

## Metrics overlay

#### `SituationDrawMetricsOverlay`

Draws FPS, frame time, render queue depth, input-to-present latency, draw/triangle counts, and VRAM on screen.

```c
void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color);
```

## P10.0 — Spike detection & phase timers

#### `SituationGetMaxFrameTime`

Highest observed frame delta in seconds since last reset.

```c
double SituationGetMaxFrameTime(void);
```

#### `SituationGetFrameSpikeCount`

Count of frames exceeding the internal spike threshold.

```c
uint32_t SituationGetFrameSpikeCount(void);
```

#### `SituationGetLastFramePhases`

Last completed frame's render-thread phase breakdown in nanoseconds.

```c
void SituationGetLastFramePhases(
    uint64_t* backpressure_ns,
    uint64_t* fence_wait_ns,
    uint64_t* execute_ns,
    uint64_t* present_ns);
```

#### `SituationExportRenderHistogram`

Writes ASCII frame-time histogram buckets into a caller buffer.

```c
void SituationExportRenderHistogram(char* buf, size_t buf_size);
```

#### `SituationGetRenderLatencyStats`

Average and maximum input-to-present latency (nanoseconds). Requires render thread.

```c
void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);
```

#### `SituationGetRenderQueueDepth`

Current render-queue depth (backpressure indicator).

```c
size_t SituationGetRenderQueueDepth(void);
```

#### `SituationGetDrawCallCount`

Draw commands recorded this frame.

```c
uint32_t SituationGetDrawCallCount(void);
```

#### `SituationGetVRAMUsage`

Total GPU memory allocated (bytes).

```c
uint64_t SituationGetVRAMUsage(void);
```

## P10.1 — Structured frame profile

#### `SituationGetFrameProfile`

Non-allocating snapshot wrapping P10.0 getters plus poll/update phase ns and render latency. When **`SIT_FEATURE_GPU_TIMESTAMPS`** is supported, **`gpu_zone_ns[]`** is filled by P10.3 (readback one frame slot late).

```c
void SituationGetFrameProfile(SituationFrameProfile* out);
```

Type: `SituationFrameProfile` in `situation_api_types_gpu.h` (`SITUATION_FRAME_PROFILE_VERSION`).

#### `SituationResetFrameProfileStats`

Clears max frame time, spike count, and histogram buckets. Does not reset last-frame phase timers.

```c
void SituationResetFrameProfileStats(void);
```

**Harness:** `sit_test.exe --module frame_profile` (4 tests, GL + VK)

## P10.2 — Tracy CPU zones (macros)

Included from `situation.h` via `situation_profiling.h`. No-ops unless `SITUATION_ENABLE_TRACY` (build with `SIT_TRACY=1`).

- `SIT_PROFILE_ZONE_SCOPED(name) { ... }` — scoped block
- `SIT_PROFILE_ZONE_CTX(ctx, name)` / `SIT_PROFILE_ZONE_END_CTX(ctx)` / `SIT_PROFILE_RETURN_CTX(ctx, val)` — early return
- `SIT_PROFILE_FRAME_MARK()` — frame boundary

Build and attach instructions: **[COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md)** → Tracy profiling.

## P10.3 — GPU timestamp zones

#### `SituationCmdGPUZoneBegin` / `SituationCmdGPUZoneEnd`

Record GPU elapsed time into fixed zone slots when **`SituationIsFeatureSupported(SIT_FEATURE_GPU_TIMESTAMPS)`** is true. Results appear in **`SituationGetFrameProfile` → `gpu_zone_ns[]`**.

```c
SituationError SituationCmdGPUZoneBegin(SituationCommandBuffer cmd, uint32_t zone_id);
SituationError SituationCmdGPUZoneEnd(SituationCommandBuffer cmd, uint32_t zone_id);
```

Zone IDs: **`SituationGPUProfileZone`** — composite, VD path A/B, text batch (library-internal), plus **`SITUATION_GPU_ZONE_USER_0` … `_USER_11`**.

**OpenGL:** user zones may be recorded inside an active render pass (uses `GL_TIME_ELAPSED` queries).

**Vulkan:** user zones must be recorded **outside** an active render pass (`SITUATION_ERROR_PROFILING_ZONE_STATE` / `-565` if inside). Internal composite zones are recorded around the VD compositor loop.

**Errors:** `-563` unsupported, `-564` bad `zone_id`, `-565` begin/end mismatch or in-pass on VK.

```c
if (SituationIsFeatureSupported(SIT_FEATURE_GPU_TIMESTAMPS)) {
    SituationCmdGPUZoneBegin(cmd, SITUATION_GPU_ZONE_USER_0);
    SituationCmdClearColor(cmd, (ColorRGBA){64, 64, 64, 255});
    SituationCmdGPUZoneEnd(cmd, SITUATION_GPU_ZONE_USER_0);
}
```

Readback is **one frame slot late** (after the GPU fence). Tracy GPU track feed from these timestamps remains a follow-on.

## P10.4 — User query pools

#### `SituationCreateQueryPool` / `SituationDestroyQueryPool`

Create a user-owned pool (`SITUATION_QUERY_TYPE_TIMESTAMP` or `SITUATION_QUERY_TYPE_OCCLUSION`, up to **`SITUATION_MAX_QUERIES_PER_POOL`** queries).

```c
SituationError SituationCreateQueryPool(SituationQueryType type, uint32_t count, SituationQueryPool* out_pool);
void SituationDestroyQueryPool(SituationQueryPool* pool);
```

#### Command recording

```c
SituationError SituationCmdResetQueryPool(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t first_query, uint32_t query_count);
SituationError SituationCmdWriteTimestamp(SituationCommandBuffer cmd, uint32_t pipeline_stage, SituationQueryPool pool, uint32_t query_index);
SituationError SituationCmdBeginOcclusionQuery(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t query_index);
SituationError SituationCmdEndOcclusionQuery(SituationCommandBuffer cmd);
```

Occlusion begin/end must be inside an active render pass.

#### `SituationGetQueryPoolResults`

```c
SituationError SituationGetQueryPoolResults(SituationQueryPool pool, uint32_t first_query, uint32_t query_count,
    uint64_t* out_results, uint32_t flags);
```

Non-blocking returns **`SITUATION_ERROR_QUERY_RESULT_NOT_READY` (-566)**. Pass **`SITUATION_QUERY_RESULT_WAIT_BIT`** to block until ready. VK timestamp results are converted to nanoseconds.

**Harness:** `sit_test.exe --module query_pool` (3 tests, GL + VK)

## Debug builds

Root **`debug.bat`** (`-O0 -g`, GDB harness). Thread names: `SituationSetCurrentThreadName`. See SDK §3.8.6.
