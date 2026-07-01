/**
 * @file build/api_only.c
 * @brief Compile-check KFS API headers without pulling in the implementation.
 */
#include "kfs/kfs.h"

void kfs_api_only(void) {
    GameDB* db = 0;
    (void)db;
}