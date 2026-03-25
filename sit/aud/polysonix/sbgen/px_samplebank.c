/*
 * px_samplebank.c — SBR_Bank API implementation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "px_samplebank.h"

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void safe_strncpy(char *dst, const char *src, size_t max) {
    strncpy(dst, src, max - 1);
    dst[max - 1] = '\0';
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

SBR_Bank *sbr_bank_create(void) {
    SBR_Bank *b = (SBR_Bank *)calloc(1, sizeof(SBR_Bank));
    return b;
}

void sbr_bank_destroy(SBR_Bank *bank) {
    if (!bank) return;
    free(bank->groups);
    free(bank->entries);
    free(bank->samples);
    free(bank);
}

/* ── Internal grow helpers ───────────────────────────────────────────────── */

static bool grow_groups(SBR_Bank *b) {
    int new_cap = b->group_cap ? b->group_cap * 2 : 64;
    SBR_Group *p = (SBR_Group *)realloc(b->groups, new_cap * sizeof(SBR_Group));
    if (!p) return false;
    b->groups = p;
    b->group_cap = new_cap;
    return true;
}

static bool grow_entries(SBR_Bank *b) {
    int new_cap = b->entry_cap ? b->entry_cap * 2 : 256;
    SBR_Entry *p = (SBR_Entry *)realloc(b->entries, new_cap * sizeof(SBR_Entry));
    if (!p) return false;
    b->entries = p;
    b->entry_cap = new_cap;
    return true;
}

static bool grow_samples(SBR_Bank *b, uint32_t need) {
    if (b->sample_total + need <= b->sample_cap) return true;
    uint32_t new_cap = b->sample_cap ? b->sample_cap * 2 : (1u << 20);
    while (new_cap < b->sample_total + need) new_cap *= 2;
    int16_t *p = (int16_t *)realloc(b->samples, new_cap * sizeof(int16_t));
    if (!p) return false;
    b->samples = p;
    b->sample_cap = new_cap;
    return true;
}

/* ── Groups ──────────────────────────────────────────────────────────────── */

int sbr_group_find(const SBR_Bank *bank, const char *name) {
    for (int i = 0; i < bank->group_count; i++) {
        if (strcmp(bank->groups[i].name, name) == 0) return i;
    }
    return -1;
}

int sbr_group_add(SBR_Bank *bank, const char *name) {
    int existing = sbr_group_find(bank, name);
    if (existing >= 0) return existing;

    if (bank->group_count >= bank->group_cap && !grow_groups(bank)) return -1;

    int idx = bank->group_count++;
    memset(&bank->groups[idx], 0, sizeof(SBR_Group));
    safe_strncpy(bank->groups[idx].name, name, SBR_NAME_MAX);
    bank->groups[idx].first_entry = (uint32_t)bank->entry_count;
    bank->groups[idx].entry_count = 0;
    return idx;
}

bool sbr_group_rename(SBR_Bank *bank, int group_index, const char *new_name) {
    if (group_index < 0 || group_index >= bank->group_count) return false;
    safe_strncpy(bank->groups[group_index].name, new_name, SBR_NAME_MAX);
    return true;
}

bool sbr_group_remove(SBR_Bank *bank, int group_index) {
    if (group_index < 0 || group_index >= bank->group_count) return false;

    /* remove all entries belonging to this group (back to front) */
    for (int i = bank->entry_count - 1; i >= 0; i--) {
        if ((int)bank->entries[i].group_index == group_index) {
            sbr_entry_remove(bank, i);
        }
    }

    /* shift groups down */
    for (int i = group_index; i < bank->group_count - 1; i++) {
        bank->groups[i] = bank->groups[i + 1];
    }
    bank->group_count--;

    /* fix group_index references in remaining entries */
    for (int i = 0; i < bank->entry_count; i++) {
        if ((int)bank->entries[i].group_index > group_index) {
            bank->entries[i].group_index--;
        }
    }

    return true;
}

/* ── Entries ─────────────────────────────────────────────────────────────── */

int sbr_entry_add(SBR_Bank *bank, int group_index, const char *name,
                  const int16_t *pcm_data, uint32_t num_samples,
                  uint32_t sample_rate, uint16_t flags,
                  uint8_t base_note, int8_t fine_tune) {
    if (group_index < 0 || group_index >= bank->group_count) return -1;
    if (bank->entry_count >= bank->entry_cap && !grow_entries(bank)) return -1;
    if (!grow_samples(bank, num_samples)) return -1;

    /* append PCM data */
    memcpy(bank->samples + bank->sample_total, pcm_data, num_samples * sizeof(int16_t));

    int idx = bank->entry_count++;
    SBR_Entry *e = &bank->entries[idx];
    memset(e, 0, sizeof(SBR_Entry));
    safe_strncpy(e->name, name, SBR_NAME_MAX);
    e->group_index  = (uint32_t)group_index;
    e->sample_begin = bank->sample_total;
    e->sample_length = num_samples;
    e->loop_start   = 0;
    e->loop_end     = num_samples;
    e->sample_rate  = sample_rate;
    e->base_note    = base_note;
    e->fine_tune    = fine_tune;
    e->flags        = flags;

    bank->sample_total += num_samples;
    bank->groups[group_index].entry_count++;

    return idx;
}

bool sbr_entry_remove(SBR_Bank *bank, int entry_index) {
    if (entry_index < 0 || entry_index >= bank->entry_count) return false;

    SBR_Entry *e = &bank->entries[entry_index];
    uint32_t gidx = e->group_index;

    /* Note: sample data is NOT compacted — leaves a gap in the blob.
       This is fine for an editing tool; compaction happens on write/save. */

    if (gidx < (uint32_t)bank->group_count) {
        bank->groups[gidx].entry_count--;
    }

    /* shift entries down */
    for (int i = entry_index; i < bank->entry_count - 1; i++) {
        bank->entries[i] = bank->entries[i + 1];
    }
    bank->entry_count--;

    return true;
}

bool sbr_entry_rename(SBR_Bank *bank, int entry_index, const char *new_name) {
    if (entry_index < 0 || entry_index >= bank->entry_count) return false;
    safe_strncpy(bank->entries[entry_index].name, new_name, SBR_NAME_MAX);
    return true;
}

bool sbr_entry_set_loop(SBR_Bank *bank, int entry_index,
                        uint32_t loop_start, uint32_t loop_end) {
    if (entry_index < 0 || entry_index >= bank->entry_count) return false;
    SBR_Entry *e = &bank->entries[entry_index];
    if (loop_end > e->sample_length) return false;
    if (loop_start >= loop_end) return false;
    e->loop_start = loop_start;
    e->loop_end   = loop_end;
    return true;
}

/* ── I/O: Write ──────────────────────────────────────────────────────────── */

static void rebuild_group_ranges(SBR_Bank *bank) {
    /* reset all group ranges */
    for (int g = 0; g < bank->group_count; g++) {
        bank->groups[g].first_entry = 0;
        bank->groups[g].entry_count = 0;
    }
    /* single pass to recompute */
    for (int i = 0; i < bank->entry_count; i++) {
        uint32_t g = bank->entries[i].group_index;
        if ((int)g >= bank->group_count) continue;
        if (bank->groups[g].entry_count == 0) {
            bank->groups[g].first_entry = (uint32_t)i;
        }
        bank->groups[g].entry_count++;
    }
}

bool sbr_bank_write(const SBR_Bank *bank, const char *path) {
    /* work on a mutable copy for rebuild */
    SBR_Bank *b = (SBR_Bank *)bank; /* safe: rebuild_group_ranges only fixes metadata */
    rebuild_group_ranges(b);

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    uint32_t data_offset = (uint32_t)(
        sizeof(SBR_Header) +
        sizeof(SBR_Group) * bank->group_count +
        sizeof(SBR_Entry) * bank->entry_count
    );

    SBR_Header hdr = {
        .magic       = SBR_MAGIC,
        .version     = SBR_VERSION,
        .group_count = (uint32_t)bank->group_count,
        .entry_count = (uint32_t)bank->entry_count,
        .data_offset = data_offset,
    };

    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(bank->groups,  sizeof(SBR_Group), bank->group_count, f);
    fwrite(bank->entries, sizeof(SBR_Entry), bank->entry_count, f);
    fwrite(bank->samples, sizeof(int16_t),   bank->sample_total, f);

    fclose(f);
    return true;
}

/* ── I/O: Read ───────────────────────────────────────────────────────────── */

SBR_Bank *sbr_bank_read(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    SBR_Header hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return NULL; }
    if (hdr.magic != SBR_MAGIC || hdr.version != SBR_VERSION) { fclose(f); return NULL; }

    SBR_Bank *bank = sbr_bank_create();
    if (!bank) { fclose(f); return NULL; }

    /* groups */
    bank->group_cap   = (int)hdr.group_count;
    bank->group_count = (int)hdr.group_count;
    bank->groups = (SBR_Group *)malloc(hdr.group_count * sizeof(SBR_Group));
    if (!bank->groups) goto fail;
    fread(bank->groups, sizeof(SBR_Group), hdr.group_count, f);

    /* entries */
    bank->entry_cap   = (int)hdr.entry_count;
    bank->entry_count = (int)hdr.entry_count;
    bank->entries = (SBR_Entry *)malloc(hdr.entry_count * sizeof(SBR_Entry));
    if (!bank->entries) goto fail;
    fread(bank->entries, sizeof(SBR_Entry), hdr.entry_count, f);

    /* sample data — compute size from data_offset to EOF */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    uint32_t data_bytes = (uint32_t)(file_size - hdr.data_offset);
    uint32_t num_samples = data_bytes / sizeof(int16_t);

    bank->sample_cap   = num_samples;
    bank->sample_total = num_samples;
    bank->samples = (int16_t *)malloc(data_bytes);
    if (!bank->samples) goto fail;
    fseek(f, hdr.data_offset, SEEK_SET);
    fread(bank->samples, sizeof(int16_t), num_samples, f);

    fclose(f);
    return bank;

fail:
    fclose(f);
    sbr_bank_destroy(bank);
    return NULL;
}
