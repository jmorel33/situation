# Situation API — Generated Supplement

_Auto-generated from `sit/situation_api.h` — Situation **2.4.126 (Vertex Index Bind SituationError)**._

This file documents **SITAPI symbols present in the header but not yet covered** in
[situation_api.md](situation_api.md). Re-generate with:

```bat
python scripts\generate_situation_api_docs.py
```

**Coverage:** 423/451 symbols documented in situation_api.md; **28** entries below.

---

## [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.

#### `SituationCmdBeginRenderToDisplay`
[DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it.
```c
__attribute__((deprecated)) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);
```

---

#### `SituationLoadComputeShader`
[DEPRECATED] Load a compute shader from a file. Use SituationCreateComputePipeline instead.
```c
SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);
```

---

#### `SituationLoadComputeShaderFromMemory`
[DEPRECATED] Create a compute shader from memory. Use SituationCreateComputePipelineFromMemory instead.
```c
SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);
```

---

## Abstracted Rendering Commands

#### `SituationCmdBindDescriptorSetDynamic`
[Core] Binds a dynamic buffer descriptor set with an offset.
```c
SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset);
```

---

#### `SituationCmdBindSampledTexture`
Binds a texture as a sampled image (sampler2D) to a binding point.
```c
SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```

---

#### `SituationCmdSetVertexAttribute`
[OpenGL Only] Attribute format + vertex buffer binding index (must match SituationCmdBindVertexBuffer).
```c
SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, uint32_t binding, int size, SituationDataType type, bool normalized, size_t offset);
```

---

## Command Buffer Recording

#### `SituationCmdBeginDebugGroup`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
```

---

#### `SituationCmdEndDebugGroup`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
```

---

#### `SituationCmdPopRasterState`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdPushRasterState`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdSetBlendEnable`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable);
```

---

#### `SituationCmdSetBlendFuncSeparate`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd, SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb, SituationBlendFactor src_a, SituationBlendFactor dst_a);
```

---

#### `SituationCmdSetCullMode`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);
```

---

#### `SituationCmdSetDepthTest`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op);
```

---

#### `SituationCmdSetDepthWrite`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable);
```

---

#### `SituationCmdSetPushConstantData`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size);
```

---

## Frame Lifecycle & Command Buffer

#### `SituationGetComputeCommandBuffer`
[v2.3.23] Get the compute-specific command buffer (Vulkan only).
```c
SituationCommandBuffer SituationGetComputeCommandBuffer(void);
```

---

## MIDI Device Control

#### `SituationGetMidiDeviceName`
PortMidi device name for device_id (hardware or virtual).
```c
SituationError SituationGetMidiDeviceName(int device_id, char* out_name, size_t out_name_size);
```

---

#### `SituationSetNodeMidiChannel`
Filter MIDI to channel 0-15, or -1 omni.
```c
SituationError SituationSetNodeMidiChannel(SituationAudioGraph* graph, SituationNodeHandle handle, int channel);
```

---

## Virtual MIDI loopback (integration testing; no hardware keyboard required)

#### `SituationSetupVirtualMidiLoopback`
Create connected virtual out→in pair. Returns input device_id for SituationEnableMidiControl().
```c
SituationError SituationSetupVirtualMidiLoopback(int* out_input_device_id);
```

---

#### `SituationTeardownVirtualMidiLoopback`
Close and destroy the virtual loopback devices.
```c
void SituationTeardownVirtualMidiLoopback(void);
```

---

#### `SituationVirtualMidiControlChange`
CC (e.g. mod wheel, expression).
```c
SituationError SituationVirtualMidiControlChange(uint8_t channel, uint8_t controller, uint8_t value);
```

---

#### `SituationVirtualMidiNoteOff`
Inject note-off on channel 0 (legacy wrapper).
```c
SituationError SituationVirtualMidiNoteOff(uint8_t note);
```

---

#### `SituationVirtualMidiNoteOffEx`
Channel-aware note-off (0-15).
```c
SituationError SituationVirtualMidiNoteOffEx(uint8_t channel, uint8_t note);
```

---

#### `SituationVirtualMidiNoteOn`
Inject note-on on channel 0 (legacy wrapper).
```c
SituationError SituationVirtualMidiNoteOn(uint8_t note, uint8_t velocity);
```

---

#### `SituationVirtualMidiNoteOnEx`
Channel-aware note-on (0-15).
```c
SituationError SituationVirtualMidiNoteOnEx(uint8_t channel, uint8_t note, uint8_t velocity);
```

---

#### `SituationVirtualMidiPitchBend`
Pitch bend 0..16383 (center 8192).
```c
SituationError SituationVirtualMidiPitchBend(uint8_t channel, int16_t bend);
```

---

#### `SituationVirtualMidiProgramChange`
Program change on channel 0-15.
```c
SituationError SituationVirtualMidiProgramChange(uint8_t channel, uint8_t program);
```

---
