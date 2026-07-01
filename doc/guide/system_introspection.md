## System Introspection Module _(v2.4.199)_

**Overview:** Platform-abstracted APIs for querying **operating system identity**, **enumerating running processes**, and reading the **currently active audio playback device**. These are lightweight OS snapshots — useful for about screens, debug overlays, telemetry, and tooling.

**Scope note:** This module covers the v2.4.199 trio only. Broader hardware introspection lives elsewhere:

| Need | Where |
|------|-------|
| CPU, GPU, RAM, storage, network, input devices | [Core Module — System & Hardware Information](core.md) (`SituationGetCPUInfo`, `SituationGetGPUInfo`, `SituationGetMemoryInfo`, …) |
| Thread pool / worker placement | [Threading Module](threading.md) (`SituationGetThreadingStatus`, `SituationGetThreadPoolSnapshot`) |
| List all audio devices (not just active) | [Audio Module](audio.md) (`SituationEnumerateAudioDevices`, device switching) |

**Canonical examples:**
- `examples/01_open_a_window/` — startup banner with OS + CPU + GPU + RAM
- `examples/console/` — `sysinfo` and `ps` shell commands (`console_commands.h`)

**Not for security decisions:** Process lists and device names are snapshots that can change between frames. Do not use them for access control or anti-cheat.

---

### API Summary

| Function | Init required? | Platforms | Memory |
|----------|----------------|-----------|--------|
| `SituationGetOSInfo` | No | Windows, Linux, macOS | Stack struct (by value) |
| `SituationGetProcessList` | No | **Windows, Linux only** | Heap array → `SituationFreeProcessList` |
| `SituationGetActiveAudioDeviceName` | **Yes** (+ audio active) | All (via miniaudio) | Static internal buffer — **do not free** |

---

### Structs

#### `SituationOSInfo`

```c
typedef struct SituationOSInfo {
    char name[64];           // Product name ("Windows 11", "Ubuntu 24.04", "macOS")
    char version[64];        // Version string ("10.0.22631", "6.8.0-41-generic", "24.4.0")
    uint32_t build_number;   // Windows build number; 0 on Linux/macOS
} SituationOSInfo;
```

#### `SituationProcessInfo`

```c
#define SITUATION_MAX_PROCESS_NAME_LEN 260

typedef struct SituationProcessInfo {
    uint32_t pid;                                   // Process ID
    char name[SITUATION_MAX_PROCESS_NAME_LEN];      // Executable name
    uint64_t memory_bytes;                          // Resident memory (see platform notes)
} SituationProcessInfo;
```

---

### `SituationGetOSInfo`

Returns operating system name, version string, and build number. **Context-free** — callable before `SituationInit()`.

```c
SituationOSInfo SituationGetOSInfo(void);
```

**Usage — about screen** (from `01_open_a_window`):

```c
SituationOSInfo os = SituationGetOSInfo();
printf("OS: %s %s (build %u)\n", os.name, os.version, os.build_number);
```

Combine with hardware queries from [Core](core.md):

```c
SituationCPUInfo cpu = {0};
SituationGPUInfo gpu = {0};
SituationMemoryInfo mem = {0};
SituationGetCPUInfo(&cpu);      /* requires SituationInit */
SituationGetGPUInfo(&gpu);
SituationGetMemoryInfo(&mem);
SituationOSInfo os = SituationGetOSInfo();   /* no init required */
```

**Platform behavior:**

| Platform | `name` | `version` | `build_number` |
|----------|--------|-----------|----------------|
| **Windows** | Win7/8/8.1/10/11 detected via `RtlGetVersion` (no compatibility shim) | `major.minor.build` | NT build number (e.g. 22631) |
| **Linux** | `PRETTY_NAME` from `/etc/os-release`, fallback `"Linux"` | Kernel release from `uname()` | Always `0` |
| **macOS** | `"macOS"` | `kern.osrelease` via `sysctlbyname` | Always `0` |

On failure paths, `name` may be `"Windows (version unknown)"` or `"Unknown OS"` with zeroed fields.

---

### `SituationGetProcessList` / `SituationFreeProcessList`

Returns a **point-in-time snapshot** of running OS processes. Caller owns the returned array.

```c
SituationProcessInfo* SituationGetProcessList(int* out_count);
void SituationFreeProcessList(SituationProcessInfo* list, int count);
```

**Parameters:**
- `out_count` — receives process count (required; returns `NULL` if `NULL`)
- `count` in `FreeProcessList` — passed for API symmetry; **currently ignored** by the implementation (only `list` is freed)

**Returns:** Heap-allocated array, or `NULL` on failure. Check `SituationGetLastErrorCode()` after failure.

**Usage — task-manager style list:**

```c
int count = 0;
SituationProcessInfo* procs = SituationGetProcessList(&count);
if (procs) {
    for (int i = 0; i < count; i++) {
        if (procs[i].name[0] == '\0') continue;
        printf("[%u] %-24s %6.1f MB\n",
               procs[i].pid, procs[i].name,
               procs[i].memory_bytes / (1024.0 * 1024.0));
    }
    SituationFreeProcessList(procs, count);
}
```

**Platform behavior:**

| Platform | Source | Process name | Memory metric |
|----------|--------|--------------|---------------|
| **Windows** | `CreateToolhelp32Snapshot` + `Process32First/Next` | `szExeFile` | Working set via `GetProcessMemoryInfo` (0 if `OpenProcess` denied) |
| **Linux** | `/proc/<pid>/comm` + `/proc/<pid>/statm` | `comm` (may truncate long names) | RSS pages × page size |
| **macOS** | **Not implemented** | — | Returns `NULL` |

**Important semantics:**
- **Unsorted** — order follows OS enumeration, not memory or name. Sort client-side if needed.
- **Snapshot** — PIDs may exit before you read the list; stale entries are possible on busy systems.
- **`memory_bytes == 0`** — often means permission denied (Windows protected processes) or unreadable `/proc` entry, not necessarily zero usage.
- **No init required** — works before `SituationInit()`.
- **macOS gap** — use platform APIs directly if you need process enumeration on macOS today.

**Reference implementation:** `examples/console/console_impl/console_commands.h` — `ps` / `processes` command prints PID, memory, name with KB/MB formatting and skips empty entries.

---

### `SituationGetActiveAudioDeviceName`

Returns the name of the **currently bound playback device** (the miniaudio device Situation opened at init). Not a full device enumeration — see [Audio Module](audio.md) for listing and switching devices.

```c
const char* SituationGetActiveAudioDeviceName(void);
```

**Requirements:**
1. `SituationInit()` completed successfully
2. Audio subsystem active (`sit_audio.is_miniaudio_device_active`)

**Return value:** Pointer to a **static internal buffer** (256 bytes). Do not free. Valid until the next call to this function (which overwrites the buffer).

**Sentinel strings** (not device names — treat as status, not user-facing labels):

| Return | Meaning |
|--------|---------|
| `"Not initialized"` | Called before `SituationInit()` |
| `"No audio device"` | Init done but playback device not active |
| `"Default Playback Device"` | Device active but name unavailable |
| *(other)* | Real device name from `ma_device_get_info` (WASAPI / PulseAudio / ALSA) |

**Usage:**

```c
const char* dev = SituationGetActiveAudioDeviceName();
if (dev && strcmp(dev, "Not initialized") != 0 && strcmp(dev, "No audio device") != 0) {
    printf("Audio output: %s\n", dev);
}
```

For a settings screen that lets users **pick** a device, enumerate with `SituationEnumerateAudioDevices()` and switch with `SituationSetAudioDevice()` — see [Audio Module](audio.md).

---

### Integration Patterns

#### A — Startup system banner (`01_open_a_window`)

Print once after init — OS from this module, hardware from Core:

```c
SituationOSInfo os = SituationGetOSInfo();
SituationCPUInfo cpu = {0};
SituationGPUInfo gpu = {0};
SituationMemoryInfo mem = {0};
SituationGetCPUInfo(&cpu);
SituationGetGPUInfo(&gpu);
SituationGetMemoryInfo(&mem);

printf("OS  : %s %s (build %u)\n", os.name, os.version, os.build_number);
printf("CPU : %s (%u cores / %u threads)\n", cpu.name, cpu.core_count, cpu.thread_count);
printf("GPU : %s\n", gpu.name);
printf("RAM : %.1f GB free / %.1f GB total\n",
       mem.available_bytes / (1024.0*1024.0*1024.0),
       mem.total_bytes / (1024.0*1024.0*1024.0));
```

#### B — In-game debug overlay

Poll sparingly (e.g. once per second, not every frame):

```c
/* OS: static — query once at startup */
static SituationOSInfo g_os = {0};
static int g_os_queried = 0;

void draw_debug_overlay(SituationCommandBuffer cmd) {
    if (!g_os_queried) { g_os = SituationGetOSInfo(); g_os_queried = 1; }

    const char* audio = SituationGetActiveAudioDeviceName();
    /* Draw g_os.name, audio via SituationCmdDrawTextEx ... */
}
```

Process lists are expensive — refresh on user action (open task overlay), not every frame.

#### C — Console `sysinfo` + `ps` (`examples/console`)

The bundled KaOS console combines Core hardware queries with this module:

- **`sysinfo`** — bordered panel: Situation version, init state, OS, CPU, RAM
- **`ps`** / **`processes`** — full process table via `SituationGetProcessList`

Useful reference when building your own terminal or debug shell.

#### D — Telemetry blob

```c
typedef struct {
    SituationOSInfo os;
    char audio_device[256];
    int process_count;
} TelemetrySnapshot;

TelemetrySnapshot snap = {0};
snap.os = SituationGetOSInfo();
const char* aud = SituationGetActiveAudioDeviceName();
if (aud) strncpy(snap.audio_device, aud, sizeof snap.audio_device - 1);

SituationProcessInfo* procs = SituationGetProcessList(&snap.process_count);
/* serialize snap + top-N processes by memory */
if (procs) SituationFreeProcessList(procs, snap.process_count);
```

---

### Relationship to Deprecated APIs

`SituationGetDeviceInfo()` (deprecated v2.4.336) aggregated CPU, GPU, RAM, monitors, and more in one struct. Prefer the split queries:

```c
/* Old (still works, deprecated) */
SituationDeviceInfo dev = SituationGetDeviceInfo();

/* New — compose yourself */
SituationGetCPUInfo(&cpu);
SituationGetGPUInfo(&gpu);
SituationGetMemoryInfo(&mem);
SituationOSInfo os = SituationGetOSInfo();
```

See [Deprecated APIs](deprecated.md) for the full migration table.

---

### API Reference

---
#### `SituationGetOSInfo`
OS name, version, build. No init required.

```c
SituationOSInfo SituationGetOSInfo(void);
```

---
#### `SituationGetProcessList`
Heap-allocated process snapshot. Windows and Linux only.

```c
SituationProcessInfo* SituationGetProcessList(int* out_count);
```

---
#### `SituationFreeProcessList`
Free array from `SituationGetProcessList`.

```c
void SituationFreeProcessList(SituationProcessInfo* list, int count);
```

---
#### `SituationGetActiveAudioDeviceName`
Active playback device name. Requires init + active audio. Static buffer.

```c
const char* SituationGetActiveAudioDeviceName(void);
```

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `GetProcessList` returns NULL on macOS | Not implemented | Use platform APIs or file Linux/Windows-only |
| Many processes show 0 MB | Windows permission denied | Expected for system/protected PIDs |
| `GetActiveAudioDeviceName` → `"Not initialized"` | Before `SituationInit` | Init first, or guard the call |
| → `"No audio device"` | Audio disabled or init failed | Check audio init flags; verify miniaudio backend |
| OS name `"Unknown OS"` | Unsupported platform or parse failure | Check platform defines |
| `build_number` always 0 | Linux/macOS | Normal — only Windows populates it |
| Process count differs from Task Manager | Different metric (RSS vs working set) | Document metric in UI |
| Memory leak | Forgot `FreeProcessList` | Always pair get/free |
| Expected CPU/GPU APIs here | Wrong module | See [Core](core.md) split queries |

---

### Performance Tips

- **`SituationGetOSInfo`** — cheap; cache at startup.
- **`SituationGetProcessList`** — allocates O(processes); can be slow on machines with hundreds of PIDs. Poll on demand, not every frame.
- **`SituationGetActiveAudioDeviceName`** — cheap once audio is up; safe to call each frame if needed, but caching is fine.
- For **full hardware panels**, prefer Core's split queries — they require init but avoid the deprecated monolithic struct.
