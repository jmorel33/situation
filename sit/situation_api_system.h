/***************************************************************************************************
*
*   situation_api_system.h - Filesystem, Threading, Timers, and Hot-Reload API
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Velocity hot-reload helpers, path and file I/O, temporal oscillators, color/YPQ/HDR science
*   utilities, and CPU topology / thread-pool management (including async file and sound jobs).
*
*   Requires SITAPI from situation_api.h.
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_SYSTEM_H
#define SITUATION_API_SYSTEM_H

#include "situation_api_config.h"
#include "situation_base_types.h"
#include "situation_base_callbacks.h"
#include "situation_api_types_system.h"
#include "situation_api_types_gpu.h"
#include "situation_api_types_audio.h"

#include <stdio.h>

//==================================================================================
// Hot-Reloading Module (Development Tools)
//==================================================================================
// These functions allow you to reload assets from disk at runtime without restarting.
// They handle GPU synchronization, resource destruction, and re-loading.
// Returns true if the reload was successful. On failure, the old handle is usually invalid.
SITAPI SituationError SituationCheckHotReloads(void);                                   // Checks all tracked resources for file changes and reloads them if necessary.
SITAPI SituationError SituationReloadShader(SituationShader* shader);                             // Recompiles and links a shader from its original source files (Synchronous/Stalls GPU).
SITAPI SituationError SituationReloadComputePipeline(SituationComputePipeline* pipeline);         // Recompiles a compute pipeline from its original source file (Synchronous/Stalls GPU).
SITAPI SituationError SituationReloadTexture(SituationTexture* texture);                          // Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls GPU).
SITAPI SituationError SituationReloadModel(SituationModel* model);                                // Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls GPU).




//==================================================================================
// Filesystem Module
//==================================================================================
// --- Path Management & Special Directories ---
SITAPI char* SituationGetAppSavePath(const char* app_name);                             // Get a safe, persistent path for saving application data (caller must free).
SITAPI char* SituationGetBasePath(void);                                                // Get the path to the directory containing the executable (caller must free).
SITAPI char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);    // Join two path components with the correct OS separator (caller must free).
SITAPI const char* SituationGetFileName(const char* full_path);                         // Extract the file name (including extension) from a full path.
SITAPI const char* SituationGetFileExtension(const char* file_path);                    // Extract the file extension from a path.

// --- File & Directory Queries ---
SITAPI bool SituationFileExists(const char* file_path);                                 // Check if a file exists at the given path.
SITAPI bool SituationDirectoryExists(const char* dir_path);                             // Check if a directory exists at the given path.
SITAPI long SituationGetFileModTime(const char* file_path);                             // Get the last modification time of a file (Unix timestamp).

// --- File Operations ---
SITAPI SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data);   // Load an entire file into a memory buffer (caller must free).
SITAPI SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);    // Save a block of memory to a file.
#ifdef SITUATION_ENABLE_THREADING
SITAPI SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data); // Asynchronously load a file.
SITAPI SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a file.
SITAPI SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data); // Asynchronously load a text file.
SITAPI SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a text file.
#endif
SITAPI char* SituationLoadFileText(const char* file_path);                              // Load a text file into a null-terminated string (caller must free).
SITAPI SituationError SituationSaveFileText(const char* file_path, const char* text);   // Save a null-terminated string to a text file.
SITAPI SituationError SituationCopyFile(const char* source_path, const char* dest_path); // Copy a file.
SITAPI SituationError SituationDeleteFile(const char* file_path);                       // Delete a file.
SITAPI SituationError SituationMoveFile(const char* old_path, const char* new_path);    // Move/rename a file, even across drives on Windows.
SITAPI SituationError SituationRenameFile(const char* old_path, const char* new_path);  // Alias for SituationMoveFile.

// --- Directory Operations ---
SITAPI SituationError SituationCreateDirectory(const char* dir_path, bool create_parents); // Create a directory, optionally creating parent directories.
SITAPI SituationError SituationDeleteDirectory(const char* dir_path, bool recursive);      // Delete a directory, optionally deleting all its contents.
SITAPI char** SituationListDirectoryFiles(const char* dir_path, int* out_count);        // List files and subdirectories in a path (caller must free with SituationFreeDirectoryFileList).
SITAPI void SituationFreeDirectoryFileList(char** file_list, int count);                // Free the memory allocated by SituationListDirectoryFiles.

//==================================================================================
// Miscellaneous Module
//==================================================================================
// --- Temporal Oscillator System ---
SITAPI bool SituationTimerGetOscillatorState(int oscillator_id);                        // Get the current binary state (0 or 1) of an oscillator.
SITAPI bool SituationTimerGetPreviousOscillatorState(int oscillator_id);                // Get the previous frame's state of an oscillator.
SITAPI bool SituationTimerHasOscillatorUpdated(int oscillator_id);                      // Check if an oscillator's state has changed this frame.
SITAPI bool SituationTimerPingOscillator(int oscillator_id);                            // Check if an oscillator's period has elapsed since the last ping.
SITAPI uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);             // Get the total number of times an oscillator has triggered.
SITAPI double SituationTimerGetOscillatorPeriod(int oscillator_id);                     // Get the period of an oscillator in seconds.
SITAPI SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds); // Set the period of an oscillator.
SITAPI double SituationTimerGetPingProgress(int oscillator_id);                         // Get progress [0.0 to 1.0+] of the interval since the last successful ping.
SITAPI double SituationTimerGetTime(void);                                              // Get the total time elapsed since initialization.

// --- Color Space Conversions ---
SITAPI void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color); // Convert an 8-bit ColorRGBA struct to a normalized Vector4.
SITAPI ColorHSV SituationRgbToHsv(ColorRGBA rgb);                                       // Converts a standard RGBA color to the Hue, Saturation, Value color space.
SITAPI ColorRGBA SituationHsvToRgb(ColorHSV hsv);                                       // Converts a Hue, Saturation, Value color back to the standard RGBA color space.

// --- YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) ---
SITAPI ColorYPQA SituationColorToYPQ(ColorRGBA color);                                  // Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space.
SITAPI ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color);                            // Converts a YPQA color back to the standard RGBA color space.
SITAPI ColorYPQA SituationYpqLerp(ColorYPQA a, ColorYPQA b, float t);                   // Interpolate YPQ; phase uses shortest arc on the hue wheel.
SITAPI ColorYPQA SituationYpqAdjustLuma(ColorYPQA color, float luma_factor);             // Scale Y (luma); preserve phase and chroma.
SITAPI ColorYPQA SituationYpqAdjustPhase(ColorYPQA color, int phase_shift);             // Rotate hue; P shifts by byte steps mod 256.
SITAPI ColorYPQA SituationYpqAdjustChroma(ColorYPQA color, float chroma_factor);          // Scale Q (chroma amplitude); preserve luma and phase.
SITAPI float SituationYpqGetLuma(ColorYPQA color);                                      // Normalized luma [0, 1].
SITAPI float SituationYpqGetHueDegrees(ColorYPQA color);                                // Hue in degrees [0, 360).
SITAPI float SituationYpqGetChroma(ColorYPQA color);                                    // Normalized chroma amplitude [0, 1].
SITAPI float SituationYpqDistance(ColorYPQA a, ColorYPQA b);                          // Weighted distance in YPQ space.
SITAPI bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tolerance);    // Per-channel tolerance compare.
SITAPI ColorYPQf SituationColorToYPQf(ColorRGBA color);                                 // RGBA → normalized float YPQ (no 8-bit quantize).
SITAPI ColorRGBA SituationColorFromYPQf(ColorYPQf ypq);                                 // Float YPQ → RGBA (linear YIQ, clamped RGB).
SITAPI ColorYPQA SituationYpqQuantize(ColorYPQf ypq);                                   // Float YPQ → 8-bit ColorYPQA.
SITAPI ColorYPQf SituationYpqClampInGamut(ColorYPQf ypq);                               // Reduce chroma if linear RGB would clip.
SITAPI ColorRGBA10 SituationYpqToRgba10(ColorYPQf ypq);                                 // Float YPQ → 10-bit RGBA (linear YIQ, clamped).
SITAPI uint32_t SituationYpqToRgb10Packed(ColorYPQf ypq);                               // Float YPQ → A2R10G10B10 packed pixel (10-bit SDR).
SITAPI uint32_t SituationYpqToRgb10PackedHdr(ColorYPQf ypq);                            // Float YPQ → PQ-encoded A2R10G10B10 (HDR10 swapchain).
SITAPI float SituationLinearToPq(float linear);                                         // Linear display light [0,1] → ST.2084 PQ [0,1].
SITAPI float SituationPqToLinear(float pq);                                              // ST.2084 PQ [0,1] → linear display light [0,1].
SITAPI uint32_t SituationPqGrayToRgb10Packed(float pq_level);                           // Uniform PQ gray → A2R10G10B10 packed pixel.
SITAPI ColorRGBA SituationColorRgbaToHdrPqClear(ColorRGBA srgb);                        // sRGB 0–255 → PQ clear color as RGBA floats×255 for debugging.
SITAPI ColorYPQf SituationRgbToYpqFrom10(ColorRGBA10 color);                            // 10-bit RGBA → float YPQ.
SITAPI ColorRGBA10 SituationRgb10FromRgba(ColorRGBA color);                             // Upscale 8-bit RGBA → 10-bit.
SITAPI ColorRGBA SituationRgbaFromRgb10(ColorRGBA10 color);                             // Downscale 10-bit RGBA → 8-bit.
SITAPI ColorRGBA SituationRgbaFromRgb10Packed(uint32_t packed);                         // A2R10G10B10 texel → RGBA8 (readback parity).

SITAPI SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out);
                                                              // Full 256³ scan: count unique RGB outputs, duplicates,
                                                              // holes, and worst fixed-Q slice. O(n³) — ~16 M calls to
                                                              // SituationColorFromYPQ; may take several seconds.
SITAPI SituationError SituationYpqSliceDuplicateCount(char axis, int value, int* out_dup);
                                                              // Count duplicate RGB outputs in one 65 536-entry axis
                                                              // slice. axis ∈ {'Y','P','Q'}, value ∈ [0,255].
                                                              // Q=0 slice always yields ≥65 000 duplicates (all gray).

//==================================================================================
// Threading Module
//==================================================================================

// --- CPU & Thread Management ---
SITAPI uint32_t SituationGetCPUThreadCount(void);           // Logical processor count (cached topology).
SITAPI uint32_t SituationGetCPUCoreCount(void);             // Gets physical processors (Cores) from cached topology
SITAPI SituationError SituationRefreshCpuTopology(void);              // Rebuilds the process-wide topology cache
SITAPI SituationError SituationGetCpuTopology(const SituationCpuTopology** out_topology); // Pointer to cached topology (NULL on failure)
SITAPI void SituationSetCurrentThreadName(const char* name);            // OS-visible name for the calling thread (UTF-8); no-op if NULL/empty
SITAPI SituationError SituationSetThreadAffinity(uint64_t core_mask); // Pins the CURRENT thread (logical CPU bitmask, bits 0..63)
SITAPI SituationError SituationSetThreadAffinityEx(uint64_t core_mask, uint64_t* out_previous); // Set affinity; optional previous mask
SITAPI SituationError SituationGetThreadAffinity(uint64_t* out_mask); // Reads affinity mask for the CURRENT thread
SITAPI int  SituationGetCurrentProcessorIndex(void);        // Logical CPU index for current thread, or -1 if unknown
SITAPI int  SituationGetThreadNumaNode(void);               // NUMA node for current thread, or -1 if unknown
SITAPI uint64_t SituationBuildPhysicalCoreMask(int physical_core_index); // All logical CPUs on one physical core
SITAPI uint64_t SituationBuildUniqueCoreMask(int start_physical_core, int count, bool avoid_siblings); // One LP per core
SITAPI uint64_t SituationBuildNumaNodeMask(int numa_node_index); // All logical CPUs on a NUMA node
SITAPI uint64_t SituationGetConfiguredMainThreadAffinity(void);   // Init mask for main thread (0 = no pin)
SITAPI uint64_t SituationGetConfiguredRenderThreadAffinity(void); // Effective render mask (init or default)
SITAPI uint64_t SituationGetConfiguredAudioThreadAffinity(void);  // Effective audio mask (init or default)
SITAPI uint64_t SituationGetConfiguredIOThreadAffinity(void);     // Effective I/O mask (init or default CPU 3)
SITAPI SituationError SituationRefreshNumaTopology(void);                   // Rebuild NUMA summary from CPU topology + OS memory
SITAPI SituationError SituationGetNumaTopology(const SituationNumaTopology** out_topology); // Cached NUMA snapshot
SITAPI int SituationGetPreferredNumaNode(void);                   // TLS: node for current thread, or -1 if unset
#ifdef SITUATION_ENABLE_THREADING
SITAPI SituationError SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io); // Initializes the thread pool with dual-priority queues and worker threads.
SITAPI void SituationDestroyThreadPool(SituationThreadPool* pool); 											// Shuts down the thread pool and releases resources.
SITAPI SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags); // Submits a job with priority flags and optional data payload.
 // Legacy wrapper for simple pointer passing (Low priority, no copy).
#define SituationSubmitJob(pool, func, user_ptr) \
    SituationSubmitJobEx(pool, (void(*)(void*, void*))func, user_ptr, 0, SIT_SUBMIT_DEFAULT)
SITAPI void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size, void (*func)(int index, void* user_data), void* user_data); // Executes a loop in parallel across worker threads (Fork-Join).
SITAPI SituationError SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id); 							// Waits for a specific job to complete (O(1) check).
SITAPI void SituationWaitForAllJobs(SituationThreadPool* pool); 											// Blocks until all queued jobs are finished.
SITAPI SituationError SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prerequisite_job, SituationJobId dependent_job); // Adds a dependency between two jobs (prereq -> dependent).
SITAPI SituationError SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job); // Adds multiple dependencies for a single dependent job.
SITAPI void SituationDumpTaskGraph(SituationThreadPool* pool, FILE* out_stream, bool json_mode); 			// Prints the current task graph state to the stream.
SITAPI SituationThreadingStatus SituationGetThreadingStatus(void);                                          // Runtime threading capabilities + pool summary
SITAPI void SituationPrintThreadingStatus(FILE* out_stream);                                                // Human-readable threading status (stdout if NULL)
SITAPI size_t SituationGetQueueDepth(SituationThreadPool* pool, SituationJobQueueMask mask);                 // Pending jobs per queue mask
SITAPI size_t SituationGetHighQueueDepth(SituationThreadPool* pool);                                          // High-priority queue depth
SITAPI int SituationGetActiveJobCount(SituationThreadPool* pool);                                           // active_jobs counter
SITAPI SituationError SituationGetThreadPoolSnapshot(SituationThreadPool* pool, SituationThreadPoolSnapshot* out);    // Worker/I/O/render/audio placement snapshot
SITAPI void SituationDumpThreadPoolStatus(SituationThreadPool* pool, FILE* out_stream, bool json_mode);      // Pool metrics + per-role CPU snapshot
SITAPI void SituationDumpThreadingReport(SituationThreadPool* pool, FILE* out_stream, bool json_mode);       // Status + topology line + pool dump
SITAPI uint32_t SituationGetRecommendedWorkerCount(uint32_t reserved_threads, bool use_physical_cores);     // Sizing helper (no pool required)
SITAPI SituationError SituationGetThreadPoolMetrics(SituationThreadPool* pool, SituationThreadPoolMetrics* out_metrics); // Scheduler counters snapshot
SITAPI void SituationResetThreadPoolStats(SituationThreadPool* pool);                                         // Zero scheduler counters
SITAPI void SituationDumpThreadPoolMetrics(SituationThreadPool* pool, FILE* out_stream, bool json_mode);      // Metrics-only dump
SITAPI SituationThreadPool* SituationGetInternalThreadPool(void);                                            // Returns pointer to the library's internal thread pool (NULL if not initialized).

SITAPI SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound); // Asynchronously loads and decodes a sound file.
#endif // SITUATION_ENABLE_THREADING

#endif /* SITUATION_API_SYSTEM_H */
