# Virtual Display — Content Update Detection & Idle Fallback Plan

**Date:** 2026-06-08  
**Status:** **Phase 1 COMPLETE** — content tracking + query API shipped. **Phase 2a** (compositor idle fallback) implemented in core + harness.  
**Scope:** Per-VD **content write** tracking, query API, and **shader-only** compositor fallback when a display goes idle  
**Related:** [`renderer_bolster_plan.md`](renderer_bolster_plan.md) (VD compositor)

### Phase 1 files touched

| File | Change |
|------|--------|
| `sit/situation_api.h` | Struct fields, `SituationGetVirtualDisplayUpdateInfo`, `SituationSetVirtualDisplayIdleThreshold`, `SIT_VD_MAX_COMPUTE_TEXTURE_BINDS` |
| `sit/situation_impl_decl.h` | Recording-state fields on `SituationGLSoftCommandBuffer` + Vulkan render state |
| `sit/situation_impl_forward.h` | Forward declarations for `_SitVD*` hooks |
| `sit/situation_impl_vd.h` | Hook implementations + public API |
| `sit/situation_impl_renderer.h` | Write hooks at EndRenderPass, draws, copy, dispatch |
| `sit/situation_base_trace.h` | Trace IDs for new APIs |
| `tests/harness/test_virtual_display.c` | `vd_content_update_info_after_draw`, `vd_content_update_info_idle` |

---

## Problem

Applications need to know when a Virtual Display (VD) last received **new pixel content**, distinct from:

| Existing field | Meaning | Updated when |
|----------------|---------|--------------|
| `last_update_time_seconds` | VD **frame clock** (animation / delta time) | Every VD tick in the main loop |
| `is_dirty` | Offscreen buffer **may** need redraw before composite | Manual / render-to-VD paths (best-effort) |
| *(missing)* | **Last content write** (app actually changed pixels) | — |

Without content-write tracking, stale VDs composite unchanged (or black) textures with no visible indication. This plan adds timestamps + optional **compositor-shader** idle visuals.

---

## Design principles

1. **Situation-only scope** — This plan changes only Situation core: `SituationVirtualDisplay`, command-buffer write hooks, compositor shaders, harness, wrappers, and docs. No references to or requirements on downstream apps or libraries.
2. **Content tracking is CPU-side metadata** (timestamps in `SituationVirtualDisplay`); cheap hooks at real write sites.
3. **Idle fallback visuals are strictly shader-based** in the **compositor** fragment shaders (`SIT_VD_FRAGMENT_SHADER_SRC`, `SIT_COMPOSITE_FRAGMENT_SHADER_SRC`). No CPU pixel fills, no rewriting the offscreen FBO for fallback.
4. **Optional debug text overlay** (metrics string) is **explicitly out of scope** for the shader-only rule — gated behind `SITUATION_VERBOSE_DIAGNOSTICS` or a separate debug flag (Phase 2b, optional).
5. **Non-disruptive:** when not idle, compositor behaviour is unchanged (sample VD texture as today).

---

## Codebase alignment (review notes)

These items were verified against the current Situation implementation and must be respected during implementation.

| Topic | Current code reality | Plan implication |
|-------|---------------------|------------------|
| **`is_dirty`** | Set via `SituationSetVirtualDisplayDirty`; **never read** by `SituationRenderVirtualDisplays`. Docs claim compositor uses it — that is stale. | Content-update tracking is **new** and independent. Optionally auto-set `is_dirty` when content is marked updated, but do not conflate the two fields. |
| **`SITUATION_VD_FLAG_COMPUTE_TARGET`** | No render pass / FBO raster path; texture registered in `texture_registry` via `texture_slot_index`. | Content writes arrive via **`SituationCmdDispatch`** / **`SituationCmdDispatchIndirect`** and **`SituationCmdCopyTexture`** to the VD texture handle — **not** via `SituationCmdBeginRenderPass` (that fails: `vk.render_pass == VK_NULL_HANDLE`). |
| **Raster VD writes** | Standard VD: FBO + render pass; no `texture_slot_index`. | Track via **EndRenderPass** when `display_id >= 0` and the pass recorded at least one draw (or clear+draw policy below). |
| **Time base** | VD frame clock uses `SituationUpdateTimers()` (`timer_system` or `sit_gs.current_time`). Plan snippets used `glfwGetTime()`. | Use `_SitVDGetTimeSeconds()` in **`situation_impl_vd.h`** (same clock as VD frame tick). Do not mix clocks. |
| **`frames_since_update`** | `vd->frame_count` increments every frame in `SituationUpdateTimers`, independent of draws. | Document that this is **VD frame ticks since last write**, not main-loop frames. |
| **Push constants today** | Path B: `VDPushConstants { mat4 model; float opacity; }`. Path A: `CompositePushConstants { mat4 model; int blendMode; float opacity; }`. | Extend **both** layouts separately (or add compositor-only uniforms on OpenGL). Do not assume one shared struct fits both pipelines. |
| **OpenGL compositor** | `SIT_OP_RENDER_VIRTUAL_DISPLAYS` executes in soft-buffer replay (`situation_impl_renderer.h`). | Idle uniforms must be set in the **GL execute path**, not only at Vulkan record time. |
| **OpenGL deferred execute** | Record-time `EndRenderPass` runs before GPU replay; draw opcodes execute later in `_SituationGLExecuteCommands`. | Also track `exec_pass_display_id` / `exec_pass_had_draw` at execute time and call `_SitVDEndRenderPassCheck` on `SIT_OP_END_RENDER_PASS` execute. |
| **Compute dispatch → VD** | No existing tracking of which storage image is bound at dispatch. | Phase 1 requires **new per-command-buffer bind state**: record `SituationCmdBindComputeTexture` targets; on dispatch, mark any bound slot that maps to a `COMPUTE_TARGET` VD. |
| **CopyTexture → VD** | Reverse lookup easy for compute VDs (`texture_slot_index`). Raster VDs are not in the registry. | Copy hooks apply to **compute-target** VD textures via slot index; raster VD pixel updates go through render-pass draws only. |

---

## Phase 1 — Content update tracking & query API

### New struct fields (`situation_api.h`)

Add to `SituationVirtualDisplay`:

```c
double   last_content_update_time;    // _SitVDGetTimeSeconds() at last content write
uint64_t last_content_update_frame;   // vd->frame_count at last content write
double   idle_threshold_seconds;      // idle if (now - last_content_update_time) > this (used by Phase 2a)
```

- [x] Fields added to `SituationVirtualDisplay` in `situation_api.h`

**Defaults at `SituationCreateVirtualDisplayEx`:**

- `last_content_update_time = _SitVDGetTimeSeconds()` (treat creation as initial write so new VDs are not immediately “idle”)
- `last_content_update_frame = 0`
- `idle_threshold_seconds = 1.0` (production-friendly; tests may set `0.1`)

- [x] Defaults initialized in `SituationCreateVirtualDisplayEx` (both `SituationCreateVirtualDisplay` and Ex route through Ex)

`fallback_mode` / `fallback_color` are **Phase 2a** — added with compositor idle fallback.

- [x] `fallback_mode` / `fallback_color` on `SituationVirtualDisplay` (default SOLID, deep blue `{13,38,102,255}`)
- [x] `SituationSetVirtualDisplayFallbackMode`, `SituationSetVirtualDisplayFallbackColor`

Do **not** overload `last_update_time_seconds` — it remains the per-frame animation clock.

### New public API (`situation_api.h`)

```c
/**
 * @brief Query when a VD last received new pixel content (not the frame clock).
 */
SITAPI SituationError SituationGetVirtualDisplayUpdateInfo(
    int display_id,
    double* out_last_content_update_time,   /* optional */
    uint64_t* out_last_content_update_frame,/* optional */
    uint64_t* out_frames_since_update,      /* optional: frame_count - last_content_update_frame */
    double* out_seconds_since_update        /* optional: now - last_content_update_time */
);

SITAPI void SituationSetVirtualDisplayIdleThreshold(int display_id, double threshold_seconds);
```

- [x] Public API declared in `situation_api.h`
- [x] Implemented in **`situation_impl_vd.h`** (VD module)
- [x] Forward declarations in **`situation_impl_forward.h`**
- [x] Recording-state **struct fields** only in **`situation_impl_decl.h`**

```c
static void _SitVDMarkContentUpdated(SituationVirtualDisplay* vd) {
    vd->last_content_update_time = _SitVDGetTimeSeconds();
    vd->last_content_update_frame = vd->frame_count;
    vd->is_dirty = true;
}
```

### Pass recording state (new internal state)

Neither backend currently tracks “which VD is being rendered to” or “did this pass produce content” at `EndRenderPass`. Phase 1 must add **per command-buffer recording state**:

```c
/* On SituationGLSoftCommandBuffer and/or sit_render.vk recording fields */
int  recording_pass_display_id;   /* -1 = main window, >= 0 = VD, unset when no pass */
bool recording_pass_had_draw;     /* true after any draw/dispatch/copy-to-VD in this pass */
```

Reset `recording_pass_had_draw` on `BeginRenderPass`; set it on first qualifying draw/copy/dispatch; consult it on `EndRenderPass`.

- [x] `SituationGLSoftCommandBuffer` fields + Vulkan `sit_render.vk` recording fields
- [x] Reset on frame acquire / broken-buffer recovery (`_SitVDResetGLRecordingState`)

### Content-write hooks (`situation_impl_renderer.h` + VD module)

Call `_SitVDMarkContentUpdated` only on **actual writes**, not binds or pass setup.

| Hook site | Condition |
|-----------|-----------|
| **`SituationCmdEndRenderPass`** | `recording_pass_display_id >= 0` **and** `recording_pass_had_draw == true` — **primary raster hook** |
| **`SituationCmdCopyTexture`** / **`SituationCmdCopyBufferToTexture`** | Destination `SituationTexture` maps to a `COMPUTE_TARGET` VD via `texture_slot_index` |
| **`SituationCmdDispatch`** / **`SituationCmdDispatchIndirect`** | After dispatch: any **currently bound** compute storage texture that maps to a `COMPUTE_TARGET` VD |

**Do not** hook `SituationCmdBeginRenderPass` for timestamps — a pass that only clears is not a content update (see open questions).

**Do not** hook `SituationCmdBindComputeTexture` — binding is not a write.

**Qualifying draws inside a VD pass** (quads, text, meshes, user shaders, **DrawTexture**): set `recording_pass_had_draw = true` on first such command while `recording_pass_display_id >= 0`.

**Main window (`display_id == -1`)** does not update any VD timestamp.

**Helper:** `_SitVDMarkContentUpdatedFromTextureSlot` (slot index → compute-target VD lookup; replaces planned `_SitVDFromTextureSlot`).

- [x] `SituationCmdEndRenderPass` raster hook
- [x] `SituationCmdCopyTexture` / `SituationCmdCopyBufferToTexture` copy hooks
- [x] `SituationCmdBindComputeTexture` bind-state tracking (no timestamp bump)
- [x] `SituationCmdDispatch` / `SituationCmdDispatchIndirect` dispatch hooks
- [x] Draw commands set `recording_pass_had_draw` (`_SitVDRecordingNoteDrawCmd`)

### Phase 1 exit criteria

- [x] API returns sensible `seconds_since_update` / `frames_since_update` after known writes (`test_vd_content_update_info_after_draw`)
- [x] `SITUATION_VD_FLAG_COMPUTE_TARGET` dispatch/copy hooks implemented (bind alone does not bump)
- [ ] `vd_compute_target_updates_on_dispatch` harness test (deferred — hooks in place, test not written yet)
- [x] `last_update_time_seconds` behaviour unchanged (separate field; frame clock still driven by `SituationUpdateTimers`)
- [x] Harness unit-style tests in `tests/harness/test_virtual_display.c` (`after_draw`, `idle`; no pixel readback)

---

## Phase 2a — Shader-only idle compositor fallback (required)

When `(now - last_content_update_time) > idle_threshold_seconds`, compositor treats the VD as **idle** for that frame.

### Idle decision (CPU, compositor only)

In `SituationRenderVirtualDisplays` (OpenGL + Vulkan):

```c
bool is_idle = (_SitVDGetTimeSeconds() - vd->last_content_update_time) > vd->idle_threshold_seconds;
double elapsed_idle = is_idle ? (_SitVDGetTimeSeconds() - vd->last_content_update_time) : 0.0;
```

Pass to **both** compositor paths:

- **Path B (alpha):** `SIT_VD_FRAGMENT_SHADER_SRC`
- **Path A (advanced blend):** `SIT_COMPOSITE_FRAGMENT_SHADER_SRC`

### New API (`situation_api.h`)

```c
typedef enum {
    SITUATION_VD_FALLBACK_SOLID,       /* flat fallback_color */
    SITUATION_VD_FALLBACK_COLORBURST   /* SMPTE-style 75% vertical color bars */
} SituationVDFallbackMode;

/* On SituationVirtualDisplay: */
SituationVDFallbackMode fallback_mode;  /* default SOLID */
ColorRGBA fallback_color;               /* default deep blue {13, 38, 102, 255} */

SITAPI void SituationSetVirtualDisplayFallbackMode(int display_id, SituationVDFallbackMode mode);
SITAPI void SituationSetVirtualDisplayFallbackColor(int display_id, ColorRGBA color);
```

### Push constants / uniforms (Vulkan + OpenGL parity)

Today the compositor uses **two different** push-constant blocks (see `situation_impl_decl.h`):

| Path | Vulkan push constants | OpenGL uniforms |
|------|----------------------|-----------------|
| **B (alpha)** | `VDPushConstants { mat4 model; float opacity; }` | `u_projection`, `u_model`, `u_opacity` |
| **A (advanced blend)** | `CompositePushConstants { mat4 model; int blendMode; float opacity; }` | `u_projection`, `u_model`, `u_blendMode`, `u_opacity` |

Extend **each** layout with idle fields (mind **Vulkan std430 alignment** — pad after `int` fields). Example additions (same semantics on both paths):

```c
/* Path B — append to VDPushConstants */
int   is_idle;           /* 0 or 1 */
int   fallback_mode;     /* SituationVDFallbackMode */
float elapsed_idle;      /* seconds since last content write */
float fallback_color[4]; /* normalized 0..1 RGBA */

/* Path A — append to CompositePushConstants (after existing blendMode/opacity) */
/* same four fields */
```

OpenGL: matching uniforms on **both** `vd_shader_program_id` and `composite_shader_program_id` (`u_isIdle`, `u_fallbackMode`, `u_fallbackColor`, `u_elapsedIdle`).

Verify combined struct sizes stay within the Vulkan push-constant limit (128 bytes typical; measure after layout).

### Fragment shader behaviour (both compositor FS)

When `is_idle == 0`: unchanged — `texture(u_screenTexture / u_sourceTexture, v_texCoord)`.

When `is_idle == 1`: **do not sample** the stale VD texture for RGB; generate background in-shader:

```glsl
vec3 _sit_smpte_color_bars(vec2 uv) { /* 2/3 main bars | 1/12 castellation | 1/4 -I/white/+Q/PLUGE */ }

if (is_idle != 0) {
    if (fallback_mode == 1) { /* COLORBURST — SMPTE 75% bars */
        vec3 rgb = _sit_smpte_color_bars(v_texCoord);
        outColor = vec4(rgb, opacity);
    } else {
        outColor = vec4(fallback_color.rgb, opacity);
    }
} else {
    /* existing texture sample path */
}
```

Advanced Path A: idle branch runs **before** blend-mode math. Synthetic idle RGB uses **`opacity` for alpha** (treat idle pixel as `vec4(idle_rgb, opacity)` when mixing with destination) so advanced blend modes still behave predictably.

### Phase 2a exit criteria

- [x] Idle VD shows solid or colorburst **without** sampling stale texels
- [x] Non-idle VD pixel-identical to pre-change compositor (regression — existing draw tests)
- [x] Both Path A and Path B covered
- [x] Harness: `vd_idle_content_switch` — timed idle → live → idle pixel readback
- [ ] Harness: `vd_idle_fallback_colorburst` — idle readback matches SMPTE bar colors (≥4 bar kinds)

---

## Phase 2b — Optional debug text overlay (NOT shader-based)

**Out of scope for “strictly shader rendering.”** Optional developer aid only.

- Gate: `#if defined(SITUATION_VERBOSE_DIAGNOSTICS)` or `SituationVirtualDisplay.flags` debug bit (TBD).
- When idle **and** debug enabled: after compositor draw, issue `SituationCmdDrawText` / `_SitGLDrawTextDirect` with `%.2f s` idle time and `frames_since_update`.
- Default **off** — production builds use shader fallback only.

Do not implement Phase 2b until Phase 2a is green.

---

## Phase 3 — Language wrappers & docs

| File | Change |
|------|--------|
| `wrappers/Odin/situation.odin` | New fields + `SituationGetVirtualDisplayUpdateInfo`, fallback setters |
| `wrappers/Zig/situation.zig` | Same |
| `wrappers/Rust/src/lib.rs` | Same |
| `doc/UPDATELOG.md` | On ship |
| `doc/situation_sdk.md` § Virtual Display | Document content vs frame clock, idle fallback |

---

## Verification plan

### Automated (harness)

File: **`tests/harness/test_virtual_display.c`** (not `situation_verify.cpp`).

| Test | Description | Status |
|------|-------------|--------|
| `vd_content_update_info_after_draw` | BeginRenderPass(display_id=vd), draw solid red, end pass → `seconds_since_update` ≈ 0 | [x] |
| `vd_content_update_info_idle` | Clear-only pass does not bump; no draw for > threshold → `seconds_since_update` > threshold | [x] |
| `vd_compute_target_updates_on_dispatch` | `COMPUTE_TARGET` VD + dispatch → timestamp bumps | [ ] Phase 1 deferred |
| `vd_idle_fallback_solid` | Idle + SOLID mode → readback matches `fallback_color` (tolerance) | [x] via `vd_idle_content_switch` |
| `vd_idle_fallback_colorburst` | Idle + COLORBURST → readback matches SMPTE bar colors (≥4 kinds) | [ ] Phase 2a optional |
| `vd_idle_content_switch` | Timed 1.5s: idle SOLID → `prairie.jpg` live → idle SOLID (continuous frames) | [x] |
| `vd_idle_content_switch_colorburst` | Timed 1.5s: idle COLORBURST → second assets photo (or built-in grid) live → COLORBURST | [x] |

Run Phase 1: `build_tests.bat static-vulkan` → `build\sit_test.exe --module virtual_display --filter content_update`

### Manual

- Create a VD, render content, then stop drawing for longer than `idle_threshold_seconds` → compositor shows colorburst/solid at the VD layer; resume draws → normal texture returns.
- Advanced blend VD (`SITUATION_BLEND_OVERLAY`): idle branch still visible (Path A).

---

## Non-goals

- Rewriting offscreen VD FBO contents when idle
- CPU-generated fallback bitmaps
- Mandatory on-screen metrics text in release builds
- Fixing unrelated stale VD API docs (`BeginVirtualDisplayFrame`, `PauseVirtualDisplay` references in comments — separate cleanup)

---

## Implementation order

1. [x] **Phase 1** — tracking + query API + harness info tests (compute-target harness test deferred)  
2. [x] **Phase 2a** — compositor shader idle branch (GL + VK) + `vd_idle_content_switch` pixel test  
3. [ ] **Phase 3** — wrappers + docs  
4. [ ] **Phase 2b** — optional debug text (if still wanted)

---

## Open questions

- [x] Should `BeginRenderPass` with only `CLEAR` count as a content update? **Decision:** no — only `EndRenderPass` with `recording_pass_had_draw`.
- [ ] Per-VD opt-out flag (`SITUATION_VD_FLAG_NO_IDLE_FALLBACK`) for apps that want stale texture visible?
- [ ] `idle_threshold_seconds == 0.0`: treat as always idle, or clamp to a minimum (e.g. `0.05`)?
- [x] Should `SituationGetVirtualDisplayUpdateInfo` return `SITUATION_ERROR_*` for invalid IDs? **Decision:** yes — returns `SITUATION_ERROR_NOT_INITIALIZED` / `SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID` (consistent with other VD APIs).
