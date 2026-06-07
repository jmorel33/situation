#!/usr/bin/env python3
"""One-shot: prepend HARDENING comments to Phase 9 void forward decls in situation_impl*.h."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "sit"

SPECIFIC: dict[str, str] = {
    "_SituationSetError": "error sink; sets global error state.",
    "_SituationFullCleanupOnError": "best-effort teardown on init failure.",
    "_SituationCleanupPlatform": "best-effort platform teardown during shutdown.",
    "_SituationCleanupRenderer": "best-effort renderer teardown during shutdown.",
    "_SituationCleanupSubsystems": "best-effort subsystem teardown during shutdown.",
    "_SituationGLFWErrorCallback": "GLFW callback ABI.",
    "_SituationGLFWFileDropCallback": "GLFW callback ABI.",
    "_SituationGLFWWindowFocusCallback": "GLFW callback ABI.",
    "_SituationGLFWWindowMaximizeCallback": "GLFW callback ABI.",
    "_SituationGLFWWindowIconifyCallback": "GLFW callback ABI.",
    "_SituationGLFWFramebufferSizeCallback": "GLFW callback ABI.",
    "_SituationGLFWKeyCallback": "GLFW callback ABI.",
    "_SituationGLFWCharCallback": "GLFW callback ABI.",
    "_SituationGLFWMouseButtonCallback": "GLFW callback ABI.",
    "_SituationGLFWCursorPosCallback": "GLFW callback ABI.",
    "_SituationGLFWScrollCallback": "GLFW callback ABI.",
    "_SituationGLFWJoystickCallback": "GLFW callback ABI.",
    "_SituationRenderJobWorker": "thread-pool job ABI; enqueue failures use _SituationSetErrorFromCode.",
    "sit_miniaudio_data_callback": "miniaudio RT callback ABI.",
    "_SituationUninitReverb": "reverb state teardown; idempotent free.",
    "_SituationProcessReverb": "real-time audio DSP path.",
    "_SituationAsyncFileLoadWorker": "thread-pool job ABI; result via callback/context.",
    "_SituationAsyncFileTextLoadWorker": "thread-pool job ABI; result via callback/context.",
    "_SituationAsyncFileTextSaveWorker": "thread-pool job ABI; result via callback/context.",
    "_SituationAsyncFileSaveWorker": "thread-pool job ABI; result via callback/context.",
    "_SitParallelWorker": "thread-pool parallel dispatch ABI.",
    "_SituationVkAsyncCompileWorker": "shader compile worker ABI.",
    "_SituationAssertMainThread": "debug assert only; sets THREAD_VIOLATION on mismatch.",
    "_SituationVulkanWaitInFlightFencesPump": "bounded shutdown wait with window pump; warnings only.",
    "_SituationVulkanShutdownWaitGpuPump": "shutdown GPU idle pump wrapper.",
    "_SituationWaitUntilVoiceSnapshotIdle": "RT spin-wait until audio snapshot completes.",
    "_sit_miniaudio_capture_callback": "miniaudio RT capture callback ABI.",
    "_SituationMixToneToBuffer": "real-time tone mix path.",
    "_SituationMixLoadedVoicesFromSnapshot": "real-time voice mix path.",
    "_SituationPublishMasterBusLevels": "RT level metering side-channel.",
    "_SituationAsyncAudioWorker": "thread-pool job ABI; use SituationGetLastErrorCode after wait if handle invalid.",
    "_SituationCheckGLError": "debug GL error probe; logs via error channel (plan §3.3).",
    "_SituationVirtualBindlessInit": "in-memory virtual texture slot table reset only.",
    "_SituationVulkanFreeAsyncShaderLoad": "async teardown; poll path sets terminal error before free.",
    "_SitFreeShaderSlot": "idempotent slot release; invalid handles ignored.",
    "_SitFreeComputePipelineSlot": "idempotent slot release; invalid handles ignored.",
    "_SitFreeMeshSlot": "idempotent slot release; invalid handles ignored.",
    "_SitFreeBufferSlot": "idempotent slot release; invalid handles ignored.",
    "_SitFreeModelSlot": "idempotent slot release; invalid handles ignored.",
    "_sit_uniform_map_destroy": "uniform map teardown; idempotent free.",
    "_SituationCleanupQuadRenderer": "best-effort quad renderer teardown.",
    "_SituationCleanupDanglingResources": "best-effort shutdown leak sweep.",
    "_SituationCleanupOpenGL": "best-effort OpenGL teardown.",
    "_SituationCleanupVulkan": "best-effort Vulkan teardown.",
    "_SituationCleanupStagingBuffers": "best-effort staging buffer teardown.",
    "_SituationInitGraveyard": "graveyard slot init (no failure paths).",
    "_SituationCleanupGraveyard": "graveyard slot teardown.",
    "_SituationFlushGraveyard": "deferred destroy flush after fence wait.",
    "_SituationDeferDestroyBuffer": "enqueue buffer for deferred destroy.",
    "_SituationDeferDestroyImage": "enqueue image for deferred destroy.",
    "_SituationDeferDestroyDescriptorSet": "enqueue descriptor set for deferred destroy.",
    "_SituationDeferDestroyPipeline": "enqueue pipeline for deferred destroy.",
    "_SituationDeferDestroyFramebuffer": "enqueue framebuffer for deferred destroy.",
    "_SituationDeferDestroyRenderPass": "enqueue render pass for deferred destroy.",
    "_SituationVulkanDestroyImage": "immediate or deferred image destroy helper.",
    "_SituationVulkanDestroyBuffer": "immediate or deferred buffer destroy helper.",
    "_SituationVulkanDestroyScreenCopyResource": "screen copy resource teardown.",
    "_SituationVulkanDestroyScreenshotResources": "screenshot resource teardown.",
    "_SituationVulkanFreeBufferDescriptorSet": "descriptor set cache release.",
    "_SituationFreeSpirvBlob": "SPIR-V blob free helper.",
    "_SituationShaderIncluderRelease": "shaderc includer release callback.",
    "_SitGLDeferDestroyBuffer": "enqueue GL buffer for deferred destroy.",
    "_SitGLDeferDestroyTexture": "enqueue GL texture for deferred destroy.",
    "_SitGLDeferCleanMeshVAO": "enqueue mesh VAO cache clean.",
    "_SitGLFlushGraveyard": "GL deferred destroy flush after fence wait.",
    "_SituationGLFreeSpirvAsyncCopies": "async SPIR-V copy buffer free.",
    "_SitGLBackupState": "GL state backup for scoped restore.",
    "_SitGLRestoreState": "GL state restore after scoped backup.",
    "_SitGLInvalidateShadowState": "invalidate GL shadow state cache.",
    "_SitGLEnsureDefaultFramebufferOpaqueAlpha": "presentation alpha fixup (best-effort).",
    "_SituationGLRingWait": "GL ring fence wait (blocking sync point).",
    "_SituationMakeGLContextCurrentForHostThread": "GL context handoff to host thread.",
    "_SituationReleaseHostGLContextForRenderThread": "GL context handoff to render thread.",
    "_SituationBindGLProgramUniformBlocks": "GL UBO block bind best-effort.",
    "_SituationBindGLProgramStorageBlocks": "GL SSBO block bind best-effort.",
    "_SituationVulkanTransitionImageLayout": "record-only image layout barrier.",
    "_SituationVulkanCopyBufferToImage": "record-only buffer-to-image copy.",
    "_SituationVulkanGenerateMipmaps": "record-only mipmap generation.",
    "_SituationVulkanRecordScreenshotCopy": "record-only screenshot blit.",
    "_SituationVulkanResolveScreenshotAfterSubmit": "post-submit screenshot resolve hook.",
    "_SituationVulkanCopyMappedColorToRGBA": "CPU color format swizzle helper.",
    "_SituationVulkanQuerySwapchainSupport": "swapchain capability query (fills out struct).",
    "_SituationVulkanFreeSwapchainSupportDetails": "free swapchain query scratch.",
}

VOID_DECL = re.compile(r"^(\s*)static void (_\w+|sit_miniaudio_data_callback)\(")


def reason(name: str) -> str:
    if name in SPECIFIC:
        return SPECIFIC[name]
    if "Defer" in name or "FlushGraveyard" in name or "Graveyard" in name:
        return "deferred destroy / graveyard lifecycle."
    if name.startswith("_SitGL"):
        return "OpenGL internal helper (state, defer, or presentation)."
    if name.startswith("_SituationVulkan") and "Destroy" in name:
        return "Vulkan resource destroy helper."
    if "Cleanup" in name or "Free" in name or "Uninit" in name or "destroy" in name:
        return "idempotent teardown or free helper."
    if "Worker" in name or "Callback" in name:
        return "callback or worker ABI."
    return "intentional void internal helper (Bucket B)."


def tag_file(path: Path) -> int:
    if not path.exists():
        return 0
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    out: list[str] = []
    added = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        m = VOID_DECL.match(line)
        if m:
            prev = out[-1] if out else ""
            if "HARDENING: void by design" not in prev:
                name = m.group(2)
                indent = m.group(1)
                text = reason(name)
                out.append(f"{indent}/* HARDENING: void by design — {text} */\n")
                added += 1
        out.append(line)
        i += 1
    if added:
        path.write_text("".join(out), encoding="utf-8")
    return added


def main() -> None:
    files = [
        ROOT / "situation_impl_forward.h",
        ROOT / "situation_impl_renderer_fwd.h",
        ROOT / "situation_impl_decl.h",
        ROOT / "situation_impl_io.h",
        ROOT / "situation_impl_audio.h",
        ROOT / "situation_impl_threading.h",
    ]
    total = 0
    for f in files:
        n = tag_file(f)
        if n:
            print(f"{f.name}: +{n}")
            total += n
    print(f"total added: {total}")

    count = 0
    for f in ROOT.glob("situation_impl*.h"):
        count += f.read_text(encoding="utf-8").count("HARDENING: void by design")
    print(f"HARDENING void count in situation_impl*.h: {count}")


if __name__ == "__main__":
    main()
