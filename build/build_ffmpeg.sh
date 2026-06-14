#!/usr/bin/env bash
# build_ffmpeg.sh — Situation minimal LGPL FFmpeg build (MSYS2 MinGW64)
# Called by build_ffmpeg.bat from the project root, or directly from build/.

set -euo pipefail

# Resolve project root (one level up from build/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC="${ROOT}/ext/ffmpeg"
BUILD="${SRC}/build"
JOBS="${FFMPEG_JOBS:-$(nproc 2>/dev/null || echo 4)}"

# Ensure MSYS2 MinGW tools are visible when invoked from PowerShell/batch.
export PATH="/mingw64/bin:/usr/bin:${PATH}"

if [[ ! -f "${SRC}/configure" ]]; then
    echo "[ERROR] FFmpeg source not found at ${SRC}/configure"
    exit 1
fi

mkdir -p "${BUILD}"
cd "${BUILD}"

ASM_FLAGS=""
if ! command -v nasm >/dev/null 2>&1; then
    echo "[WARN] nasm not found — building with --disable-x86asm (slower, still correct)."
    ASM_FLAGS="--disable-x86asm"
fi

# MSYS2 path for --prefix (must be absolute, no spaces)
PREFIX="${BUILD}"

echo "========================================"
echo " Situation FFmpeg Build"
echo " Source:  ${SRC}"
echo " Prefix:  ${PREFIX}"
echo " Jobs:    ${JOBS}"
echo "========================================"

../configure \
    --prefix="${PREFIX}" \
    --enable-static \
    --disable-shared \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --disable-network \
    --disable-hwaccels \
    --disable-avdevice \
    --disable-avfilter \
    --disable-swresample \
    --disable-gpl \
    --disable-nonfree \
    --disable-everything \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swscale \
    --enable-decoder=h264 \
    --enable-decoder=hevc \
    --enable-decoder=vp8 \
    --enable-decoder=vp9 \
    --enable-decoder=av1 \
    --enable-decoder=mpeg4 \
    --enable-decoder=mjpeg \
    --enable-demuxer=mov \
    --enable-demuxer=matroska \
    --enable-demuxer=avi \
    --enable-muxer=mp4 \
    --enable-muxer=matroska \
    --enable-protocol=file \
    --arch=x86_64 \
    --target-os=mingw32 \
    --pkg-config=pkg-config \
    ${ASM_FLAGS}

echo "[BUILD] make -j${JOBS} ..."
make -j"${JOBS}"

echo "[INSTALL] make install ..."
make install

for lib in libavcodec.a libavformat.a libswscale.a libavutil.a; do
    if [[ ! -f "${PREFIX}/lib/${lib}" ]]; then
        echo "[ERROR] Expected archive missing: ${PREFIX}/lib/${lib}"
        exit 1
    fi
done

echo ""
echo "[SUCCESS] FFmpeg libraries installed to:"
echo "  ${PREFIX}/lib/libavcodec.a"
echo "  ${PREFIX}/lib/libavformat.a"
echo "  ${PREFIX}/lib/libswscale.a"
echo "  ${PREFIX}/lib/libavutil.a"
echo "  headers: ${PREFIX}/include/"
