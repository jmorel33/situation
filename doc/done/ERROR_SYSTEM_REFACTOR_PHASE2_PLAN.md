# Error System Refactor - Phase 2 Plan

## Goal

Migrate the audio subsystem to use the new `SituationErrorEx` system, starting with the most recent code (mixer insert integration) and working backwards through the audio stack.

## Migration Strategy

### Step 1: Update Mixer Insert Integration (Newest Code)
- ✅ All error constants already defined in `situation_error_ex_audio.h`
- Update `sit/aud/mixer_insert_integration.h` function signatures
- Update `sit/aud/mixer_insert_integration_impl.h` implementations
- Update `examples/mixer_insert_demo.c` to use new error system
- Test compilation and execution

### Step 2: Update Audio Error Mapping Helper
- Update `sit/aud/audio_error_mapping.h` to provide conversion functions
- Add `SituationNodeErrorToEx()` and `SituationRegistryErrorToEx()`
- Keep legacy mapping functions for backward compatibility

### Step 3: Update Node Graph Threading
- Update `sit/aud/node_graph_threading.h` function signatures
- Update `sit/aud/node_graph_threading_impl.h` implementations
- Maintain compatibility with legacy `SituationNodeError` internally

### Step 4: Validate and Test
- Compile all audio examples
- Run mixer insert demo
- Run node graph threading tests
- Verify error messages are correct

## Implementation Details

### Function Signature Changes

**Before:**
```c
SituationError SituationMixerAttachInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    SituationThreadSafeGraph* insert_chain
);
```

**After:**
```c
SituationErrorEx SituationMixerAttachInsert(
    SituationAudioMixer* mixer,
    int track_id,
    SituationInsertPosition position,
    SituationThreadSafeGraph* insert_chain
);
```

### Error Return Changes

**Before:**
```c
if (!mixer) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
if (!mixer->is_initialized) return SITUATION_ERROR_MIXER_NOT_INITIALIZED;
```

**After:**
```c
if (!mixer) return SITUATION_ERROR_EX_MIXER_NOT_INITIALIZED;
if (!mixer->is_initialized) return SITUATION_ERROR_EX_MIXER_NOT_INITIALIZED;
```

### Error Checking Changes

**Before:**
```c
SituationError err = SituationMixerAttachInsert(...);
if (err != SITUATION_SUCCESS) {
    printf("Error: %d\n", err);
}
```

**After:**
```c
SituationErrorEx err = SituationMixerAttachInsert(...);
if (SITUATION_ERROR_EX_IS_FAILURE(err)) {
    printf("Error %d: %s\n", err.code, err.message);
}
```

## Files to Update

### Priority 1: Mixer Insert Integration
1. `sit/aud/mixer_insert_integration.h` - Function declarations
2. `sit/aud/mixer_insert_integration_impl.h` - Function implementations
3. `examples/mixer_insert_demo.c` - Demo application

### Priority 2: Error Mapping
4. `sit/aud/audio_error_mapping.h` - Add conversion functions

### Priority 3: Testing
5. Compile and test all changes
6. Verify error messages
7. Document any issues

## Backward Compatibility

### Internal Audio Code
- Legacy `SituationNodeError` and `SituationRegistryError` remain unchanged
- Conversion functions bridge old and new systems
- No breaking changes to internal APIs

### Public API
- New functions use `SituationErrorEx`
- Old functions remain unchanged (if any exist)
- Gradual migration path

## Success Criteria

- ✅ All mixer insert functions use `SituationErrorEx`
- ✅ Demo compiles without warnings
- ✅ Demo runs and shows correct error messages
- ✅ Error messages are more informative than before
- ✅ No performance regression
- ✅ Backward compatibility maintained

## Timeline

- Step 1: 30 minutes (Update mixer insert integration)
- Step 2: 15 minutes (Update error mapping)
- Step 3: 15 minutes (Testing and validation)
- **Total: ~1 hour**

## Next Steps After Phase 2

Once mixer insert integration is migrated:
1. Migrate node graph threading functions
2. Migrate device registry functions
3. Migrate serialization functions
4. Eventually migrate core audio functions

## Notes

- Start with newest code (mixer insert) because it's fresh in memory
- Use it as a template for migrating older code
- Validate thoroughly before moving to next component
- Document any issues or improvements discovered

---

**Status**: Ready to begin  
**Date**: 2026-03-03
