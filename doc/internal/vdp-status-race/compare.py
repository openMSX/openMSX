#!/usr/bin/env python3
"""Compares two result files of vdprace, e by e.

usage: compare.py REAL EMULATED

Usually the first is a file of a real machine (from the measurements repository,
see README.md) and the second one of the same program in openMSX. Both files
list "e lost seen before": the number of trials with each outcome at that e.

Prints both files as one character per e (the outcome of most trials at that e:
'*' lost, '+' seen, '-' before, '.' no trials; lower case 'x', 'p', 'm' if less
than 90% of the trials agree), then the e values where both have trials and the
outcome of most trials differs.
"""

import sys

NE = 368

def load(path):
    counts = {}
    with open(path, errors='replace') as f:
        for line in f:
            t = line.replace('\0', '').split()
            if t and t[0].isdigit():
                counts[int(t[0])] = tuple(map(int, t[1:4]))
    return counts

def outcome(c):
    return max(range(len(c)), key=lambda i: c[i])

def char(c):
    if c is None or not sum(c):
        return '.'
    i = outcome(c)
    sure = c[i] >= 0.9 * sum(c)
    return ('*+-' if sure else 'xpm')[i]

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    files = sys.argv[1:3]
    data = [load(p) for p in files]
    for path, counts in zip(files, data):
        print(path)
        line = ''.join(char(counts.get(e)) for e in range(NE))
        for i in range(0, NE, 80):
            print('%3d %s' % (i, line[i:i + 80]))
    real, emu = data
    diff = [e for e in sorted(real) if e in emu and sum(real[e]) and sum(emu[e])
            and outcome(real[e]) != outcome(emu[e])]
    print('differing e values:', len(diff))
    for e in diff:
        print('  e %d: %s %s' % (e, real[e], emu[e]))

main()
