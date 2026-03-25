#include <stdio.h>
#include <stdbool.h>

typedef struct {
    float coarse_semitones;
    float fine_cents;
    float mix_level;
    float pan;

    float cross_mod_depth;
    float phase_dist_amount;
    float osc_sync_softness;
    float ring_mod_depth;
    float bitcrush_depth;

    int   wave_idx;
    int   sequence_id;

    bool  enabled;
    bool  cross_mod_enabled;
    bool  phase_dist_enabled;
    bool  osc_sync_enabled;
    bool  ring_mod_enabled;
    bool  bitcrush_enabled;
} PxOscillator_Raw;

int main() {
    printf("Raw size: %zu\n", sizeof(PxOscillator_Raw));
    return 0;
}
