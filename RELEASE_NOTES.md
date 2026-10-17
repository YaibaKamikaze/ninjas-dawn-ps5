# Release notes

## v0.1.1-alpha - hotfix

- **Fixed: opening a chest (or anything that shows a tutorial video - a new
  weapon, ninpo...) stopped the game** with a "disc read error" when *Skip
  intro videos* was on. The skip redirected every video except the
  chapter-loading ones, and the game treats a tutorial video that fails to open
  as a damaged disc. It now skips only the opening video (NinjaVI), the one it
  was meant for; every other video plays.
- The app's version now shows its patch number (00.001.001).
- Release builds no longer contain the build machine's folder names.

Updating from v0.1.0: copy the new `eboot.bin` and `sce_sys/` over the old ones
(nothing else changed; keep your `Game Data/` and `UserData/`).

## v0.1.0-alpha - first PS5 alpha (internal build 16.4)

The first public build of Ninja's Dawn for PS5: Ninja Gaiden II, statically
recompiled, running as a native PS5 homebrew app.

**You need your own copy of Ninja Gaiden II (Xbox 360)** - no game files are
included. Installation: [docs/PS5_GUIDE.md](docs/PS5_GUIDE.md).

### What works

- The whole game boots and plays: menus, cutscenes and videos, gameplay.
- **1440p at 60 fps** by default with the exact (FSI) render path, frame pacing
  locked to the TV's refresh; 720p and 4K also offered.
- **PlayStation button prompts** in the HUD and menus (switchable back to the
  originals), and PlayStation wording where the game named Xbox features
  ("Downloadable Content", "PRESS OPTIONS BUTTON", "Profile").
- The game's own **options menu** (language, aspect ratio and the rest).
- **The hub**: settings, DLC and save import, save backup, diagnostics; and an
  **in-game menu** on a half-second touchpad hold.
- Shader cache kept between runs, so effects hitch only the first time.
- DLC weapons and costumes from your own Xbox 360 packages; Xbox 360 save import.

### Known issues

- **Occasional crashes** where the game calls code the recompiler did not find.
  Each one names its address in `PPSA94202/logs/` - please report it (see below).
- The **Flying Swallow flash** option is not available yet ("Coming soon").
- The **home-screen background** appears when the app launches, not while it is
  highlighted on the home screen.
- The game's **controller settings screen** still shows an Xbox controller.
- **4K** drops below 60 fps in heavy scenes; the **FBO** render path (marked
  experimental) is faster but hitches more than FSI.
- Brief hitches can happen while the game streams data; the game's own thread
  causes them, not the renderer, and they are being looked into.
- Loading texture packs is not implemented on PS5 (dumping is).

### Reporting a problem

Open an issue with: what you were doing, your settings (or "defaults"), and the
`PPSA94202/logs/` folder zipped. The newest `ng2_NNN.log` is the one that
matters most.

### Under the hood (for the curious)

- Built on Ninja's Dawn (PC) and the ReXGlue SDK's PS5 runtime, with this
  port's patch: the SPIR-V translator read a texture's exponent bias from the
  wrong fetch-constant word (every LOD-biased texture came back 2^-24 dark -
  the original "black screen"), pipeline storage that never grew, a
  display-locked guest vblank, a texture-load hook for the button prompts,
  frame reports, and per-frame GPU-thread timing.
- Three ReXGlue patches from the WWE 2K14 recompilation (PromethenDev; BSD-3).
- Seven virtual-call forwarding thunks and the options menu's "option changed"
  handler registered as functions (the recompiler had folded them into their
  neighbours).
- A warning logged ~50 times a second inside the fault handler, with the global
  lock held, now logs three times and then counts - it had been stalling both
  the game and the renderer.
