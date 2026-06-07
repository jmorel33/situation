#!/usr/bin/env python3
"""Repair situation_api.md <details> structure after bulk port insert."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API_MD = ROOT / "doc" / "situation_api.md"


def extract_block(text: str, start_marker: str, end_marker: str) -> str:
    i = text.index(start_marker)
    j = text.index(end_marker, i) + len(end_marker)
    return text[i:j].strip()


def main() -> None:
    text = API_MD.read_text(encoding="utf-8")

    core = extract_block(text, "#### `SituationGetInitState`", "Do not call from the audio callback thread.")
    maximize = extract_block(
        text,
        "#### `SituationSetMaximizeCallback`",
        "- `user_data` — Opaque pointer passed to the callback.",
    )
    graphics = extract_block(
        text,
        "### Dynamic raster state (command buffer)",
        "void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);",
    )

    # 1) Remove duplicate core inserted after Core </details>
    dup = (
        "</details>\n<details>\n---\n"
        + core
        + "\n\n<summary><h3>Window and Display Module</h3></summary>"
    )
    if dup not in text:
        raise SystemExit("duplicate core block pattern not found")
    text = text.replace(dup, "</details>\n<details>\n<summary><h3>Window and Display Module</h3></summary>", 1)

    # 2) Remove orphaned <details> section (maximize + graphics) before Input
    orphan = (
        "</details>\n<details>\n---\n"
        + maximize
        + "\n\n\n---\n"
        + graphics
        + "\n\n<summary><h3>Input Module</h3></summary>"
    )
    if orphan not in text:
        raise SystemExit("orphan graphics block pattern not found")
    text = text.replace(orphan, "</details>\n<details>\n<summary><h3>Input Module</h3></summary>", 1)

    # 3) Insert core docs inside Core module
    core_anchor = "- Render pass format matches the swapchain format\n\n</details>"
    if core_anchor not in text:
        raise SystemExit("core anchor not found")
    text = text.replace(
        core_anchor,
        "- Render pass format matches the swapchain format\n\n---\n" + core + "\n\n</details>",
        1,
    )

    # 4) Insert maximize inside Window module
    window_anchor = "- Data accumulates until export\n\n</details>\n<details>\n<summary><h3>Image Module</h3></summary>"
    if window_anchor not in text:
        raise SystemExit("window anchor not found")
    text = text.replace(
        window_anchor,
        "- Data accumulates until export\n\n---\n" + maximize + "\n\n</details>\n<details>\n<summary><h3>Image Module</h3></summary>",
        1,
    )

    # 5) Insert graphics inside Graphics module
    graphics_anchor = (
        "SituationCmdPipelineBarrier(cmd, &barrier);\n```\n\n</details>\n<details>\n<summary><h3>Input Module</h3></summary>"
    )
    if graphics_anchor not in text:
        raise SystemExit("graphics anchor not found")
    text = text.replace(
        graphics_anchor,
        "SituationCmdPipelineBarrier(cmd, &barrier);\n```\n\n---\n" + graphics + "\n\n</details>\n<details>\n<summary><h3>Input Module</h3></summary>",
        1,
    )

    API_MD.write_text(text, encoding="utf-8")
    print("Repaired", API_MD.relative_to(ROOT))


if __name__ == "__main__":
    main()
