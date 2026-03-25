# Polysonix Waveform Scripting Language Guide
   (c) 2025-2026 Jacques Morel
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
    - [3.5.1. State and Logic Functions](#351-state-and-logic-functions)
    - [3.5.2. Trigonometric Functions](#352-trigonometric-functions)
    - [3.5.3. Mathematical Functions](#353-mathematical-functions)
    - [3.5.4. Probability Function (prob)](#354-probability-function-prob)
    - [3.5.5. Dynamic Selection Function (select)](#355-dynamic-selection-function-select)
    - [3.5.6. Smooth Interpolated Selection (smooth_select)](#356-smooth-interpolated-selection-smooth_select)
    - [3.5.7. LFSR (Linear-Feedback Shift Register) Functions](#357-lfsr-linear-feedback-shift-register-functions)
    - [3.5.8. Summation Function (sigma)](#358-summation-function-sigma)
- [4. The Compilation and Execution Model](#4-the-compilation-and-execution-model)
  - [4.1. Overview](#41-overview)
  - [4.2. Tokenizer](#42-tokenizer)
  - [4.3. Parser (AST)](#43-parser-ast)
  - [4.4. Compiler (Bytecode)](#44-compiler-bytecode)
    - [4.4.1. The BytecodeChunk Structure](#441-the-bytecodechunk-structure)
    - [4.4.2. Flat Opcode Architecture](#442-flat-opcode-architecture)
  - [4.5. Virtual Machine (VM)](#45-virtual-machine-vm)
    - [4.5.1. Sub-Chunk Execution](#451-sub-chunk-execution)
    - [4.5.2. Security Features](#452-security-features)
    - [4.5.3. GPU Compute Shader Path](#453-gpu-compute-shader-path)
    - [4.5.4. Native Transpiler (Benchmark Mode)](#454-native-transpiler-benchmark-mode)
  - [4.6. The VmParams Struct](#46-the-vmparams-struct)
  - [4.7. Constraints and Limits](#47-constraints-and-limits)
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
  - `INV_PI`: 1 / PI
  - `INV_TWO_PI`: 1 / (2 * PI)
  - `INV_PI_OVER_2`: 1 / (PI / 2)
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

#### 3.5.1. State and Logic Functions

- **`markov(id, trigger, p00, p01, ..., pNN)`**
  Creates a state machine that randomly transitions between different states based on a set of probabilities. This is incredibly powerful for generative sequencing, random but structured rhythms, or evolving timbres.

  *How it works:* Imagine you are in State 0. You roll a set of dice to decide if you stay in State 0, move to State 1, or move to State 2. The probabilities for this decision are defined in the matrix.

  - `id`: The memory slot index (0 to 3). You can have up to 4 independent Markov chains running per voice.
  - `trigger`: A condition evaluated every sample. The dice are *only rolled* when this condition goes from `false` to `true` (a "rising edge"). For example, `x < 0.05` means the transition only happens exactly once at the very start of every wave cycle. This prevents the state from rapidly changing 48,000 times a second and turning into white noise.
  - `p00...pNN`: The transition probabilities, written as a flattened grid.
    - If you have 2 states (0 and 1), you need 4 numbers (a 2x2 grid).
      - `p00`: Probability to stay in State 0 if you are currently in State 0.
      - `p01`: Probability to jump to State 1 if you are currently in State 0.
      - `p10`: Probability to jump to State 0 if you are currently in State 1.
      - `p11`: Probability to stay in State 1 if you are currently in State 1.
    - If you have 3 states (0, 1, and 2), you need 9 numbers (a 3x3 grid), and so on up to 8x8.
    - *Note:* The compiler automatically figures out how many states you have based on how many numbers you provide. The rows do not strictly need to equal exactly 1.0; the VM will auto-normalize the probabilities gracefully.
  - **Returns**: The current integer state (0.0, 1.0, 2.0, etc.). You can use this state with the `select` function to choose different frequencies, waves, or modulation amounts.

  **Musical Example (Generative Rhythm):**
  Let's create a 2-state system where State 0 is "Quiet" and State 1 is "Loud".
  - If we are Quiet (State 0), we have a 90% chance to stay Quiet, and a 10% chance to get Loud.
  - If we are Loud (State 1), we have a 60% chance to get Quiet again, and a 40% chance to stay Loud.

  We would write this as: `markov(0, lfsr_clock(LFSR_8BIT, 0.5), 0.9, 0.1, 0.6, 0.4)`
  We can then wrap this in a `select` function to output actual volume levels based on the state:
  `select(markov(0, lfsr_clock(LFSR_8BIT, 0.5), 0.9, 0.1, 0.6, 0.4), 0.2, 1.0) * sin(x)`
  *(If state is 0, volume is 0.2. If state is 1, volume is 1.0).*

#### 3.5.2. Trigonometric Functions

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

#### 3.5.3. Mathematical Functions

- **`fma(a, b, c)`**
  Computes `(a * b) + c` as a single operation (Fused Multiply-Add). This is often faster and more precise than performing multiplication and addition separately.
  - **Example**: `fma(sin(x), 0.5, 0.5)` scales a sine wave by half and shifts it up by half, resulting in a wave from 0.0 to 1.0.

- **`fms(a, b, c)`**
  Computes `(a * b) - c` as a single operation (Fused Multiply-Subtract). This is often faster and more precise than performing multiplication and subtraction separately.
  - **Example**: `fms(sin(x), 0.5, 0.5)` scales a sine wave by half and shifts it down by half, resulting in a wave from -1.0 to 0.0.

- **`fnmadd(a, b, c)`**
  Computes `-(a * b) + c` as a single operation (Fused Negative Multiply-Add).
  - **Example**: `fnmadd(x, 2.0, 1.0)` evaluates to `-2.0*x + 1.0`.

- **`fnmsub(a, b, c)`**
  Computes `-(a * b) - c` as a single operation (Fused Negative Multiply-Subtract).
  - **Example**: `fnmsub(x, 2.0, 1.0)` evaluates to `-2.0*x - 1.0`.


- **`abs(value)`**
  Returns the absolute value of `value`.
  - **Example**: `abs(sin(x))` creates a rectified sine wave, with all values being positive.

- **`tanh(value)`**
  Computes the hyperbolic tangent of `value`. This is a useful function for soft-clipping or distortion effects, as it smoothly squashes any input value into the range `[-1.0, 1.0]`.
  - **Example**: `tanh(sin(x) * 5.0)` will create a sine wave with soft-clipping distortion.

- **`exp(value)`**
  Computes *e* raised to the power of `value`.
  - **Example**: `exp(x)` can create exponentially rising curves.

- **`exp2(value)`**
  Computes 2 raised to the power of `value` (very fast).
  - **Example**: `exp2(pitch)` can be used for octave/pitch calculations or exponential envelopes.

- **`expm1(value)`**
  Computes *e* raised to the power of `value`, minus 1. This is accurate near 0.
  - **Example**: `expm1(-time * decay)` is useful for envelopes and small exponential curves.

- **`log(value)`**
  Computes the natural logarithm of `value`.
  - `value`: Must be greater than 0.
  - **Example**: Useful for logarithmic shaping of envelopes or other parameters.

- **`log2(value)`**
  Computes the base-2 logarithm of `value`.
  - `value`: Must be greater than 0.
  - **Example**: `log2(freq / 440.0) * 12.0` can be used for Frequency to MIDI note conversion and log scaling.

- **`log1p(value)`**
  Computes the natural logarithm of `1 + value`. This is accurate near 0.
  - `value`: Must be greater than -1.0.
  - **Example**: `20.0 * log1p(gain)` is useful for logarithmic controls and dB calculations.

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

- **`hypot(x, y)`**
  Computes the square root of the sum of the squares of `x` and `y` (`sqrt(x*x + y*y)`), without undue overflow or underflow at intermediate stages of the computation.
  - **Example**: `hypot(left, right)` calculates the vector length for stereo panning.

- **`copysign(mag, sgn)`**
  Returns a value with the magnitude of `mag` and the sign of `sgn`.
  - **Example**: `copysign(1.0, velocity)` can be used to set phase direction or for bipolar signals.

- **`scalbn(value, exp)`**
  Multiplies a floating-point number `value` by `2^exp`. This is an extremely fast operation, useful for pitch shifting or octave jumps.
  - **Example**: `scalbn(freq, octave)`.

- **`remquo(value, divisor)`**
  Computes the floating-point remainder of `value / divisor`. Useful as a fast modulo operation.
  - **Example**: `remquo(phase, PI)` for phase wrapping.

- **`nextafter(x, y)`**
  Returns the next representable floating-point value of `x` in the direction of `y`.
  - **Example**: `nextafter(signal, 1.0)` is useful for smooth ramps and anti-aliasing.

- **`fdim(x, y)`**
  Returns the positive difference between `x` and `y` (i.e., `max(0, x - y)`), without branching.
  - **Example**: `fdim(signal, threshold)`.

- **`nan()`**
  Creates a NaN (Not-a-Number) value.
  - **Example**: `nan()` safely signals invalid or missing states.

- **`inf()`**
  Creates an Infinity value.
  - **Example**: `inf()` safely signals extreme values or boundaries.

- **`lgamma(value)`**
  Computes the natural logarithm of the absolute value of the gamma function of `value`.
  - **Example**: `lgamma(x)` can be used in advanced synthesis algorithms.

- **`tgamma(value)`**
  Computes the gamma function of `value`.
  - **Example**: `tgamma(x)` can be used for complex nonlinearities.

- **`rand()`**
  Returns a new pseudo-random floating-point value between 0.0 and 1.0 every time it is executed. Unlike `RAND_OFFSET`, this function is not constant for the duration of a note.
  - **Example**: `(rand() - 0.5) * 0.1` adds a small amount of random noise to the signal.

**Taming Functions (Branchless)**
These functions are compiled down to hardware-accelerated C math primitives and GLSL equivalents, making them ideal for high-performance modulation and logical flow:

- **`clamp(value, min, max)`**: Clamps the `value` strictly within the `[min, max]` range.
  - **Example**: `clamp(prob(0.5, MOD_A, 0.0), 0.2, 0.8)` creates a bounded, probabilistic modulation.
- **`mix(param, v1, v2)`**: Linearly interpolates between two expressions, `v1` and `v2`, driven by a mixing `param` (safely clamped between `0.0` and `1.0`).
  - **Example**: `mix(MOD_A, sin(x), saw(x))` crossfades between a sine wave and a sawtooth wave.
- **`ramp(start, end, time)`**: Linearly interpolates from `start` to `end` driven by a `time` or envelope progress value (clamped between `0.0` and `1.0`).
  - **Example**: `ramp(0.0, 1.0, MOD_B) * sin(x)` applies an amplitude swell to a sine wave.
- **`step(edge, x)`**: Returns `0.0` if `x < edge`, otherwise returns `1.0`. Useful for branchless conditionals.
  - **Example**: `mix(sin(x), saw(x), step(PI, x))` switches to a saw wave exactly halfway through the phase cycle.
- **`sign(x)`**: Returns `-1.0` if `x < 0`, `0.0` if `x == 0`, and `1.0` if `x > 0`. Useful for logical negation and bipolar conversions.
  - **Example**: `sign(sin(x))` generates a simple, un-aliased square wave.
- **`inversesqrt(x)`**: Returns the fast inverse square root of `x` (`1.0 / sqrt(x)`). Excellent for normalization or distance calculations.
  - **Example**: `(a * a + b * b) * inversesqrt(a * a + b * b)` calculates the hypotenuse efficiently.

#### 3.5.4. Probability Function (prob)

- **`prob(chance, true_expr, false_expr)`**
  Evaluates to `true_expr` with the probability given by `chance` (a value between 0.0 and 1.0), otherwise evaluates to `false_expr`. The probability is calculated against the per-wave `RAND_OFFSET` to ensure deterministic execution within a single cycle, preventing unwanted audio artifacts that true per-sample randomness might introduce inside continuous waveforms.
  - `chance`: The probability threshold (0.0 to 1.0).
  - `true_expr`: The expression to evaluate if the random check passes.
  - `false_expr`: The expression to evaluate if the random check fails.
  - **Example**: `prob(0.5, sin(x), cos(x))` has a 50% chance of behaving as a sine wave or a cosine wave for the duration of the current cycle.


#### 3.5.5. Dynamic Selection Function (select)

- **`select(param, v1, v2, ..., vn)`**
  Dynamically selects a value from a variable-length list of expressions (between 2 and 16 items) based on the input `param`.
  The `param` value is clamped to the `[0.0, 1.0]` range and scaled to pick the corresponding index in the provided list. This is extremely useful for morphing, waveform switching, and creating ensemble logic.
  - `param`: A float evaluating the selection index. (e.g., `0.0` selects `v1`, `1.0` selects `vn`).
  - `v1...vn`: The list of expressions to choose from. Can contain static values, mathematical operations, or nested function calls.
  - **Example**: `select(MOD_A, sin(x), saw(x), tri(x))` switches between a sine, saw, and triangle wave depending on the value of the `MOD_A` knob.

#### 3.5.6. Smooth Interpolated Selection (smooth_select)

- **`smooth_select(param, v1, v2, ..., vn)`**
  The smooth counterpart to `select`. Linearly interpolates (lerps) between adjacent items in the list for fractional `param` values. It unlocks creamy, artifact-free transitions between expressions. Like `select`, `param` is clamped and scaled across the `N-1` intervals in the list.
  - `param`: A float evaluating the selection index [0.0..1.0]. A value of `0.0` yields exactly `v1`, while `0.5` between 3 items yields a 50/50 blend of `v2` and `v3`.
  - `v1...vn`: The 2 to 16 expressions to smoothly blend between.
  - **Example**: `smooth_select(MOD_A, sin(x), saw(x), tri(x))` smoothly morphs between the waveforms as the `MOD_A` knob turns.
  - **Pro-Tip (Spiced Chaos)**: Combine with prob! `smooth_select(sin(x*0.1), prob(0.5, sin(x), cos(x)), saw(x))` creates a buttery smooth blend between a randomly alternating sin/cos wave and a sawtooth wave over time.

#### 3.5.7. LFSR (Linear-Feedback Shift Register) Functions

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

#### 3.5.8. Summation Function (sigma)

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

To achieve real-time performance, scripts are not interpreted directly. Instead, they go through a four-stage pipeline:
1.  **Tokenization**: The script string is broken into a stream of tokens.
2.  **Parsing**: The tokens are organized into an Abstract Syntax Tree (AST).
3.  **Compilation**: The AST is converted into low-level bytecode.
4.  **Execution**: A stack-based Virtual Machine (VM) runs the bytecode using computed-goto dispatch.

An optional GPU execution path serializes the bytecode into a GLSL compute shader (`px_vm.comp`) for parallel wavetable generation.

### 4.2. Tokenizer

The tokenizer is the first stage of compilation. It scans the raw script string and breaks it down into a sequence of "tokens." Each token is a small, meaningful unit of the language, such as a number (`1.23`), a variable (`x`), a function name (`sin`), or an operator (`+`). This process simplifies the next stage by allowing the parser to work with a structured list of tokens rather than raw text.

For example, the expression `sin(x) * 0.5` is tokenized into:
`sin` (Function) -> `(` (Left Paren) -> `x` (Variable) -> `)` (Right Paren) -> `*` (Operator) -> `0.5` (Number)

The tokenizer recognizes all keywords, constants, and function names using longest-match-first string comparison. Unknown identifiers that start with a letter or underscore are treated as variables (e.g., sigma loop variables like `k`). The maximum token count per expression is 1024.

### 4.3. Parser (AST)

The parser takes the flat list of tokens and builds an Abstract Syntax Tree (AST) using recursive descent. The AST is a tree-like data structure that represents the grammatical structure and logical hierarchy of the expression, correctly handling operator precedence. For instance, in the expression `2 + 3 * 4`, the parser ensures that the `*` operation is a child of the `+` operation, so multiplication is performed first.

A recursion depth counter enforces a `MAX_PARSE_DEPTH` limit (default 200) to prevent stack overflow from malicious or excessively nested expressions. The parser safely aborts with an error if this limit is exceeded.

### 4.4. Compiler (Bytecode)

The compiler walks through the AST and translates it into bytecode. Bytecode is a low-level, platform-independent set of simple instructions. These instructions are designed to be executed extremely quickly by a small, efficient Virtual Machine.

For example, the expression `x + 1.0` might be compiled into bytecode that means:
1. `PUSH_VAR_X` (Push the value of `x` onto the stack)
2. `PUSH_CONST 1.0` (Push the constant `1.0` onto the stack)
3. `ADD` (Pop the top two values, add them, and push the result back)

This bytecode, along with a table of constants, is stored in a `BytecodeChunk` struct, ready for execution.

#### 4.4.1. The BytecodeChunk Structure

A `BytecodeChunk` is the compiled representation of a single expression or sub-expression. It contains:

| Field | Type | Description |
|---|---|---|
| `code` | `uint8_t[1024]` | Raw bytecode instructions. |
| `code_count` | `int` | Number of bytes used in `code`. |
| `constants` | `float[256]` | Constant pool (32-byte aligned). Literal values referenced by `OP_PUSH_CONST`. |
| `constants_count` | `int` | Number of constants used. |
| `strings` | `char*[32]` | String pool. Stores loop variable names for `sigma`. |
| `strings_count` | `int` | Number of strings used. |
| `sigma_sub_chunks` | `BytecodeChunk*[16]` | Sub-chunks for `sigma` arguments and body. |
| `sigma_sub_chunk_count` | `int` | Number of sub-chunks used. |
| `has_error` | `bool` | Set to `true` if a compilation error occurred. |

For `sigma(k, start, end, step, body)`, the compiler generates four separate sub-chunks (for `start`, `end`, `step`, and `body`) and stores them in the parent chunk's `sigma_sub_chunks` array. The `OP_SIGMA_EXEC` instruction references these sub-chunks by index.

#### 4.4.2. Flat Opcode Architecture

Every math function is a first-class opcode (e.g., `OP_SIN`, `OP_RAND`, `OP_LFSR_NOISE`) rather than being dispatched through a generic `OP_CALL` instruction. This eliminates the double-dispatch overhead of decoding a secondary function ID inside a switch statement. Common operations like `sin(x)` consume only 1 byte in the bytecode stream (the opcode itself), compared to 4 bytes under the old `OP_CALL` mechanism.

The full opcode map (80 opcodes, `0x00`–`0x4F`):

| Range | Category | Opcodes |
|---|---|---|
| `0x00`–`0x07` | Stack | `PUSH_CONST`, `PUSH_VAR_X`, `PUSH_VAR_FREQ`, `PUSH_VAR_RAND`, `PUSH_VAR_MOD_A/B/C`, `POP` |
| `0x08`–`0x0D` | Arithmetic | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEGATE` |
| `0x0E`–`0x14` | Logic/Comparison | `NOT`, `CMP_EQ/NE/GT/GE/LT/LE` |
| `0x15`–`0x16` | Control Flow | `JUMP`, `JUMP_IF_FALSE` |
| `0x17` | Legacy | `CALL_DEPRECATED` |
| `0x18`–`0x1D` | Sigma | `SIGMA_SETUP`, `PUSH_LOOP_VAR`, `SIGMA_EXEC`, `SIGMA_INIT`, `SIGMA_CHECK`, `SIGMA_INC` |
| `0x1E`–`0x2F` | Core Math | `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `ABS`, `TANH`, `EXP`, `LOG`, `LOG10`, `FLOOR`, `CEIL`, `MIN`, `MAX`, `SQRT`, `POW`, `RAND` |
| `0x30`–`0x32` | LFSR | `LFSR_VAL`, `LFSR_NOISE`, `LFSR_CLOCK` |
| `0x33`–`0x36` | FMA Family | `FMA`, `FMS`, `FNMADD`, `FNMSUB` |
| `0x37`–`0x44` | Advanced Math | `EXP2`, `LOG2`, `EXPM1`, `LOG1P`, `HYPOT`, `COPYSIGN`, `SCALBN`, `REMQUO`, `NEXTAFTER`, `FDIM`, `NAN`, `INF`, `LGAMMA`, `TGAMMA` |
| `0x45`–`0x47` | Selection | `PROB`, `SELECT`, `SMOOTH_SELECT` |
| `0x48`–`0x4A` | Taming | `CLAMP`, `MIX`, `RAMP` |
| `0x4B` | State | `MARKOV` |
| `0x4C`–`0x4E` | Branchless | `STEP`, `SIGN`, `INVERSESQRT` |
| `0x4F` | Control | `HALT` |

### 4.5. Virtual Machine (VM)

The Virtual Machine is a stack-based engine that executes bytecode. It uses GCC/Clang computed gotos (`goto *dispatch_table[instruction]`) for dispatch, which eliminates branch misprediction overhead compared to a traditional switch statement. On compilers that don't support computed gotos, it falls back to a standard switch.

The VM maintains a 512-entry float stack (32-byte aligned for SIMD compatibility). Register caching is used for the instruction pointer (`ip`) and stack pointer (`sp`) to keep them in CPU registers during execution.

#### 4.5.1. Sub-Chunk Execution

When the VM encounters a `sigma()` loop, it calls `execute_sub_chunk()` to evaluate each sub-expression (start, end, step, body). The sub-chunk executor:

- Saves the outer execution state (chunk pointer, instruction pointer, stack position).
- Uses the caller's stack frame base pointer (`outer_stack_top`) as the boundary for all stack operations, preventing the sub-chunk from reading or corrupting the parent's stack data.
- Guarantees exactly one result value is returned to the caller, with a safe fallback to `0.0f` on errors or empty stacks.
- Resets the stack pointer to `outer_stack_top` on exit to prevent stack pollution.
- Inherits the parent's sigma state and `VmParams` pointer, allowing LFSR state modification to propagate correctly.

Sigma sub-chunks do not support nested `sigma()` calls (the sub-chunk dispatch table routes sigma opcodes to error handlers).

#### 4.5.2. Security Features

The VM includes several hardening measures:

- **Jump bounds checking**: All `OP_JUMP` and `OP_JUMP_IF_FALSE` instructions validate the target instruction pointer against the bytecode length before jumping. Out-of-bounds jumps halt execution safely.
- **Parser recursion limit**: `MAX_PARSE_DEPTH` (200) prevents stack overflow from deeply nested expressions.
- **Thread-safe RNG**: The `px_rand()` LCG generator replaces `rand()` in all critical paths, eliminating global mutex contention in the audio thread. An atomic RNG state is used for compile-time wave generation.
- **Division by zero**: `OP_DIV` does not throw errors. Division by zero naturally yields `INFINITY` or `NaN`, which is preferable for audio distortion artifacts.
- **Oscillator wave index validation**: Loaded `wave_idx` values are bounds-checked and clamped to prevent out-of-bounds memory access from corrupted presets.

#### 4.5.3. GPU Compute Shader Path

When `POLYSONIX_USE_GPU` is defined, the VM can serialize a `BytecodeChunk` into a GPU-compatible format and dispatch execution to a GLSL compute shader (`px_vm.comp`). The GPU path executes 1 thread per wavetable (sequentially per sample) to maintain correctness for stateful operations like Markov chains and LFSR state. All opcodes are implemented with parity across the CPU and GPU paths.

#### 4.5.4. Native Transpiler (Benchmark Mode)

When `PX_BENCHMARK_NATIVE_WAVES` is defined, the VM bypasses the bytecode interpreter and executes pre-transpiled native C functions generated by `tools/transpile_waves.py`. This Ahead-of-Time (AOT) path provides ~61% average performance improvement over the interpreter, validating future AOT compilation strategies. In standard builds, the bytecode interpreter is always used.

### 4.6. The VmParams Struct

The `VmParams` struct is the critical link between the C environment of the synthesizer and the isolated world of the waveform script. When the synthesizer's C code needs to execute a waveform script to generate a sample, it populates an instance of this struct with all the necessary real-time values.

```c
typedef struct {
    float x;                // Current wave phase (0 to 2*PI)
    float frequency;        // Note frequency in Hz
    float rand_offset;      // Per-wave random value (0 to 1)
    float modA;             // Modulation parameter A (-1 to 1)
    float modB;             // Modulation parameter B (-1 to 1)
    float modC;             // Modulation parameter C (-1 to 1)

    // Free-running LFSR state (managed by the C caller)
    uint32_t lfsr_state;    // Current LFSR register value
    LfsrType lfsr_type;     // Active LFSR type
    uint32_t lfsr_position; // Current sequence position
    uint32_t lfsr_seed;     // Initial seed for resetting
    float    lfsr_accum_phase; // Accumulator for fractional LFSR advances

    uint32_t* rng_state_ptr; // Pointer to the voice's RNG state (for px_rand)

    // Markov chain state (up to 4 independent chains per voice)
    uint32_t markov_state[4];       // Current state index (0, 1, 2...)
    bool     markov_prev_trigger[4]; // Previous trigger value for rising-edge detection
} VmParams;
```

When the VM executes instructions like `OP_PUSH_VAR_X` or `OP_PUSH_VAR_MOD_A`, it looks up the corresponding value from the `VmParams` struct that was provided for that specific execution run. This mechanism allows the same compiled script to produce different results based on the note being played and the current state of the synthesizer's modulation sources.

### 4.7. Constraints and Limits

| Limit | Value | Description |
|---|---|---|
| Max bytecode size | 1024 bytes | Per `BytecodeChunk`. |
| Max constants | 256 floats | Per `BytecodeChunk`. |
| Max VM stack depth | 512 floats | 32-byte aligned. |
| Max sigma sub-chunks | 16 | Per main chunk. |
| Max strings | 32 | Unique variable names per chunk. |
| Max parse depth | 200 | Recursion limit for nested expressions. |
| Max token count | 1024 | Per expression. |
| Max Markov chains | 4 | Independent chains per voice. |
| Max Markov matrix | 8×8 | 64 transition probabilities. |
| Max sigma iterations | 1000 | Safety limit per loop. |
| Max select/smooth_select items | 16 | Values in the selection list. |
| LFSR table memory | ~520 KB | All 16 types fully initialized. |

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
