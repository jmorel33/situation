# Polysonix VM v1.9.0 Performance Report

This report compares the execution time of the new Flat Opcode VM (v1.9.0) against the v1.8.10 baseline.
Both sets of measurements were taken on this cloud environment for direct comparison.

| Patch ID | Name | v1.8.10 (Before) (ns) | v1.9.7 (After) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
Initializing LFSR tables...
  Generating LFSR type LFSR_4BIT (enum 0, bits=4, period=15, tap_mask=0x00000003, seed=0x1)...
Initializing LFSR Type: LFSR_4BIT (Enum val: 0, Config Index: 0)
  Config: bit_length=4, tap_mask=0x3, period=15, seed=1
  Effective for gen: bit_length=4, period_loop_limit=15
  Stored in table: period=15. Bit table allocated: Yes
  First 4 bytes of table: 0x91 0x75 0x00 0x00
----
  Generating LFSR type LFSR_5BIT (enum 1, bits=5, period=31, tap_mask=0x00000005, seed=0x1)...
Initializing LFSR Type: LFSR_5BIT (Enum val: 1, Config Index: 1)
  Config: bit_length=5, tap_mask=0x5, period=31, seed=1
  Effective for gen: bit_length=5, period_loop_limit=31
  Stored in table: period=31. Bit table allocated: Yes
  First 4 bytes of table: 0x21 0xCD 0xC7 0x2E
----
  Generating LFSR type LFSR_6BIT (enum 2, bits=6, period=63, tap_mask=0x00000003, seed=0x1)...
Initializing LFSR Type: LFSR_6BIT (Enum val: 2, Config Index: 2)
  Config: bit_length=6, tap_mask=0x3, period=63, seed=1
  Effective for gen: bit_length=6, period_loop_limit=63
  Stored in table: period=63. Bit table allocated: Yes
  First 4 bytes of table: 0x41 0x18 0xE5 0xC5
----
  Generating LFSR type LFSR_7BIT (enum 3, bits=7, period=127, tap_mask=0x00000009, seed=0x1)...
Initializing LFSR Type: LFSR_7BIT (Enum val: 3, Config Index: 3)
  Config: bit_length=7, tap_mask=0x9, period=127, seed=1
  Effective for gen: bit_length=7, period_loop_limit=127
  Stored in table: period=127. Bit table allocated: Yes
  First 4 bytes of table: 0x81 0xC8 0xE8 0xDA
----
  Generating LFSR type LFSR_8BIT (enum 4, bits=8, period=255, tap_mask=0x0000001D, seed=0x1)...
Initializing LFSR Type: LFSR_8BIT (Enum val: 4, Config Index: 4)
  Config: bit_length=8, tap_mask=0x1D, period=255, seed=1
  Effective for gen: bit_length=8, period_loop_limit=255
  Stored in table: period=255. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x71 0xA4 0x03
----
  Generating LFSR type LFSR_9BIT (enum 5, bits=9, period=511, tap_mask=0x00000011, seed=0x1)...
Initializing LFSR Type: LFSR_9BIT (Enum val: 5, Config Index: 5)
  Config: bit_length=9, tap_mask=0x11, period=511, seed=1
  Effective for gen: bit_length=9, period_loop_limit=511
  Stored in table: period=511. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x42 0x0C 0x39
----
  Generating LFSR type LFSR_10BIT (enum 6, bits=10, period=1023, tap_mask=0x00000009, seed=0x1)...
Initializing LFSR Type: LFSR_10BIT (Enum val: 6, Config Index: 6)
  Config: bit_length=10, tap_mask=0x9, period=1023, seed=1
  Effective for gen: bit_length=10, period_loop_limit=1023
  Stored in table: period=1023. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x04 0x12 0xC1
----
  Generating LFSR type LFSR_11BIT (enum 7, bits=11, period=2047, tap_mask=0x00000005, seed=0x1)...
Initializing LFSR Type: LFSR_11BIT (Enum val: 7, Config Index: 7)
  Config: bit_length=11, tap_mask=0x5, period=2047, seed=1
  Effective for gen: bit_length=11, period_loop_limit=2047
  Stored in table: period=2047. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x08 0x50 0x20
----
  Generating LFSR type LFSR_12BIT (enum 8, bits=12, period=4095, tap_mask=0x00000053, seed=0x1)...
Initializing LFSR Type: LFSR_12BIT (Enum val: 8, Config Index: 8)
  Config: bit_length=12, tap_mask=0x53, period=4095, seed=1
  Effective for gen: bit_length=12, period_loop_limit=4095
  Stored in table: period=4095. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x10 0x94 0x50
----
  Generating LFSR type LFSR_13BIT (enum 9, bits=13, period=8191, tap_mask=0x0000001B, seed=0x1)...
Initializing LFSR Type: LFSR_13BIT (Enum val: 9, Config Index: 9)
  Config: bit_length=13, tap_mask=0x1B, period=8191, seed=1
  Effective for gen: bit_length=13, period_loop_limit=8191
  Stored in table: period=8191. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x20 0xC0 0x86
----
  Generating LFSR type LFSR_14BIT (enum 10, bits=14, period=16383, tap_mask=0x00001007, seed=0x1)...
Initializing LFSR Type: LFSR_14BIT (Enum val: 10, Config Index: 10)
  Config: bit_length=14, tap_mask=0x1007, period=16383, seed=1
  Effective for gen: bit_length=14, period_loop_limit=16383
  Stored in table: period=16383. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x40 0x55 0x89
----
  Generating LFSR type LFSR_15BIT (enum 11, bits=15, period=32767, tap_mask=0x00000003, seed=0x1)...
Initializing LFSR Type: LFSR_15BIT (Enum val: 11, Config Index: 11)
  Config: bit_length=15, tap_mask=0x3, period=32767, seed=1
  Effective for gen: bit_length=15, period_loop_limit=32767
  Stored in table: period=32767. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x80 0x00 0x60
----
  Generating LFSR type LFSR_16BIT (enum 12, bits=16, period=65535, tap_mask=0x0000002D, seed=0x1)...
Initializing LFSR Type: LFSR_16BIT (Enum val: 12, Config Index: 12)
  Config: bit_length=16, tap_mask=0x2D, period=65535, seed=1
  Effective for gen: bit_length=16, period_loop_limit=65535
  Stored in table: period=65535. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x00 0x01 0x68
----
  Generating LFSR type LFSR_17BIT (enum 13, bits=17, period=131071, tap_mask=0x00004001, seed=0x1)...
Initializing LFSR Type: LFSR_17BIT (Enum val: 13, Config Index: 13)
  Config: bit_length=17, tap_mask=0x4001, period=131071, seed=1
  Effective for gen: bit_length=17, period_loop_limit=131071
  Stored in table: period=131071. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x00 0x92 0x24
----
  Generating LFSR type LFSR_GALOIS (enum 14, bits=16, period=65535, tap_mask=0x0000002D, seed=0x1)...
Initializing LFSR Type: LFSR_GALOIS (Enum val: 14, Config Index: 14)
  Config: bit_length=16, tap_mask=0x2D, period=65535, seed=1
  Effective for gen: bit_length=16, period_loop_limit=65535
  Stored in table: period=65535. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x00 0x01 0x68
----
  Generating LFSR type LFSR_FIBONACCI (enum 15, bits=16, period=65535, tap_mask=0x0000002D, seed=0x1)...
Initializing LFSR Type: LFSR_FIBONACCI (Enum val: 15, Config Index: 15)
  Config: bit_length=16, tap_mask=0x2D, period=65535, seed=1
  Effective for gen: bit_length=16, period_loop_limit=65535
  Stored in table: period=65535. Bit table allocated: Yes
  First 4 bytes of table: 0x01 0x00 0x01 0x68
----
LFSR tables initialized.
Bytecode cache initialized (Table size: 512).
| 0 | Triangle Up | 59.97 | 54.46 | **9.19%** |
| 1 | Triangle Down | 52.41 | 54.04 | -3.11% |
| 2 | Sine Up | 153.14 | 94.36 | **38.38%** |
| 3 | Sine Down | 148.86 | 98.80 | **33.63%** |
| 4 | Square Up | 56.45 | 59.26 | -4.98% |
| 5 | Square Down | 63.85 | 59.09 | **7.45%** |
| 6 | Saw Rising | 73.47 | 77.22 | -5.10% |
| 7 | Saw Falling | 71.29 | 78.71 | -10.41% |
| 8 | Saw/Sine Up | 134.48 | 90.21 | **32.92%** |
| 9 | Sine/Saw Down | 162.98 | 93.90 | **42.39%** |
| 10 | Square/Sine Up | 109.50 | 81.97 | **25.14%** |
| 11 | Sine/Square Down | 158.11 | 81.41 | **48.51%** |
| 12 | Saw/Triangle Up | 34.33 | 42.28 | -23.17% |
| 13 | Triangle/Saw Down | 46.66 | 39.01 | **16.40%** |
| 14 | Triangle/Sine Up | 72.93 | 77.25 | -5.92% |
| 15 | Sine/Triangle Down | 76.24 | 68.61 | **10.01%** |
| 16 | Clipped Sine | 55.23 | 51.67 | **6.45%** |
| 17 | Rectified Sine | 49.24 | 51.77 | -5.14% |
| 18 | Sine * Saw | 92.70 | 53.60 | **42.18%** |
| 19 | Overload Spark | 52.92 | 78.49 | -48.33% |
| 20 | Overfolded Saw | 63.35 | 63.88 | -0.84% |
| 21 | Clipped Chaos | 67.91 | 74.59 | -9.84% |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 70.84 | 81.09 | -14.46% |
| 23 | Triangle Fold | 0.00 | 43.95 | N/A |
| 24 | Math: Tanh Drive | 0.00 | 31.85 | N/A |
| 25 | Math: Cubic | 0.00 | 32.68 | N/A |
| 26 | Math: Rectified | 0.00 | 24.40 | N/A |
| 27 | Math: Sinc | 0.00 | 26.53 | N/A |
| 28 | Weird: Step-Slope | 0.00 | 21.63 | N/A |
| 29 | Gritty Bass | 68.62 | 66.70 | **2.80%** |
| 30 | Hybrid Saw*Sine | 43.51 | 45.94 | -5.58% |
| 31 | Razor Pulse | 59.42 | 69.20 | -16.46% |
| 32 | Pulse 25% | 47.81 | 52.00 | -8.76% |
| 33 | Pulse 75% | 49.15 | 56.28 | -14.52% |
| 34 | Staircase 4 Step | 49.12 | 77.50 | -57.79% |
| 35 | Bit Crush Bomb | 52.33 | 79.42 | -51.76% |
| 36 | Bit-Crushed Square | 62.57 | 63.04 | -0.76% |
| 37 | Pulse Train Wreck | 51.69 | 101.01 | -95.41% |
| 38 | Narrow | 25.11 | 42.29 | -68.42% |
| 39 | Quantized Saw 8 | 37.33 | 37.27 | **0.17%** |
| 40 | PWM Synth (A=Width B=Sub) | 58.75 | 64.47 | -9.74% |
| 41 | PWM Gate | 39.91 | 39.90 | **0.01%** |
| 42 | Harmonic Switch | 61.72 | 72.16 | -16.91% |
| 43 | Multi-Gate | 51.71 | 67.37 | -30.28% |
| 44 | Bitwise Staircase | 0.00 | 31.23 | N/A |
| 45 | Bitwise XOR Wave | 0.00 | 62.06 | N/A |
| 46 | Hard Quantize Sine | 0.00 | 39.53 | N/A |
| 47 | Comparator Fuzz | 0.00 | 41.85 | N/A |
| 48 | Warp Speed | 22.80 | 31.25 | -37.07% |
| 49 | Ghost Wail | 50.31 | 48.81 | **2.98%** |
| 50 | Laser Malfunction | 63.90 | 61.91 | **3.11%** |
| 51 | Hyperspace Glitch | 24.44 | 63.26 | -158.82% |
| 52 | Shredded Saw | 71.11 | 67.02 | **5.75%** |
| 53 | Glitch Sine | 70.68 | 69.47 | **1.71%** |
| 54 | 4-Segment Bump | 55.12 | 56.66 | -2.79% |
| 55 | Bird Call AM | 55.31 | 54.42 | **1.60%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 71.14 | 70.80 | **0.48%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 55.27 | 52.15 | **5.65%** |
| 58 | Phase Glitch | 61.75 | 58.78 | **4.81%** |
| 59 | Phase Distortion Wave | 205.38 | 158.06 | **23.04%** |
| 60 | PD: Resonant | 0.00 | 38.42 | N/A |
| 61 | PD: Wrap | 0.00 | 41.54 | N/A |
| 62 | PD: Spike | 0.00 | 34.59 | N/A |
| 63 | PD: Windowed | 0.00 | 36.59 | N/A |
| 64 | Classic FM EP (A=Index B=Detune) | 87.44 | 79.42 | **9.17%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 127.29 | 80.43 | **36.81%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 60.81 | 65.22 | -7.25% |
| 67 | Complex FM A=Index B=ModFreq | 58.54 | 57.64 | **1.53%** |
| 68 | FM Pluck | 148.81 | 145.68 | **2.10%** |
| 69 | FM Pitched Grit | 154.93 | 107.36 | **30.70%** |
| 70 | FM Dynamic Lead | 162.73 | 182.83 | -12.35% |
| 71 | FM Glassy Evolve | 106.23 | 107.63 | -1.32% |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 88.94 | 91.85 | -3.28% |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 79.52 | 82.03 | -3.16% |
| 74 | FM: Deep Sub | 0.00 | 35.53 | N/A |
| 75 | FM: Talker | 0.00 | 43.93 | N/A |
| 76 | FM: Feedback Sim | 0.00 | 62.83 | N/A |
| 77 | FM: Cascaded | 0.00 | 64.61 | N/A |
| 78 | FM: Vowel-ish | 0.00 | 39.02 | N/A |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 74.02 | 77.79 | -5.10% |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 105.94 | 101.67 | **4.03%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 96.89 | 96.30 | **0.61%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 104.61 | 110.93 | -6.04% |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 126.57 | 129.56 | -2.36% |
| 84 | FM Bipolar Sweep Pad | 85.96 | 88.39 | -2.83% |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 148.25 | 103.06 | **30.48%** |
| 86 | FM: Noise | 0.00 | 61.68 | N/A |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 106.28 | 97.83 | **7.95%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 106.45 | 112.13 | -5.33% |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 101.17 | 103.91 | -2.71% |
| 90 | FM: Metallic 1 | 0.00 | 42.84 | N/A |
| 91 | FM: Metallic 2 | 0.00 | 43.04 | N/A |
| 92 | Weird: AM Chaos | 0.00 | 36.68 | N/A |
| 93 | Sci-Fi Drone | 246.29 | 209.09 | **15.10%** |
| 94 | Evolving Metallic Bell | 625.29 | 585.61 | **6.35%** |
| 95 | Alien Communication | 204.98 | 248.75 | -21.35% |
| 96 | Sine Harmonics | 84.07 | 80.77 | **3.92%** |
| 97 | Harmonic Noise Blast | 85.80 | 86.52 | -0.83% |
| 98 | Brass | 123.58 | 121.47 | **1.70%** |
| 99 | Bowed String | 135.04 | 127.51 | **5.58%** |
| 100 | Additive Square | 88.20 | 87.67 | **0.60%** |
| 101 | Electric Pianoish | 83.73 | 85.16 | -1.71% |
| 102 | Classic Pad | 85.66 | 84.01 | **1.92%** |
| 103 | Additive Saw (A=Harms B=Shape) | 310.54 | 86.95 | **72.00%** |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 277.43 | 132.72 | **52.16%** |
| 105 | Grit Additive (A=Grit B=Tone) | 262.65 | 142.43 | **45.77%** |
| 106 | Simple Minor Triad | 105.75 | 108.99 | -3.06% |
| 107 | Add: Spec 1 | 0.00 | 68.62 | N/A |
| 108 | Add: Spec 2 | 0.00 | 66.74 | N/A |
| 109 | Add: Bell | 0.00 | 50.04 | N/A |
| 110 | Add: Organ | 0.00 | 65.84 | N/A |
| 111 | Add: Random Phase | 0.00 | 47.75 | N/A |
| 112 | Formantish | 68.63 | 64.27 | **6.35%** |
| 113 | Vocal Ah | 103.10 | 100.72 | **2.31%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 130.23 | 87.57 | **32.75%** |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 472.08 | 457.33 | **3.13%** |
| 116 | Sync Sweep No Slant | 43.72 | 47.36 | -8.32% |
| 117 | Sync Sweep Cos Shape | 61.15 | 58.84 | **3.78%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 121.29 | 72.23 | **40.45%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 60.43 | 62.58 | -3.55% |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 58.71 | 62.53 | -6.51% |
| 121 | Oooh Choir Formant | 2302.03 | 1803.25 | **21.67%** |
| 122 | PD Vocal Formant | 0.00 | 28.17 | N/A |
| 123 | Sync Soft | 0.00 | 39.30 | N/A |
| 124 | Fractal Sine | 0.00 | 64.66 | N/A |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 93.22 | 88.74 | **4.80%** |
| 126 | Add: Saw 8 | 0.00 | 277.13 | N/A |
| 127 | Add: Square 8 | 0.00 | 288.91 | N/A |
| 128 | Kick Drum | 69.88 | 68.27 | **2.30%** |
| 129 | Snare Drum | 75.89 | 71.08 | **6.33%** |
| 130 | Clap | 94.90 | 89.69 | **5.49%** |
| 131 | Tom Drum | 65.89 | 69.09 | -4.85% |
| 132 | Cymbalish | 74.89 | 69.36 | **7.38%** |
| 133 | Double Waves | 108.08 | 108.30 | -0.20% |
| 134 | Metal Impact | 76.78 | 70.83 | **7.75%** |
| 135 | Bell Tone | 87.75 | 83.70 | **4.61%** |
| 136 | Metallic Perc | 72.81 | 67.98 | **6.64%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 300.02 | 294.96 | **1.69%** |
| 138 | Classic Noise Sim | 141.28 | 144.74 | -2.45% |
| 139 | Distorted Pitch | 129.51 | 126.93 | **1.99%** |
| 140 | Gritty Rumble Noise | 105.03 | 107.26 | -2.13% |
| 141 | Filtered Static Noise | 456.36 | 433.08 | **5.10%** |
| 142 | Wooden Percussion | 86.53 | 88.36 | -2.11% |
| 143 | Glitchy Percussion | 240.67 | 293.73 | -22.05% |
| 144 | Plucked String (A=Damp B=Body) | 368.76 | 193.68 | **47.48%** |
| 145 | Sigma A=End B=Decay | 284.10 | 85.83 | **69.79%** |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 271.07 | 207.14 | **23.59%** |
| 147 | Rich String Ensemble | 1427.42 | 979.93 | **31.35%** |
| 148 | Mellow Brass Section | 743.02 | 538.69 | **27.50%** |
| 149 | Jittery Inharmonic Pitch | 984.35 | 615.90 | **37.43%** |
| 150 | LFSR Granular Texture | 194.75 | 245.33 | -25.97% |
| 151 | Morphing Harmonics | 1753.97 | 1803.05 | -2.80% |
| 152 | Breathing Pad | 152.84 | 140.62 | **8.00%** |
| 153 | Chaotic Oscillator | 202.04 | 162.73 | **19.46%** |
| 154 | Crystalline Arpeggio | 1171.79 | 1076.74 | **8.11%** |
| 155 | Add: Shepard Cycle | 0.00 | 42.21 | N/A |
| 156 | Water Droplet | 59.48 | 68.77 | -15.63% |
| 157 | Alien Chatter | 47.43 | 88.62 | -86.84% |
| 158 | Weird: Chirp | 0.00 | 23.78 | N/A |
| 159 | Wind AM | 48.82 | 51.26 | -5.00% |
| 160 | LFSR Rhythm Gate | 86.97 | 108.85 | -25.16% |
| 161 | LFSR Harmonic Chaos | 732.79 | 800.66 | -9.26% |
| 162 | LFSR Digital Texture | 221.26 | 188.21 | **14.94%** |
| 163 | LFSR Poly Rhythm | 163.54 | 185.27 | -13.29% |
| 164 | LFSR Phase Modulation | 137.43 | 141.70 | -3.11% |
| 165 | LFSR Granular | 566.36 | 540.49 | **4.57%** |
| 166 | LFSR Rhythmic Harmonics | 774.81 | 831.89 | -7.37% |
| 167 | LFSR Spectral Shift | 150.33 | 201.38 | -33.96% |
| 168 | LFSR Euclidean Beat | 133.28 | 135.43 | -1.61% |
| 169 | LFSR Feedback Synth | 202.74 | 165.13 | **18.55%** |
| 170 | LFSR Algorithmic Lead | 165.89 | 200.57 | -20.91% |
| 171 | LFSR Morphing Pad | 1139.32 | 896.61 | **21.30%** |
| 172 | LFSR Breakbeat | 128.10 | 150.73 | -17.67% |
| 173 | LFSR Probability Gate | 157.53 | 155.53 | **1.27%** |
| 174 | LFSR Polyrhythmic Chaos | 146.10 | 179.49 | -22.85% |
| 175 | LFSR Glitch Matrix | 641.39 | 550.54 | **14.16%** |
| 176 | Pac-Man Wakka | 82.04 | 86.78 | -5.78% |
| 177 | Pac-Man Power Pellet | 83.80 | 89.04 | -6.26% |
| 178 | Pac-Man Death | 86.25 | 88.36 | -2.45% |
| 179 | Pac-Man Ghost | 85.04 | 89.83 | -5.63% |
| 180 | Space Invaders Shot | 101.09 | 102.57 | -1.47% |
| 181 | Space Invaders March | 50.83 | 67.93 | -33.64% |
| 182 | Space Invaders UFO | 97.26 | 105.60 | -8.58% |
| 183 | Space Invaders Explosion | 118.34 | 103.60 | **12.45%** |
| 184 | Asteroids Thrust | 163.12 | 151.57 | **7.08%** |
| 185 | Asteroids Shoot | 102.65 | 108.26 | -5.46% |
| 186 | Asteroids Explosion | 120.19 | 119.39 | **0.67%** |
| 187 | Asteroids Hyperspace | 119.04 | 122.92 | -3.26% |
| 188 | Galaxian Attack | 76.15 | 84.31 | -10.72% |
| 189 | Galaxian Formation | 98.57 | 106.50 | -8.04% |
| 190 | Centipede Laser | 117.71 | 119.12 | -1.20% |
| 191 | Centipede Flea Drop | 90.28 | 92.92 | -2.92% |
| 192 | Defender Thrust | 164.65 | 146.67 | **10.92%** |
| 193 | Defender Smart Bomb | 114.97 | 131.99 | -14.80% |
| 194 | Frogger Hop | 105.31 | 104.15 | **1.11%** |
| 195 | Frogger Traffic | 121.56 | 109.76 | **9.70%** |
| 196 | Donkey Kong Hammer | 129.85 | 106.99 | **17.61%** |
| 197 | Donkey Kong Jump | 94.31 | 93.84 | **0.50%** |
| 198 | Missile Command Explosion | 126.10 | 117.48 | **6.84%** |
| 199 | Tempest Shoot | 106.97 | 114.64 | -7.17% |
| 200 | Tempest Flip | 82.55 | 87.81 | -6.38% |
| 201 | Berzerk Robot Voice | 171.10 | 119.74 | **30.02%** |
| 202 | Robotron Shoot | 115.07 | 119.08 | -3.48% |
| 203 | Phoenix Bird Cry | 101.63 | 106.86 | -5.14% |
| 204 | Gorf Laser | 111.00 | 109.46 | **1.39%** |
| 205 | Scramble Engine | 162.95 | 150.02 | **7.93%** |
| 206 | Zaxxon Alarm | 99.74 | 105.64 | -5.91% |
| 207 | Moon Patrol Bounce | 125.89 | 131.27 | -4.28% |
| 208 | POKEY Pure Tone | 77.09 | 76.09 | **1.30%** |
| 209 | POKEY Filtered Noise | 111.58 | 117.86 | -5.63% |
| 210 | POKEY Distorted Bass | 128.80 | 81.45 | **36.76%** |
| 211 | POKEY Laser Zap | 164.75 | 135.39 | **17.82%** |
| 212 | POKEY Explosion | 0.00 | 114.27 | N/A |
| 213 | POKEY Engine Rumble | 0.00 | 143.47 | N/A |
| 214 | POKEY Bit Crush Lead | 0.00 | 88.32 | N/A |
| 215 | POKEY Coin Pickup | 0.00 | 95.29 | N/A |
| 216 | POKEY Jump Sound | 0.00 | 114.19 | N/A |
| 217 | POKEY Chirp Bird | 0.00 | 137.14 | N/A |
| 218 | POKEY Alien Voice | 0.00 | 152.62 | N/A |
| 219 | POKEY Power Up | 0.00 | 113.99 | N/A |
| 220 | POKEY Hit Sound | 0.00 | 109.89 | N/A |
| 221 | POKEY Sweep Down | 0.00 | 93.39 | N/A |
| 222 | POKEY Poly Counter | 0.00 | 113.87 | N/A |
| 223 | POKEY Four Channel | 0.00 | 137.17 | N/A |
| 224 | POKEY 4-bit Noise (64k) | 0.00 | 86.20 | N/A |
| 225 | POKEY 5-bit Noise (64k) | 0.00 | 86.84 | N/A |
| 226 | POKEY 17-bit Noise (64k) | 0.00 | 84.77 | N/A |
| 227 | POKEY 9-bit Noise (15k) | 0.00 | 81.82 | N/A |
| 228 | POKEY Filtered 4-bit (Fast) | 0.00 | 90.20 | N/A |
| 229 | POKEY Filtered 5-bit (Fast) | 0.00 | 90.44 | N/A |
| 230 | POKEY Tone + 4-bit (64k) | 0.00 | 110.55 | N/A |
| 231 | POKEY Tone + 5-bit (64k) | 0.00 | 112.06 | N/A |
| 232 | POKEY Tone + 17-bit (64k) | 0.00 | 110.95 | N/A |
| 233 | POKEY 4(64k)+5(15k) Combined | 0.00 | 166.66 | N/A |
| 234 | POKEY "High Pass" 4-bit (Fast) | 0.00 | 94.62 | N/A |
| 235 | POKEY 64kHz Noise (17-bit) | 0.00 | 85.69 | N/A |
| 236 | POKEY 15kHz Noise (9-bit) | 0.00 | 82.76 | N/A |
| 237 | POKEY Engine Sound (Noise Gated) | 0.00 | 169.57 | N/A |
| 238 | POKEY Explosion (Decaying Rate/Vol) | 0.00 | 223.83 | N/A |
| 239 | POKEY "Multi-Channel" (Mixed) | 0.00 | 244.23 | N/A |
| 240 | Logic: PWM Hash | 0.00 | 63.81 | N/A |
| 241 | Sample & Hold Sine | 0.00 | 39.89 | N/A |
| 242 | Digital Saw | 0.00 | 27.98 | N/A |
| 243 | Glitch Step | 0.00 | 26.97 | N/A |
| 244 | Weird: Gap | 0.00 | 25.09 | N/A |
| 245 | Noise: White-ish | 0.00 | 15.70 | N/A |
| 246 | Noise: S&H | 0.00 | 152.68 | N/A |
| 247 | Fibonacci Series | 0.00 | 76.88 | N/A |
| 248 | Logistic Chaos | 0.00 | 68.84 | N/A |
| 249 | Chebyshev 4th | 0.00 | 69.63 | N/A |
| 250 | Tanh Fold | 0.00 | 62.22 | N/A |
| 251 | Exp FM | 0.00 | 54.84 | N/A |
| 252 | Chaotic Map | 0.00 | 66.37 | N/A |
| 253 | Pseudo-LPG | 0.00 | 79.07 | N/A |
| 254 | Harmonic Steps | 0.00 | 41.86 | N/A |
| 255 | Vocal Formant 2 | 0.00 | 41.92 | N/A |

## Summary

* **Total Patches Benchmarked:** 256
* **Average Time per Sample (v1.8.10):** 146.71 ns
* **Average Time per Sample (v1.9.0):** 134.68 ns
* **Overall Performance Improvement:** **8.20%**
Freeing LFSR tables...
LFSR tables freed.
Freeing bytecode cache (0 items)... Done. Freed 0 entries.
