#ifndef SITUATION_IMPL_H
#define SITUATION_IMPL_H

/***************************************************************************************************
*
*   situation_impl.h - Implementation Orchestrator for the Situation Library
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   This file is the single entry point for the Situation implementation.
*   It includes all module files in the correct dependency order.
*   No function bodies live here — only #includes.
*
*   Include order:
*     1. Base font data (pure data, no deps)
*     2. Third-party dependencies (STB, miniaudio, glad, VMA)
*     3. Internal declarations (types, structs, globals, shaders)
*     4. Forward declarations (cross-module prototypes)
*     5. Utilities (math, string, time helpers)
*     6. Subsystem modules (threading, IO, input, WDM, image, timer)
*     7. Renderer (GL + VK backends, resources, commands)
*     8. Virtual Display (compositing, uses renderer)
*     9. Control (lifecycle, init/shutdown, update — orchestrates everything)
*
***************************************************************************************************/

#ifdef SITUATION_IMPLEMENTATION

// ============================================================================
// 1. Embedded Font Data
// ============================================================================
#include "situation_base_font.h"

// ============================================================================
// 2. Third-Party Dependencies
// ============================================================================
#include "situation_impl_deps.h"

// ============================================================================
// 3. Internal Declarations (types, structs, globals, shaders)
// ============================================================================
#include "situation_impl_decl.h"

// ============================================================================
// 4. Forward Declarations (cross-module prototypes)
// ============================================================================
#include "situation_impl_forward.h"

// ============================================================================
// 5. Utilities (math, string, time helpers)
// ============================================================================
#include "situation_impl_etc.h"

// ============================================================================
// 6. Timer & Oscillator System
// ============================================================================
#include "situation_impl_timer.h"

// ============================================================================
// 7. Subsystem Modules
// ============================================================================
#include "situation_impl_threading.h"
#include "situation_impl_io.h"
#include "situation_impl_input.h"
#include "situation_impl_wdm.h"
#include "situation_impl_image.h"

// ============================================================================
// 8. Renderer (OpenGL + Vulkan backends, resources, commands)
// ============================================================================
#include "situation_impl_renderer.h"

// ============================================================================
// 9. Virtual Display API
// ============================================================================
#include "situation_impl_vd.h"

// ============================================================================
// 10. Control (Lifecycle, Init/Shutdown, Error Handling, Update Loop)
// ============================================================================
#include "situation_impl_ctrl.h"

#endif // SITUATION_IMPLEMENTATION
#endif // SITUATION_IMPL_H
