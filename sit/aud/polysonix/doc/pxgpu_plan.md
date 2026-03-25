# Polysonix GPU Acceleration Feasibility Plan

This document outlines the architectural plan for implementing a dual-path processing system in Polysonix (`PX_Process` for CPU, `PX_ProcessGPU` for GPU). It details the requirements for OpenGL 4.6 / Vulkan 1.4 interoperability, bindless data structures, and the complexities of running a stack-based VM in a compute shader.

## 1. Architecture Overview

The system will support two independent processing paths:
1.  **CPU Path (`PX_Process`)**: Retains the existing C-based interpretation of bytecode.
2.  **GPU Path (`PX_ProcessGPU`)**: Offloads voice processing to a Compute Shader (`px.comp`) leveraging modern graphics API features (Bindless SSBOs, Scalar Layout).

### 1.1. Core Components
*   **Host API**: Extended `polysonix.h` with `PX_ProcessGPU`.
*   **Shader Header (`px_vm_core.glslh`)**: Reusable VM logic (Opcodes, Stack, ALU) extracted from `px_vm.comp`.
*   **Compute Shader (`px.comp`)**: The entry point for the polyphonic synthesizer kernel.
*   **Data Marshaling**: A structured approach to syncing CPU voice state with GPU memory per audio block.

### 1.2. Refinements
*   **LFO Integration**: While modulations are pre-calculated on CPU in Phase 1, the shader will include hooks to run LFO logic (via VM include) for sample-rate modulation in Phase 2.
*   **Flags Expansion**: The bitmask flags will include hints for "Lite Mode" (skip complex VM for simple oscillators) and "Divergence Group" (for sorting voices).

## 2. Graphics API Requirements & "Bindless" Model

Targeting **OpenGL 4.6** and **Vulkan 1.4** allows us to use **Bindless Buffer Device Addresses (BDA)**. This significantly reduces driver overhead by avoiding descriptor set updates for every buffer per frame.

### 2.1. API Features Used
*   **GLSL Extension**: `#extension GL_EXT_buffer_reference2 : require`
*   **GLSL Extension**: `#extension GL_EXT_scalar_block_layout : require` (Ensures C-struct packing rules match GLSL).
*   **Mechanism**: Instead of binding buffers to binding points (0, 1, 2...), we pass 64-bit GPU addresses (`uint64_t`) via Push Constants. The shader casts these addresses to buffer references.

### 2.2. Fallbacks & Checks
*   **Vulkan**: Use `VK_KHR_buffer_device_address` for equivalent functionality. The `.glslh` logic is compatible via SPIR-V reflection.
    *   *Note*: When compiling for Vulkan using `glslangValidator`, use the `-G` or `--scalar-block-layout` flags to preserve scalar packing compatibility.
*   **Runtime Check**: `polysonix.h` initialization will query `glGetStringi` for extensions and fallback to CPU processing if unavailable.

## 3. Data Layouts (C / GLSL Shared Structs)

To ensure seamless data exchange, we define strict structs compatible with `std430` / `scalar` layout.

### 3.1. Types
*   **Host**: `uint64_t` (Address), `float`, `uint32_t`, `int32_t`.
*   **Shader**: `uint64_t`, `float`, `uint`, `int`.

### 3.2. Struct Definitions

#### A. Global Configuration (`PxGpuGlobal`)
Passed as a Push Constant or uniform buffer root.

```glsl
// C-equivalent: PxGpuGlobal
layout(buffer_reference, scalar) buffer PxGpuGlobal {
    uint64_t voice_buffer_ptr;    // Pointer to PxGpuVoice[]
    uint64_t output_buffer_ptr;   // Pointer to PxGpuOutput struct
    uint64_t bytecode_buffer_ptr; // Pointer to uint8_t[] (Raw Bytecode Pool)
    uint64_t lfsr_table_ptr;      // Pointer to LFSR tables
    float    sample_rate;
    float    global_time;
    uint     num_active_voices;
    uint     samples_per_block;
    float    mod_sources[16];     // Global mod sources (e.g. ModWheel, PitchBend)
};
```

#### B. Per-Voice State (`PxGpuVoice`)
Represents the state of a single voice for one processing block. Padded to `vec4` alignment where optimal.

*Constants for dirty_mask:*
```c
#define PX_DIRTY_PHASE (1u << 0)
#define PX_DIRTY_FREQ  (1u << 1)
#define PX_DIRTY_MODS  (1u << 2)
#define PX_DIRTY_ALL   (0xFFFFFFFF)
```

```glsl
// C-equivalent: PxGpuVoice
struct PxGpuVoice {
    // Oscillators (Up to 3)
    float    osc_phases[3];       // Current phase (0.0 - 1.0)
    float    osc_freqs[3];        // Target frequency (Hz)
    uint     osc_bytecode_offsets[3]; // Offset into bytecode_buffer_ptr
    float    osc_mix[3];          // Mix levels
    float    osc_pan[3];          // Pan positions
    float    padding0;            // Alignment padding

    // Modulation Inputs (Calculated on CPU per block)
    float    modA[3];             // Smoothed Mod A value
    float    modB[3];             // Smoothed Mod B value
    float    modC[3];             // Smoothed Mod C value
    float    padding1;

    // Features
    uint     flags;               // Bitmask: Active, Sync, RingMod, LiteMode, etc.
    float    rand_seed;           // Per-voice random seed
    uint     dirty_mask;          // Bitmask tracking changed fields for optimization
    uint     padding2;
};
```

#### C. Output Structure (`PxGpuOutput`)
Wraps the raw float buffer to allow metadata or multi-buffering.

```glsl
layout(buffer_reference, scalar) buffer PxGpuOutput {
    uint  frame_id;
    uint  sample_count;
    uint  error_flags[64]; // Per-workgroup error flags (debug)
    float samples[];       // Interleaved Stereo [L, R, L, R...]
};
```

#### D. Push Constants (Root)
The only data bound during the draw/dispatch call.

```glsl
layout(push_constant) uniform PushConsts {
    uint64_t global_state_addr; // Address of PxGpuGlobal struct
};
```

## 4. GPU-CPU Dialog Logic

The "Dialog" is the per-frame synchronization protocol. To hide latency, we employ async pipelining.

### 4.1. Step 1: CPU Preparation (Host)
1.  **Voice Management**: The CPU engine determines which voices are active.
2.  **Modulation Processing**: Run ADSRs and LFOs on the CPU. Calculate the effective `ModA`, `ModB`, `ModC`.
3.  **Dirty Tracking**: Update `dirty_mask` in `PxGpuVoice` to minimize data transfer if using persistent mapping.
4.  **Struct Population**: Fill a mapped host-visible staging buffer (Ring Buffer Index `N`) with `PxGpuVoice` structs.

### 4.2. Step 2: Transfer (Upload)
1.  **Persistent Map**: Write directly to persistent mapped memory (pinned).
2.  **Flush**: `glFlushMappedBufferRange` ensures visibility (skip explicit barriers if coherent).

### 4.3. Step 3: Compute Dispatch (GPU)
1.  **Pipeline**: Bind `px.comp` pipeline.
2.  **Push Constants**: Set the address of the `PxGpuGlobal` buffer.
3.  **Dispatch**:
    *   `groupCountX`: `(num_samples_in_block + 63) / 64`
    *   `groupCountY`: `num_active_voices`
    *   `groupCountZ`: `1`

### 4.4. Step 4: Execution & Mixing (Shader)
1.  **Workgroup Size**: Explicitly `layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;`.
2.  **Thread ID**: `gl_GlobalInvocationID.x` = Sample Index, `.y` = Voice Index.
3.  **Fetch**: Load `PxGpuVoice` data for `Voice[y]`.
4.  **Execute**: Run VM for each Oscillator (0..2) using `px_vm_core.glslh`.
5.  **Workgroup Mixing**:
    *   Declare `shared float shared_L[64], shared_R[64]`.
    *   Each thread adds its result to the shared array.
    *   `barrier()`.
    *   Perform tree-reduction in shared memory.
    *   Single thread writes result to `OutputBuffer`. (Avoids global atomic contention).

### 4.5. Step 5: Readback (Host)
1.  **Async Readback**: CPU reads from Output Ring Buffer Index `N-1` (latency of 1 block).
2.  **Mapping**: Use persistent mapping for readback to avoid `glGetBufferSubData` stalls.
    *   **Tuning**: Use `glQueryCounter` to measure kernel time. Ideally <1ms for 64 voices @ 96kHz. If latency is high, increase ring buffer depth (2-3 blocks).
3.  **Post-Process**: Run global FX (Limiter, Master Filter) on CPU.

## 5. VM Complexity on GPU

Running a stack-based VM in a shader is non-trivial due to the SIMT architecture.

### 5.1. Stack Management
*   **Constraint**: No dynamic memory allocation.
*   **Solution**: `float stack[MAX_STACK]` (e.g., 32 depth) local array.
*   **Optimization**: If register spilling occurs (profile with Nsight), use shared memory for stack (per-warp/workgroup), though this adds sync overhead.

### 5.2. Control Flow & Divergence
*   **Issue**: Thread divergence kills performance.
*   **Mitigation**:
    *   **Sorting**: Sort voices by Patch ID / Bytecode ID before dispatch so warps process identical scripts.
    *   **Unrolling**: Compiler flattens small loops.
    *   **Cap**: Limit `OP_SIGMA` iterations to warp-friendly sizes (e.g., 32). Enforce in bytecode compiler.

### 5.3. Instruction Dispatch
*   **Optimization**: Fetch bytecode as `uvec4` (4 instructions) to reduce global memory transactions. Align opcodes to bytes.

## 6. Implementation Roadmap

### Phase 0: Prep & Validation
*   Define `PX_GPU_STUB_MODE` macro to bypass VM logic.
*   Stub `px.comp` with a simple sine generator (no VM).
*   **Implementation Sketch**:
    1.  Get `buffer_reference` from PushConstants.
    2.  Calculate phase: `(gl_GlobalInvocationID.x / sample_rate) * freq`.
    3.  Compute sine: `sin(phase * 2 * PI)`.
    4.  Store to shared memory array.
    5.  Perform parallel reduction.
    6.  Write to `OutputBuffer` (verify bindless write).
*   Validate bindless architecture, dispatch loop, and readback stability.

### Phase 1: Hybrid Core
*   **`px_vm_core.glslh`**: Extract logic.
*   **`px.comp`**: Implement struct definitions and dispatch.
*   **Host**: Implement `PX_ProcessGPU` with simple CPU-to-GPU copy.
*   **Test**: Verify match with CPU output.
    *   **Criteria**: `epsilon < 1e-5` for float comparison.
    *   **Criteria**: SNR (Signal-to-Noise Ratio) > 80dB.

### Phase 2: Full Offload & Optimization
*   Move ADSR/LFO logic to GPU (per-sample evaluation).
*   Implement sorting and async ring buffers.
*   **Benchmark**: Target <1ms kernel time at 64 voices / 96kHz on mid-range hardware (RTX 2060+).
*   **Debug**: Enable debug SSBO readback for error flags.
