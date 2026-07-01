#!/usr/bin/env python3
"""Verify internal markdown links in doc/*.md (relative .md#anchors only)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC_DIR = ROOT / "doc"

# Phase 4 + P2.5: docs touched by API guide migration.
MIGRATION_DOCS = frozenset(
    {
        "situation_command_reference.md",
        "situation_sdk.md",
        "situation_api_index.md",
        "situation_api.md",
    }
)
GUIDE_GLOB = "guide/*.md"
COMMAND_REF = "situation_command_reference.md"


def iter_doc_files(doc_dir: Path) -> dict[Path, str]:
    files: dict[Path, str] = {}
    for p in doc_dir.glob("*.md"):
        files[p.resolve()] = p.read_text(encoding="utf-8", errors="replace")
    guide = doc_dir / "guide"
    if guide.is_dir():
        for p in guide.glob("*.md"):
            files[p.resolve()] = p.read_text(encoding="utf-8", errors="replace")
    return files


def is_checked_source(path: Path) -> bool:
    if path.name in MIGRATION_DOCS:
        return True
    return path.parent.name == "guide"

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
    if not is_checked_source(source):
        return False
    if href.startswith(("http://", "https://", "mailto:")):
        return False
    if "sit/terminal/" in href.replace("\\", "/"):
        return False
    # Skip links outside doc/ (e.g. sit/terminal/*.md, repo-root plans).
    try:
        target.relative_to(DOC_DIR)
    except ValueError:
        return False
    # Pre-existing SDK gaps — not part of P2.5 guide migration.
    if source.name == "situation_sdk.md" and target.name in {
        "SITUATION_QUICK_REFERENCE.md",
        "YPQ_COLOR_PLAN.md",
    }:
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
    md_files = iter_doc_files(DOC_DIR)
    anchor_cache: dict[Path, set[str]] = {}

    errors: list[str] = []
    checked = 0
    sources = [p for p in md_files if is_checked_source(p)]

    for path, text in md_files.items():
        if not is_checked_source(path):
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
                    anchor_cache[target] = collect_anchors(md_files.get(target, ""))
                if anchor not in anchor_cache[target]:
                    errors.append(
                        f"{path.relative_to(ROOT)}: missing anchor #{anchor} in {target.relative_to(ROOT)} (from {href})"
                    )

    print(f"Checked {checked} internal links in {len(sources)} doc(s) (umbrella + guide/)")
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
