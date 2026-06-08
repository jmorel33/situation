//! Minimal Situation smoke test — expand after running the binding generator.
//!
//! Build (from wrappers/rust):
//!   cargo build --example hello_situation

use situation::*;

fn main() {
    unsafe {
        let mut config = SituationInitInfo {
            window_width: 1280,
            window_height: 720,
            window_title: c"Hello from Rust".as_ptr(),
            ..std::mem::zeroed()
        };
        let err = SituationInit(0, std::ptr::null(), &mut config);
        if err != SituationError::SITUATION_SUCCESS {
            return;
        }
        SituationShutdown();
    }
}
