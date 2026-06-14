# Situation — Core Concepts & Architecture

This document covers the internal design principles, threading model, audio pipeline, and graphics backend lifecycles of the Situation library. For a getting-started guide and API reference, see [introduction.md](introduction.md).

---

## Table of Contents

- [Design Principles](#design-principles)
- [Global System Architecture](#global-system-architecture)
- [Threading Architecture](#threading-architecture)
- [Async Vulkan Shader Compilation](#async-vulkan-shader-compilation-glsl--spir-v)
- [Audio Node Graph Architecture](#audio-node-graph-architecture)
- [OpenGL 4.6 Backend Lifecycle](#opengl-46-backend-lifecycle)
- [Vulkan 1.4 Backend Lifecycle](#vulkan-14-backend-lifecycle)

---

## Design Principles

The library is built on several core principles to ensure a simple, predictable, and high-performance development experience.

-   **Unified Command Abstraction:** The API exposes a single "Command Buffer" model for rendering.
    -   In **Vulkan**, this maps 1:1 to hardware command buffers for deferred execution.
    -   In **OpenGL**, this acts as a "pass-through" layer, executing commands immediately while maintaining API compatibility.
-   **The "Update-Before-Draw" Contract:** To guarantee identical behavior across backends, you must strictly separate data updates from draw calls within a frame. Always update your buffers/constants *before* recording the draw commands that use them.
-   **Generational Threading Model:**
    -   **Main Thread:** Handles OS Events, Windowing, and Recording Render Commands.
    -   **Task System:** Handles Logic, Physics, and File I/O via **Dual Priority Queues**:
        -   **High Priority:** For frame-critical tasks (Physics, Culling).
        -   **Low Priority:** For streaming (Asset Loading).
-   **Explicit Resource Management:** There is no garbage collector. Every resource created with `SituationCreate...` or `SituationLoad...` returns an opaque handle and **must** be explicitly released with its corresponding `SituationDestroy...` or `SituationUnload...` function.
-   **Three-Phase Frame:** The main loop follows a strict, non-blocking cadence:
    1.  **Input:** `SituationPollInputEvents()` — gathers OS events into thread-safe buffers.
    2.  **Update:** `SituationUpdateTimers()` & User Logic (Physics, AI, Audio triggers).
    3.  **Render:** `SituationAcquireFrameCommandBuffer` → Record Commands → `SituationEndFrame`.

---

## Global System Architecture

This diagram illustrates the high-level flow of the Situation Engine, showing how the main thread coordinates with the parallel task system, audio engine, and I/O subsystems.

```mermaid
graph TD
    %% Main Lifecycle
    subgraph Init ["Initialization (Main Thread)"]
        I1["SituationInit"]
        I2["Init Platform & Window"]
        I3["Init Renderer (GL/VK)"]
        I4["Init Audio Engine<br/>(miniaudio)"]
        I5["Init Input System<br/>(Ring Buffers)"]
        I6["SituationCreateThreadPool"]
    end

    subgraph Loop ["Main Loop"]
        subgraph Input ["Input Processing"]
            IN1["SituationPollInputEvents<br/>(GLFW pump)"]
            IN2["O(1) ring buffers"]
        end

        subgraph TaskSystem ["Generational Task System (Parallel)"]
            TS1["High Priority Queue<br/>(Physics / AI)"]
            TS2["Low Priority Queue<br/>(Asset Loading / IO)"]
            TS_W["Worker Threads"]
            TS_IO["Dedicated I/O Thread"]

            TS1 -.-> TS_W
            TS2 -.-> TS_IO
        end

        subgraph Audio ["Audio Engine (miniaudio callback thread)"]
            AU1["Playback callback"]
            AU_G["Node graph<br/>SituationProcessGraph"]
            AU_V["Loaded voices<br/>(snapshot mix + FX)"]
            AU_T["Tone pool"]
            AU_OUT["Mixed stereo -> device"]
            AU1 --> AU_G
            AU1 --> AU_V
            AU1 --> AU_T
            AU_G --> AU_OUT
            AU_V --> AU_OUT
            AU_T --> AU_OUT
            TS_IO -. "Async preload / streaming feeds voices" .-> AU_V
        end

        L2["Update Timers & Logic"]
        L3["User Game Code"]
        L4["Record Render Commands"]
        L5["SituationEndFrame"]
    end

    subgraph Exit ["Termination"]
        E1["SituationShutdown"]
        E2["Destroy ThreadPool"]
        E3["Shutdown Audio"]
        E4["Cleanup Renderer"]
        E5["Destroy Window"]
    end

    %% Flow Connections
    I1 --> I2 --> I3 --> I4 --> I5 --> I6 --> IN1
    IN1 --> IN2 --> L2
    L2 --> L3
    L3 -- "Dispatch Jobs" --> TS1
    L3 -- "Load Asset" --> TS2
    L3 --> L4 --> L5
    L5 -- "Next Frame" --> IN1
    L5 -- "Quit" --> E1
    E1 --> E2 --> E3 --> E4 --> E5

    %% Data Dependencies
    IN2 -. "Read Input" .-> L3
    TS_W -. "Results" .-> L3
```

**Audio pipeline (Phase H+):** The hardware callback sums **three paths** into one buffer when enabled: **`SituationProcessGraph`** (`active_graph`, optional default graph at init), **`SituationPlayLoadedSound`** / streaming (**`active_voices`** snapshot + per-voice DSP), and **`SituationPlayTone`** (**tone pool**). The legacy console **`SituationAudioMixer`** has been removed in favor of **node graphs**. Details: `doc/plan/AUDIO_NODE_COMPLETION_PLAN.md` § *Canonical miniaudio callback pipeline*.

---

## Threading Architecture

Situation's threading layer is a C11 generational job system with two priority queues, explicit wait semantics, optional CPU/NUMA placement, a dedicated I/O lane, and runtime diagnostics. The queues use a mutex per ring for mutation, while job IDs, state transitions, counters, and queue indices are tracked with atomics.

```mermaid
graph TD
    subgraph Init ["Initialization and Placement Policy"]
        INIT["SituationInitInfo"]
        TOPO["CPU topology cache<br/>logical CPUs, physical cores<br/>HT sibling detection"]
        NUMA["NUMA topology cache<br/>node masks, memory sizes<br/>preferred node per thread (TLS)"]
        POLICY["Placement policy<br/>main_thread_name, thread_affinity_*<br/>worker_numa_spread, io_thread_numa_node"]
        SIZE["Auto worker sizing<br/>SituationGetRecommendedWorkerCount<br/>logical or physical cores minus 4 reserved<br/>(Main + Render + Audio + I/O)"]
        POOL["SituationCreateThreadPool<br/>workers, queues, generations, counters"]

        INIT --> TOPO
        INIT --> NUMA
        INIT --> POLICY
        INIT --> SIZE
        TOPO --> SIZE
        NUMA --> POLICY
        SIZE --> POOL
        POLICY --> POOL
    end

    subgraph Submit ["Job Submission"]
        CALLER["Main thread or user thread"]
        DISPATCH["SituationDispatchParallel<br/>fork-join batches"]
        SUBMIT["SituationSubmitJobEx"]
        JOBID["Generational job ID<br/>1-bit queue | 15-bit gen | 16-bit slot"]
        CYCLE["Cycle detection<br/>depth-limited DFS, max depth 32<br/>SituationAddJobDependency"]
        ROUTE{"Queue mask / priority"}
        BACKPRESSURE{"Queue full?"}
        INLINE["RUN_IF_FULL<br/>execute inline on caller"]
        BLOCK["BLOCK_IF_FULL<br/>spin/yield until room"]
        FAIL["Return queue-full error"]

        CALLER --> DISPATCH
        CALLER --> SUBMIT
        DISPATCH --> SUBMIT
        SUBMIT --> JOBID
        JOBID --> CYCLE
        CYCLE --> ROUTE
        ROUTE -->|"High priority"| HQ
        ROUTE -->|"Low priority / I/O"| LQ
        HQ --> BACKPRESSURE
        LQ --> BACKPRESSURE
        BACKPRESSURE -->|"run inline policy"| INLINE
        BACKPRESSURE -->|"blocking policy"| BLOCK
        BACKPRESSURE -->|"nonblocking policy"| FAIL
        BLOCK --> ROUTE
    end

    subgraph Queues ["Dual Priority Rings"]
        HQ["High queue<br/>frame-critical work<br/>physics, culling, gameplay"]
        LQ["Low queue<br/>background work<br/>asset decode, streaming, file I/O"]
        HQM["High queue mutex<br/>atomic head/tail and pending depth"]
        LQM["Low queue mutex<br/>atomic head/tail and pending depth"]

        HQ --> HQM
        LQ --> LQM
    end

    subgraph Workers ["Execution Lanes"]
        WORKERS["Worker threads<br/>high queue first; low queue<br/>only when no I/O thread"]
        WPLACED["Worker placement<br/>multi-NUMA: spread by node<br/>single-NUMA: pin to distinct physical core"]
        SCAN["Full-queue in-place claim<br/>head→tail scan, CLAIM_BIT CAS<br/>HOL starvation fix (v2.4.232-233)"]
        IDLE["Idle wait<br/>cnd_timedwait 1ms<br/>self-wakes even on missed signals"]
        STEAL["Main-thread helping<br/>DispatchParallel drains high queue<br/>while waiting for batch"]
        IO["Dedicated I/O thread<br/>owns low queue exclusively"]
        RUN["Run callback"]
        COMPLETE["Mark job complete<br/>atomic state + counters<br/>wake continuations"]
        ORPHAN["Orphan retirement<br/>_SitThreadPoolRetireOrphanedJobMain<br/>used by async shader compile path"]

        POOL --> WPLACED --> WORKERS
        HQM --> SCAN --> WORKERS
        WORKERS --> IDLE
        IDLE --> WORKERS
        HQM --> STEAL
        LQM --> IO
        WORKERS --> RUN
        IO --> RUN
        STEAL --> RUN
        RUN --> COMPLETE
        COMPLETE --> ORPHAN
    end

    subgraph Wait ["Synchronization"]
        WAITJOB["SituationWaitForJob"]
        WAITALL["SituationWaitForAllJobs"]
        DEPWAKE["Dependent jobs become runnable<br/>continuation_id CAS chain"]
        JOIN["DispatchParallel returns<br/>batch complete"]

        COMPLETE --> DEPWAKE
        DEPWAKE --> ROUTE
        CALLER --> WAITJOB
        CALLER --> WAITALL
        DISPATCH --> JOIN
        COMPLETE --> WAITJOB
        COMPLETE --> WAITALL
        COMPLETE --> JOIN
    end

    subgraph Observe ["Observability and Metrics"]
        SNAP["SituationGetThreadPoolSnapshot<br/>worker/I/O/render/audio CPU + NUMA"]
        STATUS["SituationGetThreadingStatus<br/>capabilities and pool summary"]
        DEPTH["Queue depth APIs<br/>high, low, active jobs"]
        METRICS["Scheduler metrics<br/>lock ops/ns, scans, steal ok/fail,<br/>inline runs, queue-full spins,<br/>I/O idle/jobs, dispatch parallel calls"]
        GRAPH["SituationDumpTaskGraph<br/>active jobs, dep counts,<br/>continuation links (text or JSON)"]
        DUMP["Dump helpers<br/>SituationDumpThreadingReport<br/>SituationDumpThreadPoolStatus<br/>SituationDumpThreadPoolMetrics"]

        POOL --> SNAP
        POOL --> STATUS
        HQM --> DEPTH
        LQM --> DEPTH
        RUN --> METRICS
        BACKPRESSURE --> METRICS
        SCAN --> METRICS
        IO --> METRICS
        STEAL --> METRICS
        COMPLETE --> GRAPH
        SNAP --> DUMP
        STATUS --> DUMP
        DEPTH --> DUMP
        METRICS --> DUMP
        GRAPH --> DUMP
    end
```

In practice, use **high priority** for frame-critical jobs that the main thread may help drain during `SituationDispatchParallel`, and **low priority** for background work that can tolerate latency. Use `SituationInitInfo` affinity and NUMA fields only when you need predictable placement; the default path auto-sizes the pool and keeps affinity fail-soft so initialization does not break on restricted systems.

Key implementation details:
- Workers idle via `cnd_timedwait` (1ms cap) rather than an indefinite wait, ensuring they self-wake even if a signal is missed.
- Worker NUMA placement is context-sensitive: on multi-NUMA systems workers spread across NUMA nodes; on single-NUMA systems they pin to distinct physical cores instead.
- Dependency edges are validated with a depth-limited cycle detection DFS (max depth 32) before being committed — jobs exceeding the depth limit are rejected with `SITUATION_ERROR_THREAD_CYCLE`.
- The orphan retirement path (`_SitThreadPoolRetireOrphanedJobMain`) allows the main thread to cleanly retire pool jobs whose work was satisfied out-of-band, used primarily by the async shader compile path.

---

## Async Vulkan Shader Compilation (GLSL → SPIR-V)

Vulkan async GLSL loads run **shaderc on the high-priority thread pool**, then build pipelines on the main thread during poll. Poll and unload share one internal progress driver (v2.4.238+).

```mermaid
graph LR
    Begin["BeginLoadShaderFromMemory"] --> Submit["SubmitJobEx HIGH<br/>_SituationVkAsyncCompileWorker"]
    Submit --> Worker["shaderc on worker<br/>compile_done 0→-3→1"]
    Poll["PollShaderLoad each frame"] --> Progress["_SituationVkAsyncCompileProgress"]
    Progress --> Pipelines["Build VkPipelines on SUCCESS"]
    Unload["UnloadShader during load"] --> Progress
    Progress --> Abandon["tier-2 abandon 2s<br/>retire job + detach ctx"]
```

**Contract:** call **`SituationPollShaderLoad`** each frame (not during acquire); terminal errors **-99** (lost job) and **-557** (compile timeout) if the compile can never finish. OpenGL uses driver async compile on the host GL context — no thread-pool shaderc job. Details: `doc/situation_sdk.md` §3.3.1 and `doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md`.

---

## Audio Node Graph Architecture

Conceptual **signal-flow** diagram (channel strip → bus → master → device). Real routing uses the registered node types and **`SituationAudioGraph`** / **`SituationProcessGraph`**; node names are illustrative.

```mermaid
graph TD
    %% --- Sources ---
    subgraph Sources
        TS[Tone Synth]
        SS[Sound Source]
        MC[Mic Capture]
    end

    %% --- Modulators ---
    subgraph Modulators
        LFO[LFO]
        EF[Envelope Follower]
    end

    %% --- Insert Effects Chain ---
    subgraph "Insert Effects (per-channel)"
        GAIN_PRE[Gain - Pre]
        FX1[Effect Slot 1\nReverb / Echo / Chorus / etc.]
        FX2[Effect Slot 2\nOverdrive / Phaser / etc.]
        DYN[Dynamics\nCompressor / Gate]
        EQ[EQ 4-Band]
        FILT[Filter]
        PAN[Panner]
        GAIN_POST[Gain - Post]
    end

    %% --- Bus / Mixer ---
    subgraph "Mix Bus"
        MIX[Mixer Node\nSum N inputs -> stereo]
    end

    %% --- Master Chain ---
    subgraph "Master Chain"
        MAMP[Mastering Amp]
        MAX[Maximizer / DeafMax]
        PEAK[Peak Meter]
        SPEC[Spectrum Analyzer]
    end

    %% --- Output ---
    OUT[Audio Device Output\nminiaudio callback]

    %% --- Signal Flow ---
    TS -->|audio| GAIN_PRE
    SS -->|audio| GAIN_PRE
    MC -->|audio| GAIN_PRE

    GAIN_PRE --> FX1 --> FX2 --> DYN --> EQ --> FILT --> PAN --> GAIN_POST
    GAIN_POST --> MIX

    MIX --> MAMP --> MAX --> PEAK --> SPEC --> OUT

    %% --- Modulation (control signals) ---
    LFO -.->|ctrl: rate/depth| PAN
    LFO -.->|ctrl: mod| FX1
    EF -.->|ctrl: envelope| DYN

    %% --- Analyzers (tap, no audio modification) ---
    PEAK -.->|read-only levels| OUT
    SPEC -.->|read-only FFT bins| OUT
```

---

## OpenGL 4.6 Backend Lifecycle

The OpenGL backend is designed to be "stateless" from the user's perspective while managing complex state caching internally. It features a "Soft Command Buffer" that records commands for execution either immediately or on a dedicated render thread. Key features include **MDI Auto-Batching** for geometry and **Virtual Bindless** (LRU Slot Management) for texture compatibility.

```mermaid
graph TD
    %% Nodes
    subgraph Init ["Initialization"]
        I1["SituationInit"]
        I2["Init OpenGL Context"]
        I3["Init Subsystems<br/>(Quad, Text, RingBuffer)"]
        I4["Init Virtual Bindless"]
    end

    subgraph Loop ["Frame Cycle"]
        L1["SituationPollInputEvents"]
        L2["SituationUpdateTimers"]
        L3["User Update Logic"]
        L4["SituationAcquireFrameCommandBuffer<br/>(Get SoftBuffer)"]
        L5["Record Commands<br/>(SoftBuffer)"]
        L6["SituationEndFrame"]

        subgraph Render ["Render Execution (Main or Thread)"]
            R1["Execute SoftBuffer"]
            R2{"Command Type?"}
            R2 -- "Draw Mesh" --> R3["MDI Auto-Batching"]
            R2 -- "Bind Texture" --> R4["Virtual Bindless LRU"]
            R2 -- "Other" --> R5["Direct GL Calls"]
            R3 --> R6["glMultiDrawElementsIndirect"]
            R4 --> R7["glBindTextureUnit"]
            R5 --> R8["Draw/Dispatch"]
            R6 --> R9{"More Commands?"}
            R7 --> R9
            R8 --> R9
            R9 -- "Yes" --> R2
            R9 -- "No" --> S1["glfwSwapBuffers"]
            S1 --> S2["glFenceSync"]
            S2 --> S3["Flush Graveyard<br/>(Deferred Cleanup)"]
        end
    end

    subgraph Exit ["Shutdown"]
        E1["SituationShutdown"]
        E2["Cleanup Subsystems"]
        E3["Destroy Context"]
    end

    %% Flow
    I1 --> I2 --> I3 --> I4 --> L1
    L1 --> L2 --> L3 --> L4 --> L5 --> L6
    L6 --> R1
    S3 --> L1
    L6 -- "Quit" --> E1
    E1 --> E2 --> E3
```

---

## Vulkan 1.4 Backend Lifecycle

The Vulkan backend is built for high-performance deferred rendering. It uses **Dynamic Descriptor Management** and a **Bindless Architecture** where all textures reside in a global unbound array (`global_textures[]`), indexed via Push Constants. Frame synchronization is handled via fences and semaphores, with a per-frame **Graveyard** for safe resource destruction.

```mermaid
graph TD
    subgraph Init ["Initialization"]
        I1["SituationInit"]
        I2["Create Instance & Device"]
        I3["Init VMA Allocator"]
        I4["Create Swapchain"]
        I5["Init Pipelines & Descriptors"]
    end

    subgraph Loop ["Frame Cycle"]
        L1["SituationPollInputEvents"]
        L2["SituationUpdateTimers"]
        L3["User Update Logic"]
        L4["SituationAcquireFrameCommandBuffer"]
        L4a["Wait for Fence"]
        L4b["Acquire Next Image"]
        L4c["vkBeginCommandBuffer"]
        L5["SituationCmdBeginRenderPass<br/>(O(1) Render Pass Cache)"]

        subgraph Bindless ["Bindless Tech"]
            B1["Push Constants"]
            B2["Texture ID -> global_textures"]
            B3["nonuniformEXT Indexing"]
        end

        L6["vkCmdDraw* / Dispatch"]
        L7["vkEndCommandBuffer"]
        L8["SituationEndFrame"]

        subgraph Render ["Render Execution (Main or Thread)"]
            S1["vkQueueSubmit"]
            S2["vkQueuePresentKHR"]
            S3["Flush Graveyard<br/>(Cleanup Deferred Resources)"]
        end
    end

    subgraph Exit ["Shutdown"]
        E1["SituationShutdown"]
        E2["Wait Idle"]
        E3["Destroy Swapchain & Resources"]
        E4["Destroy Device & Instance"]
    end

    %% Flow
    I1 --> I2 --> I3 --> I4 --> I5 --> L1
    L1 --> L2 --> L3 --> L4
    L4 --> L4a --> L4b --> L4c --> L5
    L5 --> B1 --> B2 --> B3 --> L6
    L6 --> L7 --> L8
    L8 --> S1 --> S2 --> S3
    S3 --> L1
    L8 -- "Quit" --> E1
    E1 --> E2 --> E3 --> E4
```
