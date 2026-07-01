# Internal Hardening Plan — `static void` → Propagated `SituationError`

**Date**: 2026-05-24  
**Status**: ✅ COMPLETE — Phases 0–15 shipped v2.4.127–v2.4.182. Two deferred items:
- **1.6.2** `_SituationEnsureRegistryInit` — deliberately kept `void by design`; `HARDENING:` comment in place at `situation_impl_audio.h:109`
- **Phase 14** (MyBuddy central allocator) — future work, not started; tracked separately in `sit/mybuddy/`
- **Phase 15** loop-stability verify — superseded by crash-recovery harness and stable tone_synth/MIDI module runs  
**Priority**: HIGH  
**Depends On**: X-Macro Errno ✅ · Public error propagation Ph 1–2 ✅ · Cmd bind `SituationError` (v2.4.126) ✅  
**Companion**: `doc/plan/ERROR_PROPAGATION_PLAN.md` (public API only)

---

## Version release policy (every phase)

**One completed hardening phase → one patch bump → one UPDATELOG entry.** No combining phases in a single version (Phase 0 and Phase 1 were split retroactively after the first session landed as a single bump).

| Step | File | Action |
|------|------|--------|
| 1 | `sit/situation_base_version.h` | Increment **`SITUATION_VERSION_PATCH`** by 1 |
| 2 | `sit/situation_base_version.h` | Set **`SITUATION_VERSION_DESCRIPTION`** to a short phase title (≤ ~40 chars) |
| 3 | `doc/UPDATELOG.md` | Prepend **`## [v2.4.NNN "Title"] - YYYY-MM-DD`** with Description, Library Changes, Test plan, Verification |
| 4 | This doc | Mark phase ✅ in dashboard; check phase gate boxes |

**Phase → patch map** (baseline before hardening: **v2.4.126**):

| Phase | Target version | Codename (description) | Status |
|-------|----------------|------------------------|--------|
| **0** | **v2.4.127** | Internal Hardening Tooling | ✅ Shipped |
| **1** | **v2.4.128** | Internal Init Error Propagation | ✅ Shipped |
| **2** | **v2.4.129** | Internal Vulkan Swapchain Errors | ✅ Shipped |
| **2.1** | **v2.4.130** | Errno Table Phase 2.1 | ✅ Shipped |
| **3** | **v2.4.131** | Internal GL Soft Buffer Errors | ✅ Shipped |
| **4** | **v2.4.132** | Internal Uniform Map Errors | ✅ Shipped |
| **5** | v2.4.133 | Internal Shader Async Errors | ✅ |
| **6** | v2.4.134 | Internal Render Thread Errors | ✅ |
| **7** | v2.4.135 | Internal Audio Errors | ✅ |
| **8** | v2.4.136 | Internal Hardening Stragglers | ✅ |
| **9** | v2.4.137 | Internal Void By Design Docs | ⬜ |
| **10** | v2.4.138 | Internal Caller Audit | ✅ Shipped |
| **11** | v2.4.178 | Vulkan 2D/draw hygiene internals | ✅ Shipped |
| **12** | v2.4.179 | Vulkan quad/text draw validation | ✅ Shipped |
| **13** | v2.4.180 | OpenGL quad/text draw validation | ✅ Shipped |
| **15** | v2.4.182 | Audio graph RT teardown | ✅ Shipped |

**Related releases (renderer bolster — not hardening phase slots):**

| Version | Title | Doc |
|---------|-------|-----|
| **v2.4.181** | OpenGL deferred execute pass fix | **`doc/UPDATELOG.md`**, **`doc/plan/renderer_bolster_plan.md`** Phase 7-terA |
| **v2.4.183** | OpenGL executor harness green | **`doc/UPDATELOG.md`**, **`doc/plan/renderer_bolster_plan.md`** Phase 7-ter |

Formula: **`patch = 127 + phase_number`** for hardening phases; errno **2.1** uses the next patch slot (130) between phase 2 and 3. Bolster-only patches (**181**, **183**) use the general library patch line and do not consume hardening phase numbers.

### Phase completion gate (run at end of every phase)

- [ ] All phase task boxes for this phase checked (or explicitly deferred with note)
- [ ] Harness slice for phase green (record command + pass count in UPDATELOG)
- [ ] **`sit/situation_base_version.h`** — PATCH +1, DESCRIPTION updated
- [ ] **`doc/UPDATELOG.md`** — new top entry for this phase only
- [ ] **This doc** — dashboard + master checklist updated

---

## Progress Dashboard

| Phase | Version | Description | Tasks | Done | Status |
|-------|---------|-------------|-------|------|--------|
| **0** | v2.4.127 | Tooling & baseline | 12 | 12 | ✅ Shipped |
| **1** | v2.4.128 | Init chain | 78 | 76 | ✅ Shipped *(1.2.1, 1.6.2 deferred)* |
| **2** | v2.4.129 | Vulkan swapchain/submit | 32 | 32 | ✅ Shipped |
| **2.1** | v2.4.130 | Errno table completeness | — | — | ✅ Shipped |
| **3** | v2.4.131 | GL soft buffer & momentum | 35 | 33 | ✅ Shipped |
| **4** | v2.4.132 | Uniform map & resource slots | 42 | 42 | ✅ Shipped |
| **5** | v2.4.133 | Shader async & hot reload | 38 | 38 | ✅ Shipped |
| **6** | v2.4.134 | Render thread & frame flush | 18 | 18 | ✅ Shipped |
| **7** | v2.4.135 | Audio internals | 24 | 24 | ✅ Shipped |
| **8** | v2.4.136 | Stragglers & bool cleanup | 16 | 16 | ✅ Shipped |
| **9** | v2.4.137 | Bucket B documentation | 68 | 68 | ✅ Shipped |
| **10** | v2.4.138 | Caller audit | 36 | 36 | ✅ Shipped |
| | **→ v2.4.138** | **TOTAL** | **~399** | **~195** | |

_Update the Done column as you check boxes. One function ≈ 4–6 sub-tasks._

---

## Legend

Each **convert** task follows this sub-checklist pattern:

- **FWD** — update `situation_impl_forward.h` or `situation_impl_renderer_fwd.h`
- **SIG** — change signature + body; every failure path `return _SituationSetErrorFromCode(...)`
- **CALL** — update every caller to check return (or `SIT_RETURN_IF_ERR`)
- **CODE** — pick correct `SituationError` from `situation_base_errno.h`
- **TEST** — relevant `sit_test.exe` slice green

Each **document** task (Bucket B):

- **DOC** — add `// HARDENING: void by design — <reason>` at forward decl
- **AUD** — spot-check callers for null/state assumptions

---

## Design Contract (quick ref)

| Rule | Summary |
|------|---------|
| **I1** | Can fail → `SituationError`, propagate up |
| **I2** | Callbacks / teardown / RT audio → stay `void` |
| **I3** | `bool` success/fail init → `SituationError` |
| **I4** | Pointer returns: NULL + error state |
| **I5** | Internal only — no public API break |

```c
#define SIT_RETURN_IF_ERR(expr) do { \
    SituationError _sit_err = (expr); \
    if (_sit_err != SITUATION_SUCCESS) return _sit_err; \
} while(0)
```

---

# Phase 0 — Tooling & Baseline

**Goal**: Frozen inventory, helper macro, smoke test. **~1 session.**

### Infrastructure

- [x] **0.1** Create `scripts/list_internal_voids.ps1` — ripgrep all `static void _\w+` in `sit/situation_impl*.h`, output CSV (file, line, name)
- [x] **0.2** Run script; paste output into `doc/plan/internal_void_inventory.csv` (git-tracked)
- [x] **0.3** Add `SIT_RETURN_IF_ERR` to `sit/situation_impl_decl.h` (or `situation_impl_etc.h`) behind `#ifndef SIT_RETURN_IF_ERR`
- [x] **0.4** Document macro in this plan + one-line comment at definition site

### Harness smoke test

- [x] **0.5** Error-propagation smoke test — shipped as **`init_double_init_error`** in **`tests/harness/test_core.c`** (not a separate file)
- [x] **0.6** Test case: second **`SituationInit`** → **`ALREADY_INITIALIZED`**
- [x] **0.7** Test case: **`SituationGetLastErrorCode()`** matches after init fail
- [x] **0.8** Test case: error string non-empty (via existing init error paths)
- [x] **0.9** Registered in core harness module list
- [x] **0.10** **`sit_test.exe --module core`** — pass count in UPDATELOG v2.4.127

### Phase 0 gate

- [x] **0.11** Inventory CSV matches grep count (~115 unique names)
- [x] **0.12** **`sit/situation_base_version.h`** → **v2.4.127** — `"Internal Hardening Tooling"`
- [x] **0.13** **`doc/UPDATELOG.md`** — Phase 0 entry only
- [x] **0.14** Dashboard phase 0 marked ✅

---

# Phase 1 — Init Chain Hardening ★

**Goal**: `SituationInit` failure propagates without side-channel-only errors. **~2–3 sessions.**

**Caller roots to update** (each gets CALL sub-task when any child changes):

- [x] **1.CALL.01** `_SituationInitOpenGL` — audit all sub-init calls use `SIT_RETURN_IF_ERR`
- [x] **1.CALL.02** `_SituationInitVulkan` — same
- [x] **1.CALL.03** `_SituationInitRenderer` — same
- [x] **1.CALL.04** `_SituationInitSubsystems` — same (audio pool, display cache)
- [x] **1.CALL.05** `_SituationInitWindow` — if display cache moves here

### 1.1 — OpenGL ring buffer & MDI (renderer)

- [x] **1.1.1** `_SituationInitGLRingBuffer` — `void` → `SituationError`
  - [x] FWD — `situation_impl_renderer_fwd.h`
  - [x] SIG — return on `glCreateBuffers` fail, map fail
  - [x] CALL — `_SituationInitOpenGL`
  - [x] CODE — `SITUATION_ERROR_OPENGL_GENERAL` / `MEMORY_ALLOCATION`
  - [x] TEST — `--module core` init/shutdown
- [x] **1.1.2** `_SituationInitGLMDIBuffer` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG
  - [x] CALL — `_SituationInitOpenGL`
  - [x] CODE — `SITUATION_ERROR_OPENGL_GENERAL`
  - [x] TEST
- [x] **1.1.3** `_SituationInitGLRingFences` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — `SIT_CALLOC` for fence array
  - [x] CALL — `_SituationInitOpenGL`
  - [x] CODE — `SITUATION_ERROR_MEMORY_ALLOCATION`
  - [x] TEST

### 1.2 — OpenGL virtual bindless (renderer)

- [x] **1.2.1** `_SituationVirtualBindlessInit` — **void by design** (in-memory slot table reset only; no failure paths) → Phase 9 DOC
  - [x] Decision recorded — not converted at v2.4.128
  - [x] DOC — `HARDENING:` comment at forward decl (Phase 9)

### 1.3 — Internal renderer inits (`bool` → `SituationError`)

- [x] **1.3.1** `_SituationInitDefaultFont` — `bool` → `SituationError`
  - [x] FWD
  - [x] SIG — atlas upload, stb failures
  - [x] CALL — `_SituationInitOpenGL`, `_SituationInitVulkan` (if shared)
  - [x] CODE — `SITUATION_ERROR_FONT_LOAD_FAILED`, `FONT_ATLAS_FULL`
  - [x] TEST — `--module graphics` text draw smoke
- [x] **1.3.2** `_SituationInitTextRenderer` — `bool` → `SituationError`
  - [x] FWD
  - [x] SIG — shader program, VAO/VBO create
  - [x] CALL — `_SituationInitOpenGL`, `_SituationVulkanInitInternalRenderers`
  - [x] CODE — `OPENGL_SHADER_*` / `VULKAN_PIPELINE_*`
  - [x] TEST
- [x] **1.3.3** `_SituationInitQuadRenderer` — `bool` → `SituationError`
  - [x] FWD
  - [x] SIG
  - [x] CALL — `_SituationInitOpenGL`, `_SituationInitRenderer`
  - [x] CODE
  - [x] TEST — quad draw smoke
- [x] **1.3.4** `_SituationInitGLVirtualDisplayRenderer` — `bool` → `SituationError`
  - [x] FWD
  - [x] SIG — VD shader + quad resources
  - [x] CALL — `_SituationInitOpenGL`
  - [x] CODE
  - [x] TEST — `--module graphics` VD if present

### 1.4 — Render thread & caps (renderer + decl)

- [x] **1.4.1** `_SituationInitRenderThread` — `bool` → `SituationError`
  - [x] FWD — `situation_impl_forward.h`, `situation_impl_decl.h`, `renderer_fwd.h`
  - [x] SIG — mutex/cv/thread create failures
  - [x] CALL — `_SituationInitRenderer`
  - [x] CODE — `THREAD_CREATION_FAILED`, `THREAD_MUTEX_INIT_FAILED`
  - [x] TEST — threaded build config if enabled
- [x] **1.4.2** `_SituationValidateRenderCaps` — `bool` → `SituationError`
  - [x] FWD
  - [x] SIG — unsupported caps → specific code not silent false
  - [x] CALL — `_SituationInitRenderer`
  - [x] CODE — `OPENGL_UNSUPPORTED`, `VULKAN_UNSUPPORTED`, `NOT_IMPLEMENTED`
  - [x] TEST

### 1.5 — Platform / WDM (wdm)

- [x] **1.5.1** `_SituationCachePhysicalDisplays` — `void` → `SituationError`
  - [x] FWD — add to `situation_impl_forward.h` or wdm-local forward block
  - [x] SIG — `glfwGetMonitors` fail, alloc fail
  - [x] CALL — init path that refreshes displays
  - [x] CODE — `DISPLAY_QUERY_FAILED`, `MEMORY_ALLOCATION`
  - [x] TEST — monitor query smoke

### 1.6 — Audio pool (audio)

- [x] **1.6.1** `_SitAudioInitPool` — `void` → `SituationError`
  - [x] FWD — `situation_impl_forward.h`
  - [x] SIG — `mtx_init` on pool_mutex
  - [x] CALL — `_SituationInitSubsystems` / audio init block
  - [x] CODE — `THREAD_MUTEX_INIT_FAILED`, `AUDIO_CONTEXT`
  - [x] TEST — `--module audio`
- [ ] **1.6.2** `_SituationEnsureRegistryInit` — `void` → `SituationError` **(deferred — still `void` at v2.4.131)**
  - [ ] FWD — audio header or forward
  - [ ] SIG — registry init failure paths
  - [ ] CALL — audio device / graph setup
  - [ ] CODE — `DEVICE_REGISTRY_NOT_INITIALIZED`
  - [ ] TEST — `--module audio`

### Phase 1 gate

- [x] **1.G.1** Grep: zero `_SituationSetErrorFromCode` inside remaining `void` init helpers in Phase 1 set
- [x] **1.G.2** Grep: zero unchecked `_SituationInitDefaultFont(` / `_SituationInitQuadRenderer(` bool assumptions
- [x] **1.G.3** Full `sit_test.exe --module core` + `--module graphics` smoke
- [x] **1.G.4** **`sit/situation_base_version.h`** → **v2.4.128** — `"Internal Init Error Propagation"`
- [x] **1.G.5** **`doc/UPDATELOG.md`** — Phase 1 entry only
- [x] **1.G.6** Dashboard phase 1 marked ✅

---

# Phase 2 — Vulkan Swapchain & Submit

**Goal**: Resize/recreate/submit failures reach API boundary. **~2 sessions.**

### 2.1 — Swapchain lifecycle

- [x] **2.1.1** `_SituationVulkanRecreateSwapchain` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — each recreate step propagates (cleanup → create → framebuffers → screen copy)
  - [x] CALL — framebuffer resize callback, main loop recreate request
  - [x] CODE — `VULKAN_SWAPCHAIN_*`, `VULKAN_FRAMEBUFFER_FAILED`
  - [x] TEST — window resize / maximize
- [x] **2.1.2** `_SituationVulkanCleanupSwapchain` — `void` → `SituationError` (or `SituationError` only when called from recreate failure path)
  - [x] FWD
  - [x] SIG — distinguish shutdown (void ok) vs recreate (must propagate) — document choice
  - [x] CALL — `_SituationVulkanRecreateSwapchain`, `_SituationCleanupVulkan`
  - [x] CODE
  - [x] TEST

### 2.2 — Command buffer submit

- [x] **2.2.1** `_SituationVulkanEndSingleTimeCommands` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — submit, fence wait failures
  - [x] CALL — all single-time upload paths
  - [x] CODE — `VULKAN_QUEUE_SUBMIT_FAILED`, `VULKAN_COMMAND_FAILED`
  - [x] TEST — texture upload / buffer upload
- [x] **2.2.2** `_SituationSubmitCompute` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — queue submit, semaphore
  - [x] CALL — compute dispatch end-of-frame
  - [x] CODE — `VULKAN_QUEUE_SUBMIT_FAILED`, `COMPUTE_DISPATCH_FAILED`
  - [x] TEST — compute harness if present

### 2.3 — Already `SituationError` — caller verify (Phase 2 scope)

- [x] **2.3.1** `_SituationVulkanEnsureScreenshotResources` — verify all callers check return
- [x] **2.3.2** `_SituationVulkanCreateScreenCopyResource` — verify callers on recreate path
- [x] **2.3.3** `_SituationVulkanCreateSwapchain` — verify `_SituationVulkanRecreateSwapchain` propagates

### Phase 2 gate

- [x] **2.G.1** Deliberate swapchain stress (resize loop 10×) — no silent continue
- [x] **2.G.2** **`sit/situation_base_version.h`** → **v2.4.129** — `"Internal Vulkan Swapchain Errors"`
- [x] **2.G.3** **`doc/UPDATELOG.md`** — Phase 2 entry only
- [x] **2.G.4** Dashboard phase 2 marked ✅

---

# Phase 2.1 — Errno Table Completeness

**Goal**: Close errno audit gaps; document legacy duplicates. **Shipped between Phase 2 and Phase 3** (uses patch slot **130**, not a numbered hardening phase).

- [x] **2.1.E.1** New codes: `MEMORY_ACCESS`, `FILE_MODIFIED`, `BACKEND_SPECIFIC`, `VULKAN_COMMAND_BUFFER_FAILED`
- [x] **2.1.E.2** **`#define` aliases** after enum (not duplicate X-macro switch cases)
- [x] **2.1.E.3** **`EOL:`** comments on legacy duplicate pairs
- [x] **2.1.E.4** **`scripts/audit_errno.ps1`**
- [x] **2.1.E.5** Harness **`errno_table_phase_2_1`** in **`test_core.c`**

### Phase 2.1 gate

- [x] **2.1.G.1** `audit_errno.ps1` — no phantom names
- [x] **2.1.G.2** **`sit/situation_base_version.h`** → **v2.4.130** — `"Errno Table Phase 2.1"`
- [x] **2.1.G.3** **`doc/UPDATELOG.md`** — Phase 2.1 entry only
- [x] **2.1.G.4** Dashboard phase 2.1 marked ✅

---

# Phase 3 — OpenGL Soft Command Buffer & Momentum

**Goal**: Record-path OOM and queue-full surface at `SituationCmd*`. **~2–3 sessions.**

### 3.1 — Soft buffer allocation

- [x] **3.1.1** `_SitGLSoftCmdPush` — return `SituationError` + `SitCommandPacket** out_packet`
  - [x] FWD
  - [x] SIG — realloc fail → return err, set `buf->is_broken`; optional **`SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS`**
  - [x] CALL — all OpenGL **`SituationCmd*`** record paths via **`SIT_GL_SOFT_CMD_PUSH`**
  - [x] CODE — `COMMAND_BUFFER_FULL`, `MEMORY_ALLOCATION`
  - [ ] TEST — dedicated OOM/stress harness (debug cap manual only)
- [x] **3.1.2** `_SitGLSoftDataPush` — `SituationError` + `void** out_ptr`
  - [x] FWD
  - [x] SIG
  - [x] CALL — text draw, set-uniform blob, etc. via **`SIT_GL_SOFT_DATA_PUSH`**
  - [x] CODE
  - [ ] TEST

### 3.2 — Execute & momentum queue

- [x] **3.2.1** `_SituationGLExecuteCommands` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — broken buffer, per-packet `glGetError`
  - [x] CALL — **`SituationEndFrame`** (non-threaded + threaded paths); render thread logs failure
  - [x] CODE — `RENDER_COMMAND_FAILED`, `OPENGL_GENERAL`
  - [ ] TEST — record formal **`graphics`** pass count in UPDATELOG
- [x] **3.2.2** `_SituationReplayToQueue` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — incomplete list, Vulkan alloc fail
  - [x] CALL — no external callers yet (internal API ready)
  - [x] CODE — `RENDER_LIST_INCOMPLETE`, `VULKAN_COMMAND_FAILED`
  - [ ] TEST — render list harness when wired
- [x] **3.2.3** `_SituationEnqueueRenderList` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — queue full → **`THREAD_QUEUE_FULL`**
  - [x] CALL — **`SituationSubmitRenderList`**, **`_SituationRenderJobWorker`** (void APIs; error via side channel)
  - [x] CODE
  - [ ] TEST

### 3.3 — GL error helper (optional)

- [x] **3.3.1** `_SituationCheckGLError` — **stay `void`** (side-channel via `_SituationSetErrorFromCode` only)
  - [x] Decision: execute/replay path uses explicit `glGetError` → `SituationError`; macro `SIT_CHECK_GL_ERROR` remains logging-only at record sites
  - [ ] If convert later: FWD, SIG, CALL audit, TEST (deferred)

### Phase 3 gate

- [x] **3.G.1** Debug OOM cap **`SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS`** in **`_SitGLSoftCmdPush`** (dedicated harness test deferred)
- [x] **3.G.2** **`sit/situation_base_version.h`** → **v2.4.131** — `"Internal GL Soft Buffer Errors"`
- [x] **3.G.3** **`doc/UPDATELOG.md`** — Phase 3 entry only
- [x] **3.G.4** Dashboard phase 3 marked ✅

---

# Phase 4 — Uniform Map & Resource Slots

**Goal**: Alloc failures in shader uniform cache and slot lifecycle reported. **~1–2 sessions.**

### 4.1 — Uniform hash map

- [x] **4.1.1** `_sit_uniform_map_resize` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — alloc failure returns error; map unchanged on failure
  - [x] CALL — `_sit_uniform_map_set`
  - [x] CODE — `MEMORY_ALLOCATION`
  - [x] TEST — graphics module pass
- [x] **4.1.2** `_sit_uniform_map_set` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — entry alloc, key strdup
  - [x] CALL — populate, pending link, `SituationSetShaderUniform*`
  - [x] CODE
  - [x] TEST
- [x] **4.1.3** `_sit_uniform_map_create` — NULL + error (Rule I4)
  - [x] SIG — `_SituationSetErrorFromCode` on struct/bucket alloc fail
  - [x] CALL — shader load / lazy uniform paths check NULL
  - [x] TEST

### 4.2 — Slot free helpers (warn-only vs hard fail — pick one policy)

- [x] **4.2.1** `_SitFreeShaderSlot` — **stay `void`** (idempotent; invalid handle ignored)
- [x] **4.2.2** `_SitFreeComputePipelineSlot`
- [x] **4.2.3** `_SitFreeMeshSlot`
- [x] **4.2.4** `_SitFreeBufferSlot`
- [x] **4.2.5** `_SitFreeModelSlot`

Policy: **`HARDENING: void by design`** at each free helper; no `SituationError` (double-free safe).

### 4.3 — Slot alloc helpers (audit)

- [x] **4.3.1** `_SitAllocShaderSlot` — NULL + **`RESOURCE_INVALID`** message; callers use **`SituationGetLastErrorCode()`**
- [x] **4.3.2** `_SitAllocComputePipelineSlot`
- [x] **4.3.3** `_SitAllocMeshSlot`
- [x] **4.3.4** `_SitAllocBufferSlot`
- [x] **4.3.5** `_SitAllocModelSlot`

### Phase 4 gate

- [x] **4.G.1** OpenGL harness **`core` 31/31**, **`graphics` 107/107**
- [x] **4.G.2** **`sit/situation_base_version.h`** → **v2.4.132** — `"Internal Uniform Map Errors"`
- [x] **4.G.3** **`doc/UPDATELOG.md`** — Phase 4 entry only
- [x] **4.G.4** Dashboard phase 4 marked ✅

---

# Phase 5 — Shader Async, SPIR-V Poll & Hot Reload

**Goal**: Terminal async load failures visible without reading globals. **~2 sessions.**

### 5.1 — OpenGL async shader path

- [x] **5.1.1** `_SituationGLAsyncLoadFail` — `void` → `SituationError` (return terminal code set on slot)
  - [x] FWD
  - [x] SIG
  - [x] CALL — poll functions, worker
  - [x] CODE — existing SPIR-V / link codes
  - [x] TEST — `--filter spirv`
- [x] **5.1.2** `_SituationPollGLAsyncShaderLoad` — return `SituationError` (in-progress vs failed vs success)
  - [x] FWD
  - [x] SIG — map stages to `SHADER_LOAD_IN_PROGRESS` vs terminal
  - [x] CALL — public poll APIs
  - [x] TEST
- [x] **5.1.3** `_SituationPollGLAsyncSpirvShaderLoad`
  - [x] FWD · SIG · CALL · TEST
- [x] **5.1.4** `_SituationPollGLPendingProgramLink`
  - [x] FWD · SIG · CALL · TEST
- [x] **5.1.5** `_SituationSetGLErrorFromSpirvStage` — return `SituationError` (dropped out_param)
  - [x] Decision · FWD · SIG · CALL · TEST

### 5.2 — Vulkan async shader path

- [x] **5.2.1** `_SituationPollVkAsyncShaderLoad` — `void` → `SituationError`
  - [x] FWD · SIG · CALL · TEST
- [x] **5.2.2** `_SituationVulkanFreeAsyncShaderLoad` — stay void (cleanup) + DOC in Phase 9

### 5.3 — Hot reload pass

- [x] **5.3.1** `_SituationPerformHotReloadPass` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — first reload failure aggregate
  - [x] CALL — frame update / IO watcher
  - [x] CODE — `SHADER_*`, texture/audio load errors
  - [x] TEST — harness slice (core/graphics/spirv)

### Phase 5 gate

- [x] **5.G.1** SPIR-V invalid blob → deterministic code at poll boundary
- [x] **5.G.2** **`sit/situation_base_version.h`** → **v2.4.133** — `"Internal Shader Async Errors"`
- [x] **5.G.3** **`doc/UPDATELOG.md`** — Phase 5 entry only
- [x] **5.G.4** Dashboard phase 5 marked ✅

---

# Phase 6 — Render Thread & Frame Flush

**Goal**: Thread join timeout and frame resource flush propagate. **~1–2 sessions.**

- [x] **6.1** `_SituationDestroyRenderThread` — `void` → `SituationError`
  - [x] FWD — forward.h, decl.h, renderer
  - [x] SIG — join timeout already sets error; return it
  - [x] CALL — shutdown, cleanup renderer
  - [x] CODE — `RENDER_BACKPRESSURE_TIMEOUT`, `THREAD_JOIN_FAILED`
  - [x] TEST — shutdown with render thread enabled
- [x] **6.2** `_SitFlushFrameResources` — `void` → `SituationError`
  - [x] FWD
  - [x] SIG — graveyard flush, fence failures (GL wait in render thread loop)
  - [x] CALL — end frame, render thread loop
  - [x] CODE
  - [x] TEST
- [x] **6.3** `_SituationRenderJobWorker` — **stay void** (thread pool ABI) but:
  - [x] AUD — job failures call `_SituationSetErrorFromCode`
  - [x] DOC — `HARDENING:` comment (Phase 9 bucket)

### Phase 6 gate

- [x] **6.G.1** Shutdown timeout test documented
- [x] **6.G.2** **`sit/situation_base_version.h`** → **v2.4.134** — `"Internal Render Thread Errors"`
- [x] **6.G.3** **`doc/UPDATELOG.md`** — Phase 6 entry only
- [x] **6.G.4** Dashboard phase 6 marked ✅

---

# Phase 7 — Audio Internals

**Goal**: Non-RT audio paths report errors; RT callbacks unchanged. **~1–2 sessions.**

### 7.1 — Slot & effects init

- [x] **7.1.1** `_SitFreeSoundSlot` — `void` → `SituationError` on invalid handle
  - [x] FWD · SIG · CALL · CODE · TEST
- [x] **7.1.2** `_SituationInitReverb` — `SituationError` + `void** out`
  - [x] FWD — `situation_impl_forward.h`
  - [x] SIG — OOM on every buffer + rollback
  - [x] CALL — `_SituationInitSoundEffects`, load paths, device wrapper
  - [x] CODE — `MEMORY_ALLOCATION`
  - [x] TEST

### 7.2 — Async audio worker

- [x] **7.2.1** `_SituationAsyncAudioWorker` — stay void (worker ABI)
  - [x] AUD — load fail clears handle; error via `SituationGetLastErrorCode`
  - [x] DOC — `HARDENING:` comment

### 7.3 — RT paths — DO NOT CONVERT (document only in Phase 9)

- [x] **7.3.0** Confirmed no signature change + `HARDENING:` on RT callbacks

### Phase 7 gate

- [x] **7.G.1** `--module audio` full pass
- [x] **7.G.2** **`sit/situation_base_version.h`** → **v2.4.135** — `"Internal Audio Errors"`
- [x] **7.G.3** **`doc/UPDATELOG.md`** — Phase 7 entry only
- [x] **7.G.4** Dashboard phase 7 marked ✅

---

# Phase 8 — Stragglers & Remaining `bool`

**Goal**: Last success/fail bools normalized. **~1 session.**

- [x] **8.1** `_SituationExtractGLTFPrimitive` — `bool` → `SituationError`
  - [x] FWD · SIG · CALL · CODE (`ASSET_PARSE_FAILED`) · TEST — model load
- [x] **8.2** `_SituationSaveImageBMP` — `bool` → `SituationError` (internal + public wrapper)
  - [x] FWD · SIG · CALL · CODE (`FILE_WRITE_FAILED`) · TEST
- [x] **8.3** `_sit_directory_exists` — **keep bool** (query, not failure) + DOC
- [x] **8.4** `_SituationGraphHasMixerNode` — **keep bool** + DOC
- [x] **8.5** `_SituationShouldMixLatentVoices` — **keep bool** + DOC
- [x] **8.6** `_SituationDetectCycle` — **keep bool** + DOC (true = cycle detected)
- [x] **8.7** `_SituationVulkanResolveBufferDescriptor` — **keep bool** (resolver) + verify `out_err` always set on false
- [x] **8.8** `_SituationVulkanImmediateDestroyDuringShutdown` — **keep bool** + DOC

### Phase 8 gate

- [x] **8.G.1** No init-style `bool` helpers remain except documented queries
- [x] **8.G.2** **`sit/situation_base_version.h`** → **v2.4.136** — `"Internal Hardening Stragglers"`
- [x] **8.G.3** **`doc/UPDATELOG.md`** — Phase 8 entry only
- [x] **8.G.4** Dashboard phase 8 marked ✅

---

# Phase 9 — Bucket B: Document `void` by Design

**Goal**: Every intentional `void` tagged at forward decl; callers spot-checked. **~1 session.**

### 9.1 — Error & lifecycle (ctrl + forward)

- [x] **9.1.1** `_SituationSetError` — DOC: error sink
- [x] **9.1.2** `_SituationFullCleanupOnError` — DOC: best-effort teardown
- [x] **9.1.3** `_SituationCleanupSubsystems` — DOC
- [x] **9.1.4** `_SituationCleanupRenderer` — DOC
- [x] **9.1.5** `_SituationCleanupPlatform` — DOC
- [x] **9.1.6** `_SituationGLFWErrorCallback` — DOC: GLFW ABI

### 9.2 — Input callbacks (input + forward)

- [x] **9.2.1** `_SituationGLFWFileDropCallback`
- [x] **9.2.2** `_SituationGLFWWindowFocusCallback`
- [x] **9.2.3** `_SituationGLFWWindowMaximizeCallback`
- [x] **9.2.4** `_SituationGLFWWindowIconifyCallback`
- [x] **9.2.5** `_SituationGLFWFramebufferSizeCallback`
- [x] **9.2.6** `_SituationGLFWKeyCallback`
- [x] **9.2.7** `_SituationGLFWCharCallback`
- [x] **9.2.8** `_SituationGLFWMouseButtonCallback`
- [x] **9.2.9** `_SituationGLFWCursorPosCallback`
- [x] **9.2.10** `_SituationGLFWScrollCallback`
- [x] **9.2.11** `_SituationGLFWJoystickCallback`

### 9.3 — Audio RT & cleanup (audio + forward)

- [x] **9.3.1** `sit_miniaudio_data_callback` — DOC: RT ABI
- [x] **9.3.2** `_sit_miniaudio_capture_callback`
- [x] **9.3.3** `_SituationMixToneToBuffer`
- [x] **9.3.4** `_SituationMixLoadedVoicesFromSnapshot`
- [x] **9.3.5** `_SituationPublishMasterBusLevels`
- [x] **9.3.6** `_SituationProcessReverb`
- [x] **9.3.7** `_SituationUninitReverb`
- [x] **9.3.8** `_SitAudioCleanupPool`
- [x] **9.3.9** `_SituationWaitUntilVoiceSnapshotIdle`

### 9.4 — IO & thread workers

- [x] **9.4.1** `_SituationAsyncFileLoadWorker`
- [x] **9.4.2** `_SituationAsyncFileTextLoadWorker`
- [x] **9.4.3** `_SituationAsyncFileTextSaveWorker`
- [x] **9.4.4** `_SituationAsyncFileSaveWorker`
- [x] **9.4.5** `_SituationWorkerEntry`
- [x] **9.4.6** `_SituationIOThreadEntry`
- [x] **9.4.7** `_SituationRenderThreadEntry`
- [x] **9.4.8** `_SitParallelWorker`
- [x] **9.4.9** `_SituationRenderJobWorker`
- [x] **9.4.10** `_SituationVkAsyncCompileWorker`

### 9.5 — Renderer cleanup & graveyard (VK)

- [x] **9.5.1** `_SituationCleanupOpenGL`
- [x] **9.5.2** `_SituationCleanupVulkan`
- [x] **9.5.3** `_SituationCleanupQuadRenderer`
- [x] **9.5.4** `_SituationCleanupDanglingResources`
- [x] **9.5.5** `_SituationCleanupStagingBuffers`
- [x] **9.5.6** `_SituationInitGraveyard`
- [x] **9.5.7** `_SituationCleanupGraveyard`
- [x] **9.5.8** `_SituationFlushGraveyard`
- [x] **9.5.9** `_SituationDeferDestroyBuffer`
- [x] **9.5.10** `_SituationDeferDestroyImage`
- [x] **9.5.11** `_SituationDeferDestroyDescriptorSet`
- [x] **9.5.12** `_SituationDeferDestroyPipeline`
- [x] **9.5.13** `_SituationDeferDestroyFramebuffer`
- [x] **9.5.14** `_SituationDeferDestroyRenderPass`
- [x] **9.5.15** `_SituationVulkanDestroyImage`
- [x] **9.5.16** `_SituationVulkanDestroyBuffer`
- [x] **9.5.17** `_SituationVulkanDestroyScreenCopyResource`
- [x] **9.5.18** `_SituationVulkanDestroyScreenshotResources`
- [x] **9.5.19** `_SituationVulkanFreeBufferDescriptorSet`
- [x] **9.5.20** `_SituationVulkanFreeAsyncShaderLoad`
- [x] **9.5.21** `_SituationFreeSpirvBlob`
- [x] **9.5.22** `_SituationShaderIncluderRelease`

### 9.6 — Renderer cleanup & graveyard (GL)

- [x] **9.6.1** `_SitGLDeferDestroyBuffer`
- [x] **9.6.2** `_SitGLDeferDestroyTexture`
- [x] **9.6.3** `_SitGLDeferCleanMeshVAO`
- [x] **9.6.4** `_SitGLFlushGraveyard`
- [x] **9.6.5** `_SituationGLFreeSpirvAsyncCopies`
- [x] **9.6.6** `_sit_uniform_map_destroy`

### 9.7 — GL state & presentation

- [x] **9.7.1** `_SitGLBackupState`
- [x] **9.7.2** `_SitGLRestoreState`
- [x] **9.7.3** `_SitGLInvalidateShadowState`
- [x] **9.7.4** `_SitGLEnsureDefaultFramebufferOpaqueAlpha`
- [x] **9.7.5** `_SituationGLRingWait`
- [x] **9.7.6** `_SituationMakeGLContextCurrentForHostThread`
- [x] **9.7.7** `_SituationReleaseHostGLContextForRenderThread`

### 9.8 — VK record-only helpers

- [x] **9.8.1** `_SituationVulkanTransitionImageLayout`
- [x] **9.8.2** `_SituationVulkanCopyBufferToImage`
- [x] **9.8.3** `_SituationVulkanGenerateMipmaps`
- [x] **9.8.4** `_SituationVulkanRecordScreenshotCopy`
- [x] **9.8.5** `_SituationVulkanResolveScreenshotAfterSubmit`
- [x] **9.8.6** `_SituationVulkanCopyMappedColorToRGBA`
- [x] **9.8.7** `_SituationVulkanQuerySwapchainSupport`
- [x] **9.8.8** `_SituationVulkanFreeSwapchainSupportDetails`

### 9.9 — Populate / bind (infallible best-effort)

- [x] **9.9.1** `_SituationPopulateGLShaderUniformMap`
- [x] **9.9.2** `_SituationBindGLProgramUniformBlocks`
- [x] **9.9.3** `_SituationBindGLProgramStorageBlocks`

### 9.10 — Debug & assert (decl)

- [x] **9.10.1** `_SituationAssertMainThread`
- [x] **9.10.2** `_SituationVulkanWaitInFlightFencesPump`
- [x] **9.10.3** `_SituationVulkanShutdownWaitGpuPump`

### Phase 9 gate

- [x] **9.G.1** `rg "HARDENING: void by design" sit/situation_impl*.h` count ≥ 68
- [x] **9.G.2** `rg "static void _Sit" sit/situation_impl*.h` — every hit has HARDENING comment or is listed in Phase 1–8 convert queue
- [x] **9.G.3** **`sit/situation_base_version.h`** → **v2.4.137** — `"Internal Void By Design Docs"`
- [x] **9.G.4** **`doc/UPDATELOG.md`** — Phase 9 entry only
- [x] **9.G.5** Dashboard phase 9 marked ✅

---

# Phase 10 — Caller Audit (Already `SituationError`)

**Goal**: Functions that already return `SituationError` but callers may ignore it. **~1 session, can overlap Phases 1–2.**

### 10.1 — Init tree (ctrl)

- [x] **10.1.1** `_SituationInitPlatform` — all callers check
- [x] **10.1.2** `_SituationInitWindow`
- [x] **10.1.3** `_SituationInitSubsystems`

### 10.2 — Renderer root

- [x] **10.2.1** `_SituationInitRenderer`
- [x] **10.2.2** `_SituationInitOpenGL`
- [x] **10.2.3** `_SituationInitVulkan`
- [x] **10.2.4** `_SituationInitStagingBuffers`

### 10.3 — Vulkan create chain (each: grep callers, fix ignored returns)

- [x] **10.3.1** `_SituationVulkanCreateInstance`
- [x] **10.3.2** `_SituationVulkanSetupDebugMessenger`
- [x] **10.3.3** `_SituationVulkanCreateSurface`
- [x] **10.3.4** `_SituationVulkanPickPhysicalDevice`
- [x] **10.3.5** `_SituationVulkanCreateLogicalDevice`
- [x] **10.3.6** `_SituationVulkanCreateAllocator`
- [x] **10.3.7** `_SituationVulkanCreateSwapchain`
- [x] **10.3.8** `_SituationVulkanCreateImageViews`
- [x] **10.3.9** `_SituationVulkanCreateRenderPass`
- [x] **10.3.10** `_SituationVulkanCreateDepthResources`
- [x] **10.3.11** `_SituationVulkanCreateFramebuffers`
- [x] **10.3.12** `_SituationVulkanCreateCommandPool`
- [x] **10.3.13** `_SituationVulkanCreateCommandBuffers`
- [x] **10.3.14** `_SituationVulkanCreateSyncObjects`
- [x] **10.3.15** `_SituationVulkanInitInternalRenderers`
- [x] **10.3.16** `_SituationVulkanInitGraphicsSpirvLayouts`
- [x] **10.3.17** `_SituationVulkanInitComputeLayouts`

### 10.4 — GL / shader / buffer helpers

- [x] **10.4.1** `_SitGLDeferProgramUniform`
- [x] **10.4.2** `_SituationSetShaderUniformLocationImpl`
- [x] **10.4.3** `_SituationValidateSpirvBinary`
- [x] **10.4.4** `_SituationBeginGLSpirvShaderLoadAsync`
- [x] **10.4.5** `_SituationVulkanBuildGraphicsPipelinesOnSlot`
- [x] **10.4.6** `_SituationVulkanBuildMeshPipelinesOnSlot`
- [x] **10.4.7** `_SituationVulkanLoadShaderFromSpirvMemoryWithProfile`
- [x] **10.4.8** `_SituationVulkanEnsureBufferDescriptorSet`
- [x] **10.4.9** `_SituationVulkanCreateAndUploadBuffer`
- [x] **10.4.10** `_SituationVulkanReadBackBuffer`
- [x] **10.4.11** `_SituationVulkanCreateImage`

### 10.5 — Audio / IO

- [x] **10.5.1** `_SituationSetAudioDeviceInternal`
- [x] **10.5.2** `_SituationInitSoundEffects`
- [x] **10.5.3** `_SituationSetFilesystemError` — verify all FS paths use it

### Phase 10 gate

- [x] **10.G.1** Script: grep assignments like `(void)_Situation` or bare `_SituationInit*` statement-without-check — target zero in init tree
- [x] **10.G.2** **`sit/situation_base_version.h`** → **v2.4.138** — `"Internal Caller Audit"`
- [x] **10.G.3** **`doc/UPDATELOG.md`** — Phase 10 entry only
- [x] **10.G.4** Dashboard phase 10 marked ✅ — hardening complete at **v2.4.138**

---

## Per-Phase Workflow

```
1. Pick next unchecked function block (e.g. 2.1.1)
2. One commit per function or per logical group
3. Always edit FWD + body + callers together
4. Run tests:
   Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
   & ".\build_tests.bat" opengl
   Set-Location build
   $env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
   & ".\sit_test.exe" --module <m>
5. Check boxes in this doc
6. Phase completion gate (mandatory):
   - Bump SITUATION_VERSION_PATCH in sit/situation_base_version.h
   - Set SITUATION_VERSION_DESCRIPTION (see phase → patch map)
   - Prepend doc/UPDATELOG.md entry for THIS phase only
   - Update dashboard + master checklist in this doc
```

---

## Suggested PR Slicing (fun-sized)

| PR | Contains | Est. boxes |
|----|----------|------------|
| PR-A | Phase 0 + Phase 1.1 (ring/MDI/fences) | ~25 |
| PR-B | Phase 1.3 (font/text/quad/VD init) | ~25 |
| PR-C | Phase 1.4–1.6 + caller roots | ~30 |
| PR-D | Phase 2 swapchain/submit | ~32 |
| PR-E | Phase 3 soft buffer | ~35 |
| PR-F | Phase 4 uniform map | ~25 |
| PR-G | Phase 5 shader async | ~38 |
| PR-H | Phase 6–8 | ~58 |
| PR-I | Phase 9 DOC pass | ~68 |
| PR-J | Phase 10 caller audit | ~36 |

---

## Master Phase Checklist

- [x] **Phase 0** — v2.4.127 — Tooling & baseline
- [x] **Phase 1** — v2.4.128 — Init chain *(1.2.1 bindless + 1.6.2 registry init deferred)*
- [x] **Phase 2** — v2.4.129 — Vulkan swapchain/submit
- [x] **Phase 2.1** — v2.4.130 — Errno table completeness
- [x] **Phase 3** — v2.4.131 — GL soft buffer & momentum
- [x] **Phase 4** — v2.4.132 — Uniform map & slots
- [x] **Phase 5** — v2.4.133 — Shader async & hot reload
- [x] **Phase 6** — v2.4.134 — Render thread & flush
- [x] **Phase 7** — v2.4.135 — Audio internals
- [x] **Phase 8** — v2.4.136 — Stragglers
- [x] **Phase 9** — v2.4.137 — Bucket B docs
- [x] **Phase 10** — v2.4.138 — Caller audit
- [x] **Phase 11** — v2.4.178 — Vulkan 2D/draw hygiene (`_SitVulkan*` from Phase 7-bis / raster closure)

---

# Phase 11 — Vulkan 2D / draw-path hygiene (v2.4.178)

**Goal:** Apply design-contract triage to `_SitVulkanGet2DTargetHeight`, `_SitVulkanFillOrthoProjection2D`, pipeline variant resolvers, and dynamic-state record helpers added for Phase 7-bis and raster parity.

| Function | Decision | `SituationError` |
|----------|----------|------------------|
| `_SitVulkanGet2DTargetHeight` | float by design | — |
| `_SitVulkanFillOrthoProjection2D` | **convert** | `INVALID_PARAM` if `out_proj` NULL |
| `_SitVulkanBasePipelineForStride` … `_SitVulkanResolveGraphicsPipeline` | VkPipeline by design | — (null → Ensure fails) |
| `_SitVulkanGetCurrentPrimitiveTopology` | enum by design | — |
| `_SitVulkanGraphicsDynamicProcsReady` | bool by design | — |
| `_SitVulkanCmdSetDepthDynamics` … `_SitVulkanApplyTrackedRasterDynamics` | void by design (9.8 record-only) | — |
| `_SitVulkanFillGraphicsDynamicStates` | void by design | — |
| `_SitVulkanEnsureGraphicsPipelineBound` | **convert** | `INVALID_PARAM` (null cmd), `VULKAN_PIPELINE_CREATION_FAILED` (no variant), SUCCESS if `shader_slot` NULL |

**Callers updated:** `SituationCmdBeginRenderPass`, `SituationRenderVirtualDisplays`, `SituationCmdBindPipeline`, `BindVertexBuffer`, `Draw`/`DrawIndexed`/indirect, raster rebind (`SetCullMode`/`SetFrontFace`/`SetPolygonMode`), `DrawMesh`. Draw paths without bound pipeline → `INVALID_RESOURCE_HANDLE`.

**Errno:** No new codes — existing Vulkan pipeline + invalid-handle codes suffice.

### Phase 11 gate

- [x] FWD — `situation_impl_renderer_fwd.h` HARDENING comments + signatures
- [x] SIG + CALL — `_SitVulkanEnsureGraphicsPipelineBound`, `_SitVulkanFillOrthoProjection2D`
- [x] Vulkan DLL build green
- [x] **`sit/situation_base_version.h`** → **v2.4.178**
- [x] **`doc/UPDATELOG.md`** — Phase 11 entry

---

**Author**: Cursor agent (with Jacques)  
# Phase 12 — Vulkan quad / text draw validation (v2.4.179)

- [x] **`_SitVulkanValidateInternalQuadDrawReady`** — `SituationCmdDrawQuad`, `SituationCmdDrawTexture`
- [x] **`_SitVulkanValidateInternalTextDrawReady`** — `SituationCmdDrawTextEx`
- [x] FWD + caller `SIT_RETURN_IF_ERR`; no new errno codes

---

# Phase 13 — OpenGL quad / text draw validation (v2.4.180)

- [x] **`_SituationGLValidateInternalQuadDrawReady`** — record + `SIT_OP_DRAW_QUAD` execute
- [x] **`_SituationGLValidateInternalTextDrawReady`** — record + `SIT_OP_DRAW_TEXT(_EX)` execute
- [x] Parity with Vulkan Phase 12; existing errno codes only

---

**Status**: **Internal hardening through Phase 15 at v2.4.182**; OpenGL executor foundation **v2.4.183** is bolster (**Phase 7-ter**), not Phase 16.

---

# Phase 14 — Central allocator (future; MyBuddy) ⬜

**Goal:** Route all library heap traffic through `SituationAlloc` / `SituationFree` (or thin `SIT_MALLOC`/`SIT_FREE` macros) so leaks, double-free, and stomp detection can be centralized before adopting **MyBuddy**.

| Task | Status |
|------|--------|
| Inventory direct `malloc`/`free`/`calloc`/`realloc` outside `SIT_*` macros | ⬜ |
| Single implementation module; debug fences optional | ⬜ |
| Harness / full suite under debug allocator | ⬜ |
| MyBuddy integration only after allocator API is stable and MyBuddy is trusted | ⬜ |

**Not in scope for Phase 14:** changing public API signatures; replacing every call site in one commit.

---

# Phase 15 — Audio graph RT teardown (use-after-free) ✅ v2.4.182

**Observed:** Intermittent harness **ACCESS_VIOLATION** on Windows after `tone_synth.phase1_compare_a4`, often when the next test (`midi_complex_melody`) runs virtual MIDI + active graph. **Not caused by OpenGL deferred-execute work.**

**Root cause:** `SituationDestroyGraph()` on the main thread while the miniaudio callback could still be inside `SituationProcessGraph(active_graph, …)` — unsynchronized `sit_audio.active_graph` pointer.

| Task | Status |
|------|--------|
| `SituationDestroyGraph`: if `graph == sit_audio.active_graph`, clear active graph first (defensive) | ✅ |
| `is_in_audio_callback` + `_SituationWaitUntilAudioCallbackIdle()` before freeing graph nodes | ✅ |
| Clear `default_graph` / `default_graph_voice_source` when destroying that graph | ✅ |
| Document harness: `sit_midi_graph_fixture_release` + module crash cleanup | ✅ (harness) |
| Re-run `tone_synth` module in a loop until stable | ⬜ (verify after rebuild) |

**Harness (shipped):** Windows `SetUnhandledExceptionFilter` + `sit_test_crash_recover` so AV becomes a **failed test** with name, not silent process exit. See `doc/plan/TEST_HARNESS_PLAN.md`.

---

# Related — OpenGL deferred executor (bolster, not Phase 16) ✅ v2.4.181 + v2.4.183

**Not internal-hardening phase slots.** Tracked under **`doc/plan/renderer_bolster_plan.md`** **Phase 7-ter**.

| Version | Scope |
|---------|--------|
| **v2.4.181** | Execute-time **`exec_inside_render_pass`**; quad/text/texture no longer fail with **`NO_RENDER_PASS_ACTIVE`** after record-time EndRenderPass. |
| **v2.4.183** | Baseline raster reset; indexed/quad/texture execute hygiene; **`glFinish`** before screenshot; readback flip policy; NEAREST non-mipmap textures. OpenGL harness **428/428**. |

**Hardening overlap:** **v2.4.179–180** (Phases 12–13) quad/text **`_Situation*ValidateInternal*DrawReady`** validators are prerequisites for safe execute paths documented in 7-ter.
