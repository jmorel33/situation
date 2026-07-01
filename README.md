<div align="center">
  <img src="doc/situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# Situation

**Advanced Platform Awareness, Control, and Timing**

_(c) 2025-2026 Jacques Morel — MIT Licensed_

Situation is a single-file, cross-platform **C11** library providing unified, low-level access to windowing, graphics (OpenGL 4.6 / Vulkan 1.4), audio, input, filesystem, threading, and timing. Version 2.4.

---

## Documentation

| Document | Description |
| :--- | :--- |
| [Introduction](doc/introduction.md) | What Situation is, getting started, build configuration, language wrappers, FAQ |
| [Architecture](doc/architecture.md) | Internal design, threading model, audio graph, GL/VK backend lifecycles |
| [Compilation Guide](doc/COMPILATION_GUIDE.md) | Application build reference (linking, flags, platforms, language wrappers) |
| [Building the Library](doc/BUILD_SITUATION_GUIDE.md) | Full reference for building situation itself (Makefile, targets, Vulkan SDK, shaderc) |
| [SDK Reference](doc/situation_sdk.md) | Primary technical reference manual |
| [API Guide](doc/situation_api.md) | All functions, structs, and enums with usage examples |
| [What's New](doc/whatsnew.md) | Recent features and roadmap completions |
| [Update Log](doc/UPDATELOG.md) | Full version history |

Build scripts live in [`build/`](build/README.md) — quick-reference for all build targets. Code generation and binding tools live in [`tools/`](tools/README.md).

---

## Quick Start

```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"

int main(int argc, char** argv) {
    SituationInitInfo cfg = { .window_width = 1280, .window_height = 720,
                              .window_title = "Hello Situation" };
    if (SituationInit(argc, argv, &cfg) != SITUATION_SUCCESS) return -1;

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        if (SituationAcquireFrameCommandBuffer()) {
            // record render commands here
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
```

Build against the pre-built static lib — no need to recompile the library:

```bat
build\build_situation.bat static-opengl
build\build_examples.bat  static-opengl hello_situation
```

See [Introduction](doc/introduction.md) for the full getting-started walkthrough and [Compilation Guide](doc/COMPILATION_GUIDE.md) for all build targets.

---

## License

MIT — see [LICENSE](LICENSE).
