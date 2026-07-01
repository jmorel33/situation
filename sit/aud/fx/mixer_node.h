/***************************************************************************************************
*
*   sit/aud/fx/mixer_node.h - Bus Summing Mixer Node
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Sums N stereo input ports to 1 stereo output. Used as the bus summing point in the
*   node graph signal path. Multiple mixer nodes can create sub-bus hierarchies
*   (drums bus, vocals bus, etc.) converging to a master mixer.
*
*   Controls:
*     [0] master_gain — overall bus level (0.0 to 4.0, default 1.0)
*
*   Audio Ports:
*     Inputs:  16 stereo (max channels that can feed in)
*     Outputs: 1 stereo (summed result)
*
***************************************************************************************************/

#ifndef SITUATION_MIXER_NODE_H
#define SITUATION_MIXER_NODE_H

#define SITUATION_MIXER_MAX_INPUTS 16

// ================================================================================================
// STATE
// ================================================================================================

typedef struct {
    float master_gain;    // Overall bus level (default 1.0)
} SituationMixerNodeState;

// ================================================================================================
// FUNCTIONS
// ================================================================================================

static inline void situation_mixer_node_init(SituationMixerNodeState* state) {
    state->master_gain = 1.0f;
}

/**
 * @brief Sum all input port buffers to a single stereo output.
 * @param state Mixer state.
 * @param inputs Array of input audio ports (up to SITUATION_MIXER_MAX_INPUTS).
 * @param num_inputs Number of input ports allocated.
 * @param output Output audio port buffer (stereo interleaved).
 * @param frames Number of frames to process.
 * @param channels Number of channels (typically 2 for stereo).
 * @param master_gain Overall gain applied after summing.
 */
static inline void situation_mixer_node_process(
    SituationMixerNodeState* state,
    float** input_buffers,
    int num_inputs,
    float* output,
    int frames,
    int channels,
    float master_gain
) {
    (void)state;
    int total_samples = frames * channels;
    
    // Zero output
    for (int i = 0; i < total_samples; i++) {
        output[i] = 0.0f;
    }
    
    // Sum all inputs
    for (int inp = 0; inp < num_inputs; inp++) {
        if (!input_buffers[inp]) continue;
        
        for (int i = 0; i < total_samples; i++) {
            output[i] += input_buffers[inp][i];
        }
    }
    
    // Apply master gain
    if (master_gain != 1.0f) {
        for (int i = 0; i < total_samples; i++) {
            output[i] *= master_gain;
        }
    }
}

#endif // SITUATION_MIXER_NODE_H
