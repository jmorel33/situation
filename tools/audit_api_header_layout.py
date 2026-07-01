#!/usr/bin/env python3
"""Audit sit/situation_api.h layout for P2.2 before/after verification.

Usage:
  python tools/audit_api_header_layout.py

Prints line counts, domain SITAPI breakdown, and base-header sizes.
Re-run after P2.2 split to compare against plan success criteria.
"""

from __future__ import annotations

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API_H = ROOT / "sit" / "situation_api.h"

import sys
sys.path.insert(0, str(ROOT / "tools"))
from situation_api_parser import read_expanded_api_lines


def main() -> None:
    umbrella_lines = API_H.read_text(encoding="utf-8").splitlines()
    lines = read_expanded_api_lines(API_H)
    n_umbrella = len(umbrella_lines)
    n = len(lines)

    sitapi_umbrella = sum(1 for l in umbrella_lines if l.strip().startswith("SITAPI"))
    sitapi = sum(1 for l in lines if l.strip().startswith("SITAPI"))
    typedef = sum(1 for l in lines if re.match(r"^\s*typedef\s", l))
    define = sum(1 for l in lines if l.strip().startswith("#define"))

    fn_start = next(
        (i for i, l in enumerate(lines) if "Application Lifecycle & State" in l),
        next((i for i, l in enumerate(lines) if "Graphics Module:" in l), 0),
    )
    p2_types = next((i for i, l in enumerate(lines) if "Additional public types (P2.1" in l), None)
    module_map = next((i for i, l in enumerate(lines) if "API QUICK RULES" in l), None)
    deprecated_start = next((i for i, l in enumerate(lines) if "Deprecated API" in l), None)

    sections: list[tuple[str, int, int]] = []
    cur_name = "pre-functions"
    cur_start = fn_start
    for i in range(fn_start, n):
        m = re.match(r"^// --- (.+?) ---", lines[i])
        if m:
            sections.append((cur_name, cur_start, i - 1))
            cur_name = m.group(1)
            cur_start = i
    sections.append((cur_name, cur_start, n - 1))

    def sitapi_in_range(a: int, b: int) -> int:
        return sum(1 for j in range(a, b + 1) if lines[j].strip().startswith("SITAPI"))

    domain_map = {
        "platform": [
            "Application Lifecycle",
            "Frame Timing",
            "Callbacks",
            "Command-Line",
            "System & Hardware",
            "OS Information",
            "Process Enumeration",
            "Active Audio Device",
            "Profiling & Diagnostics",
            "Window State",
            "Window Property",
            "Window State Queries",
            "Window & Screen",
            "Physical Display",
            "Cursor, Clipboard",
            "Advanced Window Profile",
            "Image Loading",
            "Image Exporting",
            "Image Generation",
            "Image Manipulation",
            "Font Management",
            "Keyboard Input",
            "Mouse Input",
            "Gamepad Input",
        ],
        "graphics": [
            "Frame Lifecycle & Command",
            "Command Buffer Recording",
            "Abstracted Rendering",
            "Graphics Resource",
            "Shader Management",
            "Shader Interaction",
            "Texture Management",
            "Compute Shader",
            "GPU Buffer",
            "Virtual Displays",
            "Camera & Projection",
            "3D Model",
            "Image & Screenshot",
            "Backend-Specific",
        ],
        "audio": [
            "Audio Device Management",
            "Audio Capture",
            "Audio Output Monitoring",
            "Sound Loading",
            "Audio Handle",
            "Sound Data",
            "Sound Parameters",
            "Custom Audio Processing",
            "Device Registry",
            "Active Graph",
            "Node Graph",
            "PCM Input",
            "MIDI Device",
            "Virtual MIDI",
            "MIDI Learn",
            "Learning Operations",
            "Mapping Management",
            "Preset Persistence",
            "Graph Serialization",
            "Device Enumeration",
            "Node Graph SFX",
        ],
        "system": [
            "Path Management",
            "File & Directory",
            "File Operations",
            "Directory Operations",
            "Temporal Oscillator",
            "Color Space",
            "YPQ / HDR",
            "CPU & Thread",
        ],
    }

    def classify(sec: str) -> str:
        for dom, prefixes in domain_map.items():
            for p in prefixes:
                if sec.startswith(p) or p in sec:
                    return dom
        if "Deprecated" in sec or sec.startswith("Lifecycle") or sec.startswith("Graphics / compute"):
            return "deprecated"
        return "other"

    dom_apis: dict[str, int] = defaultdict(int)
    dom_lines: dict[str, int] = defaultdict(int)
    for name, a, b in sections:
        dom = classify(name)
        dom_apis[dom] += sitapi_in_range(a, b)
        dom_lines[dom] += b - a + 1

    print(f"Situation API header audit — {API_H.relative_to(ROOT)}")
    print(f"  Umbrella lines:     {n_umbrella}")
    print(f"  Expanded lines:     {n} (umbrella + P2.2 submodules)")
    print(f"  SITAPI (expanded):  {sitapi}")
    print(f"  SITAPI (umbrella):  {sitapi_umbrella}")
    print(f"  typedef lines:      {typedef}")
    print(f"  #define lines:      {define}")
    print(f"  Pre-function region:{fn_start} lines ({round(100 * fn_start / n)}%)")
    print(f"  Function region:    {n - fn_start} lines")
    if p2_types is not None and module_map is not None:
        print(f"  P2.1 types block:   lines {p2_types + 1}-{module_map + 1}")
    if deprecated_start is not None:
        print(f"  Deprecated block:   starts line {deprecated_start + 1}")
    print(f"  SIT_DEPRECATED API: {sum(1 for l in lines if 'SIT_DEPRECATED' in l and 'SITAPI' in l)}")
    print()
    print("situation_base_*.h:")
    for p in sorted((ROOT / "sit").glob("situation_base*.h")):
        print(f"  {p.name:28s} {len(p.read_text(encoding='utf-8').splitlines()):5d} lines")
    print()
    print("SITAPI by P2.2 target domain (function region):")
    for dom in ("platform", "graphics", "audio", "system", "deprecated", "other"):
        if dom_apis[dom]:
            print(f"  {dom:12s} {dom_apis[dom]:3d} APIs  ~{dom_lines[dom]:4d} lines")
    print()
    print("Top sections by SITAPI count:")
    for c, name in sorted(((sitapi_in_range(a, b), name) for name, a, b in sections), reverse=True)[:12]:
        if c:
            print(f"  {c:3d}  {name[:70]}")


if __name__ == "__main__":
    main()
