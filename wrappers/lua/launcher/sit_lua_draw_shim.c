#include "sit_lua_draw_shim.h"

static SitLuaCmdDrawTextExFn sit_lua_cmd_draw_text_ex_fn;

void sit_lua_draw_shim_set(SitLuaCmdDrawTextExFn fn)
{
    sit_lua_cmd_draw_text_ex_fn = fn;
}

int sit_lua_cmd_draw_text_ex(
    void *cmd,
    const SitLuaFont *font,
    const char *text,
    float x,
    float y,
    float font_size,
    float spacing,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a)
{
    SitLuaVector2 pos;
    SitLuaColorRGBA color;

    if (!sit_lua_cmd_draw_text_ex_fn || !font) {
        return -1;
    }

    pos.x = x;
    pos.y = y;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return sit_lua_cmd_draw_text_ex_fn(cmd, *font, text, pos, font_size, spacing, color);
}