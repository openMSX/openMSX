#!/usr/bin/env python3
"""Generates yjk.inc for vdpline5.asm: lines of exactly 228 Z80 cycles
(incl. M1 waits). In each line the backdrop (R#7) gets COLB (OUT (C),D /
OUT (C),E) and D1 cycles later COLA again (OUT (C),A / OUT (C),E), so that
the stripe runs from the right border into the left border of the next line.
GROUPS groups of LINES lines; in group g the first write comes PRE0 + g
cycles into the line. Two blocks: yjk0 (VDP I/O without extra wait: OUT
(C),r = 14 cycles) and yjk1 (with: 15).
Arguments: D1 PRE0 GROUPS LINES OUTPUT, default 19 36 8 8 yjk.inc (for
vdpline5.asm); `genyjk.py 22 36 16 8 yjkl.inc`, `genyjk.py 28 36 16 8
yjkm.inc` and `genyjk.py 22 38 16 8 yjkn.inc` make the lines of vdpyjkl.asm,
vdpyjkm.asm and vdpyjkn.asm."""
import sys
def delay(n):
    # instructions (with cycles, incl. M1 wait) summing to exactly n >= 10,
    # without changing A, C, D, E
    for j in (0, 1):          # 0 or 1 JR $+2 (13)
        for p in (0, 1):      # 0 or 1 JP (11)
            r = n - 13*j - 11*p
            if r < 0: continue
            for b in range(r//7 + 1):   # INC HL (7)
                if (r - 7*b) % 5 == 0:  # NOP (5)
                    a = (r - 7*b) // 5
                    return ['jr $+2']*j + ['jp $+3']*p + ['inc hl']*b + ['nop']*a
    raise ValueError(n)
D1, PRE0, NG, LPG = (int(x) for x in (sys.argv[1:5] or (19, 36, 8, 8)))
OUT = sys.argv[5] if len(sys.argv) > 5 else 'yjk.inc'
out = []
for w in (0, 1):
    o = 14 + w
    out.append(f'yjk{w}:')
    for i in range(NG * LPG):
        g = i // LPG
        pre = PRE0 + g
        rest = 228 - pre - 4*o - D1
        out.append(f'\t; line {i}, group {g}: write {g} cycles later')
        out += ['\t' + x for x in delay(pre)]
        out += ['\tout\t(c),d', '\tout\t(c),e']
        out += ['\t' + x for x in delay(D1)]
        out += ['\tout\t(c),a', '\tout\t(c),e']
        out += ['\t' + x for x in delay(rest)]
    out.append('\tjp\tlines.end')
open(OUT, 'w').write('\n'.join(out) + '\n')
print('lines', len(out))
