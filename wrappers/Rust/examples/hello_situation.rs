//! Situation Rust Interactive Demo — Torus Raymarching and Tone Synthesizer
//!
//! Build:
//!   .\build_rust_example.bat hello_situation
//!
//! Run:
//!   .\build\examples\rust\hello_situation.exe

use situation::*;
use std::os::raw::{c_char, c_int, c_void};

const VERT_SRC: &str = "#version 460\n\
void main() {\n\
    vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);\n\
    gl_Position = vec4(pos, 0.0, 1.0);\n\
}\n\0";

const FRAG_SRC: &str = "#version 460\n\
layout(location = 0) out vec4 fragColor;\n\
layout(location = 0) uniform float uTime;\n\
layout(location = 1) uniform vec2 uResolution;\n\
\n\
vec3 hsv2rgb(vec3 c) {\n\
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n\
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n\
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n\
}\n\
\n\
float sdTorus(vec3 p, vec2 t) {\n\
    vec2 q = vec2(length(p.xz) - t.x, p.y);\n\
    return length(q) - t.y;\n\
}\n\
\n\
mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }\n\
mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }\n\
\n\
void main() {\n\
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);\n\
    float x_norm = gl_FragCoord.x / uResolution.x;\n\
    vec3 bg = vec3(0.02, 0.02, 0.05);\n\
    for (int i = 0; i < 6; i++) {\n\
        float phase  = uTime * 0.3 + float(i) * 1.05;\n\
        float center = 0.5 + 0.35 * sin(phase);\n\
        float dist   = abs(x_norm - center);\n\
        float glow   = exp(-dist * dist * 200.0);\n\
        float hue    = float(i) / 6.0 + uTime * 0.02;\n\
        bg += hsv2rgb(vec3(hue, 0.9, 1.0)) * glow * 0.6;\n\
    }\n\
    vec3 ro = vec3(0.0, 0.0, -3.5);\n\
    vec3 rd = normalize(vec3(uv, 1.2));\n\
    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);\n\
    float t = 0.0;\n\
    float d = 0.0;\n\
    for (int i = 0; i < 64; i++) {\n\
        vec3 p = ro + rd * t;\n\
        p = rot * p;\n\
        d = sdTorus(p, vec2(1.0, 0.38));\n\
        if (d < 0.001 || t > 10.0) break;\n\
        t += d;\n\
    }\n\
    vec3 col = bg;\n\
    if (d < 0.001) {\n\
        vec3 p = rot * (ro + rd * t);\n\
        vec2 tt = vec2(1.0, 0.38);\n\
        vec2 e  = vec2(0.001, 0.0);\n\
        vec3 n  = normalize(vec3(\n\
            sdTorus(p + e.xyy, tt) - sdTorus(p - e.xyy, tt),\n\
            sdTorus(p + e.yxy, tt) - sdTorus(p - e.yxy, tt),\n\
            sdTorus(p + e.yyx, tt) - sdTorus(p - e.yyx, tt)\n\
        ));\n\
        vec3  light   = normalize(vec3(0.4, 0.8, -0.5));\n\
        float diff    = max(dot(n, light), 0.0);\n\
        float spec    = pow(max(dot(reflect(-light, n), normalize(-rd)), 0.0), 32.0);\n\
        float rim     = pow(1.0 - max(dot(n, normalize(-rd)), 0.0), 3.0);\n\
        float angle  = atan(p.z, p.x);\n\
        float hue    = fract(angle * 0.5 + uTime * 0.08);\n\
        vec3 baseCol = hsv2rgb(vec3(hue, 0.7, 0.9));\n\
        col = baseCol * (0.15 + diff * 0.7)\n\
            + vec3(1.0)  * spec * 0.5\n\
            + hsv2rgb(vec3(hue + 0.3, 0.8, 1.0)) * rim * 0.6;\n\
    }\n\
    float scanline = 0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159);\n\
    fragColor = vec4(col * scanline, 1.0);\n\
}\n\0";

const PENTATONIC: [u8; 10] = [48, 52, 55, 60, 64, 67, 72, 76, 79, 84];

struct SimpleRng {
    state: u32,
}

impl SimpleRng {
    fn new(seed: u32) -> Self {
        SimpleRng { state: seed }
    }
    fn next_u32(&mut self) -> u32 {
        self.state = self.state.wrapping_mul(1103515245).wrapping_add(12345);
        self.state
    }
    fn gen_range(&mut self, min: usize, max: usize) -> usize {
        let range = max - min;
        min + (self.next_u32() as usize % range)
    }
}

fn main() {
    println!("=== Situation Rust — Raster Bars + Ambient Synth ===");
    println!("  V      Toggle VSync");
    println!("  F      Toggle borderless windowed");
    println!("  Space  Trigger note immediately");
    println!("  + / -  Reverb wet up / down");
    println!("  ] / [  Delay wet up / down");
    println!("  P / O  Delay feedback up / down");
    println!("  ESC    Quit");

    let mut rng = SimpleRng::new(1337);

    unsafe {
        let mut config = SituationInitInfo {
            window_width: 900,
            window_height: 600,
            window_title: c"Situation+Rust  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]".as_ptr(),
            ..std::mem::zeroed()
        };

        let err = SituationInit(0, std::ptr::null(), &mut config);
        if err != SituationError::SITUATION_SUCCESS {
            println!("SituationInit failed: {:?}", err);
            return;
        }

        SituationSetVSync(true);

        SituationInitDeviceRegistry();

        let graph = SituationCreateGraph();
        if graph.is_null() {
            println!("WARNING: Could not create audio graph — audio disabled");
        }

        let mut synth_handle: SituationNodeHandle = 0;
        let mut echo_handle: SituationNodeHandle = 0;
        let mut reverb_handle: SituationNodeHandle = 0;
        let mut midi_device_id: c_int = -1;
        let mut audio_ok = false;

        let mut delay_wet: f32 = 0.25;
        let mut delay_feedback: f32 = 0.40;
        let delay_time: f32 = 0.35;
        let mut reverb_wet: f32 = 0.20;

        if !graph.is_null() {
            let e1 = SituationCreateNode(graph, SituationNodeType::SITUATION_NODE_TONE_SYNTH, &mut synth_handle);
            let e2 = SituationCreateNode(graph, SituationNodeType::SITUATION_NODE_ECHO, &mut echo_handle);
            let e3 = SituationCreateNode(graph, SituationNodeType::SITUATION_NODE_REVERB, &mut reverb_handle);

            if e1 == SituationError::SITUATION_SUCCESS && e2 == SituationError::SITUATION_SUCCESS && e3 == SituationError::SITUATION_SUCCESS {
                SituationCreatePatch(graph, synth_handle, 0, echo_handle, 0, false);
                SituationCreatePatch(graph, echo_handle, 0, reverb_handle, 0, false);

                SituationSetControl(graph, echo_handle, 0, delay_time);
                SituationSetControl(graph, echo_handle, 1, delay_feedback);
                SituationSetControl(graph, echo_handle, 2, delay_wet);

                SituationSetControl(graph, reverb_handle, 0, 0.65);
                SituationSetControl(graph, reverb_handle, 1, 0.55);
                SituationSetControl(graph, reverb_handle, 2, reverb_wet);
                SituationSetControl(graph, reverb_handle, 3, 0.30);
                SituationSetControl(graph, reverb_handle, 4, 0.85);

                let midi_ok = SituationSetupVirtualMidiLoopback(&mut midi_device_id) == SituationError::SITUATION_SUCCESS;
                if midi_ok {
                    SituationEnableMidiControl(graph, synth_handle, midi_device_id);
                } else {
                    println!("WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled");
                    midi_device_id = -1;
                }

                SituationSetActiveGraph(graph);
                audio_ok = true;

                SituationVirtualMidiControlChange(0, 70, 0);
                SituationVirtualMidiControlChange(0, 73, 90);
                SituationVirtualMidiControlChange(0, 75, 40);
                SituationVirtualMidiControlChange(0, 76, 80);
                SituationVirtualMidiControlChange(0, 72, 110);

                println!("Audio graph active: ToneSynth -> Echo -> Reverb");
            } else {
                println!("Node creation failed: synth={:?} echo={:?} reverb={:?}", e1, e2, e3);
            }
        }

        let mut shader = std::mem::zeroed();
        let shader_err = SituationLoadShaderFromMemory(VERT_SRC.as_ptr() as *const c_char, FRAG_SRC.as_ptr() as *const c_char, &mut shader);
        if shader_err != SituationError::SITUATION_SUCCESS {
            println!("Shader compile failed: {:?}", shader_err);
            if !graph.is_null() {
                SituationSetActiveGraph(std::ptr::null_mut());
                if audio_ok {
                    SituationTeardownVirtualMidiLoopback();
                }
                SituationDestroyGraph(graph);
            }
            SituationShutdown();
            return;
        }

        let default_font = std::mem::zeroed();
        let mut sim_time: f32 = 0.0;
        let mut note_timer: f32 = 0.0;
        let mut last_note: u8 = 0;

        while !SituationWindowShouldClose() {
            SituationPollInputEvents();
            SituationUpdateTimers();

            if SituationIsKeyPressed(256) { // ESC
                break;
            }

            let mut window_flags = SituationGetCurrentActualWindowStateFlags();
            if SituationIsKeyPressed(86) { // V
                let vsync_on = (window_flags & SituationWindowStateFlags::SITUATION_WINDOW_STATE_VSYNC_HINT as u32) != 0;
                SituationSetVSync(!vsync_on);
                window_flags = SituationGetCurrentActualWindowStateFlags();
            }

            if SituationIsKeyPressed(70) { // F
                SituationToggleBorderlessWindowed();
            }

            if SituationIsKeyPressed(32) && audio_ok { // Space
                if last_note > 0 {
                    SituationVirtualMidiNoteOff(last_note);
                }
                let note_idx = rng.gen_range(0, PENTATONIC.len());
                last_note = PENTATONIC[note_idx];
                let velocity = (50 + rng.gen_range(0, 30)) as u8;
                SituationVirtualMidiNoteOn(last_note, velocity);
                note_timer = 0.0;
            }

            if SituationIsKeyPressed(61) && audio_ok { // +
                reverb_wet = (reverb_wet + 0.05).min(1.0);
                SituationSetControl(graph, reverb_handle, 2, reverb_wet);
            }
            if SituationIsKeyPressed(45) && audio_ok { // -
                reverb_wet = (reverb_wet - 0.05).max(0.0);
                SituationSetControl(graph, reverb_handle, 2, reverb_wet);
            }

            if SituationIsKeyPressed(93) && audio_ok { // ]
                delay_wet = (delay_wet + 0.05).min(1.0);
                SituationSetControl(graph, echo_handle, 2, delay_wet);
            }
            if SituationIsKeyPressed(91) && audio_ok { // [
                delay_wet = (delay_wet - 0.05).max(0.0);
                SituationSetControl(graph, echo_handle, 2, delay_wet);
            }

            if SituationIsKeyPressed(80) && audio_ok { // P
                delay_feedback = (delay_feedback + 0.05).min(0.95);
                SituationSetControl(graph, echo_handle, 1, delay_feedback);
            }
            if SituationIsKeyPressed(79) && audio_ok { // O
                delay_feedback = (delay_feedback - 0.05).max(0.0);
                SituationSetControl(graph, echo_handle, 1, delay_feedback);
            }

            let dt = SituationGetFrameTime();
            sim_time += dt;
            note_timer += dt;

            if audio_ok && note_timer > 4.0 {
                note_timer = 0.0;
                if last_note > 0 {
                    SituationVirtualMidiNoteOff(last_note);
                }

                let waveform = rng.gen_range(0, 4) as u8;
                SituationVirtualMidiControlChange(0, 70, waveform * 32);
                let cutoff = (40 + rng.gen_range(0, 80)) as u8;
                SituationVirtualMidiControlChange(0, 74, cutoff);
                let resonance = (20 + rng.gen_range(0, 60)) as u8;
                SituationVirtualMidiControlChange(0, 71, resonance);
                let lfo_rate = rng.gen_range(0, 40) as u8;
                SituationVirtualMidiControlChange(0, 24, lfo_rate);
                let lfo_depth = rng.gen_range(0, 50) as u8;
                SituationVirtualMidiControlChange(0, 26, lfo_depth);

                let note_idx = rng.gen_range(0, PENTATONIC.len());
                last_note = PENTATONIC[note_idx];
                let velocity = (30 + rng.gen_range(0, 30)) as u8;
                SituationVirtualMidiNoteOn(last_note, velocity);
            }

            if SituationAcquireFrameCommandBuffer() == SituationError::SITUATION_SUCCESS {
                let cmd = SituationGetMainCommandBuffer();
                if !cmd.is_null() {
                    SituationCmdBeginRenderToDisplay(cmd, -1, ColorRGBA { r: 0, g: 0, b: 0, a: 255 });
                    SituationCmdBindPipeline(cmd, shader);

                    let w = SituationGetRenderWidth() as f32;
                    let h = SituationGetRenderHeight() as f32;
                    let mut resolution = [w, h];

                    SituationSetShaderUniform(shader, c"uTime".as_ptr(), &mut sim_time as *mut f32 as *mut c_void, SituationUniformType::SIT_UNIFORM_FLOAT);
                    SituationSetShaderUniform(shader, c"uResolution".as_ptr(), &mut resolution as *mut [f32; 2] as *mut c_void, SituationUniformType::SIT_UNIFORM_VEC2);

                    SituationCmdDraw(cmd, 3, 1, 0, 0);

                    let sc = h / 600.0;

                    SituationCmdDrawTextEx(
                        cmd,
                        default_font,
                        c"S I T U A T I O N".as_ptr(),
                        Vector2 { x: w * 0.27, y: 18.0 * sc },
                        24.0 * sc,
                        2.0 * sc,
                        ColorRGBA { r: 255, g: 255, b: 255, a: 220 },
                    );

                    let vsync_on = (window_flags & SituationWindowStateFlags::SITUATION_WINDOW_STATE_VSYNC_HINT as u32) != 0;
                    let fps = SituationGetFPS();
                    let vsync_str = if vsync_on { "ON" } else { "OFF" };
                    let audio_str = if audio_ok { "active" } else { "off" };

                    let line1 = format!("{} FPS  VSync:{}  Audio:{}", fps, vsync_str, audio_str);
                    let line1_c = std::ffi::CString::new(line1).unwrap();
                    SituationCmdDrawTextEx(
                        cmd,
                        default_font,
                        line1_c.as_ptr(),
                        Vector2 { x: 10.0, y: h - 36.0 * sc },
                        8.0 * sc,
                        0.0,
                        ColorRGBA { r: 180, g: 180, b: 180, a: 255 },
                    );

                    let line2 = format!("Reverb: {}%   Delay: {}%   Delay FB: {}%",
                        (reverb_wet * 100.0) as i32,
                        (delay_wet * 100.0) as i32,
                        (delay_feedback * 100.0) as i32
                    );
                    let line2_c = std::ffi::CString::new(line2).unwrap();
                    SituationCmdDrawTextEx(
                        cmd,
                        default_font,
                        line2_c.as_ptr(),
                        Vector2 { x: 10.0, y: h - 20.0 * sc },
                        8.0 * sc,
                        0.0,
                        ColorRGBA { r: 140, g: 210, b: 255, a: 255 },
                    );

                    SituationCmdEndRenderPass(cmd);
                }
                SituationEndFrame();
            }
        }

        if last_note > 0 {
            SituationVirtualMidiNoteOff(last_note);
        }

        SituationUnloadShader(&mut shader);

        if !graph.is_null() {
            SituationSetActiveGraph(std::ptr::null_mut());
            if audio_ok {
                SituationTeardownVirtualMidiLoopback();
            }
            SituationDestroyGraph(graph);
        }

        SituationShutdown();
    }

    println!("Done.");
}
