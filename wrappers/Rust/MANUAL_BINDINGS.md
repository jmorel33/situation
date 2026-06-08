# Situation Rust — Manual bindings

These symbols are **not** emitted in `situation_ffi.rs`. Add hand-written Rust wrappers as needed.

| Function | Reason |
|----------|--------|
| `SituationImageDrawTextFormatted` | variadic C function — wrap manually in Odin |
| `SituationLog` | variadic C function — wrap manually in Odin |
| `SituationLogWarning` | variadic C function — wrap manually in Odin |
| `SituationSetLogCallback` | callback registration with nested proc type |
