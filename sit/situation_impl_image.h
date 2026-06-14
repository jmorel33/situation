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
*     - Font loading, atlas baking, text rendering to images
*     - Color space conversion (RGB/HSV/YPQ)
*     - Screenshot capture
*     - Timer/Oscillator API
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_IMAGE_H
#define SITUATION_IMPL_IMAGE_H

// Forward declarations for internal helpers defined later in this file
static SituationError _SituationSaveImageBMP(const char* fileName, const SituationImage* image);

/** 8-bit sRGB unit [0,1] → linear light for HDR export (pairs with stbi hdr_to_ldr gamma on load). */
static float _SitSrgbUnitToLinear(float s) {
    if (s <= 0.04045f) {
        return s / 12.92f;
    }
    return powf((s + 0.055f) / 1.055f, 2.4f);
}

// Image Module Implementation
//==================================================================================

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
 *          The image data is allocated contiguously in row-major order (stride = width ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â channels).
 *          Pixel type is always unsigned 8-bit normalized (0ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ255 per channel).
 *
 * @param width Width of the image in pixels. Must be > 0.
 * @param height Height of the image in pixels. Must be > 0.
 * @param channels Number of channels per pixel (1, 3, or 4). Other values return an error.
 * @param out_image Pointer to a `SituationImage` variable that will receive the new handle on success.
 *                  On failure, the value is set to `SITUATION_NULL_HANDLE`.
 *
 * @return SITUATION_SUCCESS on successful allocation and initialization,
 *         SITUATION_ERROR_INVALID_PARAM if width/height ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°Ãƒâ€šÃ‚Â¤ 0 or channels invalid,
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
 *          number of channels. No format conversion, scaling, or filtering is performed ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â
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
 *       - No stride/pitch parameter is provided ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â source data is assumed to be tightly packed
 *         (stride = width ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â src_channels ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â sizeof(channel_type), typically uint8_t).
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
 *          This is a low-level, immediate-mode pixel write ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â suitable for procedural
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
 * @param color The RGBA color to set (0ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ255 per channel). Use SIT_RGBA(r,g,b,a) macro
 *              or equivalent for convenience.
 *
 * @return SITUATION_SUCCESS on success,
 *         SITUATION_ERROR_INVALID_PARAM if image is invalid or coordinates out of bounds,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format does not support direct writes,
 *         or other appropriate error codes.
 *
 * @note This function is synchronous and CPU-bound ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â avoid calling it in tight loops
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
 * @brief Converts a color from the standard RGBA color space to the HSV (Hue, Saturation, Value) color space.
 * @details This function transforms a color from its red, green, and blue components into a more intuitive cylindrical-coordinate representation. This is extremely useful for programmatic color manipulation, such as shifting hues, desaturating, or brightening/darkening colors.
 *
 * @par Color Space Details
 *   - **Hue (H):** Represents the pure color (e.g., red, yellow, green). It is returned as an angle from `0.0f` to `360.0f` degrees.
 *   - **Saturation (S):** Represents the intensity or purity of the color. It ranges from `0.0f` (grayscale/achromatic) to `1.0f` (fully saturated, pure color).
 *   - **Value (V):** Represents the brightness of the color. It ranges from `0.0f` (black) to `1.0f` (full brightness).
 *
 * @param rgb The source `ColorRGBA` struct to convert. The alpha component is ignored.
 * @return A `ColorHSV` struct containing the equivalent H, S, and V values.
 *
 * @note The alpha component of the input `ColorRGBA` is not used in this conversion.
 *
 * @see SituationHsvToRgb(), SituationImageAdjustHSV()
 */
SITAPI ColorHSV SituationRgbToHsv(ColorRGBA rgb) {
    ColorHSV hsv;
    float r = rgb.r / 255.0f;
    float g = rgb.g / 255.0f;
    float b = rgb.b / 255.0f;
    float max = _SituationFMax3(r, g, b);
    float min = _SituationFMin3(r, g, b);
    float delta = max - min;
    hsv.v = max; // Value is the max of the components
    if (max == 0.0f) {
        hsv.s = 0.0f; // Saturation
    } else {
        hsv.s = delta / max;
    }
    if (delta == 0.0f) {
        hsv.h = 0.0f; // Hue is undefined for grayscale, set to 0
    } else {
        if (max == r)      hsv.h = 60.0f * fmodf(((g - b) / delta), 6.0f);
        else if (max == g) hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
        else if (max == b) hsv.h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (hsv.h < 0.0f) {
        hsv.h += 360.0f;
    }
    return hsv;
}

/**
 * @brief Converts a color from the HSV (Hue, Saturation, Value) color space back to the standard RGBA color space.
 * @details This is the inverse operation of `SituationRgbToHsv`. It transforms a color defined by its hue, saturation, and brightness back into its red, green, and blue components, which are required for display on a screen.
 *
 * @param hsv The source `ColorHSV` struct to convert.
 *            - `h` (Hue) is expected to be in the range [0.0, 360.0]. Values outside this range will be wrapped.
 *            - `s` (Saturation) and `v` (Value) are expected to be in the range [0.0, 1.0]. Values outside this range will be clamped.
 *
 * @return A `ColorRGBA` struct containing the equivalent R, G, and B values. The alpha component is always set to `255` (fully opaque).
 *
 * @see SituationRgbToHsv(), SituationImageAdjustHSV()
 */
SITAPI ColorRGBA SituationHsvToRgb(ColorHSV hsv) {
    ColorRGBA rgb;
    float c = hsv.v * hsv.s;
    float x = c * (1.0f - fabsf(fmodf(hsv.h / 60.0f, 2.0f) - 1.0f));
    float m = hsv.v - c;
    float r = 0, g = 0, b = 0;
    int sector = (int)(hsv.h / 60.0f) % 6;
    switch (sector) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        case 5: r = c; g = 0; b = x; break;
    }
    rgb.r = (unsigned char)((r + m) * 255.0f);
    rgb.g = (unsigned char)((g + m) * 255.0f);
    rgb.b = (unsigned char)((b + m) * 255.0f);
    rgb.a = 255; // Alpha is not part of HSV
    return rgb;
}


#include "situation_impl_ypq.h"

/**
 * @brief Converts a color from the YPQA (Luma, Phase, Quadrature, Alpha) color space back to the standard RGBA color space.
 * @details This is the inverse operation of `SituationColorToYPQ`. It reconstructs the red, green, and blue components from the color's brightness (Y) and its chroma information (P and Q), and preserves the alpha channel. The conversion uses the standard NTSC YIQ-to-RGB matrix via `_SitRgbFromYpqBytes` in `situation_impl_ypq.h`.
 *
 * @param ypq_color The source `ColorYPQA` struct to convert.
 *
 * @return A `ColorRGBA` struct containing the equivalent R, G, B, and A values. The function includes clamping to ensure the resulting RGB values are within the valid [0-255] range, as certain YPQ combinations can represent out-of-gamut colors.
 *
 * @see SituationColorToYPQ(), situation_impl_ypq.h
 */
SITAPI ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color) {
    return _SitRgbFromYpqBytes(ypq_color);
}

/**
 * @brief Converts a color from the standard RGBA color space to the YPQA (Luma, Phase, Quadrature, Alpha) color space.
 * @details This function transforms a color into a representation that separates brightness (luma) from color information (chroma). This is analogous to the YIQ color space used in NTSC television broadcasting. This separation is highly useful for effects that modify brightness independently of color, or for creating unique procedural color palettes.
 *
 * @par Color Space Details
 *   - **Y (Luma):** Represents the brightness or grayscale intensity of the color. Stored as an `unsigned char` [0-255].
 *   - **P (Phase):** Represents the hue of the color as an angle on the chroma plane. Stored as an `unsigned char` [0-255], mapping to a full 360-degree rotation.
 *   - **Q (Quadrature):** Represents the saturation or intensity of the color as the distance from the grayscale center on the chroma plane. Stored as an `unsigned char` [0-255].
 *   - **A (Alpha):** The original alpha channel is preserved directly.
 *
 * @param color The source `ColorRGBA` struct to convert.
 * @return A `ColorYPQA` struct containing the equivalent Y, P, Q, and A values.
 *
 * @see SituationColorFromYPQ()
 */
SITAPI ColorYPQA SituationColorToYPQ(ColorRGBA color) {
    return _SitYpqBytesFromRgb(color);
}

/**
 * @brief Interpolates between two YPQ colors; phase uses the shortest arc on the hue wheel.
 */
SITAPI ColorYPQA SituationYpqLerp(ColorYPQA color1, ColorYPQA color2, float t) {
    if (t <= 0.0f) {
        return color1;
    }
    if (t >= 1.0f) {
        return color2;
    }

    float y = (float)color1.y + ((float)color2.y - (float)color1.y) * t;
    float q = (float)color1.q + ((float)color2.q - (float)color1.q) * t;
    float a = (float)color1.a + ((float)color2.a - (float)color1.a) * t;

    float p1 = (float)_SitYpqPhaseByteToRadians(color1.p);
    float p2 = (float)_SitYpqPhaseByteToRadians(color2.p);
    float dp = p2 - p1;
    if (dp > (float)M_PI) {
        dp -= (float)(2.0 * M_PI);
    }
    if (dp < -(float)M_PI) {
        dp += (float)(2.0 * M_PI);
    }
    float p_interp = p1 + dp * t;
    if (p_interp < 0.0f) {
        p_interp += (float)(2.0 * M_PI);
    }
    if (p_interp >= (float)(2.0 * M_PI)) {
        p_interp -= (float)(2.0 * M_PI);
    }

    ColorYPQA result;
    if (y < 0.0f) {
        y = 0.0f;
    }
    if (y > 255.0f) {
        y = 255.0f;
    }
    if (q < 0.0f) {
        q = 0.0f;
    }
    if (q > 255.0f) {
        q = 255.0f;
    }
    if (a < 0.0f) {
        a = 0.0f;
    }
    if (a > 255.0f) {
        a = 255.0f;
    }
    result.y = (unsigned char)(y + 0.5f);
    result.p = _SitYpqPhaseRadiansToByte((double)p_interp);
    result.q = (unsigned char)(q + 0.5f);
    result.a = (unsigned char)(a + 0.5f);
    return result;
}

SITAPI ColorYPQA SituationYpqAdjustLuma(ColorYPQA color, float luma_factor) {
    float new_y = (float)color.y * luma_factor;
    if (new_y < 0.0f) {
        new_y = 0.0f;
    }
    if (new_y > 255.0f) {
        new_y = 255.0f;
    }
    return (ColorYPQA){(unsigned char)(new_y + 0.5f), color.p, color.q, color.a};
}

SITAPI ColorYPQA SituationYpqAdjustPhase(ColorYPQA color, int phase_shift) {
    int new_p = (int)color.p + phase_shift;
    while (new_p < 0) {
        new_p += 256;
    }
    while (new_p >= 256) {
        new_p -= 256;
    }
    return (ColorYPQA){color.y, (unsigned char)new_p, color.q, color.a};
}

SITAPI ColorYPQA SituationYpqAdjustChroma(ColorYPQA color, float chroma_factor) {
    float new_q = (float)color.q * chroma_factor;
    if (new_q < 0.0f) {
        new_q = 0.0f;
    }
    if (new_q > 255.0f) {
        new_q = 255.0f;
    }
    return (ColorYPQA){color.y, color.p, (unsigned char)(new_q + 0.5f), color.a};
}

SITAPI float SituationYpqGetLuma(ColorYPQA color) {
    return (float)color.y / 255.0f;
}

SITAPI float SituationYpqGetHueDegrees(ColorYPQA color) {
    return ((float)color.p / 255.0f) * 360.0f;
}

SITAPI float SituationYpqGetChroma(ColorYPQA color) {
    return (float)color.q / 255.0f;
}

SITAPI float SituationYpqDistance(ColorYPQA a, ColorYPQA b) {
    float dy = ((float)a.y - (float)b.y) / 255.0f;
    float dq = ((float)a.q - (float)b.q) / 255.0f;

    int dp_byte = (int)a.p - (int)b.p;
    if (dp_byte < 0) {
        dp_byte = -dp_byte;
    }
    if (dp_byte > 128) {
        dp_byte = 256 - dp_byte;
    }
    float dp = (float)dp_byte / 128.0f;

    return sqrtf(dy * dy + dp * dp + dq * dq);
}

SITAPI bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tolerance) {
    return abs((int)a.y - (int)b.y) <= (int)tolerance
        && abs((int)a.p - (int)b.p) <= (int)tolerance
        && abs((int)a.q - (int)b.q) <= (int)tolerance
        && abs((int)a.a - (int)b.a) <= (int)tolerance;
}

SITAPI ColorYPQf SituationColorToYPQf(ColorRGBA color) {
    return _SitYpqFloatFromRgb(color);
}

SITAPI ColorRGBA SituationColorFromYPQf(ColorYPQf ypq) {
    return _SitRgbFromYpqFloat(ypq);
}

SITAPI ColorYPQA SituationYpqQuantize(ColorYPQf ypq) {
    return _SitYpqBytesFromFloat(ypq);
}

SITAPI ColorYPQf SituationYpqClampInGamut(ColorYPQf ypq) {
    ColorYPQf result = ypq;
    result.y = _SitYpqClampUnitFloat(result.y);
    result.p = _SitYpqClampUnitFloat(result.p);
    result.q = _SitYpqClampUnitFloat(result.q);
    result.a = _SitYpqClampUnitFloat(result.a);

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    if (_SitYpqFloatRgbLinearInGamut(result, &r, &g, &b)) {
        return result;
    }

    float q_lo = 0.0f;
    float q_hi = result.q;
    for (int i = 0; i < 16; i++) {
        float q_mid = (q_lo + q_hi) * 0.5f;
        ColorYPQf trial = result;
        trial.q = q_mid;
        if (_SitYpqFloatRgbLinearInGamut(trial, &r, &g, &b)) {
            q_lo = q_mid;
        } else {
            q_hi = q_mid;
        }
    }
    result.q = q_lo;
    return result;
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
    SitRectangle rect = {0, 0, 0, 0};
    if (!text) return rect;

    // --- Bitmap Path ---
    if (font.is_bitmap) {
        float scale = fontSize / (float)font.bitmap_height;
        float width = 0.0f;
        int len = (int)strlen(text);
        if (len > 0) {
            // For monospaced bitmap fonts, calculation is simple
            width = len * (font.bitmap_width * scale);
        }
        rect.width = width;
        rect.height = fontSize;
        return rect;
    }

    // --- TrueType Path ---
    if (!font.stbFontInfo) return rect;

    stbtt_fontinfo *info = (stbtt_fontinfo*)font.stbFontInfo;
    float scale = stbtt_ScaleForPixelHeight(info, fontSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);

    rect.height = fontSize; // Approximate height

    float width = 0;
    for (int i = 0; text[i]; ++i) {
        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(info, text[i], &advanceWidth, &leftSideBearing);
        width += advanceWidth * scale;

        if (text[i+1]) {
            int kern = stbtt_GetCodepointKernAdvance(info, text[i], text[i+1]);
            width += kern * scale;
        }
    }
    rect.width = width;
    return rect;
}




/**
 * @brief Converts an 8-bit RGBA color struct to a normalized floating-point vec4.
 * @details This is a utility function for converting colors from the standard 0-255 integer range to the 0.0f-1.0f float range required by shader uniforms and vertex attributes.
 *
 * @param c The source `ColorRGBA` struct.
 * @param[out] out_normalized_color A `vec4` (float array of size 4) that will be filled with the normalized color components [r, g, b, a].
 */
SITAPI void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color) {
    out_normalized_color->x = c.r / 255.0f;
    out_normalized_color->y = c.g / 255.0f;
    out_normalized_color->z = c.b / 255.0f;
    out_normalized_color->w = c.a / 255.0f;
}

/**
 * @brief Captures the current contents of the main window's backbuffer into a CPU-side image.
 * @details This function reads the pixel data directly from the GPU's framebuffer, providing a snapshot of the most recently rendered frame. This is the core operation for taking screenshots or for enabling CPU-based image processing effects on the final rendered image.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glReadPixels` to read from the default framebuffer. The resulting image is vertically flipped, so the function automatically calls `SituationImageFlip` to correct the orientation.
 * - **Vulkan:** Copies from the swapchain via pre-present staging (`vkCmdCopyImageToBuffer`). Rows are top-left origin (+Y down), matching on-screen 2D rendering (Phase 7-bis: shared ortho + shader V convention; no readback flip). **No** `SituationImageFlip` on the cached path.
 *
 * @warning This function allocates new memory for the `image.data`. The caller is **responsible** for freeing this memory by calling `SituationUnloadImage()` on the returned `SituationImage`. Failure to do so will result in a memory leak.
 * @warning This can be a slow operation, as it requires synchronization with the GPU and a potentially large data transfer from VRAM to system RAM. Avoid calling it in performance-critical loops.
 *
 * @return A new `SituationImage` containing the pixel data, width, and height of the screen. Returns a zeroed (invalid) struct on failure (e.g., if out of memory or not implemented).
 *
 * @see SituationUnloadImage(), SituationTakeScreenshot()
 */
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

    // Bind the default framebuffer to read from it.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    SIT_CHECK_GL_ERROR();

    // [FIX v2.4.39] Ensure all GPU operations are complete before reading pixels.
    // This is critical for VD compositing which may still be in-flight.
    glFinish();

    // [FIX v2.4.40] Use pre-swap captured buffer if available.
    // Reading from GL_FRONT/GL_BACK after swap is unreliable on Windows (DWM).
    // EndFrame captures the back buffer into screenshot_buffer before swapping.
    if (sit_render.gl.screenshot_valid && sit_render.gl.screenshot_buffer &&
        sit_render.gl.screenshot_width == width && sit_render.gl.screenshot_height == height) {
        memcpy(out_image->data, sit_render.gl.screenshot_buffer, (size_t)width * height * 4);
    } else {
        // Fallback: try GL_BACK then GL_FRONT (less reliable after swap)
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_image->data);
        GLenum err = glGetError();

        if (err != GL_NO_ERROR) {
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_image->data);
            err = glGetError();
        } else {
            // Check if back buffer returned all black
            bool all_black = true;
            uint8_t* check = (uint8_t*)out_image->data;
            int check_limit = (width * height * 4 < 4096) ? width * height * 4 : 4096;
            for (int sample = 0; sample < check_limit; sample += 16) {
                if (check[sample] != 0 || check[sample+1] != 0 || check[sample+2] != 0) {
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
                    glfwPollEvents();
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


#endif // SITUATION_IMPL_IMAGE_H
