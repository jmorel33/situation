//! Situation Rust Interactive Demo — Torus Raymarching and Tone Synthesizer
//!
//! Build:
//!   .\build_rust_example.bat hello_situation
//!
//! Run:
//!   .\build\examples\rust\hello_situation.exe

use situation::*;
use std::ffi::CStr;
use std::os::raw::{c_int, c_void};

// Minimal vertex shader — generates a full-screen triangle from gl_VertexID alone.
// No vertex buffer needed; call situation_cmd_draw(cmd, 3, 1, 0, 0).
const VERT_SRC: &CStr = c"#version 460
void main() {
    vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}";

// Fragment shader: animated raster bars as background + raymarched spinning torus.
// Uniforms: uTime (float, seconds), uResolution (vec2, pixels).
const FRAG_SRC: &CStr = c"#version 460
layout(location = 0) out vec4 fragColor;
layout(location = 0) uniform float uTime;
layout(location = 1) uniform vec2 uResolution;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);
    float x_norm = gl_FragCoord.x / uResolution.x;
    vec3 bg = vec3(0.02, 0.02, 0.05);
    for (int i = 0; i < 6; i++) {
        float phase  = uTime * 0.3 + float(i) * 1.05;
        float center = 0.5 + 0.35 * sin(phase);
        float dist   = abs(x_norm - center);
        float glow   = exp(-dist * dist * 200.0);
        float hue    = float(i) / 6.0 + uTime * 0.02;
        bg += hsv2rgb(vec3(hue, 0.9, 1.0)) * glow * 0.6;
    }
    vec3 ro = vec3(0.0, 0.0, -3.5);
    vec3 rd = normalize(vec3(uv, 1.2));
    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);
    float t = 0.0;
    float d = 0.0;
    for (int i = 0; i < 64; i++) {
        vec3 p = ro + rd * t;
        p = rot * p;
        d = sdTorus(p, vec2(1.0, 0.38));
        if (d < 0.001 || t > 10.0) break;
        t += d;
    }
    vec3 col = bg;
    if (d < 0.001) {
        vec3 p = rot * (ro + rd * t);
        vec2 tt = vec2(1.0, 0.38);
        vec2 e  = vec2(0.001, 0.0);
        vec3 n  = normalize(vec3(
            sdTorus(p + e.xyy, tt) - sdTorus(p - e.xyy, tt),
            sdTorus(p + e.yxy, tt) - sdTorus(p - e.yxy, tt),
            sdTorus(p + e.yyx, tt) - sdTorus(p - e.yyx, tt)
        ));
        vec3  light   = normalize(vec3(0.4, 0.8, -0.5));
        float diff    = max(dot(n, light), 0.0);
        float spec    = pow(max(dot(reflect(-light, n), normalize(-rd)), 0.0), 32.0);
        float rim     = pow(1.0 - max(dot(n, normalize(-rd)), 0.0), 3.0);
        float angle  = atan(p.z, p.x);
        float hue    = fract(angle * 0.5 + uTime * 0.08);
        vec3 baseCol = hsv2rgb(vec3(hue, 0.7, 0.9));
        col = baseCol * (0.15 + diff * 0.7)
            + vec3(1.0)  * spec * 0.5
            + hsv2rgb(vec3(hue + 0.3, 0.8, 1.0)) * rim * 0.6;
    }
    float scanline = 0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159);
    fragColor = vec4(col * scanline, 1.0);
}";

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

// RAII cleanup guard to automatically unload resources and shutdown Situation on exit or error.
struct CleanupGuard {
    graph: Option<*mut SituationAudioGraph>,
    audio_ok: bool,
    shader: Option<SituationShader>,
}

impl Drop for CleanupGuard {
    fn drop(&mut self) {
        if let Some(mut shader) = self.shader {
            let _ = situation_unload_shader(&mut shader);
        }
        if let Some(graph) = self.graph {
            let _ = situation_set_active_graph(std::ptr::null_mut());
            if self.audio_ok {
                let _ = situation_teardown_virtual_midi_loopback();
            }
            let _ = situation_destroy_graph(graph);
        }
        let _ = situation_shutdown();
        println!("Situation cleanup complete.");
    }
}

fn main() -> Result<(), SituationError> {
    println!("=== Situation Rust — Raster Bars + Ambient Synth ===");
    println!("  V      Toggle VSync");
    println!("  F      Toggle borderless windowed");
    println!("  Space  Trigger note immediately");
    println!("  + / -  Reverb wet up / down");
    println!("  ] / [  Delay wet up / down");
    println!("  P / O  Delay feedback up / down");
    println!("  ESC    Quit");

    let mut rng = SimpleRng::new(1337);

    let mut config = SituationInitInfo {
        window_width: 900,
        window_height: 600,
        window_title: c"Situation+Rust  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]".as_ptr(),
        ..SituationInitInfo::default()
    };

    situation_init(0, std::ptr::null_mut(), &mut config)?;

    let mut guard = CleanupGuard {
        graph: None,
        audio_ok: false,
        shader: None,
    };

    situation_set_vsync(true);
    situation_init_device_registry();

    let graph = situation_create_graph();
    let mut synth_handle: SituationNodeHandle = 0;
    let mut echo_handle: SituationNodeHandle = 0;
    let mut reverb_handle: SituationNodeHandle = 0;
    let mut midi_device_id: c_int = -1;

    let mut delay_wet: f32 = 0.25;
    let mut delay_feedback: f32 = 0.40;
    let delay_time: f32 = 0.35;
    let mut reverb_wet: f32 = 0.20;

    if let Some(g) = graph {
        guard.graph = Some(g);

        let e1 = situation_create_node(g, SituationNodeType::SITUATION_NODE_TONE_SYNTH, &mut synth_handle);
        let e2 = situation_create_node(g, SituationNodeType::SITUATION_NODE_ECHO, &mut echo_handle);
        let e3 = situation_create_node(g, SituationNodeType::SITUATION_NODE_REVERB, &mut reverb_handle);

        if e1.is_ok() && e2.is_ok() && e3.is_ok() {
            situation_create_patch(g, synth_handle, 0, echo_handle, 0, false)?;
            situation_create_patch(g, echo_handle, 0, reverb_handle, 0, false)?;

            situation_set_control(g, echo_handle, 0, delay_time)?;
            situation_set_control(g, echo_handle, 1, delay_feedback)?;
            situation_set_control(g, echo_handle, 2, delay_wet)?;

            situation_set_control(g, reverb_handle, 0, 0.65)?;
            situation_set_control(g, reverb_handle, 1, 0.55)?;
            situation_set_control(g, reverb_handle, 2, reverb_wet)?;
            situation_set_control(g, reverb_handle, 3, 0.30)?;
            situation_set_control(g, reverb_handle, 4, 0.85)?;

            let midi_ok = situation_setup_virtual_midi_loopback(&mut midi_device_id).is_ok();

            if midi_ok {
                situation_enable_midi_control(g, synth_handle, midi_device_id)?;
            } else {
                println!("WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled");
            }

            situation_set_active_graph(g)?;
            guard.audio_ok = true;

            situation_virtual_midi_control_change(0, 70, 0)?;
            situation_virtual_midi_control_change(0, 73, 90)?;
            situation_virtual_midi_control_change(0, 75, 40)?;
            situation_virtual_midi_control_change(0, 76, 80)?;
            situation_virtual_midi_control_change(0, 72, 110)?;

            println!("Audio graph active: ToneSynth -> Echo -> Reverb");
        } else {
            println!("Node creation failed: synth={:?} echo={:?} reverb={:?}", e1, e2, e3);
        }
    } else {
        println!("WARNING: Could not create audio graph — audio disabled");
    }

    let mut shader = SituationShader::default();
    situation_load_shader_from_memory(VERT_SRC, FRAG_SRC, &mut shader)?;
    guard.shader = Some(shader);

    let default_font = SituationFont::default();
    let mut sim_time: f32 = 0.0;
    let mut note_timer: f32 = 0.0;
    let mut last_note: u8 = 0;

    while !situation_window_should_close() {
        situation_poll_input_events();
        situation_update_timers();

        if situation_is_key_pressed(256) { // ESC
            break;
        }

        let mut window_flags = situation_get_current_actual_window_state_flags();
        if situation_is_key_pressed(86) { // V
            let vsync_on = (window_flags & SituationWindowStateFlags::SITUATION_WINDOW_STATE_VSYNC_HINT as u32) != 0;
            situation_set_vsync(!vsync_on);
            window_flags = situation_get_current_actual_window_state_flags();
        }

        if situation_is_key_pressed(70) { // F
            let _ = situation_toggle_borderless_windowed();
        }

        if situation_is_key_pressed(32) && guard.audio_ok { // Space
            if last_note > 0 {
                let _ = situation_virtual_midi_note_off(last_note);
            }
            let note_idx = rng.gen_range(0, PENTATONIC.len());
            last_note = PENTATONIC[note_idx];
            let velocity = (50 + rng.gen_range(0, 30)) as u8;
            let _ = situation_virtual_midi_note_on(last_note, velocity);
            note_timer = 0.0;
        }

        if let Some(g) = graph {
            if situation_is_key_pressed(61) && guard.audio_ok { // +
                reverb_wet = (reverb_wet + 0.05).min(1.0);
                let _ = situation_set_control(g, reverb_handle, 2, reverb_wet);
            }
            if situation_is_key_pressed(45) && guard.audio_ok { // -
                reverb_wet = (reverb_wet - 0.05).max(0.0);
                let _ = situation_set_control(g, reverb_handle, 2, reverb_wet);
            }

            if situation_is_key_pressed(93) && guard.audio_ok { // ]
                delay_wet = (delay_wet + 0.05).min(1.0);
                let _ = situation_set_control(g, echo_handle, 2, delay_wet);
            }
            if situation_is_key_pressed(91) && guard.audio_ok { // [
                delay_wet = (delay_wet - 0.05).max(0.0);
                let _ = situation_set_control(g, echo_handle, 2, delay_wet);
            }

            if situation_is_key_pressed(80) && guard.audio_ok { // P
                delay_feedback = (delay_feedback + 0.05).min(0.95);
                let _ = situation_set_control(g, echo_handle, 1, delay_feedback);
            }
            if situation_is_key_pressed(79) && guard.audio_ok { // O
                delay_feedback = (delay_feedback - 0.05).max(0.0);
                let _ = situation_set_control(g, echo_handle, 1, delay_feedback);
            }
        }

        let dt = situation_get_frame_time();
        sim_time += dt;
        note_timer += dt;

        if guard.audio_ok && note_timer > 4.0 {
            note_timer = 0.0;
            if last_note > 0 {
                let _ = situation_virtual_midi_note_off(last_note);
            }

            let waveform = rng.gen_range(0, 4) as u8;
            let _ = situation_virtual_midi_control_change(0, 70, waveform * 32);
            let cutoff = (40 + rng.gen_range(0, 80)) as u8;
            let _ = situation_virtual_midi_control_change(0, 74, cutoff);
            let resonance = (20 + rng.gen_range(0, 60)) as u8;
            let _ = situation_virtual_midi_control_change(0, 71, resonance);
            let lfo_rate = rng.gen_range(0, 40) as u8;
            let _ = situation_virtual_midi_control_change(0, 24, lfo_rate);
            let lfo_depth = rng.gen_range(0, 50) as u8;
            let _ = situation_virtual_midi_control_change(0, 26, lfo_depth);

            let note_idx = rng.gen_range(0, PENTATONIC.len());
            last_note = PENTATONIC[note_idx];
            let velocity = (30 + rng.gen_range(0, 30)) as u8;
            let _ = situation_virtual_midi_note_on(last_note, velocity);
        }

        if situation_acquire_frame_command_buffer().is_ok() {
            if let Some(cmd) = situation_get_main_command_buffer() {
                let _ = situation_cmd_begin_render_to_display(cmd, -1, ColorRGBA { r: 0, g: 0, b: 0, a: 255 });
                let _ = situation_cmd_bind_pipeline(cmd, shader);

                let w = situation_get_render_width() as f32;
                let h = situation_get_render_height() as f32;
                let mut resolution = [w, h];

                let _ = situation_set_shader_uniform(shader, c"uTime", &mut sim_time as *mut f32 as *mut c_void, SituationUniformType::SIT_UNIFORM_FLOAT);
                let _ = situation_set_shader_uniform(shader, c"uResolution", &mut resolution as *mut [f32; 2] as *mut c_void, SituationUniformType::SIT_UNIFORM_VEC2);

                let _ = situation_cmd_draw(cmd, 3, 1, 0, 0);

                let sc = h / 600.0;

                let _ = situation_cmd_draw_text_ex(
                    cmd,
                    default_font,
                    c"S I T U A T I O N",
                    Vector2 { x: w * 0.27, y: 18.0 * sc },
                    24.0 * sc,
                    2.0 * sc,
                    ColorRGBA { r: 255, g: 255, b: 255, a: 220 },
                );

                let vsync_on = (window_flags & SituationWindowStateFlags::SITUATION_WINDOW_STATE_VSYNC_HINT as u32) != 0;
                let fps = situation_get_fps();
                let vsync_str = if vsync_on { "ON" } else { "OFF" };
                let audio_str = if guard.audio_ok { "active" } else { "off" };

                let line1 = format!("{} FPS  VSync:{}  Audio:{}", fps, vsync_str, audio_str);
                let line1_c = std::ffi::CString::new(line1).unwrap();
                let _ = situation_cmd_draw_text_ex(
                    cmd,
                    default_font,
                    &line1_c,
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
                let _ = situation_cmd_draw_text_ex(
                    cmd,
                    default_font,
                    &line2_c,
                    Vector2 { x: 10.0, y: h - 20.0 * sc },
                    8.0 * sc,
                    0.0,
                    ColorRGBA { r: 140, g: 210, b: 255, a: 255 },
                );

                let _ = situation_cmd_end_render_pass(cmd);
            }
            let _ = situation_end_frame();
        }
    }

    if last_note > 0 {
        let _ = situation_virtual_midi_note_off(last_note);
    }

    Ok(())
}
