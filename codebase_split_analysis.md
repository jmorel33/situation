# Version 2.4 Roadmap: Codebase Modularization

## 1. Executive Summary

This document outlines the strategic roadmap for version 2.4, focusing on modularizing the `situation` library's codebase while strictly adhering to the "Single Header API" philosophy. The goal is to separate the implementation logic (~30k lines) from the public API definition (~3k lines) to improve maintainability, without complicating the integration process for end-users.

**Selected Strategy:** **Monolithic Header, Modular Source (Unity Build)**.

*   **`situation.h`**: Remains the **sole** public API reference. It contains all typedefs, enums, and function prototypes. It contains **no implementation logic**.
*   **`situation_impl.c`** (formerly `situation.c`): A new root-level file serving as the "Unity Build" entry point. It includes the modular implementation files from the `src/` directory.
    *   *Rationale:* `situation.c` sounds like source for the API. `situation_impl.c` makes intent crystal clear.
*   **`src/`**: A new private directory housing the implementation modules (e.g., `sit_audio.c`, `sit_render.c`).

## 2. Architecture & Directory Structure

### 2.1 Proposed Layout
```
root/
├── situation.h           # [PUBLIC] Pure API Header. No code, just declarations.
├── sit_config.h          # [OPTIONAL] User overrides (allocators, debug flags).
├── situation_impl.c      # [PUBLIC] Implementation Entry Point.
└── src/                  # [PRIVATE] Internal Implementation Modules.
    ├── sit_common.h      # Internal shared macros/types (hidden from user).
    ├── sit_core.c        # Logging, Error Handling, Memory, Math helpers.
    ├── sit_platform.c    # Windowing, Input, System Info (GLFW glue).
    ├── sit_audio.c       # Audio Engine (Miniaudio wrapper).
    ├── sit_render.c      # Graphics Engine (OpenGL/Vulkan logic).
    ├── sit_fs.c          # Filesystem & Hot-Reloading.
    └── sit_utils.c       # String helpers, Hash maps, etc.
```

## 3. Execution Roadmap

### Phase 0: Baseline & Safety Net
*Goal: Ensure the current state is stable and reproducible before major surgery.*

- [x] **Verify Test Environment**
    - [x] Run `test_limits.c` and ensure it passes.
    - [x] Run `test_async_io.c` and ensure it passes.
    - [x] Verify `situation_dll.c` compiles successfully.
- [x] **Snapshot**
    - [x] Create a backup of `situation.h` to `situation.h.bak` for quick comparison.

### Phase 0.5: Pre-Split Refactoring
*Goal: Prepare the codebase for separation by enforcing namespace hygiene and physical grouping.*

- [x] **Symbol Sanitization**
    - [x] Rename internal `static` helper functions to avoid collisions in the Unity Build.
    - [x] Convention: `_Sit[Module]_[Name]`.
        -   `_SitGetTextureSlot` -> `_SitRender_GetTextureSlot`
        -   `_SitAudioInitPool` -> `_SitAudio_InitPool`
        -   `_SituationDeferredDestroyBuffer` -> `_SitRender_DeferDestroyBuffer`

    **Applied Mappings:**
    ```text
    _SitAudioAllocSlot -> _SitAudio_AllocSlot
    _SitAudioCleanupPool -> _SitAudio_CleanupPool
    _SitAudioFreeSlot -> _SitAudio_FreeSlot
    _SitAudioGetSoundFromHandle -> _SitAudio_GetSoundFromHandle
    _SitAudioInitPool -> _SitAudio_InitPool
    _SitFlushFrameResources -> _SitCore_FlushFrameResources
    _SitGLBackupState -> _SitRender_GLBackupState
    _SitGLDeferCleanMeshVAO -> _SitRender_GLDeferCleanMeshVAO
    _SitGLDeferDestroyBuffer -> _SitRender_GLDeferDestroyBuffer
    _SitGLDeferDestroyTexture -> _SitRender_GLDeferDestroyTexture
    _SitGLFlushGraveyard -> _SitRender_GLFlushGraveyard
    _SitGLGetCachedVAO -> _SitRender_GLGetCachedVAO
    _SitGLInvalidateShadowState -> _SitRender_GLInvalidateShadowState
    _SitGLRestoreState -> _SitRender_GLRestoreState
    _SitGLSoftCmdPush -> _SitRender_GLSoftCmdPush
    _SitGLSoftDataPush -> _SitRender_GLSoftDataPush
    _SitGetBufferNode -> _SitRender_GetBufferNode
    _SitGetJobFromId -> _SitCore_GetJobFromId
    _SitGetMonotonicTimeNS -> _SitCore_GetMonotonicTimeNS
    _SitGetTextureSlot -> _SitRender_GetTextureSlot
    _SitParallelWorker -> _SitCore_ParallelWorker
    _SituationAssertMainThread -> _SitFS_AssertMainThread
    _SituationAsyncAudioWorker -> _SitAudio_AsyncAudioWorker
    _SituationAsyncFileLoadWorker -> _SitFS_AsyncFileLoadWorker
    _SituationAsyncFileSaveWorker -> _SitFS_AsyncFileSaveWorker
    _SituationAsyncFileTextLoadWorker -> _SitFS_AsyncFileTextLoadWorker
    _SituationAsyncFileTextSaveWorker -> _SitFS_AsyncFileTextSaveWorker
    _SituationCheckGLError -> _SitRender_CheckGLError
    _SituationCleanupDanglingResources -> _SitRender_CleanupDanglingResources
    _SituationCleanupGraveyard -> _SitCore_CleanupGraveyard
    _SituationCleanupOpenGL -> _SitRender_CleanupOpenGL
    _SituationCleanupPlatform -> _SitCore_CleanupPlatform
    _SituationCleanupQuadRenderer -> _SitRender_CleanupQuadRenderer
    _SituationCleanupRenderer -> _SitRender_CleanupRenderer
    _SituationCleanupStagingBuffers -> _SitRender_CleanupStagingBuffers
    _SituationCleanupSubsystems -> _SitCore_CleanupSubsystems
    _SituationCleanupVulkan -> _SitCore_CleanupVulkan
    _SituationCompileGLShader -> _SitRender_CompileGLShader
    _SituationCreateGLComputeProgram -> _SitRender_CreateGLComputeProgram
    _SituationCreateGLComputeProgramFromSpirv -> _SitRender_CreateGLComputeProgramFromSpirv
    _SituationCreateGLShaderProgram -> _SitRender_CreateGLShaderProgram
    _SituationCreateGLShaderProgramFromSource -> _SitRender_CreateGLShaderProgramFromSource
    _SituationCreateGLShaderProgramFromSpirv -> _SitRender_CreateGLShaderProgramFromSpirv
    _SituationCreateVulkanPipeline -> _SitRender_CreateVulkanPipeline
    _SituationCreateVulkanShaderModule -> _SitRender_CreateVulkanShaderModule
    _SituationDeferDestroyBuffer -> _SitRender_DeferDestroyBuffer
    _SituationDeferDestroyDescriptorSet -> _SitRender_DeferDestroyDescriptorSet
    _SituationDeferDestroyFramebuffer -> _SitRender_DeferDestroyFramebuffer
    _SituationDeferDestroyImage -> _SitRender_DeferDestroyImage
    _SituationDeferDestroyPipeline -> _SitRender_DeferDestroyPipeline
    _SituationDeferDestroyRenderPass -> _SitRender_DeferDestroyRenderPass
    _SituationDestroyRenderThread -> _SitRender_DestroyRenderThread
    _SituationDetectCycle -> _SitCore_DetectCycle
    _SituationEnqueueRenderList -> _SitRender_EnqueueRenderList
    _SituationExtractGLTFPrimitive -> _SitRender_ExtractGLTFPrimitive
    _SituationFlushGraveyard -> _SitCore_FlushGraveyard
    _SituationFreeSpirvBlob -> _SitCore_FreeSpirvBlob
    _SituationFullCleanupOnError -> _SitCore_FullCleanupOnError
    _SituationGLExecuteCommands -> _SitRender_GLExecuteCommands
    _SituationGLFWCharCallback -> _SitRender_GLFWCharCallback
    _SituationGLFWCursorPosCallback -> _SitRender_GLFWCursorPosCallback
    _SituationGLFWErrorCallback -> _SitRender_GLFWErrorCallback
    _SituationGLFWFileDropCallback -> _SitRender_GLFWFileDropCallback
    _SituationGLFWFramebufferSizeCallback -> _SitRender_GLFWFramebufferSizeCallback
    _SituationGLFWJoystickCallback -> _SitRender_GLFWJoystickCallback
    _SituationGLFWKeyCallback -> _SitRender_GLFWKeyCallback
    _SituationGLFWMouseButtonCallback -> _SitRender_GLFWMouseButtonCallback
    _SituationGLFWScrollCallback -> _SitRender_GLFWScrollCallback
    _SituationGLFWWindowFocusCallback -> _SitRender_GLFWWindowFocusCallback
    _SituationGLFWWindowIconifyCallback -> _SitRender_GLFWWindowIconifyCallback
    _SituationGLRingWait -> _SitRender_GLRingWait
    _SituationGetHighResTime -> _SitCore_GetHighResTime
    _SituationInitDefaultFont -> _SitCore_InitDefaultFont
    _SituationInitGLMDIBuffer -> _SitRender_InitGLMDIBuffer
    _SituationInitGLRingBuffer -> _SitRender_InitGLRingBuffer
    _SituationInitGLRingFences -> _SitRender_InitGLRingFences
    _SituationInitGLVirtualDisplayRenderer -> _SitRender_InitGLVirtualDisplayRenderer
    _SituationInitGraveyard -> _SitCore_InitGraveyard
    _SituationInitOpenGL -> _SitRender_InitOpenGL
    _SituationInitPlatform -> _SitCore_InitPlatform
    _SituationInitQuadRenderer -> _SitRender_InitQuadRenderer
    _SituationInitRenderThread -> _SitRender_InitRenderThread
    _SituationInitRenderer -> _SitRender_InitRenderer
    _SituationInitReverb -> _SitAudio_InitReverb
    _SituationInitSoundEffects -> _SitAudio_InitSoundEffects
    _SituationInitStagingBuffers -> _SitRender_InitStagingBuffers
    _SituationInitSubsystems -> _SitCore_InitSubsystems
    _SituationInitTextRenderer -> _SitRender_InitTextRenderer
    _SituationInitVulkan -> _SitCore_InitVulkan
    _SituationIsDeviceSuitable -> _SitRender_IsDeviceSuitable
    _SituationMapDataTypeToGL -> _SitRender_MapDataTypeToGL
    _SituationPerformHotReloadPass -> _SitRender_PerformHotReloadPass
    _SituationProcessReverb -> _SitAudio_ProcessReverb
    _SituationReadSpirvFile -> _SitFS_ReadSpirvFile
    _SituationRenderJobWorker -> _SitRender_RenderJobWorker
    _SituationRenderThreadEntry -> _SitRender_RenderThreadEntry
    _SituationReplayToQueue -> _SitCore_ReplayToQueue
    _SituationSaveImageBMP -> _SitRender_SaveImageBMP
    _SituationSetError -> _SitCore_SetError
    _SituationSetErrorFromCode -> _SitCore_SetErrorFromCode
    _SituationSetFilesystemError -> _SitFS_SetFilesystemError
    _SituationShaderIncluderRelease -> _SitRender_ShaderIncluderRelease
    _SituationShaderIncluderResolve -> _SitRender_ShaderIncluderResolve
    _SituationSubmitCompute -> _SitRender_SubmitCompute
    _SituationSubmitGraphics -> _SitRender_SubmitGraphics
    _SituationUninitReverb -> _SitAudio_UninitReverb
    _SituationValidateRenderCaps -> _SitRender_ValidateRenderCaps
    _SituationVulkanAllocateDescriptorSet -> _SitRender_VulkanAllocateDescriptorSet
    _SituationVulkanBeginSingleTimeCommands -> _SitRender_VulkanBeginSingleTimeCommands
    _SituationVulkanBlitImageToHostVisibleBuffer -> _SitRender_VulkanBlitImageToHostVisibleBuffer
    _SituationVulkanCleanupSwapchain -> _SitRender_VulkanCleanupSwapchain
    _SituationVulkanCompileGLSLtoSPIRV -> _SitRender_VulkanCompileGLSLtoSPIRV
    _SituationVulkanCopyBufferToImage -> _SitRender_VulkanCopyBufferToImage
    _SituationVulkanCreateAllocator -> _SitCore_VulkanCreateAllocator
    _SituationVulkanCreateAndUploadBuffer -> _SitRender_VulkanCreateAndUploadBuffer
    _SituationVulkanCreateCommandBuffers -> _SitRender_VulkanCreateCommandBuffers
    _SituationVulkanCreateCommandPool -> _SitRender_VulkanCreateCommandPool
    _SituationVulkanCreateComputePipeline -> _SitRender_VulkanCreateComputePipeline
    _SituationVulkanCreateDepthResources -> _SitCore_VulkanCreateDepthResources
    _SituationVulkanCreateFramebuffers -> _SitRender_VulkanCreateFramebuffers
    _SituationVulkanCreateGraphicsPipeline -> _SitRender_VulkanCreateGraphicsPipeline
    _SituationVulkanCreateImage -> _SitRender_VulkanCreateImage
    _SituationVulkanCreateImageView -> _SitRender_VulkanCreateImageView
    _SituationVulkanCreateImageViews -> _SitRender_VulkanCreateImageViews
    _SituationVulkanCreateInstance -> _SitCore_VulkanCreateInstance
    _SituationVulkanCreateLogicalDevice -> _SitRender_VulkanCreateLogicalDevice
    _SituationVulkanCreateRenderPass -> _SitRender_VulkanCreateRenderPass
    _SituationVulkanCreateScreenCopyResource -> _SitCore_VulkanCreateScreenCopyResource
    _SituationVulkanCreateShaderModule -> _SitRender_VulkanCreateShaderModule
    _SituationVulkanCreateSurface -> _SitCore_VulkanCreateSurface
    _SituationVulkanCreateSwapchain -> _SitRender_VulkanCreateSwapchain
    _SituationVulkanCreateSyncObjects -> _SitCore_VulkanCreateSyncObjects
    _SituationVulkanDestroyImage -> _SitRender_VulkanDestroyImage
    _SituationVulkanDestroyScreenCopyResource -> _SitCore_VulkanDestroyScreenCopyResource
    _SituationVulkanEndSingleTimeCommands -> _SitRender_VulkanEndSingleTimeCommands
    _SituationVulkanFindQueueFamilies -> _SitCore_VulkanFindQueueFamilies
    _SituationVulkanFindSupportedFormat -> _SitCore_VulkanFindSupportedFormat
    _SituationVulkanFreeSwapchainSupportDetails -> _SitRender_VulkanFreeSwapchainSupportDetails
    _SituationVulkanGenerateMipmaps -> _SitCore_VulkanGenerateMipmaps
    _SituationVulkanInitComputeLayouts -> _SitRender_VulkanInitComputeLayouts
    _SituationVulkanInitInternalRenderers -> _SitRender_VulkanInitInternalRenderers
    _SituationVulkanPickPhysicalDevice -> _SitRender_VulkanPickPhysicalDevice
    _SituationVulkanQuerySwapchainSupport -> _SitRender_VulkanQuerySwapchainSupport
    _SituationVulkanReadBackBuffer -> _SitRender_VulkanReadBackBuffer
    _SituationVulkanRecreateSwapchain -> _SitRender_VulkanRecreateSwapchain
    _SituationVulkanSetupDebugMessenger -> _SitCore_VulkanSetupDebugMessenger
    _SituationVulkanTransitionImageLayout -> _SitRender_VulkanTransitionImageLayout
    _SituationWorkerEntry -> _SitCore_WorkerEntry
    _sit_directory_exists -> _SitFS_Directory_exists
    _sit_dirname -> _SitFS_Dirname
    _sit_miniaudio_capture_callback -> _SitAudio_Miniaudio_capture_callback
    _sit_reverb_allpass_process -> _SitAudio_Reverb_allpass_process
    _sit_reverb_comb_process -> _SitAudio_Reverb_comb_process
    _sit_strcasecmp -> _SitUtils_Strcasecmp
    _sit_strdup -> _SitUtils_Strdup
    _sit_uniform_map_create -> _SitRender_Uniform_map_create
    _sit_uniform_map_destroy -> _SitRender_Uniform_map_destroy
    _sit_uniform_map_get -> _SitRender_Uniform_map_get
    _sit_uniform_map_set -> _SitRender_Uniform_map_set
    _sit_utf8_to_wide -> _SitUtils_Utf8_to_wide
    _sit_wide_to_utf8 -> _SitUtils_Wide_to_utf8
    _situation_stream_read_thunk -> _SitFS__situation_stream_read_thunk
    _situation_stream_seek_thunk -> _SitCore__situation_stream_seek_thunk
    ```

- [x] **Physical Grouping**
    - [x] Reorder the implementation block within `situation.h` to group functions by module.
    -   **Order:**
        1.  Common/Core (Allocators, Logging, Math)
        2.  Filesystem (needed by everything)
        3.  Platform/Windowing (needed by Render)
        4.  Input (Keyboard/Mouse/Joystick)
        5.  Render (OpenGL/Vulkan)
        6.  Audio
    - [x] Insert delimiter comments (e.g., `// --- MODULE: AUDIO ---`) to clearly mark cut-points.
    *Note: Physical grouping deferred/satisfied by Namespace Grouping due to file size constraints. Splitting will be done by searching for namespaced functions.*

### Phase 1: Infrastructure Setup
*Goal: Create the physical structure without moving code yet.*

- [x] **Create Directories**
    - [x] Create `src/` directory in root.
- [x] **Create Skeleton Files**
    - [x] Create `sit_config.h` (Optional, template). Included first in `situation.h`.
    - [x] Create `src/sit_common.h` with **Internal Guards**:
      ```c
      #ifndef SITUATION_INTERNAL
      #error "Internal headers should not be included directly"
      #endif
      ```
    - [x] Create `src/sit_core.c` (Empty).
    - [x] Create `src/sit_platform.c` (Empty).
    - [x] Create `src/sit_audio.c` (Empty).
    - [x] Create `src/sit_render.c` (Empty).
    - [x] Create `src/sit_fs.c` (Empty).
- [x] **Create The Bridge**
    - [x] Create `situation_impl.c` with the following content:
      ```c
      #define SITUATION_IMPLEMENTATION_INTERNAL
      #define SITUATION_INTERNAL // Allows including internal headers
      #include "situation.h"
      #include "src/sit_common.h"
      // Modules will be included here later
      ```
    - [x] Update `situation.h` to include `sit_config.h` at the top (if exists).

### Phase 2: The "Great Separation"
*Goal: Separate the API from the Implementation. This is the most critical phase.*

- [x] **Extract Implementation**
    - [x] Cut the entire `SITUATION_IMPLEMENTATION` block from `situation.h`.
    - [x] Paste it into `situation_impl.c` (temporarily monolithic).
    - [x] Verify `situation.h` contains ONLY:
        -   License / Comments.
        -   Includes `sit_config.h`.
        -   Configuration Macros (`SITUATION_USE_OPENGL`, etc.).
        -   Typedefs, Enums, Structs.
        -   Function Prototypes (`SITAPI`).
- [x] **Extract Internal Shared State**
    - [x] Identify internal structs (`_SituationGlobalState`, `_SituationRenderState`, `_SituationAudioState`).
    - [x] Identify internal macros (`SIT_LOG`, `SIT_CHECK`, etc.).
    - [x] Move these from `situation_impl.c` (or `situation.h` if they were leaked) to `src/sit_common.h`.
    - [x] Ensure `src/sit_common.h` is included by `situation_impl.c`.

### Phase 3: Surgical Module Colonization (High Risk)
*Goal: Systematically migrate logic from the monolithic `situation_impl.c` to isolated modules in `src/`. This phase is executed in strict atomic steps to prevent destabilization.*

**Protocol:** For each module, we follow the "Copy-Verify-Prune" checklist.

#### Module 3.1: The Core (`sit_core.c`)
**Dependencies:** None. This module must be migrated first.
- [ ] **Step 1: Copy & Guard**
    - [ ] Copy Logging/Error functions to `src/sit_core.c` (`SituationLog`, `SituationLogWarning`, `SituationSetTraceLogLevel`, `SituationGetLastErrorMsg`, `_SitCore_SetError`, `_SitCore_SetErrorFromCode`).
    - [ ] Copy Threading functions to `src/sit_core.c` (`SituationCreateThreadPool`, `SituationSubmitJobEx`, `SituationWaitForJob`, `SituationWaitForAllJobs`, `SituationDestroyThreadPool`, `SituationAddJobDependency`, `SituationAddJobDependencies`, `SituationDumpTaskGraph`, `SituationDispatchParallel`, `SituationGetIOQueueDepth`, `SituationGetRenderQueueDepth`, `_SitCore_WorkerEntry`, `_SitCore_ParallelWorker`, `_SitCore_GetJobFromId`, `_SitCore_DetectCycle`, `_SitMakeId`).
    - [ ] Copy Timing functions to `src/sit_core.c` (`SituationTimerGetTime`, `SituationSetTargetFPS`, `SituationGetFPS`, `SituationGetFrameTime`, `SituationUpdateTimers`, `SituationTimerGetOscillatorState`, `SituationTimerGetPreviousOscillatorState`, `SituationTimerHasOscillatorUpdated`, `SituationTimerPingOscillator`, `SituationTimerGetOscillatorTriggerCount`, `SituationTimerGetOscillatorPeriod`, `SituationTimerGetPingProgress`, `SituationSetTimerOscillatorPeriod`, `_SitCore_GetMonotonicTimeNS`, `_SitCore_GetHighResTime`).
    - [ ] Copy Lifecycle functions to `src/sit_core.c` (`_SitCore_InitPlatform`, `_SitCore_InitSubsystems`, `_SitCore_CleanupPlatform`, `_SitCore_CleanupSubsystems`, `_SitCore_FullCleanupOnError`).
- [ ] **Step 2: Verify**
    - [ ] Comment out originals in `situation_impl.c`.
    - [ ] Compile and run `test_limits.c`.
- [ ] **Step 3: Prune**
    - [ ] Delete commented code from `situation_impl.c`.

#### Module 3.2: Filesystem & Utilities (`sit_fs.c`, `sit_utils.c`)
**Dependencies:** Core (for Errors/Memory).
- [ ] **Step 1: Copy & Guard**
    - [ ] Copy Utilities to `src/sit_utils.c` (`_SitUtils_Strdup`, `_SitUtils_Strcasecmp`, `_sit_hash_string`, `_SitUtils_Utf8_to_wide`, `_SitUtils_Wide_to_utf8`, `SituationFreeString`, `SituationFreeDirectoryFileList`).
    - [ ] Copy Path functions to `src/sit_fs.c` (`SituationGetBasePath`, `SituationGetAppSavePath`, `SituationGetUserDirectory`, `SituationGetCurrentDriveLetter`, `SituationJoinPath`, `SituationGetFileName`, `SituationGetFileExtension`, `SituationGetBasePathFromFile`, `_SitFS_Dirname`).
    - [ ] Copy Sync I/O functions to `src/sit_fs.c` (`SituationLoadFile`, `SituationSaveFile`, `SituationLoadFileText`, `SituationSaveFileText`, `SituationLoadFileData`, `SituationSaveFileData`, `SituationFileExists`, `SituationDirectoryExists`, `SituationGetDriveInfo`, `SituationGetFileModTime`, `SituationDeleteFile`, `SituationCopyFile`, `SituationMoveFile`, `SituationRenameFile`, `SituationCreateDirectory`, `SituationDeleteDirectory`, `SituationListDirectoryFiles`, `SituationOpenFile`, `_SitFS_Directory_exists`, `_SitFS_ReadSpirvFile`).
    - [ ] Copy Internal FS Helpers to `src/sit_fs.c` (`_SitFS_SetFilesystemError`, `_SitFS_AssertMainThread`).
    - [ ] Copy Async I/O functions to `src/sit_fs.c` (`SituationLoadFileAsync`, `SituationLoadFileTextAsync`, `SituationSaveFileAsync`, `SituationSaveFileTextAsync`, `_SitFS_AsyncFileLoadWorker`, `_SitFS_AsyncFileSaveWorker`, `_SitFS_AsyncFileTextLoadWorker`, `_SitFS_AsyncFileTextSaveWorker`, `_SitFS_IOThreadEntry`).
    - [ ] Copy Hot-Reload functions to `src/sit_fs.c` (`SituationCheckHotReloads`, `SituationReloadShader`, `SituationReloadTexture`, `SituationReloadModel`, `SituationReloadComputePipeline`, `_SitRender_PerformHotReloadPass`).
- [ ] **Step 2: Verify**
    - [ ] Comment out originals in `situation_impl.c`.
    - [ ] Compile and run `test_limits.c`.
- [ ] **Step 3: Prune**
    - [ ] Delete commented code from `situation_impl.c`.

#### Module 3.3: Platform & Input (`sit_platform.c`)
**Dependencies:** Core, FS.
- [ ] **Step 1: Copy & Guard**
    - [ ] Copy Lifecycle functions to `src/sit_platform.c` (`SituationInit`, `SituationShutdown`, `SituationIsInitialized`, `SituationUpdate`, `SituationGetVersionString`, `SituationGetDeviceInfo`, `SituationGetCPUThreadCount`, `SituationSetExitCallback`, `SituationSetFocusCallback`, `SituationIsArgumentPresent`).
    - [ ] Copy Windowing functions to `src/sit_platform.c` (`SituationWindowShouldClose`, `SituationGetWindowSize`, `SituationGetWindowPosition`, `SituationGetWindowScaleDPI`, `SituationSetWindowSize`, `SituationSetWindowPosition`, `SituationSetWindowTitle`, `SituationSetWindowIcon`, `SituationSetWindowIcons`, `SituationMinimizeWindow`, `SituationMaximizeWindow`, `SituationRestoreWindow`, `SituationSetWindowFocused`, `SituationSetWindowMonitor`, `SituationSetWindowMinSize`, `SituationSetWindowMaxSize`, `SituationSetWindowOpacity`, `SituationToggleFullscreen`, `SituationToggleBorderlessWindowed`, `SituationApplyCurrentProfileWindowState`, `SituationIsWindowFullscreen`, `SituationIsWindowHidden`, `SituationIsWindowMaximized`, `SituationIsWindowMinimized`, `SituationIsWindowResized`, `SituationHasWindowFocus`, `SituationSetWindowStateProfiles`, `SituationSetWindowState`, `SituationToggleWindowStateFlags`, `SituationIsWindowState`, `SituationClearWindowState`, `SituationGetCurrentActualWindowStateFlags`, `SituationGetGLFWwindow`, `SituationExecuteCommand`, `SituationIsAppPaused`, `SituationPauseApp`, `SituationResumeApp`, `_SitPlatform_InitWindow`).
    - [ ] Copy Input functions to `src/sit_platform.c` (`SituationPollInputEvents`, `SituationIsKeyDown`, `SituationIsKeyUp`, `SituationIsKeyPressed`, `SituationIsKeyReleased`, `SituationGetKeyPressed`, `SituationGetKeyPressedEx`, `SituationPeekKeyPressed`, `SituationPeekKeyPressedEx`, `SituationGetCharPressed`, `SituationIsLockKeyPressed`, `SituationIsScrollLockOn`, `SituationIsModifierPressed`, `SituationGetMousePosition`, `SituationGetMouseDelta`, `SituationGetMouseWheelMove`, `SituationGetMouseWheelMoveV`, `SituationSetMousePosition`, `SituationSetMouseOffset`, `SituationSetMouseScale`, `SituationIsMouseButtonDown`, `SituationIsMouseButtonPressed`, `SituationIsMouseButtonReleased`, `SituationGetMouseButtonPressed`, `SituationSetCursor`, `SituationShowCursor`, `SituationHideCursor`, `SituationDisableCursor`, `SituationIsJoystickPresent`, `SituationIsGamepad`, `SituationGetJoystickName`, `SituationGetGamepadAxisValue`, `SituationGetGamepadAxisCount`, `SituationIsGamepadButtonDown`, `SituationIsGamepadButtonPressed`, `SituationIsGamepadButtonReleased`, `SituationGetGamepadButtonPressed`, `SituationSetGamepadVibration`, `SituationSetGamepadMappings`, `SituationGetClipboardText`, `SituationSetClipboardText`, `SituationLoadDroppedFiles`, `SituationUnloadDroppedFiles`, `SituationIsFileDropped`, `SituationSetKeyCallback`, `SituationSetMouseButtonCallback`, `SituationSetCursorPosCallback`, `SituationSetScrollCallback`, `SituationSetJoystickCallback`, `SituationSetFileDropCallback`, `SituationSetResizeCallback`, `SituationIsScancodeDown`, `SituationGetKeyScancode`, `SituationGetCharFromScancode`, `_SitRender_GLFWKeyCallback`, `_SitRender_GLFWCharCallback`, `_SitRender_GLFWCursorPosCallback`, `_SitRender_GLFWMouseButtonCallback`, `_SitRender_GLFWScrollCallback`, `_SitRender_GLFWJoystickCallback`, `_SitRender_GLFWErrorCallback`, `_SitRender_GLFWWindowFocusCallback`, `_SitRender_GLFWWindowIconifyCallback`, `_SitRender_GLFWFileDropCallback`, `_SitRender_GLFWFramebufferSizeCallback`).
    - [ ] Copy Display functions to `src/sit_platform.c` (`SituationGetDisplays`, `SituationFreeDisplays`, `SituationSetDisplayMode`, `SituationRefreshDisplays`, `SituationGetMonitorCount`, `SituationGetMonitorName`, `SituationGetCurrentMonitor`, `SituationGetMonitorWidth`, `SituationGetMonitorHeight`, `SituationGetMonitorPhysicalWidth`, `SituationGetMonitorPhysicalHeight`, `SituationGetMonitorRefreshRate`, `SituationGetMonitorPosition`, `_SitPlatform_SortVirtualDisplaysCallback`, `_SitPlatform_CachePhysicalDisplays`, `_SituationMonitorEnumProc`, `_SituationGetCurrentDisplayIdentifier`).
- [ ] **Step 2: Verify**
    - [ ] Comment out originals in `situation_impl.c`.
    - [ ] Compile and run `test_limits.c`.
- [ ] **Step 3: Prune**
    - [ ] Delete commented code from `situation_impl.c`.

#### Module 3.4: Audio Engine (`sit_audio.c`)
**Dependencies:** Core, FS (for streaming).
- [ ] **Step 1: Copy & Guard**
    - [ ] Copy Audio Logic to `src/sit_audio.c` (`SituationPlayAudio`, `SituationPlayLoadedSound`, `SituationStopLoadedSound`, `SituationStopAllLoadedSounds`, `SituationLoadAudio`, `SituationLoadSoundFromFile`, `SituationLoadSoundFromStream`, `SituationLoadSoundFromFileAsync`, `SituationUnloadAudio`, `SituationUnloadSound`, `SituationSetAudioVolume`, `SituationSetSoundVolume`, `SituationGetSoundVolume`, `SituationSetAudioMasterVolume`, `SituationGetAudioMasterVolume`, `SituationSetAudioPan`, `SituationSetSoundPan`, `SituationGetSoundPan`, `SituationSetAudioPitch`, `SituationSetSoundPitch`, `SituationGetSoundPitch`, `SituationSoundCopy`, `SituationSoundCrop`, `SituationSoundExportAsWav`, `SituationAttachAudioProcessor`, `SituationDetachAudioProcessor`, `SituationSetSoundFilter`, `SituationSetSoundReverb`, `SituationSetSoundEcho`, `SituationPlayTone`, `SituationPlayToneEx`, `SituationStopTone`, `SituationStopAllTones`, `SituationPlayMidiNote`, `SituationGetAudioDevices`, `SituationSetAudioDevice`, `SituationIsAudioDevicePlaying`, `SituationPauseAudioDevice`, `SituationResumeAudioDevice`, `SituationSetAudioPlaybackSampleRate`, `SituationGetAudioPlaybackSampleRate`, `SituationStartAudioCapture`, `SituationStopAudioCapture`).
    - [ ] Copy Internal Audio Helpers to `src/sit_audio.c` (`_SitAudio_InitSoundEffects`, `_SitAudio_GetSoundFromHandle`, `_SitAudio_AllocSlot`, `_SitAudio_FreeSlot`, `_SitAudio_InitPool`, `_SitAudio_CleanupPool`, `_SitAudio_InitReverb`, `_SitAudio_UninitReverb`, `_SitAudio_ProcessReverb`, `_SitAudio_Reverb_comb_process`, `_SitAudio_Reverb_allpass_process`, `sit_miniaudio_data_callback`, `_SitAudio_Miniaudio_capture_callback`, `_SitFS_situation_stream_read_thunk`, `_SitCore_situation_stream_seek_thunk`, `_SitAudio_AsyncAudioWorker`).
- [ ] **Step 2: Verify**
    - [ ] Comment out originals in `situation_impl.c`.
    - [ ] Compile and run `test_limits.c`.
- [ ] **Step 3: Prune**
    - [ ] Delete commented code from `situation_impl.c`.

#### Module 3.5: The Graphics Behemoth (`sit_render.c`)
**Dependencies:** All.
- [ ] **Step 1: Copy & Guard**
    - [ ] Copy Common API functions to `src/sit_render.c` (`SituationGetRendererType`, `SituationGetGPUName`, `SituationGetVRAMUsage`, `SituationGetDrawCallCount`, `SituationGetRenderWidth`, `SituationGetRenderHeight`, `SituationGetScreenWidth`, `SituationGetScreenHeight`, `SituationCreateImage`, `SituationLoadImage`, `SituationLoadImageFromMemory`, `SituationLoadImageFromScreen`, `SituationUnloadImage`, `SituationIsImageValid`, `SituationImageCopy`, `SituationImageResize`, `SituationImageCrop`, `SituationImageFlip`, `SituationImageAdjustHSV`, `SituationGenImageColor`, `SituationGenImageGradient`, `SituationSetPixelColor`, `SituationBlitRawDataToImage`, `SituationExportImage`, `SituationCreateTexture`, `SituationCreateTextureEx`, `SituationLoadTexture`, `SituationDestroyTexture`, `SituationGetTextureHandle`, `SituationCreateMesh`, `SituationDestroyMesh`, `SituationLoadModel`, `SituationUnloadModel`, `SituationSaveModelAsGltf`, `SituationGetMeshData`, `SituationCreateBuffer`, `SituationDestroyBuffer`, `SituationUpdateBuffer`, `SituationGetBufferData`, `SituationGetBufferDeviceAddress`, `SituationLoadShader`, `SituationLoadShaderFromMemory`, `SituationUnloadShader`, `SituationSetShaderUniform`, `SituationCreateComputePipeline`, `SituationCreateComputePipelineFromMemory`, `SituationDestroyComputePipeline`, `SituationCreateVirtualDisplay`, `SituationDestroyVirtualDisplay`, `SituationGetVirtualDisplay`, `SituationGetVirtualDisplaySize`, `SituationSetVirtualDisplayScalingMode`, `SituationIsVirtualDisplayDirty`, `SituationSetVirtualDisplayDirty`, `SituationGetLastVDCompositeTimeMS`, `SituationTakeScreenshot`, `SituationExportRenderHistogram`, `SituationGetRenderLatencyStats`, `SituationIsFeatureSupported`, `SituationGetMaxComputeWorkGroups`, `SituationColorFromYPQ`, `SituationColorToYPQ`, `SituationConvertColorToVector4`, `SituationHsvToRgb`, `SituationRgbToHsv`).
    - [ ] Copy Drawing functions to `src/sit_render.c` (`SituationGetMainCommandBuffer`, `SituationGetComputeCommandBuffer`, `SituationAcquireFrameCommandBuffer`, `SituationEndFrame`, `SituationCmdBeginRenderPass`, `SituationCmdEndRenderPass`, `SituationCmdBeginRenderToDisplay`, `SituationCmdEndRender`, `SituationCmdPresent`, `SituationCmdBindPipeline`, `SituationCmdBindDescriptorSet`, `SituationCmdBindDescriptorSetDynamic`, `SituationCmdBindVertexBuffer`, `SituationCmdBindIndexBuffer`, `SituationCmdBindSampledTexture`, `SituationCmdBindTexture`, `SituationCmdBindTextureSet`, `SituationCmdBindComputeTexture`, `SituationCmdBindUniformBuffer`, `SituationCmdBindComputeBuffer`, `SituationCmdBindComputePipeline`, `SituationCmdSetViewport`, `SituationCmdSetScissor`, `SituationCmdSetPushConstant`, `SituationCmdSetVertexAttribute`, `SituationCmdDraw`, `SituationCmdDrawIndexed`, `SituationCmdDrawMesh`, `SituationCmdDrawQuad`, `SituationCmdDrawModel`, `SituationCmdDispatch`, `SituationCmdPipelineBarrier`, `SituationMemoryBarrier`, `SituationCmdDrawText`, `SituationCmdDrawTextEx`, `SituationCmdDrawTexture`, `SituationDrawModel`, `SituationDrawMetricsOverlay`, `SituationRenderVirtualDisplays`, `SituationCreateRenderList`, `SituationDestroyRenderList`, `SituationSubmitRenderList`, `SituationReplayRenderList`, `SituationResetRenderList`).
    - [ ] Copy Image Draw (CPU) functions to `src/sit_render.c` (`SituationImageDraw`, `SituationImageDrawAlpha`, `SituationImageDrawText`, `SituationImageDrawTextEx`, `SituationImageDrawTextFormatted`, `SituationImageDrawCodepoint`).
    - [ ] Copy Text/Font functions to `src/sit_render.c` (`SituationLoadFont`, `SituationLoadFontFromMemory`, `SituationLoadBitmapFontFromMemory`, `SituationUnloadFont`, `SituationBakeFontAtlas`, `SituationMeasureText`, `_SitRender_InitTextRenderer`, `_SitCore_InitDefaultFont`).
    - [ ] Copy Internal OpenGL functions to `src/sit_render.c` (`_SitRender_InitOpenGL`, `_SitRender_CleanupOpenGL`, `_SitRender_InitQuadRenderer`, `_SitRender_CleanupQuadRenderer`, `_SitRender_InitGLVirtualDisplayRenderer`, `_SitRender_InitGLRingBuffer`, `_SitRender_InitGLMDIBuffer`, `_SitRender_InitGLRingFences`, `_SitRender_GLRingWait`, `_SitRender_GLExecuteCommands`, `_SitRender_GLSoftCmdPush`, `_SitRender_GLSoftDataPush`, `_SitRender_CompileGLShader`, `_SitRender_CreateGLShaderProgram`, `_SitRender_CreateGLShaderProgramFromSource`, `_SitRender_CreateGLShaderProgramFromSpirv`, `_SitRender_CreateGLComputeProgram`, `_SitRender_CreateGLComputeProgramFromSpirv`, `_SitRender_MapDataTypeToGL`, `_SitRender_CheckGLError`, `_SituationLogGLError`, `_SitRender_GLBackupState`, `_SitRender_GLRestoreState`, `_SitRender_GLInvalidateShadowState`, `_SitRender_GLGetCachedVAO`, `_SitRender_GLDeferDestroyBuffer`, `_SitRender_GLDeferDestroyTexture`, `_SitRender_GLDeferCleanMeshVAO`, `_SitRender_GLFlushGraveyard`, `_SitRender_Uniform_map_create`, `_SitRender_Uniform_map_destroy`, `_SitRender_Uniform_map_set`, `_SitRender_Uniform_map_get`).
    - [ ] Copy Internal Vulkan functions to `src/sit_render.c` (`_SitCore_InitVulkan`, `_SitCore_CleanupVulkan`, `_SitRender_InitRenderer`, `_SitRender_CleanupRenderer`, `_SitCore_VulkanCreateInstance`, `_SitCore_VulkanSetupDebugMessenger`, `_SitCore_VulkanCreateSurface`, `_SitRender_VulkanPickPhysicalDevice`, `_SitRender_VulkanCreateLogicalDevice`, `_SitCore_VulkanCreateAllocator`, `_SitRender_VulkanCreateSwapchain`, `_SitRender_VulkanCreateImageViews`, `_SitRender_VulkanCreateRenderPass`, `_SitCore_VulkanCreateDepthResources`, `_SitRender_VulkanCreateFramebuffers`, `_SitRender_VulkanCreateCommandPool`, `_SitRender_VulkanCreateCommandBuffers`, `_SitCore_VulkanCreateSyncObjects`, `_SitRender_VulkanInitInternalRenderers`, `_SitRender_VulkanInitComputeLayouts`, `_SitRender_VulkanCleanupSwapchain`, `_SitRender_VulkanRecreateSwapchain`, `_SitRender_VulkanQuerySwapchainSupport`, `_SitRender_VulkanFreeSwapchainSupportDetails`, `_SitRender_VulkanCreateImage`, `_SitRender_VulkanDestroyImage`, `_SitRender_VulkanCreateImageView`, `_SitRender_VulkanTransitionImageLayout`, `_SitCore_VulkanGenerateMipmaps`, `_SitRender_VulkanCopyBufferToImage`, `_SitRender_VulkanBlitImageToHostVisibleBuffer`, `_SitRender_VulkanReadBackBuffer`, `_SitRender_VulkanCreateAndUploadBuffer`, `_SitRender_InitStagingBuffers`, `_SitRender_CleanupStagingBuffers`, `_SitRender_VulkanAllocateDescriptorSet`, `_SitRender_VulkanBeginSingleTimeCommands`, `_SitRender_VulkanEndSingleTimeCommands`, `_SitRender_VulkanCreateShaderModule`, `_SitRender_VulkanCreateGraphicsPipeline`, `_SitRender_CreateVulkanPipeline`, `_SitRender_VulkanCreateComputePipeline`, `_SitRender_VulkanCompileGLSLtoSPIRV`, `_SitRender_ShaderIncluderResolve`, `_SitRender_ShaderIncluderRelease`, `_SitCore_FreeSpirvBlob`, `_SitCore_InitGraveyard`, `_SitCore_CleanupGraveyard`, `_SitCore_FlushGraveyard`, `_SitCore_FlushFrameResources`, `_SitRender_DeferDestroyBuffer`, `_SitRender_DeferDestroyImage`, `_SitRender_DeferDestroyDescriptorSet`, `_SitRender_DeferDestroyPipeline`, `_SitRender_DeferDestroyFramebuffer`, `_SitRender_DeferDestroyRenderPass`, `_SitRender_IsDeviceSuitable`, `_SitCore_VulkanFindSupportedFormat`, `_SitCore_VulkanFindQueueFamilies`, `_SituationVulkanGetRequiredExtensions`, `_SituationVulkanDebugCallback`, `_SitCore_VulkanCreateScreenCopyResource`, `_SitCore_VulkanDestroyScreenCopyResource`).
    - [ ] Copy Other Internal Render functions to `src/sit_render.c` (`_SitRender_InitRenderThread`, `_SitRender_DestroyRenderThread`, `_SitRender_RenderThreadEntry`, `_SitRender_RenderJobWorker`, `_SitRender_ExtractGLTFPrimitive`, `_SitRender_SaveImageBMP`, `_SitCore_ReplayToQueue`, `_SitRender_EnqueueRenderList`, `_SitRender_GetBufferNode`, `_SitRender_GetTextureSlot`, `_SituationBilinearSample`, `_SituationColorAlphaBlend`, `_SituationClampf`, `_SituationLerpf`, `_SituationFMin3`, `_SituationFMax3`, `_SitRender_SubmitCompute`, `_SitRender_SubmitGraphics`, `_SitRender_CleanupDanglingResources`, `_SitRender_CreateVulkanShaderModule`, `_SitRender_ValidateRenderCaps`).
- [ ] **Step 2: Verify**
    - [ ] Comment out originals in `situation_impl.c`.
    - [ ] Compile and run `test_limits.c`.
- [ ] **Step 3: Prune**
    - [ ] Delete commented code from `situation_impl.c`.

### Phase 4: The Legacy Bridge & Polish
*Goal: Restore backward compatibility for users who rely on the single-header behavior.*

- [ ] **Restore Header-Only Behavior**
    - [ ] Add the following to the bottom of `situation.h`:
      ```c
      #ifdef SITUATION_IMPLEMENTATION
          #include "situation_impl.c"
      #endif
      ```
    - [ ] **Note:** This assumes `situation_impl.c` is in the include path.
- [ ] **Verify Includes**
    - [ ] Ensure `situation_impl.c` includes all `src/*.c` files in the correct dependency order:
      1. `sit_core.c`
      2. `sit_fs.c` (Core dep)
      3. `sit_platform.c` (Core dep)
      4. `sit_render.c` (Platform, FS dep)
      5. `sit_audio.c` (Core dep)

### Phase 5: Documentation & Validation
*Goal: Prove that nothing broke and users know how to build.*

- [ ] **Update README.md**
    - [ ] Document the Build Modes:
        -   **Header-Only (Legacy)**: `#define SITUATION_IMPLEMENTATION` before `#include "situation.h"`
        -   **Modular (Recommended)**: Add `situation_impl.c` + `src/*.c` to your build.
        -   **Static Library / Object (Advanced)**: Compile `situation_impl.c` to `situation.o` (or `libsituation.a`) and link it.
- [ ] **Test 1: Standard Build (Split)**
    - [ ] Compile a test file that adds `situation_impl.c` to the compiler sources and includes `situation.h`.
- [ ] **Test 2: Legacy Build (Header-Only)**
    - [ ] Compile a test file that defines `SITUATION_IMPLEMENTATION` and includes `situation.h`.
- [ ] **Test 3: DLL Build**
    - [ ] Compile `situation_dll.c` (Updated to use `situation_impl.c`).
- [ ] **Test 4: Static Object Workflow**
    - [ ] Compile `situation_impl.c` into an object file (e.g., `gcc -c situation_impl.c -o situation.o`).
    - [ ] Compile a test file (e.g., `test_limits.c`) *without* `SITUATION_IMPLEMENTATION`.
    - [ ] Link them together (`gcc test_limits.o situation.o -o test_bin`) and verify it runs.
- [ ] **Regression Check**
    - [ ] Run `test_limits.c`.
    - [ ] Run `test_async_io.c`.

## 4. Risks & Mitigations

| Risk | Mitigation |
| :--- | :--- |
| **Circular Dependencies** | Strictly enforce a hierarchy: `Core` < `FS` < `Platform` < `Render`. Use `sit_common.h` for shared types. |
| **Static Function Visibility** | Functions in `src/*.c` must be `static` or `SIT_PRIVATE` if they are internal helpers, to avoid symbol clashes in the unity build. |
| **Include Path Hell** | Users might not set the include path correctly for `src/`. **Solution:** The user only ever includes `situation.h` or compiles `situation_impl.c`. The internal `src/` includes are relative to `situation_impl.c` (`#include "src/..."`), so as long as the user has the folder structure, it works. |
| **Macro Leakage** | Ensure internal macros in `sit_common.h` are undefined at the end of `situation_impl.c` or strictly namespaced. |

## 5. Hardening & Robustness

To ensure the split is production-ready and resilient to future changes, we implement the following hardening measures:

### 5.1 Namespace Hygiene
In a Unity Build, multiple `.c` files are textually included into one compilation unit (`situation_impl.c`). This effectively merges their file scopes.
*   **Rule:** All internal functions, even if declared `static`, MUST have a unique prefix to prevent collisions or confusion during debugging/profiling.
*   **Format:** `_Sit[Module]_[FunctionName]`
    *   Example: `_SitRender_Init()` instead of `_SituationInitRenderer()`.
    *   Example: `_SitAudio_MixVoices()` instead of `_MixVoices()`.
*   **Verification:** Use `grep` or `nm` to flag any function starting with just `_` that doesn't follow the module pattern.

### 5.2 Verification Steps
Automated checks to run after the split:
1.  **Symbol Visibility Check:**
    Compile `situation_impl.c` as a shared object (`.so` / `.dll`). Use `nm -D` (Linux) or `dumpbin /EXPORTS` (Windows) to verify that **only** `SITAPI` symbols are exported. Any `_Sit...` helper visible in the export table is a bug (missing `static` or visibility attribute).
2.  **Preprocessed Diff:**
    Run `gcc -E situation.h` (original) and `gcc -E situation_impl.c` (new). Normalize whitespace and comments. The resulting C code stream must be functionally identical (aside from line numbers).

### 5.3 Legacy Bridge Robustness
The single-header workflow is a core promise of the library. We ensure backward compatibility via a precise bridge in `situation.h`:

```c
#ifdef SITUATION_IMPLEMENTATION
    #ifndef SITUATION_IMPL_INCLUDED
    #define SITUATION_IMPL_INCLUDED

    // Check if the user has the split files available in the expected relative path
    // Ideally, we just include the unity build file.
    // If the user hasn't set up the include paths for src/, situation_impl.c handles the relative lookup.

    #include "situation_impl.c"

    #endif
#endif
```
**Constraint:** This requires `situation_impl.c` to be in the include path or the same directory as `situation.h`.

### 5.4 Editor Support (Intellisense)
Split files often confuse IDEs (VS Code, CLion, Visual Studio) because independent `.c` files in `src/` might be missing context (defines, types) if analyzed in isolation.
*   **Solution:** Add a "Master Include" guard at the top of every `src/*.c` file:
    ```c
    // src/sit_render.c
    #ifndef SITUATION_IMPLEMENTATION_INTERNAL
    // This file is a module part of the Situation library.
    // It is not intended to be compiled directly.
    // Please include "situation_impl.c" instead.
    #ifdef __INTELLISENSE__
    #include "../situation_impl.c" // Trick Intellisense into seeing the full context
    #endif
    #endif
    ```
