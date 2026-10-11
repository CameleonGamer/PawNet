#!/usr/bin/env python3
"""Draws the PawNet app icon as a PNG, without any third-party library.

The paw is the same geometry as the inline SVG on the home page (see
src/home_page.cc), written on a 64 x 64 grid:

    python3 tools/make_icon.py assets/pawnet-256.png
"""

import math
import struct
import sys
import zlib

SIZE = 256
GRID = 64
SUPERSAMPLE = 4
BACKGROUND = (0x16, 0x18, 0x1D)
ACCENT = (0xFF, 0x7F, 0x62)  # coral, the accent colour of the home page
CORNER_RADIUS = 14  # on the 64-unit grid

# (cx, cy, rx, ry, rotation in degrees)
TOES = [
    (12, 30, 5.5, 7.0, -20),
    (25, 16, 5.5, 7.5, -8),
    (39, 16, 5.5, 7.5, 8),
    (52, 30, 5.5, 7.0, 20),
]
PAD = (32, 44, 14, 12, 0)


def inside_ellipse(x, y, cx, cy, rx, ry, angle):
    radians = math.radians(-angle)
    dx, dy = x - cx, y - cy
    rx_local = dx * math.cos(radians) - dy * math.sin(radians)
    ry_local = dx * math.sin(radians) + dy * math.cos(radians)
    return (rx_local / rx) ** 2 + (ry_local / ry) ** 2 <= 1


def inside_rounded_square(x, y):
    r = CORNER_RADIUS
    if r <= x <= GRID - r or r <= y <= GRID - r:
        return 0 <= x <= GRID and 0 <= y <= GRID
    cx = r if x < r else GRID - r
    cy = r if y < r else GRID - r
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def paw_coverage(x, y):
    if any(inside_ellipse(x, y, *toe) for toe in TOES):
        return True
    return inside_ellipse(x, y, *PAD)


def render():
    pixels = bytearray()
    step = 1.0 / SUPERSAMPLE
    scale = GRID / SIZE
    samples = SUPERSAMPLE * SUPERSAMPLE
    for py in range(SIZE):
        pixels.append(0)  # PNG filter type 0 for each row
        for px in range(SIZE):
            paw = 0
            tile = 0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    x = (px + (sx + 0.5) * step) * scale
                    y = (py + (sy + 0.5) * step) * scale
                    if inside_rounded_square(x, y):
                        tile += 1
                        if paw_coverage(x, y):
                            paw += 1
            if tile == 0:
                pixels += bytes((0, 0, 0, 0))
                continue
            paw_fraction = paw / samples
            tile_alpha = round(255 * tile / samples)
            color = [
                round(BACKGROUND[i] * (1 - paw_fraction) + ACCENT[i] * paw_fraction)
                for i in range(3)
            ]
            pixels += bytes((color[0], color[1], color[2], tile_alpha))
    return bytes(pixels)


def png_chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def write_png(path, raw_rows):
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)  # RGBA, 8 bits
    data = zlib.compress(raw_rows, 9)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(png_chunk(b"IHDR", header))
        out.write(png_chunk(b"IDAT", data))
        out.write(png_chunk(b"IEND", b""))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    write_png(sys.argv[1], render())


if __name__ == "__main__":
    main()
