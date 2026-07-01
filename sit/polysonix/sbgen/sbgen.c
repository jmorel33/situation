/*
 * sbgen.c — CLI driver for creating .sbr sample bank files
 *
 * Reads a directory tree of .wav files (AKWF format) and packs them
 * into a single .sbr file using the px_samplebank API.
 *
 * Usage: sbgen <input_dir> <output.sbr>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "px_samplebank.h"

#define MAX_PATH_LEN 512

/* ── Minimal WAV reader (PCM 16/24-bit, mono/stereo) ─────────────────────── */

typedef struct {
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;   /* original bit depth from file */
    uint32_t num_samples;       /* frames (per channel) */
    int16_t *data;              /* always PCM16 output — caller must free */
} WavData;

/* return code for wav_read */
#define WAV_OK          0
#define WAV_ERR_OPEN    1
#define WAV_ERR_NOTRIFF 2
#define WAV_ERR_NOFMT   3
#define WAV_ERR_COMPRESSED 4
#define WAV_ERR_UNSUPPORTED_BITS 5
#define WAV_ERR_UNSUPPORTED_CH   6
#define WAV_ERR_ALLOC   7

static int wav_read(const char *path, WavData *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return WAV_ERR_OPEN;

    char riff[4]; uint32_t file_size; char wave[4];
    fread(riff, 1, 4, f);
    fread(&file_size, 4, 1, f);
    fread(wave, 1, 4, f);
    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
        fclose(f); return WAV_ERR_NOTRIFF;
    }

    uint16_t channels = 0, bits = 0;
    uint32_t sample_rate = 0, data_size = 0;
    bool got_fmt = false, got_data = false;

    while (!got_data && !feof(f)) {
        char chunk_id[4]; uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4) break;
        if (fread(&chunk_size, 4, 1, f) != 1) break;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t audio_fmt;
            fread(&audio_fmt, 2, 1, f);
            fread(&channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            fseek(f, 6, SEEK_CUR);
            fread(&bits, 2, 1, f);
            long remaining = (long)chunk_size - 16;
            if (remaining > 0) fseek(f, remaining, SEEK_CUR);
            if (audio_fmt != 1) { fclose(f); return WAV_ERR_COMPRESSED; }
            got_fmt = true;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            got_data = true;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }

    if (!got_fmt || !got_data) { fclose(f); return WAV_ERR_NOFMT; }
    if (channels != 1 && channels != 2) { fclose(f); return WAV_ERR_UNSUPPORTED_CH; }

    out->channels = channels;
    out->sample_rate = sample_rate;
    out->bits_per_sample = bits;

    if (bits == 16) {
        uint32_t total_i16 = data_size / 2;
        uint32_t num_frames = total_i16 / channels;
        int16_t *data = (int16_t *)malloc(total_i16 * sizeof(int16_t));
        if (!data) { fclose(f); return WAV_ERR_ALLOC; }
        fread(data, 2, total_i16, f);
        fclose(f);
        out->num_samples = num_frames;
        out->data = data;
        return WAV_OK;
    }

    if (bits == 24) {
        /* 24-bit PCM: 3 bytes per sample, little-endian signed.
           Convert to 16-bit by taking the top 16 bits (>> 8). */
        uint32_t total_samples_24 = data_size / 3;
        uint32_t num_frames = total_samples_24 / channels;
        uint8_t *raw = (uint8_t *)malloc(data_size);
        int16_t *data = (int16_t *)malloc(total_samples_24 * sizeof(int16_t));
        if (!raw || !data) { free(raw); free(data); fclose(f); return WAV_ERR_ALLOC; }
        fread(raw, 1, data_size, f);
        fclose(f);
        for (uint32_t i = 0; i < total_samples_24; i++) {
            /* reconstruct signed 24-bit value from 3 LE bytes */
            int32_t s = (int32_t)(raw[i*3] | (raw[i*3+1] << 8) | (raw[i*3+2] << 16));
            if (s & 0x800000) s |= 0xFF000000;  /* sign-extend */
            data[i] = (int16_t)(s >> 8);
        }
        free(raw);
        out->num_samples = num_frames;
        out->data = data;
        return WAV_OK;
    }

    fclose(f);
    return WAV_ERR_UNSUPPORTED_BITS;
}

/* ── Scan a group directory and add its .wav files to the bank ───────────── */

static uint16_t g_loop_mode = SBR_FMT_LOOP;  /* default; overridden by --oneshot */

static bool scan_group_dir(SBR_Bank *bank, const char *dirpath, const char *group_name) {
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "warning: cannot open %s\n", dirpath);
        return true;
    }

    int gidx = sbr_group_add(bank, group_name);
    if (gidx < 0) { closedir(d); return false; }

    if (strlen(group_name) >= SBR_NAME_MAX) {
        fprintf(stderr, "  note: group name truncated: \"%s\" -> \"%s\"\n",
                group_name, bank->groups[gidx].name);
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len < 5) continue;
        if (strcmp(name + len - 4, ".wav") != 0 &&
            strcmp(name + len - 4, ".WAV") != 0) continue;

        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, name);

        /* sample name = filename without .wav */
        char sample_name[SBR_NAME_MAX];
        size_t nlen = len - 4;
        bool name_truncated = (nlen >= SBR_NAME_MAX);
        if (nlen >= SBR_NAME_MAX) nlen = SBR_NAME_MAX - 1;
        memcpy(sample_name, name, nlen);
        sample_name[nlen] = '\0';

        WavData wav;
        int rc = wav_read(filepath, &wav);
        if (rc != WAV_OK) {
            const char *reason = "unknown error";
            switch (rc) {
                case WAV_ERR_OPEN:       reason = "cannot open file"; break;
                case WAV_ERR_NOTRIFF:    reason = "not a WAV file"; break;
                case WAV_ERR_NOFMT:      reason = "missing fmt/data chunk"; break;
                case WAV_ERR_COMPRESSED: reason = "compressed (not PCM)"; break;
                case WAV_ERR_UNSUPPORTED_BITS: reason = "unsupported bit depth (not 16 or 24)"; break;
                case WAV_ERR_UNSUPPORTED_CH:   reason = "unsupported channel count (not 1 or 2)"; break;
                case WAV_ERR_ALLOC:      reason = "out of memory"; break;
            }
            fprintf(stderr, "warning: skipping %s (%s)\n", filepath, reason);
            continue;
        }

        if (wav.bits_per_sample == 24) {
            fprintf(stderr, "  note: %s converted 24-bit -> 16-bit\n", name);
        }

        uint16_t fmt_flags = SBR_FMT_PCM16 | g_loop_mode
                           | (wav.channels == 2 ? SBR_FMT_STEREO : SBR_FMT_MONO);
        uint32_t raw_count = wav.num_samples * wav.channels; /* total int16 values to store */

        int eidx = sbr_entry_add(bank, gidx, sample_name,
                                  wav.data, raw_count, wav.sample_rate,
                                  fmt_flags, 26, 2);
        free(wav.data);

        if (eidx < 0) {
            fprintf(stderr, "error: failed to add entry %s\n", sample_name);
            closedir(d);
            return false;
        }

        if (name_truncated) {
            fprintf(stderr, "  note: sample name truncated: \"%.*s\" -> \"%s\"\n",
                    (int)(len - 4), name, sample_name);
        }
    }

    closedir(d);
    return true;
}

/* ── Scan root for group subdirectories ──────────────────────────────────── */

static bool scan_root(SBR_Bank *bank, const char *root) {
    DIR *d = opendir(root);
    if (!d) {
        fprintf(stderr, "error: cannot open %s\n", root);
        return false;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char subpath[MAX_PATH_LEN];
        snprintf(subpath, sizeof(subpath), "%s/%s", root, ent->d_name);

        struct stat st;
        if (stat(subpath, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (!scan_group_dir(bank, subpath, ent->d_name)) {
            closedir(d);
            return false;
        }
    }

    closedir(d);
    return true;
}

/* ── List command: dump the TOC of an .sbr file ──────────────────────────── */

static int cmd_list(const char *path) {
    SBR_Bank *bank = sbr_bank_read(path);
    if (!bank) {
        fprintf(stderr, "error: cannot read %s\n", path);
        return 1;
    }

    printf("Sample Bank: %s\n", path);
    printf("Groups: %d  Entries: %d  Samples: %u\n\n",
           bank->group_count, bank->entry_count, bank->sample_total);

    for (int g = 0; g < bank->group_count; g++) {
        SBR_Group *grp = &bank->groups[g];
        printf("[%3d] %s (%u entries)\n", g, grp->name, grp->entry_count);

        for (int i = 0; i < bank->entry_count; i++) {
            SBR_Entry *e = &bank->entries[i];
            if ((int)e->group_index != g) continue;

            const char *ch  = (e->flags & SBR_FMT_STEREO) ? "stereo" : "mono";
            const char *lp  = (e->flags & SBR_FMT_LOOP) ? "loop" :
                              (e->flags & SBR_FMT_PINGPONG) ? "ppong" : "1shot";
            printf("      [%4d] %-31s %6u smp  %5uHz  %s %s  note=%u ft=%d\n",
                   i, e->name, e->sample_length, e->sample_rate,
                   ch, lp, e->base_note, e->fine_tune);
        }
        printf("\n");
    }

    sbr_bank_destroy(bank);
    return 0;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

static void print_usage(void) {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  sbgen create [--oneshot] <input_dir> <output.sbr>  — build bank from WAV directory\n");
    fprintf(stderr, "  sbgen list   <file.sbr>                 — list bank contents\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) { print_usage(); return 1; }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 3) { print_usage(); return 1; }
        return cmd_list(argv[2]);
    }

    if (strcmp(argv[1], "create") == 0) {
        /* parse optional flags */
        int arg_idx = 2;
        while (arg_idx < argc && argv[arg_idx][0] == '-') {
            if (strcmp(argv[arg_idx], "--oneshot") == 0) {
                g_loop_mode = SBR_FMT_ONESHOT;
            } else {
                fprintf(stderr, "unknown flag: %s\n", argv[arg_idx]);
                return 1;
            }
            arg_idx++;
        }
        if (argc - arg_idx != 2) { print_usage(); return 1; }
        const char *input_dir  = argv[arg_idx];
        const char *output_sbr = argv[arg_idx + 1];

        SBR_Bank *bank = sbr_bank_create();
        if (!bank) { fprintf(stderr, "error: out of memory\n"); return 1; }

        printf("sbgen: scanning %s ...\n", input_dir);

        if (!scan_root(bank, input_dir)) {
            sbr_bank_destroy(bank);
            return 1;
        }

        printf("sbgen: %d groups, %d samples, %u total sample frames\n",
               bank->group_count, bank->entry_count, bank->sample_total);

        if (bank->entry_count == 0) {
            fprintf(stderr, "error: no samples found\n");
            sbr_bank_destroy(bank);
            return 1;
        }

        if (!sbr_bank_write(bank, output_sbr)) {
            fprintf(stderr, "error: failed to write %s\n", output_sbr);
            sbr_bank_destroy(bank);
            return 1;
        }

        printf("sbgen: wrote %s\n", output_sbr);
        sbr_bank_destroy(bank);
        return 0;
    }

    print_usage();
    return 1;
}
