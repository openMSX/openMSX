#!/usr/bin/env python3
"""Generates stair.inc for vdpline4.asm: 64 lines of exactly 228 Z80 cycles
(incl. M1 waits), in 8 groups of 8; in group k the first write (OUT (C),D
/ OUT (C),E) comes k cycles later than in group 0, the second (OUT (C),A /
OUT (C),E) D1 cycles after the first. Two blocks: stair0 (VDP I/O without
extra wait: OUT (C),r = 14 cycles) and stair1 (with: 15).
Arguments: D1 (default 84) and the output file (default stair.inc);
`genstair.py 29 stairsw.inc` makes the lines of vdpsw.asm."""
import sys
def delay(n):
    # instructions (with cycles, incl. M1 wait) summing to exactly n >= 10
    for j in (0, 1):          # 0 or 1 JR $+2 (13)
        for p in (0, 1):      # 0 or 1 JP (11)
            r = n - 13*j - 11*p
            if r < 0: continue
            for b in range(r//7 + 1):   # INC HL (7)
                if (r - 7*b) % 5 == 0:  # NOP (5)
                    a = (r - 7*b) // 5
                    return ['jr $+2']*j + ['jp $+3']*p + ['inc hl']*b + ['nop']*a
    raise ValueError(n)
D1 = int(sys.argv[1]) if len(sys.argv) > 1 else 84
OUT = sys.argv[2] if len(sys.argv) > 2 else 'stair.inc'
out = []
for w in (0, 1):
    o = 14 + w
    out.append(f'stair{w}:')
    for i in range(64):
        k = i // 8
        pre = 10 + k
        rest = 228 - pre - 4*o - D1
        out.append(f'\t; line {i}, group {k}: write {k} cycles later')
        out += ['\t' + x for x in delay(pre)]
        out += ['\tout\t(c),d', '\tout\t(c),e']
        out += ['\t' + x for x in delay(D1)]
        out += ['\tout\t(c),a', '\tout\t(c),e']
        out += ['\t' + x for x in delay(rest)]
    out.append('\tjp\tlines.end')
open(OUT, 'w').write('\n'.join(out) + '\n')
print('lines', len(out))
