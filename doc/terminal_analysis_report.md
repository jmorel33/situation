# Terminal Subsystem Analysis Report

## 1. Executive Summary
The `sit/terminal/terminal.h` subsystem is a robust, single-header C library implementing a modern terminal emulator on top of the "Situation" engine. It features a Compute Shader-based rendering pipeline, extensive VT compatibility (VT52 to VT525), and support for advanced features like Sixel graphics and true color.

The codebase successfully compiles under OpenGL 4.3 (Compute Shader support) after applying fixes to struct definitions and macro logic.

## 2. Core Architecture
*   **Pipeline:** Hybrid architecture combining a CPU-based state machine (VT parser) with a GPU-based Compute Shader renderer.
    *   **CPU:** Handles input processing (UTF-8, VT sequences), state management (cursor, modes), and updates the SSBO staging buffer.
    *   **GPU:** A Compute Shader (`TERMINAL_COMPUTE_SHADER_SRC`) processes the SSBO (character/attribute grid) and renders directly to a Storage Image, which is then presented.
*   **Data Structure:**
    *   **Grid:** Linear SSBO array of `GPUCell` structs (char code, FG color, BG color, flags).
    *   **Font:** Texture atlas loaded from `font_data.h` or soft fonts.
    *   **Sixel:** Dedicated overlay texture for graphics.

## 3. Feature Checklist & Status

### 3.1 Emulation Standards
| Feature | Level | Status | Notes |
| :--- | :--- | :--- | :--- |
| **VT52** | Legacy | ✅ Implemented | Full support including graphics mode. |
| **VT100** | Standard | ✅ Implemented | Includes 132-column mode logic (rendering scales). |
| **VT220** | Standard | ✅ Implemented | 8-bit controls, soft fonts (DECDLD). |
| **VT320** | Standard | ✅ Implemented | Sixel graphics, User Defined Keys (DECUDK). |
| **VT420** | Advanced | ✅ Implemented | Rectangular operations (DECCRA, DECFRA, DECERA). |
| **VT520/525** | Modern | ✅ Implemented | ISO color support. |
| **XTERM** | Extension | ✅ Implemented | Mouse tracking, Window ops, 256/True Color. |

### 3.2 Input & Interaction
| Feature | Status | Details |
| :--- | :--- | :--- |
| **Keyboard** | ✅ Robust | Handles modifiers, function keys, and programmable keys. |
| **Mouse Tracking** | ✅ Comprehensive | Supports X10, VT200, SGR (1006), URXVT (1015), and Pixel (1016) modes. |
| **Focus Reporting** | ✅ Implemented | CSI ? 1004 h/l focus in/out events. |
| **Bracketed Paste** | ✅ Implemented | Safe pasting (CSI ? 2004 h/l). |
| **Locator (Mouse)** | ✅ Implemented | DEC Locator (CSI ? 53 n) support. |

### 3.3 Visual & Rendering
| Feature | Status | Details |
| :--- | :--- | :--- |
| **True Color** | ✅ Supported | 24-bit RGB via SGR 38;2 / 48;2. |
| **Text Attributes** | ✅ Complete | Bold, Faint, Italic, Underline (Single/Double), Blink, Reverse, Strike, Conceal. |
| **Sixel Graphics** | ✅ Implemented | Decodes Sixel data to a texture overlay. |
| **Double Height/Width** | ✅ Implemented | DECDHL/DECDWL supported in Compute Shader. |
| **Soft Fonts** | ⚠️ Partial | DECDLD parsing exists; rendering integration is basic. |

### 3.4 Advanced VT Features
| Feature | Status | Details |
| :--- | :--- | :--- |
| **Rectangular Ops** | ✅ Implemented | Copy, Fill, Erase, Selective Erase rectangular areas. |
| **Protection** | ✅ Implemented | DECSCA (Select Character Protection Attribute). |
| **Multiple Pages** | ❌ Missing | Only primary and alternate screen buffers supported. |
| **Printer Port** | ⚠️ Partial | MC (Media Copy) commands parsed; output simulated/callback. |

## 4. Nuances & Implementation Details
1.  **Compute Shader Dependency:** Rendering *requires* a backend capable of Compute Shaders (OpenGL 4.3+ or Vulkan 1.0+). It does not use standard triangle rasterization for text.
2.  **SSBO Layout:** The `GPUCell` struct layout must strictly match between C and GLSL (std430 layout).
3.  **UTF-8:** The internal buffer stores `uint32_t` codepoints. The font atlas is currently hardcoded to CP437 (256 chars), so true Unicode rendering requires implementing a dynamic glyph cache or larger texture.
4.  **Integration:** The library heavily relies on `Situation` for timing (`SituationTimerGetTime`), input (`SituationGetKeyPressed`), and resource management.

## 5. Compilation Status
*   **Original State:** Failed due to duplicated `GPUCell` definitions and invalid preprocessor macro nesting for shaders.
*   **Current State:** **FIXED**. Compiles successfully with `gcc -c`.
*   **Validation:** Verified compilation of `demo_terminal.c` with `SITUATION_USE_OPENGL`.

## 6. Recommendations
*   **Font Expansion:** Upgrade the font system to support full Unicode ranges (e.g., via `stb_truetype` atlas baking already present in Situation) instead of the static `font_data.h`.
*   **Performance:** Monitor SSBO upload costs (`SituationUpdateBuffer`) on high-resolution displays. Consider partial updates.
