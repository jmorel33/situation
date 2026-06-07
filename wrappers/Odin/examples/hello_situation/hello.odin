package hello_situation

import sit "../../"
import "core:fmt"
import "core:c"
import "core:math/rand"

// =============================================================================
//  Situation + Odin — Raster Bars with Ambient Synth
//
//  Demonstrates:
//    • Raymarched torus with iridescent lighting + animated raster-bar backdrop
//    • Audio node graph: ToneSynth -> Echo (delay) -> Reverb
//    • Interactive FX control: reverb wet, delay wet, delay feedback
//    • Virtual MIDI loopback for programmatic note triggering
//    • VSync state read directly from Situation (no stale local bool)
//
//  Controls:
//    V          Toggle VSync
//    F          Toggle borderless windowed
//    Space      Trigger a random pentatonic note immediately
//    + / -      Reverb wet  up / down
//    ] / [      Delay wet   up / down
//    P / O      Delay feedback up / down
//    ESC        Quit
//
//  Audio signal chain:
//    ToneSynth  ->  Echo (delay 350 ms)  ->  Reverb  ->  device output
//
//  Echo control IDs (SituationSetControl):
//    0 = delay_time   (seconds)
//    1 = feedback     (0..0.95)
//    2 = wet_level    (0..1)
//
//  Reverb control IDs:
//    0 = room_size    (0..1)
//    1 = damping      (0..1)
//    2 = wet_level    (0..1)
//    3 = dry_level    (0..1)
//    4 = width        (0..1)
// =============================================================================

// Minimal vertex shader — generates a full-screen triangle from gl_VertexID alone.
// No vertex buffer needed; call SituationCmdDraw(cmd, 3, 1, 0, 0).
VERT_SRC :: `#version 460
void main() {
    vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}
`

// Fragment shader: animated raster bars as background + raymarched spinning torus.
// Uniforms: uTime (float, seconds), uResolution (vec2, pixels).
FRAG_SRC :: `#version 460
layout(location = 0) out vec4 fragColor;
layout(location = 0) uniform float uTime;
layout(location = 1) uniform vec2 uResolution;

// Convert HSV to RGB — used for iridescent torus and bar colours.
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Signed-distance field for a torus at the origin.
// t.x = major radius, t.y = tube radius.
float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);

    // --- Background: six animated Gaussian glowing bars ---
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

    // --- Foreground: sphere-march the torus ---
    vec3 ro = vec3(0.0, 0.0, -3.5);                 // ray origin (camera)
    vec3 rd = normalize(vec3(uv, 1.2));              // ray direction

    // Slowly wobble the torus orientation over time
    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);

    float t = 0.0;
    float d;
    for (int i = 0; i < 64; i++) {
        vec3 p = ro + rd * t;
        p = rot * p;
        d = sdTorus(p, vec2(1.0, 0.38));
        if (d < 0.001 || t > 10.0) break;
        t += d;
    }

    vec3 col = bg;
    if (d < 0.001) {
        // We hit the torus — shade it
        vec3 p = rot * (ro + rd * t);

        // Approximate surface normal via central differences
        vec2 tt = vec2(1.0, 0.38);
        vec2 e  = vec2(0.001, 0.0);
        vec3 n  = normalize(vec3(
            sdTorus(p + e.xyy, tt) - sdTorus(p - e.xyy, tt),
            sdTorus(p + e.yxy, tt) - sdTorus(p - e.yxy, tt),
            sdTorus(p + e.yyx, tt) - sdTorus(p - e.yyx, tt)
        ));

        // Blinn-Phong + rim lighting
        vec3  light   = normalize(vec3(0.4, 0.8, -0.5));
        float diff    = max(dot(n, light), 0.0);
        float spec    = pow(max(dot(reflect(-light, n), normalize(-rd)), 0.0), 32.0);
        float rim     = pow(1.0 - max(dot(n, normalize(-rd)), 0.0), 3.0);

        // Iridescent hue driven by polar angle + time
        float angle  = atan(p.z, p.x);
        float hue    = fract(angle * 0.5 + uTime * 0.08);
        vec3 baseCol = hsv2rgb(vec3(hue, 0.7, 0.9));

        col = baseCol * (0.15 + diff * 0.7)
            + vec3(1.0)  * spec * 0.5
            + hsv2rgb(vec3(hue + 0.3, 0.8, 1.0)) * rim * 0.6;
    }

    // Subtle scanline effect
    float scanline = 0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159);
    fragColor = vec4(col * scanline, 1.0);
}
`

// Pentatonic scale — MIDI note numbers across three octaves.
// Used for both Space-triggered notes and the auto-note timer.
PENTATONIC :: [?]u8{48, 52, 55, 60, 64, 67, 72, 76, 79, 84}

// SITUATION_FLAG_VSYNC_HINT (from situation_api.h) — used to query real vsync state
// rather than tracking a local bool that can drift from the library's actual setting.
VSYNC_FLAG :: u32(0x00001000)

main :: proc() {
    fmt.println("=== Situation Odin — Raster Bars + Ambient Synth ===")
    fmt.println("  V      Toggle VSync")
    fmt.println("  F      Toggle borderless windowed")
    fmt.println("  Space  Trigger note immediately")
    fmt.println("  + / -  Reverb wet up / down")
    fmt.println("  ] / [  Delay wet up / down")
    fmt.println("  P / O  Delay feedback up / down")
    fmt.println("  ESC    Quit")

    config := sit.Situation_Init_Info{
        window_width  = 900,
        window_height = 600,
        window_title  = "Situation+Odin  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]",
    }

    err := sit.SituationInit(0, nil, &config)
    if err != .SITUATION_SUCCESS {
        fmt.printf("SituationInit failed: %v\n", err)
        return
    }
    defer sit.SituationShutdown()

    // Request VSync on at startup; actual state is always queried from Situation
    // via SituationGetCurrentActualWindowStateFlags() — no local tracking variable.
    sit.SituationSetVSync(b8(true))

    // -------------------------------------------------------------------------
    //  Audio graph: ToneSynth -> Echo -> Reverb -> device output
    // -------------------------------------------------------------------------
    sit.SituationInitDeviceRegistry()

    graph := sit.SituationCreateGraph()
    if graph == nil {
        fmt.println("WARNING: Could not create audio graph — audio disabled")
    }

    synth_handle:   sit.Situation_Node_Handle
    echo_handle:    sit.Situation_Node_Handle
    reverb_handle:  sit.Situation_Node_Handle
    midi_device_id: c.int = -1
    audio_ok := false

    // These mirror the current control values so the HUD can display them.
    // They are the authoritative source — we push them to Situation via SetControl.
    delay_wet:      f32 = 0.25
    delay_feedback: f32 = 0.40
    delay_time:     f32 = 0.35  // seconds — fixed in this demo; expose as a key if desired
    reverb_wet:     f32 = 0.20

    if graph != nil {
        e1 := sit.SituationCreateNode(graph, .SITUATION_NODE_TONE_SYNTH, &synth_handle)
        e2 := sit.SituationCreateNode(graph, .SITUATION_NODE_ECHO,       &echo_handle)
        e3 := sit.SituationCreateNode(graph, .SITUATION_NODE_REVERB,     &reverb_handle)

        if e1 == .SITUATION_SUCCESS && e2 == .SITUATION_SUCCESS && e3 == .SITUATION_SUCCESS {

            // Wire the chain: synth -> echo -> reverb
            // SituationCreatePatch(graph, src, src_port, dst, dst_port, is_control)
            sit.SituationCreatePatch(graph, synth_handle, 0, echo_handle,   0, b8(false))
            sit.SituationCreatePatch(graph, echo_handle,  0, reverb_handle, 0, b8(false))

            // Echo initial settings
            // Control 0 = delay_time (seconds), 1 = feedback, 2 = wet_level
            sit.SituationSetControl(graph, echo_handle, 0, delay_time)      // delay length
            sit.SituationSetControl(graph, echo_handle, 1, delay_feedback)  // how much repeats
            sit.SituationSetControl(graph, echo_handle, 2, delay_wet)       // echo send level

            // Reverb initial settings
            // Control 0 = room_size, 1 = damping, 2 = wet_level, 3 = dry_level, 4 = width
            sit.SituationSetControl(graph, reverb_handle, 0, 0.65)      // large room
            sit.SituationSetControl(graph, reverb_handle, 1, 0.55)      // moderate damping
            sit.SituationSetControl(graph, reverb_handle, 2, reverb_wet)
            sit.SituationSetControl(graph, reverb_handle, 3, 0.30)      // dry signal level
            sit.SituationSetControl(graph, reverb_handle, 4, 0.85)      // stereo width

            // Virtual MIDI loopback — optional, only available when PortMidi is present.
            // If unavailable the synth still plays via the auto-note timer.
            midi_ok := sit.SituationSetupVirtualMidiLoopback(&midi_device_id) == .SITUATION_SUCCESS
            if midi_ok {
                // Route MIDI from the virtual loopback device into the tone synth node
                sit.SituationEnableMidiControl(graph, synth_handle, midi_device_id)
            } else {
                fmt.println("WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled")
                midi_device_id = -1
            }

            // Activate graph — audio thread starts processing from next callback
            sit.SituationSetActiveGraph(graph)
            audio_ok = true

            // Program the initial synth voice via MIDI CC on channel 0
            sit.SituationVirtualMidiControlChange(0, 70, 0)    // CC70 = sine waveform (0=sine, 32=square, 64=tri, 96=saw)
            sit.SituationVirtualMidiControlChange(0, 73, 90)   // CC73 = attack time (long, for ambient feel)
            sit.SituationVirtualMidiControlChange(0, 75, 40)   // CC75 = decay
            sit.SituationVirtualMidiControlChange(0, 76, 80)   // CC76 = sustain level
            sit.SituationVirtualMidiControlChange(0, 72, 110)  // CC72 = release time

            fmt.println("Audio graph active: ToneSynth -> Echo -> Reverb")
        } else {
            fmt.printf("Node creation failed: synth=%v echo=%v reverb=%v\n", e1, e2, e3)
        }
    }

    // Defer graph teardown: deactivate first so the audio thread stops referencing
    // the graph, then tear down MIDI, then free the graph itself.
    // The DestroyGraph double-wait (added in v2.4.215) ensures no audio callback
    // can race with the free.
    defer {
        if graph != nil {
            sit.SituationSetActiveGraph(nil)
            if audio_ok { sit.SituationTeardownVirtualMidiLoopback() }
            sit.SituationDestroyGraph(graph)
        }
    }

    // -------------------------------------------------------------------------
    //  Compile the raster-bars + torus shader
    // -------------------------------------------------------------------------
    shader := sit.Situation_Shader{}
    err = sit.SituationLoadShaderFromMemory(VERT_SRC, FRAG_SRC, &shader)
    if err != .SITUATION_SUCCESS {
        fmt.printf("Shader compile failed: %v\n", err)
        return
    }
    defer sit.SituationUnloadShader(&shader)

    default_font := sit.Situation_Font{}  // uses the library's built-in bitmap font
    sim_time:    f32 = 0.0
    note_timer:  f32 = 0.0
    last_note:   u8  = 0
    fps_buf: [256]u8   // scratch buffer for fmt.bprintf HUD lines

    // -------------------------------------------------------------------------
    //  Main loop
    // -------------------------------------------------------------------------
    for !sit.SituationWindowShouldClose() {
        sit.SituationPollInputEvents()
        sit.SituationUpdateTimers()

        if sit.SituationIsKeyPressed(256) { break }  // ESC

        // V — toggle VSync.
        // SituationGetCurrentActualWindowStateFlags is now O(1) (cached by the library
        // after each SituationPollInputEvents and invalidated by SetWindowState).
        window_flags := sit.SituationGetCurrentActualWindowStateFlags()
        if sit.SituationIsKeyPressed(86) {
            vsync_on := (window_flags & VSYNC_FLAG) != 0
            sit.SituationSetVSync(b8(!vsync_on))
            window_flags = sit.SituationGetCurrentActualWindowStateFlags()  // pick up new state immediately
        }
        // F — borderless windowed toggle (keeps the effect visible while going "fullscreen")
        if sit.SituationIsKeyPressed(70) {
            sit.SituationToggleBorderlessWindowed()
        }

        // Space — trigger a random note from the pentatonic scale immediately
        if sit.SituationIsKeyPressed(32) && audio_ok {
            if last_note > 0 { sit.SituationVirtualMidiNoteOff(last_note) }
            scale     := PENTATONIC
            note_idx  := rand.uint32() % u32(len(PENTATONIC))
            last_note  = scale[note_idx]
            sit.SituationVirtualMidiNoteOn(last_note, u8(50 + rand.uint32() % 30))
            note_timer = 0.0
        }

        // +/= — reverb wet up, - — reverb wet down
        if sit.SituationIsKeyPressed(61) && audio_ok {
            reverb_wet = min(reverb_wet + 0.05, 1.0)
            sit.SituationSetControl(graph, reverb_handle, 2, reverb_wet)
        }
        if sit.SituationIsKeyPressed(45) && audio_ok {
            reverb_wet = max(reverb_wet - 0.05, 0.0)
            sit.SituationSetControl(graph, reverb_handle, 2, reverb_wet)
        }

        // ] — delay wet up (key 93), [ — delay wet down (key 91)
        if sit.SituationIsKeyPressed(93) && audio_ok {
            delay_wet = min(delay_wet + 0.05, 1.0)
            sit.SituationSetControl(graph, echo_handle, 2, delay_wet)
        }
        if sit.SituationIsKeyPressed(91) && audio_ok {
            delay_wet = max(delay_wet - 0.05, 0.0)
            sit.SituationSetControl(graph, echo_handle, 2, delay_wet)
        }

        // P — delay feedback up (key 80), O — delay feedback down (key 79)
        // Capped at 0.95 to prevent runaway infinite feedback
        if sit.SituationIsKeyPressed(80) && audio_ok {
            delay_feedback = min(delay_feedback + 0.05, 0.95)
            sit.SituationSetControl(graph, echo_handle, 1, delay_feedback)
        }
        if sit.SituationIsKeyPressed(79) && audio_ok {
            delay_feedback = max(delay_feedback - 0.05, 0.0)
            sit.SituationSetControl(graph, echo_handle, 1, delay_feedback)
        }

        dt          := sit.SituationGetFrameTime()
        sim_time    += dt
        note_timer  += dt

        // Auto-note: every ~4 seconds pick a random note and randomise the synth voice
        if audio_ok && note_timer > 4.0 {
            note_timer = 0.0
            if last_note > 0 { sit.SituationVirtualMidiNoteOff(last_note) }

            // Randomise synth timbre via MIDI CC
            waveform := u8(rand.uint32() % 4)
            sit.SituationVirtualMidiControlChange(0, 70, waveform * 32)              // waveform (0=sine,32=sq,64=tri,96=saw)
            sit.SituationVirtualMidiControlChange(0, 74, u8(40 + rand.uint32() % 80)) // CC74 filter cutoff
            sit.SituationVirtualMidiControlChange(0, 71, u8(20 + rand.uint32() % 60)) // CC71 resonance
            sit.SituationVirtualMidiControlChange(0, 24, u8(rand.uint32() % 40))      // CC24 LFO rate
            sit.SituationVirtualMidiControlChange(0, 26, u8(rand.uint32() % 50))      // CC26 LFO pitch depth

            scale     := PENTATONIC
            note_idx  := rand.uint32() % u32(len(PENTATONIC))
            last_note  = scale[note_idx]
            sit.SituationVirtualMidiNoteOn(last_note, u8(30 + rand.uint32() % 30))   // soft velocity
        }

        // -----------------------------------------------------------------------
        //  Render
        // -----------------------------------------------------------------------
        if sit.SituationAcquireFrameCommandBuffer() == .SITUATION_SUCCESS {
            cmd := sit.SituationGetMainCommandBuffer()
            if cmd != nil {
                sit.SituationCmdBeginRenderToDisplay(cmd, -1, sit.Color_RGBA{0, 0, 0, 255})

                // Draw the full-screen raster-bars + torus shader
                sit.SituationCmdBindPipeline(cmd, shader)
                w := f32(sit.SituationGetRenderWidth())
                h := f32(sit.SituationGetRenderHeight())
                resolution := [2]f32{w, h}
                sit.SituationSetShaderUniform(shader, "uTime",       &sim_time,   .SIT_UNIFORM_FLOAT)
                sit.SituationSetShaderUniform(shader, "uResolution", &resolution, .SIT_UNIFORM_VEC2)
                sit.SituationCmdDraw(cmd, 3, 1, 0, 0)  // 3 verts = full-screen triangle

                // Scale UI elements relative to render height so they look consistent
                // at any resolution (anchor point: 600 px = 1.0x scale)
                sc := h / 600.0

                // Title bar
                sit.SituationCmdDrawTextEx(
                    cmd, default_font, "S I T U A T I O N",
                    sit.Vector2{w * 0.27, 18 * sc},
                    24 * sc, 2.0 * sc,
                    sit.Color_RGBA{255, 255, 255, 220})

                // HUD line 1 — system status
                // window_flags is the cached value from the top of this frame.
                // The library refreshes it after every SituationPollInputEvents call,
                // so this is an O(1) read with always-accurate state.
                vsync_on  := (window_flags & VSYNC_FLAG) != 0
                fps       := sit.SituationGetFPS()
                vsync_str := vsync_on ? "ON" : "OFF"
                audio_str := audio_ok ? "active" : "off"
                line1     := fmt.bprintf(fps_buf[:128], "%d FPS  VSync:%s  Audio:%s", fps, vsync_str, audio_str)
                sit.SituationCmdDrawTextEx(
                    cmd, default_font, cstring(raw_data(line1)),
                    sit.Vector2{10, h - 36 * sc},
                    8 * sc, 0,
                    sit.Color_RGBA{180, 180, 180, 255})

                // HUD line 2 — FX levels
                // Displayed in a tinted blue so it's visually distinct from the status line
                line2 := fmt.bprintf(fps_buf[128:], "Reverb: %.0f%%   Delay: %.0f%%   Delay FB: %.0f%%",
                    reverb_wet * 100, delay_wet * 100, delay_feedback * 100)
                sit.SituationCmdDrawTextEx(
                    cmd, default_font, cstring(raw_data(fps_buf[128:])),
                    sit.Vector2{10, h - 20 * sc},
                    8 * sc, 0,
                    sit.Color_RGBA{140, 210, 255, 255})

                _ = line1
                _ = line2

                sit.SituationCmdEndRenderPass(cmd)
            }
            sit.SituationEndFrame()
        }
    }

    // Release the last held note cleanly before shutdown
    if last_note > 0 { sit.SituationVirtualMidiNoteOff(last_note) }
    fmt.println("Done.")
}
