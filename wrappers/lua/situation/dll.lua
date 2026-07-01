-- Situation DLL discovery and ffi.load for LuaJIT.

local ffi = require("ffi")

local M = {}

local function repo_root()
    local dir = debug.getinfo(1, "S").source:match("^@(.+)[/\\]")
    if not dir then
        return nil
    end
    local p = dir
    for _ = 1, 8 do
        local parent = p:match("^(.*)[/\\][^/\\]+$")
        if not parent then
            break
        end
        local marker = parent .. "/sit/situation_api.h"
        if io.open(marker, "r") then
            return parent
        end
        p = parent
    end
    return nil
end

local function candidate_dirs()
    local dirs = {}
    local seen = {}

    local function add(path)
        if path and path ~= "" and not seen[path] then
            seen[path] = true
            dirs[#dirs + 1] = path
        end
    end

    local here = debug.getinfo(1, "S").source:match("^@(.+)[/\\]")
    if here then
        add(here)
        local parent = here:match("^(.*)[/\\][^/\\]+$")
        if parent then
            add(parent)
        end
    end

    add(".")
    local root = repo_root()
    if root then
        add(root .. "/build/examples/lua")
        add(root .. "/build/dll")
    end

    return dirs
end

function M.find_dll(backend)
    backend = backend or "opengl"
    local name = "situation_" .. backend .. ".dll"
    for _, dir in ipairs(candidate_dirs()) do
        local path = dir .. "/" .. name
        local f = io.open(path, "rb")
        if f then
            f:close()
            return path
        end
    end
    error(name .. " not found. Run: build\\build_situation.bat " .. backend)
end

function M.load(backend)
    backend = backend or os.getenv("SIT_LUA_BACKEND") or "opengl"
    local embedded = os.getenv("SIT_LUA_DLL_PATH")
    if embedded and embedded ~= "" then
        local path = embedded:gsub("%.dll$", "")
        local lib = ffi.load(path)
        M._lib = lib
        M._backend = backend
        M._path = embedded
        return lib
    end
    local path = M.find_dll(backend)
    local lib = ffi.load(path:gsub("%.dll$", ""))
    M._lib = lib
    M._backend = backend
    M._path = path
    return lib
end

return M