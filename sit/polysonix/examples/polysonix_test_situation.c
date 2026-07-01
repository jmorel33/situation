#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
// situation.h is not provided, but we are coding against its API documentation.
// It is assumed to be in the include path.
#include "situation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <cglm/cglm.h>


// --- Situation SDK Types ---
// Note: Situation SDK provides Vector2, ColorRGBA, and Rectangle.
// Pre-defined colors from raylib
#define LIGHTGRAY  (ColorRGBA){ 200, 200, 200, 255 }
#define RAYWHITE   (ColorRGBA){ 245, 245, 245, 255 }
#define DARKGRAY   (ColorRGBA){ 80, 80, 80, 255 }
#define BLUE       (ColorRGBA){ 0, 121, 241, 255 }
#define GREEN      (ColorRGBA){ 0, 228, 48, 255 }
#define DARKGREEN  (ColorRGBA){ 0, 117, 44, 255 }
#define RED        (ColorRGBA){ 230, 41, 55, 255 }
#define DARKBLUE   (ColorRGBA){ 0, 82, 172, 255 }
#define BLACK      (ColorRGBA){ 0, 0, 0, 255 }
#define MAROON     (ColorRGBA){ 190, 33, 55, 255 }
#define GRAY       (ColorRGBA){ 130, 130, 130, 255 }


#define POLYSONIX_IMPLEMENTATION
#define POLYSONIX_PATCHING_IMPLEMENTATION
#include "../px_patching.h"

// --- Global Application State (Not Synth State) ---
static bool enable_drawing = true;

// --- Configuration ---
#define SCREEN_WIDTH        800
#define SCREEN_HEIGHT       750
#define TARGET_FPS          60
#define REQUESTED_SAMPLE_RATE 48000
#define BITS_PER_SAMPLE     16
#define CHANNELS            2
#define SAMPLES_PER_UPDATE  2048
#define DRAW_WAVEFORM_HEIGHT 128
#define SINGLE_CYCLE_LENGTH 256 // For waveform drawing

// <<< NEW: Configuration now also uses defines for the library
#define NUM_VOICES          8
#define NUM_VOICE_ADSRS     3
#define NUM_LFOS            3
#define LFO_UPDATE_INTERVAL_MS 1.0f

// --- Global Pointers & Buffers ---
static PxSynth* synth = NULL;
static SituationSound audio_stream; // Will be a streaming sound managed by Situation
static float mix_buffer[SAMPLES_PER_UPDATE * CHANNELS];
static int16_t static_display_buffer[SINGLE_CYCLE_LENGTH];

// --- NEW: CPU-side rendering resources ---
static SituationImage canvas_image;
static SituationTexture canvas_texture;
static SituationShader canvas_shader;
static SituationFont main_font;
static mat4 projection;


// --- UI & Control State ---
static int current_wave_index = 0;
static int current_patch_index = 0;
static int octave_shift = 0;
static int last_drawn_wave_index = -1;
static bool last_wave_compile_status = false;

// UI Edit state
typedef enum {
    EDIT_TARGET_ADSR_0_PARAMS,    EDIT_TARGET_ADSR_1_PARAMS,    EDIT_TARGET_ADSR_2_PARAMS,
    EDIT_TARGET_ADSR_0_ROUTING,   EDIT_TARGET_ADSR_1_ROUTING,   EDIT_TARGET_ADSR_2_ROUTING,
    EDIT_TARGET_LFO_0_CORE_PARAMS,EDIT_TARGET_LFO_0_ADSR_PARAMS,EDIT_TARGET_LFO_0_ROUTING,
    EDIT_TARGET_LFO_1_CORE_PARAMS,EDIT_TARGET_LFO_1_ADSR_PARAMS,EDIT_TARGET_LFO_1_ROUTING,
    EDIT_TARGET_LFO_2_CORE_PARAMS,EDIT_TARGET_LFO_2_ADSR_PARAMS,EDIT_TARGET_LFO_2_ROUTING,
    EDIT_TARGET_FILTER_PARAMS,    EDIT_TARGET_GLIDE_PARAMS,     EDIT_TARGET_OSC_PARAMS,
    EDIT_TARGET_COUNT
} EditTarget;

static EditTarget current_edit_target = EDIT_TARGET_ADSR_0_PARAMS;
static int current_editing_adsr_destination_idx = PX_ADSR_DEST_PARAM1;
static int current_editing_lfo_destination_idx = PX_LFO_DEST_PARAM1;

static const char* edit_target_names[] = {
    "V.ADSR 0 PARAMS", "V.ADSR 1 PARAMS", "V.ADSR 2 PARAMS",
    "V.ADSR 0 ROUTING", "V.ADSR 1 ROUTING", "V.ADSR 2 ROUTING",
    "LFO 0 CORE", "LFO 0 ADSR", "LFO 0 ROUTING",
    "LFO 1 CORE", "LFO 1 ADSR", "LFO 1 ROUTING",
    "LFO 2 CORE", "LFO 2 ADSR", "LFO 2 ROUTING",
    "FILTER PARAMS",   "GLIDE PARAMS",    "OSC 0 PARAMS"
};

// Keyboard mapping
typedef struct {
    int situation_key;
    int midi_note;
} KeyNoteMapping;

static KeyNoteMapping piano_keys[] = {
    { SIT_KEY_Q, 72 }, { SIT_KEY_2, 73 }, { SIT_KEY_W, 74 }, { SIT_KEY_3, 75 }, { SIT_KEY_E, 76 }, { SIT_KEY_R, 77 }, { SIT_KEY_5, 78 }, { SIT_KEY_T, 79 }, { SIT_KEY_6, 80 }, { SIT_KEY_Y, 81 }, { SIT_KEY_7, 82 }, { SIT_KEY_U, 83 }, { SIT_KEY_I, 84 }, { SIT_KEY_9, 85 },
    { SIT_KEY_O, 86 }, { SIT_KEY_0, 87 }, { SIT_KEY_P, 88 }, { SIT_KEY_LEFT_BRACKET, 89 }, { SIT_KEY_EQUAL, 90 }, { SIT_KEY_RIGHT_BRACKET, 91 }, { SIT_KEY_Z, 60 }, { SIT_KEY_S, 61 }, { SIT_KEY_X, 62 }, { SIT_KEY_D, 63 }, { SIT_KEY_C, 64 }, { SIT_KEY_V, 65 }, { SIT_KEY_G, 66 },
    { SIT_KEY_B, 67 }, { SIT_KEY_H, 68 }, { SIT_KEY_N, 69 }, { SIT_KEY_J, 70 }, { SIT_KEY_M, 71 }, { SIT_KEY_COMMA, 72 }, { SIT_KEY_L, 73 }, { SIT_KEY_PERIOD, 74}, { SIT_KEY_SEMICOLON, 75}, { SIT_KEY_SLASH, 76 }, { SIT_KEY_APOSTROPHE, 77 }, { 0, -1 }
};
#define KEY_OCTAVE_UP   SIT_KEY_RIGHT
#define KEY_OCTAVE_DOWN SIT_KEY_LEFT

// --- Helper Functions ---

// A simple replacement for raylib's TextFormat
const char* TextFormat(const char* format, ...) {
    static char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}


static bool compile_all_waves() {
    printf("Compiling %d waveform expressions...\n", PX_GetNumWaveforms());
    int success_count = 0;
    for (int i = 0; i < PX_GetNumWaveforms(); ++i) {
        if (default_waves[i].compiled_bytecode != NULL) {
            free_bytecode_chunk(default_waves[i].compiled_bytecode);
            free(default_waves[i].compiled_bytecode);
            default_waves[i].compiled_bytecode = NULL;
        }
        default_waves[i].compiled_bytecode = compile_expression_to_bytecode(default_waves[i].expression);
        if (default_waves[i].compiled_bytecode != NULL) success_count++;
        else {
            PxWaveInfo info = PX_GetWaveInfo(i);
            fprintf(stderr, "Failed to compile waveform %d ('%s'): %s\n", i, info.name, default_waves[i].expression);
        }
    }
    printf("Finished compiling waveforms (%d successful).\n", success_count);
    return success_count > 0;
}

// --- Audio Callback for Situation ---
// SDK 2.3.36: Callback receives bytesToRead, not frames.
static uint64_t on_audio_stream_read(void* user_data, void* buffer, uint64_t bytes_to_read) {
    if (!synth) return 0;

    // Calculate frames requested based on bytes and format (float, stereo)
    uint64_t frames_requested = bytes_to_read / (sizeof(float) * CHANNELS);

    // Clamp to our buffer size
    uint64_t frames_to_generate = (frames_requested < SAMPLES_PER_UPDATE) ? frames_requested : SAMPLES_PER_UPDATE;

    // 1. Generate float samples from Polysonix directly into mix_buffer
    PX_Process(synth, mix_buffer, (int)frames_to_generate);

    // 2. Copy to the output buffer
    float* out_buffer = (float*)buffer;
    memcpy(out_buffer, mix_buffer, frames_to_generate * CHANNELS * sizeof(float));

    // Return the number of bytes written, not frames
    return frames_to_generate * sizeof(float) * CHANNELS;
}

static uint64_t on_audio_stream_seek(void* user_data, int64_t offset, int whence) {
    // This is a live stream, seeking is not supported.
    return 0;
}

// --- UI Drawing Functions (Situation Replacements) ---

typedef enum {
    RENDER_MODE_CPU_CANVAS,
    RENDER_MODE_GPU_OVERLAY
} RenderMode;

typedef struct {
    SituationCommandBuffer cmd;
    RenderMode mode;
} RenderContext;

void DrawPixelV(RenderContext* ctx, Vector2 position, ColorRGBA color) {
    if (ctx->mode == RENDER_MODE_CPU_CANVAS) {
        SituationSetPixelColor(&canvas_image, (int)position.x, (int)position.y, color);
    }
}

void DrawLine(RenderContext* ctx, int startPosX, int startPosY, int endPosX, int endPosY, ColorRGBA color) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    int dx = abs(endPosX - startPosX);
    int sx = startPosX < endPosX ? 1 : -1;
    int dy = -abs(endPosY - startPosY);
    int sy = startPosY < endPosY ? 1 : -1;
    int err = dx + dy;
    int e2;

    for (;;) {
        SituationSetPixelColor(&canvas_image, startPosX, startPosY, color);
        if (startPosX == endPosX && startPosY == endPosY) break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            startPosX += sx;
        }
        if (e2 <= dx) {
            err += dx;
            startPosY += sy;
        }
    }
}

void DrawLineStrip(RenderContext* ctx, Vector2 *points, int pointCount, ColorRGBA color) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    if (pointCount < 2) return;
    for (int i = 0; i < pointCount - 1; i++) {
        DrawLine(ctx, (int)points[i].x, (int)points[i].y, (int)points[i+1].x, (int)points[i+1].y, color);
    }
}

void DrawCircle(RenderContext* ctx, int centerX, int centerY, float radius, ColorRGBA color) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    int x = (int)radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        SituationSetPixelColor(&canvas_image, centerX + x, centerY + y, color);
        SituationSetPixelColor(&canvas_image, centerX + y, centerY + x, color);
        SituationSetPixelColor(&canvas_image, centerX - y, centerY + x, color);
        SituationSetPixelColor(&canvas_image, centerX - x, centerY + y, color);
        SituationSetPixelColor(&canvas_image, centerX - x, centerY - y, color);
        SituationSetPixelColor(&canvas_image, centerX - y, centerY - x, color);
        SituationSetPixelColor(&canvas_image, centerX + y, centerY - x, color);
        SituationSetPixelColor(&canvas_image, centerX + x, centerY - y, color);

        y++;
        err += 1 + 2*y;
        if (2*(err-x) + 1 > 0) {
            x--;
            err += 1 - 2*x;
        }
    }
}

void DrawRectangle(RenderContext* ctx, int x, int y, int width, int height, ColorRGBA color) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    for (int j = y; j < y + height; j++) {
        for (int i = x; i < x + width; i++) {
            SituationSetPixelColor(&canvas_image, i, j, color);
        }
    }
}

void DrawRectangleLines(RenderContext* ctx, int x, int y, int width, int height, ColorRGBA color) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    DrawLine(ctx, x, y, x + width, y, color);
    DrawLine(ctx, x + width, y, x + width, y + height, color);
    DrawLine(ctx, x + width, y + height, x, y + height, color);
    DrawLine(ctx, x, y + height, x, y, color);
}

void DrawText(RenderContext* ctx, const char* text, int x, int y, int fontSize, ColorRGBA color) {
    if (ctx->mode == RENDER_MODE_GPU_OVERLAY && main_font.id != 0) {
        // SDK 2.3.36: Use GPU command for text.
        // Spacing 0.0f is standard.
        SituationCmdDrawTextEx(ctx->cmd, main_font, text, (Vector2){(float)x, (float)y}, (float)fontSize, 0.0f, color);
    }
}

int MeasureText(const char *text, int fontSize) {
    if (!main_font.id) return 0;
    // Assuming SituationMeasureText returns float* or vec2 (float[2]) compatible with glm_vec2_copy
    vec2 size;
    glm_vec2_copy(SituationMeasureText(main_font, text, (float)fontSize, 1.0f), size);
    return (int)size[0];
}

static void DrawLFOIndicator(RenderContext* ctx, float lfo_value_normalized, int x, int y, int radius) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    float t = fmaxf(0.0f, fminf(1.0f, (lfo_value_normalized + 1.0f) * 0.5f));
    ColorRGBA color = {(uint8_t)(255 * t), (uint8_t)(255 * (1.0f - t)), 0, 255};
    DrawCircle(ctx, x, y, radius, color);
}

// --- Application Lifecycle ---

static bool InitializeApplication() {
    srand((unsigned int)time(NULL));

    // SDK 2.3.36 Initialization
    SituationInitInfo init_info = {0};
    init_info.app_name = "Polysonix Situation Player";
    init_info.app_version = "1.0.0";
    init_info.window_width = SCREEN_WIDTH;
    init_info.window_height = SCREEN_HEIGHT;
    init_info.window_title = "Polysonix Situation Player";
    init_info.initial_active_window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT;

    if (SituationInit(0, NULL, &init_info) != SITUATION_SUCCESS) {
        char* err = SituationGetLastErrorMsg();
        printf("Failed to initialize Situation: %s\n", err);
        free(err);
        return false;
    }

    SituationSetTargetFPS(TARGET_FPS);

    // Initialize Polysonix tables explicitly
    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    float actual_sample_rate = REQUESTED_SAMPLE_RATE;
    printf("Audio: SR: %.0f Hz, Channels: %d\n", actual_sample_rate, CHANNELS);

    //Create the synthesizer instance
    PxConfig config = {
        .num_voices = NUM_VOICES,
        .num_lfos = NUM_LFOS,
        .num_voice_adsrs = NUM_VOICE_ADSRS,
        .sample_rate = actual_sample_rate,
        .lfo_update_interval_ms = LFO_UPDATE_INTERVAL_MS,
        .samples_per_lfo_update = (int)(actual_sample_rate * (LFO_UPDATE_INTERVAL_MS / 1000.0f)),
        .osc_update_mode = PX_OSC_UPDATE_MODE_PER_SAMPLE,
        .osc_fixed_update_rate_hz = 48000,
        .nyquist_precision_multiplier = 1024.0f
    };
    if (config.samples_per_lfo_update < 1) config.samples_per_lfo_update = 1;

    synth = PX_Create(&config);
    if (!synth) {
        printf("Critical: Failed to create PxSynth instance.\n");
        free_bytecode_cache();
        px_vm_free_lfsr_tables();
        SituationShutdown();
        return false;
    }

    // Load initial patch
    PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);

    if (!compile_all_waves()) {
        printf("Critical: Wave compilation resulted in zero successful waveforms. Exiting.\n");
        PX_Destroy(synth);
        free_bytecode_cache();
        px_vm_free_lfsr_tables();
        SituationShutdown();
        return false;
    }

    // --- Setup Situation Audio Stream ---
    SituationAudioFormat format = {
        .channels = CHANNELS,
        .sample_rate = REQUESTED_SAMPLE_RATE,
        .bit_depth = 32 // We will be providing float samples
    };
    SituationLoadSoundFromStream(on_audio_stream_read, on_audio_stream_seek, NULL, &format, true, &audio_stream);
    SituationPlayLoadedSound(&audio_stream);

    // Load and Bake Font (SDK 2.3.36)
    if (SituationLoadFont("./font.ttf", &main_font) != SITUATION_SUCCESS) {
        printf("Warning: Failed to load font. Text will not be rendered.\n");
    } else {
        if (SituationBakeFontAtlas(&main_font, 20.0f) != SITUATION_SUCCESS) {
            printf("Warning: Failed to bake font atlas.\n");
        }
    }

    canvas_image = SituationGenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, (ColorRGBA){0,0,0,255});
    SituationCreateTexture(canvas_image, false, &canvas_texture);

    // Using Push Constants for matrix per SDK 2.3.36 "Fastest way"
    const char* vs_canvas =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec2 v_tex_coord;\n"
        "layout(push_constant) uniform Constants {\n"
        "    mat4 u_mvp;\n"
        "} pc;\n"
        "void main() {\n"
        "    gl_Position = pc.u_mvp * vec4(aPos.xy, 0.0, 1.0);\n"
        "    v_tex_coord = aPos.xy * 0.5 + 0.5;\n"
        "    v_tex_coord.y = 1.0 - v_tex_coord.y;\n"
        "}\n";

    const char* fs_canvas =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 v_tex_coord;\n"
        "uniform sampler2D u_texture0;\n"
        "void main() {\n"
        "    FragColor = texture(u_texture0, v_tex_coord);\n"
        "}\n";

    SituationLoadShaderFromMemory(vs_canvas, fs_canvas, &canvas_shader);
    if (!canvas_shader.id) {
        printf("Failed to load canvas shader.\n");
        return false;
    }

    glm_ortho(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, -1.0f, 1.0f, projection);

    return true;
}

static void CleanupApplication() {
    printf("Exiting program.\n");

    if (SituationIsFontValid(main_font)) {
        SituationUnloadFont(main_font);
    }

    // Audio clean up
    SituationStopLoadedSound(&audio_stream);
    SituationUnloadSound(&audio_stream);

    if (synth) PX_Destroy(synth);

    // Free bytecode caches
    free_bytecode_cache();
    px_vm_free_lfsr_tables();

    SituationShutdown();
}

// --- Main Loop Functions ---

static void ProcessInput() {
    if (SituationIsKeyPressed(SIT_KEY_F11)) {
        enable_drawing = !enable_drawing;
        printf("Drawing %s\n", enable_drawing ? "ENABLED" : "DISABLED");
    }

    if (SituationIsKeyPressed(SIT_KEY_DOWN)) current_wave_index = (current_wave_index + 1) % PX_GetNumWaveforms();
    if (SituationIsKeyPressed(SIT_KEY_UP)) current_wave_index = (current_wave_index - 1 + PX_GetNumWaveforms()) % PX_GetNumWaveforms();

    bool ctrl_down = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
    if (ctrl_down) {
        // Patch Loading
        if (SituationIsKeyPressed(SIT_KEY_LEFT)) {
             current_patch_index = (current_patch_index - 1 + 64) % 64;
             PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);
        }
        if (SituationIsKeyPressed(SIT_KEY_RIGHT)) {
             current_patch_index = (current_patch_index + 1) % 64;
             PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);
        }
    } else {
        // Octave Shift
        if (SituationIsKeyPressed(KEY_OCTAVE_UP)) { if (octave_shift < 3) octave_shift++; }
        if (SituationIsKeyPressed(KEY_OCTAVE_DOWN)) { if (octave_shift > -3) octave_shift--; }
    }

    // Restore LFO update interval control
    if (SituationIsKeyPressed(SIT_KEY_F1)) PX_SetLFOUpdateInterval(synth, fmaxf(0.1f, PX_GetLFOUpdateInterval(synth) - 0.1f));
    if (SituationIsKeyPressed(SIT_KEY_F2)) PX_SetLFOUpdateInterval(synth, fminf(50.0f, PX_GetLFOUpdateInterval(synth) + 0.1f));

    if (SituationIsKeyPressed(SIT_KEY_F3)) PX_SetGlobalVoicePan(synth, fmaxf(-1.0f, PX_GetGlobalVoicePan(synth) - 0.1f));
    if (SituationIsKeyPressed(SIT_KEY_F4)) PX_SetGlobalVoicePan(synth, fminf( 1.0f, PX_GetGlobalVoicePan(synth) + 0.1f));
    if (SituationIsKeyPressed(SIT_KEY_F5)) PX_SetLimiterThreshold(synth, fmaxf(0.7f, PX_GetLimiterThreshold(synth) - 0.05f));
    if (SituationIsKeyPressed(SIT_KEY_F6)) PX_SetLimiterThreshold(synth, fminf(0.99f, PX_GetLimiterThreshold(synth) + 0.05f));
    if (SituationIsKeyPressed(SIT_KEY_F7)) PX_SetLimiterRelease(synth, fmaxf(1.0f, PX_GetLimiterRelease(synth) - 25.0f));
    if (SituationIsKeyPressed(SIT_KEY_F8)) PX_SetLimiterRelease(synth, fminf(500.0f, PX_GetLimiterRelease(synth) + 25.0f));
    if (SituationIsKeyPressed(SIT_KEY_F10))PX_SetUnilegatoEnabled(synth, !PX_GetUnilegatoEnabled(synth));

    // Edit Target Selection
    if (SituationIsKeyPressed(SIT_KEY_KP_ENTER)) {
        current_edit_target = (EditTarget)((current_edit_target + 1) % EDIT_TARGET_COUNT);
    }

    // Parameter Editing (all calls now go through the PX_... API)
    float adsr_time_step_small = 0.01f, adsr_time_step_large = 0.05f, adsr_level_step = 0.05f;
    float adsr_route_amount_step_small = 0.05f, adsr_route_amount_step_large = 0.2f;
    float lfo_freq_step_small = 0.1f, lfo_freq_step_large = 1.0f;
    float lfo_route_amount_step_small = 0.05f, lfo_route_amount_step_large = 0.2f;

    if (current_edit_target >= EDIT_TARGET_ADSR_0_PARAMS && current_edit_target <= EDIT_TARGET_ADSR_2_PARAMS) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_PARAMS;
        if (SituationIsKeyPressed(SIT_KEY_KP_0)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK) - adsr_time_step_small);
        if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK) + adsr_time_step_small);
        if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY) - adsr_time_step_large);
        if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY) + adsr_time_step_large);
        if (SituationIsKeyPressed(SIT_KEY_KP_4)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN) - adsr_level_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_5)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN) + adsr_level_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_6)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE) - adsr_time_step_large);
        if (SituationIsKeyPressed(SIT_KEY_KP_7)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE) + adsr_time_step_large);
        if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetVoiceADSREnabled(synth, adsr_idx, !PX_GetVoiceADSREnabled(synth, adsr_idx));
    }
    else if (current_edit_target >= EDIT_TARGET_ADSR_0_ROUTING && current_edit_target <= EDIT_TARGET_ADSR_2_ROUTING) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_ROUTING;
        if (SituationIsKeyPressed(SIT_KEY_KP_0)) current_editing_adsr_destination_idx = (current_editing_adsr_destination_idx - 1 + PX_ADSR_DEST_COUNT) % PX_ADSR_DEST_COUNT;
        if (SituationIsKeyPressed(SIT_KEY_KP_1)) current_editing_adsr_destination_idx = (current_editing_adsr_destination_idx + 1) % PX_ADSR_DEST_COUNT;
        float current_amount = PX_GetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx);
        float step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT)) ? adsr_route_amount_step_large : adsr_route_amount_step_small;
        if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx, current_amount - step);
        if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx, current_amount + step);
    }
    else if (current_edit_target >= EDIT_TARGET_LFO_0_CORE_PARAMS && current_edit_target <= EDIT_TARGET_LFO_2_ROUTING) {
        int lfo_idx = (current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS) / 3;
        int lfo_function_type = (current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS) % 3;
        if (lfo_function_type == 0) { // Core
            float freq_step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT)) ? lfo_freq_step_large : lfo_freq_step_small;
            if (SituationIsKeyPressed(SIT_KEY_KP_0)) PX_SetLFOWaveform(synth, lfo_idx, (PX_GetLFOWaveform(synth, lfo_idx) - 1 + PX_GetNumWaveforms()) % PX_GetNumWaveforms());
            if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetLFOWaveform(synth, lfo_idx, (PX_GetLFOWaveform(synth, lfo_idx) + 1) % PX_GetNumWaveforms());
            if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY, PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY) - freq_step);
            if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY, PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY) + freq_step);
            if (SituationIsKeyPressed(SIT_KEY_KP_8)) PX_SetLFOResetOnKeyOn(synth, lfo_idx, !PX_GetLFOResetOnKeyOn(synth, lfo_idx));
            if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetLFOEnabled(synth, lfo_idx, !PX_GetLFOEnabled(synth, lfo_idx));
        } else if (lfo_function_type == 1) { // ADSR
            if (SituationIsKeyPressed(SIT_KEY_KP_0)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK) - adsr_time_step_small);
            if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK) + adsr_time_step_small);
            if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY) - adsr_time_step_large);
            if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY) + adsr_time_step_large);
            if (SituationIsKeyPressed(SIT_KEY_KP_4)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN) - adsr_level_step);
            if (SituationIsKeyPressed(SIT_KEY_KP_5)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN) + adsr_level_step);
            if (SituationIsKeyPressed(SIT_KEY_KP_6)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE) - adsr_time_step_large);
            if (SituationIsKeyPressed(SIT_KEY_KP_7)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE) + adsr_time_step_large);
            if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetLFOADSREnabled(synth, lfo_idx, !PX_GetLFOADSREnabled(synth, lfo_idx));
        } else if (lfo_function_type == 2) { // Routing
            if (SituationIsKeyPressed(SIT_KEY_KP_0)) current_editing_lfo_destination_idx = (current_editing_lfo_destination_idx - 1 + PX_LFO_DEST_COUNT) % PX_LFO_DEST_COUNT;
            if (SituationIsKeyPressed(SIT_KEY_KP_1)) current_editing_lfo_destination_idx = (current_editing_lfo_destination_idx + 1) % PX_LFO_DEST_COUNT;
            float current_amount = PX_GetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx);
            float step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT)) ? lfo_route_amount_step_large : lfo_route_amount_step_small;
            if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx, current_amount - step);
            if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx, current_amount + step);
        }
    }
    else if (current_edit_target == EDIT_TARGET_FILTER_PARAMS) {
        if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetFilterMode(synth, (PxFilterMode)((PX_GetFilterMode(synth) - 1 + PX_FILTER_MODE_COUNT) % PX_FILTER_MODE_COUNT));
        if (SituationIsKeyPressed(SIT_KEY_KP_2)) PX_SetFilterMode(synth, (PxFilterMode)((PX_GetFilterMode(synth) + 1) % PX_FILTER_MODE_COUNT));
        float cutoff_step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT)) ? 500.0f : 50.0f;
        if (SituationIsKeyPressed(SIT_KEY_KP_3)) PX_SetFilterParam(synth, PX_FILTER_PARAM_CUTOFF, PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF) - cutoff_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_4)) PX_SetFilterParam(synth, PX_FILTER_PARAM_CUTOFF, PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF) + cutoff_step);
        float res_step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT)) ? 1.0f : 0.1f;
        if (SituationIsKeyPressed(SIT_KEY_KP_5)) PX_SetFilterParam(synth, PX_FILTER_PARAM_RESONANCE, PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE) - res_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_6)) PX_SetFilterParam(synth, PX_FILTER_PARAM_RESONANCE, PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE) + res_step);
        float env_amt_step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT)) ? 1000.0f : 100.0f;
        if (SituationIsKeyPressed(SIT_KEY_KP_7)) PX_SetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT, PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT) - env_amt_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_8)) PX_SetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT, PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT) + env_amt_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_DIVIDE)) {
            int current_poles = (int)PX_GetFilterParam(synth, PX_FILTER_PARAM_POLES);
            int next_poles = current_poles + 1;
            if (next_poles > 4) next_poles = 2; // Cycle from 4 back to 2
            if (next_poles < 2) next_poles = 2;
            PX_SetFilterParam(synth, PX_FILTER_PARAM_POLES, (float)next_poles);
        }
        // Filter Drive and Key Tracking controls
        float drive_step = (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT)) ? 0.2f : 0.05f;
        if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetFilterParam(synth, PX_FILTER_PARAM_DRIVE, PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE) - drive_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_MULTIPLY)) PX_SetFilterParam(synth, PX_FILTER_PARAM_DRIVE, PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE) + drive_step);
        float keytrack_step = 0.1f;
        if (SituationIsKeyPressed(SIT_KEY_KP_SUBTRACT)) PX_SetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK, PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK) - keytrack_step);
        if (SituationIsKeyPressed(SIT_KEY_KP_ADD)) PX_SetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK, PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK) + keytrack_step);
    }
    else if (current_edit_target == EDIT_TARGET_GLIDE_PARAMS) {
        if (SituationIsKeyPressed(SIT_KEY_KP_0)) PX_SetGlideTime(synth, fmaxf(0.0f, PX_GetGlideTime(synth) - 0.05f));
        if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetGlideTime(synth, PX_GetGlideTime(synth) + 0.05f);
        if (SituationIsKeyPressed(SIT_KEY_KP_2)) {
            PxGlideMode m = PX_GetGlideMode(synth);
            if (m == PX_GLIDE_OFF) m = PX_GLIDE_STEP_LINEAR;
            else if (m == PX_GLIDE_STEP_LINEAR) m = PX_GLIDE_SMOOTH_RC;
            else m = PX_GLIDE_OFF;
            PX_SetGlideMode(synth, m);
        }
        if (SituationIsKeyPressed(SIT_KEY_KP_8)) PX_SetGlideLegatoOnly(synth, !PX_GetGlideLegatoOnly(synth));
        if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetGlideAlways(synth, !PX_GetGlideAlways(synth));
    }
    else if (current_edit_target == EDIT_TARGET_OSC_PARAMS) {
        float bc_step = 0.05f;
        if (SituationIsKeyPressed(SIT_KEY_KP_0)) PX_SetOscBitcrush(synth, 0, PX_GetOscBitcrushEnabled(synth, 0), fmaxf(0.0f, PX_GetOscBitcrush(synth, 0) - bc_step));
        if (SituationIsKeyPressed(SIT_KEY_KP_1)) PX_SetOscBitcrush(synth, 0, PX_GetOscBitcrushEnabled(synth, 0), fminf(1.0f, PX_GetOscBitcrush(synth, 0) + bc_step));
        if (SituationIsKeyPressed(SIT_KEY_KP_9)) PX_SetOscBitcrush(synth, 0, !PX_GetOscBitcrushEnabled(synth, 0), PX_GetOscBitcrush(synth, 0));
    }

    // Note On/Off
    for (int i = 0; piano_keys[i].situation_key != 0; ++i) {
        int key = piano_keys[i].situation_key;
        if (SituationIsKeyPressed(key)) {
            int midi_note = piano_keys[i].midi_note + octave_shift * 12;
            midi_note = (int)fmaxf(0.0f, fminf(127.0f, (float)midi_note));
            // Call the library function
            PX_NoteOn(synth, midi_note, current_wave_index, key, 1.0f);
        }
        if (SituationIsKeyReleased(key)) {
            // Call the library function
            PX_NoteOff(synth, key);
        }
    }
}

static void DrawLiveOscillator(RenderContext* ctx, float* stereo_buffer, int sampleFrames, int x, int y, int width, int height) {
    if (ctx->mode != RENDER_MODE_CPU_CANVAS) return;
    if (sampleFrames <= 0) return;
    if (sampleFrames > SAMPLES_PER_UPDATE) sampleFrames = SAMPLES_PER_UPDATE;

    Vector2* points = (Vector2*)malloc(sampleFrames * sizeof(Vector2));
    if (!points) return;

    for (int i = 0; i < sampleFrames; ++i) {
        float sample_l = stereo_buffer[i * 2 + 0];
        float sample_r = stereo_buffer[i * 2 + 1];
        float mono_sample = (sample_l + sample_r) * 0.5f;
        points[i].x = x + (float)i / (sampleFrames > 1 ? (sampleFrames - 1) : 1) * width;
        points[i].y = y + height / 2.0f - mono_sample * (height / 2.0f);
    }
    DrawLine(ctx, x, y + height / 2, x + width, y + height / 2, LIGHTGRAY);
    if (sampleFrames > 1) {
        DrawLineStrip(ctx, points, sampleFrames, GREEN);
    } else if (sampleFrames == 1) {
        DrawPixelV(ctx, points[0], GREEN);
    }
    free(points);
}

static void DrawInterface(RenderContext* ctx) {
    // --- Local constants for layout ---
    int y_offset = 5;
    int line_height = 16;
    int small_line_height = 14;

    // --- Header / Help Text ---
    DrawText(ctx, "Polysonix Synthesizer Player (Situation)", 10, y_offset, line_height, DARKGRAY); y_offset += line_height + 2;
    // <<< NEW: Update Help Text
    DrawText(ctx, "CTRL+L/R:Patch, UP/DN:Wave, L/R:Oct, F1/F2:LFO Rate, F3/F4:Pan, Keys:Play", 10, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    DrawText(ctx, "KP_ENTER: Edit Target, KP0-9/etc: Edit Params/Routing", 10, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height + 3;

    // --- Currently Editing Target Display ---
    DrawText(ctx, TextFormat("EDITING: %s", edit_target_names[current_edit_target]), 10, y_offset, line_height, BLUE); y_offset += line_height;

    // --- Parameter Editing Display Block ---
    if (current_edit_target >= EDIT_TARGET_ADSR_0_PARAMS && current_edit_target <= EDIT_TARGET_ADSR_2_PARAMS) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_PARAMS;
        DrawText(ctx, TextFormat("ADSR %d [%s]: A:%.2fs D:%.2fs S:%.2f R:%.2fs", adsr_idx,
            PX_GetVoiceADSREnabled(synth, adsr_idx) ? "ON" : "OFF",
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE)),
            20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
        DrawText(ctx, "KP0/1:Atk, KP2/3:Dcy, KP4/5:Sus, KP6/7:Rel, KP9:En/Dis", 20, y_offset, small_line_height - 2, GRAY);
        y_offset += small_line_height -2;
    } else if (current_edit_target >= EDIT_TARGET_ADSR_0_ROUTING && current_edit_target <= EDIT_TARGET_ADSR_2_ROUTING) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_ROUTING;
        DrawText(ctx, TextFormat("ADSR %d Routing -> Dest (KP0/1): %s", adsr_idx, PX_GetADSRDestinationName((PxADSRDestination)current_editing_adsr_destination_idx)), 20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
        DrawText(ctx, TextFormat("Amount (KP2/3): %.2f", PX_GetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx)), 20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    } else if (current_edit_target >= EDIT_TARGET_LFO_0_CORE_PARAMS && current_edit_target <= EDIT_TARGET_LFO_2_ROUTING) {
        int base_lfo_offset = current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS;
        int lfo_idx = base_lfo_offset / 3;
        int lfo_function_type = base_lfo_offset % 3;

        if (lfo_function_type == 0) { // LFO Core Parameters
            PxWaveInfo lfo_wave_info = PX_GetWaveInfo(PX_GetLFOWaveform(synth, lfo_idx));
            DrawText(ctx, TextFormat("LFO %d CORE PARAMETERS:", lfo_idx), 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
            DrawText(ctx, TextFormat("Wave (KP0/1): %s [%d]", lfo_wave_info.name, PX_GetLFOWaveform(synth, lfo_idx)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(ctx, TextFormat("Freq (KP2/3): %.2f Hz", PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(ctx, TextFormat("ResetOnKey (KP8): %s", PX_GetLFOResetOnKeyOn(synth, lfo_idx) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(ctx, TextFormat("LFO Enabled (KP9): %s", PX_GetLFOEnabled(synth, lfo_idx) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        } else if (lfo_function_type == 1) { // LFO ADSR Parameters
            DrawText(ctx, TextFormat("LFO %d ADSR PARAMETERS:", lfo_idx), 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
            DrawText(ctx, TextFormat("ADSR [%s]: A:%.2fs D:%.2fs S:%.2f R:%.2fs",
                PX_GetLFOADSREnabled(synth, lfo_idx) ? "ON" : "OFF",
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE)),
                20, y_offset, small_line_height, DARKGRAY);
            y_offset += small_line_height;
            DrawText(ctx, "KP0/1:Atk, KP2/3:Dcy, KP4/5:Sus, KP6/7:Rel, KP9:En/Dis ADSR", 20, y_offset, small_line_height - 2, GRAY);
            y_offset += small_line_height -2;
        } else if (lfo_function_type == 2) { // LFO Routing
            DrawText(ctx, TextFormat("LFO %d ROUTING -> Dest (KP0/1): %s", lfo_idx, PX_GetLFODestinationName((PxLFODestination)current_editing_lfo_destination_idx)), 20, y_offset, small_line_height, DARKBLUE);
            y_offset += small_line_height;
            float amount = PX_GetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx);
            DrawText(ctx, TextFormat("Amount (KP2/3): %.2f", amount), 20, y_offset, small_line_height, DARKGRAY);
            y_offset += small_line_height;
        }
    } else if (current_edit_target == EDIT_TARGET_FILTER_PARAMS) {
        DrawText(ctx, "FILTER PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Mode (KP1/2): %s", PX_GetFilterModeName(PX_GetFilterMode(synth))), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Slope (KP/): %ddB", (int)PX_GetFilterParam(synth, PX_FILTER_PARAM_POLES) * 6), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Cutoff (KP3/4): %.0f Hz", PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Res (KP5/6): %.2f Q", PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Env Amt (KP7/8): %.0f Hz", PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Drive(KP9/*): %.2f  KeyTrk(KP-/+): %.2f",
            PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE),
            PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK)),
            20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    } else if (current_edit_target == EDIT_TARGET_GLIDE_PARAMS) {
        DrawText(ctx, "GLIDE PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        const char* mode_str = "OFF";
        PxGlideMode m = PX_GetGlideMode(synth);
        if (m == PX_GLIDE_STEP_LINEAR) mode_str = "LINEAR (Step)";
        if (m == PX_GLIDE_SMOOTH_RC) mode_str = "SMOOTH (RC)";
        DrawText(ctx, TextFormat("Mode (KP2): %s", mode_str), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Time (KP0/1): %.2fs", PX_GetGlideTime(synth)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Legato Only (KP8): %s", PX_GetGlideLegatoOnly(synth) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Always Glide (KP9): %s", PX_GetGlideAlways(synth) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    } else if (current_edit_target == EDIT_TARGET_OSC_PARAMS) {
        DrawText(ctx, "OSC 0 PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        DrawText(ctx, TextFormat("Bitcrush (KP9): %s", PX_GetOscBitcrushEnabled(synth, 0) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(ctx, TextFormat("BC Depth (KP0/1): %.2f", PX_GetOscBitcrush(synth, 0)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    }
    y_offset += 3;

    // --- Main Synth Status Display ---
    PxWaveInfo waveInfo = PX_GetWaveInfo(current_wave_index);
    DrawText(ctx, TextFormat("Osc Wave[%d]:%s Oct:%d", current_wave_index, waveInfo.name, octave_shift), 10, y_offset, small_line_height, waveInfo.is_compiled ? DARKBLUE : RED);
    y_offset += small_line_height;
    // <<< NEW: Display Current Patch
    DrawText(ctx, TextFormat("Patch [%d]: %s", current_patch_index, ROM_PATCHES[current_patch_index].name), 10, y_offset, small_line_height, DARKGREEN);
    y_offset += small_line_height;

    DrawText(ctx, TextFormat("Global Voice Pan (F3/F4): %.2f", PX_GetGlobalVoicePan(synth)), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;
    DrawText(ctx, TextFormat("Unilegato (F10): %s", PX_GetUnilegatoEnabled(synth) ? "ON" : "OFF"), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;

    // --- Template LFO Display ---
    for (int lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx) {
        PxLFOInfo lfo_info = PX_GetLFOInfo(synth, lfo_idx);
        PxWaveInfo lfo_wave_info = PX_GetWaveInfo(lfo_info.wave_idx);
        DrawText(ctx, TextFormat("TPL LFO %d[%s]:%s %.1fHz Rst:%s ADSR[%s]Lvl:%.2f", lfo_idx,
            lfo_info.enabled ? "ON" : "OFF", lfo_wave_info.name, lfo_info.frequency,
            lfo_info.reset_on_key_on ? "KEY" : "FREE", lfo_info.adsr_enabled ? "ON" : "OFF", lfo_info.adsr_level),
            10, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    }
    float lfo_interval = PX_GetLFOUpdateInterval(synth);
    float lfo_rate_hz = (lfo_interval > 0) ? 1000.0f / lfo_interval : 0.0f;
    float samples_per_update = (lfo_rate_hz > 0) ? (REQUESTED_SAMPLE_RATE / lfo_rate_hz) : 0;
    DrawText(ctx, TextFormat("LFO Update Rate: %.1f Hz (%.0f samples/update)", lfo_rate_hz, samples_per_update), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;

    // --- Top-Right Status Block ---
    int right_x = SCREEN_WIDTH - 250;
    int right_y_start = 10;
    int active_voice_count = 0;
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (PX_GetVoiceInfo(synth, i).active) active_voice_count++;
    }
    DrawText(ctx, TextFormat("Voices: %d/%d", active_voice_count, NUM_VOICES), SCREEN_WIDTH - 100, right_y_start, small_line_height, DARKGRAY);
    right_y_start += small_line_height;

    for (int lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx) {
        PxLFOInfo lfo_info = PX_GetLFOInfo(synth, lfo_idx);
        float display_output = lfo_info.enabled ? (lfo_info.adsr_enabled ? lfo_info.raw_output * lfo_info.adsr_level : lfo_info.raw_output) : 0.0f;
        DrawText(ctx, TextFormat("TPL LFO %d Out: %+.2f", lfo_idx, display_output), right_x, right_y_start, small_line_height, DARKGRAY);
        if (lfo_info.enabled && (!lfo_info.adsr_enabled || lfo_info.adsr_level > 0.001f)) {
            DrawLFOIndicator(ctx, lfo_info.raw_output, right_x + 160, right_y_start + 7, 7);
        }
        right_y_start += small_line_height;
    }
    PxLimiterInfo lim_info = PX_GetLimiterInfo(synth);
    if (lim_info.initialized) {
        DrawText(ctx, TextFormat("Limiter: -%.1fdB", lim_info.gain_reduction_db), SCREEN_WIDTH - 120, right_y_start, small_line_height, lim_info.gain_reduction_db > 0.1f ? RED : DARKGRAY);
    }

    // --- Voice Status Table ---
    int voice_display_y_start = y_offset + 10;
    int voice_line_h = small_line_height - 2;
    DrawText(ctx, "VOICE STATUS:", 10, voice_display_y_start, line_height - 2, BLACK);
    voice_display_y_start += line_height - 2;

    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voice_display_y_start + i * voice_line_h > SCREEN_HEIGHT - DRAW_WAVEFORM_HEIGHT - 100 - voice_line_h) break;

        PxVoiceInfo v_info = PX_GetVoiceInfo(synth, i);
        char adsr_summary[NUM_VOICE_ADSRS * 20 + 5] = "";
        for (int k = 0; k < NUM_VOICE_ADSRS; ++k) {
            char temp_summary[32];
            snprintf(temp_summary, sizeof(temp_summary), " A%d(%s:%.1f)", k, PX_GetADSRStateName(v_info.adsr_states[k]), v_info.adsr_levels[k]);
            strncat(adsr_summary, temp_summary, sizeof(adsr_summary) - strlen(adsr_summary) - 1);
        }

        char lfo_outputs_str[NUM_LFOS * 10 + 5] = "";
        for (int k = 0; k < NUM_LFOS; ++k) {
            char temp_lfo_out[15];
            snprintf(temp_lfo_out, sizeof(temp_lfo_out), " L%d:%.1f", k, v_info.lfo_outputs[k]);
            strncat(lfo_outputs_str, temp_lfo_out, sizeof(lfo_outputs_str) - strlen(lfo_outputs_str) - 1);
        }

        DrawText(ctx, TextFormat("V%d:%s N:%02d F:%.0f EAmp:%.1f P:%.1f %s%s",
                 i, v_info.active ? "On" : "Off", v_info.midi_note, v_info.frequency,
                 v_info.effective_amplitude, v_info.pan_position, adsr_summary, lfo_outputs_str), 10, voice_display_y_start + i * voice_line_h, voice_line_h, v_info.active ? DARKGREEN : GRAY);
    }
    y_offset = voice_display_y_start + NUM_VOICES * voice_line_h + 5;

    // --- Static Waveform Display ---
    int waveform_draw_y = y_offset + 10;
    if (SCREEN_HEIGHT - (waveform_draw_y + DRAW_WAVEFORM_HEIGHT + 100) < 0) {
        waveform_draw_y = SCREEN_HEIGHT - (DRAW_WAVEFORM_HEIGHT + 100 + 5);
    }
    if (waveform_draw_y < voice_display_y_start + NUM_VOICES * voice_line_h + 10) {
        waveform_draw_y = voice_display_y_start + NUM_VOICES * voice_line_h + 10;
    }

    if (current_wave_index != last_drawn_wave_index || waveInfo.is_compiled != last_wave_compile_status) {
        if (waveInfo.is_compiled) {
            VmParams display_params = { .rand_offset = 0.5f, .modA = 0.0f, .modB = 0.0f, .modC = 0.0f, .lfsr_type = LFSR_8BIT, .lfsr_state = 1, .lfsr_seed = 1 };
            for (int k = 0; k < SINGLE_CYCLE_LENGTH; ++k) {
                display_params.x = ((float)k / SINGLE_CYCLE_LENGTH) * 2.0f * M_PI;
                float s_f = execute_bytecode(default_waves[current_wave_index].compiled_bytecode, &display_params);
                static_display_buffer[k] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, s_f)) * 32767.0f);
            }
        }
        last_drawn_wave_index = current_wave_index;
        last_wave_compile_status = waveInfo.is_compiled;
    }

    int wf_x = 10;
    int wf_w = SCREEN_WIDTH - 20;
    DrawRectangleLines(ctx, wf_x - 1, waveform_draw_y - 1, wf_w + 2, DRAW_WAVEFORM_HEIGHT + 2, LIGHTGRAY);
    if (waveInfo.is_compiled) {
        Vector2 pts[SINGLE_CYCLE_LENGTH];
        DrawLine(ctx, wf_x, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2, wf_x + wf_w, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2, LIGHTGRAY);
        for (int k = 0; k < SINGLE_CYCLE_LENGTH; ++k) {
            pts[k].x = wf_x + (float)k / (SINGLE_CYCLE_LENGTH - 1) * wf_w;
            pts[k].y = waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2.0f - (static_display_buffer[k] / 32768.0f) * (DRAW_WAVEFORM_HEIGHT / 2.0f);
        }
        DrawLineStrip(ctx, pts, SINGLE_CYCLE_LENGTH, MAROON);
    } else {
        DrawText(ctx, "Wave compilation failed!", wf_x + wf_w / 2 - MeasureText("Wave compilation failed!", 20) / 2, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2 - 10, 20, RED);
    }

    // --- Live Output Display (Identical to v8) ---
    DrawLiveOscillator(ctx, mix_buffer, SAMPLES_PER_UPDATE, 10, waveform_draw_y + DRAW_WAVEFORM_HEIGHT + 10, SCREEN_WIDTH - 20, 80);
}

static void DrawFrame() {
    // --- 1. Draw everything to the CPU-side canvas ---
    SituationImageClearBackground(&canvas_image, (ColorRGBA){ 245, 245, 245, 255 }); // RAYWHITE

    // First Pass: Draw all shapes/pixels to CPU canvas
    RenderContext ctx_cpu = { .cmd = {0}, .mode = RENDER_MODE_CPU_CANVAS };
    DrawInterface(&ctx_cpu);

    // --- 2. Upload the canvas to the GPU and render it ---
    // SDK 2.3.36: Use UpdateTexture for efficiency
    if (canvas_texture.id == 0) {
        SituationCreateTexture(canvas_image, false, &canvas_texture);
    } else {
        SituationUpdateTexture(canvas_texture, canvas_image);
    }

    if (SituationAcquireFrameCommandBuffer()) {
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        // SDK 2.3.36 RenderPass Info
        SituationRenderPassInfo pass_info = {
            .display_id = -1, // Main Window
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_DONT_CARE,
                .storeOp = SIT_STORE_OP_STORE,
                .clear = { .color = {0,0,0,255} }
            },
            .depth_attachment = {
                .loadOp = SIT_LOAD_OP_DONT_CARE,
                .storeOp = SIT_STORE_OP_DONT_CARE
            }
        };
        SituationCmdBeginRenderPass(cmd, &pass_info);

        // Draw Canvas (CPU Rendering result)
        SituationCmdBindPipeline(cmd, canvas_shader);
        // SDK 2.3.36: CmdBindTextureSet with args (cmd, set_index, texture)
        SituationCmdBindTextureSet(cmd, 0, canvas_texture);

        mat4 model;
        glm_mat4_identity(model);
        vec3 t = { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f };
        glm_translate(model, t);
        vec3 s = { SCREEN_WIDTH, SCREEN_HEIGHT, 1.0f };
        glm_scale(model, s);

        mat4 mvp;
        glm_mat4_mul(projection, model, mvp);

        // SDK 2.3.36: Use Push Constants for matrices (Fastest way)
        SituationCmdSetPushConstant(cmd, 0, &mvp, sizeof(mat4));

        // SDK 2.3.36: CmdDrawQuad with args (cmd, model, color)
        Vector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
        SituationCmdDrawQuad(cmd, model, white);

        // Second Pass: Draw Text/UI overlay directly on GPU
        RenderContext ctx_gpu = { .cmd = cmd, .mode = RENDER_MODE_GPU_OVERLAY };
        DrawInterface(&ctx_gpu);

        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }
}

int main(void) {
    if (!InitializeApplication()) {
        printf("Application initialization failed.\n");
        return 1;
    }

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        ProcessInput();

        if (enable_drawing) {
            DrawFrame();
        } else {
            if (SituationAcquireFrameCommandBuffer()) {
                SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
                SituationRenderPassInfo pass_info = {
                    .display_id = -1,
                    .color_attachment = {
                        .loadOp = SIT_LOAD_OP_CLEAR,
                        .storeOp = SIT_STORE_OP_STORE,
                        .clear = { .color = { 0, 0, 0, 255 } } // BLACK
                    }
                };
                SituationCmdBeginRenderPass(cmd, &pass_info);
                // Draw directly with GPU command
                RenderContext ctx_gpu = { .cmd = cmd, .mode = RENDER_MODE_GPU_OVERLAY };
                DrawText(&ctx_gpu, "DRAWING DISABLED (F11 to toggle)", 10, 10, 20, RAYWHITE);
                SituationCmdEndRenderPass(cmd);
                SituationEndFrame();
            }
        }
    }

    CleanupApplication();
    return 0;
}
