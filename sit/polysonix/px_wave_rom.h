#ifndef PX_WAVE_ROM_H
#define PX_WAVE_ROM_H

#include <stdio.h>
#include <stdint.h>
#include "px_vm.h"

/***************************************************************************************************
*
* -- Polysonix Waveform Scripting Language --  PATCHES
*   (c) 2025 Jacques Morel
*
*
****************************************************************************************************/
#ifndef NUM_DEFAULT_WAVES
#define NUM_DEFAULT_WAVES 256
#endif

// --- Waveform Expressions ---
// WaveDefinition is defined in px_vm.h


#ifdef __cplusplus
extern "C" {
#endif

extern WaveDefinition default_waves[NUM_DEFAULT_WAVES];

typedef struct {
    bool system_initialized;
    int lfsr_types_initialized;
    bool cache_initialized;
    size_t cache_entry_count;
    int default_waves_compiled;
    size_t total_memory_usage_estimate; // Rough estimate in bytes
} PxVmStats;

bool px_vm_init(void);
void px_vm_deinit(void);
BytecodeChunk* get_default_wave_bytecode(int wave_index);
BytecodeChunk* get_or_compile_wave_bytecode(const char* expression);
bool px_vm_is_initialized(void);
void px_vm_get_stats(PxVmStats* stats);
void px_vm_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif // PX_WAVE_ROM_H

#ifdef PX_WAVE_ROM_IMPLEMENTATION
#ifndef PX_WAVE_ROM_IMP_INCLUDED
#define PX_WAVE_ROM_IMP_INCLUDED

WaveDefinition default_waves[NUM_DEFAULT_WAVES] = {
    // --- Bank 0: Analog & Basic Shapes (0-15) ---
/*   0*/ { "Triangle Up", "1.0 - 2.0 * abs((x + MOD_C * 0.5) / PI - 1.0) + 1.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Unused. MOD_B: Harmonic. MOD_C: Phase skew/bend.

/*   1*/ { "Triangle Down", "2.0 * abs((x + MOD_C * 0.5) / PI - 1.0) - 1.0 + 1.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Unused. MOD_B: Harmonic. MOD_C: Phase skew/bend.

/*   2*/ { "Sine Up", "((1.0 - (0.5 * (MOD_A + 1.0))) * sin(x + MOD_C * 0.25) + (0.5 * (MOD_A + 1.0)) * tanh(5.0 * sin(x + MOD_C * 0.25))) + 1.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Sine-to-tanh mix. MOD_B: Harmonic. MOD_C: Phase modulation.

/*   3*/ { "Sine Down", "((1.0 - (0.5 * (MOD_A + 1.0))) * (-sin(x + MOD_C * 0.25)) + (0.5 * (MOD_A + 1.0)) * (-tanh(5.0 * sin(x + MOD_C * 0.25)))) + 1.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Sine-to-tanh mix. MOD_B: Harmonic. MOD_C: Phase modulation.

/*   4*/ { "Square Up", "(x < (TWO_PI * (0.5 + 0.4 * MOD_A)) ? 1.0 : -1.0) + 2.0 * MOD_B * sin(2.0 * x + MOD_C * PI)" }, // MOD_A: PWM. MOD_B: Harmonic. MOD_C: Phase modulation on the harmonic.

/*   5*/ { "Square Down", "(x < (TWO_PI * (0.5 + 0.4 * MOD_A)) ? -1.0 : 1.0) + 2.0 * MOD_B * sin(2.0 * x + MOD_C * PI)" }, // MOD_A: PWM. MOD_B: Harmonic. MOD_C: Phase modulation on the harmonic.

/*   6*/ { "Saw Rising", "(1.0 - (0.5 + 0.5*MOD_A)) * ((x + x*MOD_C*0.5) / PI - 1.0) + (0.5 + 0.5*MOD_A) * (x > PI ? 1.0 : -1.0) + 2.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Saw-to-pulse mix. MOD_B: Harmonic. MOD_C: Phase distortion/bend.

/*   7*/ { "Saw Falling", "(1.0 - (0.5 + 0.5*MOD_A)) * (1.0 - (x + x*MOD_C*0.5) / PI) + (0.5 + 0.5*MOD_A) * (x > PI ? -1.0 : 1.0) + 2.0 * MOD_B * sin(2.0*x)" }, // MOD_A: Saw-to-pulse mix. MOD_B: Harmonic. MOD_C: Phase distortion/bend.

    // --- Half/Half Waves (8-15) ---

/*   8*/ { "Saw/Sine Up", "((1.0 - (0.5 * (MOD_A + 1.0))) * (x < PI ? (x / PI_OVER_2 - 1.0) : sin(x + PI * MOD_B)) + (0.5 * (MOD_A + 1.0)) * tanh(6.0 * (x < PI ? (x / PI_OVER_2 - 1.0) : sin(x + PI * MOD_B))))" }, // Phase shift on sine part

/*   9*/ { "Sine/Saw Down", "((1.0 - (0.5 * (MOD_A + 1.0))) * (x < PI ? -sin(x + PI * MOD_B) : (1.0 - (x - PI) / PI_OVER_2)) + (0.5 * (MOD_A + 1.0)) * tanh(6.0 * (x < PI ? -sin(x + PI * MOD_B) : (1.0 - (x - PI) / PI_OVER_2))))" }, // Phase shift on sine

/* 10*/ { "Square/Sine Up", "((1.0 - (0.5 * (MOD_A + 1.0))) * (x < PI ? 1.0 : sin(x + PI * MOD_B)) + (0.5 * (MOD_A + 1.0)) * tanh(6.0 * (x < PI ? 1.0 : sin(x + PI * MOD_B))))" }, // Phase shift on sine

/* 11*/ { "Sine/Square Down", "((1.0 - (0.5 * (MOD_A + 1.0))) * (x < PI ? sin(x + PI * MOD_B) : -1.0) + (0.5 * (MOD_A + 1.0)) * tanh(6.0 * (x < PI ? sin(x + PI * MOD_B) : -1.0)))" }, // Phase shift on sine

/* 12*/ { "Saw/Triangle Up", "(x < (TWO_PI * (0.5 + 0.25 * MOD_A + 0.1 * MOD_B)) ? (x / PI_OVER_2 - 1.0) : ((x - PI) < PI_OVER_2 ? ((x - PI) / PI_OVER_2) : (1.0 - ((x - PI) - PI_OVER_2) / PI_OVER_2)))" }, // Adjust split point

/* 13*/ { "Triangle/Saw Down", "(x < (TWO_PI * (0.5 + 0.25 * MOD_A + 0.1 * MOD_B)) ? (-2.0 * abs(x / PI - 0.5)) : (3.0 - 2.0 * x / PI))" }, // Adjust split point

/* 14*/ { "Triangle/Sine Up", "((1.0 - (0.5 * (MOD_A + 1.0))) * (2.0 * (1.0 - abs(x / (PI/2) - 1.0)) - 1.0) + (0.5 * (MOD_A + 1.0)) * sin(x + PI * MOD_B))" }, // Phase shift on sine

/* 15*/ { "Sine/Triangle Down", "((1.0 - (0.5 * (MOD_A + 1.0))) * cos(x + PI + PI * MOD_B) + (0.5 * (MOD_A + 1.0)) * (-2.0 * (abs((x - PI) / PI - 0.5)) + 0.5))" }, // Phase shift on sine



    // --- Bank 1: Waveshaping & Distortion (16-31) ---
/* 16*/ { "Clipped Sine", "min(0.5 + 0.5 * MOD_B, max(-0.5 - 0.5 * MOD_B, sin(x + 0.3 * MOD_A))) * 2.0" }, // Adjust clipping level

/* 17*/ { "Rectified Sine", "abs(sin(x + 0.4 * MOD_A)) * 2.0 - 1.0 + 1.0 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 18*/ { "Sine * Saw", "sin(x + 0.3 * MOD_A) * (tanh(x) * PI) + 0.5 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 19*/ { "Overload Spark", "(x < (TWO_PI * (0.2 + 0.2 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.4 + 0.2 * MOD_A)) ? -1.0 : (x < (TWO_PI * (0.6 + 0.2 * MOD_A)) ? 0.5 : (1.0 - x / TWO_PI) * 1.5 - 0.5))) + 1.0 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 20*/ { "Overfolded Saw", "min(1.0, max(-1.0, 1.2 * (x / PI - 1.0) * sin(3*x + RAND_OFFSET + 0.3 * MOD_A + PI * MOD_B)))" }, // Phase shift on modulation

/* 21*/ { "Clipped Chaos", "min(0.5, max(-0.5, 1.5 * ((x < PI_OVER_2) ? (x / PI_OVER_2) : (x < THREE_PI_OVER_2 ? (1.0 - (x - PI_OVER_2) / PI_OVER_2) : (-1.0 + (x - THREE_PI_OVER_2) / PI_OVER_2))))) * sin(4*x + 0.4 * MOD_A + PI * MOD_B)" }, // Phase shift on modulation

/* 22*/ { "Wavefolder Sim (A=Fold B=Bias)", "asin(sin(x* (1.0 + FREQUENCY/1000.0) + MOD_B*PI) * (1.0 + abs(MOD_A)*5.0)) / (PI/2.0) * 0.8" },

/* 23*/ { "Triangle Fold", "abs(sin(x + MOD_A*sin(x))) * 2.0 - 1.0" }, // Folded triangle wave.


/* 24*/ { "Math: Tanh Drive", "tanh(sin(x) * (1.0 + MOD_A * 10.0))" }, // Soft-clipped sine.

/* 25*/ { "Math: Cubic", "pow((x/PI-1.0), 3.0)" }, // Cubic function.

/* 26*/ { "Math: Rectified", "abs(sin(x)) * 2.0 - 1.0" }, // Full-wave rectification.

/* 27*/ { "Math: Sinc", "sin(x*MOD_A*10.0)/(x*MOD_A*10.0 + 0.01)" }, // Sinc function approximation.

/* 28*/ { "Weird: Step-Slope", "(x < PI) ? 1.0 : (1.0 - (x-PI)/PI * 2.0)" }, // Half-square, half-saw.


/* 29*/ { "Gritty Bass", "min(1.0, max(-1.0, 1.1 * ((x / PI - 1.0) + 0.2 * MOD_A * sin(2*x + PI * MOD_B))))" }, // Phase shift on modulation

/* 30*/ { "Hybrid Saw*Sine", "(x / PI - 1.0) * sin(2*x + 0.5 * MOD_A + PI * MOD_B)" }, // Additional phase shift

/* 31*/ { "Razor Pulse", "(x < (TWO_PI * (0.05 + 0.02 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.1 + 0.02 * MOD_A)) ? -1.0 : (x / PI - 1.0))) + 0.5 * MOD_B * sin(2*x)" }, // Harmonic texture


    // --- Bank 2: Digital Steps & Logic (32-47) ---
/* 32*/ { "Pulse 25%", "(x < (TWO_PI * (0.25 + 0.25 * MOD_A)) ? 1.0 : -1.0) + 1 * MOD_B * sin(2*x)" }, // Harmonic softening

/* 33*/ { "Pulse 75%", "(x < (TWO_PI * (0.75 + 0.25 * MOD_A)) ? 1.0 : -1.0) + 1 * MOD_B * sin(2*x)" }, // Harmonic softening

/* 34*/ { "Staircase 4 Step", "(x < (TWO_PI * (0.125 + 0.05 * MOD_A)) ? -0.75 : (x < (TWO_PI * (0.25 + 0.05 * MOD_A)) ? -0.25 : (x < (TWO_PI * (0.375 + 0.05 * MOD_A)) ? 0.25 : 0.75))) + 0.5 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 35*/ { "Bit Crush Bomb", "(x < (TWO_PI * (0.4 + 0.1 * MOD_A)) ? 1.0 : ((floor((x - (TWO_PI * (0.4 + 0.1 * MOD_A))) / (TWO_PI/10)) % 4) - 2) * 0.5) + 0.5 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 36*/ { "Bit-Crushed Square", "min(1.0, max(-1.0, (x < PI ? 1.0 : -1.0) + sin(2*x + RAND_OFFSET + 0.5 * MOD_A + PI * MOD_B)))" }, // Phase shift on noise

/* 37*/ { "Pulse Train Wreck", "(x < (TWO_PI * (0.1 + 0.03 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.15 + 0.03 * MOD_A)) ? -1.0 : (x < (TWO_PI * (0.3 + 0.03 * MOD_A)) ? 0.5 : (x < (TWO_PI * (0.35 + 0.03 * MOD_A)) ? -0.5 : 0.0)))) + 0.2 * MOD_B * sin(2*x)" }, // Harmonic texture

/* 38*/ { "Narrow", "(x < (TWO_PI * (0.2 + 0.1 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.5 + 0.05 * MOD_A + 0.1 * MOD_B)) ? -1.0 : 0.0))" }, // Adjust second pulse width

/* 39*/ { "Quantized Saw 8", "floor((x / PI - 1.0 + 0.25 * MOD_A + 0.5 * MOD_B) * 4.0) * 0.25" }, // Adjust quantization offset


/* 40*/ { "PWM Synth (A=Width B=Sub)", "( (x < (TWO_PI * (0.05 + 0.9*abs(MOD_A))) ? 1.0 : -1.0) * (1.0 - 0.5*abs(MOD_B)) + sin(x*0.5)*0.5*abs(MOD_B) ) * 0.7" },

/* 41*/ { "PWM Gate", "(( (x < (PI + MOD_B*PI_OVER_2)) ^ (MOD_A > 0) ) ? 1.0 : -1.0) * 0.8" },

/* 42*/ { "Harmonic Switch", "( ( (MOD_A > 0.1) ^ (MOD_B < -0.1) ) ? (sin(x) + 0.5*sin(3*x)) : (sin(x) + 0.3*cos(2*x + PI*0.25)) ) * 0.7" },

/* 43*/ { "Multi-Gate", "(x < PI_OVER_2 ? sin(2*x) : (x < PI ? ((MOD_A > 0) * cos(x - PI_OVER_2) * 1.5) : ( ((MOD_A <= 0) ^ (MOD_B > 0)) * sin(3*(x-PI)) * 0.8 ) ) ) * (1.0 - 0.2*abs(MOD_A) - 0.2*abs(MOD_B))" },

/* 44*/ { "Bitwise Staircase", "floor(x * (2.0 + MOD_A*14.0)) / (2.0 + MOD_A*14.0)" }, // Creates a stepped version of a sawtooth.

/* 45*/ { "Bitwise XOR Wave", "( (floor(x*32.0/TWO_PI) + floor(x*MOD_A*32.0/TWO_PI)) % 2.0 ) * 2.0 - 1.0" }, // Simulates XOR parity noise.

/* 46*/ { "Hard Quantize Sine", "floor(sin(x) * (2.0 + MOD_A*6.0)) / (2.0 + MOD_A*6.0)" }, // A sine wave quantized to low bit depth.

/* 47*/ { "Comparator Fuzz", "sin(x) > sin(x * (2.0 + MOD_A)) ? 1.0 : -1.0" }, // 1-bit comparator fuzz effect.


    // --- Bank 3: Phase Distortion & Modulation (48-63) ---
/* 48*/ { "Warp Speed", "(x < (TWO_PI * 0.7) ? (x / PI_OVER_2 - 1.0) : sin(5*x + 0.5 * MOD_A + PI * MOD_B))" }, // Phase shift on high harmonic

/* 49*/ { "Ghost Wail", "sin(x + 0.3 * sin(4*x + MOD_A) + PI * MOD_B)" }, // Additional phase modulation

/* 50*/ { "Laser Malfunction", "min(1.0, max(-1.0, (x / PI - 1.0) * sin(3*x + RAND_OFFSET + 0.4 * MOD_A + PI * MOD_B)))" }, // Phase shift on modulation

/* 51*/ { "Hyperspace Glitch", "(x < (TWO_PI * (0.2 + 0.05 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.4 + 0.05 * MOD_A)) ? cos(4*x + 0.4 * MOD_A + PI * MOD_B) : (x < (TWO_PI * (0.6 + 0.05 * MOD_A)) ? -1.0 : sin(6*x + 0.4 * MOD_A) * 0.7)))" }, // Phase shift on cos


/* 52*/ { "Shredded Saw", "min(0.8, max(-0.8, 2 * (x / PI - 1.0 + 0.2 * MOD_A * sin(3*x + PI * MOD_B))))" }, // Phase shift on modulation

/* 53*/ { "Glitch Sine", "sin(x + 0.5 * MOD_A + PI * MOD_B) + (x > (TWO_PI*0.2) && x < PI_OVER_2 ? -0.5 : 0) + (x > (TWO_PI*0.6) && x < (TWO_PI*0.65) ? 0.5 : 0)" }, // Additional phase shift

/* 54*/ { "4-Segment Bump", "-1.0 + (x < PI_OVER_2 ? (1.0 * sin(2.0 * (x + 0.2 * MOD_A + PI * MOD_B))) : (x < PI ? (1.5 * sin(2.0 * (x - PI_OVER_2))) : (x < THREE_PI_OVER_2 ? (1.75 * sin(2.0 * (x - PI))) : (0.75 * sin(2.0 * (x - THREE_PI_OVER_2))))))" }, // Phase shift on first segment

/* 55*/ { "Bird Call AM", "sin(x + 0.5 * MOD_A) * (0.5 + 0.5 * sin(5*x + PI * MOD_B))" }, // Phase shift on carrier

/* 56*/ { "Phase Distortion (A=Amt B=Shape)", "sin(x + (abs(x/(PI) - 1.0)*2.0 - 1.0) * (1.0 + MOD_B*0.5) * PI * abs(MOD_A) * (1.0 + FREQUENCY/600.0)) * 0.9" },

/* 57*/ { "Chaos Sine (A=ModRate B=ModAmt)", "sin(x + sin(x * (1.0 + abs(MOD_A)*8.0)) * abs(MOD_B) * 3.0)" },

/* 58*/ { "Phase Glitch", "sin(x + ( ((MOD_A > 0) ^ (x > PI)) * PI_OVER_2 * (0.5 + 0.5*MOD_B) ))" },

/* 59*/ { "Phase Distortion Wave", "sin(x + sin(x*0.5 + MOD_A*PI)*MOD_B*4.0 + lfsr_noise(LFSR_6BIT, 1.0)*0.1) * (1.0 + 0.5*sin(x*0.25 + RAND_OFFSET*PI)) + tanh(sin(x*2.0 + MOD_A*PI_OVER_2)*3.0)*0.2*abs(MOD_B)" },

/* 60*/ { "PD: Resonant", "sin(x + sin(x) * MOD_A * 3.0)" }, // Classic phase distortion resonance.

/* 61*/ { "PD: Wrap", "sin(x * (1.0 + MOD_A * sin(x)))" }, // Frequency warping phase distortion.

/* 62*/ { "PD: Spike", "sin(x + (x>PI?1.0:-1.0)*MOD_A)" }, // Discontinuous phase spike.

/* 63*/ { "PD: Windowed", "sin(x) * sin(x * (1.0 + MOD_A * 10.0))" }, // AM windowing creating formant-like effects.


    // --- Bank 4: FM Synthesis I (Classic & Bass) (64-79) ---
/* 64*/ { "Classic FM EP (A=Index B=Detune)", "(sin(x + sin(x * 2.0 + 0.01*MOD_B) * (1.0 + 5.0*abs(MOD_A)) * (FREQUENCY / 440.0)) + sin(x)*0.1*abs(MOD_B)) / 1.1" },

/* 65*/ { "FM Bass Growl (A=Fdbk B=Index)", "tanh( sin(x + sin(x*0.5 + 0.05*MOD_B) * MOD_B * 3.0 * (1.0+FREQUENCY/200.0) ) * (1.0 + abs(MOD_A)*2.0) ) * 0.9" },

/* 66*/ { "Freq Shifter FM (A=Shift B=Index)", "sin(x * (1.0 + MOD_A * (FREQUENCY/440.0)) + sin(x*1.5)*abs(MOD_B)*3.0)" },

/* 67*/ { "Complex FM A=Index B=ModFreq", "sin(x + sin(x * (1.0 + abs(MOD_B)*4.0)) * (FREQUENCY / 220.0) * (1.0 + MOD_A * 3.0))" },

/* 68*/{ "FM Pluck", "sin(x * (1.0 + 0.001*FREQUENCY/100.0) + sin(x * ( ((MOD_A > 0.2) ^ (MOD_B < -0.2)) ? (3.5 + MOD_A*2.0) : (1.5 - MOD_B*1.0) ) ) * (1.5 + 2.0*abs(MOD_A) + 1.0*abs(MOD_B)) * exp(-x* (2.0 + abs(MOD_B)*3.0)) ) * 0.9 * exp(-x*0.3)" },

/* 69*/{ "FM Pitched Grit", "tanh( sin( x * FREQUENCY + (2.0 + MOD_A * 3.0) * sin(x * FREQUENCY * (1.414 + MOD_B * 0.5 + RAND_OFFSET*0.05)) ) * (1.0 + MOD_A * 0.5) ) * 0.9" },

/* 70*/{ "FM Dynamic Lead", "sin(x + ( sin(x * (1.5 + MOD_A*1.5)) * (2.0 + MOD_B*2.0) ) ) + 0.5 * sin(x*2.0 + ( sin(x*2.0 * (1.5 + MOD_A*1.5)) * (2.0 + MOD_B*2.0) ) ) + 0.33 * sin(x*3.0 + ( sin(x*3.0 * (1.5 + MOD_A*1.5)) * (2.0 + MOD_B*2.0) ) )" },

/* 71*/{ "FM Glassy Evolve", "sin(x * (1.0 + RAND_OFFSET*0.005) + ( sin(x * (3.01 + RAND_OFFSET*0.01) + ( sin(x * (7.03 + RAND_OFFSET*0.02)) * (0.4 + MOD_A*0.6) ) ) * (1.0 + MOD_B*1.5) ) )" },

/* 72*/ { "FM: Classic EP (A=Tine B=Bell C=Ratio)", "sin(x + sin(x * 2.0) * (2.0 + 3.0 * MOD_A) * exp(-x*2.0) + sin(x * (3.0 + 2.0*MOD_C)) * (1.5 * MOD_B))" }, // A classic 2-Op electric piano sound that evolves.
              // MOD_A: Controls the "tine" or sharp attack amount. MOD_B: Adds a 'bell-like' higher harmonic modulator. MOD_C: Changes the harmonic ratio of the bell component.

/* 73*/ { "FM: Growl Bass (A=Index B=Fdbk C=Ratio)", "sin(x + sin(x * (1.5 + MOD_C) + sin(x * (1.5 + MOD_C)) * MOD_B) * (1.0 + 4.0 * MOD_A))" }, // A 3-Op setup designed for aggressive bass sounds.
              // MOD_A: Main FM index (overall brightness/growl). MOD_B: Simulates feedback by modulating Op3 with itself, adding grit. MOD_C: Changes the modulator frequency for different growl characters.

/* 74*/ { "FM: Deep Sub", "sin(x) + 0.5 * sin(x * 0.5 + MOD_A)" }, // Sub-oscillator FM.

/* 75*/ { "FM: Talker", "sin(x * (1.0 + sin(x*5.0)*MOD_A))" }, // Vowel-like FM modulation.

/* 76*/ { "FM: Feedback Sim", "sin(x + sin(x)*MOD_A + sin(x)*sin(x)*MOD_B)" }, // Simulated feedback path.

/* 77*/ { "FM: Cascaded", "sin(x + sin(x*2.0 + sin(x*4.0)*MOD_B)*MOD_A)" }, // 3-operator cascade.

/* 78*/ { "FM: Vowel-ish", "sin(x * (1.0 + MOD_A)) * sin(x * (3.0 + MOD_B))" }, // Formant-ish AM/FM.


/* 79*/ { "FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch)", "sin(x * (1.0 + MOD_C * 0.01) + sin(x * 0.25) * (2.0 + 2.0*MOD_A) + sin(x * 13.0) * MOD_B)" }, // An evolving, complex pad/drone sound.
              // MOD_A: Slowly sweeps the main modulation index. MOD_B: Introduces a high-frequency, chaotic modulator. MOD_C: Modulates the pitch of the carrier for vibrato/instability.


    // --- Bank 5: FM Synthesis II (Complex & Metallic) (80-95) ---
/* 80*/{ "FM Metallic Bell (A=Decay B=Ratio)", "sin(x + sin(x * (4.03 + MOD_B*2.0 + RAND_OFFSET*0.02)) * (3.0 + abs(MOD_A)*4.0) * exp(-x * (2.0 + abs(MOD_A)*8.0)) ) * exp(-x*0.5)" },

/* 81*/{ "FM Hollow Drone (A=ModMix B=ModRatio)", "( (sin(x + sin(x*0.75) * 3.5) * (1.0 - abs(MOD_A))) + (sin(x + sin(x*(2.5 + MOD_B*2.0)) * 1.5) * abs(MOD_A)) ) * 0.7" },

/* 82*/{ "FM Harsh Noise Sweep (A=Sweep B=Intensity)", "sin(x + tan(x * (10.0 + MOD_A*20.0 + x*5.0 )) * (0.5 + abs(MOD_B)*2.5) ) * 0.8" },

/* 83*/{ "FM Soft Pad (A=Brightness B=Chorus)", "( sin(x + sin(x*1.99) * (0.5 + MOD_A*1.5)) + sin(x*(1.0 + (RAND_OFFSET-0.5)*0.002*abs(MOD_B)) + sin(x*1.99*(1.0 + (RAND_OFFSET-0.5)*0.003*abs(MOD_B))) * (0.5 + MOD_A*1.5)) ) * 0.45" },

/* 84*/{ "FM Bipolar Sweep Pad", "sin(x + sin(x * (3.0 + MOD_A * 2.5)) * (1.5 + (MOD_B * (MOD_B > 0.0 ? 1.0 : 0.3)) * 2.0) + (MOD_B < -0.1 ? (sin(x * 0.49) * (abs(MOD_B) * 2.5)) : 0.0) ) * 0.75 * (1.0 - abs(MOD_A)*0.2)" },

/* 85*/{ "FM Clangorous Hit (A=Metal B=Dissonance)", "tanh( sin(x + sin(x * (1.414 + MOD_B*1.5)) * (1.0 + abs(MOD_B)*2.0) + sin(x * 4.75) * ((MOD_A+1.0)*0.5 * 1.5) ) * (1.0 + ((MOD_A+1.0)*0.5*0.3) ) ) * 0.85" },

/* 86*/ { "FM: Noise", "sin(x + lfsr_noise(LFSR_4BIT, 10.0) * MOD_A)" }, // Noise modulation FM.

/* 87*/{ "FM Evolving SciFi (A=Evolve B=Harmonics)", "sin(x + sin(x * (1.5 + sin(x*0.05 + RAND_OFFSET)* (1.0+MOD_A*2.0) )) * 2.0 + sin(x * (6.0 + RAND_OFFSET*0.1)) * (abs(MOD_B)*1.5) ) * 0.75" },

    // LFSR Examples (112-127)

/* 88*/ { "FM: Metallic Bell (A=Decay B=Index2 C=Ratio)", "sin(x + sin(x * (2.414 + MOD_C * 2.0) + sin(x * (5.75 - MOD_C * 3.0)) * (1.0 + 2.0*MOD_B)) * (4.0 * exp(-x * (1.5 + 4.0*MOD_A))))" }, // A classic inharmonic bell tone.
              // MOD_A: Controls the decay speed of the modulation, making it percussive. MOD_B: Controls the amount of the highest modulator (Op3). MOD_C: Sweeps the frequency ratio, creating shifting metallic tones.

/* 89*/ { "FM: Glitchy Noise (A=Index B=Bit C=Rate)", "sin(x + sin(x * (1.0 + 15.0*MOD_C) + floor(sin(x*27.0)* (8.0*MOD_B))/8.0) * (5.0*MOD_A))" }, // Uses FM to create harsh, digital noise effects.
              // MOD_A: The overall intensity of the FM effect. MOD_B: Simulates bitcrushing by using floor() on a modulator. MOD_C: Controls the frequency of the noisy modulator.


/* 90*/ { "FM: Metallic 1", "sin(x + sin(x * 1.414) * MOD_A * 5.0)" }, // Inharmonic metallic FM.

/* 91*/ { "FM: Metallic 2", "sin(x + sin(x * 2.718) * MOD_A * 5.0)" }, // Another flavor of metallic FM.

/* 92*/ { "Weird: AM Chaos", "sin(x) * sin(x * (1.0 + MOD_A * 100.0))" }, // High-frequency AM sidebands.

/* 93*/ { "Sci-Fi Drone", "( (x/PI - 1.0 + 0.3*sin(x*11.0 + RAND_OFFSET*PI)) * 0.6 + ( ( ((MOD_A > 0) ^ (MOD_B > 0)) ? ( (x*0.5 < PI ? 0.4 : -0.4) ) : ( sin(x*0.5 + MOD_B*PI_OVER_2) * 0.4 ) ) * (0.5 + 0.5*abs(MOD_A)) ) ) * (0.4 + 0.6 * (0.5 + 0.5 * tanh( 3.0 * ( (MOD_A * 0.8 - MOD_B * 0.6) - cos(x*(0.5 + ((MOD_A > 0) ^ (MOD_B < 0) ? 1.5 : 0.5) )) ) ))) * 0.7" },

/* 94*/ { "Evolving Metallic Bell", "sigma(k, 1.0, 6.0, 1.0, sin(x * k * (1.0 + k*0.14159 + MOD_A*0.1*k)) * exp(-x * k * (0.3 + MOD_B*0.7)) / k) * (1.0 + 0.3*sin(x*0.5 + RAND_OFFSET*PI)) * 0.4" },

/* 95*/ { "Alien Communication", "lfsr_val(LFSR_11BIT, x/TWO_PI*2.0 + MOD_A, RAND_OFFSET)*2.0 - 1.0 + sin(x*(1.0 + lfsr_noise(LFSR_8BIT, 4.0)*0.5)) * (0.5 + 0.5*lfsr_clock(LFSR_7BIT, 0.4 + MOD_B*0.4)) + lfsr_noise(LFSR_4BIT, 16.0)*0.2" },



    // --- Bank 6: Additive & Harmonic (96-111) ---
/* 96*/ { "Sine Harmonics", "0.5 * (sin(x) + (0.5 + 0.3 * MOD_A) * sin(2*x) + 0.3*sin(3*x) + 0.5 * MOD_B * sin(4*x))" }, // Add 4th harmonic

/* 97*/ { "Harmonic Noise Blast", "0.5 * (sin(2*x) + sin(4.6*x + RAND_OFFSET) + MOD_A * sin(7.85*x - RAND_OFFSET*2) + 0.5 * MOD_B * sin(3*x))" }, // Add 3rd harmonic

/* 98*/ { "Brass", "0.3 * (sin(x) + 0.8*sin(2*x + 0.3 * MOD_A) + 0.6*sin(3*x) + 0.5*sin(4*x + PI * MOD_B) + 0.4*sin(5*x) + 0.3*sin(6*x))" }, // Phase shift on 4th harmonic

/* 99*/ { "Bowed String", "0.5 * (sin(x) + sin(2*x)/2 + (0.5 + 0.3 * MOD_A) * sin(3*x)/3 + sin(4*x + PI * MOD_B)/4 + sin(5*x)/5 + sin(6*x)/6)" }, // Phase shift on 4th harmonic

/*100*/ { "Additive Square", "0.8 * (sin(x) + sin(3*x)/3 + MOD_A * sin(5*x + PI * MOD_B)/5 + sin(7*x)/7)" }, // Phase shift on 5th harmonic

/*101*/ { "Electric Pianoish", "0.6 * (sin(x) + 0.5*sin(3*x + 0.3 * MOD_A + PI * MOD_B) + 0.25*sin(5*x) + 0.125*sin(7*x))" }, // Phase shift on 3rd harmonic

/*102*/ { "Classic Pad", "0.7 * (sin(x) + (0.5 + 0.25 * MOD_A) * sin(2*x + PI * MOD_B) + 0.25*sin(3*x) + 0.125*sin(4*x))" }, // Phase shift on 2nd harmonic

/*103*/ { "Additive Saw (A=Harms B=Shape)", "sigma(k, 1.0, 1.0 + 8.0*abs(MOD_A), 1.0, sin(x*k) / pow(k, 1.0 + abs(MOD_B))) * 0.5" },

/*104*/ { "Random Phase Additive (A=RndAmt B=Harm)", "sigma(k, 1.0, 2.0 + 6.0*abs(MOD_B), 1.0, sin(x*k + MOD_A * RAND_OFFSET * k * PI) / k) * 0.6" },

/*105*/ { "Grit Additive (A=Grit B=Tone)", "tanh( sigma(k, 1.0, 3.0+4.0*abs(MOD_B), 1.0, sin(x*k)/k ) * (1.0 + abs(MOD_A)*2.0) ) * 0.8" },

/*106*/ { "Simple Minor Triad", "( sin(x) + sin(x * 1.189207 * (1.0 + MOD_A*0.005) + RAND_OFFSET*0.05) * 0.8 + sin(x * 1.498307 * (1.0 - MOD_A*0.004) + RAND_OFFSET*0.07) * 0.7 ) * (0.33 / (1.0 + abs(MOD_B*0.5))) + (MOD_B > 0.5 ? sin(x * 1.781797) * 0.2 : 0.0)" },

/*107*/ { "Add: Spec 1", "sin(x) + 0.5*sin(x*2.0) + 0.25*sin(x*3.0) + 0.125*sin(x*4.0)" }, // Basic harmonic series.

/*108*/ { "Add: Spec 2", "sin(x) + 0.3*sin(x*3.0) + 0.2*sin(x*5.0) + 0.1*sin(x*7.0)" }, // Odd harmonics.

/*109*/ { "Add: Bell", "sin(x) + 0.5*sin(x*2.5) + 0.3*sin(x*4.2)" }, // Inharmonic bell spectrum.

/*110*/ { "Add: Organ", "sin(x) + 0.5*sin(x*2.0) + 0.2*sin(x*4.0) + 0.1*sin(x*8.0)" }, // Octave stacking.

/*111*/ { "Add: Random Phase", "sin(x) + sin(x*2.0 + MOD_A) + sin(x*3.0 + MOD_B)" }, // Harmonics with phase control.


    // --- Bank 7: Formant, Sync & Filter (112-127) ---
/*112*/ { "Formantish", "0.33 * (sin(2*x) + sin(3*x + 0.4 * MOD_A) + sin(5*x + PI * MOD_B))" }, // Phase shift on 5x harmonic

/*113*/ { "Vocal Ah", "0.35 * (0.6*sin(x) + 0.8*sin(2*x) + sin(3*x) + 0.9*sin(5*x + 0.3 * MOD_A + PI * MOD_B) + 0.2*sin(10*x))" }, // Phase shift on 5th harmonic

/*114*/ { "Reso Filter Sweep (A=Reso B=Cutoff)", "( (x/PI-1.0) + 0.7*sin(x * (2.0+abs(MOD_A)*5.0)) ) * 0.4 * (1.0 + tanh( 3.0 * ( MOD_B - cos(x) * (1.0 - FREQUENCY/1500.0) ) ))" },

/*115*/ { "Formant Vowel (A=Phsr1 B=Phsr2)", "0.4 * sigma(k, 1.0, 5.0, 1.0, sin(x*k) * (1.0 + (k>1.5 && k<2.5 ? 2.0*abs(MOD_A) : 0.0) + (k>3.5 && k<4.5 ? 2.0*abs(MOD_B) : 0.0)) / (k+1.0)) * (1.0 + 0.1*(FREQUENCY/1000.0))" },

/*116*/ { "Sync Sweep No Slant", "( sin(x * (1.0 + 5.0*abs(MOD_A))) > (MOD_B * 0.9) ? 1.0 : -1.0 ) * 0.6" },

/*117*/ { "Sync Sweep Cos Shape", "( ( sin(x * (1.0 + 5.0*abs(MOD_A))) > (MOD_B * 0.9) ? 1.0 : -1.0 ) * 0.6 + cos(x)*0.3 )"},

/*118*/ { "Smoothed Sync (A=SyncFreq B=Duty)", "( tanh( 10.0 * (sin(x * (1.0 + 5.0*abs(MOD_A))) - (MOD_B * 0.9)) ) * 0.7 + (x/PI-1.0)*0.2 )" },


/*119*/ { "Limited Sync (A=SyncFreq B=Duty)", "( ( sin(x * (1.0 + 2.5*abs(MOD_A))) > (MOD_B * 0.9) ? 1.0 : -1.0 ) * 0.6 + (x/PI-1.0)*0.3 )" },

/*120*/ { "Sync Sweep (A=SyncFreq B=Duty)", "( ( sin(x * (1.0 + 5.0*abs(MOD_A))) > (MOD_B * 0.9) ? 1.0 : -1.0 ) * 0.6 + (x/PI-1.0)*0.3 )" },

/*121*/ { "Oooh Choir Formant", "sigma(k, 1.0, 8.0 + 4.0*abs(MOD_B), 1.0, ( sin(x*k + RAND_OFFSET*0.1*k) * ( (1.0 / (1.0 + pow( (k - (2.5 + MOD_A*1.5)) / (0.8 + abs(MOD_A*0.3)) , 2.0))) + (0.7 / (1.0 + pow( (k - ( ((MOD_A > 0) ^ (MOD_B > 0.3)) ? (6.0 - MOD_A*2.0) : (5.0 + MOD_A*1.0) ) ) / 1.2 , 2.0))) ) ) / pow(k, 0.7 + 0.2*abs(MOD_B)) ) * 0.25 * (1.0 + 0.15*sin(x*6.0 + RAND_OFFSET*PI))" },

/*122*/ { "PD Vocal Formant", "sin(x + sin(x * MOD_A * 8.0))" }, // Simple PD Formant

/*123*/ { "Sync Soft", "sin(x * (1.0 + MOD_A * 3.0)) * (1.0 - x/TWO_PI)" }, // Windowed sync effect.

/*124*/ { "Fractal Sine", "sin(x + sin(x*3.0 + sin(x*9.0)*MOD_A)*MOD_B)" }, // Nested sine modulations.

/*125*/{ "FM Breathy Flute (A=Air B=PitchMod)", "sin(x*(1.0 + sin(x*0.2 + RAND_OFFSET*TWO_PI)*0.005*abs(MOD_B)) + ( sin(x*2.95) + (rand()*2.0-1.0)*abs(MOD_A)*0.7 ) * 1.2 ) * 0.8" },

/*126*/ { "Add: Saw 8", "sigma(k, 1, 8, 1, sin(x*k)/k)" }, // 8-harmonic sawtooth approximation.

/*127*/ { "Add: Square 8", "sigma(k, 1, 15, 2, sin(x*k)/k)" }, // 8-harmonic square approximation.


    // --- Bank 8: Percussion & Noise (128-143) ---
/*128*/ { "Kick Drum", "0.6 * (sin(x + 0.3 * MOD_A) + 0.4*sin(2*x) + 0.2*sin(3*x + PI * MOD_B))" }, // Phase shift on 3rd harmonic

/*129*/ { "Snare Drum", "min(1.0, max(-1.0, (sin(5*x + 0.5 * MOD_A + PI * MOD_B) + sin(7.5*x)) * 2.0)) * (1.0 - x / (TWO_PI * 1.27))" }, // Phase shift on 5x harmonic

/*130*/ { "Clap", "min(1.0, max(-1.0, (sin(4*x) + sin(6*x + 0.4 * MOD_A + PI * MOD_B) + sin(8*x)) * 1.5)) * (1.0 - x / (TWO_PI * 1.6))" }, // Phase shift on 6x harmonic

/*131*/ { "Tom Drum", "0.7 * (sin(x + 0.3 * MOD_A + PI * MOD_B) + 0.3*sin(2*x) + 0.1*sin(3*x))" }, // Phase shift on fundamental

/*132*/ { "Cymbalish", "0.25 * (sin(x) + sin(1.57*x) + sin(2.25*x) + MOD_A * sin(3.14*x + PI * MOD_B))" }, // Phase shift on 3.14x harmonic

/*133*/ { "Double Waves", "max(-1.0, 0.7 * (sin(x) + 0.8 * sin(1.1*x + RAND_OFFSET)) * (1.0 - x / TWO_PI) + 0.5 * (sin(2*x) + MOD_A * sin(2.2*x + RAND_OFFSET + PI * MOD_B)))" }, // Phase shift on 2.2x

/*134*/ { "Metal Impact", "0.25 * (sin(x) + sin(1.25*x + 0.5 * MOD_A + PI * MOD_B) + sin(1.85*x) + sin(2.1*x))" }, // Phase shift on 1.25x

/*135*/ { "Bell Tone", "0.5 * (sin(x + PI * MOD_B) + 0.5*sin(1.05*x + 0.4 * MOD_A) + 0.3*sin(1.6*x) + 0.2*sin(2.15*x))" }, // Phase shift on fundamental

/*136*/ { "Metallic Perc", "0.25 * (sin(x) + sin(1.5*x) + MOD_A * sin(2.5*x + PI * MOD_B) + sin(3.5*x))" }, // Phase shift on 2.5x harmonic

/*137*/ { "Sigma Bell (A=Decay B=Metal)", "sigma(k, 1.0, 4.0, 1.0, sin(x * k * (1.0 + 0.2*k*abs(MOD_B))) / (k+1.0)) * exp(-x * 2.0 * (1.0 + abs(MOD_A)*4.0) / (1.0 + FREQUENCY/300.0)) * 0.7" },

/*138*/ { "Classic Noise Sim", " ( ( ( (x * FREQUENCY * (15.0 + MOD_A*10.0) * (1.0 + RAND_OFFSET*0.1) ) % (1.0 + abs(MOD_B)*0.3) ) - 0.5 ) * ( 0.6 + 0.4 * ( ((MOD_A > 0.1) ^ (MOD_B > 0.1)) ? sin(x * FREQUENCY * (23.0 - MOD_A*5.0) * (1.0 - RAND_OFFSET*0.05)) : cos(x * FREQUENCY * (7.0 + MOD_B*3.0)) ) ) + ( (MOD_B < -0.2) ? ( ( (x * FREQUENCY * (37.0 + MOD_A*3.0) ) % 0.8 ) - 0.4 ) * 0.3 : 0.0 ) ) * 0.55 * (1.0 - abs(MOD_A)*0.1) " },

/*139*/ { "Distorted Pitch", "( floor( ( ( (x * FREQUENCY * (8.0 + MOD_A*24.0)) % (1.0 + abs(MOD_A)*0.7 + (RAND_OFFSET * 0.1 * (MOD_A < 0.1 ? 1.0:0.0) ) ) ) ) * (2.5 + abs(MOD_B)*1.5) ) / (2.0 + abs(MOD_B)*1.0) * (1.0 + MOD_B * 0.15 * sin(x * FREQUENCY * (31.0 + MOD_A*5.0))) ) * 0.7" },

/*140*/ { "Gritty Rumble Noise", "( (rand()*2.0-1.0) * 0.4 + (rand()*2.0-1.0) * sin(x*0.1 + RAND_OFFSET*TWO_PI) * 0.3 + (rand()*2.0-1.0) * sin(x*0.5 + RAND_OFFSET*TWO_PI*0.5) * 0.2 ) * (1.0 + MOD_A*0.5)"},

/*141*/ { "Filtered Static Noise", "sigma(k, 0.0, 7.0, 1.0, ( (rand()*2.0-1.0) * pow(0.6 + MOD_A*0.35, k) ) ) * 0.125 + ( (rand()*2.0-1.0) * sin(x * (100.0 + MOD_B*900.0) / (FREQUENCY+0.01) ) * MOD_B * 0.2 )"},


/*142*/ { "Wooden Percussion", "0.7 * (sin(x + 0.2 * MOD_A) + 0.5*sin(2*x) + 0.25*sin(3*x + PI * MOD_B) + 0.125*sin(4*x))" }, // Phase shift on 3rd harmonic

/*143*/ { "Glitchy Percussion", "lfsr_clock(LFSR_8BIT, 0.8) * sin(x*(1.0 + lfsr_noise(LFSR_12BIT, 32.0)*2.0) + lfsr_val(LFSR_16BIT, x/PI, MOD_A)*PI) * exp(-x*(2.0 + MOD_B*8.0)) + lfsr_clock(LFSR_4BIT, 0.5)*0.3*exp(-x*12.0)" },


    // --- Bank 9: Atmospheric & Textures (144-159) ---
/*144*/ { "Plucked String (A=Damp B=Body)", "sigma(k, 1.0, 3.0 + 4.0*abs(MOD_B), 1.0, sin(x * k) * pow(0.85, k*(1.0+abs(MOD_A))) ) * exp(-x*0.5) * 0.8" },

/*145*/ { "Sigma A=End B=Decay", "sigma(k, 1.0, 1.0 + abs(MOD_A) * 5.0, 1.0, sin(x * k) / (pow(k, 1.0 + abs(MOD_B)*2.0) + 0.1)) * 0.5" },

/*146*/ { "Noisy Pad (A=NoiseAmt B=Flt)", "( sigma(k,1.0, 4.0, 1.0, sin(x*k)/k) * (1.0-abs(MOD_A)) + (rand()-0.5)*1.5*abs(MOD_A) ) * (0.5 + 0.5*tanh(3.0*(1.0-abs(MOD_B)*(1.0 - FREQUENCY/1000.0)))) * 0.6" },

/*147*/ { "Rich String Ensemble", "sigma(k, 1.0, 5.0 + 3.0*abs(MOD_B), 1.0, (sin(x*k + MOD_A * 0.02 * k * RAND_OFFSET) + sin(x*k*(1.0 + MOD_A*0.005) + MOD_A * 0.03 * k * (k*0.1)) + sin(x*k*(1.0 - MOD_A*0.005) - MOD_A * 0.025 * k * (k*0.13)) ) / (pow(k, 1.1 + 0.3*abs(MOD_B))) ) * 0.15 * (1.0 + 0.2*sin(x*0.5 + PI_OVER_2))" },

/*148*/ { "Mellow Brass Section", "tanh( sigma(k, 1.0, 4.0 + 2.0*abs(MOD_A), 1.0, sin(x*k + MOD_B*0.02*sin(x*7.0)) * (1.0 - 0.2*k + abs(MOD_A)*(0.1*k - 0.05)) / (pow(k, 0.8 + 0.4*(1.0-abs(MOD_A)))) ) * (0.6 + 0.3*abs(MOD_A)) ) * 0.85" },

/*149*/ { "Jittery Inharmonic Pitch", " sigma(k, 1.0, (4.0 + abs(MOD_A)*4.0), 1.0, sin( x * FREQUENCY * (k + (MOD_B * 0.05 * (k-1.0) * ( (k%2==0)?-1:1 ) )) + (RAND_OFFSET * 0.2 * k * ( (k%3==0)?0.5:0.1 )) ) * ( (1.0 / (k + abs(MOD_B)*0.5)) * (0.8 + RAND_OFFSET*0.2*MOD_A) ) ) * (0.6 / (1.0 + abs(MOD_A)*0.5))"},

/*150*/ { "LFSR Granular Texture", "sin(x + lfsr_noise(LFSR_12BIT, 8.0 + MOD_A*16.0) * (0.5 + MOD_B*1.5)) * lfsr_clock(LFSR_8BIT, 0.3 + MOD_A*0.4) + lfsr_val(LFSR_16BIT, x/TWO_PI + RAND_OFFSET, MOD_B*0.5)*0.3 - 0.15" },

/*151*/ { "Morphing Harmonics", "sigma(k, 1.0, 12.0, 1.0, sin(x*k + sin(x*k*0.25)*MOD_A*k*0.1) * (MOD_B > 0 ? (1.0/(k + abs(MOD_B)*8.0)) : exp(-k*abs(MOD_B)*0.5)) * (1.0 + 0.2*sin(x*k*3.0 + RAND_OFFSET*k))) * 0.15" },

/*152*/ { "Breathing Pad", "(sin(x) + sin(x*1.498)*0.7 + sin(x*2.006)*0.5) * (0.6 + 0.4*sin(x*0.125 + MOD_A*PI)) * (1.0 + MOD_B*0.3*sin(x*0.0625 + RAND_OFFSET*TWO_PI)) + lfsr_noise(LFSR_4BIT, 0.5)*0.05" },

/*153*/ { "Chaotic Oscillator", "tanh((sin(x + sin(x*1.618 + MOD_A*PI)*0.5) + sin(x*0.618 + MOD_B*PI_OVER_2)*0.7) * (2.0 + MOD_A*3.0)) * (0.8 + 0.2*lfsr_val(LFSR_7BIT, x/TWO_PI*3.0, RAND_OFFSET))" },

/*154*/ { "Crystalline Arpeggio", "sigma(k, 1.0, 8.0, 1.0, sin(x*pow(2.0, floor(k + MOD_A*4.0) * 0.33333) + RAND_OFFSET*k*0.1) * exp(-x*k*(0.8 + MOD_B*1.2)) * lfsr_clock(LFSR_5BIT, 0.6 + k*0.05)) * 0.18" },

/*155*/ { "Add: Shepard Cycle", "sin(x * (1.0 + MOD_A)) + sin(x * (2.0 + MOD_A)) * 0.5" }, // Shepard tone component.


/*156*/ { "Water Droplet", "(x < (TWO_PI*0.05) ? -1.0 : (sin((x - (TWO_PI*0.05)) * 5.0 + 1 * MOD_A + PI * MOD_B) * 0.6 * exp(-(x - (TWO_PI*0.05)) * 0.5)))" }, // Phase shift on ripple

/*157*/ { "Alien Chatter", "(x < (TWO_PI * (0.15 + 0.05 * MOD_A)) ? 1.0 : (x < (TWO_PI * (0.25 + 0.05 * MOD_A)) ? -0.5 : (x < (TWO_PI * (0.35 + 0.05 * MOD_A)) ? 0.7 : (x < PI ? -1.0 : sin(3*x + 0.4 * MOD_A) * 0.6)))) + 0.5 * MOD_B * sin(2*x)" }, // Harmonic texture

/*158*/ { "Weird: Chirp", "sin(x * x / PI)" }, // Quadratic phase chirp.

/*159*/ { "Wind AM", "sin(15*x + 0.75 * MOD_A + PI * MOD_B) * (0.5 + 0.5 * sin(x))" }, // Phase shift on high harmonic



    // --- Bank 10: LFSR Rhythmic (160-175) ---
/*160*/ { "LFSR Rhythm Gate", "sin(x) * lfsr_clock(LFSR_8BIT, 0.3 + 0.4 * MOD_A) + 0.2 * lfsr_noise(LFSR_4BIT, 2.0 + 3.0 * MOD_B)" },

/*161*/ { "LFSR Harmonic Chaos", "sigma(k, 1.0, 6.0, 1.0, sin(x * k + lfsr_val(LFSR_12BIT, x / TWO_PI + k * 0.1, RAND_OFFSET) * PI * MOD_A) / k) * (0.4 + 0.3 * lfsr_noise(LFSR_6BIT, 0.5 + 1.5 * MOD_B))" },

/*162*/ { "LFSR Digital Texture", "tanh(2.0 * (lfsr_val(LFSR_15BIT, x / PI, MOD_A + RAND_OFFSET) * 2.0 - 1.0 + 0.5 * sin(x + lfsr_clock(LFSR_7BIT, 0.5) * PI_OVER_2) + 0.3 * lfsr_noise(LFSR_5BIT, 4.0 + 6.0 * MOD_B)))" },

/*163*/ { "LFSR Poly Rhythm", "sin(x) * (0.7 + 0.3 * lfsr_clock(LFSR_11BIT, 0.25)) + sin(x * 1.5) * (0.6 + 0.4 * lfsr_clock(LFSR_9BIT, 0.33 + 0.3 * MOD_A)) + 0.3 * lfsr_noise(LFSR_13BIT, 1.0 + 2.0 * MOD_B)" },

/*164*/ { "LFSR Phase Modulation", "sin(x + lfsr_val(LFSR_16BIT, x / TWO_PI + FREQUENCY / 1000.0, RAND_OFFSET) * MOD_A * TWO_PI + lfsr_noise(LFSR_8BIT, 0.1 + MOD_B) * 0.5)" },

/*165*/ { "LFSR Granular", "sigma(k, 1.0, 4.0, 1.0, sin(x * k) * lfsr_clock(LFSR_10BIT, 0.1 + k * 0.15) * lfsr_val(LFSR_14BIT, x / PI + k * MOD_A, RAND_OFFSET)) * 0.6" },

/*166*/ { "LFSR Rhythmic Harmonics", "sigma(k, 1.0, 8.0, 1.0, sin(x * k) * lfsr_clock((k % 3 == 0) ? LFSR_7BIT : LFSR_5BIT, 0.2 + MOD_A * 0.3) / k) * (0.5 + 0.3 * lfsr_noise(LFSR_12BIT, 0.8 + MOD_B))" },

/*167*/ { "LFSR Spectral Shift", "sin(x * (1.0 + lfsr_val(LFSR_13BIT, x / PI + FREQUENCY / 500.0, MOD_A) * 0.5)) + 0.4 * sin(x * 2.0 + lfsr_noise(LFSR_9BIT, 2.0 + MOD_B * 3.0) * PI)" },

/*168*/ { "LFSR Euclidean Beat", "sin(x) * (lfsr_clock(LFSR_16BIT, 0.4) && lfsr_clock(LFSR_11BIT, 0.3 + MOD_A * 0.4) ? 1.0 : 0.3) + 0.2 * lfsr_noise(LFSR_6BIT, 3.0 + MOD_B * 2.0)" },

/*169*/ { "LFSR Feedback Synth", "tanh(sin(x + lfsr_val(LFSR_15BIT, x / TWO_PI, RAND_OFFSET) * MOD_A * PI) + 0.3 * lfsr_noise(LFSR_8BIT, 1.5 + MOD_B * 2.0) * sin(x * 0.5)) * 0.8" },

/*170*/ { "LFSR Algorithmic Lead", "sin(x * (1.0 + lfsr_clock(LFSR_12BIT, 0.25) * 0.5)) + sin(x * 2.0) * lfsr_val(LFSR_10BIT, x / PI + MOD_A, RAND_OFFSET) + 0.3 * lfsr_noise(LFSR_7BIT, 4.0 + MOD_B * 3.0)" },

/*171*/ { "LFSR Morphing Pad", "sigma(k, 1.0, 5.0 + 3.0 * abs(MOD_B), 1.0, sin(x * k + lfsr_val(LFSR_14BIT, x / TWO_PI + k * 0.1, RAND_OFFSET + MOD_A) * PI) * lfsr_clock(LFSR_8BIT, 0.1 + k * 0.05) / (k + 1.0)) * 0.4" },

/*172*/ { "LFSR Breakbeat", "sin(x) * (lfsr_clock(LFSR_9BIT, 0.5) ? 1.0 : (lfsr_clock(LFSR_7BIT, 0.7 + MOD_A * 0.2) ? 0.6 : 0.2)) + 0.4 * lfsr_noise(LFSR_11BIT, 2.5 + MOD_B * 2.0) * sin(x * 3.0)" },

/*173*/ { "LFSR Probability Gate", "sin(x + MOD_A * PI) * (lfsr_val(LFSR_16BIT, x / PI + FREQUENCY / 440.0, RAND_OFFSET) > (0.3 + MOD_B * 0.4) ? 1.0 : 0.1) + 0.3 * lfsr_noise(LFSR_6BIT, 3.0)" },

/*174*/ { "LFSR Polyrhythmic Chaos", "sin(x) * lfsr_clock(LFSR_13BIT, 0.3) + sin(x * 1.333) * lfsr_clock(LFSR_11BIT, 0.4 + MOD_A * 0.3) + sin(x * 1.666) * lfsr_clock(LFSR_9BIT, 0.35 + MOD_B * 0.25) * 0.8" },

/*175*/ { "LFSR Glitch Matrix", "tanh(sigma(k, 1.0, 4.0, 1.0, sin(x * k) * lfsr_val(3, x / TWO_PI + k * MOD_A * 0.1, RAND_OFFSET) * lfsr_clock(0, 0.2 + k * 0.1)) * (1.0 + MOD_B * 2.0)) * 0.7" },

    // Atari 8-bit POKEY Sounds (128-143)


    // --- Bank 11: Arcade I (Classic) (176-191) ---
/*176*/ { "Pac-Man Wakka", "sin(x * (1.0 + 0.3 * sin(x * 8.0 + MOD_A * PI))) * (x < (PI * (0.6 + 0.2 * MOD_B)) ? 1.0 : 0.3) * 0.8" },

/*177*/ { "Pac-Man Power Pellet", "sin(x * (2.0 + sin(x * 12.0) * 1.5 + MOD_A * 2.0)) * (0.7 + 0.3 * sin(x * (4.0 + MOD_B * 4.0))) * 0.9" },

/*178*/ { "Pac-Man Death", "sin(x * (2.0 - x * (3.0 + MOD_A * 2.0) / TWO_PI)) * exp(-x * (0.8 + MOD_B * 1.0)) * (1.0 + 0.4 * sin(x * 15.0))" },

/*179*/ { "Pac-Man Ghost", "sin(x * (0.8 + 0.2 * sin(x * (6.0 + MOD_A * 4.0)))) * (0.6 + 0.4 * sin(x * (0.5 + MOD_B * 1.0))) * 0.7" },

/*180*/ { "Space Invaders Shot", "sin(x * (3.0 + MOD_A * 2.0) + sin(x * (25.0 + MOD_B * 15.0)) * 0.8) * exp(-x * 2.5) * (1.0 + 0.3 * sin(x * 40.0))" },

/*181*/ { "Space Invaders March", "((x < (TWO_PI * 0.25) ? 0.8 : (x < (TWO_PI * 0.5) ? 0.4 : (x < (TWO_PI * 0.75) ? 0.6 : 0.2))) + 0.3 * sin(x * (2.0 + MOD_A)) * (0.8 + 0.2 * MOD_B)) * 0.9" },

/*182*/ { "Space Invaders UFO", "sin(x * (0.3 + 0.1 * sin(x * (3.0 + MOD_A * 2.0)))) + 0.4 * sin(x * (8.0 + MOD_B * 4.0) + sin(x * 0.2) * 2.0) * 0.8" },

/*183*/ { "Space Invaders Explosion", "((floor(x * FREQUENCY * (80.0 + MOD_A * 40.0) / 22050.0) % 31) - 15) * 0.1 * exp(-x * (1.5 + MOD_B * 2.0)) + sin(x * 0.2) * 0.3 * exp(-x * 0.8)" },

/*184*/ { "Asteroids Thrust", "sin(x * (0.5 + 0.3 * sin(x * (12.0 + MOD_A * 8.0)))) * (0.4 + 0.6 * ((floor(x * 20.0) % 3) > 0 ? 1.0 : 0.3)) + 0.2 * ((floor(x * FREQUENCY / 1000.0) % 7) - 3) * 0.1 * MOD_B" },

/*185*/ { "Asteroids Shoot", "sin(x * (4.0 + MOD_A * 3.0) + sin(x * (35.0 + MOD_B * 20.0)) * 1.2) * exp(-x * 3.0) * (1.0 + 0.5 * sin(x * 60.0))" },

/*186*/ { "Asteroids Explosion", "((floor(x * FREQUENCY * (120.0 + MOD_A * 80.0) / 22050.0) % 127) - 63) * 0.02 * exp(-x * (1.0 + MOD_B * 1.5)) + sin(x * (0.1 + x * 2.0 / TWO_PI)) * 0.4 * exp(-x * 0.5)" },

/*187*/ { "Asteroids Hyperspace", "sin(x * (2.0 + x * (8.0 + MOD_A * 4.0) / TWO_PI) + sin(x * (45.0 + MOD_B * 25.0)) * 2.0) * exp(-x * 0.8) * (1.0 + 0.6 * sin(x * 80.0))" },

/*188*/ { "Galaxian Attack", "sin(x * (1.5 + sin(x * (6.0 + MOD_A * 4.0)) * 0.8 + x * (2.0 + MOD_B * 2.0) / TWO_PI)) * exp(-x * 1.2) * 0.9" },

/*189*/ { "Galaxian Formation", "sin(x * (0.8 + 0.2 * sin(x * (4.0 + MOD_A * 2.0)))) + 0.3 * sin(x * (2.4 + 0.4 * sin(x * (1.0 + MOD_B)))) * 0.8" },

/*190*/ { "Centipede Laser", "sin(x * (5.0 + MOD_A * 3.0) + sin(x * (40.0 + MOD_B * 20.0)) * 0.6) * exp(-x * 2.8) * (1.0 + 0.4 * ((floor(x * 30.0) % 2) * 2.0 - 1.0))" },

/*191*/ { "Centipede Flea Drop", "sin(x * (1.2 + x * (1.5 + MOD_A * 2.0) / TWO_PI) + sin(x * (18.0 + MOD_B * 10.0)) * 1.0) * exp(-x * 1.0) * 0.8" },


    // --- Bank 12: Arcade II (Action) (192-207) ---
/*192*/ { "Defender Thrust", "sin(x * (0.4 + 0.3 * sin(x * (8.0 + MOD_A * 6.0)))) * (0.5 + 0.5 * ((floor(x * 15.0) % 5) > 2 ? 1.0 : 0.4)) + 0.3 * ((floor(x * FREQUENCY / 800.0) % 15) - 7) * 0.02 * MOD_B" },

/*193*/ { "Defender Smart Bomb", "sin(x * (2.0 + x * 4.0 / TWO_PI + sin(x * (30.0 + MOD_A * 15.0)) * 3.0)) * exp(-x * (0.6 + MOD_B * 0.8)) * (1.0 + 0.5 * sin(x * 100.0))" },

/*194*/ { "Frogger Hop", "sin(x * (1.8 + MOD_A * 1.0) + sin(x * (12.0 + MOD_B * 8.0)) * 1.5) * exp(-x * 2.0) * (1.0 + 0.3 * sin(x * 25.0)) * 0.9" },

/*195*/ { "Frogger Traffic", "sin(x * (0.6 + 0.1 * sin(x * (2.0 + MOD_A)))) + 0.2 * ((floor(x * FREQUENCY / 1200.0) % 11) - 5) * 0.1 * (0.8 + 0.2 * MOD_B)" },

/*196*/ { "Donkey Kong Hammer", "sin(x * (1.5 + MOD_A * 1.5)) * exp(-x * (3.0 + MOD_B * 2.0)) + 0.4 * ((floor(x * FREQUENCY / 600.0) % 8) - 3.5) * 0.1 * exp(-x * 1.5)" },

/*197*/ { "Donkey Kong Jump", "sin(x * (2.5 + x * (2.0 + MOD_A * 3.0) / TWO_PI) + sin(x * (20.0 + MOD_B * 10.0)) * 0.8) * exp(-x * 1.8) * 0.9" },

/*198*/ { "Missile Command Explosion", "((floor(x * FREQUENCY * (90.0 + MOD_A * 60.0) / 22050.0) % 63) - 31) * 0.05 * exp(-x * (1.2 + MOD_B * 1.0)) + sin(x * (0.15 + x * 1.5 / TWO_PI)) * 0.5 * exp(-x * 0.7)" },

/*199*/ { "Tempest Shoot", "sin(x * (4.5 + MOD_A * 2.5) + sin(x * (50.0 + MOD_B * 25.0)) * 0.7) * exp(-x * 3.5) * (1.0 + 0.4 * sin(x * 70.0)) * 0.8" },

/*200*/ { "Tempest Flip", "sin(x * (3.0 + sin(x * (15.0 + MOD_A * 10.0)) * 2.0 + x * (1.0 + MOD_B) / TWO_PI)) * exp(-x * 1.5) * 0.9" },

/*201*/ { "Berzerk Robot Voice", "tanh(sin(x * (1.2 + MOD_A * 0.8) + sin(x * (8.0 + MOD_B * 6.0)) * 1.5) * 2.5) * (0.7 + 0.3 * ((floor(x * 10.0) % 4) > 1 ? 1.0 : 0.5)) * 0.8" },

/*202*/ { "Robotron Shoot", "sin(x * (6.0 + MOD_A * 4.0) + sin(x * (45.0 + MOD_B * 30.0)) * 0.5) * exp(-x * 4.0) * (1.0 + 0.3 * ((floor(x * 40.0) % 2) * 2.0 - 1.0))" },

/*203*/ { "Phoenix Bird Cry", "sin(x * (0.9 + 0.4 * sin(x * (3.0 + MOD_A * 2.0)) + x * (1.5 + MOD_B) / TWO_PI)) * exp(-x * 0.9) * (1.0 + 0.2 * sin(x * 12.0)) * 0.8" },

/*204*/ { "Gorf Laser", "sin(x * (3.5 + MOD_A * 2.0) + sin(x * (28.0 + MOD_B * 15.0)) * 1.0) * exp(-x * 2.2) * (1.0 + 0.5 * sin(x * 35.0)) * 0.9" },

/*205*/ { "Scramble Engine", "sin(x * (0.7 + 0.2 * sin(x * (5.0 + MOD_A * 3.0)))) * (0.6 + 0.4 * ((floor(x * 12.0) % 6) > 2 ? 1.0 : 0.5)) + 0.25 * ((floor(x * FREQUENCY / 900.0) % 9) - 4) * 0.05 * MOD_B" },

/*206*/ { "Zaxxon Alarm", "sin(x * (2.2 + 0.8 * sin(x * (1.5 + MOD_A)) + 0.3 * sin(x * (7.0 + MOD_B * 4.0)))) * (0.8 + 0.2 * sin(x * 0.8)) * 0.9" },

/*207*/ { "Moon Patrol Bounce", "sin(x * (1.6 + MOD_A * 1.2) + sin(x * (14.0 + MOD_B * 8.0)) * 1.2) * exp(-x * (1.5 + abs(sin(x * 2.0)) * 1.0)) * (1.0 + 0.3 * sin(x * 20.0)) * 0.8" },

// POKEY Noise Emulation using LFSR (176-191) - REVISED FOR MORE AUTHENTIC POKEY CLOCKING
    // Note: FREQUENCY is the note's frequency. We need to divide by it.
    // To avoid division by zero if FREQUENCY is 0 (e.g., during table gen), add a small epsilon.
    // Let POKEY_MASTER_CLK_DIV_N be a script constant or calculated from FREQUENCY.
    // The "rate" argument to lfsr_noise now means "how many times to cycle the LFSR per x cycle"
    // So, rate = (TargetPokeyNoiseClockHz / NoteFrequencyHz) / LfsrTablePeriod

    // Define target POKEY clock rates (approx)
    // POKEY_CLK_1790K = 1789773.0 (not usually used directly for noise)
    // POKEY_CLK_64K   = 63920.0
    // POKEY_CLK_15K   = 15699.0

    // Periods for different LFSRs (as per your LfsrType enum and lfsr_configs)
    // LFSR_4BIT_PERIOD = 15.0
    // LFSR_5BIT_PERIOD = 31.0
    // LFSR_9BIT_PERIOD = 511.0
    // LFSR_17BIT_PERIOD = 131071.0 (or 65535.0 if using LFSR_16BIT for it)



    // --- Bank 13: Chiptune Tones (POKEY) (208-223) ---
/*208*/ { "POKEY Pure Tone", "(x < (TWO_PI * (0.5 + 0.3 * MOD_A)) ? 1.0 : -1.0) + 0.1 * (floor(x * FREQUENCY / 100.0) % 2 == 0 ? 0.3 : -0.3) * MOD_B" },

/*209*/ { "POKEY Filtered Noise", "((floor(x * FREQUENCY * (15.0 + MOD_A * 10.0) / 64000.0) % 2) * 2.0 - 1.0) * (0.8 + 0.2 * sin(x * (2.0 + MOD_B * 3.0)))" },

/*210*/ { "POKEY Distorted Bass", "tanh(3.0 * ((x < (TWO_PI * (0.3 + 0.4 * MOD_A)) ? 1.0 : -1.0) + 0.3 * sin(x * (1.0 + MOD_B * 2.0)))) * 0.8" },

/*211*/ { "POKEY Laser Zap", "sin(x * (1.0 + MOD_A * 10.0) + sin(x * (31.0 + MOD_B * 20.0)) * 2.0) * exp(-x * 0.8) * (1.0 + 0.5 * ((floor(x * FREQUENCY / 1000.0) % 4) - 1.5))" },

/*212*/ { "POKEY Explosion", "((floor(x * FREQUENCY * (50.0 + MOD_A * 100.0) / 64000.0) % 7) - 3) * 0.3 * exp(-x * (0.5 + MOD_B * 2.0)) + sin(x * 0.1) * 0.4 * exp(-x * 0.3)" },

/*213*/ { "POKEY Engine Rumble", "sin(x * 0.25 + sin(x * 0.5) * 0.5) * 0.6 + ((floor(x * FREQUENCY * (8.0 + MOD_A * 12.0) / 31000.0) % 3) - 1) * 0.4 * (0.7 + 0.3 * sin(x * (0.1 + MOD_B * 0.2)))" },

/*214*/ { "POKEY Bit Crush Lead", "floor(sin(x + sin(x * (2.0 + MOD_A * 4.0)) * 1.5) * (4.0 + MOD_B * 8.0)) / (4.0 + MOD_B * 8.0) * 0.9" },

/*215*/ { "POKEY Coin Pickup", "sin(x * (1.0 + floor((x * 8.0 / TWO_PI)) * 0.5 + MOD_A * 2.0)) * exp(-x * (2.0 + MOD_B * 3.0)) * (1.0 + 0.3 * sin(x * 15.0))" },

/*216*/ { "POKEY Jump Sound", "sin(x * (2.0 + MOD_A * 3.0) + sin(x * (8.0 + MOD_B * 6.0)) * 0.8) * exp(-x * 1.5) * (1.0 + 0.4 * ((floor(x * 20.0) % 2) * 2.0 - 1.0))" },

/*217*/ { "POKEY Chirp Bird", "sin(x * (0.5 + x * (2.0 + MOD_A * 4.0) / TWO_PI)) * exp(-x * (1.0 + MOD_B * 2.0)) + 0.2 * ((floor(x * FREQUENCY / 5000.0) % 5) - 2) * 0.1" },

/*218*/ { "POKEY Alien Voice", "tanh(sin(x * (0.8 + MOD_A * 1.5) + sin(x * (17.0 + MOD_B * 10.0)) * 1.2) * 2.0) * 0.7 + 0.2 * ((floor(x * FREQUENCY / 8000.0) % 8) - 3.5) * 0.1" },

/*219*/ { "POKEY Power Up", "sin(x * (0.5 + x * (4.0 + MOD_A * 6.0) / TWO_PI + sin(x * (25.0 + MOD_B * 15.0)) * 0.3)) * exp(-x * 0.4) * (1.0 + 0.3 * sin(x * 12.0))" },

/*220*/ { "POKEY Hit Sound", "sin(x * (2.0 + MOD_A * 2.0)) * exp(-x * (5.0 + MOD_B * 5.0)) + ((floor(x * FREQUENCY * 100.0 / 64000.0) % 16) - 8) * 0.1 * exp(-x * 3.0)" },

/*221*/ { "POKEY Sweep Down", "sin(x * (3.0 + MOD_A * 2.0 - x * (2.0 + MOD_B * 3.0) / TWO_PI)) * exp(-x * 0.6) * (0.8 + 0.2 * ((floor(x * 10.0) % 3) - 1))" },

/*222*/ { "POKEY Poly Counter", "((floor(x * FREQUENCY * (31.0 + MOD_A * 20.0) / 64000.0) % 31) > (15 + MOD_B * 10) ? 1.0 : -1.0) * 0.7 + 0.2 * sin(x * (0.5 + MOD_A * 0.5))" },

/*223*/ { "POKEY Four Channel", "0.25 * (sin(x) + sin(x * 1.33 + MOD_A * PI) + ((x < PI ? 1.0 : -1.0) * (0.7 + 0.3 * MOD_B)) + ((floor(x * FREQUENCY * 17.0 / 64000.0) % 2) * 2.0 - 1.0) * 0.8)" },

    // Classic Arcade Sounds (144-175)


    // --- Bank 14: Chiptune Noise (POKEY) (224-239) ---
/*224*/ { "POKEY 4-bit Noise (64k)", "lfsr_noise(LFSR_4BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + 0.5*abs(MOD_B)) ) * (0.6 + 0.39 * MOD_A)" },
        // MOD_A: Volume. MOD_B: Fine tune rate around 64k.


/*225*/ { "POKEY 5-bit Noise (64k)", "lfsr_noise(LFSR_5BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + 0.5*abs(MOD_B)) ) * (0.6 + 0.39 * MOD_A)" },
        // MOD_A: Volume. MOD_B: Fine tune rate.


/*226*/ { "POKEY 17-bit Noise (64k)", "lfsr_noise(LFSR_17BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 131071.0)) * (1.0 + 0.5*abs(MOD_B)) ) * (0.6 + 0.39 * MOD_A)" },
        // Assumes LFSR_17BIT has period 131071. If using LFSR_16BIT (period 65535), change 131071.0 to 65535.0


/*227*/ { "POKEY 9-bit Noise (15k)", "lfsr_noise(LFSR_9BIT, (15699.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 511.0)) * (1.0 + 0.5*abs(MOD_B)) ) * (0.6 + 0.39 * MOD_A)" },
        // Typically 9-bit noise is clocked slower.


/*228*/ { "POKEY Filtered 4-bit (Fast)", "lfsr_noise(LFSR_4BIT, (63920.0*2.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + 1.5*abs(MOD_B)) ) * (0.5 + 0.35 * MOD_A)" },
        // "Filtered" by making it effectively run faster than standard 64k.


/*229*/ { "POKEY Filtered 5-bit (Fast)", "lfsr_noise(LFSR_5BIT, (63920.0*2.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + 1.5*abs(MOD_B)) ) * (0.5 + 0.35 * MOD_A)" },


/*230*/ { "POKEY Tone + 4-bit (64k)", "( (x < (PI * (1.0 + 0.9*MOD_B)) ? 0.6 : -0.6) * (1.0 - abs(MOD_A*0.9)) + lfsr_noise(LFSR_4BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + RAND_OFFSET*0.2) ) * abs(MOD_A*0.9) )" },
        // MOD_A: Mix. MOD_B: PWM. Noise rate slightly randomized around 64k.


/*231*/ { "POKEY Tone + 5-bit (64k)", "( (x < (PI * (1.0 + 0.9*MOD_B)) ? 0.6 : -0.6) * (1.0 - abs(MOD_A*0.9)) + lfsr_noise(LFSR_5BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + RAND_OFFSET*0.2) ) * abs(MOD_A*0.9) )" },


/*232*/ { "POKEY Tone + 17-bit (64k)", "( (x < (PI * (1.0 + 0.9*MOD_B)) ? 0.6 : -0.6) * (1.0 - abs(MOD_A*0.9)) + lfsr_noise(LFSR_17BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 131071.0)) * (1.0 + RAND_OFFSET*0.2) ) * abs(MOD_A*0.9) )" },
        // Again, adjust 131071.0 if using LFSR_16BIT with period 65535.


/*233*/ { "POKEY 4(64k)+5(15k) Combined", "(lfsr_noise(LFSR_4BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + RAND_OFFSET*0.1) ) * (0.5 + 0.48 * MOD_A) + lfsr_noise(LFSR_5BIT, (15699.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + RAND_OFFSET*0.1) ) * (0.5 - 0.48 * MOD_A)) * (0.55 + 0.35 * abs(MOD_B))" },
        // Mix 4-bit at ~64k and 5-bit at ~15k. MOD_A: Mix. MOD_B: Volume.


/*234*/ { "POKEY \"High Pass\" 4-bit (Fast)", "lfsr_noise(LFSR_4BIT, (63920.0*3.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + 2.0*abs(MOD_A)) ) * (0.5 + 0.35 * abs(MOD_B))" },
        // High rate. MOD_A varies rate further. MOD_B volume.


/*235*/ { "POKEY 64kHz Noise (17-bit)", "lfsr_noise(LFSR_17BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 131071.0)) * (1.0 + 0.5*abs(MOD_A)) ) * (0.6 + 0.39 * abs(MOD_B))" },
        // This is essentially same as 178, just named differently for clarity.


/*236*/ { "POKEY 15kHz Noise (9-bit)", "lfsr_noise(LFSR_9BIT, (15699.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 511.0)) * (1.0 + 0.5*abs(MOD_A)) ) * (0.6 + 0.39 * abs(MOD_B))" },
        // This is essentially same as 179.


/*237*/ { "POKEY Engine Sound (Noise Gated)", "lfsr_noise(LFSR_5BIT, (15699.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + 2.0*abs(MOD_A)) ) * (0.2 + 0.7 * lfsr_val(LFSR_4BIT, (x / TWO_PI) * ((15699.0*0.5 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + 3.0*abs(MOD_B))), RAND_OFFSET)) * 0.8" },
        // 5-bit noise at ~15k (rate mod by A). Gated by 4-bit LFSR also running relative to a POKEY-like clock (rate mod by B).


/*238*/ { "POKEY Explosion (Decaying Rate/Vol)", "(lfsr_noise(LFSR_17BIT, ((63920.0*2.0*exp(-x*3.5) + 15699.0*0.5) / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 131071.0)) * (1.0 + 0.5*abs(MOD_A)) ) * exp(-x * (1.2 + 3.0*abs(MOD_B))) + lfsr_noise(LFSR_4BIT, ((15699.0*exp(-x*2.5) + 63920.0*0.1) / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + RAND_OFFSET*0.2) ) * 0.3 * exp(-x * (1.8 + 2.5*abs(MOD_B)))) * 0.85" },
        // Rate of noise decays with x. MOD_A: Initial rate factor. MOD_B: Decay speed factor.


/*239*/ { "POKEY \"Multi-Channel\" (Mixed)", "0.25 * (lfsr_noise(LFSR_4BIT, (63920.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 15.0)) * (1.0 + RAND_OFFSET*0.1)) + lfsr_noise(LFSR_5BIT, (15699.0 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 31.0)) * (1.0 + RAND_OFFSET*0.1)) + (x < (PI * (1.0 + 0.9*MOD_A)) ? 0.9 : -0.9) + lfsr_noise(LFSR_9BIT, (63920.0*0.5 / ( (FREQUENCY > 1.0 ? FREQUENCY : 1.0) * 511.0)) * (1.0 + 1.5*abs(MOD_B)) ) )" },
        // MOD_A: Tone PWM. MOD_B: Rate factor for 9-bit noise.

    // --- Authentic POKEY Noise (using lfsr_noise for simplicity, MOD_A=Vol, MOD_B=Fine Clock Adj) ---


    // --- Bank 15: Digital, Utilities & Extras (240-255) ---
/*240*/ { "Logic: PWM Hash", "(sin(x) > 0.0) ^ (sin(x * (1.0+MOD_A)) > 0.0) ? 1.0 : -1.0" }, // PWM-like effect using XOR logic.

/*241*/ { "Sample & Hold Sine", "sin(floor(x / (PI/8.0)) * (PI/8.0))" }, // Sampled and held sine wave.

/*242*/ { "Digital Saw", "floor(x/PI*8.0)/8.0 - 1.0" }, // Low-resolution sawtooth.

/*243*/ { "Glitch Step", "(x < PI ? 1.0 : -1.0) * floor(x * MOD_A)" }, // Square wave amplitude modulated by a stepper.


/*244*/ { "Weird: Gap", "(x > MOD_A * PI) ? sin(x) : 0.0" }, // Gated sine wave.

/*245*/ { "Noise: White-ish", "rand() * 2.0 - 1.0" }, // White noise generator.

/*246*/ { "Noise: S&H", "lfsr_val(LFSR_16BIT, floor(x/PI*MOD_A), 0.0) * 2.0 - 1.0" }, // Sample & Hold noise.

/*247*/ { "Fibonacci Series", "0.5 * (sin(x) + 0.618*sin(x*1.618) + 0.382*sin(x*2.618 + MOD_A) + 0.236*sin(x*4.236 + MOD_B))" }, // Golden ratio additive.

/*248*/ { "Logistic Chaos", "sin(x * (1.0 + MOD_A * sin(x * (1.0 + MOD_B)))) * sin(x * 2.718)" }, // Chaotic AM/FM texture.

/*249*/ { "Chebyshev 4th", "8.0*pow(x/PI - 1.0, 4.0) - 8.0*pow(x/PI - 1.0, 2.0) + 1.0" }, // Chebyshev T4 polynomial mapped to phase.

/*250*/ { "Tanh Fold", "tanh(sin(x)*5.0 + sin(x*2.0 + MOD_A*PI)*5.0 * MOD_B)" }, // Soft-saturated wavefolder.

    // --- Math & Synthesis Experiments ---

/*251*/ { "Exp FM", "sin(x + exp(sin(x) * (1.0+MOD_A*2.0)) * MOD_B * 5.0)" }, // Exponential Frequency Modulation.


/*252*/ { "Chaotic Map", "sin(x * (1.0 + MOD_A * sin(x*E))) + sin(x*PI*MOD_B)" }, // Recursive sine mapping.


/*253*/ { "Pseudo-LPG", "sin(x + sin(x)*exp(-x*5.0)*MOD_B*10.0) * exp(-x*(1.0+abs(MOD_A)*5.0))" }, // Simulated Low Pass Gate pluck.


/*254*/ { "Harmonic Steps", "floor(sin(x) * (2.0 + abs(MOD_A) * 10.0)) / (2.0 + abs(MOD_A) * 10.0)" }, // Quantized sine wave.

/*255*/ { "Vocal Formant 2", "sin(x) * (1.0 + cos(x * (2.0 + MOD_A * 10.0)))" }, // Windowed sync formant.


    };



/**
 * @brief Initialize the Polysonix wave system.
 *
 * This function must be called once before using any other Polysonix wave functions.
 * It initializes:
 * - Random number generator seeding
 * - LFSR pre-computed tables
 * - Bytecode cache system
 *
 * @return true on successful initialization, false on failure
 */
bool px_vm_init(void) {
    printf("Initializing Polysonix Wave System...\n");

    // 1. Initialize random number generator with current time
    srand((unsigned int)time(NULL));
    printf("  Random number generator seeded with current time.\n");

    // 2. Initialize LFSR tables
    printf("  Initializing LFSR tables...\n");
    px_vm_init_lfsr_tables();

    // Verify LFSR initialization
    bool lfsr_success = true;
    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        if (!precomputed_lfsrs[i].initialized) {
            fprintf(stderr, "  ERROR: LFSR type %d failed to initialize.\n", i);
            lfsr_success = false;
        }
    }

    if (!lfsr_success) {
        fprintf(stderr, "ERROR: LFSR initialization failed.\n");
        return false;
    }

    // 3. Initialize bytecode cache
    printf("  Initializing bytecode cache...\n");
    initialize_bytecode_cache();

    if (!cache_initialized) {
        fprintf(stderr, "ERROR: Bytecode cache initialization failed.\n");
        return false;
    }

    // 4. Initialize default wave bytecode (optional - compile on demand instead)
    printf("  Default waves will be compiled on first use (lazy loading).\n");
    for (int i = 0; i < NUM_DEFAULT_WAVES; i++) {
        default_waves[i].compiled_bytecode = NULL; // Ensure they start as NULL
    }

    printf("Polysonix Wave System initialized successfully.\n");
    printf("  - %d LFSR types available\n", NUM_LFSR_TYPES);
    printf("  - %d default waveforms available\n", NUM_DEFAULT_WAVES);
    printf("  - Bytecode cache ready (size: %d entries)\n", CACHE_TABLE_SIZE);

    return true;
}

/**
 * @brief Deinitialize the Polysonix wave system.
 *
 * This function should be called when shutting down the application.
 * It frees all allocated resources including:
 * - LFSR pre-computed tables
 * - Bytecode cache and all cached compiled expressions
 * - Default wave compiled bytecode
 */
void px_vm_deinit(void) {
    printf("Deinitializing Polysonix Wave System...\n");

    // 1. Free default wave compiled bytecode
    printf("  Freeing default wave bytecode...\n");
    int freed_default_waves = 0;
    for (int i = 0; i < NUM_DEFAULT_WAVES; i++) {
        if (default_waves[i].compiled_bytecode != NULL) {
            free_bytecode_chunk(default_waves[i].compiled_bytecode);
            free(default_waves[i].compiled_bytecode);
            default_waves[i].compiled_bytecode = NULL;
            freed_default_waves++;
        }
    }
    printf("    Freed %d compiled default waves.\n", freed_default_waves);

    // 2. Free bytecode cache
    printf("  Freeing bytecode cache...\n");
    free_bytecode_cache();

    // 3. Free LFSR tables
    printf("  Freeing LFSR tables...\n");
    px_vm_free_lfsr_tables();

    printf("Polysonix Wave System deinitialized successfully.\n");
}

/**
 * @brief Get compiled bytecode for a default wave, compiling if necessary.
 *
 * This function implements lazy loading - waves are compiled on first access
 * and cached for subsequent use.
 *
 * @param wave_index Index of the default wave (0 to NUM_DEFAULT_WAVES-1)
 * @return Pointer to compiled bytecode, or NULL on error
 */
BytecodeChunk* get_default_wave_bytecode(int wave_index) {
    // Validate index
    if (wave_index < 0 || wave_index >= NUM_DEFAULT_WAVES) {
        fprintf(stderr, "Error: Invalid wave index %d (valid range: 0-%d)\n",
                wave_index, NUM_DEFAULT_WAVES - 1);
        return NULL;
    }

    WaveDefinition* wave = &default_waves[wave_index];

    // If already compiled, return existing bytecode
    if (wave->compiled_bytecode != NULL) {
        return wave->compiled_bytecode;
    }

    // Compile the wave expression
    printf("Compiling default wave %d: '%s'\n", wave_index, wave->name);
    wave->compiled_bytecode = compile_expression_to_bytecode(wave->expression);

    if (wave->compiled_bytecode == NULL) {
        fprintf(stderr, "Error: Failed to compile default wave %d ('%s'): %s\n",
                wave_index, wave->name, wave->expression);
        return NULL;
    }

    printf("  Successfully compiled wave %d (%d instructions)\n",
           wave_index, count_bytecode_instructions(wave->compiled_bytecode));

    return wave->compiled_bytecode;
}

/**
 * @brief Get or compile bytecode for any expression with caching.
 *
 * This function first checks the cache for pre-compiled bytecode.
 * If not found, it compiles the expression and caches the result.
 *
 * @param expression Mathematical expression string
 * @return Pointer to compiled bytecode (cached), or NULL on error
 */
BytecodeChunk* get_or_compile_wave_bytecode(const char* expression) {
    if (!expression) {
        fprintf(stderr, "Error: NULL expression provided to get_or_compile_wave_bytecode\n");
        return NULL;
    }

    if (!cache_initialized) {
        fprintf(stderr, "Error: Bytecode cache not initialized. Call px_vm_init() first.\n");
        return NULL;
    }

    // Check cache first
    BytecodeChunk* cached_chunk = lookup_cache(expression);
    if (cached_chunk != NULL) {
        // Cache hit - return existing compiled bytecode
        return cached_chunk;
    }

    // Cache miss - compile and cache the expression
    BytecodeChunk* new_chunk = compile_expression_to_bytecode(expression);
    if (new_chunk == NULL) {
        fprintf(stderr, "Error: Failed to compile expression: %s\n", expression);
        return NULL;
    }

    // Insert into cache (cache takes ownership)
    bool cache_success = insert_cache(expression, new_chunk);
    if (!cache_success) {
        fprintf(stderr, "Warning: Failed to cache compiled expression (continuing anyway): %s\n", expression);
        // Return the chunk anyway, caller is responsible for freeing it
        return new_chunk;
    }

    // Return the cached chunk
    return new_chunk;
}

/**
 * @brief Check if the Polysonix wave system is properly initialized.
 *
 * @return true if system is ready for use, false otherwise
 */
bool px_vm_is_initialized(void) {
    // Check LFSR initialization
    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        if (!precomputed_lfsrs[i].initialized) {
            return false;
        }
    }

    // Check cache initialization
    if (!cache_initialized) {
        return false;
    }

    return true;
}

/**
 * @brief Get system status and statistics.
 *
 * @param stats Pointer to structure to fill with statistics (can be NULL)
 */


void px_vm_get_stats(PxVmStats* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(PxVmStats));

    // Check LFSR initialization
    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        if (precomputed_lfsrs[i].initialized) {
            stats->lfsr_types_initialized++;
        }
    }

    // Cache stats
    stats->cache_initialized = cache_initialized;
    if (cache_initialized) {
        stats->cache_entry_count = bytecode_cache.count;
    }

    // Default waves compiled
    for (int i = 0; i < NUM_DEFAULT_WAVES; i++) {
        if (default_waves[i].compiled_bytecode != NULL) {
            stats->default_waves_compiled++;
        }
    }

    // System status
    stats->system_initialized = px_vm_is_initialized();

    // Memory usage estimate (rough)
    stats->total_memory_usage_estimate = 0;

    // LFSR tables memory
    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        if (precomputed_lfsrs[i].initialized && precomputed_lfsrs[i].bit_table && precomputed_lfsrs[i].period > 0) { // Added period > 0 check
            stats->total_memory_usage_estimate += LFSR_TABLE_BYTES(precomputed_lfsrs[i].period); // Corrected: use .period
        }
    }

    // Cache memory (rough estimate: assume average 100 bytes per cached expression string + chunk overhead)
    // This is very rough. BytecodeChunk itself is large.
    stats->total_memory_usage_estimate += stats->cache_entry_count * (sizeof(CacheEntry) + 100 + sizeof(BytecodeChunk) + MAX_BYTECODE_SIZE/4); // String, data, struct

    // Default waves memory (rough estimate)
    stats->total_memory_usage_estimate += stats->default_waves_compiled * (sizeof(BytecodeChunk) + MAX_BYTECODE_SIZE/4); // Struct + average bytecode
}

/**
 * @brief Print system status and statistics to stdout.
 */
void px_vm_print_stats(void) {
    PxVmStats stats;
    px_vm_get_stats(&stats);

    printf("=== Polysonix Wave System Status ===\n");
    printf("System Initialized: %s\n", stats.system_initialized ? "YES" : "NO");
    printf("LFSR Types Ready: %d/%d\n", stats.lfsr_types_initialized, NUM_LFSR_TYPES);
    printf("Cache Initialized: %s\n", stats.cache_initialized ? "YES" : "NO");
    printf("Cache Entries: %zu\n", stats.cache_entry_count);
    printf("Default Waves Compiled: %d/%d\n", stats.default_waves_compiled, NUM_DEFAULT_WAVES);
    printf("Estimated Memory Usage: %zu bytes (%.1f KB)\n", stats.total_memory_usage_estimate, stats.total_memory_usage_estimate / 1024.0f);
    printf("=====================================\n");
}

/* Usage Example:
int main() {
    // Initialize the system
    if (!px_vm_init()) {
        fprintf(stderr, "Failed to initialize Polysonix wave system!\n");
        return 1;
    }

    // Use the system...
    BytecodeChunk* wave0 = get_default_wave_bytecode(0);
    BytecodeChunk* custom = get_or_compile_wave_bytecode("sin(x) * 0.5");

    // Print stats
    px_vm_print_stats();

    // Cleanup when done
    px_vm_deinit();

    return 0;
}
*/

#endif // PX_WAVE_ROM_IMP_INCLUDED
#endif // PX_WAVE_ROM_IMPLEMENTATION
