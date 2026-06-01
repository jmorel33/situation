/***************************************************************************************************
 *
 *   situation_impl_ypq.h — Internal YPQ / NTSC YIQ conversion core
 *   (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 *   Single source of truth for YIQ matrix constants and Y↔P↔Q ↔ RGB linear math.
 *   Included only from situation_impl_image.h — not a public header.
 *
 ***************************************************************************************************/
#ifndef SITUATION_IMPL_YPQ_H
#define SITUATION_IMPL_YPQ_H

#include <math.h>

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

#endif /* SITUATION_IMPL_YPQ_H */
