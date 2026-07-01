#include <math.h>
#include <stdio.h>
#include "../sit/aud/fx/filter.h"

static float rms(const float* b, int n) {
    double s=0; for(int i=0;i<n;i++) s+=b[i]*b[i]; return (float)sqrt(s/n);
}

static float run_saw(SituationFilter* f, int n, float amp) {
    float sum=0;
    for(int i=0;i<n;i++) {
        float t=(float)i/48000.0f;
        float ph=fmodf(440.0f*t,1.0f);
        float saw=2.0f*ph-1.0f;
        sum += filter_process_oversampled_amp(f, saw, amp)*filter_process_oversampled_amp(f, saw, amp);
    }
    return (float)sqrt(sum/n);
}

int main(void) {
    SituationFilter f;
    filter_init(&f,48000.0f);
    filter_set_coefficients(&f,800.0f,0.5f,PX_FILTER_MODE_LP,2);
    f.drive=1.0f;
    /* settle Q=0.5 */
    for(int i=0;i<48000;i++) {
        float t=(float)i/48000.0f;
        float saw=2.0f*fmodf(440.0f*t,1.0f)-1.0f;
        filter_process_oversampled_amp(&f, saw, 0.8f);
    }
    float before=run_saw(&f,12000,0.8f);
    /* change Q without state reset */
    filter_set_coefficients(&f,800.0f,20.0f,PX_FILTER_MODE_LP,2);
    float after_no_reset=run_saw(&f,12000,0.8f);
    /* fresh filter Q=20 */
    SituationFilter f2; filter_init(&f2,48000.0f);
    filter_set_coefficients(&f2,800.0f,20.0f,PX_FILTER_MODE_LP,2);
    f2.drive=1.0f;
    for(int i=0;i<48000;i++) {
        float t=(float)i/48000.0f;
        float saw=2.0f*fmodf(440.0f*t,1.0f)-1.0f;
        filter_process_oversampled_amp(&f2, saw, 0.8f);
    }
    float fresh_q20=run_saw(&f2,12000,0.8f);
    filter_init(&f2,48000.0f);
    filter_set_coefficients(&f2,800.0f,0.5f,PX_FILTER_MODE_LP,2);
    f2.drive=1.0f;
    for(int i=0;i<48000;i++) {
        float t=(float)i/48000.0f;
        float saw=2.0f*fmodf(440.0f*t,1.0f)-1.0f;
        filter_process_oversampled_amp(&f2, saw, 0.8f);
    }
    float fresh_q05=run_saw(&f2,12000,0.8f);
    printf("steady Q0.5=%.5f\n", before);
    printf("after Q change no reset=%.5f (ratio vs before %.3f)\n", after_no_reset, after_no_reset/before);
    printf("fresh Q20=%.5f fresh Q0.5=%.5f ratio=%.3f\n", fresh_q20, fresh_q05, fresh_q20/fresh_q05);
    /* attack peak Q compare */
    SituationFilter fa,fb;
    filter_init(&fa,48000.0f); filter_init(&fb,48000.0f);
    filter_set_coefficients(&fa,800.0f,0.5f,PX_FILTER_MODE_LP,2);
    filter_set_coefficients(&fb,800.0f,20.0f,PX_FILTER_MODE_LP,2);
    fa.drive=fb.drive=1.0f;
    float pa=0,pb=0;
    for(int i=0;i<4000;i++){
        float in=(i==0)?1.0f:0.0f;
        float ya=filter_process_oversampled_amp(&fa,in,0.8f);
        float yb=filter_process_oversampled_amp(&fb,in,0.8f);
        if(fabsf(ya)>pa) pa=fabsf(ya);
        if(fabsf(yb)>pb) pb=fabsf(yb);
    }
    printf("attack peak Q0.5=%.4f Q20=%.4f ratio=%.2f\n", pa,pb,pb/(pa+1e-9f));
    return 0;
}
