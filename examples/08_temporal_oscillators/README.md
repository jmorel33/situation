# 08 — Temporal Oscillators

**Tier:** Feature Spotlight  
**Backends:** OpenGL + Vulkan  
**Assets:** None — procedural audio + geometry

## What you see and hear

A **polyrhythmic pulse machine**: eight colored discs, each driven by its own temporal oscillator at a different beat division relative to the current BPM (default **120**):

| Channel | Division | At 120 BPM |
|---------|----------|------------|
| 1 | 1/4 beat | 125 ms |
| 2 | 1/2 beat | 250 ms |
| 3 | 3/4 beat | 375 ms |
| 4 | **1 beat** | 500 ms (downbeat reference) |
| 5 | 5/4 beat | 625 ms |
| 6 | 3/2 beat | 750 ms |
| 7 | 2 beats | 1.0 s |
| 8 | 4 beats | 2.0 s |

On each trigger the disc **flashes white**, an outer **ping-progress ring** expands, and a short **sine tick** plays at a unique pitch (C5–C6 pentatonic). The result feels like a drum machine wired to eight independent metronomes.

## Controls

| Key | Action |
|-----|--------|
| `-` / `=` (or numpad `-` / `+`) | BPM −5 / +5 (60–200) |
| `1`–`8` | Toggle mute on channel |
| `ESC` | Quit (+ universal hotkeys from `sit_example.h`) |

## What it teaches

| Concept | API |
|---------|-----|
| Set oscillator period in seconds | `SituationSetTimerOscillatorPeriod(id, period_sec)` |
| Edge-detect triggers (game logic pattern) | `SituationTimerGetOscillatorTriggerCount` — compare to last frame's count |
| Visual phase / "time since last ping" | `SituationTimerGetPingProgress` |
| Frame timing | `SituationGetFrameTime`, `SITUATION_BEGIN_FRAME()` → `SituationUpdateTimers` |
| Percussive feedback | `SituationPlayToneEx` with short ADSR |

## Why oscillators?

Most libraries give you `delta_time` and leave beat-sync to you. Situation provides **256 independent drift-free oscillators** — ideal for:

- Music-reactive gameplay (spawn on beat, flash UI on bar)
- Staggered AI pulses without accumulating timer error
- Polyrhythms without a full audio engine

This example makes the edge-detection idiom (`trigger_count` changed since last frame) obvious and audible.

## Build and run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 08_temporal_oscillators
build\examples\08_temporal_oscillators.exe
```

Vulkan: use `static-vulkan` in both build commands. Short name: `temporal_oscillators`
