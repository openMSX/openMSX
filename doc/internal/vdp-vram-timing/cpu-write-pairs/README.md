# Two CPU writes to VRAM in quick succession: which byte lands where

## Background

The CPU doesn't write VRAM itself. An `OUT` to port 98h hands the VDP a byte and a
*request*; the VDP's arbiter then gives that request one of the access slots of the line,
the moments at which the VDP's memory sequencer lets the CPU at the VRAM. The slots are at
fixed places in each line, the same every line of a screen mode, between the VDP's own
accesses for the display and the refresh. The logic-analyser captures
(`../2026-measurement-analysis.md`, sections 2 and 8.2) give the slots and the request
rule exactly: a request gets the first slot at least 16 memory cycles after it reaches the
arbiter, and there is room for only one waiting request. A request that comes while
another one still waits is lost: the VDP does only one access.

Most of the time the CPU is slow enough that each write gets its own access well before
the next `OUT`. But a program that writes VRAM as fast as it can (`OTIR`, unrolled `OUT`s)
can send a second write while the first one still waits for its slot, depending on where
in the line it is. This test looks at exactly those moments.

## The question

When the CPU writes to VRAM faster than the VDP serves it, which byte ends up at which
address? The logic-analyser captures (`../2026-measurement-analysis.md`, section 8.2) show
which access slot a CPU write gets, but not which byte it writes.

## The test

`vdpvram.asm` with `vdpvram.bas`. Each trial:

1. wait for the start of the display (VR's falling edge in S#2);
2. measure where in the line the program is, to one Z80 cycle (HR, S#2 bit 5, read once in
   each of 48 lines, each read one Z80 cycle later in its line than the one before; the
   number of reads before HR falls gives the phase);
3. wait a chosen number of Z80 cycles, then write AAh and 55h to VRAM 7000h and 7001h
   (cleared to 00h before) with two `OUT`s, the second 14, 19, 21, 24, 28, 35 or 42 Z80
   cycles after the first (*gap* 0 to 6);
4. read 7000h to 7002h back and count the outcome at the *write moment*: the first `OUT`,
   in Z80 cycles from HR's falling edge plus a constant of the program.

12 trials per write moment; 368 write moments one Z80 cycle apart, a little more than one
line (228 Z80 cycles). Run on a Panasonic FS-A1GT (V9958; its S1990 adds a wait to VDP I/O),
a Sony HB-F700D (V9938) and a Philips NMS 8250 with a V9958 (no extra wait); in text 80
and 40 columns, SCREEN 1, SCREEN 5 (sprites on, sprites off, display off) and SCREEN 8.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The VDP serves CPU accesses in fixed slots in each line; a request gets the first slot at least 16 memory cycles ahead; a request that comes while another is still waiting is lost | logic-analyser captures, sections 2 and 8.2 | measured; exact on the 2013 captures (V9938) |
| The same holds for the V9958 | not measured with the analyser | assumed; the results below look the same on both chips |
| The write moments are one Z80 cycle apart and fixed relative to HR | the program's cycle counts; HR's phase measured per trial | to one Z80 cycle (6 VDP cycles), the test's resolution |

The conclusions below don't depend on where exactly the slots are relative to HR (that is a
separate question): they use how the outcomes change *within* a window.

## The results

Outcomes, as read back at 7000h, 7001h, 7002h:

| outcome | bytes | meaning |
|---|---|---|
| 0 | AAh, 55h, 00h | both writes as written |
| 1 | 55h, 00h, 00h | one byte written: the second |
| 2 | 55h, 55h, 00h | both addresses written, both with the second byte |
| 3, 4, 5 | AAh, AAh, 00h; 00h, 55h, 00h; anything else | |

Summary, at a gap of 14 Z80 cycles (from a gap of 24 on, every trial gives outcome 0 on all
three machines):

- The write moments that go wrong come in short windows, which repeat with the slot pattern
  of the screen mode: every 16 Z80 cycles in the text modes, every 21 or 22 in SCREEN 1 and
  SCREEN 5 with sprites. With the display off, with sprites off and in SCREEN 8, where the
  slots are close together, at most 2 write moments go wrong.
- Within each window, as the first write comes later: first outcome 1 (3 to 5 write
  moments), then outcome 2, then outcome 0 again. For example the FS-A1GT in text 80: write
  moments 35 to 38 give outcome 1, 39 to 42 outcome 2, and so on every 16.
- How many write moments outcome 2 lasts per window:

  | machine | text modes | SCREEN 1 and 5 |
  |---|---|---|
  | FS-A1GT (V9958) | 3-4 | 2-3 |
  | NMS 8250 (V9958) | 3 | 2-3 |
  | HB-F700D (V9938) | 1 | 1-2 |

- Right at the end of a window, in some trials: (00h, 55h, 00h) on the FS-A1GT; (00h, 00h,
  00h) and (AAh, AAh, 00h) on the HB-F700D, and on the NMS 8250 in a minority of the trials,
  where also (AAh, 00h, 00h) comes up a few times per run (4 to 12 of the 3840 trials).
  Rarely a byte with only some of the bits of AAh or 55h.

## Conclusions for the hardware

1. **Outcome 1 is the second request lost, and its byte still written.** Only one access
   happens, as the request rule says for a request that comes while the first one is
   waiting; but that access writes 55h, the second byte. So there is one data register,
   and every `OUT` writes it, also when its request is lost.
2. **The byte reaches the data register before the request reaches the arbiter.** In
   outcome 2 the second request comes late enough to get its own access, yet the first
   access writes 55h too. That can only happen if the second byte was in the data register
   before the first access while its request came after it. So the byte is taken earlier
   than the request is seen, and an access writes what the data register holds at that
   moment.
3. **How much earlier.** Outcome 2 happens when the second request comes after the first
   access (from about 1 VDP cycle after it, by the request rule) while its byte came before
   it. So outcome 2 lasts about as long as the byte's lead over the request, wherever the
   slots are. With write moments 6 VDP cycles apart, 3 to 4 write moments of outcome 2
   give about 13 to 25 VDP cycles. A fit of the whole slot model to every write moment
   (`fit.py`) narrows it to about 18: on both V9958s the number of wrong write moments has
   a clear minimum there (`fit.py --k`: FS-A1GT 76 to 100 for 16 to 22, against 168 at 12
   and 205 at 26; NMS 8250 57 to 68 for 16 to 20, against 146 at 12 and 211 at 26). With
   the request seen 10.9 VDP cycles after /CSW rises (section 8.2), the byte is in the data
   register a few cycles before /CSW rises.
   - *This is a model that fits, not something seen on the VDP's pins.*
   - **The HB-F700D (V9938) doesn't pin it down.** Its windows are as long as the V9958s',
     but outcome 2 lasts only their first write moment; the next ones give (00h, 00h, 00h)
     and then (AAh, AAh, 00h) (text 80, gap 14: outcome 1 at write moments 40 to 43, outcome
     2 at 44, (00h, 00h, 00h) at 45, (AAh, AAh, 00h) at 46, outcome 0 from 48; the NMS 8250
     has outcome 2 at 44 to 47). The model has no outcome for those, so every lead from 8
     to 20 fits the HB-F700D about equally (`fit.py --k`: 230 to 236 wrong).
   - That is with the HB-F700D's own best slot position, which `fit.py --k` allows each
     machine. At the V9958s' position the HB-F700D prefers 8 (`fit.py --fixed`, text 80:
     137 wrong with 8, 187 with 18). But its own best position is 4 to 7 VDP cycles later in
     all four screen modes, for either lead, and there 18 fits as well as 8 (text 80: 88
     against 86). So what sets the HB-F700D apart is that its requests reach the arbiter
     about one Z80 cycle later relative to HR, not the lead. Whether that is the chip or the
     machine's I/O timing, these tests can't tell. The same lead of 18 is taken for both
     chips.
4. **Not explained:** the outcomes right at the end of a window, where the second `OUT` comes
   about when the first access happens: (00h, 55h, 00h) on the FS-A1GT; (00h, 00h, 00h) and
   (AAh, AAh, 00h) on the HB-F700D, over the last write moments of the zone where the
   V9958s give outcome 2; on the NMS 8250 both of those in some trials, and partial bytes
   such as (08h, 08h, 00h) and (54h, 54h, 00h). The partial bytes are a byte taken into the
   data register while the data bus changes, and (AAh, AAh, 00h) is the second access
   happening before its byte is there. So at the end of a window the CPU's byte and the
   access collide, and how that comes out depends on when the byte is on the VDP's pins,
   which can differ per machine (its I/O timing) as well as per chip. With one V9938 machine
   these tests can't tell which. A V9938 and a V9958 in the same machine would.

## Files

- `vdpvram.asm`, `vdpvram.bas`: the test. `vdpvram2.bas`: the same program, SCREEN 5 with
  sprites on and R#18 = 08h, gap 14 only (for the slot position, a separate question).
- `summary.py`: lists the write moments that went wrong, per screen and gap, from a result
  file.
- `fit.py`: the slot model of section 8.2, simulated over every write moment, with the data
  register's lead.

The full results are in https://github.com/MBilderbeek/vdp-measurements, folder
`vdp-vram-timing/cpu-write-pairs`, one file per machine: per screen and gap a header line
(`S=` the screen: T text 80, U text 40, C SCREEN 1, G SCREEN 5, H sprites off, D display
off, Y SCREEN 8, R R#18 = 08h, P R#18 = 08h with sprites on; `G=` the gap), then per write
moment the number of trials per outcome (`35 0=1 1=3`: write moment 35, one trial outcome
0, three outcome 1), runs with only outcome 0 as `first-last 0=trials`, and all byte
triples read back. (The runs marked Y and R had sprites off by mistake: the program sets
R#8 bit 1 for the sprites-off screen and never clears it, and BASIC's `SCREEN` keeps it.)
