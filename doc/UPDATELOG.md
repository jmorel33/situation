# Situation UPDATELOG

Release history in archive files.

- **v2.4.x:** split across `updatelog_24_*.md` (table lists **newest file first**).
- **v2.3.x:** single file [`updatelog_23.md`](updatelog_23.md) (not split).

Within each archive file, entries are **oldest first** (top → bottom). For full v2.4 history, read files in order: `_01` → `_02` → `_03` → `_04` → `_05`.

**Current version:** v2.4.407 — narrative summary in [`whatsnew.md`](whatsnew.md).

---

## v2.4.x

| File | Range (oldest → newest) | Patches |
|------|-------------------------|--------:|
| [updatelog_24_05.md](updatelog_24_05.md) | 2.4.401 "Profiling File Layout" – 2.4.407 "Grid Subsystem Phase E" | 7 |
| [updatelog_24_04.md](updatelog_24_04.md) | 2.4.301 – 2.4.400 "Win32 Identity WI-5 — Plan Complete" | 84 |
| [updatelog_24_03.md](updatelog_24_03.md) | 2.4.201 "Error Propagation Phase 0" – 2.4.300 | 98 |
| [updatelog_24_02.md](updatelog_24_02.md) | 2.4.101 "SPIR-V Driver Log Capture" – 2.4.200 "API Documentation Refresh" | 100 |
| [updatelog_24_01.md](updatelog_24_01.md) | 2.4.0 "Modular Revolution & Architectural Reorganization" – 2.4.100 "SPIR-V Poll Diagnostics" | 100 |

## v2.3.x archive

| File | Range (oldest → newest) | Patches |
|------|-------------------------|--------:|
| [updatelog_23.md](updatelog_23.md) | 2.3.1 "Base" – 2.3.64 "Registry Phase 1" | 113 |

---

## Adding a new patch

1. Append the release block to the bottom of [`updatelog_24_05.md`](updatelog_24_05.md).
2. When patch numbers exceed **2.4.500**, add `updatelog_24_06.md` (extend `scripts/split_updatelog_chunks.py`).
3. Update this index (or let the script regenerate it).

## Maintenance

```bat
python scripts\split_updatelog_chunks.py
```

Regenerates v2.4 archive files (`updatelog_24_01.md` … `_05.md` by patch band) and the v2.3 single archive.

