#!/usr/bin/env python3
"""
    Reads a 3DS homebrew crash dump.

    The dump holds the registers of the ARM11, the stack pointer, the program
    counter and the return address, and a bit of the code that was running.
    The addresses are the ones of the binary that crashed, so they have to be
    looked up in the executable of that binary.

    The script prints what it finds, and leaves the addresses that look like
    code to tools/resolve-crash.sh.
"""

import struct
import sys

HEADER = {
    0x00: "magic",
    0x08: "version",
    0x14: "file size",
    0x2C: "arm9 base",
    0x30: "stack pointer",
    0x34: "program counter",
    0x5C: "system stack",
    0x60: "return address",
    0x64: "code address",
    0x68: "status register",
    0x74: "system stack top",
}

CODE_OFFSET = 0xB8
CODE_LENGTH = 44


def main():
    if len(sys.argv) < 2:
        print("usage: %s <dump> [executable]" % sys.argv[0])
        return 1

    data = open(sys.argv[1], "rb").read()

    if data[:4] != b"\xde\xc0\xad\xde":
        print("this does not look like a 3DS homebrew crash dump")
        return 1

    print("dump %s, %d bytes" % (sys.argv[1], len(data)))

    for offset, name in HEADER.items():
        if offset + 4 <= len(data):
            print("  %-18s 0x%08x" % (name, struct.unpack("<I", data[offset:offset + 4])[0]))

    print("\n  the general registers")
    for i in range(6):
        offset = 0xA0 + i * 4
        if offset + 4 <= len(data):
            print("    r%-2d 0x%08x" % (i, struct.unpack("<I", data[offset:offset + 4])[0]))

    print("\n  the floating point registers")
    for i in range(8):
        offset = 0x78 + i * 4
        if offset + 4 <= len(data):
            value = struct.unpack("<I", data[offset:offset + 4])[0]
            print("    s%-2d 0x%08x  %f" % (i, value, struct.unpack("<f", struct.pack("<I", value))[0]))

    code = data[CODE_OFFSET:CODE_OFFSET + CODE_LENGTH]
    print("\n  the code that was running")
    for i in range(0, len(code), 4):
        word = struct.unpack("<I", code[i:i + 4])[0]
        print("    %04x  %08x" % (CODE_OFFSET + i, word))

    tail = data[CODE_OFFSET + CODE_LENGTH:]
    name = tail.split(b"\x00")[0]
    print("\n  module %s" % name.decode("utf-8", "replace"))

    if len(sys.argv) > 2:
        print("\n  look the addresses up with:")
        print("    ./tools/resolve-crash.sh <address> <base>")

    return 0


if __name__ == "__main__":
    sys.exit(main())
