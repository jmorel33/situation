/*
 * build/tracy_client.cpp - Single translation unit for the Tracy profiler client (P10.2).
 *
 * Build-only glue (not part of the sit/ library sources). Compile and link only when
 * SIT_TRACY=1 / SITUATION_ENABLE_TRACY is defined (see sit/Makefile).
 */
#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif
#include "../ext/tracy/public/TracyClient.cpp"
