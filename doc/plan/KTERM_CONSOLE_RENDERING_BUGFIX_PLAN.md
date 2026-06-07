# K-Term Console Rendering Bugfix Plan

**Date**: 2026-06-02  
**Status**: IN PROGRESS  
**Goal**: Fix two visual rendering bugs in `kterm_console.c`

## Bug 1: Box-Drawing Characters Rendering as 'M' ✅ FIXED

### Root Cause

The console banner wrote raw CP437 byte values via `KTerm_WriteChar(term, 205)` (0xCD = ═). In the terminal's processing pipeline:

1. `TranslateCharacter()` sees byte 0xCD (>= 0x80) and routes through the **GR** character set
2. Default GR = `CHARSET_DEC_SPECIAL`
3. DEC Special Graphics strips the high bit: `0xCD & 0x7F = 0x4D`
4. `KTerm_TranslateDECSpecial(0x4D)` — 0x4D is 'M' and NOT in the DEC Special range (0x5F-0x7E)
5. Returns `'M'` unchanged → stored as character 'M' in the cell → renders as 'M'

### Fix Applied

- Switched the console example to **UTF-8 mode** (`ESC %G`) before writing the banner
- Replaced raw CP437 byte writes with proper UTF-8 encoded box-drawing sequences:
  - `\xe2\x95\x94` = U+2554 ╔
  - `\xe2\x95\x90` = U+2550 ═
  - `\xe2\x95\x97` = U+2557 ╗
  - `\xe2\x95\x91` = U+2551 ║
  - `\xe2\x95\x9a` = U+255A ╚
  - `\xe2\x95\x9d` = U+255D ╝

In UTF-8 mode, these decode properly and `MapUnicodeToCP437()` maps them back to the correct CP437 atlas slots (200-206), which have the built-in bitmap font glyphs.

### Files Changed
- `examples/kterm_console.c` — banner rewritten with UTF-8 + `ESC %G` mode switch

---

## Bug 2: White Boxes on Input Line — INVESTIGATION

### Symptoms

White rectangular blocks appear on the command input line, interspersed with typed characters. They look like solid white cells behind or instead of expected characters.

### Possible Causes (Ranked by Likelihood)

1. **Unmapped glyph fallback rendering as hex box**  
   If a character reaches `KTerm_AllocateGlyph()` and no TTF is loaded, the fallback renders a white outlined rectangle (the "hex box"). This would appear as white boxes on a black background. This could happen if:
   - The UTF-8 decoder produces unexpected codepoints from the input
   - Characters written by `RedrawEditLine` get mangled by the charset translation

2. **Cursor position tracking issue**  
   `RedrawEditLine` relies on DSR (Device Status Report) to know where the prompt starts. If the DSR response is delayed or misinterpreted, the cursor may be at the wrong position, and `ESC[K` (erase to end of line) followed by character writes could produce unexpected results.

3. **Stale bg color in cleared cells**  
   If `current_bg` is non-zero when `ESC[K` runs, erased cells inherit that bg. However, the prompt always ends with `ESC[0m` which resets to COLOR_BLACK, so this is unlikely unless there's a race.

4. **Shader alpha handling**  
   The font atlas uses RGBA with alpha=0 for empty pixels. If the compute shader ignores alpha and renders the white RGB channel directly, all "lit" atlas pixels would appear as white boxes.

### Recommended Investigation Steps

- [ ] Add a `CONSOLE_LOG` in `RedrawEditLine` to print the exact characters being written and their hex values
- [ ] Check if the white boxes appear immediately on first input or only after some interaction
- [ ] Verify that in UTF-8 mode, typed ASCII characters (0x20-0x7E) don't get mangled by `TranslateCharacter`
- [ ] Check the compute shader's alpha handling for the font texture (is it `fg * alpha` or `fg * pixel_on`?)
- [ ] Test with a simple string write after the prompt instead of interactive input to isolate

### Key Code Paths

| Component | File | Function |
|-----------|------|----------|
| Character input | `kterm_console.c` | `HandlePrintableKey` → `RedrawEditLine` |
| Byte processing | `kterm_impl.h:4469` | `KTerm_ProcessNormalChar` |
| Charset translation | `kterm_impl.h:3575` | `TranslateCharacter` |
| Render buffer build | `kt_composite_sit.h:289` | `KTerm_UpdatePaneRow` |
| Glyph allocation | `kterm_impl.h:3733` | `KTerm_AllocateGlyph` |
| Fallback rendering | `kterm_impl.h:3623` | `RenderGlyphToAtlas` (hex box path) |
| Compute shader | `sit/k-term/shaders/kterm_terminal.glsl` | Fragment/cell rendering |

---

## Tasks

### Phase 1: Banner Fix ✅
- [x] Switch console to UTF-8 mode (`ESC %G`) before banner
- [x] Replace raw CP437 byte writes with UTF-8 encoded box-drawing strings
- [x] Verify compilation

### Phase 2: White Box Investigation
- [ ] Run `kterm_console.exe` with the UTF-8 fix and check if white boxes persist
- [ ] If yes: add diagnostic logging to track what char codes reach the GPU cells on the input line
- [ ] Examine the kterm compute shader for alpha blending correctness
- [ ] Check if enabling `enable_wide_chars` (UTF-8 width awareness) is needed after `ESC %G`
- [ ] Test with a minimal repro: write "hello" directly after prompt, see if white boxes appear

### Phase 3: Fix White Boxes
- [ ] Apply fix based on investigation findings
- [ ] Rebuild and verify visually
