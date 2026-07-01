# Polysonix Integration Status

**Date**: 2026-03-03  
**Status**: Relocated to Audio Subsystem  
**Version**: Polysonix 1.9.13 / Situation 2.4.0

## Overview

Polysonix is a self-contained polyphonic synthesizer engine now properly located in `sit/aud/polysonix/` as part of the audio subsystem. It integrates with Situation through the audio callback system and is organized alongside other audio components.

## Polysonix Location

```
sit/aud/polysonix/                 # Polysonix synthesizer engine (AUDIO SUBSYSTEM)
├── polysonix.h                    # Main single-header library
├── px_vm.h                        # Virtual machine for waveform generation
├── px_*.h                         # Component headers (patches, patching, waves)
├── examples/                      # Integration examples
│   ├── polysonix_test_situation.c # Situation integration example
│   └── polysonix_test_raylib.c    # Raylib integration example
├── test/                          # Test suite
├── tools/                         # Development tools
└── doc/                          # Documentation
```

## Rationale for Audio Subsystem Placement

Polysonix is fundamentally an **audio synthesis engine**, making `sit/aud/` the logical location:

1. **Audio Component** - Polysonix generates audio samples, just like other audio devices
2. **Consistent Organization** - Groups all audio-related code together
3. **Clear Hierarchy** - Audio subsystem contains:
   - Effects (`sit/aud/fx/`)
   - Node graph system (`sit/aud/node_graph*.h`)
   - Audio devices (`sit/aud/sound_source.h`, `sit/aud/mic_capture.h`)
   - Synthesizers (`sit/aud/polysonix/`, `sit/aud/tone_synth.h`)
4. **Professional Structure** - Mirrors industry-standard audio library organization

## Integration Pattern

Polysonix is designed as a **decoupled audio engine** that integrates with any audio framework through a simple callback pattern.

### Integration with Situation

```c
#include "situation.h"
#define POLYSONIX_IMPLEMENTATION
#include "polysonix.h"

// Global synth instance
static PxSynth* synth = NULL;

// Audio callback wrapper
static uint64_t AudioCallback(void* user_data, void* buffer, uint64_t bytes_to_read) {
    if (!synth) return 0;
    
    uint64_t frames = bytes_to_read / (sizeof(float) * 2);
    PX_Process(synth, (float*)buffer, (int)frames);
    
    return bytes_to_read;
}

int main() {
    // 1. Initialize Situation
    SituationInitInfo info = { 
        .window_width = 800, 
        .window_height = 600, 
        .window_title = "Synth" 
    };
    SituationInit(0, NULL, &info);
    
    // 2. Create Polysonix synth
    PxConfig config = { 
        .num_voices = 8, 
        .sample_rate = 48000 
    };
    synth = PX_Create(&config);
    
    // 3. Start audio stream
    SituationAudioFormat fmt = { 
        .sample_rate = 48000, 
        .channels = 2, 
        .bit_depth = 32 
    };
    SituationSound stream;
    SituationLoadSoundFromStream(AudioCallback, NULL, NULL, &fmt, true, &stream);
    SituationPlayLoadedSound(&stream);
    
    // 4. Main loop
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        
        // Note control
        if (SituationIsKeyPressed(SIT_KEY_C)) 
            PX_NoteOn(synth, 60, 0, SIT_KEY_C, 1.0f);
        if (SituationIsKeyReleased(SIT_KEY_C)) 
            PX_NoteOff(synth, SIT_KEY_C);
        
        SituationEndFrame();
    }
    
    // 5. Cleanup
    SituationStopLoadedSound(&stream);
    SituationUnloadSound(&stream);
    PX_Destroy(synth);
    SituationShutdown();
}
```

## Include Path Status

### Polysonix Example Files

The Polysonix example `polysonix_test_situation.c` includes Situation without a relative path:

```c
#include "situation.h"  // Expects to be in include path
```

This is **correct and intentional** because:

1. **Compilation with Include Flags** - The example is meant to be compiled with proper `-I` flags:
   ```bash
   gcc polysonix_test_situation.c -I../../.. -o polysonix_test
   ```

2. **Framework Agnostic** - Polysonix is designed to work with multiple frameworks (Situation, Raylib, PortAudio, etc.), so it doesn't hardcode paths

3. **Self-Contained** - Polysonix has no dependencies on Situation's internal structure, only its public API

### No Changes Required

After the core headers reorganization:
- ✓ Polysonix examples remain unchanged
- ✓ Include paths work correctly with proper compiler flags
- ✓ No hardcoded relative paths to update

## Polysonix Architecture

### Key Features

**Synthesis Engine:**
- Polyphonic (up to 16 voices)
- Triple oscillator architecture per voice
- Dynamic waveform generation via bytecode VM
- ADSR envelopes (up to 3 per voice)
- LFOs (up to 3 global)
- Multi-mode filter (LP/BP/HP/Notch/Allpass)
- Wave sequencing with 256 ROM sequences
- Master limiter and dynamics

**Advanced Features:**
- Cross-modulation (FM/PM)
- Phase distortion
- Oscillator sync (hard/soft)
- Ring modulation
- Bitcrush per oscillator
- Portamento/glide (multiple modes)
- Polyphonic aftertouch
- Modulation matrix (16 slots)

**Thread Safety:**
- Lock-free command queue
- Safe UI/audio thread separation
- Snapshot-based state queries

### CPU vs GPU Backends

Polysonix supports two execution backends:

**CPU Backend (Default):**
- Pure C99, zero dependencies
- Low latency (< 1ms)
- Optimized with computed gotos
- ~100ns per sample on Apple M3

**GPU Backend (`POLYSONIX_USE_GPU`):**
- OpenGL 4.6 compute shaders
- Massive parallelism
- Ideal for offline rendering
- Can handle 1000+ voices simultaneously

## File Structure

### Core Headers

- **polysonix.h** - Main single-header library (define `POLYSONIX_IMPLEMENTATION`)
- **px_vm.h** - Virtual machine for waveform bytecode execution
- **px_patches_rom.h** - ROM patch library
- **px_patching.h** - Patch management system
- **px_wave_native.h** - Native waveform definitions
- **px_wave_rom.h** - ROM waveform library
- **px_wseq_rom.h** - Wave sequence ROM (256 sequences)

### Examples

- **polysonix_test_situation.c** - Full Situation integration example
- **polysonix_test_raylib.c** - Raylib integration example

### Documentation

- **README.md** - Comprehensive library documentation
- **doc/updatelog.md** - Version history and changes
- **doc/PERFORMANCE_REPORT.md** - Benchmark results

## Design Philosophy

Polysonix follows a **decoupled architecture**:

1. **No Built-in UI** - Host application handles all UI
2. **No Built-in Audio I/O** - Host provides audio callback
3. **No Built-in Effects** - Focus on core synthesis
4. **Framework Agnostic** - Works with any audio framework

This makes Polysonix:
- Highly portable
- Easy to integrate
- Minimal dependencies
- Suitable for embedded systems

## Compilation

### Standalone Compilation

```bash
# From sit/aud/polysonix/examples/
gcc polysonix_test_situation.c \
    -I../../../.. \
    -I../../../../ext \
    -I../../../../ext/glfw/include \
    -DSITUATION_IMPLEMENTATION \
    -DPOLYSONIX_IMPLEMENTATION \
    -o polysonix_test
```

### With Situation Build System

Polysonix can be integrated into Situation's build system by:
1. Adding include path: `-Isit/aud/polysonix`
2. Defining implementation in one file
3. Linking with Situation's audio system

## Status Summary

✓ Polysonix relocated to `sit/aud/polysonix/` (audio subsystem)  
✓ Self-contained with no external dependencies  
✓ Include paths work correctly with compiler flags  
✓ Properly organized alongside other audio components  
✓ Framework-agnostic design maintained  
✓ Examples demonstrate proper integration pattern  

## Audio Subsystem Structure

```
sit/aud/                           # Audio Subsystem
├── fx/                            # Audio effects (15 files)
│   ├── reverb.h, echo.h, chorus_4stage.h
│   ├── filter.h, eq_4band.h, dynamics.h
│   └── ... (other effects)
├── polysonix/                     # Polyphonic synthesizer engine
│   ├── polysonix.h                # Main synth library
│   ├── px_vm.h                    # Waveform VM
│   └── ... (synth components)
├── node_graph*.h                  # Node graph system (5 files)
├── device_*.h                     # Device system (3 files)
├── sound_source.h                 # Audio file playback
├── mic_capture.h                  # Microphone capture
├── tone_synth.h                   # Simple tone generator
└── threading_diagnostics.h        # Threading utilities
```  

## Related Documentation

- `CORE_HEADERS_REORGANIZATION.md` - Core headers moved to sit/
- `KTERM_INTEGRATION_STATUS.md` - K-Term terminal library integration
- `FX_FOLDER_ORGANIZATION.md` - Audio effects organization
- `sit/aud/polysonix/README.md` - Polysonix library documentation
- `sit/aud/polysonix/doc/updatelog.md` - Polysonix version history

## Future Considerations

1. **Example Compilation Scripts** - Add batch/shell scripts for building Polysonix examples
2. **Situation Audio Integration** - Consider deeper integration with Situation's node graph system
3. **Preset Management** - Integrate Polysonix patch system with Situation's resource management
4. **GPU Backend** - Explore GPU-accelerated synthesis for massive polyphony
5. **Documentation** - Keep Polysonix docs synchronized with Situation releases
