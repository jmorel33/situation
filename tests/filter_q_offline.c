#include <math.h>
#include <stdio.h>
#include "../sit/aud/fx/filter.h"

static float goertzel440(const float* buf, int n, float sr) {
    float k = (int)(0.5f + (440.0f / sr) * n);
    float w = 2.0f * 3.14159265359f * k / n;
    float c = 2.0f * cosf(w);
    float s0=0,s1=0,s2=0;
    for (int i=0;i<n;i++) { s0=buf[i]+c*s1-s2; s2=s1; s1=s0; }
    return s1*s1+s2*s2-c*s1*s2;
}

static float run(float q) {
    SituationFilter f; filter_init(&f, 48000.0f);
    filter_set_coefficients(&f, 440.0f, q, PX_FILTER_MODE_BP, 2);
    f.drive=1.0f; f.use_oversampling=0;
    float buf[48000];
    for (int i=0;i<48000;i++) {
        float ph = 2.0f*3.14159265359f*440.0f*i/48000.0f;
        float saw = 2.0f*(440.0f*i/48000.0f - floorf(440.0f*i/48000.0f + 0.5f)); /* rough */
        float in = saw;
        buf[i] = filter_process_oversampled_amp(&f, in, 0.8f);
    }
    return goertzel440(buf+24000, 12000, 48000.0f);
}

int main(void) {
    float lo=run(0.5f), hi=run(20.0f);
    printf("offline saw BP440 amp0.8 lo=%.3e hi=%.3e ratio=%.3f q_inv lo=%.6f hi=%.6f\n", lo, hi, hi/(lo+1e-20f),
        (float)(0.115128), (float)(0.002878));
    return (hi > lo*1.5f) ? 0 : 1;
}
