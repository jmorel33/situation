/***************************************************************************************************
 *
 *   situation_impl_ypq.h — YPQ / NTSC YIQ conversion core + diagnostics
 *   (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 *   Single source of truth for YIQ matrix constants, Y↔P↔Q↔RGB linear math,
 *   and public YPQ mapping-quality diagnostics (SituationYpqAnalyzeRgbMapping,
 *   SituationYpqSliceDuplicateCount). Included only from situation_impl_image.h.
 *
 ***************************************************************************************************/
#ifndef SITUATION_IMPL_YPQ_H
#define SITUATION_IMPL_YPQ_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/** Linear Y, I, Q before 8-bit YPQ packing or RGB clamp. */
typedef struct SitYpqYiqLinear {
    double y;
    double i;
    double q;
} SitYpqYiqLinear;

/** NTSC RGB ↔ YIQ coefficients and in-gamut chroma scale (matches legacy Situation YPQ). */
#define SIT_YIQ_NTSC_MAX_I           (0.595715671472)
#define SIT_YIQ_NTSC_MAX_Q           (0.522591049541)
#define SIT_YIQ_NTSC_MAX_Y           (1.0)
#define SIT_YIQ_NTSC_YPQ_BYTE_MAX    (255.0)
#define SIT_YIQ_NTSC_INV_MAX_I       (1.0 / SIT_YIQ_NTSC_MAX_I)
#define SIT_YIQ_NTSC_INV_MAX_Q       (1.0 / SIT_YIQ_NTSC_MAX_Q)
#define SIT_YIQ_NTSC_INV_MAX_Y       (1.0 / SIT_YIQ_NTSC_MAX_Y)
#define SIT_YIQ_NTSC_INV_YPQ_BYTE    (1.0 / SIT_YIQ_NTSC_YPQ_BYTE_MAX)
#define SIT_YIQ_NTSC_RGB_TO_Y_R      (0.299)
#define SIT_YIQ_NTSC_RGB_TO_Y_G      (0.587)
#define SIT_YIQ_NTSC_RGB_TO_Y_B      (0.114)
#define SIT_YIQ_NTSC_RGB_TO_I_R      (0.596)
#define SIT_YIQ_NTSC_RGB_TO_I_G      (-0.274)
#define SIT_YIQ_NTSC_RGB_TO_I_B      (-0.322)
#define SIT_YIQ_NTSC_RGB_TO_Q_R      (0.211)
#define SIT_YIQ_NTSC_RGB_TO_Q_G      (-0.523)
#define SIT_YIQ_NTSC_RGB_TO_Q_B      (0.312)
#define SIT_YIQ_NTSC_YIQ_TO_R_Y      (1.0)
#define SIT_YIQ_NTSC_YIQ_TO_R_I      (0.95568806036115671171)
#define SIT_YIQ_NTSC_YIQ_TO_R_Q      (0.62082467141531188082)
#define SIT_YIQ_NTSC_YIQ_TO_G_Y      (1.0)
#define SIT_YIQ_NTSC_YIQ_TO_G_I      (-0.27178838506206335708)
#define SIT_YIQ_NTSC_YIQ_TO_G_Q      (-0.64860590248778682744)
#define SIT_YIQ_NTSC_YIQ_TO_B_Y      (1.0)
#define SIT_YIQ_NTSC_YIQ_TO_B_I      (-1.1081773266826619523)
#define SIT_YIQ_NTSC_YIQ_TO_B_Q      (1.7025019884020956631)

#define SIT_YIQ_TAU                  (2.0 * M_PI)
#define SIT_YIQ_INV_TAU              (1.0 / SIT_YIQ_TAU)

static inline double _SitYpqMad(double a, double b, double c) {
    return fma(a, b, c);
}

static inline double _SitYpqByteToUnit(unsigned char v) {
    return _SitYpqMad((double)v, SIT_YIQ_NTSC_INV_YPQ_BYTE, 0.0);
}

static inline unsigned char _SitYpqUnitToByte(double unit) {
    if (unit <= 0.0) {
        return 0;
    }
    if (unit >= SIT_YIQ_NTSC_MAX_Y) {
        return (unsigned char)SIT_YIQ_NTSC_YPQ_BYTE_MAX;
    }
    return (unsigned char)_SitYpqMad(unit, SIT_YIQ_NTSC_YPQ_BYTE_MAX, 0.5);
}

static inline double _SitYpqWrapAngleRadians(double angle) {
    if (angle < 0.0) {
        angle = _SitYpqMad(angle, 1.0, SIT_YIQ_TAU);
    }
    if (angle >= SIT_YIQ_TAU) {
        angle = _SitYpqMad(angle, 1.0, -SIT_YIQ_TAU);
    }
    return angle;
}

static inline double _SitYpqNorm2Amplitude(double i_norm, double q_norm) {
    return sqrt(_SitYpqMad(i_norm, i_norm, q_norm * q_norm));
}

static inline double _SitYpqPhaseByteToRadians(unsigned char p) {
    return _SitYpqMad(_SitYpqByteToUnit(p), SIT_YIQ_TAU, 0.0);
}

static inline unsigned char _SitYpqPhaseRadiansToByte(double angle) {
    angle = _SitYpqWrapAngleRadians(angle);
    return _SitYpqUnitToByte(_SitYpqMad(angle, SIT_YIQ_INV_TAU, 0.0));
}

/** c0*x0 + c1*x1 + c2*x2 with fused multiply-add where available. */
static inline double _SitYpqDot3(
    double c0, double c1, double c2,
    double x0, double x1, double x2)
{
    return fma(c1, x1, fma(c0, x0, c2 * x2));
}

static inline void _SitYiqFromRgbLinear(double r, double g, double b, SitYpqYiqLinear* out) {
    if (!out) {
        return;
    }
    out->y = _SitYpqDot3(SIT_YIQ_NTSC_RGB_TO_Y_R, SIT_YIQ_NTSC_RGB_TO_Y_G, SIT_YIQ_NTSC_RGB_TO_Y_B, r, g, b);
    out->i = _SitYpqDot3(SIT_YIQ_NTSC_RGB_TO_I_R, SIT_YIQ_NTSC_RGB_TO_I_G, SIT_YIQ_NTSC_RGB_TO_I_B, r, g, b);
    out->q = _SitYpqDot3(SIT_YIQ_NTSC_RGB_TO_Q_R, SIT_YIQ_NTSC_RGB_TO_Q_G, SIT_YIQ_NTSC_RGB_TO_Q_B, r, g, b);
}

static inline void _SitRgbLinearFromYiq(const SitYpqYiqLinear* yiq, double* out_r, double* out_g, double* out_b) {
    if (!yiq || !out_r || !out_g || !out_b) {
        return;
    }
    *out_r = _SitYpqDot3(SIT_YIQ_NTSC_YIQ_TO_R_Y, SIT_YIQ_NTSC_YIQ_TO_R_I, SIT_YIQ_NTSC_YIQ_TO_R_Q, yiq->y, yiq->i, yiq->q);
    *out_g = _SitYpqDot3(SIT_YIQ_NTSC_YIQ_TO_G_Y, SIT_YIQ_NTSC_YIQ_TO_G_I, SIT_YIQ_NTSC_YIQ_TO_G_Q, yiq->y, yiq->i, yiq->q);
    *out_b = _SitYpqDot3(SIT_YIQ_NTSC_YIQ_TO_B_Y, SIT_YIQ_NTSC_YIQ_TO_B_I, SIT_YIQ_NTSC_YIQ_TO_B_Q, yiq->y, yiq->i, yiq->q);
}

static inline double _SitClampUnitLinear(double v) {
    if (v <= 0.0) {
        return 0.0;
    }
    if (v >= 1.0) {
        return 1.0;
    }
    return v;
}

static inline void _SitRgbLinearFromYiqClamped(const SitYpqYiqLinear* yiq, double* out_r, double* out_g, double* out_b) {
    _SitRgbLinearFromYiq(yiq, out_r, out_g, out_b);
    *out_r = _SitClampUnitLinear(*out_r);
    *out_g = _SitClampUnitLinear(*out_g);
    *out_b = _SitClampUnitLinear(*out_b);
}

static inline void _SitYiqFromYpqBytes(ColorYPQA ypq, SitYpqYiqLinear* out) {
    if (!out) {
        return;
    }
    double angle = _SitYpqPhaseByteToRadians(ypq.p);
    double amplitude = _SitYpqByteToUnit(ypq.q);
    out->y = _SitYpqByteToUnit(ypq.y);
    out->i = _SitYpqMad(amplitude, _SitYpqMad(cos(angle), SIT_YIQ_NTSC_MAX_I, 0.0), 0.0);
    out->q = _SitYpqMad(amplitude, _SitYpqMad(sin(angle), SIT_YIQ_NTSC_MAX_Q, 0.0), 0.0);
}

static inline ColorYPQA _SitYpqBytesFromYiqLinear(const SitYpqYiqLinear* yiq, unsigned char alpha) {
    ColorYPQA result = {0, 0, 0, alpha};
    if (!yiq) {
        return result;
    }

    double i_norm = _SitYpqMad(yiq->i, SIT_YIQ_NTSC_INV_MAX_I, 0.0);
    double q_norm = _SitYpqMad(yiq->q, SIT_YIQ_NTSC_INV_MAX_Q, 0.0);
    double amplitude = _SitYpqNorm2Amplitude(i_norm, q_norm);
    if (amplitude > 1.0) {
        amplitude = 1.0;
    }

    double angle = _SitYpqWrapAngleRadians(atan2(q_norm, i_norm));

    result.y = _SitYpqUnitToByte(yiq->y);
    result.p = _SitYpqPhaseRadiansToByte(angle);
    result.q = _SitYpqUnitToByte(amplitude);
    return result;
}

static inline ColorRGBA _SitRgbFromYpqBytes(ColorYPQA ypq) {
    SitYpqYiqLinear yiq;
    double r_lin = 0.0;
    double g_lin = 0.0;
    double b_lin = 0.0;
    ColorRGBA result = {0, 0, 0, ypq.a};

    _SitYiqFromYpqBytes(ypq, &yiq);
    _SitRgbLinearFromYiqClamped(&yiq, &r_lin, &g_lin, &b_lin);
    result.r = _SitYpqUnitToByte(r_lin);
    result.g = _SitYpqUnitToByte(g_lin);
    result.b = _SitYpqUnitToByte(b_lin);
    return result;
}

static inline ColorYPQA _SitYpqBytesFromRgb(ColorRGBA color) {
    SitYpqYiqLinear yiq;
    _SitYiqFromRgbLinear(
        _SitYpqByteToUnit(color.r),
        _SitYpqByteToUnit(color.g),
        _SitYpqByteToUnit(color.b),
        &yiq);
    return _SitYpqBytesFromYiqLinear(&yiq, color.a);
}

static inline float _SitYpqClampUnitFloat(float v) {
    if (v <= 0.0f) {
        return 0.0f;
    }
    if (v >= 1.0f) {
        return 1.0f;
    }
    return v;
}

static inline void _SitYiqFromYpqFloat(ColorYPQf ypq, SitYpqYiqLinear* out) {
    if (!out) {
        return;
    }
    double angle = _SitYpqMad((double)_SitYpqClampUnitFloat(ypq.p), SIT_YIQ_TAU, 0.0);
    double amplitude = (double)_SitYpqClampUnitFloat(ypq.q);
    out->y = (double)_SitYpqClampUnitFloat(ypq.y);
    out->i = _SitYpqMad(amplitude, _SitYpqMad(cos(angle), SIT_YIQ_NTSC_MAX_I, 0.0), 0.0);
    out->q = _SitYpqMad(amplitude, _SitYpqMad(sin(angle), SIT_YIQ_NTSC_MAX_Q, 0.0), 0.0);
}

static inline ColorYPQf _SitYpqFloatFromYiqLinear(const SitYpqYiqLinear* yiq, float alpha) {
    ColorYPQf result = {0.0f, 0.0f, 0.0f, _SitYpqClampUnitFloat(alpha)};
    if (!yiq) {
        return result;
    }

    double i_norm = _SitYpqMad(yiq->i, SIT_YIQ_NTSC_INV_MAX_I, 0.0);
    double q_norm = _SitYpqMad(yiq->q, SIT_YIQ_NTSC_INV_MAX_Q, 0.0);
    double amplitude = _SitYpqNorm2Amplitude(i_norm, q_norm);
    if (amplitude > 1.0) {
        amplitude = 1.0;
    }

    double angle = _SitYpqWrapAngleRadians(atan2(q_norm, i_norm));

    result.y = (float)_SitClampUnitLinear(yiq->y);
    result.p = (float)_SitYpqMad(angle, SIT_YIQ_INV_TAU, 0.0);
    result.q = (float)amplitude;
    return result;
}

static inline ColorYPQf _SitYpqFloatFromRgb(ColorRGBA color) {
    SitYpqYiqLinear yiq;
    _SitYiqFromRgbLinear(
        _SitYpqByteToUnit(color.r),
        _SitYpqByteToUnit(color.g),
        _SitYpqByteToUnit(color.b),
        &yiq);
    return _SitYpqFloatFromYiqLinear(&yiq, _SitYpqByteToUnit(color.a));
}

static inline bool _SitYpqFloatRgbLinearInGamut(ColorYPQf ypq, double* out_r, double* out_g, double* out_b) {
    SitYpqYiqLinear yiq;
    _SitYiqFromYpqFloat(ypq, &yiq);
    _SitRgbLinearFromYiq(&yiq, out_r, out_g, out_b);
    return *out_r >= 0.0 && *out_r <= 1.0
        && *out_g >= 0.0 && *out_g <= 1.0
        && *out_b >= 0.0 && *out_b <= 1.0;
}

static inline ColorRGBA _SitRgbFromYpqFloat(ColorYPQf ypq) {
    SitYpqYiqLinear yiq;
    double r_lin = 0.0;
    double g_lin = 0.0;
    double b_lin = 0.0;
    ColorRGBA result = {0, 0, 0, _SitYpqUnitToByte((double)_SitYpqClampUnitFloat(ypq.a))};

    _SitYiqFromYpqFloat(ypq, &yiq);
    _SitRgbLinearFromYiqClamped(&yiq, &r_lin, &g_lin, &b_lin);
    result.r = _SitYpqUnitToByte(r_lin);
    result.g = _SitYpqUnitToByte(g_lin);
    result.b = _SitYpqUnitToByte(b_lin);
    return result;
}

static inline ColorYPQA _SitYpqBytesFromFloat(ColorYPQf ypq) {
    ColorYPQA result;
    result.y = _SitYpqUnitToByte((double)_SitYpqClampUnitFloat(ypq.y));
    result.p = _SitYpqPhaseRadiansToByte(_SitYpqMad((double)_SitYpqClampUnitFloat(ypq.p), SIT_YIQ_TAU, 0.0));
    result.q = _SitYpqUnitToByte((double)_SitYpqClampUnitFloat(ypq.q));
    result.a = _SitYpqUnitToByte((double)_SitYpqClampUnitFloat(ypq.a));
    return result;
}

/**
 * @brief Full 256³ YPQ→RGB mapping quality analysis.
 * @details Iterates every (Y,P,Q) byte triple (16 777 216 total) and counts
 *          unique 8-bit RGB outputs, duplicate mappings, unreachable RGB triples
 *          (holes), and the worst fixed-Q slice duplicate count.
 *
 * This is an O(n³) scan — expect a few seconds on a modern CPU. Use the
 * SIT_SKIP_YPQ_RGB_STATS environment variable to guard against it in CI.
 *
 * @param[out] out  Pointer to a SituationYpqRgbMappingStats struct to fill.
 *                  Must not be NULL.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_INVALID_PARAM if out is NULL.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION if the 16 MB hit bitmap cannot be allocated.
 */
SITAPI SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out) {
    if (!out) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(out, 0, sizeof(*out));

    /* 16 MB flat bitmap: hit[key]=1 once we have seen RGB key=(r<<16)|(g<<8)|b */
    uint8_t* hit = (uint8_t*)SIT_CALLOC(1 << 24, 1);
    if (!hit) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    int64_t unique_rgb = 0;

    /* Pass 1: global unique count */
    for (int y = 0; y < 256; y++) {
        for (int p = 0; p < 256; p++) {
            for (int q = 0; q < 256; q++) {
                ColorRGBA rgb = _SitRgbFromYpqBytes(
                    (ColorYPQA){(unsigned char)y, (unsigned char)p, (unsigned char)q, 255});
                uint32_t key = ((uint32_t)rgb.r << 16)
                             | ((uint32_t)rgb.g << 8)
                             | (uint32_t)rgb.b;
                if (!hit[key]) {
                    hit[key] = 1;
                    unique_rgb++;
                }
            }
        }
    }

    out->ypq_mappings       = 256LL * 256LL * 256LL;
    out->unique_rgb         = unique_rgb;
    out->duplicate_mappings = out->ypq_mappings - unique_rgb;
    out->rgb_holes          = (1LL << 24) - unique_rgb;

    /* Pass 2: worst fixed-Q slice duplicate count.
     * Reuse hit bitmap, resetting per slice.
     * For each Q slice (65536 entries), count unique RGB — duplicates = 65536 - unique. */
    int worst_dup = 0;
    int worst_at  = 0;

    for (int q = 0; q < 256; q++) {
        memset(hit, 0, (size_t)(1 << 24));
        int slice_unique = 0;

        for (int y = 0; y < 256; y++) {
            for (int p = 0; p < 256; p++) {
                ColorRGBA rgb = _SitRgbFromYpqBytes(
                    (ColorYPQA){(unsigned char)y, (unsigned char)p, (unsigned char)q, 255});
                uint32_t key = ((uint32_t)rgb.r << 16)
                             | ((uint32_t)rgb.g << 8)
                             | (uint32_t)rgb.b;
                if (!hit[key]) {
                    hit[key] = 1;
                    slice_unique++;
                }
            }
        }

        int slice_dup = 65536 - slice_unique;
        if (slice_dup > worst_dup) {
            worst_dup = slice_dup;
            worst_at  = q;
        }
    }

    SIT_FREE(hit);

    out->worst_axis_dup = worst_dup;
    out->worst_axis_at  = worst_at;
    return SITUATION_SUCCESS;
}

/**
 * @brief Count duplicate RGB outputs in one 65 536-entry fixed-axis YPQ slice.
 * @details Iterates all 256×256 (Y,P,Q) combinations with the named axis held
 *          at `value`, radix-sorts the resulting RGB keys, and counts adjacent
 *          duplicates.
 *
 * Notable results:
 *   axis='Q', value=0  →  all 65 536 entries map to gray  →  ≥65 000 duplicates
 *
 * @param axis     Which axis to hold fixed: 'Y', 'P', or 'Q'.
 * @param value    The fixed byte value [0, 255].
 * @param[out] out_dup  Receives the duplicate count on SITUATION_SUCCESS.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_INVALID_PARAM if axis is invalid, value is out of range, or out_dup is NULL.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION if temporary sort buffers cannot be allocated.
 */
SITAPI SituationError SituationYpqSliceDuplicateCount(char axis, int value, int* out_dup) {
    if ((axis != 'Y' && axis != 'P' && axis != 'Q')
            || value < 0 || value > 255
            || !out_dup) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    uint32_t* keys = (uint32_t*)SIT_MALLOC(65536u * sizeof(uint32_t));
    uint32_t* temp = (uint32_t*)SIT_MALLOC(65536u * sizeof(uint32_t));
    if (!keys || !temp) {
        SIT_FREE(keys);
        SIT_FREE(temp);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    /* Fill the 65 536 RGB keys for this slice */
    int idx = 0;
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            unsigned char cy, cp, cq;
            if (axis == 'Y') {
                cy = (unsigned char)value;
                cp = (unsigned char)a;
                cq = (unsigned char)b;
            } else if (axis == 'P') {
                cy = (unsigned char)a;
                cp = (unsigned char)value;
                cq = (unsigned char)b;
            } else { /* axis == 'Q' */
                cy = (unsigned char)a;
                cp = (unsigned char)b;
                cq = (unsigned char)value;
            }

            ColorRGBA rgb = _SitRgbFromYpqBytes((ColorYPQA){cy, cp, cq, 255});
            keys[idx++] = ((uint32_t)rgb.r << 16) | ((uint32_t)rgb.g << 8) | (uint32_t)rgb.b;
        }
    }

    /* 4-pass byte radix sort (LSB first) */
    uint32_t count[256];
    uint32_t* src = keys;
    uint32_t* dst = temp;
    for (int pass = 0; pass < 4; pass++) {
        memset(count, 0, sizeof(count));
        const int shift = pass * 8;
        for (int i = 0; i < 65536; i++) {
            count[(src[i] >> shift) & 0xffu]++;
        }
        int pos = 0;
        for (int bi = 0; bi < 256; bi++) {
            const int c = count[bi];
            count[bi] = pos;
            pos += c;
        }
        for (int i = 0; i < 65536; i++) {
            const int bi = (src[i] >> shift) & 0xffu;
            dst[count[bi]++] = src[i];
        }
        uint32_t* swap = src;
        src = dst;
        dst = swap;
    }
    if (src != keys) {
        memcpy(keys, src, 65536u * sizeof(uint32_t));
    }

    /* Count adjacent duplicates in the sorted array */
    int dup = 0;
    for (int i = 1; i < 65536; i++) {
        if (keys[i] == keys[i - 1]) {
            dup++;
        }
    }

    SIT_FREE(temp);
    SIT_FREE(keys);

    *out_dup = dup;
    return SITUATION_SUCCESS;
}

#endif /* SITUATION_IMPL_YPQ_H */
