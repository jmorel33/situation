! Situation Fortran Interactive Demo — Raster Bars + Ambient Synth
!
! Build:
!   build\build_fortran_example.bat opengl hello_situation
!
! Run:
!   build\examples\fortran\hello_situation.exe

program hello_situation
  use, intrinsic :: iso_c_binding
  use situation
  use hello_situation_demo
  implicit none

  integer(c_int), parameter :: PENTATONIC_LEN = 10
  integer(c_int8_t), parameter :: PENTATONIC(10) = [ &
    int(48, c_int8_t), int(52, c_int8_t), int(55, c_int8_t), int(60, c_int8_t), &
    int(64, c_int8_t), int(67, c_int8_t), int(72, c_int8_t), int(76, c_int8_t), &
    int(79, c_int8_t), int(84, c_int8_t) ]

  type(SituationInitInfo), target :: config
  type(SituationShader), target :: shader
  type(SituationMesh), target :: fs_mesh
  type(SituationFont) :: default_font
  type(c_ptr) :: graph, cmd
  type(ColorRGBA) :: clear_color, white_color, gray_color, hud_color
  type(Vector2) :: title_pos, hud_pos1, hud_pos2

  character(kind=c_char, len=128) :: window_title
  character(kind=c_char, len=32) :: title_text
  character(kind=c_char, len=128) :: hud_line1, hud_line2
  character(len=128) :: hud_ascii1, hud_ascii2
  character(kind=c_char, len=32) :: uniform_time, uniform_res

  character(kind=c_char, len=512) :: vert_src_vk, vert_src_gl
  character(kind=c_char, len=8192) :: frag_src_vk, frag_src_gl

  real(c_float), target :: tri_verts(9)
  integer(c_int32_t), target :: tri_indices(3)

  integer(c_int) :: err, backend, window_flags, fps
  integer(c_int32_t), target :: synth_handle, echo_handle, reverb_handle
  integer(c_int), target :: midi_device_id
  integer(c_int8_t) :: last_note
  integer(c_int32_t) :: rng_state
  integer :: note_idx, pct_rev, pct_dly, pct_fb
  integer :: shader_refresh_left, shader_reload_tag, shader_settle_frames

  logical :: audio_ok, vsync_on, was_focused, has_focus
  real(c_float) :: delay_wet, delay_feedback, delay_time, reverb_wet
  real(c_float) :: sim_time, note_timer, dt, w, h, sc
  real(c_float), target :: pc_data(4)

  write(*, '(A)') '=== Situation Fortran — Raster Bars + Ambient Synth ==='
  write(*, '(A)') '  V      Toggle VSync'
  write(*, '(A)') '  F      Toggle borderless windowed'
  write(*, '(A)') '  Space  Trigger note immediately'
  write(*, '(A)') '  + / -  Reverb wet up / down'
  write(*, '(A)') '  ] / [  Delay wet up / down'
  write(*, '(A)') '  P / O  Delay feedback up / down'
  write(*, '(A)') '  ESC    Quit'

  rng_state = 1337
  delay_wet = 0.25
  delay_feedback = 0.40
  delay_time = 0.35
  reverb_wet = 0.20
  sim_time = 0.0
  note_timer = 0.0
  last_note = 0
  audio_ok = .false.
  midi_device_id = -1
  synth_handle = 0
  echo_handle = 0
  reverb_handle = 0
  graph = c_null_ptr

  call asc2c_128('Situation+Fortran  [V]Sync [F]ull [Spc]Note  [+/-]Rev  []/[]Dly  [P/O]DlyFB  [Esc]', window_title)
  call zero_init_info(config)
  config%window_width = 900
  config%window_height = 600
  config%window_title = cstr_ptr(window_title)

  err = SituationInit(0, c_null_ptr, c_loc(config))
  if (.not. sit_ok(err)) then
    write(*, '(A)') '[ERROR] SituationInit failed'
    stop 1
  end if

  call SituationSetVSync(C_TRUE)
  call SituationInitDeviceRegistry()

  graph = SituationCreateGraph()
  if (c_associated(graph)) then
    err = SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, c_loc(synth_handle))
    if (.not. sit_ok(err)) synth_handle = 0
    err = SituationCreateNode(graph, SITUATION_NODE_ECHO, c_loc(echo_handle))
    if (.not. sit_ok(err)) echo_handle = 0
    err = SituationCreateNode(graph, SITUATION_NODE_REVERB, c_loc(reverb_handle))
    if (.not. sit_ok(err)) reverb_handle = 0

    if (synth_handle /= 0 .and. echo_handle /= 0 .and. reverb_handle /= 0) then
      err = SituationCreatePatch(graph, synth_handle, 0, echo_handle, 0, C_FALSE)
      if (sit_ok(err)) then
        err = SituationCreatePatch(graph, echo_handle, 0, reverb_handle, 0, C_FALSE)
      end if

      if (sit_ok(err)) then
        err = SituationSetControl(graph, echo_handle, 0, delay_time)
        if (sit_ok(err)) err = SituationSetControl(graph, echo_handle, 1, delay_feedback)
        if (sit_ok(err)) err = SituationSetControl(graph, echo_handle, 2, delay_wet)
        if (sit_ok(err)) err = SituationSetControl(graph, reverb_handle, 0, 0.65)
        if (sit_ok(err)) err = SituationSetControl(graph, reverb_handle, 1, 0.55)
        if (sit_ok(err)) err = SituationSetControl(graph, reverb_handle, 2, reverb_wet)
        if (sit_ok(err)) err = SituationSetControl(graph, reverb_handle, 3, 0.30)
        if (sit_ok(err)) err = SituationSetControl(graph, reverb_handle, 4, 0.85)

        err = SituationSetupVirtualMidiLoopback(c_loc(midi_device_id))
        if (sit_ok(err)) then
          err = SituationEnableMidiControl(graph, synth_handle, midi_device_id)
        else
          write(*, '(A)') 'WARNING: Virtual MIDI loopback unavailable — auto-notes only, Space key disabled'
        end if

        if (sit_ok(err)) then
          err = SituationSetActiveGraph(graph)
          if (sit_ok(err)) then
            audio_ok = .true.
            err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(70, c_int8_t), int(0, c_int8_t))
            if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(73, c_int8_t), int(90, c_int8_t))
            if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(75, c_int8_t), int(40, c_int8_t))
            if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(76, c_int8_t), int(80, c_int8_t))
            if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(72, c_int8_t), int(110, c_int8_t))
            write(*, '(A)') 'Audio graph active: ToneSynth -> Echo -> Reverb'
          end if
        end if
      end if
    else
      write(*, '(A)') 'Node creation failed — audio disabled'
    end if
  else
    write(*, '(A)') 'WARNING: Could not create audio graph — audio disabled'
  end if

  backend = SituationGetGraphicsBackend()
  call init_shader_sources(backend == SIT_GRAPHICS_BACKEND_VULKAN, vert_src_vk, vert_src_gl, frag_src_vk, frag_src_gl)

  ! Vulkan: gl_VertexIndex + push_constant fragment (Rust/Zig/Odin pattern).
  ! OpenGL: gl_VertexID + layout(location) uniforms via SituationSetShaderUniform.
  if (backend == SIT_GRAPHICS_BACKEND_VULKAN) then
    err = SituationLoadShaderFromMemory(cstr_ptr(vert_src_vk), cstr_ptr(frag_src_vk), c_loc(shader))
  else
    err = SituationLoadShaderFromMemory(cstr_ptr(vert_src_gl), cstr_ptr(frag_src_gl), c_loc(shader))
  end if
  if (.not. sit_ok(err)) then
    write(*, '(A)') '[ERROR] SituationLoadShaderFromMemory failed'
    call cleanup_resources(graph, audio_ok, shader, fs_mesh, last_note)
    stop 1
  end if

  was_focused = SituationHasWindowFocus()
  shader_refresh_left = 0
  shader_reload_tag = 0
  shader_settle_frames = 0

  tri_verts = [ -1.0_c_float, -1.0_c_float, 0.0_c_float, &
                 3.0_c_float, -1.0_c_float, 0.0_c_float, &
                -1.0_c_float,  3.0_c_float, 0.0_c_float ]
  tri_indices = [ 0_c_int32_t, 1_c_int32_t, 2_c_int32_t ]
  err = SituationCreateMesh(c_loc(tri_verts(1)), 3, int(3_c_size_t * c_sizeof(1.0_c_float), c_size_t), &
    c_loc(tri_indices(1)), 3, c_loc(fs_mesh))
  if (.not. sit_ok(err)) then
    write(*, '(A)') '[ERROR] SituationCreateMesh failed'
    call cleanup_resources(graph, audio_ok, shader, fs_mesh, last_note)
    stop 1
  end if

  default_font = SituationFont( &
    SituationTexture(0, 0, 0, 0), 0.0, 0.0, 0.0, 0.0, 0, C_FALSE)

  call asc2c_32('uTime', uniform_time)
  call asc2c_32('uResolution', uniform_res)

  do while (.not. SituationWindowShouldClose())
    call SituationPollInputEvents()
    call SituationUpdateTimers()

    if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) exit

    window_flags = SituationGetCurrentActualWindowStateFlags()
    if (SituationIsKeyPressed(SIT_KEY_V)) then
      vsync_on = iand(window_flags, SITUATION_WINDOW_STATE_VSYNC_HINT) /= 0
      ! Toggle: was ON -> OFF, was OFF -> ON (merge 1st arg when mask is true)
      call SituationSetVSync(merge(C_FALSE, C_TRUE, vsync_on))
      window_flags = SituationGetCurrentActualWindowStateFlags()
      if (backend == SIT_GRAPHICS_BACKEND_VULKAN) then
        shader_settle_frames = 3
        shader_refresh_left = 6
      end if
    end if

    if (SituationIsKeyPressed(SIT_KEY_F)) then
      call SituationToggleBorderlessWindowed()
      if (backend == SIT_GRAPHICS_BACKEND_VULKAN) then
        shader_settle_frames = 3
        shader_refresh_left = 6
      end if
    end if

    if (SituationIsKeyPressed(SIT_KEY_SPACE) .and. audio_ok) then
      if (last_note > 0) err = SituationVirtualMidiNoteOff(last_note)
      note_idx = rng_range(rng_state, 0, PENTATONIC_LEN)
      last_note = PENTATONIC(note_idx + 1)
      err = SituationVirtualMidiNoteOn(last_note, int(50 + rng_range(rng_state, 0, 30), c_int8_t))
      note_timer = 0.0
    end if

    if (c_associated(graph)) then
      if (SituationIsKeyPressed(SIT_KEY_EQUAL) .and. audio_ok) then
        reverb_wet = min(1.0, reverb_wet + 0.05)
        err = SituationSetControl(graph, reverb_handle, 2, reverb_wet)
      end if
      if (SituationIsKeyPressed(SIT_KEY_MINUS) .and. audio_ok) then
        reverb_wet = max(0.0, reverb_wet - 0.05)
        err = SituationSetControl(graph, reverb_handle, 2, reverb_wet)
      end if
      if (SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET) .and. audio_ok) then
        delay_wet = min(1.0, delay_wet + 0.05)
        err = SituationSetControl(graph, echo_handle, 2, delay_wet)
      end if
      if (SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET) .and. audio_ok) then
        delay_wet = max(0.0, delay_wet - 0.05)
        err = SituationSetControl(graph, echo_handle, 2, delay_wet)
      end if
      if (SituationIsKeyPressed(SIT_KEY_P) .and. audio_ok) then
        delay_feedback = min(0.95, delay_feedback + 0.05)
        err = SituationSetControl(graph, echo_handle, 1, delay_feedback)
      end if
      if (SituationIsKeyPressed(SIT_KEY_O) .and. audio_ok) then
        delay_feedback = max(0.0, delay_feedback - 0.05)
        err = SituationSetControl(graph, echo_handle, 1, delay_feedback)
      end if
    end if

    has_focus = SituationHasWindowFocus()
    if (backend == SIT_GRAPHICS_BACKEND_VULKAN) then
      if (has_focus .and. .not. was_focused) then
        shader_settle_frames = 1
        shader_refresh_left = 6
      end if
      if (shader_settle_frames > 0) then
        shader_settle_frames = shader_settle_frames - 1
      else if (shader_refresh_left > 0) then
        shader_reload_tag = shader_reload_tag + 1
        err = reload_user_shader(shader, .true., shader_reload_tag)
        if (.not. sit_ok(err)) then
          write(*, '(A)') '[WARN] Vulkan shader reload failed'
        else
          shader_refresh_left = shader_refresh_left - 1
        end if
      end if
    end if
    was_focused = has_focus

    dt = SituationGetFrameTime()
    sim_time = sim_time + dt
    note_timer = note_timer + dt

    if (audio_ok .and. note_timer > 4.0) then
      note_timer = 0.0
      if (last_note > 0) err = SituationVirtualMidiNoteOff(last_note)

      err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(70, c_int8_t), &
        int(rng_range(rng_state, 0, 4) * 32, c_int8_t))
      if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(74, c_int8_t), &
        int(40 + rng_range(rng_state, 0, 80), c_int8_t))
      if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(71, c_int8_t), &
        int(20 + rng_range(rng_state, 0, 60), c_int8_t))
      if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(24, c_int8_t), &
        int(rng_range(rng_state, 0, 40), c_int8_t))
      if (sit_ok(err)) err = SituationVirtualMidiControlChange(int(0, c_int8_t), int(26, c_int8_t), &
        int(rng_range(rng_state, 0, 50), c_int8_t))

      note_idx = rng_range(rng_state, 0, PENTATONIC_LEN)
      last_note = PENTATONIC(note_idx + 1)
      err = SituationVirtualMidiNoteOn(last_note, int(30 + rng_range(rng_state, 0, 30), c_int8_t))
    end if

    err = SituationAcquireFrameCommandBuffer()
    if (sit_ok(err)) then
      cmd = SituationGetMainCommandBuffer()
      if (c_associated(cmd)) then
        clear_color = make_rgba(0, 0, 0, 255)
        err = SituationCmdBeginRenderToDisplay(cmd, -1, clear_color)
        if (sit_ok(err)) err = SituationCmdBindPipeline(cmd, shader)

        w = real(SituationGetRenderWidth(), c_float)
        h = real(SituationGetRenderHeight(), c_float)

        if (backend == SIT_GRAPHICS_BACKEND_VULKAN) then
          pc_data(1) = sim_time
          pc_data(2) = 0.0_c_float
          pc_data(3) = w
          pc_data(4) = h
          err = SituationCmdSetPushConstant(cmd, 0, c_loc(pc_data(1)), int(16, c_size_t))
        else
          block
            real(c_float), target :: sim_time_u
            real(c_float), target :: res(2)
            sim_time_u = sim_time
            res(1) = w
            res(2) = h
            err = SituationSetShaderUniform(shader, cstr_ptr(uniform_time), c_loc(sim_time_u), SIT_UNIFORM_FLOAT)
            if (sit_ok(err)) &
              err = SituationSetShaderUniform(shader, cstr_ptr(uniform_res), c_loc(res), SIT_UNIFORM_VEC2)
          end block
        end if

        err = SituationCmdDrawMesh(cmd, fs_mesh)

        sc = h / 600.0
        call asc2c_32('S I T U A T I O N', title_text)
        title_pos = Vector2(w * 0.27, 18.0 * sc)
        white_color = make_rgba(255, 255, 255, 220)
        err = SituationCmdDrawTextEx(cmd, default_font, cstr_ptr(title_text), title_pos, &
          24.0 * sc, 2.0 * sc, white_color)

        vsync_on = iand(window_flags, SITUATION_WINDOW_STATE_VSYNC_HINT) /= 0
        fps = SituationGetFPS()
        if (audio_ok) then
          if (vsync_on) then
            write(hud_ascii1, '(I0,A)') fps, ' FPS  VSync:ON   Audio:active'
          else
            write(hud_ascii1, '(I0,A)') fps, ' FPS  VSync:OFF  Audio:active'
          end if
        else
          if (vsync_on) then
            write(hud_ascii1, '(I0,A)') fps, ' FPS  VSync:ON   Audio:off'
          else
            write(hud_ascii1, '(I0,A)') fps, ' FPS  VSync:OFF  Audio:off'
          end if
        end if
        call asc2c_128(hud_ascii1, hud_line1)
        hud_pos1 = Vector2(10.0, h - 36.0 * sc)
        gray_color = make_rgba(180, 180, 180, 255)
        err = SituationCmdDrawTextEx(cmd, default_font, cstr_ptr(hud_line1), hud_pos1, &
          8.0 * sc, 0.0, gray_color)

        pct_rev = int(reverb_wet * 100.0)
        pct_dly = int(delay_wet * 100.0)
        pct_fb = int(delay_feedback * 100.0)
        write(hud_ascii2, '(A,I0,A,I0,A,I0,A)') 'Reverb: ', pct_rev, '%   Delay: ', pct_dly, &
          '%   Delay FB: ', pct_fb, '%'
        call asc2c_128(hud_ascii2, hud_line2)
        hud_pos2 = Vector2(10.0, h - 20.0 * sc)
        hud_color = make_rgba(140, 210, 255, 255)
        err = SituationCmdDrawTextEx(cmd, default_font, cstr_ptr(hud_line2), hud_pos2, &
          8.0 * sc, 0.0, hud_color)

        err = SituationCmdEndRenderPass(cmd)
      end if
      err = SituationEndFrame()
    end if
  end do

  if (last_note > 0) err = SituationVirtualMidiNoteOff(last_note)
  call cleanup_resources(graph, audio_ok, shader, fs_mesh, last_note)

end program hello_situation