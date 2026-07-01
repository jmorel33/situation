/***************************************************************************************************
*
*   situation_impl_renderer_core.h - Shared Renderer Infrastructure
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Cross-cutting renderer utilities: uniform map, staging, graveyard, GL state
*   shadow, ring buffers, program cache, GL error helpers.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_CORE_H
#define SITUATION_IMPL_RENDERER_CORE_H

/** Per-frame view/projection UBO (binding 0 in default 3D shaders). */
typedef struct ViewDataUBO {
    mat4 view;
    mat4 projection;
} ViewDataUBO;

#if defined(SITUATION_USE_VULKAN)
/** Pass as pipeline_flags to _SituationVulkanCreateGraphicsPipeline for opaque color (blend off). */
#define SIT_VK_PIPELINE_BLEND_OPAQUE   1u
/** VD compositor Path B blend variants (match OpenGL glBlendFunc in SIT_OP_RENDER_VIRTUAL_DISPLAYS). */
#define SIT_VK_PIPELINE_BLEND_ADDITIVE (1u << 1)
#define SIT_VK_PIPELINE_BLEND_MULTIPLY (1u << 2)
#define SIT_VK_PIPELINE_BLEND_SCREEN   (1u << 3)
/** 2D compositor draws: depth test/write off (matches GL VD compositing glDisable(GL_DEPTH_TEST)). */
#define SIT_VK_PIPELINE_NO_DEPTH       (1u << 4)
#endif

// ============================================================================
// OpenGL Ring Buffer & MDI Helpers (needed early by _SituationInitOpenGL)
// ============================================================================
#if defined(SITUATION_USE_OPENGL)
static SituationError _SituationInitGLRingBuffer(void) {
    if (sit_render.gl.ring_buffer_id != 0) return SITUATION_SUCCESS;

    sit_render.gl.ring_size = SITUATION_GL_RING_SIZE;
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glCreateBuffers(1, &sit_render.gl.ring_buffer_id);
    if (sit_render.gl.ring_buffer_id == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create persistent ring buffer object.");
    }
    glNamedBufferStorage(sit_render.gl.ring_buffer_id, sit_render.gl.ring_size, NULL, flags);
    SIT_CHECK_GL_ERROR();
    sit_render.gl.ring_data_ptr = glMapNamedBufferRange(sit_render.gl.ring_buffer_id, 0, sit_render.gl.ring_size, flags);
    if (!sit_render.gl.ring_data_ptr) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to map persistent ring buffer.");
    }

    atomic_init(&sit_render.gl.ring_head, 0);
    return SITUATION_SUCCESS;
}

static SituationError _SituationInitGLMDIBuffer(void) {
    if (sit_render.gl.mdi_buffer_id != 0) return SITUATION_SUCCESS;

    sit_render.gl.mdi_ring_size = 1024 * 1024 * SITUATION_MAX_FRAMES_IN_FLIGHT;
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glCreateBuffers(1, &sit_render.gl.mdi_buffer_id);
    if (sit_render.gl.mdi_buffer_id == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create MDI ring buffer object.");
    }
    glNamedBufferStorage(sit_render.gl.mdi_buffer_id, sit_render.gl.mdi_ring_size, NULL, flags);
    SIT_CHECK_GL_ERROR();
    sit_render.gl.mdi_data_ptr = glMapNamedBufferRange(sit_render.gl.mdi_buffer_id, 0, sit_render.gl.mdi_ring_size, flags);
    if (!sit_render.gl.mdi_data_ptr) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to map MDI ring buffer.");
    }

    atomic_init(&sit_render.gl.mdi_ring_head, 0);
    return SITUATION_SUCCESS;
}

static SituationError _SituationInitGLRingFences(void) {
    sit_render.gl.ring_fence_count = SITUATION_MAX_FRAMES_IN_FLIGHT;
    sit_render.gl.current_fence_index = 0;
    sit_render.gl.ring_fences = (GLsync*)SIT_CALLOC(sit_render.gl.ring_fence_count, sizeof(GLsync));
    if (!sit_render.gl.ring_fences) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate GL ring buffer fence array.");
    }
    for(size_t i=0; i<sit_render.gl.ring_fence_count; i++) {
        sit_render.gl.ring_fences[i] = 0;
    }
    return SITUATION_SUCCESS;
}

static void _SituationGLRingWait(void) {
    int prev_frame = (sit_render.current_frame_index + SITUATION_MAX_FRAMES_IN_FLIGHT - 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
    GLsync fence = sit_render.gl.ring_fences[prev_frame];

    if (fence) {
        glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
    }
}
#endif // SITUATION_USE_OPENGL

/**
 * @brief [INTERNAL] Creates and initializes a new uniform map.
 *
 * @details This helper function allocates memory for a new `_SituationUniformMap` struct and its internal hash table buckets. The map is initialized to be empty, ready to store key-value pairs (uniform name strings mapped to GLint locations).
 *          This map is used as a cache to avoid repeated, costly calls to `glGetUniformLocation` for the same uniform name within a shader program.
 *
 * @return A pointer to the newly created `_SituationUniformMap` struct on success.
 * @return NULL if memory allocation fails for the map struct itself or its bucket array.
 *
 * @note The caller is responsible for destroying the returned map using `_sit_uniform_map_destroy` when it is no longer needed to prevent memory leaks.
 * @warning This function is for internal library use only and is not part of the public API.
 *
 * @see _sit_uniform_map_destroy(), _sit_uniform_map_set(), _sit_uniform_map_get()
 */
static _SituationUniformMap* _sit_uniform_map_create() {
    // --- 1. Allocate Memory for the Map Struct ---
    _SituationUniformMap* map = (_SituationUniformMap*)SIT_CALLOC(1, sizeof(_SituationUniformMap));
    // Using SIT_CALLOC initializes map->count and map->capacity to 0, and map->buckets to NULL.

    // Check if allocation for the map struct itself was successful.
    if (!map) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_create: Failed to allocate map struct.");
        return NULL;
    }

    // --- 2. Initialize Map Properties ---
    map->capacity = SIT_UNIFORM_MAP_INITIAL_CAPACITY; // Set initial bucket count
    map->count = 0; // Start with an empty map

    // --- 3. Allocate Memory for the Bucket Array ---
    // Allocate an array of pointers to `_SituationUniformMapEntry`.
    // The size is `capacity * sizeof(_SituationUniformMapEntry*)`.
    map->buckets = (_SituationUniformMapEntry**)SIT_CALLOC(map->capacity, sizeof(_SituationUniformMapEntry*));
    // Using SIT_CALLOC initializes all bucket pointers to NULL.

    // Check if allocation for the bucket array was successful.
    if (!map->buckets) {
        // Allocation failed for the bucket array.
        // Free the previously allocated map struct to prevent a memory leak.
        SIT_FREE(map);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_create: Failed to allocate bucket array.");
        return NULL;
    }

    // --- 4. Success ---
    // Both allocations were successful. The map is initialized and ready for use.
    return map;
}

// --- Updated/Added Documentation Block for _sit_uniform_map_destroy ---
/**
 * @brief [INTERNAL] Destroys a uniform map and frees all associated memory.
 *
 * @details This helper function recursively frees all memory associated with a `_SituationUniformMap`, including:
 * 1.  All individual key strings (`entry->key`) stored within the map entries.
 * 2.  All `_SituationUniformMapEntry` structs themselves.
 * 3.  The internal bucket array (`map->buckets`).
 * 4.  The main `_SituationUniformMap` struct (`map`).
 *
 * It safely handles being called on a NULL pointer or an already destroyed map.
 *
 * @param map A pointer to the `_SituationUniformMap` struct to be destroyed. This pointer can be NULL.
 *
 * @note This function must be called exactly once for each map created by `_sit_uniform_map_create` to prevent memory leaks.
 * @warning This function is for internal library use only and is not part of the public API.
 * @warning After calling this function, the `map` pointer becomes invalid and must not be used again.
 *
 * @see _sit_uniform_map_create()
 */
static void _sit_uniform_map_destroy(_SituationUniformMap* map) {
    // --- 1. Input Validation ---
    // Check if the map pointer is NULL. If so, there's nothing to destroy.
    // This is a safe and common pattern for destroy/free functions.
    if (!map) {
        return;
    }

    // --- 2. Destroy All Entries in All Buckets ---
    // Iterate through each bucket in the hash table.
    for (int i = 0; i < map->capacity; ++i) {
        // Get the head of the linked list for the current bucket.
        _SituationUniformMapEntry* entry = map->buckets[i];

        // Traverse the linked list for this bucket.
        while (entry != NULL) {
            // Save the pointer to the next entry *before* freeing the current one.
            _SituationUniformMapEntry* next_entry = entry->next;

            // Free the key string. strdup/strndup requires SIT_FREE().
            // It's safe to call SIT_FREE(NULL).
            SIT_FREE(entry->key);
            entry->key = NULL; // Defensive nulling (optional)

            // Free the entry struct itself.
            SIT_FREE(entry);
            entry = NULL; // Defensive nulling (optional)

            // Move to the next entry in the list.
            entry = next_entry;
        }
        // After the loop, the entire linked list for bucket `i` is freed.
        map->buckets[i] = NULL; // Defensive nulling (optional)
    }
    // All entries in all buckets have been destroyed.

    // --- 3. Free the Bucket Array ---
    // Free the memory allocated for the array of bucket pointers.
    SIT_FREE(map->buckets);
    map->buckets = NULL; // Defensive nulling
    map->capacity = 0;   // Reset capacity
    map->count = 0;      // Reset count

    // --- 4. Free the Map Struct Itself ---
    // Finally, free the memory allocated for the main map struct.
    SIT_FREE(map);
    // Note: The `map` pointer itself is not set to NULL here because
    // the caller's copy of the pointer is not passed by reference.
    // It is the caller's responsibility to not use the pointer after this call.
}

/**
 * @brief [INTERNAL] Resizes the uniform map when the load factor exceeds a threshold.
 * @details Doubles the capacity of the hash map and rehashes all existing entries.
 *          This function is called by _sit_uniform_map_set when the load factor > 0.75.
 * @return `SITUATION_SUCCESS` or `SITUATION_ERROR_MEMORY_ALLOCATION` / `SITUATION_ERROR_INVALID_PARAM`.
 */
static SituationError _sit_uniform_map_resize(_SituationUniformMap* map) {
    if (!map) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_sit_uniform_map_resize: map is NULL.");
    }

    int new_capacity = map->capacity * 2;
    if (new_capacity <= map->capacity) {
        return SITUATION_SUCCESS;
    }

    _SituationUniformMapEntry** new_buckets = (_SituationUniformMapEntry**)SIT_CALLOC(new_capacity, sizeof(_SituationUniformMapEntry*));
    if (!new_buckets) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_resize: Failed to allocate new buckets.");
    }

    // Rehash all entries
    for (int i = 0; i < map->capacity; ++i) {
        _SituationUniformMapEntry* entry = map->buckets[i];
        while (entry != NULL) {
            _SituationUniformMapEntry* next = entry->next;

            // Recalculate hash for new capacity
            unsigned long hash = _sit_hash_string(entry->key);
            int index = hash % new_capacity;

            // Insert into new bucket list (prepend)
            entry->next = new_buckets[index];
            new_buckets[index] = entry;

            entry = next;
        }
    }

    // Free old buckets array
    SIT_FREE(map->buckets);

    // Update map properties
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    return SITUATION_SUCCESS;
}

// --- Updated/Added Documentation Block for _sit_uniform_map_set ---
/**
 * @brief [INTERNAL] Adds or updates a key-value pair in the uniform map.
 *
 * @details This helper function inserts a new key-value pair (`key` -> `value`) into the hash map, or updates the value if the key already exists. It uses a simple chaining hash table structure.
 * 1.  Calculates the hash of the `key`.
 * 2.  Determines the bucket index using `hash % capacity`.
 * 3.  Searches the linked list at that bucket for the key.
 * 4.  If found, updates the existing entry's value.
 * 5.  If not found, allocates a new entry, duplicates the key string, sets the value, and prepends the new entry to the bucket's linked list.
 *
 * @param map A pointer to the `_SituationUniformMap` struct to modify.
 * @param key A null-terminated C string representing the uniform name (the key).
 * @param value The GLint value (typically the uniform location) to associate with the key.
 *
 * @return `SITUATION_SUCCESS` or `SITUATION_ERROR_INVALID_PARAM` / `SITUATION_ERROR_MEMORY_ALLOCATION`.
 */
static SituationError _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value) {
    if (!map || !key) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_sit_uniform_map_set: map or key is NULL.");
    }
    // Compute the hash value of the key string.
    unsigned long hash = _sit_hash_string(key);
    // Determine the index of the bucket where this key/value pair should reside.
    // Use the modulo operator to fit the hash within the bucket array size.
    int index = hash % map->capacity;
    // Ensure index is within valid bounds (should be guaranteed by modulo, but good practice).
    // if (index < 0 || index >= map->capacity) { /* Handle error */ return; } // Redundant with modulo

    // --- 3. Search for Existing Key ---
    // Get the head of the linked list for the determined bucket.
    _SituationUniformMapEntry* entry = map->buckets[index];

    // Traverse the linked list in this bucket to check if the key already exists.
    while (entry != NULL) {
        // Compare the current entry's key with the provided key.
        if (strcmp(entry->key, key) == 0) {
            // Key found. Update the existing entry's value.
            entry->value = value;
            return SITUATION_SUCCESS;
        }
        // Key not found in this entry, move to the next one in the chain.
        entry = entry->next;
    }

    // --- 4. Key Not Found: Create New Entry ---
    // If the loop completes, the key was not found in the bucket's list.
    // We need to create a new entry for this key/value pair.

    // Allocate memory for the new entry struct.
    _SituationUniformMapEntry* new_entry = (_SituationUniformMapEntry*)SIT_MALLOC(sizeof(_SituationUniformMapEntry));
    // Check if allocation for the new entry was successful.
    if (!new_entry) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "_sit_uniform_map_set: Failed to allocate new entry struct.");
    }

    // --- 5. Initialize New Entry ---
    // Duplicate the key string. This allocates memory and copies the string.
    // The caller retains ownership of the original `key` string.
    new_entry->key = _sit_strdup(key);
    // Check if strdup was successful.
    if (!new_entry->key) {
        SIT_FREE(new_entry);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
            "_sit_uniform_map_set: Failed to duplicate key string.");
    }

    // Set the value for the new entry.
    new_entry->value = value;

    // --- 6. Insert New Entry into the Hash Table ---
    // Link the new entry into the linked list for the bucket.
    // Set the new entry's `next` pointer to the current head of the list.
    new_entry->next = map->buckets[index];
    // Update the bucket's head pointer to point to the new entry.
    map->buckets[index] = new_entry;

    // Increment the total count of entries in the map.
    map->count++;

    // --- 7. Consider Resizing ---
    // Check if the load factor (count/capacity) is too high.
    if (map->count > map->capacity * 0.75) {
        SituationError resize_err = _sit_uniform_map_resize(map);
        if (resize_err != SITUATION_SUCCESS) {
            return resize_err;
        }
    }
    return SITUATION_SUCCESS;
}

// --- Updated/Added Documentation Block for _sit_uniform_map_get ---
/**
 * @brief [INTERNAL] Retrieves a value associated with a key from the uniform map.
 *
 * @details This helper function looks up a `key` (uniform name) in the hash map and returns the associated `GLint` value (uniform location) if found.
 * 1.  Calculates the hash of the `key`.
 * 2.  Determines the bucket index using `hash % capacity`.
 * 3.  Searches the linked list at that bucket for the key.
 * 4.  If found, returns the value.
 * 5.  If not found after traversing the list, returns -1.
 *
 * @param map A pointer to the `_SituationUniformMap` struct to search.
 * @param key A null-terminated C string representing the uniform name (the key) to find.
 *
 * @return The GLint value associated with the `key` if it is found in the map.
 * @return -1 if the `key` is not found, if `map` is NULL, or if `key` is NULL. Returning -1 is safe because valid uniform locations are non-negative.
 *
 * @warning This function is for internal library use only and is not part of the
 *          public API.
 *
 * @see _sit_uniform_map_set(), _sit_hash_string()
 */
static int32_t _sit_uniform_map_get(_SituationUniformMap* map, const char* key) {
    // --- 1. Input Validation ---
    // Check if the map or key pointer is NULL.
    if (!map || !key) {
        // Cannot search a NULL map or for a NULL key.
        // Return -1 to indicate "not found" or invalid input.
        return -1;
    }

    // --- 2. Calculate Hash and Bucket Index ---
    unsigned long hash = _sit_hash_string(key);
    // Determine the index of the bucket where this key might reside.
    int index = hash % map->capacity;
    // Ensure index is within valid bounds.
    // if (index < 0 || index >= map->capacity) { return -1; } // Redundant with modulo

    // --- 3. Search the Bucket's Linked List ---
    // Get the head of the linked list for the determined bucket.
    _SituationUniformMapEntry* entry = map->buckets[index];

    // Traverse the linked list in this bucket.
    while (entry != NULL) {
        // Compare the current entry's key with the provided key.
        if (strcmp(entry->key, key) == 0) {
            // Key found. Return the associated value.
            return entry->value;
        }
        // Key not found in this entry, move to the next one in the chain.
        entry = entry->next;
    }

    // --- 4. Key Not Found ---
    // If the loop completes, the key was not present in the map.
    // Return -1 to indicate "not found".
    return -1;
}

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Initializes the Per-Frame Staging Ring Buffers.
 *
 * @details Allocates a persistent, CPU-visible memory region for each frame-in-flight to serve as a
 *          high-performance staging area for asynchronous data uploads.
 *
 * @section Strategy The "Velocity" Ring Buffer Strategy
 *          To eliminate the massive overhead of allocating and destroying temporary staging buffers
 *          for every `SituationUpdateBuffer` call, this function pre-allocates a large (e.g., 32MB)
 *          buffer for each frame.
 *
 *          - **Persistent Mapping:** The memory is mapped immediately upon creation and stays mapped
 *            until shutdown. This allows `SituationUpdateBuffer` to simply `memcpy` data without
 *            any Vulkan API calls or kernel transitions in the fast path.
 *          - **Double Buffering:** By having a separate buffer for each frame-in-flight, the CPU
 *            can write to the current frame's buffer while the GPU is reading from the previous
 *            frame's buffer, requiring no pipeline stalls.
 *
 * @return SITUATION_SUCCESS on success.
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED if VMA cannot allocate the large blocks.
 */
static SituationError _SituationInitStagingBuffers(void) {
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        _SituationStagingBuffer* sb = &sit_render.vk.staging_buffers[i];
        sb->capacity = sit_render.vk.staging_buffer_size;
        sb->cursor = 0;

        VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_info.size = sb->capacity;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo alloc_info = {0};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Persistent Map

        VmaAllocationInfo result_info;
        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info,
            &sb->buffer, &sb->allocation, &result_info) != VK_SUCCESS) {
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
        }

        sb->mapped_data = (uint8_t*)result_info.pMappedData;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Releases the Per-Frame Staging Ring Buffers.
 *
 * @details Destroys the `VkBuffer` objects and frees the associated VMA memory allocations for the
 *          staging system. This is called during the Vulkan subsystem shutdown sequence.
 *
 * @note Because these buffers are persistently mapped, no explicit unmap call is required; VMA
 *       handles the unmapping during destruction.
 */
static void _SituationCleanupStagingBuffers(void) {
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        _SituationStagingBuffer* sb = &sit_render.vk.staging_buffers[i];
        if (sb->buffer != VK_NULL_HANDLE) {
            // No need to unmap explicitly if using VMA mapped bit
            vmaDestroyBuffer(sit_render.vk.vma_allocator, sb->buffer, sb->allocation);
            sb->buffer = VK_NULL_HANDLE;
        }
    }
}

// --- Vulkan Graveyard (Deferred Deletion) Implementation ---
/**
 * @section Vulkan Graveyard - The Solution to GPU Stalls
 *
 * @brief This subsystem is the core solution to the critical performance bottlenecks identified in the v2.3.4 release. It implements a deferred deletion queue, colloquially known as a "graveyard," to manage the lifecycle of GPU resources without ever stalling the CPU to wait for the GPU.
 *
 * @subsection The Problem: `vkDeviceWaitIdle` Abuse
 *   - **Issue:** Previous versions called `vkDeviceWaitIdle()` inside every `SituationDestroy*` and `SituationReload*` function. This forces the entire GPU to a complete stop, causing severe stuttering, especially during asset streaming or hot-reloading.
 *   - **Solution:** The `SituationDestroy*` functions no longer destroy resources immediately. Instead, they call a `_SituationDeferDestroy*` helper, which adds the resource's handles to a queue (the "graveyard").
 *
 * @subsection How It Works
 *   1. **Queuing:** When a resource is "destroyed," it's added to the graveyard associated with the *current* frame being recorded (`sit_render.vk.current_frame_index`).
 *   2. **Waiting:** The main render loop proceeds as normal. At the beginning of a new frame `N`, the engine calls `vkWaitForFences` to ensure that frame `N - max_frames_in_flight` has finished rendering on the GPU.
 *   3. **Flushing:** Only after the fence confirms the GPU is done with that old frame do we call `_SituationFlushGraveyard`. This function iterates through the old frame's graveyard queue and calls the real `vkDestroy*` / `vmaDestroy*` functions.
 *   - **Result:** This guarantees that resources are only deleted after the GPU is confirmed to be finished with them, eliminating all `vkDeviceWaitIdle` stalls from the resource management path.
 *
 * @subsection Asynchronous Transfers
 *   - **Issue:** Previously, functions like `_SituationVulkanCreateAndUploadBuffer` would create a temporary command buffer, submit it, and immediately call `vkQueueWaitIdle` to wait for the upload to finish, causing a CPU-GPU stall for every asset.
 *   - **Solution:** `_SituationVulkanCreateAndUploadBuffer` can now use the main frame's command buffer. It records the copy command and uses `_SituationDeferDestroyBuffer` to place the temporary staging buffer into the graveyard. The staging buffer is now cleaned up automatically and asynchronously, allowing for dozens of assets to be uploaded in a single frame without any stalls.
 *
 * @subsection Descriptor Pool Fragmentation
 *   - **Issue:** Calling `vkFreeDescriptorSets` frequently on pools that were not created with specific flags can be slow and lead to memory fragmentation, eventually causing allocation failures.
 *   - **Solution:** The `_SituationFlushGraveyard` function **intentionally does not call `vkFreeDescriptorSets`**. Instead, descriptor sets are treated as if they were allocated from a linear or "bump" allocator. They are only reclaimed when the entire `VkDescriptorPool` is reset or destroyed at shutdown. This is a standard high-performance strategy that trades a small amount of memory for maximum stability and zero-stutter performance during runtime.
 */

/**
 * @brief [INTERNAL] Initializes a single graveyard structure.
 * @details Allocates the initial memory for the resource handle arrays used to track deferred resources.
 *          Each type of resource (buffers, images, pipelines) has its own dynamic array.
 * @param gy Pointer to the `_SituationVKGraveyard` struct to initialize.
 */
static void _SituationInitGraveyard(_SituationVKGraveyard* gy) {
    memset(gy, 0, sizeof(_SituationVKGraveyard));
    // Pre-allocate some capacity to avoid initial reallocs
    gy->buffer_capacity = 16;
    gy->buffers = (VkBuffer*)SIT_MALLOC(sizeof(VkBuffer) * gy->buffer_capacity);
    gy->buffer_allocations = (VmaAllocation*)SIT_MALLOC(sizeof(VmaAllocation) * gy->buffer_capacity);

    gy->image_capacity = 16;
    gy->images = (VkImage*)SIT_MALLOC(sizeof(VkImage) * gy->image_capacity);
    gy->image_allocations = (VmaAllocation*)SIT_MALLOC(sizeof(VmaAllocation) * gy->image_capacity);
    gy->image_views = (VkImageView*)SIT_MALLOC(sizeof(VkImageView) * gy->image_capacity);
    gy->samplers = (VkSampler*)SIT_MALLOC(sizeof(VkSampler) * gy->image_capacity);

    gy->descriptor_set_capacity = 32;
    gy->descriptor_sets = (VkDescriptorSet*)SIT_MALLOC(sizeof(VkDescriptorSet) * gy->descriptor_set_capacity);
    gy->descriptor_pools = (VkDescriptorPool*)SIT_MALLOC(sizeof(VkDescriptorPool) * gy->descriptor_set_capacity);

    gy->pipeline_capacity = 8;
    gy->pipelines = (VkPipeline*)SIT_MALLOC(sizeof(VkPipeline) * gy->pipeline_capacity);
    gy->pipeline_layouts = (VkPipelineLayout*)SIT_MALLOC(sizeof(VkPipelineLayout) * gy->pipeline_capacity);

    gy->framebuffer_capacity = 4;
    gy->framebuffers = (VkFramebuffer*)SIT_MALLOC(sizeof(VkFramebuffer) * gy->framebuffer_capacity);

    gy->render_pass_capacity = 4;
    gy->render_passes = (VkRenderPass*)SIT_MALLOC(sizeof(VkRenderPass) * gy->render_pass_capacity);
}

/**
 * @brief [INTERNAL] Frees the CPU memory used by a graveyard's internal arrays.
 * @details This function only frees the containers (C arrays), not the Vulkan objects themselves.
 *          It assumes that `_SituationFlushGraveyard` has already been called to release the GPU resources.
 * @param gy Pointer to the `_SituationVKGraveyard` struct to clean up.
 */
static void _SituationCleanupGraveyard(_SituationVKGraveyard* gy) {
    // Ensure everything is flushed first (though device should be idle by now if called from CleanupVulkan)
    // Just free the arrays
    SIT_FREE(gy->buffers);
    SIT_FREE(gy->buffer_allocations);
    SIT_FREE(gy->images);
    SIT_FREE(gy->image_allocations);
    SIT_FREE(gy->image_views);
    SIT_FREE(gy->samplers);
    SIT_FREE(gy->descriptor_sets);
    SIT_FREE(gy->descriptor_pools);
    SIT_FREE(gy->pipelines);
    SIT_FREE(gy->pipeline_layouts);
    SIT_FREE(gy->framebuffers);
    SIT_FREE(gy->render_passes);
    memset(gy, 0, sizeof(_SituationVKGraveyard));
}

/**
 * @brief [INTERNAL] Destroys all Vulkan resources queued in a specific frame's graveyard.
 * @details This function iterates through all deferred destruction queues for the specified frame index
 *          and calls the appropriate Vulkan/VMA destroy functions.
 *          It must only be called when the GPU is confirmed to be finished with the frame (via fence wait).
 * @param frame_index The index of the frame whose graveyard should be flushed.
 */
static void _SituationFlushGraveyard(uint32_t frame_index) {
    if (!sit_render.vk.graveyards) return;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[frame_index];

    // Buffers
    for (int i = 0; i < gy->buffer_count; ++i) {
        if (gy->buffers[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, gy->buffers[i], gy->buffer_allocations[i]);
        }
    }
    gy->buffer_count = 0;

    // Images
    for (int i = 0; i < gy->image_count; ++i) {
        if (gy->samplers[i] != VK_NULL_HANDLE) vkDestroySampler(sit_render.vk.device, gy->samplers[i], NULL);
        if (gy->image_views[i] != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, gy->image_views[i], NULL);
        if (gy->images[i] != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, gy->images[i], gy->image_allocations[i]);
    }
    gy->image_count = 0;

    // Descriptor Sets
    if (gy->descriptor_set_count > 0) {
        for (int i = 0; i < gy->descriptor_set_count; ++i) {
            // [FIX v2.3.27B] Actually free the sets.
            // Note: If pool is NULL (legacy), we skip. But new logic ensures pool is passed.
            if (gy->descriptor_pools[i] != VK_NULL_HANDLE && gy->descriptor_sets[i] != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(sit_render.vk.device, gy->descriptor_pools[i], 1, &gy->descriptor_sets[i]);
            }
        }
    }
    gy->descriptor_set_count = 0;

    // Pipelines
    for (int i = 0; i < gy->pipeline_count; ++i) {
        if (gy->pipelines[i] != VK_NULL_HANDLE) vkDestroyPipeline(sit_render.vk.device, gy->pipelines[i], NULL);
        if (gy->pipeline_layouts[i] != VK_NULL_HANDLE) vkDestroyPipelineLayout(sit_render.vk.device, gy->pipeline_layouts[i], NULL);
    }
    gy->pipeline_count = 0;

    // Framebuffers
    for (int i = 0; i < gy->framebuffer_count; ++i) {
        if (gy->framebuffers[i] != VK_NULL_HANDLE) vkDestroyFramebuffer(sit_render.vk.device, gy->framebuffers[i], NULL);
    }
    gy->framebuffer_count = 0;

    // Render Passes
    for (int i = 0; i < gy->render_pass_count; ++i) {
        if (gy->render_passes[i] != VK_NULL_HANDLE) vkDestroyRenderPass(sit_render.vk.device, gy->render_passes[i], NULL);
    }
    gy->render_pass_count = 0;
}

#if defined(SITUATION_USE_VULKAN)
/* HARDENING: bool by design — shutdown-state query for immediate vs deferred destroy. */
static bool _SituationVulkanImmediateDestroyDuringShutdown(void) {
    return (SituationInitState)atomic_load(&sit_render.init_state) == SITUATION_STATE_SHUTTING_DOWN;
}
#endif

/**
 * @brief [INTERNAL] Schedules a Vulkan Buffer for deferred destruction.
 * @details Adds the buffer and its allocation to the graveyard of the *current* frame.
 * @param buffer The buffer handle to destroy.
 * @param allocation The VMA allocation handle associated with the buffer.
 */
static void _SituationDeferDestroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
    if (buffer == VK_NULL_HANDLE) return;
    // The resource is added to the graveyard of the *current* frame.
    // It will be destroyed when this frame's fence is signaled in a future SituationAcquireFrameCommandBuffer call.
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->buffer_count >= gy->buffer_capacity) {
        int new_cap = gy->buffer_capacity * 2;
        VkBuffer* new_buffers = (VkBuffer*)SIT_REALLOC(gy->buffers, sizeof(VkBuffer) * new_cap);
        VmaAllocation* new_allocs = (VmaAllocation*)SIT_REALLOC(gy->buffer_allocations, sizeof(VmaAllocation) * new_cap);
        if (!new_buffers || !new_allocs) {
            // Emergency: destroy immediately rather than leak or crash
            vmaDestroyBuffer(sit_render.vk.vma_allocator, buffer, allocation);
            return;
        }
        gy->buffers = new_buffers;
        gy->buffer_allocations = new_allocs;
        gy->buffer_capacity = new_cap;
    }
    gy->buffers[gy->buffer_count] = buffer;
    gy->buffer_allocations[gy->buffer_count] = allocation;
    gy->buffer_count++;
}

/**
 * @brief [INTERNAL] Schedules a Vulkan Image and its views/samplers for deferred destruction.
 * @details Adds the image, its memory, view, and sampler to the graveyard. Any parameter can be VK_NULL_HANDLE.
 * @param image The image handle.
 * @param allocation The VMA allocation handle.
 * @param view The image view handle (optional).
 * @param sampler The sampler handle (optional).
 */
static void _SituationDeferDestroyImage(VkImage image, VmaAllocation allocation, VkImageView view, VkSampler sampler) {
    if (image == VK_NULL_HANDLE && view == VK_NULL_HANDLE && sampler == VK_NULL_HANDLE) return;
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->image_count >= gy->image_capacity) {
        int new_cap = gy->image_capacity * 2;
        VkImage* new_images = (VkImage*)SIT_REALLOC(gy->images, sizeof(VkImage) * new_cap);
        VmaAllocation* new_allocs = (VmaAllocation*)SIT_REALLOC(gy->image_allocations, sizeof(VmaAllocation) * new_cap);
        VkImageView* new_views = (VkImageView*)SIT_REALLOC(gy->image_views, sizeof(VkImageView) * new_cap);
        VkSampler* new_samplers = (VkSampler*)SIT_REALLOC(gy->samplers, sizeof(VkSampler) * new_cap);
        if (!new_images || !new_allocs || !new_views || !new_samplers) {
            // Emergency: destroy immediately rather than leak or crash
            if (sampler != VK_NULL_HANDLE) vkDestroySampler(sit_render.vk.device, sampler, NULL);
            if (view != VK_NULL_HANDLE) vkDestroyImageView(sit_render.vk.device, view, NULL);
            if (image != VK_NULL_HANDLE) vmaDestroyImage(sit_render.vk.vma_allocator, image, allocation);
            return;
        }
        gy->images = new_images;
        gy->image_allocations = new_allocs;
        gy->image_views = new_views;
        gy->samplers = new_samplers;
        gy->image_capacity = new_cap;
    }
    gy->images[gy->image_count] = image;
    gy->image_allocations[gy->image_count] = allocation;
    gy->image_views[gy->image_count] = view;
    gy->samplers[gy->image_count] = sampler;
    gy->image_count++;
}

/**
 * @brief [INTERNAL] Schedules a Descriptor Set for deferred destruction.
 * @details If 'pool' is provided, the set will be freed back to that pool. If 'pool' is VK_NULL_HANDLE,
 *          the set is assumed to be from a linear allocator and will NOT be freed (just dropped).
 * @param set The descriptor set handle.
 * @param pool The descriptor pool it belongs to (or VK_NULL_HANDLE).
 */
static void _SituationDeferDestroyDescriptorSet(VkDescriptorSet set, VkDescriptorPool pool) {
    if (set == VK_NULL_HANDLE) return;
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->descriptor_set_count >= gy->descriptor_set_capacity) {
        int new_cap = gy->descriptor_set_capacity * 2;
        VkDescriptorSet* new_sets = (VkDescriptorSet*)SIT_REALLOC(gy->descriptor_sets, sizeof(VkDescriptorSet) * new_cap);
        VkDescriptorPool* new_pools = (VkDescriptorPool*)SIT_REALLOC(gy->descriptor_pools, sizeof(VkDescriptorPool) * new_cap);
        if (!new_sets || !new_pools) {
            // Emergency: free immediately rather than leak or crash
            if (pool != VK_NULL_HANDLE) vkFreeDescriptorSets(sit_render.vk.device, pool, 1, &set);
            return;
        }
        gy->descriptor_sets = new_sets;
        gy->descriptor_pools = new_pools;
        gy->descriptor_set_capacity = new_cap;
    }
    gy->descriptor_sets[gy->descriptor_set_count] = set;
    gy->descriptor_pools[gy->descriptor_set_count] = pool;
    gy->descriptor_set_count++;
}

/**
 * @brief [INTERNAL] Schedules a Pipeline and its Layout for deferred destruction.
 * @details Ensures that the pipeline and its layout are kept alive until the GPU finishes using them.
 * @param pipeline The pipeline handle.
 * @param layout The pipeline layout handle.
 */
static void _SituationDeferDestroyPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    if (pipeline == VK_NULL_HANDLE) return;
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->pipeline_count >= gy->pipeline_capacity) {
        int new_cap = gy->pipeline_capacity * 2;
        VkPipeline* new_pipelines = (VkPipeline*)SIT_REALLOC(gy->pipelines, sizeof(VkPipeline) * new_cap);
        VkPipelineLayout* new_layouts = (VkPipelineLayout*)SIT_REALLOC(gy->pipeline_layouts, sizeof(VkPipelineLayout) * new_cap);
        if (!new_pipelines || !new_layouts) {
            // Emergency: destroy immediately
            vkDestroyPipeline(sit_render.vk.device, pipeline, NULL);
            if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(sit_render.vk.device, layout, NULL);
            return;
        }
        gy->pipelines = new_pipelines;
        gy->pipeline_layouts = new_layouts;
        gy->pipeline_capacity = new_cap;
    }
    gy->pipelines[gy->pipeline_count] = pipeline;
    gy->pipeline_layouts[gy->pipeline_count] = layout;
    gy->pipeline_count++;
}

/**
 * @brief [INTERNAL] Schedules a Framebuffer for deferred destruction.
 * @param framebuffer The framebuffer handle.
 */
static void _SituationDeferDestroyFramebuffer(VkFramebuffer framebuffer) {
    if (framebuffer == VK_NULL_HANDLE) return;
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->framebuffer_count >= gy->framebuffer_capacity) {
        gy->framebuffer_capacity *= 2;
        gy->framebuffers = (VkFramebuffer*)SIT_REALLOC(gy->framebuffers, sizeof(VkFramebuffer) * gy->framebuffer_capacity);
    }
    gy->framebuffers[gy->framebuffer_count++] = framebuffer;
}

/**
 * @brief [INTERNAL] Schedules a Render Pass for deferred destruction.
 * @param render_pass The render pass handle.
 */
static void _SituationDeferDestroyRenderPass(VkRenderPass render_pass) {
    if (render_pass == VK_NULL_HANDLE) return;
    uint32_t gy_idx = sit_render.vk.current_frame_index;
    _SituationVKGraveyard* gy = &sit_render.vk.graveyards[gy_idx];
    if (gy->render_pass_count >= gy->render_pass_capacity) {
        gy->render_pass_capacity *= 2;
        gy->render_passes = (VkRenderPass*)SIT_REALLOC(gy->render_passes, sizeof(VkRenderPass) * gy->render_pass_capacity);
    }
    gy->render_passes[gy->render_pass_count++] = render_pass;
}
#endif

// --- Error Handling Implementation ---
/**
 * @brief [INTERNAL] Atomically sets the library's last error message.
 * @details This is the core internal function for setting the global error string. It is designed to be robust and thread-safe. It uses `strncpy` to prevent buffer overflows and correctly handles `NULL` input by providing a default message.
 *
 * @par Thread Safety
 *   All write access to the global `sit_gs.last_error_msg` buffer is protected by a dedicated mutex (`sit_gs.error_mutex`). This ensures that if two different threads encounter errors simultaneously,
 *   the error messages will not be interleaved or corrupted. This is a critical feature for preparing the library for future multi-threading.
 *
 * @note This function is designed to be callable at any point, even before the library is fully initialized, making it suitable for reporting errors during the startup sequence.
 *
 * @param msg The null-terminated error message string to be set. If NULL, a default "Unknown error" message will be used.
 */
// [Helper] Get Buffer Node by ID



/**
 * @brief [INTERNAL] Sets the library's last error message from an error code and an optional detail string.
 * @details This internal helper translates a `SituationError` enum into a human-readable base message, appends a specific detail string if provided, and stores the final, formatted result in the global error message buffer via `_SituationSetError`.
 *
 * This function serves as the central switchboard for all error reporting in the library, ensuring consistent and descriptive messages.
 *
 * @param err The `SituationError` code to translate.
 * @param detail An optional, more specific string describing the context of the error (can be `NULL`).
 *
 * @note This function is for internal use only.
 * @see _SituationSetError(), SituationGetLastErrorMsg(), SituationError
 */

// --- Updated/Added Documentation Block for _SituationGLFWErrorCallback ---

#if defined(SITUATION_USE_OPENGL)
/**
 * @brief [INTERNAL] Snapshots specific OpenGL state variables into a backup structure.
 * @param s A pointer to a `_SitGLStateBackup` struct to populate with the current state.
 * @see _SitGLRestoreState(), SituationRenderVirtualDisplays()
 */
static void _SitGLBackupState(_SitGLStateBackup* s) {
    s->program = sit_render.gl.current_program_id;
    s->vao = sit_render.gl.current_vao_id;
    s->fbo = sit_render.gl.current_fbo_id;

    s->blend = (sit_render.gl.blend_enabled == -1) ? glIsEnabled(GL_BLEND) : (GLboolean)sit_render.gl.blend_enabled;
    s->depth_test = (sit_render.gl.depth_test_enabled == -1) ? glIsEnabled(GL_DEPTH_TEST) : (GLboolean)sit_render.gl.depth_test_enabled;
    s->cull_face = (sit_render.gl.cull_face_enabled == -1) ? glIsEnabled(GL_CULL_FACE) : (GLboolean)sit_render.gl.cull_face_enabled;
    s->scissor_test = (sit_render.gl.scissor_test_enabled == -1) ? glIsEnabled(GL_SCISSOR_TEST) : (GLboolean)sit_render.gl.scissor_test_enabled;

    // [FIX v2.4.39] Query actual GL state when shadow state is invalid (GL_NONE = 0).
    // GL_NONE is not a valid blend factor — passing it to glBlendFuncSeparate causes GL_INVALID_ENUM.
    if (sit_render.gl.blend_src_rgb == GL_NONE) {
        glGetIntegerv(GL_BLEND_SRC_RGB, &s->blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &s->blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &s->blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &s->blend_dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &s->blend_equ_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &s->blend_equ_alpha);
    } else {
        s->blend_src_rgb = sit_render.gl.blend_src_rgb;
        s->blend_dst_rgb = sit_render.gl.blend_dst_rgb;
        s->blend_src_alpha = sit_render.gl.blend_src_alpha;
        s->blend_dst_alpha = sit_render.gl.blend_dst_alpha;
        s->blend_equ_rgb = sit_render.gl.blend_eq_rgb;
        s->blend_equ_alpha = sit_render.gl.blend_eq_alpha;
    }
}

/**
 * @brief [INTERNAL] Restores OpenGL state from a backup structure.
 * @param s A pointer to the `_SitGLStateBackup` struct containing the state to restore.
 * @see _SitGLBackupState()
 */
static void _SitGLRestoreState(_SitGLStateBackup* s) {
    glUseProgram(s->program);
    glBindVertexArray(s->vao);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->fbo);

    if (s->blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_alpha, s->blend_dst_alpha);
    glBlendEquationSeparate(s->blend_equ_rgb, s->blend_equ_alpha);

    if (s->depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (s->cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (s->scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

static bool _SitGLHasStencilBuffer(void) {
    GLint stencil_size = 0;
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_STENCIL,
        GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil_size);
    if (stencil_size > 0) {
        return true;
    }
    GLint bits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &bits);
    return bits > 0;
}

static GLenum _SitGLMapDepthCompare(SituationDepthCompareOp op) {
    switch (op) {
        case SIT_DEPTH_COMPARE_ALWAYS:   return GL_ALWAYS;
        case SIT_DEPTH_COMPARE_LESS:     return GL_LESS;
        case SIT_DEPTH_COMPARE_LEQUAL:   return GL_LEQUAL;
        case SIT_DEPTH_COMPARE_GREATER:  return GL_GREATER;
        case SIT_DEPTH_COMPARE_GEQUAL:   return GL_GEQUAL;
        case SIT_DEPTH_COMPARE_EQUAL:    return GL_EQUAL;
        case SIT_DEPTH_COMPARE_NOTEQUAL: return GL_NOTEQUAL;
        case SIT_DEPTH_COMPARE_NEVER:    return GL_NEVER;
        default:                         return GL_LESS;
    }
}

static GLenum _SitGLMapStencilOp(SituationStencilOp op) {
    switch (op) {
        case SIT_STENCIL_OP_ZERO:              return GL_ZERO;
        case SIT_STENCIL_OP_REPLACE:           return GL_REPLACE;
        case SIT_STENCIL_OP_INCREMENT_CLAMP:   return GL_INCR;
        case SIT_STENCIL_OP_DECREMENT_CLAMP:   return GL_DECR;
        case SIT_STENCIL_OP_INVERT:            return GL_INVERT;
        case SIT_STENCIL_OP_INCREMENT_WRAP:    return GL_INCR_WRAP;
        case SIT_STENCIL_OP_DECREMENT_WRAP:    return GL_DECR_WRAP;
        case SIT_STENCIL_OP_KEEP:
        default:                               return GL_KEEP;
    }
}

static void _SitGLApplyStencilFace(GLenum face, const SituationStencilState* state) {
    if (!state) return;
    glStencilFuncSeparate(face, _SitGLMapDepthCompare(state->compare_op),
                        (GLint)state->reference, (GLuint)state->compare_mask);
    glStencilOpSeparate(face, _SitGLMapStencilOp(state->fail_op),
                        _SitGLMapStencilOp(state->depth_fail_op),
                        _SitGLMapStencilOp(state->pass_op));
    glStencilMaskSeparate(face, (GLuint)state->write_mask);
}

static void _SitGLCaptureRasterState(_SitGLRasterStackEntry* e) {
    if (!e) return;
    e->blend = (sit_render.gl.blend_enabled == -1) ? glIsEnabled(GL_BLEND) : (GLboolean)sit_render.gl.blend_enabled;
    e->depth_test = (sit_render.gl.depth_test_enabled == -1) ? glIsEnabled(GL_DEPTH_TEST) : (GLboolean)sit_render.gl.depth_test_enabled;
    e->cull_face = (sit_render.gl.cull_face_enabled == -1) ? glIsEnabled(GL_CULL_FACE) : (GLboolean)sit_render.gl.cull_face_enabled;
    e->scissor_test = (sit_render.gl.scissor_test_enabled == -1) ? glIsEnabled(GL_SCISSOR_TEST) : (GLboolean)sit_render.gl.scissor_test_enabled;
    e->stencil_test = glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK, e->color_mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &e->depth_mask);
    if (sit_render.gl.blend_src_rgb == GL_NONE) {
        glGetIntegerv(GL_BLEND_SRC_RGB, &e->blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &e->blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &e->blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &e->blend_dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &e->blend_equ_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &e->blend_equ_alpha);
    } else {
        e->blend_src_rgb = (GLint)sit_render.gl.blend_src_rgb;
        e->blend_dst_rgb = (GLint)sit_render.gl.blend_dst_rgb;
        e->blend_src_alpha = (GLint)sit_render.gl.blend_src_alpha;
        e->blend_dst_alpha = (GLint)sit_render.gl.blend_dst_alpha;
        e->blend_equ_rgb = (GLint)sit_render.gl.blend_eq_rgb;
        e->blend_equ_alpha = (GLint)sit_render.gl.blend_eq_alpha;
    }
    glGetIntegerv(GL_DEPTH_FUNC, (GLint*)&e->depth_func);
    glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&e->cull_face_mode);
    glGetIntegerv(GL_FRONT_FACE, (GLint*)&e->front_face);
    e->polygon_mode = sit_render.gl.current_polygon_mode;
    e->polygon_offset_fill = sit_render.gl.polygon_offset_enabled ? GL_TRUE : GL_FALSE;
    if (e->polygon_offset_fill) {
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &e->polygon_offset_factor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &e->polygon_offset_units);
    } else {
        e->polygon_offset_factor = 0.0f;
        e->polygon_offset_units = 0.0f;
    }
    glGetFloatv(GL_LINE_WIDTH, &e->line_width);
    e->primitive_mode = sit_render.gl.current_primitive_mode;
    e->primitive_mode_set = sit_render.gl.current_primitive_mode_set;
    glGetIntegerv(GL_STENCIL_FUNC, &e->stencil_func_front);
    glGetIntegerv(GL_STENCIL_REF, &e->stencil_ref_front);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, &e->stencil_value_mask_front);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &e->stencil_writemask_front);
    glGetIntegerv(GL_STENCIL_FAIL, &e->stencil_fail_front);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &e->stencil_depth_fail_front);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &e->stencil_pass_front);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &e->stencil_func_back);
    glGetIntegerv(GL_STENCIL_BACK_REF, &e->stencil_ref_back);
    glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &e->stencil_value_mask_back);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &e->stencil_writemask_back);
    glGetIntegerv(GL_STENCIL_BACK_FAIL, &e->stencil_fail_back);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &e->stencil_depth_fail_back);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &e->stencil_pass_back);
    // Multisample state
    e->multisample_sample_shading   = glIsEnabled(GL_SAMPLE_SHADING);
    glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE, &e->multisample_min_shading);
    GLint sample_mask_val = 0;
    glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &sample_mask_val);
    e->multisample_sample_mask      = (GLuint)sample_mask_val;
    e->multisample_alpha_to_coverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

static void _SitGLApplyRasterState(const _SitGLRasterStackEntry* e) {
    if (!e) return;
    if (e->blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    sit_render.gl.blend_enabled = e->blend ? 1 : 0;

    if (e->depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthFunc(e->depth_func);
    sit_render.gl.depth_test_enabled = e->depth_test ? 1 : 0;

    glDepthMask(e->depth_mask);

    if (e->cull_face) {
        glEnable(GL_CULL_FACE);
        glCullFace(e->cull_face_mode);
    } else {
        glDisable(GL_CULL_FACE);
    }
    sit_render.gl.cull_face_enabled = e->cull_face ? 1 : 0;

    glFrontFace(e->front_face);

    if (e->scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    sit_render.gl.scissor_test_enabled = e->scissor_test ? 1 : 0;

    if (e->stencil_test) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

    glColorMask(e->color_mask[0], e->color_mask[1], e->color_mask[2], e->color_mask[3]);

    glPolygonMode(GL_FRONT_AND_BACK, e->polygon_mode);
    sit_render.gl.current_polygon_mode = e->polygon_mode;

    if (e->polygon_offset_fill) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(e->polygon_offset_factor, e->polygon_offset_units);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    sit_render.gl.polygon_offset_enabled = e->polygon_offset_fill ? 1 : 0;

    glLineWidth(e->line_width);

    sit_render.gl.current_primitive_mode = e->primitive_mode;
    sit_render.gl.current_primitive_mode_set = e->primitive_mode_set;
    if (e->primitive_mode == GL_POINTS) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    } else {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
    // Multisample state
    if (e->multisample_sample_shading) {
        glEnable(GL_SAMPLE_SHADING);
        glMinSampleShading(e->multisample_min_shading);
    } else {
        glDisable(GL_SAMPLE_SHADING);
    }
    glSampleMaski(0, e->multisample_sample_mask ? e->multisample_sample_mask : 0xFFFFFFFFu);
    if (e->multisample_alpha_to_coverage) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
}

/**
 * @brief [INTERNAL] Helper to force a re-upload of state next time it's requested.
 * @details [2.3.14A] Invalidates the internal shadow state tracking.
 */
static void _SitGLInvalidateShadowState(void) {
    sit_render.gl.current_program_id = 0;
    sit_render.gl.current_vao_id = 0;
    sit_render.gl.current_fbo_id = 0;

    sit_render.gl.blend_enabled = -1;
    sit_render.gl.blend_src_rgb = GL_NONE;
    sit_render.gl.depth_test_enabled = -1;
    sit_render.gl.cull_face_enabled = -1;
    sit_render.gl.scissor_test_enabled = -1;

    sit_render.gl.shadow_state_dirty = true;
}

static GLuint _SitGLGetCachedVAO(SituationMesh mesh) {
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot || slot->vbo_id == 0) return 0;

    uint64_t key = (uint64_t)slot->vbo_id;

    int bucket = (int)(key % 256);
    _SitGLVaoCacheEntry* entry = sit_render.gl.vao_cache[bucket];
    while (entry) {
        if (entry->mesh_id == key) return entry->vao_id;
        entry = entry->next;
    }

    GLuint vao;
    glCreateVertexArrays(1, &vao);

    glVertexArrayVertexBuffer(vao, 0, slot->vbo_id, 0, (GLsizei)slot->vertex_stride);
    if (slot->ebo_id) glVertexArrayElementBuffer(vao, slot->ebo_id);

    glEnableVertexArrayAttrib(vao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(vao, SIT_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, SIT_ATTR_POSITION, 0);

    glEnableVertexArrayAttrib(vao, SIT_ATTR_NORMAL);
    glVertexArrayAttribFormat(vao, SIT_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(vao, SIT_ATTR_NORMAL, 0);

    if (slot->vertex_layout == SIT_MESH_LAYOUT_POS_NRM_TAN_TEX) {
        glEnableVertexArrayAttrib(vao, SIT_ATTR_TANGENT);
        glVertexArrayAttribFormat(vao, SIT_ATTR_TANGENT, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
        glVertexArrayAttribBinding(vao, SIT_ATTR_TANGENT, 0);
        glEnableVertexArrayAttrib(vao, SIT_ATTR_TEXCOORD_0);
        glVertexArrayAttribFormat(vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float));
        glVertexArrayAttribBinding(vao, SIT_ATTR_TEXCOORD_0, 0);
    } else {
        glEnableVertexArrayAttrib(vao, SIT_ATTR_TEXCOORD_0);
        glVertexArrayAttribFormat(vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
        glVertexArrayAttribBinding(vao, SIT_ATTR_TEXCOORD_0, 0);
    }

    _SitGLVaoCacheEntry* new_entry = (_SitGLVaoCacheEntry*)SIT_MALLOC(sizeof(_SitGLVaoCacheEntry));
    if (new_entry) {
        new_entry->mesh_id = key;
        new_entry->vao_id = vao;
        new_entry->next = sit_render.gl.vao_cache[bucket];
        sit_render.gl.vao_cache[bucket] = new_entry;
    }

    return vao;
}

#if SIT_GL_SHADER_CACHE_ENABLE
static void _SitGLDeferDestroyProgram(GLuint id); /* defined with other GL graveyard helpers */

/* FNV-1a 64-bit — same algorithm as Vulkan Layer A (_SitVkHashBytes64). */
static inline uint64_t _SitGLHashBytes64(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static inline uint32_t _SitGLProgramCacheBucket(uint64_t key) {
    return (uint32_t)(key % SIT_GL_SHADER_CACHE_MAX_ENTRIES);
}

/* Layer A source key for GLSL memory loads (matches Vulkan vs_hash/fs_hash/fingerprint shape). */
static inline uint64_t _SitGLLayerAKeyFromSource(const char* vs_code, const char* fs_code) {
    uint64_t vs_h = _SitGLHashBytes64(vs_code, strlen(vs_code));
    uint64_t fs_h = _SitGLHashBytes64(fs_code, strlen(fs_code));
    uint64_t fp = 0x474C5F50524F4701ULL; /* "GL_PROG" + version salt */
#if !defined(NDEBUG)
    fp ^= 0x1ULL;
#endif
    return vs_h ^ (fs_h << 1) ^ fp;
}

static void _SitGLProgramCacheInit(_SitGLProgramCache* c) {
    memset(c, 0, sizeof(*c));
    if (mtx_init(&c->mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,
            "Shader cache: failed to initialize cache mutex.");
        return;
    }
    c->mutex_initialized = true;
}

static void _SitGLProgramCacheShutdown(_SitGLProgramCache* c) {
    if (!c || !c->mutex_initialized) return;
    _SituationMakeGLContextCurrentForHostThread();
    mtx_lock(&c->mutex);
    for (uint32_t i = 0; i < SIT_GL_SHADER_CACHE_MAX_ENTRIES; i++) {
        _SitGLProgramCacheEntry* e = c->buckets[i];
        while (e) {
            _SitGLProgramCacheEntry* next = e->next;
            if (e->program_id) glDeleteProgram(e->program_id);
            SIT_FREE(e);
            e = next;
        }
        c->buckets[i] = NULL;
    }
#if !defined(NDEBUG)
    if (c->hits + c->misses > 0) {
        fprintf(stderr,
            "Situation [OpenGL Debug]: program cache — hits=%llu misses=%llu evictions=%llu\n",
            (unsigned long long)c->hits, (unsigned long long)c->misses,
            (unsigned long long)c->evictions);
    }
#endif
    mtx_unlock(&c->mutex);
    mtx_destroy(&c->mutex);
    c->mutex_initialized = false;
}

static bool _SitGLProgramCacheTryHit(_SitGLProgramCache* c, uint64_t layer_a_key,
        GLuint* out_program, _SitGLProgramCacheRef* out_ref, uint32_t current_frame) {
    uint32_t bucket = _SitGLProgramCacheBucket(layer_a_key);
    mtx_lock(&c->mutex);
    for (_SitGLProgramCacheEntry* e = c->buckets[bucket]; e; e = e->next) {
        if (e->layer_a_key == layer_a_key
                && (e->state == SIT_GL_PROG_READY || e->state == SIT_GL_PROG_EVICT_PENDING)) {
            atomic_fetch_add(&e->ref_count, 1u);
            e->last_used_frame = current_frame;
            if (e->state == SIT_GL_PROG_EVICT_PENDING)
                e->state = SIT_GL_PROG_READY;
            *out_program = e->program_id;
            out_ref->entry = e;
            out_ref->generation = e->generation;
#if !defined(NDEBUG)
            c->hits++;
#endif
            mtx_unlock(&c->mutex);
            return true;
        }
    }
    mtx_unlock(&c->mutex);
    return false;
}

static bool _SitGLProgramCacheInsert(_SitGLProgramCache* c, uint64_t layer_a_key, GLuint program,
        _SitGLProgramCacheRef* out_ref, uint32_t current_frame) {
    uint32_t bucket = _SitGLProgramCacheBucket(layer_a_key);
    mtx_lock(&c->mutex);
    for (_SitGLProgramCacheEntry* e = c->buckets[bucket]; e; e = e->next) {
        if (e->layer_a_key == layer_a_key
                && (e->state == SIT_GL_PROG_READY || e->state == SIT_GL_PROG_EVICT_PENDING)) {
            atomic_fetch_add(&e->ref_count, 1u);
            e->last_used_frame = current_frame;
            if (e->state == SIT_GL_PROG_EVICT_PENDING)
                e->state = SIT_GL_PROG_READY;
            *out_ref = (_SitGLProgramCacheRef){ e, e->generation };
            if (program && program != e->program_id) glDeleteProgram(program);
            mtx_unlock(&c->mutex);
            return true;
        }
    }
    _SitGLProgramCacheEntry* n = (_SitGLProgramCacheEntry*)SIT_MALLOC(sizeof(_SitGLProgramCacheEntry));
    if (!n) { mtx_unlock(&c->mutex); return false; }
    memset(n, 0, sizeof(*n));
    n->layer_a_key = layer_a_key;
    n->program_id = program;
    atomic_init(&n->ref_count, 1u);
    n->generation = 1u;
    n->last_used_frame = current_frame;
    n->state = SIT_GL_PROG_READY;
    n->next = c->buckets[bucket];
    c->buckets[bucket] = n;
    *out_ref = (_SitGLProgramCacheRef){ n, n->generation };
#if !defined(NDEBUG)
    c->misses++;
#endif
    mtx_unlock(&c->mutex);
    return true;
}

static void _SitGLProgramCacheRelease(_SitGLProgramCacheRef* ref, uint32_t current_frame) {
    if (!ref || !ref->entry) return;
    _SitGLProgramCacheEntry* e = ref->entry;
    if (e->generation != ref->generation) {
        ref->entry = NULL;
        ref->generation = 0;
        return;
    }
    uint32_t prev = atomic_fetch_sub(&e->ref_count, 1u);
    if (prev == 1u) {
        mtx_lock(&sit_render.gl.program_cache.mutex);
        e->state = SIT_GL_PROG_EVICT_PENDING;
        e->last_used_frame = current_frame;
        mtx_unlock(&sit_render.gl.program_cache.mutex);
    }
    ref->entry = NULL;
    ref->generation = 0;
}

static void _SitGLProgramCacheProcessEvictions(_SitGLProgramCache* c, uint32_t current_frame) {
    if (!c->mutex_initialized) return;
    mtx_lock(&c->mutex);
    for (uint32_t bi = 0; bi < SIT_GL_SHADER_CACHE_MAX_ENTRIES; bi++) {
        _SitGLProgramCacheEntry** prev_ptr = &c->buckets[bi];
        _SitGLProgramCacheEntry* e = *prev_ptr;
        while (e) {
            bool evict = e->state == SIT_GL_PROG_EVICT_PENDING
                && atomic_load(&e->ref_count) == 0u
                && e->last_used_frame + SIT_GL_SHADER_CACHE_EVICT_DELAY_FRAMES <= current_frame;
            if (evict) {
                e->generation++;
                GLuint prog = e->program_id;
                _SitGLProgramCacheEntry* dead = e;
                *prev_ptr = e->next;
                e = e->next;
                SIT_FREE(dead);
                mtx_unlock(&c->mutex);
                _SitGLDeferDestroyProgram(prog);
                mtx_lock(&c->mutex);
#if !defined(NDEBUG)
                c->evictions++;
#endif
            } else {
                prev_ptr = &e->next;
                e = e->next;
            }
        }
    }
    mtx_unlock(&c->mutex);
}

/* Load helper: cache hit skips compile/link; miss creates program and inserts. */
static SituationError _SitGLLoadShaderProgramCached(_SituationShaderSlot* slot,
        const char* vs_code, const char* fs_code) {
    _SitGLProgramCache* cache = &sit_render.gl.program_cache;
    uint64_t key = _SitGLLayerAKeyFromSource(vs_code, fs_code);
    uint32_t frame = (uint32_t)sit_render.current_frame_index;
    GLuint program = 0;
    _SitGLProgramCacheRef cref = {0};

    if (_SitGLProgramCacheTryHit(cache, key, &program, &cref, frame)) {
        slot->gl_program_id = program;
        slot->gl_program_cache_ref = cref;
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) return SITUATION_ERROR_MEMORY_ALLOCATION;
        return SITUATION_SUCCESS;
    }

    SituationError err = SITUATION_SUCCESS;
    program = _SituationCreateGLShaderProgram(vs_code, fs_code, &err);
    if (err != SITUATION_SUCCESS) return err;

    if (!_SitGLProgramCacheInsert(cache, key, program, &cref, frame)) {
        glDeleteProgram(program);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    slot->gl_program_id = program;
    slot->gl_program_cache_ref = cref;
    slot->uniform_map = _sit_uniform_map_create();
    if (!slot->uniform_map) {
        _SitGLProgramCacheRelease(&slot->gl_program_cache_ref, frame);
        slot->gl_program_id = 0;
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    return SITUATION_SUCCESS;
}
#endif /* SIT_GL_SHADER_CACHE_ENABLE */


static void _SitGLDeferDestroyProgram(GLuint id) {
    if (id == 0) return;
    _SituationGLGraveyard* gy = &sit_render.gl.graveyards[sit_render.current_frame_index];
    ma_mutex_lock(&gy->lock);
    if (gy->program_count >= gy->program_capacity) {
        size_t new_cap = gy->program_capacity ? gy->program_capacity * 2 : 8;
        GLuint* new_ptr = (GLuint*)SIT_REALLOC(gy->programs_to_delete, new_cap * sizeof(GLuint));
        if (!new_ptr) {
            glDeleteProgram(id);
            ma_mutex_unlock(&gy->lock);
            return;
        }
        gy->programs_to_delete = new_ptr;
        gy->program_capacity = new_cap;
    }
    gy->programs_to_delete[gy->program_count++] = id;
    ma_mutex_unlock(&gy->lock);
}

static void _SitGLDeferDestroyBuffer(GLuint id) {
    if (id == 0) return;
    _SituationGLGraveyard* gy = &sit_render.gl.graveyards[sit_render.current_frame_index];
    ma_mutex_lock(&gy->lock);
    if (gy->buffer_count >= gy->buffer_capacity) {
        size_t new_cap = gy->buffer_capacity * 2;
        GLuint* new_ptr = (GLuint*)SIT_REALLOC(gy->buffers_to_delete, new_cap * sizeof(GLuint));
        if (!new_ptr) {
            // Emergency: delete immediately (safe since we hold the lock and this is deferred anyway)
            glDeleteBuffers(1, &id);
            ma_mutex_unlock(&gy->lock);
            return;
        }
        gy->buffers_to_delete = new_ptr;
        gy->buffer_capacity = new_cap;
    }
    gy->buffers_to_delete[gy->buffer_count++] = id;
    ma_mutex_unlock(&gy->lock);
}

static void _SitGLDeferDestroyTexture(GLuint id) {
    if (id == 0) return;
    _SituationGLGraveyard* gy = &sit_render.gl.graveyards[sit_render.current_frame_index];
    ma_mutex_lock(&gy->lock);
    if (gy->texture_count >= gy->texture_capacity) {
        size_t new_cap = gy->texture_capacity * 2;
        GLuint* new_ptr = (GLuint*)SIT_REALLOC(gy->textures_to_delete, new_cap * sizeof(GLuint));
        if (!new_ptr) {
            glDeleteTextures(1, &id);
            ma_mutex_unlock(&gy->lock);
            return;
        }
        gy->textures_to_delete = new_ptr;
        gy->texture_capacity = new_cap;
    }
    gy->textures_to_delete[gy->texture_count++] = id;
    ma_mutex_unlock(&gy->lock);
}

static void _SitGLDeferCleanMeshVAO(uint64_t mesh_id) {
    if (mesh_id == 0) return;
    _SituationGLGraveyard* gy = &sit_render.gl.graveyards[sit_render.current_frame_index];
    ma_mutex_lock(&gy->lock);
    if (gy->mesh_count >= gy->mesh_capacity) {
        size_t new_cap = gy->mesh_capacity * 2;
        gy->mesh_ids_to_clean = (uint64_t*)SIT_REALLOC(gy->mesh_ids_to_clean, new_cap * sizeof(uint64_t));
        gy->mesh_capacity = new_cap;
    }
    gy->mesh_ids_to_clean[gy->mesh_count++] = mesh_id;
    ma_mutex_unlock(&gy->lock);
}

static void _SitGLFlushGraveyard(int frame_index) {
    if (frame_index < 0 || frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) return;
    _SituationGLGraveyard* gy = &sit_render.gl.graveyards[frame_index];

    ma_mutex_lock(&gy->lock);

    if (gy->buffer_count == 0 && gy->texture_count == 0 && gy->mesh_count == 0
            && gy->program_count == 0) {
        ma_mutex_unlock(&gy->lock);
        return;
    }

    if (gy->buffer_count > 0) {
        glDeleteBuffers((GLsizei)gy->buffer_count, gy->buffers_to_delete);
        gy->buffer_count = 0;
    }

    if (gy->texture_count > 0) {
        glDeleteTextures((GLsizei)gy->texture_count, gy->textures_to_delete);
        gy->texture_count = 0;
    }

    if (gy->program_count > 0) {
        for (size_t i = 0; i < gy->program_count; ++i) {
            glDeleteProgram(gy->programs_to_delete[i]);
        }
        gy->program_count = 0;
    }

    if (gy->mesh_count > 0) {
        for (size_t i = 0; i < gy->mesh_count; ++i) {
            uint64_t id = gy->mesh_ids_to_clean[i];
            int bucket = (int)(id % 256);
            _SitGLVaoCacheEntry* entry = sit_render.gl.vao_cache[bucket];
            _SitGLVaoCacheEntry* prev = NULL;

            while (entry) {
                if (entry->mesh_id == id) {
                    if (prev) prev->next = entry->next;
                    else sit_render.gl.vao_cache[bucket] = entry->next;

                    glDeleteVertexArrays(1, &entry->vao_id);
                    SIT_FREE(entry);
                    break;
                }
                prev = entry;
                entry = entry->next;
            }
        }
        gy->mesh_count = 0;
    }

    ma_mutex_unlock(&gy->lock);
}

static void _SituationCheckGLError(const char* location) {
    if (!location) {
        location = "<unknown_location>";
    }

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char detail[SITUATION_MAX_ERROR_MSG_LEN];
        int written = snprintf(
            detail,
            sizeof(detail),
            "_SituationCheckGLError: OpenGL Error at '%s': 0x%X",
            location,
            (unsigned int)err
        );

        if (written < 0 || (size_t)written >= sizeof(detail)) {
            snprintf(
                detail,
                sizeof(detail),
                "_SituationCheckGLError: Error formatting failed for check at '%s', original code was 0x%X",
                location,
                (unsigned int)err
            );
        }

        // --- 5. Store Error in Global State ---
        // Update the library's global error state with the detailed message.
        // This makes the error retrievable via SituationGetLastErrorMsg().
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, detail);

        // Note: Unlike _SituationLogGLError, there is no fprintf to stderr here.
        // The error is only stored in the global state.
    }
    // --- 6. Loop Exit ---
    // The loop exits when glGetError() returns GL_NO_ERROR, indicating the OpenGL error queue is now empty (or was empty to begin with).
}

/**
 * @brief [INTERNAL] Logs OpenGL errors detected by `glGetError`.
 *
 * @details This helper function is the core implementation used by the `SIT_CHECK_GL_ERROR` macro. It polls the OpenGL error state using `glGetError()` in a loop, processing each error individually until no more errors are reported (`GL_NO_ERROR` is returned).
 *          For each error found, it translates the `GLenum` error code into a human-readable string, formats a detailed message including the source file and line number where the check occurred, and stores this message in the library's global error state using `_SituationSetErrorFromCode`.
 *          This ensures that `SituationGetLastErrorMsg()` will return the most recent OpenGL error details.
 *          In debug builds (when `NDEBUG` is not defined), it also prints the error message to `stderr`. This immediate feedback is invaluable during development and debugging.
 *
 * @param file The source code file name where the error check was triggered.
 *             This is typically provided by the `__FILE__` macro.
 * @param line The line number within the source code file where the error check was triggered. This is typically provided by the `__LINE__` macro.
 *
 * @note This function should generally not be called directly by user code.
 *       Instead, use the `SIT_CHECK_GL_ERROR()` macro, which automatically provides the `file` and `line` parameters.
 * @note Calling `glGetError()` clears the error flag. Therefore, if multiple OpenGL errors occur in sequence, this function (via the loop) ensures that *all* pending errors are retrieved and logged, not just the first one.
 * @warning This function overwrites the library's last error message with the *most recently processed* OpenGL error from the sequence of errors.
 *          If multiple errors are pending, only the details of the last one checked in the loop will remain in `sit_gs.last_error_msg` after the function completes. However, all errors will have been logged to `stderr` in debug builds.
 *
 * @see SIT_CHECK_GL_ERROR(), _SituationSetErrorFromCode(), SituationGetLastErrorMsg()
 */
SITAPI void _SituationLogGLError(const char* file, int line) {
    // --- 1. Input Validation ---
    // While internal, checking for NULL file prevents potential crashes
    // or garbage data if called incorrectly.
    if (!file) {
        file = "<unknown_file>"; // Provide a default if file name is missing
    }

    // --- 2. Poll and Process OpenGL Errors ---
    GLenum err;
    // Loop as long as glGetError reports an error (not GL_NO_ERROR).
    // This handles cases where multiple errors might be queued.
    while ((err = glGetError()) != GL_NO_ERROR) {
        // --- 3. Translate Error Code to String ---
        const char* err_str = "UNKNOWN_ERROR";
        // Map the GLenum error code to a descriptive string.
        switch (err) {
            case GL_INVALID_ENUM:                  err_str = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 err_str = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             err_str = "GL_INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                err_str = "GL_STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               err_str = "GL_STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 err_str = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: err_str = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            // Note: Other error codes like GL_CONTEXT_LOST are part of newer
            // OpenGL versions/profiles and could be added if needed.
        }

        // --- 4. Format Detailed Error Message ---
        // Create a comprehensive message including the error, location, and code.
        // Use a sufficiently large buffer to hold the formatted string.
        char detail[SITUATION_MAX_ERROR_MSG_LEN]; // Use the library's defined max length Use snprintf for safer string formatting, preventing buffer overflows.
        int written = snprintf(
            detail,
            sizeof(detail),
            "OpenGL Error (%s:%d): %s (0x%X)",
            file,
            line,
            err_str,
            (unsigned int)err // Cast to unsigned int for consistent formatting
        );

        // --- 5. Handle Formatting Errors ---
        // Check if snprintf truncated the output or failed.
        if (written < 0 || (size_t)written >= sizeof(detail)) {
            // If snprintf failed or truncated, provide a fallback message.
            snprintf(
                detail,
                sizeof(detail),
                "OpenGL Error (%s:%d): Error formatting failed, original code was 0x%X",
                file,
                line,
                (unsigned int)err
            );
        }

        // --- 6. Store Error in Global State ---
        // Update the library's global error state with the detailed message.
        // This makes the error retrievable via SituationGetLastErrorMsg().
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, detail);

        // --- 7. Debug Output ---
        // In debug builds, print the error immediately to stderr for visibility.
#ifndef NDEBUG
        fprintf(stderr, "[DEBUG] %s\n", sit_gs.last_error_msg);
#endif
        // Note: In release builds, the error is still stored in sit_gs.last_error_msg but not printed to stderr automatically by this function.
    }
    // --- 8. Loop Exit ---
    // The loop exits when glGetError() returns GL_NO_ERROR, indicating the OpenGL error queue is now empty (or was empty to begin with).
}
#endif



static inline bool _SitShouldEngageBackpressure(void) {
    if (sit_gs.target_frame_time > 0.0) {
        return true;
    }
    return (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0;
}

static inline bool _SitShouldEngageRenderQueueBackpressure(void) {
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        return true;
    }
#endif
    return _SitShouldEngageBackpressure();
}

static void _SituationRecomputePacedFramesInFlight(void) {
    if (_SitShouldEngageBackpressure()) {
        sit_render.paced_frames_in_flight = 2;
    } else {
        sit_render.paced_frames_in_flight = SITUATION_MAX_FRAMES_IN_FLIGHT;
    }
    atomic_store(&sit_render.metric_max_latency_ns, 0);
    atomic_store(&sit_render.metric_latency_sum_ns, 0);
    atomic_store(&sit_render.metric_latency_count, 0);
    atomic_store(&sit_render.metric_window_frame_count, 0);
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    sit_render.fps_monotonic_window_start_ns = 0;
    atomic_store(&sit_render.fps_present_counter, 0);
#endif
}

static inline int _SituationEffectiveQueueDepthLimit(void) {
    int n = sit_render.paced_frames_in_flight;
    if (n < 1) {
        n = 1;
    }
    if (n > SITUATION_MAX_FRAMES_IN_FLIGHT) {
        n = SITUATION_MAX_FRAMES_IN_FLIGHT;
    }
    return n;
}

#if defined(SITUATION_ENABLE_RENDER_THREAD)

static int _SituationRoundDisplayFps(int raw_fps) {
    if (raw_fps <= 0) {
        return raw_fps;
    }
    if (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) {
        float refresh_hz = SituationGetMonitorRefreshRateHz(0);
        if (refresh_hz > 0.0f) {
            int rounded_refresh_hz = (int)(refresh_hz + 0.5f);
            int diff = raw_fps - rounded_refresh_hz;
            if (diff < 0) {
                diff = -diff;
            }
            if (diff < 1) {
                return rounded_refresh_hz;
            }
        }
    }
    return raw_fps;
}

static void _SituationPublishPresentTimingFromRenderThread(void) {
    uint64_t now_ns = _SitGetMonotonicTimeNS();
    uint64_t prev_ns = atomic_exchange_explicit(
        &sit_render.last_present_complete_time_ns, now_ns, memory_order_acq_rel);
    if (prev_ns != 0) {
        atomic_store_explicit(
            &sit_render.latest_present_delta_ns, now_ns - prev_ns, memory_order_release);
        atomic_fetch_add_explicit(&sit_render.present_timing_seq, 1u, memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&sit_render.fps_present_counter, 1u, memory_order_relaxed);
}

static void _SituationApplyPresentTimingDirect(void) {
    uint64_t now_ns = _SitGetMonotonicTimeNS();
    uint64_t prev_ns = sit_render.st_last_present_complete_time_ns;
    if (prev_ns != 0) {
        sit_gs.previous_time = sit_gs.current_time;
        sit_gs.frame_time = (double)(now_ns - prev_ns) / 1e9;
        sit_gs.current_time = sit_gs.previous_time + sit_gs.frame_time;
    } else {
        sit_gs.frame_time = 0.0;
    }
    sit_render.st_last_present_complete_time_ns = now_ns;
}

static void _SituationConsumePresentTimingOnMain(void) {
    if (!sit_render.enabled) {
        return;
    }
    uint32_t seq = atomic_load_explicit(&sit_render.present_timing_seq, memory_order_acquire);
    if (seq == 0 || seq == sit_render.last_consumed_present_seq) {
        return;
    }
    sit_render.last_consumed_present_seq = seq;
    uint64_t delta_ns = atomic_load_explicit(&sit_render.latest_present_delta_ns, memory_order_acquire);
    sit_gs.previous_time = sit_gs.current_time;
    sit_gs.frame_time = (double)delta_ns / 1e9;
    sit_gs.current_time = sit_gs.previous_time + sit_gs.frame_time;
}

static void _SituationUpdateFpsCounter(void) {
    if (sit_render.enabled) {
        uint64_t now_ns = _SitGetMonotonicTimeNS();
        if (sit_render.fps_monotonic_window_start_ns == 0) {
            sit_render.fps_monotonic_window_start_ns = now_ns;
        }
        uint32_t presents = (uint32_t)atomic_exchange_explicit(
            &sit_render.fps_present_counter, 0u, memory_order_acq_rel);
        sit_gs.fps_frame_counter += (int)presents;
        double elapsed = (double)(now_ns - sit_render.fps_monotonic_window_start_ns) / 1e9;
        if (elapsed >= 1.0) {
            int raw_fps = (int)((double)sit_gs.fps_frame_counter / elapsed + 0.5);
            sit_gs.current_fps = _SituationRoundDisplayFps(raw_fps);
            sit_gs.fps_frame_counter = 0;
            sit_render.fps_monotonic_window_start_ns = now_ns;
            sit_gs.fps_last_update_time = glfwGetTime();
        }
        return;
    }

    sit_gs.fps_frame_counter++;
    double current_time = glfwGetTime();
    double time_since_last_fps_update = current_time - sit_gs.fps_last_update_time;
    if (time_since_last_fps_update >= 1.0) {
        int raw_fps = (int)((double)sit_gs.fps_frame_counter / time_since_last_fps_update + 0.5);
        sit_gs.current_fps = _SituationRoundDisplayFps(raw_fps);
        sit_gs.fps_frame_counter = 0;
        sit_gs.fps_last_update_time = glfwGetTime();
    }
}

#endif

#endif // SITUATION_IMPL_RENDERER_CORE_H
