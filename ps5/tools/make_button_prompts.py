"""[ps5 port] Draw the PlayStation button prompts and write them as .rect patches.

    python3 ps5/tools/make_button_prompts.py <hud_pictures dump> [out folder] [--preview folder]

The game's 2D HUD is 8-bit index pictures looked up in a 256-entry colour table
(see decode_hud_pictures.py). This draws this project's own pictures - the
PlayStation face buttons, L1/R1, L2/R2, the touch pad and OPTIONS, and the few
text labels that name Xbox features - and writes each as a .rect file
(ps5_button_prompts.cpp): only the rectangles drawn here, as indices into the
picture's own colour table, pasted over the game's picture in memory on the
console. Nothing of the game's art is copied: the dump (the owner's own, from
SETTINGS > Developer > Dump HUD pictures) is read only for each picture's
colour table, size and layout, and to make the preview.

The output goes to ps5/package/hub/button_prompts/playstation/ by default.
--preview writes before/after PNGs to look at (they show the game's art, so
they belong in a scratch folder, never in the repo). Needs Pillow and numpy.
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
sys.path.insert(0, HERE)
import upscale_textures as ut  # noqa: E402
from decode_hud_pictures import NAME, read_table  # noqa: E402

FONT_DIR = os.path.join(HERE, "..", "assets", "fonts")
SERIF = os.path.join(FONT_DIR, "cinzel-700.ttf")  # (OFL, ships with the hub)
TYPEWRITER = "/usr/share/fonts/opentype/urw-base35/NimbusMonoPS-Bold.otf"  # rendered here; the font is not shipped
ROMAN = "/usr/share/fonts/opentype/urw-base35/NimbusRoman-Bold.otf"  # (likewise; Cinzel has no plain 1)
S = 4  # supersampling

# The pictures, by the hash prefix of the Xbox picture (from the owner's dump).
BUTTON_ATLAS = "8F9624F514C0E9AB"   # 512x256: 64x64 cells, face buttons / triggers / sticks / d-pad
MARKETPLACE = "F3C472A55C851518"    # 520x90 "Xbox LIVE Marketplace"
PRESS_START = "D66D4AD32FB753CE"    # 585x91 "PRESS START BUTTON"
ACTION_STRIP = "572F595E"           # 140x640 action names, one per 40-pixel row ("Gamer Card" is row 13)

# PlayStation colours for the symbols (the colour table decides the nearest it has).
PINK, GREEN, RED, BLUE = (232, 140, 200), (90, 200, 120), (230, 80, 90), (120, 170, 240)


# --- The dump --------------------------------------------------------------

class Dump:
    def __init__(self, folder):
        self.folder = folder
        self.entries = []
        for fn in os.listdir(folder):
            m = NAME.match(fn)
            if m and m.group(9):
                h16, w, h, fmt, tiled, endian, pitch, _mips, addr = m.groups()
                self.entries.append(dict(fn=fn, hash=h16, w=int(w), h=int(h), fmt=int(fmt), tiled=int(tiled),
                                         endian=int(endian), pitch=int(pitch), addr=int(addr, 16)))
        self.tables = sorted((e for e in self.entries if e["fmt"] == 6 and (e["w"], e["h"]) == (256, 1)),
                             key=lambda e: e["addr"])

    def picture(self, prefix):
        e = next((e for e in self.entries if e["hash"].startswith(prefix) and e["fmt"] == 2), None)
        if e is None:
            return None
        raw = open(os.path.join(self.folder, e["fn"]), "rb").read()
        pitch = max(e["pitch"] * 32, e["w"])
        idx = np.frombuffer(ut.untile(raw, e["w"], e["h"], 1, 1, pitch)[:e["w"] * e["h"]], np.uint8)
        t = next((t for t in self.tables if e["addr"] < t["addr"] <= e["addr"] + (1 << 20)), None)
        if t is None:  # its colour table was not dumped
            return None
        tb = read_table(open(os.path.join(self.folder, t["fn"]), "rb").read(), t["tiled"], t["endian"], t["pitch"])
        table = np.frombuffer(tb, np.uint8).reshape(256, 4).astype(np.float32)
        return dict(entry=e, index=idx.reshape(e["h"], e["w"]).copy(), table=table)


# --- Colour table matching -----------------------------------------------------

def quantize(rgba, table, transparent):
    """RGBA image (straight alpha) -> indices into `table`. Compared
    premultiplied, so a faint edge pixel picks a faint entry."""
    a = np.asarray(rgba, np.float32)
    h, w, _ = a.shape
    px = a.reshape(-1, 4)
    out = np.full(len(px), transparent, np.uint8)
    keep = px[:, 3] >= 12
    if keep.any():
        p = px[keep]
        pm = np.concatenate([p[:, :3] * (p[:, 3:4] / 255.0), p[:, 3:4] * 1.5], 1)
        usable = table[:, 3] > 0
        tt = table[usable]
        tm = np.concatenate([tt[:, :3] * (tt[:, 3:4] / 255.0), tt[:, 3:4] * 1.5], 1)
        ids = np.nonzero(usable)[0]
        best = np.empty(len(pm), np.int64)
        for s in range(0, len(pm), 4096):
            d = ((pm[s:s + 4096, None, :] - tm[None, :, :]) ** 2).sum(2)
            best[s:s + 4096] = ids[d.argmin(1)]
        out[keep] = best
    return out.reshape(h, w)


def transparent_index(pic):
    """The table entry the picture's background uses (alpha 0)."""
    values, counts = np.unique(pic["index"], return_counts=True)
    for v in values[np.argsort(-counts)]:
        if pic["table"][v, 3] == 0:
            return int(v)
    return int(np.nonzero(pic["table"][:, 3] == 0)[0][0])


# --- Drawing helpers (all at S x) ------------------------------------------------

def canvas(w, h):
    return Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))


def down(img):
    return img.resize((img.width // S, img.height // S), Image.LANCZOS)


def sphere(img, cx, cy, r, dark, light, rim):
    """A glossy round button: a dark body lit from the top left, a thin rim."""
    d = ImageDraw.Draw(img)
    cx, cy, r = cx * S, cy * S, r * S
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=rim)
    r2 = r - 1.4 * S
    steps = 40
    for i in range(steps):
        t = i / (steps - 1)
        rr = r2 * (1 - t * 0.92)
        ox, oy = -r2 * 0.28 * t, -r2 * 0.32 * t
        c = tuple(int(dark[k] + (light[k] - dark[k]) * t ** 1.6) for k in range(3)) + (255,)
        d.ellipse((cx + ox - rr, cy + oy - rr, cx + ox + rr, cy + oy + rr), fill=c)


def pill(img, box, horizontal, light, dark, outline):
    """A silver trigger or bumper plate, shaded like a cylinder."""
    x0, y0, x1, y1 = [v * S for v in box]
    mask = Image.new("L", img.size, 0)
    rad = (min(x1 - x0, y1 - y0)) / 2
    ImageDraw.Draw(mask).rounded_rectangle((x0, y0, x1, y1), rad, fill=255)
    grad = Image.new("RGBA", img.size)
    gd = ImageDraw.Draw(grad)
    span = (y1 - y0) if horizontal else (x1 - x0)
    for i in range(int(span) + 1):
        t = i / max(span, 1)
        k = 1 - abs(t - 0.38) * 1.5
        c = tuple(int(dark[j] + (light[j] - dark[j]) * max(0.0, min(1.0, k))) for j in range(3)) + (255,)
        if horizontal:
            gd.line((x0, y0 + i, x1, y0 + i), fill=c)
        else:
            gd.line((x0 + i, y0, x0 + i, y1), fill=c)
    img.alpha_composite(Image.composite(grad, Image.new("RGBA", img.size), mask))
    ImageDraw.Draw(img).rounded_rectangle((x0, y0, x1, y1), rad, outline=outline, width=int(1.2 * S))


def text(img, xy, s, size, fill, font=SERIF, anchor="mm", outline=None, outline_width=0):
    f = ImageFont.truetype(font, int(size * S))
    d = ImageDraw.Draw(img)
    if outline:
        d.text((xy[0] * S, xy[1] * S), s, font=f, fill=outline, anchor=anchor, stroke_width=int(outline_width * S),
               stroke_fill=outline)
    d.text((xy[0] * S, xy[1] * S), s, font=f, fill=fill, anchor=anchor)


# --- The button atlas ----------------------------------------------------------------

def face_symbol(img, kind, cx, cy, colour):
    d = ImageDraw.Draw(img)
    w = 3.0 * S
    cx, cy = cx * S, cy * S
    c = colour + (255,)
    if kind == "square":
        h = 8.2 * S
        d.rectangle((cx - h, cy - h, cx + h, cy + h), outline=c, width=int(w))
    elif kind == "triangle":
        h = 9.5 * S
        pts = [(cx, cy - h), (cx + h * 0.95, cy + h * 0.62), (cx - h * 0.95, cy + h * 0.62)]
        d.line(pts + [pts[0], pts[1]], fill=c, width=int(w), joint="curve")
    elif kind == "circle":
        r = 8.2 * S
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=c, width=int(w))
    else:  # cross
        h = 7.4 * S
        d.line((cx - h, cy - h, cx + h, cy + h), fill=c, width=int(w))
        d.line((cx - h, cy + h, cx + h, cy - h), fill=c, width=int(w))


def atlas_cells():
    """{(column, row): 64x64 RGBA picture} for the cells this project redraws.
    The sticks and the d-pad (rows 1-3, columns 0-5) stay the game's."""
    cells = {}
    dark, light, rim = (14, 14, 16), (88, 88, 94), (150, 150, 156)
    # Row 0, columns 0-3: Xbox X, Y, B, A -> Square, Triangle, Circle, Cross.
    for col, (kind, colour) in enumerate((("square", PINK), ("triangle", GREEN), ("circle", RED), ("cross", BLUE))):
        img = canvas(64, 64)
        sphere(img, 33.5, 34, 21, dark, light, rim)
        face_symbol(img, kind, 33.5, 34.5, colour)
        cells[(col, 0)] = down(img)
    silver_light, silver_dark, outline = (238, 238, 240), (120, 120, 126), (70, 70, 74, 255)
    ink = (40, 40, 44, 255)
    # Columns 4-5: LT, RT -> L2, R2 (an upright plate, the letter above the number).
    for col, side in ((4, "L"), (5, "R")):
        img = canvas(64, 64)
        pill(img, (21, 10, 46, 57), False, silver_light, silver_dark, outline)
        text(img, (33.5, 23), side, 17, ink, ROMAN)
        text(img, (33.5, 43), "2", 17, ink, ROMAN)
        cells[(col, 0)] = down(img)
    # Columns 6-7: LB, RB -> L1, R1.
    for col, side in ((6, "L1"), (7, "R1")):
        img = canvas(64, 64)
        pill(img, (9, 20, 57, 47), True, silver_light, silver_dark, outline)
        text(img, (33, 33.5), side, 17, ink, ROMAN)
        cells[(col, 0)] = down(img)
    label, label_edge = (232, 232, 236, 255), (30, 30, 34, 255)
    # Row 3, column 6: BACK -> the touch pad.
    img = canvas(64, 64)
    text(img, (33.5, 16), "TOUCH", 13, label, ROMAN, outline=label_edge, outline_width=1.2)
    pill(img, (12, 26, 55, 50), True, (205, 205, 210), (95, 95, 100), outline)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((16 * S, 29 * S, 51 * S, 46 * S), 4 * S, outline=(255, 255, 255, 120), width=int(0.8 * S))
    cells[(6, 3)] = down(img)
    # Column 7: START -> OPTIONS.
    img = canvas(64, 64)
    text(img, (32, 16), "OPTIONS", 11.5, label, ROMAN, outline=label_edge, outline_width=1.2)
    sphere_box = (17, 28, 47, 47)
    pill(img, sphere_box, True, (110, 110, 116), (22, 22, 26), (150, 150, 156, 255))
    for k in range(3):
        y = (33.2 + k * 4.2) * S
        ImageDraw.Draw(img).line((26 * S, y, 38 * S, y), fill=(240, 240, 244, 255), width=int(1.6 * S))
    cells[(7, 3)] = down(img)
    return cells


# --- Text labels ---------------------------------------------------------------------

def typewriter_label(w, h, s, text_box, align="center"):
    """White typewriter letters with the HUD's dark red glow, fitted into
    text_box (x0, y0, x1, y1) of a w x h picture."""
    x0, y0, x1, y1 = text_box
    size = (y1 - y0) * 1.35
    f = ImageFont.truetype(TYPEWRITER, int(size * S))
    while size > 6:
        f = ImageFont.truetype(TYPEWRITER, int(size * S))
        l, t, r, b = f.getbbox(s)
        if r - l <= (x1 - x0) * S and b - t <= (y1 - y0) * S * 1.15:
            break
        size -= 0.5
    l, t, r, b = f.getbbox(s)
    tx = {"center": (x0 + x1) / 2 * S - (l + r) / 2, "right": x1 * S - r}[align]
    ty = (y0 + y1) / 2 * S - (t + b) / 2
    # Heavy, worn letters: a stroke for weight, then speckled wear (a fixed
    # seed, so the output is the same every run).
    letters = Image.new("L", (w * S, h * S), 0)
    ImageDraw.Draw(letters).text((tx, ty), s, font=f, fill=255, stroke_width=int(size * S * 0.018),
                                 stroke_fill=255)
    rng = np.random.default_rng(len(s) * 7919 + w)
    wear = rng.random((h * S // 3 + 1, w * S // 3 + 1))
    wear = np.kron(wear, np.ones((3, 3)))[:h * S, :w * S]
    lm = np.asarray(letters, np.float32) / 255.0
    lm = lm * np.where(wear < 0.05, 0.4, 1.0)
    letters = Image.fromarray((lm * 255).astype(np.uint8), "L")
    halo = letters.filter(ImageFilter.MaxFilter(int(3.4 * S) | 1)).filter(ImageFilter.GaussianBlur(1.4 * S))
    glow = letters.filter(ImageFilter.MaxFilter(int(2.0 * S) | 1)).filter(ImageFilter.GaussianBlur(0.9 * S))
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))

    def layer(rgb, alpha, offset=(0, 0)):
        img.alpha_composite(Image.merge("RGBA", (*(Image.new("L", img.size, v) for v in rgb), alpha)), offset)

    layer((16, 8, 12), halo.point(lambda v: min(255, v * 1.3)), (int(1.0 * S), int(1.5 * S)))
    layer((136, 0, 0), glow.point(lambda v: min(255, v * 1.6)))
    shade = np.clip(250 - np.linspace(0, 70, h * S)[:, None] - (wear * 36 - 18), 170, 255).astype(np.uint8)
    face = Image.fromarray(np.broadcast_to(shade, (h * S, w * S)).copy(), "L")
    layer((0, 0, 0), Image.new("L", img.size, 0))
    img.alpha_composite(Image.merge("RGBA", (face, face, face, letters)))
    return down(img)


def opaque_box(pic, rows=None):
    """Bounding box of the picture's visible texels, optionally within rows."""
    alpha = pic["table"][pic["index"], 3]
    if rows:
        alpha = alpha.copy()
        alpha[:rows[0]] = 0
        alpha[rows[1]:] = 0
    ys, xs = np.nonzero(alpha > 24)
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


# --- Output ---------------------------------------------------------------------------

def write_rect(path, rects):
    with open(path, "wb") as f:
        f.write(b"NG2R" + struct.pack("<II", 1, len(rects)))
        for x, y, idx in rects:
            f.write(struct.pack("<HHHH", x, y, idx.shape[1], idx.shape[0]))
            f.write(idx.astype(np.uint8).tobytes())


def preview(pic, rects, path):
    before = pic["index"]
    after = before.copy()
    for x, y, idx in rects:
        after[y:y + idx.shape[0], x:x + idx.shape[1]] = idx
    out = []
    for ind in (before, after):
        rgba = pic["table"][ind].astype(np.uint8)
        img = Image.new("RGBA", (ind.shape[1], ind.shape[0]), (30, 30, 60, 255))
        img.alpha_composite(Image.fromarray(rgba, "RGBA"))
        out.append(img)
    both = Image.new("RGBA", (out[0].width, out[0].height * 2 + 8), (0, 0, 0, 255))
    both.paste(out[0], (0, 0))
    both.paste(out[1], (0, out[0].height + 8))
    both.resize((both.width * 2, both.height * 2), Image.NEAREST).save(path)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    prev = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    if prev in args:
        args.remove(prev)
    dump = Dump(args[0])
    out = args[1] if len(args) > 1 else os.path.join(HERE, "..", "package", "hub", "button_prompts", "playstation")
    os.makedirs(out, exist_ok=True)
    if prev:
        os.makedirs(prev, exist_ok=True)

    jobs = []
    pic = dump.picture(BUTTON_ATLAS)
    if pic:
        tr = transparent_index(pic)
        rects = [(c * 64, r * 64, quantize(img, pic["table"], tr)) for (c, r), img in sorted(atlas_cells().items())]
        jobs.append((pic, rects))
    for prefix, words, align in ((MARKETPLACE, "Downloadable Content", "center"),
                                 (PRESS_START, "PRESS OPTIONS BUTTON", "center")):
        pic = dump.picture(prefix)
        if pic:
            h, w = pic["index"].shape
            img = typewriter_label(w, h, words, opaque_box(pic), align)
            jobs.append((pic, [(0, 0, quantize(img, pic["table"], transparent_index(pic)))]))
    pic = dump.picture(ACTION_STRIP)
    if pic:
        # Row 13 says "Gamer Card", an Xbox feature: the same row, "Profile".
        # The row keeps the game's backing box (its most used visible entry).
        y0, y1 = 13 * 40, 14 * 40
        box = opaque_box(pic, (y0, y1))
        band = pic["index"][y0:y1]
        vis = band[pic["table"][band, 3] > 24]
        values, counts = np.unique(vis, return_counts=True)
        backing = tuple(int(v) for v in pic["table"][values[counts.argmax()]])
        h, w = pic["index"].shape
        img = Image.new("RGBA", (w, 40), (0, 0, 0, 0))
        label_w = 86
        ImageDraw.Draw(img).rectangle((w - label_w - 6, box[1] - y0, w - 1, box[3] - y0 - 1), fill=backing)
        img.alpha_composite(typewriter_label(w, 40, "Profile", (w - label_w, box[1] - y0 + 3, w - 4, box[3] - y0 - 3),
                                             "right"))
        jobs.append((pic, [(0, y0, quantize(img, pic["table"], transparent_index(pic)))]))

    for pic, rects in jobs:
        e = pic["entry"]
        name = e["hash"] + ".rect"
        write_rect(os.path.join(out, name), rects)
        print("%s  %dx%d  %d rectangle(s)" % (name, e["w"], e["h"], len(rects)))
        if prev:
            preview(pic, rects, os.path.join(prev, e["hash"] + "_preview.png"))


if __name__ == "__main__":
    main()
