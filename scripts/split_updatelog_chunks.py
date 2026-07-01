#!/usr/bin/env python3
"""Split Situation UPDATELOG archives into chunk files + index UPDATELOG.md."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "doc"
V24_PARTS = 5  # 01: ≤100, 02: 101–200, 03: 201–300, 04: 301–400, 05: 401+

ENTRY_RE = re.compile(r"^## \[", re.MULTILINE)
VERSION_RE = re.compile(r"^## \[(?:v)?([^\]]+)\]", re.MULTILINE)
FOOTER_RE = re.compile(
    r"\n---\s*\n\s*> \*\*Current releases:\*\*.*\Z",
    re.DOTALL,
)

# v2.4: updatelog_24_01.md = oldest … updatelog_24_04.md = newest (read 01 → 04).
# v2.3: single updatelog_23.md (113 patches — never chunked).


def split_entries(text: str) -> list[str]:
    text = text.strip()
    if not text:
        return []
    matches = list(ENTRY_RE.finditer(text))
    if not matches:
        raise ValueError("No ## [ entries found")
    entries: list[str] = []
    for i, m in enumerate(matches):
        start = m.start()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        block = text[start:end].rstrip()
        if block:
            entries.append(block)
    return entries


def entry_version(entry: str) -> str:
    m = VERSION_RE.search(entry)
    return m.group(1).strip() if m else "unknown"


def patch_number(entry: str) -> float:
    ver = entry_version(entry)
    m = re.match(r"2\.(\d+)\.(\d+)", ver)
    if m:
        return float(f"{m.group(1)}.{m.group(2)}")
    m = re.match(r"2\.(\d+)\.(\d+)([A-Z])", ver)
    if m:
        return float(f"{m.group(1)}.{m.group(2)}") + 0.01
    return 0.0


def v24_patch_minor(entry: str) -> int:
    ver = entry_version(entry)
    m = re.match(r"2\.4\.(\d+)", ver)
    return int(m.group(1)) if m else 0


def v24_part_for_entry(entry: str) -> int:
    p = v24_patch_minor(entry)
    if p <= 100:
        return 1
    if p <= 200:
        return 2
    if p <= 300:
        return 3
    if p <= 400:
        return 4
    return 5


def to_oldest_first(entries: list[str]) -> list[str]:
    if len(entries) < 2:
        return entries
    if patch_number(entries[0]) > patch_number(entries[-1]):
        return list(reversed(entries))
    return entries


def v24_chunk_filename(part: int) -> str:
    return f"updatelog_24_{part:02d}.md"


def archive_header(
    title: str,
    first_ver: str,
    last_ver: str,
    count: int,
    *,
    prev_file: str | None = None,
    next_file: str | None = None,
) -> str:
    nav_prev = f"[`{prev_file}`]({prev_file})" if prev_file else "—"
    nav_next = f"[`{next_file}`]({next_file})" if next_file else "—"
    nav = ""
    if prev_file or next_file:
        nav = (
            f"Index: [`UPDATELOG.md`](UPDATELOG.md) · "
            f"Previous: {nav_prev} · Next: {nav_next}\n\n"
        )
    return (
        f"# {title}\n\n"
        f"Patches **{first_ver}** through **{last_ver}** "
        f"({count} entries, oldest first).\n\n"
        f"{nav}"
        f"---\n\n"
    )


def write_v24_chunks(
    entries: list[str],
    *,
    dry_run: bool = False,
) -> list[dict]:
    entries = to_oldest_first(entries)
    if not entries:
        return []

    buckets: list[list[str]] = [[] for _ in range(V24_PARTS)]
    for entry in entries:
        buckets[v24_part_for_entry(entry) - 1].append(entry)
    chunks = [b for b in buckets if b]

    meta: list[dict] = []
    total = V24_PARTS
    for idx, chunk in enumerate(chunks):
        part = idx + 1  # 01 = patches ≤100 … 05 = patches ≥401
        fname = v24_chunk_filename(part)
        prev_f = v24_chunk_filename(part - 1) if part > 1 else None
        next_f = v24_chunk_filename(part + 1) if part < total else None
        title = f"Situation UPDATELOG — v2.4.x (part {part} of {total})"
        body = archive_header(
            title,
            entry_version(chunk[0]),
            entry_version(chunk[-1]),
            len(chunk),
            prev_file=prev_f,
            next_file=next_f,
        )
        body += "\n\n---\n\n".join(chunk)
        if not body.endswith("\n"):
            body += "\n"

        row = {
            "part": part,
            "file": fname,
            "first": entry_version(chunk[0]),
            "last": entry_version(chunk[-1]),
            "count": len(chunk),
        }
        meta.append(row)
        if not dry_run:
            (DOC / fname).write_text(body, encoding="utf-8")
            print(f"Wrote {fname}: {row['count']} entries ({row['first']} .. {row['last']})")

    # Remove stale extra v2.4 parts if count shrank
    if not dry_run:
        for stale in DOC.glob("updatelog_24_*.md"):
            if stale.name not in {m["file"] for m in meta}:
                stale.unlink()
                print(f"Removed stale {stale.name}")

    return meta


def write_v23_single(
    entries: list[str],
    *,
    dry_run: bool = False,
) -> dict | None:
    entries = to_oldest_first(entries)
    if not entries:
        return None

    fname = "updatelog_23.md"
    title = "Situation UPDATELOG — v2.3.x archive"
    body = archive_header(
        title,
        entry_version(entries[0]),
        entry_version(entries[-1]),
        len(entries),
    )
    body += "\n\n---\n\n".join(entries)
    if not body.endswith("\n"):
        body += "\n"

    row = {
        "file": fname,
        "first": entry_version(entries[0]),
        "last": entry_version(entries[-1]),
        "count": len(entries),
    }
    if not dry_run:
        (DOC / fname).write_text(body, encoding="utf-8")
        print(f"Wrote {fname}: {row['count']} entries ({row['first']} .. {row['last']})")
        for stale in DOC.glob("updatelog_23_*.md"):
            stale.unlink()
            print(f"Removed split v2.3 chunk {stale.name}")

    return row


def build_index(v24_meta: list[dict], v23_row: dict | None, current_version: str) -> str:
    newest_v24 = v24_meta[-1]["file"] if v24_meta else "updatelog_24_04.md"
    lines = [
        "# Situation UPDATELOG",
        "",
        "Release history in archive files.",
        "",
        "- **v2.4.x:** split across `updatelog_24_*.md` (table lists **newest file first**).",
        "- **v2.3.x:** single file [`updatelog_23.md`](updatelog_23.md) (not split).",
        "",
        "Within each archive file, entries are **oldest first** (top → bottom). "
        "For full v2.4 history, read files in order: `_01` → `_02` → `_03` → `_04` → `_05`.",
        "",
        f"**Current version:** {current_version} — narrative summary in "
        "[`whatsnew.md`](whatsnew.md).",
        "",
        "---",
        "",
        "## v2.4.x",
        "",
        "| File | Range (oldest → newest) | Patches |",
        "|------|-------------------------|--------:|",
    ]
    for row in reversed(v24_meta):
        lines.append(
            f"| [{row['file']}]({row['file']}) | "
            f"{row['first']} – {row['last']} | {row['count']} |"
        )
    lines.extend(
        [
            "",
            "## v2.3.x archive",
            "",
            "| File | Range (oldest → newest) | Patches |",
            "|------|-------------------------|--------:|",
        ]
    )
    if v23_row:
        lines.append(
            f"| [{v23_row['file']}]({v23_row['file']}) | "
            f"{v23_row['first']} – {v23_row['last']} | {v23_row['count']} |"
        )
    lines.extend(
        [
            "",
            "---",
            "",
            "## Adding a new patch",
            "",
            f"1. Append the release block to the bottom of [`{newest_v24}`]({newest_v24}).",
            "2. When patch numbers exceed **2.4.500**, add `updatelog_24_06.md` "
            "(extend `scripts/split_updatelog_chunks.py`).",
            "3. Update this index (or let the script regenerate it).",
            "",
            "## Maintenance",
            "",
            "```bat",
            "python scripts\\split_updatelog_chunks.py",
            "```",
            "",
            "Regenerates v2.4 archive files (`updatelog_24_01.md` … `_05.md` by patch band) "
            "and the v2.3 single archive.",
            "",
        ]
    )
    return "\n".join(lines) + "\n"


def load_entries_from_chunks(glob_pattern: str) -> list[str]:
    parts = sorted(DOC.glob(glob_pattern))
    if not parts:
        return []
    entries: list[str] = []
    for p in parts:
        t = p.read_text(encoding="utf-8")
        if "---" in t:
            t = t.split("---", 1)[-1]
        entries.extend(split_entries(t.strip()))
    return entries


def load_v24_entries() -> list[str]:
    main = DOC / "UPDATELOG.md"
    text = main.read_text(encoding="utf-8")
    if text.startswith("# Situation UPDATELOG\n") and (
        "| Part |" in text or "| File | Range" in text
    ):
        entries = load_entries_from_chunks("updatelog_24_*.md")
        if entries:
            return entries
        raise SystemExit("UPDATELOG.md is an index but no updatelog_24_*.md chunks found")
    text = FOOTER_RE.sub("", text).strip()
    return split_entries(text)


def load_v23_entries() -> list[str]:
    single = DOC / "updatelog_23.md"
    if single.exists():
        t = single.read_text(encoding="utf-8")
        if "---" in t:
            t = t.split("---", 1)[-1]
        return split_entries(t.strip())

    legacy = DOC / "UPDFATELOG_23.md"
    if legacy.exists():
        text = legacy.read_text(encoding="utf-8")
        m = ENTRY_RE.search(text)
        if not m:
            raise SystemExit("No entries in UPDFATELOG_23.md")
        text = text[m.start() :]
        text = FOOTER_RE.sub("", text).strip()
        return split_entries(text)

    entries = load_entries_from_chunks("updatelog_23_*.md")
    if entries:
        return entries
    raise SystemExit("No v2.3 source found")


def main() -> None:
    dry_run = "--dry-run" in sys.argv
    v24 = load_v24_entries()
    v23 = load_v23_entries()
    print(f"v2.4 entries: {len(v24)}")
    print(f"v2.3 entries: {len(v23)}")

    v24_sorted = to_oldest_first(v24)
    current_raw = entry_version(v24_sorted[-1]) if v24_sorted else "unknown"
    current = f"v{current_raw}" if not current_raw.startswith("v") else current_raw

    v24_meta = write_v24_chunks(v24, dry_run=dry_run)
    v23_row = write_v23_single(v23, dry_run=dry_run)

    if not dry_run:
        (DOC / "UPDATELOG.md").write_text(
            build_index(v24_meta, v23_row, current), encoding="utf-8"
        )
        print("Wrote UPDATELOG.md (index)")
        legacy = DOC / "UPDFATELOG_23.md"
        if legacy.exists():
            legacy.unlink()
            print("Removed UPDFATELOG_23.md")

    print("Done.")


if __name__ == "__main__":
    main()