#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#define POLYSONIX_IMPLEMENTATION
#include "polysonix.h"

int main() {
    printf("Testing ADSR_TriggerAttack...\n");

    ADSR adsr;
    PxADSRParams params;
    float sample_rate = 44100.0f;

    // Test 1: Trigger Attack when disabled
    params.attack_time = 0.1f;
    params.decay_time = 0.1f;
    params.sustain_level = 0.5f;
    params.release_time = 0.1f;
    params.enabled = false;
    ADSR_Init(&adsr, &params, sample_rate);
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_IDLE);
    printf("Test 1 passed: Disabled ADSR does not trigger attack.\n");

    // Test 2: Normal Trigger Attack from IDLE
    params.enabled = true;
    ADSR_Init(&adsr, &params, sample_rate);
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_ATTACK);
    printf("Test 2 passed: Normal Trigger Attack from IDLE works.\n");

    // Test 3: Trigger Attack from RELEASE
    adsr.state = ADSR_STATE_RELEASE;
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_ATTACK);
    printf("Test 3 passed: Trigger Attack from RELEASE works.\n");

    // Test 4: Trigger Attack when already in ATTACK
    adsr.state = ADSR_STATE_ATTACK;
    adsr.level = 0.5f; // intermediate level
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_ATTACK);
    assert(adsr.level == 0.5f);
    printf("Test 4 passed: Trigger Attack ignored when already in ATTACK.\n");

    // Test 5: Instant Attack
    params.attack_time = 0.0f; // Will be clamped to MIN_ADSR_TIME or less
    params.decay_time = 0.1f;
    params.sustain_level = 0.5f;
    ADSR_Init(&adsr, &params, sample_rate);
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_DECAY);
    assert(adsr.level == 1.0f);
    printf("Test 5 passed: Instant Attack transitions to DECAY.\n");

    // Test 6: Instant Attack and Instant Decay
    params.attack_time = 0.0f;
    params.decay_time = 0.0f;
    params.sustain_level = 0.5f;
    ADSR_Init(&adsr, &params, sample_rate);
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_SUSTAIN);
    assert(adsr.level == params.sustain_level);
    printf("Test 6 passed: Instant Attack and Decay transitions to SUSTAIN.\n");

    // Test 7: Instant Attack and high sustain level
    params.attack_time = 0.0f;
    params.decay_time = 0.1f;
    params.sustain_level = 1.0f;
    ADSR_Init(&adsr, &params, sample_rate);
    ADSR_TriggerAttack(&adsr);
    assert(adsr.state == ADSR_STATE_SUSTAIN);
    assert(adsr.level == 1.0f);
    printf("Test 7 passed: Instant Attack and high sustain transitions to SUSTAIN.\n");

    printf("All ADSR_TriggerAttack tests passed!\n");
    return 0;
}
