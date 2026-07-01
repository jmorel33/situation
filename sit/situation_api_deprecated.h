/***************************************************************************************************
*
*   situation_api_deprecated.h - Deprecated API (Legacy Symbols)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Legacy SITAPI declarations retained for ABI compatibility. Each symbol carries SIT_DEPRECATED
*   with a migration hint. Omit this file by defining SITUATION_INCLUDE_DEPRECATED_API to 0
*   before including situation.h.
*
*   Requires SITAPI and SIT_DEPRECATED from situation_api.h.
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_DEPRECATED_H
#define SITUATION_API_DEPRECATED_H

#include "situation_api_config.h"
#include "situation_base_types.h"
#include "situation_api_types_system.h"
#include "situation_api_types_gpu.h"
#include "situation_api_types_audio.h"
#include "situation_api_platform.h"

//==================================================================================
// Deprecated API (still supported — see doc/situation_api_index.md)
//==================================================================================

// --- Lifecycle ---
SITAPI SIT_DEPRECATED("Use SituationPollInputEvents() and SituationUpdateTimers()") void SituationUpdate(void);

// --- System ---
SITAPI SIT_DEPRECATED("Prefer SituationGetCPUInfo(), SituationGetGPUInfo(), SituationGetMemoryInfo(), and related split queries") SituationDeviceInfo SituationGetDeviceInfo(void);

// --- Audio ---
SITAPI SIT_DEPRECATED("Use SituationEnumerateAudioDevices() and SituationFreeDeviceList()") SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);

// --- Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) ---
SITAPI SIT_DEPRECATED("Use SituationCmdBeginRenderPass()") SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);
SITAPI SIT_DEPRECATED("Use SituationCmdEndRenderPass()") SituationError SituationCmdEndRender(SituationCommandBuffer cmd);
SITAPI SIT_DEPRECATED("Use SituationCmdBindDescriptorSet()") SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t contract_id, SituationBuffer buffer);
SITAPI SIT_DEPRECATED("Use SituationCmdBindTextureSet()") SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);
SITAPI SIT_DEPRECATED("Use SituationCmdBindDescriptorSet()") SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer);
SITAPI SIT_DEPRECATED("Use SituationCreateComputePipeline()") SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);
SITAPI SIT_DEPRECATED("Use SituationCreateComputePipelineFromMemory()") SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);
SITAPI SIT_DEPRECATED("Use SituationCmdPipelineBarrierEx(), SituationCmdBufferBarrier(), or SituationCmdTextureBarrier()") void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits);

#endif /* SITUATION_API_DEPRECATED_H */
