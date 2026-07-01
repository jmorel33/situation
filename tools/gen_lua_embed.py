#!/usr/bin/env python3
"""Compile staged Lua sources to embedded bytecode for sit_lua_host.c.

Usage:
  python tools/gen_lua_embed.py <stage_dir> <out_dir> [--luajit PATH]
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def find_luajit(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit)
        if p.is_file():
            return p
        raise SystemExit(f"luajit not found: {explicit}")

    candidates = [
        ROOT / "_languages" / "lua" / "luajit" / "bin" / "luajit.exe",
        Path(r"C:\msys64\mingw64\bin\luajit.exe"),
    ]
    for c in candidates:
        if c.is_file():
            return c
    found = subprocess.run(["where", "luajit"], capture_output=True, text=True, shell=True)
    if found.returncode == 0 and found.stdout.strip():
        return Path(found.stdout.strip().splitlines()[0])
    raise SystemExit("luajit not found — run _languages\\lua\\populate_toolchain.bat install")


def module_name(stage: Path, lua_file: Path) -> str:
    rel = lua_file.relative_to(stage)
    if rel.name == "init.lua":
        if len(rel.parts) == 1:
            return rel.parent.name or "main"
        return ".".join(rel.parts[:-1])
    if len(rel.parts) == 1:
        return "__main__"
    return ".".join(rel.with_suffix("").parts)


def c_symbol(module: str) -> str:
    sym = re.sub(r"[^A-Za-z0-9_]", "_", module)
    if sym[0].isdigit():
        sym = "m_" + sym
    return f"sit_bc_{sym}"


def bytecode_size(data: str) -> int:
    return len([part for part in data.split(",") if part.strip()])


def compile_one(luajit: Path, lua_file: Path, chunk_name: str, tmp_c: Path) -> tuple[str, int]:
    subprocess.run(
        [str(luajit), "-b", "-s", "-n", chunk_name, str(lua_file), str(tmp_c)],
        check=True,
    )
    text = tmp_c.read_text(encoding="utf-8", errors="replace")
    m_array = re.search(r"const unsigned char \w+\[\] = \{([^}]*)\};", text, re.S)
    if not m_array:
        raise SystemExit(f"failed to parse bytecode output: {tmp_c}")
    data = m_array.group(1).strip()
    m_size = re.search(r"const unsigned int \w+ = (\d+);", text)
    size = int(m_size.group(1)) if m_size else bytecode_size(data)
    return data, size


def render(stage: Path, out_dir: Path, luajit: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    tmp_dir = out_dir / "_bc_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    modules: list[tuple[str, str, str, int]] = []
    for lua_file in sorted(stage.rglob("*.lua")):
        mod = module_name(stage, lua_file)
        sym = c_symbol(mod)
        data, size = compile_one(luajit, lua_file, mod, tmp_dir / f"{sym}.c")
        modules.append((mod, sym, data, size))

    if not any(m[0] == "__main__" for m in modules):
        raise SystemExit("stage must include the example script at stage root (__main__)")

    lines = [
        '#include "sit_lua_embed.h"',
        '#include "sit_lua_runtime.h"',
        "#include <stddef.h>",
        "#include <string.h>",
        "",
        "typedef struct {",
        "    const char *name;",
        "    const unsigned char *data;",
        "    unsigned int size;",
        "} SitLuaModule;",
        "",
    ]

    for _mod, sym, data, size in modules:
        lines.append(f"static const unsigned char {sym}[] = {{{data}}};")
        lines.append(f"static const unsigned int {sym}_len = {size};")
        lines.append("")

    lines.append("static const SitLuaModule sit_lua_modules[] = {")
    for mod, sym, _data, _size in modules:
        lines.append(f'    {{"{mod}", {sym}, {sym}_len}},')
    lines.append("};")
    lines.append("static const int sit_lua_module_count =")
    lines.append("    (int)(sizeof(sit_lua_modules) / sizeof(sit_lua_modules[0]));")
    lines.append("")
    lines.append(
        """static const SitLuaModule *sit_lua_find_module(const char *name)
{
    int i;
    for (i = 0; i < sit_lua_module_count; i++) {
        if (strcmp(sit_lua_modules[i].name, name) == 0) {
            return &sit_lua_modules[i];
        }
    }
    return NULL;
}

static int sit_lua_embed_loader(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    const SitLuaModule *mod = sit_lua_find_module(name);
    if (!mod) {
        lua_pushnil(L);
        lua_pushfstring(L, "no embedded module '%s'", name);
        return 2;
    }
    if (luaL_loadbuffer(L, (const char *)mod->data, mod->size, mod->name)) {
        return 1;
    }
    if (lua_pcall(L, 0, 1, 0)) {
        return 1;
    }
    return 1;
}

void sit_lua_embed_register(lua_State *L)
{
    int i;
    lua_getfield(L, LUA_GLOBALSINDEX, "package");
    lua_getfield(L, -1, "preload");
    for (i = 0; i < sit_lua_module_count; i++) {
        const SitLuaModule *mod = &sit_lua_modules[i];
        if (strcmp(mod->name, "__main__") == 0) {
            continue;
        }
        lua_pushcfunction(L, sit_lua_embed_loader);
        lua_setfield(L, -2, mod->name);
    }
    lua_pop(L, 2);
}

int sit_lua_embed_run(lua_State *L)
{
    const SitLuaModule *main_mod = sit_lua_find_module("__main__");
    if (!main_mod) {
        lua_pushstring(L, "embedded __main__ module missing");
        return 1;
    }
    if (luaL_loadbuffer(L, (const char *)main_mod->data, main_mod->size, "__main__")) {
        return 1;
    }
    if (lua_pcall(L, 0, 0, 0)) {
        return 1;
    }
    return 0;
}
"""
    )

    (out_dir / "sit_lua_embed.c").write_text("\n".join(lines), encoding="utf-8")
    (out_dir / "sit_lua_embed.h").write_text(
        "#ifndef SIT_LUA_EMBED_H\n"
        "#define SIT_LUA_EMBED_H\n"
        "\n"
        "#include <lua.h>\n"
        "\n"
        "void sit_lua_embed_register(lua_State *L);\n"
        "int sit_lua_embed_run(lua_State *L);\n"
        "\n"
        "#endif\n",
        encoding="utf-8",
    )

    for f in tmp_dir.glob("*.c"):
        f.unlink()
    try:
        tmp_dir.rmdir()
    except OSError:
        pass

    print(f"Embedded {len(modules)} Lua modules -> {out_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate embedded Lua bytecode for Situation examples")
    parser.add_argument("stage_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--luajit", default=None)
    args = parser.parse_args()

    stage = args.stage_dir.resolve()
    if not stage.is_dir():
        raise SystemExit(f"stage dir not found: {stage}")

    render(stage, args.out_dir.resolve(), find_luajit(args.luajit))


if __name__ == "__main__":
    main()