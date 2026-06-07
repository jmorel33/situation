/*
 * Digital Rain - A Creative Situation Demo
 * 
 * Inspired by The Matrix, but with temporal oscillators driving
 * the animation for perfectly synchronized chaos.
 * 
 * Features:
 * - Multiple rain columns with varying speeds
 * - Temporal oscillators for rhythm
 * - Color gradients and fading effects
 * - Dynamic character cycling
 * - Smooth wave patterns
 */

#ifndef SITUATION_USE_SHARED
    #define SITUATION_IMPLEMENTATION
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER
#include "../situation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_COLUMNS 80
#define MAX_ROWS 60
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// Rain column state
typedef struct {
    float y_position;
    float speed;
    int length;
    int oscillator_id;
    float phase_offset;
    uint8_t brightness;
} RainColumn;

// Audio visualization buffer - circular buffer for scrolling oscilloscope
#define WAVEFORM_SAMPLES 512
float waveform_buffer[WAVEFORM_SAMPLES] = {0};
volatile int waveform_write_pos = 0;  // Volatile because written from audio thread

// Audio monitor callback - called from audio thread
void audio_monitor_callback(const float* samples, uint32_t frame_count, void* user_data) {
    (void)user_data;
    
    // Scroll samples into circular buffer (mono - just use left channel)
    // samples are interleaved stereo: [L, R, L, R, ...]
    for (uint32_t i = 0; i < frame_count; i++) {
        waveform_buffer[waveform_write_pos] = samples[i * 2];  // Left channel only
        waveform_write_pos = (waveform_write_pos + 1) % WAVEFORM_SAMPLES;
    }
}

// Debug: Track tone activity
typedef struct {
    int active_tone_count;
    float last_sample_values[8];
    int frames_since_tone_start;
} AudioDebugInfo;

AudioDebugInfo audio_debug = {0};

RainColumn columns[MAX_COLUMNS];

// Character set for the rain (mix of ASCII and symbols)
const char* rain_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%^&*()_+-=[]{}|;:,.<>?/~`";

// Piano keyboard mapping (2 octaves)
typedef struct {
    int key;
    int midi_note;
} KeyNoteMapping;

KeyNoteMapping piano_keys[] = {
    // Lower octave (ZSXDCVGBHNJM row)
    { SIT_KEY_Z, 60 }, { SIT_KEY_S, 61 }, { SIT_KEY_X, 62 }, { SIT_KEY_D, 63 }, 
    { SIT_KEY_C, 64 }, { SIT_KEY_V, 65 }, { SIT_KEY_G, 66 }, { SIT_KEY_B, 67 }, 
    { SIT_KEY_H, 68 }, { SIT_KEY_N, 69 }, { SIT_KEY_J, 70 }, { SIT_KEY_M, 71 },
    { SIT_KEY_COMMA, 72 }, { SIT_KEY_L, 73 }, { SIT_KEY_PERIOD, 74 }, 
    { SIT_KEY_SEMICOLON, 75 }, { SIT_KEY_SLASH, 76 },
    
    // Upper octave (QWERTY row)
    { SIT_KEY_Q, 72 }, { SIT_KEY_2, 73 }, { SIT_KEY_W, 74 }, { SIT_KEY_3, 75 }, 
    { SIT_KEY_E, 76 }, { SIT_KEY_R, 77 }, { SIT_KEY_5, 78 }, { SIT_KEY_T, 79 }, 
    { SIT_KEY_6, 80 }, { SIT_KEY_Y, 81 }, { SIT_KEY_7, 82 }, { SIT_KEY_U, 83 }, 
    { SIT_KEY_I, 84 }, { SIT_KEY_9, 85 }, { SIT_KEY_O, 86 }, { SIT_KEY_0, 87 }, 
    { SIT_KEY_P, 88 },
    
    { 0, -1 }  // Terminator
};

// Initialize a rain column  
void InitColumn(RainColumn* col, int index, int window_height) {
    col->y_position = window_height + (rand() % (window_height / 2));  // Start below screen
    col->speed = -(50.0f + (rand() % 150));  // NEGATIVE = moves toward y=0 (upward on screen)
    col->length = 10 + (rand() % 20);
    col->oscillator_id = index % SITUATION_MAX_OSCILLATORS;
    col->phase_offset = (float)(rand() % 360) * 0.0174533f;  // Random phase in radians
    col->brightness = 180 + (rand() % 75);
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("     DIGITAL RAIN - Situation Demo\n");
    printf("========================================\n");
    printf("A creative exploration of temporal\n");
    printf("oscillators and text rendering.\n");
    printf("========================================\n\n");

    srand((unsigned int)time(NULL));

    SituationInitInfo init_info = {
        .window_width = 1280,
        .window_height = 960,
        .window_title = "Digital Rain - Situation Creative Demo",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,  // Enable VSync to prevent 100% CPU usage
        .enable_vulkan_validation = false
    };

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        printf("Failed to initialize Situation!\n");
        return -1;
    }

    // === THREADING DIAGNOSTICS ===
    printf("\n=== THREADING STATUS ===\n");
    printf("SITUATION_ENABLE_THREADING is defined: YES\n");
    printf("CPU thread count: %d\n", SituationGetCPUThreadCount());
    printf("Expected worker threads: %d\n", SituationGetCPUThreadCount() - 1);
    printf("I/O thread should be created: YES (disable_io_thread not set)\n");
    printf("Audio thread (miniaudio): Should be created automatically\n");
    printf("Total expected threads: 1 (main) + %d (workers) + 1 (I/O) + 1 (audio) = %d\n",
           SituationGetCPUThreadCount() - 1,
           SituationGetCPUThreadCount() + 2);
    printf("========================\n\n");

    printf("Initialization complete!\n");
    
    bool audio_enabled = true;
    
    // Initialize audio device (required!)
    printf("Starting audio device...\n");
    
    // Try with explicit format to avoid any format mismatch issues
    SituationAudioFormat audio_fmt = {
        .channels = 2,
        .sample_rate = 48000,
        .bit_depth = 32  // float32
    };
    
    // List all available audio devices
    printf("\n=== AVAILABLE AUDIO DEVICES ===\n");
    int device_count = 0;
    SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
    int selected_device = 0;  // Default to device 0
    
    if (devices && device_count > 0) {
        for (int i = 0; i < device_count; i++) {
            printf("  [%d] %s\n", i, devices[i].name);
            
            // Auto-select Anker Soundsync if available
            if (strstr(devices[i].name, "Anker") != NULL || 
                strstr(devices[i].name, "Speakers") != NULL) {
                selected_device = i;
            }
        }
        SIT_FREE(devices);
    } else {
        printf("  No devices found!\n");
    }
    printf("================================\n");
    printf("Using device [%d]\n\n", selected_device);
    
    SituationError audio_result = SituationSetAudioDevice(selected_device, &audio_fmt);
    if (audio_result != SITUATION_SUCCESS) {
        printf("ERROR: Failed to initialize audio device (error code: %d)\n", audio_result);
        printf("Audio will be disabled.\n");
        audio_enabled = false;
    } else {
        printf("Audio device initialized successfully!\n");
        
        // Check if device is actually playing
        bool is_playing = SituationIsAudioDevicePlaying();
        printf("Audio device playing status: %s\n", is_playing ? "YES" : "NO");
        
        // Check sample rate
        int sample_rate = SituationGetAudioPlaybackSampleRate();
        printf("Audio sample rate: %d Hz\n", sample_rate);
        
        // Check master volume
        float volume = SituationGetAudioMasterVolume();
        printf("Master volume: %.2f\n", volume);
        
        printf("\nDEBUG: A 440Hz test tone should play CONTINUOUSLY from the audio callback.\n");
        printf("This bypasses ALL tone generation code to test the audio path.\n");
        printf("If you don't hear it, the audio device or callback has an issue.\n");
        
        // REMOVED: Test tone that was causing ticks
        // if (is_playing) {
        //     printf("\nPlaying test beep (440Hz sine wave, loud volume)...\n");
        //     SituationPlayTone(SIT_WAVE_SQUARE, 440.0f, 1.0f, 0.01f, 0.1f, 0.9f, 0.5f, 1.0f);
        //     printf("Test tone sent. You should hear a beep for about 1.5 seconds.\n");
        // }
    }
    
    printf("\nPress ESC to exit, SPACE to reset, M to toggle audio\n\n");

    // Register audio output monitor for waveform visualization
    SituationSetAudioOutputMonitor(audio_monitor_callback, NULL);
    printf("Audio output monitor registered for visualization\n\n");

    // Initialize rain columns
    for (int i = 0; i < MAX_COLUMNS; i++) {
        InitColumn(&columns[i], i, init_info.window_height);
    }

    // Setup temporal oscillators for rhythm variations
    for (int i = 0; i < SITUATION_MAX_OSCILLATORS && i < 16; i++) {
        double period = 0.1 + (i * 0.05);  // Varying periods
        SituationSetTimerOscillatorPeriod(i, period);
    }

    SituationFont font = {0};  // Use default font
    int frame = 0;
    double last_time = SituationTimerGetTime();
    
    // Track which oscillators triggered last frame for audio
    bool prev_osc_states[16] = {false};

    // Main loop
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        double current_time = SituationTimerGetTime();
        float delta_time = (float)(current_time - last_time);
        last_time = current_time;

        // SPACE: Play test tone
        static bool space_was_pressed = false;
        bool space_is_pressed = SituationIsKeyPressed(SIT_KEY_SPACE);
        
        if (space_is_pressed && !space_was_pressed) {
            printf("\n=== SPACE: Test Tone ===\n");
            SituationPlayTone(SIT_WAVE_SQUARE, 880.0f, 0.5f, 0.01f, 0.05f, 0.8f, 0.2f, 0.3f);
        }
        space_was_pressed = space_is_pressed;

        // Piano keyboard input (use IsKeyDown for immediate response)
        static bool prev_key_states[256] = {false};
        
        for (int i = 0; piano_keys[i].key != 0; i++) {
            int key = piano_keys[i].key;
            bool is_down = SituationIsKeyDown(key);
            
            // Trigger on key press (transition from up to down)
            if (is_down && !prev_key_states[key]) {
                double now = SituationTimerGetTime();
                printf("[KEY %.3fms] Note %d pressed\n", now * 1000.0, piano_keys[i].midi_note);
                
                // Play note with sine wave, short envelope
                SituationPlayMidiNote(
                    piano_keys[i].midi_note,
                    SIT_WAVE_SINE,
                    0.8f,      // Volume (increased from 0.3 to 0.8 for better audibility)
                    0.001f,    // Attack: 1ms (instant)
                    0.01f,     // Decay: 10ms
                    0.7f,      // Sustain: 70%
                    0.05f,     // Release: 50ms
                    0.15f      // Hold: 150ms (longer for better note clarity)
                );
                
                printf("[KEY %.3fms] Tone triggered\n", SituationTimerGetTime() * 1000.0);
            }
            
            prev_key_states[key] = is_down;
        }

        // Exit on ESC
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }

        // NO OTHER AUDIO - all oscillator code disabled
        
        /* ORIGINAL CODE - DISABLED FOR TESTING
        // Only check every few frames to reduce mutex contention with audio thread
        static int audio_check_counter = 0;
        audio_check_counter++;
        
        if (audio_enabled && audio_check_counter >= 3) {  // Check every 3 frames (~50ms at 60fps)
            audio_check_counter = 0;
            
            for (int i = 0; i < 16; i++) {
                bool current_state = SituationTimerGetOscillatorState(i);
                
                // Trigger sound on rising edge (state change from false to true)
                if (current_state && !prev_osc_states[i]) {
                    // Create a pentatonic scale note based on oscillator index
                    // Pentatonic: C, D, E, G, A (MIDI: 60, 62, 64, 67, 69)
                    int pentatonic_scale[] = {60, 62, 64, 67, 69};
                    int octave_offset = (i / 5) * 12;  // Spread across octaves
                    int note = pentatonic_scale[i % 5] + octave_offset - 12;  // Start one octave lower
                    
                    // Vary the wave type based on oscillator
                    SituationWaveType wave = (i % 4 == 0) ? SIT_WAVE_SINE : 
                                            (i % 4 == 1) ? SIT_WAVE_TRIANGLE :
                                            (i % 4 == 2) ? SIT_WAVE_SQUARE : SIT_WAVE_SAW;
                    
                    // Much quieter volume to avoid clipping when multiple notes play
                    float volume = 0.08f;  // 8% volume per note
                    float attack = 0.05f;
                    float decay = 0.1f;
                    float sustain = 0.6f;
                    float release = 0.3f;
                    float hold = 0.1f;
                    
                    // Debug output - only print occasionally to avoid blocking
                    if (frame % 120 == 0) {  // Once every 2 seconds at 60fps
                        printf("Playing note %d (osc %d) with wave type %d\n", note, i, wave);
                    }
                    
                    SituationPlayMidiNote(note, wave, volume, attack, decay, sustain, release, hold);
                }
                
                prev_osc_states[i] = current_state;
            }
        }
        */

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            // Begin render pass with black background
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {0, 0, 0, 255} }
                }
            };
            
            SituationCmdBeginRenderPass(cmd, &pass);

            // Update and render each column
            for (int col = 0; col < MAX_COLUMNS; col++) {
                RainColumn* rain = &columns[col];
                
                // Update position (speed is negative, so this moves UP)
                rain->y_position += rain->speed * delta_time;
                
                // Reset if off screen (went above top)
                if (rain->y_position < -(rain->length * CHAR_HEIGHT)) {
                    InitColumn(rain, col, init_info.window_height);
                }

                // Get oscillator state for this column
                bool osc_state = SituationTimerGetOscillatorState(rain->oscillator_id);
                float osc_progress = (float)SituationTimerGetPingProgress(rain->oscillator_id);
                
                // Draw the rain trail (extends UPWARD from head since we're moving up)
                for (int i = 0; i < rain->length; i++) {
                    float y = rain->y_position - (i * CHAR_HEIGHT);  // Trail extends upward (lower y values)
                    
                    // Skip if off screen
                    if (y < -CHAR_HEIGHT || y > init_info.window_height) continue;
                    
                    // Calculate fade based on position in trail
                    float fade = 1.0f - ((float)i / rain->length);
                    
                    // Add oscillator influence
                    if (osc_state) {
                        fade *= (0.7f + 0.3f * osc_progress);
                    }
                    
                    // Calculate color with green tint and brightness variation
                    uint8_t green = (uint8_t)(rain->brightness * fade);
                    uint8_t red = (uint8_t)(green * 0.3f);
                    uint8_t blue = (uint8_t)(green * 0.3f);
                    
                    // TAIL of the trail is brighter (white-ish) - Matrix style!
                    if (i == rain->length - 1) {
                        red = green = blue = 255;
                    }
                    
                    // Pick a character (cycle based on frame and position)
                    int char_index = (frame + col + i) % strlen(rain_chars);
                    char single_char[2] = {rain_chars[char_index], '\0'};
                    
                    // Add wave effect to x position
                    float wave = sin(current_time * 2.0f + rain->phase_offset + i * 0.2f) * 3.0f;
                    float x = col * (init_info.window_width / (float)MAX_COLUMNS) + wave;
                    
                    SituationCmdDrawText(cmd, font, single_char,
                        (Vector2){x, y},
                        (ColorRGBA){red, green, blue, (uint8_t)(255 * fade)});
                }
            }

            // Draw title overlay with pulsing effect
            float pulse = 0.7f + 0.3f * sin(current_time * 2.0f);
            uint8_t title_alpha = (uint8_t)(255 * pulse);
            
            SituationCmdDrawText(cmd, font, "DIGITAL RAIN",
                (Vector2){init_info.window_width / 2.0f - 50, 20},
                (ColorRGBA){0, 255, 100, title_alpha});
            
            // Piano keyboard instructions
            SituationCmdDrawText(cmd, font, "Piano: ZSXDCVGBHNJM... (lower) | QWERTY... (upper)",
                (Vector2){init_info.window_width / 2.0f - 200, 40},
                (ColorRGBA){100, 200, 255, 180});

            // Audio status indicator
            if (audio_enabled) {
                SituationCmdDrawText(cmd, font, "[AUDIO ON]",
                    (Vector2){init_info.window_width / 2.0f - 40, 40},
                    (ColorRGBA){100, 255, 100, 180});
            } else {
                SituationCmdDrawText(cmd, font, "[AUDIO OFF]",
                    (Vector2){init_info.window_width / 2.0f - 45, 40},
                    (ColorRGBA){255, 100, 100, 180});
            }

            // Draw stats in corner
            char stats[128];
            snprintf(stats, sizeof(stats), "Frame: %d | Columns: %d | Active Tones: %d", 
                frame, MAX_COLUMNS, audio_debug.active_tone_count);
            SituationCmdDrawText(cmd, font, stats,
                (Vector2){10, init_info.window_height - 30},
                (ColorRGBA){0, 200, 100, 180});
            
            // Show last sample values for debugging
            char sample_debug[256];
            snprintf(sample_debug, sizeof(sample_debug), "Samples: %.3f %.3f %.3f %.3f",
                audio_debug.last_sample_values[0],
                audio_debug.last_sample_values[1],
                audio_debug.last_sample_values[2],
                audio_debug.last_sample_values[3]);
            SituationCmdDrawText(cmd, font, sample_debug,
                (Vector2){10, init_info.window_height - 50},
                (ColorRGBA){0, 200, 100, 180});

            // Draw oscillator visualization
            for (int i = 0; i < 16; i++) {
                bool state = SituationTimerGetOscillatorState(i);
                uint8_t brightness = state ? 255 : 80;
                
                SituationCmdDrawText(cmd, font, state ? "█" : "░",
                    (Vector2){10 + i * 10, init_info.window_height - 50},
                    (ColorRGBA){0, brightness, 50, 255});
            }

            // Draw ACTUAL audio waveform from the audio system (scrolling oscilloscope)
            int wave_y_base = 200;
            int wave_width = 640;  // Width of oscilloscope
            int wave_height = 120; // Height of oscilloscope (increased)
            int samples_to_draw = 80;  // Number of samples to display
            
            // Draw title
            SituationCmdDrawText(cmd, font, "=== AUDIO OSCILLOSCOPE ===",
                (Vector2){50, wave_y_base - 30},
                (ColorRGBA){100, 200, 255, 255});
            
            // Draw center line (zero crossing)
            for (int i = 0; i < samples_to_draw; i++) {
                SituationCmdDrawText(cmd, font, ".",
                    (Vector2){50 + i * 8, wave_y_base},
                    (ColorRGBA){50, 100, 150, 100});
            }
            
            // Read from circular buffer (most recent samples)
            int read_pos = waveform_write_pos;
            
            // Draw waveform as vertical bars
            for (int i = 0; i < samples_to_draw; i++) {
                // Read backwards from write position (newest samples on right)
                int buffer_index = (read_pos - samples_to_draw + i + WAVEFORM_SAMPLES) % WAVEFORM_SAMPLES;
                float sample = waveform_buffer[buffer_index];
                
                // Calculate Y position based on amplitude (5x multiplier for visibility)
                int y_offset = (int)(sample * 250.0f);  // 5x the previous 50.0f
                
                // Clamp to display range
                if (y_offset > 60) y_offset = 60;
                if (y_offset < -60) y_offset = -60;
                
                // Draw vertical line from center to sample value
                int steps = abs(y_offset) / 8;  // 8 pixels per character
                if (steps == 0 && fabsf(sample) > 0.01f) steps = 1;  // At least one char if non-zero
                
                for (int s = 0; s <= steps; s++) {
                    int y_pos;
                    if (y_offset > 0) {
                        y_pos = wave_y_base - (s * 8);
                    } else {
                        y_pos = wave_y_base + (s * 8);
                    }
                    
                    // Color based on amplitude
                    uint8_t brightness = (uint8_t)(200 + (fabsf(sample) * 55));
                    
                    SituationCmdDrawText(cmd, font, "|",
                        (Vector2){50 + i * 8, y_pos},
                        (ColorRGBA){0, brightness, 255, 255});
                }
            }
            
            // Calculate signal statistics
            bool has_signal = false;
            float max_amplitude = 0.0f;
            float min_amplitude = 0.0f;
            float avg_amplitude = 0.0f;
            int sample_count = 0;
            
            // Check last 80 samples for stats
            for (int i = 0; i < 80; i++) {
                int buffer_index = (read_pos - 80 + i + WAVEFORM_SAMPLES) % WAVEFORM_SAMPLES;
                float sample = waveform_buffer[buffer_index];
                float abs_sample = fabsf(sample);
                
                if (abs_sample > 0.001f) has_signal = true;
                if (sample > max_amplitude) max_amplitude = sample;
                if (sample < min_amplitude) min_amplitude = sample;
                avg_amplitude += abs_sample;
                sample_count++;
            }
            avg_amplitude /= sample_count;
            
            // Show signal status with sample values
            char signal_status[256];
            snprintf(signal_status, sizeof(signal_status), 
                "%s | Range: [%.3f, %.3f] | Avg: %.3f | Samples: %d",
                has_signal ? "SIGNAL" : "SILENCE", 
                min_amplitude, max_amplitude, avg_amplitude, sample_count);
            SituationCmdDrawText(cmd, font, signal_status,
                (Vector2){50, wave_y_base + 80},
                (ColorRGBA){has_signal ? 0 : 255, has_signal ? 255 : 100, has_signal ? 255 : 0, 255});

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }

        frame++;
    }

    printf("\nShutting down...\n");
    SituationShutdown();
    printf("Done!\n");

    return 0;
}
