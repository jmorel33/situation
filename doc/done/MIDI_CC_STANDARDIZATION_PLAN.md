# MIDI CC Standardization Plan

**Date**: March 9, 2026  
**Priority**: HIGH  
**Estimated Effort**: 1 day  
**Target Version**: v2.5.0 (before MIDI Learn)

---

## Problem Statement

Currently, each device type uses different CC numbers to avoid conflicts:
- Compander: CC 16-39 (24 controls)
- Dynamics: CC 70-76 (7 controls)
- Filter: CC 40-45 (6 controls)
- EQ 4-Band: CC 46-57 (12 controls)
- Reverb: CC 91-95 (5 controls)
- etc.

**This is unnecessary!** Since each device instance gets its own `SIT_MidiDevice` with independent MIDI routing, they can all use the same CC numbers (e.g., CC 1-127).

---

## Benefits of Standardization

1. **Intuitive Mapping**: CC 1 = first parameter, CC 2 = second parameter, etc.
2. **Hardware Controller Friendly**: Standard MIDI controllers use CC 1-8 for knobs/faders
3. **Easier to Remember**: No need to memorize which device uses which CC range
4. **MIDI Learn Ready**: Simplifies MIDI Learn implementation
5. **Better Documentation**: Clearer parameter → CC mapping
6. **Standard MIDI CCs**: Can use standard CCs where appropriate (CC 7 = Volume, CC 10 = Pan, etc.)

---

## Proposed Standard CC Mapping

### General Purpose Parameters (All Devices)

Use standard MIDI CCs where semantically appropriate:

```
CC 1   → Modulation / Primary Control (e.g., Filter Cutoff, LFO Rate)
CC 2   → Breath Controller / Secondary Control (e.g., Resonance, Depth)
CC 7   → Volume / Level / Gain
CC 10  → Pan
CC 11  → Expression / Intensity
CC 64  → Sustain Pedal / On-Off Toggle
CC 71  → Resonance / Timbre
CC 72  → Release Time
CC 73  → Attack Time
CC 74  → Brightness / Cutoff
CC 75  → Decay Time
CC 76  → Vibrato Rate
CC 77  → Vibrato Depth
CC 78  → Vibrato Delay
```

### Device-Specific Parameters

Start from CC 16 (General Purpose Controllers):

```
CC 16-31  → Device-specific parameters (16 slots)
CC 32-63  → LSB for 14-bit control (if needed)
CC 80-90  → Additional parameters (11 slots)
CC 102-119 → Extended parameters (18 slots)
```

---

## Standardized CC Mappings by Device

### Compander (24 controls → 3 bands × 8 params)

**Band 0 (Low)**:
```
CC 16 → comp_thresh (0.0-1.0)
CC 17 → exp_thresh (0.0-1.0)
CC 18 → comp_slope (1.0-10.0)
CC 19 → exp_slope (1.0-10.0)
CC 20 → noise_gate (-96dB to 0dB)
CC 21 → bell_freq (20Hz-200Hz, log)
CC 22 → bell_gain (-24dB to +24dB)
CC 23 → bell_Q (0.1-10.0)
```

**Band 1 (Mid)**:
```
CC 24 → comp_thresh (0.0-1.0)
CC 25 → exp_thresh (0.0-1.0)
CC 26 → comp_slope (1.0-10.0)
CC 27 → exp_slope (1.0-10.0)
CC 28 → noise_gate (-96dB to 0dB)
CC 29 → bell_freq (200Hz-4000Hz, log)
CC 30 → bell_gain (-24dB to +24dB)
CC 31 → bell_Q (0.1-10.0)
```

**Band 2 (High)**:
```
CC 80 → comp_thresh (0.0-1.0)
CC 81 → exp_thresh (0.0-1.0)
CC 82 → comp_slope (1.0-10.0)
CC 83 → exp_slope (1.0-10.0)
CC 84 → noise_gate (-96dB to 0dB)
CC 85 → bell_freq (4000Hz-20000Hz, log)
CC 86 → bell_gain (-24dB to +24dB)
CC 87 → bell_Q (0.1-10.0)
```

### Dynamics (7 controls)

```
CC 16 → mode (0=comp, 1=limit, 2=gate, 3=expand)
CC 17 → threshold_db (-60dB to 0dB)
CC 18 → ratio (1.0-20.0)
CC 73 → attack_ms (0.1-100ms, log) [Standard MIDI Attack]
CC 72 → release_ms (10-1000ms, log) [Standard MIDI Release]
CC 19 → knee_db (0-12dB)
CC 7  → makeup_gain_db (0-24dB) [Standard MIDI Volume]
```

### Filter (6 controls)

```
CC 16 → type (0=LP, 1=HP, 2=BP, 3=notch, 4=peak, 5=LS, 6=HS)
CC 74 → cutoff (20Hz-20000Hz, log) [Standard MIDI Brightness]
CC 71 → resonance/Q (0.1-10.0) [Standard MIDI Resonance]
CC 7  → gain_db (-24dB to +24dB) [Standard MIDI Volume]
CC 17 → drive (1.0-10.0)
CC 18 → oversampling (0=off, 1=2x, 2=4x)
```

### EQ 4-Band (12 controls → 4 bands × 3 params)

```
Band 0 (Low):
  CC 16 → freq (20Hz-500Hz, log)
  CC 17 → Q (0.1-10.0)
  CC 18 → gain (-24dB to +24dB)

Band 1 (Low-Mid):
  CC 19 → freq (200Hz-2000Hz, log)
  CC 20 → Q (0.1-10.0)
  CC 21 → gain (-24dB to +24dB)

Band 2 (High-Mid):
  CC 22 → freq (1000Hz-8000Hz, log)
  CC 23 → Q (0.1-10.0)
  CC 24 → gain (-24dB to +24dB)

Band 3 (High):
  CC 25 → freq (4000Hz-20000Hz, log)
  CC 26 → Q (0.1-10.0)
  CC 27 → gain (-24dB to +24dB)
```

### Reverb (5 controls)

```
CC 16 → room_size (0.0-1.0)
CC 17 → damp (0.0-1.0)
CC 18 → wet (0.0-1.0)
CC 19 → dry (0.0-1.0)
CC 20 → width (0.0-1.0)
```

### Chorus (4 controls)

```
CC 76 → rate (0.1Hz-10Hz, log) [Standard MIDI Vibrato Rate]
CC 77 → depth (0.0-1.0) [Standard MIDI Vibrato Depth]
CC 16 → feedback (0.0-0.9)
CC 17 → mix (0.0-1.0)
```

### Overdrive (4 controls)

```
CC 16 → mode (0=soft, 1=hard, 2=fuzz, 3=tube)
CC 17 → drive (1.0-100.0, log)
CC 18 → tone (0.0-1.0)
CC 7  → level (0.0-2.0) [Standard MIDI Volume]
```

### Panner (1 control)

```
CC 10 → pan (-1.0 to +1.0) [Standard MIDI Pan]
```

### LFO (2 controls)

```
CC 16 → waveform (0=sine, 1=tri, 2=saw, 3=square, 4=random)
CC 17 → frequency (0.01Hz-20Hz, log)
```

### Echo (4 controls)

```
CC 16 → delay_time (10ms-2000ms, log)
CC 17 → feedback (0.0-0.95)
CC 18 → wet (0.0-1.0)
CC 19 → dry (0.0-1.0)
```

### Phaser (5 controls)

```
CC 76 → rate (0.1Hz-10Hz, log) [Standard MIDI Vibrato Rate]
CC 77 → depth (0.0-1.0) [Standard MIDI Vibrato Depth]
CC 16 → feedback (0.0-0.95)
CC 17 → stages (2-12)
CC 18 → mix (0.0-1.0)
```

### Exciter (4 controls)

```
CC 16 → frequency (1000Hz-10000Hz, log)
CC 17 → harmonics (0.0-1.0)
CC 18 → blend (0.0-1.0)
CC 19 → drive (1.0-10.0)
```

### Studio Reverb (8 controls)

```
CC 16 → pre_delay (0ms-100ms)
CC 17 → room_size (0.0-1.0)
CC 18 → damping (0.0-1.0)
CC 19 → diffusion (0.0-1.0)
CC 20 → decay_time (0.1s-10s, log)
CC 21 → early_reflections (0.0-1.0)
CC 22 → wet (0.0-1.0)
CC 23 → dry (0.0-1.0)
```

### Spring Reverb (6 controls)

```
CC 16 → spring_tension (0.0-1.0)
CC 17 → spring_damping (0.0-1.0)
CC 18 → spring_length (0.0-1.0)
CC 19 → drive (1.0-10.0)
CC 20 → wet (0.0-1.0)
CC 21 → dry (0.0-1.0)
```

### SST-282 (13 controls)

```
CC 16 → input_gain (0.0-2.0)
CC 17 → lf_cut_db (-12dB to +12dB)
CC 18 → hf_cut_db (-12dB to +12dB)
CC 19 → tap_level_0 (0.0-1.0)
CC 20 → tap_level_1 (0.0-1.0)
CC 21 → tap_level_2 (0.0-1.0)
CC 22 → tap_level_3 (0.0-1.0)
CC 23 → feedback (0.0-0.95)
CC 24 → dry_db (-60dB to 0dB)
CC 25 → echo_db (-60dB to 0dB)
CC 26 → direct_db (-60dB to 0dB)
CC 27 → mode (0=normal, 1=reverse)
CC 28 → echo_delay (0ms-500ms)
```

### Mastering Amp (15 controls)

```
CC 16 → amp_type (0=clean, 1=warm, 2=vintage, 3=modern)
CC 17 → drive (0.0-10.0)
CC 18 → low_freq (20Hz-500Hz, log)
CC 19 → low_gain (-24dB to +24dB)
CC 20 → mid_freq (200Hz-5kHz, log)
CC 21 → mid_gain (-24dB to +24dB)
CC 22 → mid_q (0.1-10.0)
CC 23 → high_freq (2kHz-20kHz, log)
CC 24 → high_gain (-24dB to +24dB)
CC 25 → air_freq (8kHz-20kHz, log)
CC 26 → air_gain (-12dB to +12dB)
CC 27 → tight_cutoff (20Hz-200Hz, log)
CC 28 → aspect_ratio (0.0-1.0)
CC 64 → vintage (0=off, 1=on) [Standard MIDI Sustain]
CC 65 → circuit_bending (0=off, 1=on)
```

### Maximizer (18 controls → 4 bands × 4 params + 2 filters)

```
Band 0 (Low):
  CC 16 → freq (20Hz-500Hz, log)
  CC 17 → threshold (0.0-2.0)
  CC 18 → ratio (1.0-10.0)
  CC 19 → attack (1-10)

Band 1 (Low-Mid):
  CC 20 → freq (200Hz-2kHz, log)
  CC 21 → threshold (0.0-2.0)
  CC 22 → ratio (1.0-10.0)
  CC 23 → attack (1-10)

Band 2 (High-Mid):
  CC 24 → freq (1kHz-8kHz, log)
  CC 25 → threshold (0.0-2.0)
  CC 26 → ratio (1.0-10.0)
  CC 27 → attack (1-10)

Band 3 (High):
  CC 28 → freq (4kHz-20kHz, log)
  CC 29 → threshold (0.0-2.0)
  CC 30 → ratio (1.0-10.0)
  CC 31 → attack (1-10)

Filters:
  CC 80 → hpf_cutoff (20Hz-500Hz, log)
  CC 81 → lpf_cutoff (5kHz-20kHz, log)
```

---

## Implementation Plan

### Phase 1: Update Callback Definitions (2 hours)

1. **Edit `sit/aud/midi_device_callbacks.h`**:
   - Update all `_Situation*OnControlChange()` functions
   - Change CC number checks to new standardized values
   - Add comments documenting the new mapping

2. **Verify all 17 device types**:
   - Compander ✓
   - Dynamics ✓
   - Filter ✓
   - EQ 4-Band ✓
   - Reverb ✓
   - Chorus ✓
   - Overdrive ✓
   - Panner ✓
   - LFO ✓
   - Echo ✓
   - Phaser ✓
   - Exciter ✓
   - Studio Reverb ✓
   - Spring Reverb ✓
   - SST-282 ✓
   - Mastering Amp ✓
   - Maximizer ✓

### Phase 2: Update Documentation (1 hour)

1. **Update `doc/midi_api.md`**:
   - Replace all CC mapping tables
   - Add section explaining standardization
   - Document standard MIDI CC usage

2. **Update callback header comments**:
   - Ensure each callback has clear CC mapping documentation
   - Add rationale for standard CC choices

### Phase 3: Update Examples (1 hour)

1. **Update `examples/midi_compander_control.c`**:
   - Change CC numbers in documentation
   - Update any hardcoded CC references

2. **Update `examples/midi_14bit_example.c`**:
   - Adjust to new CC mapping

3. **Update any other examples** that reference specific CCs

### Phase 4: Add Device Auto-Assignment (2 hours)

Add to MIDI Learn plan:

```c
// In midi_learn.h

/**
 * @brief Auto-select first available MIDI input device.
 * @return Device ID or PM_NO_DEVICE if none available.
 */
PmDeviceID SIT_MidiLearn_AutoSelectInput(void);

/**
 * @brief List all available MIDI input devices.
 * @param devices Output array for device info.
 * @param max_count Maximum number of devices to return.
 * @return Number of devices found.
 */
int SIT_MidiLearn_ListInputDevices(SIT_MidiLearnDeviceInfo* devices, int max_count);

/**
 * @brief Set which MIDI device to learn from.
 * @param state MIDI Learn state.
 * @param device_id Device ID to use for learning.
 */
void SIT_MidiLearn_SetInputDevice(SIT_MidiLearnState* state, PmDeviceID device_id);
```

### Phase 5: Testing (2 hours)

1. **Test each device type** with new CC mappings
2. **Test with hardware MIDI controller**
3. **Verify no regressions** in existing functionality
4. **Update test programs** if needed

---

## Migration Guide

### For Users

**Old Way** (Compander):
```
CC 16 → Band 0 comp_thresh
CC 24 → Band 1 comp_thresh
CC 32 → Band 2 comp_thresh
```

**New Way** (Compander):
```
CC 16 → Band 0 comp_thresh
CC 24 → Band 1 comp_thresh
CC 80 → Band 2 comp_thresh
```

**Impact**: Existing MIDI mappings will need to be updated. This is acceptable since:
- MIDI Learn will make remapping easy
- Presets can be updated programmatically
- New mappings are more intuitive

### For Developers

**Old Code**:
```c
// Compander used CC 16-39
// Filter used CC 40-45
// Had to remember which device used which range
```

**New Code**:
```c
// All devices start from CC 16
// Standard CCs used where appropriate (CC 7 = Volume, CC 10 = Pan)
// Much easier to remember and document
```

---

## Benefits Summary

1. **Simpler**: All devices use similar CC ranges
2. **Standard**: Uses standard MIDI CCs where appropriate
3. **Intuitive**: CC 16 = first param, CC 17 = second param
4. **Hardware Friendly**: Works with standard MIDI controllers
5. **MIDI Learn Ready**: Simplifies implementation
6. **Better UX**: Users don't need to memorize CC ranges per device

---

## Timeline

- **Phase 1**: 2 hours (Update callbacks)
- **Phase 2**: 1 hour (Update docs)
- **Phase 3**: 1 hour (Update examples)
- **Phase 4**: 2 hours (Add auto-assignment)
- **Phase 5**: 2 hours (Testing)

**Total**: 8 hours (1 day)

---

## Approval Checklist

- ✅ Standardized CC mapping defined
- ✅ All 17 device types mapped
- ✅ Standard MIDI CCs used appropriately
- ✅ Implementation plan outlined
- ✅ Migration guide provided
- ✅ Testing strategy defined

---

## Next Steps

1. **Get approval** on standardized CC mapping
2. **Implement Phase 1-3** (update code and docs)
3. **Implement Phase 4** (auto-assignment for MIDI Learn)
4. **Test thoroughly**
5. **Update MIDI Learn plan** to use new mappings

---

**Status**: 📋 Ready for Implementation  
**Blocks**: MIDI Learn implementation (should be done first)  
**Estimated Completion**: 1 day
