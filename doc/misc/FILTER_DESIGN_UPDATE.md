# Filter Design Update - Multi-Pole SVF Implementation

**Date**: 2026-03-02  
**Status**: ✅ Complete  
**Library Version**: v2.3.63

## Overview

The filter implementation has been upgraded from a simple biquad design to a sophisticated multi-pole State Variable Filter (SVF) with advanced features.

## New Filter Features

### Multi-Pole Responses
- **1-pole (6dB/oct)**: Gentle slope, minimal resonance
- **2-pole (12dB/oct)**: Standard SVF response
- **3-pole (18dB/oct)**: Steeper slope with cascaded stages
- **4-pole (24dB/oct)**: Very steep slope, classic synth filter sound

### Filter Modes
- **LP** (Low-pass): Passes frequencies below cutoff
- **HP** (High-pass): Passes frequencies above cutoff
- **BP** (Band-pass): Passes frequencies around cutoff
- **Notch**: Removes frequencies around cutoff (LP + HP)
- **Allpass**: Passes all frequencies with phase shift
- **LP+BP**: Low-pass and band-pass combo (0.707 mix)
- **LP+HP**: Low-pass and high-pass combo (0.707 mix)
- **BP+HP**: Band-pass and high-pass combo (0.707 mix)

### Advanced Features
1. **DC Blocking**: Removes DC offset with high-pass filter (0.999 coefficient)
2. **Soft-Clipping Drive**: Tanh saturation for harmonic richness
3. **Anti-Denormal Protection**: Prevents CPU spikes from denormal numbers
4. **2x Oversampling**: Optional oversampling with half-band filter for alias reduction
5. **Resonance Scaling**: Automatic Q adjustment based on pole count
6. **Frequency-Dependent Q Limiting**: Prevents instability at high frequencies

## Technical Implementation

### State Variable Filter Topology

The SVF uses a feedback structure that generates LP, BP, and HP outputs simultaneously:

```
Input → [Notch] → [LP integrator] → LP out
           ↓            ↓
        [BP integrator] → BP out
           ↓
        HP = Notch - LP
```

### Multi-Pole Cascading

- **2-pole**: Single SVF stage (12dB/oct)
- **3-pole**: SVF stage + 1-pole stage (18dB/oct)
- **4-pole**: Two cascaded SVF stages (24dB/oct)

### Resonance Compensation

Resonance is automatically scaled based on pole count to maintain consistent feel:
- 1-pole: Q = 0.707 (neutral, no peaking)
- 2-pole: Q = user value
- 3-pole: Q = user_value^0.75
- 4-pole: Q = sqrt(user_value)

### Oversampling

When enabled, the filter processes at 2x sample rate and downsamples using a half-band FIR filter:

```c
static const float HB[4] = { 
    0.036681502163648017f,  // Tap 0
    0.2893081761252365f,    // Tap 1
    0.6739103217610154f,    // Tap 2
    -0.0f                   // Tap 3
};
```

## API Changes

### Old API (Simple Biquad)
```c
typedef enum {
    FILTER_LOWPASS,
    FILTER_HIGHPASS,
    FILTER_BANDPASS,
    FILTER_NOTCH,
    FILTER_PEAK,
    FILTER_LOWSHELF,
    FILTER_HIGHSHELF
} FilterType;

filter_init(filter, sample_rate);
filter_set_type(filter, FILTER_LOWPASS);
filter_set_frequency(filter, 1000.0f);
filter_set_q(filter, 0.707f);
filter_set_gain(filter, 0.0f);
```

### New API (Multi-Pole SVF)
```c
typedef enum {
    PX_FILTER_MODE_OFF,
    PX_FILTER_MODE_LP,
    PX_FILTER_MODE_HP,
    PX_FILTER_MODE_BP,
    PX_FILTER_MODE_NOTCH,
    PX_FILTER_MODE_ALLPASS,
    PX_FILTER_MODE_LP_BP,
    PX_FILTER_MODE_LP_HP,
    PX_FILTER_MODE_BP_HP
} PxFilterMode;

filter_init(filter, sample_rate);
filter_set_coefficients(filter, cutoff_hz, resonance_q, mode, poles);
filter_set_drive(filter, 1.0f);
filter_set_oversampling(filter, false);
```

## Device Wrapper Integration

The filter device wrapper now exposes 6 controls:

1. **Mode** (0-8): Filter mode selection
2. **Frequency** (20-20000 Hz): Cutoff frequency
3. **Resonance Q** (0.5-20.0): Resonance amount
4. **Poles** (1-4): Number of poles (6dB, 12dB, 18dB, 24dB)
5. **Drive** (0.1-10.0): Input drive/saturation
6. **Oversampling** (0/1): Enable 2x oversampling

## Performance Characteristics

### CPU Usage (relative to 2-pole)
- 1-pole: ~50% (simpler path)
- 2-pole: 100% (baseline)
- 3-pole: ~150% (SVF + 1-pole)
- 4-pole: ~200% (two SVF stages)
- Oversampling: +100% (2x processing)

### Stability
- Cutoff limited to 45% of Nyquist frequency
- Q limited based on frequency (prevents instability)
- Anti-denormal protection prevents CPU spikes
- Tanh saturation prevents numerical overflow

## Musical Characteristics

### 1-Pole (6dB/oct)
- Very gentle, natural-sounding slope
- Minimal resonance effect
- Good for subtle filtering
- Similar to RC filter in analog circuits

### 2-Pole (12dB/oct)
- Classic SVF sound
- Smooth resonance
- Versatile for most applications
- Standard for most digital filters

### 3-Pole (18dB/oct)
- Steeper than 2-pole, gentler than 4-pole
- Unique character
- Good for creative effects
- Less common in analog designs

### 4-Pole (24dB/oct)
- Very steep slope
- Classic analog synth sound (Moog-style)
- Strong resonance character
- Can self-oscillate at high Q

## Comparison to Original Design

| Feature | Old (Biquad) | New (SVF) |
|---------|-------------|-----------|
| Poles | 2 only | 1, 2, 3, or 4 |
| Modes | 7 types | 9 modes + combos |
| Drive | No | Yes (tanh) |
| Oversampling | No | Yes (2x) |
| DC Blocking | No | Yes |
| Resonance Scaling | No | Yes (pole-dependent) |
| Anti-Denormal | No | Yes |
| Combo Modes | No | Yes (LP+BP, etc.) |

## Files Modified

1. **sit/aud/filter.h** - Complete rewrite with SVF topology
2. **sit/aud/eq_4band.h** - Updated to use standalone biquad peaking filters
3. **sit/aud/device_wrappers.h** - Updated filter wrapper for new API

## Compilation Status

✅ **Compiles successfully** with MSYS2 GCC 15.1.0  
✅ **No errors**, only harmless warnings (unused functions)  
✅ **Demo application** builds and links correctly

## Usage Example

```c
// Create filter
SituationFilter filter;
filter_init(&filter, 48000.0f);

// Configure as 4-pole low-pass with resonance
filter_set_coefficients(&filter, 
    1000.0f,              // Cutoff: 1kHz
    5.0f,                 // Resonance: Q=5
    PX_FILTER_MODE_LP,    // Mode: Low-pass
    4                     // Poles: 24dB/oct
);

// Add some drive for character
filter_set_drive(&filter, 2.0f);

// Enable oversampling for high-quality
filter_set_oversampling(&filter, true);

// Process audio
filter_process(&filter, input, output, frames, channels);
```

## Design Philosophy

This filter design prioritizes:
1. **Musical character** over clinical accuracy
2. **Flexibility** with multiple pole counts and modes
3. **Stability** with automatic limiting and protection
4. **Efficiency** with optional oversampling
5. **Simplicity** with intuitive parameter scaling

The design is inspired by classic analog synthesizer filters (Moog, ARP) while adding modern digital features like oversampling and combo modes.

## Credits

Filter design based on the PxFilter architecture with State Variable Filter topology, multi-pole cascading, and advanced features for musical audio processing.

---

**Document Created**: 2026-03-02  
**Author**: Kiro AI Assistant  
**Related Documents**:
- `sit/aud/filter.h` - Implementation
- `doc/PHASE4_DEVICE_WRAPPERS_COMPLETE.md` - Device wrapper status
- `doc/AUDIO_DEVICE_INVENTORY.md` - Device catalog
