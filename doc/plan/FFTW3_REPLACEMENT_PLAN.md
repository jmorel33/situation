# FFTW3 Replacement Plan

## Problem Statement

FFTW3 is causing major deployment issues:
- Requires external DLL at runtime (`libfftw3f-3.dll`)
- No true static library available in prebuilt binaries
- Creates dependency hell for end users
- Breaks the "single header" philosophy of Situation
- Users shouldn't need to deal with external dependencies

## Current Usage

FFTW3 is only used in ONE place:
- `sit/aud/fx/maximizer.h` - Spectral maximizer effect using FFT/STFT processing

Functions used:
- `fftwf_malloc()` - Aligned memory allocation
- `fftwf_free()` - Free aligned memory
- `fftwf_plan_dft_r2c_1d()` - Create forward FFT plan (real to complex)
- `fftwf_plan_dft_c2r_1d()` - Create inverse FFT plan (complex to real)
- `fftwf_execute()` - Execute FFT
- `fftwf_destroy_plan()` - Cleanup

## Replacement Strategy

### Option 1: Custom Radix-2 FFT (RECOMMENDED)
Implement a simple, efficient FFT using Cooley-Tukey algorithm:
- Pure C11 implementation
- SSE/AVX optimized for real performance
- Power-of-2 sizes only (sufficient for audio)
- Single-header library style
- No external dependencies

**Pros:**
- Complete control over implementation
- Zero dependencies
- Can optimize specifically for audio use cases
- Educational value

**Cons:**
- Need to implement and test
- May not be as optimized as FFTW initially

### Option 2: Use pffft (Public Domain FFT)
Small, fast FFT library specifically designed for audio:
- Public domain (no licensing issues)
- Single C file
- SSE optimized
- Designed for real-time audio

**Pros:**
- Battle-tested
- Very fast
- Easy to integrate
- Public domain

**Cons:**
- Still an external dependency (but can be embedded)

### Option 3: Kiss FFT
Simple FFT library in C:
- BSD licensed
- Small codebase
- Easy to understand

**Pros:**
- Simple
- Well documented
- Permissive license

**Cons:**
- Not as fast as FFTW or pffft

## Recommended Approach: FFTW3 API-Compatible Replacement

Create `ext/fftw3.h` (drop-in replacement) with EXACT same API:

1. **Mimic FFTW3 API Exactly**
   - Same function names: `fftwf_*`
   - Same types: `fftwf_plan`, `fftwf_complex`
   - Same behavior: plan creation, execution, destruction
   - Zero code changes needed in maximizer.h

2. **Implementation**
   - Radix-2 Cooley-Tukey FFT algorithm
   - Real-to-Complex (R2C) and Complex-to-Real (C2R)
   - Power-of-2 sizes only (sufficient for audio)
   - SSE optimized

3. **Drop-in Replacement**
   - Replace `#include <fftw3.h>` with our version
   - No changes to maximizer.h code
   - No changes to API calls
   - Just works

## Implementation Plan

### Phase 1: Create FFTW3-Compatible Header (2-3 hours)
- [ ] Create `ext/fftw3.h` with exact FFTW3 API
- [ ] Implement `fftwf_malloc()` / `fftwf_free()`
- [ ] Implement `fftwf_plan_dft_r2c_1d()`
- [ ] Implement `fftwf_plan_dft_c2r_1d()`
- [ ] Implement `fftwf_execute()`
- [ ] Implement `fftwf_destroy_plan()`

### Phase 2: Core FFT Algorithm (1-2 hours)
- [ ] Radix-2 Cooley-Tukey FFT (forward)
- [ ] Radix-2 Cooley-Tukey IFFT (inverse)
- [ ] Bit-reversal permutation
- [ ] Twiddle factor computation
- [ ] Test with sine waves

### Phase 3: Optimization (1 hour)
- [ ] SSE intrinsics for butterfly operations
- [ ] Pre-compute twiddle factors in plan
- [ ] Optimize memory layout

### Phase 4: Integration & Testing (30 minutes)
- [ ] Remove FFTW3 from build scripts (no more `-lfftw3f-3`)
- [ ] Remove FFTW3 include paths
- [ ] Test maximizer effect
- [ ] Verify no code changes needed in maximizer.h

### Phase 5: Cleanup (15 minutes)
- [ ] Delete `ext/fftw-3.3.5-dll64/` directory
- [ ] Update documentation
- [ ] Celebrate zero dependencies

## API Design (EXACT FFTW3 COMPATIBILITY)

```c
// ext/fftw3.h - Drop-in replacement for FFTW3

#ifndef FFTW3_H
#define FFTW3_H

#include <stdlib.h>

// FFTW3 types (exact match)
typedef float fftwf_complex[2];  // [0] = real, [1] = imag

typedef struct fftwf_plan_s {
    int n;                       // FFT size
    int sign;                    // 1 = forward, -1 = inverse
    float* twiddle_cos;          // Pre-computed cos(2πk/n)
    float* twiddle_sin;          // Pre-computed sin(2πk/n)
    int* bit_reverse;            // Bit-reversal lookup
} *fftwf_plan;

// FFTW3 flags (we ignore these, but keep for compatibility)
#define FFTW_MEASURE 0
#define FFTW_ESTIMATE 1

// FFTW3 API (exact function signatures)
void* fftwf_malloc(size_t n);
void fftwf_free(void* p);

fftwf_plan fftwf_plan_dft_r2c_1d(int n, float* in, fftwf_complex* out, unsigned flags);
fftwf_plan fftwf_plan_dft_c2r_1d(int n, fftwf_complex* in, float* out, unsigned flags);

void fftwf_execute(const fftwf_plan p);
void fftwf_destroy_plan(fftwf_plan p);

#endif // FFTW3_H
```

## Migration Path

1. Create `ext/fftw3.h` with FFTW3-compatible API
2. Remove external FFTW3 from build scripts
3. Test maximizer - should work with ZERO code changes
4. Delete `ext/fftw-3.3.5-dll64/` directory
5. Ship v2.4.0 with zero external dependencies

**Key Point**: maximizer.h doesn't change AT ALL. It still calls `fftwf_*` functions, but now they're our implementation.

## Benefits

- **Zero Dependencies**: No external DLLs needed
- **Single Header**: True header-only library
- **Full Control**: Can optimize for our specific use cases
- **No Licensing Issues**: Our own code, MIT licensed
- **Better User Experience**: Users never see dependency errors
- **Smaller Distribution**: No need to ship FFTW3 DLL

## Timeline

- **Tonight**: Create plan (DONE)
- **Next Session**: Implement Phase 1 (Core FFT)
- **Following Session**: Phases 2-4 (Optimize, integrate, cleanup)
- **Total Time**: ~5-6 hours of focused work

## Success Criteria

- [ ] Maximizer effect works identically to FFTW3 version
- [ ] No external dependencies required
- [ ] All examples compile and run without DLL errors
- [ ] Performance within 20% of FFTW3 (acceptable for audio)
- [ ] Zero breaking changes to user-facing API

---

**Status**: PLAN CREATED - Ready for implementation
**Priority**: HIGH - Blocking v2.4.0 release
**Assigned**: Next development session
