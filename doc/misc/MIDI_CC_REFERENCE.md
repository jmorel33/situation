# MIDI CC Reference - Situation Audio Engine

Quick reference for all MIDI CC mappings in Situation.

## 14-bit MIDI CC Support

Situation supports high-resolution 14-bit MIDI using MSB/LSB pairs:
- **MSB (Coarse)**: CC 0-31 (7 bits)
- **LSB (Fine)**: CC 32-63 (7 bits)
- **Combined**: (MSB << 7) | LSB = 0-16383 (14 bits)

**Benefits:**
- 128x more precision (16384 vs 128 steps)
- Smooth parameter sweeps (no zipper noise)
- ~1.2Hz resolution for filter cutoff (20Hz-20kHz)

**See:** `doc/MIDI_14BIT_SUPPORT.md` for complete documentation and examples.

## Standard MIDI CCs (Preserved)

| CC | Parameter | Range | Notes |
|----|-----------|-------|-------|
| 1 | Modulation Wheel | 0-127 | Standard MIDI |
| 7 | Volume | 0-127 | Standard MIDI |
| 10 | Pan | 0-127 | Used by Panner device |
| 11 | Expression | 0-127 | Standard MIDI |
| 64 | Sustain Pedal | 0-127 | Standard MIDI |
| 91 | Reverb Level | 0-127 | Used by Reverb device |
| 92 | Tremolo Depth | 0-127 | Used by Reverb (wet) |
| 93 | Chorus Depth | 0-127 | Used by Reverb (damp) |
| 94 | Celeste Depth | 0-127 | Used by Reverb (dry) |
| 95 | Phaser Depth | 0-127 | Used by Reverb (width) |

## Compander (CC 16-39)

3-band multiband compander with per-band EQ.

### Band 0 (Low, <200Hz) - CC 16-23

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 16 | Comp Threshold | 0.0-1.0 | Linear |
| 17 | Exp Threshold | 0.0-1.0 | Linear |
| 18 | Comp Slope | 1.0-10.0 | Linear |
| 19 | Exp Slope | 1.0-10.0 | Linear |
| 20 | Noise Gate | -96dB to 0dB | Linear (dB) |
| 21 | Bell Freq | 20Hz-200Hz | Logarithmic |
| 22 | Bell Gain | -24dB to +24dB | Linear (dB) |
| 23 | Bell Q | 0.1-10.0 | Linear |

### Band 1 (Mid, 200-4000Hz) - CC 24-31

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 24 | Comp Threshold | 0.0-1.0 | Linear |
| 25 | Exp Threshold | 0.0-1.0 | Linear |
| 26 | Comp Slope | 1.0-10.0 | Linear |
| 27 | Exp Slope | 1.0-10.0 | Linear |
| 28 | Noise Gate | -96dB to 0dB | Linear (dB) |
| 29 | Bell Freq | 200Hz-4000Hz | Logarithmic |
| 30 | Bell Gain | -24dB to +24dB | Linear (dB) |
| 31 | Bell Q | 0.1-10.0 | Linear |

### Band 2 (High, >4000Hz) - CC 32-39

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 32 | Comp Threshold | 0.0-1.0 | Linear |
| 33 | Exp Threshold | 0.0-1.0 | Linear |
| 34 | Comp Slope | 1.0-10.0 | Linear |
| 35 | Exp Slope | 1.0-10.0 | Linear |
| 36 | Noise Gate | -96dB to 0dB | Linear (dB) |
| 37 | Bell Freq | 4000Hz-20000Hz | Logarithmic |
| 38 | Bell Gain | -24dB to +24dB | Linear (dB) |
| 39 | Bell Q | 0.1-10.0 | Linear |

## Filter (CC 40-45)

Biquad filter with multiple modes.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 40 | Type | 0-6 | Discrete | 0=LP, 1=HP, 2=BP, 3=Notch, 4=Peak, 5=LS, 6=HS |
| 41 | Cutoff | 20Hz-20kHz | Logarithmic | |
| 42 | Resonance/Q | 0.1-10.0 | Linear | |
| 43 | Gain | -24dB to +24dB | Linear (dB) | For peak/shelf modes |
| 44 | Drive | 1.0-10.0 | Linear | Saturation amount |
| 45 | Oversampling | 0-2 | Discrete | 0=off, 1=2x, 2=4x |

## EQ 4-Band (CC 46-57)

Parametric equalizer with 4 bands.

### Band 0 (Low) - CC 46-48

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 46 | Frequency | 20Hz-500Hz | Logarithmic |
| 47 | Q | 0.1-10.0 | Linear |
| 48 | Gain | -24dB to +24dB | Linear (dB) |

### Band 1 (Low-Mid) - CC 49-51

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 49 | Frequency | 200Hz-2kHz | Logarithmic |
| 50 | Q | 0.1-10.0 | Linear |
| 51 | Gain | -24dB to +24dB | Linear (dB) |

### Band 2 (High-Mid) - CC 52-54

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 52 | Frequency | 1kHz-8kHz | Logarithmic |
| 53 | Q | 0.1-10.0 | Linear |
| 54 | Gain | -24dB to +24dB | Linear (dB) |

### Band 3 (High) - CC 55-57

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 55 | Frequency | 4kHz-20kHz | Logarithmic |
| 56 | Q | 0.1-10.0 | Linear |
| 57 | Gain | -24dB to +24dB | Linear (dB) |

## Echo (CC 58-61)

Delay effect.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 58 | Delay Time | 10ms-2000ms | Logarithmic |
| 59 | Feedback | 0.0-0.95 | Linear |
| 60 | Wet | 0.0-1.0 | Linear |
| 61 | Dry | 0.0-1.0 | Linear |

## SST-282 (CC 62-69, 77-79, 89-90)

Hardware emulation (stereo delay/reverb).

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 62 | Input Gain | 0.0-2.0 | Linear | |
| 63 | LF Cut | -12dB to +12dB | Linear (dB) | Low frequency EQ |
| 64 | HF Cut | -12dB to +12dB | Linear (dB) | High frequency EQ |
| 65 | Tap Level 0 | 0.0-1.0 | Linear | First tap |
| 66 | Tap Level 1 | 0.0-1.0 | Linear | Second tap |
| 67 | Tap Level 2 | 0.0-1.0 | Linear | Third tap |
| 68 | Tap Level 3 | 0.0-1.0 | Linear | Fourth tap |
| 69 | Feedback | 0.0-0.95 | Linear | |
| 77 | Dry Level | -60dB to 0dB | Linear (dB) | |
| 78 | Echo Level | -60dB to 0dB | Linear (dB) | |
| 79 | Direct Level | -60dB to 0dB | Linear (dB) | |
| 89 | Mode | 0-1 | Discrete | 0=Normal, 1=Reverse |
| 90 | Echo Delay | 0ms-500ms | Linear | |

## Dynamics (CC 70-76)

Compressor/Limiter/Gate/Expander.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 70 | Mode | 0-3 | Discrete | 0=Comp, 1=Limit, 2=Gate, 3=Expand |
| 71 | Threshold | -60dB to 0dB | Linear (dB) | |
| 72 | Ratio | 1.0-20.0 | Linear | |
| 73 | Attack | 0.1ms-100ms | Logarithmic | |
| 74 | Release | 10ms-1000ms | Logarithmic | |
| 75 | Knee | 0dB-12dB | Linear (dB) | |
| 76 | Makeup Gain | 0dB-24dB | Linear (dB) | |

## Overdrive (CC 80-83)

Multi-mode distortion.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 80 | Mode | 0-3 | Discrete | 0=Soft, 1=Hard, 2=Fuzz, 3=Tube |
| 81 | Drive | 1.0-100.0 | Logarithmic | |
| 82 | Tone | 0.0-1.0 | Linear | |
| 83 | Level | 0.0-2.0 | Linear | Output level |

## Chorus (CC 12-15)

4-stage chorus effect.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 12 | Rate | 0.1Hz-10Hz | Logarithmic |
| 13 | Depth | 0.0-1.0 | Linear |
| 14 | Feedback | 0.0-0.9 | Linear |
| 15 | Mix | 0.0-1.0 | Linear |

## Overdrive (CC 80-83)

Multi-mode distortion.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 80 | Mode | 0-3 | Discrete | 0=Soft, 1=Hard, 2=Fuzz, 3=Tube |
| 81 | Drive | 1.0-100.0 | Logarithmic | |
| 82 | Tone | 0.0-1.0 | Linear | |
| 83 | Level | 0.0-2.0 | Linear | Output level |

## Phaser (CC 84-88)

All-pass filter phaser effect.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 84 | Rate | 0.1Hz-10Hz | Logarithmic |
| 85 | Depth | 0.0-1.0 | Linear |
| 86 | Feedback | 0.0-0.95 | Linear |
| 87 | Stages | 2-12 | Linear |
| 88 | Mix | 0.0-1.0 | Linear |

## Exciter (CC 96-99)

Harmonic enhancer.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 96 | Frequency | 1kHz-10kHz | Logarithmic |
| 97 | Harmonics | 0.0-1.0 | Linear |
| 98 | Blend | 0.0-1.0 | Linear |
| 99 | Drive | 1.0-10.0 | Linear |

## Mastering Amp (CC 3-11, 19-21, 100-101, 118-119)

SSE-optimized mastering console processor.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 100 | Amp Type | 0-3 | Discrete | 0=Clean, 1=Warm, 2=Vintage, 3=Modern |
| 101 | Drive | 0.0-10.0 | Linear | |
| 118 | Low Freq | 20Hz-500Hz | Logarithmic | |
| 119 | Low Gain | -24dB to +24dB | Linear (dB) | |
| 3 | Mid Freq | 200Hz-5kHz | Logarithmic | |
| 9 | Mid Gain | -24dB to +24dB | Linear (dB) | |
| 4 | Mid Q | 0.1-10.0 | Linear | |
| 5 | High Freq | 2kHz-20kHz | Logarithmic | |
| 6 | High Gain | -24dB to +24dB | Linear (dB) | |
| 7 | Air Freq | 8kHz-20kHz | Logarithmic | |
| 8 | Air Gain | -12dB to +12dB | Linear (dB) | |
| 11 | Tight Cutoff | 20Hz-200Hz | Logarithmic | |
| 19 | Aspect Ratio | 0.0-1.0 | Linear | |
| 20 | Vintage | 0-1 | Discrete | 0=Off, 1=On |
| 21 | Circuit Bending | 0-1 | Discrete | 0=Off, 1=On |

## Maximizer (CC 22-31, 65-69, 77-79)

FFTW3-based spectral enhancer (4-band multiband).

### Band 0 (Low, CC 22-25)
| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 22 | Frequency | 20Hz-500Hz | Logarithmic |
| 23 | Threshold | 0.0-2.0 | Linear |
| 24 | Ratio | 1.0-10.0 | Linear |
| 25 | Attack | 1-10 | Linear |

### Band 1 (Low-Mid, CC 26-29)
| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 26 | Frequency | 200Hz-2kHz | Logarithmic |
| 27 | Threshold | 0.0-2.0 | Linear |
| 28 | Ratio | 1.0-10.0 | Linear |
| 29 | Attack | 1-10 | Linear |

### Band 2 (High-Mid, CC 30-31, 65-66)
| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 30 | Frequency | 1kHz-8kHz | Logarithmic |
| 31 | Threshold | 0.0-2.0 | Linear |
| 65 | Ratio | 1.0-10.0 | Linear |
| 66 | Attack | 1-10 | Linear |

### Band 3 (High, CC 67-69, 77)
| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 67 | Frequency | 4kHz-20kHz | Logarithmic |
| 68 | Threshold | 0.0-2.0 | Linear |
| 69 | Ratio | 1.0-10.0 | Linear |
| 77 | Attack | 1-10 | Linear |

### Filters (CC 78-79)
| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 78 | HPF Cutoff | 20Hz-500Hz | Logarithmic |
| 79 | LPF Cutoff | 5kHz-20kHz | Logarithmic |

## LFO (CC 102-103)

Low frequency oscillator.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 102 | Waveform | 0-4 | Discrete | 0=Sine, 1=Tri, 2=Saw, 3=Square, 4=Random |
| 103 | Frequency | 0.01Hz-20Hz | Logarithmic | |

## LFO (CC 102-103)

Low frequency oscillator.

| CC | Parameter | Range | Scaling | Notes |
|----|-----------|-------|---------|-------|
| 102 | Waveform | 0-4 | Discrete | 0=Sine, 1=Tri, 2=Saw, 3=Square, 4=Random |
| 103 | Frequency | 0.01Hz-20Hz | Logarithmic | |

## Studio Reverb (CC 104-111)

Professional algorithmic reverb.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 104 | Pre-Delay | 0ms-100ms | Linear |
| 105 | Room Size | 0.0-1.0 | Linear |
| 106 | Damping | 0.0-1.0 | Linear |
| 107 | Diffusion | 0.0-1.0 | Linear |
| 108 | Decay Time | 0.1s-10s | Logarithmic |
| 109 | Early Reflections | 0.0-1.0 | Linear |
| 110 | Wet | 0.0-1.0 | Linear |
| 111 | Dry | 0.0-1.0 | Linear |

## Spring Reverb (CC 112-117)

Physical modeling spring reverb.

| CC | Parameter | Range | Scaling |
|----|-----------|-------|---------|
| 112 | Spring Tension | 0.0-1.0 | Linear |
| 113 | Spring Damping | 0.0-1.0 | Linear |
| 114 | Spring Length | 0.0-1.0 | Linear |
| 115 | Drive | 1.0-10.0 | Linear |
| 116 | Wet | 0.0-1.0 | Linear |
| 117 | Dry | 0.0-1.0 | Linear |

## Reserved Ranges

| CC Range | Status | Notes |
|----------|--------|-------|
| 0-9 | Reserved | Standard MIDI controllers |
| 62-69 | Available | For future devices |
| 77-79 | Available | For future devices |
| 89-90 | Available | For future devices |
| 100-101 | Available | For future devices |
| 118-119 | Available | For future devices |
| 120-127 | Reserved | MIDI channel mode messages |

## CC Allocation Guidelines

When adding MIDI support to a new device:

1. **Check availability**: Use the "Available" ranges above
2. **Group parameters**: Keep related CCs contiguous (e.g., all band params together)
3. **Use standard CCs**: When applicable (e.g., CC 10 for pan, CC 91 for reverb)
4. **Document thoroughly**: Add to this reference and callback function header
5. **Consider ergonomics**: Put frequently-adjusted params on lower CC numbers

## Scaling Types

### Linear
```
value = min + (cc_value / 127.0) * (max - min)
```
Example: CC 64 → 0.504 (for 0.0-1.0 range)

### Logarithmic
```
log_min = log(min)
log_max = log(max)
value = exp(log_min + (cc_value / 127.0) * (log_max - log_min))
```
Example: CC 64 → 632Hz (for 20Hz-20kHz range)

### Decibel (Linear in dB)
```
value_db = min_db + (cc_value / 127.0) * (max_db - min_db)
```
Example: CC 64 → -30dB (for -60dB to 0dB range)

### Discrete
```
value = floor(min + (cc_value / 127.0) * (max - min + 0.99))
```
Example: CC 64 → 2 (for 0-3 range, 4 modes)

## MIDI Learn (Future)

Planned feature for dynamic CC mapping:

```c
// Enter learn mode for a parameter
SIT_MidiDevice_StartLearn(midi_device, control_index);

// Next CC received will be mapped to control_index
// Mapping saved to preset file
```

## See Also

- `sit/aud/midi_device_callbacks.h` - Implementation
- `doc/MIDI_DEVICE_CALLBACKS_ARCHITECTURE.md` - Architecture details
- `examples/midi_compander_control.c` - Usage example

---

## Complete Device Summary

| Device | Node Type | CC Range | Parameters | Status |
|--------|-----------|----------|------------|--------|
| Compander | SITUATION_NODE_COMPANDER | 16-39 | 24 (3 bands × 8) | ✅ |
| Filter | SITUATION_NODE_FILTER | 40-45 | 6 | ✅ |
| EQ 4-Band | SITUATION_NODE_EQ_4BAND | 46-57 | 12 (4 bands × 3) | ✅ |
| Echo | SITUATION_NODE_ECHO | 58-61 | 4 | ✅ |
| SST-282 | SITUATION_NODE_SST282 | 62-69, 77-79, 89-90 | 13 | ✅ |
| Dynamics | SITUATION_NODE_DYNAMICS | 70-76 | 7 | ✅ |
| Overdrive | SITUATION_NODE_OVERDRIVE | 80-83 | 4 | ✅ |
| Phaser | SITUATION_NODE_PHASER | 84-88 | 5 | ✅ |
| Reverb | SITUATION_NODE_REVERB | 91-95 | 5 | ✅ |
| Exciter | SITUATION_NODE_EXCITER | 96-99 | 4 | ✅ |
| Mastering Amp | SITUATION_NODE_MASTERING_AMP | 3-11, 19-21, 100-101, 118-119 | 15 | ✅ |
| Maximizer | SITUATION_NODE_MAXIMIZER | 22-31, 65-69, 77-79 | 18 (4 bands × 4 + 2) | ✅ |
| Panner | SITUATION_NODE_PANNER | 10 | 1 | ✅ |
| Chorus | SITUATION_NODE_CHORUS | 12-15 | 4 | ✅ |
| LFO | SITUATION_NODE_LFO | 102-103 | 2 | ✅ |
| Studio Reverb | SITUATION_NODE_STUDIO_REVERB | 104-111 | 8 | ✅ |
| Spring Reverb | SITUATION_NODE_SPRING_REVERB | 112-117 | 6 | ✅ |

**Total: 17 devices with MIDI control, 133 parameters**
