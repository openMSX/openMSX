#!/usr/bin/env python3
"""Summarise a vdpvram result file: per screen and gap, the write moments whose
majority outcome is not 'both written', as runs of 'first-last:class'.

Usage: summary.py DATA/<machine>.txt

DATA: the folder vdp-vram-timing/cpu-write-pairs in
https://github.com/MBilderbeek/vdp-measurements.

Classes: 0 = (AAh, 55h, 00h) both written, 1 = (55h, 00h, 00h),
2 = (55h, 55h, 00h), 3 = (AAh, AAh, 00h), 4 = (00h, 55h, 00h), 5 = other.
The second OUT comes 14, 19, 21, 24, 28, 35 or 42 Z80 cycles after the
first (gap 0 to 6; one more with the extra VDP I/O wait of the S1990).
"""
import re
import sys
from collections import Counter, defaultdict

GAPS = [14, 19, 21, 24, 28, 35, 42]


SCREENS = {"T": "text 80", "U": "text 40", "C": "SCREEN 1", "G": "SCREEN 5",
           "H": "SCREEN 5, sprites off", "D": "SCREEN 5, display off",
           "Y": "SCREEN 8 (sprites off, see README.md)",
           "R": "SCREEN 5, R#18 = 08h (sprites off, see README.md)",
           "P": "SCREEN 5, sprites on, R#18 = 08h (vdpvram2.bas)"}


def parse(path, screen):
    """the blocks of one screen (its letter in SCREENS) ->
    {gap: {e: [n0..n5]}}, {gap: [((b0, b1, b2), n)]}"""
    data = defaultdict(lambda: defaultdict(lambda: [0] * 6))
    triples = defaultdict(list)
    gap = None
    for line in open(path, errors="replace"):
        line = line.replace("\x1a", "").strip()
        if not line or line.startswith("triples"):
            continue
        if line.startswith("VDPVRAM"):
            s = re.search(r"S=(\w)", line).group(1)
            gap = int(re.search(r"G=\s*(\d+)", line).group(1)) if s == screen else None
        elif gap is None:
            continue
        elif line.startswith("T"):
            v = line[1:].split()
            triples[gap].append((tuple(int(x) for x in v[:3]), int(v[3])))
        elif m := re.match(r"(\d+)-(\d+)\s+0=(\d+)$", line):
            first, last, n = map(int, m.groups())
            for e in range(first, last + 1):
                data[gap][e][0] = max(1, n // (last - first + 1))
        else:
            parts = line.split()
            for p in parts[1:]:
                k, n = p.split("=")
                data[gap][int(parts[0])][int(k)] = int(n)
    return data, triples


def wrong(counts):
    """{e: majority class} for the e values whose majority isn't class 0"""
    out = {}
    for e, c in counts.items():
        if sum(c):
            k = max(range(6), key=lambda i: c[i])
            if k:
                out[e] = k
    return out


def runs(bad):
    out = []
    for e in sorted(bad):
        if out and out[-1][1] == e - 1 and out[-1][2] == bad[e]:
            out[-1][1] = e
        else:
            out.append([e, e, bad[e]])
    return " ".join(f"{a}-{b}:{k}" if a != b else f"{a}:{k}" for a, b, k in out)


if __name__ == "__main__":
    for screen, name in SCREENS.items():
        data, triples = parse(sys.argv[1], screen)
        if not data:
            continue
        print(f"== {name}")
        for gap in sorted(data):
            bad = wrong(data[gap])
            n = Counter(bad.values())
            print(f"gap {gap} ({GAPS[gap]} Z80 cycles): "
                  + (", ".join(f"class {k}: {n[k]} write moments" for k in sorted(n)) or "none"))
            if bad:
                print("  " + runs(bad))
            print("  triples: " + ", ".join(
                f"({a:02X},{b:02X},{c:02X}) {cnt}"
                for (a, b, c), cnt in sorted(triples[gap], key=lambda t: -t[1])))
