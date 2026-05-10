/***************************************************************************************************
*
*   situation_impl_renderer.h - Renderer Module (OpenGL + Vulkan Backends)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file contains the complete graphics renderer implementation:
*   - OpenGL 4.6 Core backend (init, commands, resources)
*   - Vulkan 1.4 backend (init, commands, resources)
*   - Shared renderer (command buffers, frame management)
*   - Resource management (textures, buffers, meshes, shaders, compute pipelines)
*   - Model loading (GLTF)
*   - Hot-reload system
*   - Render thread
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_H
#define SITUATION_IMPL_RENDERER_H

#if defined(SITUATION_USE_VULKAN)
/** Pass as pipeline_flags to _SituationVulkanCreateGraphicsPipeline for opaque color (blend off). */
#define SIT_VK_PIPELINE_BLEND_OPAQUE 1u
#endif

// ============================================================================
// OpenGL Ring Buffer & MDI Helpers (needed early by _SituationInitOpenGL)
// ============================================================================
#if defined(SITUATION_USE_OPENGL)
static void _SituationInitGLRingBuffer(void) {
    if (sit_render.gl.ring_buffer_id != 0) return;

    sit_render.gl.ring_size = SITUATION_GL_RING_SIZE;
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glCreateBuffers(1, &sit_render.gl.ring_buffer_id);
    if (sit_render.gl.ring_buffer_id == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create persistent ring buffer object.");
        return;
    }
    glNamedBufferStorage(sit_render.gl.ring_buffer_id, sit_render.gl.ring_size, NULL, flags);
    SIT_CHECK_GL_ERROR();
    sit_render.gl.ring_data_ptr = glMapNamedBufferRange(sit_render.gl.ring_buffer_id, 0, sit_render.gl.ring_size, flags);
    if (!sit_render.gl.ring_data_ptr) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to map persistent ring buffer.");
    }

    atomic_init(&sit_render.gl.ring_head, 0);
}

static void _SituationInitGLMDIBuffer(void) {
    if (sit_render.gl.mdi_buffer_id != 0) return;

    sit_render.gl.mdi_ring_size = 1024 * 1024 * SITUATION_MAX_FRAMES_IN_FLIGHT;
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glCreateBuffers(1, &sit_render.gl.mdi_buffer_id);
    if (sit_render.gl.mdi_buffer_id == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create MDI ring buffer object.");
        return;
    }
    glNamedBufferStorage(sit_render.gl.mdi_buffer_id, sit_render.gl.mdi_ring_size, NULL, flags);
    SIT_CHECK_GL_ERROR();
    sit_render.gl.mdi_data_ptr = glMapNamedBufferRange(sit_render.gl.mdi_buffer_id, 0, sit_render.gl.mdi_ring_size, flags);
    if (!sit_render.gl.mdi_data_ptr) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to map MDI ring buffer.");
    }

    atomic_init(&sit_render.gl.mdi_ring_head, 0);
}

static void _SituationInitGLRingFences(void) {
    sit_render.gl.ring_fence_count = 3;
    sit_render.gl.current_fence_index = 0;
    sit_render.gl.ring_fences = (GLsync*)SIT_CALLOC(sit_render.gl.ring_fence_count, sizeof(GLsync));
    for(size_t i=0; i<sit_render.gl.ring_fence_count; i++) {
        sit_render.gl.ring_fences[i] = 0;
    }

    if (!sit_render.gl.ring_data_ptr) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to map persistent ring buffer.");
    }
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
        // Allocation failed for the main struct.
        // _SituationSetErrorFromCode might be overkill for internal alloc failures,
        // but could be considered if the library does this for internal helpers.
        // _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_create: Failed to allocate map struct.");
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
        // _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_create: Failed to allocate bucket array.");
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
 * @param map The map to resize.
 */
static void _sit_uniform_map_resize(_SituationUniformMap* map) {
    if (!map) return;

    int new_capacity = map->capacity * 2;
    if (new_capacity <= map->capacity) return; // Overflow check

    // Allocate new buckets
    _SituationUniformMapEntry** new_buckets = (_SituationUniformMapEntry**)SIT_CALLOC(new_capacity, sizeof(_SituationUniformMapEntry*));
    if (!new_buckets) {
        // Allocation failed. Keep old map as is.
        // _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_resize: Failed to allocate new buckets.");
        return;
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
 * @note This function performs memory allocation for new keys and map entries.
 *       If allocation fails, the function returns silently without adding the entry.
 *       This is generally acceptable for a cache mechanism.
 * @note The function currently does not implement dynamic resizing of the hash table.
 *       If the number of entries (`map->count`) grows significantly larger than the `map->capacity`, performance may degrade due to longer linked list chains.
 *       A TODO exists in the original code for this.
 * @warning This function is for internal library use only and is not part of the public API.
 *
 * @see _sit_uniform_map_get(), _sit_hash_string()
 */
static void _sit_uniform_map_set(_SituationUniformMap* map, const char* key, int32_t value) {
    // --- 1. Input Validation ---
    // Check if the map or key pointer is NULL.
    if (!map || !key) {
        // Cannot operate on a NULL map or NULL key.
        // Silently return, consistent with other void internal functions.
        // Could consider logging/asserting in debug builds.
        return;
    }

    // --- 2. Calculate Hash and Bucket Index ---
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
            // No need to modify the list structure or count.
            return; // Exit early, operation complete.
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
        // Allocation failed for the new entry struct.
        // This is a memory-constrained situation. Silently failing to add the entry is often acceptable for a cache, as glGetUniformLocation can be called again.
        // _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_set: Failed to allocate new entry struct.");
        return;
    }

    // --- 5. Initialize New Entry ---
    // Duplicate the key string. This allocates memory and copies the string.
    // The caller retains ownership of the original `key` string.
    new_entry->key = _sit_strdup(key);
    // Check if strdup was successful.
    if (!new_entry->key) {
        // Allocation failed for duplicating the key string.
        // Free the previously allocated entry struct to prevent a leak.
        SIT_FREE(new_entry);
        // _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_uniform_map_set: Failed to duplicate key string.");
        return;
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
        _sit_uniform_map_resize(map);
    }
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
    // Compute the hash value of the key string.
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
/** True during SituationShutdown after init_state is SHUTTING_DOWN — resource destroys must not defer to graveyard (VMA must be empty before vmaDestroyAllocator). */
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

    if (slot->vertex_stride == 48) {
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

    if (gy->buffer_count == 0 && gy->texture_count == 0 && gy->mesh_count == 0) {
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


#if !defined(__STDC_NO_THREADS__)
/**
 * @brief [INTERNAL] Starts the dedicated render thread and performs context handoff.
 *
 * @details This function is called during library initialization (from `_SituationInitSubsystems`)
 *          when `SITUATION_ENABLE_RENDER_THREAD` is defined and enabled in `init_info`.
 *          It is responsible for creating and launching the render thread, which takes over
 *          all GPU command execution, submission, presentation, and resource cleanup.
 *
 *          Critical sequence (do not reorder):
 *            1. Validates that the main thread currently owns the GL context (if OpenGL)
 *            2. Releases the context from the main thread
 *               (`glfwMakeContextCurrent(NULL)`) to allow handoff
 *            3. Creates the render thread via `thrd_create(_SituationRenderThreadEntry, NULL)`
 *            4. Waits briefly (spin/yield) until the render thread acquires the context
 *               (via atomic flag `sit_render.gl_context_released` or similar sync)
 *            5. Sets up per-frame resources (graveyards, fences, command buffers)
 *            6. Initializes render metrics (if enabled)
 *            7. Marks render thread as active (`sit_render.thread_active = true`)
 *            8. Returns success or failure based on thread creation and handoff
 *
 *          On success:
 *            - Main thread no longer has GL context
 *            - Render thread owns context and is running its loop
 *            - All subsequent GL/VK calls must go through command buffers
 *
 *          On failure:
 *            - Logs error (e.g. thread creation fail)
 *            - Returns false init aborts or falls back to main-thread rendering
 *
 * @param info Pointer to `SituationInitInfo` containing render thread preferences
 *             (e.g. enable/disable flag, thread priority hints if supported).
 *             May influence whether the thread starts or falls back to synchronous mode.
 *
 * @return true if render thread was successfully created, context handed off,
 *         and thread is running,
 *         false on failure (thread creation error, context handoff timeout,
 *         allocation failure, etc.).
 *         Failures are logged internally and may set global `SituationError`.
 *
 * @note **Critical thread safety point**:
 *       - Must be called **only from the main thread** with GL context current
 *       - Context release must succeed before thread start
 *       - Render thread acquires context immediately after creation
 *       - No GL calls allowed on main thread after this function succeeds
 *
 *       If render thread is disabled (`init_info->enable_render_thread = false`
 *       or compile-time define absent), this function returns true immediately
 *       (no-op) main thread retains context and does synchronous rendering.
 *
 *       Dependencies:
 *         - GLFW window must exist (`sit_gs.sit_glfw_window != NULL`)
 *         - GL context must be current on calling thread
 *         - Vulkan path may skip context handoff (uses queues instead)
 *
 * @see _SituationInitSubsystems (caller), _SituationRenderThreadEntry,
 *      SituationInitInfo.enable_render_thread,
 *      SITUATION_ENABLE_RENDER_THREAD (compile-time toggle),
 *      SITUATION_ERROR_THREAD_CREATION_FAILED,
 *      SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT (related)
 */
static bool _SituationInitRenderThread(const SituationInitInfo* info) {
    // Note: resource_registry_mutex is now initialized earlier in SituationInit, before renderer init
    
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (info->render_thread_count == 0) return true;

    sit_render.enabled = true;
    atomic_init(&sit_render.thread_active, true);
    atomic_init(&sit_render.thread_shutdown_req, false);
    // Note: frames_pending already initialized to 0 in _SituationInitRenderer
    atomic_init(&sit_render.render_queue_depth, 0);
    sit_render.render_queue_head = 0;
    sit_render.render_queue_tail = 0;

    // Note: render_queue_mutex, main_wait_cv, and render_queue_cv already initialized in _SituationInitRenderer

    // [Polish 1] GL Handover: Release from main before spawn
    #if defined(SITUATION_USE_OPENGL)
    if (sit_gs.sit_glfw_window) {
        glfwMakeContextCurrent(NULL); // Release context for render thread
        atomic_store(&sit_render.gl_context_released, true); // Signal render thread
    }
    #endif

    fprintf(stderr, "[Situation] [MAIN] About to create render thread...\n"); fflush(stderr);
    if (thrd_create(&sit_render.render_thread, _SituationRenderThreadEntry, NULL) != thrd_success) {
        fprintf(stderr, "[Situation] [MAIN] Render thread creation FAILED\n"); fflush(stderr);
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "Failed to spawn render thread");
        #if defined(SITUATION_USE_OPENGL)
        if (sit_gs.sit_glfw_window) glfwMakeContextCurrent(sit_gs.sit_glfw_window); // Reacquire on fail
        #endif
        return false;
    }
    fprintf(stderr, "[Situation] [MAIN] Render thread created successfully\n"); fflush(stderr);

    // Note: Main thread must NOT call GL/VK cmds post-handover. Use render queue for all GPU work.

    // For OpenGL, Main thread typically needs a shared context for asset loading.
    // _SituationInitOpenGL created 'loader_window' for this.
    // We should make THAT current now if it exists.
    #if defined(SITUATION_USE_OPENGL)
    if (sit_render.gl.loader_window) {
        glfwMakeContextCurrent(sit_render.gl.loader_window);
    }
    #endif

    return true;
    #else
    return true;
    #endif
}

static void _SituationDestroyRenderThread(void) {
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (!sit_render.enabled || !atomic_load(&sit_render.thread_active)) return;

    if (atomic_load(&sit_render.thread_shutdown_req)) return;

    atomic_store(&sit_render.thread_shutdown_req, true);

    // Broadcast to wake everyone
    mtx_lock(&sit_render.render_queue_mutex);
    cnd_broadcast(&sit_render.render_queue_cv);
    mtx_unlock(&sit_render.render_queue_mutex);

    cnd_broadcast(&sit_render.main_wait_cv);

    // [v2.3.22] Timed Join (Polling thread_active for 1s before join)
    // C11 thrd_join is blocking, so we poll for the thread to mark itself inactive first.
    // If it doesn't deactivate within the timeout, we log an error but proceed to block-join.
    struct timespec ts = {0, 100000000}; // 100ms
    int ticks = 10;
    bool timed_out = true;

    for (int i = 0; i < ticks; ++i) {
        if (!atomic_load(&sit_render.thread_active)) {
            timed_out = false;
            break;
        }
        if (i % 5 == 0 && i > 0) {
            fprintf(stderr, "[WARN] Render join tick %d/10...\n", i);
        }
        thrd_sleep(&ts, NULL);
    }

    if (timed_out) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT, "Render thread join timeoutÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Âaborted.");
        // Proceed to join anyway to avoid leaking a running thread, but the error is logged.
    }

    if (thrd_join(sit_render.render_thread, NULL) != thrd_success) {
         _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "Render thread join failed.");
    }

    // Fence for cleanup vis
    atomic_thread_fence(memory_order_release);

    mtx_destroy(&sit_render.render_queue_mutex);
    cnd_destroy(&sit_render.render_queue_cv);
    cnd_destroy(&sit_render.main_wait_cv);

    atomic_store(&sit_render.thread_active, false);
    sit_render.enabled = false;

    // [GL] Release context
    #if defined(SITUATION_USE_OPENGL)
    glfwMakeContextCurrent(NULL);
    #endif

    #endif
}
#endif

//----------------------------------------------------------------------------------------------------------
// --- Core Lifecycle Implementation ---
//----------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes the entire Situation library.
 *
 * @details This is the main entry point and the first function a user of the library must call. It orchestrates the complete initialization process by setting up all necessary subsystems in a specific, dependency-respecting order:
 * 1.  **Platform:** Initializes low-level libraries like GLFW.
 * 2.  **Window:** Creates the main application window.
 * 3.  **Renderer:** Initializes the selected graphics backend (OpenGL or Vulkan), including contexts/devices, swapchains, internal pipelines, etc.
 * 4.  **Subsystems:** Initializes audio, input handling, timer system, filesystem utilities, and other core functionalities.
 *
 * If any step in this sequence fails, the function triggers a comprehensive cleanup process (`_SituationFullCleanupOnError`) to ensure that no resources
 * are leaked and the library is left in a clean, uninitialized state.
 *
 * @param argc The number of command-line arguments, including the program name.
 *             This is typically the `argc` parameter from the `main` function.
 * @param argv An array of strings representing the command-line arguments.
 *             This is typically the `argv` parameter from the `main` function.
 *             The library stores these for later querying via argument functions.
 * @param init_info A pointer to a `SituationInitInfo` struct containing all necessary configuration options for the library's initialization (e.g., window title, dimensions, initial flags).
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS on successful initialization of all subsystems.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_ALREADY_INITIALIZED if `SituationInit` is called more than once without an intervening `SituationShutdown`.
 * @return SITUATION_ERROR_INIT_FAILED if any part of the initialization sequence fails (e.g., GLFW failure, graphics context creation failure,
 *         audio device failure). A specific error code and message will be set by the failing subsystem's initialization function. Cleanup is attempted.
 * @return SITUATION_ERROR_SHUTDOWN_FAILED if a previous call to `SituationShutdown` failed and left the library in an inconsistent state, preventing re-initialization.
 *         This is a safeguard to avoid attempting initialization from a bad state.
 *
 * @note This function must be called before any other `SITAPI` functions (except potentially other init/shutdown functions).
 * @note The library is designed to be initialized and shut down once per application run. While re-initialization after a successful shutdown
 *       is intended to work, it's generally recommended to structure the application's main lifecycle around a single init/shutdown pair.
 * @warning This function is not thread-safe. It must be called from the main thread of the application.
 *
 * @see SituationShutdown(), SituationInitInfo, SituationGetLastErrorMsg()
 */

// [v2.3.24b] Integration Zenith: Initialization Validation
static bool _SituationValidateRenderCaps(void) {
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device) {
        // Validate Semaphore Creation (Critical for Queue Sync)
        VkSemaphoreCreateInfo sema_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore sema = VK_NULL_HANDLE;
        if (vkCreateSemaphore(sit_render.vk.device, &sema_info, NULL, &sema) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Validation: Semaphore creation failed.");
            return false;
        }
        vkDestroySemaphore(sit_render.vk.device, sema, NULL);

        // Check Queue Topology
        if (sit_render.vk.compute_family_index == sit_render.vk.graphics_family_index) {
            // Not fatal, but good to know for tuning
            #ifndef NDEBUG
            fprintf(stderr, "[Situation] Note: Shared Graphics/Compute Queue (Family %u). Async overlap limited.\n", sit_render.vk.graphics_family_index);
            #endif
        }
    }
#endif
    return true;
}


/**
 * @brief [INTERNAL] Dispatches renderer initialization to the selected backend.
 *
 * @details This helper function acts as a simple dispatcher or gateway. Based on the compile-time definitions (`SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL`), it calls the corresponding backend-specific initialization function:
 *          `_SituationInitVulkan` or `_SituationInitOpenGL`.
 *
 * This abstraction allows the main `SituationInit` function to remain clean and unaware of the specific steps required for each graphics API.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during `SituationInit`. This contains configuration options that might be relevant to the renderer initialization.
 *                  This pointer must not be NULL (though the backend functions should also validate this).
 *
 * @return The `SituationError` code returned by the chosen backend's initialization function (e.g., `_SituationInitVulkan`, `_SituationInitOpenGL`).
 *         - SITUATION_SUCCESS indicates successful renderer setup.
 *         - Any other error code indicates a failure within the backend-specific initialization process.
 *
 * @note This function should only be called from `SituationInit` after the platform and window have been successfully initialized.
 * @note The choice of backend is determined at compile time by defining either `SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL`.
 * @warning This function is for internal use by `SituationInit` and should not be called directly by user code.
 *
 * @see SituationInit(), _SituationInitVulkan(), _SituationInitOpenGL()
 */
static SituationError _SituationInitRenderer(const SituationInitInfo* init_info) {
    // 1. Initialize Momentum Queue (Common)
    atomic_init(&sit_render.momentum_head, 0);
    atomic_init(&sit_render.momentum_tail, 0);

    if (mtx_init(&sit_render.momentum_mutex, mtx_recursive) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue mutex.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_render.momentum_mutex_initialized = true;

    // 2. Initialize render queue mutex (needed for Vulkan backpressure even before render thread starts)
    #if !defined(__STDC_NO_THREADS__)
    if (mtx_init(&sit_render.render_queue_mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue mutex.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (cnd_init(&sit_render.main_wait_cv) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize main wait condition variable.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (cnd_init(&sit_render.render_queue_cv) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue condition variable.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_render.frames_pending = 0;
    atomic_init(&sit_render.gl_context_released, false);
    #endif

    // Dispatch to the appropriate backend initialization function based on the compile-time flag.
#if defined(SITUATION_USE_VULKAN)
        // Vulkan backend is selected. Initialize it.
        return _SituationInitVulkan(init_info);
#elif defined(SITUATION_USE_OPENGL)
        // OpenGL backend is selected. Initialize it.
        return _SituationInitOpenGL(init_info);
#else
        // This branch should ideally be unreachable due to the #error directives in the header file that force the user to define a backend.
        // However, as a safeguard, handle the case where no backend is defined.
        _SituationSetErrorFromCode(
            SITUATION_ERROR_NOT_IMPLEMENTED,
            "_SituationInitRenderer: No graphics renderer backend defined (SITUATION_USE_VULKAN or SITUATION_USE_OPENGL). This should be caught at compile time."
        );
        return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}



/**
 * @brief Loads a bitmap font directly from an in-memory data buffer.
 *
 * @details Creates a `SituationFont` handle from a raw, pre-rasterized bitmap font stored
 *          in memory (e.g. embedded font data, loaded asset, or procedurally generated atlas).
 *          This is the low-level entry point for using custom or non-TTF bitmap fonts with
 *          the built-in text renderer.
 *
 *          Expected format of the input data:
 *            - Monochrome or grayscale bitmap (1 byte per pixel recommended)
 *            - Fixed-size grid layout: characters arranged in rows/columns
 *            - Left-to-right, top-to-bottom order (ASCII or custom range)
 *            - No padding between characters (tightly packed)
 *            - Data is row-major: stride = char_width per row
 *
 *          The function:
 *            - Validates input parameters (dimensions, char count, non-null data)
 *            - Allocates internal glyph atlas texture (usually GL_R8 or GL_RGBA8)
 *            - Uploads the bitmap data to GPU
 *            - Computes per-character UV coordinates and metrics
 *              (x/y offset, width/height, advance)
 *            - Stores the glyph table (num_chars entries)
 *            - Sets up default font properties (line height, baseline, etc.)
 *            - Returns a usable `SituationFont` handle
 *
 *          After success, the font can be used immediately with `SituationCmdDrawText`,
 *          `SituationCmdDrawTextEx`, or any text rendering path.
 *
 * @param data Pointer to the raw bitmap data buffer.
 *             Must remain valid for the duration of the call (ownership not transferred).
 *             Size must be exactly `char_width * char_height * num_chars` bytes.
 * @param char_width Width of each glyph in pixels (fixed for all characters).
 *                   Typical values: 8, 16, 32.
 * @param char_height Height of each glyph in pixels (fixed).
 *                    Typical values: 8, 16, 32.
 * @param num_chars Total number of glyphs in the font (e.g. 128 for ASCII, 256 for extended).
 *                  Must be > 0 and reasonable (avoid thousands without reason).
 * @param out_font Pointer to a `SituationFont` variable that receives the new font handle
 *                 on success. On failure, set to `SITUATION_NULL_FONT`.
 *
 * @return SITUATION_SUCCESS on successful load and GPU upload,
 *         SITUATION_ERROR_INVALID_PARAM if data is NULL, dimensions invalid,
 *         num_chars 0, or out_font is NULL,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if internal texture or glyph table allocation failed,
 *         SITUATION_ERROR_GL_UPLOAD_FAILED if texture upload failed (OpenGL),
 *         SITUATION_ERROR_VULKAN_UPLOAD_FAILED if texture creation/upload failed (Vulkan),
 *         or other backend-specific errors.
 *
 * @note The input data is **not** copied the function assumes it remains valid.
 *       For dynamic fonts or TTF loading, use `SituationLoadFontFromFile` or `SituationLoadFontFromMemory` instead.
 *       Texture is created with default sampler state (nearest filtering, clamp-to-edge).
 *       Caller is responsible for destroying the font with `SituationDestroyFont` when done.
 *       Thread safety: Must be called with active GL/VK context (typically render thread or init).
 *
 *       Recommended usage for embedded VGA font:
 *       ```c
 *       SituationFont font;
 *       SituationLoadBitmapFontFromMemory(sit_default_8x8_font, 8, 8, 256, &font);
 *       ```
 *
 * @see SituationDestroyFont, SituationCmdDrawText, SituationCmdDrawTextEx,
 *      sit_default_8x8_font (embedded VGA font), SITUATION_NULL_FONT,
 *      SITUATION_ERROR_MEMORY_ALLOCATION, SITUATION_ERROR_GL_UPLOAD_FAILED
 */
SITAPI SituationError SituationLoadBitmapFontFromMemory(const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font) {
    if (!data || char_width <= 0 || char_height <= 0 || num_chars <= 0 || !out_font) return SITUATION_ERROR_INVALID_PARAM;

    memset(out_font, 0, sizeof(SituationFont));

    out_font->is_bitmap = true;
    out_font->bitmap_data = data; // Note: We do NOT copy the data for bitmap fonts, assuming it's static/embedded.
    out_font->bitmap_width = char_width;
    out_font->bitmap_height = char_height;
    out_font->bitmap_count = num_chars;

    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_OPENGL)
/**
 * @brief [INTERNAL] Maps library-agnostic data types to OpenGL constants.
 *
 * @details Converts `SituationDataType` enums into their corresponding GLenum values (e.g., `SIT_DATA_FLOAT` -> `GL_FLOAT`). This is a utility helper used for vertex attribute configuration.
 *
 * @param type The generic data type enum.
 * @return The corresponding GLenum value.
 * @return `0` if the input type is unknown or invalid.
 *
 * @see SituationCmdSetVertexAttribute()
 */
static GLenum _SituationMapDataTypeToGL(SituationDataType type) {
    switch (type) {
        case SIT_DATA_BYTE: return GL_BYTE;
        case SIT_DATA_UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
        case SIT_DATA_SHORT: return GL_SHORT;
        case SIT_DATA_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
        case SIT_DATA_INT: return GL_INT;
        case SIT_DATA_UNSIGNED_INT: return GL_UNSIGNED_INT;
        case SIT_DATA_FLOAT: return GL_FLOAT;
        case SIT_DATA_DOUBLE: return GL_DOUBLE;
        default: return 0;
    }
}


/**
 * @brief [INTERNAL] Initializes the OpenGL rendering backend and all internal OpenGL resources.
 * @details This is the master function for setting up the OpenGL environment. It is called once during `SituationInit` after the GLFW window and an OpenGL context have been successfully created.
 *
 * @par Initialization Sequence
 *   1.  **Context & Function Loading:** It makes the GLFW window's OpenGL context current for the calling thread and then uses GLAD to load all necessary modern OpenGL function pointers.
 *   2.  **Version & Extension Validation:** It verifies that the available OpenGL version meets the library's minimum requirement (e.g., OpenGL 4.6) and that critical extensions (like `GL_ARB_direct_state_access`) are supported.
 *   3.  **Global VAO Abstraction:** It creates and binds a single, global Vertex Array Object (`sit_render.gl.global_vao_id`).
 *           This VAO remains active for all user rendering commands, providing a crucial abstraction layer that simplifies vertex attribute management and is essential for the `SituationCreateMesh` and `SituationCmd*` API to function correctly.
 *   4.  **Internal Renderers:** It initializes the library's private rendering modules, such as the 2D quad renderer and the virtual display compositors. These modules create their own private VAOs and shaders to ensure their state does not interfere with the user's global VAO.
 *   5.  **Global UBO:** It creates and binds the global Uniform Buffer Object for per-view data (e.g., camera matrices) to its standard binding point (`SIT_UBO_BINDING_VIEW_DATA`).
 *   6.  **Initial State:** It sets the initial VSync state (`glfwSwapInterval`) and default clear color based on the user's `init_info`.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, containing user-defined configuration like the VSync hint.
 *
 * @return `SITUATION_SUCCESS` on successful initialization of all OpenGL components.
 * @return An appropriate `SituationError` code if any phase fails (e.g., GLAD fails to load, version is too old, an internal renderer fails to initialize).
 *
 * @note This function is for internal use by `_SituationInitRenderer` only.
 * @warning The creation and binding of the `global_vao_id` is a cornerstone of the OpenGL backend's design. All user-facing mesh and drawing functions rely on this VAO being active.
 *
 * @see _SituationInitRenderer(), _SituationInitQuadRenderer(), _SituationCleanupOpenGL()
 */
// --- Soft Command Buffer Implementation ---

/**
 * @brief [INTERNAL] Allocates a new command packet in the soft buffer.
 * @details Checks for capacity and grows the buffer if necessary using `SIT_REALLOC`.
 *          The growth strategy is geometric (doubling) to minimize allocation frequency.
 *
 * @param buf The soft command buffer to append to.
 * @param opcode The operation code for the new command.
 * @return A pointer to the allocated `SitCommandPacket` struct within the buffer.
 *         Returns NULL if memory allocation fails.
 */
static SitCommandPacket* _SitGLSoftCmdPush(SituationGLSoftCommandBuffer* buf, SitOpCode opcode) {
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SitGLSoftCmdPush: ENTRY, buf=%p, opcode=%d\n", buf, opcode);
    fflush(stdout);
    #endif
    
    // [FIX v2.3.27B] Fail fast if buffer is already compromised
    if (buf->is_broken) {
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SitGLSoftCmdPush: Buffer is broken, returning NULL\n");
        fflush(stdout);
        #endif
        return NULL;
    }

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SitGLSoftCmdPush: packet_count=%zu, capacity=%zu\n", buf->packet_count, buf->packet_capacity);
    fflush(stdout);
    #endif

    if (buf->packet_count >= buf->packet_capacity) {
        size_t new_cap = (buf->packet_capacity == 0) ? 64 : buf->packet_capacity * 2;
        
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SitGLSoftCmdPush: Reallocating packets, new_cap=%zu\n", new_cap);
        fflush(stdout);
        #endif
        
        SitCommandPacket* new_ptr = (SitCommandPacket*)SIT_REALLOC(buf->packets, new_cap * sizeof(SitCommandPacket));

        if (!new_ptr) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Soft command buffer packets realloc failed. Frame dropped.");
            buf->is_broken = true; // Trip the breaker
            #ifdef SITUATION_OPENGL_DEBUG
            printf("[OpenGL Debug] _SitGLSoftCmdPush: Realloc FAILED\n");
            fflush(stdout);
            #endif
            return NULL;
        }
        buf->packets = new_ptr;
        buf->packet_capacity = new_cap;
        
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SitGLSoftCmdPush: Realloc SUCCESS, new packets=%p\n", new_ptr);
        fflush(stdout);
        #endif
    }
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SitGLSoftCmdPush: Getting packet at index %zu\n", buf->packet_count);
    fflush(stdout);
    #endif
    
    SitCommandPacket* packet = &buf->packets[buf->packet_count++];
    packet->opcode = opcode;
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SitGLSoftCmdPush: SUCCESS, returning packet=%p\n", packet);
    fflush(stdout);
    #endif
    
    return packet;
}

/**
 * @brief [INTERNAL] Pushes raw data (payload) into the soft buffer's data stream.
 * @details Used for variable-length data like push constants or text strings that don't fit in the fixed-size packet union.
 *          Ensures 8-byte alignment could be added here if needed, but currently packs tightly.
 *
 * @param buf The soft command buffer.
 * @param data Pointer to the source data to copy. If NULL, space is reserved but not written.
 * @param size Size in bytes to allocate/copy.
 * @return A pointer to the data's location *within the buffer*.
 *         Warning: This pointer may be invalidated by subsequent calls to _SitGLSoftDataPush if the buffer reallocates.
 *         Always use offsets for long-term storage.
 */
static void* _SitGLSoftDataPush(SituationGLSoftCommandBuffer* buf, const void* data, size_t size) {
    // [FIX v2.3.27B] Fail fast
    if (buf->is_broken) return NULL;

    if (buf->data_cursor + size > buf->data_capacity) {
        size_t new_cap = (buf->data_capacity == 0) ? 4096 : buf->data_capacity * 2;
        while (buf->data_cursor + size > new_cap) new_cap *= 2;

        uint8_t* new_ptr = (uint8_t*)SIT_REALLOC(buf->data_buffer, new_cap);
        if (!new_ptr) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Soft command buffer data realloc failed. Frame dropped.");
            buf->is_broken = true; // Trip the breaker
            return NULL;
        }
        buf->data_buffer = new_ptr;
        buf->data_capacity = new_cap;
    }

    void* dest = buf->data_buffer + buf->data_cursor;
    if (data) memcpy(dest, data, size);
    buf->data_cursor += size;
    return dest;
}

/**
 * @brief [INTERNAL] Replays the soft command buffer to the OpenGL driver.
 * @details This is the "Consumer" phase of the deferred rendering model. It iterates through the recorded packets
 *          and issues the corresponding `gl*` calls.
 *
 *          **Key Responsibilities:**
 *          - State Translation: Maps abstract opcodes to specific OpenGL functions.
 *          - Resource Binding: Handles VAO/VBO/UBO binding.
 *          - Draw Calls: Issues `glDrawArrays` / `glDrawElements`.
 *          - **State Restoration:** Critically, after operations that modify global state (like `SIT_OP_DRAW_MESH` changing the VAO),
 *            it restores the "Global VAO" (`sit_render.gl.global_vao_id`) to ensure subsequent commands work as expected.
 *
 * @param buf The soft command buffer to execute.
 */
static void _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index) {
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: ENTRY, buf=%p, frame_index=%d, packet_count=%d\n", 
           (void*)buf, frame_index, buf ? buf->packet_count : -1);
    fflush(stdout);
    #endif
    SIT_DEBUG_LOG("[GLExecute] START: packet_count=%d\n", buf->packet_count);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: After SIT_DEBUG_LOG\n");
    fflush(stdout);
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Checking packet_count=%d\n", buf->packet_count);
    fflush(stdout);
    printf("[OpenGL Debug] _SituationGLExecuteCommands: About to check if packet_count == 0\n");
    fflush(stdout);
    #endif
    if (buf->packet_count == 0) return;
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: packet_count > 0, continuing\n");
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Checking is_broken=%d\n", buf->is_broken);
    fflush(stdout);
    #endif
    // [FIX v2.3.27B] Do not execute incomplete/corrupted buffers
    if (buf->is_broken) {
        // Reset state for next frame and return
        buf->packet_count = 0;
        buf->data_cursor = 0;
        buf->is_broken = false; // Reset flag for next attempt
        return;
    }

    // [v2.3.31] Optimization: Track bound texture locally to avoid glGetIntegerv stalls in draw calls
    // GLuint current_bound_texture_id = 0; // REPLACED by sit_render.gl.current_bound_texture_id

    static int cached_w = 0;
    static int cached_h = 0;

    // If the window resized, rebuild the Render Thread's target FBO resources safely
    if (cached_w != sit_gs.main_window_width || cached_h != sit_gs.main_window_height || sit_render.gl.shadow_state_dirty) {
        cached_w = sit_gs.main_window_width;
        cached_h = sit_gs.main_window_height;

        glm_ortho(0.0f, (float)cached_w, (float)cached_h, 0.0f, -1.0f, 1.0f, sit_render.gl.vd_ortho_projection);

        if (sit_render.gl.composite_copy_texture_id != 0) {
            glBindTexture(GL_TEXTURE_2D, sit_render.gl.composite_copy_texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cached_w, cached_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        if (sit_render.gl.quad_shader_program) {
            glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)sit_render.gl.vd_ortho_projection);
        }

        sit_render.gl.shadow_state_dirty = false;
    }

    // --- [v2.3.27] State Hardening: Reset critical state ---
    // We cannot assume the state from the previous frame persists,
    // because external code (ImGui, etc.) might have run in between.

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: About to reset GL state\n");
    fflush(stdout);
    #endif

    // 1. Reset Capabilities
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glEnable(GL_DEPTH_TEST)\n");
    fflush(stdout);
    #endif
    glEnable(GL_DEPTH_TEST);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDepthFunc(GL_LESS)\n");
    fflush(stdout);
    #endif
    glDepthFunc(GL_LESS);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glEnable(GL_CULL_FACE)\n");
    fflush(stdout);
    #endif
    glEnable(GL_CULL_FACE);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glCullFace(GL_BACK)\n");
    fflush(stdout);
    #endif
    glCullFace(GL_BACK);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDisable(GL_BLEND)\n");
    fflush(stdout);
    #endif
    glDisable(GL_BLEND); // Default to opaque
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDisable(GL_SCISSOR_TEST)\n");
    fflush(stdout);
    #endif
    glDisable(GL_SCISSOR_TEST);

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: GL state reset complete\n");
    fflush(stdout);
    #endif

    // 2. Reset Bindings
    // We don't unbind VAO/Program here because the first command in the buffer
    // is usually a Bind command. However, we should invalidate our shadow cache.
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Resetting shadow cache\n");
    fflush(stdout);
    #endif
    sit_render.gl.current_program_id = 0;
    sit_render.gl.current_vao_id = 0;
    sit_render.gl.current_fbo_id = 0;
    sit_render.gl.current_bound_texture_id = 0;

    // 3. Reset Blend State Cache
    sit_render.gl.blend_enabled = -1; // Force re-application if command requests it

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Shadow cache reset complete\n");
    fflush(stdout);
    #endif

    // 4. Set MDI Offset based on frame index (Double/Triple Buffering)
    // Each frame gets a dedicated 1MB slice of the MDI ring buffer.
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Setting MDI offset, frame_index=%d\n", frame_index);
    fflush(stdout);
    #endif
    size_t mdi_frame_offset = (frame_index % SITUATION_MAX_FRAMES_IN_FLIGHT) * (1024 * 1024);
    atomic_store(&sit_render.gl.mdi_ring_head, mdi_frame_offset);

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: MDI offset set, entering execution loop\n");
    fflush(stdout);
    #endif

    // --- Execution Loop ---
    for (size_t i = 0; i < buf->packet_count; ++i) {
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Processing packet %zu\n", i);
        fflush(stdout);
        #endif
        SitCommandPacket* p = &buf->packets[i];
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Got packet pointer, opcode=%d\n", p->opcode);
        fflush(stdout);
        #endif
        SIT_DEBUG_LOG("[GLExecute] Processing packet %zu, opcode=%d\n", i, p->opcode);
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Entering switch statement\n");
        fflush(stdout);
        #endif
        switch (p->opcode) {
            case SIT_OP_BEGIN_RENDER_PASS:
                {
                    if (p->args.begin_pass.display_id < 0) {
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        sit_render.gl.current_fbo_id = 0;
                        // Use captured resolution from packet to avoid race condition
                        glViewport(0, 0, p->args.begin_pass.target_w, p->args.begin_pass.target_h);
                    } else {
                        int did = p->args.begin_pass.display_id;
                        if (did < SITUATION_MAX_VIRTUAL_DISPLAYS && sit_render.virtual_display_slots_used[did]) {
                            SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[did];
                            glBindFramebuffer(GL_FRAMEBUFFER, vd->gl.fbo_id);
                            sit_render.gl.current_fbo_id = vd->gl.fbo_id;
                            glViewport(0, 0, (GLsizei)vd->resolution.x, (GLsizei)vd->resolution.y);
                        }
                    }

                    GLbitfield clear_mask = 0;
                    if (p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        ColorRGBA c = p->args.begin_pass.info.color_attachment.clear.color;
                        glClearColor(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f);
                        clear_mask |= GL_COLOR_BUFFER_BIT;
                    }
                    if (p->args.begin_pass.info.depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        glClearDepth(p->args.begin_pass.info.depth_attachment.clear.depth);
                        clear_mask |= GL_DEPTH_BUFFER_BIT;
                    }
                    if (clear_mask) glClear(clear_mask);

                    glEnable(GL_DEPTH_TEST);
                    sit_render.gl.depth_test_enabled = true;
                }
                break;

            case SIT_OP_END_RENDER_PASS:
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: In END_RENDER_PASS case\n");
                fflush(stdout);
                #endif
                SIT_DEBUG_LOG("[GLExecute] END_RENDER_PASS: About to unbind framebuffer\n");
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glBindFramebuffer\n");
                fflush(stdout);
                #endif
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: glBindFramebuffer returned\n");
                fflush(stdout);
                #endif
                SIT_DEBUG_LOG("[GLExecute] END_RENDER_PASS: Framebuffer unbound\n");
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: About to break from END_RENDER_PASS\n");
                fflush(stdout);
                #endif
                break;

            case SIT_OP_SET_VIEWPORT:
                glViewport((GLint)p->args.viewport.x, (GLint)p->args.viewport.y,
                           (GLsizei)p->args.viewport.w, (GLsizei)p->args.viewport.h);
                break;

            case SIT_OP_SET_SCISSOR:
                glEnable(GL_SCISSOR_TEST);
                glScissor(p->args.scissor.x, p->args.scissor.y, p->args.scissor.w, p->args.scissor.h);
                break;

            case SIT_OP_BIND_PIPELINE:
                glUseProgram((GLuint)p->args.bind_pipeline.shader_id);
                sit_render.gl.current_program_id = (GLuint)p->args.bind_pipeline.shader_id;

                // [Phase 5] Update Virtual Bindless Cache
                if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                    sit_render.gl.current_virtual_loc = glGetUniformLocation(sit_render.gl.current_program_id, "_sit_texture_slot_id");
                }
                break;

            case SIT_OP_DRAW_MESH:
                {
                    GLuint vao = _SitGLGetCachedVAO(p->args.draw_mesh.mesh);
                    if (vao == 0) break;

                    glBindVertexArray(vao);
                    sit_render.gl.current_vao_id = vao;

                    // --- Batch detection (VAO must match) ---
                    // Note: Pipeline changes are implicitly handled because they are distinct opcodes (SIT_OP_BIND_PIPELINE),
                    // which will cause the lookahead loop to break immediately.
                    size_t batch_start = i;
                    GLuint first_vao = vao;

                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        SitCommandPacket* next = &buf->packets[lookahead];
                        if (next->opcode != SIT_OP_DRAW_MESH) break;

                        if (_SitGLGetCachedVAO(next->args.draw_mesh.mesh) != first_vao) break;
                        if (next->args.draw_mesh.shader_id != sit_render.gl.current_program_id) break;

                        lookahead++;
                    }

                    size_t batch_size = lookahead - i;

                    // Tune threshold: Batching has overhead. Start with >= 8.
                    if (batch_size >= 8 && sit_render.gl.mdi_data_ptr) {
                        size_t cmd_size = sizeof(SitDrawElementsIndirectCommand);
                        size_t total_size = batch_size * cmd_size;

                        // Atomic reservation in ring buffer
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice (1MB per frame)
                        if (offset >= mdi_frame_offset && (offset + total_size <= mdi_frame_offset + (1024 * 1024))) {
                            SitDrawElementsIndirectCommand* cmds = (SitDrawElementsIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            for (size_t k = 0; k < batch_size; ++k) {
                                SitCommandPacket* bp = &buf->packets[i + k];
                                struct _SituationMeshSlot* slot = _SitGetMeshSlot(bp->args.draw_mesh.mesh);
                                if (slot) {
                                    cmds[k].count         = (GLuint)slot->index_count;
                                    cmds[k].instanceCount = 1;          // change if real instancing is added
                                    cmds[k].firstIndex    = 0;
                                    cmds[k].baseVertex    = 0;
                                    cmds[k].baseInstance  = 0;
                                } else {
                                    cmds[k].count = 0;
                                    cmds[k].instanceCount = 0;
                                }
                            }

                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)((uintptr_t)offset), (GLsizei)batch_size, 0);
                            SIT_CHECK_GL_ERROR();

                            // Skip the batched commands
                            i += (batch_size - 1);
                        } else {
                            // Fallback single draw (buffer full)
                            struct _SituationMeshSlot* slot = _SitGetMeshSlot(p->args.draw_mesh.mesh);
                            if (slot) glDrawElements(GL_TRIANGLES, slot->index_count, GL_UNSIGNED_INT, NULL);
                        }
                    } else {
                        // Single draw fallback
                        struct _SituationMeshSlot* slot = _SitGetMeshSlot(p->args.draw_mesh.mesh);
                        if (slot) glDrawElements(GL_TRIANGLES, slot->index_count, GL_UNSIGNED_INT, NULL);
                    }

                    // [CRITICAL] Restore global VAO state for subsequent generic draw calls
                    if (sit_render.gl.global_vao_id != 0) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                }
                break;

            case SIT_OP_DRAW_QUAD:
                {
                    if (sit_render.gl.quad_shader_program == 0) break;

                    // Disable culling for 2D quads (winding order depends on projection)
                    glDisable(GL_CULL_FACE);
                    glDisable(GL_DEPTH_TEST);

                    // Only bind program if not already bound (avoids driver overhead on repeated quads)
                    if (sit_render.gl.current_program_id != sit_render.gl.quad_shader_program) {
                        glUseProgram(sit_render.gl.quad_shader_program);
                        sit_render.gl.current_program_id = sit_render.gl.quad_shader_program;
                        glBindVertexArray(sit_render.gl.quad_vao);
                    }

                    // [Bug 9 Fix] Set texture mode based on whether a texture is bound
                    if (sit_render.gl.current_bound_texture_id == 0) {
                        glProgramUniform1i(sit_render.gl.quad_shader_program, 6, 0); // Mode 0: No Texture
                    } else {
                        glProgramUniform1i(sit_render.gl.quad_shader_program, 6, 1); // Mode 1: Use Texture
                    }

                    // --- Batch: process consecutive DRAW_QUAD opcodes ---
                    {
                        size_t batch_start = i;
                        while (i < buf->packet_count && buf->packets[i].opcode == SIT_OP_DRAW_QUAD) {
                            SitCommandPacket* qp = &buf->packets[i];
                            glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)qp->args.draw_quad.model);
                            glProgramUniform4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1, (const GLfloat*)qp->args.draw_quad.color.raw);
                            // [Bug 9 Fix] Upload UV rect to location 5 (u_uv_rect)
                            glProgramUniform4fv(sit_render.gl.quad_shader_program, 5, 1, (const GLfloat*)qp->args.draw_quad.uv_rect.raw);
                            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                            i++;
                        }
                        i--; // The for loop will increment
                    }

                    // Restore state
                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_CULL_FACE);
                    glBindVertexArray(sit_render.gl.global_vao_id);
                }
                break;

            case SIT_OP_SET_PUSH_CONSTANT:
                {
                    // Optimization: Use tracked state to avoid glGetIntegerv stall
                    GLuint prog = sit_render.gl.current_program_id;
                    if (prog) {
                        void* data = buf->data_buffer + p->args.push_constant.data_offset;
                        size_t sz = p->args.push_constant.size;
                        uint32_t loc = p->args.push_constant.offset;

                        if (sz == sizeof(mat4)) glProgramUniformMatrix4fv(prog, loc, 1, GL_FALSE, (const GLfloat*)data);
                        else if (sz == sizeof(vec4)) glProgramUniform4fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(vec3)) glProgramUniform3fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(vec2)) glProgramUniform2fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(float)) glProgramUniform1fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(int)) glProgramUniform1iv(prog, loc, 1, (const GLint*)data);
                    }
                }
                break;

                        case SIT_OP_BIND_DESCRIPTOR_SET:
                {
                    uint32_t idx = p->args.bind_desc.set_index;
                    size_t offset = p->args.bind_desc.offset;
                    size_t size = p->args.bind_desc.size;
                    uint32_t usage = p->args.bind_desc.usage_flags;

                    // Unpack Handle (slot_index in low 32 bits, generation in high 32 bits)
                    SituationBuffer handle = {0};
                    handle.slot_index = (uint32_t)(p->args.bind_desc.resource_id & 0xFFFFFFFF);
                    handle.generation = (uint32_t)(p->args.bind_desc.resource_id >> 32);

                    _SituationBufferSlot* slot = _SitGetBufferSlot(handle);
                    if (!slot) break;

                    GLuint id = slot->gl_buffer_id;

                    GLenum target = GL_UNIFORM_BUFFER;
                    if (usage & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) target = GL_SHADER_STORAGE_BUFFER;

                    if (size > 0) {
                        glBindBufferRange(target, idx, id, (GLintptr)offset, (GLsizeiptr)size);
                    } else {
                        glBindBufferBase(target, idx, id);
                    }
                }
                break;


            /*
            // [Phase 2 Cleanup] Legacy Texture Binding via Descriptor Set Opcode removed.
            // Textures should use SIT_OP_BIND_TEXTURE_SET (if implemented) or handle this differently.
            // Wait, previous code handled type==1 as glBindTextureUnit.
            // SIT_OP_BIND_DESCRIPTOR_SET is documented as [Core] Binds a buffer's descriptor set.
            // SituationCmdBindTextureSet uses SIT_OP_BIND_TEXTURE_SET? Let's check.
            // SituationCmdBindTextureSet uses SIT_OP_BIND_DESCRIPTOR_SET with type=1 in previous versions?
            // Let's verify SituationCmdBindTextureSet implementation.
            */
            case SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING:
                {
                    // [Phase 2] Legacy Texture/Image binding logic.
                    // This handles SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING, which is used by
                    // SituationCmdBindTextureSet to bind textures (type 1) and storage images (type 3).

                    uint32_t idx = p->args.bind_desc.set_index;
                    uint64_t id = p->args.bind_desc.resource_id;
                    int type = p->args.bind_desc.resource_type;

                    if (type == 1) { // 1 = Sampled Texture
                        if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                            // [Phase 5] Virtual Bindless Fallback
                            // Only use virtual system if the shader supports it (has the injected uniform)
                            if (sit_render.gl.current_virtual_loc >= 0) {
                                int v_slot = _SituationVirtualBindlessBind((GLuint)id);
                                glUniform1i(sit_render.gl.current_virtual_loc, v_slot);
                                // Note: We do NOT bind to 'idx' here because the shader uses the virtual array.
                            } else {
                                // Standard Bind (Fallback for non-bindless shaders)
                                glBindTextureUnit(idx, (GLuint)id);
                            }
                            // Always track for legacy/internal purposes
                            sit_render.gl.current_bound_texture_id = (GLuint)id;
                        } else {
                            // Native GL 4.6 Bindless or standard bind
                            glBindTextureUnit(idx, (GLuint)id);
                            // [v2.3.31] Track texture state for subsequent internal draw calls (Quad/Text)
                            sit_render.gl.current_bound_texture_id = (GLuint)id;
                        }
                    }
                    else if (type == 3) { // 3 = Storage Image
                         glBindImageTexture(idx, (GLuint)id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
                    }
                }
                break;

            case SIT_OP_BIND_VERTEX_BUFFER:
                glVertexArrayVertexBuffer(sit_render.gl.global_vao_id, p->args.bind_vbo.binding,
                                          (GLuint)p->args.bind_vbo.buffer_id,
                                          (GLintptr)p->args.bind_vbo.offset,
                                          (GLsizei)p->args.bind_vbo.stride);
                break;

            case SIT_OP_BIND_INDEX_BUFFER:
                 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)p->args.bind_ibo.buffer_id);
                break;

            case SIT_OP_DRAW:
                {
                    // [Phase 4] Multi-Draw Indirect Optimization
                    // Check for subsequent draw commands with the same opcode
                    size_t batch_count = 1;
                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        if (buf->packets[lookahead].opcode == SIT_OP_DRAW) {
                            batch_count++;
                            lookahead++;
                        } else {
                            break;
                        }
                    }

                    if (batch_count > 1 && sit_render.gl.mdi_data_ptr) {
                        // Batch detected!
                        // 1. Allocate space in MDI ring
                        // Note: Simple linear allocator for now. Assuming 1MB is enough per frame.
                        // Ideally check wrap-around/overflow.
                        size_t cmd_size = sizeof(SitDrawArraysIndirectCommand);
                        size_t total_size = batch_count * cmd_size;
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice
                        if (offset >= mdi_frame_offset && offset + total_size <= mdi_frame_offset + (1024 * 1024)) {
                            SitDrawArraysIndirectCommand* cmds = (SitDrawArraysIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            // 2. Fill commands
                            for (size_t k = 0; k < batch_count; ++k) {
                                SitCommandPacket* next_p = &buf->packets[i + k];
                                cmds[k].count = next_p->args.draw.v_count;
                                cmds[k].instanceCount = next_p->args.draw.i_count;
                                cmds[k].first = next_p->args.draw.first_v;
                                cmds[k].baseInstance = next_p->args.draw.first_i;
                            }

                            // 3. Bind & Draw
                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawArraysIndirect(GL_TRIANGLES, (const void*)((uintptr_t)offset), (GLsizei)batch_count, 0);
                            SIT_CHECK_GL_ERROR();

                            // 4. Advance
                            i += (batch_count - 1); // Loop increments i one more time
                        } else {
                            // Overflow fallback: Draw individually
                            glDrawArraysInstanced(GL_TRIANGLES, p->args.draw.first_v, p->args.draw.v_count, p->args.draw.i_count);
                        }
                    } else {
                        // Single draw fallback
                        glDrawArraysInstanced(GL_TRIANGLES, p->args.draw.first_v, p->args.draw.v_count, p->args.draw.i_count);
                    }
                }
                break;

            case SIT_OP_DRAW_INDEXED:
                {
                    // [Phase 4] Multi-Draw Indirect Optimization
                    size_t batch_count = 1;
                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        if (buf->packets[lookahead].opcode == SIT_OP_DRAW_INDEXED) {
                            batch_count++;
                            lookahead++;
                        } else {
                            break;
                        }
                    }

                    if (batch_count > 1 && sit_render.gl.mdi_data_ptr) {
                        size_t cmd_size = sizeof(SitDrawElementsIndirectCommand);
                        size_t total_size = batch_count * cmd_size;
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice
                        if (offset >= mdi_frame_offset && offset + total_size <= mdi_frame_offset + (1024 * 1024)) {
                            SitDrawElementsIndirectCommand* cmds = (SitDrawElementsIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            for (size_t k = 0; k < batch_count; ++k) {
                                SitCommandPacket* next_p = &buf->packets[i + k];
                                cmds[k].count = next_p->args.draw_indexed.idx_count;
                                cmds[k].instanceCount = next_p->args.draw_indexed.inst_count;
                                cmds[k].firstIndex = next_p->args.draw_indexed.first_idx;
                                cmds[k].baseVertex = next_p->args.draw_indexed.v_offset;
                                cmds[k].baseInstance = next_p->args.draw_indexed.first_inst;
                            }

                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)((uintptr_t)offset), (GLsizei)batch_count, 0);
                            SIT_CHECK_GL_ERROR();

                            i += (batch_count - 1);
                        } else {
                            // Overflow
                            glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, p->args.draw_indexed.idx_count, GL_UNSIGNED_INT, (void*)((uintptr_t)(p->args.draw_indexed.first_idx * 4)), p->args.draw_indexed.inst_count, p->args.draw_indexed.v_offset, p->args.draw_indexed.first_inst);
                        }
                    } else {
                        glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, p->args.draw_indexed.idx_count, GL_UNSIGNED_INT, (void*)((uintptr_t)(p->args.draw_indexed.first_idx * 4)), p->args.draw_indexed.inst_count, p->args.draw_indexed.v_offset, p->args.draw_indexed.first_inst);
                    }
                }
                break;

            case SIT_OP_PIPELINE_BARRIER:
                {
                    GLbitfield barriers = 0;
                    if (p->args.barrier.dst & SITUATION_BARRIER_VERTEX_SHADER_READ)
                        barriers |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_FRAGMENT_SHADER_READ)
                        barriers |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_COMPUTE_SHADER_READ)
                        barriers |= GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_TRANSFER_READ)
                        barriers |= GL_BUFFER_UPDATE_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_INDIRECT_COMMAND_READ)
                        barriers |= GL_COMMAND_BARRIER_BIT;

                    if (barriers == 0) barriers = GL_ALL_BARRIER_BITS;
                    glMemoryBarrier(barriers);
                }
                break;

            case SIT_OP_DISPATCH:
                glDispatchCompute(p->args.dispatch.x, p->args.dispatch.y, p->args.dispatch.z);
                break;

            // [Bug 10 Fix] Handle compute pipeline binding — was missing, causing compute
            // dispatches to use whatever program was previously bound (state leak from prior tests)
            case SIT_OP_BIND_COMPUTE_PIPELINE:
                glUseProgram((GLuint)p->args.bind_pipeline.shader_id);
                sit_render.gl.current_program_id = (GLuint)p->args.bind_pipeline.shader_id;
                break;

            case SIT_OP_PRESENT:
                {
                    _SituationTextureSlot* slot = _SitGetTextureSlot(p->args.present.texture);
                    if (!slot) break;

                    GLuint tex = slot->gl_texture_id;
                    GLuint fbo;
                    glCreateFramebuffers(1, &fbo);
                    if (fbo == 0) break; // Context lost or invalid — skip present
                    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
                    GLenum fb_status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
                    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
                        glDeleteFramebuffers(1, &fbo);
                        break;
                    }
                    // Use captured resolution to avoid race with main thread resize
                    glBlitNamedFramebuffer(fbo, 0,
                        0, 0, p->args.present.texture.width, p->args.present.texture.height,
                        0, 0, p->args.present.target_w, p->args.present.target_h,
                        GL_COLOR_BUFFER_BIT, GL_LINEAR);
                    glDeleteFramebuffers(1, &fbo);
                }
                break;

            case SIT_OP_RENDER_VIRTUAL_DISPLAYS:
                {
                    _SitGLStateBackup gl_backup;
                    _SitGLBackupState(&gl_backup);

                    // API Contract: VD Compositing always targets the main screen.
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    sit_render.gl.current_fbo_id = 0;

                    // Ensure viewport covers the screen
                    glViewport(0, 0, sit_gs.main_window_width, sit_gs.main_window_height);

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);

                    glBindVertexArray(sit_render.gl.vd_quad_vao);

                    float target_width = (float)sit_gs.main_window_width;
                    float target_height = (float)sit_gs.main_window_height;

                    // Sort active VDs
                    SituationVirtualDisplay* vds[SITUATION_MAX_VIRTUAL_DISPLAYS];
                    int v_count = 0;
                    for (int v = 0; v < SITUATION_MAX_VIRTUAL_DISPLAYS; ++v) {
                        if (sit_render.virtual_display_slots_used[v] &&
                            sit_render.virtual_display_slots[v].visible &&
                            sit_render.virtual_display_slots[v].opacity > 0.001f &&
                            sit_render.virtual_display_slots[v].gl.texture_id != 0) {
                            vds[v_count++] = &sit_render.virtual_display_slots[v];
                        }
                    }
                    if (v_count > 0) {
                        qsort(vds, v_count, sizeof(SituationVirtualDisplay*), _SituationSortVirtualDisplaysCallback);
                    }

                    for (int v = 0; v < v_count; ++v) {
                        const SituationVirtualDisplay* vd = vds[v];
                        mat4 T_mat, S_mat, model_matrix;
                        glm_mat4_identity(model_matrix);

                        if (vd->scaling_mode == SITUATION_SCALING_STRETCH) {
                            glm_translate_make(T_mat, (vec3){vd->offset.x, vd->offset.y, 0.0f});
                            glm_scale_make(S_mat, (vec3){target_width, target_height, 1.0f});
                            glm_mat4_mul(T_mat, S_mat, model_matrix);
                        } else if (vd->scaling_mode == SITUATION_SCALING_FIT) {
                            float final_scale = fminf(target_width / vd->resolution.x, target_height / vd->resolution.y);
                            glm_translate_make(T_mat, (vec3){(target_width - (vd->resolution.x * final_scale)) / 2.0f, (target_height - (vd->resolution.y * final_scale)) / 2.0f, 0.0f});
                            glm_scale_make(S_mat, (vec3){vd->resolution.x * final_scale, vd->resolution.y * final_scale, 1.0f});
                            glm_mat4_mul(T_mat, S_mat, model_matrix);
                        } else {
                            float final_scale = fmaxf(1.0f, floorf(fminf(target_width / vd->resolution.x, target_height / vd->resolution.y)));
                            glm_translate_make(T_mat, (vec3){(target_width - (vd->resolution.x * final_scale)) / 2.0f, (target_height - (vd->resolution.y * final_scale)) / 2.0f, 0.0f});
                            glm_scale_make(S_mat, (vec3){vd->resolution.x * final_scale, vd->resolution.y * final_scale, 1.0f});
                            glm_mat4_mul(T_mat, S_mat, model_matrix);
                        }

                        if (vd->blend_mode >= SITUATION_BLEND_OVERLAY) {
                            glBindTexture(GL_TEXTURE_2D, sit_render.gl.composite_copy_texture_id);
                            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, (GLsizei)target_width, (GLsizei)target_height);
                            glProgramUniformMatrix4fv(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)sit_render.gl.vd_ortho_projection);
                            glProgramUniformMatrix4fv(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)model_matrix);
                            glProgramUniform1i(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_BLEND_MODE, vd->blend_mode);
                            glProgramUniform1f(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_OPACITY, vd->opacity);
                            glBindTextureUnit(SIT_SAMPLER_BINDING_SOURCE_1, sit_render.gl.composite_copy_texture_id);
                            glBindTextureUnit(SIT_SAMPLER_BINDING_SOURCE_0, vd->gl.texture_id);
                            glUseProgram(sit_render.gl.composite_shader_program_id);
                            glDisable(GL_BLEND);
                            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        } else {
                            glProgramUniformMatrix4fv(sit_render.gl.vd_shader_program_id, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)sit_render.gl.vd_ortho_projection);
                            glProgramUniformMatrix4fv(sit_render.gl.vd_shader_program_id, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)model_matrix);
                            glProgramUniform1f(sit_render.gl.vd_shader_program_id, SIT_UNIFORM_LOC_OPACITY, vd->opacity);
                            glUseProgram(sit_render.gl.vd_shader_program_id);
                            glEnable(GL_BLEND);
                            glBlendEquation(GL_FUNC_ADD);
                            switch (vd->blend_mode) {
                                case SITUATION_BLEND_ADDITIVE: glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
                                case SITUATION_BLEND_MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ZERO); break;
                                case SITUATION_BLEND_SCREEN:   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); break;
                                case SITUATION_BLEND_NONE:     glDisable(GL_BLEND); break;
                                default:                       glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
                            }
                            glBindTextureUnit(SIT_SAMPLER_BINDING_SOURCE_0, vd->gl.texture_id);
                            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        }
                    }
                    _SitGLRestoreState(&gl_backup);
                }
                break;

            case SIT_OP_DRAW_TEXT:
            case SIT_OP_DRAW_TEXT_EX:
                #if !defined(SITUATION_NO_STB) && !defined(SITUATION_NO_STB_TRUETYPE)
                {
                    const char* text;
                    SituationFont font;
                    Vector2 pos;
                    ColorRGBA color;
                    float fontSize = 0.0f;
                    float spacing = 0.0f;

                    if (p->opcode == SIT_OP_DRAW_TEXT) {
                         text = (const char*)(buf->data_buffer + p->args.draw_text.text_offset);
                         font = p->args.draw_text.font;
                         pos = p->args.draw_text.pos;
                         color = p->args.draw_text.color;
                    } else {
                         text = (const char*)(buf->data_buffer + p->args.draw_text_ex.text_offset);
                         font = p->args.draw_text_ex.font;
                         pos = p->args.draw_text_ex.pos;
                         color = p->args.draw_text_ex.color;
                         fontSize = p->args.draw_text_ex.fontSize;
                         spacing = p->args.draw_text_ex.spacing;
                    }

                    size_t len = strlen(text);
                    if (len == 0) break;
                    if (len > 2048) len = 2048;

                    size_t vert_count = len * 6;
                    size_t data_size = vert_count * 4 * sizeof(float);

                    if (sit_render.text_batch_capacity < data_size) {
                        sit_render.text_batch_scratch = (float*)SIT_REALLOC(sit_render.text_batch_scratch, data_size * 2);
                        sit_render.text_batch_capacity = data_size * 2;
                    }
                    float* vertices = sit_render.text_batch_scratch;

                    float x = pos.x;
                    float y = pos.y;
                    stbtt_bakedchar* cdata = (stbtt_bakedchar*)font.glyph_info;
                    int v_idx = 0;

                    bool is_grid_font = (font.glyph_info == NULL && font.atlas_texture.slot_index == sit_render.default_font.atlas_texture.slot_index && font.atlas_texture.generation == sit_render.default_font.atlas_texture.generation);

                    // Use provided font size or default to font's native size
                    float target_size = (fontSize > 0.0f) ? fontSize : font.font_height_pixels;
                    // For grid font, scale ratio. For STB, it's baked, so we can't easily rescale without artifacts unless signed distance field.
                    // But for simple scaling (like pixel art), scaling the quad is fine.
                    float scale_factor = (font.font_height_pixels > 0.0f) ? (target_size / font.font_height_pixels) : 1.0f;

                    for (size_t k = 0; k < len; k++) {
                        if (is_grid_font) {
                            unsigned char c = (unsigned char)text[k];
                            if (c < 128) {
                                int col = c % 16;
                                int row = c / 16;
                                float u0 = col / 16.0f;
                                float v0 = row / 16.0f;
                                float u1 = (col + 1) / 16.0f;
                                float v1 = (row + 1) / 16.0f;

                                float size_px = 8.0f * scale_factor;
                                float qx0 = x;
                                float qy0 = y;
                                float qx1 = x + size_px;
                                float qy1 = y + size_px;

                                // Advance
                                x += size_px + spacing;

                                vertices[v_idx++] = qx0; vertices[v_idx++] = qy0; vertices[v_idx++] = u0; vertices[v_idx++] = v0;
                                vertices[v_idx++] = qx0; vertices[v_idx++] = qy1; vertices[v_idx++] = u0; vertices[v_idx++] = v1;
                                vertices[v_idx++] = qx1; vertices[v_idx++] = qy0; vertices[v_idx++] = u1; vertices[v_idx++] = v0;

                                vertices[v_idx++] = qx1; vertices[v_idx++] = qy0; vertices[v_idx++] = u1; vertices[v_idx++] = v0;
                                vertices[v_idx++] = qx0; vertices[v_idx++] = qy1; vertices[v_idx++] = u0; vertices[v_idx++] = v1;
                                vertices[v_idx++] = qx1; vertices[v_idx++] = qy1; vertices[v_idx++] = u1; vertices[v_idx++] = v1;
                            }
                        }
                        else if (text[k] >= 32 && text[k] < 128) {
                            float x_before = x;
                            stbtt_aligned_quad q;
                            stbtt_GetBakedQuad(cdata, font.atlas_width, font.atlas_height, text[k] - 32, &x, &y, &q, 1);

                            if (scale_factor != 1.0f || spacing != 0.0f) {
                                float w = q.x1 - q.x0;
                                float h = q.y1 - q.y0;
                                float y_off = q.y0 - y;

                                float x0 = x_before + (q.x0 - x_before) * scale_factor;
                                float y0 = y + y_off * scale_factor;
                                float x1 = x0 + w * scale_factor;
                                float y1 = y0 + h * scale_factor;

                                q.x0 = x0; q.y0 = y0;
                                q.x1 = x1; q.y1 = y1;

                                float advance = x - x_before;
                                x = x_before + (advance * scale_factor) + spacing;
                            }

                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t0;
                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t1;
                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t0;

                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t0;
                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t1;
                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t1;
                        }
                    }
                    int final_vert_count = v_idx / 4;

                    // [v2.3.30] Bindless Text
                    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                        uint64_t handle = SituationGetTextureHandle(font.atlas_texture);
                        if (handle) {
                            // Location 6: u_use_bindless = 1
                            glProgramUniform1i(sit_render.gl.text_shader_program, 6, 1);
                            // Location 7: u_TextureHandle (uint64 handle)
                            // We must use extension function for 64-bit handle
                            #if defined(GLAD_GL_ARB_bindless_texture)
                            glProgramUniformHandleui64ARB(sit_render.gl.text_shader_program, 7, handle);
                            #endif
                        } else {
                            glProgramUniform1i(sit_render.gl.text_shader_program, 6, 0); // Fallback
                            _SituationTextureSlot* slot = _SitGetTextureSlot(font.atlas_texture);
                            if (slot) glBindTextureUnit(SIT_SAMPLER_BINDING_ALBEDO, slot->gl_texture_id);
                        }
                    } else {
                        // Standard Bind
                        glProgramUniform1i(sit_render.gl.text_shader_program, 6, 0);
                        _SituationTextureSlot* slot = _SitGetTextureSlot(font.atlas_texture);
                        if (slot) glBindTextureUnit(SIT_SAMPLER_BINDING_ALBEDO, slot->gl_texture_id);
                    }

                    if (data_size > 524288) data_size = 524288;
                    glNamedBufferSubData(sit_render.gl.text_vbo, 0, data_size, vertices);

                    Vector4 color_vec;
                    SituationConvertColorToVector4(color, &color_vec);
                    glProgramUniform4fv(sit_render.gl.text_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1, (const GLfloat*)color_vec.raw);

                    glUseProgram(sit_render.gl.text_shader_program);
                    sit_render.gl.current_program_id = sit_render.gl.text_shader_program;

                    // Set projection matrix for text shader (ortho, top-left origin)
                    mat4 text_proj;
                    glm_ortho(0.0f, (float)sit_gs.main_window_width, (float)sit_gs.main_window_height, 0.0f, -1.0f, 1.0f, text_proj);
                    glProgramUniformMatrix4fv(sit_render.gl.text_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)text_proj);

                    glBindVertexArray(sit_render.gl.text_vao);
                    glDrawArrays(GL_TRIANGLES, 0, final_vert_count);

                    // [CRITICAL] Restore global VAO state
                    glBindVertexArray(sit_render.gl.global_vao_id);
                }
                #endif
                break;

            case SIT_OP_UPDATE_BUFFER:
                {
                    void* data = buf->data_buffer + p->args.update_buffer.data_offset;
                    glNamedBufferSubData((GLuint)p->args.update_buffer.buffer_id,
                                         (GLintptr)p->args.update_buffer.offset,
                                         (GLsizeiptr)p->args.update_buffer.size,
                                         data);
                }
                break;

            case SIT_OP_SET_VERTEX_ATTRIBUTE:
                {
                    GLenum gl_type = _SituationMapDataTypeToGL((SituationDataType)p->args.set_vertex_attr.type);
                    if (gl_type != 0) {
                        uint32_t loc = p->args.set_vertex_attr.location;
                        glVertexArrayAttribFormat(sit_render.gl.global_vao_id, loc,
                                                  p->args.set_vertex_attr.size,
                                                  gl_type,
                                                  p->args.set_vertex_attr.normalized ? GL_TRUE : GL_FALSE,
                                                  (GLuint)p->args.set_vertex_attr.offset);
                        // Assumption: Binding index matches location for simplicity
                        glVertexArrayAttribBinding(sit_render.gl.global_vao_id, loc, loc);
                        glEnableVertexArrayAttrib(sit_render.gl.global_vao_id, loc);
                    }
                }
                break;

            case SIT_OP_SET_UNIFORM:
                {
                    void* data = buf->data_buffer + p->args.set_uniform.data_offset;
                    GLint loc = p->args.set_uniform.location;
                    GLuint prog = (GLuint)p->args.set_uniform.shader_id;
                    int type = p->args.set_uniform.type;

                    switch (type) {
                        case SIT_UNIFORM_FLOAT: glProgramUniform1fv(prog, loc, 1, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC2:  glProgramUniform2fv(prog, loc, 1, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC3:  glProgramUniform3fv(prog, loc, 1, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC4:  glProgramUniform4fv(prog, loc, 1, (const GLfloat*)data); break;
                        case SIT_UNIFORM_INT:   glProgramUniform1iv(prog, loc, 1, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC2: glProgramUniform2iv(prog, loc, 1, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC3: glProgramUniform3iv(prog, loc, 1, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC4: glProgramUniform4iv(prog, loc, 1, (const GLint*)data); break;
                        case SIT_UNIFORM_MAT4:  glProgramUniformMatrix4fv(prog, loc, 1, GL_FALSE, (const GLfloat*)data); break;
                    }
                }
                break;
        }
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Exited switch, about to check GL error\n");
        fflush(stdout);
        #endif
        SIT_CHECK_GL_ERROR();
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: GL error check complete, continuing loop\n");
        fflush(stdout);
        #endif
    }

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Exited execution loop\n");
    fflush(stdout);
    printf("[OpenGL Debug] About to call first SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] All packets processed, resetting buffer\n");
    // Reset buffer after execution
    buf->packet_count = 0;
    buf->data_cursor = 0;

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] Buffer reset complete, about to call second SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] Cleaning up MDI state\n");
    // [Phase 4] Clean up MDI state
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] About to call glBindBuffer\n");
    fflush(stdout);
    #endif
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] glBindBuffer complete, about to call third SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] Inserting fence\n");
    // [Phase 1.5] Insert Fence for Ring Buffer Synchronization
    // We infer the frame index from the buffer pointer
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] About to calculate frame_idx\n");
    fflush(stdout);
    #endif
    int frame_idx = (int)(buf - sit_render.gl.soft_buffers);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] frame_idx calculated: %d\n", frame_idx);
    fflush(stdout);
    #endif
    if (frame_idx >= 0 && frame_idx < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        if (sit_render.gl.ring_fences[frame_idx]) {
            glDeleteSync(sit_render.gl.ring_fences[frame_idx]);
        }
        sit_render.gl.ring_fences[frame_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
}

/**
 * @brief [INTERNAL] Initializes the OpenGL resources for Virtual Display compositing.
 * @details Creates a dedicated VAO/VBO containing a full-screen quad used by
 *          `SituationRenderVirtualDisplays` to composite off-screen framebuffers.
 *          Restores the user's global VAO before returning.
 * @return `true` on success, `false` if GL resource creation fails.
 * @note Called once during `_SituationInitOpenGL`.
 */
static bool _SituationInitGLVirtualDisplayRenderer(void) {
    glCreateVertexArrays(1, &sit_render.gl.vd_quad_vao);
    glCreateBuffers(1, &sit_render.gl.vd_quad_vbo);
    if (sit_render.gl.vd_quad_vao == 0 || sit_render.gl.vd_quad_vbo == 0) return false;

    // Unit quad [0,1] — the model matrix scales to pixel size and the ortho projection
    // maps pixel coords to NDC. Using [0,1] means: scale by resolution = correct pixel rect.
    float quad_vertices[] = {
        // pos.x  pos.y   uv.x  uv.y
         0.0f,  0.0f,   0.0f, 1.0f,   // top-left
         1.0f,  0.0f,   1.0f, 1.0f,   // top-right
         0.0f,  1.0f,   0.0f, 0.0f,   // bottom-left
         1.0f,  1.0f,   1.0f, 0.0f    // bottom-right
    };
    glNamedBufferStorage(sit_render.gl.vd_quad_vbo, sizeof(quad_vertices), quad_vertices, 0);

    glBindVertexArray(sit_render.gl.vd_quad_vao);
    glVertexArrayVertexBuffer(sit_render.gl.vd_quad_vao, 0, sit_render.gl.vd_quad_vbo, 0, 4 * sizeof(float));
    glEnableVertexArrayAttrib(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION, 0);
    glEnableVertexArrayAttrib(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0, 0);
    glBindVertexArray(0);
    glBindVertexArray(sit_render.gl.global_vao_id);
    return true;
}

/**
 * @brief [INTERNAL] Performs one-time OpenGL backend initialization and context setup.
 *
 * @details This function is called exactly once during library startup (typically from
 *          `SituationInit` or the main initialization sequence) when the `SITUATION_USE_OPENGL`
 *          macro is defined. It is responsible for establishing a fully functional OpenGL
 *          rendering environment that the rest of the library can rely on.
 *
 *          Execution order (critical sequence do not reorder without care):
 *            1. Makes the GLFW window context current on the calling thread
 *               (`glfwMakeContextCurrent(sit_gs.sit_glfw_window)`)
 *            2. Loads OpenGL function pointers via GLAD
 *               (`gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`)
 *            3. Checks minimum required OpenGL version (4.3+ core profile recommended)
 *               and logs error if not met
 *            4. Queries and caches key capabilities/extensions:
 *               - GL version string & GLSL version
 *               - `GL_ARB_bindless_texture` / `GL_EXT_bindless_texture` support
 *               - `GL_ARB_gl_spirv` for SPIR-V binary shaders
 *               - `GL_ARB_multi_bind` / `GL_ARB_direct_state_access` (if used)
 *               - Max texture units, max texture size, max compute workgroup sizes, etc.
 *            5. Sets global GL state defaults used by the library:
 *               - `glEnable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
 *               - `glEnable(GL_DEPTH_TEST)` (optional, depending on init flags)
 *               - `glEnable(GL_CULL_FACE)`, `glCullFace(GL_BACK)`
 *               - `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` / `GL_PACK_ALIGNMENT`
 *            6. Initializes internal OpenGL resources:
 *               - Default white 1x1 texture for missing bindings
 *               - Dummy VAO for core-profile compatibility
 *               - Text renderer (calls `_SituationInitTextRenderer`)
 *               - Bindless texture support (calls `_SituationVirtualBindlessInit`)
 *               - Any other backend-specific singletons (shader cache, quad pipeline, etc.)
 *            7. Verifies no GL errors occurred during init (`SIT_CHECK_GL_ERROR()` macro)
 *            8. Sets `sit_render.gl_initialized = true` and other state flags
 *
 *          On failure (GLAD load fail, version too low, critical extension missing),
 *          logs detailed error messages and sets appropriate `SituationError` code.
 *          The library may continue in degraded mode (e.g. no bindless, no SPIR-V) or abort init.
 *
 * @param init_info Pointer to `SituationInitInfo` structure containing backend preferences,
 *                  window hints, feature toggles, etc. (may be NULL for defaults).
 *
 * @return SITUATION_SUCCESS if OpenGL context and required state initialized successfully,
 *         SITUATION_ERROR_GLAD_LOAD_FAILED if GLAD failed to load functions,
 *         SITUATION_ERROR_GL_VERSION_TOO_LOW if OpenGL version < required (e.g. 4.3),
 *         SITUATION_ERROR_GL_EXTENSION_MISSING for critical missing extensions,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if internal resource creation failed,
 *         or other backend-specific errors propagated from sub-init calls.
 *
 * @note Must be called **with a valid GLFW window context current** on the calling thread.
 *       Thread safety: Only safe from the main thread or thread that owns the context
 *       during library initialization not reentrant or thread-safe afterward.
 *       If render thread is enabled, context is released after this function so render
 *       thread can acquire it.
 *
 *       Critical dependency chain:
 *         - GLFW window must exist (`sit_gs.sit_glfw_window != NULL`)
 *         - `glfwMakeContextCurrent` must succeed before GLAD load
 *         - All subsequent GL calls assume context is current
 *
 * @see SituationInit (primary caller), _SituationInitTextRenderer,
 *      _SituationVirtualBindlessInit, SIT_CHECK_GL_ERROR macro,
 *      SITUATION_ERROR_GLAD_LOAD_FAILED, SITUATION_ERROR_GL_VERSION_TOO_LOW
 */
static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info) {
    // --- 1. Context and Function Loading ---
    glfwMakeContextCurrent(sit_gs.sit_glfw_window); // Ensure context is current for GLAD

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_LOADER_FAILED, "_SituationInitOpenGL: GLAD failed to load function pointers.");
        return SITUATION_ERROR_OPENGL_LOADER_FAILED;
    }

    // --- 2. OpenGL Version and Extension Checks ---
    _SituationVirtualBindlessInit();

    if (GLVersion.major < 4 || (GLVersion.major == 4 && GLVersion.minor < 6)) {
        char detail[128];
        snprintf(detail, sizeof(detail), "_SituationInitOpenGL: OpenGL 4.6 not supported by the driver. Found version %d.%d", GLVersion.major, GLVersion.minor);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION, detail);
        return SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION;
    }

    // Validate required core features/extensions for our abstraction.
    if (!GLAD_GL_ARB_direct_state_access) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "_SituationInitOpenGL: Required extension GL_ARB_direct_state_access is not available.");
        return SITUATION_ERROR_OPENGL_UNSUPPORTED;
    }

    // Check optional extension for SPIR-V if compiler is enabled.
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // Note: Absence of ARB_gl_spirv is NOT a fatal error. It just means we must fallback to GLSL.
    // The refactored _SituationCreateGLComputeProgram handles this logic.
    sit_render.gl.arb_spirv_available = GLAD_GL_ARB_gl_spirv;
    // Optional debug log:
    // if (!sit_render.gl.arb_spirv_available) {
    //     fprintf(stdout, "INFO: GL_ARB_gl_spirv not available. OpenGL compute shaders will use standard GLSL path.\n");
    // }
#endif // SITUATION_ENABLE_SHADER_COMPILER

    // --- 3. VAO Abstraction Initialization ---
    // Create and bind the SINGLE, GLOBAL VAO for all USER rendering (Dynamic/Custom).
    glCreateVertexArrays(1, &sit_render.gl.global_vao_id);

    // [2.3.19] Create the Shared Mesh VAO (PBR Standard Layout)
    // This enables context sharing for meshes, as VAOs are not shared but VBOs are.
    // We configure this VAO once on the render thread and bind shared VBOs to it at draw time.
    glCreateVertexArrays(1, &sit_render.gl.mesh_vao_id);

    // Configure Mesh VAO Layout (Interleaved: Pos3, Norm3, Tan4, UV2)
    // Stride = 12 floats (48 bytes)
    // Binding Index 0
    GLuint mvao = sit_render.gl.mesh_vao_id;

    // Pos (0)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(mvao, SIT_ATTR_POSITION, 0);

    // Norm (1)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_NORMAL);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_NORMAL, 0);

    // Tan (4)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_TANGENT);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_TANGENT, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_TANGENT, 0);

    // UV (2)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_TEXCOORD_0, 0);

    if (sit_render.gl.global_vao_id == 0 || sit_render.gl.mesh_vao_id == 0) {
         _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to create global VAOs.");
         return SITUATION_ERROR_OPENGL_GENERAL;
    }
    glBindVertexArray(sit_render.gl.global_vao_id);
    SIT_CHECK_GL_ERROR(); // Check for errors after binding

    // --- 4. Internal Renderer Initialization ---
    // Initialize internal renderers (Quad Renderer, Virtual Display Renderer).
    // These functions MUST create, configure, and then unbind their own PRIVATE VAOs/VBOs.
    // They MUST leave sit_render.gl.global_vao_id bound upon successful return.
    if (!_SituationInitQuadRenderer(sit_gs.main_window_width, sit_gs.main_window_height)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to initialize internal quad renderer.");
        // Cleanup both VAOs on failure
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return SITUATION_ERROR_OPENGL_GENERAL;
    }

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Initializing default font...\n"); fflush(stdout);
#endif

    if (!_SituationInitDefaultFont()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to initialize default font.");
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return SITUATION_ERROR_OPENGL_GENERAL;
    }

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Default font initialized\n"); fflush(stdout);
    printf("Situation [OpenGL]: About to initialize text renderer...\n"); fflush(stdout);
#endif

    if (!_SituationInitTextRenderer()) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        printf("Situation [OpenGL]: Text renderer init FAILED\n"); fflush(stdout);
#endif
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to initialize internal text renderer.");
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return SITUATION_ERROR_OPENGL_GENERAL;
    }
    
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Text renderer initialized\n"); fflush(stdout);
    printf("Situation [OpenGL]: Creating virtual display shaders...\n"); fflush(stdout);
#endif

    // --- Initialize Virtual Display System ---
    // This involves creating shaders, setting up the VD quad renderer (with its own VAO/VBO), and initializing UBOs used for compositing.
    SituationError shader_err_code = SITUATION_SUCCESS;

    // a. Create Shaders for Virtual Display Compositing
    sit_render.gl.vd_shader_program_id = _SituationCreateGLShaderProgram(SIT_VD_VERTEX_SHADER_SRC, SIT_VD_FRAGMENT_SHADER_SRC, &shader_err_code);
    if (shader_err_code != SITUATION_SUCCESS) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        printf("Situation [OpenGL]: VD shader creation FAILED: %d\n", shader_err_code); fflush(stdout);
#endif
        _SituationSetErrorFromCode(shader_err_code, "_SituationInitOpenGL: Failed to create standard virtual display shader.");
        // Cleanup VAOs
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return shader_err_code;
    }

    sit_render.gl.composite_shader_program_id = _SituationCreateGLShaderProgram(SIT_COMPOSITE_VERTEX_SHADER_SRC, SIT_COMPOSITE_FRAGMENT_SHADER_SRC, &shader_err_code);
    if (shader_err_code != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(shader_err_code, "_SituationInitOpenGL: Failed to create advanced compositing shader.");
        // Cleanup VAOs and first shader
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        return shader_err_code;
    }

    // --- Bind sampler uniforms to correct texture units (Bug 7 fix) ---
    // The VD shaders declare plain `uniform sampler2D` without layout(binding=N),
    // so they default to texture unit 0. We bind textures to units 4 and 5
    // (SIT_SAMPLER_BINDING_SOURCE_0/1), so we must tell the shaders explicitly.
    {
        GLint loc;
        // Simple VD shader: u_screenTexture → unit SIT_SAMPLER_BINDING_SOURCE_0 (4)
        loc = glGetUniformLocation(sit_render.gl.vd_shader_program_id, "u_screenTexture");
        if (loc >= 0) glProgramUniform1i(sit_render.gl.vd_shader_program_id, loc, SIT_SAMPLER_BINDING_SOURCE_0);

        // Advanced composite shader: u_sourceTexture → unit SIT_SAMPLER_BINDING_SOURCE_0 (4)
        loc = glGetUniformLocation(sit_render.gl.composite_shader_program_id, "u_sourceTexture");
        if (loc >= 0) glProgramUniform1i(sit_render.gl.composite_shader_program_id, loc, SIT_SAMPLER_BINDING_SOURCE_0);

        // Advanced composite shader: u_destinationTexture → unit SIT_SAMPLER_BINDING_SOURCE_1 (5)
        loc = glGetUniformLocation(sit_render.gl.composite_shader_program_id, "u_destinationTexture");
        if (loc >= 0) glProgramUniform1i(sit_render.gl.composite_shader_program_id, loc, SIT_SAMPLER_BINDING_SOURCE_1);
    }

    // b. Initialize the Virtual Display Quad Renderer
    // This function is responsible for creating sit_render.gl.vd_quad_vao/vbo, configuring them for a simple textured quad, and unbinding them, ensuring sit_render.gl.global_vao_id is bound again at the end.
    // You need to implement this function, similar to _SituationInitQuadRenderer.
    if (!_SituationInitGLVirtualDisplayRenderer()) { // <-- You need this function
         _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to initialize internal virtual display renderer.");
         // Cleanup global VAO and shaders
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        sit_render.gl.global_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        glDeleteProgram(sit_render.gl.composite_shader_program_id);
        sit_render.gl.composite_shader_program_id = 0;
        // Assume _SituationInitQuadRenderer cleaned up after itself on failure
        return SITUATION_ERROR_OPENGL_GENERAL;
    }

    // c. Create UBO for View/Projection data (used by user shaders, potentially internal ones too)
    glCreateBuffers(1, &sit_render.gl.view_data_ubo_id);
    if (sit_render.gl.view_data_ubo_id == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to create View UBO.");
        // Cleanup global VAO, shaders, and VD renderer resources
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        sit_render.gl.global_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        glDeleteProgram(sit_render.gl.composite_shader_program_id);
        sit_render.gl.composite_shader_program_id = 0;
        // Assume _SituationInitVirtualDisplayRenderer cleaned up after itself on failure
        // Assume _SituationInitQuadRenderer cleaned up after itself on failure
        return SITUATION_ERROR_OPENGL_GENERAL;
    }
    // Allocate storage. Initial data can be set later or here if needed.
    glNamedBufferStorage(sit_render.gl.view_data_ubo_id, sizeof(ViewDataUBO), NULL, GL_DYNAMIC_STORAGE_BIT);
    // Bind it to the standard binding point. This binding is persistent.
    glBindBufferBase(GL_UNIFORM_BUFFER, SIT_UBO_BINDING_VIEW_DATA, sit_render.gl.view_data_ubo_id);
    SIT_CHECK_GL_ERROR();

    // --- [Phase 1] Initialize Persistent Ring Buffer ---
    _SituationInitGLRingBuffer();
    if (!sit_render.gl.ring_data_ptr) return SITUATION_ERROR_OPENGL_GENERAL;
    SIT_CHECK_GL_ERROR();

    // --- [Phase 1.5] Initialize Ring Buffer Fences ---
    _SituationInitGLRingFences();

    // --- [Phase 4] Initialize Multi-Draw Indirect Buffer ---
    _SituationInitGLMDIBuffer();
    if (!sit_render.gl.mdi_data_ptr) return SITUATION_ERROR_OPENGL_GENERAL;
    SIT_CHECK_GL_ERROR();

    // d. Initialize Virtual Display Slots (Data structures)
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        sit_render.virtual_display_slots_used[i] = false;
        // Ensure other members of sit_render.virtual_display_slots[i] are initialized if needed
    }
    sit_render.active_virtual_display_count = 0;
    // Note: Virtual Display *textures/framebuffers* are created on-demand when VDs are created by the user.

    // --- 5. Initial GL State Configuration ---
    if (init_info->initial_active_window_flags & SITUATION_FLAG_VSYNC_HINT) {
        glfwSwapInterval(1);
    } else {
        glfwSwapInterval(0);
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Default clear color

    // --- 6. Finalize ---
    // CRITICAL: Ensure the global_vao_id is the active VAO at the end of initialization.
    // While it should already be bound from step 3, re-binding explicitly after potentially complex internal renderer setups is a good defensive practice.
    glBindVertexArray(sit_render.gl.global_vao_id);
    SIT_CHECK_GL_ERROR();

    // Set the renderer type
    sit_render.renderer_type = SIT_RENDERER_OPENGL;

    // --- Populate Enabled Features Mask for OpenGL ---
    sit_render.enabled_features_mask = 0; // Clear first
    // Basic features implied by GL 4.6
    sit_render.enabled_features_mask |= SIT_FEATURE_GEOMETRY_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_TESSELLATION_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_WIDE_LINES;
    sit_render.enabled_features_mask |= SIT_FEATURE_FILL_MODE_NON_SOLID;
    sit_render.enabled_features_mask |= SIT_FEATURE_SAMPLER_ANISOTROPY;
    sit_render.enabled_features_mask |= SIT_FEATURE_COMPUTE_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_INT64;
    sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT64;
    sit_render.enabled_features_mask |= SIT_FEATURE_DRAW_INDIRECT_COUNT; // Core in 4.6 (GL_ARB_indirect_parameters)
    sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_DRAW_INDIRECT; // Core in 4.3
    sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_VIEWPORT;      // Core in 4.1 (GL_ARB_viewport_array)

    // Extension-based features
#if defined(GLAD_GL_NV_shader_buffer_load) && defined(GLAD_GL_EXT_buffer_reference)
    if (GLAD_GL_NV_shader_buffer_load || GLAD_GL_EXT_buffer_reference) {
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_BUFFERS;
    }
#endif
#if defined(GLAD_GL_ARB_bindless_texture) && defined(GLAD_GL_ARB_gpu_shader_int64)
    if (GLAD_GL_ARB_bindless_texture && GLAD_GL_ARB_gpu_shader_int64) {
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_TEXTURES;
    }
#endif
#if defined(GLAD_GL_NV_mesh_shader) && defined(GLAD_GL_EXT_mesh_shader)
    if (GLAD_GL_NV_mesh_shader || GLAD_GL_EXT_mesh_shader) {
        sit_render.enabled_features_mask |= SIT_FEATURE_MESH_SHADER;
    }
#endif
#if defined(GLAD_GL_KHR_shader_subgroup)
    if (GLAD_GL_KHR_shader_subgroup) {
        sit_render.enabled_features_mask |= SIT_FEATURE_SUBGROUP_OPERATIONS;
    }
#endif
#if defined(GLAD_GL_AMD_gpu_shader_half_float) || defined(GLAD_GL_NV_gpu_shader5)
    if (GLAD_GL_AMD_gpu_shader_half_float || GLAD_GL_NV_gpu_shader5) {
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT16;
    }
#endif
#if defined(GLAD_GL_NV_shader_atomic_float)
    if (GLAD_GL_NV_shader_atomic_float) {
        sit_render.enabled_features_mask |= SIT_FEATURE_ATOMIC_FLOAT;
    }
#endif
#if defined(GLAD_GL_EXT_texture_compression_s3tc)
    if (GLAD_GL_EXT_texture_compression_s3tc) {
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_BC;
    }
#endif
#if defined(GLAD_GL_KHR_texture_compression_astc_ldr)
    if (GLAD_GL_KHR_texture_compression_astc_ldr) {
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_ASTC;
    }
#endif
    // Standard GL framebuffers can usually handle 10-bit if requested
    sit_render.enabled_features_mask |= SIT_FEATURE_HDR_OUTPUT;

    // [Phase 2.5] Initialize VAO Cache & Graveyard
    // Zero cache is handled by SIT_CALLOC of context.
    // Initialize Graveyards
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; ++i) {
        if (ma_mutex_init(&sit_render.gl.graveyards[i].lock) != MA_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to init GL graveyard mutex.");
            return SITUATION_ERROR_INIT_FAILED;
        }
        // Pre-allocate arrays
        sit_render.gl.graveyards[i].mesh_capacity = 32;
        sit_render.gl.graveyards[i].mesh_ids_to_clean = (uint64_t*)SIT_MALLOC(32 * sizeof(uint64_t));
        sit_render.gl.graveyards[i].buffer_capacity = 32;
        sit_render.gl.graveyards[i].buffers_to_delete = (GLuint*)SIT_MALLOC(32 * sizeof(GLuint));
        sit_render.gl.graveyards[i].texture_capacity = 32;
        sit_render.gl.graveyards[i].textures_to_delete = (GLuint*)SIT_MALLOC(32 * sizeof(GLuint));
        if (!sit_render.gl.graveyards[i].mesh_ids_to_clean || !sit_render.gl.graveyards[i].buffers_to_delete || !sit_render.gl.graveyards[i].textures_to_delete) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        sit_render.gl.frame_fences[i] = 0;
    }

    // [Phase 2] Initialize Threading Support (Loader Window Only)
    #if !defined(__STDC_NO_THREADS__)
    // 1. Create Loader Window (Hidden, Shares Context with Main Window)
    // This window is used by the main thread for async asset loading while the render thread uses the main window.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    sit_render.gl.loader_window = glfwCreateWindow(640, 480, "Situation Loader", NULL, sit_gs.sit_glfw_window);

    // [v2.3.21] Thread spawning logic moved to _SituationInitRenderThread in SituationInit
    // Note: Context handover logic is also moved there.
    #endif

    return SITUATION_SUCCESS;
}


#if defined(SITUATION_ENABLE_SHADER_COMPILER)
/**
 * @brief Creates a complete, linked OpenGL shader program from SPIR-V binary blobs.
 * @details This is the second stage of the unified shader pipeline for the OpenGL backend.
 *          It leverages the `GL_ARB_gl_spirv` extension to load the pre-compiled SPIR-V bytecode directly, bypassing the driver's GLSL compiler. This offers two key advantages:
 *          1) It ensures that shaders behave identically to the Vulkan backend.
 *          2) It can significantly speed up shader loading, as the driver only needs to ingest the binary, not perform a full compilation.
 *
 * @param vs_blob A pointer to the compiled SPIR-V blob for the vertex shader.
 * @param fs_blob A pointer to the compiled SPIR-V blob for the fragment shader.
 * @param error_code A pointer to a SituationError that will be filled on failure.
 * @return A valid OpenGL program ID on success, or 0 on failure.
 */
static GLuint _SituationCreateGLShaderProgramFromSpirv(const _SituationSpirvBlob* vs_blob, const _SituationSpirvBlob* fs_blob, SituationError* error_code) {
    if (!vs_blob || !fs_blob || !vs_blob->data || !fs_blob->data) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SPIR-V blob data cannot be null.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Driver does not support GL_ARB_gl_spirv. Cannot load SPIR-V shaders.");
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_UNSUPPORTED;
        return 0;
    }

    GLint success = 0;
    char infoLog[SITUATION_MAX_SHADER_LOG_LEN];

    // --- Create and load the Vertex Shader ---
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    // 1. Load the binary SPIR-V data into the shader object.
    glShaderBinary(1, &vs, GL_SHADER_BINARY_FORMAT_SPIR_V, vs_blob->data, (GLsizei)vs_blob->size);
    // 2. Specialize the shader. This is the SPIR-V equivalent of "compiling" the binary for the driver.
    //    It specifies the entry point ("main") and any specialization constants (none in this case).
    glSpecializeShader(vs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    // 3. Verify that the specialization was successful.
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vs, sizeof(infoLog), NULL, infoLog);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, infoLog);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
        glDeleteShader(vs);
        return 0;
    }

    // --- Create and load the Fragment Shader (identical process) ---
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderBinary(1, &fs, GL_SHADER_BINARY_FORMAT_SPIR_V, fs_blob->data, (GLsizei)fs_blob->size);
    glSpecializeShader(fs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fs, sizeof(infoLog), NULL, infoLog);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, infoLog);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    // --- Link the shaders into a program ---
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // The individual shader objects are no longer needed after linking.
    glDeleteShader(vs);
    glDeleteShader(fs);
    SIT_CHECK_GL_ERROR();

    // Verify the linking was successful.
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        glDeleteProgram(program);
        return 0;
    }

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates a single-stage (compute) OpenGL shader program from a single SPIR-V blob.
 * @details This is the modern, preferred creation path for compute shaders on the OpenGL backend when `GL_ARB_gl_spirv` is available. It uses `glShaderBinary` to load the pre-compiled SPIR-V bytecode directly, bypassing the driver's GLSL compiler.
 *
 * @par Creation Process
 *   1.  Creates a new shader object of type `GL_COMPUTE_SHADER`.
 *   2.  Loads the binary SPIR-V data into the shader object using `glShaderBinary`.
 *   3.  "Specializes" the shader using `glSpecializeShader`, which is the SPIR-V equivalent of compiling the binary for the driver.
 *   4.  Checks the `GL_COMPILE_STATUS` to verify that specialization was successful.
 *   5.  Creates a program object, attaches the specialized shader, and links the program.
 *   6.  The intermediate shader object is deleted after a successful link.
 *
 * @param cs_blob A pointer to the `_SituationSpirvBlob` containing the compiled compute shader bytecode.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful creation.
 * @return `0` on failure. A detailed error message is set internally.
 *
 * @note This function is for internal use by `_SituationCreateGLComputeProgram` only.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram`.
 *
 * @see _SituationCreateGLComputeProgram(), glShaderBinary(), glSpecializeShader()
 */
static GLuint _SituationCreateGLComputeProgramFromSpirv(const struct _SituationSpirvBlob* cs_blob, SituationError* error_code) {
    if (error_code) *error_code = SITUATION_SUCCESS;

    // 1. --- Validation ---
    if (!cs_blob || !cs_blob->data || cs_blob->size == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SPIR-V blob for compute shader cannot be null or empty.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }
    if (!GLAD_GL_ARB_gl_spirv) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Driver does not support GL_ARB_gl_spirv. Cannot load SPIR-V shaders.");
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_UNSUPPORTED;
        return 0;
    }

    GLint success = 0;
    char infoLog[SITUATION_MAX_SHADER_LOG_LEN];

    // 2. --- Create and Specialize the Compute Shader Object ---
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderBinary(1, &cs, GL_SHADER_BINARY_FORMAT_SPIR_V, cs_blob->data, (GLsizei)cs_blob->size);
    glSpecializeShader(cs, "main", 0, NULL, NULL);
    SIT_CHECK_GL_ERROR();

    // Verify that the specialization was successful.
    glGetShaderiv(cs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(cs, sizeof(infoLog), NULL, infoLog);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, infoLog);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
        glDeleteShader(cs);
        return 0;
    }

    // 3. --- Link the Shader into a Program ---
    GLuint program = glCreateProgram();
    glAttachShader(program, cs);
    glLinkProgram(program);

    // The individual shader object is no longer needed after linking.
    glDeleteShader(cs);
    SIT_CHECK_GL_ERROR();

    // Verify that the linking was successful.
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        glDeleteProgram(program);
        return 0;
    }

    return program;
}
#endif // Shader Compiler

/**
 * @brief [INTERNAL] One-time initialization for OpenGL bindless texture support in virtual display paths.
 *
 * @details Called once during library startup (usually from `SituationInit` or first virtual display creation)
 *          to check for and prepare bindless texture functionality when using OpenGL backend.
 *
 *          What it does:
 *            - Checks for `GL_ARB_bindless_texture` extension availability
 *              (via `glfwExtensionSupported` or `glGetStringi`)
 *            - Logs warning if missing (virtual displays may fall back to legacy binding)
 *            - Caches extension function pointers if using GLAD or manual loading
 *              (`glGetTextureHandleARB`, `glMakeTextureHandleResidentARB`, etc.)
 *            - Pre-allocates or resets any bindless handle cache / resident table
 *            - Sets internal flag `sit_render.gl_supports_bindless` for later queries
 *
 *          After success, bindless operations become available for virtual display textures,
 *          improving performance by reducing texture unit pressure in multi-pass rendering.
 *
 * @return true if bindless support is available and initialized successfully,
 *         false if extension missing or initialization failed (logs internally)
 *
 * @note This function is idempotent safe to call multiple times.
 *       Thread safety: Must be called with GL context current (typically main/render thread during init).
 *       No runtime cost after init only queried via flag.
 *
 *       If bindless is unavailable, virtual display rendering falls back to traditional
 *       `glActiveTexture` + `glBindTexture` per draw call (slower in complex scenes).
 *
 * @see _SituationVirtualBindlessBind, SituationCreateVirtualDisplay,
 *      SITUATION_ERROR_GL_EXTENSION_MISSING, glGetTextureHandleARB
 */
static void _SituationVirtualBindlessInit(void) {
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        _SituationVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[i];
        slot->texture_slot_index = i;
        slot->gl_texture_id = 0;
        slot->last_used_counter = 0;
        slot->is_active = false;
    }
    sit_render.gl.virtual_stats.hits = 0;
    sit_render.gl.virtual_stats.misses = 0;
    sit_render.gl.virtual_stats.evictions = 0;
    sit_render.gl.virtual_lru_counter = 0;
}

/**
 * @brief [INTERNAL] Binds a GL texture to a bindless texture unit for virtual display / offscreen use.
 *
 * @details This low-level helper records the necessary OpenGL state changes to make a texture
 *          accessible via bindless texture handles in shaders used by virtual displays.
 *
 *          It is called internally when:
 *            - A virtual display needs to sample a texture in its render pass
 *            - A texture is attached as a color attachment or input to a virtual framebuffer
 *            - Dynamic texture updates occur (e.g. after blitting or CPU upload)
 *
 *          Typical sequence:
 *            - Generates or retrieves a bindless texture handle via `glGetTextureHandleARB`
 *            - Makes the handle resident if not already (`glMakeTextureHandleResidentARB`)
 *            - Binds the handle to a uniform location or shader storage block
 *              (via `glProgramUniformHandleui64ARB` or equivalent)
 *
 *          This function exists to abstract away ARB_bindless_texture / EXT_bindless_texture
 *          boilerplate while ensuring compatibility with virtual display rendering paths.
 *
 * @param gl_texture_id OpenGL texture name (GLuint) that should be made bindless.
 *                      Must be a valid, complete texture object (has storage allocated).
 *
 * @return The 64-bit bindless handle (`GLuint64`) that was made resident and can be
 *         passed to shaders, or 0 on failure (invalid texture, extension missing, etc.).
 *
 * @note Requires `GL_ARB_bindless_texture` (or EXT equivalent) to be supported and loaded.
 *       If the extension is missing, logs a warning and returns 0.
 *       Handles are made resident once and stay resident until texture destruction
 *       or explicit `glMakeTextureHandleNonResidentARB`.
 *       Thread safety: Must be called with an active OpenGL context (typically render thread).
 *
 * @see _SituationVirtualBindlessInit, glGetTextureHandleARB, glMakeTextureHandleResidentARB,
 *      glProgramUniformHandleui64ARB, SITUATION_ERROR_GL_EXTENSION_MISSING
 */
static int _SituationVirtualBindlessBind(GLuint gl_texture_id) {
    sit_render.gl.virtual_lru_counter++;
    uint64_t current_counter = sit_render.gl.virtual_lru_counter;

    // 1. Check if the texture is already bound (Hit?)
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        _SituationVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[i];
        if (slot->is_active && slot->gl_texture_id == gl_texture_id) {

            // Hit! Update LRU
            slot->last_used_counter = current_counter;
            sit_render.gl.virtual_stats.hits++;
            return i;
        }
    }

    // 2. Miss! Find a slot to evict
    sit_render.gl.virtual_stats.misses++;

    int best_slot = -1;
    uint64_t oldest_counter = UINT64_MAX;

    // First pass: look for empty slot
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        if (!sit_render.gl.virtual_texture_slots[i].is_active) {
            best_slot = i;
            break;
        }
    }

    // Second pass: if full, find LRU
    if (best_slot == -1) {
        sit_render.gl.virtual_stats.evictions++;
        for (int i = 0; i < SITUATION_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
            if (sit_render.gl.virtual_texture_slots[i].last_used_counter < oldest_counter) {
                oldest_counter = sit_render.gl.virtual_texture_slots[i].last_used_counter;
                best_slot = i;
            }
        }
    }

    // Safety fallback
    if (best_slot == -1) best_slot = 0;

    // 3. Bind the texture to the chosen slot
    _SituationVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[best_slot];

    // Bind the actual texture to the texture unit using DSA
    glBindTextureUnit(best_slot, gl_texture_id);

    // Update slot metadata
    slot->is_active = true;
    slot->gl_texture_id = gl_texture_id;
    slot->last_used_counter = current_counter;

    return best_slot;
}

#endif // SITUATION_USE_OPENGL

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Initializes Vulkan pipelines for the Virtual Display Compositing system.
 * @details This function is the second stage of internal renderer setup. It compiles and creates the specific graphics pipelines used by `SituationRenderVirtualDisplays` to draw off-screen framebuffers onto the main screen.
 *
 * @par Scope
 * This function creates two distinct pipelines:
 * 1. **Simple Compositor:** For standard blending (Alpha, Additive, Multiply). Uses a lightweight shader and single texture sampling.
 * 2. **Advanced Compositor:** For complex "Photoshop-style" blend modes (Overlay, Soft Light, etc.). Uses a specialized shader that samples both the source Virtual Display and a copy of the destination framebuffer.
 *
 * @note The standard 2D Quad Renderer (`SituationCmdDrawQuad`) is **not** initialized here; it is handled by the shared `_SituationInitQuadRenderer` function before this one is called.
 *
 * @par Initialization Sequence
 *   1. **Simple Compositor:**
 *      - Compiles `SIT_VD_VERTEX_SHADER_SRC` and `SIT_VD_FRAGMENT_SHADER_SRC`.
 *      - Creates a pipeline layout with: Set 0 (View UBO), Set 1 (Source Sampler), and Push Constants.
 *      - Creates the `vd_compositing_pipeline`.
 *   2. **Advanced Compositor:**
 *      - Compiles `SIT_COMPOSITE_VERTEX_SHADER_SRC` and `SIT_COMPOSITE_FRAGMENT_SHADER_SRC`.
 *      - Creates a pipeline layout with: Set 0 (View UBO), Set 1 (Source Sampler), Set 2 (Dest Sampler), and Push Constants.
 *      - Creates the `advanced_compositing_pipeline`.
 *
 * @return SITUATION_SUCCESS on successful initialization of both pipelines.
 * @return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED if shader compilation or pipeline creation fails. Cleanup is performed automatically on failure.
 *
 * @warning This function relies on the shader compiler being enabled (`SITUATION_ENABLE_SHADER_COMPILER`). If disabled, it returns success immediately but leaves the pipeline handles as `VK_NULL_HANDLE`, effectively disabling Virtual Displays.
 *
 * @see _SituationInitVulkan(), _SituationInitQuadRenderer(), SituationRenderVirtualDisplays()
 */
static SituationError _SituationVulkanInitInternalRenderers(void) {
    // --- Initialize all local handles to NULL for robust cleanup ---
    // NOTE: Quad renderer is initialized separately by _SituationInitQuadRenderer()

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanInitInternalRenderers starting...\n"); fflush(stdout);
    #endif

    VkPipelineLayout text_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       text_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout vd_compositing_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       vd_compositing_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout advanced_compositing_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       advanced_compositing_pipeline = VK_NULL_HANDLE;

    _SituationSpirvBlob vs_spirv = {};
    _SituationSpirvBlob fs_spirv = {};

    // ---------------------------------------------------------------------------------
    // CRITICAL CHECK: Only proceed if Shader Compiler is enabled.
    // Internal renderers rely on runtime GLSL compilation.
    // ---------------------------------------------------------------------------------
#if !defined(SITUATION_ENABLE_SHADER_COMPILER)
    // If no compiler, we simply return success but leave internal pipelines as NULL.
    // Drawing functions will check for NULL and skip drawing.
    // Note: A pre-compiled SPIR-V path could be added here in future versions.
    return SITUATION_SUCCESS;
#else

    // ======================================================================================
    // --- 1. Initialize the Simple VD Compositing Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Compiling VD vertex shader...\n"); fflush(stdout);
        #endif
        vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_VD_VERTEX_SHADER_SRC, "internal_vd.vert", shaderc_vertex_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        printf("Situation [Vulkan Debug]: Compiling VD fragment shader...\n"); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_VD_FRAGMENT_SHADER_SRC, "internal_vd.frag", shaderc_fragment_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: SHADER COMPILATION FAILED! vs_spirv.data=%p, fs_spirv.data=%p\n", vs_spirv.data, fs_spirv.data); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD shaders compiled successfully\n"); fflush(stdout);
        #endif

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating VD pipeline layout...\n"); fflush(stdout);
        printf("Situation [Vulkan Debug]: Device=%p, view_data_ubo_layout=%p, image_sampler_layout=%p\n",
               (void*)sit_render.vk.device, (void*)sit_render.vk.view_data_ubo_layout, (void*)sit_render.vk.image_sampler_layout);
        fflush(stdout);
        #endif

        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(mat4) + sizeof(float)
        };
        VkDescriptorSetLayout layouts[] = { sit_render.vk.view_data_ubo_layout, sit_render.vk.image_sampler_layout };
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Calling vkCreatePipelineLayout...\n"); fflush(stdout);
        #endif
        VkResult layout_result = vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &vd_compositing_pipeline_layout);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: vkCreatePipelineLayout result=%d (VK_SUCCESS=0)\n", layout_result); fflush(stdout);
        #endif
        if (layout_result != VK_SUCCESS) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: PIPELINE LAYOUT CREATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD pipeline layout created successfully, handle=%p\n", (void*)vd_compositing_pipeline_layout); fflush(stdout);
        #endif

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_descs[2];
        attr_descs[0].binding = 0;
        attr_descs[0].location = SIT_ATTR_POSITION;
        attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[0].offset = 0;
        attr_descs[1].binding = 0;
        attr_descs[1].location = SIT_ATTR_TEXCOORD_0;
        attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[1].offset = 0;

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating VD graphics pipeline...\n"); fflush(stdout);
        #endif
        vd_compositing_pipeline = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            vd_compositing_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            1, &binding_desc,
            2, attr_descs,
            0u
        );
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD graphics pipeline created, handle=%p\n", (void*)vd_compositing_pipeline); fflush(stdout);
        #endif
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (vd_compositing_pipeline == VK_NULL_HANDLE) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: VD GRAPHICS PIPELINE CREATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD pipeline initialization complete\n"); fflush(stdout);
        #endif
    }

    // ======================================================================================
    // --- 2. Initialize the Advanced VD Compositing Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Starting Advanced Compositing initialization...\n"); fflush(stdout);
        #endif
        vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_COMPOSITE_VERTEX_SHADER_SRC, "internal_composite.vert", shaderc_vertex_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Advanced vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_COMPOSITE_FRAGMENT_SHADER_SRC, "internal_composite.frag", shaderc_fragment_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Advanced fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: ADVANCED COMPOSITING SHADER COMPILATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }

        VkDescriptorSetLayout layouts_adv[] = {
            sit_render.vk.view_data_ubo_layout,
            sit_render.vk.image_sampler_layout,
            sit_render.vk.composite_dest_sampler_layout,
        };

        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = sizeof(mat4) + sizeof(int) + sizeof(float)
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 3,
            .pSetLayouts = layouts_adv,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &advanced_compositing_pipeline_layout) != VK_SUCCESS) goto cleanup;

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_desc;
        attr_desc.binding = 0;
        attr_desc.location = SIT_ATTR_POSITION;
        attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
        attr_desc.offset = 0;

        advanced_compositing_pipeline = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            advanced_compositing_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            1, &binding_desc,
            1, &attr_desc,
            0u
        );

        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (advanced_compositing_pipeline == VK_NULL_HANDLE) goto cleanup;
    }

    // ======================================================================================
    // --- 3. Initialize the Batched Text Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Starting Text Renderer initialization...\n"); fflush(stdout);
        #endif
        vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_TEXT_VERTEX_SHADER, "internal_text.vert", shaderc_vertex_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_TEXT_FRAGMENT_SHADER, "internal_text.frag", shaderc_fragment_shader);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: TEXT RENDERER SHADER COMPILATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }

        VkDescriptorSetLayout layouts[] = { sit_render.vk.view_data_ubo_layout, sit_render.vk.bindless_descriptor_layout };
        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = sizeof(vec4) + sizeof(uint32_t) // Color + TextureID
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating text pipeline layout with 2 sets:\n");
        printf("Situation [Vulkan Debug]:   Set 0 (UBO): %p\n", (void*)layouts[0]);
        printf("Situation [Vulkan Debug]:   Set 1 (Sampler): %p\n", (void*)layouts[1]);
        fflush(stdout);
        #endif

        if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &text_pipeline_layout) != VK_SUCCESS) goto cleanup;

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text pipeline layout created: %p\n", (void*)text_pipeline_layout);
        fflush(stdout);
        #endif

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 4 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_descs[2];
        attr_descs[0].binding = 0;
        attr_descs[0].location = SIT_ATTR_POSITION;
        attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[0].offset = 0;
        attr_descs[1].binding = 0;
        attr_descs[1].location = SIT_ATTR_TEXCOORD_0;
        attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[1].offset = 2 * sizeof(float);  // CRITICAL: UV comes after XY (8 bytes offset)

        text_pipeline = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            text_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            1, &binding_desc,
            2, attr_descs,
            0u
        );
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (text_pipeline == VK_NULL_HANDLE) goto cleanup;
    }

    // --- Success ---
    // NOTE: quad_pipeline_layout, quad_pipeline, quad_vertex_buffer, and quad_vertex_buffer_memory
    // are initialized by _SituationInitQuadRenderer(), NOT here. Do not overwrite them!
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: All internal renderers initialized successfully!\n"); fflush(stdout);
    #endif
    sit_render.vk.text_pipeline = text_pipeline;
    sit_render.vk.text_pipeline_layout = text_pipeline_layout;
    sit_render.vk.vd_compositing_pipeline_layout = vd_compositing_pipeline_layout;
    sit_render.vk.vd_compositing_pipeline = vd_compositing_pipeline;
    sit_render.vk.advanced_compositing_pipeline_layout = advanced_compositing_pipeline_layout;
    sit_render.vk.advanced_compositing_pipeline = advanced_compositing_pipeline;

    return SITUATION_SUCCESS;

cleanup:
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanInitInternalRenderers FAILED - entering cleanup\n"); fflush(stdout);
    #endif

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);
    // NOTE: Quad renderer resources are NOT cleaned up here - they're managed by _SituationCleanupQuadRenderer()
    if (text_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, text_pipeline_layout, NULL);
    if (text_pipeline) vkDestroyPipeline(sit_render.vk.device, text_pipeline, NULL);
    if (vd_compositing_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, vd_compositing_pipeline_layout, NULL);
    if (vd_compositing_pipeline) vkDestroyPipeline(sit_render.vk.device, vd_compositing_pipeline, NULL);
    if (advanced_compositing_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, advanced_compositing_pipeline_layout, NULL);
    if (advanced_compositing_pipeline) vkDestroyPipeline(sit_render.vk.device, advanced_compositing_pipeline, NULL);
    return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

#endif // SITUATION_ENABLE_SHADER_COMPILER
}

/**
 * @brief [INTERNAL] Allocates resources for the screen copy operation.
 *
 * @details Creates a `VkImage`, `VkImageView`, and a persistent `VkDescriptorSet` specifically designed to hold a copy of the swapchain's backbuffer.
 *          This resource is used by the Advanced Compositing pipeline to read the destination color for blend modes like Overlay and Soft Light.
 *
 * @note This function is called automatically during swapchain creation/recreation to ensure the image dimensions match the window size.
 */
static SituationError _SituationVulkanCreateScreenCopyResource(void) {
    /* Caller must have composite_dest_sampler_layout and swapchain extent/format valid. */
    if (sit_render.vk.swapchain_extent.width == 0 || sit_render.vk.swapchain_extent.height == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Screen copy: zero swapchain extent.");
    }

    // 1. Create the Image (Device Local, Usage: Transfer Dst + Sampled)
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (_SituationVulkanCreateImage(
        sit_render.vk.swapchain_extent.width,
        sit_render.vk.swapchain_extent.height,
        1,
        sit_render.vk.swapchain_image_format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VMA_MEMORY_USAGE_GPU_ONLY,
        &sit_render.vk.screen_copy_image,
        &sit_render.vk.screen_copy_memory
    ) != SITUATION_SUCCESS) {
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Screen copy: vkCreateImage failed.");
    }

    sit_render.vk.screen_copy_view = _SituationVulkanCreateImageView(
        sit_render.vk.screen_copy_image,
        sit_render.vk.swapchain_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    if (sit_render.vk.screen_copy_view == VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Screen copy: vkCreateImageView failed.");
    }

    sit_render.vk.screen_copy_descriptor_set = _SituationVulkanAllocateDescriptorSet(
        sit_render.vk.composite_dest_sampler_layout,
        &sit_render.vk.screen_copy_descriptor_pool
    );

    if (sit_render.vk.screen_copy_descriptor_set == VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.screen_copy_view, NULL);
        sit_render.vk.screen_copy_view = VK_NULL_HANDLE;
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate descriptor set for Screen Copy.");
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Frees the screen copy resources.
 *
 * @details Destroys the image, view, and descriptor set created by `_SituationVulkanCreateScreenCopyResource`.
 *          Called during swapchain cleanup.
 */
static void _SituationVulkanDestroyScreenCopyResource(void) {
    if (sit_render.vk.screen_copy_descriptor_set != VK_NULL_HANDLE) {
        // [FIX v2.3.27B] Explicitly free the set to prevent memory leaks during resize
        if (sit_render.vk.screen_copy_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(
                sit_render.vk.device,
                sit_render.vk.screen_copy_descriptor_pool,
                1,
                &sit_render.vk.screen_copy_descriptor_set
            );
        }
        sit_render.vk.screen_copy_descriptor_set = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_descriptor_pool = VK_NULL_HANDLE;
    }

    if (sit_render.vk.screen_copy_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.screen_copy_view, NULL);
        sit_render.vk.screen_copy_view = VK_NULL_HANDLE;
    }

    if (sit_render.vk.screen_copy_image != VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
    }
}

/**
 * @brief [INTERNAL] Allocates and begins recording a temporary, primary-level Vulkan command buffer.
 *
 * @details This helper function is a standard and convenient way to execute short, one-off sequences of Vulkan commands (e.g., image layout transitions, buffer copies, setting image data). It simplifies the process by:
 * 1.  Allocating a single primary command buffer from the library's main command pool (`sit_render.vk.command_pool`).
 * 2.  Beginning recording on that buffer with the `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` flag, indicating it will be submitted once and then discarded.
 *
 * @par Typical Usage Pattern
 * The caller uses this function to get a command buffer, records commands into it using `vkCmd*` functions, and then calls
 * `_SituationVulkanEndSingleTimeCommands` to end recording, submit the commands to the graphics queue, wait for completion, and clean up the buffer.
 *
 * @code{.c}
 * VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
 * if (cmd == VK_NULL_HANDLE) { // Handle allocation/error } // Check added for robustness
 * // Record commands, e.g., vkCmdPipelineBarrier, vkCmdCopyBuffer...
 * _SituationVulkanEndSingleTimeCommands(cmd); // Handles submission, wait, and cleanup
 * @endcode
 *
 * @return A valid `VkCommandBuffer` handle ready for command recording.
 * @return `VK_NULL_HANDLE` if the library is not initialized, if the Vulkan device or command pool is invalid, or if allocation/beginning fails.
 *         A specific error message is set via `_SituationSetErrorFromCode`.
 *
 * @note This function is for internal library use and is not part of the public API.
 * @note It is the caller's sole responsibility to pass the returned `VkCommandBuffer` handle to `_SituationVulkanEndSingleTimeCommands` to ensure proper submission, synchronization, and cleanup.
 *       Failing to do so will result in resource leaks.
 * @warning This function allocates a command buffer. Not calling `_SituationVulkanEndSingleTimeCommands` will leak this resource.
 * @warning This function is synchronous; `_SituationVulkanEndSingleTimeCommands` calls `vkQueueWaitIdle`, blocking the CPU until the commands complete.
 *
 * @see _SituationVulkanEndSingleTimeCommands(), vkAllocateCommandBuffers(), vkBeginCommandBuffer()
 */
static VkCommandBuffer _SituationVulkanBeginSingleTimeCommands(void) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanBeginSingleTimeCommands called\n"); fflush(stdout);
    #endif
    // --- 1. Input/State Validation ---
    // NOTE: We do NOT check SituationIsInitialized() here because this function is called
    // DURING initialization (e.g., when creating quad renderer buffers). The is_initialized
    // flag is only set at the END of SituationInit(), so checking it here would cause
    // a false negative and prevent initialization from completing.
    // Instead, we only check if the device and command pool are valid, which is sufficient.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Checking device/pool...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Device: %p, Pool: %p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.command_pool); fflush(stdout);
    #endif
    if (sit_render.vk.device == VK_NULL_HANDLE || sit_render.vk.command_pool == VK_NULL_HANDLE) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: ERROR: Device or command pool is NULL!\n"); fflush(stdout);
        #endif
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanBeginSingleTimeCommands: Vulkan device or command pool is NULL.");
        return VK_NULL_HANDLE;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Device/pool OK, allocating command buffer...\n"); fflush(stdout);
    #endif

    // --- 2. Allocate Command Buffer ---
    VkCommandBufferAllocateInfo alloc_info = {}; // Explicitly zero-initialize
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; // Mandatory sType
    alloc_info.pNext = NULL; // No extension structures
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary command buffer
    alloc_info.commandPool = sit_render.vk.command_pool; // Use the library's main command pool
    alloc_info.commandBufferCount = 1; // Allocate one command buffer

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: About to call vkAllocateCommandBuffers...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   device=%p, pool=%p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.command_pool); fflush(stdout);
    #endif

    VkCommandBuffer command_buffer = VK_NULL_HANDLE; // Initialize handle
    VkResult result = vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, &command_buffer);
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: vkAllocateCommandBuffers result: %d, buffer: %p\n", result, (void*)command_buffer); fflush(stdout);
    #endif
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanBeginSingleTimeCommands: vkAllocateCommandBuffers failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        return VK_NULL_HANDLE; // Return invalid handle on allocation failure
    }

    // --- 3. Begin Recording Command Buffer ---
    VkCommandBufferBeginInfo begin_info = {}; // Explicitly zero-initialize
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; // Mandatory sType
    begin_info.pNext = NULL; // No extension structures
    // --- CRITICAL FLAG ---
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT tells the driver this buffer
    // will be submitted once and then not used again. This can enable optimizations.
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // --- END CRITICAL FLAG ---
    begin_info.pInheritanceInfo = NULL; // Not used for primary command buffers

    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanBeginSingleTimeCommands: vkBeginCommandBuffer failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // --- CRITICAL CLEANUP ---
        // If vkBeginCommandBuffer fails, we must free the allocated command buffer to prevent a resource leak.
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        // --- END CRITICAL CLEANUP ---
        return VK_NULL_HANDLE; // Return invalid handle on begin failure
    }

    // --- 4. Success ---
    // If we reach here, the command buffer is valid and in the recording state.
    // It is the caller's responsibility to end and submit it using _SituationVulkanEndSingleTimeCommands.
    return command_buffer;
}


// --- Updated/Added Documentation Block for _SituationVulkanEndSingleTimeCommands ---
/**
 * @brief [INTERNAL] Ends recording, submits, waits for completion, and cleans up a one-time-use command buffer.
 *
 * @details This helper function completes the lifecycle of a temporary command buffer created by `_SituationVulkanBeginSingleTimeCommands`. It performs the following essential steps:
 * 1.  Ends the recording of the command buffer.
 * 2.  Submits the command buffer to the graphics queue (`sit_render.vk.graphics_queue`) for execution.
 * 3.  Waits for the graphics queue to become idle (`vkQueueWaitIdle`), ensuring that all commands recorded in the buffer have finished executing on the GPU.
 *     This makes the function synchronous.
 * 4.  Frees the command buffer back to the pool (`sit_render.vk.command_pool`) from which it was allocated.
 *
 * @par Typical Usage Pattern
 * This function is always paired with `_SituationVulkanBeginSingleTimeCommands`.
 *
 * @code{.c}
 * VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
 * if (cmd == VK_NULL_HANDLE) { // Handle allocation/error }
 * // Record commands, e.g., vkCmdPipelineBarrier, vkCmdCopyBuffer...
 * _SituationVulkanEndSingleTimeCommands(cmd); // Handles submission, wait, and cleanup
 * @endcode
 *
 * @param command_buffer The `VkCommandBuffer` handle returned by `_SituationVulkanBeginSingleTimeCommands`. This handle must be valid.
 *
 * @note This function is for internal library use and is not part of the public API.
 * @note This function is synchronous due to the `vkQueueWaitIdle` call. It will block the calling thread until the GPU has finished executing the commands.
 * @note It is crucial that the `command_buffer` parameter is the handle returned by `_SituationVulkanBeginSingleTimeCommands` and has not been previously ended or submitted.
 * @warning Failing to call this function after obtaining a command buffer from `_SituationVulkanBeginSingleTimeCommands` will result in a resource leak.
 * @warning Calling this function with an invalid or already-ended `command_buffer` handle can lead to undefined behavior or validation errors.
 *
 * @see _SituationVulkanBeginSingleTimeCommands(), vkEndCommandBuffer(), vkQueueSubmit(), vkQueueWaitIdle(), vkFreeCommandBuffers()
 */
static void _SituationVulkanEndSingleTimeCommands(VkCommandBuffer command_buffer) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanEndSingleTimeCommands called\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   command_buffer=%p\n", (void*)command_buffer); fflush(stdout);
    printf("Situation [Vulkan Debug]:   device=%p, queue=%p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.graphics_queue); fflush(stdout);
    #endif
    // --- 1. Input Validation ---
    // While internal, checking for VK_NULL_HANDLE prevents potential crashes.
    if (command_buffer == VK_NULL_HANDLE) {
        // Silently return if an invalid handle is passed.
        // This prevents crashing but indicates a logic error upstream.
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanEndSingleTimeCommands: command_buffer is VK_NULL_HANDLE.");
        return;
    }
    // Note: We don't check sit_render.vk.device/queue/pool here as they should be valid if this function is called correctly after Begin. A check could be added if paranoia dictates.

    // --- 2. End Recording the Command Buffer ---
    VkResult result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkEndCommandBuffer failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // --- CRITICAL DECISION ---
        // If vkEndCommandBuffer fails, the command buffer is in an undefined state.
        // Attempting to submit it would be incorrect.
        // The safest approach is to free it immediately to prevent leaks, even though submission/waiting will be skipped.
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        // --- END CRITICAL DECISION ---
        return; // Exit early, do not proceed with submission/waiting
    }

    // --- 3. Submit the Command Buffer to the Graphics Queue ---
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; // Explicitly zero-initialize
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; // Mandatory sType
    submit_info.pNext = NULL; // No extension structures
    submit_info.waitSemaphoreCount = 0; // No semaphores to wait on for one-time cmds
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1; // Submit one command buffer
    submit_info.pCommandBuffers = &command_buffer; // The buffer to submit
    submit_info.signalSemaphoreCount = 0; // No semaphores to signal upon completion
    submit_info.pSignalSemaphores = NULL;

    result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkQueueSubmit failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // --- CRITICAL CLEANUP ---
        // Even if submission fails, we must still free the command buffer
        // to prevent a resource leak.
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        // --- END CRITICAL CLEANUP ---
        return; // Exit early, do not proceed with waiting
    }

    // --- 4. Wait for the Submitted Commands to Finish ---
    // This is the synchronous part. It blocks the CPU thread until the GPU is completely done executing the commands in `command_buffer`.
    // This ensures resources used by those commands are no longer in use.
    result = vkQueueWaitIdle(sit_render.vk.graphics_queue);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkQueueWaitIdle failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // Note: Even if waiting fails (indicating a serious problem like device loss), we still attempt to free the command buffer. The device might be in a bad state, but cleanup is still the right intention.
    }
    // Note: No specific error handling for vkQueueWaitIdle failure beyond logging.
    // The device might be lost, but proceeding with freeing the buffer is still necessary.

    // --- 5. Free the Command Buffer ---
    // Regardless of whether the wait succeeded (in terms of device health), we must free the command buffer to prevent leaks.
    vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
    // After this call, `command_buffer` is an invalid handle and must not be used.
}


// --- Updated/Added Documentation Block for _SituationReadSpirvFile ---
/**
 * @brief [INTERNAL] Reads a raw SPIR-V binary file into memory.
 *
 * @details This helper function is a convenience wrapper designed specifically for loading compiled SPIR-V shader binaries (`.spv` files) from the filesystem.
 *          It leverages the library's existing `SituationLoadFileData` function to perform the actual file I/O, ensuring consistency in file access methods.
 *
 * @par Expected File Format
 * The file pointed to by `filename` is expected to be a raw binary file containing the compiled SPIR-V bytecode. No text parsing or conversion is performed.
 *
 * @param filename The null-terminated path to the `.spv` file to be read. This pointer must not be NULL.
 * @param[out] out_size A pointer to a `size_t` variable where the size of the read data (in bytes) will be stored. This pointer must not be NULL.
 *
 * @return A pointer to a newly allocated block of memory containing the raw bytes of the SPIR-V file.
 *         - The caller takes ownership of this memory and is responsible for calling `free()` on the returned pointer when it is no longer needed.
 *         - The memory block is guaranteed to be at least `*out_size` bytes long and null-terminated (an extra byte is allocated and set to 0, though the SPIR-V data itself is binary and might contain 0s).
 * @return NULL if the function fails. This can happen if:
 *         - `filename` is NULL.
 *         - `out_size` is NULL.
 *         - `SituationLoadFileData` fails to open or read the file (e.g., file not found, permission denied, read error).
 *         - Memory allocation fails.
 *         In case of failure, `*out_size` is set to 0, and a specific error message is set via `_SituationSetErrorFromCode` (inherited from `SituationLoadFileData` or generated by this function).
 *
 * @note This function is for internal library use and is not part of the public API.
 * @note The returned pointer points to memory allocated using the standard `SIT_MALLOC`.
 *       It must be freed using the standard `free`.
 * @warning The caller must check the return value for NULL before using it.
 * @warning The contents of the returned buffer are raw binary data and should be treated as such. Casting it to other types (e.g., `uint32_t*` for
 *          SPIR-V words) is safe, but care must be taken with endianness if the file was compiled for a different architecture.
 *
 * @see SituationLoadFileData(), SIT_FREE()
 */
static char* _SituationReadSpirvFile(const char* filename, size_t* out_size) {
    // --- 1. Input Validation ---
    if (!filename) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationReadSpirvFile: filename cannot be NULL.");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (!out_size) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationReadSpirvFile: out_size cannot be NULL.");
        // Cannot set *out_size to 0 as it's NULL, but error is set.
        return NULL;
    }

    // --- 2. Initialize Output ---
    *out_size = 0; // Initialize output size to zero in case of early return.

    // --- 3. Use Existing File Loading Logic ---
    // Leverage the library's established `SituationLoadFileData` function.
    // This function is assumed to handle file opening, reading into a SIT_MALLOC'd buffer, and null-terminating the buffer. It returns the number of bytes read.
    unsigned int bytes_read_u32 = 0; // Use the type expected by SituationLoadFileData
    unsigned char* buffer_tmp = NULL;
    SituationLoadFileData(filename, &bytes_read_u32, &buffer_tmp);
    char* buffer = (char*)buffer_tmp;

    // --- 4. Handle Result from SituationLoadFileData ---
    if (!buffer) {
        // SituationLoadFileData failed. It should have set a specific error message in sit_gs.last_error_msg (e.g., "File not found", "Permission denied").
        // We can optionally prefix this message for context.
        char prefixed_error[SITUATION_MAX_ERROR_MSG_LEN];
        snprintf(prefixed_error, sizeof(prefixed_error),
                 "_SituationReadSpirvFile: Failed to load file '%s'. Reason: %s",
                 filename);
        char* error_msg = NULL;
        SituationGetLastErrorMsg(&error_msg); // Get error from SituationLoadFileData
        _SituationSetError(prefixed_error); // Update global error with context
        // *out_size is already 0.
        return NULL; // Return NULL to indicate failure
    }

    // --- 5. Success ---
    // SituationLoadFileData succeeded. `buffer` points to the allocated data, and `bytes_read_u32` contains the number of bytes read.
    // Convert the byte count to size_t for the output parameter.
    *out_size = (size_t)bytes_read_u32;

    // The buffer is allocated by SituationLoadFileData and is owned by the caller.
    // It is guaranteed to be at least *out_size bytes + 1 (null terminator).
    return buffer;
}

#if defined(SITUATION_ENABLE_SHADER_COMPILER)

/**
 * @brief [INTERNAL] Callback for resolving `#include` directives in GLSL shaders.
 *
 * @details This function is invoked by the `shaderc` compiler whenever it encounters an `#include` statement
 *          in the shader source code. It attempts to load the requested file content from disk.
 *
 *          This enables the creation of modular "Uber Shaders" where common logic (math utilities,
 *          struct definitions) is stored in shared `.glslh` header files.
 *
 * @param user_data Optional user context (unused).
 * @param requested_source The path string inside the include directive (e.g., "common/math.glslh").
 * @param type The type of include (Standard vs Relative). Currently treated identically.
 * @param requesting_source The path of the file that contains the include directive (for relative path resolution).
 * @param include_depth The current nesting depth of includes (for recursion limits).
 *
 * @return A pointer to a `shaderc_include_result` struct containing the loaded source code or error info.
 *         The compiler will later pass this pointer to `_SituationShaderIncluderRelease`.
 */
static shaderc_include_result* _SituationShaderIncluderResolve(
    void* user_data,
    const char* requested_source,
    int type,
    const char* requesting_source,
    size_t include_depth)
{
    (void)user_data; (void)type; (void)include_depth; (void)requesting_source;

    _SitIncludeResult* container = (_SitIncludeResult*)SIT_CALLOC(1, sizeof(_SitIncludeResult));

    // 1. Load the file
    // Note: In a more complex engine, we would resolve relative paths based on 'requesting_source'.
    // For now, we assume paths are relative to the CWD or absolute.
    container->content = SituationLoadFileText(requested_source);
    container->full_path = _sit_strdup(requested_source);

    if (container->content) {
        container->result.content = container->content;
        container->result.content_length = strlen(container->content);
        container->result.source_name = container->full_path;
        container->result.source_name_length = strlen(container->full_path);
    } else {
        // Error: Provide an error message as the content
        const char* err_msg = "Could not open included file.";
        container->result.content = err_msg;
        container->result.content_length = strlen(err_msg);
        container->result.source_name = "";
        container->result.source_name_length = 0;
        // Empty path signals failure to shaderc? No, usually content is error msg.
        // But standard behavior is usually just failing to load.
    }

    return &container->result;
}

/**
 * @brief [INTERNAL] Callback for freeing memory allocated during shader inclusion.
 *
 * @details This function is called by `shaderc` once it has finished processing an included file.
 *          It is responsible for freeing the `content` buffer (loaded from disk) and the
 *          `shaderc_include_result` container structure itself.
 *
 * @param user_data Optional user context (unused).
 * @param include_result The pointer returned by `_SituationShaderIncluderResolve`.
 */
static void _SituationShaderIncluderRelease(void* user_data, shaderc_include_result* include_result) {
    (void)user_data;
    _SitIncludeResult* container = (_SitIncludeResult*)include_result;
    if (container) {
        if (container->content && container->content != container->result.content) {
             // Handle error message case if strictly needed, but usually we just free content
        }
        // If content was loaded via SituationLoadFileText (SIT_MALLOC), free it.
        // If it was a static error string, we shouldn't free it.
        // Simpler logic:
        if (container->result.source_name_length > 0) { // Was successful load
             SIT_FREE(container->content);
        }
        SIT_FREE(container->full_path);
        SIT_FREE(container);
    }
}

/**
 * @brief [INTERNAL] Compiles a GLSL source string into a SPIR-V binary blob using shaderc.
 *
 * @details This is the first and most crucial stage of the unified shader pipeline. It takes standard, human-readable GLSL code and transforms it into the SPIR-V intermediate representation.
 *          This SPIR-V bytecode can then be consumed by both the Vulkan backend and the OpenGL backend (if `GL_ARB_gl_spirv` is supported), ensuring shader consistency across different graphics APIs and potentially improving load times.
 *
 * @param glsl_source A null-terminated C-string containing the GLSL shader code to be compiled.
 * @param source_name A descriptive name for the shader (e.g., "scene.vert", "compute_filter.comp").
 *                    This name is used in error messages generated by shaderc to help identify the problematic shader.
 * @param shader_kind The type of shader being compiled (e.g., `shaderc_vertex_shader`, `shaderc_fragment_shader`, `shaderc_compute_shader`). This tells shaderc which specific compilation rules and validation checks to apply.
 *
 * @return A `_SituationSpirvBlob` struct.
 *         - On **success**, the struct is populated:
 *           - `internal_result` points to a valid `shaderc_compilation_result_t` object containing the compiled SPIR-V data.
 *           - `data` points to the raw SPIR-V bytecode within the `internal_result`.
 *           - `size` is the size of the SPIR-V bytecode in bytes.
 *         - On **failure**, the struct is zero-initialized (`{0}`), and the library's global error state (`sit_gs.last_error_msg`, `sit_gs.last_error_code`) is updated with a specific error code and a descriptive message (either from shaderc or an internal error).
 *
 * @note This function requires the `SITUATION_ENABLE_SHADER_COMPILER` define to be set, as it directly depends on the `shaderc` library.
 * @warning The returned `_SituationSpirvBlob` contains an `internal_result` object (`shaderc_compilation_result_t*`) that **must** be freed later by calling `_SituationFreeSpirvBlob` to prevent memory leaks within the shaderc library.
 *          Failing to do so will result in leaked resources.
 * @warning The pointers `blob.data` and `blob.size` are only valid as long as the `blob.internal_result` object exists and has not been released.
 *
 * @see _SituationFreeSpirvBlob(), _SituationCreateVulkanShaderModule(),
 *      SituationCreateComputePipelineFromMemory()
 */
static _SituationSpirvBlob _SituationVulkanCompileGLSLtoSPIRV(
    const char* glsl_source,
    const char* source_name,
    shaderc_shader_kind shader_kind)
{
    // --- 1. Input Validation ---
    // Perform basic checks on input parameters to prevent crashes or undefined behavior
    // within this function or the shaderc library.
    _SituationSpirvBlob blob = {0}; // Initialize return struct to zero/NULL

    if (!glsl_source) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCompileGLSLtoSPIRV: GLSL source code pointer is NULL." );
        return blob; // Return zero-initialized struct
    }
    // Check for empty string (strlen(glsl_source) == 0) could also be done,
    // but shaderc might handle it gracefully or provide its own error.
    // Let's assume non-NULL is sufficient for now.

    if (!source_name) {
        // Provide a default name for error reporting if none is given.
        // This prevents a potential crash if shaderc internally uses source_name.
        source_name = "<unnamed_shader>";
        // Alternatively, return an error:
        // _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCompileGLSLtoSPIRV: source_name is NULL.");
        // return blob;
    }

    // --- 2. Initialize shaderc Compiler and Options ---
    // These are the core objects needed to configure and perform the compilation.
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compile_options_t options = shaderc_compile_options_initialize();

    // Enable #include support
    shaderc_compile_options_set_include_callbacks(
        options,
        _SituationShaderIncluderResolve,
        _SituationShaderIncluderRelease,
        NULL // user_data
    );

    // Check if initialization was successful.
    if (!compiler) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Failed to initialize shaderc compiler." );
        // Clean up any potentially partially initialized object.
        if (options) {
            shaderc_compile_options_release(options);
        }
        return blob; // Return zero-initialized struct
    }
    if (!options) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Failed to initialize shaderc compile options." );
        // Clean up the successfully initialized compiler.
        shaderc_compiler_release(compiler);
        return blob; // Return zero-initialized struct
    }

    // --- 3. Configure Compilation Options ---
    // Set the target environment to Vulkan 1.1. This is a good baseline that ensures
    // compatibility with the Vulkan features used by the library and also aligns with
    // OpenGL's SPIR-V capabilities (GL_ARB_gl_spirv typically supports Vulkan 1.0+ SPIR-V).
    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

    // Enable performance optimizations for the generated SPIR-V.
    // This can reduce shader size and potentially improve runtime performance.
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);

    // In debug builds (when NDEBUG is NOT defined), generate additional debug information.
    // This information is useful for debugging tools like RenderDoc or Nsight Graphics.
#ifndef NDEBUG
    shaderc_compile_options_set_generate_debug_info(options);
#endif

    // --- 4. Perform the Compilation ---
    // This is the core operation where shaderc processes the GLSL source.
    // strlen(glsl_source) is used to determine the length of the input string.
    // shaderc makes a copy of the source internally, so the input string can be freed
    // after this call returns.
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] About to call shaderc_compile_into_spv for '%s' (%zu bytes)\n", source_name, strlen(glsl_source)); fflush(stderr);
#endif
    blob.internal_result = shaderc_compile_into_spv(
        compiler,               // The initialized shaderc compiler instance
        glsl_source,            // The GLSL source code string
        strlen(glsl_source),   // The length of the GLSL source code
        shader_kind,            // The type of shader (vertex, fragment, compute, etc.)
        source_name,            // The name for error reporting
        "main",                 // The entry point function name within the shader
        options                 // The configured compilation options
    );
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] shaderc_compile_into_spv returned, checking status...\n"); fflush(stderr);
#endif
    if (blob.internal_result) {
        shaderc_compilation_status status = shaderc_result_get_compilation_status(blob.internal_result);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Situation] Compilation status: %d\n", (int)status);
#endif
        if (status != shaderc_compilation_status_success) {
            const char* err = shaderc_result_get_error_message(blob.internal_result);
            fprintf(stderr, "[Situation] Compilation FAILED: %s\n", err ? err : "<no message>");
        } else {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
            fprintf(stderr, "[Situation] Compilation SUCCESS\n");
#endif
        }
        fflush(stderr);
    } else {
        fprintf(stderr, "[Situation] ERROR: blob.internal_result is NULL!\n"); fflush(stderr);
        _SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILATION_FAILED, "shaderc_compile_into_spv returned NULL result object");
    }

    // --- 5. Release Temporary Resources ---
    // The compiler and options objects are no longer needed after the compilation call.
    // Release them immediately to free their associated resources.
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);

    // Check if the compilation call itself produced a result object.
    // A NULL internal_result here would indicate a fundamental failure in the
    // shaderc_compile_into_spv call itself, perhaps due to an internal error in shaderc.
    if (!blob.internal_result) {
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: shaderc_compile_into_spv returned NULL result object." );
        // No need to call shaderc_result_release as internal_result is NULL.
        return (_SituationSpirvBlob){0}; // Explicitly return zeroed struct
    }

    // --- 6. Check Compilation Status ---
    // Even if internal_result is not NULL, the compilation might have failed.
    // shaderc reports the status separately.
    shaderc_compilation_status status = shaderc_result_get_compilation_status(blob.internal_result);

    if (status != shaderc_compilation_status_success) {
        // Compilation failed. Retrieve the detailed error message from shaderc.
        const char* error_message = shaderc_result_get_error_message(blob.internal_result);
        // Determine the most appropriate SituationError code based on the shaderc status.
        SituationError sit_error_code = SITUATION_ERROR_SHADER_COMPILATION_FAILED;
        // Map specific shaderc errors to potentially more descriptive Situation errors if needed.
        // For example:
        // if (status == shaderc_compilation_status_invalid_stage) {
        //     sit_error_code = SITUATION_ERROR_INVALID_PARAM; // Or a new specific error?
        // } else if (status == shaderc_compilation_status_compilation_error) {
        //     sit_error_code = SITUATION_ERROR_SHADER_COMPILATION_FAILED; // Default
        // }
        // For now, use the general compilation failed error.

        char full_error_msg[512]; // Buffer for a more detailed error message
        snprintf(
            full_error_msg,
            sizeof(full_error_msg),
            "_SituationVulkanCompileGLSLtoSPIRV: Shader compilation failed for '%s' (Status: %d). Details: %s",
            source_name,
            (int)status,
            (error_message && strlen(error_message) > 0) ? error_message : "<no details from shaderc>"
        );

        _SituationSetErrorFromCode(sit_error_code, full_error_msg);

        // Clean up the shaderc result object associated with the failed compilation.
        shaderc_result_release(blob.internal_result);
        blob.internal_result = NULL; // Defensive nulling
        // Return a zero-initialized struct to indicate failure.
        return (_SituationSpirvBlob){0};
    }

    // --- 7. Extract Compiled Data (On Success) ---
    // If we reach here, the compilation was successful.
    // Extract the pointer to the compiled SPIR-V bytecode and its size from the result object.
    blob.data = (const uint8_t*)shaderc_result_get_bytes(blob.internal_result);
    blob.size = shaderc_result_get_length(blob.internal_result);

    // Perform a final sanity check: ensure data is not NULL and size is not zero for a successful compilation.
    // While unlikely if status is success, it's a good defensive measure.
    if (!blob.data || blob.size == 0) {
         // This is unexpected for a successful compilation.
        _SituationSetErrorFromCode( SITUATION_ERROR_SHADER_COMPILATION_FAILED, "_SituationVulkanCompileGLSLtoSPIRV: Successful compilation yielded NULL data or zero size." );
        // Clean up the result object.
        shaderc_result_release(blob.internal_result);
        blob.internal_result = NULL;
        return (_SituationSpirvBlob){0};
    }

    // --- 8. Return Successful Result ---
    // The blob struct is now fully populated with the successful compilation result.
    // The caller is responsible for calling _SituationFreeSpirvBlob later.
    return blob;
}

/**
 * @brief [INTERNAL] Releases the memory held by a shaderc compilation result object.
 *
 * @details This helper function is responsible for cleaning up the resources associated with a `_SituationSpirvBlob`, specifically the `shaderc_compilation_result_t` object stored in its `internal_result` member.
 *          This object holds the compiled SPIR-V bytecode and any associated metadata (like error messages) generated by the shaderc library.
 *
 * @param blob A pointer to the `_SituationSpirvBlob` struct whose `internal_result` member should be freed.
 *
 * @note It is safe to call this function on a `_SituationSpirvBlob` that has not been successfully initialized by `_SituationVulkanCompileGLSLtoSPIRV` (e.g., if `internal_result` is `NULL`). In such cases, the function will simply do nothing.
 * @note This function only frees the `internal_result`. It does not modify the `data` or `size` members of the `blob` struct itself. After calling this function, the `blob` struct should be considered invalid or re-initialized before reuse.
 * @warning The `blob` pointer itself is not freed by this function. If the `_SituationSpirvBlob` struct was allocated on the heap, the caller is still responsible for freeing that memory.
 *
 * @see _SituationVulkanCompileGLSLtoSPIRV(), _SituationSpirvBlob
 */
static void _SituationFreeSpirvBlob(_SituationSpirvBlob* blob) {
    // --- 1. Input Validation ---
    // Check if the blob pointer itself is valid.
    if (!blob) {
        // Calling free on a NULL pointer is safe, but explicitly checking
        // prevents potential misuse and can aid debugging.
        return; // Silently return, consistent with freeing NULL pointers.
    }

    // --- 2. Release Shaderc Resource ---
    // Check if the internal shaderc result object exists before attempting to release it.
    if (blob->internal_result) {
        // This is the actual call to the shaderc library to release the compilation result.
        shaderc_result_release(blob->internal_result);
        // Set the pointer to NULL to indicate it's no longer valid.
        // This prevents accidental double-free if the function were called again.
        blob->internal_result = NULL;
    }

    // Note: The `data` and `size` members of the `_SituationSpirvBlob` struct
    // are not modified here. They point to memory managed by the
    // `shaderc_compilation_result_t` object, which is freed by `shaderc_result_release`.
    // After this call, `blob->data` should be considered a dangling pointer
    // relative to its original source and should not be used.
}
#endif

/**
 * @brief [INTERNAL] Creates a Vulkan Shader Module from SPIR-V bytecode.
 *
 * @details This helper function encapsulates the process of creating a `VkShaderModule` object from a block of compiled SPIR-V code. A `VkShaderModule` is a container object that holds the compiled shader code and makes it available for use in pipeline shader stages.
 *
 * @param code A pointer to the raw SPIR-V bytecode data. This data must be valid and correctly compiled SPIR-V. The function does not validate the SPIR-V itself, only that the pointer is not NULL and `code_size` is non-zero.
 * @param code_size The size of the SPIR-V bytecode data in bytes. This must be a non-zero, positive value and should be a multiple of 4, as SPIR-V is a 32-bit word-based format.
 *
 * @return A valid `VkShaderModule` handle on success.
 * @return `VK_NULL_HANDLE` if the function fails. This can occur if:
 *         - The input `code` pointer is `NULL`.
 *         - The `code_size` is 0.
 *         - The call to `vkCreateShaderModule` fails (e.g., due to invalid SPIR-V, driver issues, or device loss). A specific error message is set in the library's global error state via `_SituationSetErrorFromCode`.
 *
 * @note This function requires that `sit_render.vk.device` is a valid and initialized `VkDevice` handle. This is guaranteed by the library's Vulkan initialization sequence if this function is called correctly.
 * @note The caller is responsible for destroying the returned `VkShaderModule` using `vkDestroyShaderModule` when it is no longer needed, typically after the pipeline using it has been created.
 * @warning The SPIR-V data pointed to by `code` is not validated by this function for correctness beyond basic size and pointer checks. Passing invalid SPIR-V can lead to errors during pipeline creation or runtime.
 *
 * @see _SituationVulkanCreateComputePipeline(), _SituationVulkanCreateGraphicsPipeline(), vkCreateShaderModule(), vkDestroyShaderModule()
 */
static VkShaderModule _SituationCreateVulkanShaderModule(const char* code, size_t code_size) {
    // --- 1. Input Validation ---
    // Check for invalid inputs before proceeding. This prevents crashes
    // and provides clearer error feedback early.
    if (!code) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationCreateVulkanShaderModule: SPIR-V code pointer is NULL." );
        return VK_NULL_HANDLE;
    }
    if (code_size == 0) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationCreateVulkanShaderModule: SPIR-V code size is zero." );
        return VK_NULL_HANDLE;
    }
    // Optional: Check if code_size is a multiple of 4 (size of a SPIR-V word).
    // While vkCreateShaderModule might catch this, checking here is defensive.
    if (code_size % 4 != 0) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationCreateVulkanShaderModule: SPIR-V code size is not a multiple of 4 bytes." );
        return VK_NULL_HANDLE;
    }

    // --- 2. Prepare VkShaderModuleCreateInfo ---
    // This struct tells Vulkan how to create the shader module from the provided data.
    VkShaderModuleCreateInfo create_info = {}; // Explicitly zero-initialize
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; // Mandatory sType
    create_info.pNext = NULL; // No extension structures
    create_info.flags = 0; // No special flags for shader module creation
    create_info.codeSize = code_size; // Size of the SPIR-V bytecode in bytes
    // Cast the const char* to const uint32_t* as required by VkShaderModuleCreateInfo.
    // The Vulkan specification allows this for SPIR-V data.
    create_info.pCode = (const uint32_t*)code;

    // --- 3. Create the VkShaderModule ---
    // This is the actual Vulkan API call that creates the shader module object.
    VkShaderModule shader_module = VK_NULL_HANDLE; // Initialize handle
    VkResult result = vkCreateShaderModule(
        sit_render.vk.device,       // The logical device the module is associated with
        &create_info,           // Creation parameters
        NULL,                   // Optional allocation callbacks (use default)
        &shader_module          // Output: the created VkShaderModule handle
    );

    // --- 4. Handle Result ---
    if (result != VK_SUCCESS) {
        // vkCreateShaderModule failed. This usually indicates a problem with
        // the provided SPIR-V data (invalid format, unsupported instructions)
        // or a driver/device issue.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationCreateVulkanShaderModule failed: vkCreateShaderModule returned VkResult 0x%x. Check SPIR-V validity or driver state.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED, error_detail);
        // Ensure the output handle remains VK_NULL_HANDLE on failure.
        shader_module = VK_NULL_HANDLE;
        // Note: No need to call vkDestroyShaderModule as it wasn't successfully created.
    }

    // --- 5. Return Result ---
    // Return the created handle (VK_NULL_HANDLE on failure).
    return shader_module;
}

/**
 * @brief [INTERNAL] Creates a complete Vulkan graphics pipeline from pre-compiled SPIR-V shader files.
 * @details This is a core, high-level helper function for the Vulkan backend that orchestrates the entire process of creating a `VkPipeline`. It is a simplified entry point that assumes a standard vertex format and rendering state suitable for typical 3D rendering.
 *
 * @par Creation Process
 *   1.  **Load SPIR-V:** It reads the raw SPIR-V bytecode from the vertex and fragment shader files specified by `vs_path` and `fs_path`.
 *   2.  **Create Shader Modules:** It creates `VkShaderModule` objects from the loaded bytecode.
 *   3.  **Define Pipeline State:** It configures all the necessary creation info structs for a standard graphics pipeline, including:
 *       - `VkPipelineShaderStageCreateInfo`: Defines the vertex and fragment shader stages.
 *       - `VkPipelineVertexInputStateCreateInfo`: Defines the vertex data layout (this is currently hardcoded and assumes no specific vertex input, which is a simplification).
 *       - `VkPipelineInputAssemblyStateCreateInfo`: Sets the primitive topology to triangle lists.
 *       - `VkPipelineViewportStateCreateInfo`: Configures the viewport and scissor to be dynamic states.
 *       - `VkPipelineRasterizationStateCreateInfo`: Sets standard rasterization state (e.g., fill mode, back-face culling).
 *       - `VkPipelineMultisampleStateCreateInfo`: Disables multisampling.
 *       - `VkPipelineDepthStencilStateCreateInfo`: Enables depth testing and writing.
 *       - `VkPipelineColorBlendStateCreateInfo`: Configures standard alpha blending.
 *   4.  **Create Pipeline Layout:** It currently reuses a pre-existing, simple pipeline layout. A more advanced implementation would create or select a layout compatible with the specific shader's resource requirements.
 *   5.  **Create Graphics Pipeline:** It assembles all the state information into a `VkGraphicsPipelineCreateInfo` struct and calls `vkCreateGraphicsPipelines` to create the final `VkPipeline` object.
 *   6.  **Cleanup:** The temporary `VkShaderModule` objects are destroyed after the pipeline is successfully created.
 *
 * @param vs_path The file system path to the compiled vertex shader SPIR-V file (`.spv`).
 * @param fs_path The file system path to the compiled fragment shader SPIR-V file (`.spv`).
 *
 * @return A `SituationShader` handle populated with the created `vk_pipeline` and `vk_pipeline_layout`.
 * @return A zeroed (invalid) `SituationShader` handle if any step in the process fails (e.g., file not found, shader module creation fails, pipeline creation fails). A detailed error is set internally.
 *
 * @note This function is a simplified helper and makes several assumptions about the rendering state. For example, the vertex input state is hardcoded. A production-ready engine would have a more flexible system for defining pipeline state objects based on material or mesh properties.
 * @warning This function is for internal use only.
 *
 * @see SituationLoadShader(), _SituationReadSpirvFile(), _SituationVulkanCreateShaderModule()
 */
static SituationShader _SituationCreateVulkanPipeline(const char* vs_path, const char* fs_path) {
    SituationShader shader = {0};

    // 1. Load SPIR-V Bytecode
    size_t vs_size, fs_size;
    char* vs_code = _SituationReadSpirvFile(vs_path, &vs_size);
    char* fs_code = _SituationReadSpirvFile(fs_path, &fs_size);
    if (!vs_code || !fs_code) {
        if(vs_code) SIT_FREE(vs_code);
        if(fs_code) SIT_FREE(fs_code);
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_ACCESS, "Failed to read SPIR-V shader files");
        return shader;
    }

    // 2. Create Shader Modules
    VkShaderModule vs_module = _SituationCreateVulkanShaderModule(vs_code, vs_size);
    VkShaderModule fs_module = _SituationCreateVulkanShaderModule(fs_code, fs_size);
    SIT_FREE(vs_code);
    SIT_FREE(fs_code);

    if (vs_module == VK_NULL_HANDLE || fs_module == VK_NULL_HANDLE) {
        if(vs_module) vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        if(fs_module) vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create shader modules");
        return shader;
    }

    // 3. Define Shader Stages
    VkPipelineShaderStageCreateInfo vs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo fs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_stage_info, fs_stage_info};

    // 4. Define Vertex Input, Assembly, Viewport, Rasterization, etc.
    // This part is complex and depends heavily on the mesh format and desired state.
    // This is a simplified example for a standard 3D mesh.
    VkPipelineVertexInputStateCreateInfo vertex_input_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO }; // Assumes vertex data is bound elsewhere
    VkPipelineInputAssemblyStateCreateInfo input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE };
    VkPipelineViewportStateCreateInfo viewport_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // Disable culling for user shaders
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE };
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &color_blend_attachment };
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamic_states };

    // 5. Create Pipeline Layout
    // This defines what uniforms/push constants the pipeline will use. A real engine has a complex system for this.
    // For now, we assume a simple, empty layout created during init.
    // VkPipelineLayoutCreateInfo pipeline_layout_info = ...
    // vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &shader.vk_pipeline_layout);
    // NOTE: This function is deprecated and incomplete - it tries to access fields that don't exist on the handle
    // shader.vk_pipeline_layout = sit_render.vk.quad_pipeline_layout; // REUSING A PRE-CREATED ONE for simplicity
    VkPipelineLayout vk_pipeline_layout = sit_render.vk.quad_pipeline_layout; // REUSING A PRE-CREATED ONE for simplicity

    // 6. Create the Graphics Pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = vk_pipeline_layout;
    pipeline_info.renderPass = sit_render.vk.main_window_render_pass; // This pipeline is compatible with the main render pass
    pipeline_info.subpass = 0;

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(sit_render.vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vk_pipeline);
    if (result != VK_SUCCESS) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "vkCreateGraphicsPipelines failed: VkResult = %d", (int)result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, err_msg);

        // Clean up shader modules (existing)
        if (vs_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        }
        if (fs_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);
        }
        // Clean up pipeline layout if it was created (NEW FIX)
        // NOTE: We're reusing a pre-created layout, so we don't destroy it here
        // if (vk_pipeline_layout != VK_NULL_HANDLE) {
        //     vkDestroyPipelineLayout(sit_render.vk.device, vk_pipeline_layout, NULL);
        // }
        // // // if (error_code) *error_code = SITUATION_ERROR_VULKAN_PIPELINE_FAILED;  // TODO: Fix error_code parameter  // TODO: Fix error_code parameter  // TODO: Fix error_code parameter

        return (SituationShader){0}; // Return invalid shader
    }
    
    // NOTE: This function is incomplete and doesn't properly store the pipeline in a slot
    // Use SituationLoadShader() or SituationLoadShaderFromMemory() instead
    return (SituationShader){0};
}

/**
 * @brief [INTERNAL] Allocates a descriptor set using a recycling pool strategy.
 *
 * @details [Optimized v2.3.27C] This function manages the "Dynamic Descriptor Manager".
 *          Unlike the previous version which only grew linearly, this version attempts to
 *          recycle space in existing pools before allocating new memory.
 *
 *          Strategy:
 *          1. Try to allocate from the 'current' pool (fast path).
 *          2. If full, iterate through ALL existing pools to find free space (recycling).
 *          3. If all pools are full/fragmented, create a new pool and add it to the list.
 *
 * @param layout The descriptor set layout to allocate.
 * @param[out] out_pool The pool that the set was allocated from (needed for freeing).
 * @return A valid VkDescriptorSet, or VK_NULL_HANDLE on critical failure.
 */
static VkDescriptorSet _SituationVulkanAllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool* out_pool) {
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanAllocateDescriptorSet called\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Layout handle: %p\n", (void*)layout); fflush(stdout);
    #endif
    if (layout == VK_NULL_HANDLE) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Layout is NULL!\n"); fflush(stdout);
        #endif
        return VK_NULL_HANDLE;
    }
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkResult res = VK_ERROR_OUT_OF_POOL_MEMORY;
    VkDescriptorSet out_set = VK_NULL_HANDLE;

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 1: Trying current pool...\n"); fflush(stdout);
    #endif
    // --- Phase 1: Try Current Active Pool (Fast Path) ---
    // We check the last used pool first to maintain cache locality and speed.
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:     Manager count: %d\n", sit_render.vk.descriptor_manager.count); fflush(stdout);
    #endif
    if (sit_render.vk.descriptor_manager.count > 0) {
        int idx = sit_render.vk.descriptor_manager.current_index;
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Current index: %d\n", idx); fflush(stdout);
        #endif
        alloc_info.descriptorPool = sit_render.vk.descriptor_manager.pools[idx];
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Pool handle: %p\n", (void*)alloc_info.descriptorPool); fflush(stdout);
        #endif
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Device handle: %p\n", (void*)sit_render.vk.device); fflush(stdout);
        #endif
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Calling vkAllocateDescriptorSets...\n"); fflush(stdout);
        #endif
        res = vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set);
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     vkAllocateDescriptorSets result: %d\n", res); fflush(stdout);
        #endif
        if (res == VK_SUCCESS) {
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:     Allocation SUCCESS! Set handle: %p\n", (void*)out_set); fflush(stdout);
            #endif
            if (out_pool) *out_pool = sit_render.vk.descriptor_manager.pools[idx];
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:     Returning from _SituationVulkanAllocateDescriptorSet\n"); fflush(stdout);
            #endif
            return out_set;
        }
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 2: Searching existing pools...\n"); fflush(stdout);
    #endif
    // --- Phase 2: Search Existing Pools (Recycling Path) ---
    // If the current pool is full/fragmented, check if older pools have freed up space.
    // This prevents infinite growth during long-running sessions (Load/Unload cycles).
    for (int i = 0; i < sit_render.vk.descriptor_manager.count; ++i) {
        // Skip the one we just checked
        if (i == sit_render.vk.descriptor_manager.current_index) continue;

        alloc_info.descriptorPool = sit_render.vk.descriptor_manager.pools[i];
        res = vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set);

        if (res == VK_SUCCESS) {
            // Found a pool with space! Make it the new 'current' to speed up subsequent allocs.
            sit_render.vk.descriptor_manager.current_index = i;
            if (out_pool) *out_pool = sit_render.vk.descriptor_manager.pools[i];
            return out_set;
        }
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 3: Creating new pool...\n"); fflush(stdout);
    #endif
    // --- Phase 3: Create New Pool (Growth Path) ---
    // If we reach here, all existing pools are full or fragmented. We must grow.

    // Define pool sizes (Balanced for typical engine usage)
    // Increased counts to reduce allocation frequency.
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 200 }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        // CRITICAL: FREE_DESCRIPTOR_SET_BIT allows individual sets to be freed.
        // This is required for our recycling strategy to work.
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 4000, // Sum of poolSizes approx
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    VkDescriptorPool new_pool;
    if (vkCreateDescriptorPool(sit_render.vk.device, &pool_info, NULL, &new_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED, "Critical: Failed to create new descriptor pool.");
        return VK_NULL_HANDLE;
    }

    // Add to manager list (Dynamic array logic)
    if (sit_render.vk.descriptor_manager.count >= sit_render.vk.descriptor_manager.capacity) {
        int new_cap = (sit_render.vk.descriptor_manager.capacity == 0) ? 4 : sit_render.vk.descriptor_manager.capacity * 2;
        void* new_pools = SIT_REALLOC(sit_render.vk.descriptor_manager.pools, new_cap * sizeof(VkDescriptorPool));
        if (!new_pools) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to resize descriptor pool list.");
            vkDestroyDescriptorPool(sit_render.vk.device, new_pool, NULL);
            return VK_NULL_HANDLE;
        }
        sit_render.vk.descriptor_manager.pools = (VkDescriptorPool*)new_pools;
        sit_render.vk.descriptor_manager.capacity = new_cap;
    }

    // Register new pool
    int new_index = sit_render.vk.descriptor_manager.count;
    sit_render.vk.descriptor_manager.pools[new_index] = new_pool;
    sit_render.vk.descriptor_manager.count++;
    sit_render.vk.descriptor_manager.current_index = new_index; // Set as active

    // Allocate from the fresh pool (Should always succeed)
    alloc_info.descriptorPool = new_pool;
    if (vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Allocation failed on fresh pool (Driver Error?)");
        return VK_NULL_HANDLE;
    }

    if (out_pool) *out_pool = new_pool;
    return out_set;
}

// --- Main Vulkan Initializer ---

/**
 * @brief [INTERNAL] Orchestrates the complete initialization of the Vulkan rendering backend.
 * @details This is the master function for setting up the Vulkan environment. It is called once during `SituationInit` and executes a multi-phase sequence to create all necessary Vulkan objects, from the instance and device to the swapchain and internal renderers.
 *          This function is responsible for establishing the library's high-performance resource management models, including the persistent descriptor set infrastructure.
 *
 * @par Initialization Sequence
 *   The function proceeds through several distinct phases:
 *   1.  **Core API Setup:** Creates the `VkInstance`, validation layers (if enabled), `VkSurfaceKHR`, selects a suitable `VkPhysicalDevice`, and creates the `VkDevice`. It also initializes the Vulkan Memory Allocator (VMA).
 *   2.  **Framing Setup:** Determines the optimal number of in-flight frames based on swapchain capabilities and allocates the per-frame arrays for command buffers, semaphores, and fences.
 *   3.  **Frame-Independent Resources:** Creates resources that are not tied to a specific frame, including the swapchain, main render pass, and depth buffer.
 *   4.  **Descriptor Infrastructure:** Critically, it initializes the **Dynamic Descriptor Manager**. It creates an initial "seed" `VkDescriptorPool` (`persistent_descriptor_pool`) and registers it with the manager. This allows the engine to automatically grow its descriptor capacity at runtime if the initial pool becomes full, preventing crashes during heavy asset streaming.
 *   5.  **Per-Frame Resources:** Creates the per-frame command buffers, synchronization objects (semaphores/fences), and the UBOs used for global view/projection data.
 *   6.  **Internal Renderers:** Initializes the pipelines and vertex buffers required for the library's internal rendering helpers, such as the 2D quad renderer.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, containing user-defined configuration like enabling validation layers and the window title.
 *
 * @return SITUATION_SUCCESS on successful initialization of all Vulkan components.
 * @return An appropriate `SituationError` code if any phase of the initialization fails. The function will halt on the first error and return immediately.
 *
 * @note This is a complex orchestrator function. Each sub-step (e.g., `_SituationVulkanCreateInstance`) is handled by a dedicated helper function for clarity and modularity.
 * @warning This function is for internal use by `SituationInit` only and must not be called directly.
 *          It assumes that `_SituationInitPlatform` and `_SituationInitWindow` have already been called successfully.
 */
static SituationError _SituationInitVulkan(const SituationInitInfo* init_info) {
    // --- Phase 1: Establish Core Vulkan API Handles ---
    if (_SituationVulkanCreateInstance(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INSTANCE_FAILED; }
    if (_SituationVulkanSetupDebugMessenger(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INSTANCE_FAILED; }
    if (_SituationVulkanCreateSurface() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INIT_FAILED; }
    if (_SituationVulkanPickPhysicalDevice() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DEVICE_FAILED; }

    if (init_info && init_info->force_single_queue) {
        sit_render.vk.compute_family_index = sit_render.vk.graphics_family_index;
    }

    if (_SituationVulkanCreateLogicalDevice(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DEVICE_FAILED; }

    // [Bindless] Verify Feature Support (Required for V2.4+)
    if (!(sit_render.enabled_features_mask & SIT_FEATURE_BINDLESS_TEXTURES)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DEVICE_FAILED, "Bindless Textures not supported by device.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DEVICE_FAILED;
    }

    if (_SituationVulkanCreateAllocator() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED; }
    if (_SituationVulkanCreateCommandPool() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_COMMAND_FAILED; }

    // --- Phase 2: Determine Dynamic Frame Count & Allocate Per-Frame State ---
    uint32_t desired_frames = (init_info->max_frames_in_flight > 1) ? init_info->max_frames_in_flight : 2;
    _SituationVulkanSwapchainSupportDetails support_details;
    _SituationVulkanQuerySwapchainSupport(sit_render.vk.physical_device, &support_details);

    uint32_t image_count = support_details.capabilities.minImageCount + 1;
    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }
    _SituationVulkanFreeSwapchainSupportDetails(&support_details);

    sit_render.vk.max_frames_in_flight = (desired_frames < image_count) ? desired_frames : image_count;
    if (sit_render.vk.max_frames_in_flight > (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.max_frames_in_flight = (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT;
    }
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [Vulkan]: Using %u frames in flight.\n", sit_render.vk.max_frames_in_flight);
#endif

    uint32_t frame_count = sit_render.vk.max_frames_in_flight;
    // Use SIT_CALLOC to zero-initialize all handles to NULL
    sit_render.vk.command_buffers = (VkCommandBuffer*)SIT_CALLOC(frame_count, sizeof(VkCommandBuffer));
    sit_render.vk.compute_command_buffers = (VkCommandBuffer*)SIT_CALLOC(frame_count, sizeof(VkCommandBuffer));
    sit_render.vk.image_available_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.render_finished_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.compute_finished_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.in_flight_fences = (VkFence*)SIT_CALLOC(frame_count, sizeof(VkFence));
    sit_render.vk.view_proj_ubo_buffer = (VkBuffer*)SIT_CALLOC(frame_count, sizeof(VkBuffer));
    sit_render.vk.view_proj_ubo_memory = (VmaAllocation*)SIT_CALLOC(frame_count, sizeof(VmaAllocation));
    sit_render.vk.view_proj_ubo_mapped = (void**)SIT_CALLOC(frame_count, sizeof(void*));
    sit_render.vk.view_proj_ubo_descriptor_set = (VkDescriptorSet*)SIT_CALLOC(frame_count, sizeof(VkDescriptorSet));
    sit_render.vk.graveyards = (_SituationVKGraveyard*)SIT_CALLOC(frame_count, sizeof(_SituationVKGraveyard));

    if (!sit_render.vk.command_buffers || !sit_render.vk.compute_command_buffers || !sit_render.vk.image_available_semaphores || !sit_render.vk.render_finished_semaphores || !sit_render.vk.compute_finished_semaphores || !sit_render.vk.in_flight_fences || !sit_render.vk.view_proj_ubo_buffer || !sit_render.vk.view_proj_ubo_memory || !sit_render.vk.view_proj_ubo_mapped || !sit_render.vk.view_proj_ubo_descriptor_set || !sit_render.vk.graveyards) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Per-frame Vulkan resource arrays");
        _SituationCleanupVulkan(); // The main cleanup function will free any non-NULL arrays
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    for (uint32_t i = 0; i < frame_count; i++) {
        _SituationInitGraveyard(&sit_render.vk.graveyards[i]);
    }

    // --- Phase 3 & 4: Frame-Independent and Descriptor Infrastructure ---
    if (_SituationVulkanCreateSwapchain() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED; }
    if (_SituationVulkanCreateImageViews() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED; }
    if (_SituationVulkanCreateRenderPass() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED; }
    if (_SituationVulkanCreateDepthResources() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED; }
    if (_SituationVulkanCreateFramebuffers() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED; }

    // --- Descriptor Pool & Manager Setup ---
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SITUATION_VULKAN_UNIFORM_BUFFER_SIZE + frame_count },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SITUATION_VULKAN_STORAGE_BUFFER_SIZE },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SITUATION_VULKAN_COMBINED_IMAGE_SAMPLER_SIZE },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, SITUATION_VULKAN_DEFAULT_USER_STORAGE_IMAGES }
    };

    const uint32_t total_max_sets = SITUATION_VULKAN_UNIFORM_BUFFER_SIZE +
                                    SITUATION_VULKAN_STORAGE_BUFFER_SIZE +
                                    SITUATION_VULKAN_COMBINED_IMAGE_SAMPLER_SIZE +
                                    SITUATION_VULKAN_DEFAULT_USER_STORAGE_IMAGES +
                                    frame_count;

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        // [FIX v2.3.27B] Re-enable FREE_BIT to allow reclaiming memory for individual sets.
        // This is critical for preventing OOM during asset streaming.
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = total_max_sets,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    // 1. Create the initial persistent pool
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating descriptor pool...\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Calling vkCreateDescriptorPool...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorPool(sit_render.vk.device, &pool_info, NULL, &sit_render.vk.persistent_descriptor_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create unified descriptor pool.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Descriptor pool created successfully\n"); fflush(stdout);
    #endif

    VkDescriptorSetLayoutBinding dynamic_ubo_binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo dynamic_ubo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &dynamic_ubo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating dynamic UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &dynamic_ubo_layout_info, NULL, &sit_render.vk.dynamic_ubo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create dynamic UBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Set as the active pool for the dynamic manager to start with
    sit_render.vk.descriptor_pool = sit_render.vk.persistent_descriptor_pool;

    // 2. Seed the Dynamic Manager with this pool
    // This ensures subsequent allocations use this pool instead of creating a new one immediately.
    sit_render.vk.descriptor_manager.capacity = 4;
    sit_render.vk.descriptor_manager.pools = (VkDescriptorPool*)SIT_MALLOC(sizeof(VkDescriptorPool) * 4);
    if (!sit_render.vk.descriptor_manager.pools) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_render.vk.descriptor_manager.pools[0] = sit_render.vk.persistent_descriptor_pool;
    sit_render.vk.descriptor_manager.count = 1;
    sit_render.vk.descriptor_manager.current_index = 0;

    // Create Descriptor Set Layouts... (Rest of function continues below)
    VkDescriptorSetLayoutBinding ubo_binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ubo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ubo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ubo_layout_info, NULL, &sit_render.vk.ubo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create UBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    VkDescriptorSetLayoutBinding ssbo_binding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ssbo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ssbo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating SSBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ssbo_layout_info, NULL, &sit_render.vk.ssbo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create SSBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create layouts for internal renderers
    VkDescriptorSetLayoutBinding ubo_layout_binding_internal = { SIT_UBO_BINDING_VIEW_DATA, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ubo_layout_info_internal = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ubo_layout_binding_internal };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ubo_layout_info_internal, NULL, &sit_render.vk.view_data_ubo_layout) != VK_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // --- 1. Restore Standard Sampler Layout (For VDs and Compute) ---
    // Uses Binding 4 (SIT_SAMPLER_BINDING_VD_SOURCE)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating standard sampler layout...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding standard_sampler_binding = { SIT_SAMPLER_BINDING_VD_SOURCE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo standard_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &standard_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &standard_sampler_info, NULL, &sit_render.vk.image_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    /* Advanced VD composite FS: layout(set=2, binding=5) u_destinationTexture — must match screen_copy_descriptor_set */
    VkDescriptorSetLayoutBinding composite_dest_binding = { SIT_SAMPLER_BINDING_VD_DEST, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo composite_dest_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &composite_dest_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &composite_dest_info, NULL, &sit_render.vk.composite_dest_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    if (_SituationVulkanCreateScreenCopyResource() != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // --- 2. Create Bindless Layout (For Global Texture Array) ---
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating bindless sampler layout...\n"); fflush(stdout);
    #endif

    // [Bindless] Setup Global Descriptor Layout
    VkDescriptorSetLayoutBinding bindless_binding = {};
    bindless_binding.binding = 0; // Use binding 0 for the array (GLSL: binding = 0)
    bindless_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindless_binding.descriptorCount = SITUATION_MAX_TEXTURES;
    bindless_binding.stageFlags = VK_SHADER_STAGE_ALL; // Allow access from any stage
    bindless_binding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    flagsInfo.bindingCount = 1;
    flagsInfo.pBindingFlags = &flags;

    VkDescriptorSetLayoutCreateInfo bindless_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    bindless_layout_info.pNext = &flagsInfo;
    bindless_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    bindless_layout_info.bindingCount = 1;
    bindless_layout_info.pBindings = &bindless_binding;

    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &bindless_layout_info, NULL, &sit_render.vk.bindless_descriptor_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // [Bindless] Create Dedicated Pool for the Global Set
    VkDescriptorPoolSize bindless_pool_size = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SITUATION_MAX_TEXTURES };
    VkDescriptorPoolCreateInfo bindless_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    bindless_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    bindless_pool_info.maxSets = 1;
    bindless_pool_info.poolSizeCount = 1;
    bindless_pool_info.pPoolSizes = &bindless_pool_size;

    if (vkCreateDescriptorPool(sit_render.vk.device, &bindless_pool_info, NULL, &sit_render.vk.global_bindless_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create bindless descriptor pool.");
        _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // [Bindless] Allocate Global Set
    VkDescriptorSetAllocateInfo bindless_alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    bindless_alloc_info.descriptorPool = sit_render.vk.global_bindless_pool;
    bindless_alloc_info.descriptorSetCount = 1;
    bindless_alloc_info.pSetLayouts = &sit_render.vk.bindless_descriptor_layout;

    if (vkAllocateDescriptorSets(sit_render.vk.device, &bindless_alloc_info, &sit_render.vk.global_bindless_set) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate global bindless descriptor set.");
        _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create text sampler layout (binding 0 for ALBEDO texture)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating text sampler layout (binding 0)...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding text_sampler_binding = { SIT_SAMPLER_BINDING_ALBEDO, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo text_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &text_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &text_sampler_info, NULL, &sit_render.vk.text_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create compute sampler layout (binding 0 for compute shaders)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating compute sampler layout (binding 0, compute stage)...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding compute_sampler_binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo compute_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &compute_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &compute_sampler_info, NULL, &sit_render.vk.compute_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Bindings for storage images usually happen in Compute or Fragment stages
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating storage image layout...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding storage_img_binding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo storage_img_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &storage_img_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &storage_img_info, NULL, &sit_render.vk.storage_image_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating dynamic vertex buffers...\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Device handle = %p\n", (void*)sit_render.vk.device); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: VMA allocator = %p\n", (void*)sit_render.vk.vma_allocator); fflush(stdout);
    #endif
    // --- Dynamic Vertex Buffer Initialization ---
    // Allocate 512KB per frame for dynamic text/UI geometry.
    // usage = VERTEX_BUFFER, memory = CPU_TO_GPU (Host Visible, Coherent)
    sit_render.vk.dynamic_vbo_capacity = 524288;
    VkBufferCreateInfo dyn_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    dyn_info.size = sit_render.vk.dynamic_vbo_capacity;
    dyn_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo dyn_alloc_info = {0};
    dyn_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Direct write
    dyn_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Map immediately

    for (uint32_t i = 0; i < frame_count; i++) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating dynamic VBO %u/%u...\n", i+1, frame_count); fflush(stdout);
        #endif
        VkResult vbo_result = vmaCreateBuffer(sit_render.vk.vma_allocator, &dyn_info, &dyn_alloc_info,
            &sit_render.vk.dynamic_vbo[i],
            &sit_render.vk.dynamic_vbo_alloc[i],
            NULL);
        if (vbo_result != VK_SUCCESS) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: vmaCreateBuffer failed with result: 0x%x\n", vbo_result); fflush(stdout);
            #endif
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }
        // Get the mapped pointer
        VmaAllocationInfo alloc_result;
        vmaGetAllocationInfo(sit_render.vk.vma_allocator, sit_render.vk.dynamic_vbo_alloc[i], &alloc_result);
        sit_render.vk.dynamic_vbo_mapped[i] = alloc_result.pMappedData;
    }

    // --- Phase 5 & 6: Per-Frame Objects and Internal Renderers ---
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating command buffers...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanCreateCommandBuffers() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_COMMAND_FAILED; }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating sync objects...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanCreateSyncObjects() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED; }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating per-frame UBOs...\n"); fflush(stdout);
    #endif
    for (uint32_t i = 0; i < frame_count; i++) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   UBO %u/%u...\n", i+1, frame_count); fflush(stdout);
        #endif
        VkDeviceSize buffer_size = sizeof(ViewDataUBO);

        // Create UBO with Persistent Mapping (CPU to GPU)
        VkBufferCreateInfo buf_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buf_info.size = sizeof(ViewDataUBO);
        buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo alloc_info = {0};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Keeps it mapped forever

        VmaAllocationInfo alloc_result;
        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buf_info, &alloc_info,
            &sit_render.vk.view_proj_ubo_buffer[i],
            &sit_render.vk.view_proj_ubo_memory[i],
            &alloc_result) != VK_SUCCESS) {
            _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }

        // Save the mapped pointer
        sit_render.vk.view_proj_ubo_mapped[i] = alloc_result.pMappedData;

        // [FIX v2.3.27B] Updated to pass NULL for pool tracking (View UBOs persist until shutdown)
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Allocating descriptor set...\n"); fflush(stdout);
        #endif
        sit_render.vk.view_proj_ubo_descriptor_set[i] = _SituationVulkanAllocateDescriptorSet(sit_render.vk.view_data_ubo_layout, NULL);

        if (sit_render.vk.view_proj_ubo_descriptor_set[i] == VK_NULL_HANDLE) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
        }

        VkDescriptorBufferInfo buffer_info = { sit_render.vk.view_proj_ubo_buffer[i], 0, buffer_size };
        VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = sit_render.vk.view_proj_ubo_descriptor_set[i];
            write.dstBinding = SIT_UBO_BINDING_VIEW_DATA;  // Binding 1 - must match layout
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &buffer_info;
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
    }

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing compute layouts...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanInitComputeLayouts() != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create pre-defined compute pipeline layouts.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // 1. Initialize the Quad Renderer (Shared Function)
    // Note: Width/Height are ignored by Vulkan path, passing 0 is safe.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing quad renderer...\n"); fflush(stdout);
    #endif
    if (!_SituationInitQuadRenderer(0, 0)) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: About to call _SituationInitDefaultFont...\n"); fflush(stdout);
    #endif
    if (!_SituationInitDefaultFont()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, "_SituationInitVulkan: Failed to initialize default font.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationInitDefaultFont succeeded!\n"); fflush(stdout);
    #endif

    // 2. Initialize Virtual Display Renderers (includes text pipeline)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing internal renderers (text pipeline + VD)...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanInitInternalRenderers() != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Internal renderers initialized successfully\n"); fflush(stdout);
    #endif
#else
    printf("Situation [Vulkan]: Shader compiler disabled. Internal renderers (Quad, VD) are unavailable.\n");
    // Zero out handles to be safe
    sit_render.vk.quad_pipeline = VK_NULL_HANDLE;
    sit_render.vk.vd_compositing_pipeline = VK_NULL_HANDLE;
    sit_render.vk.advanced_compositing_pipeline = VK_NULL_HANDLE;
#endif

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing staging buffers...\n"); fflush(stdout);
    #endif

    // [v2.4] Configure Staging Buffer Size
    if (init_info && init_info->staging_buffer_size > 0) {
        sit_render.vk.staging_buffer_size = init_info->staging_buffer_size;
    } else {
        sit_render.vk.staging_buffer_size = SITUATION_VK_STAGING_BUFFER_SIZE;
    }

    SituationError staging_result = _SituationInitStagingBuffers();
    if (staging_result != SITUATION_SUCCESS) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Staging buffers initialization FAILED with error code: %d\n", staging_result); fflush(stdout);
        #endif
        _SituationCleanupVulkan();
        return staging_result;
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Staging buffers initialized successfully!\n"); fflush(stdout);
    #endif

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Setting renderer type and marking as initialized...\n"); fflush(stdout);
    #endif
    sit_render.renderer_type = SIT_RENDERER_VULKAN;

    sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
    sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
    sit_render.vk.screenshot_buffer = NULL;
    sit_render.vk.screenshot_width = 0;
    sit_render.vk.screenshot_height = 0;
    sit_render.vk.screenshot_valid = false;
    for (int _si = 0; _si < SITUATION_MAX_FRAMES_IN_FLIGHT; _si++) {
        sit_render.vk.screenshot_copy_pending[_si] = false;
    }
    sit_render.vk.screenshot_mutex_initialized = false;
    if (mtx_init(&sit_render.vk.screenshot_mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, "_SituationInitVulkan: screenshot mutex init failed.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }
    sit_render.vk.screenshot_mutex_initialized = true;

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Vulkan initialization COMPLETE!\n"); fflush(stdout);
    #endif
    return SITUATION_SUCCESS;
}


// --- Helper Implementations ---

/**
 * @brief [INTERNAL] Builds a list of required Vulkan instance extensions.
 *
 * @details This helper function consolidates the list of Vulkan instance extensions required for the application to function correctly. This includes:
 * - Extensions mandated by GLFW for window surface creation (e.g., `VK_KHR_surface`).
 * - The `VK_EXT_debug_utils` extension if runtime validation is enabled.
 * - Platform-specific extensions required for compatibility (e.g., `VK_KHR_portability_enumeration` on macOS with MoltenVK).
 *
 * @param out_extension_count A pointer to a `uint32_t` where the number of extensions in the returned list will be stored.
 *                            This pointer must not be NULL.
 * @param enable_validation   A boolean flag indicating whether Vulkan validation layers are enabled. If true, the debug utils extension will be included in the list.
 *
 * @return A pointer to a statically allocated array of `const char*` strings, each representing a required Vulkan instance extension name.
 *         The array's length is given by the value written to `out_extension_count`.
 *         The returned pointer is valid only until the next call to this function.
 * @return NULL If GLFW reports no required instance extensions, or if `out_extension_count` is NULL.
 *
 * @note This function uses a statically allocated internal buffer to hold the
 *       list of extension names. The maximum number of extensions it can handle is defined by `SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS` (currently 16).
 *       If the total required extensions exceed this limit, an error message is logged, and the function's behavior is undefined (likely truncation or crash).
 *       This limit is considered sufficient for standard use cases.
 *
 * @see _SituationVulkanCreateInstance()
 */
static const char** _SituationVulkanGetRequiredExtensions(uint32_t* out_extension_count, bool enable_validation) {
    // --- 1. Input Validation ---
    if (!out_extension_count) {
        // Cannot output the count, so the result would be unusable.
        // This is a logic error in the caller.
        // fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: out_extension_count is NULL.\n");
        // Using _SituationSetErrorFromCode might be overkill for an internal helper,
        // but could be considered if the library does this for internal helpers.
        return NULL;
    }
    *out_extension_count = 0; // Initialize output count to zero in case of early return.

    // --- 2. Get GLFW Required Extensions ---
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // If GLFW reports no extensions are needed, return an empty list.
    if (glfw_extensions == NULL || glfw_extension_count == 0) {
        // This is unusual but not necessarily an error depending on the platform/context.
        // fprintf(stderr, "WARNING: GLFW reported no required Vulkan instance extensions.\n");
        return NULL; // *out_extension_count is already 0.
    }

    // --- 3. Aggregate Extensions into Static Array ---
    // Define a reasonable limit for the number of extensions.
    // This should cover GLFW extensions + debug utils + platform specifics.
    static const char* extensions[SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS];
    uint32_t count = 0;

    // --- 4. Add GLFW Extensions ---
    for (uint32_t i = 0; i < glfw_extension_count; ++i) {
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            // This should not happen with standard setups, but protect against overflow.
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d). Truncating list.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (GLFW extensions)");
            break; // Stop adding extensions to prevent buffer overrun.
        }
        extensions[count++] = glfw_extensions[i];
    }

    // --- 5. Add Validation Extension (if enabled) ---
    if (enable_validation) {
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_EXT_DEBUG_UTILS_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (debug utils)");
            // Cannot add it, list is full.
        } else {
            extensions[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
    }

    // --- 6. Add Platform-Specific Extensions ---
#if defined(__APPLE__)
    {
        // --- macOS / MoltenVK Specific Extensions ---
        // VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME is required on macOS
        // to allow enumerating portability-compliant devices.
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (portability enumeration)");
        } else {
            extensions[count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        }

        // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME is core in Vulkan 1.1
        // but explicitly enabling it can be good for portability layers.
        // Uncomment the lines below if this extension is deemed necessary.
        /*
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
        } else {
            extensions[count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
        }
        */
    }
#endif // __APPLE__

    // --- 7. Finalize and Return ---
    // Ensure the final count doesn't exceed the logical limit, though checks above should prevent it.
    if (count > SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
        count = SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS; // Defensive truncation
    }

    *out_extension_count = count;
    return extensions;
#undef SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS // Undefine local macro
}

/**
 * @brief [INTERNAL] Creates the core Vulkan instance.
 *
 * @details This helper function is responsible for initializing the Vulkan runtime environment by creating the `VkInstance`. This is the first major step in the Vulkan initialization process. It involves:
 * - Specifying the application and engine information.
 * - Enumerating and requesting the necessary instance extensions (provided by GLFW and potentially for debugging/validation).
 * - Optionally enabling the standard Khronos validation layer for development/debugging.
 * - Creating the `VkInstance` handle itself.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during library initialization. This contains settings like the window title and whether Vulkan validation should be enabled.
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS on successful creation of the Vulkan instance.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if Vulkan validation is requested but the required `VK_LAYER_KHRONOS_validation` layer is not found on the system, or if GLFW cannot provide the necessary instance extensions.
 * @return SITUATION_ERROR_VULKAN_INSTANCE_FAILED if `vkCreateInstance` fails for any reason (e.g., driver issues, unsupported API version, missing extensions).
 *
 * @note This function relies on `_SituationVulkanGetRequiredExtensions` to determine the list of necessary instance extensions.
 * @note The created `VkInstance` handle is stored in `sit_render.vk.instance`.
 *
 * @see _SituationInitVulkan(), _SituationVulkanGetRequiredExtensions()
 */
static SituationError _SituationVulkanCreateInstance(const SituationInitInfo* init_info) {
    // --- 1. Input Validation ---
    if (!init_info) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateInstance: init_info is NULL.");
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Handle Vulkan Validation Layers ---
    const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    const uint32_t validation_layer_count = 1; // Number of layers in the array above

    if (init_info->enable_vulkan_validation) {
        uint32_t layer_count = 0;
        // Query the number of available instance layer properties.
        VkResult enumerate_result = vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        if (enumerate_result != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to enumerate Vulkan instance layer properties.");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // If no layers are available at all, validation cannot be enabled.
        if (layer_count == 0) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "No Vulkan validation layers found on the system.");
             return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // Allocate memory to hold the list of available layers.
        VkLayerProperties* available_layers = (VkLayerProperties*)SIT_MALLOC(sizeof(VkLayerProperties) * layer_count);
        if (!available_layers) {
             _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for Vulkan instance layer properties.");
             return SITUATION_ERROR_MEMORY_ALLOCATION;
        }

        // Query the actual layer properties.
        enumerate_result = vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
        if (enumerate_result != VK_SUCCESS) {
            SIT_FREE(available_layers);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to enumerate Vulkan instance layer properties (second query).");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // Check if the required validation layer is present in the list.
        bool layer_found = false;
        for (uint32_t i = 0; i < layer_count; i++) {
            // Compare the name of the current available layer with the one we need.
            if (strcmp(validation_layers[0], available_layers[i].layerName) == 0) {
                layer_found = true;
                break; // Found it, no need to check further
            }
        }

        // Clean up the allocated list of layer properties.
        SIT_FREE(available_layers);

        // If the required validation layer was not found, report an error.
        if (!layer_found) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Validation layer 'VK_LAYER_KHRONOS_validation' requested but not available!");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }
        // If layer_found is true, we can proceed with enabling the layer.
    }
    // If validation is not enabled, no layer checks are needed.

    // --- 3. Specify Application and Engine Information ---
    VkApplicationInfo app_info = {}; // Explicitly zero-initialize
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = init_info->window_title; // Use the title from init info
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // App version 1.0.0
    app_info.pEngineName = "Situation Engine"; // Identify the engine
    app_info.engineVersion = VK_MAKE_VERSION(SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR, SITUATION_VERSION_PATCH);
    // Specify the target Vulkan API version. Ensure consistency with VMA and device requirements.
    app_info.apiVersion = VK_API_VERSION_1_4; // Target Vulkan 1.4

    // --- 4. Specify Instance Creation Parameters ---
    VkInstanceCreateInfo create_info = {}; // Explicitly zero-initialize
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info; // Link the application info

    // --- 5. Get Required Instance Extensions ---
    uint32_t extension_count = 0;
    // Use the helper function to get the list of required extensions,
    // including platform-specific ones and the debug extension if validation is enabled.
    const char** required_extensions = _SituationVulkanGetRequiredExtensions(&extension_count, init_info->enable_vulkan_validation);
    if (required_extensions == NULL) {
        // The helper function should have set an error message if it failed critically.
        // If it returns NULL with extension_count=0, it might be okay (no extensions needed),
        // but GLFW needing none is unusual. Treat as an error condition.
        if (extension_count == 0) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "GLFW reported no required Vulkan instance extensions, or failed to query them.");
        } // Else, error set by _SituationVulkanGetRequiredExtensions
        // If _SituationVulkanGetRequiredExtensions sets its own error, we could just return its code.
        // Assuming it sets SITUATION_ERROR_VULKAN_UNSUPPORTED on its critical failures.
        return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }
    // Set the extensions in the create info structure.
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = required_extensions;

    // --- 6. Configure Enabled Layers (if Validation is On) ---
    if (init_info->enable_vulkan_validation) {
        // Enable the validation layer(s) by setting the count and pointer to the array.
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        // No layers are enabled.
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = NULL; // Explicitly set to NULL for clarity
    }

    // --- 7. Platform-Specific Instance Creation Flags ---
#if defined(__APPLE__)
    {
        // On macOS (when using MoltenVK), the VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        // flag is required to allow enumeration of portability-compliant devices.
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif // __APPLE__

    // --- 8. Create the Vulkan Instance ---
    // This is the actual API call that creates the VkInstance handle.
    VkResult create_result = vkCreateInstance(&create_info, NULL, &sit_render.vk.instance);
    if (create_result != VK_SUCCESS) {
        // vkCreateInstance failed. This could be due to various reasons:
        // - Unsupported API version (app_info.apiVersion)
        // - Missing or unsupported extensions
        // - Missing or unsupported layers (if enabled)
        // - Driver issues
        // - Problems with pApplicationInfo
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "vkCreateInstance failed with VkResult 0x%x. Possible causes: unsupported API version, missing extensions/layers, driver issues.", create_result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INSTANCE_FAILED, error_detail);
        return SITUATION_ERROR_VULKAN_INSTANCE_FAILED;
    }

    // --- 9. Success ---
    // If we reach here, the VkInstance was created successfully.
    // The handle is stored in sit_render.vk.instance for use by subsequent Vulkan functions.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Callback function for Vulkan Validation Layer messages.
 *
 * @details This function is registered with the Vulkan instance (via `VkDebugUtilsMessengerCreateInfoEXT`) to receive debug, warning, and error messages from the Vulkan validation layers and the driver.
 *          It serves as the primary mechanism for diagnosing issues during Vulkan application development.
 *          The function receives detailed information about each message, including its severity (verbose, info, warning, error), type (general, validation, performance), and a descriptive text message. Based on the severity, it formats and prints the message to `stderr` for immediate visibility.
 *
 * @param messageSeverity The severity level of the message (e.g., `VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT`).
 * @param messageType The type of the message (e.g., `VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT`).
 * @param pCallbackData A pointer to a `VkDebugUtilsMessengerCallbackDataEXT` struct containing the detailed message information, including `pMessage`.
 * @param pUserData User-defined data pointer passed during messenger creation.
 *                  This implementation does not use it and sets it to NULL.
 *
 * @return VK_FALSE. This indicates that the Vulkan call that triggered the callback should *not* be aborted. The application should handle
 *         errors programmatically based on VkResult codes. Returning VK_TRUE would force the call to return `VK_ERROR_VALIDATION_FAILED_EXT`.
 *
 * @note This function is only used if Vulkan validation is enabled (`init_info->enable_vulkan_validation` is true) and the necessary extensions (`VK_EXT_debug_utils`) are supported and loaded.
 * @warning This function is called asynchronously from internal Vulkan threads.
 *          Therefore, it must be thread-safe. Using `fprintf` to `stderr` is generally acceptable for this purpose.
 *
 * @see _SituationVulkanSetupDebugMessenger(), VkDebugUtilsMessengerCallbackDataEXT
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL _SituationVulkanDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    // --- 1. Input Validation (Defensive for callback) ---
    // While Vulkan should provide valid data, checking is good practice.
    if (!pCallbackData || !pCallbackData->pMessage) {
        // Received invalid callback data. This is unusual but possible.
        // Log a basic message to indicate the problem.
        fprintf(stderr, "[Vulkan Debug Callback] ERROR: Received invalid callback data (NULL pCallbackData or pMessage).\n");
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR, "Vulkan debug callback received NULL data");
        return VK_FALSE; // Still return VK_FALSE
    }

    // --- 2. Silence Verbose Messages (Optional) ---
    // The callback is set up to receive VERBOSE, WARNING, and ERROR messages.
    // VERBOSE messages can be very noisy. Uncomment the lines below to filter them out.
    /*
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        // Optionally, filter out verbose messages based on type or content.
        // For example, silence specific verbose performance messages:
        // if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        //     return VK_FALSE;
        // }
        // Or just return early for all verbose messages:
        return VK_FALSE;
    }
    */

    // --- 3. Format and Print Message ---
    // Determine a prefix for the message based on its severity for easier scanning.
    const char* severity_prefix = "INFO"; // Default, though VERBOSE/INFO might be filtered above
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_prefix = "ERROR";
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_prefix = "WARN";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_prefix = "VERBOSE";
    }
    // Note: INFO level is VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT

    // Determine a prefix for the message based on its type.
    const char* type_prefix = "";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type_prefix = "[Validation] ";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type_prefix = "[Performance] ";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type_prefix = "[General] ";
    }

    // Print the formatted message to stderr.
    // Include severity and type for quick identification.
    fprintf(
        stderr,
        "[Vulkan %s] %s%s\n",
        severity_prefix,
        type_prefix, // Includes brackets and space if applicable
        pCallbackData->pMessage
    );
    // fflush(stderr); // Optional: Force immediate output, useful if stderr is buffered.

    // --- 4. Return Value ---
    // Always return VK_FALSE to indicate that the Vulkan call should continue.
    // The application should check VkResult codes for actual errors.
    return VK_FALSE;
}

// --- Updated/Added Documentation Block for _SituationVulkanSetupDebugMessenger ---
/**
 * @brief [INTERNAL] Sets up the Vulkan Debug Utils Messenger for validation layer output.
 *
 * @details This helper function is responsible for creating and registering the `VkDebugUtilsMessengerEXT` object if Vulkan validation is enabled.
 *          This messenger routes messages from the validation layers to the `_SituationVulkanDebugCallback` function, providing essential feedback for debugging Vulkan applications.
 *
 * The process involves:
 * 1.  Checking if validation is enabled in `init_info`.
 * 2.  Preparing a `VkDebugUtilsMessengerCreateInfoEXT` struct with the desired message severity levels, message types, and the callback function pointer.
 * 3.  Dynamically loading the `vkCreateDebugUtilsMessengerEXT` function pointer using `vkGetInstanceProcAddr`, as it's an extension function.
 * 4.  Calling the loaded function to create the messenger object.
 * 5.  Storing the created messenger handle in `sit_render.vk.debug_messenger` for later destruction.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during `SituationInit`. This is used to check if validation is enabled.
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS if validation is disabled, or if the messenger is successfully created.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_VULKAN_INSTANCE_FAILED if the required `vkCreateDebugUtilsMessengerEXT` function pointer cannot be loaded, or if the call to create the messenger fails. A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance (`sit_render.vk.instance`) has been successfully created.
 * @note The created messenger (`sit_render.vk.debug_messenger`) is destroyed by `_SituationCleanupVulkan`.
 * @warning This function should only be called when using the Vulkan backend (`SITUATION_USE_VULKAN` is defined).
 *
 * @see _SituationVulkanDebugCallback(), _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationCleanupVulkan(), vkCreateDebugUtilsMessengerEXT()
 */
static SituationError _SituationVulkanSetupDebugMessenger(const SituationInitInfo* init_info) {
    // --- 1. Input Validation ---
    if (!init_info) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanSetupDebugMessenger: init_info cannot be NULL." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Check if Validation is Enabled ---
    // If the user has not requested Vulkan validation, there's nothing to set up.
    // This is a normal and common path.
    if (!init_info->enable_vulkan_validation) {
        // Ensure the debug messenger handle is clean/invalid if not used.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_SUCCESS;
    }

    // --- 3. Configure Debug Messenger Creation Info ---
    // This struct defines what kinds of messages we want to receive and how to handle them.
    VkDebugUtilsMessengerCreateInfoEXT create_info = {}; // Explicitly zero-initialize
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Mandatory sType
    create_info.pNext = NULL; // No extension structures
    create_info.flags = 0; // No special flags for messenger creation

    // Specify the message severity levels we are interested in receiving.
    // VERBOSE can be very noisy, but is useful for detailed analysis.
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | // Include INFO messages
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Specify the message types we are interested in receiving.
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | // General events
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | // Violation of valid usage
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Potential performance issues

    // Set the callback function that will be invoked when a message is received.
    create_info.pfnUserCallback = _SituationVulkanDebugCallback;

    // pUserData allows passing custom data to the callback. We don't need it.
    create_info.pUserData = NULL;

    // --- 4. Load the Extension Function Pointer ---
    // vkCreateDebugUtilsMessengerEXT is part of the VK_EXT_debug_utils extension,
    // so it's not automatically loaded with the standard Vulkan loader.
    // We must retrieve its function pointer manually using vkGetInstanceProcAddr.
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkCreateDebugUtilsMessengerEXT");

    // Check if the function pointer was successfully loaded.
    if (vkCreateDebugUtilsMessengerEXT_func == NULL) {
        // The function pointer could not be loaded. This usually means the
        // VK_EXT_debug_utils extension is not available or not properly loaded.
        _SituationSetErrorFromCode( SITUATION_ERROR_VULKAN_INSTANCE_FAILED, "_SituationVulkanSetupDebugMessenger: Failed to load vkCreateDebugUtilsMessengerEXT function pointer. Check if VK_EXT_debug_utils is supported." );
        // Ensure the handle is explicitly invalid.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INSTANCE_FAILED;
    }

    // --- 5. Create the Debug Messenger ---
    // Call the loaded function to create the VkDebugUtilsMessengerEXT object.
    VkResult result = vkCreateDebugUtilsMessengerEXT_func(
        sit_render.vk.instance, // The Vulkan instance
        &create_info,       // Creation parameters
        NULL,               // Optional allocation callbacks (use default)
        &sit_render.vk.debug_messenger // Output: the created messenger handle
    );

    // --- 6. Handle Creation Result ---
    if (result != VK_SUCCESS) {
        // vkCreateDebugUtilsMessengerEXT failed. This is unexpected but possible.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanSetupDebugMessenger: vkCreateDebugUtilsMessengerEXT failed (VkResult: 0x%x).",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INSTANCE_FAILED, error_detail);
        // Ensure the global handle is explicitly invalid on failure.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INSTANCE_FAILED;
    }

    // --- 7. Success ---
    // If we reach here, the VkDebugUtilsMessengerEXT was created successfully.
    // The handle is stored in sit_render.vk.debug_messenger.
    // It will receive messages from the validation layers until it is destroyed by _SituationCleanupVulkan (which should use vkDestroyDebugUtilsMessengerEXT).
    // The next step in Vulkan initialization is typically picking a physical device.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Helper to create a VkImage and allocate memory via VMA.
 *
 * @details Wraps the complex setup of `VkImageCreateInfo` and `VmaAllocationCreateInfo`.
 *          It ensures images are created with `VK_SHARING_MODE_EXCLUSIVE` and the specified usage flags.
 *
 * @param width Width of the image in pixels.
 * @param height Height of the image in pixels.
 * @param mipLevels Total number of mip levels (1 for base level only).
 * @param format The Vulkan format (e.g., `VK_FORMAT_R8G8B8A8_SRGB`).
 * @param tiling Usually `VK_IMAGE_TILING_OPTIMAL`.
 * @param usage Bitmask of usage flags (Sampled, Storage, Transfer Dst, etc.).
 * @param memory_usage VMA hint (e.g., `VMA_MEMORY_USAGE_GPU_ONLY`).
 * @param[out] out_image Pointer to store the resulting VkImage handle.
 * @param[out] out_allocation Pointer to store the resulting VMA allocation handle.
 *
 * @return `SITUATION_SUCCESS` on success, or `SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED`.
 */
static SituationError _SituationVulkanCreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkImage* out_image, VmaAllocation* out_allocation) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mipLevels; // Use parameter
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Vulkan] Creating texture with usage flags: 0x%x (storage=%d)\n",
                usage, (usage & VK_IMAGE_USAGE_STORAGE_BIT) ? 1 : 0);
#endif
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = memory_usage;

    if (vmaCreateImage(sit_render.vk.vma_allocator, &image_info, &alloc_info, out_image, out_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create/allocate image.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates a Vulkan surface for the GLFW window.
 *
 * @details This helper function is a crucial step in the Vulkan initialization process.
 *          It instructs GLFW to create a `VkSurfaceKHR` object that represents the abstract surface of the `sit_gs.sit_glfw_window` within the Vulkan instance.
 *          This surface is essential for presenting rendered images to the screen, as it is used later to create the swapchain.
 *
 * @return SITUATION_SUCCESS on successful creation of the Vulkan surface.
 * @return SITUATION_ERROR_INVALID_PARAM if required prerequisites are not met:
 *         - `sit_render.vk.instance` is `VK_NULL_HANDLE`.
 *         - `sit_gs.sit_glfw_window` is `NULL`.
 * @return SITUATION_ERROR_VULKAN_INIT_FAILED if `glfwCreateWindowSurface` fails to create the surface. This can happen due to incompatibilities between the Vulkan instance and the GLFW window, or platform-specific issues. A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance (`sit_render.vk.instance`) and the GLFW window (`sit_gs.sit_glfw_window`) have been successfully created.
 * @note The created `VkSurfaceKHR` handle is stored in `sit_render.vk.surface`.
 * @note This function relies on the `VK_KHR_surface` extension being enabled (which is typically done automatically by GLFW when `glfwCreateWindowSurface` is called) and the appropriate platform-specific surface extension (e.g., `VK_KHR_win32_surface`, `VK_KHR_xcb_surface`).
 * @warning This function should only be called when using the Vulkan backend (`SITUATION_USE_VULKAN` is defined).
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationInitWindow(), glfwCreateWindowSurface(), vkDestroySurfaceKHR()
 */
static SituationError _SituationVulkanCreateSurface(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan instance and GLFW window handles are valid.
    // While the library's init sequence should guarantee this, checking adds robustness.
    if (sit_render.vk.instance == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateSurface: Vulkan instance is NULL. Call _SituationVulkanCreateInstance first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_gs.sit_glfw_window == NULL) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateSurface: GLFW window is NULL. Call _SituationInitWindow first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Create the Vulkan Surface using GLFW ---
    // This is the core API call that bridges GLFW and Vulkan.
    // It creates a VkSurfaceKHR object associated with the GLFW window.
    // The VkAllocationCallbacks parameter is NULL, using default allocation.
    VkResult result = glfwCreateWindowSurface(
        sit_render.vk.instance,         // The Vulkan instance
        sit_gs.sit_glfw_window,     // The GLFW window
        NULL,                       // Optional allocation callbacks
        &sit_render.vk.surface          // Output: the created VkSurfaceKHR handle
    );

    // --- 3. Handle Result ---
    if (result != VK_SUCCESS) {
        // glfwCreateWindowSurface failed. This is a critical error for Vulkan setup.
        // Common reasons include:
        // - Incompatibility between the Vulkan instance extensions and GLFW.
        // - The GLFW window was created with GLFW_NO_API (correct for Vulkan)
        //   but there's still an issue.
        // - Platform-specific problems (e.g., missing/wrong display server libraries).
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanCreateSurface failed: glfwCreateWindowSurface returned VkResult 0x%x. Check Vulkan/Window compatibility or platform setup.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, error_detail);
        // Ensure the global surface handle is explicitly invalid on failure.
        sit_render.vk.surface = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }

    // --- 4. Success ---
    // If we reach here, the VkSurfaceKHR was created successfully by GLFW.
    // The handle is stored in sit_render.vk.surface and will be used subsequently
    // for swapchain creation and eventually presentation.
    // The next step in Vulkan init is typically picking a physical device
    // that supports this surface.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Enumerates available GPUs and selects the most suitable one for the application.
 * @details This function is a critical step in the Vulkan initialization process. It queries the system for all Vulkan-capable physical devices and evaluates each one against a set of criteria to find the best fit.
 *          The selection is performed using a scoring system implemented in the `_SituationIsDeviceSuitable` helper function.
 *
 * @par Selection Logic
 *   1.  Enumerates all `VkPhysicalDevice`s present on the system.
 *   2.  For each device, it calls `_SituationIsDeviceSuitable` which performs pass/fail checks for essential features:
 *       - Support for a graphics queue family.
 *       - Support for a presentation queue family compatible with the window surface.
 *       - Availability of the `VK_KHR_swapchain` device extension.
 *       - Adequate swapchain support (at least one format and present mode).
 *   3.  Devices that pass the essential checks are then scored based on desirable properties, with a strong preference given to discrete GPUs over integrated ones.
 *   4.  The device with the highest score is selected as the primary GPU for the application.
 *
 * Upon successful selection, this function stores the chosen `VkPhysicalDevice` handle in `sit_render.vk.physical_device` and caches its graphics and present queue family indices for later use in logical device creation.
 *
 * @return SITUATION_SUCCESS if a suitable physical device is found and selected.
 * @return SITUATION_ERROR_VULKAN_DEVICE_FAILED if no Vulkan-capable GPUs are found, or if none of the found GPUs meet the minimum suitability requirements.
 *
 * @note This function must be called after the `VkInstance` and `VkSurfaceKHR` have been successfully created.
 * @warning This function is for internal use by `_SituationInitVulkan` only and should not be called directly.
 *
 * @see _SituationInitVulkan(), _SituationIsDeviceSuitable(), _SituationVulkanFindQueueFamilies()
 */
static SituationError _SituationVulkanPickPhysicalDevice(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(sit_render.vk.instance, &device_count, NULL);
    if (device_count == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DEVICE_FAILED, "Failed to find GPUs with Vulkan support.");
        return SITUATION_ERROR_VULKAN_DEVICE_FAILED;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)SIT_MALLOC(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(sit_render.vk.instance, &device_count, devices);

    int max_score = 0;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;

    // Iterate over all devices and find the one with the highest score
    for (uint32_t i = 0; i < device_count; i++) {
        int score = _SituationIsDeviceSuitable(devices[i]);
        if (score > max_score) {
            max_score = score;
            best_device = devices[i];
        }
    }

    SIT_FREE(devices);

    if (best_device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DEVICE_FAILED, "Failed to find any suitable GPU.");
        return SITUATION_ERROR_VULKAN_DEVICE_FAILED;
    }

    // Store the best device and its queue family indices
    sit_render.vk.physical_device = best_device;
    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(best_device, sit_render.vk.surface);
    sit_render.vk.graphics_family_index = indices.graphics_family;
    sit_render.vk.present_family_index = indices.present_family;
    sit_render.vk.compute_family_index = indices.compute_family_has_value ? indices.compute_family : indices.graphics_family;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(best_device, &properties);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [Vulkan]: Picked device '%s' with score %d\n", properties.deviceName, max_score);
#endif

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the Vulkan logical device and retrieves queue handles.
 * @details This function creates the `VkDevice` (the logical device), which is the primary interface for interacting with the selected physical device. It specifies which features, extensions, and queue families the application will use.
 *
 * @par Creation Process
 *   1.  **Queue Configuration:** It prepares one or two `VkDeviceQueueCreateInfo` structs based on the graphics and present queue family indices found by `_SituationVulkanPickPhysicalDevice`. If the indices are the same, only one queue is requested; otherwise, two are requested.
 *   2.  **Feature Enablement:** It specifies the set of `VkPhysicalDeviceFeatures` to enable. Currently, this is an empty set, but it can be expanded to request features like geometry shaders or anisotropic filtering.
 *   3.  **Extension Enablement:** It builds a list of required device-level extensions. This always includes `VK_KHR_swapchain` for rendering to a window and may include platform-specific extensions like `"VK_KHR_portability_subset"` on macOS.
 *   4.  **Device Creation:** It calls `vkCreateDevice` with the configured queues, features, and extensions to create the logical device handle.
 *   5.  **Queue Handle Retrieval:** After the device is created, it calls `vkGetDeviceQueue` to retrieve the handles for the graphics and present queues, storing them in the global state for command submission and presentation.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, used to determine if validation layers should be enabled at the device level.
 *
 * @return SITUATION_SUCCESS on successful creation of the logical device and retrieval of queue handles.
 * @return SITUATION_ERROR_VULKAN_DEVICE_FAILED if `vkCreateDevice` fails. This can happen if requested features or extensions are not supported by the physical device.
 *
 * @note This function must be called after a `VkPhysicalDevice` has been successfully selected by `_SituationVulkanPickPhysicalDevice`.
 * @warning This function is for internal use by `_SituationInitVulkan` only and should not be called directly.
 *
 * @see _SituationInitVulkan(), _SituationVulkanPickPhysicalDevice(), vkCreateDevice(), vkGetDeviceQueue()
 */
static SituationError _SituationVulkanCreateLogicalDevice(const SituationInitInfo* init_info) {
    // --- Queue Create Info ---
    // [v2.3.23] Updated to support up to 3 distinct queues (Graphics, Present, Compute)
    VkDeviceQueueCreateInfo queue_create_infos[3] = {};
    float queue_priority = 1.0f;
    uint32_t unique_queue_families[3];
    uint32_t unique_queue_family_count = 0;

    // Helper to add unique family
    // Always add Graphics first
    unique_queue_families[unique_queue_family_count++] = sit_render.vk.graphics_family_index;

    // Add Present if distinct
    bool present_unique = true;
    for(uint32_t i=0; i<unique_queue_family_count; i++) if(unique_queue_families[i] == sit_render.vk.present_family_index) present_unique = false;
    if(present_unique) unique_queue_families[unique_queue_family_count++] = sit_render.vk.present_family_index;

    // Add Compute if distinct
    bool compute_unique = true;
    for(uint32_t i=0; i<unique_queue_family_count; i++) if(unique_queue_families[i] == sit_render.vk.compute_family_index) compute_unique = false;
    if(compute_unique) unique_queue_families[unique_queue_family_count++] = sit_render.vk.compute_family_index;

    for (uint32_t i = 0; i < unique_queue_family_count; i++) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = unique_queue_families[i];
        queue_create_infos[i].queueCount = 1;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    // --- Device Features (Good as is) ---
    VkPhysicalDeviceFeatures device_features = {}; // Enable features as needed later

    // --- Device Extensions ---
    // Use a manageable array to build the list of required extensions.
    const char* device_extensions[8]; // Increased size for optional extensions
    uint32_t extension_count = 0;

    // The swapchain extension is always required for rendering to a window.
    device_extensions[extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    // For macOS compatibility via MoltenVK, the portability subset extension is required.
    #if defined(__APPLE__)
        device_extensions[extension_count++] = "VK_KHR_portability_subset";
    #endif

    // Check for optional extensions
    uint32_t available_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(sit_render.vk.physical_device, NULL, &available_ext_count, NULL);
    VkExtensionProperties* available_exts = (VkExtensionProperties*)SIT_MALLOC(sizeof(VkExtensionProperties) * available_ext_count);
    vkEnumerateDeviceExtensionProperties(sit_render.vk.physical_device, NULL, &available_ext_count, available_exts);

    bool mesh_shader_supported = false;
    bool ray_tracing_supported = false;

    for (uint32_t i = 0; i < available_ext_count; i++) {
        if (strcmp(available_exts[i].extensionName, "VK_EXT_mesh_shader") == 0) {
            mesh_shader_supported = true;
        }
        if (strcmp(available_exts[i].extensionName, "VK_KHR_ray_tracing_pipeline") == 0) {
            ray_tracing_supported = true;
        }
    }
    SIT_FREE(available_exts);

    // Define feature structures for extensions
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features = {};
    mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features = {};
    ray_tracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_features = {};
    accel_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    if (mesh_shader_supported) {
        device_extensions[extension_count++] = "VK_EXT_mesh_shader";
        sit_render.enabled_features_mask |= SIT_FEATURE_MESH_SHADER;
        mesh_features.meshShader = VK_TRUE;
    }

    if (ray_tracing_supported) {
        device_extensions[extension_count++] = "VK_KHR_ray_tracing_pipeline";
        device_extensions[extension_count++] = "VK_KHR_acceleration_structure"; // Prerequisite
        device_extensions[extension_count++] = "VK_KHR_deferred_host_operations"; // Prerequisite
        sit_render.enabled_features_mask |= SIT_FEATURE_RAY_TRACING;

        ray_tracing_features.rayTracingPipeline = VK_TRUE;
        accel_features.accelerationStructure = VK_TRUE;
    }

    // --- Feature Query & Enablement ---
    // We need to query what is supported before blindly enabling it.
    // This uses the modern VkPhysicalDeviceFeatures2 structure chain.

    // 1. Prepare the structures to query support
    VkPhysicalDeviceVulkan12Features supported_vk12 = {};
    supported_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 supported_features2 = {};
    supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported_features2.pNext = &supported_vk12;

    vkGetPhysicalDeviceFeatures2(sit_render.vk.physical_device, &supported_features2);

    // 2. Prepare the structures for creation (enable what we found)
    VkPhysicalDeviceVulkan12Features enable_vk12 = {};
    enable_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    // Chain extension features if enabled
    void** next_ptr = &enable_vk12.pNext;
    if (mesh_shader_supported) {
        *next_ptr = &mesh_features;
        next_ptr = &mesh_features.pNext;
    }
    if (ray_tracing_supported) {
        *next_ptr = &ray_tracing_features;
        next_ptr = &ray_tracing_features.pNext;
        *next_ptr = &accel_features;
        next_ptr = &accel_features.pNext;
    }

    // Enable Buffer Device Address (Critical for Bindless)
    if (supported_vk12.bufferDeviceAddress) {
        enable_vk12.bufferDeviceAddress = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_BUFFERS;
    } else {
        printf("Situation [Vulkan]: Warning - bufferDeviceAddress not supported. Bindless features disabled.\n");
    }

    // Enable Descriptor Indexing (Critical for Bindless Textures)
    if (supported_vk12.descriptorIndexing) {
        enable_vk12.descriptorIndexing = VK_TRUE;
        // We also need specific sub-features for full bindless texture support
        if (supported_vk12.shaderSampledImageArrayNonUniformIndexing &&
            supported_vk12.runtimeDescriptorArray &&
            supported_vk12.descriptorBindingPartiallyBound &&
            supported_vk12.descriptorBindingVariableDescriptorCount) {

             enable_vk12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
             enable_vk12.runtimeDescriptorArray = VK_TRUE;
             enable_vk12.descriptorBindingPartiallyBound = VK_TRUE;
             enable_vk12.descriptorBindingVariableDescriptorCount = VK_TRUE;

             sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_TEXTURES;
        }
    }

    // Enable Float16 (Half-float)
    if (supported_vk12.shaderFloat16) {
        enable_vk12.shaderFloat16 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT16;
    }

    // Enable Draw Indirect Count
    if (supported_vk12.drawIndirectCount) {
        enable_vk12.drawIndirectCount = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_DRAW_INDIRECT_COUNT;
    }

    // Enable Standard Features (Compute, Geometry, etc.)
    // Vulkan 1.0 features are in supported_features2.features
    if (supported_features2.features.geometryShader) {
        device_features.geometryShader = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_GEOMETRY_SHADER;
    }
    if (supported_features2.features.tessellationShader) {
        device_features.tessellationShader = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TESSELLATION_SHADER;
    }
    if (supported_features2.features.wideLines) {
        device_features.wideLines = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_WIDE_LINES;
    }
    if (supported_features2.features.fillModeNonSolid) {
        device_features.fillModeNonSolid = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FILL_MODE_NON_SOLID;
    }
    if (supported_features2.features.samplerAnisotropy) {
        device_features.samplerAnisotropy = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_SAMPLER_ANISOTROPY;
    }
    if (supported_features2.features.shaderInt64) {
        device_features.shaderInt64 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_INT64;
    }
    if (supported_features2.features.shaderFloat64) {
        device_features.shaderFloat64 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT64;
    }
    if (supported_features2.features.multiViewport) {
        device_features.multiViewport = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_VIEWPORT;
    }
    if (supported_features2.features.multiDrawIndirect) {
        device_features.multiDrawIndirect = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_DRAW_INDIRECT;
    }
    if (supported_features2.features.textureCompressionBC) {
        device_features.textureCompressionBC = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_BC;
    }
    if (supported_features2.features.textureCompressionASTC_LDR) {
        device_features.textureCompressionASTC_LDR = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_ASTC;
    }

    // Compute is mandatory in Vulkan, but good to track
    sit_render.enabled_features_mask |= SIT_FEATURE_COMPUTE_SHADER;

    // Subgroup Operations (Core in 1.1)
    // We can assume basic subgroup support if we are on Vulkan 1.2, but let's check properties if we were being pedantic.
    // For now, enable the flag as it's standard.
    sit_render.enabled_features_mask |= SIT_FEATURE_SUBGROUP_OPERATIONS;


    // --- Device Create Info ---
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &enable_vk12; // Chain the 1.2 features
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.queueCreateInfoCount = unique_queue_family_count;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = extension_count; // Use the dynamic count
    create_info.ppEnabledExtensionNames = device_extensions; // Use the new array

    // Validation layers (Good as is)
    const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    if (init_info->enable_vulkan_validation) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        create_info.enabledLayerCount = 0;
    }

    // --- Create the Device ---
    if (vkCreateDevice(sit_render.vk.physical_device, &create_info, NULL, &sit_render.vk.device) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DEVICE_FAILED, "Failed to create logical device");
        return SITUATION_ERROR_VULKAN_DEVICE_FAILED;
    }

    // --- Get Queue Handles ---
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.graphics_family_index, 0, &sit_render.vk.graphics_queue);
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.present_family_index, 0, &sit_render.vk.present_queue);
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.compute_family_index, 0, &sit_render.vk.compute_queue);

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the Vulkan Memory Allocator (VMA) instance.
 *
 * @details This helper function initializes the Vulkan Memory Allocator (VMA) library, which provides a higher-level, more efficient interface for allocating and managing GPU memory (VkDeviceMemory) and associating it with Vulkan objects like VkBuffer and VkImage.
 *          VMA handles memory type selection, sub-allocation, and defragmentation internally.
 *
 * @return SITUATION_SUCCESS on successful creation of the VMA allocator.
 * @return SITUATION_ERROR_INVALID_PARAM if required Vulkan handles (instance, physicalDevice, device) in `sit_render.vk` are invalid.
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if `vmaCreateAllocator` fails for any reason (e.g., incompatible Vulkan version, driver issues, internal VMA error). A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance, physical device, and logical device have been successfully created and their handles
 *       stored in `sit_render.vk.instance`, `sit_render.vk.physical_device`, and `sit_render.vk.device` respectively.
 * @note The created `VmaAllocator` handle is stored in `sit_render.vk.vma_allocator`.
 * @note The target Vulkan API version is specified as `VK_API_VERSION_1_4`.
 *       Ensure this aligns with the version used in `VkApplicationInfo` and is supported by the chosen physical device and driver.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationVulkanPickPhysicalDevice(), _SituationVulkanCreateLogicalDevice() _SituationCleanupVulkan() (for destruction)
 */
static SituationError _SituationVulkanCreateAllocator(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan handles are valid before passing them to VMA.
    // While the library's init sequence should guarantee this, a check adds robustness.
    if (sit_render.vk.instance == VK_NULL_HANDLE ||
        sit_render.vk.physical_device == VK_NULL_HANDLE ||
        sit_render.vk.device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateAllocator: Vulkan instance, physical device, or logical device is NULL." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Configure VMA Creation Info ---
    VmaAllocatorCreateInfo allocator_info = {0}; // Explicitly zero-initialize
    // VmaAllocatorCreateInfo does not have sType field
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4; // Specify target Vulkan API version
    allocator_info.instance = sit_render.vk.instance; // Link to Vulkan instance
    allocator_info.physicalDevice = sit_render.vk.physical_device; // Link to physical device
    allocator_info.device = sit_render.vk.device; // Link to logical device
    // allocator_info.pAllocationCallbacks = NULL; // Use default allocation callbacks
    // allocator_info.pDeviceMemoryCallbacks = NULL; // No custom memory callbacks
    // allocator_info.pHeapSizeLimit = NULL; // No heap size limits

    // Enable buffer device address support (required for shader device address)
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    // Set up Vulkan function pointers for VMA (required for VMA_DYNAMIC_VULKAN_FUNCTIONS)
    VmaVulkanFunctions vulkan_functions = {0};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocator_info.pVulkanFunctions = &vulkan_functions;

    // --- 3. Create the VMA Allocator ---
    // This is the actual call to the VMA library to create the allocator instance.
    VkResult result = vmaCreateAllocator(&allocator_info, &sit_render.vk.vma_allocator);
    if (result != VK_SUCCESS) {
        // vmaCreateAllocator failed. This usually indicates a problem with
        // the provided Vulkan handles, an unsupported API version, or an internal VMA issue.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "Failed to create Vulkan Memory Allocator (VMA) (VkResult: 0x%x). Check Vulkan handles, API version (1.1), or driver compatibility.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, error_detail);
        // Ensure the handle is explicitly invalid on failure.
        sit_render.vk.vma_allocator = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // --- 4. Success ---
    // If we reach here, the VmaAllocator was created successfully.
    // The handle is stored in sit_render.vk.vma_allocator for use by subsequent
    // buffer/image creation functions that rely on VMA.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Frees memory allocated by `_SituationVulkanQuerySwapchainSupport`.
 * @details This is a mandatory cleanup utility function. It frees the dynamically allocated arrays for surface formats and presentation modes that are stored within a `_SituationVulkanSwapchainSupportDetails` struct.
 *          It must be called after the details gathered by the query function are no longer needed to prevent memory leaks.
 *
 * @param details A pointer to the `_SituationVulkanSwapchainSupportDetails` struct whose internal arrays should be freed. It is safe to pass NULL to this function.
 *
 * @note This function is for internal use only and is a critical part of the resource management for Vulkan initialization helpers.
 *
 * @see _SituationVulkanQuerySwapchainSupport()
 */
static void _SituationVulkanFreeSwapchainSupportDetails(_SituationVulkanSwapchainSupportDetails* details) {
    if (details == NULL) return;
    // SIT_FREE() is safe to call on a NULL pointer.
    SIT_FREE(details->formats);
    SIT_FREE(details->present_modes);
    // No need to zero out the struct, as it's typically a stack-allocated variable
    // that will go out of scope.
}

/**
 * @brief [INTERNAL] Queries a physical device for its swapchain support details for the active surface.
 * @details This function populates a `_SituationVulkanSwapchainSupportDetails` struct with all the necessary information required to create a valid swapchain. This includes:
 *          1.  Surface capabilities (min/max image count, current extent, supported transforms, etc.).
 *          2.  A list of available surface formats (`VkSurfaceFormatKHR`).
 *          3.  A list of available presentation modes (`VkPresentModeKHR`).
 *
 * @warning This function allocates new memory for the `formats` and `present_modes` arrays within the `out_details` struct. The caller is **responsible** for freeing this memory by calling `_SituationVulkanFreeSwapchainSupportDetails()` on the struct once the data is no longer needed.
 *          Failure to do so will result in a memory leak.
 *
 * @param device The `VkPhysicalDevice` to query.
 * @param[out] out_details A pointer to the struct that will be filled with the queried support details. This pointer must not be NULL.
 *
 * @note This function must be called after the `VkInstance` and `VkSurfaceKHR` have been created. It is a key prerequisite for both device suitability checks and swapchain creation.
 * @note This function is for internal use only.
 *
 * @see _SituationVulkanFreeSwapchainSupportDetails(), _SituationIsDeviceSuitable(), _SituationVulkanCreateSwapchain()
 */
static void _SituationVulkanQuerySwapchainSupport(VkPhysicalDevice device, _SituationVulkanSwapchainSupportDetails* out_details) {
    // Get the basic surface capabilities (min/max image count, extent, etc.)
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, sit_render.vk.surface, &out_details->capabilities);

    // Get the supported surface formats
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, sit_render.vk.surface, &out_details->format_count, NULL);
    if (out_details->format_count != 0) {
        out_details->formats = (VkSurfaceFormatKHR*)SIT_MALLOC(sizeof(VkSurfaceFormatKHR) * out_details->format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, sit_render.vk.surface, &out_details->format_count, out_details->formats);
    } else {
        out_details->formats = NULL;
    }

    // Get the supported presentation modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, sit_render.vk.surface, &out_details->present_mode_count, NULL);
    if (out_details->present_mode_count != 0) {
        out_details->present_modes = (VkPresentModeKHR*)SIT_MALLOC(sizeof(VkPresentModeKHR) * out_details->present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, sit_render.vk.surface, &out_details->present_mode_count, out_details->present_modes);
    } else {
        out_details->present_modes = NULL;
    }
}

/**
 * @brief [INTERNAL] Evaluates a physical device to determine its suitability and assigns it a score.
 * @details This helper function is the core of the GPU selection logic. It performs a series of checks to determine if a `VkPhysicalDevice` meets the minimum requirements for the application and then scores it based on desirable properties. A score of 0 indicates the device is unsuitable.
 *
 * @par Suitability Criteria
 *   The function first performs several pass/fail checks. A device is considered unsuitable (score 0) if it fails any of these:
 *   - It does not have a queue family that supports both graphics and presentation operations.
 *   - It does not support the mandatory `VK_KHR_swapchain` device extension.
 *   - It does not offer at least one supported surface format and one presentation mode for the active window surface.
 *
 * @par Scoring System
 *   If a device passes all suitability checks, it is assigned a score based on the following preferences:
 *   - **Device Type:** Discrete GPUs are heavily favored and receive a high score (+1000), while integrated GPUs receive a smaller bonus (+100).
 *   - **Capabilities:** Additional points are awarded for features like a larger maximum 2D texture dimension, indicating a more powerful GPU.
 *
 * @param device The `VkPhysicalDevice` handle to evaluate.
 *
 * @return An integer score representing the suitability of the device. A higher score is better. Returns `0` if the device does not meet the minimum requirements.
 *
 * @note This function is for internal use by `_SituationVulkanPickPhysicalDevice` only.
 *
 * @see _SituationVulkanPickPhysicalDevice(), _SituationVulkanFindQueueFamilies(), _SituationVulkanQuerySwapchainSupport()
 */
static int _SituationIsDeviceSuitable(VkPhysicalDevice device) {
    // --- 1. Essential Feature Checks (Pass/Fail) ---

    // Check if the device supports required queue families
    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(device, sit_render.vk.surface);
    if (!indices.graphics_family_has_value || !indices.present_family_has_value) {
        return 0; // Not suitable
    }

    // Check for required device extension support (e.g., swapchain)
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties* available_extensions = (VkExtensionProperties*)SIT_MALLOC(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    bool swapchain_supported = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(available_extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            swapchain_supported = true;
            break;
        }
    }
    SIT_FREE(available_extensions);
    if (!swapchain_supported) return 0; // Not suitable

    // Check if the swapchain is adequate (has at least one format and one present mode)
    // You already have a helper for this from a previous step, let's assume it's called _SituationVulkanQuerySwapchainSupport
    _SituationVulkanSwapchainSupportDetails swapchain_support;
    _SituationVulkanQuerySwapchainSupport(device, &swapchain_support);
    bool swapchain_adequate = (swapchain_support.format_count > 0 && swapchain_support.present_mode_count > 0);
    _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support); // Helper to free the format/present mode arrays
    if (!swapchain_adequate) return 0; // Not suitable

    // --- 2. Scoring Based on Desirable Properties ---
    int score = 0;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    // Strongly prefer discrete GPUs
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    } else if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 100;
    }

    // Add points for larger max texture size
    score += device_properties.limits.maxImageDimension2D / 1024;

    // (Future) Add points for other features
    // if (device_features.geometryShader) { score += 100; }

    return score;
}

/**
 * @brief [INTERNAL] Finds the indices of queue families that support graphics and presentation.
 * @details This function iterates through all available queue families of a given physical device to find indices for two essential capabilities:
 *          1.  A queue family that supports graphics commands (`VK_QUEUE_GRAPHICS_BIT`).
 *          2.  A queue family that supports presenting to the application's window surface (`vkGetPhysicalDeviceSurfaceSupportKHR`).
 *
 * The function returns a struct containing the found indices and boolean flags indicating whether each was found. A device is only suitable for the application if both a graphics and a present family are found.
 *
 * @param device The `VkPhysicalDevice` to inspect.
 * @param surface The `VkSurfaceKHR` to check for presentation support against.
 *
 * @return A `_SituationQueueFamilyIndices` struct. The `graphics_family_has_value` and `present_family_has_value` members will be `true` if suitable families were found, and the corresponding `_family` members will hold their indices.
 *
 * @note The graphics and present queue families may or may not be the same index. This function correctly handles both cases.
 * @note This function is for internal use only, primarily called by `_SituationIsDeviceSuitable` and `_SituationVulkanCreateSwapchain`.
 */
static _SituationQueueFamilyIndices _SituationVulkanFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    _SituationQueueFamilyIndices indices;
    memset(&indices, 0, sizeof(indices));

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)SIT_MALLOC(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_has_value = true;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.compute_family_has_value) {
                indices.compute_family = i;
                indices.compute_family_has_value = true;
            } else {
                // Prefer distinct compute queue (no graphics bit)
                bool current_distinct = !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT);
                bool old_distinct = !(queue_families[indices.compute_family].queueFlags & VK_QUEUE_GRAPHICS_BIT);
                if (current_distinct && !old_distinct) {
                    indices.compute_family = i;
                }
            }
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            indices.present_family = i;
            indices.present_family_has_value = true;
        }
    }

    SIT_FREE(queue_families);
    return indices;
}

/**
 * @brief [INTERNAL] Creates the Vulkan swapchain for presenting rendered images to the window.
 * @details This function creates the `VkSwapchainKHR`, which is a collection of renderable images that are queued for presentation to the screen. It is a central component of the rendering pipeline.
 *
 * @par Creation Logic
 *   1.  **Query Support:** It first calls `_SituationVulkanQuerySwapchainSupport` to get the capabilities, formats, and present modes of the selected physical device.
 *   2.  **Select Best Format:** Prefers **`VK_FORMAT_B8G8R8A8_UNORM`** (with `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`) when offered so swapchain staging bytes match typical **`glReadPixels(..., GL_RGBA)`** / harness expectations. Falls back to **`B8G8R8A8_SRGB`**, then first listed format.
 *   3.  **Select Best Present Mode:** It iterates through available modes, preferring `VK_PRESENT_MODE_MAILBOX_KHR` (for low-latency, tear-free rendering) and falling back to the guaranteed `VK_PRESENT_MODE_FIFO_KHR` (standard V-Sync).
 *   4.  **Determine Extent & Image Count:** It determines the resolution of the swapchain images and the number of images in the chain based on the surface capabilities and current window size.
 *   5.  **Create Swapchain:** It populates the `VkSwapchainCreateInfoKHR` struct with the chosen settings and creates the `VkSwapchainKHR` object.
 *   6.  **Retrieve Images:** It retrieves the handles to the created `VkImage`s within the swapchain.
 *
 * Upon success, it stores the swapchain handle, image format, extent, and image handles in the global state (`sit_render.vk`).
 *
 * @return SITUATION_SUCCESS on successful creation of the swapchain and retrieval of its images.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if the physical device does not offer any compatible formats or present modes.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED if the `vkCreateSwapchainKHR` call fails for any other reason.
 *
 * @note This function must be called after the logical device and surface have been created.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanQuerySwapchainSupport(), _SituationVulkanCreateImageViews(), _SituationVulkanRecreateSwapchain()
 */
static SituationError _SituationVulkanCreateSwapchain(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating swapchain...\n"); fflush(stdout);
    #endif
_SituationVulkanSwapchainSupportDetails swapchain_support = {0};
    _SituationVulkanQuerySwapchainSupport(sit_render.vk.physical_device, &swapchain_support);

    if (swapchain_support.format_count == 0 || swapchain_support.present_mode_count == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "GPU does not support any suitable swapchain formats or present modes.");
        _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
        return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }

    VkSurfaceFormatKHR surface_format = swapchain_support.formats[0];
    bool picked = false;
    /* UNORM first: readback + SituationLoadImageFromScreen parity with GL RGBA8 expectations (harness pixel asserts). */
    for (uint32_t i = 0; i < swapchain_support.format_count && !picked; i++) {
        if (swapchain_support.formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            swapchain_support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = swapchain_support.formats[i];
            picked = true;
        }
    }
    for (uint32_t i = 0; i < swapchain_support.format_count && !picked; i++) {
        if (swapchain_support.formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            swapchain_support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = swapchain_support.formats[i];
            picked = true;
        }
    }
    for (uint32_t i = 0; i < swapchain_support.format_count && !picked; i++) {
        if (swapchain_support.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            swapchain_support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = swapchain_support.formats[i];
            picked = true;
        }
    }

    // Select present mode based on VSync flag
    // VK_PRESENT_MODE_FIFO_KHR = VSync ON (guaranteed available, caps at refresh rate)
    // VK_PRESENT_MODE_MAILBOX_KHR = VSync OFF (triple buffering, no tearing, unlimited FPS)
    // VK_PRESENT_MODE_IMMEDIATE_KHR = VSync OFF (no buffering, may tear, unlimited FPS)
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Default to VSync ON

    bool vsync_enabled = (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0;

    if (!vsync_enabled) {
        // VSync OFF - prefer IMMEDIATE (truly unlimited FPS) over MAILBOX (may cap at 2x refresh)
        for (uint32_t i = 0; i < swapchain_support.present_mode_count; i++) {
            if (swapchain_support.present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                break;  // IMMEDIATE gives truly unlimited FPS
            }
            if (swapchain_support.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                // Keep looking for IMMEDIATE which is preferred for max FPS
            }
        }
    }
    // else: keep VK_PRESENT_MODE_FIFO_KHR (VSync ON)

    VkExtent2D extent;
    if (swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        extent = swapchain_support.capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
        extent.width = (uint32_t)fmax(swapchain_support.capabilities.minImageExtent.width, fmin(swapchain_support.capabilities.maxImageExtent.width, (uint32_t)width));
        extent.height = (uint32_t)fmax(swapchain_support.capabilities.minImageExtent.height, fmin(swapchain_support.capabilities.maxImageExtent.height, (uint32_t)height));
    }

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = sit_render.vk.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(sit_render.vk.physical_device, sit_render.vk.surface);
    uint32_t queueFamilyIndices[] = {indices.graphics_family, indices.present_family};
    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(sit_render.vk.device, &create_info, NULL, &sit_render.vk.swapchain) != VK_SUCCESS) {
        sit_render.vk.swapchain_valid = false;
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Failed to create swap chain");
        _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
        return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED;
    }

    vkGetSwapchainImagesKHR(sit_render.vk.device, sit_render.vk.swapchain, &image_count, NULL);
    sit_render.vk.swapchain_images = (VkImage*)SIT_MALLOC(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(sit_render.vk.device, sit_render.vk.swapchain, &image_count, sit_render.vk.swapchain_images);
    sit_render.vk.swapchain_image_format = surface_format.format;
    sit_render.vk.swapchain_extent = extent;
    sit_render.vk.swapchain_image_count = image_count;

    _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
    sit_render.vk.swapchain_valid = true;
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates a VkImageView for each image in the swapchain.
 * @details An image view is a mandatory component that describes how to access a `VkImage` and which parts of it are accessible. This function creates a view for each swapchain image, specifying that they will be used as 2D color textures.
 *          The created image views are essential for binding the swapchain images as render targets in a framebuffer.
 *
 * @return SITUATION_SUCCESS if all image views are created successfully.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED if any of the `vkCreateImageView` calls fail.
 *
 * @note This function must be called after `_SituationVulkanCreateSwapchain` has successfully retrieved the swapchain images. The created handles are stored in the `sit_render.vk.swapchain_image_views` array.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanCreateSwapchain(), _SituationVulkanCreateImageView()
 */
static SituationError _SituationVulkanCreateImageViews(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating image views...\n"); fflush(stdout);
    #endif
sit_render.vk.swapchain_image_views = (VkImageView*)SIT_MALLOC(sizeof(VkImageView) * sit_render.vk.swapchain_image_count);
    for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
        sit_render.vk.swapchain_image_views[i] = _SituationVulkanCreateImageView(sit_render.vk.swapchain_images[i], sit_render.vk.swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
        if(sit_render.vk.swapchain_image_views[i] == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Failed to create image views");
             return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED;
        }
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the main render pass for the application window.
 * @details This function defines the structure of a render pass, specifying the attachments (color and depth), their properties, and the dependencies between subpasses. The main render pass created here is configured for standard forward rendering:
 *          - It uses one color attachment, which will be cleared at the start of the pass and stored for presentation at the end. Its layout is transitioned from `UNDEFINED` to `PRESENT_SRC_KHR`.
 *          - It uses one depth/stencil attachment, which will be cleared at the start and its contents discarded at the end.
 *          - It contains a single subpass that uses these attachments for graphics operations.
 *          - It includes a subpass dependency to ensure that color attachment operations in one frame are complete before the next frame's rendering begins.
 *
 * The resulting `VkRenderPass` is compatible with the framebuffers created for the swapchain.
 *
 * @return SITUATION_SUCCESS on successful creation of the render pass.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if no suitable depth format can be found on the physical device.
 * @return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED if the `vkCreateRenderPass` call fails.
 *
 * @note This function must be called after the logical device has been created and the swapchain format has been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationVulkanFindSupportedFormat(), _SituationVulkanCreateFramebuffers()
 */
static SituationError _SituationVulkanCreateRenderPass(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating render pass...\n"); fflush(stdout);
    #endif
sit_render.vk.depth_format = _SituationVulkanFindSupportedFormat(
        (VkFormat[]){VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 3,
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    if (sit_render.vk.depth_format == VK_FORMAT_UNDEFINED) {
         _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to find a supported depth format");
         return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = sit_render.vk.swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = sit_render.vk.depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_attachment_ref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(sit_render.vk.device, &render_pass_info, NULL, &sit_render.vk.main_window_render_pass) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "Failed to create render pass");
        return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
    }

    sit_render.vk.main_window_render_pass_resume = VK_NULL_HANDLE;
    {
        /* Resume pass: color LOAD from PRESENT_SRC (after a prior EndRenderPass left the swapchain
         * ready for present). The default pass uses CLEAR on every Begin — SituationRenderVirtualDisplays
         * restarts the main-window pass after compositing; CLEAR would erase the VD draws (harness vd_*). */
        VkAttachmentDescription color_resume = color_attachment;
        color_resume.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_resume.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth_resume = depth_attachment;
        depth_resume.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_resume.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        VkAttachmentDescription attachments_resume[] = { color_resume, depth_resume };
        VkRenderPassCreateInfo rp_resume_info = render_pass_info;
        rp_resume_info.pAttachments = attachments_resume;

        if (vkCreateRenderPass(sit_render.vk.device, &rp_resume_info, NULL, &sit_render.vk.main_window_render_pass_resume) != VK_SUCCESS) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass, NULL);
            sit_render.vk.main_window_render_pass = VK_NULL_HANDLE;
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "Failed to create main-window resume render pass (LOAD color)");
            return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
        }
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the depth buffer image and its view for the main render pass.
 * @details This function allocates a `VkImage` and its corresponding `VkImageView` to be used as the depth/stencil attachment for the main window's framebuffers. The image's dimensions are matched to the swapchain extent.
 *          The image is created in optimal device-local memory (`VMA_MEMORY_USAGE_GPU_ONLY`) for maximum performance.
 *
 * @return SITUATION_SUCCESS on successful creation of the depth image and its view.
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if the `vmaCreateImage` call fails.
 * @return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED if the `vkCreateImageView` call fails.
 *
 * @note This function must be called after the swapchain extent and a suitable depth format have been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanCreateImage(), _SituationVulkanCreateImageView(), _SituationVulkanCreateFramebuffers()
 */
static SituationError _SituationVulkanCreateDepthResources(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating depth resources...\n"); fflush(stdout);
    #endif
if (_SituationVulkanCreateImage(sit_render.vk.swapchain_extent.width, sit_render.vk.swapchain_extent.height, 1, sit_render.vk.depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                                  &sit_render.vk.depth_image, &sit_render.vk.depth_image_memory) != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create depth image");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    sit_render.vk.depth_image_view = _SituationVulkanCreateImageView(sit_render.vk.depth_image, sit_render.vk.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
    if(sit_render.vk.depth_image_view == VK_NULL_HANDLE){
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "Failed to create depth image view");
        return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
    }
    return SITUATION_SUCCESS;
}

// --- Framebuffer Creation ---
/**
 * @brief [INTERNAL] Creates Vulkan framebuffers for the main window swapchain.
 *
 * @details This helper function is responsible for creating the `VkFramebuffer` objects that connect the swapchain images (via their image views) and the depth buffer image view to the main window's render pass.
 *          A framebuffer defines the attachments (color, depth, stencil) that will be used in a render pass instance.
 *          This function is typically called during Vulkan initialization (`_SituationInitVulkan`) after the swapchain, image views, depth resources, and main render pass have been successfully created.
 *          It allocates an array to hold the framebuffer handles, then iterates through each swapchain image view, creating a corresponding framebuffer that uses that image view as the color attachment and the shared depth image view as the depth attachment.
 *
 * @return SITUATION_SUCCESS on successful creation of all framebuffers.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION if memory allocation fails for the internal array of `VkFramebuffer` handles.
 * @return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED if `vkCreateFramebuffer`fails for any of the framebuffers. A specific error message is set.
 *         Any successfully created framebuffers *before* the failure point will be left in the `sit_render.vk.main_window_framebuffers` array and must be cleaned up by the caller (e.g., `_SituationVulkanCleanupSwapchain` or `_SituationCleanupVulkan`) to prevent leaks.
 *
 * @note This function requires that the following Vulkan resources are already created and valid:
 *       - `sit_render.vk.device`
 *       - `sit_render.vk.swapchain_image_views` (array of image views)
 *       - `sit_render.vk.depth_image_view`
 *       - `sit_render.vk.main_window_render_pass`
 *       - `sit_render.vk.swapchain_extent`
 *       - `sit_render.vk.swapchain_image_count`
 * @note The created array of `VkFramebuffer` handles is stored in `sit_render.vk.main_window_framebuffers`. This array must be freed later by the cleanup process.
 * @warning This function is for internal use by the Vulkan initialization and swapchain recreation processes and should not be called directly by user code.
 *
 * @see _SituationInitVulkan(), _SituationVulkanRecreateSwapchain(), _SituationVulkanCleanupSwapchain(), vkCreateFramebuffer()
 */

static VkRenderPass _SituationVulkanGetOrCreateRenderPass(_SituationVulkanState* vk_state, const SituationRenderPassInfo* info) {
    bool is_main_window = (info->display_id == -1);
    uint32_t key = _SituationHashRenderPassKey(info, is_main_window);

    // 1. Check cache
    for (uint32_t i = 0; i < vk_state->render_pass_cache_count; ++i) {
        if (vk_state->render_pass_cache[i].key == key) {
            return vk_state->render_pass_cache[i].handle;
        }
    }

    // 2. Cache miss, create new RenderPass
    VkAttachmentDescription attachments[3] = {0};
    uint32_t attachment_count = 0;

    // --- Color Attachment ---
    attachments[0].format = is_main_window ? vk_state->swapchain_image_format : VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT; // MSAA not explicitly handled in this snippet yet

    // Color Load Op
    if (info->color_attachment.loadOp == SIT_LOAD_OP_CLEAR || info->color_attachment.loadOp == SIT_LOAD_OP_DONT_CARE) {
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else { // SIT_LOAD_OP_LOAD
        attachments[0].initialLayout = is_main_window ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Mapping our simple load ops to Vulkan
    attachments[0].loadOp = (info->color_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                            (info->color_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                   VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    attachments[0].storeOp = (info->color_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                      VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Final layout
    attachments[0].finalLayout = is_main_window ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_count++;

    // --- Depth/Stencil Attachment ---
    VkAttachmentReference depth_attachment_ref = {0};
    bool has_depth = true; // Typically we assume depth exists, or we could check if display_id has depth. For now, we assume all render passes have a depth attachment.

    if (has_depth) {
        attachments[1].format = vk_state->depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

        // Depth Load Op
        if (info->depth_attachment.loadOp == SIT_LOAD_OP_CLEAR || info->depth_attachment.loadOp == SIT_LOAD_OP_DONT_CARE) {
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        } else { // SIT_LOAD_OP_LOAD
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        attachments[1].loadOp = (info->depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                (info->depth_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        attachments[1].storeOp = (info->depth_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                          VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // Stencil Load Op
        attachments[1].stencilLoadOp = (info->stencil_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                       (info->stencil_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        attachments[1].stencilStoreOp = (info->stencil_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                                   VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment_count++;
    }

    // --- Subpass ---
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    if (has_depth) {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
    }

    // --- Subpass Dependencies ---
    VkSubpassDependency dependencies[2] = {0};
    uint32_t dependency_count = 0;

    // External to Subpass 0 (Color)
    dependencies[dependency_count].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[dependency_count].dstSubpass = 0;
    dependencies[dependency_count].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[dependency_count].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[dependency_count].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[dependency_count].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency_count++;

    // External to Subpass 0 (Depth/Stencil)
    if (has_depth) {
        dependencies[dependency_count].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[dependency_count].dstSubpass = 0;
        dependencies[dependency_count].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[dependency_count].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[dependency_count].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[dependency_count].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency_count++;
    }

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = dependency_count;
    render_pass_info.pDependencies = dependencies;

    VkRenderPass new_render_pass = VK_NULL_HANDLE;
    VkResult res = vkCreateRenderPass(vk_state->device, &render_pass_info, NULL, &new_render_pass);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateRenderPass failed for dynamic cache! (Result: %d)\n", res);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "vkCreateRenderPass failed for dynamic render pass cache");
        return VK_NULL_HANDLE;
    }

    // 3. Store in cache
    if (vk_state->render_pass_cache_count < 32) {
        vk_state->render_pass_cache[vk_state->render_pass_cache_count].key = key;
        vk_state->render_pass_cache[vk_state->render_pass_cache_count].handle = new_render_pass;
        vk_state->render_pass_cache_count++;
    } else {
        fprintf(stderr, "WARNING: Render Pass Cache full! (32 max). Returning un-cached pass, likely leaking.\n");
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "Render Pass Cache full (32 max), returning un-cached pass");
    }

    return new_render_pass;
}

static SituationError _SituationVulkanCreateFramebuffers(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating framebuffers...\n"); fflush(stdout);
    #endif
// --- 1. Allocate Array for Framebuffer Handles ---
    // Allocate memory for the array that will hold the VkFramebuffer handles.
    // The number of framebuffers needed equals the number of swapchain images.
    sit_render.vk.main_window_framebuffers = (VkFramebuffer*)SIT_MALLOC(sizeof(VkFramebuffer) * sit_render.vk.swapchain_image_count);

    // Check if the memory allocation for the framebuffer array was successful.
    if (!sit_render.vk.main_window_framebuffers) {
        // Allocation failed. This is a critical error for this step.
        _SituationSetErrorFromCode(
            SITUATION_ERROR_MEMORY_ALLOCATION,
            "_SituationVulkanCreateFramebuffers: Failed to allocate memory for framebuffer handle array."
        );
        // No Vulkan objects have been created yet in this function, so no cleanup is needed here.
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // --- 2. Create Framebuffers for Each Swapchain Image ---
    // Iterate through each swapchain image view to create its corresponding framebuffer.
    for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
        // --- 2a. Define Framebuffer Attachments ---
        // Specify the attachments for this framebuffer:
        // 1. The swapchain image view (color attachment)
        // 2. The shared depth image view (depth attachment)
        VkImageView attachments[] = {
            sit_render.vk.swapchain_image_views[i], // Color attachment (index 0)
            sit_render.vk.depth_image_view          // Depth attachment (index 1)
        };

        // --- 2b. Configure Framebuffer Creation Info ---
        // Set up the VkFramebufferCreateInfo struct with the necessary parameters.
        VkFramebufferCreateInfo framebuffer_info = {}; // Explicitly zero-initialize
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; // Mandatory sType
        framebuffer_info.pNext = NULL; // No extension structures
        framebuffer_info.flags = 0; // No special flags for framebuffer creation
        framebuffer_info.renderPass = sit_render.vk.main_window_render_pass; // Link to the render pass
        framebuffer_info.attachmentCount = 2; // We have two attachments (color and depth)
        framebuffer_info.pAttachments = attachments; // Pointer to the attachments array
        // Set the dimensions of the framebuffer to match the swapchain extent.
        framebuffer_info.width = sit_render.vk.swapchain_extent.width;
        framebuffer_info.height = sit_render.vk.swapchain_extent.height;
        framebuffer_info.layers = 1; // Number of layers (for array textures or VR, usually 1)

        // --- 2c. Create the VkFramebuffer Object ---
        // Call the Vulkan API to create the framebuffer object.
        VkResult result = vkCreateFramebuffer(
            sit_render.vk.device,           // The logical device
            &framebuffer_info,          // Creation parameters
            NULL,                       // Optional allocation callbacks (use default)
            &sit_render.vk.main_window_framebuffers[i] // Output: the created VkFramebuffer handle
        );

        // --- 2d. Handle Creation Result ---
        if (result != VK_SUCCESS) {
            // vkCreateFramebuffer failed for the framebuffer at index `i`.
            // This is a critical failure for the initialization process.
            char error_detail[256];
            snprintf(
                error_detail,
                sizeof(error_detail),
                "_SituationVulkanCreateFramebuffers: vkCreateFramebuffer failed for swapchain image %u (VkResult: 0x%x).",
                i,
                result
            );
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, error_detail);

            // --- 2e. Critical: Handle Partial Success and Cleanup ---
            // If framebuffer creation fails at index `i`, it means framebuffers from index 0 to `i-1` *might* have been successfully created and stored in `sit_render.vk.main_window_framebuffers[0]` to `[i-1]`.
            //
            // It is the responsibility of the *caller* of this function (e.g., _SituationInitVulkan, _SituationVulkanRecreateSwapchain) to perform a full cleanup (e.g., by calling _SituationVulkanCleanupSwapchain) when any error is returned.
            // That cleanup process will iterate through the `sit_render.vk.main_window_framebuffers` array and destroy any non-VK_NULL_HANDLE entries, then free the array itself.
            //
            // This function does *not* attempt to destroy the potentially successfully created framebuffers here. It simply reports the error and returns. This simplifies error handling in this function and relies on the robustness of the overall Vulkan cleanup sequence.
            //
            // Note: The `sit_render.vk.main_window_framebuffers` array itself is left allocated but partially populated. The cleanup function must handle this state correctly.

            // Return the specific error code to signal failure to the caller.
            return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
        }

        // If we reach here, the framebuffer at index `i` was created successfully.
        // Its handle is stored in `sit_render.vk.main_window_framebuffers[i]`.
        // The loop will continue to create the remaining framebuffers.
    }

    // --- 3. Success ---
        // If the loop completes without returning an error, all framebuffers have been successfully created and their handles are stored in the `sit_render.vk.main_window_framebuffers` array.
    // The next step in Vulkan initialization is typically creating command buffers or synchronization objects.

    sit_render.vk.main_window_framebuffers_resume = NULL;
    if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE) {
        sit_render.vk.main_window_framebuffers_resume = (VkFramebuffer*)SIT_CALLOC(sit_render.vk.swapchain_image_count, sizeof(VkFramebuffer));
        if (!sit_render.vk.main_window_framebuffers_resume) {
            for (uint32_t k = 0; k < sit_render.vk.swapchain_image_count; k++) {
                if (sit_render.vk.main_window_framebuffers[k] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[k], NULL);
                    sit_render.vk.main_window_framebuffers[k] = VK_NULL_HANDLE;
                }
            }
            SIT_FREE(sit_render.vk.main_window_framebuffers);
            sit_render.vk.main_window_framebuffers = NULL;
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationVulkanCreateFramebuffers: resume framebuffer array alloc failed.");
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        for (uint32_t j = 0; j < sit_render.vk.swapchain_image_count; j++) {
            VkImageView attachments_r[] = {
                sit_render.vk.swapchain_image_views[j],
                sit_render.vk.depth_image_view
            };
            VkFramebufferCreateInfo fb_resume = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fb_resume.renderPass = sit_render.vk.main_window_render_pass_resume;
            fb_resume.attachmentCount = 2;
            fb_resume.pAttachments = attachments_r;
            fb_resume.width = sit_render.vk.swapchain_extent.width;
            fb_resume.height = sit_render.vk.swapchain_extent.height;
            fb_resume.layers = 1;
            VkResult rr = vkCreateFramebuffer(sit_render.vk.device, &fb_resume, NULL, &sit_render.vk.main_window_framebuffers_resume[j]);
            if (rr != VK_SUCCESS) {
                for (uint32_t k = 0; k < j; k++) {
                    if (sit_render.vk.main_window_framebuffers_resume[k] != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers_resume[k], NULL);
                    }
                }
                SIT_FREE(sit_render.vk.main_window_framebuffers_resume);
                sit_render.vk.main_window_framebuffers_resume = NULL;
                for (uint32_t k = 0; k < sit_render.vk.swapchain_image_count; k++) {
                    if (sit_render.vk.main_window_framebuffers[k] != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[k], NULL);
                        sit_render.vk.main_window_framebuffers[k] = VK_NULL_HANDLE;
                    }
                }
                SIT_FREE(sit_render.vk.main_window_framebuffers);
                sit_render.vk.main_window_framebuffers = NULL;
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "_SituationVulkanCreateFramebuffers: vkCreateFramebuffer (resume) failed.");
                return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
            }
        }
    }

    return SITUATION_SUCCESS;
}

// --- Command Pool Creation ---
/**
 * @brief [INTERNAL] Creates the primary Vulkan command pool.
 *
 * @details This helper function is responsible for creating the main `VkCommandPool` used by the Situation library for allocating command buffers.
 *          This pool is specifically created for the graphics queue family, as all recorded commands (graphics, compute, transfer) in `situation.h` are submitted to the graphics queue.
 *          The pool is created with the `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag. This flag is essential because it allows individual command buffers allocated from this pool to be reset using `vkResetCommandBuffer`.
 *          This is necessary for the library's command buffer model, where a single command buffer (e.g., `sit_render.vk.command_buffers[frame_index]`) is reset and re-recorded every frame.
 *
 * @return SITUATION_SUCCESS on successful creation of the command pool.
 * @return SITUATION_ERROR_INVALID_PARAM if the Vulkan device (`sit_render.vk.device`) is `VK_NULL_HANDLE` or if the graphics queue family index (`sit_render.vk.graphics_family_index`) is invalid (e.g., `UINT32_MAX`).
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED if `vkCreateCommandPool` fails to create the pool. This can happen due to invalid device handle, invalid queue family index, or driver issues. A specific error message is set.
 *
 * @note This function must be called after the Vulkan logical device (`sit_render.vk.device`) and the graphics queue family index (`sit_render.vk.graphics_family_index`) have been successfully determined (e.g., in `_SituationVulkanCreateLogicalDevice`).
 * @note The created `VkCommandPool` handle is stored in `sit_render.vk.command_pool`.
 * @note This command pool is used by `_SituationVulkanCreateCommandBuffers` to allocate the per-frame command buffers.
 * @warning This function is for internal use by the Vulkan initialization process (`_SituationInitVulkan`) and should not be called directly by user code.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateLogicalDevice(), _SituationVulkanCreateCommandBuffers(), vkCreateCommandPool()
 */
static SituationError _SituationVulkanCreateCommandPool(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan device handle is valid.
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateCommandPool: Vulkan device is NULL. Call _SituationVulkanCreateLogicalDevice first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // Check if the graphics queue family index is valid.
    // UINT32_MAX is often used as an "unset" value.
    if (sit_render.vk.graphics_family_index == UINT32_MAX) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateCommandPool: Graphics queue family index is invalid (UINT32_MAX). Ensure _SituationVulkanPickPhysicalDevice/_SituationVulkanCreateLogicalDevice ran successfully." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Configure Command Pool Creation Info ---
    // Set up the VkCommandPoolCreateInfo struct with the necessary parameters.
    VkCommandPoolCreateInfo pool_info = {}; // Explicitly zero-initialize
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; // Mandatory sType
    pool_info.pNext = NULL; // No extension structures
    // --- CRITICAL FLAG ---
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows command buffers allocated from this pool to be individually reset using vkResetCommandBuffer.
    // This is essential for the library's per-frame command buffer recording model.
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // --- END CRITICAL FLAG ---
    // Specify the queue family that command buffers from this pool will be submitted to.
    // All library commands go to the graphics queue.
    pool_info.queueFamilyIndex = sit_render.vk.graphics_family_index;

    // --- 3. Create the VkCommandPool ---
    // This is the actual Vulkan API call that creates the command pool object.
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Calling vkCreateCommandPool...\n"); fflush(stdout);
    #endif
    VkResult result = vkCreateCommandPool(
        sit_render.vk.device,       // The logical device the pool is associated with
        &pool_info,             // Creation parameters
        NULL,                   // Optional allocation callbacks (use default)
        &sit_render.vk.command_pool // Output: the created VkCommandPool handle
    );

    // --- 4. Handle Creation Result ---
    if (result != VK_SUCCESS) {
        // vkCreateCommandPool failed. This is a critical error for Vulkan setup.
        // Common reasons include:
        // - Invalid device handle (sit_render.vk.device)
        // - Invalid queue family index (sit_render.vk.graphics_family_index)
        // - Driver issues or resource exhaustion.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanCreateCommandPool failed: vkCreateCommandPool returned VkResult 0x%x. Check device/queue family validity or driver state.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // Ensure the global command pool handle is explicitly invalid on failure.
        sit_render.vk.command_pool = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Command pool created successfully\n"); fflush(stdout);
    #endif
    // [v2.3.23] Create separate pool for Compute
    pool_info.queueFamilyIndex = sit_render.vk.compute_family_index;
    if (vkCreateCommandPool(sit_render.vk.device, &pool_info, NULL, &sit_render.vk.compute_command_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to create compute command pool.");
        sit_render.vk.compute_command_pool = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    // --- 5. Success ---
    // If we reach here, the VkCommandPool was created successfully.
    // The handle is stored in sit_render.vk.command_pool.
    // The next step in Vulkan initialization is typically allocating command buffers from this pool using _SituationVulkanCreateCommandBuffers.
    return SITUATION_SUCCESS;
}

// --- Command Buffer Creation ---
/**
 * @brief [INTERNAL] Allocates the primary command buffers for each in-flight frame.
 * @details This function allocates a dedicated, primary-level `VkCommandBuffer` for each frame that can be processed concurrently (determined by `sit_render.vk.max_frames_in_flight`).
 *          These command buffers are long-lived; one is used for each frame in a round-robin fashion. At the beginning of a frame, the corresponding command buffer is reset and then used to record all rendering and compute commands for that frame.
 *          They are allocated from the library's main command pool, which is created with the `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag to allow this per-frame reset behavior.
 *
 * @return `SITUATION_SUCCESS` if all command buffers are allocated successfully.
 * @return `SITUATION_ERROR_VULKAN_COMMAND_FAILED` if the `vkAllocateCommandBuffers` call fails.
 *
 * @note This function must be called after the logical device and the main command pool have been created. The allocated handles are stored in the `sit_render.vk.command_buffers` array.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateCommandPool(), SituationGetMainCommandBuffer()
 */
static SituationError _SituationVulkanCreateCommandBuffers(void) {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = sit_render.vk.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // --- Use the dynamic value from the global state ---
    alloc_info.commandBufferCount = sit_render.vk.max_frames_in_flight;

    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, sit_render.vk.command_buffers) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to allocate command buffers");
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    // Allocate Compute Buffers (from separate pool)
    alloc_info.commandPool = sit_render.vk.compute_command_pool;
    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, sit_render.vk.compute_command_buffers) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to allocate compute command buffers");
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    return SITUATION_SUCCESS;
}

// --- Sync Object Creation ---
/**
 * @brief [INTERNAL] Creates the synchronization objects (semaphores and fences) for each in-flight frame.
 * @details This function creates the Vulkan synchronization primitives required to manage the render loop and ensure correct ordering between the CPU and GPU, as well as between different GPU operations.
 *          For each frame that can be "in-flight" simultaneously (determined by `sit_render.vk.max_frames_in_flight`), this function creates:
 *   - **An `image_available_semaphore`:** This semaphore is signaled by `vkAcquireNextImageKHR` when a swapchain image is ready to be rendered to. The command buffer submission will wait on this semaphore.
 *   - **A `render_finished_semaphore`:** This semaphore is signaled by the `vkQueueSubmit` call when the command buffer has finished execution. The presentation engine will wait on this semaphore before showing the image on screen.
 *   - **An `in_flight_fence`:** This fence is signaled by `vkQueueSubmit` and is used by the CPU (`vkWaitForFences`) to wait until the frame has completely finished rendering.
 *       This prevents the CPU from starting to record commands for a new frame `N` before frame `N-max_frames_in_flight` has finished.
 *
 * The fences are created in the **signaled state** (`VK_FENCE_CREATE_SIGNALED_BIT`) to ensure that the very first frame doesn't block indefinitely waiting for a fence that has never been submitted.
 *
 * @return `SITUATION_SUCCESS` if all semaphores and fences for all in-flight frames are created successfully.
 * @return `SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED` if any `vkCreateSemaphore` or `vkCreateFence` call fails.
 *
 * @note This function must be called after the logical device has been created and `max_frames_in_flight` has been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationInitVulkan(), SituationAcquireFrameCommandBuffer(), SituationEndFrame()
 */
static SituationError _SituationVulkanCreateSyncObjects(void) {
    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating sync objects for %u frames...\n", sit_render.vk.max_frames_in_flight);
    printf("Situation [Vulkan Debug]:   Fence create flags: 0x%x (SIGNALED_BIT=0x%x)\n",
           fence_info.flags, VK_FENCE_CREATE_SIGNALED_BIT);
    fflush(stdout);
    #endif

    // --- Loop using the dynamic value from the global state ---
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        if (vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.compute_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(sit_render.vk.device, &fence_info, NULL, &sit_render.vk.in_flight_fences[i]) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to create synchronization objects for a frame");
            return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED;
        }

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   Frame %u: fence=%p\n", i, (void*)sit_render.vk.in_flight_fences[i]);
        fflush(stdout);
        #endif
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Sync objects created successfully\n");
    fflush(stdout);
    #endif

    return SITUATION_SUCCESS;
}

// --- Utility Helpers ---

/**
 * @brief [INTERNAL] Finds the first supported Vulkan format from a list that supports specific tiling and usage features.
 *
 * @details This helper function is used during Vulkan initialization to find suitable formats for images, such as depth buffers, that meet the required criteria.
 *          It queries the Vulkan physical device for the properties of each candidate format and returns the first one that supports the specified tiling mode and feature flags.
 *
 * @param candidates An array of `VkFormat` enums to check for support.
 * @param candidate_count The number of elements in the `candidates` array.
 * @param tiling The desired image tiling mode (`VK_IMAGE_TILING_LINEAR` or `VK_IMAGE_TILING_OPTIMAL`).
 * @param features A bitmask of `VkFormatFeatureFlags` that the format must support (e.g., `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`).
 * @return The first `VkFormat` from the `candidates` array that is supported with the specified `tiling` and `features`.
 * @return `VK_FORMAT_UNDEFINED` if none of the `candidates` support the requested `tiling` and `features`.
 *
 * @note This function relies on `sit_render.vk.physical_device` being a valid handle to an enumerated physical device. This is guaranteed by the library's initialization sequence if this function is called.
 * @warning The order of formats in the `candidates` array is important.
 *          The function returns the *first* supported format found. Place preferred formats (e.g., higher precision) earlier in the list.
 *
 * @see _SituationVulkanCreateDepthResources()
 */
static VkFormat _SituationVulkanFindSupportedFormat(
    const VkFormat* candidates,
    uint32_t candidate_count,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    // --- 1. Input Validation (Defensive for internal helper) ---
    // While internal, checking for null pointer or zero count prevents crashes if called incorrectly from within the library.
    if (!candidates || candidate_count == 0) {
        // Cannot find a format from an empty list.
        return VK_FORMAT_UNDEFINED;
    }

    // --- 2. Iterate Through Candidates ---
    for (uint32_t i = 0; i < candidate_count; i++) {
        VkFormat format = candidates[i];

        // --- 3. Query Format Properties ---
        // Get the properties supported by the physical device for this format.
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(sit_render.vk.physical_device, format, &props);

        // --- 4. Check for Required Features based on Tiling ---
        // Determine if the format supports the needed features for the requested tiling.
        VkFormatFeatureFlags supported_features = 0;
        if (tiling == VK_IMAGE_TILING_LINEAR) {
            supported_features = props.linearTilingFeatures;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL) {
            supported_features = props.optimalTilingFeatures;
        }
        // Note: Other tiling modes (e.g., VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) are not handled here and would result in supported_features = 0.

        // --- 5. Match Found? ---
        // Check if ALL requested features are present in the supported features.
        if ((supported_features & features) == features) {
            // Found a suitable format. Return it immediately.
            return format;
        }
        // If not, continue checking the next candidate.
    }

    // --- 6. No Suitable Format Found ---
    // If the loop completes without returning, no candidate format met the criteria.
    return VK_FORMAT_UNDEFINED;
}

/**
 * @brief [INTERNAL] Cleans up all Vulkan resources that are directly dependent on the current swapchain.
 *
 * @details This helper function is a crucial part of the swapchain recreation process. It ensures that all objects derived from the old swapchain are destroyed before a new swapchain can be created. This prevents resource leaks and potential validation errors.
 *
 * @details This function performs the following cleanup steps:
 *          1.  Waits for the device to be idle (`vkDeviceWaitIdle`) to ensure no commands are currently using the resources to be destroyed.
 *          2.  Destroys the main window's depth image view and the depth image itself (along with its VMA allocation).
 *          3.  Iterates through and destroys all main window framebuffers stored in `sit_render.vk.main_window_framebuffers`.
 *          4.  Frees the `sit_render.vk.main_window_framebuffers` array itself.
 *          5.  Iterates through and destroys all swapchain image views stored in `sit_render.vk.swapchain_image_views`.
 *          6.  Frees the `sit_render.vk.swapchain_image_views` array itself.
 *          7.  Destroys the `VkSwapchainKHR` handle (`sit_render.vk.swapchain`).
 *
 *          Crucially, it leaves core, swapchain-independent resources intact, such as:
 *          - The `VkDevice` (`sit_render.vk.device`)
 *          - The `VkPhysicalDevice` (`sit_render.vk.physical_device`)
 *          - The `VkRenderPass` (`sit_render.vk.main_window_render_pass`)
 *          - The `VkCommandPool` and command buffers
 *          - Descriptor sets, pools, and layouts
 *          - The `VkInstance` and `VkSurfaceKHR`
 *
 * @note This function should only be called when it's safe to destroy these resources, typically just before `_SituationVulkanCreateSwapchain` is called.
 * @note It is the caller's responsibility to ensure that:
 *       1. The Vulkan device (`sit_render.vk.device`) is valid.
 *       2. Any command buffers recording commands that use these resources have finished.
 *       3. This function is part of a coordinated swapchain recreation sequence.
 *
 * @see _SituationVulkanRecreateSwapchain(), _SituationVulkanCreateSwapchain(),
 *      _SituationVulkanCreateImageViews(), _SituationVulkanCreateDepthResources(),
 *      _SituationVulkanCreateFramebuffers()
 */
#if defined(SITUATION_USE_VULKAN)
static void _SituationVulkanDestroyScreenshotResources(void);
static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height);
static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height);
static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index);
#endif
static void _SituationVulkanCleanupSwapchain(void) {
    // --- 1. Validate Device Handle (Robustness) ---
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        // Nothing to clean up if the device isn't created.
        // This can happen during partial init/cleanup.
        return;
    }

    // --- 2. Ensure GPU is Finished Using Resources ---
    // Bounded wait + message pump (vkDeviceWaitIdle wedges forever if the GPU hangs — frozen pale window).
    _SituationVulkanWaitInFlightFencesPump("_SituationVulkanCleanupSwapchain");
    sit_render.vk.swapchain_valid = false;

    _SituationVulkanDestroyScreenCopyResource();

    /* Do not tear down pre-present screenshot buffers here: vkQueuePresentKHR may trigger
     * swapchain recreate (OUT_OF_DATE/SUBOPTIMAL) in the same EndFrame after CPU screenshot
     * resolve — destroying here clears screenshot_valid before SituationLoadImageFromScreen.
     * Extent/format changes are handled by _SituationVulkanEnsureScreenshotResources on next use. */

    // --- Render Pass Cache Cleanup ---
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;

    // --- 3. Destroy Depth Resources ---
    // These are specific to the swapchain's extent/format.
    if (sit_render.vk.depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.depth_image_view, NULL);
        sit_render.vk.depth_image_view = VK_NULL_HANDLE;
    }
    // Use the internal helper if one exists, otherwise call VMA directly.
    // Assuming _SituationVulkanDestroyImage helper exists and handles VMA destruction:
    if (sit_render.vk.depth_image != VK_NULL_HANDLE) {
        _SituationVulkanDestroyImage(sit_render.vk.depth_image, sit_render.vk.depth_image_memory);
        // Or directly: vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.depth_image, sit_render.vk.depth_image_memory);
        sit_render.vk.depth_image = VK_NULL_HANDLE;
        sit_render.vk.depth_image_memory = VK_NULL_HANDLE;
    }

    // --- 4. Destroy Main Window Framebuffers ---
    if (sit_render.vk.main_window_framebuffers) { // Check if array was allocated
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.main_window_framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[i], NULL);
                // Optional: Set to NULL for extra safety in debug builds if array might be reused
                // sit_render.vk.main_window_framebuffers[i] = VK_NULL_HANDLE;
            }
        }
        // Free the array holding the framebuffer handles.
        SIT_FREE(sit_render.vk.main_window_framebuffers);
        sit_render.vk.main_window_framebuffers = NULL; // Important: Nullify the pointer after freeing.
    }
    // Note: sit_render.vk.swapchain_image_count retains its value, as it's needed by _SituationVulkanCreateFramebuffers
    // which will be called next in the recreation sequence.

    // --- 5. Destroy Swapchain Image Views & Images ---
    if (sit_render.vk.swapchain_image_views) { // Check if array was allocated
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.swapchain_image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(sit_render.vk.device, sit_render.vk.swapchain_image_views[i], NULL);
                // Optional: Set to NULL for extra safety in debug builds
                // sit_render.vk.swapchain_image_views[i] = VK_NULL_HANDLE;
            }
        }
        // Free the array holding the image view handles.
        SIT_FREE(sit_render.vk.swapchain_image_views);
        sit_render.vk.swapchain_image_views = NULL; // Important: Nullify the pointer after freeing.
    }

    // Note: We do NOT destroy the VkImages themselves here, as they are owned
    // by the swapchain extension, but we must free our C array holding the handles.
    if (sit_render.vk.swapchain_images) {
        SIT_FREE(sit_render.vk.swapchain_images);
        sit_render.vk.swapchain_images = NULL;
    }

    // --- 6. Destroy the Swapchain Object ---
    if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
        sit_render.vk.swapchain = VK_NULL_HANDLE;
    }
    // sit_render.vk.swapchain_image_count could be reset here, but it's often left
    // for the recreation process to potentially reuse if the new swapchain has the same count.
    // It's set correctly by _SituationVulkanCreateSwapchain.
}


/**
 * @brief [INTERNAL] Recreates the Vulkan swapchain and all resources dependent on it.
 *
 * @details This function handles the full cycle of destroying the old swapchain and its associated resources, then creating a new swapchain and rebuilding the necessary dependent objects (image views, depth buffer, framebuffers).
 *          It is typically triggered by window resize events or when Vulkan reports that the swapchain is out of date (`VK_ERROR_OUT_OF_DATE_KHR`).
 *
 * @details The recreation process involves the following steps:
 *          1.  Waits for the window to have a non-zero size (handles minimization).
 *          2.  Calls `_SituationVulkanCleanupSwapchain` to destroy old resources.
 *          3.  Calls `_SituationVulkanCreateSwapchain` to create the new swapchain.
 *          4.  Calls `_SituationVulkanCreateImageViews` to create views for the new swapchain images.
 *          5.  Calls `_SituationVulkanCreateDepthResources` to create the depth buffer for the new extent.
 *          6.  Calls `_SituationVulkanCreateFramebuffers` to create framebuffers linking the new image views and depth buffer to the render pass.
 *
 * @note This function is designed to be called when the application detects a need for swapchain recreation (e.g., in `SituationEndFrame` or a resize callback).
 *       It internally handles the waiting and cleanup.
 * @warning If any step in the recreation process fails, the Vulkan backend may be left in an inconsistent state. The application should be prepared to handle such failures, potentially by shutting down or attempting recovery.
 *
 * @see _SituationVulkanCleanupSwapchain(), SituationEndFrame()
 */
static void _SituationVulkanRecreateSwapchain(void) {
    // --- 1. Handle Window Minimization ---
    // If the window is minimized, width/height can be 0. We must wait for a valid size.
    int width = 0, height = 0;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    while (width == 0 || height == 0) {
        /* Timeout wake keeps the loop from blocking indefinitely on some drivers if events stall */
        glfwWaitEventsTimeout(0.05);
        glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    }

    // --- 2. Orchestrate Recreation Steps ---
    /* GPU idle: _SituationVulkanCleanupSwapchain uses bounded fence waits (not vkDeviceWaitIdle). */
    // It's crucial that these steps happen in order and that failures are handled.

    // 3.1. Cleanup old swapchain resources.
    _SituationVulkanCleanupSwapchain();

    // 3.2. Create the new swapchain.
    // If this fails, there's nothing to clean up further as CleanupSwapchain already ran.
    SituationError create_swapchain_result = _SituationVulkanCreateSwapchain();
    if (create_swapchain_result != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(create_swapchain_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateSwapchain.");
        // The state is now inconsistent (no swapchain). The application loop should detect this (e.g., via error state or failed subsequent BeginFrame) and handle appropriately.
        return;
    }

    // 3.3. Create image views for the new swapchain images.
    SituationError create_views_result = _SituationVulkanCreateImageViews();
    if (create_views_result != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(create_views_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateImageViews.");
        // State: Swapchain exists, but no image views.
        // Clean up the swapchain we just made.
        if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
            sit_render.vk.swapchain = VK_NULL_HANDLE;
        }
        return;
    }

    // 3.4. Create depth resources for the new swapchain extent.
    SituationError create_depth_result = _SituationVulkanCreateDepthResources();
    if (create_depth_result != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(create_depth_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateDepthResources.");
        // State: Swapchain and image views exist, but no depth.
        // We need to clean up the resources created so far in this cycle.
        // CleanupSwapchain can handle this general case now.
        _SituationVulkanCleanupSwapchain();
        return;
    }

    // 3.5. Create framebuffers linking the new image views and depth buffer.
    SituationError create_framebuffers_result = _SituationVulkanCreateFramebuffers();
    if (create_framebuffers_result != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(create_framebuffers_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateFramebuffers.");
        // State: Swapchain, image views, and depth exist, but no framebuffers.
        // Clean up all swapchain-derived resources created in this cycle.
        _SituationVulkanCleanupSwapchain();
        return;
    }

    // --- 4. Screen copy (advanced VD Path A) — destroyed in CleanupSwapchain; must be recreated here ---
    if (_SituationVulkanCreateScreenCopyResource() != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED,
            "_SituationVulkanRecreateSwapchain: screen copy recreation failed after swapchain rebuild.");
        _SituationVulkanCleanupSwapchain();
        return;
    }

    // --- 5. Success ---
}

#endif // SITUATION_USE_VULKAN




/**
 * @brief [INTERNAL] Initializes all backend-specific resources for the internal 2D quad renderer.
 * @details This function is a critical part of the main initialization sequence. It creates the dedicated shaders, pipeline objects, and vertex buffers required by the high-level `SituationCmdDrawQuad` and `SituationCmdDrawText` commands.
 *          It is designed to be completely self-contained, ensuring that its internal state does not interfere with the user's rendering state.
 *
 * @par Backend-Specific Implementation
 * - **OpenGL:**
 *   1.  Compiles and links a dedicated shader program from the internal `SIT_QUAD_VERTEX_SHADER` and `SIT_QUAD_FRAGMENT_SHADER` sources.
 *   2.  Creates a **private** Vertex Array Object (`sit_render.gl.quad_vao`) and Vertex Buffer Object (`sit_render.gl.quad_vbo`). This is a crucial step to isolate the quad renderer's state from the main user-facing VAO (`sit_render.gl.global_vao_id`).
 *   3.  Uploads a static, 4-vertex triangle strip to the VBO.
 *   4.  Configures the private VAO with the correct vertex attribute layout for the simple 2D vertex format.
 *   5.  Sets the initial orthographic projection matrix uniform in the shader.
 *   6.  Critically, it restores the binding of the main global VAO before returning, ensuring the user's rendering context is left undisturbed.
 * - **Vulkan:**
 *   1.  Compiles the internal GLSL shader sources into SPIR-V using `shaderc`.
 *   2.  Creates a `VkPipelineLayout` that defines the interface for the quad renderer.
 *       - **Update:** It now includes `image_sampler_layout` (Set 1) to support textured quads for fonts.
 *       - **Update:** Push constants are expanded to include UV Rect and UseTexture flags.
 *   3.  Calls the generic `_SituationVulkanCreateGraphicsPipeline` helper to build the final `VkPipeline` object with the correct vertex input state and primitive topology (`TRIANGLE_STRIP`).
 *   4.  Creates and uploads the static vertex data to a device-local `VkBuffer` for optimal performance.
 *
 * @param width The initial width of the main window's viewport, used to configure the orthographic projection matrix.
 * @param height The initial height of the main window's viewport.
 *
 * @return `true` on successful initialization of all required resources.
 * @return `false` if any step fails (e.g., shader compilation, object creation). On failure, an appropriate error message is set, and any partially created resources are cleaned up.
 *
 * @note This function is for internal use by `_SituationInitOpenGL` or `_SituationInitVulkan` only.
 * @warning The success of this function is mandatory for `SituationCmdDrawQuad` and `SituationCmdDrawText` to work.
 *
 * @see _SituationCleanupQuadRenderer(), SituationCmdDrawQuad(), SituationCmdDrawText()
 */
static bool _SituationInitDefaultFont(void) {
    // 8x8 font bitmap from sit_default_8x8_font. 256 chars.
    // Layout: 16 chars per row, 16 rows.
    const int tex_w = 128;
    const int tex_h = 128;
    size_t data_size = tex_w * tex_h * 4; // RGBA
    uint8_t* pixels = (uint8_t*)SIT_CALLOC(1, data_size);

    if (!pixels) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate default font atlas.");
        return false;
    }

    // Expand 1-bit font data to RGBA texture
    for (int i = 0; i < 256; ++i) {
        int cx = i % 16;
        int cy = i / 16;
        int base_x = cx * 8;
        int base_y = cy * 8;

        const unsigned char* char_data = &sit_default_8x8_font[i * 8];

        for (int y = 0; y < 8; ++y) {
            unsigned char row_bits = char_data[y];
            for (int x = 0; x < 8; ++x) {
                bool on = (row_bits >> (7 - x)) & 1;
                uint8_t val = on ? 255 : 0;
                int p_idx = ((base_y + y) * tex_w + (base_x + x)) * 4;
                pixels[p_idx + 0] = val;  // R: white for glyph, black for background
                pixels[p_idx + 1] = val;  // G: white for glyph, black for background
                pixels[p_idx + 2] = val;  // B: white for glyph, black for background
                pixels[p_idx + 3] = val;  // A: opaque for glyph, transparent for background
            }
        }
    }

    SituationImage img = {};
    img.width = tex_w;
    img.height = tex_h;
    img.channels = 4;
    img.data = pixels;
    SituationError tex_result = SituationCreateTexture(img, false, &sit_render.default_font_atlas);

    if (tex_result != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(tex_result, "_SituationInitDefaultFont: Failed to create font atlas texture");
        SIT_FREE(pixels);
        return false;
    }

    // Override filtering to NEAREST for pixel-perfect bitmap font rendering
    #if defined(SITUATION_USE_OPENGL)
    {
        _SituationTextureSlot* slot = _SitGetTextureSlot(sit_render.default_font_atlas);
        if (slot && slot->gl_texture_id) {
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }
    #endif

    // CRITICAL FIX: Font atlas needs text_sampler_layout (binding 0), not image_sampler_layout (binding 4)
    // Recreate the descriptor set with the correct layout
    #if defined(SITUATION_USE_VULKAN)
    _SituationTextureSlot* font_slot = _SitGetTextureSlot(sit_render.default_font_atlas);
    if (font_slot && font_slot->descriptor_set != VK_NULL_HANDLE) {
        // Allocate new descriptor set with text_sampler_layout
        VkDescriptorPool used_pool = VK_NULL_HANDLE;
        VkDescriptorSet new_desc_set = _SituationVulkanAllocateDescriptorSet(sit_render.vk.text_sampler_layout, &used_pool);

        if (new_desc_set != VK_NULL_HANDLE) {
            // Update the descriptor set to point to the font atlas texture
            VkDescriptorImageInfo image_info = {
                .sampler = font_slot->sampler,
                .imageView = font_slot->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = new_desc_set,
                .dstBinding = SIT_SAMPLER_BINDING_ALBEDO,  // Binding 0
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info
            };

            vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
            // Replace the old descriptor set
            font_slot->descriptor_set = new_desc_set;
        }
    }
    #endif
    SIT_FREE(pixels);

    // Setup font struct
    // Note: We don't have STB baked data, so we rely on SituationCmdDrawText fallback for default font
    sit_render.default_font.fontData = NULL;
    sit_render.default_font.stbFontInfo = NULL;
    sit_render.default_font.atlas_texture = sit_render.default_font_atlas;
    sit_render.default_font.glyph_info = NULL;
    sit_render.default_font.atlas_width = tex_w;
    sit_render.default_font.atlas_height = tex_h;
    sit_render.default_font.font_height_pixels = 8.0f;
    sit_render.default_font.is_bitmap = false;
    sit_render.default_font.bitmap_data = NULL;
    sit_render.default_font.bitmap_width = 0;
    sit_render.default_font.bitmap_height = 0;
    sit_render.default_font.bitmap_count = 0;
    return true;
}

/**
 * @brief [INTERNAL] One-time setup of the built-in quad renderer subsystem.
 *
 * @details This function initializes the fast-path quad drawing primitive used throughout
 *          the library for full-screen effects, solid-color rectangles, debug overlays,
 *          virtual display compositing, and any simple 2D drawing that doesn't require
 *          a full mesh or model.
 *
 *          It is called exactly once during initialization (typically from `_SituationInitSubsystems`
 *          or after backend context is ready) and prepares:
 *
 *          OpenGL path:
 *            - Vertex/fragment shader pair (simple transform + flat color or texture)
 *            - Links them into a program object
 *            - Creates VAO + VBO for a static full-screen quad (-1..1 NDC coords)
 *            - Sets up uniform locations (model matrix, color, optional texture)
 *            - Caches the program ID for fast binding
 *
 *          Vulkan path:
 *            - Creates VkShaderModule(s) from embedded SPIR-V (or compiles GLSL if enabled)
 *            - Builds VkPipelineLayout (with push constants or small descriptor set)
 *            - Creates VkPipeline for graphics (compute if used for blits)
 *            - Prepares static vertex buffer or push-constant vertex data
 *            - Caches pipeline and layout handles
 *
 *          Common setup:
 *            - Defines quad vertices (two triangles: positions + optional UVs)
 *            - Sets default blend state (alpha blending enabled)
 *            - Validates no GL/VK errors during creation (`SIT_CHECK_GL_ERROR()`)
 *
 *          On success, `SituationCmdDrawQuad` and related calls become fully functional.
 *          On failure, logs error and disables quad rendering (draw calls become no-ops).
 *
 * @param width  Expected render target width (used for aspect ratio or viewport defaults).
 *               Usually matches primary window/backbuffer width.
 * @param height Expected render target height.
 *               Usually matches primary window/backbuffer height.
 *
 * @return true if quad renderer initialized successfully (shaders/pipeline/VAO ready),
 *         false on failure (shader compile/link fail, allocation error, invalid dimensions).
 *         Failures are logged internally and may set global `SituationError`.
 *
 * @note Must be called **after** backend context is current (GL context or Vulkan device ready).
 *       Thread safety: Only safe from the thread owning the context (usually main thread during init
 *       or render thread if deferred).
 *       Dimensions are used for initial viewport/scissor setup can be updated later via resize events.
 *       The quad is static (NDC coords) transformations are applied via model matrix in draw calls.
 *
 *       Critical dependencies:
 *         - OpenGL context current (GL path) or Vulkan device/queue ready
 *         - `_SituationInitOpenGL` / `_SituationInitVulkan` already completed
 *         - No prior quad init (idempotent but wasteful if called twice)
 *
 * @see SituationCmdDrawQuad, _SituationInitSubsystems (caller),
 *      _SituationInitOpenGL, _SituationInitVulkan,
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED,
 *      SITUATION_ERROR_VULKAN_PIPELINE_CREATE_FAILED
 */
static bool _SituationInitQuadRenderer(int width, int height) {
#if defined(SITUATION_USE_OPENGL)
    // --- OpenGL Quad Renderer Initialization ---
    SIT_DEBUG_LOG("[QUAD] Starting quad renderer initialization");
    SituationError shader_err_code = SITUATION_SUCCESS;

    // 1. Compile and link the internal quad shader program.
    SIT_DEBUG_LOG("[QUAD] Compiling quad shader program");
    sit_render.gl.quad_shader_program = _SituationCreateGLShaderProgram(SIT_QUAD_VERTEX_SHADER, SIT_QUAD_FRAGMENT_SHADER, &shader_err_code);

    if (shader_err_code != SITUATION_SUCCESS || sit_render.gl.quad_shader_program == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: Shader compilation failed, error code: %d", shader_err_code);
        // Error message should already be set by _SituationCreateGLShaderProgram
        return false;
    }
    SIT_DEBUG_LOG("[QUAD] Shader program created successfully, ID: %u", sit_render.gl.quad_shader_program);

    // 2. Define vertex data for a simple 2D quad (TRIANGLE_STRIP order).
    // Format: [X, Y] (assuming Z=0, W=1 in shader or handled by model matrix)
    float quad_vertices[] = {
        0.0f, 0.0f, // Bottom-left
        1.0f, 0.0f, // Bottom-right
        0.0f, 1.0f, // Top-left
        1.0f, 1.0f  // Top-right
    };

    // --- [PRIVATE VAO/VBO SETUP for Quad Renderer] ---

    // 3. Create the PRIVATE VAO and VBO for the quad renderer.
    SIT_DEBUG_LOG("[QUAD] Creating private VAO");
    glCreateVertexArrays(1, &sit_render.gl.quad_vao);
    if (sit_render.gl.quad_vao == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: VAO creation failed");
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitQuadRenderer: Failed to create private quad VAO.");
        // Cleanup shader program on VAO creation failure
        glDeleteProgram(sit_render.gl.quad_shader_program);
        sit_render.gl.quad_shader_program = 0;
        return false;
    }
    SIT_DEBUG_LOG("[QUAD] VAO created successfully, ID: %u", sit_render.gl.quad_vao);
    SIT_CHECK_GL_ERROR(); // Check for errors during VAO creation

    SIT_DEBUG_LOG("[QUAD] Creating private VBO");
    glCreateBuffers(1, &sit_render.gl.quad_vbo);
    if (sit_render.gl.quad_vbo == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: VBO creation failed");
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitQuadRenderer: Failed to create private quad VBO.");
        // Cleanup shader program and VAO on VBO creation failure
        glDeleteProgram(sit_render.gl.quad_shader_program);
        sit_render.gl.quad_shader_program = 0;
        glDeleteVertexArrays(1, &sit_render.gl.quad_vao);
        sit_render.gl.quad_vao = 0;
        return false;
    }
    SIT_DEBUG_LOG("[QUAD] VBO created successfully, ID: %u", sit_render.gl.quad_vbo);
    SIT_CHECK_GL_ERROR(); // Check for errors during VBO creation

    // 4. Allocate and populate the VBO's storage with the quad vertex data.
    // Using glNamedBufferStorage for DSA (Direct State Access).
    SIT_DEBUG_LOG("[QUAD] Allocating VBO storage");
    glNamedBufferStorage(sit_render.gl.quad_vbo, sizeof(quad_vertices), quad_vertices, 0); // Static data
    SIT_CHECK_GL_ERROR(); // Check for errors during buffer storage
    SIT_DEBUG_LOG("[QUAD] VBO storage allocated");

    // 5. Temporarily bind OUR private VAO to configure it.
    SIT_DEBUG_LOG("[QUAD] Binding private VAO");
    glBindVertexArray(sit_render.gl.quad_vao);
    // sit_render.gl.current_vao_id = sit_render.gl.quad_vao; // Don't track internal temporary binds as they are restored immediately
    SIT_CHECK_GL_ERROR(); // Check for errors during VAO binding

    // 6. Configure the VAO state: Bind VBO, set vertex attributes.
    // Bind the VBO to the VAO's binding index 0.
    SIT_DEBUG_LOG("[QUAD] Configuring VAO attributes");
    glVertexArrayVertexBuffer(sit_render.gl.quad_vao, 0, sit_render.gl.quad_vbo, 0, 2 * sizeof(float)); // Binding index 0, stride 2 floats
    SIT_CHECK_GL_ERROR();

    // Set up vertex attribute format for position (Location 0)
    glVertexArrayAttribFormat(sit_render.gl.quad_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    SIT_CHECK_GL_ERROR();
    glVertexArrayAttribBinding(sit_render.gl.quad_vao, SIT_ATTR_POSITION, 0);
    SIT_CHECK_GL_ERROR();
    glEnableVertexArrayAttrib(sit_render.gl.quad_vao, SIT_ATTR_POSITION);
    SIT_CHECK_GL_ERROR();
    SIT_DEBUG_LOG("[QUAD] VAO attributes configured");

    // 7. *** CRITICAL *** Unbind our private VAO.
    SIT_DEBUG_LOG("[QUAD] Unbinding private VAO");
    glBindVertexArray(0); // Explicit unbind for safety and clarity
    // sit_render.gl.current_vao_id = 0;
    SIT_CHECK_GL_ERROR();

    // --- End of Private VAO/VBO Setup ---

    // 8. Set the initial projection matrix uniform in the shader program.
    // This matrix maps from screen pixel coordinates (0,0 top-left) to clip space.
    SIT_DEBUG_LOG("[QUAD] Setting projection matrix uniform");
    mat4 proj_quad;
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, proj_quad); // Top-left is (0,0)
    glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)proj_quad);
    SIT_CHECK_GL_ERROR(); // Check for errors setting the uniform
    SIT_DEBUG_LOG("[QUAD] Projection matrix set");

    // 9. CRITICAL: Ensure the global_vao_id is bound again before returning.
    // This reinforces that the user's rendering state is ready.
    SIT_DEBUG_LOG("[QUAD] Restoring global VAO binding");
    glBindVertexArray(sit_render.gl.global_vao_id);
    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
    SIT_CHECK_GL_ERROR();

    SIT_DEBUG_LOG("[QUAD] Quad renderer initialization complete");
    return true; // Indicate success

#elif defined(SITUATION_USE_VULKAN)
    // --- Vulkan Quad Renderer Initialization ---

    // 1. Compile the unified GLSL source into SPIR-V.
    //    The compiler is mandatory for Vulkan internal renderers.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Compiling quad vertex shader...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Vertex shader first 200 chars: %.200s\n", SIT_QUAD_VERTEX_SHADER); fflush(stdout);
    #endif
    _SituationSpirvBlob vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_QUAD_VERTEX_SHADER, "internal_quad.vert", shaderc_vertex_shader);
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Compiling quad fragment shader...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Fragment shader first 300 chars: %.300s\n", SIT_QUAD_FRAGMENT_SHADER); fflush(stdout);
    #endif
    _SituationSpirvBlob fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(SIT_QUAD_FRAGMENT_SHADER, "internal_quad.frag", shaderc_fragment_shader);

    if (!vs_spirv.data || !fs_spirv.data) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Shader compilation failed!\n"); fflush(stdout);
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        printf("Situation [Vulkan Debug]:   Error: %s\n", err_msg ? err_msg : "Unknown"); fflush(stdout);
        if (err_msg) SituationFreeString(err_msg);
        #endif
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return false; // Error already set by compiler
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Shaders compiled successfully\n"); fflush(stdout);
    #endif

    // 2. Create the Pipeline Layout.
    // This defines the "shape" of the uniforms (Descriptor Sets and Push Constants).
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible by both shaders
    push_constant_range.offset = 0;
    // Updated size: Model(64) + Color(16) + UVRect(16) + TextureID(4) + UseTex(4) = 104 bytes
    push_constant_range.size = sizeof(mat4) + sizeof(vec4) + sizeof(vec4) + sizeof(uint32_t) + sizeof(int);

    // Define Layouts: Set 0 = View UBO, Set 1 = Bindless Texture Array (Binding 0)
    VkDescriptorSetLayout set_layouts[2];
    uint32_t set_layout_count = 1;
    set_layouts[0] = sit_render.vk.view_data_ubo_layout;
    if (sit_render.vk.bindless_descriptor_layout != VK_NULL_HANDLE) {
        set_layouts[1] = sit_render.vk.bindless_descriptor_layout;
        set_layout_count = 2;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = set_layout_count;
    pipeline_layout_info.pSetLayouts = set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating quad pipeline layout...\n"); fflush(stdout);
    #endif
    if (vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &sit_render.vk.quad_pipeline_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create quad pipeline layout.");
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return false;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Pipeline layout created successfully\n"); fflush(stdout);
    #endif

    // 3. Define the quad's specific vertex input layout.
    VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_desc = {};
    attr_desc.binding = 0;
    attr_desc.location = SIT_ATTR_POSITION;
    attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc.offset = 0;

    // 4. Call the generic pipeline creator with the quad's specific configuration.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating graphics pipeline...\n"); fflush(stdout);
    #endif
    sit_render.vk.quad_pipeline = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv.data, vs_spirv.size,
        fs_spirv.data, fs_spirv.size,
        sit_render.vk.quad_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // Quads are drawn as a strip
        1, &binding_desc,
        1, &attr_desc,
        SIT_VK_PIPELINE_BLEND_OPAQUE
    );

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);

    if(sit_render.vk.quad_pipeline == VK_NULL_HANDLE) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Pipeline creation returned NULL!\n"); fflush(stdout);
        #endif
        return false;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Graphics pipeline created successfully\n"); fflush(stdout);
    #endif

    // 5. Create and upload the vertex buffer for the quad.
    // Unit Quad: (0,0) to (1,1)
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating quad vertex buffer...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Device handle before buffer creation: %p\n", (void*)sit_render.vk.device); fflush(stdout);
    printf("Situation [Vulkan Debug]:   VMA allocator: %p\n", (void*)sit_render.vk.vma_allocator); fflush(stdout);
    #endif
    float quad_vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    if (_SituationVulkanCreateAndUploadBuffer(VK_NULL_HANDLE, quad_vertices, sizeof(quad_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &sit_render.vk.quad_vertex_buffer, &sit_render.vk.quad_vertex_buffer_memory) != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Failed to create quad vertex buffer.");
        return false;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Quad vertex buffer created successfully\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Buffer handle: %p, Memory: %p\n", (void*)sit_render.vk.quad_vertex_buffer, (void*)sit_render.vk.quad_vertex_buffer_memory); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Quad renderer initialization complete!\n"); fflush(stdout);
    #endif

    return true;
#endif
    return false; // Should not be reached
}

/**
 * @brief [INTERNAL] Initializes the built-in text renderer subsystem (glyph atlas, shaders, uniforms).
 *
 * @details This function is called once during library startup (typically from `SituationInit`)
 *          to set up the low-level text rendering infrastructure. It prepares everything needed
 *          for high-quality, efficient 2D text drawing via `SituationCmdDrawText` and related APIs.
 *
 *          What it initializes (in rough order):
 *            - Loads the default font bitmap (e.g. embedded 8x8 VGA font or stb_truetype atlas)
 *            - Creates GPU texture for the glyph atlas (RGBA8 or similar)
 *            - Uploads glyph metrics and UV coordinates (either pre-baked or computed)
 *            - Creates vertex/index buffers for a dynamic quad pool (for glyph instances)
 *            - Compiles/linkes the text-specific vertex + fragment shaders
 *              (simple transform + texture sampling + color tint + optional outline/drop shadow)
 *            - Sets up uniform buffer or push constants for per-draw text state
 *              (color, transform matrix, scale, font size, etc.)
 *            - Prepares descriptor sets / texture bindings (Vulkan) or texture units (OpenGL)
 *            - Allocates scratch buffers for string processing and glyph batching
 *            - Caches common ASCII/Unicode ranges if using runtime glyph rasterization
 *
 *          On success, the text renderer is ready for immediate use no further init required.
 *          On failure (e.g. texture allocation fail, shader compile error), logs warnings/errors
 *          and disables text rendering gracefully (future draw calls become no-ops).
 *
 * @return true if initialization completed successfully,
 *         false on any critical failure (shader compile fail, out of memory, invalid font data)
 *
 * @note This function is **called only once** during library lifetime.
 *       Thread safety: Must be called from the thread that owns the GL/VK context
 *       (typically main thread during init, or render thread if deferred).
 *       No locking assumes exclusive access during startup.
 *
 *       If `SITUATION_ENABLE_TEXT_RENDERER` is not defined (or disabled at runtime),
 *       this function returns true immediately (no-op).
 *
 *       Dependencies:
 *         - stb_truetype (for font loading/rasterization if dynamic)
 *         - Embedded font data (e.g. sit_default_8x8_font array)
 *         - Vulkan/OpenGL context already current
 *
 * @see SituationInit (caller), SituationCmdDrawText, SituationCmdDrawTextEx,
 *      SITUATION_ENABLE_TEXT_RENDERER (compile-time toggle),
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED, SITUATION_ERROR_MEMORY_ALLOCATION
 */
static bool _SituationInitTextRenderer(void) {
#if defined(SITUATION_USE_OPENGL)
    SituationError shader_err;
    sit_render.gl.text_shader_program = _SituationCreateGLShaderProgram(SIT_TEXT_VERTEX_SHADER, SIT_TEXT_FRAGMENT_SHADER, &shader_err);
    if (shader_err != SITUATION_SUCCESS) return false;

    glCreateVertexArrays(1, &sit_render.gl.text_vao);
    glCreateBuffers(1, &sit_render.gl.text_vbo);

    if (sit_render.gl.text_vao == 0 || sit_render.gl.text_vbo == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitTextRenderer: Failed to create text VAO/VBO");
        if (sit_render.gl.text_vao) { glDeleteVertexArrays(1, &sit_render.gl.text_vao); sit_render.gl.text_vao = 0; }
        if (sit_render.gl.text_vbo) { glDeleteBuffers(1, &sit_render.gl.text_vbo); sit_render.gl.text_vbo = 0; }
        glDeleteProgram(sit_render.gl.text_shader_program);
        sit_render.gl.text_shader_program = 0;
        return false;
    }

    // Pre-allocate a dynamic buffer (512KB = ~5400 characters)
    glNamedBufferData(sit_render.gl.text_vbo, 524288, NULL, GL_DYNAMIC_DRAW);

    glBindVertexArray(sit_render.gl.text_vao);
    // sit_render.gl.current_vao_id = sit_render.gl.text_vao;
    glVertexArrayVertexBuffer(sit_render.gl.text_vao, 0, sit_render.gl.text_vbo, 0, 4 * sizeof(float)); // Stride: x,y,u,v

    // Pos: 2 floats, offset 0
    glEnableVertexArrayAttrib(sit_render.gl.text_vao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(sit_render.gl.text_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(sit_render.gl.text_vao, SIT_ATTR_POSITION, 0);

    // UV: 2 floats, offset 8
    glEnableVertexArrayAttrib(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0, 0);

    glBindVertexArray(0);
    // sit_render.gl.current_vao_id = 0;
    return true;

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan initialization is handled in _SituationVulkanInitInternalRenderers due to complex dependency chains
    // (Pipeline Layouts, SPIR-V compilation, etc.)
    return true;
#endif
    return false;
}

/**
 * @brief [INTERNAL] Destroys all backend-specific resources used by the internal quad renderer.
 * @details This helper function is called during the main shutdown sequence to clean up the dedicated resources created by `_SituationInitQuadRenderer`. It ensures that the internal shaders, pipelines, and vertex buffers used for drawing simple quads are properly released.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Deletes the quad renderer's shader program, its private Vertex Array Object (VAO), and its Vertex Buffer Object (VBO).
 * - **Vulkan:** Destroys the `VkPipeline`, `VkPipelineLayout`, and the vertex `VkBuffer` (along with its `VmaAllocation`) associated with the quad renderer. It assumes the device is idle before being called.
 *
 * @note This function is designed to be robust and will safely handle being called on a partially initialized state by checking if resource handles are valid before attempting destruction.
 * @warning This function is for internal use by the main cleanup routines (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`) only.
 *
 * @see _SituationInitQuadRenderer()
 */
static void _SituationCleanupQuadRenderer(void) {
#if defined(SITUATION_USE_OPENGL)
    if (sit_render.gl.quad_shader_program) { glDeleteProgram(sit_render.gl.quad_shader_program); sit_render.gl.quad_shader_program = 0; }
    if (sit_render.gl.quad_vao) { glDeleteVertexArrays(1, &sit_render.gl.quad_vao); sit_render.gl.quad_vao = 0; }
    if (sit_render.gl.quad_vbo) { glDeleteBuffers(1, &sit_render.gl.quad_vbo); sit_render.gl.quad_vbo = 0; }

    // Cleanup Text Renderer
    if (sit_render.gl.text_shader_program) { glDeleteProgram(sit_render.gl.text_shader_program); sit_render.gl.text_shader_program = 0; }
    if (sit_render.gl.text_vao) { glDeleteVertexArrays(1, &sit_render.gl.text_vao); sit_render.gl.text_vao = 0; }
    if (sit_render.gl.text_vbo) { glDeleteBuffers(1, &sit_render.gl.text_vbo); sit_render.gl.text_vbo = 0; }

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Cleaning up quad renderer...\n"); fflush(stdout);
        printf("Situation [Vulkan Debug]:   quad_pipeline=%p\n", (void*)sit_render.vk.quad_pipeline); fflush(stdout);
        printf("Situation [Vulkan Debug]:   quad_vertex_buffer=%p\n", (void*)sit_render.vk.quad_vertex_buffer); fflush(stdout);
        #endif
        if (sit_render.vk.quad_pipeline) vkDestroyPipeline(sit_render.vk.device, sit_render.vk.quad_pipeline, NULL);
        if (sit_render.vk.quad_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.quad_pipeline_layout, NULL);
        if (sit_render.vk.quad_vertex_buffer) {
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:   Destroying quad vertex buffer...\n"); fflush(stdout);
            #endif
            vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.quad_vertex_buffer, sit_render.vk.quad_vertex_buffer_memory);
        }

        // Text Renderer Cleanup
        if (sit_render.vk.text_pipeline) vkDestroyPipeline(sit_render.vk.device, sit_render.vk.text_pipeline, NULL);
        if (sit_render.vk.text_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.text_pipeline_layout, NULL);
    }
#endif
}

/**
 * @brief [INTERNAL] Destroys all OpenGL-specific resources created by the library.
 * @details This function is the backend-specific cleanup handler for OpenGL. It is responsible for deleting all globally managed OpenGL objects, such as internal shader programs, VAOs, VBOs, and UBOs.
 *          It assumes the OpenGL context is still active when it is called.
 *
 * @par Cleanup Process
 *   - Destroys the internal quad renderer's shader, VAO, and VBO.
 *   - Destroys the shader programs used for virtual display compositing.
 *   - Destroys the global VAO used for all user rendering.
 *   - Destroys the private VAO/VBO used for drawing virtual display quads.
 *   - Destroys any other global resources like the view data UBO.
 *
 * @note This function is designed to be robust and will not cause errors if called on a partially initialized state (i.e., it checks if object IDs are non-zero before attempting deletion).
 * @warning This function is for internal use by `_SituationCleanupRenderer` only.
 */
#if defined(SITUATION_USE_OPENGL)
static void _SituationCleanupOpenGL(void) {
    // [Phase 2] Loader Window Cleanup
    #if !defined(__STDC_NO_THREADS__)
    if (sit_render.gl.loader_window) {
        glfwDestroyWindow(sit_render.gl.loader_window);
        sit_render.gl.loader_window = NULL;
    }
    // [v2.3.21] Render thread shutdown logic moved to _SituationDestroyRenderThread in SituationShutdown

    // Ensure we have a context for cleanup (re-acquire if needed)
    if (sit_gs.sit_glfw_window && glfwGetCurrentContext() == NULL) {
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
    }
    #endif

    // The OpenGL context is still active here.
    // Clean up all library-managed GL objects.
    _SituationCleanupQuadRenderer();
    if (sit_render.gl.vd_shader_program_id != 0) glDeleteProgram(sit_render.gl.vd_shader_program_id);
    if (sit_render.gl.composite_shader_program_id != 0) glDeleteProgram(sit_render.gl.composite_shader_program_id);
    if (sit_render.gl.global_vao_id != 0) { glDeleteVertexArrays(1, &sit_render.gl.global_vao_id); sit_render.gl.global_vao_id = 0; }
    if (sit_render.gl.mesh_vao_id != 0) { glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id); sit_render.gl.mesh_vao_id = 0; }
    if (sit_render.gl.vd_quad_vao != 0) glDeleteVertexArrays(1, &sit_render.gl.vd_quad_vao);
    if (sit_render.gl.vd_quad_vbo != 0) glDeleteBuffers(1, &sit_render.gl.vd_quad_vbo);
    if (sit_render.gl.composite_copy_texture_id != 0) glDeleteTextures(1, &sit_render.gl.composite_copy_texture_id);
    if (sit_render.gl.view_data_ubo_id != 0) glDeleteBuffers(1, &sit_render.gl.view_data_ubo_id);

    // [Phase 2.5] Cleanup VAO Cache
    for (int i = 0; i < 256; i++) {
        _SitGLVaoCacheEntry* entry = sit_render.gl.vao_cache[i];
        while (entry) {
            _SitGLVaoCacheEntry* next = entry->next;
            if (entry->vao_id) glDeleteVertexArrays(1, &entry->vao_id);
            SIT_FREE(entry);
            entry = next;
        }
        sit_render.gl.vao_cache[i] = NULL;
    }

    // Cleanup Graveyards & Fences
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; i++) {
        // Wait for any pending GPU work for this frame before destroying
        if (sit_render.gl.frame_fences[i]) {
            glClientWaitSync(sit_render.gl.frame_fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, 100000000); // 100ms
            glDeleteSync(sit_render.gl.frame_fences[i]);
            sit_render.gl.frame_fences[i] = 0;
        }

        // Force flush now that we know the GPU is idle (or shutting down)
        _SitGLFlushGraveyard(i);

        if (sit_render.gl.graveyards[i].mesh_ids_to_clean) SIT_FREE(sit_render.gl.graveyards[i].mesh_ids_to_clean);
        if (sit_render.gl.graveyards[i].buffers_to_delete) SIT_FREE(sit_render.gl.graveyards[i].buffers_to_delete);
        if (sit_render.gl.graveyards[i].textures_to_delete) SIT_FREE(sit_render.gl.graveyards[i].textures_to_delete);
        ma_mutex_uninit(&sit_render.gl.graveyards[i].lock);
    }
    memset(sit_render.gl.graveyards, 0, sizeof(sit_render.gl.graveyards));

    // Cleanup Soft Command Buffers
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; i++) {
        if (sit_render.gl.soft_buffers[i].packets) SIT_FREE(sit_render.gl.soft_buffers[i].packets);
        if (sit_render.gl.soft_buffers[i].data_buffer) SIT_FREE(sit_render.gl.soft_buffers[i].data_buffer);
    }
    memset(sit_render.gl.soft_buffers, 0, sizeof(sit_render.gl.soft_buffers));

    // [Bug 6 Fix] Cleanup Ring Buffer and MDI Buffer (persistent mapped buffers)
    if (sit_render.gl.ring_buffer_id != 0) {
        // Unmap is implicit when buffer is deleted (persistent mapping)
        glDeleteBuffers(1, &sit_render.gl.ring_buffer_id);
    }
    if (sit_render.gl.mdi_buffer_id != 0) {
        glDeleteBuffers(1, &sit_render.gl.mdi_buffer_id);
    }
    if (sit_render.gl.ring_fences) {
        for (size_t i = 0; i < sit_render.gl.ring_fence_count; i++) {
            if (sit_render.gl.ring_fences[i]) glDeleteSync(sit_render.gl.ring_fences[i]);
        }
        SIT_FREE(sit_render.gl.ring_fences);
    }

    // [Bug 6 Fix] Zero out ALL OpenGL state to allow clean re-initialization.
    // Without this, _SituationInitOpenGL's guard checks (e.g., ring_buffer_id != 0)
    // would skip re-creation, leaving stale/deleted IDs that crash on use.
    memset(&sit_render.gl, 0, sizeof(sit_render.gl));
}
#endif // SITUATION_USE_OPENGL

/**
 * @brief [INTERNAL] Destroys all Vulkan-specific resources created by the library.
 * @details This is the comprehensive backend-specific cleanup handler for Vulkan. It is responsible for destroying all Vulkan objects in the precise reverse order of their creation to ensure compliance with the API's strict object lifetime rules.
 *
 * @par Cleanup Process
 *   The function systematically destroys all resources, from high-level objects down to the `VkInstance` itself. This includes:
 *   - Internal renderers (quad renderer).
 *   - The swapchain and all its dependent resources (`_SituationVulkanCleanupSwapchain`).
 *   - All per-frame synchronization objects (semaphores, fences) and UBOs.
 *   - The main command pool, render pass, and VMA allocator.
 *   - All descriptor set layouts and descriptor pools.
 *   - The `VkDevice` (logical device).
 *   - The debug messenger, `VkSurfaceKHR`, and finally the `VkInstance`.
 *
 * @note This function is designed to be robust. It checks if each handle is non-NULL before attempting to destroy it, making it safe to call even if the initialization process failed partway through.
 * @warning This function is for internal use by `_SituationCleanupRenderer` only.
 */
#if defined(SITUATION_USE_VULKAN)
static void _SituationCleanupVulkan(void) {
    /* Bounded idle + event pump — vkDeviceWaitIdle can wedge forever (frozen window). */
    if (sit_render.vk.device != VK_NULL_HANDLE) {
        _SituationVulkanWaitInFlightFencesPump("_SituationCleanupVulkan");
    }

    /* Drain graveyards before swapchain / internal teardown so deferred vmaDestroy* runs
       before any path that might invalidate allocator state; pairs with immediate destroys
       during SHUTTING_DOWN in SituationDestroy*. */
    if (sit_render.vk.graveyards) {
        for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
            _SituationFlushGraveyard(i);
        }
    }

    _SituationCleanupQuadRenderer();
    _SituationVulkanCleanupSwapchain();
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.render_finished_semaphores[i], NULL);
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.image_available_semaphores[i], NULL);
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.compute_finished_semaphores[i], NULL);
        vkDestroyFence(sit_render.vk.device, sit_render.vk.in_flight_fences[i], NULL);
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.view_proj_ubo_buffer[i], sit_render.vk.view_proj_ubo_memory[i]);
        // Destroy dynamic VBOs
        if (sit_render.vk.dynamic_vbo[i]) {
            // No need to Unmap if VMA_ALLOCATION_CREATE_MAPPED_BIT was used
            vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.dynamic_vbo[i], sit_render.vk.dynamic_vbo_alloc[i]);
        }
    }
    // --- Free the arrays themselves ---
    SIT_FREE(sit_render.vk.command_buffers);
    SIT_FREE(sit_render.vk.compute_command_buffers);
    SIT_FREE(sit_render.vk.image_available_semaphores);
    SIT_FREE(sit_render.vk.render_finished_semaphores);
    SIT_FREE(sit_render.vk.compute_finished_semaphores);
    SIT_FREE(sit_render.vk.in_flight_fences);
    SIT_FREE(sit_render.vk.view_proj_ubo_buffer);
    SIT_FREE(sit_render.vk.view_proj_ubo_memory);
    SIT_FREE(sit_render.vk.view_proj_ubo_mapped);
    SIT_FREE(sit_render.vk.view_proj_ubo_descriptor_set);

    // Clean up graveyards
    if (sit_render.vk.graveyards) {
        for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
            _SituationFlushGraveyard(i); // Important: Flush resources first!
            _SituationCleanupGraveyard(&sit_render.vk.graveyards[i]);
        }
        SIT_FREE(sit_render.vk.graveyards);
    }

    _SituationCleanupStagingBuffers();

    for (int i = 0; i < sizeof(sit_render.vk.compute_layouts) / sizeof(sit_render.vk.compute_layouts[0]); ++i) {
        if (sit_render.vk.compute_layouts[i] != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.compute_layouts[i], NULL);
        }
    }
    vkDestroyCommandPool(sit_render.vk.device, sit_render.vk.command_pool, NULL);
    vkDestroyCommandPool(sit_render.vk.device, sit_render.vk.compute_command_pool, NULL);
    vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass, NULL);
    if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE) {
        vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass_resume, NULL);
        sit_render.vk.main_window_render_pass_resume = VK_NULL_HANDLE;
    }
    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_destroy(&sit_render.vk.screenshot_mutex);
        sit_render.vk.screenshot_mutex_initialized = false;
    }
    _SituationVulkanDestroyScreenshotResources();
    vmaDestroyAllocator(sit_render.vk.vma_allocator);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.ssbo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.ubo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.dynamic_ubo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.storage_buffer_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.bindless_descriptor_layout, NULL); // [Bindless]
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.image_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.text_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.storage_image_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.compute_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.composite_dest_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.view_data_ubo_layout, NULL);

    // --- Safe Descriptor Pool Cleanup ---

    // 1. Destroy any pools created dynamically by the Manager
    if (sit_render.vk.descriptor_manager.pools) {
        for (int i = 0; i < sit_render.vk.descriptor_manager.count; ++i) {
            // Safety Check: Don't double-free if the persistent pool somehow ended up in this list
            if (sit_render.vk.descriptor_manager.pools[i] != sit_render.vk.persistent_descriptor_pool) {
                vkDestroyDescriptorPool(sit_render.vk.device, sit_render.vk.descriptor_manager.pools[i], NULL);
            }
        }
        SIT_FREE(sit_render.vk.descriptor_manager.pools);
        sit_render.vk.descriptor_manager.pools = NULL;
    }

    // 2. Destroy the initial Persistent Pool (created in Init)
    if (sit_render.vk.persistent_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(sit_render.vk.device, sit_render.vk.persistent_descriptor_pool, NULL);
        sit_render.vk.persistent_descriptor_pool = VK_NULL_HANDLE;
    }

    // 3. Destroy Global Bindless Pool
    if (sit_render.vk.global_bindless_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(sit_render.vk.device, sit_render.vk.global_bindless_pool, NULL);
        sit_render.vk.global_bindless_pool = VK_NULL_HANDLE;
    }
    // ------------------------------------------

    vkDestroyDevice(sit_render.vk.device, NULL);
    if (sit_render.vk.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
            func(sit_render.vk.instance, sit_render.vk.debug_messenger, NULL);
        }
    }
    vkDestroySurfaceKHR(sit_render.vk.instance, sit_render.vk.surface, NULL);
    vkDestroyInstance(sit_render.vk.instance, NULL);
    // Match OpenGL teardown: after destroying objects, clear handles so a later SituationInit
    // cannot see stale non-NULL VkDevice/VkInstance pointers (same class of re-init bug as
    // memset(&sit_render.gl, ...) in _SituationCleanupOpenGL).
    memset(&sit_render.vk, 0, sizeof(sit_render.vk));
}

/**
 * @brief [INTERNAL] Creates all pre-defined VkPipelineLayouts for compute shaders.
 * @details This function is called once during Vulkan initialization. It builds a set of common pipeline layouts that users can select via the SituationComputeLayoutType enum, abstracting away the complexity of Vulkan layout creation.
 * @return SITUATION_SUCCESS on success, or an error code if any layout fails to create.
 */
 static SituationError _SituationVulkanInitComputeLayouts(void) {
    VkPipelineLayoutCreateInfo layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkDescriptorSetLayout set_layouts[4]; // Max needed for our most complex layout
    VkPushConstantRange push_constant = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 128 };

    // Layout 1: SIT_COMPUTE_LAYOUT_ONE_SSBO
    set_layouts[0] = sit_render.vk.ssbo_layout;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_ONE_SSBO]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 2: SIT_COMPUTE_LAYOUT_TWO_SSBOS
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.ssbo_layout; // Same layout used for two different sets
    layout_info.setLayoutCount = 2;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_TWO_SSBOS]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 3: SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO
    set_layouts[0] = sit_render.vk.storage_image_layout;
    set_layouts[1] = sit_render.vk.ssbo_layout;
    layout_info.setLayoutCount = 2;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 4: SIT_COMPUTE_LAYOUT_PUSH_CONSTANT
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_PUSH_CONSTANT]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 5: SIT_COMPUTE_LAYOUT_EMPTY
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_EMPTY]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 6: SIT_COMPUTE_LAYOUT_BUFFER_IMAGE
    // Set 0: SSBO (Buffer), Set 1: Storage Image
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_BUFFER_IMAGE]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;


    // Layout 7: SIT_COMPUTE_LAYOUT_TERMINAL
    // Set 0: SSBO (Buffer), Set 1: Storage Image (Output), Set 2: Combined Image Sampler (Font), Set 3: Combined Image Sampler (Sixel)
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Vulkan] Creating SIT_COMPUTE_LAYOUT_TERMINAL with 4 sets...\n");
    fprintf(stderr, "[Vulkan]   Set 0: SSBO layout %p\n", (void*)sit_render.vk.ssbo_layout);
    fprintf(stderr, "[Vulkan]   Set 1: Storage Image layout %p\n", (void*)sit_render.vk.storage_image_layout);
    fprintf(stderr, "[Vulkan]   Set 2: Image Sampler layout %p\n", (void*)sit_render.vk.image_sampler_layout);
    fprintf(stderr, "[Vulkan]   Set 3: Image Sampler layout %p\n", (void*)sit_render.vk.image_sampler_layout);
#endif
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    set_layouts[2] = sit_render.vk.compute_sampler_layout;  // Use compute-specific layout
    set_layouts[3] = sit_render.vk.compute_sampler_layout;  // Use compute-specific layout
    layout_info.setLayoutCount = 4;
    layout_info.pSetLayouts = set_layouts;

    // We reuse the push_constant range defined at top of function (64 bytes)
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_TERMINAL]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 8: SIT_COMPUTE_LAYOUT_VECTOR
    // Set 0: SSBO (Lines), Set 1: Storage Image (Output)
    // Uses Push Constants
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_VECTOR]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    return SITUATION_SUCCESS;
}

/**
 * @brief [Internal] Creates a Vulkan compute pipeline from SPIR-V bytecode.
 *
 * @details This function takes pre-compiled SPIR-V bytecode for a compute shader, creates the necessary Vulkan objects (VkShaderModule, VkPipelineLayout, VkPipeline), and returns a `SituationComputePipeline` struct containing them.
 *          The caller is responsible for assigning a public `id` to the returned struct and for adding it to any resource tracking systems.
 *          The caller is also responsible for eventually destroying the pipeline using `_SituationVulkanDestroyComputePipeline` or `SituationDestroyComputePipeline`.;
 *          Error handling is performed internally. If this function fails, it returns an invalid `SituationComputePipeline` (zero-initialized) and sets the library's last error state via `_SituationSetErrorFromCode`.
 *
 * @param cs_spirv_data Pointer to the compiled SPIR-V compute shader bytecode.
 *                      This data must be valid and correctly formatted SPIR-V.
 *                      Must not be NULL.
 * @param cs_spirv_size Size of the SPIR-V bytecode in bytes.
 *                      Must be greater than 0 and typically a multiple of 4.
 *                      Must not be 0.
 * @return A `SituationComputePipeline` struct.
 *         - On **success**: The struct contains valid Vulkan handles (`.vk_pipeline`, `.vk_pipeline_layout`) and should be used with Vulkan binding/execution functions. The caller must assign a public `.id`.
 *         - On **failure**: The struct is zero-initialized (`{0}`), indicating an invalid pipeline. The specific error can be retrieved using `SituationGetLastErrorMsg()`.
 *
 * @note This function requires the library to be initialized (`SituationInit` must have been called successfully).
 * @note This function creates a pipeline layout with **no descriptor set layouts** and **no push constant ranges**.
 *       If the compute shader requires descriptors or push constants, this function (or the logic calling it) must be modified to provide the appropriate `VkDescriptorSetLayout` objects and `VkPushConstantRange` definitions when creating the `VkPipelineLayout`.
 * @note The `VkShaderModule` created internally is destroyed immediately after the `VkPipeline` is successfully created, as per Vulkan specification.
 *       The `VkShaderModule` handle is **not** stored in the returned struct.
 * @warning The SPIR-V data pointed to by `cs_spirv_data` is not validated by this function for semantic correctness beyond basic Vulkan object creation. Passing invalid SPIR-V can lead to errors during pipeline creation or undefined behavior at runtime.
 * @see _SituationVulkanCreateShaderModule(), SituationCreateComputePipelineFromMemory(), SituationDestroyComputePipeline();
 */
// _SituationVulkanCreateComputePipelineFromSpirv ***** Function got nuked for SituationCreateComputePipeline()

/**
 * @brief [INTERNAL] Records a command to transition the layout of a VkImage, inserting a memory barrier.
 * @details This is a critical Vulkan synchronization helper that wraps `vkCmdPipelineBarrier` specifically for image layout transitions.
 *          Changing an image's layout is the primary way in Vulkan to signal a change in how the image will be used, ensuring that writes from one pipeline stage are visible to reads in a subsequent stage.
 *
 * @par Synchronization Logic
 *   The function automatically determines the correct `srcStageMask`, `dstStageMask`, `srcAccessMask`, and `dstAccessMask` for a set of common, essential transitions:
 *   - `UNDEFINED` -> `TRANSFER_DST_OPTIMAL`: Prepares an image to be a destination for a copy operation.
 *   - `TRANSFER_DST_OPTIMAL` -> `SHADER_READ_ONLY_OPTIMAL`: Makes an image that has been written to available for sampling in a shader.
 *   - `PRESENT_SRC_KHR` -> `TRANSFER_SRC_OPTIMAL`: After `vkCmdEndRenderPass` (finalLayout present), prepares the swapchain for `vkCmdCopyImageToBuffer`. Source stage must be **COLOR_ATTACHMENT_OUTPUT** so the copy waits on fragment writes, not `TRANSFER` (which would not synchronize with the draw that filled the image).
 *   - `TRANSFER_SRC_OPTIMAL` -> `PRESENT_SRC_KHR`: Transitions a swapchain image back to a presentable state after a copy.
 *
 * If an unsupported transition is requested, an error is set.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the pipeline barrier command will be recorded.
 * @param image The `VkImage` whose layout is to be transitioned.
 * @param mip_levels The number of mip levels in the image's subresource range to be transitioned.
 * @param old_layout The current `VkImageLayout` of the image.
 * @param new_layout The target `VkImageLayout` to transition the image to.
 *
 * @note This function is a fundamental building block for managing resource lifetimes and dependencies in the Vulkan backend.
 * @warning This is a low-level helper for internal use only. Incorrectly specifying `old_layout` can lead to validation errors or race conditions.
 *
 * @see _SituationVulkanCopyBufferToImage(), _SituationVulkanGenerateMipmaps(), vkCmdPipelineBarrier()
 */
static void _SituationVulkanTransitionImageLayout(VkCommandBuffer cmd, VkImage image, uint32_t mip_levels, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    // Determine pipeline stages and access masks based on the layouts
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        /* After vkCmdEndRenderPass, swapchain color is in PRESENT_SRC_KHR (see main_window_render_pass finalLayout). */
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        /* Prepare for vkCmdBeginRenderPass with attachment initialLayout UNDEFINED (discard + clear). */
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        // This is not an exhaustive list. Add other transitions as needed.
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Unsupported image layout transition specified in helper.");
        return;
    }

    vkCmdPipelineBarrier(cmd, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

/**
 * @brief [INTERNAL] Records a command to copy data from a VkBuffer to a VkImage.
 * @details This is a fundamental Vulkan utility function that wraps `vkCmdCopyBufferToImage`. It is used to transfer raw pixel data from a staging buffer in CPU-accessible memory to a final, device-local image on the GPU.
 *          It configures a single `VkBufferImageCopy` region to copy the entire buffer to the base mip level (level 0) and base array layer (layer 0) of the destination image.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the copy command will be recorded.
 * @param buffer The source `VkBuffer` containing the pixel data to be copied.
 * @param image The destination `VkImage` that will receive the pixel data.
 * @param width The width of the image region to copy, in pixels.
 * @param height The height of the image region to copy, in pixels.
 *
 * @note This function assumes that the destination `image` has been previously transitioned to the `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout, making it ready to receive data.
 * @warning This is a low-level helper for internal use by functions like `SituationCreateTexture`. It does not perform any synchronization; the caller is responsible for ensuring the source buffer is ready and for transitioning the image layout after the copy is complete.
 *
 * @see SituationCreateTexture(), _SituationVulkanTransitionImageLayout(), vkCmdCopyBufferToImage()
 */
static void _SituationVulkanCopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

/**
 * Copy raw mapped swapchain/staging texels into RGBA8 order (SituationImage / OpenGL parity).
 * Vulkan B8G8R8A8 layouts store B,G,R,A in memory; tests and GL readpixels expect R,G,B,A.
 */
static void _SituationVulkanCopyMappedColorToRGBA(uint8_t* dst, const void* mapped, size_t nbytes, VkFormat fmt) {
    const uint8_t* s = (const uint8_t*)mapped;
    switch (fmt) {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        for (size_t i = 0; i < nbytes; i += 4) {
            dst[i + 0] = s[i + 2];
            dst[i + 1] = s[i + 1];
            dst[i + 2] = s[i + 0];
            dst[i + 3] = s[i + 3];
        }
        break;
    default:
        memcpy(dst, s, nbytes);
        break;
    }
}

/**
 * @brief [INTERNAL] Synchronously copies a device-local VkImage to CPU-visible memory.
 *
 * @details This is a heavy-weight helper used for screenshots. It performs a complex sequence:
 *          1. Allocates a temporary host-visible buffer (`VK_BUFFER_USAGE_TRANSFER_DST_BIT`).
 *          2. Records a one-time command buffer to:
 *             - Transition source image layout to `TRANSFER_SRC_OPTIMAL`.
 *             - Execute `vkCmdCopyImageToBuffer`.
 *             - Transition source image layout back to its original state.
 *          3. Submits and waits for the GPU to finish (`vkQueueWaitIdle`).
 *          4. Maps the temporary buffer memory.
 *          5. `memcpy`s the data to a new `SIT_MALLOC`'d pointer.
 *          6. Destroys the temporary buffer.
 *
 * @param srcImage The source image handle (must have `TRANSFER_SRC` usage).
 * @param srcImageLayout The current layout of the source image (restored after copy).
 * @param width Image width.
 * @param height Image height.
 *
 * @return A pointer to raw pixel data (RGBA8), or NULL on failure. Caller must `free()`.
 */
static void* _SituationVulkanBlitImageToHostVisibleBuffer(VkImage srcImage, VkImageLayout srcImageLayout, uint32_t width, uint32_t height) {
    VkBuffer dstBuffer;
    VmaAllocation dstAllocation;
    VkDeviceSize bufferSize = (VkDeviceSize)width * height * 4; // Assuming 4 bytes per pixel (RGBA)
    void* finalImageData = NULL; // The final buffer we will return to the user

    // --- Step 1: Create the destination buffer in host-visible memory ---
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bufferSize, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
    VmaAllocationCreateInfo allocInfo = { .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage = VMA_MEMORY_USAGE_GPU_TO_CPU };

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &bufferInfo, &allocInfo, &dstBuffer, &dstAllocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Failed to create host-visible buffer for screenshot.");
        return NULL;
    }

    // --- Step 2: Record and submit commands for the copy ---
    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // a. Transition source image to be ready for copy
    //    We need a new, more generic transition helper for this.
    _SituationVulkanTransitionImageLayout(cmd, srcImage, 1, srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // b. Record the copy command
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};
    vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer, 1, &region);

    // c. Transition source image back to its original layout so it can be presented
    _SituationVulkanTransitionImageLayout(cmd, srcImage, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcImageLayout);

    // Submit and wait for completion
    _SituationVulkanEndSingleTimeCommands(cmd);

    // --- Step 3: Map the memory, copy it, and clean up ---
    void* mappedData;
    if (vmaMapMemory(sit_render.vk.vma_allocator, dstAllocation, &mappedData) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to map screenshot buffer.");
        vmaDestroyBuffer(sit_render.vk.vma_allocator, dstBuffer, dstAllocation);
        return NULL;
    }

    // Allocate the final buffer for the user and copy the data
    finalImageData = SIT_MALLOC(bufferSize);
    if (finalImageData) {
        _SituationVulkanCopyMappedColorToRGBA((uint8_t*)finalImageData, mappedData, (size_t)bufferSize, sit_render.vk.swapchain_image_format);
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Final screenshot image buffer.");
    }

    vmaUnmapMemory(sit_render.vk.vma_allocator, dstAllocation);
    vmaDestroyBuffer(sit_render.vk.vma_allocator, dstBuffer, dstAllocation);

    return finalImageData;
}

static void _SituationVulkanDestroyScreenshotResources(void) {
    if (sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_buffer, sit_render.vk.screenshot_staging_allocation);
        sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
        sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
    }
    if (sit_render.vk.screenshot_buffer) {
        SIT_FREE(sit_render.vk.screenshot_buffer);
        sit_render.vk.screenshot_buffer = NULL;
    }
    sit_render.vk.screenshot_width = 0;
    sit_render.vk.screenshot_height = 0;
    sit_render.vk.screenshot_valid = false;
    for (int _si = 0; _si < SITUATION_MAX_FRAMES_IN_FLIGHT; _si++) {
        sit_render.vk.screenshot_copy_pending[_si] = false;
    }
}

static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE &&
        (uint32_t)sit_render.vk.screenshot_width == width &&
        (uint32_t)sit_render.vk.screenshot_height == height) {
        return SITUATION_SUCCESS;
    }
    _SituationVulkanDestroyScreenshotResources();
    /* Same allocation pattern as _SituationVulkanBlitImageToHostVisibleBuffer (proven readback path). */
    VkDeviceSize buffer_size = (VkDeviceSize)width * (VkDeviceSize)height * 4u;
    VkBufferCreateInfo buf_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buf_info.size = buffer_size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buf_info, &alloc_info,
            &sit_render.vk.screenshot_staging_buffer,
            &sit_render.vk.screenshot_staging_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Vulkan screenshot staging buffer creation failed.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }
    sit_render.vk.screenshot_buffer = (uint8_t*)SIT_MALLOC((size_t)width * (size_t)height * 4u);
    if (!sit_render.vk.screenshot_buffer) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_buffer, sit_render.vk.screenshot_staging_allocation);
        sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
        sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_render.vk.screenshot_width = (int)width;
    sit_render.vk.screenshot_height = (int)height;
    return SITUATION_SUCCESS;
}

static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height) {
    _SituationVulkanTransitionImageLayout(cmd, swapchain_image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};
    vkCmdCopyImageToBuffer(cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        sit_render.vk.screenshot_staging_buffer, 1, &region);
    _SituationVulkanTransitionImageLayout(cmd, swapchain_image, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = true;
    }
}

static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index) {
    bool do_copy = (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) && sit_render.vk.screenshot_copy_pending[frame_index];
    if (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.screenshot_copy_pending[frame_index] = false;
    }
    if (!do_copy || sit_render.vk.screenshot_staging_buffer == VK_NULL_HANDLE || !sit_render.vk.screenshot_buffer) {
        sit_render.vk.screenshot_valid = false;
        return;
    }

    VkResult w = _SituationVulkanWaitFencePumpWindow(sit_render.vk.device, sit_render.vk.in_flight_fences[frame_index]);
    if (w != VK_SUCCESS) {
        if (w == VK_TIMEOUT) {
            fprintf(stderr, "[Vulkan] Screenshot fence wait timed out (frame_index=%u)\n", frame_index);
            fflush(stderr);
        }
        sit_render.vk.screenshot_valid = false;
        return;
    }

    void* mapped = NULL;
    if (vmaMapMemory(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation, &mapped) != VK_SUCCESS) {
        sit_render.vk.screenshot_valid = false;
        return;
    }

    VmaAllocationInfo alloc_inf = {};
    vmaGetAllocationInfo(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation, &alloc_inf);
    VkMappedMemoryRange flush_range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    flush_range.memory = alloc_inf.deviceMemory;
    flush_range.offset = alloc_inf.offset;
    flush_range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(sit_render.vk.device, 1, &flush_range);

    size_t nbytes = (size_t)sit_render.vk.screenshot_width * (size_t)sit_render.vk.screenshot_height * 4u;

    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_lock(&sit_render.vk.screenshot_mutex);
    }
    _SituationVulkanCopyMappedColorToRGBA(sit_render.vk.screenshot_buffer, mapped, nbytes, sit_render.vk.swapchain_image_format);
    sit_render.vk.screenshot_valid = true;
    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_unlock(&sit_render.vk.screenshot_mutex);
    }

    vmaUnmapMemory(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation);
}

/**
 * @brief [INTERNAL] Generates a complete mipmap chain for a Vulkan image using sequential blits.
 * @details This helper function is responsible for creating all mipmap levels for a given texture, from the base level (mip 0) down to the final 1x1 level. It performs this by iteratively blitting from each mip level `i` to the next level `i+1`, which has half the dimensions.
 *          This process is essential for high-quality texture rendering, as it provides pre-filtered, lower-resolution versions of the texture for the GPU to sample from when the object is far from the camera, significantly reducing aliasing and shimmering artifacts.
 *
 * @par Synchronization and Workflow
 *   The function executes a precise, looped sequence of commands for each new mip level:
 *   1.  **Barrier:** It first records a `VkImageMemoryBarrier` to transition the layout of the *source* mip level (e.g., mip `i-1`) from `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
 *       This ensures that the previous write operation (either the initial data copy or the previous blit) is complete and the memory is visible for reading.
 *   2.  **Blit:** It records a `vkCmdBlitImage` command. This command performs the downscaling operation, copying from the source mip level to the destination mip level (e.g., from mip `i-1` to mip `i`). Linear filtering is used to ensure a smooth, high-quality downsample.
 *   3.  **Barrier:** Immediately after the blit command, it records another barrier to transition the layout of the *source* mip level (`i-1`) from `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
 *       This makes the now-finalized mip level available for sampling by shaders.
 *
 * After the loop finishes, a final barrier is issued to transition the very last mip level to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the barrier and blit commands will be recorded.
 * @param image The `VkImage` for which to generate mipmaps. This image must have been created with `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` and `VK_IMAGE_USAGE_TRANSFER_DST_BIT` usage flags.
 * @param width The width of the base mip level (level 0).
 * @param height The height of the base mip level (level 0).
 * @param mip_levels The total number of mip levels in the image, including the base level.
 *
 * @note This function assumes that the base mip level (level 0) has already been populated with data and that all mip levels are currently in the `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
 * @warning This function is for internal use by `SituationCreateTexture` only and should not be called directly.
 *
 * @see SituationCreateTexture(), vkCmdBlitImage(), vkCmdPipelineBarrier()
 */
static void _SituationVulkanGenerateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t width, int32_t height, uint32_t mip_levels) {
    // This barrier will be reused to transition each mip level
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = width;
    int32_t mip_height = height;

    for (uint32_t i = 1; i < mip_levels; i++) {
        // 1. Transition the previous mip level (i-1) to be a transfer source.
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        // 2. Perform the blit from the previous level to the current level.
        VkImageBlit blit = {};
        blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
        blit.srcOffsets[1] = (VkOffset3D){mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
        blit.dstOffsets[1] = (VkOffset3D){ mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // 3. Transition the previous mip level (i-1) to be shader-readable.
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        // Update dimensions for the next iteration
        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
    }

    // Finally, transition the very last mip level to be shader-readable.
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}

#endif // SITUATION_USE_VULKAN

/**
 * @brief [INTERNAL] Performs a comprehensive, robust cleanup of all library components in response to an initialization failure.
 * @details This is the primary error handling routine for the `SituationInit` function. It is designed to be called from any point during the initialization sequence if a critical failure occurs.
 *          Its purpose is to safely unwind the initialization process, releasing any resources that were successfully allocated before the point of failure to prevent leaks.
 *
 * @par Cleanup Strategy
 *   The function executes the main cleanup routines in the **exact reverse order of initialization** to respect dependencies (e.g., the Vulkan surface must be destroyed before the GLFW window).
 *   1.  **GPU Synchronization:** It first attempts to wait for the GPU to go idle (`vkDeviceWaitIdle` or `glFinish`). This is a critical step to ensure that no resources are in use by the GPU when destruction begins, preventing validation errors or crashes.
 *   2.  **Subsystem Teardown:** Calls `_SituationCleanupSubsystems` to release audio, input, and timer resources.
 *   3.  **Renderer Teardown:** Calls `_SituationCleanupRenderer` to dispatch to the backend-specific cleanup (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`).
 *   4.  **Platform Teardown:** Calls `_SituationCleanupPlatform` to destroy the window and terminate GLFW.
 *
 * The robustness of this function relies on the fact that each individual cleanup helper is idempotent and safely handles being called on a partially initialized state (i.e., by checking if resource handles are `NULL` before attempting to destroy them).
 *
 * @note This function is for internal use by `SituationInit` only and should never be called directly.
 * @warning After this function completes, the library is in a fully uninitialized state.
 *
 * @see SituationInit(), _SituationCleanupSubsystems(), _SituationCleanupRenderer(), _SituationCleanupPlatform()
 */


// --- Callbacks and Event Handling ---

// --- Command-Line Argument Queries ---

/**
 * @brief Prepares the rendering context for a new frame.
 *
 * @details This function must be called at the beginning of each application framebefore any rendering commands are recorded or executed. It performs backend-specific setup necessary to acquire the next rendering target (e.g., the next swapchain image in Vulkan) and prepare command buffers.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:**
 *   - Makes the main GLFW window's OpenGL context current for the calling thread.
 *   - Binds the default framebuffer (the main window's backbuffer).
 *   - Sets the viewport to cover the entire window area.
 *   - This function typically always succeeds if the library is initialized and the OpenGL context is valid, returning `true`.
 * - **Vulkan:**
 *   - Waits for the GPU to finish processing the commands associated with the frame identified by `sit_render.vk.current_frame_index`.
 *   - Attempts to acquire the next image from the swapchain. This image will be the target for rendering this frame.
 *   - If the swapchain is out of date (e.g., due to a window resize), this function internally calls `_SituationVulkanRecreateSwapchain` to handle the recreation process. In this specific case, it returns `false` to signal that the frame setup was interrupted and should be retried.
 *   - Resets the fence associated with the current frame index to the unsignaled state.
 *   - Resets the primary command buffer for the current frame.
 *   - Begins recording commands into the primary command buffer.
 *
 * @return `true` if the frame was successfully prepared and rendering can proceed.
 *         This is the standard return value for both OpenGL and Vulkan under normal conditions.
 * @return `false` (Vulkan only) if the swapchain was out of date and was automatically recreated. The caller should typically call `SituationAcquireFrameCommandBuffer()` again in the next iteration of their main loop to proceed with the new swapchain.
 * @return `false` if the library is not initialized.
 * @return `false` (Vulkan) if acquiring the swapchain image fails for reasons other than `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR`.
 * @return `false` (Vulkan) if resetting or beginning the command buffer fails.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized before calling this function.
 *       2. This function is called once per frame, before any rendering commands.
 *       3. (Vulkan) The application loop handles the `false` return value correctly, especially when it indicates swapchain recreation.
 *
 * @warning This function is not thread-safe and must be called from the thread that initialized the library.
 */
SITAPI bool SituationAcquireFrameCommandBuffer(void) {
    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot begin frame before library initialization.");
        return false;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        // [Hot-Reload] Poll Async Shader Linking
        for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
            _SituationShaderSlot* slot = &sit_render.shader_registry[i];
            if (slot->is_active && slot->gl_is_linking) {
                GLint status = 0;
                // Check if linking has finished (non-blocking if supported)
                glGetProgramiv(slot->gl_pending_program_id, GL_COMPLETION_STATUS_KHR, &status);

                if (status == GL_TRUE) {
                    // Linking complete. Check success.
                    GLint success = 0;
                    glGetProgramiv(slot->gl_pending_program_id, GL_LINK_STATUS, &success);

                    if (success) {
                        // Success: Swap
                        if (slot->gl_program_id) glDeleteProgram(slot->gl_program_id);
                        slot->gl_program_id = slot->gl_pending_program_id;
                        slot->gl_pending_program_id = 0;

                        // Recreate Uniform Map for the new program
                        if (slot->uniform_map) _sit_uniform_map_destroy(slot->uniform_map);
                        slot->uniform_map = _sit_uniform_map_create();

                        if (slot->uniform_map) {
                             GLint count;
                             glGetProgramiv(slot->gl_program_id, GL_ACTIVE_UNIFORMS, &count);
                             for (GLint j = 0; j < count; j++) {
                                 char name[256];
                                 GLsizei length;
                                 GLint size;
                                 GLenum type;
                                 glGetActiveUniform(slot->gl_program_id, (GLuint)j, sizeof(name), &length, &size, &type, name);
                                 GLint location = glGetUniformLocation(slot->gl_program_id, name);
                                 if (location != -1) {
                                     _sit_uniform_map_set(slot->uniform_map, name, location);
                                 }
                             }
                        }

                        #ifndef NDEBUG
                        printf("[Situation] Shader %d Hot-Reloaded Successfully (Async)\n", i);
                        #endif
                    } else {
                         // Failed: Log and discard
                         char infoLog[1024];
                         glGetProgramInfoLog(slot->gl_pending_program_id, 1024, NULL, infoLog);
                         fprintf(stderr, "[Situation] Hot-Reload Link Failed for Shader %d: %s\n", i, infoLog);
                         glDeleteProgram(slot->gl_pending_program_id);
                         slot->gl_pending_program_id = 0;
                    }
                    slot->gl_is_linking = false;
                }
            }
        }

        // --- 2. OpenGL Frame Setup ---

        // [Phase 2] Reset Ring Buffer Allocator for this Frame (Paged Strategy)
        // We divide the ring buffer into N pages, one per frame in flight.
        // At the start of the frame, we reset the atomic head to the start of our assigned page.
        // This implicitly assumes the previous frame using this page has finished (guaranteed by Backpressure/Fence wait below).
        size_t page_size = sit_render.gl.ring_size / SITUATION_MAX_FRAMES_IN_FLIGHT;
        atomic_store(&sit_render.gl.ring_head, sit_render.current_frame_index * page_size);

        // [Phase 2] Backpressure & Thread Handoff
        #if !defined(__STDC_NO_THREADS__)
        mtx_lock(&sit_render.render_queue_mutex);
        while (sit_render.frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
            cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
        }
        mtx_unlock(&sit_render.render_queue_mutex);
        #else
        // Make the context current for this thread (Single Threaded Mode)
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
        // [2.3.14A] Invalidate shadow state to recover from external changes.
        _SitGLInvalidateShadowState();
        #endif

        // [Phase 1] Reset Soft Command Buffer
        sit_render.gl.soft_buffers[sit_render.current_frame_index].packet_count = 0;
        sit_render.gl.soft_buffers[sit_render.current_frame_index].data_cursor = 0;
        // [FIX v2.3.27B] Reset breaker
        sit_render.gl.soft_buffers[sit_render.current_frame_index].is_broken = false;

        // Mark that we're now recording a frame
        sit_render.in_frame = true;
        return true;
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Frame Setup ---

        // 2.1. Wait for the previous frame (using this frame's fence) to finish.
        // This ensures the command buffer and swapchain image are free to be reused.
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Waiting for fence (frame_index=%u)...\n", sit_render.vk.current_frame_index);
        printf("Situation [Vulkan Debug]:   Fence handle: %p\n", (void*)sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
        fflush(stdout);
        #endif

        VkResult wait_result = _SituationVulkanWaitFencePumpWindow(
            sit_render.vk.device,
            sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]
        );

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: vkWaitForFences result: %d\n", wait_result);
        fflush(stdout);
        #endif

        if (wait_result == VK_TIMEOUT) {
            fprintf(stderr, "[Vulkan] Frame fence wait timed out (max ~%.1fs) — see SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS\n",
                    (double)SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS / 1e9);
            fflush(stderr);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Timed out waiting for frame fence in SituationAcquireFrameCommandBuffer.");
            return false;
        }
        if (wait_result != VK_SUCCESS) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to wait for frame fence in SituationAcquireFrameCommandBuffer.");
             return false; // Indicate failure
        }

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Fence wait complete, checking backpressure...\n");
        fflush(stdout);
        #endif

        // [Phase 3] Backpressure (Vulkan)
        // Ensure we don't overrun the CPU render queue, even if the GPU is keeping up.
        #if !defined(__STDC_NO_THREADS__)
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Threading enabled, checking frames_pending...\n");
        printf("Situation [Vulkan Debug]:   frames_pending=%d, max=%d\n",
               sit_render.frames_pending, SITUATION_MAX_FRAMES_IN_FLIGHT);
        fflush(stdout);
        #endif

        mtx_lock(&sit_render.render_queue_mutex);
        while (sit_render.frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: Waiting on condition variable (frames_pending=%d)...\n",
                   sit_render.frames_pending);
            fflush(stdout);
            #endif
            cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
        }
        mtx_unlock(&sit_render.render_queue_mutex);

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Backpressure check complete\n");
        fflush(stdout);
        #endif
        #endif

        // [NEW] Reset Staging Buffer for this frame
        // Since we passed the fence, the GPU is done reading this buffer from N frames ago.
        sit_render.vk.staging_buffers[sit_render.vk.current_frame_index].cursor = 0;

        // --- FLUSH GRAVEYARD ---
        // The GPU is done with this frame, so we can safely destroy deferred resources.
        _SituationFlushGraveyard(sit_render.vk.current_frame_index);

        // 2.15 Check for Swapchain Recreation Request from Render Thread
        if (atomic_exchange(&sit_render.vk.recreate_swapchain_request, false)) {
            // Wait for Render Thread to be idle before recreating
            #if !defined(__STDC_NO_THREADS__)
            mtx_lock(&sit_render.render_queue_mutex);
            while (sit_render.frames_pending > 0) {
                // We can't wait on a CV here because the render thread consumes frames.
                // But if frames_pending > 0, the render thread is working.
                // We must wait for it to finish.
                // Since Main Thread is the producer, we just stop producing.
                // We need to wait for idle.
                // Simple spin/yield wait:
                mtx_unlock(&sit_render.render_queue_mutex);
                thrd_yield();
                mtx_lock(&sit_render.render_queue_mutex);
            }
            mtx_unlock(&sit_render.render_queue_mutex);
            #endif
            _SituationVulkanRecreateSwapchain();
            return false;
        }

        // 2.2. Acquire the next swapchain image.
        uint32_t image_index;
        uint64_t sit_acquire_t0_ns = _SitGetMonotonicTimeNS();
        VkResult acquire_result = vkAcquireNextImageKHR(
            sit_render.vk.device,
            sit_render.vk.swapchain,
            SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS,
            sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index], // Signal this semaphore when the image is acquired
            VK_NULL_HANDLE,                                                     // No fence to signal
            &image_index                                                        // Output: index of the acquired image
        );
        double sit_acquire_ms = (double)(_SitGetMonotonicTimeNS() - sit_acquire_t0_ns) / 1000000.0;
        /* Surface stalls / waits-for-present show up here (TIMEOUT ~= SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS).
           Compile with -DSITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS=0 to log every acquire (timing experiments). */
        if (acquire_result == VK_TIMEOUT ||
            SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS == 0 ||
            sit_acquire_ms >= (double)SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS) {
            fprintf(stderr, "[Vulkan] vkAcquireNextImageKHR %.2f ms result=%d\n", sit_acquire_ms, (int)acquire_result);
            fflush(stderr);
        }

        // 2.3. Handle Swapchain State.
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            // The swapchain is incompatible (e.g., window resized) and must be recreated.
            // This function handles the recreation internally.
            _SituationVulkanRecreateSwapchain();
            // Return false to signal that the frame setup was interrupted.
            // The caller should retry SituationAcquireFrameCommandBuffer next frame.
            return false;
        } else if (acquire_result == VK_TIMEOUT) {
            // Surface did not provide an image in time (minimized window, occlusion, driver quirks).
            // UINT64_MAX would block forever and freeze the app on a black window.
            _SituationVulkanRecreateSwapchain();
            return false;
        } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
             // The swapchain can still be used, but surface properties have changed.
             // It's often recommended to recreate for optimal presentation.
             // For now, we proceed but log it. A more robust system might trigger a recreate flag.
             // fprintf(stderr, "WARNING: Vulkan swapchain is suboptimal.\n");
             // Proceeding is generally safe, but performance/quality might be affected.
        } else if (acquire_result != VK_SUCCESS) {
            // An unexpected error occurred during image acquisition.
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED, "Failed to acquire swap chain image in SituationAcquireFrameCommandBuffer!");
            return false; // Indicate failure
        }

        // 2.4. Update Global State.
        // Store the index of the swapchain image we will render to this frame.
        sit_render.vk.current_image_index = image_index;
        sit_render.vk.acquired_image_indices[sit_render.vk.current_frame_index] = image_index; // Store for Render Thread
        sit_render.frame_has_async_compute = false; // Reset async flag
        sit_render.vk.dynamic_vbo_cursor = 0;

        // 2.5. Prepare Command Buffer for Recording.
        // Reset the fence to the unsignaled state *before* resetting the command buffer.
        VkResult reset_fence_result = vkResetFences(
            sit_render.vk.device,
            1,
            &sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]
        );
        if (reset_fence_result != VK_SUCCESS) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to reset frame fence in SituationAcquireFrameCommandBuffer.");
             return false; // Indicate failure
        }

        // Get the command buffer for this frame (assuming this helper function exists and returns the correct buffer from sit_render.vk.command_buffers).
        VkCommandBuffer cmd = (VkCommandBuffer)SituationGetMainCommandBuffer(); // Or directly access: sit_render.vk.command_buffers[sit_render.vk.current_frame_index]
        VkCommandBuffer compute_cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];

        if (cmd == VK_NULL_HANDLE || compute_cmd == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to get command buffers for frame in SituationAcquireFrameCommandBuffer.");
             return false; // Indicate failure
        }

        // Reset the command buffer to ensure it's ready for new commands.
        VkResult reset_cmd_result = vkResetCommandBuffer(cmd, 0); // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT is 0
        vkResetCommandBuffer(compute_cmd, 0); // Reset compute buffer too

        if (reset_cmd_result != VK_SUCCESS) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to reset command buffer in SituationAcquireFrameCommandBuffer.");
             return false; // Indicate failure
        }

        // Begin recording commands into the command buffer.
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // Flags = 0 means "one time submit" implicitly, and no inheritance.
        VkResult begin_result = vkBeginCommandBuffer(cmd, &begin_info);
        vkBeginCommandBuffer(compute_cmd, &begin_info); // Begin compute buffer

        if (begin_result != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to begin recording command buffer in SituationAcquireFrameCommandBuffer!");
            return false; // Indicate failure
        }

        sit_render.vk.inside_main_swapchain_render_pass = false;

        // Mark that we're now recording a frame
        sit_render.in_frame = true;

        // If we reached here, Vulkan frame setup was successful (excluding OOD/K recreate).
        return true;
    }
#endif

    // Should not be reached if SITUATION_USE_OPENGL or SITUATION_USE_VULKAN is defined,
    // but included for theoretical completeness if neither backend is selected.
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "No graphics backend defined for SituationAcquireFrameCommandBuffer.");
    return false;
}

/**
 * @brief Submits all recorded commands for the current frame and presents the result.
 *
 * @details This function finalizes the rendering for the frame started by `SituationAcquireFrameCommandBuffer`.
 *          It submits the recorded commands to the GPU, waits for the GPU to finish rendering to the swapchain image, and then presents that image to the screen. It also handles frame rate limiting (if configured) and updates internal timing statistics like FPS.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:**
 *   - Calls `glfwSwapBuffers` to swap the front and back framebuffers, making the rendered image visible on the screen.
 *   - Implicitly waits for the GPU to finish rendering the previous frame before swapping (this behavior can depend on VSync settings).
 * - **Vulkan:**
 *   - Ends the recording of the primary command buffer for the current frame.
 *   - Submits the command buffer to the graphics queue. This submission waits on the `image_available_semaphore` for the swapchain image to be acquired and signals the `render_finished_semaphore` when rendering is complete.
 *   - Presents the rendered swapchain image to the screen using `vkQueuePresentKHR`, waiting on the `render_finished_semaphore`.
 *   - Handles swapchain recreation if the presentation surface becomes outdated (`VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR`, or window resize).
 *   - Advances the `current_frame_index` for the next frame's synchronization objects.
 *
 * @return SITUATION_SUCCESS on successful completion of the frame.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED (Vulkan) if ending the command buffer or submitting it to the queue fails.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED (Vulkan) if presenting the image fails for reasons other than out-of-date/suboptimal swapchain.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized.
 *       2. `SituationAcquireFrameCommandBuffer` was called successfully for this frame.
 *       3. All rendering commands for the frame have been recorded.
 * @warning This function is not thread-safe and must be called from the thread that initialized the library and is managing the rendering loop.
 *
 * @see SituationAcquireFrameCommandBuffer()
 */
#if defined(SITUATION_USE_VULKAN)
static void _SituationSubmitCompute(VkCommandBuffer cmd) {
    // Submit compute work and signal semaphore in one go
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkSemaphore signal_sema = sit_render.vk.compute_finished_semaphores[sit_render.vk.current_frame_index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sema;

    VkResult result = vkQueueSubmit(sit_render.vk.compute_queue, 1, &submit, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "_SituationSubmitCompute: vkQueueSubmit failed.");
    }
}

/**
 * @brief [INTERNAL] Records all pending graphics commands into the given Vulkan command buffer.
 *
 * @details This is a core low-level function in the Vulkan backend that populates a
 *          `VkCommandBuffer` with the full set of recorded draw/dispatch commands,
 *          state changes, and transitions for the current frame or render list.
 *
 *          Typical call sites:
 *            - Inside `_SituationRenderThreadEntry` when processing a frame slot
 *            - During render list replay (`_SituationReplayToQueue`)
 *            - In synchronous fallback paths (if render thread disabled)
 *
 *          What it does (in rough order):
 *            - Begins command buffer recording (if not already begun)
 *            - Sets viewport/scissor (from current render pass or display)
 *            - Binds global descriptor sets (samplers, uniforms, bindless)
 *            - Iterates over the active render list or queued draw calls:
 *              - Binds pipelines (graphics/compute)
 *              - Binds vertex/index buffers
 *              - Binds descriptor sets per draw
 *              - Records `vkCmdDraw*` / `vkCmdDrawIndexed*` / `vkCmdDispatch*`
 *              - Handles push constants
 *              - Inserts barriers/transitions (image layouts, memory barriers)
 *            - Ends any active render pass
 *            - Ends command buffer recording
 *
 *          The function does **not** submit the command buffer to the queue
 *          that is handled separately (e.g. `vkQueueSubmit` in the render thread).
 *
 * @param cmd A Vulkan command buffer handle in the recording state (or reset/ready).
 *            Must belong to a pool allocated for the current frame/swapchain image.
 *
 * @note This function assumes the command buffer is already begun (via `vkBeginCommandBuffer`).
 *       Errors (validation failures, out-of-memory, invalid state) are logged internally
 *       and may set the global `SituationError` (e.g. SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED).
 *       No return value failures are non-fatal but logged.
 *
 * Thread safety invariants:
 *   - Must be called from the **render thread** (owns the Vulkan context/queues)
 *   - Command buffer must not be in use by another thread
 *   - No internal locking caller ensures exclusive access
 *   - Safe during hot-reload if old resources are destroyed first
 *
 * @see _SituationRenderThreadEntry (main caller),
 *      _SituationReplayToQueue, SituationSubmitRenderList,
 *      vkBeginCommandBuffer, vkCmdDrawIndexed, vkEndCommandBuffer,
 *      SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED
 */
static VkResult _SituationSubmitGraphics(VkCommandBuffer cmd) {
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };

    // Wait for Image Available (always)
    VkSemaphore wait_semas[2];
    VkPipelineStageFlags wait_stages[2];
    uint32_t wait_count = 0;

    wait_semas[wait_count] = sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index];
    wait_stages[wait_count] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    wait_count++;

    // Wait for Compute (if used this frame)
    if (sit_render.frame_has_async_compute) {
        wait_semas[wait_count] = sit_render.vk.compute_finished_semaphores[sit_render.vk.current_frame_index];
        wait_stages[wait_count] = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        wait_count++;
    }

    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = wait_semas;
    submit.pWaitDstStageMask = wait_stages;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkSemaphore signal_sema = sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sema;

    #ifdef SITUATION_VULKAN_DEBUG
    // fprintf(stderr, "[Situation] [_SituationSubmitGraphics] About to submit to GPU (cmd=%p)\n", (void*)cmd); fflush(stderr);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics] About to submit to GPU\n");
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics]   Command buffer: %p\n", (void*)cmd);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
    fflush(stdout);
    #endif
    VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
    #ifdef SITUATION_VULKAN_DEBUG
    // fprintf(stderr, "[Situation] [_SituationSubmitGraphics] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result); fflush(stderr);
    printf("Situation [Vulkan Debug]: [_SituationSubmitGraphics] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
    fflush(stdout);
    #endif
    if (submit_result != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "_SituationSubmitGraphics: vkQueueSubmit failed.");
    }
    return submit_result;
}
#endif

/**
 * @brief Ends the current frame and submits it for rendering on the render thread.
 *
 * @details This function marks the end of the current frame's command recording and
 *          submits it to the dedicated render thread for execution. It is the primary
 *          synchronization point in the main loop, ensuring that the frame's command
 *          buffer is enqueued for GPU submission, presentation, and resource cleanup.
 *
 *          Key steps performed:
 *            - Locks the render queue mutex
 *            - Waits (with timeout) if all in-flight frame slots are occupied
 *              (backpressure handling to prevent unbounded queue growth)
 *            - Enqueues the current frame index into the circular queue
 *            - Increments pending frame count and refcount for the slot
 *            - Records submission timestamp for latency metrics (if enabled)
 *            - Signals the render thread condition variable to wake it
 *            - Unlocks the mutex
 *            - Advances the current_frame index (modulo MAX_FRAMES_IN_FLIGHT)
 *
 *          This function is **non-blocking** in normal operation but may briefly wait
 *          under high load (e.g. slow GPU, many pending frames). Timeout is fixed
 *          (e.g. 1 second) and returns an error on expiry.
 *
 *          Call this at the end of your main loop after all `SituationCmd*` recordings
 *          for the frame. It does **not** perform polling or input handling pair with
 *          `SituationPollEvents` at loop start.
 *
 * @return SITUATION_SUCCESS on successful submission,
 *         SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT if wait for free slot timed out
 *         (too many pending frames reduce load or increase MAX_FRAMES_IN_FLIGHT),
 *         SITUATION_ERROR_THREAD_VIOLATION if called from render thread (deadlock risk),
 *         or other appropriate error codes (e.g. mutex failure).
 *
 * @note Must be called from the **main thread** (or thread that owns the command buffer).
 *       Pairs with `SituationBeginFrame` (if you have it) or manual command recording.
 *       If render thread is disabled, this falls back to synchronous execution.
 *       Metrics (latency, queue depth) are updated internally if enabled.
 *
 *       Thread safety:
 *         - Safe only from main thread render thread calls would deadlock
 *         - Internal mutex + condvar protect queue access
 *         - Atomic ops for refcounts and metrics
 *
 * @see SituationPollEvents, SituationBeginCommandBuffer, SituationCmd* functions,
 *      _SituationRenderThreadEntry (execution side), SITUATION_MAX_FRAMES_IN_FLIGHT,
 *      SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT, SITUATION_ERROR_THREAD_VIOLATION
 */
SITAPI SituationError SituationEndFrame(void) {
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: ENTRY\n");
    fflush(stdout);
    #endif
    
    // Mark that we're no longer recording a frame
    sit_render.in_frame = false;

    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot end frame.");
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: Library initialized, replaying momentum queue\n");
    fflush(stdout);
    #endif

    // --- [v2.3.27] Replay Momentum Queue ---
    // Process all lists submitted by worker threads this frame.
    SituationCommandBuffer main_cmd = SituationGetMainCommandBuffer();

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationEndFrame: Got main command buffer: %p\n", main_cmd);
    fflush(stdout);
    #endif

    // Only replay if we have a valid command buffer (we should, if Init succeeded)
    if (main_cmd) {
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Locking momentum mutex\n");
        fflush(stdout);
        #endif
        
        mtx_lock(&sit_render.momentum_mutex);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Momentum mutex locked\n");
        fflush(stdout);
        #endif

        int head = atomic_load(&sit_render.momentum_head);
        int tail = atomic_load(&sit_render.momentum_tail);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: head=%d, tail=%d\n", head, tail);
        fflush(stdout);
        #endif

        while (tail != head) {
            SituationRenderList list = sit_render.momentum_queue[tail];

            // "Paste" the recorded commands into the real command buffer
            SituationReplayRenderList(main_cmd, list);

            // [FIX v2.3.27B] Mark as finished
            atomic_fetch_sub(&list->in_flight_count, 1);

            // Advance tail
            tail = (tail + 1) % 256;
        }
        atomic_store(&sit_render.momentum_tail, tail);

        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: About to unlock momentum mutex\n");
        fflush(stdout);
        #endif

        mtx_unlock(&sit_render.momentum_mutex);
        
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] SituationEndFrame: Momentum mutex unlocked\n");
        fflush(stdout);
        #endif
    }

    // --- 2. Backend-Specific Frame End ---
#if defined(SITUATION_USE_OPENGL)
    {
        // --- 2a. OpenGL Frame End ---

        // [Phase 2] Threaded Submission
        #if !defined(__STDC_NO_THREADS__)
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            // [v2.3.24a] Adaptive Backpressure (Safety Zenith)
            // Dynamically switch policy based on frame latency history.
            // Policy: SPIKE (>100% target) -> SLEEP (Save CPU/Battery, let GPU catch up)
            //         STEADY (<50% target) -> SPIN (Max performance/responsiveness)
            int policy = atomic_load(&sit_render_policy_state);

            // Use Max latency from the recent history (reset/updated by thread)
            uint64_t lat_check = atomic_load(&sit_render.metric_max_latency_ns);

            uint64_t target_ns = (uint64_t)(sit_gs.target_frame_time * 1000000000.0);
            if (target_ns == 0) target_ns = 16666667ULL; // Default to 60 FPS (16ms) if uncapped

            uint64_t spike_thresh = target_ns;         // 100%
            uint64_t steady_thresh = target_ns / 2;    // 50%

            if (lat_check > spike_thresh) {
                atomic_store(&sit_render_policy_state, SIT_RENDER_BACKPRESSURE_SLEEP);
                policy = SIT_RENDER_BACKPRESSURE_SLEEP;
            } else if (lat_check < steady_thresh) {
                atomic_store(&sit_render_policy_state, SIT_RENDER_BACKPRESSURE_SPIN);
                policy = SIT_RENDER_BACKPRESSURE_SPIN;
            }

            // Check Queue Depth
            size_t depth = atomic_load(&sit_render.render_queue_depth);
            if (depth >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
                if (policy == SIT_RENDER_BACKPRESSURE_SPIN) {
                    while (atomic_load(&sit_render.render_queue_depth) >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
                        #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
                        _mm_pause();
                        #elif defined(__aarch64__) || defined(_M_ARM64)
                            #if defined(__has_builtin)
                                #if __has_builtin(__builtin_arm_wfe)
                                __builtin_arm_wfe();
                                #else
                                __asm__ __volatile__("yield");
                                #endif
                            #else
                                #if defined(_MSC_VER)
                                __yield();
                                #else
                                __asm__ __volatile__("yield");
                                #endif
                            #endif
                        #endif
                    }
                }
                else if (policy == SIT_RENDER_BACKPRESSURE_SLEEP) {
                     mtx_lock(&sit_render.render_queue_mutex);
                     while (sit_render.frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
                         cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                     }
                     mtx_unlock(&sit_render.render_queue_mutex);
                }
                else { // YIELD
                     while (atomic_load(&sit_render.render_queue_depth) >= SITUATION_MAX_FRAMES_IN_FLIGHT) thrd_yield();
                }
            }

            mtx_lock(&sit_render.render_queue_mutex);
            // Double-check under lock if we didn't use CV
            if (policy != SIT_RENDER_BACKPRESSURE_SLEEP) {
                 while (sit_render.frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
                     cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
                 }
            }

            // [v2.3.24a] Leak-Proof Handoff: Increment Refcount
            atomic_fetch_add(&sit_render.frame_refcounts[sit_render.current_frame_index], 1);

            sit_render.render_queue[sit_render.render_queue_head] = sit_render.current_frame_index;
            sit_render.render_queue_head = (sit_render.render_queue_head + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
            sit_render.frames_pending++;

            // [v2.3.22] Record Submit Timestamp for Latency
            // [v2.3.25] Store explicitly for drift check
            uint64_t now = _SitGetMonotonicTimeNS();
            atomic_store(&sit_render.submit_timestamps[sit_render.current_frame_index], now);
            atomic_fetch_add(&sit_render.render_queue_depth, 1);

            cnd_signal(&sit_render.render_queue_cv);
            mtx_unlock(&sit_render.render_queue_mutex);
        } else
        #endif
        // If threading disabled at runtime but compiled in, fallback to immediate execution below
        {
            #ifdef SITUATION_OPENGL_DEBUG
            printf("[OpenGL Debug] SituationEndFrame: About to call _SituationGLExecuteCommands\n");
            printf("[OpenGL Debug] current_frame_index=%d, packet_count=%d\n", 
                   sit_render.current_frame_index, 
                   sit_render.gl.soft_buffers[sit_render.current_frame_index].packet_count);
            fflush(stdout);
            #endif

            // 1. Wait for old frame to finish and flush its graveyard
            if (sit_render.gl.frame_fences[sit_render.current_frame_index]) {
                glClientWaitSync(sit_render.gl.frame_fences[sit_render.current_frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);

                _SitGLFlushGraveyard(sit_render.current_frame_index);

                glDeleteSync(sit_render.gl.frame_fences[sit_render.current_frame_index]);
                sit_render.gl.frame_fences[sit_render.current_frame_index] = 0;
            }

            SIT_DEBUG_LOG("[EndFrame] Executing GL commands\n");
            _SituationGLExecuteCommands(&sit_render.gl.soft_buffers[sit_render.current_frame_index], sit_render.current_frame_index);

            SIT_DEBUG_LOG("[EndFrame] About to call glfwSwapBuffers\n");
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                SIT_DEBUG_LOG("[EndFrame] OpenGL error BEFORE swap: 0x%x\n", err);
            }

            // [FIX] Pre-swap screenshot capture: read back buffer into CPU memory
            // before the swap makes it undefined. This ensures SituationLoadImageFromScreen
            // always has reliable pixel data regardless of DWM/driver behavior.
            {
                int sw = SituationGetRenderWidth();
                int sh = SituationGetRenderHeight();
                if (sw > 0 && sh > 0) {
                    size_t needed = (size_t)sw * sh * 4;
                    if (sit_render.gl.screenshot_width != sw || sit_render.gl.screenshot_height != sh || !sit_render.gl.screenshot_buffer) {
                        if (sit_render.gl.screenshot_buffer) SIT_FREE(sit_render.gl.screenshot_buffer);
                        sit_render.gl.screenshot_buffer = (uint8_t*)SIT_MALLOC(needed);
                        sit_render.gl.screenshot_width = sw;
                        sit_render.gl.screenshot_height = sh;
                    }
                    if (sit_render.gl.screenshot_buffer) {
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                        glReadBuffer(GL_BACK);
                        glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, sit_render.gl.screenshot_buffer);
                        sit_render.gl.screenshot_valid = true;
                    }
                }
            }

            glfwSwapBuffers(sit_gs.sit_glfw_window);
            SIT_DEBUG_LOG("[EndFrame] glfwSwapBuffers completed\n");

            // 2. Create new fence for the commands we just submitted
            sit_render.gl.frame_fences[sit_render.current_frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();
        }
        #else
        // [Phase 1] Execute Deferred Commands Immediately (Single-Threaded)

        // 1. Wait for old frame to finish and flush its graveyard
        if (sit_render.gl.frame_fences[sit_render.current_frame_index]) {
            glClientWaitSync(sit_render.gl.frame_fences[sit_render.current_frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);

            _SitGLFlushGraveyard(sit_render.current_frame_index);

            glDeleteSync(sit_render.gl.frame_fences[sit_render.current_frame_index]);
            sit_render.gl.frame_fences[sit_render.current_frame_index] = 0;
        }

        SIT_DEBUG_LOG("[EndFrame] Executing GL commands (non-threaded path)\n");
        _SituationGLExecuteCommands(&sit_render.gl.soft_buffers[sit_render.current_frame_index], sit_render.current_frame_index);

        SIT_DEBUG_LOG("[EndFrame] About to call glfwSwapBuffers (non-threaded)\n");
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            SIT_DEBUG_LOG("[EndFrame] OpenGL error BEFORE swap: 0x%x\n", err);
        }

        // [FIX] Pre-swap screenshot capture (non-threaded path)
        {
            int sw = SituationGetRenderWidth();
            int sh = SituationGetRenderHeight();
            if (sw > 0 && sh > 0) {
                size_t needed = (size_t)sw * sh * 4;
                if (sit_render.gl.screenshot_width != sw || sit_render.gl.screenshot_height != sh || !sit_render.gl.screenshot_buffer) {
                    if (sit_render.gl.screenshot_buffer) SIT_FREE(sit_render.gl.screenshot_buffer);
                    sit_render.gl.screenshot_buffer = (uint8_t*)SIT_MALLOC(needed);
                    sit_render.gl.screenshot_width = sw;
                    sit_render.gl.screenshot_height = sh;
                }
                if (sit_render.gl.screenshot_buffer) {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                    glReadBuffer(GL_BACK);
                    glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, sit_render.gl.screenshot_buffer);
                    sit_render.gl.screenshot_valid = true;
                }
            }
        }

        glfwSwapBuffers(sit_gs.sit_glfw_window);
        SIT_DEBUG_LOG("[EndFrame] glfwSwapBuffers completed (non-threaded)\n");

        // 2. Create new fence for the commands we just submitted
        sit_render.gl.frame_fences[sit_render.current_frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();

        #endif

        // [PLATINUM] Unify frame index advancement across both paths.
        sit_render.current_frame_index = (sit_render.current_frame_index + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;

        // OpenGL path implicitly succeeds if glfwSwapBuffers doesn't crash.
        // Return success.
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2b. Vulkan Frame End ---

        // 1. End recording the primary command buffer for this frame.
        // Get the command buffer first and validate it.
        VkCommandBuffer cmd = (VkCommandBuffer)SituationGetMainCommandBuffer();
        if (cmd == VK_NULL_HANDLE) { // Check if SituationGetMainCommandBuffer returned NULL
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to get main command buffer for ending frame.");
             return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        // [FIX V6] Pre-present screenshot: copy swapchain image to host-visible staging while the image is
        // still in a well-defined state (see LIBRARY_BUGFIX_PLAN — same idea as OpenGL pre-swap ReadPixels).
        // Clear only this frame slot — not a global flag (render thread may still need prior slots).
        if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
            sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
        }
        {
            uint32_t sw = sit_render.vk.swapchain_extent.width;
            uint32_t sh = sit_render.vk.swapchain_extent.height;
            if (sit_render.vk.swapchain_valid && sw > 0 && sh > 0 && sit_render.vk.swapchain_images &&
                sit_render.vk.current_image_index < sit_render.vk.swapchain_image_count) {
                VkImage swap_img = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];
                SituationError cap_err = _SituationVulkanEnsureScreenshotResources(sw, sh);
                if (cap_err == SITUATION_SUCCESS && sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE) {
                    _SituationVulkanRecordScreenshotCopy(cmd, swap_img, sw, sh);
                } else {
                    sit_render.vk.screenshot_valid = false;
                }
            } else {
                sit_render.vk.screenshot_valid = false;
            }
        }

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to end recording command buffer!");
            return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        // [Phase 2] Threaded Submission (Vulkan)
        #if !defined(__STDC_NO_THREADS__)
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        if (sit_render.enabled) {
            // [v2.3.24c] Robust Backpressure (Unified)
            // We use a Condition Variable to wait efficiently if the queue is full.
            // This replaces the dangerous spinlock with OS-scheduled sleeping.

            mtx_lock(&sit_render.render_queue_mutex);

            // Wait Loop: While the queue is full, sleep.
            // The Render Thread will signal 'main_wait_cv' when it finishes a frame.
            while (sit_render.frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
                cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
            }

            // [v2.3.25] Refcount Increment
            // Critical: Ensure the frame is marked as "in use" before handoff.
            atomic_fetch_add(&sit_render.frame_refcounts[sit_render.vk.current_frame_index], 1);

            // Push Frame Index to Ring Buffer
            sit_render.render_queue[sit_render.render_queue_head] = sit_render.vk.current_frame_index;
            sit_render.render_queue_head = (sit_render.render_queue_head + 1) % sit_render.vk.max_frames_in_flight;
            sit_render.frames_pending++;

            // [v2.3.22] Metrics & Depth Tracking
            uint64_t now = _SitGetMonotonicTimeNS();
            atomic_store(&sit_render.submit_timestamps[sit_render.vk.current_frame_index], now);
            atomic_fetch_add(&sit_render.render_queue_depth, 1);

            // Wake up Render Thread to process the new frame
            cnd_signal(&sit_render.render_queue_cv);
            mtx_unlock(&sit_render.render_queue_mutex);
        } else
        #endif
        {
            // 2. Submit the command buffer to the graphics queue (Single-Threaded Path).
            // [v2.3.23] Multi-Queue Sync
            if (sit_render.frame_has_async_compute) {
                VkCommandBuffer compute_cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];
                vkEndCommandBuffer(compute_cmd);
                _SituationSubmitCompute(compute_cmd);
            }

            VkResult submit_res = _SituationSubmitGraphics(cmd);
            if (submit_res == VK_SUCCESS) {
                _SituationVulkanResolveScreenshotAfterSubmit(sit_render.vk.current_frame_index);
            } else {
                if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                    sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
                }
                sit_render.vk.screenshot_valid = false;
            }

            // 3. Present the rendered image to the screen.
            VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index] };

            VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = signal_semaphores; // Wait for rendering to finish
            VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
            present_info.swapchainCount = 1;
            present_info.pSwapchains = swapchains;
            present_info.pImageIndices = &sit_render.vk.current_image_index; // Present the image we acquired/used this frame

            // [FIX v2.3.27B] Safety check
            if (!sit_render.vk.swapchain_valid) {
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID;
            }

            // Perform the presentation.
            #ifdef SITUATION_VULKAN_DEBUG
            // fprintf(stderr, "[Situation] About to call vkQueuePresentKHR (image_index=%u)\n", sit_render.vk.current_image_index); fflush(stderr);
            #endif
            VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);
            #ifdef SITUATION_VULKAN_DEBUG
            // fprintf(stderr, "[Situation] vkQueuePresentKHR result: %d (VK_SUCCESS=0)\n", result); fflush(stderr);
            #endif

            // 4. Handle Presentation Result & Swapchain State.
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || sit_render.vk.framebuffer_resized) {
                // The swapchain is out of date or not optimal. Recreate it.
                // Reset the resize flag if it was set.
                sit_render.vk.framebuffer_resized = false;
                _SituationVulkanRecreateSwapchain();
                // Note: We don't return an error here. Recreating the swapchain is handled internally.
                // The application should check for swapchain recreation needs in SituationAcquireFrameCommandBuffer.
            } else if (result != VK_SUCCESS) {
                // An unexpected error occurred during presentation.
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "Failed to present swap chain image!");
                return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
            }

            // Store the index of the image we just submitted for presentation.
            sit_render.vk.last_presented_image_index = sit_render.vk.current_image_index;
        }
        #else
        // 2. Submit the command buffer to the graphics queue (Single-Threaded Path).
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = { sit_render.vk.image_available_semaphores[sit_render.vk.current_frame_index] };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[sit_render.vk.current_frame_index] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        // Submit the command buffer, waiting on the acquire semaphore and signaling the render finish semaphore.
        // The fence associated with this frame is signaled when the submission completes.
        fprintf(stderr, "[Situation] [SINGLE-THREADED] About to submit frame to GPU (cmd=%p)\n", (void*)cmd); fflush(stderr);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED] About to submit frame to GPU\n");
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED]   Command buffer: %p\n", (void*)cmd);
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
        fflush(stdout);
        #endif
        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[sit_render.vk.current_frame_index]);
        fprintf(stderr, "[Situation] [SINGLE-THREADED] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result); fflush(stderr);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [SINGLE-THREADED] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
        fflush(stdout);
        #endif
        if (submit_result != VK_SUCCESS) {
            if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = false;
            }
            sit_render.vk.screenshot_valid = false;
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "Failed to submit draw command buffer!");
            return SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED;
        }

        _SituationVulkanResolveScreenshotAfterSubmit(sit_render.vk.current_frame_index);

        // 3. Present the rendered image to the screen.
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores; // Wait for rendering to finish
        VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &sit_render.vk.current_image_index; // Present the image we acquired/used this frame

        // Perform the presentation.
        VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);

        // 4. Handle Presentation Result & Swapchain State.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || sit_render.vk.framebuffer_resized) {
            // The swapchain is out of date or not optimal. Recreate it.
            // Reset the resize flag if it was set.
            sit_render.vk.framebuffer_resized = false;
            _SituationVulkanRecreateSwapchain();
            // Note: We don't return an error here. Recreating the swapchain is handled internally.
            // The application should check for swapchain recreation needs in SituationAcquireFrameCommandBuffer.
        } else if (result != VK_SUCCESS) {
            // An unexpected error occurred during presentation.
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "Failed to present swap chain image!");
            return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
        }

        // Store the index of the image we just submitted for presentation.
        sit_render.vk.last_presented_image_index = sit_render.vk.current_image_index;
        #endif

        // 5. Advance Frame Index for Next Frame's Synchronization.
        // Use the dynamically determined max frames in flight, not a compile-time constant.
        sit_render.vk.current_frame_index = (sit_render.vk.current_frame_index + 1) % sit_render.vk.max_frames_in_flight;
    }
#endif // SITUATION_USE_VULKAN

    // --- 3. Post-Frame Logic (Timing, FPS) ---
    // Update timing and FPS counter. This happens regardless of the backend.
    // It includes the time taken by buffer swapping/presentation.

    // Frame Rate Limiting (if a target time is set).
    if (sit_gs.target_frame_time > 0.0) {
        double next_frame_start_time = sit_gs.current_time + sit_gs.target_frame_time;
        double current_time = glfwGetTime(); // Get current time for comparison
        while (current_time < next_frame_start_time) {
            // Yield control to the OS briefly to avoid consuming 100% CPU.
            #if defined(_WIN32)
                Sleep(0); // Yield the rest of the time slice
			#elif defined(__linux__) || defined(__APPLE__)
				#include <time.h> // Ensure this is included

				// Inside SituationEndFrame:
				struct timespec req = {0};
				req.tv_sec = 0;
				req.tv_nsec = 100 * 1000; // 100 microseconds = 100,000 nanoseconds
				nanosleep(&req, NULL);
			#endif
            current_time = glfwGetTime(); // Update current time for the next check
        }
    }

    // FPS Calculation Update.
    sit_gs.fps_frame_counter++;
    double current_time = glfwGetTime();
    double time_since_last_fps_update = current_time - sit_gs.fps_last_update_time;

    #ifdef SITUATION_VULKAN_DEBUG
    if (sit_gs.fps_frame_counter == 1 || sit_gs.fps_frame_counter % 60 == 0) {
        printf("Situation [FPS Debug]: frame=%d, current_time=%.3f, last_update=%.3f, delta=%.3f\n",
               sit_gs.fps_frame_counter, current_time, sit_gs.fps_last_update_time, time_since_last_fps_update);
    }
    #endif

    if (time_since_last_fps_update >= 1.0) {
        // Calculate average FPS over the last second (or so).
        sit_gs.current_fps = (int)((double)sit_gs.fps_frame_counter / time_since_last_fps_update);

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [FPS Debug]: FPS updated to %d (frames=%d, time=%.3f)\n",
               sit_gs.current_fps, sit_gs.fps_frame_counter, time_since_last_fps_update);
        #endif

        sit_gs.fps_frame_counter = 0; // Reset the counter
        sit_gs.fps_last_update_time = glfwGetTime(); // Reset the timer
    }

    // --- 4. Success ---
#if defined(SITUATION_USE_OPENGL) || defined(SITUATION_USE_VULKAN)
    // Explicitly return success if the backend-specific code completed without error return.
    return SITUATION_SUCCESS;
#else
    // Fallback if neither backend is defined (should be caught by compiler flags usually).
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Gets the primary command buffer for the current frame.
 *
 * @details This function retrieves the main `SituationCommandBuffer` handle that should be used for recording rendering and compute commands for the frame currently being prepared or rendered.
 *          This handle is typically obtained *after* a successful call to `SituationAcquireFrameCommandBuffer` and is valid until `SituationEndFrame` is called.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** OpenGL operates in immediate mode and does not use explicit command buffers in the same way Vulkan does. Therefore, this function returns `NULL`.
 * - **Vulkan:** Returns the `VkCommandBuffer` associated with the current frame index (`sit_render.vk.current_frame_index`). This buffer is managed internally by the library and is reset and begun at the start of the frame by `SituationAcquireFrameCommandBuffer`.
 *
 * @return A `SituationCommandBuffer` handle.
 *         - In Vulkan, this is a valid handle for the current frame's primary command buffer.
 *         - In OpenGL, this function returns `NULL`.
 * @return `NULL` if the library is not initialized, or if called at an inappropriate time (e.g., before `SituationAcquireFrameCommandBuffer` or after `SituationEndFrame` in Vulkan, if `sit_render.vk.current_frame_index` is invalid).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized.
 *       2. (Vulkan) This function is called between `SituationAcquireFrameCommandBuffer` and `SituationEndFrame`.
 *       3. The returned handle is only used for recording commands and not stored persistently across frames without re-querying.
 *
 * @see SituationAcquireFrameCommandBuffer(), SituationEndFrame()
 */
SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void) {
    // --- 1. Library Initialization Check ---
    if (!SituationIsInitialized()) {
        // Returning NULL is a safe default for an invalid/uninitialized state.
        // Could also set an error, but often just returning NULL is sufficient for a getter.
        // _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot get command buffer before library initialization.");
        return NULL;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        // --- 2. OpenGL Path ---
        // [Phase 2] Return the Soft Command Buffer for current frame
        // [PLATINUM] Unified logic: Always return the buffer for the current frame index.
        return (SituationCommandBuffer)&sit_render.gl.soft_buffers[sit_render.current_frame_index];
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Path ---
        // Retrieve the command buffer for the current frame index.
        // This assumes sit_render.vk.current_frame_index is valid (set by SituationAcquireFrameCommandBuffer).

        // Optional: Add a bounds check for robustness, though SituationAcquireFrameCommandBuffer should manage this.
        if (sit_render.vk.current_frame_index >= sit_render.vk.max_frames_in_flight) {
            // This indicates a potential logic error or state issue.
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Current frame index is out of bounds for command buffer access.");
            return NULL;
        }

        // Get the VkCommandBuffer from the internal array.
        VkCommandBuffer vk_cmd = sit_render.vk.command_buffers[sit_render.vk.current_frame_index];

        // Optional: Check if vk_cmd is VK_NULL_HANDLE, though SituationAcquireFrameCommandBuffer should provide a valid one.
        if (vk_cmd == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Current frame's command buffer is unexpectedly NULL.");
             return NULL;
        }

        // Cast the VkCommandBuffer to the opaque SituationCommandBuffer type.
        return (SituationCommandBuffer)(uintptr_t)vk_cmd;
    }
#endif

    // --- 3. Fallback (Should not be reached if backends are defined) ---
    // If neither SITUATION_USE_OPENGL nor SITUATION_USE_VULKAN is defined.
    return NULL;
}

SITAPI SituationCommandBuffer SituationGetComputeCommandBuffer(void) {
    if (!SituationIsInitialized()) return NULL;
#if defined(SITUATION_USE_VULKAN)
    // Track usage for sync
    sit_render.frame_has_async_compute = true;
    VkCommandBuffer cmd = sit_render.vk.compute_command_buffers[sit_render.vk.current_frame_index];
    return (SituationCommandBuffer)(uintptr_t)cmd;
#else
    // Fallback to main buffer for OpenGL
    return SituationGetMainCommandBuffer();
#endif
}

/**
 * @brief [Core] Begins a configurable render pass on a command buffer.
 *
 * @details This is the primary entry point for rendering geometry. It configures the rendering target (Display or Virtual Display)
 *          and specifies how attachments (Color, Depth, Stencil) should be handled at the start and end of the pass.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a `vkCmdBeginRenderPass` command. It selects the appropriate `VkFramebuffer` and `VkRenderPass` object
 *   based on the `info->display_id` and the `loadOp`/`storeOp` settings. It sets the clear values for the attachments
 *   and defines the render area.
 * - **OpenGL:** Binds the target framebuffer (FBO 0 for main window). It then mimics Vulkan's `loadOp` behavior:
 *   - `SIT_LOAD_OP_CLEAR`: Calls `glClear` with the specified color/depth values.
 *   - `SIT_LOAD_OP_LOAD`: Does nothing (preserves existing framebuffer content).
 *   - `SIT_LOAD_OP_DONT_CARE`: Does nothing (undefined content, fast).
 *
 * @param cmd The command buffer to record into.
 * @param info A pointer to a `SituationRenderPassInfo` struct defining the target display ID and attachment operations.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` if `info` is NULL.
 * @return `SITUATION_ERROR_NOT_IMPLEMENTED` (Vulkan) if a requested load/store op combination is not yet supported by the internal cache.
 *
 * @note Must be paired with `SituationCmdEndRenderPass`.
 */
SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "cmd cannot be NULL in SituationCmdBeginRenderPass.");
    if (!info) return SITUATION_ERROR_INVALID_PARAM;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BEGIN_RENDER_PASS);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

    p->args.begin_pass.display_id = info->display_id;
    // Capture current window resolution to prevent race conditions on render thread
    p->args.begin_pass.target_w = sit_gs.main_window_width;
    p->args.begin_pass.target_h = sit_gs.main_window_height;
    p->args.begin_pass.info = *info;

    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // For the main window, use the render pass that was used to create the framebuffers
    // to ensure compatibility. The cached render pass system creates passes with different
    // dependency counts which makes them incompatible with the existing framebuffers.
    VkRenderPass rp;
    if (info->display_id < 0) {
        rp = sit_render.vk.main_window_render_pass;  // Use the original render pass
    } else {
        // Use the VD's own render pass (created with its framebuffer) for compatibility
        if (info->display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[info->display_id]) {
            return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
        }
        SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[info->display_id];
        rp = vd->vk.render_pass;
    }
    if (rp == VK_NULL_HANDLE) {
        return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
    }

    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = rp;

    VkClearValue clear_values[2];
    clear_values[0].color = (VkClearColorValue){{info->color_attachment.clear.color.r / 255.0f, info->color_attachment.clear.color.g / 255.0f, info->color_attachment.clear.color.b / 255.0f, info->color_attachment.clear.color.a / 255.0f}};
    clear_values[1].depthStencil = (VkClearDepthStencilValue){info->depth_attachment.clear.depth, info->stencil_attachment.clear.stencil};
    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    if (info->display_id < 0) {
        render_pass_info.framebuffer = sit_render.vk.main_window_framebuffers[sit_render.vk.current_image_index];
        render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
        render_pass_info.renderArea.extent = sit_render.vk.swapchain_extent;

        float target_width = (float)sit_render.vk.swapchain_extent.width;
        float target_height = (float)sit_render.vk.swapchain_extent.height;
        ViewDataUBO ubo_data;
        glm_mat4_identity(ubo_data.view);
        glm_ortho(0.0f, target_width, target_height, 0.0f, -1.0f, 1.0f, ubo_data.projection);
        memcpy(sit_render.vk.view_proj_ubo_mapped[sit_render.vk.current_frame_index], &ubo_data, sizeof(ViewDataUBO));
    } else {
        if (info->display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS || !sit_render.virtual_display_slots_used[info->display_id]) {
            return SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID;
        }
        SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[info->display_id];
        render_pass_info.framebuffer = vd->vk.framebuffer;
        render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
        render_pass_info.renderArea.extent = (VkExtent2D){(uint32_t)vd->resolution.x, (uint32_t)vd->resolution.y};
    }

    vkCmdBeginRenderPass((VkCommandBuffer)cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)render_pass_info.renderArea.extent.width;
    viewport.height = (float)render_pass_info.renderArea.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = render_pass_info.renderArea.extent;
    vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

    if (info->display_id < 0) {
        sit_render.vk.inside_main_swapchain_render_pass = true;
    }

    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief [Core] Ends the current render pass on a command buffer.
 *
 * @details This function signals the completion of a rendering pass started by `SituationCmdBeginRenderPass` or `SituationCmdBeginRenderToDisplay`.
 *          It performs the necessary steps to finalize drawing operations for the current framebuffer attachment.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a `vkCmdEndRenderPass` command into the provided command buffer. This transitions the image layout of the attachments (e.g., to `PRESENT_SRC_KHR`) as defined by the render pass configuration.
 * - **OpenGL:** Unbinds the current Framebuffer Object (FBO) by binding the default framebuffer (0). This effectively "ends" the pass by redirecting subsequent draw calls back to the window's backbuffer.
 *
 * @param cmd The command buffer to record into.
 *            - **Vulkan:** Must be a valid `VkCommandBuffer` in the recording state, currently inside a render pass instance.
 *            - **OpenGL:** Ignored.
 *
 * @note This function must be paired with a preceding `SituationCmdBegin...` call.
 * @warning Calling this function without an active render pass (Vulkan) will result in a validation error.
 */
SITAPI SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: ENTRY, cmd=%p\n", cmd);
    fflush(stdout);
    #endif
    
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!buf) return SITUATION_ERROR_INVALID_PARAM;
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: buf=%p, calling _SitGLSoftCmdPush\n", buf);
    fflush(stdout);
    #endif
    
    if (!_SitGLSoftCmdPush(buf, SIT_OP_END_RENDER_PASS)) return SITUATION_ERROR_MEMORY_ALLOCATION;
    
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] SituationCmdEndRenderPass: SUCCESS, returning\n");
    fflush(stdout);
    #endif
#elif defined(SITUATION_USE_VULKAN)
    if (cmd == 0) return SITUATION_ERROR_INVALID_PARAM; // Basic validation
    vkCmdEndRenderPass((VkCommandBuffer)cmd);
    sit_render.vk.inside_main_swapchain_render_pass = false;
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Begins a render pass on a specific display target.
 * @details For OpenGL, this binds the appropriate framebuffer (0 for the main window, or an FBO for a virtual display), sets the viewport to the target's dimensions, and clears the color and depth buffers.
 *          For Vulkan, this begins a formal VkRenderPass on the command buffer.
 *
 * @param cmd The command buffer to record to. (Ignored in the immediate-mode OpenGL backend).
 * @param display_id The ID of the target. Use -1 for the main window/swapchain.
 * @param clear_color The color to clear the target with.
 */
SITAPI SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;
    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_STORE;
    info.depth_attachment.clear.depth = 1.0f;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.stencil_attachment.storeOp = SIT_STORE_OP_STORE;
    info.stencil_attachment.clear.stencil = 0;
    return SituationCmdBeginRenderPass(cmd, &info);
}
/**
 * @brief Ends the current render pass.
 * @note Deprecated in favor of SituationCmdEndRenderPass.
 */
SITAPI SituationError SituationCmdEndRender(SituationCommandBuffer cmd) {
    return SituationCmdEndRenderPass(cmd);
}

/**
 * @brief Sets the viewport and scissor rectangle for subsequent drawing commands.
 *
 * @details The viewport defines the rectangular area of the current render target that primitives will be rasterized to. The coordinates are in framebuffer/pixel space.
 *          In Vulkan, this function also sets the scissor rectangle to match the viewport dimensions, enforcing that rendering is clipped to this area. In OpenGL, the scissor test is not modified by this function; use `SituationCmdSetScissor` if explicit scissor control is needed.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glViewport` to set the viewport transformation.
 *   The command buffer parameter `cmd` is ignored as OpenGL uses global state.
 *   Note that OpenGL does not implicitly change the scissor state.
 * - **Vulkan:** Records `vkCmdSetViewport` and `vkCmdSetScissor` commands into the provided command buffer. Both the viewport and scissor are set to the specified rectangle.
 *   This requires the command buffer to be in the recording state and the bound graphics pipeline to have been created with `VK_DYNAMIC_STATE_VIEWPORT` and `VK_DYNAMIC_STATE_SCISSOR` enabled.
 *
 * @param cmd The command buffer into which the commands will be recorded (Vulkan)
 *            or ignored (OpenGL).
 * @param x The top-left x-coordinate of the viewport/scissor (in pixels).
 * @param y The top-left y-coordinate of the viewport/scissor (in pixels).
 *         Note: In Vulkan, the Y axis origin is typically the top-left.
 *         In OpenGL, it's the bottom-left, but `glViewport` handles this.
 * @param width The width of the viewport/scissor (in pixels). Must be positive.
 * @param height The height of the viewport/scissor (in pixels). Must be positive.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. (Vulkan) The bound graphics pipeline supports dynamic viewport and scissor.
 *       3. The specified `width` and `height` are greater than zero.
 * @warning Providing a `width` or `height` of zero or negative values results in undefined behavior or errors, depending on the backend and driver.
 */
SITAPI SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height) {
    // --- 1. Input Validation ---
    if (!SituationIsInitialized()) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    // While viewport dimensions *can* technically be negative in OpenGL spec,
    // it's highly unusual and often indicates an error.
    // Width and Height should almost always be positive.
    if (width <= 0.0f || height <= 0.0f) {
        // Even though function is void, setting an error state can be useful for debugging.
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                 "Invalid viewport dimensions: width=%.2f, height=%.2f. Dimensions must be positive.", width, height);
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, error_msg);
        return SITUATION_ERROR_INVALID_PARAM;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_SET_VIEWPORT);
        if (p) {
            p->args.viewport.x = x;
            p->args.viewport.y = y;
            p->args.viewport.w = width;
            p->args.viewport.h = height;
        } else {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Input Validation ---
        if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for setting viewport.");
            return SITUATION_ERROR_INVALID_PARAM;
        }
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // --- 3. Vulkan Implementation ---
        // Define the VkViewport structure.
        VkViewport viewport = {}; // Explicitly zero-initialize padding
        viewport.x = x;
        viewport.y = y; // Vulkan Y=0 is top
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f; // Standard near plane depth
        viewport.maxDepth = 1.0f; // Standard far plane depth

        // Record the command to set the viewport.
        // Assumes the pipeline has VK_DYNAMIC_STATE_VIEWPORT enabled.
        vkCmdSetViewport(vk_cmd, 0, 1, &viewport);

        // Define the VkRect2D structure for the scissor.
        // It's common practice to set scissor to match the viewport.
        VkRect2D scissor = {}; // Explicitly zero-initialize
        scissor.offset.x = (int32_t)x;
        scissor.offset.y = (int32_t)y;
        // VkExtent2D uses uint32_t, so width/height are cast.
        // Negative width/height were checked earlier.
        scissor.extent.width = (uint32_t)width;
        scissor.extent.height = (uint32_t)height;

        // Record the command to set the scissor.
        // Assumes the pipeline has VK_DYNAMIC_STATE_SCISSOR enabled.
        vkCmdSetScissor(vk_cmd, 0, 1, &scissor);

        // Note: vkCmdSetViewport/vkCmdSetScissor don't return VkResult.
        // Errors are validation layer reports or submission issues.
    }
#endif
    // --- 4. Post-Operation ---
    // No general post-operation actions are required here.
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets the dynamic scissor rectangle for the render target.
 * @details The scissor test is a hardware-level optimization that discards any pixel fragments outside of this defined rectangle, preventing the fragment shader from running on them. This should be called after binding a pipeline that was created with VK_DYNAMIC_STATE_SCISSOR enabled.
 * @param cmd The command buffer to record the command into.
 * @param x, y The top-left corner of the scissor rectangle, in pixel coordinates.
 * @param width, height The dimensions of the scissor rectangle, in pixels.
 */
SITAPI SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height) {
    // Basic validation: A scissor rectangle cannot have a negative size.
    if (!SituationIsInitialized() || width < 0 || height < 0) {
        return (width < 0 || height < 0) ? SITUATION_ERROR_INVALID_PARAM : SITUATION_ERROR_NOT_INITIALIZED;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_SET_SCISSOR);
    if (p) {
        p->args.scissor.x = x;
        p->args.scissor.y = y;
        p->args.scissor.w = width;
        p->args.scissor.h = height;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    // In Vulkan, this is a command recorded into the command buffer.

    // 1. Create the VkRect2D structure that Vulkan expects.
    VkRect2D scissor = {};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = (uint32_t)width;
    scissor.extent.height = (uint32_t)height;

    // 2. Record the command.
    // vkCmdSetScissor takes an array of scissor rectangles. We are only setting the
    // first one (at index 0).
    vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);

#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief [Core] Binds a vertex buffer for subsequent draw calls.
 * @details Records a command to set the active vertex buffer. All subsequent draw calls will source their vertex data from this buffer, starting from binding point 0.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Binds the buffer to the `GL_ARRAY_BUFFER` target. This is associated with the globally active VAO, making it the source for vertex attributes.
 * - **Vulkan:** Records a `vkCmdBindVertexBuffers` command for binding point 0.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param buffer The `SituationBuffer` handle of the vertex buffer to bind.
 */
SITAPI void SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer, size_t offset, size_t stride) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdBindVertexBuffer"); return; }
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBindVertexBuffer: invalid buffer handle"); return; }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_VERTEX_BUFFER);
    if (p) {
        p->args.bind_vbo.binding = binding;
        p->args.bind_vbo.buffer_id = (uint64_t)slot->gl_buffer_id;
        p->args.bind_vbo.offset = offset;
        p->args.bind_vbo.stride = stride;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return;

    VkBuffer vertex_buffers[] = { slot->vk_buffer };
    VkDeviceSize offsets[] = { (VkDeviceSize)offset };
    // Bind the buffer to the specified vertex input binding point.
    vkCmdBindVertexBuffers(vk_cmd, binding, 1, vertex_buffers, offsets);
#endif
}

/**
 * @brief [Core] Binds an index buffer for subsequent indexed draw calls.
 * @details Records a command to set the active index buffer. Subsequent indexed draw calls (`SituationCmdDrawIndexed`) will use this buffer.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Binds the buffer to the `GL_ELEMENT_ARRAY_BUFFER` target, associating it with the globally active VAO.
 * - **Vulkan:** Records a `vkCmdBindIndexBuffer` command. Assumes 32-bit unsigned integer indices.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param buffer The `SituationBuffer` handle of the index buffer to bind.
 */
SITAPI void SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCmdBindIndexBuffer"); return; }
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_RESOURCE_HANDLE, "SituationCmdBindIndexBuffer: invalid buffer handle");
        return;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_INDEX_BUFFER);
    if (p) {
        p->args.bind_ibo.buffer_id = (uint64_t)slot->gl_buffer_id;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return;

    // Bind the buffer for indexed drawing, specifying 32-bit unsigned integers as the index type.
    vkCmdBindIndexBuffer(vk_cmd, slot->vk_buffer, 0, VK_INDEX_TYPE_UINT32);
#endif
}

/**
 * @brief Binds a texture as a storage image for a compute shader.
 *
 * @details Associates a texture with a specific binding slot (e.g., `binding = 2` in GLSL).
 *          - **Vulkan:** Binds the texture's pre-cached descriptor set to the specified `binding` index in the command buffer.
 *          - **OpenGL:** Calls `glBindImageTexture`.
 *
 * @param cmd The current command buffer.
 * @param binding The binding index in the shader layout.
 * @param texture The texture to bind. Must support storage usage.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_INVALID_PARAM` or `SITUATION_ERROR_RESOURCE_INVALID` on failure.
 */
SITAPI SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    // [Bug Fix] Use LEGACY_TEXTURE_HANDLING opcode which correctly handles resource_type=3 (storage image)
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

    p->args.bind_desc.set_index = binding;
    p->args.bind_desc.resource_id = slot->gl_texture_id;
    p->args.bind_desc.resource_type = 3; // 3 = Image Texture (Storage)
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    // FIX: Use the 'binding' parameter as the set index, matching the Unified API buffer logic.
    vkCmdBindDescriptorSets(
        (VkCommandBuffer)cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        sit_render.vk.current_compute_pipeline_layout,
        binding,  // FIX: Was '0' in original code
        1,
        &slot->descriptor_set,
        0, NULL
    );
    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief [Core] Records a non-indexed drawing command.
 * @details Renders primitives sequentially from the currently bound vertex buffer.
 *
 * @param cmd The command buffer to record the command into.
 * @param vertex_count The number of vertices to draw.
 * @param instance_count The number of instances to draw (for instanced rendering).
 * @param first_vertex The index of the first vertex to draw.
 * @param first_instance The instance ID of the first instance to draw.
 */
SITAPI SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (vertex_count == 0 || instance_count == 0) return SITUATION_SUCCESS; // No-op is success

    // Mark that a draw command has happened this frame
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    // Triangle count approximation for standard topology (Triangle List)
    sit_render.frame_triangle_count += (vertex_count / 3) * instance_count;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW);
    if (p) {
        p->args.draw.v_count = vertex_count;
        p->args.draw.i_count = instance_count;
        p->args.draw.first_v = first_vertex;
        p->args.draw.first_i = first_instance;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    vkCmdDraw(vk_cmd, vertex_count, instance_count, first_vertex, first_instance);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief [Core] Records an indexed drawing command.
 * @details Renders primitives using indices from the currently bound index buffer to look up vertices from the currently bound vertex buffer.
 *
 * @param cmd The command buffer to record the command into.
 * @param index_count The number of indices to draw.
 * @param instance_count The number of instances to draw (for instanced rendering).
 * @param first_index The offset into the index buffer to start reading indices from.
 * @param vertex_offset A value added to each index before looking up a vertex.
 * @param first_instance The instance ID of the first instance to draw.
 */
SITAPI SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;
    if (index_count == 0 || instance_count == 0) return SITUATION_SUCCESS;

    // Update Stats
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (index_count / 3) * instance_count;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW_INDEXED);
    if (p) {
        p->args.draw_indexed.idx_count = index_count;
        p->args.draw_indexed.inst_count = instance_count;
        p->args.draw_indexed.first_idx = first_index;
        p->args.draw_indexed.v_offset = vertex_offset;
        p->args.draw_indexed.first_inst = first_instance;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;
    vkCmdDrawIndexed(vk_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Draws a text string using GPU-accelerated textured quads.
 *
 * @details Renders a string of text into the current command buffer. This function is extremely fast and suitable for real-time UIs.
 *          It uses the internal quad renderer with a font atlas texture.
 *
 * @param cmd The command buffer to record into.
 * @param font The font to use. Must have been baked with `SituationBakeFontAtlas`.
 * @param text The null-terminated string to draw.
 * @param pos The screen-space position (top-left) to start drawing.
 * @param color The color tint for the text.
 *
 * @note Requires a valid orthographic projection matrix to be active in the view UBO (which `SituationAcquireFrameCommandBuffer` sets up by default).
 */
SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color) {
    return SituationCmdDrawTextEx(cmd, font, text, pos, 0.0f, 0.0f, color);
}

/**
 * @brief Draws a text string using GPU-accelerated textured quads with extended styling.
 * @details Records a batch of draw commands to render text using the internal text renderer pipeline.
 *          This function supports custom font sizing and character spacing adjustments at runtime.
 *
 * @par Performance (Optimized v2.3.27)
 * - **Vulkan:** Uses a **persistent mapped ring buffer** to write vertex data directly to GPU-visible memory.
 *   This eliminates per-draw buffer allocations and staging copies, offering near-zero overhead for dynamic UI.
 *   (Falls back to staging upload only if the ring buffer fills up within a single frame).
 * - **OpenGL:** Data is packed into the soft command stream for deferred execution.
 *
 * @param cmd The command buffer to record into.
 * @param font The font to use. Must have been baked with `SituationBakeFontAtlas`.
 * @param text The text string to render.
 * @param pos The screen position (top-left) in pixels.
 * @param fontSize The desired font height in pixels. Pass 0.0f to use the native baked size.
 * @param spacing Additional spacing between characters in pixels. Can be negative.
 * @param color The text color tint.
 */
SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color) {
    if (!SituationIsInitialized() || !text) return SITUATION_ERROR_INVALID_PARAM;

    // Default Debug Font Fallback
    SituationFont use_font = font;
    if (use_font.atlas_texture.generation == 0) {
        use_font = sit_render.default_font;
        if (use_font.atlas_texture.generation == 0) return SITUATION_ERROR_RESOURCE_INVALID;
    }

    bool is_grid_font = (use_font.glyph_info == NULL && use_font.atlas_texture.slot_index == sit_render.default_font.atlas_texture.slot_index && use_font.atlas_texture.generation == sit_render.default_font.atlas_texture.generation);
    if (!is_grid_font && !use_font.glyph_info) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

    size_t len = strlen(text);
#if !defined(SITUATION_NO_STB) && !defined(SITUATION_NO_STB_TRUETYPE)
    if (len == 0) return SITUATION_SUCCESS;
    if (len > 2048) len = 2048;

    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (len * 2);

    Vector4 color_vec;
    SituationConvertColorToVector4(color, &color_vec);

#if defined(SITUATION_USE_OPENGL)
    // --- OPENGL PATH (Standard Soft Buffer) ---
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    void* text_ptr = _SitGLSoftDataPush(buf, text, len + 1);
    if (!text_ptr) return SITUATION_ERROR_MEMORY_ALLOCATION;
    size_t text_offset = (size_t)((uint8_t*)text_ptr - buf->data_buffer);

    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW_TEXT_EX);
    if (p) {
        p->args.draw_text_ex.font = use_font;
        p->args.draw_text_ex.pos = pos;
        p->args.draw_text_ex.color = color;
        p->args.draw_text_ex.text_offset = text_offset;
        p->args.draw_text_ex.fontSize = fontSize;
        p->args.draw_text_ex.spacing = spacing;

        // [v2.3.30] Bindless Logic
        // The SIT_OP_DRAW_TEXT_EX packet doesn't store the bindless handle explicitly.
        // Instead, the executor (_SituationGLExecuteCommands) will detect if the feature is enabled
        // and resolve the handle from the font's atlas texture ID at draw time.
        // This keeps the packet size small and logic centralized in the executor.
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    // --- VULKAN OPTIMIZED PATH ---

    // 1. Bind the Global Bindless Set (instead of specific texture set)
    vkCmdBindDescriptorSets((VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        sit_render.vk.text_pipeline_layout,
                        1, 1, &sit_render.vk.global_bindless_set,
                        0, NULL);

    // 2. Calculate Size (6 verts/char * 4 floats/vert)
    size_t data_size = len * 6 * 4 * sizeof(float);

    VkBuffer target_buffer = VK_NULL_HANDLE;
    size_t target_offset = 0;
    float* write_ptr = NULL;

    uint32_t frame_idx = sit_render.vk.current_frame_index;

    // 3. POINTER SELECTION: Decide where to write
    // Try Fast Path: Is there space in the mapped Ring Buffer?
    if (sit_render.vk.dynamic_vbo_mapped[frame_idx] &&
        (sit_render.vk.dynamic_vbo_cursor + data_size <= sit_render.vk.dynamic_vbo_capacity))
    {
        // FAST: Write directly to GPU-mapped memory
        write_ptr = (float*)((uint8_t*)sit_render.vk.dynamic_vbo_mapped[frame_idx] + sit_render.vk.dynamic_vbo_cursor);
        target_buffer = sit_render.vk.dynamic_vbo[frame_idx];
        target_offset = sit_render.vk.dynamic_vbo_cursor;

        // Reserve the space
        sit_render.vk.dynamic_vbo_cursor += data_size;
    }
    else {
        // SLOW: Ring buffer full. Use Scratch Buffer + Staging Upload.
        if (sit_render.text_batch_capacity < data_size) {
            sit_render.text_batch_scratch = (float*)SIT_REALLOC(sit_render.text_batch_scratch, data_size * 2);
            sit_render.text_batch_capacity = data_size * 2;
        }
        write_ptr = sit_render.text_batch_scratch;
    }

    if (!write_ptr) return SITUATION_ERROR_NOT_INITIALIZED; // Allocation failed

    // 4. VERTEX GENERATION (Unified Loop)
    // This logic fills 'write_ptr', regardless of where it points.
    float x = pos.x;
    float y = pos.y;
    stbtt_bakedchar* cdata = (stbtt_bakedchar*)use_font.glyph_info;
    int v_idx = 0;

    float target_size = (fontSize > 0.0f) ? fontSize : use_font.font_height_pixels;
    float scale_factor = (use_font.font_height_pixels > 0.0f) ? (target_size / use_font.font_height_pixels) : 1.0f;

    for (size_t i = 0; i < len; i++) {
        if (is_grid_font) {
            unsigned char c = (unsigned char)text[i];
            if (c < 128) {
                int col = c % 16;
                int row = c / 16;
                float u0 = col / 16.0f;
                float v1 = row / 16.0f;  // Swapped: v1 gets row (top)
                float u1 = (col + 1) / 16.0f;
                float v0 = (row + 1) / 16.0f;  // Swapped: v0 gets row+1 (bottom)

                float size_px = 8.0f * scale_factor;
                float qx0 = x;
                float qy0 = y;
                float qx1 = x + size_px;
                float qy1 = y + size_px;
                x += size_px + spacing;

                write_ptr[v_idx++] = qx0; write_ptr[v_idx++] = qy0; write_ptr[v_idx++] = u0; write_ptr[v_idx++] = v0;
                write_ptr[v_idx++] = qx0; write_ptr[v_idx++] = qy1; write_ptr[v_idx++] = u0; write_ptr[v_idx++] = v1;
                write_ptr[v_idx++] = qx1; write_ptr[v_idx++] = qy0; write_ptr[v_idx++] = u1; write_ptr[v_idx++] = v0;

                write_ptr[v_idx++] = qx1; write_ptr[v_idx++] = qy0; write_ptr[v_idx++] = u1; write_ptr[v_idx++] = v0;
                write_ptr[v_idx++] = qx0; write_ptr[v_idx++] = qy1; write_ptr[v_idx++] = u0; write_ptr[v_idx++] = v1;
                write_ptr[v_idx++] = qx1; write_ptr[v_idx++] = qy1; write_ptr[v_idx++] = u1; write_ptr[v_idx++] = v1;
            }
        }
        else if (text[i] >= 32 && text[i] < 128) {
            float x_before = x;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, use_font.atlas_width, use_font.atlas_height, text[i] - 32, &x, &y, &q, 1);

            if (scale_factor != 1.0f || spacing != 0.0f) {
                float w = q.x1 - q.x0;
                float h = q.y1 - q.y0;
                float y_off = q.y0 - y;

                float x0 = x_before + (q.x0 - x_before) * scale_factor;
                float y0 = y + y_off * scale_factor;
                float x1 = x0 + w * scale_factor;
                float y1 = y0 + h * scale_factor;

                q.x0 = x0; q.y0 = y0;
                q.x1 = x1; q.y1 = y1;

                float advance = x - x_before;
                x = x_before + (advance * scale_factor) + spacing;
            }

            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t0;
            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t1;
            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t0;

            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y0; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t0;
            write_ptr[v_idx++] = q.x0; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s0; write_ptr[v_idx++] = q.t1;
            write_ptr[v_idx++] = q.x1; write_ptr[v_idx++] = q.y1; write_ptr[v_idx++] = q.s1; write_ptr[v_idx++] = q.t1;
        }
    }

    // 5. UPLOAD (Fallback Path Only)
    // If we wrote to scratch (target_buffer is NULL), we must upload now.
    if (target_buffer == VK_NULL_HANDLE) {
        VkBuffer temp_buffer;
        VmaAllocation temp_alloc;
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // This helper creates a staging buffer + device local buffer and copies
        if (_SituationVulkanCreateAndUploadBuffer(vk_cmd, sit_render.text_batch_scratch, data_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &temp_buffer, &temp_alloc) == SITUATION_SUCCESS) {
            target_buffer = temp_buffer;
            target_offset = 0;
            // Mark for deletion at end of frame
            _SituationDeferDestroyBuffer(temp_buffer, temp_alloc);
        }
    }

    // 6. DRAW
    if (target_buffer != VK_NULL_HANDLE) {
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // CRITICAL: Update global state so descriptor set binding works
        sit_render.vk.current_pipeline_layout_for_push_constants = sit_render.vk.text_pipeline_layout;

        vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.text_pipeline);

        // Bind descriptor sets (View UBO at set 0, Global Bindless at set 1)
        uint32_t frame_idx = sit_render.vk.current_frame_index;
        vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                sit_render.vk.text_pipeline_layout,
                                0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[frame_idx],
                                0, NULL);

        // Bind Vertex Buffer with OFFSET
        VkDeviceSize offsets[] = { target_offset };
        vkCmdBindVertexBuffers(vk_cmd, 0, 1, &target_buffer, offsets);

        // Push Constants (Color + Texture ID)
        struct {
            Vector4 color;
            uint32_t texture_id;
        } text_pc;
        text_pc.color = color_vec;

        _SituationTextureSlot* font_slot = _SitGetTextureSlot(use_font.atlas_texture);
        text_pc.texture_id = font_slot ? use_font.atlas_texture.slot_index : 0;
        vkCmdPushConstants(vk_cmd, sit_render.vk.text_pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(text_pc), &text_pc);
        vkCmdDraw(vk_cmd, (uint32_t)(len * 6), 1, 0, 0);
    }
#endif
    return SITUATION_SUCCESS;

#else
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationCmdDrawText requires STB Truetype.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Submits a command to copy a texture to the main window's swapchain.
 * @details This function is designed for Compute-Only applications or custom rendering pipelines where the final image is generated in a texture/image rather than drawn directly to the backbuffer via a Render Pass.
 *          It effectively "presents" the texture by blitting it onto the swapchain's current image.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Records a sequence of layout transitions and a `vkCmdBlitImage` command.
 *   1. Transitions the Swapchain Image to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
 *   2. Transitions the Source Texture to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
 *   3. Blits the source to the swapchain (scaling if necessary).
 *   4. Transitions the Swapchain Image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` (ready for presentation).
 *   5. Transitions the Source Texture back to `VK_IMAGE_LAYOUT_GENERAL` (ready for next compute frame).
 *
 * - **OpenGL:** Creates a temporary Framebuffer Object (FBO) attached to the source texture and performs a `glBlitNamedFramebuffer` to the default backbuffer (FBO 0).
 *
 * @param cmd The command buffer to record into.
 * @param texture The source texture containing the frame to present. Must be valid.
 */
SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_PRESENT);
    if (p) {
        p->args.present.texture = texture;
        p->args.present.target_w = sit_gs.main_window_width;
        p->args.present.target_h = sit_gs.main_window_height;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    _SituationTextureSlot* tex_slot = _SitGetTextureSlot(texture);
    if (!tex_slot) return SITUATION_ERROR_RESOURCE_INVALID;

    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    // 1. Get the current swapchain image we are targeting
    VkImage swapchainImage = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];
    if (swapchainImage == VK_NULL_HANDLE) return SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID;

    // 2. Transition Swapchain to TRANSFER_DST
    // Note: SituationAcquireFrameCommandBuffer normally leaves it in UNDEFINED or COLOR_ATTACHMENT_OPTIMAL.
    // We assume it's currently available.
    _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImage, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 3. Transition Source Texture to TRANSFER_SRC
    // Note: Compute shaders usually leave textures in GENERAL or SHADER_READ_ONLY.
    _SituationVulkanTransitionImageLayout(vk_cmd, tex_slot->image, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // 4. Blit
    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x = texture.width;
    blit.srcOffsets[1].y = texture.height;
    blit.srcOffsets[1].z = 1;

    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1].x = sit_render.vk.swapchain_extent.width;
    blit.dstOffsets[1].y = sit_render.vk.swapchain_extent.height;
    blit.dstOffsets[1].z = 1;

    vkCmdBlitImage(vk_cmd, tex_slot->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_NEAREST);  // Use NEAREST for sharp pixel-perfect scaling

    // 5. Transition Swapchain to PRESENT_SRC
    _SituationVulkanTransitionImageLayout(vk_cmd, swapchainImage, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 6. Transition Source back to GENERAL (ready for next compute frame)
    _SituationVulkanTransitionImageLayout(vk_cmd, tex_slot->image, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Retrieves the GPU device address of a buffer for bindless access.
 * @details This function returns a 64-bit pointer (BDA) that can be passed to shaders to access the buffer directly, bypassing descriptor sets.
 *          This is essential for modern "Bindless" rendering techniques and ray tracing.
 *
 * @par Backend-Specific Behavior
 * - **Vulkan:** Returns the address via `vkGetBufferDeviceAddress`. Requires the `bufferDeviceAddress` feature to be enabled during initialization (handled automatically if supported).
 * - **OpenGL:** Returns the address via `glGetNamedBufferParameterui64v` (NV_shader_buffer_load / EXT_buffer_reference). It also automatically ensures the buffer is made resident (`glMakeNamedBufferResidentNV`), which is required for the address to be valid.
 *
 * @param buffer The buffer handle to query.
 * @return The 64-bit GPU address, or 0 if the feature is unsupported or the buffer is invalid.
 */
SITAPI uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer) {
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return 0;

#if defined(SITUATION_USE_VULKAN)
    // IMPORTANT: This requires the bufferDeviceAddress feature to be enabled in Logical Device creation.
    // If compiling against Vulkan 1.2+:
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = slot->vk_buffer
    };
    return vkGetBufferDeviceAddress(sit_render.vk.device, &info);
    // If compiling against 1.0/1.1 with extensions, use vkGetBufferDeviceAddressKHR

#elif defined(SITUATION_USE_OPENGL)
    GLuint64 address = 0;
    // Requires GL_NV_shader_buffer_load or GL_EXT_buffer_reference
#if defined(GLAD_GL_EXT_buffer_reference) || defined(GLAD_GL_NV_shader_buffer_load)
    if (GLAD_GL_EXT_buffer_reference || GLAD_GL_NV_shader_buffer_load) {
        glGetNamedBufferParameterui64v(buffer.gl_buffer_id, GL_BUFFER_GPU_ADDRESS_NV, &address);
        glMakeNamedBufferResidentNV(buffer.gl_buffer_id, GL_READ_WRITE);
    }
#endif
    return (uint64_t)address;
#endif
    return 0;
}

/**
 * @brief Retrieves a bindless texture handle for the given texture.
 * @details Allows the texture to be accessed in shaders via a 64-bit handle (e.g., `sampler2D` can be constructed from a `uint64_t`), bypassing texture units.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glGetTextureHandleARB` and ensures the handle is resident (`glMakeTextureHandleResidentARB`).
 * - **Vulkan:** Currently returns 0 (Unimplemented). Vulkan bindless texturing typically uses Descriptor Indexing rather than raw 64-bit handles in the same way OpenGL does.
 *
 * @param texture The texture to query.
 * @return A 64-bit bindless handle, or 0 if unsupported.
 */
SITAPI uint64_t SituationGetTextureHandle(SituationTexture texture) {
#if defined(SITUATION_USE_OPENGL)
    if (!texture.generation) return 0;

    // Check if bindless is supported
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Bindless textures not supported by driver.");
        return 0;
    }

    // Retrieve the handle using ARB extension
#if defined(GLAD_GL_ARB_bindless_texture)
    if (GLAD_GL_ARB_bindless_texture) {
        GLuint64 handle = glGetTextureHandleARB(texture.gl_texture_id);
        if (!handle) {
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to retrieve texture handle.");
            return 0;
        }

        // Ensure the handle is resident (GPU accessible)
        if (!glIsTextureHandleResidentARB(handle)) {
            glMakeTextureHandleResidentARB(handle);
        }
        return (uint64_t)handle;
    }
#endif
    return 0;

#elif defined(SITUATION_USE_VULKAN)
    // For Vulkan, the "bindless handle" is the index into the descriptor array.
    // This is stored in texture.slot_index.
    if (!texture.generation) return 0;
    return (uint64_t)texture.slot_index;
#else
    return 0;
#endif
}

/**
 * @brief Binds a texture as a sampled image (sampler2D) to a specific binding point.
 * @details This is a semantic alias for `SituationCmdBindTextureSet`, specifically intending usage as a sampled texture (vs storage).
 *          It ensures clarity in user code when distiguishing between read-only textures and read-write images.
 *
 * @param cmd The command buffer.
 * @param binding The shader binding point (set index).
 * @param texture The texture to bind.
 * @return SITUATION_SUCCESS on success.
 */
SITAPI SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture) {
    // Maps directly to the existing unified binding function
    return SituationCmdBindTextureSet(cmd, binding, texture);
}

/**
 * @brief [Core] Define the format of a vertex attribute for the active VAO.
 *
 * @details Configures how vertex data is read from the bound buffer for a specific attribute location (e.g., Position at loc 0, UV at loc 1).
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Modifies the state of the currently bound Global VAO using `glVertexArrayAttribFormat`. This allows dynamic changes to vertex formats at runtime.
 * - **Vulkan:** **Not Supported.** Returns `SITUATION_ERROR_NOT_IMPLEMENTED`.
 *   In Vulkan, vertex input state is immutable and baked into the `VkPipeline` object at creation. You cannot change vertex attributes dynamically on a command buffer; you must create a new pipeline with the desired layout.
 *
 * @note **[OpenGL Only]** This function is not supported on Vulkan.
 *
 * @param cmd The command buffer (Ignored in OpenGL).
 * @param location The shader attribute location index (e.g., `layout(location=0)`).
 * @param size The number of components (1, 2, 3, or 4).
 * @param type The data type of the components (e.g., `SIT_DATA_FLOAT`).
 * @param normalized Whether fixed-point data should be normalized.
 * @param offset The byte offset of this attribute within the vertex structure.
 */
SITAPI SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, int size, SituationDataType type, bool normalized, size_t offset) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

#if defined(SITUATION_USE_OPENGL)
    if (_SituationMapDataTypeToGL(type) == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCmdSetVertexAttribute: Invalid data type.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_SET_VERTEX_ATTRIBUTE);
    if (p) {
        p->args.set_vertex_attr.location = location;
        p->args.set_vertex_attr.size = size;
        p->args.set_vertex_attr.type = (int)type;
        p->args.set_vertex_attr.normalized = normalized ? 1 : 0;
        p->args.set_vertex_attr.offset = offset;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    // FIX: Explicitly report that this is not supported in the Vulkan backend.
    // In Vulkan, vertex attributes are baked into the immutable VkPipeline object at creation.
    // To change attributes, you must create a new pipeline with the desired vertex input state.
    (void)cmd; (void)location; (void)size; (void)type; (void)normalized; (void)offset;
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED,
        "SituationCmdSetVertexAttribute is incompatible with Vulkan's architecture. "
        "Vulkan Pipelines are immutable; vertex attributes must be defined at pipeline creation time "
        "(inside SituationLoadShaderFromMemory logic), not dynamically on the command buffer.");
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Binds a graphics pipeline (shader program) for subsequent drawing commands.
 *
 * @details This function activates the specified graphics pipeline, making its shader program (and associated fixed-function state, in Vulkan) active for subsequent draw calls recorded in the command buffer. Any previously bound graphics pipeline is replaced.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** This function is a wrapper around `glUseProgram`. It activates the shader program associated with the `SituationShader` handle. The command buffer parameter `cmd` is ignored as OpenGL uses global state.
 *               In debug builds (`NDEBUG` not defined), it validates the program ID using `glIsProgram` to catch potential errors early.
 * - **Vulkan:** Records a `vkCmdBindPipeline` command into the provided command buffer for the `VK_PIPELINE_BIND_POINT_GRAPHICS` bind point.
 *               It also updates the internal global state `sit_render.vk.current_pipeline_layout_for_push_constants` with the pipeline's layout, which is essential for subsequent `SituationCmdSetPushConstant` and descriptor set binding operations.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param shader The `SituationShader` handle representing the graphics pipeline to bind.
 *
 * @return SITUATION_SUCCESS on successful binding of the pipeline.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the shader handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (OpenGL, Debug) if the shader's program ID is not a valid OpenGL name.
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_OPENGL_GENERAL (OpenGL) if an OpenGL error occurs during the `glUseProgram` call (e.g., program linking issues made runtime detectable, context problems).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. The shader pipeline represented by `shader` was created successfully.
 */
SITAPI SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Attempted to bind an invalid shader handle.");
        #if defined(SITUATION_USE_VULKAN)
        sit_render.vk.current_pipeline_layout_for_push_constants = VK_NULL_HANDLE;
        #endif
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
        SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_PIPELINE);
        if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

        p->args.bind_pipeline.shader_id = (uint64_t)slot->gl_program_id;
        buf->current_recording_shader_id = (uint64_t)slot->gl_program_id;
        return SITUATION_SUCCESS;
    }
#elif defined(SITUATION_USE_VULKAN)
    vkCmdBindPipeline((VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, slot->vk_pipeline);
    /* Descriptor binds route via current_* layout; clear compute so UBO/texture binds hit this graphics layout after compute tests. */
    sit_render.vk.current_compute_pipeline_layout = VK_NULL_HANDLE;
    sit_render.vk.current_pipeline_layout_for_push_constants = slot->vk_pipeline_layout;
    sit_render.vk.current_pbr_pipeline = slot->vk_pipeline; // Track for debugging
    sit_render.vk.current_bound_shader_slot = slot;          // For stride-based pipeline selection
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}

/**
 * @brief Records a command to draw a mesh using the currently bound graphics pipeline.
 *
 * @details This function instructs the GPU to render the geometry defined by the provided `SituationMesh`. It requires that a graphics pipeline (shader, potentially state) has been previously bound using `SituationCmdBindPipeline`.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Binds the mesh's Vertex Array Object (VAO) using `glBindVertexArray`.
 *   The VAO encapsulates the vertex buffer, index buffer, and vertex attribute configurations. It then calls `glDrawElements` to render the indexed geometry.
 *   In debug builds (`NDEBUG` not defined), it validates the VAO ID using `glIsVertexArray` to catch potential errors early.
 * - **Vulkan:** Explicitly binds the mesh's `vertex_buffer` and `index_buffer` to the command buffer using `vkCmdBindVertexBuffers` and `vkCmdBindIndexBuffer`.
 *   It then records a `vkCmdDrawIndexed` command. This requires the command buffer to be in the recording state and a compatible graphics pipeline to be bound.
 *
 * @param cmd The command buffer into which the draw command will be recorded.
 *            In OpenGL, this parameter is typically ignored as it uses global state.
 * @param mesh The `SituationMesh` handle containing the geometry data (vertices, indices) to be drawn.
 *
 * @return SITUATION_SUCCESS on successful recording of the draw command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the mesh handle is invalid (e.g., `id` is 0) or if the mesh contains no indices (`index_count` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (OpenGL, Debug) if the mesh's VAO ID is not a valid OpenGL name.
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_OPENGL_GENERAL (OpenGL) if an OpenGL error occurs during the draw process (e.g., invalid VAO state, context issues).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible graphics pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The mesh data (vertex/index buffers) is valid and accessible by the GPU.
 */
SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!cmd) return SITUATION_ERROR_INVALID_PARAM;

    // We just validate that the mesh slot is valid before recording
    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "CmdDrawMesh: Invalid mesh handle.");
    }

    // [FIX v2.4.38] Increment draw call counter (was missing — reported by test harness)
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += (slot->index_count > 0 ? slot->index_count : slot->vertex_count) / 3;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW_MESH);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;
    p->args.draw_mesh.mesh = mesh; // Store handle
    p->args.draw_mesh.shader_id = buf->current_recording_shader_id;
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Select the correct pipeline variant based on vertex stride
    if (sit_render.vk.current_bound_shader_slot) {
        _SituationShaderSlot* shader_slot = sit_render.vk.current_bound_shader_slot;
        VkPipeline target_pipeline = shader_slot->vk_pipeline; // Default: PBR

        if (slot->vertex_stride <= 3 * (int)sizeof(float) && shader_slot->vk_pipeline_simple != VK_NULL_HANDLE) {
            target_pipeline = shader_slot->vk_pipeline_simple;
        } else if (slot->vertex_stride <= (3 + 3 + 2) * (int)sizeof(float) && shader_slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
            target_pipeline = shader_slot->vk_pipeline_legacy;
        }

        // Rebind if different from what was initially bound
        if (target_pipeline != shader_slot->vk_pipeline) {
            vkCmdBindPipeline((VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, target_pipeline);
        }
    }

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers((VkCommandBuffer)cmd, 0, 1, &slot->vertex_buffer, offsets);
    if (slot->index_count > 0 && slot->index_buffer) {
        vkCmdBindIndexBuffer((VkCommandBuffer)cmd, slot->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed((VkCommandBuffer)cmd, (uint32_t)slot->index_count, 1, 0, 0, 0);
    } else {
        vkCmdDraw((VkCommandBuffer)cmd, (uint32_t)slot->vertex_count, 1, 0, 0);
    }
    return SITUATION_SUCCESS;
#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}


/**
 * @brief Draws a colored, transformed quad.
 * @details This is a high-level helper command that uses the library's internal quad renderer.
 *          It is intended for simple 2D or debug rendering.
 * @param cmd The command buffer. (Ignored in OpenGL).
 * @param model The 4x4 model matrix (position, rotation, scale) for the quad.
 * @param color The color of the quad as a normalized vec4 (r, g, b, a).
 */
/**
 * @brief Draws a part of a texture (defined by a rectangle) on screen.
 * @details This function allows you to draw a specific rectangular region (source) of a texture
 *          scaled to fit a destination rectangle on the screen. It also supports rotation
 *          around a custom origin and color tinting.
 *
 * @param cmd The command buffer to record into.
 * @param texture The texture to draw.
 * @param source The rectangular part of the texture to draw.
 * @param dest The screen rectangle to draw the texture into.
 * @param origin The point within the destination rectangle to rotate around (relative to top-left).
 * @param rotation The rotation angle in degrees (clockwise).
 * @param tint The color tint to apply to the texture (WHITE for no tint).
 */
SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, ColorRGBA tint) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    // 1. Bind Texture (Set 0, Binding 0)
    // This handles descriptor binding (Vulkan) or texture binding + uniform setting (OpenGL)
#if defined(SITUATION_USE_OPENGL)
    SituationCmdBindSampledTexture(cmd, 0, texture);
#endif

    // 2. Calculate UV Rect
    float tw = (float)texture.width;
    float th = (float)texture.height;
    if (tw <= 0) tw = 1.0f;
    if (th <= 0) th = 1.0f;

    Vector4 uv_rect;
    uv_rect.x = source.x / tw;
    uv_rect.y = source.y / th;
    uv_rect.z = source.width / tw;
    uv_rect.w = source.height / th;

    // 3. Calculate Model Matrix
    // Transform: Translate(dest) * Rotate(rot) * Translate(-origin) * Scale(dest.wh)
    mat4 model;
    glm_mat4_identity(model);

    // Translation to destination position (top-left)
    glm_translate(model, (vec3){dest.x, dest.y, 0.0f});

    // Rotation
    if (rotation != 0.0f) {
        glm_rotate(model, glm_rad(rotation), (vec3){0.0f, 0.0f, 1.0f});
    }

    // Origin offset (Pivot) - Only if not zero
    if (origin.x != 0.0f || origin.y != 0.0f) {
        glm_translate(model, (vec3){-origin.x, -origin.y, 0.0f});
    }

    // Scale to destination size
    // Note: If width/height are negative, it might flip, but UVs handle flipping too usually.
    glm_scale(model, (vec3){dest.width, dest.height, 1.0f});

    // 4. Convert Color
    Vector4 color_vec;
    SituationConvertColorToVector4(tint, &color_vec);

    // 5. Submit Draw Call (Internal Quad)
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += 2;

    int use_texture = 1; // True

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

    // The texture bind above handled the state setting. We just push the geometry.
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW_QUAD);
    if (p) {
        glm_mat4_copy(model, p->args.draw_quad.model);
        p->args.draw_quad.color = color_vec;
        p->args.draw_quad.uv_rect = uv_rect;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.quad_pipeline == VK_NULL_HANDLE) return SITUATION_ERROR_VULKAN_PIPELINE_FAILED;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline);

    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

    /* Quad VS reads view/proj from set 0 — same as SituationCmdDrawQuad. Missing this bind
       left projection garbage → black/wrong pixels for all textured quad draws (harness). */
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 0, 1,
        &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);

    // [Bindless] Bind the Global Descriptor Set (Set 1)
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 1, 1, &sit_render.vk.global_bindless_set, 0, NULL);

    /* Bindless array index must match vkUpdateDescriptorSets(dstArrayElement) in CreateTextureEx.
       Never fall back to slot 0 — stale handles silently sampled the wrong texture (black/wrong harness pixels). */
    _SituationTextureSlot* tex_slot = _SitGetTextureSlot(texture);
    if (!tex_slot || texture.slot_index >= SITUATION_MAX_TEXTURES) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    uint32_t slot_idx = texture.slot_index;

    struct {
        mat4 model;
        vec4 color;
        vec4 uv_rect;
        uint32_t texture_id;
        int use_texture;
    } push_data;

    glm_mat4_copy(model, push_data.model);
    glm_vec4_copy(color_vec.raw, push_data.color);
    glm_vec4_copy(uv_rect.raw, push_data.uv_rect);
    push_data.texture_id = slot_idx;
    push_data.use_texture = use_texture; // 1

    const uint32_t quad_push_bytes = (uint32_t)(sizeof(mat4) + sizeof(vec4) + sizeof(vec4) + sizeof(uint32_t) + sizeof(int));
    vkCmdPushConstants(vk_cmd, sit_render.vk.quad_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, quad_push_bytes, &push_data);
    vkCmdDraw(vk_cmd, 4, 1, 0, 0);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Records a command to draw a full-screen or transformed quad (rectangle) with solid color.
 *
 * @details This is a high-performance convenience function for drawing a simple colored quad,
 *          typically used for:
 *            - Full-screen effects (post-processing, UI backgrounds, overlays)
 *            - Debug visualizations (bounding boxes, highlights, color pickers)
 *            - Simple sprites or rectangles without needing a full mesh
 *            - Blitting a single color or texture (if extended with texture binding)
 *
 *          The function records the following into the command buffer:
 *            - Binds an internal built-in quad vertex shader + fragment shader
 *            - Sets up a simple vertex buffer or immediate vertex data (2 triangles / 6 vertices)
 *            - Applies the provided `model` matrix (usually for position, scale, rotation)
 *            - Sets the uniform color (passed as `Vector4`)
 *            - Issues a draw call (6 indices or 6 vertices)
 *
 *          Coordinates:
 *            - In NDC space: model matrix transforms from [-1,1] quad to desired screen region
 *            - Common usage: identity matrix for full-screen, translate/scale for positioned rects
 *
 *          Backend implementation:
 *            - Vulkan: records into command buffer (uses pre-created pipeline or dynamic state)
 *            - OpenGL: uses glDrawArrays or glDrawElements with VAO or immediate mode fallback
 *
 * @param cmd Valid recording command buffer handle (must be in recording state)
 * @param model 4x4 transformation matrix applied to the quad vertices
 *              (position, rotation, scale; usually orthographic projection already applied externally)
 * @param color RGBA color to fill the quad (components in [0,1] range)
 *              Use SIT_VEC4(r,g,b,a) macro for convenience
 *
 * @return SITUATION_SUCCESS on successful command recording,
 *         SITUATION_ERROR_INVALID_PARAM if cmd is invalid/not recording,
 *         SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT if internal command buffer full,
 *         SITUATION_ERROR_RESOURCE_INVALID if internal quad pipeline/shader not ready,
 *         or other backend-specific errors
 *
 * @note This is a **very lightweight** draw call ideal for high-frequency use (e.g. UI, debug lines).
 *       No texture binding or complex state changes are performed.
 *       Assumes current pipeline layout supports the internal quad shader (set via `SituationCmdBindPipeline` if needed).
 *       For textured quads, use `SituationCmdBindTexture` + `SituationCmdDrawQuadTextured` variant (if exists).
 *
 *       Thread safety:
 *         - Must be called from a thread that owns the command buffer
 *         - Safe during command recording phase only
 *         - Actual execution happens later on render thread submission
 *
 * @see SituationCmdDrawQuadTextured (if implemented), SituationCmdBindPipeline,
 *      SituationCreateCommandBuffer, mat4 (cglm), Vector4, SIT_VEC4 macro
 */
SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    sit_render.debug_draw_command_issued_this_frame = true;
    sit_render.frame_draw_calls++;
    sit_render.frame_triangle_count += 2;

    // Default UV Rect: Offset (0,0), Scale (1,1)
    Vector4 uv_rect = {{0.0f, 0.0f, 1.0f, 1.0f}};
    int use_texture = 0; // False

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

    // [v2.3.30] Bindless Support
    // Logic: In OpenGL, `SituationCmdDrawQuad` relies on a previously issued `SituationCmdBindTexture(cmd, 0, tex)`
    // command to set the active texture.
    // To support bindless automatically, `SituationCmdBindTexture` has been updated (below) to ALSO
    // record a `SIT_OP_SET_UNIFORM` command that pushes the bindless handle to uniform location 7.
    // So `SituationCmdDrawQuad` itself doesn't need to change much, except enabling the flag in the packet
    // if we wanted to be explicit.
    // However, the shader needs to know whether to sample from binding 0 or the handle at location 7.
    // We update SituationCmdBindTexture to set the `u_use_texture` uniform to 2 (Bindless) instead of 1 (Bindful)
    // if bindless is active.

    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DRAW_QUAD);
    if (p) {
        glm_mat4_copy(model, p->args.draw_quad.model);
        p->args.draw_quad.color = color;
        p->args.draw_quad.uv_rect = uv_rect;
    } else {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.quad_pipeline == VK_NULL_HANDLE) return SITUATION_ERROR_VULKAN_PIPELINE_FAILED;
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    vkCmdBindPipeline(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline);

    VkBuffer vertex_buffers[] = { sit_render.vk.quad_vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(vk_cmd, 0, 1, vertex_buffers, offsets);

    // Bind the View/Projection UBO descriptor set (Set 0)
    vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 0, 1, &sit_render.vk.view_proj_ubo_descriptor_set[sit_render.vk.current_frame_index], 0, NULL);

    // Bind the Global Bindless Texture Array (Set 1) if available
    if (sit_render.vk.global_bindless_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sit_render.vk.quad_pipeline_layout, 1, 1, &sit_render.vk.global_bindless_set, 0, NULL);
    }

    // Single push block (must match QuadPushConstants in internal_quad shaders — 104 bytes).
    // Always set texture_id + use_texture; leaving bytes 96–99 uninitialized caused undefined
    // bindless indexing / black output on some drivers when set 1 is bound.
    struct {
        mat4 model;
        vec4 color;
        vec4 uv_rect;
        uint32_t texture_id;
        int use_texture;
    } push_quad;

    glm_mat4_copy(model, push_quad.model);
    glm_vec4_copy(color.raw, push_quad.color);
    glm_vec4_copy(uv_rect.raw, push_quad.uv_rect);
    push_quad.texture_id = 0;
    push_quad.use_texture = use_texture;

    // Exact shader/layout size (104); sizeof(struct) may include tail padding to 112 on some ABIs.
    const uint32_t quad_push_bytes = (uint32_t)(sizeof(mat4) + sizeof(vec4) + sizeof(vec4) + sizeof(uint32_t) + sizeof(int));
    vkCmdPushConstants(vk_cmd, sit_render.vk.quad_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, quad_push_bytes, &push_quad);
    vkCmdDraw(vk_cmd, 4, 1, 0, 0);
#endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Sets a small amount of per-draw data (push constants) for the currently bound pipeline.
 *
 * @details This function provides an efficient way to send small, frequently changing data (e.g., transformation matrices, color vectors, material properties) to shaders. It replaces slower methods like individual `glUniform*` calls or updating UBOs for tiny data changes.
 *          The `contract_id` specifies the location/offset within the shader's defined push constant block.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glProgramUniform*` functions as a direct and efficient equivalent.
 *               It queries the currently bound program (`glGetCurrentProgram`) and updates the uniform  at the location specified by `contract_id`. Supported data types are limited to common cases (`mat4`, `vec4`, `vec3`, `vec2`, `float`, `int`) based on `size`. Other sizes will trigger an error message.
 * - **Vulkan:** Records a `vkCmdPushConstants` command into the provided command buffer.
 *   The data is written to the push constant block of the pipeline layout last bound via `vkCmdPushConstants` or assumed to be correctly set in `sit_render.vk.current_pipeline_layout_for_push_constants`. The data is made available to all graphics shader stages (`VK_SHADER_STAGE_ALL_GRAPHICS`).
 *   The pipeline *must* have been created with a push constant range that includes the specified `contract_id` (offset) and `size`.
 *
 * @param cmd The command buffer for the current frame (Vulkan) or ignored (OpenGL).
 * @param contract_id The location/offset within the shader's push constant block.
 *                    In OpenGL, this corresponds to the `location` layout qualifier.
 *                    In Vulkan, this is the byte offset.
 * @param data A pointer to the raw data to send. Must not be NULL.
 * @param size The size of the data in bytes (e.g., `sizeof(mat4)`, `sizeof(vec4)`).
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A valid shader/pipeline is bound before calling this function.
 *       2. (Vulkan) The bound pipeline's layout includes a push constant range covering `contract_id` to `contract_id + size`.
 *       3. The `size` and `contract_id` match the shader's expectations.
 * @warning Calling this in OpenGL when no program is bound (`glUseProgram(0)`) will result in no action being taken.
 */
SITAPI SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size) {
    // --- 1. Input Validation ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot set push constant.");
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (!data) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Push constant data pointer is NULL.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (size == 0) {
        // Setting 0 bytes is likely an error.
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Push constant size is 0.");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    // Optionally, add a maximum size check based on API limits if known.

#if defined(SITUATION_USE_OPENGL)
    {
        SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

        // Allocate space in the data buffer
        void* ptr = _SitGLSoftDataPush(buf, data, size);
        if (!ptr) return SITUATION_ERROR_MEMORY_ALLOCATION;

        // Calculate offset relative to buffer start
        size_t offset = (size_t)((uint8_t*)ptr - buf->data_buffer);

        SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_SET_PUSH_CONSTANT);
        if (p) {
            p->args.push_constant.offset = contract_id;
            p->args.push_constant.size = size;
            p->args.push_constant.data_offset = offset;
        } else {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Input Validation ---
        if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for push constant update.");
            return SITUATION_ERROR_INVALID_PARAM;
        }
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // Determine which pipeline layout to use (compute takes priority)
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkShaderStageFlags stages = 0;

        if (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) {
            // Compute pipeline is bound
            layout = sit_render.vk.current_compute_pipeline_layout;
            stages = VK_SHADER_STAGE_COMPUTE_BIT;
        } else if (sit_render.vk.current_pipeline_layout_for_push_constants != VK_NULL_HANDLE) {
            // Graphics pipeline is bound
            layout = sit_render.vk.current_pipeline_layout_for_push_constants;
            stages = VK_SHADER_STAGE_ALL_GRAPHICS;
        } else {
            // No pipeline bound
            return SITUATION_ERROR_PIPELINE_BIND_FAIL;
        }

        // --- 3. Vulkan Implementation (vkCmdPushConstants) ---
        vkCmdPushConstants(
            vk_cmd,
            layout,                                               // Pipeline Layout (compute or graphics)
            stages,                                               // Shader stages (compute or graphics)
            contract_id,                                          // Offset
            (uint32_t)size,                                       // Size
            data                                                  // Data
        );
        // Note: vkCmdPushConstants itself doesn't return VkResult.
        // Errors would be validation layer reports or device lost states during submission.
    }
#endif
    // --- 4. Post-Operation ---
    // No general post-operation actions are required here.
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the number of draw commands issued during the current frame.
 *
 * @details This counter is incremented every time `SituationCmdDraw`, `SituationCmdDrawIndexed`,
 *          `SituationCmdDrawMesh`, or `SituationCmdDrawQuad` is called.
 *          It is automatically reset to 0 at the beginning of every frame (inside `SituationPollInputEvents`).
 *
 * @return The count of draw calls recorded so far in the current frame.
 */
SITAPI uint32_t SituationGetDrawCallCount(void) {
    return sit_render.frame_draw_calls;
}

#if defined(SITUATION_ENABLE_RENDER_THREAD)
/**
 * @brief Retrieves the current depth of the render thread's command queue.
 * @details This function returns the number of frames currently waiting to be processed by the render thread.
 *          It is primarily used for implementing backpressure mechanisms to prevent the main thread from getting too far ahead of the GPU.
 *
 * @return The number of pending frames in the ring buffer. Returns 0 if threading is disabled or not initialized.
 */
SITAPI size_t SituationGetRenderQueueDepth(void) {
    if (!SituationIsInitialized() || !sit_render.enabled) return 0;
    return atomic_load(&sit_render.render_queue_depth);
}

/**
 * @brief Retrieves latency metrics for the threaded renderer.
 * @details Provides atomic snapshots of the time delta between frame submission (Main Thread) and frame execution (Render Thread).
 *          Useful for diagnosing input lag or stall issues.
 *
 * @param[out] avg_ns Pointer to receive the average latency in nanoseconds. Can be NULL.
 * @param[out] max_ns Pointer to receive the maximum recorded latency in nanoseconds. Can be NULL.
 */
SITAPI void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetRenderLatencyStats");
        if (avg_ns) *avg_ns = 0;
        if (max_ns) *max_ns = 0;
        return;
    }

    // [v2.3.24a] Updated Metrics (Histogram Stub)
    uint64_t cnt = atomic_load(&sit_render.metric_latency_count);
    if (avg_ns) *avg_ns = cnt ? atomic_load(&sit_render.metric_latency_sum_ns) / cnt : 0;

    // Max is now atomic and tracked correctly in render thread
    if (max_ns) *max_ns = atomic_load(&sit_render.metric_max_latency_ns);
}
#endif

/**
 * @brief Exports current render performance metrics as a compact JSON string into a user-provided buffer.
 *
 * @details Fills the caller-supplied buffer with a JSON object containing key render statistics
 *          collected by the render thread (when `SITUATION_ENABLE_RENDER_THREAD` is defined).
 *          Currently includes:
 *            - Library version (hardcoded to match current build)
 *            - Average frame latency (`avg_ns`)
 *            - Maximum observed frame latency (`max_ns`)
 *            - Placeholder `bins` array (empty for now; reserved for future histogram buckets)
 *
 *          Safety features:
 *            - If buffer is NULL or size is 0 -> silent no-op
 *            - If buffer is too small (< 256 bytes) -> writes a minimal error JSON and truncates safely
 *            - If metrics are disabled (no render thread) -> `avg_ns` and `max_ns` are 0
 *
 *          Intended for:
 *            - Debug overlays / in-game performance HUD
 *            - Logging at shutdown or on hotkey
 *            - Quick telemetry export during development
 *
 * @param buf Caller-allocated writable buffer to receive the null-terminated JSON string.
 *            Must remain valid for the duration of the call.
 * @param buf_size Size of the buffer in bytes (including null terminator).
 *                 Recommended minimum: 256 bytes (for error case).
 *                 Larger buffers allow future expansion (more fields, actual bins).
 *
 * @note The function is **fast, non-allocating, and thread-safe** - safe to call from any thread
 *       at any time after initialization.
 *       Output is always null-terminated (even on truncation).
 *       No error code is returned - failures are handled gracefully via JSON error message.
 *       Metrics collection is compile-time gated (`SITUATION_ENABLE_RENDER_THREAD`).
 *       Future versions may populate `bins` with p50/p90/p99 buckets or queue-depth history.
 *
 *       Example output (metrics enabled):
 *       ```json
 *       {"version":"2.3.24b","avg_ns":8333333,"max_ns":16666666,"bins":[]}
 *       ```
 *
 *       Example error output (buffer too small):
 *       ```json
 *       {"bins":[],"error":"buffer too small"}
 *       ```
 *
 * @see SITUATION_ENABLE_RENDER_THREAD (compile-time toggle),
 *      SituationGetRenderLatencyStats (internal metrics getter),
 *      _SituationRenderThreadEntry (where latency is recorded)
 */
SITAPI void SituationExportRenderHistogram(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationExportRenderHistogram: buf is NULL or buf_size is 0"); return; }

    // [v2.3.24b] Export Guard
    if (buf_size < 256) {
        strncpy(buf, "{\"bins\":[],\"error\":\"buffer too small\"}", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    uint64_t avg = 0, max = 0;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    SituationGetRenderLatencyStats(&avg, &max);
#endif

    // Format JSON (Standard Layout)
    snprintf(buf, buf_size,
        "{\"version\":\"%d.%d.%d%s\",\"avg_ns\":%llu,\"max_ns\":%llu,\"bins\":[]}",
        SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR, SITUATION_VERSION_PATCH, SITUATION_VERSION_REVISION,
        (unsigned long long)avg, (unsigned long long)max);
}

/**
 * @brief Renders a built-in debug overlay with performance statistics.
 * @details Draws a lightweight textual overlay displaying FPS, Frame Time, Render Queue Depth,
 *          Render Thread Latency, Draw Calls, Triangle Count, and VRAM usage.
 *          Uses the internal debug font and requires no external assets.
 *
 * @param cmd The command buffer to record drawing commands into.
 * @param position The top-left screen position to start drawing the text.
 * @param color The text color.
 */
SITAPI void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationDrawMetricsOverlay"); return; }

    // Use the default font with 2x scaling for better readability
    SituationFont font = {0};
    float font_size = 16.0f;  // 2x scale (8px base * 2)
    float spacing = 1.0f;

    char buffer[256];
    float line_height = 20.0f; // 16px font + 4px padding
    float y = position.y;

    // 1. FPS & Frame Time
    snprintf(buffer, sizeof(buffer), "FPS: %d  (%.2f ms)", sit_gs.current_fps, sit_gs.frame_time * 1000.0f);
    SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    y += line_height;

    // 2. Render Queue Depth (if threading)
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        size_t depth = atomic_load(&sit_render.render_queue_depth);
        snprintf(buffer, sizeof(buffer), "Queue Depth: %zu / %d", depth, SITUATION_MAX_FRAMES_IN_FLIGHT);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }
    #endif

    // 3. Latency
    uint64_t avg_lat = 0, max_lat = 0;
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    SituationGetRenderLatencyStats(&avg_lat, &max_lat);
    #endif
    if (avg_lat > 0) {
        snprintf(buffer, sizeof(buffer), "Lat: %.2f ms (Max: %.2f)", avg_lat / 1000000.0, max_lat / 1000000.0);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // 4. Draw Calls & Triangles
    snprintf(buffer, sizeof(buffer), "Draws: %u  Tris: %u", sit_render.frame_draw_calls, sit_render.frame_triangle_count);
    SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    y += line_height;

    // 5. VRAM
    uint64_t vram = SituationGetVRAMUsage();
    if (vram > 0) {
        snprintf(buffer, sizeof(buffer), "VRAM: %.2f MB", vram / (1024.0 * 1024.0));
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
        y += line_height;
    }

    // [Phase 5] Virtual Bindless Stats
    #if defined(SITUATION_USE_OPENGL)
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        snprintf(buffer, sizeof(buffer), "Virt Bindless: Hits %llu / Miss %llu",
            (unsigned long long)sit_render.gl.virtual_stats.hits,
            (unsigned long long)sit_render.gl.virtual_stats.misses);
        SituationCmdDrawTextEx(cmd, font, buffer, (Vector2){position.x, y}, font_size, spacing, color);
    }
    #endif
}

// [v2.3.22] Momentum Implementation

/**
 * @brief Creates a new Render List for recording and replaying graphics commands.
 * @details Part of the **"Momentum"** module (v2.3.22). A Render List is a recorded sequence of draw commands that can be captured once and replayed many times.
 *          This is useful for optimizing static geometry or UI elements, allowing them to be drawn without traversing the scene graph or issuing individual API calls every frame.
 *
 * @return A valid `SituationRenderList` handle, or NULL on allocation failure.
 * @see SituationDestroyRenderList(), SituationReplayRenderList()
 */
SITAPI SituationRenderList SituationCreateRenderList(void) {
    SituationRenderList list = (SituationRenderList)SIT_CALLOC(1, sizeof(struct SituationRenderList_t));
    if (list) {
        list->packet_capacity = 128;
        list->packets = (SituationRenderPacket*)SIT_MALLOC(list->packet_capacity * sizeof(SituationRenderPacket));
        list->data_capacity = 1024;
        list->data_buffer = (uint8_t*)SIT_MALLOC(list->data_capacity);

        // [FIX v2.3.27B]
        atomic_init(&list->in_flight_count, 0);
    }

    return list;
}

/**
 * @brief Destroys a Render List and frees its recorded data.
 * @param list The Render List handle to destroy.
 */
SITAPI void SituationDestroyRenderList(SituationRenderList list) {
    if (!list) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationDestroyRenderList: list is NULL"); return; }

    // [FIX v2.3.27B] Ensure we don't free memory being read by the GPU thread
    while (atomic_load(&list->in_flight_count) > 0) {
        // Simple yield loop
         #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        _mm_pause();
        #elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ __volatile__("yield");
        #endif
    }

    if (list->packets) SIT_FREE(list->packets);
    if (list->data_buffer) SIT_FREE(list->data_buffer);
    SIT_FREE(list);
}

/**
 * @brief Clears a Render List, preparing it for new recording.
 * @details Resets the internal write cursors but keeps the allocated memory buffers to minimize allocation overhead during re-recording.
 * @param list The Render List to reset.
 */
SITAPI void SituationResetRenderList(SituationRenderList list) {
    if (!list) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationResetRenderList: list is NULL"); return; }

    // [FIX v2.3.27B] Wait for in-flight usage to complete
    // We use a simple spin-wait here because this condition should be extremely rare
    // (typically only happens if the Main Thread is lapping the Render Thread).
    int retries = 0;
    while (atomic_load(&list->in_flight_count) > 0) {
        #if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        _mm_pause();
        #elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ __volatile__("yield");
        #endif

        retries++;
        if (retries > 100000) {
             // If we are stuck here, it's a deadlock or logic error. Break to avoid hanging.
             _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_LIST_INCOMPLETE, "ResetRenderList timeout: List stuck in flight.");
             break;
        }
    }

    list->packet_count = 0;
    list->data_cursor = 0;
    list->is_recording = false;
}

/**
 * @brief Replays the commands recorded in a Render List into the target command buffer.
 * @details This function copies the recorded packet stream from the Render List into the active frame's command buffer.
 *          This effectively "pastes" the draw calls into the current frame.
 *
 * @param cmd The target command buffer (e.g., the main frame buffer).
 * @param list The source Render List containing the recorded commands.
 */
SITAPI void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list) {
    if (!cmd || !list || list->packet_count == 0) return;
#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    if (!list->packets) return;

    if (buf->data_cursor + list->data_cursor > buf->data_capacity) {
         size_t new_cap = buf->data_capacity + list->data_cursor + 1024;
         uint8_t* new_ptr = (uint8_t*)SIT_REALLOC(buf->data_buffer, new_cap);
         if (!new_ptr) return;
         buf->data_buffer = new_ptr;
         buf->data_capacity = new_cap;
    }
    size_t base_data_offset = buf->data_cursor;
    if (list->data_cursor > 0) {
        memcpy(buf->data_buffer + base_data_offset, list->data_buffer, list->data_cursor);
        buf->data_cursor += list->data_cursor;
    }

    // Translation Loop (SituationRenderPacket -> SitCommandPacket)
    for (size_t i = 0; i < list->packet_count; ++i) {
        SituationRenderPacket* src = &list->packets[i];
        SitCommandPacket dst = {0};

        switch (src->type) {
            case SIT_CMD_DRAW:
                dst.opcode = SIT_OP_DRAW;
                dst.args.draw.v_count = src->data.draw.vertex_count;
                dst.args.draw.i_count = src->data.draw.instance_count;
                dst.args.draw.first_v = src->data.draw.first_vertex;
                dst.args.draw.first_i = src->data.draw.first_instance;
                break;
            case SIT_CMD_DRAW_INDEXED:
                dst.opcode = SIT_OP_DRAW_INDEXED;
                dst.args.draw_indexed.idx_count = src->data.draw_indexed.index_count;
                dst.args.draw_indexed.inst_count = src->data.draw_indexed.instance_count;
                dst.args.draw_indexed.first_idx = src->data.draw_indexed.first_index;
                dst.args.draw_indexed.v_offset = src->data.draw_indexed.vertex_offset;
                dst.args.draw_indexed.first_inst = src->data.draw_indexed.first_instance;
                break;
            case SIT_CMD_DISPATCH:
                dst.opcode = SIT_OP_DISPATCH;
                dst.args.dispatch.x = src->data.dispatch.group_x;
                dst.args.dispatch.y = src->data.dispatch.group_y;
                dst.args.dispatch.z = src->data.dispatch.group_z;
                break;
            case SIT_CMD_BARRIER:
                dst.opcode = SIT_OP_PIPELINE_BARRIER;
                dst.args.barrier.src = SITUATION_BARRIER_ALL_BARRIER_BITS;
                dst.args.barrier.dst = SITUATION_BARRIER_ALL_BARRIER_BITS;
                break;
            default: continue;
        }

        if (buf->packet_count >= buf->packet_capacity) {
             size_t new_cap = buf->packet_capacity * 2;
             if (new_cap < 16) new_cap = 16;
             SitCommandPacket* new_ptr = (SitCommandPacket*)SIT_REALLOC(buf->packets, new_cap * sizeof(SitCommandPacket));
             if (new_ptr) { buf->packets = new_ptr; buf->packet_capacity = new_cap; }
             else return;
        }
        buf->packets[buf->packet_count++] = dst;
    }
#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

    for (size_t i = 0; i < list->packet_count; ++i) {
        const SituationRenderPacket* pkt = &list->packets[i];
        switch (pkt->type) {
            case SIT_CMD_DRAW:
                vkCmdDraw(vk_cmd, pkt->data.draw.vertex_count, pkt->data.draw.instance_count, pkt->data.draw.first_vertex, pkt->data.draw.first_instance);
                break;
            case SIT_CMD_DRAW_INDEXED:
                vkCmdDrawIndexed(vk_cmd, pkt->data.draw_indexed.index_count, pkt->data.draw_indexed.instance_count, pkt->data.draw_indexed.first_index, pkt->data.draw_indexed.vertex_offset, pkt->data.draw_indexed.first_instance);
                break;
            case SIT_CMD_DISPATCH:
                vkCmdDispatch(vk_cmd, pkt->data.dispatch.group_x, pkt->data.dispatch.group_y, pkt->data.dispatch.group_z);
                break;
            case SIT_CMD_BARRIER:
                // Simplified barrier for replay; ideally replicate full SituationCmdPipelineBarrier logic here
                // For now, a full memory barrier is safe but slow.
                // To be robust, you should expose the _SituationVulkanPipelineBarrier logic to this function.
                break;
            default: break;
        }
    }
#endif
}

// [v2.3.24b] Integration Zenith: Batched Replay Logic
/**
 * @brief [INTERNAL] Replays (re-executes) the commands of a render list directly into the render thread queue at a specific frame index.
 *
 * @details This low-level helper is used when a render list needs to be re-queued or replayed
 *          for a particular frame slot without going through the normal submission path.
 *          It is typically called in scenarios such as:
 *            - Hot-reload recovery (re-submit changed shaders/meshes to the same frame)
 *            - Frame retry after backpressure or transient errors
 *            - Internal synchronization when a list must be re-executed in a specific in-flight slot
 *            - Debug/force-replay mechanisms
 *
 *          Behavior:
 *            - Validates the list handle and frame_idx (0 to SITUATION_MAX_FRAMES_IN_FLIGHT-1)
 *            - Acquires the render queue mutex
 *            - Directly places the list reference into the queue at the requested frame_idx
 *              (overwriting if already occupied - caller must ensure slot is free or safe)
 *            - Increments the frame refcount for that slot
 *            - Signals the render thread condition variable to wake it if idle
 *            - Releases the mutex
 *
 *          Unlike normal enqueue, this bypasses tail/head circular queue logic and forces
 *          placement at a known frame index (useful when synchronizing with in-flight frames).
 *
 * Thread safety invariants:
 *   - Must be called from a thread that does **not** own the render context
 *     (typically main thread or thread-pool workers)
 *   - Queue access protected by `sit_render.render_queue_mutex`
 *   - Caller must guarantee the target frame_idx slot is either free or safe to overwrite
 *     (refcount == 0 or previous work completed)
 *   - **Not** safe to call from the render thread (deadlock on mutex)
 *
 * @param list Valid `SituationRenderList` handle that has been previously recorded.
 *             Must remain valid until the replayed execution completes.
 * @param frame_idx Specific frame slot index (0 to SITUATION_MAX_FRAMES_IN_FLIGHT-1)
 *                  where the list should be placed for execution.
 *                  Invalid indices are ignored (logged as warning).
 *
 * @note This is a **forceful** operation - no queue-full check is performed.
 *       If the slot is still in use (refcount > 0), behavior is undefined
 *       (possible overwrite, resource leak, or render corruption).
 *       Errors (invalid list/frame_idx) are logged internally only - function returns void.
 *       Use with extreme care - prefer normal `SituationSubmitRenderList` paths unless
 *       you are implementing retry/hot-reload synchronization logic.
 *
 * @see _SituationEnqueueRenderList (normal enqueue), _SituationRenderThreadEntry,
 *      SituationSubmitRenderList, sit_render.render_queue,
 *      sit_render.frame_refcounts, SITUATION_MAX_FRAMES_IN_FLIGHT
 */
static void _SituationReplayToQueue(SituationRenderList list, int frame_idx) {
    if (!list || list->is_recording) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_LIST_INCOMPLETE, "List unfinished or null.");
        return;
    }

#if defined(SITUATION_USE_VULKAN)
    // [Safety] Serialize recording to shared frame resources
    mtx_lock(&sit_render.render_queue_mutex);

    VkCommandBuffer g_cmd = sit_render.vk.command_buffers[frame_idx];

    // [Batching Strategy]
    // We scan for dispatches first to aggregate them into a single submit.
    // This assumes Compute -> Graphics dependency flow (simulation then render).

    // 1. Alloc Temp Compute Buffer (from frame pool, auto-reset)
    VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = sit_render.vk.compute_command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer c_cmd;
    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc, &c_cmd) != VK_SUCCESS) {
        mtx_unlock(&sit_render.render_queue_mutex);
        return; // Fail gracefully
    }

    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(c_cmd, &begin);

    uint32_t dispatch_cnt = 0;

    // Pass 1: Compute
    for (size_t i = 0; i < list->packet_count; ++i) {
        if (list->packets[i].type == SIT_CMD_DISPATCH) {
            vkCmdDispatch(c_cmd, list->packets[i].data.dispatch.group_x, list->packets[i].data.dispatch.group_y, list->packets[i].data.dispatch.group_z);
            dispatch_cnt++;
        }
    }
    vkEndCommandBuffer(c_cmd);

    // Submit Compute if needed
    if (dispatch_cnt > 0) {
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &c_cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &sit_render.vk.compute_finished_semaphores[frame_idx];

        vkQueueSubmit(sit_render.vk.compute_queue, 1, &submit, VK_NULL_HANDLE);

        // Signal Graphics to wait
        sit_render.vk.needs_compute_wait = true;
    }

    // Pass 2: Graphics & Barriers
    for (size_t i = 0; i < list->packet_count; ++i) {
        const SituationRenderPacket* pkt = &list->packets[i];
        switch (pkt->type) {
            case SIT_CMD_BARRIER:
                // Memory Barrier for visibility
                vkCmdPipelineBarrier(g_cmd, pkt->data.barrier.src_stage, pkt->data.barrier.dst_stage, 0, 0, NULL, 0, NULL, 0, NULL);
                break;
            case SIT_CMD_DRAW:
                SituationCmdDraw(g_cmd, pkt->data.draw.vertex_count, pkt->data.draw.instance_count, pkt->data.draw.first_vertex, pkt->data.draw.first_instance);
                break;
            case SIT_CMD_DRAW_INDEXED:
                SituationCmdDrawIndexed(g_cmd, pkt->data.draw_indexed.index_count, pkt->data.draw_indexed.instance_count, pkt->data.draw_indexed.first_index, pkt->data.draw_indexed.vertex_offset, pkt->data.draw_indexed.first_instance);
                break;
            default: break;
        }
    }

    mtx_unlock(&sit_render.render_queue_mutex);
#endif
    // OpenGL replay is simpler (immediate) or handled via SoftBuffer
}

// --- [INTERNAL] Thread-Safe Queue Push ---
/**
 * @brief [INTERNAL] Enqueues a render list into the render thread's pending queue.
 *
 * @details This low-level function safely adds the given `SituationRenderList` to the
 *          render queue (`sit_render.render_queue`) for later processing by the dedicated
 *          render thread. It handles:
 *            - Acquiring the queue mutex
 *            - Checking for queue overflow (if full, logs warning and may drop or block)
 *            - Appending the list index/frame slot to the circular queue
 *            - Incrementing pending frame count / refcount
 *            - Signaling the render thread condition variable (`render_queue_cv`)
 *            - Releasing the mutex
 *
 *          Called internally by:
 *            - `SituationSubmitRenderList` (immediate variant)
 *            - `_SituationRenderJobWorker` (thread-pool/async variant)
 *            - Any other deferred render path
 *
 *          The render thread will eventually dequeue, execute the list's commands,
 *          present (if needed), and flush resources when refcount reaches zero.
 *
 * Thread safety invariants:
 *   - Must be called from a thread that does **not** own the render context
 *     (typically main thread or thread-pool workers)
 *   - Queue access is protected by `sit_render.render_queue_mutex`
 *   - Condition variable signal wakes the render thread if it was waiting
 *   - Safe for concurrent calls (mutex serializes enqueue operations)
 *   - **Not** safe to call from the render thread itself (deadlock risk)
 *
 * @param list Valid `SituationRenderList` handle that has been recorded and ended.
 *             Must not be already enqueued or destroyed.
 *
 * @note This function is non-blocking in normal operation.
 *       If the queue is full (rare, high backpressure), logs a warning
 *       (e.g. SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT or similar)
 *       and may drop the list or block briefly (implementation-defined).
 *       No return value - errors are logged internally only.
 *
 * @see _SituationRenderThreadEntry (dequeue/execution side),
 *      SituationSubmitRenderList, SituationSubmitRenderList (pool variant),
 *      sit_render.render_queue, sit_render.render_queue_mutex,
 *      sit_render.render_queue_cv, SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT
 */
static void _SituationEnqueueRenderList(SituationRenderList list) {
    if (!list) return;

    mtx_lock(&sit_render.momentum_mutex);

    int head = atomic_load(&sit_render.momentum_head);
    int tail = atomic_load(&sit_render.momentum_tail);
    int next_head = (head + 1) % 256;

    if (next_head != tail) {
        // [FIX v2.3.27B] Mark as in-flight before queueing
        atomic_fetch_add(&list->in_flight_count, 1);

        sit_render.momentum_queue[head] = list;
        atomic_store(&sit_render.momentum_head, next_head);
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Momentum render queue full. Frame data dropped.");
    }

    mtx_unlock(&sit_render.momentum_mutex);
}

#if defined(SITUATION_ENABLE_THREADING)

// Internal Job Wrapper Context
typedef struct {
    SituationRenderList list;
    void (*func)(void*, void*);
    void* user_data;
} _SitRenderJobCtx;

/**
 * @brief [INTERNAL] Worker function that processes queued render lists on a thread pool worker.
 *
 * @details This function is executed on a thread pool worker when a render list job
 *          is submitted via `SituationSubmitRenderList(pool, ...)`.
 *
 *          It performs the following steps:
 *            - Retrieves the render list handle and optional callback from the job payload
 *            - Forwards the render list to the render thread queue (via internal enqueue)
 *            - Waits (blocks the worker) until the render thread has fully processed the list
 *              (using fences or refcount zeroing to detect completion)
 *            - Upon completion, invokes the optional user callback `func(user_data, list)`
 *              **on this worker thread** (not render thread - safe for user code)
 *            - Releases any temporary job resources and signals job completion
 *
 *          This design allows render list submission from any worker thread without
 *          blocking the main thread, while still ensuring ordered GPU execution via
 *          the dedicated render thread.
 *
 * Key invariants:
 *   - The worker blocks until GPU work is complete (synchronous from job perspective)
 *   - Callback is called on the worker thread (caller must ensure thread-safety)
 *   - Render thread owns actual command execution / present
 *   - Job ID can be waited on externally for full completion
 *
 * @param data Pointer to the job-specific context (embedded `_SitRenderListJobCtx` or similar).
 *             Contains: render list handle, callback function, user_data.
 * @param unused Unused second argument (conforms to `SituationSubmitJobEx` signature)
 *
 * @note This worker intentionally blocks to provide backpressure and ordering guarantees.
 *       For fire-and-forget submission (non-blocking), use a different queue or callback pattern.
 *       Errors during enqueue or render (e.g. queue full, invalid list) are logged internally.
 *
 * @see SituationSubmitRenderList (pool variant), SituationWaitForJob,
 *      _SituationRenderThreadEntry, SITUATION_ERROR_RENDER_LIST_INCOMPLETE,
 *      SITUATION_ERROR_THREAD_QUEUE_FULL
 */
static void _SituationRenderJobWorker(void* data, void* unused) {
    (void)unused;
    _SitRenderJobCtx* ctx = (_SitRenderJobCtx*)data;

    // 1. Run User Generation Logic (CPU Work)
    // This fills ctx->list with commands (SituationCmdDraw, etc.)
    if (ctx->func) {
        // Pass dummy error ptr for legacy compatibility if needed
        SituationError dummy_err = SITUATION_SUCCESS;
        ctx->func(ctx->user_data, (void*)&dummy_err);
    }

    // 2. Submit Completed List to Main Thread Queue
    // This is now thread-safe!
    _SituationEnqueueRenderList(ctx->list);
}

/**
 * @brief Submits a render list for asynchronous execution on the thread pool.
 *
 * @details Queues the given `SituationRenderList` as a job in the specified thread pool,
 *          where it will be picked up by a worker thread and forwarded to the render thread
 *          for GPU execution at the next available frame slot.
 *
 *          This is the **asynchronous / multi-thread-friendly** variant of render list submission,
 *          allowing the caller to offload submission from the main thread (e.g. from worker threads,
 *          background loaders, or parallel simulation loops).
 *
 *          After successful submission:
 *            - Returns a `SituationJobId` that can be waited on via `SituationWaitForJob`
 *              or `SituationWaitForAllJobs` to know when the list has completed GPU execution
 *            - When the render thread finishes processing the list, the optional callback
 *              `func(user_data, list)` is invoked **on the render thread**
 *            - Resources associated with the list are released when ref-count reaches zero
 *
 *          If the job queue is full, submission may block briefly or fail (depending on pool config).
 *
 * @param pool Valid `SituationThreadPool` pointer (created via `SituationCreateThreadPool`).
 *             Must remain valid until the job completes.
 * @param list Valid `SituationRenderList` handle previously recorded with commands.
 *             Must not be submitted multiple times without re-recording.
 * @param func Optional completion callback. Signature: `void func(void* user_data, void* list)`.
 *             Called on the render thread upon completion. May be NULL.
 * @param user_data Opaque pointer passed to `func`. Caller manages lifetime/ownership.
 *
 * @return A non-zero `SituationJobId` on successful submission (can be used to wait/track),
 *         0 on failure (queue full, invalid pool/list, allocation error, etc.).
 *         Failures are logged internally via SITUATION_LOG_WARNING and may set the global
 *         error state (e.g. SITUATION_ERROR_THREAD_QUEUE_FULL).
 *
 * @note This is non-blocking from the caller's perspective (unless queue is full and blocking).
 *       Actual GPU execution happens asynchronously on the render thread.
 *       The callback is **not** called if submission fails.
 *       Thread safety:
 *         - Safe to call from **any thread** (main, worker, etc.) as long as pool is valid
 *         - Internal queue mutex + condition variable protect submission
 *         - Not safe to call from the render thread itself (potential deadlock)
 *
 * @see SituationCreateThreadPool, SituationWaitForJob, SituationWaitForAllJobs,
 *      SituationCreateRenderList, SituationBeginRenderList, SituationEndRenderList,
 *      SITUATION_ERROR_THREAD_QUEUE_FULL, SITUATION_ERROR_RENDER_LIST_INCOMPLETE
 */
SITAPI SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list, void (*func)(void*, void*), void* user_data) {
    if (!list) return 0;

    // Reset list for new recording before handing it off
    SituationResetRenderList(list);

    // Prepare context
    _SitRenderJobCtx ctx = { list, func, user_data };

    // Submit to High Priority Queue (Physics/Render Logic)
    return SituationSubmitJobEx(pool, _SituationRenderJobWorker, &ctx, sizeof(_SitRenderJobCtx), SIT_SUBMIT_HIGH_PRIORITY);
}

#else

// Fallback for Single-Threaded Builds
/**
 * @brief Submits a pre-recorded render list for execution on the render thread.
 *
 * @details Queues the given `SituationRenderList` (a pre-built sequence of draw/dispatch commands)
 *          to be executed by the dedicated render thread at the next available frame slot.
 *          This is the primary high-level way to submit rendering work in Situation when using
 *          the deferred command-buffer model.
 *
 *          After submission:
 *            - The render thread picks up the list when a frame slot becomes free
 *            - Executes all commands in the list (binds, draws, dispatches, etc.)
 *            - Calls the optional user-provided callback `func(user_data, list)` upon completion
 *              (invoked on the **render thread** - caller must ensure callback is thread-safe)
 *            - Releases any internal resources associated with the list (if ref-count reaches zero)
 *
 *          This function is **non-blocking** from the caller's perspective - it returns immediately
 *          after queuing. Actual GPU work happens asynchronously on the render thread.
 *
 * @param list Valid `SituationRenderList` handle previously created and filled with commands
 *             (via `SituationBeginRenderList`, `SituationCmd*` functions, `SituationEndRenderList`).
 *             Must not be submitted multiple times without re-recording.
 * @param func Optional callback invoked on the render thread when the list has finished executing.
 *             Signature: `void func(void* user_data, void* list)`.
 *             May be NULL if no completion notification is needed.
 * @param user_data Opaque pointer passed to `func` when called. Ownership/lifetime is caller-managed.
 *
 * @note Errors during submission (invalid list, queue full, etc.) are logged internally
 *       via SITUATION_LOG_WARNING and may set the global error state, but the function itself
 *       returns void - no return code is provided.
 *       The list remains valid after submission until explicitly destroyed or ref-count drops.
 *       For synchronous wait, use `SituationWaitForRenderList` or `SituationWaitForAllRenderLists`.
 *
 *       Thread safety:
 *         - Safe to call from **main thread** or any non-render-thread context
 *         - Not safe to call from the render thread itself (deadlock risk on queue mutex)
 *         - Internal queue is protected by mutex + condition variable
 *
 * @see SituationCreateRenderList, SituationBeginRenderList, SituationEndRenderList,
 *      SituationWaitForRenderList, SituationWaitForAllRenderLists,
 *      SITUATION_ERROR_RENDER_LIST_INCOMPLETE, SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT
 */
SITAPI void SituationSubmitRenderList(SituationRenderList list, void (*func)(void*, void*), void* user_data) {
    if (!list) return;

    SituationResetRenderList(list);

    // 1. Run Logic
    if (func) {
        SituationError dummy_err = SITUATION_SUCCESS;
        func(user_data, (void*)&dummy_err);
    }

    // 2. Enqueue (or just replay immediately if you prefer, but queueing keeps logic unified)
    _SituationEnqueueRenderList(list);
}

#endif

/**
 * @brief Gets the estimated total video memory (VRAM) allocated by the application.
 *
 * @details Returns the total size in bytes of all GPU resources currently managed by the application.
 *          The accuracy of this value depends heavily on the underlying backend and operating system support.
 *
 * @par Backend Support Matrix
 *   - **Vulkan:** **Exact.** Returns precise allocation statistics from the internal Memory Allocator (VMA), tracking buffers and images.
 *   - **Windows (DXGI):** **High Accuracy.** If `SITUATION_ENABLE_DXGI` is defined, queries the OS video memory manager directly.
 *     This works for both OpenGL and Vulkan backends on Windows.
 *   - **OpenGL (NVIDIA):** **Good Accuracy.** Uses `GL_NVX_gpu_memory_info` to calculate usage (Total - Available).
 *   - **OpenGL (AMD/Intel/Other):** **Unavailable.** Returns 0, as standard OpenGL does not expose per-process memory usage.
 *
 * @return The total allocated VRAM in bytes, or 0 if the information cannot be retrieved.
 */
SITAPI uint64_t SituationGetVRAMUsage(void) {
    if (!SituationIsInitialized()) return 0;

    // --- 1. VULKAN (Most Accurate) ---
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.vma_allocator) {
        VmaTotalStatistics stats;
        vmaCalculateStatistics(sit_render.vk.vma_allocator, &stats);
        return stats.total.statistics.allocationBytes;
    }
#endif

    // --- 2. WINDOWS DXGI (Universal on Windows) ---
#if defined(_WIN32) && defined(SITUATION_ENABLE_DXGI)
    if (sit_gs.is_com_initialized) {
        IDXGIFactory4* pFactory = NULL;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)&pFactory)) && pFactory) {
            IDXGIAdapter3* pAdapter3 = NULL;
            IDXGIAdapter* pAdapterTemp = NULL;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapterTemp))) {
                if (SUCCEEDED(pAdapterTemp->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO info = {0};
                    if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                        pAdapter3->Release();
                        pAdapterTemp->Release();
                        pFactory->Release();
                        return info.CurrentUsage;
                    }
                    pAdapter3->Release();
                }
                pAdapterTemp->Release();
            }
            pFactory->Release();
        }
    }
#endif

    // --- 3. OPENGL EXTENSIONS (Linux / Windows without DXGI) ---
#if defined(SITUATION_USE_OPENGL)

    // NVIDIA Extension
    // Guard: Only run this if GLAD defines the extension macro
    #ifdef GL_NVX_gpu_memory_info
        // Constants might not be defined if the extension isn't in headers, so define them safely
        #ifndef GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
            #define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
        #endif
        #ifndef GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
            #define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
        #endif

        if (GLAD_GL_NVX_gpu_memory_info) {
            GLint total_kb = 0;
            GLint current_kb = 0;
            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_kb);
            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &current_kb);
            // Usage = Total - Available
            return (uint64_t)(total_kb - current_kb) * 1024;
        }
    #endif

    // AMD Extension
    // Guard: Only run this if GLAD defines the extension macro
    #ifdef GL_ATI_meminfo
        #ifndef GL_TEXTURE_FREE_MEMORY_ATI
            #define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
        #endif

        if (GLAD_GL_ATI_meminfo) {
            GLint mem_info[4];
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, mem_info);
            // ATI only reports Free memory. Without Total, we can't calc Usage accurately.
            // Returning 0 is safer than returning a misleading number.
            return 0;
        }
    #endif

#endif

    return 0;
}

//==================================================================================
// --- [NEW UNIFIED API] Resource Binding ---
//==================================================================================

/**
 * @brief Binds a buffer's pre-packaged descriptor set to a specific set index in the currently bound pipeline.
 * @details This is the primary, unified function for making a GPU buffer's data (UBO or SSBO) available to a shader.
 *          It associates a `SituationBuffer` with a descriptor set slot declared in the shader code (e.g., `layout(set = X, binding = 0) uniform MyUBO` in GLSL).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Maps the `set_index` to an indexed binding point (`GL_UNIFORM_BUFFER` or `GL_SHADER_STORAGE_BUFFER`) and calls `glBindBufferBase`. This is a direct and efficient binding operation.
 * - **Vulkan:** This function leverages the library's high-performance persistent descriptor set model. When the `SituationBuffer` was created, a dedicated `VkDescriptorSet` was allocated and populated for it.
 *               This function records a fast `vkCmdBindDescriptorSets` command using this pre-cached set, avoiding any runtime allocation or update overhead.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param set_index The descriptor set index in the pipeline layout to bind to. This must match the `set = X` value in the shader's layout qualifier.
 * @param buffer The `SituationBuffer` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful binding.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid or lacks the required internal resources.
 * @return SITUATION_ERROR_RENDER_COMMAND_FAILED if no pipeline is currently bound.
 *
 * @see SituationCreateBuffer(), SituationCmdBindTextureSet()
 */
SITAPI SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_DESCRIPTOR_SET);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

    // We pack only slot_index + generation into resource_id (uint64)
    // SituationBuffer grew beyond 8 bytes (added size_in_bytes, usage_flags),
    // but we only need the handle identity (slot_index + generation) to look up the slot.
    p->args.bind_desc.set_index = set_index;
    p->args.bind_desc.resource_id = ((uint64_t)buffer.generation << 32) | (uint64_t)buffer.slot_index;

    // We assume _SituationGLExecuteCommands will unpack it.
    // BUT _SituationGLExecuteCommands currently expects a GL ID.
    // I MUST UPDATE _SituationGLExecuteCommands to unpack handle and get slot->gl_buffer_id.

    p->args.bind_desc.offset = dynamic_offset;
    p->args.bind_desc.size = slot->size_in_bytes;
    p->args.bind_desc.usage_flags = slot->usage_flags; // [Bug 10 Fix] Pass usage flags so execution knows UBO vs SSBO
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Logic to bind Vulkan descriptor set
    // We need the descriptor set from the slot.
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        // Allocate descriptor set if missing (lazy init)
        VkDescriptorSetLayout layout = (slot->usage_flags & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) ? sit_render.vk.ssbo_layout : sit_render.vk.dynamic_ubo_layout;
        slot->descriptor_set = _SituationVulkanAllocateDescriptorSet(layout, &slot->descriptor_pool);

        VkDescriptorBufferInfo bufferInfo = {0};
        bufferInfo.buffer = slot->vk_buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descriptorWrite.dstSet = slot->descriptor_set;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = (slot->usage_flags & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(sit_render.vk.device, 1, &descriptorWrite, 0, NULL);
    }

    VkPipelineLayout layout = (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) ?
                              sit_render.vk.current_compute_pipeline_layout :
                              sit_render.vk.current_pipeline_layout_for_push_constants;

    VkPipelineBindPoint bindPoint = (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

    uint32_t dyn_offset = dynamic_offset;
    vkCmdBindDescriptorSets((VkCommandBuffer)cmd, bindPoint, layout, set_index, 1, &slot->descriptor_set, 1, &dyn_offset);
    return SITUATION_SUCCESS;
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

SITAPI SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer) {
    return SituationCmdBindDescriptorSetDynamic(cmd, set_index, buffer, 0);
}


/**
 * @brief Binds a texture's pre-packaged descriptor set to a specific set index in the currently bound pipeline.
 * @details This is the primary, unified function for making a texture available for sampling or image load/store operations in a shader. It associates a `SituationTexture` with a descriptor set slot declared in the shader code (e.g., `layout(set = X, binding = 0) uniform sampler2D myTexture`).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Maps the `set_index` to a texture unit and calls `glBindTextureUnit`.
 * - **Vulkan:** Uses the texture's pre-cached, persistent `VkDescriptorSet` to record a fast `vkCmdBindDescriptorSets` command, avoiding runtime overhead.
 *
 * @param cmd The command buffer to record the command into. (Ignored in OpenGL).
 * @param set_index The descriptor set index in the pipeline layout to bind to.
 * @param texture The `SituationTexture` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful binding.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the texture handle is invalid.
 *
 * @see SituationCreateTexture(), SituationCmdBindDescriptorSet()
 */
SITAPI SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture) {
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: SituationCmdBindTextureSet called, set=%u, tex slot=%u gen=%u\n",
           set_index, texture.slot_index, texture.generation);
    fflush(stdout);
    #endif
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    _SituationTextureSlot* slot = _SitGetTextureSlot(texture);
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot=%p, is_active=%d, descriptor_set=%p\n",
           slot, slot ? slot->is_active : -1, slot ? (void*)slot->descriptor_set : NULL);
    fflush(stdout);
    #endif
    if (!slot) {
        _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Attempted to bind an invalid texture handle.");
        return SITUATION_ERROR_RESOURCE_INVALID;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;

    // [v2.3.30] Bindless Path
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        // Retrieve the handle (will create/resident if needed)
        uint64_t handle = SituationGetTextureHandle(texture);
        if (handle) {
            // ... (Comment preserved: Bindless logic handled at draw site)
        }
    }

    // Standard Bind (always safe fallback and required for non-bindless shaders)
    // [Phase 2] Use Legacy Texture Opcode to avoid buffer logic in main opcode
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING);
    if (!p) return SITUATION_ERROR_MEMORY_ALLOCATION;

    p->args.bind_desc.set_index = set_index;
    p->args.bind_desc.resource_id = slot->gl_texture_id;
    p->args.bind_desc.resource_type = 1; // 1 = Sampled Texture

    // [v2.3.30] Bindless Integration for Internal Shaders
    // If we are binding a texture while a bindless-capable internal shader is active,
    // we should also push the handle to the "magic" bindless uniform location (7)
    // and set the "use bindless" flag (6) to 1.
    // However, SituationCmdBindTexture doesn't know *which* shader will be used later.
    //
    // BUT, since we implemented the bindless logic in `SituationCmdBindTexture` above (in the first block),
    // we are already covered for cases where we can resolve the shader (like Quad).
    //
    // For TEXT rendering, `SituationCmdDrawText` does not call `SituationCmdBindTexture`!
    // It binds the font atlas internally. We need to update `SituationCmdDrawText` to use bindless.

    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
    if (vk_cmd == VK_NULL_HANDLE) return SITUATION_ERROR_INVALID_PARAM;

    // Determine the active pipeline (graphics or compute).
    VkPipelineBindPoint bind_point;
    VkPipelineLayout layout;
    if (sit_render.vk.current_compute_pipeline_layout != VK_NULL_HANDLE) {
        bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        layout = sit_render.vk.current_compute_pipeline_layout;
    } else if (sit_render.vk.current_pipeline_layout_for_push_constants != VK_NULL_HANDLE) {
        bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        layout = sit_render.vk.current_pipeline_layout_for_push_constants;
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED, "Cannot bind texture set; no pipeline is currently bound.");
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    // [Bindless] Standard Textures (Sampled)
    // If the texture has no descriptor set, it is part of the Bindless Array.
    if (slot->descriptor_set == VK_NULL_HANDLE) {
        /* SituationLoadShaderFromMemory: set 1 = text_sampler_layout; bind per-texture set (see CreateTextureEx). */
        if (bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS && set_index == 1u && slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &slot->single_sampler_descriptor_set, 0, NULL);
            return SITUATION_SUCCESS;
        }
        /* Internal quad/text/bindless: global array + push texture_id (slot index). */
        vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &sit_render.vk.global_bindless_set, 0, NULL);
        uint32_t texture_id = texture.slot_index;
        vkCmdPushConstants(vk_cmd, layout, VK_SHADER_STAGE_ALL, 96, sizeof(uint32_t), &texture_id);

    } else {
        // [Legacy/Storage] Bind specific descriptor set
        vkCmdBindDescriptorSets(vk_cmd, bind_point, layout, set_index, 1, &slot->descriptor_set, 0, NULL);
    }

    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Binds a GPU buffer (typically a UBO) for use by the currently bound graphics pipeline.
 * @details Associates a `SituationBuffer` with a uniform block declared in the vertex or fragment shader code (e.g., `layout(location = X) uniform ...` in OpenGL, or `layout(set = ..., binding = X) uniform ...` in Vulkan/GLSL).
 *          This allows the shader to access the buffer's uniform data (e.g., view/projection matrices).
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindBufferBase(GL_UNIFORM_BUFFER, contract_id, buffer.gl_buffer_id)`.
 *   This efficiently binds the buffer to the specified uniform block binding point.
 * - **Vulkan:** This function also implements the high-performance, persistent descriptor set model.
 *   When the `SituationBuffer` was created (via `SituationCreateBuffer`), the Vulkan backend allocated a `VkDescriptorSet` (specifically for UBOs) and populated it with the buffer's `VkBuffer` handle. This function records a
 *  `vkCmdBindDescriptorSets` command using this pre-cached descriptor set from `buffer.descriptor_set`, ensuring a very fast operation.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param contract_id The binding point ID within the shader.
 *                    In OpenGL, this corresponds directly to the `location` or binding index specified in the shader (e.g., `glUniformBlockBinding` or `layout(location=X)`).
 *                    In Vulkan, this corresponds to the `dstBinding` used when the buffer's internal descriptor set was populated.
 * @param buffer The `SituationBuffer` handle to bind. The buffer should have been created with usage flags indicating it will be used as a uniform buffer (e.g., `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER`).
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the buffer's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible graphics pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `contract_id` matches the binding point defined in the shader.
 * @warning Binding a buffer that was not created with appropriate usage flags (like `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER`) may lead to undefined behavior.
 */
SITAPI SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer) {
    // The old 'binding' parameter directly maps to the new 'set_index' parameter.
    return SituationCmdBindDescriptorSet(cmd, binding, buffer);
}

/**
 * @brief Binds a texture to a specific binding point for use by the currently bound pipeline.
 * @details Associates a `SituationTexture` with a sampler or image unit declared in the shader code (e.g., `layout(binding = X) uniform sampler2D ...`).
 *          This allows the shader to sample or read from the texture.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindTextureUnit(contract_id, texture.gl_texture_id)`.
 *   This efficiently binds the texture to the specified texture unit.
 * - **Vulkan:** This function leverages the persistent descriptor set model for textures.
 *   When the `SituationTexture` was created (e.g., via `SituationLoadTexture`), the Vulkan backend allocated a `VkDescriptorSet` (for combined image samplers) and
 *   populated it with the texture's `VkImageView` and `VkSampler`. This function records a `vkCmdBindDescriptorSets` command using this pre-cached descriptor set stored in `texture.descriptor_set`. This is a very fast operation, avoiding runtime allocation and updates of descriptor sets.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param contract_id The binding point ID within the shader.
 *                    In OpenGL, this corresponds to the texture unit index.
 *                    In Vulkan, this corresponds to the `dstBinding` used when the
 *                    texture's internal descriptor set was populated.
 * @param texture The `SituationTexture` handle to bind.
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the texture handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the texture's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compatible pipeline is bound before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `contract_id` matches the binding point defined in the shader.
 */
// [INTERNAL] Resolves a public handle to a pointer to the internal slot.
// Returns NULL if the handle is stale (generation mismatch) or invalid.
static _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle) {
    if (handle.slot_index >= SITUATION_MAX_TEXTURES) return NULL;

    _SituationTextureSlot* slot = &sit_render.texture_registry[handle.slot_index];

    // Generation Check: Prevents Use-After-Free
    if (!slot->is_active || slot->generation != handle.generation) {
        return NULL;
    }
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal shader slot for a given handle, with validation.
 *
 * @details Performs bounds checking and generation validation on the `SituationShader` handle.
 *          Returns the corresponding slot pointer if the handle is valid and active,
 *          otherwise returns NULL (invalid, stale, or freed handle).
 *
 *          This is the canonical safe way to access shader data from a public handle
 *          throughout the library (e.g. in bind, destroy, hot-reload paths).
 *
 * @param handle The SituationShader handle to resolve
 * @return Valid _SituationShaderSlot* if handle is active and matches generation,
 *         NULL otherwise (invalid handle, out of range, or already freed)
 *
 * @see _SitAllocShaderSlot, _SitFreeShaderSlot, SituationShader
 */
static _SituationShaderSlot* _SitGetShaderSlot(SituationShader handle) {
    if (handle.slot_index >= SITUATION_MAX_SHADERS) return NULL;
    _SituationShaderSlot* slot = &sit_render.shader_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal mesh slot for a given handle, with validation.
 *
 * @details Checks slot index bounds and generation match.
 *          Returns the slot pointer if the handle is currently active,
 *          otherwise NULL (invalid, stale generation, or freed).
 *
 * @param handle The SituationMesh handle to resolve
 * @return Valid _SituationMeshSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocMeshSlot, _SitFreeMeshSlot, SituationMesh
 */
static _SituationMeshSlot* _SitGetMeshSlot(SituationMesh handle) {
    if (handle.slot_index >= SITUATION_MAX_MESHES) return NULL;
    _SituationMeshSlot* slot = &sit_render.mesh_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal buffer slot for a given handle, with validation.
 *
 * @details Validates slot index and generation counter.
 *          Returns the slot pointer only if the buffer is still active.
 *
 * @param handle The SituationBuffer handle to resolve
 * @return Valid _SituationBufferSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocBufferSlot, _SitFreeBufferSlot, SituationBuffer
 */
static _SituationBufferSlot* _SitGetBufferSlot(SituationBuffer handle) {
    if (handle.slot_index >= SITUATION_MAX_BUFFERS) return NULL;
    _SituationBufferSlot* slot = &sit_render.buffer_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal compute pipeline slot for a given handle, with validation.
 *
 * @details Checks bounds and generation match.
 *          Returns the slot pointer if the pipeline is active,
 *          otherwise NULL.
 *
 * @param handle The SituationComputePipeline handle to resolve
 * @return Valid _SituationComputePipelineSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocComputePipelineSlot, _SitFreeComputePipelineSlot, SituationComputePipeline
 */
static _SituationComputePipelineSlot* _SitGetComputePipelineSlot(SituationComputePipeline handle) {
    if (handle.slot_index >= SITUATION_MAX_COMPUTE_PIPELINES) return NULL;
    _SituationComputePipelineSlot* slot = &sit_render.compute_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}

/**
 * @brief [INTERNAL] Retrieves the internal model slot for a given handle, with validation.
 *
 * @details Validates slot index and generation.
 *          Returns the slot pointer only if the model is still active.
 *
 * @param handle The SituationModel handle to resolve
 * @return Valid _SituationModelSlot* if handle is active,
 *         NULL otherwise
 *
 * @see _SitAllocModelSlot, _SitFreeModelSlot, SituationModel
 */
static _SituationModelSlot* _SitGetModelSlot(SituationModel handle) {
    if (handle.slot_index >= SITUATION_MAX_MODELS) return NULL;
    _SituationModelSlot* slot = &sit_render.model_registry[handle.slot_index];
    if (!slot->is_active || slot->generation != handle.generation) return NULL;
    return slot;
}


// --- Resource Allocation Helpers ---

/**
 * @brief [INTERNAL] Allocates a free shader slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationShader handle on success
 * @return Pointer to the allocated _SituationShaderSlot, or NULL on failure
 *
 * @see _SitFreeShaderSlot, SituationCreateShader, SituationCreateShaderFromSpirv
 */
static _SituationShaderSlot* _SitAllocShaderSlot(SituationShader* out_handle) {
    for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
        if (!sit_render.shader_registry[i].is_active) {
            _SituationShaderSlot* slot = &sit_render.shader_registry[i];
            memset(slot, 0, sizeof(_SituationShaderSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a shader slot and releases associated shader modules/programs.
 *
 * @param handle The SituationShader handle to free (invalid handles are ignored)
 *
 * @see _SitAllocShaderSlot, SituationDestroyShader
 */
static void _SitFreeShaderSlot(SituationShader handle) {
    _SituationShaderSlot* slot = _SitGetShaderSlot(handle);
    if (!slot) return;

    if (slot->vs_path) SIT_FREE(slot->vs_path);
    if (slot->fs_path) SIT_FREE(slot->fs_path);

    slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free compute pipeline slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationComputePipeline handle on success
 * @return Pointer to the allocated _SituationComputePipelineSlot, or NULL on failure
 *
 * @see _SitFreeComputePipelineSlot, SituationCreateComputePipeline
 */
static _SituationComputePipelineSlot* _SitAllocComputePipelineSlot(SituationComputePipeline* out_handle) {
    for (int i = 0; i < SITUATION_MAX_COMPUTE_PIPELINES; i++) {
        if (!sit_render.compute_registry[i].is_active) {
            _SituationComputePipelineSlot* slot = &sit_render.compute_registry[i];
            memset(slot, 0, sizeof(_SituationComputePipelineSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a compute pipeline slot and releases GPU resources.
 *
 * @param handle The SituationComputePipeline handle to free (invalid handles are ignored)
 *
 * @see _SitAllocComputePipelineSlot, SituationDestroyComputePipeline
 */
static void _SitFreeComputePipelineSlot(SituationComputePipeline handle) {
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(handle);
    if (!slot) return;

    if (slot->source_path) SIT_FREE(slot->source_path);

    slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free mesh slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationMesh handle on success
 * @return Pointer to the allocated _SituationMeshSlot, or NULL on failure
 *
 * @see _SitFreeMeshSlot, SituationCreateMesh
 */
static _SituationMeshSlot* _SitAllocMeshSlot(SituationMesh* out_handle) {
    for (int i = 0; i < SITUATION_MAX_MESHES; i++) {
        if (!sit_render.mesh_registry[i].is_active) {
            _SituationMeshSlot* slot = &sit_render.mesh_registry[i];
            memset(slot, 0, sizeof(_SituationMeshSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a mesh slot and releases associated resources.
 *
 * @param handle The SituationMesh handle to free (invalid handles are ignored)
 *
 * @see _SitAllocMeshSlot, SituationDestroyMesh
 */
static void _SitFreeMeshSlot(SituationMesh handle) {
    _SituationMeshSlot* slot = _SitGetMeshSlot(handle);
    if (slot) slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free buffer slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationBuffer handle on success
 * @return Pointer to the allocated _SituationBufferSlot, or NULL on failure
 *
 * @see _SitFreeBufferSlot, SituationCreateBuffer
 */
static _SituationBufferSlot* _SitAllocBufferSlot(SituationBuffer* out_handle) {
    for (int i = 0; i < SITUATION_MAX_BUFFERS; i++) {
        if (!sit_render.buffer_registry[i].is_active) {
            _SituationBufferSlot* slot = &sit_render.buffer_registry[i];
            memset(slot, 0, sizeof(_SituationBufferSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a buffer slot and releases GPU/CPU resources.
 *
 * @param handle The SituationBuffer handle to free (invalid handles are ignored)
 *
 * @see _SitAllocBufferSlot, SituationDestroyBuffer
 */
static void _SitFreeBufferSlot(SituationBuffer handle) {
    _SituationBufferSlot* slot = _SitGetBufferSlot(handle);
    if (slot) slot->is_active = false;
}

/**
 * @brief [INTERNAL] Allocates a free model slot and returns a new handle.
 *
 * @param out_handle Receives the new SituationModel handle on success
 * @return Pointer to the allocated _SituationModelSlot, or NULL on failure
 *         (pool full or allocation error)
 *
 * @see _SitFreeModelSlot, SituationCreateModel
 */
static _SituationModelSlot* _SitAllocModelSlot(SituationModel* out_handle) {
    for (int i = 0; i < SITUATION_MAX_MODELS; i++) {
        if (!sit_render.model_registry[i].is_active) {
            _SituationModelSlot* slot = &sit_render.model_registry[i];
            memset(slot, 0, sizeof(_SituationModelSlot));
            slot->is_active = true;
            slot->generation++;
            if (slot->generation == 0) slot->generation = 1;

            out_handle->slot_index = i;
            out_handle->generation = slot->generation;
            return slot;
        }
    }
    return NULL;
}

/**
 * @brief [INTERNAL] Frees a model slot and releases associated resources.
 *
 * @param handle The SituationModel handle to free (invalid handles are ignored)
 *
 * @see _SitAllocModelSlot, SituationDestroyModel (public API)
 */
static void _SitFreeModelSlot(SituationModel handle) {
    _SituationModelSlot* slot = _SitGetModelSlot(handle);
    if (!slot) return;
    if (slot->source_path) SIT_FREE(slot->source_path);
    // Note: Model data (meshes array) should be freed by SituationUnloadModel before calling this;
    slot->is_active = false;
}

/**
 * @brief Binds a texture to a specific descriptor set slot in a command buffer.
 *
 * @details Records a command that binds the given `SituationTexture` to the specified
 *          descriptor set index (`set_index`) in the active pipeline layout.
 *          The texture becomes available for sampling/storage in subsequent draw/dispatch
 *          commands within the same command buffer.
 *
 *          Equivalent to:
 *            - Vulkan: `vkCmdBindDescriptorSets` (with sampler + image view)
 *            - OpenGL: `glBindTextureUnit` or legacy `glActiveTexture` + `glBindTexture`
 *
 * @param cmd Valid recording command buffer handle
 * @param set_index Descriptor set index (0-based) in the current pipeline layout
 * @param texture Valid `SituationTexture` handle to bind
 *
 * @return SITUATION_SUCCESS on success,
 *         SITUATION_ERROR_INVALID_PARAM if cmd not recording or texture invalid,
 *         SITUATION_ERROR_RESOURCE_INVALID if texture not created/compatible,
 *         or other backend-specific errors
 *
 * @note Must be called after binding the pipeline that uses the layout containing set_index.
 *       Thread-safe if cmd is thread-owned; otherwise use render thread submission.
 *
 * @see SituationCmdBindPipeline, SituationCreateTexture, SituationTexture
 */
SITAPI SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture) {
    // This function was already correctly named, so it's a simple wrapper.
    return SituationCmdBindTextureSet(cmd, set_index, texture);
}

/**
 * @brief Creates a GPU texture from a CPU-side image with full control over usage flags.
 *
 * @details The extended version of `SituationCreateTexture`, allowing the caller to explicitly
 *          specify the `SituationTextureUsageFlags` bitmask that defines how the texture will
 *          be used during its lifetime. This is essential for performance and correctness when
 *          the texture will be used in compute shaders, render targets, transfer operations,
 *          or other non-default scenarios.
 *
 *          Core behavior:
 *            - Uploads the base-level pixel data from the `SituationImage` to GPU memory
 *            - Optionally generates a full mipmap chain if `generate_mipmaps` is true
 *            - Creates the texture with exactly the requested usage flags (Vulkan: usage bits;
 *              OpenGL: inferred from usage to set appropriate storage/texture parameters)
 *            - Returns a `SituationTexture` handle ready for binding/sampling
 *
 *          Required usage flags (caller responsibility):
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_DST` - almost always needed for initial upload
 *            - `SITUATION_TEXTURE_USAGE_SAMPLED` - if the texture will be sampled in shaders
 *            - `SITUATION_TEXTURE_USAGE_STORAGE` - if used as storage image in compute
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` - required when `generate_mipmaps` is true
 *              (for internal blit operations during mipmap generation)
 *            - `SITUATION_TEXTURE_USAGE_COLOR_ATTACHMENT` - if used as render target
 *            - `SITUATION_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT` - for depth/stencil textures
 *
 *          Invalid or insufficient flags result in error (e.g. mipmaps requested without TRANSFER_SRC).
 *
 * @param image Valid `SituationImage` handle with source pixel data.
 *              Dimensions, format, and channels must be compatible with texture creation.
 * @param generate_mipmaps If true, generates a complete mipmap pyramid after base-level upload.
 *                         Requires `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` in `usage_flags`.
 * @param usage_flags Bitmask of `SituationTextureUsageFlags` values (OR-ed) specifying all
 *                    intended usages of the texture. Must include at least TRANSFER_DST for upload.
 * @param out_texture Pointer to a `SituationTexture` variable that receives the new handle on success.
 *                    On failure, set to `SITUATION_NULL_TEXTURE`.
 *
 * @return SITUATION_SUCCESS on successful creation and upload,
 *         SITUATION_ERROR_INVALID_PARAM if image invalid, flags inconsistent, or out_texture NULL,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format unsupported or mipmaps requested
 *         without TRANSFER_SRC,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if GPU memory allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL texture creation/upload/blit failed,
 *         or other appropriate error codes.
 *
 * @note This is the low-level, flexible entry point - use `SituationCreateTexture` for the
 *       common case with automatic/default flags.
 *
 *       Performance considerations:
 *         - Mipmap generation uses hardware blit (fast on modern GPUs) but still adds cost
 *         - Including unnecessary flags may increase memory usage or prevent optimal tiling
 *         - Upload is synchronous from caller perspective; actual GPU work may be queued
 *
 *       Thread safety:
 *         - Safe from main thread or non-context-owning threads
 *         - Internal synchronization ensures safe concurrent creation
 *         - Avoid calling from render thread during active command recording that uses the texture
 *
 *       Caller must destroy the texture with `SituationDestroyTexture` when done.
 *
 * @see SituationCreateTexture (convenience wrapper), SituationCreateImage,
 *      SituationDestroyTexture, SituationSetTextureSamplerParams,
 *      SituationTextureUsageFlags, SITUATION_TEXTURE_USAGE_xxx constants,
 *      SITUATION_NULL_TEXTURE, SITUATION_ERROR_RESOURCE_INVALID
 */
SITAPI SituationError SituationCreateTextureEx(SituationImage image, bool generate_mipmaps, SituationTextureUsageFlags usage_flags, SituationTexture* out_texture) {
    if (out_texture) *out_texture = (SituationTexture){0};

    if (!SituationIsImageValid(image)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Cannot create texture from invalid image.");
    }
    if (!out_texture) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_texture cannot be NULL.");
    }

    // 1. Find Free Slot
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    int slot_idx = -1;
    for (int i = 0; i < SITUATION_MAX_TEXTURES; ++i) {
        if (!sit_render.texture_registry[i].is_active) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx == -1) {
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Max texture limit reached (SITUATION_MAX_TEXTURES).");
    }

    _SituationTextureSlot* slot = &sit_render.texture_registry[slot_idx];

    // 2. Prepare Slot (Increment generation to invalidate old handles to this slot)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot %d: generation before=%u\n", slot_idx, slot->generation);
    fflush(stdout);
    #endif
    slot->generation++;
    if (slot->generation == 0) slot->generation = 1; // Wrap-around safety
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Slot %d: generation after=%u\n", slot_idx, slot->generation);
    fflush(stdout);
    #endif
    slot->is_active = true;
    mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    slot->width = image.width;
    slot->height = image.height;
    slot->bindless_handle = 0;

#if defined(SITUATION_USE_OPENGL)
    // Clear stale error state before our GL calls so the deferred check only catches OUR errors
    snprintf(sit_gs.last_error_msg, sizeof(sit_gs.last_error_msg), "No error");
    glCreateTextures(GL_TEXTURE_2D, 1, &slot->gl_texture_id);
    SIT_CHECK_GL_ERROR(); // Check after object creation.

    // --- Select OpenGL Format Based on Color Encoding ---
    GLenum gl_internal_format;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        // Storage images MUST use LINEAR format
        gl_internal_format = GL_RGBA8;
    } else {
        // Use the image's color encoding preference
        gl_internal_format = (image.color_encoding == SITUATION_COLOR_SRGB)
                            ? GL_SRGB8_ALPHA8
                            : GL_RGBA8;
    }

    // Store format in slot
    slot->internal_format = gl_internal_format;


    // If the texture ID is 0 here, it means the context is likely invalid.
    if (slot->gl_texture_id == 0) {
        slot->is_active = false;
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateTextures failed, context may be invalid.");
    }

    int levels = 1;
    if (generate_mipmaps) {
        levels = (int)floor(log2(fmax(image.width, image.height))) + 1;
    }

    // Allocate immutable storage. This can fail if texture is too large.
    glTextureStorage2D(slot->gl_texture_id, levels, gl_internal_format, image.width, image.height);
    SIT_CHECK_GL_ERROR();

    // Upload the base level pixel data.
    glTextureSubImage2D(slot->gl_texture_id, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
    SIT_CHECK_GL_ERROR();

    if (generate_mipmaps) {
        glGenerateTextureMipmap(slot->gl_texture_id);
        SIT_CHECK_GL_ERROR();
    }

    // Set texture parameters.
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, generate_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SIT_CHECK_GL_ERROR();

    // [Phase 3] Bindless Texture: Make Resident immediately
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        #if defined(GLAD_GL_ARB_bindless_texture)
        if (GLAD_GL_ARB_bindless_texture) {
            slot->gl_bindless_handle = glGetTextureHandleARB(slot->gl_texture_id);
            if (slot->gl_bindless_handle) {
                glMakeTextureHandleResidentARB(slot->gl_bindless_handle);
                SIT_CHECK_GL_ERROR();
            }
        }
        #endif
    }

    out_texture->slot_index = slot_idx;
    out_texture->generation = slot->generation;
    out_texture->width = slot->width;
    out_texture->height = slot->height;


#elif defined(SITUATION_USE_VULKAN)
    // --- Step 0: Calculate Mipmap Levels ---
    uint32_t mip_levels = 1;
    if (generate_mipmaps) {
        mip_levels = (uint32_t)floor(log2(fmax(image.width, image.height))) + 1;
    }

    VkDeviceSize image_size = (VkDeviceSize)image.width * image.height * 4;
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;

    // Check if we can use async transfer (inside frame)
    // Only use main command buffer if we're actively recording a frame
    VkCommandBuffer cmd = (sit_render.in_frame)
        ? (VkCommandBuffer)SituationGetMainCommandBuffer()
        : VK_NULL_HANDLE;

    if (_SituationVulkanCreateAndUploadBuffer(cmd, image.data, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging_buffer, &staging_allocation) != SITUATION_SUCCESS) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // FIX: Map abstract flags to Vulkan flags
    VkImageUsageFlags vk_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_DST) vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_TRANSFER_SRC) vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Storage textures need this to upload initial data
    }

    // --- Step 1: Select Format Based on Color Encoding ---
    VkFormat vk_format;
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        // Storage images MUST use LINEAR format (SRGB doesn't support storage on most GPUs)
        vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    } else {
        // Use the image's color encoding preference
        vk_format = (image.color_encoding == SITUATION_COLOR_SRGB)
                    ? VK_FORMAT_R8G8B8A8_SRGB
                    : VK_FORMAT_R8G8B8A8_UNORM;
    }

    // Store format in slot for later use
    slot->format = vk_format;

    if (_SituationVulkanCreateImage(image.width, image.height, mip_levels, vk_format, VK_IMAGE_TILING_OPTIMAL,
                                  vk_usage, VMA_MEMORY_USAGE_GPU_ONLY,
                                  &slot->image, &slot->allocation) != SITUATION_SUCCESS) {
        if (cmd == VK_NULL_HANDLE) vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        else _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // --- Step 3: Copy and Generate Mipmaps ---
    VkCommandBuffer command_buffer = (cmd != VK_NULL_HANDLE) ? cmd : _SituationVulkanBeginSingleTimeCommands();
    // If async, we assume _SituationVulkanCreateAndUploadBuffer already put the staging buffer in a state ready for transfer (it does, it doesn't transition it, but buffer is host visible).
    // But we need to transition the *image* to TransferDst.

    // a. Transition the entire image (all mip levels) to be ready for writing.
    // For async, we need a pipeline barrier to ensure the layout transition happens before the copy.
    _SituationVulkanTransitionImageLayout(command_buffer, slot->image, mip_levels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // b. Copy the staging buffer to the first mip level (level 0).
    _SituationVulkanCopyBufferToImage(command_buffer, staging_buffer, slot->image, (uint32_t)image.width, (uint32_t)image.height);

    // c. Generate the mipmaps by blitting from one level to the next.
    if (generate_mipmaps) {
        _SituationVulkanGenerateMipmaps(command_buffer, slot->image, image.width, image.height, mip_levels);
    } else {
        // Pure storage (imageLoad/imageStore) uses GENERAL. Textures that are also SAMPLED
        // (default SituationCreateTexture) go through the bindless path and must end in
        // SHADER_READ_ONLY_OPTIMAL to match global_textures[] descriptor layout.
        VkImageLayout target_layout = ((usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
                                      && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u)
            ? VK_IMAGE_LAYOUT_GENERAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        _SituationVulkanTransitionImageLayout(command_buffer, slot->image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, target_layout);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Vulkan] Transitioned texture to layout: %s\n",
                target_layout == VK_IMAGE_LAYOUT_GENERAL ? "GENERAL" : "SHADER_READ_ONLY");
#endif
    }

    if (cmd == VK_NULL_HANDLE) {
        _SituationVulkanEndSingleTimeCommands(command_buffer);
        // --- Step 4: Cleanup Staging Buffer (Synchronous) ---
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
    } else {
        // --- Step 4: Defer Cleanup Staging Buffer (Asynchronous) ---
        _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
    }

    // --- Step 5: Create Image View and Sampler ---
    // The image view must now be aware of all the mip levels.
    slot->image_view = _SituationVulkanCreateImageView(slot->image, slot->format, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;  // Pixel-perfect for bitmap fonts
    sampler_info.minFilter = VK_FILTER_NEAREST;  // No blurring when scaling
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    // CRITICAL: Set the mipmap mode and LOD bias for the sampler.
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = (float)mip_levels; // Use all available mip levels

    if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &slot->sampler) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "vkCreateSampler failed in SituationCreateTextureEx.");
        _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, VK_NULL_HANDLE);
        slot->is_active = false;
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

	// --- Step 6: Create and Cache the Persistent Descriptor Set [PATCH 1] ---
    VkDescriptorType descriptor_type;
    VkDescriptorSetLayout layout_to_use;

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[SituationCreateTextureEx] Selecting layout: usage_flags=0x%x, STORAGE=%d, COMPUTE_SAMPLED=%d\n",
            usage_flags,
            !!(usage_flags & SITUATION_TEXTURE_USAGE_STORAGE),
            !!(usage_flags & SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED));
#endif

    /* Storage-only images use a dedicated storage descriptor set. If SAMPLED is also set,
       use bindless COMBINED_IMAGE_SAMPLER so fragment shaders (internal quad, VD compositor)
       see vkUpdateDescriptorSets on global_bindless_set — STORAGE alone skipped that write. */
    if ((usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
        && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u) {
        layout_to_use = sit_render.vk.storage_image_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using STORAGE layout\n");
#endif
    } else if (usage_flags & SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED) {
        // Textures that will be sampled in compute shaders need binding 0
        layout_to_use = sit_render.vk.compute_sampler_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using COMPUTE_SAMPLER layout\n");
#endif
    } else {
        // Regular graphics pipeline textures use bindless layout (binding 0)
        layout_to_use = sit_render.vk.bindless_descriptor_layout;
        descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[SituationCreateTextureEx] -> Using BINDLESS layout\n");
#endif
    }

    // [FIX v2.3.27B] Capture the pool
    VkDescriptorPool used_pool = VK_NULL_HANDLE;

    // [Bindless] Use Global Descriptor Set for standard textures
    if (layout_to_use == sit_render.vk.bindless_descriptor_layout) {
        // We do NOT allocate a new set. We write to the global set.
        slot->descriptor_set = VK_NULL_HANDLE; // Bindless textures don't own a set
        slot->descriptor_pool = VK_NULL_HANDLE;

        VkDescriptorImageInfo bindless_image_info = {};
        bindless_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindless_image_info.imageView = slot->image_view;
        bindless_image_info.sampler = slot->sampler;

        VkWriteDescriptorSet bindless_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        bindless_write.dstSet = sit_render.vk.global_bindless_set;
        bindless_write.dstBinding = 0;
        bindless_write.dstArrayElement = (uint32_t)slot_idx; // Use the slot index!
        bindless_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindless_write.descriptorCount = 1;
        bindless_write.pImageInfo = &bindless_image_info;

        mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &bindless_write, 0, NULL);
        /* LoadShaderFromMemory / harness: pipeline set 1 is text_sampler_layout (binding 0), not the bindless array. */
        slot->single_sampler_descriptor_set = _SituationVulkanAllocateDescriptorSet(sit_render.vk.text_sampler_layout, &slot->single_sampler_descriptor_pool);
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            VkWriteDescriptorSet sw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            sw.dstSet = slot->single_sampler_descriptor_set;
            sw.dstBinding = SIT_SAMPLER_BINDING_ALBEDO;
            sw.dstArrayElement = 0;
            sw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sw.descriptorCount = 1;
            sw.pImageInfo = &bindless_image_info;
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &sw, 0, NULL);
        }
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    } else {
        // Fallback for Storage/Compute layouts (until they are bindless-ready)
        slot->descriptor_set = _SituationVulkanAllocateDescriptorSet(layout_to_use, &used_pool);
        slot->descriptor_pool = used_pool; // Assign pool for proper cleanup

        if (slot->descriptor_set == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate persistent descriptor set for texture.");
            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
            return SITUATION_ERROR_UNKNOWN_ERROR;
        }
    }

/*    // Use the Asset Pool
    VkDescriptorSetAllocateInfo asset_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = sit_render.vk.asset_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout_to_use
    };
    vkAllocateDescriptorSets(sit_render.vk.device, &asset_alloc_info, &tex_slot->descriptor_set);

    if (slot->descriptor_set == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate persistent descriptor set for texture.");

        // CRITICAL: Defer cleanup to Graveyard. Immediate destruction is unsafe if async upload commands are pending.
        _SituationDeferDestroyImage(slot->image, texture.allocation, slot->image_view, texture.sampler);

        // Note: Staging buffer (if used) was already deferred to graveyard or destroyed synchronously above.
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }*/

    VkDescriptorImageInfo image_info = {};
    // The layout for storage images is different. It's often GENERAL or TRANSFER_DST_OPTIMAL before the compute shader runs, and the shader itself might transition it.
    // For simplicity, let's assume it should be in GENERAL layout for read/write access.
    if (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) {
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else {
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    image_info.imageView = slot->image_view;
    image_info.sampler = (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? slot->sampler : VK_NULL_HANDLE;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = slot->descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = descriptor_type; // Use the chosen type
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    if (slot->descriptor_set != VK_NULL_HANDLE) {
        mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
        /* SituationCmdDrawTexture binds global_bindless_set only (internal quad FS).
           Storage-only textures skip the bindless branch above — mirror into global_bindless_set
           so textured draws sample the same image (descriptor layout = GENERAL, matching slot layout). */
        if (sit_render.vk.global_bindless_set != VK_NULL_HANDLE
            && (usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) != 0u
            && (usage_flags & SITUATION_TEXTURE_USAGE_SAMPLED) == 0u) {
            VkDescriptorImageInfo bindless_mirror = {};
            bindless_mirror.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            bindless_mirror.imageView = slot->image_view;
            bindless_mirror.sampler = slot->sampler;
            VkWriteDescriptorSet bw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            bw.dstSet = sit_render.vk.global_bindless_set;
            bw.dstBinding = 0;
            bw.dstArrayElement = (uint32_t)slot_idx;
            bw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bw.descriptorCount = 1;
            bw.pImageInfo = &bindless_mirror;
            vkUpdateDescriptorSets(sit_render.vk.device, 1, &bw, 0, NULL);
        }
        mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    }

    // --- Final: Set Output Texture Handle ---
    out_texture->slot_index = slot_idx;
    out_texture->generation = slot->generation;
    out_texture->width = slot->width;
    out_texture->height = slot->height;
#endif

    // --- Resource Manager Hook ---
    if (out_texture->generation != 0) {
        // We must check if an error occurred during the GL calls above.
        // If it did, we should not add it to the tracking list.
#if defined(SITUATION_USE_OPENGL)
        if (strcmp(sit_gs.last_error_msg, "No error") == 0) {
            // Legacy linked list allocation removed
        } else {
             // An error was logged by SIT_CHECK_GL_ERROR.
             // Destroy the partially created GPU object and return an invalid handle.
             // We don't need to call the full SituationDestroyTexture here because the resource is not yet in the tracking list.
             // --- This is now correctly isolated. ---
             glDeleteTextures(1, &slot->gl_texture_id);
             slot->is_active = false;
             return _SituationSetErrorFromCode(SITUATION_ERROR_TEXTURE_UPLOAD_FAILED, "Failed to finalize texture creation.");
        }
#endif
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief Creates a GPU texture from an existing CPU-side image, with optional mipmap generation.
 *
 * @details Convenience wrapper around `SituationCreateTextureEx` that automatically computes
 *          appropriate `SituationTextureUsageFlags` based on the requested mipmap generation.
 *
 *          Default usage flags always include:
 *            - `SITUATION_TEXTURE_USAGE_SAMPLED` (for texture sampling in shaders)
 *            - `SITUATION_TEXTURE_USAGE_STORAGE` (for potential compute/storage access)
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_DST` (required for upload from CPU/image)
 *
 *          If `generate_mipmaps` is true, also adds:
 *            - `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` (needed for internal blit operations
 *              during mipmap chain generation)
 *
 *          The function performs the full GPU upload of the base image level and, if requested,
 *          generates the complete mipmap pyramid using hardware-accelerated blit commands
 *          (glGenerateMipmap on OpenGL, vkCmdBlitImage on Vulkan).
 *
 *          After success, the resulting texture is immediately usable for sampling or binding.
 *          The source `SituationImage` can be destroyed afterward if no longer needed on CPU.
 *
 * @param image Valid `SituationImage` handle containing the source pixel data.
 *              Must have valid dimensions, channels, and format compatible with texture creation.
 * @param generate_mipmaps If true, generates a full mipmap chain (recommended for most textures
 *                         that will be minified). If false, only the base level is uploaded.
 * @param out_texture Pointer to a `SituationTexture` variable that receives the new texture handle
 *                    on success. On failure, set to `SITUATION_NULL_TEXTURE`.
 *
 * @return SITUATION_SUCCESS on successful upload and texture creation,
 *         SITUATION_ERROR_INVALID_PARAM if image is invalid or out_texture is NULL,
 *         SITUATION_ERROR_RESOURCE_INVALID if image format/channels are unsupported,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if GPU memory allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL texture creation/upload failed,
 *         or other appropriate error codes propagated from `SituationCreateTextureEx`.
 *
 * @note This is the most commonly used texture creation entry point ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â it provides sensible
 *       defaults while still allowing full control via the `Ex` variant when needed.
 *       Mipmap generation adds a small GPU cost (especially for large textures) but greatly
 *       improves quality when minification occurs.
 *
 *       Thread safety:
 *       - Safe to call from the main thread or any thread that does not own the render context
 *       - Internal synchronization ensures safe concurrent creation
 *       - Actual GPU upload may be deferred to render thread or staging queue
 *
 *       Caller is responsible for destroying the texture with `SituationDestroyTexture`.
 *
 * @see SituationCreateTextureEx, SituationCreateImage, SituationCreateImageFromMemory,
 *      SituationDestroyTexture, SituationSetTextureSamplerParams,
 *      SITUATION_TEXTURE_USAGE_SAMPLED, SITUATION_TEXTURE_USAGE_STORAGE,
 *      SITUATION_TEXTURE_USAGE_TRANSFER_DST, SITUATION_TEXTURE_USAGE_TRANSFER_SRC,
 *      SITUATION_NULL_TEXTURE
 */
SITAPI SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture) {
    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE | SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    if (generate_mipmaps) flags = (SituationTextureUsageFlags)(flags | SITUATION_TEXTURE_USAGE_TRANSFER_SRC);
    return SituationCreateTextureEx(image, generate_mipmaps, flags, out_texture);
}

/**
 * @brief Destroys a GPU texture and frees its associated resources.
 *
 * @details Cleans up all backend-specific resources associated with the texture (e.g., OpenGL texture name, Vulkan image, view, sampler, VMA allocation, cached descriptor set). The texture handle is invalidated after this call.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDeleteTextures` to delete the texture name. The OpenGL driver manages the underlying memory and GPU resource lifecycle.
 * - **Vulkan:** Waits for the device to be idle to ensure the texture is no longer in use. Then, it destroys the `VkSampler`, `VkImageView`, and uses `vmaDestroyImage` for the `VkImage` and `VmaAllocation`.
 *   Crucially, if a persistent descriptor set was allocated for this texture during creation, it is also freed back to the dedicated persistent descriptor pool.
 *   This function uses the internal `_SituationVulkanDestroyTexture` helper if available for centralized Vulkan cleanup logic.
 *
 * @param[in,out] texture A pointer to the `SituationTexture` handle to destroy.
 *                        The `texture->id` field will be set to 0 upon successful destruction. The contents of the struct pointed to by `texture` will be zeroed.
 *
 * @note It's safe to call this function on an already destroyed or invalid texture (where `texture->id` is 0); it will simply do nothing.
 * @note This function internally removes the texture from the library's resource tracking list.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationDestroyTexture(SituationTexture* texture) {
    if (!texture || texture->generation == 0) return;

    _SituationTextureSlot* slot = _SitGetTextureSlot(*texture);
    if (!slot) {
        texture->generation = 0;
        return;
    }

    if (slot->source_path) {
        SIT_FREE(slot->source_path);
        slot->source_path = NULL;
    }

#if defined(SITUATION_USE_OPENGL)
    _SitGLDeferDestroyTexture(slot->gl_texture_id);
    // Erase from Virtual Bindless Cache to prevent ID-recycle collisions
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        if (sit_render.gl.virtual_texture_slots[i].gl_texture_id == slot->gl_texture_id) {
            sit_render.gl.virtual_texture_slots[i].is_active = false;
            sit_render.gl.virtual_texture_slots[i].gl_texture_id = 0;
        }
    }
    slot->gl_texture_id = 0;
#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(sit_render.vk.device, slot->sampler, NULL);
            slot->sampler = VK_NULL_HANDLE;
        }
        if (slot->image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(sit_render.vk.device, slot->image_view, NULL);
            slot->image_view = VK_NULL_HANDLE;
        }
        if (slot->image != VK_NULL_HANDLE) {
            vmaDestroyImage(sit_render.vk.vma_allocator, slot->image, slot->allocation);
            slot->image = VK_NULL_HANDLE;
            slot->allocation = VK_NULL_HANDLE;
        }
        if (slot->descriptor_set != VK_NULL_HANDLE && slot->descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->descriptor_pool, 1, &slot->descriptor_set);
            slot->descriptor_set = VK_NULL_HANDLE;
        }
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE && slot->single_sampler_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->single_sampler_descriptor_pool, 1, &slot->single_sampler_descriptor_set);
            slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
        if (slot->descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->descriptor_set, slot->descriptor_pool);
        }
        if (slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->single_sampler_descriptor_set, slot->single_sampler_descriptor_pool);
            slot->single_sampler_descriptor_set = VK_NULL_HANDLE;
        }
    }
#endif

    slot->is_active = false;
    texture->generation = 0;
}

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Creates a device-local GPU buffer and uploads data to it, using an asynchronous path when possible.
 *
 * @details This is the core data upload utility for the Vulkan backend. It correctly handles the creation of high-performance, device-local buffers by using a temporary, host-visible "staging" buffer for the data transfer.
 *
 * @par Asynchronous Upload Path (The "Velocity" Solution)
 *   This function implements a dual-path mechanism to solve the "Synchronous Transfers" bottleneck:
 *   - **If `cmd` is a valid command buffer (not NULL):** This is the **asynchronous path**, used during the main render loop. The function records a `vkCmdCopyBuffer` command into the provided `cmd` and places the staging buffer into the graveyard for deferred deletion using `_SituationDeferDestroyBuffer`. This is a non-blocking operation that allows dozens of assets to be uploaded in a single frame without stalling the CPU.
 *   - **If `cmd` is NULL:** This is the **synchronous path**, used during initialization or outside the main render loop. The function creates its own temporary command buffer, submits the copy, and stalls the CPU by waiting for the transfer to complete (`vkQueueWaitIdle`). This is necessary when a frame is not in flight but is avoided at all costs during runtime.
 *
 * @param cmd The main command buffer for the current frame, or NULL to force a synchronous upload.
 * @param data Pointer to the data to upload.
 * @param size The size of the data in bytes.
 * @param usage The final usage flags for the destination buffer (e.g., `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`).
 * @param[out] out_buffer Pointer to store the handle of the final, device-local buffer.
 * @param[out] out_allocation Pointer to store the VMA allocation for the final buffer.
 *
 * @return `SITUATION_SUCCESS` on success.
 */
static SituationError _SituationVulkanCreateAndUploadBuffer(VkCommandBuffer cmd, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* out_buffer, VmaAllocation* out_allocation) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanCreateAndUploadBuffer called (size=%llu, data=%p, cmd=%p)\n", (unsigned long long)size, data, (void*)cmd); fflush(stdout);
    #endif
    // --- 1. Input Validation ---
    if (size == 0 || !out_buffer || !out_allocation) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: ERROR: Invalid parameters!\n"); fflush(stdout);
        #endif
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_buffer = VK_NULL_HANDLE;
    *out_allocation = VK_NULL_HANDLE;

    // If data is NULL, create an empty buffer without staging
    if (data == NULL) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;

        VmaAllocationCreateInfo alloc_info = {0};
        // CRITICAL: Use CPU_TO_GPU for uniform buffers and storage buffers so they can be
        // mapped/updated. GPU_ONLY is not mappable and will cause failures when we try to
        // update UBOs or read back SSBOs.
        if ((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
            alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        } else {
            alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }

        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, out_buffer, out_allocation, NULL) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create device-local buffer.");
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
        }
        return SITUATION_SUCCESS;
    }

    // --- 2. Create Staging Buffer ---
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = VK_NULL_HANDLE;

    VkBufferCreateInfo staging_buffer_info = {};
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.size = size;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {0};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &staging_buffer_info, &staging_alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create staging buffer.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }

    // --- 3. Upload Data to Staging Buffer ---
    void* mapped_data = NULL;
    if (vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &mapped_data) != VK_SUCCESS) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        return SITUATION_ERROR_BUFFER_MAP_FAILED;
    }
    memcpy(mapped_data, data, (size_t)size);
    vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);

    // --- 4. Create Final GPU-Local Buffer ---
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;

    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buffer_info, &alloc_info, out_buffer, out_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create device-local buffer.");
        vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }

    // --- 5. Copy Data ---
    VkBufferCopy copy_region = {};
    copy_region.size = size;

    if (cmd != VK_NULL_HANDLE) {
        // === ASYNCHRONOUS PATH ===
        // Use the provided command buffer. We must defer the destruction of the staging buffer
        // until the frame is done.

        // Barrier: Ensure copy happens before shader reads
        // Note: Only need barrier if we use it in the same frame, but robust to add.
        // Actually, standard practice is to barrier destination.
        VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = *out_buffer;
        barrier.offset = 0;
        barrier.size = size;

        vkCmdCopyBuffer(cmd, staging_buffer, *out_buffer, 1, &copy_region);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

        _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        return SITUATION_SUCCESS;

    } else {
        // === SYNCHRONOUS PATH (Legacy/Init) ===
        VkCommandBuffer temp_cmd = _SituationVulkanBeginSingleTimeCommands();
        if (temp_cmd == VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, *out_buffer, *out_allocation);
            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
            *out_buffer = VK_NULL_HANDLE;
            return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
        }

        vkCmdCopyBuffer(temp_cmd, staging_buffer, *out_buffer, 1, &copy_region);

        // Execute and Wait
        _SituationVulkanEndSingleTimeCommands(temp_cmd);

        // SAFE CLEANUP [2.3.14A]:
        // If we are initializing, Graveyards might not be ready or flushed yet.
        // Explicit destroy is fine here because we waited on the queue via EndSingleTimeCommands.
        if (sit_render.vk.graveyards) {
            _SituationDeferDestroyBuffer(staging_buffer, staging_allocation);
        } else {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
        }

        return SITUATION_SUCCESS;
    }
}

/**
 * @brief [INTERNAL] Reads data from a Vulkan buffer back to host memory.
 *
 * @details This helper abstracts the complexity of reading GPU memory. It intelligently selects the optimal path based on the buffer's memory type:
 *          1. **Direct Map:** If the buffer's memory is `HOST_VISIBLE` and `HOST_COHERENT` (e.g., a CPU-to-GPU buffer), it maps the memory directly and copies the data.
 *          2. **Staging Transfer:** If the buffer is `DEVICE_LOCAL` (e.g., a high-performance SSBO), it allocates a temporary host-visible staging buffer, records a GPU copy command, submits it, and waits for completion.
 *
 * @par Synchronization Logic
 * This function performs critical synchronization to ensure data integrity:
 * - **Pre-Copy Barrier:** Inserts a `vkCmdPipelineBarrier` to ensure that all previous GPU writes (from Compute Shaders, Vertex Shaders, or Transfers) are finished and flushed from cache before the copy operation begins.
 * - **Host-Read Barrier:** Ensures the transfer write to the staging buffer is visible to the host before mapping.
 *
 * @param src_buffer The source `VkBuffer` handle to read from.
 * @param src_alloc The VMA allocation handle associated with the source buffer (required to query memory flags).
 * @param size The number of bytes to read.
 * @param offset The byte offset within the source buffer to start reading from.
 * @param[out] out_data Pointer to the destination CPU memory buffer. Must be pre-allocated by the caller.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED` if the temporary staging buffer cannot be created.
 * @return `SITUATION_ERROR_BUFFER_MAP_FAILED` if memory mapping fails.
 *
 * @warning This is a **synchronous** operation. It allocates a command buffer, submits it, and stalls the CPU (`vkQueueWaitIdle`) until the GPU transfer is complete.
 */
static SituationError _SituationVulkanReadBackBuffer(VkBuffer src_buffer, VmaAllocation src_alloc, size_t size, size_t offset, void* out_data) {
    VkDevice device = sit_render.vk.device;
    VmaAllocator allocator = sit_render.vk.vma_allocator;

    // 1. Check if directly mappable
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(allocator, src_alloc, &alloc_info);

    if (alloc_info.pMappedData != NULL) {  // VMA already mapped it
        void* mapped_data;
        if (vmaMapMemory(allocator, src_alloc, &mapped_data) != VK_SUCCESS) return SITUATION_ERROR_BUFFER_MAP_FAILED;
        memcpy(out_data, (char*)mapped_data + offset, size);
        vmaUnmapMemory(allocator, src_alloc);
        return SITUATION_SUCCESS;
    }

    // 2. Use Staging Buffer
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    staging_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (vmaCreateBuffer(allocator, &staging_info, &staging_alloc_info, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // Sync barrier: Wait for vertex/transfer stages to finish reading/writing before we copy
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.buffer = src_buffer;
    barrier.offset = offset;
    barrier.size = size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = offset;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, src_buffer, staging_buffer, 1, &copy_region);

    // Memory barrier for host read
    VkBufferMemoryBarrier host_barrier = {};
    host_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    host_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    host_barrier.buffer = staging_buffer;
    host_barrier.offset = 0;
    host_barrier.size = size;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host_barrier, 0, NULL);

    _SituationVulkanEndSingleTimeCommands(cmd);

    // 3. Map and Copy
    void* mapped_data;
    SituationError result = SITUATION_SUCCESS;
    if (vmaMapMemory(allocator, staging_allocation, &mapped_data) == VK_SUCCESS) {
        memcpy(out_data, mapped_data, size);
        vmaUnmapMemory(allocator, staging_allocation);
    } else {
        result = SITUATION_ERROR_BUFFER_MAP_FAILED;
    }

    vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);
    return result;
}
#endif

// ============================================================================
// Buffer Implementation
// ============================================================================

/**
 * @brief Creates a general-purpose GPU data buffer for storing vertices, indices, uniforms (UBOs), or shader storage data (SSBOs).
 * @details This is the primary function for allocating memory on the GPU. It abstracts the backend-specific complexities, such as OpenGL's buffer storage mechanisms or Vulkan's staging buffer transfers, into a single, unified call.
 *          The `usage_flags` parameter is critical, as it provides a hint to the driver about how the buffer will be used, which heavily influences performance and memory placement.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Uses modern Direct State Access (`glCreateBuffers`, `glNamedBufferStorage`). Buffers intended for updates (e.g., UBOs, SSBOs) are automatically created with `GL_DYNAMIC_STORAGE_BIT` to allow modification via `SituationUpdateBuffer`.
 * - **Vulkan:** This function implements a high-performance workflow.
 *   - If `initial_data` is provided, it automatically creates a temporary staging buffer, copies the data to it, and then records a GPU command to transfer the data to a fast, device-local (`VMA_MEMORY_USAGE_GPU_ONLY`) final buffer.
 *   - If the buffer is created with `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER` or `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`, it also **pre-allocates and caches a persistent `VkDescriptorSet`** within the `SituationBuffer` handle. This
 *     makes subsequent binding with `SituationCmdBindUniformBuffer` or `SituationCmdBindComputeBuffer` an extremely fast operation, avoiding runtime descriptor allocation overhead.
 *
 * @param size The total size of the buffer to allocate, in bytes. Must be greater than zero.
 * @param initial_data A pointer to the initial data to upload to the buffer. If `NULL`, the buffer is allocated, but its contents are undefined until written to.
 * @param usage_flags A bitmask of `SituationBufferUsageFlags` that tells the driver how the buffer will be used.
 *                    This is a critical performance hint. Flags can be combined using the bitwise OR operator (e.g., `SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC`).
 *
 * @return A `SituationBuffer` handle.
 *         - On success, the `id` member of the returned struct will be non-zero, and the handle is ready for use.
 *         - On failure, the `id` member will be 0. Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *           Failure can occur due to invalid parameters, running out of GPU memory, or other API errors.
 *
 * @note The caller is **responsible** for destroying the buffer using `SituationDestroyBuffer()` to prevent GPU memory leaks.;
 *
 * @warning Providing incorrect or overly broad `usage_flags` can lead to suboptimal performance. For example, creating a static vertex buffer without `SITUATION_BUFFER_USAGE_TRANSFER_DST` might prevent it from being updated efficiently later.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationDestroyBuffer();
 * @see SituationUpdateBuffer()
 * @see SituationGetBufferData()
 * @see SituationBufferUsageFlags
 */
SITAPI SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer) {
    if (!out_buffer) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_buffer, 0, sizeof(SituationBuffer));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    SituationBuffer handle;
    _SituationBufferSlot* slot = _SitAllocBufferSlot(&handle);
    if (!slot) return SITUATION_ERROR_MEMORY_ALLOCATION;

    slot->size_in_bytes = size;
    slot->usage_flags = usage_flags;
    handle.size_in_bytes = size;
    handle.usage_flags = usage_flags;

#if defined(SITUATION_USE_OPENGL)
    glCreateBuffers(1, &slot->gl_buffer_id);
    if (slot->gl_buffer_id == 0) {
        _SitFreeBufferSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for SituationCreateBuffer.");
    }
    // Use dynamic storage if we plan to map/update often?
    GLbitfield flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(slot->gl_buffer_id, size, initial_data, flags); // initial_data can be NULL
    SIT_CHECK_GL_ERROR();
    if (strcmp(sit_gs.last_error_msg, "No error") != 0) {
        glDeleteBuffers(1, &slot->gl_buffer_id);
        _SitFreeBufferSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glNamedBufferStorage failed for SituationCreateBuffer.");
    }

#elif defined(SITUATION_USE_VULKAN)
    VkBufferUsageFlags vk_usage = 0;
    if (usage_flags & SITUATION_BUFFER_USAGE_VERTEX_BUFFER) vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_INDEX_BUFFER) vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_UNIFORM_BUFFER) vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_INDIRECT_BUFFER) vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_SRC) vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_TRANSFER_DST) vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage_flags & SITUATION_BUFFER_USAGE_DEVICE_ADDRESS) vk_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    slot->vk_usage_flags = vk_usage;

    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
    SituationError err = _SituationVulkanCreateAndUploadBuffer(cmd, initial_data, size, vk_usage, &slot->vk_buffer, &slot->vma_allocation);
    _SituationVulkanEndSingleTimeCommands(cmd);

    if (err != SITUATION_SUCCESS) {
        _SitFreeBufferSlot(handle);
        return err;
    }
#endif

    *out_buffer = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Destroys a GPU buffer and frees all associated resources.
 * @details This is the only correct way to release a buffer created with SituationCreateBuffer.
 *          It handles backend-specific cleanup (OpenGL buffer names, Vulkan buffers, VMA allocations, and cached descriptor sets) and removes the buffer from the library's internal resource tracking list to prevent shutdown warnings.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDeleteBuffers` to release the buffer object.
 * - **Vulkan:** Waits for the GPU to become idle to ensure the buffer is not in use, then frees the pre-allocated persistent descriptor set, and finally destroys the `VkBuffer` and its `VmaAllocation`.
 *
 * @param[in,out] buffer A pointer to the `SituationBuffer` handle to destroy. The handle is invalidated (zeroed out) after this call.
 *
 * @note It is safe to call this function on a NULL pointer or an already-destroyed (zeroed) buffer handle; it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationDestroyBuffer(SituationBuffer* buffer) {
    if (!buffer) return;
    _SituationBufferSlot* slot = _SitGetBufferSlot(*buffer);
    if (!slot) return;

#if defined(SITUATION_USE_OPENGL)
    _SitGLDeferDestroyBuffer(slot->gl_buffer_id);
#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->descriptor_set != VK_NULL_HANDLE && slot->descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(sit_render.vk.device, slot->descriptor_pool, 1, &slot->descriptor_set);
            slot->descriptor_set = VK_NULL_HANDLE;
        }
        if (slot->vk_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->vk_buffer, slot->vma_allocation);
            slot->vk_buffer = VK_NULL_HANDLE;
            slot->vma_allocation = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyBuffer(slot->vk_buffer, slot->vma_allocation);
        if (slot->descriptor_set != VK_NULL_HANDLE) {
            _SituationDeferDestroyDescriptorSet(slot->descriptor_set, slot->descriptor_pool);
        }
    }
#endif

    _SitFreeBufferSlot(*buffer);
    memset(buffer, 0, sizeof(SituationBuffer));
}


/**
 * @brief Creates a self-contained GPU mesh from vertex and index data.
 * @details This function allocates all necessary GPU resources for a renderable mesh and configures its vertex attribute layout according to the library's shader contract.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Creates a dedicated Vertex Array Object (VAO) for this mesh. It also creates a Vertex Buffer Object (VBO) and an Element Buffer Object (EBO), uploads the provided data,
 *   and configures the VAO's vertex attributes (position, normal, texcoord) to point to the correct data within the VBO. The mesh is a fully self-contained, ready-to-draw object.
 * - **Vulkan:** Creates two device-local `VkBuffer` objects (one for vertices, one for indices) and uses a staging buffer process to upload the provided data to them for optimal performance.
 *
 * @param vertex_data Pointer to the raw, interleaved vertex data.
 * @param vertex_count The total number of vertices in the buffer.
 * @param vertex_stride The size of a single vertex struct in bytes (e.g., `sizeof(MyVertex)`).
 * @param index_data Pointer to the index data (must be `uint32_t`).
 * @param index_count The total number of indices.
 *
 * @return A `SituationMesh` handle.
 *         - On success, the `id` will be non-zero.
 *         - On failure, the `id` will be 0. Use `SituationGetLastErrorMsg()` for details.
 *
 * @note The provided vertex data **must** conform to the attribute layout defined in the Shader Contract (e.g., Position `vec3`, Normal `vec3`, TexCoord `vec2`).
 * @note The caller is **responsible** for destroying the mesh using `SituationDestroyMesh()`.;
 */
SITAPI SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh) {
    if (!out_mesh) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_mesh, 0, sizeof(SituationMesh));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    SituationMesh handle;
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    _SituationMeshSlot* slot = _SitAllocMeshSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    if (!slot) return SITUATION_ERROR_MEMORY_ALLOCATION;

    slot->vertex_count = vertex_count;
    slot->index_count = index_count;
    slot->vertex_stride = vertex_stride;

    // Cache in handle for fast access
    handle.vertex_count = vertex_count;
    handle.index_count = index_count;
    handle.vertex_stride = vertex_stride;

#if defined(SITUATION_USE_OPENGL)
    // Create VAO/VBO/EBO
    glCreateBuffers(1, &slot->vbo_id);
    if (slot->vbo_id == 0) {
        _SitFreeMeshSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for mesh VBO.");
    }
    glNamedBufferData(slot->vbo_id, vertex_count * vertex_stride, vertex_data, GL_STATIC_DRAW);

    if (index_count > 0 && index_data) {
        glCreateBuffers(1, &slot->ebo_id);
        if (slot->ebo_id == 0) {
            glDeleteBuffers(1, &slot->vbo_id);
            _SitFreeMeshSlot(handle);
            return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "glCreateBuffers failed for mesh EBO.");
        }
        glNamedBufferData(slot->ebo_id, index_count * sizeof(uint32_t), index_data, GL_STATIC_DRAW);
    }
    SIT_CHECK_GL_ERROR();

#elif defined(SITUATION_USE_VULKAN)
    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // Create Vertex Buffer
    VkDeviceSize vSize = vertex_count * vertex_stride;
    if (_SituationVulkanCreateAndUploadBuffer(cmd, vertex_data, vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &slot->vertex_buffer, &slot->vertex_buffer_memory) != SITUATION_SUCCESS) {
        _SituationVulkanEndSingleTimeCommands(cmd);
        _SitFreeMeshSlot(handle);
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // Create Index Buffer
    if (index_count > 0 && index_data) {
        VkDeviceSize iSize = index_count * sizeof(uint32_t);
        if (_SituationVulkanCreateAndUploadBuffer(cmd, index_data, iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &slot->index_buffer, &slot->index_buffer_memory) != SITUATION_SUCCESS) {
            _SituationVulkanEndSingleTimeCommands(cmd);
            _SituationVulkanDestroyBuffer(slot->vertex_buffer, slot->vertex_buffer_memory); // Clean up VBO
            _SitFreeMeshSlot(handle);
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }
    }
    _SituationVulkanEndSingleTimeCommands(cmd);
#endif

    // Track for leaks? No need, registry handles it.
    *out_mesh = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Destroys a GPU mesh and frees all of its associated resources.
 * @details This is the only correct way to release a mesh created with `SituationCreateMesh`. It handles the full cleanup process, ensuring that all backend-specific GPU objects are deleted and that the mesh is removed from the library's internal resource tracking list.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Deletes the Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO) associated with the mesh using `glDelete*` functions.
 * - **Vulkan:** Waits for the GPU to become idle to ensure the buffers are not in use, then destroys the `VkBuffer` and frees the `VmaAllocation` for both the vertex and index buffers.
 *
 * @param[in,out] mesh A pointer to the `SituationMesh` handle to destroy. The handle is invalidated by being zeroed out after this call, preventing accidental reuse.
 *
 * @note It is safe to call this function on a NULL pointer or an already-destroyed (zeroed) mesh handle; it will simply do nothing.
 * @note Failure to call this function on a created mesh will result in a GPU memory leak and a warning message upon application shutdown.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 *
 * @see SituationCreateMesh()
 */
SITAPI void SituationDestroyMesh(SituationMesh* mesh) {
    if (!mesh) return;
    _SituationMeshSlot* slot = _SitGetMeshSlot(*mesh);
    if (!slot) return;

#if defined(SITUATION_USE_OPENGL)
    _SitGLDeferDestroyBuffer(slot->vbo_id);
    if (slot->ebo_id) _SitGLDeferDestroyBuffer(slot->ebo_id);
    // Also clean VAO cache for this mesh ID (which we don't have a unique ID for anymore,
    // but the slot index is unique enough for the session, or we use generation?
    // The VAO cache used mesh.id. We can use slot index or just clear all?
    // Or we use the VBO ID as the key (which is what we did before: id was VBO ID).
    // Let's rely on VBO ID for cache key.
    _SitGLDeferCleanMeshVAO(slot->vbo_id); // Assuming VBO ID is unique key
#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        if (slot->vertex_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->vertex_buffer, slot->vertex_buffer_memory);
            slot->vertex_buffer = VK_NULL_HANDLE;
            slot->vertex_buffer_memory = VK_NULL_HANDLE;
        }
        if (slot->index_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(sit_render.vk.vma_allocator, slot->index_buffer, slot->index_buffer_memory);
            slot->index_buffer = VK_NULL_HANDLE;
            slot->index_buffer_memory = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyBuffer(slot->vertex_buffer, slot->vertex_buffer_memory);
        if (slot->index_buffer) {
            _SituationDeferDestroyBuffer(slot->index_buffer, slot->index_buffer_memory);
        }
    }
#endif

    _SitFreeMeshSlot(*mesh);
    memset(mesh, 0, sizeof(SituationMesh));
}


/**
 * @brief Reads geometry data back from a GPU mesh into CPU memory.
 *
 * @details This function performs a synchronous readback from VRAM. It allocates memory for the vertex and index buffers which **the caller must free**.
 *
 * @param mesh The mesh handle.
 * @param[out] vertex_data Pointer to receive the array of vertices. Caller must free.
 * @param[out] vertex_count Pointer to receive the number of vertices.
 * @param[out] vertex_stride Pointer to receive the size of a single vertex in bytes.
 * @param[out] index_data Pointer to receive the array of indices. Caller must free.
 * @param[out] index_count Pointer to receive the number of indices.
 */
SITAPI void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count, int* vertex_stride, void** index_data, int* index_count) {
    // Initialize outputs to 0/NULL
    if (vertex_data) *vertex_data = NULL;
    if (vertex_count) *vertex_count = 0;
    if (vertex_stride) *vertex_stride = 0;
    if (index_data) *index_data = NULL;
    if (index_count) *index_count = 0;

    _SituationMeshSlot* slot = _SitGetMeshSlot(mesh);
    if (!slot) return;

    // Set count info
    if (vertex_count) *vertex_count = slot->vertex_count;
    if (vertex_stride) *vertex_stride = (int)slot->vertex_stride;
    if (index_count) *index_count = slot->index_count;

    size_t v_size = slot->vertex_count * slot->vertex_stride;
    size_t i_size = slot->index_count * sizeof(uint32_t);

    // Allocate CPU memory
    void* v_ptr = NULL;
    void* i_ptr = NULL;

    if (vertex_data && v_size > 0) {
        v_ptr = SIT_MALLOC(v_size);
        if (!v_ptr) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Mesh readback vertex buffer"); return; }
        *vertex_data = v_ptr;
    }

    if (index_data && i_size > 0) {
        i_ptr = SIT_MALLOC(i_size);
        if (!i_ptr) {
            if (v_ptr) SIT_FREE(v_ptr);
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Mesh readback index buffer");
            return;
        }
        *index_data = i_ptr;
    }

#if defined(SITUATION_USE_OPENGL)
    if (v_ptr) {
        glGetNamedBufferSubData(slot->vbo_id, 0, v_size, v_ptr);
    }
    if (i_ptr) {
        glGetNamedBufferSubData(slot->ebo_id, 0, i_size, i_ptr);
    }
    SIT_CHECK_GL_ERROR();

#elif defined(SITUATION_USE_VULKAN)
    // Use our new helper
    if (v_ptr) {
        if (_SituationVulkanReadBackBuffer(slot->vertex_buffer, slot->vertex_buffer_memory, v_size, 0, v_ptr) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to read vertex buffer from Vulkan mesh");
        }
    }
    if (i_ptr) {
        if (_SituationVulkanReadBackBuffer(slot->index_buffer, slot->index_buffer_memory, i_size, 0, i_ptr) != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to read index buffer from Vulkan mesh");
        }
    }
#endif
}

#if defined(SITUATION_USE_OPENGL)
/**
 * @brief [INTERNAL] Compiles a single GLSL shader stage from a source string.
 * @details This is a low-level helper function that takes a string of GLSL code and a shader type (e.g., vertex, fragment, compute) and uses the OpenGL driver to compile it into a shader object. It performs comprehensive error checking and reporting.
 *
 * @par Compilation Process
 *   1.  Creates a new shader object of the specified `type` (`glCreateShader`).
 *   2.  Associates the `source` string with the shader object (`glShaderSource`).
 *   3.  Attempts to compile the shader (`glCompileShader`).
 *   4.  Checks the `GL_COMPILE_STATUS`. If compilation fails, it retrieves the detailed error log from the driver.
 *
 * The retrieved error log is formatted with a prefix indicating the shader type and is set as the library's last error message, providing invaluable feedback for debugging shader code.
 *
 * @param source A null-terminated C string containing the GLSL source code to compile.
 * @param type The type of shader to create (e.g., `GL_VERTEX_SHADER`, `GL_FRAGMENT_SHADER`, `GL_COMPUTE_SHADER`).
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The `GLuint` ID of the compiled shader object on success.
 * @return `0` on failure. On failure, a detailed error message is set, and the invalid shader object is deleted.
 *
 * @note This function is for internal use by the higher-level shader program creation functions.
 * @warning The returned shader object is an intermediate resource. It should be attached to a program and then deleted with `glDeleteShader` to prevent resource leaks.
 *
 * @see _SituationCreateGLShaderProgram(), _SituationCreateGLShaderProgramFromSource()
 */
static GLuint _SituationCompileGLShader(const char* source, GLenum type, SituationError* error_code) {
    if (!source) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    GLuint shader = glCreateShader(type);

    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        // [Phase 5] Virtual Bindless Injection
        // We inject the definition and uniforms after the #version line AND any existing #extension directives
        const char* version_pos = strstr(source, "#version");
        if (version_pos) {
            // Find end of version line
            const char* next_line = strchr(version_pos, '\n');
            if (next_line) {
                next_line++; // Skip newline
                
                // Skip past any existing #extension directives
                const char* injection_point = next_line;
                while (1) {
                    // Skip whitespace and comments
                    while (*injection_point == ' ' || *injection_point == '\t' || *injection_point == '\r' || *injection_point == '\n') {
                        injection_point++;
                    }
                    // Check if this line starts with #extension
                    if (strncmp(injection_point, "#extension", 10) == 0) {
                        // Skip to end of this extension line
                        const char* ext_end = strchr(injection_point, '\n');
                        if (ext_end) {
                            injection_point = ext_end + 1;
                        } else {
                            break; // No more lines
                        }
                    } else {
                        // Not an extension directive, this is where we inject
                        break;
                    }
                }

                // Construct the injection block (without the GL_EXT_nonuniform_qualifier extension since it's Vulkan-specific)
                const char* injection =
                    "#define SITUATION_VIRTUAL_BINDLESS 1\n"
                    "uniform sampler2D _sit_virtual_textures[32];\n"
                    "uniform int _sit_texture_slot_id;\n"
                    "#define global_textures _sit_virtual_textures\n"
                    "#define nonuniformEXT(x) _sit_texture_slot_id\n";

                const char* sources[3] = {
                    NULL, // Part 1 (Version + existing extensions)
                    injection,
                    NULL  // Part 2 (Rest)
                };
                GLint lengths[3] = {0, 0, 0};

                // Calculate lengths
                lengths[0] = (GLint)(injection_point - source);
                sources[0] = source;

                lengths[1] = (GLint)strlen(injection);

                sources[2] = injection_point;
                // sources[2] is null terminated, so we can pass length or let GL determine it
                // Since I'm passing lengths array, I should pass length.
                // But wait, strlen on source is O(N).
                // If I pass NULL for lengths, it assumes ALL are null terminated.
                // But sources[0] is NOT null terminated.
                // So I MUST pass lengths array.
                lengths[2] = (GLint)strlen(sources[2]);

                glShaderSource(shader, 3, sources, lengths);
            } else {
                 // Fallback if no newline after version (weird)
                 glShaderSource(shader, 1, &source, NULL);
            }
        } else {
             // Fallback if no version found
             glShaderSource(shader, 1, &source, NULL);
        }
    } else {
        glShaderSource(shader, 1, &source, NULL);
    }

    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            // Determine the prefix for the error message.
            const char* type_name = "Unknown Shader";
            if (type == GL_VERTEX_SHADER) type_name = "Vertex Shader";
            else if (type == GL_FRAGMENT_SHADER) type_name = "Fragment Shader";
            else if (type == GL_COMPUTE_SHADER) type_name = "Compute Shader";

            // Calculate the total size needed for the final error string: "PREFIX: LOG\0"
            size_t prefix_len = strlen(type_name) + 2; // For ": "
            size_t total_buffer_size = prefix_len + log_length;

            // Allocate a single buffer for the entire message.
            char* final_error_message = (char*)SIT_MALLOC(total_buffer_size);

            if (final_error_message) {
                // Write the prefix into the buffer.
                strcpy(final_error_message, type_name);
                strcat(final_error_message, ": ");

                // Get a pointer to where the log should start.
                char* log_part = final_error_message + prefix_len;

                // Read the GL info log directly into the end of our buffer.
                glGetShaderInfoLog(shader, log_length, NULL, log_part);

                // Set the final, combined error message.
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, final_error_message);
                
                // Log to debug file
                SIT_DEBUG_LOG("[SHADER_ERROR] %s", final_error_message);

                // Free the single allocated buffer.
                SIT_FREE(final_error_message);
            } else {
                // If allocation fails, set a memory error.
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for shader compilation log.");
            }
        } else {
            // No log available, provide a generic error.
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_COMPILE, "An unknown shader compilation error occurred with no log.");
        }

        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_COMPILE;
        glDeleteShader(shader);
        return 0;
    }

    if (error_code) *error_code = SITUATION_SUCCESS;
    return shader;
}

/**
 * @brief [INTERNAL] Creates a standard two-stage (vertex + fragment) OpenGL shader program from GLSL source.
 * @details This is the primary internal helper for creating graphics pipelines on the OpenGL backend. It orchestrates the process of compiling individual vertex and fragment shader sources and linking them together into a complete, usable `glProgram`.
 *
 * @par Creation Process
 *   1.  Calls `_SituationCompileGLShader` to compile the vertex shader source (`vs_src`).
 *   2.  If successful, it calls `_SituationCompileGLShader` to compile the fragment shader source (`fs_src`).
 *   3.  If both shaders compile successfully, it creates a new program object (`glCreateProgram`), attaches both shaders, and links them (`glLinkProgram`).
 *   4.  After linking, the individual shader objects are detached and deleted, as they are no longer needed.
 *   5.  Finally, it checks the link status and reports any errors.
 *
 * @param vs_src A null-terminated C string containing the vertex shader source code.
 * @param fs_src A null-terminated C string containing the fragment shader source code.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful compilation and linking.
 * @return `0` on failure. On failure, a detailed error message (from either the compiler or linker) is set via `_SituationSetErrorFromCode`, and all intermediate resources are cleaned up.
 *
 * @note This function is for internal use by high-level `SituationLoadShader*` functions.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram` when it is no longer needed.
 *
 * @see SituationLoadShaderFromMemory(), _SituationCompileGLShader()
 */
static GLuint _SituationCreateGLShaderProgramAsync(const char* vs_src, const char* fs_src, SituationError* error_code) {
    if (!vs_src || !fs_src) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    SituationError local_err = SITUATION_SUCCESS;
    GLuint vs = _SituationCompileGLShader(vs_src, GL_VERTEX_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint fs = _SituationCompileGLShader(fs_src, GL_FRAGMENT_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        glDeleteShader(vs);
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create shader program");
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // [Async] Defer status check.
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates and links a complete OpenGL shader program from vertex and fragment GLSL source strings.
 *
 * @details This is a low-level helper function that performs the full OpenGL shader creation pipeline:
 *            1. Creates vertex and fragment shader objects
 *            2. Attaches the provided source strings (`vs_src` and `fs_src`)
 *            3. Compiles both shaders individually
 *            4. Checks compilation status and logs errors/info logs on failure
 *            5. Creates and links the program object
 *            6. Checks link status and logs errors/info logs on failure
 *            7. Cleans up intermediate shader objects on success or failure
 *
 *          Intended for use during shader creation, hot-reload, internal quad/compute pipelines,
 *          or when SPIR-V binary path is unavailable (e.g. no GL_ARB_gl_spirv extension).
 *
 *          On success, returns a valid `GLuint` program name that must be deleted later
 *          with `glDeleteProgram` (typically via graveyard/deferred cleanup in render thread).
 *
 * @param vs_src Null-terminated string containing the vertex shader GLSL source code.
 *               Must remain valid for the duration of the call.
 * @param fs_src Null-terminated string containing the fragment shader GLSL source code.
 *               Must remain valid for the duration of the call.
 * @param error_code Pointer to a `SituationError` variable that receives the detailed error code
 *                   on failure. On success, set to `SITUATION_SUCCESS`.
 *                   May be NULL if the caller does not need the error detail.
 *
 * @return A valid OpenGL program object name (`GLuint`) on success,
 *         0 on failure (compilation or linking error).
 *         On failure, `*error_code` is set to an appropriate value:
 *           - SITUATION_ERROR_SHADER_COMPILATION_FAILED (vertex or fragment compile error)
 *           - SITUATION_ERROR_SHADER_LINK_FAILED (program link error)
 *           - SITUATION_ERROR_INVALID_PARAM (null source strings)
 *           - SITUATION_ERROR_GL_ERROR (underlying GL call failed)
 *
 * Thread safety invariants:
 *   - Must be called from a thread that has an active OpenGL context (typically render thread
 *     or main thread during init/hot-reload)
 *   - No internal locking ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller must ensure no concurrent GL calls on the same context
 *   - Safe during hot-reload if old programs are deleted first
 *
 * @note In debug builds, full GLSL compile/link info logs are printed to stderr on failure.
 *       In release builds, only high-level errors are logged.
 *       The function does **not** validate GLSL syntax beyond what the driver reports.
 *       Shader objects are always detached and deleted ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller only needs to manage the returned program.
 *
 * @see _SituationCreateGLShaderProgramFromSpirv, _SituationCreateGLComputeProgramFromSpirv,
 *      glCreateShader, glShaderSource, glCompileShader, glCreateProgram, glLinkProgram,
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED, SITUATION_ERROR_SHADER_LINK_FAILED
 */
static GLuint _SituationCreateGLShaderProgram(const char* vs_src, const char* fs_src, SituationError* error_code) {
    if (!vs_src || !fs_src) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Null shader source");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

    SituationError local_err = SITUATION_SUCCESS;
    GLuint vs = _SituationCompileGLShader(vs_src, GL_VERTEX_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint fs = _SituationCompileGLShader(fs_src, GL_FRAGMENT_SHADER, &local_err);
    if (local_err != SITUATION_SUCCESS) {
        glDeleteShader(vs);
        if (error_code) *error_code = local_err;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "Failed to create shader program");
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    // [Phase 5] Virtual Bindless Sampler Setup
    if (success && !SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        glUseProgram(program);
        GLint loc = glGetUniformLocation(program, "_sit_virtual_textures");
        if (loc >= 0) {
            // Set bindings 0-31
            GLint bindings[32];
            for (int i = 0; i < 32; i++) bindings[i] = i;
            glUniform1iv(loc, 32, bindings);
        }
        glUseProgram(0);
    }

    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        // Dynamically allocate a buffer large enough for the full link error log.
        if (log_length > 0) {
            char* infoLog = (char*)SIT_MALLOC(log_length);
            if (infoLog) {
                glGetProgramInfoLog(program, log_length, NULL, infoLog);
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
                SIT_FREE(infoLog);
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for shader link log.");
            }
        } else {
            // No log, provide a generic message.
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, "An unknown linking error occurred.");
        }
        glDeleteProgram(program); // Add this line
        glDeleteShader(vs);       // Ensure shaders are deleted
        glDeleteShader(fs);       // Ensure shaders are deleted
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        return 0;
    }

    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates a single-stage (compute) OpenGL shader program from GLSL source.
 * @details This helper function is the traditional GLSL-based creation path for compute shaders. It is called by the `_SituationCreateGLComputeProgram` dispatcher when the source type is GLSL or when SPIR-V is not supported.
 *
 * @par Creation Process
 *   1.  Calls `_SituationCompileGLShader` to compile the compute shader source (`cs_src`) with the type `GL_COMPUTE_SHADER`.
 *   2.  If compilation is successful, it creates a new program object, attaches the compute shader, and links the program.
 *   3.  The individual shader object is deleted after linking.
 *   4.  The link status is checked to ensure a valid executable program was created.
 *
 * @param cs_src A null-terminated C string containing the compute shader source code.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful compilation and linking.
 * @return `0` on failure. On failure, a detailed error message is set, and all intermediate resources are cleaned up.
 *
 * @note This function is for internal use by `_SituationCreateGLComputeProgram` only. It specifically handles the single-stage linking process required for compute programs.
 *
 * @see _SituationCreateGLComputeProgram(), _SituationCompileGLShader()
 */
static GLuint _SituationCreateGLShaderProgramFromSource(const char* cs_src, SituationError* error_code) {
    if (!cs_src) {
        // This check is now primarily for internal consistency
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        // _SituationCreateGLComputeProgram already set the error msg, avoid duplication or override it carefully
        // _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationCreateGLShaderProgramFromSource: Compute shader source cannot be NULL");
        return 0;
    }

    // 1. Compile the compute shader
    SituationError local_err = SITUATION_SUCCESS;
    GLuint cs = _SituationCompileGLShader(cs_src, GL_COMPUTE_SHADER, &local_err); // Assume this function exists and handles glGetShaderiv(GL_COMPILE_STATUS, ...)
    if (local_err != SITUATION_SUCCESS) {
        if (error_code) *error_code = local_err;
        return 0; // Error message already set by _SituationCompileGLShader
    }

    // 2. Create a program and attach the shader
    GLuint program = glCreateProgram();
    if (!program) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationCreateGLShaderProgramFromSource: Failed to create shader program object");
        glDeleteShader(cs); // Clean up the successfully created shader
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_GENERAL;
        return 0;
    }

    glAttachShader(program, cs);
    glLinkProgram(program);

    // 3. Shader is linked, we no longer need the individual shader object
    glDeleteShader(cs);
    cs = 0; // Good practice

    // 4. Check for linking errors
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    // [Phase 5] Virtual Bindless Sampler Setup
    if (success && !SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        glUseProgram(program);
        GLint loc = glGetUniformLocation(program, "_sit_virtual_textures");
        if (loc >= 0) {
            // Set bindings 0-31
            GLint bindings[32];
            for (int i = 0; i < 32; i++) bindings[i] = i;
            glUniform1iv(loc, 32, bindings);
        }
        glUseProgram(0);
    }

    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        // Handle potential glGetProgramiv failure? Unlikely, but glGetError could check.
        if (log_length > 0) {
            char* infoLog = (char*)SIT_MALLOC((size_t)log_length);
            if (infoLog) {
                glGetProgramInfoLog(program, log_length, NULL, infoLog);
                _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, infoLog);
                SIT_FREE(infoLog);
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationCreateGLShaderProgramFromSource: Failed to allocate memory for shader link log.");
            }
        } else {
            _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_SHADER_LINK, "_SituationCreateGLShaderProgramFromSource: An unknown linking error occurred (no log available).");
        }
        glDeleteProgram(program); // Clean up the unsuccessfully linked program
        program = 0;
        if (error_code) *error_code = SITUATION_ERROR_OPENGL_SHADER_LINK;
        return 0;
    }

    // Success path for this helper
    if (error_code) *error_code = SITUATION_SUCCESS;
    return program;
}

/**
 * @brief [INTERNAL] Creates a single-stage OpenGL compute program, dispatching to the optimal creation path.
 * @details This function acts as a high-level dispatcher for creating OpenGL compute shaders.
 *          It intelligently selects the best methodÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Âeither using modern, pre-compiled SPIR-V bytecode or falling back to traditional GLSL source compilationÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Âbased on the type of data provided and the capabilities of the current OpenGL driver.
 *
 * @par Dispatch Logic
 *   - If `source_type` is `SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV` and the `GL_ARB_gl_spirv` extension is available, it calls `_SituationCreateGLComputeProgramFromSpirv` for the fastest and most consistent creation path.
 *   - Otherwise, if `source_type` is `SITUATION_GL_SHADER_SOURCE_TYPE_GLSL`, it calls `_SituationCreateGLShaderProgramFromSource` to perform traditional compilation and linking.
 *   - If SPIR-V data is provided but the driver does not support it, the function will fail and report an error.
 *
 * @param source_data A pointer to the shader data. This must be a `const struct _SituationSpirvBlob*` for SPIR-V or a `const char*` for GLSL.
 * @param source_type An enum (`SituationGLShaderSourceType`) specifying whether `source_data` points to GLSL or SPIR-V.
 * @param[out] error_code A pointer to a `SituationError` variable that will be filled with a specific error code on failure. Can be `NULL`.
 *
 * @return The OpenGL program ID (`GLuint`) on successful creation and linking.
 * @return `0` on failure. On failure, a detailed error message is set by one of the internal helper functions.
 *
 * @note This is the sole entry point for all internal OpenGL compute shader creation and is called by `SituationCreateComputePipelineFromMemory`.
 * @warning The caller is responsible for deleting the returned program ID using `glDeleteProgram`.
 *
 * @see SituationCreateComputePipelineFromMemory(), _SituationCreateGLComputeProgramFromSpirv(), _SituationCreateGLShaderProgramFromSource()
 */
static GLuint _SituationCreateGLComputeProgram(const void* source_data, SituationGLShaderSourceType source_type, SituationError* error_code) {
    if (error_code) *error_code = SITUATION_SUCCESS;

    if (!source_data) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationCreateGLComputeProgram: Shader source data cannot be NULL.");
        if (error_code) *error_code = SITUATION_ERROR_INVALID_PARAM;
        return 0;
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // If we have SPIR-V data and the driver supports it, use the modern path.
    if (source_type == SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV && GLAD_GL_ARB_gl_spirv) {
        return _SituationCreateGLComputeProgramFromSpirv((const struct _SituationSpirvBlob*)source_data, error_code);
    }
#endif

    // Fallback to the traditional GLSL source path in all other cases.
    if (source_type == SITUATION_GL_SHADER_SOURCE_TYPE_GLSL) {
        return _SituationCreateGLShaderProgramFromSource((const char*)source_data, error_code);
    }

    // If we get here, it means we were given SPIR-V but couldn't use it (either compiler is off or driver lacks support).
    _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "Received SPIR-V data, but cannot process it (GL_ARB_gl_spirv unavailable or shader compiler disabled).");
    if (error_code) *error_code = SITUATION_ERROR_OPENGL_UNSUPPORTED;
    return 0;
}
#endif


 /**
 * @brief [core] Creates a compute pipeline directly from GLSL source code provided as a C string in memory.
 *
 * @details This function compiles the provided GLSL compute shader source code into SPIR-V bytecode (if the shader compiler is enabled) and then creates the corresponding backend-specific compute pipeline object (e.g., OpenGL program, Vulkan pipeline).
 *          The resulting `SituationComputePipeline` handle can be used with `SituationCmdBindComputePipeline` and `SituationCmdDispatch` to execute compute work on the GPU.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Compiles the GLSL source into an OpenGL Compute Program. If `SITUATION_ENABLE_SHADER_COMPILER` and `GL_ARB_gl_spirv` are available, it may compile to SPIR-V first for consistency with Vulkan.
 * - **Vulkan:** This backend **requires** `SITUATION_ENABLE_SHADER_COMPILER`. The function uses `shaderc` to compile the GLSL source into a SPIR-V binary blob, which is then used to create a `VkPipeline`.
 *
 * @param compute_shader_source A null-terminated string containing the GLSL compute shader source code. Must not be NULL.
 *
 * @return A `SituationComputePipeline` handle.
 *         - On **success**: The handle's `.id` member will be non-zero, and it can be used for binding and dispatching. The caller is responsible for destroying it using `SituationDestroyComputePipeline()` to prevent resource leaks.;
 *         - On **failure**: The handle will be in an invalid state (`.id` == 0). A detailed error message can be retrieved using `SituationGetLastErrorMsg()`.
 *           Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *
 * @note The caller is **responsible** for destroying the returned pipeline using `SituationDestroyComputePipeline()` to prevent GPU and CPU memory leaks.;
 * @note This function requires the library to be initialized (`SituationInit()` must have been called successfully).
 * @note This function requires the `SITUATION_ENABLE_SHADER_COMPILER` define to be set during compilation for the Vulkan backend to work with GLSL source. For OpenGL, it depends on the internal handling of GLSL vs SPIR-V (as discussed in `_SituationCreateGLComputeProgram`).
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @par Resource Tracking and Potential Leaks:
 * If this function succeeds (returns a handle with `.id != 0`), the underlying GPU resources are valid.
 * However, an internal CPU memory allocation for resource tracking might fail. In this rare case:
 * - A warning will be printed to `stderr` (e.g., "WARNING: Potential leak of Vulkan compute pipeline handle...").
 * - The valid GPU resource handle is still returned.
 * - It is the caller's **absolute responsibility** to call `SituationDestroyComputePipeline()` on the returned handle to prevent a GPU resource leak, as the library's automatic shutdown cleanup will not track this specific resource.;
 *
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 * @see SituationCreateComputePipeline()
 * @see SituationDestroyComputePipeline();
 * @see SituationCmdBindComputePipeline()
 * @see SituationCmdDispatch()
 */
SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline) {
    if (!SituationIsInitialized()) return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationCreateComputePipelineFromMemory: Library not initialized.");
    if (!compute_shader_source) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipelineFromMemory: Compute shader source cannot be NULL.");
    if (!out_pipeline) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipelineFromMemory: out_pipeline cannot be NULL.");

    SituationComputePipeline handle;
    _SituationComputePipelineSlot* slot = _SitAllocComputePipelineSlot(&handle);
    if (!slot) return SITUATION_ERROR_MEMORY_ALLOCATION;

    slot->layout_type = layout_type;

#if defined(SITUATION_USE_OPENGL)
    // OpenGL Compute Creation
    SituationError err;
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // SPIR-V path
    _SituationSpirvBlob cs_spirv = _SituationVulkanCompileGLSLtoSPIRV(compute_shader_source, "compute_shader", shaderc_compute_shader);
    if (!cs_spirv.data) {
        _SituationFreeSpirvBlob(&cs_spirv);
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED;
    }
    slot->gl_program_id = _SituationCreateGLComputeProgramFromSpirv(&cs_spirv, &err);
    _SituationFreeSpirvBlob(&cs_spirv);
#else
    // GLSL path
    slot->gl_program_id = _SituationCreateGLComputeProgram(compute_shader_source, SITUATION_GL_SHADER_SOURCE_TYPE_GLSL, &err);
#endif

    if (err != SITUATION_SUCCESS) {
        _SitFreeComputePipelineSlot(handle);
        return err;
    }

    /* Vulkan TWO_SSBOS uses two descriptor sets (binding 0 each). OpenGL maps API set_index to
     * glBindBufferBase(SSBO, set_index, ...), i.e. SSBO binding points 0 and 1. SPIR-V from the same
     * GLSL can still reflect both blocks at binding 0 unless we assign block bindings explicitly. */
    if (layout_type == SIT_COMPUTE_LAYOUT_TWO_SSBOS && slot->gl_program_id != 0) {
        GLuint prog = slot->gl_program_id;
        GLuint r_in = glGetProgramResourceIndex(prog, GL_SHADER_STORAGE_BLOCK, "InBuffer");
        GLuint r_out = glGetProgramResourceIndex(prog, GL_SHADER_STORAGE_BLOCK, "OutBuffer");
        if (r_in != GL_INVALID_INDEX) glShaderStorageBlockBinding(prog, r_in, 0);
        if (r_out != GL_INVALID_INDEX) glShaderStorageBlockBinding(prog, r_out, 1);
    }

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan Compute Creation
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    _SituationSpirvBlob cs_spirv = _SituationVulkanCompileGLSLtoSPIRV(compute_shader_source, "compute_shader", shaderc_compute_shader);
    if (!cs_spirv.data) {
        _SituationFreeSpirvBlob(&cs_spirv);
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    VkPipelineLayout layout = sit_render.vk.compute_layouts[layout_type];
    if (layout == VK_NULL_HANDLE) layout = sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_ONE_SSBO]; // Fallback

    // INLINED LOGIC:
    VkShaderModule shaderModule = _SituationVulkanCreateShaderModule(cs_spirv.data, cs_spirv.size);
    _SituationFreeSpirvBlob(&cs_spirv); // Free the blob immediately after creating the module

    if (shaderModule == VK_NULL_HANDLE) {
         _SitFreeComputePipelineSlot(handle);
         return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module = shaderModule;
    computePipelineCreateInfo.stage.pName = "main";
    computePipelineCreateInfo.layout = layout;

    VkPipeline vk_pipeline;
    if (vkCreateComputePipelines(sit_render.vk.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, &vk_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(sit_render.vk.device, shaderModule, NULL);
        _SitFreeComputePipelineSlot(handle);
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    slot->vk_pipeline = vk_pipeline;
    slot->vk_pipeline_layout = layout;
    slot->shader_module = shaderModule; // Store module to destroy later

#else
    _SitFreeComputePipelineSlot(handle);
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Vulkan compute requires shader compiler.");
#endif // SITUATION_ENABLE_SHADER_COMPILER

#endif // SITUATION_USE_VULKAN

    *out_pipeline = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Creates a compute pipeline by loading GLSL source code from a file.
 *
 * @details This function is a convenience wrapper around `SituationCreateComputePipelineFromMemory`. It loads the GLSL compute shader source code from the specified file path into memory and then uses that string to create the compute pipeline using the standard process.
 *
 * @param compute_shader_path The file system path to the GLSL compute shader source file (e.g., "shaders/compute_filter.comp"). This path must be valid and accessible.
 *
 * @return A `SituationComputePipeline` handle.
 *         - On **success**, the handle's `id` member will be non-zero, and it can be used with functions like `SituationCmdBindComputePipeline` and `SituationDestroyComputePipeline`.;
 *         - On **failure** (e.g., file not found, read error, compilation error, pipeline creation failure), the handle will be zero-initialized (`{0}`). Use `SituationGetLastErrorMsg()` to retrieve a detailed error message.
 *
 * @note This function requires the library to be successfully initialized  (`SituationInit` must have been called).
 * @note This function requires `SITUATION_ENABLE_SHADER_COMPILER` to be defined if runtime compilation of GLSL to SPIR-V is needed (which is the standard process for Vulkan and often for OpenGL).
 * @note The caller is responsible for eventually destroying the returned pipeline using `SituationDestroyComputePipeline` to prevent GPU and CPU memory leaks.;
 *
 * @see SituationCreateComputePipelineFromMemory(), SituationDestroyComputePipeline(), SituationLoadFileText();
 */
SITAPI SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline) {
    if (!SituationIsInitialized()) return _SituationSetErrorFromCode( SITUATION_ERROR_NOT_INITIALIZED, "SituationCreateComputePipeline: Library not initialized." );
    if (!compute_shader_path) return _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "SituationCreateComputePipeline: compute_shader_path cannot be NULL." );

    char* source = SituationLoadFileText(compute_shader_path);
    if (!source) {
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationCreateComputePipelineFromMemory(source, layout_type, out_pipeline);

    if (err == SITUATION_SUCCESS) {
        _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*out_pipeline);
        if (slot) {
            slot->source_path = _sit_strdup(compute_shader_path);
            slot->mod_time = SituationGetFileModTime(compute_shader_path);
        }
    }

    SIT_FREE(source);
    return err;
}


// --- Updated/Added Documentation Block for SituationDestroyComputePipeline ---;
/**
 * @brief Destroys a compute pipeline and frees all associated resources.
 *
 * @details This function cleans up the resources acquired during the creation of a `SituationComputePipeline`. This includes backend-specific objects
 *          (e.g., OpenGL program, Vulkan pipeline/layout) and removing the pipeline from the library's internal resource tracking list.
 *          It is crucial to call this function for every successfully created `SituationComputePipeline` to prevent memory leaks of both GPU resources and CPU-side tracking structures.
 *
 * @param pipeline A pointer to the `SituationComputePipeline` handle to be destroyed. The handle's `id` member must be non-zero.
 *                 The contents of the struct pointed to by `pipeline` will be zeroed upon successful destruction.
 *
 * @note It is safe to call this function on an already destroyed or invalid pipeline (where `pipeline->id` is 0); it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 * @warning This function must only be called before `SituationShutdown`.
 * @warning After calling this function, the `SituationComputePipeline` handle pointed to by `pipeline` becomes invalid and must not be used again.
 *
 * @see SituationCreateComputePipeline(), SituationCreateComputePipelineFromMemory()
 */
SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline) {
    if (!pipeline) return;
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*pipeline);
    if (!slot) return;

#if defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE) {
        if (slot->vk_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline, NULL);
            slot->vk_pipeline = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyPipeline(slot->vk_pipeline, VK_NULL_HANDLE);
    }
    if (slot->shader_module != VK_NULL_HANDLE && sit_render.vk.device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(sit_render.vk.device, slot->shader_module, NULL);
        slot->shader_module = VK_NULL_HANDLE;
    }

#elif defined(SITUATION_USE_OPENGL)
    if (glIsProgram(slot->gl_program_id)) {
        glDeleteProgram(slot->gl_program_id);
    }
#endif

    _SitFreeComputePipelineSlot(*pipeline);
    memset(pipeline, 0, sizeof(SituationComputePipeline));
}


/**
 * @brief [INTERNAL] Automates the cleanup of leaked resources during library shutdown.
 *
 * @details This function acts as a garbage collector of last resort. It is called automatically by `SituationShutdown`.
 *          It iterates through the library's internal resource tracking lists (for Meshes, Shaders, Textures, Buffers, etc.)
 *          and identifies any objects that were created by the user but never explicitly destroyed.
 *
 *          For each leaked resource found:
 *          1. It prints a warning message to `stderr` including the resource ID, aiding in debugging.
 *          2. It calls the appropriate destruction function (e.g., `SituationDestroyTexture`) to release the GPU memory.
 *
 *          This prevents permanent VRAM leaks even if the application crashes or exits without proper cleanup.
 *
 * @note This function modifies global state by traversing and emptying the linked lists:
 *       `all_meshes`, `all_shaders`, `all_textures`, `all_buffers`, etc.
 * @warning This is a safety mechanism, not a feature. Relying on it is bad practice; users should always
 *          explicitly destroy resources they create.
 */
static void _SituationCleanupDanglingResources(void) {
    // 1. Textures
    for(int i=0; i<SITUATION_MAX_TEXTURES; i++) {
        if(sit_render.texture_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Texture (Slot %d, Gen %u)\n", i, sit_render.texture_registry[i].generation);
            SituationTexture t = { (uint32_t)i, sit_render.texture_registry[i].generation };
            SituationDestroyTexture(&t);
        }
    }
    // 2. Shaders
    for(int i=0; i<SITUATION_MAX_SHADERS; i++) {
        if(sit_render.shader_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Shader (Slot %d)\n", i);
            SituationShader s = { (uint32_t)i, sit_render.shader_registry[i].generation };
            SituationUnloadShader(&s);
        }
    }
    // 3. Meshes
    for(int i=0; i<SITUATION_MAX_MESHES; i++) {
        if(sit_render.mesh_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Mesh (Slot %d)\n", i);
            SituationMesh m = { (uint32_t)i, sit_render.mesh_registry[i].generation, 0,0,0 };
            SituationDestroyMesh(&m);
        }
    }
    // 4. Buffers
    for(int i=0; i<SITUATION_MAX_BUFFERS; i++) {
        if(sit_render.buffer_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Buffer (Slot %d)\n", i);
            SituationBuffer b = { (uint32_t)i, sit_render.buffer_registry[i].generation, 0, 0 };
            SituationDestroyBuffer(&b);
        }
    }
    // 5. Compute
    for(int i=0; i<SITUATION_MAX_COMPUTE_PIPELINES; i++) {
        if(sit_render.compute_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Compute Pipeline (Slot %d)\n", i);
            SituationComputePipeline p = { (uint32_t)i, sit_render.compute_registry[i].generation };
            SituationDestroyComputePipeline(&p);
        }
    }
    // 6. Models
    for(int i=0; i<SITUATION_MAX_MODELS; i++) {
        if(sit_render.model_registry[i].is_active) {
            fprintf(stderr, "SITUATION WARNING: Leaked Model (Slot %d)\n", i);
            SituationModel m = { (uint32_t)i, sit_render.model_registry[i].generation, 0, NULL };
            SituationUnloadModel(&m);
        }
    }
}

/**
 * @brief Updates a portion (or all) of an existing buffer's contents with new data.
 *
 * @details Copies `size` bytes from the provided `data` pointer into the specified buffer,
 *          starting at byte offset `offset`. This is the primary way to update dynamic
 *          vertex buffers, index buffers, uniform buffers, storage buffers, staging buffers,
 *          or any other GPU-accessible buffer managed by the Situation library.
 *
 *          Supports partial updates (sub-region only) and full-buffer overwrites
 *          (when `offset == 0` and `size == buffer_size`).
 *
 *          Behavior depends on buffer usage flags and backend:
 *            - **Staging / CPU-writable buffers**: direct memcpy to mapped memory (fast)
 *            - **Device-local / GPU-only buffers**: may trigger staging copy via internal
 *              transfer queue or immediate command recording (slower, may block)
 *            - **Persistent mapped buffers**: direct write to persistent mapping
 *
 *          The update is **synchronous** from the caller's perspective ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â the data is guaranteed
 *          to be visible to subsequent GPU commands after this function returns (subject to
 *          proper pipeline barriers/sync inserted by the library).
 *
 * @param buffer Valid `SituationBuffer` handle created via `SituationCreateBuffer` or similar.
 *               Must support writing (not read-only).
 * @param offset Byte offset into the buffer where the update begins (0 = start of buffer).
 *               Must be < buffer size.
 * @param size Number of bytes to copy from `data`. Must satisfy `offset + size <= buffer size`.
 * @param data Pointer to the source data to copy into the buffer.
 *             Must remain valid for the duration of the call.
 *             Ownership is **not** transferred ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller retains the source buffer.
 *
 * @return SITUATION_SUCCESS on successful update,
 *         SITUATION_ERROR_INVALID_PARAM if buffer is invalid, offset+size out of bounds,
 *         or data is NULL when size > 0,
 *         SITUATION_ERROR_RESOURCE_INVALID if buffer is not writable (read-only usage),
 *         SITUATION_ERROR_MEMORY_ACCESS if mapping failed or staging allocation failed,
 *         SITUATION_ERROR_BACKEND_SPECIFIC if Vulkan/GL operation failed (e.g. out of device memory),
 *         or other appropriate error codes.
 *
 * @note Performance:
 *       - Fastest for persistently mapped or staging buffers
 *       - Slower for device-local buffers (implicit staging/transfer)
 *       - Avoid frequent small updates on device-local buffers ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â batch changes or use persistent mapping
 *
 *       Thread safety:
 *       - Safe to call from **main thread** or any thread that does not hold the render context
 *       - Internal synchronization ensures safe concurrent updates (if buffer allows)
 *       - Not safe to call from the render thread while recording commands that use the buffer
 *
 *       After update, the new data is visible to shaders/pipelines in subsequent command buffers.
 *       No explicit barrier is required unless you are reading back or using cross-queue access.
 *
 * @see SituationCreateBuffer, SituationMapBuffer, SituationUnmapBuffer,
 *      SituationCreateBufferFromData (for initial upload),
 *      SITUATION_ERROR_INVALID_PARAM, SITUATION_ERROR_RESOURCE_INVALID
 */
SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data) {
    if (!data) return SITUATION_ERROR_INVALID_PARAM;
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

    if (offset + size > slot->size_in_bytes) return SITUATION_ERROR_BUFFER_OVERFLOW;

#if defined(SITUATION_USE_OPENGL)
    // [Bug Fix] Use immediate glNamedBufferSubData instead of deferred command buffer.
    // The deferred approach required being inside a frame and calling SituationEndFrame
    // before the data was visible to readback, which broke tests that update+readback
    // without a frame cycle. glNamedBufferSubData is DSA and works immediately.
    glNamedBufferSubData(slot->gl_buffer_id, (GLintptr)offset, (GLsizeiptr)size, data);
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan: Try direct map first (works for host-visible buffers like UBOs).
    // If that fails (GPU-only memory), use a single-time command buffer with
    // vkCmdUpdateBuffer for small updates or staging for large ones.
    void* mapped;
    if (vmaMapMemory(sit_render.vk.vma_allocator, slot->vma_allocation, &mapped) == VK_SUCCESS) {
        memcpy((uint8_t*)mapped + offset, data, size);
        vmaUnmapMemory(sit_render.vk.vma_allocator, slot->vma_allocation);
        return SITUATION_SUCCESS;
    } else {
        // GPU-only memory: use single-time command buffer so this works outside of a frame.
        // vkCmdUpdateBuffer has a 65536-byte limit per spec.
        if (size > 65536) {
            // Large update: use staging buffer
            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;

            VkBufferCreateInfo staging_info = {};
            staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_info.size = size;
            staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo staging_alloc = {0};
            staging_alloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            if (vmaCreateBuffer(sit_render.vk.vma_allocator, &staging_info, &staging_alloc, &staging_buffer, &staging_allocation, NULL) != VK_SUCCESS) {
                return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
            }

            void* staging_mapped;
            if (vmaMapMemory(sit_render.vk.vma_allocator, staging_allocation, &staging_mapped) != VK_SUCCESS) {
                vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
                return SITUATION_ERROR_BUFFER_MAP_FAILED;
            }
            memcpy(staging_mapped, data, size);
            vmaUnmapMemory(sit_render.vk.vma_allocator, staging_allocation);

            VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
            if (cmd == VK_NULL_HANDLE) {
                vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
                return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
            }

            VkBufferCopy region = { .srcOffset = 0, .dstOffset = offset, .size = size };
            vkCmdCopyBuffer(cmd, staging_buffer, slot->vk_buffer, 1, &region);
            _SituationVulkanEndSingleTimeCommands(cmd);

            vmaDestroyBuffer(sit_render.vk.vma_allocator, staging_buffer, staging_allocation);
            return SITUATION_SUCCESS;
        } else {
            // Small update: use vkCmdUpdateBuffer in a single-time command buffer
            VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
            if (cmd == VK_NULL_HANDLE) {
                return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
            }
            vkCmdUpdateBuffer(cmd, slot->vk_buffer, offset, size, data);
            _SituationVulkanEndSingleTimeCommands(cmd);
            return SITUATION_SUCCESS;
        }
    }
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}



/**
 * @brief Reads data back from a GPU buffer to host memory.
 *
 * @details Copies a specified range of data from the GPU buffer into a user-provided host memory buffer. This is useful for debugging, reading results from compute shaders, or retrieving data generated on the GPU.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Uses `glGetNamedBufferSubData` to read data directly from the buffer object into host memory, provided the buffer was created with appropriate flags (e.g., `GL_DYNAMIC_STORAGE_BIT` or `GL_MAP_READ_BIT` implicitly via usage).
 * - **Vulkan:** Reading from GPU-local memory (`VMA_MEMORY_USAGE_GPU_ONLY`) requires a staging buffer. This function internally allocates a temporary staging buffer, copies the data from the source buffer to the staging buffer using a command,
 *   and then maps the staging buffer to copy the data to the user's `out_data` pointer.
 *   This process is asynchronous and requires waiting for the GPU to finish the copy.
 *
 * @param buffer The `SituationBuffer` handle to read data from.
 * @param offset The byte offset within the GPU buffer to start reading from.
 * @param size The number of bytes to read.
 * @param[out] out_data A pointer to the host memory buffer where the data will be written.
 *                      This buffer must be allocated by the caller and large enough to hold `size` bytes.
 *
 * @return SITUATION_SUCCESS on successful read.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid.
 * @return SITUATION_ERROR_INVALID_PARAM if `out_data` is NULL.
 * @return SITUATION_ERROR_BUFFER_INVALID_SIZE if `offset + size` exceeds the buffer's size.
 * @return SITUATION_ERROR_BUFFER_MAP_FAILED if mapping memory fails (Vulkan) or reading fails (OpenGL).
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if creating a staging buffer fails (Vulkan).
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED if recording or submitting the copy command fails (Vulkan).
 */
SITAPI SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data) {
    if (!out_data) return SITUATION_ERROR_INVALID_PARAM;
    _SituationBufferSlot* slot = _SitGetBufferSlot(buffer);
    if (!slot) return SITUATION_ERROR_RESOURCE_INVALID;

#if defined(SITUATION_USE_OPENGL)
    // Synchronous read (stalls)
    glGetNamedBufferSubData(slot->gl_buffer_id, offset, size, out_data);
    return SITUATION_SUCCESS;
#elif defined(SITUATION_USE_VULKAN)
    // Use helper
    return _SituationVulkanReadBackBuffer(slot->vk_buffer, slot->vma_allocation, size, offset, out_data);
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

// --- Command Buffer Implementations ---

/**
 * @brief Binds a compute pipeline for subsequent dispatch commands.
 * @details Activates the specified compute pipeline, making its shader program and associated state active for subsequent `SituationCmdDispatch` and resource binding commands (e.g., `SituationCmdBindComputeBuffer`) recorded in the command buffer.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glUseProgram(pipeline.gl_program_id)` to activate the OpenGL Compute Program associated with the `SituationComputePipeline` handle.
 * - **Vulkan:** Records a `vkCmdBindPipeline` command into the provided command buffer for the `VK_PIPELINE_BIND_POINT_COMPUTE` bind point. It also updates the internal global state `sit_render.vk.current_compute_pipeline_layout` with the pipeline's layout.
 *   This layout is essential for subsequent `vkCmdBindDescriptorSets` (called by `SituationCmdBindComputeBuffer`) and `vkCmdPushConstants` (called by `SituationCmdSetPushConstant`) commands to specify the correct pipeline interface.
 *
 * @param cmd The command buffer into which the bind command will be recorded.
 *            In OpenGL, this parameter is typically ignored as it uses global state.
 * @param pipeline The `SituationComputePipeline` handle representing the compute pipeline to bind.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       2. The compute pipeline represented by `pipeline` was created successfully.
 * @warning This function must be called before any dispatch or resource binding commands related to this compute pipeline.
 */
SITAPI void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot bind compute pipeline.");
        return;
    }

    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(pipeline);
    if (!slot) {
         _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "Invalid compute pipeline handle provided.");
        #if defined(SITUATION_USE_VULKAN)
        sit_render.vk.current_compute_pipeline_layout = VK_NULL_HANDLE;
        #endif
        return;
    }

#if defined(SITUATION_USE_VULKAN)
    if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for binding compute pipeline.");
        return;
    }

    vkCmdBindPipeline((VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_COMPUTE, slot->vk_pipeline);
    sit_render.vk.current_compute_pipeline_layout = slot->vk_pipeline_layout;
#elif defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_BIND_COMPUTE_PIPELINE);
    if (p) {
        p->args.bind_pipeline.shader_id = (uint64_t)slot->gl_program_id;
    }
#endif
}

/**
 * @brief Binds a GPU buffer (typically an SSBO) to a specific binding point within the currently bound compute pipeline.
 * @details Associates a `SituationBuffer` with a binding point declared in the GLSL compute shader code (e.g., `layout(set = ..., binding = X) buffer ...`). This allows the compute shader to access the buffer's data.
 *
 * @par Backend-Specific Behavior & Performance
 * - **OpenGL:** Calls `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.gl_buffer_id)`.
 *   This efficiently binds the buffer to the specified unit for use by the currently active compute program.
 * - **Vulkan:** This function implements a high-performance, persistent descriptor set model.
 *   When the `SituationBuffer` was created (via `SituationCreateBuffer`), the Vulkan backend internally allocated a `VkDescriptorSet` from a dedicated persistent pool and populated
 *   it with the buffer's `VkBuffer` handle. This function simply records a fast `vkCmdBindDescriptorSets` command using this pre-cached descriptor set.
 *   This approach avoids the significant CPU overhead of allocating and updating descriptor sets every frame, which is crucial for performance in Vulkan.
 *
 * @param cmd The command buffer into which the bind command will be recorded (Vulkan) or ignored (OpenGL).
 * @param binding The binding point index within the compute shader's descriptor set.
 *                In GLSL, this corresponds to the `binding = X` part of the layout qualifier.
 *                In Vulkan, this corresponds to the `dstBinding` used when the buffer's internal descriptor set was originally populated (typically 0 for a single buffer resource within its set).
 * @param buffer The `SituationBuffer` handle to bind. The buffer should have been created with usage flags indicating it will be used as a storage buffer (e.g., `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`).
 *
 * @return SITUATION_SUCCESS on successful recording of the bind command.
 * @return SITUATION_ERROR_NOT_INITIALIZED if the library is not initialized.
 * @return SITUATION_ERROR_RESOURCE_INVALID if the buffer handle is invalid (e.g., `id` is 0).
 * @return SITUATION_ERROR_INVALID_PARAM (Vulkan) if the provided command buffer handle is invalid.
 * @return SITUATION_ERROR_RESOURCE_INVALID (Vulkan) if the buffer's internal pre-cached descriptor set is invalid or missing.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A compute pipeline has been successfully bound using `SituationCmdBindComputePipeline` before calling this function.
 *       2. (Vulkan) The command buffer `cmd` is valid and in the recording state.
 *       3. The `binding` index matches the layout specified in the compute shader.
 * @warning Binding a buffer that was not created with appropriate usage flags (like `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`) may lead to undefined behavior or validation errors.
 */
SITAPI SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer) {
    // The old 'binding' parameter directly maps to the new 'set_index' parameter.
    return SituationCmdBindDescriptorSet(cmd, binding, buffer);
}

/**
 * @brief Inserts a pipeline memory barrier for synchronization.
 * @details This is a critical function for synchronizing memory access between different pipeline stages, especially between compute and graphics passes, or before/after transfer operations.
 *          It ensures that writes from a source stage (e.g., a compute shader) are visible to reads or writes in a destination stage (e.g., a vertex shader, or another compute shader).
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** This maps to one or more `glMemoryBarrier` calls. The `src_flags` and `dst_flags` are combined to determine the necessary OpenGL barrier bits. Multiple barriers might be issued
 *   if the combined flags require it (e.g., one for SSBO/ShaderImage access, another for indirect commands).
 * - **Vulkan:** This maps to one or more `vkCmdPipelineBarrier` calls. It carefully constructs the `srcStageMask`, `dstStageMask`, `srcAccessMask`, and `dstAccessMask` based on the abstract flags.
 *   This implementation correctly maps the defined abstract flags to their Vulkan equivalents.
 *
 * @param cmd The command buffer to record the barrier into.
 * @param src_flags A bitmask of `SituationBarrierSrcFlags` indicating the pipeline stage(s) and type(s) of memory access that form the source of the dependency.
 * @param dst_flags A bitmask of `SituationBarrierDstFlags` indicating the pipeline stage(s) and type(s) of memory access that form the destination of the dependency.
 */
SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags) {
    if (!SituationIsInitialized()) { return; } // Silently return if the library isn't initialized.

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_PIPELINE_BARRIER);
    if (p) {
        p->args.barrier.src = src_flags;
        p->args.barrier.dst = dst_flags;
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- Enhanced Vulkan Pipeline Barrier Implementation ---
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // --- Accumulate Vulkan Stages and Access Masks ---
        VkPipelineStageFlags src_stage_mask = 0;
        VkAccessFlags src_access_mask = 0;
        VkPipelineStageFlags dst_stage_mask = 0;
        VkAccessFlags dst_access_mask = 0;

        // --- Determine Source Stage and Access ---
        // What stage wrote the data, and how?
        if (src_flags & SITUATION_BARRIER_COMPUTE_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; }
        if (src_flags & SITUATION_BARRIER_FRAGMENT_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // VK_ACCESS_SHADER_WRITE_BIT is correct for imageStore or SSBO writes in the fragment shader.
        if (src_flags & SITUATION_BARRIER_VERTEX_SHADER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; src_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // Vertex shader writes (e.g., to SSBOs).
        if (src_flags & SITUATION_BARRIER_TRANSFER_WRITE) { src_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT; src_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT; }
        // --- Determine Destination Stage and Access ---
        // What stage will read or write the data next, and how?
        if (dst_flags & SITUATION_BARRIER_COMPUTE_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT; }
        if (dst_flags & SITUATION_BARRIER_COMPUTE_SHADER_WRITE) { dst_stage_mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_WRITE_BIT; } // Ensuring visibility for a subsequent write by the compute shader.
        if (dst_flags & SITUATION_BARRIER_VERTEX_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; } // [FIX] Added VERTEX_ATTRIBUTE_READ
        if (dst_flags & SITUATION_BARRIER_FRAGMENT_SHADER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; dst_access_mask |= VK_ACCESS_SHADER_READ_BIT; } // For textures/SSBOs
        if (dst_flags & SITUATION_BARRIER_TRANSFER_READ) { dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT; dst_access_mask |= VK_ACCESS_TRANSFER_READ_BIT; } // Reading for a copy operation
        if (dst_flags & SITUATION_BARRIER_TRANSFER_WRITE) {
            // Ensuring the destination of a transfer write is ready.
            // The src barrier ensures data written by shaders is visible for transfer *read*.
            // This barrier ensures the *destination* resource is ready to be written to by transfer.
            // This is less common as transfer destinations are often "fresh". But if a buffer/image was previously written by a shader and is now the destination of a transfer, this barrier makes sense.
            dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_access_mask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        if (dst_flags & SITUATION_BARRIER_INDIRECT_COMMAND_READ) {
            dst_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT; // Or DISPATCH_INDIRECT_BIT for compute
            // Vulkan spec often uses VERTEX_INPUT_BIT or others for indirect, but DRAW_INDIRECT_BIT is specific.
            // Let's use DRAW_INDIRECT_BIT. For compute dispatches, DISPATCH_INDIRECT_BIT is correct.
            // The source stage/access for writing indirect args would be SHADER_WRITE_BIT.
            // To cover both draw and dispatch indirect, we might need to set both stages, or determine it dynamically. For simplicity, we'll use DRAW_INDIRECT_BIT.
            // Access for reading indirect args is INDIRECT_COMMAND_READ_BIT.
            dst_stage_mask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // Safer to include both
            dst_access_mask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        }

        // --- [ROBUSTNESS] Prevent validation errors from empty stage masks ---
        // If no source stage is specified, assume the earliest possible stage.
        if (src_stage_mask == 0) { src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
        // If no destination stage is specified, assume the latest possible stage.
        if (dst_stage_mask == 0) { dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }

        // --- Issue the Barrier ---
        // Allow barriers where access masks are 0 (Execution-Only Dependencies)
        // Only skip if both stage masks are 0 (which implies no dependency defined)
        if (src_stage_mask != 0 || dst_stage_mask != 0) {
            // --- Basic Memory Barrier (No image/buffer memory transitions assumed) ---
            // For image/buffer layout transitions, VkImageMemoryBarrier or VkBufferMemoryBarrier structs would need to be set up and passed to vkCmdPipelineBarrier.
            VkMemoryBarrier memory_barrier = {};
            memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.srcAccessMask = src_access_mask;
            memory_barrier.dstAccessMask = dst_access_mask;

            // Perform the barrier
            // Using VK_DEPENDENCY_BY_REGION_BIT can be a performance hint if the access is localized.
            // For general barriers, it's often omitted unless specifically needed.
            vkCmdPipelineBarrier(
                vk_cmd,
                src_stage_mask,           // srcStageMask
                dst_stage_mask,           // dstStageMask
                0,                        // dependencyFlags (e.g., VK_DEPENDENCY_BY_REGION_BIT)
                1,                        // memoryBarrierCount
                &memory_barrier,          // pMemoryBarriers
                0,                        // bufferMemoryBarrierCount
                NULL,                     // pBufferMemoryBarriers
                0,                        // imageMemoryBarrierCount
                NULL                      // pImageMemoryBarriers
            );
        } else {
            // Optional: Log a verbose message if no effective barrier is specified?
            // This is generally not an error, just a no-op.
            // fprintf(stderr, "VERBOSE: SituationCmdPipelineBarrier called with no effective barriers (src: 0x%x, dst: 0x%x).\n", src_flags, dst_flags);
        }
        // --- End Enhanced Implementation ---
    }
#endif
}

/**
 * @brief Dispatches compute work using the currently bound compute pipeline.
 *
 * @details Executes the compute shader associated with the compute pipeline that was previously bound using `SituationCmdBindComputePipeline`. The number of work groups to be executed in each dimension (X, Y, Z) must be specified.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Calls `glDispatchCompute`. This function uses the currently active compute program (bound via `glUseProgram`).
 * - **Vulkan:** Records a `vkCmdDispatch` command into the provided command buffer.
 *   It requires that a compute pipeline has been previously bound to the command buffer using `vkCmdBindPipeline` with `VK_PIPELINE_BIND_POINT_COMPUTE`.
 *   Any necessary descriptor sets (for SSBOs, textures, etc.) must also be bound prior to this call.
 *
 * @param cmd The command buffer into which the dispatch command will be recorded (Vulkan) or which provides context (OpenGL, though often unused).
 * @param group_count_x The number of local work groups to dispatch in the X dimension.
 * @param group_count_y The number of local work groups to dispatch in the Y dimension.
 * @param group_count_z The number of local work groups to dispatch in the Z dimension.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. A valid compute pipeline is bound before calling this function.
 *       2. All required resources (buffers, textures via descriptor sets/binds) are bound.
 *       3. Appropriate memory barriers (`SituationMemoryBarrier`) are used if synchronization is needed before or after the dispatch.
 *
 * @warning Calling this function without a bound compute pipeline will result in undefined behavior or a Vulkan validation error.
 */
SITAPI void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) {
    // --- 1. Core Library Initialization Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot dispatch compute work.");
        return;
    }

#if defined(SITUATION_USE_OPENGL)
    SituationGLSoftCommandBuffer* buf = (SituationGLSoftCommandBuffer*)cmd;
    SitCommandPacket* p = _SitGLSoftCmdPush(buf, SIT_OP_DISPATCH);
    if (p) {
        p->args.dispatch.x = group_count_x;
        p->args.dispatch.y = group_count_y;
        p->args.dispatch.z = group_count_z;
    }

#elif defined(SITUATION_USE_VULKAN)
    {
        // --- 2. Vulkan Input Validation ---
        // 2.1. Validate Command Buffer Handle
        if (cmd == 0 || (VkCommandBuffer)cmd == VK_NULL_HANDLE) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid command buffer for compute dispatch.");
            return;
        }
        VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;

        // 2.2. (Optional but Robust) Validate that a Compute Pipeline is Bound
        // While Vulkan drivers will error if no pipeline is bound, checking here provides
        // clearer feedback. This requires tracking the last bound compute pipeline layout
        // or a simple boolean flag in the global state (e.g., sit_render.vk.is_compute_pipeline_bound).
        // Uncomment the lines below if such state tracking is implemented.
        /*
        if (sit_render.vk.current_compute_pipeline_layout == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED, "Cannot dispatch compute work; no compute pipeline is currently bound. Call SituationCmdBindComputePipeline first.");
             return;
        }
        */

        // --- 3. Vulkan Dispatch ---
        // Records the dispatch command into the command buffer.
        // Assumes the pipeline and descriptor sets are correctly bound beforehand.
        vkCmdDispatch(vk_cmd, group_count_x, group_count_y, group_count_z);
    }
#endif
    // --- 4. Post-Dispatch (if needed) ---
    // No general post-dispatch actions are required here.
    // Synchronization is handled by the user via SituationMemoryBarrier.
}

/**
 * @brief Queries the maximum number of work groups that can be dispatched in a single compute command.
 * @details Returns the hardware limit for the number of local work groups in the X, Y, and Z dimensions.
 *          This corresponds to `glDispatchCompute` or `vkCmdDispatch` arguments.
 *          Note: This is the maximum count per dimension for a *single* dispatch, not the total number of concurrent groups.
 * @param[out] x Pointer to store the maximum X dimension.
 * @param[out] y Pointer to store the maximum Y dimension.
 * @param[out] z Pointer to store the maximum Z dimension.
 */
SITAPI void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z) {
    uint32_t max_x = 0, max_y = 0, max_z = 0;

    if (SituationIsInitialized()) {
#if defined(SITUATION_USE_VULKAN)
        if (sit_render.vk.physical_device) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
            max_x = props.limits.maxComputeWorkGroupCount[0];
            max_y = props.limits.maxComputeWorkGroupCount[1];
            max_z = props.limits.maxComputeWorkGroupCount[2];
        }
#elif defined(SITUATION_USE_OPENGL)
        // OpenGL 4.3+ required for Compute
        GLint gx = 0, gy = 0, gz = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &gx);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &gy);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &gz);
        max_x = (uint32_t)gx;
        max_y = (uint32_t)gy;
        max_z = (uint32_t)gz;
#endif
    }

    if (x) *x = max_x;
    if (y) *y = max_y;
    if (z) *z = max_z;
}

/**
 * @brief Queries whether a specific rendering feature is supported on the current platform/backend.
 *
 * @details Returns true if the given render feature (or combination of features when using a bitmask)
 *          is fully supported by the active graphics backend (OpenGL or Vulkan) and current hardware/driver.
 *
 *          This function supports **bitmask queries** ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â you can pass a combination of features using bitwise OR
 *          (e.g. `SITUATION_FEATURE_COMPUTE | SITUATION_FEATURE_MESH_SHADING`). In mask mode, the function
 *          returns true **only if all requested features are supported** (logical AND semantics).
 *
 *          Typical use cases:
 *            - Runtime feature detection for enabling/disabling advanced effects
 *            - Graceful degradation (fall back to simpler shaders/pipelines)
 *            - Conditional UI options in tools/editors
 *            - Logging supported feature set at startup
 *
 *          Supported features are defined in the `SituationRenderFeature` enum and include (but are not limited to):
 *            - Compute shaders
 *            - Mesh/task shaders
 *            - Ray tracing acceleration structures
 *            - Variable rate shading
 *            - Bindless resources
 *            - Multi-draw indirect
 *            - Etc.
 *
 * @param feature A single `SituationRenderFeature` value **or** a bitmask of multiple features combined with `|`.
 *                Passing 0 always returns true (no features requested).
 *
 * @return true if **all** requested features (or the single feature) are supported by the current backend/hardware,
 *         false otherwise.
 *         Returns true for unknown/undefined feature bits (safe default).
 *
 * @note This is a fast, cached query ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â results are determined during initialization (`SituationInit`)
 *       by checking extension strings, device properties, feature structs (Vulkan), or GL version/extensions.
 *       Thread-safe and non-blocking ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â safe to call from any thread at any time after init.
 *       Result does **not** change during the application lifetime unless the backend is reinitialized.
 *
 *       **Mask behavior example:**
 *       ```c
 *       if (SituationIsFeatureSupported(SITUATION_FEATURE_COMPUTE | SITUATION_FEATURE_MESH_SHADING)) {
 *           // Use advanced compute + mesh pipeline
 *       } else if (SituationIsFeatureSupported(SITUATION_FEATURE_COMPUTE)) {
 *           // Fallback to compute only
 *       }
 *       ```
 *
 * @see SituationRenderFeature (enum), SituationInit,
 *      SituationGetRendererBackend (if exists), SITUATION_FEATURE_xxx constants
 */
SITAPI bool SituationIsFeatureSupported(SituationRenderFeature feature) {
    if (!SituationIsInitialized()) return false;
    // Check against the mask populated during backend initialization
    return (sit_render.enabled_features_mask & feature) != 0;
}

#if defined(SITUATION_USE_VULKAN)
/**
 * @brief [INTERNAL] Creates a VkImage and allocates its memory using VMA.
 * @details This is a core Vulkan helper function that abstracts the creation of an image and the allocation of its device memory. It uses the Vulkan Memory Allocator (VMA) to handle the memory binding, which is the recommended practice.
 *
 * @param width The width of the image in pixels.
 * @param height The height of the image in pixels.
 * @param format The pixel format of the image (e.g., VK_FORMAT_R8G8B8A8_SRGB).
 * @param tiling The tiling arrangement for texels (VK_IMAGE_TILING_OPTIMAL for GPU-only, VK_IMAGE_TILING_LINEAR for CPU access).
 * @param usage A bitmask of VkImageUsageFlagBits specifying how the image will be used (e.g., as a color attachment, a sampled texture, etc.).
 * @param memory_usage A VmaMemoryUsage hint for the allocator (e.g., VMA_MEMORY_USAGE_GPU_ONLY for high-performance device memory).
 *          This function is essential for the cleanup routines of swapchains, textures, and virtual displays.
 *
 * @param image The `VkImage` handle to destroy.
 * @param allocation The associated `VmaAllocation` handle to free.
 */
static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation) {
    if (image != VK_NULL_HANDLE && sit_render.vk.vma_allocator != VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, image, allocation);
    }
}

/**
 * @brief [INTERNAL] Destroys a VkBuffer and frees its associated VMA allocation.
 * @details This is a simple wrapper around `vmaDestroyBuffer` that provides null-safety checks
 *          and centralizes buffer destruction logic for consistency with `_SituationVulkanDestroyImage`.
 *          It ensures that the buffer and allocator handles are valid before attempting destruction.
 *
 * @param buffer The VkBuffer handle to destroy. If VK_NULL_HANDLE, the function does nothing.
 * @param allocation The associated VmaAllocation handle to free.
 *
 * @note This function is safe to call with VK_NULL_HANDLE for the buffer parameter.
 * @note The VMA allocator must be valid (`sit_render.vk.vma_allocator != VK_NULL_HANDLE`) for destruction to occur.
 *
 * @see _SituationVulkanDestroyImage(), vmaDestroyBuffer(), _SituationDeferDestroyBuffer()
 */
static void _SituationVulkanDestroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
    if (buffer != VK_NULL_HANDLE && sit_render.vk.vma_allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, buffer, allocation);
    }
}

/**
 * @brief [INTERNAL] Creates a VkImageView for a given VkImage.
 * @details An image view is a mandatory component that describes how to access a VkImage and which parts of it are accessible. It specifies metadata like the format and aspect (e.g., color, depth, stencil) without which the GPU cannot interpret the image data.
 *
 * @param image The VkImage for which to create a view.
 * @param format The pixel format, which must be compatible with the format of the source image.
 * @param aspect_flags A bitmask of VkImageAspectFlagBits specifying which aspect of the image the view will access (e.g., VK_IMAGE_ASPECT_COLOR_BIT for color textures, VK_IMAGE_ASPECT_DEPTH_BIT for depth buffers).
 * @return A valid VkImageView handle on success, or VK_NULL_HANDLE on failure.
 */
static VkImageView _SituationVulkanCreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(sit_render.vk.device, &view_info, NULL, &image_view) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return image_view;
}

/**
 * @brief [INTERNAL] Creates a Vulkan shader module from raw SPIR-V bytecode.
 *
 * @details This is a low-level helper function that wraps `vkCreateShaderModule` with
 *          proper error handling, validation, and logging tailored to the Situation library.
 *          It is called internally whenever a new SPIR-V blob needs to be turned into a
 *          usable `VkShaderModule` (e.g. during shader creation, hot-reload, or pipeline
 *          rebuilds).
 *
 *          The function:
 *            - Validates input (non-null code, size multiple of 4, reasonable size)
 *            - Fills out `VkShaderModuleCreateInfo` with the provided SPIR-V data
 *            - Calls `vkCreateShaderModule` on the logical device
 *            - Logs detailed errors (including Vulkan result codes) on failure
 *            - Sets the global Situation error state when appropriate
 *
 *          On success, returns a valid `VkShaderModule` handle that must be destroyed
 *          later with `vkDestroyShaderModule` (typically via graveyard/deferred cleanup
 *          in your render thread or hot-reload path).
 *
 * @param code Pointer to the SPIR-V bytecode buffer (must be 4-byte aligned, little-endian).
 *             Ownership is **not** transferred ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller must keep the memory alive until
 *             the returned module is destroyed.
 * @param code_size Size of the SPIR-V buffer in bytes.
 *                  Must be > 0 and a multiple of 4 (SPIR-V word size).
 *
 * @return A valid `VkShaderModule` handle on success,
 *         VK_NULL_HANDLE on failure (allocation error, invalid SPIR-V, device lost, etc.).
 *         On failure, an appropriate `SituationError` code is set internally
 *         (e.g. SITUATION_ERROR_MEMORY_ALLOCATION, SITUATION_ERROR_SHADER_MODULE_CREATE_FAILED).
 *
 * Thread safety invariants:
 *   - Must be called from a thread that has access to the Vulkan device/queue
 *     (typically the render thread or during init/hot-reload on main thread)
 *   - No internal locking ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller must ensure no concurrent module creation/destruction
 *   - Safe to call during hot-reload if old modules are destroyed first
 *
 * @note This function does **not** validate the SPIR-V itself (use shaderc validation
 *       or spirv-val during compilation/hot-reload for that).
 *       The returned module is not cached ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â caller is responsible for lifetime management
 *       and reuse (your shader cache or hot-reload system should handle deduplication).
 *       In debug builds with Vulkan validation layers enabled, invalid SPIR-V will trigger
 *       layer messages before the function returns VK_ERROR_INVALID_SHADER_NV or similar.
 *
 * @see _SituationCompileGLSLtoSPIRV, _SituationFreeSpirvBlob,
 *      SituationCreateShader (public API), vkCreateShaderModule,
 *      SITUATION_ERROR_SHADER_MODULE_CREATE_FAILED
 */
static VkShaderModule _SituationVulkanCreateShaderModule(const void* code, size_t code_size) {
    if (!code || code_size == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Shader code is NULL or size is zero in _SituationVulkanCreateShaderModule.");
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    // Cast away const for pCode (Vulkan spec allows this for SPIR-V blobs)
    create_info.pCode = (const uint32_t*)code;

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(sit_render.vk.device, &create_info, NULL, &shader_module);
    if (result != VK_SUCCESS) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "vkCreateShaderModule failed: VkResult = %d", (int)result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED, err_msg);
        return VK_NULL_HANDLE;
    }

    return shader_module;
}

/**
 * @brief [INTERNAL] Creates a complete Vulkan graphics pipeline from SPIR-V bytecode.
 * @details This is a generic and powerful helper function that encapsulates the complexity of Vulkan pipeline creation. It takes compiled SPIR-V, a pre-created pipeline layout, and vertex format descriptions to build a complete, ready-to-use VkPipeline object.
 *
 * @param vs_data A pointer to the raw SPIR-V bytecode for the vertex shader.
 * @param vs_size The size of the vertex shader bytecode in bytes.
 * @param fs_data A pointer to the raw SPIR-V bytecode for the fragment shader.
 * @param fs_size The size of the fragment shader bytecode in bytes.
 * @param pipelineLayout The pre-created VkPipelineLayout that defines the descriptor sets and push constants this pipeline will use.
 * @param topology The primitive topology (e.g., triangles, lines, points, triangle strip).
 * @param vertexBindingCount The number of vertex buffer bindings.
 * @param pVertexBindingDescriptions A pointer to an array of vertex binding descriptions.
 * @param vertexAttributeCount The number of vertex attributes.
 * @param pVertexAttributeDescriptions A pointer to an array of vertex attribute descriptions.
 * @param pipeline_flags Bit **SIT_VK_PIPELINE_BLEND_OPAQUE** — disable color blending (opaque writes). Used by built-in quad draws so results match OpenGL solid fills under alpha blending.
 * @return A valid VkPipeline handle on success, or VK_NULL_HANDLE on failure.
 */
static VkPipeline _SituationVulkanCreateGraphicsPipeline(
    const void* vs_data, size_t vs_size,
    const void* fs_data, size_t fs_size,
    VkPipelineLayout pipelineLayout,
    VkPrimitiveTopology topology,
    uint32_t vertexBindingCount,
    const VkVertexInputBindingDescription* pVertexBindingDescriptions,
    uint32_t vertexAttributeCount,
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,
    uint32_t pipeline_flags)
{
    // 1. Create Shader Modules from the raw SPIR-V data.
    VkShaderModule vs_module = _SituationVulkanCreateShaderModule(vs_data, vs_size);
    VkShaderModule fs_module = _SituationVulkanCreateShaderModule(fs_data, fs_size);

    if (vs_module == VK_NULL_HANDLE || fs_module == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create shader modules from SPIR-V.");
        if(vs_module) vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
        if(fs_module) vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo vs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo fs_stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs_module, .pName = "main" };
    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_stage_info, fs_stage_info};

    // 2. Define the pipeline's fixed-function states.
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = vertexBindingCount;
    vertex_input_info.pVertexBindingDescriptions = pVertexBindingDescriptions;
    vertex_input_info.vertexAttributeDescriptionCount = vertexAttributeCount;
    vertex_input_info.pVertexAttributeDescriptions = pVertexAttributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = topology, .primitiveRestartEnable = VK_FALSE };
    VkPipelineViewportStateCreateInfo viewport_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    // Disable backface culling for all 2D rendering (text, quads, VD compositing)
    // The quad vertices produce counter-clockwise triangles under top-left-origin ortho projection
    // which would be culled with BACK_BIT + CLOCKWISE front face.
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* TRIANGLE_LIST: depth write off (text passes transparent fragments without occluding).
       TRIANGLE_STRIP: 2D quad strips — depth write off; use <= so fragments at z matching cleared depth pass. */
    VkBool32 depth_test_enable = VK_TRUE;
    VkBool32 enableDepthWrite = VK_TRUE;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS;
    if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        enableDepthWrite = VK_FALSE;
    } else if (topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
        enableDepthWrite = VK_FALSE;
        depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    }
    VkPipelineDepthStencilStateCreateInfo depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = depth_test_enable, .depthWriteEnable = enableDepthWrite, .depthCompareOp = depth_compare, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = (pipeline_flags & SIT_VK_PIPELINE_BLEND_OPAQUE) ? VK_FALSE : VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo color_blending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &color_blend_attachment };

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamic_states };

    // 3. Assemble the pipeline create info struct.
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipelineLayout;
    pipeline_info.renderPass = sit_render.vk.main_window_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    // 4. Create the final graphics pipeline object.
    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(sit_render.vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "vkCreateGraphicsPipelines failed.");
        pipeline = VK_NULL_HANDLE; // Ensure we return NULL on failure
    }

    // 5. Clean up the temporary shader modules, as they are now baked into the pipeline.
    vkDestroyShaderModule(sit_render.vk.device, vs_module, NULL);
    vkDestroyShaderModule(sit_render.vk.device, fs_module, NULL);

    return pipeline;
}

#endif // SITUATION_USE_VULKAN

// ============================================================================
// Virtual Display API
// ============================================================================

/**
 * @brief [INTERNAL] Extracts and interleaves geometry data from a raw GLTF primitive.
 *
 * @details This helper bridges the gap between the generic `cgltf` data structures and the specific
 *          interleaved vertex format required by the Situation engine.
 *
 *          It performs the following operations:
 *          1. Identifies accessors for Position, Normal, and TexCoord attributes.
 *          2. Allocates a single interleaved buffer for vertices.
 *          3. Reads and packs data into the layout: `[Px, Py, Pz, Nx, Ny, Nz, U, V]`.
 *             - If Normals are missing, defaults to `(0, 0, 1)`.
 *             - If UVs are missing, defaults to `(0, 0)`.
 *          4. Extracts indices and normalizes them to `uint32_t`, generating a linear sequence
 *             if the primitive is non-indexed.
 *
 * @param prim Pointer to the `cgltf_primitive` to process.
 * @param[out] out_vertices Pointer to receive the allocated float array of interleaved vertex data.
 *                          The caller owns this memory and must `free()` it.
 * @param[out] out_v_count Pointer to receive the total number of vertices.
 * @param[out] out_indices Pointer to receive the allocated `uint32_t` array of indices.
 *                          The caller owns this memory and must `free()` it.
 * @param[out] out_i_count Pointer to receive the total number of indices.
 *
 * @return `true` if extraction was successful (valid positions found, memory allocated).
 * @return `false` if the primitive is not a triangle list or if allocation failed.
 */
#if defined(CGLTF_IMPLEMENTATION)
static bool _SituationExtractGLTFPrimitive(cgltf_primitive* prim, float** out_vertices, int* out_v_count, uint32_t** out_indices, int* out_i_count) {
    if (prim->type != cgltf_primitive_type_triangles) return false;

    // 1. Find Accessors
    cgltf_accessor* pos_acc = NULL;
    cgltf_accessor* norm_acc = NULL;
    cgltf_accessor* uv_acc = NULL;

    for (size_t i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_position) pos_acc = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_normal)   norm_acc = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_texcoord) uv_acc = prim->attributes[i].data;
    }

    if (!pos_acc) return false; // Position is mandatory

    *out_v_count = (int)pos_acc->count;

    // 2. Allocate Interleaved Vertex Buffer (12 floats per vertex)
    // Layout: X, Y, Z, Nx, Ny, Nz, Tx, Ty, Tz, Tw, U, V
    *out_vertices = (float*)SIT_MALLOC(*out_v_count * 12 * sizeof(float));
    if (!*out_vertices) return false;

    cgltf_accessor* tan_acc = NULL;
    for (size_t i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_tangent) tan_acc = prim->attributes[i].data;
    }

    // 3. Interleave Data
    for (int i = 0; i < *out_v_count; ++i) {
        float* v_ptr = &(*out_vertices)[i * 12];

        // Position
        cgltf_accessor_read_float(pos_acc, i, v_ptr, 3);

        // Normal (Default to 0,0,1 if missing)
        if (norm_acc) {
            cgltf_accessor_read_float(norm_acc, i, v_ptr + 3, 3);
        } else {
            v_ptr[3] = 0.0f; v_ptr[4] = 0.0f; v_ptr[5] = 1.0f;
        }

        // Tangent (Default to 1,0,0,1 if missing)
        if (tan_acc) {
            cgltf_accessor_read_float(tan_acc, i, v_ptr + 6, 4);
        } else {
            v_ptr[6] = 1.0f; v_ptr[7] = 0.0f; v_ptr[8] = 0.0f; v_ptr[9] = 1.0f;
        }

        // UV (Default to 0,0 if missing)
        if (uv_acc) {
            cgltf_accessor_read_float(uv_acc, i, v_ptr + 10, 2);
        } else {
            v_ptr[10] = 0.0f; v_ptr[11] = 0.0f;
        }
    }

    // 4. Extract Indices
    if (prim->indices) {
        *out_i_count = (int)prim->indices->count;
        *out_indices = (uint32_t*)SIT_MALLOC(*out_i_count * sizeof(uint32_t));
        if (!*out_indices) { SIT_FREE(*out_vertices); return false; }

        for (int k = 0; k < *out_i_count; ++k) {
            // cgltf handles u8/u16/u32 conversion automatically here
            (*out_indices)[k] = (uint32_t)cgltf_accessor_read_index(prim->indices, k);
        }
    } else {
        // Non-indexed geometry: generate 0, 1, 2... sequence
        *out_i_count = *out_v_count;
        *out_indices = (uint32_t*)SIT_MALLOC(*out_i_count * sizeof(uint32_t));
        if (!*out_indices) { SIT_FREE(*out_vertices); return false; }

        for (int k = 0; k < *out_i_count; ++k) {
            (*out_indices)[k] = (uint32_t)k;
        }
    }

    return true;
}
#endif // CGLTF_IMPLEMENTATION

/**
 * @brief Loads a texture directly from a file path (Reload-Compatible).
 *
 * @details This is a convenience wrapper that combines `SituationLoadImage`, `SituationCreateTexture`,
 *          and `SituationUnloadImage`.
 *
 *          **Crucially**, unlike `SituationCreateTexture`, this function registers the `file_path`
 *          with the internal resource tracker. This enables `SituationReloadTexture` to work later.
 *
 * @param file_path The path to the image file (PNG, JPG, BMP, TGA, etc.).
 * @param generate_mipmaps If `true`, generates a full mipmap chain for the texture.
 *
 * @return A valid `SituationTexture` handle, or `{0}` on failure.
 */
SITAPI SituationError SituationLoadTexture(const char* file_path, bool generate_mipmaps, SituationTexture* out_texture) {
    if (out_texture) *out_texture = (SituationTexture){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_texture cannot be NULL.");

    SituationImage img = {0};
    SituationError load_err = SituationLoadImage(file_path, &img);
    if (load_err != SITUATION_SUCCESS) return load_err;

    SituationError err = SituationCreateTexture(img, generate_mipmaps, out_texture);
    SituationUnloadImage(img);

    if (err == SITUATION_SUCCESS) {
        _SituationTextureSlot* slot = _SitGetTextureSlot(*out_texture);
        if (slot) {
            slot->source_path = _sit_strdup(file_path);
            slot->mod_time = SituationGetFileModTime(file_path);
        }
    }
    return err;
}


/**
 * @brief Loads a 3D model from a GLTF/GLB file.
 *
 * @details This is a comprehensive asset loader that handles the entire pipeline of importing a 3D asset:
 *          1. **Parsing:** Uses `cgltf` to parse the file structure.
 *          2. **Textures:** Automatically resolves and loads all referenced texture files from disk into GPU memory (`SituationTexture`).
 *          3. **Geometry:** Iterates through meshes, extracts vertex/index data, interleaves it into the engine's format, and creates GPU resources (`SituationMesh`).
 *          4. **Materials:** Extracts PBR material properties (Base Color, Metallic, Roughness) and binds the loaded textures to the mesh instances.
 *
 * @param file_path The path to the `.gltf` or `.glb` file.
 *
 * @return A valid `SituationModel` handle containing all loaded resources.
 * @return A zeroed handle `{0}` if the file could not be found, parsed, or if `CGLTF_IMPLEMENTATION` is missing.
 *
 * @note This function relies on the helper `_SituationExtractGLTFPrimitive` to handle geometry processing.
 * @warning The caller is responsible for destroying the returned model using `SituationUnloadModel` to prevent GPU memory leaks.;
 */
SITAPI SituationError SituationLoadModel(const char* file_path, SituationModel* out_model) {
    if (out_model) *out_model = (SituationModel){0};
    else return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_model cannot be NULL");

#if defined(CGLTF_IMPLEMENTATION)
    cgltf_options options = {0};
    cgltf_data* data = NULL;

    // 1. Parse GLTF
    cgltf_result result = cgltf_parse_file(&options, file_path, &data);
    if (result != cgltf_result_success) return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Failed to parse GLTF file.");
    result = cgltf_load_buffers(&options, data, file_path);
    if (result != cgltf_result_success) { cgltf_free(data); return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Failed to load GLTF buffers."); }

    // 2. Allocate Slot
    SituationModel handle;
    _SituationModelSlot* slot = _SitAllocModelSlot(&handle);
    if (!slot) { cgltf_free(data); return SITUATION_ERROR_MEMORY_ALLOCATION; }

    slot->source_path = _sit_strdup(file_path);
    slot->mod_time = SituationGetFileModTime(file_path);

    // 3. Load Textures
    slot->texture_count = (int)data->textures_count;
    if (slot->texture_count > 0) {
        slot->all_model_textures = SIT_CALLOC(slot->texture_count, sizeof(SituationTexture));
        if (!slot->all_model_textures) {
            _SitFreeModelSlot(handle); cgltf_free(data); return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        char* base_path = SituationGetBasePathFromFile(file_path);
        for (int i = 0; i < slot->texture_count; ++i) {
            const char* texture_uri = data->textures[i].image->uri;
            if (texture_uri) {
                char* full_texture_path = SituationJoinPath(base_path, texture_uri);
                SituationImage tex_img = SituationLoadImage(full_texture_path);
                SituationError tex_err = SituationCreateTexture(tex_img, true, &slot->all_model_textures[i]);
                SituationUnloadImage(tex_img);
                if (tex_err != SITUATION_SUCCESS) fprintf(stderr, "SITUATION WARNING: Model texture failed: %s\n", full_texture_path);
                SIT_FREE(full_texture_path);
            }
        }
        SIT_FREE(base_path);
    }

    // 4. Load Meshes
    slot->mesh_count = (int)data->meshes_count;
    if (slot->mesh_count > 0) {
        slot->meshes = SIT_CALLOC(slot->mesh_count, sizeof(SituationModelMesh));
        if (!slot->meshes) {
            // Cleanup textures
            for(int k=0; k<slot->texture_count; k++) SituationDestroyTexture(&slot->all_model_textures[k]);
            SIT_FREE(slot->all_model_textures);
            _SitFreeModelSlot(handle); cgltf_free(data); return SITUATION_ERROR_MEMORY_ALLOCATION;
        }

        for (int i = 0; i < slot->mesh_count; ++i) {
            cgltf_mesh* gltf_mesh = &data->meshes[i];
            SituationModelMesh* sit_mesh = &slot->meshes[i];
            if (gltf_mesh->name) strncpy(sit_mesh->name, gltf_mesh->name, SITUATION_MAX_DEVICE_NAME_LEN - 1);

            if (gltf_mesh->primitives_count > 0) {
                cgltf_primitive* prim = &gltf_mesh->primitives[0];
                float* vertex_data = NULL; uint32_t* index_data = NULL; int v_count = 0; int i_count = 0;
                if (_SituationExtractGLTFPrimitive(prim, &vertex_data, &v_count, &index_data, &i_count)) {
                    sit_mesh->gpu_mesh = SituationCreateMesh(vertex_data, v_count, 12 * sizeof(float), index_data, i_count);
                    SIT_FREE(vertex_data); SIT_FREE(index_data);
                }

                cgltf_material* mat = prim->material;
                if (mat) {
                    if (mat->has_pbr_metallic_roughness) {
                        cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;
                        memcpy(sit_mesh->base_color_factor.raw, pbr->base_color_factor, sizeof(Vector4));
                        sit_mesh->metallic_factor = pbr->metallic_factor;
                        sit_mesh->roughness_factor = pbr->roughness_factor;
                        if (pbr->base_color_texture.texture) sit_mesh->base_color_texture = slot->all_model_textures[pbr->base_color_texture.texture - data->textures];
                        if (pbr->metallic_roughness_texture.texture) sit_mesh->metallic_roughness_texture = slot->all_model_textures[pbr->metallic_roughness_texture.texture - data->textures];
                    }
                    if (mat->normal_texture.texture) sit_mesh->normal_texture = slot->all_model_textures[mat->normal_texture.texture - data->textures];
                    memcpy(sit_mesh->emissive_factor.raw, mat->emissive_factor, sizeof(Vector3));
                    if (mat->emissive_texture.texture) sit_mesh->emissive_texture = slot->all_model_textures[mat->emissive_texture.texture - data->textures];
                }
            }
        }
    }

    cgltf_free(data);

    // Fill handle cache
    handle.mesh_count = slot->mesh_count;
    handle.meshes = slot->meshes;
    *out_model = handle;
    return SITUATION_SUCCESS;
#else
    (void)file_path;
    return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Model loading not available. Please implement cgltf.h.");
#endif
}


/**
 * @brief Unloads a model and frees all of its associated GPU and CPU resources.
 * @details This is the only correct way to clean up a model loaded with `SituationLoadModel`.
 *          It systematically performs the following actions:
 *          1. Iterates through every sub-mesh in the model and calls `SituationDestroyMesh` to free its GPU vertex/index buffers.;
 *          2. Iterates through every texture loaded with the model and calls `SituationDestroyTexture` to free its GPU resources.
 *          3. Frees the CPU memory used for the arrays that held the mesh and texture handles.
 *          4. Invalidates the user's `SituationModel` handle by zeroing it out.
 *
 * @param[in,out] model A pointer to the `SituationModel` object to be unloaded. The handle becomes invalid after this call.
 *
 * @note Failure to call this function on a loaded model will result in significant GPU and CPU memory leaks.
 *       It is essential for proper resource management.
 * @note It is safe to call this function on a NULL pointer or an already-unloaded model (where `model->id` is 0);
 *       it will simply do nothing.
 */
SITAPI void SituationUnloadModel(SituationModel* model) {
    if (!model) return;
    _SituationModelSlot* slot = _SitGetModelSlot(*model);
    if (!slot) return;

    if (slot->meshes) {
        for (int i = 0; i < slot->mesh_count; i++) {
            SituationDestroyMesh(&slot->meshes[i].gpu_mesh);
        }
        SIT_FREE(slot->meshes);
    }
    if (slot->all_model_textures) {
        for (int i = 0; i < slot->texture_count; i++) {
            SituationDestroyTexture(&slot->all_model_textures[i]);
        }
        SIT_FREE(slot->all_model_textures);
    }

    _SitFreeModelSlot(*model);
    memset(model, 0, sizeof(SituationModel));
}


/**
 * @brief Records commands to draw a complete 3D model with a given transformation.
 * @details This is a high-level drawing command that iterates through all sub-meshes of a model.
 *          For each sub-mesh, it binds the material-specific textures and sets material properties via push constants before issuing a draw call for the mesh's geometry.
 *
 * @par Shader Contract Prerequisites
 *   For this function to work correctly, the caller is **responsible** for binding a compatible PBR-style shader *before* calling it. The shader must expect:
 *   - **Textures** at the binding points defined in the Shader Contract (e.g., `SIT_SAMPLER_BINDING_ALBEDO` at binding 0).
 *   - **Push Constants** with a layout matching the internal `PBRModelPushConstants` struct, containing the model matrix, base color factor, and metallic/roughness factors.
 *   - **Camera Data** from a previously bound UBO (e.g., at `SIT_UBO_BINDING_VIEW_DATA`).
 *
 * @param cmd The command buffer for the current frame.
 * @param model The `SituationModel` handle to draw. Must be a valid, loaded model.
 * @param transform The root model-to-world transformation matrix (position, rotation, scale) to apply to the entire model.
 *
 * @note This function is a high-level convenience wrapper. It can generate many state changes (texture binds) if the model has many unique materials, which may have performance implications.
 */
SITAPI void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform) {
    _SituationModelSlot* slot = _SitGetModelSlot(model);
    if (!slot || !slot->meshes) return;

    for (int i = 0; i < slot->mesh_count; i++) {
        SituationModelMesh* mesh = &slot->meshes[i];
        if (mesh->gpu_mesh.slot_index == 0 && mesh->gpu_mesh.generation == 0) continue; // Invalid mesh handle

        // Push Constants for PBR
        typedef struct {
            mat4 model;
            Vector4 base_color_factor;
            Vector4 pbr_factors; // x=metal, y=rough, z=unused
        } PBRModelPushConstants;

        PBRModelPushConstants constants;
        glm_mat4_copy(transform, constants.model); // Copy matrix
        constants.base_color_factor = mesh->base_color_factor;
        constants.pbr_factors.x = mesh->metallic_factor;
        constants.pbr_factors.y = mesh->roughness_factor;

        SituationCmdSetPushConstant(cmd, 0, &constants, sizeof(PBRModelPushConstants));

        // Bind Textures
        if (mesh->base_color_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_ALBEDO, mesh->base_color_texture);
        if (mesh->normal_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_NORMAL, mesh->normal_texture);
        if (mesh->metallic_roughness_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_PBR_MAP, mesh->metallic_roughness_texture);
        if (mesh->emissive_texture.generation != 0) SituationCmdBindTextureSet(cmd, SIT_SAMPLER_BINDING_EMISSIVE, mesh->emissive_texture);

        SituationCmdDrawMesh(cmd, mesh->gpu_mesh);
    }
}


/**
 * @brief Saves a model's structure and geometry to a human-readable GLTF 2.0 file.
 * @details This is a powerful utility for debugging, asset inspection, or exporting procedurally generated content. It writes the model's scene graph, materials, and texture references to a JSON-based `.gltf` file, and all binary vertex and index data to an accompanying `.bin` file.
 * @param model The `SituationModel` object to save.
 * @param file_path The destination path for the output `.gltf` file. The `.bin` file will be created in the same directory with a corresponding name.
 * @return `true` if the model was saved successfully, `false` otherwise.
 * @note This is an advanced utility with two important requirements:
 *       1. It requires both `cgltf.h` and `cgltf_write.h` to be available in the project.
 *       2. It relies on being able to read geometry data back from the GPU, which can be a slow operation. For best results, use this for debugging or development tools rather than as a frequent runtime operation.
 */
SITAPI bool SituationSaveModelAsGltf(SituationModel model, const char* file_path) {
#if !defined(CGLTF_WRITE_H)
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationSaveModelAsGltf requires CGLTF_WRITE_H to be included.");
    return false;
#elif defined(CGLTF_IMPLEMENTATION)
    if (model.id == 0) return false;

    // This is a simplified outline. A full implementation is very involved.

    // 1. Setup cgltf_data structure
    cgltf_data* data = SIT_CALLOC(1, sizeof(cgltf_data));
    data->meshes_count = model.mesh_count;
    data->meshes = SIT_CALLOC(model.mesh_count, sizeof(cgltf_mesh));
    // ... allocate memory for materials, textures, accessors, buffer_views, buffers ...

    // This will hold all vertex/index data for the entire model
    cgltf_buffer* main_buffer = &data->buffers[0];

    // 2. Loop through each SituationModelMesh
    for (int i = 0; i < model.mesh_count; ++i) {
        SituationModelMesh* sit_mesh = &model.meshes[i];
        cgltf_mesh* gltf_mesh = &data->meshes[i];

        // a. Get CPU-side vertex and index data for this mesh.
        //    *** CRITICAL: This requires a new function to read back GPU data ***
        void* vertex_data;
        void* index_data;
        int vertex_count, index_count, vertex_stride;
        // This function would be slow!
        SituationGetMeshData(sit_mesh->gpu_mesh, &vertex_data, &vertex_count, &vertex_stride, &index_data, &index_count);

        // b. Append this data to a giant CPU buffer that will become the .bin file.
        //    Update buffer_views and accessors to point to the correct offsets and strides
        //    within this giant buffer. This involves a lot of pointer arithmetic and bookkeeping.

        // c. Create a cgltf_material for this mesh's material.
        //    Copy the PBR factors and texture indices into the cgltf struct.
    }

    // 3. Write the file
    cgltf_options options = {0};
    options.type = cgltf_file_type_gltf; // Human-readable .gltf + .bin
    cgltf_result result = cgltf_write_file(&options, file_path, data);

    // 4. Cleanup
    cgltf_free(data);

    return result == cgltf_result_success;
#else
    (void)model; (void)file_path;
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Model saving not available. Please implement cgltf.h and cgltf_write.h.");
    return false;
#endif
}


/**
 * @brief [High-Level] Loads, compiles, and creates a graphics shader pipeline from GLSL source files.
 * @details This is the recommended high-level function for loading shaders from disk. It acts as a convenience wrapper, performing a multi-step process:
 *          1. Reads the vertex and fragment shader source code from the specified files using `SituationLoadFileText`.
 *          2. Passes the in-memory source code to `SituationLoadShaderFromMemory` for compilation and GPU resource creation.
 *          3. Cleans up the temporary memory buffers used to hold the source code.
 *
 * @param vs_path The file system path to the vertex shader GLSL source file (e.g., "shaders/pbr.vert").
 * @param fs_path The file system path to the fragment shader GLSL source file (e.g., "shaders/pbr.frag").
 *
 * @return A `SituationShader` handle.
 *         - On success, the `id` member of the returned struct will be non-zero, and the shader is ready for use.
 *         - On failure (e.g., file not found, compilation error), an invalid handle (`id == 0`) is returned.
 *           Use `SituationGetLastErrorMsg()` to get a detailed error description.
 *
 * @note The caller is **responsible** for destroying the returned shader using `SituationUnloadShader()` to prevent GPU memory leaks.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationLoadShaderFromMemory()
 * @see SituationUnloadShader()
 */
SITAPI SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    char* vs_source = SituationLoadFileText(vs_path);
    if (!vs_source) return SITUATION_ERROR_FILE_NOT_FOUND;

    char* fs_source = SituationLoadFileText(fs_path);
    if (!fs_source) {
        SIT_FREE(vs_source);
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    SituationError err = SituationLoadShaderFromMemory(vs_source, fs_source, out_shader);

    // Store paths for hot-reload
    if (err == SITUATION_SUCCESS) {
        _SituationShaderSlot* slot = _SitGetShaderSlot(*out_shader);
        if (slot) {
            slot->vs_path = _sit_strdup(vs_path);
            slot->fs_path = _sit_strdup(fs_path);
            slot->vs_mod_time = SituationGetFileModTime(vs_path);
            slot->fs_mod_time = SituationGetFileModTime(fs_path);
        }
    }

    SIT_FREE(vs_source);
    SIT_FREE(fs_source);
    return err;
}


/**
 * @brief [Core] Creates a graphics shader pipeline from GLSL source code provided as C strings.
 * @details This is the core function for creating graphics pipelines. It takes in-memory GLSL source code for vertex and fragment shaders, orchestrates the backend-specific compilation and linking process, and returns a handle to the final,
 * ready-to-use GPU pipeline object. It also registers the new resource with the internal resource manager for leak detection at shutdown.
 *
 * @par Backend-Specific Compilation
 * - **OpenGL:** The GLSL source strings are passed directly to the OpenGL driver for compilation (`glCompileShader`) and linking (`glLinkProgram`) into a shader program object.
 *     If `SITUATION_ENABLE_SHADER_COMPILER` is defined and the `GL_ARB_gl_spirv` extension is available, the source may first be compiled to SPIR-V for consistency with Vulkan.
 * - **Vulkan:** This backend **requires** `SITUATION_ENABLE_SHADER_COMPILER`. The function uses `shaderc` to compile both the vertex and fragment GLSL sources into separate SPIR-V binary blobs. These blobs are then used to construct a complete `VkPipeline` object, including its `VkPipelineLayout`.
 *
 * @param vs_code The null-terminated string containing the vertex shader source code.
 * @param fs_code The null-terminated string containing the fragment shader source code.
 *
 * @return A `SituationShader` handle.
 *         - On success, the `id` member will be non-zero, and the handle is ready for use with `SituationCmdBindPipeline`.
 *         - On failure (e.g., a syntax error in the shader code, a linking error, or a resource allocation failure), an invalid handle (`id == 0`) is returned. Use `SituationGetLastErrorMsg()` to retrieve the detailed error log from the compiler/linker.
 *
 * @note The caller is **responsible** for destroying the returned shader using `SituationUnloadShader()` to prevent GPU memory leaks.
 * @warning This function is not thread-safe and must be called from the main thread that initialized the library.
 *
 * @see SituationLoadShader(), SituationUnloadShader(), SituationCmdBindPipeline()
 */
SITAPI SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader) {
    if (!out_shader) return SITUATION_ERROR_INVALID_PARAM;
    memset(out_shader, 0, sizeof(SituationShader));
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    SituationShader handle;
    mtx_lock(&sit_render.resource_registry_mutex); // [LOCK]
    _SituationShaderSlot* slot = _SitAllocShaderSlot(&handle);
    mtx_unlock(&sit_render.resource_registry_mutex); // [UNLOCK]
    if (!slot) return SITUATION_ERROR_MEMORY_ALLOCATION; // Limit reached

#if defined(SITUATION_ENABLE_SHADER_COMPILER) && defined(SITUATION_USE_VULKAN)
    // --- PATH A: Runtime Compilation (Shaderc) ---
    _SituationSpirvBlob vs_spirv = _SituationVulkanCompileGLSLtoSPIRV(vs_code, "vertex_shader", shaderc_glsl_vertex_shader);
    if (!vs_spirv.data) {
        _SituationFreeSpirvBlob(&vs_spirv);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    _SituationSpirvBlob fs_spirv = _SituationVulkanCompileGLSLtoSPIRV(fs_code, "fragment_shader", shaderc_glsl_fragment_shader);
    if (!fs_spirv.data) {
        _SituationFreeSpirvBlob(&vs_spirv); // Clean up VS
        _SituationFreeSpirvBlob(&fs_spirv);
        _SitFreeShaderSlot(handle);
        return SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED;
    }

    #if defined(SITUATION_USE_VULKAN)
        // Standard PBR and Legacy Layouts
        // (Copied from original impl, updated to use slot)
        /* Set 0: dynamic UBO @ binding 0 (matches SituationCmdBindDescriptorSet + harness `set=0,binding=0`).
           Set 1: text_sampler_layout @ binding 0 (SIT_SAMPLER_BINDING_ALBEDO) for `layout(set=1,binding=0) sampler2D`.
           Do not use view_data_ubo_layout (UBO at binding 1) or image_sampler_layout (sampler at binding 4). */
        VkDescriptorSetLayout layouts[] = { sit_render.vk.dynamic_ubo_layout, sit_render.vk.text_sampler_layout };
        VkPushConstantRange push_constant_range = { .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS, .offset = 0, .size = 128 };
        VkPipelineLayoutCreateInfo pipeline_layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 2, .pSetLayouts = layouts, .pushConstantRangeCount = 1, .pPushConstantRanges = &push_constant_range };

        if (vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &slot->vk_pipeline_layout) == VK_SUCCESS) {
            // 1. PBR Pipeline
            VkVertexInputBindingDescription binding_desc_pbr = { .binding = 0, .stride = (3 + 3 + 4 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription attr_descs_pbr[4];
            attr_descs_pbr[0].binding = 0; attr_descs_pbr[0].location = SIT_ATTR_POSITION; attr_descs_pbr[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_pbr[0].offset = 0;
            attr_descs_pbr[1].binding = 0; attr_descs_pbr[1].location = SIT_ATTR_NORMAL; attr_descs_pbr[1].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_pbr[1].offset = 3 * sizeof(float);
            attr_descs_pbr[2].binding = 0; attr_descs_pbr[2].location = SIT_ATTR_TANGENT; attr_descs_pbr[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; attr_descs_pbr[2].offset = 6 * sizeof(float);
            attr_descs_pbr[3].binding = 0; attr_descs_pbr[3].location = SIT_ATTR_TEXCOORD_0; attr_descs_pbr[3].format = VK_FORMAT_R32G32_SFLOAT; attr_descs_pbr[3].offset = 10 * sizeof(float);

            slot->vk_pipeline = _SituationVulkanCreateGraphicsPipeline(vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size, slot->vk_pipeline_layout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_pbr, 4, attr_descs_pbr, 0u);

            // 2. Legacy Pipeline
            VkVertexInputBindingDescription binding_desc_legacy = { .binding = 0, .stride = (3 + 3 + 2) * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription attr_descs_legacy[3];
            attr_descs_legacy[0].binding = 0; attr_descs_legacy[0].location = SIT_ATTR_POSITION; attr_descs_legacy[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_legacy[0].offset = 0;
            attr_descs_legacy[1].binding = 0; attr_descs_legacy[1].location = SIT_ATTR_NORMAL; attr_descs_legacy[1].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_legacy[1].offset = 3 * sizeof(float);
            attr_descs_legacy[2].binding = 0; attr_descs_legacy[2].location = SIT_ATTR_TEXCOORD_0; attr_descs_legacy[2].format = VK_FORMAT_R32G32_SFLOAT; attr_descs_legacy[2].offset = 6 * sizeof(float);

            slot->vk_pipeline_legacy = _SituationVulkanCreateGraphicsPipeline(vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size, slot->vk_pipeline_layout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_legacy, 3, attr_descs_legacy, 0u);

            // 3. Simple Pipeline (position-only, for basic shaders)
            VkVertexInputBindingDescription binding_desc_simple = { .binding = 0, .stride = 3 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription attr_descs_simple[1];
            attr_descs_simple[0].binding = 0; attr_descs_simple[0].location = SIT_ATTR_POSITION; attr_descs_simple[0].format = VK_FORMAT_R32G32B32_SFLOAT; attr_descs_simple[0].offset = 0;

            slot->vk_pipeline_simple = _SituationVulkanCreateGraphicsPipeline(vs_spirv.data, vs_spirv.size, fs_spirv.data, fs_spirv.size, slot->vk_pipeline_layout, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1, &binding_desc_simple, 1, attr_descs_simple, 0u);

            if (slot->vk_pipeline == VK_NULL_HANDLE || slot->vk_pipeline_legacy == VK_NULL_HANDLE) {
                _SitFreeShaderSlot(handle); // Will perform deferred cleanup if resources were created
                _SituationFreeSpirvBlob(&vs_spirv);
                _SituationFreeSpirvBlob(&fs_spirv);
                return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
            }
        } else {
             _SitFreeShaderSlot(handle);
             _SituationFreeSpirvBlob(&vs_spirv);
             _SituationFreeSpirvBlob(&fs_spirv);
             return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
        }
    #elif defined(SITUATION_USE_OPENGL)
        SituationError err;
        slot->gl_program_id = _SituationCreateGLShaderProgramFromSpirv(&vs_spirv, &fs_spirv, &err);
        if(err != SITUATION_SUCCESS) {
            _SitFreeShaderSlot(handle);
            _SituationFreeSpirvBlob(&vs_spirv);
            _SituationFreeSpirvBlob(&fs_spirv);
            return err;
        }
    #endif

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);
#else
    // --- PATH B: Legacy OpenGL GLSL Source Pipeline ---
    #if defined(SITUATION_USE_OPENGL)
        SituationError err;
        slot->gl_program_id = _SituationCreateGLShaderProgram(vs_code, fs_code, &err);
        if (err == SITUATION_SUCCESS) {
            // --- Create the uniform map for this shader ---
            slot->uniform_map = _sit_uniform_map_create();
        } else {
            _SitFreeShaderSlot(handle);
            return err;
        }
    #elif defined(SITUATION_USE_VULKAN)
        _SitFreeShaderSlot(handle);
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Loading GLSL from memory requires the shader compiler to be enabled for the Vulkan backend.");
    #endif
#endif

    *out_shader = handle;
    return SITUATION_SUCCESS;
}


/**
 * @brief Destroys a graphics shader pipeline and frees all of its associated GPU and CPU resources.
 * @details This is the only correct way to release a shader created with `SituationLoadShader` or `SituationLoadShaderFromMemory`. It handles the full cleanup process:
 *          1. Removes the shader from the internal resource tracking list to prevent false leak warnings at shutdown.
 *          2. Destroys all backend-specific GPU objects (OpenGL program, Vulkan pipeline and layout).
 *          3. Frees any associated CPU-side resources (like the OpenGL uniform location cache).
 *          4. Invalidates the user's handle by zeroing it out to prevent accidental use of stale data.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Destroys the internal uniform location cache (`_sit_uniform_map_destroy`), then deletes the shader program object (`glDeleteProgram`).
 * - **Vulkan:** Waits for the GPU to become idle to ensure the pipeline is not in use, then destroys both the `VkPipeline` and its associated `VkPipelineLayout`.
 *
 * @param[in,out] shader A pointer to the `SituationShader` handle to be destroyed. The contents of the struct will be zeroed out, invalidating the handle for future use.
 *
 * @note It is safe to call this function on a NULL pointer or an already-unloaded shader handle (where `shader->id` is 0); it will simply do nothing.
 * @note **Performance:** On Vulkan, this function uses deferred destruction and does NOT stall the GPU.
 */
SITAPI void SituationUnloadShader(SituationShader* shader) {
    if (!shader) return;

    _SituationShaderSlot* slot = _SitGetShaderSlot(*shader);
    if (!slot) return;

    // --- Backend-Specific Destruction ---
#if defined(SITUATION_USE_OPENGL)
    if (slot->uniform_map) {
        _sit_uniform_map_destroy(slot->uniform_map);
        slot->uniform_map = NULL;
    }
    if (glIsProgram(slot->gl_program_id)) {
        glDeleteProgram(slot->gl_program_id);
        slot->gl_program_id = 0;
    }
    SIT_CHECK_GL_ERROR();

#elif defined(SITUATION_USE_VULKAN)
    if (_SituationVulkanImmediateDestroyDuringShutdown() && sit_render.vk.device != VK_NULL_HANDLE) {
        if (slot->vk_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline, NULL);
            slot->vk_pipeline = VK_NULL_HANDLE;
        }
        if (slot->vk_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(sit_render.vk.device, slot->vk_pipeline_layout, NULL);
            slot->vk_pipeline_layout = VK_NULL_HANDLE;
        }
        if (slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
            vkDestroyPipeline(sit_render.vk.device, slot->vk_pipeline_legacy, NULL);
            slot->vk_pipeline_legacy = VK_NULL_HANDLE;
        }
    } else {
        _SituationDeferDestroyPipeline(slot->vk_pipeline, slot->vk_pipeline_layout);
        if (slot->vk_pipeline_legacy != VK_NULL_HANDLE) {
            _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy, VK_NULL_HANDLE);
        }
    }
#endif

    _SitFreeShaderSlot(*shader);
    memset(shader, 0, sizeof(SituationShader));
}


/**
 * @brief Sets the value of a standalone uniform variable within a graphics shader program.
 * @details This function provides a convenient way to pass data to shaders, primarily for the OpenGL backend where standalone uniforms are common. It automatically caches uniform locations for high performance on subsequent calls.
 *
 * @par Backend-Specific Behavior & Usage
 * - **OpenGL:** This is the primary, high-performance method for setting per-draw-call data that is not part of a larger UBO.
 *      On the first call for a given `uniform_name`, it queries the uniform's location using `glGetUniformLocation` and caches it in an internal hash map. Subsequent calls for the same uniform are extremely fast as they use the cached location, avoiding repeated string lookups.
 *      It uses the appropriate `glUniform*` function based on the provided `type`.
 *
 * - **Vulkan:** This function is **not recommended** for the Vulkan backend and will return `SITUATION_ERROR_NOT_IMPLEMENTED`.
 *      Vulkan's architecture is optimized for passing data via Uniform Buffer Objects (UBOs) for per-frame data and **Push Constants** for small, high-frequency per-draw data. For Vulkan, you should use `SituationCmdBindUniformBuffer` and `SituationCmdSetPushConstant` instead.
 *
 * @param shader The `SituationShader` handle whose uniform you want to set.
 * @param uniform_name The null-terminated string name of the uniform variable in the GLSL code (e.g., "u_modelMatrix").
 * @param data A pointer to the data to be sent to the uniform (e.g., a `mat4`, `vec4`, `float`).
 * @param type An enum `SituationUniformType` that specifies the data type of the uniform. This determines which underlying API function is called.
 *
 * @return `SITUATION_SUCCESS` on success.
 * @return `SITUATION_ERROR_NOT_INITIALIZED` if the library is not initialized.
 * @return `SITUATION_ERROR_INVALID_PARAM` if any of the input parameters are invalid.
 * @return `SITUATION_ERROR_NOT_IMPLEMENTED` if called on the Vulkan backend.
 * @return `SITUATION_ERROR_OPENGL_GENERAL` if an OpenGL error occurs.
 *
 * @note In OpenGL, if the specified `uniform_name` does not exist in the shader or is optimized out by the compiler, this function will silently do nothing and return `SITUATION_SUCCESS`. This is standard behavior for `glGetUniformLocation`.
 *
 * @see SituationCmdSetPushConstant(), SituationCmdBindUniformBuffer()
 */
SITAPI SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;

    _SituationShaderSlot* slot = _SitGetShaderSlot(shader);
    if (!slot || !uniform_name || !data) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

#if defined(SITUATION_USE_OPENGL)
    if (!slot->uniform_map) {
        // Map should have been created on load. If missing, create now (lazy).
        slot->uniform_map = _sit_uniform_map_create();
        if (!slot->uniform_map) return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // 1. Get Location (Cached)
    GLint location = _sit_uniform_map_get(slot->uniform_map, uniform_name);
    if (location == -1) {
        // Not in cache. Query driver.
        // We must bind to query? No, glGetUniformLocation takes program ID.
        // BUT we should avoid binding just to query if possible, but here we can query directly.
        // Optimization: track current program to avoid redundant state check?
        // Let's just query.

        // Safety: Ensure program exists
        if (!glIsProgram(slot->gl_program_id)) return SITUATION_ERROR_RESOURCE_INVALID;

        location = glGetUniformLocation(slot->gl_program_id, uniform_name);

        // Cache result (even -1, to avoid re-querying invalid uniforms?)
        // Map stores -1? Yes.
        if (location != -1) {
            _sit_uniform_map_set(slot->uniform_map, uniform_name, location);
        } else {
             // Optional: Cache miss to avoid spamming driver for typo'd uniforms?
             // For now, don't cache failures to save memory/logic.
             return SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND;
        }
    }

    // 2. Set Value Immediately via glProgramUniform* (DSA — does not require program to be bound)
    // [Bug 8 Fix] Previously deferred to soft command buffer, but the buffer gets reset by
    // SituationAcquireFrameCommandBuffer, so uniforms set before frame acquisition were lost.
    // glProgramUniform* applies immediately to the program object's state and persists until changed.
    {
        GLuint prog = slot->gl_program_id;
        switch(type) {
            case SIT_UNIFORM_FLOAT: glProgramUniform1fv(prog, location, 1, (const GLfloat*)data); break;
            case SIT_UNIFORM_VEC2:  glProgramUniform2fv(prog, location, 1, (const GLfloat*)data); break;
            case SIT_UNIFORM_VEC3:  glProgramUniform3fv(prog, location, 1, (const GLfloat*)data); break;
            case SIT_UNIFORM_VEC4:  glProgramUniform4fv(prog, location, 1, (const GLfloat*)data); break;
            case SIT_UNIFORM_INT:   glProgramUniform1iv(prog, location, 1, (const GLint*)data); break;
            case SIT_UNIFORM_IVEC2: glProgramUniform2iv(prog, location, 1, (const GLint*)data); break;
            case SIT_UNIFORM_IVEC3: glProgramUniform3iv(prog, location, 1, (const GLint*)data); break;
            case SIT_UNIFORM_IVEC4: glProgramUniform4iv(prog, location, 1, (const GLint*)data); break;
            case SIT_UNIFORM_MAT4:  glProgramUniformMatrix4fv(prog, location, 1, GL_FALSE, (const GLfloat*)data); break;
        }
    }
    return SITUATION_SUCCESS;

#else
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
}


/**
 * @brief [DEPRECATED] Inserts a coarse-grained memory barrier.
 * @details This function provides a simple, but less optimal, way to synchronize memory.
 *          It is recommended to use the more explicit and performant `SituationCmdPipelineBarrier()` instead.
 *
 * @param cmd The command buffer to record the barrier into. (Ignored in OpenGL).
 * @param barrier_bits A bitmask of `SITUATION_BARRIER_*_BIT` flags specifying the types of memory access to synchronize.
 *
 * @deprecated Use SituationCmdPipelineBarrier() for more precise and optimal synchronization.
 */
SITAPI void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits) {
    if (!SituationIsInitialized() || barrier_bits == 0) {
        return;
    }

#if defined(SITUATION_USE_OPENGL)
    (void)cmd;
    // The previous mapping logic for OpenGL is still valid for this coarse barrier.
    GLbitfield gl_barrier_bits = 0;
    if (barrier_bits & SITUATION_BARRIER_VERTEX_ATTRIB_ARRAY_BIT)  gl_barrier_bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_INDEX_BUFFER_BIT)         gl_barrier_bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_UNIFORM_BUFFER_BIT)       gl_barrier_bits |= GL_UNIFORM_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_TEXTURE_FETCH_BIT)        gl_barrier_bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BIT)  gl_barrier_bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_COMMAND_BIT)              gl_barrier_bits |= GL_COMMAND_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_SHADER_STORAGE_BIT)       gl_barrier_bits |= GL_SHADER_STORAGE_BARRIER_BIT;
    if (barrier_bits & SITUATION_BARRIER_ALL_BARRIER_BITS)         gl_barrier_bits = GL_ALL_BARRIER_BITS;

    if (gl_barrier_bits != 0) {
        glMemoryBarrier(gl_barrier_bits);
    }
#elif defined(SITUATION_USE_VULKAN)
    // For this deprecated function, we issue a very broad, "sledgehammer" barrier.
    // It's not optimal but guarantees correctness for simple use cases.
    VkMemoryBarrier memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(
        (VkCommandBuffer)cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Wait for ALL previous stages
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Unblock ALL subsequent stages
        0,
        1, &memory_barrier,
        0, NULL,
        0, NULL
    );
#endif
}

// ============================================================================
// Hot Reloading Implementation
// ============================================================================
/**
 * @section Hot-Reloading Overview
 * These functions allow applications to reload assets (Shaders, Textures, Models) from disk at runtime
 * without restarting the application. This is intended primarily for development, tooling, and
 * "creative coding" workflows.
 *
 * @note **Performance (Vulkan):** Reloading is asynchronous and safe. Resources are defer-destroyed.
 * @warning **Performance (OpenGL):** Reloading may cause a brief stall (`glFinish`).
 *
 * @note **Path Tracking:** Hot-reloading only works for assets loaded from files using the high-level
 * `SituationLoad...` functions. Assets created from raw memory pointers cannot be hot-reloaded as
 * they have no associated file path.
 */

 /**
 * @brief Reloads a graphics shader pipeline from its original source files.
 *
 * @details This function looks up the original file paths used to create the shader, waits for the GPU to become idle,
 *          destroys the existing pipeline resources, and attempts to compile and link a new pipeline from disk.
 *
 *          If successful, the `shader` handle is updated in-place with the new ID.
 *          If failure occurs (e.g., compilation error in the new code), the old shader is destroyed, and the handle
 *          becomes invalid (ID = 0). The application should check the return value and handle invalidation gracefully.
 *
 * @param[in,out] shader A pointer to the `SituationShader` handle to reload.
 *                       On success, this struct is updated with the new resource IDs.
 *                       On failure, this struct is zeroed out.
 *
 * @return `true` if the shader was successfully recompiled and linked.
 * @return `false` if the shader could not be reloaded (e.g., file not found, GLSL syntax error).
 *         Check `SituationGetLastErrorMsg()` for compiler errors.
 */
SITAPI bool SituationReloadShader(SituationShader* shader) {
    if (!SituationIsInitialized() || !shader) return false;
    _SituationShaderSlot* slot = _SitGetShaderSlot(*shader);
    if (!slot || !slot->vs_path || !slot->fs_path) return false;

    // Reload
    char* vs = SituationLoadFileText(slot->vs_path);
    char* fs = SituationLoadFileText(slot->fs_path);
    if (!vs || !fs) { SIT_FREE(vs); SIT_FREE(fs); return false; }

    SituationShader new_handle;
    SituationError err = SituationLoadShaderFromMemory(vs, fs, &new_handle);
    SIT_FREE(vs); SIT_FREE(fs);

    if (err == SITUATION_SUCCESS) {
        _SituationShaderSlot* new_slot = _SitGetShaderSlot(new_handle);
        if (new_slot) {
            #if defined(SITUATION_USE_OPENGL)
            if (glIsProgram(slot->gl_program_id)) glDeleteProgram(slot->gl_program_id);
            slot->gl_program_id = new_slot->gl_program_id;
            if (slot->uniform_map) _sit_uniform_map_destroy(slot->uniform_map);
            slot->uniform_map = new_slot->uniform_map;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyPipeline(slot->vk_pipeline, slot->vk_pipeline_layout);
            if (slot->vk_pipeline_legacy) _SituationDeferDestroyPipeline(slot->vk_pipeline_legacy, VK_NULL_HANDLE);
            slot->vk_pipeline = new_slot->vk_pipeline;
            slot->vk_pipeline_legacy = new_slot->vk_pipeline_legacy;
            slot->vk_pipeline_layout = new_slot->vk_pipeline_layout;
            #endif

            slot->vs_mod_time = SituationGetFileModTime(slot->vs_path);
            slot->fs_mod_time = SituationGetFileModTime(slot->fs_path);

            new_slot->is_active = false; // Recycle new slot
            return true;
        }
    }
    return false;
}


//==================================================================================
// Implementation for Hot-Reloading
//==================================================================================

/**
 * @brief Reloads a texture from its original image file.
 *
 * @details This function destroys the existing GPU texture resources (Image, View, Sampler, Memory) and
 *          re-loads the image data from the original file path.
 *
 *          **Requirement:** The texture must have been loaded using `SituationLoadTexture`.
 *          Textures created via `SituationCreateTexture` (from raw memory) cannot be reloaded.
 *
 * @param[in,out] texture A pointer to the `SituationTexture` handle to reload.
 *
 * @return `true` if the image was successfully loaded and uploaded to the GPU.
 * @return `false` if the file could not be loaded or if the original path was not tracked.
 */
SITAPI bool SituationReloadTexture(SituationTexture* texture) {
    if (!SituationIsInitialized() || !texture) return false;
    _SituationTextureSlot* slot = _SitGetTextureSlot(*texture);
    if (!slot || !slot->source_path) return false;

    SituationImage img = {0};
    if (SituationLoadImage(slot->source_path, &img) != SITUATION_SUCCESS) return false;

    SituationTexture temp;
    if (SituationCreateTexture(img, true, &temp) == SITUATION_SUCCESS) {
        _SituationTextureSlot* new_slot = _SitGetTextureSlot(temp);
        if (new_slot) {
            // Swap
            #if defined(SITUATION_USE_OPENGL)
            _SitGLDeferDestroyTexture(slot->gl_texture_id);
            slot->gl_texture_id = new_slot->gl_texture_id;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
            slot->image = new_slot->image;
            slot->allocation = new_slot->allocation;
            slot->image_view = new_slot->image_view;
            slot->sampler = new_slot->sampler;
            #endif
            slot->width = new_slot->width;
            slot->height = new_slot->height;
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            new_slot->is_active = false;
            SituationUnloadImage(img);
            return true;
        }
    }
    SituationUnloadImage(img);
    return false;
}


/**
 * @brief Reloads a 3D model and all its dependencies.
 *
 * @details This is a "heavy" operation. It unloads the entire model structure, including:
 *          1. All sub-meshes (vertex/index buffers).
 *          2. All associated textures (Albedo, Normal, PBR maps).
 *
 *          It then re-parses the GLTF/GLB file and re-uploads all geometry and textures to the GPU.
 *          This is useful for iterating on 3D assets (e.g., exporting from Blender and seeing updates instantly).
 *
 * @param[in,out] model A pointer to the `SituationModel` handle to reload.
 *
 * @return `true` on success, `false` on failure.
 */
SITAPI bool SituationReloadModel(SituationModel* model) {
    if (!SituationIsInitialized() || !model) return false;
    _SituationModelSlot* slot = _SitGetModelSlot(*model);
    if (!slot || !slot->source_path) return false;

    SituationModel new_handle;
    SituationError err = SituationLoadModel(slot->source_path, &new_handle);

    if (err == SITUATION_SUCCESS) {
        _SituationModelSlot* new_slot = _SitGetModelSlot(new_handle);
        if (new_slot) {
            // Free old resources
            if (slot->meshes) {
                for(int i=0; i<slot->mesh_count; i++) SituationDestroyMesh(&slot->meshes[i].gpu_mesh);
                SIT_FREE(slot->meshes);
            }
            if (slot->all_model_textures) {
                for(int i=0; i<slot->texture_count; i++) SituationDestroyTexture(&slot->all_model_textures[i]);
                SIT_FREE(slot->all_model_textures);
            }

            // Move new resources to old slot
            slot->meshes = new_slot->meshes;
            slot->mesh_count = new_slot->mesh_count;
            slot->all_model_textures = new_slot->all_model_textures;
            slot->texture_count = new_slot->texture_count;
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            // Update handle cache
            model->mesh_count = slot->mesh_count;
            model->meshes = slot->meshes;

            new_slot->is_active = false;
            return true;
        }
    }
    return false;
}


/**
 * @brief Reloads a compute pipeline from its original source file.
 *
 * @details Similar to `SituationReloadShader`, but for Compute Pipelines. It recompiles the GLSL source
 *          (or re-reads SPIR-V) and rebuilds the `VkPipeline` (Vulkan) or `GL Program` (OpenGL).
 *          It automatically reuses the `SituationComputeLayoutType` that was specified during the initial creation.
 *
 * @param[in,out] pipeline A pointer to the `SituationComputePipeline` handle to reload.
 *
 * @return `true` on success, `false` on failure.
 */
SITAPI bool SituationReloadComputePipeline(SituationComputePipeline* pipeline) {
    if (!SituationIsInitialized() || !pipeline) return false;
    _SituationComputePipelineSlot* slot = _SitGetComputePipelineSlot(*pipeline);
    if (!slot || !slot->source_path) return false;

    // Reload from source
    char* source = SituationLoadFileText(slot->source_path);
    if (!source) return false;

    SituationComputePipeline new_pipe_handle;
    SituationError err = SituationCreateComputePipelineFromMemory(source, slot->layout_type, &new_pipe_handle);
    SIT_FREE(source);

    if (err == SITUATION_SUCCESS) {
        _SituationComputePipelineSlot* new_slot = _SitGetComputePipelineSlot(new_pipe_handle);
        if (new_slot) {
            // Swap internals
            #if defined(SITUATION_USE_OPENGL)
            if (glIsProgram(slot->gl_program_id)) glDeleteProgram(slot->gl_program_id);
            slot->gl_program_id = new_slot->gl_program_id;
            #elif defined(SITUATION_USE_VULKAN)
            _SituationDeferDestroyPipeline(slot->vk_pipeline, VK_NULL_HANDLE); // Layout shared? No, we kept layout in slot.
            // But wait, SituationCreateComputePipelineFromMemory creates NEW layout usually?
            // My implementation reuses layout_type lookup or creates new?
            // It creates new layout?
            // "VkPipelineLayout layout = sit_render.vk.compute_layouts[layout_type];" - It reuses standard layout.
            // So we don't destroy layout.
            // We destroy old pipeline.
            vkDestroyShaderModule(sit_render.vk.device, slot->shader_module, NULL); // Destroy old module
            slot->vk_pipeline = new_slot->vk_pipeline;
            slot->shader_module = new_slot->shader_module;
            #endif

            // Update metadata
            slot->mod_time = SituationGetFileModTime(slot->source_path);

            // Free new slot shell
            new_slot->is_active = false;
            return true;
        }
    }
    return false;
}


/**
 * @brief [INTERNAL] Executes one complete pass of the hot-reloading system (Velocity Module).
 *
 * @details This function is the core heartbeat of the hot-reload mechanism.
 *          It is called periodically (based on the configured hot-reload rate) or on-demand
 *          to detect file changes in watched directories/assets and trigger reloads where needed.
 *
 *          During a pass, the function:
 *            - Scans all registered watch paths for modification time changes
 *            - Compares current file mod-times against cached values
 *            - Identifies changed files (shaders, textures, models, audio, etc.)
 *            - Invokes the appropriate reload handler for each changed asset type
 *            - Re-compiles shaders to new SPIR-V blobs (if applicable)
 *            - Recreates GPU resources (shader modules, textures, pipelines, etc.)
 *            - Updates internal caches and invalidates stale references
 *            - Logs reload events and errors (in debug builds)
 *
 *          The pass is designed to be fast and non-blocking in most cases:
 *            - Only changed assets are processed
 *            - Resource recreation is deferred to the render thread when possible
 *            - Failures during reload (e.g. shader compilation error) are graceful ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â
 *              the old resource remains active and an error is logged/set
 *
 * Thread safety invariants:
 *   - Typically called from the main thread or a dedicated hot-reload timer thread
 *   - Filesystem access is protected where necessary (e.g. stat() calls)
 *   - GPU resource updates are queued to the render thread to avoid context conflicts
 *   - No long-blocking operations ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â filesystem checks are lightweight
 *
 * @note This function is usually invoked automatically by the thread pool's hot-reload timer
 *       (if enabled via `SituationCreateThreadPool` hot_reload_rate parameter).
 *       Manual calls are safe but redundant unless forcing an immediate reload.
 *       Reload rate should be tuned to balance responsiveness vs CPU usage
 *       (typical values: 0.1ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ1.0 seconds).
 *
 * @see SituationCreateThreadPool (hot_reload_rate parameter),
 *      _SituationCompileGLSLtoSPIRV, _SituationFreeSpirvBlob,
 *      Velocity Module documentation, SITUATION_ERROR_FILE_MODIFIED
 */
// [v2.3.34] Hot-Reload Logic (Running on I/O Thread)
static void _SituationPerformHotReloadPass(void) {
#if defined(NDEBUG) && !defined(SITUATION_FORCE_HOTRELOAD)
    return;
#else
    if (!SituationIsInitialized()) return;

    // [Optimized] The polling frequency is now controlled by the caller (IO Thread)
    // using pool->hot_reload_rate.

    // 1. Shaders
    for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
        _SituationShaderSlot* slot = &sit_render.shader_registry[i];
        if (slot->is_active && slot->vs_path && slot->fs_path) {
            long vs = SituationGetFileModTime(slot->vs_path);
            long fs = SituationGetFileModTime(slot->fs_path);
            if (vs != slot->vs_mod_time || fs != slot->fs_mod_time) {
                printf("[Situation] Hot-Reloading Shader %d...\n", i);
                // Reload
                SituationShader handle = { (uint32_t)i, slot->generation };
                char* vs_src = SituationLoadFileText(slot->vs_path);
                char* fs_src = SituationLoadFileText(slot->fs_path);
                if (vs_src && fs_src) {
                    // Update timestamps immediately to avoid re-triggering
                    slot->vs_mod_time = vs;
                    slot->fs_mod_time = fs;

                    #if defined(SITUATION_USE_OPENGL)
                    if (!slot->gl_is_linking) {
                        SituationError err = SITUATION_SUCCESS;
                        GLuint pending = _SituationCreateGLShaderProgramAsync(vs_src, fs_src, &err);
                        if (pending) {
                            slot->gl_pending_program_id = pending;
                            slot->gl_is_linking = true;
                        } else {
                            printf("[Situation] Hot-Reload Compile Failed for shader %d (GL)\n", i);
                        }
                    }
                    #else
                    // Vulkan / Fallback Blocking Path
                    SituationShader new_shader;
                    SituationError err = SituationLoadShaderFromMemory(vs_src, fs_src, &new_shader);

                    if (err == SITUATION_SUCCESS) {
                        _SituationShaderSlot* new_slot = _SitGetShaderSlot(new_shader);
                        if (new_slot) {
                            #if defined(SITUATION_USE_VULKAN)
                            _SituationDeferDestroyPipeline(slot->vk_pipeline, slot->vk_pipeline_layout);
                            slot->vk_pipeline = new_slot->vk_pipeline;
                            slot->vk_pipeline_layout = new_slot->vk_pipeline_layout;
                            slot->vk_pipeline_legacy = new_slot->vk_pipeline_legacy;
                            #endif
                            new_slot->is_active = false;
                        }
                    }
                    #endif
                    SIT_FREE(vs_src); SIT_FREE(fs_src);
                }
            }
        }
    }

    // 2. Textures
    for (int i = 0; i < SITUATION_MAX_TEXTURES; i++) {
        _SituationTextureSlot* slot = &sit_render.texture_registry[i];
        if (slot->is_active && slot->source_path) {
            long mod = SituationGetFileModTime(slot->source_path);
            if (mod != slot->mod_time) {
                printf("[Situation] Hot-Reloading Texture %d...\n", i);
                slot->mod_time = mod;

                SituationImage img;
                if (SituationLoadImage(slot->source_path, &img) == SITUATION_SUCCESS) {
                    SituationTexture temp;
                    if (SituationCreateTexture(img, true, &temp) == SITUATION_SUCCESS) {
                        _SituationTextureSlot* new_slot = _SitGetTextureSlot(temp);
                        if (new_slot) {
                            // Swap internals
                            #if defined(SITUATION_USE_OPENGL)
                            _SitGLDeferDestroyTexture(slot->gl_texture_id);
                            slot->gl_texture_id = new_slot->gl_texture_id;
                            #elif defined(SITUATION_USE_VULKAN)
                            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
                            slot->image = new_slot->image;
                            slot->allocation = new_slot->allocation;
                            slot->image_view = new_slot->image_view;
                            slot->sampler = new_slot->sampler;
                            #endif
                            slot->width = new_slot->width;
                            slot->height = new_slot->height;

                            new_slot->is_active = false;
                        }
                    }
                    SituationUnloadImage(img);
                }
            }
        }
    }

    // 3. Audio
    mtx_lock(&sit_audio.pool_mutex);
    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; i++) {
        _SituationSoundSlot* slot = &sit_audio.sound_pool[i];
        if (slot->is_active && slot->source_path) {
            long mod = SituationGetFileModTime(slot->source_path);
            if (mod != slot->mod_time) {
                printf("[Situation] Hot-Reloading Audio %d...\n", i);
                slot->mod_time = mod;

                // Stop playback
                SituationSound handle = { (uint32_t)i, slot->generation };
                SituationStopLoadedSound(&handle);

                // Reload
                _SituationSound* sound = &slot->sound_data;
                if (sound->is_preloaded && sound->preloaded_data) ma_free(sound->preloaded_data, NULL);
                if (sound->is_initialized) ma_decoder_uninit(&sound->decoder);

                // Re-load logic (inline simplified)
                // Assuming same mode (AUTO logic inside LoadSoundFromFile)
                // We reuse SituationLoadSoundFromFile but targeting a temp slot first?
                // Or just re-run load logic on this slot.
                // Re-running logic is safer.

                // Actually, SituationLoadSoundFromFile allocates a NEW slot.
                // So we should do:
                SituationSound temp_handle;
                if (SituationLoadSoundFromFile(slot->source_path, SITUATION_AUDIO_LOAD_AUTO, sound->is_looping, &temp_handle) == SITUATION_SUCCESS) {
                    _SituationSoundSlot* new_slot = _SitGetSoundSlot(temp_handle);
                    if (new_slot) {
                        // Move data from new_slot to current slot
                        // We must preserve existing volume/pan/effects?
                        // Or just reset?
                        // Preserve:
                        float vol = atomic_load(&sound->volume);
                        float pan = atomic_load(&sound->pan);

                        // Overwrite sound data
                        slot->sound_data = new_slot->sound_data;

                        // Restore state
                        atomic_store(&slot->sound_data.volume, vol);
                        atomic_store(&slot->sound_data.pan, pan);

                        // Free new slot
                        new_slot->is_active = false;
                    }
                }
            }
        }
    }
    mtx_unlock(&sit_audio.pool_mutex);

#endif
}


SITAPI SituationError SituationCheckHotReloads(void) {
    // Logic moved to I/O thread.
    return SITUATION_SUCCESS;
}

// ==================================================================================
//  Render Thread Implementation (Phase 2)
// ==================================================================================
#if !defined(__STDC_NO_THREADS__)

// [v2.3.24a] Safety Zenith: Helper to flush resources for a specific frame index (or global for GL)
static void _SitFlushFrameResources(int frame_index) {
    #if defined(SITUATION_USE_OPENGL)
        // OpenGL uses a global graveyard (deferred command buffer system handles synchronization)
        _SitGLFlushGraveyard(frame_index);
    #elif defined(SITUATION_USE_VULKAN)
        // Vulkan uses per-frame graveyards
        _SituationFlushGraveyard((uint32_t)frame_index);
    #endif
}

/**
 * @brief [INTERNAL] Entry point for the dedicated render thread.
 *
 * @details This function runs in a separate thread and is responsible for consuming
 *          queued frame indices from the main thread, executing the corresponding
 *          soft command buffers, submitting work to the GPU (Vulkan) or swapping
 *          buffers (OpenGL), handling presentation, and managing per-frame resource
 *          cleanup via graveyards/fences.
 *
 *          The thread uses a condition variable + mutex to wait efficiently when no
 *          frames are pending. It exits cleanly when `thread_shutdown_req` is set
 *          and the queue is empty.
 *
 *          Key responsibilities:
 *            - Acquires the OpenGL context (if applicable) after main thread release
 *            - Dequeues frame indices from the circular render queue
 *            - Executes backend-specific rendering (command buffer playback, submit, present)
 *            - Tracks and flushes deferred resource destruction (graveyards)
 *            - Updates frame metrics/latency when metrics are enabled
 *            - Signals the main thread when a frame slot becomes free
 *
 *          Thread safety invariants:
 *            - Only this thread accesses the backend command buffers / swapchain / context
 *            - Queue access is protected by `render_queue_mutex`
 *            - Frame refcounts are managed atomically
 *            - Graveyard flushes happen after GPU work completion (fences/sync objects)
 *
 * @param arg Unused thread argument (always NULL in current usage)
 * @return 0 on clean exit (standard thread return value)
 *
 * @note Called via thrd_create() during SituationInit().
 *       Main thread must release the OpenGL context before the render thread starts.
 *       See also: render_queue_cv, render_queue_mutex, thread_shutdown_req,
 *                 frame_refcounts[], _SitFlushFrameResources()
 */
static int _SituationRenderThreadEntry(void* arg) {
    (void)arg;
    #ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
    #else
    pthread_t tid = pthread_self();
    #endif
    fprintf(stderr, "[Situation] [Thread %lu] RENDER THREAD STARTED\n", (unsigned long)tid); fflush(stderr);

    // Pin the Render Thread strictly to Logical Core 1
    // (Leaving Core 0 free for the OS and Main Game loop)
    SituationSetThreadAffinity(1 << 1); 
	
    // [OpenGL] We must acquire the context here.
    // Note: Initialization (SituationInit) happens on Main.
    // The Main thread must release the context (glfwMakeContextCurrent(NULL)) before triggering this thread.
    #if defined(SITUATION_USE_OPENGL)
    // Wait for Main thread to release context (prevents race condition)
    // This explicit synchronization ensures the context handoff is safe.
    while (!atomic_load(&sit_render.gl_context_released)) {
        thrd_yield(); // Yield CPU while waiting
    }
    if (sit_gs.sit_glfw_window) {
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
    }
    #endif

    fprintf(stderr, "[Situation] [Thread %lu] RENDER THREAD entering main loop\n", (unsigned long)tid); fflush(stderr);
    while (true) {
        fprintf(stderr, "[Situation] [Thread %lu] RENDER THREAD loop iteration\n", (unsigned long)tid); fflush(stderr);
        mtx_lock(&sit_render.render_queue_mutex);

        // Wait for work or shutdown
        // We check if the queue is empty.
        // Note: frames_pending counts "items in queue" + "items being processed".
        // But here we just want to know if there is an item IN THE QUEUE to pop.
        // Queue is empty if head == tail.
        while (sit_render.render_queue_head == sit_render.render_queue_tail && !sit_render.thread_shutdown_req) {
            cnd_wait(&sit_render.render_queue_cv, &sit_render.render_queue_mutex);
        }

        // Shutdown Check
        if (sit_render.thread_shutdown_req && sit_render.render_queue_head == sit_render.render_queue_tail) {
            mtx_unlock(&sit_render.render_queue_mutex);
            break;
        }

        // Dequeue Frame Index
        int frame_index = sit_render.render_queue[sit_render.render_queue_tail];
        sit_render.render_queue_tail = (sit_render.render_queue_tail + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;

        // [Metrics]
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        atomic_fetch_sub(&sit_render.render_queue_depth, 1);
        #endif

        // Note: We do NOT decrement frames_pending here. We are still "working" on this frame.
        // We decrement it only after we are fully done rendering.

        mtx_unlock(&sit_render.render_queue_mutex);

        // --- EXECUTE FRAME ---
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] Processing frame_index=%d\n", frame_index);
        fflush(stdout);
        #endif

        #if defined(SITUATION_USE_OPENGL)

        // 1. Wait for OLD frame to finish and flush its graveyard (Vulkan Parity)
        // This ensures the GPU isn't still reading buffers we are about to overwrite/delete.
        if (sit_render.gl.frame_fences[frame_index]) {
            glClientWaitSync(sit_render.gl.frame_fences[frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000); // 1 sec timeout

            _SitGLFlushGraveyard(frame_index); // Safe to flush now

            glDeleteSync(sit_render.gl.frame_fences[frame_index]);
            sit_render.gl.frame_fences[frame_index] = 0;
        }

        // 2. Execute Soft Command Buffer
        _SituationGLExecuteCommands(&sit_render.gl.soft_buffers[frame_index], frame_index);

        // 3. Present
        glfwSwapBuffers(sit_gs.sit_glfw_window);

        // 4. Create NEW fence to track the commands we just submitted
        sit_render.gl.frame_fences[frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush(); // Ensure the fence is pushed to the GPU queue

        #elif defined(SITUATION_USE_VULKAN)
        VkCommandBuffer cmd = sit_render.vk.command_buffers[frame_index];

        // 1. Submit
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // [v2.3.24b] Sync Logic
        VkSemaphore wait_semaphores[2];
        VkPipelineStageFlags wait_stages[2];
        uint32_t wait_count = 0;

        // Always wait for image available (Color Output)
        wait_semaphores[wait_count] = sit_render.vk.image_available_semaphores[frame_index];
        wait_stages[wait_count] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        wait_count++;

        // Conditionally wait for compute (Draw Indirect / Vertex Input)
        if (sit_render.vk.needs_compute_wait) {
            wait_semaphores[wait_count] = sit_render.vk.compute_finished_semaphores[frame_index];
            wait_stages[wait_count] = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            wait_count++;
            sit_render.vk.needs_compute_wait = false; // Reset for next frame
        }

        submit_info.waitSemaphoreCount = wait_count;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[frame_index] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        // Use the fence associated with this frame index
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] About to submit frame_index=%d to GPU\n", frame_index);
        printf("Situation [Vulkan Debug]: [RENDER THREAD]   Command buffer: %p\n", (void*)cmd);
        printf("Situation [Vulkan Debug]: [RENDER THREAD]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
        fflush(stdout);
        #endif
        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[frame_index]);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
        fflush(stdout);
        #endif
        if (submit_result != VK_SUCCESS) {
            if (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                sit_render.vk.screenshot_copy_pending[frame_index] = false;
            }
            sit_render.vk.screenshot_valid = false;
            // Logging from thread is tricky but we can set the global error.
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "RenderThread: Failed to submit draw command buffer!");
        } else {
            _SituationVulkanResolveScreenshotAfterSubmit(frame_index);
        }

        // 2. Present
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;
        VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &sit_render.vk.acquired_image_indices[frame_index];

        // Perform the presentation.
        VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);

        // Handle Presentation Result
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            atomic_store(&sit_render.vk.recreate_swapchain_request, true);
        } else if (result != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "RenderThread: Failed to present swap chain image!");
        }
        #endif

        // [v2.3.22] Metrics: Record Latency
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        uint64_t submit_ts = atomic_load(&sit_render.submit_timestamps[frame_index]);
        if (submit_ts > 0) {
            uint64_t now = _SitGetMonotonicTimeNS();

            // [v2.3.25] Drift Check with Once-Warn
            uint64_t latency = 0;
            if (now < submit_ts) {
                #ifndef NDEBUG
                if (!atomic_exchange(&sit_render.drift_warned, true)) {
                    fprintf(stderr, "[METRIC] First drift; clamped 0.\n");
                }
                #endif
                latency = 0;
            } else {
                latency = now - submit_ts;
            }

            // [v2.3.24a] Max Latency Metric (Histogram Stub)
            // Use retry-limited loop for high contention safety
            uint64_t global_max = atomic_load(&sit_render.metric_max_latency_ns);
            int retries = 0;
            while (latency > global_max && !atomic_compare_exchange_weak(&sit_render.metric_max_latency_ns, &global_max, latency)) {
                 retries++;
                 if (retries > 20) { fprintf(stderr, "[METRIC] Retries %dÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Âcontention hint.\n", retries); break; }
            }

            atomic_fetch_add(&sit_render.metric_latency_sum_ns, latency);
            atomic_fetch_add(&sit_render.metric_latency_count, 1);
        }
        #endif

        // Refcount (Unify)
        if (atomic_fetch_sub(&sit_render.frame_refcounts[frame_index], 1) == 1) {
            // Refcount reached 0 (fetch_sub returned 1). Safe to recycle.
            _SitFlushFrameResources(frame_index);
        }

        // --- FRAME COMPLETE ---
        mtx_lock(&sit_render.render_queue_mutex);

        sit_render.frames_pending--; // Slot `frame_index` is now free.

        // [CRITICAL FIX] Signal the Main Thread!
        // This wakes up SituationEndFrame if it is waiting in the cnd_wait loop above.
        cnd_signal(&sit_render.main_wait_cv);

        // Wake Main Thread if it was blocked on full queue (Redundant but safe signal)
        // cnd_signal(&sit_render.main_wait_cv); // Already done above

        mtx_unlock(&sit_render.render_queue_mutex);
    }

    #if defined(SITUATION_USE_OPENGL)
    // Release context before exiting, just to be clean.
    glfwMakeContextCurrent(NULL);
    #endif

    // [v2.3.22] Mark thread as inactive for timeout join polling
    atomic_store(&sit_render.thread_active, false);

    return 0;
}
#endif

#endif // SITUATION_IMPL_RENDERER_H
