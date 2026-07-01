-- Hand-written wrappers for variadic / callback symbols omitted from ffi_cdef.

local ffi = require("ffi")

ffi.cdef[[
void SituationLog(int msgType, const char* text);
void SituationLogWarning(int code, const char* fmt);
void SituationSetLogCallback(void (*callback)(int level, const char* message, void* user), void* user);

typedef int (*SitLuaCmdDrawTextExFn)(void* cmd, SituationFont font, const char* text, Vector2 pos, float font_size, float spacing, ColorRGBA color);
void sit_lua_draw_shim_set(SitLuaCmdDrawTextExFn fn);
int sit_lua_cmd_draw_text_ex(void* cmd, const SituationFont* font, const char* text, float x, float y, float font_size, float spacing, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
]]

local M = {}

M._callback_refs = {}
M._draw_shim_ready = false

local function host_exports()
    local ok, exports = pcall(function()
        return ffi.C
    end)
    if ok then
        return exports
    end
    return nil
end

function M.init_draw_shim(lib)
    if M._draw_shim_ready then
        return
    end
    local host = host_exports()
    if not host or not host.sit_lua_draw_shim_set then
        error("sit_lua draw shim unavailable (rebuild hello_situation.exe)", 2)
    end
    host.sit_lua_draw_shim_set(ffi.cast("SitLuaCmdDrawTextExFn", lib.SituationCmdDrawTextEx))
    M._draw_shim_ready = true
end

function M.cmd_draw_text_ex(lib, cmd, font, text, x, y, font_size, spacing, color)
    M.init_draw_shim(lib)
    local host = host_exports()
    return host.sit_lua_cmd_draw_text_ex(
        cmd,
        font,
        text,
        x,
        y,
        font_size,
        spacing,
        color.r,
        color.g,
        color.b,
        color.a
    )
end

function M.default_font()
    local font = ffi.new("SituationFont")
    ffi.fill(font, 0)
    return font
end

function M.situation_log(lib, message)
    lib.SituationLog(0, message)
end

function M.situation_log_warning(lib, message)
    lib.SituationLogWarning(0, message)
end

function M.situation_image_draw_text_formatted(lib, image, font, x, y, size, color, fmt, ...)
    local text = string.format(fmt, ...)
    return lib.SituationImageDrawText(image, font, text, x, y, size, color)
end

function M.situation_set_log_callback(lib, cb)
    M._callback_refs.log = cb
    lib.SituationSetLogCallback(cb)
end

return M