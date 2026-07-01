## Image Module

**Overview:** The Image module is a comprehensive, CPU-side toolkit for image manipulation. It allows you to load images, generate new images programmatically, perform transformations, and **rasterize text into pixel buffers**. Font loading, baking, measurement, and lifecycle are documented in the [Fonts Module](font.md). The `SituationImage` objects produced by this module are the primary source for creating GPU-side `SituationTexture`s.

### Structs and Enums

#### `Vector2`
A simple 2D vector, used for positions, sizes, and other 2D coordinates.
```c
typedef struct Vector2 {
    float x;
    float y;
} Vector2;
```
-   `x`: The x-component of the vector.
-   `y`: The y-component of the vector.

---
#### `Rectangle`
Represents a rectangle with a position (x, y) and dimensions (width, height).
```c
typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;
```
-   `x`, `y`: The screen coordinates of the top-left corner.
-   `width`, `height`: The dimensions of the rectangle.

---
#### `SituationImage`
A handle representing a CPU-side image. It contains the raw pixel data and metadata. All pixel data is stored in uncompressed 32-bit RGBA format unless otherwise specified. This struct is the primary source for creating GPU-side `SituationTexture`s.
```c
typedef struct SituationImage {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} SituationImage;
```
-   `data`: A pointer to the raw pixel data in system memory (RAM).
-   `width`: The width of the image in pixels.
-   `height`: The height of the image in pixels.
-   `mipmaps`: The number of mipmap levels generated for the image. `1` means no mipmaps.
-   `format`: The pixel format of the data (e.g., `SIT_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`).

---
#### Fonts (`SituationFont`)

Font handles (`SituationFont`, `SituationPackedFont`) and all load/bake/measure/unload APIs are documented in the [Fonts Module](font.md). CPU text functions below require a loaded font from that guide; GPU text uses the same handle via [Text Rendering](text_rendering.md).

### Functions

#### Image Loading and Unloading
---
#### `SituationLoadImage`
Loads an image from a file into CPU memory (RAM). Supported formats include PNG, BMP, TGA, and JPEG. All loaded images are converted to a 32-bit RGBA format.
```c
SituationError SituationLoadImage(const char *fileName, SituationImage* out_image);
```
**Usage Example:**
```c
// Load an image to be used for a player sprite.
SituationImage player_avatar;
if (SituationLoadImage("assets/sprites/player.png", &player_avatar) == SITUATION_SUCCESS) {
    // The image is now in CPU memory, ready to be manipulated or uploaded to the GPU.
    SituationTexture player_texture;
    SituationCreateTexture(player_avatar, true, &player_texture);
    // Once uploaded to a texture, the CPU-side copy can often be safely unloaded.
    SituationUnloadImage(player_avatar);
}
```

---
#### `SituationUnloadImage`
Unloads a CPU-side image and frees its associated memory.
```c
void SituationUnloadImage(SituationImage image);
```
**Usage Example:**
```c
SituationImage temp_image = SituationGenImageColor(128, 128, (ColorRGBA){255, 0, 255, 255});
// ... perform some operations on the image ...
SituationUnloadImage(temp_image); // Free the memory when done.
```
---
#### `SituationLoadImageFromMemory`
Loads an image from a data buffer in memory. The `fileType` parameter must include the leading dot (e.g., `.png`).
```c
SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image);
```
**Usage Example:**
```c
// Assume 'g_embedded_player_png' is a byte array with an embedded PNG file,
// and 'g_embedded_player_png_len' is its size.
SituationImage player_img;
SituationLoadImageFromMemory(".png", g_embedded_player_png, g_embedded_player_png_len, &player_img);
// ... use player_img ...
SituationUnloadImage(player_img);
```

---
#### `SituationLoadImageFromTexture`
Creates a CPU-side `SituationImage` by reading back pixel data from a GPU `SituationTexture`. This is a slow operation (GPU-to-CPU transfer) and should be used sparingly.

```c
SituationImage SituationLoadImageFromTexture(SituationTexture texture);
```

**Parameters:**
- `texture` - GPU texture to read from

**Returns:** CPU-side image with pixel data, or invalid image on failure

**Usage Example:**
```c
// Read back render target for processing
SituationTexture render_target = GetRenderTargetTexture();
SituationImage cpu_image = SituationLoadImageFromTexture(render_target);

if (SituationIsImageValid(&cpu_image)) {
    // Process on CPU
    ApplyCustomFilter(&cpu_image);

    // Save to disk
    SituationExportImage(cpu_image, "processed.png");

    // Upload back to GPU
    SituationTexture new_texture;
    SituationCreateTexture(cpu_image, false, &new_texture);

    SituationUnloadImage(cpu_image);
}

// Screenshot functionality
SituationTexture screen_texture = GetScreenTexture();
SituationImage screenshot = SituationLoadImageFromTexture(screen_texture);
SituationExportImage(screenshot, "screenshot.png");
SituationUnloadImage(screenshot);

// Debug texture contents
SituationImage debug_img = SituationLoadImageFromTexture(texture);
printf("Texture size: %dx%d, channels: %d\n",
    debug_img.width, debug_img.height, debug_img.channels);
// Inspect pixel data...
SituationUnloadImage(debug_img);
```

**Notes:**
- **SLOW** - causes GPU-to-CPU transfer and pipeline stall
- Use only when necessary (screenshots, debugging)
- Avoid in performance-critical code
- Returned image must be freed with `SituationUnloadImage()`
- Check validity with `SituationIsImageValid()`

---
#### `SituationLoadImageFromScreen`
Reads the **current window framebuffer** into a CPU `SituationImage`. **Not free** — triggers GPU readback when the cache is stale.

**Capture contract (v2.4.384+):** OpenGL no longer readbacks every frame unconditionally. Readback runs **on demand** when:
- you call **`SituationRequestScreenCapture()`** before **`SituationEndFrame()`**, or
- you call **`LoadImageFromScreen`** after **`EndFrame`** (implicit urgent/sync path).

Game loops with **no** screenshot request pay **zero** readback cost on those frames. See [architecture.md — On-demand screen capture](../architecture.md#on-demand-screen-capture-opengl).

```c
SituationError SituationLoadImageFromScreen(SituationImage* out_image);
```

**Recommended pattern (explicit):**
```c
SituationRequestScreenCapture();
SituationEndFrame();
SituationImage shot = {0};
if (SituationLoadImageFromScreen(&shot) == SITUATION_SUCCESS) {
    SituationExportImage(shot, "capture.png");
    SituationUnloadImage(shot);
}
```

**Implicit pattern (also supported):** `EndFrame()` then `LoadImageFromScreen()` without a prior request — render-thread OpenGL uses an urgent latch; single-threaded path sync-reads on cache miss.

---
#### `SituationRequestScreenCapture`
Arms capture for the **next** frame slot so the render thread (or ST present path) performs readback before swap. Pair with **`SituationEndFrame()`** then **`SituationLoadImageFromScreen()`**.

```c
void SituationRequestScreenCapture(void);
```

---
#### `SituationExportImage`
Exports the pixel data of a `SituationImage` to a file. The file format is determined by the extension. Currently, `.png` and `.bmp` are supported.
```c
SituationError SituationExportImage(SituationImage image, const char *fileName);
```
**Usage Example:**
```c
// Take a screenshot and save it as a PNG.
SituationImage screenshot = {0};
if (SituationLoadImageFromScreen(&screenshot) == SITUATION_SUCCESS) {
    SituationExportImage(screenshot, "screenshots/capture.png");
    SituationUnloadImage(screenshot);
}
```

---
#### Image Generation and Manipulation

---
#### Image Generation and Manipulation
---
#### `SituationImageFromImage`
Creates a new `SituationImage` by copying a sub-rectangle from a source image. This is useful for extracting sprites from a spritesheet.
```c
SituationImage SituationImageFromImage(SituationImage image, Rectangle rect);
```
**Usage Example:**
```c
// Extract a 16x16 sprite from a larger spritesheet.
SituationImage spritesheet;
if (SituationLoadImage("assets/sprites.png", &spritesheet) == SITUATION_SUCCESS) {
    Rectangle sprite_rect = { .x = 32, .y = 16, .width = 16, .height = 16 };
    SituationImage single_sprite = SituationImageFromImage(spritesheet, sprite_rect);
    // 'single_sprite' is now a new 16x16 image that can be used independently.
    // ... use single_sprite ...
    SituationUnloadImage(single_sprite);
    SituationUnloadImage(spritesheet);
}
```

---
#### `SituationImageCopy`
Creates a new image by making a deep copy of another image. This allocates new memory and copies all pixel data, allowing you to modify the copy without affecting the original.

```c
SituationError SituationImageCopy(SituationImage image, SituationImage* out_image);
```

**Parameters:**
- `image` - The source image to copy from
- `out_image` - Pointer to receive the newly created image copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load an image and create a backup copy
SituationImage original = SituationLoadImage("photo.png");
SituationImage backup;

if (SituationImageCopy(original, &backup) == SITUATION_SUCCESS) {
    // Modify the original without affecting the backup
    SituationImageFlipVertical(&original);
    SituationImageAdjustHSV(&original, 0.1f, 1.2f, 1.0f);

    // Save both versions
    SituationSaveImage(original, "photo_modified.png");
    SituationSaveImage(backup, "photo_original.png");

    SituationUnloadImage(backup);
}
SituationUnloadImage(original);
```

**Notes:**
- Allocates new memory for the copy - remember to call `SituationUnloadImage()` on both images
- Copies all pixel data, dimensions, and channel information
- Useful for creating variations of an image or preserving original state

---
#### `SituationCreateImage`
Allocates a new SituationImage container with UNINITIALIZED data. You must fill the image data manually after creation.

```c
SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);
```

**Parameters:**
- `width` - Image width in pixels
- `height` - Image height in pixels
- `channels` - Number of color channels (1=grayscale, 3=RGB, 4=RGBA)
- `out_image` - Pointer to receive the created image

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create empty image and fill manually
SituationImage image;
if (SituationCreateImage(256, 256, 4, &image) == SITUATION_SUCCESS) {
    // Fill with procedural pattern
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            int index = (y * 256 + x) * 4;
            image.data[index + 0] = x;        // R
            image.data[index + 1] = y;        // G
            image.data[index + 2] = 128;      // B
            image.data[index + 3] = 255;      // A
        }
    }

    // Upload to GPU
    SituationTexture texture;
    SituationCreateTexture(image, false, &texture);

    SituationUnloadImage(image);
}

// Create image for custom rendering
SituationImage canvas;
SituationCreateImage(512, 512, 4, &canvas);

// Draw to it
SituationImageDrawRectangle(&canvas, 10, 10, 100, 100, RED);
SituationImageDrawText(&canvas, font, "Hello", 20, 20, WHITE);

// Use it
SituationTexture tex;
SituationCreateTexture(canvas, false, &tex);
SituationUnloadImage(canvas);
```

**Notes:**
- Data is UNINITIALIZED - contains garbage
- Use `SituationGenImageColor()` for solid color images
- Must call `SituationUnloadImage()` when done
- Data layout: row-major, channels interleaved

---
#### `SituationGenImageColor`
Generates a new image filled with a single, solid color.
```c
SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a solid red 1x1 pixel image to use as a default texture.
SituationImage red_pixel;
if (SituationGenImageColor(1, 1, (ColorRGBA){255, 0, 0, 255}, &red_pixel) == SITUATION_SUCCESS) {
    SituationTexture default_texture;
    SituationCreateTexture(red_pixel, false, &default_texture);
    SituationUnloadImage(red_pixel);
}
```

---
#### `SituationGenImageGradient`
Generates an image with a linear, radial, or square gradient.
```c
SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a vertical gradient from red to black
SituationImage background;
if (SituationGenImageGradient(1280, 720, (ColorRGBA){255,0,0,255}, (ColorRGBA){255,0,0,255}, (ColorRGBA){0,0,0,255}, (ColorRGBA){0,0,0,255}, &background) == SITUATION_SUCCESS) {
    // ... use background ...
    SituationUnloadImage(background);
}
```

---
#### `SituationImageClearBackground`
Fills the entire destination image with a specified solid color, replacing all existing pixel data.

```c
void SituationImageClearBackground(SituationImage *dst, ColorRGBA color);
```

**Parameters:**
- `dst` - Pointer to the image to fill
- `color` - The color to fill with (RGBA format)

**Usage Example:**
```c
// Create an image and fill it with a solid blue background
SituationImage canvas;
SituationCreateImage(800, 600, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){30, 60, 120, 255});

// Now draw on top of the blue background
SituationImage logo = SituationLoadImage("logo.png");
SituationImageDraw(&canvas, logo,
    (Rectangle){0, 0, logo.width, logo.height},
    (Rectangle){100, 100, 200, 200},
    (ColorRGBA){255, 255, 255, 255});

SituationUnloadImage(logo);
SituationUnloadImage(canvas);
```

**Notes:**
- Modifies the image in-place
- Fills all pixels regardless of the image's channel count
- Useful for initializing canvases before drawing operations

---
#### `SituationImageDraw`
Draws a source image (or a sub-rectangle of it) onto a destination image, with scaling and tinting.
```c
void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Rectangle dstRect, ColorRGBA tint);
```

---
#### `SituationImageDrawRectangle` / `SituationImageDrawLine`
Draws a colored rectangle or line directly onto an image's pixel data.
```c
void SituationImageDrawRectangle(SituationImage *dst, Rectangle rect, ColorRGBA color);
void SituationImageDrawLine(SituationImage *dst, Vector2 start, Vector2 end, ColorRGBA color);
```
**Usage Example:**
```c
// Create a canvas and draw a red border around it.
SituationImage canvas = SituationGenImageColor(256, 256, (ColorRGBA){255,255,255,255});
Rectangle border = { .x = 0, .y = 0, .width = 256, .height = 256 };
SituationImageDrawRectangleLines(&canvas, border, 4, (ColorRGBA){255,0,0,255});
```

---
#### `SituationImageCrop` / `SituationImageResize`
Crops or resizes an image in-place.
```c
void SituationImageCrop(SituationImage *image, Rectangle crop);
void SituationImageResize(SituationImage *image, int newWidth, int newHeight);
```
**Usage Example:**
```c
SituationImage atlas;
if (SituationLoadImage("sprite_atlas.png", &atlas) == SITUATION_SUCCESS) {
    // Crop the atlas to get the first sprite (e.g., a 32x32 sprite at top-left)
    SituationImageCrop(&atlas, (Rectangle){0, 0, 32, 32});
    // Now 'atlas' contains only the cropped sprite data.
    SituationUnloadImage(atlas);
}
```

---
#### `SituationImageFlipVertical` / `SituationImageFlipHorizontal`
Flips an image vertically or horizontally in-place.
```c
void SituationImageFlipVertical(SituationImage *image);
void SituationImageFlipHorizontal(SituationImage *image);
```

---
#### `SituationImageRotate`
Rotates an image by multiples of 90 degrees clockwise. The image is modified in-place, with dimensions being swapped for 90° and 270° rotations.

```c
void SituationImageRotate(SituationImage *image, int rotations);
```

**Parameters:**
- `image` - Pointer to the image to rotate (modified in-place)
- `rotations` - Number of 90-degree clockwise rotations (0-3, values wrap around)

**Usage Example:**
```c
// Load an image and create rotated versions
SituationImage original = SituationLoadImage("photo.png");

// Rotate 90 degrees clockwise
SituationImage rot90;
SituationImageCopy(original, &rot90);
SituationImageRotate(&rot90, 1);
SituationSaveImage(rot90, "photo_90.png");

// Rotate 180 degrees
SituationImage rot180;
SituationImageCopy(original, &rot180);
SituationImageRotate(&rot180, 2);
SituationSaveImage(rot180, "photo_180.png");

// Rotate 270 degrees (or 90 counter-clockwise)
SituationImage rot270;
SituationImageCopy(original, &rot270);
SituationImageRotate(&rot270, 3);
SituationSaveImage(rot270, "photo_270.png");

SituationUnloadImage(original);
SituationUnloadImage(rot90);
SituationUnloadImage(rot180);
SituationUnloadImage(rot270);
```

**Notes:**
- Only supports 90-degree increments (no arbitrary angles)
- Modifies the image in-place
- For 90° and 270° rotations, width and height are swapped
- Very fast operation (no interpolation needed)
- Useful for correcting image orientation or creating sprite variations

---
#### `SituationImageColorTint` / `SituationImageColorInvert`
Applies a color tint or inverts the colors of an image in-place.
```c
void SituationImageColorTint(SituationImage *image, ColorRGBA color);
void SituationImageColorInvert(SituationImage *image);
```

---
#### `SituationImageColorGrayscale` / `SituationImageColorContrast` / `SituationImageColorBrightness`
Adjusts the grayscale, contrast, or brightness of an image in-place.
```c
void SituationImageColorGrayscale(SituationImage *image);
void SituationImageColorContrast(SituationImage *image, float contrast);
void SituationImageColorBrightness(SituationImage *image, int brightness);
```

---
#### `SituationImageAlphaMask` / `SituationImagePremultiplyAlpha`
Applies an alpha mask to an image or premultiplies the color channels by the alpha channel.
```c
void SituationImageAlphaMask(SituationImage *image, SituationImage alphaMask);
void SituationImagePremultiplyAlpha(SituationImage *image);
```

---
#### Text onto images

Load fonts with [Fonts Module](font.md) (`SituationLoadFont`, bitmap builders, bake steps). GPU drawing uses [Text Rendering](text_rendering.md). The functions below write glyph pixels directly into a `SituationImage` on the CPU — no `SituationBakeFontAtlas` required for CPU-only use.

---
#### `SituationImageDrawText`
Draws a simple, tinted text string onto an image.
```c
void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint);
```
**Usage Example:**
```c
SituationImage canvas;
SituationGenImageColor(800, 600, (ColorRGBA){20, 20, 20, 255}, &canvas);
SituationFont my_font;
if (SituationLoadFont("fonts/my_font.ttf", &my_font) == SITUATION_SUCCESS) {
    SituationImageDrawText(&canvas, my_font, "Hello, World!", (Vector2){50, 50}, 40, 1, (ColorRGBA){255, 255, 255, 255});

    // ... you can now upload 'canvas' to a GPU texture ...

    SituationUnloadFont(my_font);
}
SituationUnloadImage(canvas);
```

---
#### `SituationImageDrawTextEx`
Draws text with rotation, skew, and SDF outline onto an image. See [Fonts Module — SDF clarification](font.md#cpu-vs-gpu--when-to-use-which).

```c
void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text,
    Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor,
    ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

---
#### `SituationImageDrawTextFormatted`
`printf`-style formatted text onto an image.

```c
void SituationImageDrawTextFormatted(SituationImage *dst, SituationFont font, Vector2 position,
    float fontSize, float spacing, ColorRGBA tint, const char* fmt, ...);
```

---
#### `SituationImageDrawCodepoint`
Single Unicode codepoint with full styling (rotation, outline via SDF).

```c
void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint,
    Vector2 position, float fontSize, float rotationDegrees, float skewFactor,
    ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

---
#### `SituationImageStampText` / `SituationImageStampTextBoxed`
Stamp text with a solid background fill. Documented in [Fonts Module — CPU Stamp](font.md#cpu-stamp-background-fill).

---
#### Pixel-Level Access
---
#### `SituationGetPixelColor`
Gets the color of a single pixel from an image.
```c
ColorRGBA SituationGetPixelColor(SituationImage image, int x, int y);
```
**Usage Example:**
```c
SituationImage my_image;
if (SituationLoadImage("assets/my_image.png", &my_image) == SITUATION_SUCCESS) {
    ColorRGBA top_left_pixel = SituationGetPixelColor(my_image, 0, 0);
    printf("Top-left pixel color: R%d G%d B%d A%d\n",
           top_left_pixel.r, top_left_pixel.g, top_left_pixel.b, top_left_pixel.a);
    SituationUnloadImage(my_image);
}
```

---
#### `SituationSetPixelColor`
Sets the color of a single pixel in an image.
```c
void SituationSetPixelColor(SituationImage* image, int x, int y, ColorRGBA color);
```
**Usage Example:**
```c
SituationImage canvas = SituationGenImageColor(10, 10, (ColorRGBA){0, 0, 0, 255});
// Draw a red pixel in the center
SituationSetPixelColor(&canvas, 5, 5, (ColorRGBA){255, 0, 0, 255});
// ... use the canvas ...
SituationUnloadImage(canvas);
```

---
#### `SituationIsImageValid`
Checks if an image has been loaded successfully and contains valid data. This is useful for error checking after loading operations.

```c
bool SituationIsImageValid(SituationImage image);
```

**Parameters:**
- `image` - The image to validate

**Returns:** `true` if the image is valid and ready to use, `false` otherwise

**Usage Example:**
```c
// Safely load an image with error checking
SituationImage texture_image = SituationLoadImage("textures/wall.png");

if (SituationIsImageValid(texture_image)) {
    // Image loaded successfully, create GPU texture
    SituationTexture wall_texture = SituationCreateTextureFromImage(texture_image);
    SituationUnloadImage(texture_image);

    // Use the texture...
} else {
    // Image failed to load, use fallback
    printf("Failed to load wall.png, using default texture\n");
    SituationImage fallback;
    SituationGenImageColor(64, 64, (ColorRGBA){128, 128, 128, 255}, &fallback);
    SituationTexture wall_texture = SituationCreateTextureFromImage(fallback);
    SituationUnloadImage(fallback);
}
```

**Notes:**
- Returns `false` if the image data pointer is NULL or dimensions are invalid
- Always check validity after loading from files to handle missing or corrupted files
- Useful for implementing fallback textures in asset loading systems

---
#### `SituationImageDrawAlpha`
Draws a portion of a source image onto a destination image with alpha blending and optional color tinting. This respects the alpha channel of both images for proper transparency compositing.

```c
void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos, ColorRGBA tint);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `src` - The source image to draw from
- `srcRect` - Rectangle defining the portion of the source image to draw
- `dstPos` - Position in the destination image to draw to
- `tint` - Color tint to apply (use white {255,255,255,255} for no tint)

**Usage Example:**
```c
// Create a composite image with multiple layers
SituationImage canvas;
SituationCreateImage(800, 600, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){50, 50, 50, 255});

// Draw background layer
SituationImage background = SituationLoadImage("background.png");
SituationImageDrawAlpha(&canvas, background,
    (Rectangle){0, 0, background.width, background.height},
    (Vector2){0, 0},
    (ColorRGBA){255, 255, 255, 255});

// Draw character sprite with transparency
SituationImage character = SituationLoadImage("character.png");
SituationImageDrawAlpha(&canvas, character,
    (Rectangle){0, 0, 64, 64},
    (Vector2){300, 400},
    (ColorRGBA){255, 255, 255, 255});

// Draw UI element with red tint and 50% opacity
SituationImage icon = SituationLoadImage("icon.png");
SituationImageDrawAlpha(&canvas, icon,
    (Rectangle){0, 0, 32, 32},
    (Vector2){750, 20},
    (ColorRGBA){255, 100, 100, 128});

SituationSaveImage(canvas, "composite.png");
SituationUnloadImage(background);
SituationUnloadImage(character);
SituationUnloadImage(icon);
SituationUnloadImage(canvas);
```

**Notes:**
- Properly handles alpha blending using the alpha channels of both images
- Tint color's alpha channel controls overall opacity
- Source rectangle can be used to draw sprite atlas regions
- Useful for compositing layers, sprite rendering, and UI overlays

---
#### `SituationImageResize`
Resizes an image to new dimensions using high-quality bicubic interpolation. The image is modified in-place, with memory being reallocated as needed.

```c
void SituationImageResize(SituationImage *image, int newWidth, int newHeight);
```

**Parameters:**
- `image` - Pointer to the image to resize (modified in-place)
- `newWidth` - The new width in pixels
- `newHeight` - The new height in pixels

**Usage Example:**
```c
// Load a high-resolution image and create a thumbnail
SituationImage photo = SituationLoadImage("photo_4k.png");
printf("Original size: %dx%d\n", photo.width, photo.height);

// Resize to thumbnail dimensions
SituationImageResize(&photo, 256, 256);
printf("Thumbnail size: %dx%d\n", photo.width, photo.height);

// Save the thumbnail
SituationSaveImage(photo, "photo_thumb.png");
SituationUnloadImage(photo);
```

**Notes:**
- Uses bicubic interpolation for high-quality scaling
- Modifies the image in-place - original data is lost
- Memory is automatically reallocated to fit the new dimensions
- For creating multiple sizes, use `SituationImageCopy()` first to preserve the original

---
#### `SituationImageFlip`
Flips an image horizontally, vertically, or both. The image is modified in-place.

```c
void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode);
```

**Parameters:**
- `image` - Pointer to the image to flip (modified in-place)
- `mode` - The flip mode: `SITUATION_FLIP_HORIZONTAL`, `SITUATION_FLIP_VERTICAL`, or `SITUATION_FLIP_BOTH`

**Usage Example:**
```c
// Load an image and create mirrored versions
SituationImage original = SituationLoadImage("arrow.png");

// Create horizontal mirror
SituationImage h_mirror;
SituationImageCopy(original, &h_mirror);
SituationImageFlip(&h_mirror, SITUATION_FLIP_HORIZONTAL);
SituationSaveImage(h_mirror, "arrow_h_flip.png");

// Create vertical mirror
SituationImage v_mirror;
SituationImageCopy(original, &v_mirror);
SituationImageFlip(&v_mirror, SITUATION_FLIP_VERTICAL);
SituationSaveImage(v_mirror, "arrow_v_flip.png");

// Create 180-degree rotation (both flips)
SituationImage rotated;
SituationImageCopy(original, &rotated);
SituationImageFlip(&rotated, SITUATION_FLIP_BOTH);
SituationSaveImage(rotated, "arrow_180.png");

SituationUnloadImage(original);
SituationUnloadImage(h_mirror);
SituationUnloadImage(v_mirror);
SituationUnloadImage(rotated);
```

**Notes:**
- Modifies the image in-place
- Very fast operation (no interpolation needed)
- Useful for creating sprite variations or correcting image orientation

---
#### `SituationImageAdjustHSV`
Adjusts the Hue, Saturation, and Value (brightness) of an image with optional mixing. This allows for color correction, stylization, and visual effects.

```c
void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix);
```

**Parameters:**
- `image` - Pointer to the image to adjust (modified in-place)
- `hue_shift` - Hue rotation in normalized range (-1.0 to 1.0, where 1.0 = 360°)
- `sat_factor` - Saturation multiplier (0.0 = grayscale, 1.0 = original, >1.0 = more saturated)
- `val_factor` - Value/brightness multiplier (0.0 = black, 1.0 = original, >1.0 = brighter)
- `mix` - Blend factor between original and adjusted (0.0 = original, 1.0 = fully adjusted)

**Usage Example:**
```c
// Load an image and create color variations
SituationImage photo = SituationLoadImage("landscape.png");

// Create a warmer version (shift hue toward red/orange)
SituationImage warm;
SituationImageCopy(photo, &warm);
SituationImageAdjustHSV(&warm, 0.05f, 1.1f, 1.05f, 1.0f);
SituationSaveImage(warm, "landscape_warm.png");

// Create a cooler version (shift hue toward blue)
SituationImage cool;
SituationImageCopy(photo, &cool);
SituationImageAdjustHSV(&cool, -0.1f, 1.2f, 0.95f, 1.0f);
SituationSaveImage(cool, "landscape_cool.png");

// Create a desaturated version (50% grayscale)
SituationImage desaturated;
SituationImageCopy(photo, &desaturated);
SituationImageAdjustHSV(&desaturated, 0.0f, 0.0f, 1.0f, 0.5f);
SituationSaveImage(desaturated, "landscape_desat.png");

SituationUnloadImage(photo);
SituationUnloadImage(warm);
SituationUnloadImage(cool);
SituationUnloadImage(desaturated);
```

**Notes:**
- Modifies the image in-place
- Hue shift wraps around (e.g., red → orange → yellow → green → cyan → blue → magenta → red)
- Saturation factor of 0.0 produces grayscale
- Value factor affects overall brightness
- Mix parameter allows subtle adjustments by blending with original

---
#### `SituationImageAdjustYPQ`
Adjusts phase (hue), chroma, and luma of every pixel in-place via float YPQ, with optional mixing. Mirrors `SituationImageAdjustHSV` but uses NTSC-style luma/chroma separation — useful for TV/retro grading and effects that should preserve perceived brightness.

```c
void SituationImageAdjustYPQ(SituationImage *image, float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

**Parameters:**
- `image` - Pointer to the image to adjust (modified in-place)
- `phase_shift_deg` - Hue rotation in degrees (wraps on the phase wheel)
- `chroma_factor` - Chroma (Q) multiplier (0.0 = grayscale, 1.0 = original, >1.0 = more saturated)
- `luma_factor` - Luma (Y) multiplier (0.0 = black, 1.0 = original, >1.0 = brighter)
- `mix` - Blend factor between original and adjusted (0.0 = original, 1.0 = fully adjusted)

**Usage Example:**
```c
SituationImage photo = SituationLoadImage("frame.png");

// Boost chroma without a full HSV push
SituationImageAdjustYPQ(&photo, 0.0f, 1.5f, 1.0f, 1.0f);

// Warm shift: +15° phase, slight luma lift
SituationImageAdjustYPQ(&photo, 15.0f, 1.0f, 1.05f, 0.75f);

SituationSaveImage(photo, "frame_ypq_grade.png");
SituationUnloadImage(photo);
```

**Notes:**
- Context-free CPU path (no `SituationInit` required)
- Internally uses `SituationColorToYPQf` / `SituationColorFromYPQf` per pixel
- RGB textures and framebuffers remain the GPU display encoding; YPQ is an authoring boundary

---
#### `SituationUnloadFont`
Unloads a CPU-side font and frees its memory. Always call this when done with a font to prevent memory leaks.

```c
void SituationUnloadFont(SituationFont font);
```

**Parameters:**
- `font` - Font to unload

**Usage Example:**
```c
// Load and use font
SituationFont font;
if (SituationLoadFont("fonts/arial.ttf", &font) == SITUATION_SUCCESS) {
    // Use the font
    SituationCmdDrawText(cmd, font, "Hello", 100, 100, 24.0f, WHITE);

    // Cleanup when done
    SituationUnloadFont(font);
}

// Proper resource management
SituationFont ui_font;
SituationFont title_font;

void LoadFonts() {
    SituationLoadFont("fonts/roboto.ttf", &ui_font);
    SituationLoadFont("fonts/title.ttf", &title_font);
}

void UnloadFonts() {
    SituationUnloadFont(ui_font);
    SituationUnloadFont(title_font);
}

// Font switching
SituationFont current_font;
SituationLoadFont("fonts/default.ttf", &current_font);

// Switch to different font
SituationUnloadFont(current_font);
SituationLoadFont("fonts/fancy.ttf", &current_font);

// Cleanup at exit
SituationUnloadFont(current_font);

// Multiple fonts
#define MAX_FONTS 5
SituationFont fonts[MAX_FONTS];
const char* font_paths[] = {
    "fonts/regular.ttf",
    "fonts/bold.ttf",
    "fonts/italic.ttf",
    "fonts/mono.ttf",
    "fonts/title.ttf"
};

// Load all
for (int i = 0; i < MAX_FONTS; i++) {
    SituationLoadFont(font_paths[i], &fonts[i]);
}

// Cleanup all
for (int i = 0; i < MAX_FONTS; i++) {
    SituationUnloadFont(fonts[i]);
}
```

**Notes:**
- Frees font atlas texture and glyph data
- Safe to call multiple times with same font
- Always call before application exit
- Don't use font after unloading
- Unloading doesn't affect already-rendered text

---
#### `SituationImageDrawCodepoint`
Draws a single Unicode character onto an image with advanced styling options including rotation, skew, fill color, and outline.

```c
void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `font` - The font to use for rendering
- `codepoint` - Unicode codepoint of the character to draw (e.g., 'A' = 65, '★' = 9733)
- `position` - Position to draw the character (top-left corner)
- `fontSize` - Font size in pixels
- `rotationDegrees` - Rotation angle in degrees (clockwise)
- `skewFactor` - Horizontal skew factor (0.0 = no skew, positive = italic-like effect)
- `fillColor` - Color for the character fill
- `outlineColor` - Color for the character outline
- `outlineThickness` - Thickness of the outline in pixels (0.0 = no outline)

**Usage Example:**
```c
// Create a canvas and draw styled characters
SituationImage canvas;
SituationCreateImage(512, 512, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){255, 255, 255, 255});

SituationFont font = SituationLoadFont("fonts/arial.ttf", 64);

// Draw a red 'A' with black outline
SituationImageDrawCodepoint(&canvas, font, 'A',
    (Vector2){50, 50}, 64.0f, 0.0f, 0.0f,
    (ColorRGBA){255, 0, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 2.0f);

// Draw a rotated blue star with yellow outline
SituationImageDrawCodepoint(&canvas, font, 9733, // Unicode star ★
    (Vector2){200, 200}, 72.0f, 45.0f, 0.0f,
    (ColorRGBA){0, 100, 255, 255},
    (ColorRGBA){255, 255, 0, 255}, 3.0f);

// Draw an italicized green '@' symbol
SituationImageDrawCodepoint(&canvas, font, '@',
    (Vector2){350, 100}, 48.0f, 0.0f, 0.3f,
    (ColorRGBA){0, 200, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 1.5f);

SituationSaveImage(canvas, "styled_characters.png");
SituationUnloadFont(font);
SituationUnloadImage(canvas);
```

**Notes:**
- Useful for creating logos, icons, or decorative text elements
- Supports full Unicode range (emoji, symbols, international characters)
- Rotation is applied around the character's origin point
- Skew factor creates italic-like effects
- Outline is drawn outside the character fill

---
#### `SituationImageDrawTextEx`
Draws a text string onto an image with advanced styling options including rotation, skew, spacing, fill color, and outline.

```c
void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `font` - The font to use for rendering
- `text` - UTF-8 encoded text string to draw
- `position` - Position to draw the text (top-left corner)
- `fontSize` - Font size in pixels
- `spacing` - Additional spacing between characters in pixels
- `rotationDegrees` - Rotation angle in degrees (clockwise)
- `skewFactor` - Horizontal skew factor (0.0 = no skew, positive = italic-like effect)
- `fillColor` - Color for the text fill
- `outlineColor` - Color for the text outline
- `outlineThickness` - Thickness of the outline in pixels (0.0 = no outline)

**Usage Example:**
```c
// Create a thumbnail with styled text overlay
SituationImage thumbnail = SituationLoadImage("video_frame.png");
SituationFont bold_font = SituationLoadFont("fonts/bold.ttf", 48);

// Draw title with white text and black outline
SituationImageDrawTextEx(&thumbnail, bold_font, "EPISODE 1",
    (Vector2){50, 50}, 48.0f, 2.0f, 0.0f, 0.0f,
    (ColorRGBA){255, 255, 255, 255},
    (ColorRGBA){0, 0, 0, 255}, 3.0f);

// Draw subtitle with yellow italic text
SituationImageDrawTextEx(&thumbnail, bold_font, "The Beginning",
    (Vector2){50, 110}, 32.0f, 1.0f, -5.0f, 0.2f,
    (ColorRGBA){255, 255, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 2.0f);

// Draw watermark with semi-transparent text
SituationImageDrawTextEx(&thumbnail, bold_font, "© 2026 Studio",
    (Vector2){thumbnail.width - 200, thumbnail.height - 40},
    20.0f, 0.5f, 0.0f, 0.0f,
    (ColorRGBA){255, 255, 255, 128},
    (ColorRGBA){0, 0, 0, 0}, 0.0f);

SituationSaveImage(thumbnail, "thumbnail.png");
SituationUnloadFont(bold_font);
SituationUnloadImage(thumbnail);
```

**Notes:**
- Supports full UTF-8 text including emoji and international characters
- Spacing parameter adds extra pixels between each character
- Rotation is applied to the entire text string
- Skew factor creates italic-like effects
- Outline is drawn outside the text fill
- Useful for creating thumbnails, watermarks, memes, and image overlays

---

#### `SituationImageDraw`
Draws a source image onto a destination image.
```c
SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos);
```
**Usage Example:**
```c
SituationImage canvas;
if (SituationGenImageColor(256, 26, (ColorRGBA){255, 255, 255, 255}, &canvas) == SITUATION_SUCCESS) {
    SituationImage sprite;
    if (SituationLoadImage("assets/sprite.png", &sprite) == SITUATION_SUCCESS) {
        Rectangle sprite_rect = { .x = 0, .y = 0, .width = 16, .height = 16 };
        Vector2 position = { .x = 120, .y = 120 };
        SituationImageDraw(&canvas, sprite, sprite_rect, position);
        SituationUnloadImage(sprite);
    }
    // ... use canvas ...
    SituationUnloadImage(canvas);
}
```

---

#### `SituationGenImageGradient`
Generates an image with a linear gradient.
```c
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a vertical gradient from red to black
SituationImage background;
if (SituationGenImageGradient(1280, 720, (ColorRGBA){255,0,0,255}, (ColorRGBA){255,0,0,255}, (ColorRGBA){0,0,0,255}, (ColorRGBA){0,0,0,255}, &background) == SITUATION_SUCCESS) {
    // ... use background ...
    SituationUnloadImage(background);
}
```

---

#### `SituationMeasureText`
Measures the dimensions of a string of text if it were to be rendered with a specific font and size.
```c
SITAPI Rectangle SituationMeasureText(SituationFont font, const char *text, float fontSize);
```
**Usage Example:**
```c
const char* button_text = "Click Me!";
SituationFont my_font;
if (SituationLoadFont("fonts/my_font.ttf", &my_font) == SITUATION_SUCCESS) {
    Rectangle text_bounds = SituationMeasureText(my_font, button_text, 20);
    // Now you can create a button rectangle that perfectly fits the text.
    Rectangle button_rect = { .x = 100, .y = 100, .width = text_bounds.width + 20, .height = text_bounds.height + 10 };
    SituationUnloadFont(my_font);
}
```

---
#### `SituationBlitRawDataToImage`
Copies raw pixel data directly into an image at a specified position.
```c
void SituationBlitRawDataToImage(
    SituationImage* dst,
    const unsigned char* src_data,
    int src_width,
    int src_height,
    int dst_x,
    int dst_y
);
```
**Usage Example:**
```c
// Copy a 32x32 sprite into a larger texture atlas
unsigned char sprite_data[32 * 32 * 4]; // RGBA data
SituationBlitRawDataToImage(&atlas, sprite_data, 32, 32, 0, 0);
```

---
#### `SituationImageDrawTextFormatted`
Draws formatted text (printf-style) onto an image.
```c
void SituationImageDrawTextFormatted(
    SituationImage* dst,
    SituationFont font,
    Vector2 pos,
    ColorRGBA color,
    const char* format,
    ...
);
```
**Usage Example:**
```c
// Draw dynamic text with variables
int score = 1000;
SituationImageDrawTextFormatted(&image, font, (Vector2){10, 10}, WHITE, "Score: %d", score);
```


---

#### `SituationIsStbImageLoadExtension`
True for stb_image decode extensions (.jpg, .png, .bmp, .tga, .psd, .gif, .hdr, .pic, .ppm, .pgm, .pnm).
```c
bool SituationIsStbImageLoadExtension(const char* extension);
```
