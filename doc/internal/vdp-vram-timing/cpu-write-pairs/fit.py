#!/usr/bin/env python3
"""Fit the model of section 8.2 of ../2026-measurement-analysis.md to the
vdpvram results (see README.md).

Usage: fit.py DATA [--fixed | --r18 | --k]

DATA: the folder with the results (<machine>.txt), vdp-vram-timing/
cpu-write-pairs in https://github.com/MBilderbeek/vdp-measurements. Uses the
text 80, text 40, SCREEN 1 and SCREEN 5 results of the three machines.

--fixed: no fit, but the number of wrong write moments with HR falling at
cycle 212 of the tables' line in the text modes and 181 in the others, and K
18.

--k: per machine, the number of wrong write moments (text 80 and 40,
SCREEN 1 and 5 together) for K = 0 to 30, with the best Y for each.

--r18: with those values, the windows of gap 14 with sprites off, measured
and predicted, for R#18 = 00h and 08h (the padding 32 cycles earlier, and the
slots moving with HR, staying, or moving the other way).

The model, in VDP cycles, in the slot tables' line of 1368 cycles:
  * write 1's request reaches the arbiter at Y + 6 * e (e = write moment),
    write 2's 6 * gap (Z80 cycles) later;
  * a request gets the first slot s that is at least 16 memory cycles away
    (18 for a 'late' slot), never a slot 6 cycles after the previous one;
    memory cycles are VDP cycles minus the line's padding;
  * write 2's request is lost if it arrives before write 1's access A plus
    1 (plain slot) or minus 1 (late slot): then only one access takes place;
  * a write's byte is in the data register K cycles before its request
    reaches the arbiter, and an access writes what the data register holds.
Predicted classes: 0 = (AAh, 55h, 00h), 1 = (55h, 00h, 00h),
2 = (55h, 55h, 00h), 5 = anything else. Per result file Y and K are fitted
to the majority class of every write moment with trials, gaps 14, 19 and 21.

The slot tables, their padding and the slot classes are read from openMSX's
src/video/VDPAccessSlots.cc, which holds the tables of vdp-timing.html.
"""
import os
import re
import sys

import numpy as np

from summary import GAPS, parse

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "../../../../src/video/VDPAccessSlots.cc")
TICKS = 1368

# name: (table, padding (cycle, cycles), late lead, plain slots)
MODES = {
    "text": ("slotsText", [], 0, []),
    "char": ("slotsChar", [], 2, [166, 174]),
    "sprOn": ("slotsSpritesOn", [(1330, 2), (1337, 1), (1348, 1)], 2, [162, 170]),
    "sprOff": ("slotsSpritesOff", [(1332, 2), (1342, 2)], 0, []),
    "sprOff-r18-08": ("slotsSpritesOff", [(1300, 2), (1310, 2)], 0, []),
}
# screen letter in the result files: (name, slot table)
SCREENS = {"T": ("text80", "text"), "U": ("text40", "text"),
           "C": ("screen1", "char"), "G": ("screen5", "sprOn")}
# Cycles from write moment 0 (relative to HR's falling edge) to the request:
# with the S1990's extra wait (FS-A1GT) and without.
REQUEST_AFTER_E0 = {"fs-a1gt": 161, "hb-f700d": 135, "nms8250-v9958": 135}


def table(name):
    s = open(SRC).read()
    body = re.search(name + r" = \{(.*?)\};", s, re.S).group(1)
    vals = [eval(t) for t in body.replace("\n", " ").split(",") if t.strip()]
    return sorted(v for v in vals if v < TICKS)


class Mode:
    def __init__(self, mode):
        tname, pad, late, plain = MODES[mode]
        base = table(tname)
        slots = [s + k * TICKS for k in range(3) for s in base]
        mem = [0] * (3 * TICKS)
        for t in range(1, 3 * TICKS):
            mem[t] = mem[t - 1] + 1 - sum(c for at, c in pad if t % TICKS == at)
        tight = {s for i, s in enumerate(base) if i > 0 and base[i - 1] + 6 == s}
        self.late = np.zeros(TICKS, dtype=bool)
        for s in base:
            self.late[s] = late != 0 and s not in plain
        # the slot for a request at every cycle of the line
        self.slotfor = np.zeros(TICKS, dtype=np.int64)
        for r in range(TICKS):
            for s in slots:
                lead = late if self.late[s % TICKS] else 0
                if s % TICKS not in tight and mem[s] - mem[r] >= 16 + lead:
                    self.slotfor[r] = s
                    break


def predict(m, Y, K, gap, ne):
    e = np.arange(ne)
    r1 = Y + 6 * e
    r2 = r1 + 6 * gap
    line, t = np.divmod(r1, TICKS)
    A = line * TICKS + m.slotfor[t]
    thr = np.where(m.late[A % TICKS], -1, 1)
    lost = r2 < A + thr
    early = (r2 - K) <= A
    return np.where(lost, np.where(early, 1, 5), np.where(early, 2, 0))


def majority(machine, screen):
    data, _ = parse(os.path.join(DATA, f"{machine}.txt"), screen)
    out = {}
    for g in (0, 1, 2):
        out[g] = {}
        for e, c in data.get(g, {}).items():
            if sum(c):
                k = max(range(6), key=lambda i: c[i])
                out[g][e] = k if k in (0, 1, 2) else 5
    return out


FIXED_HR = {"text": 212, "char": 181, "sprOn": 181}
FIXED_K = 18


def fit(machine, screen, fixed):
    name = f"{machine} {SCREENS[screen][0]}"
    table = SCREENS[screen][1]
    m = Mode(table)
    real = majority(machine, screen)
    n = sum(len(r) for r in real.values())
    ne = 1 + max(max(r) for r in real.values() if r)

    def wrong(Y, K):
        return sum(sum(1 for e, c in r.items() if p[e] != c)
                   for g, r in real.items()
                   for p in [predict(m, Y, K, GAPS[g], ne)])

    if fixed:
        Y = FIXED_HR[table] + REQUEST_AFTER_E0[machine]
        K = FIXED_K
        bad = wrong(Y, K)
    else:
        bad, Y, K = min((wrong(Y, K), Y, K)
                        for K in range(0, 41, 2) for Y in range(TICKS))
    hr = (Y - REQUEST_AFTER_E0[machine]) % TICKS
    print(f"{name:28} {n:5} write moments, {bad:3} wrong: Y {Y:4}, K {K:2}, "
          f"HR falls at {hr}")


def r18():
    for machine, C in REQUEST_AFTER_E0.items():
        Y = 181 + C
        K = FIXED_K
        for screen, name in (("H", "screen5-sprites-off"), ("R", "screen5-r18-08")):
            real = majority(machine, screen)[0]
            print(f"{machine} {name}: measured {sorted(e for e, c in real.items() if c)}")
        cases = [("R#18 = 00h", "sprOff", Y),
                 ("R#18 = 08h, slots with HR", "sprOff-r18-08", Y),
                 ("R#18 = 08h, slots staying", "sprOff-r18-08", Y + 32),
                 ("R#18 = 08h, slots the other way", "sprOff-r18-08", Y - 32)]
        for label, mode, y in cases:
            p = predict(Mode(mode), y, K, GAPS[0], 368)
            print(f"    {label}: predicted {[int(e) for e in np.nonzero(p)[0]]}")


def kscan():
    for machine in REQUEST_AFTER_E0:
        total = {}
        for screen in SCREENS:
            m = Mode(SCREENS[screen][1])
            real = majority(machine, screen)
            ne = 1 + max(max(r) for r in real.values() if r)

            def wrong(Y, K):
                return sum(sum(1 for e, c in r.items() if p[e] != c)
                           for g, r in real.items()
                           for p in [predict(m, Y, K, GAPS[g], ne)])
            Y0 = min((wrong(Y, 14), Y) for Y in range(TICKS))[1]
            for K in range(0, 31, 2):
                total[K] = total.get(K, 0) + min(
                    wrong(Y, K) for Y in range(Y0 - 30, Y0 + 31))
        print(machine, " ".join(f"{K}:{n}" for K, n in sorted(total.items())))


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or args[0].startswith("--"):
        sys.exit(__doc__)
    DATA = args.pop(0)
    if "--r18" in args:
        r18()
        sys.exit()
    if "--k" in args:
        kscan()
        sys.exit()
    fixed = "--fixed" in args
    for machine in REQUEST_AFTER_E0:
        for screen in SCREENS:
            fit(machine, screen, fixed)
