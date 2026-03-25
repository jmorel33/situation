#define POLYSONIX_IMPLEMENTATION
#define POLYSONIX_PATCHING_IMPLEMENTATION
#include "../polysonix.h"
#include "../px_patching.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Security test for Oscillator wave_idx bounds checking.
 * This test verifies that out-of-bounds wave_idx values in a preset
 * are correctly clamped to 0 during deserialization.
 */

static uint32_t adler32_test(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    const uint32_t MOD_ADLER = 65521;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t pos;
} MemStream;

size_t mem_write(void* token, const void* data, size_t size) {
    MemStream* ms = (MemStream*)token;
    if (ms->pos + size > ms->size) {
        ms->size = ms->pos + size;
        ms->buffer = (uint8_t*)realloc(ms->buffer, ms->size);
    }
    memcpy(ms->buffer + ms->pos, data, size);
    ms->pos += size;
    return size;
}

size_t mem_read(void* token, void* data, size_t size) {
    MemStream* ms = (MemStream*)token;
    if (ms->pos + size > ms->size) size = ms->size - ms->pos;
    memcpy(data, ms->buffer + ms->pos, size);
    ms->pos += size;
    return size;
}

int main() {
    PxConfig config = {
        .num_voices = 1,
        .num_lfos = 1,
        .num_voice_adsrs = 1,
        .sample_rate = 44100.0f,
        .samples_per_lfo_update = 44,
        .osc_update_mode = PX_OSC_UPDATE_MODE_PER_SAMPLE
    };

    PxSynth* synth = PX_Create(&config);
    if (!synth) return 1;

    // 1. Prepare a valid preset with a known marker
    synth->patch.osc[0].wave_idx = 200;
    synth->patch.osc[0].enabled = true;

    MemStream ms = {0};
    PX_SavePresetToBus(synth, mem_write, &ms, "SecurityTest");

    // 2. Corrupt the wave_idx to be out-of-bounds (e.g., 9999)
    uint32_t data_len = ((uint32_t)ms.buffer[26] << 24) | ((uint32_t)ms.buffer[27] << 16) |
                        ((uint32_t)ms.buffer[28] << 8) | ((uint32_t)ms.buffer[29]);

    uint8_t marker[] = {0x01, 0x00, 0x00, 0x00, 0xC8}; // enabled=1, wave_idx=200
    int found_pos = -1;
    for (int i = 40; i < (int)(40 + data_len - 5); i++) {
        if (memcmp(ms.buffer + i, marker, 5) == 0) {
            found_pos = i + 1;
            break;
        }
    }

    if (found_pos == -1) {
        printf("FAILED: Could not locate wave_idx marker\n");
        return 1;
    }

    // Change 200 to 9999 (0x0000270F)
    ms.buffer[found_pos] = 0x00;
    ms.buffer[found_pos + 1] = 0x00;
    ms.buffer[found_pos + 2] = 0x27;
    ms.buffer[found_pos + 3] = 0x0F;

    // Recalculate checksum and update footer
    uint32_t new_sum = adler32_test(ms.buffer + 40, data_len);
    ms.buffer[40 + data_len] = (new_sum >> 24) & 0xFF;
    ms.buffer[40 + data_len + 1] = (new_sum >> 16) & 0xFF;
    ms.buffer[40 + data_len + 2] = (new_sum >> 8) & 0xFF;
    ms.buffer[40 + data_len + 3] = new_sum & 0xFF;

    // 3. Load the malicious preset
    ms.pos = 0;
    char error[256];
    if (!PX_LoadPresetFromBus(synth, mem_read, &ms, error, sizeof(error))) {
        printf("FAILED: Load failed: %s\n", error);
        return 1;
    }

    // 4. Verify result
    int loaded_idx = synth->patch.osc[0].wave_idx;
    if (loaded_idx == 0) {
        printf("SUCCESS: wave_idx clamped to 0\n");
    } else if (loaded_idx == 9999) {
        printf("FAILED: wave_idx remained at 9999 (VULNERABLE)\n");
        return 1;
    } else {
        printf("FAILED: unexpected wave_idx %d\n", loaded_idx);
        return 1;
    }

    PX_Destroy(synth);
    free(ms.buffer);
    return 0;
}
