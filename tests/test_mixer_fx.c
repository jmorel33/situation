#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <assert.h>
#include <math.h>
#include <unistd.h> // for sleep/usleep

// Simple Sine Wave Stream
typedef struct {
    double phase;
    double freq;
    double sampleRate;
} SineUserData;

ma_uint64 on_read_sine(void* pUserData, void* pBufferOut, ma_uint64 frameCount) {
    SineUserData* sine = (SineUserData*)pUserData;
    float* pOut = (float*)pBufferOut;
    for (ma_uint64 i = 0; i < frameCount; ++i) {
        float sample = (float)sin(sine->phase);
        pOut[i*2] = sample;     // Left
        pOut[i*2+1] = sample;   // Right
        sine->phase += 2.0 * M_PI * sine->freq / sine->sampleRate;
        if (sine->phase > 2.0 * M_PI) sine->phase -= 2.0 * M_PI;
    }
    return frameCount;
}

int main() {
    SituationInitInfo init = {0};
    init.window_title = "Mixer Test";
    init.window_width = 100;
    init.window_height = 100;
    if (SituationInit(0, NULL, &init) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to init\n");
        return 1;
    }

    // 1. Create Mixer
    SituationAudioMixer* mixer = SituationCreateMixer();
    assert(mixer != NULL);
    // Bind to default device
    SituationBindMixerToDevice(mixer, NULL, 2);

    // 2. Add Track
    SituationAudioTrack* track = SituationAddTrack(mixer, "Test Track");
    assert(track != NULL);
    SituationSetTrackVolume(track, 1.0f);

    // 3. Load Sound (Sine Wave)
    SineUserData sineData = {0, 440.0, 48000.0};
    SituationSoundHandle sound;
    SituationAudioFormat fmt = {48000, 2, 32};
    SituationError err = SituationLoadSoundFromStream(on_read_sine, NULL, &sineData, &fmt, true, &sound);
    assert(err == SITUATION_SUCCESS);

    // 4. Route
    err = SituationRouteSoundToTrack(sound, track);
    assert(err == SITUATION_SUCCESS);

    // 5. Play
    SituationPlayAudio(sound);

    // 6. Metering Check
    float peakL = 0, peakR = 0;
    int timeout = 100;
    while (timeout > 0) {
        SituationPollInputEvents();
        SituationGetTrackMeter(track, &peakL, &peakR, NULL);
        if (peakL > 0.001f) break;
        usleep(10000); // 10ms
        timeout--;
    }
    printf("Peak L: %f, Peak R: %f\n", peakL, peakR);
    assert(peakL > 0.0f);
    assert(peakR > 0.0f);

    // 7. FX Insert Test (Aux Bus 0)
    SituationAudioBus* aux = SituationGetAuxBus(mixer, 0);
    assert(aux != NULL);

    ma_node_graph* graph = SituationGetMixerGraph(mixer);
    assert(graph != NULL);

    // Create a Low Pass Filter Node manually
    ma_lpf_node* lpf = (ma_lpf_node*)malloc(sizeof(ma_lpf_node));
    ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(2, 48000, 500, 0); // 500Hz, default order
    ma_result res = ma_lpf_node_init(graph, &lpfConfig, NULL, lpf);
    assert(res == MA_SUCCESS);

    // Insert into Aux 0 Slot 0
    err = SituationInsertEffect(aux, 0, (ma_node*)lpf);
    assert(err == SITUATION_SUCCESS);

    // Route Track to Aux 0 (Post-Fader)
    err = SituationSetTrackSend(track, 0, 1.0f, false);
    assert(err == SITUATION_SUCCESS);

    // Let it run for a bit to ensure stability
    usleep(100000); // 100ms

    // Remove Effect
    void* removed = SituationRemoveEffect(aux, 0);
    assert(removed == (void*)lpf);

    // Uninit node
    ma_lpf_node_uninit(lpf, NULL);
    free(lpf);

    // Cleanup
    SituationUnloadSound(&sound);
    SituationDestroyMixer(mixer);
    SituationShutdown();
    printf("Test Passed!\n");
    return 0;
}
