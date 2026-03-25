# Update Log

## v1.9.38 (2026/03/20)
- **Performance**:
  - Hoisted the per-voice modulation matrix computation (velocity/aftertouch curve evaluation, mod source population, and 16-slot matrix accumulation) out of the per-sample audio loop and into the LFO update block, which runs at a reduced rate.
  - Results are cached on the Voice struct (`cached_mod_sources`, `cached_dest_mod`) and reused at audio rate via pointer alias, eliminating redundant per-sample work across all active voices.
  - Also converted remaining multiply-add patterns in `Filter_Process_Internal` to `fmaf`: DC blocker, 1-pole state updates, allpass output, notch calculations, and 18dB pole3 state updates.

## v1.9.37 (2026/03/20)
- **Features / Fixes**:
  - Implemented a critical fix for the Virtual Machine's sub-chunk execution (`execute_sub_chunk` in `px_vm.h`). The stack boundary is now correctly enforced using the sub-chunk's frame base pointer (`outer_stack_top`) instead of the absolute stack bottom, preventing potential stack corruption and ensuring correct result returns for nested `sigma()` loops.
  - Refactored `OP_HALT` and sub-chunk exit logic to guarantee that exactly one result is left on the stack for the caller, with a safe fallback to `0.0f` on errors or empty stacks.
  - Added support for inverse math constants (`INV_PI`, `INV_TWO_PI`, `INV_PI_OVER_2`) to the VM's lexer and compiler to resolve compilation errors for several default waveforms.
  - Fixed a syntax error in the "Formant Vowel" (Wave 115) expression in `px_wave_rom.h` to restore its functionality.
  - Verified that all 256 default waveforms now compile successfully and generate unique audio output.

## v1.9.36 (2026/03/20)
**Feature: Sample Bank ROM Format & sbgen Utility**

- **New Format:** Introduced the `.sbr` (Sample Bank ROM) binary format for storing packed sample libraries. The format is flat, memory-mappable, and designed for fast lookup in synthesizers and samplers.
  - Layout: `SBR_Header` → `SBR_Group[]` → `SBR_Entry[]` → PCM16 data blob.
  - Groups store folder/category names (up to 39 chars). Entries store per-sample metadata: name, loop points, sample rate, MIDI base note, fine tune, and a format bitfield (depth, channels, loop mode).
  - Format flags cover PCM8/12/16/FLOAT32 depth, mono/stereo (interleaved), and loop/pingpong/oneshot modes.
- **New API:** Added `px_samplebank.h` / `px_samplebank.c` — a reusable C11 API for creating, reading, modifying, and writing `.sbr` banks programmatically.
  - `sbr_bank_create/destroy`, `sbr_group_add/remove/rename/find`, `sbr_entry_add/remove/rename/set_loop`, `sbr_bank_write/read`.
  - Dynamic in-memory `SBR_Bank` handle with auto-growing arrays for groups, entries, and sample data.
- **New Tool:** Added `sbgen` CLI utility (`sbgen.c`) for packing WAV directory trees into `.sbr` files.
  - `sbgen create [--oneshot] <input_dir> <output.sbr>` — builds a bank from a directory-per-group structure.
  - `sbgen list <file.sbr>` — dumps the full TOC with all entry details.
  - WAV reader supports 16-bit and 24-bit PCM (mono and stereo). 24-bit is converted to 16-bit on import. Unsupported formats are skipped with specific diagnostic messages.
  - Name truncation is warned but never causes samples to be skipped.
- **Documentation:** Added `sbgen/README.md` with full format specification, CLI usage, and API reference.

## v1.9.35 (2026/03/10)
- **Security**:
  - Replaced unsafe `rand()` function calls with the application-specific, lock-free `px_rand()` generator in all critical paths.
  - Modified `LFOInstance_Init` and related initialization functions to accept an explicit RNG state pointer for improved thread safety and determinism.
  - Implemented a thread-safe atomic RNG state management in `px_vm.h` using `stdatomic.h` for compile-time wave generation.
  - Eliminated potential priority inversion and performance jitters in the real-time audio thread caused by global `rand()` mutexes.

## v1.9.34 (2026/03/09)
- **Performance**:
  - Optimized jump instruction patching in the Polysonix VM compiler (`px_vm.h`).
  - Refactored patching logic to use pointer arithmetic and branchless fast-paths for offset calculations.
  - Implemented robust 32-bit intermediate offset validation to prevent silent 16-bit truncation errors during compilation of very large expressions.
  - Added centralized `patch_jump_to` helper to improve maintainability and safety of the compilation backend.

## v1.9.33 (2026/03/08)
- **Features / Fixes**:
  - Optimized mathematical operations inside the `PX_Process` loop by leveraging `fmaf` for improved performance and calculation precision. Key replacements include:
    - Modulation matrix destination accumulations (`dest_mod[slot->dest] = fmaf(...)`)
    - ADSR LFO output scaling adjustments (`adsr_lfo_scale[i] = fmaf(...)`)
    - Aggregate LFO target calculations (pitch, cutoff, amp, pan, params)
    - Cubic glide interpolation curve calculation (`t = t * t * fmaf(-2.0f, t, 3.0f)`)
    - Glide frequency approach (`v->frequency = fmaf(...)`)
    - Oscillator tuning mapping (`tuning_st = fmaf(...)`)
    - Phase distortion processing (`effective_phase = fmaf(...)`)
    - Ring modulation and cross modulation wet/dry mixing (`raw_sample = fmaf(wet - raw_sample, depth, raw_sample)`)
    - Stereo mixer panning applications (`mixed_sample_l_f = fmaf(final_sample_l, gain_l, mixed_sample_l_f)`)

## v1.9.32 (2026/03/08)
- **Features / Fixes**:
  - Optimized velocity and aftertouch curve calculations (`PX_CURVE_EXP`, `PX_CURVE_LOG`, `PX_CURVE_S`) in `polysonix.h` by utilizing fused multiply-add (`fmaf`) and replacing expensive `powf` and `logf` calls with fast algebraic equivalents and accurate rational polynomial approximations.

## v1.9.31 (2026/03/07)
- **Features / Fixes**:
  - Optimized template LFO phase wrapping calculation in `polysonix.h` by utilizing fused multiply-add (`fmaf`) and replacing the expensive `fmodf` operation with fast branch-predicted conditionals, improving overall LFO processing efficiency.

## v1.9.30 (2026/03/07)
- **Features / Fixes**:
  - Replaced legacy ternary conditionals in `px_wave_rom.h` with branchless `mix` and `step` combined with `fma` expressions to improve wave calculation efficiency.
  - Added new inverse math constants (`INV_PI`, `INV_TWO_PI`, `INV_PI_OVER_2`) across the VM (`px_vm.h`, `px_vm.comp`, and `tools/transpile_waves.py`) to reduce slow floating-point divisions.
  - Optimized the `OP_HYPOT` computation in `px_vm.comp` to utilize native FMA instructions.
  - Fixed `OP_LFSR_CLOCK` and `OP_LFSR_NOISE` table-mode phase calculations in the compute shader to use `INV_TWO_PI`.
  - Removed dead `call_stack_top` references from the compute shader's default loop block.

## v1.9.29 (2026/03/07)
- **Features / Fixes**:
  - Optimized the factory waveforms in `px_wave_rom.h` by aggressively replacing nested `min`/`max` bounds, explicit linear interpolations, and boolean branching conditions with native Polysonix VM branchless opcodes (`clamp`, `mix`, `ramp`, `step`, and `sign`).
  - Extended the AST parsing capabilities of `tools/transpile_waves.py` to seamlessly convert these operations into high-performance `fmaf` and macro polyfills inside `px_wave_native.h`.

## v1.9.28 (2026/03/07)
- **Features / Fixes**:
  - Introduced `step`, `sign`, and `inversesqrt` math operations to the Virtual Machine natively.
  - Re-architected execution paths for `prob`, `select`, `smooth_select`, `hypot`, conditional opcodes, and `lfsr_val` to utilize advanced branchless fast-paths (`mix`, `step`, `sign`, `inversesqrt`) in both `px_vm.h` and the GLSL shader (`px_vm.comp`).

## v1.9.27 (2026/03/07)
- **Features / Fixes**:
  - Fixed `PxOscillator` structure padding so that it aligns cleanly to exactly 64 bytes.
  - Changed `OP_MARKOV` execution to correctly pop values into a local array, eliminating fragile stack pointer math.
  - Implemented branchless Markov matrix evaluation in the GLSL compute shader for faster processing.
  - Optimized `lfsr_get_bit` in the GLSL compute shader to load 32-bit words directly instead of utilizing byte offsets.
  - Fixed LFSR advancement to explicitly guard against `bit_length == 0`.
  - Converted loose magic numbers in shader code to explicitly named constants.

## v1.9.26 (2026/03/07)
- **Features / Fixes**:
  - Re-architected `px_vm.comp` to execute 1 thread per wavetable sequentially instead of 1 thread per sample, completely fixing stateful DSP data races for structures like Markov chains and LFSR states.
  - Refactored `OP_MARKOV` in both the CPU VM (`px_vm.h`) and GPU compute shader (`px_vm.comp`) to read its state matrix using direct Stack-Peek pointer offsets. This eliminates expensive array copying into local variables, saving CPU cache bandwidth and avoiding GPU register spilling.

## v1.9.25 (2026/03/07)
- **Features / Fixes**:
  - Implemented `OP_MARKOV` for probability-based state transitions using dynamically-sized square matrices and a rising-edge trigger.

## v1.9.24 (2026/03/07)
- **Features / Fixes**:
  - Increased token stack limit in `px_vm.h` from 256 to 1024 to support parsing complex FM modulation scripts without exhausting tokens.
  - Made `OP_RAND` execution deterministic per-cycle by seeding it predictably with the phase and `rand_offset`, preventing undesirable white-noise generation.
  - Removed division-by-zero VM error throws for `OP_DIV`. Division by zero now naturally yields `INFINITY` or `NaN` to better support audio distortion artifacts.

## v1.9.23 (2026/03/07)
- **Features**:
  - Implemented advanced math opcodes (`exp2`, `log2`, `expm1`, `log1p`, `hypot`, `copysign`, `scalbn`, `remquo`, `nextafter`, `fdim`, `nan`, `inf`, `lgamma`, `tgamma`) and the FMA family (`fma`, `fms`, `fnmadd`, `fnmsub`) in the GLSL compute shader (`px_vm.comp`).
- **Maintenance**:
  - Reorganized `Makefile` test targets to output binaries directly into the `test/` directory to prevent repository root clutter. Added `.gitignore` for test artifacts.

## v1.9.22 (2026/03/07)
**Feature: VM Taming Opcodes**

- Added `OP_CLAMP`, `OP_MIX`, and `OP_RAMP` opcodes to the VM to control bounded ranges, precise blending, and linear envelopes respectively.
- Syntax: `clamp(value, min, max)`, `mix(param, v1, v2)`, `ramp(start, end, time)`.
- Implemented entirely branchless natively using `fminf`, `fmaxf`, and `fmaf`.
- Fully supported across the CPU interpreter (`px_vm.h`), the GLSL compute shader (`px_vm.comp`), and native transpiler (`transpile_waves.py`).

## v1.9.21 (2026/03/07)
**Bug Fix: Safe Index Clamping for OP_SELECT/OP_SMOOTH_SELECT**

- Fixed an issue where out-of-bounds or edge-case floating-point parameters in `OP_SELECT` and `OP_SMOOTH_SELECT` could result in out-of-bounds array accesses or integer overflow behavior due to premature casting.
- The floating-point scaling index is now safely clamped to `[0.0, n - 1]` before extraction, ensuring stability for edge case parameters like `NaN` or `+Inf`.
- Parity maintained across `px_vm.h` (CPU) and `px_vm.comp` (GPU).

## v1.9.20 (2026/03/06)
**Feature: Smooth Selection Operator**

- Added `OP_SMOOTH_SELECT` opcode to VM for fractional linear interpolation across a variable-length list.
- Syntax: `smooth_select(param, v1, v2, ..., vn)`.
- Allows for sub-index lerping for creamy morphs and transitions. Similar to `select`, maps the `param` value `[0..1]` across the N-1 intervals of the array elements.
- Fully supported across the CPU interpreter and GLSL compute shader, leveraging `fma`/`fmaf`.

## v1.9.19 (2026/03/06)
**Feature: Dynamic Selection Operator**

- Added `OP_SELECT` opcode to VM for dynamic selection from a variable-length list.
- Syntax: `select(param, v1, v2, ..., vn)`.
- Implements fast, branchless clamping with `fmaf` for mapping the `param` value `[0..1]` to a 0-indexed array choice.
- Fully supported across the CPU interpreter, GLSL compute shader, and Python transpiler.
- Allows for nested operations and probabilities as operands to easily construct non-linear morphs and sequence logic.


## v1.9.18 (2026/03/06)
- Added `OP_PROB` opcode to VM for probabilistic ternary operations (`prob(chance, true_expr, false_expr)`).

## v1.9.17 (2026/03/06)
**Feature: Advanced Math Functions for VM**

*   **Virtual Machine Additions:**
    *   Expanded the Polysonix Waveform Scripting Language with 14 new advanced mathematical functions.
    *   Added functions: `exp2`, `log2`, `expm1`, `log1p`, `hypot`, `copysign`, `scalbn`, `remquo`, `nextafter`, `fdim`, `nan`, `inf`, `lgamma`, `tgamma`.
    *   Mapped new functions directly to their underlying C `<math.h>` equivalents for optimal execution.
    *   Special handling added in `tools/transpile_waves.py` to seamlessly convert 0-arity functions (`nan()`, `inf()`) and functions utilizing pointer returns (`remquo`) to native C implementations.
*   **Documentation:**
    *   Updated `README.md` and `doc/px_vm.md` with comprehensive explanations and practical synthesizer use-cases for each new mathematical operation.

## v1.9.16 (2026/03/06)
**Performance Optimization: Titanium FMA DSP & New VM Instructions**

*   **Optimization:**
    *   Refactored `dsp_math.h` with the "Titanium" version, leveraging heavily inlined `fmaf` and vectorized `_mm_fmadd_ps` calls for faster scalar and SSE4.1 execution of fast trigonometric approximations.
    *   Enhanced mathematical boundary safety by decoupling linear and trigonometric mappings during `InitFastDSP` table initialization, preventing `NaN` pollution in inverse trig routines.
*   **Virtual Machine Additions:**
    *   Added new `OP_FMS`, `OP_FNMADD`, and `OP_FNMSUB` instructions to the `px_vm` stack machine execution loop.
    *   These new functions compile directly to standard FMA scalar instructions without requiring unsupported non-ISO `<math.h>` dependencies.

## v1.9.15 (2026/03/06)
**Performance Optimization: Fused Multiply-Add (FMA)**

*   **Optimization:**
    *   Replaced numerous instances of `(a * b) + c` and `a + (b * c)` with the `fmaf(a, b, c)` function across the `polysonix.h` and `px_vm.h` core processing paths.
    *   This leverages hardware-accelerated Fused Multiply-Add instructions (like FMA3 or FMA4) on supported architectures, resulting in faster execution and improved precision due to single rounding.
    *   Affected areas include: SVF filter processing, enhanced limiter envelope calculation, ADSR envelope levels, cubic interpolation (Horner's method), and soft clipping Padé approximations.
    *   Exposed `fma` as a new opcode directive directly usable through scripting.

## v1.9.14 (2026/03/05)
**Security Fix: Oscillator Wave Index Validation**

*   **Vulnerability Fix:**
    *   **The Issue:** During preset deserialization, the oscillator `wave_idx` was loaded from the file without any bounds checking. A malicious or corrupted preset could specify an index larger than the number of valid waveforms. Since this index is used directly in the audio processing loop to access the waveform data array, it could lead to out-of-bounds memory access, causing a crash or potential information leakage.
    *   **The Fix:** Added a robust bounds check in `px_patching.h`. Any loaded `wave_idx` that exceeds or equals `NUM_WAVEFORMS` (256) is now automatically clamped to `0` (a safe default waveform).
    *   **Verification:** Added a dedicated security test `test/test_security_wave_idx.c` that confirms the fix by attempting to load a corrupted preset with an invalid index.

## v1.9.13 (2026/02/28)
* **Optimization**: Replaced the division operation `progress / total_phase` in the oscillator interpolation logic with a precomputed multiplication `progress * v->osc_inv_total_phase[o]`, yielding a ~16% speedup in the simulated interpolation hot path.
* **Optimization**: Converted several safe divisions by constants (e.g., `(float)UINT32_MAX`) and explicitly computed variables in `PX_Process` to equivalent multiplication-by-reciprocal operations to avoid DSP pipeline stalls.

## v1.9.12 (2026/02/24)
**Performance: Optimized LFO Phase Wrapping**

This release optimizes the LFO update loop, significantly reducing CPU usage for patches with heavy modulation.

*   **Fast Phase Wrapping:**
    *   **The Issue:** The LFO phase update logic previously used an unconditional `fmodf` call to wrap the phase accumulator. `fmodf` is computationally expensive (division/modulus) and was being called every update tick for every LFO, even though the phase increment for an LFO is typically very small (< 1.0).
    *   **The Fix:** Replaced `fmodf` with a conditional subtraction (`if (phase >= 1.0f) phase -= 1.0f;`) for the common case.
    *   **Safety:** Added a robust fallback to `fmodf` inside the conditional block. This ensures that extreme modulation scenarios (e.g., audio-rate FM where phase increment > 1.0) are still handled correctly, maintaining mathematical precision.
    *   **Impact:** Benchmarks show a **~7.6% improvement** in LFO update throughput.

## v1.9.11 (2026/02/21)
**Refactor: Single-Header Compliance & Code Health**

This release focuses on strict adherence to the single-header library pattern and improving the maintainability of the `px_vm` core.

*   **Single-Header Architecture:**
    *   **The Issue:** `px_vm.h` previously exposed function definitions in the header without a guard, leading to potential multiple-definition errors when included in multiple translation units.
    *   **The Fix:** Split `px_vm.h` into a declarative header section (guarded by `PX_VM_H`) and an implementation section (guarded by `PX_VM_IMPLEMENTATION`).
    *   **Integration:** `polysonix.h` automatically defines `PX_VM_IMPLEMENTATION` before including `px_vm.h` inside its own implementation block, ensuring seamless integration.

*   **Code Health:**
    *   **Merged Function Tables:** Consolidated the redundant `vm_functions` (compiler) and `functions` (parser) arrays into a single `px_functions` table, reducing code duplication and maintenance overhead.
    *   **API Visibility:** Explicitly declared public API functions (like `tokenize`, `execute_bytecode`) in the header section to ensure proper visibility for external consumers.
    *   **Safety:** Added regression tests to verify that the library can be linked against multiple client units without symbol conflicts.

## v1.9.10 (2026/02/21)
**Fix: VM Comparison Logic & Native Accuracy**

This release fixes critical discrepancies between the Bytecode VM and the Native Transpiler caused by inconsistent floating-point comparisons.

*   **Standardized Comparisons:**
    *   **The Issue:** The VM previously used an epsilon-based "fuzzy" logic for inequality operators (e.g., `GT` was `a > b + epsilon`). This caused it to return `false` for values very close to thresholds (e.g., `sin(x) > 0` near zero crossings), diverging from standard C behavior used in native code.
    *   **The Fix:** Updated `px_vm.h` (CPU) and `px_vm.comp` (GPU) to use standard floating-point comparisons (`>`, `>=`, `<`, `<=`) for all inequality operators. Equality operators (`EQ`, `NE`) retain the fuzzy check for robustness.
*   **Verification:**
    *   **New Tool:** Added `tools/verify_all_waves.c` to rigorously compare the output of the bytecode interpreter against the native C implementation.
    *   **Result:** Verified that all 256 factory waveforms now produce identical output across both execution modes.

*   **Security Fix: Parser Stack Overflow**
    *   **The Issue:** The expression parser (`parseExpression`) was recursively calling itself for parenthesized expressions without checking recursion depth. A malicious or overly complex script could trigger a C-stack overflow.
    *   **The Fix:** Implemented a recursion depth counter and `MAX_PARSE_DEPTH` (default 200) limit. The parser now safely aborts with an error if the expression nesting exceeds this limit.

## v1.9.9 (2026/02/21)
**Security Fix: Compute Shader Bounds Checking**

*   **The Issue:** The `OP_JUMP` and `OP_JUMP_IF_FALSE` instructions in the `px_vm.comp` compute shader did not validate the target instruction pointer against the bytecode length. This could allow malicious bytecode to access memory outside the designated buffer.
*   **The Fix:** Added runtime bounds checks in the shader. If a jump target is out of range, the shader now halts execution safely. This required updating `VmMetadataBuffer` to include the `bytecode_length` field.

## v1.9.8 (2026/02/21)
**Fix: Oscillator Phase Wrapping & VM Security**

This release hardens the engine against extreme modulation scenarios and potential security vulnerabilities in the bytecode interpreter.

*   **Robust Phase Wrapping:**
    *   **The Issue:** Under extreme frequency modulation (e.g., negative frequencies or increments > 1.0 per sample), the oscillator phase could drift outside the valid [0.0, 1.0) range, causing audio glitches or silence.
    *   **The Fix:** Implemented a unified, branchless/fast-cast wrapping logic in `PX_Process`. It strictly enforces the phase range using integer casting, correctly handling both large positive increments and negative increments during forward or reverse playback.

*   **VM Security:**
    *   **Bounds Checking:** Added runtime bounds checking to all jump instructions (`OP_JUMP`, `OP_JUMP_IF_FALSE`, `OP_SIGMA_CHECK`) in the VM.
    *   **The Impact:** Prevents malicious or malformed bytecode from jumping to arbitrary memory locations (heap buffer over-read), ensuring the VM halts safely with a descriptive error instead of crashing or leaking data.

## v1.9.7 (2026/02/21)
**Performance: Secure Bytecode Benchmarking**

Internal release focused on verifying the performance impact of the new security measures.

*   **Benchmarks:** Verified that the secure bytecode interpreter (with bounds checks enabled) maintains a ~10% performance improvement over the v1.8.10 baseline for complex patches, confirming that safety measures introduce negligible overhead when compiled with optimization.

## v1.9.6 (2026/02/21)
**Performance: Optimized Panning Calculation**

This release optimizes the main per-sample audio loop by caching trigonometric panning coefficients.

*   **Smart Pan Caching:**
    *   **The Issue:** The engine was calculating `cosf` and `sinf` for every sample, even when the pan position hadn't changed.
    *   **The Fix:** Added a caching mechanism to the `Voice` struct (`cached_pan_l`, `cached_pan_r`, `last_pan_effective`). The engine now checks if the effective pan (Base + LFO) has changed significantly (> 0.001f) before recalculating the gains.
    *   **Impact:** Reduces CPU usage by eliminating redundant trigonometric calls for voices with static or slowly moving pan positions.

## v1.9.5 (2026/02/21)
**Performance: Redundant Filter Calculation Check**

This release optimizes the main per-sample filter loop by eliminating unnecessary mathematical operations for static or slowly-moving filters.

*   **Filter Coefficient Caching:**
    *   **The Issue:** The expensive trigonometric calculations (`sinf`, `tanf`) required to derive the State Variable Filter coefficients were being performed for every single sample, even if the filter cutoff and resonance had not changed from the previous sample.
    *   **The Fix:** Added a caching mechanism to the `Voice` struct. The engine now stores the last used cutoff, resonance, mode, and pole settings. Before calculating new coefficients, it checks if the current target values differ from the cached values.
    *   **Impact:** If the values match (which is true for the vast majority of samples unless audio-rate modulation is active), the expensive math is skipped. This results in a measured ~7% improvement in total engine throughput for standard polyphonic patches. Correctness is guaranteed as any change in parameters immediately triggers a recalculation.

## v1.9.4 (2026/02/21)
**Performance: Optimized Filter Key Tracking**

This release includes a focused optimization for the filter engine, reducing the CPU cost of frequency modulation.

*   **Key Tracking Hoisting:**
    *   **The Issue:** The filter key tracking factor (`exp2f((midi_note - 60) / 12 * amount)`) was previously recalculated for every sample in the audio block, despite `midi_note` and `filter_key_track` being constant for the duration of the block.
    *   **The Fix:** This calculation has been hoisted out of the per-sample loop. It is now computed once per block for each active voice and stored in a stack array.
    *   **Impact:** Reduces the number of expensive `exp2f` calls by a factor of `samples_per_block` (typically 256 or 512) for this specific parameter, yielding a measurable improvement in overall engine performance (approx 2.3% in benchmarks).

## v1.9.3 (2026/02/21)
**Dev Tooling: Transpilation for Benchmarks**

This release introduces new tooling to assist in performance optimization and debugging, focusing on the overhead of the VM interpreter vs. native C execution.

*   **Native Waveform Transpiler:**
    *   **New Tool:** Added `tools/transpile_waves.py`, a script that parses the mathematical expressions in the ROM (`px_wave_rom.h`) and generates equivalent, optimized native C functions (`px_wave_native.h`).
    *   **Benchmarking Mode:** Added the `PX_BENCHMARK_NATIVE_WAVES` preprocessor definition. When enabled, the VM will bypass the bytecode interpreter and execute the transpiled C functions directly.
    *   **Performance Insight:** Benchmarks run with this mode show an average performance improvement of ~61% across the factory library, with complex patches (using `sigma` loops or heavy modulation) seeing gains up to 90%. This validates the potential of Ahead-of-Time (AOT) compilation strategies for future versions.
    *   **Safe Defaults:** In standard builds (without the benchmark flag), the engine continues to use the proven bytecode interpreter to ensure binary size remains small and behavior remains strictly dynamic.

*   **Internal Refactoring (VM Logic):**
    *   **Logic Extraction:** Core logic for Random Number Generation (`OP_RAND`) and LFSR operations (`OP_LFSR_*`) has been extracted into static inline helper functions within `px_vm.h` (`vm_rand`, `vm_lfsr_val`, etc.).
    *   **Code Reuse:** These helpers are now shared between the interpreted path and the transpiled native functions, ensuring identical behavior across both execution modes and improving maintainability.

## v1.9.2 (2026/02/21)
**Performance: Optimized Pitch Modulation**

This release optimizes core frequency calculations by leveraging base-2 exponential functions, yielding significant performance gains in the main audio loop.

*   **Fast Exp2 Optimization:**
    *   **The Issue:** The engine previously used the general-purpose `powf(2.0f, x)` function for all pitch-related calculations (MIDI-to-Hz conversion, LFO modulation, key tracking, oscillator detuning). `powf` is computationally expensive as it handles arbitrary bases.
    *   **The Fix:** Replaced all instances of `powf(2.0f, x)` with `exp2f(x)`. This specialized function is significantly faster on modern FPUs while maintaining mathematical equivalence.
    *   **VM Optimization:** Updated the `OP_POW` instruction in both the CPU VM and GPU Compute Shader to conditionally use `exp2` when the base is exactly `2.0f`, improving performance for scripts that perform octave shifting or pitch scaling.
    *   **Impact:** Micro-benchmarks show an ~8.7x speedup for the specific pitch calculation loop, translating to reduced CPU usage per voice.

## v1.9.1 (2026/02/21)
**Performance: Optimized Transcendental Functions**

This maintenance release introduces a high-performance optimization for the `tanh` function, significantly reducing CPU load for saturation-heavy patches.

*   **Fast Tanh Optimization:**
    *   **The Issue:** The standard library `tanhf` function is computationally expensive (15-20 cycles). This bottlenecked performance in both the filter's drive stage (called up to 8 times per sample) and in VM scripts using the `tanh` opcode for wave shaping.
    *   **The Fix:** Implemented a fast polynomial approximation (Padé [2/2]) for `tanh`.
    *   **Architecture:**
        *   Added `vm_fast_tanh` to `px_vm.h` to optimize the `OP_TANH` opcode.
        *   Re-implemented `fast_tanh` in `polysonix.h` for the filter stage, ensuring separation of concerns while sharing the optimization strategy.
    *   **Impact:** Benchmarks show a ~2.2x speedup for scripts dominated by saturation logic, freeing up CPU headroom for higher polyphony.

## v1.9.0 (2026/01/24)
**Refactor: Flat Math Opcodes**

This major update overhauls the `px_vm` execution engine to replace the generic function call mechanism with high-performance, flat opcodes.

*   **Flat Opcode Architecture:**
    *   **The Issue:** Previously, all mathematical functions (sin, cos, pow, etc.) were executed via a single `OP_CALL` instruction, which required decoding a secondary `FunctionID` inside a switch statement. This double-dispatch caused branch mispredictions and stalled the CPU pipeline.
    *   **The Fix:** Every math function is now a first-class citizen with its own opcode (e.g., `OP_SIN`, `OP_RAND`, `OP_LFSR_NOISE`). The main interpreter loop dispatches directly to the implementation label using computed gotos (where supported).
    *   **Inline Stack Operations:** Mathematical operations now perform stack bounds checking and value manipulation inline within the dispatch block, eliminating the overhead of helper function calls for argument popping and result pushing.
    *   **Code Size:** Common operations like `sin(x)` now consume fewer bytes in the bytecode stream (2 bytes vs 4 bytes), allowing for more complex expressions within the 1024-byte chunk limit.
    *   **Legacy Support:** The original `OP_CALL` (0x17) has been renamed to `OP_CALL_DEPRECATED` and will trigger a safe error if encountered, preserving enum alignment for existing tools.

## v1.8.12 (2026/01/23)
**Fix: Unsafe Oscillator Phase Wrapping**

This release resolves a critical logic bug where the oscillator phase could drift out of bounds under extreme modulation or high-frequency conditions.

*   **Safe Phase Wrapping:**
    *   **The Issue:** The phase update logic previously used a simple conditional subtraction (`if (phase >= 1.0f) phase -= 1.0f;`). If the phase increment per sample exceeded `1.0f` (e.g., during deep FM, high-pitch playback, or aggressive pitch modulation), the phase would remain > 1.0f, causing audio corruption or silence in VM scripts expecting normalized input.
    *   **The Fix:** Updated the phase wrapping logic to use integer casting (`phase -= (float)((int)phase)`). This ensures the phase is correctly wrapped into the [0.0, 1.0) interval regardless of how large the increment is, guaranteeing stability for all synthesis scenarios.

## v1.8.11 (2026/01/23)
**Fix: DSP Aliasing & Optimization**

This release addresses two critical DSP issues: aliasing artifacts from the soft clipper and CPU performance overhead in the filter.

*   **Soft Clip Anti-Aliasing:**
    *   **The Issue:** The per-voice soft clipper was previously applied *after* the oversampled filter had been decimated back to the base sample rate. Non-linear saturation generates harmonics that extend beyond Nyquist, causing them to fold back as metallic aliasing.
    *   **The Fix:** Moved the amplitude scaling and `soft_clip` logic *inside* the `Filter_Process_Oversampled` loop. Saturation is now applied at the 2x oversampled rate, allowing the subsequent FIR half-band decimation filter to effectively remove high-frequency harmonics before downsampling.

*   **Fast Tanh Optimization:**
    *   **The Issue:** The State-Variable Filter (SVF) used the standard library `tanhf` function for input drive and state saturation. This transcendental function is computationally expensive (15-20 cycles), consuming significant CPU when called 8 times per sample per voice (for 4-pole filters).
    *   **The Fix:** Replaced `tanhf` with a fast Pade-approximated polynomial (`fast_tanh`). This provides a massive ~15-20% reduction in overall CPU usage while maintaining the desired analog saturation characteristics.

## v1.8.10 (2026/01/22)
**Fix: Command Queue Spinlock**

Replaced the spinlock-based CommandQueue implementation with a true lock-free Multi-Producer Single-Consumer (MPSC) design using atomic flags.

*   **The Issue:**
    *   The previous implementation used `atomic_flag` as a spinlock around the push operation. If a UI thread was preempted by the OS while holding this lock, the audio thread (or other producer threads) would spin infinitely, burning CPU cycles and causing potential audio dropouts (priority inversion).
*   **The Fix:**
    *   **Lock-Free MPSC:** Implemented a robust "reserve-then-commit" strategy.
    *   **Atomic Flags:** Each slot in the ring buffer now has an `_Atomic uint8_t ready` flag.
    *   **Push:** Producers use a CAS loop to reserve a write index, write data, and then set the `ready` flag.
    *   **Pop:** The consumer checks the `ready` flag at the read index. If it's not set (data not yet committed), it treats the queue as empty, avoiding race conditions without blocking.

## v1.8.9 (2026/01/21)
**Fix: Sigma VM Stack Overflow**

This release flattens the `sigma` (summation) function implementation in the VM to use iterative bytecode instead of C-stack recursion.

*   **Iterative Sigma:**
    *   **Architecture:** Replaced recursive `execute_sub_chunk` calls with a flat loop using new opcodes (`OP_SIGMA_INIT`, `OP_SIGMA_CHECK`, `OP_SIGMA_INC`).
    *   **Safety:** Prevents C-stack overflows in real-time audio threads when using deeply nested or high-iteration summation loops.
    *   **Stability:** Removed `exit(1)` calls from VM error handling to prevent host process termination.

## v1.8.8 (2026/01/20)
**Fix: LFO Zipper Noise**

This release eliminates audible stepping artifacts ("zipper noise") when using LFOs to modulate pitch or filter cutoff.

*   **Smooth LFO Modulation:**
    *   **Interpolation:** Implemented linear interpolation for all LFO control signals (Pitch, Filter Cutoff, Amp, Pan, Params). Instead of holding a value constant for ~32 samples (the control block size), the engine now calculates a target value and smoothly ramps towards it every audio sample.
    *   **Performance:** Optimized the modulation summing logic. By moving the heavy lifting of summing multiple LFOs and scaling by ADSRs to the control-rate block (once every 32 samples), per-sample overhead is actually reduced despite the added interpolation math.
    *   **Clean Attacks:** LFO interpolation state is reset on Note On, ensuring a clean start to modulation without slew artifacts from previous voice states.

## v1.8.7 (2026/01/19)
**Fix: UI Snapshot Thread Safety**

This release resolves a data race condition where the UI thread could read partially updated ("torn") state from the audio thread, potentially leading to visual glitches or invalid values in the UI.

*   **SeqLock Implementation:**
    *   **Mechanism:** Implemented a Sequence Lock pattern using C11 atomics (`snapshot_seqlock`). The audio thread increments a counter before and after writing to the snapshot buffer. The UI thread reads the counter, copies the data, and checks the counter again to ensure validity.
    *   **Zero-Cost Writer:** The writer (audio thread) never blocks and only performs two lightweight atomic increments, ensuring zero impact on real-time audio performance.
    *   **Consistency:** Getters like `PX_GetVoiceInfo`, `PX_GetLFOInfo`, and `PX_GetLimiterInfo` now guarantee that the returned struct is a consistent, atomic snapshot of a single point in time.

## v1.8.6 (2026/01/18)
**Fix: Audio Thread Thread-Safety (PRNG)**

This update eliminates a source of potential audio dropouts by removing non-thread-safe standard library calls from the audio path.

*   **Lock-Free PRNG:**
    *   **The Issue:** The standard `rand()` function often uses a global lock (mutex) in many libc implementations. Calling this from the real-time audio thread could cause priority inversion or blocking if the UI thread was also calling `rand()`.
    *   **The Fix:** Replaced all usages of `rand()` in `PX_Process` (e.g., for noise generation, probabilistic sequencing) with `px_rand`, a context-aware, lock-free Linear Congruential Generator.
    *   **Per-Voice State:** The PRNG state is stored per-voice (`rng_state`), ensuring that random sequences are deterministic and independent for each voice.

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
