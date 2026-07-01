# =============================================================================
# Situation Test Harness -- Makefile
#
# Base directory: tests/harness/  -- project root reached via ../..
# Usage (from project root via thin launcher):
#   build\build_tests.bat static-opengl
#   build\build_tests.bat static-vulkan
#   build\build_tests.bat all-static
#   build\build_tests.bat opengl
#   build\build_tests.bat vulkan
#   build\build_tests.bat all
#
# Or directly (from tests/harness/):
#   make static-opengl
#
# Prerequisites: pre-built library from build\build_situation.bat (same backend).
# Does NOT compile situation_dll.c or any sit/ library sources.
#
# (c) 2025-2026 Jacques Morel -- MIT Licensed
# =============================================================================

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

ROOT          := ../..
BUILD_DIR     := $(ROOT)/build/tests
LIB_DIR       := $(ROOT)/build/dll
GLFW_LIB      := $(ROOT)/ext/glfw/build/src
SHADERC_LIB   := $(ROOT)/ext/shaderc/build/libshaderc
BUILD_SCRIPTS := $(ROOT)/build

HARNESS_SRCS := \
    main.c \
    sit_test_registry.c \
    sit_harness_test_pattern_helpers.c \
    test_filesystem.c \
    test_threading.c \
    test_core.c \
    test_identity_init.c \
    test_window.c \
    test_output_color_depth.c \
    test_input.c \
    test_timer.c \
    test_proj.c \
    test_graphics.c \
    test_graphics_patterns.c \
    test_text_rendering.c \
    test_text_retro_builders.c \
    test_grid.c \
    test_virtual_display.c \
    test_compute.c \
    test_transfer.c \
    test_render_target.c \
    test_graphics_spirv.c \
    test_model_loader.c \
    test_stl_loader.c \
    test_obj_loader.c \
    test_projection_3d.c \
    test_audio.c \
    test_tone_synth.c \
    test_audio_effects_heard.c \
    audio_freq_detect.c \
    sit_test_audio_levels.c \
    sit_test_stereo_scope.c \
    test_misc.c \
    test_system_info.c \
    test_kterm_console.c \
    test_advanced.c \
    test_advanced_font_showcase.c \
    test_frame_pacing.c \
    test_frame_profile.c \
    test_query_pool.c

EMBED_GL_SRCS := sit_harness_spirv_gl_embed.c sit_harness_test_pattern_gl_embed.c
EMBED_VK_SRCS := sit_harness_spirv_vk_embed.c sit_harness_test_pattern_vk_embed.c
APP_ICON_OBJ  := $(BUILD_DIR)/sit_app_icon.o
APP_RC        ?= $(ROOT)/sit/platform/windows/sit_app.rc

ifeq ($(HOST_OS),windows)
    CC  := gcc
    CXX := g++
    CSTD := -std=c11

    include $(ROOT)/build/sit_version.mk

    INCLUDES := \
        -I$(ROOT) -I$(ROOT)/sit -I$(ROOT)/ext \
        -I$(ROOT)/ext/cglm/include -I$(ROOT)/ext/glfw/include \
        -I$(ROOT)/sit/k-term -I.

    DEFINES_COMMON := \
        -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
        -DSITUATION_ENABLE_THREADING \
        -DSITUATION_ENABLE_RENDER_THREAD

    CFLAGS_BASE := $(CSTD) -O0 -g $(INCLUDES) $(DEFINES_COMMON)

    SYSLIBS_GL := \
        -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 \
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput \
        -lws2_32 -lpsapi -lbcrypt -lm

    SYSLIBS_VK := \
        -lglfw3 -lvulkan-1 -lshaderc_combined \
        -lgdi32 -lwinmm -luser32 -lshell32 -lole32 \
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput \
        -lws2_32 -lpsapi -lbcrypt -lm

    STATIC_LDFLAGS     := -static-libgcc -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
    STATIC_LDFLAGS_CXX := -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive

    ifeq ($(strip $(VULKAN_SDK)),)
        VK_CANDIDATES := $(wildcard C:/VulkanSDK/*)
        VULKAN_SDK := $(firstword $(foreach d,$(VK_CANDIDATES),\
            $(if $(wildcard $(d)/Include/vulkan/vulkan.h),$(d),)))
    endif
    VULKAN_INCLUDES := -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include"

    $(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
endif

.PHONY: shaders check-static-opengl-lib check-static-vulkan-lib check-opengl-dll check-vulkan-dll check-vulkan-sdk
.PHONY: all all-static static-opengl static-vulkan opengl vulkan clean help
.PHONY: _static_opengl_status _static_vulkan_status _opengl_dll_status _vulkan_dll_status

all: opengl vulkan

all-static: static-opengl static-vulkan

shaders:
	@powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass \
	    -File "$(BUILD_SCRIPTS)/compile_harness_shaders.ps1"

check-static-opengl-lib:
	@test -f "$(LIB_DIR)/situation_opengl.a" || \
	    (echo "[ERROR] $(LIB_DIR)/situation_opengl.a missing. Run: build\\build_situation.bat static-opengl" && exit 1)

check-static-vulkan-lib:
	@test -f "$(LIB_DIR)/situation_vulkan.a" || \
	    (echo "[ERROR] $(LIB_DIR)/situation_vulkan.a missing. Run: build\\build_situation.bat static-vulkan" && exit 1)

check-opengl-dll:
	@test -f "$(LIB_DIR)/situation_opengl.dll" || \
	    (echo "[ERROR] $(LIB_DIR)/situation_opengl.dll missing. Run: build\\build_situation.bat opengl" && exit 1)

check-vulkan-dll:
	@test -f "$(LIB_DIR)/situation_vulkan.dll" || \
	    (echo "[ERROR] $(LIB_DIR)/situation_vulkan.dll missing. Run: build\\build_situation.bat vulkan" && exit 1)

check-vulkan-sdk:
	@test -n "$(VULKAN_SDK)" || (echo "[ERROR] Vulkan SDK not found. Set VULKAN_SDK." && exit 1)

$(APP_ICON_OBJ): $(APP_RC) $(SIT_VERSION_HEADER) $(ROOT)/sit/platform/windows/situation_icon.ico
	@echo "[rc] Compiling $(APP_RC)..."
	@windres $(APP_RC) -o $@ --include-dir $(ROOT)/sit/platform/windows \
	    $(SIT_VERSION_WINDRES_FLAGS)

# --- OpenGL static (single compile+link, mirrors legacy build_tests.bat) -------
static-opengl: shaders check-static-opengl-lib $(APP_ICON_OBJ) _build_static_opengl _static_opengl_status

_build_static_opengl:
	@echo ""
	@echo "========================================"
	@echo " Situation Test Harness - OpenGL static"
	@echo "========================================"
	@echo ""
	@echo "[1/1] Compiling and linking..."
	@$(CC) $(HARNESS_SRCS) $(EMBED_GL_SRCS) $(APP_ICON_OBJ) -o $(BUILD_DIR)/sit_test_opengl.exe \
	    $(CFLAGS_BASE) -DSITUATION_USE_OPENGL \
	    -L$(GLFW_LIB) $(STATIC_LDFLAGS) \
	    "$(LIB_DIR)/situation_opengl.a" $(SYSLIBS_GL)
	@echo "[BUILT]   $(BUILD_DIR)/sit_test_opengl.exe"

.PHONY: _build_static_opengl _build_opengl_dll _build_static_vulkan _build_vulkan_dll

_static_opengl_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/sit_test_opengl.exe"
	@echo "  Link     : $(LIB_DIR)/situation_opengl.a (static)"
	@test -f "$(BUILD_DIR)/sit_test_opengl.exe" && echo "  Status   : OK" || (echo "  Status   : [ERROR]" && exit 1)
	@echo ""

# --- Vulkan static (gcc per-file compile, g++ link, .o cleaned after link) ----
VK_ALL_SRCS := $(HARNESS_SRCS) $(EMBED_VK_SRCS)
VK_OBJS     := $(addprefix $(BUILD_DIR)/,$(VK_ALL_SRCS:.c=.o))

$(BUILD_DIR)/%.o: %.c
	@$(CC) -c $< -o $@ $(CFLAGS_BASE) $(VULKAN_INCLUDES) \
	    -DSITUATION_USE_VULKAN -DSITUATION_ENABLE_SHADER_COMPILER

static-vulkan: shaders check-vulkan-sdk check-static-vulkan-lib $(APP_ICON_OBJ) _vk_static_banner $(VK_OBJS) _build_static_vulkan _static_vulkan_status

_vk_static_banner:
	@echo ""
	@echo "========================================"
	@echo " Situation Test Harness - Vulkan static"
	@echo "========================================"
	@echo ""
	@echo "[1/2] Compiling $(words $(VK_ALL_SRCS)) sources..."

_build_static_vulkan: $(VK_OBJS) $(APP_ICON_OBJ)
	@echo "[2/2] Linking..."
	@$(CXX) $(VK_OBJS) $(APP_ICON_OBJ) -o $(BUILD_DIR)/sit_test_vulkan.exe \
	    -L$(GLFW_LIB) -L$(SHADERC_LIB) -L"$(VULKAN_SDK)/Lib" \
	    $(STATIC_LDFLAGS_CXX) \
	    "$(LIB_DIR)/situation_vulkan.a" $(SYSLIBS_VK)
	@rm -f $(VK_OBJS)
	@echo "[BUILT]   $(BUILD_DIR)/sit_test_vulkan.exe"

_static_vulkan_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/sit_test_vulkan.exe"
	@echo "  Link     : $(LIB_DIR)/situation_vulkan.a (static)"
	@test -f "$(BUILD_DIR)/sit_test_vulkan.exe" && echo "  Status   : OK" || (echo "  Status   : [ERROR]" && exit 1)
	@echo ""

# --- OpenGL DLL-linked --------------------------------------------------------
opengl: shaders check-opengl-dll $(APP_ICON_OBJ) _build_opengl_dll _opengl_dll_status

_build_opengl_dll:
	@echo ""
	@echo "========================================"
	@echo " Situation Test Harness - OpenGL DLL"
	@echo "========================================"
	@echo ""
	@echo "[1/1] Compiling and linking..."
	@$(CC) $(HARNESS_SRCS) $(EMBED_GL_SRCS) $(APP_ICON_OBJ) -o $(BUILD_DIR)/sit_test_opengl.exe \
	    $(CFLAGS_BASE) -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED \
	    -L$(LIB_DIR) -lsituation_opengl -static-libgcc -lm
	@echo "[BUILT]   $(BUILD_DIR)/sit_test_opengl.exe"

_opengl_dll_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/sit_test_opengl.exe"
	@echo "  Link     : $(LIB_DIR)/situation_opengl.dll"
	@test -f "$(BUILD_DIR)/sit_test_opengl.exe" && echo "  Status   : OK" || (echo "  Status   : [ERROR]" && exit 1)
	@echo "  Note     : run via build\\run_tests.bat or add build\\dll to PATH"
	@echo ""

# --- Vulkan DLL-linked --------------------------------------------------------
vulkan: shaders check-vulkan-sdk check-vulkan-dll $(APP_ICON_OBJ) _build_vulkan_dll _vulkan_dll_status

_build_vulkan_dll:
	@echo ""
	@echo "========================================"
	@echo " Situation Test Harness - Vulkan DLL"
	@echo "========================================"
	@echo ""
	@echo "[1/1] Compiling and linking..."
	@$(CC) $(HARNESS_SRCS) $(EMBED_VK_SRCS) $(APP_ICON_OBJ) -o $(BUILD_DIR)/sit_test_vulkan.exe \
	    $(CFLAGS_BASE) $(VULKAN_INCLUDES) \
	    -DSITUATION_USE_VULKAN -DSITUATION_USE_SHARED -DSITUATION_ENABLE_SHADER_COMPILER \
	    -L$(LIB_DIR) -lsituation_vulkan -static-libgcc -lm
	@echo "[BUILT]   $(BUILD_DIR)/sit_test_vulkan.exe"

_vulkan_dll_status:
	@echo ""
	@echo "  Artifact : $(BUILD_DIR)/sit_test_vulkan.exe"
	@echo "  Link     : $(LIB_DIR)/situation_vulkan.dll"
	@test -f "$(BUILD_DIR)/sit_test_vulkan.exe" && echo "  Status   : OK" || (echo "  Status   : [ERROR]" && exit 1)
	@echo "  Note     : run via build\\run_tests.bat or add build\\dll to PATH"
	@echo ""

clean:
	rm -f $(BUILD_DIR)/sit_test_*.exe $(BUILD_DIR)/*.o

help:
	@echo ""
	@echo "Situation Test Harness Makefile (tests/harness/)"
	@echo ""
	@echo "  static-opengl   Self-contained OpenGL exe (requires situation_opengl.a)"
	@echo "  static-vulkan   Self-contained Vulkan exe (requires situation_vulkan.a)"
	@echo "  all-static      Build both static harness exes"
	@echo "  opengl          DLL-linked OpenGL exe (requires situation_opengl.dll)"
	@echo "  vulkan          DLL-linked Vulkan exe (requires situation_vulkan.dll)"
	@echo "  all             Build both DLL-linked harness exes"
	@echo "  shaders         Regenerate harness SPIR-V embeds"
	@echo "  clean           Remove harness build products under build/tests/"
	@echo ""
	@echo "Build library first: build\\build_situation.bat [target]"
	@echo ""