#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <string.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

#define NUM_PRODUCERS 4
#define ITEMS_PER_PRODUCER 10000

// We need to access the queue from the synth.
PxSynth* synth;

void* producer(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        // We use key_id to store a unique value: id * 100000 + i
        int val = id * 100000 + i;
        PxCommand cmd;
        memset(&cmd, 0, sizeof(PxCommand));
        cmd.command_type = PX_CMD_NOTE_ON;
        cmd.data.note_on.midi_note = 60;
        cmd.data.note_on.wave_idx = 0;
        cmd.data.note_on.key_id = val;

        // Spin until pushed
        while (!cmd_push(&synth->cmd_queue, cmd)) {
            // sched_yield();
        }
    }
    return NULL;
}

void* consumer(void* arg) {
    int received_counts[NUM_PRODUCERS] = {0};
    int total_received = 0;
    int expected_total = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    while (total_received < expected_total) {
        PxCommand cmd;
        if (cmd_pop(&synth->cmd_queue, &cmd)) {
            if (cmd.command_type != PX_CMD_NOTE_ON) {
                 fprintf(stderr, "Error: Invalid command type %d\n", cmd.command_type);
                 exit(1);
            }
            int val = cmd.data.note_on.key_id;
            int producer_id = val / 100000;
            int seq = val % 100000;

            if (producer_id < 0 || producer_id >= NUM_PRODUCERS) {
                 fprintf(stderr, "Error: Invalid producer id %d (val=%d)\n", producer_id, val);
                 exit(1);
            }

            // Note: With lock-free MPSC, order per producer should be preserved?
            // Yes.
            if (seq != received_counts[producer_id]) {
                 fprintf(stderr, "Error: Producer %d sequence mismatch. Expected %d, got %d\n", producer_id, received_counts[producer_id], seq);
                 exit(1);
            }
            received_counts[producer_id]++;
            total_received++;
        } else {
             // Empty queue, spin
        }
    }
    return NULL;
}

int main() {
    printf("Starting concurrency test...\n");

    // Create synth (allocates queue)
    PxConfig config = {
        .num_voices=4,
        .sample_rate=44100,
        .num_lfos = 1,
        .num_voice_adsrs = 1,
        .lfo_update_interval_ms = 1.0f
    };
    synth = PX_Create(&config);
    if(!synth) { printf("Failed to create synth\n"); return 1; }

    pthread_t producers[NUM_PRODUCERS];
    int ids[NUM_PRODUCERS];
    pthread_t cons;

    if (pthread_create(&cons, NULL, consumer, NULL) != 0) {
        perror("Failed to create consumer thread");
        return 1;
    }

    for(int i=0; i<NUM_PRODUCERS; i++) {
        ids[i] = i;
        if (pthread_create(&producers[i], NULL, producer, &ids[i]) != 0) {
            perror("Failed to create producer thread");
            return 1;
        }
    }

    for(int i=0; i<NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    pthread_join(cons, NULL);

    PX_Destroy(synth);
    printf("Concurrency test PASSED.\n");
    return 0;
}
