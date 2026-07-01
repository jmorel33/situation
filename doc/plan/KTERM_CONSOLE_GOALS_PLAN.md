# KTerm Console Goals — Implementation Plan

**Canonical console:** `examples/console/` — **K-Term core product** (KaOS Terminal), hosted under Situation examples for dev. Entry: `console_host_app.c`; implementation: `console.h` + `console_impl/*.h`. Not Situation core; do not log in `doc/UPDATELOG.md`. See `doc/plan/CONSOLE_MERGE_DEPRECATION_PLAN.md`.

*(Legacy note: older plans refer to monolithic `examples/kterm_console.c`; that code now lives under `examples/console/`.)*

## Current State Summary

- **Shell backend**: `kt_shell.h` uses ConPTY via `CreatePseudoConsole` on Windows. Size is **hardcoded** to `{ 80, 60 }` at start. No resize call exists in the public API.
- **Ctrl+C in shell mode**: Keyboard forwarding already sends `ctrl + rk` as `(char)(rk - 'A' + 1)` → byte `0x03` goes to `KTShell_Write`. ConPTY should relay `CTRL_C_EVENT` to the child. **Needs verification only.**
- **Built-in commands**: `ProcessCommand()` in `console_impl/console_commands.h` handles ~40 commands. Includes `cd`, `ls`, `pwd`, `font`, and VT UI widgets (`vt_box`, `vt_menu`, `vt_styles`).
- **Font selection**: `KTerm_SetFont(term, name)` already exists and works via `available_fonts[]` table (18 fonts). OSC 50 wired to `ProcessFontCommand`. Just needs a `font <name>` CLI command.
- **Scrollback**: Ring buffer with `view_offset`. Shift+PgUp/PgDn partially wired in `KTermSit_ProcessInput` (the non-shell path). Needs testing post-8×8 cell change.
- **Window title**: `SituationSetWindowTitle` exposed via `KTerm_SetWindowTitlePlatform`. The console sets the title at init via `init_info.window_title`. No dynamic updates on mode change or directory change.
- **Win32 shell / PE identity**: library defaults (`sit_app.rc`, `Situation.Application` AppID) — see [Windows app identity](../guide/windows_app_identity.md). Override only when KaOS ships standalone branding.
- **ANSI art**: CP437 charset on G1 already configured. `type <file>` command pipes raw bytes. Basic rendering should work — needs a real `.ANS` file test.
- **VT UI widgets**: `console_impl/console_vt_ui.h` provides DEC/Unicode framed boxes, SGR color styling, and interactive pulldown menus via proper VT escape sequences (see §9).

---

## 1. ConPTY Size Sync

**Problem**: `CreatePseudoConsole` is called with `COORD { 80, 60 }` regardless of the terminal's actual dimensions. When the user resizes the window, `KTerm_Resize` is called but the ConPTY is never informed, so `cmd.exe` wraps at 80 columns even if the terminal is wider.

**Tasks**:

- [x] Add `KTShell_Resize()` declaration to `kt_shell.h` API section
- [x] Implement `KTShell_Resize()` for Windows (calls `ResizePseudoConsole`)
- [x] Implement `KTShell_Resize()` for POSIX (calls `ioctl TIOCSWINSZ`)
- [x] Change `KTShell_Start` signature to accept `int cols, int rows`
- [x] Replace hardcoded `COORD { 80, 60 }` with passed dimensions
- [x] Update `kterm_console.c` call site to pass `term->width, term->height`
- [x] Hook `KTShell_Resize` in the `SituationIsWindowResized()` block in the main loop

### Reference Implementation:

```c
// In the API section (before #ifdef KT_SHELL_IMPLEMENTATION):
void KTShell_Resize(KTShell* shell, int cols, int rows);
```

```c
// Windows implementation:
void KTShell_Resize(KTShell* shell, int cols, int rows) {
    if (!shell || !shell->running || !shell->pty_handle) return;
    COORD size = { (SHORT)cols, (SHORT)rows };
    ResizePseudoConsole((HPCON)shell->pty_handle, size);
}
```

```c
// POSIX implementation:
void KTShell_Resize(KTShell* shell, int cols, int rows) {
    if (!shell || !shell->running) return;
    struct winsize ws = { .ws_row = rows, .ws_col = cols };
    ioctl(shell->master_fd, TIOCSWINSZ, &ws);
}
```

```c
// In kterm_console.c main loop:
if (term && SituationIsWindowResized()) {
    int w, h;
    SituationGetWindowSize(&w, &h);
    int cols = w / (DEFAULT_CHAR_WIDTH * 2);
    int rows = h / (DEFAULT_CHAR_HEIGHT * 2);
    KTerm_Resize(term, cols, rows);
    if (shell_mode) {
        KTShell_Resize(&shell_proc, cols, rows);
    }
}
```

**Files to modify**: `kt_shell.h`, `kterm_console.c`

---

## 2. Test ANSI Art With a Real .ANS File

- [ ] Obtain a `.ANS` file (e.g., from [16colo.rs](https://16colo.rs/) or bundled test art)
- [ ] Run `type <path-to-ans-file>` in the console
- [ ] Verify CP437 block/line characters (bytes 0x80–0xFF) render correctly
- [ ] Verify color/attribute rendering (bold, blink, inverse)
- [ ] Verify line wrapping at 80 columns
- [ ] Check for SAUCE garbage at EOF — if present, implement SUB-byte stripping
- [ ] Add SAUCE stripping to the `type` command handler (stop at `0x1A`)

### SAUCE Strip Fix:

```c
// In the "type" command handler, after fread:
for (size_t i = 0; i < bytes_read; i++) {
    if ((unsigned char)chunk[i] == 0x1A) {
        bytes_read = i;
        break;  // Stop piping — SAUCE metadata follows
    }
}
```

---

## 3. Ctrl+C in Shell Mode

- [ ] Verify: Start `shell` → run `ping -t 127.0.0.1` → press Ctrl+C → confirm it interrupts
- [ ] If not working: investigate `PSEUDOCONSOLE_INHERIT_CURSOR` flag or `GenerateConsoleCtrlEvent`
- [x] Fix Enter key: send only `\r` instead of `\r\n` to ConPTY (ConPTY expects `\r`)

**Current state**: Already partially working:
- Key press `SIT_KEY_C` with Ctrl held → `seq[0] = 0x03` → `KTShell_Write(&shell_proc, seq, 1)`
- ConPTY should translate this into `CTRL_C_EVENT` for the child.

**Potential issue**: The Enter key sends `\r\n` (two bytes) in shell mode. ConPTY typically expects just `\r`.

---

## 4. Font Selection Command

- [x] Add `font` command to `ProcessCommand` in `kterm_console.c`
- [x] `font` (no args) — list available fonts with dimensions
- [x] `font <name>` — call `KTerm_SetFont(term, tokens[1])`
- [x] Add help text entry for the `font` command
- [ ] Decide on font list source: hardcode names in console or expose via API

### Reference Implementation:

```c
} else if (strcmp(cmd, "font") == 0) {
    if (token_count < 2) {
        KTerm_WriteString(term, "\x1B[1;33mAvailable fonts:\x1B[0m\n");
        // Hardcoded list (mirrors kterm_impl.h available_fonts[])
        const char* fonts[] = {
            "VT220    8x10", "IBM     10x10", "VGA      8x8",
            "ULTIMATE 8x16", "CP437_16 8x16", "NEC      8x16",
            "TOSHIBA  8x16", "TRIDENT  8x16", "COMPAQ   8x16",
            "OLYMPIAD 8x16", "MC6847   8x8",  "NEOGEO   8x8",
            "ATASCII  8x8",  "PETSCII  8x8",  "PETSCII_SHIFT 8x8",
            "TOPAZ    8x8",  "PREPPIE  8x8",  "VCR     12x14",
            NULL
        };
        for (int i = 0; fonts[i]; i++)
            KTerm_WriteFormat(term, "  \x1B[36m%s\x1B[0m\n", fonts[i]);
        KTerm_WriteString(term, "\nUsage: \x1B[33mfont <name>\x1B[0m\n");
    } else {
        KTerm_SetFont(term, tokens[1]);
        KTerm_WriteFormat(term, "Font set to: %s\n", tokens[1]);
    }
}
```

**Files to modify**: `kterm_console.c`

---

## 5. Built-in `cd` / `ls` / `pwd`

- [ ] Implement `pwd` command (Win32: `GetCurrentDirectoryA`)
- [ ] Implement `cd <path>` command (Win32: `SetCurrentDirectoryA`)
- [ ] Implement `cd` (no args) — print current directory
- [ ] Implement `ls` / `dir` command (Win32: `FindFirstFileA` / `FindNextFileA`)
- [ ] Color-code directories (blue) vs files
- [ ] Show file sizes in human-readable format
- [ ] Handle path reconstruction for spaces in names
- [ ] Add help text entries for `cd`, `ls`, `pwd`
- [ ] (Optional) Update window title after `cd`

### Reference Implementation:

```c
// pwd
} else if (strcmp(cmd, "pwd") == 0) {
    char cwd[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
        KTerm_WriteFormat(term, "%s\n", cwd);
    } else {
        KTerm_WriteString(term, "\x1B[31mError: Could not get current directory\x1B[0m\n");
    }
}

// cd
} else if (strcmp(cmd, "cd") == 0) {
    if (token_count < 2) {
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd))
            KTerm_WriteFormat(term, "%s\n", cwd);
    } else {
        char path[MAX_PATH];
        path[0] = '\0';
        for (int i = 1; i < token_count; i++) {
            if (i > 1) strcat(path, " ");
            strcat(path, tokens[i]);
        }
        if (!SetCurrentDirectoryA(path)) {
            KTerm_WriteFormat(term, "\x1B[31mcd: no such directory: %s\x1B[0m\n", path);
        }
    }
}

// ls / dir
} else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
    const char* target = (token_count > 1) ? tokens[1] : ".";
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", target);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        KTerm_WriteFormat(term, "\x1B[31mls: cannot access '%s'\x1B[0m\n", target);
    } else {
        do {
            if (strcmp(fd.cFileName, ".") == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                KTerm_WriteFormat(term, "\x1B[1;34m%s/\x1B[0m\n", fd.cFileName);
            } else {
                LARGE_INTEGER filesize;
                filesize.LowPart = fd.nFileSizeLow;
                filesize.HighPart = fd.nFileSizeHigh;
                if (filesize.QuadPart < 1024)
                    KTerm_WriteFormat(term, "  %s  (%lld B)\n", fd.cFileName, filesize.QuadPart);
                else if (filesize.QuadPart < 1024*1024)
                    KTerm_WriteFormat(term, "  %s  (%.1f KB)\n", fd.cFileName, filesize.QuadPart / 1024.0);
                else
                    KTerm_WriteFormat(term, "  %s  (%.1f MB)\n", fd.cFileName, filesize.QuadPart / (1024.0*1024.0));
            }
        } while (FindNextFileA(hFind, &fd));
        FindCloseA(hFind);
    }
}
```

**Files to modify**: `kterm_console.c`  
**Platform note**: Uses Win32 APIs directly. Could wrap with `#ifdef _WIN32` / POSIX fallback if desired.

---

## 6. Scrollback in Built-in Mode

- [x] Test Shift+PgUp increments `view_offset` (scrolls back into history)
- [x] Test Shift+PgDn decrements `view_offset` (scrolls toward live)
- [x] Verify `view_offset` caps at `history_rows_populated`
- [x] Verify any new output resets `view_offset` to 0 (snap to bottom)
- [x] Verify typing while scrolled back snaps to bottom before inserting
- [x] Check rendering correctness after 8×8 cell size change (row count calculation)
- [x] **BUG FIXED**: Cursor now hidden when `view_offset > 0` (was clipping to screen area misleadingly)
- [ ] (If broken) Fix `KTermSit_ProcessInput` Shift+PgUp/PgDn to set correct offset

**Current implementation**: `view_offset` tracks how far back the user is scrolling. `GetScreenRow` subtracts `view_offset` from `screen_head` when rendering.

**Potential issue**: If the console's built-in prompt/redraw logic doesn't account for `view_offset`, typing while scrolled back could display incorrectly.

---

## 7. Window Title Update

- [x] Add `KTerm_SetTitleCallback(term, OnTitleChanged)` after `KTerm_Create` in setup
- [x] Implement `OnTitleChanged` callback — prefixes with "KaOS - " and calls `SituationSetWindowTitle`
- [x] Set title to "KaOS - Shell" when entering shell mode
- [x] Set title to "KaOS - Kaizen Operating System v0.1" when exiting shell mode
- [ ] (Optional) Update title to include current directory after `cd`

### Reference Implementation:

```c
// Callback (file-level static):
static void OnTitleChanged(KTerm* t, const char* title, bool is_icon) {
    (void)t;
    if (!is_icon) {
        char buf[256];
        snprintf(buf, sizeof(buf), "KaOS - %s", title);
        SituationSetWindowTitle(buf);
    }
}

// In setup, after KTerm_Create:
KTerm_SetTitleCallback(term, OnTitleChanged);

// When entering shell mode:
SituationSetWindowTitle("KaOS - Shell (cmd.exe)");

// When exiting shell mode:
SituationSetWindowTitle("KaOS - Kaizen Operating System v0.1");
```

**Files to modify**: `kterm_console.c`

---

## 8. System Information Command (`sysinfo`)

**Goal**: A single, production-viable command that gives a complete snapshot of the machine, OS, running processes, product versions, graphics backend, audio device, and hardware specs. Something you'd screenshot to share specs or use for bug reports.

**Available Situation APIs** (already implemented):
- `SituationGetDeviceInfo()` → `SituationDeviceInfo` struct: CPU name/cores/clock, GPU name/VRAM, RAM total/available, storage devices, network adapters, input devices, displays
- `SituationGetVersionString()` → Situation library version (e.g., "2.4.0")
- `SituationGetGraphicsBackendName()` → "OpenGL 4.6" / "Vulkan 1.4"
- `SituationGetGraphicsCaps()` → MSAA, bindless, compute, shader compiler, max viewports, API version
- `SituationGetGPUName()` → GPU string
- `SituationGetCPUThreadCount()` → logical core count
- `SituationGetAudioDevices(&count)` → list of audio playback devices with names, channels, sample rates
- `SituationGetAudioPlaybackSampleRate()` → current playback rate
- `SituationGetFPS()` → live framerate
- `SituationGetVRAMUsage()` → allocated GPU memory
- `SituationGetDrawCallCount()` → per-frame draw calls
- `KTERM_VERSION_STRING` → KTerm version (e.g., "2.7.15")

**Missing from Situation API** (should be added to the library, not raw Win32 in the console):
- [x] OS name and version — `SituationGetOSInfo()` added (v2.4.199)
- [x] Process enumeration — `SituationGetProcessList()` / `SituationFreeProcessList()` added (v2.4.199)
- [x] Active audio device name — `SituationGetActiveAudioDeviceName()` added (v2.4.199)

### Tasks — Situation Library Side (`situation_api.h` / `situation_impl_io.h`) — DONE ✓

- [x] Add OS info struct `SituationOSInfo`: name, version, build_number
- [x] Implement OS version query: Win32 `RtlGetVersion` (from ntdll), POSIX `uname()` + `/etc/os-release`
- [x] Add `SituationGetProcessList()` API: returns array of `SituationProcessInfo` (PID, name, memory usage)
- [x] Add `SituationFreeProcessList()` to free the returned array
- [x] Implement process enumeration: Win32 `CreateToolhelp32Snapshot` + `Process32First/Next` + `GetProcessMemoryInfo`
- [x] Linux fallback: `/proc` readdir + `/proc/<pid>/comm` + `/proc/<pid>/statm`
- [x] Add `SituationGetActiveAudioDeviceName()` (uses `ma_device_get_info` on active device)
- [x] Declare all new APIs in `situation_api.h`
- [x] Link `-lpsapi` in `build_situation.bat` (OpenGL + Vulkan)
- [x] Bump version to 2.4.199 "System Introspection APIs"
- [x] DLL compiles and links cleanly (GCC 15.1.0)
- [x] Test harness builds and passes (39/39 core tests pass; 1 unrelated GPU test skipped headless)

### Tasks — Console Side (`kterm_console.c`) — DONE ✓

- [x] Add `sysinfo` command to `ProcessCommand`
- [x] Display Situation version (`SituationGetVersionString()`)
- [x] Display KTerm version (`KTERM_VERSION_STRING`)
- [x] Display OS name and version (via new Situation API)
- [x] Display CPU info from `SituationDeviceInfo` (name, cores, clock)
- [x] Display RAM info (total, available) from `SituationDeviceInfo`
- [x] Display GPU info (name, dedicated VRAM) from `SituationDeviceInfo`
- [x] Display graphics backend and caps (`SituationGetGraphicsCaps`)
- [x] Display current VRAM usage (`SituationGetVRAMUsage()`)
- [x] Display audio device(s) — active device name + sample rate (via new Situation API)
- [x] Display connected displays (name, resolution, refresh rate) from `SituationDeviceInfo`
- [x] Display storage devices (name, capacity, free) from `SituationDeviceInfo`
- [x] Display current FPS and draw call count
- [x] Add `ps` / `processes` command — calls `SituationGetProcessList()`, formats as table (OS-level processes)
- [x] Add `threads` / `workers` command — shows internal Situation subsystem threads via `SituationGetThreadPoolSnapshot()`
- [x] Display thread pool status: active workers, I/O thread, render thread, audio thread
- [x] Display per-slot info: role, name, last CPU, NUMA node, active state
- [x] Display job stats: submitted, completed, queue depths, steal counts
- [x] Display audio subsystem state: `SituationIsAudioDevicePlaying()`, sample rate, master volume
- [x] Display init state: `SituationGetInitState()` (READY / INITIALIZING / etc.)
- [ ] (Optional) Add `sysinfo --json` for machine-readable export
- [x] Add help text entries for `sysinfo`, `ps`, `threads`

### Reference Output Format

```
╔══════════════════════════════════════════════════════════════════╗
║                    KaOS System Information                       ║
╠══════════════════════════════════════════════════════════════════╣
║ Situation  : v2.4.0        KTerm  : v2.7.15                     ║
║ OS         : Windows 11 (Build 22631)                           ║
║ Backend    : OpenGL 4.6 (NVIDIA)                                ║
╠══════════════════════════════════════════════════════════════════╣
║ CPU        : AMD Ryzen 9 5900X (24 threads @ 3.70 GHz)         ║
║ RAM        : 12.4 GB free / 32.0 GB total                      ║
║ GPU        : NVIDIA GeForce RTX 3080 (10 GB VRAM)              ║
║ VRAM Used  : 247 MB  |  Draw Calls: 12  |  FPS: 60            ║
╠══════════════════════════════════════════════════════════════════╣
║ Audio      : Focusrite Scarlett 2i2 (48000 Hz, 2ch)            ║
║ Display[0] : DELL U2720Q — 3840×2160 @ 60 Hz                  ║
║ Display[1] : LG 27GL850  — 2560×1440 @ 144 Hz                 ║
╠══════════════════════════════════════════════════════════════════╣
║ Storage[0] : NVMe SSD — 931.5 GB (412.3 GB free)              ║
╚══════════════════════════════════════════════════════════════════╝
```

### Reference Implementation Sketch

```c
} else if (strcmp(cmd, "sysinfo") == 0) {
    SituationDeviceInfo dev = SituationGetDeviceInfo();
    SituationGraphicsCaps caps;
    SituationGetGraphicsCaps(&caps);

    KTerm_WriteString(term, "\n\x1B[1;36m--- KaOS System Information ---\x1B[0m\n\n");

    // Product versions
    KTerm_WriteFormat(term, "  \x1B[33mSituation\x1B[0m : v%s\n", SituationGetVersionString());
    KTerm_WriteFormat(term, "  \x1B[33mKTerm\x1B[0m     : v%s\n", KTERM_VERSION_STRING);

    // OS (Windows-specific)
    #ifdef _WIN32
    // Use RtlGetVersion or GetVersionExW for accurate build info
    #endif

    // Graphics
    KTerm_WriteFormat(term, "  \x1B[33mBackend\x1B[0m   : %s\n", SituationGetGraphicsBackendName());
    KTerm_WriteFormat(term, "  \x1B[33mGPU\x1B[0m       : %s (%llu MB VRAM)\n",
        dev.gpu_name, dev.gpu_dedicated_memory_bytes / (1024*1024));
    KTerm_WriteFormat(term, "  \x1B[33mVRAM Used\x1B[0m : %llu MB  |  Draw Calls: %u  |  FPS: %d\n",
        SituationGetVRAMUsage() / (1024*1024), SituationGetDrawCallCount(), SituationGetFPS());

    // CPU / RAM
    KTerm_WriteFormat(term, "  \x1B[33mCPU\x1B[0m       : %s (%d cores @ %.2f GHz)\n",
        dev.cpu_name, dev.cpu_cores, dev.cpu_clock_speed_ghz);
    KTerm_WriteFormat(term, "  \x1B[33mRAM\x1B[0m       : %.1f GB free / %.1f GB total\n",
        dev.available_ram_bytes / (1024.0*1024*1024),
        dev.total_ram_bytes / (1024.0*1024*1024));

    // Audio
    KTerm_WriteFormat(term, "  \x1B[33mAudio Rate\x1B[0m: %d Hz\n", SituationGetAudioPlaybackSampleRate());

    // Displays
    for (int i = 0; i < dev.display_count; i++) {
        KTerm_WriteFormat(term, "  \x1B[33mDisplay[%d]\x1B[0m: %s — %dx%d @ %d Hz\n",
            i, dev.display_names[i], dev.display_widths[i], dev.display_heights[i], dev.display_refresh_rates[i]);
    }

    // Storage
    for (int i = 0; i < dev.storage_device_count; i++) {
        KTerm_WriteFormat(term, "  \x1B[33mDisk[%d]\x1B[0m   : %s — %.1f GB (%.1f GB free)\n",
            i, dev.storage_device_names[i],
            dev.storage_capacity_bytes[i] / (1024.0*1024*1024),
            dev.storage_free_bytes[i] / (1024.0*1024*1024));
    }

    KTerm_WriteString(term, "\n");
}
```

### Process List (`ps` command) — uses Situation API (OS-level processes)

```c
} else if (strcmp(cmd, "ps") == 0 || strcmp(cmd, "processes") == 0) {
    int count = 0;
    SituationProcessInfo* procs = SituationGetProcessList(&count);
    if (!procs || count == 0) {
        KTerm_WriteString(term, "\x1B[31mError: Could not enumerate processes\x1B[0m\n");
    } else {
        KTerm_WriteString(term, "\x1B[1;33m  PID      Memory     Name\x1B[0m\n");
        for (int i = 0; i < count; i++) {
            if (procs[i].memory_bytes < 1024*1024)
                KTerm_WriteFormat(term, "  %-8u %6.0f KB  %s\n",
                    procs[i].pid, procs[i].memory_bytes / 1024.0, procs[i].name);
            else
                KTerm_WriteFormat(term, "  %-8u %6.1f MB  %s\n",
                    procs[i].pid, procs[i].memory_bytes / (1024.0*1024), procs[i].name);
        }
        SituationFreeProcessList(procs, count);
    }
}
```

### Internal Threads (`threads` command) — uses existing `SituationGetThreadPoolSnapshot()`

This shows the *software's own* threads/subsystems — not OS processes. Already fully supported by the Situation threading API.

```c
} else if (strcmp(cmd, "threads") == 0 || strcmp(cmd, "workers") == 0) {
    SituationThreadPool* pool = SituationGetDefaultThreadPool();
    if (!pool) {
        KTerm_WriteString(term, "\x1B[31mThread pool not active\x1B[0m\n");
    } else {
        SituationThreadPoolSnapshot snap;
        SituationGetThreadPoolSnapshot(pool, &snap);

        KTerm_WriteFormat(term, "\n\x1B[1;36m--- Situation Thread Pool ---\x1B[0m\n");
        KTerm_WriteFormat(term, "  State: %s  |  Workers: %zu  |  Active Jobs: %d\n",
            snap.pool_active ? "\x1B[32mACTIVE\x1B[0m" : "\x1B[31mINACTIVE\x1B[0m",
            snap.worker_count, snap.active_jobs);
        KTerm_WriteFormat(term, "  Queues: Low=%zu  High=%zu  |  Submitted: %llu  Completed: %llu\n",
            snap.low_queue_depth, snap.high_queue_depth,
            snap.stats_jobs_submitted, snap.stats_jobs_completed);
        KTerm_WriteString(term, "\n\x1B[1;33m  Role            Name                  CPU  NUMA  Active\x1B[0m\n");
        for (int i = 0; i < snap.slot_count; i++) {
            const char* role_str = "???";
            switch (snap.slots[i].role) {
                case SIT_THREAD_ROLE_WORKER: role_str = "Worker"; break;
                case SIT_THREAD_ROLE_IO:     role_str = "I/O"; break;
                case SIT_THREAD_ROLE_RENDER: role_str = "Render"; break;
                case SIT_THREAD_ROLE_AUDIO:  role_str = "Audio"; break;
                case SIT_THREAD_ROLE_MAIN:   role_str = "Main"; break;
                default: break;
            }
            KTerm_WriteFormat(term, "  %-14s  %-20s  %3d  %4d  %s\n",
                role_str, snap.slots[i].name,
                snap.slots[i].last_logical_cpu,
                snap.slots[i].numa_node,
                snap.slots[i].active ? "\x1B[32mYES\x1B[0m" : "\x1B[90mno\x1B[0m");
        }

        // Audio subsystem status
        KTerm_WriteFormat(term, "\n  \x1B[33mAudio\x1B[0m: %s  |  Rate: %d Hz  |  Vol: %.0f%%\n",
            SituationIsAudioDevicePlaying() ? "\x1B[32mPlaying\x1B[0m" : "\x1B[90mStopped\x1B[0m",
            SituationGetAudioPlaybackSampleRate(),
            SituationGetAudioMasterVolume() * 100.0f);

        // Init state
        const char* state_str = "Unknown";
        switch (SituationGetInitState()) {
            case SITUATION_STATE_UNINITIALIZED: state_str = "UNINITIALIZED"; break;
            case SITUATION_STATE_INITIALIZING:  state_str = "INITIALIZING"; break;
            case SITUATION_STATE_READY:         state_str = "\x1B[32mREADY\x1B[0m"; break;
            case SITUATION_STATE_SHUTTING_DOWN: state_str = "SHUTTING DOWN"; break;
        }
        KTerm_WriteFormat(term, "  \x1B[33mState\x1B[0m: %s\n\n", state_str);
    }
}
```

### Proposed Situation API additions (`situation_api.h`)

```c
// --- OS Information ---
typedef struct {
    char os_name[64];           // e.g., "Windows 11", "Ubuntu 24.04", "macOS Sequoia"
    char os_version[64];        // e.g., "10.0.22631", "6.8.0-45-generic"
    uint32_t build_number;      // Windows build number (0 on other platforms)
} SituationOSInfo;

SITAPI SituationOSInfo SituationGetOSInfo(void);

// --- Process Enumeration ---
typedef struct {
    uint32_t pid;
    char name[260];             // Process executable name
    uint64_t memory_bytes;      // Working set / RSS
} SituationProcessInfo;

SITAPI SituationProcessInfo* SituationGetProcessList(int* out_count);   // Caller must free with SituationFreeProcessList.
SITAPI void SituationFreeProcessList(SituationProcessInfo* list, int count);

// --- Active Audio Device ---
SITAPI const char* SituationGetActiveAudioDeviceName(void);  // Returns name of currently bound playback device (static buffer, do not free).
```

**Files to modify (library)**: `sit/situation_api.h`, `sit/situation_impl_io.h`  
**Files to modify (console)**: `examples/console/console_impl/console_commands.h` (and related `console_impl/*.h`)  
**Headers needed internally**: `<tlhelp32.h>` (Win32), `<dirent.h>` (POSIX /proc)

---

## 9. VT UI Tooling (Boxes & Pulldown Menus)

**Goal**: Reusable console-side helpers for drawing framed UI (boxes, menus) using **proper VT byte streams** — not direct screen-buffer writes or raw CP437 shortcuts where DEC/SGR equivalents exist. Enables future built-in dialogs, pickers, and chrome consistent with K-Term's emulation model.

**Status**: **DONE ✓** (initial implementation)

**Module**: `examples/console/console_impl/console_vt_ui.h`  
**Wired via**: `console_impl.h` (include order), `console_response.h` (modal input), `console_lifecycle.h` (suppress prompt while menu active), `console_commands.h` (CLI demos), `console_cli_edit.h` (tab completion)

### Design Principles

| Concern | VT approach (not buffer pokes) |
|---------|--------------------------------|
| **Positioning** | CUP `ESC[row;colH`, DECSC/DECRC `ESC[s` / `ESC[u` |
| **Line drawing** | DEC Special Graphics `ESC(0` … `ESC(B` with `l/k/m/j/q/x` corners and lines |
| **Unicode frames** | UTF-8 box-drawing block elements (`U+2500` light, `U+2550` heavy) |
| **Legacy double lines** | CP437 bytes (`0xC9`/`0xBB`/…) where double-line DEC is unavailable |
| **Colors** | SGR: ANSI 16/bright, 256-color `38;5;n` / `48;5;n`, truecolor `38;2;r;g;b` |
| **Attributes** | SGR bold (1), dim (2), italic (3), underline (4), inverse (7), blink (5) |
| **Clearing** | EL `ESC[K` per row when dismissing overlays |
| **Cursor** | DECTCEM `ESC[?25l/h` hide/show during widget paint |
| **Menus (mouse)** | SGR mouse reports (`CSI ?1003h`, `CSI ?1006h` — enabled at `Console_Init`) |

Public K-Term write APIs consume VT streams; see `sit/k-term/doc/kterm.md` § Character Sets (DEC Special Graphics vs UTF-8 vs CP437 internal atlas).

### Core Types

```c
ConsoleVtBorderStyle   // dec, dec_double, unicode light/heavy, ascii
ConsoleVtStyle         // fg/bg ANSI, 256, truecolor + attribute flags
ConsoleVtBoxParams     // row, col, width, height, border, fill, title_text
ConsoleVtMenuState     // modal pulldown state (internal)
```

### Public Helpers (static, console-internal)

| Function | Purpose |
|----------|---------|
| `ConsoleVtEmitStyle` / `ConsoleVtResetAttributes` | Build and emit SGR sequences |
| `ConsoleVtCup` | Cursor positioning |
| `ConsoleVtBeginDecLineDraw` / `ConsoleVtEndDecLineDraw` | `ESC(0` / `ESC(B` wrappers |
| `ConsoleVtDrawBox` | Framed rectangle with optional interior fill and title |
| `ConsoleVtClearRegion` | Erase overlay area on dismiss |
| `ConsoleVtMenuShow` | Open modal pulldown at `(row, col)` |
| `ConsoleVtMenuHandleResponse` | Consume keyboard/mouse while menu active |
| `ConsoleVtMenuIsActive` | Query modal state (lifecycle / prompt gating) |
| `ConsoleVtDemoStyles` | Style gallery for `vt_styles` command |

Style tokens parsed from CLI strings: `cyan`, `bold`, `inverse`, `256:N`, `rgb:R,G,B`, `bg256:N`, `bgrgb:R,G,B`, `hi:…` (highlight-only on menus).

### CLI Commands

| Command | Description |
|---------|-------------|
| `vt_styles` / `vt_demo` | Gallery of boxes: DEC single, Unicode light, CP437 double, 256-color fill, truecolor |
| `vt_box [row col w h] [border] [style tokens…]` | Draw one framed box. Borders: `dec`, `unicode`, `heavy`, `double`, `ascii`. Optional `fill`, `title:Label` |
| `vt_menu <row> <col> <item> [items…] [style tokens…]` | Interactive pulldown. ↑/↓, Enter, Esc, mouse click; prints selection then restores prompt |
| `vt_combo <row> <col> <item> [items…]` | Collapsed field with ▼; Enter/Space/Down opens list; Esc collapses |
| `vt_dialog <row> <col> <width> <message>` | OK/Cancel dialog; ←/→ or Tab to focus, Enter to confirm |
| `vt_styles_capture` | Runs style gallery and resets screenshot frame counter (requires `KTERM_CAPTURE_SCREENSHOT`) |

**Examples**:

```
vt_box 5 10 30 8 dec cyan bold
vt_box 5 10 24 6 unicode fill bg256:17 yellow title:Options
vt_box 8 8 20 4 ascii rgb:255,128,64 bgrgb:32,16,48
vt_menu 12 20 Red Green Blue Yellow hi:inverse
```

Documented in `help 2` under **VT UI Widgets**.

### Input Integration

When `ConsoleVtMenuIsActive()`:

1. `HandleKTermResponse` routes bytes to `ConsoleVtMenuHandleResponse` **before** the CLI line editor.
2. Arrow keys move highlight; Enter confirms; Esc / Ctrl+C cancels; SGR mouse click selects row or dismisses on outside click.
3. Main loop skips `ShowPrompt()` until menu completes (`console_lifecycle.h`).

Pattern mirrors the embedded editor (`console_ed.h`) — modal overlay, response-driven, no blocking loop.

### Tasks — Console Side — DONE ✓

- [x] Add `console_vt_ui.h` with box drawing and SGR style builder
- [x] Support DEC Special Graphics single-line borders
- [x] Support Unicode light/heavy and ASCII fallback borders
- [x] Support CP437 double-line border style
- [x] Support ANSI / 256-color / truecolor foreground and background
- [x] Add `vt_box`, `vt_menu`, `vt_styles` commands
- [x] Wire modal menu into `HandleKTermResponse`
- [x] Suppress prompt while menu active
- [x] Tab completion and help text entries

### Follow-ups (Optional)

- [x] Collapsed “combo box” chrome (closed field + ▼ glyph) before expand — `vt_combo`
- [x] `DECSTBM` scroll-region save/restore around modal widgets
- [x] DECSCA protected rows for widget border chrome (`ESC[1 q` / `ESC[0 q`)
- [x] Reusable dialog with OK/Cancel button row — `vt_dialog`
- [ ] Export minimal public API on `console_api.h` if other examples need widgets *(deferred — no cross-example consumer yet)*
- [x] Screenshot hook via `vt_styles_capture` + existing `KTERM_CAPTURE_SCREENSHOT` env var

**Capture usage** (no library changes):

```powershell
$env:KTERM_CAPTURE_SCREENSHOT = "vt_styles.png"
$env:KTERM_CAPTURE_EXIT = "1"   # optional
# run console, then: vt_styles_capture
```

**Files**: `examples/console/console_impl/console_vt_ui.h`, `console_response.h`, `console_lifecycle.h`, `console_commands.h`, `console_cli_edit.h`, `console_forward.h`, `console_impl.h`

---

## Priority Order (Suggested)

| # | Feature | Effort | Value |
|---|---------|--------|-------|
| 1 | ConPTY size sync | Medium | High — fixes wrong wrapping |
| 2 | `font <name>` command | Small | Quick QoL win |
| 3 | `cd`/`ls`/`pwd` | Small | Self-sufficient prompt |
| 4 | `sysinfo` / `ps` commands | Medium | Production-grade diagnostics |
| 5 | Window title update | Small | Polish |
| 6 | Ctrl+C verification | Test only | Confirm working |
| 7 | ANSI art test | Test + small fix | SAUCE stripping |
| 8 | Scrollback test | Test only | Verify post-cell-change |
| 9 | VT UI tooling (boxes/menus) | Medium | Widget foundation for CLI chrome |

---

## Files Summary

| File | Changes |
|------|---------|
| `sit/situation_api.h` | Add `SituationOSInfo`, `SituationProcessInfo`, `SituationGetOSInfo()`, `SituationGetProcessList()`, `SituationFreeProcessList()`, `SituationGetActiveAudioDeviceName()` |
| `sit/situation_impl_io.h` | Implement OS version query, process enumeration, active audio device name |
| `sit/k-term/kt_shell.h` | Add `KTShell_Resize()`, modify `KTShell_Start` signature for cols/rows |
| `examples/console/console_host_app.c` | Situation init + main loop |
| `examples/console/console_impl/*.h` | Modular CLI: commands, lifecycle, response routing, editor, **VT UI** |
| `examples/console/console_impl/console_vt_ui.h` | **VT box drawing, SGR styles, pulldown menus** (`vt_box`, `vt_menu`, `vt_styles`) |
| `examples/console/console_impl/console_commands.h` | Built-in commands (`font`, `cd`, `ls`, `pwd`, `sysinfo`, `ps`, VT UI, …) |
