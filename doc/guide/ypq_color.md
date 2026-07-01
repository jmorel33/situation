## YPQ Color Module _(v2.4.192+)_

**Overview:** YPQ is Situation's NTSC YIQ-inspired color space for **grading and manipulation** — not a replacement for RGB on the GPU. **Y** = luma (brightness), **P** = phase (hue angle on the chroma plane), **Q** = chroma amplitude (saturation). Unlike HSV, YPQ separates brightness from hue in a way that matches broadcast color science: you can push luma without the hue "spinning" unpredictably, and rotate phase without automatically changing brightness.

**RGB stays the highway.** Textures, framebuffers, and shaders remain RGB(A). YPQ is applied at boundaries — CPU image tools, per-pixel math, and one GPU draw command that grades every sampled texel in the fragment shader.

**Canonical example:** `examples/07_ypq_color_grading/` — split-screen color lab: original RGB on the left, live GPU grade on the right, cinematic presets, and mapping diagnostics.

**Related:** [2D Drawing — YPQ textured draw](drawing_2d.md#drawing-textured-sprites-situationcmddrawtexture) · [Miscellaneous — byte YPQ helpers](miscellaneous.md) · [Image Module](image.md) · [`doc/plan/YPQ_COLOR_PLAN.md`](../plan/YPQ_COLOR_PLAN.md)

---

### Two Representations — Know Which One You Have

Situation exposes YPQ at two precision levels. They are **not interchangeable** — check the function signature before passing a struct.

| Type | Storage | When to use |
|------|---------|-------------|
| **`ColorYPQf`** | Float `[0, 1]` per channel; `p` = phase as **fraction of full wheel** (0→1 = 0°→360°) | Float editing, `SituationColorToYPQf`, `SituationImageAdjustYPQ`, GPU grade shader |
| **`ColorYPQA`** | 8-bit bytes per channel (0–255) | Legacy pixel ops, `SituationColorToYPQ`, lerp/adjust/distance helpers |

```c
typedef struct ColorYPQf { float y, p, q, a; } ColorYPQf;   // float edit space

typedef struct ColorYPQA {
    unsigned char y, p, q, a;   // packed byte space
} ColorYPQA;
```

**Rule of thumb:** Use **`ColorYPQf`** for new grading code and image-wide adjust. Use **`ColorYPQA`** when calling the byte-level helpers documented in [Miscellaneous](miscellaneous.md) (`SituationYpqLerp`, `SituationYpqAdjustLuma`, etc.). Convert between them with `SituationYpqQuantize(ypq_f)` → `ColorYPQA`.

---

### Architecture — RGB ↔ YIQ ↔ YPQ

All conversion math lives in `sit/situation_impl_color.h` (single source of truth). CPU and GPU paths share the same NTSC matrix constants.

```
  ColorRGBA (8-bit sRGB)
       │  linearize + RGB→YIQ matrix
       ▼
  Y, I, Q  (linear)
       │  amplitude = √(I² + Q²),  angle = atan2(Q, I)
       ▼
  ColorYPQf  { y, p, q, a }     p = angle / 360°
       │
       ├─ CPU: SituationImageAdjustYPQ (per pixel)
       ├─ GPU: ypq_grade.frag shader (per texel)
       └─ Byte: ColorYPQA via SituationYpqQuantize
       │
       ▼
  YIQ → RGB (clamped to [0,1]) → 8-bit RGBA
```

**Grading pipeline** (same on CPU and GPU):

1. Sample RGB → convert to YIQ
2. Derive normalized amplitude `q` and phase `p`
3. Apply adjustments:
   - `p ← fract(p + phase_shift_deg / 360)`
   - `q ← clamp(q × chroma_factor, 0, 1)`
   - `y ← clamp(y × luma_factor, 0, 1)`
4. Reconstruct YIQ from adjusted YPQ → RGB
5. `mix(original, adjusted, mix)` — blend factor 0 = untouched, 1 = fully graded

Alpha is preserved through image adjust; the GPU shader multiplies graded RGB by the source alpha and optional tint color.

---

### YPQ vs HSV — Why Phase ≠ Hue

Example 07's color lab is deliberately designed to expose this difference:

| Operation | HSV | YPQ |
|-----------|-----|-----|
| Rotate hue / phase | Shifts hue on the **HSV cone** — also changes which RGB primaries dominate | Rotates the **I/Q chroma vector** — luma (Y) is independent |
| Boost saturation | Scales distance from gray axis in HSV | Scales chroma amplitude Q in the I/Q plane |
| Brighten | Increases V (value) — can shift perceived hue at low S | Scales Y only — phase and Q preserved before clamp |

The P–Q wheel in example 07 visualizes phase as a rotation in the chroma plane, not an HSV hue twist. When you sweep phase on the spectrum wheel quadrant, you see the wheel "spin" while luma bands stay stable — behavior HSV hue rotation does not replicate.

For image-wide effects, compare `SituationImageAdjustYPQ` with `SituationImageAdjustHSV` on the same source to feel the difference.

---

### The Four Grading Parameters

These four knobs appear in both `SituationImageAdjustYPQ` and `SituationCmdDrawTextureYpqGrade`:

| Parameter | Typical range | Effect |
|-----------|---------------|--------|
| **`phase_shift_deg`** | 0 – 360 | Rotates chroma in the YIQ plane. +30° ≈ warm shift; +180° ≈ complementary. |
| **`chroma_factor`** | 0 – 2.5 | 0 = grayscale, 1 = unchanged, >1 = oversaturated (may clip). |
| **`luma_factor`** | 0.25 – 1.75 | Brightness multiplier on Y. 1 = unchanged. |
| **`mix`** | 0 – 1 | Blend original vs graded. 0.5 = 50% effect. |

**Cinematic presets** from example 07:

```c
typedef struct { float phase, chroma, luma, mix; const char* name; } YpqPreset;

static const YpqPreset k_presets[] = {
    {  28.0f, 1.35f, 1.05f, 1.0f, "Teal / Orange" },
    {   0.0f, 0.35f, 1.45f, 1.0f, "Bleach Bypass" },
    { 180.0f, 0.15f, 0.55f, 1.0f, "Noir"          },
    { 120.0f, 2.00f, 1.15f, 1.0f, "Hyperpop"      },
};
```

Apply with `SituationImageAdjustYPQ(&img, p.phase, p.chroma, p.luma, p.mix)` or pass the same values to `SituationCmdDrawTextureYpqGrade`.

---

### CPU Path — Images and Single Pixels

#### Image-wide grading — `SituationImageAdjustYPQ`

Modifies every pixel **in-place** on a CPU `SituationImage`. Alpha channel is preserved.

```c
void SituationImageAdjustYPQ(SituationImage* image,
    float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

**Workflow — bake once, upload as texture:**

```c
SituationImage img = {0};
SituationLoadImage("photo.png", &img);   // or SituationCreateImage + fill

/* Work on a copy if you need the original */
SituationImage graded = {0};
SituationImageCopy(img, &graded);
SituationImageAdjustYPQ(&graded, 28.0f, 1.35f, 1.05f, 1.0f);   // Teal/Orange

SituationTexture tex = {0};
SituationCreateTexture(graded, true, &tex);
SituationUnloadImage(graded);
SituationUnloadImage(img);
```

**Notes:**
- Returns `void` — validates internally; no-op on invalid image.
- `mix` is clamped to `[0, 1]`.
- Synchronous, O(pixels) — fine for screenshots, thumbnails, asset pipelines. Avoid per-frame on large images.

#### Float pixel conversion

```c
ColorYPQf SituationColorToYPQf(ColorRGBA color);
ColorRGBA SituationColorFromYPQf(ColorYPQf ypq);
```

Manual per-pixel grade without the image helper:

```c
ColorYPQf ypq = SituationColorToYPQf((ColorRGBA){200, 80, 40, 255});

ypq.p = fmodf(ypq.p + 30.0f / 360.0f, 1.0f);   /* +30° phase */
ypq.q = fminf(ypq.q * 1.2f, 1.0f);              /* +20% chroma */
ypq.y = fminf(ypq.y * 1.1f, 1.0f);              /* +10% luma */

ColorRGBA out = SituationColorFromYPQf(ypq);
```

#### Gamut safety — `SituationYpqClampInGamut`

Some YPQ combinations reconstruct to out-of-gamut RGB (values > 1.0 before clamp). `SituationYpqClampInGamut` binary-searches chroma down until linear RGB fits `[0, 1]³`:

```c
ColorYPQf safe = SituationYpqClampInGamut(ypq);
ColorRGBA rgb  = SituationColorFromYPQf(safe);
```

Use after aggressive chroma boosts when you need predictable results without hard clipping.

#### Export to byte space

```c
ColorYPQA packed = SituationYpqQuantize(ypq_f);   /* float → 8-bit YPQA */
```

---

### GPU Path — Live Grading — `SituationCmdDrawTextureYpqGrade`

Grades every texel in the fragment shader — **no CPU copy**, parameters can change every frame. Shader logic mirrors `SituationImageAdjustYPQ` (`sit/gpu/ypq_grade.frag`).

```c
SituationError SituationCmdDrawTextureYpqGrade(
    SituationCommandBuffer cmd,
    SituationTexture texture,
    SitRectangle source, SitRectangle dest,
    Vector2 origin, float rotation,
    float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

**Split-screen preview** (from example 07):

```c
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationRenderPassInfo pass = SituationRenderPassInfoDefault(-1, (ColorRGBA){6, 8, 14, 255});
SituationCmdBeginRenderPass(cmd, &pass);

SitRectangle src = {0, 0, (float)LAB_W, (float)LAB_H};
SitRectangle dst_l = {ox, oy, half_w, img_h};
SitRectangle dst_r = {ox + half_w + gap, oy, half_w, img_h};
Vector2 origin = {{0, 0}};

/* Left: original */
SituationCmdDrawTexture(cmd, g_tex, src, dst_l, origin, 0.0f,
                        (ColorRGBA){255, 255, 255, 255});

/* Right: live YPQ grade */
SituationCmdDrawTextureYpqGrade(cmd, g_tex, src, dst_r, origin, 0.0f,
                                g_phase, g_chroma, g_luma, g_mix);

SituationCmdEndRenderPass(cmd);
```

**When to prefer GPU vs CPU:**

| Scenario | Path |
|----------|------|
| Real-time preview, sliders, animation | GPU `DrawTextureYpqGrade` |
| Offline asset bake, save to PNG | CPU `SituationImageAdjustYPQ` |
| Full-screen post on render target | Draw the offscreen texture through YPQ grade into window pass |
| Thumbnail / one-shot filter | CPU on small image |

**Requirements:** Active render pass, valid texture handle, `SituationInit()` completed (YPQ grade pipeline initializes with the internal quad renderer). Same transform args as `SituationCmdDrawTexture` — source rect, dest rect, pivot, rotation, plus the four grade parameters.

**Tint:** The underlying draw still applies the `color` push constant (normalized RGBA). Pass white `{255,255,255,255}` for grade-only; use alpha in tint for fade effects on the graded result.

---

### Byte-Level Pixel Helpers (`ColorYPQA`)

These operate on **8-bit `ColorYPQA`** — parallel to the HSV byte helpers in [Miscellaneous](miscellaneous.md). All are context-free (no `SituationInit` required).

| Function | Signature | Notes |
|----------|-----------|-------|
| `SituationColorToYPQ` | `ColorYPQA SituationColorToYPQ(ColorRGBA)` | RGBA → byte YPQ |
| `SituationColorFromYPQ` | `ColorRGBA SituationColorFromYPQ(ColorYPQA)` | Byte YPQ → RGBA |
| `SituationYpqLerp` | `ColorYPQA SituationYpqLerp(ColorYPQA a, ColorYPQA b, float t)` | Phase shortest-arc interpolation |
| `SituationYpqAdjustLuma` | `ColorYPQA SituationYpqAdjustLuma(ColorYPQA, float factor)` | Scale Y byte |
| `SituationYpqAdjustPhase` | `ColorYPQA SituationYpqAdjustPhase(ColorYPQA, int shift)` | Rotate P by **byte steps** mod 256 |
| `SituationYpqAdjustChroma` | `ColorYPQA SituationYpqAdjustChroma(ColorYPQA, float factor)` | Scale Q byte |
| `SituationYpqDistance` | `float SituationYpqDistance(ColorYPQA a, ColorYPQA b)` | Weighted YPQ distance |
| `SituationYpqEquals` | `bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tol)` | Per-channel tolerance |

**Example — palette interpolation:**

```c
ColorYPQA a = SituationColorToYPQ((ColorRGBA){220, 90, 40, 255});
ColorYPQA b = SituationColorToYPQ((ColorRGBA){40, 120, 200, 255});
ColorYPQA mid = SituationYpqLerp(a, b, 0.5f);
ColorRGBA rgb = SituationColorFromYPQ(mid);
```

For float editing without byte quantization loss, convert via `SituationColorToYPQf` / `SituationColorFromYPQf` instead.

---

### 10-Bit and HDR Output Paths

**Full guide:** **[HD Color Output Module](hd_color_output.md)** — policy (`SIT_OUTPUT_COLOR_*`), caps, DXGI monitor metadata, Vulkan vs OpenGL, limitations, testing.

Advanced helpers for high bit-depth framebuffers and HDR10 swapchain output. Most 2D apps can ignore these until they opt into HD color at init.

| Function | Purpose |
|----------|---------|
| `SituationRgb10FromRgba` | Upscale 8-bit → 10-bit RGBA |
| `SituationRgbaFromRgb10` | Downscale 10-bit → 8-bit |
| `SituationRgbToYpqFrom10` | 10-bit RGBA → float YPQ |
| `SituationYpqToRgba10` | Float YPQ → 10-bit RGBA (linear YIQ, clamped) |
| `SituationYpqToRgb10Packed` | Float YPQ → A2R10G10B10 packed (SDR) |
| `SituationYpqToRgb10PackedHdr` | Float YPQ → PQ-encoded A2R10G10B10 (HDR10) |
| `SituationRgbaFromRgb10Packed` | Unpack A2R10G10B10 texel → RGBA8 |
| `SituationLinearToPq` / `SituationPqToLinear` | ST.2084 PQ ↔ linear light |
| `SituationPqGrayToRgb10Packed` | Uniform PQ gray → packed pixel |
| `SituationColorRgbaToHdrPqClear` | sRGB clear color → PQ-encoded RGBA floats×255 |

Use when writing directly to 10-bit swapchains or debugging HDR clear colors — not for standard 8-bit texture workflows.

---

### Mapping Quality Diagnostics

The 8-bit YPQ cube has **256³ = 16,777,216** entries but only ~**5.6M** distinct RGB outputs (~33% of the 24-bit RGB cube). Many YPQ triples collide to the same RGB; conversely ~10.6M RGB triples are unreachable ("holes"). This is inherent to the NTSC packing — not a bug — but it matters for palette authoring and round-trip fidelity.

#### `SituationYpqAnalyzeRgbMapping`

Full O(n³) scan — expect **a few seconds** on a modern CPU. Guard in CI with `SIT_SKIP_YPQ_RGB_STATS`.

```c
typedef struct SituationYpqRgbMappingStats {
    int64_t ypq_mappings;        /* always 256³ */
    int64_t unique_rgb;            /* distinct RGB reachable from YPQ */
    int64_t duplicate_mappings;    /* ypq_mappings - unique_rgb */
    int64_t rgb_holes;             /* 2²⁴ - unique_rgb */
    int     worst_axis_dup;        /* max duplicates in any fixed-Q slice */
    int     worst_axis_at;         /* Q value of that slice */
} SituationYpqRgbMappingStats;

SituationYpqRgbMappingStats stats = {0};
if (SituationYpqAnalyzeRgbMapping(&stats) == SITUATION_SUCCESS) {
    printf("unique RGB: %lld  holes: %lld  worst Q@%d=%d dups\n",
           (long long)stats.unique_rgb,
           (long long)stats.rgb_holes,
           stats.worst_axis_at, stats.worst_axis_dup);
}
```

Press **X** in example 07 to trigger this from the interactive lab.

#### `SituationYpqSliceDuplicateCount`

Count duplicates in one 65,536-entry slice with axis Y, P, or Q held fixed:

```c
int dup = 0;
SituationYpqSliceDuplicateCount('Q', 0, &dup);
/* Q=0 → all entries map to gray → ~65,000 duplicates */
```

Useful for understanding why low-chroma slices collapse heavily (Q=0 is pure grayscale).

---

### Example 07 — Interactive Color Lab

`examples/07_ypq_color_grading/main.c` demonstrates the full workflow:

1. **Procedural 512×512 atlas** — spectrum wheel, hue bars, Macbeth chips, sunset gradient (no external assets).
2. **CPU verify once** — copies atlas, runs `SituationImageAdjustYPQ`, discards (proves CPU/GPU parity path exists).
3. **Upload once** — `SituationCreateTexture(g_source, false, &g_tex)`.
4. **Every frame** — left panel `DrawTexture`, right panel `DrawTextureYpqGrade` with live parameters.
5. **Controls:**

| Key | Action |
|-----|--------|
| Q / A | Phase ± (degrees/sec) |
| W / S | Chroma × |
| E / D | Luma × |
| R / F | Mix |
| 1 – 4 | Cinematic presets |
| TAB | Auto phase sweep |
| SPACE | Reset parameters |
| X | Run `SituationYpqAnalyzeRgbMapping` |

Build:

```powershell
& "c:\Users\User\Desktop\hobby\_kiro\situation\build\build_examples.bat" static-opengl 07_ypq_color_grading
```

---

### Integration Patterns

#### Post-process a render target

```c
/* After rendering scene to offscreen texture: */
SituationTexture scene = /* from VD or render target */;

SituationCmdBeginRenderPass(cmd, &window_pass);
SituationCmdDrawTextureYpqGrade(cmd, scene, full_src, full_dest, origin, 0.0f,
    15.0f, 1.1f, 0.95f, 0.8f);   /* subtle warm grade at 80% mix */
SituationCmdEndRenderPass(cmd);
```

#### Compare HSV vs YPQ on same image

```c
SituationImage hsv_copy = {0}, ypq_copy = {0};
SituationImageCopy(source, &hsv_copy);
SituationImageCopy(source, &ypq_copy);

SituationImageAdjustHSV(&hsv_copy, 30.0f, 1.2f, 1.0f, 1.0f);
SituationImageAdjustYPQ(&ypq_copy, 30.0f, 1.2f, 1.0f, 1.0f);
/* Side-by-side upload — visually compare grading character */
```

#### Animated grade

Drive parameters from time — no image reprocessing:

```c
float phase = fmodf((float)SituationTimerGetTime() * 48.0f, 360.0f);
SituationCmdDrawTextureYpqGrade(cmd, tex, src, dst, origin, 0.0f,
                                phase, 1.0f, 1.0f, 1.0f);
```

---

### API Reference

---
#### Float pixel (`ColorYPQf`)

```c
ColorYPQf SituationColorToYPQf(ColorRGBA color);
ColorRGBA SituationColorFromYPQf(ColorYPQf ypq);
ColorYPQA SituationYpqQuantize(ColorYPQf ypq);
ColorYPQf SituationYpqClampInGamut(ColorYPQf ypq);
```

---
#### Image grading (CPU)

```c
void SituationImageAdjustYPQ(SituationImage* image,
    float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

---
#### GPU live grade

```c
SituationError SituationCmdDrawTextureYpqGrade(
    SituationCommandBuffer cmd, SituationTexture texture,
    SitRectangle source, SitRectangle dest,
    Vector2 origin, float rotation,
    float phase_shift_deg, float chroma_factor, float luma_factor, float mix);
```

---
#### Byte pixel (`ColorYPQA`) — see also [Miscellaneous](miscellaneous.md)

```c
ColorYPQA SituationColorToYPQ(ColorRGBA color);
ColorRGBA SituationColorFromYPQ(ColorYPQA ypq);
ColorYPQA SituationYpqLerp(ColorYPQA a, ColorYPQA b, float t);
ColorYPQA SituationYpqAdjustLuma(ColorYPQA c, float factor);
ColorYPQA SituationYpqAdjustPhase(ColorYPQA c, int phase_shift);
ColorYPQA SituationYpqAdjustChroma(ColorYPQA c, float factor);
float SituationYpqDistance(ColorYPQA a, ColorYPQA b);
bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tolerance);
```

---
#### Diagnostics

```c
SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out);
SituationError SituationYpqSliceDuplicateCount(char axis, int value, int* out_dup);
```

---
#### 10-bit / HDR

```c
ColorRGBA10 SituationRgb10FromRgba(ColorRGBA color);
ColorRGBA   SituationRgbaFromRgb10(ColorRGBA10 color);
ColorRGBA   SituationRgbaFromRgb10Packed(uint32_t packed);
ColorYPQf   SituationRgbToYpqFrom10(ColorRGBA10 color);
ColorRGBA10 SituationYpqToRgba10(ColorYPQf ypq);
uint32_t    SituationYpqToRgb10Packed(ColorYPQf ypq);
uint32_t    SituationYpqToRgb10PackedHdr(ColorYPQf ypq);
float       SituationLinearToPq(float linear);
float       SituationPqToLinear(float pq);
uint32_t    SituationPqGrayToRgb10Packed(float pq_level);
ColorRGBA   SituationColorRgbaToHdrPqClear(ColorRGBA srgb);
```

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| GPU grade shows same as original | `mix = 0` or all factors at identity | Set `mix > 0`; change phase/chroma/luma |
| `DrawTextureYpqGrade` error | Outside render pass or invalid texture | Begin pass first; check texture `generation` |
| CPU and GPU look slightly different | 8-bit quantize vs float path | Expected at extreme settings; compare with `mix=1` on smooth gradients |
| Colors clip / posterize | Chroma > 2, luma > 1.75 | Reduce factors; use `SituationYpqClampInGamut` on CPU path |
| `ImageAdjustYPQ` no effect | Invalid image or `mix=0` | Validate with `SituationIsImageValid`; check parameters |
| Analyze hangs CI | Full 256³ scan | Set `SIT_SKIP_YPQ_RGB_STATS=1` in CI env |
| Wrong struct type | Passed `ColorYPQf` to byte helper | Match struct to function signature |
| Phase shift feels like HSV | Expecting HSV behavior | Re-read [YPQ vs HSV](#ypq-vs-hsv-why-phase-hue); try example 07 wheel |

**Legacy note:** Older docs listed `SituationCmdApplyYPQGrade` — that symbol does not exist. Use `SituationCmdDrawTextureYpqGrade`. Older docs also described `ColorYPQf.p` in degrees; the float struct stores **normalized phase [0, 1]** — only the adjust/grade APIs take `phase_shift_deg` in degrees.

---

### Performance Tips

- **GPU grade is O(screen pixels drawn)** — one textured quad draw, grade in fragment shader. Prefer for real-time.
- **CPU `ImageAdjustYPQ` is O(image pixels)** — fine for 512² atlases at load time; avoid 4K× per frame.
- Float pixel ops (`ColorYPQf`) are cheap — use for tooling UI swatches, not full images.
- `SituationYpqAnalyzeRgbMapping` allocates a 16 MB bitmap — run offline or on user request (example 07: X key).
- Matrix constants are shared CPU/GPU — no drift between paths when using the same four parameters.
