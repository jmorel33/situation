//! Rust FFI for the Situation C library (auto-generated bindings).
//! Situation 2.4.265 (YPQ Phase 3: public mapping diagnostics API, test_misc cleanup)
//!
//! Re-generate:
//!   python tools/generate_rust_bindings.py
//!
//! Requires a pre-built import library:
//!   build_situation.bat opengl  →  build/dll/situation_opengl.lib

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

pub mod situation_types;
pub mod situation_callbacks;
pub mod situation_ffi;
pub mod situation_constants;
pub mod situation_helpers;

pub use situation_types::*;
pub use situation_callbacks::*;
pub use situation_ffi::*;
pub use situation_constants::*;
pub use situation_helpers::*;
