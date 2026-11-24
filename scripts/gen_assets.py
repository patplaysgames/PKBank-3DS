#!/usr/bin/env python3
"""
Generates default Pearl White 3DS Home Menu assets (meta/icon.png, meta/banner.png, meta/audio.wav)
using only the Python 3 standard library so 'make release' builds .3dsx and .cia out of the box.
Replace meta/icon.png (48x48), meta/banner.png (256x128), or meta/audio.wav with custom artwork anytime.
"""
import math
import os
import struct
import wave
import zlib

os.makedirs("meta", exist_ok=True)


def write_png(path: str, width: int, height: int, pixels: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)  # filter type None
        raw.extend(pixels[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def make_pokeball_canvas(width: int, height: int) -> bytes:
    buf = bytearray(width * height * 4)
    cx, cy = width / 2.0, height / 2.0
    radius = min(width, height) * 0.36
    for y in range(height):
        for x in range(width):
            dx, dy = x - cx, y - cy
            dist = math.hypot(dx, dy)
            # Pearl White background (#FFFFFF) with subtle slate border
            r, g, b, a = 255, 255, 255, 255
            if x < 2 or y < 2 or x >= width - 2 or y >= height - 2:
                r, g, b = 226, 232, 240
            if dist <= radius:
                if abs(dy) <= max(2.0, radius * 0.14):
                    r, g, b = 30, 41, 59  # center band
                elif dy < 0:
                    r, g, b = 16, 185, 129  # emerald top hemisphere
                else:
                    r, g, b = 248, 250, 252  # pearl bottom hemisphere
                if dist >= radius - max(2.0, radius * 0.12):
                    r, g, b = 30, 41, 59  # outer outline
                if dist <= radius * 0.32:
                    r, g, b = 30, 41, 59
                if dist <= radius * 0.20:
                    r, g, b = 255, 255, 255
            idx = (y * width + x) * 4
            buf[idx : idx + 4] = bytes((r, g, b, a))
    return bytes(buf)


def write_chime_wav(path: str) -> None:
    sample_rate = 22050
    duration_sec = 0.65
    n_samples = int(sample_rate * duration_sec)
    with wave.open(path, "w") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            t = i / sample_rate
            freq = 659.25 if t < 0.22 else 880.0  # crisp two-tone chime (E5 -> A5)
            env = max(0.0, 1.0 - (t / duration_sec))
            sample = int(9000 * env * math.sin(2.0 * math.pi * freq * t))
            frames.extend(struct.pack("<h", sample))
        wf.writeframes(bytes(frames))


if not os.path.exists("meta/icon.png"):
    write_png("meta/icon.png", 48, 48, make_pokeball_canvas(48, 48))
if not os.path.exists("meta/banner.png"):
    write_png("meta/banner.png", 256, 128, make_pokeball_canvas(256, 128))
if not os.path.exists("meta/audio.wav"):
    write_chime_wav("meta/audio.wav")

print("Default 3DS Home Menu assets verified in meta/.")
