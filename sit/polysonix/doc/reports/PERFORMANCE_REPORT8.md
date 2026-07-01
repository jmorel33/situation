# Polysonix VM Performance Report 8

This report compares the execution time of the new VM against the PERFORMANCE_REPORT7.md baseline.
Measurements are averaged over 10 runs to eliminate OS and cache noise.

| Patch ID | Name | Report 7 (Before) (ns) | Current (After) (ns) | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| 2 | Sine Up | 94.72 | 89.00 | **6.04%** |
| 3 | Sine Down | 97.88 | 92.15 | **5.86%** |
| 4 | Square Up | 53.96 | 55.69 | -3.21% |
| 5 | Square Down | 54.61 | 56.00 | -2.54% |
| 6 | Saw Rising | 72.11 | 66.13 | **8.29%** |
| 8 | Saw/Sine Up | 86.44 | 73.88 | **14.53%** |
| 9 | Sine/Saw Down | 91.57 | 77.98 | **14.84%** |
| 10 | Square/Sine Up | 77.69 | 67.42 | **13.22%** |
| 11 | Sine/Square Down | 78.38 | 67.31 | **14.12%** |
| 14 | Triangle/Sine Up | 66.72 | 56.73 | **14.98%** |
| 15 | Sine/Triangle Down | 67.01 | 58.78 | **12.27%** |
| 16 | Clipped Sine | 48.74 | 45.54 | **6.57%** |
| 19 | Overload Spark | 71.13 | 72.31 | -1.65% |
| 20 | Overfolded Saw | 62.69 | 58.27 | **7.06%** |
| 21 | Clipped Chaos | 71.37 | 70.45 | **1.28%** |
| 29 | Gritty Bass | 53.94 | 53.10 | **1.56%** |
| 32 | Pulse 25% | 47.60 | 49.64 | -4.29% |
| 33 | Pulse 75% | 47.16 | 49.53 | -5.04% |
| 36 | Bit-Crushed Square | 62.13 | 59.36 | **4.47%** |
| 50 | Laser Malfunction | 58.98 | 55.29 | **6.26%** |
| 52 | Shredded Saw | 54.58 | 53.47 | **2.03%** |
| 81 | FM Hollow Drone (A=ModMix B=ModRatio) | 88.13 | 83.78 | **4.94%** |
| 116 | Sync Sweep No Slant | 43.58 | 40.03 | **8.13%** |
| 117 | Sync Sweep Cos Shape | 53.88 | 50.51 | **6.25%** |
| 119 | Limited Sync (A=SyncFreq B=Duty) | 55.92 | 52.21 | **6.63%** |
| 120 | Sync Sweep (A=SyncFreq B=Duty) | 55.80 | 52.55 | **5.83%** |
| 123 | Sync Soft | 35.42 | 40.94 | -15.57% |
| 129 | Snare Drum | 71.36 | 70.65 | **0.99%** |
| 130 | Clap | 88.56 | 88.10 | **0.51%** |
| 133 | Double Waves | 97.72 | 108.97 | -11.51% |
| 146 | Noisy Pad (A=NoiseAmt B=Flt) | 219.49 | 215.13 | **1.98%** |
| 208 | POKEY Pure Tone | 76.32 | 79.11 | -3.66% |
| 210 | POKEY Distorted Bass | 61.52 | 63.29 | -2.88% |
| 223 | POKEY Four Channel | 133.02 | 134.34 | -1.00% |
| 240 | Logic: PWM Hash | 61.11 | 35.10 | **42.55%** |
| 243 | Glitch Step | 25.65 | 27.91 | -8.83% |

## Summary

* **Total Patches Benchmarked:** 36
* **Averaged Over:** 10 test runs
* **Average Time per Sample (Report 7):** 71.86 ns
* **Average Time per Sample (Current):** 68.63 ns
* **Overall Performance Improvement:** **4.49%**
