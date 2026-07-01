-- Situation LuaJIT FFI package. Usage:
--   local sit = require("situation")
--   sit.load("opengl")
--   sit.check(sit.SituationInit(0, nil, info))

local ffi = require("ffi")

local ffi_cdef = require("situation.ffi_cdef")
ffi.cdef(ffi_cdef.cdef)

local dll_mod = require("situation.dll")
local constants = require("situation.constants")
local helpers = require("situation.helpers")
local manual = require("situation.manual")
local types = require("situation.types")
require("situation.callbacks")

local M = {
    ffi = ffi,
    constants = constants,
    helpers = helpers,
    manual = manual,
    types = types,
    lib = nil,
    _backend = nil,
}

for k, v in pairs(constants) do
    M[k] = v
end

for k, v in pairs(helpers) do
    if k ~= "begin_frame" then
        M[k] = v
    end
end

function M.load(backend)
    backend = backend or os.getenv("SIT_LUA_BACKEND") or "opengl"
    local lib = dll_mod.load(backend)
    M.lib = lib
    M._backend = backend
    setmetatable(M, {
        __index = function(_, key)
            local h = helpers[key]
            if h ~= nil then
                return h
            end
            local m = manual[key]
            if m ~= nil then
                return m
            end
            return lib[key]
        end,
    })
    return M
end

function M.begin_frame()
    if not M.lib then
        error("situation.load() must be called first", 2)
    end
    helpers.begin_frame(M.lib)
end

function M.window_should_close()
    if not M.lib then
        error("situation.load() must be called first", 2)
    end
    return helpers.window_should_close(M.lib)
end

function M.key_pressed(key)
    if not M.lib then
        error("situation.load() must be called first", 2)
    end
    return helpers.key_pressed(M.lib, key)
end

function M.cmd_draw_text_ex(cmd, font, text, x, y, font_size, spacing, color)
    if not M.lib then
        error("situation.load() must be called first", 2)
    end
    return manual.cmd_draw_text_ex(M.lib, cmd, font, text, x, y, font_size, spacing, color)
end

function M.default_font()
    return manual.default_font()
end

return M