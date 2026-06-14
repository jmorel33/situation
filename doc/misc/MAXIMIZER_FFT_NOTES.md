# Maximizer FFT Implementation Notes

## Current Status

FFTW3 dependency successfully eliminated. The maximizer now uses a custom Radix-2 Cooley-Tukey FFT implementation with:
- Zero external dependencies
- Precomputed twiddle factors and bit-reversal tables
- Proper conjugate symmetry for real signals
- SSE SIMD optimizations in the maximizer processor

## Performance Optimization Opportunities

### 1. FFT Butterfly Loop Unrolling
**Current**: Generic loop for all FFT stages
**Optimization**: Special-case small stages for better performance

```c
// In sit_fft_execute(), add before main loop:
if (len == 2) {
    // Unrolled len=2 butterfly (most common case)
    for (int i = 0; i < N; i += 2) {
        float u_re = data[i][0];
        float u_im = data[i][1];
        float v_re = data[i+1][0];
        float v_im = data[i+1][1];
        data[i][0] = u_re + v_re;
        data[i][1] = u_im + v_im;
        data[i+1][0] = u_re - v_re;
        data[i+1][1] = u_im - v_im;
    }
    continue;
}

if (len == 4) {
    // Unrolled len=4 butterfly
    // ... similar optimization
}
```

**Impact**: 10-20% speedup for typical audio FFT sizes (512-2048)

### 2. SIMD Butterfly Operations
**Current**: Scalar complex arithmetic in FFT core
**Optimization**: Use SSE to process 2 butterflies at once

```c
// Process 2 complex pairs simultaneously with SSE
__m128 u = _mm_load_ps(&data[i][0]);      // u_re, u_im, v_re, v_im
__m128 v = _mm_load_ps(&data[i+2][0]);    // next pair
// ... SIMD butterfly math
```

**Impact**: 30-40% speedup, but increases code complexity

### 3. ARM NEON Support
**Current**: x86 SSE only
**Future**: Add ARM NEON intrinsics for mobile/embedded

```c
#ifdef __ARM_NEON
    float32x4_t u = vld1q_f32(&data[i][0]);
    float32x4_t result = vmlaq_f32(u, v, w);  // Multiply-accumulate
    vst1q_f32(&data[i][0], result);
#endif
```

**Impact**: Essential for ARM performance parity

## Edge Cases to Test

### 1. Low FFT Sizes (H=16, N=128)
**Risk**: Bit-reversal and twiddle indexing at small N
**Test**: 
```c
init_maximizer(&state, 48000, 16, 5, 20000.0f, 4);
// Verify output is clean, no artifacts
```

### 2. High Band Count with Many Overtones
**Risk**: Bin overflow when D is large (e.g., D=10)
**Test**:
```c
set_band_params(&state, 0, 100.0f, 1.0f, 2.0f, 10);  // 10 overtones
// Check that bin_overtone doesn't exceed N/2
```

**Fix**: Add bounds check in overtone loop:
```c
for (int d = 1; d <= band->D; d++) {
    float bin_overtone = dominant_bin_est * d;
    if (bin_overtone > state->N / 2) break;  // Already present
    // ...
}
```

### 3. Extreme Q Values
**Risk**: Very narrow bands (high Q) or very wide bands (low Q < 0.5)
**Test**:
```c
set_band_params(&state, 0, 1000.0f, 0.1f, 1.5f, 3);  // Q=0.1 (very wide)
set_band_params(&state, 1, 5000.0f, 50.0f, 1.5f, 3); // Q=50 (very narrow)
```

**Risk**: 
- Low Q: `low_bin >= high_bin` (band collapses)
- High Q: Band becomes single bin, no peaks found

**Fix**: Add validation in `set_band_params`:
```c
static void set_band_params(MaximizerState *state, int band_index, 
                           float center_freq, float Q, 
                           float enhancement_factor, int D) {
    if (band_index < 0 || band_index >= state->num_bands) return;
    
    // Clamp Q to reasonable range
    if (Q < 0.5f) Q = 0.5f;
    if (Q > 100.0f) Q = 100.0f;
    
    // Clamp D to prevent bin overflow
    if (D > 20) D = 20;
    
    state->bands[band_index].center_freq = center_freq;
    state->bands[band_index].Q = Q;
    state->bands[band_index].enhancement_factor = enhancement_factor;
    state->bands[band_index].D = D;
}
```

### 4. Nyquist Frequency Handling
**Risk**: Bin N/2 (Nyquist) must be real-only
**Current**: Properly enforced in maximizer_processor:
```c
state->fft_buffer[state->N / 2][1] = 0.0f; // Nyquist is strictly real
```
**Status**: ✓ Correct

### 5. DC Bin Handling
**Risk**: Bin 0 (DC) must be real-only
**Current**: Properly enforced:
```c
state->fft_buffer[0][1] = 0.0f; // DC is strictly real
```
**Status**: ✓ Correct

## Performance Benchmarks (TODO)

Test on typical audio workload:
- Sample rate: 48kHz
- Hop size: 512 (N=4096)
- 4 bands, D=3-5 overtones each
- Measure: CPU cycles per frame

Target: < 5% CPU on modern x86 (2GHz+)

## Known Limitations

1. **Power-of-2 Only**: FFT requires N to be power of 2
   - Current: H is enforced to nearest power of 2 in init_maximizer
   - Status: ✓ Handled

2. **No Mixed-Radix**: Cannot handle N=3, 5, 7, etc.
   - Impact: Minimal for audio (powers of 2 are standard)
   - Future: Could add Bluestein's algorithm for arbitrary N

3. **Single Precision Only**: Uses float, not double
   - Impact: Sufficient for audio (24-bit = ~144dB SNR)
   - Status: ✓ Acceptable

## Validation Tests

### Correctness Test
Compare output against known FFT (e.g., numpy.fft):
```python
import numpy as np
N = 512
x = np.random.randn(N)
X_ref = np.fft.fft(x)
# Compare with sit_fft_execute output
```

### Impulse Response Test
```c
float impulse[512] = {1.0f, 0.0f, ...};
maximizer_processor(&state, impulse, output, 512);
// Output should be clean, no ringing
```

### Sine Wave Test
```c
// Generate 440Hz sine at 48kHz
for (int i = 0; i < 512; i++) {
    input[i] = sinf(2.0f * M_PI * 440.0f * i / 48000.0f);
}
maximizer_processor(&state, input, output, 512);
// Peak should be at bin ~5 (440Hz / (48000/512))
```

## Future Enhancements

1. **Cache Twiddle Factors**: Store in plan, not recompute
   - Status: ✓ Already done

2. **Split-Radix FFT**: Faster than pure Radix-2
   - Complexity: High
   - Benefit: 10-15% speedup

3. **Real-to-Complex Optimization**: Exploit Hermitian symmetry
   - Current: Uses full complex FFT
   - Benefit: 2x speedup + half memory

4. **Wisdom/Planning**: FFTW-style plan optimization
   - Complexity: Very high
   - Benefit: Marginal for fixed audio sizes

---

**Status**: Production ready for audio DSP
**License**: MIT (no external dependencies)
**Maintainer**: Jacques Morel
