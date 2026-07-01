/***************************************************************************************************
*
*   situation_impl_renderer.h - Renderer orchestrator (OpenGL + Vulkan)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Includes renderer domain slices in dependency order:
*   core, lc, shader, resources, frame_cmd.
*
*   Do not include directly — use situation_impl.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_H
#define SITUATION_IMPL_RENDERER_H


#include "situation_impl_renderer_core.h"
#include "situation_impl_gpu_prof.h"
#include "situation_impl_query_pool.h"
#include "situation_impl_renderer_lc.h"
#include "situation_impl_renderer_shader.h"
#include "situation_impl_renderer_resources.h"
#include "situation_impl_render_target.h"
#include "situation_impl_renderer_frame_cmd.h"

#endif // SITUATION_IMPL_RENDERER_H
