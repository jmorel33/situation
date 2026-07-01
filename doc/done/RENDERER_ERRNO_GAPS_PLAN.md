# Renderer Pipeline Errno Gaps — Fix Plan

**Audit date:** 2026-06-17  
**Implemented:** v2.4.293 (2026-06-17)  
**Library version at audit:** v2.4.292  
**Scope:** Renderer pipeline changes introduced in v2.4.279–v2.4.292 (shader cache Phases 1–4,
Vulkan hot-reload in-place bundle swap, OpenGL program cache, descriptor parity work).  
**Reference:** `doc/plan/ERRNO_ADOPTION_PLAN.md` — standing practice and adopted codes.

---

## Background

During the 20+ patch run that implemented the Vulkan shader cache (Phases 1–4), the OpenGL
program cache (Phase 4), and the hot-reload bundle-swap path, several new failure modes were
introduced without corresponding errno codes. Some failures silently return NULL or fall through
to `SITUATION_ERROR_GENERAL`. This plan documents each gap and prescribes the exact fix.

No code changes here — this is a tracking document. **Status: implemented in v2.4.293** (see checklist below).

---

## New Error Codes Required

Two new codes were added to `sit/situation_base_errno.h`. Strings are served automatically via
`SITUATION_ERROR_TABLE` / `SituationErrorToString()` in `sit/situation_impl_ctrl.h` (no manual
string table entries required).

| Code name | Value | Message | Range |
|-----------|-------|---------|-------|
| `SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED` | `-758` | `"Vulkan: vkCreatePipelineCache failed during init"` | Vulkan (-700) |
| `SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL`     | `-759` | `"Vulkan: render pass cache full (32 max); likely leaking render passes"` | Vulkan (-700) |

Both fit cleanly in the gap after `-757` (`SITUATION_ERROR_SHADER_COMPILER_REQUIRED`).

---

## Gaps — Ordered by Severity

---

### GAP-1 — `SituationReloadShader` / `SituationReloadTexture` / `SituationReloadComputePipeline` fall through to `SITUATION_ERROR_GENERAL`

**Severity:** High  
**Introduced:** Pre-existing non-cache path; not caught until this audit.  
**Location:** `sit/situation_impl_renderer.h` — bottom of `SituationReloadShader` (non-cache `#else` branch), and the analogous fallthrough at the end of `SituationReloadTexture` and `SituationReloadComputePipeline`.

**Exact lines pattern:**
```c
// after successful SituationLoadShaderFromMemory but _SitGetShaderSlot(new_handle) returns NULL:
return SITUATION_ERROR_GENERAL;   // ← lines ~27667, ~27723, ~27784, ~27840
```

**What it means:** `SituationLoadShaderFromMemory` succeeded but the resulting handle resolved
to a NULL slot immediately afterward. This is an internal state corruption — the registry gave
out a handle for a slot that isn't accessible. `GENERAL` tells the caller nothing.

**Fix:**
```c
return _SituationSetErrorFromCode(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,
    "SituationReloadShader: new slot not accessible after successful load (registry defect)");
```
Apply the same change to the equivalent paths in `SituationReloadTexture` and
`SituationReloadComputePipeline`.

**Actions:**
- [x] `SituationReloadShader` non-cache `#else` branch fallthrough (~line 27667): `GENERAL` → `INTERNAL_STATE_CORRUPTED`; failed load returns underlying `err`
- [x] `SituationReloadTexture` fallthrough (~line 27723): same fix
- [x] `SituationReloadComputePipeline` fallthrough (~line 27840): same fix (1 occurrence; audit line refs were stale)

**New codes needed:** None. `SITUATION_ERROR_INTERNAL_STATE_CORRUPTED` (-9) already exists.

---

### GAP-2 — `vkCreatePipelineCache` failure uses EOL generic code `-700`

**Severity:** Medium  
**Introduced:** v2.4.286 (`_SituationInitVulkan` — `SIT_VK_SHADER_CACHE_PHASE2` block)  
**Location:** `sit/situation_impl_renderer.h` — `_SituationInitVulkan`, inside `#if SIT_VK_SHADER_CACHE_PHASE2`

**Current code:**
```c
if (vkCreatePipelineCache(sit_render.vk.device, &pci, NULL, &sit_render.vk.pipeline_cache) != VK_SUCCESS) {
    _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, "vkCreatePipelineCache failed.");
    _SituationCleanupVulkan();
```

**Problem:** `SITUATION_ERROR_VULKAN_INIT_FAILED` (-700) is EOL-tagged and is supposed to be
redirected away from. Using it here makes the pipeline cache init failure indistinguishable
from any other Vulkan init failure.

**Fix:** Use the new dedicated code:
```c
_SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED,
    "vkCreatePipelineCache failed during Vulkan init.");
```

**Actions:**
- [x] Add `SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED` (-758) to `sit/situation_base_errno.h`
- [x] Replace `SITUATION_ERROR_VULKAN_INIT_FAILED` with `-758` at the `vkCreatePipelineCache` failure site in `_SituationInitVulkan`

---

### GAP-3 — `_SitVkCreateDefaultSimplePipeline` and `_SitVkCreateBundlePipelineForVariant` return `VK_NULL_HANDLE` without setting errno

**Severity:** Medium  
**Introduced:** v2.4.280 (`_SitVkCreateDefaultSimplePipeline`), v2.4.286 (`_SitVkCreateBundlePipelineForVariant`)  
**Location:** `sit/situation_impl_renderer.h`

**Current code (`_SitVkCreateDefaultSimplePipeline`):**
```c
if (vkCreateGraphicsPipelines(...) != VK_SUCCESS) {
    return VK_NULL_HANDLE;   // ← no errno set
}
```

**Problem:** Caller `_SitVkShaderCacheAcquireBundle` receives `VK_NULL_HANDLE`, propagates
`NULL` bundle upward, and whatever public function called that (e.g. `SituationReloadShader`)
sets `SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED` with the message "bundle acquire
failed". The actual root cause — a `vkCreateGraphicsPipelines` failure inside the cache — is
invisible to any diagnostic path.

Same issue in `_SitVkCreateBundlePipelineForVariant` (Phase 2).

**Fix:** Add `_SituationSetErrorFromCode` before each `return VK_NULL_HANDLE`:
```c
// _SitVkCreateDefaultSimplePipeline:
if (vkCreateGraphicsPipelines(...) != VK_SUCCESS) {
    _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
        "Shader cache: vkCreateGraphicsPipelines for default bundle pipeline failed.");
    return VK_NULL_HANDLE;
}

// _SitVkCreateBundlePipelineForVariant:
if (vkCreateGraphicsPipelines(...) != VK_SUCCESS) {
    _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
        "Shader cache: vkCreateGraphicsPipelines for lazy bundle variant failed.");
    return VK_NULL_HANDLE;
}
```

**Actions:**
- [x] `_SitVkCreateDefaultSimplePipeline`: add `_SituationSetErrorFromCode(VULKAN_PIPELINE_CREATION_FAILED, ...)` before `return VK_NULL_HANDLE`
- [x] `_SitVkCreateBundlePipelineForVariant`: same fix

**New codes needed:** None. `SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED` (-747) exists.

---

### GAP-4 — `_SitVkShaderCacheAcquireBundle` silent NULL returns on layout failure and OOM

**Severity:** Medium  
**Introduced:** v2.4.280  
**Location:** `sit/situation_impl_renderer.h` — `_SitVkShaderCacheAcquireBundle`

**Current code (three unguarded `return NULL` paths):**
```c
// (a) vkCreatePipelineLayout failure:
if (vkCreatePipelineLayout(...) != VK_SUCCESS)
    return NULL;   // ← no errno

// (b) SIT_MALLOC for _SitVkPipelineBundle:
if (!b) {
    vkDestroyPipeline(...); vkDestroyPipelineLayout(...);
    return NULL;   // ← no errno
}

// (c) SIT_MALLOC for _SitVkShaderCacheEntry:
if (!entry) {
    vkDestroyPipeline(...); vkDestroyPipelineLayout(...); SIT_FREE(b);
    return NULL;   // ← no errno
}
```

**Note:** GAP-3 covers the `_SitVkCreateDefaultSimplePipeline` NULL already inlined here.

**Fix:**
```c
// (a):
if (vkCreatePipelineLayout(sit_render.vk.device, &pli, NULL, &pl) != VK_SUCCESS) {
    _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,
        "Shader cache: vkCreatePipelineLayout for bundle failed.");
    return NULL;
}

// (b):
if (!b) {
    vkDestroyPipeline(...); vkDestroyPipelineLayout(...);
    _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
        "Shader cache: failed to allocate _SitVkPipelineBundle.");
    return NULL;
}

// (c):
if (!entry) {
    vkDestroyPipeline(...); vkDestroyPipelineLayout(...); SIT_FREE(b);
    _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
        "Shader cache: failed to allocate _SitVkShaderCacheEntry.");
    return NULL;
}
```

**Actions:**
- [x] `_SitVkShaderCacheAcquireBundle` path (a): add `VULKAN_PIPELINE_CREATION_FAILED` on `vkCreatePipelineLayout` failure
- [x] `_SitVkShaderCacheAcquireBundle` path (b): add `MEMORY_ALLOCATION` on `SIT_MALLOC(bundle)` failure
- [x] `_SitVkShaderCacheAcquireBundle` path (c): add `MEMORY_ALLOCATION` on `SIT_MALLOC(entry)` failure

**New codes needed:** None. Both `-747` and `-8` exist.

---

### GAP-5 — `_SitVkShaderCacheAcquireModules` OOM on entry allocation has no errno

**Severity:** Low  
**Introduced:** v2.4.280  
**Location:** `sit/situation_impl_renderer.h` — `_SitVkShaderCacheAcquireModules`

**Current code:** The `SIT_MALLOC(sizeof(_SitVkModulePairEntry))` failure path returns `NULL`
without setting an error code. Callers (`SituationReloadShader`, `SituationLoadShaderFromMemory`)
do set `SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED` on NULL return, which is misleading — the
actual failure was an allocation, not a module creation.

**Fix:**
```c
_SitVkModulePairEntry* entry = (_SitVkModulePairEntry*)SIT_MALLOC(sizeof(_SitVkModulePairEntry));
if (!entry) {
    _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
        "Shader cache: failed to allocate _SitVkModulePairEntry.");
    return NULL;
}
```

**Actions:**
- [x] `_SitVkShaderCacheAcquireModules`: add `MEMORY_ALLOCATION` on `SIT_MALLOC(_SitVkModulePairEntry)` failure

**New codes needed:** None.

---

### GAP-6 — Render pass cache overflow conflates two distinct conditions under one code

**Severity:** Medium  
**Introduced:** Pre-existing, but the new shader cache (v2.4.280+) increases the number of
render passes the system can generate, making this overflow more reachable.  
**Location:** `sit/situation_impl_renderer.h` — `_SituationVulkanGetOrCreateRenderPass`, the
"WARNING: Render Pass Cache full" branch

**Current code:**
```c
fprintf(stderr, "WARNING: Render Pass Cache full! (32 max). Returning un-cached pass, likely leaking.\n");
_SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED,
    "Render Pass Cache full (32 max), returning un-cached pass");
```

**Problem:** `SITUATION_ERROR_VULKAN_RENDERPASS_FAILED` (-730) also covers `vkCreateRenderPass`
failing two lines earlier in the same function. The two conditions are meaningfully different:
one is a Vulkan API failure, the other is a resource exhaustion / leak in the cache. A caller
checking the error code cannot distinguish them.

**Fix:** Use the new dedicated code:
```c
_SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL,
    "Render pass cache full (32 max); un-cached render pass returned. Likely leaking.");
```

**Actions:**
- [x] Add `SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL` (-759) to `sit/situation_base_errno.h`
- [x] `_SituationVulkanGetOrCreateRenderPass` cache-full branch: replace `VULKAN_RENDERPASS_FAILED` with `-759`

---

### GAP-7 — Cache mutex `mtx_init` return value unchecked in both VK and GL caches

**Severity:** Medium  
**Introduced:** v2.4.279 (`_SitVkShaderCacheInit`), v2.4.291 (`_SitGLProgramCacheInit`)  
**Location:** `sit/situation_impl_renderer.h`

**Current code (both caches follow this pattern):**
```c
mtx_init(&c->mutex, mtx_plain);
c->mutex_initialized = true;  // ← set unconditionally; wrong if mtx_init failed
```

**Problem:** If `mtx_init` returns `thrd_error`, `mutex_initialized` is still `true`. Every
guard in the cache functions checks `mutex_initialized` before locking — the guard is now
meaningless and the subsequent `mtx_lock` on an uninitialized mutex is undefined behavior.
The existing thread pool code (the established pattern) correctly checks the return value.

**Fix (both `_SitVkShaderCacheInit` and `_SitGLProgramCacheInit`):**
```c
if (mtx_init(&c->mutex, mtx_plain) != thrd_success) {
    _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,
        "Shader cache: failed to initialize cache mutex.");
    return;  /* mutex_initialized stays false; all cache ops will no-op safely */
}
c->mutex_initialized = true;
```

**Actions:**
- [x] `_SitVkShaderCacheInit`: check `mtx_init` return; only set `mutex_initialized = true` on `thrd_success`
- [x] `_SitGLProgramCacheInit`: same fix

**New codes needed:** None. `SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED` (-84) exists.

---

## Implementation Checklist

### errno table additions (`sit/situation_base_errno.h`)

- [x] Add `SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED` at `-758` in `SITUATION_ERRORS_VULKAN`
- [x] Add `SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL` at `-759` in `SITUATION_ERRORS_VULKAN`

### Error strings (`SituationErrorToString()` via `SITUATION_ERROR_TABLE`)

- [x] `-758` and `-759` picked up automatically by X-macro table (no manual entries in `situation_impl_ctrl.h`)

### Call-site fixes (`sit/situation_impl_renderer.h`)

- [x] **GAP-1** — `SituationReloadShader` non-cache fallthrough: `GENERAL` → `INTERNAL_STATE_CORRUPTED`
- [x] **GAP-1** — `SituationReloadTexture` fallthrough: same fix
- [x] **GAP-1** — `SituationReloadComputePipeline` fallthrough: same fix
- [x] **GAP-2** — `_SituationInitVulkan` `vkCreatePipelineCache`: `VULKAN_INIT_FAILED` → `VULKAN_PIPELINE_CACHE_INIT_FAILED`
- [x] **GAP-3** — `_SitVkCreateDefaultSimplePipeline`: add `_SituationSetErrorFromCode` before `return VK_NULL_HANDLE`
- [x] **GAP-3** — `_SitVkCreateBundlePipelineForVariant`: same fix
- [x] **GAP-4** — `_SitVkShaderCacheAcquireBundle` `vkCreatePipelineLayout` fail: add errno
- [x] **GAP-4** — `_SitVkShaderCacheAcquireBundle` `SIT_MALLOC(bundle)` fail: add `MEMORY_ALLOCATION`
- [x] **GAP-4** — `_SitVkShaderCacheAcquireBundle` `SIT_MALLOC(entry)` fail: add `MEMORY_ALLOCATION`
- [x] **GAP-5** — `_SitVkShaderCacheAcquireModules` entry alloc fail: add `MEMORY_ALLOCATION`
- [x] **GAP-6** — `_SituationVulkanGetOrCreateRenderPass` cache-full branch: `VULKAN_RENDERPASS_FAILED` → `VULKAN_RENDER_PASS_CACHE_FULL`
- [x] **GAP-7** — `_SitVkShaderCacheInit`: check `mtx_init` return value
- [x] **GAP-7** — `_SitGLProgramCacheInit`: check `mtx_init` return value

### Version and log

- [x] Bump `SITUATION_VERSION_PATCH` in `sit/situation_base_version.h` → **293**
- [x] Add entry to `doc/UPDATELOG.md`

### ERRNO_ADOPTION_PLAN cross-reference

- [x] Add `-758` and `-759` to the Vulkan section of `ERRNO_ADOPTION_PLAN.md` (Phase 5)
- [x] Mark GAP-1 item in the Rendering Core section (Phase 3) of `ERRNO_ADOPTION_PLAN.md`

---

## Notes

- All GAPs are additive — no existing error codes are removed or renumbered.
- GAP-7 is the only one that changes control flow (early return from init). All others are
  purely `_SituationSetErrorFromCode` additions before an already-existing return.
- The render pass cache (GAP-6) currently has a hard cap of 32. If that cap is ever raised or
  made dynamic, the new error code is still the right one to use.
- **Known follow-up (not in scope of v2.4.293):** Public callers such as `SituationReloadShader`
  still overwrite the last error when a cache acquire returns NULL (e.g. OOM at GAP-5 surfaces as
  `VULKAN_SHADER_MODULE_FAILED` / `VULKAN_PIPELINE_CREATION_FAILED` at the public boundary).
  Fixing that requires caller-side propagation (`SituationGetLastErrorCode()` guard) — deferred.
- No test harness changes are strictly required for these fixes, but adding assertions on the
  new codes in `test_graphics.c` (e.g. a deliberately-triggered mutex-init failure mock or a
  cache-full scenario) would be good Phase 2 follow-up.
