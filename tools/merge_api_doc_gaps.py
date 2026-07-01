#!/usr/bin/env python3
"""Insert missing SITAPI docs from the header into doc/guide/*.md (P2.5)."""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from generate_api_index import read_api_doc_corpus, render_entry  # noqa: E402
from situation_api_parser import ROOT, parse_api_header, read_version  # noqa: E402

API_MD = ROOT / "doc" / "situation_api.md"
GUIDE_DIR = ROOT / "doc" / "guide"

# Header `// --- section ---` prefix → guide file (first match wins).
SECTION_TO_FILE: list[tuple[str, str]] = [
    # Core / platform
    ("Application Lifecycle", "core.md"),
    ("Frame Timing", "core.md"),
    ("Callbacks and Event", "core.md"),
    ("Command-Line Argument", "core.md"),
    ("System & Hardware Information", "core.md"),
    ("System & backend", "core.md"),
    ("Profiling & Diagnostics", "core.md"),
    ("Initialization State", "core.md"),
    ("OS Information", "system_introspection.md"),
    ("Process Enumeration", "system_introspection.md"),
    ("Active Audio Device Query", "system_introspection.md"),
    ("Window State", "window_display.md"),
    ("Window Property", "window_display.md"),
    ("Window & Screen Dimension", "window_display.md"),
    ("Physical Display", "window_display.md"),
    ("Cursor, Clipboard", "window_display.md"),
    ("Advanced Window Profile", "window_display.md"),
    ("Image Loading", "image.md"),
    ("Image Exporting", "image.md"),
    ("Image Generation", "image.md"),
    ("Image Manipulation", "image.md"),
    ("Font Management", "font.md"),
    ("Keyboard Input", "input.md"),
    ("Mouse Input", "input.md"),
    ("Gamepad Input", "input.md"),
    # Graphics
    ("Frame Lifecycle & Command Buffer", "graphics.md"),
    ("Command Buffer Recording", "graphics.md"),
    ("Abstracted Rendering Commands", "graphics.md"),
    ("Graphics Resource Management", "graphics.md"),
    ("Shader Management", "graphics.md"),
    ("Shader Interaction", "graphics.md"),
    ("Texture Management", "graphics.md"),
    ("Compute Shader Pipeline", "compute.md"),
    ("GPU Buffer Management", "graphics.md"),
    ("Virtual Displays", "graphics.md"),
    ("Camera & Projection", "graphics.md"),
    ("3D Model Utilities", "graphics.md"),
    ("Image & Screenshot Utilities", "graphics.md"),
    ("Backend-Specific Accessors", "graphics.md"),
    ("Renderer Bolster", "renderer_bolster.md"),
    ("Text Rendering", "text_rendering.md"),
    ("2D Rendering", "drawing_2d.md"),
    # Audio
    ("Audio Device Management", "audio.md"),
    ("Audio Capture", "audio.md"),
    ("Audio Output Monitoring", "audio.md"),
    ("Sound Loading", "audio.md"),
    ("Audio Handle API", "audio.md"),
    ("Procedural Tones", "audio.md"),
    ("Resonance", "audio.md"),
    ("Sound Data Manipulation", "audio.md"),
    ("Sound Parameters", "audio.md"),
    ("Custom Audio Processing", "audio.md"),
    ("Device Registry", "audio_graph.md"),
    ("Active Graph", "audio_graph.md"),
    ("Node Graph Functions", "audio_graph.md"),
    ("PCM Input Node", "audio_graph.md"),
    ("Graph Serialization", "audio_graph.md"),
    ("Device Enumeration", "audio_graph.md"),
    ("Node Graph SFX Routing", "audio_graph.md"),
    ("MIDI Device Control", "midi.md"),
    ("Virtual MIDI", "midi.md"),
    ("MIDI Learn", "midi.md"),
    ("Learning Operations", "midi.md"),
    ("Mapping Management", "midi.md"),
    ("Preset Persistence", "midi.md"),
    ("Official names for harness virtual MIDI", "midi.md"),
    # System
    ("Path Management", "filesystem.md"),
    ("File & Directory Queries", "filesystem.md"),
    ("File Operations", "filesystem.md"),
    ("Directory Operations", "filesystem.md"),
    ("Hot-Reload", "hot_reload.md"),
    ("Hot Reload", "hot_reload.md"),
    ("Logging", "logging.md"),
    ("YPQ", "ypq_color.md"),
    ("HDR color", "ypq_color.md"),
    ("CPU & Thread Management", "threading.md"),
    ("Threading observability", "threading.md"),
    ("Thread pool", "threading.md"),
    ("Temporal Oscillator", "miscellaneous.md"),
    ("Timer System", "miscellaneous.md"),
    ("Miscellaneous", "miscellaneous.md"),
    # Deprecated
    ("Lifecycle", "deprecated.md"),
    ("System", "deprecated.md"),
    ("Audio", "deprecated.md"),
    ("Graphics / compute", "deprecated.md"),
]

FILE_INSERT_ORDER = [
    "core.md",
    "window_display.md",
    "image.md",
    "graphics.md",
    "input.md",
    "audio.md",
    "audio_graph.md",
    "filesystem.md",
    "threading.md",
    "system_introspection.md",
    "ypq_color.md",
    "midi.md",
    "renderer_bolster.md",
    "compute.md",
    "font.md",
    "text_rendering.md",
    "drawing_2d.md",
    "hot_reload.md",
    "logging.md",
    "miscellaneous.md",
    "deprecated.md",
]


def file_for_section(section: str) -> str | None:
    for prefix, fname in SECTION_TO_FILE:
        if section.startswith(prefix):
            return fname
    return None


def update_header_metadata(text: str, version_short: str, count: int) -> str:
    text = re.sub(
        r"^_.*Core API library v[\d.]+ ·",
        f"_Core API library v{version_short} ·",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    text = re.sub(
        r"> \*\*\d+\*\* public `SITAPI` functions",
        f"> **{count}** public `SITAPI` functions",
        text,
        count=1,
    )
    text = re.sub(
        r"^# Situation v[\d.]+\ API Programming Guide$",
        f"# Situation v{version_short} API Programming Guide",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    return text


def main() -> None:
    version = read_version()
    version_short = version.split(" ", 1)[0]
    entries = parse_api_header()
    corpus = read_api_doc_corpus()

    missing = [e for e in entries if e.name not in corpus]
    by_file: dict[str, list] = defaultdict(list)
    unmapped: list[str] = []

    for e in missing:
        fname = file_for_section(e.section)
        if fname:
            by_file[fname].append(e)
        else:
            unmapped.append(f"{e.name} ({e.section})")

    inserted = 0
    if GUIDE_DIR.is_dir():
        for fname in FILE_INSERT_ORDER:
            group = by_file.get(fname)
            if not group:
                continue
            path = GUIDE_DIR / fname
            text = path.read_text(encoding="utf-8") if path.exists() else f"# {fname[:-3].replace('_', ' ').title()}\n\n"
            block = "\n---\n\n" + "\n---\n\n".join(
                render_entry(e) for e in sorted(group, key=lambda x: x.name)
            )
            path.write_text(text.rstrip() + block + "\n", encoding="utf-8")
            inserted += len(group)

    if API_MD.exists():
        API_MD.write_text(
            update_header_metadata(API_MD.read_text(encoding="utf-8"), version_short, len(entries)),
            encoding="utf-8",
        )

    front = GUIDE_DIR / "_front_matter.md"
    if front.exists():
        front.write_text(
            update_header_metadata(front.read_text(encoding="utf-8"), version_short, len(entries)),
            encoding="utf-8",
        )

    corpus = read_api_doc_corpus()
    remaining = [e for e in entries if e.name not in corpus]
    print(f"Situation {version_short}")
    print(f"Inserted {inserted} API entries into doc/guide/")
    if unmapped:
        print(f"WARN: {len(unmapped)} unmapped (manual placement needed):")
        for line in unmapped[:20]:
            print(f"  - {line}")
        if len(unmapped) > 20:
            print(f"  ... and {len(unmapped) - 20} more")
    print(f"Coverage: {len(entries) - len(remaining)}/{len(entries)} ({len(remaining)} still missing)")


if __name__ == "__main__":
    main()
