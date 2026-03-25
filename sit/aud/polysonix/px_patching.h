#ifndef PX_PATCHING_H
#define PX_PATCHING_H

#ifdef POLYSONIX_IMPLEMENTATION
  #define _PX_IMP_WAS_DEF
  #undef POLYSONIX_IMPLEMENTATION
#endif

#include "polysonix.h"

#ifdef _PX_IMP_WAS_DEF
  #define POLYSONIX_IMPLEMENTATION
  #undef _PX_IMP_WAS_DEF
#endif
#include <stdint.h>
#include <stddef.h>

#include "px_patches_rom.h"
#include "px_wseq_rom.h"
#include "px_wave_rom.h"



#ifdef __cplusplus
extern "C" {
#endif

// --- IO Abstraction ---

/**
 * @brief Callback for writing a batch of data to a stream/bus.
 * @param token User-provided context.
 * @param data Pointer to the data to write.
 * @param size Number of bytes to write.
 * @return Number of bytes actually written.
 */
typedef size_t (*PxIOWriteFn)(void* token, const void* data, size_t size);

/**
 * @brief Callback for reading a batch of data from a stream/bus.
 * @param token User-provided context.
 * @param data Pointer to the buffer to read into.
 * @param size Number of bytes to read.
 * @return Number of bytes actually read.
 */
typedef size_t (*PxIOReadFn)(void* token, void* data, size_t size);

// --- Core API ---

/**
 * @brief Calculates the total binary size (Header + Data + Checksum) required for a preset.
 * @param s The synthesizer instance.
 * @return The size in bytes.
 */
PX_API size_t PX_CalculatePresetSize(PxSynth* s);

/**
 * @brief Saves the current patch to an abstract bus/stream using a single batched write.
 * @details This function allocates a temporary buffer, serializes the full preset (Header, Data, Checksum),
 *          and calls the write_fn exactly once with the complete payload.
 * @param s The synthesizer instance.
 * @param write_fn The callback to handle the write operation.
 * @param token Context passed to the callback.
 * @param patch_name Name of the patch (max 15 chars).
 * @return true on success, false on failure.
 */
PX_API bool PX_SavePresetToBus(PxSynth* s, PxIOWriteFn write_fn, void* token, const char* patch_name);

/**
 * @brief Loads a patch from an abstract bus/stream.
 * @param s The synthesizer instance.
 * @param read_fn The callback to handle read operations.
 * @param token Context passed to the callback.
 * @param error_msg Optional buffer to receive an error message on failure. Can be NULL.
 * @param error_len Size of the error buffer.
 * @return true on success, false on failure.
 */
PX_API bool PX_LoadPresetFromBus(PxSynth* s, PxIOReadFn read_fn, void* token, char* error_msg, size_t error_len);

// --- File IO Wrappers ---

/**
 * @brief Saves the current patch to a file (disk).
 * @param s The synthesizer instance.
 * @param filename File path.
 * @param patch_name Patch name.
 * @param error_msg Optional buffer to receive an error message on failure. Can be NULL.
 * @param error_len Size of the error buffer.
 * @return true on success.
 */
PX_API bool PX_SavePreset(PxSynth* s, const char* filename, const char* patch_name, char* error_msg, size_t error_len);

/**
 * @brief Loads a patch from a file (disk).
 * @param s The synthesizer instance.
 * @param filename File path.
 * @param error_msg Optional buffer to receive an error message on failure. Can be NULL.
 * @param error_len Size of the error buffer.
 * @return true on success.
 */
PX_API bool PX_LoadPreset(PxSynth* s, const char* filename, char* error_msg, size_t error_len);

// --- Patch Bank API ---

#define PX_PATCH_BANK_SIZE 128

/**
 * @struct PxPatchBank
 * @brief A container for multiple patches, matching a specific configuration.
 */
typedef struct PxPatchBank {
    PxConfig config;
    PxPatch* patches; // Array of size PX_PATCH_BANK_SIZE
} PxPatchBank;

/**
 * @brief Creates a new patch bank initialized with the given configuration.
 * @details Allocates memory for the bank and all internal structures (ADSRs, LFOs) for each patch slot.
 * @param config The configuration to use for allocating patch memory.
 * @return A pointer to the new bank, or NULL on failure.
 */
PX_API PxPatchBank* PX_CreatePatchBank(const PxConfig* config);

/**
 * @brief Destroys a patch bank and frees all associated memory.
 * @param bank The bank to destroy.
 */
PX_API void PX_DestroyPatchBank(PxPatchBank* bank);

/**
 * @brief Saves the current state of the synth to a specific slot in the bank.
 * @param bank The destination bank.
 * @param slot_idx The slot index (0 to PX_PATCH_BANK_SIZE - 1).
 * @param s The source synthesizer.
 * @return true on success, false if invalid index or bank/synth is NULL.
 */
PX_API bool PX_Bank_SaveToSlot(PxPatchBank* bank, int slot_idx, PxSynth* s);

/**
 * @brief Loads a patch from a specific slot in the bank to the synth.
 * @param bank The source bank.
 * @param slot_idx The slot index (0 to PX_PATCH_BANK_SIZE - 1).
 * @param s The destination synthesizer.
 * @return true on success, false if invalid index or bank/synth is NULL.
 */
PX_API bool PX_Bank_LoadFromSlot(PxPatchBank* bank, int slot_idx, PxSynth* s);

/**
 * @brief Copies a patch from one slot to another within the same bank.
 * @param bank The bank.
 * @param src_idx Source slot index.
 * @param dest_idx Destination slot index.
 * @return true on success.
 */
PX_API bool PX_Bank_CopySlot(PxPatchBank* bank, int src_idx, int dest_idx);

#ifdef __cplusplus
}
#endif

#endif // PX_PATCHING_H

#ifdef POLYSONIX_PATCHING_IMPLEMENTATION

#define PX_PATCHES_ROM_IMPLEMENTATION
#include "px_patches_rom.h"

#define PX_WSEQ_ROM_IMPLEMENTATION
#include "px_wseq_rom.h"

#define PX_WAVE_ROM_IMPLEMENTATION
#include "px_wave_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Helper Functions ---

static void write_u8(uint8_t** ptr, uint8_t val) {
    if (*ptr) { **ptr = val; (*ptr)++; }
}

static void write_u16(uint8_t** ptr, uint16_t val) {
    if (*ptr) {
        (*ptr)[0] = (val >> 8) & 0xFF;
        (*ptr)[1] = val & 0xFF;
        *ptr += 2;
    }
}

static void write_u32(uint8_t** ptr, uint32_t val) {
    if (*ptr) {
        (*ptr)[0] = (val >> 24) & 0xFF;
        (*ptr)[1] = (val >> 16) & 0xFF;
        (*ptr)[2] = (val >> 8) & 0xFF;
        (*ptr)[3] = val & 0xFF;
        *ptr += 4;
    }
}

static void write_f32(uint8_t** ptr, float val) {
    // Assuming IEEE 754 layout match (most platforms)
    // For portability, one might use a union or memcpy.
    if (*ptr) {
        union { float f; uint32_t u; } u;
        u.f = val;
        // Write as big endian to allow cross-platform safety
        write_u32(ptr, u.u);
    }
}

static void write_bool(uint8_t** ptr, bool val) {
    write_u8(ptr, val ? 1 : 0);
}

static void write_buf(uint8_t** ptr, const void* data, size_t size) {
    if (*ptr) {
        memcpy(*ptr, data, size);
        *ptr += size;
    }
}

static bool read_u8(const uint8_t** ptr, const uint8_t* end, uint8_t* out) {
    if (*ptr + 1 > end) return false;
    *out = **ptr;
    (*ptr)++;
    return true;
}

static bool read_u16(const uint8_t** ptr, const uint8_t* end, uint16_t* out) {
    if (*ptr + 2 > end) return false;
    *out = ((uint16_t)(*ptr)[0] << 8) | (*ptr)[1];
    *ptr += 2;
    return true;
}

static bool read_u32(const uint8_t** ptr, const uint8_t* end, uint32_t* out) {
    if (*ptr + 4 > end) return false;
    *out = ((uint32_t)(*ptr)[0] << 24) | ((uint32_t)(*ptr)[1] << 16) |
           ((uint32_t)(*ptr)[2] << 8) | (*ptr)[3];
    *ptr += 4;
    return true;
}

static bool read_f32(const uint8_t** ptr, const uint8_t* end, float* out) {
    uint32_t u_val;
    if (!read_u32(ptr, end, &u_val)) return false;
    union { float f; uint32_t u; } u;
    u.u = u_val;
    *out = u.f;
    return true;
}

static bool read_bool(const uint8_t** ptr, const uint8_t* end, bool* out) {
    uint8_t val;
    if (!read_u8(ptr, end, &val)) return false;
    *out = (val != 0);
    return true;
}

static bool read_buf(const uint8_t** ptr, const uint8_t* end, void* out, size_t size) {
    if (*ptr + size > end) return false;
    memcpy(out, *ptr, size);
    *ptr += size;
    return true;
}

// --- Checksum Implementation (Adler-32) ---
static uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    const uint32_t MOD_ADLER = 65521;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

// --- Header Constant ---
#define PX_PRESET_HEADER_SIZE 40

// --- Serialization Core ---

static void px_serialize_patch_impl(const PxPatch* p, const PxConfig* c, uint8_t** ptr_ref, size_t* size_ref, bool calculate_only) {
    // If calculate_only is true, ptr_ref might be NULL, but we track size.
    uint8_t* ptr = (ptr_ref && !calculate_only) ? *ptr_ref : NULL;
    size_t size = 0;

    #define WR_U8(v)   do { if(ptr) write_u8(&ptr, v); size += 1; } while(0)
    #define WR_U16(v)  do { if(ptr) write_u16(&ptr, v); size += 2; } while(0)
    #define WR_U32(v)  do { if(ptr) write_u32(&ptr, v); size += 4; } while(0)
    #define WR_F32(v)  do { if(ptr) write_f32(&ptr, v); size += 4; } while(0)
    #define WR_BOOL(v) do { if(ptr) write_bool(&ptr, v); size += 1; } while(0)
    #define WR_BUF(v, s) do { if(ptr) write_buf(&ptr, v, s); size += s; } while(0)

    // 1. Dynamic Arrays
    if (p->template_voice_adsrs)
        WR_BUF(p->template_voice_adsrs, sizeof(PxADSRParams) * c->num_voice_adsrs);
    if (p->template_voice_adsr_mod_amounts)
        WR_BUF(p->template_voice_adsr_mod_amounts, sizeof(float) * c->num_voice_adsrs * PX_ADSR_DEST_COUNT);
    if (p->template_lfos)
        WR_BUF(p->template_lfos, sizeof(PxLFOParams) * c->num_lfos);

    // 2. Scalars
    WR_F32(p->filter_cutoff_hz);
    WR_F32(p->filter_resonance_q);
    WR_F32(p->filter_env_amount_hz);
    WR_F32(p->filter_drive);
    WR_F32(p->filter_key_track);
    WR_U32((uint32_t)p->filter_poles);
    WR_U8((uint8_t)p->filter_mode);
    WR_F32(p->voice_pan_setting);
    WR_F32(p->default_note_amp);
    WR_F32(p->limiter_threshold);
    WR_F32(p->limiter_release_ms);
    WR_BOOL(p->unilegato_enabled);
    WR_F32(p->unilegato_slide_duration_s);

    WR_BUF(p->mod_matrix, sizeof(PxModSlot) * PX_MOD_MATRIX_SLOTS);

    WR_F32(p->pitchbend_range_semitones);

    WR_BOOL(p->global_filter_enabled);
    WR_F32(p->global_filter_cutoff_hz);
    WR_F32(p->global_filter_resonance_q);
    WR_F32(p->global_filter_env_amount_hz);
    WR_F32(p->global_filter_drive);
    WR_F32(p->global_filter_key_track);
    WR_U32((uint32_t)p->global_filter_poles);
    WR_U8((uint8_t)p->global_filter_mode);

    WR_U8((uint8_t)p->velocity_curve);
    WR_U8((uint8_t)p->aftertouch_curve);

    // 3. Oscillators (Explicit Fields)
    for (int i = 0; i < PX_MAX_OSC_PER_VOICE; i++) {
        const PxOscillator* o = &p->osc[i];
        WR_BOOL(o->enabled);
        WR_U32((uint32_t)o->wave_idx);
        WR_F32(o->coarse_semitones);
        WR_F32(o->fine_cents);
        WR_F32(o->mix_level);
        WR_F32(o->pan);
        WR_U32((uint32_t)(int32_t)o->sequence_id); // Cast to signed 32-bit then unsigned for transport

        WR_BOOL(o->cross_mod_enabled);
        WR_F32(o->cross_mod_depth);
        WR_BOOL(o->phase_dist_enabled);
        WR_F32(o->phase_dist_amount);
        WR_BOOL(o->osc_sync_enabled);
        WR_F32(o->osc_sync_softness);
        WR_BOOL(o->ring_mod_enabled);
        WR_F32(o->ring_mod_depth);

        WR_BOOL(o->bitcrush_enabled);
        WR_F32(o->bitcrush_depth);
    }

    WR_BOOL(p->wseq_fixed_time);
    WR_F32(p->wseq_ref_freq);

    WR_U8((uint8_t)p->glide_mode);
    WR_F32(p->glide_time);
    WR_BOOL(p->glide_legato_only);
    WR_BOOL(p->glide_always);

    // v1.8.5: Full patch name
    WR_BUF(p->name, PX_PATCH_NAME_LEN);

    if (ptr_ref && !calculate_only) *ptr_ref = ptr;
    if (size_ref) *size_ref = size;

    #undef WR_U8
    #undef WR_U16
    #undef WR_U32
    #undef WR_F32
    #undef WR_BOOL
    #undef WR_BUF
}

static bool px_deserialize_patch_impl(PxPatch* p, const PxConfig* c, const uint8_t** ptr_ref, const uint8_t* end, uint16_t maj, uint16_t min, uint16_t pat) {
    const uint8_t* ptr = *ptr_ref;

    #define RD_U8(v)   if(!read_u8(&ptr, end, (uint8_t*)v)) return false
    #define RD_U16(v)  if(!read_u16(&ptr, end, (uint16_t*)v)) return false
    #define RD_U32(v)  if(!read_u32(&ptr, end, (uint32_t*)v)) return false
    #define RD_F32(v)  if(!read_f32(&ptr, end, (float*)v)) return false
    #define RD_BOOL(v) if(!read_bool(&ptr, end, (bool*)v)) return false
    #define RD_BUF(v, s) if(!read_buf(&ptr, end, v, s)) return false

    // 1. Dynamic Arrays
    if (p->template_voice_adsrs)
        RD_BUF(p->template_voice_adsrs, sizeof(PxADSRParams) * c->num_voice_adsrs);
    if (p->template_voice_adsr_mod_amounts)
        RD_BUF(p->template_voice_adsr_mod_amounts, sizeof(float) * c->num_voice_adsrs * PX_ADSR_DEST_COUNT);
    if (p->template_lfos)
        RD_BUF(p->template_lfos, sizeof(PxLFOParams) * c->num_lfos);

    // 2. Scalars
    RD_F32(&p->filter_cutoff_hz);
    RD_F32(&p->filter_resonance_q);
    RD_F32(&p->filter_env_amount_hz);
    RD_F32(&p->filter_drive);
    RD_F32(&p->filter_key_track);
    uint32_t u_poles; RD_U32(&u_poles); p->filter_poles = (int)u_poles;
    uint8_t u_mode; RD_U8(&u_mode); p->filter_mode = (PxFilterMode)u_mode;

    RD_F32(&p->voice_pan_setting);
    RD_F32(&p->default_note_amp);
    RD_F32(&p->limiter_threshold);
    RD_F32(&p->limiter_release_ms);
    RD_BOOL(&p->unilegato_enabled);
    RD_F32(&p->unilegato_slide_duration_s);

    RD_BUF(p->mod_matrix, sizeof(PxModSlot) * PX_MOD_MATRIX_SLOTS);

    RD_F32(&p->pitchbend_range_semitones);

    RD_BOOL(&p->global_filter_enabled);
    RD_F32(&p->global_filter_cutoff_hz);
    RD_F32(&p->global_filter_resonance_q);
    RD_F32(&p->global_filter_env_amount_hz);
    RD_F32(&p->global_filter_drive);
    RD_F32(&p->global_filter_key_track);
    RD_U32(&u_poles); p->global_filter_poles = (int)u_poles;
    RD_U8(&u_mode); p->global_filter_mode = (PxFilterMode)u_mode;

    uint8_t u_curve;
    RD_U8(&u_curve); p->velocity_curve = (PxCurveType)u_curve;
    RD_U8(&u_curve); p->aftertouch_curve = (PxCurveType)u_curve;

    // 3. Oscillators
    for (int i = 0; i < PX_MAX_OSC_PER_VOICE; i++) {
        PxOscillator* o = &p->osc[i];
        RD_BOOL(&o->enabled);
        uint32_t u_wave; RD_U32(&u_wave);
        if (u_wave >= NUM_WAVEFORMS) u_wave = 0; // Security Fix: Clamp to valid waveform index
        o->wave_idx = (int)u_wave;
        RD_F32(&o->coarse_semitones);
        RD_F32(&o->fine_cents);
        RD_F32(&o->mix_level);
        RD_F32(&o->pan);
        uint32_t u_seq; RD_U32(&u_seq); o->sequence_id = (int32_t)u_seq; // Cast back to signed int

        RD_BOOL(&o->cross_mod_enabled);
        RD_F32(&o->cross_mod_depth);
        RD_BOOL(&o->phase_dist_enabled);
        RD_F32(&o->phase_dist_amount);
        RD_BOOL(&o->osc_sync_enabled);
        RD_F32(&o->osc_sync_softness);
        RD_BOOL(&o->ring_mod_enabled);
        RD_F32(&o->ring_mod_depth);

        RD_BOOL(&o->bitcrush_enabled);
        RD_F32(&o->bitcrush_depth);
    }

    RD_BOOL(&p->wseq_fixed_time);
    RD_F32(&p->wseq_ref_freq);

    uint8_t u_glide_mode;
    RD_U8(&u_glide_mode); p->glide_mode = (PxGlideMode)u_glide_mode;
    RD_F32(&p->glide_time);
    RD_BOOL(&p->glide_legato_only);
    RD_BOOL(&p->glide_always);

    // v1.8.5: Full patch name
    // Check version: only read if >= 1.8.5
    bool has_full_name = false;
    if (maj > 1 || (maj == 1 && min > 8) || (maj == 1 && min == 8 && pat >= 5)) {
        has_full_name = true;
    }

    if (has_full_name) {
        RD_BUF(p->name, PX_PATCH_NAME_LEN);
        p->name[PX_PATCH_NAME_LEN - 1] = '\0'; // Safety null termination
    }

    *ptr_ref = ptr;

    #undef RD_U8
    #undef RD_U16
    #undef RD_U32
    #undef RD_F32
    #undef RD_BOOL
    #undef RD_BUF

    return true;
}

PX_API size_t PX_CalculatePresetSize(PxSynth* s) {
    size_t size = PX_PRESET_HEADER_SIZE;
    size_t body_size = 0;
    px_serialize_patch_impl(&s->patch, &s->config, NULL, &body_size, true);
    size += body_size;
    size += 4; // Footer Checksum
    return size;
}

PX_API bool PX_SavePresetToBus(PxSynth* s, PxIOWriteFn write_fn, void* token, const char* patch_name) {
    if (!s || !write_fn) return false;

    size_t total_size = PX_CalculatePresetSize(s);
    uint8_t* buffer = (uint8_t*)malloc(total_size);
    if (!buffer) return false;

    uint8_t* ptr = buffer;

    // --- Header ---
    // Manually write header to ensure packing/layout
    // Magic (4)
    write_buf(&ptr, "POLY", 4);
    // Ver (2+2+2) = 6
    write_u16(&ptr, POLYSONIX_VERSION_MAJOR);
    write_u16(&ptr, POLYSONIX_VERSION_MINOR);
    write_u16(&ptr, POLYSONIX_VERSION_PATCH);

    // Name (16)
    char name[16];
    memset(name, 0, 16); // Zero pad
    const char* src_name = (patch_name && patch_name[0] != '\0') ? patch_name : s->patch.name;
    if (src_name) {
        size_t len = strlen(src_name);
        if (len > 15) len = 15;
        memcpy(name, src_name, len);
    }
    write_buf(&ptr, name, 16);

    // Data Len (4) - calculated later, placeholder
    uint8_t* len_ptr = ptr;
    write_u32(&ptr, 0); // Placeholder

    // Config (2+2) = 4
    write_u16(&ptr, (uint16_t)s->config.num_voice_adsrs);
    write_u16(&ptr, (uint16_t)s->config.num_lfos);

    // Config Hash (4)
    uint32_t config_hash = (s->config.num_voices * 31UL) + (s->config.num_lfos * 37UL) + (s->config.num_voice_adsrs * 41UL);
    write_u32(&ptr, config_hash);

    // Padding (2) to reach 40 bytes
    write_u16(&ptr, 0);

    // Total Header Written: 4 + 6 + 16 + 4 + 4 + 4 + 2 = 40 bytes.

    // --- Data Block ---
    uint8_t* data_start = ptr;
    px_serialize_patch_impl(&s->patch, &s->config, &ptr, NULL, false);

    // Patch Data Len
    size_t written_data_len = (size_t)(ptr - data_start);

    // Write actual length to placeholder
    uint8_t* temp_ptr = len_ptr;
    write_u32(&temp_ptr, (uint32_t)written_data_len);

    // --- Checksum (Adler-32) ---
    uint32_t checksum = adler32(data_start, written_data_len);
    write_u32(&ptr, checksum);

    // --- Batch Write ---
    size_t written = write_fn(token, buffer, total_size);

    free(buffer);
    return (written == total_size);
}

PX_API bool PX_LoadPresetFromBus(PxSynth* s, PxIOReadFn read_fn, void* token, char* error_msg, size_t error_len) {
    if (!s || !read_fn) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Invalid arguments (NULL synth or callback)");
        return false;
    }
    if (error_msg && error_len > 0) error_msg[0] = '\0';

    // 1. Read Header (40 bytes)
    uint8_t header[PX_PRESET_HEADER_SIZE];
    if (read_fn(token, header, PX_PRESET_HEADER_SIZE) != PX_PRESET_HEADER_SIZE) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Failed to read header");
        return false;
    }

    // Parse Header
    if (memcmp(header, "POLY", 4) != 0) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Invalid magic number");
        return false;
    }

    const uint8_t* hptr = header + 4;
    // Helper to read header fields from buffer without advancing stream (already read)
    // We can use the read_u16 helpers if we treat 'hptr' as the stream.
    const uint8_t* hend = header + PX_PRESET_HEADER_SIZE;

    uint16_t maj, min, pat;
    read_u16(&hptr, hend, &maj);
    read_u16(&hptr, hend, &min);
    read_u16(&hptr, hend, &pat);

    // Version check
    if (maj > POLYSONIX_VERSION_MAJOR || (maj == POLYSONIX_VERSION_MAJOR && min > POLYSONIX_VERSION_MINOR)) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Version mismatch: File %d.%d > Synth %d.%d", maj, min, POLYSONIX_VERSION_MAJOR, POLYSONIX_VERSION_MINOR);
        return false;
    }

    // Name (16)
    memcpy(s->patch.name, hptr, 16);
    s->patch.name[15] = '\0'; // Ensure null termination
    hptr += 16;

    uint32_t data_len;
    read_u32(&hptr, hend, &data_len);

    uint16_t file_n_adsrs, file_n_lfos;
    read_u16(&hptr, hend, &file_n_adsrs);
    read_u16(&hptr, hend, &file_n_lfos);

    uint32_t file_config_hash;
    read_u32(&hptr, hend, &file_config_hash);

    // Config validation
    uint32_t current_hash = (s->config.num_voices * 31UL) + (s->config.num_lfos * 37UL) + (s->config.num_voice_adsrs * 41UL);

    if (file_config_hash != current_hash) {
         if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Config mismatch (Hash %u != %u)", file_config_hash, current_hash);
         return false;
    }

    // Skip padding (2)
    hptr += 2;

    // 2. Allocate Data + Footer
    size_t payload_size = data_len + 4;
    uint8_t* buffer = (uint8_t*)malloc(payload_size);
    if (!buffer) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Memory allocation failed");
        return false;
    }

    // 3. Read Body + Checksum
    if (read_fn(token, buffer, payload_size) != payload_size) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Failed to read body");
        free(buffer); return false;
    }

    // 4. Verify Checksum (Adler-32)
    uint32_t expected_checksum;
    // Checksum is at end of buffer
    const uint8_t* cptr = buffer + data_len;
    const uint8_t* cend = buffer + payload_size;
    read_u32(&cptr, cend, &expected_checksum);

    uint32_t calc_sum = adler32(buffer, data_len);

    if (calc_sum != expected_checksum) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Checksum mismatch (Calc %08X != Exp %08X)", calc_sum, expected_checksum);
        free(buffer); return false;
    }

    // 5. Deserialize
    const uint8_t* ptr = buffer;
    const uint8_t* end = buffer + data_len;

    bool result = px_deserialize_patch_impl(&s->patch, (const PxConfig*)&s->config, &ptr, end, maj, min, pat);
    if (!result) {
        if(error_msg && error_len > 0) snprintf(error_msg, error_len, "Deserialization failed (buffer overrun?)");
    }

    free(buffer);
    return result;
}

// --- Wrappers for File IO ---

static size_t file_write_wrapper(void* token, const void* data, size_t size) {
    return fwrite(data, 1, size, (FILE*)token);
}

static size_t file_read_wrapper(void* token, void* data, size_t size) {
    return fread(data, 1, size, (FILE*)token);
}

PX_API bool PX_SavePreset(PxSynth* s, const char* filename, const char* patch_name, char* error_msg, size_t error_len) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        if (error_msg && error_len > 0) snprintf(error_msg, error_len, "Failed to open file for writing: %s", filename);
        return false;
    }
    bool res = PX_SavePresetToBus(s, file_write_wrapper, f, patch_name);
    fclose(f);
    return res;
}

PX_API bool PX_LoadPreset(PxSynth* s, const char* filename, char* error_msg, size_t error_len) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        if (error_msg && error_len > 0) snprintf(error_msg, error_len, "Failed to open file for reading: %s", filename);
        return false;
    }
    bool res = PX_LoadPresetFromBus(s, file_read_wrapper, f, error_msg, error_len);
    fclose(f);
    return res;
}

// --- Patch Bank Implementation ---

// Helper: Deep copy a single patch
static bool px_copy_patch_deep(PxPatch* dest, const PxPatch* src, const PxConfig* config) {
    // Save dest pointers (preservation of allocated memory)
    PxADSRParams* dest_adsrs = dest->template_voice_adsrs;
    float* dest_mod_amounts = dest->template_voice_adsr_mod_amounts;
    PxLFOParams* dest_lfos = dest->template_lfos;

    // Apply shallow copy (copies all scalars and embedded arrays)
    *dest = *src;

    // Restore dest pointers (so we don't leak memory or lose our buffers)
    dest->template_voice_adsrs = dest_adsrs;
    dest->template_voice_adsr_mod_amounts = dest_mod_amounts;
    dest->template_lfos = dest_lfos;

    // Perform deep copy into buffers
    if (dest->template_voice_adsrs && src->template_voice_adsrs)
        memcpy(dest->template_voice_adsrs, src->template_voice_adsrs, sizeof(PxADSRParams) * config->num_voice_adsrs);

    if (dest->template_voice_adsr_mod_amounts && src->template_voice_adsr_mod_amounts)
        memcpy(dest->template_voice_adsr_mod_amounts, src->template_voice_adsr_mod_amounts, sizeof(float) * config->num_voice_adsrs * PX_ADSR_DEST_COUNT);

    if (dest->template_lfos && src->template_lfos)
        memcpy(dest->template_lfos, src->template_lfos, sizeof(PxLFOParams) * config->num_lfos);

    return true;
}

// Helper: Allocate patch memory
static bool px_allocate_patch_memory(PxPatch* p, const PxConfig* config) {
    p->template_voice_adsrs = (PxADSRParams*)calloc(config->num_voice_adsrs, sizeof(PxADSRParams));
    p->template_voice_adsr_mod_amounts = (float*)calloc(config->num_voice_adsrs * PX_ADSR_DEST_COUNT, sizeof(float));
    p->template_lfos = (PxLFOParams*)calloc(config->num_lfos, sizeof(PxLFOParams));

    if (!p->template_voice_adsrs || !p->template_voice_adsr_mod_amounts || !p->template_lfos) {
        if (p->template_voice_adsrs) free(p->template_voice_adsrs);
        if (p->template_voice_adsr_mod_amounts) free(p->template_voice_adsr_mod_amounts);
        if (p->template_lfos) free(p->template_lfos);
        return false;
    }
    return true;
}

static void px_free_patch_memory(PxPatch* p) {
    if (p->template_voice_adsrs) free(p->template_voice_adsrs);
    if (p->template_voice_adsr_mod_amounts) free(p->template_voice_adsr_mod_amounts);
    if (p->template_lfos) free(p->template_lfos);
}

PX_API PxPatchBank* PX_CreatePatchBank(const PxConfig* config) {
    if (!config) return NULL;

    PxPatchBank* bank = (PxPatchBank*)calloc(1, sizeof(PxPatchBank));
    if (!bank) return NULL;

    bank->config = *config;
    bank->patches = (PxPatch*)calloc(PX_PATCH_BANK_SIZE, sizeof(PxPatch));
    if (!bank->patches) {
        free(bank);
        return NULL;
    }

    // Allocate deep memory for each patch
    for (int i = 0; i < PX_PATCH_BANK_SIZE; i++) {
        if (!px_allocate_patch_memory(&bank->patches[i], config)) {
            // Unwind
            for (int j = 0; j < i; j++) px_free_patch_memory(&bank->patches[j]);
            free(bank->patches);
            free(bank);
            return NULL;
        }
    }

    return bank;
}

PX_API void PX_DestroyPatchBank(PxPatchBank* bank) {
    if (!bank) return;
    if (bank->patches) {
        for (int i = 0; i < PX_PATCH_BANK_SIZE; i++) {
            px_free_patch_memory(&bank->patches[i]);
        }
        free(bank->patches);
    }
    free(bank);
}

PX_API bool PX_Bank_SaveToSlot(PxPatchBank* bank, int slot_idx, PxSynth* s) {
    if (!bank || !s || slot_idx < 0 || slot_idx >= PX_PATCH_BANK_SIZE) return false;

    // Safety Check: Ensure bank configuration is sufficient to hold synth data
    if (bank->config.num_voice_adsrs < s->config.num_voice_adsrs ||
        bank->config.num_lfos < s->config.num_lfos) {
        return false;
    }

    return px_copy_patch_deep(&bank->patches[slot_idx], (const PxPatch*)&s->patch, (const PxConfig*)&s->config);
}

PX_API bool PX_Bank_LoadFromSlot(PxPatchBank* bank, int slot_idx, PxSynth* s) {
    if (!bank || !s || slot_idx < 0 || slot_idx >= PX_PATCH_BANK_SIZE) return false;

    // Safety Check: Ensure synth configuration is sufficient to hold bank data
    if (s->config.num_voice_adsrs < bank->config.num_voice_adsrs ||
        s->config.num_lfos < bank->config.num_lfos) {
        return false;
    }

    return px_copy_patch_deep(&s->patch, (const PxPatch*)&bank->patches[slot_idx], (const PxConfig*)&bank->config);
}

PX_API bool PX_Bank_CopySlot(PxPatchBank* bank, int src_idx, int dest_idx) {
    if (!bank || src_idx < 0 || src_idx >= PX_PATCH_BANK_SIZE || dest_idx < 0 || dest_idx >= PX_PATCH_BANK_SIZE) return false;
    if (src_idx == dest_idx) return true;
    return px_copy_patch_deep(&bank->patches[dest_idx], (const PxPatch*)&bank->patches[src_idx], (const PxConfig*)&bank->config);
}



#endif // POLYSONIX_PATCHING_IMPLEMENTATION
