# =============================================================================
# KFS test harness — Makefile
#
# Run from sit/kfs/tests/:
#   make build    # compile kfs_test (requires libkfs.a)
#   make test            # correctness (41) + perf (11) = 52 tests
#   make test-correctness # H0-H6 only (41 tests, fast CI)
#   make test-perf       # H7 perf only (11 tests)
#   make test-mybuddy    # H0 harness linked against libkfs_mybuddy.a (M7)
#   make test-mybuddy-perf-c  # H7 perf on MyBuddy profile C (BUDDY_LARGE + 256 MiB)
#   make clean    # remove harness objects/binary only
#
# libkfs.a is built via ../kfs/Makefile if missing.
# Outputs: ../build/kfs_test, objects in ../build/obj/
# =============================================================================

KFS_PKG_DIR   := ..
KFS_SRC_DIR   := $(KFS_PKG_DIR)/kfs
SIT_DIR       := $(KFS_PKG_DIR)/..
BUILD_DIR     := $(KFS_PKG_DIR)/build
EXT_DIR       := $(KFS_PKG_DIR)/../../ext
TEST_DIR      := .

OBJ_DIR       := $(BUILD_DIR)/obj

CC            := gcc
RM            ?= rm -f
MKDIR         ?= mkdir -p

CFLAGS        ?= -std=c11 -Wall -Wextra -Werror
INC           := -I$(SIT_DIR) -I$(KFS_PKG_DIR) -I$(KFS_SRC_DIR) -I$(EXT_DIR) -I$(TEST_DIR)

KFS_HDR       := $(KFS_SRC_DIR)/kfs.h $(KFS_SRC_DIR)/kfs_version.h $(KFS_SRC_DIR)/kfs_mem.h \
                 $(KFS_SRC_DIR)/kfs_api.h \
                 $(KFS_SRC_DIR)/kfs_impl.h \
                 $(KFS_SRC_DIR)/kfs_impl_fwd.h $(KFS_SRC_DIR)/kfs_impl_core.h \
                 $(KFS_SRC_DIR)/kfs_impl_auth.h $(KFS_SRC_DIR)/kfs_impl_lc.h
LIBKFS            := $(BUILD_DIR)/libkfs.a
LIBKFS_MYBUDDY    := $(BUILD_DIR)/libkfs_mybuddy.a
TEST_BIN_MYBUDDY  := $(BUILD_DIR)/kfs_test_mybuddy$(EXE_EXT)

ifeq ($(OS),Windows_NT)
    MYBUDDY_LIBS  := -lbcrypt -pthread
else
    MYBUDDY_LIBS  := -pthread
endif

ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    MAKE    ?= mingw32-make
else
    EXE_EXT :=
    MAKE    ?= make
endif

TEST_BIN        := $(BUILD_DIR)/kfs_test$(EXE_EXT)
HARNESS_SRCS    := kfs_test_main.c kfs_test_registry.c kfs_test_fixture.c \
                   kfs_test_props.c kfs_test_perf.c kfs_test_timing.c
MODULE_SRCS     := $(wildcard test_*.c)
ALL_SRCS        := $(HARNESS_SRCS) $(MODULE_SRCS)

HARNESS_OBJS    := $(addprefix $(OBJ_DIR)/,$(HARNESS_SRCS:.c=.o))
MODULE_OBJS     := $(addprefix $(OBJ_DIR)/,$(MODULE_SRCS:.c=.o))
TEST_OBJS       := $(HARNESS_OBJS) $(MODULE_OBJS)

SYNC_PROPS_PS1  := fixtures/scripts/sync_harness_props.ps1

.PHONY: all build test test-correctness test-perf test-mybuddy test-mybuddy-perf-c test-all sync-props clean dirs

all: test

dirs:
	$(MKDIR) $(OBJ_DIR)

build: dirs $(LIBKFS) $(TEST_BIN)
	@echo "KFS test harness build OK"

test-correctness: build
	@echo "Running KFS correctness tests (H0-H6)..."
	$(TEST_BIN)
	@echo "KFS correctness tests OK"

test: test-correctness test-perf
	@echo "KFS full harness OK (correctness + perf)"

sync-props:
	@echo "Syncing Situation harness props into fixtures/props/..."
	powershell -ExecutionPolicy Bypass -File $(SYNC_PROPS_PS1)

test-perf: sync-props build
	@echo "Running KFS perf tests..."
	$(TEST_BIN) --perf --perf-iters 100
	@echo "KFS perf tests OK"

test-all: test

test-mybuddy: build-mybuddy
	@echo "Running KFS harness (H0) on MyBuddy backend..."
	$(TEST_BIN_MYBUDDY) --module harness
	@echo "KFS MyBuddy harness OK"

test-mybuddy-perf-c: build-mybuddy
	@echo "Running KFS H7 perf on MyBuddy (profile C)..."
	$(TEST_BIN_MYBUDDY) --perf --perf-iters 100
	@echo "KFS MyBuddy perf OK"

build-mybuddy: dirs $(LIBKFS_MYBUDDY) $(TEST_BIN_MYBUDDY)
	@echo "KFS MyBuddy harness build OK"

$(LIBKFS_MYBUDDY):
	$(MAKE) -C $(KFS_SRC_DIR) lib-mybuddy

$(TEST_BIN_MYBUDDY): $(TEST_OBJS) $(LIBKFS_MYBUDDY) | dirs
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) -L$(BUILD_DIR) -lkfs_mybuddy $(MYBUDDY_LIBS)

$(LIBKFS):
	$(MAKE) -C $(KFS_SRC_DIR) lib

$(TEST_BIN): $(TEST_OBJS) $(LIBKFS) | dirs
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) -L$(BUILD_DIR) -lkfs

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c $(KFS_HDR) | dirs
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	$(RM) $(TEST_OBJS) $(TEST_BIN) $(TEST_BIN_MYBUDDY)