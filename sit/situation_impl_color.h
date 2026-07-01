/***************************************************************************************************
 *
 *   situation_impl_color.h — YPQ / HSV / PQ / 10-bit color-space core + public pixel APIs
 *   (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 *   Single source of truth for YIQ matrix constants, Y↔P↔Q↔RGB linear math,
 *   HSV conversion, ST.2084 PQ, 10-bit packing, sRGB linearization,
 *   and YPQ mapping-quality diagnostics (SituationYpqAnalyzeRgbMapping,
 *   SituationYpqSliceDuplicateCount). Included from situation_impl.h (orchestrator)
 *   and at the top of situation_impl_image.h (primary consumer).
 *
 ***************************************************************************************************/
#ifndef SITUATION_IMPL_COLOR_H
#define SITUATION_IMPL_COLOR_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
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
#define SIT_YIQ_NTSC_YPQ_10BIT_MAX   (1023.0)
#define SIT_YIQ_NTSC_INV_MAX_I       (1.0 / SIT_YIQ_NTSC_MAX_I)
#define SIT_YIQ_NTSC_INV_MAX_Q       (1.0 / SIT_YIQ_NTSC_MAX_Q)
#define SIT_YIQ_NTSC_INV_MAX_Y       (1.0 / SIT_YIQ_NTSC_MAX_Y)
#define SIT_YIQ_NTSC_INV_YPQ_BYTE    (1.0 / SIT_YIQ_NTSC_YPQ_BYTE_MAX)
#define SIT_YIQ_NTSC_INV_YPQ_10BIT   (1.0 / SIT_YIQ_NTSC_YPQ_10BIT_MAX)
#define SIT_YIQ_NTSC_ALPHA2_MAX      (3.0)
#define SIT_YIQ_NTSC_INV_ALPHA2      (1.0 / SIT_YIQ_NTSC_ALPHA2_MAX)
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

static inline uint16_t _SitYpqUnitTo10Bit(double unit) {
    if (unit <= 0.0) {
        return 0;
    }
    if (unit >= SIT_YIQ_NTSC_MAX_Y) {
        return (uint16_t)SIT_YIQ_NTSC_YPQ_10BIT_MAX;
    }
    return (uint16_t)_SitYpqMad(unit, SIT_YIQ_NTSC_YPQ_10BIT_MAX, 0.5);
}

static inline double _SitYpq10BitToUnit(uint16_t v) {
    if (v >= (uint16_t)SIT_YIQ_NTSC_YPQ_10BIT_MAX) {
        return SIT_YIQ_NTSC_MAX_Y;
    }
    return _SitYpqMad((double)v, SIT_YIQ_NTSC_INV_YPQ_10BIT, 0.0);
}

/** Quantize one 10-bit RGB channel to 8-bit (FMA via unit linear light). */
static inline uint8_t _SitRgb10ChannelTo8(uint32_t v10) {
    return (uint8_t)_SitYpqUnitToByte(_SitYpq10BitToUnit((uint16_t)v10));
}

/** Expand A2R10G10B10 2-bit alpha to 8-bit (FMA). */
static inline uint8_t _SitAlpha2ChannelTo8(uint32_t a2) {
    if (a2 >= 3u) {
        return (uint8_t)SIT_YIQ_NTSC_YPQ_BYTE_MAX;
    }
    return (uint8_t)_SitYpqUnitToByte(_SitYpqMad((double)a2, SIT_YIQ_NTSC_INV_ALPHA2, 0.0));
}

/** Upscale one 8-bit channel to 10-bit (FMA). */
static inline uint16_t _SitRgb8ChannelTo10(unsigned char v) {
    return (uint16_t)_SitYpqUnitTo10Bit(_SitYpqByteToUnit(v));
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

static inline ColorRGBA10 _SitRgb10FromYpqFloat(ColorYPQf ypq) {
    SitYpqYiqLinear yiq;
    double r_lin = 0.0;
    double g_lin = 0.0;
    double b_lin = 0.0;
    ColorRGBA10 result = {0, 0, 0, _SitYpqUnitTo10Bit((double)_SitYpqClampUnitFloat(ypq.a))};

    _SitYiqFromYpqFloat(ypq, &yiq);
    _SitRgbLinearFromYiqClamped(&yiq, &r_lin, &g_lin, &b_lin);
    result.r = _SitYpqUnitTo10Bit(r_lin);
    result.g = _SitYpqUnitTo10Bit(g_lin);
    result.b = _SitYpqUnitTo10Bit(b_lin);
    return result;
}

static inline uint32_t _SitRgb10PackedFromRgba10(ColorRGBA10 color) {
    uint32_t a2 = ((uint32_t)color.a >> 8) & 0x3u;
    return (a2 << 30)
         | (((uint32_t)color.r & 0x3FFu) << 20)
         | (((uint32_t)color.g & 0x3FFu) << 10)
         | ((uint32_t)color.b & 0x3FFu);
}

/* --- ST.2084 PQ (BT.2100) for HDR10 swapchain output (Phase 7) --- */
#define SIT_ST2084_M1              (2610.0 / 4096.0)
#define SIT_ST2084_M2              (2523.0 / 4096.0)
#define SIT_ST2084_C1              (3424.0 / 4096.0)
#define SIT_ST2084_C2              (2413.0 / 4096.0)
#define SIT_ST2084_C3              (2392.0 / 4096.0)
#define SIT_ST2084_INV_M1          (4096.0 / 2610.0)
#define SIT_ST2084_INV_M2          (4096.0 / 2523.0)

/** sRGB unit [0,1] → linear light (display-referred). Single CPU EOTF for the color module. */
static inline float _SitSrgbUnitToLinear(float s) {
    if (s <= 0.04045f) {
        return s / 12.92f;
    }
    return powf((s + 0.055f) / 1.055f, 2.4f);
}

/** 8-bit sRGB channel → linear light [0,1] (display-referred). */
static inline double _SitYpqSrgbByteToLinear(unsigned char v) {
    return (double)_SitSrgbUnitToLinear((float)v / 255.0f);
}

/** Linear display light [0,1] (10000 nits = 1.0) → ST.2084 PQ signal [0,1]. */
static inline double _SitSt2084LinearToPq(double linear) {
    if (linear <= 0.0) {
        return 0.0;
    }
    const double lp = pow(linear, SIT_ST2084_M1);
    const double num = SIT_ST2084_C1 + SIT_ST2084_C2 * lp;
    const double den = 1.0 + SIT_ST2084_C3 * lp;
    return pow(num / den, SIT_ST2084_M2);
}

/** ST.2084 PQ signal [0,1] → linear display light [0,1]. */
static inline double _SitSt2084PqToLinear(double pq) {
    if (pq <= 0.0) {
        return 0.0;
    }
    const double np = pow(pq, SIT_ST2084_INV_M2);
    const double numer = fmax(0.0, np - SIT_ST2084_C1);
    const double denom = fmax(1e-12, SIT_ST2084_C2 - SIT_ST2084_C3 * np);
    return pow(numer / denom, SIT_ST2084_INV_M1);
}

/** User ColorRGBA (sRGB 0–255) → VkClearColorValue floats; PQ-encoded when HDR swapchain active. */
static inline void _SituationColorRgbaToClearFloats(ColorRGBA color, bool hdr_output_active, float out_rgba[4]) {
    if (hdr_output_active) {
        out_rgba[0] = (float)_SitSt2084LinearToPq(_SitYpqSrgbByteToLinear(color.r));
        out_rgba[1] = (float)_SitSt2084LinearToPq(_SitYpqSrgbByteToLinear(color.g));
        out_rgba[2] = (float)_SitSt2084LinearToPq(_SitYpqSrgbByteToLinear(color.b));
        out_rgba[3] = (float)_SitYpqByteToUnit(color.a);
    } else {
        out_rgba[0] = color.r / 255.0f;
        out_rgba[1] = color.g / 255.0f;
        out_rgba[2] = color.b / 255.0f;
        out_rgba[3] = color.a / 255.0f;
    }
}

static inline ColorRGBA10 _SitRgb10FromPqSignal(double pq_r, double pq_g, double pq_b, double alpha_unit) {
    ColorRGBA10 result;
    result.r = _SitYpqUnitTo10Bit(pq_r);
    result.g = _SitYpqUnitTo10Bit(pq_g);
    result.b = _SitYpqUnitTo10Bit(pq_b);
    result.a = _SitYpqUnitTo10Bit(alpha_unit);
    return result;
}

/** Float YPQ → linear RGB → PQ → A2R10G10B10 (HDR10 swapchain texel). */
static inline uint32_t _SitRgb10PackedHdrFromYpqFloat(ColorYPQf ypq) {
    SitYpqYiqLinear yiq;
    double r_lin = 0.0;
    double g_lin = 0.0;
    double b_lin = 0.0;
    _SitYiqFromYpqFloat(ypq, &yiq);
    _SitRgbLinearFromYiqClamped(&yiq, &r_lin, &g_lin, &b_lin);
    return _SitRgb10PackedFromRgba10(_SitRgb10FromPqSignal(
        _SitSt2084LinearToPq(r_lin),
        _SitSt2084LinearToPq(g_lin),
        _SitSt2084LinearToPq(b_lin),
        (double)_SitYpqClampUnitFloat(ypq.a)));
}

/** PQ signal [0,1] on all RGB channels → A2R10G10B10 (reference patch / tests). */
static inline uint32_t _SitRgb10PackedFromPqGray(float pq_level) {
    const double pq = (double)_SitYpqClampUnitFloat(pq_level);
    return _SitRgb10PackedFromRgba10(_SitRgb10FromPqSignal(pq, pq, pq, 1.0));
}

static inline ColorYPQf _SitYpqFloatFromRgb10(ColorRGBA10 color) {
    SitYpqYiqLinear yiq;
    _SitYiqFromRgbLinear(
        _SitYpq10BitToUnit(color.r),
        _SitYpq10BitToUnit(color.g),
        _SitYpq10BitToUnit(color.b),
        &yiq);
    return _SitYpqFloatFromYiqLinear(&yiq, (float)_SitYpq10BitToUnit(color.a));
}

static inline ColorRGBA10 _SitRgb10FromRgba(ColorRGBA color) {
    return (ColorRGBA10){
        _SitRgb8ChannelTo10(color.r),
        _SitRgb8ChannelTo10(color.g),
        _SitRgb8ChannelTo10(color.b),
        _SitRgb8ChannelTo10(color.a),
    };
}

static inline ColorRGBA _SitRgbaFromRgb10(ColorRGBA10 color) {
    return (ColorRGBA){
        _SitRgb10ChannelTo8(color.r),
        _SitRgb10ChannelTo8(color.g),
        _SitRgb10ChannelTo8(color.b),
        _SitRgb10ChannelTo8(color.a),
    };
}

/** Unpack one A2R10G10B10 texel (Vulkan swapchain layout) into RGBA8. */
static inline void _SitUnpackA2R10G10B10ToRgba8(uint8_t dst_rgba[4], uint32_t px) {
    dst_rgba[0] = _SitRgb10ChannelTo8((px >> 20) & 0x3FFu);
    dst_rgba[1] = _SitRgb10ChannelTo8((px >> 10) & 0x3FFu);
    dst_rgba[2] = _SitRgb10ChannelTo8(px & 0x3FFu);
    dst_rgba[3] = _SitAlpha2ChannelTo8((px >> 30) & 0x3u);
}

static inline ColorRGBA _SitRgbaFromRgb10Packed(uint32_t px) {
    ColorRGBA out = {0, 0, 0, 255};
    uint8_t rgba[4];
    _SitUnpackA2R10G10B10ToRgba8(rgba, px);
    out.r = rgba[0];
    out.g = rgba[1];
    out.b = rgba[2];
    out.a = rgba[3];
    return out;
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


// --- HSV pixel APIs ---

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


// --- Public YPQ / PQ / 10-bit pixel APIs ---

/**
 * @brief Converts a color from the YPQA (Luma, Phase, Quadrature, Alpha) color space back to the standard RGBA color space.
 * @details This is the inverse operation of `SituationColorToYPQ`. It reconstructs the red, green, and blue components from the color's brightness (Y) and its chroma information (P and Q), and preserves the alpha channel. The conversion uses the standard NTSC YIQ-to-RGB matrix via `_SitRgbFromYpqBytes` in `situation_impl_color.h`.
 *
 * @param ypq_color The source `ColorYPQA` struct to convert.
 *
 * @return A `ColorRGBA` struct containing the equivalent R, G, B, and A values. The function includes clamping to ensure the resulting RGB values are within the valid [0-255] range, as certain YPQ combinations can represent out-of-gamut colors.
 *
 * @see SituationColorToYPQ(), situation_impl_color.h
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

SITAPI ColorRGBA10 SituationYpqToRgba10(ColorYPQf ypq) {
    return _SitRgb10FromYpqFloat(ypq);
}

SITAPI uint32_t SituationYpqToRgb10Packed(ColorYPQf ypq) {
    return _SitRgb10PackedFromRgba10(_SitRgb10FromYpqFloat(ypq));
}

SITAPI uint32_t SituationYpqToRgb10PackedHdr(ColorYPQf ypq) {
    return _SitRgb10PackedHdrFromYpqFloat(ypq);
}

SITAPI float SituationLinearToPq(float linear) {
    if (linear <= 0.0f) {
        return 0.0f;
    }
    return (float)_SitSt2084LinearToPq((double)linear);
}

SITAPI float SituationPqToLinear(float pq) {
    if (pq <= 0.0f) {
        return 0.0f;
    }
    return (float)_SitSt2084PqToLinear((double)_SitYpqClampUnitFloat(pq));
}

SITAPI uint32_t SituationPqGrayToRgb10Packed(float pq_level) {
    return _SitRgb10PackedFromPqGray(pq_level);
}

SITAPI ColorRGBA SituationColorRgbaToHdrPqClear(ColorRGBA srgb) {
    float pq[4];
    _SituationColorRgbaToClearFloats(srgb, true, pq);
    return (ColorRGBA){
        (unsigned char)(pq[0] * 255.0f + 0.5f),
        (unsigned char)(pq[1] * 255.0f + 0.5f),
        (unsigned char)(pq[2] * 255.0f + 0.5f),
        (unsigned char)(pq[3] * 255.0f + 0.5f),
    };
}

SITAPI ColorYPQf SituationRgbToYpqFrom10(ColorRGBA10 color) {
    return _SitYpqFloatFromRgb10(color);
}

SITAPI ColorRGBA10 SituationRgb10FromRgba(ColorRGBA color) {
    return _SitRgb10FromRgba(color);
}

SITAPI ColorRGBA SituationRgbaFromRgb10(ColorRGBA10 color) {
    return _SitRgbaFromRgb10(color);
}

SITAPI ColorRGBA SituationRgbaFromRgb10Packed(uint32_t packed) {
    return _SitRgbaFromRgb10Packed(packed);
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



// --- Normalized color utility ---

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
#endif /* SITUATION_IMPL_COLOR_H */
