"""[ps5 port] Turn the GPU thread profile in a log into function names.

    python3 ps5/tools/symbolize_profile.py <ng2_NNN.log> <symbols.txt | llvm-pie.elf> [top]

SETTINGS > Developer > Profile GPU thread makes the runtime sample the GPU
command thread a thousand times a second and log, every 10 s, the addresses it
found: "self" (where the thread was) and "on stack" (every return address on
its stack, counted once per sample - an inclusive profile). The addresses are
relative to rex::ps5diag::NowMs (+0x10000000), so they map back to the linked
ELF of that exact build: `out/ps5/ps5-link/build/llvm-pie.elf`, or the
`symbols.txt` (llvm-nm output) archived with the build. Prints both profiles
by function, summed over every profile window in the log.
"""
import bisect
import collections
import re
import subprocess
import sys

ANCHOR_BIAS = 0x10000000


def load_symbols(path):
    if path.endswith(".txt"):
        text = open(path, encoding="utf-8", errors="replace").read()
    else:
        text = subprocess.run(["llvm-nm-19", "-C", "--defined-only", path], capture_output=True, text=True,
                              check=True).stdout
    syms = []
    for line in text.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[1] in "tTwW":
            try:
                syms.append((int(parts[0], 16), parts[2]))
            except ValueError:
                pass
    syms.sort()
    anchor = next(a for a, n in syms if n == "rex::ps5diag::NowMs()")
    return syms, anchor


def name_of(syms, starts, address):
    # Past the last code symbol (the eboot's text ends there) is system code:
    # libkernel and libc, which have no symbols here. A thread sampled while
    # blocked in the kernel (asleep, waiting on a lock) is reported there too.
    if address < starts[0] or address > starts[-1] + 0x100000:
        return f"[system code {address:#x}]"
    i = bisect.bisect_right(starts, address) - 1
    return syms[i][1] if i >= 0 else "?"


def short(name, width=110):
    name = re.sub(r"\(.*\)$", "()", name)  # drop parameter lists
    return name if len(name) <= width else name[:width - 3] + "..."


def main():
    log, symbols = sys.argv[1], sys.argv[2]
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    syms, anchor = load_symbols(symbols)
    starts = [a for a, _ in syms]
    totals = {"self": collections.Counter(), "on stack": collections.Counter()}
    system_calls = collections.Counter()
    system_samples = 0
    samples = 0
    for line in open(log, encoding="utf-8", errors="replace"):
        m = re.search(r"ps5 profile .*?: (\d+) samples in \d+ s", line)
        if m:
            samples += int(m.group(1))
            continue
        m = re.search(r"ps5 profile .*?: (\d+) samples in system code", line)
        if m:
            system_samples += int(m.group(1))
            continue
        m = re.search(r"ps5 profile .*? system calls:((?: [0-9A-F]+<[0-9A-F]+<[0-9A-F]+:\d+)+)", line)
        if m:
            for item in m.group(1).split():
                chain, count = item.rsplit(":", 1)
                rip, caller, caller2 = (int(x, 16) for x in chain.split("<"))
                key = (f"{rip:#x}",
                       short(name_of(syms, starts, caller - ANCHOR_BIAS + anchor), 60) if caller else "-",
                       short(name_of(syms, starts, caller2 - ANCHOR_BIAS + anchor), 60) if caller2 else "-")
                system_calls[key] += int(count)
            continue
        m = re.search(r"ps5 profile .*? (self|on stack):((?: [0-9A-F]+:\d+)+)", line)
        if not m:
            continue
        for item in m.group(2).split():
            rel, count = item.split(":")
            address = int(rel, 16) - ANCHOR_BIAS + anchor
            totals[m.group(1)][name_of(syms, starts, address)] += int(count)
    if not samples:
        raise SystemExit("no 'ps5 profile' lines in the log (turn on Developer > Profile GPU thread)")
    print(f"{samples} samples\n")
    for kind in ("self", "on stack"):
        print(f"--- {kind} (share of samples) ---")
        for name, count in totals[kind].most_common(top):
            print(f"{100.0 * count / samples:6.1f}%  {short(name)}")
        print()
    if system_calls:
        print(f"--- system code ({100.0 * system_samples / samples:.1f}% of samples), by caller ---")
        for (rip, caller, caller2), count in system_calls.most_common(top):
            print(f"{100.0 * count / samples:6.1f}%  {rip}  <- {caller}  <- {caller2}")


if __name__ == "__main__":
    main()
