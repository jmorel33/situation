#include "sit_api_include.h"
#include <stdio.h>

static double ypq_bench_now_seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static double bench_color_from_ypq(void) {
    volatile unsigned char sink = 0;
    double t0 = ypq_bench_now_seconds();

    for (int y = 0; y < 256; y++) {
        for (int p = 0; p < 256; p++) {
            for (int q = 0; q < 256; q++) {
                ColorYPQA ypq = {(unsigned char)y, (unsigned char)p, (unsigned char)q, 255};
                ColorRGBA rgb = SituationColorFromYPQ(ypq);
                sink = (unsigned char)(sink ^ rgb.r ^ rgb.g ^ rgb.b);
            }
        }
    }

    double elapsed = ypq_bench_now_seconds() - t0;
    printf("  16M SituationColorFromYPQ: %.3fs (sink=%u)\n", elapsed, (unsigned)sink);
    return elapsed;
}

static double bench_image_adjust_ypq(void) {
    SituationImage img = {0};
    if (SituationCreateImage(384, 384, 4, &img) != SITUATION_SUCCESS) {
        fprintf(stderr, "bench: create image failed\n");
        return -1.0;
    }

    unsigned char* px = (unsigned char*)img.data;
    for (int i = 0; i < 384 * 384; i++) {
        px[i * 4 + 0] = (unsigned char)((i * 17) & 255);
        px[i * 4 + 1] = (unsigned char)((i * 31) & 255);
        px[i * 4 + 2] = (unsigned char)((i * 47) & 255);
        px[i * 4 + 3] = 255;
    }

    double t0 = ypq_bench_now_seconds();
    for (int step = 0; step < 256; step++) {
        float t = (float)step / 255.0f;
        SituationImageAdjustYPQ(&img, t * 360.0f, 1.0f + t, 1.0f, 1.0f);
    }
    double elapsed = ypq_bench_now_seconds() - t0;

    printf("  256x SituationImageAdjustYPQ(384x384): %.3fs\n", elapsed);
    SituationUnloadImage(img);
    return elapsed;
}

int main(void) {
    printf("YPQ bench (linked DLL):\n");
    bench_color_from_ypq();
    bench_image_adjust_ypq();
    return 0;
}
