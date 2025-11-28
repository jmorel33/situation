#ifndef MINIAUDIO_H
#define MINIAUDIO_H

#include <stdint.h>
#include <stddef.h>

#define MA_SUCCESS 0
#define MA_AT_END -1
#define MA_NOT_IMPLEMENTED -2

typedef int ma_result;
typedef uint32_t ma_uint32;
typedef uint64_t ma_uint64;
typedef int64_t ma_int64;

typedef enum {
    ma_format_unknown = 0,
    ma_format_u8      = 1,
    ma_format_s16     = 2,
    ma_format_s24     = 3,
    ma_format_s32     = 4,
    ma_format_f32     = 5
} ma_format;

typedef struct {
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} ma_device_config;

typedef struct {
    void* pUserData;
    struct {
        ma_format format;
        ma_uint32 channels;
    } playback;
    ma_uint32 sampleRate;
} ma_device;

typedef struct {
    int placeholder;
} ma_context;

typedef struct {
    int placeholder;
} ma_context_config;

typedef struct {
    int placeholder;
} ma_decoder;

typedef struct {
    ma_format outputFormat;
    ma_uint32 outputChannels;
    ma_uint32 outputSampleRate;
} ma_decoder_config;

typedef struct {
    int placeholder;
} ma_data_converter;

typedef struct {
    ma_format formatIn;
    ma_uint32 channelsIn;
    ma_uint32 sampleRateIn;
    ma_format formatOut;
    ma_uint32 channelsOut;
    ma_uint32 sampleRateOut;
} ma_data_converter_config;

typedef struct {
    int placeholder;
} ma_mutex;

typedef struct {
    int placeholder;
} ma_biquad;

typedef struct {
    int placeholder;
} ma_biquad_config;

typedef struct {
    int placeholder;
} ma_delay;

typedef struct {
    int placeholder;
} ma_delay_config;

typedef struct {
    int placeholder;
} ma_reverb;

typedef struct {
    float roomSize;
    float damping;
    float wetVolume;
    float dryVolume;
} ma_reverb_config;

typedef struct {
    char name[256];
    struct {
        int id;
    } id;
    int isDefault;
} ma_device_info;

typedef struct {
    int placeholder;
} ma_encoder;

typedef struct {
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} ma_encoder_config;

typedef struct {
    int placeholder;
} ma_device_id;

typedef void (*ma_device_callback_proc)(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

// Enums
typedef enum {
    ma_device_type_playback,
    ma_device_type_capture
} ma_device_type;

typedef enum {
    ma_seek_origin_start,
    ma_seek_origin_current
} ma_seek_origin;

typedef enum {
    ma_encoding_format_wav
} ma_encoding_format;

// Functions
ma_context_config ma_context_config_init(void);
ma_result ma_context_init(const void* backends, ma_uint32 backendCount, const ma_context_config* pConfig, ma_context* pContext);
ma_result ma_context_uninit(ma_context* pContext);
ma_result ma_context_get_devices(ma_context* pContext, ma_device_info** ppPlaybackDeviceInfos, ma_uint32* pPlaybackDeviceCount, ma_device_info** ppCaptureDeviceInfos, ma_uint32* pCaptureDeviceCount);

ma_device_config ma_device_config_init(ma_device_type deviceType);
ma_result ma_device_init(ma_context* pContext, const ma_device_config* pConfig, ma_device* pDevice);
ma_result ma_device_uninit(ma_device* pDevice);
ma_result ma_device_start(ma_device* pDevice);
ma_result ma_device_stop(ma_device* pDevice);
int ma_device_is_started(ma_device* pDevice);
ma_result ma_device_set_master_volume(ma_device* pDevice, float volume);
ma_result ma_device_get_master_volume(ma_device* pDevice, float* pVolume);

ma_result ma_mutex_init(ma_mutex* pMutex);
void ma_mutex_uninit(ma_mutex* pMutex);
void ma_mutex_lock(ma_mutex* pMutex);
void ma_mutex_unlock(ma_mutex* pMutex);

size_t ma_get_bytes_per_frame(ma_format format, ma_uint32 channels);

// Decoder
ma_decoder_config ma_decoder_config_init(ma_format outputFormat, ma_uint32 outputChannels, ma_uint32 outputSampleRate);
ma_result ma_decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result ma_decoder_uninit(ma_decoder* pDecoder);
ma_result ma_decoder_read_pcm_frames(ma_decoder* pDecoder, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
ma_result ma_decoder_seek_to_pcm_frame(ma_decoder* pDecoder, ma_uint64 frameIndex);
ma_result ma_decoder_get_length_in_pcm_frames(ma_decoder* pDecoder, ma_uint64* pLength);
ma_result ma_decoder_get_cursor_in_pcm_frames(ma_decoder* pDecoder, ma_uint64* pCursor);

// Converter
ma_data_converter_config ma_data_converter_config_init_default(void);
ma_result ma_data_converter_init(const ma_data_converter_config* pConfig, const void* pAllocationCallbacks, ma_data_converter* pConverter);
void ma_data_converter_uninit(ma_data_converter* pConverter, const void* pAllocationCallbacks);
ma_result ma_data_converter_process_pcm_frames(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
ma_result ma_data_converter_set_rate_in_hz(ma_data_converter* pConverter, ma_uint32 rateInHz);
void ma_data_converter_get_required_input_frame_count(ma_data_converter* pConverter, ma_uint64 outputFrameCount, ma_uint64* pInputFrameCount);

// Effects
ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, double f0, double Q, double gainDB);
ma_result ma_biquad_init(const ma_biquad_config* pConfig, const void* pAllocationCallbacks, ma_biquad* pFilter);
ma_result ma_biquad_process_pcm_frames(ma_biquad* pFilter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount, ma_uint32 channels);

ma_delay_config ma_delay_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay);
ma_result ma_delay_init(const ma_delay_config* pConfig, const void* pAllocationCallbacks, ma_delay* pDelay);
void ma_delay_uninit(ma_delay* pDelay, const void* pAllocationCallbacks);
ma_result ma_delay_process_pcm_frames(ma_delay* pDelay, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount, ma_uint32 channels);
void ma_delay_set_wet(ma_delay* pDelay, float value);
void ma_delay_set_dry(ma_delay* pDelay, float value);

ma_reverb_config ma_reverb_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate);
ma_result ma_reverb_init(const ma_reverb_config* pConfig, const void* pAllocationCallbacks, ma_reverb* pReverb);
void ma_reverb_uninit(ma_reverb* pReverb, const void* pAllocationCallbacks);
ma_result ma_reverb_process_pcm_frames(ma_reverb* pReverb, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount, ma_uint32 channels);
void ma_reverb_set_room_size(ma_reverb* pReverb, float value);
void ma_reverb_set_damping(ma_reverb* pReverb, float value);
void ma_reverb_set_wet_volume(ma_reverb* pReverb, float value);
void ma_reverb_set_dry_volume(ma_reverb* pReverb, float value);

// Encoder
ma_encoder_config ma_encoder_config_init(ma_encoding_format resourceFormat, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);
ma_result ma_encoder_init_file(const char* pFilePath, const ma_encoder_config* pConfig, ma_encoder* pEncoder);
void ma_encoder_uninit(ma_encoder* pEncoder);
ma_result ma_encoder_write_pcm_frames(ma_encoder* pEncoder, const void* pFramesIn, ma_uint64 frameCount, ma_uint64* pFramesWritten);

const char* ma_result_description(ma_result result);

// Vtable stuff
typedef ma_uint64 (*ma_decoder_read_proc)(ma_decoder* pDecoder, void* pBufferOut, ma_uint64 bytesToRead);
typedef ma_result (*ma_decoder_seek_proc)(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);
typedef struct {
    ma_decoder_read_proc onRead;
    ma_decoder_seek_proc onSeek;
} ma_decoder_vtable;

ma_decoder_config ma_decoder_config_init_custom(const ma_decoder_vtable* pVTable, void* pUserData);
ma_result ma_decoder_seek_relative_pcm_frames(ma_decoder* pDecoder, ma_int64 frameCount, ma_uint64* pNewFrameIndex);

#define MA_MAX_CHANNELS 32

#endif // MINIAUDIO_H
