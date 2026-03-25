#ifndef DSP_MATH_H
#define DSP_MATH_H

#include <stdint.h>
#include <math.h>

#if defined(PX_USE_SSE41) && defined(__SSE4_1__)
#include <smmintrin.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FAST_TRIG_TABLE_BITS   15
#define FAST_TRIG_TABLE_SIZE   (1u << FAST_TRIG_TABLE_BITS)   // 32768 — optimal cache
#define FAST_TRIG_TABLE_MASK   (FAST_TRIG_TABLE_SIZE - 1)

extern int16_t sin_table[FAST_TRIG_TABLE_SIZE];
extern int16_t cos_table[FAST_TRIG_TABLE_SIZE];
extern int16_t tan_table[FAST_TRIG_TABLE_SIZE];
extern int16_t asin_table[FAST_TRIG_TABLE_SIZE];
extern int16_t acos_table[FAST_TRIG_TABLE_SIZE];
extern int16_t atan_table[FAST_TRIG_TABLE_SIZE];

extern void InitFastDSP(void);
extern void FreeFastDSP(void);

// ===================================================================
// FAST SCALAR TRIG — using FMA for speed & precision
static inline float fastsin(float x)
{
    const float two_pi = 6.283185307179586f;
    int k = (int)(x * 0.15915494309189535f);
    float wrapped = x - (float)k * two_pi;
    if (wrapped < 0.0f) wrapped += two_pi;

    int quadrant = (int)(wrapped * 0.6366197723675813f);
    float phase = wrapped - quadrant * 1.5707963267948966f;

    if (quadrant == 1 || quadrant == 3)
        phase = 1.5707963267948966f - phase;

    float t = phase * (FAST_TRIG_TABLE_SIZE - 1);
    uint32_t idx = (uint32_t)t;
    float frac = t - idx;

    int16_t a = sin_table[idx & FAST_TRIG_TABLE_MASK];
    int16_t b = sin_table[(idx + 1) & FAST_TRIG_TABLE_MASK];

    float val = fmaf(frac, (b - a), a) * 0.00003051850947599719f;

    if (quadrant >= 2) val = -val;
    if (x < 0.0f) val = -val;

    return val;
}

static inline float fastcos(float x) { return fastsin(x + 1.5707963267948966f); }

static inline float fasttan(float x)
{
    const float pi = 3.141592653589793f;
    int k = (int)(x * 0.3183098861837907f);
    float wrapped = x - (float)k * pi;
    if (wrapped < 0.0f) wrapped += pi;

    float phase_pi2 = wrapped * 0.6366197723675813f;
    int quadrant = (int)phase_pi2;
    float phase = phase_pi2 - (float)quadrant;

    if (phase > 0.999f) phase = 0.999f;

    float t = phase * (FAST_TRIG_TABLE_SIZE - 1);
    uint32_t idx = (uint32_t)t;
    float frac = t - idx;

    int16_t a = tan_table[idx & FAST_TRIG_TABLE_MASK];
    int16_t b = tan_table[(idx + 1) & FAST_TRIG_TABLE_MASK];

    float val = fmaf(frac, (b - a), a) * 0.003051850947599719f;
    return (quadrant & 1) ? -val : val;
}

static inline float fastasin(float x) {
    int is_neg = (x < 0.0f);
    float ax = is_neg ? -x : x;
    if (ax > 1.0f) ax = 1.0f;

    float t = ax * (FAST_TRIG_TABLE_SIZE - 1);
    uint32_t idx = (uint32_t)t;
    float frac = t - idx;

    int16_t a = asin_table[idx & FAST_TRIG_TABLE_MASK];
    int16_t b = asin_table[(idx + 1) & FAST_TRIG_TABLE_MASK];

    float val = fmaf(frac, (b - a), a) * 0.00004793689f;
    return is_neg ? -val : val;
}

static inline float fastacos(float x) {
    int is_neg = (x < 0.0f);
    float ax = is_neg ? -x : x;
    if (ax > 1.0f) ax = 1.0f;

    float t = ax * (FAST_TRIG_TABLE_SIZE - 1);
    uint32_t idx = (uint32_t)t;
    float frac = t - idx;

    int16_t a = acos_table[idx & FAST_TRIG_TABLE_MASK];
    int16_t b = acos_table[(idx + 1) & FAST_TRIG_TABLE_MASK];

    float val = fmaf(frac, (b - a), a) * 0.00004793689f;
    return is_neg ? (M_PI - val) : val;
}

static inline float fastatan(float x) {
    int is_neg = (x < 0.0f);
    float ax = is_neg ? -x : x;
    float mapped = ax / (1.0f + ax);

    float t = mapped * (FAST_TRIG_TABLE_SIZE - 1);
    uint32_t idx = (uint32_t)t;
    float frac = t - idx;

    int16_t a = atan_table[idx & FAST_TRIG_TABLE_MASK];
    int16_t b = atan_table[(idx + 1) & FAST_TRIG_TABLE_MASK];

    float val = fmaf(frac, (b - a), a) * 0.00004793689f;
    return is_neg ? -val : val;
}

// ===================================================================
// SSE4.1 PATH — using FMA
#if defined(PX_USE_SSE41) && defined(__SSE4_1__)
static inline __m128 fastsin_sse(__m128 x)
{
    const __m128 twopi = _mm_set1_ps(6.283185307179586f);
    const __m128 inv_twopi = _mm_set1_ps(0.15915494309189535f);
    const __m128 inv_pi2 = _mm_set1_ps(0.6366197723675813f);
    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 scale = _mm_set1_ps((float)(FAST_TRIG_TABLE_SIZE - 1));

    __m128 sign_mask = _mm_set1_ps(-0.0f);
    __m128 is_neg = _mm_and_ps(x, sign_mask);
    x = _mm_andnot_ps(sign_mask, x);

    __m128 div = _mm_mul_ps(x, inv_twopi);
    __m128i q_mod = _mm_cvttps_epi32(div);
    __m128 wrapped = _mm_sub_ps(x, _mm_mul_ps(_mm_cvtepi32_ps(q_mod), twopi));

    __m128 neg_mask = _mm_cmplt_ps(wrapped, _mm_setzero_ps());
    wrapped = _mm_add_ps(wrapped, _mm_and_ps(neg_mask, twopi));

    __m128 phase_pi2 = _mm_mul_ps(wrapped, inv_pi2);
    __m128i q = _mm_cvttps_epi32(phase_pi2);
    __m128 phase = _mm_sub_ps(phase_pi2, _mm_cvtepi32_ps(q));

    __m128i q_mod_2 = _mm_and_si128(q, _mm_set1_epi32(1));
    __m128 mirror_mask = _mm_castsi128_ps(_mm_cmpeq_epi32(q_mod_2, _mm_set1_epi32(1)));
    __m128 mirrored_phase = _mm_sub_ps(one, phase);
    phase = _mm_or_ps(_mm_and_ps(mirror_mask, mirrored_phase), _mm_andnot_ps(mirror_mask, phase));

    __m128 t = _mm_mul_ps(phase, scale);
    __m128i idx = _mm_cvttps_epi32(t);
    __m128 frac = _mm_sub_ps(t, _mm_cvtepi32_ps(idx));

    alignas(16) uint32_t i[4];
    _mm_storeu_si128((__m128i*)i, idx);

    __m128 a = _mm_set_ps((float)sin_table[(i[3]+1)&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[(i[2]+1)&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[(i[1]+1)&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[(i[0]+1)&FAST_TRIG_TABLE_MASK]);

    __m128 b = _mm_set_ps((float)sin_table[i[3]&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[i[2]&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[i[1]&FAST_TRIG_TABLE_MASK],
                          (float)sin_table[i[0]&FAST_TRIG_TABLE_MASK]);

    __m128 val = _mm_fmadd_ps(frac, _mm_sub_ps(a, b), b);
    val = _mm_mul_ps(val, _mm_set1_ps(0.00003051850947599719f));

    __m128i q_ge_2 = _mm_cmpgt_epi32(q, _mm_set1_epi32(1));
    val = _mm_xor_ps(val, _mm_and_ps(_mm_castsi128_ps(q_ge_2), sign_mask));
    val = _mm_xor_ps(val, is_neg);

    return val;
}

static inline __m128 fastcos_sse(__m128 x) {
    return fastsin_sse(_mm_add_ps(x, _mm_set1_ps(1.5707963267948966f)));
}
#endif

// ===================================================================
// FAST FFT (dedicated twiddle table)
typedef struct { float re; float im; } Complex;

extern Complex* twiddles[13];
extern int fast_dsp_ref_count;

extern void FastFFT(float* real, float* imag, int n);

static inline int fast_log2_32(uint32_t n) {
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse(&index, n);
    return index;
#else
    int result = 0;
    while (n >>= 1) result++;
    return result;
#endif
}

#endif // DSP_MATH_H

#ifdef DSP_MATH_IMPLEMENTATION
#include <stdlib.h>

int16_t sin_table[FAST_TRIG_TABLE_SIZE];
int16_t cos_table[FAST_TRIG_TABLE_SIZE];
int16_t tan_table[FAST_TRIG_TABLE_SIZE];
int16_t asin_table[FAST_TRIG_TABLE_SIZE];
int16_t acos_table[FAST_TRIG_TABLE_SIZE];
int16_t atan_table[FAST_TRIG_TABLE_SIZE];

Complex* twiddles[13] = {0};
int fast_dsp_ref_count = 0;

void InitFastDSP(void)
{
    fast_dsp_ref_count++;
    if (fast_dsp_ref_count > 1) return;

    const float step_trig = (M_PI / 2.0f) / (FAST_TRIG_TABLE_SIZE - 1);
    const float step_linear = 1.0f / (FAST_TRIG_TABLE_SIZE - 1);

    for (uint32_t i = 0; i < FAST_TRIG_TABLE_SIZE; ++i) {
        float x_trig = i * step_trig;
        float x_lin = i * step_linear;

        sin_table[i] = (int16_t)(sinf(x_trig) * 32767.0f);
        cos_table[i] = (int16_t)(cosf(x_trig) * 32767.0f);

        float t_val = tanf(x_trig);
        if (t_val > 100.0f) t_val = 100.0f;
        tan_table[i] = (int16_t)(t_val * 327.67f);

        asin_table[i] = (int16_t)((asinf(x_lin) / 1.5707963267948966f) * 32767.0f);
        acos_table[i] = (int16_t)((acosf(x_lin) / 1.5707963267948966f) * 32767.0f);

        float mapped = x_lin;
        float actual_x = mapped / (1.0f - mapped + 1e-6f);
        atan_table[i] = (int16_t)((atanf(actual_x) / 1.5707963267948966f) * 32767.0f);
    }

    for (int log2n = 8; log2n <= 12; ++log2n) {
        int n = 1 << log2n;
        twiddles[log2n] = (Complex*)malloc((n/2) * sizeof(Complex));
        for (int i = 0; i < n/2; ++i) {
            float angle = -2.0f * M_PI * i / n;
            twiddles[log2n][i].re = fastcos(angle);
            twiddles[log2n][i].im = fastsin(angle);
        }
    }
}

void FreeFastDSP(void)
{
    if (fast_dsp_ref_count > 0) fast_dsp_ref_count--;
    if (fast_dsp_ref_count == 0) {
        for (int i = 8; i <= 12; ++i) {
            if (twiddles[i]) free(twiddles[i]);
        }
    }
}

void FastFFT(float* real, float* imag, int n)
{
    if (n < 256 || n > 4096 || (n & (n-1)) != 0) return;
    int log2n = fast_log2_32(n);

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j >= bit; bit >>= 1) j -= bit;
        j += bit;
        if (i < j) {
            float tmp = real[i]; real[i] = real[j]; real[j] = tmp;
            tmp = imag[i]; imag[i] = imag[j]; imag[j] = tmp;
        }
    }

    Complex* w = twiddles[log2n];
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; ++j) {
                Complex* tw = &w[j * step];
                float tr = real[i+j+half] * tw->re - imag[i+j+half] * tw->im;
                float ti = real[i+j+half] * tw->im + imag[i+j+half] * tw->re;

                real[i+j+half] = real[i+j] - tr;
                imag[i+j+half] = imag[i+j] - ti;
                real[i+j] += tr;
                imag[i+j] += ti;
            }
        }
    }
}

#endif // DSP_MATH_IMPLEMENTATION
