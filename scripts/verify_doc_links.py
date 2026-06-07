#!/usr/bin/env python3
"""Verify internal markdown links in doc/*.md (relative .md#anchors only)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC_DIR = ROOT / "doc"

# Phase 4: docs touched by command-reference migration (not entire doc tree).
MIGRATION_DOCS = frozenset(
    {
        "situation_command_reference.md",
        "situation_sdk.md",
        "situation_api_index.md",
        "situation_api.md",
    }
)
COMMAND_REF = "situation_command_reference.md"

LINK_RE = re.compile(r"\]\(([^)]+)\)")
HTML_ID_RE = re.compile(r'<a\s+id="([^"]+)"')


def slugify(heading: str) -> str:
    s = heading.strip().lower()
    s = re.sub(r"[^\w\s-]", "", s)
    s = re.sub(r"\s+", "-", s)
    return s


def collect_anchors(text: str) -> set[str]:
    anchors: set[str] = set()
    for m in HTML_ID_RE.finditer(text):
        anchors.add(m.group(1))
    for line in text.splitlines():
        m = re.match(r"^(#{1,6})\s+(.+)$", line.strip())
        if m:
            anchors.add(slugify(m.group(2)))
    return anchors


def should_check_link(source: Path, href: str, target: Path) -> bool:
    if source.name not in MIGRATION_DOCS:
        return False
    if source.name == "situation_api.md":
        return COMMAND_REF in href
    if target.name not in MIGRATION_DOCS:
        return False
    return True


def resolve_target(href: str, source: Path) -> tuple[Path | None, str | None]:
    href = href.strip()
    if href.startswith(("http://", "https://", "mailto:")):
        return None, None
    if "#" in href:
        path_part, anchor = href.split("#", 1)
    else:
        path_part, anchor = href, None
    if not path_part:
        target = source
    else:
        target = (source.parent / path_part).resolve()
    return target, anchor


def main() -> int:
    md_files = {p.resolve(): p.read_text(encoding="utf-8", errors="replace") for p in DOC_DIR.glob("*.md")}
    anchor_cache: dict[Path, set[str]] = {}

    errors: list[str] = []
    checked = 0

    for path, text in md_files.items():
        if path.name not in MIGRATION_DOCS:
            continue
        for match in LINK_RE.finditer(text):
            href = match.group(1)
            target, anchor = resolve_target(href, path)
            if target is None:
                continue
            if not should_check_link(path, href, target):
                continue
            checked += 1
            if not target.exists():
                errors.append(f"{path.relative_to(ROOT)}: broken file link -> {href}")
                continue
            if anchor:
                if target not in anchor_cache:
                    anchor_cache[target] = collect_anchors(md_files.get(target, target.read_text(encoding="utf-8", errors="replace")))
                if anchor not in anchor_cache[target]:
                    errors.append(
                        f"{path.relative_to(ROOT)}: missing anchor #{anchor} in {target.relative_to(ROOT)} (from {href})"
                    )

    print(f"Checked {checked} internal links in {len(MIGRATION_DOCS)} migration doc(s)")
    if errors:
        print(f"FAIL: {len(errors)} broken link(s)")
        for e in errors[:50]:
            print(f"  - {e}")
        if len(errors) > 50:
            print(f"  ... and {len(errors) - 50} more")
        return 1
    print("OK: all internal links resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
