## Deprecated APIs

**Overview:** This section lists public APIs that have been removed or superseded since v2.4.106. Deprecated functions still compile and run but emit warnings or behave suboptimally; removed functions no longer exist in the header. When upgrading an existing project, search your codebase for the symbols below and migrate to the recommended replacements.

**Migration approach:**
1. Search for deprecated symbol names in your project.
2. Replace with the listed replacement (see [situation_command_reference.md](../situation_command_reference.md) §11 for rendering commands).
3. Rebuild and fix any signature differences — most replacements return `SituationError` where the old API returned `void` or `bool`.
4. For aggregate queries like `SituationGetDeviceInfo`, call the split functions and assemble only the fields you need.

The following APIs have been removed or deprecated since v2.4.106:

| Removed/Deprecated | Version | Replacement |
|--------------------|---------|-------------|
| `SituationStartAudioPlayback` | Removed v2.4.198 | `SITUATION_NODE_PCM_INPUT` + `SituationPushNodePCM()` |
| `SituationCmdBeginRenderToDisplay` | Deprecated v2.4.147 | `SituationCmdBeginRenderPass()` with `SituationRenderPassInfo` |
| `SituationCmdBindUniformBuffer` | Deprecated v2.4.147 | `SituationCmdBindDescriptorSet()` |
| `SituationCmdBindTexture` | Deprecated v2.4.147 | `SituationCmdBindTextureSet()` or `SituationCmdBindSampledTexture()` |
| `SituationCmdBindComputeBuffer` | Deprecated v2.4.147 | `SituationCmdBindDescriptorSet()` with compute pipeline |
| `SituationLoadComputeShader` | Deprecated | `SituationCreateComputePipeline()` |
| `SituationLoadComputeShaderFromMemory` | Deprecated | `SituationCreateComputePipelineFromMemory()` |
| `SituationGetDeviceInfo` | Deprecated v2.4.336 | Split queries: `SituationGetCPUInfo`, `SituationGetGPUInfo`, `SituationGetMemoryInfo`, etc. |
| `SituationGetAudioDevices` | Deprecated v2.4.336 | `SituationEnumerateAudioDevices()` + `SituationFreeDeviceList()` |
| `SituationGetRendererType` | Legacy v2.4.336 | `SituationGetGraphicsBackend()` + `SituationGetGraphicsCaps()` |
