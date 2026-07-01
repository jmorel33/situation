## Filesystem Module

**Overview:** The Filesystem module provides a cross-platform, UTF-8 aware API for reading, writing, and organizing files on disk. All paths are passed as null-terminated UTF-8 strings. For portable applications, prefer library helpers over hardcoded paths: use `SituationGetBasePath()` to locate assets next to the executable, and `SituationGetAppSavePath()` for per-user save data. Strings returned by path helpers must be freed with `SituationFreeString()` (see [Miscellaneous](miscellaneous.md#situationfreestring)).

**Typical workflow:**
1. Resolve a base directory (`SituationGetBasePath()` or `SituationGetAppSavePath()`).
2. Build paths with `SituationJoinPath()` or `snprintf`.
3. Check existence before loading (`SituationFileExists()` / `SituationDirectoryExists()`).
4. Load or save with text/binary helpers.
5. Free any allocated strings and directory listings when done.

When threading is enabled (`SITUATION_ENABLE_THREADING`), use the async variants in the [Threading](threading.md) module to load or save without blocking the main thread.

### Path Management & Special Directories

---
#### `SituationGetBasePath`
Gets the absolute path to the directory containing the application's executable. This is the recommended way to locate asset files because it is unaffected by the current working directory.

```c
char* SituationGetBasePath(void);
```

**Returns:** Dynamically allocated path string (caller must free with `SituationFreeString()`).

**Usage Example:**
```c
char* base = SituationGetBasePath();
char* tex_path = SituationJoinPath(base, "assets/textures/player.png");
SituationTexture tex = SituationLoadTexture(tex_path);
SituationFreeString(tex_path);
SituationFreeString(base);
```

**Notes:**
- Includes a trailing path separator.
- On macOS app bundles, may resolve to the `.app` Resources directory.
- Do not use the process working directory for assets — it varies by launch method.

---
#### `SituationGetAppSavePath`
Gets a platform-appropriate, user-specific directory for configuration files, save games, and other persistent data (e.g. `%APPDATA%/YourApp` on Windows).

```c
char* SituationGetAppSavePath(const char* app_name);
```

**Usage Example:**
```c
char* save_dir = SituationGetAppSavePath("MyCoolGame");
char* config_path = SituationJoinPath(save_dir, "settings.ini");
SituationSaveFileText(config_path, "[Graphics]\nwidth=1920\nheight=1080");
SituationFreeString(config_path);
SituationFreeString(save_dir);
```

---
#### `SituationJoinPath`
Joins two path components with the correct OS separator.

```c
char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);
```

**Returns:** Newly allocated path (caller must free with `SituationFreeString()`).

---
#### `SituationGetFileName`
Extracts the file name component from a path, including the extension.

```c
const char* SituationGetFileName(const char* full_path);
```

**Returns:** Pointer into `full_path` (do not free).

**Usage Example:**
```c
const char* name = SituationGetFileName("C:/assets/textures/player_avatar.png");
// name -> "player_avatar.png"
```

---
#### `SituationGetFileExtension`
Extracts the extension from a path, including the leading dot.

```c
const char* SituationGetFileExtension(const char* file_path);
```

**Returns:** Pointer into `file_path` (do not free).

### File & Directory Queries

---
#### `SituationFileExists`
Checks whether a file exists and is accessible.

```c
bool SituationFileExists(const char* file_path);
```

**Usage Example:**
```c
if (SituationFileExists("settings.ini")) {
    LoadSettings("settings.ini");
} else {
    CreateDefaultSettings();
}
```

---
#### `SituationDirectoryExists`
Checks whether a directory exists and is accessible.

```c
bool SituationDirectoryExists(const char* dir_path);
```

---
#### `SituationGetFileModTime`
Returns the last modification time of a file as a Unix timestamp, or `-1` if the file does not exist. Used by the [Hot-Reloading](hot_reload.md) system and for manual asset refresh.

```c
long SituationGetFileModTime(const char* file_path);
```

**Usage Example:**
```c
long mod = SituationGetFileModTime("shaders/main.frag");
if (mod > g_last_shader_mtime) {
    SituationReloadShader(&g_shader);
    g_last_shader_mtime = mod;
}
```

### File I/O

---
#### `SituationLoadFileText`
Loads an entire text file into a null-terminated string.

```c
char* SituationLoadFileText(const char* file_path);
```

**Returns:** Allocated string, or NULL on failure. Free with `SituationFreeString()`.

**Usage Example:**
```c
char* glsl = SituationLoadFileText("shaders/effect.comp");
if (glsl) {
    SituationCreateComputePipelineFromMemory(glsl, SIT_COMPUTE_LAYOUT_DEFAULT, &pipeline);
    SituationFreeString(glsl);
}
```

---
#### `SituationSaveFileText`
Saves a null-terminated string to a text file.

```c
SituationError SituationSaveFileText(const char* file_path, const char* text);
```

---
#### `SituationLoadFileData`
Loads an entire file into a binary memory buffer.

```c
SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data);
```

**Usage Example:**
```c
unsigned int size = 0;
unsigned char* data = NULL;
if (SituationLoadFileData("assets/level.dat", &size, &data) == SITUATION_SUCCESS) {
    ParseLevelData(data, size);
    SituationFreeString((char*)data); // same allocator as text strings
}
```

---
#### `SituationSaveFileData`
Saves a raw memory buffer to a file.

```c
SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);
```

**Usage Example:**
```c
PlayerState save = { .health = 100, .score = 5000 };
SituationSaveFileData("save.dat", &save, sizeof(save));
```

---
#### `SituationCopyFile`
Copies a file. Overwrites the destination if it already exists.

```c
SituationError SituationCopyFile(const char* source_path, const char* dest_path);
```

---
#### `SituationDeleteFile`
Permanently deletes a file (not moved to recycle bin).

```c
SituationError SituationDeleteFile(const char* file_path);
```

---
#### `SituationMoveFile`
Moves or renames a file. Works across drives on Windows (copy + delete internally when needed).

```c
SituationError SituationMoveFile(const char* old_path, const char* new_path);
```

---
#### `SituationRenameFile`
Alias for `SituationMoveFile()`.

```c
SituationError SituationRenameFile(const char* old_path, const char* new_path);
```

### Directory Operations

---
#### `SituationCreateDirectory`
Creates a directory, optionally creating parent directories in the path.

```c
SituationError SituationCreateDirectory(const char* dir_path, bool create_parents);
```

**Usage Example:**
```c
SituationCreateDirectory("saves/slot_1", true);
```

---
#### `SituationDeleteDirectory`
Deletes a directory. When `recursive` is true, deletes all contents.

```c
SituationError SituationDeleteDirectory(const char* dir_path, bool recursive);
```

---
#### `SituationListDirectoryFiles`
Lists files and subdirectories in a path.

```c
char** SituationListDirectoryFiles(const char* dir_path, int* out_count);
```

**Returns:** Allocated array of path strings, or NULL on failure. Free with `SituationFreeDirectoryFileList()`.

**Usage Example:**
```c
int count = 0;
char** files = SituationListDirectoryFiles("assets/textures", &count);
if (files) {
    for (int i = 0; i < count; i++) {
        if (SituationGetFileExtension(files[i]) &&
            strcmp(SituationGetFileExtension(files[i]), ".png") == 0) {
            SituationLoadTexture(files[i]);
        }
    }
    SituationFreeDirectoryFileList(files, count);
}
```

---
#### `SituationFreeDirectoryFileList`
Frees a listing returned by `SituationListDirectoryFiles()`.

```c
void SituationFreeDirectoryFileList(char** file_list, int count);
```

### Async File I/O _(requires `SITUATION_ENABLE_THREADING`)_

These functions submit work to a [Threading](threading.md) pool and invoke a callback on completion. They are ideal for loading large assets or writing save files without stalling the main loop.

| Function | Purpose |
|----------|---------|
| `SituationLoadFileAsync` | Load binary file asynchronously |
| `SituationSaveFileAsync` | Save binary file asynchronously |
| `SituationLoadFileTextAsync` | Load text file asynchronously |
| `SituationSaveFileTextAsync` | Save text file asynchronously |

See [Threading Module](threading.md) for pool setup and job completion patterns.
