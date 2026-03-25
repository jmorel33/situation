# sbgen — Sample Bank ROM Generator
**Version 1.0 (March 2026)** | **Author:** Jacques Morel | **Part of [Polysonix v1.9.36](../README.md)**

A standalone CLI utility and C11 API for packing directories of WAV samples into `.sbr` (Sample Bank ROM) files — a flat, memory-mappable binary format designed for fast sample lookup in synthesizers and samplers.

<details>
<summary>Table of Contents</summary>

- [Overview](#overview)
- [Building](#building)
- [CLI Usage](#cli-usage)
- [WAV Support](#wav-support)
- [SBR File Format](#sbr-file-format)
- [C API](#c-api)
- [API Reference](#api-reference)
- [Name Limits](#name-limits)
- [License](#license)

</details>

## Overview

`sbgen` reads a directory tree where each subdirectory represents a **group** (instrument category, drum machine, etc.) and each `.wav` file within becomes a **sample entry**. The output `.sbr` file contains a header, group table, entry table (TOC), and a packed PCM16 sample data blob — all laid out sequentially for direct memory mapping.

```
MyBank/
  Kicks/
    kick_hard.wav
    kick_soft.wav
  Snares/
    snare_rim.wav
    snare_ghost.wav
```

The API layer (`px_samplebank.h` / `px_samplebank.c`) is fully reusable — you can create, modify, read, and write `.sbr` banks programmatically without the CLI.

## Building

Requires GCC (MinGW/MSYS2 on Windows). From the project root:

```
compile_sbgen.bat
```

Produces `sbgen.exe`. No external dependencies beyond the C11 standard library.

## CLI Usage

### Create a bank

```
sbgen create [--oneshot] <input_dir> <output.sbr>
```

- `input_dir` — root directory containing one subdirectory per group
- `--oneshot` — mark all entries as one-shot (no loop); default is forward loop

```bash
# Looping waveforms (synth oscillators, pads)
sbgen create AKWF akwf_bank.sbr

# One-shot samples (drums, percussion, hits)
sbgen create --oneshot "Drum Machines" drums.sbr
```

### List bank contents

```
sbgen list <file.sbr>
```

Prints every group and entry with sample length, sample rate, channel layout, loop mode, MIDI base note, and fine tune.

```
Sample Bank: drums.sbr
Groups: 200  Entries: 6736  Samples: 348183121

[  0] Acetone Rhythm Ace (28 entries)
      [   0] CLAVE                             8828 smp  44100Hz  stereo 1shot  note=26 ft=2
      [   1] Clave2                           11772 smp  44100Hz  stereo 1shot  note=26 ft=2
      ...
```

## WAV Support

| Format | Channels | Handling |
|--------|----------|----------|
| 16-bit PCM | Mono, Stereo | Imported directly |
| 24-bit PCM | Mono, Stereo | Converted to 16-bit on import (top 16 bits) |
| Compressed, 8-bit, 32-bit float | — | Skipped with diagnostic message |

When a file is skipped, `sbgen` prints a specific reason (compressed, unsupported bit depth, unsupported channel count, etc.) so you know exactly what to fix.

## SBR File Format

All values are little-endian. The file is laid out sequentially:

```
┌─────────────────────────────────┐
│ SBR_Header (20 bytes)           │
├─────────────────────────────────┤
│ SBR_Group[0..group_count-1]     │
├─────────────────────────────────┤
│ SBR_Entry[0..entry_count-1]     │
├─────────────────────────────────┤
│ Sample Data Blob (PCM16)        │
└─────────────────────────────────┘
```

### SBR_Header (20 bytes)

| Field | Type | Description |
|-------|------|-------------|
| `magic` | uint32 | `0x53425200` (`"SBR\0"`) |
| `version` | uint32 | Format version (currently `1`) |
| `group_count` | uint32 | Number of groups |
| `entry_count` | uint32 | Total number of sample entries |
| `data_offset` | uint32 | Byte offset from file start to sample data blob |

### SBR_Group (48 bytes)

| Field | Type | Description |
|-------|------|-------------|
| `name` | char[40] | NUL-terminated group name |
| `first_entry` | uint32 | Index of first entry belonging to this group |
| `entry_count` | uint32 | Number of entries in this group |

### SBR_Entry (64 bytes)

| Field | Type | Description |
|-------|------|-------------|
| `name` | char[40] | NUL-terminated sample name |
| `group_index` | uint32 | Index into group table |
| `sample_begin` | uint32 | Offset in samples from start of data blob |
| `sample_length` | uint32 | Length in samples |
| `loop_start` | uint32 | Loop start, relative to `sample_begin` |
| `loop_end` | uint32 | Loop end, relative to `sample_begin` |
| `sample_rate` | uint32 | Sample rate in Hz |
| `base_note` | uint8 | MIDI note number |
| `fine_tune` | int8 | Cents (−128..+127) |
| `flags` | uint16 | Format bitfield (see below) |

### Flags Bitfield

| Bits | Constant | Value | Meaning |
|------|----------|-------|---------|
| 0 | `SBR_FMT_PCM8` | 0x001 | 8-bit unsigned PCM |
| 1 | `SBR_FMT_PCM12` | 0x002 | 12-bit signed (in 16-bit containers) |
| 2 | `SBR_FMT_PCM16` | 0x004 | 16-bit signed PCM |
| 3 | `SBR_FMT_FLOAT32` | 0x008 | 32-bit IEEE float |
| 4 | `SBR_FMT_MONO` | 0x010 | Mono |
| 5 | `SBR_FMT_STEREO` | 0x020 | Stereo (interleaved L R L R) |
| 6 | `SBR_FMT_LOOP` | 0x040 | Forward loop |
| 7 | `SBR_FMT_PINGPONG` | 0x080 | Ping-pong loop |
| 8 | `SBR_FMT_ONESHOT` | 0x100 | One-shot (no loop) |

Stereo samples are always stored interleaved. There is no planar layout.

## C API

Include `px_samplebank.h` and compile `px_samplebank.c` alongside your project. Pure C11, no dependencies beyond `stdlib`, `stdint`, `string`, and `stdio`.

### Reading an existing bank

```c
#include "px_samplebank.h"

SBR_Bank *bank = sbr_bank_read("my_bank.sbr");

for (int i = 0; i < bank->entry_count; i++) {
    SBR_Entry *e = &bank->entries[i];
    int16_t *pcm = bank->samples + e->sample_begin;
    // e->name, e->sample_length, e->sample_rate, e->flags ...
}

sbr_bank_destroy(bank);
```

### Creating a bank programmatically

```c
SBR_Bank *bank = sbr_bank_create();

int g = sbr_group_add(bank, "Kicks");
sbr_entry_add(bank, g, "kick_hard", pcm_data, num_samples,
              44100, SBR_FMT_PCM16 | SBR_FMT_MONO | SBR_FMT_ONESHOT,
              36, 0);

sbr_bank_write(bank, "output.sbr");
sbr_bank_destroy(bank);
```

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `sbr_bank_create()` | Create an empty in-memory bank |
| `sbr_bank_destroy(bank)` | Free bank and all associated data |

### Groups

| Function | Description |
|----------|-------------|
| `sbr_group_add(bank, name)` | Add a group; returns index (or existing index if name matches) |
| `sbr_group_remove(bank, idx)` | Remove a group and all its entries |
| `sbr_group_rename(bank, idx, name)` | Rename a group |
| `sbr_group_find(bank, name)` | Find group by name; returns index or −1 |

### Entries

| Function | Description |
|----------|-------------|
| `sbr_entry_add(bank, group, name, pcm, len, rate, flags, note, tune)` | Add a sample entry with PCM data |
| `sbr_entry_remove(bank, idx)` | Remove an entry |
| `sbr_entry_rename(bank, idx, name)` | Rename an entry |
| `sbr_entry_set_loop(bank, idx, start, end)` | Set loop points |

### I/O

| Function | Description |
|----------|-------------|
| `sbr_bank_write(bank, path)` | Write bank to `.sbr` file |
| `sbr_bank_read(path)` | Read `.sbr` file into memory; returns `NULL` on failure |

## Name Limits

Group and entry names are capped at 39 characters (40 bytes including NUL terminator). Names that exceed this limit are truncated on import with a warning — they are never skipped.

## License

See [Polysonix LICENSE](../LICENSE).
