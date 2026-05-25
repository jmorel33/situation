/***************************************************************************************************
*
*   situation_impl_io.h - File I/O, Path Management & Hot-Reload Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h after situation_impl_threading.h.
*
*   Contains:
*     - UTF-8/Wide string conversion helpers (Windows)
*     - Platform includes for filesystem operations
*     - Synchronous file load/save (binary and text)
*     - Filesystem error reporting
*     - Path management helpers
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_IO_H
#define SITUATION_IMPL_IO_H

//==================================================================================
// Platform Includes for Filesystem Operations
//==================================================================================
#if defined(_WIN32)
    #include <direct.h> // For _mkdir _rmdir

// Converts a UTF-8 string to a UTF-16 (wide) string.
// Caller must free the returned WCHAR* with SIT_FREE().
static WCHAR* _sit_utf8_to_wide(const char* utf8_str) {
    if (!utf8_str) return NULL;
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wide_len == 0) return NULL;
    WCHAR* wide_str = (WCHAR*)SIT_MALLOC(wide_len * sizeof(WCHAR));
    if (!wide_str) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str, wide_len);
    return wide_str;
}

// Converts a UTF-16 (wide) string to a UTF-8 string.
// Caller must free the returned char* with SIT_FREE().
static char* _sit_wide_to_utf8(const WCHAR* wide_str) {
    if (!wide_str) return NULL;
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (utf8_len == 0) return NULL;
    char* utf8_str = (char*)SIT_MALLOC(utf8_len * sizeof(char));
    if (!utf8_str) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, utf8_str, utf8_len, NULL, NULL);
    return utf8_str;
}

#else
    #include <errno.h> // For checking errno on rename failure
    #include <unistd.h>
    #include <sys/stat.h> // For mkdir rmdir
    #include <dirent.h>   // For opendir, readdir, closedir
#endif

//==================================================================================
// Filesystem Error Reporting
//==================================================================================

/**
 * @brief [INTERNAL] Sets a detailed filesystem error message including the OS-specific reason.
 * @details This function now attempts to map common OS-level error codes to the more specific SituationError enums for better diagnostics.
 * @param base_message A string describing the operation that failed (e.g., "Failed to create directory").
 * @param path The file or directory path that was involved in the failed operation.
 * @param default_error The default error code to use if no specific mapping is found.
 * @return The specific error code that was set.
 */
static SituationError _SituationSetFilesystemError(const char* base_message, const char* path, SituationError default_error) {
    char platform_error_str[256] = {0};
    SituationError specific_error_code = default_error;

#if defined(_WIN32)
    DWORD error_code = GetLastError();
    if (error_code != 0) {
        // --- Map Windows error codes to our enums ---
        switch (error_code) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                specific_error_code = SITUATION_ERROR_PATH_NOT_FOUND;
                break;
            case ERROR_ACCESS_DENIED:
                specific_error_code = SITUATION_ERROR_PERMISSION_DENIED;
                break;
            case ERROR_SHARING_VIOLATION:
            case ERROR_LOCK_VIOLATION:
                specific_error_code = SITUATION_ERROR_FILE_LOCKED;
                break;
            case ERROR_HANDLE_DISK_FULL:
                specific_error_code = SITUATION_ERROR_DISK_FULL;
                break;
            case ERROR_ALREADY_EXISTS:
            case ERROR_FILE_EXISTS:
                specific_error_code = SITUATION_ERROR_FILE_ALREADY_EXISTS;
                break;
            case ERROR_DIR_NOT_EMPTY:
                specific_error_code = SITUATION_ERROR_DIR_NOT_EMPTY;
                break;
            // Add other mappings as needed...
        }

        // Get the descriptive string from the OS
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            platform_error_str, sizeof(platform_error_str) - 1, NULL
        );
        // Clean up trailing newline characters from FormatMessageA
        size_t len = strlen(platform_error_str);
        if (len > 1 && platform_error_str[len-2] == '\r') platform_error_str[len-2] = '\0';
    } else {
        strncpy(platform_error_str, "No system error code reported.", sizeof(platform_error_str) - 1);
    }
#else
    int err_num = errno; // Capture errno immediately

    // --- Map POSIX error codes to our enums ---
    switch (err_num) {
        case ENOENT:
            specific_error_code = SITUATION_ERROR_PATH_NOT_FOUND;
            break;
        case EACCES:
        case EPERM:
            specific_error_code = SITUATION_ERROR_PERMISSION_DENIED;
            break;
        case EBUSY:
            specific_error_code = SITUATION_ERROR_FILE_LOCKED;
            break;
        case ENOSPC:
            specific_error_code = SITUATION_ERROR_DISK_FULL;
            break;
        case EEXIST:
            specific_error_code = SITUATION_ERROR_FILE_ALREADY_EXISTS;
            break;
        case ENOTEMPTY:
            specific_error_code = SITUATION_ERROR_DIR_NOT_EMPTY;
            break;
        case ENOTDIR:
            specific_error_code = SITUATION_ERROR_PATH_IS_FILE;
            break;
        case EISDIR:
            specific_error_code = SITUATION_ERROR_PATH_IS_DIRECTORY;
            break;
        // Add other mappings as needed...
    }

    // Get the descriptive string from the OS
    strncpy(platform_error_str, strerror(err_num), sizeof(platform_error_str) - 1);
#endif

    char final_message[SITUATION_MAX_ERROR_MSG_LEN];
    snprintf(final_message, sizeof(final_message), "%s: '%s' - %s", base_message, path, platform_error_str);

    // Use the specific error code we determined, with the full message as detail.
    return _SituationSetErrorFromCode(specific_error_code, final_message);
}

//==================================================================================
// Synchronous File Operations
//==================================================================================

/**
 * @brief [INTERNAL] Extracts the directory component from a file path.
 *
 * @details Helper utility used during model loading to resolve relative paths for textures
 *          (e.g., finding "texture.png" located in the same folder as "model.gltf").
 *          Handles both forward slash ('/') and backslash ('\') separators.
 *
 * @param file_path The full path to a file.
 * @return A newly allocated string containing the directory path (e.g., "assets/models").
 *         Returns a duplicate of "." if no directory separator is found.
 *         Returns NULL if input is invalid.
 *
 * @warning The caller is responsible for freeing the returned string.
 */
static char* SituationGetBasePathFromFile(const char* file_path) {
    if (!file_path) return NULL;
    char* path_copy = _sit_strdup(file_path);
    char* last_sep = strrchr(path_copy, '/');
    if (!last_sep) last_sep = strrchr(path_copy, '\\');

    if (last_sep) {
        *last_sep = '\0'; // Truncate at the separator
    } else {
        // No separator found, assume current directory
        SIT_FREE(path_copy);
        return _sit_strdup(".");
    }
    return path_copy;
}

/**
 * @brief Loads the entire contents of a file into a newly allocated memory buffer.
 *
 * @details Reads the complete file at the given path into a contiguous block of memory
 *          allocated with `SIT_MALLOC`. The caller receives ownership of the buffer and
 *          is responsible for freeing it with `SIT_FREE` when no longer needed.
 *
 *          This is the preferred low-level function for reading binary files (images,
 *          models, shaders, audio samples, serialized data, etc.) when you want full
 *          control over the data and do not need text-specific handling (e.g. null-termination).
 *
 * @param file_path Null-terminated path to the file (relative or absolute).
 * @param out_bytes_read Pointer to an unsigned int that receives the actual number of bytes read.
 *                       On success, equals the file size. On failure, set to 0.
 * @param out_data Pointer to an `unsigned char*` that receives the allocated buffer address.
 *                 On success, points to a newly allocated block of size `*out_bytes_read`.
 *                 On failure, set to NULL.
 *                 Caller must free this pointer with `SIT_FREE` when done.
 *
 * @return SITUATION_SUCCESS on successful read and allocation,
 *         or an appropriate error code on failure.
 *
 * @note The returned buffer is **not** null-terminated (suitable for binary data).
 *       For text files, prefer `SituationLoadFileText` which adds null-termination.
 *
 * @see SituationLoadFileText, SituationSaveFileData, SIT_FREE
 */
SITAPI SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data) {
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;

    if (!file_path) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "file_path cannot be NULL.");
    if (!out_bytes_read) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_bytes_read cannot be NULL.");
    if (!out_data) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "out_data cannot be NULL.");

#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(file_path);
    if (!wide_path) {
        _SituationSetErrorFromCode(SITUATION_ERROR_PATH_INVALID, "Could not convert path to wide string (check for invalid UTF-8 characters).");
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    HANDLE hFile = CreateFileW(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SIT_FREE(wide_path);

    if (hFile == INVALID_HANDLE_VALUE) {
        _SituationSetFilesystemError("Failed to open file for reading", file_path, SITUATION_ERROR_FILE_OPEN_FAILED);
        return SITUATION_ERROR_FILE_NOT_FOUND;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size)) {
        CloseHandle(hFile);
        return _SituationSetFilesystemError("Failed to get file size", file_path, SITUATION_ERROR_FILE_READ_FAILED);
    }

    if (file_size.QuadPart > 0xFFFFFFFF) {
        CloseHandle(hFile);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_TOO_LARGE, "File is too large (>4GB).");
    }

    unsigned int size_to_read = (unsigned int)file_size.QuadPart;
    if (size_to_read == 0) {
        CloseHandle(hFile);
        *out_bytes_read = 0;
        *out_data = (unsigned char*)SIT_MALLOC(1); // Return valid, empty buffer.
        return SITUATION_SUCCESS;
    }

    unsigned char* buffer = (unsigned char*)SIT_MALLOC(size_to_read);
    if (!buffer) {
        CloseHandle(hFile);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate buffer for file data.");
    }

    DWORD bytes_read_win = 0;
    if (!ReadFile(hFile, buffer, size_to_read, &bytes_read_win, NULL) || bytes_read_win != size_to_read) {
        SIT_FREE(buffer);
        CloseHandle(hFile);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Error during file read.");
    }

    CloseHandle(hFile);
    *out_bytes_read = size_to_read;
    *out_data = buffer;
    return SITUATION_SUCCESS;

#else // Standard C library implementation (POSIX)
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return _SituationSetFilesystemError("Failed to open file for reading", file_path, SITUATION_ERROR_FILE_OPEN_FAILED);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        return _SituationSetFilesystemError("Failed to get file size", file_path, SITUATION_ERROR_FILE_READ_FAILED);
    }

    unsigned int size_to_read = (unsigned int)file_size;
    if (size_to_read == 0) {
        fclose(file);
        *out_bytes_read = 0;
        *out_data = (unsigned char*)SIT_MALLOC(1);
        return SITUATION_SUCCESS;
    }

    unsigned char* buffer = (unsigned char*)SIT_MALLOC(size_to_read);
    if (!buffer) {
        fclose(file);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate buffer for file data.");
    }

    size_t read_count = fread(buffer, 1, size_to_read, file);
    if (read_count != size_to_read) {
        SIT_FREE(buffer);
        fclose(file);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Error during file read.");
    }

    fclose(file);
    *out_bytes_read = size_to_read;
    *out_data = buffer;
    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Saves a block of memory to a file, overwriting it if it exists.
 * @param file_path The path to the file to save.
 * @param data A pointer to the data to write.
 * @param bytes_to_write The number of bytes to write from the data buffer.
 * @return SITUATION_SUCCESS on success, or an error code on failure.
 */
SITAPI SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write) {
    if (!file_path) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "file_path cannot be NULL.");
    if (!data && bytes_to_write > 0) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "data cannot be NULL when bytes_to_write is > 0.");

#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(file_path);
    if (!wide_path) {
        _SituationSetErrorFromCode(SITUATION_ERROR_PATH_INVALID, "Could not convert path to wide string.");
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }

    HANDLE hFile = CreateFileW(wide_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    SIT_FREE(wide_path);

    if (hFile == INVALID_HANDLE_VALUE) {
        _SituationSetFilesystemError("Failed to create file for writing", file_path, SITUATION_ERROR_FILE_OPEN_FAILED);
        return SITUATION_ERROR_UNKNOWN_ERROR;
    }

    if (bytes_to_write > 0) {
        DWORD bytes_written = 0;
        if (!WriteFile(hFile, data, bytes_to_write, &bytes_written, NULL) || bytes_written != bytes_to_write) {
            _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Error during file write.");
            CloseHandle(hFile);
            return SITUATION_ERROR_UNKNOWN_ERROR;
        }
    }

    CloseHandle(hFile);
    return SITUATION_SUCCESS;

#else // Standard C library implementation
    FILE* file = fopen(file_path, "wb");
    if (!file) {
        return _SituationSetFilesystemError("Failed to create file for writing", file_path, SITUATION_ERROR_FILE_OPEN_FAILED);
    }

    if (bytes_to_write > 0) {
        if (fwrite(data, 1, bytes_to_write, file) != bytes_to_write) {
            fclose(file);
            return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Error during file write.");
        }
    }

    fclose(file);
    return SITUATION_SUCCESS;
#endif
}

/**
 * @brief Load a text file into a null-terminated string.
 * @warning The returned string is dynamically allocated. The caller is responsible for freeing
 *          this memory using `free()` or `SIT_FREE()`.
 * @param file_path The path to the text file to load.
 * @return A new null-terminated string containing the file text, or NULL on failure.
 */
SITAPI char* SituationLoadFileText(const char* file_path) {
    if (!file_path) return NULL;

    unsigned int bytes_read = 0;
    unsigned char* file_data = NULL;
    if (SituationLoadFileData(file_path, &bytes_read, &file_data) != SITUATION_SUCCESS || !file_data) {
        return NULL; // Load failed, error message is already set by the underlying function.
    }

    // Allocate a new buffer that is one byte larger for the null terminator.
    char* text_buffer = (char*)SIT_MALLOC(bytes_read + 1);
    if (!text_buffer) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate buffer for file text.");
        SIT_FREE(file_data);
        return NULL;
    }

    // Copy the file data and null-terminate it.
    memcpy(text_buffer, file_data, bytes_read);
    text_buffer[bytes_read] = '\0';

    SIT_FREE(file_data); // Free the original buffer.

    return text_buffer;
}

/**
 * @brief Saves a null-terminated string to a text file.
 * @param file_path The path to the file to save.
 * @param text The null-terminated string to write.
 * @return True on success, false on failure.
 */
SITAPI bool SituationSaveFileText(const char* file_path, const char* text) {
    if (!file_path || !text) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "file_path or text cannot be NULL.");
        return false;
    }

    // Use strlen to get the number of bytes to write, excluding the null terminator.
    unsigned int len = (unsigned int)strlen(text);

    return (SituationSaveFileData(file_path, text, len) == SITUATION_SUCCESS);
}


//==================================================================================
// Path Management & Special Directories
//==================================================================================

/**
 * @brief Returns a platform-appropriate, writable path for storing application-specific save data.
 *
 * @details Constructs and returns a null-terminated string containing a standard, user-writable
 *          directory path suitable for saving persistent application data.
 *
 * @param app_name Null-terminated string identifying your application (e.g. "MyGame", "SuperEditor").
 * @return A newly allocated string containing the full path to the app's save directory.
 *         Caller must free with `SIT_FREE`. Returns NULL on failure.
 *
 * @see SituationGetBasePath, SituationCreateDirectory, SituationJoinPath
 */
SITAPI char* SituationGetAppSavePath(const char* app_name) {
    if (!app_name || app_name[0] == '\0') {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "App name cannot be NULL or empty.");
        return NULL;
    }

#if defined(_WIN32)
    PWSTR wide_path_appdata = NULL;
    #ifdef __cplusplus
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &wide_path_appdata);
    #else
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wide_path_appdata);
    #endif

    if (FAILED(hr)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SHGetKnownFolderPath failed to retrieve AppData.");
        return NULL;
    }

    char* path_appdata = _sit_wide_to_utf8(wide_path_appdata);
    CoTaskMemFree(wide_path_appdata);

    if (!path_appdata) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to convert AppData path to UTF-8.");
        return NULL;
    }

    size_t final_len = strlen(path_appdata) + 1 + strlen(app_name) + 1;
    char* final_path = (char*)SIT_MALLOC(final_len);
    if (!final_path) {
        SIT_FREE(path_appdata);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGetAppSavePath: failed to allocate final path");
        return NULL;
    }

    snprintf(final_path, final_len, "%s\\%s", path_appdata, app_name);
    SIT_FREE(path_appdata);

    // Create the directory if it doesn't exist
    WCHAR* wide_final_path = _sit_utf8_to_wide(final_path);
    if (wide_final_path) {
        CreateDirectoryW(wide_final_path, NULL); // Fails harmlessly if it already exists
        SIT_FREE(wide_final_path);
    }

    return final_path;
#else // POSIX fallback
    const char* home_dir = getenv("HOME");
    if (!home_dir) { _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SituationGetAppSavePath: HOME environment variable not set"); return NULL; }

    // Follow XDG Base Directory Spec: $XDG_DATA_HOME or fallback to ~/.local/share
    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    char* base_path = NULL;
    if (xdg_data_home && xdg_data_home[0] != '\0') {
        base_path = _sit_strdup(xdg_data_home);
    } else {
        const char* fallback_suffix = "/.local/share";
        size_t len = strlen(home_dir) + strlen(fallback_suffix) + 1;
        base_path = (char*)SIT_MALLOC(len);
        if (base_path) snprintf(base_path, len, "%s%s", home_dir, fallback_suffix);
    }

    if (!base_path) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGetAppSavePath: failed to allocate base path"); return NULL; }

    // Create the base directory if it doesn't exist.
    SituationCreateDirectory(base_path, true);

    size_t final_len = strlen(base_path) + 1 + strlen(app_name) + 1;
    char* final_path = (char*)SIT_MALLOC(final_len);
    if (!final_path) {
        SIT_FREE(base_path);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGetAppSavePath: failed to allocate final path (POSIX)");
        return NULL;
    }
    snprintf(final_path, final_len, "%s/%s", base_path, app_name);
    SIT_FREE(base_path);

    // Create the final directory (recursive).
    SituationCreateDirectory(final_path, true);

    return final_path;
#endif
}

/**
 * @brief Get the path to the directory containing the executable.
 * @warning The returned string is dynamically allocated. The caller is responsible for freeing
 *          this memory using `free()` or `SIT_FREE()`.
 * @return A new string containing the base path, or NULL on failure.
 */
SITAPI char* SituationGetBasePath(void) {
#if defined(_WIN32)
    WCHAR wide_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, wide_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "GetModuleFileNameW failed or path too long.");
        return NULL;
    }

    // Find the last backslash to get the directory part
    WCHAR* last_slash = wcsrchr(wide_path, L'\\');
    if (last_slash) {
        *last_slash = L'\0'; // Null-terminate at the slash to chop off the filename
    }

    return _sit_wide_to_utf8(wide_path);
#else // POSIX fallback
    char path_buf[1024] = {0};
    // readlink is the standard way on Linux
    ssize_t len = readlink("/proc/self/exe", path_buf, sizeof(path_buf) - 1);

    if (len != -1) {
        path_buf[len] = '\0'; // Null-terminate the result
        char* last_slash = strrchr(path_buf, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
        return _sit_strdup(path_buf);
    }

    // Fallback for other systems or if /proc isn't available
    return _sit_strdup("."); // Current working directory
#endif
}

/**
 * @brief Join two path components with the correct OS separator.
 * @warning The returned string is dynamically allocated. The caller is responsible for freeing
 *          this memory using `free()` or `SIT_FREE()`.
 * @return A new string containing the combined path.
 */
SITAPI char* SituationJoinPath(const char* base_path, const char* file_or_dir_name) {
    if (!base_path || !file_or_dir_name) return NULL;

#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif

    size_t base_len = strlen(base_path);
    if (base_len == 0) return _sit_strdup(file_or_dir_name);

    // Check if the base path already ends with a separator
    bool needs_separator = (base_path[base_len - 1] != '\\' && base_path[base_len - 1] != '/');

    size_t final_len = base_len + strlen(file_or_dir_name) + (needs_separator ? 1 : 0) + 1;
    char* final_path = (char*)SIT_MALLOC(final_len);
    if (!final_path) return NULL;

    strcpy(final_path, base_path);
    if (needs_separator) {
        final_path[base_len] = separator;
        final_path[base_len + 1] = '\0';
    }
    strcat(final_path, file_or_dir_name);

    return final_path;
}

/**
 * @brief Extracts the file name (including extension) from a full path.
 * @param full_path The full path to a file (e.g., "/data/assets/player.png").
 * @return A pointer to the start of the file name within the original string (e.g., "player.png").
 */
SITAPI const char* SituationGetFileName(const char* full_path) {
    if (!full_path) return NULL;

    const char* last_slash = strrchr(full_path, '/');
    const char* last_backslash = strrchr(full_path, '\\');

    const char* last_separator = (last_slash > last_backslash) ? last_slash : last_backslash;

    if (last_separator) {
        return last_separator + 1; // Return the character after the separator
    }

    return full_path; // No separator found, the whole string is the filename
}

/**
 * @brief Extracts the file extension from a path.
 * @param file_path The path to a file.
 * @return A pointer to the '.' in the file extension within the original string. Returns NULL if no extension.
 */
SITAPI const char* SituationGetFileExtension(const char* file_path) {
    if (!file_path) return NULL;

    // First, find the filename part to avoid matching dots in directory names
    const char* filename = SituationGetFileName(file_path);
    if (!filename) return NULL;

    const char* last_dot = strrchr(filename, '.');

    // Return the pointer to the dot if it exists and is not the first character (e.g., ".bashrc")
    if (last_dot && last_dot != filename) {
        return last_dot;
    }

    return NULL; // No extension found
}

//==================================================================================
// File & Directory Queries
//==================================================================================

/**
 * @brief Checks if a file exists at the given path.
 * @param file_path The path to the file to check.
 * @return True if the file exists and is a regular file, false otherwise.
 */
SITAPI bool SituationFileExists(const char* file_path) {
    if (!file_path) return false;
#if defined(_WIN32)
    WIN32_FIND_DATAW find_data;
    WCHAR* wide_path = _sit_utf8_to_wide(file_path);
    if (wide_path == NULL) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_sit_utf8_to_wide failed for file path.");
        return false;
    }

    HANDLE handle = FindFirstFileW(wide_path, &find_data);
    SIT_FREE(wide_path);

    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    FindClose(handle);

    return !is_directory;
#else // POSIX
    struct stat st;
    if (stat(file_path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
#endif
}

/**
 * @brief Checks if a directory exists at the given path.
 * @param dir_path The path to the directory to check.
 * @return True if the directory exists, false otherwise.
 */
SITAPI bool SituationDirectoryExists(const char* dir_path) {
    if (!dir_path) return false;
#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(dir_path);
    if (!wide_path) return false;

    DWORD attrib = GetFileAttributesW(wide_path);
    SIT_FREE(wide_path);

    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else // POSIX
    struct stat st;
    if (stat(dir_path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
#endif
}

/**
 * @brief Gets the last modification time of a file.
 * @param file_path The path to the file.
 * @return The last modification time as a Unix timestamp (seconds since epoch), or 0 on failure.
 */
SITAPI long SituationGetFileModTime(const char* file_path) {
    if (!file_path) return 0;
#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(file_path);
    if (!wide_path) return 0;

    HANDLE hFile = CreateFileW(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    SIT_FREE(wide_path);

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    FILETIME ft_write;
    if (!GetFileTime(hFile, NULL, NULL, &ft_write)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);

    // Convert Windows FILETIME to Unix timestamp
    ULARGE_INTEGER uli;
    uli.LowPart = ft_write.dwLowDateTime;
    uli.HighPart = ft_write.dwHighDateTime;
    const ULONGLONG EPOCH_DIFFERENCE = 116444736000000000ULL;
    return (long)((uli.QuadPart - EPOCH_DIFFERENCE) / 10000000L);
#else // POSIX
    struct stat st;
    if (stat(file_path, &st) != 0) return 0;
    return (long)st.st_mtime;
#endif
}

/**
 * @brief Deletes a file from the file system.
 * @param file_path The path to the file to be deleted.
 * @return True if the file was successfully deleted, false otherwise.
 */
SITAPI bool SituationDeleteFile(const char* file_path) {
    if (!file_path) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "file_path cannot be NULL.");
        return false;
    }
#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(file_path);
    if (!wide_path) {
        _SituationSetErrorFromCode(SITUATION_ERROR_PATH_INVALID, "Could not convert path to wide string.");
        return false;
    }

    BOOL result = DeleteFileW(wide_path);
    SIT_FREE(wide_path);

    if (result == 0) {
        _SituationSetFilesystemError("Failed to delete file", file_path, SITUATION_ERROR_FILE_ACCESS);
        return false;
    }
    return true;
#else // POSIX/Standard C
    if (remove(file_path) != 0) {
        _SituationSetFilesystemError("Failed to delete file", file_path, SITUATION_ERROR_FILE_ACCESS);
        return false;
    }
    return true;
#endif
}

/**
 * @brief Renames a file or directory.
 * @param old_path The current path of the file or directory.
 * @param new_path The new path for the file or directory.
 * @return True on success, false on failure.
 */
SITAPI bool SituationRenameFile(const char* old_path, const char* new_path) {
    return SituationMoveFile(old_path, new_path);
}

/**
 * @brief Renames or moves a file or directory. Can move items across different drives on Windows.
 * @param old_path The current path of the file or directory.
 * @param new_path The new path for the file or directory.
 * @return True on success, false on failure.
 */
SITAPI bool SituationMoveFile(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return false;

#if defined(_WIN32)
    WCHAR* wide_old = _sit_utf8_to_wide(old_path);
    if (!wide_old) return false;
    WCHAR* wide_new = _sit_utf8_to_wide(new_path);
    if (!wide_new) { SIT_FREE(wide_old); return false; }

    BOOL result = MoveFileExW(wide_old, wide_new, MOVEFILE_REPLACE_EXISTING);
    SIT_FREE(wide_old);
    SIT_FREE(wide_new);

    if (result == 0) {
        _SituationSetFilesystemError("Failed to move/rename file", old_path, SITUATION_ERROR_FILE_ACCESS);
        return false;
    }
    return true;
#else
    if (rename(old_path, new_path) == 0) {
        return true;
    }
    if (errno == EXDEV) {
        if (SituationCopyFile(old_path, new_path)) {
            return SituationDeleteFile(old_path);
        }
        _SituationSetFilesystemError("Failed to copy file during cross-device move", old_path, SITUATION_ERROR_FILE_ACCESS);
        return false;
    }
    _SituationSetFilesystemError("Failed to move/rename file", old_path, SITUATION_ERROR_FILE_ACCESS);
    return false;
#endif
}

/**
 * @brief Copies a file from a source path to a destination path.
 * @details If the destination file already exists, it will be overwritten.
 * @param source_path The path of the file to copy.
 * @param dest_path The path where the file will be copied to.
 * @return True on success, false on failure.
 */
SITAPI bool SituationCopyFile(const char* source_path, const char* dest_path) {
    if (!source_path || !dest_path) return false;

#if defined(_WIN32)
    WCHAR* wide_source = _sit_utf8_to_wide(source_path);
    if (!wide_source) return false;

    WCHAR* wide_dest = _sit_utf8_to_wide(dest_path);
    if (!wide_dest) {
        SIT_FREE(wide_source);
        return false;
    }

    BOOL result = CopyFileW(wide_source, wide_dest, FALSE);

    SIT_FREE(wide_source);
    SIT_FREE(wide_dest);
    if (result == 0) {
        _SituationSetFilesystemError("Failed to copy file", source_path, SITUATION_ERROR_FILE_ACCESS);
        return false;
    }
    return true;

#else // POSIX/Standard C manual implementation
    const int BUFFER_SIZE = 65536; // 64KB buffer for copying
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    FILE* source = fopen(source_path, "rb");
    if (!source) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "Failed to open source file for copying.");
        return false;
    }

    FILE* dest = fopen(dest_path, "wb");
    if (!dest) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "Failed to open destination file for copying.");
        fclose(source);
        return false;
    }

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest) != bytes_read) {
            _SituationSetErrorFromCode(SITUATION_ERROR_FILE_WRITE_FAILED, "Error writing to destination file.");
            fclose(source);
            fclose(dest);
            return false;
        }
    }

    if (ferror(source)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_READ_FAILED, "Error reading from source file.");
        fclose(source);
        fclose(dest);
        return false;
    }

    fclose(source);
    fclose(dest);
    return true;
#endif
}

//==================================================================================
// Directory Operations
//==================================================================================

/**
 * @brief Creates a directory, optionally creating parent directories.
 * @param dir_path The path of the directory to create.
 * @param create_parents If true, create all missing parent directories.
 * @return True on success, false on failure.
 */
SITAPI bool SituationCreateDirectory(const char* dir_path, bool create_parents) {
    if (!dir_path || dir_path[0] == '\0') {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "dir_path cannot be NULL or empty.");
        return false;
    }

    if (SituationDirectoryExists(dir_path)) {
        return true; // Already exists, success.
    }

    if (!create_parents) {
#if defined(_WIN32)
        WCHAR* wide_path = _sit_utf8_to_wide(dir_path);
        if (!wide_path) {
            _SituationSetErrorFromCode(SITUATION_ERROR_PATH_INVALID, "Could not convert path to wide string.");
            return false;
        }
        BOOL result = CreateDirectoryW(wide_path, NULL);
        SIT_FREE(wide_path);
        if (result == 0) {
            _SituationSetFilesystemError("Failed to create directory", dir_path, SITUATION_ERROR_DIRECTORY_CREATION_FAILED);
            return false;
        }
        return true;
#else
        if (mkdir(dir_path, 0755) != 0) {
            _SituationSetFilesystemError("Failed to create directory", dir_path, SITUATION_ERROR_DIRECTORY_CREATION_FAILED);
            return false;
        }
        return true;
#endif
    }

    char* path_copy = _sit_strdup(dir_path);
    if (!path_copy) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Could not duplicate path string for recursive create.");
        return false;
    }

    char* p = path_copy;
    bool success = true;

    // Handle drive letters or root on both platforms
    if (p[0] == '/') { p++; } // POSIX root
    else if (p[1] == ':' && (p[2] == '\\' || p[2] == '/')) { p += 3; } // Windows drive

    while (*p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (!SituationDirectoryExists(path_copy)) {
                if (!SituationCreateDirectory(path_copy, false)) {
                    success = false;
                    break;
                }
            }
            *p = (p[-1] == ':') ? '\\' : '/'; // Restore separator
        }
        p++;
    }

    if (success) {
        success = SituationCreateDirectory(dir_path, false);
    }

    SIT_FREE(path_copy);
    return success;
}

/**
 * @brief Deletes a directory and, optionally, all of its contents.
 * @param dir_path The path to the directory to delete.
 * @param recursive If true, perform a recursive deletion of all contents.
 * @return True if the directory was successfully deleted, false otherwise.
 */
SITAPI bool SituationDeleteDirectory(const char* dir_path, bool recursive) {
    if (!dir_path || !SituationDirectoryExists(dir_path)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_PATH_NOT_FOUND, "Directory path is invalid or does not exist.");
        return false;
    }

    if (recursive) {
        int count = 0;
        char** entries = SituationListDirectoryFiles(dir_path, &count);
        if (entries) {
            bool all_deleted = true;
            for (int i = 0; i < count; i++) {
                char* full_entry_path = SituationJoinPath(dir_path, entries[i]);
                if (!full_entry_path) {
                    all_deleted = false;
                    continue;
                }

                if (SituationDirectoryExists(full_entry_path)) {
                    if (!SituationDeleteDirectory(full_entry_path, true)) {
                        all_deleted = false;
                    }
                } else {
                    if (!SituationDeleteFile(full_entry_path)) {
                        all_deleted = false;
                    }
                }
                SIT_FREE(full_entry_path);
            }
            SituationFreeDirectoryFileList(entries, count);

            if (!all_deleted) {
                _SituationSetErrorFromCode(SITUATION_ERROR_DIR_NOT_EMPTY, "Failed to delete one or more items within the directory.");
                return false;
            }
        }
    }

    // At this point, the directory should be empty. Proceed with deletion.
#if defined(_WIN32)
    WCHAR* wide_path = _sit_utf8_to_wide(dir_path);
    if (!wide_path) return false;
    BOOL result = RemoveDirectoryW(wide_path);
    SIT_FREE(wide_path);
    return (result != 0);
#else // POSIX
    return (rmdir(dir_path) == 0);
#endif
}

/**
 * @brief Lists the files and subdirectories within a given directory.
 * @param dir_path The path of the directory to scan.
 * @param out_count A pointer to an integer that will be filled with the number of entries found.
 * @return A dynamically allocated array of strings containing the names of the entries.
 *         The caller MUST free using SituationFreeDirectoryFileList. Returns NULL on failure.
 */
SITAPI char** SituationListDirectoryFiles(const char* dir_path, int* out_count) {
    if (!dir_path || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    *out_count = 0;

    int capacity = 32;
    char** files = (char**)SIT_MALLOC(capacity * sizeof(char*));
    if (!files) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Initial allocation for file list failed.");
        return NULL;
    }

#if defined(_WIN32)
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR* wide_search_path = NULL;
#else
    DIR* dir = NULL;
#endif

    char* new_entry_name = NULL;

#if defined(_WIN32)
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", dir_path);

    wide_search_path = _sit_utf8_to_wide(search_path);
    if (!wide_search_path) goto error_cleanup;

    WIN32_FIND_DATAW find_data;
    hFind = FindFirstFileW(wide_search_path, &find_data);
    SIT_FREE(wide_search_path);
    wide_search_path = NULL;

    if (hFind == INVALID_HANDLE_VALUE) goto success_cleanup;

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }
        new_entry_name = _sit_wide_to_utf8(find_data.cFileName);
#else // POSIX
    dir = opendir(dir_path);
    if (!dir) goto success_cleanup;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        new_entry_name = _sit_strdup(entry->d_name);
#endif
        // --- COMMON LOGIC BLOCK ---
        if (!new_entry_name) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "strdup/_sit_wide_to_utf8 failed.");
            goto error_cleanup;
        }

        if (*out_count >= capacity) {
            capacity *= 2;
            char** temp_files = (char**)SIT_REALLOC(files, capacity * sizeof(char*));
            if (!temp_files) {
                SIT_FREE(new_entry_name);
                new_entry_name = NULL;
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SIT_REALLOC failed.");
                goto error_cleanup;
            }
            files = temp_files;
        }

        files[*out_count] = new_entry_name;
        new_entry_name = NULL;
        (*out_count)++;
        // --- END COMMON LOGIC BLOCK ---
#if defined(_WIN32)
    } while (FindNextFileW(hFind, &find_data) != 0);
#else
    }
#endif

success_cleanup:
#if defined(_WIN32)
    if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
#else
    if (dir) closedir(dir);
#endif
    return files;

error_cleanup:
    SituationFreeDirectoryFileList(files, *out_count);
#if defined(_WIN32)
    SIT_FREE(wide_search_path);
    if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
#else
    if (dir) closedir(dir);
#endif
    return NULL;
}

/**
 * @brief Frees the memory allocated by SituationListDirectoryFiles.
 * @param file_list The array of strings returned by SituationListDirectoryFiles.
 * @param count The number of entries in the array.
 */
SITAPI void SituationFreeDirectoryFileList(char** file_list, int count) {
    if (!file_list) return;
    for (int i = 0; i < count; i++) {
        SIT_FREE(file_list[i]);
    }
    SIT_FREE(file_list);
}

//==================================================================================
// IO Thread & Queue Metrics (requires threading)
//==================================================================================
#ifdef SITUATION_ENABLE_THREADING

/**
 * @brief Returns the current number of pending jobs in the Low Priority (I/O) queue.
 * @return The number of pending IO jobs, or 0 if the thread pool is not active.
 */
SITAPI size_t SituationGetIOQueueDepth(void) {
    if (!sit_gs.thread_pool.is_active) return 0;
    return SituationGetQueueDepth(&sit_gs.thread_pool, SIT_JOB_QUEUE_LOW);
}

// [v2.3.34] Dedicated I/O Thread Entry
static int _SituationIOThreadEntry(void* arg) {
    SituationThreadPool* pool = (SituationThreadPool*)arg;
    atomic_store(&pool->io_active, true);

    _SituationApplyIoThreadNumaPlacement(pool);

    // Rate Limiting for Hot-Reload
    struct timespec last_hr_time;
    timespec_get(&last_hr_time, TIME_UTC);

    while (!atomic_load(&pool->shutdown)) {
        {
            int cpu = SituationGetCurrentProcessorIndex();
            atomic_store(&pool->io_last_logical_cpu, cpu);
        }
        // --- 1. Process Low Priority Queue (Index 0) ---
        bool worked = false;
        mtx_lock(&pool->queues[0].lock);
        size_t head = atomic_load(&pool->queues[0].head);
        size_t tail = atomic_load(&pool->queues[0].tail);

        if (tail != head) {
            size_t idx = tail & pool->queues[0].mask;
            SituationJob* job = &pool->queues[0].jobs[idx];
            int dep = atomic_load(&job->dependency_count);

            if (dep == 0) {
                atomic_store(&pool->queues[0].tail, tail + 1);
                mtx_unlock(&pool->queues[0].lock);

                // Execute
                void* d = job->uses_large_data ? job->large_data_ptr : job->storage;
                if (job->func) {
                    SituationError dummy = SITUATION_SUCCESS;
                    job->func(d, (void*)&dummy);
                }

                // Continuation
                uint32_t cont_id = atomic_load(&job->continuation_id);
                if (cont_id != 0) {
                    SituationJob* next_job = _SitGetJobFromId(pool, cont_id);
                    if (next_job) {
                        if (atomic_fetch_sub(&next_job->dependency_count, 1) == 1) cnd_signal(&pool->wake_condition);
                    }
                }

                atomic_store(&job->is_completed, true);
                uint16_t old = atomic_load(&job->generation);
                atomic_store(&job->generation, (uint16_t)((old + 1) & SIT_ID_GEN_MASK));

                if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) cnd_broadcast(&pool->idle_condition);
                atomic_fetch_add(&pool->stats_jobs_completed, 1);
                atomic_fetch_add(&pool->stats_io_jobs_run, 1);
                worked = true;
            } else {
                mtx_unlock(&pool->queues[0].lock);
                thrd_yield();
            }
        } else {
            mtx_unlock(&pool->queues[0].lock);
        }

        // --- 2. Hot-Reload Polling ---
        if (pool->hot_reload_rate > 0.0) {
            struct timespec now;
            timespec_get(&now, TIME_UTC);
            double diff = (now.tv_sec - last_hr_time.tv_sec) + (now.tv_nsec - last_hr_time.tv_nsec) / 1e9;

            if (diff >= pool->hot_reload_rate) {
                _SituationPerformHotReloadPass();
                last_hr_time = now;
            }
        }

        // --- 3. Sleep ---
        if (!worked) {
            atomic_fetch_add(&pool->stats_io_idle_waits, 1);
            struct timespec ts;
            timespec_get(&ts, TIME_UTC);
            ts.tv_nsec += 33000000; // 33ms
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            mtx_lock(&pool->queues[0].lock);
            if (atomic_load(&pool->queues[0].head) == atomic_load(&pool->queues[0].tail) && !atomic_load(&pool->shutdown)) {
                cnd_timedwait(&pool->wake_condition, &pool->queues[0].lock, &ts);
            }
            mtx_unlock(&pool->queues[0].lock);
        }
    }
    atomic_store(&pool->io_active, false);
    return 0;
}

//==================================================================================
// Async File I/O Implementation (requires threading)
//==================================================================================

/**
 * @brief [INTERNAL] Context for asynchronous file loading.
 */
typedef struct {
    char* path;
    SituationFileLoadCallback callback;
    void* user_data;
} _SitAsyncFileLoadCtx;

/**
 * @brief [INTERNAL] Worker function for asynchronous binary file load jobs.
 */
/* HARDENING: void by design — thread-pool job ABI; result via callback/context. */
static void _SituationAsyncFileLoadWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileLoadCtx* ctx = (_SitAsyncFileLoadCtx*)data;

    unsigned int bytes_read = 0;
    unsigned char* file_data = NULL;
    SituationError err = SituationLoadFileData(ctx->path, &bytes_read, &file_data);
    (void)err;

    if (ctx->callback) {
        ctx->callback(file_data, (size_t)bytes_read, ctx->user_data);
    } else {
        if (file_data) SIT_FREE(file_data);
    }

    SIT_FREE(ctx->path);
}

/**
 * @brief Asynchronously loads a file from disk.
 * @details Offloads the blocking `SituationLoadFileData` call to a background thread.
 *
 * @param pool The thread pool.
 * @param file_path The path to load.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data) {
    if (!pool || !file_path) return 0;

    _SitAsyncFileLoadCtx ctx;
    ctx.path = _sit_strdup(file_path);
    if (!ctx.path) return 0;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SituationAsyncFileLoadWorker, &ctx, sizeof(_SitAsyncFileLoadCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
    }
    return jid;
}

/**
 * @brief [INTERNAL] Context for asynchronous text file loading.
 */
typedef struct {
    char* path;
    SituationFileTextLoadCallback callback;
    void* user_data;
} _SitAsyncFileTextLoadCtx;

/**
 * @brief [INTERNAL] Worker function for asynchronous text file load jobs.
 */
/* HARDENING: void by design — thread-pool job ABI; result via callback/context. */
static void _SituationAsyncFileTextLoadWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileTextLoadCtx* ctx = (_SitAsyncFileTextLoadCtx*)data;

    char* text = SituationLoadFileText(ctx->path);

    if (ctx->callback) {
        ctx->callback(text, ctx->user_data);
    } else {
        if (text) SIT_FREE(text);
    }

    SIT_FREE(ctx->path);
}

/**
 * @brief Asynchronously loads a text file from disk.
 * @details Offloads the blocking `SituationLoadFileText` call to a background thread.
 *
 * @param pool The thread pool.
 * @param file_path The path to load.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data) {
    if (!pool || !file_path) return 0;

    _SitAsyncFileTextLoadCtx ctx;
    ctx.path = _sit_strdup(file_path);
    if (!ctx.path) return 0;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SituationAsyncFileTextLoadWorker, &ctx, sizeof(_SitAsyncFileTextLoadCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
    }
    return jid;
}

/**
 * @brief [INTERNAL] Context for asynchronous text file saving.
 */
typedef struct {
    char* path;
    char* text_copy;
    SituationFileSaveCallback callback;
    void* user_data;
} _SitAsyncFileTextSaveCtx;

/**
 * @brief [INTERNAL] Worker function for asynchronous text file save jobs.
 */
/* HARDENING: void by design — thread-pool job ABI; result via callback/context. */
static void _SituationAsyncFileTextSaveWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileTextSaveCtx* ctx = (_SitAsyncFileTextSaveCtx*)data;

    bool success = SituationSaveFileText(ctx->path, ctx->text_copy);

    if (ctx->callback) {
        ctx->callback(success, ctx->user_data);
    }

    SIT_FREE(ctx->path);
    SIT_FREE(ctx->text_copy);
}

/**
 * @brief Asynchronously saves a string to a text file.
 * @details Copies the input string to a temporary buffer and offloads the write to a worker thread.
 *
 * @param pool The thread pool.
 * @param file_path The path to save to.
 * @param text The null-terminated string to write.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data) {
    if (!pool || !file_path || !text) return 0;

    _SitAsyncFileTextSaveCtx ctx;
    ctx.path = _sit_strdup(file_path);
    if (!ctx.path) return 0;

    ctx.text_copy = _sit_strdup(text);
    if (!ctx.text_copy) {
        SIT_FREE(ctx.path);
        return 0;
    }

    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SituationAsyncFileTextSaveWorker, &ctx, sizeof(_SitAsyncFileTextSaveCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
        SIT_FREE(ctx.text_copy);
    }
    return jid;
}

/**
 * @brief [INTERNAL] Context for asynchronous file saving.
 */
typedef struct {
    char* path;
    void* data_copy;
    size_t size;
    SituationFileSaveCallback callback;
    void* user_data;
} _SitAsyncFileSaveCtx;

/**
 * @brief [INTERNAL] Worker function for asynchronous file save jobs.
 */
/* HARDENING: void by design — thread-pool job ABI; result via callback/context. */
static void _SituationAsyncFileSaveWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileSaveCtx* ctx = (_SitAsyncFileSaveCtx*)data;

    bool success = (SituationSaveFileData(ctx->path, ctx->data_copy, (unsigned int)ctx->size) == SITUATION_SUCCESS);

    if (ctx->callback) {
        ctx->callback(success, ctx->user_data);
    }

    SIT_FREE(ctx->path);
    SIT_FREE(ctx->data_copy);
}

/**
 * @brief Asynchronously saves data to a file.
 * @details Copies the input data to a temporary buffer and offloads the write to a worker thread.
 *
 * @param pool The thread pool.
 * @param file_path The path to save to.
 * @param data The data to write.
 * @param size The size of the data in bytes.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data) {
    if (!pool || !file_path || !data || size == 0) return 0;

    _SitAsyncFileSaveCtx ctx;
    ctx.path = _sit_strdup(file_path);
    if (!ctx.path) return 0;

    ctx.data_copy = SIT_MALLOC(size);
    if (!ctx.data_copy) {
        SIT_FREE(ctx.path);
        return 0;
    }
    memcpy(ctx.data_copy, data, size);

    ctx.size = size;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SituationAsyncFileSaveWorker, &ctx, sizeof(_SitAsyncFileSaveCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
        SIT_FREE(ctx.data_copy);
    }
    return jid;
}

#endif // SITUATION_ENABLE_THREADING

// --- System Profiling Implementation ---
#if defined(__GNUC__) || defined(__clang__)
    #define SITUATION_DEVICE_INFO_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define SITUATION_DEVICE_INFO_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define SITUATION_DEVICE_INFO_DEPRECATED(msg)
#endif
/**
 * @brief Gathers and returns a comprehensive snapshot of the host system's hardware.
 * @details This function queries the operating system and underlying platform libraries to collect a wide range of information about the CPU, GPU, memory, storage, and connected devices. The collected data is aggregated into a single `SituationDeviceInfo` struct.
 *          This function is designed to give the application deep "Awareness" of its runtime environment, which can be used for logging, debugging, selecting quality settings, or displaying system information to the user.
 *
 * @par Data Collection & Platform Specificity
 *   The level of detail provided is platform-dependent and relies on various native APIs:
 *   - **CPU Info (Name, Cores, Speed):** Retrieved from the Windows Registry on Windows and `sysconf` on POSIX systems.
 *   - **GPU Info (Name, VRAM):**
 *     - On Windows, it prioritizes the DXGI API (if `SITUATION_ENABLE_DXGI` is defined) for the most accurate information, including dedicated VRAM.
 *     - As a fallback, or on other platforms, it retrieves the renderer string provided by the active graphics context (OpenGL or Vulkan).
 *   - **RAM Info (Total, Available):** Uses `GlobalMemoryStatusEx` on Windows and `sysinfo` on Linux. Not implemented on all POSIX systems.
 *   - **Storage Info (Drives, Capacity):** Enumerates logical drives on Windows. Not implemented on other platforms.
 *   - **Network & Input Devices:** Enumerates adapters and device classes on Windows for detailed names. Not implemented on other platforms.
 *
 * @return A `SituationDeviceInfo` struct populated with the discovered hardware information.
 * @return A zeroed struct if the library is not initialized. Fields for which information could not be retrieved will also be zero or empty.
 *
 * @note This can be a moderately expensive call, as it may involve querying multiple system APIs. It is best to call it once at startup and cache the results if the information is needed frequently.
 * @warning The completeness of the returned data is highly dependent on the operating system. Features like VRAM size, storage info, and detailed network/input device names are most reliable on Windows.
 */
/**
 * @brief Returns the number of logical CPU cores (threads) available.
 *        Falls back to 1 if query fails.
 * @return uint32_t Number of threads (hyper-threading included)
 */
SITAPI uint32_t SituationGetCPUThreadCount(void) {
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (uint32_t)sysinfo.dwNumberOfProcessors;

#elif defined(__APPLE__)
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0) == 0) {
        return (uint32_t)(count > 0 ? count : 1);
    }
    // Fallback
    if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0) == 0) {
        return (uint32_t)(count > 0 ? count : 1);
    }
    return 1;

#elif defined(__linux__)
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)(cores > 0 ? cores : 1);

#else
    return 1;  // Minimal safe fallback
#endif
}

SITUATION_DEVICE_INFO_DEPRECATED("Use the new, more specific functions like SituationGetCPUInfo(), SituationGetGPUInfo(), etc. This function will be removed in a future version.")
SITAPI SituationDeviceInfo SituationGetDeviceInfo(void) {
    SituationDeviceInfo info = {0};
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot get device info"); return info; }

    #if defined(_WIN32)
    // CPU Info
    SYSTEM_INFO sys_info_win;
    GetSystemInfo(&sys_info_win);
    info.cpu_cores = (int)SituationGetCPUThreadCount();
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size_cpu_name = sizeof(info.cpu_name);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)info.cpu_name, &size_cpu_name) != ERROR_SUCCESS) {
            strncpy(info.cpu_name, "Unknown CPU", SITUATION_MAX_CPU_NAME_LEN -1);
            info.cpu_name[SITUATION_MAX_CPU_NAME_LEN -1] = '\0';
        }
        DWORD speed_mhz = 0;
        DWORD size_speed = sizeof(speed_mhz);
        if (RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&speed_mhz, &size_speed) == ERROR_SUCCESS) {
            info.cpu_clock_speed_ghz = speed_mhz / 1000.0f;
        }
        RegCloseKey(hKey);
    } else {
        strncpy(info.cpu_name, "Unknown CPU (RegOpenKeyExA failed)", SITUATION_MAX_CPU_NAME_LEN -1);
        info.cpu_name[SITUATION_MAX_CPU_NAME_LEN -1] = '\0';
    }

    // GPU Info
    #ifdef SITUATION_ENABLE_DXGI
    // DXGI needs COM to be initialized. The flag sit_gs.is_com_initialized should be true.
    if (sit_gs.is_com_initialized) { // Check if COM is available
        IDXGIFactory* pFactory = NULL;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory)) && pFactory) {
            IDXGIAdapter* pAdapter = NULL;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter)) && pAdapter) { // Get primary adapter
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, info.gpu_name, SITUATION_MAX_GPU_NAME_LEN-1, NULL, NULL);
                    info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
                    info.gpu_dedicated_memory_bytes = desc.DedicatedVideoMemory;
                } else { strncpy(info.gpu_name, "Unknown GPU (DXGI desc failed)", SITUATION_MAX_GPU_NAME_LEN-1);
                info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';}
                pAdapter->Release();
            } else { strncpy(info.gpu_name, "Unknown GPU (DXGI adapter enum failed)", SITUATION_MAX_GPU_NAME_LEN-1);
            info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';}
            pFactory->Release();
        } else { /* CreateDXGIFactory failed, or pFactory is NULL */
            strncpy(info.gpu_name, "Unknown GPU (DXGI factory failed)", SITUATION_MAX_GPU_NAME_LEN-1);
            info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
        }
    } else
    #endif // SITUATION_ENABLE_DXGI
    #ifdef SITUATION_USE_OPENGL
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
#else
    if (sit_gs.sit_glfw_window) {
        const char* gl_renderer = "Vulkan";
#endif
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Unknown GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Unknown GPU (No context/DXGI/COM)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }

    // RAM Info
    MEMORYSTATUSEX mem_status = { .dwLength = sizeof(MEMORYSTATUSEX) };
    if (GlobalMemoryStatusEx(&mem_status)) {
        info.total_ram_bytes = mem_status.ullTotalPhys;
        info.available_ram_bytes = mem_status.ullAvailPhys;
    }

    // Storage Info
    DWORD drives_mask = GetLogicalDrives();
    info.storage_device_count = 0;
    for (int i = 0; i < 26 && info.storage_device_count < SITUATION_MAX_STORAGE_DEVICES; ++i) {
        if (drives_mask & (1 << i)) {
            char drive_path[] = { (char)('A' + i), ':', '\\', '\0' };
            ULARGE_INTEGER total_cap, free_space;
            if (GetDiskFreeSpaceExA(drive_path, NULL, &total_cap, &free_space)) { snprintf(info.storage_device_names[info.storage_device_count], SITUATION_MAX_DEVICE_NAME_LEN, "Drive %c:", (char)('A' + i));
                info.storage_capacity_bytes[info.storage_device_count] = total_cap.QuadPart;
                info.storage_free_bytes[info.storage_device_count] = free_space.QuadPart;
                info.storage_device_count++;
            }
        }
    }

    // Network Adapter Info
    ULONG adapters_buffer_size = 15000; // Recommended starting size by MS docs
    info.network_adapter_count = 0;
    IP_ADAPTER_ADDRESSES* adapters_list = (IP_ADAPTER_ADDRESSES*)SIT_MALLOC(adapters_buffer_size);
    if (adapters_list) {
        DWORD ret_val = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters_list, &adapters_buffer_size);
        if (ret_val == ERROR_BUFFER_OVERFLOW) { // Should have been caught if initial buffer was 0 and we got size.
                                                // But if initial guess was too small.
            SIT_FREE(adapters_list);
            adapters_list = (IP_ADAPTER_ADDRESSES*)SIT_MALLOC(adapters_buffer_size); // Retry with new size
            if (adapters_list) { ret_val = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters_list, &adapters_buffer_size);
            }
        }
        if (ret_val == ERROR_SUCCESS && adapters_list) {
            IP_ADAPTER_ADDRESSES* current_adapter = adapters_list;
            while (current_adapter && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS) {
                // Filter for common operational adapters if desired (e.g., IfOperStatusUp) if (current_adapter->OperStatus == IfOperStatusUp) {
                WideCharToMultiByte(CP_UTF8, 0, current_adapter->FriendlyName, -1, info.network_adapter_names[info.network_adapter_count], SITUATION_MAX_DEVICE_NAME_LEN-1, NULL, NULL);
                info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                info.network_adapter_count++;
                // }
                current_adapter = current_adapter->Next;
            }
        }
        SIT_FREE(adapters_list); adapters_list = NULL;
    }


    // Input Device Info
    info.input_device_count = 0;
    const GUID* device_classes[] = { &GUID_DEVCLASS_KEYBOARD, &GUID_DEVCLASS_MOUSE, &GUID_DEVCLASS_HIDCLASS };
    for (int class_idx = 0; class_idx < 3 && info.input_device_count < SITUATION_MAX_INPUT_DEVICES; ++class_idx) {
        HDEVINFO hDevInfo = SetupDiGetClassDevsW(device_classes[class_idx], NULL, NULL, DIGCF_PRESENT);
        if (hDevInfo == INVALID_HANDLE_VALUE) continue;
        SP_DEVINFO_DATA dev_info_data = { .cbSize = sizeof(SP_DEVINFO_DATA) };
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &dev_info_data) && info.input_device_count < SITUATION_MAX_INPUT_DEVICES; ++i) {
            char friendly_name[SITUATION_MAX_DEVICE_NAME_LEN];
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &dev_info_data, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendly_name, sizeof(friendly_name)-1, NULL) ||
                SetupDiGetDeviceRegistryPropertyW(hDevInfo, &dev_info_data, SPDRP_DEVICEDESC, NULL, (PBYTE)friendly_name, sizeof(friendly_name)-1, NULL) ) {
                friendly_name[sizeof(friendly_name)-1] = '\0';
                if (device_classes[class_idx] == &GUID_DEVCLASS_HIDCLASS) { // Filter HIDCLASS for gamepads/controllers
                    if (!strstr(friendly_name, "Controller") && !strstr(friendly_name, "Gamepad") &&
                        !strstr(friendly_name, "Joystick") && !strstr(friendly_name, "XBOX") &&
                        !strstr(friendly_name, "Wireless Controller") && !strstr(friendly_name, "Joy-Con") &&
                        !strstr(friendly_name, "controller") && !strstr(friendly_name, "gamepad") ) { // Add lowercase checks
                        continue; // Skip if not a typical gamepad name
                    }
                }
                strncpy(info.input_device_names[info.input_device_count], friendly_name, SITUATION_MAX_DEVICE_NAME_LEN -1);
                info.input_device_names[info.input_device_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                info.input_device_count++;
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    #elif defined(__linux__) // Linux Implementation
    // CPU Info
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        bool found = false;
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* start = strchr(line, ':');
                if (start) {
                    strncpy(info.cpu_name, start + 2, SITUATION_MAX_CPU_NAME_LEN-1); // +2 to skip ": "
                    info.cpu_name[SITUATION_MAX_CPU_NAME_LEN-1] = '\0';
                    // Remove newline
                    size_t len = strlen(info.cpu_name);
                    if (len > 0 && info.cpu_name[len-1] == '\n') info.cpu_name[len-1] = '\0';
                    found = true;
                    break;
                }
            }
        }
        fclose(cpuinfo);
        if (!found) strncpy(info.cpu_name, "Linux CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    } else {
        strncpy(info.cpu_name, "Unknown Linux CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    }
    info.cpu_cores = (int)SituationGetCPUThreadCount();

    // RAM Info
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.total_ram_bytes = (uint64_t)si.totalram * si.mem_unit;
        info.available_ram_bytes = (uint64_t)si.freeram * si.mem_unit;
    }

    // Storage Info (Root partition)
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        info.storage_device_count = 1;
        strncpy(info.storage_device_names[0], "/", SITUATION_MAX_DEVICE_NAME_LEN-1);
        info.storage_capacity_bytes[0] = (uint64_t)stat.f_blocks * stat.f_frsize;
        info.storage_free_bytes[0] = (uint64_t)stat.f_bfree * stat.f_frsize;
    }

    // Network Adapter Info
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            // Only care about AF_INET (IPv4) or AF_INET6 (IPv6) and not loopback
            if ((ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) &&
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                // Check if we already added this interface (getifaddrs returns one entry per address per interface)
                bool exists = false;
                for(int i=0; i<info.network_adapter_count; ++i) {
                    if (strcmp(info.network_adapter_names[i], ifa->ifa_name) == 0) { exists = true; break; }
                }
                if (!exists) {
                    strncpy(info.network_adapter_names[info.network_adapter_count], ifa->ifa_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                    info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                    info.network_adapter_count++;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    // Input Device Info
    FILE* bus_devices = fopen("/proc/bus/input/devices", "r");
    if (bus_devices) {
        char line[256];
        char current_name[SITUATION_MAX_DEVICE_NAME_LEN] = {0};
        while (fgets(line, sizeof(line), bus_devices) && info.input_device_count < SITUATION_MAX_INPUT_DEVICES) {
            if (strncmp(line, "N: Name=", 8) == 0) {
                // Extract name
                strncpy(current_name, line + 9, SITUATION_MAX_DEVICE_NAME_LEN - 1); // Skip "N: Name=\""
                size_t len = strlen(current_name);
                if (len > 0 && current_name[len-1] == '\n') current_name[len-1] = '\0';
                if (len > 0 && current_name[len-2] == '"') current_name[len-2] = '\0'; // Remove trailing quote
                if (len > 0 && current_name[len-1] == '"') current_name[len-1] = '\0'; // Or just quote
            } else if (strncmp(line, "H: Handlers=", 12) == 0) {
                // Check if it has a relevant handler like kbd, mouse, js, or event
                if (strstr(line, "kbd") || strstr(line, "mouse") || strstr(line, "js") || strstr(line, "event")) {
                    if (strlen(current_name) > 0) {
                        strncpy(info.input_device_names[info.input_device_count], current_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                        info.input_device_names[info.input_device_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                        info.input_device_count++;
                        current_name[0] = '\0'; // Reset
                    }
                }
            }
        }
        fclose(bus_devices);
    }

    // GPU Info
    #ifdef SITUATION_USE_OPENGL
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
#else
    if (sit_gs.sit_glfw_window) {
        const char* gl_renderer = "Vulkan";
#endif
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Generic GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }

    #elif defined(__APPLE__) // macOS Implementation
    // CPU Info
    size_t size = sizeof(info.cpu_name);
    if (sysctlbyname("machdep.cpu.brand_string", info.cpu_name, &size, NULL, 0) != 0) {
        strncpy(info.cpu_name, "Apple CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    }
    info.cpu_cores = (int)SituationGetCPUThreadCount();

    // RAM Info
    int64_t memsize = 0;
    size = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) == 0) {
        info.total_ram_bytes = (uint64_t)memsize;
        // Available RAM is complex on macOS (vm_stat), omitting for brevity/stability
        info.available_ram_bytes = 0;
    }

    // Storage Info
    struct statfs stats;
    if (statfs("/", &stats) == 0) {
        info.storage_device_count = 1;
        strncpy(info.storage_device_names[0], "/", SITUATION_MAX_DEVICE_NAME_LEN-1);
        info.storage_capacity_bytes[0] = (uint64_t)stats.f_blocks * stats.f_bsize;
        info.storage_free_bytes[0] = (uint64_t)stats.f_bfree * stats.f_bsize;
    }

    // Network Adapter Info (Shared with Linux via getifaddrs)
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) &&
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                bool exists = false;
                for(int i=0; i<info.network_adapter_count; ++i) {
                    if (strcmp(info.network_adapter_names[i], ifa->ifa_name) == 0) { exists = true; break; }
                }
                if (!exists) {
                    strncpy(info.network_adapter_names[info.network_adapter_count], ifa->ifa_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                    info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                    info.network_adapter_count++;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    // GPU Info
    #ifdef SITUATION_USE_OPENGL
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
#else
    if (sit_gs.sit_glfw_window) {
        const char* gl_renderer = "Vulkan";
#endif
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
    }

    #else // Fallback for other platforms
    strncpy(info.cpu_name, "Generic CPU", SITUATION_MAX_CPU_NAME_LEN-1); info.cpu_name[SITUATION_MAX_CPU_NAME_LEN-1] = '\0';
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    info.cpu_cores = (nproc > 0) ? (int)nproc : 1;

    #ifdef SITUATION_USE_OPENGL
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
#else
    if (sit_gs.sit_glfw_window) {
        const char* gl_renderer = "Vulkan";
#endif
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Generic GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }
    #endif

    // --- Common: Display Info (via GLFW) ---
    // This runs on all platforms where GLFW is available (Windows, Linux, macOS)
    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    info.display_count = (monitor_count < SITUATION_MAX_MONITORS) ? monitor_count : SITUATION_MAX_MONITORS;

    for (int i = 0; i < info.display_count; ++i) {
        const char* name = glfwGetMonitorName(monitors[i]);
        if (name) {
            strncpy(info.display_names[i], name, SITUATION_MAX_MONITOR_NAME_LEN - 1);
            info.display_names[i][SITUATION_MAX_MONITOR_NAME_LEN - 1] = '\0';
        } else {
            strncpy(info.display_names[i], "Unknown Display", SITUATION_MAX_MONITOR_NAME_LEN - 1);
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (mode) {
            info.display_widths[i] = mode->width;
            info.display_heights[i] = mode->height;
            info.display_refresh_rates[i] = mode->refreshRate;
        }
    }

    return info;
}

/**
 * @brief Gets the human-readable name of the active GPU.
 *
 * @details Returns the renderer string provided by the active backend.
 *          - **OpenGL:** Returns `glGetString(GL_RENDERER)`.
 *          - **Vulkan:** Returns `VkPhysicalDeviceProperties.deviceName`.
 *
 * @return A pointer to a static string containing the GPU name (e.g., "NVIDIA GeForce RTX 4090").
 *         Do not free this string.
 */
SITAPI const char* SituationGetGPUName(void) {
    if (!SituationIsInitialized()) return "Unknown (Not Initialized)";

#if defined(SITUATION_USE_OPENGL)
    if (sit_gs.sit_glfw_window) {
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        if (renderer) return renderer;
    }
    return "Unknown OpenGL Device";

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.physical_device != VK_NULL_HANDLE) {
        // We use a static buffer to return a valid const char* pointer without SIT_MALLOC.
        // This is not thread-safe if called concurrently, but getting GPU name is usually a setup-time task.
        static char device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

        // Only query if we haven't already (simple optimization)
        if (device_name[0] == '\0') {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &properties);
            strncpy(device_name, properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
        }
        return device_name;
    }
    return "Unknown Vulkan Device";
#endif

    return "Unknown Backend";
}

// --- Storage Media Information Implementation ---

/**
 * @brief Retrieves the full path to the current user's home directory.
 *
 * @details This function provides a cross-platform way to get the root directory for the current user profile.
 *          - **Windows:** Returns the path mapped to `FOLDERID_Profile` (e.g., `C:\Users\Name`).
 *            It internally handles the conversion from Windows Wide Characters (UTF-16) to UTF-8.
 *          - **Linux/macOS:** Returns the value of the `$HOME` environment variable.
 *            If `$HOME` is unset, it falls back to querying the password database (`getpwuid`).
 *
 * @return A dynamically allocated, null-terminated UTF-8 string containing the path.
 * @return `NULL` if the directory could not be determined or if memory allocation failed.
 *
 * @warning The returned string is allocated on the heap. The caller is **responsible** for freeing this memory
 *          using `free()` or `SituationFreeString()` when it is no longer needed.
 *
 * @see SituationGetAppSavePath()
 */
SITAPI char* SituationGetUserDirectory(void) {
    #if defined(_WIN32)
    if (!SituationIsInitialized() || !sit_gs.is_com_initialized) { // Check COM for SHGetKnownFolderPath
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "COM or library not initialized for GetUserDirectory");
        return NULL;
    }
    PWSTR wPath = NULL;
    #ifdef __cplusplus
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &wPath);
    #else
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wPath);
    #endif
    if (SUCCEEDED(hr) && wPath) {
        char path_utf8[MAX_PATH * 4]; // Ensure enough space for UTF-8
        int chars_converted = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, path_utf8, sizeof(path_utf8), NULL, NULL);
        CoTaskMemFree(wPath);
        if (chars_converted > 0) {
            char* result = (char*)SIT_MALLOC(strlen(path_utf8) + 1);
            if (!result) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "User directory string"); return NULL; }
            strcpy(result, path_utf8);
            return result;
        }
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SHGetKnownFolderPath failed");
    return NULL;
    #else
    // On Linux/macOS, get $HOME or use getpwuid(getuid())->pw_dir
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home_dir = pw->pw_dir;
    }
    if (home_dir) {
        char* result = (char*)SIT_MALLOC(strlen(home_dir) + 1);
        if (!result) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "User directory string"); return NULL; }
        strcpy(result, home_dir);
        return result;
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "Could not get user directory");
    return NULL;
    #endif
}


#if defined(_WIN32)
#include <fileapi.h> // For GetVolumeInformationA, GetDiskFreeSpaceExA
#include <shlwapi.h> // For PathGetDriveNumberA (link with Shlwapi.lib)
#pragma comment(lib, "Shlwapi.lib") // For PathGetDriveNumberA

/**
 * @brief Gets the drive letter of the logical volume where the running executable is located.
 * @details This is a Windows-specific utility function. It retrieves the full path of the current application's executable and extracts the drive letter from it (e.g., 'C', 'D').
 *
 * @par Platform Specificity
 *   This function is only implemented on Windows and will not be available on other platforms like Linux or macOS, where the concept of drive letters does not exist.
 *
 * @return The uppercase drive letter (e.g., 'C') on success.
 * @return `0` (null character) if the function fails, if the executable is running from a path without a drive letter (e.g., a UNC network path), or if the library is not initialized.
 *
 * @note This function is useful for applications that need to be aware of their installation location in a Windows environment, for example, to check for available space on the current drive.
 *
 * @see SituationGetDriveInfo()
 */
SITAPI char SituationGetCurrentDriveLetter(void) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetCurrentDriveLetter");
        return 0;
    }
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "GetModuleFileNameA failed for current drive letter");
        return 0;
    }

    int drive_number = PathGetDriveNumberA(exe_path);
    if (drive_number != -1) { // -1 means no drive letter (e.g. UNC path) or error
        return (char)('A' + drive_number);
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "PathGetDriveNumberA failed or path has no drive letter");
    return 0;
}

/**
 * @brief Retrieves information about a specific logical drive on Windows, including its capacity, free space, and volume name.
 * @details This is a Windows-specific utility function that provides detailed information about a storage volume identified by its drive letter.
 *
 * @par Platform Specificity
 *   This function is only implemented on Windows and will not be available on other platforms. It uses the Win32 API functions `GetDiskFreeSpaceExA` and `GetVolumeInformationA`.
 *
 * @param drive_letter The letter of the drive to query (e.g., 'C' or 'c').
 * @param[out] out_total_capacity_bytes A pointer to a `uint64_t` that will be filled with the total size of the drive in bytes. Can be `NULL` if not needed.
 * @param[out] out_free_space_bytes A pointer to a `uint64_t` that will be filled with the free space available to the current user on the drive, in bytes. Can be `NULL` if not needed.
 * @param[out] out_volume_name A character buffer that will be filled with the drive's volume label (e.g., "Local Disk"). Can be `NULL` if not needed.
 * @param volume_name_len The size of the `out_volume_name` buffer, including the null terminator.
 *
 * @return `true` if the function was able to attempt the query.
 * @return `false` if the library is not initialized or if the provided drive letter is invalid.
 *
 * @note The function is considered successful if the API calls are made. If a specific query fails (e.g., a drive is not ready), the corresponding output parameter will not be filled, and an internal error will be set via `SituationGetLastErrorMsg()`.
 *   The caller should always check the contents of the output parameters.
 *
 * @see SituationGetCurrentDriveLetter()
 */
SITAPI bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetDriveInfo");
        return false;
    }
    if (!((drive_letter >= 'A' && drive_letter <= 'Z') || (drive_letter >= 'a' && drive_letter <= 'z'))) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid drive letter for GetDriveInfo");
        return false;
    }
    if (!out_total_capacity_bytes && !out_free_space_bytes && !out_volume_name) {
         _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "No output parameters provided for GetDriveInfo");
        return false; // Nothing to retrieve
    }


    char root_path[4]; // "X:\\"
    snprintf(root_path, sizeof(root_path), "%c:\\", toupper(drive_letter));

    if (out_total_capacity_bytes || out_free_space_bytes) {
        ULARGE_INTEGER total_bytes, free_bytes_to_caller, total_free_bytes;
        if (GetDiskFreeSpaceExA(root_path, &free_bytes_to_caller, &total_bytes, &total_free_bytes)) {
            if (out_total_capacity_bytes) *out_total_capacity_bytes = total_bytes.QuadPart;
            if (out_free_space_bytes) *out_free_space_bytes = free_bytes_to_caller.QuadPart; // Free space available to the caller
        } else {
            char err_detail[128];
            snprintf(err_detail, sizeof(err_detail), "GetDiskFreeSpaceExA failed for drive %c (Error: %lu)", toupper(drive_letter), GetLastError());
            _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, err_detail);
            // Don't return false yet, try to get volume name if requested
        }
    }

    if (out_volume_name && volume_name_len > 0) {
        char volume_name_buffer[MAX_PATH + 1]; // MAX_PATH for volume name is generous
        char file_system_name_buffer[MAX_PATH + 1];
        DWORD volume_serial_number;
        DWORD max_component_length;
        DWORD file_system_flags;

        if (GetVolumeInformationA(
                root_path,
                volume_name_buffer,
                sizeof(volume_name_buffer),
                &volume_serial_number,
                &max_component_length,
                &file_system_flags,
                file_system_name_buffer,
                sizeof(file_system_name_buffer))) {
            strncpy(out_volume_name, volume_name_buffer, volume_name_len - 1);
            out_volume_name[volume_name_len - 1] = '\0';
        } else {
            char err_detail[128];
            snprintf(err_detail, sizeof(err_detail), "GetVolumeInformationA failed for drive %c (Error: %lu)", toupper(drive_letter), GetLastError());
            _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, err_detail);
            out_volume_name[0] = '\0'; // Clear output volume name on error
            // If both GetDiskFreeSpaceExA and GetVolumeInformationA failed, then return false
            if (!out_total_capacity_bytes && !out_free_space_bytes) return false; // if only volume name was requested and failed
            if ( (out_total_capacity_bytes || out_free_space_bytes) && GetLastError() != ERROR_SUCCESS) {
                // if space was also requested and failed, this is an overall failure
                // The check above for GetDiskFreeSpaceExA already set an error.
            }
        }
    } else if (out_volume_name) {
        out_volume_name[0] = '\0'; // No space to write volume name
    }

    // Return true if at least one requested piece of info was successfully retrieved or attempted.
    // A more strict approach would return false if any part fails.
    // For now, let's assume if we got here without an early return, it's "successful enough" unless both GetDiskFreeSpaceExA and GetVolumeInformationA specifically failed and were requested.
    // The error state will hold the latest error.
    return true; // Simplification: if function runs, it's considered a success, caller checks outputs.
                 // A better check: return true only if ALL requested outputs were successfully populated.
                 // For now, if GetDiskFreeSpaceExA fails for requested space info, it's a problem.
                 // If GetVolumeInformationA fails for requested name info, it's a problem.
                 // Let's return based on whether the *last* critical operation succeeded or if nothing critical was requested.
    // Revised logic:
    // if ((out_total_capacity_bytes || out_free_space_bytes) && GetLastError() from GetDiskFreeSpaceExA was not SUCCESS) return false;
    // if (out_volume_name && GetLastError() from GetVolumeInformationA was not SUCCESS) return false;
    // This becomes complex due to GetLastError state. The current code is simpler.
    // Let's assume if we try to get info and it fails, the out params won't be valid, and the error message will be set. The boolean indicates an attempt was made.
}
#endif // _WIN32

/**
 * @brief Asks the operating system to open a file, folder, or URL with its default application.
 * @details This functions like a "double-click". It uses the platform's recommended native APIs for a secure and reliable operation (e.g., ShellExecute on Windows, xdg-open on Linux).
 * @param filePath The path to the file, folder, or a full URL to open.
 */
SITAPI void SituationOpenFile(const char* filePath) {
    if (!filePath || filePath[0] == '\0') {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "File path cannot be null or empty.");
        return;
    }

#if defined(_WIN32)
    // Use ShellExecuteA explicitly to avoid macro expansion issues in C++
    int result = (int)(uintptr_t)ShellExecuteA(NULL, "open", filePath, NULL, NULL, SW_SHOWNORMAL);
    if (result <= 32) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "ShellExecuteA failed to open file or path.");
    }
#elif defined(__APPLE__)
    char command[2048];
    snprintf(command, sizeof(command), "open \"%s\"", filePath);
    if (system(command) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "macOS 'open' command failed.");
    }
#elif defined(__linux__)
    char command[2048];
    snprintf(command, sizeof(command), "xdg-open \"%s\"", filePath);
    if (system(command) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "Linux 'xdg-open' command failed.");
    }
#else
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationOpenFile is not supported on this platform.");
#endif
}


/**
 * @brief Executes a system command hidden from the user, capturing stdout/stderr.
 *
 * @details This function runs a shell command in a hidden manner (no window popup on Windows,
 *          no new terminal on POSIX) and captures the combined output (stdout + stderr).
 *
 * @param cmd The full command line to execute (e.g., "dir C:\\Windows" or "ls -l /tmp").
 *            On Windows, this is passed to `cmd.exe /C`. On POSIX, to `/bin/sh -c`.
 * @param[out] output Pointer to a `char*` that will be allocated with the command output.
 *                    The caller MUST free this string using `SituationFreeString()` or `SIT_FREE()`.
 *                    If output is captured, this pointer is set. If no output or error, it may be NULL or empty string.
 *
 * @return The exit code of the process (0 usually means success).
 * @return -1 if the process failed to launch or execution setup failed. In this case,
 *         `SituationGetLastErrorMsg()` may provide more details.
 */
SITAPI int SituationExecuteCommand(const char *cmd, char **output) {
    if (!cmd || !output) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Cmd or Output pointer is NULL");
        return -1;
    }

    *output = NULL;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "CreatePipe failed");
        return -1;
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "SetHandleInformation failed");
        return -1;
    }

    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE); // Inherit stdin? Or NULL? Snippet used GetStdHandle.
    si.wShowWindow = SW_HIDE;

    // Use cmd.exe /C to interpret the command.
    // We construct "cmd.exe /C \"<cmd>\"" to handle shell features.
    // Length calculation: "cmd.exe /C \"" (13) + cmd len + "\"" (1) + null (1) = len + 15
    size_t cmd_len = strlen(cmd);
    size_t full_len = cmd_len + 32; // Safety margin
    char *cmdline = (char*)SIT_MALLOC(full_len);
    if (!cmdline) {
        CloseHandle(hRead); CloseHandle(hWrite);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate cmdline buffer");
        return -1;
    }
    snprintf(cmdline, full_len, "cmd.exe /C \"%s\"", cmd);

    BOOL success = CreateProcessA(
        NULL,           // Application Name
        cmdline,        // Command Line
        NULL,           // Process Attributes
        NULL,           // Thread Attributes
        TRUE,           // Inherit Handles
        CREATE_NO_WINDOW, // Creation Flags
        NULL,           // Environment
        NULL,           // Current Directory
        &si,            // Startup Info
        &pi             // Process Information
    );

    SIT_FREE(cmdline);
    CloseHandle(hWrite);  // Close write end in parent, otherwise ReadFile blocks forever

    if (!success) {
        CloseHandle(hRead);
        _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "CreateProcessA failed");
        return -1;
    }

    // Read output
    char buf[4096];
    DWORD bytesRead;
    size_t total = 0;

    // Initial empty string allocation so *output is valid even if empty
    *output = (char*)SIT_CALLOC(1, 1);

    while (ReadFile(hRead, buf, sizeof(buf)-1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        char *tmp = (char*)SIT_REALLOC(*output, total + bytesRead + 1);
        if (!tmp) {
            SIT_FREE(*output);
            *output = NULL;
            CloseHandle(hRead);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to realloc output buffer");
            return -1;
        }
        *output = tmp;
        memcpy(*output + total, buf, bytesRead);
        total += bytesRead;
        (*output)[total] = '\0';
    }

    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (int)exit_code;

#else  // Linux & macOS
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "pipe() failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]); close(pipefd[1]);
        _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "fork() failed");
        return -1;
    }

    if (pid == 0) {  // Child
        close(pipefd[0]);  // Close read end

        // Redirect both stdout and stderr to write end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // To prevent any terminal allocation (extra safety on macOS/Linux)
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd != -1) {
            dup2(nullfd, STDIN_FILENO);
            close(nullfd);
        }

        // Use shell to interpret the command
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        // If execl fails
        _exit(127);
    }

    // Parent
    close(pipefd[1]);  // Close write end

    char buf[4096];
    ssize_t bytesRead;
    size_t total = 0;

    // Initial empty string
    *output = (char*)SIT_CALLOC(1, 1);

    while ((bytesRead = read(pipefd[0], buf, sizeof(buf)-1)) > 0) {
        buf[bytesRead] = '\0';
        char *tmp = (char*)SIT_REALLOC(*output, total + bytesRead + 1);
        if (!tmp) {
            SIT_FREE(*output);
            *output = NULL;
            close(pipefd[0]);
            int status;
            waitpid(pid, &status, 0);
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to realloc output buffer");
            return -1;
        }
        *output = tmp;
        memcpy(*output + total, buf, bytesRead);
        total += bytesRead;
        (*output)[total] = '\0';
    }

    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
#endif
}

#endif // SITUATION_IMPL_IO_H
