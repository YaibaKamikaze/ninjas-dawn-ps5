# Ninja's Dawn for PS5 - player guide

Everything about installing and playing the PS5 alpha. For building it, see
[ps5/BUILD.md](../ps5/BUILD.md).

## 1. What you need

- **Your own copy of Ninja Gaiden II for Xbox 360.** This project gives you no
  game files. You need the disc's contents extracted to a folder: `default.xex`
  at its root, with the `.ng2` archives (`char.ng2`, `cmn.ng2`, `other.ng2`,
  `rtm.ng2`, `s_chap_01.ng2` ... `s_chap_14.ng2` and the rest), the `.wmv`
  videos and the other files beside it - about 6.7 GB.
  - From a disc image (`.iso`) you can extract it on a PC with
    `python tools/extract_disc.py <your.iso> <out folder>` from this repository.
- **A PS5 that runs homebrew apps**: a jailbroken console with a homebrew
  launcher that starts apps from `/data/homebrew/`. The alpha was tested on
  firmware 13.60.
- **About 7 GB free** on the internal storage, and a way to copy files to the
  console (an FTP server running on the PS5 and an FTP client on your PC is the
  usual way).

## 2. Installing

1. Download the latest **`ninjas-dawn-ps5-v…-alpha.zip`** from the Releases page and
   unzip it. Inside is a folder named **`PPSA94202`** (the app's title id):

   ```
   PPSA94202/
     eboot.bin         the game
     sce_sys/          the app's icon and launch picture
     sce_module/       a system library the app needs
     hub/              the hub's sounds, music and the PlayStation button pictures
     Game Data/        <- your game files go here
     DLC/              <- optional: your Xbox 360 DLC packages
     Saves Import/     <- optional: Xbox 360 saves to import
     Texture Packs/    texture dumps are written here
   ```

2. Copy **your extracted game** into `PPSA94202/Game Data/`, so that
   `PPSA94202/Game Data/default.xex` exists, with the archives and videos next
   to it. Copy the files themselves, not a folder containing them.
3. Copy the whole **`PPSA94202`** folder to **`/data/homebrew/`** on the PS5,
   giving `/data/homebrew/PPSA94202/`.
4. Start it from your homebrew launcher.

The first start takes a little longer: the app sets up its folders, and the
game builds its shaders the first time each effect appears (they are kept for
later runs in `UserData/cache`).

### Updating to a newer release

Copy the new `eboot.bin`, `sce_sys/`, `sce_module/` and `hub/` over the old
ones. Leave `Game Data/`, `DLC/`, `Saves Import/` and **`UserData/`** (your
settings, saves and shader cache) where they are.

## 3. The hub

The hub opens first. It checks `Game Data/` and tells you exactly what is
missing if something is.

| Button | In the hub |
|---|---|
| D-pad / left stick | move |
| Cross | choose |
| Circle | back |
| L1 / R1 | switch settings tab |
| Left / Right | change the highlighted setting |

- **START GAME** - starts Ninja Gaiden II with the current settings.
- **SETTINGS** - see below. Settings marked *restart* apply the next time the
  game starts; the others apply at once.
- **DOWNLOADABLE CONTENT** - lists the packages in `DLC/`.
- **SAVES** - imports Xbox 360 saves from `Saves Import/` and backs up your
  current saves.
- **ABOUT**, **QUIT**.

### Recommended settings

The defaults are the tested ones: **Resolution 1440p**, **Render target path
FSI**, **Frame pacing: Display refresh**, **Input latency: Standard**. 4K works
but drops below 60 fps in heavy scenes; FBO is faster but hitches more.

### The settings

| Tab | Setting | What it does |
|---|---|---|
| Display | Resolution | 720p, 1440p (default) or 4K - exact multiples of the game's 720p. |
| Display | Frame rate | 60 (default) or 30. The game paces itself from this; above 60 it would run fast, not smooth. |
| Display | Base render size / Render scale | 1280x720 (the community 720p patch, default) or the original 1120x584; the scale is advanced. |
| Display | Dither output | Faint noise to hide banding. |
| Graphics | Anti-aliasing, Output filter, Anisotropic filtering | FXAA, CAS sharpening / FSR, and texture filtering. |
| Graphics | Workarounds (fuzzy alpha test, accurate depth) | Only for specific glitches. |
| Game | Button prompts | PlayStation (default) or the original buttons. |
| Game | Flying Swallow flash | *Coming soon.* |
| Game | Skip intro videos | Straight to the title screen. |
| Game | Cutscene audio fix | On (default): keeps sound level with the picture - when the game falls behind for a moment, the sound that piled up is skipped instead of playing late. |
| Game | Workaround: chapter 12 | The community fix for the chapter 12 crash - only if you are stuck there. |
| Game | Hub music | Music behind the hub. |
| Textures | Dump textures | Writes the game's textures to `Texture Packs/PS5 Dump/` for the PC pack tools. |
| Performance | Shader compilation | Smooth (default) builds shaders in the background. |
| Performance | Texture cache size | Memory for the game's textures. |
| Performance | Performance readouts | Frame rate and CPU in a corner. |
| Performance | Frame pacing | Display refresh (default) or the runtime's timer. |
| Performance | Input latency | Standard (default) or Low: Low shows each frame about 16 ms sooner, but a frame that runs late is then shown a refresh later, so timing can feel uneven in heavy scenes. |
| Performance | Memory refresh | How often the GPU's copy of the game's memory is refreshed. Keep **Every frame**: less often is faster but makes character animation twitch. |
| Performance | Skip unchanged memory | On (default): memory that has not changed since the last refresh is not sent again. |
| Performance | Fast memory checks | On (default): a quicker check before taking the memory lock. |
| Performance | Async submission | On (default): most of each frame's GPU work is sent from a second thread. |
| Performance | Descriptor reuse (experimental) | Off by default; reuses texture bindings between draws. |
| Performance | Render target path | FSI (default, exact and stable) or FBO (experimental, faster, hitches more). |
| Developer | Dump HUD pictures, GPU timing details, Profile GPU thread / Profile thread, Frame report | Diagnostics for bug reports (the profile costs a little frame rate while on). |

## 4. In the game

The DualSense works as the Xbox 360 controller the game expects:

| DualSense | Game |
|---|---|
| Cross / Circle / Square / Triangle | A / B / X / Y |
| L1 / R1 | LB / RB |
| L2 / R2 | LT / RT |
| OPTIONS | START |
| Touchpad click | BACK |
| L3 / R3 | stick clicks |

**The in-game menu:** hold the **touchpad** for about half a second. It shows
the settings that apply live, and a GAME pane (R1) with **Resume**, **Return to
hub**, **Quit to home screen** and **Capture frame report**. Circle closes it.
While it is open the game gets no input.

To skip a cutscene, use the game's own controls (cutscenes you have not seen
before cannot be skipped, as in the original).

## 5. If something goes wrong

- **"Ninja Gaiden II not found" in the hub** - `Game Data/default.xex` is
  missing or the files are inside a subfolder. Move them up into `Game Data/`.
- **A crash** - the app writes logs to `PPSA94202/logs/`. Please open an issue
  with that folder attached (zip it), what you were doing, and your settings.
  The last line of the newest `ng2_NNN.log` usually names the cause, e.g.
  `Call to invalid or unregistered function at guest address 0x...`.
- **Stutter** - make sure you are on 1440p and FSI. The first time an effect
  appears it may hitch once while its shader is built; it is kept for next time.
- **Settings did not stick** - settings marked *restart* apply at the next
  START GAME.

Your saves and settings live in `PPSA94202/UserData/`. Back that folder up
before trying something risky; the SAVES page can make a backup too.
