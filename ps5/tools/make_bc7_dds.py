"""[ps5 port] Write a picture as the PS5 home screen's background DDS.

    python3 ps5/tools/make_bc7_dds.py <picture> <out.dds>

The launcher wants sce_sys/pic0.dds and pic1.dds as a single 3840x2160 BC7_UNORM
DX10 DDS without mipmaps (ps5-plume-triangle's validate-assets.sh). No BC7
encoder ships with Ubuntu, so this is a small one: every block in mode 6 (one
colour line for RGBA, 4-bit indices), endpoints along the block's principal
axis, the p-bit per endpoint chosen for the smaller error. Enough for a photo
seen across a room; it prints the PSNR of a decode so a regression shows.
Needs Pillow and numpy.
"""
import struct
import sys

import numpy as np
from PIL import Image

WEIGHTS = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], np.int32)


def blocks_of(img):
    a = np.asarray(img.convert("RGBA"), np.int32)
    h, w, _ = a.shape
    return a.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4), w, h


def palettes(e0, e1):
    """(n, 4) endpoints -> (n, 16, 4) colours, as the decoder interpolates."""
    return ((64 - WEIGHTS)[None, :, None] * e0[:, None, :] + WEIGHTS[None, :, None] * e1[:, None, :] + 32) >> 6


def quantize_endpoint(v):
    """(n, 4) floats -> (n, 4) 7-bit values and (n,) p-bits, decoded = q << 1 | p."""
    best_q, best_p, best_err = None, None, None
    for p in (0, 1):
        q = np.clip(np.rint((v - p) / 2.0), 0, 127).astype(np.int32)
        err = (((q << 1 | p) - v) ** 2).sum(1)
        if best_err is None:
            best_q, best_p, best_err = q, np.full(len(v), p, np.int32), err
        else:
            take = err < best_err
            best_q[take], best_p[take], best_err[take] = q[take], p, err[take]
    return best_q, best_p


def encode(blocks):
    n = len(blocks)
    px = blocks.astype(np.float64)
    mean = px.mean(1)
    centred = px - mean[:, None, :]
    cov = np.einsum("nki,nkj->nij", centred[:, :, :3], centred[:, :, :3])
    axis = np.ones((n, 3)) / np.sqrt(3)
    for _ in range(8):  # power iteration for the principal axis
        axis = np.einsum("nij,nj->ni", cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-9)
    t = np.einsum("nki,ni->nk", centred[:, :, :3], axis)
    lo, hi = t.min(1), t.max(1)
    e0 = mean.copy()
    e1 = mean.copy()
    e0[:, :3] += lo[:, None] * axis
    e1[:, :3] += hi[:, None] * axis
    e0[:, 3] = px[:, :, 3].min(1)
    e1[:, 3] = px[:, :, 3].max(1)
    q0, p0 = quantize_endpoint(np.clip(e0, 0, 255))
    q1, p1 = quantize_endpoint(np.clip(e1, 0, 255))
    # An opaque block must decode to alpha 255, which only p = 1 reaches.
    opaque = px[:, :, 3].min(1) >= 255
    for q, p, e in ((q0, p0, e0), (q1, p1, e1)):
        fix = opaque & (p == 0)
        p[fix] = 1
        q[fix] = np.clip(np.rint((np.clip(e[fix], 0, 255) - 1) / 2.0), 0, 127).astype(np.int32)
        q[fix, 3] = 127
    d0, d1 = q0 << 1 | p0[:, None], q1 << 1 | p1[:, None]
    idx = np.empty((n, 16), np.int32)
    for s in range(0, n, 65536):
        pal = palettes(d0[s:s + 65536], d1[s:s + 65536])
        diff = blocks[s:s + 65536, :, None, :] - pal[:, None, :, :]
        idx[s:s + 65536] = (diff * diff).sum(3).argmin(2)
    # The first index is stored in 3 bits: swap the endpoints where it is >= 8.
    flip = idx[:, 0] >= 8
    idx[flip] = 15 - idx[flip]
    q0[flip], q1[flip] = q1[flip].copy(), q0[flip].copy()
    p0[flip], p1[flip] = p1[flip].copy(), p0[flip].copy()

    # Pack: mode 6 (bit 6), R0 R1 G0 G1 B0 B1 A0 A1 (7 bits each), P0 P1, indices.
    out = np.zeros((n, 2), np.uint64)
    fields = [(np.full(n, 1 << 6, np.uint64), 7)]
    for c in range(4):
        fields += [(q0[:, c].astype(np.uint64), 7), (q1[:, c].astype(np.uint64), 7)]
    fields += [(p0.astype(np.uint64), 1), (p1.astype(np.uint64), 1), (idx[:, 0].astype(np.uint64), 3)]
    fields += [(idx[:, i].astype(np.uint64), 4) for i in range(1, 16)]
    pos = 0
    for value, bits in fields:
        for b in range(bits):
            bit = (value >> np.uint64(b)) & np.uint64(1)
            word, shift = divmod(pos, 64)
            out[:, word] |= bit << np.uint64(shift)
            pos += 1
    assert pos == 128
    return out.astype("<u8").tobytes(), (d0, d1, idx, flip)


def decoded(state):
    d0, d1, idx, flip = state
    # (d0/d1 are the pre-swap endpoints; undo the index flip to match them)
    i = idx.copy()
    i[flip] = 15 - i[flip]
    pal = palettes(d0, d1)
    return np.take_along_axis(pal, i[:, :, None].repeat(4, 2), 1)


def main():
    img = Image.open(sys.argv[1])
    if img.size != (3840, 2160):
        img = img.resize((3840, 2160), Image.LANCZOS)
    blocks, w, h = blocks_of(img)
    data, state = encode(blocks)
    # DDS_HEADER (flags: caps, height, width, pixel format, linear size), the
    # pixel format saying DX10, then DDS_HEADER_DXT10: BC7_UNORM (98), 2D.
    header = struct.pack("<4sIIIIIII44xII4sIIIIIIIIII", b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000, h, w,
                         len(data), 0, 1, 32, 0x4, b"DX10", 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0)
    dx10 = struct.pack("<IIIII", 98, 3, 0, 1, 0)
    with open(sys.argv[2], "wb") as f:
        f.write(header + dx10 + data)
    mse = ((decoded(state)[:, :, :3] - blocks[:, :, :3]) ** 2).mean()
    print("%s: %dx%d BC7, %d bytes, PSNR %.1f dB" % (sys.argv[2], w, h, len(header + dx10 + data),
                                                      10 * np.log10(255 ** 2 / max(mse, 1e-9))))


if __name__ == "__main__":
    main()
