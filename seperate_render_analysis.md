# Decoupled Rendering Analysis: The "Airgap" Architecture

## 1. Executive Summary

**Verdict:** **Phase 1 and Phase 2 are 100% COMPLETE for both OpenGL and Vulkan.**

Moving rendering to a dedicated thread ("Decoupled Rendering") is the standard for high-performance engines. It unblocks the Main Thread (Game Logic) from VSync and driver stalls.

*   **Vulkan Feasibility:** **High.** Vulkan is naturally threaded. `Situation` now implements a robust threaded submission model where the Main Thread records command buffers, and the Render Thread handles queue submission and presentation.
*   **OpenGL Feasibility:** **Medium.** OpenGL contexts are thread-bound. The context has been successfully moved to the render thread. The immediate-mode nature of `Situation`'s OpenGL backend has been fully abstracted using the "Phantom Command Buffer" (Soft Command Buffer), allowing recording on Main and replay on Render.

## 2. Architectural Gap Analysis

### 2.1. Current State (Coupled)
Historically, `Situation` executed a linear, single-threaded frame:
```c
while (!Close) {
    PollInput();      // Main Thread
    Update();         // Main Thread
    AcquireFrame();   // Main Thread (Blocks on Fence/Swapchain)
    RecordCommands(); // Main Thread (Expensive driver validation)
    EndFrame();       // Main Thread (Blocks on Present/VSync)
}
```
This capped game logic at the VSync rate (e.g., 60Hz).

### 2.2. The Target State (Decoupled)
We have achieved two independent timelines:

**Main Thread (Logic - "Producer"):**
Runs as fast as possible (e.g., 144Hz+).
1.  Polls Input.
2.  Runs Physics/Game Logic.
3.  *Generates* a lightweight list of render commands (The "Draw Packet" / Recorded Command Buffer).
4.  Pushes the frame index to a thread-safe queue.

**Render Thread (GPU - "Consumer"):**
Runs at VSync (e.g., 60Hz).
1.  Pops the latest complete Frame Index.
2.  **Vulkan:** Submits the pre-recorded `VkCommandBuffer` to the queue and presents.
3.  **OpenGL:** Replays the `SituationSoftCommandBuffer` and swaps buffers.

### 2.3. The "Airgap" Problem
To decouple this, `SituationCmdDrawMesh` **cannot** call `glDrawElements` on the Main Thread anymore, because the Main Thread does not have the GL Context.

## 3. The Titanium Solution: "Phantom Command Buffers"

The solution unifies the backends under a strict Command Buffer model.

### 3.1. The Phantom Buffer (Soft Command Buffer)
We introduced a ring-buffer based opcode system for OpenGL.

**Struct Definition (Internal):**
```c
typedef enum {
    SIT_OP_DRAW_MESH,
    SIT_OP_BIND_PIPELINE,
    SIT_OP_SET_SCISSOR,
    SIT_OP_UPDATE_BUFFER, // [New]
    // ...
} SitOpCode;

typedef struct {
    SitOpCode op;
    union {
        struct { uint64_t mesh_id; } draw;
        struct { float x, y, w, h; } viewport;
        struct { uint64_t buffer_id; size_t offset; size_t size; size_t data_offset; } update_buffer;
        // ...
    } args;
} SitCommandPacket;
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
    *   *Resource Creation:* Creating resources (Textures, Meshes) requires a GL Context.
        *   *Option A (Shared Context):* Main Thread keeps a shared context for uploads.
        *   *Option B (Deferral):* All creation is deferred. `SituationCreateTexture` returns a handle, but upload happens on Render Thread. (Preferred for simplicity).
2.  **Transient Data (Uniforms):** `SituationCmdSetPushConstant` copies data *by value* into the command packet. This is safe.
3.  **Dynamic Buffers (UBOs):** If the user maps a pointer and writes to it, we have a race.
    *   *Solution:* We must enforce `SituationUpdateBuffer`. This function will now allocate from a "Frame Linear Allocator" (staging memory) in the soft buffer and encode a copy operation. The staging memory is kept alive until the Render Thread finishes the frame.

## 4. Implementation Roadmap

### Phase 1: The Soft Command Buffer (Refactor OpenGL) [COMPLETE]
*   **Goal:** Make the OpenGL backend "deferred" like Vulkan.
*   **Step 1:** [Done] Create the `SituationSoftCommandBuffer` struct.
*   **Step 2:** [Done] Rewrite all `SituationCmd*` functions to write opcodes if the backend is OpenGL.
    *   *Status:* All render commands, including `SituationUpdateBuffer` (Critical for UBOs) and `SituationCmdSetVertexAttribute`, now utilize the soft command buffer.
*   **Step 3:** [Done] Create a `_SituationGLExecuteCommands(SituationSoftCommandBuffer* buf)` function that contains the switch-case interpreter.
*   **Verification:** Run this on the Main Thread first. Behavior should be identical, just buffered.

### Phase 2: Thread Infrastructure [COMPLETE]
*   **Goal:** Spin up the Render Thread and establish the Producer/Consumer loop.
*   **Step 1:** [Done] Implement `_SituationRenderThreadEntry` loop.
    *   Handles context acquisition (`glfwMakeContextCurrent`).
    *   Implements a condition-variable based wait loop.
*   **Step 2:** [Done] Implement `FrameQueue`.
    *   Main Thread pushes `FrameData` (containing `SoftCommandBuffer` or `VkCommandBuffer` index).
    *   Render Thread pops `FrameData`.
*   **Step 3:** [Done] Update `SituationInit` to spawn the thread and create a shared "Loader Context" for the Main Thread.
*   **Step 4:** [Done] Update `SituationEndFrame` to push to queue and `SituationAcquireFrameCommandBuffer` to wait for free slots (Backpressure).
*   **Vulkan Support:** The Render Thread logic explicitly handles Vulkan by submitting the recorded `VkCommandBuffer` to the queue and presenting the swapchain. The `_SituationRenderThreadEntry` function contains dedicated paths for both backends.
*   **Constraint:** Currently uses a **Shared Global VAO** (`mesh_vao_id`) for all meshes. This works perfectly but requires re-binding VBOs on every draw call (`glVertexArrayVertexBuffer`), which has a small CPU overhead compared to baking VAOs.

### Phase 2.5: High-Performance Mesh Architecture (Lazy VAO Cache) [PLANNED]
*   **Context:** VAO objects (Vertex Array Objects) are **not shared** between OpenGL contexts.
    *   *Problem:* We create meshes on the Main Thread (Loader Context), but we draw them on the Render Thread (Main Context). We cannot create a VAO on Main and use it on Render.
    *   *Current Solution (Phase 2):* We use a single global VAO on the Render Thread and bind the shared VBOs dynamically. This works but isn't optimal.
*   **Goal:** Restore per-mesh VAOs for maximum performance without threading violations.
*   **Strategy: Lazy Initialization on Render Thread.**
    1.  **Main Thread:** Creates VBO/EBO (Shared Resources). Assigns a unique ID.
    2.  **Render Thread:** Maintains a `HashTable<MeshID, VaoID>`.
    3.  **On Draw (Render Thread):**
        *   Look up MeshID in the map.
        *   **If Missing:** Create a new VAO *right now*, configure attributes (using data from the command packet), and store it in the map.
        *   **If Present:** Bind the cached VAO.
    4.  **On Destroy:** Main Thread pushes a `SIT_OP_DESTROY_CACHED_VAO` command to the Render Thread to clean up the map entry.

### Phase 3: Synchronization & Latency [PENDING]
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
             // Wait for semaphores, then submit
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
