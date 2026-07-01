# =============================================================================
# KFS (Kaizen Filing System) — Makefile
#
# Source lives in this directory (sit/kfs/kfs/). Build inputs and outputs live in
# sit/kfs/build/. See ../doc/COMPILATION_GUIDE.md for full build reference.
#
# Usage (from sit/kfs/kfs/):
#   make build        # mem-check + compile lib + link-check + harness + api-check
#   make test         # run API harness (delegates to ../tests/Makefile)
#   make lib          # build ../build/libkfs.a only
#   make link-check   # §5.7 link verification binary
#   make api-check    # compile ../build/api_only.c
#   make sync-api     # regenerate kfs_api.h from kfs_impl.h
#   make lib-mybuddy  # libkfs_mybuddy.a (-DKFS_MEM_USE_MYBUDDY)
#   make mybuddy-smoke # link + run MyBuddy backend smoke test
#   make clean        # KFS lib + link-check + api-check artifacts
#
# Harness-only (from sit/kfs/tests/): make build | make test
#
# Windows: use mingw32-make if `make` is unavailable.
# Requires: gcc, ar, ext/sqlite3.c + ext/sqlite3.h (repo root ext/)
# =============================================================================

SIT_DIR       := ../..
KFS_PKG_DIR   := ..
KFS_SRC_DIR   := .
BUILD_DIR     := $(KFS_PKG_DIR)/build
EXT_DIR       := ../../../ext
TEST_DIR      := $(KFS_PKG_DIR)/tests

OBJ_DIR       := $(BUILD_DIR)/obj

CC            ?= gcc
AR            ?= ar
RM            ?= rm -f
MKDIR         ?= mkdir -p
MAKE          ?= make

CFLAGS        ?= -std=c11 -Wall -Wextra -O2
CFLAGS        += -I$(SIT_DIR) -I$(KFS_PKG_DIR) -I$(KFS_SRC_DIR) -I$(EXT_DIR)

STRICT_CFLAGS ?= -std=c11 -Wall -Wextra -Werror

SQLITE_SRC        := $(EXT_DIR)/sqlite3.c
KFS_SRC           := $(KFS_SRC_DIR)/kfs.c
LINK_CHECK_SRC    := $(BUILD_DIR)/link_check_main.c
API_CHECK_SRC     := $(BUILD_DIR)/api_only.c

KFS_HDR           := kfs.h kfs_version.h kfs_mem.h kfs_api.h kfs_impl.h \
                     kfs_impl_fwd.h kfs_impl_core.h kfs_impl_auth.h kfs_impl_lc.h

KFS_OBJ           := $(OBJ_DIR)/kfs.o
KFS_MYBUDDY_OBJ   := $(OBJ_DIR)/kfs_mybuddy.o

SQLITE_OBJ        := $(OBJ_DIR)/sqlite3.o
LINK_CHECK_OBJ    := $(OBJ_DIR)/kfs_link_check_main.o
API_CHECK_OBJ     := $(OBJ_DIR)/kfs_api_only.o
MYBUDDY_SMOKE_SRC := $(BUILD_DIR)/mybuddy_smoke_main.c
MYBUDDY_SMOKE_OBJ := $(OBJ_DIR)/kfs_mybuddy_smoke_main.o
LIBKFS            := $(BUILD_DIR)/libkfs.a
LIBKFS_MYBUDDY    := $(BUILD_DIR)/libkfs_mybuddy.a
MYBUDDY_CFLAGS    := -DKFS_MEM_USE_MYBUDDY -pthread
ifeq ($(OS),Windows_NT)
    MYBUDDY_LIBS  := -lbcrypt -pthread
else
    MYBUDDY_LIBS  := -pthread
endif

ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
else
    EXE_EXT :=
endif

LINK_CHECK_BIN    := $(BUILD_DIR)/kfs_link_check$(EXE_EXT)
MYBUDDY_SMOKE_BIN := $(BUILD_DIR)/kfs_mybuddy_smoke$(EXE_EXT)
TEST_BIN          := $(BUILD_DIR)/kfs_test$(EXE_EXT)

.PHONY: all build test lib lib-mybuddy lib-mybuddy-c link-check api-check mem-check mybuddy-smoke clean sync-api dirs test-build
.PHONY: smoke test-kfs test-all

all: build

dirs:
	$(MKDIR) $(OBJ_DIR)

mem-check:
	python $(KFS_PKG_DIR)/scripts/check_kfs_mem.py

lib: mem-check dirs $(LIBKFS)

lib-mybuddy: mem-check dirs $(LIBKFS_MYBUDDY)
	@echo "KFS MyBuddy library OK"

lib-mybuddy-c: lib-mybuddy
	@echo "KFS MyBuddy library OK (profile C is default for lib-mybuddy)"

mybuddy-smoke: $(MYBUDDY_SMOKE_BIN)
	@echo "Running KFS MyBuddy smoke..."
	$(MYBUDDY_SMOKE_BIN)
	@echo "KFS MyBuddy smoke OK"

link-check: $(LINK_CHECK_BIN)
	@echo "Running KFS link check..."
	$(LINK_CHECK_BIN)
	@echo "KFS link check OK"

test-build:
	$(MAKE) -C $(TEST_DIR) build

build: lib link-check api-check test-build
	@echo "KFS build OK"

test:
	$(MAKE) -C $(TEST_DIR) test

api-check: $(API_CHECK_OBJ)
	@echo "KFS API header check OK"

# Deprecated aliases (old smoke / test-kfs names)
smoke: link-check
test-kfs: test
test-all: build
	$(MAKE) -C $(TEST_DIR) test-all

$(LIBKFS): $(KFS_OBJ) $(SQLITE_OBJ)
	$(AR) rcs $@ $^

$(KFS_OBJ): $(KFS_SRC) $(KFS_HDR) | dirs
	$(CC) $(CFLAGS) -c $(KFS_SRC) -o $@

$(KFS_MYBUDDY_OBJ): $(KFS_SRC) $(KFS_HDR) | dirs
	$(CC) $(CFLAGS) $(MYBUDDY_CFLAGS) -c $(KFS_SRC) -o $@

$(LIBKFS_MYBUDDY): $(KFS_MYBUDDY_OBJ) $(SQLITE_OBJ)
	$(AR) rcs $@ $^

$(MYBUDDY_SMOKE_OBJ): $(MYBUDDY_SMOKE_SRC) $(KFS_HDR) | dirs
	$(CC) $(STRICT_CFLAGS) $(MYBUDDY_CFLAGS) -I$(SIT_DIR) -I$(KFS_PKG_DIR) -I$(KFS_SRC_DIR) -I$(EXT_DIR) -c $(MYBUDDY_SMOKE_SRC) -o $@

$(MYBUDDY_SMOKE_BIN): $(MYBUDDY_SMOKE_OBJ) $(LIBKFS_MYBUDDY) | dirs
	$(CC) $(STRICT_CFLAGS) $(MYBUDDY_CFLAGS) -o $@ $(MYBUDDY_SMOKE_OBJ) -L$(BUILD_DIR) -lkfs_mybuddy $(MYBUDDY_LIBS)

$(SQLITE_OBJ): $(SQLITE_SRC) | dirs
	$(CC) $(CFLAGS) -c $(SQLITE_SRC) -o $@

$(LINK_CHECK_OBJ): $(LINK_CHECK_SRC) $(KFS_HDR) | dirs
	$(CC) $(STRICT_CFLAGS) -I$(SIT_DIR) -I$(KFS_PKG_DIR) -I$(KFS_SRC_DIR) -I$(EXT_DIR) -c $(LINK_CHECK_SRC) -o $@

$(API_CHECK_OBJ): $(API_CHECK_SRC) $(KFS_HDR) | dirs
	$(CC) $(STRICT_CFLAGS) -I$(SIT_DIR) -I$(KFS_PKG_DIR) -I$(KFS_SRC_DIR) -I$(EXT_DIR) -c $(API_CHECK_SRC) -o $@

$(LINK_CHECK_BIN): $(LINK_CHECK_OBJ) $(LIBKFS) | dirs
	$(CC) $(STRICT_CFLAGS) -o $@ $(LINK_CHECK_OBJ) -L$(BUILD_DIR) -lkfs

sync-api:
	python $(KFS_PKG_DIR)/scripts/sync_api.py

clean:
	$(RM) $(KFS_OBJ) $(KFS_MYBUDDY_OBJ) $(SQLITE_OBJ) $(LINK_CHECK_OBJ) $(API_CHECK_OBJ) \
	      $(MYBUDDY_SMOKE_OBJ) $(LIBKFS) $(LIBKFS_MYBUDDY) $(LINK_CHECK_BIN) $(MYBUDDY_SMOKE_BIN)
	$(MAKE) -C $(TEST_DIR) clean