/***************************************************************************************************
*
*   situation_impl_forward.h - Forward Declarations for Internal Static Functions
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file contains forward declarations for static helper functions used
*   throughout the Situation implementation. It is included by situation_impl.h
*   after types are defined (via situation_impl_decl.h).
*
*   Public SITAPI prototypes: sit/situation_api.h. Renderer statics:
*   situation_impl_renderer_fwd.h (included below).
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_FORWARD_H
#define SITUATION_IMPL_FORWARD_H

//----------------------------------------------------------------------------------
// Control Module (Lifecycle, Error Handling)
//----------------------------------------------------------------------------------
/* HARDENING: void by design — error sink; sets global error state. */
static void _SituationSetError(const char* msg);
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail);
static SituationError _SituationInitPlatform(const SituationInitInfo* init_info);
static SituationError _SituationInitWindow(const SituationInitInfo* init_info);
static void _SituationApplyDefaultWindowIconPath(const char* path_utf8);
static SituationError _SituationInitSubsystems(const SituationInitInfo* init_info);
/* HARDENING: void by design — best-effort platform teardown during shutdown. */
static void _SituationCleanupPlatform(void);
/* HARDENING: void by design — best-effort renderer teardown during shutdown. */
static void _SituationCleanupRenderer(void);
/* HARDENING: void by design — best-effort subsystem teardown during shutdown. */
static void _SituationCleanupSubsystems(void);
/* HARDENING: void by design — best-effort teardown on init failure. */
static void _SituationFullCleanupOnError(void);
static uint32_t _SituationPackApiVersionMajorMinor(uint32_t major, uint32_t minor);

//----------------------------------------------------------------------------------
// Utility Helpers (situation_impl_etc.h)
//----------------------------------------------------------------------------------
static char* _sit_dirname(const char* path);
static bool _sit_directory_exists(const char* dir_path);
static unsigned long _sit_hash_string(const char* str);
static int _sit_strcasecmp(const char* s1, const char* s2);

//----------------------------------------------------------------------------------
// Timer (situation_impl_timer.h)
//----------------------------------------------------------------------------------
static uint64_t _SituationGetHighResTime(void);

//----------------------------------------------------------------------------------
// GLFW Callback Helpers
//----------------------------------------------------------------------------------
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWErrorCallback(int error, const char* description);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWFileDropCallback(GLFWwindow* window, int count, const char** paths);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWWindowFocusCallback(GLFWwindow* window, int focused);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWWindowMaximizeCallback(GLFWwindow* window, int maximized);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWWindowIconifyCallback(GLFWwindow* window, int iconified);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWFramebufferSizeCallback(GLFWwindow* window, int width, int height);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWCharCallback(GLFWwindow* window, unsigned int codepoint);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWJoystickCallback(int jid, int event);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWMonitorCallback(GLFWmonitor* monitor, int event);
/* HARDENING: void by design — GLFW callback ABI. */
static void _SituationGLFWWindowPosCallback(GLFWwindow* window, int xpos, int ypos);

//----------------------------------------------------------------------------------
// Threading & IO Helpers
//----------------------------------------------------------------------------------
static int _SituationIOThreadEntry(void* arg);
static char* _sit_strdup(const char* s);
static int _SituationWorkerEntry(void* arg);
static void _SituationApplyWorkerNumaPlacement(struct SituationThreadPool* pool, size_t worker_index);
static void _SituationApplyIoThreadNumaPlacement(struct SituationThreadPool* pool);

#if !defined(__STDC_NO_THREADS__)
static SituationError _SituationInitRenderThread(const SituationInitInfo* info);
static SituationError _SituationDestroyRenderThread(void);
static SituationError _SitFlushFrameResources(int frame_index);
static int _SituationRenderThreadEntry(void* arg);
#endif // !__STDC_NO_THREADS__

#if defined(SITUATION_ENABLE_THREADING)
/* HARDENING: void by design — thread-pool job ABI; enqueue failures use _SituationSetErrorFromCode. */
static void _SituationRenderJobWorker(void* data, void* unused);
static void _SitQueueCompactTailLocked(SituationThreadPool* pool, int q_idx);
static bool _SitWorkerTryClaimReadyJob(SituationThreadPool* pool, int q_idx, SituationJob** out_job);
static void _SitThreadPoolRetireOrphanedJobMain(SituationThreadPool* pool, SituationJobId job_id);
static bool _SitJobHandleSettled(SituationThreadPool* pool, SituationJobId job_id);
static SituationJob* _SitGetJobFromId(SituationThreadPool* pool, SituationJobId id);
static void _SitParallelWorker(void* data, void* ctx);
static bool _SituationDetectCycle(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id, uint8_t* out_new_depth);
#endif // SITUATION_ENABLE_THREADING

//----------------------------------------------------------------------------------
// Threading Diagnostics (situation_impl_threading_diag.h)
//----------------------------------------------------------------------------------
static void _SituationSetCurrentThreadName(const char* name);
static const char* _SituationResolveMainThreadOsName(const char* main_thread_name, const char* window_title);
static void _SituationCopyThreadName(char* dest, size_t dest_sz, const char* name);
static void _SituationWin32SetThreadDescriptionUtf8(const char* name);

//----------------------------------------------------------------------------------
// Threading NUMA (situation_impl_threading_numa.h)
//----------------------------------------------------------------------------------
static void _SitSetPreferredNumaNode(int node);
static int _SitNodeForLogicalCpu(int logical_cpu);
static void _SitFetchNumaMemoryWindows(SituationNumaTopology* numa);
static bool _SitReadNumaMemTotalLinux(unsigned int node, uint64_t* out_bytes);
static uint16_t _SitCountLinuxNumaNodes(void);
static bool _SitRebuildNumaFromCpuTopology(SituationNumaTopology* numa);
static bool _SituationSetThreadAffinityForRole(SituationThreadRole role, uint64_t explicit_mask);

//----------------------------------------------------------------------------------
// Threading Observability (situation_impl_threading_observability.h)
//----------------------------------------------------------------------------------
static void _SitObsInitSpecialThreads(void);
static void _SituationObservabilityRecordRenderThread(uint64_t affinity_mask);
static void _SituationObservabilityRecordAudioThread(uint64_t affinity_mask);
static void _SitWorkerSampleCpu(SituationThreadPool* pool, size_t worker_index);
static int _SitNumaForLogicalCpu(int logical_cpu);
static size_t _SitQueueDepthUnlocked(SituationThreadPool* pool, int q_idx);
static const char* _SitRoleName(SituationThreadRole role);

//----------------------------------------------------------------------------------
// Threading Scheduler (situation_impl_threading_scheduler.h)
//----------------------------------------------------------------------------------
static size_t _SitResolveAutoWorkerCount(void);
static void _SitPoolMetricsFromPool(SituationThreadPool* pool, SituationThreadPoolMetrics* out);

//----------------------------------------------------------------------------------
// Threading Topology (situation_impl_threading_topology.h)
//----------------------------------------------------------------------------------
static int _SitCountBits64(uint64_t mask);
static bool _SitRefreshTopologyWindows(SituationCpuTopology* topo);
static bool _SitReadSysfsUint(const char* path, unsigned long* out);
static bool _SitRefreshTopologyLinux(SituationCpuTopology* topo);
static bool _SitRefreshTopologyMacOS(SituationCpuTopology* topo);
static bool _SitRefreshTopologyInternal(SituationCpuTopology* topo);
static bool _SitEnsureTopology(void);

//----------------------------------------------------------------------------------
// I/O Module (situation_impl_io.h)
//----------------------------------------------------------------------------------
static WCHAR* _sit_utf8_to_wide(const char* utf8_str);
static char* _sit_wide_to_utf8(const WCHAR* wide_str);
static SituationError _SituationSetFilesystemError(const char* base_message, const char* path, SituationError default_error);
static void _SituationAsyncFileLoadWorker(void* data, void* unused);
static void _SituationAsyncFileTextLoadWorker(void* data, void* unused);
static void _SituationAsyncFileTextSaveWorker(void* data, void* unused);
static void _SituationAsyncFileSaveWorker(void* data, void* unused);
static int _SituationCollectStorageDevices(
    char names[SITUATION_MAX_STORAGE_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN],
    uint64_t capacity[SITUATION_MAX_STORAGE_DEVICES],
    uint64_t free_bytes[SITUATION_MAX_STORAGE_DEVICES]);
static int _SituationCollectNetworkAdapters(char names[SITUATION_MAX_NETWORK_ADAPTERS][SITUATION_MAX_DEVICE_NAME_LEN]);
static int _SituationCollectInputDevices(char names[SITUATION_MAX_INPUT_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN]);

//----------------------------------------------------------------------------------
// Input Module (situation_impl_input.h)
// (GLFW callbacks above; additional helpers here)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Audio Helpers
//----------------------------------------------------------------------------------
static void _SituationWaitUntilAudioCallbackIdle(void);
static void _SituationEnsureRegistryInit(void);
static void _SituationWaitUntilVoiceSnapshotIdle(void);
static _SituationSoundSlot* _SitAllocSoundSlot(SituationSound* out_handle);
static SituationError _SitFreeSoundSlot(SituationSound handle);
static _SituationSoundSlot* _SitGetSoundSlot(SituationSound handle);
static bool _SituationGraphHasMixerNode(const SituationAudioGraph* graph);
static bool _SituationShouldMixLatentVoices(const _SituationAudioState* pGs);
static void _SituationMixLoadedVoicesFromSnapshot(
    _SituationAudioState* pGs,
    ma_device* pDevice,
    uint32_t frameCount,
    float* decoder_buffer,
    float* effects_buffer,
    float* mix_dest_stereo,
    int voices_to_mix
);
static void _SituationPublishMasterBusLevels(_SituationAudioState* pGs, const float* pOut, uint32_t frameCount, uint32_t channels);
static SituationError _SituationSetAudioDeviceInternal(int situation_internal_id, const SituationAudioFormat* format, ma_share_mode playback_share_mode);
static SituationError _SitAudioInitPool(void);
/* HARDENING: void by design — idempotent teardown or free helper. */
static void _SitAudioCleanupPool(void);
/* HARDENING: void by design — miniaudio RT callback ABI. */
static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
static void _sit_miniaudio_capture_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
static ma_result _situation_stream_read_thunk(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);
static ma_result _situation_stream_seek_thunk(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);
static SituationError _SituationInitReverb(uint32_t sample_rate, void** out_state);
/* HARDENING: void by design — reverb state teardown; idempotent free. */
static void _SituationUninitReverb(void* state_ptr);
/* HARDENING: void by design — real-time audio callback; errors via device state only. */
static void _SituationProcessReverb(void* state_ptr, float* pOutput, const float* pInput, uint32_t frameCount, int channels);
static SituationError _SituationInitSoundEffects(_SituationSound* sound);
static void _SituationAsyncAudioWorker(void* data, void* unused);
static void _SituationMixToneToBuffer(SituationTone* t, float* buffer, uint32_t frameCount);

//----------------------------------------------------------------------------------
// Window/Display/Monitor (situation_impl_wdm.h)
//----------------------------------------------------------------------------------
static BOOL CALLBACK _SituationMonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
static void _SituationDxgiFillDisplayHdrMetadata(SituationDisplayInfo* display, HMONITOR hMonitor);
static SituationError _SituationCachePhysicalDisplays(void);
static int _SituationResolveWindowMonitorId(void);
static bool _SituationWindowMonitorDxgiHdrEnabled(void);
static GLFWmonitor* _SituationGetWindowGLFWMonitor(void);
static bool _SituationIsExclusiveFullscreen(void);
static void _SituationGetDisplayPresentSize(int* out_w, int* out_h);
static void _SituationGetRenderCanvasSize(int* out_w, int* out_h);
static bool _SituationRenderCanvasStretchActive(void);
static uint32_t _SituationComputeWindowStateFlags(void);

//----------------------------------------------------------------------------------
// Color module (situation_impl_color.h)
//----------------------------------------------------------------------------------
static float _SitSrgbUnitToLinear(float s);

//----------------------------------------------------------------------------------
// Image Module (situation_impl_image.h)
//----------------------------------------------------------------------------------
static bool _SituationIsStbImageLoadExtensionImpl(const char* extension);
static SituationError _SituationSaveImageBMP(const char* fileName, const SituationImage* image);
static unsigned char _SituationBilinearSample(const unsigned char *bitmap, int width, int height, float u, float v);
static bool _SituationFontIsGridAtlas(const SituationFont* font);
static int _SituationFontGridCellWidth(const SituationFont* font);
static int _SituationFontGridCellHeight(const SituationFont* font);
static float _SituationFontGridLineAdvance(const SituationFont* font, float scale_factor);
static void _SituationFontEmitGridGlyph(
    float* vertices, int* v_idx,
    const SituationFont* font,
    unsigned char c,
    float* x, float* y, float line_start_x,
    float scale_factor, float spacing);

//----------------------------------------------------------------------------------
// Virtual Display (defined in situation_impl_vd.h; used from renderer before VD include)
//----------------------------------------------------------------------------------
static void _SitVDFallbackColorNormalized(const SituationVirtualDisplay* vd, float out_rgba[4]);
static void _SitVDFillPathBPushConstants(uint8_t* out, const mat4 model_matrix, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle);
static void _SitVDFillPathAPushConstants(uint8_t* out, const mat4 model_matrix, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle);
static double _SitVDGetTimeSeconds(void);
static void _SitVDMarkContentUpdated(SituationVirtualDisplay* vd);
static bool _SitVDIsInsideActivePass(int display_id);
static SituationVirtualDisplayAttachmentDefaults _SitVDDefaultAttachmentDefaults(void);
static bool _SitVDHasDepthAttachment(const SituationVirtualDisplay* vd);
#if defined(SITUATION_USE_VULKAN)
static VkFormat _SitVDVkColorFormat(SituationVirtualDisplayColorFormat fmt);
static VkFormat _SitVDVkDepthFormat(const SituationVirtualDisplay* vd);
static void _SitVkDestroyVDDynamicPipelinesOnSlot(_SituationShaderSlot* slot);
static VkPipeline _SitVulkanResolveVDDynamicPipeline(_SituationShaderSlot* slot, VkPipeline rp_pipeline, const SituationVirtualDisplay* vd);
#endif
static void _SitVDMarkContentUpdatedFromTextureSlot(int slot_index);
static void _SitVDMarkComputeBindingsWritten(const int* slots, int slot_count);
static void _SitVDEndRenderPassCheck(int display_id, bool had_draw);
#if defined(SITUATION_USE_OPENGL)
static void _SitVDResetGLRecordingState(SituationGLSoftCommandBuffer* buf);
#endif
static void _SitVDRecordingNoteDrawCmd(SituationCommandBuffer cmd);
static void _SitVDNoteComputeTextureBind(SituationCommandBuffer cmd, uint32_t binding, int texture_slot_index);
static void _SitVDNoteComputeDispatch(SituationCommandBuffer cmd);
static int _SituationSortVirtualDisplaysCallback(const void* a, const void* b);
static void _SitVDGetCompositorIdleState(const SituationVirtualDisplay* vd, int* out_is_idle, double* out_elapsed_idle);
#if defined(SITUATION_USE_OPENGL)
static void _SitVDApplyCompositorIdleUniformsGL(GLuint program, const SituationVirtualDisplay* vd, int is_idle, double elapsed_idle);
static SituationError _SitGLExecRenderVirtualDisplays(int frame_index); /* Siamese S2 — body in situation_impl_vd.h */
#endif

//----------------------------------------------------------------------------------
// Renderer Forward Declarations
//----------------------------------------------------------------------------------
#include "situation_impl_renderer_fwd.h"

#endif // SITUATION_IMPL_FORWARD_H
