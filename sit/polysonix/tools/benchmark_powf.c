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

    // Baseline powf
    double start = get_time_ns();
    float sum_powf = 0.0f;
    for (int i = 0; i < ITERS; i++) {
        float x = (float)(i % 100) / 100.0f;
        sum_powf += powf(0.05f, x);
        sum_powf += powf(0.1f, x);
        sum_powf += powf(0.1f, x);
    }
    double end = get_time_ns();
    double time_powf = end - start;

    // Test alternative 1: exp2f(x * log2(base))
    start = get_time_ns();
    float sum_exp2f = 0.0f;
    for (int i = 0; i < ITERS; i++) {
        float x = (float)(i % 100) / 100.0f;
        sum_exp2f += exp2f(x * -4.32192809489f);
        sum_exp2f += exp2f(x * -3.32192809489f);
        sum_exp2f += exp2f(x * -3.32192809489f);
    }
    end = get_time_ns();
    double time_exp2f = end - start;

    printf("Results for %d iterations:\n", ITERS);
    printf("powf():      %.2f ms (sum=%f)\n", time_powf / 1000000.0, sum_powf);
    printf("exp2f():     %.2f ms (sum=%f)\n", time_exp2f / 1000000.0, sum_exp2f);

    FreeFastDSP();
    return 0;
}
