# 18 — Text Showcase

**Tier 5 — Capstone** | OpenGL + Vulkan | Promoted from `examples/other/text_showcase.c`

A single-screen catalog of **GPU text rendering**: colors, font sizes, letter spacing, sine-wave motion, and per-glyph rainbow coloring.

## Build & run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 18_text_showcase
build\examples\18_text_showcase.exe
```

Vulkan: replace `static-opengl` with `static-vulkan`.

## Keys

| Key | Action |
|-----|--------|
| `1` | Features + size/spacing ramp |
| `2` | Animated / rainbow section |
| `3` | Runtime stats + API summary |
| Universal | `ESC`, `F11`, `F9`, `P`, `F12` via `sit_example.h` |

## APIs demonstrated

| API | Role |
|-----|------|
| `SituationCmdDrawText` | Basic string draw |
| `SituationCmdDrawTextEx` | Font size + spacing |
| Default font fallback | Zero-initialized `SituationFont` → built-in 8×8 VGA |

## vs raylib

Covers the same learning arc as raylib's text examples — metrics, color, positioning — without external font files.
