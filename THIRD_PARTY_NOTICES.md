# Third-party notices

## How the licences fit together

- **This repository's own source** (the PS5 port, and Ninja's Dawn it forks) is
  under the **MIT licence** - see [LICENSE](LICENSE).
- **The PS5 release binary** (`eboot.bin`, and `sce_module/libc.prx` beside it)
  is built from this source together with the components below. Some of them -
  the PS5 payload SDK's runtime and ps5-plume-triangle's `libc.prx` - are
  **GPLv3 or later**, so the release is distributed under the terms of the
  **GNU General Public License, version 3** (the release zip carries its text
  as `COPYING-GPL-3.0.txt`).
- **Corresponding source** for a release: this repository at the release's tag,
  plus the pinned upstream revisions below. `ps5/tools/setup-linux.sh` fetches
  them at those revisions and `ps5/BUILD.md` describes the build.
- **No game material** is covered by any of these licences or included in the
  repository. The release binary contains Ninja Gaiden II's code in translated
  form, which remains Koei Tecmo's.

## Pinned upstream revisions (v0.1.0-alpha)

| Component | Source | Revision | Licence |
|---|---|---|---|
| ReXGlue SDK, PS5 runtime branch | https://github.com/elripalda/svr2011-ps5 (branch `rexglue-ps5`) + `ps5/patches/rexglue-ng2.patch` | `f9b13da` | BSD 3-Clause (Tom Clay; Xenia contributors) |
| ps5-plume-triangle | https://github.com/Nazky/ps5-plume-triangle | `d54d9ad` | GPLv3+ |
| PS5 payload SDK (fork) | https://github.com/mihawk-99/PS5_PayloadSDK | `b5efad5` | GPLv3+ (BSD for `include/freebsd`) |
| PS5_Mesa (RADV) | https://github.com/mihawk-99/PS5_Mesa | `1ba9874` | MIT and others (Mesa) |

The SDK in turn pins its own third-party code as submodules (FFmpeg
`0604b46`, SDL3 `8bf3b72`, Dear ImGui `6d910d5`, glslang `f4f1d8a`,
SPIRV-Tools `9a49b08`, SPIRV-Headers `29981f6`, Vulkan-Headers `e3b1eec`,
Vulkan-Loader `5f157b6`, Vulkan Memory Allocator `1d8f600`, spdlog `79524dd`,
fmt `407c905`, toml++ `3017243`, xxHash `e626a72`, utfcpp `63d64de`, simde
`71fd833`, libmspack `3059077`, o1heap `388a73f`, inja `7d1b460`, CLI11
`bfffd37`, stb). Each is under its own licence, found in its directory of the
SDK checkout (`thirdparty/<name>/`): MIT, BSD, zlib, Apache-2.0, LGPL (FFmpeg)
and others.

## Fonts

Cinzel (Natanael Gama) and Inter (Rasmus Andersson) are under the SIL Open
Font License 1.1; the licence texts are `ps5/assets/fonts/OFL-Cinzel.txt` and
`ps5/assets/fonts/OFL-Inter.txt`.

## WWE 2K14 Recomp patches

Three ReXGlue patches from the WWE 2K14 recompilation are ported in
`ps5/patches/rexglue-ng2.patch` (marked "WWE 2K14 Recomp's 0037/0041/0050" in
the code). Their licence requires this notice:

> BSD 3-Clause License
>
> Copyright (c) 2026, PromethenDev and WWE 2K14 Recomp contributors
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice,
>    this list of conditions and the following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice,
>    this list of conditions and the following disclaimer in the documentation
>    and/or other materials provided with the distribution.
> 3. Neither the name of the copyright holder nor the names of its
>    contributors may be used to endorse or promote products derived from
>    this software without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
> AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
> IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
> ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
> LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
> CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
> SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
> INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
> CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
> ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
> POSSIBILITY OF SUCH DAMAGE.

## ReXGlue SDK

> Copyright (c) 2026, Tom Clay <tomc@tctechstuff.com>
>
> Portions of this software are derived from the Xenia project:
> Copyright (c) 2022, Ben Vanik and Xenia project contributors
>
> Licensed under the BSD 3-Clause License (the same conditions and disclaimer
> as above); the full text is the `LICENSE` file of the SDK checkout.

## Release-only artwork and music

The release zip's home-screen background, icon and hub theme music are not in
this repository and are not covered by its licence. They are Koei Tecmo's
Ninja Gaiden II artwork and soundtrack, edited; the repository ships the
project's own icon and no music instead.
