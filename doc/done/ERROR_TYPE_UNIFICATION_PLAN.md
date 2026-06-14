# Error Type Unification Plan

**Date**: 2026-03-03  
**Status**: PLANNED  
**Priority**: HIGH (v2.4.0 release requirement)

---

## Objective

Unify all audio subsystem error codes into the main `SituationError` enum for consistency across the entire library.

**User Requirement**: "these should be part of the SituationError enum not separate, and the modules should all use SituationError"

---

## Current State

The audio subsystem currently uses two separate error enums:
- `SituationRegistryError` (8 error codes)
- `SituationNodeError` (14 error codes)

These are redundant with the main `SituationError` enum which already has dedicated sections for:
- Audio Node Graph Errors (-460 to -479)
- Audio Device Registry Errors (-480 to -499)

---

## Changes Required

### Phase 1: API Layer (situation_api.h) ✅ COMPLETE

1. ✅ Remove `SituationRegistryError` enum
2. ✅ Remove `SituationNodeError` enum
3. ✅ Update all API function declarations to use `SituationError`
4. ✅ Consolidate error message functions into `SituationGetErrorMessage()`

### Phase 2: Implementation Layer (PENDING)

#### Files to Update:

1. **sit/aud/device_registry.h**
   - Change return type: `SituationRegistryError` → `SituationError`
   - Update all error codes to use SITUATION_ERROR_DEVICE_* constants
   - Update `SituationGetRegistryErrorMessage()` to use main error system

2. **sit/aud/node_graph.h**
   - Change return type: `SituationNodeError` → `SituationError`
   - Update all error codes to use SITUATION_ERROR_NODE_* constants

3. **sit/aud/node_graph_impl.h**
   - Update all function return types
   - Replace error code constants throughout
   - Update error handling logic

4. **sit/aud/node_graph_process.h**
   - Update `SituationProcessGraph()` return type
   - Update helper function return types
   - Replace error constants

5. **sit/aud/graph_serialization.h**
   - Update function declarations

6. **sit/aud/graph_serialization_impl.h**
   - Update all error handling
   - Replace error constants

### Phase 3: Demo/Example Code (PENDING)

1. **examples/graph_save_demo.c**
   - Update error type variables
   - Update error checking logic

2. **examples/graph_load_demo.c**
   - Update error type variables
   - Update error checking logic

3. **examples/node_graph_demo.c**
   - Update if used

### Phase 4: Error Message System (PENDING)

Update `_SituationSetErrorFromCode()` in `situation_impl.h` to handle all the new error codes properly.

---

## Error Code Mapping

See `ERROR_TYPE_UNIFICATION_MAPPING.md` for complete mapping table.

---

## Testing Strategy

After each phase:
1. Compile all affected demos
2. Run graph_save_demo
3. Run graph_load_demo
4. Verify error messages are correct
5. Check that error codes are properly propagated

---

## Risks & Considerations

1. **Breaking Change**: This changes the public API (function signatures)
   - Mitigation: This is for v2.4.0 which is a major release
   
2. **Large Scope**: Many files need updates
   - Mitigation: Systematic approach, one file at a time
   
3. **Error Code Conflicts**: Need to ensure no duplicate error codes
   - Mitigation: Already verified in SituationError enum

---

## Benefits

1. **Consistency**: Single error type across entire library
2. **Simplicity**: Users only need to handle one error enum
3. **Professional**: Matches industry standard practice
4. **Maintainability**: Easier to add new error codes in one place

---

## Next Steps

1. Update device_registry.h implementation
2. Update node_graph implementation files
3. Update serialization implementation
4. Update demo code
5. Test all changes
6. Update documentation
