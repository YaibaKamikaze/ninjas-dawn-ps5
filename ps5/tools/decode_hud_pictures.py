"""[ps5 port] Turn a HUD picture dump (logs/hud_pictures) into PNGs.

SETTINGS > Developer > Dump HUD pictures writes every small picture the game
loads, once each, as the raw guest bytes. This decodes them so the Xbox button
pictures can be found and their hashes copied into
ps5/package/hub/button_prompts/playstation/.

    python3 ps5/tools/decode_hud_pictures.py <hud_pictures folder> [out folder]

The game's 2D HUD is drawn from 8-bit index pictures (k_8, format 2) looked up
in a 256x1 colour table (format 6) that it keeps in the page after the
picture. The dump names carry the guest address (_a<addr>), so each index
picture is written twice: <hash>_..._index.png (the raw indices as grey) and
<hash>_....png with the colour table applied. Needs Pillow.
"""
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import upscale_textures as ut  # noqa: E402

NAME = re.compile(r"^([0-9A-F]{16})_(\d+)x(\d+)_f(\d+)_t(\d)_e(\d)_p(\d+)_m(\d+)(?:_a([0-9A-F]{8}))?\.bin$")


def decode(data, w, h, fmt, tiled, endian, pitch):
    """-> RGBA bytes, or None for a format the tool does not read."""
    if fmt not in ut.FMT_INFO:
        return None
    bpb, block = ut.FMT_INFO[fmt]
    data = ut.swap_endian(data, endian)
    if tiled:
        data = ut.untile(data, w, h, bpb, block, max(1, max(pitch * 32, w) // block))
    px = ut.decode_dxt(data, w, h, fmt) if fmt in ut.DXT_FORMATS else ut.decode_plain(data, w, h, fmt)
    return bytes(px)


def read_table(data, tiled, endian, pitch):
    """A HUD colour table -> 256 RGBA entries as bytes. Once byte-swapped its
    entries are B, G, R, A (the face buttons come out blue X, yellow Y, red B,
    green A only this way round)."""
    data = ut.swap_endian(data, endian)
    if tiled:
        data = ut.untile(data, 256, 1, 4, 1, max(pitch * 32, 256))
    return b"".join(bytes((data[i * 4 + 2], data[i * 4 + 1], data[i * 4], data[i * 4 + 3])) for i in range(256))


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(src, "png")
    os.makedirs(out, exist_ok=True)
    entries = []
    for fn in sorted(os.listdir(src)):
        m = NAME.match(fn)
        if m:
            h16, w, h, fmt, tiled, endian, pitch, _mips, addr = m.groups()
            entries.append((fn, h16, int(w), int(h), int(fmt), int(tiled), int(endian), int(pitch),
                            int(addr, 16) if addr else None))

    # Colour tables by address, for the index pictures.
    tables = {}
    for fn, h16, w, h, fmt, tiled, endian, pitch, addr in entries:
        if addr is not None and fmt == 6 and (w, h) == (256, 1):
            tables[addr] = read_table(open(os.path.join(src, fn), "rb").read(), tiled, endian, pitch)
    table_addrs = sorted(tables)

    done = paired = 0
    for fn, h16, w, h, fmt, tiled, endian, pitch, addr in entries:
        try:
            rgba = decode(open(os.path.join(src, fn), "rb").read(), w, h, fmt, tiled, endian, pitch)
        except Exception as e:  # a short or odd dump: say so and go on
            print("cannot decode %s: %s" % (fn, e))
            continue
        if rgba is None:
            continue
        stem = "%s_%dx%d_f%d" % (h16, w, h, fmt)
        if fmt == 2 and addr is not None:
            Image.frombytes("RGBA", (w, h), rgba).save(os.path.join(out, stem + "_index.png"))
            # The first colour table after the picture, within the next 1 MB.
            table = next((tables[a] for a in table_addrs if addr < a <= addr + (1 << 20)), None)
            if table is not None:
                lut = [table[i * 4:i * 4 + 4] for i in range(256)]
                rgba = b"".join(lut[rgba[i * 4]] for i in range(w * h))
                paired += 1
        Image.frombytes("RGBA", (w, h), rgba).save(os.path.join(out, stem + ".png"))
        done += 1
    print("decoded %d picture(s), %d index picture(s) with their colour table -> %s" % (done, paired, out))


if __name__ == "__main__":
    main()
