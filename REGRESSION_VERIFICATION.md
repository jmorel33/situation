# Regression Verification Log

## Soft Command Buffer Implementation (Phase 1)

### Summary of Changes
- Implemented `SituationGLSoftCommandBuffer` struct and helper functions.
- Implemented `_SituationGLExecuteCommands` to replay commands.
- Updated `SituationCmd*` functions to record commands into the soft buffer when `SITUATION_USE_OPENGL` is defined.
- Fixed critical issue where generic resource IDs were used instead of backend-specific GL handles (e.g., `vao_id`, `gl_texture_id`).

### Verification Steps
1. **Compilation Check:**
   - Compiled `examples/basic_quad.c` with `SITUATION_USE_OPENGL`.
   - Status: **PASSED**

2. **Correctness Check:**
   - Verified that `_SituationGLExecuteCommands` uses `p->args.draw_mesh.mesh.vao_id` and `p->args.present.texture.gl_texture_id`.
   - Verified that `SituationCmd*` recorders store `gl_buffer_id`, `gl_texture_id`, and `gl_program_id` into the packet.
   - Status: **PASSED**

### Known Issues
- None.

### Future Work
- Phase 2: Implement true separate render thread logic (currently runs on main thread at end of frame).
