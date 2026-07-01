#!/usr/bin/env python3
"""
gen_situation_base_trace.py
Regenerate sit/situation_base_trace.h from Situation implementation sources.

Scans sit/**/*.h for function definitions (SITAPI, static helpers, kfs_*, KTerm
bodies) and emits the X-macro trace ID table used by SituationTraceId / diagnostics.

When to run:
  After adding, removing, or renaming any function body under sit/ (especially
  situation_impl_*.h). Commit the regenerated header with the code change.

Usage (from repo root):
  python3 scripts/gen_situation_base_trace.py

Windows note:
  Use MSYS MinGW python3 — NOT the default ``python`` on PATH (often Inkscape /
  Windows Store 3.8). Example:
    & "C:\\msys64\\mingw64\\bin\\python3.exe" scripts\\gen_situation_base_trace.py

Output:
  sit/situation_base_trace.h   — do not edit by hand

Requirements:
  Python 3.8+ (stdlib only)
"""

from __future__ import annotations

import inspect
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT_DIR = ROOT / "sit"

SKIP_FILES = {
    "situation_api.h",
    "kterm_api.h",
    "situation_base_errno.h",
    "situation_base_font.h",
    "situation_base_version.h",
    "situation_base_etc.h",
    "situation_base_trace.h",
    "situation_impl_deps.h",
    "situation_impl.h",
    "situation_impl_proj.h",
    "mock_situation.h",
}

SKIP_DIR_PARTS = {
    "tests",
    "example",
    "examples",
    "mybuddy",
    "shaders",
    "doc",
    "logs",
    "deps",
}

SKIP_NAME_PREFIXES = (
    "stb_",
    "pcap_",
    "VK_",
    "GL_",
    "ma_",
)

FILE_PRIORITY = {
    "situation_impl_forward.h": 10,
    "situation_impl_renderer_fwd.h": 20,
    "situation_impl_decl.h": 30,
    "situation_impl_etc.h": 40,
    "situation_impl_timer.h": 50,
    "situation_impl_threading.h": 60,
    "situation_impl_threading_diag.h": 70,
    "situation_impl_io.h": 80,
    "situation_impl_input.h": 90,
    "situation_impl_wdm.h": 100,
    "situation_impl_color.h": 105,
    "situation_impl_image.h": 110,
    "situation_impl_audio.h": 120,
    "situation_impl_renderer.h": 130,
    "situation_impl_vd.h": 140,
    "situation_impl_ctrl.h": 150,
    "kterm_impl.h": 160,
    "lib_kfs.h": 170,
}

API_BASE = 10_000_000
INTERNAL_BASE = 20_000_000
FILE_STRIDE = 10_000

SKIP_NAMES = {
    "if", "for", "while", "switch", "return", "sizeof", "defined",
    "X", "do", "case", "default", "break", "continue", "goto",
}

RE_KTERM_API_NAME = re.compile(r"\b(KTerm_\w+)\s*\(")

def should_skip_path(path: Path) -> bool:
    if path.name in SKIP_FILES:
        return True
    if path.name.startswith("stb_"):
        return True
    if path.name == "pcap.h":
        return True
    if "deps" in path.parts:
        return True
    if set(path.parts) & SKIP_DIR_PARTS:
        return True
    return path.suffix != ".h"


def rel_display(path: Path) -> str:
    return path.relative_to(SIT_DIR).as_posix()


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def load_kterm_api_names() -> set[str]:
    path = SIT_DIR / "k-term" / "kterm_api.h"
    if not path.exists():
        return set()
    text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    return set(RE_KTERM_API_NAME.findall(text))


def is_traceable_name(name: str) -> bool:
    if name in SKIP_NAMES:
        return False
    if name.startswith(SKIP_NAME_PREFIXES):
        return False
    if name.startswith("SITUATION_") and name.isupper():
        return False
    return True


RE_FUNC_DEF_START = re.compile(
    r"^(?:SITAPI|static|(?:int|void|char|size_t|uint\d*_t|bool|float|double)\s+\w+)"
)


def is_function_definition_start(stripped: str) -> bool:
    if stripped.startswith('"') or stripped.startswith("#"):
        return False
    if "(" not in stripped:
        return False
    if stripped.startswith(("if ", "for ", "while ", "switch ", "return ", "else ")):
        return False
    if stripped.startswith("SITAPI") or stripped.startswith("static"):
        return True
    if RE_FUNC_DEF_START.match(stripped):
        return True
    if re.match(r"^void\s+(KTerm|Situation)\w+\s*\(", stripped):
        return True
    return False


def definition_prefix(stripped: str) -> str:
    if stripped.startswith("SITAPI"):
        return "SITAPI"
    if stripped.startswith("static"):
        return "static"
    return "plain"


def function_name_from_def(line: str) -> str | None:
    head = line.split("{", 1)[0]
    matches = list(re.finditer(r"\b(\w+)\s*\(", head))
    if not matches:
        return None
    return matches[-1].group(1)


def is_forward_declaration(stripped: str) -> bool:
    return "{" not in stripped and re.search(r"\)\s*;\s*$", stripped) is not None


def iter_definition_lines(text: str) -> list[tuple[str, str]]:
    """Return (kind_prefix, joined_line) for each function definition."""
    lines = strip_comments(text).splitlines()
    out: list[tuple[str, str]] = []
    i = 0
    while i < len(lines):
        stripped = lines[i].strip()
        if not stripped or stripped.startswith("#") or stripped.startswith('"'):
            i += 1
            continue

        if not is_function_definition_start(stripped):
            i += 1
            continue

        if is_forward_declaration(stripped):
            i += 1
            continue

        prefix = definition_prefix(stripped)
        buf = stripped
        if "{" not in buf:
            i += 1
            complete = False
            while i < len(lines):
                part = lines[i].strip()
                if not part or part.startswith("#") or part.startswith('"'):
                    i += 1
                    continue
                buf += " " + part
                i += 1
                if is_forward_declaration(buf):
                    buf = ""
                    break
                if ";" in part and "{" not in buf:
                    buf = ""
                    break
                if "{" in part:
                    complete = True
                    break
            if complete and buf:
                out.append((prefix, buf))
        else:
            out.append((prefix, buf))
            i += 1
    return out


def classify_definition(prefix: str, line: str, name: str, kterm_api: set[str]) -> str | None:
    if not is_traceable_name(name):
        return None
    if prefix == "SITAPI":
        return "api"
    if prefix == "static":
        return "internal"
    if name.startswith("kfs_"):
        return "api"
    if name in kterm_api:
        return "api"
    return None


def extract_functions(text: str, kterm_api: set[str]) -> tuple[list[str], list[str]]:
    api: list[str] = []
    internal: list[str] = []
    seen_api: set[str] = set()
    seen_internal: set[str] = set()

    for prefix, line in iter_definition_lines(text):
        name = function_name_from_def(line)
        if not name:
            continue
        kind = classify_definition(prefix, line, name, kterm_api)
        if kind == "api" and name not in seen_api:
            seen_api.add(name)
            api.append(name)
        elif kind == "internal" and name not in seen_internal:
            seen_internal.add(name)
            internal.append(name)
    return api, internal


def section_macro_name(rel_path: str) -> str:
    stem = rel_path.replace("/", "_").replace("-", "_").replace(".", "_")
    return f"SITUATION_TRACE_{stem.upper()}"


def trace_enum_name(func: str) -> str:
    return f"SIT_TRACE_{func}"


def format_x_row(ename: str, value: int, sym: str, name_w: int, val_w: int, cont: bool) -> str:
    suffix = " \\" if cont else ""
    pad = max(1, name_w - len(ename))
    return f'    X({ename}, {" " * pad}{value:>{val_w}d}, "{sym}"){suffix}'


def render_section(
    rel: str,
    api_entries: list[tuple[str, int, str]],
    internal_entries: list[tuple[str, int, str]],
) -> tuple[list[str], str]:
    macro = section_macro_name(rel)
    all_entries = api_entries + internal_entries
    if not all_entries:
        return [], macro

    name_w = max(len(e[0]) for e in all_entries)
    val_w = max(len(str(e[1])) for e in all_entries)

    parts: list[str] = []
    if api_entries:
        parts.append(f"API {api_entries[0][1]:08d} – {api_entries[-1][1]:08d}")
    if internal_entries:
        parts.append(f"internal {internal_entries[0][1]:08d} – {internal_entries[-1][1]:08d}")
    header = f"// ── {rel} ({', '.join(parts)}) ──"

    lines = [header, f"#define {macro}(X) \\"]
    for i, (ename, value, sym) in enumerate(all_entries):
        lines.append(format_x_row(ename, value, sym, name_w, val_w, i < len(all_entries) - 1))
    lines.append("")
    return lines, macro


def collect_source_files() -> list[Path]:
    files: list[Path] = []
    for path in sorted(SIT_DIR.rglob("*.h")):
        if should_skip_path(path):
            continue
        files.append(path)
    files.sort(
        key=lambda p: (
            FILE_PRIORITY.get(p.name, 200),
            str(p).replace("\\", "/"),
        )
    )
    return files


def generate() -> tuple[str, int, int, int]:
    kterm_api = load_kterm_api_names()
    global_api_seen: set[str] = set()
    global_internal_seen: set[str] = set()
    file_index = 0

    sections: list[tuple[str, list[tuple[str, int, str]], list[tuple[str, int, str]]]] = []
    total_api = 0
    total_internal = 0

    for path in collect_source_files():
        rel = rel_display(path)
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
        except OSError as e:
            print(f"skip unreadable {path}: {e}", file=sys.stderr)
            continue

        api_names, internal_names = extract_functions(raw, kterm_api)
        api_base = API_BASE + 1 + file_index * FILE_STRIDE
        internal_base = INTERNAL_BASE + 1 + file_index * FILE_STRIDE

        api_entries: list[tuple[str, int, str]] = []
        internal_entries: list[tuple[str, int, str]] = []
        api_offset = 0
        internal_offset = 0

        for name in api_names:
            if name in global_api_seen:
                continue
            global_api_seen.add(name)
            api_entries.append((trace_enum_name(name), api_base + api_offset, name))
            api_offset += 1

        for name in internal_names:
            if name in global_internal_seen:
                continue
            global_internal_seen.add(name)
            internal_entries.append((trace_enum_name(name), internal_base + internal_offset, name))
            internal_offset += 1

        if not api_entries and not internal_entries:
            continue

        sections.append((rel, api_entries, internal_entries))
        total_api += len(api_entries)
        total_internal += len(internal_entries)
        file_index += 1

    lines: list[str] = [
        "/**",
        " * @file situation_base_trace.h",
        " * @brief Situation function trace ID table (X-macro single source of truth).",
        " *",
        " * Auto-generated by scripts/gen_situation_base_trace.py — do not edit by hand.",
        " * Regenerate after adding or renaming library functions.",
        " *",
        " * FORMAT: X(NAME, VALUE, SYMBOL)",
        " *   NAME   — enum constant (SIT_TRACE_<FunctionName>)",
        " *   VALUE  — 8-digit trace ID (+10000 per source file)",
        " *   SYMBOL — C function name string",
        " *",
        " * Sources:",
        " *   - API (10000001+): SITAPI, kfs_*, and KTerm public functions with bodies",
        " *   - Internal (20000001+): static and file-local helpers with bodies",
        " *",
        " * Excludes declaration-only headers: situation_api.h, kterm_api.h",
        " */",
        "",
        "#ifndef SITUATION_BASE_TRACE_H",
        "#define SITUATION_BASE_TRACE_H",
        "",
        "//==================================================================================",
        "//  SituationTraceId — X-Macro Function Trace Table",
        "//==================================================================================",
        "",
        "//  FORMAT: X(NAME, VALUE, SYMBOL)",
        "//",
        f"//  {len(sections)} implementation files | {total_api} API | {total_internal} internal | stride {FILE_STRIDE}",
        "//",
    ]

    if sections:
        for rel, api_entries, internal_entries in sections:
            if api_entries:
                lines.append(f"//    {rel}: API from {api_entries[0][1]:08d}")
                break
        lines.append("")

    section_names: list[str] = []
    for rel, api_entries, internal_entries in sections:
        block, macro = render_section(rel, api_entries, internal_entries)
        lines.extend(block)
        section_names.append(macro)

    lines.extend([
        "//==================================================================================",
        "//  Master Table",
        "//==================================================================================",
        "#define SITUATION_TRACE_TABLE(X) \\",
    ])
    for i, name in enumerate(section_names):
        suffix = " \\" if i < len(section_names) - 1 else ""
        lines.append(f"    {name}(X){suffix}")
    lines.append("")

    lines.extend([
        "//==================================================================================",
        "//  Generated Enum",
        "//==================================================================================",
        "typedef enum {",
        "    #define _SIT_TRACE_ENUM(name, value, sym) name = value,",
        "    SITUATION_TRACE_TABLE(_SIT_TRACE_ENUM)",
        "    #undef _SIT_TRACE_ENUM",
        "    SITUATION_TRACE_ID_COUNT",
        "} SituationTraceId;",
        "",
        "#endif // SITUATION_BASE_TRACE_H",
        "",
    ])

    return "\n".join(lines), len(sections), total_api, total_internal


def write_generated(path: Path, content: str) -> None:
    """Write UTF-8 with Unix newlines (Py 3.10+ Path.write_text newline=, else open)."""
    sig = inspect.signature(Path.write_text)
    if "newline" in sig.parameters:
        path.write_text(content, encoding="utf-8", newline="\n")
    else:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)


def main() -> int:
    content, file_count, api_count, internal_count = generate()
    out = SIT_DIR / "situation_base_trace.h"
    write_generated(out, content)
    print(f"Wrote {out}")
    print(f"  Source files: {file_count}")
    print(f"  API impls:    {api_count}")
    print(f"  Internal:     {internal_count}")
    print(f"  Total:        {api_count + internal_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
