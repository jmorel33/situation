# v2.4.0 API Cleanup - CRITICAL

## Problem

The node graph and device registry public types are currently defined in BOTH places:
1. `situation_api.h` (correct - where users look)
2. Internal headers like `sit/aud/device_registry.h`, `sit/aud/node_graph.h` (incorrect - causes redefinition errors)

This violates the library architecture principle: **Users should only need to look at `situation_api.h` for all public types and APIs.**

## Current Status

✅ **Completed:**
- All public types added to `situation_api.h` (lines ~1586-1760)
- All public API functions declared in `situation_api.h` (lines ~2638-2670)
- Graph serialization implementation included in `situation_impl_audio.h`
- Compilation scripts updated for graph demos
- Demo code updated to use public API

❌ **Remaining Issue:**
- Internal headers still define the same types, causing compilation errors
- File `sit/aud/device_registry.h` partially cleaned but corrupted during edit

## Required Fix

### Files to Clean:

1. **sit/aud/device_registry.h**
   - Remove: All type definitions (enums, structs) from lines ~50-220
   - Keep: Only implementation code (static functions, global storage)
   - Add: Comment directing users to `situation_api.h` for public types

2. **sit/aud/node_graph.h**
   - Remove: All type definitions (SituationNodeHandle, SituationAudioPort, SituationPatch, etc.)
   - Keep: Only implementation code
   - Add: Comment directing users to `situation_api.h`

3. **sit/aud/graph_serialization.h**
   - Remove: Forward declaration of `SituationDeviceFunctions` (now in situation_api.h)
   - Keep: Only function declarations (if any remain that aren't in situation_api.h)

4. **sit/aud/node_graph_process.h**
   - Remove: `SituationDeviceFunctions` struct definition (now in situation_api.h)
   - Keep: Only implementation code

### Template for Cleaned Internal Header:

```c
/***************************************************************************************************
*
*   sit/aud/[filename].h - [Module] Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   INTERNAL IMPLEMENTATION FILE
*   
*   PUBLIC TYPES AND API: All public types, enums, and function declarations are in situation_api.h
*   This file contains ONLY implementation code (static functions, internal helpers).
*   
*   Users should never include this file directly - it's included automatically via situation_impl.h
*   
***************************************************************************************************/

#ifndef SITUATION_[MODULE]_H
#define SITUATION_[MODULE]_H

#include <stdint.h>
#include <stdbool.h>

// NOTE: All public types are defined in situation_api.h
// This file contains only implementation code

// ================================================================================================
// IMPLEMENTATION
// ================================================================================================

[Keep only static functions and internal helpers here]

#endif
```

## Why This Matters

1. **User Experience**: Users expect to find all public APIs in ONE place (`situation_api.h`)
2. **Compilation**: Duplicate definitions cause redefinition errors
3. **Maintainability**: Single source of truth for public types
4. **Professionalism**: Clean separation of public API vs internal implementation

## Next Steps

1. Restore `sit/aud/device_registry.h` from a clean state or rebuild it
2. Apply the template above to each internal header
3. Test compilation of all demos
4. Verify no redefinition errors

## Files Affected

- `sit/aud/device_registry.h` (263 lines - needs cleanup)
- `sit/aud/node_graph.h` (~300 lines - needs cleanup)
- `sit/aud/node_graph_process.h` (has SituationDeviceFunctions duplicate)
- `sit/aud/graph_serialization.h` (has forward declaration duplicate)

This must be completed before v2.4.0 release.
