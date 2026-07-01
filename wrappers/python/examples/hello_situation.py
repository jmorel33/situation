#!/usr/bin/env python3
"""Situation Python interactive demo — torus raymarching + tone synthesizer.

Build & run:
  build\\build_situation.bat opengl
  build\\build_python_example.bat opengl hello_situation
"""

from __future__ import annotations

import os
import sys
from ctypes import Structure, byref, c_char_p, c_float, c_int, c_uint32, c_uint8, c_void_p
from pathlib import Path

# PyInstaller bundles `situation/` — skip dev-tree path setup when frozen.
if not getattr(sys, "frozen", False):
    _here = Path(__file__).resolve().parent
    _pkg_root = _here if (_here / "situation").is_dir() else _here.parent
    if str(_pkg_root) not in sys.path:
        sys.path.insert(0, str(_pkg_root))

import situation
from situation import constants as C
from situation import helpers as H
from situation import types as T

VERT_SRC_VK = b"""#version 460
void main() {
    int vid = gl_VertexIndex;
    vec2 pos = vec2(float(vid & 1) * 4.0 - 1.0, float(vid & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}"""

VERT_SRC_GL = b"""#version 460
void main() {
    vec2 pos = vec2(float(gl_VertexID & 1) * 4.0 - 1.0, float(gl_VertexID & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}"""

FRAG_SRC_VK = b"""#version 460
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
}"""

FRAG_SRC_GL = b"""#version 460
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
}"""

PENTATONIC = (48, 52, 55, 60, 64, 67, 72, 76, 79, 84)


class SimpleRng:
    def __init__(self, seed: int) -> None:
        self.state = seed

    def next_u32(self) -> int:
        self.state = (self.state * 1103515245 + 12345) & 0xFFFFFFFF
        return self.state

    def gen_range(self, lo: int, hi: int) -> int:
        return lo + (self.next_u32() % (hi - lo))


class ShaderPc(Structure):
    _fields_ = [
        ("time", c_float),
        ("_pad", c_float),
        ("res_x", c_float),
        ("res_y", c_float),
    ]


class Res2(Structure):
    _fields_ = [("x", c_float), ("y", c_float)]


def _backend_from_env() -> str:
    return os.environ.get("SIT_PYTHON_BACKEND", "opengl")


def main() -> int:
    backend = _backend_from_env()
    dll = situation.load_dll(backend)

    print("=== Situation Python — Raster Bars + Ambient Synth ===")
    print("  V      Toggle VSync")
    print("  F      Toggle borderless windowed")
    print("  Space  Trigger note immediately")
    print("  + / -  Reverb wet up / down")
    print("  ] / [  Delay wet up / down")
    print("  P / O  Delay feedback up / down")
    print("  ESC    Quit")

    rng = SimpleRng(1337)
    title = (
        b"Situation+Python  [V]Sync [F]ull [Spc]Note  "
        b"[+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]"
    )
    config = H.init_info_window(900, 600, title)
    H.check(dll.SituationInit(0, None, byref(config)))

    graph = None
    audio_ok = False
    shader = T.SituationShader()
    synth_handle = c_uint32(0)
    echo_handle = c_uint32(0)
    reverb_handle = c_uint32(0)
    midi_device_id = c_int(-1)
    delay_wet = 0.25
    delay_feedback = 0.40
    delay_time = 0.35
    reverb_wet = 0.20
    last_note = 0

    try:
        dll.SituationSetVSync(True)
        dll.SituationInitDeviceRegistry()

        graph = dll.SituationCreateGraph()
        if graph:
            e1 = dll.SituationCreateNode(
                graph, T.SituationNodeType.SITUATION_NODE_TONE_SYNTH, byref(synth_handle)
            )
            e2 = dll.SituationCreateNode(
                graph, T.SituationNodeType.SITUATION_NODE_ECHO, byref(echo_handle)
            )
            e3 = dll.SituationCreateNode(
                graph, T.SituationNodeType.SITUATION_NODE_REVERB, byref(reverb_handle)
            )
            if H.situation_success(e1) and H.situation_success(e2) and H.situation_success(e3):
                H.check(dll.SituationCreatePatch(graph, synth_handle, 0, echo_handle, 0, False))
                H.check(dll.SituationCreatePatch(graph, echo_handle, 0, reverb_handle, 0, False))
                H.check(dll.SituationSetControl(graph, echo_handle, 0, c_float(delay_time)))
                H.check(dll.SituationSetControl(graph, echo_handle, 1, c_float(delay_feedback)))
                H.check(dll.SituationSetControl(graph, echo_handle, 2, c_float(delay_wet)))
                H.check(dll.SituationSetControl(graph, reverb_handle, 0, c_float(0.65)))
                H.check(dll.SituationSetControl(graph, reverb_handle, 1, c_float(0.55)))
                H.check(dll.SituationSetControl(graph, reverb_handle, 2, c_float(reverb_wet)))
                H.check(dll.SituationSetControl(graph, reverb_handle, 3, c_float(0.30)))
                H.check(dll.SituationSetControl(graph, reverb_handle, 4, c_float(0.85)))

                midi_ok = H.situation_success(
                    dll.SituationSetupVirtualMidiLoopback(byref(midi_device_id))
                )
                if midi_ok:
                    H.check(dll.SituationEnableMidiControl(graph, synth_handle, midi_device_id))
                else:
                    print("WARNING: Virtual MIDI loopback unavailable — auto-notes only")

                H.check(dll.SituationSetActiveGraph(graph))
                audio_ok = True
                dll.SituationVirtualMidiControlChange(0, 70, 0)
                dll.SituationVirtualMidiControlChange(0, 73, 90)
                dll.SituationVirtualMidiControlChange(0, 75, 40)
                dll.SituationVirtualMidiControlChange(0, 76, 80)
                dll.SituationVirtualMidiControlChange(0, 72, 110)
                print("Audio graph active: ToneSynth -> Echo -> Reverb")
            else:
                print(f"Node creation failed: synth={e1} echo={e2} reverb={e3}")
        else:
            print("WARNING: Could not create audio graph — audio disabled")

        is_vulkan = (
            dll.SituationGetGraphicsBackend() == T.SituationGraphicsBackend.SIT_GRAPHICS_BACKEND_VULKAN
        )
        vert = VERT_SRC_VK if is_vulkan else VERT_SRC_GL
        frag = FRAG_SRC_VK if is_vulkan else FRAG_SRC_GL
        H.check(dll.SituationLoadShaderFromMemory(vert, frag, byref(shader)))

        default_font = T.SituationFont()
        sim_time = 0.0
        note_timer = 0.0

        while not dll.SituationWindowShouldClose():
            H.situation_begin_frame(dll)

            if dll.SituationIsKeyPressed(256):
                break

            window_flags = dll.SituationGetCurrentActualWindowStateFlags()
            if dll.SituationIsKeyPressed(86):
                vsync_on = bool(
                    window_flags & T.SituationWindowStateFlags.SITUATION_WINDOW_STATE_VSYNC_HINT
                )
                dll.SituationSetVSync(not vsync_on)
                window_flags = dll.SituationGetCurrentActualWindowStateFlags()

            if dll.SituationIsKeyPressed(70):
                dll.SituationToggleBorderlessWindowed()

            if dll.SituationIsKeyPressed(32) and audio_ok:
                if last_note:
                    dll.SituationVirtualMidiNoteOff(last_note)
                last_note = PENTATONIC[rng.gen_range(0, len(PENTATONIC))]
                velocity = 50 + rng.gen_range(0, 30)
                dll.SituationVirtualMidiNoteOn(last_note, velocity)
                note_timer = 0.0

            if graph and audio_ok:
                if dll.SituationIsKeyPressed(61):
                    reverb_wet = min(1.0, reverb_wet + 0.05)
                    dll.SituationSetControl(graph, reverb_handle, 2, c_float(reverb_wet))
                if dll.SituationIsKeyPressed(45):
                    reverb_wet = max(0.0, reverb_wet - 0.05)
                    dll.SituationSetControl(graph, reverb_handle, 2, c_float(reverb_wet))
                if dll.SituationIsKeyPressed(93):
                    delay_wet = min(1.0, delay_wet + 0.05)
                    dll.SituationSetControl(graph, echo_handle, 2, c_float(delay_wet))
                if dll.SituationIsKeyPressed(91):
                    delay_wet = max(0.0, delay_wet - 0.05)
                    dll.SituationSetControl(graph, echo_handle, 2, c_float(delay_wet))
                if dll.SituationIsKeyPressed(80):
                    delay_feedback = min(0.95, delay_feedback + 0.05)
                    dll.SituationSetControl(graph, echo_handle, 1, c_float(delay_feedback))
                if dll.SituationIsKeyPressed(79):
                    delay_feedback = max(0.0, delay_feedback - 0.05)
                    dll.SituationSetControl(graph, echo_handle, 1, c_float(delay_feedback))

            dt = dll.SituationGetFrameTime()
            sim_time += dt
            note_timer += dt

            if audio_ok and note_timer > 4.0:
                note_timer = 0.0
                if last_note:
                    dll.SituationVirtualMidiNoteOff(last_note)
                waveform = rng.gen_range(0, 4)
                dll.SituationVirtualMidiControlChange(0, 70, waveform * 32)
                dll.SituationVirtualMidiControlChange(0, 74, 40 + rng.gen_range(0, 80))
                dll.SituationVirtualMidiControlChange(0, 71, 20 + rng.gen_range(0, 60))
                dll.SituationVirtualMidiControlChange(0, 24, rng.gen_range(0, 40))
                dll.SituationVirtualMidiControlChange(0, 26, rng.gen_range(0, 50))
                last_note = PENTATONIC[rng.gen_range(0, len(PENTATONIC))]
                velocity = 30 + rng.gen_range(0, 30)
                dll.SituationVirtualMidiNoteOn(last_note, velocity)

            if H.situation_success(dll.SituationAcquireFrameCommandBuffer()):
                cmd = dll.SituationGetMainCommandBuffer()
                if cmd:
                    bg = T.ColorRGBA(0, 0, 0, 255)
                    H.check(dll.SituationCmdBeginRenderToDisplay(cmd, -1, bg))
                    H.check(dll.SituationCmdBindPipeline(cmd, shader))

                    w = float(dll.SituationGetRenderWidth())
                    h = float(dll.SituationGetRenderHeight())
                    if is_vulkan:
                        pc = ShaderPc(sim_time, 0.0, w, h)
                        H.check(
                            dll.SituationCmdSetPushConstant(
                                cmd, 0, byref(pc), ctypes_sizeof(pc)
                            )
                        )
                    else:
                        t_val = c_float(sim_time)
                        dll.SituationSetShaderUniform(
                            shader,
                            c_char_p(b"uTime"),
                            byref(t_val),
                            T.SituationUniformType.SIT_UNIFORM_FLOAT,
                        )
                        res = Res2(w, h)
                        dll.SituationSetShaderUniform(
                            shader,
                            c_char_p(b"uResolution"),
                            byref(res),
                            T.SituationUniformType.SIT_UNIFORM_VEC2,
                        )

                    H.check(dll.SituationCmdDraw(cmd, 3, 1, 0, 0))

                    sc = h / 600.0
                    white = T.ColorRGBA(255, 255, 255, 220)
                    H.check(
                        dll.SituationCmdDrawTextEx(
                            cmd,
                            default_font,
                            c_char_p(b"S I T U A T I O N"),
                            T.Vector2(w * 0.27, 18.0 * sc),
                            24.0 * sc,
                            2.0 * sc,
                            white,
                        )
                    )

                    vsync_on = bool(
                        window_flags & T.SituationWindowStateFlags.SITUATION_WINDOW_STATE_VSYNC_HINT
                    )
                    fps = dll.SituationGetFPS()
                    audio_str = b"active" if audio_ok else b"off"
                    line1 = f"{fps:.0f} FPS  VSync:{'ON' if vsync_on else 'OFF'}  Audio:{audio_str.decode()}".encode()
                    gray = T.ColorRGBA(180, 180, 180, 255)
                    H.check(
                        dll.SituationCmdDrawTextEx(
                            cmd,
                            default_font,
                            c_char_p(line1),
                            T.Vector2(10.0, h - 36.0 * sc),
                            8.0 * sc,
                            0.0,
                            gray,
                        )
                    )

                    line2 = (
                        f"Reverb: {int(reverb_wet * 100)}%   "
                        f"Delay: {int(delay_wet * 100)}%   "
                        f"Delay FB: {int(delay_feedback * 100)}%"
                    ).encode()
                    cyan = T.ColorRGBA(140, 210, 255, 255)
                    H.check(
                        dll.SituationCmdDrawTextEx(
                            cmd,
                            default_font,
                            c_char_p(line2),
                            T.Vector2(10.0, h - 20.0 * sc),
                            8.0 * sc,
                            0.0,
                            cyan,
                        )
                    )

                    H.check(dll.SituationCmdEndRenderPass(cmd))
                H.check(dll.SituationEndFrame())

        if last_note:
            dll.SituationVirtualMidiNoteOff(last_note)
        return 0
    finally:
        if shader.slot_index or shader.generation:
            dll.SituationUnloadShader(byref(shader))
        if graph:
            dll.SituationSetActiveGraph(None)
            if audio_ok:
                dll.SituationTeardownVirtualMidiLoopback()
            dll.SituationDestroyGraph(graph)
        dll.SituationShutdown()
        print("Situation cleanup complete.")


def ctypes_sizeof(obj) -> int:
    from ctypes import sizeof

    return sizeof(obj)


if __name__ == "__main__":
    raise SystemExit(main())
