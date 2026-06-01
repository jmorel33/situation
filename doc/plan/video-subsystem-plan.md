# Video Subsystem — Phased Implementation Plan

**Status:** proposed  
**Goal:** add bidirectional video I/O — **decode video → buffer** and **record buffer → stream/file** — as a modern Situation subsystem without bloating renderer or IO modules.  
**Constraint:** public API lives in `sit/situation_api.h` (single-line `SITAPI` + EOL `//` description); implementation lives in **`sit/situation_impl_video.h`** (new module).  
**Related:** `doc/plan/v2.5-api-expansion.md`, `sit/situation_impl_audio.h` (stream callback model), Phase 1–2 GPU readback APIs.

---

## How to use this file

1. Execute phases **in order** unless a maintainer explicitly reprioritizes.  
2. Check boxes as work lands; link PR/issue next to completed items when possible.  
3. Signatures below are **proposals** — names and parameter order may change after review.  
4. Tag shipping items **`[GL+VK]`** when both backends must behave the same; codec work is CPU-side and backend-agnostic unless sourcing pixels from GPU readback.  
5. When an item graduates to implementation, add entries to `doc/situation_api.md` and `doc/UPDATELOG.md`.

---

## Feature summary

| Direction | User story | Primary integration |
|-----------|------------|---------------------|
| **Read → buffer** | Open a video file or custom byte stream, decode frames, deliver pixels into caller-owned CPU or GPU buffers | Mirror `SituationLoadSoundFromStream`; output via `SituationImage`, raw pointer, or `SituationUpdateBuffer` |
| **Buffer → record/stream** | Accept frame data from CPU memory or GPU readback, encode, write to file or custom write callback | Mirror audio capture callbacks; source from `SituationReadTexture` / `SituationReadFramebuffer` / raw CPU buffer |

---

## Conventions (follow existing Situation patterns)

- Returns **`SituationError`** for create/update failures; `void` for release.  
- Handles are generational POD structs (`SituationVideo`, `SituationVideoRecorder`) like `SituationSound` / `SituationBuffer`.  
- Stream I/O uses read/write/seek callback typedefs parallel to audio (`SituationStreamReadCallback` family).  
- Heavy work (decode/encode/mux) runs off the main thread via **`situation_impl_threading.h`** IO pool or dedicated worker — never block `SituationUpdate` unless explicitly documented as blocking (test helpers only).  
- Frame timing uses **`SituationUpdateTimers`** / presentation timestamps (PTS) in nanoseconds or seconds — pick one unit in Phase 0 and stick to it.  
- Optional compile flag: **`SITUATION_ENABLE_VIDEO`** (default off until codec dependency is vendored) to keep lean builds working.

---

## Phase 0 — Scope lock and dependency decision

**Purpose:** lock architecture before any public signatures ship.

**Status:** ✅ **Decision locked** (2026-05-23) — see [Phase 0 decision record](#phase-0-decision-record) below.

- [ ] Confirm v2.x target version tag for video APIs (e.g. v2.6 or post–Phase 6 v2.5).  
- [x] Choose codec backend → **Option A: FFmpeg (LGPL, dynamic link)** — see decision record.  
- [x] Define supported **input containers/codecs** for v1 ship list — see decision record.  
- [x] Define supported **output containers/codecs** for v1 ship list — see decision record.  
- [x] Decide pixel format contract → **`RGBA8` / `SituationImage` (4 channels)** as canonical CPU interchange; YUV passthrough deferred.  
- [x] Decide audio tracks → **video-only v1**; demux audio stubbed/ignored; A/V mux deferred to Phase 5+.  
- [ ] Confirm error semantics for void release functions with invalid handles (match audio/image modules).  
- [ ] Add Phase 0 decision summary to this file and a stub **Section Q** pointer in `doc/plan/v2.5-api-expansion.md`.

**Exit criteria:** codec choice, compile flag, pixel format, and v1 codec/container list are written down; no signature churn expected after Phase 1.

---

### Phase 0 decision record

#### Why FFmpeg wins for MIT / GPL open-source C infrastructure

Situation is **MIT**. The goal is the best **modern, maintained, portable C stack** for demux → decode → color convert → encode → mux — not a home-grown codec pile or platform lock-in.

| Option | Verdict | Why |
|--------|---------|-----|
| **A. FFmpeg (LGPL, dynamic)** | ✅ **Chosen** | Single C ecosystem covering containers, codecs, timestamps, seeking, streaming I/O, and pixel conversion (`libavformat`, `libavcodec`, `libswscale`, `libavutil`). Matches how miniaudio covers audio — one serious backend, thin Situation wrapper. |
| B. Platform APIs (MF / AVFoundation) | ❌ Rejected | Not portable OSS infrastructure; three backends to maintain; breaks Linux/MSYS2-first workflow. |
| C. Raw `.y4m` / plumbing only | ⚠️ Dev-only | Useful for Phase 1–2 harness tests **without** FFmpeg present; not a v1 ship path. |
| D. Hand-rolled BSD stack (dav1d + libvpx + openh264 + custom demuxer) | ❌ Rejected | Maximally MIT-pure but **reimplements FFmpeg's job** across 4–6 libraries with no unified seek/mux/stream story; high glue cost for worse coverage. |

**License model (keeps Situation MIT):**

1. Build FFmpeg **without** `--enable-gpl` and **without** `--enable-nonfree` ([FFmpeg legal checklist](https://www.ffmpeg.org/legal.html)).  
2. Link **dynamically** to FFmpeg DLLs (`.dll` on Windows, `.so` on Linux) — same pattern as `situation_opengl.dll` + optional deps. Situation source stays MIT; LGPL obligations apply to the **bundled FFmpeg binaries**, not to Situation itself.  
3. Ship `ext/ffmpeg/` with **headers + prebuilt LGPL DLLs** (or documented MSYS2 build script); `SITUATION_ENABLE_VIDEO` gates compile-time inclusion.  
4. **Never call GPL codecs** in the default build (notably **libx264**, **libx265**, **libxvid**).  
5. Include LGPL attribution in docs / about string: *"This software uses libraries from the FFmpeg project under the LGPLv2.1."*

**Optional future tier (not v1):** `SITUATION_VIDEO_GPL` build profile that links a GPL-enabled FFmpeg + x264 for users who **want** GPL and accept copyleft on their **application**. Situation core remains MIT; this is an opt-in distribution variant.

#### FFmpeg source provenance

- **Origin:** latest **GitHub** tree (`FFmpeg/FFmpeg` master), not a pinned release tarball.  
- **Local path:** `ext/ffmpeg/` (sanitized 2026-05-23; ~4,991 source files remain).  
- **Version pinning:** for reproducible Situation builds, `build_ffmpeg.bat` should record the git commit or tag used (e.g. `n7.1` / `N-120xxx-gxxxxxx`) once `configure` has run. Without a `.git` directory, `ffbuild/version.sh` falls back to `unknown` — acceptable for dev, but tag a release before shipping.  
- **Note:** root `RELEASE` was removed during sanitization; version is recovered at build time via `configure` / generated `libavutil/ffversion.h`.


#### Build artifact model

Situation has **three dependency tiers** today:

| Tier | Examples | How it lands in the DLL |
|------|----------|-------------------------|
| **Header-only** | stb, miniaudio, glad | `#include` inside `situation_dll.c` → one giant `situation_dll_*.o` |
| **Small `.c` → `.o`** | `tinycthread.c`, `vma_wrapper.cpp` | `gcc -c` → `build/dll/*.o` → linked at end |
| **Pre-built static `.a`** | `libglfw3.a`, `libshaderc_combined.a` | CMake/build in `ext/<lib>/build/` first; `-L` + `-l` in `build_situation.bat` |

**FFmpeg is tier 3 (like GLFW), not tier 1 (unlike miniaudio).** You cannot drop headers into `situation_impl_deps.h` and `#include` it — FFmpeg is hundreds of `.c` files that must be **compiled into static libraries** (or shared DLLs) in a separate build step **before** `build_situation.bat` links Situation.

**Source:** latest GitHub master in `ext/ffmpeg/` (present; sanitized).

**Recommended layout after first build (Phase 0b — ext/ only, Situation untouched):**

```
ext/ffmpeg/                          ← latest GitHub master (present, sanitized)
  configure / Makefile / libav*/       ← upstream FFmpeg source
  build/                             ← out-of-tree build dir (created by build_ffmpeg.bat)
    lib/libavcodec.a
    lib/libavformat.a
    lib/libswscale.a
    lib/libavutil.a
    include/libavcodec/...           ← installed public headers
```

**Situation integration (Phase 1+, non-disruptive):**

```
sit/situation_impl_video.h   ← ONLY new Situation impl module for video
sit/situation_api.h          ← single-line SITAPI prototypes + EOL descriptions
```

- Video is a **detachable** subsystem: `#include` in `situation_impl.h` only when `SITUATION_ENABLE_VIDEO` is defined.  
- **No** changes to `build_situation.bat` until a later explicit wiring phase. Default Situation builds stay identical.  
- FFmpeg `#include`s live **inside** `situation_impl_video.h` (guarded by `#ifdef SITUATION_ENABLE_VIDEO`), not in a separate glue layer.  
- No FFmpeg types in `situation_api.h` — public API uses Situation handles only.

#### Phase 0b — FFmpeg fetch & build pipeline (ext/ only — does NOT touch Situation)

**Scope:** build FFmpeg static libs under `ext/ffmpeg/build/`. Situation DLL, `build_situation.bat`, and impl headers are **unchanged** in this phase.

- [x] **Fetch source** into `ext/ffmpeg/` — **done** (latest GitHub master; sanitized).  
- [x] Add **`build_ffmpeg.bat`** + **`build_ffmpeg.sh`** (MSYS2 MinGW64, minimal LGPL configure).  
- [x] **Verify** `make install` produces static `.a` files in `ext/ffmpeg/build/lib/` — done 2026-05-24 (`libavcodec.a`, `libavformat.a`, `libswscale.a`, `libavutil.a`).  
- [ ] Document one-time build in `doc/COMPILATION_GUIDE.md` (FFmpeg only — no Situation link step).  
- [ ] LGPL compliance note in docs (source offer + build instructions for bundled FFmpeg).

**Explicitly NOT Phase 0b:** `situation_impl_video.h`, `build_situation.bat` changes, new glue `.c` files, or FFmpeg linked into `situation_*.dll`. Those belong to **Phase 1+**.

**Alternative (LGPL-easier deploy, later):** shared FFmpeg DLLs shipped beside Situation — decide when wiring Phase 1, not now.

#### Stripping the FFmpeg source tree (~10K files / ~85 MB)

Your tree matches a standard FFmpeg checkout. **File count is high; disk size is modest (~85 MB).** Most bulk is source you never compile if `configure` is tuned — not runtime weight.

| Directory | Files | ~MB | Safe to delete? |
|-----------|------:|----:|-----------------|
| `tests/` | 5059 | 9.9 | ✅ Yes (with stub `tests/Makefile` — see below) |
| `libavcodec/` | 2697 | 43.2 | ❌ **Never** prune by hand — use `configure` |
| `libavfilter/` | 813 | 10.5 | ❌ Keep folder — `configure` reads it even when disabled |
| `libavformat/` | 715 | 8.8 | ❌ Required |
| `libavutil/` | 444 | 3.9 | ❌ Required |
| `libswscale/` | 150 | 2.8 | ❌ Required |
| `doc/` | 114 | 2.8 | ✅ Yes (with stub `doc/Makefile`) |
| `libavdevice/` | 77 | 0.7 | ❌ Keep folder — disable via configure only |
| `tools/` | 58 | 0.4 | ⚠️ Optional — stub `tools/Makefile` if cleared |
| `fftools/` | 53 | 1.3 | ⚠️ Optional — stub `fftools/Makefile` if cleared |
| `libswresample/` | 46 | 0.3 | ❌ Keep folder — disable via configure for video-only v1 |
| `.forgejo/` | 9 | — | ✅ Delete entirely (hosting metadata) |

**Tier 1 — delete now, zero build risk**

- [x] `ext/ffmpeg/.forgejo/` (entire folder)
- [x] Root meta only (optional): `CONTRIBUTING.md`, `INSTALL.md`, `MAINTAINERS`, `FUNDING.json`, `Changelog`, `RELEASE`, `.mailmap`
- [x] **Keep:** `configure`, `Makefile`, `ffbuild/`, `compat/`, all `libav*/`, `COPYING.*`, `LICENSE.md`

**Tier 2 — ~5,200 fewer files (recommended if count bothers you)** — **done 2026-05-23** (10,291 → 4,991 files; ~85 MB → ~71 MB)

Top-level `Makefile` always `include`s `tests/Makefile`, `doc/Makefile`, `tools/Makefile`, `fftools/Makefile`. Deleting those folders **without stubs breaks `make`**. Procedure:

- [x] Stubbed and cleared `tests/` (5,059 → 1 file)
- [x] Stubbed and cleared `doc/` + `doc/examples/` (114 → 2 files)
- [x] Stubbed and cleared `fftools/` (53 → 1 file)
- [x] Stubbed and cleared `tools/` (58 → 1 file)
- [x] Cleared `presets/` (5 → 0 files)

**Tier 3 — do NOT do this**

- ❌ Deleting individual codec `.c` files inside `libavcodec/` (breaks `configure` component scanning; use flags instead)
- ❌ Deleting whole `libavfilter/` / `libavdevice/` / `libswresample/` folders (`configure` still parses their `all*.c` lists)
- ❌ Deleting `ffbuild/` or `compat/`

**Shrink the build output (not file count) — minimal Situation configure**

Use `--disable-everything` then enable only what v1 needs. Example baseline:

```sh
./configure \
  --prefix="$PWD/build" \
  --enable-static --disable-shared \
  --disable-programs --disable-doc --disable-debug \
  --disable-network --disable-hwaccels \
  --disable-avdevice --disable-avfilter --disable-swresample \
  --disable-gpl --disable-nonfree \
  --enable-decoder=h264 --enable-decoder=hevc \
  --enable-decoder=vp8 --enable-decoder=vp9 --enable-decoder=av1 \
  --enable-decoder=mpeg4 \
  --enable-demuxer=mov --enable-demuxer=matroska --enable-demuxer=avi \
  --enable-muxer=mp4 --enable-muxer=matroska \
  --enable-protocol=file \
  --arch=x86_64 --target-os=mingw32
```

Add encoders (OpenH264, libvpx) once external libs are available. This compiles a **small fraction** of the 2,697 `libavcodec` files while leaving sources on disk.

**Long-term:** consider **not** committing `ext/ffmpeg/` to the project at all — only a `build_ffmpeg.bat` that downloads the release tarball into `ext/ffmpeg/` locally (same pattern as “build GLFW first”).

#### Integration layout (Phase 1 — runtime code)

```
sit/situation_impl_video.h   ← sole impl module; FFmpeg includes guarded inside
sit/situation_api.h          ← public single-line SITAPI + EOL descriptions
```

Included from `situation_impl.h` only when `SITUATION_ENABLE_VIDEO` is defined. With the flag off, zero video code is compiled and the library behaves exactly as today.

#### v1 container / codec matrix (LGPL-safe)

**Read (decode → buffer):**

| Container | Video codecs | Notes |
|-----------|--------------|-------|
| MP4 / MOV | H.264, HEVC*, MPEG-4 | *HEVC decode availability depends on FFmpeg build; document if omitted |
| WebM / MKV | VP8, VP9, AV1 | AV1 via FFmpeg's native/dav1d path in LGPL build |
| Raw `.y4m` | Uncompressed YUV | Harness / plumbing only |

**Write (buffer → record/stream):**

| Container | Video codecs | Encoder backend |
|-----------|--------------|-----------------|
| MP4 | H.264 | **OpenH264** (BSD) via FFmpeg, or FFmpeg native MPEG-4 as fallback |
| WebM | VP9 | **libvpx** (BSD) via FFmpeg |
| MKV | VP9, AV1 | libvpx / libaom (both BSD) via FFmpeg |

**Explicitly excluded from default build:** x264, x265 (GPL), FAAC/FDK-AAC (nonfree), any `--enable-nonfree` encoder.

#### Pixel format & timing contracts

- **CPU interchange:** `RGBA8` (`SituationVideoPixelFormat` = `SITUATION_VIDEO_PIXEL_RGBA8`), compatible with `SituationImage` 4-channel path.  
- **Internal decode:** FFmpeg native YUV → `swscale` → RGBA8 before handing to caller (YUV passthrough API deferred).  
- **PTS unit:** `double` seconds (`pts_sec` in `SituationVideoFrameDesc`); monotonic per stream.  
- **Audio:** demuxer may probe audio streams; v1 APIs ignore them. Phase 5+ may add PCM side channel.

#### Rejected options (detail)

- **Platform APIs:** violates cross-platform mandate; Windows-only H.264 encode already available as OpenH264 inside FFmpeg when needed.  
- **Vulkan Video (`ext/vulkan/vk_video/`):** headers exist for GL/VK backend parity elsewhere, but hardware video encode/decode is backend-specific, driver-gated, and does not replace file/container I/O — out of scope for v1.  
- **GStreamer:** LGPL and heavier; pipeline model fights Situation's thin C API style.

#### Phase 0 remaining open items

- [ ] Target version tag (v2.6 proposed).  
- [ ] Void-release error semantics (match audio/image).  
- [ ] MSYS2 FFmpeg build script + LGPL compliance checklist in `doc/COMPILATION_GUIDE.md`.  
- [ ] Stub **Section Q** in `doc/plan/v2.5-api-expansion.md`.

---

## Phase 1 — Module skeleton and public types

**Purpose:** land the subsystem shell with zero codec dependency (or stub backend) so wiring and tests compile.

### 1.1 New implementation module

- [ ] Create `sit/situation_impl_video.h` with standard header block (subsystem name, bullet list, "do not include directly").  
- [ ] Add `#include "situation_impl_video.h"` to `sit/situation_impl.h` after `situation_impl_image.h`, before renderer (video is CPU-first; renderer only needed for GPU upload helpers in Phase 3).  
- [ ] Add internal structs to `sit/situation_impl_decl.h` (or `sit/situation_impl_forward.h` prototypes only):  
  - [ ] `_SituationVideoDecoder` — file/stream state, frame index, dimensions, duration.  
  - [ ] `_SituationVideoRecorder` — encoder context, output sink, frame queue, FPS.  
- [ ] Add generational handle slots / ID tables following `SituationSound` pattern.  
- [ ] Stub all `SITAPI` bodies with `SITUATION_ERROR_NOT_IMPLEMENTED` or no-op when `SITUATION_ENABLE_VIDEO` is undefined.

### 1.2 Public types in `situation_api.h`

Add a **`// Video Module`** section (after Image or after Audio — pick one and stay consistent).

```c
// ── Video handles and enums ──────────────────────────────────────────────────
typedef struct SituationVideo { uint32_t id; uint32_t generation; } SituationVideo;
typedef struct SituationVideoRecorder { uint32_t id; uint32_t generation; } SituationVideoRecorder;

typedef enum SituationVideoPixelFormat {
    SITUATION_VIDEO_PIXEL_RGBA8 = 0,
    SITUATION_VIDEO_PIXEL_BGRA8,
    SITUATION_VIDEO_PIXEL_RGB8,
} SituationVideoPixelFormat;

typedef enum SituationVideoLoadMode {
    SITUATION_VIDEO_LOAD_FULL = 0,   // Decode all frames into RAM (short clips only)
    SITUATION_VIDEO_LOAD_STREAM,     // Decode on demand, one frame at a time
} SituationVideoLoadMode;

typedef struct SituationVideoInfo {
    int      width;
    int      height;
    double   duration_sec;
    double   frame_rate;
    int      frame_count;              // -1 if unknown (streaming)
    SituationVideoPixelFormat pixel_format;
} SituationVideoInfo;

typedef struct SituationVideoFrameDesc {
    void*    data;                     // Caller-provided or library-allocated pixel buffer
    size_t   data_size;
    int      width;
    int      height;
    int      stride_bytes;
    SituationVideoPixelFormat pixel_format;
    double   pts_sec;                  // Presentation timestamp
    uint64_t frame_index;
} SituationVideoFrameDesc;

typedef struct SituationVideoRecorderDesc {
    int      width;
    int      height;
    double   frame_rate;
    const char* output_path;           // NULL if using stream write callback only
    SituationVideoPixelFormat input_pixel_format;
} SituationVideoRecorderDesc;
```

### 1.3 Stream callback typedefs (parallel to audio)

```c
typedef size_t (*SituationVideoStreamReadCallback)(void* user_data, void* buffer_out, size_t bytes_to_read); // Return bytes read; 0 = EOF.
typedef bool   (*SituationVideoStreamSeekCallback)(void* user_data, int64_t offset, int origin);            // Return true on success.
typedef size_t (*SituationVideoStreamWriteCallback)(void* user_data, const void* data, size_t bytes_to_write); // Return bytes written.
```

### 1.4 Phase 1 API stubs (signatures only — implement in later phases)

```c
SITAPI bool SituationIsVideoEnabled(void);                                                                                // Returns whether SITUATION_ENABLE_VIDEO was compiled in.
SITAPI SituationError SituationGetVideoInfo(SituationVideo video, SituationVideoInfo* out_info);                          // Query width, height, duration, and frame rate.
SITAPI bool SituationIsVideoValid(SituationVideo video);                                                                  // Check whether a video handle is live.
SITAPI void SituationCloseVideo(SituationVideo* video);                                                                   // Close decoder and free resources.
SITAPI bool SituationIsVideoRecorderValid(SituationVideoRecorder recorder);                                               // Check whether a recorder handle is live.
SITAPI void SituationDestroyVideoRecorder(SituationVideoRecorder* recorder);                                              // Finalize encode, flush muxer, and free resources.
```

- [ ] All Phase 1 signatures added to `situation_api.h` with EOL descriptions.  
- [ ] Harness compiles with video disabled (default) and with video enabled (stub).

**Exit criteria:** new module wired; handles and enums public; stub implementations link; no codec required yet.

---

## Phase 2 — Video read path (file/stream → buffer)

**Purpose:** decode video and deliver frames into caller buffers — the **read → buffer** capability.

### 2.1 Open / load APIs

```c
SITAPI SituationError SituationOpenVideoFromFile(const char* file_path, SituationVideoLoadMode mode, SituationVideo* out_video); // Open a video file for sequential frame decoding.
SITAPI SituationError SituationOpenVideoFromStream(SituationVideoStreamReadCallback on_read, SituationVideoStreamSeekCallback on_seek, void* user_data, SituationVideoLoadMode mode, SituationVideo* out_video); // Open a video from a custom byte stream.
SITAPI SituationError SituationOpenVideoFromMemory(const void* data, size_t data_size, SituationVideoLoadMode mode, SituationVideo* out_video); // Open a video from a memory buffer (e.g. SituationLoadFileData result).
```

- [ ] Implement file open via IO helpers (`situation_impl_io.h`) — async path optional in Phase 5.  
- [ ] Implement stream open with read/seek callbacks (same ergonomics as `SituationLoadSoundFromStream`).  
- [ ] Implement memory open for embedded / network-downloaded blobs.  
- [ ] Populate `SituationGetVideoInfo` after demux probe.

### 2.2 Frame decode → CPU buffer

```c
SITAPI SituationError SituationVideoDecodeNextFrame(SituationVideo video, SituationVideoFrameDesc* inout_frame);            // Decode the next frame into caller-provided data buffer; advances read cursor.
SITAPI SituationError SituationVideoDecodeFrameAt(SituationVideo video, uint64_t frame_index, SituationVideoFrameDesc* inout_frame); // Decode a specific frame by index (FULL mode or seekable streams).
SITAPI SituationError SituationVideoDecodeNextFrameAlloc(SituationVideo video, SituationVideoFrameDesc* out_frame);       // Decode next frame into library-allocated buffer; caller frees with SituationVideoFreeFrame.
SITAPI void SituationVideoFreeFrame(SituationVideoFrameDesc* frame);                                                      // Free pixel data allocated by SituationVideoDecodeNextFrameAlloc.
SITAPI SituationError SituationVideoSeek(SituationVideo video, double time_sec);                                          // Seek to timestamp for streaming playback.
SITAPI SituationError SituationVideoReset(SituationVideo video);                                                          // Rewind to first frame.
```

- [ ] `SituationVideoDecodeNextFrame` validates `inout_frame.data` / `data_size` against `width * height * channels`.  
- [ ] Convert decoded native pixel format → `SituationVideoPixelFormat` (swscale or equivalent).  
- [ ] Set `pts_sec` and `frame_index` on every decoded frame.  
- [ ] `SituationVideoDecodeNextFrameAlloc` uses `SIT_MALLOC`; document ownership.  
- [ ] `STREAM` mode: do not decode entire file at open; `FULL` mode: optional eager decode cache for short clips.

### 2.3 Convenience: decode → `SituationImage`

```c
SITAPI SituationError SituationVideoDecodeNextImage(SituationVideo video, SituationImage* out_image);                    // Decode next frame directly into a SituationImage (RGBA8).
SITAPI SituationError SituationVideoDecodeImageAt(SituationVideo video, uint64_t frame_index, SituationImage* out_image); // Decode specific frame into SituationImage.
```

- [ ] Reuse `SituationCreateImage` + fill `data` — no duplicate pixel container type.  
- [ ] Caller calls `SituationUnloadImage` when done.

### 2.4 Tests (Phase 2)

- [ ] Add `tests/harness/test_video_decode.c` with a tiny bundled test clip (or generate `.y4m` at build time).  
- [ ] Test: open file, decode N frames, verify dimensions and monotonic PTS.  
- [ ] Test: stream callback open with memory-backed context.  
- [ ] Test: seek + decode frame at index.  
- [ ] Test: error paths — truncated file, wrong buffer size, invalid handle.

**Exit criteria:** caller can open a video and pull frames into raw buffers or `SituationImage`; streaming mode does not load whole file into RAM.

---

## Phase 3 — Video read path GPU upload (buffer → texture)

**Purpose:** after CPU decode, optionally push frames into **GPU buffers** without an extra copy through user code.

```c
SITAPI SituationError SituationVideoDecodeNextFrameToBuffer(SituationVideo video, SituationBuffer dst_buffer, size_t dst_offset); // Decode next frame and upload to GPU buffer (SSBO/storage).
SITAPI SituationError SituationVideoDecodeNextFrameToTexture(SituationVideo video, SituationTexture dst_texture);         // Decode next frame and upload to existing texture (overwrite level 0).
```

- [ ] Document: `dst_buffer` must be sized ≥ `width * height * 4`; usage flags include transfer dst.  
- [ ] Upload via existing `SituationUpdateBuffer` (GL) / staging path (VK) — no new backend code unless profiling shows need.  
- [ ] Texture path: require `SituationGetTextureInfo` dimensions match video frame size.  
- [ ] Optional helper: decode → texture create in one call:

```c
SITAPI SituationError SituationVideoDecodeNextFrameToNewTexture(SituationVideo video, bool generate_mipmaps, SituationTexture* out_texture); // Decode next frame and create a new GPU texture.
```

- [ ] Regression test: decode 1 frame → buffer → readback compare (`SituationReadBuffer` / Phase 1 path).

**Exit criteria:** decoded frames can land directly in GPU memory for shader/compute consumption.

---

## Phase 4 — Video record path (buffer → stream/file)

**Purpose:** accept frames from CPU or GPU readback and **encode + mux** to file or custom write stream — the **record → stream save** capability.

### 4.1 Recorder lifecycle

```c
SITAPI SituationError SituationCreateVideoRecorder(const SituationVideoRecorderDesc* desc, SituationVideoRecorder* out_recorder); // Create a recorder targeting a file path.
SITAPI SituationError SituationCreateVideoRecorderToStream(SituationVideoStreamWriteCallback on_write, void* user_data, const SituationVideoRecorderDesc* desc, SituationVideoRecorder* out_recorder); // Create a recorder that writes encoded output via callback.
SITAPI SituationError SituationVideoRecorderBegin(SituationVideoRecorder recorder);                                       // Start encoder and muxer (headers written).
SITAPI SituationError SituationVideoRecorderEnd(SituationVideoRecorder recorder);                                         // Flush encoder and finalize container.
```

- [ ] `SituationDestroyVideoRecorder` calls `End` if still active.  
- [ ] Stream callback receives muxed bytes (MP4/WebM) — document chunk boundaries may split on any byte boundary.

### 4.2 Push frames from CPU buffer

```c
SITAPI SituationError SituationVideoRecorderPushFrame(SituationVideoRecorder recorder, const SituationVideoFrameDesc* frame); // Encode one frame from CPU pixel buffer.
SITAPI SituationError SituationVideoRecorderPushImage(SituationVideoRecorder recorder, SituationImage image, double pts_sec); // Encode one frame from SituationImage.
```

- [ ] Validate width/height match `SituationVideoRecorderDesc`.  
- [ ] Auto-assign PTS from frame index / FPS if `pts_sec < 0` (define sentinel convention in Phase 0).  
- [ ] Internal frame queue if encode thread lags — bounded queue with backpressure (`SITUATION_ERROR_WOULD_BLOCK` or drop policy documented).

### 4.3 Push frames from GPU readback

```c
SITAPI SituationError SituationVideoRecorderPushFrameFromReadback(SituationVideoRecorder recorder, SituationBuffer readback_buf, size_t size, double pts_sec); // Encode frame from a mapped readback buffer (previous frame copy).
```

- [ ] Document pairing with `SituationCreateReadbackBuffer` + `SituationCmdCopyBuffer` + `SituationReadBuffer` from v2.5 Phase 1.  
- [ ] Example flow: render → copy framebuffer to readback → next frame read → push to recorder.

### 4.4 Capture helper (optional sugar)

```c
SITAPI SituationError SituationVideoRecorderCaptureFramebuffer(SituationVideoRecorder recorder, int x, int y, int width, int height, double pts_sec); // Read back screen region and encode one frame.
```

- [ ] Implement via `SituationReadFramebuffer` internally — convenience only.  
- [ ] Tag `[slow path]` in docs; not for production high-FPS capture without async queue.

### 4.5 Tests (Phase 4)

- [ ] Record 30 frames from synthetic color bars → file → re-open with Phase 2 decode → compare checksum.  
- [ ] Record to stream callback → accumulate → compare to file-based record.  
- [ ] Test finalize without frames (error) and double-`End` (no-op or error — match Phase 0 decision).

**Exit criteria:** caller can push CPU or readback frames and produce a playable file or byte stream.

---

## Phase 5 — Async I/O, threading, and performance

**Purpose:** keep decode/encode off the main thread; align with existing Situation async patterns.

```c
SITAPI SituationError SituationOpenVideoFromFileAsync(const char* file_path, SituationVideoLoadMode mode, SituationVideo* out_video); // Open video on IO thread; poll SituationIsVideoValid or use completion callback.
SITAPI SituationError SituationVideoDecodeNextFrameAsync(SituationVideo video, SituationVideoFrameDesc* inout_frame);     // Queue decode job; result ready next frame or via callback.
SITAPI SituationError SituationVideoRecorderPushFrameAsync(SituationVideoRecorder recorder, const SituationVideoFrameDesc* frame); // Non-blocking enqueue of frame for encoder thread.
```

- [ ] Reuse `situation_impl_threading.h` job pool and/or IO thread (mirror `SituationLoadSoundFromFileAsync`).  
- [ ] Define max in-flight frames (recorder queue depth, decoder prefetch).  
- [ ] Add optional completion callbacks if polling is insufficient:

```c
typedef void (*SituationVideoFrameReadyCallback)(SituationVideo video, const SituationVideoFrameDesc* frame, SituationError result, void* user_data); // Called when async decode completes.
typedef void (*SituationVideoWriteReadyCallback)(SituationVideoRecorder recorder, size_t bytes_written, SituationError result, void* user_data); // Called when async encode chunk is muxed.
```

- [ ] Profile 1080p30 record + decode on mid-tier hardware; document expected thread counts.  
- [ ] Ensure real-time safety: no `SIT_MALLOC` on audio thread; video callbacks run on worker threads only.

**Exit criteria:** main loop stays responsive during 1080p decode/record on reference hardware.

---

## Phase 6 — Examples, docs, and v2.5 catalog integration

**Purpose:** make the subsystem discoverable and shippable.

- [ ] Add **`examples/video_playback/`** — open file, decode to texture, draw full-screen quad.  
- [ ] Add **`examples/video_record/`** — virtual display or offscreen render → readback → MP4 file.  
- [ ] Add **`examples/video_stream_io/`** — custom read/write callbacks (memory or network stub).  
- [ ] Document full API in `doc/situation_api.md` under **Video Module**.  
- [ ] Add **Section Q — Video** to `doc/plan/v2.5-api-expansion.md` with link to this plan.  
- [ ] Update `doc/situation_sdk.md` forward-look blurb (Section 8.0 tease) with real section link.  
- [ ] Update `doc/SITUATION_QUICK_REFERENCE.md` with video one-liners.  
- [ ] Add `SITUATION_ENABLE_VIDEO` to `doc/COMPILATION_GUIDE.md` and `build_situation.bat`.  
- [ ] Add UPDATELOG entry when Phase 2 or Phase 4 first ships to users.

**Exit criteria:** two examples run on Windows; docs and compile guide updated; plan checkboxes through Phase 4 minimum are checked.

---

## File touch list (by phase)

| Phase | Files |
|-------|-------|
| 0 | `doc/plan/video-subsystem-plan.md`, `doc/plan/v2.5-api-expansion.md`, build docs |
| 1 | `sit/situation_api.h`, `sit/situation_impl_video.h`, `sit/situation_impl.h`, `sit/situation_impl_decl.h`, `sit/situation_impl_forward.h` |
| 2–4 | `sit/situation_impl_video.h` (bulk), `tests/harness/test_video_*.c` |
| 3 | `sit/situation_impl_renderer.h` (only if upload helpers need internal access) |
| 5 | `sit/situation_impl_threading.h`, `sit/situation_impl_io.h` |
| 6 | `examples/`, `doc/*`, `build_situation.bat` |

---

## Proposed full API checklist (single-line signature target)

Use this as the master checklist when adding to `situation_api.h`. Every line must follow:

`SITAPI <ret> <Name>(<params>); // <short description>`

### Decoder / reader

- [ ] `SituationIsVideoEnabled`
- [ ] `SituationOpenVideoFromFile`
- [ ] `SituationOpenVideoFromStream`
- [ ] `SituationOpenVideoFromMemory`
- [ ] `SituationOpenVideoFromFileAsync`
- [ ] `SituationGetVideoInfo`
- [ ] `SituationIsVideoValid`
- [ ] `SituationCloseVideo`
- [ ] `SituationVideoSeek`
- [ ] `SituationVideoReset`
- [ ] `SituationVideoDecodeNextFrame`
- [ ] `SituationVideoDecodeFrameAt`
- [ ] `SituationVideoDecodeNextFrameAlloc`
- [ ] `SituationVideoFreeFrame`
- [ ] `SituationVideoDecodeNextFrameAsync`
- [ ] `SituationVideoDecodeNextImage`
- [ ] `SituationVideoDecodeImageAt`
- [ ] `SituationVideoDecodeNextFrameToBuffer`
- [ ] `SituationVideoDecodeNextFrameToTexture`
- [ ] `SituationVideoDecodeNextFrameToNewTexture`

### Recorder / writer

- [ ] `SituationCreateVideoRecorder`
- [ ] `SituationCreateVideoRecorderToStream`
- [ ] `SituationVideoRecorderBegin`
- [ ] `SituationVideoRecorderEnd`
- [ ] `SituationIsVideoRecorderValid`
- [ ] `SituationDestroyVideoRecorder`
- [ ] `SituationVideoRecorderPushFrame`
- [ ] `SituationVideoRecorderPushImage`
- [ ] `SituationVideoRecorderPushFrameFromReadback`
- [ ] `SituationVideoRecorderPushFrameAsync`
- [ ] `SituationVideoRecorderCaptureFramebuffer`

---

## Risk register

| Risk | Mitigation |
|------|------------|
| FFmpeg binary size / licensing | Optional `SITUATION_ENABLE_VIDEO`; document LGPL dynamic link requirements |
| Main-thread stalls on 4K encode | Phase 5 async queue mandatory before claiming "production record" |
| GPU readback bandwidth | Document `SituationVideoRecorderCaptureFramebuffer` as debug/slow path; prefer explicit readback buffer ring |
| Signature churn | Phase 0 lock + stub Phase 1 compile before codec integration |
| A/V sync | Defer audio mux to Phase 5+; document video-only v1 |

---

## Suggested implementation order (sprint view)

1. **Sprint A:** Phase 0 decisions + Phase 1 skeleton (compile stubs).  
2. **Sprint B:** Phase 2 CPU decode path + tests.  
3. **Sprint C:** Phase 4 CPU record path + round-trip test (record → decode).  
4. **Sprint D:** Phase 3 GPU upload + Phase 4 readback push.  
5. **Sprint E:** Phase 5 async + Phase 6 examples/docs.

---

*Last updated: 2026-05-23*
