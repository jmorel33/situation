--[[
  Situation Lua interactive demo — torus raymarching + tone synthesizer.

  Build & run:
    build\build_situation.bat opengl
    python tools\generate_lua_bindings.py
    build\build_lua_example.bat opengl hello_situation
]]

-- Dev mode (luajit script.lua): set package.path. Embedded exe uses package.preload.
if not package.preload["situation"] then
    local function script_dir()
        local src = debug.getinfo(1, "S").source
        if src:sub(1, 1) == "@" then
            src = src:sub(2)
        end
        return (src:match("^(.*)[/\\]") or ".")
    end
    local dir = script_dir()
    package.path = dir .. "/?/init.lua;" .. dir .. "/situation/?.lua;" .. package.path
end

local ffi = require("ffi")
local bit = require("bit")
local sit = require("situation")
local C = sit.constants

local VERT_SRC_VK = [[#version 460
void main() {
    int vid = gl_VertexIndex;
    vec2 pos = vec2(float(vid & 1) * 4.0 - 1.0, float(vid & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}]]

local VERT_SRC_GL = [[#version 460
void main() {
    vec2 pos = vec2(float(gl_VertexID & 1) * 4.0 - 1.0, float(gl_VertexID & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}]]

local FRAG_SRC_VK = [[#version 460
layout(push_constant) uniform PC { float uTime; vec2 uResolution; } pc;
#define uTime       pc.uTime
#define uResolution pc.uResolution
layout(location = 0) out vec4 fragColor;
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }
mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);
    float x_norm = gl_FragCoord.x / uResolution.x;
    vec3 bg = vec3(0.02, 0.02, 0.05);
    for (int i = 0; i < 6; i++) {
        float phase = uTime * 0.3 + float(i) * 1.05;
        float center = 0.5 + 0.35 * sin(phase);
        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);
        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;
    }
    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));
    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);
    float t = 0.0; float d = 0.0;
    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }
    vec3 col = bg;
    if (d < 0.001) {
        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);
        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));
        vec3 light = normalize(vec3(0.4, 0.8, -0.5));
        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);
        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);
        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;
    }
    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);
}]]

local FRAG_SRC_GL = [[#version 460
layout(location = 0) uniform float uTime;
layout(location = 1) uniform vec2  uResolution;
layout(location = 0) out vec4 fragColor;
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }
mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);
    float x_norm = gl_FragCoord.x / uResolution.x;
    vec3 bg = vec3(0.02, 0.02, 0.05);
    for (int i = 0; i < 6; i++) {
        float phase = uTime * 0.3 + float(i) * 1.05;
        float center = 0.5 + 0.35 * sin(phase);
        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);
        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;
    }
    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));
    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);
    float t = 0.0; float d = 0.0;
    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }
    vec3 col = bg;
    if (d < 0.001) {
        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);
        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));
        vec3 light = normalize(vec3(0.4, 0.8, -0.5));
        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);
        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);
        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;
    }
    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);
}]]

local PENTATONIC = { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 }

local rng = { state = 1337 }

function rng:next_u32()
    self.state = (self.state * 1103515245 + 12345) % 0x100000000
    return self.state
end

function rng:gen_range(lo, hi)
    return lo + (self:next_u32() % (hi - lo))
end

local function safe_cleanup_step(label, fn)
    local step_ok, step_err = pcall(fn)
    if not step_ok then
        print("WARNING: cleanup (" .. label .. "): " .. tostring(step_err))
    end
end

local function cleanup(lib, state)
    if state._cleaned then
        return
    end
    state._cleaned = true

    safe_cleanup_step("note off", function()
        if state.last_note and state.last_note > 0 then
            lib.SituationVirtualMidiNoteOff(state.last_note)
            state.last_note = 0
        end
    end)

    safe_cleanup_step("unload shader", function()
        if state.shader[0].slot_index ~= 0 or state.shader[0].generation ~= 0 then
            lib.SituationUnloadShader(state.shader)
        end
    end)

    safe_cleanup_step("deactivate graph", function()
        if state.graph then
            lib.SituationSetActiveGraph(nil)
        end
    end)

    safe_cleanup_step("midi teardown", function()
        if state.graph and state.audio_ok then
            lib.SituationTeardownVirtualMidiLoopback()
        end
    end)

    safe_cleanup_step("destroy graph", function()
        if state.graph then
            lib.SituationDestroyGraph(state.graph)
            state.graph = nil
        end
    end)

    safe_cleanup_step("drain frame", function()
        if sit.window_should_close() then
            return
        end
        if sit.situation_success(lib.SituationAcquireFrameCommandBuffer()) then
            sit.check(lib.SituationEndFrame())
        end
    end)

    safe_cleanup_step("final poll", function()
        lib.SituationPollInputEvents()
    end)

    safe_cleanup_step("shutdown", function()
        lib.SituationShutdown()
    end)

    print("Situation cleanup complete.")
    io.stdout:flush()
end

local function main()
    local backend = os.getenv("SIT_LUA_BACKEND") or "opengl"
    sit.load(backend)
    local lib = sit.lib

    print("=== Situation Lua — Raster Bars + Ambient Synth ===")
    print("  V      Toggle VSync")
    print("  F      Toggle borderless windowed")
    print("  Space  Trigger note immediately")
    print("  + / -  Reverb wet up / down")
    print("  ] / [  Delay wet up / down")
    print("  P / O  Delay feedback up / down")
    print("  ESC    Quit")

    local title =
        "Situation+Lua  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]"
    local config = sit.init_info_window(900, 600, title)
    sit.check(lib.SituationInit(0, nil, config))

    local state = {
        graph = nil,
        audio_ok = false,
        shader = ffi.new("SituationShader[1]"),
        synth_handle = ffi.new("uint32_t[1]"),
        echo_handle = ffi.new("uint32_t[1]"),
        reverb_handle = ffi.new("uint32_t[1]"),
        midi_device_id = ffi.new("int[1]", -1),
        delay_wet = 0.25,
        delay_feedback = 0.40,
        delay_time = 0.35,
        reverb_wet = 0.20,
        last_note = 0,
        default_font = sit.default_font(),
        _cleaned = false,
    }

    local ok, err = pcall(function()
        lib.SituationSetVSync(true)
        lib.SituationInitDeviceRegistry()

        state.graph = lib.SituationCreateGraph()
        if state.graph ~= nil then
            local e1 = lib.SituationCreateNode(state.graph, C.SITUATION_NODE_TONE_SYNTH, state.synth_handle)
            local e2 = lib.SituationCreateNode(state.graph, C.SITUATION_NODE_ECHO, state.echo_handle)
            local e3 = lib.SituationCreateNode(state.graph, C.SITUATION_NODE_REVERB, state.reverb_handle)
            if sit.situation_success(e1) and sit.situation_success(e2) and sit.situation_success(e3) then
                sit.check(lib.SituationCreatePatch(state.graph, state.synth_handle[0], 0, state.echo_handle[0], 0, false))
                sit.check(lib.SituationCreatePatch(state.graph, state.echo_handle[0], 0, state.reverb_handle[0], 0, false))
                sit.check(lib.SituationSetControl(state.graph, state.echo_handle[0], 0, state.delay_time))
                sit.check(lib.SituationSetControl(state.graph, state.echo_handle[0], 1, state.delay_feedback))
                sit.check(lib.SituationSetControl(state.graph, state.echo_handle[0], 2, state.delay_wet))
                sit.check(lib.SituationSetControl(state.graph, state.reverb_handle[0], 0, 0.65))
                sit.check(lib.SituationSetControl(state.graph, state.reverb_handle[0], 1, 0.55))
                sit.check(lib.SituationSetControl(state.graph, state.reverb_handle[0], 2, state.reverb_wet))
                sit.check(lib.SituationSetControl(state.graph, state.reverb_handle[0], 3, 0.30))
                sit.check(lib.SituationSetControl(state.graph, state.reverb_handle[0], 4, 0.85))

                local midi_ok = sit.situation_success(
                    lib.SituationSetupVirtualMidiLoopback(state.midi_device_id)
                )
                if midi_ok then
                    sit.check(lib.SituationEnableMidiControl(state.graph, state.synth_handle[0], state.midi_device_id[0]))
                else
                    print("WARNING: Virtual MIDI loopback unavailable — auto-notes only")
                end

                sit.check(lib.SituationSetActiveGraph(state.graph))
                state.audio_ok = true
                lib.SituationVirtualMidiControlChange(0, 70, 0)
                lib.SituationVirtualMidiControlChange(0, 73, 90)
                lib.SituationVirtualMidiControlChange(0, 75, 40)
                lib.SituationVirtualMidiControlChange(0, 76, 80)
                lib.SituationVirtualMidiControlChange(0, 72, 110)
                print("Audio graph active: ToneSynth -> Echo -> Reverb")
            else
                print(string.format("Node creation failed: synth=%d echo=%d reverb=%d", e1, e2, e3))
            end
        else
            print("WARNING: Could not create audio graph — audio disabled")
        end

        local is_vulkan = lib.SituationGetGraphicsBackend() == C.SIT_GRAPHICS_BACKEND_VULKAN
        local vert = is_vulkan and VERT_SRC_VK or VERT_SRC_GL
        local frag = is_vulkan and FRAG_SRC_VK or FRAG_SRC_GL
        sit.check(lib.SituationLoadShaderFromMemory(vert, frag, state.shader))
        local shader = state.shader[0]

        local sim_time = 0.0
        local note_timer = 0.0
        local window_flags = 0

        while not sit.window_should_close() do
            sit.begin_frame()

            if sit.key_pressed(256) then
                break
            end

            window_flags = lib.SituationGetCurrentActualWindowStateFlags()
            if sit.key_pressed(86) then
                local vsync_on = bit.band(window_flags, C.SITUATION_WINDOW_STATE_VSYNC_HINT) ~= 0
                lib.SituationSetVSync(not vsync_on)
                window_flags = lib.SituationGetCurrentActualWindowStateFlags()
            end

            if sit.key_pressed(70) then
                lib.SituationToggleBorderlessWindowed()
            end

            if sit.key_pressed(32) and state.audio_ok then
                if state.last_note > 0 then
                    lib.SituationVirtualMidiNoteOff(state.last_note)
                end
                state.last_note = PENTATONIC[1 + rng:gen_range(0, #PENTATONIC)]
                lib.SituationVirtualMidiNoteOn(state.last_note, 50 + rng:gen_range(0, 30))
                note_timer = 0.0
            end

            if state.graph and state.audio_ok then
                if sit.key_pressed(61) then
                    state.reverb_wet = math.min(1.0, state.reverb_wet + 0.05)
                    lib.SituationSetControl(state.graph, state.reverb_handle[0], 2, state.reverb_wet)
                end
                if sit.key_pressed(45) then
                    state.reverb_wet = math.max(0.0, state.reverb_wet - 0.05)
                    lib.SituationSetControl(state.graph, state.reverb_handle[0], 2, state.reverb_wet)
                end
                if sit.key_pressed(93) then
                    state.delay_wet = math.min(1.0, state.delay_wet + 0.05)
                    lib.SituationSetControl(state.graph, state.echo_handle[0], 2, state.delay_wet)
                end
                if sit.key_pressed(91) then
                    state.delay_wet = math.max(0.0, state.delay_wet - 0.05)
                    lib.SituationSetControl(state.graph, state.echo_handle[0], 2, state.delay_wet)
                end
                if sit.key_pressed(80) then
                    state.delay_feedback = math.min(0.95, state.delay_feedback + 0.05)
                    lib.SituationSetControl(state.graph, state.echo_handle[0], 1, state.delay_feedback)
                end
                if sit.key_pressed(79) then
                    state.delay_feedback = math.max(0.0, state.delay_feedback - 0.05)
                    lib.SituationSetControl(state.graph, state.echo_handle[0], 1, state.delay_feedback)
                end
            end

            local dt = lib.SituationGetFrameTime()
            sim_time = sim_time + dt
            note_timer = note_timer + dt

            if state.audio_ok and note_timer > 4.0 then
                note_timer = 0.0
                if state.last_note > 0 then
                    lib.SituationVirtualMidiNoteOff(state.last_note)
                end
                lib.SituationVirtualMidiControlChange(0, 70, rng:gen_range(0, 4) * 32)
                lib.SituationVirtualMidiControlChange(0, 74, 40 + rng:gen_range(0, 80))
                lib.SituationVirtualMidiControlChange(0, 71, 20 + rng:gen_range(0, 60))
                lib.SituationVirtualMidiControlChange(0, 24, rng:gen_range(0, 40))
                lib.SituationVirtualMidiControlChange(0, 26, rng:gen_range(0, 50))
                state.last_note = PENTATONIC[1 + rng:gen_range(0, #PENTATONIC)]
                lib.SituationVirtualMidiNoteOn(state.last_note, 30 + rng:gen_range(0, 30))
            end

            if sit.situation_success(lib.SituationAcquireFrameCommandBuffer()) then
                local cmd = lib.SituationGetMainCommandBuffer()
                if cmd ~= nil then
                    local bg = ffi.new("ColorRGBA", { r = 0, g = 0, b = 0, a = 255 })
                    sit.check(lib.SituationCmdBeginRenderToDisplay(cmd, -1, bg))
                    sit.check(lib.SituationCmdBindPipeline(cmd, shader))

                    local w = lib.SituationGetRenderWidth()
                    local h = lib.SituationGetRenderHeight()
                    if is_vulkan then
                        local pc = ffi.new("struct { float time; float pad; float res_x; float res_y; }", {
                            sim_time, 0.0, w, h,
                        })
                        sit.check(lib.SituationCmdSetPushConstant(cmd, 0, pc, ffi.sizeof(pc)))
                    else
                        local t_val = ffi.new("float[1]", sim_time)
                        sit.check(lib.SituationSetShaderUniform(shader, "uTime", t_val, C.SIT_UNIFORM_FLOAT))
                        local res = ffi.new("float[2]", { w, h })
                        sit.check(lib.SituationSetShaderUniform(shader, "uResolution", res, C.SIT_UNIFORM_VEC2))
                    end

                    sit.check(lib.SituationCmdDraw(cmd, 3, 1, 0, 0))

                    local sc = h / 600.0
                    sit.check(sit.cmd_draw_text_ex(
                        cmd,
                        state.default_font,
                        "S I T U A T I O N",
                        w * 0.27,
                        18.0 * sc,
                        24.0 * sc,
                        2.0 * sc,
                        ffi.new("ColorRGBA", { r = 255, g = 255, b = 255, a = 220 })
                    ))

                    local vsync_on = bit.band(window_flags, C.SITUATION_WINDOW_STATE_VSYNC_HINT) ~= 0
                    local fps = lib.SituationGetFPS()
                    local audio_str = state.audio_ok and "active" or "off"
                    local line1 = string.format(
                        "%.0f FPS  VSync:%s  Audio:%s",
                        fps,
                        vsync_on and "ON" or "OFF",
                        audio_str
                    )
                    sit.check(sit.cmd_draw_text_ex(
                        cmd,
                        state.default_font,
                        line1,
                        10.0,
                        h - 36.0 * sc,
                        8.0 * sc,
                        0.0,
                        ffi.new("ColorRGBA", { r = 180, g = 180, b = 180, a = 255 })
                    ))

                    local line2 = string.format(
                        "Reverb: %d%%   Delay: %d%%   Delay FB: %d%%",
                        math.floor(state.reverb_wet * 100),
                        math.floor(state.delay_wet * 100),
                        math.floor(state.delay_feedback * 100)
                    )
                    sit.check(sit.cmd_draw_text_ex(
                        cmd,
                        state.default_font,
                        line2,
                        10.0,
                        h - 20.0 * sc,
                        8.0 * sc,
                        0.0,
                        ffi.new("ColorRGBA", { r = 140, g = 210, b = 255, a = 255 })
                    ))

                    sit.check(lib.SituationCmdEndRenderPass(cmd))
                end
                sit.check(lib.SituationEndFrame())
            end
        end
    end)

    cleanup(lib, state)
    if not ok then
        print("ERROR: " .. tostring(err))
        io.stdout:flush()
        return 1
    end
    print("Situation Lua demo finished.")
    io.stdout:flush()
    return 0
end

return main() or 0