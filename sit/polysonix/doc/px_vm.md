# Polysonix Waveform Scripting Language Guide
   (c) 2025 Jacques Morel
   This software is licensed under the MIT License.

## 1. Introduction

The Polysonix Waveform Scripting Language is a small, efficient, C-like expression language designed for generating audio waveforms in real-time. It provides a flexible way to define complex mathematical formulas that go far beyond standard sine, square, and sawtooth waves.

This guide provides a comprehensive overview of the language, from its basic syntax to advanced modulation techniques and integration with the C environment.

<details>
<summary>Table of Contents</summary>

- [1. Introduction](#1-introduction)
- [2. Quick Start](#2-quick-start)
- [3. Language Reference](#3-language-reference)
  - [3.1. Structure and Syntax](#31-structure-and-syntax)
  - [3.2. Operators](#32-operators)
    - [3.2.1. Arithmetic Operators](#321-arithmetic-operators)
    - [3.2.2. Logical and Comparison Operators](#322-logical-and-comparison-operators)
    - [3.2.3. Unary Operators](#323-unary-operators)
    - [3.2.4. Ternary Operator](#324-ternary-operator)
    - [3.2.5. Operator Precedence](#325-operator-precedence)
  - [3.3. Variables](#33-variables)
  - [3.4. Constants](#34-constants)
  - [3.5. Functions](#35-functions)
    - [3.5.1. Trigonometric Functions](#351-trigonometric-functions)
    - [3.5.2. Mathematical Functions](#352-mathematical-functions)
    - [3.5.3. LFSR (Linear-Feedback Shift Register) Functions](#353-lfsr-linear-feedback-shift-register-functions)
    - [3.5.4. Summation Function (sigma)](#354-summation-function-sigma)
- [4. The Compilation and Execution Model](#4-the-compilation-and-execution-model)
  - [4.1. Overview](#41-overview)
  - [4.2. Tokenizer](#42-tokenizer)
  - [4.3. Parser (AST)](#43-parser-ast)
  - [4.4. Compiler (Bytecode)](#44-compiler-bytecode)
  - [4.5. Virtual Machine (VM)](#45-virtual-machine-vm)
  - [4.6. The VmParams Struct](#46-the-vmparams-struct)
- [5. Practical Examples and Techniques](#5-practical-examples-and-techniques)
  - [5.1. Basic Waveforms](#51-basic-waveforms)
  - [5.2. Modulation Techniques](#52-modulation-techniques)
  - [5.3. Rhythmic and Noise-Based Patterns](#53-rhythmic-and-noise-based-patterns)
- [6. C Integration Guide](#6-c-integration-guide)
  - [6.1. Defining a Waveform Script](#61-defining-a-waveform-script)
  - [6.2. Compiling and Using the Waveform](#62-compiling-and-using-the-waveform)
  - [6.3. Real-time Modulation](#63-real-time-modulation)

</details>

## 2. Quick Start

A waveform script is a simple string containing a mathematical expression. The result of the expression should be a value between -1.0 and 1.0, which represents the amplitude of the wave at a given point in time.

The most important variable is `x`, which represents the phase of the wave, progressing from `0` to `2*PI` over one cycle.

**Example: A simple sine wave**
```c
"sin(x)"
```

**Example: A sawtooth wave**
```c
"(x / PI) - 1.0"
```

**Example: A pulse wave with 25% duty cycle**
```c
"x < PI/2 ? 1.0 : -1.0"
```

## 3. Language Reference

### 3.1. Structure and Syntax

The language uses a familiar C-style syntax for mathematical expressions. It is composed of:
- **Literals**: Floating-point numbers (e.g., `1.0`, `0.5`, `-2.7`).
- **Variables**: `x`, `FREQUENCY`, `MOD_A`, etc.
- **Constants**: `PI`, `E`, `LFSR_8BIT`, etc.
- **Functions**: `sin()`, `lfsr_noise()`, `sigma()`.
- **Operators**: `+`, `-`, `*`, `/`, `&&`, `? :`.
- **Grouping**: Parentheses `()` are used to control the order of operations and for function arguments.
- **Commas**: Used to separate arguments in function calls.

### 3.2. Operators

#### 3.2.1. Arithmetic Operators
- `+`: Addition
- `-`: Subtraction
- `*`: Multiplication
- `/`: Division
- `%`: Modulo (remainder)

#### 3.2.2. Logical and Comparison Operators
- `==`: Equal to
- `!=`: Not equal to
- `<`: Less than
- `>`: Greater than
- `<=`: Less than or equal to
- `>=`: Greater than or equal to
- `&&`: Logical AND
- `||`: Logical OR
- `^`: Logical XOR

#### 3.2.3. Unary Operators
- `-`: Negation (e.g., `-x`)
- `!`: Logical NOT

#### 3.2.4. Ternary Operator
- `? :`: The conditional operator, used as `condition ? value_if_true : value_if_false`.

#### 3.2.5. Operator Precedence
The language follows standard C operator precedence, from highest to lowest:
1. `()` (Grouping, Function Calls)
2. `!`, `-` (Unary)
3. `*`, `/`, `%`
4. `+`, `-`
5. `<`, `>`, `<=`, `>=`
6. `==`, `!=`
7. `&&`
8. `||`
9. `^`
10. `? :`

### 3.3. Variables

The following variables are available within a script:
- `x`: The current phase of the wave, from `0` to `2*PI`.
- `FREQUENCY`: The frequency of the current note in Hz.
- `MOD_A`, `MOD_B`, `MOD_C`: Modulation inputs, typically ranging from -1.0 to 1.0.
- `RAND_OFFSET`: A random value between 0.0 and 1.0 that is constant for the duration of a single note.
- `k`: (or other names) The loop variable used inside a `sigma()` function.

### 3.4. Constants

- **Mathematical Constants**
  - `PI`: 3.14159...
  - `TWO_PI`: 2 * PI
  - `PI_OVER_2`: PI / 2
  - `THREE_PI_OVER_2`: 3 * PI / 2
  - `E`: 2.71828...

- **LFSR Type Constants**
  These are integer values passed to LFSR functions to select a specific type of pseudo-random sequence.
  - `LFSR_4BIT`: 0 (Period: 15)
  - `LFSR_5BIT`: 1 (Period: 31)
  - `LFSR_6BIT`: 2 (Period: 63)
  - `LFSR_7BIT`: 3 (Period: 127)
  - `LFSR_8BIT`: 4 (Period: 255)
  - `LFSR_9BIT`: 5 (Period: 511)
  - `LFSR_10BIT`: 6 (Period: 1023)
  - `LFSR_11BIT`: 7 (Period: 2047)
  - `LFSR_12BIT`: 8 (Period: 4095)
  - `LFSR_13BIT`: 9 (Period: 8191)
  - `LFSR_14BIT`: 10 (Period: 16383)
  - `LFSR_15BIT`: 11 (Period: 32767)
  - `LFSR_16BIT`: 12 (Period: 65535)
  - `LFSR_17BIT`: 13 (Period: 131071)
  - `LFSR_GALOIS`: 14 (Alternative feedback topology, same period as 16-bit)
  - `LFSR_FIBONACCI`: 15 (Standard feedback topology, same period as 16-bit)

### 3.5. Functions

#### 3.5.1. Trigonometric Functions

These functions operate on radians.

- **`sin(value)`**
  Computes the sine of `value`.
  - `value`: The angle in radians.
  - **Example**: `sin(x)` produces a standard sine wave.

- **`cos(value)`**
  Computes the cosine of `value`.
  - `value`: The angle in radians.
  - **Example**: `cos(x)` produces a cosine wave, which is phase-shifted 90 degrees from a sine wave.

- **`tan(value)`**
  Computes the tangent of `value`.
  - `value`: The angle in radians.
  - **Example**: `tan(x)` can be used to create sharp, repeating curves, but be aware that it approaches infinity at odd multiples of `PI/2`.

- **`asin(value)`**
  Computes the arc sine of `value`.
  - `value`: A number between -1.0 and 1.0. Input outside this range will be clamped.
  - **Returns**: The angle in radians, from `-PI/2` to `PI/2`.
  - **Example**: `asin(sin(x))` will produce a triangle wave.

- **`acos(value)`**
  Computes the arc cosine of `value`.
  - `value`: A number between -1.0 and 1.0. Input outside this range will be clamped.
  - **Returns**: The angle in radians, from `0` to `PI`.
  - **Example**: Can be used for creating interesting non-linear curves.

- **`atan(value)`**
  Computes the arc tangent of `value`.
  - `value`: Any floating-point number.
  - **Returns**: The angle in radians, from `-PI/2` to `PI/2`.

#### 3.5.2. Mathematical Functions

- **`abs(value)`**
  Returns the absolute value of `value`.
  - **Example**: `abs(sin(x))` creates a rectified sine wave, with all values being positive.

- **`tanh(value)`**
  Computes the hyperbolic tangent of `value`. This is a useful function for soft-clipping or distortion effects, as it smoothly squashes any input value into the range `[-1.0, 1.0]`.
  - **Example**: `tanh(sin(x) * 5.0)` will create a sine wave with soft-clipping distortion.

- **`exp(value)`**
  Computes *e* raised to the power of `value`.
  - **Example**: `exp(x)` can create exponentially rising curves.

- **`log(value)`**
  Computes the natural logarithm of `value`.
  - `value`: Must be greater than 0.
  - **Example**: Useful for logarithmic shaping of envelopes or other parameters.

- **`log10(value)`**
  Computes the base-10 logarithm of `value`.
  - `value`: Must be greater than 0.

- **`floor(value)`**
  Rounds `value` down to the nearest integer.
  - **Example**: `floor(x / PI)` will produce a step-like signal.

- **`ceil(value)`**
  Rounds `value` up to the nearest integer.

- **`min(a, b)`**
  Returns the smaller of the two values `a` and `b`.
  - **Example**: `min(sin(x), 0.5)` clips the top of a sine wave at 0.5.

- **`max(a, b)`**
  Returns the larger of the two values `a` and `b`.
  - **Example**: `max(sin(x), 0.0)` removes the negative portion of a sine wave.

- **`sqrt(value)`**
  Computes the square root of `value`.
  - `value`: Must be non-negative.

- **`pow(base, exponent)`**
  Computes `base` raised to the power of `exponent`.
  - **Example**: `pow(x / TWO_PI, 2.0)` creates a parabolic curve, useful for shaping envelopes.

- **`rand()`**
  Returns a new pseudo-random floating-point value between 0.0 and 1.0 every time it is executed. Unlike `RAND_OFFSET`, this function is not constant for the duration of a note.
  - **Example**: `(rand() - 0.5) * 0.1` adds a small amount of random noise to the signal.

#### 3.5.3. LFSR (Linear-Feedback Shift Register) Functions

LFSRs are powerful tools for generating pseudo-random sequences, useful for creating noise, random triggers, or complex, evolving textures. The LFSR functions can operate in two distinct modes, determined by the C environment:

1.  **Precomputed Table Mode (Default)**: In this mode, the functions read from a pre-calculated table of LFSR bit sequences. This is deterministic and efficient for creating textures that are consistent with every note trigger.
2.  **Free-Running Mode**: This mode uses a stateful LFSR whose value is maintained by the C host and updated with every function call. This is ideal for creating noise that evolves continuously over time, rather than restarting with each note.

- **`lfsr_val(type, position, seed)`**
  Returns the raw LFSR bit value (0.0 or 1.0).
  - `type`: An LFSR type constant (e.g., `LFSR_8BIT`).
  - `position`: (Precomputed Mode only) A normalized value from 0.0 to 1.0 indicating the position in the sequence to read from.
  - `seed`: (Precomputed Mode only) A normalized value from 0.0 to 1.0 that provides an offset to the `position`.
  - **Example**: `lfsr_val(LFSR_8BIT, x / TWO_PI, RAND_OFFSET)` reads from an 8-bit LFSR sequence, using the wave's phase as the primary position and the note's random offset to vary the starting point.

- **`lfsr_noise(type, rate)`**
  Returns bipolar LFSR noise (-1.0 or 1.0). This is the most common function for audio noise generation.
  - `type`: An LFSR type constant.
  - `rate`: A multiplier for how quickly the sequence is scanned relative to the wave's phase `x`.
  - **Example**: `lfsr_noise(LFSR_12BIT, 1.0 + MOD_A * 20.0)` generates noise from a 12-bit LFSR. The speed at which the noise changes can be controlled by `MOD_A`.

- **`lfsr_clock(type, density)`**
  Generates rhythmic clock pulses (1.0 for "on", 0.0 for "off").
  - `type`: An LFSR type constant.
  - `density`: A threshold from 0.0 to 1.0. A pulse is generated if the LFSR value is greater than or equal to the density.
  - **Example**: `sin(x) * lfsr_clock(LFSR_7BIT, 0.75)` creates a gated sine wave that plays in a pseudo-random rhythmic pattern.

#### 3.5.4. Summation Function (sigma)

The `sigma` function provides a powerful way to perform summations, which is fundamental to additive synthesis and creating complex harmonic structures.

- **`sigma(loop_var, start, end, step, expression)`**
  - `loop_var`: The name of the loop variable (e.g., `k`). This must be a valid, unquoted identifier.
  - `start`: The initial value of the loop variable.
  - `end`: The final value of the loop variable. The loop continues as long as `loop_var <= end`.
  - `step`: The amount to add to the loop variable in each iteration.
  - `expression`: The expression to be evaluated and summed in each iteration. This expression can, and usually should, involve the `loop_var`.

  **Basic Example: Building a Sawtooth Wave**
  This example uses `sigma` to sum sine waves, which is the principle of additive synthesis. This specific formula approximates a sawtooth wave.
  ```c
  "sigma(k, 1.0, 10.0, 1.0, sin(k * x) / k)"
  ```

  **Advanced Example: Modulated Harmonic Series**
  This example creates a more complex sound where a modulation parameter (`MOD_A`) controls which harmonics are present. Odd harmonics are always present, while even harmonics fade in as `MOD_A` increases.
  ```c
  "sigma(k, 1.0, 8.0, 1.0, (k % 2 == 1 ? 1.0 : MOD_A) * sin(k * x) / k)"
  ```
  - `k % 2 == 1 ? 1.0 : MOD_A`: This is the core of the modulation.
    - If `k` is odd (`k % 2 == 1`), the amplitude multiplier is `1.0`.
    - If `k` is even, the amplitude multiplier is `MOD_A`. When `MOD_A` is 0, even harmonics are silent. As `MOD_A` approaches 1.0, they fade in.

## 4. The Compilation and Execution Model

### 4.1. Overview

To achieve real-time performance, scripts are not interpreted directly. Instead, they go through a four-stage process:
1.  **Tokenization**: The script string is broken into a stream of tokens.
2.  **Parsing**: The tokens are organized into an Abstract Syntax Tree (AST).
3.  **Compilation**: The AST is converted into low-level bytecode.
4.  **Execution**: A Virtual Machine (VM) runs the bytecode.

### 4.2. Tokenizer

The tokenizer is the first stage of compilation. It scans the raw script string and breaks it down into a sequence of "tokens." Each token is a small, meaningful unit of the language, such as a number (`1.23`), a variable (`x`), a function name (`sin`), or an operator (`+`). This process simplifies the next stage by allowing the parser to work with a structured list of tokens rather than raw text.

For example, the expression `sin(x) * 0.5` is tokenized into:
`sin` (Function) -> `(` (Left Paren) -> `x` (Variable) -> `)` (Right Paren) -> `*` (Operator) -> `0.5` (Number)

### 4.3. Parser (AST)

The parser takes the flat list of tokens and builds an Abstract Syntax Tree (AST). The AST is a tree-like data structure that represents the grammatical structure and logical hierarchy of the expression, correctly handling operator precedence. For instance, in the expression `2 + 3 * 4`, the parser ensures that the `*` operation is a child of the `+` operation, so multiplication is performed first.

This hierarchical representation is crucial for the compiler to generate correct and efficient code.

### 4.4. Compiler (Bytecode)

The compiler walks through the AST and translates it into bytecode. Bytecode is a low-level, platform-independent set of simple instructions. These instructions are designed to be executed extremely quickly by a small, efficient Virtual Machine.

For example, the expression `x + 1.0` might be compiled into bytecode that means:
1. `PUSH_VAR_X` (Push the value of `x` onto the stack)
2. `PUSH_CONST 1.0` (Push the constant `1.0` onto the stack)
3. `ADD` (Pop the top two values, add them, and push the result back)

This bytecode, along with a table of constants, is stored in a `BytecodeChunk` struct, ready for execution.

### 4.5. Virtual Machine (VM)

The Virtual Machine (VM) is a stack-based engine that executes the bytecode. It reads the instructions one by one and manipulates a small stack of numbers. Because the VM only needs to perform very simple operations (like pushing, popping, and adding), it can run with extremely high performance, making it suitable for generating audio samples in real-time without causing audio dropouts.

### 4.6. The VmParams Struct

The `VmParams` struct is the critical link between the C environment of the synthesizer and the isolated world of the waveform script. When the synthesizer's C code needs to execute a waveform script to generate a sample, it populates an instance of this struct with all the necessary real-time values.

```c
typedef struct {
    float x;            // Current wave phase (0 to 2*PI)
    float frequency;    // Note frequency in Hz
    float rand_offset;  // Note-specific random value
    float modA;         // Modulation parameter A
    float modB;         // Modulation parameter B
    float modC;         // Modulation parameter C
    // ... fields for free-running LFSR state ...
} VmParams;
```

When the VM executes instructions like `OP_PUSH_VAR_X` or `OP_PUSH_VAR_MOD_A`, it looks up the corresponding value from the `VmParams` struct that was provided for that specific execution run. This mechanism allows the same compiled script to produce different results based on the note being played and the current state of the synthesizer's modulation sources.

## 5. Practical Examples and Techniques

### 5.1. Basic Waveforms

- **Sawtooth**
  ```c
  "((x / PI) - 1.0)"
  ```

- **Square**
  ```c
  "x < PI ? 1.0 : -1.0"
  ```

- **Pulse Wave (Modulated Width)**
  ```c
  "x < (PI * (0.5 + MOD_A * 0.5)) ? 1.0 : -1.0"
  ```

- **Triangle**
  ```c
  "(2.0 * abs((x / PI) - 1.0) - 1.0)"
  ```
  *Alternate Triangle using `asin`*:
  ```c
  "asin(sin(x)) * (2.0 / PI)"
  ```

### 5.2. Modulation Techniques

- **Frequency Modulation (FM)**
  A simple FM sound can be created by modulating the phase of one sine wave with another.
  ```c
  "sin(x + sin(x * 8.0) * MOD_A * 5.0)"
  ```
  Here, `sin(x * 8.0)` is the modulator, and `MOD_A` controls the FM depth.

- **Pulse Width Modulation (PWM)**
  This is achieved by modulating the threshold of the ternary operator in a pulse wave.
  ```c
  "x < (PI * (0.5 + sin(x * MOD_A * 5.0) * 0.4)) ? 1.0 : -1.0"
  ```
  Here, an LFO-like `sin()` function controls the pulse width, with `MOD_A` controlling the LFO speed.

- **Additive Synthesis**
  Creating a rich tone by summing harmonically related sine waves.
  ```c
  "0.6*sin(x) + 0.3*sin(x*2) + 0.1*sin(x*3)"
  ```

### 5.3. Rhythmic and Noise-Based Patterns

- **Gated Noise Pad**
  A sine wave's amplitude is controlled by a rhythmic `lfsr_clock`.
  ```c
  "sin(x) * lfsr_clock(LFSR_10BIT, 0.8 + MOD_A * 0.19)"
  ```
  `MOD_A` can be used to subtly alter the density of the rhythm.

- **Digital Noise**
  Direct output of an LFSR, with rate controlled by `MOD_A`.
  ```c
  "lfsr_noise(LFSR_8BIT, 20.0 + MOD_A * 200.0)"
  ```

- **8-bit Style Arpeggio**
  Uses `floor` to create discrete pitch steps based on the phase.
  ```c
  "sin(x * floor( (x / (PI/4)) % 4) * 2.0)"
  ```

## 6. C Integration Guide

### 6.1. Defining a Waveform Script

Waveforms are defined in C as simple string literals within a `WaveDefinition` struct. This is typically done in a global array.

```c
#include "px_vm.h"

// An array to hold all the synth's waveform definitions.
WaveDefinition default_waves[] = {
    { "Sine", "sin(x)" },
    { "Sawtooth", "((x / PI) - 1.0)" },
    { "FM Bass", "sin(x + sin(x * 8.0) * MOD_A * 5.0)" },
    // ... more waves
};

// It's useful to have an enum to map indices to names.
typedef enum {
    WAVE_SINE,
    WAVE_SAWTOOTH,
    WAVE_FM_BASS,
    // ...
    NUM_WAVES
} WaveIndex;
```

### 6.2. Compiling and Using the Waveform

Before a script can be used, it must be compiled into bytecode. This is typically done once at application startup. The `compile_expression_to_bytecode` function handles this.

```c
// At startup...
for (int i = 0; i < NUM_WAVES; ++i) {
    // This field will store the pointer to the compiled bytecode.
    default_waves[i].compiled_bytecode = compile_expression_to_bytecode(default_waves[i].expression);

    if (default_waves[i].compiled_bytecode == NULL) {
        fprintf(stderr, "Failed to compile wave: %s\n", default_waves[i].name);
    }
}

// When triggering a note in the synthesizer...
PX_NoteOn(synth, midi_note, WAVE_FM_BASS, key_id);
```

### 6.3. Real-time Modulation

The `MOD_A`, `MOD_B`, and `MOD_C` parameters are the bridge for real-time control. The main synthesizer engine can map LFOs, envelopes, or UI controls to these parameters. When the VM executes a script, it uses the `VmParams` struct, which is populated with the current values of these modulation sources.

```c
// Inside the synthesizer's audio processing loop (simplified)...

// 1. Update LFOs and other modulation sources.
float lfo_value = update_lfo(lfo0);

// 2. Populate the VmParams for the current voice.
VmParams params;
params.x = current_phase;
params.frequency = note_frequency;
params.modA = lfo_value; // Map LFO output to MOD_A
params.modB = envelope_value; // Map envelope output to MOD_B
// ...

// 3. Execute the bytecode with these parameters.
float sample = execute_bytecode(voice->wave->compiled_bytecode, &params);
```
This demonstrates how the same compiled script for `"sin(x + MOD_A)"` can produce a vibrato effect, as the value of `MOD_A` (and therefore the result of the expression) changes on every audio block.
