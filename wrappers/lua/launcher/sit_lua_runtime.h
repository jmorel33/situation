#ifndef SIT_LUA_RUNTIME_H
#define SIT_LUA_RUNTIME_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern lua_State *(*sit_luaL_newstate)(void);
extern void (*sit_lua_close)(lua_State *L);
extern void (*sit_luaL_openlibs)(lua_State *L);
extern void (*sit_lua_getfield)(lua_State *L, int idx, const char *k);
extern void (*sit_lua_setfield)(lua_State *L, int idx, const char *k);
extern void (*sit_lua_pushstring)(lua_State *L, const char *s);
extern void (*sit_lua_pushnil)(lua_State *L);
extern const char *(*sit_lua_pushfstring)(lua_State *L, const char *fmt, ...);
extern int (*sit_luaL_loadbuffer)(lua_State *L, const char *buff, size_t sz, const char *name);
extern int (*sit_lua_pcall)(lua_State *L, int nargs, int nresults, int errfunc);

const char *sit_lua_tostring(lua_State *L, int idx);
void sit_lua_pushcfunction(lua_State *L, lua_CFunction fn);
const char *sit_luaL_checkstring(lua_State *L, int arg);
void sit_lua_pop(lua_State *L, int n);

#undef luaL_newstate
#undef lua_close
#undef luaL_openlibs
#undef lua_tostring
#undef lua_getfield
#undef lua_setfield
#undef lua_pushcfunction
#undef lua_pushstring
#undef lua_pushnil
#undef lua_pushfstring
#undef luaL_checkstring
#undef luaL_loadbuffer
#undef lua_pcall
#undef lua_pop

#define luaL_newstate sit_luaL_newstate
#define lua_close sit_lua_close
#define luaL_openlibs sit_luaL_openlibs
#define lua_tostring sit_lua_tostring
#define lua_getfield sit_lua_getfield
#define lua_setfield sit_lua_setfield
#define lua_pushcfunction sit_lua_pushcfunction
#define lua_pushstring sit_lua_pushstring
#define lua_pushnil sit_lua_pushnil
#define lua_pushfstring sit_lua_pushfstring
#define luaL_checkstring sit_luaL_checkstring
#define luaL_loadbuffer sit_luaL_loadbuffer
#define lua_pcall sit_lua_pcall
#define lua_pop sit_lua_pop

int sit_lua_runtime_init(const char *extract_dir);
void sit_lua_runtime_shutdown(void);

#endif