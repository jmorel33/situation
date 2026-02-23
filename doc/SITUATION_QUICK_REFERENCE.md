# Situation API Quick Reference v2.3.55

**Single-header library for cross-platform graphics (Vulkan/OpenGL 4.6), windowing, input, and system utilities.**

## Core Initialization

```c
SituationInitInfo info = {
    .window_title = "My App",
    .window_width = 1024,
    .window_height = 768,
    .target_fps = 60,
    .backend = SITUATION_BACKEND_AUTO  // or VULKAN/OPENGL
};
SituationInit(&info);
SituationShutdown();
```

## Frame Loop

```c
while (SituationBeginFrame()) {
    SituationCommandBuffer cmd = SituationAcquireFrameCommandBuffer();
    // ... render commands ...
    SituationEndFrame();
}
```

## Validation Helpers (v2.3.39+)

```c
bool SituationIsTextureValid(SituationTexture tex);
bool SituationIsBufferValid(SituationBuffer buf);
bool SituationIsComputePipelineValid(SituationComputePipeline pipeline);
```

## Images & Textures

```c
// Load from file
SituationImage img;
SituationLoadImage("texture.png", &img);

// Create blank image (uninitialized data)
SituationCreateImage(width, height, 4, &img);  // RGBA

// Load from memory buffer
SituationLoadImageFromMemory("png", data, size, &img);

// Set color encoding (v2.3.40+)
img.color_encoding = SITUATION_COLOR_SRGB;    // For sampled textures (default)
img.color_encoding = SITUATION_COLOR_LINEAR;  // For storage images (required)

// Create GPU texture
SituationTexture tex;
SituationCreateTexture(img, false, &tex);
SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE, &tex);

// Cleanup
SituationUnloadImage(img);
SituationDestroyTexture(tex);

// Get bindless handle
uint64_t handle = SituationGetTextureHandle(tex);
```

### Color Encoding (v2.3.40+)

Images have a `color_encoding` field that determines GPU texture format:

| Encoding | Vulkan Format | OpenGL Format | Use Case |
|----------|---------------|---------------|----------|
| `SITUATION_COLOR_LINEAR` | `VK_FORMAT_R8G8B8A8_UNORM` | `GL_RGBA8` | Storage images, compute writes |
| `SITUATION_COLOR_SRGB` | `VK_FORMAT_R8G8B8A8_SRGB` | `GL_SRGB8_ALPHA8` | Sampled textures, photos, UI |

**Important:** Storage images (writable by compute shaders) MUST use `SITUATION_COLOR_LINEAR`. SRGB formats don't support storage operations on most GPUs.

## Buffers (SSBOs)

```c
SituationBuffer buf;
SituationCreateBuffer(size, data, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &buf);
SituationUpdateBuffer(buf, offset, size, data);
SituationDestroyBuffer(buf);

// Get device address for push constants
uint64_t addr = SituationGetBufferDeviceAddress(buf);
```

## Compute Shaders

```c
// From SPIR-V file
SituationComputePipeline pipeline;
SituationCreateComputePipeline("shader.spv", SIT_COMPUTE_LAYOUT_GENERIC, &pipeline);

// From memory
SituationCreateComputePipelineFromMemory(spirv_data, spirv_size, layout, &pipeline);

// Dispatch
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdBindComputeTexture(cmd, binding, texture);
SituationCmdSetPushConstant(cmd, offset, &data, size);
SituationCmdDispatch(cmd, groups_x, groups_y, groups_z);

// Barriers
SituationCmdPipelineBarrier(cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_COMPUTE_SHADER_READ);

// Present to screen
SituationCmdPresent(cmd, output_texture);
```

## Compute Layouts

- `SIT_COMPUTE_LAYOUT_GENERIC` - General purpose (set 0)
- `SIT_COMPUTE_LAYOUT_TERMINAL` - Terminal rendering (set 1)
- `SIT_COMPUTE_LAYOUT_VECTOR` - Vector graphics (set 2)

## Text Rendering

```c
// Load font
SituationFont font = SituationLoadFont("font.ttf", 16);

// Draw text
SituationDrawText("Hello", 10, 10, 16, WHITE);
SituationDrawTextEx(font, "World", pos, 0.0f, 2.0f, WHITE);

// Measure
Vector2 size = SituationMeasureText("Text", 16);
```

## Input

```c
// Keyboard
bool down = SituationIsKeyDown(SIT_KEY_SPACE);
bool pressed = SituationIsKeyPressed(SIT_KEY_ENTER);
bool released = SituationIsKeyReleased(SIT_KEY_ESCAPE);
int key = SituationGetKeyPressed();      // Queue-based
int ch = SituationGetCharPressed();      // Unicode

// Mouse
Vector2 pos = SituationGetMousePosition();
bool btn = SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
float wheel = SituationGetMouseWheelMove();

// Window focus
bool focused = SituationHasWindowFocus();
```

## Window Management

```c
SituationSetWindowTitle("New Title");
SituationSetWindowSize(1920, 1080);
SituationToggleFullscreen();
bool fullscreen = SituationIsWindowFullscreen();
SituationMaximizeWindow();
SituationMinimizeWindow();
SituationRestoreWindow();
```

## System Info

```c
// Device info (deprecated but still works)
SituationDeviceInfo info = SituationGetDeviceInfo();

// Displays
SituationDisplayInfo* displays;
int count;
SituationGetDisplays(&displays, &count);
SituationFreeDisplays(displays, count);

// Audio devices
SituationAudioDeviceInfo* devices;
SituationGetAudioDevices(&devices, &count, true);  // true=playback, false=capture
SituationFreeAudioDevices(devices, count);

// Misc
uint32_t threads = SituationGetCPUThreadCount();
const char* gpu = SituationGetGPUName();
int width = SituationGetScreenWidth();
int height = SituationGetScreenHeight();
```

## Timing

```c
double time = SituationTimerGetTime();
float delta = SituationGetFrameTime();
SituationSetTargetFPS(60);
```

## Clipboard

```c
SituationSetClipboardText("Copy this");
const char* text = SituationGetClipboardText();
SituationFreeString(text);  // Important!
```

## Virtual Displays (Render Targets)

```c
int vd_id;
SituationCreateVirtualDisplay(resolution, 1.0, 0, SITUATION_SCALING_NEAREST, SITUATION_BLEND_ALPHA, &vd_id);
SituationDestroyVirtualDisplay(vd_id);
SituationRenderVirtualDisplays(cmd);
```

## Error Handling

```c
const char* err = SituationGetLastErrorMsg();
SituationError code = SituationGetLastError();
bool ok = SituationIsInitialized();
```

## Common Patterns

### Compute Shader with Push Constants + SSBO

```c
typedef struct {
    uint64_t buffer_addr;
    uint32_t count;
    float time;
} PushConstants;

SituationBuffer ssbo;
SituationCreateBuffer(data_size, data, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &ssbo);

PushConstants pc = {
    .buffer_addr = SituationGetBufferDeviceAddress(ssbo),
    .count = 1000,
    .time = (float)SituationTimerGetTime()
};

SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdBindComputeTexture(cmd, 0, output_tex);
SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
SituationCmdDispatch(cmd, (count + 63) / 64, 1, 1);
```

### Image → Texture → Render → Present

```c
SituationImage img;
SituationCreateImage(1024, 768, 4, &img);
memset(img.data, 0, img.width * img.height * 4);
img.color_encoding = SITUATION_COLOR_LINEAR;  // Required for storage images!

SituationTexture tex;
SituationCreateTextureEx(img, false, 
    SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE, &tex);
SituationUnloadImage(img);

// Render to texture with compute shader
SituationCommandBuffer cmd = SituationAcquireFrameCommandBuffer();
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdBindComputeTexture(cmd, 1, tex);  // binding 1 = storage image
SituationCmdDispatch(cmd, width/16, height/16, 1);
SituationCmdPipelineBarrier(cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_TRANSFER_READ);
SituationCmdPresent(cmd, tex);
SituationEndFrame();
```

## Key Constants

### Color Encoding (v2.3.40+)
- `SITUATION_COLOR_LINEAR` - Linear color space (required for storage images)
- `SITUATION_COLOR_SRGB` - SRGB with gamma correction (for sampled textures)

### Texture Usage Flags
- `SITUATION_TEXTURE_USAGE_SAMPLED` - Read in shaders
- `SITUATION_TEXTURE_USAGE_STORAGE` - Write in compute shaders
- `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` - Copy source
- `SITUATION_TEXTURE_USAGE_TRANSFER_DST` - Copy destination

### Buffer Usage Flags
- `SITUATION_BUFFER_USAGE_STORAGE_BUFFER` - SSBO (shader storage)
- `SITUATION_BUFFER_USAGE_TRANSFER_DST` - Can be updated

### Barrier Flags
- `SITUATION_BARRIER_COMPUTE_SHADER_WRITE`
- `SITUATION_BARRIER_COMPUTE_SHADER_READ`
- `SITUATION_BARRIER_TRANSFER_READ`

### Key Codes
- `SIT_KEY_SPACE`, `SIT_KEY_ENTER`, `SIT_KEY_ESCAPE`
- `SIT_KEY_LEFT_CONTROL`, `SIT_KEY_LEFT_SHIFT`, `SIT_KEY_LEFT_ALT`
- `SIT_KEY_PAGE_UP`, `SIT_KEY_PAGE_DOWN`
- `SIT_KEY_F1` through `SIT_KEY_F12`

## Backend-Specific Notes

### Vulkan
- Uses bindless descriptors (no descriptor set management needed)
- Buffer device addresses in push constants
- Compute shaders use SPIR-V

### OpenGL 4.6
- Emulates bindless via ARB_bindless_texture
- SSBOs work identically to Vulkan
- Compute shaders use GLSL (auto-compiled from SPIR-V)

## Migration from Old APIs

| Old (Deprecated) | New (v2.3.39) |
|------------------|---------------|
| `tex.id != 0` | `SituationIsTextureValid(tex)` |
| `buf.handle != 0` | `SituationIsBufferValid(buf)` |
| `pipeline.id != 0` | `SituationIsComputePipelineValid(pipeline)` |
| Manual descriptor sets | Bindless (automatic) |

---

**Pro Tip:** Situation uses a "single header" design. Just `#define SITUATION_IMPLEMENTATION` before including `situation.h` in ONE .c file.
