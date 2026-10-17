"""[ps5 port] Turn a frame report (logs/frame_reports/NNN) into PNGs.

A frame report is every resolve the game made during a couple of frames - the
copies out of EDRAM that hold the scene, the post-processing passes and the
front buffer - read back from the GPU's copy of guest memory by the Vulkan
backend (frame_report_now / frame_report_after_seconds). Each res_NNNN.bin is
the raw tiled guest bytes; index.txt says how to read them.

    python3 ps5/tools/decode_frame_report.py <report folder> [out folder]

Writes res_NNNN_<w>x<h>_f<format>.png and prints, per resolve, its size,
format and the mean and maximum of each channel - a black image says so in the
numbers before anyone opens it. Float formats are shown clamped to 0..1 (HDR
values above 1 saturate). Needs Pillow.
"""
import os
import re
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from upscale_textures import tiled_offset_2d  # noqa: E402  (the SDK's GetTiledOffset2D, ported)

# xenos::ColorFormat -> (bytes per pixel, decoder name)
FORMATS = {
    2: (1, "r8"),
    6: (4, "rgba8"),
    7: (4, "rgb10a2"),
    10: (2, "rg8"),
    14: (4, "rgba8"),
    16: (4, "r10g11b11"),
    17: (4, "r11g11b10"),
    24: (2, "r16"),
    25: (4, "rg16"),
    26: (8, "rgba16"),
    30: (2, "r16f"),
    31: (4, "rg16f"),
    32: (8, "rgba16f"),
    36: (4, "r32f"),
    37: (8, "rg32f"),
    38: (16, "rgba32f"),
    50: (4, "rgba8"),
    54: (4, "rgb10a2"),
    55: (4, "r10g11b11"),
    56: (4, "r11g11b10"),
}


def swap_endian(data, endian, element):
    """xenos::Endian128 per element: none, 8in16, 8in32, 16in32, 8in64, 8in128."""
    b = bytearray(data)
    n = len(b)
    if endian == 1:
        b[0:n - n % 2:2], b[1:n - n % 2:2] = b[1:n - n % 2:2], b[0:n - n % 2:2]
    elif endian == 2:
        for i in range(0, n - 3, 4):
            b[i:i + 4] = b[i:i + 4][::-1]
    elif endian == 3:
        for i in range(0, n - 3, 4):
            b[i:i + 4] = b[i + 2:i + 4] + b[i:i + 2]
    elif endian == 4:
        for i in range(0, n - 7, 8):
            b[i:i + 8] = b[i:i + 8][::-1]
    elif endian == 5:
        for i in range(0, n - 15, 16):
            b[i:i + 16] = b[i:i + 16][::-1]
    return bytes(b)


def half(h):
    return struct.unpack("<e", struct.pack("<H", h))[0]


def clamp8(v):
    if v != v:  # NaN
        return 0
    return max(0, min(255, int(v * 255.0 + 0.5)))


def decode_pixel(name, px):
    """-> (r, g, b, a) as floats, roughly 0..1."""
    if name == "rgba8":
        return px[0] / 255, px[1] / 255, px[2] / 255, px[3] / 255
    if name == "r8":
        return px[0] / 255, px[0] / 255, px[0] / 255, 1.0
    if name == "rg8":
        return px[0] / 255, px[1] / 255, 0.0, 1.0
    v = int.from_bytes(px[:4], "little") if len(px) >= 4 else 0
    if name == "rgb10a2":
        return (v & 1023) / 1023, ((v >> 10) & 1023) / 1023, ((v >> 20) & 1023) / 1023, (v >> 30) / 3
    if name in ("r10g11b11", "r11g11b10"):
        return (v & 2047) / 2047, ((v >> 11) & 2047) / 2047, ((v >> 22) & 1023) / 1023, 1.0
    if name == "r16":
        r = int.from_bytes(px[:2], "little") / 65535
        return r, r, r, 1.0
    if name == "rg16":
        return int.from_bytes(px[0:2], "little") / 65535, int.from_bytes(px[2:4], "little") / 65535, 0.0, 1.0
    if name == "rgba16":
        c = [int.from_bytes(px[i:i + 2], "little") / 65535 for i in range(0, 8, 2)]
        return c[0], c[1], c[2], c[3]
    if name == "r16f":
        r = half(int.from_bytes(px[:2], "little"))
        return r, r, r, 1.0
    if name == "rg16f":
        return half(int.from_bytes(px[0:2], "little")), half(int.from_bytes(px[2:4], "little")), 0.0, 1.0
    if name == "rgba16f":
        c = [half(int.from_bytes(px[i:i + 2], "little")) for i in range(0, 8, 2)]
        return c[0], c[1], c[2], c[3]
    if name == "r32f":
        r = struct.unpack("<f", px[:4])[0]
        return r, r, r, 1.0
    if name == "rg32f":
        r, g = struct.unpack("<ff", px[:8])
        return r, g, 0.0, 1.0
    if name == "rgba32f":
        return struct.unpack("<ffff", px[:16])
    return 0.0, 0.0, 0.0, 1.0


def main():
    folder = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(folder, "png")
    os.makedirs(out, exist_ok=True)
    for line in open(os.path.join(folder, "index.txt")):
        f = dict(re.findall(r"(\w+) (-?[0-9A-Fa-f+]+)", line))
        seq = int(line.split()[0])
        path = os.path.join(folder, "res_%04d.bin" % seq)
        written = re.search(r"written ([0-9A-F]+)\+([0-9A-F]+)", line)
        if not os.path.isfile(path) or not written:
            print("%4d  (no data) %s" % (seq, line.strip()[:120]))
            continue
        written_addr = int(written.group(1), 16)
        dest_base = int(f["dest_base"], 16)
        fmt = int(f["dest_format"])
        endian = int(f["dest_endian"])
        pitch, height = int(f["pitch"]), int(f["height"])
        if fmt not in FORMATS or pitch == 0 or height == 0:
            print("%4d  format %d %dx%d: not decoded" % (seq, fmt, pitch, height))
            continue
        bpp, name = FORMATS[fmt]
        data = swap_endian(open(path, "rb").read(), endian, bpp)
        base_shift = written_addr - dest_base  # the file starts this far into the image
        bpb_log2 = bpp.bit_length() - 1
        img = Image.new("RGB", (pitch, height))
        pix = img.load()
        sums = [0.0, 0.0, 0.0, 0.0]
        maxs = [0.0, 0.0, 0.0, 0.0]
        n = 0
        for y in range(height):
            for x in range(pitch):
                off = tiled_offset_2d(x, y, pitch, bpb_log2) - base_shift
                if off < 0 or off + bpp > len(data):
                    continue
                c = decode_pixel(name, data[off:off + bpp])
                for i in range(4):
                    v = c[i] if c[i] == c[i] else 0.0
                    sums[i] += v
                    maxs[i] = max(maxs[i], v)
                n += 1
                pix[x, y] = (clamp8(c[0]), clamp8(c[1]), clamp8(c[2]))
        name_out = "res_%04d_%dx%d_f%d.png" % (seq, pitch, height, fmt)
        img.save(os.path.join(out, name_out))
        means = [s / max(n, 1) for s in sums]
        print("%4d  %4dx%-4d f%-2d %-9s src %s/%s  mean %.3f %.3f %.3f %.3f  max %.2f %.2f %.2f  -> %s" %
              (seq, pitch, height, fmt, name, f.get("src"), f.get("src_format"), means[0], means[1], means[2],
               means[3], maxs[0], maxs[1], maxs[2], name_out))


if __name__ == "__main__":
    main()
