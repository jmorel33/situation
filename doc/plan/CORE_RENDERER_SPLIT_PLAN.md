# Control & Renderer Split Plan

**Date:** 2026-05-05  
**Status:** Complete (v2.4.9)  
**Prerequisite:** VD extraction complete (v2.4.8)

## Goal

Transform `situation_impl.h` from a 19,800-line monolith into a thin orchestrator that includes two major new modules:
- `sit/situation_impl_ctrl.h` — Lifecycle, logging, error handling, init/shutdown, update loop
- `sit/situation_impl_renderer.h` — All OpenGL + Vulkan backend code (init, commands, resources, shaders, textures, meshes, buffers, models, render thread)

After this, `situation_impl.h` becomes a ~200-line file that just includes everything in the right order.

## Final Target Architecture

```
situation.h
├── sit/situation_api.h
└── sit/situation_impl.h              (~200 lines — orchestrator, includes only)
    ├── #include "situation_impl_deps.h"
    ├── #include "situation_impl_decl.h"
    ├── #include "situation_impl_forward.h"
    ├── #include "situation_impl_threading.h"
    ├── #include "situation_impl_io.h"
    ├── #include "situation_impl_input.h"
    ├── #include "situation_impl_wdm.h"
    ├── #include "situation_impl_image.h"
    ├── #include "situation_impl_timer.h"
    ├── #include "situation_impl_renderer.h"   ← NEW (~17,500 lines)
    ├── #include "situation_impl_vd.h"
    ├── #include "situation_impl_ctrl.h"       ← NEW (~1,500 lines) — LAST
    └── sit/situation_impl_audio.h (separate)
```

**Include order rationale:** The public API forward declarations (`situation_api.h`) come first —
included by the outer `situation.h` before `situation_impl.h` is ever reached. This means all
`SITAPI` function prototypes are already visible to every module. Then `situation_impl_forward.h`
provides the second layer: internal `static` function prototypes that let implementation modules
call each other regardless of include order. Renderer comes before VD (VD uses renderer helpers)
and before ctrl. Ctrl comes LAST because it's the orchestrator — it calls into everything else,
and by the time it's included all called functions are already defined (not just forward-declared).
Renderer calling ctrl functions (e.g., `_SituationSetErrorFromCode`) works because
`situation_impl_forward.h` provides the prototype, and in a single TU static functions
only need a forward declaration to be callable from anywhere.

---

## `situation_impl_ctrl.h` — Scope (~1,500 lines)

### Error & Logging
- `_SituationSetError`
- `_SituationSetErrorFromCode` (+ the big error message switch)
- `SituationSetTraceLogLevel`
- `SituationLog`, `SituationLogWarning`
- `SituationGetLastErrorMsg`

### Version & State
- `SituationGetVersionString`
- `SituationIsInitialized`, `SituationGetInitState`
- `SituationGetRendererType`
- `SituationFreeString`

### Lifecycle
- `SituationInit` (the master init orchestrator)
- `SituationShutdown`
- `SituationUpdate`, `SituationPollInputEvents`, `SituationUpdateTimers`
- `_SituationInitPlatform`, `_SituationInitWindow`, `_SituationInitRenderer`, `_SituationInitSubsystems`
- `_SituationCleanupPlatform`, `_SituationCleanupRenderer`, `_SituationCleanupSubsystems`
- `_SituationFullCleanupOnError`

### Callbacks & Arguments
- `SituationSetExitCallback`, `SituationSetResizeCallback`
- `SituationIsArgumentPresent`, `SituationGetArgumentValue`

### Clipboard & File Drop
- `SituationGetClipboardText`, `SituationSetClipboardText`
- `SituationSetFileDropCallback`, `SituationIsFileDropped`
- `SituationLoadDroppedFiles`, `SituationUnloadDroppedFiles`

### Utility Helpers (small, used everywhere)
- `_SituationClampf`, `_SituationLerpf`, `_SituationFMin3`, `_SituationFMax3`
- `_SituationGetHighResTime`
- `_sit_hash_string`, `_sit_strcasecmp`, `_sit_dirname`, `_sit_directory_exists`
- `SituationLoadBitmapFontFromMemory` (GPU upload but called during init)

---

## `situation_impl_renderer.h` — Scope (~17,500 lines)

Everything else — the pure renderer:

### OpenGL Backend
- `_SituationInitOpenGL`, `_SituationCleanupOpenGL`
- GL ring buffer, MDI buffer, fences
- `_SitGLBackupState`, `_SitGLRestoreState`, `_SitGLInvalidateShadowState`
- `_SitGLGetCachedVAO`, `_SitGLDeferDestroy*`, `_SitGLFlushGraveyard`
- `_SituationCheckGLError`, `_SituationLogGLError`
- `_SituationGLExecuteCommands` (the big GL command processor)
- `_SituationInitGLVirtualDisplayRenderer`
- GL shader compilation (`_SituationCompileGLShader`, `_SituationCreateGLShaderProgram*`)
- Virtual Bindless system

### Vulkan Backend
- `_SituationInitVulkan`, `_SituationCleanupVulkan`
- All Vulkan create/destroy helpers (swapchain, image views, render pass, framebuffers, command pool, sync objects)
- `_SituationVulkanRecreateSwapchain`
- `_SituationVulkanInitInternalRenderers`
- Vulkan staging buffers, graveyard, deferred destroy
- SPIR-V compilation (shaderc integration)
- Descriptor set management

### Shared Renderer
- `SituationAcquireFrameCommandBuffer`, `SituationEndFrame`
- `SituationGetMainCommandBuffer`, `SituationGetComputeCommandBuffer`
- All `SituationCmd*` functions (draw, bind, viewport, scissor, dispatch, barriers)
- Resource management: `SituationCreateTexture/Buffer/Mesh/Shader/ComputePipeline` + destroy + update
- `SituationLoadTexture`, `SituationLoadModel`, `SituationLoadShader`
- Model rendering (`SituationDrawModel`, GLTF extraction)
- Render lists (`SituationCreateRenderList`, `SituationSubmitRenderList`, etc.)
- Metrics (`SituationGetDrawCallCount`, `SituationGetVRAMUsage`, `SituationGetRenderLatencyStats`)
- Hot-reload (`_SituationPerformHotReloadPass`, `SituationReloadShader/Texture/Model`)
- Render thread (`_SitFlushFrameResources`, `_SituationRenderThreadEntry`)

---

## Execution Plan

### Phase 1: Extract Renderer

**Actions:**
- [x] Create `sit/situation_impl_renderer.h`
- [x] Move ALL renderer code from `situation_impl.h` (GL/VK init, commands, resources, shaders, textures, meshes, buffers, models, render thread, hot-reload)
- [x] Move resource slot alloc/free/get helpers
- [x] Move `SituationLoadBitmapFontFromMemory` (GPU upload)
- [x] Move GLFW callbacks that are renderer-coupled (`_SituationGLFWFramebufferSizeCallback`, etc.)
- [x] Add `#include "situation_impl_renderer.h"` in `situation_impl.h` (before VD, before ctrl)
- [x] Build both backends

### Phase 2: Extract Control

**Actions:**
- [x] Create `sit/situation_impl_ctrl.h`
- [x] Move error/logging functions (`_SituationSetError`, `_SituationSetErrorFromCode`, `SituationLog`, `SituationLogWarning`, `SituationGetLastErrorMsg`)
- [x] Move version/state queries (`SituationGetVersionString`, `SituationIsInitialized`, `SituationGetInitState`, `SituationGetRendererType`)
- [x] Move lifecycle functions (`SituationInit`, `SituationShutdown`, `SituationUpdate`, `SituationPollInputEvents`, `SituationUpdateTimers`)
- [x] Move init/cleanup helpers (`_SituationInitPlatform`, `_SituationInitWindow`, `_SituationInitRenderer`, `_SituationInitSubsystems`, `_SituationCleanupPlatform`, `_SituationCleanupRenderer`, `_SituationCleanupSubsystems`, `_SituationFullCleanupOnError`)
- [x] Move callbacks, arguments, clipboard, file drop
- [x] Move small utility helpers (`_SituationClampf`, `_SituationLerpf`, etc.)
- [x] Add `#include "situation_impl_ctrl.h"` in `situation_impl.h` (LAST — after VD)
- [x] Build both backends

### Phase 3: Verify & Finalize

**Actions:**
- [x] Clean build passes
- [x] `situation_impl.h` is ~707 lines (orchestrator with font data + includes)
- [x] `situation_impl_ctrl.h` is ~2,277 lines
- [x] `situation_impl_renderer.h` is ~16,917 lines
- [x] Bump version to 2.4.9
- [ ] Update UPDATELOG.md

---

## Constraints

- **Include order matters.** Renderer must come before VD and before ctrl. Ctrl is last — it orchestrates everything.
- **Two-layer forward declarations.** Layer 1: `situation_api.h` (public SITAPI prototypes, visible to all). Layer 2: `situation_impl_forward.h` (internal static prototypes, cross-module bridge).
- **Renderer must come before VD.** (VD calls renderer helpers like `_SitGLBackupState`, deferred destroy)
- **Ctrl depends on decl/forward/threading/io/input/wdm/image/timer/renderer** being already included (for types, state access, and direct function calls)
- **Single TU.** Still all one translation unit.
- **No functional changes.**

---

## Risk Assessment

- **Phase 1 (Renderer) is the big move** but conceptually simple — it's everything between the module includes and the lifecycle functions. The forward declarations file already covers cross-module calls.
- **Phase 2 (Control) is clean.** By the time we extract ctrl, everything it calls is already defined in earlier includes. No forward-declaration gymnastics needed for ctrl→renderer calls.
- **The tricky part is `SituationInit`.** It orchestrates everything (calls `_SituationInitPlatform`, `_SituationInitWindow`, `_SituationInitRenderer`, `_SituationInitSubsystems`). The init helpers that do actual GL/VK work (`_SituationInitRenderer`) go in renderer. The orchestrator (`SituationInit`) and platform/window init go in ctrl. Ctrl calls renderer init helpers — those are already defined by the time ctrl is included.
- **`_SituationSetErrorFromCode` in ctrl, called by renderer:** Works because `situation_impl_forward.h` forward-declares it, and in a single TU that's all you need.

---

## After This

The architecture is complete:
- `situation_impl.h` = thin orchestrator (~200 lines)
- 11 focused module files, each <3,000 lines
- 1 renderer file (~17,500 lines) — future work could split GL vs VK
- Clean separation of concerns
- Every file has a clear, single responsibility
