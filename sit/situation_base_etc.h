#ifndef SITUATION_BASE_ETC_H
#define SITUATION_BASE_ETC_H

// ================================================================================================
// 16 STANDARD ANSI / VGA TERMINAL COLORS
// ================================================================================================

// --- Standard / Dark Colors (ANSI 0-7) ---
#define SIT_COLOR_BLACK           ((ColorRGBA){   0,   0,   0, 255 }) // ANSI 0
#define SIT_COLOR_RED             ((ColorRGBA){ 170,   0,   0, 255 }) // ANSI 1
#define SIT_COLOR_GREEN           ((ColorRGBA){   0, 170,   0, 255 }) // ANSI 2
#define SIT_COLOR_YELLOW          ((ColorRGBA){ 170,  85,   0, 255 }) // ANSI 3 (Brown/Dark Yellow)
#define SIT_COLOR_BLUE            ((ColorRGBA){   0,   0, 170, 255 }) // ANSI 4
#define SIT_COLOR_MAGENTA         ((ColorRGBA){ 170,   0, 170, 255 }) // ANSI 5
#define SIT_COLOR_CYAN            ((ColorRGBA){   0, 170, 170, 255 }) // ANSI 6
#define SIT_COLOR_WHITE           ((ColorRGBA){ 170, 170, 170, 255 }) // ANSI 7 (Light Gray)

// --- Bright / Intense Colors (ANSI 8-15) ---
#define SIT_COLOR_BRIGHT_BLACK    ((ColorRGBA){  85,  85,  85, 255 }) // ANSI 8 (Dark Gray)
#define SIT_COLOR_BRIGHT_RED      ((ColorRGBA){ 255,  85,  85, 255 }) // ANSI 9
#define SIT_COLOR_BRIGHT_GREEN    ((ColorRGBA){  85, 255,  85, 255 }) // ANSI 10
#define SIT_COLOR_BRIGHT_YELLOW   ((ColorRGBA){ 255, 255,  85, 255 }) // ANSI 11
#define SIT_COLOR_BRIGHT_BLUE     ((ColorRGBA){  85,  85, 255, 255 }) // ANSI 12
#define SIT_COLOR_BRIGHT_MAGENTA  ((ColorRGBA){ 255,  85, 255, 255 }) // ANSI 13
#define SIT_COLOR_BRIGHT_CYAN     ((ColorRGBA){  85, 255, 255, 255 }) // ANSI 14
#define SIT_COLOR_BRIGHT_WHITE    ((ColorRGBA){ 255, 255, 255, 255 }) // ANSI 15

// ================================================================================================
// PURE DIGITAL COLORS (Optional, for general 2D Graphics)
// ================================================================================================
#define SIT_COLOR_PURE_RED        ((ColorRGBA){ 255,   0,   0, 255 })
#define SIT_COLOR_PURE_GREEN      ((ColorRGBA){   0, 255,   0, 255 })
#define SIT_COLOR_PURE_BLUE       ((ColorRGBA){   0,   0, 255, 255 })
#define SIT_COLOR_PURE_YELLOW     ((ColorRGBA){ 255, 255,   0, 255 })
#define SIT_COLOR_PURE_MAGENTA    ((ColorRGBA){ 255,   0, 255, 255 })
#define SIT_COLOR_PURE_CYAN       ((ColorRGBA){   0, 255, 255, 255 })
#define SIT_COLOR_TRANSPARENT     ((ColorRGBA){   0,   0,   0,   0 })

// ================================================================================================
// KEY CODES (from GLFW, re-defined for API stability)
// ================================================================================================
#define SIT_KEY_SPACE              32
#define SIT_KEY_APOSTROPHE         39  /* ' */
#define SIT_KEY_COMMA              44  /* , */
#define SIT_KEY_MINUS              45  /* - */
#define SIT_KEY_PERIOD             46  /* . */
#define SIT_KEY_SLASH              47  /* / */
#define SIT_KEY_0                  48
#define SIT_KEY_1                  49
#define SIT_KEY_2                  50
#define SIT_KEY_3                  51
#define SIT_KEY_4                  52
#define SIT_KEY_5                  53
#define SIT_KEY_6                  54
#define SIT_KEY_7                  55
#define SIT_KEY_8                  56
#define SIT_KEY_9                  57
#define SIT_KEY_SEMICOLON          59  /* ; */
#define SIT_KEY_EQUAL              61  /* = */
#define SIT_KEY_A                  65
#define SIT_KEY_B                  66
#define SIT_KEY_C                  67
#define SIT_KEY_D                  68
#define SIT_KEY_E                  69
#define SIT_KEY_F                  70
#define SIT_KEY_G                  71
#define SIT_KEY_H                  72
#define SIT_KEY_I                  73
#define SIT_KEY_J                  74
#define SIT_KEY_K                  75
#define SIT_KEY_L                  76
#define SIT_KEY_M                  77
#define SIT_KEY_N                  78
#define SIT_KEY_O                  79
#define SIT_KEY_P                  80
#define SIT_KEY_Q                  81
#define SIT_KEY_R                  82
#define SIT_KEY_S                  83
#define SIT_KEY_T                  84
#define SIT_KEY_U                  85
#define SIT_KEY_V                  86
#define SIT_KEY_W                  87
#define SIT_KEY_X                  88
#define SIT_KEY_Y                  89
#define SIT_KEY_Z                  90
#define SIT_KEY_LEFT_BRACKET       91  /* [ */
#define SIT_KEY_BACKSLASH          92  /* \ */
#define SIT_KEY_RIGHT_BRACKET      93  /* ] */
#define SIT_KEY_GRAVE_ACCENT       96  /* ` */
#define SIT_KEY_WORLD_1            161 /* non-US #1 */
#define SIT_KEY_WORLD_2            162 /* non-US #2 */

// --- Function keys ---
#define SIT_KEY_ESCAPE             256
#define SIT_KEY_ENTER              257
#define SIT_KEY_TAB                258
#define SIT_KEY_BACKSPACE          259
#define SIT_KEY_INSERT             260
#define SIT_KEY_DELETE             261
#define SIT_KEY_RIGHT              262
#define SIT_KEY_LEFT               263
#define SIT_KEY_DOWN               264
#define SIT_KEY_UP                 265
#define SIT_KEY_PAGE_UP            266
#define SIT_KEY_PAGE_DOWN          267
#define SIT_KEY_HOME               268
#define SIT_KEY_END                269
#define SIT_KEY_CAPS_LOCK          280
#define SIT_KEY_SCROLL_LOCK        281
#define SIT_KEY_NUM_LOCK           282
#define SIT_KEY_PRINT_SCREEN       283
#define SIT_KEY_PAUSE              284
#define SIT_KEY_F1                 290
#define SIT_KEY_F2                 291
#define SIT_KEY_F3                 292
#define SIT_KEY_F4                 293
#define SIT_KEY_F5                 294
#define SIT_KEY_F6                 295
#define SIT_KEY_F7                 296
#define SIT_KEY_F8                 297
#define SIT_KEY_F9                 298
#define SIT_KEY_F10                299
#define SIT_KEY_F11                300
#define SIT_KEY_F12                301
#define SIT_KEY_F13                302
#define SIT_KEY_F14                303
#define SIT_KEY_F15                304
#define SIT_KEY_F16                305
#define SIT_KEY_F17                306
#define SIT_KEY_F18                307
#define SIT_KEY_F19                308
#define SIT_KEY_F20                309
#define SIT_KEY_F21                310
#define SIT_KEY_F22                311
#define SIT_KEY_F23                312
#define SIT_KEY_F24                313
#define SIT_KEY_F25                314

// --- Keypad keys ---
#define SIT_KEY_KP_0               320
#define SIT_KEY_KP_1               321
#define SIT_KEY_KP_2               322
#define SIT_KEY_KP_3               323
#define SIT_KEY_KP_4               324
#define SIT_KEY_KP_5               325
#define SIT_KEY_KP_6               326
#define SIT_KEY_KP_7               327
#define SIT_KEY_KP_8               328
#define SIT_KEY_KP_9               329
#define SIT_KEY_KP_DECIMAL         330
#define SIT_KEY_KP_DIVIDE          331
#define SIT_KEY_KP_MULTIPLY        332
#define SIT_KEY_KP_SUBTRACT        333
#define SIT_KEY_KP_ADD             334
#define SIT_KEY_KP_ENTER           335
#define SIT_KEY_KP_EQUAL           336

// --- Modifier keys (positional) ---
#define SIT_KEY_LEFT_SHIFT         340
#define SIT_KEY_LEFT_CONTROL       341
#define SIT_KEY_LEFT_ALT           342
#define SIT_KEY_LEFT_SUPER         343 // Windows/Command/Meta key
#define SIT_KEY_RIGHT_SHIFT        344
#define SIT_KEY_RIGHT_CONTROL      345
#define SIT_KEY_RIGHT_ALT          346
#define SIT_KEY_RIGHT_SUPER        347
#define SIT_KEY_MENU               348

// --- Modifier Bitmasks ---
#define SIT_MOD_SHIFT              0x0001
#define SIT_MOD_CONTROL            0x0002
#define SIT_MOD_ALT                0x0004
#define SIT_MOD_SUPER              0x0008
#define SIT_MOD_CAPS_LOCK          0x0010
#define SIT_MOD_NUM_LOCK           0x0020

// ================================================================================================
// MIDI note to Frequency hz table
// ================================================================================================
static const float SITUATION_MIDI_NOTE_FREQUENCY[128] = {
    8.1758f,   8.66196f,  9.17702f,  9.72272f,  10.3009f,  10.9134f,  11.5623f,  12.2499f,  12.9783f,  13.75f,    14.5676f,  15.4339f,
    16.3516f,  17.3239f,  18.3540f,  19.4454f,  20.6017f,  21.8268f,  23.1247f,  24.4997f,  25.9565f,  27.5f,     29.1352f,  30.8677f,
    32.7032f,  34.6478f,  36.7081f,  38.8909f,  41.2034f,  43.6535f,  46.2493f,  48.9994f,  51.9131f,  55.0f,     58.2705f,  61.7354f,
    65.4064f,  69.2957f,  73.4162f,  77.7817f,  82.4069f,  87.3071f,  92.4986f,  97.9989f,  103.826f,  110.0f,    116.541f,  123.471f,
    130.813f,  138.591f,  146.832f,  155.563f,  164.814f,  174.614f,  184.997f,  195.998f,  207.652f,  220.0f,    233.082f,  246.942f,
    261.626f,  277.183f,  293.665f,  311.127f,  329.628f,  349.228f,  369.994f,  391.995f,  415.305f,  440.0f,    466.164f,  493.883f,
    523.251f,  554.365f,  587.330f,  622.254f,  659.255f,  698.456f,  739.989f,  783.991f,  830.609f,  880.0f,    932.328f,  987.767f,
    1046.50f,  1108.73f,  1174.66f,  1244.51f,  1318.51f,  1396.91f,  1479.98f,  1567.98f,  1661.22f,  1760.0f,   1864.66f,  1975.53f,
    2093.00f,  2217.46f,  2349.32f,  2489.02f,  2637.02f,  2793.83f,  2959.96f,  3135.96f,  3322.44f,  3520.0f,   3729.31f,  3951.07f,
    4186.01f,  4434.92f,  4698.64f,  4978.03f,  5274.04f,  5587.65f,  5919.91f,  6271.93f,  6644.88f,  7040.0f,   7458.62f,  7902.13f,
    8372.02f,  8869.84f,  9397.27f,  9956.06f,  10548.1f,  11175.3f,  11839.8f,  12543.9f
};

#endif // SITUATION_BASE_ETC_H
