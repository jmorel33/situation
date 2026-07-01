## Audio Module

**Overview:** The Audio module offers a full-featured audio engine capable of loading sounds (`SituationLoadSoundFromFile`) for low-latency playback and streaming longer tracks (`SituationLoadSoundFromStream`) to conserve memory. It supports device management, playback control (volume, pan, pitch), a built-in effects chain (filters, reverb), and custom real-time audio processors. For modular effect chains and synthesizer routing, see the [Audio Node Graph](audio_graph.md) module.

### Playback path & threading (v2.4.48+)

- **Default graph (`Policy B`)**: After **`SituationInit`**, the library may enable an internal **`default_graph`** where **`SituationPlayLoadedSound`** voices are summed into the **`SITUATION_NODE_SOUND_SOURCE`** before **`SituationProcessGraph`**, so the **mixer** combines loaded sounds with the graph tone synth. Latent mixing into the device buffer is skipped when that path is active (see **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** § *Canonical miniaudio callback pipeline*).
- **Graph topology**: Call **`SituationCreateNode`**, **`SituationDestroyNode`**, **`SituationCreatePatch`**, **`SituationRemovePatch`**, and **`SituationTopologicalSort`** from the **main / control thread** only — **not** from a device **`process`** callback running inside **`SituationProcessGraph`**.
- **Streaming decoder**: **`ma_decoder`** reads on the audio thread and seeks / queue updates on the main thread are serialized via the same **`audio_queue_mutex`** used for the voice queue (**`SituationPlayLoadedSound`** restart seek runs under that lock).

### Structs and Enums

#### `SituationAudioDeviceInfo`
Contains information about a single audio playback device available on the system.
```c
typedef struct SituationAudioDeviceInfo {
    int internal_id;
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    bool is_default;
    int min_channels, max_channels;
    int min_sample_rate, max_sample_rate;
} SituationAudioDeviceInfo;
```
-   `internal_id`: The ID used to select this device with `SituationSetAudioDevice()`.
-   `name`: The human-readable name of the device.
-   `is_default`: `true` if this is the operating system's default audio device.
-   `min_channels`, `max_channels`: The minimum and maximum number of channels supported by the device.
-   `min_sample_rate`, `max_sample_rate`: The minimum and maximum sample rates supported by the device.

---
#### `SituationAudioFormat`
Describes the format of audio data, used when initializing the audio device or loading sounds from custom streams.
```c
typedef struct SituationAudioFormat {
    int channels;
    int sample_rate;
    int bit_depth;
} SituationAudioFormat;
```
-   `channels`: Number of audio channels (e.g., 1 for mono, 2 for stereo).
-   `sample_rate`: Number of samples per second (e.g., 44100 Hz).
-   `bit_depth`: Number of bits per sample (e.g., 16-bit).

---
#### `SituationSound`
An opaque handle to a sound resource. This handle encapsulates all the necessary internal state for a sound, whether it's fully loaded into memory or streamed from a source. It is initialized by `SituationLoadSoundFromFile()` or `SituationLoadSoundFromStream()` and must be cleaned up with `SituationUnloadSound()`.
```c
typedef struct SituationSound {
    uint64_t id; // Internal unique ID
    // Internal data is not exposed to the user
} SituationSound;
```
- **Creation:** `SituationLoadSoundFromFile()`, `SituationLoadSoundFromStream()`
- **Usage:** `SituationPlayLoadedSound()`, `SituationSetSoundVolume()`
- **Destruction:** `SituationUnloadSound()`

---
#### `SituationFilterType`
Specifies the type of filter to apply to a sound.
| Type | Description |
|---|---|
| `SIT_FILTER_NONE` | No filter is applied. |
| `SIT_FILTER_LOW_PASS` | Allows low frequencies to pass through. |
| `SIT_FILTER_HIGH_PASS` | Allows high frequencies to pass through. |

#### Functions
### Functions

#### Audio Device Management
---
#### `SituationIsAudioDeviceReady`
Checks if the audio device has been successfully initialized via `SituationInit`. Always check this before attempting audio operations.

```c
bool SituationIsAudioDeviceReady(void);
```

**Returns:** `true` if audio device is initialized, `false` otherwise

**Usage Example:**
```c
// Check before playing audio
if (SituationIsAudioDeviceReady()) {
    SituationPlayLoadedSound(&sound);
} else {
    printf("Error: Audio device not initialized\n");
}

// Graceful degradation
if (!SituationIsAudioDeviceReady()) {
    printf("Running in silent mode (no audio device)\n");
    // Continue without audio
}

// Verify initialization
if (!SituationIsAudioDeviceReady()) {
    fprintf(stderr, "Failed to initialize audio device\n");
    // Check error message
    const char* error = SituationGetLastErrorMsg();
    fprintf(stderr, "Audio error: %s\n", error);
}
```

**Notes:**
- Returns false if audio init failed or was disabled
- Check before any audio operations
- Audio may fail on headless systems

---
#### `SituationIsAudioDevicePlaying`
Checks if the audio device is currently playing any sounds. Returns false if all sounds are stopped or paused.

```c
bool SituationIsAudioDevicePlaying(void);
```

**Returns:** `true` if any sound is playing, `false` if all stopped

**Usage Example:**
```c
// Wait for all sounds to finish
while (SituationIsAudioDevicePlaying()) {
    SituationPollInputEvents();
    SituationSleep(10);
}

// Pause game when no audio is playing
if (!SituationIsAudioDevicePlaying() && game_state == CUTSCENE) {
    // Cutscene audio finished
    EndCutscene();
}

// Show audio indicator
if (SituationIsAudioDevicePlaying()) {
    DrawAudioIcon();
}
```

**Notes:**
- Returns false if device is paused
- Checks all active sounds
- Useful for cutscene synchronization

---
#### `SituationGetAudioMasterVolume`
Gets the current master volume for the audio device. Returns a value between 0.0 (silent) and 1.0 (full volume).

```c
float SituationGetAudioMasterVolume(void);
```

**Returns:** Current master volume (0.0 to 1.0)

**Usage Example:**
```c
// Display volume slider
float volume = SituationGetAudioMasterVolume();
DrawVolumeSlider(volume);

// Save volume to settings
float current_volume = SituationGetAudioMasterVolume();
SaveSetting("audio.master_volume", current_volume);

// Toggle mute
static float saved_volume = 1.0f;
if (SituationIsKeyPressed(SIT_KEY_M)) {
    float current = SituationGetAudioMasterVolume();
    if (current > 0.0f) {
        saved_volume = current;
        SituationSetAudioMasterVolume(0.0f);
    } else {
        SituationSetAudioMasterVolume(saved_volume);
    }
}
```

**Notes:**
- Returns 0.0 if audio device not ready
- Use with `SituationSetAudioMasterVolume()` for volume controls
- Affects all sounds globally

---
#### `SituationSetAudioMasterVolume`
Sets the master volume for the entire audio device, from `0.0` (silent) to `1.0` (full volume). Affects all currently playing and future sounds.

```c
SituationError SituationSetAudioMasterVolume(float volume);
```

**Parameters:**
- `volume` - Master volume (0.0 = silent, 1.0 = full)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Volume slider
float volume = GetSliderValue();  // 0.0 to 1.0
SituationSetAudioMasterVolume(volume);

// Fade out audio
for (float v = 1.0f; v >= 0.0f; v -= 0.01f) {
    SituationSetAudioMasterVolume(v);
    SituationSleep(16);  // ~60 FPS
}

// Mute when window loses focus
if (!SituationHasWindowFocus()) {
    SituationSetAudioMasterVolume(0.0f);
} else {
    SituationSetAudioMasterVolume(settings.master_volume);
}
```

**Notes:**
- Clamps to 0.0-1.0 range
- Affects all sounds immediately
- Multiplies with individual sound volumes

---
#### `SituationGetAudioPlaybackSampleRate`
Gets the sample rate of the current audio device in Hz (e.g., 44100, 48000).

```c
int SituationGetAudioPlaybackSampleRate(void);
```

**Returns:** Sample rate in Hz, or 0 if device not ready

**Usage Example:**
```c
// Display audio info
int sample_rate = SituationGetAudioPlaybackSampleRate();
printf("Audio device: %d Hz\n", sample_rate);

// Verify sample rate
int rate = SituationGetAudioPlaybackSampleRate();
if (rate != 48000) {
    printf("Warning: Expected 48kHz, got %dHz\n", rate);
}

// Calculate buffer size
int sample_rate = SituationGetAudioPlaybackSampleRate();
int buffer_size = sample_rate / 60;  // 1 frame at 60 FPS
```

**Notes:**
- Common rates: 44100 (CD quality), 48000 (professional)
- Returns 0 if audio device not initialized
- Set during initialization, read-only at runtime

---
#### `SituationSetAudioPlaybackSampleRate`
Re-initializes the audio device with a new sample rate. This stops all currently playing sounds and may cause a brief audio interruption.

```c
SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);
```

**Parameters:**
- `sample_rate` - New sample rate in Hz (e.g., 44100, 48000)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Change sample rate in settings menu
if (user_selected_48khz) {
    // Stop all sounds first
    SituationStopAllLoadedSounds();
    
    // Change sample rate
    if (SituationSetAudioPlaybackSampleRate(48000) == SITUATION_SUCCESS) {
        printf("Sample rate changed to 48kHz\n");
    } else {
        printf("Failed to change sample rate\n");
    }
}

// Try high-quality audio, fallback to standard
if (SituationSetAudioPlaybackSampleRate(96000) != SITUATION_SUCCESS) {
    printf("96kHz not supported, using 48kHz\n");
    SituationSetAudioPlaybackSampleRate(48000);
}
```

**Notes:**
- Stops all currently playing sounds
- May fail if hardware doesn't support the rate
- Common rates: 44100, 48000, 96000
- Causes brief audio interruption

---
#### `SituationGetAudioDevices` _(deprecated v2.4.336)_
Legacy playback device enumeration. **Deprecated:** use **`SituationEnumerateAudioDevices`** and free with **`SituationFreeDeviceList`**. Emits a compile-time deprecation warning when included from current headers.
```c
SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);
```
**Usage Example:**
```c
int device_count;
SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
printf("Available Audio Devices:\n");
for (int i = 0; i < device_count; i++) {
    printf("- %s %s\n", devices[i].name, devices[i].is_default ? "(Default)" : "");
}
// Note: The returned array's memory is managed by the library and should not be freed.
```

---
#### `SituationSetAudioDevice`
Sets the active audio playback device by its ID. This should be called before loading any sounds.
```c
SituationError SituationSetAudioDevice(int device_id);
```

---
#### `SituationSetAudioMasterVolume`
Sets the master volume for the entire audio device. See full documentation in the Audio Device Management section above.

```c
SituationError SituationSetAudioMasterVolume(float volume);
```

---
#### `SituationSuspendAudioContext` / `SituationResumeAudioContext`
Suspends or resumes the entire audio context, stopping or restarting all sounds.
```c
SituationError SituationSuspendAudioContext(void);
SituationError SituationResumeAudioContext(void);
```
---
#### Sound Loading and Management
---
#### `SituationLoadSoundFromFile` / `SituationUnloadSound`
Loads a sound from a file (WAV, MP3, OGG, FLAC). The `mode` parameter determines whether to decode fully to RAM (`SITUATION_AUDIO_LOAD_FULL`, `AUTO`) or stream from disk (`SITUATION_AUDIO_LOAD_STREAM`). `SituationUnloadSound` frees the sound's memory.
```c
SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound);
void SituationUnloadSound(SituationSound* sound);
```
**Usage Example:**
```c
// At init:
SituationSound jump_sound;
SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &jump_sound);

// During gameplay:
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayLoadedSound(&jump_sound);
}

// At shutdown:
SituationUnloadSound(&jump_sound);
```
---
#### `SituationLoadSoundFromStream`
Initializes a sound for playback by streaming it from a custom data source. This is highly memory-efficient and the preferred method for long music tracks. You provide callbacks to read and seek in your custom data stream.

```c
SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound);
```

**Parameters:**
- `on_read` - Callback to read audio data from your source
- `on_seek` - Callback to seek to a position in your source
- `user_data` - User data passed to callbacks
- `format` - Audio format specification (sample rate, channels, format)
- `looping` - Whether the sound should loop
- `out_sound` - Pointer to receive the sound handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Custom stream from file
typedef struct {
    FILE* file;
    long file_size;
} FileStream;

// Read callback
size_t FileStreamRead(void* user_data, void* buffer, size_t bytes_to_read) {
    FileStream* stream = (FileStream*)user_data;
    return fread(buffer, 1, bytes_to_read, stream->file);
}

// Seek callback
bool FileStreamSeek(void* user_data, size_t byte_offset) {
    FileStream* stream = (FileStream*)user_data;
    return fseek(stream->file, byte_offset, SEEK_SET) == 0;
}

// Load music from custom stream
FileStream stream = {
    .file = fopen("music/background.ogg", "rb"),
    .file_size = GetFileSize("music/background.ogg")
};

SituationAudioFormat format = {
    .sample_rate = 44100,
    .channels = 2,
    .format = SITUATION_AUDIO_FORMAT_F32
};

SituationSound music;
if (SituationLoadSoundFromStream(FileStreamRead, FileStreamSeek, &stream, &format, true, &music) == SITUATION_SUCCESS) {
    SituationPlayLoadedSound(&music);
}
```

**Advanced Example (Network Stream):**
```c
// Stream audio from network
typedef struct {
    int socket;
    uint8_t buffer[8192];
    size_t buffer_pos;
} NetworkStream;

size_t NetworkStreamRead(void* user_data, void* buffer, size_t bytes) {
    NetworkStream* stream = (NetworkStream*)user_data;
    return recv(stream->socket, buffer, bytes, 0);
}

bool NetworkStreamSeek(void* user_data, size_t offset) {
    // Network streams typically don't support seeking
    return false;
}
```

**Notes:**
- Ideal for music tracks and long audio files
- Uses minimal memory (only buffers small chunks)
- Callbacks are called from audio thread - keep them fast
- Seeking may not be supported by all stream types

---
#### `SituationLoadSoundFromMemory`
Loads a sound from a data buffer already in memory. Useful for embedded audio data or audio downloaded from network.

```c
SituationError SituationLoadSoundFromMemory(const char* file_type, const unsigned char* data, int data_size, bool looping, SituationSound* out_sound);
```

**Parameters:**
- `file_type` - File format extension (e.g., "wav", "ogg", "mp3")
- `data` - Pointer to audio file data in memory
- `data_size` - Size of the data in bytes
- `looping` - Whether the sound should loop
- `out_sound` - Pointer to receive the sound handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load embedded audio data
extern const unsigned char embedded_sound_data[];
extern const int embedded_sound_size;

SituationSound embedded_sound;
if (SituationLoadSoundFromMemory("wav", embedded_sound_data, embedded_sound_size, false, &embedded_sound) == SITUATION_SUCCESS) {
    SituationPlayLoadedSound(&embedded_sound);
}

// Load audio from network
uint8_t* downloaded_audio = DownloadAudio("https://example.com/sound.ogg", &size);
SituationSound network_sound;
SituationLoadSoundFromMemory("ogg", downloaded_audio, size, false, &network_sound);
free(downloaded_audio);  // Can free after loading
```

**Advanced Example (Asset Pack):**
```c
// Load sounds from a packed asset file
typedef struct {
    const char* name;
    size_t offset;
    size_t size;
} AssetEntry;

void LoadSoundsFromPack(const char* pack_file) {
    // Read entire pack into memory
    size_t pack_size;
    uint8_t* pack_data = LoadFile(pack_file, &pack_size);
    
    // Read asset table
    AssetEntry* entries = (AssetEntry*)pack_data;
    int entry_count = *(int*)(pack_data + sizeof(AssetEntry));
    
    // Load each sound
    for (int i = 0; i < entry_count; i++) {
        SituationSound sound;
        const uint8_t* sound_data = pack_data + entries[i].offset;
        
        if (SituationLoadSoundFromMemory("ogg", sound_data, entries[i].size, false, &sound) == SITUATION_SUCCESS) {
            RegisterSound(entries[i].name, sound);
        }
    }
}
```

**Notes:**
- Data must remain valid until sound is loaded (then can be freed)
- File type determines decoder (wav, ogg, mp3, flac)
- Entire file is decoded into memory
- Good for embedded assets or downloaded audio
---
#### Playback Control
---
#### `SituationPlayLoadedSound` / `SituationStopLoadedSound`
Begins or stops playback of a specific loaded sound.
```c
SituationError SituationPlayLoadedSound(SituationSound* sound);
SituationError SituationStopLoadedSound(SituationSound* sound);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayLoadedSound(&jump_sound);
}
```

---
#### `SituationIsSoundPlaying`
Checks if a specific sound is currently playing. Returns `true` if the sound is actively playing, `false` if it has stopped, finished, or was never started.
```c
bool SituationIsSoundPlaying(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound to check

**Returns:**
- `true` if sound is playing
- `false` if sound is stopped or finished

**Usage Example:**
```c
// Only play if not already playing
if (!SituationIsSoundPlaying(&background_music)) {
    SituationPlayLoadedSound(&background_music);
}

// Wait for sound to finish before continuing
SituationPlayLoadedSound(&dialogue_sound);
while (SituationIsSoundPlaying(&dialogue_sound)) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    // Update but don't advance game logic
}

// Stop sound if playing too long
static float play_time = 0.0f;
if (SituationIsSoundPlaying(&alarm_sound)) {
    play_time += SituationGetFrameTime();
    if (play_time > 5.0f) {
        SituationStopLoadedSound(&alarm_sound);
        play_time = 0.0f;
    }
}

// Restart looping sound if it stopped unexpectedly
if (!SituationIsSoundPlaying(&ambient_loop) && should_be_playing) {
    SituationPlayLoadedSound(&ambient_loop);
}
```
**Notes:**
- Returns `false` for one-shot sounds that have finished
- Returns `true` for looping sounds until explicitly stopped
- Useful for preventing overlapping sound effects
- Can be used to detect when a sound has finished

---
#### `SituationSetSoundVolume`
Sets the volume for a specific, individual sound. This is multiplied with the master volume.

```c
SituationError SituationSetSoundVolume(SituationSound* sound, float volume);
```

**Parameters:**
- `sound` - Sound to modify
- `volume` - Volume level (0.0 = silent, 1.0 = full)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set volume before playing
SituationSound explosion;
SituationLoadSoundFromFile("sounds/explosion.wav", SITUATION_AUDIO_LOAD_AUTO, false, &explosion);
SituationSetSoundVolume(&explosion, 0.7f);  // 70% volume
SituationPlayLoadedSound(&explosion);

// Fade out effect
for (float v = 1.0f; v >= 0.0f; v -= 0.01f) {
    SituationSetSoundVolume(&music, v);
    SituationSleep(16);  // ~60 FPS
}

// Distance-based volume
float distance = CalculateDistance(player, sound_source);
float volume = 1.0f / (1.0f + distance * 0.1f);  // Inverse distance
SituationSetSoundVolume(&ambient_sound, volume);

// Category-based volume
SituationSetSoundVolume(&sfx_sound, settings.sfx_volume);
SituationSetSoundVolume(&music_sound, settings.music_volume);
SituationSetSoundVolume(&voice_sound, settings.voice_volume);
```

**Notes:**
- Multiplied with master volume
- Clamped to 0.0-1.0 range
- Affects currently playing sound immediately
- Persists across play/stop cycles

---
#### `SituationSetSoundPan`
Sets the stereo panning for a sound. Controls left/right speaker balance.

```c
SituationError SituationSetSoundPan(SituationSound* sound, float pan);
```

**Parameters:**
- `sound` - Sound to modify
- `pan` - Pan value (-1.0 = full left, 0.0 = center, 1.0 = full right)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Pan sound based on position
float player_x = 400.0f;  // Screen center
float sound_x = 600.0f;   // Right of center
float pan = (sound_x - player_x) / 400.0f;  // -1.0 to 1.0
SituationSetSoundPan(&sound, pan);

// Stereo effect - alternate left/right
static bool left_side = true;
SituationSetSoundPan(&footstep, left_side ? -0.5f : 0.5f);
left_side = !left_side;
SituationPlayLoadedSound(&footstep);

// Panning animation
float time = SituationGetTime();
float pan = sinf(time * 2.0f);  // Oscillate -1.0 to 1.0
SituationSetSoundPan(&ambient, pan);

// Positional audio (simple)
float relative_x = sound_pos.x - listener_pos.x;
float pan = glm_clamp(relative_x / 100.0f, -1.0f, 1.0f);
SituationSetSoundPan(&sound, pan);
```

**Notes:**
- -1.0 = full left speaker
- 0.0 = center (both speakers equal)
- 1.0 = full right speaker
- Clamped to -1.0 to 1.0 range
- Useful for positional audio

---
#### `SituationSetSoundPitch`
Sets the playback pitch for a sound by resampling (`1.0` is normal pitch, `0.5` is one octave lower, `2.0` is one octave higher).
```c
SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);
```
**Usage Example:**
```c
// Make the sound effect's pitch slightly random
float random_pitch = 1.0f + ((rand() % 200) - 100) / 1000.0f; // Range 0.9 to 1.1
SituationSetSoundPitch(&jump_sound, random_pitch);
SituationPlayLoadedSound(&jump_sound);
```

---
#### Querying Sound State
---
#### `SituationIsSoundLooping`
Checks if a sound is set to loop.
```c
bool SituationIsSoundLooping(SituationSound* sound);
```
**Usage Example:**
```c
if (SituationIsSoundLooping(&music)) {
    printf("The music track is set to loop.\n");
}
```

---
#### `SituationGetSoundLength`
Gets the total length of a sound in seconds.
```c
double SituationGetSoundLength(SituationSound* sound);
```
**Usage Example:**
```c
double length = SituationGetSoundLength(&music);
printf("Music track length: %.2f seconds\n", length);
```

---
#### `SituationGetSoundCursor`
Gets the current playback position of a sound in seconds.
```c
double SituationGetSoundCursor(SituationSound* sound);
```
**Usage Example:**
```c
double position = SituationGetSoundCursor(&music);
printf("Music is currently at %.2f seconds\n", position);
```

---
#### `SituationSetSoundCursor`
Sets the current playback position of a sound in seconds.
```c
void SituationSetSoundCursor(SituationSound* sound, double seconds);
```
**Usage Example:**
```c
// Skip 30 seconds into the music track
SituationSetSoundCursor(&music, 30.0);
```
---
#### Effects and Custom Processing
---
#### `SituationSetSoundFilter`
Applies a low-pass or high-pass filter to a sound's effects chain.
```c
SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);
```
**Usage Example:**
```c
// To simulate sound coming from another room, apply a low-pass filter.
SituationSetSoundFilter(&music, SIT_FILTER_LOW_PASS, 800.0f, 1.0f); // Cut off frequencies above 800 Hz
```

---
#### `SituationSetSoundReverb`
Applies a reverb effect to a sound.
```c
SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);
```

---
#### `SituationAttachAudioProcessor`
Attaches a custom DSP (Digital Signal Processing) processor to a sound's effect chain for real-time audio processing. Useful for visualization, custom effects, or audio analysis.

```c
SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData);
```

**Parameters:**
- `sound` - Pointer to the sound to attach processor to
- `processor` - Callback function that processes audio samples
- `userData` - User data passed to the callback

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Callback Signature:**
```c
typedef void (*SituationAudioProcessorCallback)(void* buffer, unsigned int frames, void* userData);
```

**Usage Example:**
```c
// Audio visualization processor
void VisualizationProcessor(void* buffer, unsigned int frames, void* userData) {
    float* samples = (float*)buffer;
    float* peak_level = (float*)userData;
    
    // Calculate peak level for visualization
    float max_sample = 0.0f;
    for (unsigned int i = 0; i < frames * 2; i++) {  // Stereo
        float abs_sample = fabsf(samples[i]);
        if (abs_sample > max_sample) {
            max_sample = abs_sample;
        }
    }
    *peak_level = max_sample;
}

// Attach to music for waveform display
float music_peak = 0.0f;
SituationAttachAudioProcessor(&background_music, VisualizationProcessor, &music_peak);

// In render loop, use music_peak for visualization
DrawWaveform(music_peak);
```

**Advanced Example (Custom Distortion Effect):**
```c
// Distortion processor
void DistortionProcessor(void* buffer, unsigned int frames, void* userData) {
    float* samples = (float*)buffer;
    float drive = *(float*)userData;
    
    for (unsigned int i = 0; i < frames * 2; i++) {
        // Apply soft clipping distortion
        float sample = samples[i] * drive;
        samples[i] = tanhf(sample);  // Soft clip
    }
}

float distortion_drive = 2.0f;
SituationAttachAudioProcessor(&guitar_sound, DistortionProcessor, &distortion_drive);
```

**Notes:**
- Processor is called from audio thread - keep it fast!
- Avoid allocations, locks, or heavy computations
- Multiple processors can be attached to the same sound
- Processors are called in attachment order

---
#### `SituationDetachAudioProcessor`
Detaches a custom DSP processor from a sound's effect chain.

```c
SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor);
```

**Parameters:**
- `sound` - Pointer to the sound
- `processor` - The processor callback to detach

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Attach processor
SituationAttachAudioProcessor(&music, VisualizationProcessor, &peak_data);

// Later, detach when no longer needed
SituationDetachAudioProcessor(&music, VisualizationProcessor);

// Toggle effect on/off
if (effect_enabled) {
    SituationAttachAudioProcessor(&sound, EffectProcessor, &effect_params);
} else {
    SituationDetachAudioProcessor(&sound, EffectProcessor);
}
```

**Notes:**
- Must pass the same function pointer used in `AttachAudioProcessor()`
- Safe to call even if processor is not attached
- Detaching during playback is safe

---
#### `SituationSetSoundEcho`
Applies an echo/delay effect to a sound with configurable delay time, feedback, and wet/dry mix.

```c
SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);
```

**Parameters:**
- `sound` - Pointer to the sound
- `enabled` - Whether echo effect is enabled
- `delay_sec` - Delay time in seconds (e.g., 0.3 for 300ms)
- `feedback` - Feedback amount (0.0 to 1.0, controls number of repeats)
- `wet_mix` - Wet/dry mix (0.0 = dry only, 1.0 = wet only, 0.5 = 50/50)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Apply subtle echo to voice
SituationSetSoundEcho(&voice_sound, true, 0.15f, 0.3f, 0.25f);

// Cave/canyon echo effect
SituationSetSoundEcho(&footstep_sound, true, 0.5f, 0.6f, 0.4f);

// Disable echo
SituationSetSoundEcho(&sound, false, 0.0f, 0.0f, 0.0f);

// Dynamic echo based on environment
void UpdateEnvironmentEcho(SituationSound* sound, EnvironmentType env) {
    switch (env) {
        case ENV_CAVE:
            SituationSetSoundEcho(sound, true, 0.8f, 0.7f, 0.5f);
            break;
        case ENV_CANYON:
            SituationSetSoundEcho(sound, true, 1.2f, 0.5f, 0.4f);
            break;
        case ENV_ROOM:
            SituationSetSoundEcho(sound, true, 0.1f, 0.2f, 0.15f);
            break;
        case ENV_OUTDOOR:
            SituationSetSoundEcho(sound, false, 0.0f, 0.0f, 0.0f);
            break;
    }
}
```

**Parameter Guidelines:**
- **Delay:** 0.05-0.2s (slapback), 0.2-0.5s (echo), 0.5-2.0s (long delay)
- **Feedback:** 0.0-0.3 (single repeat), 0.3-0.6 (multiple repeats), 0.6-0.9 (many repeats)
- **Wet Mix:** 0.1-0.3 (subtle), 0.3-0.5 (noticeable), 0.5-0.8 (prominent)

**Notes:**
- Echo effect is applied in real-time during playback
- Higher feedback values create more repeats
- Wet mix controls the balance between original and echoed signal
- Can be changed dynamically during playback

---
#### `SituationGetAudioPlaybackSampleRate`
Gets the sample rate of the current audio device. See full documentation in the Audio Device Management section above.

```c
int SituationGetAudioPlaybackSampleRate(void);
```

---
#### `SituationSetAudioPlaybackSampleRate`
Re-initializes the audio device with a new sample rate. See full documentation in the Audio Device Management section above.

```c
SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);
```

---
#### `SituationGetAudioMasterVolume`
Gets the current master volume for the audio device. See full documentation in the Audio Device Management section above.

```c
float SituationGetAudioMasterVolume(void);
```

---
#### `SituationIsAudioDevicePlaying`
Checks if the audio device is currently playing any sounds. See full documentation in the Audio Device Management section above.

```c
bool SituationIsAudioDevicePlaying(void);
```

---
#### `SituationPauseAudioDevice`
Pauses audio playback on the device.
```c
SituationError SituationPauseAudioDevice(void);
```

---
#### `SituationResumeAudioDevice`
Resumes audio playback on the device.
```c
SituationError SituationResumeAudioDevice(void);
```

---
#### `SituationStopLoadedSound`
Stops a specific sound that is currently playing. If the sound is not playing, this function has no effect. The sound remains loaded in memory and can be played again.
```c
SituationError SituationStopLoadedSound(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound to stop

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if sound is invalid

**Usage Example:**
```c
// Stop background music when entering menu
if (game_state == STATE_MENU) {
    SituationStopLoadedSound(&background_music);
}

// Stop sound effect on collision
void on_collision(void) {
    if (SituationIsSoundPlaying(&explosion_sound)) {
        SituationStopLoadedSound(&explosion_sound);
    }
    // Play new explosion sound
    SituationPlayLoadedSound(&explosion_sound);
}

// Stop looping ambient sound
if (player_left_area) {
    SituationStopLoadedSound(&ambient_loop);
}
```
**Notes:**
- Sound remains loaded and can be replayed
- Does nothing if sound is not currently playing
- For looping sounds, stops the loop immediately
- Use `SituationUnloadSound()` to free memory after stopping

---
#### `SituationStopAllLoadedSounds`
Stops all currently playing sounds immediately. This is useful for scene transitions, pause menus, or emergency audio cutoff. Sounds remain loaded in memory.
```c
SituationError SituationStopAllLoadedSounds(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Stop all audio when pausing game
void pause_game(void) {
    SituationStopAllLoadedSounds();
    game_paused = true;
}

// Stop all audio on scene transition
void load_new_scene(const char* scene_name) {
    SituationStopAllLoadedSounds();
    unload_current_scene();
    load_scene(scene_name);
}

// Emergency audio cutoff
if (SituationIsKeyPressed(SIT_KEY_M)) {  // Mute key
    SituationStopAllLoadedSounds();
    audio_muted = true;
}

// Stop all before cleanup
void shutdown_audio_system(void) {
    SituationStopAllLoadedSounds();
    // Now safe to unload all sounds
    for (int i = 0; i < sound_count; i++) {
        SituationUnloadSound(&sounds[i]);
    }
}
```
**Notes:**
- Stops ALL sounds, including music and effects
- Sounds remain loaded in memory
- Does not affect master volume setting
- Consider fading out instead for smoother transitions

---
#### `SituationSoundCopy`
Creates a new sound by making a deep copy of the raw PCM audio data from a source sound. Useful for creating variations or backups.

```c
SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination);
```

**Parameters:**
- `source` - The source sound to copy from
- `out_destination` - Pointer to receive the new sound copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a copy for pitch variation
SituationSound original_explosion;
SituationLoadAudio("explosion.wav", false, &original_explosion);

// Create multiple variations with different pitches
SituationSound explosion_low, explosion_high;
SituationSoundCopy(&original_explosion, &explosion_low);
SituationSoundCopy(&original_explosion, &explosion_high);

SituationSetSoundPitch(&explosion_low, 0.8f);
SituationSetSoundPitch(&explosion_high, 1.2f);

// Play random variation
int variation = rand() % 3;
if (variation == 0) SituationPlayLoadedSound(&original_explosion);
else if (variation == 1) SituationPlayLoadedSound(&explosion_low);
else SituationPlayLoadedSound(&explosion_high);
```

**Advanced Example (Sound Pool):**
```c
// Create a pool of identical sounds for overlapping playback
#define SOUND_POOL_SIZE 8
SituationSound laser_pool[SOUND_POOL_SIZE];

SituationSound laser_original;
SituationLoadAudio("laser.wav", false, &laser_original);

for (int i = 0; i < SOUND_POOL_SIZE; i++) {
    SituationSoundCopy(&laser_original, &laser_pool[i]);
}

// Play from pool (allows overlapping)
int pool_index = 0;
void PlayLaser() {
    SituationPlayLoadedSound(&laser_pool[pool_index]);
    pool_index = (pool_index + 1) % SOUND_POOL_SIZE;
}
```

**Notes:**
- Creates a complete copy of PCM data
- Both sounds are independent after copying
- Remember to unload both original and copy
- Useful for creating sound variations without reloading

---
#### `SituationSoundCrop`
Crops a sound's PCM data in-place to a specific frame range. This permanently modifies the sound data.

```c
SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame);
```

**Parameters:**
- `sound` - Pointer to the sound to crop (modified in-place)
- `initFrame` - Starting frame (inclusive)
- `finalFrame` - Ending frame (exclusive)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load a long audio file
SituationSound long_audio;
SituationLoadAudio("long_recording.wav", false, &long_audio);

// Extract just the intro (first 2 seconds at 44100 Hz)
SituationSound intro;
SituationSoundCopy(&long_audio, &intro);
SituationSoundCrop(&intro, 0, 44100 * 2);  // 0 to 2 seconds

// Extract middle section (2 to 5 seconds)
SituationSound middle;
SituationSoundCopy(&long_audio, &middle);
SituationSoundCrop(&middle, 44100 * 2, 44100 * 5);

// Extract outro (last 3 seconds)
uint64_t total_frames = GetSoundFrameCount(&long_audio);
SituationSound outro;
SituationSoundCopy(&long_audio, &outro);
SituationSoundCrop(&outro, total_frames - (44100 * 3), total_frames);
```

**Advanced Example (Remove Silence):**
```c
// Trim silence from beginning and end
void TrimSilence(SituationSound* sound, float threshold) {
    float* samples = GetSoundSamples(sound);
    uint64_t frame_count = GetSoundFrameCount(sound);
    
    // Find first non-silent frame
    uint64_t start_frame = 0;
    for (uint64_t i = 0; i < frame_count; i++) {
        if (fabsf(samples[i * 2]) > threshold || fabsf(samples[i * 2 + 1]) > threshold) {
            start_frame = i;
            break;
        }
    }
    
    // Find last non-silent frame
    uint64_t end_frame = frame_count;
    for (uint64_t i = frame_count - 1; i > start_frame; i--) {
        if (fabsf(samples[i * 2]) > threshold || fabsf(samples[i * 2 + 1]) > threshold) {
            end_frame = i + 1;
            break;
        }
    }
    
    // Crop to non-silent region
    SituationSoundCrop(sound, start_frame, end_frame);
}
```

**Notes:**
- Modifies the sound in-place - original data is lost
- Frame numbers are in audio frames (not bytes or samples)
- For stereo, one frame = 2 samples (left + right)
- Use `SituationSoundCopy()` first if you need to preserve original

---
#### `SituationSoundExportAsWav`
Exports a sound's raw PCM data to a WAV file. Useful for saving procedurally generated audio or processed sounds.

```c
SituationError SituationSoundExportAsWav(const SituationSound* sound, const char* fileName);
```

**Parameters:**
- `sound` - The sound to export
- `fileName` - Output file path (e.g., "output.wav")

**Returns:** `SITUATION_SUCCESS` on success, or an error code (`SITUATION_ERROR_INVALID_PARAM`, `SITUATION_ERROR_FILE_WRITE_FAILED`, `SITUATION_ERROR_AUDIO_INVALID_OPERATION`)

**Usage Example:**
```c
// Export processed audio
SituationSound voice;
SituationLoadAudio("voice.wav", false, &voice);

// Apply effects
SituationSetSoundPitch(&voice, 0.8f);
SituationSetSoundEcho(&voice, true, 0.3f, 0.4f, 0.3f);

// Export the processed version
if (SituationSoundExportAsWav(&voice, "voice_processed.wav") == SITUATION_SUCCESS) {
    printf("Exported processed audio\n");
}
```

**Advanced Example (Procedural Audio Generation):**
```c
// Generate a sine wave tone
SituationSound GenerateTone(float frequency, float duration, int sample_rate) {
    int frame_count = (int)(duration * sample_rate);
    float* samples = malloc(frame_count * 2 * sizeof(float));  // Stereo
    
    for (int i = 0; i < frame_count; i++) {
        float t = (float)i / sample_rate;
        float sample = sinf(2.0f * M_PI * frequency * t) * 0.5f;
        samples[i * 2] = sample;      // Left
        samples[i * 2 + 1] = sample;  // Right
    }
    
    SituationSound tone = CreateSoundFromSamples(samples, frame_count, sample_rate, 2);
    free(samples);
    return tone;
}

// Generate and export a 440Hz A note
SituationSound a_note = GenerateTone(440.0f, 2.0f, 44100);
SituationSoundExportAsWav(&a_note, "a_note.wav");

// Generate chord
SituationSound c_note = GenerateTone(261.63f, 2.0f, 44100);
SituationSound e_note = GenerateTone(329.63f, 2.0f, 44100);
SituationSound g_note = GenerateTone(392.00f, 2.0f, 44100);

// Mix and export
SituationSound chord = MixSounds(&c_note, &e_note, &g_note);
SituationSoundExportAsWav(&chord, "c_major_chord.wav");
```

**Notes:**
- Exports in standard WAV format (PCM)
- Preserves sample rate and channel count
- Useful for saving procedurally generated audio
- Can export sounds after applying effects
- File can be reloaded with `SituationLoadAudio()`

---
#### `SituationGetSoundVolume`
Gets the current volume level of a specific sound. Returns a value between 0.0 (silent) and 1.0 (full volume).
```c
float SituationGetSoundVolume(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Volume level (0.0 to 1.0)

**Usage Example:**
```c
// Check current volume before adjusting
float current_volume = SituationGetSoundVolume(&music);
if (current_volume > 0.5f) {
    SituationSetSoundVolume(&music, 0.3f);  // Lower it
}

// Fade out effect
void fade_out_sound(SituationSound* sound, float duration) {
    float start_volume = SituationGetSoundVolume(sound);
    float elapsed = 0.0f;
    
    while (elapsed < duration) {
        float t = elapsed / duration;
        float volume = start_volume * (1.0f - t);
        SituationSetSoundVolume(sound, volume);
        elapsed += SituationGetFrameTime();
    }
    SituationStopLoadedSound(sound);
}

// Display volume in UI
float volume = SituationGetSoundVolume(&sfx_sound);
printf("SFX Volume: %.0f%%\n", volume * 100.0f);
```

---
#### `SituationGetSoundPan`
Gets the current stereo panning of a sound. Returns -1.0 (full left), 0.0 (center), or 1.0 (full right).
```c
float SituationGetSoundPan(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Pan value (-1.0 to 1.0)

**Usage Example:**
```c
// Check current pan position
float pan = SituationGetSoundPan(&footstep_sound);
printf("Sound is panned: %s\n", pan < 0 ? "left" : pan > 0 ? "right" : "center");

// Gradually pan sound based on object position
float object_x = get_object_screen_x();
float screen_center = SituationGetScreenWidth() / 2.0f;
float pan = (object_x - screen_center) / screen_center;  // -1 to 1
SituationSetSoundPan(&sound, pan);

// Reset pan to center
if (SituationIsKeyPressed(SIT_KEY_C)) {
    SituationSetSoundPan(&sound, 0.0f);
}
```

---
#### `SituationGetSoundPitch`
Gets the current pitch multiplier of a sound. Returns 1.0 for normal pitch, < 1.0 for lower pitch, > 1.0 for higher pitch.
```c
float SituationGetSoundPitch(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Pitch multiplier (typically 0.5 to 2.0)

**Usage Example:**
```c
// Check current pitch
float pitch = SituationGetSoundPitch(&engine_sound);
printf("Engine pitch: %.2fx\n", pitch);

// Gradually increase pitch based on speed
float speed = get_vehicle_speed();
float target_pitch = 0.8f + (speed / max_speed) * 1.2f;  // 0.8 to 2.0
float current_pitch = SituationGetSoundPitch(&engine_sound);
float new_pitch = lerp(current_pitch, target_pitch, 0.1f);
SituationSetSoundPitch(&engine_sound, new_pitch);

// Reset to normal pitch
if (SituationIsKeyPressed(SIT_KEY_R)) {
    SituationSetSoundPitch(&sound, 1.0f);
}
```

---
#### `SituationSetSoundEcho`
Applies an echo/delay effect to a sound with configurable parameters. Creates repeating copies of the sound that decay over time.

```c
SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);
```

**Parameters:**
- `sound` - Sound to apply echo to
- `enabled` - Enable/disable the echo effect
- `delay_sec` - Delay time in seconds between echoes (e.g., 0.3)
- `feedback` - Amount of echo feedback (0.0-1.0, higher = more repeats)
- `wet_mix` - Dry/wet mix (0.0 = original only, 1.0 = echo only)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Cave/canyon echo effect
SituationSound voice;
SituationLoadSoundFromFile("sounds/voice.wav", SITUATION_AUDIO_LOAD_AUTO, false, &voice);
SituationSetSoundEcho(&voice, true, 0.4f, 0.6f, 0.5f);
SituationPlayLoadedSound(&voice);

// Subtle echo for ambience
SituationSetSoundEcho(&ambient, true, 0.2f, 0.3f, 0.2f);

// Disable echo
SituationSetSoundEcho(&sound, false, 0.0f, 0.0f, 0.0f);

// Dynamic echo based on environment
if (player_in_cave) {
    SituationSetSoundEcho(&footstep, true, 0.5f, 0.7f, 0.6f);  // Strong echo
} else if (player_in_room) {
    SituationSetSoundEcho(&footstep, true, 0.1f, 0.3f, 0.2f);  // Subtle echo
} else {
    SituationSetSoundEcho(&footstep, false, 0.0f, 0.0f, 0.0f);  // No echo (outdoors)
}

// Rhythmic delay effect
SituationSetSoundEcho(&drum, true, 0.25f, 0.5f, 0.4f);  // Quarter note delay at 120 BPM
```

**Notes:**
- delay_sec: Time between echoes (0.1-2.0 typical)
- feedback: Controls decay (0.0 = one echo, 0.9 = many echoes)
- wet_mix: Balance between original and effect (0.5 = 50/50)
- Higher feedback values can create infinite echo
- Use sparingly for performance

---
#### `SituationUnloadSound`
Unloads a sound and frees its resources. Always call this when done with a sound to prevent memory leaks.

```c
void SituationUnloadSound(SituationSound* sound);
```

**Parameters:**
- `sound` - Pointer to sound to unload

**Usage Example:**
```c
// Load and use sound
SituationSound jump_sound;
SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &jump_sound);

// Use the sound
SituationPlayLoadedSound(&jump_sound);

// Cleanup when done
SituationUnloadSound(&jump_sound);

// Proper resource management
void LoadGameSounds() {
    SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &game_sounds.jump);
    SituationLoadSoundFromFile("sounds/shoot.wav", SITUATION_AUDIO_LOAD_AUTO, false, &game_sounds.shoot);
    SituationLoadSoundFromFile("sounds/music.ogg", SITUATION_AUDIO_LOAD_STREAM, true, &game_sounds.music);
}

void UnloadGameSounds() {
    SituationUnloadSound(&game_sounds.jump);
    SituationUnloadSound(&game_sounds.shoot);
    SituationUnloadSound(&game_sounds.music);
}

// Safe to call multiple times
SituationSound sound;
SituationLoadSoundFromFile("test.wav", SITUATION_AUDIO_LOAD_AUTO, false, &sound);
SituationUnloadSound(&sound);
SituationUnloadSound(&sound);  // Safe, but unnecessary

// Array of sounds
#define MAX_SOUNDS 10
SituationSound sounds[MAX_SOUNDS];
int sound_count = 0;

// Load sounds
for (int i = 0; i < sound_count; i++) {
    SituationLoadSoundFromFile(sound_files[i], SITUATION_AUDIO_LOAD_AUTO, false, &sounds[i]);
}

// Cleanup all
for (int i = 0; i < sound_count; i++) {
    SituationUnloadSound(&sounds[i]);
}
```

**Notes:**
- Stops sound if currently playing
- Frees audio data from memory
- Safe to call multiple times (idempotent)
- Always call before application exit
- Use handle-based API (`SituationUnloadAudio`) for simpler management

---
#### Audio Handle API
These functions operate on the new `SituationSoundHandle` system, which simplifies audio management by using opaque handles instead of structs. This is the recommended API for new code.

---
#### `SituationLoadAudio`
Loads an audio file and returns an opaque handle for simplified audio management. This is the modern, handle-based API that's easier to use than the struct-based API.
```c
SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping);
```
**Parameters:**
- `file_path`: Path to audio file (WAV, OGG, MP3, FLAC)
- `mode`: `SITUATION_AUDIO_LOAD_AUTO`, `SITUATION_AUDIO_LOAD_MEMORY`, or `SITUATION_AUDIO_LOAD_STREAM`
- `looping`: `true` for looping playback, `false` for one-shot

**Returns:**
- Valid handle on success, invalid handle (0) on failure

**Usage Example:**
```c
// Load background music (streaming for memory efficiency)
SituationSoundHandle music = SituationLoadAudio(
    "assets/music/background.ogg",
    SITUATION_AUDIO_LOAD_STREAM,
    true  // Loop
);

// Load sound effect (in memory for low latency)
SituationSoundHandle jump_sfx = SituationLoadAudio(
    "assets/sfx/jump.wav",
    SITUATION_AUDIO_LOAD_MEMORY,
    false  // One-shot
);

// Auto mode (library decides based on file size)
SituationSoundHandle ambient = SituationLoadAudio(
    "assets/ambient.ogg",
    SITUATION_AUDIO_LOAD_AUTO,
    true
);

// Check if load succeeded
if (music == 0) {
    printf("Failed to load music!\n");
}
```
**Notes:**
- Use `SITUATION_AUDIO_LOAD_STREAM` for long music tracks
- Use `SITUATION_AUDIO_LOAD_MEMORY` for short sound effects
- Use `SITUATION_AUDIO_LOAD_AUTO` to let library decide
- Always check return value for 0 (invalid handle)

---
#### `SituationPlayAudio`
Plays audio using its handle. If the audio is already playing, it restarts from the beginning. For looping audio, this starts the loop.
```c
SituationError SituationPlayAudio(SituationSoundHandle handle);
```
**Parameters:**
- `handle`: Audio handle from `SituationLoadAudio()`

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Play background music
SituationPlayAudio(background_music);

// Play sound effect on event
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayAudio(jump_sound);
}

// Play with volume control
SituationSetAudioVolume(explosion_sfx, 0.7f);
SituationPlayAudio(explosion_sfx);

// Restart currently playing sound
if (player_died) {
    SituationPlayAudio(death_sound);  // Restarts if already playing
}

// Play multiple instances (load multiple times)
SituationSoundHandle shot1 = SituationLoadAudio("shot.wav", SITUATION_AUDIO_LOAD_MEMORY, false);
SituationSoundHandle shot2 = SituationLoadAudio("shot.wav", SITUATION_AUDIO_LOAD_MEMORY, false);
SituationPlayAudio(shot1);
SituationPlayAudio(shot2);  // Plays simultaneously
```
**Notes:**
- Restarts sound if already playing
- For simultaneous playback, load the same file multiple times
- Looping sounds will loop until stopped
- No need to check if sound is playing first

---
#### `SituationUnloadAudio`
Unloads audio and frees its resources. The handle becomes invalid after this call. Always unload audio when no longer needed to free memory.
```c
void SituationUnloadAudio(SituationSoundHandle handle);
```
**Parameters:**
- `handle`: Audio handle to unload

**Usage Example:**
```c
// Unload when done
SituationUnloadAudio(menu_music);

// Unload all audio on scene change
void cleanup_scene_audio(void) {
    for (int i = 0; i < audio_count; i++) {
        SituationUnloadAudio(audio_handles[i]);
    }
    audio_count = 0;
}

// Unload at shutdown
void shutdown(void) {
    SituationUnloadAudio(background_music);
    SituationUnloadAudio(ambient_sound);
    for (int i = 0; i < sfx_count; i++) {
        SituationUnloadAudio(sfx_handles[i]);
    }
    SituationShutdown();
}
```
**Notes:**
- Always unload audio when done to free memory
- Automatically stops audio if playing
- Handle becomes invalid after unload
- Safe to call multiple times (no-op if already unloaded)

---
#### `SituationSetAudioVolume`
Sets the volume for an audio handle. Volume range is 0.0 (silent) to 1.0 (full volume). Values outside this range are clamped.
```c
SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume);
```
**Parameters:**
- `handle`: Audio handle
- `volume`: Volume level (0.0 to 1.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Set music volume from settings
SituationSetAudioVolume(music, user_settings.music_volume);

// Lower volume for background ambience
SituationSetAudioVolume(ambient, 0.3f);

// Fade in effect
void fade_in(SituationSoundHandle handle, float duration) {
    SituationSetAudioVolume(handle, 0.0f);
    SituationPlayAudio(handle);
    
    float elapsed = 0.0f;
    while (elapsed < duration) {
        float t = elapsed / duration;
        SituationSetAudioVolume(handle, t);
        elapsed += SituationGetFrameTime();
    }
}

// Dynamic volume based on distance
float distance = calculate_distance_to_source();
float volume = 1.0f / (1.0f + distance * 0.1f);  // Inverse distance
SituationSetAudioVolume(sound_handle, volume);
```
**Notes:**
- Values are clamped to 0.0-1.0 range
- Can be set before or during playback
- Combines with master volume setting
- Use for fade effects, distance attenuation, mixing

---
#### `SituationSetAudioPan`
Sets the stereo panning for an audio handle. Pan range is -1.0 (full left) to 1.0 (full right), with 0.0 being center.
```c
SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan);
```
**Parameters:**
- `handle`: Audio handle
- `pan`: Pan position (-1.0 to 1.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Pan sound based on object position
float object_x = enemy.position.x;
float camera_x = camera.position.x;
float screen_width = SituationGetScreenWidth();
float relative_x = (object_x - camera_x) / (screen_width / 2.0f);
float pan = clamp(relative_x, -1.0f, 1.0f);
SituationSetAudioPan(enemy_sound, pan);

// Alternate between left and right speakers
static float pan_time = 0.0f;
pan_time += SituationGetFrameTime();
float pan = sinf(pan_time * 2.0f);  // Oscillate -1 to 1
SituationSetAudioPan(siren_sound, pan);

// Hard pan to left or right
SituationSetAudioPan(left_engine, -1.0f);   // Full left
SituationSetAudioPan(right_engine, 1.0f);   // Full right
SituationSetAudioPan(center_voice, 0.0f);   // Center
```
**Notes:**
- Values are clamped to -1.0 to 1.0 range
- 0.0 is center (equal in both speakers)
- Useful for spatial audio in 2D games
- Can create stereo effects and positional audio

---
#### `SituationSetAudioPitch`
Sets the pitch multiplier for an audio handle. Pitch of 1.0 is normal, < 1.0 is lower/slower, > 1.0 is higher/faster. Typical range is 0.5 to 2.0.
```c
SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch);
```
**Parameters:**
- `handle`: Audio handle
- `pitch`: Pitch multiplier (typically 0.5 to 2.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Engine sound based on RPM
float rpm = engine.get_rpm();
float pitch = 0.5f + (rpm / max_rpm) * 1.5f;  // 0.5 to 2.0
SituationSetAudioPitch(engine_sound, pitch);

// Slow motion effect
if (game_in_slow_motion) {
    SituationSetAudioPitch(all_sounds, 0.7f);  // Slow and deep
} else {
    SituationSetAudioPitch(all_sounds, 1.0f);  // Normal
}

// Random pitch variation for variety
float random_pitch = 0.9f + (rand() / (float)RAND_MAX) * 0.2f;  // 0.9 to 1.1
SituationSetAudioPitch(footstep_sound, random_pitch);

// Musical intervals (semitones)
float semitones_up = 7;  // Perfect fifth
float pitch = powf(2.0f, semitones_up / 12.0f);
SituationSetAudioPitch(note_sound, pitch);
```
**Notes:**
- Also affects playback speed (higher pitch = faster)
- Typical range is 0.5 to 2.0 for natural sounds
- Use for engine sounds, slow-motion, musical effects
- Random variation adds realism to repeated sounds

---
#### `SituationPauseAudioDevice`
Pauses the entire audio device, stopping all audio playback. This is more efficient than stopping individual sounds when you need to pause everything (e.g., game pause menu).
```c
SituationError SituationPauseAudioDevice(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Pause all audio when game is paused
void pause_game(void) {
    SituationPauseAudioDevice();
    game_paused = true;
}

// Pause audio when window loses focus
void on_focus_changed(int focused, void* user_data) {
    if (!focused) {
        SituationPauseAudioDevice();
    } else {
        SituationResumeAudioDevice();
    }
}
SituationSetFocusCallback(on_focus_changed, NULL);

// Pause during loading screen
void show_loading_screen(void) {
    SituationPauseAudioDevice();
    load_assets();
    SituationResumeAudioDevice();
}
```
**Notes:**
- Pauses ALL audio, not individual sounds
- More efficient than stopping each sound individually
- Audio resumes from where it paused
- Use `SituationResumeAudioDevice()` to continue playback

---
#### `SituationResumeAudioDevice`
Resumes audio playback on the device after it was paused with `SituationPauseAudioDevice()`. All sounds continue from where they were paused.
```c
SituationError SituationResumeAudioDevice(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Resume audio when game is unpaused
void unpause_game(void) {
    SituationResumeAudioDevice();
    game_paused = false;
}

// Resume when window regains focus
void on_focus_changed(int focused, void* user_data) {
    if (focused) {
        SituationResumeAudioDevice();
    } else {
        SituationPauseAudioDevice();
    }
}

// Resume after loading
void finish_loading(void) {
    hide_loading_screen();
    SituationResumeAudioDevice();
}
```
**Notes:**
- Resumes ALL paused audio
- Sounds continue from where they were paused
- No effect if audio is not paused
- Pair with `SituationPauseAudioDevice()`

---
#### `SituationSetAudioDevice`
Sets the active audio playback device by its ID and format.
```c
SITAPI SituationError SituationSetAudioDevice(int situation_internal_id, const SituationAudioFormat* format);
```
**Usage Example:**
```c
int device_count;
SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
if (device_count > 0) {
    SituationAudioFormat format = { .channels = 2, .sample_rate = 44100, .bit_depth = 16 };
    SituationSetAudioDevice(devices[0].internal_id, &format);
}
```

---

#### `SituationSetSoundReverb`
Applies a reverb effect to a sound.
```c
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);
```
**Usage Example:**
```c
SituationSound my_sound;
if (SituationLoadSoundFromFile("sounds/footstep.wav", SITUATION_AUDIO_LOAD_AUTO, false, &my_sound) == SITUATION_SUCCESS) {
    // Apply a reverb to simulate a large room
    SituationSetSoundReverb(&my_sound, true, 0.8f, 0.5f, 0.6f, 0.4f);
    SituationPlayLoadedSound(&my_sound);
}
```

---
#### Audio Capture
---
#### `SituationStartAudioCapture`
Start capturing audio input with default format. Initializes and starts capturing audio from the default microphone or recording device using the device's native sample rate and channel count. The captured audio data is delivered via a callback that you provide.
```c
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback on_capture, void* user_data);
```
-   `on_capture`: A pointer to a function that will be called whenever a new buffer of audio data is available. The callback receives the raw audio buffer, the number of frames, and the user data pointer.
-   `user_data`: A custom pointer that will be passed to your `on_capture` callback.
**Usage Example:**
```c
// Define a callback to process the incoming audio data.
void MyAudioCaptureCallback(const float* frames, int frame_count, void* user_data) {
    // 'frames' is an interleaved buffer of 32-bit float samples.
    // For stereo, it would be [L, R, L, R, ...].
    printf("Captured %d audio frames.\n", frame_count);
    // You could write this data to a file, perform FFT, or visualize it.
}

// In your initialization code:
if (SituationStartAudioCapture(MyAudioCaptureCallback, NULL) != SIT_SUCCESS) {
    fprintf(stderr, "Failed to start audio capture: %s\n", SituationGetLastErrorMsg());
}
```

---
#### `SituationStartAudioCaptureEx`
Start capturing with explicit sample rate and channel count. This is the extended version of `SituationStartAudioCapture`, which uses the device's native settings (0, 0) by default. Use this if your application requires a specific format (e.g. 44100Hz Mono for FFT analysis) regardless of the hardware default.
```c
SITAPI SituationError SituationStartAudioCaptureEx(SituationAudioCaptureCallback callback, void* user_data, uint32_t sample_rate, uint32_t channels);
```
-   `callback`: The function to call when data is available.
-   `user_data`: Custom pointer passed to the callback.
-   `sample_rate`: The desired sample rate in Hz (e.g., 44100, 48000). Pass 0 to use the device's native rate.
-   `channels`: The desired number of channels (e.g., 1 for Mono, 2 for Stereo). Pass 0 to use the device's native channel count.

**Usage Example:**
```c
// Request 48kHz Stereo capture explicitly
if (SituationStartAudioCaptureEx(MyAudioCaptureCallback, NULL, 48000, 2) != SIT_SUCCESS) {
    // Handle error (e.g. device doesn't support format)
}
```

---
#### `SituationStopAudioCapture`
Stops the audio capture stream and releases the input device.
```c
SITAPI void SituationStopAudioCapture(void);
```
**Usage Example:**
```c
// When the user clicks a "Stop Recording" button.
SituationStopAudioCapture();
printf("Audio capture stopped.\n");
```

---
#### `SituationSetAudioOutputMonitor`
Sets a callback to receive the final mixed audio output samples for visualization purposes (VU meters, FFT spectrum analyzers, oscilloscopes, etc.). Pass `NULL` to disable monitoring.
```c
SITAPI void SituationSetAudioOutputMonitor(void (*callback)(const float* samples, uint32_t frame_count, void* user_data), void* user_data);
```
-   `callback`: A function pointer invoked with the mixed output buffer each time a block is processed. Pass `NULL` to disable.
-   `user_data`: Custom pointer passed through to your callback.

**Threading:** The callback runs on the **Audio Thread**. Keep processing minimal and lock-free.

**Usage Example:**
```c
// Monitor output for a VU meter
void MyOutputMonitor(const float* samples, uint32_t frame_count, void* user_data) {
    // 'samples' is interleaved stereo float data [L, R, L, R, ...]
    // Compute RMS or peak levels here for visualization.
}

SituationSetAudioOutputMonitor(MyOutputMonitor, &my_vu_state);

// To disable monitoring:
SituationSetAudioOutputMonitor(NULL, NULL);
```

---
#### `SituationGetMasterOutputMeter`
Reads **approximate peak** (max absolute sample in the block) and **RMS** for the **last completed** playback callback mix (**final stereo buffer** after graph + latent voices + tones). Updated once per audio period; safe to poll from the **main thread** or UI (**relaxed atomic** loads internally).

```c
SITAPI void SituationGetMasterOutputMeter(float* out_peak, float* out_rms);
```

- Pass **`NULL`** for either pointer if you only need one value.
- **Not** a substitute for per-node metering inside **`SITUATION_NODE_PEAK_METER`** — this is the **master bus** snapshot for lightweight VU / HUD.

**Usage Example:**
```c
float peak, rms;
SituationGetMasterOutputMeter(&peak, &rms);
// Drive a UI level meter (clamp display range as needed)
```

---
#### `SituationIsAudioCapture`
Checks if the audio capture stream is currently active.
```c
SITAPI bool SituationIsAudioCapture(void);
```
**Usage Example:**
```c
if (SituationIsAudioCapture()) {
    // Update UI to show a "Recording" indicator.
}
```

---
#### Tone Generation and Synthesis
---
#### `SituationPlayToneEx`
Generates and plays a synthesized tone with full ADSR envelope control. Returns a handle that can be used to stop the tone early.
```c
SituationToneHandle SituationPlayToneEx(
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
```
**Parameters:**
- `type`: Waveform type (`SIT_WAVE_SINE`, `SIT_WAVE_SQUARE`, `SIT_WAVE_TRIANGLE`, `SIT_WAVE_SAWTOOTH`, `SIT_WAVE_NOISE`)
- `frequency`: Frequency in Hz (e.g., 440.0 for A4)
- `volume`: Volume from 0.0 to 1.0
- `pan`: Stereo panning from -1.0 (left) to 1.0 (right)
- `attack_sec`: Attack time in seconds
- `decay_sec`: Decay time in seconds
- `sustain_level`: Sustain level from 0.0 to 1.0
- `release_sec`: Release time in seconds
- `hold_sec`: How long to hold the sustain phase

**Usage Example:**
```c
// Play a 440Hz sine wave with a smooth envelope
SituationToneHandle tone = SituationPlayToneEx(
    SIT_WAVE_SINE,
    440.0f,      // A4 note
    0.5f,        // 50% volume
    0.0f,        // Center pan
    0.01f,       // 10ms attack
    0.1f,        // 100ms decay
    0.7f,        // 70% sustain level
    0.2f,        // 200ms release
    0.5f         // Hold for 500ms
);

// Optionally stop it early
if (user_cancelled) {
    SituationStopTone(tone);
}
```

---
#### `SituationPlayTone`
Simplified tone generation function without pan control. Useful for quick UI sounds and effects.
```c
void SituationPlayTone(
    SituationWaveType type,
    float frequency,
    float volume,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);
```
**Usage Example:**
```c
// Play a quick UI beep
SituationPlayTone(SIT_WAVE_SINE, 800.0f, 0.3f, 0.01f, 0.05f, 0.0f, 0.05f, 0.1f);

// Play a retro game jump sound
SituationPlayTone(SIT_WAVE_SQUARE, 600.0f, 0.4f, 0.0f, 0.0f, 1.0f, 0.1f, 0.15f);
```

---
#### `SituationPlayMidiNote`
Plays a tone using MIDI note numbering (0-127, where 60 = Middle C). This is convenient for musical applications.
```c
void SituationPlayMidiNote(
    int note,
    SituationWaveType type,
    float volume,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);
```
**Usage Example:**
```c
// Play Middle C (MIDI note 60) as a piano-like sound
SituationPlayMidiNote(60, SIT_WAVE_SINE, 0.5f, 0.01f, 0.2f, 0.6f, 0.3f, 1.0f);

// Play a C major chord
SituationPlayMidiNote(60, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // C
SituationPlayMidiNote(64, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // E
SituationPlayMidiNote(67, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // G
```

---
#### `SituationStopTone`
Stops a specific tone that was started with `SituationPlayToneEx()`.
```c
void SituationStopTone(SituationToneHandle handle);
```
**Usage Example:**
```c
SituationToneHandle alarm = SituationPlayToneEx(SIT_WAVE_SQUARE, 1000.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 5.0f);

// Later, when the alarm should stop
SituationStopTone(alarm);
```

---
#### `SituationStopAllTones`
Immediately stops all currently playing tones.
```c
void SituationStopAllTones(void);
```
**Usage Example:**
```c
// Emergency stop for all synthesized sounds
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    SituationStopAllTones();
}
```

---
#### `SituationSetToneReverbEnabled`
Enable or disable reverb effect for all tone generation. When enabled, tones will have a spacious, ambient sound simulating room acoustics.
```c
void SituationSetToneReverbEnabled(bool enabled);
```
**Parameters:**
- `enabled` - `true` to enable reverb, `false` to disable

**Usage Example:**
```c
// Enable reverb for ambient atmosphere
SituationSetToneReverbEnabled(true);

// Play tones - they will now have reverb
SituationPlayMidiNote(60, SIT_WAVE_SINE, 0.5f, 0.01f, 0.2f, 0.6f, 0.3f, 1.0f);

// Disable reverb for dry, direct sound
SituationSetToneReverbEnabled(false);
```

---
#### `SituationSetToneReverbParameters`
Configure the characteristics of the tone reverb effect. Allows fine-tuning of room size, damping, wet/dry mix, and stereo width.
```c
void SituationSetToneReverbParameters(
    float room_size,
    float damping,
    float wet_level,
    float dry_level,
    float width
);
```
**Parameters:**
- `room_size` - Room size (0.0 to 1.0). Larger values create longer reverb tails. Default: 0.7
- `damping` - High frequency damping (0.0 to 1.0). Higher values make the reverb darker. Default: 0.5
- `wet_level` - Reverb mix level (0.0 to 1.0). Amount of reverb signal in output. Default: 0.3
- `dry_level` - Dry signal level (0.0 to 1.0). Amount of original signal in output. Default: 1.0
- `width` - Stereo width (0.0 to 1.0). Controls the stereo spread of the reverb. Default: 1.0

**Usage Example:**
```c
// Large cathedral-like reverb
SituationSetToneReverbParameters(0.9f, 0.3f, 0.5f, 0.8f, 1.0f);

// Small room with bright reverb
SituationSetToneReverbParameters(0.3f, 0.2f, 0.2f, 1.0f, 0.7f);

// Subtle ambient space
SituationSetToneReverbParameters(0.5f, 0.5f, 0.15f, 1.0f, 0.8f);
```
