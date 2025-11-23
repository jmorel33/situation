# C11 Compliance Report: Situation SDK

The `README.md` claims that the "Situation" library is written in "Strict C11 (ISO/IEC 9899:2011)". However, an analysis of `situation.h` reveals several critical violations of the C11 standard, ranging from syntax errors to architectural incompatibilities.

The following issues render the library incompatible with a strict C11 compiler (e.g., `gcc -std=c11 -pedantic`).

## 1. Illegal Return Types (Array Returns)
**Location:** `SituationGetMousePosition` (Line 1750), `SituationGetMouseDelta`, `SituationGetMouseWheelMoveV`
**Issue:** The library uses `cglm/cglm.h` types in its public API. Specifically, `vec2` is defined by `cglm` as `typedef float vec2[2];`. In C, functions are strictly forbidden from returning array types.
**Code:**
```c
SITAPI vec2 SituationGetMousePosition(void); // Error: function cannot return array type 'vec2'
```
**Violation:** ISO/IEC 9899:2011 §6.9.1.3: "The return type of a function shall not be an array type."

## 2. Non-Standard Macro Extensions
**Location:** `SITUATION_LOG_WARNING` (Line 207)
**Issue:** The macro uses the `##__VA_ARGS__` syntax to swallow the comma when no variadic arguments are provided. This is a GCC extension and is not part of standard C11.
**Code:**
```c
#define SITUATION_LOG_WARNING(code, msg, ...) do { \
    // ...
    snprintf(_sit_err_buf, sizeof(_sit_err_buf), msg, ##__VA_ARGS__); \
    // ...
} while(0)
```
**Violation:** C11 requires at least one argument for the ellipsis `...` if the comma is present. The `##` operator behavior for comma suppression is non-standard.

## 3. Invalid Pointer Access Syntax
**Location:** `SituationDestroyTexture` (Line 4425)
**Issue:** The function receives a pointer `SituationTexture* texture` but attempts to access its member `id` using the dot operator `.` instead of the arrow operator `->`.
**Code:**
```c
SITAPI void SituationDestroyTexture(SituationTexture* texture) {
    // ...
    if (!texture.id) { // Error: 'texture' is a pointer; did you mean to use '->'?
        // ...
    }
```
**Violation:** Syntax error.

## 4. Missing Function Arguments
**Location:** `SituationSetClipboardText` (Line 4099)
**Issue:** The implementation calls `glfwSetClipboardString` but fails to pass the actual text string argument.
**Code:**
```c
SITAPI void SituationSetClipboardText(const char* text) {
    // ...
    glfwSetClipboardString(sit_gs.sit_glfw_window); // Error: too few arguments
}
```
**Violation:** Constraint violation (mismatched arguments).

## 5. Accessing Undefined Struct Members
**Location:** `SituationCmdBindDescriptorSet` (Line 3227)
**Issue:** The code attempts to access `buffer.abstract_usage_flags`, but the `SituationBuffer` struct definition (Line 796) does not contain this member.
**Code:**
```c
// Line 3227
if (buffer.abstract_usage_flags & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) { ... }

// Line 796 (Definition)
typedef struct {
    uint64_t id;
    size_t size_in_bytes;
    // abstract_usage_flags is missing here
    // ...
} SituationBuffer;
```
**Violation:** Accessing a non-existent member.

## 6. C++ Dependency in C Code
**Location:** `VMA_IMPLEMENTATION` (Line 1970)
**Issue:** The Vulkan backend includes `vk_mem_alloc.h` (Vulkan Memory Allocator). VMA is primarily a C++ library. While it has a C interface, simply defining `VMA_IMPLEMENTATION` inside a C file often fails to compile with strict C compilers because VMA implementation code uses C++ features (classes, templates) unless very specific C-compatibility configuration macros are used (which are not present here).
**Violation:** Usage of C++ implementation code within a translation unit compiled as C11.
