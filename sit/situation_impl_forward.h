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
*   Renderer-specific forward declarations live in situation_impl_renderer_fwd.h.
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_FORWARD_H
#define SITUATION_IMPL_FORWARD_H

//----------------------------------------------------------------------------------
// Control Module (Lifecycle, Error Handling)
//----------------------------------------------------------------------------------
static void _SituationSetError(const char* msg);
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail);
static SituationError _SituationInitPlatform(void);
static SituationError _SituationInitWindow(const SituationInitInfo* init_info);
static SituationError _SituationInitSubsystems(const SituationInitInfo* init_info);
static void _SituationCleanupPlatform(void);
static void _SituationCleanupRenderer(void);
static void _SituationCleanupSubsystems(void);
static void _SituationFullCleanupOnError(void);

//----------------------------------------------------------------------------------
// GLFW Callback Helpers
//----------------------------------------------------------------------------------
static void _SituationGLFWErrorCallback(int error, const char* description);
static void _SituationGLFWFileDropCallback(GLFWwindow* window, int count, const char** paths);
static void _SituationGLFWWindowFocusCallback(GLFWwindow* window, int focused);
static void _SituationGLFWWindowIconifyCallback(GLFWwindow* window, int iconified);
static void _SituationGLFWFramebufferSizeCallback(GLFWwindow* window, int width, int height);
static void _SituationGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void _SituationGLFWCharCallback(GLFWwindow* window, unsigned int codepoint);
static void _SituationGLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void _SituationGLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
static void _SituationGLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
static void _SituationGLFWJoystickCallback(int jid, int event);

//----------------------------------------------------------------------------------
// Threading & IO Helpers
//----------------------------------------------------------------------------------
static int _SituationIOThreadEntry(void* arg);
static char* _sit_strdup(const char* s);
static int _SituationWorkerEntry(void* arg);

#if !defined(__STDC_NO_THREADS__)
static bool _SituationInitRenderThread(const SituationInitInfo* info);
static void _SituationDestroyRenderThread(void);
static void _SitFlushFrameResources(int frame_index);
static int _SituationRenderThreadEntry(void* arg);
#endif // !__STDC_NO_THREADS__

#if defined(SITUATION_ENABLE_THREADING)
static void _SituationRenderJobWorker(void* data, void* unused);
#endif // SITUATION_ENABLE_THREADING

//----------------------------------------------------------------------------------
// Audio Helpers
//----------------------------------------------------------------------------------
static void _SitAudioInitPool(void);
static void _SitAudioCleanupPool(void);
static void sit_miniaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
static ma_result _situation_stream_read_thunk(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);
static ma_result _situation_stream_seek_thunk(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);
static void* _SituationInitReverb(uint32_t sample_rate);
static void _SituationUninitReverb(void* state_ptr);
static void _SituationProcessReverb(void* state_ptr, float* pOutput, const float* pInput, uint32_t frameCount, int channels);

//----------------------------------------------------------------------------------
// Renderer Forward Declarations
//----------------------------------------------------------------------------------
#include "situation_impl_renderer_fwd.h"

#endif // SITUATION_IMPL_FORWARD_H
