# 04 — Play a Sound

**Tier:** Fundamental  
**Backends:** OpenGL + Vulkan  
**Audio assets required:** None — 100% procedural

## What you see and hear

A visual piano-style keyboard with 5 keys (C4, E4, G4, C5, E5). Pressing number keys `1`–`5` plays the corresponding note with a piano-like ADSR envelope. Keys light up when triggered.

## Controls

| Key | Sound |
|-----|-------|
| `1` | C4 (261.6 Hz) |
| `2` | E4 (329.6 Hz) |
| `3` | G4 (392.0 Hz) |
| `4` | C5 (523.3 Hz) |
| `5` | E5 (659.3 Hz) |
| `Q` | Major chord — C4 + E4 + G4 (3 simultaneous voices) |
| `M` | Minor chord — C4 + Eb4 + G4 |
| `R` | Pad chord — same 4 notes with long ADSR (beautiful decay) |
| `W` | Cycle wave type: Sine → Square → Triangle → Sawtooth → Noise |
| `Space` | Stop all active tones immediately |

## What it teaches

| Concept | API |
|---------|-----|
| Play a synthesized tone | `SituationPlayToneEx(wave, freq, vol, pan, attack, decay, sustain, release, hold)` |
| Note frequency lookup | `SITUATION_MIDI_NOTE_FREQUENCY[midi_note]` — 128-entry float array (MIDI 0 = C-1 = 8.175 Hz) |
| Multiple simultaneous voices | Call `SituationPlayToneEx` multiple times (up to 64 voices) |
| Instant silence | `SituationStopAllTones()` |
| Route tones to output | `SituationSetActiveGraph(NULL)` + `SituationResumeAudioDevice()` |
| Wave types | `SIT_WAVE_SINE`, `SIT_WAVE_SQUARE`, `SIT_WAVE_TRIANGLE`, `SIT_WAVE_SAW`, `SIT_WAVE_NOISE` |

## ADSR parameter guide

```
SituationPlayToneEx(
    wave,          // SituationWaveType — the oscillator shape
    freq,          // Hz — pitch (use SITUATION_MIDI_NOTE_FREQUENCY[note])
    volume,        // 0.0 – 1.0 — peak amplitude
    pan,           // -1.0 (left) to +1.0 (right), 0 = centre
    attack_sec,    // ramp-up time before the peak
    decay_sec,     // fall from peak to sustain level
    sustain_level, // 0.0 – 1.0 — fraction of peak held while key is down
    release_sec,   // fade-out after hold ends
    hold_sec       // how long before release phase starts
);
```

For a piano feel: short attack (0.004s), quick decay (0.08s), moderate hold (0.18s).  
For a pad/reverb feel: longer attack (0.02s), long release (0.8s), longer hold (0.8s).

## Why no audio files?

Situation's tone pool synthesizes audio directly in the mixing callback. No loading, no decoding, no disk access — instant playback with zero latency. This is perfect for game SFX and procedural music.

For file-based audio, see the `SituationLoadSoundFromFile` / `SituationPlayLoadedSound` API described in `doc/situation_api.md`.

## Build

```bat
build\build_situation.bat static-opengl
build\build_examples.bat  static-opengl 04_play_a_sound
build\examples\04_play_a_sound.exe
```
