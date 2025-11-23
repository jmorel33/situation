# The "Situation" Advanced Platform Awareness, Control, and Timing
## Core API Library Reference Manual

| Metadata | Details |
| :--- | :--- |
| **Version** | 2.3.4F "Velocity" (Hotfix F) |
| **Language** | Strict C11 (ISO/IEC 9899:2011) / C++ Compatible |
| **Backends** | OpenGL 4.6 Core / Vulkan 1.1+ |
| **License** | MIT License |
| **Author** | Jacques Morel |

### Executive Summary

"Situation" is a high-performance, single-file application kernel designed for cross-platform C and C++ development. It provides a unified, deterministic abstraction layer over the host operating system and graphics hardware.

Unlike simple windowing wrappers, Situation is an opinionated System Abstraction Layer (SAL). It isolates the developer from the fragmentation of OS APIs (Windows/Linux/macOS) and Graphics Drivers (OpenGL/Vulkan), providing a stable, "Titanium-grade" foundation for building sophisticated interactive software—from real-time simulations and game engines to scientific visualization tools and multimedia installations.

The library is engineered around three architectural pillars:

1. **Awareness**
   The library provides deep introspection of the host environment. It does not simply open a window; it queries the entire system topology.
   *   **Hardware:** Real-time monitoring of GPU memory (VRAM), CPU topology, and storage capacity.
   *   **Display:** Complete multi-monitor awareness, including physical dimensions, refresh rates, and HiDPI scaling factors.
   *   **Events:** Precise handling of OS signals, file drops, focus changes, and clipboard state.

2. **Control**
   The library hands the developer explicit, granular control over the application stack.
   *   **Unified Graphics:** A modern Command Buffer architecture that abstracts OpenGL 4.6 and Vulkan 1.1 behind a single API. It manages complex resources—Shaders, Compute Pipelines, and Descriptor Sets—automatically, while enforcing correct synchronization barriers.
   *   **Hardened Audio:** A professional-grade audio engine supporting thread-safe capture (microphone), low-latency SFX playback, disk streaming for music, and a programmable DSP chain (Reverb, Echo, Filters).
   *   **Input:** A lock-free, ring-buffered input system that guarantees $O(1)$ access time and zero event loss, even during frame-rate spikes.

3. **Timing**
   The library enforces a strict temporal cadence to ensure simulation stability.
   *   **High-Resolution Timing:** Sub-millisecond performance measurement and frame-rate limiting.
   *   **Oscillator System:** An integrated temporal oscillator bank for synchronizing logic and visuals to rhythmic, periodic events independent of the rendering frame rate.

### New in v2.3.4 "Velocity"

This release shifts focus from pure stability to Developer Efficiency. The "Velocity" module introduces a comprehensive suite of Hot-Reloading tools.

Developers can now modify Shaders, Compute Pipelines, Textures, and 3D Models on disk and reload them instantly into the running application via the API. The engine handles the complex task of stalling the GPU, destroying old resources, and seamlessly swapping in new assets while maintaining existing handle IDs. This drastically accelerates the iteration loop for visual programming and content creation.

## Table of Contents

- [1.0 Core System Architecture](#10-core-system-architecture)
  - [1.1 Lifecycle Management](#11-lifecycle-management)
    - [Initialization (SituationInit, SituationInitInfo)](#initialization-situationinit-situationinitinfo)
    - [Shutdown & Cleanup (SituationShutdown)](#shutdown--cleanup-situationshutdown)
    - [The Application Loop (SituationWindowShouldClose)](#the-application-loop-situationwindowshouldclose)
  - [1.2 The Execution Model](#12-the-execution-model)
    - [The Three-Phase Frame (Input, Update, Render)](#the-three-phase-frame-input-update-render)
    - [Thread Safety Contracts](#thread-safety-contracts)
    - [Error Handling & Logging](#error-handling--logging)
  - [1.3 Timing & Synchronization](#13-timing--synchronization)
    - [Frame Pacing (SituationSetTargetFPS)](#frame-pacing-situationsettargetfps)
    - [High-Resolution Timing (SituationGetFrameTime)](#high-resolution-timing-situationgetframetime)
    - [The Temporal Oscillator System](#the-temporal-oscillator-system)
  - [1.4 Hardware Awareness](#14-hardware-awareness)
    - [System Query (SituationGetDeviceInfo)](#system-query-situationgetdeviceinfo)
    - [Power Management](#power-management)
- [2.0 Windowing & Display Subsystem](#20-windowing--display-subsystem)
  - [2.1 Window State Management](#21-window-state-management)
    - [Configuration Flags (SITUATION_FLAG_*)](#configuration-flags-situation_flag_)
    - [Fullscreen & Borderless Modes](#fullscreen--borderless-modes)
    - [Focus & Attention](#focus--attention)
  - [2.2 Multi-Monitor Topology](#22-multi-monitor-topology)
    - [Monitor Enumeration (SituationGetDisplays)](#monitor-enumeration-situationgetdisplays)
    - [Video Modes & Refresh Rates](#video-modes--refresh-rates)
    - [DPI Scaling & Coordinate Mapping](#dpi-scaling--coordinate-mapping)
  - [2.3 Cursor & Clipboard](#23-cursor--clipboard)
    - [Hardware Cursor Shapes](#hardware-cursor-shapes)
    - [Raw Motion (FPS Mode)](#raw-motion-fps-mode)
    - [System Clipboard Access](#system-clipboard-access)
- [3.0 The Graphics Pipeline](#30-the-graphics-pipeline)
  - [3.1 The Command Buffer Abstraction](#31-the-command-buffer-abstraction)
    - [Concept: Immediate (GL) vs Deferred (VK)](#concept-immediate-gl-vs-deferred-vk)
    - [Frame Acquisition (SituationAcquireFrameCommandBuffer)](#frame-acquisition-situationacquireframecommandbuffer)
    - [Submission (SituationEndFrame)](#submission-situationendframe)
  - [3.2 Render Passes](#32-render-passes)
    - [Pass Configuration (SituationRenderPassInfo)](#pass-configuration-situationrenderpassinfo)
    - [Load/Store Operations & Clearing](#loadstore-operations--clearing)
    - [Viewport & Scissor](#viewport--scissor)
  - [3.3 Shader Pipelines](#33-shader-pipelines)
    - [Graphics Pipelines (SituationLoadShader)](#graphics-pipelines-situationloadshader)
    - [Uniform Management](#uniform-management)
    - [Hot-Reloading Shaders](#hot-reloading-shaders)
  - [3.4 Geometry & Meshes](#34-geometry--meshes)
    - [Vertex Data Layouts](#vertex-data-layouts)
    - [Mesh Creation & Destruction](#mesh-creation--destruction)
    - [Model Loading (GLTF)](#model-loading-gltf)
  - [3.5 Textures & Images](#35-textures--images)
    - [Image Manipulation (CPU)](#image-manipulation-cpu)
    - [Texture Creation (GPU)](#texture-creation-gpu)
    - [Samplers & Mipmaps](#samplers--mipmaps)
  - [3.6 Virtual Display Compositor](#36-virtual-display-compositor)
    - [Concept: Offscreen Rendering](#concept-offscreen-rendering)
    - [Scaling Modes (Integer, Fit, Stretch)](#scaling-modes-integer-fit-stretch)
    - [Blend Modes (Overlay, Soft Light, etc.)](#blend-modes-overlay-soft-light-etc)
  - [3.7 Compute & GPGPU](#37-compute--gpgpu)
    - [Compute Pipelines](#compute-pipelines)
    - [Storage Buffers (SSBOs)](#storage-buffers-ssbos)
    - [Dispatch & Synchronization Barriers](#dispatch--synchronization-barriers)
- [4.0 Audio Engine](#40-audio-engine)
  - [4.1 Audio Context](#41-audio-context)
    - [Device Enumeration](#device-enumeration)
    - [Master Control](#master-control)
  - [4.2 Resource Management](#42-resource-management)
    - [Loading Strategies (Stream vs. RAM)](#loading-strategies-stream-vs-ram)
    - [Decoding](#decoding)
  - [4.3 Voice Management](#43-voice-management)
    - [Playback Control (Play/Stop/Loop)](#playback-control-playstoploop)
    - [Properties (Pitch, Pan, Volume)](#properties-pitch-pan-volume)
  - [4.4 DSP Chain](#44-dsp-chain)
    - [Filters (Low/High Pass)](#filters-lowhigh-pass)
    - [Environmental Effects (Reverb, Echo)](#environmental-effects-reverb-echo)
    - [Custom Processors](#custom-processors)
  - [4.5 Audio Capture](#45-audio-capture)
    - [Microphone Initialization](#microphone-initialization)
    - [Buffer Access](#buffer-access)
- [5.0 Input & Haptics](#50-input--haptics)
  - [5.1 Architecture: Ring Buffers & Polling](#51-architecture-ring-buffers--polling)
  - [5.2 Keyboard](#52-keyboard)
    - [Key States vs. Events](#key-states-vs-events)
    - [Text Input](#text-input)
  - [5.3 Mouse](#53-mouse)
    - [Position, Delta, and Wheel](#position-delta-and-wheel)
    - [Buttons](#buttons)
  - [5.4 Gamepad](#54-gamepad)
    - [Connection Handling](#connection-handling)
    - [Axis & Button Mapping](#axis--button-mapping)
    - [Haptic Feedback (Rumble)](#haptic-feedback-rumble)
- [6.0 Filesystem & I/O](#60-filesystem--io)
  - [6.1 Path Management](#61-path-management)
    - [Virtual Paths & Base Directories](#virtual-paths--base-directories)
    - [User Data / Save Paths](#user-data--save-paths)
  - [6.2 File Operations](#62-file-operations)
    - [Reading/Writing (Binary & Text)](#readingwriting-binary--text)
    - [File Drop Events](#file-drop-events)
