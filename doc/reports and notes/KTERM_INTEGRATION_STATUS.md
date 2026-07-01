# K-Term Integration Status

**Date**: 2026-03-03  
**Status**: Updated  
**Version**: 2.4.0

## Overview

Updated K-Term terminal emulation library integration to use correct include paths after the core headers reorganization. K-Term remains a self-contained subsystem within `sit/k-term/` with proper integration into the Situation library.

## K-Term Location

```
sit/k-term/                        # K-Term terminal emulation library
├── kterm.h                        # Main wrapper header
├── kterm_api.h                    # Public API declarations
├── kterm_impl.h                   # Implementation
├── kt_*.h                         # Component headers (parser, render, io, etc.)
├── example/                       # K-Term standalone examples
├── tests/                         # K-Term test suite
└── doc/                          # K-Term documentation
```

## Include Path Updates

### Fixed Files

Updated example files to use correct include paths after reorganization:

1. **examples/kterm_simple_test.c**
   ```c
   // Before
   #include "../kterm.h"
   
   // After
   #include "../sit/k-term/kterm.h"
   ```

2. **examples/kterm_console.c** — **K-Term core product** (KaOS Terminal). Lives under Situation examples for development; uses Situation as host (VD, window, sysinfo). Legacy `examples/console.c` merged here and removed (2026-06).
   ```c
   // Before
   #include "kterm.h"
   
   // After
   #include "sit/k-term/kterm.h"
   ```

### Already Correct

These files were already using the correct paths:

- `examples/kterm_minimal_test.c` - Uses `../sit/k-term/kterm_api.h`
- `examples/kterm_showcase.c` - Uses `../sit/k-term/kterm_api.h`

### K-Term Internal Files

Files within `sit/k-term/` subdirectories (tests/, example/) correctly use `../kterm.h` to reference the parent directory. These do NOT need updating:

- `sit/k-term/tests/*.c` - Use `../kterm.h` (correct - one level up)
- `sit/k-term/example/*.c` - Use `../kterm.h` (correct - one level up)

## Integration Pattern

K-Term integrates with Situation through two main approaches:

### 1. Full Implementation (Static Linking)
```c
#define SITUATION_IMPLEMENTATION
#define KTERM_IMPLEMENTATION
#include "../situation.h"
#include "../sit/k-term/kterm.h"
```

Used by:
- `kterm_simple_test.c`
- `kterm_console.c`

### 2. Shared Library (DLL)
```c
#define SITUATION_USE_SHARED
#include "../situation.h"
#include "../sit/k-term/kterm_api.h"
```

Used by:
- `kterm_minimal_test.c`
- `kterm_showcase.c`

## K-Term Components

### Core Headers

- **kterm.h** - Main wrapper that includes all components
- **kterm_api.h** - Public API declarations
- **kterm_impl.h** - Core implementation

### Component Headers

- **kt_parser.h** - VT/ANSI escape sequence parser
- **kt_render_sit.h** - Situation-based rendering backend
- **kt_io_sit.h** - Situation input/output adapter
- **kt_layout.h** - Terminal layout management
- **kt_composite_sit.h** - Composite rendering
- **kt_voice.h** - Voice synthesis integration
- **kt_voip.h** - VoIP functionality
- **kt_net.h** - Network utilities
- **kt_gateway.h** - Gateway functionality
- **kt_serialize.h** - State serialization
- **kt_ops.h** - Operations
- **kt_cp437_tables.h** - CP437 character tables

### Support Files

- **font_data.h** - Embedded font data
- **terminfo_data.h** - Terminal capability database
- **stb_truetype.h** - Font rendering (STB library)

## Example Programs

### Working Examples

1. **kterm_minimal_test** - Basic "Hello World" display
2. **kterm_showcase** - Rich terminal features demo (256-color, true color, attributes)
3. **kterm_simple_test** - Simple test based on console pattern
4. **kterm_console** - Full interactive console (KaOS)

### Compilation Scripts

- `compile_kterm_minimal_test.bat`
- `compile_kterm_showcase.bat`
- `compile_kterm_showcase_static.bat`
- `compile_kterm_simple_test.bat`

## K-Term Features

### Terminal Emulation

- VT100/VT220/VT320 compatibility
- ANSI escape sequence support
- 256-color palette
- True Color (24-bit RGB)
- Text attributes (bold, italic, underline, blink, reverse)
- Cursor styles and positioning
- Box drawing characters
- Scrollback buffer

### Advanced Features

- Voice synthesis integration
- VoIP functionality
- Network utilities (telnet, SSH)
- Gateway mode
- State serialization
- Sixel graphics support
- CP437 character set

### Integration with Situation

- Uses Situation's Vulkan rendering backend
- Integrates with Situation's input system
- Leverages Situation's window management
- Compatible with Situation's threading model

## Architecture

K-Term is designed as a self-contained subsystem that:

1. **Standalone Capability** - Can be used independently with its own test suite
2. **Situation Integration** - Seamlessly integrates with Situation's rendering and input
3. **Modular Design** - Components can be enabled/disabled via defines
4. **Header-Only** - Single-header library pattern with implementation guards

## Build Modes

### 1. Testing Mode (Mock Situation)
```c
#define KTERM_TESTING
#include "../sit/k-term/kterm.h"
```
- Uses mock Situation implementation
- Smaller executable (~950 KB)
- For unit testing without GPU dependencies

### 2. Production Mode (Full Situation)
```c
#include "../sit/k-term/kterm.h"
```
- Uses full Situation library
- Larger executable (~14 MB)
- Full GPU rendering support

## Status

✓ K-Term library properly located in `sit/k-term/`  
✓ Include paths updated in example files  
✓ Integration with Situation maintained  
✓ Self-contained subsystem architecture preserved  
✓ Test suite remains functional  

## Related Documentation

- `CORE_HEADERS_REORGANIZATION.md` - Core headers moved to sit/
- `FX_FOLDER_ORGANIZATION.md` - Audio effects organization
- `sit/k-term/README.md` - K-Term library documentation
- `sit/k-term/doc/kterm.md` - K-Term API reference

## Future Considerations

1. **Modular Compilation** - Consider separating K-Term components for selective linking
2. **API Stability** - Maintain stable public API in `kterm_api.h`
3. **Documentation** - Keep K-Term docs synchronized with Situation docs
4. **Testing** - Ensure K-Term tests run as part of CI/CD pipeline
