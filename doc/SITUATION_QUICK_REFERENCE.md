# Situation API Quick Reference v2.4.10

**Single-header library for cross-platform graphics (Vulkan 1.4/OpenGL 4.6), windowing, input, audio, and system utilities.**

## Project Structure (v2.4.10)

```
situation/                              # Project root
├── situation.h                         # ← Public entry point (include this)
│
├── sit/                               # ← Core implementation (internal)
│   ├── situation_api.h                # Public API declarations (~2,690 lines)
│   │   └── situation_base_errno.h     # Error enum (SituationError)
│   │
│   ├── situation_impl.h              # Orchestrator (80 lines, includes only)
│   ├── situation_base_font.h         # Embedded 8x8 VGA font (CP437)
│   ├── situation_impl_deps.h         # Third-party libs (STB, miniaudio, glad, VMA)
│   ├── situation_impl_decl.h         # Internal types, structs, globals, shaders
│   ├── situation_impl_forward.h      # Cross-module forward declarations
│   │   └── situation_impl_renderer_fwd.h  # Renderer-specific forward decls
│   ├── situation_impl_etc.h          # Utilities (math, strings, SituationFreeString)
│   ├── situation_impl_timer.h        # Oscillators, high-res time
│   ├── situation_impl_threading.h    # Thread pool, job system
│   ├── situation_impl_io.h           # File I/O, async, system info
│   ├── situation_impl_input.h        # Keyboard, mouse, gamepad, GLFW callbacks
│   ├── situation_impl_wdm.h          # Window, display, monitor management
│   ├── situation_impl_image.h        # Image, font, color, screenshot
│   ├── situation_impl_renderer.h     # GL + VK backends, resources, commands (~17K lines)
│   ├── situation_impl_vd.h           # Virtual display compositing
│   ├── situation_impl_ctrl.h         # Lifecycle, init/shutdown, update loop
│   ├── situation_impl_audio.h        # Audio subsystem
│   │
│   ├── aud/                          # Audio Subsystem
│   │   ├── fx/                       # Effects (15 files)
│   │   ├── polysonix/                # Polyphonic synthesizer
│   │   ├── node_graph*.h             # Node graph system
│   │   ├── device_*.h                # Device system
│   │   └── ...                       # Audio components
│   │
│   └── k-term/                       # Terminal Subsystem
│       └── ...                       # Terminal components
│
├── examples/                          # Example programs
├── ext/                              # External dependencies
├── doc/                              # Documentation
├── concat_situation.ps1              # Concatenate to single file (PowerShell)
└── concat_situation.sh               # Concatenate to single file (Bash)
```

## Compilation

### Basic Compilation (Windows with GCC)

```bash
# Compile with OpenGL backend
gcc -o myapp.exe myapp.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -DSITUATION_IMPLEMENTATION \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32

# Compile with Vulkan backend
gcc -o myapp.exe myapp.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_VULKAN \
    -DSITUATION_IMPLEMENTATION \
    -L%VULKAN_SDK%/Lib -lvulkan-1 \
    -lglfw3 -lgdi32 -lwinmm -lws2_32
```

### Required Dependencies

- **GLFW3** - Windowing and input
- **OpenGL** or **Vulkan SDK** - Graphics backend
- **cglm** - Math library (vectors, matrices)
- **miniaudio** - Audio engine (embedded)
- **stb_image** - Image loading (embedded)

### Include Pattern

```c
// In your main.c or one implementation file:
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL  // or SITUATION_USE_VULKAN
#include "situation.h"

// In other files, just include normally:
#include "situation.h"
```

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

## Audio Subsystem (v2.4.0)

### Organization

The audio subsystem is organized in `sit/aud/`:

- **Effects** (`sit/aud/fx/`) - 16 audio processors
  - Time-based: reverb, echo, studio_reverb, spring_reverb, sst282
  - Modulation: chorus, phaser, lfo
  - Distortion: overdrive, exciter
  - Dynamics: dynamics, filter, eq_4band
  - Mastering: maximizer, mastering_amp, deafmax

- **Synthesizers**
  - `sit/aud/tone_synth.h` - Simple tone generator
  - `sit/aud/polysonix/` - Full polyphonic synth engine

- **Node Graph** - Modular audio routing system
  - `sit/aud/node_graph.h` - Base types
  - `sit/aud/node_graph_impl.h` - Graph topology
  - `sit/aud/node_graph_process.h` - Audio processing
  - `sit/aud/node_graph_serialization.h` - Save/load graphs

- **Device System**
  - `sit/aud/device_registry.h` - Device registration
  - `sit/aud/device_wrappers.h` - Device wrappers
  - `sit/aud/registry_init.h` - Initialization

### Audio Examples

```c
// Simple sound playback
SituationSound sound;
SituationLoadSoundFromFile("sound.wav", &sound);
SituationPlayLoadedSound(&sound);

// Node graph with effects
SituationAudioGraph* graph = SituationCreateGraph(48000);
SituationAudioNode* tone = SituationCreateNode(graph, "Tone Synth");
SituationAudioNode* reverb = SituationCreateNode(graph, "Reverb");
SituationCreatePatch(graph, tone, 0, reverb, 0);
SituationProcessGraph(graph, output_buffer, frames);
```

## Terminal Subsystem (K-Term)

Located in `sit/k-term/` - Full VT100/VT220/VT320 terminal emulation with:
- 256-color and true color support
- Sixel graphics
- Voice synthesis and VoIP
- Network utilities (telnet, SSH)

```c
#define KTERM_IMPLEMENTATION
#include "sit/k-term/kterm.h"

KTermConfig config = { .width = 80, .height = 50 };
KTerm* term = KTerm_Create(config);
KTerm_WriteString(term, "\x1B[1;33mHello World!\x1B[0m\n");
```
