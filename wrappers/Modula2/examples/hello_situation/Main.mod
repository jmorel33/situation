(* Situation Modula-2 Interactive Demo — Torus Raymarching and Tone Synthesizer
 *
 * Build:
 *   build\build_modula2_example.bat opengl hello_situation
 *
 * Run:
 *   build\examples\modula2\hello_situation.exe
 *)

MODULE Main;

FROM SYSTEM IMPORT ADR, ADDRESS, CARDINAL8, REAL32, TSIZE;
FROM libc IMPORT strlen, write;
FROM SituationForeign IMPORT
  SituationAcquireFrameCommandBuffer,
  SituationCmdBeginRenderToDisplay,
  SituationCmdBindPipeline,
  SituationCmdDraw,
  SituationCmdDrawTextEx,
  SituationCmdEndRenderPass,
  SituationCmdSetPushConstant,
  SituationCreateGraph,
  SituationCreateNode,
  SituationCreatePatch,
  SituationDestroyGraph,
  SituationEnableMidiControl,
  SituationEndFrame,
  SituationFreeString,
  SituationGetCurrentActualWindowStateFlags,
  SituationGetFrameTime,
  SituationGetFPS,
  SituationGetGraphicsBackend,
  SituationGetGraphicsBackendName,
  SituationGetLastErrorMsg,
  SituationGetMainCommandBuffer,
  SituationGetRenderHeight,
  SituationGetRenderWidth,
  SituationInit,
  SituationInitDeviceRegistry,
  SituationIsKeyPressed,
  SituationLoadShaderFromMemory,
  SituationPollInputEvents,
  SituationSetActiveGraph,
  SituationSetControl,
  SituationSetShaderUniform,
  SituationSetVSync,
  SituationSetupVirtualMidiLoopback,
  SituationShutdown,
  SituationTeardownVirtualMidiLoopback,
  SituationToggleBorderlessWindowed,
  SituationUnloadShader,
  SituationUpdateTimers,
  SituationVirtualMidiControlChange,
  SituationVirtualMidiNoteOff,
  SituationVirtualMidiNoteOn,
  SituationWindowShouldClose;
FROM SituationConstants IMPORT
  SIT_KEY_EQUAL,
  SIT_KEY_ESCAPE,
  SIT_KEY_F,
  SIT_KEY_LEFT_BRACKET,
  SIT_KEY_MINUS,
  SIT_KEY_O,
  SIT_KEY_P,
  SIT_KEY_RIGHT_BRACKET,
  SIT_KEY_SPACE,
  SIT_KEY_V;
FROM SituationGlue IMPORT SituationM2InitInfoWindow;
FROM SituationHelpers IMPORT situationSuccess;
FROM SituationTypes IMPORT
  ColorRGBA,
  SIT_GRAPHICS_BACKEND_VULKAN,
  SITUATION_SUCCESS,
  SITUATION_WINDOW_STATE_VSYNC_HINT,
  SituationAudioGraph,
  SituationCommandBuffer,
  SituationError,
  SituationFont,
  SituationGraphicsBackend,
  SituationInitInfo,
  SituationNodeHandle,
  SituationShader,
  Vector2;

CONST
  (* Node / uniform constants not exported from SituationTypes.def *)
  SITUATION_NODE_REVERB     = 0;
  SITUATION_NODE_ECHO       = 1;
  SITUATION_NODE_TONE_SYNTH = 21;
  SIT_UNIFORM_FLOAT         = 0;
  SIT_UNIFORM_VEC2          = 1;

  WINDOW_TITLE =
    "Situation+Modula2  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]";
  TITLE_TEXT = "S I T U A T I O N";
  UNIFORM_TIME = "uTime";
  UNIFORM_RESOLUTION = "uResolution";

TYPE
  CStr128 = ARRAY [0..127] OF CHAR;
  CStr512 = ARRAY [0..511] OF CHAR;
  CStr8192 = ARRAY [0..8191] OF CHAR;
  GpuVec2 = ARRAY [0..1] OF REAL32;
  NoteScale = ARRAY [0..9] OF CARDINAL8;
  ShaderPc = RECORD
    time, pad, resX, resY: REAL32;
  END;

  SimpleRng = RECORD
    state: CARDINAL;
  END;

VAR
  vertSrcVk, vertSrcGl: CStr512;
  fragSrcVk, fragSrcGl: CStr8192;
  windowTitle, titleText, uniformTime, uniformRes: CStr128;
  pentatonic: NoteScale;
  rng: SimpleRng;
  config: SituationInitInfo;
  graph: SituationAudioGraph;
  synthHandle, echoHandle, reverbHandle: SituationNodeHandle;
  midiDeviceId: INTEGER;
  audioOk: BOOLEAN;
  delayWet, delayFeedback, delayTime, reverbWet: REAL;
  shader: SituationShader;
  defaultFont: SituationFont;
  simTime, noteTimer, dt: REAL;
  simTimeU: REAL32;
  lastNote: CARDINAL8;
  isVulkan: BOOLEAN;
  e1, e2, e3: SituationError;
  quitRequested: BOOLEAN;
  windowFlags: CARDINAL;
  cmd: SituationCommandBuffer;
  clear: ColorRGBA;
  w, h, sc: REAL;
  hud1, hud2: ARRAY [0..127] OF CHAR;
  vsyncOn: BOOLEAN;
  pc: ShaderPc;
  resolution: GpuVec2;
  titlePos, hud1Pos, hud2Pos: Vector2;
  titleColor, hud1Color, hud2Color: ColorRGBA;
  renderErrLogged: BOOLEAN;
  frameLogCount: CARDINAL;

PROCEDURE LogLine (msg: ARRAY OF CHAR);
VAR line: ARRAY [0..511] OF CHAR;
    n: CARDINAL;
    discard: INTEGER;
BEGIN
  n := 0;
  WHILE (n <= HIGH(msg)) AND (msg[n] # CHR(0)) AND (n < HIGH(line) - 1) DO
    line[n] := msg[n];
    INC(n)
  END;
  line[n] := CHR(10);
  INC(n);
  discard := write(1, ADR(line), n)
END LogLine;

PROCEDURE LogCStr (s: ADDRESS);
VAR n: INTEGER;
    nl: CHAR;
    discard: INTEGER;
BEGIN
  IF s # VAL(ADDRESS, 0) THEN
    n := strlen(s);
    IF n > 0 THEN
      discard := write(1, s, n)
    END;
    nl := CHR(10);
    discard := write(1, ADR(nl), 1)
  END
END LogCStr;

PROCEDURE LogLastError;
VAR msg: ADDRESS;
    discard: SituationError;
BEGIN
  msg := VAL(ADDRESS, 0);
  discard := SituationGetLastErrorMsg(ADR(msg));
  IF msg # VAL(ADDRESS, 0) THEN
    LogCStr(msg);
    SituationFreeString(msg)
  END
END LogLastError;

PROCEDURE CopyCStr (VAR dest: ARRAY OF CHAR; src: ARRAY OF CHAR);
VAR i: CARDINAL;
BEGIN
  i := 0;
  WHILE (i <= HIGH(src)) AND (src[i] # CHR(0)) AND (i <= HIGH(dest)) DO
    dest[i] := src[i];
    INC(i)
  END;
  IF i <= HIGH(dest) THEN dest[i] := CHR(0) END
END CopyCStr;

PROCEDURE IgnoreErr (err: SituationError);
BEGIN
END IgnoreErr;

PROCEDURE LogErrOnce (VAR logged: BOOLEAN; msg: ARRAY OF CHAR; err: SituationError);
BEGIN
  IF (NOT logged) AND (NOT situationSuccess(err)) THEN
    logged := TRUE;
    LogLine(msg);
    LogLastError()
  END
END LogErrOnce;

(* --- Simple LCG RNG (matches Rust demo seed 1337) --- *)

PROCEDURE RngInit (VAR r: SimpleRng; seed: CARDINAL);
BEGIN
  r.state := seed
END RngInit;

PROCEDURE RngNext (VAR r: SimpleRng): CARDINAL;
BEGIN
  r.state := r.state * 1103515245 + 12345;
  RETURN r.state
END RngNext;

PROCEDURE RngRange (VAR r: SimpleRng; min, max: CARDINAL): CARDINAL;
VAR range: CARDINAL;
BEGIN
  range := max - min;
  IF range = 0 THEN RETURN min END;
  RETURN min + (RngNext(r) MOD range)
END RngRange;

PROCEDURE InitPentatonic;
BEGIN
  pentatonic[0] := 48;
  pentatonic[1] := 52;
  pentatonic[2] := 55;
  pentatonic[3] := 60;
  pentatonic[4] := 64;
  pentatonic[5] := 67;
  pentatonic[6] := 72;
  pentatonic[7] := 76;
  pentatonic[8] := 79;
  pentatonic[9] := 84
END InitPentatonic;

(* --- Fixed-buffer string helpers for HUD lines --- *)

PROCEDURE AppendChar (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; ch: CHAR);
BEGIN
  IF pos < HIGH(buf) THEN
    buf[pos] := ch;
    INC(pos)
  END
END AppendChar;

PROCEDURE AppendStr (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; s: ARRAY OF CHAR);
VAR i: CARDINAL;
BEGIN
  i := 0;
  WHILE (i <= HIGH(s)) AND (s[i] # CHR(0)) DO
    AppendChar(buf, pos, s[i]);
    INC(i)
  END
END AppendStr;

PROCEDURE AppendInt (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; n: INTEGER);
VAR neg: BOOLEAN;
  u: CARDINAL;
  digits: ARRAY [0..15] OF CHAR;
  count, i: CARDINAL;
BEGIN
  IF n < 0 THEN
    neg := TRUE;
    u := CARDINAL(-n)
  ELSE
    neg := FALSE;
    u := CARDINAL(n)
  END;
  IF neg THEN AppendChar(buf, pos, '-') END;
  count := 0;
  REPEAT
    digits[count] := CHR(ORD('0') + CARDINAL(u MOD 10));
    u := u DIV 10;
    INC(count)
  UNTIL u = 0;
  IF count = 0 THEN
    AppendChar(buf, pos, '0')
  ELSE
    i := count;
    WHILE i > 0 DO
      DEC(i);
      AppendChar(buf, pos, digits[i])
    END
  END
END AppendInt;

PROCEDURE AppendCard (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; n: CARDINAL);
VAR u, count, i: CARDINAL;
    digits: ARRAY [0..19] OF CHAR;
BEGIN
  u := n;
  count := 0;
  REPEAT
    digits[count] := CHR(ORD('0') + u MOD 10);
    u := u DIV 10;
    INC(count)
  UNTIL u = 0;
  IF count = 0 THEN
    AppendChar(buf, pos, '0')
  ELSE
    i := count;
    WHILE i > 0 DO
      DEC(i);
      AppendChar(buf, pos, digits[i])
    END
  END
END AppendCard;

PROCEDURE NullTerminate (VAR buf: ARRAY OF CHAR; pos: CARDINAL);
BEGIN
  IF pos <= HIGH(buf) THEN buf[pos] := CHR(0) END
END NullTerminate;

PROCEDURE AppendLine (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; line: ARRAY OF CHAR);
BEGIN
  AppendStr(buf, pos, line);
  AppendChar(buf, pos, CHR(10))
END AppendLine;

(* Build multi-line GLSL sources (matches Fortran demo_helpers init_shader_sources). *)
PROCEDURE InitShaderSources;
VAR pos: CARDINAL;
BEGIN
  pos := 0;
  AppendLine(vertSrcVk, pos, "#version 460");
  AppendLine(vertSrcVk, pos, "void main() {");
  AppendLine(vertSrcVk, pos, "    int vid = gl_VertexIndex;");
  AppendLine(vertSrcVk, pos, "    vec2 pos = vec2(float(vid & 1) * 4.0 - 1.0, float(vid & 2) * 2.0 - 1.0);");
  AppendLine(vertSrcVk, pos, "    gl_Position = vec4(pos, 0.0, 1.0);");
  AppendLine(vertSrcVk, pos, "}");
  NullTerminate(vertSrcVk, pos);

  pos := 0;
  AppendLine(vertSrcGl, pos, "#version 460");
  AppendLine(vertSrcGl, pos, "void main() {");
  AppendLine(vertSrcGl, pos, "    vec2 pos = vec2(float(gl_VertexID & 1) * 4.0 - 1.0, float(gl_VertexID & 2) * 2.0 - 1.0);");
  AppendLine(vertSrcGl, pos, "    gl_Position = vec4(pos, 0.0, 1.0);");
  AppendLine(vertSrcGl, pos, "}");
  NullTerminate(vertSrcGl, pos);

  pos := 0;
  AppendLine(fragSrcVk, pos, "#version 460");
  AppendLine(fragSrcVk, pos, "layout(push_constant) uniform PC { float uTime; vec2 uResolution; } pc;");
  AppendLine(fragSrcVk, pos, "#define uTime       pc.uTime");
  AppendLine(fragSrcVk, pos, "#define uResolution pc.uResolution");
  AppendLine(fragSrcVk, pos, "layout(location = 0) out vec4 fragColor;");
  AppendLine(fragSrcVk, pos, "vec3 hsv2rgb(vec3 c) {");
  AppendLine(fragSrcVk, pos, "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);");
  AppendLine(fragSrcVk, pos, "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);");
  AppendLine(fragSrcVk, pos, "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);");
  AppendLine(fragSrcVk, pos, "}");
  AppendLine(fragSrcVk, pos, "float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }");
  AppendLine(fragSrcVk, pos, "mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }");
  AppendLine(fragSrcVk, pos, "mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }");
  AppendLine(fragSrcVk, pos, "void main() {");
  AppendLine(fragSrcVk, pos, "    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);");
  AppendLine(fragSrcVk, pos, "    float x_norm = gl_FragCoord.x / uResolution.x;");
  AppendLine(fragSrcVk, pos, "    vec3 bg = vec3(0.02, 0.02, 0.05);");
  AppendLine(fragSrcVk, pos, "    for (int i = 0; i < 6; i++) {");
  AppendLine(fragSrcVk, pos, "        float phase = uTime * 0.3 + float(i) * 1.05;");
  AppendLine(fragSrcVk, pos, "        float center = 0.5 + 0.35 * sin(phase);");
  AppendLine(fragSrcVk, pos, "        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);");
  AppendLine(fragSrcVk, pos, "        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;");
  AppendLine(fragSrcVk, pos, "    }");
  AppendLine(fragSrcVk, pos, "    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));");
  AppendLine(fragSrcVk, pos, "    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);");
  AppendLine(fragSrcVk, pos, "    float t = 0.0; float d = 0.0;");
  AppendLine(fragSrcVk, pos, "    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }");
  AppendLine(fragSrcVk, pos, "    vec3 col = bg;");
  AppendLine(fragSrcVk, pos, "    if (d < 0.001) {");
  AppendLine(fragSrcVk, pos, "        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);");
  AppendLine(fragSrcVk, pos, "        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));");
  AppendLine(fragSrcVk, pos, "        vec3 light = normalize(vec3(0.4, 0.8, -0.5));");
  AppendLine(fragSrcVk, pos, "        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);");
  AppendLine(fragSrcVk, pos, "        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);");
  AppendLine(fragSrcVk, pos, "        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;");
  AppendLine(fragSrcVk, pos, "    }");
  AppendLine(fragSrcVk, pos, "    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);");
  AppendLine(fragSrcVk, pos, "}");
  NullTerminate(fragSrcVk, pos);

  pos := 0;
  AppendLine(fragSrcGl, pos, "#version 460");
  AppendLine(fragSrcGl, pos, "layout(location = 0) uniform float uTime;");
  AppendLine(fragSrcGl, pos, "layout(location = 1) uniform vec2  uResolution;");
  AppendLine(fragSrcGl, pos, "layout(location = 0) out vec4 fragColor;");
  AppendLine(fragSrcGl, pos, "vec3 hsv2rgb(vec3 c) {");
  AppendLine(fragSrcGl, pos, "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);");
  AppendLine(fragSrcGl, pos, "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);");
  AppendLine(fragSrcGl, pos, "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);");
  AppendLine(fragSrcGl, pos, "}");
  AppendLine(fragSrcGl, pos, "float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }");
  AppendLine(fragSrcGl, pos, "mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }");
  AppendLine(fragSrcGl, pos, "mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }");
  AppendLine(fragSrcGl, pos, "void main() {");
  AppendLine(fragSrcGl, pos, "    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);");
  AppendLine(fragSrcGl, pos, "    float x_norm = gl_FragCoord.x / uResolution.x;");
  AppendLine(fragSrcGl, pos, "    vec3 bg = vec3(0.02, 0.02, 0.05);");
  AppendLine(fragSrcGl, pos, "    for (int i = 0; i < 6; i++) {");
  AppendLine(fragSrcGl, pos, "        float phase = uTime * 0.3 + float(i) * 1.05;");
  AppendLine(fragSrcGl, pos, "        float center = 0.5 + 0.35 * sin(phase);");
  AppendLine(fragSrcGl, pos, "        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);");
  AppendLine(fragSrcGl, pos, "        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;");
  AppendLine(fragSrcGl, pos, "    }");
  AppendLine(fragSrcGl, pos, "    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));");
  AppendLine(fragSrcGl, pos, "    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);");
  AppendLine(fragSrcGl, pos, "    float t = 0.0; float d = 0.0;");
  AppendLine(fragSrcGl, pos, "    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }");
  AppendLine(fragSrcGl, pos, "    vec3 col = bg;");
  AppendLine(fragSrcGl, pos, "    if (d < 0.001) {");
  AppendLine(fragSrcGl, pos, "        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);");
  AppendLine(fragSrcGl, pos, "        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));");
  AppendLine(fragSrcGl, pos, "        vec3 light = normalize(vec3(0.4, 0.8, -0.5));");
  AppendLine(fragSrcGl, pos, "        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);");
  AppendLine(fragSrcGl, pos, "        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);");
  AppendLine(fragSrcGl, pos, "        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;");
  AppendLine(fragSrcGl, pos, "    }");
  AppendLine(fragSrcGl, pos, "    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);");
  AppendLine(fragSrcGl, pos, "}");
  NullTerminate(fragSrcGl, pos)
END InitShaderSources;

PROCEDURE BuildHudLine1 (VAR buf: ARRAY OF CHAR; fps: INTEGER; vsyncOn, audioOk: BOOLEAN);
VAR pos: CARDINAL;
BEGIN
  pos := 0;
  AppendInt(buf, pos, fps);
  AppendStr(buf, pos, " FPS  VSync:");
  IF vsyncOn THEN AppendStr(buf, pos, "ON") ELSE AppendStr(buf, pos, "OFF") END;
  AppendStr(buf, pos, "  Audio:");
  IF audioOk THEN AppendStr(buf, pos, "active") ELSE AppendStr(buf, pos, "off") END;
  NullTerminate(buf, pos)
END BuildHudLine1;

PROCEDURE AppendPercent (VAR buf: ARRAY OF CHAR; VAR pos: CARDINAL; value: REAL);
VAR pct: CARDINAL;
BEGIN
  IF value < 0.0 THEN pct := 0
  ELSIF value > 1.0 THEN pct := 100
  ELSE pct := VAL(CARDINAL, TRUNC(value * 100.0 + 0.5))
  END;
  AppendCard(buf, pos, pct);
  AppendChar(buf, pos, '%')
END AppendPercent;

PROCEDURE BuildHudLine2 (VAR buf: ARRAY OF CHAR; reverbWet, delayWet, delayFeedback: REAL);
VAR pos: CARDINAL;
BEGIN
  pos := 0;
  AppendStr(buf, pos, "Reverb: ");
  AppendPercent(buf, pos, reverbWet);
  AppendStr(buf, pos, "   Delay: ");
  AppendPercent(buf, pos, delayWet);
  AppendStr(buf, pos, "   Delay FB: ");
  AppendPercent(buf, pos, delayFeedback);
  NullTerminate(buf, pos)
END BuildHudLine2;

PROCEDURE HasFlag (flags, bit: CARDINAL): BOOLEAN;
BEGIN
  IF bit = 0 THEN RETURN FALSE END;
  RETURN (flags DIV bit) MOD 2 = 1
END HasFlag;

PROCEDURE Cleanup (
  graph: SituationAudioGraph;
  audioOk: BOOLEAN;
  VAR shader: SituationShader;
  lastNote: CARDINAL8
);
BEGIN
  IF lastNote > 0 THEN
    IgnoreErr(SituationVirtualMidiNoteOff(lastNote))
  END;
  SituationUnloadShader(ADR(shader));
  IF graph # NIL THEN
    IgnoreErr(SituationSetActiveGraph(NIL));
    IF audioOk THEN
      SituationTeardownVirtualMidiLoopback()
    END;
    SituationDestroyGraph(graph)
  END;
  SituationShutdown();
  LogLine("Situation cleanup complete.")
END Cleanup;

BEGIN
  LogLine("=== Situation+Modula2 — Raster Bars + Ambient Synth ===");
  LogLine("  V      Toggle VSync");
  LogLine("  F      Toggle borderless windowed");
  LogLine("  Space  Trigger note immediately");
  LogLine("  + / -  Reverb wet up / down");
  LogLine("  ] / [  Delay wet up / down");
  LogLine("  P / O  Delay feedback up / down");
  LogLine("  ESC    Quit");

  RngInit(rng, 1337);
  InitPentatonic();

  CopyCStr(windowTitle, WINDOW_TITLE);
  SituationM2InitInfoWindow(ADR(config), 900, 600, ADR(windowTitle));
  IF NOT situationSuccess(SituationInit(0, 0, ADR(config))) THEN
    LogLine("SituationInit failed.");
    HALT
  END;

  SituationSetVSync(TRUE);
  SituationInitDeviceRegistry();

  graph := SituationCreateGraph();
  synthHandle := 0;
  echoHandle := 0;
  reverbHandle := 0;
  midiDeviceId := -1;
  audioOk := FALSE;
  delayWet := 0.25;
  delayFeedback := 0.40;
  delayTime := 0.35;
  reverbWet := 0.20;
  simTime := 0.0;
  noteTimer := 0.0;
  lastNote := 0;
  shader.slot_index := 0;
  shader.generation := 0;

  IF graph # NIL THEN
    e1 := SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, ADR(synthHandle));
    e2 := SituationCreateNode(graph, SITUATION_NODE_ECHO, ADR(echoHandle));
    e3 := SituationCreateNode(graph, SITUATION_NODE_REVERB, ADR(reverbHandle));

    IF situationSuccess(e1) AND situationSuccess(e2) AND situationSuccess(e3) THEN
      IgnoreErr(SituationCreatePatch(graph, synthHandle, 0, echoHandle, 0, FALSE));
      IgnoreErr(SituationCreatePatch(graph, echoHandle, 0, reverbHandle, 0, FALSE));

      IgnoreErr(SituationSetControl(graph, echoHandle, 0, delayTime));
      IgnoreErr(SituationSetControl(graph, echoHandle, 1, delayFeedback));
      IgnoreErr(SituationSetControl(graph, echoHandle, 2, delayWet));

      IgnoreErr(SituationSetControl(graph, reverbHandle, 0, 0.65));
      IgnoreErr(SituationSetControl(graph, reverbHandle, 1, 0.55));
      IgnoreErr(SituationSetControl(graph, reverbHandle, 2, reverbWet));
      IgnoreErr(SituationSetControl(graph, reverbHandle, 3, 0.30));
      IgnoreErr(SituationSetControl(graph, reverbHandle, 4, 0.85));

      IF situationSuccess(SituationSetupVirtualMidiLoopback(ADR(midiDeviceId))) THEN
        IgnoreErr(SituationEnableMidiControl(graph, synthHandle, midiDeviceId));
      ELSE
        LogLine("WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled")
      END;

      IgnoreErr(SituationSetActiveGraph(graph));
      audioOk := TRUE;

      IgnoreErr(SituationVirtualMidiControlChange(0, 70, 0));
      IgnoreErr(SituationVirtualMidiControlChange(0, 73, 90));
      IgnoreErr(SituationVirtualMidiControlChange(0, 75, 40));
      IgnoreErr(SituationVirtualMidiControlChange(0, 76, 80));
      IgnoreErr(SituationVirtualMidiControlChange(0, 72, 110));

      LogLine("Audio graph active: ToneSynth -> Echo -> Reverb")
    ELSE
      LogLine("Node creation failed.")
    END
  ELSE
    LogLine("WARNING: Could not create audio graph — audio disabled")
  END;

  isVulkan := SituationGetGraphicsBackend() = SituationGraphicsBackend(SIT_GRAPHICS_BACKEND_VULKAN);
  LogCStr(SituationGetGraphicsBackendName());

  InitShaderSources();
  CopyCStr(titleText, TITLE_TEXT);
  CopyCStr(uniformTime, UNIFORM_TIME);
  CopyCStr(uniformRes, UNIFORM_RESOLUTION);

  defaultFont.texture.slot_index := 0;
  defaultFont.texture.generation := 0;
  defaultFont.texture.width := 0;
  defaultFont.texture.height := 0;
  defaultFont.font_size := 0.0;
  defaultFont.ascent := 0.0;
  defaultFont.descent := 0.0;
  defaultFont.line_gap := 0.0;
  defaultFont.glyph_count := 0;
  defaultFont.is_bitmap := FALSE;

  IF isVulkan THEN
    IF NOT situationSuccess(SituationLoadShaderFromMemory(ADR(vertSrcVk), ADR(fragSrcVk), ADR(shader))) THEN
      LogLine("Shader load failed (Vulkan).");
      LogLastError();
      Cleanup(graph, audioOk, shader, lastNote);
      HALT
    END
  ELSE
    IF NOT situationSuccess(SituationLoadShaderFromMemory(ADR(vertSrcGl), ADR(fragSrcGl), ADR(shader))) THEN
      LogLine("Shader load failed (OpenGL).");
      LogLastError();
      Cleanup(graph, audioOk, shader, lastNote);
      HALT
    END
  END;
  LogLine("Shader loaded — entering main loop.");

  quitRequested := FALSE;
  renderErrLogged := FALSE;
  frameLogCount := 0;
  WHILE (NOT SituationWindowShouldClose()) AND (NOT quitRequested) DO
    SituationPollInputEvents();
    SituationUpdateTimers();
    INC(frameLogCount);
    IF (frameLogCount MOD 300) = 0 THEN
      LogLine("loop alive (frames ticking)")
    END;

    IF SituationIsKeyPressed(SIT_KEY_ESCAPE) THEN
      quitRequested := TRUE
    END;

    windowFlags := SituationGetCurrentActualWindowStateFlags();

    IF SituationIsKeyPressed(SIT_KEY_V) THEN
      (* V — toggle VSync *)
      IF HasFlag(windowFlags, SITUATION_WINDOW_STATE_VSYNC_HINT) THEN
        SituationSetVSync(FALSE)
      ELSE
        SituationSetVSync(TRUE)
      END;
      windowFlags := SituationGetCurrentActualWindowStateFlags()
    END;

    IF SituationIsKeyPressed(SIT_KEY_F) THEN
      SituationToggleBorderlessWindowed()
    END;

    IF SituationIsKeyPressed(SIT_KEY_SPACE) AND audioOk THEN
      IF lastNote > 0 THEN
        IgnoreErr(SituationVirtualMidiNoteOff(lastNote));
      END;
      lastNote := pentatonic[RngRange(rng, 0, 10)];
      IgnoreErr(SituationVirtualMidiNoteOn(lastNote, VAL(CARDINAL8, 50 + RngRange(rng, 0, 30))));
      noteTimer := 0.0
    END;

    IF graph # NIL THEN
      IF SituationIsKeyPressed(SIT_KEY_EQUAL) AND audioOk THEN
        reverbWet := reverbWet + 0.05;
        IF reverbWet > 1.0 THEN reverbWet := 1.0 END;
        IgnoreErr(SituationSetControl(graph, reverbHandle, 2, reverbWet));
      END;
      IF SituationIsKeyPressed(SIT_KEY_MINUS) AND audioOk THEN
        reverbWet := reverbWet - 0.05;
        IF reverbWet < 0.0 THEN reverbWet := 0.0 END;
        IgnoreErr(SituationSetControl(graph, reverbHandle, 2, reverbWet));
      END;
      IF SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET) AND audioOk THEN
        delayWet := delayWet + 0.05;
        IF delayWet > 1.0 THEN delayWet := 1.0 END;
        IgnoreErr(SituationSetControl(graph, echoHandle, 2, delayWet));
      END;
      IF SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET) AND audioOk THEN
        delayWet := delayWet - 0.05;
        IF delayWet < 0.0 THEN delayWet := 0.0 END;
        IgnoreErr(SituationSetControl(graph, echoHandle, 2, delayWet));
      END;
      IF SituationIsKeyPressed(SIT_KEY_P) AND audioOk THEN
        delayFeedback := delayFeedback + 0.05;
        IF delayFeedback > 0.95 THEN delayFeedback := 0.95 END;
        IgnoreErr(SituationSetControl(graph, echoHandle, 1, delayFeedback));
      END;
      IF SituationIsKeyPressed(SIT_KEY_O) AND audioOk THEN
        delayFeedback := delayFeedback - 0.05;
        IF delayFeedback < 0.0 THEN delayFeedback := 0.0 END;
        IgnoreErr(SituationSetControl(graph, echoHandle, 1, delayFeedback));
      END
    END;

    dt := SituationGetFrameTime();
    simTime := simTime + dt;
    simTimeU := VAL(REAL32, simTime);
    noteTimer := noteTimer + dt;

    IF audioOk AND (noteTimer > 4.0) THEN
      noteTimer := 0.0;
      IF lastNote > 0 THEN
        IgnoreErr(SituationVirtualMidiNoteOff(lastNote));
      END;

      IgnoreErr(SituationVirtualMidiControlChange(0, 70, VAL(CARDINAL8, RngRange(rng, 0, 4) * 32)));
      IgnoreErr(SituationVirtualMidiControlChange(0, 74, VAL(CARDINAL8, 40 + RngRange(rng, 0, 80))));
      IgnoreErr(SituationVirtualMidiControlChange(0, 71, VAL(CARDINAL8, 20 + RngRange(rng, 0, 60))));
      IgnoreErr(SituationVirtualMidiControlChange(0, 24, VAL(CARDINAL8, RngRange(rng, 0, 40))));
      IgnoreErr(SituationVirtualMidiControlChange(0, 26, VAL(CARDINAL8, RngRange(rng, 0, 50))));

      lastNote := pentatonic[RngRange(rng, 0, 10)];
      IgnoreErr(SituationVirtualMidiNoteOn(lastNote, VAL(CARDINAL8, 30 + RngRange(rng, 0, 30))));
    END;

    IF situationSuccess(SituationAcquireFrameCommandBuffer()) THEN
      cmd := SituationGetMainCommandBuffer();
      IF cmd # NIL THEN
        clear.r := 0; clear.g := 0; clear.b := 0; clear.a := 255;
        LogErrOnce(renderErrLogged, "BeginRenderToDisplay failed.", SituationCmdBeginRenderToDisplay(cmd, -1, clear));
        LogErrOnce(renderErrLogged, "BindPipeline failed.", SituationCmdBindPipeline(cmd, shader));

        w := VAL(REAL, SituationGetRenderWidth());
        h := VAL(REAL, SituationGetRenderHeight());
        sc := h / 600.0;
        resolution[0] := VAL(REAL32, w);
        resolution[1] := VAL(REAL32, h);

        IF isVulkan THEN
          pc.time := simTimeU;
          pc.pad := 0.0;
          pc.resX := resolution[0];
          pc.resY := resolution[1];
          LogErrOnce(renderErrLogged, "SetPushConstant failed.", SituationCmdSetPushConstant(cmd, 0, ADR(pc), TSIZE(pc)))
        ELSE
          LogErrOnce(renderErrLogged, "SetShaderUniform uTime failed.",
            SituationSetShaderUniform(shader, ADR(uniformTime), ADR(simTimeU), SIT_UNIFORM_FLOAT));
          LogErrOnce(renderErrLogged, "SetShaderUniform uResolution failed.",
            SituationSetShaderUniform(shader, ADR(uniformRes), ADR(resolution), SIT_UNIFORM_VEC2));
        END;

        LogErrOnce(renderErrLogged, "CmdDraw failed.", SituationCmdDraw(cmd, 3, 1, 0, 0));

        titlePos.x := w * 0.27;
        titlePos.y := 18.0 * sc;
        titleColor.r := 255; titleColor.g := 255; titleColor.b := 255; titleColor.a := 220;
        IgnoreErr(SituationCmdDrawTextEx(
          cmd, defaultFont, ADR(titleText),
          titlePos,
          24.0 * sc, 2.0 * sc,
          titleColor
        ));

        vsyncOn := HasFlag(windowFlags, SITUATION_WINDOW_STATE_VSYNC_HINT);
        BuildHudLine1(hud1, SituationGetFPS(), vsyncOn, audioOk);
        BuildHudLine2(hud2, reverbWet, delayWet, delayFeedback);

        hud1Pos.x := 10.0;
        hud1Pos.y := h - 36.0 * sc;
        hud1Color.r := 180; hud1Color.g := 180; hud1Color.b := 180; hud1Color.a := 255;
        IgnoreErr(SituationCmdDrawTextEx(
          cmd, defaultFont, ADR(hud1),
          hud1Pos,
          8.0 * sc, 0.0,
          hud1Color
        ));
        hud2Pos.x := 10.0;
        hud2Pos.y := h - 20.0 * sc;
        hud2Color.r := 140; hud2Color.g := 210; hud2Color.b := 255; hud2Color.a := 255;
        IgnoreErr(SituationCmdDrawTextEx(
          cmd, defaultFont, ADR(hud2),
          hud2Pos,
          8.0 * sc, 0.0,
          hud2Color
        ));

        IgnoreErr(SituationCmdEndRenderPass(cmd));
      END;
      IgnoreErr(SituationEndFrame());
    END
  END;

  Cleanup(graph, audioOk, shader, lastNote)
END Main.
