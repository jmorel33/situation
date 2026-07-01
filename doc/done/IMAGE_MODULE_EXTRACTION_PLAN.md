# Image Module Extraction Plan

**Date:** 2026-05-04  
**Status:** Planned  
**Target Version:** 2.4.7 (combined with WDM)  
**Prerequisite:** WDM extraction complete

## Goal

Extract the Image & Font subsystem from `situation_impl.h` into a new module:
- `sit/situation_impl_image.h` — Image loading/saving, manipulation, font rendering, color conversion, screenshots

Also move stray window/timer functions to their correct homes.

## Target File: `sit/situation_impl_image.h`

### Image Operations
- `SituationLoadImage`, `SituationLoadImageFromMemory`
- `SituationUnloadImage`, `SituationIsImageValid`
- `SituationExportImage`, `SituationCreateImage`
- `SituationBlitRawDataToImage`, `SituationSetPixelColor`
- `SituationImageCopy`, `SituationImageDraw`, `SituationImageDrawAlpha`
- `SituationGenImageColor`, `SituationGenImageGradient`
- `SituationImageCrop`, `SituationImageResize`, `SituationImageFlip`
- `SituationImageAdjustHSV`
- `SituationLoadImageFromScreen`, `SituationTakeScreenshot`

### Font Operations
- `SituationLoadFont`, `SituationLoadFontFromMemory`
- `SituationBakeFontAtlas`, `SituationUnloadFont`
- `SituationImageDrawCodepoint`, `SituationImageDrawTextEx`
- `SituationImageDrawText`, `SituationImageDrawTextFormatted`
- `SituationMeasureText`

### Color Conversion
- `SituationRgbToHsv`, `SituationHsvToRgb`
- `SituationColorFromYPQ`, `SituationColorToYPQ`
- `SituationConvertColorToVector4`

### Internal Helpers
- `_SituationSaveImageBMP`
- `_SituationColorAlphaBlend`
- `_SituationBilinearSample`

### Move to WDM (stray window functions)
- `SituationGetGLFWwindow`, `SituationGetWindowSize`
- `SituationWindowShouldClose`
- `SituationSetTargetFPS`, `SituationGetFrameTime`, `SituationGetFPS`

### Move to WDM (utility)
- `SituationFreeDisplays`

### Stays in situation_impl.h
- Timer/Oscillator API (coupled with timer system state)
- `SituationFreeString` (general utility)
- `_SitFlushFrameResources` (renderer internal)

---

## Execution

### Phase 1: Move stray window functions to WDM
- [ ] Move `SituationGetGLFWwindow`, `SituationGetWindowSize`, `SituationWindowShouldClose` to `situation_impl_wdm.h`
- [ ] Move `SituationSetTargetFPS`, `SituationGetFrameTime`, `SituationGetFPS` to `situation_impl_wdm.h`
- [ ] Move `SituationFreeDisplays` to `situation_impl_wdm.h`
- [ ] Build both backends

### Phase 2: Extract Image Module
- [ ] Create `sit/situation_impl_image.h`
- [ ] Move all image, font, color, and screenshot functions
- [ ] Add `#include "situation_impl_image.h"` in `situation_impl.h` (after WDM)
- [ ] Build both backends

### Phase 3: Clean Build & Verify
- [ ] Clean build passes
- [ ] Verify line counts

---

## Constraints
- Include after WDM (screenshots may reference window state)
- Image functions use `SituationLoadFileData` (in IO module — already included before)
- Font functions use `SIT_MALLOC`/`SIT_FREE` (in deps — fine)
- Screenshot uses GL/VK calls — needs to be after backend forward declarations
