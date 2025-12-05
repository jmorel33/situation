# Decoupled Rendering Analysis: The "Airgap" Architecture

## 1. Executive Summary

**Verdict:** Feasible, but requires significant architectural refactoring for the OpenGL backend.

Moving rendering to a dedicated thread ("Decoupled Rendering") is the standard for high-performance engines. It unblocks the Main Thread (Game Logic) from VSync and driver stalls.

*   **Vulkan Feasibility:** **High.** Vulkan is designed for threading. The main challenge is synchronizing the Swapchain Acquire/Present cycle and managing command pool ownership.
*   **OpenGL Feasibility:** **Medium.** OpenGL contexts are thread-bound. The context must be moved to the render thread. The immediate-mode nature of the current `Situation` OpenGL backend requires a new intermediate "Command List" abstraction to record commands on Main and replay them on Render.

## 2. Architectural Gap Analysis

### 2.1. Current State (Coupled)
Currently, `Situation` executes a linear, single-threaded frame:
```c
while (!Close) {
    PollInput();      // Main Thread
    Update();         // Main Thread
    AcquireFrame();   // Main Thread (Blocks on Fence/Swapchain)
    RecordCommands(); // Main Thread (Expensive driver validation)
    EndFrame();       // Main Thread (Blocks on Present/VSync)
}
```
This means if VSync is on (60 FPS), the *entire* game logic is capped at 60Hz. If a frame takes 17ms, the game slows down.

### 2.2. The Target State (Decoupled)
We aim for two independent timelines:

**Main Thread (Logic - "Producer"):**
Runs as fast as possible (e.g., 144Hz+).
1.  Polls Input.
2.  Runs Physics/Game Logic.
3.  *Generates* a lightweight list of render commands (The "Draw Packet").
4.  Pushes the packet to a thread-safe queue.

**Render Thread (GPU - "Consumer"):**
Runs at VSync (e.g., 60Hz).
1.  Pops the latest complete Draw Packet.
2.  Acquires Swapchain Image.
3.  Translates Packet -> GL/VK API Calls.
4.  Presents.

### 2.3. The "Airgap" Problem
The `Situation` API exposes direct control to the user in the render loop:
```c
SituationCmdDrawMesh(cmd, mesh); // In GL, this calls glDrawElements immediately!
```
To decouple this, `SituationCmdDrawMesh` **cannot** call `glDrawElements` on the Main Thread anymore, because the Main Thread won't have the GL Context.

## 3. The Titanium Solution: "Phantom Command Buffers"

The solution is to unify the backends under a strict Command Buffer model. In Vulkan, we already have `VkCommandBuffer`. For OpenGL, we must invent one.

### 3.1. The Phantom Buffer (Soft Command Buffer)
We introduce a ring-buffer based opcode system for OpenGL.

**Struct Definition (Internal):**
```c
typedef enum {
    SIT_OP_DRAW_MESH,
    SIT_OP_BIND_PIPELINE,
    SIT_OP_SET_SCISSOR,
    // ...
} SitOpCode;

typedef struct {
    SitOpCode op;
    union {
        struct { uint64_t mesh_id; } draw;
        struct { float x, y, w, h; } viewport;
        // ...
    } args;
} SitCommandPacket;

typedef struct {
    SitCommandPacket* commands;
    size_t count;
    size_t capacity;
} SituationSoftCommandBuffer;
```

**New Workflow (OpenGL):**
1.  `SituationAcquireFrameCommandBuffer()` (Main Thread) returns a pointer to a `SituationSoftCommandBuffer`.
2.  `SituationCmdDrawMesh()` writes a `SIT_OP_DRAW_MESH` packet into the buffer. **No GL calls are made.**
3.  `SituationEndFrame()` pushes this buffer to the Render Thread.
4.  **Render Thread:** Reads the buffer and executes real `glDrawElements`, `glUseProgram`, etc.

### 3.2. Data Ownership & The "Double Buffer" Trap
If the Main Thread modifies a `SituationMesh` or `UniformBuffer` while the Render Thread is drawing it, we get tearing or crashes.

**The Rules of the Airgap:**
1.  **Immutable Resources:** Meshes and Textures are immutable once uploaded. To change them, you destroy and recreate (or use specific thread-safe update APIs).
2.  **Transient Data (Uniforms):** `SituationCmdSetPushConstant` copies data *by value* into the command packet. This is safe.
3.  **Dynamic Buffers (UBOs):** If the user maps a pointer and writes to it, we have a race.
    *   *Solution:* We must enforce `SituationUpdateBuffer`. This function will now allocate from a "Frame Linear Allocator" (staging memory) and encode a copy operation. The staging memory is kept alive until the Render Thread finishes the frame.

## 4. Implementation Roadmap

### Phase 1: The Soft Command Buffer (Refactor OpenGL)
*   **Goal:** Make the OpenGL backend "deferred" like Vulkan.
*   **Step 1:** Create the `SituationSoftCommandBuffer` struct.
*   **Step 2:** Rewrite all `SituationCmd*` functions to write opcodes if the backend is OpenGL.
*   **Step 3:** Create a `_SituationGLExecuteCommands(SituationSoftCommandBuffer* buf)` function that contains the switch-case interpreter.
*   **Verification:** Run this on the Main Thread first. Behavior should be identical, just buffered.

### Phase 2: Thread Infrastructure
*   **Goal:** Spin up the Render Thread.
*   **Step 1:** Move `glfwMakeContextCurrent` to the new thread (OpenGL).
*   **Step 2:** Move `vkQueueSubmit` / `vkQueuePresent` to the new thread (Vulkan).
*   **Step 3:** Implement a thread-safe `FrameQueue` (Size 2 or 3). Main pushes, Render pops.

### Phase 3: Synchronization & Latency
*   **Goal:** Prevent "runaway" Main Thread.
*   **Mechanism:** If the `FrameQueue` is full (Render Thread is stuck on VSync), the Main Thread must block in `SituationAcquireFrame`.
*   **Result:** We get "Triple Buffering" behavior for free. Smooth framerates, no tearing.

## 5. Pseudo-Code Reference

### The New `SituationCmdDrawMesh`
```c
SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh) {
    if (sit_render.renderer_type == SIT_RENDERER_VULKAN) {
        // Existing Vulkan Logic
        vkCmdDrawIndexed((VkCommandBuffer)cmd, ...);
    } else {
        // New OpenGL "Phantom" Logic
        SituationSoftCommandBuffer* soft = (SituationSoftCommandBuffer*)cmd;
        SitCommandPacket* pkt = _SitSoftCmdPush(soft, SIT_OP_DRAW_MESH);
        pkt->args.draw.mesh_id = mesh.id;
    }
    return SITUATION_SUCCESS;
}
```

### The Render Thread Loop
```c
void _SituationRenderThreadEntry(void* arg) {
    _SituationMakeContextCurrent(); // GL Only

    while (!shutdown) {
        FrameData* frame = _SitFrameQueuePop(); // Blocks if empty

        if (backend == VULKAN) {
             vkQueueSubmit(..., frame->vk_cmd_buffer);
             vkQueuePresent(...);
        } else {
             _SituationGLExecuteCommands(frame->soft_cmd_buffer);
             glfwSwapBuffers();
        }

        _SitReturnFrameToPool(frame);
    }
}
```

## 6. Conclusion
This is a "Titanium" upgrade. It hardens the API boundary, enforces correct data flow, and unlocks maximum performance. Phase 1 (Soft Command Buffer) is the critical dependency; once that is built, the actual threading is trivial.
