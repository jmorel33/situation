#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

int main() {
    printf("Initializing...\n");
    SituationInitInfo info = {0};
    if (SituationInit(0, NULL, &info) != SITUATION_SUCCESS) {
        printf("Init failed\n");
        return 1;
    }

    printf("Creating Mixer 1...\n");
    SituationAudioMixer* mixer = SituationCreateMixer();
    if (!mixer) { printf("Mixer creation failed\n"); return 1; }

    printf("Configuring Track 0...\n");
    SituationAudioTrack* t0 = SituationAddTrack(mixer, "Kick Drum");
    SituationSetTrackVolume(t0, 0.75f);
    SituationSetTrackPan(t0, -0.5f);

    // Set EQ
    float freqs[4] = {50.0f, 200.0f, 2000.0f, 10000.0f};
    float gains[4] = {0.0f, 3.0f, -2.0f, 1.0f}; // HPF gain ignored
    float Qs[4] = {0.0f, 0.7f, 1.0f, 0.7f}; // HPF Q ignored
    SituationSetTrackEQ(t0, true, freqs, gains, Qs);

    printf("Saving Session...\n");
    if (!SituationSaveMixerSession(mixer, "test_session.smx")) {
        printf("Save failed\n");
        return 1;
    }

    printf("Destroying Mixer 1...\n");
    SituationDestroyMixer(mixer);

    printf("Creating Mixer 2...\n");
    mixer = SituationCreateMixer(); // New instance

    printf("Loading Session...\n");
    if (!SituationLoadMixerSession(mixer, "test_session.smx")) {
        printf("Load failed\n");
        return 1;
    }

    printf("Verifying State...\n");

    // Check Track 0
    SituationAudioTrack* loaded_t0 = &mixer->tracks[0];

    printf("Name: %s (Expected: Kick Drum)\n", loaded_t0->name);
    if (strcmp(loaded_t0->name, "Kick Drum") != 0) { printf("FAIL: Name mismatch\n"); return 1; }

    float vol = atomic_load(&loaded_t0->volume);
    printf("Volume: %f (Expected: 0.75)\n", vol);
    if (fabs(vol - 0.75f) > 0.001f) { printf("FAIL: Volume mismatch\n"); return 1; }

    float pan = atomic_load(&loaded_t0->pan);
    printf("Pan: %f (Expected: -0.5)\n", pan);
    if (fabs(pan + 0.5f) > 0.001f) { printf("FAIL: Pan mismatch\n"); return 1; }

    printf("EQ Enabled: %d (Expected: 1)\n", loaded_t0->eq_state.enabled);
    if (!loaded_t0->eq_state.enabled) { printf("FAIL: EQ not enabled\n"); return 1; }

    printf("EQ Peak Freq: %f (Expected: 2000.0)\n", loaded_t0->eq_state.peak_freq);
    if (fabs(loaded_t0->eq_state.peak_freq - 2000.0f) > 0.1f) { printf("FAIL: EQ Freq mismatch\n"); return 1; }

    printf("Success!\n");
    SituationDestroyMixer(mixer);
    SituationShutdown();
    remove("test_session.smx");
    return 0;
}
