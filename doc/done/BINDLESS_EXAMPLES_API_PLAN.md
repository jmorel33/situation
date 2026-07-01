# Bindless & Compute Examples API Plan

## Overview

This plan tracks the API additions and patterns required to support the compute/bindless
example suite (`atascii_example`, `confetti_example`, `fox_example`, `GlyphAPI` system).
Originally captured as `doc/situation_proposal.md`. All items have since been implemented.

---

## Status Summary

| Item | Status |
|------|--------|
| `SituationCmdPresent` | ✅ Implemented |
| `SituationGetBufferDeviceAddress` | ✅ Implemented |
| `SituationGetTextureHandle` | ✅ Implemented (VK: bindless descriptor index; GL: NV bindless handle) |
| `SituationCmdBindSampledTexture` | ✅ Implemented |
| `SituationCreateImage` | ✅ Implemented |
| `SituationBlitRawDataToImage` | ✅ Implemented |
| `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` | ✅ Implemented (enum value added to `SituationComputeLayoutType`) |
| Bindless documented as core feature | ✅ `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` flag + `SIT_FEATURE_BINDLESS_BUFFERS` |
| Best practice: prefer `SituationGenImageColor` | ✅ Function exists; pattern documented here |
| Best practice: manual 1bpp font atlas expansion | ✅ Pattern documented here |
| Best practice: UBO struct alignment (mirror GLSL layout) | ✅ Pattern documented here |

---

## 1. API Additions (All Implemented)

### 1.1 `SituationCmdPresent`

- [x] Declare in `sit/situation_api.h`
- [x] Implement in `sit/situation_impl_renderer.h` (GL + VK paths)

**Purpose:** Blit an arbitrary storage texture to the swapchain. Required for compute-only
pipelines that never go through a raster render pass.

```c
SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);
```

### 1.2 `SituationGetBufferDeviceAddress`

- [x] Declare in `sit/situation_api.h`
- [x] Implement: VK → `vkGetBufferDeviceAddress`; GL → `GL_NV_shader_buffer_load` (NVIDIA only)
- [x] Non-NV GL path sets `SITUATION_ERROR_OPENGL_UNSUPPORTED` and returns 0 (v2.4.259)
- [x] Callers should gate on `SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)`

```c
uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);
```

### 1.3 `SituationGetTextureHandle`

- [x] Declare in `sit/situation_api.h`
- [x] Implement: VK → descriptor index into `global_bindless_set`; GL → `GL_ARB_bindless_texture` resident handle
- [x] Stale-handle guard: returns 0 + `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` (v2.4.259)
- [x] Callers should gate on `SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)`

```c
uint64_t SituationGetTextureHandle(SituationTexture texture);
```

### 1.4 `SituationCmdBindSampledTexture`

- [x] Declare in `sit/situation_api.h`
- [x] Implement (GL + VK): binds as `sampler2D` / `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`

Distinct from `SituationCmdBindComputeTexture` which binds `image2D` (storage, read/write).
Required for shadow-map sampling in compute shaders (`fox_example`).

```c
SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```

### 1.5 `SituationCreateImage`

- [x] Declare in `sit/situation_api.h`
- [x] Returns uninitialized pixel data — use `SituationGenImageColor` when zeroed memory is needed

```c
SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);
```

### 1.6 `SituationBlitRawDataToImage`

- [x] Declare in `sit/situation_api.h`
- [x] Implement: copies raw bytes into a rectangular region of a CPU-side image
- [x] Used for packing 1bpp font data into RGBA texture atlases

```c
void SituationBlitRawDataToImage(SituationImage* dst, const void* data, int x, int y,
                                  int width, int height, int src_channels);
```

---

## 2. Structural Additions (All Implemented)

### 2.1 `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE`

- [x] Added to `SituationComputeLayoutType` enum in `sit/situation_api.h`

Supports the pattern of one SSBO at Set 0 and one Storage Image at Set 1. Replaces the
old `BUFFER_X2` workaround that was used as a stopgap in early example code.

Current full enum (for reference):
```c
SIT_COMPUTE_LAYOUT_ONE_SSBO          // 1 SSBO (Set 0)
SIT_COMPUTE_LAYOUT_TWO_SSBOS         // 2 SSBOs (Set 0, Set 1)
SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO    // Storage Image (Set 0) + SSBO (Set 1)
SIT_COMPUTE_LAYOUT_PUSH_CONSTANT     // 64-byte push constant, no resources
SIT_COMPUTE_LAYOUT_EMPTY             // No external resources
SIT_COMPUTE_LAYOUT_BUFFER_IMAGE      // SSBO (Set 0) + Storage Image (Set 1)
SIT_COMPUTE_LAYOUT_TERMINAL          // Terminal emulator layout
SIT_COMPUTE_LAYOUT_VECTOR            // Vector/path rendering layout
```

### 2.2 Bindless as a Core Feature

- [x] `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` flag exists in `SituationBufferUsageFlags`
- [x] `SITUATION_BUFFER_USAGE_STORAGE_COMPUTE` convenience preset includes `DEVICE_ADDRESS`
- [x] `SIT_FEATURE_BINDLESS_BUFFERS` and `SIT_FEATURE_BINDLESS_TEXTURES` queryable via `SituationIsFeatureSupported`
- [x] Minimum requirements: Vulkan 1.2+ (bufferDeviceAddress core) or `GL_ARB_bindless_texture` / `GL_NV_shader_buffer_load`

---

## 3. Best Practices (Documented for Example Authors)

### 3.1 Image Initialization

Prefer `SituationGenImageColor` over `SituationCreateImage` unless every pixel will be
overwritten — the former zeroes memory and avoids garbage artifacts.

```c
// Prefer this:
SituationImage img;
SituationGenImageColor(512, 512, (ColorRGBA){0,0,0,0}, &img);

// Only use CreateImage when you will overwrite every pixel:
SituationImage img;
SituationCreateImage(512, 512, 4, &img);
```

### 3.2 Manual 1bpp Font Atlas Expansion

Raw 1-bit-per-pixel font data cannot be `memcpy`'d into an RGBA atlas. Expand explicitly:

```c
for (int y = 0; y < glyph_h; ++y) {
    for (int x = 0; x < glyph_w; ++x) {
        int bit = (raw_byte >> (7 - x)) & 1;
        ColorRGBA col = bit ? (ColorRGBA){255,255,255,255} : (ColorRGBA){0,0,0,0};
        SituationSetPixelColor(&atlas, origin_x + x, origin_y + y, col);
    }
}
```

### 3.3 UBO / Buffer Reference Struct Alignment

Mirror the GLSL `layout(std430)` block exactly on the C side, including any wrapper structs:

```c
// C side — must match GLSL block layout exactly
typedef struct { GlyphAPI api; } PushConstants;
SituationBuffer ubo = SituationCreateBuffer(SITUATION_BUFFER_USAGE_UNIFORM_BUFFER,
                                            &pc, sizeof(PushConstants));

// GLSL
layout(buffer_reference, std430) buffer PushConstants { GlyphAPI api; };
```

Using bare `sizeof(GlyphAPI)` when the shader expects a `PushConstants` wrapper will
produce a size mismatch and undefined reads in the shader.

---

## 4. Residual Follow-Ups

These were flagged as open questions in the original proposal. They have answers now:

- **`BUFFER_X2` workaround** — obsolete. Use `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` or
  `SIT_COMPUTE_LAYOUT_TWO_SSBOS` as appropriate. `BUFFER_X2` never existed in the enum;
  it was a naming suggestion in the proposal that was superseded before being adopted.

- **Bindless formalization** — done. Feature flags + usage flags + error codes are all
  wired. The requirement (VK 1.2+ / NV GL extension) is enforced at the call site with
  a proper `SITUATION_ERROR_OPENGL_UNSUPPORTED` return and a `SIT_FEATURE_*` query API.

- **stale `situation_api.md` entry for `BUFFER_X2`** — the old API doc still references
  `SIT_COMPUTE_LAYOUT_BUFFER_X2` which was never real. That doc entry should be removed
  or corrected to `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` during the next doc refresh.
  - [x] Clean up stale `BUFFER_X2` entry in `doc/situation_api.md` (§ Compute Layout table)

---

## 5. Plan Status: COMPLETE

All items implemented and documentation corrected. This plan can be considered closed.
