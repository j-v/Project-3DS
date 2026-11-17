#!/bin/sh
#
# Builds the project, runs the tests, or packages the installable CIA.
#
# The build runs inside the devkitPro container, so that the toolchain only
# has to be set up once. The workspace is mounted into the container, which
# means that every object file, every converted texture and the finished
# binary end up on the disk of the host, and that the writable layer of the
# container is thrown away when it exits.
#
# The container has no tool that can write a CIA, so makerom runs on the host:
# it only needs the ELF and the metadata that the container has just built.

set -e

IMAGE="${DKP_IMAGE:-devkitpro/devkitarm:latest}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
TARGET="${TARGET:-project-3ds}"

# makerom is pinned: the binary is downloaded once, checked against the
# checksum below, and kept in tools/.cache so that later builds work offline
MAKEROM_VERSION="0.19.0"

if ! command -v docker > /dev/null 2>&1; then
    echo "docker is needed to build the project" >&2
    exit 1
fi

docker_make() {
    docker run --rm \
        --user "$(id -u):$(id -g)" \
        --env HOME=/tmp \
        --volume "$ROOT:/workspace" \
        --workdir /workspace \
        "$IMAGE" \
        make "$@"
}

# The release binaries of makerom are named after the system they run on
makerom_platform() {
    case "$(uname -s):$(uname -m)" in
        Darwin:arm64)   echo macos_arm64 ;;
        Darwin:x86_64)  echo macos_x86_64 ;;
        Linux:x86_64)   echo ubuntu_x86_64 ;;
        Linux:amd64)    echo ubuntu_x86_64 ;;
        *)              echo "" ;;
    esac
}

makerom_checksum() {
    case "$1" in
        macos_arm64)   echo "ea222647bf362d1aa0d8b7570dc65e7968ce6b3d7b103e9cc815a99698d930e0" ;;
        macos_x86_64)  echo "a52563e5549cd2dd84e9b8d1cb0d1fdf15ea9bd0f6241e2e178fe4e4bf3a9277" ;;
        ubuntu_x86_64) echo "287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e" ;;
    esac
}

sha256_of() {
    if command -v shasum > /dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    elif command -v sha256sum > /dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    fi
}

extract_zip() {
    if command -v unzip > /dev/null 2>&1; then
        unzip -o -q "$1" -d "$2"
    elif command -v python3 > /dev/null 2>&1; then
        python3 -m zipfile -e "$1" "$2"
    else
        echo "unzip or python3 is needed to unpack makerom" >&2
        exit 1
    fi
}

# Prints the path of the makerom binary for this machine, downloading it into
# the project's cache on the first run
fetch_makerom() {
    platform="$1"
    cache="$ROOT/tools/.cache/makerom-$MAKEROM_VERSION-$platform"
    binary="$cache/makerom"

    if [ -x "$binary" ]; then
        echo "$binary"
        return
    fi

    mkdir -p "$cache"
    archive="$cache/makerom.zip"
    url="https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v$MAKEROM_VERSION/makerom-v$MAKEROM_VERSION-$platform.zip"

    echo "Downloading makerom $MAKEROM_VERSION" >&2
    if command -v curl > /dev/null 2>&1; then
        curl -fsSL -o "$archive" "$url"
    elif command -v wget > /dev/null 2>&1; then
        wget -q -O "$archive" "$url"
    else
        echo "curl or wget is needed to download makerom" >&2
        exit 1
    fi

    expected="$(makerom_checksum "$platform")"
    actual="$(sha256_of "$archive")"
    if [ -z "$actual" ]; then
        echo "no sha256 tool is available to check makerom" >&2
        rm -f "$archive"
        exit 1
    fi
    if [ "$actual" != "$expected" ]; then
        echo "makerom has the wrong checksum: expected $expected, got $actual" >&2
        rm -f "$archive"
        exit 1
    fi

    extract_zip "$archive" "$cache"
    rm -f "$archive"
    chmod +x "$binary"

    echo "$binary"
}

build_cia() {
    platform="$(makerom_platform)"
    if [ -z "$platform" ]; then
        echo "makerom has no binary for $(uname -s) $(uname -m)" >&2
        exit 1
    fi

    if [ ! -f "$ROOT/$TARGET.elf" ] || [ ! -f "$ROOT/$TARGET.smdh" ]; then
        echo "$TARGET.elf and $TARGET.smdh are needed to build the CIA" >&2
        exit 1
    fi

    makerom="$(fetch_makerom "$platform")"

    # The Home Menu draws the top screen from a banner, but the package is
    # valid without one. assets/banner.bnr is used unless BANNER points at
    # another one
    banner="${BANNER-$ROOT/assets/banner.bnr}"
    if [ ! -f "$banner" ] && [ -n "$BANNER" ]; then
        echo "BANNER is set to $banner, which does not exist" >&2
        exit 1
    fi
    if [ -f "$banner" ]; then
        set -- -banner "$banner"
    else
        set --
    fi

    echo "Packaging $TARGET.cia with makerom $MAKEROM_VERSION"
    "$makerom" -f cia -target t -exefslogo \
        -o "$ROOT/$TARGET.cia" \
        -elf "$ROOT/$TARGET.elf" \
        -rsf "$ROOT/$TARGET.rsf" \
        -icon "$ROOT/$TARGET.smdh" "$@"
}

# The tests are built as native binaries for the machine running this build,
# and they only need this project: the differential outcomes are frozen in
# tests/fixtures
if [ "$1" = "test" ]; then
    shift
    echo "Running the tests with $IMAGE"
    exec docker run --rm \
        --user "$(id -u):$(id -g)" \
        --env HOME=/tmp \
        --volume "$ROOT:/workspace" \
        --workdir /workspace \
        "$IMAGE" \
        make -f Makefile.tests BUILD=build-tests-container test "$@"
fi

# The CIA is built from the same ELF as the 3dsx, so this builds the project
# first and then packs the result
if [ "$1" = "cia" ]; then
    shift
    echo "Building with $IMAGE"
    docker_make "$@"
    build_cia
    exit 0
fi

echo "Building with $IMAGE"

docker_make "$@"
