#include "../px_vm.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

void test_lfsr_noise_determinism() {
    printf("Testing LFSR noise determinism...\n");
    px_vm_init_lfsr_tables();

    for (int type_idx = 0; type_idx < NUM_LFSR_TYPES; type_idx++) {
        LfsrType type = (LfsrType)type_idx;
        printf("  Testing type: %s\n", lfsr_type_to_string(type));

        float phase = 1.234f;
        float rate = 2.5f;

        float val1 = lfsr_get_noise(type, phase, rate);
        float val2 = lfsr_get_noise(type, phase, rate);

        if (val1 != val2) {
            fprintf(stderr, "FAIL: Non-deterministic output for type %s! val1=%f, val2=%f\n", lfsr_type_to_string(type), val1, val2);
            exit(1);
        }

        // Noise should be either -1.0 or 1.0
        if (fabsf(val1) != 1.0f) {
            fprintf(stderr, "FAIL: Noise value out of expected range for type %s! val=%f\n", lfsr_type_to_string(type), val1);
            exit(1);
        }

        // Test that a significantly different phase can produce a different value
        // For a period P, changing phase by 2*PI/P should move at least one bit.
        // We'll just check if we can find at least one different value in the whole period.
        bool found_different = false;
        for (int i = 1; i <= 100; i++) {
            float val_diff = lfsr_get_noise(type, phase + (float)i * 0.1f, rate);
            if (val_diff != val1) {
                found_different = true;
                break;
            }
        }
        if (!found_different) {
            fprintf(stderr, "FAIL: Noise for type %s is constant across many phases!\n", lfsr_type_to_string(type));
            exit(1);
        }
    }

    px_vm_free_lfsr_tables();
    printf("LFSR noise determinism test passed!\n");
}

void test_lfsr_uninitialized() {
    printf("Testing uninitialized LFSR...\n");
    // Ensure tables are free
    px_vm_free_lfsr_tables();

    float val = lfsr_get_noise(LFSR_8BIT, 1.0f, 1.0f);
    if (val != 0.0f) {
        fprintf(stderr, "FAIL: Uninitialized LFSR should return 0.0, got %f\n", val);
        exit(1);
    }
    printf("Uninitialized LFSR test passed!\n");
}

void test_lfsr_bounds() {
    printf("Testing LFSR type bounds...\n");
    px_vm_init_lfsr_tables();

    float val = lfsr_get_noise((LfsrType)NUM_LFSR_TYPES, 1.0f, 1.0f);
    if (val != 0.0f) {
        fprintf(stderr, "FAIL: Out-of-bounds LFSR type should return 0.0, got %f\n", val);
        exit(1);
    }

    px_vm_free_lfsr_tables();
    printf("LFSR type bounds test passed!\n");
}

int main() {
    srand((unsigned int)time(NULL));
    test_lfsr_noise_determinism();
    test_lfsr_uninitialized();
    test_lfsr_bounds();
    printf("ALL LFSR TESTS PASSED!\n");
    return 0;
}
