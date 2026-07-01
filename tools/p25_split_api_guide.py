#!/usr/bin/env python3
"""P2.5: split monolithic API guide into doc/guide/*.md + thin umbrella (Option A)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
DOC = ROOT / "doc"
GUIDE = DOC / "guide"
API_MD = DOC / "situation_api.md"
DEFAULT_SOURCE = DOC / ".situation_api_monolith_source.md"

if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from situation_api_parser import parse_api_header, read_version  # noqa: E402

OPEN_DETAILS = re.compile(r"<details(?:\s+open)?>", re.IGNORECASE)
CLOSE_DETAILS = re.compile(r"</details>", re.IGNORECASE)
SUMMARY_RE = re.compile(
    r"<details(?:\s+open)?>\s*<summary><h([23])>(.*?)</h\1></summary>",
    re.DOTALL | re.IGNORECASE,
)

MODULE_TO_FILE: list[tuple[str, str]] = [
    ("Core Module", "core.md"),
    ("Window and Display Module", "window_display.md"),
    ("Image Module", "image.md"),
    ("Graphics Module", "graphics.md"),
    ("Input Module", "input.md"),
    ("Audio Module", "audio.md"),
    ("Audio Node Graph Module", "audio_graph.md"),
    ("Filesystem Module", "filesystem.md"),
    ("Threading Module", "threading.md"),
    ("System Introspection Module", "system_introspection.md"),
    ("YPQ Color Module", "ypq_color.md"),
    ("MIDI Integration Module", "midi.md"),
    ("Renderer Bolster Commands", "renderer_bolster.md"),
    ("Deprecated APIs", "deprecated.md"),
    ("Miscellaneous Module", "miscellaneous.md"),
    ("Hot-Reloading Module", "hot_reload.md"),
    ("Logging Module", "logging.md"),
    ("Compute Shaders", "compute.md"),
    ("Text Rendering", "text_rendering.md"),
    ("Fonts", "font.md"),
    ("2D Rendering & Drawing", "drawing_2d.md"),
    ("Examples & Tutorials", "examples_faq.md"),
    ("Frequently Asked Questions", "examples_faq.md"),
]

FRONT_MATTER_SECTIONS = [
    "Introduction and Core Concepts",
    "Building the Library",
    "Getting Started",
]

# Relative links from doc/guide/*.md to sibling doc/*.md
GUIDE_LINK_FIXES: list[tuple[str, str]] = [
    ("](situation_command_reference.md", "](../situation_command_reference.md"),
    ("](situation_api_index.md", "](../situation_api_index.md"),
    ("](situation_sdk.md", "](../situation_sdk.md"),
    ("](whatsnew.md", "](../whatsnew.md"),
    ("](UPDATELOG.md", "](../UPDATELOG.md"),
    ("](COMPILATION_GUIDE.md", "](../COMPILATION_GUIDE.md"),
    ("](C11_Compliance_Report.md", "](../C11_Compliance_Report.md"),
]


def normalize_title(title: str) -> str:
    return re.sub(r"\s+", " ", title.strip())


def file_for_module(title: str) -> str | None:
    t = normalize_title(title)
    for prefix, fname in MODULE_TO_FILE:
        if t.startswith(prefix):
            return fname
    return None


def extract_details(text: str) -> list[tuple[str, str, str]]:
    """Return (level, title, body) for each top-level <details> block."""
    out: list[tuple[str, str, str]] = []
    pos = 0
    while True:
        m = SUMMARY_RE.search(text, pos)
        if not m:
            break
        level, title = m.group(1), normalize_title(m.group(2))
        body_start = m.end()
        depth = 1
        i = body_start
        closed = False
        while i < len(text) and depth > 0:
            open_m = OPEN_DETAILS.search(text, i)
            close_m = CLOSE_DETAILS.search(text, i)
            if close_m is None:
                break
            if open_m and open_m.start() < close_m.start():
                depth += 1
                i = open_m.end()
            else:
                depth -= 1
                if depth == 0:
                    body = text[body_start : close_m.start()].strip()
                    out.append((level, title, body))
                    pos = close_m.end()
                    closed = True
                    break
                i = close_m.end()
        if not closed:
            break
    return out


def front_matter_preamble(text: str) -> str:
    first = text.find("<details")
    if first == -1:
        return text.strip()
    chunk = text[:first].strip()
    toc = chunk.find("## Table of Contents")
    if toc != -1:
        chunk = chunk[:toc].strip()
    return chunk


def module_heading(level: str, title: str) -> str:
    return f"{'#' if level == '2' else '##'} {title}\n\n"


def build_front_matter(preamble: str, sections: dict[str, str]) -> str:
    parts = [preamble, "", "---", ""]
    for key in FRONT_MATTER_SECTIONS:
        body = sections.get(key)
        if not body:
            continue
        parts.extend([f"## {key}\n", body, "", "---", ""])
    return "\n".join(parts).strip() + "\n"


def fix_guide_links(text: str) -> str:
    for old, new in GUIDE_LINK_FIXES:
        text = text.replace(old, new)
    return text


def symbol_count(text: str) -> int:
    return len(re.findall(r"#### `Situation\w+`", text))


def merge_module_content(existing: str, new: str) -> str:
    """Keep whichever body documents more symbols; prefer longer on tie."""
    ec, nc = symbol_count(existing), symbol_count(new)
    if nc > ec:
        return new
    if ec > nc:
        return existing
    return new if len(new) > len(existing) else existing


def build_umbrella(version_short: str, count: int) -> str:
    return f"""# Situation — Advanced Platform Awareness, Control, and Timing

_Core API library v{version_short} · (c) 2025-2026 Jacques Morel · MIT Licensed_

**Situation** is a **strict C11** single-file library providing unified access to windowing, graphics (OpenGL 4.6 / Vulkan 1.4), audio (23-effect node graph, 16-voice MIDI synth), input, filesystem, NUMA-aware threading, and high-resolution timing. One header, one DLL, one `SituationInit()` call.

Ships as header-only or pre-built DLL with auto-generated FFI bindings for **Odin**, **Zig**, **Rust**, **Fortran**, and **Modula-2** (`wrappers/`).

> **{count}** public `SITAPI` functions · Windows 10+ · OpenGL 4.6 or Vulkan 1.4 hardware required

**Documentation map**

| Resource | Description |
|----------|-------------|
| **[guide/_front_matter.md](guide/_front_matter.md)** | Introduction, build integration, quick start |
| **[situation_api_index.md](situation_api_index.md)** | Categorized function index (auto-generated) |
| **[situation_command_reference.md](situation_command_reference.md)** | All `SituationCmd*` rendering commands |
| **[situation_sdk.md](situation_sdk.md)** | SDK manual (architecture, workflows, examples) |
| **[whatsnew.md](whatsnew.md)** / **[UPDATELOG.md](UPDATELOG.md)** | Release history |

For release history and changelogs, see **[whatsnew.md](whatsnew.md)** and **[UPDATELOG.md](UPDATELOG.md)**.

---

# Situation v{version_short} API Programming Guide

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
- [Renderer bolster commands](guide/renderer_bolster.md)
- [Compute shaders](guide/compute.md)
- [Fonts](guide/font.md)
- [Text rendering](guide/text_rendering.md)
- [2D rendering and drawing](guide/drawing_2d.md)

### Media and I/O
- [Audio](guide/audio.md)
- [Audio node graph](guide/audio_graph.md)
- [MIDI integration](guide/midi.md)
- [Filesystem](guide/filesystem.md)

### Utilities
- [Threading](guide/threading.md)
- [YPQ color](guide/ypq_color.md)
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
tools\\run_all.bat
python tools\\merge_api_doc_gaps.py
```

---

## License (MIT)

"Situation" is licensed under the permissive MIT License. In simple terms, this means you are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for both commercial and private projects. The only requirement is that you include the original copyright and license notice in any substantial portion of the software or derivative work you distribute. This library is provided "as is", without any warranty.

---

Copyright (c) 2025-2026 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--source",
        type=Path,
        default=None,
        help="Monolithic situation_api.md to split (default: thin umbrella uses DEFAULT_SOURCE)",
    )
    ap.add_argument("--force", action="store_true", help="Split even if umbrella is already thin")
    args = ap.parse_args()

    source = args.source
    if source is None:
        if API_MD.exists() and len(API_MD.read_text(encoding="utf-8").splitlines()) >= 500:
            source = API_MD
        elif DEFAULT_SOURCE.exists():
            source = DEFAULT_SOURCE
        else:
            print("ERROR: no monolith source (pass --source or restore .situation_api_monolith_source.md)")
            return 1

    if not source.exists():
        print(f"ERROR: missing source {source}")
        return 1

    text = source.read_text(encoding="utf-8")
    if not args.force and API_MD.exists() and len(API_MD.read_text(encoding="utf-8").splitlines()) < 500:
        if source == API_MD:
            print("WARN: umbrella already thin; use --source for monolith or --force")
            return 0

    version = read_version()
    version_short = version.split(" ", 1)[0]
    entries = parse_api_header()
    blocks = extract_details(text)
    if not blocks:
        print("ERROR: no <details> sections found")
        return 1

    GUIDE.mkdir(parents=True, exist_ok=True)

    front_sections: dict[str, str] = {}
    module_files: dict[str, list[str]] = {}
    unmapped: list[str] = []

    for level, title, body in blocks:
        if title in FRONT_MATTER_SECTIONS:
            front_sections[title] = body
            continue
        fname = file_for_module(title)
        if not fname:
            unmapped.append(title)
            continue
        module_files.setdefault(fname, []).append(module_heading(level, title) + body)

    front_path = GUIDE / "_front_matter.md"
    front_text = fix_guide_links(build_front_matter(front_matter_preamble(text), front_sections))
    if front_path.exists():
        front_text = merge_module_content(front_path.read_text(encoding="utf-8"), front_text)
    front_path.write_text(front_text, encoding="utf-8")

    for fname, chunks in sorted(module_files.items()):
        new_content = fix_guide_links("\n\n---\n\n".join(chunks).strip() + "\n")
        path = GUIDE / fname
        if path.exists():
            new_content = merge_module_content(path.read_text(encoding="utf-8"), new_content)
        path.write_text(new_content, encoding="utf-8")

    umbrella = build_umbrella(version_short, len(entries))
    API_MD.write_text(umbrella, encoding="utf-8")

    print(f"Situation {version_short}")
    print(f"Source: {source.relative_to(ROOT)} ({len(text.splitlines())} lines, {len(blocks)} sections)")
    print(f"Wrote {front_path.relative_to(ROOT)}")
    print(f"Wrote {len(module_files)} module files under {GUIDE.relative_to(ROOT)}/")
    print(f"Umbrella {API_MD.relative_to(ROOT)}: {len(umbrella.splitlines())} lines")
    if unmapped:
        print(f"WARN: {len(unmapped)} unmapped sections:")
        for t in unmapped:
            print(f"  - {t}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
