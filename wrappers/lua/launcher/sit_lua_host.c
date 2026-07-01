/* Embedded LuaJIT host for Situation wrapper examples (single self-contained .exe). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sit_lua_runtime.h"
#include "sit_lua_embed.h"
#include "sit_lua_dll_embed.h"

#ifndef SIT_LUA_BACKEND
#define SIT_LUA_BACKEND "opengl"
#endif

static void sit_lua_set_backend_env(void)
{
#if defined(_WIN32)
    char buf[64];
    snprintf(buf, sizeof(buf), "SIT_LUA_BACKEND=%s", SIT_LUA_BACKEND);
    _putenv(buf);
#else
    setenv("SIT_LUA_BACKEND", SIT_LUA_BACKEND, 1);
#endif
}

static int sit_lua_prepare_runtime(char *extract_dir, size_t extract_dir_size)
{
    char dll_path[1024];
    char env_buf[1152];
    int rc = sit_lua_extract_embedded_dlls(extract_dir, extract_dir_size);
    if (rc != 0) {
        fprintf(stderr, "Lua host: failed to extract embedded DLLs (%d)\n", rc);
        return rc;
    }

    rc = sit_lua_runtime_init(extract_dir);
    if (rc != 0) {
        return rc;
    }

#if defined(_WIN32)
    snprintf(dll_path, sizeof(dll_path), "%s\\situation_%s.dll", extract_dir, SIT_LUA_BACKEND);
    snprintf(env_buf, sizeof(env_buf), "SIT_LUA_DLL_PATH=%s", dll_path);
    _putenv(env_buf);
#else
    snprintf(dll_path, sizeof(dll_path), "%s/situation_%s.dll", extract_dir, SIT_LUA_BACKEND);
    setenv("SIT_LUA_DLL_PATH", dll_path, 1);
#endif
    return 0;
}

int main(void)
{
    lua_State *L;
    const char *errmsg;
    char extract_dir[1024];

    if (sit_lua_prepare_runtime(extract_dir, sizeof(extract_dir)) != 0) {
        return 1;
    }

    L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Lua: failed to create state\n");
        sit_lua_runtime_shutdown();
        return 1;
    }

    luaL_openlibs(L);
    sit_lua_set_backend_env();
    sit_lua_embed_register(L);

    if (sit_lua_embed_run(L) != 0) {
        errmsg = lua_tostring(L, -1);
        fprintf(stderr, "Lua error: %s\n", errmsg ? errmsg : "unknown");
        lua_close(L);
        sit_lua_runtime_shutdown();
        return 1;
    }

    lua_close(L);
    sit_lua_runtime_shutdown();
    return 0;
}