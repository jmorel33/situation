# Situation SDK Enhancement Proposal

## Overview
This document serves as a detailed proposal for enhancing the `situation` SDK. It identifies critical gaps between the current documentation and the functionality required by the provided examples. The proposal formalizes the missing API functions, clarifying their signatures, parameters, and intended usage, and suggests architectural improvements for better type safety and clarity.

## 1. Proposed API Additions

The following functions are essential for the operation of the example suite but are currently undocumented or missing from the public API headers.

### Graphics Module

#### `SituationCmdPresent`
**Signature:**
```c
void SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);
```
*   **Description:**
    Submits a command to copy a specified texture to the window's swapchain image. This function acts as a bridge between off-screen rendering (e.g., compute shaders writing to storage images) and the presentation engine.
*   **Parameters:**
    *   `cmd` (SituationCommandBuffer): The active command buffer where the command is recorded.
    *   `texture` (SituationTexture): The source texture containing the final rendered frame.
*   **Why it is needed:**
    The SDK's standard `SituationEndFrame` function typically presents the swapchain image. In compute-only pipelines (like `atascii_example` or `confetti_example`), the "render" pass writes to an arbitrary storage texture (`output_texture`). Without an explicit "blit-to-swapchain" command, `SituationEndFrame` would present an empty or undefined backbuffer.
*   **Usage Example:**
    ```c
    // Compute shader writes to 'output_texture'
    SituationCmdDispatch(cmd, grid_w, grid_h, 1);

    // Copy the result to the screen
    SituationCmdPresent(cmd, output_texture);

    // Submit and swap buffers
    SituationEndFrame();
    ```

#### `SituationGetBufferDeviceAddress`
**Signature:**
```c
uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);
```
*   **Description:**
    Retrieves the 64-bit physical GPU device address of a buffer. This address allows shaders to access the buffer's memory directly via the `GL_EXT_buffer_reference` extension (bindless access).
*   **Parameters:**
    *   `buffer` (SituationBuffer): The buffer handle to query.
*   **Returns:**
    *   `uint64_t`: The 64-bit device address.
*   **Why it is needed:**
    The `GlyphAPI` system relies entirely on a "bindless" data architecture where a single Uniform Buffer Object (UBO) contains pointers (addresses) to other buffers (materials, glyphs, grids). This avoids the need to bind multiple descriptor sets and allows for complex, pointer-based data structures in GLSL.
*   **Usage Example:**
    ```c
    SituationBuffer data_buffer = SituationCreateBuffer(SIT_BUFFER_USAGE_STORAGE, data, size);

    // Populate the API struct with the address
    GlyphAPI api;
    api.materialData = SituationGetBufferDeviceAddress(data_buffer);

    SituationUpdateBuffer(api_ubo, &api, sizeof(GlyphAPI));
    ```

#### `SituationGetTextureHandle`
**Signature:**
```c
uint64_t SituationGetTextureHandle(SituationTexture texture);
```
*   **Description:**
    Retrieves the 64-bit bindless handle (resident handle) for a texture. This allows shaders to sample the texture without it being bound to a specific binding point in the descriptor set.
*   **Parameters:**
    *   `texture` (SituationTexture): The texture handle to query.
*   **Returns:**
    *   `uint64_t`: The 64-bit texture handle.
*   **Why it is needed:**
    Similar to buffers, textures in the `GlyphAPI` system (like font atlases or material maps) are passed to shaders as 64-bit handles within the UBO. This decouples the shader logic from specific texture binding slots (`layout(binding=X)`).
*   **Usage Example:**
    ```c
    SituationTexture atlas = SituationCreateTexture(image, false);

    GlyphAPI api;
    api.bitmapFontAtlas = SituationGetTextureHandle(atlas);
    // Shader does: sampler2D(api.bitmapFontAtlas)
    ```

#### `SituationCmdBindSampledTexture`
**Signature:**
```c
void SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```
*   **Description:**
    Binds a texture to a specific binding point for use as a *sampled image* (combined image-sampler) in a shader pipeline.
*   **Parameters:**
    *   `cmd`: The command buffer.
    *   `binding` (int): The shader binding point index (e.g., `layout(binding=22) uniform sampler2D ...`).
    *   `texture`: The texture to bind.
*   **Why it is needed:**
    While `SituationCmdBindComputeTexture` exists for *storage images* (`image2D`, read/write), the examples (specifically `fox_example.c` for shadow mapping) require binding a texture for *sampling* (`sampler2D`, read-only with filtering) within a compute shader. This distinction is crucial in APIs like Vulkan.
*   **Usage Example:**
    ```c
    // Bind shadow map for reading in the detection pass
    SituationCmdBindSampledTexture(cmd, 22, shadow_cubemap);
    ```

### Image Module

#### `SituationCreateImage`
**Signature:**
```c
SituationImage SituationCreateImage(int width, int height, int channels);
```
*   **Description:**
    Allocates a new `SituationImage` container with the specified dimensions and channel count. The pixel data is allocated but **uninitialized**.
*   **Parameters:**
    *   `width`, `height` (int): Dimensions in pixels.
    *   `channels` (int): Number of color channels (1-4).
*   **Returns:**
    *   `SituationImage`: The new image struct.
*   **Why it is needed:**
    Used for creating intermediate CPU-side buffers, such as texture atlases that will be populated procedurally. Note: `SituationGenImageColor` is safer as it initializes memory, but `CreateImage` is standard for performance when every pixel will be overwritten.

#### `SituationBlitRawDataToImage`
**Signature:**
```c
void SituationBlitRawDataToImage(SituationImage dst, const void* data, int x, int y, int width, int height, int channels);
```
*   **Description:**
    Copies raw pixel data from a memory buffer into a specific rectangular region of a destination image.
*   **Parameters:**
    *   `dst` (SituationImage): The destination image.
    *   `data` (void*): Pointer to the raw source data.
    *   `x`, `y`: Top-left coordinates in the destination.
    *   `width`, `height`: Dimensions of the region to copy.
    *   `channels`: Number of channels in the source data.
*   **Why it is needed:**
    Essential for loading font data where the source is a raw byte array (like `font_data.h`) and needs to be uploaded into a larger texture atlas.

## 2. Best Practices & Patterns

Derived from the corrections applied to the example suite, the following patterns should be adopted for all future development:

### 1. Robust Image Initialization
*   **Problem:** Using `SituationCreateImage` leaves pixel data uninitialized, leading to visual noise or garbage artifacts if the image isn't completely overwritten.
*   **Best Practice:** Prefer **`SituationGenImageColor`** to create images initialized to a known state (e.g., transparent black).
    ```c
    // BAD: Uninitialized memory
    SituationImage img = SituationCreateImage(512, 512, 4);

    // GOOD: Zeroed memory
    SituationImage img = SituationGenImageColor(512, 512, (ColorRGBA){0,0,0,0});
    ```

### 2. Manual Font Atlas Expansion
*   **Problem:** 1-bit-per-pixel font data cannot be simply `memcpy`'d into an RGBA texture.
*   **Best Practice:** Use an explicit loop to expand bits into full colors using `SituationSetPixelColor`.
    ```c
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            int bit = (raw_byte >> (7-x)) & 1;
            ColorRGBA col = bit ? (ColorRGBA){255,255,255,255} : (ColorRGBA){0,0,0,0};
            SituationSetPixelColor(&img, atlas_x + x, atlas_y + y, col);
        }
    }
    ```

### 3. Compute Pipeline Layouts
*   **Problem:** The `SituationComputeLayoutType` enum does not currently support a "Buffer + Storage Image" layout, which is common for render pipelines.
*   **Workaround:**
    *   Use `SIT_COMPUTE_LAYOUT_BUFFER` for pipelines that only bind a UBO/SSBO.
    *   Use `SIT_COMPUTE_LAYOUT_BUFFER_X2` as a convention for pipelines that need to bind a UBO *and* an Output Texture, acknowledging this is a fallback until `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` is added.

### 4. UBO Structure Alignment
*   **Problem:** Using `sizeof(GlyphAPI)` for UBO creation can lead to mismatches if the GLSL shader expects a wrapper struct (like `PushConstants` in `examples_common.glslh`).
*   **Best Practice:** Always define a C-side struct that mirrors the GLSL `layout(std430)` block exactly, including any wrapper structs.
    ```c
    // C
    typedef struct PushConstants { GlyphAPI api; } PushConstants;
    SituationCreateBuffer(..., sizeof(PushConstants));

    // GLSL
    layout(buffer_reference) buffer PushConstants { GlyphAPI api; };
    ```

## 3. Structural Recommendations

To resolve the layout ambiguity and improve type safety:

1.  **Add `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE`:** Explicitly support the pattern of "One Uniform Buffer (Binding 0) + One Storage Image (Binding 0/1)".
2.  **Formalize Bindless:** Since the SDK relies heavily on bindless addresses (`SituationGetBufferDeviceAddress`), the documentation should explicitly mark this as a core feature requirement (e.g., requiring Vulkan 1.2+ or `GL_ARB_bindless_texture`).
