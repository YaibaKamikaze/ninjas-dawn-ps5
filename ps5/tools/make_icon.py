"""[ps5 port] Paint the app's home-screen icon, ps5/package/sce_sys/icon0.png.

Our own artwork, drawn here so it can be remade and nobody has to wonder where
it came from: the hub's scene in miniature (misty grey sky, mountains, katanas
standing in a field) with the title in Cinzel (OFL, ps5/assets/fonts) and a red
brush-stroke "II". No game art.

    ~/ng2-ps5-deps/pyenv/bin/python ps5/tools/make_icon.py

Needs Pillow (setup-linux.sh puts it in the deps venv).
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "assets", "fonts")
OUT = os.path.join(HERE, "..", "package", "sce_sys", "icon0.png")
S = 512


def lerp(a, b, t):
    return int(a + (b - a) * t)


def main():
    rnd = random.Random(2)  # the same picture every time
    img = Image.new("RGB", (S, S))
    d = ImageDraw.Draw(img)

    # Sky: grey, paler towards the horizon.
    for y in range(S):
        t = min(1.0, y / (S * 0.62))
        g = lerp(70, 170, t)
        d.line([(0, y), (S, y)], fill=(g, g, g + 3))

    # Three mountain ranges, darker as they come nearer.
    for layer, (base, height, grey) in enumerate([(300, 120, 140), (330, 105, 112), (360, 90, 86)]):
        pts = [(0, S)]
        x = 0
        while x <= S:
            ridge = sum(math.sin(x / w + layer * 3 + k) * a for k, (w, a) in enumerate([(60, 0.5), (23, 0.3), (9, 0.2)]))
            pts.append((x, base - (0.5 + 0.5 * ridge) * height))
            x += 4
        pts.append((S, S))
        d.polygon(pts, fill=(grey, grey, grey + 3))

    # Fog over the valley, then the dark field.
    fog = Image.new("L", (S, S), 0)
    fd = ImageDraw.Draw(fog)
    for y in range(300, 420):
        fd.line([(0, y), (S, y)], fill=int(170 * math.sin((y - 300) / 120 * math.pi)))
    img.paste((176, 176, 180), mask=fog.filter(ImageFilter.GaussianBlur(6)))
    for y in range(400, S):
        t = (y - 400) / (S - 400)
        g = lerp(66, 24, t)
        d.line([(0, y), (S, y)], fill=(g, g, g + 2))

    # Katanas standing in the field: blade, guard, handle, as silhouettes.
    for _ in range(9):
        depth = rnd.random()
        x = rnd.uniform(-10, S + 10)
        ground = 410 + depth * 80
        length = 70 + depth * depth * 150
        tilt = rnd.uniform(-0.18, 0.18)
        dx, dy = math.sin(tilt), -math.cos(tilt)
        blade, handle = length * 0.72, length * 0.24
        w = max(2.0, length * 0.02)

        def p(a, c):
            return (x + dx * a - dy * c, ground + dy * a + dx * c)

        shade = (20, 20, 24)
        d.polygon([p(0, 0), p(blade, -w), p(blade, w)], fill=shade)
        gx, gy = p(blade, 0)
        d.ellipse([gx - w * 3, gy - w * 1.2, gx + w * 3, gy + w * 1.2], fill=shade)
        d.polygon([p(blade + w, -w * 0.9), p(blade + handle, -w * 0.8), p(blade + handle, w * 0.8),
                   p(blade + w, w * 0.9)], fill=(12, 10, 12))

    # A vignette, so the title reads.
    vig = Image.new("L", (S, S), 0)
    vd = ImageDraw.Draw(vig)
    for i in range(60):
        vd.rectangle([i, i, S - i, S - i], outline=int(150 * (1 - i / 60)))
    img.paste((0, 0, 0), mask=vig)

    # The title: chrome-ish Cinzel with a black bevel.
    big = ImageFont.truetype(os.path.join(FONTS, "cinzel-700.ttf"), 78)
    small = ImageFont.truetype(os.path.join(FONTS, "cinzel-700.ttf"), 40)
    text = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    for (word, font, y) in [("NINJA", big, 86), ("GAIDEN", big, 168)]:
        w = td.textlength(word, font=font)
        x = (S - w) / 2 - 40
        for k in range(16):
            a = k / 16 * 2 * math.pi
            td.text((x + math.cos(a) * 5, y + math.sin(a) * 5 + 2), word, font=font, fill=(6, 6, 8, 255))
        # chrome: a vertical ramp, masked by the letters
        mask = Image.new("L", (S, S), 0)
        ImageDraw.Draw(mask).text((x, y), word, font=font, fill=255)
        ramp = Image.new("RGBA", (S, S))
        rd = ImageDraw.Draw(ramp)
        for yy in range(S):
            t = (yy - y - 12) / 70.0
            if t < 0.48:
                g = 255 - max(0.0, t) * 170
            elif t < 0.56:
                g = 60
            else:
                g = min(255, 110 + (t - 0.56) * 230)
            rd.line([(0, yy), (S, yy)], fill=(int(g), int(g), int(min(255, g + 6)), 255))
        text.paste(ramp, mask=mask)
    img.paste(text, mask=text)

    # The red brush "II": two tapered, ragged strokes leaning like a quick hand.
    brush = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(brush)
    for stroke in range(2):
        x0 = 392 + stroke * 40
        top, height = 70, 200
        left, right = [], []
        for i in range(21):
            t = i / 20
            lean = (1 - t) * 16
            width = 7 + 24 * math.sin(math.pi * t ** 0.8)
            j = (rnd.random() - 0.5) * 5
            left.append((x0 + lean - width / 2 + j, top + t * height))
            right.append((x0 + lean + width / 2 + j * 0.5, top + t * height))
        bd.polygon(left + right[::-1], fill=(140, 12, 14, 255))
        core_l = [(lx * 0.7 + rx * 0.3, ly) for (lx, ly), (rx, _) in zip(left, right)]
        core_r = [(lx * 0.3 + rx * 0.7, ly) for (lx, ly), (rx, _) in zip(left, right)]
        bd.polygon(core_l + core_r[::-1], fill=(214, 32, 28, 255))
        for _ in range(6):
            t = rnd.random()
            bx = right[-1][0] + (left[-1][0] - right[-1][0]) * t
            bd.line([(bx, top + height), (bx - 3, top + height + 10 + 22 * t)], fill=(170, 20, 18, 220), width=2)
    img.paste(brush, mask=brush)

    # The platform word under it, small.
    w = d.textlength("PS5", font=small)
    d.text(((S - w) / 2, 420), "PS5", font=small, fill=(236, 230, 214))

    img.save(OUT, optimize=True)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
