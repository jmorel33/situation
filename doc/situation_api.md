# Situation — Advanced Platform Awareness, Control, and Timing

_Core API library v2.4.407 · (c) 2025-2026 Jacques Morel · MIT Licensed_

**Situation** is a **strict C11** single-file library providing unified access to windowing, graphics (OpenGL 4.6 / Vulkan 1.4), audio (23-effect node graph, 16-voice MIDI synth), input, filesystem, NUMA-aware threading, and high-resolution timing. One header, one DLL, one `SituationInit()` call.

Ships as header-only or pre-built DLL with auto-generated FFI bindings for **Odin**, **Zig**, **Rust**, **Fortran**, and **Modula-2** (`wrappers/`).

> **581** public `SITAPI` functions · Windows 10+ · OpenGL 4.6 or Vulkan 1.4 hardware required

**Documentation map**

| Resource | Description |
|----------|-------------|
| **[guide/_front_matter.md](guide/_front_matter.md)** | Introduction, build integration, quick start |
| **[situation_api.md](situation_api.md)** (this file) | Module map — links to every guide section |
| **[situation_api_index.md](situation_api_index.md)** | Categorized function index (auto-generated) |
| **[situation_command_reference.md](situation_command_reference.md)** | All `SituationCmd*` rendering commands |
| **[situation_sdk.md](situation_sdk.md)** | SDK manual (architecture, workflows, examples) |
| **[whatsnew.md](whatsnew.md)** / **[UPDATELOG.md](UPDATELOG.md)** | Release history |

**Module guides** (`doc/guide/` — 29 files):

| Category | Guides |
|----------|--------|
| **Intro** | [_front_matter](guide/_front_matter.md) |
| **Core systems** | [core](guide/core.md) · [window & display](guide/window_display.md) · [input](guide/input.md) · [image](guide/image.md) · [fonts](guide/font.md) · [system introspection](guide/system_introspection.md) |
| **Graphics** | [graphics](guide/graphics.md) · [virtual display](guide/virtual_display.md) · [test patterns](guide/test_patterns.md) · [advanced GPU commands](guide/renderer_bolster.md) · [compute](guide/compute.md) · [2D grid](guide/grid.md) · [fonts](guide/font.md) · [text rendering](guide/text_rendering.md) · [2D drawing](guide/drawing_2d.md) · [3D drawing](guide/drawing_3d.md) |
| **Media & I/O** | [audio](guide/audio.md) · [audio node graph](guide/audio_graph.md) · [MIDI](guide/midi.md) · [filesystem](guide/filesystem.md) |
| **Utilities** | [threading](guide/threading.md) · [profiling & diagnostics](guide/profiling.md) · [YPQ color](guide/ypq_color.md) · [HD color output](guide/hd_color_output.md) · [hot-reload](guide/hot_reload.md) · [logging](guide/logging.md) · [miscellaneous](guide/miscellaneous.md) · [deprecated](guide/deprecated.md) |
| **Learning** | [examples & FAQ](guide/examples_faq.md) |

For release history and changelogs, see **[whatsnew.md](whatsnew.md)** and **[UPDATELOG.md](UPDATELOG.md)**.

---

# Situation v2.4.407 API Programming Guide

Module reference lives under **`doc/guide/`** (P2.5 split). This file is the stable entry URL — same role as `#include "sit/situation_api.h"`.

## Getting started

- [Introduction and core concepts](guide/_front_matter.md#introduction-and-core-concepts)
- [Building the library](guide/_front_matter.md#building-the-library)
- [Getting started (quick start)](guide/_front_matter.md#getting-started)

## Module reference

### Core systems
- [Core](guide/core.md)
- [Window and display](guide/window_display.md)
- [Input](guide/input.md)
- [Image](guide/image.md)
- [Fonts](guide/font.md)
- [System introspection](guide/system_introspection.md)

### Graphics and rendering
- [Graphics](guide/graphics.md)
- [Virtual Display compositor](guide/virtual_display.md) — off-screen layers, scaling, blend modes
- [Test patterns](guide/test_patterns.md) — calibration shaders, VD idle SMPTE, `#include` library
- [Advanced GPU commands](guide/renderer_bolster.md) — barriers, transfers, raster state, indirect draw
- [Compute shaders](guide/compute.md)
- [2D grid (cell playfields)](guide/grid.md) — stacked tile grids, compute VD, scroll
- [Fonts](guide/font.md)
- [Text rendering](guide/text_rendering.md) — GPU command-buffer text
- [2D rendering and drawing](guide/drawing_2d.md)
- [3D rendering and drawing](guide/drawing_3d.md)

### Media and I/O
- [Audio](guide/audio.md)
- [Audio node graph](guide/audio_graph.md)
- [MIDI integration](guide/midi.md)
- [Filesystem](guide/filesystem.md)

### Utilities
- [Threading](guide/threading.md)
- [Profiling & diagnostics](guide/profiling.md) — frame metrics, Tracy zones, GPU timestamp zones, debug builds
- [YPQ color](guide/ypq_color.md)
- [HD color output (10-bit & HDR10)](guide/hd_color_output.md)
- [Hot-reloading](guide/hot_reload.md)
- [Logging](guide/logging.md)
- [Miscellaneous](guide/miscellaneous.md)
- [Deprecated APIs](guide/deprecated.md)

### Learning and support
- [Examples and tutorials](guide/examples_faq.md#examples-tutorials)
- [FAQ and troubleshooting](guide/examples_faq.md#frequently-asked-questions-faq-troubleshooting)

---

## Complete API Index (generated)

Every public **`SITAPI`** function is indexed in **[situation_api_index.md](situation_api_index.md)** (auto-generated from `sit/situation_api.h`).

After header changes, regenerate bindings and docs:

```bat
tools\run_all.bat
python tools\merge_api_doc_gaps.py
```

---

## License (MIT)

"Situation" is licensed under the permissive MIT License. In simple terms, this means you are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for both commercial and private projects. The only requirement is that you include the original copyright and license notice in any substantial portion of the software or derivative work you distribute. This library is provided "as is", without any warranty.

---

Copyright (c) 2025-2026 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
