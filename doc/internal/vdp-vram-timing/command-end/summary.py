#!/usr/bin/env python3
"""Summarise a vdpce, vdpce2, ..., vdpce5 result file: per run the edge.

Usage: summary.py DATA/<machine>-vdpce<n>.txt

vdpce (VDPCE.ASM): MODE 0, the read of S#2 starts 19 + i Z80 cycles after
the start of the OUT that starts the command; MODE 1 (run RH): the second
R#46 write starts 33 + i cycles after it, CE read 27 cycles later.
vdpce2 (VDPCE2.ASM): the IN starts 12 + i cycles after it (plus 3 + 18 n with
a coarse delay n: LINE 32 dots 25, LMMM 16 dots 10); i = 1-4 and 6 have no
trials. MODE 0: CE; MODE 2: the byte at the command's last write; MODE 3:
POINT's S#7. vdpce3 (VDPCE3.ASM): as vdpce2, with the display and sprites
on; there the edges are spread over several cycles (the slots are far apart
and the trials start at slightly different moments), so the T printed is
where the majority changes. vdpce4 (VDPCE4.ASM): as vdpce2; run STP reads
12 + i cycles after a STOP written during a 16-byte HMMV; runs HC1, HCT, LCT
and LMT read TR (S#2 bit 7) instead of CE; WHC and WLC: MODE 2. vdpce5
(VDPCE5.ASM): as vdpce4; runs S0 (STOP with no command running) and S2 to S5
(STOP during a 16-byte HMMV, 18 Z80 cycles later in each) read 12 + i cycles
after the STOP; runs R4H and R4L (MODE 4): R#44 written by an OUT that
starts 26 + i cycles after the start of the command, class 1 if the new
value is what the command wrote.

Printed per run: for CE, the first T (Z80 cycles) at which the majority of
the trials saw CE = 0 after the last at which it saw CE = 1; for TR, the
first T at which the majority saw TR = 1; for MODE 2 and 3, the first T at
which the majority saw the new value.
"""
import sys

COARSE = {"L32": 25, "MG": 10}
MODES2 = {"WV": 2, "WL": 2, "WH": 2, "WM": 2, "WP": 2, "WN": 2, "WY": 2, "WT": 3,
          "WHC": 2, "WLC": 2}
TR_RUNS = {"HC1", "HCT", "LCT", "LMT"}


def parse(path):
    """-> [(run name, header, {i: [n0, n1, ...]})]"""
    runs = []
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        if line.startswith("VDPCE"):
            name = line.split("run=")[1].split()[0]
            runs.append((name, line, {}))
            continue
        i, *parts = line.split()
        c = [0, 0, 0]
        for p in parts:
            k, n = p.split("=")
            c[int(k)] = int(n)
        runs[-1][2][int(i)] = c
    return runs


if __name__ == "__main__":
    path = sys.argv[1]
    round2 = any(f"vdpce{n}" in path for n in (2, 3, 4, 5))
    round4 = "vdpce4" in path or "vdpce5" in path
    for name, header, counts in parse(path):
        ones = [i for i, c in counts.items() if c[1] > c[0]]
        if round2:
            base = 12 + (3 + 18 * COARSE[name] if name in COARSE else 0)
            mode = MODES2.get(name, 0)
        else:
            base = 33 if name == "RH" else 19
            mode = 1 if name == "RH" else 0
        if round4 and name in ("R4H", "R4L"):
            what = (f"the new R#44 written from T = {26 + min(ones)}" if ones
                    else f"the new R#44 never written (T = 26 to {26 + max(counts)})")
        elif round4 and name in TR_RUNS:
            what = f"TR set from T = {base + min(ones)}" if ones else "TR never set"
        elif mode in (0, 1):
            what = f"CE clear from T = {base + max(ones) + 1}" if ones else "CE clear at every read"
        else:
            what = f"new value from T = {base + min(ones)}" if ones else "never the new value"
        if name == "RH":
            what = f"CE set at {len(ones)} of {len(counts)} read moments (restart taken)"
        print(f"{name:4} {what}")
