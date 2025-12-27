/***************************************************************************************************
*   Situation Library - Example: Terminal Piano Roll
*   ------------------------------------------------
*   A synthesizer that visualizes a piano roll in the terminal stdout while using the
*   Situation library for low-latency audio synthesis and input handling.
*
*   Features:
*   - Polyphonic Synthesis (Sine, Square, Triangle, Saw, Noise)
*   - ADSR Envelope
*   - Terminal Visualization (ANSI Escape Codes)
*   - "Carrier Sound" technique for pure synthesis (via Stream)
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Required backend selection
#include "situation.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

// --- Configuration ---
#define SAMPLE_RATE 48000
#define MAX_VOICES 32
#define MIDI_NOTE_C3 48

// --- Synthesizer State ---
typedef struct {
    float freq;
    float phase;
    bool active;
    float envelope_time;
    bool releasing;
    float release_start_level;
} Voice;

typedef struct {
    // Shared State (Input Thread writes, Audio Thread reads)
    _Atomic int wave_type; // 0=Sine, 1=Square, 2=Tri, 3=Saw, 4=Noise
    _Atomic bool keys_held[128]; // Mapped by logical note index (0 = C3)

    // Audio Thread State
    Voice voices[MAX_VOICES];

    // ADSR Config
    float attack;
    float decay;
    float sustain;
    float release;
} SynthState;

static SynthState g_synth = {
    .wave_type = 0, // Sine
    .attack = 0.05f,
    .decay = 0.1f,
    .sustain = 0.7f,
    .release = 0.3f
};

// --- Audio Processor ( The Synth ) ---
static void SynthAudioCallback(float* buffer, uint32_t frames, uint32_t channels, uint32_t sample_rate, void* user_data) {
    SynthState* synth = (SynthState*)user_data;
    float dt = 1.0f / (float)sample_rate;
    int wave = atomic_load(&synth->wave_type);

    // 1. Voice Management (Simple allocation)
    // Check all possible keys (0-60 range approx)
    for (int note = 0; note < 60; ++note) {
        bool key_down = atomic_load(&synth->keys_held[note]);

        // Find if this note is already playing
        int voice_idx = -1;
        int free_idx = -1;
        for (int v = 0; v < MAX_VOICES; ++v) {
            if (synth->voices[v].active) {
                // If freq matches roughly (using integer note ID would be better but freq check works for monophonic per key)
                if (fabsf(synth->voices[v].freq - SITUATION_MIDI_NOTE_FREQUENCY[note + MIDI_NOTE_C3]) < 0.1f) {
                    voice_idx = v;
                }
            } else {
                if (free_idx == -1) free_idx = v;
            }
        }

        if (key_down) {
            if (voice_idx != -1) {
                // Already playing, ensure not releasing
                synth->voices[voice_idx].releasing = false;
            } else if (free_idx != -1) {
                // Start new voice
                synth->voices[free_idx].active = true;
                synth->voices[free_idx].freq = SITUATION_MIDI_NOTE_FREQUENCY[note + MIDI_NOTE_C3];
                synth->voices[free_idx].phase = 0.0f;
                synth->voices[free_idx].envelope_time = 0.0f;
                synth->voices[free_idx].releasing = false;
            }
        } else {
            if (voice_idx != -1 && !synth->voices[voice_idx].releasing) {
                // Key released, enter release phase
                synth->voices[voice_idx].releasing = true;
            }
        }
    }

    // 2. Synthesis Loop
    for (uint32_t i = 0; i < frames; ++i) {
        float sample_l = 0.0f;
        float sample_r = 0.0f;
        int active_count = 0;

        for (int v = 0; v < MAX_VOICES; ++v) {
            if (!synth->voices[v].active) continue;

            Voice* vc = &synth->voices[v];
            active_count++;

            // --- Envelope ---
            float amp = 0.0f;
            if (!vc->releasing) {
                vc->envelope_time += dt;
                if (vc->envelope_time < synth->attack) {
                    amp = vc->envelope_time / synth->attack;
                } else if (vc->envelope_time < synth->attack + synth->decay) {
                    float decay_progress = (vc->envelope_time - synth->attack) / synth->decay;
                    amp = 1.0f - decay_progress * (1.0f - synth->sustain);
                } else {
                    amp = synth->sustain;
                }
                vc->release_start_level = amp; // Track for release
            } else {
                // Release Phase
                vc->release_start_level -= (dt / synth->release);
                if (vc->release_start_level <= 0.0f) {
                    vc->release_start_level = 0.0f;
                    vc->active = false;
                }
                amp = vc->release_start_level;
            }

            // --- Oscillator ---
            float osc = 0.0f;
            vc->phase += vc->freq * dt;
            if (vc->phase > 1.0f) vc->phase -= 1.0f;

            switch (wave) {
                case 0: // Sine
                    osc = sinf(vc->phase * 6.283185f);
                    break;
                case 1: // Square
                    osc = (vc->phase < 0.5f) ? 1.0f : -1.0f;
                    break;
                case 2: // Triangle
                    osc = 4.0f * fabsf(vc->phase - 0.5f) - 1.0f;
                    break;
                case 3: // Saw
                    osc = 2.0f * (vc->phase - 0.5f);
                    break;
                case 4: // Noise
                    osc = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
                    break;
            }

            sample_l += osc * amp;
            sample_r += osc * amp; // Mono to Stereo
        }

        // Master Mix
        if (active_count > 0) {
            float master_gain = 0.3f; // Prevent clipping
            buffer[i * channels + 0] = sample_l * master_gain;
            if (channels > 1) buffer[i * channels + 1] = sample_r * master_gain;
        } else {
             // If we rely on SituationLoadSoundFromStream returning zeros, we can just += or =
             // Since we know we are generating, overwriting is safer/cleaner.
             buffer[i * channels + 0] = 0.0f;
             if (channels > 1) buffer[i * channels + 1] = 0.0f;
        }
    }
}

typedef struct {
    int scancode;
    int note;
} KeyMapping;

static KeyMapping active_keys[48];
static int active_key_count = 0;
static bool g_debug_mode = false;

// --- Key Mapping ---
// Maps physical scancodes to "Piano Roll" indices (0 = C3).
int get_piano_note_from_scancode(int scancode) {
    // Linear search (small set)
    for (int i = 0; i < active_key_count; ++i) {
        if (active_keys[i].scancode == scancode) {
            return active_keys[i].note;
        }
    }
    return -1;
}

// Initialize mappings at startup
void init_scancode_mapping(void) {
    // Define logical keys for US layout positions
    int keys_row_z[] = {SIT_KEY_Z, SIT_KEY_X, SIT_KEY_C, SIT_KEY_V, SIT_KEY_B, SIT_KEY_N, SIT_KEY_M, SIT_KEY_COMMA, SIT_KEY_PERIOD, SIT_KEY_SLASH, 0, 0};
    int notes_row_z[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 0, 0};

    int keys_row_a[] = {SIT_KEY_A, SIT_KEY_S, SIT_KEY_D, SIT_KEY_F, SIT_KEY_G, SIT_KEY_H, SIT_KEY_J, SIT_KEY_K, SIT_KEY_L, SIT_KEY_SEMICOLON, SIT_KEY_APOSTROPHE, 0};
    int notes_row_a[] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22, 25, 0};

    int keys_row_q[] = {SIT_KEY_Q, SIT_KEY_W, SIT_KEY_E, SIT_KEY_R, SIT_KEY_T, SIT_KEY_Y, SIT_KEY_U, SIT_KEY_I, SIT_KEY_O, SIT_KEY_P, SIT_KEY_LEFT_BRACKET, SIT_KEY_RIGHT_BRACKET};
    int notes_row_q[] = {12, 14, 16, 17, 19, 21, 23, 24, 26, 28, 29, 31};

    int keys_row_1[] = {SIT_KEY_1, SIT_KEY_2, SIT_KEY_3, SIT_KEY_4, SIT_KEY_5, SIT_KEY_6, SIT_KEY_7, SIT_KEY_8, SIT_KEY_9, SIT_KEY_0, SIT_KEY_MINUS, SIT_KEY_EQUAL};
    int notes_row_1[] = {13, 15, 18, 20, 22, 25, 27, 30, 32, 34, 37, 39};

    // Unroll setup
    printf("\n--- Scancode Resolution (US Layout Assumption) ---\n");
    active_key_count = 0;

    printf("Row Z (Bottom): ");
    for(int i=0; i<10; ++i) {
        if (keys_row_z[i] == 0) continue;
        int sc = SituationGetKeyScancode(keys_row_z[i]);
        active_keys[active_key_count++] = (KeyMapping){ .scancode = sc, .note = notes_row_z[i] };
        printf("[%d] ", sc);
    }
    printf("\nRow A (Middle): ");
    for(int i=0; i<11; ++i) {
        if (keys_row_a[i] == 0) continue;
        int sc = SituationGetKeyScancode(keys_row_a[i]);
        active_keys[active_key_count++] = (KeyMapping){ .scancode = sc, .note = notes_row_a[i] };
        printf("[%d] ", sc);
    }
    printf("\nRow Q (Top)   : ");
    for(int i=0; i<12; ++i) {
        int sc = SituationGetKeyScancode(keys_row_q[i]);
        active_keys[active_key_count++] = (KeyMapping){ .scancode = sc, .note = notes_row_q[i] };
        printf("[%d] ", sc);
    }
    printf("\nRow 1 (Num)   : ");
    for(int i=0; i<12; ++i) {
        int sc = SituationGetKeyScancode(keys_row_1[i]);
        active_keys[active_key_count++] = (KeyMapping){ .scancode = sc, .note = notes_row_1[i] };
        printf("[%d] ", sc);
    }
    printf("\n--------------------------------------------------\n");
}

// --- Visuals ---
void render_terminal_ui(void) {
    // Reset Cursor
    printf("\033[H");

    // Header
    printf("\033[1;36mSituation Piano Roll\033[0m\n");
    printf("Controls: \033[33mZ/Q\033[0m=Whites, \033[33mA/1\033[0m=Blacks, \033[33mF1-F5\033[0m=Waveform\n");
    printf("Current Wave: \033[1;32m%s\033[0m\n\n",
        g_synth.wave_type == 0 ? "Sine" :
        g_synth.wave_type == 1 ? "Square" :
        g_synth.wave_type == 2 ? "Triangle" :
        g_synth.wave_type == 3 ? "Saw" : "Noise");

    // Draw Piano Roll (2 Octaves Visualization)
    // We'll draw 4 rows to match the keyboard layout style

    // --- Row 4 (Blacks +1) ---
    printf("   ");
    const char* labels_1[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
    static const int note_1[] = {13, 15, 18, 20, 22, 25, 27, 30, 32, 34, 37, 39};
    for(int i=0; i<12; i++) {
        bool pressed = atomic_load(&g_synth.keys_held[note_1[i]]);
        if(pressed) printf("\033[1;42;37m %s \033[0m", labels_1[i]);
        else        printf("\033[1;30;47m %s \033[0m", labels_1[i]);
    }
    printf("\n");

    // --- Row 3 (Whites +1) ---
    const char* labels_q[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]"};
    static const int note_q[] = {12, 14, 16, 17, 19, 21, 23, 24, 26, 28, 29, 31};
    for(int i=0; i<12; i++) {
        bool pressed = atomic_load(&g_synth.keys_held[note_q[i]]);
        if(pressed) printf("\033[1;42;37m %s \033[0m", labels_q[i]);
        else        printf("\033[1;37;40m %s \033[0m", labels_q[i]);
    }
    printf("\n");

    // --- Row 2 (Blacks) ---
    printf("   ");
    const char* labels_a[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"};
    static const int note_a[] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22, 25};
    for(int i=0; i<11; i++) {
        bool pressed = atomic_load(&g_synth.keys_held[note_a[i]]);
        if(pressed) printf("\033[1;42;37m %s \033[0m", labels_a[i]);
        else        printf("\033[1;30;47m %s \033[0m", labels_a[i]);
    }
    printf("\n");

    // --- Row 1 (Whites) ---
    const char* labels_z[] = {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};
    static const int note_z[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16};
    for(int i=0; i<10; i++) {
        bool pressed = atomic_load(&g_synth.keys_held[note_z[i]]);
        if(pressed) printf("\033[1;42;37m %s \033[0m", labels_z[i]);
        else        printf("\033[1;37;40m %s \033[0m", labels_z[i]);
    }
    printf("\n");
}

// --- Silent Stream Callback ---
static ma_uint64 SilentStreamRead(void* pUserData, void* pBufferOut, ma_uint64 bytesToRead) {
    (void)pUserData;
    memset(pBufferOut, 0, bytesToRead);
    return bytesToRead; // Infinite stream of silence
}

int main(int argc, char** argv) {
    // 1. Situation Init
    SituationInitInfo config = {
        .window_title = "Piano Roll Input",
        .window_width = 400,
        .window_height = 200,
        .max_audio_voices = 64 // Ensure enough internal voices
    };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        return 1;
    }

    // 2. Setup Audio "Carrier"
    // We create a stream of silence.
    // NOTE: SituationLoadSoundFromStream prototype:
    // SITAPI SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound);

    SituationAudioFormat format = {
        .sample_rate = SAMPLE_RATE,
        .channels = 2,
        .bit_depth = 32 // Float
    };

    SituationSound carrier_sound = {0};
    SituationLoadSoundFromStream(SilentStreamRead, NULL, NULL, &format, true, &carrier_sound);

    // 3. Attach Synth
    SituationAttachAudioProcessor(&carrier_sound, SynthAudioCallback, &g_synth);
    SituationPlayLoadedSound(&carrier_sound);

    // Initialize Scancode Mapping
    init_scancode_mapping();

    // Clear Terminal Screen once
    printf("\033[2J");

    // 4. Main Loop
    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME(); // Poll Input

        // Debug Toggle
        if (SituationIsKeyPressed(SIT_KEY_TAB)) {
            g_debug_mode = !g_debug_mode;
            printf("\033[2J"); // Clear screen on toggle
        }

        if (g_debug_mode) {
            // --- DEBUG MODE ---
            printf("\033[H\033[1;33m[DEBUG MODE] Press any key to see its logical ID and physical Scancode.\033[0m\n");
            printf("Press TAB to return to Piano.\n\n");

            int sc = 0;
            int key = SituationGetKeyPressedEx(&sc);
            if (key != 0 || sc != 0) {
                // If a key event occurred
                printf("Event -> Key: \033[1;36m%3d\033[0m | Scancode: \033[1;32m%3d\033[0m | Note: %d\n",
                    key, sc, get_piano_note_from_scancode(sc));
            }

            // Minimal sleep to avoid terminal spam if we were polling (but we are event driven here)
        } else {
            // --- PIANO MODE ---

            // 1. Waveform Selection
            if (SituationIsKeyPressed(SIT_KEY_F1)) atomic_store(&g_synth.wave_type, 0);
            if (SituationIsKeyPressed(SIT_KEY_F2)) atomic_store(&g_synth.wave_type, 1);
            if (SituationIsKeyPressed(SIT_KEY_F3)) atomic_store(&g_synth.wave_type, 2);
            if (SituationIsKeyPressed(SIT_KEY_F4)) atomic_store(&g_synth.wave_type, 3);
            if (SituationIsKeyPressed(SIT_KEY_F5)) atomic_store(&g_synth.wave_type, 4);

            // 2. Update Key States using Scancodes
            // We iterate our cached scancodes to check their state directly.
            // This is the efficient, "Positional Scancode" way: we only check the physical keys we care about.
            for (int i = 0; i < active_key_count; ++i) {
                bool down = SituationIsScancodeDown(active_keys[i].scancode);
                atomic_store(&g_synth.keys_held[active_keys[i].note], down);
            }

            // --- Render Terminal ---
            render_terminal_ui();
        }

        // --- Render Window (Minimal) ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {10, 10, 10, 255} } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            // We could draw text here if we loaded a font, but the prompt says "terminal display".
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    // Cleanup
    SituationStopLoadedSound(&carrier_sound);
    SituationUnloadSound(&carrier_sound);
    SituationShutdown();

    return 0;
}
