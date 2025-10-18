/**
 * build_dll.c - Compilation unit for building situation.h as a shared library (DLL).
 *
 * This file's sole purpose is to define the macros required to instantiate
 * the implementation of the situation.h library and mark its functions for
 * export from the DLL.
 *
 * To compile on Windows with GCC (MinGW):
 *   gcc -shared -o situation.dll build_dll.c glad.c -DSITUATION_BUILD_SHARED -I<path_to_cglm> -I<path_to_glad> -I<path_to_glfw> -lglfw3 -lgdi32 -lopengl32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -lxinput -lm
 *
 * To compile on Linux with GCC:
 *   gcc -shared -fPIC -o libsituation.so build_dll.c glad.c -DSITUATION_BUILD_SHARED -I<includes> -lglfw -lGL -lm -ldl -lpthread
 */

// Define this to tell situation.h that we are building it as a shared library.
// This will activate the `__declspec(dllexport)` definition for SITAPI on Windows.
#define SITUATION_BUILD_SHARED

// Define this to include the implementation of the library in this compilation unit.
#define SITUATION_IMPLEMENTATION
#include "situation.h"

// We also need to compile the GLAD loader implementation alongside our library.
// Make sure the path is correct relative to this file.
#include "ext/glad.c"

/*
How to Use build_dll.c
Let's assume the following project structure:

project_root/
├── your_game/
│   └── main.c
├── situation_lib/
│   ├── situation.h
│   ├── build_dll.c
│   └── ext/
│       ├── glad.c
│       ├── glad/
│       │   └── glad.h
│       ├── ... (miniaudio.h, cglm/, GLFW/)
│
└── (output will go here)
    ├── situation.dll
    └── your_game.exe
    
    
1. Build the DLL:
Navigate to your situation_lib directory and run the compilation command.
On Windows (using MinGW GCC):
gcc -shared -o ../situation.dll build_dll.c -DSITUATION_BUILD_SHARED -I./ext -lgdi32 -lopengl32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -lxinput -lm -lglfw3
Use code with caution.
Bash
-shared: Tells GCC to create a shared library.
-o ../situation.dll: Specifies the output file name and location.
build_dll.c: Your main compilation source.
-DSITUATION_BUILD_SHARED: This is the crucial macro that activates the dllexport mode.
-I./ext: Tells the compiler where to find cglm, glad.h, glfw3.h, etc.
-l...: Links all the necessary system and GLFW libraries. I've included the full list.
2. Compile Your Game to Use the DLL:
Navigate to your your_game directory.
In your main.c, you will now define SITUATION_USE_SHARED before including the header.
// your_game/main.c

#define SITUATION_USE_SHARED // Tell the header to import functions from a DLL
#include "../situation_lib/situation.h"

// You no longer need #define SITUATION_IMPLEMENTATION here.

int main(void) {
    // ... your game code ...
    return 0;
}
*/
