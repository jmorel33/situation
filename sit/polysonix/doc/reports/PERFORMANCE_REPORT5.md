# Polysonix VM v1.10.0 (FMA) Performance Report

This report compares the execution time of the new Flat Opcode VM (v1.10.0 (FMA)) against the v1.8.10 baseline.
Both sets of measurements were taken on this cloud environment for direct comparison.

| Patch ID | Name | v1.8.10 (Before) (ns) | v1.10.0 (After) (ns) | Improvement |
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
| 0 | Triangle Up | 59.97 | 49.74 | **17.06%** |
| 1 | Triangle Down | 52.41 | 51.25 | **2.22%** |
| 2 | Sine Up | 153.14 | 95.16 | **37.86%** |
| 3 | Sine Down | 148.86 | 97.31 | **34.63%** |
| 4 | Square Up | 56.45 | 55.58 | **1.54%** |
| 5 | Square Down | 63.85 | 54.49 | **14.66%** |
| 6 | Saw Rising | 73.47 | 71.56 | **2.60%** |
| 7 | Saw Falling | 71.29 | 71.97 | -0.95% |
| 8 | Saw/Sine Up | 134.48 | 87.27 | **35.11%** |
| 9 | Sine/Saw Down | 162.98 | 92.33 | **43.35%** |
| 10 | Square/Sine Up | 109.50 | 79.21 | **27.67%** |
| 11 | Sine/Square Down | 158.11 | 79.48 | **49.73%** |
| 12 | Saw/Triangle Up | 34.33 | 44.10 | -28.46% |
| 13 | Triangle/Saw Down | 46.66 | 37.70 | **19.21%** |
| 14 | Triangle/Sine Up | 72.93 | 68.42 | **6.18%** |
| 15 | Sine/Triangle Down | 76.24 | 71.42 | **6.32%** |
| 16 | Clipped Sine | 55.23 | 48.79 | **11.67%** |
| 17 | Rectified Sine | 49.24 | 48.59 | **1.31%** |
| 18 | Sine * Saw | 92.70 | 53.82 | **41.94%** |
| 19 | Overload Spark | 52.92 | 74.64 | -41.04% |
| 20 | Overfolded Saw | 63.35 | 63.22 | **0.21%** |
| 21 | Clipped Chaos | 67.91 | 73.35 | -8.01% |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 70.84 | 77.60 | -9.54% |
| 23 | Triangle Fold | 0.00 | 41.21 | N/A |
| 24 | Math: Tanh Drive | 0.00 | 33.71 | N/A |
| 25 | Math: Cubic | 0.00 | 32.25 | N/A |
| 26 | Math: Rectified | 0.00 | 24.66 | N/A |
| 27 | Math: Sinc | 0.00 | 27.82 | N/A |
| 28 | Weird: Step-Slope | 0.00 | 21.64 | N/A |
| 29 | Gritty Bass | 68.62 | 54.60 | **20.43%** |
| 30 | Hybrid Saw*Sine | 43.51 | 46.44 | -6.73% |
| 31 | Razor Pulse | 59.42 | 65.50 | -10.23% |
| 32 | Pulse 25% | 47.81 | 47.94 | -0.26% |
| 33 | Pulse 75% | 49.15 | 47.66 | **3.04%** |
| 34 | Staircase 4 Step | 49.12 | 70.41 | -43.34% |
| 35 | Bit Crush Bomb | 52.33 | 80.00 | -52.87% |
| 36 | Bit-Crushed Square | 62.57 | 62.22 | **0.56%** |
| 37 | Pulse Train Wreck | 51.69 | 94.51 | -82.85% |
| 38 | Narrow | 25.11 | 38.94 | -55.06% |
| 39 | Quantized Saw 8 | 37.33 | 37.41 | -0.21% |
| 40 | PWM Synth (A=Width B=Sub) | 58.75 | 62.33 | -6.09% |
| 41 | PWM Gate | 39.91 | 41.45 | -3.86% |
| 42 | Harmonic Switch | 61.72 | 68.51 | -11.00% |
| 43 | Multi-Gate | 51.71 | 67.61 | -30.75% |
| 44 | Bitwise Staircase | 0.00 | 29.71 | N/A |
| 45 | Bitwise XOR Wave | 0.00 | 62.54 | N/A |
| 46 | Hard Quantize Sine | 0.00 | 38.84 | N/A |
| 47 | Comparator Fuzz | 0.00 | 40.26 | N/A |
| 48 | Warp Speed | 22.80 | 33.43 | -46.63% |
| 49 | Ghost Wail | 50.31 | 46.33 | **7.91%** |
| 50 | Laser Malfunction | 63.90 | 60.96 | **4.61%** |
| 51 | Hyperspace Glitch | 24.44 | 58.81 | -140.63% |
| 52 | Shredded Saw | 71.11 | 54.74 | **23.03%** |
| 53 | Glitch Sine | 70.68 | 69.49 | **1.68%** |
| 54 | 4-Segment Bump | 55.12 | 54.57 | **1.01%** |
| 55 | Bird Call AM | 55.31 | 51.36 | **7.14%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 71.14 | 72.51 | -1.92% |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 55.27 | 47.31 | **14.40%** |
| 58 | Phase Glitch | 61.75 | 49.23 | **20.28%** |
| 59 | Phase Distortion Wave | 205.38 | 167.15 | **18.62%** |
| 60 | PD: Resonant | 0.00 | 36.36 | N/A |
| 61 | PD: Wrap | 0.00 | 36.66 | N/A |
| 62 | PD: Spike | 0.00 | 29.36 | N/A |
| 63 | PD: Windowed | 0.00 | 33.95 | N/A |
| 64 | Classic FM EP (A=Index B=Detune) | 87.44 | 76.18 | **12.88%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 127.29 | 79.43 | **37.60%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 60.81 | 60.23 | **0.96%** |
| 67 | Complex FM A=Index B=ModFreq | 58.54 | 56.74 | **3.08%** |
| 68 | FM Pluck | 148.81 | 140.46 | **5.61%** |
| 69 | FM Pitched Grit | 154.93 | 104.05 | **32.84%** |
| 70 | FM Dynamic Lead | 162.73 | 171.76 | -5.55% |
| 71 | FM Glassy Evolve | 106.23 | 99.71 | **6.13%** |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 88.94 | 81.49 | **8.37%** |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 79.52 | 73.55 | **7.51%** |
| 74 | FM: Deep Sub | 0.00 | 32.23 | N/A |
| 75 | FM: Talker | 0.00 | 41.81 | N/A |
| 76 | FM: Feedback Sim | 0.00 | 56.70 | N/A |
| 77 | FM: Cascaded | 0.00 | 62.61 | N/A |
| 78 | FM: Vowel-ish | 0.00 | 38.69 | N/A |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 74.02 | 67.65 | **8.60%** |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 105.94 | 95.53 | **9.83%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 96.89 | 91.75 | **5.30%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 104.61 | 103.01 | **1.53%** |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 126.57 | 115.81 | **8.50%** |
| 84 | FM Bipolar Sweep Pad | 85.96 | 83.81 | **2.50%** |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 148.25 | 99.96 | **32.57%** |
| 86 | FM: Noise | 0.00 | 59.26 | N/A |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 106.28 | 95.46 | **10.18%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 106.45 | 101.30 | **4.84%** |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 101.17 | 90.26 | **10.78%** |
| 90 | FM: Metallic 1 | 0.00 | 40.35 | N/A |
| 91 | FM: Metallic 2 | 0.00 | 40.75 | N/A |
| 92 | Weird: AM Chaos | 0.00 | 33.95 | N/A |
| 93 | Sci-Fi Drone | 246.29 | 192.61 | **21.80%** |
| 94 | Evolving Metallic Bell | 625.29 | 596.35 | **4.63%** |
| 95 | Alien Communication | 204.98 | 249.12 | -21.53% |
| 96 | Sine Harmonics | 84.07 | 73.58 | **12.48%** |
| 97 | Harmonic Noise Blast | 85.80 | 82.80 | **3.49%** |
| 98 | Brass | 123.58 | 112.56 | **8.92%** |
| 99 | Bowed String | 135.04 | 127.25 | **5.77%** |
| 100 | Additive Square | 88.20 | 84.91 | **3.73%** |
| 101 | Electric Pianoish | 83.73 | 80.53 | **3.82%** |
| 102 | Classic Pad | 85.66 | 80.15 | **6.43%** |
| 103 | Additive Saw (A=Harms B=Shape) | 310.54 | 81.37 | **73.80%** |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 277.43 | 124.32 | **55.19%** |
| 105 | Grit Additive (A=Grit B=Tone) | 262.65 | 138.64 | **47.21%** |
| 106 | Simple Minor Triad | 105.75 | 101.63 | **3.90%** |
| 107 | Add: Spec 1 | 0.00 | 64.31 | N/A |
| 108 | Add: Spec 2 | 0.00 | 64.61 | N/A |
| 109 | Add: Bell | 0.00 | 49.21 | N/A |
| 110 | Add: Organ | 0.00 | 64.14 | N/A |
| 111 | Add: Random Phase | 0.00 | 47.09 | N/A |
| 112 | Formantish | 68.63 | 63.20 | **7.90%** |
| 113 | Vocal Ah | 103.10 | 99.55 | **3.44%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 130.23 | 83.86 | **35.61%** |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 472.08 | 523.76 | -10.95% |
| 116 | Sync Sweep No Slant | 43.72 | 42.52 | **2.75%** |
| 117 | Sync Sweep Cos Shape | 61.15 | 54.52 | **10.84%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 121.29 | 68.42 | **43.59%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 60.43 | 57.26 | **5.24%** |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 58.71 | 57.31 | **2.38%** |
| 121 | Oooh Choir Formant | 2302.03 | 1747.56 | **24.09%** |
| 122 | PD Vocal Formant | 0.00 | 28.32 | N/A |
| 123 | Sync Soft | 0.00 | 35.38 | N/A |
| 124 | Fractal Sine | 0.00 | 61.56 | N/A |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 93.22 | 93.97 | -0.80% |
| 126 | Add: Saw 8 | 0.00 | 273.96 | N/A |
| 127 | Add: Square 8 | 0.00 | 286.29 | N/A |
| 128 | Kick Drum | 69.88 | 61.51 | **11.98%** |
| 129 | Snare Drum | 75.89 | 72.01 | **5.11%** |
| 130 | Clap | 94.90 | 88.62 | **6.62%** |
| 131 | Tom Drum | 65.89 | 61.65 | **6.44%** |
| 132 | Cymbalish | 74.89 | 67.17 | **10.31%** |
| 133 | Double Waves | 108.08 | 96.29 | **10.91%** |
| 134 | Metal Impact | 76.78 | 69.11 | **9.99%** |
| 135 | Bell Tone | 87.75 | 76.64 | **12.66%** |
| 136 | Metallic Perc | 72.81 | 67.14 | **7.79%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 300.02 | 276.36 | **7.88%** |
| 138 | Classic Noise Sim | 141.28 | 136.41 | **3.44%** |
| 139 | Distorted Pitch | 129.51 | 119.95 | **7.38%** |
| 140 | Gritty Rumble Noise | 105.03 | 110.84 | -5.53% |
| 141 | Filtered Static Noise | 456.36 | 437.78 | **4.07%** |
| 142 | Wooden Percussion | 86.53 | 77.07 | **10.94%** |
| 143 | Glitchy Percussion | 240.67 | 297.64 | -23.67% |
| 144 | Plucked String (A=Damp B=Body) | 368.76 | 199.99 | **45.77%** |
| 145 | Sigma A=End B=Decay | 284.10 | 86.38 | **69.60%** |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 271.07 | 203.79 | **24.82%** |
| 147 | Rich String Ensemble | 1427.42 | 986.10 | **30.92%** |
| 148 | Mellow Brass Section | 743.02 | 529.01 | **28.80%** |
| 149 | Jittery Inharmonic Pitch | 984.35 | 668.93 | **32.04%** |
| 150 | LFSR Granular Texture | 194.75 | 238.71 | -22.57% |
| 151 | Morphing Harmonics | 1753.97 | 1757.91 | -0.22% |
| 152 | Breathing Pad | 152.84 | 139.19 | **8.93%** |
| 153 | Chaotic Oscillator | 202.04 | 169.52 | **16.09%** |
| 154 | Crystalline Arpeggio | 1171.79 | 1061.17 | **9.44%** |
| 155 | Add: Shepard Cycle | 0.00 | 42.92 | N/A |
| 156 | Water Droplet | 59.48 | 69.29 | -16.49% |
| 157 | Alien Chatter | 47.43 | 82.63 | -74.22% |
| 158 | Weird: Chirp | 0.00 | 23.50 | N/A |
| 159 | Wind AM | 48.82 | 49.47 | -1.33% |
| 160 | LFSR Rhythm Gate | 86.97 | 109.23 | -25.60% |
| 161 | LFSR Harmonic Chaos | 732.79 | 802.99 | -9.58% |
| 162 | LFSR Digital Texture | 221.26 | 204.75 | **7.46%** |
| 163 | LFSR Poly Rhythm | 163.54 | 193.92 | -18.58% |
| 164 | LFSR Phase Modulation | 137.43 | 142.71 | -3.84% |
| 165 | LFSR Granular | 566.36 | 557.86 | **1.50%** |
| 166 | LFSR Rhythmic Harmonics | 774.81 | 874.88 | -12.92% |
| 167 | LFSR Spectral Shift | 150.33 | 203.21 | -35.18% |
| 168 | LFSR Euclidean Beat | 133.28 | 136.91 | -2.72% |
| 169 | LFSR Feedback Synth | 202.74 | 165.44 | **18.40%** |
| 170 | LFSR Algorithmic Lead | 165.89 | 207.24 | -24.93% |
| 171 | LFSR Morphing Pad | 1139.32 | 933.43 | **18.07%** |
| 172 | LFSR Breakbeat | 128.10 | 153.89 | -20.14% |
| 173 | LFSR Probability Gate | 157.53 | 156.20 | **0.85%** |
| 174 | LFSR Polyrhythmic Chaos | 146.10 | 185.90 | -27.24% |
| 175 | LFSR Glitch Matrix | 641.39 | 569.43 | **11.22%** |
| 176 | Pac-Man Wakka | 82.04 | 81.08 | **1.17%** |
| 177 | Pac-Man Power Pellet | 83.80 | 88.09 | -5.12% |
| 178 | Pac-Man Death | 86.25 | 79.27 | **8.09%** |
| 179 | Pac-Man Ghost | 85.04 | 84.61 | **0.51%** |
| 180 | Space Invaders Shot | 101.09 | 99.94 | **1.14%** |
| 181 | Space Invaders March | 50.83 | 68.88 | -35.51% |
| 182 | Space Invaders UFO | 97.26 | 94.86 | **2.47%** |
| 183 | Space Invaders Explosion | 118.34 | 99.39 | **16.01%** |
| 184 | Asteroids Thrust | 163.12 | 147.60 | **9.51%** |
| 185 | Asteroids Shoot | 102.65 | 106.10 | -3.36% |
| 186 | Asteroids Explosion | 120.19 | 117.79 | **1.99%** |
| 187 | Asteroids Hyperspace | 119.04 | 119.52 | -0.40% |
| 188 | Galaxian Attack | 76.15 | 77.93 | -2.34% |
| 189 | Galaxian Formation | 98.57 | 97.57 | **1.01%** |
| 190 | Centipede Laser | 117.71 | 115.95 | **1.49%** |
| 191 | Centipede Flea Drop | 90.28 | 95.22 | -5.47% |
| 192 | Defender Thrust | 164.65 | 140.95 | **14.39%** |
| 193 | Defender Smart Bomb | 114.97 | 114.32 | **0.56%** |
| 194 | Frogger Hop | 105.31 | 100.27 | **4.79%** |
| 195 | Frogger Traffic | 121.56 | 105.60 | **13.13%** |
| 196 | Donkey Kong Hammer | 129.85 | 108.39 | **16.53%** |
| 197 | Donkey Kong Jump | 94.31 | 96.68 | -2.51% |
| 198 | Missile Command Explosion | 126.10 | 118.16 | **6.30%** |
| 199 | Tempest Shoot | 106.97 | 110.07 | -2.90% |
| 200 | Tempest Flip | 82.55 | 80.48 | **2.51%** |
| 201 | Berzerk Robot Voice | 171.10 | 123.00 | **28.11%** |
| 202 | Robotron Shoot | 115.07 | 115.48 | -0.35% |
| 203 | Phoenix Bird Cry | 101.63 | 99.17 | **2.42%** |
| 204 | Gorf Laser | 111.00 | 105.84 | **4.65%** |
| 205 | Scramble Engine | 162.95 | 142.30 | **12.68%** |
| 206 | Zaxxon Alarm | 99.74 | 96.02 | **3.73%** |
| 207 | Moon Patrol Bounce | 125.89 | 118.83 | **5.61%** |
| 208 | POKEY Pure Tone | 77.09 | 78.36 | -1.65% |
| 209 | POKEY Filtered Noise | 111.58 | 120.49 | -7.99% |
| 210 | POKEY Distorted Bass | 128.80 | 68.16 | **47.08%** |
| 211 | POKEY Laser Zap | 164.75 | 127.32 | **22.72%** |
| 212 | POKEY Explosion | 0.00 | 110.84 | N/A |
| 213 | POKEY Engine Rumble | 0.00 | 138.82 | N/A |
| 214 | POKEY Bit Crush Lead | 0.00 | 79.91 | N/A |
| 215 | POKEY Coin Pickup | 0.00 | 90.51 | N/A |
| 216 | POKEY Jump Sound | 0.00 | 109.58 | N/A |
| 217 | POKEY Chirp Bird | 0.00 | 133.68 | N/A |
| 218 | POKEY Alien Voice | 0.00 | 149.94 | N/A |
| 219 | POKEY Power Up | 0.00 | 99.92 | N/A |
| 220 | POKEY Hit Sound | 0.00 | 106.99 | N/A |
| 221 | POKEY Sweep Down | 0.00 | 90.01 | N/A |
| 222 | POKEY Poly Counter | 0.00 | 111.27 | N/A |
| 223 | POKEY Four Channel | 0.00 | 133.36 | N/A |
| 224 | POKEY 4-bit Noise (64k) | 0.00 | 82.28 | N/A |
| 225 | POKEY 5-bit Noise (64k) | 0.00 | 83.11 | N/A |
| 226 | POKEY 17-bit Noise (64k) | 0.00 | 82.10 | N/A |
| 227 | POKEY 9-bit Noise (15k) | 0.00 | 80.16 | N/A |
| 228 | POKEY Filtered 4-bit (Fast) | 0.00 | 88.05 | N/A |
| 229 | POKEY Filtered 5-bit (Fast) | 0.00 | 88.26 | N/A |
| 230 | POKEY Tone + 4-bit (64k) | 0.00 | 106.93 | N/A |
| 231 | POKEY Tone + 5-bit (64k) | 0.00 | 107.79 | N/A |
| 232 | POKEY Tone + 17-bit (64k) | 0.00 | 106.21 | N/A |
| 233 | POKEY 4(64k)+5(15k) Combined | 0.00 | 161.12 | N/A |
| 234 | POKEY "High Pass" 4-bit (Fast) | 0.00 | 92.98 | N/A |
| 235 | POKEY 64kHz Noise (17-bit) | 0.00 | 83.23 | N/A |
| 236 | POKEY 15kHz Noise (9-bit) | 0.00 | 81.43 | N/A |
| 237 | POKEY Engine Sound (Noise Gated) | 0.00 | 166.39 | N/A |
| 238 | POKEY Explosion (Decaying Rate/Vol) | 0.00 | 214.86 | N/A |
| 239 | POKEY "Multi-Channel" (Mixed) | 0.00 | 244.00 | N/A |
| 240 | Logic: PWM Hash | 0.00 | 60.36 | N/A |
| 241 | Sample & Hold Sine | 0.00 | 39.59 | N/A |
| 242 | Digital Saw | 0.00 | 28.54 | N/A |
| 243 | Glitch Step | 0.00 | 25.58 | N/A |
| 244 | Weird: Gap | 0.00 | 25.79 | N/A |
| 245 | Noise: White-ish | 0.00 | 16.79 | N/A |
| 246 | Noise: S&H | 0.00 | 156.26 | N/A |
| 247 | Fibonacci Series | 0.00 | 69.96 | N/A |
| 248 | Logistic Chaos | 0.00 | 64.01 | N/A |
| 249 | Chebyshev 4th | 0.00 | 70.95 | N/A |
| 250 | Tanh Fold | 0.00 | 59.87 | N/A |
| 251 | Exp FM | 0.00 | 52.32 | N/A |
| 252 | Chaotic Map | 0.00 | 59.00 | N/A |
| 253 | Pseudo-LPG | 0.00 | 75.47 | N/A |
| 254 | Harmonic Steps | 0.00 | 40.85 | N/A |
| 255 | Vocal Formant 2 | 0.00 | 39.82 | N/A |

## Summary

* **Total Patches Benchmarked:** 256
* **Average Time per Sample (v1.8.10):** 145.92 ns
* **Average Time per Sample (v1.10.0 (FMA)):** 132.23 ns
* **Overall Performance Improvement:** **9.38%**
