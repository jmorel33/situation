/***************************************************************************************************
*
*   situation_impl_etc.h - Miscellaneous Utilities & Helpers
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   The "et cetera" file. Contains small utility functions, math helpers,
*   string helpers, and platform abstractions that don't belong to any
*   single module but are used across the implementation.
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_ETC_H
#define SITUATION_IMPL_ETC_H

// --- Math Helpers ---
static inline float _SituationClampf(float value, float min, float max) { const float t = value < min ? min : value; return t > max ? max : t; }
static inline float _SituationLerpf(float a, float b, float t) { return a + t * (b - a); }
static inline float _SituationFMin3(float a, float b, float c) { return fminf(a, fminf(b, c)); }
static inline float _SituationFMax3(float a, float b, float c) { return fmaxf(a, fmaxf(b, c)); }

// --- String Helpers ---

/**
 * @brief [INTERNAL] Standard C11 replacement for strdup.
 */
static char* _sit_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)SIT_MALLOC(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/**
 * @brief [INTERNAL] Simple implementation of dirname (get parent directory).
 */
static char* _sit_dirname(const char* path) {
    if (!path) return NULL;
    char* copy = _sit_strdup(path);
    if (!copy) return NULL;

    char* last_slash = strrchr(copy, '/');
    char* last_backslash = strrchr(copy, '\\');
    char* end = (last_slash > last_backslash) ? last_slash : last_backslash;

    if (end) {
        *end = '\0';
    } else {
        free(copy);
        return _sit_strdup(".");
    }
    return copy;
}

/**
 * @brief [INTERNAL] Alias for SituationDirectoryExists used in internal helpers.
 * HARDENING: bool by design — existence query, not a failure path.
 */
static bool _sit_directory_exists(const char* dir_path) {
    return SituationDirectoryExists(dir_path);
}

// --- Simple string hashing function (djb2) ---
static unsigned long _sit_hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief [INTERNAL] Standard C11 replacement for strcasecmp/_stricmp.
 */
static int _sit_strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/**
 * @brief Frees the memory for a string allocated and returned by the Situation library.
 * @details Must be used to free any `char*` returned by functions like
 *          `SituationGetLastErrorMsg()`, `SituationGetBasePath()`, etc.
 * @param str A pointer to the string to be freed. It is safe to pass NULL.
 */
SITAPI void SituationFreeString(char* str) {
    if (str) {
        SIT_FREE(str);
    }
}

#endif // SITUATION_IMPL_ETC_H
