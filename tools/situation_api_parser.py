"""Shared Situation public API parser for doc index and Odin FFI generators."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def strip_c_api_attributes(sig: str) -> str:
    """Remove SIT_DEPRECATED / SITAPI macros that break naive signature regexes."""
    sig = re.sub(r'SIT_DEPRECATED\s*\(\s*"[^"]*"\s*\)', "", sig)
    sig = re.sub(r"SIT_DEPRECATED\s*\(\s*'[^']*'\s*\)", "", sig)
    sig = re.sub(r"\bSITAPI\s+", "", sig)
    return re.sub(r"\s+", " ", sig).strip()
API_H = ROOT / "sit" / "situation_api.h"
SIT_DIR = ROOT / "sit"
ERRNO_H = ROOT / "sit" / "situation_base_errno.h"
ETC_H = ROOT / "sit" / "situation_base_etc.h"
VERSION_H = ROOT / "sit" / "situation_base_version.h"

INCLUDE_QUOTED_RE = re.compile(r'^\s*#include\s+"([^"]+)"')

SITAPI_RE = re.compile(
    r"^SITAPI\s+(?:const\s+)?(.+?\s+(\w+)\s*\([^;]*\);)\s*(?://\s*(.*))?$"
)
SECTION_RE = re.compile(r"^// --- (.+?) ---")
ENUM_MEMBER_RE = re.compile(r"^(\w+)\s*(?:=\s*([^,/]+))?\s*,?\s*(?://.*)?$")
DEFINE_RE = re.compile(r"^#define\s+(SIT_(?:KEY_|COLOR_|WINDOW_|LOG_|FEATURE_|RENDER_|SUBMIT_|AUDIO_|LOAD_)\w+)\s+(.+)$")
ERRNO_X_RE = re.compile(r"X\((\w+),\s*(-?\d+),")


@dataclass
class ApiEntry:
    name: str
    signature: str
    section: str
    comment: str = ""
    is_variadic: bool = False
    has_function_pointer: bool = False
    manual_only: bool = False
    manual_reason: str = ""


@dataclass
class EnumEntry:
    name: str
    members: list[tuple[str, str | None]]  # (name, value expr or None)


@dataclass
class StructField:
    ctype: str
    name: str


@dataclass
class StructEntry:
    name: str
    fields: list[StructField]
    is_union: bool = False
    is_opaque: bool = False


@dataclass
class CallbackEntry:
    name: str
    signature: str
    comment: str = ""


@dataclass
class DefineEntry:
    name: str
    value: str


def read_version() -> str:
    text = VERSION_H.read_text(encoding="utf-8", errors="replace")
    major = re.search(r"SITUATION_VERSION_MAJOR\s+(\d+)", text)
    minor = re.search(r"SITUATION_VERSION_MINOR\s+(\d+)", text)
    patch = re.search(r"SITUATION_VERSION_PATCH\s+(\d+)", text)
    desc = re.search(r'SITUATION_VERSION_DESCRIPTION\s+"([^"]+)"', text)
    if major and minor and patch:
        v = f"{major.group(1)}.{minor.group(1)}.{patch.group(1)}"
        if desc:
            return f"{v} ({desc.group(1)})"
        return v
    return "unknown"


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def _expand_situation_api_includes(path: Path, seen: set[Path] | None = None) -> list[str]:
    """Expand `#include "situation_api_*.h"` submodules for binding/doc parsers (P2.2)."""
    if seen is None:
        seen = set()
    path = path.resolve()
    if path in seen:
        return []
    seen.add(path)

    out: list[str] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = INCLUDE_QUOTED_RE.match(line)
        if m:
            inc_name = m.group(1)
            if inc_name.startswith("situation_api_") and inc_name.endswith(".h"):
                inc_path = (path.parent / inc_name).resolve()
                if inc_path.is_file():
                    out.extend(_expand_situation_api_includes(inc_path, seen))
                    continue
        out.append(line)
    return out


def read_expanded_api_lines(path: Path = API_H) -> list[str]:
    """Read the public API header tree as a flat line list (umbrella + P2.2 submodules)."""
    return _expand_situation_api_includes(path)


def parse_api_header(path: Path = API_H) -> list[ApiEntry]:
    lines = read_expanded_api_lines(path)
    section = "General"
    entries: list[ApiEntry] = []
    pending_comment: list[str] = []

    # Join multi-line SITAPI declarations into a single logical line before processing.
    # A multi-line declaration starts with SITAPI and has no ';' until a later line.
    joined_lines: list[str] = []
    i = 0
    while i < len(lines):
        raw = lines[i].rstrip()
        stripped = raw.strip()
        if stripped.startswith("SITAPI") and ";" not in stripped:
            # Consume continuation lines until we hit the closing ');'
            parts = [stripped]
            i += 1
            while i < len(lines):
                cont = lines[i].rstrip().strip()
                parts.append(cont)
                i += 1
                if ";" in cont:
                    break
            # Collapse to single line, normalise whitespace in param list
            joined = " ".join(parts)
            joined = re.sub(r"\s+", " ", joined)
            joined_lines.append(joined)
        else:
            joined_lines.append(raw)
            i += 1

    for raw in joined_lines:
        line = raw.rstrip()
        sm = SECTION_RE.match(line.strip())
        if sm:
            section = sm.group(1).strip()
            pending_comment = []
            continue

        if line.strip().startswith("/**"):
            pending_comment = []
            continue
        if pending_comment is not None and line.strip().startswith("*"):
            t = line.strip().lstrip("*").strip()
            if t and not t.startswith("/"):
                pending_comment.append(t)
            continue
        if line.strip() == "*/":
            continue

        api_line = line.strip()
        if api_line.startswith("SITAPI"):
            api_line = "SITAPI " + strip_c_api_attributes(api_line)
        m = SITAPI_RE.match(api_line)
        if not m:
            if not line.strip().startswith("SITAPI"):
                pending_comment = []
            continue

        full_sig, name, inline = m.group(1), m.group(2), m.group(3) or ""
        if name.startswith("_"):
            pending_comment = []
            continue

        comment = " ".join(pending_comment).strip()
        if inline and not comment:
            comment = inline.strip()
        elif inline and comment:
            comment = f"{comment} {inline.strip()}"

        is_variadic = "..." in full_sig
        has_fn_ptr = "(*" in full_sig or ")(*" in full_sig
        manual_only = False
        manual_reason = ""
        if is_variadic:
            manual_only = True
            manual_reason = "variadic C function — wrap manually in Odin"
        elif name == "SituationSetLogCallback":
            manual_only = True
            manual_reason = "callback registration with nested proc type"

        entries.append(
            ApiEntry(
                name=name,
                signature=full_sig.strip(),
                section=section,
                comment=comment,
                is_variadic=is_variadic,
                has_function_pointer=has_fn_ptr,
                manual_only=manual_only,
                manual_reason=manual_reason,
            )
        )
        pending_comment = []

    return entries


def parse_errno_enum(path: Path = ERRNO_H) -> EnumEntry:
    text = path.read_text(encoding="utf-8", errors="replace")
    members: list[tuple[str, str | None]] = []
    for name, value in ERRNO_X_RE.findall(text):
        members.append((name, value.strip()))
    return EnumEntry(name="SituationError", members=members)


def parse_defines(path: Path = ETC_H) -> list[DefineEntry]:
    entries: list[DefineEntry] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = DEFINE_RE.match(line.strip())
        if not m:
            continue
        name, value = m.group(1), m.group(2).strip()
        if "(" in value and "ColorRGBA" in value:
            continue  # skip composite color macros for now
        entries.append(DefineEntry(name=name, value=value))
    return entries


def parse_callbacks(text: str) -> list[CallbackEntry]:
    # Match callback typedefs: typedef <ret> (*<Name>)(<params>); // comment
    # Strategy: first strip inline comments from typedef blocks to avoid ')' in comments
    # breaking the param regex, then match.
    entries: list[CallbackEntry] = []
    # Strip all single-line comments to avoid ')' inside comments confusing the regex
    clean_text = re.sub(r"//[^\n]*", "", text)
    for m in re.finditer(
        r"typedef\s+([\w\s*]+?)\s*\(\*\s*(\w+)\s*\)\s*\(([^)]*)\)\s*;",
        clean_text,
    ):
        ret_type = m.group(1).strip()
        name = m.group(2)
        params_str = m.group(3)
        # Collapse whitespace for clean storage
        params_clean = re.sub(r"\s+", " ", params_str).strip()
        clean_sig = f"{ret_type} (*{name})({params_clean})"
        # Try to find the trailing comment from the original text at this position
        # Look for ); // <comment> on the same line in the original
        end_pos = m.end()
        # Map back to original text: find the typedef name near this region
        orig_m = re.search(
            r"typedef\s+[\w\s*]+?\s*\(\*\s*" + re.escape(name) + r"\s*\)\s*\([^;]*\)\s*;[ \t]*(?://\s*(.*))?",
            text,
        )
        comment = ""
        if orig_m and orig_m.group(1):
            comment = orig_m.group(1).strip()
        entries.append(
            CallbackEntry(
                name=name,
                signature=clean_sig,
                comment=comment,
            )
        )
    return entries


def _parse_enum_block(name: str, body: str) -> EnumEntry:
    members: list[tuple[str, str | None]] = []
    for line in body.splitlines():
        line = line.strip().rstrip(",")
        if not line or line.startswith("//"):
            continue
        m = ENUM_MEMBER_RE.match(line)
        if m:
            members.append((m.group(1), m.group(2).strip() if m.group(2) else None))
    return EnumEntry(name=name, members=members)


def parse_enums(path: Path = API_H) -> list[EnumEntry]:
    text = _strip_comments("\n".join(read_expanded_api_lines(path)))
    entries: list[EnumEntry] = []

    for m in re.finditer(
        r"typedef\s+enum(?:\s+(\w+))?\s*\{([^}]+)\}\s*(\w+)\s*;",
        text,
        re.DOTALL,
    ):
        tag, body, name = m.group(1), m.group(2), m.group(3)
        if name.startswith("_"):
            continue
        entries.append(_parse_enum_block(name, body))

    for m in re.finditer(
        r"typedef\s+enum\s*\{([^}]+)\}\s*(\w+)\s*;",
        text,
        re.DOTALL,
    ):
        body, name = m.group(1), m.group(2)
        if any(e.name == name for e in entries):
            continue
        entries.append(_parse_enum_block(name, body))

    return entries


def _split_fields(body: str) -> list[StructField]:
    fields: list[StructField] = []
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)
    for part in body.split(";"):
        part = part.strip()
        if not part or part.startswith("//"):
            continue
        part = re.sub(r"\s+", " ", part)
        m = re.match(r"^(.+?)\s+(\w+)$", part)
        if m:
            fields.append(StructField(ctype=m.group(1).strip(), name=m.group(2).strip()))
    return fields


def parse_structs(path: Path = API_H) -> list[StructEntry]:
    text = "\n".join(read_expanded_api_lines(path))
    entries: list[StructEntry] = []

    # Opaque forward declarations
    for name in re.findall(r"typedef\s+struct\s+(\w+)\s+(\w+)\s*;", text):
        struct_tag, alias = name
        if struct_tag == alias:
            entries.append(StructEntry(name=alias, fields=[], is_opaque=True))

    # Single-line struct/union typedefs
    for kind, _tag, body, name in re.findall(
        r"typedef\s+(struct|union)\s+(\w+)\s*\{([^}]+)\}\s*(\w+)\s*;",
        text,
    ):
        entries.append(
            StructEntry(
                name=name,
                fields=_split_fields(body),
                is_union=(kind == "union"),
            )
        )

    for kind, body, name in re.findall(
        r"typedef\s+(struct|union)\s*\{([^}]+)\}\s*(\w+)\s*;",
        text,
    ):
        if any(e.name == name for e in entries):
            continue
        entries.append(
            StructEntry(
                name=name,
                fields=_split_fields(body),
                is_union=(kind == "union"),
            )
        )

    # Pointer typedef aliases
    for inner, name in re.findall(
        r"typedef\s+struct\s+(\w+)(?:_t)?\s*\*\s*(\w+)\s*;",
        text,
    ):
        if not any(e.name == name for e in entries):
            entries.append(StructEntry(name=name, fields=[], is_opaque=True))

    # Dedupe by name — prefer struct with fields over opaque
    by_name: dict[str, StructEntry] = {}
    for e in entries:
        prev = by_name.get(e.name)
        if prev is None or (prev.is_opaque and e.fields):
            by_name[e.name] = e
    return list(by_name.values())


def categorize_entries(entries: list[ApiEntry]) -> dict[str, list[ApiEntry]]:
    out: dict[str, list[ApiEntry]] = {}
    for e in entries:
        out.setdefault(e.section, []).append(e)
    return out
