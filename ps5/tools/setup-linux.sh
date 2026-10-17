#!/usr/bin/env bash
# [ps5 port] Set up a Linux build machine (Ubuntu 24.04, or WSL2 Ubuntu 24.04)
# for Ninja Gaiden II on PS5. Run from the repository root:
#
#   ps5/tools/setup-linux.sh [work folder, default ~/ng2-ps5-deps]
#
# Installs the host packages, clones and builds Nazky's ps5-plume-triangle (the
# PS5 payload SDK, RADV, app CRT, stubs, signer), and checks out the ReXGlue
# SDK with the PS5 runtime (elripalda's rexglue-ps5 branch) into
# third_party/rexglue-sdk with NG2's SDK fixes applied. Safe to run again.
#
# SKIP_APT=1 skips the package step (for a machine where someone else ran
# `sudo apt-get install` with the list below, e.g. without a sudo password at
# hand in a non-interactive shell).
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
work=${1:-$HOME/ng2-ps5-deps}
mkdir -p "$work"

# The commit rexglue-ng2.patch was written against. A branch tip that moves
# on would make the patch fail to apply for reasons unrelated to this port.
sdk_revision=f9b13da60c766020a67adbf45a7e96caad00b02e

# clang-19 is the compiler the SvR 2011 / Skate 3 ports build with. The
# payload SDK's prospero-clang picks the newest llvm-config-NN it finds, so do
# not install a clang 20+ beside it (clang 20 changes how crt objects link).
# libclang-rt-19-dev: the builtins archive RADV links (radv-link.sh).
# libx11-xcb-dev, libwayland-dev: the SDK's UI library requires them on a
# Linux host build (src/ui/CMakeLists.txt), even for the codegen tool.
# rsync, unzip, wget, python3-ply: ps5-plume-triangle's dependency scripts.
packages=(git ccache clang-19 lld-19 llvm-19 libclang-rt-19-dev libc++-19-dev libc++abi-19-dev
    cmake ninja-build python3 python3-pip python3-venv python3-mako python3-ply python3-yaml
    python3-packaging flex bison glslang-tools pkg-config zlib1g-dev libzstd-dev xxd rsync
    unzip wget libx11-xcb-dev libwayland-dev)

if [[ -z ${SKIP_APT:-} ]]; then
    echo "==> host packages"
    sudo apt-get update
    sudo apt-get install -y "${packages[@]}"
else
    missing=()
    for p in "${packages[@]}"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q "install ok installed" || missing+=("$p")
    done
    if (( ${#missing[@]} )); then
        echo "missing packages: ${missing[*]}" >&2
        echo "run: sudo apt-get install -y ${missing[*]}" >&2
        exit 1
    fi
fi

# Mesa (RADV) needs meson >= 1.4; Ubuntu 24.04 ships 1.3.2. Keep a newer one
# in a venv of its own rather than in the system Python. ps5-plume-triangle's
# build-radv.sh takes it from $MESON.
echo "==> meson (>= 1.4, for Mesa)"
if [[ ! -x $work/pyenv/bin/meson ]]; then
    python3 -m venv --system-site-packages "$work/pyenv"
    "$work/pyenv/bin/pip" install --quiet 'meson>=1.4,<2'
fi
export MESON=$work/pyenv/bin/meson
"$MESON" --version

echo "==> ps5-plume-triangle (toolchain, RADV, packaging tools)"
# Pinned to the commit the v0.1.0 alpha was built with (it fetches its own
# PS5 payload SDK and PS5_Mesa revisions: b5efad5 and 1ba9874 at the time).
plume_revision=d54d9ad
if [[ ! -d $work/ps5-plume-triangle ]]; then
    git clone https://github.com/Nazky/ps5-plume-triangle "$work/ps5-plume-triangle"
fi
git -C "$work/ps5-plume-triangle" checkout -q "$plume_revision" || \
    echo "warning: could not check out ps5-plume-triangle $plume_revision; using its current commit"

# Its wrappers (tooling/prospero-clang18, tools/radv-link.sh) look for
# `clang` or `clang-18` unless PS5_CLANG says otherwise; Ubuntu's clang-19
# package installs only clang-19. ccache is used by its scripts when present.
export PS5_CLANG=${PS5_CLANG:-$(command -v clang-19)}
(cd "$work/ps5-plume-triangle" && tools/build.sh)

echo "==> ReXGlue SDK with the PS5 runtime"
sdk=$repo/third_party/rexglue-sdk
if [[ ! -d $sdk/.git ]]; then
    git clone --branch rexglue-ps5 https://github.com/elripalda/svr2011-ps5.git "$sdk"
fi
if [[ $(git -C "$sdk" rev-parse HEAD) != "$sdk_revision" ]]; then
    git -C "$sdk" cat-file -e "$sdk_revision^{commit}" 2>/dev/null || git -C "$sdk" fetch origin rexglue-ps5
    # Refuses (and stops the script) if the tree has changes, which is wanted:
    # a patched tree on another commit needs a person to look at it.
    git -C "$sdk" checkout --detach "$sdk_revision"
fi
# Shallow submodules: FFmpeg, MoltenVK, SDL and friends are large and only
# their pinned commit is needed. Fall back to full clones if a server refuses.
git -C "$sdk" submodule update --init --recursive --depth 1 --jobs 8 ||
    git -C "$sdk" submodule update --init --recursive --jobs 8
if git -C "$sdk" apply --check "$repo/ps5/patches/rexglue-ng2.patch" 2>/dev/null; then
    git -C "$sdk" apply "$repo/ps5/patches/rexglue-ng2.patch"
    echo "    NG2 SDK fixes applied"
else
    git -C "$sdk" apply --check -R "$repo/ps5/patches/rexglue-ng2.patch" \
        && echo "    NG2 SDK fixes already applied" \
        || { echo "rexglue-ng2.patch does not apply: the branch moved, refresh the patch" >&2; exit 1; }
fi
for p in "$sdk/patches/ffmpeg-ps5.patch"; do
    [[ -f $p ]] || continue
    if git -C "$sdk/thirdparty/FFmpeg" apply --check "$p" 2>/dev/null; then
        git -C "$sdk/thirdparty/FFmpeg" apply "$p"
        echo "    FFmpeg PS5 patch applied"
    elif git -C "$sdk/thirdparty/FFmpeg" apply --check -R "$p" 2>/dev/null; then
        echo "    FFmpeg PS5 patch already applied"
    else
        echo "ffmpeg-ps5.patch does not apply to thirdparty/FFmpeg" >&2
        exit 1
    fi
done

cat <<EOF

Done. Next (ps5/BUILD.md has the details):
  export PS5_PAYLOAD_SDK=$work/ps5-plume-triangle/.deps/native/ps5-payload-sdk
  export PS5_TRIANGLE=$work/ps5-plume-triangle
  1. put your own default.xex in assets/ and run the host codegen
  2. cross-build with ps5/cmake/ps5-toolchain.cmake
  3. ps5/tools/ps5_package.sh out/ps5 \$PS5_TRIANGLE
EOF
