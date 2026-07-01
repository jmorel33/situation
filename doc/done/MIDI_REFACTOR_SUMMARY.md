# MIDI System Refactor Summary

**Date**: March 9, 2026  
**Status**: 📋 Ready for Implementation  
**Priority**: HIGH (blocks MIDI Learn)

---

## The Problem

You correctly identified a design flaw: **Each device type uses different CC numbers to avoid conflicts, but this is unnecessary!**

### Current (Inefficient) Design

```
Compander:     CC 16-39  (24 controls)
Dynamics:      CC 70-76  (7 controls)
Filter:        CC 40-45  (6 controls)
EQ 4-Band:     CC 46-57  (12 controls)
Reverb:        CC 91-95  (5 controls)
Chorus:        CC 12-15  (4 controls)
Overdrive:     CC 80-83  (4 controls)
... etc (17 devices total)
```

**Why this is silly**: Each device instance gets its own `SIT_MidiDevice` with independent MIDI routing. They can all use the SAME CC numbers!

---

## The Solution

### Two-Part Refactor

1. **MIDI CC Standardization** (1 day)
   - All devices use standardized CC ranges
   - Start from CC 16 (General Purpose Controllers)
   - Use standard MIDI CCs where appropriate (CC 7 = Volume, CC 10 = Pan)
   - Much simpler and more intuitive

2. **MIDI Learn with Auto Device Assignment** (2-3 days)
   - Automatic MIDI device selection
   - Dynamic CC → parameter mapping
   - Save/load presets
   - 14-bit CC support

---

## Benefits

### For Users

1. **Intuitive**: CC 16 = first parameter, CC 17 = second parameter
2. **Hardware Friendly**: Works with standard MIDI controllers (knobs 1-8)
3. **Easy to Remember**: No need to memorize CC ranges per device
4. **Standard MIDI**: Uses familiar CCs (CC 7 = Volume, CC 10 = Pan)
5. **MIDI Learn**: Can easily remap any CC to any parameter

### For Developers

1. **Simpler Code**: All devices follow same pattern
2. **Better Documentation**: Clear, consistent mappings
3. **Easier Testing**: Test with standard MIDI controller
4. **Less Confusion**: No need to track which device uses which CC range

---

## Example: Before vs After

### Before (Compander)

```c
// Band 0: CC 16-23
// Band 1: CC 24-31
// Band 2: CC 32-39

// Had to remember: "Compander uses CC 16-39"
// Filter uses different range: CC 40-45
// Reverb uses yet another range: CC 91-95
```

### After (All Devices)

```c
// All devices start from CC 16
// Standard CCs used where appropriate

// Compander:
CC 16 → Band 0 comp_thresh
CC 17 → Band 0 exp_thresh
...

// Filter:
CC 16 → type
CC 74 → cutoff (Standard MIDI Brightness)
CC 71 → resonance (Standard MIDI Resonance)

// Reverb:
CC 16 → room_size
CC 17 → damp
...
```

---

## Implementation Order

### Step 1: MIDI CC Standardization (FIRST)

**File**: `doc/plan/MIDI_CC_STANDARDIZATION_PLAN.md`

**Tasks**:
1. Update `sit/aud/midi_device_callbacks.h` (all 17 devices)
2. Update `doc/midi_api.md` (documentation)
3. Update examples (`midi_compander_control.c`, etc.)
4. Test with hardware MIDI controller

**Time**: 1 day (8 hours)

### Step 2: MIDI Learn Implementation (SECOND)

**File**: `doc/plan/MIDI_LEARN_PLAN.md`

**Tasks**:
1. **Phase 1**: Core learning + auto device selection (Day 1)
2. **Phase 2**: Presets and management (Day 2)
3. **Phase 3**: 14-bit support and polish (Day 3)

**Time**: 2-3 days

---

## Key Additions to MIDI Learn

### Automatic Device Selection

```c
// Auto-select first available MIDI input
PmDeviceID device = SIT_MidiLearn_AutoSelectInput();

// Or list all devices for user selection
SIT_MidiLearnDeviceInfo devices[32];
int count = SIT_MidiLearn_ListInputDevices(devices, 32);

// Set device for learning
SIT_MidiLearn_SetInputDevice(learn_state, device);
```

### No More Manual Setup

**Before** (manual):
```c
// Find device
for (int i = 0; i < Pm_CountDevices(); i++) {
    if (Pm_GetDeviceInfo(i)->input) {
        input_device = i;
        break;
    }
}

// Open stream
Pm_OpenInput(&midi_in, input_device, NULL, 512, NULL, NULL);

// Process in audio callback
PmEvent events[32];
int count = Pm_Read(midi_in, events, 32);
// ... dispatch manually
```

**After** (automatic):
```c
// Just enable MIDI Learn
fx->midi_device->learn_state = SIT_MidiLearn_Create();
SIT_MidiLearn_AutoSelectInput();  // Done!
```

---

## Migration Impact

### Breaking Changes

- Existing MIDI mappings will need to be updated
- Presets with hardcoded CC numbers will break

### Mitigation

1. **MIDI Learn makes remapping easy**: Just click "Learn" and move controller
2. **Better long-term**: More intuitive, easier to use
3. **Document migration**: Provide CC mapping conversion table
4. **Version presets**: Can detect old format and warn user

---

## Files to Modify

### Core Implementation

1. `sit/aud/midi_device_callbacks.h` - Update all 17 device callbacks
2. `sit/aud/midi_device.h` - Add `learn_state` field
3. `sit/aud/midi_learn.h` - New file (MIDI Learn API)
4. `sit/aud/midi_learn.c` - New file (MIDI Learn implementation)

### Documentation

1. `doc/midi_api.md` - Update all CC mapping tables
2. `doc/plan/MIDI_CC_STANDARDIZATION_PLAN.md` - New file (this refactor)
3. `doc/plan/MIDI_LEARN_PLAN.md` - Updated with auto-assignment

### Examples

1. `examples/midi_compander_control.c` - Update CC numbers
2. `examples/midi_14bit_example.c` - Update CC numbers
3. `examples/midi_learn_basic.c` - New file
4. `examples/midi_learn_presets.c` - New file
5. `examples/midi_learn_14bit.c` - New file

---

## Testing Strategy

### Phase 1: CC Standardization

1. Test each device type with new CC mappings
2. Verify with hardware MIDI controller
3. Check all 17 devices work correctly
4. Update and run all MIDI examples

### Phase 2: MIDI Learn

1. Test auto device selection
2. Test learning with each device type
3. Test preset save/load
4. Test 14-bit CC learning
5. Test conflict detection
6. Test timeout behavior

---

## Timeline

| Task | Time | Dependencies |
|------|------|--------------|
| CC Standardization | 1 day | None |
| MIDI Learn Phase 1 | 1 day | CC Standardization |
| MIDI Learn Phase 2 | 1 day | Phase 1 |
| MIDI Learn Phase 3 | 1 day | Phase 2 |
| **Total** | **4 days** | Sequential |

---

## Success Metrics

### CC Standardization

- ✅ All 17 devices use standardized CC ranges
- ✅ Standard MIDI CCs used appropriately
- ✅ Documentation updated
- ✅ Examples work with new mappings
- ✅ No regressions in existing functionality

### MIDI Learn

- ✅ Auto device selection works
- ✅ Can learn any CC to any parameter
- ✅ Presets save/load correctly
- ✅ 14-bit CCs detected automatically
- ✅ No audio glitches during learning
- ✅ Thread-safe operation

---

## Approval Status

- ✅ Problem identified and validated
- ✅ Solution designed
- ✅ Implementation plan created
- ✅ Timeline estimated
- ✅ Testing strategy defined
- ⏳ **Awaiting user approval to proceed**

---

## Next Steps

1. **Get approval** on this refactor plan
2. **Implement CC Standardization** (1 day)
3. **Test thoroughly** with hardware
4. **Implement MIDI Learn** (2-3 days)
5. **Update all documentation**
6. **Release as v2.5.0**

---

**Recommendation**: This refactor is essential for a good MIDI Learn implementation. The current CC assignment scheme is indeed "silly" (your words!) and should be fixed before adding MIDI Learn.

**Confidence**: HIGH (95%) - This is a straightforward refactor with clear benefits.

---

**Created**: March 9, 2026  
**Author**: Kiro AI  
**Status**: Ready for Implementation
