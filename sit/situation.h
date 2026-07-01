/***************************************************************************************************
*
*   -- The "Situation" Advanced Platform Awareness, Control, and Timing --
*   Core API library (version: sit/situation_base_version.h)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   SPLIT HEADER ARCHITECTURE (v2.3.43+ / P2.2)
*   ===========================================
*   This file acts as a bridge that includes:
*   - situation_api.h      : Public API umbrella (≤120 lines; fans out to situation_api_*.h)
*   - situation_profiling.h: Optional Tracy CPU zone macros (P10.2; not part of the API surface)
*   - situation_impl.h     : Implementation (when SITUATION_IMPLEMENTATION is defined)
*
*   API submodules (included only via situation_api.h):
*   config, types_system, types_gpu, types_audio, platform, graphics, audio, system, deprecated.
*
*   Include this file (situation.h) as before — the split is transparent.
*
***************************************************************************************************/
#include "sit/situation_base_version.h"

#ifndef SITUATION_H
#define SITUATION_H

// ================================================================================================
// EXTERNAL DEPENDENCIES AND PLATFORM CONFIGURATION
// ================================================================================================

// Require Windows Vista+ APIs (needed for SHGetKnownFolderPath, etc.)
#if defined(_WIN32)
    #if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #if !defined(WINVER) || WINVER < 0x0600
        #undef WINVER
        #define WINVER 0x0600
    #endif
#endif

#include <stddef.h> // for audio stream
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 	// For fmodf, fmaxf, fminf
#include <float.h> 	// For FLT_MAX
#ifndef M_PI // Define M_PI if not already defined (common for MSVC)
    #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
    #define M_PI_2 1.57079632679489661923
#endif

/**
 * @brief Core dependencies
 */
#include <cglm/cglm.h>
// FFTW3 removed - maximizer.h now has its own built-in FFT implementation
#define GLFW_INCLUDE_NONE   // This prevents GLFW from trying to include the system GL/gl.h header, which is missing in some environments (including this one) and is unnecessary when using glad.
#if defined(SITUATION_USE_VULKAN)
    #define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>
#include <miniaudio.h>

/**
 * @brief Backend-specific includes
 */
#if defined(SITUATION_USE_OPENGL)
    #include <glad.h>
#elif defined(SITUATION_USE_VULKAN)
    #include <vulkan/vulkan.h>
#endif

/**
 * @brief Optional Shader Compiler (shaderc)
 */
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    #include <shaderc/shaderc.h>
#endif

/**
 * @brief Platform-specific includes
 */
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <ws2tcpip.h>       // For network adapter info
    #include <iphlpapi.h>       // For GetAdaptersAddresses
    #include <objbase.h>        // For CoInitialize/CoUninitialize
    #include <shellapi.h>       // For ShellExecute
    #ifdef SITUATION_ENABLE_DXGI // Optional DXGI for GPU info
        #include <dxgi.h>
    #endif
    #include <Xinput.h>         // For gamepad vibration
    #include <setupapi.h>       // For SetupDiGetClassDevs
    #include <devguid.h>        // For GUID_DEVCLASS_xxx
    #include <shlobj.h>         // For SHGetKnownFolderPath
#else                           // For non-Windows (e.g., Linux for GetUserDirectory)
    #include <unistd.h>         // For getuid (potentially), sysconf
    #include <sys/types.h>      // For getpwuid (potentially)
    #include <pwd.h>            // For getpwuid (potentially)
    #include <sys/statvfs.h>    // For storage info on Linux/macOS
    #include <sys/wait.h>       // For waitpid (SituationExecuteCommand)
    #include <fcntl.h>          // For open, O_RDWR (SituationExecuteCommand)
    #if defined(__linux__)
        #include <sys/sysinfo.h>    // For RAM info on Linux
    #endif
    #if defined(__APPLE__)
        #include <sys/sysctl.h> // For sysctlbyname
        #include <sys/mount.h>  // For statfs
    #endif
    #if defined(__linux__) || defined(__APPLE__)
        #include <ifaddrs.h>    // For getifaddrs
        #include <netinet/in.h> // For sockaddr_in
        #include <arpa/inet.h>  // For inet_ntoa
        #include <net/if.h>     // For IFF_LOOPBACK
    #endif
#endif

// Include the public API declarations
#include "sit/situation_api.h"

// Optional compile-time CPU profiling (Tracy zones; no-ops unless SITUATION_ENABLE_TRACY)
#include "sit/situation_profiling.h"

// Include the implementation section (if requested)
#ifdef SITUATION_IMPLEMENTATION
    #include "sit/situation_impl.h"
    #include "sit/situation_impl_audio.h"
#endif

#endif // SITUATION_H
