# Polysonix VM Performance Progress Report

This consolidated report tracks the performance evolution of the Polysonix VM across recent updates.

*   **Baseline (v1.8.10):** Iterative sigma, stack-based VM (Cloud x86_64).
*   **v1.9.0 (Flat Ops):** Specialized opcodes for math functions (e.g., `OP_SIN` vs `OP_CALL`).
*   **v1.9.7 (Bytecode):** Latest bytecode interpreter with bounds checking enabled.

 ID | Name | v1.8.10 (ns) | v1.9.0 Flat (ns) | v1.9.7 Native (ns) | Total Gain |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 0 | Triangle Up | 59.97 | 56.89 | 54.46 | **9.19%** |
| 1 | Triangle Down | 52.41 | 56.25 | 54.04 | -3.11% |
| 2 | Sine Up | 153.14 | 152.41 | 94.36 | **38.38%** |
| 3 | Sine Down | 148.86 | 153.19 | 98.80 | **33.63%** |
| 4 | Square Up | 56.45 | 58.12 | 59.26 | -4.98% |
| 5 | Square Down | 63.85 | 58.35 | 59.09 | **7.45%** |
| 6 | Saw Rising | 73.47 | 78.98 | 77.22 | -5.10% |
| 7 | Saw Falling | 71.29 | 79.66 | 78.71 | -10.41% |
| 8 | Saw/Sine Up | 134.48 | 143.60 | 90.21 | **32.92%** |
| 9 | Sine/Saw Down | 162.98 | 146.46 | 93.90 | **42.39%** |
| 10 | Square/Sine Up | 109.50 | 133.32 | 81.97 | **25.14%** |
| 11 | Sine/Square Down | 158.11 | 137.01 | 81.41 | **48.51%** |
| 12 | Saw/Triangle Up | 34.33 | 43.48 | 42.28 | -23.16% |
| 13 | Triangle/Saw Down | 46.66 | 45.37 | 39.01 | **16.40%** |
| 14 | Triangle/Sine Up | 72.93 | 76.14 | 77.25 | -5.92% |
| 15 | Sine/Triangle Down | 76.24 | 75.31 | 68.61 | **10.01%** |
| 16 | Clipped Sine | 55.23 | 56.99 | 51.67 | **6.45%** |
| 17 | Rectified Sine | 49.24 | 57.48 | 51.77 | -5.14% |
| 18 | Sine * Saw | 92.70 | 97.71 | 53.60 | **42.18%** |
| 19 | Overload Spark | 52.92 | 91.33 | 78.49 | -48.32% |
| 20 | Overfolded Saw | 63.35 | 66.34 | 63.88 | -0.84% |
| 21 | Clipped Chaos | 67.91 | 74.47 | 74.59 | -9.84% |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 70.84 | 80.96 | 81.09 | -14.47% |
| 29 | Gritty Bass | 68.62 | 65.89 | 66.70 | **2.80%** |
| 30 | Hybrid Saw*Sine | 43.51 | 44.97 | 45.94 | -5.58% |
| 31 | Razor Pulse | 59.42 | 69.64 | 69.20 | -16.46% |
| 32 | Pulse 25% | 47.81 | 52.17 | 52.00 | -8.76% |
| 33 | Pulse 75% | 49.15 | 53.20 | 56.28 | -14.51% |
| 34 | Staircase 4 Step | 49.12 | 76.94 | 77.50 | -57.78% |
| 35 | Bit Crush Bomb | 52.33 | 80.17 | 79.42 | -51.77% |
| 36 | Bit-Crushed Square | 62.57 | 63.19 | 63.04 | -0.75% |
| 37 | Pulse Train Wreck | 51.69 | 98.64 | 101.01 | -95.41% |
| 38 | Narrow | 25.11 | 41.91 | 42.29 | -68.42% |
| 39 | Quantized Saw 8 | 37.33 | 36.24 | 37.27 | **0.16%** |
| 40 | PWM Synth (A=Width B=Sub) | 58.75 | 63.80 | 64.47 | -9.74% |
| 41 | PWM Gate | 39.91 | 39.00 | 39.90 | **0.03%** |
| 42 | Harmonic Switch | 61.72 | 72.54 | 72.16 | -16.92% |
| 43 | Multi-Gate | 51.71 | 69.27 | 67.37 | -30.28% |
| 48 | Warp Speed | 22.80 | 30.65 | 31.25 | -37.06% |
| 49 | Ghost Wail | 50.31 | 48.84 | 48.81 | **2.98%** |
| 50 | Laser Malfunction | 63.90 | 63.41 | 61.91 | **3.11%** |
| 51 | Hyperspace Glitch | 24.44 | 62.03 | 63.26 | -158.84% |
| 52 | Shredded Saw | 71.11 | 67.99 | 67.02 | **5.75%** |
| 53 | Glitch Sine | 70.68 | 70.36 | 69.47 | **1.71%** |
| 54 | 4-Segment Bump | 55.12 | 56.97 | 56.66 | -2.79% |
| 55 | Bird Call AM | 55.31 | 55.05 | 54.42 | **1.61%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 71.14 | 70.69 | 70.80 | **0.48%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 55.27 | 51.74 | 52.15 | **5.65%** |
| 58 | Phase Glitch | 61.75 | 58.07 | 58.78 | **4.81%** |
| 59 | Phase Distortion Wave | 205.38 | 213.14 | 158.06 | **23.04%** |
| 64 | Classic FM EP (A=Index B=Detune) | 87.44 | 80.34 | 79.42 | **9.17%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 127.29 | 126.39 | 80.43 | **36.81%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 60.81 | 64.48 | 65.22 | -7.25% |
| 67 | Complex FM A=Index B=ModFreq | 58.54 | 57.40 | 57.64 | **1.54%** |
| 68 | FM Pluck | 148.81 | 144.77 | 145.68 | **2.10%** |
| 69 | FM Pitched Grit | 154.93 | 159.35 | 107.36 | **30.70%** |
| 70 | FM Dynamic Lead | 162.73 | 183.04 | 182.83 | -12.35% |
| 71 | FM Glassy Evolve | 106.23 | 108.96 | 107.63 | -1.32% |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 88.94 | 90.75 | 91.85 | -3.27% |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 79.52 | 80.82 | 82.03 | -3.16% |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 74.02 | 78.61 | 77.79 | -5.09% |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 105.94 | 101.52 | 101.67 | **4.03%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 96.89 | 97.53 | 96.30 | **0.61%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 104.61 | 113.70 | 110.93 | -6.04% |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 126.57 | 131.01 | 129.56 | -2.36% |
| 84 | FM Bipolar Sweep Pad | 85.96 | 88.43 | 88.39 | -2.83% |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 148.25 | 150.55 | 103.06 | **30.48%** |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 106.28 | 99.43 | 97.83 | **7.95%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 106.45 | 114.02 | 112.13 | -5.34% |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 101.17 | 102.12 | 103.91 | -2.71% |
| 93 | Sci-Fi Drone | 246.29 | 262.30 | 209.09 | **15.10%** |
| 94 | Evolving Metallic Bell | 625.29 | 585.59 | 585.61 | **6.35%** |
| 95 | Alien Communication | 204.98 | 245.24 | 248.75 | -21.35% |
| 96 | Sine Harmonics | 84.07 | 79.60 | 80.77 | **3.93%** |
| 97 | Harmonic Noise Blast | 85.80 | 86.42 | 86.52 | -0.84% |
| 98 | Brass | 123.58 | 122.17 | 121.47 | **1.71%** |
| 99 | Bowed String | 135.04 | 127.16 | 127.51 | **5.58%** |
| 100 | Additive Square | 88.20 | 87.07 | 87.67 | **0.60%** |
| 101 | Electric Pianoish | 83.73 | 84.13 | 85.16 | -1.71% |
| 102 | Classic Pad | 85.66 | 82.72 | 84.01 | **1.93%** |
| 103 | Additive Saw (A=Harms B=Shape) | 310.54 | 78.00 | 86.95 | **72.00%** |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 277.43 | 123.53 | 132.72 | **52.16%** |
| 105 | Grit Additive (A=Grit B=Tone) | 262.65 | 187.57 | 142.43 | **45.77%** |
| 106 | Simple Minor Triad | 105.75 | 107.73 | 108.99 | -3.06% |
| 112 | Formantish | 68.63 | 64.37 | 64.27 | **6.35%** |
| 113 | Vocal Ah | 103.10 | 102.12 | 100.72 | **2.31%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 130.23 | 140.98 | 87.57 | **32.76%** |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 472.08 | 469.95 | 457.33 | **3.12%** |
| 116 | Sync Sweep No Slant | 43.72 | 49.17 | 47.36 | -8.33% |
| 117 | Sync Sweep Cos Shape | 61.15 | 60.16 | 58.84 | **3.78%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 121.29 | 132.56 | 72.23 | **40.45%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 60.43 | 63.10 | 62.58 | -3.56% |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 58.71 | 64.65 | 62.53 | -6.51% |
| 121 | Oooh Choir Formant | 2302.03 | 1848.68 | 1803.25 | **21.67%** |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 93.22 | 89.24 | 88.74 | **4.81%** |
| 128 | Kick Drum | 69.88 | 69.83 | 68.27 | **2.30%** |
| 129 | Snare Drum | 75.89 | 72.57 | 71.08 | **6.34%** |
| 130 | Clap | 94.90 | 93.00 | 89.69 | **5.49%** |
| 131 | Tom Drum | 65.89 | 67.99 | 69.09 | -4.86% |
| 132 | Cymbalish | 74.89 | 70.14 | 69.36 | **7.38%** |
| 133 | Double Waves | 108.08 | 110.82 | 108.30 | -0.20% |
| 134 | Metal Impact | 76.78 | 71.49 | 70.83 | **7.75%** |
| 135 | Bell Tone | 87.75 | 86.39 | 83.70 | **4.62%** |
| 136 | Metallic Perc | 72.81 | 68.51 | 67.98 | **6.63%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 300.02 | 294.22 | 294.96 | **1.69%** |
| 138 | Classic Noise Sim | 141.28 | 140.69 | 144.74 | -2.45% |
| 139 | Distorted Pitch | 129.51 | 127.29 | 126.93 | **1.99%** |
| 140 | Gritty Rumble Noise | 105.03 | 114.37 | 107.26 | -2.12% |
| 141 | Filtered Static Noise | 456.36 | 430.42 | 433.08 | **5.10%** |
| 142 | Wooden Percussion | 86.53 | 92.27 | 88.36 | -2.11% |
| 143 | Glitchy Percussion | 240.67 | 289.42 | 293.73 | -22.05% |
| 144 | Plucked String (A=Damp B=Body) | 368.76 | 195.13 | 193.68 | **47.48%** |
| 145 | Sigma A=End B=Decay | 284.10 | 84.00 | 85.83 | **69.79%** |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 271.07 | 271.18 | 207.14 | **23.58%** |
| 147 | Rich String Ensemble | 1427.42 | 1006.99 | 979.93 | **31.35%** |
| 148 | Mellow Brass Section | 743.02 | 580.04 | 538.69 | **27.50%** |
| 149 | Jittery Inharmonic Pitch | 984.35 | 607.73 | 615.90 | **37.43%** |
| 150 | LFSR Granular Texture | 194.75 | 239.22 | 245.33 | -25.97% |
| 151 | Morphing Harmonics | 1753.97 | 1982.06 | 1803.05 | -2.80% |
| 152 | Breathing Pad | 152.84 | 188.94 | 140.62 | **8.00%** |
| 153 | Chaotic Oscillator | 202.04 | 244.65 | 162.73 | **19.46%** |
| 154 | Crystalline Arpeggio | 1171.79 | 1386.62 | 1076.74 | **8.11%** |
| 156 | Water Droplet | 59.48 | 70.20 | 68.77 | -15.62% |
| 157 | Alien Chatter | 47.43 | 89.46 | 88.62 | -86.84% |
| 159 | Wind AM | 48.82 | 56.23 | 51.26 | -5.00% |
| 160 | LFSR Rhythm Gate | 86.97 | 111.52 | 108.85 | -25.16% |
| 161 | LFSR Harmonic Chaos | 732.79 | 1026.60 | 800.66 | -9.26% |
| 162 | LFSR Digital Texture | 221.26 | 309.86 | 188.21 | **14.94%** |
| 163 | LFSR Poly Rhythm | 163.54 | 219.26 | 185.27 | -13.29% |
| 164 | LFSR Phase Modulation | 137.43 | 160.41 | 141.70 | -3.11% |
| 165 | LFSR Granular | 566.36 | 605.04 | 540.49 | **4.57%** |
| 166 | LFSR Rhythmic Harmonics | 774.81 | 830.76 | 831.89 | -7.37% |
| 167 | LFSR Spectral Shift | 150.33 | 204.80 | 201.38 | -33.96% |
| 168 | LFSR Euclidean Beat | 133.28 | 136.18 | 135.43 | -1.61% |
| 169 | LFSR Feedback Synth | 202.74 | 214.50 | 165.13 | **18.55%** |
| 170 | LFSR Algorithmic Lead | 165.89 | 207.28 | 200.57 | -20.91% |
| 171 | LFSR Morphing Pad | 1139.32 | 847.09 | 896.61 | **21.30%** |
| 172 | LFSR Breakbeat | 128.10 | 148.72 | 150.73 | -17.67% |
| 173 | LFSR Probability Gate | 157.53 | 153.90 | 155.53 | **1.27%** |
| 174 | LFSR Polyrhythmic Chaos | 146.10 | 181.14 | 179.49 | -22.85% |
| 175 | LFSR Glitch Matrix | 641.39 | 558.10 | 550.54 | **14.16%** |
| 176 | Pac-Man Wakka | 82.04 | 87.35 | 86.78 | -5.78% |
| 177 | Pac-Man Power Pellet | 83.80 | 89.44 | 89.04 | -6.25% |
| 178 | Pac-Man Death | 86.25 | 86.93 | 88.36 | -2.45% |
| 179 | Pac-Man Ghost | 85.04 | 92.10 | 89.83 | -5.63% |
| 180 | Space Invaders Shot | 101.09 | 104.87 | 102.57 | -1.46% |
| 181 | Space Invaders March | 50.83 | 67.56 | 67.93 | -33.64% |
| 182 | Space Invaders UFO | 97.26 | 104.43 | 105.60 | -8.57% |
| 183 | Space Invaders Explosion | 118.34 | 104.71 | 103.60 | **12.46%** |
| 184 | Asteroids Thrust | 163.12 | 159.33 | 151.57 | **7.08%** |
| 185 | Asteroids Shoot | 102.65 | 112.51 | 108.26 | -5.47% |
| 186 | Asteroids Explosion | 120.19 | 121.41 | 119.39 | **0.67%** |
| 187 | Asteroids Hyperspace | 119.04 | 127.42 | 122.92 | -3.26% |
| 188 | Galaxian Attack | 76.15 | 85.69 | 84.31 | -10.72% |
| 189 | Galaxian Formation | 98.57 | 107.02 | 106.50 | -8.05% |
| 190 | Centipede Laser | 117.71 | 119.14 | 119.12 | -1.20% |
| 191 | Centipede Flea Drop | 90.28 | 93.43 | 92.92 | -2.92% |
| 192 | Defender Thrust | 164.65 | 153.02 | 146.67 | **10.92%** |
| 193 | Defender Smart Bomb | 114.97 | 134.47 | 131.99 | -14.80% |
| 194 | Frogger Hop | 105.31 | 104.17 | 104.15 | **1.10%** |
| 195 | Frogger Traffic | 121.56 | 112.06 | 109.76 | **9.71%** |
| 196 | Donkey Kong Hammer | 129.85 | 108.13 | 106.99 | **17.60%** |
| 197 | Donkey Kong Jump | 94.31 | 93.44 | 93.84 | **0.50%** |
| 198 | Missile Command Explosion | 126.10 | 119.78 | 117.48 | **6.84%** |
| 199 | Tempest Shoot | 106.97 | 111.57 | 114.64 | -7.17% |
| 200 | Tempest Flip | 82.55 | 87.79 | 87.81 | -6.37% |
| 201 | Berzerk Robot Voice | 171.10 | 172.86 | 119.74 | **30.02%** |
| 202 | Robotron Shoot | 115.07 | 120.64 | 119.08 | -3.48% |
| 203 | Phoenix Bird Cry | 101.63 | 104.46 | 106.86 | -5.15% |
| 204 | Gorf Laser | 111.00 | 107.72 | 109.46 | **1.39%** |
| 205 | Scramble Engine | 162.95 | 157.51 | 150.02 | **7.93%** |
| 206 | Zaxxon Alarm | 99.74 | 106.30 | 105.64 | -5.92% |
| 207 | Moon Patrol Bounce | 125.89 | 136.01 | 131.27 | -4.27% |
| 208 | POKEY Pure Tone | 77.09 | 77.04 | 76.09 | **1.30%** |
| 209 | POKEY Filtered Noise | 111.58 | 118.21 | 117.86 | -5.63% |
| 210 | POKEY Distorted Bass | 128.80 | 135.20 | 81.45 | **36.76%** |
| 211 | POKEY Laser Zap | 164.75 | 149.36 | 135.39 | **17.82%** |

## Summary

* **Patches Tracked:** 177
* **Average Baseline:** 176.89 ns
* **Average Flat Ops:** 172.34 ns
* **Average v1.9.7:** 159.49 ns
* **Overall Improvement:** **9.84%**
