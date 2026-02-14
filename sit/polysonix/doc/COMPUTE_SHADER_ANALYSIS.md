# Polysonix Wave Bytecode Compute Shader Analysis

This document provides a comprehensive analysis of the `px_vm.comp` GLSL compute shader, designed to execute the Polysonix waveform scripting language bytecode on the GPU.

## 1. Overview

The `px_vm.comp` shader is a single-workgroup compute shader that functions as a stack-based virtual machine (VM). It interprets a custom bytecode format, defined in `px_vm.h`, to generate a single floating-point audio sample. This allows for offloading the computationally intensive part of audio synthesis from the CPU to the GPU.

The shader is designed to be dispatched once per required audio sample, with all necessary data (bytecode, constants, parameters) provided via Shader Storage Buffer Objects (SSBOs) and uniforms.

## 2. GLSL Implementation Details

### 2.1. VM State

The VM's state is managed using global variables within the shader:

- `float vm_stack[MAX_VM_STACK]`: A fixed-size array for the operand stack.
- `int stack_top`: The stack pointer, an index into `vm_stack`.
- `int ip`: The instruction pointer, a byte-based index into the bytecode buffer.
- `int call_stack[MAX_CALL_STACK]`: An explicit stack for storing return instruction pointers. This is primarily used by the `OP_SIGMA_EXEC` state machine to return to the correct location after the operation is complete.
- `int call_stack_top`: The stack pointer for `call_stack`.
- **Sigma State Machine Globals**: A set of variables (`sigma_is_active`, `sigma_phase`, `sigma_sum`, etc.) that manage the state of a running `OP_SIGMA_EXEC` operation, as it requires executing multiple sub-chunks sequentially.
- `float loop_var_value`: Stores the current value of the loop variable (`k`) during the execution of a `sigma` body chunk.

### 2.2. Execution Flow

The shader uses a single, non-recursive execution flow controlled by a main loop inside the `main()` function.

1.  **Initialization**: The `main()` function initializes the VM state (`stack_top`, `ip`, `call_stack_top`, etc.) and sets the `ip` to the start of the main bytecode chunk.
2.  **Main VM Loop**: The shader enters a large `for` loop that acts as the VM's fetch-decode-execute cycle. A generous loop limiter (2048 iterations) is used to prevent accidental infinite loops on the GPU.
3.  **Instruction Processing**: Inside the loop, a `switch` statement decodes and executes each `OpCode`.
4.  **`OP_SIGMA_EXEC` Handling**: The `sigma` operation is handled not by recursion, but by a state machine. When `OP_SIGMA_EXEC` is encountered, it initializes the sigma state variables and changes the `ip` to point to the `start` sub-chunk. The `OP_HALT` at the end of each sub-chunk acts as a trigger to advance the state machine, evaluate the next sub-chunk, or perform the loop iteration, until the entire summation is complete.
5.  **Result**: The main loop terminates when `OP_HALT` is encountered outside of the sigma state machine or when the loop limit is reached. The final value on top of the stack is clamped to `[-1.0, 1.0]` and written to the output buffer.

## 3. Data Marshalling (C Host to GLSL Shader)

The C host application is responsible for preparing and binding several data buffers before dispatching the shader.

### 3.1. SSBOs (Shader Storage Buffer Objects)

-   **Binding 0: Bytecode Buffer (`uint`)**:
    -   A single, contiguous buffer containing the bytecode of the main expression followed immediately by the bytecode of all its `sigma` sub-chunks.
    -   The C host must "flatten" the `BytecodeChunk` structure. If a wave has a main chunk and two `sigma` sub-chunks, the buffer layout would be: `[main_chunk_bytes, sub_chunk_0_bytes, sub_chunk_1_bytes]`.
    -   Data is packed as `uint` to align with GLSL's `uint` array. The C host should copy the `uint8_t` bytecode into a `uint32_t` aligned buffer.

-   **Binding 1: Constants Buffer (`float`)**:
    -   A single buffer containing all floating-point constants from the main chunk and all sub-chunks.
    -   `OP_PUSH_CONST` uses an index into this buffer.

-   **Binding 2: LFSR Tables Buffer (`uint`)**:
    -   A tightly packed buffer containing the pre-computed bit sequences for all 16 LFSR types.
    -   Each bit sequence is stored as a series of packed bytes (8 bits per byte). The C host should copy the `uint8_t* bit_table` data for each LFSR type into this single large buffer.

-   **Binding 3: Output Buffer (`float`)**:
    -   A single-element buffer where the final computed sample will be written.

-   **Binding 4: LFSR State Buffer (`uint`, read-write)**:
    -   A single-element buffer that stores the state of the "free-running" LFSR.
    -   This buffer must be persistent across compute shader dispatches to allow the LFSR state to evolve.

### 3.2. Uniforms

-   **`VmParams` (Uniform Block)**:
    -   Contains the runtime variables (`x`, `frequency`, `modA`, etc.) for the current sample being generated.
    -   This data is identical to the `VmParams` struct in the C code.

-   **`VmMetadata` (Uniform Block)**:
    -   Provides the shader with essential offsets and metadata to navigate the flattened SSBOs.
    -   `main_chunk_offset`: The starting index (in bytes) of the main bytecode chunk within the Bytecode Buffer.
    -   `sigma_offsets_*`: The starting indices for each of the 16 possible `sigma` sub-chunks.
    -   `lfsr_periods`: An array containing the period length for each of the 16 LFSR types.
    -   `lfsr_offsets`: An array containing the starting index (in bytes) for each LFSR's bit table within the LFSR Tables Buffer.

## 4. Opcode Implementation Analysis

### 4.1. Standard Opcodes

-   **Stack, Arithmetic, Logical, and Control Flow** opcodes (`OP_PUSH_*`, `OP_ADD`, `OP_CMP_*`, `OP_JUMP_*`) are implemented with straightforward GLSL equivalents. They directly manipulate the `vm_stack` and `ip` global variables.

### 4.2. `OP_CALL` (Function Calls)

-   A `switch` statement maps the `FunctionID` to the corresponding GLSL built-in function (e.g., `sin`, `cos`, `pow`).
-   Arguments are popped from the stack, the function is executed, and the result is pushed back.
-   **`rand()` Function**: The `FUNC_ID_RAND` opcode is implemented using a simple, deterministic pseudo-random number generator (`pseudo_rand`). The PRNG is seeded at the start of shader execution using a combination of the per-wave `rand_offset` and the per-sample `x` phase value to ensure unique and repeatable random sequences.
-   **LFSR Functions**:
    -   `lfsr_val`, `lfsr_noise`, and `lfsr_clock` are implemented using the `lfsr_get_bit` helper function.
    -   This helper calculates the correct index into the LFSR Tables Buffer using the provided `lfsr_periods` and `lfsr_offsets` metadata, extracts the packed bit, and returns it as a float.

### 4.3. `OP_SIGMA_EXEC` (Summation)

This is the most complex operation in the VM and is handled by a non-recursive, finite state machine. This approach is used because robust support for recursion is not guaranteed across all GLSL drivers.

The state machine is managed by a set of global variables (`sigma_is_active`, `sigma_phase`, etc.) and unfolds over multiple iterations of the main VM loop. The `OP_HALT` instruction serves as the transition trigger between states.

1.  **Initiation (`OP_SIGMA_EXEC`)**:
    -   The opcode reads the indices for the `start`, `end`, `step`, and `body` sub-chunks.
    -   It saves the current instruction pointer (`ip`) to the call stack so execution can resume after the sigma operation is complete.
    -   It sets `sigma_is_active` to `true` and `sigma_phase` to `0` (evaluate start).
    -   It sets the `ip` to the bytecode of the `start` sub-chunk.

2.  **State Transitions (`OP_HALT`)**:
    -   When the VM executes an `OP_HALT` while `sigma_is_active` is true, it doesn't stop execution. Instead, it inspects the current `sigma_phase`:
    -   **Phase 0 (End of `start` chunk)**: Pops the result, stores it as `sigma_start_val`, advances phase to `1`, and jumps to the `end` chunk.
    -   **Phase 1 (End of `end` chunk)**: Pops the result, stores it as `sigma_end_val`, advances phase to `2`, and jumps to the `step` chunk.
    -   **Phase 2 (End of `step` chunk)**: Pops the result, stores it as `sigma_step_val`. The loop is now ready to begin. It initializes the loop counter (`sigma_current_k`) and advances to phase `3`.
    -   **Phase 3 (End of `body` chunk)**: Pops the result and adds it to the running `sigma_sum`. It then increments the loop counter (`sigma_current_k`) and compares it to the `end` value.
        -   If the loop continues, the `ip` is simply reset to the beginning of the `body` chunk for the next iteration.
        -   If the loop is finished, `sigma_is_active` is set to `false`, the final `sigma_sum` is pushed to the main VM stack, and the `ip` is restored from the call stack.

This state machine effectively "hijacks" the VM's control flow to manage the sequential evaluation of sub-chunks and the body loop, all without using GLSL recursion.

## 5. Feature Implementation Notes

-   **Free-Running LFSRs**: The shader now fully supports a "free-running" LFSR mode, bringing it to feature parity with the C VM. This is accomplished using a read-write SSBO at `binding = 4` to maintain the LFSR's state across invocations. When an LFSR function is called with a `type_id` that matches the `lfsr_type` uniform, the shader advances and uses the state from the SSBO instead of performing a table lookup.
-   **Non-Recursive Sigma**: The use of a state machine for `OP_SIGMA_EXEC` makes the shader highly portable and removes any dependency on driver-specific recursion limits. It guarantees that even complex, multi-stage operations can execute reliably.
-   **Single Workgroup**: The shader is designed for a `1x1x1` workgroup. This simplifies the design as no synchronization is needed, but it means the C host must dispatch one compute call per sample. For generating entire waveforms at once, a different shader structure (e.g., one invocation per sample in a 1D workgroup) would be needed.

## 6. Feature Parity Analysis (C VM vs. GLSL Shader)

As of the latest updates, the GLSL compute shader VM **fully implements all features and opcodes** present in the C-based VM defined in `px_vm.h`. The shader is now considered feature-complete.

The following table provides a detailed breakdown of the language features and their implementation status in the GLSL shader.

| Feature Group | Feature / Operation | GLSL Shader Status | Notes |
| :--- | :--- | :--- | :--- |
| **Variables** | `x`, `FREQUENCY`, `MOD_A`, `MOD_B`, `MOD_C`, `RAND_OFFSET` | Fully Implemented | Provided via `VmParams` uniform and pushed with `OP_PUSH_VAR_*` opcodes. |
| | Loop Variable (`k`, etc.) | Fully Implemented | Value stored in `loop_var_value` global and pushed with `OP_PUSH_LOOP_VAR`. |
| **Constants** | `PI`, `E`, `TWO_PI`, etc. | Fully Implemented | Handled by compiler; baked into the constants buffer and pushed with `OP_PUSH_CONST`. |
| | `LFSR_*` Type Constants | Fully Implemented | Handled by compiler; pushed as float constants via `OP_PUSH_CONST`. |
| **Operators** | Arithmetic (`+`, `-`, `*`, `/`, `%`) | Fully Implemented | Direct mapping to `OP_ADD`, `OP_SUB`, `OP_MUL`, `OP_DIV`, `OP_MOD`. |
| | Unary (`-`, `!`) | Fully Implemented | Direct mapping to `OP_NEGATE` and `OP_NOT`. Unary `+` is a no-op. |
| | Comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`) | Fully Implemented | Direct mapping to `OP_CMP_*` opcodes. |
| | Logical (`&&`, `||`, `^`) | Fully Implemented | The C compiler generates `OP_JUMP_IF_FALSE` and `OP_JUMP` sequences, which the shader executes. No dedicated logical opcodes are needed. |
| | Ternary (`? :`) | Fully Implemented | The C compiler generates `OP_JUMP_IF_FALSE` and `OP_JUMP` sequences, which the shader executes. |
| **Functions** | Standard Math (`sin`...`pow`) | Fully Implemented | All functions from C VM are mapped to GLSL built-ins via `OP_CALL`. |
| | `rand()` | Fully Implemented | Implemented via `OP_CALL` using a `pseudo_rand()` helper function. |
| | `sigma()` | Fully Implemented | Implemented via `OP_SIGMA_EXEC` using a non-recursive, multi-phase state machine. |
| **LFSR System**| Precomputed Table Mode | Fully Implemented | `lfsr_val`, `lfsr_noise`, `lfsr_clock` read from the LFSR table SSBO by default. |
| | Free-Running Mode | Fully Implemented | Supported via a read-write SSBO at binding 4 and controlled by `VmParams` uniforms. |

## 7. Integration Steps for C Host

1.  **Load and Compile Shader**: Load `px_vm.comp` and compile it into a compute shader program.
2.  **Prepare Buffers**:
    -   For each `WaveDefinition` that needs to be run on the GPU:
        -   Compile it to a `BytecodeChunk`.
        -   "Flatten" the `BytecodeChunk` and its sub-chunks into the single Bytecode Buffer.
        -   Gather all constants into the Constants Buffer.
    -   Create the global LFSR Tables Buffer once during initialization.
3.  **Create SSBOs**: Create and populate the four required SSBOs on the GPU.
4.  **Set Uniforms**: Before dispatching, update the `VmParams` and `VmMetadata` uniform blocks with the data corresponding to the specific bytecode and sample being generated.
5.  **Dispatch**: Execute `glDispatchCompute(1, 1, 1)`.
6.  **Retrieve Result**: Use a memory barrier (`glMemoryBarrier`) and then read back the data from the Output Buffer.

## 8. Execution Environment Comparison: C VM vs. GLSL Shader

While both the C-based VM (in `px_vm.h`) and the GLSL compute shader VM (`px_vm.comp`) are designed to achieve feature parity, their underlying architectures and execution models differ significantly due to the constraints and strengths of their respective environments (CPU vs. GPU).

This section provides a head-to-head comparison of their core design aspects.

### 8.1. Core Architecture and Execution Loop

-   **C VM (CPU):**
    -   **Architecture**: A classic stack-based interpreter implemented in C. The entire state of the VM (instruction pointer, stack, etc.) is encapsulated within a `VM` struct.
    -   **Execution Loop**: It uses a standard `while` or `for` loop that fetches an opcode, decodes it, and dispatches to the correct logic. For performance, this is often implemented using a "computed goto" or a large `switch` statement, which is highly efficient on modern CPUs.

-   **GLSL VM (GPU):**
    -   **Architecture**: A single-invocation compute shader that acts as a self-contained VM. State is managed through `global` shader variables (e.g., `ip`, `stack_top`, `vm_stack`), as each shader invocation is an independent execution context.
    -   **Execution Loop**: The main loop is a `for` loop with a fixed, high iteration limit (e.g., 2048) to prevent accidental infinite loops from hanging the GPU. A `switch` statement within the loop decodes and executes each opcode. This is less flexible than a CPU loop but is the standard, safe approach for GPU compute kernels.

### 8.2. `sigma()` (Summation) Implementation

This is the most significant architectural difference between the two VMs.

-   **C VM (CPU):**
    -   **Method**: Uses **recursion**. The `OP_SIGMA_EXEC` handler calls a helper function (`execute_sub_chunk`) which, in turn, calls the main `execute_bytecode` function for each of the `start`, `end`, `step`, and `body` expressions. This is a clean, natural, and efficient implementation that leverages the CPU's native call stack.

-   **GLSL VM (GPU):**
    -   **Method**: Uses a **non-recursive, explicit state machine**. GLSL recursion is not reliably supported across all drivers and hardware, making it unsuitable for a portable library. The shader instead uses a set of global variables (`sigma_is_active`, `sigma_phase`) to track the progress of the `sigma` operation. The `OP_HALT` instruction is repurposed as a state transition trigger. When an `OP_HALT` is encountered while `sigma_is_active` is true, the state machine advances to the next phase (e.g., from evaluating `start` to evaluating `end`), sets the `ip` to the next sub-chunk, and continues the main VM loop. This approach is more complex to implement but guarantees execution portability.

### 8.3. State and Data Management

-   **C VM (CPU):**
    -   **State**: All state is contained within the `VM` struct instance, making it relocatable in memory and allowing for multiple, independent VM instances to coexist (though not a current requirement).
    -   **Data Access**: Accesses bytecode, constants, and strings directly via pointers within the `BytecodeChunk` struct, which resides in standard system RAM. This provides low-latency, direct memory access.

-   **GLSL VM (GPU):**
    -   **State**: State is static and global to the shader's execution context. A manually managed `call_stack` array is used to store the return `ip` for the `sigma` state machine.
    -   **Data Access**: All data is provided by the C host via specialized GPU memory buffers:
        -   **SSBOs**: Used for bulk data like the flattened bytecode array, the constants pool, and LFSR tables. Accessing individual bytes from the `uint`-packed bytecode SSBO requires manual bit-shifting and masking within the shader.
        -   **Uniforms**: Used for smaller, per-sample data like the `VmParams` and `VmMetadata` structs.
    -   **LFSR State**: The state for the "free-running" LFSR is persisted across shader invocations using a read-write SSBO at `binding = 4`, which the shader reads from and writes back to in a single execution.
