# =============================================================================
# Situation Library -- Makefile
#
# Base directory: sit/   -- project root reached via ../
# Usage (from project root via thin launcher):
#   build\build_situation.bat opengl
#   build\build_situation.bat static-vulkan
#   build\build_situation.bat all
#   build\build_situation.bat clean
#
# Or directly (from sit/):
#   make opengl
#   make static-opengl
#
# Environment overrides:
#   MINGW_PATH            Path to MinGW bin (consumed by launcher; tools must be on PATH)
#   VULKAN_SDK            Vulkan SDK root    (autodetected from C:/VulkanSDK/* if unset)
#   SIT_OPTIMIZE_CFLAGS   Default: -O2 -mfma -ffp-contract=fast
#   EXTRA_VULKAN_CFLAGS   Extra flags appended to Vulkan compile lines (default: empty)
#
# (c) 2025-2026 Jacques Morel -- MIT Licensed
# =============================================================================

# ---------------------------------------------------------------------------
# 1. Host detection
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    HOST_OS := windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        HOST_OS := macos
    else
        HOST_OS := linux
    endif
endif

# ---------------------------------------------------------------------------
# 2. Overrides and base paths
# ---------------------------------------------------------------------------
ROOT        := ..
BUILD_DIR   := $(ROOT)/build/dll
DLL_SRC     := $(ROOT)/situation_dll.c
VD_COMPOSITOR_GL_EMBED_SRC := $(ROOT)/build/opengl/sit_vd_compositor_gl_spirv_embed.c
VMA_SRC     := $(ROOT)/ext/vma_wrapper.cpp
TC_SRC      := $(ROOT)/ext/glfw/deps/tinycthread.c
TRACY_CLIENT_SRC := $(ROOT)/build/tracy_client.cpp
GLFW_LIB    := $(ROOT)/ext/glfw/build/src
SHADERC_LIB := $(ROOT)/ext/shaderc/build/libshaderc

SIT_OPTIMIZE_CFLAGS ?= -O2 -mfma -ffp-contract=fast
EXTRA_VULKAN_CFLAGS ?=

# ---------------------------------------------------------------------------
# 2a. Version extraction (Windows PE resources — canonical: situation_base_version.h)
# ---------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
    include $(ROOT)/build/sit_version.mk
endif

# ---------------------------------------------------------------------------
# 3. Platform blocks
# ---------------------------------------------------------------------------

# --- Windows (verified -- must reproduce the legacy script exactly) ----------
ifeq ($(HOST_OS),windows)
    CC  := gcc
    CXX := g++
    CSTD      := -std=c11
    ARCH_FLAGS := -msse -msse2 -msse4.1

    INCLUDES_COMMON := \
        -I$(ROOT) \
        -I$(ROOT)/sit \
        -I$(ROOT)/ext \
        -I$(ROOT)/ext/cgltf \
        -I$(ROOT)/ext/cglm/include \
        -I$(ROOT)/ext/glfw/include \
        -I$(ROOT)/ext/glfw/deps \
        -I$(ROOT)/sit/k-term

    DEFINES_COMMON := \
        -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
        -DSITUATION_ENABLE_THREADING \
        -DSITUATION_ENABLE_RENDER_THREAD

    ifeq ($(KTERM_USE_SIT_GRID),0)
        # Legacy terminal.comp baseline — opt-in only.
    else
        DEFINES_COMMON += -DKTERM_USE_SIT_GRID=1
    endif

    # Tracy CPU profiling (opt-in): SIT_TRACY=1 make opengl
    ifeq ($(SIT_TRACY),1)
        DEFINES_TRACY := -DSITUATION_ENABLE_TRACY -DTRACY_ENABLE
        INCLUDES_TRACY := -I$(ROOT)/ext/tracy/public
        TRACY_CLIENT_OBJ := $(BUILD_DIR)/tracy_client.o
        SYSLIBS_TRACY := -ldbghelp -lsecur32
    else
        DEFINES_TRACY :=
        INCLUDES_TRACY :=
        TRACY_CLIENT_OBJ :=
        SYSLIBS_TRACY :=
    endif

    GLFW_ARCHIVE := $(GLFW_LIB)/libglfw3.a

    SYSLIBS_GL := \
        -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 \
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput \
        -lws2_32 -lpsapi -lbcrypt -lm

    SYSLIBS_VK := \
        -lglfw3 -lvulkan-1 -lshaderc_combined \
        -lgdi32 -lwinmm -luser32 -lshell32 -lole32 \
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput \
        -lws2_32 -lpsapi -lbcrypt -lm

    # -static-libgcc is already included here; Vulkan link must NOT re-emit it.
    # Order matches legacy: -static-libgcc -static-libstdc++ before -Wl,-Bstatic for CXX builds.
    STATIC_LDFLAGS     := -static-libgcc -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
    # Place -static-libstdc++ before the -Wl blocks to match legacy link-line order exactly.
    STATIC_LDFLAGS_CXX := -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive

    # Ensure BUILD_DIR exists at parse time (Windows).
    # Use mkdir with -p equivalent via sh (MSYS2 sh handles mkdir -p fine on Windows).
    $(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
endif

# --- Linux (scaffold -- inert on Windows) ------------------------------------
ifeq ($(HOST_OS),linux)
    CC  := gcc
    CXX := g++
    CSTD      := -std=c11
    ARCH_FLAGS := -msse -msse2 -msse4.1

    INCLUDES_COMMON := \
        -I$(ROOT) \
        -I$(ROOT)/sit \
        -I$(ROOT)/ext \
        -I$(ROOT)/ext/cgltf \
        -I$(ROOT)/ext/cglm/include \
        -I$(ROOT)/ext/glfw/include \
        -I$(ROOT)/ext/glfw/deps \
        -I$(ROOT)/sit/k-term

    DEFINES_COMMON := \
        -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
        -DSITUATION_ENABLE_THREADING \
        -DSITUATION_ENABLE_RENDER_THREAD

    SYSLIBS_GL := \
        -lglfw -lGL -lm -lpthread -ldl \
        -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama

    SYSLIBS_VK := \
        -lglfw -lvulkan -lshaderc -lm -lpthread -ldl \
        -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama

    $(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
endif

# --- macOS (scaffold -- inert on Windows) ------------------------------------
ifeq ($(HOST_OS),macos)
    CC  := clang
    CXX := clang++
    CSTD      := -std=c11
    ARCH_FLAGS :=

    INCLUDES_COMMON := \
        -I$(ROOT) \
        -I$(ROOT)/sit \
        -I$(ROOT)/ext \
        -I$(ROOT)/ext/cgltf \
        -I$(ROOT)/ext/cglm/include \
        -I$(ROOT)/ext/glfw/include \
        -I$(ROOT)/ext/glfw/deps \
        -I$(ROOT)/sit/k-term

    DEFINES_COMMON := \
        -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
        -DSITUATION_ENABLE_THREADING \
        -DSITUATION_ENABLE_RENDER_THREAD

    FRAMEWORKS_GL := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    FRAMEWORKS_VK := -framework Cocoa -framework IOKit -framework CoreVideo

    SYSLIBS_GL := -lglfw $(FRAMEWORKS_GL)
    SYSLIBS_VK := -lglfw -lvulkan $(FRAMEWORKS_VK)

    $(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
endif

# OpenGL-only: precompiled VD compositor SPIR-V embed (build/opengl/, not sit/).
INCLUDES_OPENGL := $(INCLUDES_COMMON) -I$(ROOT)/build/opengl

# ---------------------------------------------------------------------------
# 4. Vulkan SDK resolution (Windows: autodetect C:/VulkanSDK/* if not set)
# ---------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
    ifeq ($(strip $(VULKAN_SDK)),)
        VK_CANDIDATES := $(wildcard C:/VulkanSDK/*)
        VULKAN_SDK := $(firstword $(foreach d,$(VK_CANDIDATES),\
                       $(if $(wildcard $(d)/Include/vulkan/vulkan.h),$(d),)))
    endif
endif

# ---------------------------------------------------------------------------
# 5. Prerequisite guards
# ---------------------------------------------------------------------------
.PHONY: check-glfw check-vulkan-sdk check-vulkan-shaderc

check-glfw:
	@if [ ! -f "$(GLFW_ARCHIVE)" ]; then \
	    echo ""; \
	    echo "[glfw] libglfw3.a not found -- building GLFW now..."; \
	    echo "[glfw] Source : $(ROOT)/ext/glfw"; \
	    echo "[glfw] Output : $(GLFW_ARCHIVE)"; \
	    echo ""; \
	    mkdir -p "$(ROOT)/ext/glfw/build" && \
	    cd "$(ROOT)/ext/glfw/build" && \
	    cmake .. -G "MinGW Makefiles" \
	        -DCMAKE_C_COMPILER=gcc \
	        -DGLFW_BUILD_DOCS=OFF \
	        -DGLFW_BUILD_TESTS=OFF \
	        -DGLFW_BUILD_EXAMPLES=OFF \
	        -DGLFW_BUILD_WIN32=ON && \
	    mingw32-make && \
	    echo "" && \
	    echo "[glfw] Build complete."; \
	fi
	@test -f "$(GLFW_ARCHIVE)" || { \
	    echo ""; \
	    echo "[ERROR] GLFW auto-build failed -- $(GLFW_ARCHIVE) still missing."; \
	    echo ""; \
	    echo "  Common causes and fixes:"; \
	    echo ""; \
	    echo "  1. cmake not on PATH"; \
	    echo "     Install via MSYS2:  pacman -S mingw-w64-x86_64-cmake"; \
	    echo "     Then re-run:        build\\build_situation.bat opengl"; \
	    echo ""; \
	    echo "  2. Stale or corrupted build directory"; \
	    echo "     Delete and retry:"; \
	    echo "       rmdir /s /q ext\\glfw\\build"; \
	    echo "       build\\build_situation.bat opengl"; \
	    echo ""; \
	    echo "  3. Build manually (scroll up to see cmake/make output for details):"; \
	    echo "     cd ext\\glfw"; \
	    echo "     mkdir build && cd build"; \
	    echo "     cmake .. -G \"MinGW Makefiles\" -DCMAKE_C_COMPILER=gcc"; \
	    echo "              -DGLFW_BUILD_DOCS=OFF -DGLFW_BUILD_TESTS=OFF"; \
	    echo "              -DGLFW_BUILD_EXAMPLES=OFF"; \
	    echo "     mingw32-make"; \
	    echo ""; \
	    exit 1; \
	}

# SDK + headers -- required by both 'vulkan' DLL and 'static-vulkan'
check-vulkan-sdk:
	@if [ -z "$(VULKAN_SDK)" ]; then \
	    echo ""; \
	    echo "[ERROR] Vulkan SDK not found."; \
	    echo ""; \
	    echo "  The Makefile autodetects the SDK by scanning C:/VulkanSDK/*/"; \
	    echo "  looking for Include/vulkan/vulkan.h inside each subdirectory."; \
	    echo "  Nothing was found -- either the SDK is not installed or it is"; \
	    echo "  installed in a non-standard location."; \
	    echo ""; \
	    echo "  Fix option 1 -- Install the Vulkan SDK (recommended):"; \
	    echo "    Download from: https://vulkan.lunarg.com/sdk/home"; \
	    echo "    Latest version: 1.4.350.0 (Windows installer)"; \
	    echo "    Install to the default path: C:\\VulkanSDK\\1.4.350.0\\"; \
	    echo "    The installer sets VULKAN_SDK in the system environment."; \
	    echo "    After install, open a new terminal and re-run the build."; \
	    echo ""; \
	    echo "  Fix option 2 -- Point to an existing non-standard install:"; \
	    echo "    set VULKAN_SDK=C:\\path\\to\\your\\VulkanSDK\\1.x.y.z"; \
	    echo "    build\\build_situation.bat vulkan"; \
	    echo ""; \
	    echo "  Note: static-vulkan and vulkan both require the SDK for"; \
	    echo "  headers and vulkan-1 at link time. There is no way to build"; \
	    echo "  either Vulkan target without it."; \
	    echo ""; \
	    exit 1; \
	fi
	@if [ ! -f "$(VULKAN_SDK)/Include/vulkan/vulkan.h" ]; then \
	    echo ""; \
	    echo "[ERROR] Vulkan SDK found at $(VULKAN_SDK) but appears invalid."; \
	    echo ""; \
	    echo "  Expected header not found:"; \
	    echo "    $(VULKAN_SDK)/Include/vulkan/vulkan.h"; \
	    echo ""; \
	    echo "  This usually means the SDK path points to a partial or"; \
	    echo "  corrupted install. Try:"; \
	    echo ""; \
	    echo "  1. Reinstall the SDK from https://vulkan.lunarg.com/sdk/home"; \
	    echo "  2. Or set VULKAN_SDK explicitly to a valid install:"; \
	    echo "       set VULKAN_SDK=C:\\VulkanSDK\\1.4.350.0"; \
	    echo "       build\\build_situation.bat vulkan"; \
	    echo ""; \
	    exit 1; \
	fi

# shaderc archive -- required by DLL target only (static-vulkan does NOT check this)
check-vulkan-shaderc:
	@test -f "$(SHADERC_LIB)/libshaderc_combined.a" || { \
	    echo ""; \
	    echo "[ERROR] shaderc not found: $(SHADERC_LIB)/libshaderc_combined.a"; \
	    echo ""; \
	    echo "  Build shaderc from the project root (10-30 minutes):"; \
	    echo "    build/build_shaderc.bat"; \
	    echo ""; \
	    echo "  Output: ext/shaderc/build/libshaderc/libshaderc_combined.a"; \
	    echo "  See doc/COMPILATION_GUIDE.md -> Building shaderc"; \
	    echo ""; \
	    echo "  Note: static-vulkan does NOT require shaderc."; \
	    echo "        Only the vulkan DLL target links it."; \
	    echo ""; \
	    exit 1; \
	}

# ---------------------------------------------------------------------------
# 6. Windows resource objects (DLL targets only -- compiled once per backend)
# ---------------------------------------------------------------------------
RC_SRC  := $(ROOT)/sit/platform/windows/situation_resource.rc
ICO_SRC := $(ROOT)/sit/platform/windows/situation_icon.ico

$(BUILD_DIR)/situation_resource_opengl.o: $(RC_SRC) \
        $(ROOT)/sit/situation_base_version.h \
        $(ICO_SRC)
ifeq ($(HOST_OS),windows)
	@echo "[rc] Compiling situation_resource.rc (OpenGL)..."
	windres $(RC_SRC) -o $@ \
	    --include-dir $(ROOT)/sit/platform/windows \
	    $(SIT_VERSION_WINDRES_FLAGS) \
	    -DSIT_DLL_BASENAME=situation_opengl
endif

$(BUILD_DIR)/situation_resource_vulkan.o: $(RC_SRC) \
        $(ROOT)/sit/situation_base_version.h \
        $(ICO_SRC)
ifeq ($(HOST_OS),windows)
	@echo "[rc] Compiling situation_resource.rc (Vulkan)..."
	windres $(RC_SRC) -o $@ \
	    --include-dir $(ROOT)/sit/platform/windows \
	    $(SIT_VERSION_WINDRES_FLAGS) \
	    -DSIT_DLL_BASENAME=situation_vulkan
endif

# ---------------------------------------------------------------------------
# 7. Shared tinycthread object (compiled exactly once)
# ---------------------------------------------------------------------------
$(BUILD_DIR)/tinycthread.o: $(TC_SRC)
	@echo [common] Compiling tinycthread...
	$(CC) -c $(TC_SRC) -o $@ $(CSTD) -I$(ROOT)/ext/glfw/deps

$(BUILD_DIR)/tracy_client.o: $(TRACY_CLIENT_SRC)
ifeq ($(SIT_TRACY),1)
	@echo [tracy] Compiling tracy_client.cpp...
	$(CXX) -c $(TRACY_CLIENT_SRC) -o $@ -std=c++11 \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) $(INCLUDES_TRACY) \
	    $(DEFINES_TRACY)
endif

# ---------------------------------------------------------------------------
# 8. OpenGL DLL
# ---------------------------------------------------------------------------
.PHONY: opengl _opengl_status
opengl: check-glfw $(BUILD_DIR)/situation_opengl.dll _opengl_status

_opengl_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/situation_opengl.dll"
	@echo "  Source   : $(DLL_SRC)"
	@test -f "$(BUILD_DIR)/situation_opengl.dll" && echo "  Status   : OK" || (echo "  Status   : [ERROR] artifact missing after build step" && exit 1)
	@echo ""

$(BUILD_DIR)/situation_opengl.dll: $(DLL_SRC) $(VD_COMPOSITOR_GL_EMBED_SRC) $(BUILD_DIR)/tinycthread.o \
        $(BUILD_DIR)/situation_resource_opengl.o $(TRACY_CLIENT_OBJ)
ifeq ($(HOST_OS),windows)
	@echo ""
	@echo "========================================"
	@echo " Situation DLL Build - OpenGL"
	@echo "========================================"
	@echo ""
	@echo "[1/3] Compiling sit_vd_compositor_gl_spirv_embed.c..."
	$(CC) -c $(VD_COMPOSITOR_GL_EMBED_SRC) -o $(BUILD_DIR)/sit_vd_compositor_gl_spirv_embed.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_OPENGL) $(INCLUDES_TRACY) \
	    -DSITUATION_USE_OPENGL $(DEFINES_COMMON) $(DEFINES_TRACY) \
	    -DSITUATION_BUILD_SHARED
	@echo "[2/3] Compiling situation_dll.c (OpenGL)..."
	$(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_dll_opengl.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_OPENGL) $(INCLUDES_TRACY) \
	    -DSITUATION_USE_OPENGL $(DEFINES_COMMON) $(DEFINES_TRACY) \
	    -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION
	@echo "[3/3] Linking situation_opengl.dll..."
ifeq ($(SIT_TRACY),1)
	$(CXX) -shared $(BUILD_DIR)/situation_dll_opengl.o \
	    $(BUILD_DIR)/sit_vd_compositor_gl_spirv_embed.o $(BUILD_DIR)/tinycthread.o \
	    $(TRACY_CLIENT_OBJ) \
	    $(BUILD_DIR)/situation_resource_opengl.o \
	    -o $@ -L$(GLFW_LIB) $(STATIC_LDFLAGS_CXX) $(SYSLIBS_GL) $(SYSLIBS_TRACY)
else
	$(CC) -shared $(BUILD_DIR)/situation_dll_opengl.o \
	    $(BUILD_DIR)/sit_vd_compositor_gl_spirv_embed.o $(BUILD_DIR)/tinycthread.o \
	    $(TRACY_CLIENT_OBJ) \
	    $(BUILD_DIR)/situation_resource_opengl.o \
	    -o $@ -L$(GLFW_LIB) $(STATIC_LDFLAGS) $(SYSLIBS_GL)
endif
	gendef - $@ > $(BUILD_DIR)/situation_opengl.def 2>/dev/null
	dlltool -D situation_opengl.dll -d $(BUILD_DIR)/situation_opengl.def \
	    -l $(BUILD_DIR)/situation_opengl.lib
	@echo "[BUILT]   $@"
else
	@echo "[ERROR] opengl DLL target: platform not yet shipping ($(HOST_OS))." && exit 1
endif

# ---------------------------------------------------------------------------
# 9. Vulkan DLL
# ---------------------------------------------------------------------------
.PHONY: vulkan _vulkan_status
vulkan: check-glfw check-vulkan-sdk check-vulkan-shaderc $(BUILD_DIR)/situation_vulkan.dll _vulkan_status

_vulkan_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/situation_vulkan.dll"
	@echo "  Source   : $(DLL_SRC)"
	@test -f "$(BUILD_DIR)/situation_vulkan.dll" && echo "  Status   : OK" || (echo "  Status   : [ERROR] artifact missing after build step" && exit 1)
	@echo ""

$(BUILD_DIR)/situation_vulkan.dll: $(DLL_SRC) $(BUILD_DIR)/tinycthread.o \
        $(BUILD_DIR)/situation_resource_vulkan.o $(TRACY_CLIENT_OBJ)
ifeq ($(HOST_OS),windows)
	@echo ""
	@echo "========================================"
	@echo " Situation DLL Build - Vulkan"
	@echo " Vulkan SDK: $(VULKAN_SDK)"
	@echo "========================================"
	@echo ""
	@echo "[1/3] Compiling VMA wrapper..."
	$(CXX) -c $(VMA_SRC) -o $(BUILD_DIR)/vma_wrapper.o -std=c++11 \
	    -I$(ROOT)/ext -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include"
	@echo "[2/3] Compiling situation_dll.c (Vulkan)..."
	$(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_dll_vulkan.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) $(INCLUDES_TRACY) \
	    -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include" \
	    -I$(ROOT)/ext/shaderc/libshaderc/include \
	    -DSITUATION_USE_VULKAN $(DEFINES_COMMON) $(DEFINES_TRACY) \
	    -DSITUATION_ENABLE_SHADER_COMPILER \
	    -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION \
	    $(EXTRA_VULKAN_CFLAGS)
	@echo "[3/3] Linking situation_vulkan.dll..."
	$(CXX) -shared $(BUILD_DIR)/situation_dll_vulkan.o $(BUILD_DIR)/vma_wrapper.o \
	    $(BUILD_DIR)/tinycthread.o $(TRACY_CLIENT_OBJ) $(BUILD_DIR)/situation_resource_vulkan.o \
	    -o $@ -L$(GLFW_LIB) -L$(SHADERC_LIB) -L"$(VULKAN_SDK)/Lib" \
	    $(STATIC_LDFLAGS_CXX) $(SYSLIBS_VK) $(SYSLIBS_TRACY)
	gendef - $@ > $(BUILD_DIR)/situation_vulkan.def 2>/dev/null
	dlltool -D situation_vulkan.dll -d $(BUILD_DIR)/situation_vulkan.def \
	    -l $(BUILD_DIR)/situation_vulkan.lib
	@echo "[BUILT]   $@"
else
	@echo "[ERROR] vulkan DLL target: platform not yet shipping ($(HOST_OS))." && exit 1
endif

# ---------------------------------------------------------------------------
# 10. Static OpenGL
# ---------------------------------------------------------------------------
.PHONY: static-opengl _static_opengl_status
static-opengl: check-glfw $(BUILD_DIR)/situation_opengl.a _static_opengl_status

_static_opengl_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/situation_opengl.a"
	@echo "  Source   : $(DLL_SRC)"
	@test -f "$(BUILD_DIR)/situation_opengl.a" && echo "  Status   : OK" || (echo "  Status   : [ERROR] artifact missing after build step" && exit 1)
	@echo ""

$(BUILD_DIR)/situation_opengl.a: $(DLL_SRC) $(VD_COMPOSITOR_GL_EMBED_SRC) $(BUILD_DIR)/tinycthread.o
ifeq ($(HOST_OS),windows)
	@echo ""
	@echo "========================================"
	@echo " Situation Static Lib - OpenGL"
	@echo "========================================"
	@echo ""
	@echo "[1/3] Compiling sit_vd_compositor_gl_spirv_embed.c..."
	$(CC) -c $(VD_COMPOSITOR_GL_EMBED_SRC) -o $(BUILD_DIR)/sit_vd_compositor_gl_spirv_embed_static.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_OPENGL) \
	    -DSITUATION_USE_OPENGL $(DEFINES_COMMON)
	@echo "[2/3] Compiling situation_dll.c (OpenGL, static)..."
	$(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_static_opengl.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_OPENGL) \
	    -DSITUATION_USE_OPENGL $(DEFINES_COMMON) -DKTERM_IMPLEMENTATION
	@echo "[3/3] Archiving situation_opengl.a..."
	ar rcs $@ $(BUILD_DIR)/situation_static_opengl.o \
	    $(BUILD_DIR)/sit_vd_compositor_gl_spirv_embed_static.o \
	    $(BUILD_DIR)/tinycthread.o
	@echo "[BUILT]   $@"
else
	@echo "[ERROR] static-opengl target: platform not yet shipping ($(HOST_OS))." && exit 1
endif

# ---------------------------------------------------------------------------
# 11. Static Vulkan
# ---------------------------------------------------------------------------
.PHONY: static-vulkan _static_vulkan_status
static-vulkan: check-glfw check-vulkan-sdk $(BUILD_DIR)/situation_vulkan.a _static_vulkan_status

# Note: static-vulkan depends only on check-vulkan-sdk (NOT check-vulkan-shaderc)
# -- exact parity with legacy script, which checks SDK but not shaderc for static builds.
_static_vulkan_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/situation_vulkan.a"
	@echo "  Source   : $(DLL_SRC)"
	@test -f "$(BUILD_DIR)/situation_vulkan.a" && echo "  Status   : OK" || (echo "  Status   : [ERROR] artifact missing after build step" && exit 1)
	@echo ""

$(BUILD_DIR)/situation_vulkan.a: $(DLL_SRC) $(BUILD_DIR)/tinycthread.o
ifeq ($(HOST_OS),windows)
	@echo ""
	@echo "========================================"
	@echo " Situation Static Lib - Vulkan"
	@echo " Vulkan SDK: $(VULKAN_SDK)"
	@echo "========================================"
	@echo ""
	@echo "[1/3] Compiling VMA wrapper..."
	$(CXX) -c $(VMA_SRC) -o $(BUILD_DIR)/vma_wrapper.o -std=c++11 \
	    -I$(ROOT)/ext -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include"
	@echo "[2/3] Compiling situation_dll.c (Vulkan, static)..."
	$(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_static_vulkan.o $(CSTD) \
	    $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) \
	    -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include" \
	    -I$(ROOT)/ext/shaderc/libshaderc/include \
	    -DSITUATION_USE_VULKAN $(DEFINES_COMMON) \
	    -DSITUATION_ENABLE_SHADER_COMPILER -DKTERM_IMPLEMENTATION \
	    $(EXTRA_VULKAN_CFLAGS)
	@echo "[3/3] Archiving situation_vulkan.a..."
	ar rcs $@ $(BUILD_DIR)/situation_static_vulkan.o \
	    $(BUILD_DIR)/vma_wrapper.o $(BUILD_DIR)/tinycthread.o
	@echo "[BUILT]   $@"
else
	@echo "[ERROR] static-vulkan target: platform not yet shipping ($(HOST_OS))." && exit 1
endif

# ---------------------------------------------------------------------------
# 12. Aggregate and clean targets
# ---------------------------------------------------------------------------
.PHONY: all all-static distclean clean help

all: opengl vulkan

all-static: static-opengl static-vulkan

# clean removes all build products so a subsequent build will actually recompile
# (important after header changes). .def/.lib left for legacy parity.
clean:
	@echo Cleaning build artifacts...
	-rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.dll $(BUILD_DIR)/*.a
	@echo Done.

# distclean removes everything (use before shipping or very deep clean).
distclean:
	@echo Removing all build artifacts...
	-rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.dll $(BUILD_DIR)/*.a \
	        $(BUILD_DIR)/*.def $(BUILD_DIR)/*.lib
	@echo Done.

# Default target -- print usage when make is invoked with no arguments.
.DEFAULT_GOAL := help
help:
	@echo ""
	@echo "Situation Library -- Makefile"
	@echo ""
	@echo "Usage:"
	@echo "  make opengl          Build OpenGL DLL       (build/dll/situation_opengl.dll)"
	@echo "  make vulkan          Build Vulkan DLL        (build/dll/situation_vulkan.dll)"
	@echo "  make static-opengl   Build OpenGL static lib (build/dll/situation_opengl.a)"
	@echo "  make static-vulkan   Build Vulkan static lib  (build/dll/situation_vulkan.a)"
	@echo "  make all             Build both DLLs"
	@echo "  make all-static      Build both static libs"
	@echo "  make clean           Remove build products (*.o *.dll *.a) so next build recompiles"
	@echo "  make distclean       Remove all build artifacts (including *.def *.lib)"
	@echo "  make help            Show this message"
	@echo ""
	@echo "Environment overrides:"
	@echo "  MINGW_PATH           Path to MinGW bin (default: C:\\msys64\\mingw64\\bin)"
	@echo "  VULKAN_SDK           Vulkan SDK root (autodetected from C:/VulkanSDK/*)"
	@echo "  SIT_OPTIMIZE_CFLAGS  Compiler optimization (default: -O2 -mfma -ffp-contract=fast)"
	@echo "  EXTRA_VULKAN_CFLAGS  Extra flags for Vulkan compile lines (default: empty)"
	@echo ""
	@echo "Invoke via launcher from project root (recommended):"
	@echo "  build\\build_situation.bat static-opengl"
	@echo ""
