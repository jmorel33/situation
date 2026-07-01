"""Load situation_opengl.dll / situation_vulkan.dll and bind ctypes signatures."""

from __future__ import annotations

import os
import sys
from ctypes import CDLL
from pathlib import Path

from . import foreign

_dll = None
_backend: str | None = None


def _try_repo_root() -> Path | None:
    for parent in Path(__file__).resolve().parents:
        if (parent / "sit" / "situation_api.h").is_file():
            return parent
    return None


def _candidate_dll_dirs() -> list[Path]:
    pkg_dir = Path(__file__).resolve().parent
    staging_dir = pkg_dir.parent  # e.g. build/examples/python when package is staged
    dirs: list[Path] = [staging_dir, Path.cwd(), pkg_dir]
    root = _try_repo_root()
    if root is not None:
        dirs.extend(
            [
                root / "build" / "examples" / "python",
                root / "build" / "dll",
            ]
        )
    if getattr(sys, "frozen", False):
        dirs.insert(0, Path(sys.executable).parent)
    # Preserve order, drop duplicates
    seen: set[Path] = set()
    unique: list[Path] = []
    for d in dirs:
        key = d.resolve()
        if key not in seen:
            seen.add(key)
            unique.append(d)
    return unique


def find_dll(backend: str = "opengl") -> Path:
    name = f"situation_{backend}.dll"
    for directory in _candidate_dll_dirs():
        candidate = directory / name
        if candidate.is_file():
            return candidate
    searched = ", ".join(str(d) for d in _candidate_dll_dirs())
    raise FileNotFoundError(
        f"{name} not found. Build with build\\build_situation.bat {backend} "
        f"and/or run build\\build_python_example.bat {backend}. Searched: {searched}"
    )


def load_dll(backend: str = "opengl") -> CDLL:
    """Load the Situation DLL for backend ('opengl' or 'vulkan') and bind all functions."""
    global _dll, _backend
    path = find_dll(backend)
    dll_dir = str(path.parent)
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(dll_dir)
    elif dll_dir not in os.environ.get("PATH", ""):
        os.environ["PATH"] = dll_dir + os.pathsep + os.environ.get("PATH", "")
    _dll = CDLL(str(path))
    _backend = backend
    foreign.bind_all(_dll)
    return _dll


def get_dll() -> CDLL:
    if _dll is None:
        return load_dll("opengl")
    return _dll


def get_backend() -> str:
    return _backend or "opengl"
