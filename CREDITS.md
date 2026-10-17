# Credits

Ninja's Dawn for PS5 stands on a lot of other people's work.

## The PS5 port

- **YaibaKamikaze** - the PS5 port: bring-up, console testing, and direction.
  Built with the help of Claude (Anthropic) as a coding assistant.

## The recompilation it is built on

- **TheSaltTrader** - [Ninja's Dawn](https://github.com/FermataRest/Ninja-s-Dawn-A-Ninja-Gaiden-2-Recompilation-Project),
  the static recompilation of Ninja Gaiden II for PC that this port is a fork
  of: the function map, the game-specific fixes and hooks, the settings, the
  save import, the texture-pack format, and the long record of what was found
  and why (`docs/`). MIT licence.
- **Tom Clay** - the ReXGlue static recompilation SDK
  (codegen, runtime, kernel and GPU emulation). BSD 3-Clause.
- **Ben Vanik and the Xenia project contributors** - [Xenia](https://xenia.jp),
  from which much of ReXGlue's runtime and GPU emulation derives. BSD 3-Clause.

## PS5 runtime and techniques

- **elripalda** - the PS5 runtime this port runs on, from
  [svr2011-ps5](https://github.com/elripalda/svr2011-ps5) (the SDK branch is
  pinned at `f9b13da`), and the PS5 ports
  [wwe2k14-ps5](https://github.com/elripalda/wwe2k14-ps5) and
  [nbastreet-ps5](https://github.com/elripalda/nbastreet-ps5), studied for how
  a recompiled title is brought up on the console.
- **PromethenDev and the WWE 2K14 Recomp contributors** - the
  [WWE 2K14 recompilation](https://github.com/PromethenDev/wwe2k14-recomp)'s
  ReXGlue patches. Three are ported here: seeking the pipeline storage to the
  end before appending (0037), copying float-constant runs in one memcpy
  (0041), and no heap allocation per shared-memory range request (0050).
  BSD 3-Clause; their notice is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## PS5 homebrew toolchain

- **John Törnblom and the ps5-payload-dev contributors** - the
  [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk), used here through
  Mihawk-99's [fork](https://github.com/mihawk-99/PS5_PayloadSDK). GPLv3+.
- **Nazky, BlackBearReloaded and Mihawk-99** -
  [ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle), the native
  PS5 app boilerplate and toolchain (Vulkan on RADV, the signer, `libc.prx`).
  GPLv3+.
- **Mihawk-99** - [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa), Mesa's
  RADV Vulkan driver for the PS5. MIT (Mesa).

## Libraries (through the SDK)

SDL3, Vulkan headers and loader, Vulkan Memory Allocator, glslang, SPIRV-Tools,
Dear ImGui, spdlog, fmt, toml++, xxHash, FFmpeg, stb, and others - each under
its own licence; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Fonts

- **Cinzel** by Natanael Gama and **Inter** by Rasmus Andersson - the hub's
  fonts, under the SIL Open Font License 1.1 (`ps5/assets/fonts/`).

## The game

**Ninja Gaiden II** is © Koei Tecmo, developed by Team Ninja and published on
Xbox 360 by Microsoft. This project is not affiliated with or endorsed by any
of them, nor by Sony Interactive Entertainment.
