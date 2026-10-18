# Release notes

## v0.1.3-alpha - freeze fix

- **Fixed: the game froze going into the cutscene between the two halves of
  chapter 1** (and could freeze the same way elsewhere). Some of the game's
  worker threads put themselves to sleep after each job and are woken once
  when the next one starts. When the wake-up arrived just before the worker
  had gone to sleep, the PS5 build dropped it, and the worker slept forever -
  the game's main loop then waited for it endlessly. An early wake-up is now
  kept and cancels the worker's next sleep, as the PC build has always
  effectively done. Tested through the rest of chapter 1 into chapter 2.

Updating from v0.1.2.1: copy the new `eboot.bin` and `sce_sys/` over the old
ones; keep `Game Data/` and `UserData/`.

## v0.1.2.1-alpha - performance

The biggest performance update so far. In the heaviest fights tested, the worst
moments went from the mid-40s to the mid-50s fps, and most of the time the game
holds 60. (v0.1.2 was never published; this is it, with the lock change below.)

- **Less waiting on the runtime's global lock.** In heavy scenes the GPU
  thread spent up to a fifth of its time asleep waiting for one lock that the
  game's threads take very often, mostly for a few microseconds at a time. On
  PS5 a thread that finds that lock taken went straight to sleep in the
  system; it now retries for about 20 microseconds first. The GPU thread also
  checks larger blocks of memory for changes without holding the lock.

- **Faster game code.** The recompiler used to make the game re-read its own
  local variables from memory every time it touched them. It now lets the
  compiler keep them in the processor's registers: about 1.2 million places in
  the game's code, and the program came out 2.7% smaller.
- **Asynchronous submission, now on by default.** The GPU thread hands most of
  each frame's work to a second thread instead of sending it itself.
- **Less memory-protection work.** The game's memory is watched for changes in
  pieces that were four times larger on PS5 than intended (the PS5 uses 16 KB
  memory pages, the PC 4 KB); now they are the intended size. About a third
  fewer protection calls in heavy scenes.
- **Skip unchanged memory.** The game needs its graphics memory refreshed every
  frame (character animation is written where nothing else notices it); memory
  that has not changed is now recognised and not re-sent. Animation is
  unaffected.
- **Smoother streaming.** The game's streaming reads were done 4 KB at a time,
  each one taking a lock the GPU thread needs on almost every draw; adjacent
  pieces are now read in one go.
- **Fewer things hashed on the GPU thread:** only the button-prompt textures
  are checked for replacements unless you use the texture dump or replacements.
- **Input latency stays Standard by default.** Low (two screen images instead of
  three) shows each frame about 16 ms sooner but made timing uneven in heavy
  scenes; it is still in Performance > Input latency.
- **For bug reports:** the logs now name slow file reads and writes, the stall
  report shows what every thread was waiting on when the game stops, and
  Developer > Profile thread can sample the GPU thread or the game's main
  thread (`ps5/tools/symbolize_profile.py` reads the result).

New hub settings (Performance tab): Memory refresh, Skip unchanged memory, Fast
memory checks, Async submission, Descriptor reuse (experimental). The defaults
are the tested ones; your other settings are kept.

Updating from v0.1.1: copy the new `eboot.bin` and `sce_sys/` over the old
ones; keep `Game Data/` and `UserData/`.

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
