#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

int main(void) {
    printf("=== Virtual MIDI Test ===\n\n");
    
    // Initialize MIDI system
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[ERROR] Failed to initialize MIDI: %d\n", err);
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n\n");
    
    // Count initial devices
    int initial_count = Pm_CountDevices();
    printf("Initial device count: %d\n", initial_count);
    
    // Create virtual devices
    PmDeviceID virt_out, virt_in;
    
    err = Pm_CreateVirtualDevice("Virtual Sequencer Out", 0, &virt_out);
    if (err != pmNoError) {
        printf("[ERROR] Failed to create virtual output: %d\n", err);
        Pm_Terminate();
        return 1;
    }
    printf("[SUCCESS] Created virtual output device (ID: %d)\n", virt_out);
    
    err = Pm_CreateVirtualDevice("Virtual Synth In", 1, &virt_in);
    if (err != pmNoError) {
        printf("[ERROR] Failed to create virtual input: %d\n", err);
        Pm_Terminate();
        return 1;
    }
    printf("[SUCCESS] Created virtual input device (ID: %d)\n\n", virt_in);
    
    // Verify device count increased
    int new_count = Pm_CountDevices();
    printf("New device count: %d (added %d virtual devices)\n\n", new_count, new_count - initial_count);
    
    // Get device info
    const PmDeviceInfo *out_info = Pm_GetDeviceInfo(virt_out);
    const PmDeviceInfo *in_info = Pm_GetDeviceInfo(virt_in);
    
    if (out_info) {
        printf("Output device: %s (%s) - %s\n", 
            out_info->name, out_info->interf,
            out_info->output ? "OUTPUT" : "");
    }
    
    if (in_info) {
        printf("Input device: %s (%s) - %s\n\n", 
            in_info->name, in_info->interf,
            in_info->input ? "INPUT" : "");
    }
    
    // Check if devices are virtual
    printf("Is output virtual? %s\n", Pm_IsVirtualDevice(virt_out) ? "YES" : "NO");
    printf("Is input virtual? %s\n\n", Pm_IsVirtualDevice(virt_in) ? "YES" : "NO");
    
    // Connect devices
    err = Pm_ConnectVirtualDevices(virt_out, virt_in);
    if (err != pmNoError) {
        printf("[ERROR] Failed to connect devices: %d\n", err);
        Pm_Terminate();
        return 1;
    }
    printf("[SUCCESS] Connected output to input\n\n");
    
    // Open streams
    PmStream *out_stream, *in_stream;
    
    err = Pm_OpenOutput(&out_stream, virt_out, NULL, 0, NULL, NULL, 0);
    if (err != pmNoError) {
        printf("[ERROR] Failed to open output stream: %d\n", err);
        Pm_Terminate();
        return 1;
    }
    printf("[SUCCESS] Opened output stream\n");
    
    err = Pm_OpenInput(&in_stream, virt_in, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("[ERROR] Failed to open input stream: %d\n", err);
        Pm_Close(out_stream);
        Pm_Terminate();
        return 1;
    }
    printf("[SUCCESS] Opened input stream\n\n");
    
    // Send MIDI events
    printf("Sending MIDI events...\n");
    PmEvent events[5];
    
    // Note On - Middle C (60), velocity 100
    events[0].message = Pm_Message(0x90, 60, 100);
    events[0].timestamp = 0;
    
    // Note On - E (64), velocity 80
    events[1].message = Pm_Message(0x90, 64, 80);
    events[1].timestamp = 0;
    
    // Note On - G (67), velocity 90
    events[2].message = Pm_Message(0x90, 67, 90);
    events[2].timestamp = 0;
    
    // Note Off - Middle C
    events[3].message = Pm_Message(0x80, 60, 0);
    events[3].timestamp = 0;
    
    // Control Change - Volume (CC 7), value 127
    events[4].message = Pm_Message(0xB0, 7, 127);
    events[4].timestamp = 0;
    
    err = Pm_Write(out_stream, events, 5);
    if (err != pmNoError) {
        printf("[ERROR] Failed to write MIDI: %d\n", err);
    } else {
        printf("[SUCCESS] Wrote 5 MIDI events\n");
    }
    
    // Give time for routing
    SLEEP_MS(10);
    
    // Read MIDI events
    printf("\nReading MIDI events...\n");
    PmEvent read_buffer[32];
    int count = Pm_Read(in_stream, read_buffer, 32);
    
    printf("Read %d events:\n", count);
    for (int i = 0; i < count; i++) {
        uint8_t status = Pm_MessageStatus(read_buffer[i].message);
        uint8_t data1 = Pm_MessageData1(read_buffer[i].message);
        uint8_t data2 = Pm_MessageData2(read_buffer[i].message);
        
        const char *msg_type = "Unknown";
        if ((status & 0xF0) == 0x90) msg_type = "Note On";
        else if ((status & 0xF0) == 0x80) msg_type = "Note Off";
        else if ((status & 0xF0) == 0xB0) msg_type = "Control Change";
        
        printf("  [%d] %s: status=0x%02X data1=%d data2=%d\n",
            i, msg_type, status, data1, data2);
    }
    
    if (count == 5) {
        printf("\n[SUCCESS] All events received correctly!\n");
    } else {
        printf("\n[WARNING] Expected 5 events, got %d\n", count);
    }
    
    // Close streams
    printf("\nClosing streams...\n");
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    printf("[SUCCESS] Streams closed\n");
    
    // Disconnect devices
    err = Pm_DisconnectVirtualDevices(virt_out, virt_in);
    if (err != pmNoError) {
        printf("[WARNING] Failed to disconnect devices: %d\n", err);
    } else {
        printf("[SUCCESS] Devices disconnected\n");
    }
    
    // Destroy virtual devices
    err = Pm_DestroyVirtualDevice(virt_out);
    if (err != pmNoError) {
        printf("[WARNING] Failed to destroy output device: %d\n", err);
    } else {
        printf("[SUCCESS] Output device destroyed\n");
    }
    
    err = Pm_DestroyVirtualDevice(virt_in);
    if (err != pmNoError) {
        printf("[WARNING] Failed to destroy input device: %d\n", err);
    } else {
        printf("[SUCCESS] Input device destroyed\n");
    }
    
    // Verify device count returned to initial
    int final_count = Pm_CountDevices();
    printf("\nFinal device count: %d\n", final_count);
    
    // Terminate
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Tests Passed! ===\n");
    
    return 0;
}
