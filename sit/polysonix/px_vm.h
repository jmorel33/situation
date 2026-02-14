/***************************************************************************************************
*
*   Polysonix Waveform Scripting Language
*   Copyright (c) 2025 Jacques Morel
*   Licensed under the MIT License.
*
****************************************************************************************************/
/*
Polysonix Waveform Scripting Language: Structure and Operands

Overview:

The Polysonix Waveform Scripting Language is a domain-specific language for defining mathematical expressions that generate audio waveforms in the Polysonix synthesizer.
Expressions are stored as strings in WaveDefinition structures, tokenized, parsed into an abstract syntax tree (AST), compiled into bytecode, and executed by a virtual
This comment details the language's structure, operand symbols, and components, as used in px_vm.h.

Language Structure:

The language follows a C-like mathematical expression grammar, parsed using recursive descent. Expressions are composed of:

Literals    : Floating-point numbers (e.g., 1.0, 0.5).
Variables   : x, FREQUENCY, MOD_A, MOD_B, MOD_C, RAND_OFFSET, and loop variables (e.g., k).
Constants   : PI, TWO_PI, PI_OVER_2, THREE_PI_OVER_2, E, LFSR type constants.
Functions   : Mathematical, utility, and LFSR functions (e.g., sin, sigma, lfsr_val).
Operators   : Arithmetic, unary, comparison, logical, and ternary.
Grouping    : Parentheses () for precedence and function arguments.
Commas      : Separate function arguments.

Parsing Hierarchy (Precedence, Highest to Lowest):

Primary     : Literals, variables, constants, functions, parenthesized expressions.
Unary       : +, -, !.                  Factor      : *, /, %.
Term        : +, -.                     Comparison  : <, >, <=, >=, ==, !=.
Logical AND : &&.                       Logical XOR : ^.
Logical OR  : ||.
Conditional : ? : (ternary).            Expression  : Full expression combining above.

Example: "sin(x + 0.5 * MOD_A) + lfsr_noise(LFSR_8BIT, 2.0) * (x < PI ? 1.0 : -1.0)"
Parsed as: sin(primary) + lfsr_noise(primary, primary) * conditional(comparison ? primary : primary).

Operand Symbols:
The language supports the following operators, listed with their symbols and roles:

Arithmetic (Binary):
+   : Addition.                         -   : Subtraction.
*   : Multiplication.                   /   : Division.
%   : Modulus (remainder).

Unary:
+   : Positive (no-op).                 -   : Negation.
!   : Logical NOT.

Comparison:
<   : Less than.                        >   : Greater than.
<=  : Less than or equal to.            >=  : Greater than or equal to.
==  : Equal to.                         !=  : Not equal to.

Logical:
&&  : Logical AND.                      ||  : Logical OR.
^   : Logical XOR (true if exactly one operand is true).

Ternary:
?   : Ternary condition (e.g., cond ? true_expr : false_expr).
:   : Ternary separator.

Other Symbols:
( ) : Parentheses for grouping and function calls.
,   : Comma for function argument separation.

Variables and Parameters (Available within the script):
x                   : Phase, typically normalized from 0 to 2*PI over one wave cycle.
FREQUENCY           : Current note frequency in Hertz (Hz).
MOD_A, MOD_B, MOD_C : Modulation parameters, typically ranging from -1.0 to 1.0.
RAND_OFFSET         : A per-wave random value, typically from 0.0 to 1.0, constant for the duration of one wave generation.
k                   : Default loop variable name for the sigma() summation function. Other names can be used.

Constants:
PI                  : 3.14159265358979323846.
TWO_PI              : 2 * PI.
PI_OVER_2           : PI / 2.
THREE_PI_OVER_2     : 3 * PI / 2.
E                   : Base of natural logarithm (approx. 2.71828).

LFSR Type Constants (Integer values passed to LFSR functions):
LFSR_4BIT           : 0 (Period: 15)
LFSR_5BIT           : 1 (Period: 31)
LFSR_6BIT           : 2 (Period: 63)
LFSR_7BIT           : 3 (Period: 127)
LFSR_8BIT           : 4 (Period: 255)
LFSR_9BIT           : 5 (Period: 511)
LFSR_10BIT          : 6 (Period: 1023)
LFSR_11BIT          : 7 (Period: 2047)
LFSR_12BIT          : 8 (Period: 4095)
LFSR_13BIT          : 9 (Period: 8191)
LFSR_14BIT          : 10 (Period: 16383)
LFSR_15BIT          : 11 (Period: 32767)
LFSR_16BIT          : 12 (Period: 65535)
LFSR_17BIT          : 13 (Period: 131071)
LFSR_GALOIS         : 14 (Typically an alternative feedback topology, config matches LFSR_16BIT in current setup)
LFSR_FIBONACCI      : 15 (Standard feedback topology, config matches LFSR_16BIT in current setup)

Functions:
The language supports these functions (name(arity)):
sin(1), cos(1), tan(1)      : Trigonometric functions (input in radians).
asin(1), acos(1), atan(1)   : Inverse trigonometric functions (result in radians).
abs(1)                      : Absolute value.
tanh(1)                     : Hyperbolic tangent.
exp(1)                      : Exponential (e to the power of x).
log(1)                      : Natural logarithm (ln(x)). Input must be > 0.
log10(1)                    : Base-10 logarithm (log10(x)). Input must be > 0.
floor(1)                    : Round down to the nearest integer.
ceil(1)                     : Round up to the nearest integer.
min(2)                      : Minimum of two values.
max(2)                      : Maximum of two values.
sqrt(1)                     : Square root. Input must be >= 0.
pow(2)                      : Power (base, exponent).
rand()                      : Returns a pseudo-random float between 0.0 and 1.0.
sigma(5)                    : Summation (loop_var_name, start_val, end_val, step_val, expression_to_sum).
lfsr_val(3)                 : LFSR bit value (type, position_norm, seed_norm).
lfsr_noise(2)               : LFSR bipolar noise (type, rate).
lfsr_clock(2)               : LFSR rhythmic pulses (type, density).

LFSR Functions (Detailed Behavior):
LFSR functions can operate in two modes depending on the C execution environment (VmParams setup):
1. Precomputed Table Mode (Default): Uses pre-generated bit sequences for the specified LFSR type.
2. Free-Running Mode: Uses a stateful LFSR whose state is maintained in VmParams by the C caller.
   This mode is active if VmParams.lfsr_type matches the function's 'type' argument and VmParams.lfsr_state is non-zero.

lfsr_val(type, position_norm, seed_norm)
  Returns the current LFSR bit value (0.0 or 1.0).
  - type: LFSR type ID (e.g., LFSR_8BIT).
  - Precomputed Mode:
  - `position_norm`: Normalized position in the sequence (0.0 to <1.0), which wraps around.
  - `seed_norm`: A normalized offset (0.0 to <1.0) added to `position_norm`, which also wraps.
  - Free-Running Mode:
    - Advances the free-running LFSR state for 'type' by one step.
    - Returns the new LSB.
    - 'position_norm' and 'seed_norm' arguments are popped from stack but IGNORED.

lfsr_noise(type, rate)
  Returns bipolar LFSR noise (-1.0 or 1.0).
  - type: LFSR type ID.
  - Precomputed Mode:
    - rate: Multiplier for how quickly the LFSR sequence is scanned relative to the main phase 'x'.
            (phase_for_lfsr = (x / TWO_PI) * rate)
  - Free-Running Mode:
    - Advances the free-running LFSR state for 'type' by one step.
    - Returns the new LSB converted to bipolar noise.
    - 'rate' argument is popped from stack but IGNORED (effective rate is 1 step per call).

lfsr_clock(type, density)
  Returns rhythmic clock pulses (0.0 or 1.0) from an LFSR.
  - type: LFSR type ID.
  - density: Threshold (0.0 to 1.0). A pulse (1.0) is generated if the LFSR bit is >= density.
  - Precomputed Mode:
    - Uses the main phase 'x' to determine position in the LFSR sequence.
  - Free-Running Mode:
    - Advances the free-running LFSR state for 'type' by one step.
    - Applies 'density' to the new LSB.

Sigma Summation:
Syntax  : sigma(k, start, end, step, expr)
  - k     : The name of the loop variable (e.g., 'k', 'i', 'n'). Must be a valid identifier.
  - start : The initial floating-point value of the loop variable (inclusive).
  - end   : The final floating-point value of the loop variable. The loop continues as long as k <= end (for positive step) or k >= end (for negative step).
  - step  : The floating-point increment (or decrement if negative) for the loop variable per iteration. Cannot be zero.
  - expr  : The expression to be evaluated and summed. This expression can use the loop variable 'k'.

Example : "sigma(k, 1.0, 8.0, 1.0, sin(x*k)/k)"
  Sums sin(x*k)/k for k = 1.0, 2.0, ..., 8.0.

Compilation and Execution:
Tokenizer: Splits expressions into tokens (TOKEN_NUMBER, TOKEN_VARIABLE, etc.).
Parser: Builds an Abstract Syntax Tree (AST) via functions like parseExpression, parseTerm, etc.
Compiler: Translates the AST into a BytecodeChunk containing bytecode instructions, a constant pool, a string pool, and (for sigma) sub-chunks.
VM: A stack-based virtual machine executes the bytecode. It receives runtime parameters via a VmParams struct, which includes:
    - x (current phase)
    - frequency (current note frequency)
    - modA, modB (modulation inputs)
    - rand_offset (per-wave random value)
    - lfsr_state, lfsr_type, lfsr_position, lfsr_seed (for managing free-running LFSR state by the C caller).
LFSR System:
  - Pre-computed Tables: For each LFSR type, a bit sequence can be pre-generated and stored. In this mode, LFSR functions perform a lookup into these tables.
  - Free-Running State: Alternatively, the C code calling the VM can manage an LFSR's state (current value, type, position) within the VmParams struct. If an LFSR function call in the script matches the type
    configured for free-running mode in VmParams, the VM will advance this external state and use its output, potentially ignoring some script arguments (like position, seed, or rate for free-running LFSRs).
Output: The VM's execution of the bytecode for a given set of VmParams results in a single floating-point value, typically representing an audio sample, which is then clamped to the range [-1.0, 1.0].

Constraints:
Max bytecode size per chunk: 1024 bytes.
Max constants per chunk: 256 floats.
Max VM stack depth: 512 floats.
Max sigma sub-chunks per main chunk: 16.
Max unique strings per chunk: 32.
LFSR pre-computed table memory: Approximately 520KB for all defined types if fully initialized.

Wave Sequencing:
(This section describes C-level structures, not the scripting language itself)
Sequences in `default_sequences` (defined in C) specify wave index, cycle count, and state flags
(e.g., `WSTA_SEQ_END`, `WSTA_SEQ_PITCH_SCALE`). Example:
`{42, 1, WSTA_SEQ_RETRIGGER_ADSR}, {42, 1, WSTA_SEQ_MUTE}, ...`

Example WaveDefinition (C struct storing the script string):
`{ "LFSR Rhythm", "sin(x) * lfsr_clock(LFSR_8BIT, 0.5 + 0.3 * MOD_A) + 0.2 * lfsr_noise(LFSR_4BIT, 2.0 + MOD_B)" }`

*/

#ifndef PX_VM_H
#define PX_VM_H

#ifdef POLYSONIX_USE_GPU
#include "situation.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h> // For vm_error

#ifdef __cplusplus // Calling C code from C++

#ifdef __cplusplus
extern "C" {
#endif

#endif

// --- Portable Alignment Macro ---
#if defined(__GNUC__) || defined(__clang__)
    #define POLYSONIX_ALIGN(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
    #define POLYSONIX_ALIGN(n) __declspec(align(n))
#elif __STDC_VERSION__ >= 201112L
    #define POLYSONIX_ALIGN(n) _Alignas(n)
#else
    #define POLYSONIX_ALIGN(n)
#endif

// --- Branch Prediction Macros ---
#if defined(__GNUC__) || defined(__clang__)
    #define PX_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define PX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define PX_LIKELY(x)   (x)
    #define PX_UNLIKELY(x) (x)
#endif

// Include necessary headers from polysonix.h or define M_PI if needed
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BYTECODE_SIZE 1024
#define MAX_CONSTANTS     256
#define MAX_VM_STACK      512
#define MAX_SIGMA_CHUNKS  16 // Max nested or parallel sigma chunks needed
#define MAX_STRINGS 32 // Max unique string literals (variable names, etc.) per chunk

#ifndef NUM_DEFAULT_WAVES
#define NUM_DEFAULT_WAVES 256
#endif

// --- LFSR Definitions ---
#define NUM_LFSR_TYPES 16
#define MAX_LFSR_PERIOD 131071 // For 17-bit LFSR
#define LFSR_TABLE_BYTES(period) (((period) + 7) / 8) // Packed bits

/**
 * @enum LfsrType
 * @brief Defines the available Linear Feedback Shift Register (LFSR) types.
 *
 * Each LFSR type corresponds to a specific bit-length and feedback polynomial,
 * resulting in a pseudo-random sequence with a known period length. These constants
 * are used as integer arguments in waveform script functions like `lfsr_noise()`.
 */
typedef enum {
    LFSR_4BIT = 0,      /**< 4-bit LFSR with a period of 15. */
    LFSR_5BIT = 1,      /**< 5-bit LFSR with a period of 31. */
    LFSR_6BIT = 2,      /**< 6-bit LFSR with a period of 63. */
    LFSR_7BIT = 3,      /**< 7-bit LFSR with a period of 127. */
    LFSR_8BIT = 4,      /**< 8-bit LFSR with a period of 255. */
    LFSR_9BIT = 5,      /**< 9-bit LFSR with a period of 511. */
    LFSR_10BIT = 6,     /**< 10-bit LFSR with a period of 1023. */
    LFSR_11BIT = 7,     /**< 11-bit LFSR with a period of 2047. */
    LFSR_12BIT = 8,     /**< 12-bit LFSR with a period of 4095. */
    LFSR_13BIT = 9,     /**< 13-bit LFSR with a period of 8191. */
    LFSR_14BIT = 10,    /**< 14-bit LFSR with a period of 16383. */
    LFSR_15BIT = 11,    /**< 15-bit LFSR with a period of 32767. */
    LFSR_16BIT = 12,    /**< 16-bit LFSR with a period of 65535. */
    LFSR_17BIT = 13,    /**< 17-bit LFSR with a period of 131071. */
    LFSR_GALOIS = 14,   /**< 16-bit LFSR using a Galois feedback configuration. */
    LFSR_FIBONACCI = 15 /**< 16-bit LFSR using a Fibonacci feedback configuration. */
} LfsrType;

/**
 * @brief Initializes and pre-computes the bit sequences for all LFSR types.
 *
 * This function must be called once at application startup before any waveform
 * scripts utilizing LFSR functions are executed. It allocates memory for each
 * LFSR's bit table and generates the full pseudo-random sequence based on the
 * defined feedback polynomials. Failure to call this function will result in
 * undefined behavior for LFSR-related script functions.
 */
void px_vm_init_lfsr_tables(void);

/**
 * @brief Frees all memory allocated for the pre-computed LFSR bit tables.
 *
 * This function should be called once at application shutdown to release the
 * memory used by the LFSR system and prevent memory leaks.
 */
void px_vm_free_lfsr_tables(void);

/**
 * @brief Retrieves a single bit from a pre-computed LFSR table.
 *
 * This C function provides direct access to the pre-computed LFSR bit sequences.
 * It's used internally by the VM but can also be used for custom C-level logic.
 *
 * @param type The LFSR type (e.g., LFSR_8BIT) from which to retrieve a bit.
 * @param position The absolute position in the sequence. The value is wrapped automatically
 *                 using a modulo operation based on the period of the selected LFSR type.
 * @return The bit value as a float (1.0f or 0.0f). Returns 0.0f if the
 *         specified LFSR type is invalid or has not been initialized.
 */
float lfsr_get_bit(LfsrType type, uint32_t position);

/**
 * @brief Generates bipolar LFSR noise (-1.0f or 1.0f) based on phase.
 *
 * This C function provides direct access to the pre-computed LFSR tables,
 * generating a bipolar noise signal. It is used internally by the VM for the
 * `lfsr_noise` script function.
 *
 * @param type The LFSR type to use for noise generation.
 * @param phase The current main oscillator phase (typically 0 to 2*PI).
 * @param rate A multiplier that scales how fast the LFSR sequence is scanned
 *             relative to the main phase. A rate of 1.0 scans the full sequence
 *             over one cycle of the main phase.
 * @return A bipolar noise value (-1.0f or 1.0f). Returns 0.0f if the LFSR type
 *         is invalid or uninitialized.
 */
float lfsr_get_noise(LfsrType type, float phase, float rate);

/**
 * @brief Generates rhythmic LFSR clock pulses (0.0f or 1.0f).
 *
 * This C function uses a pre-computed LFSR sequence to generate rhythmic clock
 * pulses. A pulse (1.0f) is produced when the LFSR bit value meets or exceeds
 * the density threshold. It is used internally by the VM for the `lfsr_clock` script function.
 *
 * @param type The LFSR type to use for the clock signal.
 * @param phase The current main oscillator phase (typically 0 to 2*PI), used to
 *              determine the position in the LFSR sequence.
 * @param density A threshold (clamped between 0.0 and 1.0). A higher value results
 *                in fewer pulses (lower density), as a pulse is only generated
 *                when the LFSR bit value (0.0 or 1.0) is >= density.
 * @return A clock pulse value (1.0f or 0.0f). Returns 0.0f if the LFSR type is
 *         invalid or uninitialized.
 */
float lfsr_get_clock(LfsrType type, float phase, float density);

typedef struct BytecodeChunk BytecodeChunk;

// Structure to hold compiled bytecode and associated data
struct BytecodeChunk {
    uint8_t code[MAX_BYTECODE_SIZE]; // Array of raw bytes
    int     code_count;              // How many bytes are used in 'code'
    POLYSONIX_ALIGN(32) float   constants[MAX_CONSTANTS]; // Array of floats
    int     constants_count;         // How many constants are used
    char*   strings[MAX_STRINGS];     // Array of char pointers (to heap copies)
    int     strings_count;            // How many strings are used
    BytecodeChunk* sigma_sub_chunks[MAX_SIGMA_CHUNKS]; // Array of pointers
    int sigma_sub_chunk_count;      // How many sub-chunks are used
};

// Structure to hold loop variable info during sigma execution
typedef struct {
    const char* name;        // Points to the name string (e.g., "k") from the chunk's string pool
    float       current_value; // The value of the loop variable in the current sigma iteration
} LoopVarInfo;

typedef struct {
    const char *name;       // Human-readable name for the wave
    const char *expression; // The mathematical expression string
    BytecodeChunk* compiled_bytecode; // Pointer to the compiled version (initially NULL)
} WaveDefinition;

extern WaveDefinition default_waves[NUM_DEFAULT_WAVES];

// Structure to hold runtime parameters for the VM - Enhanced with LFSR state fields
typedef struct {
    float x;            // Phase (0 to 2*PI)
    float frequency;    // Current Note Frequency (Hz)
    float rand_offset;  // Per-wave random value (0 to 1)
    float modA;         // Modulation A (-1 to 1)
    float modB;         // Modulation B (-1 to 1)
    float modC;         // Modulation B (-1 to 1)
    // LFSR fields for stateless free-running LFSR
    uint32_t lfsr_state;    // Current state of the free-running LFSR
    LfsrType lfsr_type;     // LFSR type (e.g., LFSR_8BIT, LFSR_16BIT)
    uint32_t lfsr_position; // Current position in the LFSR sequence
    uint32_t lfsr_seed;     // Initial seed for resetting (can be used by caller to reset lfsr_state)
    float lfsr_accum_phase; // New: Accumulator for fractional LFSR advances
} VmParams;


// --- Updated VM Struct (now uses VmParams internally, or could just hold the pointer) ---
typedef struct {
    BytecodeChunk *chunk;                           // Currently executing chunk
    uint8_t       *ip;                              // Instruction pointer for the current chunk
    POLYSONIX_ALIGN(32) float stack[MAX_VM_STACK];  // Force 32-byte alignment for the stack
    float *stack_top;
    VmParams* params;                               // Pointer to the parameters for this execution run
    // Sigma loop state
    LoopVarInfo   active_loop_var;
    bool          is_in_sigma_body;
} VM;

// Helper for VM Errors
// Note: This uses vfprintf, requires <stdarg.h>
static void vm_error(VM *vm_ptr, const char *format, ...) {
    fprintf(stderr, "VM Runtime Error: ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    // Optionally print state (e.g., stack size, approximate IP)
    if (vm_ptr) {
        fprintf(stderr, " (Stack size: %td)", vm_ptr->stack_top - vm_ptr->stack);
        // Printing IP offset is hard without knowing the base address or having debug info
    }
    fprintf(stderr, "\n");
    // In a real system, you might set an error flag in the VM struct
    // or use longjmp for unrecoverable errors. For this demo, printing
    // and potentially returning a default value (0.0) is sufficient.
}

// --- Function IDs (for OP_CALL) ---
// Must correspond to the order/logic in the VM execution
typedef enum {
    FUNC_ID_SIN,
    FUNC_ID_COS,
    FUNC_ID_TAN,
    FUNC_ID_ASIN,
    FUNC_ID_ACOS,
    FUNC_ID_ATAN,
    FUNC_ID_ABS,
    FUNC_ID_TANH,
    FUNC_ID_EXP,
    FUNC_ID_LOG,
    FUNC_ID_LOG10,
    FUNC_ID_FLOOR,
    FUNC_ID_CEIL,
    FUNC_ID_MIN,
    FUNC_ID_MAX,
    FUNC_ID_SQRT,
    FUNC_ID_POW,
    FUNC_ID_RAND,
    FUNC_ID_LFSR_VAL,    // New LFSR functions
    FUNC_ID_LFSR_NOISE,
    FUNC_ID_LFSR_CLOCK,
    // Add others if needed
    FUNC_ID_COUNT // Total number of standard functions
} FunctionID;

// Map function names to IDs (used during compilation)
typedef struct {
    const char* name;
    FunctionID id;
    int arity;
} VmFunctionDef;

static VmFunctionDef vm_functions[] = {
    {"sin", FUNC_ID_SIN, 1},
    {"cos", FUNC_ID_COS, 1},
    {"tan", FUNC_ID_TAN, 1},
    {"asin", FUNC_ID_ASIN, 1},
    {"acos", FUNC_ID_ACOS, 1},
    {"atan", FUNC_ID_ATAN, 1},
    {"abs", FUNC_ID_ABS, 1},
    {"tanh", FUNC_ID_TANH, 1},
    {"exp", FUNC_ID_EXP, 1},
    {"log", FUNC_ID_LOG, 1},
    {"log10", FUNC_ID_LOG10, 1},
    {"floor", FUNC_ID_FLOOR, 1},
    {"ceil", FUNC_ID_CEIL, 1},
    {"min", FUNC_ID_MIN, 2},
    {"max", FUNC_ID_MAX, 2},
    {"sqrt", FUNC_ID_SQRT, 1},
    {"pow", FUNC_ID_POW, 2},
    {"rand", FUNC_ID_RAND, 0},
    {"lfsr_val", FUNC_ID_LFSR_VAL, 3},
    {"lfsr_noise", FUNC_ID_LFSR_NOISE, 2},
    {"lfsr_clock", FUNC_ID_LFSR_CLOCK, 2},
    {NULL, (FunctionID)0, 0} // Terminator
};

// --- Enhanced Interpreter Code ---

typedef struct {
    const char *name;
    int len;
    int arity; // Expected number of arguments (-1 for variable args, maybe?)
} FunctionDef;

// PI and E will be handled as constants, not functions
static FunctionDef functions[] = {
    {"sin", 3, 1},
    {"cos", 3, 1},
    {"tan", 3, 1},
    {"asin", 4, 1},
    {"acos", 4, 1},
    {"atan", 4, 1},
    {"abs", 3, 1},
    {"tanh", 4, 1},
    {"exp", 3, 1},
    {"log", 3, 1},
    {"log10", 5, 1},
    {"floor", 5, 1},
    {"ceil", 4, 1},
    {"min", 3, 2},
    {"max", 3, 2},
    {"sqrt", 4, 1},
    {"pow", 3, 2},
    {"rand", 4, 0},
    {"sigma", 5, 5},
    {"lfsr_val", 8, 3},
    {"lfsr_noise", 10, 2},
    {"lfsr_clock", 10, 2},
    {NULL, 0, 0}
};

// Expanded TokenType
typedef enum {
    TOKEN_NUMBER,
    TOKEN_VARIABLE,       // 'x'
    TOKEN_FREQUENCY,      // 'FREQUENCY'
    TOKEN_CONSTANT,       // PI, E, LFSR constants etc.
    TOKEN_FUNCTION,
    TOKEN_OPERATOR,       // Binary +-*/%
    TOKEN_UNARY_OP,       // Unary + - !
    TOKEN_COMPARISON,     // < > <= >= == !=
    TOKEN_LOGICAL_AND,    // &&
    TOKEN_LOGICAL_OR,     // ||
    TOKEN_LOGICAL_XOR,    // ^
    TOKEN_TERNARY_QM,     // ?
    TOKEN_TERNARY_CL,     // :
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_COMMA, TOKEN_END,
    TOKEN_RAND_OFFSET,    // Existing per-wave random value
    TOKEN_MOD_A,          // External Modifier A
    TOKEN_MOD_B,          // External Modifier B
    TOKEN_MOD_C           // External Modifier C
} TokenType;

typedef struct {
    TokenType type;
    char value[32]; // Stores number string, variable name, function name, or operator/symbol
} Token;

// Enhanced Node Structure
typedef struct Node {
    TokenType type;
    union {
        float number;
        char op_str[3];     // For binary ops (+-*/%), comparisons (==, <= etc.)
        char name[32];      // For TOKEN_VARIABLE, TOKEN_CONSTANT, TOKEN_FUNCTION
        char unary_op;      // For TOKEN_UNARY_OP ('-', '!')
    } data;
    // Children adjusted for clarity
    struct Node *child1; // Operand1 / Unary operand / Condition / Function Arg list head
    struct Node *child2; // Operand2 / True branch
    struct Node *child3; // False branch
    // Keep 'right' pointer specifically for function argument lists
    struct Node *right; // Used ONLY to link function arguments list (child1->right->...)
} Node;

// --- Forward Declarations ---

/**
 * @brief Parses a list of arguments for a function call.
 * @details Handles comma-separated expressions within parentheses. It has special handling for the `sigma`
 *          function's first argument, which must be a variable name.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @param func_name The name of the function whose arguments are being parsed.
 * @param expected_args The number of arguments the function expects, or -1 for a variable number.
 * @return A pointer to the head of a linked list of argument expression nodes, or NULL on failure.
 */
Node *parseFunctionArgs(Token *tokens, int *pos, const char* func_name, int expected_args);

/**
 * @brief Parses a full expression, starting from the lowest precedence level.
 * @details This is the main entry point for the recursive descent parser after tokenization.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the fully parsed Abstract Syntax Tree, or NULL on failure.
 */
Node *parseExpression(Token *tokens, int *pos);

/**
 * @brief Parses a conditional (ternary) expression (`condition ? true_expr : false_expr`).
 * @details This is the lowest-precedence operator. It parses a logical OR expression for the condition.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed conditional expression subtree, or NULL on failure.
 */
Node *parseConditional(Token *tokens, int *pos);

/**
 * @brief Parses logical OR expressions (`||`).
 * @details It has a lower precedence than logical XOR and logical AND.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed logical OR expression subtree, or NULL on failure.
 */
Node *parseLogicalOr(Token *tokens, int *pos);

/**
 * @brief Parses logical AND expressions (`&&`).
 * @details It has a higher precedence than logical OR but lower than comparison operators.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed logical AND expression subtree, or NULL on failure.
 */
Node *parseLogicalAnd(Token *tokens, int *pos);

/**
 * @brief Parses logical XOR expressions (`^`).
 * @details It has a higher precedence than logical OR but lower than logical AND.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed logical XOR expression subtree, or NULL on failure.
 */
Node *parseLogicalXor(Token *tokens, int *pos);

/**
 * @brief Parses comparison expressions (e.g., `<`, `>`, `==`, `!=`).
 * @details It has a higher precedence than logical operators but lower than additive terms.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed comparison expression subtree, or NULL on failure.
 */
Node *parseComparison(Token *tokens, int *pos);

/**
 * @brief Parses additive expressions (`+`, `-`).
 * @details It has a higher precedence than comparison operators but lower than multiplicative factors.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed term subtree, or NULL on failure.
 */
Node *parseTerm(Token *tokens, int *pos);

/**
 * @brief Parses multiplicative expressions (`*`, `/`, `%`).
 * @details It has a higher precedence than additive terms but lower than unary operators.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed factor subtree, or NULL on failure.
 */
Node *parseFactor(Token *tokens, int *pos);

/**
 * @brief Parses unary expressions (`-`, `!`).
 * @details It has a higher precedence than multiplicative factors but lower than primary expressions.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed unary expression subtree, or NULL on failure.
 */
Node *parseUnary(Token *tokens, int *pos);

/**
 * @brief Parses primary expressions (literals, variables, constants, parenthesized expressions, function calls).
 * @details This is the highest level of precedence in the grammar.
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the parsed primary expression subtree, or NULL on failure.
 */
Node *parsePrimary(Token *tokens, int *pos);

// Helper for freeing memory allocated with aligned_calloc
static void aligned_free(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    // On POSIX systems (Linux, macOS), memory from posix_memalign/aligned_alloc
    // can be freed with the standard free().
    free(ptr);
#endif
}

// Helper that allocates aligned, zero-initialized memory
static void* aligned_calloc(size_t alignment, size_t num, size_t size) {
    void* ptr = NULL;
    size_t total_size = num * size;
    if (total_size == 0) {
        return NULL;
    }

    // THE CHANGE IS HERE: Use _WIN32 to detect all Windows targets first.
#if defined(_WIN32)
    ptr = _aligned_malloc(total_size, alignment);
#elif defined(__GNUC__) || defined(__clang__)
    // This now correctly covers GCC/Clang on non-Windows (Linux, macOS).
    if (posix_memalign(&ptr, alignment, total_size) != 0) {
        ptr = NULL;
    }
#elif __STDC_VERSION__ >= 201112L
    // C11 standard as a fallback for other compilers.
    ptr = aligned_alloc(alignment, total_size);
#else
    // Final fallback to standard malloc.
    ptr = malloc(total_size);
#endif

    // Zero the memory if allocation was successful
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

// --- Parser, AST, and Tokenizer Functions ---

/**
 * @brief Converts a mathematical expression string into a sequence of tokens.
 *
 * This function performs lexical analysis on the input string, breaking it down
 * into numbers, variables, operators, functions, and other language symbols.
 *
 * @param expression The null-terminated expression string to tokenize.
 * @param tokens A pre-allocated array of Token structures to be filled.
 * @param maxTokens The maximum number of tokens the `tokens` array can hold.
 * @return The number of tokens generated, or a negative value on error.
 */
int tokenize(const char *expression, Token *tokens, int maxTokens);

/**
 * @brief Recursively frees all memory associated with an Abstract Syntax Tree (AST).
 *
 * @param node The root node of the AST (or sub-tree) to free.
 */
void freeAST(Node *node);

/**
 * @brief Allocates and initializes a new AST node.
 *
 * @param type The TokenType for the new node.
 * @return A pointer to the newly allocated Node, or NULL on failure.
 */
Node *createNode(TokenType type);

/**
 * @brief Parses a sequence of tokens into an Abstract Syntax Tree (AST).
 *
 * This is the entry point for the recursive descent parser. It starts parsing
 * from the lowest precedence level (the conditional/ternary operator).
 *
 * @param tokens An array of tokens generated by `tokenize()`.
 * @param pos A pointer to the current position in the `tokens` array.
 * @return A pointer to the root node of the generated AST, or NULL on parsing failure.
 */
Node *parseExpression(Token *tokens, int *pos);

// --- Enhanced Tokenizer ---
int tokenize(const char *expression, Token *tokens, int maxTokens) {
    int tokenCount = 0;
    int pos = 0;

    while (expression[pos] && tokenCount < maxTokens) {
        while (isspace(expression[pos])) pos++;
        if (!expression[pos]) break;

        Token *t = &tokens[tokenCount++];
        char c1 = expression[pos];
        char c2 = expression[pos + 1];

        // --- Check Order ---
        // 1. Numbers (Must start with digit or '.')
        if (isdigit(c1) || (c1 == '.' && isdigit(c2))) {
            t->type = TOKEN_NUMBER;
            int len = 0;
            int decimal_count = 0;
            while (isdigit(expression[pos]) || (expression[pos] == '.' && decimal_count == 0)) {
                if (expression[pos] == '.') decimal_count++;
                if (len < 31) t->value[len++] = expression[pos++];
                else pos++; // Avoid overflow
            }
            t->value[len] = '\0';
            continue;
        }

        // 2. Identifiers/Keywords (Can start with letter or underscore)
        if (isalpha(c1) || c1 == '_') { // Could be a keyword, constant, function, or 'x'
            if (strncmp(&expression[pos], "FREQUENCY", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_FREQUENCY; strncpy(t->value, "FREQUENCY", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "THREE_PI_OVER_2", 15) == 0 && !isalnum(expression[pos+15]) && expression[pos+15] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "THREE_PI_OVER_2", 31); t->value[31]='\0'; pos += 15; continue; }
            if (strncmp(&expression[pos], "PI_OVER_2", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "PI_OVER_2", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "TWO_PI", 6) == 0 && !isalnum(expression[pos+6]) && expression[pos+6] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "TWO_PI", 31); t->value[31]='\0'; pos += 6; continue; }
            if (strncmp(&expression[pos], "RAND_OFFSET", 11) == 0 && !isalnum(expression[pos+11]) && expression[pos+11] != '_') { t->type = TOKEN_RAND_OFFSET; strncpy(t->value, "RAND_OFFSET", 31); t->value[31]='\0'; pos += 11; continue; }
            if (strncmp(&expression[pos], "LFSR_FIBONACCI", 14) == 0 && !isalnum(expression[pos+14]) && expression[pos+14] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_FIBONACCI", 31); t->value[31]='\0'; pos += 14; continue; }
            if (strncmp(&expression[pos], "LFSR_GALOIS", 11) == 0 && !isalnum(expression[pos+11]) && expression[pos+11] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_GALOIS", 31); t->value[31]='\0'; pos += 11; continue; }
            if (strncmp(&expression[pos], "LFSR_17BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_17BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_16BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_16BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_15BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_15BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_14BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_14BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_13BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_13BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_12BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_12BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_11BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_11BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_10BIT", 10) == 0 && !isalnum(expression[pos+10]) && expression[pos+10] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_10BIT", 31); t->value[31]='\0'; pos += 10; continue; }
            if (strncmp(&expression[pos], "LFSR_9BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_9BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "LFSR_8BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_8BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "LFSR_7BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') {  t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_7BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "LFSR_6BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_6BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "LFSR_5BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') {  t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_5BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "LFSR_4BIT", 9) == 0 && !isalnum(expression[pos+9]) && expression[pos+9] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "LFSR_4BIT", 31); t->value[31]='\0'; pos += 9; continue; }
            if (strncmp(&expression[pos], "MOD_A", 5) == 0 && !isalnum(expression[pos+5]) && expression[pos+5] != '_') { t->type = TOKEN_MOD_A; strncpy(t->value, "MOD_A", 31); t->value[31]='\0'; pos += 5; continue; }
            if (strncmp(&expression[pos], "MOD_B", 5) == 0 && !isalnum(expression[pos+5]) && expression[pos+5] != '_') { t->type = TOKEN_MOD_B; strncpy(t->value, "MOD_B", 31); t->value[31]='\0'; pos += 5; continue; }
            if (strncmp(&expression[pos], "MOD_C", 5) == 0 && !isalnum(expression[pos+5]) && expression[pos+5] != '_') { t->type = TOKEN_MOD_C; strncpy(t->value, "MOD_C", 31); t->value[31]='\0'; pos += 5; continue; }
            if (strncmp(&expression[pos], "PI", 2) == 0 && !isalnum(expression[pos+2]) && expression[pos+2] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "PI", 31); t->value[31]='\0'; pos += 2; continue; }
            if (strncmp(&expression[pos], "E", 1) == 0 && !isalnum(expression[pos+1]) && expression[pos+1] != '_') { t->type = TOKEN_CONSTANT; strncpy(t->value, "E", 31); t->value[31]='\0'; pos += 1; continue; }
            if (c1 == 'x' && !isalnum(c2) && c2 != '_') { t->type = TOKEN_VARIABLE; strncpy(t->value, "x", 31); t->value[31]='\0'; pos++; continue; }

            // If it wasn't a specific keyword/constant/variable, check known functions
            bool funcFound = false;
            for (int i = 0; functions[i].name; i++) {
                 // This part is fine - checks strncmp for known functions like sin, cos, sigma...
                if (strncmp(&expression[pos], functions[i].name, functions[i].len) == 0 && !isalnum(expression[pos + functions[i].len]) && expression[pos + functions[i].len] != '_') {
                    t->type = TOKEN_FUNCTION;
                    strncpy(t->value, functions[i].name, 31);
                    t->value[31] = '\0';
                    pos += functions[i].len;
                    funcFound = true;
                    break; // Exit function check loop
                }
            }
            if (funcFound) continue; // Continue main loop if function found

            // If it starts with a letter/underscore but didn't match anything known
            // up to this point (keyword, constant, 'x', known function),
            // assume it's a variable (like 'k' for sigma).
            // The compiler will validate its usage later.
            { // Add braces for scope clarity if needed
                int len = 0;
                char ident_buffer[32]; // Temporary buffer to read the identifier
                while (isalnum(expression[pos]) || expression[pos] == '_') {
                    if (len < 31) ident_buffer[len++] = expression[pos++];
                    else pos++; // Avoid buffer overflow but keep advancing position
                }
                ident_buffer[len] = '\0';

                // Now assign it as a variable token
                t->type = TOKEN_VARIABLE;
                strncpy(t->value, ident_buffer, 31);
                t->value[31] = '\0';
                // pos was already advanced by the while loop above
                continue; // Continue to the next token in the main loop
            }
        } // End of isalpha/isunderscore block

        // 3. Operators and Punctuation (These don't start with letters/numbers)
        //    Order matters for multi-char ops vs single-char ops
        else if (c1 == '&' && c2 == '&') { t->type = TOKEN_LOGICAL_AND; t->value[0] = '&'; t->value[1] = '&'; t->value[2] = '\0'; pos += 2; continue; }
        else if (c1 == '|' && c2 == '|') { t->type = TOKEN_LOGICAL_OR;  t->value[0] = '|'; t->value[1] = '|'; t->value[2] = '\0'; pos += 2; continue; }
        else if (c1 == '^') { t->type = TOKEN_LOGICAL_XOR;  t->value[0] = '^'; t->value[1] = '\0'; pos ++; continue; }
        else if ((c1 == '<' || c1 == '>' || c1 == '=' || c1 == '!') && c2 == '=') { t->type = TOKEN_COMPARISON; t->value[0] = c1; t->value[1] = c2; t->value[2] = '\0'; pos += 2; continue; }
        else if (c1 == '<' || c1 == '>') { t->type = TOKEN_COMPARISON; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (strchr("*/%", c1)) { t->type = TOKEN_OPERATOR; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (strchr("+-", c1)) { t->type = TOKEN_OPERATOR; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == '!') { t->type = TOKEN_UNARY_OP; t->value[0] = '!'; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == '(') { t->type = TOKEN_LPAREN; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == ')') { t->type = TOKEN_RPAREN; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == ',') { t->type = TOKEN_COMMA; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == '?') { t->type = TOKEN_TERNARY_QM; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }
        else if (c1 == ':') { t->type = TOKEN_TERNARY_CL; t->value[0] = c1; t->value[1] = '\0'; pos++; continue; }

        // 4. Unknown Character
        else {
            fprintf(stderr, "Error: Unknown token at %d: '%c'\n", pos, c1);
            return -1; // Error
        }

    } // End while loop

    // Add END token
     if (tokenCount < maxTokens) {
         tokens[tokenCount].type = TOKEN_END;
     } else {
         fprintf(stderr,"Error: Max tokens (%d) reached for expression.\n", maxTokens);
         return -2; // Error: Too many tokens
     }
     return tokenCount;
}

// --- AST Node Management ---

void freeAST(Node *node) {
    if (!node) {
        return;
    }
    // Recursively free children based on node type
    // Note: Function args are linked via 'right', others use child1/2/3
    if (node->type == TOKEN_FUNCTION) {
         // Free the argument list linked via 'right' starting from child1
         Node* current_arg = node->child1;
         while (current_arg != NULL) {
             Node* next_arg = current_arg->right; // Get next before freeing current
             freeAST(current_arg); // Recursively free the expression tree for this arg
             current_arg = next_arg;
         }
         // No child2 or child3 for functions
    } else {
         // Free standard children
         freeAST(node->child1);
         freeAST(node->child2);
         freeAST(node->child3);
    }

    // Free the node itself
    free(node);
}

// --- Update createNode (Minor change for clarity) ---
Node *createNode(TokenType type) {
    Node *node = (Node *)malloc(sizeof(Node));
    if (!node) { /* ... error ... */ return NULL; }
    node->type = type;
    node->child1 = node->child2 = node->child3 = node->right = NULL;
    // Clear the union fields explicitly (optional but good practice)
    memset(&node->data, 0, sizeof(node->data));
    return node;
}

// --- Top-level parser function (Calls lowest precedence) ---
Node *parseExpression(Token *tokens, int *pos) {
    return parseConditional(tokens, pos); // Ternary is lowest precedence
}

// --- Parse Conditional (? :) --- (Calls next level: Logical OR)
Node *parseConditional(Token *tokens, int *pos) {
    Node *condition = parseLogicalOr(tokens, pos);
    if (!condition) return NULL;

    if (tokens[*pos].type == TOKEN_TERNARY_QM) {
        (*pos)++; // Consume '?'
        Node *true_branch = parseExpression(tokens, pos);
        if (!true_branch) { freeAST(condition); return NULL; }

        if (tokens[*pos].type != TOKEN_TERNARY_CL) {
            fprintf(stderr, "Error: Expected ':' in ternary operator.\n");
            freeAST(condition); freeAST(true_branch); return NULL;
        }
        (*pos)++; // Consume ':'
        Node *false_branch = parseConditional(tokens, pos);
        if (!false_branch) { freeAST(condition); freeAST(true_branch); return NULL; }

        Node *ternary_node = createNode(TOKEN_TERNARY_QM);
        if (!ternary_node) { freeAST(condition); freeAST(true_branch); freeAST(false_branch); return NULL;}
        ternary_node->child1 = condition;
        ternary_node->child2 = true_branch;
        ternary_node->child3 = false_branch;
        return ternary_node;
    }
    return condition; // No '?' found
}

// --- Parse Logical OR (||) --- (Calls next level: Logical XOR)
Node *parseLogicalOr(Token *tokens, int *pos) {
    Node *node = parseLogicalXor(tokens, pos); // MODIFIED: Call parseLogicalXor
    if (!node) return NULL;

    while (tokens[*pos].type == TOKEN_LOGICAL_OR) {
        (*pos)++; // Consume '||'
        Node *right = parseLogicalXor(tokens, pos); // MODIFIED: Call parseLogicalXor for right operand
        if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_LOGICAL_OR);
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Logical XOR (^) --- (Calls next level: Logical AND)
// MODIFIED: This function replaces the one provided in the prompt for consistency and correct precedence.
Node *parseLogicalXor(Token *tokens, int *pos) {
    Node *node = parseLogicalAnd(tokens, pos); // MODIFIED: Call parseLogicalAnd
    if (!node) return NULL;

    while (tokens[*pos].type == TOKEN_LOGICAL_XOR) {
        (*pos)++; // Consume '^'
        Node *right = parseLogicalAnd(tokens, pos); // MODIFIED: Call parseLogicalAnd for right operand
        if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_LOGICAL_XOR); // Use specific type, no op_str needed
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Logical AND (&&) --- (Calls next level: Comparison)
Node *parseLogicalAnd(Token *tokens, int *pos) {
    Node *node = parseComparison(tokens, pos); // Parse left operand
    if (!node) return NULL;

    while (tokens[*pos].type == TOKEN_LOGICAL_AND) {
        (*pos)++; // Consume '&&'
        Node *right = parseComparison(tokens, pos); // Parse right operand
        if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_LOGICAL_AND);
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Comparison --- (Calls next level: Term)
Node *parseComparison(Token *tokens, int *pos) {
    Node *node = parseTerm(tokens, pos); // *** CHANGED: Calls parseTerm ***
    if (!node) return NULL;

    while (tokens[*pos].type == TOKEN_COMPARISON) {
        Token op_token = tokens[*pos];
        (*pos)++;
        Node *right = parseTerm(tokens, pos); // Right operand is an additive term
        if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_COMPARISON);
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        strncpy(new_node->data.op_str, op_token.value, 2); // Store "<", "==", etc.
        new_node->data.op_str[2] = '\0';
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Term (+ - Binary) --- (Calls next level: Factor) - UNCHANGED
Node *parseTerm(Token *tokens, int *pos) {
    Node *node = parseFactor(tokens, pos);
    if (!node) return NULL;

    // Only handle BINARY + and - here
    while (tokens[*pos].type == TOKEN_OPERATOR && strchr("+-", tokens[*pos].value[0])) {
         // Check if it's truly binary - depends on what follows.
         // The structure handles this implicitly: parseFactor parses the potential right operand.
        Token op_token = tokens[*pos];
        (*pos)++;
        Node *right = parseFactor(tokens, pos);
         if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_OPERATOR); // Keep using TOKEN_OPERATOR for binary +/-
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        new_node->data.op_str[0] = op_token.value[0];
        new_node->data.op_str[1] = '\0';
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Factor (* / %) --- (Calls next level: Unary) - UNCHANGED
Node *parseFactor(Token *tokens, int *pos) {
    Node *node = parseUnary(tokens, pos);
    if (!node) return NULL;

    while (tokens[*pos].type == TOKEN_OPERATOR && strchr("*/%", tokens[*pos].value[0])) {
        Token op_token = tokens[*pos];
        (*pos)++;
        Node *right = parseUnary(tokens, pos);
        if (!right) { freeAST(node); return NULL; }

        Node *new_node = createNode(TOKEN_OPERATOR); // Keep using TOKEN_OPERATOR
        if (!new_node) { freeAST(node); freeAST(right); return NULL; }
        new_node->data.op_str[0] = op_token.value[0];
        new_node->data.op_str[1] = '\0';
        new_node->child1 = node;
        new_node->child2 = right;
        node = new_node;
    }
    return node;
}

// --- Parse Unary (+ - !) --- (Calls next level: Primary)
Node *parseUnary(Token *tokens, int *pos) {
    // Check for unary operators explicitly
    if ((tokens[*pos].type == TOKEN_OPERATOR && strchr("+-", tokens[*pos].value[0])) || /* Check binary +/- first? No, context matters */
         tokens[*pos].type == TOKEN_UNARY_OP /* This is '!' from tokenizer */ )
    {
        // Distinguish based on actual token type
        bool is_unary_plus_minus = (tokens[*pos].type == TOKEN_OPERATOR && strchr("+-", tokens[*pos].value[0]));
        bool is_unary_not = (tokens[*pos].type == TOKEN_UNARY_OP && tokens[*pos].value[0] == '!');

        if (is_unary_plus_minus || is_unary_not) {
            Token op_token = tokens[*pos];
            (*pos)++;
            Node *operand = parseUnary(tokens, pos); // Recursive call for multiple unary ops like --x or !!x
            if (!operand) return NULL;

            // Optimize: Ignore unary plus
            if (is_unary_plus_minus && op_token.value[0] == '+') {
                return operand;
            }

            // Create node for unary minus or unary not
            Node *node = createNode(TOKEN_UNARY_OP); // *** Use specific type ***
            if (!node) { freeAST(operand); return NULL; }
            node->data.unary_op = op_token.value[0]; // Store '-' or '!'
            node->child1 = operand; // Store operand in child1
            node->child2 = NULL;    // Unary ops have no child2
            return node;
        }
    }

    // If not a unary operator recognized above, parse the next level (primary)
    return parsePrimary(tokens, pos);
}

/**
 * @brief Parses the highest precedence elements of an expression.
 *
 * This includes:
 * - Literal numbers (TOKEN_NUMBER)
 * - Variables 'x' (TOKEN_VARIABLE)
 * - RAND_OFFSET variable (TOKEN_RAND_OFFSET)
 * - Predefined constants 'PI', 'E', LFSR constants (TOKEN_CONSTANT)
 * - External modulation inputs 'MOD_A', 'MOD_B', 'MOD_C' (TOKEN_MOD_A, TOKEN_MOD_B, TOKEN_MOD_C)
 * - Parenthesized expressions ( (...) )
 * - Function calls ( func(...) )
 *
 * @param tokens Array of tokens representing the expression.
 * @param pos Pointer to the current position within the tokens array.
 * @return Pointer to the root Node of the parsed primary expression subtree, or NULL on error.
 */
Node *parsePrimary(Token *tokens, int *pos) {
    Token t = tokens[*pos];
    if (t.type == TOKEN_END) { fprintf(stderr, "Parser Error: Unexpected end of expression at pos %d.\n", *pos); return NULL; }

    // Handle Number Literals
    if (t.type == TOKEN_NUMBER) {
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_NUMBER);
        if (!node) {
            perror("Failed to create number node");
            return NULL;
        }
        node->data.number = atof(t.value); // Convert string value to float
        return node;
    }
    // Handle Variable 'x'
    if (t.type == TOKEN_VARIABLE) { // 'x'
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_VARIABLE);
        if (!node) {
            perror("Failed to create variable node");
            return NULL;
        }
        strncpy(node->data.name, t.value, 31); // Store "x"
        node->data.name[31] = '\0';
        return node;
    }

    // Handle FREQUENCY Variable
    if (t.type == TOKEN_FREQUENCY) {
         (*pos)++; // Consume token
         Node *node = createNode(TOKEN_FREQUENCY);
         if (!node) { perror("Failed to create frequency node"); return NULL; }
         strncpy(node->data.name, "FREQUENCY", 31); // Store "FREQUENCY"
         node->data.name[31] = '\0';
         return node;
    }

    // Handle RAND_OFFSET Variable
    if (t.type == TOKEN_RAND_OFFSET) {
         (*pos)++; // Consume token
         Node *node = createNode(TOKEN_RAND_OFFSET);
         if (!node) {
            perror("Failed to create rand_offset node");
            return NULL;
         }
         strncpy(node->data.name, t.value, 31); // Store "RAND_OFFSET"
         node->data.name[31] = '\0';
         return node;
    }

    // Handle Predefined Constants (PI, E, LFSR constants)
    if (t.type == TOKEN_CONSTANT) { // PI, E, LFSR_*
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_CONSTANT);
        if (!node) {
            perror("Failed to create constant node");
            return NULL;
        }
        strncpy(node->data.name, t.value, 31); // Store constant name
        node->data.name[31] = '\0';
        return node;
    }

    // Handle External Modulation Inputs MOD_A / MOD_B / MOD_C
    if (t.type == TOKEN_MOD_A) { // Assumes TOKEN_MOD_A enum exists
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_MOD_A);
        if (!node) {
            perror("Failed to create mod_a node");
            return NULL;
        }
        strncpy(node->data.name, t.value, 31); // Store "MOD_A"
        node->data.name[31] = '\0';
        return node;
    }
     if (t.type == TOKEN_MOD_B) { // Assumes TOKEN_MOD_B enum exists
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_MOD_B);
        if (!node) {
            perror("Failed to create mod_b node");
            return NULL;
        }
        strncpy(node->data.name, t.value, 31); // Store "MOD_B"
        node->data.name[31] = '\0';
        return node;
    }
     if (t.type == TOKEN_MOD_C) { // Assumes TOKEN_MOD_C enum exists
        (*pos)++; // Consume token
        Node *node = createNode(TOKEN_MOD_C);
        if (!node) {
            perror("Failed to create mod_c node");
            return NULL;
        }
        strncpy(node->data.name, t.value, 31); // Store "MOD_C"
        node->data.name[31] = '\0';
        return node;
    }
    // Handle Parenthesized Expressions
    if (t.type == TOKEN_LPAREN) {
         (*pos)++; // Consume '('
         // Recursively parse the expression inside the parentheses, starting from the top level
         Node *node = parseExpression(tokens, pos);
         if (!node) {
             // Error already printed by lower-level parse function
             return NULL;
         }
         // Check for the matching closing parenthesis
         if (tokens[*pos].type != TOKEN_RPAREN) {
             fprintf(stderr, "Error: Missing ')' after expression starting at token %d ('%s'). Expected ')' but found '%s'.\n",
                     *pos - 1, // Approximate start of expression inside parens
                     tokens[*pos -1].value, // Token before expected ')'
                     tokens[*pos].value); // Unexpected token
             freeAST(node); // Clean up the parsed inner expression
             return NULL;
         }
         (*pos)++; // Consume ')'
         return node; // Return the parsed expression tree from inside the parentheses
    }

    // Handle Function Calls
    if (t.type == TOKEN_FUNCTION) {
         (*pos)++; // Consume function name token
         Node *node = createNode(TOKEN_FUNCTION);
         if (!node) {
            perror("Failed to create function node");
            return NULL;
         }
        strncpy(node->data.name, t.value, 31);
        node->data.name[31] = '\0';

        int expected_args = -1;
        for (int i = 0; functions[i].name; ++i) {
            if (strcmp(functions[i].name, node->data.name) == 0) {
                expected_args = functions[i].arity;
                break;
            }
        }

        if (tokens[*pos].type != TOKEN_LPAREN) {
            if (expected_args == 0) { // For `rand` without parens
                node->child1 = NULL;
                return node;
            } else {
                fprintf(stderr, "Error: Expected '(' after function name '%s' at pos %d, but found '%s'.\n", node->data.name, *pos, tokens[*pos].value);
                free(node);
                return NULL;
            }
        }
        (*pos)++; // Consume '('

        if (expected_args == 0) { // For `rand()` or `rand(0)`
            if (tokens[*pos].type == TOKEN_RPAREN) { // Correct: rand()
                (*pos)++; // Consume ')'
                node->child1 = NULL;
                return node;
            } else { // Incorrect: rand(something)
                fprintf(stderr, "Error: Function '%s' expects 0 arguments inside parentheses, but found '%s'. Expected ')'.\n", node->data.name, tokens[*pos].value);
                // Consume until ')' or error to avoid cascading parse errors
                while(tokens[*pos].type != TOKEN_RPAREN && tokens[*pos].type != TOKEN_END) {
                    (*pos)++;
                }
                if (tokens[*pos].type == TOKEN_RPAREN) (*pos)++;
                freeAST(node); // Free the function node and any partially parsed args
                return NULL;
            }
        }

        // For functions with >0 or variable arguments
        node->child1 = parseFunctionArgs(tokens, pos, node->data.name, expected_args);

         // If parseFunctionArgs returned NULL due to an error within it, and it wasn't just a valid 0-argument function, propagate the NULL
         if (node->child1 == NULL && expected_args != 0) {
            // Error was already printed by parseFunctionArgs if it failed mid-list.
            // If it failed immediately because expected_args > 0 but ')' was found, parseFunctionArgs should print that error.
            freeAST(node); // Clean up function node itself
            return NULL;
         }

         // Check for the closing parenthesis after arguments
         if (tokens[*pos].type != TOKEN_RPAREN) {
             // ... handle missing ')' error ...
             fprintf(stderr, "Error: Missing ')' after arguments for function '%s' at pos %d, found '%s'.\n",
                     node->data.name, *pos, tokens[*pos].value);
             freeAST(node);
             return NULL;
         }
         (*pos)++; // Consume ')'
         return node; // Return the complete function call node
    }

    // If none of the above matched, it's an unexpected token at this level
    fprintf(stderr, "Error: Unexpected token '%s' (type %d) encountered when parsing primary expression at pos %d.\n",
            t.value, t.type, *pos);
    return NULL; // Indicate parsing failure
}

// --- parseFunctionArgs ---
Node *parseFunctionArgs(Token *tokens, int *pos, const char* func_name, int expected_args) {
    Node *head = NULL;
    Node **current_arg_ptr = &head;
    int arg_count = 0;
    bool is_sigma = (strcmp(func_name, "sigma") == 0);

    // Special handling for sigma's expected 5 arguments
    if (is_sigma) {
        expected_args = 5;
    }

    if (tokens[*pos].type == TOKEN_RPAREN) { // Handle zero arguments case
        if (expected_args != 0 && expected_args != -1) {
             fprintf(stderr, "Parser Error (%s): Function expected %d arguments, but received 0.\n", func_name, expected_args);
             return NULL;
        }
        if (is_sigma) { // Sigma specifically needs 5 args
            fprintf(stderr, "Parser Error (sigma): Expected 5 arguments (variable, start, end, step, body), got 0.\n");
            return NULL;
        }
        return NULL; // Correctly handled 0 args for other functions
    }

    // --- Loop to parse arguments ---
    while (tokens[*pos].type != TOKEN_RPAREN && tokens[*pos].type != TOKEN_END) {
        if (arg_count > 0) {
            if (tokens[*pos].type != TOKEN_COMMA) {
                fprintf(stderr, "Parser Error (%s): Expected ',' between function arguments at pos %d, found '%s'.\n", func_name, *pos, tokens[*pos].value);
                freeAST(head); return NULL;
            }
            (*pos)++; // Consume ','
        }

        Node *arg_expr = NULL;

        // --- Special handling for sigma's first argument (variable name) ---
        if (is_sigma && arg_count == 0) {
            if (tokens[*pos].type != TOKEN_VARIABLE) { // Expecting an identifier like 'k' or 'n'
                 fprintf(stderr, "Parser Error (sigma): Expected variable name as first argument at pos %d, found '%s' (type %d).\n", *pos, tokens[*pos].value, tokens[*pos].type);
                 freeAST(head); return NULL;
            }
            // Create a node to *store* the variable name, not to evaluate 'k' itself here
            arg_expr = createNode(TOKEN_VARIABLE); // Use TOKEN_VARIABLE type
            if (!arg_expr) { freeAST(head); return NULL; }
            strncpy(arg_expr->data.name, tokens[*pos].value, 31); // Store the name "k"
            arg_expr->data.name[31] = '\0';
            (*pos)++; // Consume the variable name token
        } else {
            // Parse start, end, step, body (or args for other functions) as regular expressions
            arg_expr = parseExpression(tokens, pos);
            if (!arg_expr) {
                fprintf(stderr, "Parser Error (%s): Failed to parse argument %d.\n", func_name, arg_count + 1);
                freeAST(head); return NULL; // Error during arg parsing
            }
        }

        // Link the new argument
        *current_arg_ptr = arg_expr;
        // How we link depends on node structure. For sigma, we want flat children.
        // This simple list linking via ->right might need rethinking for sigma.
        // For now, assume we can access them sequentially from head later.
        current_arg_ptr = &arg_expr->right; // Advance the pointer for the next argument in the list

        arg_count++;

        if (expected_args != -1 && arg_count > expected_args) {
             fprintf(stderr, "Parser Error (%s): Too many arguments for function (expected %d, got >%d).\n", func_name, expected_args, arg_count-1);
             freeAST(head); return NULL;
        }
    }

     // Check minimum/exact args after loop
     if (expected_args != -1 && arg_count != expected_args) {
          fprintf(stderr, "Parser Error (%s): Incorrect number of arguments (got %d, expected %d).\n", func_name, arg_count, expected_args);
          freeAST(head); return NULL;
     }

    return head; // Return head of the argument list (or sequence for sigma)
}

// Helper for logical truthiness
#define C_PI 3.14159265358979323846
#define C_TWO_PI (2.0 * C_PI)
#define C_PI_OVER_2 (C_PI / 2.0)
#define C_THREE_PI_OVER_2 (3.0 * C_PI / 2.0)
#define C_E 2.71828182845904523536
#define EPSILON 1e-6f

// --- Bytecode Definitions ---
typedef enum {
    // --- Stack Manipulation ---
    OP_PUSH_CONST     = 0x00, // Operand: uint16_t const_pool_index
    OP_PUSH_VAR_X     = 0x01, // No operand
    OP_PUSH_VAR_FREQ  = 0x02, // No operand - NEW
    OP_PUSH_VAR_RAND  = 0x03, // No operand
    OP_PUSH_VAR_MOD_A = 0x04, // No operand
    OP_PUSH_VAR_MOD_B = 0x05, // No operand
    OP_PUSH_VAR_MOD_C = 0x06, // No operand
    OP_POP            = 0x07, // No operand

    // --- Arithmetic ---
    OP_ADD            = 0x08, // No operand
    OP_SUB            = 0x09, // No operand
    OP_MUL            = 0x0A, // No operand
    OP_DIV            = 0x0B, // No operand
    OP_MOD            = 0x0C, // No operand
    OP_NEGATE         = 0x0D, // No operand (Unary minus)

    // --- Logical / Comparison ---
    OP_NOT            = 0x0E, // No operand (Unary !)
    OP_CMP_EQ         = 0x0F, // No operand (==)
    OP_CMP_NE         = 0x10, // No operand (!=)
    OP_CMP_GT         = 0x11, // No operand (>)
    OP_CMP_GE         = 0x12, // No operand (>=)
    OP_CMP_LT         = 0x13, // No operand (<)
    OP_CMP_LE         = 0x14, // No operand (<=)
    // Logical AND/OR handled via jumps

    // --- Control Flow ---
    OP_JUMP           = 0x15, // Operand: int16_t relative_offset
    OP_JUMP_IF_FALSE  = 0x16, // Operand: int16_t relative_offset (pops condition)

    // --- Functions ---
    OP_CALL           = 0x17, // Operands: uint8_t func_id, uint8_t arg_count

    // --- Sigma (Specialized) ---
    OP_SIGMA_SETUP    = 0x18, // Operands: (See compiler/VM) - Likely won't be used if only OP_SIGMA_EXEC emitted
    OP_PUSH_LOOP_VAR  = 0x19, // Operand: uint8_t var_name_id
    OP_SIGMA_EXEC     = 0x1A, // Operands: uint8_t name_idx, uint16_t start_idx, uint16_t end_idx, uint16_t step_idx, uint16_t body_idx

    // --- End ---
    OP_HALT           = 0x1B  // No operand - MUST BE LAST (used for array sizes)
} OpCode;
#define VM_MAX_OPCODE OP_HALT
#define VM_DISPATCH_TABLE_SIZE (VM_MAX_OPCODE + 1) // Important for computed goto table size

// Forward declaration
// typedef struct BytecodeChunk BytecodeChunk; // Already defined

// --- Bytecode, Compiler, and VM Functions ---

/**
 * @brief Initializes a BytecodeChunk, setting all counters to zero and pointers to NULL.
 * @param chunk Pointer to the BytecodeChunk to initialize.
 */
void init_bytecode_chunk(BytecodeChunk *chunk);

/**
 * @brief Frees all heap-allocated memory owned by a BytecodeChunk.
 *
 * This includes all duplicated strings in the string pool and any compiled
 * sub-chunks (e.g., for sigma expressions).
 *
 * @param chunk Pointer to the BytecodeChunk to free.
 */
void free_bytecode_chunk(BytecodeChunk *chunk);

/**
 * @brief Executes compiled bytecode using a Virtual Machine (VM).
 *
 * @param chunk The compiled BytecodeChunk to execute.
 * @param params A pointer to a VmParams struct containing runtime variables (x, frequency, mods, etc.).
 * @return The final floating-point result from the VM stack. Returns 0.0 on error.
 */
float execute_bytecode(BytecodeChunk *chunk, VmParams* params);

/**
 * @brief Compiles an Abstract Syntax Tree (AST) into a BytecodeChunk.
 *
 * @param node The root node of the AST to compile.
 * @param chunk A pointer to the BytecodeChunk to be filled with compiled code.
 * @return `true` on successful compilation, `false` otherwise.
 */
bool compile_ast_to_bytecode(Node *node, BytecodeChunk *chunk);

/**
 * @brief Disassembles and prints the contents of a BytecodeChunk for debugging.
 *
 * This includes the bytecode instructions, constant pool, string pool, and any sub-chunks.
 *
 * @param chunk The BytecodeChunk to disassemble.
 * @param name A descriptive name for the chunk to be printed in the output.
 */
void disassembleChunk(BytecodeChunk *chunk, const char *name);

static void advance_lfsr_state(VmParams *params);


// --- AST Node Management Forward Declarations ---
// Node *createNode(TokenType type); // Already defined
// void freeAST(Node *node); // Assume this exists and frees the AST tree // Already defined
// Node *parseExpression(Token *tokens, int *pos); // Already defined
// ... other parser forward declarations ...

// --- Tokenizer Forward Declaration ---
// int tokenize(const char *expression, Token *tokens, int maxTokens); // Already defined

// --- Bytecode Cache Implementation ---

// Structure for a cache entry (node in the hash table's linked lists)
typedef struct CacheEntry {
    char*              expression_string; // Key (owned by the cache entry)
    BytecodeChunk*     compiled_chunk;    // Value (owned by the cache entry)
    struct CacheEntry* next;              // For collision chaining
} CacheEntry;

// The Cache itself
#define CACHE_TABLE_SIZE 512 // Choose a reasonable size, maybe prime or power of 2

typedef struct {
    CacheEntry* table[CACHE_TABLE_SIZE];
    // Optional: Mutex for thread safety if needed
    // Optional: Stats (hits, misses, count)
    size_t count; // Number of items in cache
} BytecodeCache;

// Declare the global cache instance
static BytecodeCache bytecode_cache;
static bool cache_initialized = false;

// Simple string hash function (djb2)
static unsigned long hash_function(const char *str);

/**
 * @brief Initializes the global bytecode cache.
 *
 * Sets up the hash table and prepares the cache for use.
 * This must be called once before any caching functions are used.
 */
void initialize_bytecode_cache();

/**
 * @brief Frees all memory used by the global bytecode cache.
 *
 * This function iterates through the entire cache, freeing all stored
 * expression strings and their corresponding BytecodeChunks. It should be
 * called once at application shutdown.
 */
void free_bytecode_cache();

// Simple string hash function (djb2)
static unsigned long hash_function(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

// Initialize the cache (call once)
void initialize_bytecode_cache() {
    if (cache_initialized) return;
    for (int i = 0; i < CACHE_TABLE_SIZE; ++i) {
        bytecode_cache.table[i] = NULL;
    }
    bytecode_cache.count = 0;
    cache_initialized = true;
    printf("Bytecode cache initialized (Table size: %d).\n", CACHE_TABLE_SIZE);
}

// Free all resources used by the cache (call on shutdown)
void free_bytecode_cache() {
    if (!cache_initialized) return;
    printf("Freeing bytecode cache (%zu items)... ", bytecode_cache.count);
    size_t freed_count = 0;
    for (int i = 0; i < CACHE_TABLE_SIZE; ++i) {
        CacheEntry *entry = bytecode_cache.table[i];
        while (entry != NULL) {
            CacheEntry *next_entry = entry->next;

            // Free the data owned by the entry
            free(entry->expression_string);
            if (entry->compiled_chunk) {
                free_bytecode_chunk(entry->compiled_chunk);
                free(entry->compiled_chunk);
            }
            free(entry);
            entry = next_entry;
            freed_count++;
        }
        bytecode_cache.table[i] = NULL;
    }
    bytecode_cache.count = 0;
    cache_initialized = false; // Mark as uninitialized
    printf("Done. Freed %zu entries.\n", freed_count);
}

// Lookup an expression in the cache
static BytecodeChunk* lookup_cache(const char *expression) {
    if (!cache_initialized || !expression) return NULL;
    unsigned long hash = hash_function(expression);
    unsigned int index = hash % CACHE_TABLE_SIZE;
    CacheEntry *entry = bytecode_cache.table[index];
    while (entry != NULL) {
        if (strcmp(entry->expression_string, expression) == 0) {
            return entry->compiled_chunk;
        }
        entry = entry->next;
    }
    return NULL;
}

// Insert a newly compiled chunk into the cache
static bool insert_cache(const char *expression, BytecodeChunk *chunk_to_store) {
    if (!cache_initialized || !expression || !chunk_to_store) return false;

    // Check if it's already in the cache (shouldn't happen if used correctly, but safety)
    if (lookup_cache(expression) != NULL) {
         fprintf(stderr, "Warning: Attempted to insert duplicate expression '%s' into cache.\n", expression);
         free_bytecode_chunk(chunk_to_store); // Free the internal data
         free(chunk_to_store);                // Free the struct itself
         return false;
    }

    unsigned long hash = hash_function(expression);
    unsigned int index = hash % CACHE_TABLE_SIZE;

    CacheEntry *new_entry = (CacheEntry*)malloc(sizeof(CacheEntry));
    if (!new_entry) {
        perror("Failed to allocate cache entry");
        // The chunk_to_store pointer is now lost if insertion failed.
        // Need a better error handling strategy or pass ownership differently.
        // For now, assume caller handles freeing the chunk if insert_cache returns false.
        // Or, let insert_cache free the chunk on failure. Let's do the latter.
        free_bytecode_chunk(chunk_to_store); // Free the internal data
        free(chunk_to_store);                // Free the struct itself
        return false;
    }

    new_entry->expression_string = strdup(expression);
    if (!new_entry->expression_string) {
        perror("Failed to duplicate expression string for cache");
        free(new_entry);
        free_bytecode_chunk(chunk_to_store); free(chunk_to_store); // Free the chunk
        return false;
    }

    new_entry->compiled_chunk = chunk_to_store; // Store the pointer (cache takes ownership)
    new_entry->next = bytecode_cache.table[index];
    bytecode_cache.table[index] = new_entry;
    bytecode_cache.count++;

    // printf("Cached bytecode for: %s\n", expression); // Debug
    return true;
}

// Forward declaration for recursive compilation helper
static bool compile_node(Node *node, BytecodeChunk *chunk, const char** active_loop_var_name_ptr);

// --- Compiler Helper Functions ---

static void emit_byte(BytecodeChunk *chunk, uint8_t byte) {
    if (chunk->code_count >= MAX_BYTECODE_SIZE) {
        fprintf(stderr, "Compile Error: Bytecode buffer overflow.\n");
        // Handle error - maybe return bool or set an error flag
        return; // Or exit? For now, just stop emitting.
    }
    chunk->code[chunk->code_count++] = byte;
}

static void emit_bytes(BytecodeChunk *chunk, uint8_t byte1, uint8_t byte2) {
    emit_byte(chunk, byte1);
    emit_byte(chunk, byte2);
}

// Emit OpCode + 16-bit operand (e.g., constant index, jump offset)
static void emit_short(BytecodeChunk *chunk, uint16_t value) {
    emit_byte(chunk, (value >> 8) & 0xFF); // High byte
    emit_byte(chunk, value & 0xFF);        // Low byte
}

// Adds a constant to the pool, returns its index. Handles duplicates.
static uint16_t add_constant(BytecodeChunk *chunk, float value) {
    // Check for existing constant to avoid duplicates
    for (int i = 0; i < chunk->constants_count; ++i) {
        if (fabsf(chunk->constants[i] - value) < 1e-9f) { // Compare floats carefully
            return (uint16_t)i;
        }
    }
    // Add new constant if pool not full
    if (chunk->constants_count >= MAX_CONSTANTS) {
        fprintf(stderr, "Compile Error: Constant pool overflow.\n");
        return 0; // Return index 0 on error? Or a special value?
    }
    chunk->constants[chunk->constants_count] = value;
    return (uint16_t)chunk->constants_count++;
}

// Adds a copy of a string to the pool, returns its index. Handles duplicates.
// Returns index (0-255) or potentially MAX_STRINGS or similar on error.
static uint8_t add_string(BytecodeChunk *chunk, const char *string) {
    if (!string) return 0; // Or handle error appropriately

    // Check for existing string to avoid duplicates
    for (int i = 0; i < chunk->strings_count; ++i) {
        if (chunk->strings[i] && strcmp(chunk->strings[i], string) == 0) {
            return (uint8_t)i; // Found existing, return its index
        }
    }

    // Add new string if pool not full
    if (chunk->strings_count >= MAX_STRINGS) {
        fprintf(stderr, "Compile Error: String pool overflow (max %d).\n", MAX_STRINGS);
        return 0; // Return index 0 on error? Requires careful handling in VM
                  // Maybe return UINT8_MAX to signal error? Let's use 0 for now.
    }

    // Duplicate the string - the chunk now owns this memory
    chunk->strings[chunk->strings_count] = strdup(string);
    if (!chunk->strings[chunk->strings_count]) {
         perror("Compile Error: Failed to duplicate string for pool");
         // Handle memory allocation failure - maybe return error index?
         return 0; // Return 0 on error
    }

    return (uint8_t)chunk->strings_count++;
}

// Emits a jump instruction with a placeholder offset, returns instruction index
static int emit_jump(BytecodeChunk *chunk, OpCode jump_type) {
    emit_byte(chunk, jump_type);
    emit_byte(chunk, 0xFF); // Placeholder bytes for offset
    emit_byte(chunk, 0xFF);
    return chunk->code_count - 2; // Return index of the first offset byte
}

// Patches a previously emitted jump instruction
static void patch_jump(BytecodeChunk *chunk, int jump_instruction_index) {
    // Calculate offset: target (current position) - jump instruction position - 2 bytes for offset itself
    int16_t offset = (int16_t)(chunk->code_count - (jump_instruction_index + 2));

    if (offset > INT16_MAX || offset < INT16_MIN) {
         fprintf(stderr, "Compile Error: Jump offset too large (%d).\n", offset);
         // Handle error
         return;
    }

    chunk->code[jump_instruction_index] = (offset >> 8) & 0xFF; // High byte
    chunk->code[jump_instruction_index + 1] = offset & 0xFF;    // Low byte
}

// Adds a sub-chunk (e.g., for sigma) and returns its index
static uint16_t add_sigma_sub_chunk(BytecodeChunk *chunk, BytecodeChunk* sub_chunk) {
    if (chunk->sigma_sub_chunk_count >= MAX_SIGMA_CHUNKS) {
        fprintf(stderr, "Compile Error: Sigma sub-chunk pool overflow.\n");
        free_bytecode_chunk(sub_chunk); // Free the unused sub-chunk
        return UINT16_MAX; // Indicate error
    }
    chunk->sigma_sub_chunks[chunk->sigma_sub_chunk_count] = sub_chunk;
    return (uint16_t)chunk->sigma_sub_chunk_count++;
}

// --- Main Recursive Compilation Function ---
// active_loop_var_name_ptr helps resolve loop variables inside sigma body
static bool compile_node(Node *node, BytecodeChunk *chunk, const char** active_loop_var_name_ptr) {
    if (!node) return false; // Should not happen with valid AST
    bool ok = true;

    switch (node->type) {
        case TOKEN_NUMBER: {
            uint16_t const_idx = add_constant(chunk, node->data.number);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, const_idx);
            break;
        }
        case TOKEN_VARIABLE: { // 'x' or loop variable 'k', 'n'
            if (active_loop_var_name_ptr && *active_loop_var_name_ptr &&
                strcmp(node->data.name, *active_loop_var_name_ptr) == 0)
            {
                uint8_t name_index = add_string(chunk, node->data.name);
                emit_byte(chunk, OP_PUSH_LOOP_VAR);
                emit_byte(chunk, name_index);
            }
            else if (strcmp(node->data.name, "x") == 0)
            {
                emit_byte(chunk, OP_PUSH_VAR_X);
            }
            // REMOVE the else-if checks for MOD_A, MOD_B, RAND_OFFSET here
            else {
                fprintf(stderr, "Compile Error: Unknown or out-of-scope variable '%s'.\n", node->data.name);
                ok = false;
            }
            break;
        }
        case TOKEN_FREQUENCY: {
            emit_byte(chunk, OP_PUSH_VAR_FREQ);
            break;
        }
        case TOKEN_RAND_OFFSET: {
            emit_byte(chunk, OP_PUSH_VAR_RAND);
            break;
        }
        case TOKEN_MOD_A: {
            emit_byte(chunk, OP_PUSH_VAR_MOD_A);
            break;
        }
        case TOKEN_MOD_B: {
            emit_byte(chunk, OP_PUSH_VAR_MOD_B);
            break;
        }
        case TOKEN_MOD_C: {
            emit_byte(chunk, OP_PUSH_VAR_MOD_C);
            break;
        }
        case TOKEN_CONSTANT: { // PI, E, LFSR constants, etc.
            float const_val = 0.0f;
            if (strcmp(node->data.name, "PI") == 0) const_val = C_PI;
            else if (strcmp(node->data.name, "E") == 0) const_val = C_E;
            else if (strcmp(node->data.name, "TWO_PI") == 0) const_val = C_TWO_PI;
            else if (strcmp(node->data.name, "PI_OVER_2") == 0) const_val = C_PI_OVER_2;
            else if (strcmp(node->data.name, "THREE_PI_OVER_2") == 0) const_val = C_THREE_PI_OVER_2;
            // LFSR Constants
            else if (strcmp(node->data.name, "LFSR_4BIT") == 0) const_val = (float)LFSR_4BIT;
            else if (strcmp(node->data.name, "LFSR_5BIT") == 0) const_val = (float)LFSR_5BIT;
            else if (strcmp(node->data.name, "LFSR_6BIT") == 0) const_val = (float)LFSR_6BIT;
            else if (strcmp(node->data.name, "LFSR_7BIT") == 0) const_val = (float)LFSR_7BIT;
            else if (strcmp(node->data.name, "LFSR_8BIT") == 0) const_val = (float)LFSR_8BIT;
            else if (strcmp(node->data.name, "LFSR_9BIT") == 0) const_val = (float)LFSR_9BIT;
            else if (strcmp(node->data.name, "LFSR_10BIT") == 0) const_val = (float)LFSR_10BIT;
            else if (strcmp(node->data.name, "LFSR_11BIT") == 0) const_val = (float)LFSR_11BIT;
            else if (strcmp(node->data.name, "LFSR_12BIT") == 0) const_val = (float)LFSR_12BIT;
            else if (strcmp(node->data.name, "LFSR_13BIT") == 0) const_val = (float)LFSR_13BIT;
            else if (strcmp(node->data.name, "LFSR_14BIT") == 0) const_val = (float)LFSR_14BIT;
            else if (strcmp(node->data.name, "LFSR_15BIT") == 0) const_val = (float)LFSR_15BIT;
            else if (strcmp(node->data.name, "LFSR_16BIT") == 0) const_val = (float)LFSR_16BIT;
            else if (strcmp(node->data.name, "LFSR_17BIT") == 0) const_val = (float)LFSR_17BIT;
            else if (strcmp(node->data.name, "LFSR_GALOIS") == 0) const_val = (float)LFSR_GALOIS;
            else if (strcmp(node->data.name, "LFSR_FIBONACCI") == 0) const_val = (float)LFSR_FIBONACCI;
            else { fprintf(stderr, "Compile Error: Unknown constant '%s'.\n", node->data.name); ok = false; break; }

            uint16_t const_idx = add_constant(chunk, const_val);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, const_idx);
            break;
        }
        case TOKEN_UNARY_OP: {
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
            if (ok) {
                switch (node->data.unary_op) {
                    case '-': emit_byte(chunk, OP_NEGATE); break;
                    case '!': emit_byte(chunk, OP_NOT); break;
                    default: fprintf(stderr, "Compile Error: Unknown unary op '%c'.\n", node->data.unary_op); ok = false; break;
                }
            }
            break;
        }
        case TOKEN_OPERATOR: { // Binary + - * / %
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr); // Left operand
            ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr); // Right operand
            if (ok) {
                switch (node->data.op_str[0]) {
                    case '+': emit_byte(chunk, OP_ADD); break;
                    case '-': emit_byte(chunk, OP_SUB); break;
                    case '*': emit_byte(chunk, OP_MUL); break;
                    case '/': emit_byte(chunk, OP_DIV); break;
                    case '%': emit_byte(chunk, OP_MOD); break;
                    default: fprintf(stderr, "Compile Error: Unknown binary op '%s'.\n", node->data.op_str); ok = false; break;
                }
            }
            break;
        }
        case TOKEN_COMPARISON: { // < > <= >= == !=
             ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
             ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr);
             if (ok) {
                OpCode op;
                if (strcmp(node->data.op_str, "<") == 0) op = OP_CMP_LT;
                else if (strcmp(node->data.op_str, ">") == 0) op = OP_CMP_GT;
                else if (strcmp(node->data.op_str, "<=") == 0) op = OP_CMP_LE;
                else if (strcmp(node->data.op_str, ">=") == 0) op = OP_CMP_GE;
                else if (strcmp(node->data.op_str, "==") == 0) op = OP_CMP_EQ;
                else if (strcmp(node->data.op_str, "!=") == 0) op = OP_CMP_NE;
                else { fprintf(stderr, "Compile Error: Unknown comparison op '%s'.\n", node->data.op_str); ok = false; break; }
                emit_byte(chunk, op);
             }
             break;
        }
        case TOKEN_LOGICAL_AND: {
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            int jump_if_false = emit_jump(chunk, OP_JUMP_IF_FALSE);
            ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            int jump_over_false_push = emit_jump(chunk, OP_JUMP);
            patch_jump(chunk, jump_if_false);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, add_constant(chunk, 0.0f));
            patch_jump(chunk, jump_over_false_push);
            break;
        }

        case TOKEN_LOGICAL_OR: {
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            int jump_if_false = emit_jump(chunk, OP_JUMP_IF_FALSE);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, add_constant(chunk, 1.0f));
            int jump_over_right = emit_jump(chunk, OP_JUMP);
            patch_jump(chunk, jump_if_false);
            ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            patch_jump(chunk, jump_over_right);
            break;
        }
        case TOKEN_LOGICAL_XOR: { // Logical XOR (A ^ B) -> (A_bool != B_bool)
            // Compile A
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            // Convert A to A_bool (stack: [A_val])
            uint16_t zero_const_idx = add_constant(chunk, 0.0f);
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, zero_const_idx); // stack: [A_val, 0.0]
            emit_byte(chunk, OP_CMP_NE);       // stack: [A_bool] (A_val != 0.0 -> 1.0, else 0.0)

            // Compile B
            ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            // Convert B to B_bool (stack: [A_bool, B_val])
            // zero_const_idx is still valid from previous add_constant call
            emit_byte(chunk, OP_PUSH_CONST);
            emit_short(chunk, zero_const_idx); // stack: [A_bool, B_val, 0.0]
            emit_byte(chunk, OP_CMP_NE);       // stack: [A_bool, B_bool] (B_val != 0.0 -> 1.0, else 0.0)

            // Compare A_bool and B_bool using OP_CMP_NE (A_bool != B_bool)
            emit_byte(chunk, OP_CMP_NE);       // stack: [result] (1.0 if XOR is true, 0.0 if false)
            break;
        }

        case TOKEN_TERNARY_QM: {
            ok &= compile_node(node->child1, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            int jump_if_false = emit_jump(chunk, OP_JUMP_IF_FALSE);
            ok &= compile_node(node->child2, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            int jump_over_false = emit_jump(chunk, OP_JUMP);
            patch_jump(chunk, jump_if_false);
            ok &= compile_node(node->child3, chunk, active_loop_var_name_ptr);
            if (!ok) break;
            patch_jump(chunk, jump_over_false);
            break;
        }

        case TOKEN_FUNCTION: {
            const char* fname = node->data.name;

            if (strcmp(fname, "sigma") == 0) {
                // --- Sigma Compilation ---
                if (active_loop_var_name_ptr && *active_loop_var_name_ptr) {
                    fprintf(stderr, "Compile Error: Nested sigma() is not supported.\n");
                    return false;
                }

                Node *var_node = node->child1;
                Node *start_node = var_node ? var_node->right : NULL;
                Node *end_node = start_node ? start_node->right : NULL;
                Node *step_node = end_node ? end_node->right : NULL;
                Node *body_node = step_node ? step_node->right : NULL;

                if (!var_node || var_node->type != TOKEN_VARIABLE || !start_node || !end_node || !step_node || !body_node) {
                    fprintf(stderr, "Compile Error (sigma): Incorrect AST structure for sigma.\n");
                    return false;
                }
                const char* loop_var_name = var_node->data.name; // e.g., "k"

                // Compile start, end, step, body into *separate* chunks
                BytecodeChunk *start_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
                BytecodeChunk *end_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
                BytecodeChunk *step_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
                BytecodeChunk *body_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));

                if (!start_chunk || !end_chunk || !step_chunk || !body_chunk) {
                    fprintf(stderr, "Compile Error (sigma): Failed to allocate memory for sub-chunks.\n");
                    free(start_chunk); free(end_chunk); free(step_chunk); free(body_chunk);
                    return false;
                }

                init_bytecode_chunk(start_chunk);
                init_bytecode_chunk(end_chunk);
                init_bytecode_chunk(step_chunk);
                init_bytecode_chunk(body_chunk);

                // Compile sub-expressions. Pass NULL for active_loop_var_name_ptr for start/end/step.
                bool start_ok = compile_node(start_node, start_chunk, NULL);
                if (start_ok) emit_byte(start_chunk, OP_HALT); // End sub-chunk
                bool end_ok = compile_node(end_node, end_chunk, NULL);
                 if (end_ok) emit_byte(end_chunk, OP_HALT);
                bool step_ok = compile_node(step_node, step_chunk, NULL);
                 if (step_ok) emit_byte(step_chunk, OP_HALT);

                // IMPORTANT: Compile the body with the loop variable name active
                bool body_ok = compile_node(body_node, body_chunk, &loop_var_name);
                 if (body_ok) emit_byte(body_chunk, OP_HALT);

                if (!start_ok || !end_ok || !step_ok || !body_ok) {
                    fprintf(stderr, "Compile Error (sigma): Failed to compile one or more sub-expressions.\n");
                    free_bytecode_chunk(start_chunk); free(start_chunk);
                    free_bytecode_chunk(end_chunk); free(end_chunk);
                    free_bytecode_chunk(step_chunk); free(step_chunk);
                    free_bytecode_chunk(body_chunk); free(body_chunk);
                    return false;
                }

                // Add sub-chunks to the main chunk's storage
                uint16_t start_idx = add_sigma_sub_chunk(chunk, start_chunk);
                uint16_t end_idx = add_sigma_sub_chunk(chunk, end_chunk);
                uint16_t step_idx = add_sigma_sub_chunk(chunk, step_chunk);
                uint16_t body_idx = add_sigma_sub_chunk(chunk, body_chunk);

                if (start_idx == UINT16_MAX || end_idx == UINT16_MAX || step_idx == UINT16_MAX || body_idx == UINT16_MAX) {
                     // Error adding sub-chunk (already printed), cleanup handled by add_sigma_sub_chunk which calls free_bytecode_chunk on the sub_chunk, so we don't need to free them again here.
                     return false;
                }

                uint8_t name_index = add_string(chunk, loop_var_name);

                emit_byte(chunk, OP_SIGMA_EXEC);
                emit_byte(chunk, name_index); // Operand 0: loop var name index
                emit_short(chunk, start_idx); // Operand 1+2: start chunk index
                emit_short(chunk, end_idx);   // Operand 3+4: end chunk index
                emit_short(chunk, step_idx);  // Operand 5+6: step chunk index
                emit_short(chunk, body_idx);  // Operand 7+8: body chunk index

            } else {
                // --- Standard Function Call Compilation ---
                // Find function ID and arity
                FunctionID func_id = FUNC_ID_COUNT; // Invalid default
                int expected_arity = -1;
                for (int i = 0; vm_functions[i].name; ++i) {
                    if (strcmp(fname, vm_functions[i].name) == 0) {
                        func_id = vm_functions[i].id;
                        expected_arity = vm_functions[i].arity;
                        break;
                    }
                }

                if (func_id == FUNC_ID_COUNT) {
                    fprintf(stderr, "Compile Error: Function '%s' not supported by VM.\n", fname);
                    ok = false; break;
                }

                // Compile arguments (in order they appear)
                int arg_count = 0;
                Node *currentArg = node->child1;
                while(currentArg) {
                    ok &= compile_node(currentArg, chunk, active_loop_var_name_ptr);
                    if (!ok) break; // Stop if argument compilation fails
                    arg_count++;
                    currentArg = currentArg->right;
                }
                if (!ok) break; // Propagate error

                // Check arity
                if (expected_arity != -1 && arg_count != expected_arity) {
                     fprintf(stderr, "Compile Error: Function '%s' expects %d arguments, got %d.\n", fname, expected_arity, arg_count);
                     ok = false; break;
                }

                // Emit CALL instruction
                emit_byte(chunk, OP_CALL);
                emit_byte(chunk, (uint8_t)func_id);
                emit_byte(chunk, (uint8_t)arg_count);
            }
            break; // End case TOKEN_FUNCTION
        } // End switch block for TOKEN_FUNCTION

        default:
            fprintf(stderr, "Compile Error: Unknown AST node type %d encountered.\n", node->type);
            ok = false;
            break;
    }
    return ok;
}

// --- Public Compiler Interface ---

// Helper to get OpCode name (you'd need to create this array)
const char* getOpCodeName(OpCode code) {
    static const char* opCodeNames[] = {
        /* 0x00 */ "OP_PUSH_CONST",
        /* 0x01 */ "OP_PUSH_VAR_X",
        /* 0x02 */ "OP_PUSH_VAR_FREQ",
        /* 0x03 */ "OP_PUSH_VAR_RAND",
        /* 0x04 */ "OP_PUSH_VAR_MOD_A",
        /* 0x05 */ "OP_PUSH_VAR_MOD_B",
        /* 0x06 */ "OP_PUSH_VAR_MOD_C",
        /* 0x07 */ "OP_POP",
        /* 0x08 */ "OP_ADD",
        /* 0x09 */ "OP_SUB",
        /* 0x0A */ "OP_MUL",
        /* 0x0B */ "OP_DIV",
        /* 0x0C */ "OP_MOD",
        /* 0x0D */ "OP_NEGATE",
        /* 0x0E */ "OP_NOT",
        /* 0x0F */ "OP_CMP_EQ",
        /* 0x10 */ "OP_CMP_NE",
        /* 0x11 */ "OP_CMP_GT",
        /* 0x12 */ "OP_CMP_GE",
        /* 0x13 */ "OP_CMP_LT",
        /* 0x14 */ "OP_CMP_LE",
        /* 0x15 */ "OP_JUMP",
        /* 0x16 */ "OP_JUMP_IF_FALSE",
        /* 0x17 */ "OP_CALL",
        /* 0x18 */ "OP_SIGMA_SETUP",
        /* 0x19 */ "OP_PUSH_LOOP_VAR",
        /* 0x1A */ "OP_SIGMA_EXEC",
        /* 0x1B */ "OP_HALT"
    };
    // Basic bounds check using VM_MAX_OPCODE
    if (code >= 0 && code <= VM_MAX_OPCODE) {
        return opCodeNames[code];
    }
    return "OP_UNKNOWN";
}

// Function to disassemble a single instruction at a given offset
// Returns the number of bytes consumed by the instruction (including operands)
int disassembleInstruction(BytecodeChunk *chunk, int offset) {
    OpCode instruction = (OpCode)chunk->code[offset];
    printf("%04d ", offset); // Print current offset

    // Simple lookup and print OpCode name
    printf("%-20s", getOpCodeName(instruction));

    int instruction_size = 1; // Start with size of opcode itself

    // Handle operands based on the instruction
    switch (instruction) {
        case OP_PUSH_CONST: {
            uint16_t const_idx = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
            printf("%4u (", const_idx);
            if (const_idx < chunk->constants_count) {
                printf("%.4f", chunk->constants[const_idx]);
            } else {
                printf("INVALID_IDX");
            }
            printf(")");
            instruction_size += 2; // Opcode + 2 bytes index
            break;
        }
        case OP_JUMP:
        case OP_JUMP_IF_FALSE: {
            int16_t jump_offset = (int16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
            int target_offset = offset + 3 + jump_offset; // Offset relative to *next* instruction
            printf("%4d (target: %04d)", jump_offset, target_offset);
            instruction_size += 2; // Opcode + 2 bytes offset
            break;
        }
        case OP_CALL: {
            uint8_t func_id = chunk->code[offset + 1];
            uint8_t arg_count = chunk->code[offset + 2];
            // Optionally lookup function name from func_id if needed
            printf("id:%u args:%u", func_id, arg_count);
            instruction_size += 2; // Opcode + func_id + arg_count
            break;
        }
        case OP_PUSH_LOOP_VAR: {
             uint8_t name_index = chunk->code[offset + 1];
             printf("idx:%u (", name_index);
             if (name_index < chunk->strings_count && chunk->strings[name_index] != NULL) {
                 printf("'%s'", chunk->strings[name_index]);
             } else {
                 printf("INVALID_IDX");
             }
             printf(")");
             instruction_size += 1; // Opcode + index
             break;
        }
         case OP_SIGMA_EXEC: {
             uint8_t name_index = chunk->code[offset + 1];
             uint16_t start_idx = (uint16_t)((chunk->code[offset + 2] << 8) | chunk->code[offset + 3]);
             uint16_t end_idx   = (uint16_t)((chunk->code[offset + 4] << 8) | chunk->code[offset + 5]);
             uint16_t step_idx  = (uint16_t)((chunk->code[offset + 6] << 8) | chunk->code[offset + 7]);
             uint16_t body_idx  = (uint16_t)((chunk->code[offset + 8] << 8) | chunk->code[offset + 9]);
             printf("name_idx:%u start:%u end:%u step:%u body:%u",
                    name_index, start_idx, end_idx, step_idx, body_idx);
             // Optionally lookup name: chunk->strings[name_index]
             instruction_size += 9; // Opcode + name_idx + 4*short_idx
             break;
         }

        // Opcodes with no operands:
        case OP_PUSH_VAR_X:
        case OP_PUSH_VAR_FREQ:
        case OP_PUSH_VAR_RAND:
        case OP_PUSH_VAR_MOD_A: case OP_PUSH_VAR_MOD_B: case OP_PUSH_VAR_MOD_C:
        case OP_POP:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_NEGATE: case OP_NOT:
        case OP_CMP_EQ: case OP_CMP_NE: case OP_CMP_GT: case OP_CMP_GE: case OP_CMP_LT: case OP_CMP_LE:
        case OP_SIGMA_SETUP: // Has operands, but getOpCodeName handles it, so no specific print needed here for operands if disassembleInstruction logic is followed correctly for size
        case OP_HALT:
            // No operands, size is 1, or operands handled by get_instruction_size for OP_SIGMA_SETUP
            if (instruction == OP_SIGMA_SETUP) instruction_size += 1 + 2+2+2+2; // name_id + 4*ushort
            break;


        default:
            printf("UNKNOWN OPCODE");
            break; // Size remains 1, will likely cause issues on next iteration
    }
    printf("\n");
    return instruction_size;
}

// Function to disassemble an entire chunk
void disassembleChunk(BytecodeChunk *chunk, const char *name) {
    printf("--- Disassembly: %s ---\n", name);
    if (!chunk) {
        printf("  (NULL Chunk)\n");
        return;
    }
    int offset = 0;
    while (offset < chunk->code_count) { // Use while loop and update offset by instruction size
        offset += disassembleInstruction(chunk, offset);
    }

    // Optional: Print constant pool
    printf("  Constant Pool (%d entries):\n", chunk->constants_count);
    for(int i = 0; i < chunk->constants_count; ++i) {
        printf("    %04d: %.4f\n", i, chunk->constants[i]);
    }
    // Optional: Print string pool
    printf("  String Pool (%d entries):\n", chunk->strings_count);
     for(int i = 0; i < chunk->strings_count; ++i) {
         printf("    %04d: '%s'\n", i, chunk->strings[i] ? chunk->strings[i] : "<NULL>");
     }
    // Optional: Recursively disassemble sub-chunks
    if (chunk->sigma_sub_chunk_count > 0) {
        printf("  Sub-Chunks (%d entries):\n", chunk->sigma_sub_chunk_count);
        for (int i = 0; i < chunk->sigma_sub_chunk_count; ++i) {
            char sub_name[100];
            snprintf(sub_name, sizeof(sub_name), "%s (Sub-Chunk %d)", name, i);
            disassembleChunk(chunk->sigma_sub_chunks[i], sub_name);
        }
    }
    printf("--- End Disassembly: %s ---\n", name);
}

void init_bytecode_chunk(BytecodeChunk *chunk) {
    chunk->code_count = 0;
    chunk->constants_count = 0;
    chunk->strings_count = 0;
    // Initialize string pointers to NULL (important for freeing later)
    for (int i = 0; i < MAX_STRINGS; ++i) {
        chunk->strings[i] = NULL;
    }
    chunk->sigma_sub_chunk_count = 0;
    for (int i = 0; i < MAX_SIGMA_CHUNKS; ++i) {
        chunk->sigma_sub_chunks[i] = NULL;
    }
}

// Free a chunk's internal data, including sub-chunks it owns and duplicated strings
void free_bytecode_chunk(BytecodeChunk *chunk) {
    if (!chunk) return;

    // Free sub-chunks recursively
    for (int i = 0; i < chunk->sigma_sub_chunk_count; ++i) {
        if (chunk->sigma_sub_chunks[i]) {
            free_bytecode_chunk(chunk->sigma_sub_chunks[i]); // Recursive free of internal data
            free(chunk->sigma_sub_chunks[i]);                // Free the sub-chunk struct itself
            chunk->sigma_sub_chunks[i] = NULL;
        }
    }

    // Free duplicated strings owned by the pool ---
    for (int i = 0; i < chunk->strings_count; ++i) {
        free(chunk->strings[i]); // Free the strdup'd string
        chunk->strings[i] = NULL;
    }

    // Reset counts, data will be overwritten on reuse or struct freed externally
    chunk->code_count = 0;
    chunk->constants_count = 0;
    chunk->strings_count = 0; // Reset string count
    chunk->sigma_sub_chunk_count = 0;
}

// Compiles the AST into the provided chunk. Returns true on success.
bool compile_ast_to_bytecode(Node *node, BytecodeChunk *chunk) {
    if (!node || !chunk) return false;
    init_bytecode_chunk(chunk); // Initialize/reset the chunk

    bool success = compile_node(node, chunk, NULL); // Start with no active loop variable

    if (success) {
        emit_byte(chunk, OP_HALT); // Add final instruction
    } else {
        // Cleanup potentially partially filled chunk? For now, the caller should probably discard it on failure.
        fprintf(stderr, "Compilation failed.\n");
        free_bytecode_chunk(chunk); // Free sub-chunks if any were created before failure
    }

    return success;
}

// --- LFSR Implementation ---

const char* lfsr_type_to_string(LfsrType type) {
    switch(type) {
        case LFSR_4BIT: return "LFSR_4BIT";
        case LFSR_5BIT: return "LFSR_5BIT";
        case LFSR_6BIT: return "LFSR_6BIT";
        case LFSR_7BIT: return "LFSR_7BIT";
        case LFSR_8BIT: return "LFSR_8BIT";
        case LFSR_9BIT: return "LFSR_9BIT";
        case LFSR_10BIT: return "LFSR_10BIT";
        case LFSR_11BIT: return "LFSR_11BIT";
        case LFSR_12BIT: return "LFSR_12BIT";
        case LFSR_13BIT: return "LFSR_13BIT";
        case LFSR_14BIT: return "LFSR_14BIT";
        case LFSR_15BIT: return "LFSR_15BIT";
        case LFSR_16BIT: return "LFSR_16BIT";
        case LFSR_17BIT: return "LFSR_17BIT";
        case LFSR_GALOIS: return "LFSR_GALOIS";
        case LFSR_FIBONACCI: return "LFSR_FIBONACCI";
        default: return "UNKNOWN_LFSR_TYPE_VALUE";
    }
}

// LFSR Pre-computed Table Structure
typedef struct {
    LfsrType type;
    uint32_t period;        // Sequence length - NEW, changed to uint32_t
    uint32_t polynomial;    // Feedback polynomial
    uint8_t* bit_table;     // Packed bit sequence
    bool initialized;       // Table ready flag
} LfsrPrecomputedTable;

// Global LFSR Tables
LfsrPrecomputedTable precomputed_lfsrs[NUM_LFSR_TYPES];
static bool lfsr_tables_initialized = false;

typedef struct {
    LfsrType type_enum;
    int bit_length;
    uint32_t tap_mask; // Represents taps to XOR for Fibonacci's MSB input (0-indexed bits)
    uint32_t period;
    uint32_t seed;     // Initial state, typically 1 or non-zero
} LfsrConfigEntry;

// LFSR Configuration Data (Moved to file scope)
static const LfsrConfigEntry lfsr_configs[NUM_LFSR_TYPES] = {
    // POKEY Standard Polynomials where available, others are common maximal-length
    {LFSR_4BIT,   4, (1 << 0) | (1 << 1),         15, 1},
    {LFSR_5BIT,   5, (1 << 0) | (1 << 2),         31, 1},
    {LFSR_6BIT,   6, (1 << 0) | (1 << 1),         63, 1},
    {LFSR_7BIT,   7, (1 << 0) | (1 << 3),        127, 1},
    {LFSR_8BIT,   8, (1<<0)|(1<<2)|(1<<3)|(1<<4), 255, 1},
    {LFSR_9BIT,   9, (1 << 0) | (1 << 4),        511, 1},
    {LFSR_10BIT, 10, (1 << 0) | (1 << 3),       1023, 1},
    {LFSR_11BIT, 11, (1 << 0) | (1 << 2),       2047, 1},
    {LFSR_12BIT, 12, (1<<0)|(1<<1)|(1<<4)|(1<<6),4095,1},
    {LFSR_13BIT, 13, (1<<0)|(1<<1)|(1<<3)|(1<<4),8191,1},
    {LFSR_14BIT, 14, (1<<0)|(1<<1)|(1<<2)|(1<<12),16383,1},
    {LFSR_15BIT, 15, (1 << 0) | (1 << 1),      32767, 1},
    {LFSR_16BIT, 16, (1<<0)|(1<<2)|(1<<3)|(1<<5),65535,1},
    {LFSR_17BIT, 17, (1 << 0) | (1 << 14),    131071, 1}, // For x^17 + x^3 + 1 (standard is x^17+x^14+1 or x^17+x^5+1, using x^14 from user comment for consistency with original)

    {LFSR_GALOIS,    16, (1<<0)|(1<<2)|(1<<3)|(1<<5), 65535, 1}, // Same as 16BIT, example for distinct type
    {LFSR_FIBONACCI, 16, (1<<0)|(1<<2)|(1<<3)|(1<<5), 65535, 1}  // Same as 16BIT, example for distinct type
};


// Initialize LFSR tables
void px_vm_init_lfsr_tables(void) {
    if (lfsr_tables_initialized) return;
    printf("Initializing LFSR tables...\n");

    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        // Use the file-scope lfsr_configs
        const LfsrConfigEntry* current_config = &lfsr_configs[i];
        LfsrPrecomputedTable* table = &precomputed_lfsrs[current_config->type_enum];

        if (current_config->type_enum != (LfsrType)i) {
             fprintf(stderr, "Critical Error: LFSR config array order mismatch for type %d (enum %d expected %d). Aborting init.\n",
                     i, current_config->type_enum, i);
             for(int j=0; j<NUM_LFSR_TYPES; ++j) {
                 precomputed_lfsrs[j].initialized = false;
                 precomputed_lfsrs[j].bit_table = NULL;
             }
             return;
        }

        table->type = current_config->type_enum;
        table->period = current_config->period;
        table->polynomial = current_config->tap_mask; // Store the tap_mask as polynomial
        table->initialized = false;

        int bit_length = current_config->bit_length;
        // uint32_t generation_tap_mask = current_config->tap_mask; // For clarity if needed

        if (table->period == 0) {
            fprintf(stderr, "  LFSR type %s (enum %d) has zero period, skipping allocation.\n", lfsr_type_to_string(table->type), table->type);
            table->bit_table = NULL;
            continue;
        }
        size_t table_bytes = LFSR_TABLE_BYTES(table->period);
        table->bit_table = (uint8_t*)calloc(table_bytes, 1);

        if (!table->bit_table) {
            fprintf(stderr, "Failed to allocate LFSR table for type %s (enum %d)\n", lfsr_type_to_string(table->type), table->type);
            continue;
        }

        uint32_t lfsr_state = current_config->seed;
        if (lfsr_state == 0 && table->period > 0) {
            fprintf(stderr, "Warning: LFSR type %s (enum %d) seed is 0, using 1 instead for generation.\n", lfsr_type_to_string(table->type), table->type);
            lfsr_state = 1;
        }
        uint32_t lfsr_full_mask = (bit_length == 32) ? UINT32_MAX : ((1UL << bit_length) - 1);
        uint32_t current_tap_mask_for_gen = current_config->tap_mask;


        printf("  Generating LFSR type %s (enum %d, bits=%d, period=%u, tap_mask=0x%08X, seed=0x%X)...\n",
               lfsr_type_to_string(table->type), table->type, bit_length, table->period, current_tap_mask_for_gen, current_config->seed);


        for (uint32_t step = 0; step < table->period; step++) {
            uint8_t output_bit = lfsr_state & 1; // LSB is output for Fibonacci
            if (output_bit) {
                size_t byte_idx = step / 8;
                int bit_idx_in_byte = step % 8;
                table->bit_table[byte_idx] |= (1 << bit_idx_in_byte);
            }

            uint8_t feedback_bit = 0;
            uint32_t temp_state_for_tapping = lfsr_state;
            for (int k = 0; k < bit_length; k++) {
                if ((current_tap_mask_for_gen >> k) & 1) {
                    feedback_bit ^= (temp_state_for_tapping >> k) & 1;
                }
            }
            lfsr_state = (lfsr_state >> 1) | ( (uint32_t)feedback_bit << (bit_length - 1) );
            lfsr_state &= lfsr_full_mask;
        }
        table->initialized = true;
        uint32_t expected_final_seed = current_config->seed;
        if (expected_final_seed == 0 && table->period > 0) expected_final_seed = 1;

        if (lfsr_state != expected_final_seed && table->period > 0) {
             printf("    WARNING: LFSR type %s (enum %d) did not return to effective seed! Final: 0x%X, Expected Seed: 0x%X (Original User Seed: 0x%X)\n",
                    lfsr_type_to_string(table->type), table->type, lfsr_state, expected_final_seed, current_config->seed);
        }

    // Debug prints from original code
    printf("Initializing LFSR Type: %s (Enum val: %d, Config Index: %d)\n",
       lfsr_type_to_string(current_config->type_enum), current_config->type_enum, i);
    printf("  Config: bit_length=%d, tap_mask=0x%X, period=%u, seed=%u\n",
       current_config->bit_length, current_config->tap_mask,
       current_config->period, current_config->seed);

    int effective_bit_length_for_generation = current_config->bit_length;
    uint32_t effective_period_for_generation_loop = table->period;

    printf("  Effective for gen: bit_length=%d, period_loop_limit=%u\n",
       effective_bit_length_for_generation, effective_period_for_generation_loop);
    printf("  Stored in table: period=%u. Bit table allocated: %s\n",
       table->period, table->bit_table ? "Yes" : "No");
    if (table->bit_table && table->period > 0) {
        printf("  First 4 bytes of table: 0x%02X 0x%02X 0x%02X 0x%02X\n",
           table->bit_table[0],
           table->period > 8 ? table->bit_table[1] : 0,
           table->period > 16 ? table->bit_table[2] : 0,
           table->period > 24 ? table->bit_table[3] : 0);
    }
    printf("----\n");


    }
    lfsr_tables_initialized = true;
    printf("LFSR tables initialized.\n");
}


// Free LFSR tables
void px_vm_free_lfsr_tables(void) {
    printf("Freeing LFSR tables...\n");
    for (int type = 0; type < NUM_LFSR_TYPES; type++) {
        LfsrPrecomputedTable* table = &precomputed_lfsrs[type];
        if (table->bit_table) {
            free(table->bit_table);
            table->bit_table = NULL;
        }
        table->initialized = false;
    }
    lfsr_tables_initialized = false;
    printf("LFSR tables freed.\n");
}

// Get LFSR bit value at specific position
float lfsr_get_bit(LfsrType type, uint32_t position) {
    if (type >= NUM_LFSR_TYPES || !precomputed_lfsrs[type].initialized || precomputed_lfsrs[type].period == 0) { return 0.0f; }
    LfsrPrecomputedTable* table = &precomputed_lfsrs[type];
    position = position % table->period; // Wrap around
    int byte_idx = position / 8;
    int bit_idx = position % 8;
    return (table->bit_table[byte_idx] & (1 << bit_idx)) ? 1.0f : 0.0f;
}

// Get LFSR noise (bipolar)
float lfsr_get_noise(LfsrType type, float phase, float rate) {
    if (type >= NUM_LFSR_TYPES || !precomputed_lfsrs[type].initialized || precomputed_lfsrs[type].period == 0) { return 0.0f; }
    LfsrPrecomputedTable* table = &precomputed_lfsrs[type];
    uint32_t position = (uint32_t)(phase * rate * table->period / (2.0f * C_PI));
    position %= table->period; // Ensure wrap around after multiplication
    float bit_val = lfsr_get_bit(type, position);
    return bit_val * 2.0f - 1.0f; // Convert 0,1 to -1,1
}

// Get LFSR clock pulses
float lfsr_get_clock(LfsrType type, float phase, float density) {
    if (type >= NUM_LFSR_TYPES || !precomputed_lfsrs[type].initialized || precomputed_lfsrs[type].period == 0) {
        return 0.0f;
    }

    LfsrPrecomputedTable* table = &precomputed_lfsrs[type];
    uint32_t position = (uint32_t)(phase * table->period / (2.0f * C_PI));
    position %= table->period; // Ensure wrap around

    float bit_val = lfsr_get_bit(type, position);
    return (bit_val >= density) ? 1.0f : 0.0f;
}

// Helper to advance the free-running LFSR state in VmParams
static void advance_lfsr_state(VmParams *params) { // REMOVED const
    if (!params || params->lfsr_type >= NUM_LFSR_TYPES) return;

    int bit_length;
    uint32_t tap_mask;
    uint32_t period;

    // Access the configuration data directly from the file-scope lfsr_configs array
    const LfsrConfigEntry* config = NULL;
    for (int i = 0; i < NUM_LFSR_TYPES; i++) {
        if (lfsr_configs[i].type_enum == params->lfsr_type) {
            config = &lfsr_configs[i];
            break;
        }
    }

    if (!config) {
        // If we can't find config directly, try to get from precomputed table (fallback)
        LfsrPrecomputedTable *table = &precomputed_lfsrs[params->lfsr_type];
        if (!table->initialized || table->period == 0) return; // Cannot proceed

        tap_mask = table->polynomial; // This was stored from config->tap_mask
        period = table->period;
        bit_length = 0;

        // Estimate bit length from period (period = 2^n - 1 for maximal length LFSR)
        for (int n = 1; n <= 32; n++) {
            if (period == ((1UL << n) - 1)) {
                bit_length = n;
                break;
            }
        }
        if (bit_length == 0) {
            for (int i = 0; i < NUM_LFSR_TYPES; i++) {
                if (lfsr_configs[i].period == period && lfsr_configs[i].tap_mask == tap_mask) {
                    bit_length = lfsr_configs[i].bit_length;
                    break;
                }
            }
            if (bit_length == 0) return;
        }
    } else {
        // Use config data directly
        bit_length = config->bit_length;
        tap_mask = config->tap_mask;
        period = config->period;
    }

    if (period == 0) return; // Cannot advance LFSR with zero period

    // Advance LFSR (Fibonacci configuration)
    uint32_t lfsr_state = params->lfsr_state;
    uint8_t feedback_bit = 0;
    uint32_t full_mask = (bit_length == 32) ? UINT32_MAX : ((1UL << bit_length) - 1);

    // Calculate feedback bit by XORing tapped bits
    for (int k = 0; k < bit_length; k++) {
        if ((tap_mask >> k) & 1) {
            feedback_bit ^= (lfsr_state >> k) & 1;
        }
    }

    // Shift and insert feedback bit
    lfsr_state = (lfsr_state >> 1) | ((uint32_t)feedback_bit << (bit_length - 1));
    lfsr_state &= full_mask; // Ensure it stays within bit_length

    // Ensure lfsr_state is not zero if it's a maximal length LFSR (except for period 0)
    if (lfsr_state == 0 && period > 0) {
        lfsr_state = params->lfsr_seed ? params->lfsr_seed : 1; // Reset to seed or 1 if seed is 0
    }

    // Update VmParams (NO MORE CONST CAST NEEDED)
    params->lfsr_state = lfsr_state;
    params->lfsr_position = (params->lfsr_position + 1) % period;
}


// --- VM Helper Functions ---

static void push(VM *vm, float value) {
    if (vm->stack_top >= vm->stack + MAX_VM_STACK) {
        vm_error(vm, "Stack overflow.");
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

static float pop(VM *vm) {
    if (vm->stack_top == vm->stack) {
        vm_error(vm, "Stack underflow.");
        return 0.0f; // Return dummy value
    }
    vm->stack_top--;
    return *vm->stack_top;
}

static float peek(VM *vm, int distance) {
    // distance 0 = top, 1 = one below top, etc.
    if (vm->stack_top - (distance + 1) < vm->stack) {
        vm_error(vm, "Stack peek underflow (distance %d).", distance);
        return 0.0f;
    }
    return *(vm->stack_top - 1 - distance);
}

// Read a 16-bit value (e.g., index, offset) from bytecode stream
static uint16_t read_short(VM *vm) {
    vm->ip += 2;
    return (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]);
}

// Read a signed 16-bit relative offset
static int16_t read_jump_offset(VM *vm) {
     vm->ip += 2;
     return (int16_t)((vm->ip[-2] << 8) | vm->ip[-1]);
}

// Read a single byte operand
static uint8_t read_byte(VM *vm) {
     return *vm->ip++;
}

// Helper for logical truthiness in VM
#define VM_IS_TRUE(f) (fabsf(f) > EPSILON)

// --- Sigma Sub-Execution Helper ---
static float execute_sub_chunk(VM *vm, BytecodeChunk *sub_chunk) {
    if (PX_UNLIKELY(!sub_chunk || sub_chunk->code_count == 0)) {
        vm_error(vm, "Attempted to execute empty or invalid sub-chunk.");
        return 0.0f;
    }
    if (PX_UNLIKELY(!vm->params)) {
        vm_error(vm, "VM parameters pointer is NULL in execute_sub_chunk.");
        return 0.0f;
    }

    // --- Store outer execution state ---
    BytecodeChunk *outer_chunk = vm->chunk;
    uint8_t       *outer_ip = vm->ip;
    float         *outer_stack_top = vm->stack_top; // CRITICAL: Save stack position

    // --- Set VM context to sub-chunk ---
    vm->chunk = sub_chunk;

    // --- Register Caching ---
    register uint8_t *ip = sub_chunk->code;
    register float *sp = vm->stack_top;

    // IMPORTANT: DON'T modify is_in_sigma_body or active_loop_var - inherit from main VM
    // vm->params remains the SAME pointer, allowing LFSR state modification

    float result = 0.0f;
    bool success = true;

    // Dispatch table for sub-chunk (same as main but with different labels)
    static const void* sub_dispatch_table[VM_DISPATCH_TABLE_SIZE] = {
        /* 0x00 OP_PUSH_CONST */       &&SUB_LABEL_OP_PUSH_CONST,
        /* 0x01 OP_PUSH_VAR_X */       &&SUB_LABEL_OP_PUSH_VAR_X,
        /* 0x02 OP_PUSH_VAR_FREQ */    &&SUB_LABEL_OP_PUSH_VAR_FREQ,
        /* 0x03 OP_PUSH_VAR_RAND */    &&SUB_LABEL_OP_PUSH_VAR_RAND,
        /* 0x04 OP_PUSH_VAR_MOD_A */   &&SUB_LABEL_OP_PUSH_VAR_MOD_A,
        /* 0x05 OP_PUSH_VAR_MOD_B */   &&SUB_LABEL_OP_PUSH_VAR_MOD_B,
        /* 0x06 OP_PUSH_VAR_MOD_C */   &&SUB_LABEL_OP_PUSH_VAR_MOD_C,
        /* 0x07 OP_POP */              &&SUB_LABEL_OP_POP,
        /* 0x08 OP_ADD */              &&SUB_LABEL_OP_ADD,
        /* 0x09 OP_SUB */              &&SUB_LABEL_OP_SUB,
        /* 0x0A OP_MUL */              &&SUB_LABEL_OP_MUL,
        /* 0x0B OP_DIV */              &&SUB_LABEL_OP_DIV,
        /* 0x0C OP_MOD */              &&SUB_LABEL_OP_MOD,
        /* 0x0D OP_NEGATE */           &&SUB_LABEL_OP_NEGATE,
        /* 0x0E OP_NOT */              &&SUB_LABEL_OP_NOT,
        /* 0x0F OP_CMP_EQ */           &&SUB_LABEL_OP_CMP_EQ,
        /* 0x10 OP_CMP_NE */           &&SUB_LABEL_OP_CMP_NE,
        /* 0x11 OP_CMP_GT */           &&SUB_LABEL_OP_CMP_GT,
        /* 0x12 OP_CMP_GE */           &&SUB_LABEL_OP_CMP_GE,
        /* 0x13 OP_CMP_LT */           &&SUB_LABEL_OP_CMP_LT,
        /* 0x14 OP_CMP_LE */           &&SUB_LABEL_OP_CMP_LE,
        /* 0x15 OP_JUMP */             &&SUB_LABEL_OP_JUMP,
        /* 0x16 OP_JUMP_IF_FALSE */   &&SUB_LABEL_OP_JUMP_IF_FALSE,
        /* 0x17 OP_CALL */             &&SUB_LABEL_OP_CALL,
        /* 0x18 OP_SIGMA_SETUP */      &&SUB_LABEL_ERROR_SIGMA_SETUP_IN_SUB,
        /* 0x19 OP_PUSH_LOOP_VAR */   &&SUB_LABEL_OP_PUSH_LOOP_VAR,
        /* 0x1A OP_SIGMA_EXEC */      &&SUB_LABEL_ERROR_SIGMA_EXEC_IN_SUB,
        /* 0x1B OP_HALT */             &&SUB_LABEL_OP_HALT
    };

    uint8_t instruction;

    // --- Sub-VM Loop Start ---
    if (PX_UNLIKELY(ip >= vm->chunk->code + vm->chunk->code_count)) {
        vm->ip = ip; vm->stack_top = sp;
        goto SUB_LABEL_ERROR_IP_OUT_OF_BOUNDS;
    }
    instruction = *ip++;
    if (PX_UNLIKELY(instruction > VM_MAX_OPCODE)) {
        vm->ip = ip; vm->stack_top = sp;
        goto SUB_LABEL_ERROR_UNKNOWN_OPCODE;
    }
    goto *sub_dispatch_table[instruction];

    // --- Opcode Implementation Labels for Sub-Chunk ---
SUB_LABEL_OP_PUSH_CONST: {
    uint16_t const_idx = (uint16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    if (PX_UNLIKELY(const_idx >= vm->chunk->constants_count)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Invalid constant index %u (sub).", const_idx);
        *sp++ = 0.0f;
        success = false;
    } else {
        *sp++ = vm->chunk->constants[const_idx];
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_X: {
    *sp++ = vm->params->x;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_FREQ: {
    *sp++ = vm->params->frequency;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_RAND: {
    *sp++ = vm->params->rand_offset;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_MOD_A: {
    *sp++ = vm->params->modA;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_MOD_B: {
    *sp++ = vm->params->modB;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_VAR_MOD_C: {
    *sp++ = vm->params->modC;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_POP: {
    if (PX_LIKELY(sp > vm->stack)) {
        sp--;
    } else {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_POP in sub-chunk.");
        success = false;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_ADD: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_ADD in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f; // Push dummy result
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = a + b;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_SUB: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_SUB in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = a - b;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_MUL: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_MUL in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = a * b;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_DIV: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_DIV in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        if (PX_UNLIKELY(!VM_IS_TRUE(b))) {
            vm->ip = ip; vm->stack_top = sp;
            vm_error(vm,"Division by zero (sub).");
            *sp++ = 0.0f;
            success = false;
        } else {
            *sp++ = a / b;
        }
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_MOD: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_MOD in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        if (PX_UNLIKELY(!VM_IS_TRUE(b))) {
            vm->ip = ip; vm->stack_top = sp;
            vm_error(vm,"Modulo by zero (sub).");
            *sp++ = 0.0f;
            success = false;
        } else {
            *sp++ = fmodf(a, b);
        }
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_NEGATE: {
    if (PX_UNLIKELY(sp <= vm->stack)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_NEGATE in sub-chunk.");
        *sp++ = 0.0f;
        success = false;
    } else {
        sp[-1] = -sp[-1];
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_NOT: {
    if (PX_UNLIKELY(sp <= vm->stack)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_NOT in sub-chunk.");
        *sp++ = 0.0f;
        success = false;
    } else {
        sp[-1] = VM_IS_TRUE(sp[-1]) ? 0.0f : 1.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_EQ: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_EQ in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = fabsf(a - b) < EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_NE: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_NE in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = fabsf(a - b) >= EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_GT: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_GT in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = (a - b) > EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_GE: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_GE in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = (a - b) > -EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_LT: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_LT in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = (a - b) < -EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CMP_LE: {
    if (PX_UNLIKELY((sp - vm->stack) < 2)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_CMP_LE in sub-chunk (need 2, have %td).", sp - vm->stack);
        *sp++ = 0.0f;
        success = false;
    } else {
        float b = *(--sp);
        float a = *(--sp);
        *sp++ = (a - b) < EPSILON ? 1.0f : 0.0f;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_JUMP: {
    int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    ip += offset;
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_JUMP_IF_FALSE: {
    int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    if (PX_UNLIKELY(sp <= vm->stack)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow on OP_JUMP_IF_FALSE in sub-chunk.");
        success = false;
    } else {
        float condition = *(--sp);
        if (!VM_IS_TRUE(condition)) {
            ip += offset;
        }
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_CALL: {
    uint8_t func_id = *ip++;
    uint8_t arg_count = *ip++;
    float call_result = 0.0f;

    // Check stack size BEFORE popping
    if (PX_UNLIKELY((sp - vm->stack) < arg_count)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Stack underflow for OP_CALL in sub-chunk! Need %u args, have %td", arg_count, sp - vm->stack);
        success = false;
        *sp++ = 0.0f;
    } else {
        vm->stack_top = sp; // Sync for helper calls
        vm->ip = ip; // Sync ip just in case

        switch (func_id) {
            case FUNC_ID_SIN: if (arg_count == 1) call_result = sinf(pop(vm)); else { vm_error(vm,"sin expects 1 arg"); success = false; } break;
            case FUNC_ID_COS: if (arg_count == 1) call_result = cosf(pop(vm)); else { vm_error(vm,"cos expects 1 arg"); success = false; } break;
            case FUNC_ID_TAN: if (arg_count == 1) call_result = tanf(pop(vm)); else { vm_error(vm,"tan expects 1 arg"); success = false; } break;
            case FUNC_ID_ASIN: if (arg_count == 1) { float v=pop(vm); call_result = asinf(fmaxf(-1.0f, fminf(1.0f, v))); } else { vm_error(vm,"asin expects 1 arg"); success = false; } break;
            case FUNC_ID_ACOS: if (arg_count == 1) { float v=pop(vm); call_result = acosf(fmaxf(-1.0f, fminf(1.0f, v))); } else { vm_error(vm,"acos expects 1 arg"); success = false; } break;
            case FUNC_ID_ATAN: if (arg_count == 1) call_result = atanf(pop(vm)); else { vm_error(vm,"atan expects 1 arg"); success = false; } break;
            case FUNC_ID_ABS: if (arg_count == 1) call_result = fabsf(pop(vm)); else { vm_error(vm,"abs expects 1 arg"); success = false; } break;
            case FUNC_ID_TANH: if (arg_count == 1) call_result = tanhf(pop(vm)); else { vm_error(vm,"tanh expects 1 arg"); success = false; } break;
            case FUNC_ID_EXP: if (arg_count == 1) call_result = expf(pop(vm)); else { vm_error(vm,"exp expects 1 arg"); success = false; } break;
            case FUNC_ID_LOG: if (arg_count == 1) { float v=pop(vm); call_result=(v > 0) ? logf(v): 0.0f; } else { vm_error(vm,"log expects 1 arg"); success = false; } break;
            case FUNC_ID_LOG10: if (arg_count == 1) { float v=pop(vm); call_result=(v > 0) ? log10f(v): 0.0f; } else { vm_error(vm,"log10 expects 1 arg"); success = false; } break;
            case FUNC_ID_FLOOR: if (arg_count == 1) call_result = floorf(pop(vm)); else { vm_error(vm,"floor expects 1 arg"); success = false; } break;
            case FUNC_ID_CEIL: if (arg_count == 1) call_result = ceilf(pop(vm)); else { vm_error(vm,"ceil expects 1 arg"); success = false; } break;
            case FUNC_ID_MIN: if (arg_count == 2) { float b=pop(vm); float a=pop(vm); call_result=fminf(a,b); } else { vm_error(vm,"min expects 2 args"); success = false; } break;
            case FUNC_ID_MAX: if (arg_count == 2) { float b=pop(vm); float a=pop(vm); call_result=fmaxf(a,b); } else { vm_error(vm,"max expects 2 args"); success = false; } break;
            case FUNC_ID_SQRT: if (arg_count == 1) { float v=pop(vm); call_result=(v >= 0) ? sqrtf(v): 0.0f; } else { vm_error(vm,"sqrt expects 1 arg"); success = false; } break;
            case FUNC_ID_POW: if (arg_count == 2) { float b=pop(vm); float a=pop(vm); call_result=powf(a,b); } else { vm_error(vm,"pow expects 2 args"); success = false; } break;
            case FUNC_ID_RAND: if (arg_count == 0) call_result = (float)rand() / RAND_MAX; else { vm_error(vm,"rand expects 0 args"); success = false; } break;

            case FUNC_ID_LFSR_VAL:
                if (arg_count == 3) {
                    float seed_arg = pop(vm);
                    float position_arg = pop(vm);
                    float type_arg_float = pop(vm);
                    int type_id = (int)roundf(type_arg_float);

                    if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                        LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                        if (table->period > 0) {
                            if (vm->params->lfsr_type == (LfsrType)type_id && vm->params->lfsr_state != 0) {
                                advance_lfsr_state(vm->params);
                                call_result = (vm->params->lfsr_state & 1) ? 1.0f : 0.0f;
                            } else {
                                float norm_pos = fmodf(position_arg, 1.0f);
                                if (norm_pos < 0.0f) norm_pos += 1.0f;
                                float norm_seed_offset = fmodf(seed_arg, 1.0f);
                                if (norm_seed_offset < 0.0f) norm_seed_offset += 1.0f;
                                float combined_norm_pos = fmodf(norm_pos + norm_seed_offset, 1.0f);
                                uint32_t table_index = (uint32_t)(combined_norm_pos * table->period);
                                if (table_index >= table->period) table_index = table->period - 1;
                                call_result = lfsr_get_bit((LfsrType)type_id, table_index);
                            }
                        } else {
                            vm_error(vm, "lfsr_val (sub): LFSR type %d (enum %s) has zero period.", type_id, lfsr_type_to_string((LfsrType)type_id));
                            call_result = 0.0f;
                            success = false;
                        }
                    } else {
                        vm_error(vm, "lfsr_val (sub): Invalid or uninitialized LFSR type %d.", type_id);
                        call_result = 0.0f;
                        success = false;
                    }
                } else {
                    vm_error(vm, "lfsr_val (sub) expects 3 arguments, got %u.", arg_count);
                    for(uint8_t k = 0; k < arg_count; ++k) if(vm->stack_top > vm->stack) pop(vm);
                    call_result = 0.0f;
                    success = false;
                }
                break;

            case FUNC_ID_LFSR_NOISE:
                if (arg_count == 2) {
                    float rate_arg = pop(vm);
                    float type_arg_float = pop(vm);
                    int type_id = (int)roundf(type_arg_float);
                    if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                        LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                        if (table->period > 0) {
                            if (vm->params->lfsr_type == (LfsrType)type_id && vm->params->lfsr_state != 0) {
                                advance_lfsr_state(vm->params);
                                call_result = ((vm->params->lfsr_state & 1) ? 1.0f : 0.0f) * 2.0f - 1.0f;
                            } else {
                                float normalized_lfsr_phase = fmodf((vm->params->x / C_TWO_PI) * rate_arg, 1.0f);
                                if (normalized_lfsr_phase < 0.0f) normalized_lfsr_phase += 1.0f;
                                uint32_t table_index = (uint32_t)(normalized_lfsr_phase * table->period);
                                if (table_index >= table->period) table_index = table->period - 1;
                                call_result = (lfsr_get_bit((LfsrType)type_id, table_index) * 2.0f) - 1.0f;
                            }
                        } else {
                            vm_error(vm, "lfsr_noise (sub): LFSR type %d has zero period.", type_id);
                            call_result = 0.0f;
                            success = false;
                        }
                    } else {
                        vm_error(vm, "lfsr_noise (sub): Invalid or uninitialized LFSR type %d.", type_id);
                        call_result = 0.0f;
                        success = false;
                    }
                } else {
                    vm_error(vm, "lfsr_noise (sub) expects 2 arguments, got %u.", arg_count);
                    for(uint8_t k = 0; k < arg_count; ++k) if(vm->stack_top > vm->stack) pop(vm);
                    call_result = 0.0f;
                    success = false;
                }
                break;

            case FUNC_ID_LFSR_CLOCK:
                if (arg_count == 2) {
                    float density_arg = pop(vm);
                    float type_arg_float = pop(vm);
                    int type_id = (int)roundf(type_arg_float);
                    if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                        LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                        if (table->period > 0) {
                            float clamped_density = fmaxf(0.0f, fminf(1.0f, density_arg));
                            if (vm->params->lfsr_type == (LfsrType)type_id && vm->params->lfsr_state != 0) {
                                advance_lfsr_state(vm->params);
                                float bit_val = (vm->params->lfsr_state & 1) ? 1.0f : 0.0f;
                                call_result = (bit_val >= clamped_density) ? 1.0f : 0.0f;
                            } else {
                                call_result = lfsr_get_clock((LfsrType)type_id, vm->params->x, clamped_density);
                            }
                        } else {
                            vm_error(vm, "lfsr_clock (sub): LFSR type %d has zero period.", type_id);
                            call_result = 0.0f;
                            success = false;
                        }
                    } else {
                        vm_error(vm, "lfsr_clock (sub): Invalid or uninitialized LFSR type %d.", type_id);
                        call_result = 0.0f;
                        success = false;
                    }
                } else {
                    vm_error(vm, "lfsr_clock (sub) expects 2 arguments, got %u.", arg_count);
                    for(uint8_t k = 0; k < arg_count; ++k) if(vm->stack_top > vm->stack) pop(vm);
                    call_result = 0.0f;
                    success = false;
                }
                break;

            default:
                vm_error(vm, "Unknown function ID %d in OP_CALL (sub).", func_id);
                success = false;
                break;
        }
        sp = vm->stack_top; // Sync back
        *sp++ = call_result;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_OP_PUSH_LOOP_VAR: {
    uint8_t name_index = *ip++;
    if (PX_UNLIKELY(!vm->is_in_sigma_body)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "OP_PUSH_LOOP_VAR used outside sigma (sub).");
        *sp++ = 0.0f;
        success = false;
    } else if (PX_UNLIKELY(name_index >= vm->chunk->strings_count || vm->chunk->strings[name_index] == NULL)) {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "OP_PUSH_LOOP_VAR invalid name index %u (sub).", name_index);
        *sp++ = 0.0f;
        success = false;
    } else if (PX_LIKELY(vm->active_loop_var.name != NULL && strcmp(vm->chunk->strings[name_index], vm->active_loop_var.name) == 0)) {
        *sp++ = vm->active_loop_var.current_value;
    } else {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "OP_PUSH_LOOP_VAR name mismatch (sub) ('%s' vs '%s')", vm->chunk->strings[name_index], vm->active_loop_var.name ? vm->active_loop_var.name : "<none>");
        *sp++ = 0.0f;
        success = false;
    }
    instruction = *ip++;
    goto *sub_dispatch_table[instruction];
}

SUB_LABEL_ERROR_SIGMA_SETUP_IN_SUB: {
    ip += 9;
    vm->ip = ip; vm->stack_top = sp;
    vm_error(vm, "Unexpected OP_SIGMA_SETUP encountered in sub-chunk.");
    success = false;
    goto sub_chunk_end;
}

SUB_LABEL_ERROR_SIGMA_EXEC_IN_SUB: {
    ip += 9;
    vm->ip = ip; vm->stack_top = sp;
    vm_error(vm, "Nested OP_SIGMA_EXEC detected (sub). Not allowed.");
    success = false;
    goto sub_chunk_end;
}

SUB_LABEL_OP_HALT: {
    // CRITICAL FIX: Always leave exactly one result on stack for caller
    if (PX_LIKELY(success && sp > vm->stack)) {
        result = *(sp - 1); // Peek at top value
        // Don't pop here - let caller handle it
    } else {
        vm->ip = ip; vm->stack_top = sp;
        vm_error(vm, "Sub-chunk halted with error or empty stack.");
        success = false;
        result = 0.0f;
        // Ensure there's a value on stack for caller
        if (sp <= vm->stack) {
            *sp++ = 0.0f;
        }
    }
    goto sub_chunk_end;
}

SUB_LABEL_ERROR_UNKNOWN_OPCODE: {
    uint8_t bad_instruction = ip[-1];
    vm->ip = ip; vm->stack_top = sp;
    vm_error(vm, "Unknown opcode 0x%02X in sub-chunk.", bad_instruction);
    success = false;
    goto sub_chunk_end;
}

SUB_LABEL_ERROR_IP_OUT_OF_BOUNDS: {
    vm->ip = ip; vm->stack_top = sp;
    vm_error(vm, "Sub-chunk IP out of bounds at offset %td (code size %d).", (ip - vm->chunk->code), vm->chunk->code_count);
    success = false;
    goto sub_chunk_end;
}

sub_chunk_end:
    // --- Consistent Stack Management ---
    // Always ensure there's exactly one result on the stack for the caller
    if (success) {
        if (sp > vm->stack) {
            result = *(sp - 1); // Get the result
            sp--; // Pop it from sub-chunk's perspective
        } else {
            // This shouldn't happen if success=true, but safety
            result = 0.0f;
            success = false;
        }
    } else {
        // Error case: ensure no stack pollution
        result = 0.0f;
        // Reset stack to where it was when we started
        vm->stack_top = outer_stack_top;
    }

    // --- Restore outer execution state ---
    vm->chunk = outer_chunk;
    vm->ip = outer_ip;
    // Note: sigma state (is_in_sigma_body, active_loop_var) inherited from main VM, don't restore

    return result;
}

// --- Public VM Execution Function ---
float execute_bytecode(BytecodeChunk *chunk, VmParams* params) {
    // --- Initial Checks ---
    if (PX_UNLIKELY(!chunk || chunk->code_count == 0)) { return 0.0f; }
    if (PX_UNLIKELY(!params)) { return 0.0f; }

    // --- VM Setup ---
    VM vm;
    vm.chunk = chunk;
    // vm.ip = chunk->code;
    vm.stack_top = vm.stack;
    vm.params = params; // Store the pointer to the parameters
    vm.is_in_sigma_body = false; // Reset sigma state for main execution
    vm.active_loop_var.name = NULL;
    vm.active_loop_var.current_value = 0.0f;

    // --- Register Caching ---
    register uint8_t *ip = chunk->code;
    register float *sp = vm.stack;

    // --- Success Flag for Robust Error Handling
    bool success = true;
    uint8_t instruction; // Moved outside the loop

    // --- Dispatch Table (Ensure size and indices match OpCode enum) ---
    static const void* dispatch_table[VM_DISPATCH_TABLE_SIZE] = {
        /* 0x00 OP_PUSH_CONST */       &&LABEL_OP_PUSH_CONST,
        /* 0x01 OP_PUSH_VAR_X */       &&LABEL_OP_PUSH_VAR_X,
        /* 0x02 OP_PUSH_VAR_FREQ */    &&LABEL_OP_PUSH_VAR_FREQ,
        /* 0x03 OP_PUSH_VAR_RAND */    &&LABEL_OP_PUSH_VAR_RAND,
        /* 0x04 OP_PUSH_VAR_MOD_A */   &&LABEL_OP_PUSH_VAR_MOD_A,
        /* 0x05 OP_PUSH_VAR_MOD_B */   &&LABEL_OP_PUSH_VAR_MOD_B,
        /* 0x06 OP_PUSH_VAR_MOD_C */   &&LABEL_OP_PUSH_VAR_MOD_C,
        /* 0x07 OP_POP */              &&LABEL_OP_POP,
        /* 0x08 OP_ADD */              &&LABEL_OP_ADD,
        /* 0x09 OP_SUB */              &&LABEL_OP_SUB,
        /* 0x0A OP_MUL */              &&LABEL_OP_MUL,
        /* 0x0B OP_DIV */              &&LABEL_OP_DIV,
        /* 0x0C OP_MOD */              &&LABEL_OP_MOD,
        /* 0x0D OP_NEGATE */           &&LABEL_OP_NEGATE,
        /* 0x0E OP_NOT */              &&LABEL_OP_NOT,
        /* 0x0F OP_CMP_EQ */           &&LABEL_OP_CMP_EQ,
        /* 0x10 OP_CMP_NE */           &&LABEL_OP_CMP_NE,
        /* 0x11 OP_CMP_GT */           &&LABEL_OP_CMP_GT,
        /* 0x12 OP_CMP_GE */           &&LABEL_OP_CMP_GE,
        /* 0x13 OP_CMP_LT */           &&LABEL_OP_CMP_LT,
        /* 0x14 OP_CMP_LE */           &&LABEL_OP_CMP_LE,
        /* 0x15 OP_JUMP */             &&LABEL_OP_JUMP,
        /* 0x16 OP_JUMP_IF_FALSE */   &&LABEL_OP_JUMP_IF_FALSE,
        /* 0x17 OP_CALL */             &&LABEL_OP_CALL,
        /* 0x18 OP_SIGMA_SETUP */      &&LABEL_OP_SIGMA_SETUP,
        /* 0x19 OP_PUSH_LOOP_VAR */   &&LABEL_OP_PUSH_LOOP_VAR,
        /* 0x1A OP_SIGMA_EXEC */      &&LABEL_OP_SIGMA_EXEC,
        /* 0x1B OP_HALT */             &&LABEL_OP_HALT
    };

    // --- Central Dispatch Loop ---
DISPATCH_LOOP:
    // Fetch, Decode, and Dispatch
    instruction = *ip++;
    goto *dispatch_table[instruction];

    // --- Opcode Implementation Labels ---
LABEL_OP_PUSH_CONST: {
    uint16_t const_idx = (uint16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    if (PX_UNLIKELY(const_idx >= vm.chunk->constants_count)) {
        vm.ip = ip; vm.stack_top = sp;
        vm_error(&vm, "Invalid constant index %u.", const_idx);
        *sp++ = 0.0f;
        success = false;
    } else {
        *sp++ = vm.chunk->constants[const_idx];
    }
    goto DISPATCH_LOOP;
}
LABEL_OP_PUSH_VAR_X:     { *sp++ = vm.params->x;          goto DISPATCH_LOOP; }
LABEL_OP_PUSH_VAR_FREQ:  { *sp++ = vm.params->frequency;  goto DISPATCH_LOOP; }
LABEL_OP_PUSH_VAR_RAND:  { *sp++ = vm.params->rand_offset;goto DISPATCH_LOOP; }
LABEL_OP_PUSH_VAR_MOD_A: { *sp++ = vm.params->modA;       goto DISPATCH_LOOP; }
LABEL_OP_PUSH_VAR_MOD_B: { *sp++ = vm.params->modB;       goto DISPATCH_LOOP; }
LABEL_OP_PUSH_VAR_MOD_C: { *sp++ = vm.params->modC;       goto DISPATCH_LOOP; }
LABEL_OP_POP:            { sp--;                          goto DISPATCH_LOOP; }

LABEL_OP_ADD: { float b = *(--sp); float a = *(--sp); *sp++ = a + b; goto DISPATCH_LOOP; }
LABEL_OP_SUB: { float b = *(--sp); float a = *(--sp); *sp++ = a - b; goto DISPATCH_LOOP; }
LABEL_OP_MUL: { float b = *(--sp); float a = *(--sp); *sp++ = a * b; goto DISPATCH_LOOP; }
LABEL_OP_DIV: {
    float b = *(--sp);
    float a = *(--sp);
    if (PX_UNLIKELY(!VM_IS_TRUE(b))) {
        vm.ip = ip; vm.stack_top = sp;
        vm_error(&vm,"Division by zero.");
        *sp++ = 0.0f;
        success = false;
    } else {
        *sp++ = a / b;
    }
    goto DISPATCH_LOOP;
}
LABEL_OP_MOD: {
    float b = *(--sp);
    float a = *(--sp);
    if (PX_UNLIKELY(!VM_IS_TRUE(b))) {
        vm.ip = ip; vm.stack_top = sp;
        vm_error(&vm,"Modulo by zero.");
        *sp++ = 0.0f;
        success = false;
    } else {
        *sp++ = fmodf(a, b);
    }
    goto DISPATCH_LOOP;
}
LABEL_OP_NEGATE: { sp[-1] = -sp[-1]; goto DISPATCH_LOOP; }
LABEL_OP_NOT:    { sp[-1] = VM_IS_TRUE(sp[-1]) ? 0.0f : 1.0f; goto DISPATCH_LOOP; }

LABEL_OP_CMP_EQ: { float b = *(--sp); float a = *(--sp); *sp++ = fabsf(a - b) < EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }
LABEL_OP_CMP_NE: { float b = *(--sp); float a = *(--sp); *sp++ = fabsf(a - b) >= EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }
LABEL_OP_CMP_GT: { float b = *(--sp); float a = *(--sp); *sp++ = (a - b) > EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }
LABEL_OP_CMP_GE: { float b = *(--sp); float a = *(--sp); *sp++ = (a - b) > -EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }
LABEL_OP_CMP_LT: { float b = *(--sp); float a = *(--sp); *sp++ = (a - b) < -EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }
LABEL_OP_CMP_LE: { float b = *(--sp); float a = *(--sp); *sp++ = (a - b) < EPSILON ? 1.0f : 0.0f; goto DISPATCH_LOOP; }

LABEL_OP_JUMP: {
    int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    ip += offset;
    goto DISPATCH_LOOP;
}
LABEL_OP_JUMP_IF_FALSE: {
    int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
    ip += 2;
    if (!VM_IS_TRUE(*(--sp))) {
        ip += offset;
    }
    goto DISPATCH_LOOP;
}

LABEL_OP_CALL: {
    uint8_t func_id = *ip++;
    uint8_t arg_count = *ip++;
    float call_result = 0.0f;
    if (PX_UNLIKELY((sp - vm.stack) < arg_count)) {
        vm.ip = ip; vm.stack_top = sp;
        vm_error(&vm, "Stack underflow for OP_CALL! Need %u args, have %td", arg_count, sp - vm.stack);
        success = false;
        *sp++ = 0.0f;
    } else {
        vm.stack_top = sp; // Sync
        vm.ip = ip; // Sync

        switch (func_id) {
        case FUNC_ID_SIN: if (arg_count == 1) call_result = sinf(pop(&vm)); else vm_error(&vm,"sin expects 1 arg"); break;
        case FUNC_ID_COS: if (arg_count == 1) call_result = cosf(pop(&vm)); else vm_error(&vm,"cos expects 1 arg"); break;
        case FUNC_ID_TAN: if (arg_count == 1) call_result = tanf(pop(&vm)); else vm_error(&vm,"tan expects 1 arg"); break;
        case FUNC_ID_ASIN: if (arg_count == 1) { float v=pop(&vm); call_result = asinf(fmaxf(-1.0f, fminf(1.0f, v))); } else vm_error(&vm,"asin expects 1 arg"); break;
        case FUNC_ID_ACOS: if (arg_count == 1) { float v=pop(&vm); call_result = acosf(fmaxf(-1.0f, fminf(1.0f, v))); } else vm_error(&vm,"acos expects 1 arg"); break;
        case FUNC_ID_ATAN: if (arg_count == 1) call_result = atanf(pop(&vm)); else vm_error(&vm,"atan expects 1 arg"); break;
        case FUNC_ID_ABS: if (arg_count == 1) call_result = fabsf(pop(&vm)); else vm_error(&vm,"abs expects 1 arg"); break;
        case FUNC_ID_TANH: if (arg_count == 1) call_result = tanhf(pop(&vm)); else vm_error(&vm,"tanh expects 1 arg"); break;
        case FUNC_ID_EXP: if (arg_count == 1) call_result = expf(pop(&vm)); else vm_error(&vm,"exp expects 1 arg"); break;
        case FUNC_ID_LOG: if (arg_count == 1) { float v=pop(&vm); call_result=(v > 0) ? logf(v): 0.0f; } else vm_error(&vm,"log expects 1 arg"); break;
        case FUNC_ID_LOG10: if (arg_count == 1) { float v=pop(&vm); call_result=(v > 0) ? log10f(v): 0.0f; } else vm_error(&vm,"log10 expects 1 arg"); break;
        case FUNC_ID_FLOOR: if (arg_count == 1) call_result = floorf(pop(&vm)); else vm_error(&vm,"floor expects 1 arg"); break;
        case FUNC_ID_CEIL: if (arg_count == 1) call_result = ceilf(pop(&vm)); else vm_error(&vm,"ceil expects 1 arg"); break;
        case FUNC_ID_MIN: if (arg_count == 2) { float b=pop(&vm); float a=pop(&vm); call_result=fminf(a,b); } else vm_error(&vm,"min expects 2 args"); break;
        case FUNC_ID_MAX: if (arg_count == 2) { float b=pop(&vm); float a=pop(&vm); call_result=fmaxf(a,b); } else vm_error(&vm,"max expects 2 args"); break;
        case FUNC_ID_SQRT: if (arg_count == 1) { float v=pop(&vm); call_result=(v >= 0) ? sqrtf(v): 0.0f; } else vm_error(&vm,"sqrt expects 1 arg"); break;
        case FUNC_ID_POW: if (arg_count == 2) { float b=pop(&vm); float a=pop(&vm); call_result=powf(a,b); } else vm_error(&vm,"pow expects 2 args"); break;
        case FUNC_ID_RAND: if (arg_count == 0) call_result = (float)rand() / RAND_MAX; else vm_error(&vm,"rand expects 0 args"); break;
        case FUNC_ID_LFSR_VAL:
            if (arg_count == 3) {
                float seed_arg = pop(&vm); float position_arg = pop(&vm); float type_arg_float = pop(&vm); int type_id = (int)roundf(type_arg_float);
                if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                    LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                    if (table->period > 0) {
                        if (vm.params->lfsr_type == (LfsrType)type_id && vm.params->lfsr_state != 0) {
                            advance_lfsr_state(vm.params); call_result = (vm.params->lfsr_state & 1) ? 1.0f : 0.0f;
                        } else {
                            float norm_pos = fmodf(position_arg, 1.0f); if (norm_pos < 0.0f) norm_pos += 1.0f;
                            float norm_seed_offset = fmodf(seed_arg, 1.0f); if (norm_seed_offset < 0.0f) norm_seed_offset += 1.0f;
                            float combined_norm_pos = fmodf(norm_pos + norm_seed_offset, 1.0f);
                            uint32_t table_index = (uint32_t)(combined_norm_pos * table->period);
                            if (table_index >= table->period) table_index = table->period - 1;
                            call_result = lfsr_get_bit((LfsrType)type_id, table_index);
                        }
                    } else { vm_error(&vm, "lfsr_val: LFSR type %d has zero period.", type_id); call_result = 0.0f; }
                } else { vm_error(&vm, "lfsr_val: Invalid or uninitialized LFSR type %d.", type_id); call_result = 0.0f; }
            } else { vm_error(&vm, "lfsr_val expects 3 arguments, got %u.", arg_count); for(uint8_t k = 0; k < arg_count; ++k) if(vm.stack_top > vm.stack) pop(&vm); call_result = 0.0f; success = false; }
            break;
        case FUNC_ID_LFSR_NOISE:
            if (arg_count == 2) {
                float rate_arg = pop(&vm); float type_arg_float = pop(&vm); int type_id = (int)roundf(type_arg_float);
                if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                    LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                    if (table->period > 0) {
                        if (vm.params->lfsr_type == (LfsrType)type_id && vm.params->lfsr_state != 0) {
                            advance_lfsr_state(vm.params); call_result = ((vm.params->lfsr_state & 1) ? 1.0f : 0.0f) * 2.0f - 1.0f;
                        } else {
                            float normalized_lfsr_phase = fmodf((vm.params->x / C_TWO_PI) * rate_arg, 1.0f); if (normalized_lfsr_phase < 0.0f) normalized_lfsr_phase += 1.0f;
                            uint32_t table_index = (uint32_t)(normalized_lfsr_phase * table->period);
                            if (table_index >= table->period) table_index = table->period - 1;
                            call_result = (lfsr_get_bit((LfsrType)type_id, table_index) * 2.0f) - 1.0f;
                        }
                    } else { vm_error(&vm, "lfsr_noise: LFSR type %d has zero period.", type_id); call_result = 0.0f; }
                } else { vm_error(&vm, "lfsr_noise: Invalid or uninitialized LFSR type %d.", type_id); call_result = 0.0f; }
            } else { vm_error(&vm, "lfsr_noise expects 2 arguments, got %u.", arg_count); for(uint8_t k = 0; k < arg_count; ++k) if(vm.stack_top > vm.stack) pop(&vm); call_result = 0.0f; success = false; }
            break;
        case FUNC_ID_LFSR_CLOCK:
            if (arg_count == 2) {
                float density_arg = pop(&vm); float type_arg_float = pop(&vm); int type_id = (int)roundf(type_arg_float);
                if (type_id >= 0 && type_id < NUM_LFSR_TYPES && precomputed_lfsrs[type_id].initialized) {
                    LfsrPrecomputedTable* table = &precomputed_lfsrs[type_id];
                    if (table->period > 0) {
                        float clamped_density = fmaxf(0.0f, fminf(1.0f, density_arg));
                        if (vm.params->lfsr_type == (LfsrType)type_id && vm.params->lfsr_state != 0) {
                            advance_lfsr_state(vm.params); float bit_val = (vm.params->lfsr_state & 1) ? 1.0f : 0.0f; call_result = (bit_val >= clamped_density) ? 1.0f : 0.0f;
                        } else { call_result = lfsr_get_clock((LfsrType)type_id, vm.params->x, clamped_density); }
                    } else { vm_error(&vm, "lfsr_clock: LFSR type %d has zero period.", type_id); call_result = 0.0f; }
                } else { vm_error(&vm, "lfsr_clock: Invalid or uninitialized LFSR type %d.", type_id); call_result = 0.0f; }
            } else { vm_error(&vm, "lfsr_clock expects 2 arguments, got %u.", arg_count); for(uint8_t k = 0; k < arg_count; ++k) if(vm.stack_top > vm.stack) pop(&vm); call_result = 0.0f; success = false; }
            break;
        default: vm_error(&vm, "Unknown function ID %d in OP_CALL.", func_id); break;
        }
        sp = vm.stack_top; // Sync back
        *sp++ = call_result;
    }
    goto DISPATCH_LOOP;
}

LABEL_OP_SIGMA_SETUP: {
    // This opcode is deprecated by OP_SIGMA_EXEC, but we handle it gracefully.
    ip += 9; // Consume all 9 operand bytes
    vm.ip = ip; vm.stack_top = sp; // Sync
    vm_error(&vm, "OP_SIGMA_SETUP encountered unexpectedly.");
    success = false;
    goto execution_end;
}

LABEL_OP_PUSH_LOOP_VAR: {
    vm.ip = ip; vm.stack_top = sp; // Sync
    vm_error(&vm, "OP_PUSH_LOOP_VAR encountered outside sigma context.");
    *sp++ = 0.0f;
    success = false;
    goto DISPATCH_LOOP;
}

LABEL_OP_SIGMA_EXEC: {
    if (PX_UNLIKELY(vm.is_in_sigma_body)) {
        vm.ip = ip; vm.stack_top = sp; // Sync
        vm_error(&vm, "Nested OP_SIGMA_EXEC call detected at runtime.");
        *sp++ = 0.0f;
        success = false;
        ip += 9; // Consume operands to advance IP correctly
    } else {
        uint8_t name_index = *ip++;
        uint16_t start_idx = (uint16_t)((ip[0] << 8) | ip[1]); ip += 2;
        uint16_t end_idx   = (uint16_t)((ip[0] << 8) | ip[1]); ip += 2;
        uint16_t step_idx  = (uint16_t)((ip[0] << 8) | ip[1]); ip += 2;
        uint16_t body_idx  = (uint16_t)((ip[0] << 8) | ip[1]); ip += 2;

        bool indices_valid = (start_idx < vm.chunk->sigma_sub_chunk_count &&
                              end_idx   < vm.chunk->sigma_sub_chunk_count &&
                              step_idx  < vm.chunk->sigma_sub_chunk_count &&
                              body_idx  < vm.chunk->sigma_sub_chunk_count &&
                              name_index < vm.chunk->strings_count &&
                              vm.chunk->strings[name_index] != NULL);

        if (PX_UNLIKELY(!indices_valid)) {
             vm.ip = ip; vm.stack_top = sp; // Sync
             vm_error(&vm, "Invalid index in OP_SIGMA_EXEC operands.");
             *sp++ = 0.0f;
             success = false;
        } else {
            const char* loop_var_name_str = vm.chunk->strings[name_index];
            float sum = 0.0f;
            bool sigma_success = true;

            // Sync for sub-call
            vm.ip = ip; vm.stack_top = sp;

            float start_val = execute_sub_chunk(&vm, vm.chunk->sigma_sub_chunks[start_idx]);
            float end_val   = execute_sub_chunk(&vm, vm.chunk->sigma_sub_chunks[end_idx]);
            float step_val  = execute_sub_chunk(&vm, vm.chunk->sigma_sub_chunks[step_idx]);

            if (fabsf(step_val) < EPSILON) {
                vm_error(&vm, "Sigma step value is zero.");
                sigma_success = false;
            } else {
                vm.active_loop_var.name = loop_var_name_str;
                vm.is_in_sigma_body = true;

                int iterations = 0;
                const int MAX_ITERATIONS = 1000;
                BytecodeChunk* body_chunk = vm.chunk->sigma_sub_chunks[body_idx];
                float end_boundary = (step_val > 0) ? (end_val + fabsf(step_val) * 0.5f) : (end_val - fabsf(step_val) * 0.5f);

                for (float k = start_val; (step_val > 0) ? (k <= end_boundary) : (k >= end_boundary); k += step_val) {
                     if (++iterations > MAX_ITERATIONS) {
                         vm_error(&vm, "Sigma max iterations (%d) reached.", MAX_ITERATIONS);
                         sigma_success = false;
                         break;
                     }
                     vm.active_loop_var.current_value = k;
                     sum += execute_sub_chunk(&vm, body_chunk);
                }
                vm.is_in_sigma_body = false;
                vm.active_loop_var.name = NULL;
            }
            sp = vm.stack_top; // Restore stack top after sub-calls
            *sp++ = sigma_success ? sum : 0.0f;
            if (!sigma_success) { success = false; }
        }
    }
    goto DISPATCH_LOOP;
}

LABEL_OP_HALT: {
    goto execution_end;
}

execution_end:
    if (PX_UNLIKELY(!success)) {
        return 0.0f;
    }
    if (sp == vm.stack) {
        return 0.0f;
    } else {
        return *(--sp);
    }
}

// Generate function (no cache lookup/insert)
static bool compile_and_generate_wave( const char *expression, BytecodeChunk** chunk_ptr_location, float *output_buffer, uint16_t wave_length)
{
    if (!expression || !chunk_ptr_location || !output_buffer || wave_length == 0) {
        fprintf(stderr, "Error: Invalid arguments to compile_and_generate_wave.\n");
        if (chunk_ptr_location) *chunk_ptr_location = NULL; // Ensure it's NULL on error entry
        return false;
    }

    // --- Free existing bytecode if recompiling ---
    if (*chunk_ptr_location != NULL) {
         printf("Warning: Recompiling expression '%s', freeing old bytecode.\n", expression);
         free_bytecode_chunk(*chunk_ptr_location); // Free internal data
         free(*chunk_ptr_location);                // Free chunk struct
         *chunk_ptr_location = NULL;
    }

    // --- Tokenize and Parse ---
    Token tokens[256];
    int tokenCount = tokenize(expression, tokens, 256);
    if (tokenCount < 0) { *chunk_ptr_location = NULL; return false; }

    int pos = 0;
    Node *ast = parseExpression(tokens, &pos);
    if (!ast || tokens[pos].type != TOKEN_END) {
        fprintf(stderr, "Parsing failed near token %d ('%s') for: %s\n", pos, tokens[pos].value, expression);
        freeAST(ast);
        *chunk_ptr_location = NULL;
        return false;
    }


    // --- Allocate and Compile ---
    BytecodeChunk *new_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
    if (!new_chunk) {
        perror("Failed to allocate bytecode chunk in compile_and_generate_wave");
        freeAST(ast);
        *chunk_ptr_location = NULL;
        return false;
    }


    bool compile_ok = compile_ast_to_bytecode(ast, new_chunk);
    freeAST(ast); // Free AST regardless of compile success here

    if (!compile_ok) {
        fprintf(stderr, "Compilation failed for expression: %s\n", expression);
        free_bytecode_chunk(new_chunk); // Free internal data if any
        free(new_chunk); // Free the chunk struct
        *chunk_ptr_location = NULL;
        return false;
    }

    // --- Store the compiled chunk pointer ---
    *chunk_ptr_location = new_chunk; // Store the heap pointer in the provided location

    // --- Execute Bytecode with Enhanced VmParams ---
    float rand_offset_val = (float)rand() / RAND_MAX;

    // Initialize VmParams with LFSR state (NOT const)
    POLYSONIX_ALIGN(32) VmParams params = {
        .x = 0.0f,
        .frequency = 0.0f,
        .rand_offset = rand_offset_val,
        .modA = 0.0f,
        .modB = 0.0f,
        .modC = 0.0f,
        .lfsr_state = 1,
        .lfsr_type = LFSR_8BIT,
        .lfsr_position = 0,
        .lfsr_seed = 1
    };

    float first_sample = 0.0f;
    for (int i = 0; i < wave_length; i++) {
        params.x = ((float)i / (float)wave_length) * C_TWO_PI;
        float sample_f = execute_bytecode(*chunk_ptr_location, &params); // Pass non-const pointer
        sample_f = fmaxf(-1.0f, fminf(1.0f, sample_f));
        output_buffer[i] = sample_f;
        if (i == 0) first_sample = output_buffer[i];
    }
    if (wave_length > 1) output_buffer[wave_length - 1] = first_sample;

    return true;
}

/**
 * @brief Compiles an expression string into a heap-allocated BytecodeChunk.
 *
 * Performs tokenization, parsing, and bytecode compilation.
 * The returned BytecodeChunk pointer must be freed by the caller
 * eventually (typically using free_bytecode_chunk() then free()).
 *
 * @param expression The mathematical expression string to compile.
 * @return BytecodeChunk* Pointer to the compiled chunk on the heap, or NULL on failure.
 */
BytecodeChunk* compile_expression_to_bytecode(const char *expression) {
    if (!expression) {
        fprintf(stderr, "Compile Error: NULL expression string provided.\n");
        return NULL;
    }
    // --- Tokenize ---
    Token tokens[256]; // Adjust size if needed
    int tokenCount = tokenize(expression, tokens, 256);
    if (tokenCount < 0) {
        fprintf(stderr, "Tokenization failed for: %s\n", expression);
        return NULL;
    }
    // --- Parse ---
    int pos = 0;
    Node *ast = parseExpression(tokens, &pos);
    if (!ast || tokens[pos].type != TOKEN_END) {
        fprintf(stderr, "Parsing failed near token %d ('%s') for: %s\n", pos, tokens[pos].value, expression);
        freeAST(ast); // Clean up potentially partially parsed AST
        return NULL;
    }
    // --- Allocate Chunk on Heap ---
    BytecodeChunk *new_chunk = (BytecodeChunk*)malloc(sizeof(BytecodeChunk));
    if (!new_chunk) {
        perror("Failed to allocate memory for bytecode chunk");
        freeAST(ast);
        return NULL;
    }
    // --- Compile AST to Bytecode ---
    bool compile_ok = compile_ast_to_bytecode(ast, new_chunk);
    // --- Free AST (No longer needed) ---
    freeAST(ast);
    ast = NULL;
    // --- Handle Compilation Result ---
    if (!compile_ok) {
        fprintf(stderr, "Compilation failed for: %s\n", expression);
        free_bytecode_chunk(new_chunk); // Free internal data if any
        free(new_chunk); // Free the just-allocated chunk struct
        return NULL;
    }
    // --- Success ---
    return new_chunk; // Return pointer to the heap-allocated, compiled chunk
}

/**
 * @brief Determines the size in bytes of a single bytecode instruction.
 *
 * Helper function for count_bytecode_instructions.
 * Reads the opcode at the given offset and determines how many bytes
 * (including operands) that single instruction occupies.
 *
 * @param chunk The bytecode chunk containing the instruction.
 * @param offset The starting offset of the instruction's opcode within chunk->code.
 * @return The total size of the instruction in bytes, or 1 if unknown (to prevent infinite loops).
 */
static int get_instruction_size(const BytecodeChunk *chunk, int offset) {
    if (!chunk || offset < 0 || offset >= chunk->code_count) { return 1; }
    OpCode instruction = (OpCode)chunk->code[offset];
    int size = 1; // Start with opcode size
    switch (instruction) {
        // Instructions with 2-byte operand (uint16_t index or int16_t offset)
        case OP_PUSH_CONST:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
            size += 2;
            break;
        // Instructions with 2 single-byte operands
        case OP_CALL:
             size += 2; // func_id + arg_count
             break;
        // Instructions with 1 single-byte operand
        case OP_PUSH_LOOP_VAR:
            size += 1; // name_index
            break;
        // Special case: OP_SIGMA_EXEC
        case OP_SIGMA_EXEC:
            size += 1 + 2 + 2 + 2 + 2; // name_index + 4 * short_idx
            break;
        // Instructions with NO operands (size is already 1)
        case OP_PUSH_VAR_X:
        case OP_PUSH_VAR_FREQ:
        case OP_PUSH_VAR_RAND:
        case OP_PUSH_VAR_MOD_A:
        case OP_PUSH_VAR_MOD_B:
        case OP_PUSH_VAR_MOD_C:
        case OP_POP:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_NEGATE:
        case OP_NOT:
        case OP_CMP_EQ:
        case OP_CMP_NE:
        case OP_CMP_GT:
        case OP_CMP_GE:
        case OP_CMP_LT:
        case OP_CMP_LE:
        case OP_HALT:
            // Size remains 1
            break;
        // Handle the setup instruction correctly, although unlikely to be used in final code
        case OP_SIGMA_SETUP:
             size += 1 + 2 + 2 + 2 + 2; // Placeholder name_id + 4 * short_idx
             break;
        default:
            fprintf(stderr, "Warning (get_instruction_size): Unknown opcode 0x%02X at offset %d. Assuming size 1.\n", instruction, offset);
            // Keep size = 1 to try and continue
            break;
    }
    // Basic sanity check to prevent reading past the end if size calculation is wrong
    if (offset + size > chunk->code_count && offset + size > 0 /* prevent underflow with large negative offset */) {
        // This shouldn't happen if bytecode is generated correctly, but acts as a safeguard.
        // It likely means the last instruction was truncated.
        return chunk->code_count - offset;
    }
    return size;
}

/**
 * @brief Counts the number of bytecode instructions in a given BytecodeChunk.
 *
 * Iterates through the bytecode, determining the size of each instruction
 * (opcode + operands) and increments a counter for each valid instruction found.
 * Stops at OP_HALT or the end of the code buffer.
 *
 * @param chunk Pointer to the BytecodeChunk to analyze. Must not be NULL.
 * @return The total number of instructions found, or 0 if the chunk is NULL or empty, or -1 on error (like unknown opcode).
 */
int count_bytecode_instructions(const BytecodeChunk *chunk) {
    if (!chunk || chunk->code_count == 0) { return 0; }
    int instruction_count = 0;
    int pc = 0; // Program counter / offset
    while (pc < chunk->code_count) {
        OpCode current_op = (OpCode)chunk->code[pc];
        instruction_count++; // Count this instruction
        // Check for halt explicitly to stop counting early if desired
        if (current_op == OP_HALT) { break; }
        int size = get_instruction_size(chunk, pc);
        if (size <= 0) {
             fprintf(stderr, "Error (count_bytecode_instructions): Invalid instruction size %d for opcode 0x%02X at offset %d. Aborting count.\n", size, current_op, pc);
             return -1; // Indicate an error
        }
        // Check if calculated size goes beyond the buffer (redundant if get_instruction_size handles it, but safer)
        if (pc + size > chunk->code_count) {
             fprintf(stderr, "Error (count_bytecode_instructions): Instruction at offset %d (opcode 0x%02X, size %d) exceeds code count %d. Count may be incomplete.\n", pc, current_op, size, chunk->code_count);
             // Return current count or -1? Returning current count might be more informative.
             break; // Stop counting
        }
        pc += size; // Move to the next instruction
    }
    return instruction_count;
}

/**
 * @brief Generates a single float sample for a specific index within a waveform,
 *        using pre-compiled bytecode.
 *
 * This function calculates the appropriate phase (0 to 2*PI) corresponding
 * to the sample_index within the given wave_length and executes the VM.
 * The LFSR state within `VmParams` will be initialized to default values for each call.
 * If persistent LFSR state across calls is needed, the caller must manage `VmParams` externally.
 *
 * @param chunk Pointer to the valid, compiled BytecodeChunk for the waveform.
 * @param sample_index The index (0 to wave_length - 1) of the desired sample.
 * @param wave_length The total number of samples in the conceptual wave cycle.
 * @param frequency The frequency (in Hz) of the note being generated.
 * @param rand_offset A per-wave random float (0.0 to 1.0) passed to the VM.
 * @param modA Modulation value A passed to the VM.
 * @param modB Modulation value B passed to the VM.
 * @param modC Modulation value C passed to the VM.
 * @return float The calculated sample value, clamped between -1.0 and 1.0.
 *         Returns 0.0f on error.
 */
float generate_sample_from_bytecode(BytecodeChunk *chunk, uint16_t sample_index, uint16_t wave_length, float frequency, float rand_offset, float modA, float modB, float modC) {
    // --- Input Validation ---
    if (!chunk) { fprintf(stderr, "Error (generate_sample_from_bytecode): NULL chunk provided.\n"); return 0.0f; }
    if (wave_length == 0) { fprintf(stderr, "Error (generate_sample_from_bytecode): wave_length cannot be zero.\n"); return 0.0f; }
    if (sample_index > wave_length) {
        fprintf(stderr, "Error (generate_sample_from_bytecode): sample_index (%u) out of bounds for wave_length (%u).\n", sample_index, wave_length);
        return 0.0f;
    }

    // --- Phase Calculation ---
    float normalized_phase;
    if (wave_length > 0) {
        normalized_phase = (float)sample_index / (float)wave_length;
    } else {
        normalized_phase = 0.0f;
    }
    float x_radians = normalized_phase * C_TWO_PI;

    // --- VM Execution with Enhanced VmParams ---
    VmParams params = {
        .x = x_radians,
        .frequency = frequency,
        .rand_offset = rand_offset,
        .modA = modA,
        .modB = modB,
        .modC = modC,
        .lfsr_state = 1,        // Default initial state for free-running LFSR
        .lfsr_type = LFSR_8BIT, // Default LFSR type, script can use other types
        .lfsr_position = 0,
        .lfsr_seed = 1          // Default seed
    };

    float sample_f = execute_bytecode(chunk, &params); // Pass non-const pointer

    // --- Clamping ---
    sample_f = fmaxf(-1.0f, fminf(1.0f, sample_f));

    return sample_f;
}


// --- Initialization Function ---
/*
static void initialize_wave_tables_interpreted(polysonix_device *dev, uint16_t base_addr, uint16_t wave_length, uint8_t num_tables) {
    // Use NUM_DEFAULT_WAVES for bounds checking against the definition array
    if (!dev || wave_length == 0 || num_tables == 0 || num_tables > NUM_DEFAULT_WAVES) { // Corrected
        fprintf(stderr, "Error: Invalid arguments to initialize_wave_tables_interpreted (dev=%p, num_tables=%u, max=%d).\n", (void*)dev, num_tables, NUM_DEFAULT_WAVES);
        return;
    }
    printf("Compiling & Generating %u wave tables of length %u at 0x%04X...\n", num_tables, wave_length, base_addr);
    // Seed random ONCE elsewhere
    for (int table_idx = 0; table_idx < num_tables; ++table_idx) {
        // table_idx is already checked against NUM_DEFAULT_WAVES by the initial check
        uint32_t table_start_offset = (uint32_t)(base_addr - USER_WAVE_RAM_START) + ((uint32_t)table_idx * wave_length * sizeof(float)); // Scaled by float
        if (table_start_offset + wave_length * sizeof(float) > USER_WAVE_RAM_SIZE) {
            fprintf(stderr, "Error: Wave table %d (length %u) exceeds user_wave_ram bounds (Offset 0x%X).\n", table_idx, wave_length, table_start_offset);
            continue;
        }
        const char* wave_name = default_waves[table_idx].name;
        const char* expression_to_compile = default_waves[table_idx].expression;
        float *output_buffer = (float *)(dev->user_wave_ram + table_start_offset);

        if (default_waves[table_idx].compiled_bytecode != NULL) {
            // This case should ideally not happen in a single-init scenario,
            // but adding a warning and freeing is safer if the assumption is wrong.
            fprintf(stderr, "Warning: Wave %d ('%s') appears already compiled during init. Freeing old chunk.\n", table_idx, wave_name);
            free_bytecode_chunk(default_waves[table_idx].compiled_bytecode);
            free(default_waves[table_idx].compiled_bytecode);
            default_waves[table_idx].compiled_bytecode = NULL;
        }

        // Compile and store the result (will be NULL if compilation fails)
        default_waves[table_idx].compiled_bytecode = compile_expression_to_bytecode(expression_to_compile);

        // --- Generate Wave Data (Only if compilation was successful) ---
        // This check is still essential
        if (default_waves[table_idx].compiled_bytecode != NULL) {
            BytecodeChunk* current_chunk = default_waves[table_idx].compiled_bytecode;
            //float generation_rand_offset = (float)rand() / RAND_MAX;
            for(uint16_t i = 0; i < wave_length; ++i) {
                float sample_float = generate_sample_from_bytecode( current_chunk, i, wave_length, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
                output_buffer[i] = sample_float;
            }
            // Optional boundary matching after the loop
            if (wave_length > 1) { output_buffer[wave_length - 1] = output_buffer[0]; }
            const int fade_len = 8; // How many samples to fade over (adjust if needed)
            if (wave_length > fade_len * 2) { // Ensure wave is long enough for fade
                for (int j = 0; j < fade_len; ++j) {
                    // Index near the end (where the fade happens)
                    int end_idx = wave_length - fade_len + j;
                    // Corresponding index at the start (target of the fade)
                    int start_idx = j;
                    // Get the original sample value near the end (before blending)
                    // Note: output_buffer[wave_length-1] was already forced equal to output_buffer[0]
                    float end_sample = output_buffer[end_idx];
                    // Get the sample value from the start that we are fading towards
                    float start_sample = output_buffer[start_idx];
                    // Calculate linear fade factor (alpha goes 0 to 1)
                    // When j=0, alpha=0. When j=fade_len-1, alpha=1.
                    float alpha = (float)j / (float)(fade_len - 1);
                    // Perform linear interpolation
                    float blended_sample = (1.0f - alpha) * end_sample + alpha * start_sample;
                    // Store the blended sample back, rounding correctly
                    // Requires <math.h> for roundf()
                    output_buffer[end_idx] = blended_sample;
                }
                // Optional: Force the very last sample *again* after the fade? Usually not needed.
                // output_buffer[wave_length - 1] = output_buffer[0];
            }
        } else {
            // Compilation failed (error message already printed by compile function)
            fprintf(stderr, "-> Compilation failed for wave '%s' (idx %d), filling buffer with silence.\n", wave_name, table_idx);
            memset(output_buffer, 0, wave_length * sizeof(float));
        }
    }
    printf("Finished initializing %u wave tables of length %u.\n", num_tables, wave_length);
}
*/

/**
 * @brief Retrieves or compiles the bytecode for a given default wave definition.
 *
 * This function implements a multi-level caching and compilation strategy:
 * 1. Checks if the `default_waves[wave_index].compiled_bytecode` pointer is already populated.
 *    If so, it returns the existing compiled chunk (fastest path).
 * 2. If not found in the `default_waves` array, it queries the global `bytecode_cache`
 *    using the wave's expression string as the key.
 *    If found in the cache, the pointer is stored back into `default_waves[wave_index]`
 *    for future faster access, and the cached chunk is returned.
 * 3. If the bytecode is neither in the `default_waves` array nor in the global cache,
 *    the function proceeds to compile the wave's expression string:
 *    - It calls `compile_expression_to_bytecode()` which tokenizes, parses, and
 *      compiles the expression into a new, heap-allocated `BytecodeChunk`.
 *    - If compilation is successful:
 *        - The pointer to the new chunk is stored in `default_waves[wave_index]`.
 *        - The new chunk is also inserted into the `bytecode_cache` for potential
 *          reuse if the same expression string appears elsewhere (though less likely
 *          for unique default waves, it's good practice for a general cache).
 *        - The newly compiled chunk is returned.
 *    - If compilation fails, an error message is printed, and NULL is returned.
 *
 * The `BytecodeChunk` returned by this function (if not NULL) is owned by
 * the `default_waves` array entry and potentially also referenced by the cache.
 * It should NOT be freed directly by the caller of this function.
 * Global cleanup (`cleanup_resources()`) is responsible for freeing all
 * `compiled_bytecode` chunks stored in `default_waves` and clearing the cache.
 *
 * It is assumed that `initialize_bytecode_cache()` has been called prior to
 * the first invocation of this function.
 *
 * @param wave_index The index of the wave definition in the `default_waves` array.
 *                   Must be within the valid range [0, NUM_DEFAULT_WAVES - 1].
 * @return BytecodeChunk* Pointer to the compiled bytecode chunk for the requested wave.
 *         Returns NULL if the `wave_index` is invalid, or if compilation fails.
 */
 /*
BytecodeChunk* get_or_compile_wave_bytecode(int wave_index) {
    if (wave_index < 0 || wave_index >= NUM_DEFAULT_WAVES) {
        fprintf(stderr, "Error: Invalid wave index %d.\n", wave_index);
        return NULL;
    }

    // 1. Check the definition array first (primary storage after initial compile)
    //    Requires default_waves to be non-const.
    if (default_waves[wave_index].compiled_bytecode != NULL) {
        return default_waves[wave_index].compiled_bytecode;
    }

    // 2. If not in the array, check the cache (secondary storage / lookup)
    //    Ensure cache is initialized before calling this.
    const char* expression = default_waves[wave_index].expression;
    BytecodeChunk* cached_chunk = lookup_cache(expression);
    if (cached_chunk) {
        // Found in cache. Store it back in the definition array for faster access next time.
        default_waves[wave_index].compiled_bytecode = cached_chunk; // OK now because non-const
        return cached_chunk;
    }

    // 3. Not in array or cache, compile it now.
    printf("Compiling wave %d ('%s') on demand...\n", wave_index, default_waves[wave_index].name);
    BytecodeChunk *compiled_chunk = compile_expression_to_bytecode(expression);

    if (compiled_chunk) {
        // 4. Compilation succeeded. Store in both locations.
        default_waves[wave_index].compiled_bytecode = compiled_chunk; // Store in definition array

        // Attempt to insert into the cache. insert_cache should handle its own errors.
        // Assume insert_cache does NOT take ownership of the compiled_chunk pointer itself,
        // but duplicates the expression string and stores the pointer.
        insert_cache(expression, compiled_chunk);

        return compiled_chunk;
    } else {
        // 5. Compilation failed.
        // Error message already printed by compile_expression_to_bytecode
        return NULL; // Return NULL to signal failure
    }
}*/

// --- Other Functions (initialize_wave_tables_interpreted, main, etc.) ---
// Example of how to use the cache cleanup at program exit
/*
void cleanup_resources() {
     // Free compiled bytecode chunks stored in default_waves
     printf("Freeing compiled wave definitions...\n");
     size_t freed_count = 0;
     for (size_t i = 0; i < NUM_DEFAULT_WAVES; ++i) {
         if (default_waves[i].compiled_bytecode != NULL) {
             free_bytecode_chunk(default_waves[i].compiled_bytecode); // Free internal data
             free(default_waves[i].compiled_bytecode);                // Free the main chunk struct
             default_waves[i].compiled_bytecode = NULL; // Clear the pointer in the struct
             freed_count++;
         }
     }
     printf("Freed %zu compiled wave definition chunks.\n", freed_count);
}*/
// Ensure initialize_wave_tables_interpreted calls the generate_wave_from_expression. The caching logic is encapsulated within generate_wave_from_expression.
// Assume default_waves structure and data exists
// typedef struct { const char* name; const char* expression; } DefaultWaveDef;
// extern DefaultWaveDef default_waves[]; // Needs actual definition

// The rest of your original code (structs, defines, etc.) should precede this.
// You will need to integrate the Bytecode/VM/Cache code sections appropriately.
// Remember to provide implementations for:
// - OpCode enum
// - BytecodeChunk struct fields
// - FunctionID enum
// - VmFunctionDef struct data
// - VM struct fields
// - init_bytecode_chunk()
// - free_bytecode_chunk() // Frees *internal* data
// - compile_ast_to_bytecode()
// - execute_bytecode()
// - freeAST()
// - All the parser functions (parseExpression, etc.)
// - createNode()
// - tokenize()
// - The various structs and defines from the original header.


// --- Example Main/Setup ---
/*
int main() {
    // --- Initialization ---
    polysonix_device dev;
    dev.user_wave_ram = (uint8_t*)malloc(USER_WAVE_RAM_SIZE);
    if (!dev.user_wave_ram) {
        perror("Failed to allocate wave RAM");
        return 1;
    }
    memset(dev.user_wave_ram, 0, USER_WAVE_RAM_SIZE); // Clear RAM

    // Seed random number generator ONCE
    srand((unsigned int)time(NULL));

    // Initialize all wave tables
    printf("Initializing 256-byte waves...\n");
    initialize_wave_tables_interpreted(&dev, 0x0000, 256, 64); // Assuming USER_WAVE_256_BASE is 0x0000 relative to Bank3 start
    printf("Initializing 128-byte waves...\n");
    initialize_wave_tables_interpreted(&dev, 0x4000, 128, 64); // Assuming USER_WAVE_128_BASE is 0x4000 relative to Bank3 start
    printf("Initializing 64-byte waves...\n");
    initialize_wave_tables_interpreted(&dev, 0x6000, 64, 64);  // Assuming USER_WAVE_64_BASE is 0x6000 relative to Bank3 start
    printf("Initializing 32-byte waves...\n");
    initialize_wave_tables_interpreted(&dev, 0x7000, 32, 64);  // Assuming USER_WAVE_32_BASE is 0x7000 relative to Bank3 start

    // --- Synthesizer Runtime ---
    // ... use the generated waves ...


    // --- Cleanup ---
    free(dev.user_wave_ram);
    return 0;
}
*/



// --- GPU Structures (Match GLSL Layout) ---

#ifdef POLYSONIX_USE_GPU

typedef struct {
    float x;
    float frequency;
    float rand_offset;
    float modA;
    float modB;
    float modC;
    // Free-running LFSR configuration
    int32_t lfsr_type;          // 4 bytes
    uint32_t lfsr_seed;         // 4 bytes
    uint32_t wave_length;       // 4 bytes
} VmParamsBuffer;

// Structure for the persistent LFSR state buffer (read-write)
// Matches layout(buffer_reference, scalar) buffer LfsrStateBuffer in shader
typedef struct {
    uint32_t state;
    uint32_t position;
    float accum_phase;
} LfsrState;

typedef struct {
    uint32_t main_chunk_offset;
    uint32_t sigma_offsets_0_3[4];
    uint32_t sigma_offsets_4_7[4];
    uint32_t sigma_offsets_8_11[4];
    uint32_t sigma_offsets_12_15[4];
    uint32_t lfsr_periods[4][4];
    uint32_t lfsr_offsets[4][4];
    uint32_t lfsr_tap_masks[4][4];
    uint32_t lfsr_bit_lengths[4][4];
} VmMetadataBuffer;

typedef struct {
    uint64_t vm_params;
    uint64_t vm_metadata;
    uint64_t bytecode;
    uint64_t constants;
    uint64_t lfsr_tables;
    uint64_t output_buffer;
    uint64_t lfsr_state;
} PushConstants;

// --- GPU Globals ---
static SituationComputePipeline wave_compute_pipeline;
static SituationBuffer lfsr_tables_buffer;
static bool gpu_resources_initialized = false;

// --- GPU Helper Functions ---

void px_vm_init_gpu_resources(void) {
    if (gpu_resources_initialized) return;

    if (SituationCreateComputePipeline("px_vm.comp", SITUATION_COMPUTE_LAYOUT_SCALAR, &wave_compute_pipeline) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to create compute pipeline for px_vm.comp\n");
        return;
    }

    if (!precomputed_lfsrs[0].initialized) px_vm_init_lfsr_tables();

    size_t total_lfsr_bytes = 0;
    for (int i = 0; i < NUM_LFSR_TYPES; ++i) {
        size_t bytes = LFSR_TABLE_BYTES(precomputed_lfsrs[i].period);
        if (bytes % 4 != 0) bytes += (4 - (bytes % 4));
        total_lfsr_bytes += bytes;
    }

    uint8_t* host_lfsr = (uint8_t*)calloc(1, total_lfsr_bytes);
    if (host_lfsr) {
        size_t offset = 0;
        for (int i = 0; i < NUM_LFSR_TYPES; ++i) {
            size_t bytes = LFSR_TABLE_BYTES(precomputed_lfsrs[i].period);
            if (bytes > 0 && precomputed_lfsrs[i].bit_table) memcpy(host_lfsr + offset, precomputed_lfsrs[i].bit_table, bytes);
            if (bytes % 4 != 0) bytes += (4 - (bytes % 4));
            offset += bytes;
        }
        SituationCreateBuffer(total_lfsr_bytes, host_lfsr, SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST, &lfsr_tables_buffer);
        free(host_lfsr);
    }
    gpu_resources_initialized = true;
    printf("Polysonix GPU resources initialized.\n");
}

void px_vm_cleanup_gpu_resources(void) {
    if (lfsr_tables_buffer.id != 0) SituationDestroyBuffer(&lfsr_tables_buffer);
    gpu_resources_initialized = false;
}

static size_t serialize_chunk_recursive(BytecodeChunk* chunk, uint8_t* buffer, size_t* current_offset, VmMetadataBuffer* meta, bool is_root) {
    if (!chunk) return 0;

    size_t code_start = *current_offset;
    if (buffer) memcpy(buffer + code_start, chunk->code, chunk->code_count);
    *current_offset += chunk->code_count;

    while ((*current_offset) % 4 != 0) {
        if (buffer) buffer[*current_offset] = OP_HALT;
        (*current_offset)++;
    }

    if (is_root && meta) meta->main_chunk_offset = (uint32_t)code_start;

    for (int i = 0; i < chunk->sigma_sub_chunk_count; ++i) {
        size_t sub_offset = *current_offset;
        if (is_root && meta && i < 16) {
             uint32_t* offsets = meta->sigma_offsets_0_3;
             offsets[i] = (uint32_t)sub_offset;
        }
        serialize_chunk_recursive(chunk->sigma_sub_chunks[i], buffer, current_offset, meta, false);
    }
    return *current_offset;
}

typedef struct {
    SituationBuffer bytecode;
    SituationBuffer constants;
    SituationBuffer metadata;
} GpuWaveBuffers;

GpuWaveBuffers upload_wave_to_gpu(BytecodeChunk* chunk) {
    GpuWaveBuffers gpu_bufs = {0};
    if (!chunk) return gpu_bufs;

    VmMetadataBuffer meta = {0};
    size_t lfsr_offset = 0;
    for (int i = 0; i < NUM_LFSR_TYPES; ++i) {
        meta.lfsr_periods[i/4][i%4] = precomputed_lfsrs[i].period;
        meta.lfsr_offsets[i/4][i%4] = (uint32_t)(lfsr_offset / 4);
        meta.lfsr_tap_masks[i/4][i%4] = lfsr_configs[i].tap_mask;
        meta.lfsr_bit_lengths[i/4][i%4] = lfsr_configs[i].bit_length;
        size_t bytes = LFSR_TABLE_BYTES(precomputed_lfsrs[i].period);
        if (bytes % 4 != 0) bytes += (4 - (bytes % 4));
        lfsr_offset += bytes;
    }

    size_t size = 0;
    serialize_chunk_recursive(chunk, NULL, &size, NULL, true);

    uint8_t* code_bytes = (uint8_t*)calloc(1, size);
    size_t actual_size = 0;
    serialize_chunk_recursive(chunk, code_bytes, &actual_size, &meta, true);

    SituationCreateBuffer(size, code_bytes, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &gpu_bufs.bytecode);
    SituationCreateBuffer(chunk->constants_count * sizeof(float), chunk->constants, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &gpu_bufs.constants);
    SituationCreateBuffer(sizeof(VmMetadataBuffer), &meta, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &gpu_bufs.metadata);

    free(code_bytes);
    return gpu_bufs;
}

// Dispatch compute for a wave (Records command to cmd buffer)
void dispatch_wave_gpu(SituationCommandBuffer cmd, GpuWaveBuffers bufs, VmParams* params, uint32_t wave_length, SituationBuffer output_buf, SituationBuffer lfsr_state_buf, SituationBuffer* out_params_buf) {
    if (!gpu_resources_initialized) px_vm_init_gpu_resources();

    VmParamsBuffer pb = {
        .x = params->x, .frequency = params->frequency, .rand_offset = params->rand_offset,
        .modA = params->modA, .modB = params->modB, .modC = params->modC,
        .lfsr_type = (int32_t)params->lfsr_type,
        .lfsr_seed = params->lfsr_seed,
        .wave_length = wave_length
    };

    SituationCreateBuffer(sizeof(VmParamsBuffer), &pb, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, out_params_buf);

    PushConstants pc = {
        .vm_params = SituationGetBufferDeviceAddress(*out_params_buf),
        .vm_metadata = SituationGetBufferDeviceAddress(bufs.metadata),
        .bytecode = SituationGetBufferDeviceAddress(bufs.bytecode),
        .constants = SituationGetBufferDeviceAddress(bufs.constants),
        .lfsr_tables = SituationGetBufferDeviceAddress(lfsr_tables_buffer),
        .output_buffer = SituationGetBufferDeviceAddress(output_buf),
        .lfsr_state = SituationGetBufferDeviceAddress(lfsr_state_buf)
    };

    SituationCmdBindComputePipeline(cmd, wave_compute_pipeline);
    SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(PushConstants));
    SituationCmdDispatch(cmd, (wave_length + 63) / 64, 1, 1);
}

#endif // POLYSONIX_USE_GPU

#ifdef __cplusplus
}
#endif

#endif // PX_VM_H
