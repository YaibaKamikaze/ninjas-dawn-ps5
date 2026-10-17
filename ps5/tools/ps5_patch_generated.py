#!/usr/bin/env python3
"""[ps5 port] Make the generated code usable in a PS5 cross build.

Run after every codegen, after NG2's own post-codegen patches
(local/diag/patch_missed_regs.py, local/diag/patch_scanguard.py).
Idempotent.

    python3 ps5/tools/ps5_patch_generated.py [generated folder]

1. generated/rexglue.cmake has a build step that runs the codegen tool. In a
   cross build that tool would be a PS5 program and cannot run on the build
   machine, so the step is skipped when CMAKE_CROSSCOMPILING: codegen runs on
   the host first (it writes the same C++ for every platform).
2. Guest addresses from 0xE0000000 sit 0x1000 higher in host memory on hosts
   with 16 KB pages (Windows, arm64 macOS, the PS5). A codegen tool built from
   rexglue-ps5 already writes the PS5 into that test; one built from a stock
   SDK does not, so the precompiled header is checked and fixed here.
3. The bne check from NG2's ISSUES_AND_FIXES (R7): no `bne cr6` may have been
   emitted as a less-than test. A fresh codegen gives zero.
"""
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "generated")
cmake = root / "rexglue.cmake"
if not cmake.exists():
    sys.exit(f"{cmake}: not found (run codegen with a host build first)")

# --- 1. codegen step off in cross builds -----------------------------------
s = cmake.read_text()
if "[ps5 port]" in s:
    print(f"{cmake}: already patched")
else:
    # The codegen target is <project>_codegen; read the name rather than
    # assuming it, so a renamed project still patches.
    m = re.search(r"add_custom_target\((\w+_codegen)\b", s)
    if not m:
        sys.exit(f"{cmake}: no <project>_codegen target found")
    codegen = m.group(1)
    n = 0
    s, k = re.subn(
        r"^(\s*)add_dependencies\((\S+) " + codegen + r"\)\n",
        lambda mm: (f"{mm.group(1)}if(NOT CMAKE_CROSSCOMPILING)  # [ps5 port]\n"
                    f"{mm.group(1)}    add_dependencies({mm.group(2)} {codegen})\n"
                    f"{mm.group(1)}endif()\n"),
        s, flags=re.M)
    n += k
    start = s.find("add_custom_command(\n    OUTPUT \"${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp\"")
    end_marker = f"add_custom_target({codegen}"
    end = s.find(end_marker, start)
    if start < 0 or end < 0:
        sys.exit(f"{cmake}: the codegen command/target block was not found")
    end = s.find(")\n", end) + 2
    s = (s[:start]
         + "if(CMAKE_CROSSCOMPILING)\n"
         + "    # [ps5 port] the codegen tool would be a PS5 program: run codegen on the host.\n"
         + f"    add_custom_target({codegen})\n"
         + "else()\n"
         + s[start:end]
         + "endif()\n"
         + s[end:])
    cmake.write_text(s)
    print(f"{cmake}: codegen step skipped in cross builds ({n} dependencies guarded)")

# --- 2. 0xE0000000 host offset ----------------------------------------------
old = "#if REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64)\n"
new = "#if REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64) || REX_PLATFORM_PS5\n"
for header in sorted((root / "default").glob("*_pch.h")) + sorted((root / "default").glob("*_init.h")):
    t = header.read_text()
    if new in t:
        print(f"{header}: host offset already covers the PS5")
    elif old in t:
        header.write_text(t.replace(old, new))
        print(f"{header}: 0xE0000000 host offset on the PS5")

# --- 3. the bne sanity check ------------------------------------------------
bad = 0
for cpp in (root / "default").glob("*.cpp"):
    lines = cpp.read_text(errors="replace").splitlines()
    for i, line in enumerate(lines[:-1]):
        if "// bne cr6" in line and "cr6.lt) goto" in lines[i + 1]:
            print(f"{cpp.name}:{i + 2}: bne emitted as a less-than test")
            bad += 1
if bad:
    sys.exit(f"{bad} mistranslated bne (see ISSUES_AND_FIXES R7): regenerate, do not ship")
print("bne check: clean")
