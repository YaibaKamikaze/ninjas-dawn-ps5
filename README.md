# Ninja's Dawn for PS5 (alpha)

A homebrew port of **Ninja's Dawn**, the static recompilation of **Ninja Gaiden II**
(Xbox 360), to the **PlayStation 5**. The game's PowerPC code is translated ahead
of time into native code and runs as a PS5 homebrew app, rendered through Vulkan
(Mesa's RADV driver) on the console's GPU.

> **Alpha.** It plays the game at **1440p and 60 fps**, but it is early: expect
> occasional crashes and rough edges. See [Known issues](#known-issues).

This is a fork of [Ninja's Dawn](https://github.com/FermataRest/Ninja-s-Dawn-A-Ninja-Gaiden-2-Recompilation-Project)
by TheSaltTrader, the PC recompilation it is built on. Their README is kept here
as [README_PC.md](README_PC.md).

---

## What you need

**This project ships no game data of any kind** - no executable image from the
disc, no textures, videos, audio, saves or DLC. You bring all of it from your
own copy.

| | What |
|---|---|
| **1** | **Your own copy of Ninja Gaiden II for Xbox 360**, extracted to a folder: `default.xex` and the `.ng2` archives and videos next to it (about 6.7 GB). |
| **2** | **A PS5 that runs homebrew apps** (a jailbroken console with a homebrew launcher). Tested on firmware **13.60**. |
| **3** | **About 7 GB free** on the console's internal storage. |
| **4** | A way to copy files to the console (FTP is the usual one). |

Optional: your own Xbox 360 DLC packages and Xbox 360 saves - the hub imports both.

Where to get the game: nowhere in this project, and please do not ask. Dump your
own disc. If you do not own Ninja Gaiden II, this has nothing to offer you.

---

## Installing

The short version (the full one, with every folder and setting explained, is in
**[docs/PS5_GUIDE.md](docs/PS5_GUIDE.md)**):

1. Download `ninjas-dawn-ps5-v0.1.0-alpha.zip` from
   [Releases](../../releases) and unzip it. You get a folder named **`PPSA94202`**.
2. Copy your extracted game - `default.xex`, every `.ng2` archive, the `.wmv`
   videos and the rest of the disc's files - into **`PPSA94202/Game Data/`**.
3. Copy the whole `PPSA94202` folder to **`/data/homebrew/`** on the console.
4. Start it from your homebrew launcher. The **hub** opens: it checks your game
   files, then **START GAME**.

---

## Features

- **Ninja Gaiden II running natively** on the PS5's CPU and GPU - no emulator.
- **1440p at 60 fps** by default (720p and 4K also offered), with frame pacing
  locked to the TV's refresh.
- **PlayStation button prompts** in the game's HUD and menus (Cross, Circle,
  Square, Triangle, L1/R1, L2/R2, TOUCH, OPTIONS), drawn for this project and
  swapped in at run time. Switchable back to the originals.
- **The hub**: a launcher with its own settings (display, performance, game,
  textures), DLC and save import, and diagnostics.
- **In-game menu**: hold the **touchpad** for half a second to change live
  settings, return to the hub or quit.
- DLC weapons and costumes from your own Xbox 360 packages; Xbox 360 save import.
- Texture dumping in the PC project's format (loading packs is not in yet).

Controls: the DualSense maps to the Xbox 360 pad the game expects - Cross = A,
Circle = B, Square = X, Triangle = Y, L1/R1 = LB/RB, L2/R2 = LT/RT,
OPTIONS = START, touchpad click = BACK, L3/R3 = stick clicks.

---

## Known issues

- **Occasional crashes.** A few parts of the game call code the recompiler did
  not find; each such spot crashes until it is registered (the game's options
  menu was one, and is fixed). If you hit one, please open an issue with the
  files from `PPSA94202/logs/` - the last line names the address.
- **The Flying Swallow's white flash** cannot be turned off yet (the option is
  shown as "Coming soon").
- **The home-screen background** shows when the app launches, but not while it
  is only highlighted on the home screen.
- **The game's controller settings screen** still shows an Xbox controller.
- **4K** runs below 60 fps in heavy scenes; **1440p** is the recommended setting.
  The **FBO** render path is faster but hitches more; **FSI** (default) is the
  stable one.
- Loading texture packs is not implemented on PS5 (dumping is).

---

## Building from source

The port builds on Linux (Ubuntu 24.04 tested) with a PS5 homebrew toolchain.
**[ps5/BUILD.md](ps5/BUILD.md)** has every step; in outline:

1. `ps5/tools/setup-linux.sh` installs the packages and builds the toolchain
   (ps5-payload-sdk, Mesa RADV for PS5, the signer), and checks out the ReXGlue
   SDK at the pinned commit with this project's patch
   (`ps5/patches/rexglue-ng2.patch`).
2. Put **your own** `default.xex` in `assets/`, build the codegen tool, and run
   codegen plus the three post-codegen scripts. This writes `generated/` - C++
   derived from your copy of the game. **Never commit or share it.**
3. Cross-build with CMake + Ninja and package with `ps5/tools/ps5_package.sh`.
   The packager refuses to produce a package that contains any game file
   (`ps5/tools/check_no_game_files.py`).

The PC build (Windows) is unchanged from Ninja's Dawn; see [README_PC.md](README_PC.md).

---

## Credits

See **[CREDITS.md](CREDITS.md)**. In short: Ninja's Dawn by TheSaltTrader; the
ReXGlue SDK (Tom Clay) and Xenia, which it derives from; the PS5 runtime of
elripalda's ports; PromethenDev's WWE 2K14 recompilation patches; the PS5
payload SDK, ps5-plume-triangle and PS5 Mesa; and the PS5 port by YaibaKamikaze.

## Licence

The project's own source is under the **MIT licence** ([LICENSE](LICENSE)), as
Ninja's Dawn is. The PS5 **release binary** also contains GPLv3-or-later
components (the PS5 payload SDK's runtime and ps5-plume-triangle's `libc.prx`),
so the release is distributed under the terms of the **GNU GPL v3**; this
repository, plus the pinned upstream versions listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), is its corresponding source.

## Legal

The source in this repository contains **no** Ninja Gaiden II code, assets or
data. The executable in each release contains the game's code in translated
form, which is what a static recompilation is; it contains no game assets or
data and does nothing without your own copy of the game. Ninja Gaiden II is
© Koei Tecmo; this project is not affiliated with Koei Tecmo, Team Ninja,
Microsoft or Sony. PlayStation, PS5 and DualSense are trademarks of Sony
Interactive Entertainment.
