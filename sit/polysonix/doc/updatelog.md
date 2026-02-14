# Update Log

## v1.8.5 (2026/01/18)
**Feature Update: Long Patch Names & ROM Persistence**

This update addresses user feedback regarding preset naming limitations and ensures that patch names are fully persisted across all system components.

*   **Expanded Patch Name:**
    *   **64-Character Support:** Increased the maximum patch name length from 16 to 64 characters (`PX_PATCH_NAME_LEN`), allowing for descriptive titles without truncation.
    *   **Serialization:** Updated the preset file format (.syx) to include the full name in the data body for versions >= 1.8.5, while maintaining backward compatibility with older readers (which will use the 16-char header fallback).
    *   **Runtime Persistence:** Ensure the full name is stored in the `PxPatch` struct and accessible via `PX_GetPatchName` at runtime.
    *   **ROM Updates:** Updated `px_patches_rom.h` to include full names for factory presets.

## v1.8.4 (2026/01/17)
**Codebase Hygiene & Content Update**

This maintenance release refactors the ROM architecture for clarity and significantly expands the preset library.

*   **Refactoring:**
    *   **Renamed Header:** `px_vm_bank.h` has been renamed to `px_wave_rom.h` to better reflect its purpose as a read-only waveform memory.
    *   **Consolidated Includes:** `px_patching.h` now serves as the single entry point for all ROM data, automatically managing the inclusion of `px_patches_rom.h`, `px_wseq_rom.h`, and `px_wave_rom.h`.
    *   **Example Updates:** Updated all examples and test harnesses to align with the new file structure.

*   **Content:**
    *   **New Patches:** Replaced the placeholder slots (Indices 16-31) in `px_patches_rom.h` with 16 new, high-quality presets. These patches showcase advanced features like FM, Filter Drive, and complex envelopes (e.g., "Ethereal Pad", "Gritty Lead", "Bell Chime").

## v1.8.3 (2026/01/13)
**Feature Update: Static Glide / Portamento**

This release implements a robust Static Glide (Portamento) system for non-sequenced playback, bridging the gap between classic Unilegato and modern polyphonic glide.

*   **Static Glide:**
    *   **New Modes (`PxGlideMode`):**
        *   `PX_GLIDE_OFF`: Instant pitch changes (default).
        *   `PX_GLIDE_STEP_LINEAR`: Legacy "Unilegato" behavior. Hard jump to new note, then linear interpolation over time.
        *   **`PX_GLIDE_SMOOTH_RC`:** A new, natural-sounding exponential glide (RC-filter style). Pitch asymptotically approaches the target note, mimicking analog capacitor charge/discharge.
    *   **Configuration:**
        *   `glide_time`: Sets the glide duration (or RC time constant).
        *   `glide_legato_only`: If true, glide only occurs when notes overlap (fingered portamento). If false, glide always occurs (monophonic/polyphonic portamento).
        *   `glide_always`: (Helper flag) Force glide behavior regardless of overlap.
    *   **Polyphonic Support:** Unlike legacy Unilegato which was strictly monophonic (voice reuse), the new system supports polyphonic glide. If a voice is stolen or re-allocated while glide is active, it will glide from its previous pitch to the new note.
    *   **Backward Compatibility:** The existing `PX_SetUnilegatoEnabled` API is fully preserved. Internally, it configures the new system to use `PX_GLIDE_STEP_LINEAR` and `glide_legato_only = true`, ensuring identical behavior for existing code.

*   **API Updates:**
    *   Added `PX_SetGlideMode`, `PX_GetGlideMode`.
    *   Added `PX_SetGlideTime`, `PX_GetGlideTime`.
    *   Added `PX_SetGlideLegatoOnly`, `PX_GetGlideLegatoOnly`.
    *   Added `PX_SetGlideAlways`, `PX_GetGlideAlways`.

## v1.8.2 (2026/01/12)
**Refactor: Bitcrush Logic**

Refactored the Bitcrush effect from being a strictly Wave Sequencer property to being a per-Oscillator property.

*   **No Regression:** The Wave Sequencer's `PX_WSEQ_BITCRUSH` flag still triggers the effect, but it now utilizes the Oscillator's `bitcrush_enabled` and `bitcrush_depth` properties.
*   **Default Behavior:** To maintain legacy behavior for ROM sequences, oscillator bitcrush depth defaults to `0.8f` (approx. 4 bits) when initialized.
*   **Cleanup:** Removed the deprecated `bitcrush_bits` field from `PxWaveSequence` and `step_bitcrush_scale` from internal state.

## v1.8.1 (2026/01/11)
**Fix: Wave Sequencer Timing Drift**

This patch addresses a timing drift issue in the Wave Sequencer where fractional phase increments were being truncated, causing step durations to slowly drift away from the intended tempo over long sequences.

*   **Fractional Carry-Over:**
    *   **Engine Logic:** Implemented a `cycle_accumulator` in the sequencer state to track fractional cycle advancement.
    *   **Precision:** The "leftover" fraction of a cycle is now carried over to the next processing frame, and any overshoot beyond the target step duration is carried over to the next step.
    *   **Result:** This ensures that the average step duration remains mathematically exact over time, eliminating rhythm drift while preserving sample-accurate transitions.

## v1.8.0 (2026/01/11)
**Major Feature: Time-Locked Wave Sequencing & Engine Fixes**

This release introduces the "Time-Locked" Wave Sequencing mode, which ensures consistent step duration across different pitches, crucial for rhythmic sequences and bass lines. It also includes critical fixes for the LFSR system and sequencer logic.

*   **Time-Locked Wave Sequencing:**
    *   **New Feature:** Added `wseq_fixed_time` global setting. When enabled, the duration of wave sequence steps is dynamically scaled based on the oscillator's pitch relative to a reference frequency (`wseq_ref_freq`, default 440 Hz).
    *   **Benefit:** This allows sequences to maintain their rhythmic timing (e.g., 16th notes) regardless of the note played, avoiding the "speed up/slow down" effect of traditional cycle-based sequencing, while preserving phase integrity.
    *   **Integration:**
        *   Added `PX_SetWSeqFixedTime` and `PX_SetWSeqRefFreq` APIs.
        *   Updated serialization to include these new fields.
        *   Optimized `PX_Process` to cache target cycle counts, ensuring zero per-sample overhead.

*   **Bug Fixes & Improvements:**
    *   **LFSR 14-bit Fix:** Corrected the tap mask and re-enabled the 14-bit LFSR generator in `px_vm.h`, which was previously disabled due to instability. It now uses a standard primitive polynomial.
    *   **Cycle Counting Logic:** Fixed a logic error in `PX_Process` where the cycle target for the *next* step was incorrectly calculated using the *current* step's properties.
    *   **Verification:** Verified timing logic correctness across octaves using a custom `mprotect`-based test harness.

## v1.7.11 (2026/01/09)
**Feature Update: Wave Sequencer Amplitude Modulation**

This release adds per-step Amplitude Modulation to the Wave Sequencer, allowing for intricate rhythmic gating and envelope shaping directly within the sequence.

*   **Amplitude Modulation:**
    *   **New Feature:** Each step in a Wave Sequence can now apply an amplitude envelope or static level modulation.
    *   **Per-Step Flag:** Added `PX_WSEQ_AMP_MOD` flag (Bit 3) to enable this feature on a per-step basis.
    *   **Envelope Shapes:** The global `amp_mod_type` setting defines the shape used when the flag is active:
        *   **Ramp Down/Up:** Classic pluck or reverse effects.
        *   **Triangle/Sine:** Smooth pulsing or tremolo.
        *   **Square/Pulse:** Hard rhythmic gating (50% or 25% duty cycle).
        *   **Exp Down:** Percussive exponential pluck.
        *   **Random:** Sample-and-Hold random level (latched per step).
    *   **Implementation:** Replaced the unused `_padding` byte in `PxWaveSequence` with `amp_mod_type`, preserving binary compatibility.

## v1.7.10 (2026/01/05)

This critical update fixes four significant issues identified in the codebase related to stability, thread safety, and resource management.

### Fixes
*   **Serialization Buffer Overflow (Patch Loading):**
    *   **The Issue:** Loading a preset saved with a larger configuration (e.g., 4 LFOs) into an instance with a smaller configuration (e.g., 3 LFOs) would cause a heap buffer overflow during deserialization.
    *   **The Fix:** Modified the preset file header to store the critical configuration dimensions (`num_voice_adsrs` and `num_lfos`) in the previously reserved block. `PX_LoadPresetFromBus` now validates these values against the running synth configuration and safely aborts the load if a mismatch is detected, preventing memory corruption.
*   **API Thread Safety (Command Queue Race):**
    *   **The Issue:** The lock-free command queue was only safe for Single-Producer/Single-Consumer. Concurrent calls from multiple threads (e.g., MIDI thread and GUI thread) could cause race conditions on the write index, leading to lost commands or queue corruption.
    *   **The Fix:** Introduced a lightweight spinlock (`atomic_flag`) around the `cmd_push` operation, making the API thread-safe for Multi-Producer/Single-Consumer scenarios. Included `PX_CPU_PAUSE()` (using `_mm_pause` / `yield`) for efficient waiting.
*   **GPU Resource Leak:**
    *   **The Issue:** `PX_Destroy` did not release Vulkan/Compute resources allocated by `PX_Create` when `use_gpu` was enabled, leading to leaks upon synth destruction.
    *   **The Fix:** Added a call to `px_vm_cleanup_gpu_resources()` within `PX_Destroy`, guarded by the appropriate configuration checks.
*   **Wave Sequence Logic (Random Jump Bounds):**
    *   **The Issue:** The `PX_WSEQ_JUMP_RANDOM` flag used a modulo of the maximum steps (64) regardless of the actual sequence length. This could cause the sequencer to jump into uninitialized memory regions (zeroed steps), resulting in silent loops.
    *   **The Fix:** Updated `PX_Process` to dynamically scan the active sequence for its valid length (checking for `PX_WSEQ_END` or implicit zero-termination) before calculating the random jump target, ensuring playback stays within valid bounds.

## v1.7.9 (2025-02-17)
- **Feature**: Added a `name` field to the `PxWaveSequence` struct in `polysonix.h`, mirroring the structure of wave definitions.
- **Update**: Populated the `name` field for all 256 Wave Sequencer entries in `px_wseq_rom.h` with descriptive names derived from their comments.
- **Documentation**: Updated the reference documentation in `px_wseq_rom.h` to include the new `name` field.

## v1.7.8 (2026-01-12)
**Content Update: Wave Sequencer ROM Completion**

This update puts the "crazy" in "Ham Crazy" by finalizing the Wave Sequencer ROM with a massive injection of experimental, digital, and aggressive sequences.

*   **ROM Finalization:**
    *   **Complete Library:** The `px_wseq_rom.h` file is now fully populated with 256 unique wave sequences (Indices 0-255), unlocking the full potential of the engine's wavetable capabilities.
    *   **Thematic Banks (128-255):** The second half of the ROM explores experimental, digital, and aggressive territories, specifically designed for modern IDM, Glitch, and Sci-Fi sound design:
        *   **Bank 16: Wavestation** (Vector synthesis textures & rhythmic loops)
        *   **Bank 17: VS Trickery** (Fast scanning & digital artifacts)
        *   **Bank 18: Glitch IDM** (Fast, chaotic, & generative patterns)
        *   **Bank 19: Trap** (Deep sub-bass & sharp, rolling hats)
        *   **Bank 20: Industrial** (Distorted, metallic, and mechanical textures)
        *   **Bank 21: Abyss** (Dark ambient drones & space atmospheres)
        *   **Bank 22: Experimental A** (Math-based complexity & prime number sequences)
        *   **Bank 23: Experimental B** (Pure glitch, data-mosh, & digital mayhem)
        *   **Bank 24: Metallic Textures** (Ring modulation & inharmonic clangor)
        *   **Bank 25: Percussive Hits** (Short, punchy drum patterns with retriggering)
        *   **Bank 26: Evolving Pads** (Long, slowly morphing textures using glide)
        *   **Bank 27: Glitch Effects** (Bitcrushed artifacts, random skipping & reverse playback)
        *   **Bank 28: Noise Sculptures** (Abstract noise art using variable-rate bitcrushing)
        *   **Bank 29: Evolving Pads II** (Extended variations of complex pad textures)
        *   **Bank 30: Screaming Edges** (Wild, funky, push-the-limit textures like "Glitch Funk" & "Neuro Tear")
        *   **Bank 31: Total Meltdown** (Pure chaos & noise destruction features "System Meltdown")

*   **Design Philosophy:**
    *   These new banks heavily utilize the engine's advanced features:
        *   **Extreme Modulation:** High-depth Ring Modulation and Cross-Modulation (FM) for inharmonic clangor.
        *   **Destructive FX:** Aggressive Bitcrushing (down to 2 bits) combined with reverse playback.
        *   **Generative Chaos:** Extensive use of Probability Skips, Random Octaves, and Random Wave selection to create non-repetitive, living textures.

## v1.7.7 (2026-01-11)
**Content Update: Full Wave Sequencer ROM**

This update unlocks the full potential of the Wave Sequencer by populating the ROM with a diverse library of 128 production-ready sequences.

*   **Expanded ROM:**
    *   **Full Capacity:** The `px_wseq_rom.h` file now contains 128 unique wave sequences (Indices 0-127), replacing the previous sparse set of examples.
    *   **Thematic Banks:** Sequences are organized into 16 thematic banks of 8 entries each, providing instant access to a wide range of rhythmic and textural behaviors:
        *   **Bank 0: Lead** (Classic sync, PWM, and FM leads)
        *   **Bank 1: Pad** (Evolving, glassy, and choir textures)
        *   **Bank 2: Strings** (Ensembles, tremolos, and pizzicato)
        *   **Bank 3: Choir** (Vowel morphs and robotic voices)
        *   **Bank 4: Ensemble** (Brass, orchestra hits, and big band stacks)
        *   **Bank 5: Pluck** (Guitars, harps, and ethnic instruments)
        *   **Bank 6: Percussive** (Drums, glitches, and industrial hits)
        *   **Bank 7: Oldskool** (Chiptune arps and retro game effects)
        *   **Bank 8: Arcade** (Classic SFX like jumps, lasers, and power-ups)
        *   **Bank 9: Fun** (Cartoonish and novelty sounds)
        *   **Bank 10: Natural** (Environmental textures like wind, rain, and fire)
        *   **Bank 11: Enhanced** (Complex modern synth techniques: supersaws, trance gates)
        *   **Bank 12: Deep** (Sub-bass, dub chords, and dark drones)
        *   **Bank 13: Futuristic** (Sci-fi textures, teleport sounds, and data streams)
        *   **Bank 14: Emulation** (Traditional instrument approximations)
        *   **Bank 15: Strange** (Experimental math-based and chaotic sequences)

## v1.7.6 (2026-01-11)
**Codebase Restructure: Organized Wave Bank & New Complex Waves**

This update significantly improves the organization of the waveform library and replaces wasteful placeholder slots with advanced new waveforms.

*   **Wave Bank Reorganization:**
    *   **Thematic Organization:** The `px_vm_bank.h` file has been completely reorganized into 16 thematic banks of 16 waves each (e.g., "Analog & Basic", "FM Synthesis", "Chiptune Tones"). This makes browsing and selecting waveforms much more intuitive.
    *   **Similarity Tagging:** Added `[SIMILAR TO ORIGINAL #...]` tags in comments to help identify variations of standard waves while maintaining a rich selection.
    *   **Comment Preservation:** All original mathematical explanations and parameter descriptions have been preserved and moved with their respective waves.

*   **New Complex Waveforms:**
    *   Replaced previously wasteful placeholder slots (Index 247 "DC Offset" and 248 "Silence") with new, mathematically rich waveforms:
        *   **#247 "Fibonacci Series":** An additive synthesis wave using the Golden Ratio for harmonic series, creating unique non-harmonic, bell-like timbres.
        *   **#248 "Logistic Chaos":** A complex AM/FM hybrid wave simulating chaotic behavior, useful for generative textures and noise.

## v1.7.5 (2026-01-10)
**Codebase Restructure: Rename to px_vm**

This maintenance release focuses on consistency and codebase hygiene by renaming the core waveform library.

*   **Renaming:**
    *   Renamed `polysonix_wave.h` to `px_vm.h`.
    *   Renamed `polysonix_wave.comp` to `px_vm.comp`.
    *   Renamed `examples/polysonix_wave_bank.h` to `px_vm_bank.h` and moved to root.
    *   Updated all internal API functions and macros to use the `px_vm_` prefix (e.g., `init_polysonix_lfsr_tables` -> `px_vm_init_lfsr_tables`).
    *   Updated all documentation and comments to reflect the new naming convention.

## v1.7.4 (2026-01-10)
**Feature Update: Patch Bank System & Robust IO**

This release adds a comprehensive patch bank management system and significantly hardens the serialization logic.

*   **Patch Bank System:**
    *   **New Architecture:** Introduced `PxPatchBank` in `px_patching.h`, a container holding `PX_PATCH_BANK_SIZE` (128) patches.
    *   **Memory Management:** Implemented `PX_CreatePatchBank` and `PX_DestroyPatchBank` which automatically handle the deep memory allocation (and deallocation) required for dynamic patch components like ADSRs and LFOs.
    *   **Safe Patch Transfer:** Added `PX_Bank_SaveToSlot`, `PX_Bank_LoadFromSlot`, and `PX_Bank_CopySlot`. These functions perform **deep copies** of patch data, ensuring that pointers to internal arrays are correctly preserved and preventing memory corruption or leaks when moving patches between the live synth and storage slots.
    *   **Safety Checks:** The transfer functions include rigorous configuration compatibility checks. They will reject operations where the source patch configuration (e.g., number of envelopes or LFOs) exceeds the capacity of the destination, preventing buffer overruns.

*   **IO & Serialization Refactor:**
    *   **Removed Macros:** The fragile `PX_SERIALIZE_BODY` macro system has been replaced with explicit, maintainable C functions `px_serialize_patch_impl` and `px_deserialize_patch_impl`.
    *   **Robustness:** The new serialization logic includes improved bounds checking and error handling during read/write operations.
    *   **Cleanup:** Removed obfuscating `PX_IO_BATCH_WRITE`/`READ` macros in favor of clear function pointer usage.

## v1.7.3 (2026-01-10)
**Feature Update: Binary Preset System & IO Abstraction**

This release introduces a robust, binary preset management system designed for production use, along with a flexible I/O abstraction layer.

*   **Preset System (.syx):**
    *   **Compact Binary Format:** Implemented a new `.syx` file format that serializes the entire `PxPatch` state (including dynamic arrays like ADSRs and LFOs) into a compact binary blob.
    *   **Production Ready:** The format includes a 32-byte header with magic bytes ("POLY"), rigorous versioning, and a checksum footer to ensure data integrity during load.
    *   **Full State Capture:** Saves all patch parameters: Oscillators, Modulation Matrix, Global Filter, Envelopes, LFOs, and performance curves.

*   **IO Abstraction:**
    *   **Batched I/O:** Introduced `PxIOWriteFn` and `PxIOReadFn` abstractions in `px_patching.h`. The system serializes the entire patch to a memory buffer before performing a single "batch write" operation.
    *   **Transport Agnostic:** While convenient file wrappers (`PX_SavePreset`/`PX_LoadPreset`) are provided for disk operations, the core logic (`PX_SavePresetToBus`) supports any transport mechanism. This enables direct preset dumps over MIDI Sysex, network packets, or memory streams without modification.

*   **API Updates:**
    *   Added `px_patching.h` extension library.
    *   Added `PX_SavePreset` / `PX_LoadPreset` (File helpers).
    *   Added `PX_SavePresetToBus` / `PX_LoadPresetFromBus` (Abstract IO).
    *   Added `PX_CalculatePresetSize`.

## v1.7.2 (2026-01-09)
**Fixes: Command Queue, Filter Accuracy, Soft Sync**

This release focuses on stability and audio fidelity improvements based on stress testing and rigorous analysis.

*   **Thread Safety:**
    *   **Increased Command Queue:** The internal command queue size (`CMD_QUEUE_SIZE`) has been doubled from 512 to 1024. This ensures robust handling of large preset dumps (e.g., loading a patch with hundreds of parameter changes) without dropping commands, even if the audio thread is momentarily preempted.

*   **Filter Accuracy:**
    *   **Linear Pole Response:** Removed the heuristic cutoff compensation ("magic numbers") that was previously applied to 18dB and 24dB slopes. The filter cutoff frequency now strictly adheres to the requested value across all pole settings, providing a mathematically accurate and predictable response. Users can now manually compensate for brightness if needed using key tracking or modulation.

*   **Oscillator Sync:**
    *   **Improved Soft Sync:** Updated the Soft Sync algorithm to use a precise "exponential pull" formula (`phase += (1.0 - softness) * (0.0 - phase)`). This ensures a smooth, musical transition from Hard Sync (0.0) to No Sync (1.0), eliminating discontinuities and providing an authentic analog feel.

*   **Limiter Safety:**
    *   **Dynamic Lookahead Buffer:** Changed the master limiter to use dynamic memory allocation for its lookahead buffer. The buffer size is now calculated at creation time based on the sample rate (`rate * 2ms`), ensuring safe and consistent 1ms lookahead behavior at any sample rate (including >192kHz) while optimizing memory usage for lower rates.

## v1.7.1 (2026-01-09)
**Feature Update: Wave Sequencer & Timbre Refinements**

This release polishes the Wave Sequencer with analog-style glide and enhanced randomization, and introduces per-oscillator bitcrushing.

*   **Wave Sequencer Updates:**
    *   **Exponential Glide (`PX_WSEQ_GLIDE`):** Added a new per-step flag `PX_WSEQ_GLIDE` (Bit 7). When enabled, pitch transitions use an exponential curve (`powf`) that mimics the charging capacitor behavior of analog synthesizer portamento.
    *   **Glide Refinements:** Implemented minimum glide duration clamping (10ms) to prevent "smearing" artifacts on very short steps.
    *   **Polyphonic RNG:** Improved the `PX_WSEQ_USE_PROB_SKIP` and `PX_WSEQ_USE_RND_OCTAVE` logic by seeding the per-voice RNG with the note's pitch (`midi_note`). This ensures that voices playing the same chord don't all skip or shift in unison, creating more organic, independent variation.

*   **Timbre Features:**
    *   **Per-Oscillator Bitcrush:** Bitcrush is now a per-oscillator effect (`PX_SetOscBitcrush`), allowing for "clean/dirty" layering (e.g., a pristine sub-bass with a crushed lead).
    *   **Modulation:** Bitcrush depth is fully modulatable via the Matrix (`PX_MOD_DEST_OSC_BITCRUSH_DEPTH`), mapping a 0.0-1.0 signal to a bit depth range of 16 down to 1.

*   **API Updates:**
    *   Added `PX_SetOscBitcrush`, `PX_GetOscBitcrush`, `PX_GetOscBitcrushEnabled`.
    *   Added `PX_MOD_DEST_OSC[1-3]_BITCRUSH_DEPTH`.

## v1.7.0 (2026-01-08)
**Major Feature Update: Advanced Oscillator Interactions**

This release unlocks deep analog-style sound design by introducing direct interaction between the three oscillators.

*   **Oscillator Interactions:**
    *   **Cross-Modulation (FM):** Oscillator N modulates the phase of Oscillator N-1. This allows for classic DX-style FM textures, metallic bells, and complex inharmonic spectra.
    *   **Phase Distortion (PD):** Implemented Casio-style phase warping per oscillator. A sine-based transfer function is applied to the phase accumulator before waveform lookup, allowing for resonant, sharp, and squelchy timbres without using the filter.
    *   **Oscillator Sync:**
        *   **Hard/Soft Sync:** Oscillator N resets its phase whenever Oscillator N-1 completes a cycle.
        *   **Adjustable Softness:** The sync effect can be blended from hard reset (classic "tearing" leads) to soft sync (gentler harmonic locking) via the `softness` parameter.
    *   **Ring Modulation:** Oscillator N amplitude modulates Oscillator N-1 (or vice versa depending on routing). This creates sum and difference frequencies, ideal for sci-fi sounds, robotics, and clangorous bells.

*   **Core Engine Updates:**
    *   **DSP Refactor:** The oscillator processing loop has been redesigned to support inter-oscillator dependencies with zero latency within the sample frame.
    *   **Output Caching:** Added caching for oscillator outputs and cycle completion status to facilitate modulation between oscillators (N interacting with N-1).
    *   **Modulation Matrix:** Expanded the modulation matrix to support 10 parameters per oscillator (Pitch, Mix, Pan, ModA, ModB, ModC, Cross-Mod, PD, Sync, Ring Mod).

*   **API Updates:**
    *   Added `PX_SetOscCrossMod` / `PX_GetOscCrossMod`.
    *   Added `PX_SetOscPhaseDist` / `PX_GetOscPhaseDist`.
    *   Added `PX_SetOscSync` / `PX_GetOscSync`.
    *   Added `PX_SetOscRingMod` / `PX_GetOscRingMod`.
    *   Corresponding `Enabled` getters for all new features.

## v1.6.0 (2026-01-07)
**Major Feature Update: Triple Oscillator Architecture**

*   **Architecture:**
    *   Transitioned from single-oscillator voices to a **Triple Oscillator** architecture (`PX_MAX_OSC_PER_VOICE = 3`).
    *   Each oscillator has independent controls for Waveform, Coarse Tuning (-24 to +24 semitones), Fine Tuning (-100 to +100 cents), Mix Level, Stereo Pan, and Enable state.
    *   Updated `PxPatch` and `Voice` structures to accommodate the new oscillator arrays.

*   **Wave Sequencing:**
    *   **Independent Sequencing:** Each of the 3 oscillators now has its own independent Wave Sequencer state (`PxSeqState`), allowing complex polyrhythmic and multi-timbral layering within a single voice.
    *   **Feature Preservation:** All v1.5 Wave Sequencer features (Glitch effects, Probability, Glide modes, Phase Locking) are preserved and function independently per oscillator.
    *   New API: `PX_SetOscSequence(synth, osc_idx, seq_id)` allows assigning different sequences to different oscillators.

*   **Audio Engine:**
    *   Refactored `PX_Process` to iterate through the 3 oscillators, summing their output into a stereo mix before the filter stage.
    *   **Unilegato Fix:** Separated base frequency calculation from modulation to prevent "double modulation" artifacts during slides. Global pitch modulation (LFO, ADSR) is now applied to the interpolated frequency, ensuring smooth and stable portamento.
    *   **Stereo Filtering:** Added `filter_instance_r` to the voice structure to support true stereo processing (e.g., for panned oscillators).

*   **API Updates:**
    *   Added `PX_SetOscEnabled`, `PX_SetOscWave`, `PX_SetOscMix`, `PX_SetOscPan`.
    *   Updated `PX_SetOscCoarseTune` and `PX_SetOscFineTune` to take an `osc_idx` parameter.
    *   Added `PX_GetOscSequence`.
    *   Legacy `PX_NoteOn` maps the `wave_idx` argument to Oscillator 0.
    *   Legacy `PX_SetSequenceID` maps to Oscillator 0.

*   **Diagnostics:**
    *   Updated `PxVoiceInfo` to include `active_wave_indices[3]`, enabling real-time monitoring of the current waveform index driven by the Wave Sequencer for each oscillator.

## v1.5.0 (2026/01/06)

This major release introduces the **Wave Sequencer**, a powerful per-voice sequencing engine for rhythmic, glitch, and generative sound design.

### Features
*   **Wave Sequencer:**
    *   **Per-Voice Logic:** Each voice runs its own independent sequencer instance, allowing for polyrhythmic and phase-perfect step transitions.
    *   **Per-Cycle Precision:** Step advancement and logic are evaluated every waveform cycle, ensuring tight synchronization with the oscillator phase.
    *   **Global Settings:**
        *   **End Actions:** Loop, PingPong, Stop, Hold, or Reverse.
        *   **Glide Modes:** `STEP` (linear glide over step duration) and `SMOOTH` (continuous 1-pole portamento).
        *   **FX:** Bitcrush (1-8 bits), Ring Mod, and XMod (Feedback FM), with modulation depth control via standard sources (Velocity, Mod Wheel, Aftertouch).
        *   **Probabilities:** Global "scores" (0-100%) for randomly muting or skipping steps.
    *   **Per-Step Flags:**
        *   **Control:** `JUMP_RANDOM`, `END`, `LOOP_POINT`.
        *   **Generative:** `USE_PROB_MUTE`, `USE_PROB_SKIP`, `USE_RND_OCTAVE`, `USE_RND_WAVE`.
        *   **Modulation:** `RESET_LFO`, `RETRIG_ADSR` (to specific phase), `LOCK_PHASE` (Hard Sync).
        *   **FX:** `BITCRUSH`, `RING_MOD`, `XMOD`, `REVERSE_PLAY`.

*   **Audio Engine Improvements:**
    *   **Thread-Safe PRNG:** Replaced `rand()` in the audio path with a context-aware Linear Congruential Generator (`px_rand`), seeded deterministically per-voice.
    *   **Optimized DSP:** Bitcrush and pitch ratios are pre-calculated per step to minimize CPU load.

### API Changes
*   **New Functions:**
    *   `PX_SetSequenceID(s, seq_id)`: Selects the active sequence (-1 for off).
    *   `PX_GetSequenceID(s)`
*   **New Structs:** `PxWaveSequence` and `PxWaveSeqStep` defined in `polysonix.h`.
*   **ROM Storage:** Sequences are stored in `ROM_WAVE_SEQUENCES` (Flash/RO memory friendly).

### Backward Compatibility
*   **Default State:** Sequence ID defaults to -1 (Off). Existing patches behave identically to previous versions.

## v1.4.6 (2026/01/05)

This update completes the core analog voicing section by introducing **Per-Oscillator Coarse & Fine Tuning**.

### Features
*   **Per-Oscillator Tuning:**
    *   **Coarse Tuning:** Each waveform can now be detuned by ±24 semitones (±2 octaves) independently.
    *   **Fine Tuning:** Each waveform can be fine-tuned by ±100 cents (±1 semitone) for rich detuning, beating, and chorus effects.
    *   **Per-Voice Logic:** Tuning offsets are applied at the voice level *before* modulation, ensuring stable intervals even when pitch is modulated.
    *   **Applications:** Enables classic analog techniques such as:
        *   Sub-octave bass layering (set coarse to -12 or -24).
        *   Fifth intervals (set coarse to +7).
        *   Supersaw-style detuning (using fine tune).
        *   Fixed-frequency drones or clusters.

### API Changes
*   **New Functions:**
    *   `PX_SetOscCoarseTune(s, wave_idx, semitones)`: Sets coarse tuning (-24 to +24).
    *   `PX_GetOscCoarseTune(s, wave_idx)`
    *   `PX_SetOscFineTune(s, wave_idx, cents)`: Sets fine tuning (-100 to +100).
    *   `PX_GetOscFineTune(s, wave_idx)`
*   **Struct Update:** `PxPatch` now includes `osc_coarse_semitones` and `osc_fine_cents` arrays.

### Backward Compatibility
*   **Defaults:** All tuning offsets default to `0.0`, ensuring that existing patches play at standard pitch and sound identical to previous versions.

## v1.4.5 (2026/01/05)

This update refines the modulation system with **Non-Linear Response Curves** for velocity and aftertouch, plus a new **Keyboard Tracking** modulation source.

### Features
*   **Response Curves:**
    *   **Per-Source Customization:** Velocity and Aftertouch (Channel & Poly) can now be mapped using one of four curves: `Linear` (default), `Exponential`, `Logarithmic`, or `S-Curve`.
    *   **Global Application:** Curves are applied globally per patch, transforming the raw input (0.0–1.0) before it enters the modulation matrix.
    *   **Curve Types:**
        *   `PX_CURVE_LINEAR`: 1:1 mapping.
        *   `PX_CURVE_EXP`: Sensitive at high velocities/pressures (power of 2).
        *   `PX_CURVE_LOG`: Sensitive at low velocities/pressures (logarithmic).
        *   `PX_CURVE_S`: Smooth ease-in/ease-out response.
*   **Keyboard Tracking Source:**
    *   **New Source:** Added `PX_MOD_SRC_KEY_TRACK` to the modulation matrix.
    *   **Functionality:** Generates a modulation signal based on the note pitch, normalized relative to C4 (MIDI 60).
    *   **Range:** -1.0 (low keys) to +1.0 (high keys), allowing key position to modulate any parameter (e.g., filter cutoff, LFO speed).

### API Changes
*   **New Functions:**
    *   `PX_SetVelocityCurve(s, curve)` / `PX_GetVelocityCurve(s)`
    *   `PX_SetAftertouchCurve(s, curve)` / `PX_GetAftertouchCurve(s)`
*   **New Enums:** `PxCurveType` (`PX_CURVE_LINEAR`, `PX_CURVE_EXP`, etc.).
*   **Updated Enum:** Added `PX_MOD_SRC_KEY_TRACK` to `PxModSource`.

### Backward Compatibility
*   Defaults to `PX_CURVE_LINEAR` and 0.0 amount for Key Track modulation, preserving existing patch behavior.

## v1.4.4 (2026/01/05)

This update introduces a **Global Post-Filter**, allowing final tone shaping of the entire mix before the limiter.

### Features
*   **Global Post-Filter:**
    *   **Architecture:** Adds a new filter stage after voice mixing and before the master limiter.
    *   **Stereo Processing:** Uses two independent filter instances (Left/Right) to ensure correct stereo signal processing without state crosstalk.
    *   **Full Control:** Supports all existing filter modes (LP, HP, BP, Notch, Allpass, Combo) and slopes (6/12/18/24 dB/oct).
    *   **API:** New functions to control the global filter: `PX_SetGlobalFilterEnabled`, `PX_SetGlobalFilterParam`, `PX_SetGlobalFilterMode`.
*   **Optimization:**
    *   Calculates filter coefficients once per block and shares them between Left/Right channels for efficiency.

### Backward Compatibility
*   **Disabled by Default:** The global filter is disabled in default patches (`global_filter_enabled = false`), ensuring existing projects sound identical.
*   **Struct Update:** `PxSynth` and `PxPatch` structures have been updated to include global filter state.

## v1.4.3 (2026/01/05)

This update delivers **Full Combo Filter Support** at all filter slopes, including the gentle **6 dB/oct (1-pole)** setting.

### Features
*   **True Combo Modes at 6 dB/oct:**
    *   **New Architecture:** Implemented parallel independent 1-pole filter stages (`combo_lp_state`, `combo_hp_state`) specifically for combo modes (`LP+BP`, `LP+HP`, `BP+HP`) when `poles == 1`.
    *   **Accurate Summation:** Ensures mathematically correct and musically useful signal summation for these combinations, which was previously limited or approximated at 6 dB/oct.
    *   **Optimized:** Standard single modes (LP, HP, BP, Notch, Allpass) continue to use the efficient shared-state path.

### Backward Compatibility
*   Fully backward compatible. Existing patches using steeper slopes (12/18/24 dB/oct) or single modes at 6 dB/oct use existing code paths and sound identical.

## v1.4.2 (2026/01/05)

This update introduces full support for **1-Pole (6 dB/oct)** filtering, enabling gentler, broader tonal shaping.

### Features
*   **True 1-Pole Filter Support:**
    *   **New Pole Option:** `PX_SetFilterParam(s, PX_FILTER_PARAM_POLES, 1.0f)` now enables a true 6 dB/oct slope.
    *   **Unified Modes:** Works with existing `LP`, `HP`, `BP`, and `Allpass` modes. (Note: BP and Allpass are 1-pole approximations).
    *   **Optimized Path:** Internally bypasses the standard SVF stages when running in 1-pole mode for efficiency.

### Backward Compatibility
*   Fully backward compatible. Existing patches using 2, 3, or 4 poles are unaffected.
*   `PX_FILTER_PARAM_POLES` clamping has been updated to accept values down to `1.0`.

## v1.4.1 (2026/01/05)

This update adds comprehensive support for **Polyphonic Aftertouch** (per-note pressure) within the unified Modulation Matrix.

### Features
*   **Polyphonic Aftertouch:**
    *   **New Source:** Added `PX_MOD_SRC_POLY_AFTERTOUCH` to the modulation matrix.
    *   **Per-Note Control:** Allows modulating parameters (e.g., filter cutoff, timbre) independently for each held note based on its individual pressure.
    *   **Voice Handling:** Implemented per-voice pressure storage (`poly_aftertouch_pressure` in `Voice` struct). Pressure state is automatically reset to 0.0 when a voice is triggered or stolen to prevent state pollution.
*   **Documentation:**
    *   Updated `@section mod_matrix` in `polysonix.h` with new usage examples for Polyphonic Aftertouch and Pitch Bend.
    *   Clarified documentation for `PX_SetPitchBendRange`.

### API Changes
*   **New Function:** `PX_PolyAftertouch(PxSynth* s, int key_id, float pressure)`: Updates the pressure for the active voice corresponding to `key_id`.
*   **New Command:** `PX_CMD_POLY_AFTERTOUCH`: Internal command to safely handle pressure updates from the API thread.

### Backward Compatibility
*   Fully backward compatible. The new modulation source defaults to 0.0, and existing code not calling `PX_PolyAftertouch` will function unchanged.

## v1.4 (2026/01/05)

This release completes the unification of the modulation system by treating **Mod Wheel** and **Pitch Bend** as first-class citizens in the Modulation Matrix.

### Features
*   **Unified Modulation Matrix:**
    *   **New Sources:** Added `PX_MOD_SRC_MODWHEEL` (CC #1) and `PX_MOD_SRC_PITCHBEND` to the matrix.
    *   **Mod Wheel:** Maps MIDI CC #1 (0.0 to 1.0) to any destination (e.g., LFO Depth for vibrato, Filter Cutoff).
    *   **Pitch Bend:** Maps Pitch Bend (normalized 0.0-1.0 to bipolar -1.0 to +1.0) to any destination.
    *   **Pitch Bend Range:** Added `pitchbend_range_semitones` (default 2.0) to `PxPatch` for easier scaling when routing pitch bend to frequency.
*   **Safety & Polish:**
    *   **Input Validation:** `PX_SetModMatrixSlot` now clamps modulation amounts to [-1.0, 1.0] and logs errors to `stderr` for invalid slot/source/dest indices.
    *   **Documentation:** Added detailed usage examples for the Modulation Matrix in `polysonix.h`.

### API Changes
*   **New Control Functions:**
    *   `PX_ControlChange(s, cc_num, value)`: Handles MIDI CC messages (currently only CC #1 Mod Wheel is routed).
    *   `PX_PitchBend(s, value)`: Handles Pitch Bend messages (accepts 0.0-1.0 normalized).
    *   `PX_SetPitchBendRange(s, semitones)` / `PX_GetPitchBendRange(s)`: Helper for pitch bend scaling.
*   **Struct Updates:** Added `modwheel_value` and `pitchbend_value` to `PxSynth`, and `pitchbend_range_semitones` to `PxPatch`.

### Backward Compatibility
*   New modulation sources are zero by default.
*   The modulation matrix defaults to disabled slots, ensuring existing patches sound unchanged.

## v1.3 (2026/01/05)

This major update introduces a full **Modulation Matrix** for Velocity and Channel Aftertouch, replacing the previous hard-wired routings with a flexible, 16-slot routing system.

### Features
*   **Modulation Matrix:** 16-slot matrix allowing Velocity and Channel Aftertouch to be routed to freely selectable destinations.
    *   **Sources:** `PX_MOD_SRC_VELOCITY`, `PX_MOD_SRC_AFTERTOUCH`.
    *   **Destinations:**
        *   ADSR 1/2/3 parameters (Attack, Decay, Sustain, Release times/levels).
        *   LFO 1/2/3 Frequency and Depth.
        *   Oscillator Parameters (modA, modB, modC).
    *   **Exponential Scaling:** ADSR time modulations use natural-sounding exponential scaling.
    *   **Flexible Amounts:** Each slot has an independent amount (-1.0 to +1.0).

### API Changes
*   **New Matrix API:**
    *   `PX_SetModMatrixSlot(s, slot, src, dest, amount)`
    *   `PX_EnableModMatrixSlot(s, slot, enabled)`
    *   `PX_ClearModMatrix(s)`
*   **Removed Hard-wired Functions:** The previous Velocity/Aftertouch setters (e.g., `PX_SetVelocityToAmp`, `PX_SetAftertouchToFilterCutoff`) have been removed from the internal logic. The API functions remain for compilation compatibility but perform no action. Users should migrate to the Modulation Matrix.

### Backward Compatibility
*   **Default State:** The matrix defaults to all slots disabled (amount 0.0). Existing patches that did not use the specific v1.2 velocity features will sound identical.
*   **Migration:** Code using v1.2 velocity functions must be updated to use `PX_SetModMatrixSlot` to achieve similar results.

## v1.2 (2026/01/04)

This release introduces expressive capabilities with full support for **MIDI Velocity** and **Channel Aftertouch**.

### Features
*   **MIDI Velocity Support:** The engine now responds to note velocity (0.0 - 1.0).
    *   **Amplitude Scaling:** Velocity can scale the note's volume (`VelocityToAmp`).
    *   **Filter Brightness:** Harder hits can open the filter cutoff (`VelocityToFilterCutoff`).
    *   **Attack Time Scaling:** High velocity can shorten the ADSR attack time for punchier sounds (`VelocityAttackScaling`).
    *   **Timbre Modulation:** Direct mapping of velocity to `modA` (Param1) of the waveform bytecode (`VelocityToParam1`).
*   **Channel Aftertouch:** Added support for monophonic channel pressure (`PX_ChannelAftertouch`).
    *   **Filter Sweep:** Pressure can modulate filter cutoff (`AftertouchToFilterCutoff`).
    *   **Vibrato Depth:** Pressure can introduce pitch modulation (`AftertouchToVibrato`).

### API Changes
*   **Updated `PX_NoteOn`:** The signature has changed to accept a 5th argument: `float velocity`.
*   **New `PX_NoteOnLegacy`:** A helper function provided for backward compatibility with the old 4-argument signature (defaults to full velocity).
*   **New Control Functions:** Added setters and getters for all new velocity and aftertouch parameters (e.g., `PX_SetVelocityToAmp`, `PX_SetAftertouchToVibrato`).

### Backward Compatibility
*   All new modulation parameters default to `0.0`. Existing patches will sound exactly the same until these features are explicitly enabled.

## v1.1.8 (2026/01/04)

This release seals the current codebase as **Version 1.0Alpha1**, marking a significant milestone in stability and mathematical precision.

### Fixes
*   **ADSR Envelope Accuracy:** Corrected the decay and release calculations in `ADSR_Update`. Replaced the linear single-step multiplier application with an exponential `powf` calculation based on the actual sample count (`num_steps`). This fixes issues where envelopes played slower than intended during block-based updates.
*   **Interpolation Phase Continuity:** Improved the phase tracking logic in `PX_Process` for oscillator interpolation. The start phase for a new processing block is now preserved from the previous block's end phase (`phase_at_interp_end`), rather than being back-calculated from the current frequency. This ensures seamless audio continuity even under heavy frequency modulation.
*   **LFO Synchronization:** Fixed a bug in `PX_NoteOn_internal` where LFOs configured to not reset on key press (`reset_on_key_on = false`) failed to synchronize with the global LFO state. These LFOs now correctly inherit the phase and VM state from the master `template_lfo_instances`, ensuring true free-running behavior across voice reuse.

## v1.1.7 (2026/01/03)

### Fixes
*   **Waveform Generation Precision:** In `px_vm` (formerly `polysonix_wave`), fixed an issue where CPU rendering of waveforms was previously quantized to 16-bit integers. This has been updated to use 32-bit floating-point precision, eliminating quantization artifacts and significantly improving audio fidelity.

## v1.1.6 (2025/11/29)
*   **Performance optimization:** Implemented Direct Threaded Code (computed gotos) for VM dispatch.
*   **Performance optimization:** Added register caching for Instruction Pointer (IP) and Stack Pointer (SP).
*   **Performance optimization:** Added PX_LIKELY/PX_UNLIKELY branch prediction macros.
*   Verified ~24% performance improvement in benchmark suite.

## v1.1.5 (2025/07/14)
*   Fully implemented lock-free thread-safety via a command queue for control and a snapshot buffer for UI data.

## v1.1.4 (2025/07/12)
*   Enhanced filter engine with combo modes (e.g., LP+BP) and selectable 12dB, 18dB, and 24dB slopes, available for all filter types.

## v1.1.3 (2025/07/12)
*   Added MOD_C support to the parameter chain and modulation capabilities.

## v1.1.2 (2025/07/08)
*   Added Oscillator update modes allowing various quality and performance modes.

## v1.1.1 (2025/07/06)
*   Added Unilegato functioning both in polyphonic (more than 1 voice) and monophonic (single voice) instances.

## v1.1.0 (2025/07/05)
*   Stable audio generation with full parity to original monolithic version. ADSRs, LFOs, Filter, and Limiter are functional.

## v1.0.0
*   Initial port from original monolithic version.
