#!/usr/bin/env python3
"""Embed runtime DLLs for the self-contained Lua example exe.

Usage:
  python tools/gen_lua_dll_embed.py <out_dir> <dll_path> [<dll_path> ...]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def render_blob(name: str, data: bytes) -> list[str]:
    chunks = ",\n".join(
        ", ".join(str(b) for b in data[i : i + 16]) for i in range(0, len(data), 16)
    )
    return [
        f"static const unsigned char sit_lua_embed_{name}[] = {{{chunks}}};",
        f"static const unsigned int sit_lua_embed_{name}_size = {len(data)};",
    ]


def canonical_embed_filename(dll_path: Path) -> str:
    """Runtime extract name — host expects situation_<backend>.dll and lua51.dll."""
    name = dll_path.name.lower()
    if "situation_opengl" in name:
        return "situation_opengl.dll"
    if "situation_vulkan" in name:
        return "situation_vulkan.dll"
    if name.startswith("lua51"):
        return "lua51.dll"
    return dll_path.name


def render(dll_paths: list[Path], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    entries: list[tuple[str, str, int]] = []
    blob_lines: list[str] = []
    for dll_path in dll_paths:
        data = dll_path.read_bytes()
        stem = dll_path.stem.replace("-", "_").replace(".", "_")
        blob_lines.extend(render_blob(stem, data))
        blob_lines.append("")
        entries.append((canonical_embed_filename(dll_path), stem, len(data)))

    table_rows = []
    for filename, stem, _size in entries:
        table_rows.append(
            f'    {{"{filename}", sit_lua_embed_{stem}, sit_lua_embed_{stem}_size}},'
        )

    (out_dir / "sit_lua_dll_embed.c").write_text(
        "\n".join(
            [
                '#include "sit_lua_dll_embed.h"',
                "",
                "#include <stdio.h>",
                "#include <string.h>",
                "",
                "#if defined(_WIN32)",
                "#include <windows.h>",
                "#endif",
                "",
                "typedef struct {",
                "    const char *filename;",
                "    const unsigned char *data;",
                "    unsigned int size;",
                "} SitLuaEmbeddedDll;",
                "",
                *blob_lines,
                "static const SitLuaEmbeddedDll sit_lua_embedded_dlls[] = {",
                *table_rows,
                "};",
                "static const int sit_lua_embedded_dll_count =",
                "    (int)(sizeof(sit_lua_embedded_dlls) / sizeof(sit_lua_embedded_dlls[0]));",
                "",
                "int sit_lua_extract_embedded_dlls(char *out_dir, size_t out_dir_size)",
                "{",
                "    FILE *f;",
                "    size_t written;",
                "    int i;",
                "#if defined(_WIN32)",
                "    char base[MAX_PATH];",
                "    char folder[MAX_PATH];",
                "    DWORD pid = GetCurrentProcessId();",
                "    DWORD n = GetTempPathA((DWORD)sizeof(base), base);",
                "    if (n == 0 || n >= sizeof(base)) {",
                "        return -1;",
                "    }",
                "    snprintf(folder, sizeof(folder), \"%ssituation_lua_%lu\", base, (unsigned long)pid);",
                "    if (!CreateDirectoryA(folder, NULL)) {",
                "        DWORD err = GetLastError();",
                "        if (err != ERROR_ALREADY_EXISTS) {",
                "            return -2;",
                "        }",
                "    }",
                "#else",
                "    char folder[1024];",
                "    snprintf(folder, sizeof(folder), \"/tmp/situation_lua_%d\", getpid());",
                "    mkdir(folder, 0755);",
                "#endif",
                "    for (i = 0; i < sit_lua_embedded_dll_count; i++) {",
                "        const SitLuaEmbeddedDll *entry = &sit_lua_embedded_dlls[i];",
                "        char path[1024];",
                "#if defined(_WIN32)",
                "        snprintf(path, sizeof(path), \"%s\\\\%s\", folder, entry->filename);",
                "#else",
                "        snprintf(path, sizeof(path), \"%s/%s\", folder, entry->filename);",
                "#endif",
                "        f = fopen(path, \"wb\");",
                "        if (!f) {",
                "            return -3;",
                "        }",
                "        written = fwrite(entry->data, 1, entry->size, f);",
                "        fclose(f);",
                "        if (written != entry->size) {",
                "            return -4;",
                "        }",
                "    }",
                "    if (out_dir_size == 0) {",
                "        return -5;",
                "    }",
                "    strncpy(out_dir, folder, out_dir_size - 1);",
                "    out_dir[out_dir_size - 1] = '\\0';",
                "    return 0;",
                "}",
                "",
                "int sit_lua_extract_embedded_dll(char *out_path, size_t out_path_size)",
                "{",
                "    char dir[1024];",
                "    int rc = sit_lua_extract_embedded_dlls(dir, sizeof(dir));",
                "    const SitLuaEmbeddedDll *entry;",
                "    if (rc != 0) {",
                "        return rc;",
                "    }",
                "    if (sit_lua_embedded_dll_count <= 0) {",
                "        return -6;",
                "    }",
                "    entry = &sit_lua_embedded_dlls[0];",
                "#if defined(_WIN32)",
                "    snprintf(out_path, out_path_size, \"%s\\\\%s\", dir, entry->filename);",
                "#else",
                "    snprintf(out_path, out_path_size, \"%s/%s\", dir, entry->filename);",
                "#endif",
                "    return 0;",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out_dir / "sit_lua_dll_embed.h").write_text(
        "\n".join(
            [
                "#ifndef SIT_LUA_DLL_EMBED_H",
                "#define SIT_LUA_DLL_EMBED_H",
                "",
                "#include <stddef.h>",
                "",
                "int sit_lua_extract_embedded_dlls(char *out_dir, size_t out_dir_size);",
                "int sit_lua_extract_embedded_dll(char *out_path, size_t out_path_size);",
                "",
                "#endif",
                "",
            ]
        ),
        encoding="utf-8",
    )

    names = ", ".join(p.name for p in dll_paths)
    total = sum(len(p.read_bytes()) for p in dll_paths)
    print(f"Embedded DLLs [{names}] ({total} bytes) -> {out_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Embed runtime DLLs for Lua host exe")
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("dll_paths", nargs="+", type=Path)
    args = parser.parse_args()

    dlls = []
    for raw in args.dll_paths:
        dll = raw.resolve()
        if not dll.is_file():
            raise SystemExit(f"dll not found: {dll}")
        dlls.append(dll)

    render(dlls, args.out_dir.resolve())


if __name__ == "__main__":
    main()