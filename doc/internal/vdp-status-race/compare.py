#!/usr/bin/env python3
"""Compares two result files of vdprace, vdpedge, vdpmode or vdpirq, e by e.

usage: compare.py REAL EMULATED

Usually the first is a file of a real machine (results/) and the second one of
the same program in openMSX. Both files list "e a b c [d]": the counts of the
outcomes at that e, in the order the program stores them:
- vdprace, vdpmode: lost, seen, before (vdpmode MODE 3: -, HR=1, HR=0)
- vdpirq: lost, seen, before, pulse
- vdpedge MODE 0-2: (unused), bit 1, bit 0
- vdpedge MODE 3-4: second write lost, both written, other

Prints both files as one character per e (the outcome of most trials at that
e: '*' first column, '+' second, '-' third, '!' fourth, '.' no trials; lower
case 'x', 'p', 'm' and 'i' if less than 90% of the trials agree), then the e
values where both
have trials and the outcome of most trials differs.
"""

import sys

NE = 368

def load(path):
    counts = {}
    triples = []
    with open(path, errors='replace') as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == 'T':
                triples.append(' '.join(t[1:]))
            elif t[0].isdigit():
                counts[int(t[0])] = tuple(map(int, t[1:5]))
    return counts, triples

def outcome(c):
    return max(range(len(c)), key=lambda i: c[i])

def char(c):
    if c is None or not sum(c):
        return '.'
    i = outcome(c)
    sure = c[i] >= 0.9 * sum(c)
    return ('*+-!' if sure else 'xpmi')[i]

def main():
    files = sys.argv[1:3]
    data = [load(p) for p in files]
    for path, (counts, triples) in zip(files, data):
        print(path)
        line = ''.join(char(counts.get(e)) for e in range(NE))
        for i in range(0, NE, 80):
            print('%3d %s' % (i, line[i:i + 80]))
        if triples:
            print('read back (MODE 4):', '; '.join(triples))
    (real, _), (emu, _) = data
    diff = [e for e in sorted(real) if e in emu and sum(real[e]) and sum(emu[e])
            and outcome(real[e]) != outcome(emu[e])]
    print('differing e values:', len(diff))
    for e in diff:
        print('  e %d: %s %s' % (e, real[e], emu[e]))

main()
