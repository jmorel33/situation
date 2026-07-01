#!/usr/bin/env python3
"""P2.3 — reorder SITAPI sections inside split headers (cosmetic; zero symbol loss)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from situation_api_parser import parse_api_header  # noqa: E402

PLATFORM = ROOT / "sit" / "situation_api_platform.h"
AUDIO = ROOT / "sit" / "situation_api_audio.h"
GRAPHICS = ROOT / "sit" / "situation_api_graphics.h"


def slice_lines(lines: list[str], start: int, end: int) -> list[str]:
    return lines[start:end]


def reorder_platform(path: Path) -> None:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)

    def find(sub: str, start: int = 0) -> int:
        for i in range(start, len(lines)):
            if sub in lines[i]:
                return i
        raise SystemExit(f"marker not found: {sub!r}")

    inc_end = find("#include", 0)
    while inc_end + 1 < len(lines) and lines[inc_end + 1].strip().startswith("#include"):
        inc_end += 1
    glfw_end = find("typedef struct GLFWmonitor", inc_end) + 1

    core_start = find("// Core Module:", glfw_end)
    window_start = find("// Window and Display Module", core_start)
    image_start = find("// Image Module:", window_start)
    input_start = find("// Input Module:", image_start)
    endif_line = find("#endif /* SITUATION_API_PLATFORM_H", input_start)

    header = lines[:glfw_end]
    preamble = lines[glfw_end:core_start]
    core = lines[core_start:window_start]
    window = lines[window_start:image_start]
    image = lines[image_start:input_start]
    input_mod = lines[input_start:endif_line]

    # Extract logging SITAPI (currently before core) — keep types/macros in preamble.
    logging_api: list[str] = []
    kept_preamble: list[str] = []
    i = 0
    while i < len(preamble):
        line = preamble[i]
        if line.startswith("SITAPI void SituationLog(") or line.startswith(
            "SITAPI void SituationSetTraceLogLevel"
        ) or line.startswith("SITAPI void SituationSetLogCallback") or line.startswith(
            "SITAPI void SituationShowMessageBox"
        ) or line.startswith("SITAPI void SituationLogWarning"):
            logging_api.append(line)
            i += 1
            continue
        if line.strip() == "#define SITUATION_LOG_WARNING SituationLogWarning":
            logging_api.append(line)
            i += 1
            continue
        kept_preamble.append(line)
        i += 1

    logging_mod = [
        "//==================================================================================\n",
        "// Logging Module\n",
        "//==================================================================================\n",
        "\n",
    ] + logging_api

    out = (
        header
        + kept_preamble
        + core
        + window
        + image
        + input_mod
        + logging_mod
        + [lines[endif_line]]
    )
    path.write_text("".join(out), encoding="utf-8")


def reorder_audio(path: Path) -> None:
    text = path.read_text(encoding="utf-8")

    tone_block_re = re.compile(
        r"// --- Resonance \(Procedural Synthesis\) ---\n"
        r"// SituationToneHandle is defined in situation_base_types\.h\n"
        r"\n"
        r"/\*\*\n"
        r" \* @brief Plays an extended procedural tone with full control\.\n"
        r"(?:.*\n)*?"
        r"\);\n",
        re.MULTILINE,
    )
    m = tone_block_re.search(text)
    if not m:
        raise SystemExit("PlayToneEx block not found in situation_api_audio.h")
    tone_block = m.group(0)
    text = text[: m.start()] + text[m.end() :]

    old_tones = (
        "SITAPI void SituationStopTone(SituationToneHandle handle);"
        "                              // Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored.\n"
        "\n"
        "SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec); // Legacy: play a simple ADSR tone (backward compat / quick UI sounds).\n"
        "SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);   // Legacy: play a tone by MIDI note number (0-127).\n"
        "SITAPI void SituationStopAllTones(void);                                                // Stop all active tones (triggers release on each).\n"
    )
    if old_tones not in text:
        raise SystemExit("tone API cluster not found in situation_api_audio.h")

    new_tones = (
        "// --- Procedural Tones (Resonance) ---\n"
        + tone_block
        + "\n"
        "SITAPI void SituationStopTone(SituationToneHandle handle);                              // Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored.\n"
        "\n"
        "SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec); // Legacy: play a simple ADSR tone (backward compat / quick UI sounds).\n"
        "SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);   // Legacy: play a tone by MIDI note number (0-127).\n"
        "SITAPI void SituationStopAllTones(void);                                                // Stop all active tones (triggers release on each).\n"
    )
    text = text.replace(old_tones, new_tones, 1)
    path.write_text(text, encoding="utf-8")


def reorder_graphics(path: Path) -> None:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    feat_idx = next(i for i, ln in enumerate(lines) if "SituationIsFeatureSupported" in ln)
    if feat_idx == 0 or "Graphics Module" in lines[feat_idx - 3]:
        return
    feat_lines: list[str] = []
    j = feat_idx
    while j < len(lines) and not lines[j].startswith("// --- Profiling"):
        feat_lines.append(lines[j])
        j += 1
    if not feat_lines:
        return
    del lines[feat_idx:j]
    insert = next(i for i, ln in enumerate(lines) if ln.startswith("// --- Profiling"))
    for k, ln in enumerate(feat_lines):
        lines.insert(insert + k, ln)
    path.write_text("".join(lines), encoding="utf-8")


def main() -> None:
    before = len(parse_api_header())
    reorder_platform(PLATFORM)
    reorder_audio(AUDIO)
    reorder_graphics(GRAPHICS)
    after = len(parse_api_header())
    if before != after:
        raise SystemExit(f"SITAPI declaration count changed: {before} -> {after}")
    names = {e.name for e in parse_api_header()}
    print(f"P2.3 reorder OK — {after} SITAPI declarations ({len(names)} unique names)")


if __name__ == "__main__":
    main()
