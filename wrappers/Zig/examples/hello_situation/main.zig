const std = @import("std");
const situation = @import("situation");

// =============================================================================
//  Situation + Zig — Raster Bars with Ambient Synth
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
// =============================================================================

// Minimal vertex shader — generates a full-screen triangle from gl_VertexID alone.
// No vertex buffer needed; call SituationCmdDraw(cmd, 3, 1, 0, 0).
const VERT_SRC =
    \\#version 460
    \\void main() {
    \\    vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);
    \\    gl_Position = vec4(pos, 0.0, 1.0);
    \\}
;

// Fragment shader: animated raster bars as background + raymarched spinning torus.
// Uniforms: uTime (float, seconds), uResolution (vec2, pixels).
const FRAG_SRC =
    \\#version 460
    \\layout(location = 0) out vec4 fragColor;
    \\layout(location = 0) uniform float uTime;
    \\layout(location = 1) uniform vec2 uResolution;
    \\
    \\// Convert HSV to RGB — used for iridescent torus and bar colours.
    \\vec3 hsv2rgb(vec3 c) {
    \\    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    \\    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    \\    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    \\}
    \\
    \\// Signed-distance field for a torus at the origin.
    \\// t.x = major radius, t.y = tube radius.
    \\float sdTorus(vec3 p, vec2 t) {
    \\    vec2 q = vec2(length(p.xz) - t.x, p.y);
    \\    return length(q) - t.y;
    \\}
    \\
    \\mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }
    \\mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }
    \\
    \\void main() {
    \\    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);
    \\
    \\    // --- Background: six animated Gaussian glowing bars ---
    \\    float x_norm = gl_FragCoord.x / uResolution.x;
    \\    vec3 bg = vec3(0.02, 0.02, 0.05);
    \\    for (int i = 0; i < 6; i++) {
    \\        float phase  = uTime * 0.3 + float(i) * 1.05;
    \\        float center = 0.5 + 0.35 * sin(phase);
    \\        float dist   = abs(x_norm - center);
    \\        float glow   = exp(-dist * dist * 200.0);
    \\        float hue    = float(i) / 6.0 + uTime * 0.02;
    \\        bg += hsv2rgb(vec3(hue, 0.9, 1.0)) * glow * 0.6;
    \\    }
    \\
    \\    // --- Foreground: sphere-march the torus ---
    \\    vec3 ro = vec3(0.0, 0.0, -3.5);                 // ray origin (camera)
    \\    vec3 rd = normalize(vec3(uv, 1.2));              // ray direction
    \\
    \\    // Slowly wobble the torus orientation over time
    \\    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);
    \\
    \\    float t = 0.0;
    \\    float d = 0.0;
    \\    for (int i = 0; i < 64; i++) {
    \\        vec3 p = ro + rd * t;
    \\        p = rot * p;
    \\        d = sdTorus(p, vec2(1.0, 0.38));
    \\        if (d < 0.001 || t > 10.0) break;
    \\        t += d;
    \\    }
    \\
    \\    vec3 col = bg;
    \\    if (d < 0.001) {
    \\        // We hit the torus — shade it
    \\        vec3 p = rot * (ro + rd * t);
    \\
    \\        // Approximate surface normal via central differences
    \\        vec2 tt = vec2(1.0, 0.38);
    \\        vec2 e  = vec2(0.001, 0.0);
    \\        vec3 n  = normalize(vec3(
    \\            sdTorus(p + e.xyy, tt) - sdTorus(p - e.xyy, tt),
    \\            sdTorus(p + e.yxy, tt) - sdTorus(p - e.yxy, tt),
    \\            sdTorus(p + e.yyx, tt) - sdTorus(p - e.yyx, tt)
    \\        ));
    \\
    \\        // Blinn-Phong + rim lighting
    \\        vec3  light   = normalize(vec3(0.4, 0.8, -0.5));
    \\        float diff    = max(dot(n, light), 0.0);
    \\        float spec    = pow(max(dot(reflect(-light, n), normalize(-rd)), 0.0), 32.0);
    \\        float rim     = pow(1.0 - max(dot(n, normalize(-rd)), 0.0), 3.0);
    \\
    \\        // Iridescent hue driven by polar angle + time
    \\        float angle  = atan(p.z, p.x);
    \\        float hue    = fract(angle * 0.5 + uTime * 0.08);
    \\        vec3 baseCol = hsv2rgb(vec3(hue, 0.7, 0.9));
    \\
    \\        col = baseCol * (0.15 + diff * 0.7)
    \\            + vec3(1.0)  * spec * 0.5
    \\            + hsv2rgb(vec3(hue + 0.3, 0.8, 1.0)) * rim * 0.6;
    \\    }
    \\
    \\    // Subtle scanline effect
    \\    float scanline = 0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159);
    \\    fragColor = vec4(col * scanline, 1.0);
    \\}
;

// Pentatonic scale — MIDI note numbers across three octaves.
const PENTATONIC = [_]u8{ 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 };

// SITUATION_FLAG_VSYNC_HINT (from situation_api.h) — used to query real vsync state
const VSYNC_FLAG: u32 = 0x00001000;

pub fn main() !void {
    std.debug.print("=== Situation Zig — Raster Bars + Ambient Synth ===\n", .{});
    std.debug.print("  V      Toggle VSync\n", .{});
    std.debug.print("  F      Toggle borderless windowed\n", .{});
    std.debug.print("  Space  Trigger note immediately\n", .{});
    std.debug.print("  + / -  Reverb wet up / down\n", .{});
    std.debug.print("  ] / [  Delay wet up / down\n", .{});
    std.debug.print("  P / O  Delay feedback up / down\n", .{});
    std.debug.print("  ESC    Quit\n", .{});

    var prng = std.Random.DefaultPrng.init(1337);
    const random = prng.random();

    var config = std.mem.zeroes(situation.SituationInitInfo);
    config.window_width = 900;
    config.window_height = 600;
    config.window_title = "Situation+Zig  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]";

    const err = situation.foreign.SituationInit(0, null, &config);
    if (err != .SITUATION_SUCCESS) {
        std.debug.print("SituationInit failed: {s}\n", .{situation.foreign.SituationErrorToString(err)});
        return;
    }
    defer situation.foreign.SituationShutdown();

    // Request VSync on at startup
    situation.foreign.SituationSetVSync(true);

    // -------------------------------------------------------------------------
    //  Audio graph: ToneSynth -> Echo -> Reverb -> device output
    // -------------------------------------------------------------------------
    situation.foreign.SituationInitDeviceRegistry();

    const graph = situation.foreign.SituationCreateGraph();
    if (@intFromPtr(graph) == 0) {
        std.debug.print("WARNING: Could not create audio graph — audio disabled\n", .{});
    }

    var synth_handle: situation.SituationNodeHandle = 0;
    var echo_handle: situation.SituationNodeHandle = 0;
    var reverb_handle: situation.SituationNodeHandle = 0;
    var midi_device_id: c_int = -1;
    var audio_ok = false;

    // These mirror the current control values so the HUD can display them.
    var delay_wet: f32 = 0.25;
    var delay_feedback: f32 = 0.40;
    const delay_time: f32 = 0.35; // seconds
    var reverb_wet: f32 = 0.20;

    if (@intFromPtr(graph) != 0) {
        const e1 = situation.foreign.SituationCreateNode(graph, .SITUATION_NODE_TONE_SYNTH, &synth_handle);
        const e2 = situation.foreign.SituationCreateNode(graph, .SITUATION_NODE_ECHO, &echo_handle);
        const e3 = situation.foreign.SituationCreateNode(graph, .SITUATION_NODE_REVERB, &reverb_handle);

        if (e1 == .SITUATION_SUCCESS and e2 == .SITUATION_SUCCESS and e3 == .SITUATION_SUCCESS) {
            // Wire the chain: synth -> echo -> reverb
            _ = situation.foreign.SituationCreatePatch(graph, synth_handle, 0, echo_handle, 0, false);
            _ = situation.foreign.SituationCreatePatch(graph, echo_handle, 0, reverb_handle, 0, false);

            // Echo initial settings
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 0, delay_time);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 1, delay_feedback);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 2, delay_wet);

            // Reverb initial settings
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 0, 0.65);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 1, 0.55);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 2, reverb_wet);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 3, 0.30);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 4, 0.85);

            // Virtual MIDI loopback
            const midi_ok = situation.foreign.SituationSetupVirtualMidiLoopback(&midi_device_id) == .SITUATION_SUCCESS;
            if (midi_ok) {
                _ = situation.foreign.SituationEnableMidiControl(graph, synth_handle, midi_device_id);
            } else {
                std.debug.print("WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled\n", .{});
                midi_device_id = -1;
            }

            _ = situation.foreign.SituationSetActiveGraph(graph);
            audio_ok = true;

            // Program the initial synth voice via MIDI CC on channel 0
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 70, 0);   // sine waveform
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 73, 90);  // attack time
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 75, 40);  // decay
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 76, 80);  // sustain
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 72, 110); // release

            std.debug.print("Audio graph active: ToneSynth -> Echo -> Reverb\n", .{});
        } else {
            std.debug.print("Node creation failed: synth={s} echo={s} reverb={s}\n", .{
                situation.foreign.SituationErrorToString(e1),
                situation.foreign.SituationErrorToString(e2),
                situation.foreign.SituationErrorToString(e3),
            });
        }
    }

    defer {
        if (@intFromPtr(graph) != 0) {
            const setActiveGraph: *const fn (?*situation.types.SituationAudioGraph) callconv(.c) situation.types.SituationError = @ptrCast(&situation.foreign.SituationSetActiveGraph);
            _ = setActiveGraph(null);
            if (audio_ok) {
                _ = situation.foreign.SituationTeardownVirtualMidiLoopback();
            }
            situation.foreign.SituationDestroyGraph(graph);
        }
    }

    // -------------------------------------------------------------------------
    //  Compile the raster-bars + torus shader
    // -------------------------------------------------------------------------
    var shader = std.mem.zeroes(situation.SituationShader);
    const shader_err = situation.foreign.SituationLoadShaderFromMemory(VERT_SRC, FRAG_SRC, &shader);
    if (shader_err != .SITUATION_SUCCESS) {
        std.debug.print("Shader compile failed: {s}\n", .{situation.foreign.SituationErrorToString(shader_err)});
        return;
    }
    defer situation.foreign.SituationUnloadShader(&shader);

    const default_font = std.mem.zeroes(situation.SituationFont);
    var sim_time: f32 = 0.0;
    var note_timer: f32 = 0.0;
    var last_note: u8 = 0;
    var fps_buf: [128]u8 = undefined;
    var fx_buf: [128]u8 = undefined;

    // -------------------------------------------------------------------------
    //  Main loop
    // -------------------------------------------------------------------------
    while (!situation.foreign.SituationWindowShouldClose()) {
        situation.foreign.SituationPollInputEvents();
        situation.foreign.SituationUpdateTimers();

        if (situation.foreign.SituationIsKeyPressed(256)) { // ESC
            break;
        }

        var window_flags = situation.foreign.SituationGetCurrentActualWindowStateFlags();
        if (situation.foreign.SituationIsKeyPressed(86)) { // V
            const vsync_on = (window_flags & VSYNC_FLAG) != 0;
            situation.foreign.SituationSetVSync(!vsync_on);
            window_flags = situation.foreign.SituationGetCurrentActualWindowStateFlags();
        }

        if (situation.foreign.SituationIsKeyPressed(70)) { // F
            _ = situation.foreign.SituationToggleBorderlessWindowed();
        }

        if (situation.foreign.SituationIsKeyPressed(32) and audio_ok) { // Space
            if (last_note > 0) {
                _ = situation.foreign.SituationVirtualMidiNoteOff(last_note);
            }
            const note_idx = random.intRangeLessThan(usize, 0, PENTATONIC.len);
            last_note = PENTATONIC[note_idx];
            const velocity = @as(u8, @intCast(50 + random.intRangeLessThan(u32, 0, 30)));
            _ = situation.foreign.SituationVirtualMidiNoteOn(last_note, velocity);
            note_timer = 0.0;
        }

        if (situation.foreign.SituationIsKeyPressed(61) and audio_ok) { // +
            reverb_wet = @min(reverb_wet + 0.05, 1.0);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 2, reverb_wet);
        }
        if (situation.foreign.SituationIsKeyPressed(45) and audio_ok) { // -
            reverb_wet = @max(reverb_wet - 0.05, 0.0);
            _ = situation.foreign.SituationSetControl(graph, reverb_handle, 2, reverb_wet);
        }

        if (situation.foreign.SituationIsKeyPressed(93) and audio_ok) { // ]
            delay_wet = @min(delay_wet + 0.05, 1.0);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 2, delay_wet);
        }
        if (situation.foreign.SituationIsKeyPressed(91) and audio_ok) { // [
            delay_wet = @max(delay_wet - 0.05, 0.0);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 2, delay_wet);
        }

        if (situation.foreign.SituationIsKeyPressed(80) and audio_ok) { // P
            delay_feedback = @min(delay_feedback + 0.05, 0.95);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 1, delay_feedback);
        }
        if (situation.foreign.SituationIsKeyPressed(79) and audio_ok) { // O
            delay_feedback = @max(delay_feedback - 0.05, 0.0);
            _ = situation.foreign.SituationSetControl(graph, echo_handle, 1, delay_feedback);
        }

        const dt = situation.foreign.SituationGetFrameTime();
        sim_time += dt;
        note_timer += dt;

        // Auto-note: every ~4 seconds pick a random note and randomise the synth voice
        if (audio_ok and note_timer > 4.0) {
            note_timer = 0.0;
            if (last_note > 0) {
                _ = situation.foreign.SituationVirtualMidiNoteOff(last_note);
            }

            const waveform = @as(u8, @intCast(random.intRangeLessThan(u32, 0, 4)));
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 70, waveform * 32);
            const cutoff = @as(u8, @intCast(40 + random.intRangeLessThan(u32, 0, 80)));
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 74, cutoff);
            const resonance = @as(u8, @intCast(20 + random.intRangeLessThan(u32, 0, 60)));
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 71, resonance);
            const lfo_rate = @as(u8, @intCast(random.intRangeLessThan(u32, 0, 40)));
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 24, lfo_rate);
            const lfo_depth = @as(u8, @intCast(random.intRangeLessThan(u32, 0, 50)));
            _ = situation.foreign.SituationVirtualMidiControlChange(0, 26, lfo_depth);

            const note_idx = random.intRangeLessThan(usize, 0, PENTATONIC.len);
            last_note = PENTATONIC[note_idx];
            const velocity = @as(u8, @intCast(30 + random.intRangeLessThan(u32, 0, 30)));
            _ = situation.foreign.SituationVirtualMidiNoteOn(last_note, velocity);
        }

        // -----------------------------------------------------------------------
        //  Render
        // -----------------------------------------------------------------------
        if (situation.foreign.SituationAcquireFrameCommandBuffer() == .SITUATION_SUCCESS) {
            const cmd = situation.foreign.SituationGetMainCommandBuffer();
            if (@intFromPtr(cmd) != 0) {
                _ = situation.foreign.SituationCmdBeginRenderToDisplay(cmd, -1, situation.ColorRGBA{ .r = 0, .g = 0, .b = 0, .a = 255 });

                // Draw the full-screen raster-bars + torus shader
                _ = situation.foreign.SituationCmdBindPipeline(cmd, shader);
                const w = @as(f32, @floatFromInt(situation.foreign.SituationGetRenderWidth()));
                const h = @as(f32, @floatFromInt(situation.foreign.SituationGetRenderHeight()));
                var resolution = [2]f32{ w, h };
                _ = situation.foreign.SituationSetShaderUniform(shader, "uTime", &sim_time, .SIT_UNIFORM_FLOAT);
                _ = situation.foreign.SituationSetShaderUniform(shader, "uResolution", &resolution, .SIT_UNIFORM_VEC2);
                _ = situation.foreign.SituationCmdDraw(cmd, 3, 1, 0, 0);

                const sc = h / 600.0;

                // Title bar
                _ = situation.foreign.SituationCmdDrawTextEx(
                    cmd,
                    default_font,
                    "S I T U A T I O N",
                    situation.Vector2{ .x = w * 0.27, .y = 18 * sc },
                    24 * sc,
                    2.0 * sc,
                    situation.ColorRGBA{ .r = 255, .g = 255, .b = 255, .a = 220 },
                );

                // HUD line 1 — system status
                const vsync_on = (window_flags & VSYNC_FLAG) != 0;
                const fps = situation.foreign.SituationGetFPS();
                const vsync_str = if (vsync_on) "ON" else "OFF";
                const audio_str = if (audio_ok) "active" else "off";
                const line1 = std.fmt.bufPrintSentinel(&fps_buf, "{d} FPS  VSync:{s}  Audio:{s}", .{ fps, vsync_str, audio_str }, 0) catch "Error";

                _ = situation.foreign.SituationCmdDrawTextEx(
                    cmd,
                    default_font,
                    line1.ptr,
                    situation.Vector2{ .x = 10, .y = h - 36 * sc },
                    8 * sc,
                    0,
                    situation.ColorRGBA{ .r = 180, .g = 180, .b = 180, .a = 255 },
                );

                // HUD line 2 — FX levels
                const line2 = std.fmt.bufPrintSentinel(&fx_buf, "Reverb: {d}%   Delay: {d}%   Delay FB: {d}%", .{
                    @as(i32, @intFromFloat(reverb_wet * 100.0)),
                    @as(i32, @intFromFloat(delay_wet * 100.0)),
                    @as(i32, @intFromFloat(delay_feedback * 100.0)),
                }, 0) catch "Error";

                _ = situation.foreign.SituationCmdDrawTextEx(
                    cmd,
                    default_font,
                    line2.ptr,
                    situation.Vector2{ .x = 10, .y = h - 20 * sc },
                    8 * sc,
                    0,
                    situation.ColorRGBA{ .r = 140, .g = 210, .b = 255, .a = 255 },
                );

                _ = situation.foreign.SituationCmdEndRenderPass(cmd);
            }
            _ = situation.foreign.SituationEndFrame();
        }
    }

    // Release the last held note cleanly before shutdown
    if (last_note > 0) {
        _ = situation.foreign.SituationVirtualMidiNoteOff(last_note);
    }
    std.debug.print("Done.\n", .{});
}
