## Fonts Module

**Overview:** The font module owns everything about **loading**, **baking**, **measuring**, and **lifecycle** for `SituationFont` handles. A font is a shared resource: the same handle feeds **GPU text** (`SituationCmdDrawText*` in [Text Rendering](text_rendering.md)) and **CPU text** (`SituationImageDrawText*` in [Image Module](image.md)).

Situation supports three atlas strategies:

| Strategy | Source | Atlas | GPU draw | CPU draw |
|----------|--------|-------|----------|----------|
| **Built-in default** | Embedded 8×8 CP437 | 128×128 grid at init | Yes (zeroed font) | Yes (grid blit) |
| **TTF baked** | `.ttf` / `.otf` | `stbtt_BakeFontBitmap` alpha atlas | Yes after `BakeFontAtlas` | Yes via stb (SDF on `DrawTextEx`) |
| **Grid / packed bitmap** | 1bpp rows, terminal grids, VCR/VGA packs | NEAREST RGBA grid | Yes after bake or builder | Yes (pixel blit) |

**Do not confuse CPU SDF with GPU atlas:** `SituationImageDrawTextEx` uses Signed Distance Fields for outlines and rotation on CPU. `SituationBakeFontAtlas` + `SituationCmdDrawTextEx` use an **alpha bitmap atlas** sampled with bilinear filtering — not SDF.

**Related docs:**
- [Text Rendering](text_rendering.md) — GPU command-buffer text (`SituationCmdDrawText*`, metrics overlay)
- [2D Grid](grid.md) — tile playfields via `SituationLoadBitmapFontFromTexture` + `SituationGridSetFont`
- [Image Module](image.md) — CPU pixel buffers; text draw functions that write into `SituationImage`
- [2D Rendering & Drawing](drawing_2d.md) — scissor, coordinates, layout panels
- [situation_sdk.md §3.9](../situation_sdk.md#39-text-rendering) — SDK workflow summary

---

### Mental Model

```
  TTF / OTF file          Embedded 1bpp / packed rows       Texture sheet on GPU
        │                        │                              │
        ▼                        ▼                              ▼
  SituationLoadFont*     SituationCreate* / LoadBitmap*   LoadBitmapFontFromTexture
        │                        │                              │
        ▼                        ▼                              │
  SituationBakeFontAtlas   BakeBitmapFontAtlas (1bpp)         │
        │                        │                              │
        └────────────┬───────────┴──────────────────────────────┘
                     ▼
              SituationFont handle
         (atlas_texture + metrics + grid metadata)
                     │
         ┌───────────┴───────────┐
         ▼                       ▼
 SituationCmdDrawText*     SituationImageDrawText*
 (GPU — text_rendering.md)  (CPU — image.md)
```

**Canonical debug pattern:** Every numbered example uses `examples/shared/sit_example.h`, which passes a **zeroed** `SituationFont` to `SituationCmdDrawTextEx` (automatic built-in 8×8 VGA font). See `SitExample_DrawHUD()`.

---

### Structs

#### `SituationFont`

```c
typedef struct SituationFont {
    void *fontData;                 // Raw TTF bytes (CPU path)
    void *stbFontInfo;              // stbtt_fontinfo context

    SituationTexture atlas_texture; // GPU atlas after bake / builder / bitmap upload
    void* glyph_info;               // stbtt_bakedchar[96] after BakeFontAtlas
    int atlas_width, atlas_height;
    float font_height_pixels;       // Cap height the atlas was baked at

    bool is_bitmap;
    const unsigned char* bitmap_data; // 1bpp or grayscale source (not always copied)
    int bitmap_width, bitmap_height, bitmap_count;

    /* Grid atlas layout (bitmap / terminal / packed fonts) */
    int first_char;
    int chars_per_row, chars_per_col;
    int display_cell_width, display_cell_height;
    float char_spacing, line_spacing;
} SituationFont;
```

| Field group | Meaning |
|-------------|---------|
| `fontData` / `stbFontInfo` | Present for TTF/OTF fonts loaded with `SituationLoadFont*`. Freed by `SituationUnloadFont`. |
| `atlas_texture` | GPU texture used by `SituationCmdDrawText*`. Owned by the font unless it is the **library default** (never destroy manually). |
| `glyph_info` | TTF baked metrics. Freed by `SituationUnloadFont`. |
| `is_bitmap` / `bitmap_*` | Raw bitmap source metadata. `bitmap_data` is often **not copied** — keep embedded arrays alive. |
| `first_char`, `chars_per_row`, … | Grid cell layout for bitmap/terminal fonts. Required for GPU grid rendering. |

A zeroed struct (`SituationFont font = {0}`) is valid: GPU text substitutes the built-in default font when `atlas_texture.generation == 0`.

#### `SituationPackedFont`

Configuration for bit-packed retro fonts (`SituationCreatePackedBitmapFont`, outlined variant, and convenience VCR/VGA builders).

```c
typedef struct SituationPackedFont {
    int char_width, char_height, display_height;
    int char_count, first_char, chars_per_row;
    int bits_per_row, data_bits, data_bit_offset;
    bool bit_order_msb_first;
    int top_padding, bottom_padding, left_padding, right_padding;
    int atlas_chars_per_row, atlas_chars_per_col;
    bool enable_outline;
    int outline_thickness;
    unsigned char outline_r, outline_g, outline_b, outline_a;
    unsigned char font_r, font_g, font_b, font_a;
} SituationPackedFont;
```

Packed builders decode each character row from `packed_data`, optionally expand outlines, upload a NEAREST-filtered RGBA atlas, and fill grid metadata automatically.

---

### Built-in Default Font

At `SituationInit()`, the library expands the embedded `sit_default_8x8_font` (IBM VGA 8×8, Code Page 437, 256 glyphs) into a **128×128** RGBA texture (16×16 grid). Filtering is **NEAREST** for crisp pixel-art text.

- No load or bake call required.
- Pass `SituationFont font = {0}` to GPU or CPU draw functions.
- `SituationUnloadFont` on the default handle is a no-op for the shared atlas.

---

### Font Types and Workflows

#### 1. TrueType / OpenType (TTF/OTF)

```c
SituationFont font = {0};
SituationLoadFont("assets/Inter-Regular.ttf", &font);
SituationBakeFontAtlas(&font, 24.0f);   // required before GPU draw

SituationCmdDrawTextEx(cmd, font, "Hello", (Vector2){10, 10}, 24.0f, 0.0f, WHITE);

SituationUnloadFont(font);   // frees CPU data, glyph_info, and owned atlas
```

- `SituationLoadFont` / `SituationLoadFontFromMemory` — parse with stb_truetype; **no GPU atlas yet**.
- `SituationBakeFontAtlas` — rasterizes ASCII **32–126** (96 glyphs) into a **512×512** alpha atlas. Returns `SITUATION_ERROR_FONT_ATLAS_FULL` if glyphs do not fit at the requested size.
- CPU drawing (`SituationImageDrawText*`) works after load without bake; SDF path available via `SituationImageDrawTextEx`.

Bake at or above your largest on-screen size to avoid blurry upscaling.

#### 2. Raw 1bpp bitmap grid

```c
extern const unsigned char my_font_8x8[256 * 8];  // 8 bytes per char (8×8 at 1bpp)

SituationFont font = {0};
SituationLoadBitmapFontFromMemory(my_font_8x8, 8, 8, 256, &font);
SituationBakeBitmapFontAtlas(&font);   // uploads NEAREST grid atlas for GPU

SituationCmdDrawTextEx(cmd, font, "RETRO", pos, 16.0f, 0.0f, CYAN);
SituationUnloadFont(font);
```

**Data layout:** `num_chars` glyphs in row-major order. Each glyph is `(char_width × char_height) / 8` bytes when stored at **1 bit per pixel** (MSB-first within each byte, same layout as `SituationImageDrawCodepoint` expects for grid fonts).

`bitmap_data` is **not copied** — keep the source buffer valid until `SituationUnloadFont`.

#### 3. Terminal / CP437 / ASCII convenience builders

| Function | Typical use |
|----------|-------------|
| `SituationCreateTerminalFontFromMemory` | Grayscale cell grid in RAM |
| `SituationCreateTerminalFontEx` | Same + `char_spacing` / `line_spacing` |
| `SituationCreateCP437Font` | 8×16 CP437 ROM-style data |
| `SituationCreateASCIIFont` | Generic `cw × ch` ASCII grid |

All upload a NEAREST atlas and set grid metadata for GPU text.

#### 4. Packed retro builders

| Builder | Source layout |
|---------|----------------|
| `SituationCreatePackedBitmapFont` | `SituationPackedFont` + bit-packed rows |
| `SituationCreateOutlinedPackedBitmapFont` | Packed rows + outline expansion |
| `SituationCreateVCRFont` / `WithOutline` | 12×14 packed `uint16_t` rows (128 chars) |
| `SituationCreateVGA8x8Font` / `WithOutline` | 8×8 packed bytes (256 chars) |

#### 5. Atlas already on GPU

```c
SituationTexture sheet = ...;  // NEAREST grid atlas you uploaded yourself
SituationFont font = {0};
SituationLoadBitmapFontFromTexture(sheet, 8, 8, 0, &font);
// font.atlas_texture references sheet — do not destroy sheet before unload
```

Fills grid layout fields; does not upload new pixels.

---

### Lifecycle — `SituationUnloadFont`

```c
void SituationUnloadFont(SituationFont font);
```

`SituationUnloadFont` frees:

- TTF `fontData` and `stbFontInfo`
- `glyph_info` array
- **Owned** `atlas_texture` (via `SituationDestroyTexture` internally)

It **does not** destroy:

- The library default font atlas (shared singleton)
- A texture passed in via `SituationLoadBitmapFontFromTexture` (caller still owns the sheet)

**Do not** call `SituationDestroyTexture(&font.atlas_texture)` separately before unload for fonts you created — double-free risk. One `SituationUnloadFont` call is the correct cleanup.

---

### Measurement and Layout

Center or right-align GPU text by measuring first (see [Text Rendering](text_rendering.md) for draw calls):

```c
SitRectangle bounds = SituationMeasureText(font, "Game Over", 32.0f);
float cx = (screen_w - bounds.width) * 0.5f;
SituationCmdDrawTextEx(cmd, font, "Game Over", (Vector2){cx, 200.0f}, 32.0f, 0.0f, RED);
```

#### `SituationMeasureText`

```c
SitRectangle SituationMeasureText(SituationFont font, const char *text, float fontSize);
```

Delegates to `SituationMeasureTextEx` with `spacing = 0.0f`.

#### `SituationMeasureTextEx`

```c
SitRectangle SituationMeasureTextEx(SituationFont font, const char *text, float fontSize, float spacing);
```

Includes extra per-character `spacing` in the width calculation.

#### `SituationGetTextLineCount`

```c
int SituationGetTextLineCount(SituationFont font, const char *text, float max_width);
```

Returns how many lines are required when wrapping at `max_width` (grid and TTF metrics).

**Multi-line text:** No function draws multiple lines in one call. Split on `\n` or use `SituationCmdDrawTextBoxed` / `SituationImageStampTextBoxed` for bounded regions.

**Clipping:** Use `SituationCmdSetScissor` before GPU text (see [drawing_2d.md](drawing_2d.md)).

---

### CPU Stamp (background fill)

Rasterize text **with a solid background** into a `SituationImage`, then upload with `SituationCreateTexture` for static labels.

#### `SituationImageStampText`

```c
SituationError SituationImageStampText(SituationImage* dst, SituationFont font, const char* text,
    Vector2 pos, float fontSize, ColorRGBA text_color, ColorRGBA bg_color);
```

#### `SituationImageStampTextBoxed`

```c
SituationError SituationImageStampTextBoxed(SituationImage* dst, SituationFont font, const char* text,
    SitRectangle bounds, float fontSize, ColorRGBA text_color, ColorRGBA bg_color,
    bool word_wrap, int* out_width, int* out_height);
```

`word_wrap` enables width-based wrapping inside `bounds`. Optional `out_width` / `out_height` receive the stamped pixel extent.

---

### CPU Draw onto Images (summary)

Full signatures live in [Image Module — Text onto images](image.md#text-onto-images). These write glyphs into an existing `SituationImage` buffer:

| Function | Description |
|----------|-------------|
| `SituationImageDrawText` | Simple tinted string (aliased bitmap) |
| `SituationImageDrawTextEx` | Rotation, skew, outline (SDF path) |
| `SituationImageDrawTextFormatted` | `printf`-style formatted text |
| `SituationImageDrawCodepoint` | Single Unicode codepoint with full styling |

CPU draw requires `SituationLoadFont*` only — **no** `SituationBakeFontAtlas` unless you also need GPU text from the same font.

---

### GPU Draw (summary)

Record inside an active render pass. See [Text Rendering](text_rendering.md) for pipeline details, `SituationCmdDrawTextBoxed`, and `SituationDrawMetricsOverlay`.

| Function | Description |
|----------|-------------|
| `SituationCmdDrawText` | Draw at baked / default size |
| `SituationCmdDrawTextEx` | Custom `fontSize` and letter `spacing` |
| `SituationCmdDrawTextBoxed` | Clip / wrap inside a `SitRectangle` |

**Character limits (GPU):**
- Default font: printable ASCII via grid layout.
- Baked TTF: ASCII 32–126; other codepoints skipped silently.
- Max string length: **2048** characters per draw (truncated internally).

---

### Quick Start Recipes

#### A — Zero-setup debug text (GPU)

```c
SituationFont font = {0};
SituationCmdDrawTextEx(cmd, font, "FPS: 60", (Vector2){10, 10}, 16.0f, 1.0f, WHITE);
```

#### B — TTF UI font (GPU)

```c
SituationFont ui = {0};
SituationLoadFont("assets/Roboto-Regular.ttf", &ui);
SituationBakeFontAtlas(&ui, 20.0f);
SituationCmdDrawText(cmd, ui, "Score: 42", (Vector2){100, 50}, GOLD);
SituationUnloadFont(ui);
```

#### C — Retro 8×8 embedded font (GPU)

```c
extern const unsigned char sit_default_8x8_font[256 * 8];
SituationFont retro = {0};
SituationLoadBitmapFontFromMemory(sit_default_8x8_font, 8, 8, 256, &retro);
SituationBakeBitmapFontAtlas(&retro);
SituationCmdDrawTextEx(cmd, retro, "RETRO", pos, 16.0f, 0.0f, WHITE);
SituationUnloadFont(retro);
```

#### D — CPU label baked into a texture

```c
SituationFont font = {0};
SituationLoadFont("assets/label.ttf", &font);
SituationImage canvas;
SituationGenImageColor(512, 64, TRANSPARENT, &canvas);
SituationImageDrawText(&canvas, font, "Loading...", (Vector2){8, 8}, 24.0f, 0.0f, WHITE);
SituationTexture label_tex;
SituationCreateTexture(canvas, false, &label_tex);
SituationUnloadImage(canvas);
SituationUnloadFont(font);
```

#### E — VCR packed font

```c
extern const uint16_t vcr_font_data[128 * 16];
SituationFont vcr = {0};
SituationCreateVCRFont(vcr_font_data, &vcr);
SituationCmdDrawTextEx(cmd, vcr, "PLAY", pos, 14.0f, 0.0f, WHITE);
SituationUnloadFont(vcr);
```

---

### API Reference — Loading and Baking

---
#### `SituationLoadFont`
Loads a TTF/OTF file into CPU memory and parses it with stb_truetype. Does **not** create a GPU atlas.

```c
SituationError SituationLoadFont(const char *fileName, SituationFont* out_font);
```

---
#### `SituationLoadFontFromMemory`
Same as `SituationLoadFont`, from an embedded buffer (copies data internally).

```c
SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font);
```

---
#### `SituationLoadBitmapFontFromMemory`
Loads a raw 1bpp (or grayscale) grid font. Call `SituationBakeBitmapFontAtlas` before GPU draw.

```c
SituationError SituationLoadBitmapFontFromMemory(
    const unsigned char* data, int char_width, int char_height,
    int num_chars, SituationFont* out_font);
```

**Note:** `data` is not copied — keep it alive for the font lifetime.

---
#### `SituationBakeFontAtlas`
Rasterizes ASCII 32–126 into a 512×512 alpha atlas and uploads to GPU.

```c
SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels);
```

---
#### `SituationBakeBitmapFontAtlas`
Uploads `bitmap_data` as a NEAREST-filtered grid atlas for GPU text (1bpp layout).

```c
SituationError SituationBakeBitmapFontAtlas(SituationFont* font);
```

---
#### `SituationLoadBitmapFontFromTexture`
Grid atlas already on GPU — fills layout metadata only. Primary path for **[2D Grid](grid.md)** tile sheets (`SituationGridSetFont`).

```c
SituationError SituationLoadBitmapFontFromTexture(
    SituationTexture sheet, int char_width, int char_height,
    int first_char, SituationFont* out_font);
```

---
#### `SituationUnloadFont`
Frees CPU font data, glyph metrics, and owned GPU atlas (not the built-in default).

```c
void SituationUnloadFont(SituationFont font);
```

---
#### Retro builders

```c
SituationError SituationCreateTerminalFontFromMemory(
    const unsigned char* data, int char_width, int char_height, int char_count,
    int chars_per_row, int first_char, SituationFont* out_font);
SituationError SituationCreateTerminalFontEx(
    const unsigned char* data, int char_width, int char_height, int char_count,
    int chars_per_row, int first_char, float char_spacing, float line_spacing,
    SituationFont* out_font);
SituationError SituationCreateCP437Font(const unsigned char* font_data_8x16, SituationFont* out_font);
SituationError SituationCreateASCIIFont(const unsigned char* data, int cw, int ch, SituationFont* out_font);
SituationError SituationCreatePackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);
SituationError SituationCreateOutlinedPackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);
SituationError SituationCreateVCRFont(const uint16_t* font_data, SituationFont* out_font);
SituationError SituationCreateVCRFontWithOutline(
    const uint16_t* data, int outline_thickness, SituationFont* out_font);
SituationError SituationCreateVGA8x8Font(const unsigned char* data, SituationFont* out_font);
SituationError SituationCreateVGA8x8FontWithOutline(
    const unsigned char* data, int outline_thickness, SituationFont* out_font);
```

---

### CPU vs GPU — When to Use Which

| Need | Use |
|------|-----|
| HUD every frame | `SituationCmdDrawTextEx` — [text_rendering.md](text_rendering.md) |
| Text with outline on CPU | `SituationImageDrawTextEx` (SDF) — [image.md](image.md) |
| Simple aliased CPU labels | `SituationImageDrawText` |
| Static label with background | `SituationImageStampText` / `StampTextBoxed` |
| Bounded GPU panel with wrap | `SituationCmdDrawTextBoxed` — [text_rendering.md](text_rendering.md) |
| FPS / latency overlay | `SituationDrawMetricsOverlay` — [text_rendering.md](text_rendering.md) |

**SDF clarification:**
- `SituationImageDrawText` → `stbtt_MakeCodepointBitmap` (aliased)
- `SituationImageDrawTextEx` → SDF via `SituationImageDrawCodepoint`
- `SituationBakeFontAtlas` + `SituationCmdDrawTextEx` → alpha bitmap atlas (not SDF)

---

### Requirements and Build Flags

| Requirement | Detail |
|-------------|--------|
| **stb_truetype** | TTF load, bake, and CPU text need STB. Do not define `SITUATION_NO_STB_TRUETYPE`. |
| **Init** | Default font atlas initializes with `SituationInit()`. |
| **Bitmap lifetime** | Embedded `bitmap_data` must outlive the font when not copied. |

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `SITUATION_ERROR_RESOURCE_INVALID` on GPU draw | TTF not baked | Call `SituationBakeFontAtlas` after `LoadFont` |
| `SITUATION_ERROR_FONT_ATLAS_FULL` | Glyphs too large at size | Reduce `fontSizePixels` or use a simpler font |
| Blocky scaled TTF | Bake size << draw size | Bake at or above largest display size |
| GPU retro font missing glyphs | Bitmap not baked | Call `SituationBakeBitmapFontAtlas` after `LoadBitmapFontFromMemory` |
| Garbled 8×8 font in tests | Wrong bitmap layout | Use 1bpp stride: `num_chars × (w×h/8)` bytes, not `w×h` grayscale |
| Default font works, custom doesn't | Missing bake or STB stripped | Check build flags; bake atlas |
| Crash after unload | Double `DestroyTexture` | Use only `SituationUnloadFont` for owned atlases |
| Wrong measured width | Ignored spacing | Use `SituationMeasureTextEx` with matching `spacing` |
| `NOT_IMPLEMENTED` | STB disabled | Remove `SITUATION_NO_STB_TRUETYPE` |

**RGL:** Bitmap and grid font paths in `doc/misc/rgl.h` delegate to these Situation APIs (`RGL_CreateTerminal*`, `RGL_DrawText*`, `RGL_MeasureText`, etc.). TTF load/draw in RGL may still use legacy paths — prefer Situation directly for new code.

---

### Performance Tips

- Bake once at startup; reloading and re-baking fonts is expensive.
- Default font + `fontSize=16` is the fastest GPU path (no file I/O, NEAREST sampling).
- For static labels, stamp or draw on CPU once, upload texture, draw with `SituationCmdDrawTexture`.
- Prefer one draw call per logical line over per-character GPU calls.
