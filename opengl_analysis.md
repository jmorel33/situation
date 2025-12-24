# OpenGL Analysis and Action Plan

This is a strategic architectural pivot. Instead of maintaining OpenGL as a "compatibility wrapper," we will upgrade it to a high-performance parallel backend. By enforcing OpenGL 4.6 Core, we gain access to memory models that rival Vulkan in efficiency while maintaining the simpler development iteration loop of GL.

Here is the concrete execution plan to "Max Out Core" (OpenGL 4.6).

## Phase 1: The "Zero-Copy" Data Highway (Persistent Mapping)
**Goal:** Eliminate glBufferSubData and driver synchronization stalls. Implement the exact same "Staging Ring Buffer" pattern used in your Vulkan backend.

### Technical Implementation:
*   **The Ring Buffer:** Create a large (e.g., 64MB) buffer using glBufferStorage.
*   **Flags:** GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT.
*   **Result:** You get a void* pointer. You can write to this pointer from the CPU while the GPU is drawing, provided you handle synchronization indices.
*   **Synchronization:** Use glFenceSync.
    *   Divide the ring buffer into 3 segments (Triple Buffering).
    *   Before writing to Segment A, check the Fence for Segment A (ensure GPU is done reading).
    *   Write data (memcpy).
    *   Issue draw commands reading from Segment A.
    *   Place a Fence at the end of the frame.

### Code Change Requirements:
*   [ ] Modify `_SituationInitOpenGL`: Allocate the global persistent staging buffer.
*   [ ] Modify `SituationUpdateBuffer`:
    *   [ ] Check if the update fits in the current ring segment.
    *   [ ] memcpy directly to the persistent pointer.
    *   [ ] No OpenGL API calls are made here. It is pure memory copy.
*   [ ] Modify `SituationCreateBuffer`: Use glCreateBuffers + glNamedBufferStorage (DSA) with GL_DYNAMIC_STORAGE_BIT only if it cannot use the ring buffer strategy (e.g., static geometry).

## Phase 2: The "Stateless" Renovation (Direct State Access)
**Goal:** Remove the "Soft Command Buffer" overhead of tracking bindings (current_vao, current_program). Make the replay loop purely functional.

### Technical Implementation:
DSA allows us to modify objects without binding them to the context. This eliminates "state leakage" bugs where one part of the engine accidentally unbinds a resource needed by another.

### Action Items:
*   **Texture Creation:** Replace glBindTexture + glTexImage2D with:
    *   [x] glCreateTextures(GL_TEXTURE_2D, ...)
    *   [x] glTextureStorage2D(...) (Immutable storage is faster/safer)
    *   [x] glTextureSubImage2D(...)
*   **Buffer Operations:** Replace glBindBuffer + glBufferData with:
    *   [x] glCreateBuffers(...)
    *   [x] glNamedBufferStorage(...)
*   **Uniforms:** Replace glUseProgram + glUniform* with:
    *   [x] glProgramUniform* (Updates uniforms without switching the active shader).

**Benefit:** The SituationCmd* functions become thread-safe regarding GL state generation (though submission is still main-thread).

## Phase 3: The "Bindless" Revolution
**Goal:** Eliminate SituationCmdBindTexture. Treat textures as 64-bit integers (handles), exactly like Vulkan descriptor indexing.

### Technical Implementation:
*   **Resident Handles:**
    *   In SituationCreateTexture, call glGetTextureHandleARB.
    *   Call glMakeTextureHandleResidentARB.
    *   Store this uint64_t in the SituationTexture struct.
*   **Shader Contract Update:**
    *   Modify your internal shaders to accept layout(bindless_sampler) uniform sampler2D via UBOs or Push Constants.
*   **API Change:**
    *   SituationCmdBindTexture becomes a no-op in terms of GL state. It simply writes the 64-bit handle into the Push Constant / UBO block for the next draw call.

**Result:** No more glActiveTexture or glBindTextureUnit limits. You can access thousands of textures in a single draw call.

## Phase 4: The "GPU-Driven" Future (Multi-Draw Indirect)
**Goal:** Collapse thousands of SituationCmdDraw calls into a single driver invocation.

### Technical Implementation:
*   **The MDI Buffer:** Create a GL_DRAW_INDIRECT_BUFFER.
*   **Batching Logic:**
    *   In your Soft Command Buffer replay loop (_SituationGLExecuteCommands), detect consecutive draw commands that share the same Pipeline (Shader + State).
    *   Instead of calling glDrawElements immediately, write the draw parameters (count, instanceCount, firstIndex, baseVertex, baseInstance) into the mapped Indirect Buffer.
*   **Execution:**
    *   When the pipeline changes (or the batch is full), issue one glMultiDrawElementsIndirect.

## Execution Plan & Timeline

### Step 1: The DSA Migration (Low Risk, High Cleanup)
*   [x] **Task:** Refactor SituationCreateTexture, SituationCreateBuffer, and SituationLoadShader to use glCreate* and glNamed* functions exclusively.
*   [x] **Verification:** Ensure no glBind* calls exist in asset creation logic.

### Step 2: Persistent Ring Buffer (High Impact)
*   [ ] **Task:** Implement _SituationGLInitRingBuffer.
*   [ ] **Task:** Rewrite SituationUpdateBuffer to use memcpy into the ring.
*   [ ] **Task:** Rewrite SituationCmdDraw to bind the ring buffer offset.
*   **Result:** SituationUpdateBuffer becomes nearly instant.

### Step 3: Bindless Textures (Modernization)
*   [ ] **Task:** Enable GL_ARB_bindless_texture.
*   [ ] **Task:** Update SituationTexture to hold the GLuint64 handle.
*   [ ] **Task:** Update SituationCmdDrawQuad to pass the handle via uniform instead of binding.

### Step 4: Multi-Draw Indirect (Optimization)
*   [ ] **Task:** Add an MDI optimizer to the Soft Command Buffer replay.

## Code Snippet: The New OpenGL "Zero-Copy" Update
This replaces the slow glBufferSubData logic.

```c
// In _SituationInitOpenGL
void _SituationInitGLRingBuffer() {
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glCreateBuffers(1, &sit_render.gl.ring_buffer_id);
    glNamedBufferStorage(sit_render.gl.ring_buffer_id, RING_SIZE, NULL, flags);
    sit_render.gl.ring_data_ptr = glMapNamedBufferRange(sit_render.gl.ring_buffer_id, 0, RING_SIZE, flags);
}

// In SituationUpdateBuffer (OpenGL Path)
SITAPI SituationError SituationUpdateBuffer(...) {
    // 1. Calculate offset in ring (aligned to 256 bytes)
    size_t offset = atomic_fetch_add(&sit_render.gl.ring_head, size);

    // 2. Wait for fence if we wrapped around (Sync logic omitted for brevity)
    _SituationGLRingWait(offset);

    // 3. Memcpy directly to GPU visible memory (No driver overhead!)
    memcpy((uint8_t*)sit_render.gl.ring_data_ptr + offset, data, size);

    // 4. Record command to bind this specific range for the next draw
    // (This actually usually happens via binding the whole UBO and using offset in glBindBufferRange)
    return SITUATION_SUCCESS;
}
