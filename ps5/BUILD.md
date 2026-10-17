# Building Ninja Gaiden II for PS5

The PS5 app is Ninja's Dawn (the Ninja Gaiden II recompilation, v1.0.24 source)
with the PS5 port on top. The recompiled code is generated from **your own**
`default.xex`, so you need your own copy of the game to build it.

Everything here runs on **Linux** (Ubuntu 24.04, or WSL2 Ubuntu 24.04 on
Windows). The PS5 toolchain does not run on Windows.

> Status: v0.1.0-alpha. Every step below runs end to end on Ubuntu 24.04 (a
> VirtualBox VM is the reference build machine) and produces the signed app
> folder the alpha release is made from; it runs on a PS5 on firmware 13.60.

## 0. Windows only: WSL2

In an administrator PowerShell, then reboot:

```
wsl --install -d Ubuntu-24.04
```

Keep the repository on the Linux side (`~/ng2-ps5`), not under `/mnt/c`: the
generated code is hundreds of MB of C++ and the Windows filesystem is slow from
WSL.

## 1. Toolchain and SDK

```
git clone <this repository> ~/ng2-ps5 && cd ~/ng2-ps5 && git checkout ps5
ps5/tools/setup-linux.sh
```

The first thing it does is `sudo apt-get install` (the package list is at the
top of the script). If sudo cannot ask for a password where you run it, install
that list yourself in a terminal and run `SKIP_APT=1 ps5/tools/setup-linux.sh`,
which only checks that the packages are there. Mesa needs meson 1.4 or newer
(Ubuntu 24.04 has 1.3.2), so the script keeps a newer one in a venv at
`~/ng2-ps5-deps/pyenv`; Pillow and fontTools for `ps5/tools/make_icon.py` go
there too. Expect about 5 minutes of downloads and 10-40 minutes for RADV.

Then it builds Nazky's
[ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle) in
`~/ng2-ps5-deps/` (PS5 payload SDK, RADV from mihawk-99's PS5_Mesa, app CRT,
stubs, `libc.prx`, `ps5-native-tool`); and checks out the ReXGlue SDK with the
PS5 runtime (elripalda's `rexglue-ps5` branch of svr2011-ps5: ReXGlue v0.10.0,
the version this port was written against) into `third_party/rexglue-sdk`,
with `ps5/patches/rexglue-ng2.patch` applied, at the commit the patch is
written against (`f9b13da`).

`rexglue-ng2.patch` re-creates the SDK fixes Ninja Gaiden II depends on (the
tree the PC releases were built from was never published):

| Fix | NG2 doc entry |
|---|---|
| `shared_vector_registers` codegen option | R3, R4 |
| `db16cyc` emitted as a pause with periodic yield | R5 |
| GPU ring re-initialisation resets both pointers, with an epoch | D12 |
| Audio credit returned on underrun, in the PS5 audio driver | A1 |
| Stuck-wait reports at 30 s and 300 s, once per wait (on the PS5 runtime's wait slots, which every wait entry point fills) | X4 |
| `video_mode_explicit` | D7 |
| A texture-load hook before the shared-memory upload (`rex/graphics/texture_load_hook.h`): the dump and the PlayStation button pictures use it | T1-T19 (dump; loading packs is still to do) |
| `rex/ps5_diag.h` symbols for host builds (the codegen tool links them) | - |

## 2. Generate the recompiled code (host build)

```
mkdir -p assets && cp /path/to/your/Game\ Data/default.xex assets/
# the codegen tool, for this machine, from the patched SDK
cmake -S third_party/rexglue-sdk -B out/sdk-host -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_C_FLAGS=-march=x86-64-v3 "-DCMAKE_CXX_FLAGS=-stdlib=libstdc++ -march=x86-64-v3" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DREXGLUE_ENABLE_TRACY=OFF
ninja -C out/sdk-host -j4 rexglue
# codegen before CMake: it also writes generated/rexglue.cmake, which the
# project's CMakeLists.txt includes. About 1.5 minutes, 2 GB of memory.
third_party/rexglue-sdk/out/linux-amd64/rexglue codegen ng2_manifest.toml
python3 local/diag/patch_missed_regs.py
python3 local/diag/patch_scanguard.py
python3 ps5/tools/ps5_patch_generated.py generated
```

The codegen tool must be built from the **patched** SDK (it carries
`shared_vector_registers` and the `db16cyc` fix) and against libstdc++ (the
libc++ build hangs during codegen, per the SvR 2011 port). `-march=x86-64-v3`
is needed because the SDK's byte-swap helpers use SSSE3 intrinsics; the tool
only runs on the build machine, so any CPU from the last ten years is fine.
Codegen prints two `Unresolved b target 0x83A4A9E0 / 0x83A4A9B4` lines on
every run; they are known and harmless (`docs/ISSUES_AND_FIXES.md`).

The two NG2 scripts find the functions they patch by content: where a function
lands among the `ng2_recomp.N.cpp` shards differs between the Windows and the
Linux codegen. Running codegen
before CMake also avoids the stale-precompiled-header failure NG2's
`tools/build.cmd` describes. This writes `generated/` (hundreds of MB of
C++ derived from the game). **Never commit or share it.** The last script also
runs the `bne cr6` check from `docs/ISSUES_AND_FIXES.md` (R7) and fails if a
mistranslation is found. Do **not** run `local/diag/patch_fix_ge2.py`.

## 3. Check the generated code is current

Codegen rewrites any generated file whose content differs from what it would
emit, which drops the two `NG2FIX` patches. Re-run the three scripts in step 2
after **every** codegen.

## 4. Cross-build for the PS5

```
export PS5_PAYLOAD_SDK=~/ng2-ps5-deps/ps5-plume-triangle/.deps/native/ps5-payload-sdk
cmake -S . -B out/ps5 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/ps5/cmake/ps5-toolchain.cmake \
  -DPS5_PAYLOAD_SDK=$PS5_PAYLOAD_SDK \
  -DREXSDK_DIR=$PWD/third_party/rexglue-sdk \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C out/ps5 -j4 ng2
```

The generated code is 545 translation units of up to a few MB each. With 8 GB
of memory start at `-j4`; measured, the largest needs about 300 MB, so `-j6`
fits with room to spare (6.5 minutes on 8 vCPUs). ccache makes a rebuild after
a codegen recompile only the shards that changed.

## 5. Package

```
ps5/tools/ps5_package.sh out/ps5 ~/ng2-ps5-deps/ps5-plume-triangle
```

The app folder is `out/ps5/ps5-app/PPSA94202`. The script ends by checking that
no game file is inside it and refuses the package if one is.

## 6. Install on the PS5

1. Copy `PPSA94202` to `/data/homebrew/` over FTP.
2. Copy your extracted disc into `/data/homebrew/PPSA94202/Game Data/`
   (`default.xex` directly inside it).
3. Optional: DLC packages into `DLC/`, Xbox 360 saves into `Saves Import/`.
4. Start it from your homebrew launcher. The hub opens; press START GAME.

Logs are in `/data/homebrew/PPSA94202/logs/`. Send the newest one with any
report.

## Where things are

| | |
|---|---|
| `src/` | The PC port's code, unchanged except `[ps5 port]` blocks |
| `src/ps5/` | PS5 versions of the Windows-only parts, the paths, button prompts |
| `src/ps5/hub/` | The hub: backdrop, pages, sounds |
| `src/ps5/overlay/` | The in-game overlay (the PC port's F10 menu, for the DualSense) |
| `ps5/package/` | What goes in the app folder besides `eboot.bin`: our own art, READMEs |
| `ps5/patches/` | NG2's SDK fixes |
| `ps5/tools/` | Setup, generated-code fix-up, packaging, the game-file guard |
