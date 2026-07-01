#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define POLYSONIX_IMPLEMENTATION
#define DSP_MATH_IMPLEMENTATION
#include "../dsp_math.h"
#include "../polysonix.h"

double get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main() {
    InitFastDSP();
    int ITERS = 10000000;

    // Baseline exp2f * val
    double start = get_time_ns();
    float sum_exp2f = 0.0f;
    for (int i = 0; i < ITERS; i++) {
        float x = (float)(i % 100) / 100.0f;
        float attack = 1.0f;
        attack *= exp2f(x * -4.32192809489f);
        sum_exp2f += attack;
    }
    double end = get_time_ns();
    double time_exp2f = end - start;

    // Test fmaf (we want y = a * exp2f(x*C), this is NOT an FMA structure directly)
    // There is no FMA structure here natively unless there is an addition (a * b + c)
    // Example: mod_params.sustain_level += dest_mod[base + 2];
    // However, wait, wait... there is:
    // mod_params.sustain_level += dest_mod[base + 2]; -> this is JUST an addition.
    // So there is NO FMA structure here.

    printf("Results for %d iterations:\n", ITERS);
    printf("exp2f():     %.2f ms (sum=%f)\n", time_exp2f / 1000000.0, sum_exp2f);

    FreeFastDSP();
    return 0;
}
