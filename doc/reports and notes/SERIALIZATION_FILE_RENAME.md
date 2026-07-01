# Serialization File Rename - Consistent Naming Convention

**Date**: 2026-03-03  
**Status**: ✅ COMPLETE

## Problem

The node graph subsystem had inconsistent file naming:
- `node_graph.h` - Base types
- `node_graph_impl.h` - Topology implementation
- `node_graph_process.h` - Audio processing
- `graph_serialization.h` ❌ - Missing `node_graph_` prefix
- `graph_serialization_impl.h` ❌ - Missing `node_graph_` prefix

This made it unclear that the serialization files were part of the node graph subsystem.

## Solution

Renamed files to follow consistent `node_graph_*` naming convention:

```
graph_serialization.h      → node_graph_serialization.h
graph_serialization_impl.h → node_graph_serialization_impl.h
```

## Files Modified

### Renamed Files:
1. **sit/aud/graph_serialization.h** → **sit/aud/node_graph_serialization.h**
   - Updated header guard: `SITUATION_GRAPH_SERIALIZATION_H` → `SITUATION_NODE_GRAPH_SERIALIZATION_H`
   - Updated endif comment

2. **sit/aud/graph_serialization_impl.h** → **sit/aud/node_graph_serialization_impl.h**
   - Updated header guard: `SITUATION_GRAPH_SERIALIZATION_IMPL_H` → `SITUATION_NODE_GRAPH_SERIALIZATION_IMPL_H`
   - Updated include: `#include "graph_serialization.h"` → `#include "node_graph_serialization.h"`
   - Updated endif comment

### Updated References:
3. **situation_impl_audio.h**
   - Updated include: `#include "sit/aud/graph_serialization_impl.h"` → `#include "sit/aud/node_graph_serialization_impl.h"`

## Final Node Graph File Structure

All node graph files now follow consistent naming:

```
sit/aud/
├── node_graph.h                        - Base types and structures
├── node_graph_impl.h                   - Graph topology (create/destroy nodes, patches)
├── node_graph_process.h                - Real-time audio processing
├── node_graph_serialization.h          - Save/load API declarations
└── node_graph_serialization_impl.h     - Save/load implementation
```

## Clear Separation of Concerns

Each file has a distinct purpose:

1. **node_graph.h** (Types)
   - `SituationAudioGraph`, `SituationNode`, `SituationPatch`
   - `SituationNodeHandle`, `SituationNodeType`
   - Port and control structures

2. **node_graph_impl.h** (Topology - 690 lines)
   - `SituationCreateGraph()`, `SituationDestroyGraph()`
   - `SituationCreateNode()`, `SituationDestroyNode()`
   - `SituationCreatePatch()`, `SituationRemovePatch()`
   - `SituationSetControl()`, `SituationGetControl()`
   - `SituationTopologicalSort()`

3. **node_graph_process.h** (Processing - 415 lines)
   - `SituationProcessGraph()` - Real-time audio callback
   - Device function types: `SituationProcessFunc`, `SituationCreateFunc`, `SituationDestroyFunc`
   - Buffer operations: `_SituationZeroBuffer()`, `_SituationSumBuffers()`, `_SituationCopyBuffer()`
   - Device helpers: `SituationCreateNodeWithDevice()`, `SituationDestroyNodeWithDevice()`

4. **node_graph_serialization.h** (Serialization API - 123 lines)
   - `SituationSaveGraphToFile()`, `SituationLoadGraphFromFile()`
   - `SituationSerializeGraphToJSON()`, `SituationDeserializeGraphFromJSON()`
   - `SituationFreeJSONString()`, `SituationGetSerializationVersion()`

5. **node_graph_serialization_impl.h** (Serialization Implementation - 995 lines)
   - JSON parser/writer
   - Node/patch serialization
   - Control value persistence
   - Version tracking

## Verification

✅ `compile_graph_save_demo.bat` - Compiles and runs successfully  
✅ `compile_graph_load_demo.bat` - Compiles and runs successfully  
✅ Round-trip serialization test passes  
✅ All node graph functionality working  

## Result

The node graph subsystem now has consistent, professional naming that clearly indicates all related files are part of the same module. The `node_graph_*` prefix makes it immediately obvious which files belong together.
