#!/bin/sh
#
# Extracts the headers of the 3DS SDK from the build container, so that the
# editor can resolve <3ds.h>, <citro2d.h> and <tremor/ivorbisfile.h> without a
# local devkitPro installation.
#
# The compiler itself is not copied: it is built for the container, and the
# editor only needs the headers. The script is safe to run more than once.

set -e

IMAGE="${DKP_IMAGE:-devkitpro/devkitarm:latest}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/.devkitpro"

if ! command -v docker > /dev/null 2>&1; then
    echo "docker is needed to extract the headers" >&2
    exit 1
fi

if [ -d "$DEST/libctru/include" ]; then
    echo "$DEST is already populated"
else
    mkdir -p "$DEST"

    # newlib, libstdc++ and the GCC headers, plus libctru and the portlibs,
    # which is where vorbisidec lives. The tar keeps the ownership of the host
    # user, so the extracted tree can be removed without root.
    echo "Extracting the headers with $IMAGE"

    docker run --rm "$IMAGE" sh -c 'cd /opt/devkitpro && tar -cf - \
        devkitARM/arm-none-eabi/include \
        devkitARM/lib/gcc/arm-none-eabi/*/include \
        libctru/include \
        portlibs/3ds/include' | tar -C "$DEST" -xf -
fi

# The version of the toolchain is part of the paths of the C++ and GCC
# headers. A "current" link keeps the editor configuration stable when the
# image is updated.
CXX_DIR="$(find "$DEST/devkitARM/arm-none-eabi/include/c++" -mindepth 1 -maxdepth 1 -type d | head -1)"
GCC_DIR="$(find "$DEST/devkitARM/lib/gcc/arm-none-eabi" -mindepth 1 -maxdepth 1 -type d | head -1)"

ln -sfn "$(basename "$CXX_DIR")" "$DEST/devkitARM/arm-none-eabi/include/c++/current"
ln -sfn "$(basename "$GCC_DIR")" "$DEST/devkitARM/lib/gcc/arm-none-eabi/current"

echo "The headers are in $DEST"
