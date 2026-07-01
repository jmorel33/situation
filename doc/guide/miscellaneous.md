## Miscellaneous Module

**Overview:** This module includes powerful utilities like the Temporal Oscillator System for rhythmic timing, a suite of color space conversion functions (RGBA, HSV, YPQA), and essential memory management helpers for data allocated by the library.

### Structs and Enums

#### `ColorRGBA`
Represents a color in the Red, Green, Blue, Alpha color space. Each component is an 8-bit unsigned integer (0-255).
```c
typedef struct ColorRGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} ColorRGBA;
```

---
#### `ColorHSVA`
Represents a color in the Hue, Saturation, Value, Alpha color space.
```c
typedef struct ColorHSVA {
    float h; // Hue (0-360)
    float s; // Saturation (0-1)
    float v; // Value (0-1)
    float a; // Alpha (0-1)
} ColorHSVA;
```

---
#### `ColorYPQA`
Represents a color in NTSC-style YPQ space (luma, phase, chroma amplitude), packed as 8-bit bytes per channel.
```c
typedef struct ColorYPQA {
    unsigned char y; // Luma (0–255)
    unsigned char p; // Phase / hue wheel position (0–255 → 0–360°)
    unsigned char q; // Chroma amplitude (0–255)
    unsigned char a; // Alpha (0–255)
} ColorYPQA;
```

---
#### `ColorYPQf`
Normalized float edit space for YPQ grading without 8-bit quantization during manipulation. All components are in **[0, 1]**; `p` is phase (0 → full hue wheel).
```c
typedef struct ColorYPQf {
    float y, p, q, a;
} ColorYPQf;
```
Use `SituationYpqQuantize()` when exporting to `ColorYPQA` or GPU RGBA.

### Functions

#### Temporal Oscillator System
---
#### `SituationTimerHasOscillatorUpdated`
Checks if an oscillator has just completed a cycle in the current frame. This is a single-frame trigger, ideal for synchronizing events to a rhythmic beat.
```c
bool SituationTimerHasOscillatorUpdated(int oscillator_id);
```
**Usage Example:**
```c
// In Init, create an oscillator with a 0.5s period (120 BPM).
double period = 0.5;
init_info.oscillator_count = 1;
init_info.oscillator_periods = &period;

// In the Update loop, check for the beat.
if (SituationTimerHasOscillatorUpdated(0)) {
    printf("Beat!\n");
    // Play a drum sound, flash a light, or trigger a game event.
}
```
---
#### `SituationSetTimerOscillatorPeriod`
Sets a new period for an oscillator at runtime, allowing you to change the tempo of rhythmic events dynamically.
```c
SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```
---
#### `SituationTimerGetOscillatorValue` / `SituationTimerGetOscillatorPhase`
Gets the current value (typically 0.0 to 1.0) or phase (0.0 to 2*PI) of an oscillator.
```c
double SituationTimerGetOscillatorValue(int oscillator_id);
double SituationTimerGetOscillatorPhase(int oscillator_id);
```
**Usage Example:**
```c
// Use an oscillator to create a smooth pulsing animation
double pulse = SituationTimerGetOscillatorValue(0); // This will smoothly go 0 -> 1 -> 0
float scale = 1.0f + (float)pulse * 0.2f;
// Apply 'scale' to a transform
```

---
#### `SituationTimerGetTime`
`double SituationTimerGetTime(void);`
Gets the total time elapsed since the library was initialized. This function returns the master application time, updated once per frame by `SituationUpdateTimers()`. It serves as the high-resolution monotonic clock for the entire application and is the basis for all other timing functions, including the Temporal Oscillator system.

**Usage Example:**
```c
// Get the total elapsed time to drive a procedural animation.
double totalTime = SituationTimerGetTime();
float pulse = sinf((float)totalTime * 2.0f); // A simple pulsing effect over time.
// Use 'pulse' to modify an object's color, size, etc.
```
---
#### Color Space Conversions
---
#### `SituationRgbToHsv` / `SituationHsvToRgb`
Converts a color between RGBA and HSV (Hue, Saturation, Value) color spaces.
```c
ColorHSV SituationRgbToHsv(ColorRGBA rgb);
ColorRGBA SituationHsvToRgb(ColorHSV hsv);
```
**Usage Example:**
```c
// Create a rainbow effect by cycling the hue
ColorHSV hsv_color = {.h = fmodf(SituationTimerGetTime() * 50.0f, 360.0f), .s = 1.0, .v = 1.0};
ColorRGBA final_color = SituationHsvToRgb(hsv_color);
```

---
#### `SituationTimerGetOscillatorState`
Gets the current binary state (0 or 1) of an oscillator. Oscillators toggle between 0 and 1 at their configured period.

```c
bool SituationTimerGetOscillatorState(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Current state (false = 0, true = 1)

**Usage Example:**
```c
// Blinking light effect
bool blink_state = SituationTimerGetOscillatorState(0);  // 1Hz oscillator
if (blink_state) {
    DrawLight(position, RED);
}

// Alternating animation
if (SituationTimerGetOscillatorState(1)) {
    DrawSprite(sprite_frame_a);
} else {
    DrawSprite(sprite_frame_b);
}

// Rhythmic game mechanic
if (SituationTimerGetOscillatorState(2)) {
    // Beat is "on"
    EnablePowerup();
} else {
    // Beat is "off"
    DisablePowerup();
}
```

**Notes:**
- Returns current state, not edge detection
- Use `SituationTimerHasOscillatorUpdated()` for edge detection
- State toggles at configured period

---
#### `SituationTimerGetPreviousOscillatorState`
Gets the previous frame's state of an oscillator. Useful for detecting state transitions (edges).

```c
bool SituationTimerGetPreviousOscillatorState(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Previous frame's state (false = 0, true = 1)

**Usage Example:**
```c
// Detect rising edge (0 -> 1 transition)
bool current = SituationTimerGetOscillatorState(0);
bool previous = SituationTimerGetPreviousOscillatorState(0);

if (current && !previous) {
    // Rising edge detected
    PlayBeatSound();
    TriggerEffect();
}

// Detect falling edge (1 -> 0 transition)
if (!current && previous) {
    // Falling edge detected
    StopEffect();
}

// Detect any change
if (current != previous) {
    // State changed
    OnBeatChange();
}
```

**Notes:**
- Use with current state for edge detection
- Alternative: use `SituationTimerHasOscillatorUpdated()`
- Updated once per frame

---
#### `SituationTimerPingOscillator`
Checks if an oscillator's period has elapsed since the last ping. This is a "manual" oscillator that only advances when you check it.

```c
bool SituationTimerPingOscillator(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** `true` if period elapsed since last ping, `false` otherwise

**Usage Example:**
```c
// Spawn enemy every 2 seconds
if (SituationTimerPingOscillator(5)) {  // 0.5Hz = 2 second period
    SpawnEnemy();
}

// Auto-save every 30 seconds
if (SituationTimerPingOscillator(6)) {  // 1/30 Hz
    AutoSave();
}

// Periodic update that only runs when needed
if (SituationTimerPingOscillator(7)) {
    UpdateSlowSystem();
}

// Get progress between pings
double progress = SituationTimerGetPingProgress(7);
DrawProgressBar(progress);  // 0.0 to 1.0
```

**Notes:**
- Only advances when you call it
- Returns true once per period
- Use `SituationTimerGetPingProgress()` for interpolation
- Different from automatic oscillators

---
#### `SituationTimerGetOscillatorTriggerCount`
Gets the total number of times an oscillator has triggered (transitioned to state 1) since initialization.

```c
uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Total trigger count

**Usage Example:**
```c
// Count beats in a rhythm game
uint64_t beat_count = SituationTimerGetOscillatorTriggerCount(0);
printf("Beat: %llu\n", beat_count);

// Trigger event every N beats
if (beat_count % 4 == 0) {
    // Every 4th beat
    PlayAccentSound();
}

// Track total game time in beats
uint64_t total_beats = SituationTimerGetOscillatorTriggerCount(1);
double game_time_in_beats = (double)total_beats;

// Reset detection (if oscillator was reset)
static uint64_t last_count = 0;
uint64_t current_count = SituationTimerGetOscillatorTriggerCount(2);
if (current_count < last_count) {
    printf("Oscillator was reset\n");
}
last_count = current_count;
```

**Notes:**
- Increments on each 0->1 transition
- Never decreases (unless oscillator is reset)
- Useful for beat counting and rhythm games

---
#### `SituationTimerGetOscillatorPeriod`
Gets the period of an oscillator in seconds. The period is the time for one complete cycle (0->1->0).

```c
double SituationTimerGetOscillatorPeriod(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Period in seconds

**Usage Example:**
```c
// Display BPM from oscillator period
double period = SituationTimerGetOscillatorPeriod(0);
double bpm = 60.0 / period;
printf("Tempo: %.1f BPM\n", bpm);

// Calculate frequency
double frequency = 1.0 / period;
printf("Frequency: %.2f Hz\n", frequency);

// Sync animation speed to oscillator
double period = SituationTimerGetOscillatorPeriod(1);
float animation_speed = 1.0f / (float)period;
UpdateAnimation(animation_speed);
```

**Notes:**
- Returns period in seconds
- Frequency = 1.0 / period
- Use `SituationSetTimerOscillatorPeriod()` to change

---
#### `SituationTimerGetPingProgress`
Gets progress [0.0 to 1.0] of the interval since the last successful ping. Useful for interpolation between ping events.

```c
double SituationTimerGetPingProgress(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Progress from 0.0 (just pinged) to 1.0 (about to ping)

**Usage Example:**
```c
// Smooth progress bar for auto-save
double progress = SituationTimerGetPingProgress(5);
DrawProgressBar(progress);  // 0% to 100%

// Interpolate between spawn points
double t = SituationTimerGetPingProgress(6);
if (SituationTimerPingOscillator(6)) {
    SpawnEnemy();
}
// Show spawn warning as progress approaches 1.0
if (t > 0.8) {
    DrawSpawnWarning(t);  // Fade in warning
}

// Breathing animation
double breath = SituationTimerGetPingProgress(7);
float scale = 1.0f + 0.1f * sinf(breath * 2.0f * PI);
DrawSprite(position, scale);
```

**Notes:**
- Returns 0.0 immediately after ping
- Approaches 1.0 as next ping nears
- Can exceed 1.0 if ping is missed
- Use for smooth interpolation

---
#### `SituationSetTimerOscillatorPeriod`
Sets a new period for an oscillator at runtime, allowing you to change the tempo or frequency dynamically.

```c
void SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)
- `period_seconds` - New period in seconds

**Usage Example:**
```c
// Change tempo based on game state
if (boss_fight_started) {
    // Speed up to 140 BPM
    double period = 60.0 / 140.0;  // ~0.428 seconds
    SituationSetTimerOscillatorPeriod(0, period);
}

// Gradual tempo increase
static double current_bpm = 120.0;
current_bpm += 0.1;  // Increase by 0.1 BPM per frame
double period = 60.0 / current_bpm;
SituationSetTimerOscillatorPeriod(1, period);

// Sync to music tempo
double music_bpm = GetCurrentMusicBPM();
double beat_period = 60.0 / music_bpm;
SituationSetTimerOscillatorPeriod(2, beat_period);

// Slow down over time
double current_period = SituationTimerGetOscillatorPeriod(3);
SituationSetTimerOscillatorPeriod(3, current_period * 1.01);  // 1% slower
```

**Notes:**
- Changes take effect immediately
- Does not reset trigger count
- Period must be > 0.0
- Use for dynamic tempo changes

---
#### `SituationConvertColorToVec4`
Converts an 8-bit RGBA color (0-255 per channel) to a normalized vec4 (0.0-1.0 per channel). Useful for passing colors to shaders.

```c
void SituationConvertColorToVec4(ColorRGBA c, vec4 out_normalized_color);
```

**Parameters:**
- `c` - Input color with 8-bit channels (0-255)
- `out_normalized_color` - Output vec4 with normalized channels (0.0-1.0)

**Usage Example:**
```c
// Convert color for shader uniform
ColorRGBA tint_color = {255, 128, 64, 255};  // Orange
vec4 normalized_color;
SituationConvertColorToVec4(tint_color, normalized_color);

// Pass to shader
SituationSetShaderUniform(shader, "u_tint_color", normalized_color, SITUATION_UNIFORM_VEC4);

// Or use in push constant
struct PushConstants {
    vec4 color;
    mat4 mvp;
} pc;
SituationConvertColorToVec4(tint_color, pc.color);
SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
```

**Notes:**
- Converts from [0, 255] to [0.0, 1.0] range
- Alpha channel is also normalized
- Useful for shader uniforms and GPU data

---
#### `SituationHsvToRgb`
Converts a color from HSV (Hue, Saturation, Value) color space to RGB color space. HSV is more intuitive for color manipulation and animation.

```c
ColorRGBA SituationHsvToRgb(ColorHSV hsv);
```

**Parameters:**
- `hsv` - Input color in HSV space (H: 0-360°, S: 0.0-1.0, V: 0.0-1.0)

**Returns:** Color in RGBA space (0-255 per channel)

**Usage Example:**
```c
// Create a rainbow effect by animating hue
float time = SituationGetTime();
ColorHSV hsv_color = {
    .h = fmodf(time * 60.0f, 360.0f),  // Cycle through hues
    .s = 1.0f,                          // Full saturation
    .v = 1.0f                           // Full brightness
};
ColorRGBA rgb_color = SituationHsvToRgb(hsv_color);
DrawRectangle(100, 100, 200, 200, rgb_color);

// Create color variations
ColorHSV base_hsv = {180.0f, 0.8f, 0.9f};  // Cyan-ish

// Lighter version (increase value)
ColorHSV light_hsv = base_hsv;
light_hsv.v = 1.0f;
ColorRGBA light_color = SituationHsvToRgb(light_hsv);

// Darker version (decrease value)
ColorHSV dark_hsv = base_hsv;
dark_hsv.v = 0.5f;
ColorRGBA dark_color = SituationHsvToRgb(dark_hsv);

// Desaturated version (decrease saturation)
ColorHSV gray_hsv = base_hsv;
gray_hsv.s = 0.2f;
ColorRGBA gray_color = SituationHsvToRgb(gray_hsv);
```

**Notes:**
- HSV is more intuitive for color picking and animation
- Hue wraps around (360° = 0°)
- Saturation 0.0 = grayscale, 1.0 = full color
- Value 0.0 = black, 1.0 = full brightness

---
#### `SituationColorToYPQ`
Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature, Alpha) color space. This is a custom color space useful for signal processing-like effects and color manipulation.

```c
ColorYPQA SituationColorToYPQ(ColorRGBA color);
```

**Parameters:**
- `color` - Input color in RGBA space

**Returns:** Color in YPQA space

**Usage Example:**
```c
// Convert to YPQ for manipulation
ColorRGBA original = {200, 100, 50, 255};
ColorYPQA ypq = SituationColorToYPQ(original);

// Adjust luma (brightness) without affecting hue
ypq.y *= 1.5f;  // Increase brightness

// Convert back to RGB
ColorRGBA brightened = SituationColorFromYPQ(ypq);

// Or manipulate phase/quadrature for color shifts
ColorYPQA ypq2 = SituationColorToYPQ(original);
ypq2.p += 0.2f;  // Shift phase
ypq2.q -= 0.1f;  // Shift quadrature
ColorRGBA shifted = SituationColorFromYPQ(ypq2);
```

**Notes:**
- Y = Luma (brightness)
- P = Phase (color component)
- Q = Quadrature (color component)
- Useful for advanced color grading and effects
- Less common than HSV but useful for specific effects

---
#### `SituationColorFromYPQ`
Converts a color from YPQA (Luma, Phase, Quadrature, Alpha) color space back to standard RGBA color space.

```c
ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color);
```

**Parameters:**
- `ypq_color` - Input color in YPQA space

**Returns:** Color in RGBA space (0-255 per channel)

**Usage Example:**
```c
// Create a color effect by manipulating YPQ components
void ApplyRetroEffect(ColorRGBA* pixels, int count) {
    for (int i = 0; i < count; i++) {
        // Convert to YPQ
        ColorYPQA ypq = SituationColorToYPQ(pixels[i]);
        
        // Reduce luma for darker look
        ypq.y *= 0.8f;
        
        // Shift phase for color tint
        ypq.p += 0.1f;
        
        // Convert back
        pixels[i] = SituationColorFromYPQ(ypq);
    }
}

// Use in image processing
SituationImage img = SituationLoadImage("photo.png");
ApplyRetroEffect((ColorRGBA*)img.data, img.width * img.height);
SituationSaveImage(img, "photo_retro.png");
```

**Notes:**
- Inverse of `SituationColorToYPQ()`
- Useful for color grading pipelines
- Allows independent manipulation of brightness and color

---
#### YPQ pixel manipulation (HSV parity)
Byte-space helpers for per-pixel YPQ math. Prefer these over manual channel arithmetic so phase wrap and chroma scaling stay consistent with `SituationColorToYPQ` / `FromYPQ`.

```c
ColorYPQA SituationYpqLerp(ColorYPQA a, ColorYPQA b, float t);
ColorYPQA SituationYpqAdjustLuma(ColorYPQA color, float luma_factor);
ColorYPQA SituationYpqAdjustPhase(ColorYPQA color, int phase_shift);
ColorYPQA SituationYpqAdjustChroma(ColorYPQA color, float chroma_factor);
float SituationYpqGetLuma(ColorYPQA color);
float SituationYpqGetHueDegrees(ColorYPQA color);
float SituationYpqGetChroma(ColorYPQA color);
float SituationYpqDistance(ColorYPQA a, ColorYPQA b);
bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tolerance);
```

- **Lerp:** `P` uses shortest arc on the hue wheel (same semantics as circular HSV hue)
- **AdjustPhase:** `phase_shift` is in byte steps (mod 256), not degrees
- **GetHueDegrees:** maps `P` to degrees `[0, 360)`

---
#### Float YPQ path
For grading pipelines that need sub-byte precision before export:

```c
ColorYPQf SituationColorToYPQf(ColorRGBA color);
ColorRGBA SituationColorFromYPQf(ColorYPQf ypq);
ColorYPQA SituationYpqQuantize(ColorYPQf ypq);
ColorYPQf SituationYpqClampInGamut(ColorYPQf ypq);
```

- **ToYPQf / FromYPQf:** linear NTSC YIQ internally; RGB output is clamped to display gamut
- **Quantize:** pack float YPQ to 8-bit `ColorYPQA` for storage or legacy byte APIs
- **ClampInGamut:** binary-search chroma (`q`) down when linear RGB would clip at the current luma/phase

**Usage Example:**
```c
ColorRGBA src = {200, 80, 40, 255};
ColorYPQf edit = SituationColorToYPQf(src);
edit.q = 1.0f;                              // push chroma in float space
edit = SituationYpqClampInGamut(edit);      // avoid RGB clip before display
ColorRGBA graded = SituationColorFromYPQf(edit);
ColorYPQA packed = SituationYpqQuantize(edit); // optional 8-bit sidecar
```

---
#### `SituationRgbToYpqa` / `SituationYpqaToRgb`
Converts a color between RGBA and the YPQA color space (a custom space for signal processing-like effects).
```c
ColorYPQA SituationRgbToYpqa(ColorRGBA rgb);
ColorRGBA SituationYpqaToRgb(ColorYPQA ypqa);
```

---
#### `SituationConvertColorToVector4`
Converts an 8-bit RGBA color to a normalized Vector4 (float values 0.0-1.0). This is useful for passing colors to shaders or graphics APIs that expect normalized values.
```c
void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color);
```
**Usage Example:**
```c
ColorRGBA color = {255, 128, 64, 255}; // Orange color
Vector4 normalized;
SituationConvertColorToVector4(color, &normalized);
// normalized is now {1.0f, 0.5f, 0.25f, 1.0f}

// Use in shader uniform
SituationSetShaderUniform(shader, "u_color", &normalized, sizeof(Vector4));
```

---
#### Rendering Utilities
---
#### `SituationDrawMetricsOverlay`
Draws a debug overlay showing FPS, frame time, memory usage, and other performance metrics. This is invaluable for development and profiling.
```c
void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color);
```
**Usage Example:**
```c
// Draw performance metrics in the top-left corner
if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
    SituationRenderPassInfo pass = {/* ... */};
    SituationCmdBeginRenderPass(cmd, &pass);
    
    // Draw your scene...
    
    // Add debug overlay
    #ifdef DEBUG
        Vector2 pos = {10, 10};
        ColorRGBA green = {0, 255, 0, 255};
        SituationDrawMetricsOverlay(cmd, pos, green);
    #endif
    
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}
```

---
#### `SituationIsFeatureSupported`
Checks if a specific graphics feature is supported and enabled on the current hardware. Use this to conditionally enable advanced rendering features.
```c
bool SituationIsFeatureSupported(SituationRenderFeature feature);
```
**Usage Example:**
```c
// Check for compute shader support before using them
if (SituationIsFeatureSupported(SIT_FEATURE_COMPUTE_SHADERS)) {
    printf("Compute shaders are supported!\n");
    // Enable compute-based post-processing
} else {
    printf("Compute shaders not available, using fallback\n");
    // Use traditional rendering path
}

// Check for bindless textures
if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
    // Use texture arrays and bindless rendering
}
```

---
#### Memory Management Helpers
---
#### `SituationFreeString`
Frees memory for a string allocated and returned by the library. Always call this for strings returned by functions like `SituationGetBasePath()`, `SituationGetUserDirectory()`, etc.

```c
void SituationFreeString(char* str);
```

**Parameters:**
- `str` - String pointer to free (can be NULL)

**Usage Example:**
```c
// Get base path (library allocates memory)
char* base_path = SituationGetBasePath();
printf("Base path: %s\n", base_path);

// Build asset path
char asset_path[512];
snprintf(asset_path, sizeof(asset_path), "%sassets/texture.png", base_path);

// Always free the string when done
SituationFreeString(base_path);

// Another example with user directory
char* user_dir = SituationGetUserDirectory();
printf("User directory: %s\n", user_dir);
SituationFreeString(user_dir);

// Safe to call with NULL
SituationFreeString(NULL);  // Does nothing
```

**Notes:**
- Must be called to avoid memory leaks
- Safe to call with NULL pointer
- Only use for strings returned by Situation functions
- Do not use `free()` directly - use this function instead

---
#### `SituationFreeDisplays`
Frees the memory allocated for the display list returned by `SituationGetDisplays()`. Always call this after you're done using the display information.

```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```

**Parameters:**
- `displays` - Pointer to the display array to free
- `count` - Number of displays in the array

**Usage Example:**
```c
// Get display information
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);

// Use the display info
for (int i = 0; i < display_count; i++) {
    printf("Display %d: %dx%d @ %dHz\n",
        i,
        displays[i].width,
        displays[i].height,
        displays[i].refresh_rate);
}

// Always free when done
SituationFreeDisplays(displays, display_count);
```

**Notes:**
- Must be called to avoid memory leaks
- Pass the same count returned by `SituationGetDisplays()`
- Safe to call with NULL pointer (does nothing)

---
#### `SituationFreeDirectoryFileList`
Frees the memory for the list of file paths returned by `SituationListDirectoryFiles()`. Always call this after processing the file list.

```c
void SituationFreeDirectoryFileList(char** files, int count);
```

**Parameters:**
- `files` - Array of file path strings to free
- `count` - Number of files in the array

**Usage Example:**
```c
// List all files in a directory
int file_count;
char** files = SituationListDirectoryFiles("assets/textures", &file_count);

if (files != NULL) {
    printf("Found %d files:\n", file_count);
    
    // Process each file
    for (int i = 0; i < file_count; i++) {
        printf("  %s\n", files[i]);
        
        // Load texture if it's a PNG
        if (strstr(files[i], ".png") != NULL) {
            SituationTexture tex = SituationLoadTexture(files[i]);
            // Use texture...
        }
    }
    
    // Always free the list when done
    SituationFreeDirectoryFileList(files, file_count);
}
```

**Advanced Example (Asset Discovery):**
```c
// Discover and load all shaders in a directory
void LoadAllShaders(const char* shader_dir) {
    int file_count;
    char** files = SituationListDirectoryFiles(shader_dir, &file_count);
    
    if (files == NULL) {
        printf("Failed to list directory: %s\n", shader_dir);
        return;
    }
    
    for (int i = 0; i < file_count; i++) {
        // Check if it's a vertex shader
        if (strstr(files[i], ".vert") != NULL) {
            // Find corresponding fragment shader
            char frag_path[512];
            strcpy(frag_path, files[i]);
            char* ext = strstr(frag_path, ".vert");
            strcpy(ext, ".frag");
            
            // Load shader pair
            SituationShader shader;
            if (SituationLoadShader(files[i], frag_path, &shader) == SITUATION_SUCCESS) {
                printf("Loaded shader: %s\n", files[i]);
            }
        }
    }
    
    // Free the file list
    SituationFreeDirectoryFileList(files, file_count);
}
```

**Notes:**
- Must be called to avoid memory leaks
- Frees both the array and all individual strings
- Safe to call with NULL pointer
- Pass the same count returned by `SituationListDirectoryFiles()`
