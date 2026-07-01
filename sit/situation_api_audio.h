/***************************************************************************************************
*
*   situation_api_audio.h - Audio Playback, Graph, and MIDI API
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Device management, capture, sound loading, procedural tones, effects, node graph routing,
*   MIDI control/learn, graph serialization, and device enumeration functions.
*
*   Requires SITAPI from situation_api.h. Types from situation_api_types_audio.h.
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_AUDIO_H
#define SITUATION_API_AUDIO_H

#include "situation_api_config.h"
#include "situation_base_types.h"
#include "situation_api_types_audio.h"

//==================================================================================
// Audio Module
//==================================================================================
//
//  Recommended API paths (see doc/situation_sdk.md):
//    One-shot SFX     → SituationLoadAudio + SituationPlayAudio
//    Per-sound DSP    → SituationSound* + SituationAttachAudioProcessor
//    Procedural/MIDI  → SituationPlayToneEx + graph routing
//    Mix / FX chain   → SituationCreateGraph + SituationCreateNode
//
// --- Audio Device Management ---
SITAPI SituationError SituationSetAudioDevice(int internal_id, const SituationAudioFormat* format); // Set the active audio device.
SITAPI int SituationGetAudioPlaybackSampleRate(void);                                   // Get the sample rate of the current audio device.
SITAPI SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);             // Re-initialize the audio device with a new sample rate.
SITAPI float SituationGetAudioMasterVolume(void);                                       // Get the master volume for the audio device.
SITAPI SituationError SituationSetAudioMasterVolume(float volume);                      // Set the master volume for the audio device.
SITAPI bool SituationIsAudioDevicePlaying(void);                                        // Check if the audio device is currently playing.
SITAPI SituationError SituationPauseAudioDevice(void);                                  // Pause audio playback on the device.
SITAPI SituationError SituationResumeAudioDevice(void);                                 // Resume audio playback on the device.

// --- Audio Capture ---
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback callback, void* user_data);                                          // Start capturing audio input with default format.
SITAPI SituationError SituationStartAudioCaptureEx(SituationAudioCaptureCallback callback, void* user_data, uint32_t sample_rate, uint32_t channels); // Start capturing with explicit sample rate and channel count.
SITAPI void SituationStopAudioCapture(void);                                            // Stop audio capture and release the input device.

// --- Audio Output Monitoring (for visualization) ---
SITAPI void SituationSetAudioOutputMonitor(void (*callback)(const float* samples, uint32_t frame_count, void* user_data), void* user_data); // Set a callback to receive mixed output samples (for VU meters, FFT, etc.).
SITAPI void SituationGetMasterOutputMeter(float* out_peak, float* out_rms); // Last playback callback block: peak sample magnitude & RMS (optional pointers; safe from main/UI thread).

// --- Sound Loading and Management ---
// --- Audio Handle API ---
SITAPI SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping); // Load audio and return a lightweight handle for playback control.
SITAPI SituationError SituationPlayAudio(SituationSoundHandle handle);                  // Play audio by handle (restarts if already playing).
SITAPI void SituationUnloadAudio(SituationSoundHandle handle);                          // Unload audio by handle and free resources.
SITAPI SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume); // Set volume for a handle-based sound [0.0 to 1.0+].
SITAPI SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan);     // Set stereo pan for a handle-based sound [-1.0 to 1.0].
SITAPI SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch); // Set pitch multiplier for a handle-based sound (1.0 = normal).

SITAPI SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound); // Load a sound from a file.
SITAPI SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound); // Load a sound from a custom stream.
SITAPI void SituationUnloadSound(SituationSound* sound);                                // Unload a sound and free its resources.
SITAPI SituationError SituationPlayLoadedSound(SituationSound* sound);                  // Play a loaded sound (restarts if already playing).
SITAPI SituationError SituationStopLoadedSound(SituationSound* sound);                  // Stop a specific sound from playing.
SITAPI SituationError SituationStopAllLoadedSounds(void);                               // Stop all currently playing sounds.

// --- Procedural Tones (Resonance) ---
// --- Resonance (Procedural Synthesis) ---
// SituationToneHandle is defined in situation_base_types.h

/**
 * @brief Plays an extended procedural tone with full control.
 *
 * @param type          Waveform type (Sine, Square, Triangle, Saw, Noise)
 * @param frequency     Frequency in Hz (e.g., 440.0f). For noise: ignored (use 0.0f)
 * @param volume        Peak volume (0.0 to 1.0)
 * @param pan           Stereo panning (-1.0 left, 0.0 center, +1.0 right)
 * @param attack_sec    Attack time in seconds
 * @param decay_sec     Decay time in seconds
 * @param sustain_level Sustain volume level (0.0 to 1.0)
 * @param release_sec   Release time in seconds
 * @param hold_sec      Hold duration in seconds. Use -1.0f for infinite sustain (key down)
 *
 * @return Handle to the playing tone, or 0 if no voice available (polyphony limit)
 */
SITAPI SituationToneHandle SituationPlayToneEx(
    SituationWaveType type,
    float frequency,
    float volume,
    float pan,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);

SITAPI void SituationStopTone(SituationToneHandle handle);                              // Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored.

SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec); // Legacy: play a simple ADSR tone (backward compat / quick UI sounds).
SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);   // Legacy: play a tone by MIDI note number (0-127).
SITAPI void SituationStopAllTones(void);                                                // Stop all active tones (triggers release on each).

// --- Sound Data Manipulation (Wave Utilities) ---
SITAPI SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination);    // Create a new sound by copying the raw PCM data from a source.
SITAPI SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame);   // Crop a sound's PCM data in-place to a new range.
SITAPI SituationError SituationSoundExportAsWav(const SituationSound* sound, const char* fileName);                   // Export the sound's raw PCM data to a WAV file.

// --- Sound Parameters and Effects ---
SITAPI SituationError SituationSetSoundVolume(SituationSound* sound, float volume);     // Set the volume for a specific sound.
SITAPI float SituationGetSoundVolume(SituationSound* sound);                            // Get the volume of a specific sound.
SITAPI SituationError SituationSetSoundPan(SituationSound* sound, float pan);           // Set the stereo pan for a sound [-1.0 to 1.0].
SITAPI float SituationGetSoundPan(SituationSound* sound);                               // Get the stereo pan of a sound.
SITAPI SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);       // Set the pitch for a sound (resamples).
SITAPI float SituationGetSoundPitch(SituationSound* sound);                             // Get the pitch of a sound.
SITAPI SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);                    // Apply a low-pass or high-pass filter to a sound.
SITAPI SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);                   // Apply an echo effect to a sound.
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);   // Apply a reverb effect to a sound.

// --- Custom Audio Processing ---
SITAPI SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Attach a custom DSP processor to a sound's effect chain.
SITAPI SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Detach a custom DSP processor from a sound.

// [Phase H] Removed: Legacy Mixer API (replaced by node graph system)
// Use SituationCreateGraph() + SituationCreateNode(SITUATION_NODE_MIXER) + SituationProcessGraph() instead.

// ================================================================================================
// NODE GRAPH & DEVICE REGISTRY API (Phase 3-5)
// ================================================================================================

// --- Device Registry Functions ---
SITAPI void SituationInitDeviceRegistry(void);                                          // Initialize the built-in device registry (call once at startup).
SITAPI int SituationGetRegisteredDeviceCount(void);                                     // Get the number of registered audio device types.
SITAPI SituationError SituationRegisterDeviceType(const SituationDeviceMetadata* meta); // Register a custom device type with the registry.
SITAPI SituationError SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out_meta); // Get metadata for a registered device type.
SITAPI bool SituationIsDeviceRegistered(SituationNodeType type);                        // Check if a device type is registered.
SITAPI const char* SituationGetCategoryName(SituationDeviceCategory category);          // Get the display name for a device category.

// --- Active Graph (Audio Callback Integration) ---
SITAPI SituationError SituationSetActiveGraph(SituationAudioGraph* graph);              // Set the active audio processing graph (replaces default). NULL disables graph processing.
SITAPI SituationAudioGraph* SituationGetActiveGraph(void);                              // Get the currently active audio processing graph (NULL if none).

// --- Node Graph Functions ---
SITAPI SituationAudioGraph* SituationCreateGraph(void);                                 // Create a new audio processing graph.
SITAPI void SituationDestroyGraph(SituationAudioGraph* graph);                          // Destroy a graph and all its nodes/patches.
SITAPI SituationError SituationCreateNode(SituationAudioGraph* graph, SituationNodeType type, SituationNodeHandle* handle); // Create a node of the given type in the graph.
SITAPI SituationError SituationDestroyNode(SituationAudioGraph* graph, SituationNodeHandle handle); // Remove and destroy a node from the graph.
SITAPI SituationNode* SituationGetNode(SituationAudioGraph* graph, SituationNodeHandle handle); // Get a direct pointer to a node (for advanced use).
SITAPI SituationError SituationCreatePatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port, bool is_control); // Connect an output port to an input port.
SITAPI SituationError SituationRemovePatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port, bool is_control); // Disconnect a specific patch between two ports.
SITAPI SituationError SituationDestroyPatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port); // Disconnect a patch between two ports (legacy, no is_control param).
SITAPI SituationError SituationTopologicalSort(SituationAudioGraph* graph);              // Re-sort the graph processing order after topology changes. Call from the main thread after CreateNode/DestroyNode/CreatePatch/RemovePatch.
SITAPI SituationError SituationSetControl(SituationAudioGraph* graph, SituationNodeHandle handle, uint32_t control_id, float value); // Set a control parameter on a node.
SITAPI SituationError SituationGetControl(SituationAudioGraph* graph, SituationNodeHandle handle, uint32_t control_id, float* out_value); // Get the current value of a node's control parameter.

// --- PCM Input Node (user-fed ring buffer source) ---
SITAPI uint32_t SituationPushNodePCM(SituationAudioGraph* graph, SituationNodeHandle node, const float* samples, uint32_t frame_count, uint32_t channels); // Push interleaved float PCM into a PCM_INPUT node's ring buffer (any thread). Returns frames written.
SITAPI uint32_t SituationGetNodePCMFreeFrames(SituationAudioGraph* graph, SituationNodeHandle node); // Query how many frames of space are available in the PCM_INPUT node's ring buffer.

// ================================================================================================
// MIDI CONTROL INTEGRATION
// ================================================================================================

// --- MIDI Device Control ---
SITAPI SituationError SituationEnableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle, int device_id);  // Enable MIDI CC control for a node. Pass device_id=-1 for auto-select.
SITAPI SituationError SituationDisableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle);                // Disable MIDI control for a node.
SITAPI SituationError SituationAutoConnectMidi(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Convenience: auto-select first available MIDI input. Equivalent to EnableMidiControl(..., -1).
SITAPI int SituationListMidiDevices(SituationMidiDeviceInfo* devices, int max_count);                                     // List available MIDI input devices. Returns number found.
SITAPI SituationError SituationGetMidiDeviceName(int device_id, char* out_name, size_t out_name_size);                    // PortMidi device name for device_id (hardware or virtual).
SITAPI int SituationIsMidiEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);                                // Check if a node has MIDI control enabled. Returns 1/0.
SITAPI SituationError SituationSetNodeMidiChannel(SituationAudioGraph* graph, SituationNodeHandle handle, int channel);   // Filter MIDI to channel 0-15, or -1 omni.

// --- Official names for harness virtual MIDI + graph tone synth target (PortMidi + SIT_MidiDevice) ---
#define SITUATION_TEST_MIDI_CHANNEL           0    /* 0-based; human-readable MIDI channel 1 */
#define SITUATION_VIRTUAL_MIDI_IN_NAME        "Situation Test MIDI In"
#define SITUATION_VIRTUAL_MIDI_OUT_NAME       "Situation Test MIDI Out"
#define SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME "Tone Synth"

// --- Virtual MIDI loopback (integration testing; no hardware keyboard required) ---
SITAPI SituationError SituationSetupVirtualMidiLoopback(int* out_input_device_id);  // Create connected virtual out→in pair. Returns input device_id for SituationEnableMidiControl().
SITAPI SituationError SituationVirtualMidiNoteOnEx(uint8_t channel, uint8_t note, uint8_t velocity); // Channel-aware note-on (0-15).
SITAPI SituationError SituationVirtualMidiNoteOffEx(uint8_t channel, uint8_t note);                  // Channel-aware note-off (0-15).
SITAPI SituationError SituationVirtualMidiNoteOn(uint8_t note, uint8_t velocity);     // Inject note-on on channel 0 (legacy wrapper).
SITAPI SituationError SituationVirtualMidiNoteOff(uint8_t note);                      // Inject note-off on channel 0 (legacy wrapper).
SITAPI SituationError SituationVirtualMidiControlChange(uint8_t channel, uint8_t controller, uint8_t value); // CC (e.g. mod wheel, expression).
SITAPI SituationError SituationVirtualMidiPitchBend(uint8_t channel, int16_t bend);   // Pitch bend 0..16383 (center 8192).
SITAPI SituationError SituationVirtualMidiProgramChange(uint8_t channel, uint8_t program); // Program change on channel 0-15.
SITAPI void SituationTeardownVirtualMidiLoopback(void);                             // Close and destroy the virtual loopback devices.

// ================================================================================================
// MIDI LEARN INTEGRATION (v2.6.0)
// ================================================================================================
// Dynamic MIDI CC learning: map physical knobs/faders to node parameters at runtime.
// Requires MIDI to be enabled first via SituationEnableMidiControl().

// --- MIDI Learn Lifecycle ---
SITAPI SituationError SituationEnableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Enable MIDI Learn capability for a node. MIDI must already be enabled.
SITAPI SituationError SituationDisableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                  // Disable MIDI Learn for a node.
SITAPI int SituationIsMidiLearnEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);                           // Check if MIDI Learn is enabled. Returns 1/0.

// --- Learning Operations ---
SITAPI SituationError SituationStartMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index, const char* param_name, float min_value, float max_value, int scaling); // Start learning: next CC received maps to this param. Scaling: 0=linear, 1=log, 2=dB, 3=discrete. Times out after 5s.
SITAPI SituationError SituationCancelMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Cancel an active learn operation.
SITAPI int SituationIsLearning(SituationAudioGraph* graph, SituationNodeHandle handle);                                   // Check if currently in learn mode. Returns 1/0.

// --- Mapping Management ---
SITAPI SituationError SituationClearMidiMapping(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index); // Clear a specific learned CC mapping.
SITAPI SituationError SituationClearAllMidiMappings(SituationAudioGraph* graph, SituationNodeHandle handle);               // Clear all learned mappings for a node.

// --- Preset Persistence ---
SITAPI SituationError SituationSaveMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);  // Save MIDI Learn mappings to JSON file.
SITAPI SituationError SituationLoadMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);  // Load MIDI Learn mappings from JSON file.

// --- Graph Serialization Functions ---
SITAPI SituationError SituationSaveGraphToFile(const SituationAudioGraph* graph, const char* filepath);   // Save a graph to a JSON file.
SITAPI SituationError SituationLoadGraphFromFile(SituationAudioGraph* graph, const char* filepath, const SituationDeviceFunctions* device_funcs, int num_device_funcs); // Load a graph from a JSON file, re-creating nodes via device_funcs.
SITAPI char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph);           // Serialize a graph to a JSON string (caller must free with SituationFreeJSONString).
SITAPI SituationError SituationDeserializeGraphFromJSON(SituationAudioGraph* graph, const char* json_string, const SituationDeviceFunctions* device_funcs, int num_device_funcs); // Deserialize a graph from a JSON string.
SITAPI void SituationFreeJSONString(char* json_string);                                 // Free a JSON string returned by SituationSerializeGraphToJSON.
SITAPI const char* SituationGetSerializationVersion(void);                              // Get the current serialization format version string.
SITAPI bool SituationIsVersionCompatible(const char* json_version);                     // Check if a serialized version is compatible with this library.

// --- Device Enumeration (Phase 0) ---
SITAPI SituationAudioDeviceInfo* SituationEnumerateAudioDevices(int* out_count);         // [Caller frees via SituationFreeDeviceList] Canonical device enumeration.
SITAPI void SituationFreeDeviceList(SituationAudioDeviceInfo* devices, int count);       // Free a device list returned by SituationEnumerateAudioDevices.
SITAPI SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type, uint32_t min_channels_out, uint32_t min_channels_in); // Find the best matching device by type and channel requirements.

// --- Node Graph SFX Routing (v2.6.5) ---
SITAPI SituationError SituationSetToneRouting(SituationToneHandle handle, bool route_to_graph);                   // Route a procedural tone to the active graph's SFX sound source.
SITAPI SituationError SituationSetGraphSFXSource(SituationNodeHandle handle);                                     // Designate the Sound Source node in the active graph to receive routed SFX tones.

#endif /* SITUATION_API_AUDIO_H */
