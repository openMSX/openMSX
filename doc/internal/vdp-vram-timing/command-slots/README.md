# V99x8: where the VRAM access slots are, relative to HR

## The question

The VDP serves VRAM accesses in fixed slots in each line, the same pattern every line of a
screen mode: the display fetches, the refresh, and the slots the CPU and the command engine
share. The logic-analyser captures (`../2026-measurement-analysis.md`, sections 2 and 8)
give that pattern exactly, with cycle 0 of their line where the VDP's HSYNC pin goes active
(`../vdp-timing.html`). But they don't show the status flags, so they don't say where in
that line the CPU sees HR. Software times its VRAM accesses and commands from HR (or from
an interrupt, which `../../vdp-status-race` places relative to HR). Where are the slots,
relative to HR? Is that the same for the CPU and the command engine, in every screen mode?
Does it move with the horizontal adjust (R#18)?

## The tests

1. **The CPU's write pairs** (`../cpu-write-pairs`): two VRAM writes 14 to 42 Z80 cycles
   apart, at every moment of a line relative to HR, and which bytes land where. The windows
   in which a write goes wrong repeat with the slot pattern, so their place shows where
   the slots are. `fit.py` there simulates the slot model of section 8.2 at every write
   moment and fits, per machine and screen mode, where HR's falling edge lies in the slot
   tables' line (together with the data register's lead, which only sets the width of one
   kind of window). Text 80, text 40, SCREEN 1 and SCREEN 5 with sprites; `--r18` also
   SCREEN 5 without sprites with R#18 = 00h and 08h (`vdpvram2.bas` repeats SCREEN 5 with
   sprites and R#18 = 08h).
2. **When a short command finishes** (`vdpcmd.asm` with `vdpcmd.bas`, SCREEN 5). A
   command started at a given moment does its first access in the first suitable slot
   after its start delay. Start it a little later and that access waits less, until the
   slot is missed and it has to wait for the next one. So the moment the command is done
   goes down by one cycle per cycle of later start, then jumps up by the distance between
   two slots. Each trial:
   1. set up an HMMV of one byte (2 dots) to an off-screen address, and wait until the
      previous command is done;
   2. measure the phase relative to HR as in `../cpu-write-pairs` (HR read once in each of
      48 lines, one Z80 cycle later in each);
   3. wait a known number of Z80 cycles, then complete the R#46 write: the command starts.
      The *write moment* is the moment of that `OUT`, relative to HR's falling edge plus a
      constant, in Z80 cycles;
   4. read S#2 five times, 25 Z80 cycles apart, the first one 34 Z80 cycles after the start
      of the `OUT` (*gap 0*) or 46 (*gap 1*); count which read first saw CE = 0.

   12 trials per write moment, 368 write moments one Z80 cycle apart. Screens: SCREEN 5
   with sprites on, with sprites off, with the display off, and with sprites on and R#18 =
   08h and 07h.

Machines for both: Panasonic FS-A1GT (V9958; its S1990 adds a wait to VDP I/O), Sony
HB-F700D (V9938), Philips NMS 8250 with a V9958 (no extra wait).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The slot pattern per screen mode, the request rule (16 memory cycles ahead), and the command start delay of HMMV | logic-analyser captures, sections 2, 8.2 and 11 | measured; exact on the 2013 captures (V9938) |
| The same pattern on the V9958 | not measured with the analyser | assumed; the windows of both tests repeat with the same pattern on both chips |
| The write moments relative to HR's falling edge | HR's phase measured per trial, the programs' cycle counts | to one Z80 cycle (6 VDP cycles) |
| The delay from an `OUT` to its request at the arbiter: about 29 VDP cycles from the start of the Z80's I/O cycle, plus the S1990's wait on the FS-A1GT | section 8.2 (10.9 VDP cycles after /CSW rises) and the Z80's bus timing | derived; an error here shifts the found position by the same amount |
| When CE clears after the command's access | not known; a free parameter in test 2 | it only changes where a run of 1s (below) ends, not where it starts |

## The results

**CPU write pairs**, `fit.py`: the cycle of the slot tables' line at which HR falls,
fitted per screen mode, and the write moments the model then gets wrong:

| machine | text 80 / 40 | SCREEN 1 | SCREEN 5 | wrong |
|---|---|---|---|---|
| FS-A1GT (V9958) | 218 | 180 | 176 | 0.8 to 2 % |
| HB-F700D (V9938) | 216 | 188 | 184 | 3 to 9 % |
| NMS 8250 (V9958) | 210 | 180 | 174 | 0.8 to 2.4 % |

The three machines agree to within 14 VDP cycles (about 2 Z80 cycles). The text modes
come out 28 to 42 VDP cycles later than the graphics modes; HR itself falls 31 VDP cycles
later in the text modes (`../../vdp-status-race/flag-positions.md`), so relative to F and
VR the slots are at the same place in all modes. One position for all (`fit.py --fixed`: HR falls at 181, 212 in the text modes) gets 0.8 to 19 % wrong;
most of what remains is the HB-F700D's outcomes right at the end of a window, which the
model doesn't have (`../cpu-write-pairs`).

**R#18**, SCREEN 5 without sprites, where one window is left (`fit.py --r18`): it is at
the same write moment relative to HR with R#18 = 08h as with 00h, on all three machines
(FS-A1GT 142-143, NMS 8250 147-148, HB-F700D 146). On the two V9958s, if the slots
stayed in place while HR moved, the model puts it 5 write moments earlier; if they moved
the other way, 5 later. (On the HB-F700D the model has no window there.)

**The command's end**, `summary.py`: the majority outcome per write moment, `1` the
command not done at the first read, `0` done. FS-A1GT, SCREEN 5 with sprites, gap 0:

```
    0 ..000.1111.0001111111100011111111000111111110001111111100000
   60 000110000110001111111000010000110001111111000011000110001111
```

In a run of 1s the command gets done earlier and earlier, until it is done in time (0s);
where a run of 1s starts, the start has just missed a slot and the wait jumps up. So **the
places where a run of 1s starts are the slot positions**, shifted by the command's fixed
start delay. With gap 1 (first read at 46) every write moment gives 0: the command is
always done by then.

The slot model was simulated with openMSX (which implements it) running the same program,
with the tables at a given position and CE held for a given time after the command's
access, both varied. Of about 1800 write moments per machine (gap 0, five screens):

| machine | best position: HR falls at | wrong there | with HR at 181 | with HR at 259 (78 later) |
|---|---|---|---|---|
| FS-A1GT | 181 | 65 | 65 | 573 |
| HB-F700D | 183 | 132 | 144 | 541 |
| NMS 8250 (V9958) | 177 | 77 | 186 | 634 |

The position and the CE time trade off a little (their sum stays within a few cycles), so
each machine's position is known to about one Z80 cycle. The best CE times are 82 to 84
VDP cycles after the access (`../command-end`).

**R#18 and the command engine:** the patterns with R#18 = 08h and 07h are those of R#18 =
00h, relative to HR. On the FS-A1GT and the NMS 8250 14 to 18 write moments differ at no
shift and 76 or more at a shift of one write moment; on the HB-F700D both are one write
moment earlier (23 differ there, 48 at no shift), the same for both values. If the slots
stayed in place while HR moved, the patterns would move about 5 write moments, in opposite
directions for 08h and 07h.

## Independent data

**Wouter's 2026 captures** (https://github.com/m9710797/vdp-timing-measurements,
`part2/5.slots/FINDINGS-ADJUST-S10.md`): with R#18 = 00h, 01h, 07h, 08h and 0Fh the
slots stay where they are relative to the refresh, except the accesses in the horizontal
blanking, which rearrange 4 cycles per step without changing the total. Together with the
tests above: HR, the refresh and the slots keep their places relative to each other, and
R#18 moves them together.

**igmar, Philips VG-8235 (V9958)**, test M04 of his test ROM (PR #2299; the ROM, its
sources and his measurements are attached there). The Z80 runs locked to the VDP (228 Z80
cycles a line) from RAM with interrupts off. At every phase of line 50, 2 Z80 cycles
apart, M04 finds the shortest delay N (in Z80 cycles) from setting a VRAM read address to
an `IN` that sees the new byte: the VDP's read-ahead, a CPU access in a slot. Step 1 is
GRAPHIC4, step 2 GRAPHIC4 with 32 sprites set up. His chip gives step 1 the checksum 1F1A in
one power-up and 35C6 in another (he reads these as two classes of the VDP's 4-cycle phase
against the Z80), and step 2's longest delay is 7 Z80 cycles at phase 196 (run 12).

Simulated with the slot model at the position found here (HR at 181), M04 step 1 gives
1F1A, his chip's value in the first power-up, and step 2's longest delay 7 Z80 cycles at
phase 196, as in run 12. With HR at 259 step 1 gives D6BD, which none of his runs gave,
and step 2's longest delay comes at 194. That's a third machine, another program and
another kind of access (a read), at the same position.

## Conclusions for the hardware

1. **The slots lie at one fixed place relative to HR**, the same for the CPU and the
   command engine: one access grid. With R#18 = 00h, **HR falls at about cycle 181 of the
   slot tables' line in the graphics modes** (174 to 188 on the three machines), and
   about 31 cycles later in the text modes, as HR itself does. In other words, the
   analysers' cycle 0 (HSYNC active) is about 181 VDP cycles before HR falls.
   - *Fitted, not derived:* the position comes from fitting the slot model to the
     results, through the request delay of section 8.2. Two tests with different accesses
     (CPU writes, a command) and igmar's reads land on the same position.
2. **R#18 moves the slots together with HR** (and with the refresh, by Wouter's captures):
   relative to HR they stay put. Only the accesses in the horizontal blanking rearrange.
3. *Interpretation, not measured:* the status flags, the refresh and the access slots all
   come from one horizontal timing, which R#18 shifts as a whole relative to the sync.
   IKA9958 (`../../vdp-status-race/flag-positions.md`) fits that as far as it goes: its
   decoder counter, which R#18 offsets, gives the flag positions, and its sync comes from
   the counter before the offset. But it has no memory interface, and its author marks
   its HSYNC output as not verified, so it gives no independent value for the 181.

## Files

- `vdpcmd.asm`, `vdpcmd.bas`: the command test (assemble and run as
  `../../vdp-status-race/README.md` describes). `summary.py DATA/<machine>.txt`: the
  majority outcome per write moment, 60 per line.
- The CPU test and `fit.py` are in `../cpu-write-pairs`.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-vram-timing/command-slots`, one file per machine: per screen and gap a header line
  (`S=` the screen: G sprites on, H sprites off, D display off, R and Q sprites on with
  R#18 = 08h and 07h; `G=` the gap; R#8 and R#18 as read back after the run), then per
  write moment the number of trials per outcome: 0 to 4 = the first of the five reads
  that saw CE = 0, 5 = none did.
