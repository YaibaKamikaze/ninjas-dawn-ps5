# Ninja's Dawn for PS5 - design notes

How the PS5 port is put together, and why. Source comments refer to these
sections by number (e.g. "PS5_PORT.md 5.4"). These are the design notes the
port was built from; where the code and this file disagree, the code is
current. Player documentation is in [PS5_GUIDE.md](PS5_GUIDE.md), the build
in [../ps5/BUILD.md](../ps5/BUILD.md).

## 3. Findings that shape the project

### 3.2 NG2 depends on SDK fixes that are not in `rexglue-ps5`

NG2's README and `docs/ISSUES_AND_FIXES.md` say the runtime and codegen were
built from a ReXGlue tree carrying fixes. That tree was never published. The
`rexglue-ps5` branch is stock v0.10.0 + svr2011 + PS5, so these must be
**re-implemented as `ng2-ps5/ps5/patches/rexglue-ng2.patch`**:

| Id | Where | Fix | Without it |
|---|---|---|---|
| R3/R4 | codegen `BuilderContext::v()` + config | New manifest option `shared_vector_registers = [64, 65, 69, 72, 96]`: those VMX128 registers stay in `ctx`, the rest of v64-v127 stay local. | Every pre-rendered video is garbled; sharing all of v64-127 instead breaks animation (stuck at a ledge, ch. 5). |
| R5 | codegen `build_db16cyc` | Emit a `PAUSE` (`_mm_pause()`), with a real yield every 1024 iterations. | Spin loops burn the SMT sibling. |
| D12 | runtime `CommandProcessor::InitializeRingBuffer` | Reset **both** read and write pointers; add a ring epoch so a re-init landing mid-pass cannot write the old read pointer back. | Attract demo freezes (32 KB of dead commands executed). |
| A1 | runtime audio driver (on PS5: `Ps5AudioDriver`) | Return the "submit one more frame" credit on an underrun too. | All audio dies for the rest of the run after the first underrun. |
| X4 | runtime waits | Watchdog scope guard on all five wait entry points and the shared primitive; 30 s / 300 s thresholds. | Diagnostics only, but needed to debug hangs on console. |
| D7 | runtime | `video_mode_explicit` handling. | Picture stretched when the output is not 16:9. |
| T* | GPU texture cache | Texture dump / pack replacement by key (made for the Fable II project, shared plugin). | No texture packs. Also the mechanism the PlayStation button swap can reuse. |

Each must be verified against the NG2 doc entry that describes it (ids above
are the entries in `docs/ISSUES_AND_FIXES.md`).

Two more fixes live **in the generated code** and are applied by NG2's own
scripts after every codegen (`local/diag/patch_missed_regs.py`,
`local/diag/patch_scanguard.py`; idempotent, marked `NG2FIX`). Never run
`local/diag/patch_fix_ge2.py`.

### 3.3 The PS5 platform layer already exists and was proven on 13.60

From `svr2011-ps5/port/docs/PS5_PORT_PLAN.md` (tested on fw 13.600.007):
guest memory as ReXGlue's 9-view 4.5 GB layout via `shm_open` mirrors into a
`MAP_FIXED` reserve (16 KB pages); **asm fiber switch** (`makecontext` does not
work on PS5); SIGSEGV/SIGBUS handler with the PS5 `ucontext` quirk (mcontext at
`uc + 64`); 16 MiB thread stacks from direct memory; `VK_KHR_display` surface
on RADV; DualSense through `scePad`; audio through `sceAudioOut`; FFmpeg (XMA)
cross-built; libc gaps stubbed; `0xE0000000` guest addresses offset by
`0x1000` in host memory (the generated header must be patched for PS5:
`ps5_patch_generated.py`). NG2 needs **fibers** (its tasks are fibers, B2),
`setjmp/longjmp` (B1) and guest page zero readable (B3): all covered by that
layer, but each must be confirmed on the console.

### 3.4 The build is Linux-only

Every PS5 toolchain piece (payload SDK, RADV/Mesa, packaging, signer) builds
and runs on **Linux (Ubuntu 24.04, clang 19, lld 19)**.

Any Ubuntu 24.04 machine works - a native install, WSL2, or a virtual machine
(the reference build machine is a VM with 8 GB RAM and 8 vCPUs). With 8 GB the
huge generated translation units can exhaust memory at full parallelism: build
the recompiled code with `ninja -j4` and raise it only if memory allows.
Codegen runs on the Linux host too (a libstdc++ build of `rexglue`; the libc++
build hangs, per svr2011's notes).

### 3.5 Rendering-path risk: EDRAM emulation

NG2's PC tuning says the host-render-target EDRAM path "is effectively broken
for this title" (3128 resolves) and it ships on D3D12 **ROV**. The Vulkan
equivalent is **FSI** (`render_target_path_vulkan = "fsi"`, needs
`VK_EXT_fragment_shader_interlock`). Upstream RADV implements it for GFX9-11
(RDNA2 included) since Mesa 23.3 and PS5_Mesa is Mesa 26.2, so it should be
exposed, **but this is the first thing to confirm on the console** (milestone
M2). If FSI is missing or slow, the fallback is `fbo` with NG2-specific fixes,
which is a research task of its own.

### 3.6 What does not port (and what replaces it)

| PC feature | PS5 |
|---|---|
| Window size, fullscreen, monitor, cursor hide, keyboard/mouse control | Removed: fixed fullscreen to the console's video output, DualSense only. |
| Disc-image (.iso) installer, folder pickers | Removed. Files go in `Game Data/` over FTP. The hub checks the folder (title id `544307D5` read from `default.xex`) and says exactly what is missing. |
| GitHub update check | Removed (no network use). |
| Hardware autodetect (DXGI) | Fixed PS5 defaults. |
| AI texture upscaler (Python, Real-ESRGAN) | Not on console. Packs are made on a PC with the PC recomp's tools and copied to `Texture Packs/`; the PS5 app dumps and loads them. |
| F10 in-game menu, F8 readouts, F9 pack toggle, F3/F4 | The in-game overlay (5.5), opened from the DualSense. F4's raw cvar list is kept behind a "Developer" page. |
| Ultrawide (3D FOV widen) | TVs are 16:9: the ultrawide switch is dropped; the **field-of-view slider** stays. |
| Tutorial videos (`tutorial_*.wmv`) | They show Xbox buttons baked into the video. Changing them means re-encoding game files, which this project does not do. Known limitation, documented in the README. |

---

## 4. Architecture

```
 /data/homebrew/PPSA94202/              (title id, see 7)
   eboot.bin                 one static PS5 program: NG2's generated C++ + NG2's
                             own src/ + ReXGlue runtime (+ NG2 fixes) + Xenos
                             Vulkan backend + RADV, signed FSELF
   sce_module/libc.prx
   sce_sys/                  param.json, icon0.png, pic0/pic1 (our own art)
   hub/                      hub pictures, fonts, sounds, music (our own art)
   hub/button_prompts/playstation/<hash>.dds   PlayStation button pictures
   Game Data/                <- the player's extracted disc (default.xex, *.ng2, ...)
   DLC/                      <- the player's STFS DLC packages (optional)
   Saves Import/             <- Xbox 360 save folders to import (optional)
   Texture Packs/            <- packs made on a PC (optional)
   UserData/                 settings.toml, profile, saves, installed DLC,
                             shader/pipeline caches
   logs/                     ng2_NNN.log, crash records
```

Code layers (all inside `ng2-ps5/`):

1. **Generated code** (`generated/`, ~200+ MB C++, never committed): `rexglue
   codegen` over your own `default.xex` with NG2's manifest and TOMLs, then
   NG2's two post-codegen patches, then `ps5/tools/ps5_patch_generated.py`.
2. **ReXGlue runtime** (`third_party/rexglue-sdk`): the `rexglue-ps5` branch
   with `ps5/patches/rexglue-ng2.patch` applied.
3. **NG2 app code** (`src/`): unchanged game fixes and hooks
   (`patch_hooks.cpp`, `ng2_audio_fix.cpp`, `ng2_chapter_fix.cpp`,
   `ng2_autoskip.cpp`, `ng2_saveimport.cpp`, ...), with Windows-only pieces
   guarded by `#if !defined(__PROSPERO__)` and marked `// [ps5 port]`.
4. **PS5 layer** (`src/ps5/`): the hub, the in-game overlay, button prompts,
   PS5 defaults, folder layout, DLC/save/texture-pack pages, sound.

Coding conventions: match NG2's style (its comments explain *why*, at length;
keep that). Every PS5 change outside `src/ps5/` is marked `// [ps5 port]` and
guarded with `__PROSPERO__`, so upstream NG2 fixes can still be merged in.

---

## 5. Feature specification

### 5.1 Game files (`Game Data/`)

- The hub checks for `Game Data/default.xex` and verifies the XEX title id
  (`544307D5`) and the expected `*.ng2` archives. Missing or wrong files show a
  clear message with the path to copy to; START GAME is disabled until fixed.
- `$SystemUpdate` is not needed and is ignored.
- The game mounts `Game Data/` as the disc (read-only).

### 5.2 DLC

- `DLC/` holds the extensionless STFS packages from
  `<content>\544307D5\00000002\`. The hub's **Downloadable Content** page lists
  each package by its display name and status (new / installed / damaged) and
  installs them into `UserData` with the SDK content manager (NG2's existing
  path). START GAME installs any not yet installed. "<name> (1)" duplicates
  count as installed (svr2011 fix).
- **Mission Mode needs the game's title update** (NG2 issue O4). Title-update
  support (`.xexp` patch at codegen, as skate3 does for TU3) is a later
  milestone; the DLC page says so rather than failing silently.

### 5.3 Saves

- Saves and the profile live in `UserData/`. The **Saves** page imports Xbox
  360 save folders from `Saves Import/` (NG2's `ng2_saveimport`), lists the
  saves found, and can back up `UserData` to a dated folder.

### 5.4 The hub (pre-game launcher)

Drawn with **ImGui on ReXGlue's Vulkan presenter**, the way skate3-ps5 does it
(`OnFinalizePaths(defaults, resume)`: NG2 already uses this hook for its PC
setup screen, so the hub replaces that screen at exactly that point and calls
`resume()` on START GAME).

Look (inspired by the NG2 title menu, **no game art shipped**; everything is
painted procedurally or is our own artwork, as the v1.3.0 PC setup screen did):
- Background: layered misty grey mountains, a grass field with dozens of
  katanas standing in it (silhouettes, slight parallax drift), slow fog.
- Title: "NINJA GAIDEN" in a chrome/black bevelled display face, with a red
  brush-stroke "II". (Use an OFL display font; do not trace the game's logo.)
- Menu: centred vertical list, embossed off-white serif capitals with a dark
  outline; the focused item turns red with a soft glow and a short
  slash-wipe; focus glides between items (ps5-homebrew-ui springs).
- Sounds: short metallic "shing" for move, a blade "slash" for confirm, a
  muted thud for back (our own recordings/synthesis). Optional menu music from
  `hub/music/` (mp3/ogg/flac/wav), off by default.
- DualSense glyphs drawn from shapes (Cross, Circle, Square, Triangle, L1/R1,
  OPTIONS) in the button hint bar.

Structure:
- **START GAME** (OPTIONS also starts) - shows the resolution chosen.
- **SETTINGS** - tabbed with L1/R1: Display, Graphics, Game, Textures,
  Performance, Developer.
- **DOWNLOADABLE CONTENT** - 5.2.
- **SAVES** - 5.3.
- **ABOUT** - version, credits (TheSaltTrader's recomp, ReXGlue, Xenia,
  elripalda's PS5 runtime, Nazky, mihawk-99, BlackBearReloaded), folder
  status, and **Write diagnostics** (log + settings + system info to `logs/`).
- **QUIT** - back to the PS5 home screen through the system shell.

Controls: Cross select, Circle back, D-pad/left stick move, L1/R1 tabs,
Square resets the focused setting to default, Triangle shows the help text for
the focused setting. Every setting row shows its value, a one-line
description, and "restart" if it only applies at the next start.

### 5.5 In-game overlay (the F10 menu, consolized)

- Opened by **holding the touchpad button for ~0.6 s** (a short press stays
  the Xbox BACK button). The game is paused while it is open (input to the
  guest is suppressed).
- Holds every setting that can change live (the hot-reload ones), the
  performance readouts switch (old F8), the texture-pack switch (old F9), and
  "Return to hub" / "Quit to home screen".
- Settings that need a restart are shown greyed and marked (restart), as on
  PC.

### 5.6 Every setting, mapped

| PC setting (ng2_settings.cfg) | PS5 | Page |
|---|---|---|
| `window_width/height`, `fullscreen`, `monitor` | Replaced by **Resolution: 4K / 1440p / 720p** (5.7) | Display |
| `fps` (30-60) | **Frame rate: 30 / 60** (60 default; above 60 the game runs fast) | Display |
| `vsync` | Always on (V-Sync off is a speed control on this title, D10) | - |
| `internal_720p` | **Base render size: 1280x720 / original 1120x584** (restart) | Display |
| `resolution_scale` | Set by Resolution; an "Advanced: render scale 1-3x" override | Display |
| `letterbox` | **Keep aspect ratio** | Display |
| `present_dither` | **Dither output** | Display |
| `fov_scale` | **Field of view** slider (live) | Display |
| `antialias` | **Anti-aliasing: Off / FXAA / FXAA Extreme** | Graphics |
| `present_effect`, `cas_sharpness` | **Output filter: Bilinear / CAS (/ FSR if the build declares it)**, **Sharpness** | Graphics |
| `anisotropic` | **Anisotropic filtering: Game default / 1x-16x** | Graphics |
| `fuzzy_alpha`, `accurate_depth` | **Workarounds** group, off by default | Graphics |
| `video_mode` (skip intros) | **Skip intro videos** | Game |
| `skip_cinematics` | **Skip chapter cinematics** | Game |
| `chapter12_workaround` | **Chapter 12 workaround** (Workarounds group) | Game |
| (S12) infinite karma | **Infinite karma** cheat, off | Game |
| (PS5) button prompts | **Button prompts: PlayStation / Xbox** | Game |
| `texture_cache_mb` | **Texture cache size** (PS5-capped choices) | Performance |
| `texture_dump`, `texture_pack`, `texture_path`, `texture_scale` | **Dump textures**, **Use texture pack** (mutually exclusive, T8), pack picker from `Texture Packs/` | Textures |
| `texture_ai`, `texture_ai_strength` | PC-only (pack is made on PC) | - |
| `hud_*` | **Performance readouts**: FPS, CPU, GPU, memory, as bars | Performance |
| (PS5) shader mode | **Shader compilation: Smooth / Exact** (5.8) | Performance |
| (PS5) | **Prepare shaders now** button, cache size, **Clear shader cache** | Performance |
| `skip setup screen` | The hub always opens; OPTIONS on the hub starts at once | - |

### 5.7 Resolution: 4K, 1440p, 720p

ReXGlue's Xenos backend scales the guest's render targets by **integer**
factors (`draw_resolution_scale_x/y`). With the 1280x720 base (NG2's
`internal_720p` hook) the presets map exactly:

| Choice | Render | Scale |
|---|---|---|
| 4K (default) | 3840x2160 | 3x of 1280x720 |
| 1440p | 2560x1440 | 2x |
| 720p | 1280x720 | 1x (the game's own resolution with the 720p patch) |

**Decided (2026-10-17): 1080p is not offered.** It would be 1.5x,
which integer scaling cannot render; 720p takes its place. Selecting a preset
sets `internal_720p` on and the matching `resolution_scale`. "Base render
size: original 1120x584" (5.6) stays available for anyone who wants the
unpatched framebuffer, and then the scale gives 1x/2x/3x of that instead.

The output goes to the console's video mode through `VK_KHR_display`; the
presenter scales the guest frame to it with the chosen output filter. Changing
resolution is a restart setting (it is chosen in the hub before START GAME).

### 5.8 Performance and shader stutter

Goal: no change to what is drawn; remove hitches.

- **Persistent shader + pipeline storage.** The Vulkan pipeline cache already
  has Xenia's shader storage (`InitializeShaderStorage`, pipelines stored by
  description). Point it at `UserData/cache/shaders`, keep it on, and also
  persist a `VkPipelineCache` blob so RADV's own compile is skipped on later
  runs.
- **Pre-build at launch.** On START GAME, before the guest starts, load the
  stored descriptions and build every pipeline on worker threads behind a
  "Preparing shaders" bar (the SDK's startup-loading pass). Skate3 1.1 and
  svr2011 do this; it is what turns "stutter the first time" into "stutter
  once per install".
- **Async creation** (`vulkan_pipeline_creation_threads`, 6 on PS5): a new
  pipeline is built off the GPU thread. The SDK draws with a placeholder
  (discard) until it is ready, which can drop an effect for a frame the first
  time it appears. That is a visible difference, so it is a setting:
  **Smooth** (async, default) or **Exact** (wait for the pipeline; may hitch
  once).
- **Never ship game-derived caches.** Shader storage is built from the
  player's own game on the player's console.
- CPU: `-march=znver2`, Release with `-O2`, no `-mcmodel=large` (breaks the
  signer), fibers via the asm switch, `ps5_wait_spin_us` / `ps5_cs_spin_us`
  tuned for 8 Zen 2 cores, 16 MiB guest thread stacks.
- Memory: PS5 has unified memory; cap texture cache (skate3: 1 GB) and never
  run texture dump and pack together.
- Measure, do not guess: frame-time log per chapter (NG2's `ng2_perf`), the
  F3-style frame-time overlay, and a stutter counter (frames > 25 ms) in the
  diagnostics file. Chapter 12 is the heaviest stage; it is the benchmark.

### 5.9 PlayStation buttons (the one asset change)

- Mechanism: generalise skate3-ps5's approach. When the GPU texture cache
  uploads a small DXT texture, hash its guest bytes (FNV-1a 64); if
  `hub/button_prompts/playstation/<hash>.dds` exists, write the PlayStation
  picture over the Xbox one in guest memory, in the guest's tiling and byte
  order. Nothing in `Game Data/` is modified; the swap happens in memory each
  run. Setting: PlayStation (default) / Xbox.
- Discovery: a developer setting dumps every small HUD/menu texture once to
  `logs/hud_pictures/` so each Xbox icon's hash and format are known. NG2 may
  keep button glyphs inside a font atlas rather than as single pictures: the
  replacement then targets that atlas by hash (size filter relaxed for listed
  hashes only).
- Mapping: A->Cross, B->Circle, X->Square, Y->Triangle, LB/RB->L1/R1,
  LT/RT->L2/R2, LS/RS(+click)->L3/R3, START->OPTIONS, BACK->touchpad,
  Xbox guide icon->PS logo where it appears. All PlayStation pictures are
  **drawn for this project** (no Sony asset files), at the same size, format
  and mip count as the Xbox picture they replace.
- Out of scope: Xbox buttons baked into the tutorial **videos**, and text such
  as the "Xbox LIVE Marketplace" menu entry (text, not an icon).

### 5.10 Input, audio, system

- DualSense via the PS5 input driver: Cross/Circle/Square/Triangle = A/B/X/Y,
  L1/R1 = LB/RB, L2/R2 = LT/RT (analog), L3/R3, OPTIONS = START, touchpad
  click = BACK. Rumble through `scePadSetVibration`.
- Audio: 48 kHz `sceAudioOut`, 256-sample frames, with the A1 credit fix.
- System language read at first start (for the hub; the game's own language
  follows the disc).
- Exit to the home screen through the system shell, never by crashing out.

---
