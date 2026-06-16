#!/usr/bin/env python3
"""Generate src/images/Logo120.h — the boot/sleep splash logo bitmap.

Design source: src/images/Logo120.svg — the "Mono X" mark from crosspoint-web
(public/logo.svg), recoloured for the 1-bit panel: terracotta tile -> black,
cream X -> white, centre knock-out diamond -> black.

This rasterizes the SAME geometry with the Python standard library only (no
cairosvg / Pillow), so the asset is reproducible on any machine. Keep the
constants below in sync with Logo120.svg.

Output matches the firmware's drawImage() 1bpp convention:
  120x120, row-major, MSB-first, 8px/byte, white=0xFF / black=0x00
  (bit 1 -> white pixel, bit 0 -> black pixel). No rotation — BootActivity /
  SleepActivity draw it via drawImage(), not drawIcon(), so no 90deg pre-rotate.

Usage:
  python3 scripts/gen_boot_logo.py            # write src/images/Logo120.h
  python3 scripts/gen_boot_logo.py --ascii     # also print an ASCII preview
  python3 scripts/gen_boot_logo.py --png out.png  # also write a PNG preview
"""
import math
import os
import struct
import sys
import zlib

SIZE = 120          # output bitmap is SIZE x SIZE
VB = 64.0           # SVG viewBox is 0..VB
SS = 4              # supersampling factor per axis (anti-aliasing before threshold)

# --- geometry in viewBox (0..64) coords; mirrors Logo120.svg ---
TILE_R = 15.0                       # rounded-rect corner radius (rx)
X_HALF = 9.5 / 2.0                  # X stroke half-width (stroke-width / 2)
X_SEGS = (((17.0, 17.0), (47.0, 47.0)),
          ((47.0, 17.0), (17.0, 47.0)))
DIA_CX = DIA_CY = 32.0              # centre diamond centre
DIA_HALF = 5.0                      # half side of the 10x10 square (pre-rotation)
COS45 = math.cos(math.radians(45.0))
SIN45 = math.sin(math.radians(45.0))


def in_tile(px, py):
    """Inside the rounded-rect tile (flat edges, rounded corners of radius TILE_R)."""
    if px < 0 or py < 0 or px > VB or py > VB:
        return False
    cx = min(max(px, TILE_R), VB - TILE_R)
    cy = min(max(py, TILE_R), VB - TILE_R)
    return (px - cx) ** 2 + (py - cy) ** 2 <= TILE_R * TILE_R


def on_x(px, py):
    """On either X stroke (butt caps: nothing beyond the segment endpoints)."""
    for (ax, ay), (bx, by) in X_SEGS:
        dx, dy = bx - ax, by - ay
        t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
        if t < 0.0 or t > 1.0:
            continue
        qx, qy = ax + t * dx, ay + t * dy
        if (px - qx) ** 2 + (py - qy) ** 2 <= X_HALF * X_HALF:
            return True
    return False


def in_diamond(px, py):
    """Inside the 45deg-rotated centre square (the knock-out diamond)."""
    u, v = px - DIA_CX, py - DIA_CY
    ur = u * COS45 + v * SIN45       # rotate point by -45deg into the square's frame
    vr = -u * SIN45 + v * COS45
    return abs(ur) <= DIA_HALF and abs(vr) <= DIA_HALF


def sample_white(px, py):
    """True if this viewBox point should be WHITE (bit 1) in the bitmap."""
    if not in_tile(px, py):
        return True                  # outside the tile -> page background (white)
    if on_x(px, py) and not in_diamond(px, py):
        return True                  # cream X stroke
    return False                     # black tile / black diamond knock-out


def pixel_white(X, Y):
    hits = 0
    for sy in range(SS):
        for sx in range(SS):
            px = (X + (sx + 0.5) / SS) * VB / SIZE
            py = (Y + (sy + 0.5) / SS) * VB / SIZE
            if sample_white(px, py):
                hits += 1
    return hits * 2 >= SS * SS        # >=50% coverage -> white


def build_rows():
    return [[1 if pixel_white(X, Y) else 0 for X in range(SIZE)] for Y in range(SIZE)]


def pack(rows):
    out = []
    for row in rows:
        for x in range(0, SIZE, 8):
            b = 0
            for k in range(8):
                if x + k < SIZE and row[x + k]:
                    b |= 1 << (7 - k)
            out.append(b)
    return out


def write_header(packed, path):
    body = []
    line = "    "
    for i, v in enumerate(packed):
        line += f"0x{v:02x}, "
        if (i + 1) % 19 == 0:
            body.append(line.rstrip())
            line = "    "
    if line.strip():
        body.append(line.rstrip())
    text = (
        "#pragma once\n#include <cstdint>\n\n"
        "// Image dimensions: 120x120\n"
        "// Source: src/images/Logo120.svg - regenerate via scripts/gen_boot_logo.py\n"
        "static const uint8_t Logo120[] = {\n"
        + "\n".join(body).rstrip(", ").rstrip()
    )
    text = text.rstrip(", \n") + "\n};\n"
    with open(path, "w") as f:
        f.write(text)


def print_ascii(rows, step=3):
    for Y in range(0, SIZE, step):
        print("".join("█" if rows[Y][X] == 0 else " " for X in range(0, SIZE, step)))


def write_png(rows, path, scale=2):
    W = H = SIZE * scale
    raw = bytearray()
    for Y in range(H):
        raw.append(0)                # filter type 0 for this scanline
        sy = Y // scale
        for X in range(W):
            raw.append(255 if rows[sy][X // scale] else 0)

    def chunk(typ, data):
        return (struct.pack(">I", len(data)) + typ + data
                + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_h = os.path.join(root, "src", "images", "Logo120.h")
    rows = build_rows()
    write_header(pack(rows), out_h)
    print(f"Wrote {out_h}")
    if "--ascii" in sys.argv:
        print_ascii(rows)
    if "--png" in sys.argv:
        png_path = sys.argv[sys.argv.index("--png") + 1]
        write_png(rows, png_path)
        print(f"Wrote {png_path}")


if __name__ == "__main__":
    main()
