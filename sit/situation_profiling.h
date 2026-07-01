/***************************************************************************************************
*
*   situation_profiling.h - Tracy CPU zone macros (P10.2)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Optional compile-time instrumentation included from situation.h (not the API umbrella).
*   When SITUATION_ENABLE_TRACY is undefined, all macros expand to no-ops (zero cost).
*   When enabled, requires Tracy client linked (see build/tracy_client.cpp) and
*   -Iext/tracy/public on the compile line.
*
*   Use SIT_PROFILE_ZONE_SCOPED("Name") { ... } for multi-statement regions.
*   Use SIT_PROFILE_ZONE_CTX(ctx, "Name") / SIT_PROFILE_ZONE_END_CTX(ctx) when returns split the block.
*
*   Do not include this file directly — include situation.h.
*
***************************************************************************************************/
#ifndef SITUATION_PROFILING_H
#define SITUATION_PROFILING_H

#if defined(SITUATION_ENABLE_TRACY)
#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif
#include "tracy/TracyC.h"

#define SIT_PROF_ZONE_CTX(ctx, name) \
    TracyCZoneN(ctx, name, 1)
#define SIT_PROF_ZONE_END_CTX(ctx) \
    ___tracy_emit_zone_end(ctx)
#define SIT_PROF_RETURN_CTX(ctx, value) \
    do { TracyCZoneEnd(ctx); return (value); } while(0)

#define SIT_PROF_ZONE_SCOPED(name) \
    SIT_PROF_ZONE_CTX(___sit_zone_scoped_##__LINE__, name); \
    for (int ___sit_prof_scoped_##__LINE__ = 1; ___sit_prof_scoped_##__LINE__; \
         ___sit_prof_scoped_##__LINE__ = 0, SIT_PROF_ZONE_END_CTX(___sit_zone_scoped_##__LINE__))

#define SIT_PROF_FRAME_MARK() \
    TracyCFrameMark

#else

#define SIT_PROF_ZONE_CTX(ctx, name) do {} while(0)
#define SIT_PROF_ZONE_END_CTX(ctx) do {} while(0)
#define SIT_PROF_RETURN_CTX(ctx, value) return (value)
#define SIT_PROF_ZONE_SCOPED(name) \
    for (int ___sit_prof_scoped_##__LINE__ = 1; ___sit_prof_scoped_##__LINE__; ___sit_prof_scoped_##__LINE__ = 0)
#define SIT_PROF_FRAME_MARK() do {} while(0)

#endif

#define SIT_PROFILE_ZONE_CTX(ctx, name) SIT_PROF_ZONE_CTX(ctx, name)
#define SIT_PROFILE_ZONE_END_CTX(ctx) SIT_PROF_ZONE_END_CTX(ctx)
#define SIT_PROFILE_RETURN_CTX(ctx, value) SIT_PROF_RETURN_CTX(ctx, value)
#define SIT_PROFILE_ZONE_SCOPED(name) SIT_PROF_ZONE_SCOPED(name)
#define SIT_PROFILE_FRAME_MARK() SIT_PROF_FRAME_MARK()

#endif /* SITUATION_PROFILING_H */
