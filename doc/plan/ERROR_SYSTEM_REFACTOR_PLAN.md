# Error System Refactor Plan

## Goal

Refactor `SituationError` from a simple enum to a structure that contains both the error code and message, eliminating the "separated twins" problem where error codes and messages are managed separately.

## Current System Analysis

### Current Structure

```c
// situation_api.h
typedef enum {
    SITUATION_SUCCESS = 0,
    SITUATION_ERROR_GENERAL = -1,
    SITUATION_ERROR_NOT_IMPLEMENTED = -2,
    // ... ~200+ error codes
} SituationError;

// situation_impl.h
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail) {
    char buffer[SITUATION_MAX_ERROR_MSG_LEN];
    const char* base_msg = "Unknown Error";
    
    switch (err) {
        case SITUATION_SUCCESS: base_msg = "No error"; break;
        case SITUATION_ERROR_GENERAL: base_msg = "A general error occurred"; break;
        // ... ~200+ case statements mapping codes to messages
    }
    
    // Format message with detail
    // Store in global state
}

// Global error state
char sit_gs.last_error_msg[SITUATION_MAX_ERROR_MSG_LEN];
```

### Current API

```c
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg);
```

### Problems with Current System

1. **Separated Twins**: Error codes and messages are defined in two different places
2. **Maintenance Burden**: Adding a new error requires updating both enum and switch statement
3. **No Compile-Time Validation**: Easy to forget to add a message for a new error code
4. **Large Switch Statement**: 200+ case statements in one function
5. **Global State**: Error message stored in global buffer, not thread-safe per error

## Proposed New System

### New Structure

```c
// situation_api.h
typedef struct {
    int code;                                    // Error code (0 = success, negative = error)
    const char* message;                         // Static error message (never NULL)
} SituationError;

// Predefined error constants (compile-time initialized)
#define SITUATION_SUCCESS                    ((SituationError){0, "No error"})
#define SITUATION_ERROR_GENERAL              ((SituationError){-1, "A general error occurred"})
#define SITUATION_ERROR_NOT_IMPLEMENTED      ((SituationError){-2, "Feature not implemented for current backend"})
// ... all error codes with messages in one place

// Helper macros for common patterns
#define SITUATION_ERROR_IS_SUCCESS(err)      ((err).code == 0)
#define SITUATION_ERROR_IS_FAILURE(err)      ((err).code != 0)
#define SITUATION_ERROR_CODE(err)            ((err).code)
#define SITUATION_ERROR_MESSAGE(err)         ((err).message)
```

### Benefits

1. **Single Source of Truth**: Code and message defined together
2. **Compile-Time Safety**: Can't define a code without a message
3. **No Switch Statement**: Messages are part of the constant definition
4. **Better Ergonomics**: `error.message` instead of calling a function
5. **Thread-Safe**: Each error carries its own message
6. **Backward Compatible**: Can provide compatibility layer

## Migration Strategy

### Phase 1: Add New System Alongside Old (Non-Breaking)

```c
// situation_api.h - Add new structure
typedef struct {
    int code;
    const char* message;
} SituationErrorEx;  // "Ex" for extended

// Keep old enum for compatibility
typedef enum {
    SITUATION_SUCCESS = 0,
    SITUATION_ERROR_GENERAL = -1,
    // ...
} SituationErrorCode;  // Rename from SituationError

// Compatibility typedef
typedef SituationErrorCode SituationError;  // Old code still works

// New error constants
#define SITUATION_ERROR_EX_SUCCESS ((SituationErrorEx){0, "No error"})
#define SITUATION_ERROR_EX_GENERAL ((SituationErrorEx){-1, "A general error occurred"})
// ...

// Conversion helpers
static inline SituationErrorEx SituationErrorToEx(SituationErrorCode code) {
    // Use existing _SituationSetErrorFromCode logic
}

static inline SituationErrorCode SituationErrorFromEx(SituationErrorEx err) {
    return (SituationErrorCode)err.code;
}
```

### Phase 2: Update Internal Functions

```c
// Old internal function
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail);

// New internal function
static SituationErrorEx _SituationMakeError(int code, const char* base_msg, const char* detail);

// Gradually migrate internal code to use SituationErrorEx
```

### Phase 3: Update Public API (Breaking Change)

```c
// Old API (deprecated)
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg);

// New API
SITAPI SituationErrorEx SituationGetLastError(void);

// Usage
SituationErrorEx err = SituationGetLastError();
if (SITUATION_ERROR_IS_FAILURE(err)) {
    printf("Error %d: %s\n", err.code, err.message);
}
```

### Phase 4: Remove Old System

- Remove `SituationErrorCode` enum
- Rename `SituationErrorEx` to `SituationError`
- Remove compatibility layer
- Update all documentation

## Implementation Details

### Error Definition Pattern

```c
// Core errors (0-99)
#define SITUATION_SUCCESS                           ((SituationError){0, "No error"})
#define SITUATION_ERROR_GENERAL                     ((SituationError){-1, "A general error occurred"})
#define SITUATION_ERROR_NOT_IMPLEMENTED             ((SituationError){-2, "Feature not implemented for current backend"})
#define SITUATION_ERROR_NOT_INITIALIZED             ((SituationError){-3, "Function called before library initialization"})
#define SITUATION_ERROR_ALREADY_INITIALIZED         ((SituationError){-4, "SituationInit() called more than once"})

// Threading errors (80-99)
#define SITUATION_ERROR_THREAD_QUEUE_FULL           ((SituationError){-80, "Threading: Thread queue full"})
#define SITUATION_ERROR_THREAD_VIOLATION            ((SituationError){-81, "Threading: Main-thread-only function called from worker thread"})
#define SITUATION_ERROR_THREAD_CYCLE                ((SituationError){-82, "Threading: Dependency cycle or depth limit exceeded"})

// Audio mixer errors (440-459)
#define SITUATION_ERROR_MIXER_NOT_INITIALIZED       ((SituationError){-440, "Mixer: Not initialized"})
#define SITUATION_ERROR_MIXER_TRACK_LIMIT           ((SituationError){-441, "Mixer: Maximum number of tracks reached"})
#define SITUATION_ERROR_MIXER_TRACK_INVALID         ((SituationError){-442, "Mixer: Invalid track ID or track not active"})

// ... etc for all ~200 error codes
```

### Error Creation with Detail

```c
// Helper function for adding detail to base error
static inline SituationError SituationErrorWithDetail(SituationError base, const char* detail) {
    if (!detail || detail[0] == '\0') {
        return base;
    }
    
    // Allocate formatted message (needs memory management strategy)
    static char buffer[SITUATION_MAX_ERROR_MSG_LEN];
    snprintf(buffer, sizeof(buffer), "%s: %s", base.message, detail);
    
    return (SituationError){base.code, buffer};
}

// Usage
return SituationErrorWithDetail(SITUATION_ERROR_FILE_NOT_FOUND, filename);
// Returns: {-310, "File does not exist: myfile.txt"}
```

### Memory Management Considerations

**Option A: Static Buffer (Current Approach)**
- Pro: Simple, no allocation
- Con: Not thread-safe, detail gets overwritten

**Option B: Thread-Local Storage**
- Pro: Thread-safe
- Con: Requires C11 `_Thread_local`

**Option C: Caller-Provided Buffer**
```c
char detail_buffer[256];
SituationError err = SituationErrorWithDetailBuffer(
    SITUATION_ERROR_FILE_NOT_FOUND, 
    filename,
    detail_buffer,
    sizeof(detail_buffer)
);
```

**Option D: Two-Tier System**
```c
typedef struct {
    int code;
    const char* message;      // Static base message
    char detail[256];         // Optional detail (empty string if none)
} SituationError;

#define SITUATION_ERROR_FILE_NOT_FOUND ((SituationError){-310, "File does not exist", ""})

// With detail
SituationError err = SITUATION_ERROR_FILE_NOT_FOUND;
snprintf(err.detail, sizeof(err.detail), "%s", filename);
```

## Compatibility Layer

```c
// For old code that uses integer error codes
#define SITUATION_ERROR_CODE_GENERAL              (-1)
#define SITUATION_ERROR_CODE_NOT_IMPLEMENTED      (-2)
// ...

// Conversion function
static inline SituationError SituationErrorFromCode(int code) {
    // Map code to predefined error constant
    switch (code) {
        case 0: return SITUATION_SUCCESS;
        case -1: return SITUATION_ERROR_GENERAL;
        case -2: return SITUATION_ERROR_NOT_IMPLEMENTED;
        // ...
        default: return (SituationError){code, "Unknown error"};
    }
}
```

## Testing Strategy

1. **Unit Tests**: Test error creation, comparison, message retrieval
2. **Integration Tests**: Test error propagation through API
3. **Compatibility Tests**: Ensure old code still compiles and works
4. **Thread Safety Tests**: Verify thread-local storage works correctly

## Rollout Plan

### Week 1: Design & Prototype
- Finalize structure design
- Create prototype with ~20 error codes
- Test ergonomics and performance

### Week 2: Full Implementation
- Define all ~200 error constants
- Implement helper functions
- Create compatibility layer

### Week 3: Internal Migration
- Update internal functions to use new system
- Keep public API unchanged
- Test thoroughly

### Week 4: Public API Update
- Add new public API functions
- Deprecate old functions
- Update documentation and examples

### Week 5: Cleanup
- Remove deprecated functions
- Remove compatibility layer
- Final testing and release

## Open Questions

1. **Memory Management**: Which option for detail strings?
2. **Thread Safety**: Use thread-local storage or accept non-thread-safe details?
3. **Backward Compatibility**: How long to maintain compatibility layer?
4. **Performance**: Any performance impact from struct vs enum?
5. **ABI Stability**: How does this affect DLL boundaries?

## Recommendation

Start with **Phase 1** (non-breaking addition) to validate the design without disrupting existing code. This allows us to:
- Test the new system in parallel
- Gather feedback on ergonomics
- Identify any unforeseen issues
- Migrate gradually without breaking existing code

Once validated, proceed with phases 2-4 over multiple releases.
