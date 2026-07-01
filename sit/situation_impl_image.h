/***************************************************************************************************
*
*   situation_impl_image.h - Image, Font & Color Module Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h after situation_impl_wdm.h.
*
*   Contains:
*     - Image loading, saving, creation, manipulation (crop, resize, flip, blit)
 *     - Font loading, grid/bitmap atlas builders, baking, text rendering to images
 *     - Color space conversion — implemented in situation_impl_color.h
*     - Screenshot capture
*     - Timer/Oscillator API
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_IMAGE_H
#define SITUATION_IMPL_IMAGE_H

#include "situation_impl_color.h"

/** stb_image decode extensions accepted by SituationLoadImage / SituationLoadTexture. */
static bool _SituationIsStbImageLoadExtensionImpl(const char* extension) {
    if (!extension || extension[0] == '\0') {
        return false;
    }
    if (extension[0] == '.') {
        extension++;
    }
    static const char* const k_stb_load_exts[] = {
        "jpg", "jpeg", "png", "bmp", "tga", "psd", "gif", "hdr", "pic", "ppm", "pgm", "pnm", NULL
    };
    for (int i = 0; k_stb_load_exts[i] != NULL; ++i) {
        if (_sit_strcasecmp(extension, k_stb_load_exts[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Returns true when `extension` is decoded by SituationLoadImage via stb_image.
 * @param extension File extension with or without a leading dot (e.g. ".png" or "png").
 */
SITAPI bool SituationIsStbImageLoadExtension(const char* extension) {
    return _SituationIsStbImageLoadExtensionImpl(extension);
}

/**
 * @brief Loads an image from a file into a CPU-side memory buffer.
 * @details This function uses the stb_image library to load common image formats from disk.
 *          Supported extensions: JPEG (.jpg, .jpeg), PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM (.ppm, .pgm, .pnm).
 *          Output is always converted to 32-bit RGBA for consistency across the library.
 *
 * @warning This function requires the `stb_image.h` implementation to be included in the project. If not available, the function will fail and set an error.
 * @warning This function allocates new memory for the `image.data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()` on the returned `SituationImage`. Failure to do so will result in a memory leak.
 *
 * @param fileName The file system path to the image file to load.
 *
 * @return A new `SituationImage` containing the pixel data, width, and height.
 * @return A zeroed (invalid) `SituationImage` if the file cannot be found, the format is unsupported, or a memory allocation error occurs.
 *
 * @see SituationUnloadImage(), SituationLoadImageFromMemory(), SituationCreateTexture()
 */
SITAPI SituationError SituationLoadImage(const char *fileName, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

#if defined(STB_IMAGE_IMPLEMENTATION)
    int channels;
    // We force 4 channels (RGBA) for consistency across the library.
    out_image->data = stbi_load(fileName, &out_image->width, &out_image->height, &channels, 4);
    if (out_image->data == NULL) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_ACCESS, "Failed to load image file or format not supported.");
    }
    out_image->channels = 4;
    out_image->color_encoding = SITUATION_COLOR_SRGB;  // Default to SRGB for proper gamma correction
#else
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Image loading not available. Please implement stb_image.h.");
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Loads an image from a memory buffer into a `SituationImage`.
 * @details This function is useful for loading image data that is already in memory, such as data embedded in the executable or loaded from a custom archive file. It uses stb_image to auto-detect the format from the buffer (same set as SituationLoadImage) and decodes it. The image is always converted to a 32-bit RGBA format.
 *
 * @warning This function requires the `stb_image.h` implementation to be included in the project.
 * @warning This function allocates new memory for the `image.data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()`.
 *
 * @param fileType A hint for the file format (e.g., ".png"). This is currently ignored as stb_image auto-detects the format from the data.
 * @param fileData A pointer to the buffer containing the raw, compressed image file data.
 * @param dataSize The size of the `fileData` buffer in bytes.
 *
 * @return A new `SituationImage` containing the decoded pixel data, width, and height.
 * @return A zeroed (invalid) `SituationImage` if the data is corrupt, the format is unsupported, or a memory allocation error occurs.
 *
 * @see SituationUnloadImage(), SituationLoadImage()
 */
SITAPI SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image) {
    (void)fileType; // fileType is mainly for hinting, stbi_load_from_memory auto-detects.
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

#if defined(STB_IMAGE_IMPLEMENTATION)
    int channels;
    out_image->data = stbi_load_from_memory(fileData, dataSize, &out_image->width, &out_image->height, &channels, 4);
    if (out_image->data == NULL) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Failed to load image from memory or format not supported.");
    }
    out_image->channels = 4;
    out_image->color_encoding = SITUATION_COLOR_SRGB;  // Default to SRGB for proper gamma correction
#else
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Image loading not available. Please implement stb_image.h.");
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Frees the CPU memory allocated for an image's pixel data.
 * @details This is the designated cleanup function for any `SituationImage` whose `data` member was allocated by the library (e.g., via `SituationLoadImage`, `SituationImageCopy`, or `SituationLoadImageFromScreen`). It ensures that the memory is correctly released.
 *
 * @param image The `SituationImage` whose pixel data buffer should be freed. The `data` pointer becomes invalid after this call.
 *
 * @note This function only frees the CPU-side pixel buffer (`image.data`). It does not affect any GPU texture created from this image. Use `SituationDestroyTexture` for GPU resources.
 * @note It is safe to call this function on an image whose `data` pointer is already NULL.
 */
SITAPI void SituationUnloadImage(SituationImage image) {
    if (image.data != NULL) {
        SIT_FREE(image.data);
        // Standard free() returns void and does not set errno.
        // We assume success if the pointer was valid.
    }
}

/**
 * @brief Checks if a `SituationImage` handle contains valid, usable data.
 * @details A valid image is defined as having a non-NULL data pointer and both width and height greater than zero.
 *
 * @param image The `SituationImage` to check.
 *
 * @return `true` if the image is valid, `false` otherwise.
 */
SITAPI bool SituationIsImageValid(SituationImage image) {
    return (image.data != NULL && image.width > 0 && image.height > 0);
}

/**
 * @brief Saves a CPU-side `SituationImage` to a file on disk.
 * @details This function determines the output format based on the file extension of `fileName`.
 *
 * @par Supported Formats
 *   - **`.png`:** Compressed PNG via stb_image_write.
 *   - **`.bmp`:** Uncompressed BMP (native encoder fallback).
 *   - **`.jpg` / `.jpeg`:** JPEG via stb_image_write (quality 90).
 *   - **`.tga`:** TGA via stb_image_write.
 *   - **`.hdr`:** Radiance RGBE HDR via stb_image_write (8-bit RGBA source is normalized to float).
 *
 * @param image The `SituationImage` to save. The image must be valid.
 * @param fileName The destination file path, including the desired extension (e.g., "output/my_image.png").
 *
 * @return `true` if the image was successfully saved, `false` otherwise. An error will be set on failure (e.g., unsupported format, file I/O error).
 *
 * @see SituationLoadImage(), SituationTakeScreenshot()
 */
SITAPI SituationError SituationExportImage(SituationImage image, const char *fileName) {
    if (!SituationIsImageValid(image) || !fileName) return SITUATION_ERROR_INVALID_PARAM;

    const char *ext = SituationGetFileExtension(fileName);

    if (ext != NULL && _sit_strcasecmp(ext, ".png") == 0) {
#if defined(STB_IMAGE_WRITE_IMPLEMENTATION)
        if (stbi_write_png(fileName, image.width, image.height, 4, image.data, image.width * 4) != 0) {
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Failed to write PNG image.");
#else
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "PNG export not available. Please implement stb_image_write.h.");
#endif
    }
    if (ext != NULL && _sit_strcasecmp(ext, ".bmp") == 0) {
        return _SituationSaveImageBMP(fileName, &image);
    }
#if defined(STB_IMAGE_WRITE_IMPLEMENTATION)
    if (ext != NULL && (_sit_strcasecmp(ext, ".jpg") == 0 || _sit_strcasecmp(ext, ".jpeg") == 0)) {
        if (stbi_write_jpg(fileName, image.width, image.height, 4, image.data, 90) != 0) {
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Failed to write JPEG image.");
    }
    if (ext != NULL && _sit_strcasecmp(ext, ".tga") == 0) {
        if (stbi_write_tga(fileName, image.width, image.height, 4, image.data) != 0) {
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Failed to write TGA image.");
    }
    if (ext != NULL && _sit_strcasecmp(ext, ".hdr") == 0) {
        const int pixel_count = image.width * image.height;
        float* float_data = (float*)SIT_MALLOC((size_t)pixel_count * 3u * sizeof(float));
        if (!float_data) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        const unsigned char* src = (const unsigned char*)image.data;
        for (int i = 0; i < pixel_count; ++i) {
            const int si = i * 4;
            const int di = i * 3;
            float_data[di + 0] = _SitSrgbUnitToLinear((float)src[si + 0] / 255.0f);
            float_data[di + 1] = _SitSrgbUnitToLinear((float)src[si + 1] / 255.0f);
            float_data[di + 2] = _SitSrgbUnitToLinear((float)src[si + 2] / 255.0f);
        }
        const int wrote = stbi_write_hdr(fileName, image.width, image.height, 3, float_data);
        SIT_FREE(float_data);
        if (wrote != 0) {
            return SITUATION_SUCCESS;
        }
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Failed to write HDR image.");
    }
#endif
    return _SituationSetErrorFromCode(
        SITUATION_ERROR_INVALID_PARAM,
        "Unsupported image export format. Use .png, .bmp, .jpg, .tga, or .hdr.");
}

/**
 * @brief Creates a new empty image with the specified dimensions and channel count.
 *
 * @details Allocates and initializes a new `SituationImage` handle representing a 2D pixel buffer
 *          in system memory (CPU-accessible). The image is filled with fully transparent black
 *          (0,0,0,0) by default.
 *
 *          This function is the primary way to create blank or procedural images that can later
 *          be filled via `SituationSetPixelColor`, `SituationBlitRawDataToImage`, or drawing
 *          commands. The resulting image can be used as a texture, render target, or data source.
 *
 *          Supported channel counts:
 *            - 1: Grayscale (luminance only)
 *            - 3: RGB
 *            - 4: RGBA (most common for textures and blending)
 *
 *          The image data is allocated contiguously in row-major order (stride = width channels).
 *          Pixel type is always unsigned 8-bit normalized (0-255 per channel).
 *
 * @param width Width of the image in pixels. Must be > 0.
 * @param height Height of the image in pixels. Must be > 0.
 * @param channels Number of channels per pixel (1, 3, or 4). Other values return an error.
 * @param out_image Pointer to a `SituationImage` variable that will receive the new handle on success.
 *                  On failure, the value is set to `SITUATION_NULL_HANDLE`.
 *
 * @return SITUATION_SUCCESS on successful allocation and initialization,
 *         SITUATION_ERROR_INVALID_PARAM if width/height 0 or channels invalid,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if system memory allocation failed,
 *         or other appropriate error codes (e.g. resource limit reached).
 *
 * @note The created image is CPU-resident and does not automatically upload to the GPU.
 *       To use it as a texture, call `SituationUploadImageToTexture` (or equivalent) after filling.
 *       Caller is responsible for destroying the image with `SituationDestroyImage` when no longer needed.
 *       Maximum dimensions are implementation-defined (typically limited by available memory).
 *
 * @see SituationDestroyImage, SituationBlitRawDataToImage, SituationSetPixelColor,
 *      SituationCreateImageFromMemory, SituationUploadImageToTexture (if defined)
 */
SITAPI SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

    if (width <= 0 || height <= 0 || channels <= 0) return SITUATION_ERROR_INVALID_PARAM;

    out_image->width = width;
    out_image->height = height;
    out_image->channels = channels;
    // Note: Library generally assumes 4-channel (RGBA) for GPU upload,
    // but we allocate based on request for intermediate buffers.
    size_t size = (size_t)width * height * channels;
    out_image->data = SIT_MALLOC(size);

    if (!out_image->data) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationCreateImage failed");
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Copies raw pixel data into a rectangular region of a destination image.
 *
 * @details Performs a direct blit (memory copy) of uncompressed pixel data from a user-provided
 *          buffer into the specified rectangular area of the target `SituationImage`.
 *          This is a low-level, CPU-side operation ideal for procedural texture generation,
 *          CPU-based image editing, uploading uncompressed data, or updating small regions
 *          of an existing image/texture without full re-upload.
 *
 *          The source data is assumed to be tightly packed or row-strided with the given
 *          number of channels. No format conversion, scaling, or filtering is performed
 *          the source channel count must match the destination image's internal format
 *          (typically 4 channels for RGBA).
 *
 *          Bounds checking is performed: coordinates and dimensions are clamped to the
 *          destination image size. Out-of-bounds regions are ignored.
 *
 * @param dst Pointer to the destination `SituationImage` handle (must be valid and writable).
 *            The image must support direct pixel access (e.g., CPU-resident or staging texture).
 * @param data Pointer to the source pixel buffer (must remain valid for the duration of the call).
 *             Data is read row-by-row, left-to-right, top-to-bottom.
 * @param x Destination x-coordinate (left edge of blit region, 0-based).
 * @param y Destination y-coordinate (top edge of blit region, 0-based).
 * @param width Width of the region to copy (pixels). Must be > 0.
 * @param height Height of the region to copy (pixels). Must be > 0.
 * @param src_channels Number of channels per pixel in the source data (usually 3 for RGB or 4 for RGBA).
 *                     Must match the channel count of the destination image format.
 *
 * @note This function is synchronous and performs a CPU memory copy.
 *       - For large regions or frequent updates, consider staging data and using GPU upload paths.
 *       - No stride/pitch parameter is provided, source data is assumed to be tightly packed
 *         (stride = width src_channels sizeof(channel_type), typically uint8_t).
 *       - If the destination image is GPU-resident only, this function may fail or trigger
 *         an internal staging/upload (implementation-defined).
 *
 * @return None (void). Errors are logged internally via SITUATION_LOG_WARNING and may set
 *         the global error state, but do not abort execution. Invalid parameters result
 *         in a no-op.
 *
 * @see SituationSetPixelColor, SituationCreateImageFromMemory,
 *      SIT_RGBA, SITUATION_PIXEL_FORMAT_RGBA8 (if defined)
 */
SITAPI void SituationBlitRawDataToImage(SituationImage *dst, const void* data, int x, int y, int width, int height, int src_channels) {
    if (!dst || !dst->data || !data) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationBlitRawDataToImage: dst, dst->data, or data is NULL"); return; }

    // Destination channels
    int dst_channels = (dst->channels > 0) ? dst->channels : 4;
    unsigned char* dst_pixels = (unsigned char*)dst->data;
    const unsigned char* src_pixels = (const unsigned char*)data;

    for (int row = 0; row < height; ++row) {
        int dst_y = y + row;
        if (dst_y >= dst->height) break;

        for (int col = 0; col < width; ++col) {
            int dst_x = x + col;
            if (dst_x >= dst->width) break;

            int dst_idx = (dst_y * dst->width + dst_x) * dst_channels;
            int src_idx = (row * width + col) * src_channels;

            if (src_channels == 1) {
                // Expand grayscale to White + Alpha 255 (if dst is RGBA)
                unsigned char val = src_pixels[src_idx];
                if (dst_channels >= 3) {
                    dst_pixels[dst_idx + 0] = val;
                    dst_pixels[dst_idx + 1] = val;
                    dst_pixels[dst_idx + 2] = val;
                }
                if (dst_channels > 3) dst_pixels[dst_idx + 3] = 255;
            } else if (src_channels == 3) {
                if (dst_channels >= 3) {
                    dst_pixels[dst_idx + 0] = src_pixels[src_idx + 0];
                    dst_pixels[dst_idx + 1] = src_pixels[src_idx + 1];
                    dst_pixels[dst_idx + 2] = src_pixels[src_idx + 2];
                }
                if (dst_channels > 3) dst_pixels[dst_idx + 3] = 255;
            } else if (src_channels == 4) {
                if (dst_channels >= 3) {
                    dst_pixels[dst_idx + 0] = src_pixels[src_idx + 0];
                    dst_pixels[dst_idx + 1] = src_pixels[src_idx + 1];
                    dst_pixels[dst_idx + 2] = src_pixels[src_idx + 2];
                }
                if (dst_channels > 3) dst_pixels[dst_idx + 3] = src_pixels[src_idx + 3];
            }
        }
    }
}

/**
 * @brief Sets the color of a single pixel in an image or texture.
 *
 * @details Writes an RGBA color value directly to the specified pixel coordinates
 *          in the target image's pixel buffer. The function performs bounds checking
 *          and clamps coordinates to the image dimensions.
 *
 *          This is a low-level, immediate-mode pixel write suitable for procedural
 *          generation, debug drawing, or one-off modifications. For bulk operations
 *          prefer `SituationBlitRawDataToImage` or command-buffer drawing.
 *
 *          The image must have been created with a format that supports direct pixel
 *          access (typically RGBA8 or similar). Compressed or GPU-only formats may
 *          return an error.
 *
 * @param image The handle of the image/texture to modify (created via SituationCreateImage
 *              or similar). Must be valid and writable.
 * @param x Horizontal pixel coordinate (0 = left edge).
 * @param y Vertical pixel coordinate (0 = top edge, assuming top-left origin).
 * @param color The RGBA color to set (0-255 per channel). Use SIT_RGBA(r,g,b,a) macro
 *              or equivalent for convenience.
 *
 * @return SITUATION_SUCCESS on success,
 *         SITUATION_ERROR_INVALID_PARAM if image is invalid or coordinates out of bounds,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format does not support direct writes,
 *         or other appropriate error codes.
 *
 * @note This function is synchronous and CPU-bound - avoid calling it in tight loops
 *       on large images. For performance-critical pixel work, consider staging data
 *       in a CPU buffer and blitting once, or using compute shaders.
 *
 * @see SituationBlitRawDataToImage, SituationGetPixelColor (if implemented),
 *      SIT_RGBA, SITUATION_ERROR_xxx codes
 */
SITAPI void SituationSetPixelColor(SituationImage *img, int x, int y, ColorRGBA col) {
    if (!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height) return;

    // Assumes 4-channel (RGBA) which is the standard for SituationImage intended for Texture creation
    // Or check img->channels if available. For now, assume standard behavior or safety check.
    int chans = (img->channels > 0) ? img->channels : 4;
    if (chans < 3) return; // Can't set RGB on 1-channel easily without mapping

    unsigned char* ptr = (unsigned char*)img->data + (y * img->width + x) * chans;
    ptr[0] = col.r;
    ptr[1] = col.g;
    ptr[2] = col.b;
    if (chans > 3) ptr[3] = col.a;
}

/**
 * @brief Creates a new `SituationImage` by making a deep copy of another.
 * @details This function allocates a new memory buffer and copies the entire pixel data from the source image into it, creating a completely independent duplicate.
 *
 * @warning This function allocates new memory for the returned image's `data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()`.
 *
 * @param image The source `SituationImage` to copy.
 *
 * @return A new, independent `SituationImage`.
 * @return A zeroed (invalid) `SituationImage` if the source image is invalid or if memory allocation fails.
 *
 * @see SituationUnloadImage(), SituationImageCrop()
 */
SITAPI SituationError SituationImageCopy(SituationImage image, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

    if (!SituationIsImageValid(image)) return SITUATION_ERROR_INVALID_PARAM;

    size_t data_size = image.width * image.height * 4;
    out_image->data = SIT_MALLOC(data_size);

    if (out_image->data) {
        memcpy(out_image->data, image.data, data_size);
        out_image->width = image.width;
        out_image->height = image.height;
    } else {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Image copy failed");
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Draws a portion of a source image onto a destination image with an opaque copy (blit).
 * @details This function performs a fast, direct memory copy of a rectangular region of pixels from a source image to a destination image. It does not perform any alpha blending; the source pixels completely overwrite the destination pixels.
 *
 * @par Boundary Handling
 *   The function is robust against invalid coordinates. It automatically calculates the intersection of the source rectangle (clamped to the source image's bounds) and the destination area (clamped to the destination image's bounds) and will only copy the overlapping region. This prevents any out-of-bounds memory access.
 *
 * @param[in,out] dst A pointer to the destination `SituationImage` to be modified.
 * @param src The source `SituationImage` to draw from.
 * @param srcRect The rectangular region within the source image to copy.
 * @param dstPos The top-left `(x, y)` position on the destination image where the `srcRect` will be drawn.
 *
 * @note For drawing with transparency, use `SituationImageDrawAlpha()`.
 *
 * @see SituationImageDrawAlpha()
 */
SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos) {
    // 1. --- Initial Validation ---
    if (!SituationIsImageValid(*dst) || !SituationIsImageValid(src)) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationImageDraw: invalid dst or src image"); return; }

    // 2. --- Calculate the Intersection SitRectangle (The core of the logic) ---

    // First, clip the source rectangle to the source image's bounds.
    int srcClipX = (srcRect.x < 0) ? 0 : srcRect.x;
    int srcClipY = (srcRect.y < 0) ? 0 : srcRect.y;
    int srcClipW = (srcRect.x + srcRect.width > src.width) ? (src.width - srcRect.x) : srcRect.width;
    int srcClipH = (srcRect.y + srcRect.height > src.height) ? (src.height - srcRect.y) : srcRect.height;

    // --- [INTEGER CONVERSION] ---
    // All pixel calculations must use integers. Round the float inputs ONCE.
    int i_dstX = (int)roundf(dstPos.x);
    int i_dstY = (int)roundf(dstPos.y);
    int i_srcX = (int)roundf(srcRect.x);
    int i_srcY = (int)roundf(srcRect.y);
    int i_srcW = (int)roundf(srcRect.width);
    int i_srcH = (int)roundf(srcRect.height);

    // Adjust destination position if source rectangle was clipped from the top-left.
    int dstClipX = i_dstX + (srcClipX - i_srcX);
    int dstClipY = i_dstY + (srcClipY - i_srcY);

    // Now, clip the destination rectangle to the destination image's bounds.
    if (dstClipX < 0) {
        srcClipW += dstClipX; // Reduce width
        srcClipX -= dstClipX; // Move source start point forward
        dstClipX = 0;
    }
    if (dstClipY < 0) {
        srcClipH += dstClipY; // Reduce height
        srcClipY -= dstClipY; // Move source start point down
        dstClipY = 0;
    }

    // Final width/height check against destination's right/bottom edges.
    if (dstClipX + srcClipW > dst->width) {
        srcClipW = dst->width - dstClipX;
    }
    if (dstClipY + srcClipH > dst->height) {
        srcClipH = dst->height - dstClipY;
    }

    // 3. --- Final Check & Pixel Copy ---
    if (srcClipW <= 0 || srcClipH <= 0) {
        return;
    }

    unsigned char *srcPixels = (unsigned char*)src.data;
    unsigned char *dstPixels = (unsigned char*)dst->data;
    const int pixelSize = 4;

    size_t rowWidthInBytes = (size_t)srcClipW * pixelSize;

    // The loop itself doesn't need to change, as it uses the clipped integer values.
    for (int y = 0; y < srcClipH; ++y) {
        unsigned char* srcRowStart = srcPixels + (((size_t)srcClipY + y) * src.width + (size_t)srcClipX) * pixelSize;
        unsigned char* dstRowStart = dstPixels + (((size_t)dstClipY + y) * dst->width + (size_t)dstClipX) * pixelSize;
        memcpy(dstRowStart, srcRowStart, rowWidthInBytes);
    }
}

/**
 * @brief Generates a new CPU-side image filled with a single, solid color.
 * @details This function allocates a new memory buffer and creates a `SituationImage` of the specified dimensions. Every pixel in the image is set to the provided color. This is a common utility for creating placeholder textures, background layers, or base images for further drawing operations.
 *
 * @par Performance Note
 *   The function uses an optimized path that packs the RGBA color into a 32-bit integer and fills the memory buffer in a single loop, which is significantly faster than setting each channel of each pixel individually.
 *
 * @warning This function allocates new memory for the `image.data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()` on the returned `SituationImage`. Failure to do so will result in a memory leak.
 *
 * @param width The width of the image to generate in pixels.
 * @param height The height of the image to generate in pixels.
 * @param color The `ColorRGBA` to fill the image with.
 *
 * @return A new `SituationImage`.
 * @return A zeroed (invalid) `SituationImage` if the width or height are invalid, or if memory allocation fails.
 *
 * @see SituationUnloadImage(), SituationGenImageGradient()
 */
SITAPI SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

    out_image->width = width;
    out_image->height = height;
    size_t data_size = width * height * 4;
    out_image->data = SIT_MALLOC(data_size);

    if (out_image->data) {
        unsigned int* pixels = (unsigned int*)out_image->data;
        // Pack color into an integer for fast filling
        unsigned int c = (unsigned int)color.a << 24 | (unsigned int)color.b << 16 | (unsigned int)color.g << 8 | (unsigned int)color.r;
        for (int i = 0; i < width * height; ++i) {
            pixels[i] = c;
        }
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    return SITUATION_SUCCESS;
}

/**
 * Generates an image with a 4-corner color gradient.
 * @param width The width of the image to generate.
 * @param height The height of the image to generate.
 * @param tl The color for the Top-Left corner.
 * @param tr The color for the Top-Right corner.
 * @param bl The color for the Bottom-Left corner.
 * @param br The color for the Bottom-Right corner.
 * @return A SituationImage containing the gradient. The data must be freed later.
 */
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

    out_image->width = width;
    out_image->height = height;
    out_image->data = SIT_MALLOC(width * height * 4); // 4 bytes per pixel (RGBA)

    if (out_image->data) {
        unsigned char* pixels = (unsigned char*)out_image->data;

        for (int y = 0; y < height; ++y) {
            // Vertical ratio (0.0 at top, 1.0 at bottom)
            float ratio_y = (height > 1) ? (float)y / (float)(height - 1) : 0.0f;

            for (int x = 0; x < width; ++x) {
                // Horizontal ratio (0.0 at left, 1.0 at right)
                float ratio_x = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;

                // 1. Interpolate horizontally along the top edge
                float top_r = (float)tl.r * (1.0f - ratio_x) + (float)tr.r * ratio_x;
                float top_g = (float)tl.g * (1.0f - ratio_x) + (float)tr.g * ratio_x;
                float top_b = (float)tl.b * (1.0f - ratio_x) + (float)tr.b * ratio_x;
                float top_a = (float)tl.a * (1.0f - ratio_x) + (float)tr.a * ratio_x;

                // 2. Interpolate horizontally along the bottom edge
                float bottom_r = (float)bl.r * (1.0f - ratio_x) + (float)br.r * ratio_x;
                float bottom_g = (float)bl.g * (1.0f - ratio_x) + (float)br.g * ratio_x;
                float bottom_b = (float)bl.b * (1.0f - ratio_x) + (float)br.b * ratio_x;
                float bottom_a = (float)bl.a * (1.0f - ratio_x) + (float)br.a * ratio_x;

                // 3. Interpolate vertically between the two results from above
                unsigned char r = (unsigned char)(top_r * (1.0f - ratio_y) + bottom_r * ratio_y);
                unsigned char g = (unsigned char)(top_g * (1.0f - ratio_y) + bottom_g * ratio_y);
                unsigned char b = (unsigned char)(top_b * (1.0f - ratio_y) + bottom_b * ratio_y);
                unsigned char a = (unsigned char)(top_a * (1.0f - ratio_y) + bottom_a * ratio_y);

                // Set the pixel color
                int index = (y * width + x) * 4;
                pixels[index + 0] = r;
                pixels[index + 1] = g;
                pixels[index + 2] = b;
                pixels[index + 3] = a;
            }
        }
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Crops an image in-place to a specified rectangular region.
 * @details This is a destructive operation that modifies the provided `SituationImage` struct.
 *          It allocates a new memory buffer for the cropped pixel data, copies the relevant pixels from the original image, frees the old image data, and then updates the image's `data`, `width`, and `height` members to reflect the new, smaller dimensions.
 *
 * @par Boundary Handling
 *   The function safely handles crop rectangles that are partially or fully outside the original image's bounds. The rectangle is automatically clamped to the valid area of the source image before the crop is performed.
 *   If the resulting intersection is empty (width or height is zero or less), the function does nothing.
 *
 * @param[in,out] image A pointer to the `SituationImage` to be modified.
 * @param crop A `SitRectangle` struct defining the desired area to keep. The `x` and `y` coordinates are the top-left corner of the crop region.
 *
 * @note If the crop operation fails due to a memory allocation error, the original image is left unmodified.
 *
 * @see SituationImageResize(), SituationImageCopy()
 */
SITAPI void SituationImageCrop(SituationImage *image, SitRectangle crop) {
    if (!SituationIsImageValid(*image) || crop.width <= 0 || crop.height <= 0) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationImageCrop: invalid image or crop dimensions <= 0"); return; }

    int x = (int)crop.x;
    int y = (int)crop.y;
    int w = (int)crop.width;
    int h = (int)crop.height;

    // Clamp crop rectangle to image bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > image->width) w = image->width - x;
    if (y + h > image->height) h = image->height - y;
    if (w <= 0 || h <= 0) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationImageCrop: crop rectangle is fully outside image bounds"); return; }

    void* cropped_data = SIT_MALLOC(w * h * 4);
    if (!cropped_data) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationImageCrop"); return; }

    unsigned char* src = (unsigned char*)image->data;
    unsigned char* dst = (unsigned char*)cropped_data;
    for (int j = 0; j < h; ++j) {
        memcpy(dst + (j * w * 4), src + ((y + j) * image->width + x) * 4, w * 4);
    }

    SituationUnloadImage(*image); // Free the old image data
    image->data = cropped_data;
    image->width = w;
    image->height = h;
}

/**
 * @brief Resizes an image in-place to new dimensions using a high-quality algorithm.
 * @details This is a destructive operation that modifies the provided `SituationImage` struct. It allocates a new memory buffer for the resized pixel data, performs the scaling, frees the old image data, and then updates the image's `data`, `width`, and `height` members.
 *
 * @par Resizing Algorithm
 *   This function uses the `stb_image_resize` library, specifically the `stbir_resize_uint8_srgb` function. This ensures a high-quality, perceptually correct resize by operating in a linear color space, which is the proper way to handle sRGB images. This prevents the common issue of resized images appearing too dark or having incorrect color tones.
 *
 * @warning This function requires the `stb_image_resize.h` implementation to be included in the project. If not available, the function will do nothing and set an error.
 * @warning If the resize operation fails (e.g., due to a memory allocation error), the original image is left unmodified.
 *
 * @param[in,out] image A pointer to the `SituationImage` to be modified.
 * @param newWidth The target width of the image in pixels. Must be greater than 0.
 * @param newHeight The target height of the image in pixels. Must be greater than 0.
 *
 * @see SituationImageCrop(), SituationImageCopy()
 */
SITAPI void SituationImageResize(SituationImage *image, int newWidth, int newHeight) {
    // 1. --- Validation ---
    if (!SituationIsImageValid(*image) || newWidth <= 0 || newHeight <= 0) { return; }
    if (image->width == newWidth && image->height == newHeight) { return; }

#if defined(STB_IMAGE_RESIZE_IMPLEMENTATION)
    // 2. --- Allocate Memory for the New Image ---
    unsigned char *newData = (unsigned char*)SIT_MALLOC((size_t)newWidth * (size_t)newHeight * 4);
    if (!newData) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Image resize buffer");
        return;
    }

    // 3. --- Call the Correct STB Resize Function ---
    // This now uses the exact signature you provided.
    unsigned char* result = stbir_resize_uint8_srgb(
        (const unsigned char*)image->data, // Input pixels
        image->width,                      // Input width
        image->height,                     // Input height
        image->width * 4,                  // Input stride (bytes per row)
        newData,                           // Output pixels buffer
        newWidth,                          // Output width
        newHeight,                         // Output height
        newWidth * 4,                      // Output stride
        STBIR_RGBA                         // The pixel layout (4 channels, RGBA order)
    );

    // 4. --- Update the SituationImage Struct ---
    // The new API returns a pointer to the output buffer on success, or NULL on failure.
    if (result) {
        SIT_FREE(image->data); // Free the old image data
        image->data = newData;
        image->width = newWidth;
        image->height = newHeight;
    } else {
        // The resize failed. Clean up and leave the original image untouched.
        _SituationSetErrorFromCode(SITUATION_ERROR_IMAGE_OPERATION_FAILED, "stb_image_resize failed.");
        SIT_FREE(newData);
    }
#else
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Image resizing not available. Please implement stb_image_resize.h.");
#endif
}

/**
 * @brief Flips an image in-place either vertically, horizontally, or both.
 * @details This is a destructive operation that directly modifies the pixel data of the provided `SituationImage`. The function uses optimized memory operations to perform the flip efficiently.
 *
 * @par Flip Modes
 *   - **`SIT_FLIP_VERTICAL`:** Flips the image top-to-bottom. The top row of pixels becomes the bottom row, and so on. This is commonly needed to correct the orientation of images read from GPU framebuffers (like with `glReadPixels`).
 *   - **`SIT_FLIP_HORIZONTAL`:** Flips the image left-to-right, creating a mirror image.
 *   - **`SIT_FLIP_BOTH`:** Performs both a vertical and a horizontal flip. This is equivalent to rotating the image by 180 degrees.
 *
 * @param[in,out] image A pointer to the `SituationImage` to be modified.
 * @param mode The `SituationImageFlipMode` enum specifying the type of flip to perform.
 */
SITAPI void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode) {
    if (!image || !SituationIsImageValid(*image)) {
        // Added a check for the image pointer itself
        return;
    }

    // Use a switch to handle the different flip modes
    switch (mode) {
        case SIT_FLIP_VERTICAL: {
            // This is your original, optimized logic.
            int row_size = image->width * 4;
            unsigned char* row_buffer = (unsigned char*)SIT_MALLOC(row_size);
            if (!row_buffer) return;

            for (int y = 0; y < image->height / 2; ++y) {
                unsigned char* top_row = (unsigned char*)image->data + (y * row_size);
                unsigned char* bottom_row = (unsigned char*)image->data + ((image->height - 1 - y) * row_size);

                // Swap the entire rows
                memcpy(row_buffer, top_row, row_size);
                memcpy(top_row, bottom_row, row_size);
                memcpy(bottom_row, row_buffer, row_size);
            }
            SIT_FREE(row_buffer);
            break;
        }

        case SIT_FLIP_HORIZONTAL: {
            // For horizontal flip, we swap pixels within each row.
            const int pixel_size = 4; // RGBA
            unsigned char pixel_buffer[4]; // Buffer for a single pixel

            for (int y = 0; y < image->height; ++y) {
                // Get a pointer to the start of the current row
                unsigned char* row = (unsigned char*)image->data + (y * image->width * pixel_size);

                for (int x = 0; x < image->width / 2; ++x) {
                    unsigned char* left_pixel = row + (x * pixel_size);
                    unsigned char* right_pixel = row + ((image->width - 1 - x) * pixel_size);

                    // Swap the left and right pixels
                    memcpy(pixel_buffer, left_pixel, pixel_size);
                    memcpy(left_pixel, right_pixel, pixel_size);
                    memcpy(right_pixel, pixel_buffer, pixel_size);
                }
            }
            break;
        }

        case SIT_FLIP_BOTH: {
            // The simplest way to flip both is to perform one flip after the other.
            // This is equivalent to a 180-degree rotation.
            SituationImageFlip(image, SIT_FLIP_VERTICAL);
            SituationImageFlip(image, SIT_FLIP_HORIZONTAL);
            break;
        }
    }
}


/**
 * @brief Adjusts phase (hue), chroma, and luma of every pixel in-place via float YPQ.
 * @see SituationImageAdjustHSV(), SituationColorToYPQf(), SituationColorFromYPQf()
 */
SITAPI void SituationImageAdjustYPQ(
    SituationImage* image,
    float phase_shift_deg,
    float chroma_factor,
    float luma_factor,
    float mix)
{
    if (!SituationIsImageValid(*image)) {
        return;
    }

    mix = fmaxf(0.0f, fminf(1.0f, mix));
    unsigned char* pixels = (unsigned char*)image->data;
    int pixel_count = image->width * image->height;
    const float phase_shift = phase_shift_deg / 360.0f;

    for (int i = 0; i < pixel_count; ++i) {
        ColorRGBA original_rgb = {
            pixels[i * 4 + 0],
            pixels[i * 4 + 1],
            pixels[i * 4 + 2],
            pixels[i * 4 + 3]
        };

        ColorYPQf ypq = SituationColorToYPQf(original_rgb);

        ypq.p = fmodf(ypq.p + phase_shift, 1.0f);
        if (ypq.p < 0.0f) {
            ypq.p += 1.0f;
        }

        ypq.q *= chroma_factor;
        ypq.y *= luma_factor;
        ypq.y = _SitYpqClampUnitFloat(ypq.y);
        ypq.q = _SitYpqClampUnitFloat(ypq.q);

        ColorRGBA adjusted_rgb = SituationColorFromYPQf(ypq);

        pixels[i * 4 + 0] = (unsigned char)((float)original_rgb.r * (1.0f - mix) + (float)adjusted_rgb.r * mix);
        pixels[i * 4 + 1] = (unsigned char)((float)original_rgb.g * (1.0f - mix) + (float)adjusted_rgb.g * mix);
        pixels[i * 4 + 2] = (unsigned char)((float)original_rgb.b * (1.0f - mix) + (float)adjusted_rgb.b * mix);
        pixels[i * 4 + 3] = original_rgb.a;
    }
}

/**
 * @brief Adjusts the Hue, Saturation, and Value (Brightness) of an entire image in-place.
 * @details This function iterates through every pixel of an image, converts it to the HSV color space, applies the specified transformations, and converts it back to RGBA. This provides a powerful and intuitive way to perform color correction and grading on CPU-side images.
 *
 * @param[in,out] image A pointer to the `SituationImage` to be modified.
 * @param hue_shift The amount to shift the hue of every pixel, in degrees. This value is added to the existing hue, wrapping around the 360-degree color wheel (e.g., a shift of 30 will turn red into orange).
 * @param sat_factor A multiplier for the saturation of every pixel.
 *                   - `1.0f` = No change.
 *                   - `0.0f` = Fully desaturate the image (grayscale).
 *                   - `2.0f` = Double the saturation (more vivid colors).
 * @param val_factor A multiplier for the value (brightness) of every pixel.
 *                   - `1.0f` = No change.
 *                   - `0.5f` = Halve the brightness.
 *                   - `1.5f` = Increase brightness by 50%.
 * @param mix The blend factor between the original and the fully adjusted color, from `0.0f` (no change) to `1.0f` (fully adjusted). This allows you to fade the effect in or out.
 *
 * @note This is a destructive operation that modifies the image's pixel data directly.
 * @note The alpha channel of the image is preserved and not affected by this function.
 *
 * @see SituationRgbToHsv(), SituationHsvToRgb()
 */
SITAPI void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix) {
    if (!SituationIsImageValid(*image)) return;

    // Clamp mix factor to a safe range [0, 1]
    mix = fmaxf(0.0f, fminf(1.0f, mix));

    unsigned char* pixels = (unsigned char*)image->data;
    int pixel_count = image->width * image->height;

    for (int i = 0; i < pixel_count; ++i) {
        // Step 1: Get original pixel and convert to HSV
        ColorRGBA original_rgb = { pixels[i*4+0], pixels[i*4+1], pixels[i*4+2], pixels[i*4+3] };
        ColorHSV hsv = SituationRgbToHsv(original_rgb);

        // Step 2: Apply adjustments to H, S, V
        // Adjust Hue (wraps around 360 degrees)
        hsv.h = fmodf(hsv.h + hue_shift, 360.0f);
        if (hsv.h < 0.0f) hsv.h += 360.0f;

        // Adjust Saturation and Value (multiplicative)
        hsv.s *= sat_factor;
        hsv.v *= val_factor;

        // Clamp S and V to the valid [0, 1] range
        hsv.s = fmaxf(0.0f, fminf(1.0f, hsv.s));
        hsv.v = fmaxf(0.0f, fminf(1.0f, hsv.v));

        // Step 3: Convert the adjusted HSV back to RGB
        ColorRGBA adjusted_rgb = SituationHsvToRgb(hsv);

        // Step 4: Linearly interpolate (mix) between original and adjusted color
        pixels[i*4 + 0] = (unsigned char)((float)original_rgb.r * (1.0f - mix) + (float)adjusted_rgb.r * mix);
        pixels[i*4 + 1] = (unsigned char)((float)original_rgb.g * (1.0f - mix) + (float)adjusted_rgb.g * mix);
        pixels[i*4 + 2] = (unsigned char)((float)original_rgb.b * (1.0f - mix) + (float)adjusted_rgb.b * mix);
        // Alpha channel is preserved from the original
        pixels[i*4 + 3] = original_rgb.a;
    }
}

// ============================================================================
// Font — internal grid / packed bitmap atlas builders
// ============================================================================

static bool _SituationFontAtlasIsLibraryDefault(SituationTexture atlas) {
    if (atlas.generation == 0 || !SituationIsInitialized()) {
        return false;
    }
    return atlas.slot_index == sit_render.default_font.atlas_texture.slot_index &&
           atlas.generation == sit_render.default_font.atlas_texture.generation;
}

static SituationError _SituationFontUploadGridAtlas(
    unsigned char* atlas_data, int atlas_width, int atlas_height,
    SituationFont* out_font, bool free_pixels_after_upload)
{
    if (!atlas_data || !out_font || atlas_width <= 0 || atlas_height <= 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationImage img = {0};
    img.width = atlas_width;
    img.height = atlas_height;
    img.channels = 4;
    img.data = atlas_data;

    SituationError err = SituationCreateTexture(img, false, &out_font->atlas_texture);
    if (free_pixels_after_upload) {
        SIT_FREE(atlas_data);
    }
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SituationSetTextureSamplerParams(
        out_font->atlas_texture,
        SIT_TEXTURE_FILTER_NEAREST,
        SIT_TEXTURE_FILTER_NEAREST,
        SIT_TEXTURE_WRAP_CLAMP_TO_EDGE,
        SIT_TEXTURE_WRAP_CLAMP_TO_EDGE);

    out_font->atlas_width = atlas_width;
    out_font->atlas_height = atlas_height;
    out_font->glyph_info = NULL;
    return SITUATION_SUCCESS;
}

static void _SituationFontClearGridFields(SituationFont* font) {
    if (!font) return;
    font->first_char = 0;
    font->chars_per_row = 0;
    font->chars_per_col = 0;
    font->display_cell_width = 0;
    font->display_cell_height = 0;
    font->char_spacing = 0.0f;
    font->line_spacing = 0.0f;
}

static SituationError _SituationCreateTerminalFontFromMemoryImpl(
    const unsigned char* font_data,
    int char_width, int char_height,
    int char_count, int chars_per_row, int first_char,
    float char_spacing, float line_spacing,
    SituationFont* out_font)
{
    if (!font_data || !out_font || char_width <= 0 || char_height <= 0 ||
        char_count <= 0 || chars_per_row <= 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    memset(out_font, 0, sizeof(*out_font));

    const int atlas_chars_per_row = 16;
    const int atlas_chars_per_col = 16;
    int atlas_width = atlas_chars_per_row * char_width;
    int atlas_height = atlas_chars_per_col * char_height;

    int chars_per_col = (char_count + chars_per_row - 1) / chars_per_row;
    int source_width = chars_per_row * char_width;
    int source_height = chars_per_col * char_height;

    unsigned char* atlas_data = (unsigned char*)SIT_CALLOC((size_t)atlas_width * (size_t)atlas_height * 4u, 1);
    if (!atlas_data) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    for (int char_idx = 0; char_idx < char_count && char_idx < 256; char_idx++) {
        int src_char_x = (char_idx % chars_per_row) * char_width;
        int src_char_y = (char_idx / chars_per_row) * char_height;
        int atlas_char_x = (char_idx % atlas_chars_per_row) * char_width;
        int atlas_char_y = (char_idx / atlas_chars_per_row) * char_height;

        for (int y = 0; y < char_height; y++) {
            for (int x = 0; x < char_width; x++) {
                int src_pixel_idx = (src_char_y + y) * source_width + (src_char_x + x);
                unsigned char src_pixel = font_data[src_pixel_idx];
                int atlas_pixel_idx = ((atlas_char_y + y) * atlas_width + (atlas_char_x + x)) * 4;
                atlas_data[atlas_pixel_idx + 0] = src_pixel;
                atlas_data[atlas_pixel_idx + 1] = src_pixel;
                atlas_data[atlas_pixel_idx + 2] = src_pixel;
                atlas_data[atlas_pixel_idx + 3] = src_pixel;
            }
        }
    }

    SituationError err = _SituationFontUploadGridAtlas(atlas_data, atlas_width, atlas_height, out_font, true);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    out_font->is_bitmap = true;
    out_font->bitmap_data = font_data;
    out_font->bitmap_width = char_width;
    out_font->bitmap_height = char_height;
    out_font->bitmap_count = char_count;
    out_font->first_char = first_char;
    out_font->chars_per_row = atlas_chars_per_row;
    out_font->chars_per_col = atlas_chars_per_col;
    out_font->display_cell_width = char_width;
    out_font->display_cell_height = char_height;
    out_font->font_height_pixels = (float)char_height;
    out_font->char_spacing = char_spacing;
    out_font->line_spacing = line_spacing;
    return SITUATION_SUCCESS;
}

static SituationError _SituationCreateOutlinedPackedBitmapFontImpl(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font)
{
    if (!packed_data || !config || !out_font ||
        config->char_width <= 0 || config->char_height <= 0 || config->char_count <= 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    memset(out_font, 0, sizeof(*out_font));

    int atlas_chars_per_row = config->atlas_chars_per_row > 0 ? config->atlas_chars_per_row : 16;
    int atlas_chars_per_col = config->atlas_chars_per_col > 0 ? config->atlas_chars_per_col :
        (config->char_count + atlas_chars_per_row - 1) / atlas_chars_per_row;

    int display_width = config->char_width + config->left_padding + config->right_padding;
    int display_height = config->display_height > 0 ? config->display_height :
        config->char_height + config->top_padding + config->bottom_padding;

    int atlas_width = atlas_chars_per_row * display_width;
    int atlas_height = atlas_chars_per_col * display_height;

    unsigned char* temp_data = (unsigned char*)SIT_CALLOC((size_t)atlas_width * (size_t)atlas_height, 1);
    unsigned char* atlas_data = (unsigned char*)SIT_CALLOC((size_t)atlas_width * (size_t)atlas_height * 4u, 1);
    if (!temp_data || !atlas_data) {
        SIT_FREE(temp_data);
        SIT_FREE(atlas_data);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    int bytes_per_entry = (config->bits_per_row + 7) / 8;
    const unsigned char* data_bytes = (const unsigned char*)packed_data;

    for (int char_idx = 0; char_idx < config->char_count; char_idx++) {
        int atlas_char_x = (char_idx % atlas_chars_per_row) * display_width;
        int atlas_char_y = (char_idx / atlas_chars_per_row) * display_height;
        int src_char_x = char_idx % config->chars_per_row;
        int src_char_y = char_idx / config->chars_per_row;
        int src_char_base_idx = (src_char_y * config->chars_per_row + src_char_x) * config->char_height;

        for (int display_row = 0; display_row < display_height; display_row++) {
            for (int display_col = 0; display_col < display_width; display_col++) {
                unsigned char pixel = 0;
                bool in_data = (display_row >= config->top_padding &&
                    display_row < display_height - config->bottom_padding &&
                    display_col >= config->left_padding &&
                    display_col < display_width - config->right_padding);
                if (in_data) {
                    int font_row = display_row - config->top_padding;
                    int font_col = display_col - config->left_padding;
                    int row_data_idx = (src_char_base_idx + font_row) * bytes_per_entry;
                    uint32_t row_data = 0;
                    for (int b = 0; b < bytes_per_entry; b++) {
                        row_data |= ((uint32_t)data_bytes[row_data_idx + b]) << (b * 8);
                    }
                    row_data >>= config->data_bit_offset;
                    int bit_pos = config->bit_order_msb_first ?
                        (config->data_bits - 1) - font_col : font_col;
                    if (bit_pos >= 0 && bit_pos < config->data_bits) {
                        pixel = (row_data & (1U << bit_pos)) ? 255 : 0;
                    }
                }
                temp_data[(atlas_char_y + display_row) * atlas_width + (atlas_char_x + display_col)] = pixel;
            }
        }
    }

    for (int y = 0; y < atlas_height; y++) {
        for (int x = 0; x < atlas_width; x++) {
            int atlas_idx = (y * atlas_width + x) * 4;
            int temp_idx = y * atlas_width + x;
            int tile_x = x / display_width;
            int tile_y = y / display_height;
            int tile_left = tile_x * display_width;
            int tile_right = tile_left + display_width - 1;
            int tile_top = tile_y * display_height;
            int tile_bottom = tile_top + display_height - 1;

            unsigned char pixel = temp_data[temp_idx];
            bool is_outline = false;
            if (config->enable_outline && pixel == 0) {
                int thickness = config->outline_thickness;
                for (int dy = -thickness; dy <= thickness && !is_outline; dy++) {
                    for (int dx = -thickness; dx <= thickness && !is_outline; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int check_x = x + dx;
                        int check_y = y + dy;
                        if (check_x < tile_left || check_x > tile_right ||
                            check_y < tile_top || check_y > tile_bottom ||
                            check_x < 0 || check_x >= atlas_width ||
                            check_y < 0 || check_y >= atlas_height) {
                            continue;
                        }
                        if (temp_data[check_y * atlas_width + check_x] > 0) {
                            float dist = sqrtf((float)(dx * dx + dy * dy));
                            if (dist <= (float)thickness) is_outline = true;
                        }
                    }
                }
            }

            if (pixel > 0) {
                atlas_data[atlas_idx + 0] = config->font_r;
                atlas_data[atlas_idx + 1] = config->font_g;
                atlas_data[atlas_idx + 2] = config->font_b;
                atlas_data[atlas_idx + 3] = config->font_a;
            } else if (is_outline) {
                atlas_data[atlas_idx + 0] = config->outline_r;
                atlas_data[atlas_idx + 1] = config->outline_g;
                atlas_data[atlas_idx + 2] = config->outline_b;
                atlas_data[atlas_idx + 3] = config->outline_a;
            }
        }
    }

    SIT_FREE(temp_data);

    SituationError err = _SituationFontUploadGridAtlas(atlas_data, atlas_width, atlas_height, out_font, true);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    out_font->is_bitmap = true;
    out_font->bitmap_data = (const unsigned char*)packed_data;
    out_font->bitmap_width = config->char_width;
    out_font->bitmap_height = config->char_height;
    out_font->bitmap_count = config->char_count;
    out_font->first_char = config->first_char;
    out_font->chars_per_row = atlas_chars_per_row;
    out_font->chars_per_col = atlas_chars_per_col;
    out_font->display_cell_width = display_width;
    out_font->display_cell_height = display_height;
    out_font->font_height_pixels = (float)display_height;
    return SITUATION_SUCCESS;
}

static bool _SituationFontIsGridAtlas(const SituationFont* font) {
    return font && font->atlas_texture.generation != 0 && font->glyph_info == NULL;
}

static int _SituationFontGridCols(const SituationFont* font) {
    if (font && font->chars_per_row > 0) return font->chars_per_row;
    return 16;
}

static int _SituationFontGridCellWidth(const SituationFont* font) {
    if (!font) return 8;
    if (font->display_cell_width > 0) return font->display_cell_width;
    if (font->bitmap_width > 0) return font->bitmap_width;
    return 8;
}

static int _SituationFontGridCellHeight(const SituationFont* font) {
    if (!font) return 8;
    if (font->display_cell_height > 0) return font->display_cell_height;
    if (font->bitmap_height > 0) return font->bitmap_height;
    return 8;
}

static float _SituationFontGridLineAdvance(const SituationFont* font, float scale_factor) {
    float cell_h = (float)_SituationFontGridCellHeight(font);
    return cell_h * scale_factor + font->line_spacing;
}

static void _SituationFontEmitGridGlyph(
    float* vertices, int* v_idx,
    const SituationFont* font,
    unsigned char c,
    float* x, float* y, float line_start_x,
    float scale_factor, float spacing)
{
    if (c == '\n') {
        *x = line_start_x;
        *y += _SituationFontGridLineAdvance(font, scale_factor);
        return;
    }
    if (c == '\r') return;

    int char_index = (int)c - font->first_char;
    int max_chars = font->bitmap_count > 0 ? font->bitmap_count : 256;
    if (char_index < 0 || char_index >= max_chars) return;

    int cols = _SituationFontGridCols(font);
    int cell_w = _SituationFontGridCellWidth(font);
    int cell_h = _SituationFontGridCellHeight(font);
    int atlas_col = char_index % cols;
    int atlas_row = char_index / cols;

    int atlas_w = font->atlas_width > 0 ? font->atlas_width : cols * cell_w;
    int atlas_h = font->atlas_height > 0 ? font->atlas_height :
        ((font->chars_per_col > 0 ? font->chars_per_col : 16) * cell_h);

    float aw = (float)atlas_w;
    float ah = (float)atlas_h;
    float u0 = (atlas_col * cell_w) / aw;
    float v0 = (atlas_row * cell_h) / ah;
    float u1 = ((atlas_col + 1) * cell_w) / aw;
    float v1 = ((atlas_row + 1) * cell_h) / ah;

    float size_px = (float)cell_h * scale_factor;
    float advance_px = (float)cell_w * scale_factor + spacing + font->char_spacing;

    float qx0 = *x;
    float qy0 = *y;
    float qx1 = *x + size_px;
    float qy1 = *y + size_px;

    vertices[(*v_idx)++] = qx0; vertices[(*v_idx)++] = qy0; vertices[(*v_idx)++] = u0; vertices[(*v_idx)++] = v0;
    vertices[(*v_idx)++] = qx0; vertices[(*v_idx)++] = qy1; vertices[(*v_idx)++] = u0; vertices[(*v_idx)++] = v1;
    vertices[(*v_idx)++] = qx1; vertices[(*v_idx)++] = qy0; vertices[(*v_idx)++] = u1; vertices[(*v_idx)++] = v0;

    vertices[(*v_idx)++] = qx1; vertices[(*v_idx)++] = qy0; vertices[(*v_idx)++] = u1; vertices[(*v_idx)++] = v0;
    vertices[(*v_idx)++] = qx0; vertices[(*v_idx)++] = qy1; vertices[(*v_idx)++] = u0; vertices[(*v_idx)++] = v1;
    vertices[(*v_idx)++] = qx1; vertices[(*v_idx)++] = qy1; vertices[(*v_idx)++] = u1; vertices[(*v_idx)++] = v1;

    *x += advance_px;
}

/**
 * @brief Loads a TrueType (.ttf) or OpenType (.otf) font file from disk for CPU-side rendering.
 * @details This function reads the entire font file into a memory buffer and initializes an `stb_truetype` context for it. The resulting `SituationFont` handle is a CPU-side resource used by the `SituationImageDraw*` text rendering functions.
 *
 * @par Memory Management
 *   The function allocates two separate blocks of memory that are stored within the returned `SituationFont` struct: one for the raw font file data and another for the `stbtt_fontinfo` context. This memory is held for the lifetime of the font and **must** be released by calling `SituationUnloadFont`.
 *
 * @param fileName The file system path to the font file (e.g., "assets/fonts/myfont.ttf").
 *
 * @return A `SituationFont` handle containing the necessary data for text rendering.
 * @return A zeroed (invalid) `SituationFont` struct if the file cannot be found, if memory allocation fails, or if the file is not a valid font. An error message will be set internally.
 *
 * @note This function is for CPU-side text rendering onto `SituationImage` objects. It is distinct from any GPU-based font atlas creation that might be used for high-performance in-game text.
 * @warning Failure to call `SituationUnloadFont` on a successfully loaded font will result in a memory leak.
 *
 * @see SituationUnloadFont(), SituationImageDrawText(), SituationMeasureText()
 */
SITAPI SituationError SituationLoadFont(const char *fileName, SituationFont* out_font) {
    if (!out_font) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_font, 0, sizeof(SituationFont));

    long size = 0;
    FILE *fontFile = fopen(fileName, "rb");

    if (!fontFile) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "SituationLoadFont: Failed to open font file.");
    }

    fseek(fontFile, 0, SEEK_END);
    size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    // 1. Load the entire font file into a buffer.
    // We must keep this buffer alive as long as we use the font.
    unsigned char *fontBuffer = (unsigned char*)SIT_MALLOC(size);
    if (!fontBuffer) {
        fclose(fontFile);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    fread(fontBuffer, 1, size, fontFile);
    fclose(fontFile);

    // 2. Allocate and initialize the stbtt_fontinfo struct.
    stbtt_fontinfo *info = (stbtt_fontinfo*)SIT_MALLOC(sizeof(stbtt_fontinfo));
    if (!info) {
        SIT_FREE(fontBuffer);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // 3. Initialize stb_truetype with our buffer.
    if (!stbtt_InitFont(info, fontBuffer, 0)) {
        // The font file is invalid or not a TrueType font.
        SIT_FREE(info);
        SIT_FREE(fontBuffer);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FONT_LOAD_FAILED, "SituationLoadFont: Failed to parse TrueType/OpenType data.");
    }

    out_font->fontData = fontBuffer;
    out_font->stbFontInfo = info;
    return SITUATION_SUCCESS;
}

/**
 * @brief Loads a font directly from a memory buffer (e.g., embedded resource).
 *
 * @details This function creates a copy of the provided font data, allowing the caller to free their source buffer immediately after this function returns.
 *          This is essential for single-file applications that embed fonts as byte arrays within the executable.
 *
 * @param data Pointer to the raw TTF/OTF file data in memory.
 * @param dataSize Size of the data in bytes.
 * @return A valid `SituationFont` handle, or a zeroed struct on failure.
 *
 * @note The returned font owns its memory copy. Call `SituationUnloadFont()` to free it.
 */
SITAPI SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font) {
    if (!out_font) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_font, 0, sizeof(SituationFont));

    if (!data || dataSize <= 0) return SITUATION_ERROR_INVALID_PARAM;

    // 1. Allocate our own buffer and copy the data.
    // This ensures SituationUnloadFont() can safely SIT_FREE(font.fontData) regardless of where the original data came from.
    out_font->fontData = SIT_MALLOC(dataSize);
    if (!out_font->fontData) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadFontFromMemory: Failed to allocate buffer copy.");
    }
    memcpy(out_font->fontData, data, dataSize);

    // 2. Allocate and initialize the stbtt_fontinfo struct.
    out_font->stbFontInfo = SIT_MALLOC(sizeof(stbtt_fontinfo));
    if (!out_font->stbFontInfo) {
        SIT_FREE(out_font->fontData);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationLoadFontFromMemory: Failed to allocate font info.");
    }

    // 3. Initialize stb_truetype.
    // Note: stbtt_InitFont does NOT copy the data, it stores the pointer. That is why we made our own copy above.
    if (!stbtt_InitFont((stbtt_fontinfo*)out_font->stbFontInfo, (unsigned char*)out_font->fontData, 0)) {
        SIT_FREE(out_font->stbFontInfo);
        SIT_FREE(out_font->fontData);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FONT_LOAD_FAILED, "SituationLoadFontFromMemory: Failed to parse TrueType/OpenType data.");
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief Generates a GPU-ready font atlas texture from a loaded font.
 *
 * @details This function rasterizes a standard range of characters (ASCII 32-126) into a single
 *          bitmap and uploads it to the GPU as a `SituationTexture`. It also calculates and caches
 *          the texture coordinates (UVs) and metrics for each character.
 *
 *          This step is **mandatory** for using the high-performance, real-time text rendering function
 *          `SituationCmdDrawText`. It is not needed for the slower, CPU-side `SituationImageDrawText` functions.
 *
 * @param font A pointer to the `SituationFont` handle. The font must have been loaded with `SituationLoadFont`
 *             or `SituationLoadFontFromMemory`. The struct will be modified to store the atlas texture handle.
 * @param fontSizePixels The height of the characters in pixels (e.g., 16.0f, 24.0f). This determines the
 *                       resolution of the rasterized glyphs.
 *
 * @return `true` if the atlas was successfully generated and uploaded.
 * @return `false` if the font data is invalid, if the requested font size is too large to fit in the
 *         default atlas size (512x512), or if texture creation fails.
 *
 * @note The generated texture is managed by the `SituationFont` struct. Calling `SituationUnloadFont` will
 *       automatically destroy this texture.
 */
SITAPI SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels) {
#if !defined(SITUATION_NO_STB) && !defined(SITUATION_NO_STB_TRUETYPE)
    if (!font || !font->fontData) return SITUATION_ERROR_INVALID_PARAM;

    // 1. Allocate Bitmap Memory (512x512 is usually enough for ASCII)
    int w = 512;
    int h = 512;
    unsigned char* bitmap = (unsigned char*)SIT_CALLOC(w * h, 1); // 1-channel alpha

    // 2. Allocate Glyph Info
    // standard ASCII 32-126 is 96 chars
    font->glyph_info = SIT_MALLOC(sizeof(stbtt_bakedchar) * 96);

    // 3. Bake using STB
    // Returns > 0 on success (rows used), or 0 on failure (didn't fit)
    int res = stbtt_BakeFontBitmap(
        (unsigned char*)font->fontData, 0,
        fontSizePixels,
        bitmap, w, h,
        32, 96,
        (stbtt_bakedchar*)font->glyph_info
    );

    if (res <= 0) {
        SIT_FREE(bitmap);
        SIT_FREE(font->glyph_info);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FONT_ATLAS_FULL, "Font atlas bake failed: glyphs did not fit in atlas bitmap");
    }

    // 4. Convert 1-channel bitmap to 4-channel RGBA for SituationCreateTexture
    // We make it white text with alpha from the bitmap.
    SituationImage img;
    img.width = w;
    img.height = h;
    img.data = SIT_MALLOC(w * h * 4);
    unsigned char* src = bitmap;
    unsigned char* dst = (unsigned char*)img.data;

    for (int i=0; i < w*h; ++i) {
        dst[i*4+0] = 255;
        dst[i*4+1] = 255;
        dst[i*4+2] = 255;
        dst[i*4+3] = src[i];
    }
    SIT_FREE(bitmap); // Done with 1-channel

    // 5. Create GPU Texture
    SituationCreateTexture(img, false, &font->atlas_texture); // No mips needed for UI text usually
    SituationUnloadImage(img);

    font->atlas_width = w;
    font->atlas_height = h;
    font->font_height_pixels = fontSizePixels;

    return (font->atlas_texture.generation != 0) ? SITUATION_SUCCESS :
        _SituationSetErrorFromCode(SITUATION_ERROR_FONT_LOAD_FAILED, "SituationBakeFontAtlas: GPU texture creation failed for atlas");
#else
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationBakeFontAtlas requires STB Truetype.");
    return false;
#endif
}

/**
 * @brief Frees all CPU memory associated with a loaded `SituationFont`.
 * @details This is the designated cleanup function for a `SituationFont` handle created by `SituationLoadFont`. It safely frees both the raw font file data buffer and the `stbtt_fontinfo` context struct.
 *
 * @param font The `SituationFont` handle to unload. The pointers within the struct become invalid after this call.
 *
 * @note It is safe to call this function on a zeroed or partially loaded `SituationFont` struct; it will only attempt to free non-NULL pointers.
 *
 * @see SituationLoadFont()
 */
SITAPI void SituationUnloadFont(SituationFont font) {
    if (font.stbFontInfo) { SIT_FREE(font.stbFontInfo); }
    if (font.fontData) { SIT_FREE(font.fontData); }
    if (font.glyph_info) { SIT_FREE(font.glyph_info); }
    if (font.atlas_texture.generation != 0 && !_SituationFontAtlasIsLibraryDefault(font.atlas_texture)) {
        SituationTexture atlas = font.atlas_texture;
        SituationDestroyTexture(&atlas);
    }
}

SITAPI SituationError SituationBakeBitmapFontAtlas(SituationFont* font) {
    if (!font || !font->is_bitmap || !font->bitmap_data ||
        font->bitmap_width <= 0 || font->bitmap_height <= 0 || font->bitmap_count <= 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    const int atlas_chars_per_row = 16;
    const int atlas_chars_per_col = (font->bitmap_count + atlas_chars_per_row - 1) / atlas_chars_per_row;
    int char_w = font->bitmap_width;
    int char_h = font->bitmap_height;
    int stride = (char_w + 7) / 8;
    int bytes_per_char = stride * char_h;
    int atlas_width = atlas_chars_per_row * char_w;
    int atlas_height = atlas_chars_per_col * char_h;

    unsigned char* atlas_data = (unsigned char*)SIT_CALLOC((size_t)atlas_width * (size_t)atlas_height * 4u, 1);
    if (!atlas_data) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    for (int char_idx = 0; char_idx < font->bitmap_count && char_idx < 256; char_idx++) {
        int atlas_char_x = (char_idx % atlas_chars_per_row) * char_w;
        int atlas_char_y = (char_idx / atlas_chars_per_row) * char_h;
        const unsigned char* char_ptr = font->bitmap_data + (size_t)char_idx * (size_t)bytes_per_char;

        for (int y = 0; y < char_h; y++) {
            for (int x = 0; x < char_w; x++) {
                int byte_idx = y * stride + (x / 8);
                int bit_idx = 7 - (x % 8);
                unsigned char px = ((char_ptr[byte_idx] >> bit_idx) & 1) ? 255 : 0;
                int dst = ((atlas_char_y + y) * atlas_width + (atlas_char_x + x)) * 4;
                atlas_data[dst + 0] = px;
                atlas_data[dst + 1] = px;
                atlas_data[dst + 2] = px;
                atlas_data[dst + 3] = px;
            }
        }
    }

    SituationError err = _SituationFontUploadGridAtlas(atlas_data, atlas_width, atlas_height, font, true);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    font->first_char = 0;
    font->chars_per_row = atlas_chars_per_row;
    font->chars_per_col = atlas_chars_per_col;
    font->display_cell_width = char_w;
    font->display_cell_height = char_h;
    font->font_height_pixels = (float)char_h;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationLoadBitmapFontFromTexture(
    SituationTexture sheet, int char_width, int char_height, int first_char, SituationFont* out_font)
{
    if (!out_font || char_width <= 0 || char_height <= 0 || sheet.generation == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationTextureInfo info = {0};
    if (SituationGetTextureInfo(sheet, &info) != SITUATION_SUCCESS) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    memset(out_font, 0, sizeof(*out_font));
    out_font->atlas_texture = sheet;
    out_font->atlas_width = info.width;
    out_font->atlas_height = info.height;
    out_font->is_bitmap = true;
    out_font->first_char = first_char;
    out_font->chars_per_row = info.width / char_width;
    out_font->chars_per_col = info.height / char_height;
    out_font->display_cell_width = char_width;
    out_font->display_cell_height = char_height;
    out_font->font_height_pixels = (float)char_height;
    out_font->bitmap_count = out_font->chars_per_row * out_font->chars_per_col;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationCreateTerminalFontFromMemory(
    const unsigned char* data, int char_width, int char_height,
    int char_count, int chars_per_row, int first_char, SituationFont* out_font)
{
    return _SituationCreateTerminalFontFromMemoryImpl(
        data, char_width, char_height, char_count, chars_per_row, first_char, 0.0f, 0.0f, out_font);
}

SITAPI SituationError SituationCreateTerminalFontEx(
    const unsigned char* data, int char_width, int char_height,
    int char_count, int chars_per_row, int first_char,
    float char_spacing, float line_spacing, SituationFont* out_font)
{
    return _SituationCreateTerminalFontFromMemoryImpl(
        data, char_width, char_height, char_count, chars_per_row, first_char,
        char_spacing, line_spacing, out_font);
}

SITAPI SituationError SituationCreateCP437Font(const unsigned char* font_data_8x16, SituationFont* out_font) {
    return SituationCreateTerminalFontFromMemory(font_data_8x16, 8, 16, 256, 16, 0, out_font);
}

SITAPI SituationError SituationCreateASCIIFont(const unsigned char* data, int cw, int ch, SituationFont* out_font) {
    return SituationCreateTerminalFontFromMemory(data, cw, ch, 95, 16, 32, out_font);
}

SITAPI SituationError SituationCreatePackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font)
{
    if (!config) return SITUATION_ERROR_INVALID_PARAM;
    SituationPackedFont cfg = *config;
    cfg.enable_outline = false;
    return _SituationCreateOutlinedPackedBitmapFontImpl(packed_data, &cfg, out_font);
}

SITAPI SituationError SituationCreateOutlinedPackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font)
{
    return _SituationCreateOutlinedPackedBitmapFontImpl(packed_data, config, out_font);
}

SITAPI SituationError SituationCreateVCRFont(const uint16_t* font_data, SituationFont* out_font) {
    if (!font_data || !out_font) return SITUATION_ERROR_INVALID_PARAM;
    SituationPackedFont config = {
        .char_width = 12, .char_height = 14, .display_height = 16,
        .char_count = 128, .first_char = 0, .chars_per_row = 1,
        .bits_per_row = 16, .data_bits = 12, .data_bit_offset = 0,
        .bit_order_msb_first = true,
        .top_padding = 1, .bottom_padding = 1,
        .atlas_chars_per_row = 16, .atlas_chars_per_col = 8,
        .font_r = 255, .font_g = 255, .font_b = 255, .font_a = 255,
    };
    return SituationCreatePackedBitmapFont(font_data, &config, out_font);
}

SITAPI SituationError SituationCreateVCRFontWithOutline(const uint16_t* data, int outline_thickness, SituationFont* out_font) {
    if (!data || !out_font) return SITUATION_ERROR_INVALID_PARAM;
    SituationPackedFont config = {
        .char_width = 12, .char_height = 14, .display_height = 16,
        .char_count = 128, .first_char = 0, .chars_per_row = 1,
        .bits_per_row = 16, .data_bits = 12, .data_bit_offset = 0,
        .bit_order_msb_first = true,
        .top_padding = 1, .bottom_padding = 1,
        .left_padding = 2, .right_padding = 2,
        .atlas_chars_per_row = 16, .atlas_chars_per_col = 8,
        .enable_outline = true, .outline_thickness = outline_thickness,
        .outline_r = 0, .outline_g = 0, .outline_b = 0, .outline_a = 255,
        .font_r = 255, .font_g = 255, .font_b = 255, .font_a = 255,
    };
    return SituationCreateOutlinedPackedBitmapFont(data, &config, out_font);
}

SITAPI SituationError SituationCreateVGA8x8Font(const unsigned char* data, SituationFont* out_font) {
    if (!data || !out_font) return SITUATION_ERROR_INVALID_PARAM;
    SituationPackedFont config = {
        .char_width = 8, .char_height = 8, .display_height = 10,
        .char_count = 256, .first_char = 0, .chars_per_row = 1,
        .bits_per_row = 8, .data_bits = 8, .data_bit_offset = 0,
        .bit_order_msb_first = true,
        .top_padding = 1, .bottom_padding = 1,
        .left_padding = 1, .right_padding = 1,
        .atlas_chars_per_row = 16, .atlas_chars_per_col = 16,
        .font_r = 255, .font_g = 255, .font_b = 255, .font_a = 255,
    };
    return SituationCreatePackedBitmapFont(data, &config, out_font);
}

SITAPI SituationError SituationCreateVGA8x8FontWithOutline(const unsigned char* data, int outline_thickness, SituationFont* out_font) {
    if (!data || !out_font) return SITUATION_ERROR_INVALID_PARAM;
    SituationPackedFont config = {
        .char_width = 8, .char_height = 8, .display_height = 10,
        .char_count = 256, .first_char = 0, .chars_per_row = 1,
        .bits_per_row = 8, .data_bits = 8, .data_bit_offset = 0,
        .bit_order_msb_first = true,
        .top_padding = 1, .bottom_padding = 1,
        .left_padding = 1, .right_padding = 1,
        .atlas_chars_per_row = 16, .atlas_chars_per_col = 16,
        .enable_outline = true, .outline_thickness = outline_thickness,
        .outline_r = 0, .outline_g = 0, .outline_b = 0, .outline_a = 255,
        .font_r = 255, .font_g = 255, .font_b = 255, .font_a = 255,
    };
    return SituationCreateOutlinedPackedBitmapFont(data, &config, out_font);
}

static void _SituationImageFillRect(SituationImage* img, int x0, int y0, int w, int h, ColorRGBA col) {
    if (!img || !img->data || w <= 0 || h <= 0) return;
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            if (x >= 0 && x < img->width && y >= 0 && y < img->height) {
                SituationSetPixelColor(img, x, y, col);
            }
        }
    }
}

SITAPI SituationError SituationImageStampText(
    SituationImage* dst, SituationFont font, const char* text,
    Vector2 pos, float fontSize, ColorRGBA text_color, ColorRGBA bg_color)
{
    if (!dst || !text || !SituationIsImageValid(*dst)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SitRectangle bounds = SituationMeasureTextEx(font, text, fontSize, 0.0f);
    int pad = 2;
    int w = (int)bounds.width + pad * 2;
    int h = (int)bounds.height + pad * 2;
    int x0 = (int)pos.x;
    int y0 = (int)pos.y;
    if (bg_color.a > 0) {
        _SituationImageFillRect(dst, x0, y0, w, h, bg_color);
    }
    SituationImageDrawText(dst, font, text, (Vector2){(float)(x0 + pad), (float)(y0 + pad)}, fontSize, 0.0f, text_color);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationImageStampTextBoxed(
    SituationImage* dst, SituationFont font, const char* text,
    SitRectangle bounds, float fontSize, ColorRGBA text_color, ColorRGBA bg_color,
    bool word_wrap, int* out_width, int* out_height)
{
    if (!dst || !text || !SituationIsImageValid(*dst)) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (bg_color.a > 0) {
        _SituationImageFillRect(dst, (int)bounds.x, (int)bounds.y,
            (int)bounds.width, (int)bounds.height, bg_color);
    }
    /* CPU boxed stamp: draw line-by-line within bounds */
    float line_h = fontSize;
    if (_SituationFontIsGridAtlas(&font) || font.is_bitmap) {
        int cell_h = _SituationFontGridCellHeight(&font);
        if (cell_h > 0) line_h = fontSize + font.line_spacing;
        (void)cell_h;
    }
    float cy = bounds.y;
    char line_buf[512];
    const char* line_start = text;
    for (const char* c = text; ; c++) {
        if (*c == '\n' || *c == '\0') {
            size_t len = (size_t)(c - line_start);
            if (len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
            for (size_t i = 0; i < len; i++) line_buf[i] = line_start[i];
            line_buf[len] = '\0';
            if (line_buf[0] && cy + line_h <= bounds.y + bounds.height) {
                SituationImageDrawText(dst, font, line_buf,
                    (Vector2){bounds.x, cy}, fontSize, 0.0f, text_color);
            }
            cy += line_h;
            if (*c == '\0') break;
            line_start = c + 1;
        }
    }
    (void)word_wrap;
    if (out_width) *out_width = (int)bounds.width;
    if (out_height) *out_height = (int)bounds.height;
    return SITUATION_SUCCESS;
}

static bool _SituationMeasureFontIsGridAtlas(const SituationFont* font) {
    return _SituationFontIsGridAtlas(font);
}

static int _SituationMeasureFontCellWidth(const SituationFont* font) {
    return _SituationFontGridCellWidth(font);
}

static int _SituationMeasureFontCellHeight(const SituationFont* font) {
    return _SituationFontGridCellHeight(font);
}

static SitRectangle _SituationMeasureGridText(
    const SituationFont* font, const char* text, float fontSize, float spacing)
{
    SitRectangle rect = {0, 0, 0, 0};
    if (!text || !font) return rect;

    int cell_h = _SituationMeasureFontCellHeight(font);
    float scale = fontSize / (float)cell_h;
    float cell_w_scaled = (float)_SituationMeasureFontCellWidth(font) * scale;
    float extra = spacing + font->char_spacing;
    float line_h = fontSize + font->line_spacing;

    float max_width = 0.0f;
    float current_width = 0.0f;
    int line_count = 1;

    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            if (current_width > max_width) max_width = current_width;
            current_width = 0.0f;
            line_count++;
        } else if (*c != '\r') {
            current_width += cell_w_scaled + extra;
        }
    }
    if (current_width > max_width) max_width = current_width;
    if (line_count > 1 && max_width > 0.0f && extra > 0.0f) {
        max_width -= extra;
    }

    rect.width = max_width;
    rect.height = (float)line_count * line_h - (line_count > 1 ? font->line_spacing : 0.0f);
    if (rect.height < fontSize) rect.height = fontSize;
    return rect;
}

static SitRectangle _SituationMeasureTtfText(
    SituationFont font, const char* text, float fontSize, float spacing, bool use_baked_metrics)
{
    SitRectangle rect = {0, 0, 0, 0};
    if (!text) return rect;

    float line_h = fontSize;
    float max_width = 0.0f;
    float current_width = 0.0f;
    int line_count = 1;

    if (use_baked_metrics && font.glyph_info && font.atlas_width > 0 && font.atlas_height > 0) {
        stbtt_bakedchar* baked = (stbtt_bakedchar*)font.glyph_info;
        float scale = (font.font_height_pixels > 0.0f) ? (fontSize / font.font_height_pixels) : 1.0f;
        for (const char* c = text; *c; c++) {
            if (*c == '\n') {
                if (current_width > max_width) max_width = current_width;
                current_width = 0.0f;
                line_count++;
            } else if (*c != '\r' && *c >= 32 && *c < 127) {
                current_width += baked[*c - 32].xadvance * scale + spacing;
            }
        }
    } else if (font.stbFontInfo) {
        stbtt_fontinfo* info = (stbtt_fontinfo*)font.stbFontInfo;
        float scale = stbtt_ScaleForPixelHeight(info, fontSize);
        for (const char* c = text; *c; c++) {
            if (*c == '\n') {
                if (current_width > max_width) max_width = current_width;
                current_width = 0.0f;
                line_count++;
            } else if (*c != '\r') {
                int advance = 0, lsb = 0;
                stbtt_GetCodepointHMetrics(info, *c, &advance, &lsb);
                current_width += advance * scale + spacing;
                if (c[1]) {
                    current_width += stbtt_GetCodepointKernAdvance(info, *c, c[1]) * scale;
                }
            }
        }
    }

    if (current_width > max_width) max_width = current_width;
  if (line_count > 1 && max_width > 0.0f && spacing > 0.0f) {
        max_width -= spacing;
    }

    rect.width = max_width;
    rect.height = (float)line_count * line_h;
    return rect;
}

SITAPI SitRectangle SituationMeasureTextEx(SituationFont font, const char *text, float fontSize, float spacing) {
    if (!text || fontSize <= 0.0f) {
        return (SitRectangle){0, 0, 0, 0};
    }

    if (font.atlas_texture.generation == 0 && SituationIsInitialized()) {
        font = sit_render.default_font;
    }

    if (_SituationFontIsGridAtlas(&font)) {
        return _SituationMeasureGridText(&font, text, fontSize, spacing);
    }
    if (font.is_bitmap && font.bitmap_width > 0 && font.bitmap_height > 0) {
        return _SituationMeasureGridText(&font, text, fontSize, spacing);
    }

    return _SituationMeasureTtfText(font, text, fontSize, spacing, true);
}

SITAPI int SituationGetTextLineCount(SituationFont font, const char *text, float max_width) {
    if (!text) return 1;

    if (font.atlas_texture.generation == 0 && SituationIsInitialized()) {
        font = sit_render.default_font;
    }

    if (max_width <= 0.0f) {
        int line_count = 1;
        for (const char* c = text; *c; c++) {
            if (*c == '\n') line_count++;
        }
        return line_count;
    }

    if (_SituationFontIsGridAtlas(&font) || font.is_bitmap) {
        int line_count = 1;
        float current_width = 0.0f;
        float cell_w = (float)_SituationMeasureFontCellWidth(&font) + font.char_spacing;
        for (const char* c = text; *c; c++) {
            if (*c == '\n') {
                line_count++;
                current_width = 0.0f;
            } else if (*c != '\r') {
                current_width += cell_w;
                if (current_width > max_width) {
                    line_count++;
                    current_width = cell_w;
                }
            }
        }
        return line_count;
    }

    if (font.glyph_info && font.atlas_width > 0) {
        stbtt_bakedchar* baked = (stbtt_bakedchar*)font.glyph_info;
        int line_count = 1;
        float current_width = 0.0f;
        for (const char* c = text; *c; c++) {
            if (*c == '\n') {
                line_count++;
                current_width = 0.0f;
            } else if (*c != '\r' && *c >= 32 && *c < 127) {
                current_width += baked[*c - 32].xadvance;
                if (current_width > max_width) {
                    line_count++;
                    current_width = baked[*c - 32].xadvance;
                }
            }
        }
        return line_count;
    }

    return 1;
}

/**
 * @brief [INTERNAL] Saves a `SituationImage` to a file in the uncompressed 24/32-bit BMP format.
 * @details This is a low-level, self-contained utility for writing bitmap files. It manually constructs the necessary BMP file and info headers, converts the image's in-memory RGBA pixel data to the BGRA format required by the BMP standard, and writes the complete file to disk using `SituationSaveFileData`.
 *          This function serves as a native, dependency-free fallback for image exporting when more advanced libraries like `stb_image_write.h` are not available for PNG encoding.
 *
 * @param fileName The destination file path for the `.bmp` file.
 * @param image A pointer to the `SituationImage` containing the pixel data to be saved.
 *
 * @return `true` if the BMP file was successfully written to disk.
 * @return `false` if any step fails (e.g., invalid parameters, memory allocation for the file buffer fails, or the final file write operation fails). An error message is set on failure.
 *
 * @note This function is for internal use by `SituationExportImage` and `SituationTakeScreenshot`. It should not be called directly.
 * @note The function does not perform vertical flipping; it assumes the input image data has the correct orientation (origin at the top-left).
 *
 * @see SituationExportImage(), SituationTakeScreenshot(), SituationSaveFileData()
 */
static SituationError _SituationSaveImageBMP(const char* fileName, const SituationImage* image) {
    if (!fileName || !image || !image->data) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM, "_SituationSaveImageBMP: fileName, image, or image->data is NULL");
    }

    int imageSize = image->width * image->height * 4;
    // BMP file format requires headers
    int fileSize = 54 + imageSize; // 54 bytes for headers

    // BMP File Header (14 bytes)
    char fileHeader[14] = {
        'B', 'M',           // Signature
        0, 0, 0, 0,         // File size in bytes
        0, 0, 0, 0,         // Reserved
        54, 0, 0, 0         // Offset to pixel data
    };
    // BMP Info Header (40 bytes)
    char infoHeader[40] = {
        40, 0, 0, 0,        // Header size
        0, 0, 0, 0,         // Image width
        0, 0, 0, 0,         // Image height
        1, 0,               // Number of color planes
        32, 0,              // Bits per pixel (RGBA = 32)
        0, 0, 0, 0,         // Compression method (0=BI_RGB)
        0, 0, 0, 0,         // Image size (can be 0 for BI_RGB)
        0, 0, 0, 0,         // Horizontal resolution (pixels per meter)
        0, 0, 0, 0,         // Vertical resolution
        0, 0, 0, 0,         // Number of colors in palette (0 for 32-bit)
        0, 0, 0, 0,         // Number of important colors (0 = all)
    };

    // Fill in the dynamic header fields
    fileHeader[2] = (char)(fileSize);
    fileHeader[3] = (char)(fileSize >> 8);
    fileHeader[4] = (char)(fileSize >> 16);
    fileHeader[5] = (char)(fileSize >> 24);

    infoHeader[4] = (char)(image->width);
    infoHeader[5] = (char)(image->width >> 8);
    infoHeader[6] = (char)(image->width >> 16);
    infoHeader[7] = (char)(image->width >> 24);
    infoHeader[8] = (char)(image->height);
    infoHeader[9] = (char)(image->height >> 8);
    infoHeader[10] = (char)(image->height >> 16);
    infoHeader[11] = (char)(image->height >> 24);

    // Create a single buffer for the entire file
    unsigned char *fileBuffer = (unsigned char *)SIT_MALLOC(fileSize);
    if (!fileBuffer) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "BMP file buffer");
    }

    memcpy(fileBuffer, fileHeader, 14);
    memcpy(fileBuffer + 14, infoHeader, 40);

    // Copy pixel data, converting RGBA to BGRA as required by BMP
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            int i = (y * image->width + x) * 4;
            int out_i = 54 + i;
            unsigned char* pixel = (unsigned char*)image->data + i;
            fileBuffer[out_i + 0] = pixel[2]; // Blue
            fileBuffer[out_i + 1] = pixel[1]; // Green
            fileBuffer[out_i + 2] = pixel[0]; // Red
            fileBuffer[out_i + 3] = pixel[3]; // Alpha
        }
    }

    // Save the buffer to disk using our existing library function
    SituationError save_err = SituationSaveFileData(fileName, fileBuffer, fileSize);
    SIT_FREE(fileBuffer);

    if (save_err != SITUATION_SUCCESS) {
        return _SituationSetFilesystemError(
            "Failed to save BMP file data to disk", fileName, SITUATION_ERROR_FILE_WRITE_FAILED);
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Blends a source color onto a destination color using standard alpha blending.
 * @details This helper function implements the "Normal" blend mode (`SRC over DST`). It calculates the final color by interpolating between the source and destination based on the source's alpha channel, which is modulated by an additional blend factor.
 *
 * @par Blending Formula
 *   - `FinalColor.rgb = Src.rgb * BlendAlpha + Dst.rgb * (1 - BlendAlpha)`
 *   - `FinalColor.a   = Dst.a + Src.a * BlendAlpha` (approximated for additive alpha)
 *   - where `BlendAlpha = (Src.a / 255.0) * alpha`
 *
 * This function is used by the CPU-side image drawing routines to composite pixels with transparency.
 *
 * @param dst The destination color that will be drawn onto.
 * @param src The source color to blend.
 * @param alpha An additional blend factor (0.0f to 1.0f) to modulate the source's alpha.
 *
 * @return The resulting `ColorRGBA` struct after blending.
 *
 * @note This function is for internal use by the `SituationImageDraw*` functions.
 */
static inline ColorRGBA _SituationColorAlphaBlend(ColorRGBA dst, ColorRGBA src, float alpha) {
    if (alpha <= 0.0f) return dst;
    if (alpha >= 1.0f) return (ColorRGBA){src.r, src.g, src.b, (unsigned char)_SituationClampf((float)dst.a + src.a, 0, 255)};

    float srcA = (float)src.a / 255.0f;
    float blendAlpha = srcA * alpha;

    ColorRGBA result;
    result.r = (unsigned char)((float)src.r * blendAlpha + (float)dst.r * (1.0f - blendAlpha));
    result.g = (unsigned char)((float)src.g * blendAlpha + (float)dst.g * (1.0f - blendAlpha));
    result.b = (unsigned char)((float)src.b * blendAlpha + (float)dst.b * (1.0f - blendAlpha));
    // The final alpha is a bit more complex, we want to add the new shape's alpha to the existing alpha
    result.a = (unsigned char)_SituationClampf((float)dst.a + ((float)src.a * alpha), 0, 255);

    return result;
}

/**
 * @brief Draws a portion of a source image onto a destination image with alpha blending and tinting.
 * @details This function composites a rectangular region from a source image onto a destination image, respecting the alpha channel of both the source pixels and the provided `tint` color. This is the primary function for drawing sprites or UI elements with transparency.
 *
 * @par Blending Formula
 *   The function uses a standard "Normal" blend mode (SRC over DST). The final color is calculated pixel by pixel:
 *   - `FinalColor.rgb = TintedSrc.rgb * FinalAlpha + Dst.rgb * (1 - FinalAlpha)`
 *   - `TintedSrc.rgb = (Src.rgb / 255) * (Tint.rgb / 255)`
 *   - `FinalAlpha = (Src.a / 255) * (Tint.a / 255)`
 *
 * @par Boundary Handling
 *   Like `SituationImageDraw`, this function is robust against invalid coordinates and will only draw the overlapping area between the source and destination rectangles, preventing out-of-bounds memory access.
 *
 * @param[in,out] dst A pointer to the destination `SituationImage` to be modified.
 * @param src The source `SituationImage` to draw from.
 * @param srcRect The rectangular region within the source image to use.
 * @param dstPos The top-left `(x, y)` position on the destination image where drawing should start.
 * @param tint The color to modulate the source image with as it's drawn. White `{255,255,255,255}` results in no color change.
 *
 * @see SituationImageDraw()
 */
SITAPI void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos, ColorRGBA tint) {
    // 1. --- Validation and Intersection Calculation ---
    // (This is the same robust boundary-checking logic from our previous `SituationImageDraw` discussion)
    if (!SituationIsImageValid(*dst) || !SituationIsImageValid(src)) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationImageDrawAlpha: invalid dst or src image"); return; }

    int srcClipX = (srcRect.x < 0) ? 0 : srcRect.x;
    int srcClipY = (srcRect.y < 0) ? 0 : srcRect.y;
    int srcClipW = (srcRect.x + srcRect.width > src.width) ? (src.width - srcRect.x) : srcRect.width;
    int srcClipH = (srcRect.y + srcRect.height > src.height) ? (src.height - srcRect.y) : srcRect.height;

    int dstClipX = (int)dstPos.x + (srcClipX - srcRect.x);
    int dstClipY = (int)dstPos.y + (srcClipY - srcRect.y);

    if (dstClipX < 0) { srcClipW += dstClipX; srcClipX -= dstClipX; dstClipX = 0; }
    if (dstClipY < 0) { srcClipH += dstClipY; srcClipY -= dstClipY; dstClipY = 0; }
    if (dstClipX + srcClipW > dst->width)  { srcClipW = dst->width - dstClipX; }
    if (dstClipY + srcClipH > dst->height) { srcClipH = dst->height - dstClipY; }

    if (srcClipW <= 0 || srcClipH <= 0) return;

    // 2. --- Pixel-by-Pixel Blending Loop ---
    unsigned char *srcPixels = (unsigned char*)src.data;
    unsigned char *dstPixels = (unsigned char*)dst->data;
    const int pixelSize = 4;

    for (int y = 0; y < srcClipH; ++y) {
        for (int x = 0; x < srcClipW; ++x) {
            // Get pointers to the source and destination pixels for this iteration
            unsigned char* srcPixel = srcPixels + (((size_t)srcClipY + y) * src.width + ((size_t)srcClipX + x)) * pixelSize;
            unsigned char* dstPixel = dstPixels + (((size_t)dstClipY + y) * dst->width + ((size_t)dstClipX + x)) * pixelSize;

            // Get the source alpha. This is the key component.
            // We also factor in the tint's alpha.
            unsigned int srcAlpha = ((unsigned int)srcPixel[3] * (unsigned int)tint.a) / 255;

            if (srcAlpha == 0) continue; // Source pixel is fully transparent, do nothing.

            // Apply the tint to the source RGB channels
            unsigned int srcR = ((unsigned int)srcPixel[0] * (unsigned int)tint.r) / 255;
            unsigned int srcG = ((unsigned int)srcPixel[1] * (unsigned int)tint.g) / 255;
            unsigned int srcB = ((unsigned int)srcPixel[2] * (unsigned int)tint.b) / 255;

            if (srcAlpha == 255) {
                // Opaque: a simple overwrite is fastest
                dstPixel[0] = (unsigned char)srcR;
                dstPixel[1] = (unsigned char)srcG;
                dstPixel[2] = (unsigned char)srcB;
                dstPixel[3] = (unsigned char)srcAlpha; // Or just 255
            } else {
                // Standard alpha blending (SRC over DST)
                float alphaFactor = (float)srcAlpha / 255.0f;
                float oneMinusAlpha = 1.0f - alphaFactor;

                dstPixel[0] = (unsigned char)((float)srcR * alphaFactor + (float)dstPixel[0] * oneMinusAlpha);
                dstPixel[1] = (unsigned char)((float)srcG * alphaFactor + (float)dstPixel[1] * oneMinusAlpha);
                dstPixel[2] = (unsigned char)((float)srcB * alphaFactor + (float)dstPixel[2] * oneMinusAlpha);
                dstPixel[3] = (unsigned char)((float)srcAlpha + (float)dstPixel[3] * oneMinusAlpha); // Blend destination alpha too
            }
        }
    }
}

/**
 * @brief [INTERNAL] Samples a pixel value from a single-channel bitmap using bilinear filtering.
 * @details This helper function retrieves a color value from a bitmap at a floating-point coordinate. Instead of simply picking the nearest pixel (point sampling), it interpolates between the four nearest pixels to produce a smoother, higher-quality result.
 *          This is essential for rendering transformed text or images where the source pixels do not map perfectly one-to-one with the destination pixels, as it reduces aliasing and "jaggies".
 *
 * @param bitmap A pointer to the raw, single-channel (e.g., alpha-only) bitmap data.
 * @param width The width of the bitmap in pixels.
 * @param height The height of the bitmap in pixels.
 * @param u The horizontal (X) coordinate to sample from.
 * @param v The vertical (Y) coordinate to sample from.
 *
 * @return The interpolated `unsigned char` value (0-255) at the specified coordinates.
 *
 * @note The function clamps the U/V coordinates to be safely within the bitmap's bounds to prevent out-of-bounds memory access.
 * @note This function is for internal use, primarily by the advanced text rendering path (`SituationImageDrawCodepoint`) for sampling the Signed Distance Field bitmap.
 */
static unsigned char _SituationBilinearSample(const unsigned char *bitmap, int width, int height, float u, float v) {
    // Clamp coordinates to be within the bitmap
    u = _SituationClampf(u, 0.0f, (float)width - 1.001f);
    v = _SituationClampf(v, 0.0f, (float)height - 1.001f);

    int x = (int)u;
    int y = (int)v;
    float u_ratio = u - x;
    float v_ratio = v - y;
    float u_opposite = 1.0f - u_ratio;
    float v_opposite = 1.0f - v_ratio;

    unsigned char c11 = bitmap[y * width + x];
    unsigned char c12 = bitmap[y * width + x + 1];
    unsigned char c21 = bitmap[(y + 1) * width + x];
    unsigned char c22 = bitmap[(y + 1) * width + x + 1];

    float result = (c11 * u_opposite + c12 * u_ratio) * v_opposite + (c21 * u_opposite + c22 * u_ratio) * v_ratio;

    return (unsigned char)result;
}

/**
 * @brief Draws a single character (codepoint) onto a SituationImage with advanced styling.
 * @details This is a powerful, low-level function for high-quality text rendering. It can render a single character with fill, outline, rotation, and horizontal skew.
 *
 * @par Rendering Method
 *   - **With Outline:** Uses a high-quality Signed Distance Field (SDF) method to render smooth, scalable outlines of a precise thickness.
 *   - **Without Outline:** Uses a simpler, faster anti-aliased bitmap rendering path.
 *   - **With Transformations:** For rotation or skew, it uses an inverse mapping algorithm with bilinear filtering to sample the glyph, ensuring smooth, high-quality results without aliasing ("jaggies").
 *
 * @param[in,out] dst A pointer to the destination `SituationImage` to draw on.
 * @param font The `SituationFont` to use for rendering.
 * @param codepoint The Unicode codepoint of the character to draw.
 * @param position The top-left position for the character's baseline.
 * @param fontSize The desired font size in pixels.
 * @param rotationDegrees The rotation of the character in degrees, pivoting around its baseline start. Positive values rotate counter-clockwise.
 * @param skewFactor A factor for horizontal shearing. `0.0` is no skew. `0.5` will skew the top of the character 50% of its height to the right.
 * @param fillColor The color for the character's interior.
 * @param outlineColor The color for the character's outline.
 * @param outlineThickness The thickness of the outline in pixels. A value of `0.0f` or less disables the outline and uses the faster non-SDF rendering path.
 *
 * @note This function is the building block for `SituationImageDrawTextEx`. It is generally more convenient to use the higher-level functions unless you need to control the placement and rendering of each character individually.
 *
 * @see SituationImageDrawTextEx()
 */
SITAPI void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness) {
    if (!SituationIsImageValid(*dst)) return;

    // --- Bitmap Font Path ---
    if (font.is_bitmap) {
        if (!font.bitmap_data || codepoint < 0 || codepoint >= font.bitmap_count) return;

        int bw = font.bitmap_width;
        int bh = font.bitmap_height;
        float scale = fontSize / (float)bh;

        // Stride is assumed to be 1 byte per 8 pixels (1bpp)
        int stride = (bw + 7) / 8;
        const unsigned char* char_ptr = &font.bitmap_data[codepoint * (stride * bh)];

        float angleRad = rotationDegrees * (M_PI / 180.0f);
        float cos_a = cosf(angleRad);
        float sin_a = sinf(angleRad);

        // Center of the glyph for rotation
        float cx = (float)bw / 2.0f - 0.5f;
        float cy = (float)bh / 2.0f - 0.5f;

        for (int y = 0; y < bh; y++) {
            for (int x = 0; x < bw; x++) {
                // Check bit (row-major 1bpp)
                int byte_idx = y * stride + (x / 8);
                int bit_idx = 7 - (x % 8);
                bool is_set = (char_ptr[byte_idx] >> bit_idx) & 1;

                if (is_set) {
                    // Local coordinate relative to center
                    float lx = (float)x - cx;
                    float ly = (float)y - cy;

                    // Apply skew
                    lx += ly * skewFactor;

                    // Apply rotation
                    float rx = lx * cos_a - ly * sin_a;
                    float ry = lx * sin_a + ly * cos_a;

                    // Apply scale and translate to target position
                    // We assume position is top-left of the glyph box
                    float screen_cx = position.x + (bw * scale) / 2.0f;
                    float screen_cy = position.y + (bh * scale) / 2.0f;

                    // Pixel center in screen space
                    float px = screen_cx + rx * scale;
                    float py = screen_cy + ry * scale;

                    // Draw the scaled block ("splat")
                    // We use ceil to ensure at least 1 pixel is drawn
                    int block_size = (int)ceilf(scale);

                    int start_bx = (int)(px - scale/2.0f);
                    int start_by = (int)(py - scale/2.0f);

                    for(int bx = 0; bx < block_size; ++bx) {
                        for(int by = 0; by < block_size; ++by) {
                            int final_x = start_bx + bx;
                            int final_y = start_by + by;
                            if (final_x >= 0 && final_x < dst->width && final_y >= 0 && final_y < dst->height) {
                                // Direct alpha blend
                                ColorRGBA *dstPixel = &((ColorRGBA *)dst->data)[final_y * dst->width + final_x];
                                *dstPixel = _SituationColorAlphaBlend(*dstPixel, fillColor, 1.0f);
                            }
                        }
                    }
                }
            }
        }
        return;
    }

    // --- TrueType Path (Standard) ---
    if (!font.stbFontInfo) return;

    // --- Common Setup (Refined to reduce duplication) ---
    stbtt_fontinfo *info = (stbtt_fontinfo*)font.stbFontInfo;
    float scale = stbtt_ScaleForPixelHeight(info, fontSize);
    int ascent, descent;
    stbtt_GetFontVMetrics(info, &ascent, &descent, NULL);
    int baseline = (int)(ascent * scale);

    // --- Path 1: Optimized for non-transformed characters ---
    if (rotationDegrees == 0.0f && skewFactor == 0.0f) {
        // --- Sub-Path 1.1: Simple bitmap rendering (no outline) ---
        if (outlineThickness <= 0) {
            int g_x0, g_y0, g_x1, g_y1;
            stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &g_x0, &g_y0, &g_x1, &g_y1);
            int glyph_w = g_x1 - g_x0;
            int glyph_h = g_y1 - g_y0;
            if (glyph_w > 0 && glyph_h > 0) {
                unsigned char *glyphBitmap = (unsigned char*)SIT_CALLOC(glyph_w * glyph_h, sizeof(unsigned char));
                if (!glyphBitmap) return;
                stbtt_MakeCodepointBitmap(info, glyphBitmap, glyph_w, glyph_h, glyph_w, scale, scale, codepoint);
                for (int y = 0; y < glyph_h; ++y) {
                    for (int x = 0; x < glyph_w; ++x) {
                        int dx = (int)position.x + g_x0 + x;
                        int dy = (int)position.y + baseline + g_y0 + y;
                        if (dx >= 0 && dx < dst->width && dy >= 0 && dy < dst->height) {
                            unsigned char alpha = glyphBitmap[y * glyph_w + x];
                            if (alpha > 0) {
                                ColorRGBA *dstPixel = &((ColorRGBA *)dst->data)[dy * dst->width + dx];
                                *dstPixel = _SituationColorAlphaBlend(*dstPixel, fillColor, (float)alpha / 255.0f);
                            }
                        }
                    }
                }
                SIT_FREE(glyphBitmap);
            }
        }
        // --- Sub-Path 1.2: SDF rendering (with outline) ---
        else {
            const int padding = (int)outlineThickness + 2;
            const unsigned char onedge_value = 180;
            const float pixel_dist_scale = (float)onedge_value / (float)padding;
            int glyph_w, glyph_h, g_x0, g_y0;
            unsigned char *sdfBitmap = stbtt_GetCodepointSDF(info, scale, codepoint, padding, onedge_value, pixel_dist_scale, &glyph_w, &glyph_h, &g_x0, &g_y0);
            if (sdfBitmap) {
                float fill_thresh_inner = onedge_value - 1.0f, fill_thresh_outer = onedge_value + 1.0f;
                float outline_thresh_inner = onedge_value - (outlineThickness * pixel_dist_scale) - 1.0f;
                float outline_thresh_outer = onedge_value - (outlineThickness * pixel_dist_scale) + 1.0f;
                for (int y = 0; y < glyph_h; ++y) {
                    for (int x = 0; x < glyph_w; ++x) {
                        int dx = (int)position.x + g_x0 + x;
                        int dy = (int)position.y + baseline + g_y0 + y;
                        if (dx >= 0 && dx < dst->width && dy >= 0 && dy < dst->height) {
                            float dist = (float)sdfBitmap[y * glyph_w + x];
                            float alpha_fill = _SituationClampf((dist - fill_thresh_inner) / (fill_thresh_outer - fill_thresh_inner), 0.0f, 1.0f);
                            float alpha_outline = 1.0f - _SituationClampf((dist - outline_thresh_inner) / (outline_thresh_outer - outline_thresh_inner), 0.0f, 1.0f);
                            if (alpha_fill > 0 || alpha_outline > 0) {
                                ColorRGBA *dstPixel = &((ColorRGBA *)dst->data)[dy * dst->width + dx];
                                if (alpha_outline > 0) *dstPixel = _SituationColorAlphaBlend(*dstPixel, outlineColor, alpha_outline);
                                if (alpha_fill > 0) *dstPixel = _SituationColorAlphaBlend(*dstPixel, fillColor, alpha_fill);
                            }
                        }
                    }
                stbtt_FreeSDF(sdfBitmap, info->userdata);
                }
            }
        }
        return; // End of fast path
    }

    // --- Path 2: Inverse mapping for rotation and skew ---
    // (This part of the code was already well-structured and bug-free)
    const int padding = (int)fmaxf(outlineThickness + 5.0f, 5.0f);
    const unsigned char onedge_value = 180;
    const float pixel_dist_scale = (float)onedge_value / (float)padding;
    int glyph_w, glyph_h, g_x0, g_y0;
    unsigned char *sdfBitmap = stbtt_GetCodepointSDF(info, scale, codepoint, padding, onedge_value, pixel_dist_scale, &glyph_w, &glyph_h, &g_x0, &g_y0);
    if (!sdfBitmap) return;

    float angleRad = rotationDegrees * (M_PI / 180.0f);
    float cos_a = cosf(angleRad), sin_a = sinf(angleRad);
    Vector2 pivot = { position.x, position.y + baseline };

    mat2 invTransform;
    float det = cos_a - sin_a * skewFactor;
    if (fabsf(det) < 1e-6) { stbtt_FreeSDF(sdfBitmap, info->userdata); return; }
    float inv_det = 1.0f / det;
    invTransform[0][0] = 1.0f * inv_det;
    invTransform[0][1] = sin_a * inv_det;
    invTransform[1][0] = -skewFactor * inv_det;
    invTransform[1][1] = cos_a * inv_det;

    vec2 corners[4] = { {(float)g_x0, (float)g_y0}, {(float)g_x0 + glyph_w, (float)g_y0}, {(float)g_x0, (float)g_y0 + glyph_h}, {(float)g_x0 + glyph_w, (float)g_y0 + glyph_h} };
    float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
    for (int i = 0; i < 4; i++) {
        float tx = corners[i][0] * cos_a - corners[i][1] * sin_a + corners[i][0] * skewFactor;
        float ty = corners[i][0] * sin_a + corners[i][1] * cos_a;
        min_x = fminf(min_x, tx); max_x = fmaxf(max_x, tx);
        min_y = fminf(min_y, ty); max_y = fmaxf(max_y, ty);
    }

    int startX = (int)floorf(pivot.x + min_x), endX = (int)ceilf(pivot.x + max_x);
    int startY = (int)floorf(pivot.y + min_y), endY = (int)ceilf(pivot.y + max_y);

    float fill_thresh_inner = onedge_value - 1.0f, fill_thresh_outer = onedge_value + 1.0f;
    float outline_thresh_inner = onedge_value - (outlineThickness * pixel_dist_scale) - 1.0f;
    float outline_thresh_outer = onedge_value - (outlineThickness * pixel_dist_scale) + 1.0f;

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            if (x < 0 || x >= dst->width || y < 0 || y >= dst->height) continue;
            vec2 dst_p = {(float)x - pivot.x, (float)y - pivot.y};
            vec2 src_p;
            glm_mat2_mulv(invTransform, dst_p, src_p);
            float src_x = src_p[0] - g_x0, src_y = src_p[1] - g_y0;
            if (src_x < -1 || src_x > glyph_w || src_y < -1 || src_y > glyph_h) continue;

            float dist = (float)_SituationBilinearSample(sdfBitmap, glyph_w, glyph_h, src_x, src_y);
            float alpha_fill = _SituationClampf((dist - fill_thresh_inner) / (fill_thresh_outer - fill_thresh_inner), 0.0f, 1.0f);
            float alpha_outline = 1.0f - _SituationClampf((dist - outline_thresh_inner) / (outline_thresh_outer - outline_thresh_inner), 0.0f, 1.0f);

            if (alpha_fill > 0 || alpha_outline > 0) {
                ColorRGBA *dstPixel = &((ColorRGBA *)dst->data)[y * dst->width + x];
                if (outlineThickness > 0 && alpha_outline > 0) *dstPixel = _SituationColorAlphaBlend(*dstPixel, outlineColor, alpha_outline);
                if (alpha_fill > 0) *dstPixel = _SituationColorAlphaBlend(*dstPixel, fillColor, alpha_fill);
            }
        }
    }
    stbtt_FreeSDF(sdfBitmap, info->userdata);
}

/**
 * @brief Draws a string of text onto an image with advanced options for styling and transformation.
 * @details This is the recommended function for all stylistic text rendering. It orchestrates calls to the internal `SituationImageDrawCodepoint` function to render each character of a string, correctly handling kerning and character spacing along the transformed baseline.
 *
 * @par Rendering Method
 *   This function uses a Signed Distance Field (SDF) rendering path via `SituationImageDrawCodepoint` to achieve high-quality, anti-aliased outlines and smooth rendering, even when rotated or skewed. For non-rotated text without an outline, it uses a faster, optimized path.
 *
 * @param[in,out] dst The destination image to draw on.
 * @param font The `SituationFont` to use for rendering.
 * @param text The null-terminated string to draw.
 * @param position The top-left anchor position for the text. Transformations pivot around this point.
 * @param fontSize The desired font size in pixels.
 * @param spacing Additional spacing between characters in pixels, applied along the text's baseline.
 * @param rotationDegrees The rotation of the entire text block in degrees. `0` is no rotation. Positive values rotate counter-clockwise.
 * @param skewFactor A factor for horizontal shearing applied to each character. `0.0` is no skew.
 * @param fillColor The color for the characters' interior.
 * @param outlineColor The color for the characters' outline.
 * @param outlineThickness The thickness of the outline in pixels. A value of `0.0f` or less disables the outline.
 *
 * @see SituationImageDrawText(), SituationImageDrawCodepoint(), SituationMeasureText()
 */
SITAPI void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness) {
    if (!SituationIsImageValid(*dst) || !text) return;

    // --- Bitmap Path ---
    if (font.is_bitmap) {
        float scale = fontSize / (float)font.bitmap_height;
        float bw = font.bitmap_width;

        // Simple linear layout, supports rotation
        Vector2 cursor = position;
        float angleRad = rotationDegrees * (M_PI / 180.0f);
        float cos_a = cosf(angleRad);
        float sin_a = sinf(angleRad);

        for (int i = 0; text[i]; ++i) {
            int codepoint = (unsigned char)text[i]; // Cast to prevent sign extension indices

            // Draw character (handles its own local rotation/scale)
            SituationImageDrawCodepoint(dst, font, codepoint, cursor, fontSize, rotationDegrees, skewFactor, fillColor, outlineColor, outlineThickness);

            // Advance cursor
            float advance = (bw * scale) + spacing;
            cursor.x += advance * cos_a;
            cursor.y += advance * sin_a;
        }
        return;
    }

    // --- TrueType Path ---
    if (!font.stbFontInfo) return;

    stbtt_fontinfo *info = (stbtt_fontinfo*)font.stbFontInfo;
    float scale = stbtt_ScaleForPixelHeight(info, fontSize);

    // --- Optimization: Use a faster path for the common, non-rotated case ---
    if (rotationDegrees == 0.0f) {
        float x = position.x;
        for (int i = 0; text[i]; ++i) {
            int codepoint = text[i];

            if (i > 0) {
                x += stbtt_GetCodepointKernAdvance(info, text[i-1], codepoint) * scale;
            }

            // Draw the character using our powerful function, but at its simple position
            Vector2 charPos = { x, position.y };
            SituationImageDrawCodepoint(dst, font, codepoint, charPos, fontSize, 0.0f, skewFactor, fillColor, outlineColor, outlineThickness);

            // Advance the simple horizontal cursor
            int advanceWidth;
            stbtt_GetCodepointHMetrics(info, codepoint, &advanceWidth, NULL);
            x += ((float)advanceWidth * scale) + spacing;
        }
        return;
    }

    // --- Transformation Path: Use vector math to handle rotated layout ---
    Vector2 cursor = position;
    float angleRad = rotationDegrees * (M_PI / 180.0f);
    float cos_a = cosf(angleRad);
    float sin_a = sinf(angleRad);

    for (int i = 0; text[i]; ++i) {
        int codepoint = text[i];

        // Handle kerning along the rotated baseline
        if (i > 0) {
            float kern = stbtt_GetCodepointKernAdvance(info, text[i-1], codepoint) * scale;
            // The kerning is a horizontal vector that we must rotate
            cursor.x += kern * cos_a;
            cursor.y += kern * sin_a;
        }

        // Draw the current character at the cursor's position, applying all transforms
        SituationImageDrawCodepoint(dst, font, codepoint, cursor, fontSize, rotationDegrees, skewFactor, fillColor, outlineColor, outlineThickness);

        // Advance the cursor along the rotated baseline for the next character
        int advanceWidth;
        stbtt_GetCodepointHMetrics(info, codepoint, &advanceWidth, NULL);
        float totalAdvance = ((float)advanceWidth * scale) + spacing;

        // The advance is a horizontal vector that we must rotate
        cursor.x += totalAdvance * cos_a;
        cursor.y += totalAdvance * sin_a;
    }
}

/**
 * @brief Draws simple, tinted text onto an image using a basic bitmap-compositing approach.
 * @details This function serves as the classic, straightforward method for rendering text. For each character in the input string, it performs the following steps:
 *          1. Generates a 1-channel (alpha-only) bitmap of the character using stb_truetype.
 *          2. Creates a temporary 4-channel (RGBA) `SituationImage` in memory to hold the glyph.
 *          3. Copies the 1-channel bitmap into the alpha channel of the temporary image.
 *          4. Calls the general-purpose `SituationImageDrawAlpha` function to composite this temporary glyph image onto the destination, applying the requested tint color.
 *          5. Frees the memory for the temporary glyph image.
 * @section Relationship to SituationImageDrawTextEx
 *   This function is **intentionally separate** from `SituationImageDrawTextEx` and does not call it.
 *   They use fundamentally different rendering techniques for different use cases:
 *   - **SituationImageDrawText (this function):** Uses `stbtt_MakeCodepointBitmap`. It is optimized for simplicity and is perfect for rendering basic, aliased, solid-color text. It cannot produce outlines.
 *   - **SituationImageDrawTextEx:** Uses a more complex Signed Distance Field (SDF) rendering path via the internal `SituationImageDrawCodepoint` function. This path is required to render high-quality, anti-aliased outlines of a specified thickness, but has a slightly higher initial processing cost.
 *   By keeping them separate, the library provides two distinct tools: a simple one for basic needs and an advanced one for stylistic text.
 * @note **Performance Considerations:** Because this function allocates and frees a temporary image for every single character drawn, it is not recommended for text that needs to be redrawn every frame (e.g., a rapidly changing score counter).
 *       For performance-critical text, it is best to pre-render the required characters to a single font atlas image and then use `SituationImageDrawAlpha` to blit the characters from the atlas.
 * @param dst The destination image to be modified.
 * @param font The `SituationFont` to use for rendering the characters.
 * @param text The null-terminated string to be drawn.
 * @param position The top-left position on the destination image for the baseline of the first character.
 * @param fontSize The height of the font in pixels.
 * @param spacing Additional horizontal space to add between each character.
 * @param tint The color to apply to the text. The text will be rendered in this color.
 */
SITAPI void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint) {
    if (!SituationIsImageValid(*dst) || !font.stbFontInfo || !text) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationImageDrawText: invalid dst image, font, or text"); return; }

    stbtt_fontinfo *info = (stbtt_fontinfo*)font.stbFontInfo;
    float scale = stbtt_ScaleForPixelHeight(info, fontSize);

    // Get vertical font metrics to correctly align characters on the baseline
    int ascent, descent;
    stbtt_GetFontVMetrics(info, &ascent, &descent, NULL);
    int baseline = (int)(ascent * scale);

    float x = position.x;

    for (int i = 0; text[i]; ++i) {
        int codepoint = text[i];

        // Add kerning for the previous character
        if (i > 0) {
            x += stbtt_GetCodepointKernAdvance(info, text[i-1], codepoint) * scale;
        }

        // --- Step 1: Generate the character's bitmap using stb_truetype ---
        int g_x0, g_y0, g_x1, g_y1;
        stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &g_x0, &g_y0, &g_x1, &g_y1);

        int glyph_w = g_x1 - g_x0;
        int glyph_h = g_y1 - g_y0;

        if (glyph_w > 0 && glyph_h > 0) {
            // Render the 1-channel alpha glyph from stb_truetype
            unsigned char *glyphBitmap = (unsigned char*)SIT_MALLOC(glyph_w * glyph_h);
            stbtt_MakeCodepointBitmap(info, glyphBitmap, glyph_w, glyph_h, glyph_w, scale, scale, codepoint);

            // --- Step 2: Create a temporary 4-channel SituationImage for our compositing function ---
            // Note: We create a white image with the glyph's alpha, because the 'tint' parameter in SituationImageDrawAlpha will provide the final color.
            SituationImage glyphImage = {0};
            glyphImage.width = glyph_w;
            glyphImage.height = glyph_h;
            glyphImage.data = SIT_MALLOC(glyph_w * glyph_h * 4);

            unsigned char* glyphPixels = (unsigned char*)glyphImage.data;
            for (int p = 0; p < glyph_w * glyph_h; ++p) {
                glyphPixels[p*4 + 0] = 255; // R
                glyphPixels[p*4 + 1] = 255; // G
                glyphPixels[p*4 + 2] = 255; // B
                glyphPixels[p*4 + 3] = glyphBitmap[p]; // Alpha comes from the rendered glyph
            }
            SIT_FREE(glyphBitmap);

            // --- Step 3: Use our powerful, generic drawing function to do the hard work! ---
            SitRectangle srcRect = { 0, 0, (float)glyph_w, (float)glyph_h };
            Vector2 dstPos = { (float)((int)x + g_x0), (float)((int)position.y + baseline + g_y0) };

            SituationImageDrawAlpha(dst, glyphImage, srcRect, dstPos, tint);

            // --- Step 4: Clean up the temporary glyph image ---
            SituationUnloadImage(glyphImage);
        }

        // Advance the cursor for the next character
        int advanceWidth;
        stbtt_GetCodepointHMetrics(info, codepoint, &advanceWidth, NULL);
        x += ((float)advanceWidth * scale) + spacing;
    }
}

/**
 * @brief Draws text onto an image using `printf`-style formatting.
 *
 * @details A convenience wrapper that formats a string and then draws it using `SituationImageDrawText`.
 *          Useful for displaying dynamic values like scores or debug info without manually managing string buffers.
 *
 * @param dst The destination image.
 * @param font The font to use.
 * @param position The top-left position.
 * @param fontSize Font size in pixels.
 * @param spacing Character spacing adjustment.
 * @param tint Color tint.
 * @param fmt The format string (e.g., "Score: %d").
 * @param ... Additional arguments matching the format string.
 *
 * @warning Uses an internal 1024-byte stack buffer. Truncates text that exceeds this length.
 */
SITAPI void SituationImageDrawTextFormatted(SituationImage *dst, SituationFont font, Vector2 position, float fontSize, float spacing, ColorRGBA tint, const char* fmt, ...) {
    if (!fmt) return;

    // Create a temporary buffer for the formatted string.
    // 1024 characters should be sufficient for any single UI label.
    char buffer[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Delegate to the existing simple text drawer
    SituationImageDrawText(dst, font, buffer, position, fontSize, spacing, tint);
}

/**
 * @brief Calculates the bounding box of a text string without rendering it.
 * @details This function determines the width and height that the specified text would occupy if drawn with the given font and size. It accounts for the font's vertical metrics (ascent and descent) and the horizontal advance of each character, including kerning pairs.
 *          This is essential for UI layout, such as centering text, creating buttons of the correct size, or implementing word-wrapping logic.
 *
 * @param font The `SituationFont` to use for measurement.
 * @param text The null-terminated string to measure.
 * @param fontSize The desired font size in pixels.
 *
 * @return A `SitRectangle` struct where `width` and `height` contain the measured dimensions. The `x` and `y` fields are set to 0.
 * @return A `SitRectangle` with `width` and `height` of 0 if the library is not initialized, the font is invalid, or the text is NULL.
 *
 * @note The returned height is the font's line height (ascent - descent), which is consistent for all strings using that font and size. The width is specific to the provided text.
 *
 * @see SituationImageDrawText(), SituationImageDrawTextEx()
 */
SITAPI SitRectangle SituationMeasureText(SituationFont font, const char *text, float fontSize) {
    return SituationMeasureTextEx(font, text, fontSize, 0.0f);
}

/**
 * @brief Captures the current contents of the main window's backbuffer into a CPU-side image.
 * @details This function reads the pixel data directly from the GPU's framebuffer, providing a snapshot of the most recently rendered frame. This is the core operation for taking screenshots or for enabling CPU-based image processing effects on the final rendered image.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glReadPixels` to read from the default framebuffer. The resulting image is vertically flipped, so the function automatically calls `SituationImageFlip` to correct the orientation. Output is always **RGBA8** (`GL_UNSIGNED_BYTE`); drivers quantize 10-bit framebuffers at readback (Phase 3).
 * - **Vulkan:** Copies from the swapchain via pre-present staging (`vkCmdCopyImageToBuffer`). Rows are top-left origin (+Y down), matching on-screen 2D rendering (Phase 7-bis: shared ortho + shader V convention; no readback flip). **No** `SituationImageFlip` on the cached path. **10-bit swapchains** (`A2R10G10B10`) are downconverted to RGBA8 in `_SituationVulkanCopyMappedColorToRGBA`.
 *
 * @warning This function allocates new memory for the `image.data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()` on the returned `SituationImage`. Failure to do so will result in a memory leak.
 * @warning This can be a slow operation, as it requires synchronization with the GPU and a potentially large data transfer from VRAM to system RAM. Avoid calling it in performance-critical loops.
 *
 * @return A new `SituationImage` containing the pixel data, width, and height of the screen. Returns a zeroed (invalid) struct on failure (e.g., if out of memory or not implemented).
 *
 * @see SituationUnloadImage(), SituationTakeScreenshot(), SituationRequestScreenCapture()
 */
SITAPI void SituationRequestScreenCapture(void) {
    if (!SituationIsInitialized()) {
        return;
    }
#if defined(SITUATION_USE_OPENGL)
    sit_render.gl.screenshot_valid = false;
    sit_render.gl.screenshot_resolved_frame_index = -1;
    sit_render.gl.screenshot_requested = true;
#elif defined(SITUATION_USE_VULKAN)
    sit_render.vk.screenshot_valid = false;
    sit_render.vk.screenshot_requested = true;
#endif
}

SITAPI SituationError SituationLoadImageFromScreen(SituationImage* out_image) {
    if (!out_image) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(SituationImage));

    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    // Get the dimensions of the framebuffer (HiDPI-aware)
    int width = SituationGetRenderWidth();
    int height = SituationGetRenderHeight();
#if defined(SITUATION_USE_VULKAN)
    // Pre-present screenshot cache uses swapchain_extent; match it so the cached path hits (GLFW vs swapchain can differ transiently).
    if (sit_render.vk.swapchain_valid && sit_render.vk.swapchain_extent.width > 0u && sit_render.vk.swapchain_extent.height > 0u) {
        width = (int)sit_render.vk.swapchain_extent.width;
        height = (int)sit_render.vk.swapchain_extent.height;
    }
#endif
	if (width == 0 || height == 0) {
		return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot capture screen: render surface invalid (width/height=0)");
	}

	out_image->width = width;
    out_image->height = height;

#if defined(SITUATION_USE_OPENGL)
    // Allocate memory for the raw pixel data (RGBA, 8 bits per channel)
    out_image->data = SIT_MALLOC(width * height * 4);
	if (!out_image->data) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Screenshot pixel buffer allocation failed (%dx%d RGBA)", width, height);
		return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, err_msg);
	}

#if !defined(__STDC_NO_THREADS__) && defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled && atomic_load(&sit_render.thread_active)) {
        const int expected_slot = (sit_render.current_frame_index + SITUATION_MAX_FRAMES_IN_FLIGHT - 1)
            % SITUATION_MAX_FRAMES_IN_FLIGHT;
        if (expected_slot >= 0 && expected_slot < SITUATION_MAX_FRAMES_IN_FLIGHT) {
            atomic_store_explicit(&sit_render.gl.screenshot_urgent[expected_slot], 1, memory_order_release);
        }
        atomic_thread_fence(memory_order_acquire);
        const bool cache_hit = sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
            sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height &&
            sit_render.gl.screenshot_resolved_frame_index == expected_slot &&
            sit_render.gl.screenshot_buffer_epoch == sit_render.gl.screenshot_capture_epoch;

        if (!cache_hit) {
            /* Give the render thread poll windows (pre-swap or post-present late path). */
            for (int pre = 0; pre < 64; ++pre) {
                atomic_thread_fence(memory_order_acquire);
                if (sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
                    sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height &&
                    sit_render.gl.screenshot_resolved_frame_index == expected_slot &&
                    sit_render.gl.screenshot_buffer_epoch == sit_render.gl.screenshot_capture_epoch) {
                    break;
                }
                if (sit_gs.sit_glfw_window) {
                    glfwPollEvents();
                }
                thrd_yield();
            }
        }

        for (int spin = 0; spin < 2000; ++spin) {
            atomic_thread_fence(memory_order_acquire);
            if (sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
                sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height &&
                sit_render.gl.screenshot_resolved_frame_index == expected_slot &&
                sit_render.gl.screenshot_buffer_epoch == sit_render.gl.screenshot_capture_epoch) {
                break;
            }
            if (sit_gs.sit_glfw_window) {
                glfwPollEvents();
            }
            SITUATION_SLEEP_MS(1);
        }

        atomic_thread_fence(memory_order_acquire);
        if (sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
            sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height &&
            sit_render.gl.screenshot_resolved_frame_index == expected_slot &&
            sit_render.gl.screenshot_buffer_epoch == sit_render.gl.screenshot_capture_epoch) {
            if (sit_render.gl.screenshot_mutex_initialized) {
                mtx_lock(&sit_render.gl.screenshot_mutex);
            }
            memcpy(out_image->data, sit_render.gl.screenshot_buffer, (size_t)width * height * 4);
            if (sit_render.gl.screenshot_mutex_initialized) {
                mtx_unlock(&sit_render.gl.screenshot_mutex);
            }
        } else {
            SIT_FREE(out_image->data); out_image->data = NULL;
            memset(out_image, 0, sizeof(SituationImage));
#ifndef NDEBUG
            static bool s_urgent_timeout_warned = false;
            if (!s_urgent_timeout_warned) {
                s_urgent_timeout_warned = true;
                fprintf(stderr,
                    "[Situation] LoadImageFromScreen: urgent capture timed out — call promptly after EndFrame "
                    "or use SituationRequestScreenCapture() before EndFrame.\n"
                    "[Situation]   See doc/architecture.md (Frame Loop Contract) and doc/plan/GAME_LOOP_PERFORMANCE_PLAN.md.\n");
            }
#endif
            return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED,
                "Screenshot capture timed out waiting for render thread");
        }
        SituationImageFlip(out_image, SIT_FLIP_VERTICAL);
        return SITUATION_SUCCESS;
    }
#endif

    // Non-threaded path (or render thread not active): GL context is on the main thread.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    SIT_CHECK_GL_ERROR();

    // [FIX v2.4.39] Ensure all GPU operations are complete before reading pixels.
    // This is critical for VD compositing which may still be in-flight.
    glFinish();

    // [Phase 1] ST path: prefer pre-swap cache from EndFrame; else sync readback.
    if (sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
        sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height &&
        sit_render.gl.screenshot_buffer_epoch == sit_render.gl.screenshot_capture_epoch) {
        if (sit_render.gl.screenshot_mutex_initialized) {
            mtx_lock(&sit_render.gl.screenshot_mutex);
        }
        memcpy(out_image->data, sit_render.gl.screenshot_buffer, (size_t)width * height * 4);
        if (sit_render.gl.screenshot_mutex_initialized) {
            mtx_unlock(&sit_render.gl.screenshot_mutex);
        }
    } else {
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_image->data);
        GLenum err = glGetError();

        if (err != GL_NO_ERROR) {
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_image->data);
            err = glGetError();
        } else {
            bool all_black = true;
            uint8_t* check = (uint8_t*)out_image->data;
            int check_limit = (width * height * 4 < 4096) ? width * height * 4 : 4096;
            for (int sample = 0; sample < check_limit; sample += 16) {
                if (check[sample] != 0 || check[sample + 1] != 0 || check[sample + 2] != 0) {
                    all_black = false;
                    break;
                }
            }
            if (all_black) {
                glReadBuffer(GL_FRONT);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_image->data);
                err = glGetError();
            }
        }

        glReadBuffer(GL_BACK);

        if (err != GL_NO_ERROR) {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "glReadPixels failed (GL error: %d)", err);
            SIT_FREE(out_image->data); out_image->data = NULL;
            return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, err_msg);
        }
    }
    SituationImageFlip(out_image, SIT_FLIP_VERTICAL);
#elif defined(SITUATION_USE_VULKAN)
    /* Sync the last submitted frame only (prev slot after EndFrame advances current_frame_index).
     * Waiting every in-flight fence serially added large latency; vkDeviceWaitIdle was worse (unbounded). */
    if (sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.in_flight_fences && sit_render.vk.max_frames_in_flight > 0) {
        uint32_t mf = sit_render.vk.max_frames_in_flight;
        uint32_t prev = (sit_render.vk.current_frame_index + mf - 1u) % mf;
#if !defined(__STDC_NO_THREADS__) && defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            for (int spin = 0; spin < 2000 && sit_render.frames_pending > 0; ++spin) {
                if (sit_gs.sit_glfw_window) {
                    _SituationPumpWindowEventsGuarded();
                }
                thrd_yield();
            }
        }
#endif
        VkResult wr = _SituationVulkanWaitFencePumpWindow(sit_render.vk.device, sit_render.vk.in_flight_fences[prev]);
        if (wr == VK_TIMEOUT) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED,
                "Timed out waiting for GPU fence before screenshot readback (see SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS)");
        }
        if (wr != VK_SUCCESS) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Fence wait failed before Vulkan screenshot readback");
        }
        _SituationVulkanEnsureScreenshotResolvedForFrame(prev);
    }
    // Prefer pre-present capture from SituationEndFrame (same role as OpenGL screenshot_buffer; reliable on all drivers).
    if (sit_render.vk.screenshot_valid && sit_render.vk.screenshot_buffer &&
        sit_render.vk.screenshot_width == width && sit_render.vk.screenshot_height == height &&
        sit_render.vk.max_frames_in_flight > 0u) {
        uint32_t mf = sit_render.vk.max_frames_in_flight;
        uint32_t prev_slot = (sit_render.vk.current_frame_index + mf - 1u) % mf;
        if (sit_render.vk.screenshot_resolved_frame_index != prev_slot) {
            sit_render.vk.screenshot_valid = false;
        }
    }
    if (sit_render.vk.screenshot_valid && sit_render.vk.screenshot_buffer &&
        sit_render.vk.screenshot_width == width && sit_render.vk.screenshot_height == height) {
        out_image->data = SIT_MALLOC((size_t)width * height * 4);
        if (!out_image->data) {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "Screenshot pixel buffer allocation failed (%dx%d RGBA)", width, height);
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, err_msg);
        }
        if (sit_render.vk.screenshot_mutex_initialized) {
            mtx_lock(&sit_render.vk.screenshot_mutex);
        }
        memcpy(out_image->data, sit_render.vk.screenshot_buffer, (size_t)width * height * 4);
        if (sit_render.vk.screenshot_mutex_initialized) {
            mtx_unlock(&sit_render.vk.screenshot_mutex);
        }
        return SITUATION_SUCCESS;
    }

    // Fallback: read swapchain image (may be unreliable after vkQueuePresentKHR on some platforms).
    uint32_t src_idx = sit_render.vk.last_presented_image_index;
    if (src_idx >= sit_render.vk.swapchain_image_count) {
        src_idx = sit_render.vk.current_image_index;
    }
    VkImage srcImage = sit_render.vk.swapchain_images[src_idx];
    if (srcImage == VK_NULL_HANDLE) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Cannot get screenshot: source swapchain image index %u is invalid", src_idx);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID, err_msg);
    }

    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    out_image->data = _SituationVulkanBlitImageToHostVisibleBuffer(
        srcImage,
        currentLayout,
        (uint32_t)width,
        (uint32_t)height
    );
    if (out_image->data == NULL) {
        return SITUATION_ERROR_TEXTURE_UPLOAD_FAILED;
    }
    /* _SituationVulkanBlitImageToHostVisibleBuffer uses vkCmdCopyImageToBuffer (top row first). */
#endif

    return SITUATION_SUCCESS;
}

/**
 * @brief Captures the current window content and saves it to a PNG file.
 *
 * @details This function reads the backbuffer pixel data and writes it to disk.
 *          It requires `stb_image_write.h` to be implemented in your project.
 *
 * @param fileName The path and name of the file to save (e.g., "screenshots/shot_01.png").
 *                 The filename **must** end in `.png` (case-insensitive).
 *
 * @return `true` if the screenshot was successfully captured and saved.
 * @return `false` if the file extension is invalid, if the library is not initialized,
 *         or if a file I/O error occurs.
 *
 * @warning This is a synchronous operation that stalls the GPU. Do not call every frame.
 */
SITAPI SituationError SituationTakeScreenshot(const char *fileName) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot take screenshot: library not initialized.");
    }

    // 1. Get extension from the table
    const char* ext = sit_screenshot_format_ext[sit_gs.screenshot_format];

    // 2. Build final path
    char path[512];
    if (!fileName || fileName[0] == '\0') {
        snprintf(path, sizeof(path), "screenshot_%llu%s", (unsigned long long)time(NULL), ext);
    } else {
        // Check if the filename already has a valid screenshot extension
        const char* existing_ext = SituationGetFileExtension(fileName);
        bool has_valid_ext = false;
        if (existing_ext) {
            for (int i = 0; i < SIT_SCREENSHOT_FORMAT_COUNT; i++) {
                if (_sit_strcasecmp(existing_ext, sit_screenshot_format_ext[i]) == 0) {
                    has_valid_ext = true;
                    break;
                }
            }
        }
        if (has_valid_ext) {
            snprintf(path, sizeof(path), "%s", fileName);
        } else {
            snprintf(path, sizeof(path), "%s%s", fileName, ext);
        }
    }

    // 3. Validate directory exists
    char* dir = _sit_dirname(path);
    if (dir && dir[0] != '\0' && !_sit_directory_exists(dir)) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Screenshot directory does not exist: %s", dir);
        SituationFreeString(dir);
        return _SituationSetErrorFromCode(SITUATION_ERROR_DIRECTORY_CREATION_FAILED, err_msg);
    }
    SituationFreeString(dir);

    // 4. Capture screen
    SituationImage image = {0};
    SituationError cap_err = SituationLoadImageFromScreen(&image);
    if (cap_err != SITUATION_SUCCESS) return cap_err;

    // 5. Write file in configured format
#if !defined(STB_IMAGE_WRITE_IMPLEMENTATION)
    SituationUnloadImage(image);
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Image write support not available (stb_image_write).");
#else
    int ok = 0;
    switch (sit_gs.screenshot_format) {
        case SIT_SCREENSHOT_PNG:
            ok = stbi_write_png(path, image.width, image.height, 4, image.data, image.width * 4);
            break;
        case SIT_SCREENSHOT_JPG:
            ok = stbi_write_jpg(path, image.width, image.height, 4, image.data, 90);
            break;
        case SIT_SCREENSHOT_TGA:
            ok = stbi_write_tga(path, image.width, image.height, 4, image.data);
            break;
        case SIT_SCREENSHOT_BMP:
        default:
            ok = stbi_write_bmp(path, image.width, image.height, 4, image.data);
            break;
    }
    SituationUnloadImage(image);

    if (!ok) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to write screenshot: %s", path);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, err_msg);
    }
    return SITUATION_SUCCESS;
#endif
}

SITAPI void SituationSetScreenshotFormat(SituationScreenshotFormat format) {
    if (!SituationIsInitialized()) return;
    sit_gs.screenshot_format = format;
}

SITAPI SituationScreenshotFormat SituationGetScreenshotFormat(void) {
    if (!SituationIsInitialized()) return SIT_SCREENSHOT_BMP;
    return sit_gs.screenshot_format;
}

#if defined(_WIN32)
#define SIT_WIN32_WINDOW_ICON_IMPLEMENTATION
#include "platform/windows/situation_win32_window_icon.h"
#endif

/**
 * @brief [INTERNAL] Apply SituationInitInfo::default_window_icon_path after the main window exists.
 * @details Fail-soft: logs a warning and returns without failing init when the path is missing or unloadable.
 */
static void _SituationApplyDefaultWindowIconPath(const char* path_utf8) {
    if (!path_utf8 || !path_utf8[0]) {
        return;
    }
    if (!sit_gs.sit_glfw_window) {
        SIT_DEBUG_LOG("[WARN] default_window_icon_path ignored: no window");
        return;
    }

    const char* ext = SituationGetFileExtension(path_utf8);
#if defined(_WIN32)
    if (ext && (_sit_strcasecmp(ext, "ico") == 0)) {
        SituationImage* images = NULL;
        int count = 0;
        if (_SituationWin32LoadImagesFromIcoPath(path_utf8, &images, &count) && images && count > 0) {
            SituationSetWindowIcons(images, count);
            for (int i = 0; i < count; ++i) {
                SituationUnloadImage(images[i]);
            }
            SIT_FREE(images);
            return;
        }
    }
#endif

    if (ext && _SituationIsStbImageLoadExtensionImpl(ext)) {
        SituationImage image = {0};
        if (SituationLoadImage(path_utf8, &image) == SITUATION_SUCCESS && image.data) {
            SituationSetWindowIcons(&image, 1);
            SituationUnloadImage(image);
            return;
        }
    }

    SIT_DEBUG_LOG("[WARN] default_window_icon_path load failed (path=%s)", path_utf8);
}


#endif // SITUATION_IMPL_IMAGE_H
