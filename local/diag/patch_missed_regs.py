"""Fix the two buggy `_missed` continuation functions.

config/functions.toml registers several branch-target fragments as their own
functions so stray `b` branches resolve. For fragments that are really the
epilogue/continuation of their parent, this severs the parent's non-volatile
registers (localised as C++ locals per function), so they read as 0.

Two of the 23 fragments read a non-volatile register before writing it, so they
crash on a null base:
  * sub_8372398C_missed  reads r26,r27  -> null write at the chapter-13 boss
  * sub_83A566F8_missed  reads r22,r31  -> latent, 11 branch sites in sub_83A4A910

Fix: at each branch site, spill the live locals into the shared context; at the
fragment's entry, restore them from the context. ctx.rNN are real fields (used
throughout the generated code), so this compiles and carries the values across.

Idempotent: a file already carrying the NG2FIX marker is skipped. Re-run after
any codegen regeneration.
"""
import sys

import os
# generated/default, located from this file so the script works from any checkout.
GEN = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", "..", "generated", "default"))


# [ps5 port] Where a function lands among the ng2_recomp.N.cpp shards depends
# on the SDK build that ran codegen (the Linux codegen for the PS5 port splits
# them differently from the Windows one), and the listed shards used a Windows
# path separator. The listed shard is tried first; if it does not hold the
# anchor (or the fix), the one shard that does is used instead.
import glob


def shard(path, *needles):
    def holds(p):
        try:
            with open(p, encoding="utf-8", errors="replace") as fh:
                t = fh.read()
        except OSError:
            return False
        return any(n in t for n in needles)
    if holds(path):
        return path
    found = [p for p in sorted(glob.glob(os.path.join(GEN, "ng2_recomp.*.cpp"))) if holds(p)]
    return found[0] if len(found) == 1 else path

EDITS = [
    # (file, old, new, expected_count)
    # 1. sub_8372398C_missed definition: restore r26/r27 from ctx at entry.
    (
        os.path.join(GEN, "ng2_recomp.234.cpp"),
        "DEFINE_REX_FUNC(sub_8372398C_missed) {\n"
        "\tREX_FUNC_PROLOGUE();\n"
        "\tPPCRegister r26{};\n"
        "\tPPCRegister r27{};\n",
        "DEFINE_REX_FUNC(sub_8372398C_missed) {\n"
        "\tREX_FUNC_PROLOGUE();\n"
        "\tPPCRegister r26{};\n"
        "\tPPCRegister r27{};\n"
        "\tr26.u64 = ctx.r26.u64; r27.u64 = ctx.r27.u64;  // NG2FIX: carry parent non-volatiles across boundary split\n",
        1,
    ),
    # 2. sub_8372398C_missed call site in sub_83723338: spill r26/r27 to ctx.
    (
        os.path.join(GEN, "ng2_recomp.373.cpp"),
        "\t// b 0x8372398c\n"
        "\tsub_8372398C_missed(ctx, base);",
        "\t// b 0x8372398c\n"
        "\tctx.r26.u64 = r26.u64; ctx.r27.u64 = r27.u64;  // NG2FIX\n"
        "\tsub_8372398C_missed(ctx, base);",
        1,
    ),
    # 3. sub_83A566F8_missed definition: restore r22/r31 from ctx at entry.
    (
        os.path.join(GEN, "ng2_recomp.236.cpp"),
        "DEFINE_REX_FUNC(sub_83A566F8_missed) {\n"
        "\tREX_FUNC_PROLOGUE();\n"
        "\tPPCXERRegister xer{};\n"
        "\tPPCCRRegister cr6{};\n"
        "\tPPCRegister r22{};\n"
        "\tPPCRegister r31{};\n",
        "DEFINE_REX_FUNC(sub_83A566F8_missed) {\n"
        "\tREX_FUNC_PROLOGUE();\n"
        "\tPPCXERRegister xer{};\n"
        "\tPPCCRRegister cr6{};\n"
        "\tPPCRegister r22{};\n"
        "\tPPCRegister r31{};\n"
        "\tr22.u64 = ctx.r22.u64; r31.u64 = ctx.r31.u64;  // NG2FIX: carry parent non-volatiles across boundary split\n",
        1,
    ),
    # 4. sub_83A566F8_missed call sites in sub_83A4A910: spill r22/r31 (11 sites).
    (
        os.path.join(GEN, "ng2_recomp.172.cpp"),
        "\tsub_83A566F8_missed(ctx, base);",
        "\tctx.r22.u64 = r22.u64; ctx.r31.u64 = r31.u64;  // NG2FIX\n"
        "\tsub_83A566F8_missed(ctx, base);",
        11,
    ),
]


def main():
    changed = 0
    for path, old, new, count in EDITS:
        path = shard(path, old, new)  # [ps5 port]
        with open(path, encoding="utf-8", errors="replace") as fh:
            txt = fh.read()
        if new in txt:
            print("SKIP (already patched): %s" % path)
            continue
        n = txt.count(old)
        if n != count:
            print("FAIL: %s expected %d of anchor, found %d" % (path, count, n))
            return 1
        txt = txt.replace(old, new)
        with open(path, "w", encoding="utf-8", newline="") as fh:
            fh.write(txt)
        print("OK: patched %d site(s) in %s" % (count, path))
        changed += 1
    print("done, %d file(s) changed" % changed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
