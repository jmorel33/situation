#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

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
static AudioStream audio_stream;
static int16_t mix_buffer[SAMPLES_PER_UPDATE * CHANNELS];
static int16_t static_display_buffer[SINGLE_CYCLE_LENGTH];

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
    int raylib_key;
    int midi_note;
} KeyNoteMapping;

static KeyNoteMapping piano_keys[] = {
    { KEY_Q, 72 }, { KEY_TWO, 73 }, { KEY_W, 74 }, { KEY_THREE, 75 }, { KEY_E, 76 }, { KEY_R, 77 }, { KEY_FIVE, 78 }, { KEY_T, 79 }, { KEY_SIX, 80 }, { KEY_Y, 81 }, { KEY_SEVEN, 82 }, { KEY_U, 83 }, { KEY_I, 84 }, { KEY_NINE, 85 },
    { KEY_O, 86 }, { KEY_ZERO, 87 }, { KEY_P, 88 }, { KEY_LEFT_BRACKET, 89 }, { KEY_EQUAL, 90 }, { KEY_RIGHT_BRACKET, 91 }, { KEY_Z, 60 }, { KEY_S, 61 }, { KEY_X, 62 }, { KEY_D, 63 }, { KEY_C, 64 }, { KEY_V, 65 }, { KEY_G, 66 },
    { KEY_B, 67 }, { KEY_H, 68 }, { KEY_N, 69 }, { KEY_J, 70 }, { KEY_M, 71 }, { KEY_COMMA, 72 }, { KEY_L, 73 }, { KEY_PERIOD, 74}, { KEY_SEMICOLON, 75}, { KEY_SLASH, 76 }, { KEY_APOSTROPHE, 77 }, { 0, -1 }
};
#define KEY_OCTAVE_UP   KEY_RIGHT
#define KEY_OCTAVE_DOWN KEY_LEFT

// --- Helper Functions ---

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

static void DrawLFOIndicator(float lfo_value_normalized, int x, int y, int radius) {
    float t = fmaxf(0.0f, fminf(1.0f, (lfo_value_normalized + 1.0f) * 0.5f));
    Color color = {(uint8_t)(255 * t), (uint8_t)(255 * (1.0f - t)), 0, 255};
    DrawCircle(x, y, radius, color);
}


// --- Application Lifecycle ---

static bool InitializeApplication() {
    srand((unsigned int)time(NULL));

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Polysonix Synthesizer Player");
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        printf("Audio device not ready.\n");
        return false;
    }

    if (!px_vm_init()) {
        fprintf(stderr, "Failed to initialize Polysonix wave system!\n");
        return false;
    }

    SetAudioStreamBufferSizeDefault(SAMPLES_PER_UPDATE);
    audio_stream = LoadAudioStream(REQUESTED_SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS);
    if (!IsAudioStreamValid(audio_stream)) {
        printf("Failed to load audio stream.\n");
        CloseAudioDevice();
        CloseWindow();
        return false;
    }
    float actual_sample_rate = (float)audio_stream.sampleRate;
    printf("Audio Stream: SR: %.0f Hz, Channels: %d\n", actual_sample_rate, CHANNELS);

    //Create the synthesizer instance
    PxConfig config = {
        .num_voices = NUM_VOICES,
        .num_lfos = NUM_LFOS,
        .num_voice_adsrs = NUM_VOICE_ADSRS,
        .sample_rate = actual_sample_rate,
        .lfo_update_interval_ms = LFO_UPDATE_INTERVAL_MS,
        .samples_per_lfo_update = (int)(actual_sample_rate * (LFO_UPDATE_INTERVAL_MS / 1000.0f)),
        .osc_update_mode = PX_OSC_UPDATE_MODE_PER_SAMPLE, //PX_OSC_UPDATE_MODE_NYQUIST, //PX_OSC_UPDATE_MODE_FIXED_RATE, //
        .osc_fixed_update_rate_hz = 48000,
        .nyquist_precision_multiplier = 1024.0f
    };
    if (config.samples_per_lfo_update < 1) config.samples_per_lfo_update = 1;

    synth = PX_Create(&config);
    if (!synth) {
        // This is the correct cleanup path if the synth fails to be created.
        printf("Critical: Failed to create PxSynth instance.\n");
        px_vm_deinit(); // Clean up the wave system
        if (IsAudioDeviceReady()) {
            CloseAudioDevice(); // Clean up the audio device
        }
        CloseWindow(); // Clean up the window
        return false;
    }

    // Load initial patch
    PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);

    if (!compile_all_waves()) {
        printf("Critical: Wave compilation resulted in zero successful waveforms. Exiting.\n");
        PX_Destroy(synth);
        px_vm_deinit();
        CloseAudioDevice();
        CloseWindow();
        return false;
    }

    PlayAudioStream(audio_stream);
    SetTargetFPS(TARGET_FPS);
    return true;
}

static void CleanupApplication() {
    printf("Exiting program.\n");
    // <<< NEW: Destroy the synth instance
    if (synth) PX_Destroy(synth);

    px_vm_print_stats();
    px_vm_deinit();

    // Free bytecode (this part is external to the synth library)
    for (int i = 0; i < PX_GetNumWaveforms(); ++i) {
        if (default_waves[i].compiled_bytecode != NULL) {
            free_bytecode_chunk(default_waves[i].compiled_bytecode);
            free(default_waves[i].compiled_bytecode);
            default_waves[i].compiled_bytecode = NULL;
        }
    }
    printf("Freed bytecode for %d waveforms.\n", PX_GetNumWaveforms());

    if (IsAudioDeviceReady()) {
        StopAudioStream(audio_stream);
        UnloadAudioStream(audio_stream);
        CloseAudioDevice();
    }
    CloseWindow();
}

// --- Main Loop Functions ---

static void ProcessInput() {
    if (IsKeyPressed(KEY_F11)) {
        enable_drawing = !enable_drawing;
        printf("Drawing %s\n", enable_drawing ? "ENABLED" : "DISABLED");
    }

    if (IsKeyPressed(KEY_DOWN)) current_wave_index = (current_wave_index + 1) % PX_GetNumWaveforms();
    if (IsKeyPressed(KEY_UP)) current_wave_index = (current_wave_index - 1 + PX_GetNumWaveforms()) % PX_GetNumWaveforms();

    bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl_down) {
        // Patch Loading
        if (IsKeyPressed(KEY_LEFT)) {
             current_patch_index = (current_patch_index - 1 + 64) % 64;
             PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);
        }
        if (IsKeyPressed(KEY_RIGHT)) {
             current_patch_index = (current_patch_index + 1) % 64;
             PX_LoadRomPatch(synth, &ROM_PATCHES[current_patch_index]);
        }
    } else {
        // Octave Shift
        if (IsKeyPressed(KEY_OCTAVE_UP)) { if (octave_shift < 3) octave_shift++; }
        if (IsKeyPressed(KEY_OCTAVE_DOWN)) { if (octave_shift > -3) octave_shift--; }
    }

    // Restore LFO update interval control
    if (IsKeyPressed(KEY_F1)) PX_SetLFOUpdateInterval(synth, fmaxf(0.1f, PX_GetLFOUpdateInterval(synth) - 0.1f));
    if (IsKeyPressed(KEY_F2)) PX_SetLFOUpdateInterval(synth, fminf(50.0f, PX_GetLFOUpdateInterval(synth) + 0.1f));

    if (IsKeyPressed(KEY_F3)) PX_SetGlobalVoicePan(synth, fmaxf(-1.0f, PX_GetGlobalVoicePan(synth) - 0.1f));
    if (IsKeyPressed(KEY_F4)) PX_SetGlobalVoicePan(synth, fminf( 1.0f, PX_GetGlobalVoicePan(synth) + 0.1f));
    if (IsKeyPressed(KEY_F5)) PX_SetLimiterThreshold(synth, fmaxf(0.7f, PX_GetLimiterThreshold(synth) - 0.05f));
    if (IsKeyPressed(KEY_F6)) PX_SetLimiterThreshold(synth, fminf(0.99f, PX_GetLimiterThreshold(synth) + 0.05f));
    if (IsKeyPressed(KEY_F7)) PX_SetLimiterRelease(synth, fmaxf(1.0f, PX_GetLimiterRelease(synth) - 25.0f));
    if (IsKeyPressed(KEY_F8)) PX_SetLimiterRelease(synth, fminf(500.0f, PX_GetLimiterRelease(synth) + 25.0f));
    if (IsKeyPressed(KEY_F10))PX_SetUnilegatoEnabled(synth, !PX_GetUnilegatoEnabled(synth));

    // Edit Target Selection
    if (IsKeyPressed(KEY_KP_ENTER)) {
        current_edit_target = (EditTarget)((current_edit_target + 1) % EDIT_TARGET_COUNT);
    }

    // Parameter Editing (all calls now go through the PX_... API)
    // <<< NEW: All direct variable manipulation is replaced with API calls
    float adsr_time_step_small = 0.01f, adsr_time_step_large = 0.05f, adsr_level_step = 0.05f;
    float adsr_route_amount_step_small = 0.05f, adsr_route_amount_step_large = 0.2f;
    float lfo_freq_step_small = 0.1f, lfo_freq_step_large = 1.0f;
    float lfo_route_amount_step_small = 0.05f, lfo_route_amount_step_large = 0.2f;

    if (current_edit_target >= EDIT_TARGET_ADSR_0_PARAMS && current_edit_target <= EDIT_TARGET_ADSR_2_PARAMS) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_PARAMS;
        if (IsKeyPressed(KEY_KP_0)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK) - adsr_time_step_small);
        if (IsKeyPressed(KEY_KP_1)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK) + adsr_time_step_small);
        if (IsKeyPressed(KEY_KP_2)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY) - adsr_time_step_large);
        if (IsKeyPressed(KEY_KP_3)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY) + adsr_time_step_large);
        if (IsKeyPressed(KEY_KP_4)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN) - adsr_level_step);
        if (IsKeyPressed(KEY_KP_5)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN) + adsr_level_step);
        if (IsKeyPressed(KEY_KP_6)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE) - adsr_time_step_large);
        if (IsKeyPressed(KEY_KP_7)) PX_SetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE, PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE) + adsr_time_step_large);
        if (IsKeyPressed(KEY_KP_9)) PX_SetVoiceADSREnabled(synth, adsr_idx, !PX_GetVoiceADSREnabled(synth, adsr_idx));
    }
    else if (current_edit_target >= EDIT_TARGET_ADSR_0_ROUTING && current_edit_target <= EDIT_TARGET_ADSR_2_ROUTING) {
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_ROUTING;
        if (IsKeyPressed(KEY_KP_0)) current_editing_adsr_destination_idx = (current_editing_adsr_destination_idx - 1 + PX_ADSR_DEST_COUNT) % PX_ADSR_DEST_COUNT;
        if (IsKeyPressed(KEY_KP_1)) current_editing_adsr_destination_idx = (current_editing_adsr_destination_idx + 1) % PX_ADSR_DEST_COUNT;
        float current_amount = PX_GetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx);
        float step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? adsr_route_amount_step_large : adsr_route_amount_step_small;
        if (IsKeyPressed(KEY_KP_2)) PX_SetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx, current_amount - step);
        if (IsKeyPressed(KEY_KP_3)) PX_SetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx, current_amount + step);
    }
    else if (current_edit_target >= EDIT_TARGET_LFO_0_CORE_PARAMS && current_edit_target <= EDIT_TARGET_LFO_2_ROUTING) {
        int lfo_idx = (current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS) / 3;
        int lfo_function_type = (current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS) % 3;
        if (lfo_function_type == 0) { // Core
            float freq_step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? lfo_freq_step_large : lfo_freq_step_small;
            if (IsKeyPressed(KEY_KP_0)) PX_SetLFOWaveform(synth, lfo_idx, (PX_GetLFOWaveform(synth, lfo_idx) - 1 + PX_GetNumWaveforms()) % PX_GetNumWaveforms());
            if (IsKeyPressed(KEY_KP_1)) PX_SetLFOWaveform(synth, lfo_idx, (PX_GetLFOWaveform(synth, lfo_idx) + 1) % PX_GetNumWaveforms());
            if (IsKeyPressed(KEY_KP_2)) PX_SetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY, PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY) - freq_step);
            if (IsKeyPressed(KEY_KP_3)) PX_SetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY, PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY) + freq_step);
            if (IsKeyPressed(KEY_KP_8)) PX_SetLFOResetOnKeyOn(synth, lfo_idx, !PX_GetLFOResetOnKeyOn(synth, lfo_idx));
            if (IsKeyPressed(KEY_KP_9)) PX_SetLFOEnabled(synth, lfo_idx, !PX_GetLFOEnabled(synth, lfo_idx));
        } else if (lfo_function_type == 1) { // ADSR
            if (IsKeyPressed(KEY_KP_0)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK) - adsr_time_step_small);
            if (IsKeyPressed(KEY_KP_1)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK) + adsr_time_step_small);
            if (IsKeyPressed(KEY_KP_2)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY) - adsr_time_step_large);
            if (IsKeyPressed(KEY_KP_3)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY) + adsr_time_step_large);
            if (IsKeyPressed(KEY_KP_4)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN) - adsr_level_step);
            if (IsKeyPressed(KEY_KP_5)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN) + adsr_level_step);
            if (IsKeyPressed(KEY_KP_6)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE) - adsr_time_step_large);
            if (IsKeyPressed(KEY_KP_7)) PX_SetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE, PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE) + adsr_time_step_large);
            if (IsKeyPressed(KEY_KP_9)) PX_SetLFOADSREnabled(synth, lfo_idx, !PX_GetLFOADSREnabled(synth, lfo_idx));
        } else if (lfo_function_type == 2) { // Routing
            if (IsKeyPressed(KEY_KP_0)) current_editing_lfo_destination_idx = (current_editing_lfo_destination_idx - 1 + PX_LFO_DEST_COUNT) % PX_LFO_DEST_COUNT;
            if (IsKeyPressed(KEY_KP_1)) current_editing_lfo_destination_idx = (current_editing_lfo_destination_idx + 1) % PX_LFO_DEST_COUNT;
            float current_amount = PX_GetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx);
            float step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? lfo_route_amount_step_large : lfo_route_amount_step_small;
            if (IsKeyPressed(KEY_KP_2)) PX_SetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx, current_amount - step);
            if (IsKeyPressed(KEY_KP_3)) PX_SetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx, current_amount + step);
        }
    }
    else if (current_edit_target == EDIT_TARGET_FILTER_PARAMS) {
        if (IsKeyPressed(KEY_KP_1)) PX_SetFilterMode(synth, (PxFilterMode)((PX_GetFilterMode(synth) - 1 + PX_FILTER_MODE_COUNT) % PX_FILTER_MODE_COUNT));
        if (IsKeyPressed(KEY_KP_2)) PX_SetFilterMode(synth, (PxFilterMode)((PX_GetFilterMode(synth) + 1) % PX_FILTER_MODE_COUNT));
        float cutoff_step = (IsKeyDown(KEY_LEFT_SHIFT)) ? 500.0f : 50.0f;
        if (IsKeyPressed(KEY_KP_3)) PX_SetFilterParam(synth, PX_FILTER_PARAM_CUTOFF, PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF) - cutoff_step);
        if (IsKeyPressed(KEY_KP_4)) PX_SetFilterParam(synth, PX_FILTER_PARAM_CUTOFF, PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF) + cutoff_step);
        float res_step = (IsKeyDown(KEY_LEFT_SHIFT)) ? 1.0f : 0.1f;
        if (IsKeyPressed(KEY_KP_5)) PX_SetFilterParam(synth, PX_FILTER_PARAM_RESONANCE, PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE) - res_step);
        if (IsKeyPressed(KEY_KP_6)) PX_SetFilterParam(synth, PX_FILTER_PARAM_RESONANCE, PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE) + res_step);
        float env_amt_step = (IsKeyDown(KEY_LEFT_SHIFT)) ? 1000.0f : 100.0f;
        if (IsKeyPressed(KEY_KP_7)) PX_SetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT, PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT) - env_amt_step);
        if (IsKeyPressed(KEY_KP_8)) PX_SetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT, PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT) + env_amt_step);
        if (IsKeyPressed(KEY_KP_DIVIDE)) {
            int current_poles = (int)PX_GetFilterParam(synth, PX_FILTER_PARAM_POLES);
            int next_poles = current_poles + 1;
            if (next_poles > 4) next_poles = 2; // Cycle from 4 back to 2
            if (next_poles < 2) next_poles = 2;
            PX_SetFilterParam(synth, PX_FILTER_PARAM_POLES, (float)next_poles);
        }
        // Filter Drive and Key Tracking controls
        float drive_step = (IsKeyDown(KEY_LEFT_SHIFT)) ? 0.2f : 0.05f;
        if (IsKeyPressed(KEY_KP_9)) PX_SetFilterParam(synth, PX_FILTER_PARAM_DRIVE, PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE) - drive_step);
        if (IsKeyPressed(KEY_KP_MULTIPLY)) PX_SetFilterParam(synth, PX_FILTER_PARAM_DRIVE, PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE) + drive_step);
        float keytrack_step = 0.1f;
        if (IsKeyPressed(KEY_KP_SUBTRACT)) PX_SetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK, PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK) - keytrack_step);
        if (IsKeyPressed(KEY_KP_ADD)) PX_SetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK, PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK) + keytrack_step);
    }
    else if (current_edit_target == EDIT_TARGET_GLIDE_PARAMS) {
        if (IsKeyPressed(KEY_KP_0)) PX_SetGlideTime(synth, fmaxf(0.0f, PX_GetGlideTime(synth) - 0.05f));
        if (IsKeyPressed(KEY_KP_1)) PX_SetGlideTime(synth, PX_GetGlideTime(synth) + 0.05f);
        if (IsKeyPressed(KEY_KP_2)) {
            PxGlideMode m = PX_GetGlideMode(synth);
            if (m == PX_GLIDE_OFF) m = PX_GLIDE_STEP_LINEAR;
            else if (m == PX_GLIDE_STEP_LINEAR) m = PX_GLIDE_SMOOTH_RC;
            else m = PX_GLIDE_OFF;
            PX_SetGlideMode(synth, m);
        }
        if (IsKeyPressed(KEY_KP_8)) PX_SetGlideLegatoOnly(synth, !PX_GetGlideLegatoOnly(synth));
        if (IsKeyPressed(KEY_KP_9)) PX_SetGlideAlways(synth, !PX_GetGlideAlways(synth));
    }
    else if (current_edit_target == EDIT_TARGET_OSC_PARAMS) {
        float bc_step = 0.05f;
        if (IsKeyPressed(KEY_KP_0)) PX_SetOscBitcrush(synth, 0, PX_GetOscBitcrushEnabled(synth, 0), fmaxf(0.0f, PX_GetOscBitcrush(synth, 0) - bc_step));
        if (IsKeyPressed(KEY_KP_1)) PX_SetOscBitcrush(synth, 0, PX_GetOscBitcrushEnabled(synth, 0), fminf(1.0f, PX_GetOscBitcrush(synth, 0) + bc_step));
        if (IsKeyPressed(KEY_KP_9)) PX_SetOscBitcrush(synth, 0, !PX_GetOscBitcrushEnabled(synth, 0), PX_GetOscBitcrush(synth, 0));
    }

    // Note On/Off
    for (int i = 0; piano_keys[i].raylib_key != 0; ++i) {
        int key = piano_keys[i].raylib_key;
        if (IsKeyPressed(key)) {
            int midi_note = piano_keys[i].midi_note + octave_shift * 12;
            midi_note = (int)fmaxf(0.0f, fminf(127.0f, (float)midi_note));
            // Call the library function
            PX_NoteOn(synth, midi_note, current_wave_index, key, 1.0f);
        }
        if (IsKeyReleased(key)) {
            // Call the library function
            PX_NoteOff(synth, key);
        }
    }
}

static void UpdateAudio() {
    if (IsAudioStreamProcessed(audio_stream)) {
        PX_Process(synth, mix_buffer, SAMPLES_PER_UPDATE);
        UpdateAudioStream(audio_stream, mix_buffer, SAMPLES_PER_UPDATE);
    }
}

static void DrawLiveOscillator(int16_t* stereo_buffer, int sampleFrames, int x, int y, int width, int height) {
    if (sampleFrames <= 0) return;
    if (sampleFrames > SAMPLES_PER_UPDATE) sampleFrames = SAMPLES_PER_UPDATE;

    Vector2* points = (Vector2*)malloc(sampleFrames * sizeof(Vector2));
    if (!points) return;

    for (int i = 0; i < sampleFrames; ++i) {
        float sample_l = stereo_buffer[i * 2 + 0] / 32768.0f;
        float sample_r = stereo_buffer[i * 2 + 1] / 32768.0f;
        float mono_sample = (sample_l + sample_r) * 0.5f;
        points[i].x = x + (float)i / (sampleFrames > 1 ? (sampleFrames - 1) : 1) * width;
        points[i].y = y + height / 2.0f - mono_sample * (height / 2.0f);
    }
    DrawLine(x, y + height / 2, x + width, y + height / 2, LIGHTGRAY);
    if (sampleFrames > 1) {
        DrawLineStrip(points, sampleFrames, GREEN);
    } else if (sampleFrames == 1) {
        DrawPixelV(points[0], GREEN);
    }
    free(points);
}

static void DrawFrame() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // --- Local constants for layout ---
    int y_offset = 5;
    int line_height = 16;
    int small_line_height = 14;

    // --- Header / Help Text (Identical to v8, but updated for new keys) ---
    DrawText("Polysonix Synthesizer Player (v9 - Feature Complete)", 10, y_offset, line_height, DARKGRAY); y_offset += line_height + 2;
    // <<< NEW: Update Help Text
    DrawText("CTRL+L/R:Patch, UP/DN:Wave, L/R:Oct, F1/F2:LFO Rate, F3/F4:Pan, Keys:Play", 10, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    DrawText("KP_ENTER: Edit Target, KP0-9/etc: Edit Params/Routing", 10, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height + 3;

    // --- Currently Editing Target Display ---
    DrawText(TextFormat("EDITING: %s", edit_target_names[current_edit_target]), 10, y_offset, line_height, BLUE); y_offset += line_height;

    // --- Parameter Editing Display Block (structurally same as v8) ---
    if (current_edit_target >= EDIT_TARGET_ADSR_0_PARAMS && current_edit_target <= EDIT_TARGET_ADSR_2_PARAMS) {
        // This section displays the parameters for the currently selected Voice ADSR.
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_PARAMS;
        // All parameter values are now fetched from the synth via the API.
        DrawText(TextFormat("ADSR %d [%s]: A:%.2fs D:%.2fs S:%.2f R:%.2fs", adsr_idx,
            PX_GetVoiceADSREnabled(synth, adsr_idx) ? "ON" : "OFF",
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_ATTACK),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_DECAY),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_SUSTAIN),
            PX_GetVoiceADSRParam(synth, adsr_idx, PX_ADSR_PARAM_RELEASE)),
            20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
        DrawText("KP0/1:Atk, KP2/3:Dcy, KP4/5:Sus, KP6/7:Rel, KP9:En/Dis", 20, y_offset, small_line_height - 2, GRAY);
        y_offset += small_line_height -2;
    } else if (current_edit_target >= EDIT_TARGET_ADSR_0_ROUTING && current_edit_target <= EDIT_TARGET_ADSR_2_ROUTING) {
        // This section displays the routing for the currently selected Voice ADSR.
        int adsr_idx = current_edit_target - EDIT_TARGET_ADSR_0_ROUTING;
        // Destination name and modulation amount are fetched from the UI state and the library API.
        DrawText(TextFormat("ADSR %d Routing -> Dest (KP0/1): %s", adsr_idx, PX_GetADSRDestinationName((PxADSRDestination)current_editing_adsr_destination_idx)), 20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
        DrawText(TextFormat("Amount (KP2/3): %.2f", PX_GetVoiceADSRModAmount(synth, adsr_idx, (PxADSRDestination)current_editing_adsr_destination_idx)), 20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    } else if (current_edit_target >= EDIT_TARGET_LFO_0_CORE_PARAMS && current_edit_target <= EDIT_TARGET_LFO_2_ROUTING) {
        // This block handles all LFO-related editing displays.
        int base_lfo_offset = current_edit_target - EDIT_TARGET_LFO_0_CORE_PARAMS;
        int lfo_idx = base_lfo_offset / 3;
        int lfo_function_type = base_lfo_offset % 3;

        if (lfo_function_type == 0) { // LFO Core Parameters
            PxWaveInfo lfo_wave_info = PX_GetWaveInfo(PX_GetLFOWaveform(synth, lfo_idx));
            DrawText(TextFormat("LFO %d CORE PARAMETERS:", lfo_idx), 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
            DrawText(TextFormat("Wave (KP0/1): %s [%d]", lfo_wave_info.name, PX_GetLFOWaveform(synth, lfo_idx)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(TextFormat("Freq (KP2/3): %.2f Hz", PX_GetLFOParam(synth, lfo_idx, PX_LFO_PARAM_FREQUENCY)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(TextFormat("ResetOnKey (KP8): %s", PX_GetLFOResetOnKeyOn(synth, lfo_idx) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
            DrawText(TextFormat("LFO Enabled (KP9): %s", PX_GetLFOEnabled(synth, lfo_idx) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        } else if (lfo_function_type == 1) { // LFO ADSR Parameters
            DrawText(TextFormat("LFO %d ADSR PARAMETERS:", lfo_idx), 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
            DrawText(TextFormat("ADSR [%s]: A:%.2fs D:%.2fs S:%.2f R:%.2fs",
                PX_GetLFOADSREnabled(synth, lfo_idx) ? "ON" : "OFF",
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_ATTACK),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_DECAY),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_SUSTAIN),
                PX_GetLFOADSRParam(synth, lfo_idx, PX_ADSR_PARAM_RELEASE)),
                20, y_offset, small_line_height, DARKGRAY);
            y_offset += small_line_height;
            DrawText("KP0/1:Atk, KP2/3:Dcy, KP4/5:Sus, KP6/7:Rel, KP9:En/Dis ADSR", 20, y_offset, small_line_height - 2, GRAY);
            y_offset += small_line_height -2;
        } else if (lfo_function_type == 2) { // LFO Routing
            DrawText(TextFormat("LFO %d ROUTING -> Dest (KP0/1): %s", lfo_idx, PX_GetLFODestinationName((PxLFODestination)current_editing_lfo_destination_idx)), 20, y_offset, small_line_height, DARKBLUE);
            y_offset += small_line_height;
            float amount = PX_GetLFOModAmount(synth, lfo_idx, (PxLFODestination)current_editing_lfo_destination_idx);
            DrawText(TextFormat("Amount (KP2/3): %.2f", amount), 20, y_offset, small_line_height, DARKGRAY);
            y_offset += small_line_height;
        }
    } else if (current_edit_target == EDIT_TARGET_FILTER_PARAMS) {
        DrawText("FILTER PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        DrawText(TextFormat("Mode (KP1/2): %s", PX_GetFilterModeName(PX_GetFilterMode(synth))), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Slope (KP/): %ddB", (int)PX_GetFilterParam(synth, PX_FILTER_PARAM_POLES) * 6), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Cutoff (KP3/4): %.0f Hz", PX_GetFilterParam(synth, PX_FILTER_PARAM_CUTOFF)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Res (KP5/6): %.2f Q", PX_GetFilterParam(synth, PX_FILTER_PARAM_RESONANCE)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Env Amt (KP7/8): %.0f Hz", PX_GetFilterParam(synth, PX_FILTER_PARAM_ENV_AMOUNT)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Drive(KP9/*): %.2f  KeyTrk(KP-/+): %.2f",
            PX_GetFilterParam(synth, PX_FILTER_PARAM_DRIVE),
            PX_GetFilterParam(synth, PX_FILTER_PARAM_KEYTRACK)),
            20, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    } else if (current_edit_target == EDIT_TARGET_GLIDE_PARAMS) {
        DrawText("GLIDE PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        const char* mode_str = "OFF";
        PxGlideMode m = PX_GetGlideMode(synth);
        if (m == PX_GLIDE_STEP_LINEAR) mode_str = "LINEAR (Step)";
        if (m == PX_GLIDE_SMOOTH_RC) mode_str = "SMOOTH (RC)";
        DrawText(TextFormat("Mode (KP2): %s", mode_str), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Time (KP0/1): %.2fs", PX_GetGlideTime(synth)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Legato Only (KP8): %s", PX_GetGlideLegatoOnly(synth) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("Always Glide (KP9): %s", PX_GetGlideAlways(synth) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    } else if (current_edit_target == EDIT_TARGET_OSC_PARAMS) {
        DrawText("OSC 0 PARAMETERS:", 20, y_offset, small_line_height, DARKBLUE); y_offset += small_line_height;
        DrawText(TextFormat("Bitcrush (KP9): %s", PX_GetOscBitcrushEnabled(synth, 0) ? "ON" : "OFF"), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
        DrawText(TextFormat("BC Depth (KP0/1): %.2f", PX_GetOscBitcrush(synth, 0)), 20, y_offset, small_line_height, DARKGRAY); y_offset += small_line_height;
    }
    y_offset += 3;

    // --- Main Synth Status Display ---
    PxWaveInfo waveInfo = PX_GetWaveInfo(current_wave_index);
    DrawText(TextFormat("Osc Wave[%d]:%s Oct:%d", current_wave_index, waveInfo.name, octave_shift), 10, y_offset, small_line_height, waveInfo.is_compiled ? DARKBLUE : RED);
    y_offset += small_line_height;
    // <<< NEW: Display Current Patch
    DrawText(TextFormat("Patch [%d]: %s", current_patch_index, ROM_PATCHES[current_patch_index].name), 10, y_offset, small_line_height, DARKGREEN);
    y_offset += small_line_height;

    DrawText(TextFormat("Global Voice Pan (F3/F4): %.2f", PX_GetGlobalVoicePan(synth)), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;
    DrawText(TextFormat("Unilegato (F10): %s", PX_GetUnilegatoEnabled(synth) ? "ON" : "OFF"), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;

    // --- Template LFO Display ---
    for (int lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx) {
        PxLFOInfo lfo_info = PX_GetLFOInfo(synth, lfo_idx);
        PxWaveInfo lfo_wave_info = PX_GetWaveInfo(lfo_info.wave_idx);
        DrawText(TextFormat("TPL LFO %d[%s]:%s %.1fHz Rst:%s ADSR[%s]Lvl:%.2f", lfo_idx,
            lfo_info.enabled ? "ON" : "OFF", lfo_wave_info.name, lfo_info.frequency,
            lfo_info.reset_on_key_on ? "KEY" : "FREE", lfo_info.adsr_enabled ? "ON" : "OFF", lfo_info.adsr_level),
            10, y_offset, small_line_height, DARKGRAY);
        y_offset += small_line_height;
    }
    // LFO Update Rate display, restored from v8
    float lfo_interval = PX_GetLFOUpdateInterval(synth);
    float lfo_rate_hz = (lfo_interval > 0) ? 1000.0f / lfo_interval : 0.0f;
    float samples_per_update = (lfo_rate_hz > 0) ? (REQUESTED_SAMPLE_RATE / lfo_rate_hz) : 0;
    DrawText(TextFormat("LFO Update Rate: %.1f Hz (%.0f samples/update)", lfo_rate_hz, samples_per_update), 10, y_offset, small_line_height, DARKGRAY);
    y_offset += small_line_height;

    // --- Top-Right Status Block (Voice Count, LFO indicators, Limiter) ---
    int right_x = SCREEN_WIDTH - 250;
    int right_y_start = 10;
    int active_voice_count = 0;
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (PX_GetVoiceInfo(synth, i).active) active_voice_count++;
    }
    DrawText(TextFormat("Voices: %d/%d", active_voice_count, NUM_VOICES), SCREEN_WIDTH - 100, right_y_start, small_line_height, DARKGRAY);
    right_y_start += small_line_height;

    // LFO Indicator Dots
    for (int lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx) {
        PxLFOInfo lfo_info = PX_GetLFOInfo(synth, lfo_idx);
        float display_output = 0.0f;
        float raw_for_indicator = 0.0f;
        bool should_display_dot = false;

        if (lfo_info.enabled) {
            PxWaveInfo lfo_wave = PX_GetWaveInfo(lfo_info.wave_idx);
            if (lfo_wave.is_compiled) {
                raw_for_indicator = lfo_info.raw_output;
            }
            if (!lfo_info.adsr_enabled) {
                display_output = raw_for_indicator;
                should_display_dot = true;
            } else {
                display_output = raw_for_indicator * lfo_info.adsr_level;
                if (lfo_info.adsr_level > 0.001f) should_display_dot = true;
            }
        }
        DrawText(TextFormat("TPL LFO %d Out: %+.2f", lfo_idx, display_output), right_x, right_y_start, small_line_height, DARKGRAY);
        if (should_display_dot) {
            DrawLFOIndicator(lfo_info.raw_output, right_x + 160, right_y_start + 7, 7);
        }
        right_y_start += small_line_height;
    }
    // Limiter display
    PxLimiterInfo lim_info = PX_GetLimiterInfo(synth);
    if (lim_info.initialized) {
        DrawText(TextFormat("Limiter: -%.1fdB", lim_info.gain_reduction_db), SCREEN_WIDTH - 120, right_y_start, small_line_height, lim_info.gain_reduction_db > 0.1f ? RED : DARKGRAY);
    }

    // --- Voice Status Table (structurally identical to v8) ---
    int voice_display_y_start = y_offset + 10;
    int voice_line_h = small_line_height - 2;
    DrawText("VOICE STATUS:", 10, voice_display_y_start, line_height - 2, BLACK);
    voice_display_y_start += line_height - 2;

    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voice_display_y_start + i * voice_line_h > SCREEN_HEIGHT - DRAW_WAVEFORM_HEIGHT - 100 - voice_line_h) break;

        PxVoiceInfo v_info = PX_GetVoiceInfo(synth, i);
        char voice_status_text[256];
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

        snprintf(voice_status_text, sizeof(voice_status_text), "V%d:%s N:%02d F:%.0f EAmp:%.1f P:%.1f %s%s",
                 i, v_info.active ? "On" : "Off", v_info.midi_note, v_info.frequency,
                 v_info.effective_amplitude, v_info.pan_position, adsr_summary, lfo_outputs_str);

        DrawText(voice_status_text, 10, voice_display_y_start + i * voice_line_h, voice_line_h, v_info.active ? DARKGREEN : GRAY);
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
                display_params.x = ((float)k / SINGLE_CYCLE_LENGTH) * 2.0f * PI;
                float s_f = execute_bytecode(default_waves[current_wave_index].compiled_bytecode, &display_params);
                static_display_buffer[k] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, s_f)) * 32767.0f);
            }
        }
        last_drawn_wave_index = current_wave_index;
        last_wave_compile_status = waveInfo.is_compiled;
    }

    int wf_x = 10;
    int wf_w = SCREEN_WIDTH - 20;
    DrawRectangleLines(wf_x - 1, waveform_draw_y - 1, wf_w + 2, DRAW_WAVEFORM_HEIGHT + 2, LIGHTGRAY);
    if (waveInfo.is_compiled) {
        Vector2 pts[SINGLE_CYCLE_LENGTH];
        DrawLine(wf_x, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2, wf_x + wf_w, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2, LIGHTGRAY);
        for (int k = 0; k < SINGLE_CYCLE_LENGTH; ++k) {
            pts[k].x = wf_x + (float)k / (SINGLE_CYCLE_LENGTH - 1) * wf_w;
            pts[k].y = waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2.0f - (static_display_buffer[k] / 32768.0f) * (DRAW_WAVEFORM_HEIGHT / 2.0f);
        }
        DrawLineStrip(pts, SINGLE_CYCLE_LENGTH, MAROON);
    } else {
        DrawText("Wave compilation failed!", wf_x + wf_w / 2 - MeasureText("Wave compilation failed!", 20) / 2, waveform_draw_y + DRAW_WAVEFORM_HEIGHT / 2 - 10, 20, RED);
    }

    // --- Live Output Display (Identical to v8) ---
    DrawLiveOscillator(mix_buffer, SAMPLES_PER_UPDATE, 10, waveform_draw_y + DRAW_WAVEFORM_HEIGHT + 10, SCREEN_WIDTH - 20, 80);

    EndDrawing();
}

int main(void) {
    if (!InitializeApplication()) {
        printf("Application initialization failed.\n");
        return 1;
    }

    while (!WindowShouldClose()) {
        ProcessInput();
        UpdateAudio();

        if (enable_drawing) {
            DrawFrame();
        } else {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("DRAWING DISABLED (F11 to toggle)", 10, 10, 20, RAYWHITE);
            EndDrawing();
        }
    }

    CleanupApplication();
    return 0;
}