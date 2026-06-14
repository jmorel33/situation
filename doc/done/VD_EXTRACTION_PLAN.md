# Virtual Display Module Extraction Plan

**Date:** 2026-05-05  
**Completed:** 2026-06-12  
**Status:** Complete  
**Target Version:** 2.4.8  
**Completed Version:** 2.4.255  
**Prerequisite:** WDM/Image/Timer extraction complete (v2.4.7)

## Goal

Extract the Virtual Display subsystem API from `situation_impl.h` into a new module:
- `sit/situation_impl_vd.h` — Virtual display creation, destruction, configuration, compositing, and queries

## Scope — What Moves

### Contiguous VD API Block (lines ~18373–19195)
- `SituationCreateVirtualDisplay`
- `SituationDestroyVirtualDisplay`
- `SituationSetVirtualDisplayScalingMode`
- `_SituationSortVirtualDisplaysCallback`
- `SituationRenderVirtualDisplays` (the main compositing entry point)
- `SituationConfigureVirtualDisplay`
- `SituationGetVirtualDisplay`
- `SituationSetVirtualDisplayDirty`
- `SituationIsVirtualDisplayDirty`
- `SituationGetLastVDCompositeTimeMS`
- `SituationGetVirtualDisplaySize`

### Standalone GL Helper (line ~3580)
- `_SituationInitGLVirtualDisplayRenderer` — creates the GL quad VAO/VBO for VD compositing

### Estimated Total
~870 lines

---

## Scope — What Stays (embedded in other functions)

These are VD-related code fragments that are woven into larger functions and CANNOT be cleanly extracted:

| Location | Context | Why it stays |
|----------|---------|--------------|
| `_SituationGLExecuteCommands` case `SIT_OP_RENDER_VIRTUAL_DISPLAYS` (~line 4373) | GL deferred compositing switch case | Embedded in a 1000-line switch statement |
| `_SituationGLExecuteCommands` case `SIT_OP_BEGIN_RENDER_PASS` VD branch (~line 3933) | GL begin pass for VD FBO | Embedded in switch case |
| `_SituationInitOpenGL` VD slot init (~line 4980) | Zeroes VD slots during GL init | 5 lines inside a 200-line init function |
| `_SituationVulkanInitInternalRenderers` (~line 5438) | Compiles VD shaders, creates VD pipeline | Entire function is VD+quad renderer init — could move but tightly coupled to Vulkan init sequence |
| `SituationShutdown` VD cleanup loop (~line 9998) | Destroys all VDs on shutdown | 4 lines inside shutdown |

---

## Execution

### Phase 1: Move `_SituationInitGLVirtualDisplayRenderer`

**Actions:**
- [x] Create `sit/situation_impl_vd.h` with header/guard
- [x] Move `_SituationInitGLVirtualDisplayRenderer` (~line 3580, ~50 lines)
- [x] Add `#include "situation_impl_vd.h"` in `situation_impl.h` (LATE — after all renderer helpers, before the VD API block's current position)
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

### Phase 2: Move Contiguous VD API Block

**Actions:**
- [x] Move lines 18373–19195 (all VD API functions + sort callback) to `situation_impl_vd.h`
- [x] Ensure `_SituationSortVirtualDisplaysCallback` forward declaration (line 564) remains for GL execute commands
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

### Phase 3: Verify & Clean

**Actions:**
- [x] Clean build passes
- [x] Verify line counts:
  - [x] `situation_impl_vd.h`: ~870–950 lines (actual: ~1500 lines — scope grew with idle/fallback additions)
  - [x] `situation_impl.h`: reduced to ~19,800 lines

---

## Include Position

The VD file must be included LATE — after:
- All OpenGL helpers (`_SitGLBackupState`, `_SitGLDeferDestroyTexture`, etc.)
- All Vulkan helpers (`_SituationDeferDestroyImage`, `_SituationDeferDestroyFramebuffer`, etc.)
- The Vulkan image/buffer creation helpers
- The GL execute commands function (which references `_SituationSortVirtualDisplaysCallback` via forward decl)

Best position: right before the GLTF/Model loading section (line ~19197 currently), which is where the VD block already ends.

---

## Constraints

- **Single TU.** Still all `#include`d into one translation unit.
- **Forward declaration for `_SituationSortVirtualDisplaysCallback`** already exists at line 564 — covers usage in GL execute commands.
- **Forward declaration for `_SituationInitGLVirtualDisplayRenderer`** already exists at line 428 — covers usage in `_SituationInitOpenGL`.
- **No functional changes.** Pure structural refactor.

---

## Risk Mitigation

- **If `SituationRenderVirtualDisplays` calls GL/VK functions directly:** It does — but those are all defined before the VD include point.
- **If `SituationCreateVirtualDisplay` creates framebuffers/textures:** It does — uses `glCreateFramebuffers`/`vmaCreateImage` which are available via deps/backend helpers defined earlier.
- **If moving `_SituationInitGLVirtualDisplayRenderer` breaks the GL init sequence:** The forward declaration at line 428 ensures `_SituationInitOpenGL` can still call it regardless of where the definition lives.
