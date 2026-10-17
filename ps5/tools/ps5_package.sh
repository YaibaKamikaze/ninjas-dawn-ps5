#!/usr/bin/env bash
# [ps5 port] Link the PS5 build of Ninja Gaiden II and package it as a PS5
# homebrew app folder.
#
#   ps5/tools/ps5_package.sh <cmake build dir> <ps5-plume-triangle checkout> [title id]
#
# The link reuses the PS5 tooling that Nazky's ps5-plume-triangle builds
# (mihawk-99's RADV link recipe, the app CRT, libc.prx and the FSELF signer).
# Build that project once first: its build/ holds app_crt.o,
# app_cpp_runtime.o, the libSceAgc stubs and ps5-native-tool.
#
# No game files are packaged. The player copies their own disc into the app's
# "Game Data" folder on the PS5; ps5/tools/check_no_game_files.py refuses to
# finish a package that holds any.
set -euo pipefail

build=$(realpath "${1:?cmake build dir}")
tri=$(realpath "${2:?ps5-plume-triangle checkout}")
title_id=${3:-PPSA94202}
ps5dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(head -n1 "$ps5dir/VERSION.txt")

sdk_root=${PS5_PAYLOAD_SDK:-$tri/.deps/native/ps5-payload-sdk}
radv_archive=${RADV_ARCHIVE:-$tri/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a}
tool=$tri/build/host/ps5-native-tool
for f in "$sdk_root/bin/prospero-lld" "$radv_archive" "$tool" "$tri/build/obj/app_crt.o" \
         "$tri/build/obj/app_cpp_runtime.o" "$tri/build/stubs/libSceAgc.so" "$tri/runtime/libc.prx"; do
    [[ -e $f ]] || { echo "missing $f (build ps5-plume-triangle first)" >&2; exit 2; }
done

# radv_link_recipe reads tooling/ and vendor/ under a staging root and writes build/.
stage=$build/ps5-link
mkdir -p "$stage/build"
ln -sfn "$tri/tooling" "$stage/tooling"
ln -sfn "$tri/vendor" "$stage/vendor"

# The objects and static libraries CMake links into ng2, in CMake's order.
link_cmd=$(ninja -C "$build" -t commands ng2 | tail -1)
mapfile -t inputs < <(cd "$build" && for w in $link_cmd; do
    case $w in
        *.o|*.a) [[ $w = /* ]] && echo "$w" || echo "$build/$w" ;;
    esac
done)
(( ${#inputs[@]} > 0 )) || { echo "no link inputs found: build the ng2 target first" >&2; exit 2; }
echo "==> [link] ${#inputs[@]} objects and libraries from CMake"

export PS5_CLANG=${PS5_CLANG:-$(command -v clang-19 || command -v clang)}
# shellcheck source=/dev/null
source "$tri/tools/radv-link.sh"
radv_link_recipe "$stage" "$sdk_root" "$radv_archive"

"$sdk_root/bin/prospero-clang" -c -O2 -fno-visibility-from-dllstorageclass \
    -o "$stage/build/ps5_stat.o" "$ps5dir/tools/ps5_stat.c"

echo "==> [link] Ninja Gaiden II + RADV into a PS5 PIE ELF"
"$sdk_root/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --version-script "$tri/tooling/native/app-symbols.map" --exclude-libs=ALL \
    --wrap=stat --wrap=lstat \
    -e _start -o "$stage/build/llvm-pie.elf" \
    "$tri/build/obj/app_crt.o" "$tri/build/obj/app_cpp_runtime.o" "$stage/build/ps5_stat.o" \
    --start-group "${inputs[@]}" --end-group \
    "$tri/build/stubs/libSceAgc.so" "$tri/build/stubs/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" \
    --as-needed "$sdk_root"/target/lib/*.so

echo "==> [package] eboot.bin for $title_id"
"$tool" link --in "$stage/build/llvm-pie.elf" --out "$stage/build/eboot.elf" \
    --stub-dir "$sdk_root/target/lib" --stub "$tri/build/stubs/libSceAgc.so" \
    --stub "$tri/build/stubs/libSceAgcDriver.so" --module-sdk 0x02000009 \
    --companion-sdk 0x08050001 --file-name eboot.elf

app=$build/ps5-app/$title_id
rm -rf -- "$app"
mkdir -p "$app/sce_sys" "$app/sce_module"
# The binary carries the absolute paths of source files (assertion and log
# messages): the build machine's home folder would be published in every
# release. Replace that prefix with a placeholder of the same length (so no
# offset moves) before signing.
python3 - "$stage/build/eboot.elf" "$(dirname "$(dirname "$ps5dir")")" <<'PY'
import sys
path, prefix = sys.argv[1], sys.argv[2].encode()
data = open(path, "rb").read()
if len(prefix) >= 4:
    placeholder = (b"/src" + b"_" * len(prefix))[:len(prefix)]
    count = data.count(prefix)
    data = data.replace(prefix, placeholder)
    open(path, "wb").write(data)
    print(f"==> [package] {count} build-machine path(s) replaced in eboot.elf")
PY
"$tool" self --sign --in "$stage/build/eboot.elf" --out "$app/eboot.bin" --magic 0x1D3D154F
cp "$tri/runtime/libc.prx" "$app/sce_module/libc.prx"

content_version=$(printf '%02d.%03d.%03d' "$(cut -d. -f1 <<<"$version")" "$(cut -d. -f2 <<<"$version")" \
    "$(cut -d. -f3 <<<"$version.0")")
cat > "$app/sce_sys/param.json" <<EOF
{
  "ageLevel": { "default": 0 },
  "applicationCategoryType": 0,
  "applicationDrmType": "free",
  "attribute": 0,
  "attribute2": 0,
  "attribute3": 0,
  "conceptId": "${title_id:4}",
  "contentBadgeType": 1,
  "contentId": "UP9000-${title_id}_00-NINJAGAIDEN2PS50",
  "contentVersion": "${content_version}",
  "downloadDataSize": 256,
  "gameIntent": { "permittedIntents": [ { "intentType": "launchActivity" } ] },
  "localizedParameters": {
    "defaultLanguage": "en-US",
    "en-US": { "titleName": "Ninja Gaiden II" }
  },
  "masterVersion": "01.00",
  "pubtools": { "creationDate": "$(date -u +%Y-%m-%d) 00:00:00", "loudnessSnd0": "-28.00", "toolVersion": "2.00" },
  "requiredSystemSoftwareVersion": "0x0000000000000000",
  "sdkVersion": "0x0000000000000000",
  "titleId": "${title_id}",
  "versionFileUri": ""
}
EOF

# Never anything from the game disc: the home-screen art (sce_sys; see its
# README for where it comes from), the hub's fonts, sounds and music, the
# PlayStation button pictures, and the README in each folder the player fills.
cp -r "$ps5dir/package/sce_sys/." "$app/sce_sys/"
cp -r "$ps5dir/package/hub" "$app/hub"
for d in "Game Data" "DLC" "Saves Import" "Texture Packs"; do
    mkdir -p "$app/$d"
    cp "$ps5dir/package/$d/README.txt" "$app/$d/README.txt"
done
rm -f "$app/sce_sys/README.txt"

python3 "$ps5dir/tools/check_no_game_files.py" "$app"
"$tool" self --inspect --file "$app/eboot.bin" | tail -3
echo "==> $app (v$version, $(stat -c %s "$app/eboot.bin") bytes)"
