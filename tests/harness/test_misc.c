/**
 * @file test_misc.c
 * @brief Miscellaneous module tests — Image CPU ops, Fonts, Color conversions
 *
 * Some tests (image CPU ops, color) are context-free.
 * Font tests that involve GPU atlas baking require SituationInit().
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SIT_YPQ_PLANE_SIZE 256
#define SIT_YPQ_Q_SWEEP_SECONDS 4.0
#define SIT_YPQ_Q_STEPS 256
#define SIT_YPQ_PHOTO_MAX_DIM 1024
#define SIT_YPQ_PHOTO_SWEEP_DIM 384
#define SIT_YPQ_PHOTO_ASSET "prairie.jpg"
#define SIT_YPQ_PHOTO_SEGMENT_COUNT 3

static bool misc_ypq_pixel_is_gray(const ColorRGBA* c, int tolerance) {
    return abs((int)c->r - (int)c->g) <= tolerance
        && abs((int)c->g - (int)c->b) <= tolerance;
}

#define SIT_YPQ_PLANE_PIXELS (SIT_YPQ_PLANE_SIZE * SIT_YPQ_PLANE_SIZE)
#define SIT_YPQ_PLANE_BYTES  ((size_t)SIT_YPQ_PLANE_PIXELS * 4u)
#define SIT_YPQ_CUBE_KEYS    (256 * 256 * 256)
#define SIT_YPQ_RGB_KEY_COUNT (1 << 24)
#define SIT_YPQ_CUBE_SIZE     (256 * 256 * 256)

/** NTSC YIQ constants (must match sit/situation_impl_ypq.h). */
static const float MISC_YIQ_MAX_I = 0.595715671472f;
static const float MISC_YIQ_MAX_Q = 0.522591049541f;
static const float MISC_YIQ_RI = 0.95568806036115671171f;
static const float MISC_YIQ_RQ = 0.62082467141531188082f;
static const float MISC_YIQ_GI = -0.27178838506206335708f;
static const float MISC_YIQ_GQ = -0.64860590248778682744f;
static const float MISC_YIQ_BI = -1.1081773266826619523f;
static const float MISC_YIQ_BQ = 1.7025019884020956631f;
static const float MISC_INV255 = 1.0f / 255.0f;

/** y/255 for px=0..255 — avoids per-pixel divide in the fill hot loop. */
static float misc_ypq_y_lin[256];

static float misc_ypq_sin_p[256];
static float misc_ypq_cos_p[256];
static bool misc_ypq_lut_ready = false;

static void misc_ypq_init_lut(void) {
    if (misc_ypq_lut_ready) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        misc_ypq_y_lin[i] = (float)i * MISC_INV255;
        const double angle = ((double)i / 255.0) * 2.0 * M_PI;
        misc_ypq_sin_p[i] = (float)sin(angle);
        misc_ypq_cos_p[i] = (float)cos(angle);
    }
    misc_ypq_lut_ready = true;
}

static inline unsigned char misc_ypq_unit_to_byte(float unit) {
    if (unit <= 0.0f) {
        return 0;
    }
    if (unit >= 1.0f) {
        return 255;
    }
    return (unsigned char)(unit * 255.0f + 0.5f);
}

/** Bulk YPQ-byte → RGB key; matches SituationColorFromYPQ for NTSC packing. */
static inline uint32_t misc_ypq_fast_rgb_key(unsigned char y, unsigned char p, unsigned char q) {
    const float y_lin = misc_ypq_y_lin[y];
    const float amp = (float)q * MISC_INV255;
    const float i = amp * misc_ypq_cos_p[p] * MISC_YIQ_MAX_I;
    const float iq = amp * misc_ypq_sin_p[p] * MISC_YIQ_MAX_Q;

    float r = y_lin + MISC_YIQ_RI * i + MISC_YIQ_RQ * iq;
    float g = y_lin + MISC_YIQ_GI * i + MISC_YIQ_GQ * iq;
    float b = y_lin + MISC_YIQ_BI * i + MISC_YIQ_BQ * iq;
    if (r < 0.0f) {
        r = 0.0f;
    } else if (r > 1.0f) {
        r = 1.0f;
    }
    if (g < 0.0f) {
        g = 0.0f;
    } else if (g > 1.0f) {
        g = 1.0f;
    }
    if (b < 0.0f) {
        b = 0.0f;
    } else if (b > 1.0f) {
        b = 1.0f;
    }

    return ((uint32_t)misc_ypq_unit_to_byte(r) << 16)
        | ((uint32_t)misc_ypq_unit_to_byte(g) << 8)
        | (uint32_t)misc_ypq_unit_to_byte(b);
}

static inline uint32_t misc_ypq_rgb_key_from_bytes(unsigned char r, unsigned char g, unsigned char b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline ColorRGBA misc_ypq_fast_from_bytes(unsigned char y, unsigned char p, unsigned char q, unsigned char a) {
    const uint32_t key = misc_ypq_fast_rgb_key(y, p, q);
    return (ColorRGBA){
        (unsigned char)((key >> 16) & 0xffu),
        (unsigned char)((key >> 8) & 0xffu),
        (unsigned char)(key & 0xffu),
        a
    };
}

/**
 * Fill one Q plane into sweep RGBA and (optionally) the Q-major cube key slice.
 * Cube layout: index = q*65536 + y*256 + p  (y=px, p=py on the plane).
 */
static void misc_ypq_fill_y_p_plane_bytes(
    unsigned char* dst,
    unsigned char q,
    uint32_t* cube_q_slice,
    uint8_t* hit,
    int64_t* unique_rgb)
{
    misc_ypq_init_lut();
    const float amp_i = (float)q * MISC_INV255 * MISC_YIQ_MAX_I;
    const float amp_q = (float)q * MISC_INV255 * MISC_YIQ_MAX_Q;

    for (int py = 0; py < SIT_YPQ_PLANE_SIZE; py++) {
        const float i_scale = amp_i * misc_ypq_cos_p[py];
        const float q_scale = amp_q * misc_ypq_sin_p[py];
        unsigned char* row = dst + (size_t)py * (size_t)SIT_YPQ_PLANE_SIZE * 4u;

        for (int px = 0; px < SIT_YPQ_PLANE_SIZE; px++) {
            const float y_lin = misc_ypq_y_lin[px];
            float r = y_lin + MISC_YIQ_RI * i_scale + MISC_YIQ_RQ * q_scale;
            float g = y_lin + MISC_YIQ_GI * i_scale + MISC_YIQ_GQ * q_scale;
            float b = y_lin + MISC_YIQ_BI * i_scale + MISC_YIQ_BQ * q_scale;
            if (r < 0.0f) {
                r = 0.0f;
            } else if (r > 1.0f) {
                r = 1.0f;
            }
            if (g < 0.0f) {
                g = 0.0f;
            } else if (g > 1.0f) {
                g = 1.0f;
            }
            if (b < 0.0f) {
                b = 0.0f;
            } else if (b > 1.0f) {
                b = 1.0f;
            }

            const unsigned char rb = misc_ypq_unit_to_byte(r);
            const unsigned char gb = misc_ypq_unit_to_byte(g);
            const unsigned char bb = misc_ypq_unit_to_byte(b);
            const int idx = px * 4;
            row[idx + 0] = rb;
            row[idx + 1] = gb;
            row[idx + 2] = bb;
            row[idx + 3] = 255;

            if (cube_q_slice) {
                const uint32_t key = misc_ypq_rgb_key_from_bytes(rb, gb, bb);
                cube_q_slice[(size_t)px * 256u + (size_t)py] = key;
                if (hit && !hit[key]) {
                    hit[key] = 1;
                    (*unique_rgb)++;
                }
            }
        }
    }
}

static void misc_ypq_fill_ypq_y_p_plane(SituationImage* img, unsigned char q) {
    misc_ypq_fill_y_p_plane_bytes((unsigned char*)img->data, q, NULL, NULL, NULL);
}

/** One pass: sweep planes + Q-major cube keys + global unique RGB count. */
static int64_t misc_ypq_build_sweep_and_cube(unsigned char* sweep_cache, uint32_t* cube_keys) {
    uint8_t* hit = (uint8_t*)calloc(SIT_YPQ_RGB_KEY_COUNT, 1);
    SIT_ASSERT_NOT_NULL(hit);

    int64_t unique_rgb = 0;
    for (int q = 0; q < 256; q++) {
        misc_ypq_fill_y_p_plane_bytes(
            sweep_cache + (size_t)q * SIT_YPQ_PLANE_BYTES,
            (unsigned char)q,
            cube_keys + (size_t)q * 65536u,
            hit,
            &unique_rgb);
    }
    free(hit);
    return unique_rgb;
}

static ColorRGBA misc_sample_ypq_plane_rgba(unsigned char y, unsigned char p, unsigned char q) {
    misc_ypq_init_lut();
    return misc_ypq_fast_from_bytes(y, p, q, 255);
}

static ColorRGBA misc_ypq_image_center_pixel(const SituationImage* img) {
    int x = img->width / 2;
    int y = img->height / 2;
    unsigned char* pixels = (unsigned char*)img->data;
    int ch = img->channels > 0 ? img->channels : 4;
    int idx = (y * img->width + x) * ch;
    return (ColorRGBA){pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]};
}

static int misc_ypq_pixel_luma_sum(const ColorRGBA* c) {
    return (int)c->r + (int)c->g + (int)c->b;
}

static bool misc_ypq_ensure_rgba(SituationImage* img) {
    if (!SituationIsImageValid(*img)) {
        return false;
    }
    if (img->channels == 4) {
        return true;
    }
    if (img->channels != 3) {
        return false;
    }

    SituationImage rgba = {0};
    if (SituationCreateImage(img->width, img->height, 4, &rgba) != SITUATION_SUCCESS) {
        return false;
    }

    unsigned char* src = (unsigned char*)img->data;
    unsigned char* dst = (unsigned char*)rgba.data;
    int pixel_count = img->width * img->height;
    for (int i = 0; i < pixel_count; i++) {
        dst[i * 4 + 0] = src[i * 3 + 0];
        dst[i * 4 + 1] = src[i * 3 + 1];
        dst[i * 4 + 2] = src[i * 3 + 2];
        dst[i * 4 + 3] = 255;
    }

    SituationUnloadImage(*img);
    *img = rgba;
    return SituationIsImageValid(*img);
}

static void misc_ypq_downscale_nearest(SituationImage* img, int max_dim) {
    if (!SituationIsImageValid(*img) || max_dim < 1) {
        return;
    }

    int w = img->width;
    int h = img->height;
    int cur_max = w > h ? w : h;
    if (cur_max <= max_dim) {
        return;
    }

    int nw = (int)((long long)w * (long long)max_dim / (long long)cur_max);
    int nh = (int)((long long)h * (long long)max_dim / (long long)cur_max);
    if (nw < 1) {
        nw = 1;
    }
    if (nh < 1) {
        nh = 1;
    }

    SituationImage small = {0};
    if (SituationCreateImage(nw, nh, img->channels, &small) != SITUATION_SUCCESS) {
        return;
    }

    unsigned char* src = (unsigned char*)img->data;
    unsigned char* dst = (unsigned char*)small.data;
    int ch = img->channels;
    for (int y = 0; y < nh; y++) {
        int sy = y * h / nh;
        for (int x = 0; x < nw; x++) {
            int sx = x * w / nw;
            int sidx = (sy * w + sx) * ch;
            int didx = (y * nw + x) * ch;
            for (int c = 0; c < ch; c++) {
                dst[didx + c] = src[sidx + c];
            }
        }
    }

    SituationUnloadImage(*img);
    *img = small;
}

static bool misc_ypq_load_harness_photo(SituationImage* out, const char* filename) {
    static const char* prefixes[] = {
        "tests/harness/assets/",
        "../tests/harness/assets/",
        "../../tests/harness/assets/",
        NULL
    };
    char path[512];

    for (int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        if (SituationLoadImage(path, out) == SITUATION_SUCCESS && SituationIsImageValid(*out)) {
            if (!misc_ypq_ensure_rgba(out)) {
                SituationUnloadImage(*out);
                memset(out, 0, sizeof(*out));
                continue;
            }

            SituationImageResize(out,
                out->width > SIT_YPQ_PHOTO_MAX_DIM ? SIT_YPQ_PHOTO_MAX_DIM : out->width,
                out->height > SIT_YPQ_PHOTO_MAX_DIM ? SIT_YPQ_PHOTO_MAX_DIM : out->height);
            misc_ypq_downscale_nearest(out, SIT_YPQ_PHOTO_MAX_DIM);
            return SituationIsImageValid(*out);
        }
    }
    return false;
}

typedef enum MiscYpqPhotoSweepAxis {
    MISC_YPQ_PHOTO_SWEEP_LUMA = 0,
    MISC_YPQ_PHOTO_SWEEP_PHASE = 1,
    MISC_YPQ_PHOTO_SWEEP_CHROMA = 2
} MiscYpqPhotoSweepAxis;

static void misc_ypq_apply_photo_sweep(
    SituationImage* work,
    const SituationImage* source,
    MiscYpqPhotoSweepAxis axis,
    int step,
    int steps)
{
    size_t bytes = (size_t)source->width * (size_t)source->height * 4u;
    memcpy(work->data, source->data, bytes);

    float t = (steps <= 1) ? 0.0f : (float)step / (float)(steps - 1);
    float phase_deg = 0.0f;
    float chroma = 1.0f;
    float luma = 1.0f;

    switch (axis) {
    case MISC_YPQ_PHOTO_SWEEP_LUMA:
        luma = 0.25f + t * 1.5f;
        break;
    case MISC_YPQ_PHOTO_SWEEP_PHASE:
        phase_deg = t * 360.0f;
        break;
    case MISC_YPQ_PHOTO_SWEEP_CHROMA:
        chroma = t * 2.0f;
        break;
    default:
        break;
    }

    SituationImageAdjustYPQ(work, phase_deg, chroma, luma, 1.0f);
}

static const char* misc_ypq_photo_sweep_axis_name(MiscYpqPhotoSweepAxis axis) {
    switch (axis) {
    case MISC_YPQ_PHOTO_SWEEP_LUMA:
        return "Y (luma)";
    case MISC_YPQ_PHOTO_SWEEP_PHASE:
        return "P (phase)";
    case MISC_YPQ_PHOTO_SWEEP_CHROMA:
        return "Q (chroma)";
    default:
        return "YPQ";
    }
}

static void misc_ypq_radix_sort_u32(uint32_t* keys, uint32_t* temp, int n) {
    uint32_t count[256];
    uint32_t* src = keys;
    uint32_t* dst = temp;

    for (int pass = 0; pass < 4; pass++) {
        memset(count, 0, sizeof(count));
        const int shift = pass * 8;
        for (int i = 0; i < n; i++) {
            count[(src[i] >> shift) & 0xffu]++;
        }
        int pos = 0;
        for (int b = 0; b < 256; b++) {
            const int c = count[b];
            count[b] = pos;
            pos += c;
        }
        for (int i = 0; i < n; i++) {
            const int b = (src[i] >> shift) & 0xffu;
            dst[count[b]++] = src[i];
        }
        uint32_t* swap = src;
        src = dst;
        dst = swap;
    }
    if (src != keys) {
        memcpy(keys, src, (size_t)n * sizeof(uint32_t));
    }
}

static int misc_ypq_count_duplicates_sorted(const uint32_t* sorted, int n) {
    if (n <= 1) {
        return 0;
    }
    int dup = 0;
    for (int i = 1; i < n; i++) {
        if (sorted[i] == sorted[i - 1]) {
            dup++;
        }
    }
    return dup;
}

typedef struct MiscYpqRegistryDupReport {
    const char* axis;
    int per_registry[256];
    int64_t duplicate_mappings;
    int min_duplicates;
    int max_duplicates;
    int max_duplicates_at;
    double avg_duplicates;
} MiscYpqRegistryDupReport;

static void misc_ypq_scan_registry_dup_report_from_cube(
    const uint32_t* cube_keys,
    uint32_t* slice_keys,
    uint32_t* slice_tmp,
    MiscYpqRegistryDupReport* report,
    char axis)
{
    memset(report, 0, sizeof(*report));
    report->axis = (axis == 'Y') ? "Y" : ((axis == 'P') ? "P" : "Q");
    report->min_duplicates = INT_MAX;
    report->max_duplicates_at = -1;

    for (int i = 0; i < 256; i++) {
        int dup = 0;
        if (axis == 'Y') {
            for (int q = 0; q < 256; q++) {
                memcpy(
                    slice_keys + (size_t)q * 256u,
                    cube_keys + (size_t)q * 65536u + (size_t)i * 256u,
                    256u * sizeof(uint32_t));
            }
        } else if (axis == 'P') {
            int n = 0;
            for (int q = 0; q < 256; q++) {
                for (int y = 0; y < 256; y++) {
                    slice_keys[n++] = cube_keys[(size_t)q * 65536u + (size_t)y * 256u + (size_t)i];
                }
            }
        } else {
            memcpy(slice_keys, cube_keys + (size_t)i * 65536u, 65536u * sizeof(uint32_t));
        }

        misc_ypq_radix_sort_u32(slice_keys, slice_tmp, 65536);
        dup = misc_ypq_count_duplicates_sorted(slice_keys, 65536);
        report->per_registry[i] = dup;
        report->duplicate_mappings += dup;
        if (dup < report->min_duplicates) {
            report->min_duplicates = dup;
        }
        if (dup > report->max_duplicates) {
            report->max_duplicates = dup;
            report->max_duplicates_at = i;
        }
    }
    report->avg_duplicates = (double)report->duplicate_mappings / 256.0;
}

static void misc_ypq_report_registry_from_cube(
    const uint32_t* cube_keys,
    int64_t unique_rgb,
    int64_t* out_duplicate_mappings,
    int64_t* out_rgb_holes,
    MiscYpqRegistryDupReport* y_report,
    MiscYpqRegistryDupReport* p_report,
    MiscYpqRegistryDupReport* q_report)
{
    uint32_t* slice_keys = (uint32_t*)malloc(65536u * sizeof(uint32_t));
    uint32_t* slice_tmp = (uint32_t*)malloc(65536u * sizeof(uint32_t));
    SIT_ASSERT_NOT_NULL(slice_keys);
    SIT_ASSERT_NOT_NULL(slice_tmp);

    *out_duplicate_mappings = (int64_t)SIT_YPQ_CUBE_SIZE - unique_rgb;
    *out_rgb_holes = (int64_t)SIT_YPQ_RGB_KEY_COUNT - unique_rgb;

    misc_ypq_scan_registry_dup_report_from_cube(cube_keys, slice_keys, slice_tmp, y_report, 'Y');
    misc_ypq_scan_registry_dup_report_from_cube(cube_keys, slice_keys, slice_tmp, p_report, 'P');
    misc_ypq_scan_registry_dup_report_from_cube(cube_keys, slice_keys, slice_tmp, q_report, 'Q');

    free(slice_tmp);
    free(slice_keys);
}

static void misc_ypq_print_registry_dup_report(const MiscYpqRegistryDupReport* report) {
    fprintf(stderr,
            "[misc]   %s registry: duplicate RGB mappings per slice (65536 %s×%s combos each)\n",
            report->axis,
            (report->axis[0] == 'Y') ? "P" : "Y",
            (report->axis[0] == 'Q') ? "P" : "Q");
    fprintf(stderr,
            "[misc]     total duplicates=%lld  min=%d  max=%d (at %s=%d)  avg=%.1f\n",
            (long long)report->duplicate_mappings,
            report->min_duplicates,
            report->max_duplicates,
            report->axis,
            report->max_duplicates_at,
            report->avg_duplicates);
}

/**
 * Print duplicate-RGB stats from a pre-built Q-major cube (see misc_ypq_build_sweep_and_cube).
 * Set SIT_SKIP_YPQ_RGB_STATS=1 to skip registry analysis after the sweep.
 */
static void misc_ypq_report_rgb_duplicate_stats(const uint32_t* cube_keys, int64_t unique_rgb) {
    if (getenv("SIT_SKIP_YPQ_RGB_STATS") != NULL) {
        fprintf(stderr, "[misc] ypq RGB duplicate stats skipped (SIT_SKIP_YPQ_RGB_STATS set)\n");
        return;
    }
    if (!cube_keys) {
        return;
    }

    fprintf(stderr, "[misc] ypq_to_rgb: registry duplicate RGB analysis (cube already built)...\n");

    int64_t duplicate_mappings = 0;
    int64_t rgb_holes = 0;
    MiscYpqRegistryDupReport y_report = {0};
    MiscYpqRegistryDupReport p_report = {0};
    MiscYpqRegistryDupReport q_report = {0};
    misc_ypq_report_registry_from_cube(
        cube_keys, unique_rgb, &duplicate_mappings, &rgb_holes, &y_report, &p_report, &q_report);

    fprintf(stderr,
            "[misc]   full cube: %d YPQ mappings -> %lld unique RGB"
            "  (%lld duplicate mappings, %lld RGB holes never hit)\n",
            SIT_YPQ_CUBE_SIZE,
            (long long)unique_rgb,
            (long long)duplicate_mappings,
            (long long)rgb_holes);

    misc_ypq_print_registry_dup_report(&y_report);
    misc_ypq_print_registry_dup_report(&p_report);
    misc_ypq_print_registry_dup_report(&q_report);
}

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
static SituationError misc_ypq_texture_barrier(
    SituationCommandBuffer cmd,
    SituationTexture tex,
    SituationTextureLayout old_layout,
    SituationTextureLayout new_layout)
{
    SituationTextureBarrierDesc desc = {0};
    desc.old_layout = old_layout;
    desc.new_layout = new_layout;
    desc.base_mip_level = 0;
    desc.mip_level_count = 1;
    return SituationCmdTextureBarrier(cmd, tex, &desc);
}

/** Upload the CPU image (if needed) and draw it stretched over the full window. */
static SituationError misc_ypq_present_image(
    SituationImage* image,
    SituationTexture* tex,
    SituationBuffer* upload_buf,
    bool* tex_ready)
{
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    const int img_w = image->width;
    const int img_h = image->height;
    const int img_ch = image->channels > 0 ? image->channels : 4;
    SituationError err;
    if (!*tex_ready) {
        err = SituationCreateTextureEx(
            *image,
            false,
            (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED |
                                         SITUATION_TEXTURE_USAGE_TRANSFER_DST),
            tex);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        *tex_ready = true;
    } else {
        size_t image_bytes = (size_t)img_w * (size_t)img_h * (size_t)img_ch;
        err = SituationUpdateBuffer(*upload_buf, 0, image_bytes, image->data);
        if (err != SITUATION_SUCCESS) {
            return err;
        }

        SituationTextureCopyRegion region = {0};
        region.src_rect = (SituationTextureRect){0, 0, img_w, img_h};
        region.dst_x = 0;
        region.dst_y = 0;

        err = misc_ypq_texture_barrier(
            cmd, *tex,
            SITUATION_TEXTURE_LAYOUT_SHADER_READ,
            SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = SituationCmdCopyBufferToTexture(cmd, *upload_buf, 0, *tex, &region);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = misc_ypq_texture_barrier(
            cmd, *tex,
            SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
            SITUATION_TEXTURE_LAYOUT_SHADER_READ);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SitRectangle source = {0.0f, 0.0f, (float)img_w, (float)img_h};
    SitRectangle dest = sit_test_full_window_dest();
    Vector2 origin = {{0.0f, 0.0f}};
    ColorRGBA tint = {255, 255, 255, 255};
    err = SituationCmdDrawTexture(cmd, *tex, source, dest, origin, 0.0f, tint);
    if (err != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return err;
    }

    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    return SituationEndFrame();
}

static SituationError misc_ypq_present_plane(
    SituationImage* plane,
    SituationTexture* tex,
    SituationBuffer* upload_buf,
    bool* tex_ready)
{
    return misc_ypq_present_image(plane, tex, upload_buf, tex_ready);
}
#endif /* GPU backends */

// ============================================================================
//  Cleanup helper
// ============================================================================

static void misc_teardown(void) {
    // Remove any leftover test artifacts
    SituationDeleteFile("_sit_test_export.png");
    SituationDeleteFile("_sit_test_export.bmp");
}

// ============================================================================
//  Image CPU Operations
// ============================================================================

static void test_create_image(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(img));
    SIT_ASSERT_EQ(img.width, 4);
    SIT_ASSERT_EQ(img.height, 4);
    SIT_ASSERT_EQ(img.channels, 4);
    SIT_ASSERT_NOT_NULL(img.data);
    SituationUnloadImage(img);
}

static void test_set_pixel_color(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    ColorRGBA red = {255, 0, 0, 255};
    SituationSetPixelColor(&img, 1, 2, red);

    // Verify pixel directly from raw data (RGBA, row-major)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset = (2 * 4 + 1) * 4; // (y * width + x) * channels
    SIT_ASSERT_EQ(pixels[offset + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset + 2], 0);   // B
    SIT_ASSERT_EQ(pixels[offset + 3], 255); // A

    SituationUnloadImage(img);
}

static void test_gen_image_color(void) {
    SituationImage img = {0};
    ColorRGBA blue = {0, 0, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, blue, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(img));
    SIT_ASSERT_EQ(img.width, 8);
    SIT_ASSERT_EQ(img.height, 8);

    // Verify a few pixels are blue
    unsigned char* pixels = (unsigned char*)img.data;
    int channels = img.channels;
    // Check pixel (0,0)
    SIT_ASSERT_EQ(pixels[0], 0);
    SIT_ASSERT_EQ(pixels[1], 0);
    SIT_ASSERT_EQ(pixels[2], 255);
    if (channels >= 4) SIT_ASSERT_EQ(pixels[3], 255);
    // Check pixel (7,7)
    int offset = (7 * 8 + 7) * channels;
    SIT_ASSERT_EQ(pixels[offset + 0], 0);
    SIT_ASSERT_EQ(pixels[offset + 1], 0);
    SIT_ASSERT_EQ(pixels[offset + 2], 255);

    SituationUnloadImage(img);
}

static void test_image_copy(void) {
    SituationImage src = {0};
    ColorRGBA green = {0, 255, 0, 255};
    SituationError err = SituationGenImageColor(4, 4, green, &src);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImage dst = {0};
    err = SituationImageCopy(src, &dst);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(dst));
    SIT_ASSERT_EQ(dst.width, src.width);
    SIT_ASSERT_EQ(dst.height, src.height);
    SIT_ASSERT_EQ(dst.channels, src.channels);

    // Verify data matches
    int size = src.width * src.height * src.channels;
    SIT_ASSERT_MEM_EQ(src.data, dst.data, size);

    // Verify they are independent copies (different pointers)
    SIT_ASSERT(src.data != dst.data);

    SituationUnloadImage(src);
    SituationUnloadImage(dst);
}

static void test_image_crop(void) {
    SituationImage img = {0};
    ColorRGBA white = {255, 255, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, white, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SitRectangle crop = {1.0f, 1.0f, 4.0f, 4.0f};
    SituationImageCrop(&img, crop);

    SIT_ASSERT_EQ(img.width, 4);
    SIT_ASSERT_EQ(img.height, 4);
    SIT_ASSERT(SituationIsImageValid(img));

    SituationUnloadImage(img);
}

static void test_image_resize(void) {
    SituationImage img = {0};
    ColorRGBA cyan = {0, 255, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, cyan, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImageResize(&img, 16, 16);

    // Resize may be a no-op if stb_image_resize is not compiled in.
    // Accept either the resized result or the original dimensions.
    SIT_ASSERT(img.width == 16 || img.width == 8);
    if (img.width == 16) {
        SIT_ASSERT_EQ(img.height, 16);
    }
    SIT_ASSERT(SituationIsImageValid(img));

    SituationUnloadImage(img);
}

static void test_image_flip_vertical(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Set top-left pixel to red
    ColorRGBA red = {255, 0, 0, 255};
    ColorRGBA black = {0, 0, 0, 255};
    // Fill with black first
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            SituationSetPixelColor(&img, x, y, black);
    SituationSetPixelColor(&img, 0, 0, red);

    SituationImageFlip(&img, SIT_FLIP_VERTICAL);

    // After vertical flip, top-left should now be at bottom-left (0, 3)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset_bottom_left = (3 * 4 + 0) * 4;
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 2], 0);   // B

    SituationUnloadImage(img);
}

static void test_image_flip_horizontal(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Fill with black, set top-left to red
    ColorRGBA red = {255, 0, 0, 255};
    ColorRGBA black = {0, 0, 0, 255};
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            SituationSetPixelColor(&img, x, y, black);
    SituationSetPixelColor(&img, 0, 0, red);

    SituationImageFlip(&img, SIT_FLIP_HORIZONTAL);

    // After horizontal flip, (0,0) should now be at (3,0)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset_top_right = (0 * 4 + 3) * 4;
    SIT_ASSERT_EQ(pixels[offset_top_right + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset_top_right + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset_top_right + 2], 0);   // B

    SituationUnloadImage(img);
}

static void test_image_export_and_load(void) {
    // Create a 4x4 red image
    SituationImage img = {0};
    ColorRGBA red = {255, 0, 0, 255};
    SituationError err = SituationGenImageColor(4, 4, red, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Export to PNG
    err = SituationExportImage(img, "_sit_test_export.png");
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Load it back
    SituationImage loaded = {0};
    err = SituationLoadImage("_sit_test_export.png", &loaded);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(loaded));
    SIT_ASSERT_EQ(loaded.width, 4);
    SIT_ASSERT_EQ(loaded.height, 4);

    // Verify pixel data matches (red)
    unsigned char* pixels = (unsigned char*)loaded.data;
    SIT_ASSERT_EQ(pixels[0], 255); // R
    SIT_ASSERT_EQ(pixels[1], 0);   // G
    SIT_ASSERT_EQ(pixels[2], 0);   // B

    SituationUnloadImage(img);
    SituationUnloadImage(loaded);
    SituationDeleteFile("_sit_test_export.png");
}

static void test_image_load_from_memory(void) {
    // Create and export an image first to get valid file data
    SituationImage img = {0};
    ColorRGBA magenta = {255, 0, 255, 255};
    SituationError err = SituationGenImageColor(2, 2, magenta, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    err = SituationExportImage(img, "_sit_test_export.bmp");
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SituationUnloadImage(img);

    // Load the file data into memory
    unsigned int dataSize = 0;
    unsigned char* fileData = NULL;
    err = SituationLoadFileData("_sit_test_export.bmp", &dataSize, &fileData);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(fileData);
    SIT_ASSERT(dataSize > 0);

    // Load image from memory
    SituationImage loaded = {0};
    err = SituationLoadImageFromMemory(".bmp", fileData, dataSize, &loaded);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(loaded));
    SIT_ASSERT_EQ(loaded.width, 2);
    SIT_ASSERT_EQ(loaded.height, 2);

    SituationUnloadImage(loaded);
    free(fileData);
    SituationDeleteFile("_sit_test_export.bmp");
}

static void test_image_invalid_check(void) {
    SituationImage invalid = {0};
    SIT_ASSERT(!SituationIsImageValid(invalid));
}

// ============================================================================
//  Font Tests
// ============================================================================

static void test_load_bitmap_font_from_memory(void) {
    // Create a minimal 8x8 bitmap font (256 chars, each 8x8 = 1 byte per row)
    // Just a block of data — we only need to verify the API doesn't crash
    unsigned char bitmap_data[256 * 8]; // 256 chars, 8 bytes each (8x8 1-bit)
    memset(bitmap_data, 0xAA, sizeof(bitmap_data)); // Striped pattern

    SituationFont font = {0};
    SituationError err = SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(font.is_bitmap);
    SIT_ASSERT_EQ(font.bitmap_width, 8);
    SIT_ASSERT_EQ(font.bitmap_height, 8);
    SIT_ASSERT_EQ(font.bitmap_count, 256);

    SituationUnloadFont(font);
}

static void test_measure_text_bitmap_font(void) {
    // Load a bitmap font and measure text
    unsigned char bitmap_data[256 * 8];
    memset(bitmap_data, 0xFF, sizeof(bitmap_data));

    SituationFont font = {0};
    SituationError err = SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SitRectangle bounds = SituationMeasureText(font, "Hello", 8.0f);
    // For a bitmap font with 8px wide chars, "Hello" (5 chars) should be ~40px wide
    SIT_ASSERT(bounds.width > 0.0f);
    SIT_ASSERT(bounds.height > 0.0f);

    // Empty string should have zero or near-zero width
    SitRectangle empty_bounds = SituationMeasureText(font, "", 8.0f);
    SIT_ASSERT(empty_bounds.width <= 0.0f);

    SituationUnloadFont(font);
}

// ============================================================================
//  Color Conversion Tests
// ============================================================================

static void test_rgb_to_hsv_roundtrip(void) {
    // Test with a known color: pure red
    ColorRGBA red = {255, 0, 0, 255};
    ColorHSV hsv = SituationRgbToHsv(red);

    // Red should be H≈0, S≈1, V≈1
    SIT_ASSERT(hsv.h >= 0.0f && hsv.h <= 1.0f); // Could be 0 or 360 normalized
    SIT_ASSERT(hsv.s >= 0.9f);
    SIT_ASSERT(hsv.v >= 0.9f);

    // Convert back
    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - (int)red.r) <= 1);
    SIT_ASSERT(abs((int)back.g - (int)red.g) <= 1);
    SIT_ASSERT(abs((int)back.b - (int)red.b) <= 1);
}

static void test_rgb_to_hsv_green(void) {
    ColorRGBA green = {0, 255, 0, 255};
    ColorHSV hsv = SituationRgbToHsv(green);

    // Green should have H≈120 (or normalized equivalent), S≈1, V≈1
    SIT_ASSERT(hsv.s >= 0.9f);
    SIT_ASSERT(hsv.v >= 0.9f);

    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - (int)green.r) <= 1);
    SIT_ASSERT(abs((int)back.g - (int)green.g) <= 1);
    SIT_ASSERT(abs((int)back.b - (int)green.b) <= 1);
}

static void test_rgb_to_hsv_gray(void) {
    // Gray has no saturation
    ColorRGBA gray = {128, 128, 128, 255};
    ColorHSV hsv = SituationRgbToHsv(gray);
    SIT_ASSERT(hsv.s < 0.01f); // Saturation should be ~0 for gray

    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - 128) <= 1);
    SIT_ASSERT(abs((int)back.g - 128) <= 1);
    SIT_ASSERT(abs((int)back.b - 128) <= 1);
}

static void test_color_to_ypq_roundtrip(void) {
    ColorRGBA original = {200, 100, 50, 255};
    ColorYPQA ypq = SituationColorToYPQ(original);

    // Y (luminance) should be non-zero for a non-black color
    SIT_ASSERT(ypq.y > 0);
    SIT_ASSERT_EQ(ypq.a, 255);

    // Convert back
    ColorRGBA back = SituationColorFromYPQ(ypq);
    // Allow ±2 tolerance for quantization
    SIT_ASSERT(abs((int)back.r - (int)original.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)original.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)original.b) <= 2);
    SIT_ASSERT_EQ(back.a, original.a);
}

/** Phase 0: shared YIQ core — Q=0 makes phase irrelevant (grayscale slice). */
static void test_ypq_groundwork_q_zero_phase_invariant(void) {
    ColorRGBA p0 = SituationColorFromYPQ((ColorYPQA){128, 0, 0, 255});
    ColorRGBA p128 = SituationColorFromYPQ((ColorYPQA){128, 128, 0, 255});
    ColorRGBA p255 = SituationColorFromYPQ((ColorYPQA){128, 255, 0, 255});

    SIT_ASSERT(misc_ypq_pixel_is_gray(&p0, 3));
    SIT_ASSERT(misc_ypq_pixel_is_gray(&p128, 3));
    SIT_ASSERT(misc_ypq_pixel_is_gray(&p255, 3));
    SIT_ASSERT(abs((int)p0.r - (int)p128.r) <= 1);
    SIT_ASSERT(abs((int)p0.g - (int)p128.g) <= 1);
    SIT_ASSERT(abs((int)p0.b - (int)p128.b) <= 1);
    SIT_ASSERT(abs((int)p128.r - (int)p255.r) <= 1);
}

/** Phase 0: Y luma axis at Q=0 tracks grayscale RGB. */
static void test_ypq_groundwork_grayscale_luma_axis(void) {
    for (int y = 0; y < 256; y += 17) {
        ColorRGBA rgb = SituationColorFromYPQ((ColorYPQA){(unsigned char)y, 64, 0, 255});
        SIT_ASSERT(misc_ypq_pixel_is_gray(&rgb, 3));
        SIT_ASSERT(abs((int)rgb.r - y) <= 2);
        SIT_ASSERT(abs((int)rgb.g - y) <= 2);
        SIT_ASSERT(abs((int)rgb.b - y) <= 2);
    }
}

/** Phase 0: regression anchors for NTSC YIQ matrix (post-refactor parity). */
static void test_ypq_groundwork_golden_vectors(void) {
    ColorRGBA black = SituationColorFromYPQ((ColorYPQA){0, 0, 0, 255});
    SIT_ASSERT_EQ(black.r, 0);
    SIT_ASSERT_EQ(black.g, 0);
    SIT_ASSERT_EQ(black.b, 0);

    ColorRGBA white = SituationColorFromYPQ((ColorYPQA){255, 0, 0, 255});
    SIT_ASSERT_EQ(white.r, 255);
    SIT_ASSERT_EQ(white.g, 255);
    SIT_ASSERT_EQ(white.b, 255);

    ColorRGBA mid_gray = SituationColorFromYPQ((ColorYPQA){128, 200, 0, 255});
    SIT_ASSERT(abs((int)mid_gray.r - 128) <= 1);
    SIT_ASSERT(abs((int)mid_gray.g - 128) <= 1);
    SIT_ASSERT(abs((int)mid_gray.b - 128) <= 1);

    ColorYPQA from_red = SituationColorToYPQ((ColorRGBA){255, 0, 0, 255});
    SIT_ASSERT(from_red.y > 50);
    SIT_ASSERT(from_red.q > 50);
}

static void test_ypq_lerp_wrap(void) {
    ColorYPQA a = {128, 250, 200, 255};
    ColorYPQA b = {128, 10, 200, 255};
    ColorYPQA mid = SituationYpqLerp(a, b, 0.5f);

    int linear_p = (250 + 10) / 2;
    SIT_ASSERT(abs((int)mid.p - linear_p) > 40);

    ColorYPQA at_a = SituationYpqLerp(a, b, 0.0f);
    ColorYPQA at_b = SituationYpqLerp(a, b, 1.0f);
    SIT_ASSERT_EQ(at_a.y, a.y);
    SIT_ASSERT_EQ(at_a.p, a.p);
    SIT_ASSERT_EQ(at_b.p, b.p);
}

static void test_ypq_adjust_luma(void) {
    ColorYPQA c = {100, 64, 180, 255};
    ColorYPQA bright = SituationYpqAdjustLuma(c, 2.0f);
    SIT_ASSERT(bright.y > c.y);
    SIT_ASSERT_EQ(bright.p, c.p);
    SIT_ASSERT_EQ(bright.q, c.q);
}

static void test_ypq_adjust_phase(void) {
    ColorYPQA c = {100, 10, 180, 255};
    ColorYPQA shifted = SituationYpqAdjustPhase(c, 20);
    SIT_ASSERT_EQ((int)shifted.p, 30);
    ColorYPQA wrap = SituationYpqAdjustPhase((ColorYPQA){100, 250, 180, 255}, 20);
    SIT_ASSERT_EQ((int)wrap.p, 14);
}

static void test_ypq_adjust_chroma(void) {
    ColorYPQA c = {100, 64, 100, 255};
    ColorYPQA sat = SituationYpqAdjustChroma(c, 2.0f);
    SIT_ASSERT(sat.q > c.q);
    SIT_ASSERT_EQ(sat.p, c.p);

    ColorRGBA p0a = SituationColorFromYPQ((ColorYPQA){128, 0, 0, 255});
    ColorRGBA p0b = SituationColorFromYPQ((ColorYPQA){128, 200, 0, 255});
    SIT_ASSERT(abs((int)p0a.r - (int)p0b.r) <= 2);
}

static void test_ypq_distance_equals(void) {
    ColorYPQA a = {100, 50, 120, 255};
    ColorYPQA b = {100, 50, 120, 255};
    SIT_ASSERT(SituationYpqEquals(a, b, 0));
    SIT_ASSERT(fabsf(SituationYpqDistance(a, b)) < 0.001f);

    ColorYPQA c = {200, 180, 240, 255};
    SIT_ASSERT(!SituationYpqEquals(a, c, 5));
    SIT_ASSERT(SituationYpqDistance(a, c) > SituationYpqDistance(a, b));

    SIT_ASSERT(fabsf(SituationYpqGetLuma(a) - (100.0f / 255.0f)) < 0.01f);
    SIT_ASSERT(fabsf(SituationYpqGetChroma(a) - (120.0f / 255.0f)) < 0.01f);
    SIT_ASSERT(fabsf(SituationYpqGetHueDegrees(a) - ((50.0f / 255.0f) * 360.0f)) < 2.0f);
}

static void test_ypq_float_roundtrip(void) {
    ColorRGBA original = {180, 90, 40, 255};
    ColorYPQf ypq = SituationColorToYPQf(original);
    ColorRGBA back = SituationColorFromYPQf(ypq);

    SIT_ASSERT(abs((int)back.r - (int)original.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)original.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)original.b) <= 2);
    SIT_ASSERT_EQ(back.a, original.a);

    ColorYPQA bytes = SituationYpqQuantize(ypq);
    SIT_ASSERT(abs((int)bytes.y - (int)(ypq.y * 255.0f + 0.5f)) <= 1);
}

static void test_ypq_quantize(void) {
    ColorYPQf src = {0.5f, 0.25f, 0.75f, 1.0f};
    ColorYPQA q = SituationYpqQuantize(src);
    SIT_ASSERT(abs((int)q.y - 128) <= 1);
    SIT_ASSERT(abs((int)q.p - 64) <= 1);
    SIT_ASSERT(abs((int)q.q - 191) <= 1);
    SIT_ASSERT_EQ(q.a, 255);

    ColorYPQf clamped = SituationYpqClampInGamut((ColorYPQf){1.0f, 0.0f, 1.0f, 1.0f});
    SIT_ASSERT(clamped.q < 1.0f);
    ColorRGBA hot = SituationColorFromYPQf((ColorYPQf){1.0f, 0.0f, 1.0f, 1.0f});
    ColorRGBA safe = SituationColorFromYPQf(clamped);
    SIT_ASSERT(abs((int)hot.r - (int)safe.r) > 0 || abs((int)hot.g - (int)safe.g) > 0);
}

static void test_image_adjust_ypq(void) {
    SituationImage img = {0};
    ColorRGBA blue = {40, 80, 220, 255};
    SituationError err = SituationGenImageColor(8, 8, blue, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImageAdjustYPQ(&img, 0.0f, 2.0f, 1.0f, 1.0f);

    unsigned char* pixels = (unsigned char*)img.data;
    ColorRGBA center = {pixels[0], pixels[1], pixels[2], pixels[3]};
    SIT_ASSERT(!misc_ypq_pixel_is_gray(&center, 12));
    SIT_ASSERT(abs((int)center.b - (int)blue.b) > 8 || abs((int)center.r - (int)blue.r) > 8);

    SituationUnloadImage(img);
}

/**
 * CPU: 256x256 Y (x) x P (y) plane at several Q values; FromYPQ must be stable and sane.
 */
static void test_ypq_to_rgb_y_p_plane(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(SIT_YPQ_PLANE_SIZE, SIT_YPQ_PLANE_SIZE, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(img.data);

    misc_ypq_fill_ypq_y_p_plane(&img, 0);
  {
    unsigned char* pixels = (unsigned char*)img.data;
    for (int i = 0; i < SIT_YPQ_PLANE_SIZE * SIT_YPQ_PLANE_SIZE; i++) {
        ColorRGBA c = {pixels[i * 4 + 0], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]};
        SIT_ASSERT(misc_ypq_pixel_is_gray(&c, 3));
    }
  }

    ColorRGBA c_q0 = misc_sample_ypq_plane_rgba(200, 64, 0);
    ColorRGBA c_qmax = misc_sample_ypq_plane_rgba(200, 64, 255);
    SIT_ASSERT(misc_ypq_pixel_is_gray(&c_q0, 3));
    SIT_ASSERT(!misc_ypq_pixel_is_gray(&c_qmax, 8));

    ColorRGBA c_p0 = misc_sample_ypq_plane_rgba(200, 0, 255);
    ColorRGBA c_p128 = misc_sample_ypq_plane_rgba(200, 128, 255);
    SIT_ASSERT(abs((int)c_p0.r - (int)c_p128.r) > 12
        || abs((int)c_p0.g - (int)c_p128.g) > 12
        || abs((int)c_p0.b - (int)c_p128.b) > 12);

    ColorRGBA seed = {180, 90, 40, 255};
    ColorYPQA mid = SituationColorToYPQ(seed);
    ColorRGBA back = SituationColorFromYPQ(mid);
    SIT_ASSERT(abs((int)back.r - (int)seed.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)seed.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)seed.b) <= 2);

    misc_ypq_init_lut();
    for (int trial = 0; trial < 8; trial++) {
        unsigned char ty = (unsigned char)(trial * 31);
        unsigned char tp = (unsigned char)(trial * 47 + 64);
        unsigned char tq = (unsigned char)(trial * 19 + 128);
        ColorYPQA ypq_t = {ty, tp, tq, 255};
        ColorRGBA lib = SituationColorFromYPQ(ypq_t);
        ColorRGBA fast = misc_ypq_fast_from_bytes(ty, tp, tq, 255);
        SIT_ASSERT(abs((int)lib.r - (int)fast.r) <= 1);
        SIT_ASSERT(abs((int)lib.g - (int)fast.g) <= 1);
        SIT_ASSERT(abs((int)lib.b - (int)fast.b) <= 1);
    }

    SituationUnloadImage(img);
}

/**
 * Timed: Q=0..255 over ~4s on the 256x256 Y (x) x P (y) plane.
 * Full-window display: Y horizontal, P vertical, Q animates grayscale → chroma.
 * After the sweep, reports duplicate 8-bit RGB mappings per Y/P/Q registry.
 * Set SIT_SKIP_YPQ_SWEEP=1 to skip (~4s wall time).
 * Set SIT_SKIP_YPQ_RGB_STATS=1 to skip post-sweep registry analysis.
 */
static void test_ypq_to_rgb_q_sweep_4s(void) {
    if (getenv("SIT_SKIP_YPQ_SWEEP") != NULL) {
        fprintf(stderr, "[misc] ypq_to_rgb_q_sweep_4s skipped (SIT_SKIP_YPQ_SWEEP set)\n");
        return;
    }

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationError init_err = sit_test_gpu_context_init("SIT_YPQ_Q_SWEEP");
    SIT_ASSERT_EQ((int)init_err, (int)SITUATION_SUCCESS);
#endif

    SituationImage plane = {0};
    SituationError err = SituationCreateImage(SIT_YPQ_PLANE_SIZE, SIT_YPQ_PLANE_SIZE, 4, &plane);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    unsigned char* sweep_cache = (unsigned char*)malloc(SIT_YPQ_PLANE_BYTES * 256u);
    uint32_t* cube_keys = (uint32_t*)malloc((size_t)SIT_YPQ_CUBE_KEYS * sizeof(uint32_t));
    SIT_ASSERT_NOT_NULL(sweep_cache);
    SIT_ASSERT_NOT_NULL(cube_keys);
    int64_t unique_rgb = misc_ypq_build_sweep_and_cube(sweep_cache, cube_keys);
    SIT_ASSERT(unique_rgb > 0);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationTexture tex = {0};
    SituationBuffer upload_buf = {0};
    bool tex_ready = false;
    err = SituationCreateBuffer(
        SIT_YPQ_PLANE_BYTES,
        NULL,
        SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &upload_buf);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif

    int max_q_seen = -1;
    int step_count = 0;
    bool saw_gray_center = false;
    bool saw_chroma_center = false;

    const double step_seconds = SIT_YPQ_Q_SWEEP_SECONDS / (double)SIT_YPQ_Q_STEPS;
    double t0 = sit_get_time_seconds();

    for (int q = 0; q < SIT_YPQ_Q_STEPS; q++) {
        const double step_end = t0 + (double)(q + 1) * step_seconds;

        memcpy(plane.data, sweep_cache + (size_t)q * SIT_YPQ_PLANE_BYTES, SIT_YPQ_PLANE_BYTES);

        if (q == 0) {
            unsigned char* pixels = (unsigned char*)plane.data;
            int idx = (128 * SIT_YPQ_PLANE_SIZE + 128) * 4;
            ColorRGBA center = {pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]};
            saw_gray_center = misc_ypq_pixel_is_gray(&center, 3);
        }
        if (q >= 200) {
            ColorRGBA center = misc_sample_ypq_plane_rgba(128, 128, (unsigned char)q);
            if (!misc_ypq_pixel_is_gray(&center, 10)) {
                saw_chroma_center = true;
            }
        }

        max_q_seen = q;
        step_count++;

        do {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
            err = misc_ypq_present_plane(&plane, &tex, &upload_buf, &tex_ready);
            SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif
        } while (sit_get_time_seconds() < step_end);
    }

    double elapsed_total = sit_get_time_seconds() - t0;
    SIT_ASSERT(elapsed_total >= (SIT_YPQ_Q_SWEEP_SECONDS - 0.05));
    SIT_ASSERT(elapsed_total <= (SIT_YPQ_Q_SWEEP_SECONDS + 2.5));
    SIT_ASSERT_EQ(step_count, SIT_YPQ_Q_STEPS);
    SIT_ASSERT_EQ(max_q_seen, 255);
    SIT_ASSERT(saw_gray_center);
    SIT_ASSERT(saw_chroma_center);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (tex_ready) {
        SituationDestroyTexture(&tex);
    }
    SituationDestroyBuffer(&upload_buf);
    sit_test_gpu_context_shutdown();
#endif

    misc_ypq_report_rgb_duplicate_stats(cube_keys, unique_rgb);

    free(cube_keys);
    free(sweep_cache);
    SituationUnloadImage(plane);
}

/**
 * CPU: load harness photo asset and verify SituationImageAdjustYPQ changes pixels.
 */
static void test_ypq_photo_asset_load(void) {
    SituationImage photo = {0};
    if (!misc_ypq_load_harness_photo(&photo, SIT_YPQ_PHOTO_ASSET)) {
        fprintf(stderr,
            "[misc] ypq_photo_asset_load skipped (missing tests/harness/assets/%s)\n",
            SIT_YPQ_PHOTO_ASSET);
        return;
    }

    SIT_ASSERT(SituationIsImageValid(photo));
    SIT_ASSERT(photo.width > 0);
    SIT_ASSERT(photo.height > 0);
    SIT_ASSERT(photo.channels >= 3);

    SituationImage work = {0};
    SituationError err = SituationImageCopy(photo, &work);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    ColorRGBA center_before = misc_ypq_image_center_pixel(&work);
    SituationImageAdjustYPQ(&work, 45.0f, 1.5f, 1.1f, 1.0f);
    ColorRGBA center_after = misc_ypq_image_center_pixel(&work);

    SIT_ASSERT(
        abs((int)center_before.r - (int)center_after.r) > 4
        || abs((int)center_before.g - (int)center_after.g) > 4
        || abs((int)center_before.b - (int)center_after.b) > 4);

    SituationUnloadImage(work);
    SituationUnloadImage(photo);
}

/**
 * Timed visual: prairie.jpg with three ~4s sweeps via SituationImageAdjustYPQ —
 * Y (luma 0.25→1.75), P (phase 0→360°), Q (chroma 0→2×). 256 steps per segment.
 * Set SIT_SKIP_YPQ_PHOTO_SWEEP=1 to skip (~12s wall time).
 */
static void test_ypq_photo_y_p_q_sweep(void) {
    if (getenv("SIT_SKIP_YPQ_PHOTO_SWEEP") != NULL) {
        fprintf(stderr, "[misc] ypq_photo_y_p_q_sweep skipped (SIT_SKIP_YPQ_PHOTO_SWEEP set)\n");
        return;
    }

    SituationImage source = {0};
    if (!misc_ypq_load_harness_photo(&source, SIT_YPQ_PHOTO_ASSET)) {
        fprintf(stderr,
            "[misc] ypq_photo_y_p_q_sweep skipped (missing tests/harness/assets/%s)\n",
            SIT_YPQ_PHOTO_ASSET);
        return;
    }
    misc_ypq_downscale_nearest(&source, SIT_YPQ_PHOTO_SWEEP_DIM);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationError init_err = sit_test_gpu_context_init("SIT_YPQ_PHOTO_SWEEP");
    SIT_ASSERT_EQ((int)init_err, (int)SITUATION_SUCCESS);
#endif

    SituationImage work = {0};
    SituationError err = SituationImageCopy(source, &work);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    work.channels = source.channels > 0 ? source.channels : 4;

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationTexture tex = {0};
    SituationBuffer upload_buf = {0};
    bool tex_ready = false;
    size_t image_bytes = (size_t)work.width * (size_t)work.height * 4u;
    err = SituationCreateBuffer(
        image_bytes,
        NULL,
        SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &upload_buf);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif

    const double step_seconds = SIT_YPQ_Q_SWEEP_SECONDS / (double)SIT_YPQ_Q_STEPS;
    const double total_seconds = SIT_YPQ_Q_SWEEP_SECONDS * (double)SIT_YPQ_PHOTO_SEGMENT_COUNT;
    double t0 = sit_get_time_seconds();
    int total_steps = 0;

    int luma_dark = 0;
    int luma_bright = 0;
    ColorRGBA phase_start = {0, 0, 0, 0};
    ColorRGBA phase_mid = {0, 0, 0, 0};
    bool phase_mid_set = false;
    bool chroma_low_gray = false;
    bool chroma_high_color = false;

    for (int seg = 0; seg < SIT_YPQ_PHOTO_SEGMENT_COUNT; seg++) {
        MiscYpqPhotoSweepAxis axis = (MiscYpqPhotoSweepAxis)seg;
        fprintf(stderr, "[misc] ypq photo sweep segment %d/3: %s\n", seg + 1, misc_ypq_photo_sweep_axis_name(axis));

        for (int step = 0; step < SIT_YPQ_Q_STEPS; step++) {
            const double step_end = t0
                + ((double)seg * (double)SIT_YPQ_Q_STEPS + (double)(step + 1)) * step_seconds;

            misc_ypq_apply_photo_sweep(&work, &source, axis, step, SIT_YPQ_Q_STEPS);

            if (axis == MISC_YPQ_PHOTO_SWEEP_LUMA) {
                ColorRGBA center = misc_ypq_image_center_pixel(&work);
                int luma = misc_ypq_pixel_luma_sum(&center);
                if (step == 0) {
                    luma_dark = luma;
                }
                if (step == SIT_YPQ_Q_STEPS - 1) {
                    luma_bright = luma;
                }
            } else if (axis == MISC_YPQ_PHOTO_SWEEP_PHASE) {
                if (step == 0) {
                    phase_start = misc_ypq_image_center_pixel(&work);
                }
                if (step == SIT_YPQ_Q_STEPS / 2) {
                    phase_mid = misc_ypq_image_center_pixel(&work);
                    phase_mid_set = true;
                }
            } else if (axis == MISC_YPQ_PHOTO_SWEEP_CHROMA) {
                ColorRGBA center = misc_ypq_image_center_pixel(&work);
                if (step == 0) {
                    chroma_low_gray = misc_ypq_pixel_is_gray(&center, 10);
                }
                if (step == SIT_YPQ_Q_STEPS - 1) {
                    chroma_high_color = !misc_ypq_pixel_is_gray(&center, 20);
                }
            }

            total_steps++;

            do {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
                err = misc_ypq_present_image(&work, &tex, &upload_buf, &tex_ready);
                SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif
            } while (sit_get_time_seconds() < step_end);
        }
    }

    double elapsed_total = sit_get_time_seconds() - t0;
    SIT_ASSERT(elapsed_total >= (total_seconds - 0.1));
    SIT_ASSERT(elapsed_total <= (total_seconds * 12.0 + 15.0));
    SIT_ASSERT_EQ(total_steps, SIT_YPQ_Q_STEPS * SIT_YPQ_PHOTO_SEGMENT_COUNT);
    SIT_ASSERT(luma_bright > luma_dark + 16);
    SIT_ASSERT(phase_mid_set);
    SIT_ASSERT(
        abs((int)phase_start.r - (int)phase_mid.r) > 6
        || abs((int)phase_start.g - (int)phase_mid.g) > 6
        || abs((int)phase_start.b - (int)phase_mid.b) > 6);
    SIT_ASSERT(chroma_low_gray);
    SIT_ASSERT(chroma_high_color);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (tex_ready) {
        SituationDestroyTexture(&tex);
    }
    SituationDestroyBuffer(&upload_buf);
    sit_test_gpu_context_shutdown();
#endif

    SituationUnloadImage(work);
    SituationUnloadImage(source);
}

static void test_color_to_vector4(void) {
    ColorRGBA white = {255, 255, 255, 255};
    Vector4 v = {0};
    SituationConvertColorToVector4(white, &v);

    // Normalized: 255/255 = 1.0
    SIT_ASSERT(fabsf(v.r - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.g - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.b - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.a - 1.0f) < 0.01f);
}

static void test_color_to_vector4_half(void) {
    ColorRGBA half = {128, 128, 128, 128};
    Vector4 v = {0};
    SituationConvertColorToVector4(half, &v);

    // 128/255 ≈ 0.502
    SIT_ASSERT(fabsf(v.r - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.g - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.b - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.a - 128.0f/255.0f) < 0.01f);
}

static void test_color_to_vector4_black(void) {
    ColorRGBA black = {0, 0, 0, 0};
    Vector4 v = {0};
    SituationConvertColorToVector4(black, &v);

    SIT_ASSERT(fabsf(v.r) < 0.01f);
    SIT_ASSERT(fabsf(v.g) < 0.01f);
    SIT_ASSERT(fabsf(v.b) < 0.01f);
    SIT_ASSERT(fabsf(v.a) < 0.01f);
}

// ============================================================================
//  Module Descriptor
// ============================================================================

static SitTestCase misc_tests[] = {
    // Image CPU operations
    {"create_image",                test_create_image,                false},
    {"set_pixel_color",             test_set_pixel_color,             false},
    {"gen_image_color",             test_gen_image_color,             false},
    {"image_copy",                  test_image_copy,                  false},
    {"image_crop",                  test_image_crop,                  false},
    {"image_resize",                test_image_resize,                false},
    {"image_flip_vertical",         test_image_flip_vertical,         false},
    {"image_flip_horizontal",       test_image_flip_horizontal,       false},
    {"image_export_and_load",       test_image_export_and_load,       false},
    {"image_load_from_memory",      test_image_load_from_memory,      false},
    {"image_invalid_check",         test_image_invalid_check,         false},
    // Fonts
    {"load_bitmap_font_memory",     test_load_bitmap_font_from_memory, false},
    {"measure_text_bitmap_font",    test_measure_text_bitmap_font,    false},
    // Color conversions
    {"rgb_to_hsv_roundtrip",        test_rgb_to_hsv_roundtrip,        false},
    {"rgb_to_hsv_green",            test_rgb_to_hsv_green,            false},
    {"rgb_to_hsv_gray",             test_rgb_to_hsv_gray,             false},
    {"color_to_ypq_roundtrip",      test_color_to_ypq_roundtrip,     false},
    {"ypq_groundwork_q_zero",       test_ypq_groundwork_q_zero_phase_invariant, false},
    {"ypq_groundwork_luma_axis",    test_ypq_groundwork_grayscale_luma_axis, false},
    {"ypq_groundwork_golden",      test_ypq_groundwork_golden_vectors, false},
    {"ypq_lerp_wrap",              test_ypq_lerp_wrap,              false},
    {"ypq_adjust_luma",            test_ypq_adjust_luma,            false},
    {"ypq_adjust_phase",           test_ypq_adjust_phase,           false},
    {"ypq_adjust_chroma",          test_ypq_adjust_chroma,          false},
    {"ypq_distance_equals",        test_ypq_distance_equals,        false},
    {"ypq_float_roundtrip",        test_ypq_float_roundtrip,        false},
    {"ypq_quantize",               test_ypq_quantize,               false},
    {"image_adjust_ypq",           test_image_adjust_ypq,           false},
    {"ypq_to_rgb_y_p_plane",        test_ypq_to_rgb_y_p_plane,       false},
    {"ypq_to_rgb_q_sweep_4s",       test_ypq_to_rgb_q_sweep_4s,      true},
    {"ypq_photo_asset_load",        test_ypq_photo_asset_load,       false},
    {"ypq_photo_y_p_q_sweep",       test_ypq_photo_y_p_q_sweep,      true},
    {"color_to_vector4",            test_color_to_vector4,            false},
    {"color_to_vector4_half",       test_color_to_vector4_half,       false},
    {"color_to_vector4_black",      test_color_to_vector4_black,      false},
};

const SitTestModule g_module_misc = {
    .name = "misc",
    .setup = NULL,
    .teardown = misc_teardown,
    .tests = misc_tests,
    .test_count = sizeof(misc_tests) / sizeof(misc_tests[0]),
    .requires_context = false
};
