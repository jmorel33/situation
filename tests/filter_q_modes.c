#include <math.h>
#include <stdio.h>
#include "../sit/aud/fx/filter.h"

static float goertzel(const float* buf, int n, float sr, float hz) {
    float k = (int)(0.5f + (hz / sr) * n);
    float w = 2.0f * 3.14159265359f * k / n;
    float c = 2.0f * cosf(w);
    float s0=0,s1=0,s2=0;
    for (int i=0;i<n;i++) { s0=buf[i]+c*s1-s2; s2=s1; s1=s0; }
    return s1*s1+s2*s2-c*s1*s2;
}

static void run_mode(const char* name, int mode, float fc, float q) {
    SituationFilter f; filter_init(&f, 48000.0f);
    filter_set_coefficients(&f, fc, q, (PxFilterMode)mode, 2);
    f.drive=1.0f;
    float buf[48000];
    for (int i=0;i<48000;i++) {
        float t = (float)i/48000.0f;
        float ph = fmodf(440.0f*t, 1.0f);
        float saw = 2.0f*ph - 1.0f;
        buf[i] = filter_process_oversampled_amp(&f, saw, 0.8f);
    }
    float p440 = goertzel(buf+36000, 8000, 48000.0f, 440.0f);
    float p880 = goertzel(buf+36000, 8000, 48000.0f, 880.0f);
    printf("%s fc=%.0f Q=%.1f 440=%.3e 880=%.3e total_rms=%.4f\n", name, fc, q, p440, p880, p440+p880);
}

int main(void) {
    run_mode("LP", PX_FILTER_MODE_LP, 800.0f, 0.5f);
    run_mode("LP", PX_FILTER_MODE_LP, 800.0f, 20.0f);
    run_mode("BP", PX_FILTER_MODE_BP, 440.0f, 0.5f);
    run_mode("BP", PX_FILTER_MODE_BP, 440.0f, 20.0f);
    /* attack ring: impulse response peak */
    SituationFilter f; filter_init(&f,48000.0f);
    filter_set_coefficients(&f,800.0f,20.0f,PX_FILTER_MODE_LP,2);
    float peak=0; for(int i=0;i<2000;i++){ float y=filter_process_oversampled_amp(&f, i==0?1.0f:0.0f, 0.8f); if(fabsf(y)>peak) peak=fabsf(y);} 
    filter_init(&f,48000.0f); filter_set_coefficients(&f,800.0f,0.5f,PX_FILTER_MODE_LP,2);
    float peak2=0; for(int i=0;i<2000;i++){ float y=filter_process_oversampled_amp(&f, i==0?1.0f:0.0f, 0.8f); if(fabsf(y)>peak2) peak=fabsf(y);}
    printf("LP800 attack peak Q0.5=%.4f Q20=%.4f ratio=%.2f\n", peak2, peak, peak/(peak2+1e-9f));
    return 0;
}
