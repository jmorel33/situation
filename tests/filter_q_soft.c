#include <math.h>
#include <stdio.h>
#include "../sit/aud/fx/filter.h"

static float attack_peak(float q, int soft) {
    SituationFilter f; filter_init(&f,48000.0f);
    filter_set_coefficients(&f,800.0f,q,PX_FILTER_MODE_LP,2);
    f.drive=1.0f;
    float peak=0;
    for(int i=0;i<8000;i++){
        float in=(i==0)?1.0f:0.0f;
        float y;
        if(soft) y=filter_process_oversampled_amp(&f,in,0.8f);
        else {
            float y0=filter_process_internal(&f,in);
            float y1=filter_process_internal(&f,in);
            y=(y0+y1)*0.5f*0.8f;
        }
        if(fabsf(y)>peak) peak=fabsf(y);
    }
    return peak;
}
int main(void){
    float lo=attack_peak(0.5f,1), hi=attack_peak(20.0f,1);
    float lo2=attack_peak(0.5f,0), hi2=attack_peak(20.0f,0);
    printf("with softclip: Q0.5=%.4f Q20=%.4f ratio=%.2f\n", lo,hi,hi/lo);
    printf("no softclip:  Q0.5=%.4f Q20=%.4f ratio=%.2f\n", lo2,hi2,hi2/lo2);
    return 0;
}
