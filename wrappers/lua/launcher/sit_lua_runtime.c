#include "sit_lua_runtime.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define SIT_LUA_MODULE "lua51.dll"
#else
#include <dlfcn.h>
#define SIT_LUA_MODULE "libluajit-5.1.so.2"
#endif

static void *sit_lua_module;

lua_State *(*sit_luaL_newstate)(void);
void (*sit_lua_close)(lua_State *L);
void (*sit_luaL_openlibs)(lua_State *L);
const char *(*sit_lua_tolstring)(lua_State *L, int idx, size_t *len);
void (*sit_lua_getfield)(lua_State *L, int idx, const char *k);
void (*sit_lua_setfield)(lua_State *L, int idx, const char *k);
void (*sit_lua_pushcclosure_fn)(lua_State *L, lua_CFunction fn, int n);
void (*sit_lua_pushstring)(lua_State *L, const char *s);
void (*sit_lua_pushnil)(lua_State *L);
const char *(*sit_lua_pushfstring)(lua_State *L, const char *fmt, ...);
const char *(*sit_luaL_checklstring)(lua_State *L, int arg, size_t *len);
int (*sit_luaL_loadbuffer)(lua_State *L, const char *buff, size_t sz, const char *name);
int (*sit_lua_pcall)(lua_State *L, int nargs, int nresults, int errfunc);
void (*sit_lua_settop)(lua_State *L, int idx);

const char *sit_lua_tostring(lua_State *L, int idx)
{
    return sit_lua_tolstring(L, idx, NULL);
}

void sit_lua_pushcfunction(lua_State *L, lua_CFunction fn)
{
    sit_lua_pushcclosure_fn(L, fn, 0);
}

const char *sit_luaL_checkstring(lua_State *L, int arg)
{
    return sit_luaL_checklstring(L, arg, NULL);
}

void sit_lua_pop(lua_State *L, int n)
{
    sit_lua_settop(L, -(n)-1);
}

#if defined(_WIN32)
static void *SIT_LUA_SYM(const char *name)
{
    return (void *)GetProcAddress((HMODULE)sit_lua_module, name);
}
#else
static void *SIT_LUA_SYM(const char *name)
{
    return dlsym(sit_lua_module, name);
}
#endif

static int sit_lua_resolve_required(void *symbol, const char *name)
{
    if (!symbol) {
        fprintf(stderr, "Lua host: missing symbol %s\n", name);
        return -1;
    }
    return 0;
}

int sit_lua_runtime_init(const char *extract_dir)
{
    char path[1024];

#if defined(_WIN32)
    snprintf(path, sizeof(path), "%s\\%s", extract_dir, SIT_LUA_MODULE);
    sit_lua_module = (void *)LoadLibraryA(path);
#else
    snprintf(path, sizeof(path), "%s/%s", extract_dir, SIT_LUA_MODULE);
    sit_lua_module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!sit_lua_module) {
        fprintf(stderr, "Lua host: failed to load %s from %s\n", SIT_LUA_MODULE, extract_dir);
        return -1;
    }

    sit_luaL_newstate = (lua_State *(*)(void))SIT_LUA_SYM("luaL_newstate");
    if (sit_lua_resolve_required((void *)sit_luaL_newstate, "luaL_newstate") != 0) return -1;
    sit_lua_close = (void (*)(lua_State *))SIT_LUA_SYM("lua_close");
    if (sit_lua_resolve_required((void *)sit_lua_close, "lua_close") != 0) return -1;
    sit_luaL_openlibs = (void (*)(lua_State *))SIT_LUA_SYM("luaL_openlibs");
    if (sit_lua_resolve_required((void *)sit_luaL_openlibs, "luaL_openlibs") != 0) return -1;
    sit_lua_tolstring = (const char *(*)(lua_State *, int, size_t *))SIT_LUA_SYM("lua_tolstring");
    if (sit_lua_resolve_required((void *)sit_lua_tolstring, "lua_tolstring") != 0) return -1;
    sit_lua_getfield = (void (*)(lua_State *, int, const char *))SIT_LUA_SYM("lua_getfield");
    if (sit_lua_resolve_required((void *)sit_lua_getfield, "lua_getfield") != 0) return -1;
    sit_lua_setfield = (void (*)(lua_State *, int, const char *))SIT_LUA_SYM("lua_setfield");
    if (sit_lua_resolve_required((void *)sit_lua_setfield, "lua_setfield") != 0) return -1;
    sit_lua_pushcclosure_fn = (void (*)(lua_State *, lua_CFunction, int))SIT_LUA_SYM("lua_pushcclosure");
    if (sit_lua_resolve_required((void *)sit_lua_pushcclosure_fn, "lua_pushcclosure") != 0) return -1;
    sit_lua_pushstring = (void (*)(lua_State *, const char *))SIT_LUA_SYM("lua_pushstring");
    if (sit_lua_resolve_required((void *)sit_lua_pushstring, "lua_pushstring") != 0) return -1;
    sit_lua_pushnil = (void (*)(lua_State *))SIT_LUA_SYM("lua_pushnil");
    if (sit_lua_resolve_required((void *)sit_lua_pushnil, "lua_pushnil") != 0) return -1;
    sit_lua_pushfstring = (const char *(*)(lua_State *, const char *, ...))SIT_LUA_SYM("lua_pushfstring");
    if (sit_lua_resolve_required((void *)sit_lua_pushfstring, "lua_pushfstring") != 0) return -1;
    sit_luaL_checklstring = (const char *(*)(lua_State *, int, size_t *))SIT_LUA_SYM("luaL_checklstring");
    if (sit_lua_resolve_required((void *)sit_luaL_checklstring, "luaL_checklstring") != 0) return -1;
    sit_luaL_loadbuffer = (int (*)(lua_State *, const char *, size_t, const char *))SIT_LUA_SYM("luaL_loadbuffer");
    if (sit_lua_resolve_required((void *)sit_luaL_loadbuffer, "luaL_loadbuffer") != 0) return -1;
    sit_lua_pcall = (int (*)(lua_State *, int, int, int))SIT_LUA_SYM("lua_pcall");
    if (sit_lua_resolve_required((void *)sit_lua_pcall, "lua_pcall") != 0) return -1;
    sit_lua_settop = (void (*)(lua_State *, int))SIT_LUA_SYM("lua_settop");
    if (sit_lua_resolve_required((void *)sit_lua_settop, "lua_settop") != 0) return -1;

    return 0;
}

void sit_lua_runtime_shutdown(void)
{
    if (!sit_lua_module) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary((HMODULE)sit_lua_module);
#else
    dlclose(sit_lua_module);
#endif
    sit_lua_module = NULL;
}