#!/usr/bin/env python3
"""Recompresses PNGs written by astra_uipreview (stored deflate blocks) with zlib.

The preview tool intentionally has no zlib dependency, so it writes valid but
uncompressed PNGs. Run this after capturing screenshots to keep the repository
small:  python3 tools/recompress_png.py docs/screenshots
"""
import binascii
import pathlib
import struct
import sys
import zlib


def recompress(path: pathlib.Path) -> int:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return 0
    pos, idat, width, height = 8, b"", None, None
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height = struct.unpack(">II", chunk[:8])
        elif kind == b"IDAT":
            idat += chunk
        pos += 12 + length
    if width is None:
        return 0
    raw = zlib.decompress(idat)
    compressed = zlib.compress(raw, 9)

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    output = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", compressed) +
              chunk(b"IEND", b""))
    path.write_bytes(output)
    return len(output)


def main() -> int:
    targets = []
    for arg in sys.argv[1:] or ["docs/screenshots"]:
        path = pathlib.Path(arg)
        targets.extend(sorted(path.glob("*.png")) if path.is_dir() else [path])
    total = 0
    for target in targets:
        size = recompress(target)
        total += size
        print(f"{target}: {size // 1024} KB")
    print(f"{len(targets)} file(s), {total // 1024} KB total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
