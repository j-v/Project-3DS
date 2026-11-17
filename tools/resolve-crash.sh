#!/bin/sh
#
# Turns an address of a crash dump into a place in the source.
#
# The crash dumps of the homebrew launchers hold the address of the
# instruction that crashed, and the address the module of the game was
# loaded at. The difference is the offset into the program, which is what
# this script looks up.
#
#     ./tools/resolve-crash.sh 0x123456
#     ./tools/resolve-crash.sh 0x123456 0x08000000
#
# The address has to be looked up in the executable of the build that
# crashed, which can be given as a third argument.

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${3:-$ROOT/project-3ds.elf}"
IMAGE="${DKP_IMAGE:-devkitpro/devkitarm:latest}"

if [ ! -f "$ELF" ]; then
    echo "$ELF is missing, build the project first" >&2
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "usage: $0 <address> [module base]" >&2
    exit 1
fi

ADDRESS="$1"
BASE="${2:-0}"

# The offset of the instruction inside the program
OFFSET=$(printf '0x%x' $(( $(printf '%d' "$ADDRESS") - $(printf '%d' "$BASE") )))

echo "address $ADDRESS, base $BASE, offset $OFFSET"

exec docker run --rm --platform linux/arm64 \
    --volume "$ROOT:/workspace" \
    --volume "$(dirname "$ELF"):/elf" \
    --workdir /workspace \
    "$IMAGE" \
    /opt/devkitpro/devkitARM/bin/arm-none-eabi-addr2line -f -C -i -e "/elf/$(basename "$ELF")" "$OFFSET"
