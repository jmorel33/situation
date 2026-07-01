/* SVF Q: high Q must ring longer than low Q (impulse), not just get louder. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "../sit/aud/fx/filter.h"

static float ring_samples(float q) {
    SituationFilter f;
    filter_init(&f, 48000.0f);
    filter_set_coefficients(&f, 440.0f, q, PX_FILTER_MODE_LP, 2);
    (void)filter_process_internal(&f, 1.0f);
    int last = 0;
    for (int i = 1; i < 48000; i++) {
        float y = filter_process_internal(&f, 0.0f);
        if (fabsf(y) > 0.01f) last = i;
    }
    return (float)last;
}

static float coeff_q_inv(float q) {
    SituationFilter f;
    filter_init(&f, 48000.0f);
    filter_set_coefficients(&f, 440.0f, q, PX_FILTER_MODE_LP, 2);
    return f.q_inv_coeff;
}

int main(void) {
    float lo = ring_samples(0.5f);
    float hi = ring_samples(20.0f);
    float q0 = coeff_q_inv(0.5f);
    float q20 = coeff_q_inv(20.0f);
    printf("q_inv Q0.5=%.6f Q20=%.6f\n", q0, q20);
    printf("ring samples Q0.5=%.0f Q20=%.0f ratio=%.2f\n", lo, hi, hi / (lo + 1e-6f));
    if (q20 >= q0) {
        fprintf(stderr, "FAIL: q_inv_coeff should drop as Q rises\n");
        return 1;
    }
    if (hi < lo * 3.0f) {
        fprintf(stderr, "FAIL: Q=20 should ring much longer than Q=0.5\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
