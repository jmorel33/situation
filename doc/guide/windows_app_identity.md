# Windows Application Identity (Win32)

This guide is the **Windows operational reference** for Situation **application identity** — PE resources, AppUserModelID, and runtime window chrome. The **cross-platform architecture** (Windows Phase I shipped; Linux **LI-*** and macOS **MA-*** planned) lives in **[architecture.md — Application Identity](../architecture.md#application-identity-architecture-v24399)**.

Situation applications on Windows have **independent identity layers**. The library owns **defaults** on each layer; authors **override** any layer without forking the library.

See also: [Window display](window_display.md), [SIT_IDENTITY_PLAN](../plan/SIT_IDENTITY_PLAN.md).

---

## Defaults + override matrix

| Layer | Situation default | Author override |
|-------|-------------------|-----------------|
| **PE icon** | `situation_icon.ico` via `sit_app.rc` | Custom `.rc` at link; `SIT_APP_RC` in build scripts |
| **PE version** | `FileDescription`: "Situation Application" in `sit_app.rc` | `sit_app_template.rc` + windres `-DAPP_*` |
| **AppUserModelID** | `Situation.Application` | `SituationInitInfo::app_user_model_id` or `SituationWin32SetAppUserModelId()` |
| **Window title** | `init_info.window_title` | `SituationSetWindowTitle()` |
| **Window icon (runtime)** | Embedded PE icon | `SituationSetWindowIcons()` |
| **Thread name** | `main_thread_name` → title → `"Sit Main"` | `SituationSetCurrentThreadName()` |

**Precedence:** explicit author RC replaces `sit_app.rc` entirely (no merge). Runtime APIs affect the live window only. AppID is set once per process, before the first window.

---

## AppUserModelID (shell identity)

Windows uses AppUserModelID for taskbar pinning, jump lists, and Start menu grouping.

```c
SituationInitInfo info = SituationInitInfoDefault(1280, 720, "My Game");
info.app_user_model_id = "MyStudio.MyGame";  /* optional — NULL → Situation.Application */
SituationInit(0, NULL, &info);
```

Set before `SituationInit` when init is deferred:

```c
SituationWin32SetAppUserModelId("MyStudio.MyGame");
```

Must run **before** the first top-level window is created. Stable across updates — do not embed version numbers in the AppID string.

Default when unset: **`Situation.Application`** (`SITUATION_DEFAULT_APP_USER_MODEL_ID`).

---

## Runtime window icon (`default_window_icon_path`, v2.4.400+)

When **`SituationInitInfo::default_window_icon_path`** is set, Situation loads the file at the **end of `SituationInit`** and calls **`SituationSetWindowIcons`**:

| Format | Platforms |
|--------|-----------|
| PNG (and other stb formats) | All |
| `.ico` (all embedded sizes) | Win32 only |

```c
SituationInitInfo info = SituationInitInfoDefault(1280, 720, "My Game");
info.default_window_icon_path = "assets/window_icon.png";  /* optional */
SituationInit(0, NULL, &info);
```

Load failures are **fail-soft** — init succeeds; the embedded PE icon remains. Override live icons anytime with **`SituationSetWindowIcons()`**.

---

## PE resources (build time)

Default for all repo examples and harness EXEs:

```
sit/platform/windows/sit_app.rc  →  1 ICON + VS_VERSION_INFO
```

**Override** with environment variable:

```powershell
$env:SIT_APP_RC = "C:\path\to\my_game.rc"
& ".\build\build_examples.bat" static-opengl my_game
```

Harness Makefile:

```powershell
mingw32-make -C tests/harness static-opengl APP_RC=C:/path/to/my_game.rc
```

**Author template:** `sit/platform/windows/sit_app_template.rc` — copy or invoke with `-DAPP_FILE_DESCRIPTION=\"My Game\"` etc. See [COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md).

**DLL vs EXE:** `situation_resource.rc` describes the **Situation library DLL** only. Task Manager reads the **EXE** for the running process.

---

## Icon asset pipeline

| Asset | Path |
|-------|------|
| Source PNG | `scripts/art/icon_source.PNG` |
| Generator | `scripts/gen_situation_icon.py` |
| Committed ICO | `sit/platform/windows/situation_icon.ico` |

Regenerate after art changes: `python scripts/gen_situation_icon.py`

---

## Future: Linux and macOS (**LI-*** / **MA-***)

Identity on non-Windows platforms will follow the same **defaults + overrides** model under `sit/platform/linux/` and `sit/platform/macos/` (`.desktop` + `StartupWMClass`, bundle ID + dock icon, etc.). Runtime APIs (`window_title`, `SetWindowIcons`, thread names) already apply cross-platform. Track progress in [SIT_IDENTITY_PLAN](../plan/SIT_IDENTITY_PLAN.md) (Phase III–IV) and [architecture.md](../architecture.md#application-identity-architecture-v24399).

---

## Repo examples (defaults by design)

In-tree samples (`demon_hunt`, KaOS console, harness) link default **`sit_app.rc`** and leave **`app_user_model_id`** NULL — they get Situation PE branding and **`Situation.Application`** shell identity. Each example sets its own **`window_title`** only.

To brand an example like a shipped game: set **`SIT_APP_RC`** (or author RC on the link line) for PE icon/version, and set **`init_info.app_user_model_id`** or call **`SituationWin32SetAppUserModelId`** before init. Same steps apply out-of-tree.

KaOS console: see [KTerm console goals](../plan/KTERM_CONSOLE_GOALS_PLAN.md) — runtime title via `SituationSetWindowTitle`; shell/PE layers use library defaults unless overridden.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Explorer shows Situation hourglass but you wanted custom | Default RC still linked | Link author RC / set `SIT_APP_RC` |
| Task Manager Description says "Situation Application" | Default `VS_VERSION_INFO` | Override in custom RC |
| Pinned taskbar shows wrong name | AppID mismatch vs shortcut | Match AppID on `.lnk` |
| Taskbar icon differs from Explorer | Runtime icon ≠ embedded RC | Align `SetWindowIcons` with linked `.ico` |
| Task Manager thread says "POSIX" | Old build / pre-init naming | v2.4.247+ thread naming path |

---

## MinGW main-thread naming

Task Manager thread names use `OpenThread(THREAD_SET_LIMITED_INFORMATION, …, GetCurrentThreadId())` — not `GetCurrentThread()` — on the process main thread. See `situation_impl_threading_diag.h` (v2.4.247).
