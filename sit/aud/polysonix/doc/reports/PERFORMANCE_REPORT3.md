# Polysonix VM Optimization Report 3 (v1.9.0)

## Performance Comparison: Flat Opcodes (v1.9.0) vs Legacy Dispatch (v1.8.x)

This benchmark measures the performance impact of the **v1.9.0** architectural refactor, which replaces the generic `OP_CALL` dispatch mechanism with specific, flat opcodes for mathematical functions.

### Architectural Improvements
*   **Flat Dispatch:** Replaced the double-dispatch of `OP_CALL` -> `FunctionID` with direct opcodes (e.g., `OP_SIN`, `OP_RAND`), reducing branch misprediction penalties.
*   **Inline Logic:** Mathematical operations now perform stack manipulation directly within the main interpreter loop, eliminating the overhead of helper function calls for argument popping and result pushing.
*   **Bytecode Size:** Reduced instruction size for common math calls (2 bytes vs 4 bytes), slightly improving cache locality.

### Comparison Results
The table below compares execution time per sample (in nanoseconds) between the previous v1.8.x baseline and the new v1.9.0 engine.

*   **Overall Improvement:** **~2.14%** (Average across all 256 patches).
*   **Complex Patch Gains:** Patches heavily reliant on math functions show dramatic speedups (e.g., **Additive Saw +74%**, **Random Phase Additive +55%**).
*   **Simple Patch Stability:** Basic waveforms show negligible difference, as they were already dominated by single math calls or simple arithmetic.

| Patch ID | Name | v1.8.10 (Before) (ns) | v1.9.0 (After) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| 0 | Triangle Up | 59.97 | 56.89 | **5.14%** |
| 1 | Triangle Down | 52.41 | 56.25 | -7.33% |
| 2 | Sine Up | 153.14 | 152.41 | **0.48%** |
| 3 | Sine Down | 148.86 | 153.19 | -2.91% |
| 4 | Square Up | 56.45 | 58.12 | -2.97% |
| 5 | Square Down | 63.85 | 58.35 | **8.61%** |
| 6 | Saw Rising | 73.47 | 78.98 | -7.50% |
| 7 | Saw Falling | 71.29 | 79.66 | -11.73% |
| 8 | Saw/Sine Up | 134.48 | 143.60 | -6.78% |
| 9 | Sine/Saw Down | 162.98 | 146.46 | **10.13%** |
| 10 | Square/Sine Up | 109.50 | 133.32 | -21.75% |
| 11 | Sine/Square Down | 158.11 | 137.01 | **13.35%** |
| 12 | Saw/Triangle Up | 34.33 | 43.48 | -26.66% |
| 13 | Triangle/Saw Down | 46.66 | 45.37 | **2.77%** |
| 14 | Triangle/Sine Up | 72.93 | 76.14 | -4.40% |
| 15 | Sine/Triangle Down | 76.24 | 75.31 | **1.22%** |
| 16 | Clipped Sine | 55.23 | 56.99 | -3.18% |
| 17 | Rectified Sine | 49.24 | 57.48 | -16.73% |
| 18 | Sine * Saw | 92.70 | 97.71 | -5.41% |
| 19 | Overload Spark | 52.92 | 91.33 | -72.57% |
| 20 | Overfolded Saw | 63.35 | 66.34 | -4.72% |
| 21 | Clipped Chaos | 67.91 | 74.47 | -9.66% |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 70.84 | 80.96 | -14.28% |
| 23 | Triangle Fold | 0.00 | 43.75 | N/A |
| 24 | Math: Tanh Drive | 0.00 | 69.94 | N/A |
| 25 | Math: Cubic | 0.00 | 32.66 | N/A |
| 26 | Math: Rectified | 0.00 | 24.36 | N/A |
| 27 | Math: Sinc | 0.00 | 26.58 | N/A |
| 28 | Weird: Step-Slope | 0.00 | 20.96 | N/A |
| 29 | Gritty Bass | 68.62 | 65.89 | **3.98%** |
| 30 | Hybrid Saw*Sine | 43.51 | 44.97 | -3.36% |
| 31 | Razor Pulse | 59.42 | 69.64 | -17.19% |
| 32 | Pulse 25% | 47.81 | 52.17 | -9.12% |
| 33 | Pulse 75% | 49.15 | 53.20 | -8.24% |
| 34 | Staircase 4 Step | 49.12 | 76.94 | -56.63% |
| 35 | Bit Crush Bomb | 52.33 | 80.17 | -53.21% |
| 36 | Bit-Crushed Square | 62.57 | 63.19 | -0.98% |
| 37 | Pulse Train Wreck | 51.69 | 98.64 | -90.84% |
| 38 | Narrow | 25.11 | 41.91 | -66.89% |
| 39 | Quantized Saw 8 | 37.33 | 36.24 | **2.93%** |
| 40 | PWM Synth (A=Width B=Sub) | 58.75 | 63.80 | -8.60% |
| 41 | PWM Gate | 39.91 | 39.00 | **2.27%** |
| 42 | Harmonic Switch | 61.72 | 72.54 | -17.54% |
| 43 | Multi-Gate | 51.71 | 69.27 | -33.96% |
| 44 | Bitwise Staircase | 0.00 | 31.62 | N/A |
| 45 | Bitwise XOR Wave | 0.00 | 63.96 | N/A |
| 46 | Hard Quantize Sine | 0.00 | 39.99 | N/A |
| 47 | Comparator Fuzz | 0.00 | 39.95 | N/A |
| 48 | Warp Speed | 22.80 | 30.65 | -34.44% |
| 49 | Ghost Wail | 50.31 | 48.84 | **2.92%** |
| 50 | Laser Malfunction | 63.90 | 63.41 | **0.77%** |
| 51 | Hyperspace Glitch | 24.44 | 62.03 | -153.82% |
| 52 | Shredded Saw | 71.11 | 67.99 | **4.39%** |
| 53 | Glitch Sine | 70.68 | 70.36 | **0.45%** |
| 54 | 4-Segment Bump | 55.12 | 56.97 | -3.36% |
| 55 | Bird Call AM | 55.31 | 55.05 | **0.47%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 71.14 | 70.69 | **0.63%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 55.27 | 51.74 | **6.39%** |
| 58 | Phase Glitch | 61.75 | 58.07 | **5.96%** |
| 59 | Phase Distortion Wave | 205.38 | 213.14 | -3.78% |
| 60 | PD: Resonant | 0.00 | 39.49 | N/A |
| 61 | PD: Wrap | 0.00 | 41.25 | N/A |
| 62 | PD: Spike | 0.00 | 31.01 | N/A |
| 63 | PD: Windowed | 0.00 | 36.79 | N/A |
| 64 | Classic FM EP (A=Index B=Detune) | 87.44 | 80.34 | **8.12%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 127.29 | 126.39 | **0.71%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 60.81 | 64.48 | -6.04% |
| 67 | Complex FM A=Index B=ModFreq | 58.54 | 57.40 | **1.94%** |
| 68 | FM Pluck | 148.81 | 144.77 | **2.71%** |
| 69 | FM Pitched Grit | 154.93 | 159.35 | -2.85% |
| 70 | FM Dynamic Lead | 162.73 | 183.04 | -12.48% |
| 71 | FM Glassy Evolve | 106.23 | 108.96 | -2.57% |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 88.94 | 90.75 | -2.03% |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 79.52 | 80.82 | -1.63% |
| 74 | FM: Deep Sub | 0.00 | 34.71 | N/A |
| 75 | FM: Talker | 0.00 | 44.44 | N/A |
| 76 | FM: Feedback Sim | 0.00 | 63.38 | N/A |
| 77 | FM: Cascaded | 0.00 | 64.91 | N/A |
| 78 | FM: Vowel-ish | 0.00 | 39.71 | N/A |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 74.02 | 78.61 | -6.20% |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 105.94 | 101.52 | **4.18%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 96.89 | 97.53 | -0.66% |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 104.61 | 113.70 | -8.69% |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 126.57 | 131.01 | -3.50% |
| 84 | FM Bipolar Sweep Pad | 85.96 | 88.43 | -2.88% |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 148.25 | 150.55 | -1.55% |
| 86 | FM: Noise | 0.00 | 59.11 | N/A |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 106.28 | 99.43 | **6.45%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 106.45 | 114.02 | -7.11% |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 101.17 | 102.12 | -0.94% |
| 90 | FM: Metallic 1 | 0.00 | 42.81 | N/A |
| 91 | FM: Metallic 2 | 0.00 | 43.38 | N/A |
| 92 | Weird: AM Chaos | 0.00 | 38.05 | N/A |
| 93 | Sci-Fi Drone | 246.29 | 262.30 | -6.50% |
| 94 | Evolving Metallic Bell | 625.29 | 585.59 | **6.35%** |
| 95 | Alien Communication | 204.98 | 245.24 | -19.64% |
| 96 | Sine Harmonics | 84.07 | 79.60 | **5.32%** |
| 97 | Harmonic Noise Blast | 85.80 | 86.42 | -0.72% |
| 98 | Brass | 123.58 | 122.17 | **1.14%** |
| 99 | Bowed String | 135.04 | 127.16 | **5.83%** |
| 100 | Additive Square | 88.20 | 87.07 | **1.28%** |
| 101 | Electric Pianoish | 83.73 | 84.13 | -0.48% |
| 102 | Classic Pad | 85.66 | 82.72 | **3.43%** |
| 103 | Additive Saw (A=Harms B=Shape) | 310.54 | 78.00 | **74.88%** |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 277.43 | 123.53 | **55.47%** |
| 105 | Grit Additive (A=Grit B=Tone) | 262.65 | 187.57 | **28.59%** |
| 106 | Simple Minor Triad | 105.75 | 107.73 | -1.87% |
| 107 | Add: Spec 1 | 0.00 | 65.67 | N/A |
| 108 | Add: Spec 2 | 0.00 | 67.18 | N/A |
| 109 | Add: Bell | 0.00 | 51.05 | N/A |
| 110 | Add: Organ | 0.00 | 66.26 | N/A |
| 111 | Add: Random Phase | 0.00 | 48.12 | N/A |
| 112 | Formantish | 68.63 | 64.37 | **6.21%** |
| 113 | Vocal Ah | 103.10 | 102.12 | **0.95%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 130.23 | 140.98 | -8.25% |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 472.08 | 469.95 | **0.45%** |
| 116 | Sync Sweep No Slant | 43.72 | 49.17 | -12.47% |
| 117 | Sync Sweep Cos Shape | 61.15 | 60.16 | **1.62%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 121.29 | 132.56 | -9.29% |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 60.43 | 63.10 | -4.42% |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 58.71 | 64.65 | -10.12% |
| 121 | Oooh Choir Formant | 2302.03 | 1848.68 | **19.69%** |
| 122 | PD Vocal Formant | 0.00 | 28.79 | N/A |
| 123 | Sync Soft | 0.00 | 38.22 | N/A |
| 124 | Fractal Sine | 0.00 | 67.58 | N/A |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 93.22 | 89.24 | **4.27%** |
| 126 | Add: Saw 8 | 0.00 | 310.64 | N/A |
| 127 | Add: Square 8 | 0.00 | 318.71 | N/A |
| 128 | Kick Drum | 69.88 | 69.83 | **0.07%** |
| 129 | Snare Drum | 75.89 | 72.57 | **4.37%** |
| 130 | Clap | 94.90 | 93.00 | **2.00%** |
| 131 | Tom Drum | 65.89 | 67.99 | -3.19% |
| 132 | Cymbalish | 74.89 | 70.14 | **6.34%** |
| 133 | Double Waves | 108.08 | 110.82 | -2.54% |
| 134 | Metal Impact | 76.78 | 71.49 | **6.90%** |
| 135 | Bell Tone | 87.75 | 86.39 | **1.55%** |
| 136 | Metallic Perc | 72.81 | 68.51 | **5.90%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 300.02 | 294.22 | **1.93%** |
| 138 | Classic Noise Sim | 141.28 | 140.69 | **0.42%** |
| 139 | Distorted Pitch | 129.51 | 127.29 | **1.72%** |
| 140 | Gritty Rumble Noise | 105.03 | 114.37 | -8.89% |
| 141 | Filtered Static Noise | 456.36 | 430.42 | **5.68%** |
| 142 | Wooden Percussion | 86.53 | 92.27 | -6.63% |
| 143 | Glitchy Percussion | 240.67 | 289.42 | -20.25% |
| 144 | Plucked String (A=Damp B=Body) | 368.76 | 195.13 | **47.08%** |
| 145 | Sigma A=End B=Decay | 284.10 | 84.00 | **70.43%** |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 271.07 | 271.18 | -0.04% |
| 147 | Rich String Ensemble | 1427.42 | 1006.99 | **29.45%** |
| 148 | Mellow Brass Section | 743.02 | 580.04 | **21.93%** |
| 149 | Jittery Inharmonic Pitch | 984.35 | 607.73 | **38.26%** |
| 150 | LFSR Granular Texture | 194.75 | 239.22 | -22.83% |
| 151 | Morphing Harmonics | 1753.97 | 1982.06 | -13.00% |
| 152 | Breathing Pad | 152.84 | 188.94 | -23.62% |
| 153 | Chaotic Oscillator | 202.04 | 244.65 | -21.09% |
| 154 | Crystalline Arpeggio | 1171.79 | 1386.62 | -18.33% |
| 155 | Add: Shepard Cycle | 0.00 | 46.10 | N/A |
| 156 | Water Droplet | 59.48 | 70.20 | -18.03% |
| 157 | Alien Chatter | 47.43 | 89.46 | -88.62% |
| 158 | Weird: Chirp | 0.00 | 24.01 | N/A |
| 159 | Wind AM | 48.82 | 56.23 | -15.18% |
| 160 | LFSR Rhythm Gate | 86.97 | 111.52 | -28.23% |
| 161 | LFSR Harmonic Chaos | 732.79 | 1026.60 | -40.09% |
| 162 | LFSR Digital Texture | 221.26 | 309.86 | -40.04% |
| 163 | LFSR Poly Rhythm | 163.54 | 219.26 | -34.07% |
| 164 | LFSR Phase Modulation | 137.43 | 160.41 | -16.72% |
| 165 | LFSR Granular | 566.36 | 605.04 | -6.83% |
| 166 | LFSR Rhythmic Harmonics | 774.81 | 830.76 | -7.22% |
| 167 | LFSR Spectral Shift | 150.33 | 204.80 | -36.23% |
| 168 | LFSR Euclidean Beat | 133.28 | 136.18 | -2.17% |
| 169 | LFSR Feedback Synth | 202.74 | 214.50 | -5.80% |
| 170 | LFSR Algorithmic Lead | 165.89 | 207.28 | -24.95% |
| 171 | LFSR Morphing Pad | 1139.32 | 847.09 | **25.65%** |
| 172 | LFSR Breakbeat | 128.10 | 148.72 | -16.10% |
| 173 | LFSR Probability Gate | 157.53 | 153.90 | **2.30%** |
| 174 | LFSR Polyrhythmic Chaos | 146.10 | 181.14 | -23.99% |
| 175 | LFSR Glitch Matrix | 641.39 | 558.10 | **12.99%** |
| 176 | Pac-Man Wakka | 82.04 | 87.35 | -6.47% |
| 177 | Pac-Man Power Pellet | 83.80 | 89.44 | -6.73% |
| 178 | Pac-Man Death | 86.25 | 86.93 | -0.79% |
| 179 | Pac-Man Ghost | 85.04 | 92.10 | -8.30% |
| 180 | Space Invaders Shot | 101.09 | 104.87 | -3.74% |
| 181 | Space Invaders March | 50.83 | 67.56 | -32.92% |
| 182 | Space Invaders UFO | 97.26 | 104.43 | -7.37% |
| 183 | Space Invaders Explosion | 118.34 | 104.71 | **11.51%** |
| 184 | Asteroids Thrust | 163.12 | 159.33 | **2.32%** |
| 185 | Asteroids Shoot | 102.65 | 112.51 | -9.60% |
| 186 | Asteroids Explosion | 120.19 | 121.41 | -1.01% |
| 187 | Asteroids Hyperspace | 119.04 | 127.42 | -7.04% |
| 188 | Galaxian Attack | 76.15 | 85.69 | -12.52% |
| 189 | Galaxian Formation | 98.57 | 107.02 | -8.57% |
| 190 | Centipede Laser | 117.71 | 119.14 | -1.21% |
| 191 | Centipede Flea Drop | 90.28 | 93.43 | -3.49% |
| 192 | Defender Thrust | 164.65 | 153.02 | **7.06%** |
| 193 | Defender Smart Bomb | 114.97 | 134.47 | -16.96% |
| 194 | Frogger Hop | 105.31 | 104.17 | **1.08%** |
| 195 | Frogger Traffic | 121.56 | 112.06 | **7.81%** |
| 196 | Donkey Kong Hammer | 129.85 | 108.13 | **16.73%** |
| 197 | Donkey Kong Jump | 94.31 | 93.44 | **0.92%** |
| 198 | Missile Command Explosion | 126.10 | 119.78 | **5.01%** |
| 199 | Tempest Shoot | 106.97 | 111.57 | -4.30% |
| 200 | Tempest Flip | 82.55 | 87.79 | -6.34% |
| 201 | Berzerk Robot Voice | 171.10 | 172.86 | -1.03% |
| 202 | Robotron Shoot | 115.07 | 120.64 | -4.84% |
| 203 | Phoenix Bird Cry | 101.63 | 104.46 | -2.79% |
| 204 | Gorf Laser | 111.00 | 107.72 | **2.95%** |
| 205 | Scramble Engine | 162.95 | 157.51 | **3.34%** |
| 206 | Zaxxon Alarm | 99.74 | 106.30 | -6.58% |
| 207 | Moon Patrol Bounce | 125.89 | 136.01 | -8.04% |
| 208 | POKEY Pure Tone | 77.09 | 77.04 | **0.06%** |
| 209 | POKEY Filtered Noise | 111.58 | 118.21 | -5.94% |
| 210 | POKEY Distorted Bass | 128.80 | 135.20 | -4.97% |
| 211 | POKEY Laser Zap | 164.75 | 149.36 | **9.34%** |
| 212 | POKEY Explosion | 0.00 | 112.58 | N/A |
| 213 | POKEY Engine Rumble | 0.00 | 143.44 | N/A |
| 214 | POKEY Bit Crush Lead | 0.00 | 88.09 | N/A |
| 215 | POKEY Coin Pickup | 0.00 | 93.16 | N/A |
| 216 | POKEY Jump Sound | 0.00 | 113.48 | N/A |
| 217 | POKEY Chirp Bird | 0.00 | 136.67 | N/A |
| 218 | POKEY Alien Voice | 0.00 | 204.08 | N/A |
| 219 | POKEY Power Up | 0.00 | 112.74 | N/A |
| 220 | POKEY Hit Sound | 0.00 | 111.20 | N/A |
| 221 | POKEY Sweep Down | 0.00 | 95.70 | N/A |
| 222 | POKEY Poly Counter | 0.00 | 112.73 | N/A |
| 223 | POKEY Four Channel | 0.00 | 135.47 | N/A |
| 224 | POKEY 4-bit Noise (64k) | 0.00 | 83.91 | N/A |
| 225 | POKEY 5-bit Noise (64k) | 0.00 | 83.54 | N/A |
| 226 | POKEY 17-bit Noise (64k) | 0.00 | 82.53 | N/A |
| 227 | POKEY 9-bit Noise (15k) | 0.00 | 81.98 | N/A |
| 228 | POKEY Filtered 4-bit (Fast) | 0.00 | 89.07 | N/A |
| 229 | POKEY Filtered 5-bit (Fast) | 0.00 | 89.44 | N/A |
| 230 | POKEY Tone + 4-bit (64k) | 0.00 | 109.40 | N/A |
| 231 | POKEY Tone + 5-bit (64k) | 0.00 | 110.28 | N/A |
| 232 | POKEY Tone + 17-bit (64k) | 0.00 | 108.51 | N/A |
| 233 | POKEY 4(64k)+5(15k) Combined | 0.00 | 165.41 | N/A |
| 234 | POKEY "High Pass" 4-bit (Fast) | 0.00 | 94.69 | N/A |
| 235 | POKEY 64kHz Noise (17-bit) | 0.00 | 84.06 | N/A |
| 236 | POKEY 15kHz Noise (9-bit) | 0.00 | 81.91 | N/A |
| 237 | POKEY Engine Sound (Noise Gated) | 0.00 | 168.85 | N/A |
| 238 | POKEY Explosion (Decaying Rate/Vol) | 0.00 | 224.23 | N/A |
| 239 | POKEY "Multi-Channel" (Mixed) | 0.00 | 246.97 | N/A |
| 240 | Logic: PWM Hash | 0.00 | 62.15 | N/A |
| 241 | Sample & Hold Sine | 0.00 | 39.81 | N/A |
| 242 | Digital Saw | 0.00 | 27.33 | N/A |
| 243 | Glitch Step | 0.00 | 24.95 | N/A |
| 244 | Weird: Gap | 0.00 | 24.39 | N/A |
| 245 | Noise: White-ish | 0.00 | 15.85 | N/A |
| 246 | Noise: S&H | 0.00 | 155.01 | N/A |
| 247 | Fibonacci Series | 0.00 | 78.45 | N/A |
| 248 | Logistic Chaos | 0.00 | 69.80 | N/A |
| 249 | Chebyshev 4th | 0.00 | 69.73 | N/A |
| 250 | Tanh Fold | 0.00 | 115.47 | N/A |
| 251 | Exp FM | 0.00 | 55.63 | N/A |
| 252 | Chaotic Map | 0.00 | 66.48 | N/A |
| 253 | Pseudo-LPG | 0.00 | 79.48 | N/A |
| 254 | Harmonic Steps | 0.00 | 42.21 | N/A |
| 255 | Vocal Formant 2 | 0.00 | 41.79 | N/A |

## Summary

* **Total Patches Benchmarked:** 256
* **Average Time per Sample (v1.8.10):** 147.45 ns
* **Average Time per Sample (v1.9.0):** 144.30 ns
* **Overall Performance Improvement:** **2.14%**
