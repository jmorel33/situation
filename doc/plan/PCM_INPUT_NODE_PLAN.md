# PCM Input Node Plan

**Date**: 2026-06-01  
**Status**: COMPLETE  
**Depends on**: Node graph (SituationThreadSafeGraph), audio callback infrastructure  
**Motivation**: kterm voice playback, network audio streams, any user-fed PCM source

## Goal

Add a single new node type `SIT_NODE_PCM_INPUT` to the existing audio node graph. This node acts as a source that reads from a lock-free ring buffer fed by the user. It participates in the graph like any other node — mixable, patchable, effects-chainable.

## API Surface

```c
// Create a PCM input node (source — produces audio from user-supplied ring buffer)
SituationCreateNodeThreadSafe(graph, SIT_NODE_PCM_INPUT, &node);

// Push PCM samples into the node's ring buffer (call from any thread)
// Returns number of frames actually written (may be less if buffer full)
SITAPI uint32_t SituationPushNodePCM(
    SituationThreadSafeGraph* graph,
    SituationNodeHandle node,
    const float* samples,       // Interleaved float PCM
    uint32_t frame_count,
    uint32_t channels           // Must match node's channel config
);

// Query how many frames of space are available in the ring buffer
SITAPI uint32_t SituationGetNodePCMFreeFrames(
    SituationThreadSafeGraph* graph,
    SituationNodeHandle node
);

// Patch into the mix (existing API — no changes needed)
SituationCreatePatchThreadSafe(graph, pcm_node, 0, master_out, 0);
```

## Internal Design

### Ring Buffer (per node instance)

- Fixed-size power-of-2 ring buffer (default 4096 frames × channels)
- Atomic head/tail (same pattern as kterm voice's existing ring)
- Producer: user thread (via `SituationPushNodePCM`)
- Consumer: audio callback thread (node's process function)
- On underrun: output silence (no glitch — just quiet)

### Node Process Function

```c
static void _SitNodePCMInputProcess(SituationNode* node, float* output, uint32_t frames, ...) {
    // Read from ring buffer into output
    // If not enough data: zero-fill remainder (underrun)
    // Apply node gain/pan from controls (reuse existing node control infrastructure)
}
```

### Controls (reuse existing node control system)

| Control | ID | Description |
|---------|-----|-------------|
| Gain | 0 | Output volume (0.0–1.0, default 1.0) |
| Pan | 1 | Stereo pan (-1.0 left, +1.0 right, default 0.0) |
| Mute | 2 | 0/1 toggle |

### Memory

- Ring buffer allocated at node creation (`SIT_MALLOC`)
- Freed at node destruction
- Size configurable via a control or compile-time default (`SIT_PCM_INPUT_RING_FRAMES 4096`)

## Integration Points

- **kterm voice**: Replace `SituationStartAudioPlayback` with `SituationPushNodePCM` in the voice tick
- **Network audio**: Push received packets into the node
- **Polysonix**: Could use this for sample playback channels
- **Testing**: Feed known waveforms, verify output via capture

## Tasks

- [x] Add `SIT_NODE_PCM_INPUT` to `SituationNodeType` enum
- [x] Implement ring buffer struct (reuse pattern from kterm voice or I/O queue)
- [x] Implement `_SitNodePCMInputProcess` (consumer side)
- [x] Implement `SituationPushNodePCM` (producer side, any-thread safe)
- [x] Implement `SituationGetNodePCMFreeFrames` (query)
- [x] Register node type in graph creation dispatch
- [x] Wire gain/pan/mute controls
- [x] Update kterm `kt_voice.h` to use the new node instead of the removed playback API
- [x] Test: push sine wave → capture output → verify frequency
- [x] Doc: update `doc/audio_analysis.md` and `doc/midi_api.md`

## Non-Goals

- No resampling (caller must match the graph's sample rate)
- No codec/decode (caller provides raw float PCM)
- No automatic device routing (goes through the graph like everything else)
