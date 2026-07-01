#ifndef SIT_LUA_DRAW_SHIM_H
#define SIT_LUA_DRAW_SHIM_H

#include <stdint.h>

#ifdef _WIN32
#define SIT_LUA_SHIM_EXPORT __declspec(dllexport)
#else
#define SIT_LUA_SHIM_EXPORT __attribute__((visibility("default")))
#endif

typedef struct SitLuaColorRGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} SitLuaColorRGBA;

typedef struct SitLuaVector2 {
    float x;
    float y;
} SitLuaVector2;

typedef struct SitLuaTexture {
    uint32_t slot_index;
    uint32_t generation;
    int width;
    int height;
} SitLuaTexture;

typedef struct SitLuaFont {
    void *font_data;
    void *stb_font_info;
    SitLuaTexture atlas_texture;
    void *glyph_info;
    int atlas_width;
    int atlas_height;
    float font_height_pixels;
    uint8_t is_bitmap;
    void *bitmap_data;
    int bitmap_width;
    int bitmap_height;
    int bitmap_count;
    int first_char;
    int chars_per_row;
    int chars_per_col;
    int display_cell_width;
    int display_cell_height;
    float char_spacing;
    float line_spacing;
} SitLuaFont;

typedef int (*SitLuaCmdDrawTextExFn)(
    void *cmd,
    SitLuaFont font,
    const char *text,
    SitLuaVector2 pos,
    float font_size,
    float spacing,
    SitLuaColorRGBA color);

SIT_LUA_SHIM_EXPORT void sit_lua_draw_shim_set(SitLuaCmdDrawTextExFn fn);

SIT_LUA_SHIM_EXPORT int sit_lua_cmd_draw_text_ex(
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
    uint8_t a);

#endif