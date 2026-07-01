# Polysonix VM v1.9.26 Performance Report

This report compares the execution time of the new Iterative Sigma / Stack-Peek VM (v1.9.26) against the v1.9.16 (FMA Peak) baseline.
Measurements are averaged over 10 runs to eliminate OS and cache noise.

| Patch ID | Name | v1.9.16 (Before) (ns) | v1.9.26 (After) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| 0 | Triangle Up | 49.74 | 51.87 | -4.27% |
| 1 | Triangle Down | 51.25 | 51.80 | -1.08% |
| 2 | Sine Up | 95.16 | 98.58 | -3.59% |
| 3 | Sine Down | 97.31 | 100.97 | -3.76% |
| 4 | Square Up | 55.58 | 55.16 | **0.75%** |
| 5 | Square Down | 54.49 | 55.94 | -2.65% |
| 6 | Saw Rising | 71.56 | 73.80 | -3.13% |
| 7 | Saw Falling | 71.97 | 73.12 | -1.60% |
| 8 | Saw/Sine Up | 87.27 | 88.98 | -1.97% |
| 9 | Sine/Saw Down | 92.33 | 95.53 | -3.47% |
| 10 | Square/Sine Up | 79.21 | 80.64 | -1.81% |
| 11 | Sine/Square Down | 79.48 | 82.12 | -3.32% |
| 12 | Saw/Triangle Up | 44.10 | 40.80 | **7.48%** |
| 13 | Triangle/Saw Down | 37.70 | 38.36 | -1.75% |
| 14 | Triangle/Sine Up | 68.42 | 69.55 | -1.64% |
| 15 | Sine/Triangle Down | 71.42 | 70.30 | **1.57%** |
| 16 | Clipped Sine | 48.79 | 49.95 | -2.39% |
| 17 | Rectified Sine | 48.59 | 49.92 | -2.74% |
| 18 | Sine * Saw | 53.82 | 55.55 | -3.21% |
| 19 | Overload Spark | 74.64 | 72.77 | **2.51%** |
| 20 | Overfolded Saw | 63.22 | 65.45 | -3.53% |
| 21 | Clipped Chaos | 73.35 | 74.78 | -1.94% |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 77.60 | 78.38 | -1.01% |
| 23 | Triangle Fold | 41.21 | 42.39 | -2.88% |
| 24 | Math: Tanh Drive | 33.71 | 34.47 | -2.24% |
| 25 | Math: Cubic | 32.25 | 33.03 | -2.40% |
| 26 | Math: Rectified | 24.66 | 25.42 | -3.10% |
| 27 | Math: Sinc | 27.82 | 27.18 | **2.32%** |
| 28 | Weird: Step-Slope | 21.64 | 22.25 | -2.84% |
| 29 | Gritty Bass | 54.60 | 55.52 | -1.68% |
| 30 | Hybrid Saw*Sine | 46.44 | 46.45 | -0.01% |
| 31 | Razor Pulse | 65.50 | 66.25 | -1.14% |
| 32 | Pulse 25% | 47.94 | 49.42 | -3.09% |
| 33 | Pulse 75% | 47.66 | 49.11 | -3.03% |
| 34 | Staircase 4 Step | 70.41 | 71.61 | -1.70% |
| 35 | Bit Crush Bomb | 80.00 | 81.97 | -2.47% |
| 36 | Bit-Crushed Square | 62.22 | 64.69 | -3.96% |
| 37 | Pulse Train Wreck | 94.51 | 98.98 | -4.73% |
| 38 | Narrow | 38.94 | 39.69 | -1.91% |
| 39 | Quantized Saw 8 | 37.41 | 37.95 | -1.43% |
| 40 | PWM Synth (A=Width B=Sub) | 62.33 | 65.60 | -5.25% |
| 41 | PWM Gate | 41.45 | 40.00 | **3.50%** |
| 42 | Harmonic Switch | 68.51 | 66.28 | **3.26%** |
| 43 | Multi-Gate | 67.61 | 69.61 | -2.95% |
| 44 | Bitwise Staircase | 29.71 | 29.73 | -0.07% |
| 45 | Bitwise XOR Wave | 62.54 | 65.27 | -4.36% |
| 46 | Hard Quantize Sine | 38.84 | 39.33 | -1.26% |
| 47 | Comparator Fuzz | 40.26 | 42.12 | -4.62% |
| 48 | Warp Speed | 33.43 | 31.66 | **5.31%** |
| 49 | Ghost Wail | 46.33 | 49.54 | -6.93% |
| 50 | Laser Malfunction | 60.96 | 60.38 | **0.94%** |
| 51 | Hyperspace Glitch | 58.81 | 60.45 | -2.80% |
| 52 | Shredded Saw | 54.74 | 55.05 | -0.56% |
| 53 | Glitch Sine | 69.49 | 70.02 | -0.76% |
| 54 | 4-Segment Bump | 54.57 | 56.41 | -3.36% |
| 55 | Bird Call AM | 51.36 | 49.37 | **3.87%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 72.51 | 69.13 | **4.66%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 47.31 | 48.74 | -3.02% |
| 58 | Phase Glitch | 49.23 | 51.52 | -4.65% |
| 59 | Phase Distortion Wave | 167.15 | 172.40 | -3.14% |
| 60 | PD: Resonant | 36.36 | 37.66 | -3.58% |
| 61 | PD: Wrap | 36.66 | 38.11 | -3.94% |
| 62 | PD: Spike | 29.36 | 30.99 | -5.57% |
| 63 | PD: Windowed | 33.95 | 35.56 | -4.76% |
| 64 | Classic FM EP (A=Index B=Detune) | 76.18 | 78.60 | -3.18% |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 79.43 | 80.62 | -1.49% |
| 66 | Freq Shifter FM (A=Shift B=Index) | 60.23 | 61.50 | -2.10% |
| 67 | Complex FM A=Index B=ModFreq | 56.74 | 55.14 | **2.83%** |
| 68 | FM Pluck | 140.46 | 145.69 | -3.72% |
| 69 | FM Pitched Grit | 104.05 | 107.05 | -2.88% |
| 70 | FM Dynamic Lead | 171.76 | 174.62 | -1.66% |
| 71 | FM Glassy Evolve | 99.71 | 103.62 | -3.93% |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 81.49 | 83.15 | -2.03% |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 73.55 | 76.00 | -3.33% |
| 74 | FM: Deep Sub | 32.23 | 33.04 | -2.51% |
| 75 | FM: Talker | 41.81 | 41.94 | -0.30% |
| 76 | FM: Feedback Sim | 56.70 | 58.19 | -2.62% |
| 77 | FM: Cascaded | 62.61 | 65.06 | -3.92% |
| 78 | FM: Vowel-ish | 38.69 | 41.42 | -7.07% |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 67.65 | 70.42 | -4.10% |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 95.53 | 97.37 | -1.92% |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 91.75 | 90.80 | **1.04%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 103.01 | 104.19 | -1.15% |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 115.81 | 120.87 | -4.36% |
| 84 | FM Bipolar Sweep Pad | 83.81 | 85.16 | -1.60% |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 99.96 | 103.43 | -3.47% |
| 86 | FM: Noise | 59.26 | 60.28 | -1.72% |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 95.46 | 99.06 | -3.78% |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 101.30 | 107.12 | -5.75% |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 90.26 | 93.85 | -3.98% |
| 90 | FM: Metallic 1 | 40.35 | 40.62 | -0.68% |
| 91 | FM: Metallic 2 | 40.75 | 41.17 | -1.03% |
| 92 | Weird: AM Chaos | 33.95 | 34.79 | -2.47% |
| 93 | Sci-Fi Drone | 192.61 | 196.00 | -1.76% |
| 94 | Evolving Metallic Bell | 596.35 | 596.98 | -0.11% |
| 95 | Alien Communication | 249.12 | 247.47 | **0.66%** |
| 96 | Sine Harmonics | 73.58 | 74.94 | -1.85% |
| 97 | Harmonic Noise Blast | 82.80 | 84.03 | -1.49% |
| 98 | Brass | 112.56 | 116.03 | -3.08% |
| 99 | Bowed String | 127.25 | 124.12 | **2.46%** |
| 100 | Additive Square | 84.91 | 84.13 | **0.92%** |
| 101 | Electric Pianoish | 80.53 | 80.73 | -0.25% |
| 102 | Classic Pad | 80.15 | 80.26 | -0.14% |
| 103 | Additive Saw (A=Harms B=Shape) | 81.37 | 83.52 | -2.64% |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 124.32 | 124.34 | -0.02% |
| 105 | Grit Additive (A=Grit B=Tone) | 138.64 | 137.99 | **0.47%** |
| 106 | Simple Minor Triad | 101.63 | 102.08 | -0.44% |
| 107 | Add: Spec 1 | 64.31 | 62.96 | **2.10%** |
| 108 | Add: Spec 2 | 64.61 | 63.66 | **1.46%** |
| 109 | Add: Bell | 49.21 | 47.78 | **2.91%** |
| 110 | Add: Organ | 64.14 | 63.73 | **0.64%** |
| 111 | Add: Random Phase | 47.09 | 47.59 | -1.05% |
| 112 | Formantish | 63.20 | 63.20 | -0.01% |
| 113 | Vocal Ah | 99.55 | 101.27 | -1.72% |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 83.86 | 86.19 | -2.77% |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 523.76 | 457.27 | **12.69%** |
| 116 | Sync Sweep No Slant | 42.52 | 45.08 | -6.02% |
| 117 | Sync Sweep Cos Shape | 54.52 | 56.06 | -2.83% |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 68.42 | 71.57 | -4.60% |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 57.26 | 58.84 | -2.77% |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 57.31 | 58.84 | -2.66% |
| 121 | Oooh Choir Formant | 1747.56 | 1782.71 | -2.01% |
| 122 | PD Vocal Formant | 28.32 | 28.75 | -1.52% |
| 123 | Sync Soft | 35.38 | 35.86 | -1.36% |
| 124 | Fractal Sine | 61.56 | 64.34 | -4.51% |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 93.97 | 105.90 | -12.69% |
| 126 | Add: Saw 8 | 273.96 | 275.33 | -0.50% |
| 127 | Add: Square 8 | 286.29 | 286.44 | -0.05% |
| 128 | Kick Drum | 61.51 | 63.05 | -2.51% |
| 129 | Snare Drum | 72.01 | 74.03 | -2.81% |
| 130 | Clap | 88.62 | 91.58 | -3.34% |
| 131 | Tom Drum | 61.65 | 62.85 | -1.95% |
| 132 | Cymbalish | 67.17 | 68.61 | -2.14% |
| 133 | Double Waves | 96.29 | 98.64 | -2.44% |
| 134 | Metal Impact | 69.11 | 70.96 | -2.68% |
| 135 | Bell Tone | 76.64 | 78.03 | -1.82% |
| 136 | Metallic Perc | 67.14 | 68.59 | -2.17% |
| 137 | Sigma Bell (A=Decay B=Metal) | 276.36 | 283.12 | -2.44% |
| 138 | Classic Noise Sim | 136.41 | 131.42 | **3.66%** |
| 139 | Distorted Pitch | 119.95 | 120.93 | -0.82% |
| 140 | Gritty Rumble Noise | 110.84 | 148.33 | -33.83% |
| 141 | Filtered Static Noise | 437.78 | 533.17 | -21.79% |
| 142 | Wooden Percussion | 77.07 | 79.94 | -3.73% |
| 143 | Glitchy Percussion | 297.64 | 291.31 | **2.13%** |
| 144 | Plucked String (A=Damp B=Body) | 199.99 | 198.47 | **0.76%** |
| 145 | Sigma A=End B=Decay | 86.38 | 88.54 | -2.50% |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 203.79 | 222.56 | -9.21% |
| 147 | Rich String Ensemble | 986.10 | 985.25 | **0.09%** |
| 148 | Mellow Brass Section | 529.01 | 553.21 | -4.57% |
| 149 | Jittery Inharmonic Pitch | 668.93 | 641.31 | **4.13%** |
| 150 | LFSR Granular Texture | 238.71 | 234.53 | **1.75%** |
| 151 | Morphing Harmonics | 1757.91 | 1865.82 | -6.14% |
| 152 | Breathing Pad | 139.19 | 136.87 | **1.67%** |
| 153 | Chaotic Oscillator | 169.52 | 169.36 | **0.10%** |
| 154 | Crystalline Arpeggio | 1061.17 | 1057.40 | **0.36%** |
| 155 | Add: Shepard Cycle | 42.92 | 43.09 | -0.40% |
| 156 | Water Droplet | 69.29 | 70.92 | -2.35% |
| 157 | Alien Chatter | 82.63 | 83.76 | -1.37% |
| 158 | Weird: Chirp | 23.50 | 23.57 | -0.30% |
| 159 | Wind AM | 49.47 | 49.72 | -0.50% |
| 160 | LFSR Rhythm Gate | 109.23 | 103.50 | **5.25%** |
| 161 | LFSR Harmonic Chaos | 802.99 | 809.99 | -0.87% |
| 162 | LFSR Digital Texture | 204.75 | 207.76 | -1.47% |
| 163 | LFSR Poly Rhythm | 193.92 | 194.75 | -0.43% |
| 164 | LFSR Phase Modulation | 142.71 | 136.74 | **4.18%** |
| 165 | LFSR Granular | 557.86 | 551.00 | **1.23%** |
| 166 | LFSR Rhythmic Harmonics | 874.88 | 840.53 | **3.93%** |
| 167 | LFSR Spectral Shift | 203.21 | 204.94 | -0.85% |
| 168 | LFSR Euclidean Beat | 136.91 | 138.68 | -1.29% |
| 169 | LFSR Feedback Synth | 165.44 | 162.46 | **1.80%** |
| 170 | LFSR Algorithmic Lead | 207.24 | 211.91 | -2.25% |
| 171 | LFSR Morphing Pad | 933.43 | 892.98 | **4.33%** |
| 172 | LFSR Breakbeat | 153.89 | 155.78 | -1.22% |
| 173 | LFSR Probability Gate | 156.20 | 156.20 | 0.00% |
| 174 | LFSR Polyrhythmic Chaos | 185.90 | 187.30 | -0.75% |
| 175 | LFSR Glitch Matrix | 569.43 | 578.50 | -1.59% |
| 176 | Pac-Man Wakka | 81.08 | 81.08 | **0.01%** |
| 177 | Pac-Man Power Pellet | 88.09 | 88.47 | -0.43% |
| 178 | Pac-Man Death | 79.27 | 81.23 | -2.47% |
| 179 | Pac-Man Ghost | 84.61 | 83.67 | **1.11%** |
| 180 | Space Invaders Shot | 99.94 | 99.65 | **0.29%** |
| 181 | Space Invaders March | 68.88 | 68.90 | -0.02% |
| 182 | Space Invaders UFO | 94.86 | 94.34 | **0.55%** |
| 183 | Space Invaders Explosion | 99.39 | 101.03 | -1.66% |
| 184 | Asteroids Thrust | 147.60 | 144.39 | **2.17%** |
| 185 | Asteroids Shoot | 106.10 | 104.44 | **1.56%** |
| 186 | Asteroids Explosion | 117.79 | 117.66 | **0.11%** |
| 187 | Asteroids Hyperspace | 119.52 | 118.84 | **0.57%** |
| 188 | Galaxian Attack | 77.93 | 78.50 | -0.73% |
| 189 | Galaxian Formation | 97.57 | 93.50 | **4.17%** |
| 190 | Centipede Laser | 115.95 | 112.42 | **3.04%** |
| 191 | Centipede Flea Drop | 95.22 | 87.91 | **7.68%** |
| 192 | Defender Thrust | 140.95 | 140.08 | **0.62%** |
| 193 | Defender Smart Bomb | 114.32 | 113.78 | **0.48%** |
| 194 | Frogger Hop | 100.27 | 99.25 | **1.02%** |
| 195 | Frogger Traffic | 105.60 | 106.13 | -0.50% |
| 196 | Donkey Kong Hammer | 108.39 | 102.61 | **5.33%** |
| 197 | Donkey Kong Jump | 96.68 | 90.04 | **6.87%** |
| 198 | Missile Command Explosion | 118.16 | 118.23 | -0.06% |
| 199 | Tempest Shoot | 110.07 | 109.13 | **0.85%** |
| 200 | Tempest Flip | 80.48 | 84.16 | -4.57% |
| 201 | Berzerk Robot Voice | 123.00 | 119.52 | **2.83%** |
| 202 | Robotron Shoot | 115.48 | 110.22 | **4.55%** |
| 203 | Phoenix Bird Cry | 99.17 | 95.13 | **4.07%** |
| 204 | Gorf Laser | 105.84 | 102.21 | **3.43%** |
| 205 | Scramble Engine | 142.30 | 141.53 | **0.54%** |
| 206 | Zaxxon Alarm | 96.02 | 97.24 | -1.27% |
| 207 | Moon Patrol Bounce | 118.83 | 120.03 | -1.01% |
| 208 | POKEY Pure Tone | 78.36 | 77.42 | **1.20%** |
| 209 | POKEY Filtered Noise | 120.49 | 111.90 | **7.13%** |
| 210 | POKEY Distorted Bass | 68.16 | 62.09 | **8.91%** |
| 211 | POKEY Laser Zap | 127.32 | 126.03 | **1.02%** |
| 212 | POKEY Explosion | 110.84 | 109.63 | **1.09%** |
| 213 | POKEY Engine Rumble | 138.82 | 137.48 | **0.97%** |
| 214 | POKEY Bit Crush Lead | 79.91 | 80.44 | -0.66% |
| 215 | POKEY Coin Pickup | 90.51 | 91.93 | -1.56% |
| 216 | POKEY Jump Sound | 109.58 | 104.84 | **4.32%** |
| 217 | POKEY Chirp Bird | 133.68 | 129.84 | **2.88%** |
| 218 | POKEY Alien Voice | 149.94 | 147.01 | **1.95%** |
| 219 | POKEY Power Up | 99.92 | 100.38 | -0.47% |
| 220 | POKEY Hit Sound | 106.99 | 106.97 | **0.01%** |
| 221 | POKEY Sweep Down | 90.01 | 88.63 | **1.53%** |
| 222 | POKEY Poly Counter | 111.27 | 111.94 | -0.60% |
| 223 | POKEY Four Channel | 133.36 | 133.66 | -0.22% |
| 224 | POKEY 4-bit Noise (64k) | 82.28 | 81.98 | **0.36%** |
| 225 | POKEY 5-bit Noise (64k) | 83.11 | 82.74 | **0.45%** |
| 226 | POKEY 17-bit Noise (64k) | 82.10 | 82.02 | **0.10%** |
| 227 | POKEY 9-bit Noise (15k) | 80.16 | 80.14 | **0.02%** |
| 228 | POKEY Filtered 4-bit (Fast) | 88.05 | 89.00 | -1.08% |
| 229 | POKEY Filtered 5-bit (Fast) | 88.26 | 88.61 | -0.39% |
| 230 | POKEY Tone + 4-bit (64k) | 106.93 | 109.03 | -1.96% |
| 231 | POKEY Tone + 5-bit (64k) | 107.79 | 107.65 | **0.13%** |
| 232 | POKEY Tone + 17-bit (64k) | 106.21 | 107.04 | -0.78% |
| 233 | POKEY 4(64k)+5(15k) Combined | 161.12 | 164.90 | -2.35% |
| 234 | POKEY "High Pass" 4-bit (Fast) | 92.98 | 93.91 | -1.00% |
| 235 | POKEY 64kHz Noise (17-bit) | 83.23 | 82.78 | **0.54%** |
| 236 | POKEY 15kHz Noise (9-bit) | 81.43 | 80.25 | **1.44%** |
| 237 | POKEY Engine Sound (Noise Gated) | 166.39 | 167.41 | -0.61% |
| 238 | POKEY Explosion (Decaying Rate/Vol) | 214.86 | 213.25 | **0.75%** |
| 239 | POKEY "Multi-Channel" (Mixed) | 244.00 | 243.26 | **0.30%** |
| 240 | Logic: PWM Hash | 60.36 | 62.00 | -2.72% |
| 241 | Sample & Hold Sine | 39.59 | 39.10 | **1.24%** |
| 242 | Digital Saw | 28.54 | 27.12 | **4.98%** |
| 243 | Glitch Step | 25.58 | 25.87 | -1.13% |
| 244 | Weird: Gap | 25.79 | 25.79 | **0.02%** |
| 245 | Noise: White-ish | 16.79 | 33.12 | -97.26% |
| 246 | Noise: S&H | 156.26 | 156.42 | -0.10% |
| 247 | Fibonacci Series | 69.96 | 70.81 | -1.21% |
| 248 | Logistic Chaos | 64.01 | 65.61 | -2.50% |
| 249 | Chebyshev 4th | 70.95 | 72.23 | -1.80% |
| 250 | Tanh Fold | 59.87 | 62.20 | -3.90% |
| 251 | Exp FM | 52.32 | 52.96 | -1.22% |
| 252 | Chaotic Map | 59.00 | 61.86 | -4.86% |
| 253 | Pseudo-LPG | 75.47 | 76.67 | -1.60% |
| 254 | Harmonic Steps | 40.85 | 40.85 | 0.00% |
| 255 | Vocal Formant 2 | 39.82 | 40.95 | -2.84% |

## Summary

* **Total Patches Benchmarked:** 256
* **Averaged Over:** 10 test runs
* **Average Time per Sample (v1.9.16):** 132.23 ns
* **Average Time per Sample (v1.9.26):** 133.36 ns
* **Overall Performance Improvement:** **-0.86%**
