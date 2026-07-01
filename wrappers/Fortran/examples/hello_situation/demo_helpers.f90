! hello_situation demo helpers — compiled before main.f90 (see build_fortran_example.bat)

module hello_situation_demo
  use, intrinsic :: iso_c_binding
  use situation
  implicit none

  logical(c_bool), parameter :: C_FALSE = transfer(0_c_int8_t, .false._c_bool)
  logical(c_bool), parameter :: C_TRUE = transfer(1_c_int8_t, .false._c_bool)

  public :: C_FALSE, C_TRUE
  public :: asc2c_32, asc2c_128, cstr_ptr, make_rgba, zero_init_info, rng_range, init_shader_sources
  public :: reload_user_shader, cleanup_resources

  ! Saved GLSL ASCII (set by init_shader_sources) for cache-busting reloads.
  character(len=512), save :: vert_vk_base_a = ''
  character(len=512), save :: vert_gl_base_a = ''
  character(len=8192), save :: frag_vk_base_a = ''
  character(len=8192), save :: frag_gl_base_a = ''

contains

  subroutine copy_ascii_to_cbuf(ascii, cstr, buflen)
    character(len=*), intent(in) :: ascii
    character(kind=c_char), intent(out) :: cstr(buflen)
    integer, intent(in) :: buflen
    integer :: i, n, ch

    n = min(len_trim(ascii), buflen - 1)
    do i = 1, n
      ch = iachar(ascii(i:i))
      if (ch < 0 .or. ch > 255) ch = iachar('?')
      cstr(i) = char(ch, kind=c_char)
    end do
    cstr(n + 1) = c_null_char
  end subroutine copy_ascii_to_cbuf

  function cstr_ptr(cstr) result(p)
    character(kind=c_char, len=*), intent(in), target :: cstr
    type(c_ptr) :: p

    p = c_loc(cstr(1:1))
  end function cstr_ptr

  subroutine asc2c_32(ascii, cstr)
    character(len=*), intent(in) :: ascii
    character(kind=c_char, len=32), intent(inout) :: cstr

    call copy_ascii_to_cbuf(ascii, cstr, 32)
  end subroutine asc2c_32

  subroutine asc2c_128(ascii, cstr)
    character(len=*), intent(in) :: ascii
    character(kind=c_char, len=128), intent(inout) :: cstr

    call copy_ascii_to_cbuf(ascii, cstr, 128)
  end subroutine asc2c_128

  subroutine asc2c_512(ascii, cstr)
    character(len=*), intent(in) :: ascii
    character(kind=c_char, len=512), intent(inout) :: cstr

    call copy_ascii_to_cbuf(ascii, cstr, 512)
  end subroutine asc2c_512

  subroutine asc2c_8192(ascii, cstr)
    character(len=*), intent(in) :: ascii
    character(kind=c_char, len=8192), intent(inout) :: cstr

    call copy_ascii_to_cbuf(ascii, cstr, 8192)
  end subroutine asc2c_8192

  function make_rgba(r, g, b, a) result(c)
    integer, intent(in) :: r, g, b, a
    type(ColorRGBA) :: c

    c = ColorRGBA(int(r, c_int8_t), int(g, c_int8_t), int(b, c_int8_t), int(a, c_int8_t))
  end function make_rgba

  subroutine zero_init_info(cfg)
    type(SituationInitInfo), intent(out) :: cfg

    cfg%window_width = 0
    cfg%window_height = 0
    cfg%window_title = c_null_ptr
    cfg%initial_active_window_flags = 0
    cfg%initial_inactive_window_flags = 0
    cfg%enable_vulkan_validation = C_FALSE
    cfg%force_single_queue = C_FALSE
    cfg%max_frames_in_flight = 0
    cfg%required_vulkan_extensions = c_null_ptr
    cfg%required_vulkan_extension_count = 0
    cfg%flags = 0
    cfg%max_audio_voices = 0
    cfg%io_queue_capacity = 0
    cfg%disable_io_thread = C_FALSE
    cfg%hot_reload_poll_rate = 0.0
    cfg%staging_buffer_size = 0
    cfg%thread_affinity_main = 0
    cfg%thread_affinity_render = 0
    cfg%thread_affinity_audio = 0
    cfg%numa_prefer_local = C_FALSE
    cfg%worker_numa_spread = C_FALSE
    cfg%io_thread_numa_node = 0
    cfg%thread_pool_use_physical_cores = C_FALSE
    cfg%thread_pool_reserved_threads = 0
  end subroutine zero_init_info

  function rng_next(state) result(val)
    integer(c_int32_t), intent(inout) :: state
    integer(c_int32_t) :: val

    state = state * 1103515245 + 12345
    val = state
  end function rng_next

  function rng_range(state, lo, hi) result(idx)
    integer(c_int32_t), intent(inout) :: state
    integer, intent(in) :: lo, hi
    integer :: idx, span

    span = hi - lo
    if (span <= 0) then
      idx = lo
    else
      idx = lo + modulo(int(rng_next(state), c_int), span)
    end if
  end function rng_range

  subroutine init_shader_sources(vulkan, vert_vk, vert_gl, frag_vk, frag_gl)
    logical, intent(in) :: vulkan
    character(kind=c_char, len=512), intent(out) :: vert_vk, vert_gl
    character(kind=c_char, len=8192), intent(out) :: frag_vk, frag_gl
    character(len=512) :: vert_vk_a, vert_gl_a
    character(len=8192) :: frag_vk_a, frag_gl_a
    character(len=1), parameter :: nl = char(10)

    if (vulkan) then
      ! both GLSL variants are built; backend selects at load time
    end if

    vert_vk_a = '#version 460' // nl // &
      'void main() {' // nl // &
      '    int vid = gl_VertexIndex;' // nl // &
      '    vec2 pos = vec2(float(vid & 1) * 4.0 - 1.0, float(vid & 2) * 2.0 - 1.0);' // nl // &
      '    gl_Position = vec4(pos, 0.0, 1.0);' // nl // &
      '}'

    vert_gl_a = '#version 460' // nl // &
      'void main() {' // nl // &
      '    vec2 pos = vec2(float(gl_VertexID & 1) * 4.0 - 1.0, float(gl_VertexID & 2) * 2.0 - 1.0);' // nl // &
      '    gl_Position = vec4(pos, 0.0, 1.0);' // nl // &
      '}'

    frag_vk_a = '#version 460' // nl // &
      'layout(push_constant) uniform PC { float uTime; vec2 uResolution; } pc;' // nl // &
      '#define uTime       pc.uTime' // nl // &
      '#define uResolution pc.uResolution' // nl // &
      'layout(location = 0) out vec4 fragColor;' // nl // &
      'vec3 hsv2rgb(vec3 c) {' // nl // &
      '    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);' // nl // &
      '    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);' // nl // &
      '    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);' // nl // &
      '}' // nl // &
      'float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }' // nl // &
      'mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }' // nl // &
      'mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }' // nl // &
      'void main() {' // nl // &
      '    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);' // nl // &
      '    float x_norm = gl_FragCoord.x / uResolution.x;' // nl // &
      '    vec3 bg = vec3(0.02, 0.02, 0.05);' // nl // &
      '    for (int i = 0; i < 6; i++) {' // nl // &
      '        float phase = uTime * 0.3 + float(i) * 1.05;' // nl // &
      '        float center = 0.5 + 0.35 * sin(phase);' // nl // &
      '        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);' // nl // &
      '        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;' // nl // &
      '    }' // nl // &
      '    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));' // nl // &
      '    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);' // nl // &
      '    float t = 0.0; float d = 0.0;' // nl // &
      '    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }' // nl // &
      '    vec3 col = bg;' // nl // &
      '    if (d < 0.001) {' // nl // &
      '        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);' // nl // &
      '        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));' // nl // &
      '        vec3 light = normalize(vec3(0.4, 0.8, -0.5));' // nl // &
      '        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);' // nl // &
      '        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);' // nl // &
      '        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;' // nl // &
      '    }' // nl // &
      '    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);' // nl // &
      '}'

    frag_gl_a = '#version 460' // nl // &
      'layout(location = 0) uniform float uTime;' // nl // &
      'layout(location = 1) uniform vec2  uResolution;' // nl // &
      'layout(location = 0) out vec4 fragColor;' // nl // &
      'vec3 hsv2rgb(vec3 c) {' // nl // &
      '    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);' // nl // &
      '    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);' // nl // &
      '    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);' // nl // &
      '}' // nl // &
      'float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }' // nl // &
      'mat3 rotY(float a) { float c=cos(a),s=sin(a); return mat3(c,0,s, 0,1,0, -s,0,c); }' // nl // &
      'mat3 rotX(float a) { float c=cos(a),s=sin(a); return mat3(1,0,0, 0,c,-s, 0,s,c); }' // nl // &
      'void main() {' // nl // &
      '    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / min(uResolution.x, uResolution.y);' // nl // &
      '    float x_norm = gl_FragCoord.x / uResolution.x;' // nl // &
      '    vec3 bg = vec3(0.02, 0.02, 0.05);' // nl // &
      '    for (int i = 0; i < 6; i++) {' // nl // &
      '        float phase = uTime * 0.3 + float(i) * 1.05;' // nl // &
      '        float center = 0.5 + 0.35 * sin(phase);' // nl // &
      '        float glow = exp(-pow(x_norm - center, 2.0) * 200.0);' // nl // &
      '        bg += hsv2rgb(vec3(float(i) / 6.0 + uTime * 0.02, 0.9, 1.0)) * glow * 0.6;' // nl // &
      '    }' // nl // &
      '    vec3 ro = vec3(0.0, 0.0, -3.5); vec3 rd = normalize(vec3(uv, 1.2));' // nl // &
      '    mat3 rot = rotX(sin(uTime * 0.2) * 0.4) * rotY(uTime * 0.35);' // nl // &
      '    float t = 0.0; float d = 0.0;' // nl // &
      '    for (int i = 0; i < 64; i++) { vec3 p = rot * (ro + rd * t); d = sdTorus(p, vec2(1.0, 0.38)); if (d < 0.001 || t > 10.0) break; t += d; }' // nl // &
      '    vec3 col = bg;' // nl // &
      '    if (d < 0.001) {' // nl // &
      '        vec3 p = rot * (ro + rd * t); vec2 e = vec2(0.001, 0.0);' // nl // &
      '        vec3 n = normalize(vec3(sdTorus(p+e.xyy,vec2(1.0,0.38))-sdTorus(p-e.xyy,vec2(1.0,0.38)), sdTorus(p+e.yxy,vec2(1.0,0.38))-sdTorus(p-e.yxy,vec2(1.0,0.38)), sdTorus(p+e.yyx,vec2(1.0,0.38))-sdTorus(p-e.yyx,vec2(1.0,0.38))));' // nl // &
      '        vec3 light = normalize(vec3(0.4, 0.8, -0.5));' // nl // &
      '        float diff = max(dot(n, light), 0.0); float spec = pow(max(dot(reflect(-light,n),normalize(-rd)),0.0),32.0); float rim = pow(1.0-max(dot(n,normalize(-rd)),0.0),3.0);' // nl // &
      '        float hue = fract(atan(p.z,p.x)*0.5 + uTime*0.08);' // nl // &
      '        col = hsv2rgb(vec3(hue,0.7,0.9))*(0.15+diff*0.7) + vec3(1.0)*spec*0.5 + hsv2rgb(vec3(hue+0.3,0.8,1.0))*rim*0.6;' // nl // &
      '    }' // nl // &
      '    fragColor = vec4(col * (0.93 + 0.07 * sin(gl_FragCoord.x * 3.14159)), 1.0);' // nl // &
      '}'

    vert_vk_base_a = vert_vk_a
    vert_gl_base_a = vert_gl_a
    frag_vk_base_a = frag_vk_a
    frag_gl_base_a = frag_gl_a

    call asc2c_512(vert_vk_a, vert_vk)
    call asc2c_512(vert_gl_a, vert_gl)
    call asc2c_8192(frag_vk_a, frag_vk)
    call asc2c_8192(frag_gl_a, frag_gl)
  end subroutine init_shader_sources

  ! Reload shader after Vulkan focus loss. reload_tag tweaks the vert source so the
  ! shader cache cannot re-attach a STALE bundle (demo-only workaround).
  integer(c_int) function reload_user_shader(shader_ref, vulkan, reload_tag) result(err)
    type(SituationShader), intent(inout), target :: shader_ref
    logical, intent(in) :: vulkan
    integer, intent(in) :: reload_tag
    character(len=520) :: vert_tagged_a
    character(kind=c_char, len=512) :: vert_tagged_c
    character(kind=c_char, len=8192) :: frag_c
    integer :: poll_i

    if (vulkan) then
      if (reload_tag > 0) then
        write(vert_tagged_a, '(A,"//r",I0)') trim(vert_vk_base_a), reload_tag
      else
        vert_tagged_a = vert_vk_base_a
      end if
      call asc2c_512(vert_tagged_a, vert_tagged_c)
      call asc2c_8192(frag_vk_base_a, frag_c)
    else
      if (reload_tag > 0) then
        write(vert_tagged_a, '(A,"//r",I0)') trim(vert_gl_base_a), reload_tag
      else
        vert_tagged_a = vert_gl_base_a
      end if
      call asc2c_512(vert_tagged_a, vert_tagged_c)
      call asc2c_8192(frag_gl_base_a, frag_c)
    end if

    call SituationUnloadShader(c_loc(shader_ref))
    err = SituationLoadShaderFromMemory(cstr_ptr(vert_tagged_c), cstr_ptr(frag_c), c_loc(shader_ref))
    if (.not. sit_ok(err)) return

    do poll_i = 1, 500
      err = SituationPollShaderLoad(shader_ref)
      if (err == SITUATION_SUCCESS) exit
      if (err /= SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) exit
    end do
  end function reload_user_shader

  subroutine cleanup_resources(graph_ptr, had_audio, shader_ref, mesh_ref, active_note)
    type(c_ptr), intent(in) :: graph_ptr
    logical, intent(in) :: had_audio
    type(SituationShader), intent(in), target :: shader_ref
    type(SituationMesh), intent(in), target :: mesh_ref
    integer(c_int8_t), intent(in) :: active_note
    integer(c_int) :: ierr

    if (active_note > 0) ierr = SituationVirtualMidiNoteOff(active_note)
    if (mesh_ref%slot_index /= 0 .or. mesh_ref%generation /= 0) &
      call SituationDestroyMesh(c_loc(mesh_ref))
    call SituationUnloadShader(c_loc(shader_ref))
    if (c_associated(graph_ptr)) then
      ierr = SituationSetActiveGraph(c_null_ptr)
      if (had_audio) call SituationTeardownVirtualMidiLoopback()
      call SituationDestroyGraph(graph_ptr)
    end if
    call SituationShutdown()
    write(*, '(A)') 'Situation cleanup complete.'
  end subroutine cleanup_resources

end module hello_situation_demo