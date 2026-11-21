#!/usr/bin/env python3
"""Summarise a vdpcmd result file: per screen and gap, the majority class of
every write moment as one character (0-4: the first of the five reads of S#2
that saw CE = 0; 5: none did; '.': no trials), 60 write moments per line.

Usage: summary.py results/<machine>.txt

Gap 0: the first read 34 Z80 cycles after the start of the OUT to R#46 that
starts the command, gap 1: 46; the reads are 25 Z80 cycles apart.
"""
import sys
from collections import defaultdict

NE = 368


def parse(path):
    """-> {(screen, gap): {e: [n0..n5]}}, {(screen, gap): header line}"""
    data = defaultdict(dict)
    headers = {}
    gap = None
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        if line.startswith("VDPCMD"):
            gap = (line.split("S=")[1].split()[0], int(line.split("G=")[1].split()[0]))
            headers[gap] = line
            continue
        e, *parts = line.split()
        c = [0] * 6
        for p in parts:
            k, n = p.split("=")
            c[int(k)] = int(n)
        data[gap][int(e)] = c
    return data, headers


def pattern(counts):
    return "".join(
        str(max(range(6), key=lambda k: counts[e][k])) if e in counts else "."
        for e in range(NE))


if __name__ == "__main__":
    data, headers = parse(sys.argv[1])
    for gap in data:
        print(headers[gap])
        p = pattern(data[gap])
        for i in range(0, NE, 60):
            print(f"  {i:3} {p[i:i + 60]}")
