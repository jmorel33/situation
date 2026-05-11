/***************************************************************************************************
*
*   examples/midi_14bit_example.c - 14-bit MIDI CC Example (console / no audio graph)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Demonstrates MSB/LSB CC pairs via SIT_MidiDevice callbacks and _SituationUpdate14BitCC helpers.
*   Uses PortMidi for hardware input; prints parameter updates to stdout.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "../situation.h"

#include "../sit/aud/midi_device_callbacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#endif

typedef struct {
    float* controls;
    SIT_MidiCC14Bit cutoff_14bit;
    SIT_MidiCC14Bit resonance_14bit;
} Filter14BitState;

typedef struct {
    SIT_MidiDevice* midi_device;
    PmStream* midi_in;
    Filter14BitState filter_state;
} AppState;

static AppState g_app = {0};

static void Filter14BitOnControlChange(void* device, uint8_t cc_number, uint8_t cc_value,
                                       uint32_t sample_offset) {
    (void)sample_offset;

    Filter14BitState* state = (Filter14BitState*)device;
    if (!state || !state->controls) {
        return;
    }

    if (cc_number == 1 || cc_number == 33) {
        if (_SituationUpdate14BitCC(&state->cutoff_14bit, cc_number, cc_value, 0u)) {
            uint16_t value_14bit = _SituationGet14BitValue(&state->cutoff_14bit);
            state->controls[1] = _SituationNormalize14BitCCLog(value_14bit, 20.0f, 20000.0f);

            printf("Cutoff 14-bit: %5u -> %7.2f Hz (MSB=%3u, LSB=%3u)\n",
                   (unsigned)value_14bit, state->controls[1],
                   (unsigned)state->cutoff_14bit.msb, (unsigned)state->cutoff_14bit.lsb);
        }
    } else if (cc_number == 2 || cc_number == 34) {
        if (_SituationUpdate14BitCC(&state->resonance_14bit, cc_number, cc_value, 0u)) {
            uint16_t value_14bit = _SituationGet14BitValue(&state->resonance_14bit);
            state->controls[2] = _SituationNormalize14BitCC(value_14bit, 0.1f, 10.0f);

            printf("Resonance 14-bit: %5u -> %5.3f (MSB=%3u, LSB=%3u)\n",
                   (unsigned)value_14bit, state->controls[2],
                   (unsigned)state->resonance_14bit.msb, (unsigned)state->resonance_14bit.lsb);
        }
    } else {
        switch (cc_number) {
            case 40:
                state->controls[0] = floorf(_SituationNormalizeMidiCC(cc_value, 0.0f, 6.99f));
                printf("Type: %d\n", (int)state->controls[0]);
                break;
            case 43:
                state->controls[3] = _SituationNormalizeMidiCCDb(cc_value, -24.0f, 24.0f);
                printf("Gain: %.2f dB\n", state->controls[3]);
                break;
            case 44:
                state->controls[4] = _SituationNormalizeMidiCC(cc_value, 1.0f, 10.0f);
                printf("Drive: %.2f\n", state->controls[4]);
                break;
            case 45:
                state->controls[5] = floorf(_SituationNormalizeMidiCC(cc_value, 0.0f, 2.99f));
                printf("Oversampling: %d\n", (int)state->controls[5]);
                break;
            default:
                break;
        }
    }
}

static bool setup_midi(void) {
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi (error code %ld).\n", (long)err);
        return false;
    }

    int device_count = Pm_CountDevices();
    int input_device = -1;

    printf("Available MIDI input devices:\n");
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info && info->input) {
            printf("  [%d] %s\n", i, info->name);
            if (input_device < 0) {
                input_device = i;
            }
        }
    }

    if (input_device < 0) {
        printf("No MIDI input devices found.\n");
        return false;
    }

    printf("\nUsing MIDI input: %s\n\n", Pm_GetDeviceInfo(input_device)->name);

    err = Pm_OpenInput(&g_app.midi_in, input_device, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("Failed to open MIDI input (error code %ld).\n", (long)err);
        return false;
    }

    return true;
}

static bool setup_midi_device(void) {
    g_app.filter_state.controls = (float*)calloc(6, sizeof(float));
    if (!g_app.filter_state.controls) {
        printf("Failed to allocate control array.\n");
        return false;
    }

    g_app.filter_state.controls[0] = 0.0f;
    g_app.filter_state.controls[1] = 1000.0f;
    g_app.filter_state.controls[2] = 1.0f;
    g_app.filter_state.controls[3] = 0.0f;
    g_app.filter_state.controls[4] = 1.0f;
    g_app.filter_state.controls[5] = 0.0f;

    memset(&g_app.filter_state.cutoff_14bit, 0, sizeof(SIT_MidiCC14Bit));
    memset(&g_app.filter_state.resonance_14bit, 0, sizeof(SIT_MidiCC14Bit));

    g_app.midi_device = SIT_MidiDevice_Create(
        "Filter 14-bit",
        SIT_MIDI_DEVICE_EFFECT,
        SIT_MIDI_CAP_INPUT,
        &g_app.filter_state
    );
    if (!g_app.midi_device) {
        printf("Failed to create MIDI device.\n");
        return false;
    }

    SIT_MidiCallbacks callbacks = {0};
    callbacks.on_control_change = Filter14BitOnControlChange;
    SIT_MidiDevice_SetCallbacks(g_app.midi_device, &callbacks);

    printf("14-bit MIDI filter ready.\n\n");
    return true;
}

static void process_midi(void) {
    if (!g_app.midi_in || !g_app.midi_device) {
        return;
    }

    PmEvent events[32];
    int count = Pm_Read(g_app.midi_in, events, 32);

    for (int i = 0; i < count; i++) {
        PmMessage msg = events[i].message;
        uint8_t status = Pm_MessageStatus(msg);
        uint8_t data1 = Pm_MessageData1(msg);
        uint8_t data2 = Pm_MessageData2(msg);

        if ((status & 0xF0) == 0xB0 && g_app.midi_device->callbacks.on_control_change) {
            g_app.midi_device->callbacks.on_control_change(
                g_app.midi_device->device_ptr,
                data1,
                data2,
                0u
            );
        }
    }
}

int main(void) {
    printf("=== 14-bit MIDI CC Example ===\n\n");

    if (!setup_midi()) {
        return 1;
    }

    if (!setup_midi_device()) {
        if (g_app.midi_in) {
            Pm_Close(g_app.midi_in);
        }
        Pm_Terminate();
        return 1;
    }

    printf("MIDI CC Mapping (14-bit):\n");
    printf("  CC 1 (MSB) + CC 33 (LSB) -> Cutoff (log Hz)\n");
    printf("  CC 2 (MSB) + CC 34 (LSB) -> Resonance\n");
    printf("\nMIDI CC Mapping (7-bit):\n");
    printf("  CC 40 -> Type, CC 43 -> Gain, CC 44 -> Drive, CC 45 -> Oversampling\n\n");

#ifdef _WIN32
    printf("Listening... press any key to quit.\n\n");
    while (!_kbhit()) {
        process_midi();
        Sleep(10);
    }
    (void)_getch();
#else
    printf("Listening... Ctrl+C to quit.\n\n");
    for (;;) {
        process_midi();
        usleep(10000);
    }
#endif

    if (g_app.midi_device) {
        SIT_MidiDevice_Destroy(g_app.midi_device);
    }
    if (g_app.midi_in) {
        Pm_Close(g_app.midi_in);
    }
    Pm_Terminate();

    if (g_app.filter_state.controls) {
        free(g_app.filter_state.controls);
    }

    printf("Cleanup complete.\n");
    return 0;
}
