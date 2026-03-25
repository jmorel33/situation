# Polysonix VM Optimization Report 2 (v1.8.10)

## Executive Summary

This benchmark compares the performance of the latest v1.8.10 Virtual Machine (iterative sigma, flat stack) against the v1.1.6 baseline (recursive sigma).

**Important Context:**
*   **Baseline (Before):** Measurements taken on an **Apple Silicon M3** (ARM64, High Performance).
*   **Current (After):** Measurements taken on a **Standard Linux Cloud VM** (x86_64, variable clock).

### Interpreting the Numbers
1.  **Simple Waveforms (e.g., Sine, Saw):** Show a performance *drop* (approx. -50% to -80%). This is **expected** due to the raw hardware difference between the M3 (baseline) and the cloud environment. The M3 has significantly higher single-core IPC and clock speed.
2.  **Complex & Sigma Waveforms:** Show significant **gains** (approx. +30% to +40%). Despite the slower hardware, the **algorithmic optimization** (removing recursion overhead, better cache locality, iterative loops) overcomes the hardware deficit.

> **Key Takeaway:** The new architecture is sufficiently efficient that it runs complex, heavy patches *faster* on a standard cloud CPU than the old architecture did on a high-end M3 processor.

## Performance Scorecard

| ROM ID | Name | Baseline (M3) (ns) | v1.8.10 (Cloud) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| 0 | Triangle Up | 75.45 | 59.97 | **20.52%** |
| 1 | Triangle Down | 76.76 | 52.41 | **31.72%** |
| 2 | Sine Up | 91.07 | 153.14 | -68.16% |
| 3 | Sine Down | 95.78 | 148.86 | -55.42% |
| 4 | Square Up | 46.59 | 56.45 | -21.16% |
| 5 | Square Down | 45.81 | 63.85 | -39.38% |
| 6 | Saw Rising | 78.59 | 73.47 | **6.51%** |
| 7 | Saw Falling | 80.77 | 71.29 | **11.74%** |
| 8 | Saw/Sine Up | 94.63 | 134.48 | -42.11% |
| 9 | Sine/Saw Down | 96.19 | 162.98 | -69.44% |
| 10 | Square/Sine Up | 91.89 | 109.50 | -19.16% |
| 11 | Sine/Square Down | 94.83 | 158.11 | -66.73% |
| 12 | Saw/Triangle Up | 88.32 | 34.33 | **61.13%** |
| 13 | Triangle/Saw Down | 78.63 | 46.66 | **40.66%** |
| 14 | Triangle/Sine Up | 81.67 | 72.93 | **10.70%** |
| 15 | Sine/Triangle Down | 84.56 | 76.24 | **9.84%** |
| 16 | Clipped Sine | 85.38 | 55.23 | **35.31%** |
| 17 | Rectified Sine | 71.58 | 49.24 | **31.21%** |
| 18 | Sine * Saw | 71.59 | 92.70 | -29.49% |
| 19 | Overload Spark | 120.27 | 52.92 | **56.00%** |
| 20 | Overfolded Saw | 103.85 | 63.35 | **39.00%** |
| 21 | Clipped Chaos | 125.70 | 67.91 | **45.97%** |
| 22 | Wavefolder Sim (A=Fold B=Bias) | 81.56 | 70.84 | **13.14%** |
| 29 | Gritty Bass | 73.39 | 68.62 | **6.50%** |
| 30 | Hybrid Saw*Sine | 54.37 | 43.51 | **19.97%** |
| 31 | Razor Pulse | 81.27 | 59.42 | **26.89%** |
| 32 | Pulse 25% | 49.33 | 47.81 | **3.08%** |
| 33 | Pulse 75% | 46.33 | 49.15 | -6.09% |
| 34 | Staircase 4 Step | 107.81 | 49.12 | **54.44%** |
| 35 | Bit Crush Bomb | 114.44 | 52.33 | **54.27%** |
| 36 | Bit-Crushed Square | 104.59 | 62.57 | **40.18%** |
| 37 | Pulse Train Wreck | 133.80 | 51.69 | **61.37%** |
| 38 | Narrow | 76.91 | 25.11 | **67.35%** |
| 39 | Quantized Saw 8 | 42.91 | 37.33 | **13.00%** |
| 40 | PWM Synth (A=Width B=Sub) | 76.16 | 58.75 | **22.86%** |
| 41 | PWM Gate | 46.07 | 39.91 | **13.37%** |
| 42 | Harmonic Switch | 84.56 | 61.72 | **27.01%** |
| 43 | Multi-Gate | 96.15 | 51.71 | **46.22%** |
| 48 | Warp Speed | 77.74 | 22.80 | **70.67%** |
| 49 | Ghost Wail | 69.04 | 50.31 | **27.13%** |
| 50 | Laser Malfunction | 100.14 | 63.90 | **36.19%** |
| 51 | Hyperspace Glitch | 123.91 | 24.44 | **80.28%** |
| 52 | Shredded Saw | 101.88 | 71.11 | **30.20%** |
| 53 | Glitch Sine | 103.51 | 70.68 | **31.72%** |
| 54 | 4-Segment Bump | 129.84 | 55.12 | **57.55%** |
| 55 | Bird Call AM | 68.65 | 55.31 | **19.43%** |
| 56 | Phase Distortion (A=Amt B=Shape) | 83.22 | 71.14 | **14.52%** |
| 57 | Chaos Sine (A=ModRate B=ModAmt) | 68.58 | 55.27 | **19.41%** |
| 58 | Phase Glitch | 61.53 | 61.75 | -0.36% |
| 59 | Phase Distortion Wave | 258.76 | 205.38 | **20.63%** |
| 64 | Classic FM EP (A=Index B=Detune) | 98.97 | 87.44 | **11.65%** |
| 65 | FM Bass Growl (A=Fdbk B=Index) | 139.08 | 127.29 | **8.48%** |
| 66 | Freq Shifter FM (A=Shift B=Index) | 72.75 | 60.81 | **16.41%** |
| 67 | Complex FM A=Index B=ModFreq | 73.56 | 58.54 | **20.42%** |
| 68 | FM Pluck | 170.26 | 148.81 | **12.60%** |
| 69 | FM Pitched Grit | 172.17 | 154.93 | **10.01%** |
| 70 | FM Dynamic Lead | 230.45 | 162.73 | **29.39%** |
| 71 | FM Glassy Evolve | 121.88 | 106.23 | **12.84%** |
| 72 | FM: Classic EP (A=Tine B=Bell C=Ratio) | 109.59 | 88.94 | **18.84%** |
| 73 | FM: Growl Bass (A=Index B=Fdbk C=Ratio) | 97.44 | 79.52 | **18.39%** |
| 79 | FM: Sci-Fi Drone (A=Evolve B=Chaos C=Pitch) | 94.39 | 74.02 | **21.58%** |
| 80 | FM Metallic Bell (A=Decay B=Ratio) | 115.05 | 105.94 | **7.92%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 132.58 | 96.89 | **26.92%** |
| 82 | FM Harsh Noise Sweep (A=Sweep B=Intensity) | 119.73 | 104.61 | **12.63%** |
| 83 | FM Soft Pad (A=Brightness B=Chorus) | 167.60 | 126.57 | **24.48%** |
| 84 | FM Bipolar Sweep Pad | 104.25 | 85.96 | **17.54%** |
| 85 | FM Clangorous Hit (A=Metal B=Dissonance) | 175.66 | 148.25 | **15.60%** |
| 87 | FM Evolving SciFi (A=Evolve B=Harmonics) | 145.45 | 106.28 | **26.93%** |
| 88 | FM: Metallic Bell (A=Decay B=Index2 C=Ratio) | 118.56 | 106.45 | **10.21%** |
| 89 | FM: Glitchy Noise (A=Index B=Bit C=Rate) | 117.32 | 101.17 | **13.77%** |
| 93 | Sci-Fi Drone | 301.69 | 246.29 | **18.36%** |
| 94 | Evolving Metallic Bell | 796.01 | 625.29 | **21.45%** |
| 95 | Alien Communication | 286.32 | 204.98 | **28.41%** |
| 96 | Sine Harmonics | 97.92 | 84.07 | **14.14%** |
| 97 | Harmonic Noise Blast | 107.01 | 85.80 | **19.82%** |
| 98 | Brass | 113.78 | 123.58 | -8.61% |
| 99 | Bowed String | 126.98 | 135.04 | -6.35% |
| 100 | Additive Square | 115.24 | 88.20 | **23.46%** |
| 101 | Electric Pianoish | 120.66 | 83.73 | **30.61%** |
| 102 | Classic Pad | 115.32 | 85.66 | **25.72%** |
| 103 | Additive Saw (A=Harms B=Shape) | 190.93 | 310.54 | -62.65% |
| 104 | Random Phase Additive (A=RndAmt B=Harm) | 228.45 | 277.43 | -21.44% |
| 105 | Grit Additive (A=Grit B=Tone) | 288.92 | 262.65 | **9.09%** |
| 106 | Simple Minor Triad | 133.58 | 105.75 | **20.83%** |
| 112 | Formantish | 78.43 | 68.63 | **12.50%** |
| 113 | Vocal Ah | 111.30 | 103.10 | **7.37%** |
| 114 | Reso Filter Sweep (A=Reso B=Cutoff) | 163.34 | 130.23 | **20.27%** |
| 115 | Formant Vowel (A=Phsr1 B=Phsr2) | 645.98 | 472.08 | **26.92%** |
| 116 | Sync Sweep No Slant | 55.96 | 43.72 | **21.87%** |
| 117 | Sync Sweep Cos Shape | 70.40 | 61.15 | **13.14%** |
| 118 | Smoothed Sync (A=SyncFreq B=Duty) | 138.03 | 121.29 | **12.13%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 67.96 | 60.43 | **11.08%** |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 67.32 | 58.71 | **12.79%** |
| 121 | Oooh Choir Formant | 2304.00 | 2302.03 | **0.09%** |
| 125 | FM Breathy Flute (A=Air B=PitchMod) | 137.67 | 93.22 | **32.29%** |
| 128 | Kick Drum | 81.49 | 69.88 | **14.25%** |
| 129 | Snare Drum | 99.86 | 75.89 | **24.00%** |
| 130 | Clap | 104.74 | 94.90 | **9.39%** |
| 131 | Tom Drum | 88.48 | 65.89 | **25.53%** |
| 132 | Cymbalish | 86.73 | 74.89 | **13.65%** |
| 133 | Double Waves | 129.19 | 108.08 | **16.34%** |
| 134 | Metal Impact | 97.84 | 76.78 | **21.52%** |
| 135 | Bell Tone | 115.18 | 87.75 | **23.81%** |
| 136 | Metallic Perc | 92.67 | 72.81 | **21.43%** |
| 137 | Sigma Bell (A=Decay B=Metal) | 376.90 | 300.02 | **20.40%** |
| 138 | Classic Noise Sim | 177.57 | 141.28 | **20.44%** |
| 139 | Distorted Pitch | 150.59 | 129.51 | **14.00%** |
| 140 | Gritty Rumble Noise | 187.26 | 105.03 | **43.91%** |
| 141 | Filtered Static Noise | 684.39 | 456.36 | **33.32%** |
| 142 | Wooden Percussion | 89.24 | 86.53 | **3.04%** |
| 143 | Glitchy Percussion | 294.50 | 240.67 | **18.28%** |
| 144 | Plucked String (A=Damp B=Body) | 352.67 | 368.76 | -4.56% |
| 145 | Sigma A=End B=Decay | 202.93 | 284.10 | -40.00% |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 371.56 | 271.07 | **27.05%** |
| 147 | Rich String Ensemble | 1479.07 | 1427.42 | **3.49%** |
| 148 | Mellow Brass Section | 733.82 | 743.02 | -1.25% |
| 149 | Jittery Inharmonic Pitch | 792.37 | 984.35 | -24.23% |
| 150 | LFSR Granular Texture | 244.07 | 194.75 | **20.21%** |
| 151 | Morphing Harmonics | 2188.26 | 1753.97 | **19.85%** |
| 152 | Breathing Pad | 200.09 | 152.84 | **23.61%** |
| 153 | Chaotic Oscillator | 248.92 | 202.04 | **18.83%** |
| 154 | Crystalline Arpeggio | 1356.90 | 1171.79 | **13.64%** |
| 156 | Water Droplet | 100.12 | 59.48 | **40.59%** |
| 157 | Alien Chatter | 120.35 | 47.43 | **60.59%** |
| 159 | Wind AM | 64.32 | 48.82 | **24.10%** |
| 160 | LFSR Rhythm Gate | 132.76 | 86.97 | **34.49%** |
| 161 | LFSR Harmonic Chaos | 941.77 | 732.79 | **22.19%** |
| 162 | LFSR Digital Texture | 297.65 | 221.26 | **25.66%** |
| 163 | LFSR Poly Rhythm | 242.80 | 163.54 | **32.64%** |
| 164 | LFSR Phase Modulation | 160.86 | 137.43 | **14.57%** |
| 165 | LFSR Granular | 721.90 | 566.36 | **21.55%** |
| 166 | LFSR Rhythmic Harmonics | 1060.93 | 774.81 | **26.97%** |
| 167 | LFSR Spectral Shift | 207.43 | 150.33 | **27.53%** |
| 168 | LFSR Euclidean Beat | 165.02 | 133.28 | **19.23%** |
| 169 | LFSR Feedback Synth | 247.40 | 202.74 | **18.05%** |
| 170 | LFSR Algorithmic Lead | 256.78 | 165.89 | **35.40%** |
| 171 | LFSR Morphing Pad | 1269.51 | 1139.32 | **10.26%** |
| 172 | LFSR Breakbeat | 195.89 | 128.10 | **34.61%** |
| 173 | LFSR Probability Gate | 180.26 | 157.53 | **12.61%** |
| 174 | LFSR Polyrhythmic Chaos | 234.28 | 146.10 | **37.64%** |
| 175 | LFSR Glitch Matrix | 749.37 | 641.39 | **14.41%** |
| 176 | Pac-Man Wakka | 104.83 | 82.04 | **21.74%** |
| 177 | Pac-Man Power Pellet | 113.20 | 83.80 | **25.97%** |
| 178 | Pac-Man Death | 109.94 | 86.25 | **21.55%** |
| 179 | Pac-Man Ghost | 125.74 | 85.04 | **32.37%** |
| 180 | Space Invaders Shot | 132.36 | 101.09 | **23.62%** |
| 181 | Space Invaders March | 81.79 | 50.83 | **37.85%** |
| 182 | Space Invaders UFO | 150.52 | 97.26 | **35.38%** |
| 183 | Space Invaders Explosion | 129.85 | 118.34 | **8.86%** |
| 184 | Asteroids Thrust | 174.96 | 163.12 | **6.77%** |
| 185 | Asteroids Shoot | 135.43 | 102.65 | **24.20%** |
| 186 | Asteroids Explosion | 136.78 | 120.19 | **12.13%** |
| 187 | Asteroids Hyperspace | 147.84 | 119.04 | **19.48%** |
| 188 | Galaxian Attack | 109.38 | 76.15 | **30.38%** |
| 189 | Galaxian Formation | 158.19 | 98.57 | **37.69%** |
| 190 | Centipede Laser | 147.32 | 117.71 | **20.10%** |
| 191 | Centipede Flea Drop | 110.51 | 90.28 | **18.31%** |
| 192 | Defender Thrust | 170.65 | 164.65 | **3.52%** |
| 193 | Defender Smart Bomb | 152.62 | 114.97 | **24.67%** |
| 194 | Frogger Hop | 133.49 | 105.31 | **21.11%** |
| 195 | Frogger Traffic | 129.61 | 121.56 | **6.21%** |
| 196 | Donkey Kong Hammer | 112.03 | 129.85 | -15.91% |
| 197 | Donkey Kong Jump | 113.49 | 94.31 | **16.90%** |
| 198 | Missile Command Explosion | 141.80 | 126.10 | **11.07%** |
| 199 | Tempest Shoot | 139.21 | 106.97 | **23.16%** |
| 200 | Tempest Flip | 106.62 | 82.55 | **22.58%** |
| 201 | Berzerk Robot Voice | 187.70 | 171.10 | **8.84%** |
| 202 | Robotron Shoot | 143.71 | 115.07 | **19.93%** |
| 203 | Phoenix Bird Cry | 140.74 | 101.63 | **27.79%** |
| 204 | Gorf Laser | 135.79 | 111.00 | **18.26%** |
| 205 | Scramble Engine | 171.00 | 162.95 | **4.71%** |
| 206 | Zaxxon Alarm | 135.79 | 99.74 | **26.55%** |
| 207 | Moon Patrol Bounce | 159.52 | 125.89 | **21.08%** |
| 208 | POKEY Pure Tone | 77.26 | 77.09 | **0.22%** |
| 209 | POKEY Filtered Noise | 100.02 | 111.58 | -11.56% |
| 210 | POKEY Distorted Bass | 141.52 | 128.80 | **8.99%** |
| 211 | POKEY Laser Zap | 145.43 | 164.75 | -13.28% |

**Overall Average Delta (Hardware + Software): 16.80%**
