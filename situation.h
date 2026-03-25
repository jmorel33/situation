/***************************************************************************************************
*
*   -- The "Situation" Advanced Platform Awareness, Control, and Timing --
*   Core API library (see version in Version Macros)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   SPLIT HEADER ARCHITECTURE (v2.3.43+)
*   ====================================
*   This file now acts as a bridge that includes:
*   - situation_api.h   : Public API declarations, types, enums, function prototypes
*   - situation_impl.h  : Implementation section (only compiled when SITUATION_IMPLEMENTATION is defined)
*
*   This split improves code organization and allows for cleaner separation of concerns.
*   Users should include this file (situation.h) as before - the split is transparent.
*
***************************************************************************************************/
// --- Version Macros ---
#define SITUATION_VERSION_MAJOR 2
#define SITUATION_VERSION_MINOR 4
#define SITUATION_VERSION_PATCH 1
#define SITUATION_VERSION_REVISION ""
#ifndef SITUATION_H
#define SITUATION_H

// ================================================================================================
// EXTERNAL DEPENDENCIES AND PLATFORM CONFIGURATION
// ================================================================================================

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

// Include the implementation section (if requested)
#ifdef SITUATION_IMPLEMENTATION
    #include "sit/situation_impl.h"
#endif

#endif // SITUATION_H

