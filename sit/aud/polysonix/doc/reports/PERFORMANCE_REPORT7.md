# Polysonix VM Performance Report 7

This report compares the execution time of the new VM against the v1.9.26 baseline.
Measurements are averaged over 10 runs to eliminate OS and cache noise.

| Patch ID | Name | v1.9.26 (Before) (ns) | Current (After) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| 0 | Triangle Up | 51.87 | 49.12 | **5.30%** |
| 1 | Triangle Down | 51.80 | 49.92 | **3.63%** |
| 2 | Sine Up | 98.58 | 94.72 | **3.92%** |
| 3 | Sine Down | 100.97 | 97.88 | **3.07%** |
| 4 | Square Up | 55.16 | 53.96 | **2.18%** |
| 5 | Square Down | 55.94 | 54.61 | **2.38%** |
| 6 | Saw Rising | 73.80 | 72.11 | **2.28%** |
| 7 | Saw Falling | 73.12 | 71.58 | **2.11%** |
| 8 | Saw/Sine Up | 88.98 | 86.44 | **2.85%** |
| 9 | Sine/Saw Down | 95.53 | 91.57 | **4.15%** |
| 10 | Square/Sine Up | 80.64 | 77.69 | **3.66%** |
| 11 | Sine/Square Down | 82.12 | 78.38 | **4.55%** |
| 12 | Saw/Triangle Up | 40.80 | 39.80 | **2.46%** |
| 13 | Triangle/Saw Down | 38.36 | 36.87 | **3.88%** |
| 14 | Triangle/Sine Up | 69.55 | 66.72 | **4.07%** |
| 15 | Sine/Triangle Down | 70.30 | 67.01 | **4.68%** |
| 16 | Clipped Sine | 49.95 | 48.74 | **2.41%** |
| 17 | Rectified Sine | 49.92 | 48.05 | **3.75%** |
| 18 | Sine * Saw | 55.55 | 54.51 | **1.87%** |
| 19 | Overload Spark | 72.77 | 71.13 | **2.25%** |
| 20 | Overfolded Saw | 65.45 | 62.69 | **4.22%** |
| 21 | Clipped Chaos | 74.78 | 71.37 | **4.57%** |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 78.38 | 76.28 | **2.68%** |
| 23 | Triangle Fold | 42.39 | 41.48 | **2.13%** |
| 24 | Math: Tanh Drive | 34.47 | 33.53 | **2.71%** |
| 25 | Math: Cubic | 33.03 | 32.11 | **2.80%** |
| 26 | Math: Rectified | 25.42 | 24.79 | **2.50%** |
| 27 | Math: Sinc | 27.18 | 25.90 | **4.71%** |
| 28 | Weird: Step-Slope | 22.25 | 21.59 | **2.99%** |
| 29 | Gritty Bass | 55.52 | 53.94 | **2.85%** |
| 30 | Hybrid Saw*Sine | 46.45 | 45.63 | **1.77%** |
| 31 | Razor Pulse | 66.25 | 64.54 | **2.58%** |
| 32 | Pulse 25% | 49.42 | 47.60 | **3.68%** |
| 33 | Pulse 75% | 49.11 | 47.16 | **3.96%** |
| 34 | Staircase 4 Step | 71.61 | 69.41 | **3.08%** |
| 35 | Bit Crush Bomb | 81.97 | 79.56 | **2.93%** |
| 36 | Bit-Crushed Square | 64.69 | 62.13 | **3.95%** |
| 37 | Pulse Train Wreck | 98.98 | 95.90 | **3.12%** |
| 38 | Narrow | 39.69 | 38.11 | **3.99%** |
| 39 | Quantized Saw 8 | 37.95 | 36.20 | **4.60%** |
| 40 | PWM Synth (A=Width B=Sub) | 65.60 | 62.37 | **4.93%** |
| 41 | PWM Gate | 40.00 | 38.05 | **4.89%** |
| 42 | Harmonic Switch | 66.28 | 64.36 | **2.90%** |
| 43 | Multi-Gate | 69.61 | 67.60 | **2.88%** |
| 44 | Bitwise Staircase | 29.73 | 28.33 | **4.71%** |
| 45 | Bitwise XOR Wave | 65.27 | 62.45 | **4.31%** |
| 46 | Hard Quantize Sine | 39.33 | 37.97 | **3.47%** |
| 47 | Comparator Fuzz | 42.12 | 40.70 | **3.36%** |
| 48 | Warp Speed | 31.66 | 30.65 | **3.21%** |
| 49 | Ghost Wail | 49.54 | 46.78 | **5.56%** |
| 50 | Laser Malfunction | 60.38 | 58.98 | **2.31%** |
| 51 | Hyperspace Glitch | 60.45 | 58.55 | **3.14%** |
| 52 | Shredded Saw | 55.05 | 54.58 | **0.85%** |
| 53 | Glitch Sine | 70.02 | 67.78 | **3.20%** |
| 54 | 4-Segment Bump | 56.41 | 54.97 | **2.55%** |
| 55 | Bird Call AM | 49.37 | 48.34 | **2.10%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 69.13 | 65.77 | **4.86%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 48.74 | 47.11 | **3.34%** |
| 58 | Phase Glitch | 51.52 | 49.83 | **3.28%** |
| 59 | Phase Distortion Wave | 172.40 | 168.00 | **2.55%** |
| 60 | PD: Resonant | 37.66 | 36.38 | **3.39%** |
| 61 | PD: Wrap | 38.11 | 37.02 | **2.86%** |
| 62 | PD: Spike | 30.99 | 30.05 | **3.02%** |
| 63 | PD: Windowed | 35.56 | 34.31 | **3.52%** |
| 64 | Classic FM EP (A=Index B=Detune) | 78.60 | 76.37 | **2.84%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 80.62 | 77.94 | **3.33%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 61.50 | 59.98 | **2.48%** |
| 67 | Complex FM A=Index B=ModFreq | 55.14 | 51.69 | **6.26%** |
| 68 | FM Pluck | 145.69 | 141.39 | **2.95%** |
| 69 | FM Pitched Grit | 107.05 | 104.63 | **2.26%** |
| 70 | FM Dynamic Lead | 174.62 | 172.84 | **1.02%** |
| 71 | FM Glassy Evolve | 103.62 | 101.64 | **1.91%** |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 83.15 | 81.69 | **1.75%** |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 76.00 | 73.81 | **2.88%** |
| 74 | FM: Deep Sub | 33.04 | 32.37 | **2.03%** |
| 75 | FM: Talker | 41.94 | 41.25 | **1.66%** |
| 76 | FM: Feedback Sim | 58.19 | 57.43 | **1.31%** |
| 77 | FM: Cascaded | 65.06 | 62.61 | **3.77%** |
| 78 | FM: Vowel-ish | 41.42 | 39.92 | **3.61%** |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 70.42 | 68.15 | **3.23%** |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 97.37 | 92.33 | **5.18%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 90.80 | 88.13 | **2.94%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 104.19 | 101.23 | **2.84%** |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 120.87 | 116.03 | **4.01%** |
| 84 | FM Bipolar Sweep Pad | 85.16 | 83.62 | **1.81%** |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 103.43 | 100.47 | **2.86%** |
| 86 | FM: Noise | 60.28 | 58.86 | **2.36%** |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 99.06 | 96.12 | **2.97%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 107.12 | 103.87 | **3.03%** |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 93.85 | 92.35 | **1.59%** |
| 90 | FM: Metallic 1 | 40.62 | 40.39 | **0.58%** |
| 91 | FM: Metallic 2 | 41.17 | 40.75 | **1.03%** |
| 92 | Weird: AM Chaos | 34.79 | 34.33 | **1.32%** |
| 93 | Sci-Fi Drone | 196.00 | 192.88 | **1.59%** |
| 94 | Evolving Metallic Bell | 596.98 | 595.18 | **0.30%** |
| 95 | Alien Communication | 247.47 | 245.44 | **0.82%** |
| 96 | Sine Harmonics | 74.94 | 74.20 | **0.99%** |
| 97 | Harmonic Noise Blast | 84.03 | 82.23 | **2.14%** |
| 98 | Brass | 116.03 | 115.87 | **0.14%** |
| 99 | Bowed String | 124.12 | 122.79 | **1.07%** |
| 100 | Additive Square | 84.13 | 83.28 | **1.00%** |
| 101 | Electric Pianoish | 80.73 | 79.00 | **2.14%** |
| 102 | Classic Pad | 80.26 | 78.64 | **2.02%** |
| 103 | Additive Saw (A=Harms B=Shape) | 83.52 | 82.17 | **1.62%** |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 124.34 | 122.29 | **1.64%** |
| 105 | Grit Additive (A=Grit B=Tone) | 137.99 | 133.88 | **2.98%** |
| 106 | Simple Minor Triad | 102.08 | 100.12 | **1.92%** |
| 107 | Add: Spec 1 | 62.96 | 61.47 | **2.37%** |
| 108 | Add: Spec 2 | 63.66 | 62.02 | **2.58%** |
| 109 | Add: Bell | 47.78 | 46.95 | **1.75%** |
| 110 | Add: Organ | 63.73 | 62.18 | **2.43%** |
| 111 | Add: Random Phase | 47.59 | 46.40 | **2.50%** |
| 112 | Formantish | 63.20 | 62.08 | **1.77%** |
| 113 | Vocal Ah | 101.27 | 99.65 | **1.60%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 86.19 | 83.95 | **2.59%** |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 457.27 | 452.50 | **1.04%** |
| 116 | Sync Sweep No Slant | 45.08 | 43.58 | **3.34%** |
| 117 | Sync Sweep Cos Shape | 56.06 | 53.88 | **3.89%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 71.57 | 67.56 | **5.61%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 58.84 | 55.92 | **4.96%** |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 58.84 | 55.80 | **5.18%** |
| 121 | Oooh Choir Formant | 1782.71 | 1766.28 | **0.92%** |
| 122 | PD Vocal Formant | 28.75 | 28.38 | **1.30%** |
| 123 | Sync Soft | 35.86 | 35.42 | **1.23%** |
| 124 | Fractal Sine | 64.34 | 62.98 | **2.11%** |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 105.90 | 102.80 | **2.93%** |
| 126 | Add: Saw 8 | 275.33 | 268.90 | **2.34%** |
| 127 | Add: Square 8 | 286.44 | 280.13 | **2.20%** |
| 128 | Kick Drum | 63.05 | 61.94 | **1.76%** |
| 129 | Snare Drum | 74.03 | 71.36 | **3.61%** |
| 130 | Clap | 91.58 | 88.56 | **3.30%** |
| 131 | Tom Drum | 62.85 | 62.04 | **1.29%** |
| 132 | Cymbalish | 68.61 | 67.64 | **1.41%** |
| 133 | Double Waves | 98.64 | 97.72 | **0.93%** |
| 134 | Metal Impact | 70.96 | 69.56 | **1.97%** |
| 135 | Bell Tone | 78.03 | 77.16 | **1.11%** |
| 136 | Metallic Perc | 68.59 | 67.55 | **1.52%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 283.12 | 274.71 | **2.97%** |
| 138 | Classic Noise Sim | 131.42 | 132.46 | -0.79% |
| 139 | Distorted Pitch | 120.93 | 117.86 | **2.54%** |
| 140 | Gritty Rumble Noise | 148.33 | 144.86 | **2.34%** |
| 141 | Filtered Static Noise | 533.17 | 532.75 | **0.08%** |
| 142 | Wooden Percussion | 79.94 | 78.16 | **2.22%** |
| 143 | Glitchy Percussion | 291.31 | 289.13 | **0.75%** |
| 144 | Plucked String (A=Damp B=Body) | 198.47 | 194.98 | **1.76%** |
| 145 | Sigma A=End B=Decay | 88.54 | 86.51 | **2.29%** |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 222.56 | 219.49 | **1.38%** |
| 147 | Rich String Ensemble | 985.25 | 961.76 | **2.38%** |
| 148 | Mellow Brass Section | 553.21 | 533.79 | **3.51%** |
| 149 | Jittery Inharmonic Pitch | 641.31 | 618.60 | **3.54%** |
| 150 | LFSR Granular Texture | 234.53 | 230.63 | **1.66%** |
| 151 | Morphing Harmonics | 1865.82 | 1833.00 | **1.76%** |
| 152 | Breathing Pad | 136.87 | 136.78 | **0.06%** |
| 153 | Chaotic Oscillator | 169.36 | 167.19 | **1.28%** |
| 154 | Crystalline Arpeggio | 1057.40 | 1044.03 | **1.26%** |
| 155 | Add: Shepard Cycle | 43.09 | 42.52 | **1.33%** |
| 156 | Water Droplet | 70.92 | 68.79 | **3.00%** |
| 157 | Alien Chatter | 83.76 | 82.03 | **2.06%** |
| 158 | Weird: Chirp | 23.57 | 23.43 | **0.59%** |
| 159 | Wind AM | 49.72 | 49.25 | **0.94%** |
| 160 | LFSR Rhythm Gate | 103.50 | 101.86 | **1.59%** |
| 161 | LFSR Harmonic Chaos | 809.99 | 799.12 | **1.34%** |
| 162 | LFSR Digital Texture | 207.76 | 205.40 | **1.14%** |
| 163 | LFSR Poly Rhythm | 194.75 | 192.91 | **0.94%** |
| 164 | LFSR Phase Modulation | 136.74 | 134.94 | **1.32%** |
| 165 | LFSR Granular | 551.00 | 546.04 | **0.90%** |
| 166 | LFSR Rhythmic Harmonics | 840.53 | 834.32 | **0.74%** |
| 167 | LFSR Spectral Shift | 204.94 | 201.70 | **1.58%** |
| 168 | LFSR Euclidean Beat | 138.68 | 137.03 | **1.19%** |
| 169 | LFSR Feedback Synth | 162.46 | 160.76 | **1.04%** |
| 170 | LFSR Algorithmic Lead | 211.91 | 209.75 | **1.02%** |
| 171 | LFSR Morphing Pad | 892.98 | 882.96 | **1.12%** |
| 172 | LFSR Breakbeat | 155.78 | 153.93 | **1.19%** |
| 173 | LFSR Probability Gate | 156.20 | 154.04 | **1.38%** |
| 174 | LFSR Polyrhythmic Chaos | 187.30 | 182.84 | **2.38%** |
| 175 | LFSR Glitch Matrix | 578.50 | 567.89 | **1.83%** |
| 176 | Pac-Man Wakka | 81.08 | 80.16 | **1.13%** |
| 177 | Pac-Man Power Pellet | 88.47 | 86.70 | **2.00%** |
| 178 | Pac-Man Death | 81.23 | 78.75 | **3.05%** |
| 179 | Pac-Man Ghost | 83.67 | 81.33 | **2.80%** |
| 180 | Space Invaders Shot | 99.65 | 97.75 | **1.91%** |
| 181 | Space Invaders March | 68.90 | 67.56 | **1.94%** |
| 182 | Space Invaders UFO | 94.34 | 92.67 | **1.76%** |
| 183 | Space Invaders Explosion | 101.03 | 98.00 | **2.99%** |
| 184 | Asteroids Thrust | 144.39 | 142.93 | **1.01%** |
| 185 | Asteroids Shoot | 104.44 | 102.49 | **1.87%** |
| 186 | Asteroids Explosion | 117.66 | 115.31 | **1.99%** |
| 187 | Asteroids Hyperspace | 118.84 | 116.70 | **1.80%** |
| 188 | Galaxian Attack | 78.50 | 77.61 | **1.13%** |
| 189 | Galaxian Formation | 93.50 | 92.37 | **1.21%** |
| 190 | Centipede Laser | 112.42 | 110.64 | **1.58%** |
| 191 | Centipede Flea Drop | 87.91 | 86.88 | **1.18%** |
| 192 | Defender Thrust | 140.08 | 137.75 | **1.67%** |
| 193 | Defender Smart Bomb | 113.78 | 112.88 | **0.79%** |
| 194 | Frogger Hop | 99.25 | 98.19 | **1.07%** |
| 195 | Frogger Traffic | 106.13 | 104.59 | **1.45%** |
| 196 | Donkey Kong Hammer | 102.61 | 100.32 | **2.23%** |
| 197 | Donkey Kong Jump | 90.04 | 87.50 | **2.82%** |
| 198 | Missile Command Explosion | 118.23 | 115.88 | **1.98%** |
| 199 | Tempest Shoot | 109.13 | 107.15 | **1.82%** |
| 200 | Tempest Flip | 84.16 | 81.10 | **3.63%** |
| 201 | Berzerk Robot Voice | 119.52 | 118.87 | **0.54%** |
| 202 | Robotron Shoot | 110.22 | 109.78 | **0.39%** |
| 203 | Phoenix Bird Cry | 95.13 | 94.06 | **1.12%** |
| 204 | Gorf Laser | 102.21 | 101.27 | **0.92%** |
| 205 | Scramble Engine | 141.53 | 139.50 | **1.43%** |
| 206 | Zaxxon Alarm | 97.24 | 96.18 | **1.10%** |
| 207 | Moon Patrol Bounce | 120.03 | 119.07 | **0.80%** |
| 208 | POKEY Pure Tone | 77.42 | 76.32 | **1.41%** |
| 209 | POKEY Filtered Noise | 111.90 | 110.02 | **1.68%** |
| 210 | POKEY Distorted Bass | 62.09 | 61.52 | **0.93%** |
| 211 | POKEY Laser Zap | 126.03 | 123.31 | **2.16%** |
| 212 | POKEY Explosion | 109.63 | 106.82 | **2.56%** |
| 213 | POKEY Engine Rumble | 137.48 | 135.21 | **1.65%** |
| 214 | POKEY Bit Crush Lead | 80.44 | 78.99 | **1.80%** |
| 215 | POKEY Coin Pickup | 91.93 | 91.00 | **1.02%** |
| 216 | POKEY Jump Sound | 104.84 | 103.15 | **1.62%** |
| 217 | POKEY Chirp Bird | 129.84 | 127.55 | **1.77%** |
| 218 | POKEY Alien Voice | 147.01 | 145.42 | **1.08%** |
| 219 | POKEY Power Up | 100.38 | 98.56 | **1.81%** |
| 220 | POKEY Hit Sound | 106.97 | 103.25 | **3.48%** |
| 221 | POKEY Sweep Down | 88.63 | 85.94 | **3.04%** |
| 222 | POKEY Poly Counter | 111.94 | 109.58 | **2.11%** |
| 223 | POKEY Four Channel | 133.66 | 133.02 | **0.48%** |
| 224 | POKEY 4-bit Noise (64k) | 81.98 | 81.34 | **0.79%** |
| 225 | POKEY 5-bit Noise (64k) | 82.74 | 80.40 | **2.83%** |
| 226 | POKEY 17-bit Noise (64k) | 82.02 | 79.52 | **3.05%** |
| 227 | POKEY 9-bit Noise (15k) | 80.14 | 77.94 | **2.75%** |
| 228 | POKEY Filtered 4-bit (Fast) | 89.00 | 87.14 | **2.08%** |
| 229 | POKEY Filtered 5-bit (Fast) | 88.61 | 87.69 | **1.04%** |
| 230 | POKEY Tone + 4-bit (64k) | 109.03 | 104.34 | **4.30%** |
| 231 | POKEY Tone + 5-bit (64k) | 107.65 | 105.16 | **2.32%** |
| 232 | POKEY Tone + 17-bit (64k) | 107.04 | 104.06 | **2.78%** |
| 233 | POKEY 4(64k)+5(15k) Combined | 164.90 | 164.73 | **0.10%** |
| 234 | POKEY "High Pass" 4-bit (Fast) | 93.91 | 90.87 | **3.24%** |
| 235 | POKEY 64kHz Noise (17-bit) | 82.78 | 80.78 | **2.41%** |
| 236 | POKEY 15kHz Noise (9-bit) | 80.25 | 79.18 | **1.34%** |
| 237 | POKEY Engine Sound (Noise Gated) | 167.41 | 165.12 | **1.36%** |
| 238 | POKEY Explosion (Decaying Rate/Vol) | 213.25 | 209.34 | **1.84%** |
| 239 | POKEY "Multi-Channel" (Mixed) | 243.26 | 242.36 | **0.37%** |
| 240 | Logic: PWM Hash | 62.00 | 61.11 | **1.44%** |
| 241 | Sample & Hold Sine | 39.10 | 38.73 | **0.93%** |
| 242 | Digital Saw | 27.12 | 26.78 | **1.25%** |
| 243 | Glitch Step | 25.87 | 25.65 | **0.87%** |
| 244 | Weird: Gap | 25.79 | 25.73 | **0.21%** |
| 245 | Noise: White-ish | 33.12 | 32.45 | **2.01%** |
| 246 | Noise: S&H | 156.42 | 153.74 | **1.71%** |
| 247 | Fibonacci Series | 70.81 | 69.95 | **1.21%** |
| 248 | Logistic Chaos | 65.61 | 64.83 | **1.19%** |
| 249 | Chebyshev 4th | 72.23 | 70.80 | **1.98%** |
| 250 | Tanh Fold | 62.20 | 61.23 | **1.56%** |
| 251 | Exp FM | 52.96 | 52.30 | **1.24%** |
| 252 | Chaotic Map | 61.86 | 60.65 | **1.96%** |
| 253 | Pseudo-LPG | 76.67 | 75.70 | **1.27%** |
| 254 | Harmonic Steps | 40.85 | 40.37 | **1.18%** |
| 255 | Vocal Formant 2 | 40.95 | 40.42 | **1.29%** |

## Summary

* **Total Patches Benchmarked:** 256
* **Averaged Over:** 10 test runs
* **Average Time per Sample (v1.9.26):** 133.36 ns
* **Average Time per Sample (Current):** 130.88 ns
* **Overall Performance Improvement:** **1.87%**
