## Logging Module

**Overview:** Situation provides a small, printf-style logging layer with severity levels, ANSI color labels, optional custom callbacks, and integration with the global error buffer. Use it for startup diagnostics, shader load traces, gameplay debug output, and pairing with `SituationGetLastErrorMsg()` after API failures.

**When to use it:**

- **Development:** raise verbosity with `SituationSetTraceLogLevel(SIT_LOG_DEBUG)` or `SIT_LOG_ALL` to see subsystem traces.
- **Release / shipped builds:** lower to `SIT_LOG_WARNING` or `SIT_LOG_ERROR` to cut noise; `SIT_LOG_NONE` silences all `SituationLog` output.
- **Telemetry / IDE integration:** redirect everything through `SituationSetLogCallback` after `SituationInit`.
- **After any `SituationError` failure:** log context with `SituationLog(SIT_LOG_ERROR, ...)` plus `SituationGetLastErrorMsg()`.

**Related:** [Core — error handling](core.md#situationgetlasterrormsg) · [System introspection](system_introspection.md) · [Examples FAQ — troubleshooting](examples_faq.md#frequently-asked-questions-faq-troubleshooting)

---

### Log levels

Defined in `sit/situation_api_platform.h` as `SituationLogLevel`:

| Level | Constant | Numeric | Typical use |
|-------|----------|---------|-------------|
| All | `SIT_LOG_ALL` | 0 | Show every severity (development default for deep debugging) |
| Trace | `SIT_LOG_TRACE` | 1 | Step-by-step subsystem traces |
| Debug | `SIT_LOG_DEBUG` | 2 | State dumps, load progress, internal diagnostics |
| Info | `SIT_LOG_INFO` | 3 | Normal operational messages (**library default threshold**) |
| Warning | `SIT_LOG_WARNING` | 4 | Recoverable problems |
| Error | `SIT_LOG_ERROR` | 5 | Failures that affect behavior |
| Fatal | `SIT_LOG_FATAL` | 6 | Unrecoverable errors |
| None | `SIT_LOG_NONE` | 8 | Threshold that suppresses all `SituationLog` output |

**Filtering rule:** A message is printed only when `msgType >= SituationSetTraceLogLevel(...)`. Lower-severity messages are dropped silently.

Examples:

| Threshold | Shown | Hidden |
|-----------|-------|--------|
| `SIT_LOG_ALL` (0) | Everything | — |
| `SIT_LOG_INFO` (3) | Info, Warning, Error, Fatal | Trace, Debug |
| `SIT_LOG_WARNING` (4) | Warning, Error, Fatal | Trace, Debug, Info |
| `SIT_LOG_NONE` (8) | Nothing | All |

**Default:** `_sit_trace_log_level` starts at `SIT_LOG_INFO` — trace and debug lines from the library are filtered until you lower the threshold.

---

### Output format

When no custom callback is registered, `SituationLog` writes to **stdout** with ANSI colors:

```
[INFO] Player reached a score of 100
[WARN] Texture path not found: assets/missing.png
[ERROR] Shader compile failed
```

| Level | Label | Color |
|-------|-------|-------|
| Trace | `[TRACE]` | Gray |
| Debug | `[DEBUG]` | Cyan |
| Info | `[INFO]` | Green |
| Warning | `[WARN]` | Yellow |
| Error | `[ERROR]` | Red |
| Fatal | `[FATAL]` | Red background |

On Windows terminals that support VT sequences, colors appear automatically. Redirect stdout to a file if you need plain text logs.

---

### Logging vs error codes

Situation uses **two complementary channels**:

| Mechanism | Purpose | API |
|-----------|---------|-----|
| **Return codes** | Programmatic failure handling | `SituationError` return values, `SituationGetLastErrorCode()` |
| **Detail strings** | Human-readable compiler/driver text | `SituationGetLastErrorMsg()` |
| **Logging** | Optional diagnostic chatter | `SituationLog`, `SituationSetTraceLogLevel` |

**Recommended failure pattern:**

```c
SituationShader shader = {0};
if (SituationLoadShaderFromMemory(vs, fs, &shader) != SITUATION_SUCCESS) {
    char* detail = NULL;
    SituationGetLastErrorMsg(&detail);
    SituationLog(SIT_LOG_ERROR, "Shader load failed: %s",
                 detail ? detail : "(no detail)");
    if (detail) free(detail);
    return -1;
}
```

The library itself logs warnings in many subsystems (renderer, audio graph, model loader). Your app can use the same API for consistent formatting.

---

### Typical workflows

#### Verbose development session

```c
SituationInit(argc, argv, &init_info);
SituationSetTraceLogLevel(SIT_LOG_DEBUG);   /* or SIT_LOG_ALL */

SituationLog(SIT_LOG_INFO, "Starting %s", init_info.app_name);
```

#### Release build — warnings and errors only

```c
#ifdef NDEBUG
    SituationSetTraceLogLevel(SIT_LOG_WARNING);
#else
    SituationSetTraceLogLevel(SIT_LOG_DEBUG);
#endif
```

#### Silence library chatter (console host pattern)

`examples/console/console_host_app.c` sets `SIT_LOG_NONE` when embedding Situation inside another process that owns stdout:

```c
SituationSetTraceLogLevel(SIT_LOG_NONE);
```

#### Custom log sink (file, IDE, telemetry)

Register **after** `SituationInit` — the callback is stored in global state and requires an active context:

```c
static void my_log_sink(SituationLogLevel level, const char* message, void* user) {
    FILE* f = (FILE*)user;
    const char* tag = "INFO";
    switch (level) {
        case SIT_LOG_TRACE: tag = "TRACE"; break;
        case SIT_LOG_DEBUG: tag = "DEBUG"; break;
        case SIT_LOG_INFO:  tag = "INFO";  break;
        case SIT_LOG_WARNING: tag = "WARN"; break;
        case SIT_LOG_ERROR: tag = "ERROR"; break;
        case SIT_LOG_FATAL: tag = "FATAL"; break;
        default: break;
    }
    fprintf(f, "[%s] %s\n", tag, message);
    fflush(f);
}

/* after SituationInit: */
SituationSetLogCallback(my_log_sink, log_file);
```

When a callback is set, **default colored stdout output is skipped** — your callback receives the fully formatted message string only (no level prefix added by the library; you choose how to tag it).

Pass `NULL` for the callback pointer to clear and restore default printing (call after init with callback `NULL`).

---

### Functions

---
#### `SituationLog`

Prints a formatted message at the given level. Uses `printf`-style variadic arguments. Messages below the current trace threshold are ignored.

```c
void SituationLog(int msgType, const char* text, ...);
```

**Usage:**

```c
int score = 100;
SituationLog(SIT_LOG_INFO, "Player reached a score of %d", score);

if (score > 9000) {
    SituationLog(SIT_LOG_WARNING, "Score is over 9000!");
}
```

**Notes:**

- Buffer limit: 4096 bytes per message (internal `vsnprintf`).
- Thread safety: safe to call from the main thread; avoid calling from audio callback unless your sink is re-entrant.
- Does **not** set `SituationGetLastErrorMsg()` — use return codes for that.

---
#### `SituationSetTraceLogLevel`

Sets the minimum severity that `SituationLog` will emit. Takes effect immediately for all subsequent log calls.

```c
void SituationSetTraceLogLevel(int logType);
```

**Usage:**

```c
SituationSetTraceLogLevel(SIT_LOG_ALL);      /* development */
SituationSetTraceLogLevel(SIT_LOG_WARNING);  /* production */
SituationSetTraceLogLevel(SIT_LOG_NONE);     /* silent */
```

---
#### `SituationSetLogCallback`

Redirects all `SituationLog` output to a user function. Requires `SituationInit` to have succeeded.

```c
void SituationSetLogCallback(
    void (*callback)(SituationLogLevel level, const char* message, void* user),
    void* user);
```

**Signature:** `callback` receives the resolved level enum and the formatted message string (no trailing newline guaranteed in buffer — implementation adds `\n` only for default stdout path).

**Usage:**

```c
SituationSetLogCallback(my_log_sink, my_user_data);
/* ... */
SituationSetLogCallback(NULL, NULL);  /* restore default stdout logging */
```

---
#### `SituationLogWarning`

Debug-build helper that **both** stores an error via `_SituationSetErrorFromCode` **and** prints to **stderr**. Stripped to a no-op in `NDEBUG` release builds.

```c
void SituationLogWarning(SituationError code, const char* fmt, ...);
#define SITUATION_LOG_WARNING SituationLogWarning
```

**Usage:**

```c
if (frequency <= 0.0f) {
    SituationLogWarning(SITUATION_ERROR_INVALID_PARAM,
                        "SituationPlayTone: frequency must be > 0 (got %.2f)", frequency);
}
```

Use this for **library-style validation warnings** where you want the error buffer populated in debug builds. For general app logging, prefer `SituationLog(SIT_LOG_WARNING, ...)`.

Also documented in [Core Module](core.md) for discoverability from lifecycle APIs.

---
#### `SIT_CHECK_GL_ERROR()` — OpenGL debug macro

OpenGL-only. Defined in `sit/situation_api.h`:

```c
#define SIT_CHECK_GL_ERROR() _SituationLogGLError(__FILE__, __LINE__)
```

After suspicious GL calls in debug builds, insert:

```c
glBindTexture(GL_TEXTURE_2D, tex_id);
SIT_CHECK_GL_ERROR();
```

Behavior:

- Drains the entire `glGetError()` queue in a loop (multiple errors logged).
- Stores the **last** processed error in `SituationGetLastErrorMsg()`.
- Prints `[DEBUG] …` to **stderr** in non-`NDEBUG` builds.
- Maps GLenum codes to readable strings (`GL_INVALID_ENUM`, etc.).

Not available on the Vulkan backend — use validation layers instead.

---

### What the library logs for you

Internal subsystems call `SituationLog` at various levels without your code:

| Subsystem | Example messages |
|-----------|------------------|
| Renderer | Soft-buffer warnings, shader compile hints |
| Model loader | Missing texture paths, mesh skip warnings |
| Audio graph | Serialization errors |
| OpenGL path | `_SituationLogGLError` via `SIT_CHECK_GL_ERROR` |

At default `SIT_LOG_INFO`, you see operational info and above from these paths. Subsystem **debug** lines require lowering the threshold.

Resource leak warnings at `SituationShutdown()` go through a separate path (stderr leak scan) — not filtered by `SituationSetTraceLogLevel`.

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| No log output at all | Threshold too high (`SIT_LOG_NONE`) | `SituationSetTraceLogLevel(SIT_LOG_INFO)` or lower |
| Missing debug lines | Default is `SIT_LOG_INFO` | Set `SIT_LOG_DEBUG` or `SIT_LOG_ALL` |
| Callback never fires | Called before `SituationInit` | Register after init succeeds |
| Colors garbled in log file | ANSI codes in redirected stdout | Use `SetLogCallback` and strip codes, or disable color |
| `LogWarning` silent in release | `NDEBUG` strips it | Use `SituationLog(SIT_LOG_WARNING, …)` for always-on warnings |
| Duplicate messages | Both callback and stdout | Callback replaces stdout — check you didn't also `printf` |
| Error string empty after log | Used `SituationLog` only | Call failing API first; use `GetLastErrorMsg` |

---

### API quick reference

```c
typedef enum SituationLogLevel {
    SIT_LOG_ALL = 0,
    SIT_LOG_TRACE,
    SIT_LOG_DEBUG,
    SIT_LOG_INFO,       /* default threshold */
    SIT_LOG_WARNING,
    SIT_LOG_ERROR,
    SIT_LOG_FATAL,
    SIT_LOG_NONE
} SituationLogLevel;

void SituationLog(int msgType, const char* text, ...);
void SituationSetTraceLogLevel(int logType);
void SituationSetLogCallback(void (*callback)(SituationLogLevel, const char*, void*), void* user);
void SituationLogWarning(SituationError code, const char* fmt, ...);
```

**See also:** [Core — `SituationGetLastErrorMsg`](core.md) · [Core — `SituationErrorToString`](core.md) · `sit/situation_base_errno.h`
