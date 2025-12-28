# Terminal Upgrade Analysis: The Path to v2.0 (Museum-Grade Compliance)

## Executive Summary
This document outlines the architectural upgrade from the current v1.3 "Modern Commercial" terminal to v2.0 "Hardware Accurate" Tektronix/ReGIS vector terminal. The core innovation is the shift from CPU-side rasterization to a **GPU-Driven Vector Engine**, utilizing Compute Shaders to emulate the specific phosphor persistence and glow of Tektronix storage tubes.

## Current Architecture (v1.3)
*   **State Machine:** `VTParseState` enum handles ANSI/VT52/Sixel.
*   **Rendering:** Single Compute Shader (`TERMINAL_COMPUTE_SHADER_SRC`) reading from a cell grid SSBO (`TerminalBuffer`) and outputting to `output_texture`.
*   **Storage:** `EnhancedTermChar` 2D array on CPU, synced to GPU SSBO.

## Phase 1: The GPU Vector Engine (Tektronix 4010/4014)

### 1. New Data Structures (`sit/terminal/terminal.h`)

We need a struct to represent the vector primitives on the GPU.

```c
// Add to sit/terminal/terminal.h
typedef struct {
    float x0, y0; // Normalized Device Coordinates (0.0 - 1.0)
    float x1, y1; // Normalized Device Coordinates
    uint32_t color; // Packed RGBA
    float intensity; // 1.0 = fresh beam, < 1.0 = decaying
    float padding[2]; // Align to 16 bytes for std430
} GPUVectorLine;
```

**Actionable:**
*   Insert `typedef struct GPUVectorLine ...` in `sit/terminal/terminal.h` before `TerminalSession`.
*   Update `Terminal` struct to include:
    *   `SituationBuffer vector_buffer;` (SSBO)
    *   `SituationTexture vector_layer_texture;` (Storage Image, likely RGBA16F for HDR glow)
    *   `SituationComputePipeline vector_pipeline;`
    *   `uint32_t vector_count;`

### 2. Compute Shader Infrastructure

We introduce a new rendering pass *before* the main terminal text pass.

**New Shader: `VECTOR_RASTERIZER_SRC`**
This shader behaves like a software rasterizer running on the GPU. It iterates over the active lines in the `VectorPrimitiveBuffer` and draws them into the `vector_layer_texture`.

**Algorithm:**
*   Each workgroup handles a subset of lines or the dispatch is per-line (e.g., `Dispatch(vector_count, 1, 1)`).
*   Use standard line drawing logic (SDF or Xiaolin Wu) to write pixels to the image.
*   **Additive Blending:** Since we are simulating a CRT beam, writes should accumulate. *Note:* Compute shaders don't support standard blending on `imageStore`. We might need to use atomic operations or a specific blend mode if supported, or simply `imageLoad` -> `add` -> `imageStore` (read-modify-write). Given the persistence, a read-modify-write is acceptable for this pre-pass.

**Actionable:**
*   Define `VECTOR_RASTERIZER_SRC` macro in `sit/terminal/terminal.h`.
*   In `InitTerminalCompute()`:
    *   Create `vector_buffer` (size ~1MB for 10k+ lines).
    *   Create `vector_layer_texture` (RGBA16F preferred).
    *   Compile `vector_pipeline` using `VECTOR_RASTERIZER_SRC`.

### 3. CPU Parser Upgrade

We need to intercept the Tektronix mode switch sequence.

**Trigger:** `ESC [ ? 38 h` (Enter Tektronix Mode)

**Actionable:**
*   **Enum Update:** Add `PARSE_TEKTRONIX` to `VTParseState` in `sit/terminal/terminal.h`.
*   **Mode Handling:** In `ExecuteSM(bool private_mode)`, add:
    ```c
    case 38: // Enter Tektronix Mode
        ACTIVE_SESSION.parse_state = PARSE_TEKTRONIX;
        // Optionally clear vector buffer or persist based on Tektronix logic
        break;
    ```
*   **Parser Dispatch:** In `ProcessChar(unsigned char ch)`, add:
    ```c
    case PARSE_TEKTRONIX: ProcessTektronixChar(ch); break;
    ```
*   **Implementation:** Implement `void ProcessTektronixChar(unsigned char ch)`:
    *   Maintain a state machine for Tektronix 4010 address bytes (Hi-Y, Lo-Y, Hi-X, Lo-X).
    *   On completing a coordinate pair, push a new `GPUVectorLine` to `terminal.vector_buffer` (via a staging buffer or direct map if unified memory).

### 4. Compositing (The "Glow")

The final step is merging the vector layer with the text layer.

**Actionable:**
*   Update `TERMINAL_COMPUTE_SHADER_SRC`:
    *   Add `layout(binding = 4) uniform sampler2D vector_layer_texture;`.
    *   In `main()`, sample this texture:
        ```glsl
        vec4 vector_color = texture(vector_layer_texture, uv_screen);
        // Additive mix with simulated phosphor decay
        pixel_color += vector_color * vector_color.a * 1.5;
        ```

## Phase 2: ReGIS Support (The "CAD" Milestone)
*   **Concept:** ReGIS is just a higher-level command set (Circles, Arcs) that decomposes into the same `GPUVectorLine` primitives.
*   **Actionable:** Implement `ProcessReGISChar` to parse `P[x,y]`, `V[dx,dy]`, `C[r]` commands, tessellate them into line segments, and push to the existing `vector_buffer`.

## Phase 3: ISO 2022 & NRCS (The "Linguist" Milestone)
*   **Refactor:** `TranslateCharacter` currently has basic switch-cases. It needs a robust lookup table approach to handle National Replacement Character Sets (swapping `#` for `£`, etc.) dynamically without spaghetti code.

## Phase 4: Scrollback & Session Management
*   **Optimization:** Implement a "Viewport Offset" uniform. Instead of re-uploading the whole screen grid when scrolling, we upload a larger ring buffer and just shift the `view_offset` in the shader.

## Immediate Next Steps (Checklist)

1.  [ ] **Structs:** Add `GPUVectorLine` to `terminal.h`.
2.  [ ] **State:** Add `PARSE_TEKTRONIX` to `VTParseState`.
3.  [ ] **Init:** Update `InitTerminalCompute` to allocate `vector_buffer` and `vector_layer_texture`.
4.  [ ] **Shader:** Write and embed `VECTOR_RASTERIZER_SRC`.
5.  [ ] **Parser:** Implement `ProcessTektronixChar` skeleton.
