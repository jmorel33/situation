/*
 * px_samplebank.h — Sample Bank ROM (.sbr) format and API
 *
 * Standalone header. C11. No external dependencies beyond stdlib/stdint/string.
 *
 * .sbr layout:
 *   [SBR_Header]                  — magic, version, counts, offsets
 *   [SBR_Group] × group_count     — group table (folder names)
 *   [SBR_Entry] × entry_count     — sample TOC
 *   [sample data blob]            — packed raw samples
 *
 * Usage:
 *   #define PX_SAMPLEBANK_IMPLEMENTATION in exactly one .c file before including.
 */
#ifndef PX_SAMPLEBANK_H
#define PX_SAMPLEBANK_H

#include <stdint.h>
#include <stdbool.h>

#define SBR_MAGIC       0x53425200  /* "SBR\0" */
#define SBR_VERSION     1
#define SBR_NAME_MAX    40          /* including NUL terminator */

/* --- Format bitfield flags --- */
/* sample depth (bits 0-3) */
#define SBR_FMT_PCM8       (1u << 0)    /* 8-bit unsigned PCM */
#define SBR_FMT_PCM12      (1u << 1)    /* 12-bit signed, stored in 16-bit containers */
#define SBR_FMT_PCM16      (1u << 2)    /* 16-bit signed PCM */
#define SBR_FMT_FLOAT32    (1u << 3)    /* 32-bit IEEE float */
/* channel layout (bits 4-5) */
#define SBR_FMT_MONO       (1u << 4)
#define SBR_FMT_STEREO     (1u << 5)    /* interleaved */
/* loop mode (bits 6-8) */
#define SBR_FMT_LOOP       (1u << 6)
#define SBR_FMT_PINGPONG   (1u << 7)
#define SBR_FMT_ONESHOT    (1u << 8)

/* ── On-disk structs ─────────────────────────────────────────────────────── */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t group_count;
    uint32_t entry_count;
    uint32_t data_offset;   /* byte offset from file start to sample data blob */
} SBR_Header;

typedef struct {
    char     name[SBR_NAME_MAX];
    uint32_t first_entry;   /* index of first SBR_Entry in this group */
    uint32_t entry_count;
} SBR_Group;

typedef struct {
    char     name[SBR_NAME_MAX];
    uint32_t group_index;   /* index into SBR_Group table */
    uint32_t sample_begin;  /* offset in samples from start of data blob */
    uint32_t sample_length;
    uint32_t loop_start;    /* relative to sample_begin */
    uint32_t loop_end;      /* relative to sample_begin */
    uint32_t sample_rate;
    uint8_t  base_note;     /* MIDI note number */
    int8_t   fine_tune;     /* cents, -128..+127 */
    uint16_t flags;         /* SBR_FMT_* */
} SBR_Entry;

/* ── In-memory bank handle ───────────────────────────────────────────────── */

typedef struct {
    SBR_Group  *groups;
    int         group_count;
    int         group_cap;

    SBR_Entry  *entries;
    int         entry_count;
    int         entry_cap;

    int16_t    *samples;        /* PCM16 sample data blob */
    uint32_t    sample_total;   /* total samples stored */
    uint32_t    sample_cap;
} SBR_Bank;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Lifecycle */
SBR_Bank *sbr_bank_create(void);
void      sbr_bank_destroy(SBR_Bank *bank);

/* Groups */
int       sbr_group_add(SBR_Bank *bank, const char *name);
bool      sbr_group_remove(SBR_Bank *bank, int group_index);
bool      sbr_group_rename(SBR_Bank *bank, int group_index, const char *new_name);
int       sbr_group_find(const SBR_Bank *bank, const char *name);

/* Entries */
int       sbr_entry_add(SBR_Bank *bank, int group_index, const char *name,
                         const int16_t *pcm_data, uint32_t num_samples,
                         uint32_t sample_rate, uint16_t flags,
                         uint8_t base_note, int8_t fine_tune);
bool      sbr_entry_remove(SBR_Bank *bank, int entry_index);
bool      sbr_entry_rename(SBR_Bank *bank, int entry_index, const char *new_name);
bool      sbr_entry_set_loop(SBR_Bank *bank, int entry_index,
                              uint32_t loop_start, uint32_t loop_end);

/* I/O */
bool      sbr_bank_write(const SBR_Bank *bank, const char *path);
SBR_Bank *sbr_bank_read(const char *path);

#endif /* PX_SAMPLEBANK_H */
