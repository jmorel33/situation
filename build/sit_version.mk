# =============================================================================
# sit_version.mk — PE / windres version flags from situation_base_version.h
#
# Include after ROOT is set (project root path):
#   include $(ROOT)/build/sit_version.mk
#
# Provides: SIT_VERSION_MAJOR, SIT_VERSION_MINOR, SIT_VERSION_PATCH,
#           SIT_VERSION_HEADER, SIT_VERSION_WINDRES_FLAGS
# =============================================================================

SIT_VERSION_HEADER := $(ROOT)/sit/situation_base_version.h

SIT_VERSION_MAJOR := $(shell grep -m1 'SITUATION_VERSION_MAJOR' \
    $(SIT_VERSION_HEADER) | grep -o '[0-9]*')
SIT_VERSION_MINOR := $(shell grep -m1 'SITUATION_VERSION_MINOR' \
    $(SIT_VERSION_HEADER) | grep -o '[0-9]*')
SIT_VERSION_PATCH := $(shell grep -m1 'SITUATION_VERSION_PATCH' \
    $(SIT_VERSION_HEADER) | grep -o '[0-9]*')

SIT_VERSION_WINDRES_FLAGS := \
    -DSIT_VERSION_MAJOR=$(SIT_VERSION_MAJOR) \
    -DSIT_VERSION_MINOR=$(SIT_VERSION_MINOR) \
    -DSIT_VERSION_PATCH=$(SIT_VERSION_PATCH)
