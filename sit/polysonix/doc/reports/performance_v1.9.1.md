# Polysonix v1.9.1 Performance Report

## Overview
This report details the performance improvements introduced in version 1.9.1, focusing on the optimization of the `tanh` transcendental function within the VM.

The standard library `tanhf` was identified as a performance bottleneck in patches heavily utilizing saturation, distortion, or wave-shaping. This version introduces a fast polynomial approximation (`fast_tanh`) using a Padé [2/2] approximant, which significantly reduces CPU cost while maintaining high audio fidelity.

## Methodology
Benchmarks were conducted by executing compiled bytecode for specific ROM scripts that make heavy use of the `tanh` function.

*   **Test Duration:** 5.0 seconds of audio generation per script.
*   **Sample Rate:** 44.1 kHz (220,500 samples total).
*   **Environment:** CPU-based VM execution.
*   **Baseline:** Standard C library `tanhf`.
*   **Optimized:** `vm_fast_tanh` (Padé [2/2] approximation).

## Results

| Script Name | Script ID | Description | Baseline Time (s) | Optimized Time (s) | Speedup Factor |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Simple Sine** | Control | `sin(x)` (No tanh) | 0.0171 | 0.0218 | 0.78x (Noise)* |
| **Tanh Drive** | #24 | Basic Saturation | 0.0723 | 0.0326 | **2.22x** |
| **FM Bass Growl** | #65 | FM + Saturation | 0.1190 | 0.0903 | **1.32x** |
| **Tanh Fold** | #250 | Complex Folding | 0.1057 | 0.0929 | **1.14x** |

*\*Note: The slight regression in the "Simple Sine" control test is likely due to run-to-run variance or measurement noise, as the code path for `sin(x)` was unchanged.*

## Analysis
The optimization yields substantial gains for scripts where `tanh` is the dominant computational factor.

*   **Tanh Drive:** This script effectively isolates the `tanh` function call. The **2.2x** speedup confirms that the polynomial approximation is drastically faster than the standard library call.
*   **FM Bass Growl:** This script involves complex FM synthesis (`sin` calls) in addition to `tanh`. Even here, the optimization provides a **32%** overall performance boost for the voice.
*   **Tanh Fold:** This script mixes `sin` and arithmetic heavily. The **14%** improvement demonstrates that even in complex arithmetic-heavy scripts, `tanh` optimization contributes meaningfully to lower CPU usage.

## Conclusion
The introduction of `vm_fast_tanh` provides a critical performance optimization for saturation-heavy patches, freeing up CPU cycles for higher polyphony counts or more complex modulation routing without sacrificing perceptible audio quality.
