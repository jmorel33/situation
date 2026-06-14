# Tone Synthesis — Legacy Pool vs Graph Node

**Status:** v2.4.123 (2026-05-23)  
**Canonical reference** for the two tone paths, graph **Tone Synth** controls/MIDI, migration, and harness tests.

---

## 1. Why two paths exist

Situation has **two independent synthesizers** that both reach the master bus:

| | **Legacy tone pool** | **Graph tone synth** |
|---|----------------------|----------------------|
| **Module** | `sit/aud/tone_synth.h` | `SITUATION_NODE_TONE_SYNTH` + `sit/aud/tone_synth_graph.h` |
| **Trigger API** | `SituationPlayToneEx`, `SituationPlayMidiNote` | PortMidi → node (`SituationEnableMidiControl`) |
| **Harness prefix** | `legacy_tone_pool_*` | `graph_tone_synth_*` |
| **Instances** | Single global `tone_pool[64]` | **Many nodes** per graph (each with **16** voices, v2.4.119) |
| **Voice storage** | `sit_audio.tone_pool[64]` | `SituationToneSynthNodeState.voices[16]` per node |
| **Oscillators** | miniaudio `ma_waveform` / `ma_noise` | Hand-rolled DSP in `device_wrappers.h` |
| **Filter** | None (default path) | Per-voice SVF (`sit/aud/fx/filter.h`) |
| **Mono / poly** | Poly only | Poly (default) or mono pivot |
| **MIDI CC / bend / program** | No | Full map (v2.4.111+) |
| **Patchable FX chain** | Optional `SetToneRouting` bridge | Native node graph |

The legacy pool is the original **“Resonance”** module: fire-and-forget procedural blips — ADSR, five waveforms, pan, 64 voices. No MIDI channel model, no CC map, no built-in filter.

The graph tone synth (v2.4.109–115) is the **MIDI-first successor**: poly/mono, per-voice filter, pulse width, patchable FX, multi-instance nodes.

---

## 2. Legacy tone pool (summary)

### Mixing

In `sit_miniaudio_data_callback` (`sit/situation_impl_audio.h`):

1. Active graph processes into `pOut` (if any)
2. Loaded voices mix
3. **`tone_mixing:`** — legacy voices **add** to `pOut` via `_SituationMixToneToBuffer`

Per voice: miniaudio sample × ADSR × velocity × linear pan. **No** bus EQ/filter on tones.

### Optional graph bridge

`SituationSetToneRouting` + `SituationSetGraphSFXSource` feeds **legacy PCM** into a Sound Source node. That routes legacy audio through FX; it does **not** use graph synth voices.

---

## 3. Graph tone synth — architecture

### Signal flow

```
PortMidi (hardware or virtual loopback)
  → node_graph_process.h (per-node MIDI channel filter)
  → midi_device_callbacks.h (note/CC/bend/program)
  → _SituationProcessToneSynthNode
       osc (waveform + pulse width)
       → ADSR × velocity × (CC7 × CC11) × CC92 tremolo
       → per-voice SVF filter (if enabled; LFO can mod cutoff)
       → pan (CC10 / control 3)
       → voice sum (L/R)
       → **sum limiter** (Polysonix `EnhancedLimiter`: 1 ms lookahead, threshold **0.95**, release **50 ms**)
  → node output (unpatched → master bus, or patched FX chain)
  → SituationProcessGraph → pOut
```

### Multi-instance

Each `SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &handle)` allocates:

- Its own **16-voice pool** and `SituationToneSynthNodeState`
- Its own **34 control values** (voice template)
- Optional **MIDI device** (`SituationEnableMidiControl`)

Use `SituationSetNodeMidiChannel(graph, handle, ch)` (0–15, or `-1` omni) so multiple synths on one MIDI input respond to different channels.

Virtual MIDI (`SituationVirtualMidiNoteOnEx`, etc.) is **global loopback** — all enabled nodes on matching channels receive messages.

### Mono / poly (control 16)

| Mode | Value | Behavior |
|------|-------|----------|
| **Poly** | 0 (default) | Up to **16** voices per node; standard steal policy; portamento **off** (pitch snaps per voice) |
| **Mono** | 1 | **Voice slot 0 only**; last-note priority on the pivot voice |

MIDI: **CC126** → mono, **CC127** → poly.

### Portamento & legato (mono only, v2.4.118)

Portamento applies only when **control 16 = mono**. Each voice keeps **`base_hz`** (current pitch, glided) and **`target_hz`** (note + bend + mod LFO). The audio loop runs an RC smoother toward `target_hz` every sample when either portamento control is &gt; 0.

#### Controls (registry — not the same numbers as MIDI CC)

| Idx | Name | Default | Range | Role |
|-----|------|---------|-------|------|
| 28 | `portamento_time` | 0 | 0–2 s | Fixed glide time constant **τ** (log UI) |
| 29 | `portamento_speed` | 0 | 0–48 st/s | Glide rate: larger intervals take longer |

**MIDI mapping** (GM-adjacent where noted):

| MIDI CC | Registry control | Scaling (value 0–127 → `norm = value/127`) |
|---------|------------------|-----------------------------------------------|
| **5** | 28 `portamento_time` | `norm × 2` seconds |
| **20** | 29 `portamento_speed` | `norm × 48` semitones per second |

> **Note:** MIDI **CC28** / **CC29** still map to **mod LFO PWM** amount/range (registry controls **22–23**). Portamento uses **CC5** and **CC20** only.

#### How time vs speed is chosen (no separate “mode”)

Both controls are read every glide. Implementation: `_SituationToneSynthVoicePortamentoTauSec` in `sit/aud/tone_synth_graph.h`.

1. If **both ≤ 0** → instant pitch (`base_hz = target_hz`).
2. Let **`time`** = `portamento_time` if &gt; 0, else unused.
3. If **`portamento_speed` &gt; 0** and both pitches are valid, compute  
   `speed_τ = semitones_between(base_hz, target_hz) ÷ speed`  
   (semitones = `12 × log2(max(ratio, 1/ratio))`).
4. **Combine:**
   - **Time only** (`speed` off) → `τ = time`.
   - **Speed only** (`time` off) → `τ = speed_τ` (a tritone glide takes longer than a semitone at the same st/s).
   - **Both on** → `τ = max(time, speed_τ)` — the **slower** glide wins; speed cannot shorten a glide below the fixed time, but can lengthen it on wide intervals.

Glide DSP: `coeff = 1 − exp(−1/(τ × sample_rate))`, then  
`base_hz += (target_hz − base_hz) × coeff` per sample (`_SituationToneSynthVoiceGlidePitch`).

**Examples**

| time | speed | C4→C5 (12 st) | Effect |
|------|-------|---------------|--------|
| 0.35 s | 0 | τ = 0.35 s | Same glide duration for any interval |
| 0 | 48 st/s | τ = 12/48 = 0.25 s | Faster on small hops, scales with interval |
| 0.35 s | 48 st/s | τ = max(0.35, 0.25) = **0.35 s** | Time caps short jumps |
| 0.1 s | 48 st/s | τ = max(0.1, 0.25) = **0.25 s** | Wide interval uses speed floor |

Harness tests drive **CC5 = 0**, **CC20 = 127** (max speed, time off) for a snappy mono sweep.

#### Legato vs retrigger (when portamento actually glides)

On **note-on** in mono (`_SituationToneSynthTriggerNoteOn`):

| Prior voice state | Path | Pitch | Envelope |
|-------------------|------|-------|----------|
| Idle, or in **release** | `_SituationToneSynthVoiceInitFromControls` | `base_hz = target_hz = new note` (then glide only if controls move target later) | Full ADSR from attack |
| **Attack / decay / sustain** (still sounding) | `_SituationToneSynthVoiceLegatoFromControls` | Keeps `base_hz`, sets new `target_hz` → **portamento glides** | Sustain held if already in sustain; no re-attack |

So **linked notes** (new note-on before the old note-off, voice still in ADSR) glide; **unlinked** (note-off → release finishes or gap → new note-on) re-init and snap to the new pitch. Portamento being “on” does not glide across silence — legato linkage is required.

Delayed note-offs (stacked MIDI note-ons, note-off of the previous key after the next) are the usual way to play linked mono lines.

### Sub-oscillator (v2.4.119)

Per-voice **sub-osc** mixed with the main oscillator **before** the SVF (same envelope and filter as the combined signal). Independent **waveform** (same five shapes as the main osc), **octave** offset, **fine tune**, and **level**.

| Idx | Name | Default | Range | MIDI CC |
|-----|------|---------|-------|---------|
| 30 | `sub_level` | 0 | 0–1 | **107** |
| 31 | `sub_waveform` | Sine | 0–4 | **108** (`value % 5`) |
| 32 | `sub_octave` | Oct −1 | 0 = unison, 1 = −1 oct, 2 = −2 oct | **109** |
| 33 | `sub_fine` | 0 | ±1 semitone | **110** (`norm×2−1`) |
| 34 | `sub_coarse` | 0 | ±12 semitones | **111** (`(value−64)/64×12` st from main) |
| 35 | `sub_sync` | Off | bool | **112** (≥64 on) |
| 36 | `sub_ring_mod` | Off | bool | **113** (≥64 on) |

**Pitch** — base = voice `main_hz` (after bend, portamento, mod LFO). Then `sub_coarse`, `sub_octave`, and `sub_fine` apply:

`sub_hz = main_hz × 2^((sub_coarse − sub_octave×12 + sub_fine) / 12)` — `sub_octave` 0/1/2 = unison / −1 oct / −2 oct; `sub_coarse` ±12 st from main

**Mix** (pre-filter): additive `main + sub_level×sub` when **ring off** (CC113 off).

**Ring mod** (CC113 on): **four-quadrant multiply with dry/wet crossfade** — `main×(1−ring_level) + (main×sub)×ring_level`.

Main is the **carrier** (played note). Sub is the **modulator** (CC109–111 set its frequency). At `ring_level = 1` the carrier is fully suppressed — only sum/difference sidebands at **f_main ± f_sub** remain (classic metallic ring mod sound). At intermediate levels you get a blend of dry carrier and ring product. Sweeping CC111 retunes sideband spacing.

**CC107** = ring depth / dry-wet when ring on (defaults to **1** if ring on and level is 0).  
**Additive sub is disabled while ring is on.**

This is **analog four-quadrant multiply** routing, not **SID-style** ring (triangle MSB XOR with the previous voice’s accumulator). For metallic SID bells use **triangle** main and detuned modulator; timbre will differ from a real SID but sideband physics are the same multiply.

**Sync** (CC112, ring off): main = master; sub phase resets each main cycle. Slave ratio =
`main_hz × 2^((sub_coarse + sub_fine) / 12)` (**sub_octave ignored** — CC111 sweeps classic **0.5×…2×**
hard-sync ratios). Output favors the synced sub (`sub + (1−sub_level)×main`).

Each voice has **`sub_phase`** (advanced at `sub_hz`; preserved on mono legato like `phase`). **CC108** updates active voices’ `sub_waveform` (like **CC70** for the main osc).

### Patch memory (v2.4.123)

Per **node** (not global): **16 slots** × snapshot of registry controls **1–36** (waveform … `sub_ring_mod`). Manual **frequency** (ctrl **0**) is **not** stored.

| Idx | Name | MIDI CC | Behaviour |
|-----|------|---------|-----------|
| 37 | `patch_slot` | **114** | Slot **0–15** (`value` quantized to 16 steps). **Recall** when the CC value **changes** (empty slot = no change). |
| 38 | `patch_store` | **115** | **≥64** on a rising edge **saves** current controls into the active slot. `SituationSetControl(..., 38, 1)` also saves. |

Recall copies the slot into `control_values[1..36]`, refreshes waveform/sub-waveform on active voices, ADSR template, and mono enforcement when applicable.

---

## 4. Waveforms

| Index | Registry name | Graph DSP | Legacy (miniaudio) |
|-------|---------------|-----------|-------------------|
| 0 | Sine | `sin(phase)` | `ma_waveform_type_sine` |
| 1 | **Pulse** | Variable duty square (see §5) | `ma_waveform_type_square` (50% fixed) |
| 2 | Triangle | Piecewise linear | `ma_waveform_type_triangle` |
| 3 | Saw | Ramp | `ma_waveform_type_sawtooth` |
| 4 | Noise | Hash noise | `ma_noise` |

**Choosing timbre vs pitch (MIDI):**

- **Note number** → pitch (`SITUATION_MIDI_NOTE_FREQUENCY[note]`)
- **Program change** or **CC70** → waveform `program % 5` / `value % 5`
- **CC106** or control **17** → pulse width (waveform 1 only)

There is **no GM patch bank** — program change selects oscillator shape, not a preset.

---

## 5. Pulse width (waveform 1)

Control **17** `pulse_width`: duty cycle **0.05–0.95** (default **0.5** = symmetric square).

Oscillator: high when `phase < 2π × pulse_width`, else −1.

| Source | Mapping |
|--------|---------|
| Control 17 | Direct float 0.05–0.95 |
| **CC106** | `0.05 + (value/127) × 0.90` |

Only affects waveform **1** (Pulse). Other waveforms ignore pulse width. LFO can also modulate pulse width (§6).

---

## 6. Mod LFO (global)

One **shared LFO** per Tone Synth node — simple by design (not a Polysonix-style mod matrix). Separate from **CC1 vibrato** (fixed 5 Hz sine on pitch) and **CC92 tremolo** (fixed 5 Hz on level).

| Control | Name | Default | Range |
|---------|------|---------|-------|
| 18 | `lfo_rate` | **0 (off)** | 0 or 0.05–20 Hz (log) |
| 19 | `lfo_waveform` | Triangle | 0=tri, 1=square, 2=random S&H |
| 20 | `lfo_pitch_amount` | 0 | 0–1 |
| 21 | `lfo_pitch_range` | 2 st | 0–12 semitones |
| 22 | `lfo_pwm_amount` | 0 | 0–1 |
| 23 | `lfo_pwm_range` | 0.2 | 0–0.45 duty excursion |
| 24 | `lfo_filter_amount` | 0 | 0–1 |
| 25 | `lfo_filter_range` | 1000 Hz | 20–8000 Hz span (log) |

**Formula (bipolar LFO −1..1):** `delta = lfo × amount × range` applied to pitch (semitones), pulse duty, or filter cutoff (Hz, added before keytrack/clamp).

| CC | Maps to |
|----|---------|
| 24 | Rate (0=off) |
| 25 | Waveform |
| 26–27 | Pitch amount / range |
| 28–29 | PWM amount / range |
| 30–31 | Filter amount / range |

---

## 7. Per-voice filter (SVF)

Based on **Polysonix / `sit/aud/fx/filter.h`**: multi-pole state-variable filter, optional 2× oversampling, key tracking from MIDI note.

**Order in voice:** osc → envelope → **filter** → pan.

| Control | Name | Default | Range |
|---------|------|---------|-------|
| 9 | `filter_mode` | Off (0) | 0=Off, 1=LP, 2=HP, 3=BP, 4=Notch, 5=Allpass, 6=LP+BP, 7=LP+HP, 8=BP+HP |
| 10 | `filter_cutoff` | 2000 Hz | 20–20000 (log) |
| 11 | `filter_resonance` | 0.707 | Q 0.5–20 |
| 12 | `filter_poles` | 3 | 1–4 |
| 13 | `filter_drive` | 1.0 | 1–10 |
| 14 | `filter_keytrack` | 0 | 0–1 (`exp2((note−60)/12 × keytrack)` on cutoff) |
| 15 | `filter_oversample` | 1 (2× on) | 0=off, ≥1=2× |

Filter **off by default** (mode 0) so harness parity tests stay clean until CC/filter controls are sent.

### ADSR → filter cutoff

The same per-voice **ADSR envelope** that scales amplitude also modulates cutoff when depth is non-zero (simple filter envelope — not a separate ADSR generator).

| Control | Name | Default | Range |
|---------|------|---------|-------|
| 26 | `filter_env_amount` | 0 | 0–1 |
| 27 | `filter_env_range` | 4000 Hz | 20–8000 Hz span (log) |

**Formula:** `cutoff += envelope × amount × range` (added with LFO filter offset before keytrack/clamp).

| CC | Maps to |
|----|---------|
| 32 | Filter env amount |
| 33 | Filter env range |

---

## 8. Node controls (34 total)

These are **production / runtime parameters** — the values in each node’s `control_values[]` array that the DSP reads (`SituationSetControl`, UI, serialization, and MIDI **after** CC→ctrl mapping). They are **not** raw MIDI bytes.

| Layer | Where | Values |
|-------|--------|--------|
| **ctrl (production)** | `control_values[i]`, `SIT_TONE_CTRL_*` in `tone_synth_graph.h` | Float engineering units (seconds, Hz, 0–1) or **integer** enums (waveform 0–4, octaves 0–2) |
| **MIDI (input)** | CC / note / bend on the wire | **Integers only** (CC 0–127, etc.) — see §9 |

Registry metadata (`registry_init.h`) and the table below list **ctrl** (production) ranges and defaults. EOL comments on `SIT_TONE_CTRL_*` name the index, production type/range, and which **MIDI CC** on the card maps in — not fractional “CC values,” because the card is always integer.

Controls are a **voice template**: copied/used at note-on; some CCs refresh active voices live.

| Idx | Name | ctrl type | Default (ctrl) | Notes |
|-----|------|------|---------|-------|
| 0 | `frequency` | Hz | 440 | Manual path when no MIDI voices |
| 1 | `waveform` | enum | Sine | 0–4 |
| 2 | `volume` | 0–1 | 0.5 | Template; MIDI uses velocity + CC7/11 |
| 3 | `pan` | −1..1 | 0 | CC10 |
| 4 | `attack` | s | 0.01 | CC73 |
| 5 | `decay` | s | 0.1 | CC75 |
| 6 | `sustain` | 0–1 | 0.7 | CC76 |
| 7 | `release` | s | 0.2 | CC72 |
| 8 | `hold` | s | 1.0 | CC77; −1 = infinite |
| 9–15 | Filter block | — | see §6 | |
| 16 | `voice_mode` | Poly/Mono | Poly | CC126/127 |
| 17 | `pulse_width` | duty | 0.5 | CC106 |
| 18–25 | Mod LFO block | — | off | CC24–31; see §6 |
| 26–27 | Filter env mod | — | off | CC32–33; see §7 |
| 28 | `portamento_time` | s | 0 | 0–2; **CC5**; mono only; see §3 |
| 29 | `portamento_speed` | st/s | 0 | 0–48; **CC20**; mono only; see §3 |
| 30 | `sub_level` | 0–1 | 0 | **CC107**; see §3 |
| 31 | `sub_waveform` | enum | Sine | 0–4; **CC108** |
| 32 | `sub_octave` | enum | Oct −1 | 0 / −1 / −2 oct; **CC109** |
| 33 | `sub_fine` | st | 0 | ±1; **CC110** |
| 34 | `sub_coarse` | st | 0 | ±12 from main; **CC111** |

Set via `SituationSetControl(graph, handle, index, value)`.

---

## 9. MIDI reference (graph node)

### Why MIDI and production values stay separate

Almost every MIDI instrument, module, and controller is built around a **MIDI card**: the fixed set of messages the hardware (or USB-MIDI driver) actually sends and receives — note **0–127**, velocity **0–127**, CC **0–127**, program **0–127**, pitch bend **0–16383**. Patch editors, DAW automation lanes, and hardware knobs all speak **that** language. They do not send `0.35` on a cable; they send **CC 5 = 22** (or similar), and the device interprets it.

Situation’s graph Tone Synth follows the same split:

| Layer | Role | Who uses it |
|-------|------|-------------|
| **MIDI card** | Integer messages on the wire | Keyboards, DAWs, `SituationVirtualMidi*`, hardware MIDI |
| **Production (`control_values[]`)** | Real-time DSP parameters (seconds, Hz, levels, enum indices) | `SituationSetControl`, voice code, UI that drives the node directly |

The **CC map** (§9 table) is the translation table from MIDI card → production. Registry defaults and §8 are production values. Confusing the two (e.g. documenting `0.5` as if it were a CC value) breaks interoperability with any normal MIDI gear.

### Integer MIDI values (wire format)

All MIDI traffic uses **integer bytes only** — there are no fractional CC values on the wire:

| Message | Integer fields |
|---------|----------------|
| Note on/off | note **0–127**, velocity **0–127** |
| Control change | controller **0–127**, value **0–127** |
| Program change | program **0–127** |
| Pitch bend | **0–16383** (center 8192) |

`SituationVirtualMidi*` APIs take `uint8_t` (or `int16_t` for bend). Values **> 127** are clamped before send/process.

**Internal node controls** (`SituationSetControl`) are **floats** in engineering units (seconds, Hz, 0–1 level). That path is for UI/harness, not raw MIDI.

**From MIDI CC**, the tone synth maps in two ways:

1. **Continuous** — `norm = value / 127` then scale (e.g. CC5 → portamento seconds, CC7 → volume). The CC is still an integer; the stored control is float.
2. **Discrete** — integer CC → integer index only (`value % 5` for waveforms; `(value×(steps−1)+63)/127` for enums like sub octave, filter mode, LFO wave). Stored as `0.0f`, `1.0f`, … with no fractional enum values.

Sub fine (**CC110**): **128 steps**, MIDI **64 = 0 st**, **0 → −1 st**, **127 → +1 st** (`(value − 64) / 64` semitones internally).

Sub coarse (**CC111**): **128 steps**, MIDI **64 = 0 st**, **0 → −12 st**, **127 → +12 st** (`(value − 64) / 64 × 12` semitones from main note).

### Channel messages

| Message | Handling |
|---------|----------|
| **Note on** (0x90) | Alloc voice (mono → slot 0); legato or full init (§3); velocity → `volume_peak` |
| **Note off** (0x80, or vel 0) | Release matching note (respect CC64 sustain) |
| **Pitch bend** (0xE0) | ±2 semitones: `((bend/8192) − 1) × 2` on all active voices |
| **Program change** (0xC0) | Waveform = `program % 5` |

### Control change (CC)

| CC | Control / target | Scaling (value 0–127) |
|----|------------------|------------------------|
| 1 | Vibrato depth | `norm × 1` semitone, 5 Hz LFO |
| 5 | Portamento time | `norm × 2` s → control **28** |
| 7 | Channel volume | `norm` → `ch_volume` |
| 20 | Portamento speed | `norm × 48` st/s → control **29** |
| 107 | Sub level | `norm` → control **30** |
| 108 | Sub waveform | `value % 5` → control **31** |
| 109 | Sub octave | integer **0 / 1 / 2** → control **32** |
| 110 | Sub fine tune | integer **0–127** → **(value−64)/64** st → control **33** |
| 111 | Sub coarse tune | integer **0–127** → **(value−64)/64×12** st from main → control **34** |
| 10 | Pan | `norm × 2 − 1` → control 3 |
| 11 | Expression | `norm` → `expression` |
| 16 | Filter mode | `floor(norm × 8.99)` → 0–8 |
| 17 | Filter drive | `norm × 9 + 1` |
| 18 | Filter oversample | `floor(norm × 2.99)` → 0–2 |
| 22 | Filter keytrack | `norm` → 0–1 |
| 64 | Sustain pedal | ≥64 on; off releases pending notes |
| 70 | Waveform | `value % 5` |
| 71 | Filter resonance Q | `norm × 19.5 + 0.5` |
| 72 | Release time | `norm × 5` s → control 7 |
| 73 | Attack time | `norm × 2` s → control 4 |
| 74 | Filter cutoff | log **20 Hz–20 kHz** |
| 75 | Decay time | `norm × 2` s → control 5 |
| 76 | Sustain level | `norm` → control 6 |
| 77 | Hold time | `norm × 4` s → control 8 |
| 92 | Tremolo depth | `norm`, 5 Hz amplitude LFO |
| 102 | Filter poles | `floor(norm × 3.99) + 1` → 1–4 |
| 106 | Pulse width | `0.05 + norm × 0.90` duty |
| 24 | LFO rate | 0=off; else log 0.05–20 Hz |
| 25 | LFO waveform | 0=tri, 1=square, 2=random |
| 26 | LFO pitch amount | `norm` |
| 27 | LFO pitch range | `norm × 12` semitones |
| 28 | LFO PWM amount | `norm` |
| 29 | LFO PWM range | `norm × 0.45` duty |
| 30 | LFO filter amount | `norm` |
| 31 | LFO filter range | log 20–8000 Hz |
| 32 | Filter env amount | `norm` |
| 33 | Filter env range | log 20–8000 Hz |
| 123 | All notes off | Release all voices |
| 126 | Mono mode | Sets control 16 = mono |
| 127 | Poly mode | Sets control 16 = poly |

**Effective level:** `envelope × (velocity/127) × ch_volume × expression × tremolo_mult`.

### Virtual MIDI API

```c
SituationVirtualMidiNoteOnEx(channel, note, velocity);
SituationVirtualMidiNoteOffEx(channel, note);
SituationVirtualMidiControlChange(channel, cc, value);
SituationVirtualMidiPitchBend(channel, bend);      // 0..16383, center 8192
SituationVirtualMidiProgramChange(channel, program);
SituationSetNodeMidiChannel(graph, handle, channel); // 0–15 or -1 omni
```

Requires `SituationEnableMidiControl(graph, handle, device_id)` and active graph.

---

## 10. Parity vs legacy

We do **not** require bit-identical output (different oscillator engines).

| Behavior | Legacy | Graph | Notes |
|----------|--------|-------|-------|
| Poly + steal | 64 (legacy pool) | **16** per graph node (v2.4.119) | Graph also has mono mode |
| ADSR + hold | ✓ | ✓ | |
| Velocity → level | ✓ | ✓ | |
| Pan | ✓ | ✓ | |
| Five waveforms | ✓ | ✓ | Pulse width on graph only |
| A4 = 440 Hz | ✓ | ✓ | `tone_synth_phase1_compare_a4` |
| Pitch bend / CC / sustain | ✗ | ✓ | |
| Per-voice filter | ✗ | ✓ | v2.4.113 |
| Mono mode | ✗ | ✓ | v2.4.114 |
| Mod LFO (pitch/PWM/filter) | ✗ | ✓ | v2.4.116 |
| ADSR → filter cutoff | ✗ | ✓ | v2.4.117 |
| Mono portamento (time / speed) | ✗ | ✓ | v2.4.118 |
| Mono legato (linked notes) | ✗ | ✓ | v2.4.118 |
| Sub-oscillator (wave / oct / fine) | ✗ | ✓ | v2.4.119 |

Phase 1 sample (sine A4, exclusive paths):

```
legacy: hz=440.00 peak≈0.60 rms≈0.40
graph:  hz=440.00 peak≈0.55 rms≈0.40
```

| Patchable FX | bridge only | native | |

---

## 11. Migration plan

### Phase 1 — Side-by-side compare ✅

One path active at a time. Harness: `tone_synth_phase1_compare_a4`.

```powershell
Set-Location build
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module audio --filter tone_synth_phase1_compare_a4
```

### Phase 2 — App switchover (next)

1. `SituationCreateGraph` + one or more `SITUATION_NODE_TONE_SYNTH` nodes (+ optional FX).
2. `SituationEnableMidiControl` + `SituationSetNodeMidiChannel`.
3. `SituationSetActiveGraph(graph)`.
4. Stop calling `PlayToneEx` / `PlayMidiNote`; `SituationStopAllTones()` at cutover.
5. Drive notes via MIDI (hardware or virtual).

Procedural call sites need a note+CC mapper or a future `SituationPlayGraphToneEx` — **not built yet**.

### Phase 3 — Optional legacy PCM → graph FX

Keep `SetToneRouting` only if legacy oscillators must run through graph FX without migrating synthesis.

---

## 12. Critical rules when flipping paths

1. **Never run both engines** for the same content — outputs are **additive**.
2. **`SetToneRouting` ≠ graph synth** — legacy PCM into Sound Source only.
3. **Graph active ≠ legacy silent** — stop legacy API calls explicitly.
4. Failed harness tests must call `sit_midi_graph_fixture_release` before the next MIDI test.

---

## 13. Harness tests (Phase 8)

| Test | What it verifies |
|------|------------------|
| `legacy_tone_pool_midi_note_frequency` | Legacy A4 pitch |
| `graph_tone_synth_midi_note_frequency` | Graph A4 via virtual MIDI |
| `tone_synth_phase1_compare_a4` | Legacy vs graph, exclusive |
| `graph_tone_synth_midi_complex_melody` | Bend, CC1, velocity |
| `graph_tone_synth_velocity_ramp` | Velocity → RMS |
| `graph_tone_synth_cc_mod_vibrato` | CC1 pitch LFO |
| `graph_tone_synth_cc92_tremolo` | CC92 amplitude LFO |
| `graph_tone_synth_filter_modes` | LP/HP vs bypass @ 440 Hz |
| `graph_tone_synth_pulse_width` | Wide vs narrow pulse @ 440 Hz |
| `graph_tone_synth_waveforms_all` | CC70 waveforms 0–4 on A4 |
| `graph_tone_synth_lfo_mod` | LFO pitch (sine) + PWM (pulse) |
| `graph_tone_synth_filter_env_adsr` | ADSR opens LP cutoff @ 440 Hz |
| `sub_oscillator` | A4 + sub sine oct−1; energy at 220 Hz |
| `mono_portamento_linked` | Mono, CC5 off / CC20 max; 4-note legato phrase C–E–G–C; glide on linked steps |
| `mono_portamento_unlinked` | Same phrase with gap between notes; snap, no glide from prior pitch |

Dedicated module (legacy pool + graph tone synth + MIDI):

```powershell
Set-Location build
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module tone_synth
& ".\sit_test.exe" --module tone_synth --filter portamento
```

Graph-only MIDI/filter tests also remain under `--module audio --filter graph_tone_synth` where not yet moved.

---

## 14. Key source files

| Area | Path |
|------|------|
| Legacy API + pool | `sit/aud/tone_synth.h` |
| Legacy mix | `sit/situation_impl_audio.h` |
| Graph voices + CC | `sit/aud/tone_synth_graph.h` |
| Graph process + filter in voice | `sit/aud/device_wrappers.h` |
| Shared SVF | `sit/aud/fx/filter.h` |
| MIDI callbacks | `sit/aud/midi_device_callbacks.h` |
| MIDI routing | `sit/aud/node_graph_process.h`, `sit/aud/node_graph_midi.h` |
| Control registry | `sit/aud/registry_init.h` |
| Public API | `sit/situation_api.h` |
| Harness | `tests/harness/test_tone_synth.c`, `tests/harness/test_audio.c`, `tests/harness/midi_test_info.h` |
| Changelog | `doc/UPDATELOG.md` (v2.4.109–119) |

---

## 15. Out of scope (for now)

- Unifying legacy pool and graph into one engine
- Public graph voice handles like `SituationPlayToneEx` return values
- Bit-identical miniaudio vs graph oscillators
- Polyphonic portamento / per-voice glide in poly mode
- Global legacy tone reverb in main impl

---

## Appendix — Resonance legacy design (v2.3.38)

The legacy pool was specified as zero-allocation 64-voice poly, frame-perfect ADSR, miniaudio band-limited waveforms, fire-and-forget API for UI/SFX. See `sit/aud/tone_synth.h` and `doc/plan/TONE_SYNTH_EXTRACTION_PLAN.md`.
