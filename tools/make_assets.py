#!/usr/bin/env python3
"""
    Prepares the artwork of the port from the upstream Project-DS checkout.
    Archived here for reference only; the build does not run it.

    The port reuses the game's artwork as it is, and adds a second set of the
    note artwork for the multi-note variant: when more than one note is drawn
    at once, the white and the shadow pixels of a note are recoloured.

    The script only uses the standard library, so it runs anywhere Python
    does.
"""

import os
import struct
import sys
import zlib

SOURCE = "../Project-DS/gfx"
TARGET = "gfx"

# The colour that marks transparent pixels
TRANSPARENT = (128, 128, 128)

# The colours replaced in the multi-note variant: the lightest colour of the
# palette becomes yellow, and the darkest one becomes a dark orange
LIGHTEST = (255, 255, 0)
DARKEST = (25, 5, 0)

# The artwork that has a multi-note variant. The game only selects the
# multi_* textures for notes, holes and sliders; the UI artwork (judgements,
# life gauge, arrow, hold bar, combo numbers, song list) never uses one.
# Keep this list in sync with the TextureMulti* entries in render_context.h.
MULTI_BASES = {
    "triangle", "circle", "cross", "square",
    "triangle_hole", "circle_hole", "cross_hole", "square_hole",
    "slider_l", "slider_r", "slider_lh", "slider_rh",
    "slider_l_hole", "slider_r_hole", "slider_lh_hole", "slider_rh_hole",
}


def read_bmp(path):
    """Reads a palettized BMP file and returns its width, height and RGBA pixels."""
    data = open(path, "rb").read()

    offset = struct.unpack("<I", data[10:14])[0]
    width = struct.unpack("<i", data[18:22])[0]
    height = struct.unpack("<i", data[22:26])[0]
    bits = struct.unpack("<H", data[28:30])[0]

    if bits != 4:
        raise ValueError("%s has to be a 4 bit BMP file" % path)

    colours = []

    for i in range(16):
        entry = data[14 + 40 + i * 4:14 + 40 + i * 4 + 4]
        colours.append((entry[2], entry[1], entry[0], 255))

    stride = (width + 1) // 2
    stride = (stride + 3) // 4 * 4
    rows = abs(height)
    pixels = bytearray(width * rows * 4)

    for row in range(rows):
        source = offset + (rows - 1 - row) * stride if height > 0 else offset + row * stride

        for x in range(width):
            byte = data[source + x // 2]
            index = (byte >> 4) if x % 2 == 0 else (byte & 0x0F)
            colour = colours[index]
            at = (row * width + x) * 4
            pixels[at] = colour[0]
            pixels[at + 1] = colour[1]
            pixels[at + 2] = colour[2]
            pixels[at + 3] = colour[3]

    return width, rows, pixels


def resize_nearest(width, height, pixels, newWidth, newHeight):
    """Resizes an image to the size that is wanted."""
    out = bytearray(newWidth * newHeight * 4)

    for y in range(newHeight):
        sourceY = y * height // newHeight

        for x in range(newWidth):
            sourceX = x * width // newWidth
            source = (sourceY * width + sourceX) * 4
            target = (y * newWidth + x) * 4
            out[target:target + 4] = pixels[source:source + 4]

    return out


def read_png(path):
    """Reads a PNG file and returns its width, height and RGBA pixels."""
    data = open(path, "rb").read()

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s is not a PNG file" % path)

    width = height = 0
    depth = colour = 0
    idat = b""

    pos = 8
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length

        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break

    if depth != 8 or colour not in (2, 6):
        raise ValueError("%s has to be an 8 bit RGB or RGBA image" % path)

    channels = 4 if colour == 6 else 3

    raw = zlib.decompress(idat)
    stride = width * channels
    pixels = bytearray(height * stride)
    prev = bytearray(stride)

    pos = 0
    for row in range(height):
        filter = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride

        for i in range(stride):
            left = line[i - 4] if i >= 4 else 0
            up = prev[i]
            upleft = prev[i - 4] if i >= 4 else 0

            if filter == 1:
                line[i] = (line[i] + left) & 0xFF
            elif filter == 2:
                line[i] = (line[i] + up) & 0xFF
            elif filter == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
            elif filter == 4:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                if pa <= pb and pa <= pc:
                    pred = left
                elif pb <= pc:
                    pred = up
                else:
                    pred = upleft
                line[i] = (line[i] + pred) & 0xFF

        if channels == 3:
            for i in range(0, stride, 3):
                line[i + 3:i + 3] = b"\xff"

        pixels[row * stride:(row + 1) * stride] = line
        prev = line

    return width, height, pixels


def write_png(path, width, height, pixels):
    """Writes RGBA pixels into a PNG file."""
    raw = bytearray()

    for row in range(height):
        raw.append(0)
        raw += pixels[row * width * 4:(row + 1) * width * 4]

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", header)
    out += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += chunk(b"IEND", b"")

    open(path, "wb").write(out)


def main():
    if not os.path.isdir(SOURCE):
        print("The source artwork is not in %s" % SOURCE, file=sys.stderr)
        return 1

    os.makedirs(TARGET, exist_ok=True)

    # Remove multi variants that are no longer generated, so a changed list
    # does not leave stale files behind
    for existing in os.listdir(TARGET):
        if existing.startswith("multi_") and existing.endswith(".png"):
            if existing[len("multi_"):-4] not in MULTI_BASES:
                os.remove(os.path.join(TARGET, existing))

    names = sorted(name for name in os.listdir(SOURCE) if name.endswith(".png"))

    for name in names:
        width, height, pixels = read_png(os.path.join(SOURCE, name))
        write_png(os.path.join(TARGET, name), width, height, pixels)

        # The transparent colour is grey, so the pixels that use it are made
        # transparent here as well
        for i in range(0, len(pixels), 4):
            if (pixels[i], pixels[i + 1], pixels[i + 2]) == TRANSPARENT:
                pixels[i + 3] = 0

        write_png(os.path.join(TARGET, name), width, height, pixels)

        # The multi-note variant of the artwork
        if name.startswith("multi_"):
            continue

        if name[:-4] not in MULTI_BASES:
            continue

        variant = bytearray(pixels)
        colours = set()

        for i in range(0, len(variant), 4):
            if variant[i + 3]:
                colours.add((variant[i], variant[i + 1], variant[i + 2]))

        if len(colours) < 2:
            continue

        def luminance(colour):
            return colour[0] * 3 + colour[1] * 6 + colour[2]

        lightest = max(colours, key=luminance)
        darkest = min(colours, key=luminance)

        for i in range(0, len(variant), 4):
            colour = (variant[i], variant[i + 1], variant[i + 2])

            if colour == lightest:
                variant[i] = LIGHTEST[0]
                variant[i + 1] = LIGHTEST[1]
                variant[i + 2] = LIGHTEST[2]
            elif colour == darkest:
                variant[i] = DARKEST[0]
                variant[i + 1] = DARKEST[1]
                variant[i + 2] = DARKEST[2]

        write_png(os.path.join(TARGET, "multi_" + name), width, height, variant)

    # The home menu icon comes from the source artwork, at the size the 3DS
    # metadata wants
    icon = os.path.join(SOURCE, "..", "icon.bmp")

    if os.path.isfile(icon):
        width, height, pixels = read_bmp(icon)
        pixels = resize_nearest(width, height, pixels, 48, 48)
        write_png(os.path.join(TARGET, "icon.png"), 48, 48, pixels)
        print("Prepared the icon")

    print("Prepared %d images" % len(names))
    return 0


if __name__ == "__main__":
    sys.exit(main())
