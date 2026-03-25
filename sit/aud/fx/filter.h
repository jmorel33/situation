/***************************************************************************************************
*
*   sit/aud/filter.h - Multi-Pole State Variable Filter with Oversampling
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Advanced multi-pole filter implementation featuring:
*   - 1-pole (6dB/oct), 2-pole (12dB/oct), 3-pole (18dB/oct), 4-pole (24dB/oct) responses
*   - State Variable Filter (SVF) topology for smooth, musical response
*   - Multiple filter modes: LP, HP, BP, Notch, Allpass, and combo modes (LP+BP, LP+HP, BP+HP)
*   - DC blocking and soft-clipping drive
*   - Optional 2x oversampling with half-band filter
*   - Resonance control with pole-dependent scaling
*   - Anti-denormal protection
*
*   Based on the PxFilter design with SVF topology and multi-pole cascading.
*
***************************************************************************************************/

#ifndef SITUATION_FILTER_H
#define SITUATION_FILTER_H

#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// FMA detection and optimization
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define FILTER_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
#else
    #define FILTER_FMA(a, b, c) ((a) * (b) + (c))
#endif

// ================================================================================================
// FILTER MODES
// ================================================================================================

typedef enum {
    PX_FILTER_MODE_OFF = 0,
    PX_FILTER_MODE_LP,       // Low-pass
    PX_FILTER_MODE_HP,       // High-pass
    PX_FILTER_MODE_BP,       // Band-pass
    PX_FILTER_MODE_NOTCH,    // Notch (LP + HP)
    PX_FILTER_MODE_ALLPASS,  // All-pass
    PX_FILTER_MODE_LP_BP,    // Low-pass + Band-pass combo
    PX_FILTER_MODE_LP_HP,    // Low-pass + High-pass combo
    PX_FILTER_MODE_BP_HP     // Band-pass + High-pass combo
} PxFilterMode;

// ================================================================================================
// FILTER STATE
// ================================================================================================

typedef struct {
    // Configuration
    PxFilterMode current_mode;
    int poles;                  // 1, 2, 3, or 4
    float sample_rate;
    float drive;                // Input drive (default 1.0)
    bool use_oversampling;      // Enable 2x oversampling
    
    // Coefficients
    float f_coeff;              // Frequency coefficient for SVF
    float q_inv_coeff;          // Inverse Q coefficient for SVF
    float pole3_coeff;          // Coefficient for 3rd pole (1-pole stage)
    
    // DC Blocker state
    float dc_block_x1;
    float dc_block_y1;
    
    // 1-pole combo mode states (for 6dB/oct path)
    float combo_lp_state;
    float combo_hp_state;
    
    // 2-pole SVF states (Stage 1: 12dB)
    float lp_state1;
    float bp_state1;
    
    // 3-pole states (18dB)
    float pole3_lp_state;
    float pole3_hp_state;
    float pole3_bp_state;
    
    // 4-pole SVF states (Stage 2: 24dB)
    float lp_state2;
    float bp_state2;
    
    // Oversampling state
    float os_x1;
    float os_x2;
} SituationFilter;

// Half-band filter coefficients for 2x oversampling
static const float HB[4] = { 0.036681502163648017f, 0.2893081761252365f, 0.6739103217610154f, -0.0f };

// ================================================================================================
// INITIALIZATION
// ================================================================================================

/**
 * @brief Initialize filter to default state.
 * @param filter Pointer to filter structure.
 */
static void filter_init(SituationFilter* filter, float sample_rate) {
    memset(filter, 0, sizeof(SituationFilter));
    filter->current_mode = PX_FILTER_MODE_OFF;
    filter->sample_rate = sample_rate;
    filter->poles = 2;
    filter->drive = 1.0f;
    filter->use_oversampling = false;
    filter->dc_block_x1 = 0.0f;
    filter->dc_block_y1 = 0.0f;
    filter->os_x1 = 0.0f;
    filter->os_x2 = 0.0f;
    filter->combo_lp_state = 0.0f;
    filter->combo_hp_state = 0.0f;
}

// ================================================================================================
// COEFFICIENT CALCULATION
// ================================================================================================

/**
 * @brief Set filter coefficients based on cutoff, resonance, mode, and poles.
 * @param filter Pointer to filter structure.
 * @param cutoff_hz Cutoff frequency in Hz.
 * @param resonance_q Resonance Q factor (0.5 to 20.0).
 * @param mode Filter mode (LP, HP, BP, etc.).
 * @param poles Number of poles (1, 2, 3, or 4).
 */
static void filter_set_coefficients(SituationFilter* filter, float cutoff_hz, float resonance_q, 
                                    PxFilterMode mode, int poles) {
    if (filter->sample_rate <= 0) return;
    
    // Clamp cutoff to safe range (20 Hz to 45% of Nyquist)
    float max_safe_cutoff = filter->sample_rate * 0.45f;
    cutoff_hz = fmaxf(20.0f, fminf(cutoff_hz, max_safe_cutoff));
    
    // Adjust resonance based on the number of poles to maintain a similar feel
    float q_for_poles = resonance_q;
    if (poles == 3) {
        q_for_poles = powf(resonance_q, 0.75f);
    } else if (poles == 4) {
        q_for_poles = sqrtf(resonance_q);
    }
    // For 1-pole (6dB), resonance has little effect — clamp gently
    else if (poles == 1) {
        q_for_poles = 0.707f;  // Neutral, no peaking
    }
    
    // Frequency-dependent Q limiting to prevent instability
    float freq_factor = cutoff_hz / filter->sample_rate;
    float max_q_at_freq = 20.0f * (1.0f - freq_factor * freq_factor);
    resonance_q = fmaxf(0.5f, fminf(q_for_poles, max_q_at_freq));
    
    // Coefficients for the main 2-pole SVF stage
    float omega = 2.0f * PI * cutoff_hz / filter->sample_rate;
    float sin_omega = sinf(omega);
    float alpha = sin_omega / (2.0f * resonance_q);
    filter->f_coeff = 2.0f * sin_omega / (1.0f + alpha);
    filter->q_inv_coeff = 2.0f * alpha;
    
    // Coefficient for the additional 1-pole stage (3rd pole)
    float tan_val = tanf(PI * cutoff_hz / filter->sample_rate);
    filter->pole3_coeff = tan_val / (1.0f + tan_val);
    
    filter->current_mode = mode;
    filter->poles = poles;
}

// ================================================================================================
// PARAMETER SETTERS
// ================================================================================================

static void filter_set_type(SituationFilter* f, PxFilterMode mode) {
    f->current_mode = mode;
}

static void filter_set_frequency(SituationFilter* f, float freq) {
    // Recalculate coefficients with new frequency
    filter_set_coefficients(f, freq, 0.707f, f->current_mode, f->poles);
}

static void filter_set_q(SituationFilter* f, float q) {
    // Recalculate coefficients with new Q
    filter_set_coefficients(f, 1000.0f, q, f->current_mode, f->poles);
}

static void filter_set_poles(SituationFilter* f, int poles) {
    f->poles = poles;
}

static void filter_set_drive(SituationFilter* f, float drive) {
    f->drive = drive;
}

static void filter_set_oversampling(SituationFilter* f, bool enable) {
    f->use_oversampling = enable;
}

// ================================================================================================
// INTERNAL PROCESSING
// ================================================================================================

/**
 * @brief Internal filter processing (single sample).
 * @param filter Pointer to filter structure.
 * @param input_sample Input sample.
 * @return Filtered output sample.
 */
static float filter_process_internal(SituationFilter* filter, float input_sample) {
    if (filter->current_mode == PX_FILTER_MODE_OFF) return input_sample;
    
    // --- DC Blocker & Drive using FMA ---
    float dc_block_coeff = 0.999f;
    float dc_blocked = FILTER_FMA(dc_block_coeff, filter->dc_block_y1, input_sample - filter->dc_block_x1);
    filter->dc_block_x1 = input_sample;
    filter->dc_block_y1 = dc_blocked;
    
    float driven_input = tanhf(dc_blocked * filter->drive);
    
    // v1.4.3: Unified 6 dB/oct (1-pole) path with full combo support
    if (filter->poles == 1) {
        // Always compute both LP and HP 1-pole responses in parallel using FMA
        filter->combo_lp_state = FILTER_FMA(filter->pole3_coeff, driven_input - filter->combo_lp_state, filter->combo_lp_state);
        filter->combo_hp_state = FILTER_FMA(filter->pole3_coeff, driven_input - filter->combo_hp_state, filter->combo_hp_state);
        
        float lp = filter->combo_lp_state;
        float hp = driven_input - filter->combo_hp_state;
        float bp = driven_input - lp - hp;  // Derived band-pass
        
        float output;
        switch (filter->current_mode) {
            case PX_FILTER_MODE_LP:       output = lp; break;
            case PX_FILTER_MODE_HP:       output = hp; break;
            case PX_FILTER_MODE_BP:       output = bp; break;
            case PX_FILTER_MODE_NOTCH:    output = lp + hp; break;
            case PX_FILTER_MODE_ALLPASS:  output = driven_input - 2.0f * bp; break;
            case PX_FILTER_MODE_LP_BP:    output = (lp + bp) * 0.707f; break;
            case PX_FILTER_MODE_LP_HP:    output = (lp + hp) * 0.707f; break;
            case PX_FILTER_MODE_BP_HP:    output = (bp + hp) * 0.707f; break;
            default:                      output = driven_input; break;
        }
        return output;
    }
    
    // --- Multi-pole path (poles >= 2): Full SVF with combos ---
    // --- Stage 1: 12dB SVF (Always runs) using FMA ---
    // These are the raw 12dB outputs, calculated fresh each sample.
    float notch1 = FILTER_FMA(-filter->q_inv_coeff, filter->bp_state1, driven_input);
    float lp1 = FILTER_FMA(filter->f_coeff, filter->bp_state1, filter->lp_state1);
    float hp1 = notch1 - lp1;
    float bp1 = FILTER_FMA(filter->f_coeff, hp1, filter->bp_state1);
    
    // Update the state variables for the next sample.
    const float anti_denormal = 1e-25f;
    filter->lp_state1 = tanhf(lp1 + anti_denormal) - anti_denormal;
    filter->bp_state1 = tanhf(bp1 + anti_denormal) - anti_denormal;
    
    // --- Initialize final outputs with the 12dB results ---
    float final_lp = filter->lp_state1;
    float final_bp = filter->bp_state1;
    float final_hp = hp1;
    
    // --- Stage 2: Apply additional poles if needed ---
    if (filter->poles == 3) {
        // --- Correct 18dB Logic using FMA ---
        // 18dB LP = 12dB LP -> 6dB LP stage
        filter->pole3_lp_state = FILTER_FMA(filter->pole3_coeff, final_lp - filter->pole3_lp_state, filter->pole3_lp_state);
        final_lp = filter->pole3_lp_state;
        
        // 18dB HP = 12dB HP -> 6dB HP stage
        filter->pole3_hp_state = FILTER_FMA(filter->pole3_coeff, final_hp - filter->pole3_hp_state, filter->pole3_hp_state);
        final_hp = final_hp - filter->pole3_hp_state;
        
        // 18dB BP = 12dB BP -> 6dB LP stage (to smooth the peak)
        filter->pole3_bp_state = FILTER_FMA(filter->pole3_coeff, final_bp - filter->pole3_bp_state, filter->pole3_bp_state);
        final_bp = filter->pole3_bp_state;
    } else if (filter->poles == 4) {
        // --- Correct 24dB Logic using FMA ---
        float stage2_input = filter->lp_state1;
        float notch2 = FILTER_FMA(-filter->q_inv_coeff, filter->bp_state2, stage2_input);
        float lp2 = FILTER_FMA(filter->f_coeff, filter->bp_state2, filter->lp_state2);
        float hp2 = notch2 - lp2;
        float bp2 = FILTER_FMA(filter->f_coeff, hp2, filter->bp_state2);
        
        filter->lp_state2 = tanhf(lp2 + anti_denormal) - anti_denormal;
        filter->bp_state2 = tanhf(bp2 + anti_denormal) - anti_denormal;
        
        final_lp = filter->lp_state2;
        final_bp = filter->bp_state2;
        final_hp = hp2;
    }
    
    // --- Final Output Selection ---
    switch (filter->current_mode) {
        case PX_FILTER_MODE_LP:      return final_lp;
        case PX_FILTER_MODE_BP:      return final_bp;
        case PX_FILTER_MODE_HP:      return final_hp;
        case PX_FILTER_MODE_LP_BP:   return (final_lp + final_bp) * 0.707f;
        case PX_FILTER_MODE_LP_HP:   return (final_lp + final_hp) * 0.707f;
        case PX_FILTER_MODE_BP_HP:   return (final_bp + final_hp) * 0.707f;
        case PX_FILTER_MODE_NOTCH:   return final_lp + final_hp;
        case PX_FILTER_MODE_ALLPASS: return final_lp - final_bp + final_hp;
        default: return input_sample;
    }
}

/**
 * @brief Process filter with 2x oversampling.
 * @param filter Pointer to filter structure.
 * @param input_sample Input sample.
 * @return Filtered and downsampled output sample.
 */
static float filter_process_oversampled(SituationFilter* filter, float input_sample) {
    float y0 = filter_process_internal(filter, input_sample);
    float y1 = filter_process_internal(filter, input_sample);
    
    float out = (HB[0] * filter->os_x2) + (HB[1] * filter->os_x1) + (HB[2] * y0) + (HB[3] * y1);
    
    filter->os_x2 = filter->os_x1;
    filter->os_x1 = y0;
    
    return out;
}

// ================================================================================================
// PUBLIC PROCESSING FUNCTION
// ================================================================================================

/**
 * @brief Process audio through filter.
 * @param filter Pointer to filter structure.
 * @param input Input audio buffer.
 * @param output Output audio buffer.
 * @param frames Number of frames to process.
 * @param channels Number of channels (1 or 2).
 */
static void filter_process(SituationFilter* filter, const float* input, float* output, 
                          int frames, int channels) {
    if (channels == 1) {
        // Mono processing
        for (int i = 0; i < frames; i++) {
            if (filter->use_oversampling) {
                output[i] = filter_process_oversampled(filter, input[i]);
            } else {
                output[i] = filter_process_internal(filter, input[i]);
            }
        }
    } else {
        // Stereo processing (process each channel independently)
        for (int i = 0; i < frames; i++) {
            float in_l = input[i * 2];
            float in_r = input[i * 2 + 1];
            
            if (filter->use_oversampling) {
                output[i * 2] = filter_process_oversampled(filter, in_l);
                output[i * 2 + 1] = filter_process_oversampled(filter, in_r);
            } else {
                output[i * 2] = filter_process_internal(filter, in_l);
                output[i * 2 + 1] = filter_process_internal(filter, in_r);
            }
        }
    }
}

// Compatibility function for gain parameter (unused in this design)
static void filter_set_gain(SituationFilter* f, float gain_db) {
    (void)f;
    (void)gain_db;
    // This filter design doesn't use gain parameter
}

#endif // SITUATION_FILTER_H
