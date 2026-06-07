#!/usr/bin/env python3
"""Phase 10 gate: find likely ignored SituationError returns in init tree."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "sit"
INIT_FUNCS = (
    "_SituationInitPlatform",
    "_SituationInitWindow",
    "_SituationInitSubsystems",
    "_SituationInitRenderer",
    "_SituationInitOpenGL",
    "_SituationInitVulkan",
    "_SituationInitStagingBuffers",
)

CALL = re.compile(
    r"(?P<prefix>.*?)\b(?P<fn>_SituationInit\w+|_SituationVulkanCreate\w+)\s*\("
)

def scan_file(path: Path) -> list[str]:
    hits: list[str] = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue
        m = CALL.search(line)
        if not m:
            continue
        fn = m.group("fn")
        if fn not in INIT_FUNCS and not fn.startswith("_SituationVulkanCreate"):
            continue
        prefix = m.group("prefix").strip()
        if prefix.endswith("=") or "if (" in prefix or "return " in prefix:
            continue
        if "(void)" in prefix or prefix.endswith("(void)"):
            hits.append(f"{path.name}:{i}: {stripped[:120]}")
    return hits

def main() -> None:
    files = sorted(ROOT.glob("situation_impl*.h"))
    all_hits: list[str] = []
    for f in files:
        all_hits.extend(scan_file(f))
    print(f"unchecked init/vk-create calls: {len(all_hits)}")
    for h in all_hits[:40]:
        print(h)
    if len(all_hits) > 40:
        print(f"... and {len(all_hits) - 40} more")

if __name__ == "__main__":
    main()
