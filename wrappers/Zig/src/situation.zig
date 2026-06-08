//! Zig FFI for the Situation C library (auto-generated bindings).
//! Situation 2.4.218 (STL Model Loader, Demon Hunt Visual Bolster Fixes)
//!
//! Re-generate:
//!   python tools/generate_zig_bindings.py
//!
//! Requires a pre-built import library:
//!   build_situation.bat opengl  →  build/dll/situation_opengl.lib
//!
//! Usage (namespaced — recommended):
//!   const sit = @import("situation");
//!   sit.foreign.SituationInit(0, null, &config);
//!
//! Usage (flat — mirrors Odin / C style):
//!   const sit = @import("situation");
//!   sit.SituationInit(0, null, &config);   // via flat re-exports below

pub const types     = @import("situation_types.zig");
pub const callbacks = @import("situation_callbacks.zig");
pub const foreign   = @import("situation_foreign.zig");
pub const constants = @import("situation_constants.zig");
pub const helpers   = @import("situation_helpers.zig");

// --- Flat re-exports (types) ---
pub const ColorRGBA              = types.ColorRGBA;
pub const SitRectangle           = types.SitRectangle;
pub const Vector2                = types.Vector2;
pub const Vector3                = types.Vector3;
pub const Vector4                = types.Vector4;
pub const SituationTexture       = types.SituationTexture;
pub const SituationShader        = types.SituationShader;
pub const SituationFont          = types.SituationFont;
pub const SituationCommandBuffer = types.SituationCommandBuffer;
pub const SituationThreadPool    = types.SituationThreadPool;
pub const SituationAudioGraph    = types.SituationAudioGraph;
pub const SituationNode          = types.SituationNode;
pub const SituationJobId         = types.SituationJobId;
pub const SituationNodeHandle    = types.SituationNodeHandle;
pub const SituationSound         = types.SituationSound;
pub const SituationBuffer        = types.SituationBuffer;
pub const SituationComputePipeline = types.SituationComputePipeline;
pub const SituationInitInfo      = types.SituationInitInfo;
pub const SituationError         = types.SituationError;
pub const Mat4                   = types.Mat4;
pub const mat4                   = types.mat4;

// --- Flat re-exports (helpers) ---
pub const situationBeginFrame = helpers.situationBeginFrame;
pub const situationSuccess    = helpers.situationSuccess;
