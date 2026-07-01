# Situation Python — Manual bindings

These symbols are **not** bound in `foreign.py`. See `situation/manual.py`.

| Function | Reason |
|----------|--------|
| `SituationImageDrawTextFormatted` | variadic C function — wrap manually in Odin |
| `SituationLog` | variadic C function — wrap manually in Odin |
| `SituationLogWarning` | variadic C function — wrap manually in Odin |
| `SituationSetLogCallback` | callback registration with nested proc type |
