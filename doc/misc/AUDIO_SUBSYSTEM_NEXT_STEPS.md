# Audio Subsystem: Next Steps and Follow-Up Planning

**Date**: 2026-03-01  
**Library Version**: v2.3.63  
**Current Status**: Phase 3 Complete, Ready for Phase 4

## What We've Accomplished (Phases 1-3)

### Phase 1: Device Registry Foundation ✅
- Complete registry system with metadata storage
- 7 device categories, 4 control types
- Query and validation API
- **Time**: 1 day

### Phase 2: Device Population ✅
- 18 devices registered with complete metadata
- 150+ control parameters documented
- All existing audio devices unified under registry
- **Time**: 1 day

### Phase 3: Node Graph API ✅
- Complete node creation and patching API
- Generational handles for safe references
- Cycle detection and validation
- Control parameter access
- **Time**: 1 day

**Total**: 2,750+ lines of code, 3 days of development

## Current Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DEVICE REGISTRY                          │
│  • 18 registered devices (effects, sources, capture)        │
│  • 150+ control parameters with metadata                    │
│  • Type-safe device lookup and validation                   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                    NODE GRAPH API                           │
│  • Create nodes from registered types                       │
│  • Patch audio and control connections                      │
│  • Validate topology (cycle detection)                      │
│  • Control parameter access with clamping                   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│              REAL-TIME PROCESSING (Phase 4)                 │
│  • Topological sort for evaluation order                    │
│  • Audio processing loop with buffer summing/splitting      │
│  • Device-specific process functions                        │
│  • Thread-safe parameter updates                            │
└─────────────────────────────────────────────────────────────┘
```

## Phase 4: Real-Time Audio Processing (Next)

**Goal**: Bring the node graph to life with real-time audio evaluation.

**Estimated Time**: 4-5 days

### Key Deliverables

1. **Topological Sort Implementation**:
   - Kahn's algorithm for evaluation order
   - Cache sorted order, recompute only on topology changes
   - Handle source nodes (no inputs) and sink nodes (master output)

2. **Audio Processing Loop**:
   - Zero input buffers before processing
   - Sum multiple sources to same input (implicit summing)
   - Split single source to multiple outputs (implicit splitting)
   - Call device-specific process functions
   - Route to master output node

3. **Device Integration**:
   - Implement create/destroy function pointers for all 18 devices
   - Implement process function pointers
   - Connect existing device implementations (reverb, echo, etc.)
   - Handle device-specific state management

4. **Thread Safety**:
   - Snapshot-and-unlock pattern for graph updates
   - Lock-free reads in audio callback
   - Safe parameter updates from UI thread

5. **Master Output Node**:
   - Hidden master output node
   - Automatic summing of unpatched outputs
   - Connection to miniaudio device buffer

### Success Criteria
- ✅ Play sound through Source → Reverb → Output chain
- ✅ Real-time parameter updates without clicks/pops
- ✅ Latency < 10ms at 48kHz (< 480 samples)
- ✅ CPU usage < 20% for typical graph (10-20 nodes)

### Files to Create
- `sit/aud/node_graph_process.h` - Processing loop implementation
- `sit/aud/device_wrappers.h` - Wrapper functions for all 18 devices
- `examples/node_graph_demo.c` - Demo application

## Phase 5: Persistence and Validation (After Phase 4)

**Goal**: Save/load graphs, custom device support, robust error handling.

**Estimated Time**: 2-3 days

### Key Features
1. **Graph Serialization**:
   - JSON or binary format for graph storage
   - Save node types and control values
   - Save patch connections
   - Version compatibility

2. **Custom Device Registration**:
   - API for user-defined devices
   - C function pointers for create/process/destroy
   - Dynamic registration at runtime

3. **Validation and Error Handling**:
   - Centralized error callback system
   - Runtime validation of patches
   - Graceful degradation on errors

## Phase 6: Optimization and Polish (Final Core Phase)

**Goal**: Performance optimization, testing, documentation.

**Estimated Time**: 2 days

### Key Tasks
1. **Performance Optimization**:
   - SIMD in device process functions
   - Graph pruning (skip silent nodes)
   - Buffer reuse and pooling

2. **Testing**:
   - Unit tests for all API functions
   - Integration tests for common graphs
   - Stress tests (100+ nodes)

3. **Documentation**:
   - Complete API reference
   - Usage examples and tutorials
   - Migration guide from old mixer

## Future Enhancements (Post-Core)

### Phase 7: Modulators (High Priority)
**Time**: 2-3 days

Implement LFO and Envelope Follower for control signal modulation:
- LFO: sine/square/triangle/sawtooth waveforms, 0.1-20 Hz
- Envelope Follower: audio amplitude to control signal conversion

### Phase 8+: Advanced Features
**Time**: 20-30 days total

- Visual graph editor (node-based UI)
- Preset system (save/load device presets)
- MIDI integration (note input, CC mapping)
- Automation (timeline-based parameter automation)
- Multi-channel support (surround sound, ambisonics)
- Plugin system (VST3 wrapper)

## Timeline Summary

| Phase | Duration | Status | Start Date | End Date |
|-------|----------|--------|------------|----------|
| Phase 1: Registry | 1 day | ✅ Complete | 2026-03-01 | 2026-03-01 |
| Phase 2: Population | 1 day | ✅ Complete | 2026-03-01 | 2026-03-01 |
| Phase 3: Node Graph | 1 day | ✅ Complete | 2026-03-01 | 2026-03-01 |
| Phase 4: Processing | 4-5 days | ⏳ Next | TBD | TBD |
| Phase 5: Persistence | 2-3 days | ⏳ Planned | TBD | TBD |
| Phase 6: Optimization | 2 days | ⏳ Planned | TBD | TBD |
| **Core Total** | **11-14 days** | **21% Complete** | - | - |

## Recommended Approach for Phase 4

### Session 1: Topological Sort (1 day)
1. Implement Kahn's algorithm
2. Add caching and invalidation
3. Unit tests for sort correctness
4. Handle edge cases (cycles, disconnected nodes)

### Session 2: Processing Loop (2 days)
1. Implement buffer summing/splitting
2. Create device wrapper functions (start with 3-4 devices)
3. Integrate with miniaudio callback
4. Test with simple graph (Source → Reverb → Output)

### Session 3: Device Integration (1-2 days)
1. Complete wrapper functions for all 18 devices
2. Test each device individually
3. Test complex graphs with multiple devices
4. Performance profiling and optimization

### Session 4: Thread Safety (1 day)
1. Implement snapshot-and-unlock pattern
2. Test concurrent parameter updates
3. Stress test with rapid topology changes
4. Verify no audio glitches

## Key Technical Challenges

### Challenge 1: Buffer Management
**Issue**: Efficient buffer summing/splitting without excessive copying.  
**Solution**: Use in-place operations where possible, SIMD for summing.

### Challenge 2: Thread Safety
**Issue**: Graph updates from UI thread while audio callback is running.  
**Solution**: Snapshot-and-unlock pattern, lock-free reads in callback.

### Challenge 3: Latency
**Issue**: Topological sort and processing overhead.  
**Solution**: Cache sorted order, optimize hot paths, use SIMD.

### Challenge 4: Device State Management
**Issue**: Each device has different state structures.  
**Solution**: Opaque `void* device_data` pointer, type-specific wrappers.

## Success Metrics

### Performance Targets
- **Latency**: < 10ms at 48kHz (< 480 samples)
- **CPU Usage**: < 20% for typical graph (10-20 nodes)
- **Memory**: < 100MB for large graph (100+ nodes)
- **Throughput**: Process 1000+ nodes in real-time

### Quality Targets
- **Stability**: Zero crashes in 24-hour stress test
- **Accuracy**: Bit-identical output to reference implementations
- **Compatibility**: Works on Windows, Linux, macOS
- **Usability**: New users can build graph in < 5 minutes

## Documentation Status

### Completed
- ✅ `doc/AUDIO_DEVICE_INVENTORY.md` - Complete device list
- ✅ `doc/PHASE1_COMPLETE.md` - Phase 1 completion report
- ✅ `doc/PHASE2_COMPLETE.md` - Phase 2 completion report
- ✅ `doc/PHASE3_COMPLETE.md` - Phase 3 completion report
- ✅ `doc/AUDIO_SUBSYSTEM_ROADMAP.md` - Comprehensive roadmap
- ✅ `doc/plan_audio_registry.md` - Updated with Phase 3 completion

### Needed for Phase 4
- ⏳ `doc/PHASE4_PROGRESS.md` - Track Phase 4 development
- ⏳ `doc/NODE_GRAPH_API_GUIDE.md` - API usage examples
- ⏳ `doc/DEVICE_WRAPPER_GUIDE.md` - How to wrap devices

## Questions for Next Session

1. **Processing Strategy**: Should we implement all 18 device wrappers at once, or start with a subset (e.g., Reverb, Echo, Tone Synth, Sound Source)?

2. **Buffer Format**: Should we standardize on interleaved stereo (L, R, L, R) or planar (L, L, L... R, R, R)?

3. **Master Output**: Should the master output node be explicit (user-created) or implicit (automatically created)?

4. **Feedback Loops**: Should we support intentional feedback (e.g., delay with feedback) by disabling cycle detection for specific patches?

5. **Control Rate**: Should control parameters update per-sample, per-block, or on-change only?

## Conclusion

The audio subsystem has made excellent progress in just 3 days:
- ✅ Solid foundation with registry and node graph API
- ✅ 18 devices registered and ready for instantiation
- ✅ Type-safe, validated connections
- ✅ Clean compilation with no errors

Phase 4 will bring this architecture to life with real-time audio processing. The groundwork is complete, and the path forward is clear.

**Recommendation**: Begin Phase 4 in the next session, starting with topological sort implementation and a simple demo graph (Source → Reverb → Output).

---

**Document Created**: 2026-03-01  
**Next Review**: Before Phase 4 kickoff  
**Maintained By**: Kiro AI Assistant
