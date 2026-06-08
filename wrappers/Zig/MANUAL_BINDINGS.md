# Situation Zig — Manual bindings

These symbols are **not** emitted in `situation_foreign.zig`. Add hand-written Zig wrappers as needed.

| Function | Reason |
|----------|--------|
| `SituationImageDrawTextFormatted` | variadic C function — wrap manually in Odin |
| `SituationLog` | variadic C function — wrap manually in Odin |
| `SituationLogWarning` | variadic C function — wrap manually in Odin |
| `SituationSetLogCallback` | callback registration with nested proc type |
