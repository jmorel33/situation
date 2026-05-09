/*
 * midi.h - Hybrid MIDI Library (Hardware + Virtual Routing)
 *
 * This is a complete MIDI library providing both:
 *   - Hardware MIDI I/O via OS APIs (WinMM on Windows, future: ALSA/CoreMIDI)
 *   - Virtual MIDI routing for internal cross-platform event routing
 *
 * Features:
 *   - PortMidi-compatible API
 *   - Hardware device enumeration and hotplug detection
 *   - Virtual devices for internal routing (cross-platform)
 *   - Lock-free ring buffers for real-time safety
 *   - MIDI routing matrix (connect any device to any device)
 *   - SysEx support with non-blocking output
 *   - Thread-safe operation
 *
 * Usage:
 *   1. Include this header in your project.
 *   2. Define MIDI_IMPLEMENTATION in exactly one C file before including.
 *   3. On Windows: Link with -lwinmm for hardware MIDI support.
 *   4. Virtual MIDI works on all platforms without dependencies.
 */

#ifndef MIDI_H
#define MIDI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <stdint.h> 

/* Types */
typedef struct PmStream PmStream;
typedef int32_t PmError;
typedef int32_t PmDeviceID;
typedef int32_t PmTimestamp; 
typedef int32_t PmMessage;

typedef struct {
    int structVersion;
    const char *interf;
    const char *name;
    int input;
    int output;
    int opened;
} PmDeviceInfo;

typedef struct {
    PmMessage message;
    PmTimestamp timestamp;
} PmEvent;

// NEW: Callback type for device list changes
typedef void (*PmDeviceChangeCallback)(void *user_data);

/* Error codes */
#define pmNoError 0
#define pmNoDevice -1 
#define pmHostError -10000
#define pmInvalidDeviceId -10001
#define pmInsufficientMemory -10002
#define pmBufferTooSmall -10003
#define pmBufferOverflow -10004 
#define pmBadPtr -10005
#define pmBadData -10006
#define pmInternalError -10007
#define pmBufferMaxSize -10008
#define pmSysexOverflow -10009 
#define pmSysexBusy -10010 // NEW: For non-blocking sysex if already pending

/* Special device ID */
#define PM_NO_DEVICE -1 

/* Device types */
#define PM_DEVICE_TYPE_HARDWARE  0
#define PM_DEVICE_TYPE_VIRTUAL   1

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define PM_PLATFORM_WINDOWS 1
    #define PM_HAS_HARDWARE_MIDI 1
#elif defined(__linux__)
    #define PM_PLATFORM_LINUX 1
    #define PM_HAS_HARDWARE_MIDI 0  // TODO: ALSA support
#elif defined(__APPLE__)
    #define PM_PLATFORM_MACOS 1
    #define PM_HAS_HARDWARE_MIDI 0  // TODO: CoreMIDI support
#else
    #define PM_PLATFORM_UNKNOWN 1
    #define PM_HAS_HARDWARE_MIDI 0
#endif

#define PM_HAS_VIRTUAL_MIDI 1  // Always available

// ================================================================================================
// CONSTANTS
// ================================================================================================

#define MAX_DEVICES 32
#define MAX_VIRTUAL_DEVICES 64
#define MAX_CONNECTIONS 128
#define MAX_NAME_LEN 128 
#define EVENT_BUFFER_SIZE 1024
#define VIRTUAL_BUFFER_SIZE 8192  // Power of 2 for fast modulo
#define SYSEX_BUFFER_SIZE 4096

// ================================================================================================
// VIRTUAL MIDI STRUCTURES
// ================================================================================================

/* Lock-free ring buffer for virtual MIDI (SPSC - Single Producer Single Consumer) */
typedef struct {
    PmEvent events[VIRTUAL_BUFFER_SIZE];
    _Atomic uint32_t write_pos;
    _Atomic uint32_t read_pos;
    char padding[64 - 2*sizeof(_Atomic uint32_t)]; // Cache line alignment
} PmVirtualBuffer;

/* Virtual device structure */
typedef struct {
    PmDeviceInfo info;
    PmDeviceID id;
    PmVirtualBuffer *buffer;
    int active;
    int device_type;  // PM_DEVICE_TYPE_VIRTUAL
} PmVirtualDevice;

/* Connection between devices */
typedef struct {
    PmDeviceID source;
    PmDeviceID destination;
    int active;
    
    // Advanced features (Phase 3)
    void *filter;      // PmFilter* (optional)
    void *transform;   // PmTransform* (optional)
} PmConnection;

/* MIDI Filter - selectively pass/block MIDI messages */
typedef struct {
    uint8_t filter_note_on;         // 0=pass, 1=block
    uint8_t filter_note_off;        // 0=pass, 1=block
    uint8_t filter_cc;              // 0=pass, 1=block
    uint8_t filter_program_change;  // 0=pass, 1=block
    uint8_t filter_pitch_bend;      // 0=pass, 1=block
    uint8_t filter_aftertouch;      // 0=pass, 1=block
    uint8_t filter_sysex;           // 0=pass, 1=block
    uint16_t channel_mask;          // Bit mask for channels 0-15 (0=block, 1=pass)
} PmFilter;

/* MIDI Transform - modify MIDI messages in transit */
typedef struct {
    int8_t transpose;               // Semitones (-127 to +127)
    uint8_t velocity_curve;         // 0=linear, 1=exponential, 2=logarithmic
    float velocity_scale;           // Multiply velocity (0.0 to 2.0)
    uint8_t channel_remap[16];      // Map input channel to output channel (0xFF = no remap)
} PmTransform;

/* MIDI Recording - capture and playback MIDI events */
typedef struct {
    PmEvent *events;
    int32_t event_count;
    int32_t capacity;
    PmTimestamp start_time;
    int is_recording;
} PmRecording;

/* MIDI Router */
typedef struct {
    PmConnection connections[MAX_CONNECTIONS];
    int connection_count;
} PmRouter;

/* Functions - Hardware & Virtual */
PmError Pm_Initialize(void);
PmError Pm_Terminate(void);
int Pm_CountDevices(void);
int Pm_HasDeviceListChanged(void); 
PmError Pm_SetDeviceChangeCallback(PmDeviceChangeCallback callback, void *user_data); // NEW
const PmDeviceInfo *Pm_GetDeviceInfo(PmDeviceID id);
PmDeviceID Pm_GetDefaultInputDeviceID(void);
PmDeviceID Pm_GetDefaultOutputDeviceID(void);
PmError Pm_OpenInput(PmStream **stream, PmDeviceID inputDevice, void *inputDriverInfo, int32_t bufferSize, PmTimestamp (*time_proc)(void *), void *time_info);
PmError Pm_OpenOutput(PmStream **stream, PmDeviceID outputDevice, void *outputDriverInfo, int32_t bufferSize, PmTimestamp (*time_proc)(void *), void *time_info, int32_t latency);
PmError Pm_Close(PmStream *stream);
int32_t Pm_Read(PmStream *stream, PmEvent *buffer, int32_t length);
PmError Pm_ReadSysex(PmStream *stream, uint8_t *msg_buffer, int32_t *length); 
PmError Pm_Write(PmStream *stream, PmEvent *buffer, int32_t length);
PmError Pm_WriteSysEx(PmStream *stream, PmTimestamp when, const uint8_t *msg, int32_t length);

/* Virtual MIDI Functions */
PmError Pm_CreateVirtualDevice(const char *name, int is_input, PmDeviceID *out_device_id);
PmError Pm_DestroyVirtualDevice(PmDeviceID device_id);
PmError Pm_ConnectVirtualDevices(PmDeviceID source_output, PmDeviceID dest_input);
PmError Pm_DisconnectVirtualDevices(PmDeviceID source_output, PmDeviceID dest_input);
int Pm_IsVirtualDevice(PmDeviceID device_id);

/* Advanced Features - Phase 3 */
PmError Pm_SetConnectionFilter(PmDeviceID source, PmDeviceID dest, const PmFilter *filter);
PmError Pm_SetConnectionTransform(PmDeviceID source, PmDeviceID dest, const PmTransform *transform);
PmError Pm_CreateRecording(PmRecording *recording, int32_t capacity);
PmError Pm_FreeRecording(PmRecording *recording);
PmError Pm_StartRecording(PmStream *stream, PmRecording *recording);
PmError Pm_StopRecording(PmStream *stream);
PmError Pm_PlayRecording(PmStream *stream, const PmRecording *recording);

/* Utility Functions */
#define Pm_Message(status, data1, data2) \
    ((((data2) << 16) & 0xFF0000) | (((data1) << 8) & 0xFF00) | ((status) & 0xFF))
#define Pm_MessageStatus(msg) ((msg) & 0xFF)
#define Pm_MessageData1(msg) (((msg) >> 8) & 0xFF)
#define Pm_MessageData2(msg) (((msg) >> 16) & 0xFF)

#ifdef MIDI_IMPLEMENTATION

#if PM_HAS_HARDWARE_MIDI
#include <mmsystem.h>
#endif

#include <stdlib.h>
#include <string.h> 
#include <stdio.h>
#include <math.h>  // For sqrtf in velocity curve transformation
#include <stdatomic.h>

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
#pragma comment(lib, "winmm.lib") 
#endif

// ================================================================================================
// HARDWARE MIDI STRUCTURES (Windows WinMM)
// ================================================================================================

typedef struct MidiDevice MidiDevice;

typedef struct {
    PmEvent events[EVENT_BUFFER_SIZE];
    volatile int head, tail; 
    uint8_t sysex_buffer[SYSEX_BUFFER_SIZE];
    volatile int sysex_len;
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    MIDIHDR sysex_header_input;
#endif
    mtx_t lock;  // C11 mutex (portable via tinycthread)
} MidiInputBuffer;

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
// Forward declaration for midi_out_proc
static void CALLBACK midi_out_proc(HMIDIOUT hMidiOut, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
#endif

struct PmStream {
    MidiDevice *device;           // Hardware device (if hardware)
    MidiInputBuffer *input_buffer; // Hardware input buffer
    
    // Virtual device support
    PmVirtualDevice *virtual_device;
    PmVirtualBuffer *virtual_buffer;
    int is_virtual;
    
    // Routing connections
    struct PmStream *connected_streams[16]; // Max 16 connections per stream
    int connection_count;
    
    // Recording support (Phase 3)
    PmRecording *recording;
    
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    MIDIHDR sysex_hdr_output; 
    uint8_t sysex_buffer_output[SYSEX_BUFFER_SIZE]; 
    volatile int sysex_pending;
#endif

    PmTimestamp (*time_proc)(void *time_info);
    void *time_info;
    int32_t latency;
};

struct MidiDevice {
    PmDeviceInfo info;
    PmDeviceID id;
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    HMIDIIN in_handle;
    HMIDIOUT out_handle;
    UINT winmm_id;
#endif
    int is_input_device;
    int device_type;  // PM_DEVICE_TYPE_HARDWARE or PM_DEVICE_TYPE_VIRTUAL
};

typedef struct {
    // Hardware devices
    MidiDevice devices[MAX_DEVICES];
    int device_count;
    
    // Virtual devices
    PmVirtualDevice virtual_devices[MAX_VIRTUAL_DEVICES];
    int virtual_device_count;
    
    // Router
    PmRouter router;
    
    // Context state
    int initialized;
    mtx_t context_lock;  // C11 mutex (portable via tinycthread)
    volatile int device_list_changed_flag; 
    PmDeviceChangeCallback device_change_callback;
    void *device_change_user_data;
} MidiContext;

static MidiContext midi_context = {0}; 

// ================================================================================================
// VIRTUAL MIDI IMPLEMENTATION
// ================================================================================================

/* Forward declarations for Phase 3 helper functions */
static int Pm_ApplyFilter(const PmFilter *filter, PmMessage msg);
static PmMessage Pm_ApplyTransform(const PmTransform *transform, PmMessage msg);

/* Initialize virtual buffer */
static void Pm_InitVirtualBuffer(PmVirtualBuffer *vbuf) {
    if (!vbuf) return;
    atomic_store(&vbuf->write_pos, 0);
    atomic_store(&vbuf->read_pos, 0);
    memset(vbuf->events, 0, sizeof(vbuf->events));
}

/* Write to virtual buffer (lock-free producer) */
static PmError Pm_WriteVirtual(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !stream->virtual_buffer || !buffer) return pmBadPtr;
    
    PmVirtualBuffer *vbuf = stream->virtual_buffer;
    
    for (int i = 0; i < length; i++) {
        uint32_t write_pos = atomic_load(&vbuf->write_pos);
        uint32_t next_write = (write_pos + 1) & (VIRTUAL_BUFFER_SIZE - 1); // Fast modulo
        uint32_t read_pos = atomic_load(&vbuf->read_pos);
        
        // Check if buffer full (leave one slot empty for full/empty distinction)
        if (next_write == read_pos) {
            return pmBufferOverflow;
        }
        
        // Write event
        vbuf->events[write_pos] = buffer[i];
        
        // Update write position (atomic)
        atomic_store(&vbuf->write_pos, next_write);
        
        // Route to connected streams
        for (int j = 0; j < stream->connection_count; j++) {
            if (stream->connected_streams[j]) {
                Pm_Write(stream->connected_streams[j], &buffer[i], 1);
            }
        }
    }
    
    return pmNoError;
}

/* Read from virtual buffer (lock-free consumer) */
static int32_t Pm_ReadVirtual(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !stream->virtual_buffer || !buffer) return 0;
    
    PmVirtualBuffer *vbuf = stream->virtual_buffer;
    int count = 0;
    
    while (count < length) {
        uint32_t read_pos = atomic_load(&vbuf->read_pos);
        uint32_t write_pos = atomic_load(&vbuf->write_pos);
        
        if (read_pos == write_pos) break; // Buffer empty
        
        // Read event
        buffer[count] = vbuf->events[read_pos];
        count++;
        
        // Update read position
        uint32_t next_read = (read_pos + 1) & (VIRTUAL_BUFFER_SIZE - 1);
        atomic_store(&vbuf->read_pos, next_read);
    }
    
    return count;
}

/* Initialize virtual MIDI system */
static void Pm_InitializeVirtualMidi(void) {
    midi_context.virtual_device_count = 0;
    midi_context.router.connection_count = 0;
    
    for (int i = 0; i < MAX_VIRTUAL_DEVICES; i++) {
        midi_context.virtual_devices[i].active = 0;
        midi_context.virtual_devices[i].buffer = NULL;
    }
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        midi_context.router.connections[i].active = 0;
    }
}

// ================================================================================================
// HARDWARE MIDI CALLBACKS (Windows WinMM)
// ================================================================================================

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS

static void CALLBACK midi_in_proc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    MidiInputBuffer *buffer = (MidiInputBuffer *)dwInstance;
    (void)hMidiIn; 

    if (!buffer) return;

    if (wMsg == MIM_DATA) { 
        mtx_lock(&buffer->lock);
        int next_head = (buffer->head + 1) % EVENT_BUFFER_SIZE;
        if (next_head != buffer->tail) { 
            buffer->events[buffer->head].message = (PmMessage)dwParam1;
            buffer->events[buffer->head].timestamp = (PmTimestamp)dwParam2; 
            buffer->head = next_head;
        }
        mtx_unlock(&buffer->lock);
    } else if (wMsg == MIM_LONGDATA) { 
        MIDIHDR *hdr = (MIDIHDR *)dwParam1;
        mtx_lock(&buffer->lock);
        if (buffer->sysex_len + hdr->dwBytesRecorded <= SYSEX_BUFFER_SIZE) {
            memcpy(buffer->sysex_buffer + buffer->sysex_len, hdr->lpData, hdr->dwBytesRecorded);
            buffer->sysex_len += hdr->dwBytesRecorded;
        }
        
        if (hdr->dwBytesRecorded > 0) { 
            int eox_found = 0;
            if (buffer->sysex_len > 0 && buffer->sysex_buffer[buffer->sysex_len -1] == 0xF7) {
                eox_found = 1;
            }

            if (eox_found || (hdr->dwFlags & MHDR_DONE)) { 
                int next_head = (buffer->head + 1) % EVENT_BUFFER_SIZE;
                if (next_head != buffer->tail) {
                    buffer->events[buffer->head].message = Pm_Message(0xF0, 0, 0); 
                    buffer->events[buffer->head].timestamp = (PmTimestamp)dwParam2; 
                    buffer->head = next_head;
                } else {
                    buffer->sysex_len = 0; 
                }
            }
        }
        midiInAddBuffer(hMidiIn, &buffer->sysex_header_input, sizeof(MIDIHDR));
        mtx_unlock(&buffer->lock);
    }
}

// NEW: Callback for non-blocking sysex output (MOM_DONE)
static void CALLBACK midi_out_proc(HMIDIOUT hMidiOut, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)dwParam1; (void)dwParam2; // Unused
    if (wMsg == MOM_DONE) {
        PmStream *stream = (PmStream *)dwInstance;
        if (stream && stream->device && stream->device->out_handle == hMidiOut) {
            // Check if MHDR_PREPARED is still set before unpreparing
            // (it should be, as MOM_DONE implies it was successfully sent)
            if (stream->sysex_hdr_output.dwFlags & MHDR_PREPARED) {
                 midiOutUnprepareHeader(hMidiOut, &stream->sysex_hdr_output, sizeof(MIDIHDR));
            }
            stream->sysex_pending = 0;
        }
    }
}

#endif // PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS

// ================================================================================================
// CLEANUP FUNCTIONS
// ================================================================================================

static void midi_cleanup_internal(void) {
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    for (int i = 0; i < midi_context.device_count; i++) {
        MidiDevice *dev = &midi_context.devices[i];
        if (dev->info.opened) { 
            // Streams should ideally be closed by Pm_Close by the user.
            // This is a fallback during Pm_Terminate or destructive re-init.
            if (dev->is_input_device && dev->in_handle) {
                // Pm_Close for the specific stream handles unpreparing input headers
                midiInStop(dev->in_handle);
                midiInReset(dev->in_handle); 
                midiInClose(dev->in_handle);
                dev->in_handle = NULL;
            } else if (!dev->is_input_device && dev->out_handle) {
                // Pm_Close for the specific stream handles sysex_pending logic
                midiOutReset(dev->out_handle); 
                midiOutClose(dev->out_handle);
                dev->out_handle = NULL;
            }
            dev->info.opened = 0;
        }
        if (dev->info.name) {
            free((void *)dev->info.name);
            dev->info.name = NULL;
        }
    }
    midi_context.device_count = 0;
#endif // PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    
    // Clean up virtual devices
    for (int i = 0; i < midi_context.virtual_device_count; i++) {
        PmVirtualDevice *vdev = &midi_context.virtual_devices[i];
        if (vdev->active && vdev->buffer) {
            free(vdev->buffer);
            vdev->buffer = NULL;
        }
        if (vdev->info.name) {
            free((void *)vdev->info.name);
            vdev->info.name = NULL;
        }
        vdev->active = 0;
    }
    midi_context.virtual_device_count = 0;
    
    // Clear router connections
    midi_context.router.connection_count = 0;
}


PmError Pm_Initialize(void) {
    if (midi_context.initialized) return pmNoError;

    mtx_init(&midi_context.context_lock, mtx_plain);
    mtx_lock(&midi_context.context_lock);

    midi_cleanup_internal(); 

    midi_context.device_count = 0;
    midi_context.device_list_changed_flag = 0; 
    midi_context.device_change_callback = NULL;
    midi_context.device_change_user_data = NULL;

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    // Enumerate hardware MIDI devices
    UINT num_in_devs = midiInGetNumDevs();
    UINT num_out_devs = midiOutGetNumDevs();

    for (UINT i = 0; i < num_in_devs; i++) {
        if (midi_context.device_count >= MAX_DEVICES) break;
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            MidiDevice *dev = &midi_context.devices[midi_context.device_count];
            dev->id = midi_context.device_count;
            dev->winmm_id = i;
            dev->is_input_device = 1;
            dev->device_type = PM_DEVICE_TYPE_HARDWARE;
            dev->info.structVersion = 1; 
            dev->info.interf = "WinMM";
            
            char *name_alloc = (char*)malloc(MAX_NAME_LEN);
            if (!name_alloc) { continue; } 
            strncpy(name_alloc, caps.szPname, MAX_NAME_LEN -1);
            name_alloc[MAX_NAME_LEN -1] = '\0';
            dev->info.name = name_alloc;

            dev->info.input = 1;
            dev->info.output = 0;
            dev->info.opened = 0;
            dev->in_handle = NULL;
            dev->out_handle = NULL;
            midi_context.device_count++;
        }
    }

    for (UINT i = 0; i < num_out_devs; i++) {
        if (midi_context.device_count >= MAX_DEVICES) break;
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            MidiDevice *dev = &midi_context.devices[midi_context.device_count];
            dev->id = midi_context.device_count;
            dev->winmm_id = i;
            dev->is_input_device = 0;
            dev->device_type = PM_DEVICE_TYPE_HARDWARE;
            dev->info.structVersion = 1;
            dev->info.interf = "WinMM";

            char *name_alloc = (char*)malloc(MAX_NAME_LEN);
            if (!name_alloc) { continue; }
            strncpy(name_alloc, caps.szPname, MAX_NAME_LEN -1);
            name_alloc[MAX_NAME_LEN-1] = '\0';
            dev->info.name = name_alloc;

            dev->info.input = 0;
            dev->info.output = 1;
            dev->info.opened = 0;
            dev->in_handle = NULL;
            dev->out_handle = NULL;
            midi_context.device_count++;
        }
    }
#else
    // No hardware MIDI on this platform
    printf("[MIDI] Hardware MIDI not available on this platform\n");
#endif
    
    // Initialize virtual MIDI system (always available)
    Pm_InitializeVirtualMidi();
    
    midi_context.initialized = 1;
    
    mtx_unlock(&midi_context.context_lock);
    
    return pmNoError;
}

PmError Pm_Terminate(void) {
    if (!midi_context.initialized) return pmNoError;
    
    mtx_lock(&midi_context.context_lock);
    
    midi_cleanup_internal(); 
    midi_context.initialized = 0;
    midi_context.device_list_changed_flag = 0;
    midi_context.device_change_callback = NULL;
    midi_context.device_change_user_data = NULL;
    
    mtx_unlock(&midi_context.context_lock);
    mtx_destroy(&midi_context.context_lock);
    
    return pmNoError;
}

// ================================================================================================
// VIRTUAL DEVICE API
// ================================================================================================

PmError Pm_CreateVirtualDevice(const char *name, int is_input, PmDeviceID *out_device_id) {
    if (!midi_context.initialized) {
        PmError err = Pm_Initialize();
        if (err != pmNoError) return err;
    }
    
    if (!name || !out_device_id) return pmBadPtr;
    if (midi_context.virtual_device_count >= MAX_VIRTUAL_DEVICES) {
        return pmInsufficientMemory;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_VIRTUAL_DEVICES; i++) {
        if (!midi_context.virtual_devices[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return pmInsufficientMemory;
    
    PmVirtualDevice *vdev = &midi_context.virtual_devices[slot];
    
    // Allocate and initialize buffer
    vdev->buffer = (PmVirtualBuffer *)malloc(sizeof(PmVirtualBuffer));
    if (!vdev->buffer) return pmInsufficientMemory;
    
    Pm_InitVirtualBuffer(vdev->buffer);
    
    // Set device info
    vdev->id = MAX_DEVICES + slot; // Virtual device IDs start after hardware devices
    vdev->device_type = PM_DEVICE_TYPE_VIRTUAL;
    vdev->active = 1;
    
    vdev->info.structVersion = 1;
    vdev->info.interf = "Virtual";
    
    // Copy name
    char *name_alloc = (char *)malloc(MAX_NAME_LEN);
    if (!name_alloc) {
        free(vdev->buffer);
        vdev->buffer = NULL;
        return pmInsufficientMemory;
    }
    strncpy(name_alloc, name, MAX_NAME_LEN - 1);
    name_alloc[MAX_NAME_LEN - 1] = '\0';
    vdev->info.name = name_alloc;
    
    vdev->info.input = is_input;
    vdev->info.output = !is_input;
    vdev->info.opened = 0;
    
    midi_context.virtual_device_count++;
    *out_device_id = vdev->id;
    
    return pmNoError;
}

PmError Pm_DestroyVirtualDevice(PmDeviceID device_id) {
    if (!midi_context.initialized) return pmHostError;
    
    // Check if it's a virtual device
    if (device_id < MAX_DEVICES) return pmInvalidDeviceId; // Not a virtual device
    
    int slot = device_id - MAX_DEVICES;
    if (slot < 0 || slot >= MAX_VIRTUAL_DEVICES) return pmInvalidDeviceId;
    
    PmVirtualDevice *vdev = &midi_context.virtual_devices[slot];
    if (!vdev->active) return pmInvalidDeviceId;
    
    // Check if device is opened
    if (vdev->info.opened) return pmHostError; // Device still in use
    
    // Free resources
    if (vdev->buffer) {
        free(vdev->buffer);
        vdev->buffer = NULL;
    }
    
    if (vdev->info.name) {
        free((void *)vdev->info.name);
        vdev->info.name = NULL;
    }
    
    vdev->active = 0;
    midi_context.virtual_device_count--;
    
    // Remove any connections involving this device
    for (int i = 0; i < midi_context.router.connection_count; i++) {
        PmConnection *conn = &midi_context.router.connections[i];
        if (conn->active && (conn->source == device_id || conn->destination == device_id)) {
            conn->active = 0;
        }
    }
    
    return pmNoError;
}

int Pm_IsVirtualDevice(PmDeviceID device_id) {
    if (!midi_context.initialized) return 0;
    
    if (device_id < MAX_DEVICES) return 0; // Hardware device
    
    int slot = device_id - MAX_DEVICES;
    if (slot < 0 || slot >= MAX_VIRTUAL_DEVICES) return 0;
    
    return midi_context.virtual_devices[slot].active ? 1 : 0;
}

PmError Pm_ConnectVirtualDevices(PmDeviceID source_output, PmDeviceID dest_input) {
    if (!midi_context.initialized) return pmHostError;
    
    // Validate source is an output device
    const PmDeviceInfo *source_info = Pm_GetDeviceInfo(source_output);
    if (!source_info || !source_info->output) return pmInvalidDeviceId;
    
    // Validate destination is an input device
    const PmDeviceInfo *dest_info = Pm_GetDeviceInfo(dest_input);
    if (!dest_info || !dest_info->input) return pmInvalidDeviceId;
    
    // Check if connection already exists
    for (int i = 0; i < midi_context.router.connection_count; i++) {
        PmConnection *conn = &midi_context.router.connections[i];
        if (conn->active && conn->source == source_output && conn->destination == dest_input) {
            return pmNoError; // Already connected
        }
    }
    
    // Find free connection slot
    int slot = -1;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!midi_context.router.connections[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return pmBufferMaxSize; // No free connection slots
    
    // Create connection
    PmConnection *conn = &midi_context.router.connections[slot];
    conn->source = source_output;
    conn->destination = dest_input;
    conn->active = 1;
    
    if (slot >= midi_context.router.connection_count) {
        midi_context.router.connection_count = slot + 1;
    }
    
    return pmNoError;
}

PmError Pm_DisconnectVirtualDevices(PmDeviceID source_output, PmDeviceID dest_input) {
    if (!midi_context.initialized) return pmHostError;
    
    // Find and remove connection
    for (int i = 0; i < midi_context.router.connection_count; i++) {
        PmConnection *conn = &midi_context.router.connections[i];
        if (conn->active && conn->source == source_output && conn->destination == dest_input) {
            // Free filter and transform if allocated
            if (conn->filter) {
                free(conn->filter);
                conn->filter = NULL;
            }
            if (conn->transform) {
                free(conn->transform);
                conn->transform = NULL;
            }
            conn->active = 0;
            return pmNoError;
        }
    }
    
    return pmInvalidDeviceId; // Connection not found
}

// ================================================================================================
// DEVICE ENUMERATION
// ================================================================================================

int Pm_CountDevices(void) {
    if (!midi_context.initialized) Pm_Initialize();

    mtx_lock(&midi_context.context_lock);
    
    UINT current_in_count = midiInGetNumDevs();
    UINT current_out_count = midiOutGetNumDevs();
    
    UINT listed_in_count = 0;
    UINT listed_out_count = 0;
    for (int i = 0; i < midi_context.device_count; i++) {
        if (midi_context.devices[i].info.input) listed_in_count++;
        if (midi_context.devices[i].info.output) listed_out_count++;
    }

    int re_initialized_by_us = 0;
    if (current_in_count != listed_in_count || current_out_count != listed_out_count) {
        midi_cleanup_internal(); 
        midi_context.initialized = 0;
        re_initialized_by_us = 1; 
        
        // Store callback info before Pm_Initialize overwrites them
        PmDeviceChangeCallback temp_callback = midi_context.device_change_callback;
        void* temp_user_data = midi_context.device_change_user_data;

        mtx_unlock(&midi_context.context_lock);
        Pm_Initialize(); 
        mtx_lock(&midi_context.context_lock); 
        
        // Restore callback info
        midi_context.device_change_callback = temp_callback;
        midi_context.device_change_user_data = temp_user_data;

        if(re_initialized_by_us) { 
            midi_context.device_list_changed_flag = 1;
            if (midi_context.device_change_callback) {
                // Call the callback outside the lock to prevent deadlocks if callback tries to use API
                mtx_unlock(&midi_context.context_lock);
                midi_context.device_change_callback(midi_context.device_change_user_data);
                mtx_lock(&midi_context.context_lock);
            }
        }
    }

    // Count total devices (hardware + virtual)
    int count = midi_context.device_count;
    for (int i = 0; i < MAX_VIRTUAL_DEVICES; i++) {
        if (midi_context.virtual_devices[i].active) {
            count++;
        }
    }

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_unlock(&midi_context.context_lock);
#endif
    return count;
}

int Pm_HasDeviceListChanged(void) {
    if (!midi_context.initialized) return 0; 
    
    mtx_lock(&midi_context.context_lock);
    int flag_status = midi_context.device_list_changed_flag;
    midi_context.device_list_changed_flag = 0; 
    mtx_unlock(&midi_context.context_lock);
    return flag_status;
}

// NEW: Function to set the device change callback
PmError Pm_SetDeviceChangeCallback(PmDeviceChangeCallback callback, void *user_data) {
    if (!midi_context.initialized) {
        Pm_Initialize(); // Ensure context is initialized before setting callback
    }
    mtx_lock(&midi_context.context_lock);
    midi_context.device_change_callback = callback;
    midi_context.device_change_user_data = user_data;
    mtx_unlock(&midi_context.context_lock);
    return pmNoError;
}


const PmDeviceInfo *Pm_GetDeviceInfo(PmDeviceID id) {
    if (!midi_context.initialized) return NULL;
    
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_lock(&midi_context.context_lock);
#endif
    
    const PmDeviceInfo *info = NULL;
    
    // Check hardware devices
    if (id >= 0 && id < midi_context.device_count) {
        info = &midi_context.devices[id].info;
    }
    // Check virtual devices
    else if (id >= MAX_DEVICES) {
        int slot = id - MAX_DEVICES;
        if (slot >= 0 && slot < MAX_VIRTUAL_DEVICES && midi_context.virtual_devices[slot].active) {
            info = &midi_context.virtual_devices[slot].info;
        }
    }
    
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_unlock(&midi_context.context_lock);
#endif
    
    return info;
}

PmDeviceID Pm_GetDefaultInputDeviceID(void) {
    if (!midi_context.initialized) Pm_Initialize();
    mtx_lock(&midi_context.context_lock);
    PmDeviceID default_id = PM_NO_DEVICE;
    for (int i = 0; i < midi_context.device_count; i++) {
        if (midi_context.devices[i].info.input) {
            default_id = i;
            break;
        }
    }
    mtx_unlock(&midi_context.context_lock);
    return default_id;
}

PmDeviceID Pm_GetDefaultOutputDeviceID(void) {
    if (!midi_context.initialized) Pm_Initialize();
    mtx_lock(&midi_context.context_lock);
    PmDeviceID default_id = PM_NO_DEVICE;
    for (int i = 0; i < midi_context.device_count; i++) {
        if (midi_context.devices[i].info.output) {
            default_id = i;
            break;
        }
    }
    mtx_unlock(&midi_context.context_lock);
    return default_id;
}

// ================================================================================================
// STREAM OPENING (Hardware + Virtual)
// ================================================================================================

/* Helper: Open virtual input stream */
static PmError Pm_OpenVirtualInput(PmStream **stream_ptr, PmDeviceID inputDevice) {
    int slot = inputDevice - MAX_DEVICES;
    if (slot < 0 || slot >= MAX_VIRTUAL_DEVICES) return pmInvalidDeviceId;
    
    PmVirtualDevice *vdev = &midi_context.virtual_devices[slot];
    if (!vdev->active || !vdev->info.input) return pmInvalidDeviceId;
    if (vdev->info.opened) return pmHostError; // Already opened
    
    PmStream *s = (PmStream *)malloc(sizeof(PmStream));
    if (!s) return pmInsufficientMemory;
    
    memset(s, 0, sizeof(PmStream));
    s->virtual_device = vdev;
    s->virtual_buffer = vdev->buffer;
    s->is_virtual = 1;
    s->device = NULL;
    s->input_buffer = NULL;
    s->connection_count = 0;
    
    vdev->info.opened = 1;
    *stream_ptr = s;
    
    return pmNoError;
}

/* Helper: Open virtual output stream */
static PmError Pm_OpenVirtualOutput(PmStream **stream_ptr, PmDeviceID outputDevice) {
    int slot = outputDevice - MAX_DEVICES;
    if (slot < 0 || slot >= MAX_VIRTUAL_DEVICES) return pmInvalidDeviceId;
    
    PmVirtualDevice *vdev = &midi_context.virtual_devices[slot];
    if (!vdev->active || !vdev->info.output) return pmInvalidDeviceId;
    if (vdev->info.opened) return pmHostError; // Already opened
    
    PmStream *s = (PmStream *)malloc(sizeof(PmStream));
    if (!s) return pmInsufficientMemory;
    
    memset(s, 0, sizeof(PmStream));
    s->virtual_device = vdev;
    s->virtual_buffer = vdev->buffer;
    s->is_virtual = 1;
    s->device = NULL;
    s->input_buffer = NULL;
    s->connection_count = 0;
    
    vdev->info.opened = 1;
    *stream_ptr = s;
    
    return pmNoError;
}

PmError Pm_OpenInput(PmStream **stream_ptr, PmDeviceID inputDevice, void *inputDriverInfo, int32_t bufferSize, PmTimestamp (*time_proc_unused)(void *), void *time_info_unused) {
    (void)inputDriverInfo; (void)bufferSize; (void)time_proc_unused; (void)time_info_unused;

    if (!midi_context.initialized) return pmHostError; 
    if (!stream_ptr) return pmBadPtr;
    
    // Check if it's a virtual device
    if (inputDevice >= MAX_DEVICES) {
        return Pm_OpenVirtualInput(stream_ptr, inputDevice);
    }

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_lock(&midi_context.context_lock); 

    if (inputDevice < 0 || inputDevice >= midi_context.device_count) {
        mtx_unlock(&midi_context.context_lock);
        return pmInvalidDeviceId;
    }
    MidiDevice *dev = &midi_context.devices[inputDevice];
    if (!dev->info.input) {
        mtx_unlock(&midi_context.context_lock);
        return pmInvalidDeviceId;
    }
    if (dev->info.opened) {
        mtx_unlock(&midi_context.context_lock);
        return pmHostError; 
    }

    PmStream *s = (PmStream*)malloc(sizeof(PmStream));
    if (!s) {
        mtx_unlock(&midi_context.context_lock);
        return pmInsufficientMemory;
    }
    s->device = dev;
    s->is_virtual = 0;
    s->virtual_device = NULL;
    s->virtual_buffer = NULL;
    s->connection_count = 0;
    
    s->input_buffer = (MidiInputBuffer*)malloc(sizeof(MidiInputBuffer));
    if (!s->input_buffer) {
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmInsufficientMemory;
    }

    s->input_buffer->head = 0;
    s->input_buffer->tail = 0;
    s->input_buffer->sysex_len = 0;
    mtx_init(&s->input_buffer->lock, mtx_plain);

    s->time_proc = NULL; 
    s->time_info = NULL;
    s->latency = 0;
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    s->sysex_pending = 0;
#endif


    MMRESULT res = midiInOpen(&dev->in_handle, dev->winmm_id, (DWORD_PTR)midi_in_proc, (DWORD_PTR)s->input_buffer, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        mtx_destroy(&s->input_buffer->lock);
        free(s->input_buffer);
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmHostError;
    }

    s->input_buffer->sysex_header_input.lpData = (LPSTR)s->input_buffer->sysex_buffer;
    s->input_buffer->sysex_header_input.dwBufferLength = SYSEX_BUFFER_SIZE;
    s->input_buffer->sysex_header_input.dwFlags = 0;
    s->input_buffer->sysex_header_input.dwUser = (DWORD_PTR)s->input_buffer; 

    res = midiInPrepareHeader(dev->in_handle, &s->input_buffer->sysex_header_input, sizeof(MIDIHDR));
    if (res != MMSYSERR_NOERROR) {
        midiInClose(dev->in_handle); 
        dev->in_handle = NULL;
        mtx_destroy(&s->input_buffer->lock);
        free(s->input_buffer);
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmHostError;
    }

    res = midiInAddBuffer(dev->in_handle, &s->input_buffer->sysex_header_input, sizeof(MIDIHDR));
    if (res != MMSYSERR_NOERROR) {
        midiInUnprepareHeader(dev->in_handle, &s->input_buffer->sysex_header_input, sizeof(MIDIHDR)); 
        midiInClose(dev->in_handle); 
        dev->in_handle = NULL;
        mtx_destroy(&s->input_buffer->lock);
        free(s->input_buffer);
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmHostError;
    }
    
    res = midiInStart(dev->in_handle);
    if (res != MMSYSERR_NOERROR) {
        midiInUnprepareHeader(dev->in_handle, &s->input_buffer->sysex_header_input, sizeof(MIDIHDR));
        midiInClose(dev->in_handle);
        dev->in_handle = NULL;
        mtx_destroy(&s->input_buffer->lock);
        free(s->input_buffer);
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmHostError;
    }

    dev->info.opened = 1;
    *stream_ptr = s;
    
    mtx_unlock(&midi_context.context_lock);
    return pmNoError;
#else
    return pmHostError; // No hardware MIDI support
#endif
}

PmError Pm_OpenOutput(PmStream **stream_ptr, PmDeviceID outputDevice, void *outputDriverInfo, int32_t bufferSize, PmTimestamp (*time_proc)(void *), void *time_info, int32_t latency) {
    (void)outputDriverInfo; (void)bufferSize; 

    if (!midi_context.initialized) return pmHostError;
    if (!stream_ptr) return pmBadPtr;
    
    // Check if it's a virtual device
    if (outputDevice >= MAX_DEVICES) {
        return Pm_OpenVirtualOutput(stream_ptr, outputDevice);
    }

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_lock(&midi_context.context_lock);

    if (outputDevice < 0 || outputDevice >= midi_context.device_count) {
        mtx_unlock(&midi_context.context_lock);
        return pmInvalidDeviceId;
    }
    MidiDevice *dev = &midi_context.devices[outputDevice];
    if (!dev->info.output) {
        mtx_unlock(&midi_context.context_lock);
        return pmInvalidDeviceId;
    }
    if (dev->info.opened) {
        mtx_unlock(&midi_context.context_lock);
        return pmHostError; 
    }

    PmStream *s = (PmStream*)malloc(sizeof(PmStream));
    if (!s) {
        mtx_unlock(&midi_context.context_lock);
        return pmInsufficientMemory;
    }
    s->device = dev;
    s->input_buffer = NULL;
    s->is_virtual = 0;
    s->virtual_device = NULL;
    s->virtual_buffer = NULL;
    s->connection_count = 0;
    
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    memset(&s->sysex_hdr_output, 0, sizeof(MIDIHDR)); 
    s->sysex_pending = 0;
#endif

    s->time_proc = time_proc;
    s->time_info = time_info;
    s->latency = latency;

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    // Open with callback for non-blocking sysex
    MMRESULT res = midiOutOpen(&dev->out_handle, dev->winmm_id, (DWORD_PTR)midi_out_proc, (DWORD_PTR)s, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        free(s);
        mtx_unlock(&midi_context.context_lock);
        return pmHostError;
    }
#endif

    dev->info.opened = 1;
    *stream_ptr = s;

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    mtx_unlock(&midi_context.context_lock);
#endif
    
    return pmNoError;
#else
    return pmHostError; // No hardware MIDI support
#endif
}

PmError Pm_Close(PmStream *stream) {
    if (!stream) return pmBadPtr;
    
    // Handle virtual device streams
    if (stream->is_virtual) {
        if (stream->virtual_device) {
            stream->virtual_device->info.opened = 0;
        }
        free(stream);
        return pmNoError;
    }
    
    // Handle hardware device streams
    if (!stream->device) return pmInternalError; 
    
    MidiDevice *dev = stream->device;

#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    if (dev->info.input && dev->in_handle) {
        midiInStop(dev->in_handle);
        midiInReset(dev->in_handle); 

        if (stream->input_buffer) {
             mtx_lock(&stream->input_buffer->lock); 
             if(stream->input_buffer->sysex_header_input.dwFlags & MHDR_PREPARED) {
                midiInUnprepareHeader(dev->in_handle, &stream->input_buffer->sysex_header_input, sizeof(MIDIHDR));
             }
             mtx_unlock(&stream->input_buffer->lock);
        }
        midiInClose(dev->in_handle);
        dev->in_handle = NULL;
        if (stream->input_buffer) {
            mtx_destroy(&stream->input_buffer->lock);
            free(stream->input_buffer);
            stream->input_buffer = NULL;
        }
    } else if (dev->info.output && dev->out_handle) {
        midiOutReset(dev->out_handle); // Stop all notes and cancel pending messages
        
        // NEW: Handle potentially pending sysex for non-blocking output
        if (stream->sysex_pending) {
            // A sysex message was sent, but MOM_DONE might not have been processed.
            // midiOutReset should have returned the buffer to the application.
            // We need to ensure it's unprepared.
            // A simple busy wait here is pragmatic for Pm_Close, as it's a cleanup operation.
            // The midi_out_proc should ideally set sysex_pending to 0.
            // If it hasn't, this loop helps ensure the header is done.
            int wait_cycles = 0;
            const int max_wait_cycles = 200; // Wait for max ~200ms for MOM_DONE via callback
            while (stream->sysex_pending && wait_cycles < max_wait_cycles) {
                Sleep(1); 
                wait_cycles++;
            }
            // If still pending, MOM_DONE didn't arrive quickly or callback failed.
            // Unprepare anyway if it was prepared. midiOutReset should make this safe.
            if (stream->sysex_hdr_output.dwFlags & MHDR_PREPARED) {
                 midiOutUnprepareHeader(dev->out_handle, &stream->sysex_hdr_output, sizeof(MIDIHDR));
            }
            stream->sysex_pending = 0; // Force clear
        }
        midiOutClose(dev->out_handle);
        dev->out_handle = NULL;
    }

    dev->info.opened = 0; 
    free(stream);
    return pmNoError;
#endif
}

int32_t Pm_Read(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !buffer || length <= 0) return 0;
    
    int32_t count = 0;
    
    // Virtual device stream
    if (stream->is_virtual) {
        count = Pm_ReadVirtual(stream, buffer, length);
    }
    // Hardware device stream
    else {
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
        if (!stream->input_buffer || !stream->device || !stream->device->info.input) return pmBadData; 

        MidiInputBuffer *in_buf = stream->input_buffer;

        mtx_lock(&in_buf->lock);
        while (count < length && in_buf->tail != in_buf->head) {
            buffer[count] = in_buf->events[in_buf->tail];
            in_buf->tail = (in_buf->tail + 1) % EVENT_BUFFER_SIZE;
            count++;
        }
        mtx_unlock(&in_buf->lock);
#else
        return 0; // No hardware MIDI support
#endif
    }
    
    // Recording support - record events that were read
    if (count > 0 && stream->recording && stream->recording->is_recording) {
        for (int i = 0; i < count && stream->recording->event_count < stream->recording->capacity; i++) {
            stream->recording->events[stream->recording->event_count++] = buffer[i];
        }
    }
    
    return count;
}

PmError Pm_ReadSysex(PmStream *stream, uint8_t *msg_buffer, int32_t *length) {
    if (!stream || !msg_buffer || !length || *length <= 0) return pmBadPtr;
    if (!stream->input_buffer || !stream->device || !stream->device->info.input) return pmBadData;

    MidiInputBuffer *in_buf = stream->input_buffer;
    PmError err = pmNoError;

    mtx_lock(&in_buf->lock);
    if (in_buf->sysex_len == 0) {
        *length = 0; 
    } else {
        int32_t bytes_to_copy = in_buf->sysex_len;
        if (bytes_to_copy > *length) {
            bytes_to_copy = *length;
            err = pmBufferTooSmall; 
        }
        memcpy(msg_buffer, in_buf->sysex_buffer, bytes_to_copy);
        *length = bytes_to_copy;
        
        in_buf->sysex_len = 0; 
    }
    mtx_unlock(&in_buf->lock);
    return err;
}


PmError Pm_Write(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !buffer || length < 0) return pmBadPtr;
    if (length == 0) return pmNoError;
    
    // Recording support
    if (stream->recording && stream->recording->is_recording) {
        for (int i = 0; i < length && stream->recording->event_count < stream->recording->capacity; i++) {
            stream->recording->events[stream->recording->event_count++] = buffer[i];
        }
    }
    
    // Virtual device stream
    if (stream->is_virtual) {
        PmError err = Pm_WriteVirtual(stream, buffer, length);
        if (err != pmNoError) return err;
        
        // Route to connected devices via router with filtering and transformation
        for (int i = 0; i < midi_context.router.connection_count; i++) {
            PmConnection *conn = &midi_context.router.connections[i];
            if (conn->active && conn->source == stream->virtual_device->id) {
                // Find destination stream
                const PmDeviceInfo *dest_info = Pm_GetDeviceInfo(conn->destination);
                if (dest_info && dest_info->opened) {
                    // Write to destination's buffer
                    int dest_slot = conn->destination - MAX_DEVICES;
                    if (dest_slot >= 0 && dest_slot < MAX_VIRTUAL_DEVICES) {
                        PmVirtualDevice *dest_vdev = &midi_context.virtual_devices[dest_slot];
                        if (dest_vdev->active && dest_vdev->buffer) {
                            // Write directly to destination buffer with filter/transform
                            for (int j = 0; j < length; j++) {
                                PmMessage msg = buffer[j].message;
                                
                                // Apply filter
                                if (conn->filter && !Pm_ApplyFilter((PmFilter*)conn->filter, msg)) {
                                    continue; // Message blocked by filter
                                }
                                
                                // Apply transform
                                if (conn->transform) {
                                    msg = Pm_ApplyTransform((PmTransform*)conn->transform, msg);
                                }
                                
                                // Write transformed message
                                uint32_t write_pos = atomic_load(&dest_vdev->buffer->write_pos);
                                uint32_t next_write = (write_pos + 1) & (VIRTUAL_BUFFER_SIZE - 1);
                                uint32_t read_pos = atomic_load(&dest_vdev->buffer->read_pos);
                                
                                if (next_write != read_pos) {
                                    dest_vdev->buffer->events[write_pos].message = msg;
                                    dest_vdev->buffer->events[write_pos].timestamp = buffer[j].timestamp;
                                    atomic_store(&dest_vdev->buffer->write_pos, next_write);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        return pmNoError;
    }
    
    // Hardware device stream
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    if (!stream->device || !stream->device->info.output || !stream->device->out_handle) return pmBadData; 

    HMIDIOUT handle = stream->device->out_handle;

    for (int i = 0; i < length; i++) {
        PmMessage msg = buffer[i].message;
        PmTimestamp timestamp = buffer[i].timestamp;

        if (stream->time_proc && timestamp > 0) {
            PmTimestamp current_time = stream->time_proc(stream->time_info);
            if (timestamp > current_time) {
                DWORD delay_ms = timestamp - current_time;
                if (delay_ms > 0) {
                    Sleep(delay_ms);
                }
            }
        }
        
        uint8_t status = Pm_MessageStatus(msg);
        if (status == 0xF0 || status == 0xF7) { 
            return pmBadData; 
        }

        MMRESULT res_short = midiOutShortMsg(handle, msg);
        if (res_short != MMSYSERR_NOERROR) {
            return pmHostError; 
        }
    }
    return pmNoError;
#else
    return pmHostError; // No hardware MIDI support
#endif
}


PmError Pm_WriteSysEx(PmStream *stream, PmTimestamp when, const uint8_t *msg, int32_t length) {
#if PM_HAS_HARDWARE_MIDI && PM_PLATFORM_WINDOWS
    if (!stream || !msg || length <= 0) return pmBadPtr;
    if (!stream->device || !stream->device->info.output || !stream->device->out_handle) return pmBadData;

    if (length > 0 && msg[0] != 0xF0) return pmBadData; 
    if (length > 1 && msg[length - 1] != 0xF7) return pmBadData; 

    // NEW: Check for pending sysex (non-blocking)
    if (stream->sysex_pending) return pmSysexBusy; 

    HMIDIOUT handle = stream->device->out_handle;

    if (stream->time_proc && when > 0) {
        PmTimestamp current_time = stream->time_proc(stream->time_info);
        if (when > current_time) {
            DWORD delay_ms = when - current_time;
            if (delay_ms > 0) {
                Sleep(delay_ms);
            }
        }
    }

    if (length > SYSEX_BUFFER_SIZE) { 
        return pmSysexOverflow; 
    }
    
    memcpy(stream->sysex_buffer_output, msg, length);

    stream->sysex_hdr_output.lpData = (LPSTR)stream->sysex_buffer_output;
    stream->sysex_hdr_output.dwBufferLength = length;
    stream->sysex_hdr_output.dwBytesRecorded = length; 
    stream->sysex_hdr_output.dwFlags = 0; 

    MMRESULT res_prepare = midiOutPrepareHeader(handle, &stream->sysex_hdr_output, sizeof(MIDIHDR));
    if (res_prepare != MMSYSERR_NOERROR) {
        return pmHostError;
    }

    stream->sysex_pending = 1; // NEW: Set pending flag
    MMRESULT res_long = midiOutLongMsg(handle, &stream->sysex_hdr_output, sizeof(MIDIHDR));
    if (res_long != MMSYSERR_NOERROR) {
        // If midiOutLongMsg fails, we need to unprepare the header ourselves
        // and clear the pending flag.
        midiOutUnprepareHeader(handle, &stream->sysex_hdr_output, sizeof(MIDIHDR));
        stream->sysex_pending = 0; 
        return pmHostError;
    }

    // NEW: No blocking wait here. MOM_DONE callback will handle unprepare and clear pending.
    return pmNoError;
#else
    return pmHostError; // No hardware MIDI support
#endif
}

// ================================================================================================
// PHASE 3: ADVANCED FEATURES - FILTERING, TRANSFORMATION, RECORDING
// ================================================================================================

/* Apply MIDI filter to an event - returns 1 if event should pass, 0 if blocked */
static int Pm_ApplyFilter(const PmFilter *filter, PmMessage msg) {
    if (!filter) return 1; // No filter = pass all
    
    uint8_t status = Pm_MessageStatus(msg);
    uint8_t channel = status & 0x0F;
    uint8_t msg_type = status & 0xF0;
    
    // Check channel mask
    if (!(filter->channel_mask & (1 << channel))) return 0;
    
    // Check message type filters
    switch (msg_type) {
        case 0x80: return !filter->filter_note_off;
        case 0x90: return !filter->filter_note_on;
        case 0xB0: return !filter->filter_cc;
        case 0xC0: return !filter->filter_program_change;
        case 0xE0: return !filter->filter_pitch_bend;
        case 0xA0:
        case 0xD0: return !filter->filter_aftertouch;
        case 0xF0: return !filter->filter_sysex;
        default: return 1; // Unknown message types pass through
    }
}

/* Apply MIDI transformation to an event */
static PmMessage Pm_ApplyTransform(const PmTransform *transform, PmMessage msg) {
    if (!transform) return msg; // No transform = pass through
    
    uint8_t status = Pm_MessageStatus(msg);
    uint8_t data1 = Pm_MessageData1(msg);
    uint8_t data2 = Pm_MessageData2(msg);
    uint8_t channel = status & 0x0F;
    uint8_t msg_type = status & 0xF0;
    
    // Channel remapping
    if (transform->channel_remap[channel] != 0xFF) {
        channel = transform->channel_remap[channel] & 0x0F;
        status = msg_type | channel;
    }
    
    // Note transposition (only for Note On/Off)
    if (msg_type == 0x80 || msg_type == 0x90) {
        int new_note = (int)data1 + transform->transpose;
        if (new_note < 0) new_note = 0;
        if (new_note > 127) new_note = 127;
        data1 = (uint8_t)new_note;
    }
    
    // Velocity scaling (only for Note On and Poly Aftertouch)
    if (msg_type == 0x90 || msg_type == 0xA0) {
        float velocity = (float)data2;
        
        // Apply curve
        switch (transform->velocity_curve) {
            case 1: // Exponential
                velocity = (velocity / 127.0f) * (velocity / 127.0f) * 127.0f;
                break;
            case 2: // Logarithmic
                velocity = sqrtf(velocity / 127.0f) * 127.0f;
                break;
            default: // Linear (0)
                break;
        }
        
        // Apply scale
        velocity *= transform->velocity_scale;
        if (velocity < 0.0f) velocity = 0.0f;
        if (velocity > 127.0f) velocity = 127.0f;
        data2 = (uint8_t)velocity;
    }
    
    return Pm_Message(status, data1, data2);
}

/* Set filter for a connection */
PmError Pm_SetConnectionFilter(PmDeviceID source, PmDeviceID dest, const PmFilter *filter) {
    if (!midi_context.initialized) return pmHostError;
    
    // Find the connection
    for (int i = 0; i < midi_context.router.connection_count; i++) {
        PmConnection *conn = &midi_context.router.connections[i];
        if (conn->source == source && conn->destination == dest && conn->active) {
            // Free existing filter if any
            if (conn->filter) {
                free(conn->filter);
                conn->filter = NULL;
            }
            
            // Set new filter
            if (filter) {
                conn->filter = malloc(sizeof(PmFilter));
                if (!conn->filter) return pmInsufficientMemory;
                memcpy(conn->filter, filter, sizeof(PmFilter));
            }
            
            return pmNoError;
        }
    }
    
    return pmInvalidDeviceId; // Connection not found
}

/* Set transform for a connection */
PmError Pm_SetConnectionTransform(PmDeviceID source, PmDeviceID dest, const PmTransform *transform) {
    if (!midi_context.initialized) return pmHostError;
    
    // Find the connection
    for (int i = 0; i < midi_context.router.connection_count; i++) {
        PmConnection *conn = &midi_context.router.connections[i];
        if (conn->source == source && conn->destination == dest && conn->active) {
            // Free existing transform if any
            if (conn->transform) {
                free(conn->transform);
                conn->transform = NULL;
            }
            
            // Set new transform
            if (transform) {
                conn->transform = malloc(sizeof(PmTransform));
                if (!conn->transform) return pmInsufficientMemory;
                memcpy(conn->transform, transform, sizeof(PmTransform));
            }
            
            return pmNoError;
        }
    }
    
    return pmInvalidDeviceId; // Connection not found
}

/* Create a recording buffer */
PmError Pm_CreateRecording(PmRecording *recording, int32_t capacity) {
    if (!recording || capacity <= 0) return pmBadPtr;
    
    recording->events = (PmEvent *)malloc(capacity * sizeof(PmEvent));
    if (!recording->events) return pmInsufficientMemory;
    
    recording->capacity = capacity;
    recording->event_count = 0;
    recording->start_time = 0;
    recording->is_recording = 0;
    
    return pmNoError;
}

/* Free a recording buffer */
PmError Pm_FreeRecording(PmRecording *recording) {
    if (!recording) return pmBadPtr;
    
    if (recording->events) {
        free(recording->events);
        recording->events = NULL;
    }
    
    recording->capacity = 0;
    recording->event_count = 0;
    recording->is_recording = 0;
    
    return pmNoError;
}

/* Start recording on a stream */
PmError Pm_StartRecording(PmStream *stream, PmRecording *recording) {
    if (!stream || !recording) return pmBadPtr;
    if (!recording->events || recording->capacity <= 0) return pmBadData;
    
    stream->recording = recording;
    recording->event_count = 0;
    recording->is_recording = 1;
    
    // Set start time
    if (stream->time_proc) {
        recording->start_time = stream->time_proc(stream->time_info);
    } else {
        recording->start_time = 0;
    }
    
    return pmNoError;
}

/* Stop recording on a stream */
PmError Pm_StopRecording(PmStream *stream) {
    if (!stream) return pmBadPtr;
    
    if (stream->recording) {
        stream->recording->is_recording = 0;
        stream->recording = NULL;
    }
    
    return pmNoError;
}

/* Play back a recording */
PmError Pm_PlayRecording(PmStream *stream, const PmRecording *recording) {
    if (!stream || !recording) return pmBadPtr;
    if (!recording->events || recording->event_count <= 0) return pmBadData;
    
    // Get current time
    PmTimestamp current_time = 0;
    if (stream->time_proc) {
        current_time = stream->time_proc(stream->time_info);
    }
    
    // Play all events with timing
    for (int32_t i = 0; i < recording->event_count; i++) {
        PmEvent event = recording->events[i];
        
        // Adjust timestamp relative to current time
        if (stream->time_proc && event.timestamp > 0) {
            PmTimestamp relative_time = event.timestamp - recording->start_time;
            event.timestamp = current_time + relative_time;
        }
        
        PmError err = Pm_Write(stream, &event, 1);
        if (err != pmNoError) return err;
    }
    
    return pmNoError;
}


#endif /* MIDI_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MIDI_H */