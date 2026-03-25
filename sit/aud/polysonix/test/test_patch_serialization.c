#define POLYSONIX_IMPLEMENTATION
#define POLYSONIX_PATCHING_IMPLEMENTATION
#include "../polysonix.h"
#include "../px_patching.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// --- Memory Stream ---

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t capacity;
    size_t pos;
} MemStream;

size_t mem_write(void* token, const void* data, size_t size) {
    MemStream* s = (MemStream*)token;
    if (s->pos + size > s->capacity) {
        size_t new_cap = (s->pos + size) * 2;
        if (new_cap < 1024) new_cap = 1024;
        uint8_t* new_buf = realloc(s->buffer, new_cap);
        if (!new_buf) return 0;
        s->buffer = new_buf;
        s->capacity = new_cap;
    }
    memcpy(s->buffer + s->pos, data, size);
    s->pos += size;
    if (s->pos > s->size) s->size = s->pos;
    return size;
}

size_t mem_read(void* token, void* data, size_t size) {
    MemStream* s = (MemStream*)token;
    if (s->pos + size > s->size) {
        size_t available = s->size - s->pos;
        if (available == 0) return 0;
        size = available;
    }
    memcpy(data, s->buffer + s->pos, size);
    s->pos += size;
    return size;
}

// --- Helpers ---

float rand_f(float min, float max) {
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

int rand_i(int min, int max) {
    return min + rand() % (max - min + 1);
}

bool rand_b() {
    return rand() % 2 == 0;
}

void populate_patch(PxPatch* p, const PxConfig* c) {
    snprintf(p->name, PX_PATCH_NAME_LEN, "Test Patch %d", rand_i(0, 1000));

    // Scalars
    p->filter_cutoff_hz = rand_f(20.0f, 20000.0f);
    p->filter_resonance_q = rand_f(0.5f, 20.0f);
    p->filter_env_amount_hz = rand_f(-5000.0f, 5000.0f);
    p->filter_drive = rand_f(1.0f, 5.0f);
    p->filter_key_track = rand_f(0.0f, 1.0f);
    p->filter_poles = rand_i(1, 4);
    p->filter_mode = (PxFilterMode)rand_i(0, PX_FILTER_MODE_COUNT - 1);

    p->voice_pan_setting = rand_f(-1.0f, 1.0f);
    p->default_note_amp = rand_f(0.0f, 1.0f);
    p->limiter_threshold = rand_f(0.1f, 1.0f);
    p->limiter_release_ms = rand_f(10.0f, 500.0f);
    p->unilegato_enabled = rand_b();
    p->unilegato_slide_duration_s = rand_f(0.01f, 1.0f);

    p->pitchbend_range_semitones = rand_f(1.0f, 12.0f);
    p->global_filter_enabled = rand_b();
    p->global_filter_cutoff_hz = rand_f(20.0f, 20000.0f);
    p->global_filter_resonance_q = rand_f(0.5f, 10.0f);
    p->global_filter_env_amount_hz = rand_f(-1000.0f, 1000.0f); // Ignored but field exists
    p->global_filter_drive = rand_f(1.0f, 2.0f);
    p->global_filter_key_track = rand_f(0.0f, 1.0f);
    p->global_filter_poles = rand_i(1, 4);
    p->global_filter_mode = (PxFilterMode)rand_i(0, PX_FILTER_MODE_COUNT - 1);

    p->velocity_curve = (PxCurveType)rand_i(0, PX_CURVE_COUNT - 1);
    p->aftertouch_curve = (PxCurveType)rand_i(0, PX_CURVE_COUNT - 1);

    p->wseq_fixed_time = rand_b();
    p->wseq_ref_freq = rand_f(100.0f, 1000.0f);
    p->glide_mode = (PxGlideMode)rand_i(0, 2);
    p->glide_time = rand_f(0.0f, 2.0f);
    p->glide_legato_only = rand_b();
    p->glide_always = rand_b();

    // Arrays: ADSRs
    for(int i=0; i<c->num_voice_adsrs; i++) {
        p->template_voice_adsrs[i].attack_time = rand_f(0.01f, 5.0f);
        p->template_voice_adsrs[i].decay_time = rand_f(0.01f, 5.0f);
        p->template_voice_adsrs[i].sustain_level = rand_f(0.0f, 1.0f);
        p->template_voice_adsrs[i].release_time = rand_f(0.01f, 5.0f);
        p->template_voice_adsrs[i].enabled = rand_b();

        for(int j=0; j<PX_ADSR_DEST_COUNT; j++) {
            p->template_voice_adsr_mod_amounts[i * PX_ADSR_DEST_COUNT + j] = rand_f(-1.0f, 1.0f);
        }
    }

    // Arrays: LFOs
    for(int i=0; i<c->num_lfos; i++) {
        p->template_lfos[i].wave_idx = rand_i(0, 255);
        p->template_lfos[i].frequency = rand_f(0.1f, 20.0f);
        p->template_lfos[i].enabled = rand_b();
        p->template_lfos[i].reset_on_key_on = rand_b();

        p->template_lfos[i].adsr.attack_time = rand_f(0.01f, 1.0f);
        p->template_lfos[i].adsr.decay_time = rand_f(0.01f, 1.0f);
        p->template_lfos[i].adsr.sustain_level = rand_f(0.0f, 1.0f);
        p->template_lfos[i].adsr.release_time = rand_f(0.01f, 1.0f);
        p->template_lfos[i].adsr.enabled = rand_b();

        for(int j=0; j<PX_LFO_DEST_COUNT; j++) {
            p->template_lfos[i].mod_amounts[j] = rand_f(-1.0f, 1.0f);
        }
    }

    // Mod Matrix
    for(int i=0; i<PX_MOD_MATRIX_SLOTS; i++) {
        p->mod_matrix[i].source = (PxModSource)rand_i(0, PX_MOD_SRC_COUNT - 1);
        p->mod_matrix[i].dest = (PxModDestination)rand_i(0, PX_MOD_DEST_COUNT - 1);
        p->mod_matrix[i].amount = rand_f(-1.0f, 1.0f);
        p->mod_matrix[i].enabled = rand_b();
    }

    // Oscillators
    for(int i=0; i<PX_MAX_OSC_PER_VOICE; i++) {
        p->osc[i].enabled = rand_b();
        p->osc[i].wave_idx = rand_i(0, 255);
        p->osc[i].coarse_semitones = rand_f(-24.0f, 24.0f);
        p->osc[i].fine_cents = rand_f(-100.0f, 100.0f);
        p->osc[i].mix_level = rand_f(0.0f, 1.0f);
        p->osc[i].pan = rand_f(-1.0f, 1.0f);
        p->osc[i].sequence_id = rand_i(-1, 200); // Some invalid ones maybe? No, stick to valid range if possible, or -1

        p->osc[i].cross_mod_enabled = rand_b();
        p->osc[i].cross_mod_depth = rand_f(0.0f, 1.0f);
        p->osc[i].phase_dist_enabled = rand_b();
        p->osc[i].phase_dist_amount = rand_f(0.0f, 1.0f);
        p->osc[i].osc_sync_enabled = rand_b();
        p->osc[i].osc_sync_softness = rand_f(0.0f, 1.0f);
        p->osc[i].ring_mod_enabled = rand_b();
        p->osc[i].ring_mod_depth = rand_f(0.0f, 1.0f);
        p->osc[i].bitcrush_enabled = rand_b();
        p->osc[i].bitcrush_depth = rand_f(0.0f, 1.0f);
    }
}

#define FLOAT_EPSILON 0.0001f

bool check_float(const char* name, float a, float b) {
    if (fabsf(a - b) > FLOAT_EPSILON) {
        printf("FAIL: %s mismatch: %f != %f\n", name, a, b);
        return false;
    }
    return true;
}

bool check_int(const char* name, int a, int b) {
    if (a != b) {
        printf("FAIL: %s mismatch: %d != %d\n", name, a, b);
        return false;
    }
    return true;
}

bool check_bool(const char* name, bool a, bool b) {
    if (a != b) {
        printf("FAIL: %s mismatch: %s != %s\n", name, a?"true":"false", b?"true":"false");
        return false;
    }
    return true;
}

bool compare_patches(const PxPatch* a, const PxPatch* b, const PxConfig* c) {
    bool ok = true;
    if (strcmp(a->name, b->name) != 0) {
        printf("FAIL: Name mismatch: '%s' != '%s'\n", a->name, b->name);
        ok = false;
    }

    ok &= check_float("filter_cutoff_hz", a->filter_cutoff_hz, b->filter_cutoff_hz);
    ok &= check_float("filter_resonance_q", a->filter_resonance_q, b->filter_resonance_q);
    ok &= check_float("filter_env_amount_hz", a->filter_env_amount_hz, b->filter_env_amount_hz);
    ok &= check_float("filter_drive", a->filter_drive, b->filter_drive);
    ok &= check_float("filter_key_track", a->filter_key_track, b->filter_key_track);
    ok &= check_int("filter_poles", a->filter_poles, b->filter_poles);
    ok &= check_int("filter_mode", a->filter_mode, b->filter_mode);
    ok &= check_float("voice_pan_setting", a->voice_pan_setting, b->voice_pan_setting);
    ok &= check_float("default_note_amp", a->default_note_amp, b->default_note_amp);
    ok &= check_float("limiter_threshold", a->limiter_threshold, b->limiter_threshold);
    ok &= check_float("limiter_release_ms", a->limiter_release_ms, b->limiter_release_ms);
    ok &= check_bool("unilegato_enabled", a->unilegato_enabled, b->unilegato_enabled);
    ok &= check_float("unilegato_slide_duration_s", a->unilegato_slide_duration_s, b->unilegato_slide_duration_s);
    ok &= check_float("pitchbend_range_semitones", a->pitchbend_range_semitones, b->pitchbend_range_semitones);

    ok &= check_bool("global_filter_enabled", a->global_filter_enabled, b->global_filter_enabled);
    ok &= check_float("global_filter_cutoff_hz", a->global_filter_cutoff_hz, b->global_filter_cutoff_hz);
    ok &= check_float("global_filter_resonance_q", a->global_filter_resonance_q, b->global_filter_resonance_q);
    ok &= check_float("global_filter_env_amount_hz", a->global_filter_env_amount_hz, b->global_filter_env_amount_hz);
    ok &= check_float("global_filter_drive", a->global_filter_drive, b->global_filter_drive);
    ok &= check_float("global_filter_key_track", a->global_filter_key_track, b->global_filter_key_track);
    ok &= check_int("global_filter_poles", a->global_filter_poles, b->global_filter_poles);
    ok &= check_int("global_filter_mode", a->global_filter_mode, b->global_filter_mode);

    ok &= check_int("velocity_curve", a->velocity_curve, b->velocity_curve);
    ok &= check_int("aftertouch_curve", a->aftertouch_curve, b->aftertouch_curve);

    ok &= check_bool("wseq_fixed_time", a->wseq_fixed_time, b->wseq_fixed_time);
    ok &= check_float("wseq_ref_freq", a->wseq_ref_freq, b->wseq_ref_freq);
    ok &= check_int("glide_mode", a->glide_mode, b->glide_mode);
    ok &= check_float("glide_time", a->glide_time, b->glide_time);
    ok &= check_bool("glide_legato_only", a->glide_legato_only, b->glide_legato_only);
    ok &= check_bool("glide_always", a->glide_always, b->glide_always);

    for (int i=0; i<c->num_voice_adsrs; i++) {
        char buf[64];
        snprintf(buf, 64, "adsr[%d].attack", i); ok &= check_float(buf, a->template_voice_adsrs[i].attack_time, b->template_voice_adsrs[i].attack_time);
        snprintf(buf, 64, "adsr[%d].decay", i); ok &= check_float(buf, a->template_voice_adsrs[i].decay_time, b->template_voice_adsrs[i].decay_time);
        snprintf(buf, 64, "adsr[%d].sustain", i); ok &= check_float(buf, a->template_voice_adsrs[i].sustain_level, b->template_voice_adsrs[i].sustain_level);
        snprintf(buf, 64, "adsr[%d].release", i); ok &= check_float(buf, a->template_voice_adsrs[i].release_time, b->template_voice_adsrs[i].release_time);
        snprintf(buf, 64, "adsr[%d].enabled", i); ok &= check_bool(buf, a->template_voice_adsrs[i].enabled, b->template_voice_adsrs[i].enabled);

        for (int j=0; j<PX_ADSR_DEST_COUNT; j++) {
            snprintf(buf, 64, "adsr_mod[%d][%d]", i, j);
            ok &= check_float(buf, a->template_voice_adsr_mod_amounts[i*PX_ADSR_DEST_COUNT + j], b->template_voice_adsr_mod_amounts[i*PX_ADSR_DEST_COUNT + j]);
        }
    }

    for (int i=0; i<c->num_lfos; i++) {
        char buf[64];
        snprintf(buf, 64, "lfo[%d].wave", i); ok &= check_int(buf, a->template_lfos[i].wave_idx, b->template_lfos[i].wave_idx);
        snprintf(buf, 64, "lfo[%d].freq", i); ok &= check_float(buf, a->template_lfos[i].frequency, b->template_lfos[i].frequency);
        snprintf(buf, 64, "lfo[%d].enabled", i); ok &= check_bool(buf, a->template_lfos[i].enabled, b->template_lfos[i].enabled);
        snprintf(buf, 64, "lfo[%d].reset", i); ok &= check_bool(buf, a->template_lfos[i].reset_on_key_on, b->template_lfos[i].reset_on_key_on);

        snprintf(buf, 64, "lfo_adsr[%d].attack", i); ok &= check_float(buf, a->template_lfos[i].adsr.attack_time, b->template_lfos[i].adsr.attack_time);
        snprintf(buf, 64, "lfo_adsr[%d].enabled", i); ok &= check_bool(buf, a->template_lfos[i].adsr.enabled, b->template_lfos[i].adsr.enabled);

        for(int j=0; j<PX_LFO_DEST_COUNT; j++) {
            snprintf(buf, 64, "lfo_mod[%d][%d]", i, j);
            ok &= check_float(buf, a->template_lfos[i].mod_amounts[j], b->template_lfos[i].mod_amounts[j]);
        }
    }

    for(int i=0; i<PX_MOD_MATRIX_SLOTS; i++) {
        char buf[64];
        snprintf(buf, 64, "matrix[%d].source", i); ok &= check_int(buf, a->mod_matrix[i].source, b->mod_matrix[i].source);
        snprintf(buf, 64, "matrix[%d].dest", i); ok &= check_int(buf, a->mod_matrix[i].dest, b->mod_matrix[i].dest);
        snprintf(buf, 64, "matrix[%d].amount", i); ok &= check_float(buf, a->mod_matrix[i].amount, b->mod_matrix[i].amount);
        snprintf(buf, 64, "matrix[%d].enabled", i); ok &= check_bool(buf, a->mod_matrix[i].enabled, b->mod_matrix[i].enabled);
    }

    for(int i=0; i<PX_MAX_OSC_PER_VOICE; i++) {
        char buf[64];
        snprintf(buf, 64, "osc[%d].enabled", i); ok &= check_bool(buf, a->osc[i].enabled, b->osc[i].enabled);
        snprintf(buf, 64, "osc[%d].wave", i); ok &= check_int(buf, a->osc[i].wave_idx, b->osc[i].wave_idx);
        snprintf(buf, 64, "osc[%d].coarse", i); ok &= check_float(buf, a->osc[i].coarse_semitones, b->osc[i].coarse_semitones);
        snprintf(buf, 64, "osc[%d].fine", i); ok &= check_float(buf, a->osc[i].fine_cents, b->osc[i].fine_cents);
        snprintf(buf, 64, "osc[%d].mix", i); ok &= check_float(buf, a->osc[i].mix_level, b->osc[i].mix_level);
        snprintf(buf, 64, "osc[%d].pan", i); ok &= check_float(buf, a->osc[i].pan, b->osc[i].pan);
        snprintf(buf, 64, "osc[%d].seq", i); ok &= check_int(buf, a->osc[i].sequence_id, b->osc[i].sequence_id);

        snprintf(buf, 64, "osc[%d].xmod_en", i); ok &= check_bool(buf, a->osc[i].cross_mod_enabled, b->osc[i].cross_mod_enabled);
        snprintf(buf, 64, "osc[%d].xmod_dp", i); ok &= check_float(buf, a->osc[i].cross_mod_depth, b->osc[i].cross_mod_depth);
        snprintf(buf, 64, "osc[%d].pd_en", i); ok &= check_bool(buf, a->osc[i].phase_dist_enabled, b->osc[i].phase_dist_enabled);
        snprintf(buf, 64, "osc[%d].pd_amt", i); ok &= check_float(buf, a->osc[i].phase_dist_amount, b->osc[i].phase_dist_amount);
        snprintf(buf, 64, "osc[%d].sync_en", i); ok &= check_bool(buf, a->osc[i].osc_sync_enabled, b->osc[i].osc_sync_enabled);
        snprintf(buf, 64, "osc[%d].sync_soft", i); ok &= check_float(buf, a->osc[i].osc_sync_softness, b->osc[i].osc_sync_softness);
        snprintf(buf, 64, "osc[%d].rm_en", i); ok &= check_bool(buf, a->osc[i].ring_mod_enabled, b->osc[i].ring_mod_enabled);
        snprintf(buf, 64, "osc[%d].rm_dp", i); ok &= check_float(buf, a->osc[i].ring_mod_depth, b->osc[i].ring_mod_depth);
        snprintf(buf, 64, "osc[%d].bc_en", i); ok &= check_bool(buf, a->osc[i].bitcrush_enabled, b->osc[i].bitcrush_enabled);
        snprintf(buf, 64, "osc[%d].bc_dp", i); ok &= check_float(buf, a->osc[i].bitcrush_depth, b->osc[i].bitcrush_depth);
    }

    return ok;
}

int main() {
    srand(12345); // Deterministic seed

    PxConfig c = {
        .num_voices = 4,
        .num_lfos = 3,
        .num_voice_adsrs = 3,
        .sample_rate = 44100.0f
    };

    printf("Creating Synth 1...\n");
    PxSynth* s1 = PX_Create(&c);
    if (!s1) { printf("Failed to create s1\n"); return 1; }

    printf("Populating Patch 1...\n");
    populate_patch(&s1->patch, &c);

    printf("Serializing s1...\n");
    MemStream stream = {0};
    if (!PX_SavePresetToBus(s1, mem_write, &stream, s1->patch.name)) {
        printf("Failed to save preset\n");
        return 1;
    }
    printf("Serialized size: %zu bytes\n", stream.size);

    printf("Creating Synth 2...\n");
    PxSynth* s2 = PX_Create(&c);
    if (!s2) { printf("Failed to create s2\n"); return 1; }

    printf("Deserializing into s2...\n");
    // Rewind stream read pos
    stream.pos = 0;
    char err[256];
    if (!PX_LoadPresetFromBus(s2, mem_read, &stream, err, 256)) {
        printf("Failed to load preset: %s\n", err);
        return 1;
    }

    printf("Comparing s1 and s2...\n");
    if (compare_patches(&s1->patch, &s2->patch, &c)) {
        printf("SUCCESS: Patches match exactly!\n");
    } else {
        printf("FAILURE: Patches differ.\n");
        return 1;
    }

    PX_Destroy(s1);
    PX_Destroy(s2);
    free(stream.buffer);
    return 0;
}
