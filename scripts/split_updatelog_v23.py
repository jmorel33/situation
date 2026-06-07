#!/usr/bin/env python3
"""Split v2.3.x entries from doc/UPDATELOG.md into doc/updatelog_23.md."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "doc"
MAIN = DOC / "UPDATELOG.md"
ARCHIVE = DOC / "updatelog_23.md"

V23_HEADER = """# Situation UPDATELOG — v2.3.x archive

Older release notes split from [`UPDATELOG.md`](UPDATELOG.md) (v2.4.x and later). Newest entries first within this file.

---

"""

ARCHIVE_FOOTER = """
---

> **Current releases:** [`UPDATELOG.md`](UPDATELOG.md) (v2.4.x+).
"""

SPLIT_RE = re.compile(r"^## \[(?:v)?2\.3\.", re.MULTILINE)


def main() -> None:
    text = MAIN.read_text(encoding="utf-8")
    m = SPLIT_RE.search(text)
    if not m:
        raise SystemExit("No v2.3.x section found in UPDATELOG.md")

    v4_part = text[: m.start()].rstrip()
    v3_part = text[m.start() :].lstrip()

    if not v3_part.startswith("## ["):
        raise SystemExit("Split produced invalid v2.3 section")

    if not v4_part.startswith("## [v2.4."):
        first = v4_part.splitlines()[0] if v4_part else "(empty)"
        raise SystemExit(f"Expected v2.4.x at top of main log, got: {first!r}")

    if ARCHIVE_FOOTER.strip() not in v4_part:
        v4_part += ARCHIVE_FOOTER

    archive_body = V23_HEADER + v3_part
    if not archive_body.endswith("\n"):
        archive_body += "\n"

    ARCHIVE.write_text(archive_body, encoding="utf-8")
    MAIN.write_text(v4_part + "\n", encoding="utf-8")

    v4_lines = len(v4_part.splitlines())
    v3_lines = len(v3_part.splitlines())
    v3_count = len(re.findall(r"^## \[(?:v)?2\.3\.", v3_part, re.MULTILINE))
    print(f"UPDATELOG.md: {v4_lines} lines (v2.4.x)")
    print(f"updatelog_23.md: {v3_lines} lines ({v3_count} v2.3.x entries)")


if __name__ == "__main__":
    main()
