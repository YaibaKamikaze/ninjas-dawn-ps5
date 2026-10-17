#!/usr/bin/env python3
"""[ps5 port] Refuse a package that holds any Ninja Gaiden II game file.

    python3 ps5/tools/check_no_game_files.py <app folder>

The app ships no game material: no executable image, archives, videos, audio
banks, saves or DLC. This looks for the disc's file names and for the file
signatures of Xbox 360 executables and content packages anywhere in the app
folder, and exits non-zero (failing the package) if it finds one. The README
in each folder the player fills is the only thing allowed there.
"""
import pathlib
import sys

app = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")

# Names from the disc (the Game Data folder's contents).
DISC_NAMES = {"default.xex", "load_img.bin", "pack_img.bin"}
DISC_SUFFIXES = {".ng2", ".xex", ".xexp", ".wmv", ".xwb", ".xsb", ".xgs", ".ssc", ".sso"}
DISC_PREFIXES = ("ng2_", "s_chap_", "aurora", "tutorial_", "ghost0", "ninjavi")
# Magic numbers at offset 0: XEX2, and STFS packages (CON/LIVE/PIRS).
MAGICS = (b"XEX2", b"CON ", b"LIVE", b"PIRS")
# The folders the player fills hold only our README.
PLAYER_FOLDERS = ("Game Data", "DLC", "Saves Import", "Texture Packs")

problems = []
for path in app.rglob("*"):
    if not path.is_file():
        continue
    rel = path.relative_to(app)
    name = path.name.lower()
    if rel.parts[0] in PLAYER_FOLDERS and name != "readme.txt":
        problems.append(f"{rel}: the player's folders must be empty in a package")
        continue
    if name in DISC_NAMES or path.suffix.lower() in DISC_SUFFIXES or name.startswith(DISC_PREFIXES):
        problems.append(f"{rel}: looks like a file from the game disc")
        continue
    with path.open("rb") as f:
        head = f.read(4)
    if head in MAGICS:
        problems.append(f"{rel}: an Xbox 360 executable or content package ({head!r})")

if problems:
    print("Game files in the package - refusing:", file=sys.stderr)
    for p in problems:
        print(f"  {p}", file=sys.stderr)
    sys.exit(1)
print(f"{app}: no game files")
