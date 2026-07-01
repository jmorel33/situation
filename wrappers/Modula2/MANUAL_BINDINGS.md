# Situation Modula-2 — Manual bindings

These symbols are **not** emitted in `SituationForeign.def`. Add hand-written Modula-2 wrappers as needed.

| Function | Reason |
|----------|--------|
| `SituationImageDrawTextFormatted` | variadic C function — wrap manually in Odin |
| `SituationLog` | variadic C function — wrap manually in Odin |
| `SituationLogWarning` | variadic C function — wrap manually in Odin |
| `SituationSetLogCallback` | callback registration with nested proc type |
