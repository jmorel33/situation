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
static SituationError _SituationInitPlatform(void);
static SituationError _SituationInitWindow(const SituationInitInfo* init_info);
static SituationError _SituationInitSubsystems(const SituationInitInfo* init_info);
/* HARDENING: void by design — best-effort platform teardown during shutdown. */
static void _SituationCleanupPlatform(void);
/* HARDENING: void by design — best-effort renderer teardown during shutdown. */
static void _SituationCleanupRenderer(void);
/* HARDENING: void by design — best-effort subsystem teardown during shutdown. */
static void _SituationCleanupSubsystems(void);
/* HARDENING: void by design — best-effort teardown on init failure. */
static void _SituationFullCleanupOnError(void);

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
#endif // SITUATION_ENABLE_THREADING

//----------------------------------------------------------------------------------
// Audio Helpers
//----------------------------------------------------------------------------------
static _SituationSoundSlot* _SitGetSoundSlot(SituationSound handle);
static SituationError _SituationSetAudioDeviceInternal(int situation_internal_id, const SituationAudioFormat* format, ma_share_mode playback_share_mode);
static SituationError _SitAudioInitPool(void);
/* HARDENING: void by design — idempotent teardown or free helper. */
static void _SitAudioCleanupPool(void);
/* HARDENING: void by design — miniaudio RT callback ABI. */
static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
static ma_result _situation_stream_read_thunk(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);
static ma_result _situation_stream_seek_thunk(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);
static SituationError _SituationInitReverb(uint32_t sample_rate, void** out_state);
/* HARDENING: void by design — reverb state teardown; idempotent free. */
static void _SituationUninitReverb(void* state_ptr);
/* HARDENING: void by design — real-time audio callback; errors via device state only. */
static void _SituationProcessReverb(void* state_ptr, float* pOutput, const float* pInput, uint32_t frameCount, int channels);

//----------------------------------------------------------------------------------
// Virtual Display (defined in situation_impl_vd.h; used from renderer before VD include)
//----------------------------------------------------------------------------------
static double _SitVDGetTimeSeconds(void);
static void _SitVDMarkContentUpdated(SituationVirtualDisplay* vd);
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
#endif

//----------------------------------------------------------------------------------
// Renderer Forward Declarations
//----------------------------------------------------------------------------------
#include "situation_impl_renderer_fwd.h"

#endif // SITUATION_IMPL_FORWARD_H
