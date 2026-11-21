# V99x8: a status read that loses a flag being set

## The question

F (S#0 bit 7, set at the start of vertical blanking) and FH (S#1 bit 0, set at the line
interrupt's line, when the line interrupt is enabled) are cleared by reading their status
register. What happens when the CPU reads the register at about the moment the VDP sets
the flag? On real machines the read can *lose* the flag: it returns 0, the flag stays 0,
and there is no interrupt. Software that polls S#0 for F then misses a frame (issue #2244).
When exactly does that happen?

## The test

`vdprace.asm` with `vdprace.bas`. The method is used by every test in this folder and its
subfolders, so here it is in full. One trial per frame, interrupts disabled:

1. Sync on the falling edge of VR (S#2 bit 6). Reading S#2 clears nothing; the polling loop
   leaves a jitter of about 30 Z80 cycles.
2. Measure that jitter: read HR (S#2 bit 5) 48 times, once per line, each read 229 Z80
   cycles after the previous one (a line is 228), so each one is one Z80 cycle later in its
   line. The number of reads before HR falls gives the trial's phase relative to HR, to one
   Z80 cycle.
3. Wait a fixed time plus d Z80 cycles (d = 0 to 319), read the status register once (A1),
   and once more about 3600 Z80 cycles later (A2):
   - flag set in A1: *seen*;
   - flag clear in A1, set in A2: the read came *before* the flag was set;
   - flag clear in both: *lost*.
4. Count the outcome at e = d corrected with the measured phase: the read's moment in Z80
   cycles relative to HR's falling edge, plus a constant of the program. Neighbouring e
   are one Z80 cycle (6 VDP cycles) apart.

Modes: 0 = F in S#0; 1 = FH in S#1 (line interrupt enabled, R#19 = 191, interrupts off in
the CPU); 2 = VR in S#2 (VR can't be lost: it shows where vertical blanking starts). 32
trials per e. `autoexec.bas` (`clktest`, then the three modes, SCREEN 0 as BASIC starts) and `autoexec-text.bas` (with
`RUN` typed in, SCREEN 0 WIDTH 80) run it without input.

Also: `slottest.asm`/`.bas` (the same with the display off, and with FH in a border line,
R#19 = 200); `irqtest.asm`/`.bas` (turboR: are interrupts lost too?); `clktest.asm`/`.bas`
(the ratio of the CPU clock to the VDP clock, from two HR phases 313 lines apart).

Machines: Panasonic FS-A1GT (V9958, the S1990 adds a wait to VDP I/O; CPU and VDP clocks
locked to within 3 ppm), Philips NMS 8250 with a V9958 instead of its V9938 (no extra wait;
its CPU runs 116 ppm fast against the VDP), Sony HB-F700D (V9938, clocks locked).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The read's moment relative to HR: the program's cycle counts plus the phase measured per trial | the program; checked in openMSX, where each mode gives one sharp edge (all trials before an e "before", from the next e on "seen") | to one Z80 cycle |
| On the NMS 8250 the reads land about 26 VDP cycles early (116 ppm over the about 38000 Z80 cycles between the HR reads and the read) | `clktest` | measured; corrected for below |
| The e values of different machines can't be compared directly | they differ in VDP I/O timing (the S1990's wait) | known |
| Converting e to VDP cycles relative to the flag | the same program in openMSX, whose CPU and VDP clocks are locked: each e is one read time there | derived; on a machine whose clocks aren't locked, the fraction of lost trials in the bin at a window's edge gives the edge's position within the Z80 cycle |

## The results

Summary. A read loses the flag when it starts within a window around the moment the flag
is set (VDP cycles; before / after the moment openMSX sets the flag, which is a fixed
reference here):

| flag | machine (VDP) | before | after | width |
|---|---|---|---|---|
| F | FS-A1GT (V9958) | 6.5 | 14.1 | 20.6 |
| F | NMS 8250 (V9958) | 6.7 | 13.2 | 19.9 |
| F | HB-F700D (V9938) | 13 ± 3 | 14 ± 3 | 27 ± 4 |
| FH | FS-A1GT (V9958) | 20.3 | 5 | 25 |
| FH | NMS 8250 (V9958) | 21.2 | 6.8 | 28 |
| FH | HB-F700D (V9938) | 12 to 30 | 6 to 18 | |

(The HB-F700D's clocks are locked, so each e is one fixed read time and only some e get
trials: its edges are known to within a Z80 cycle.) Example, FS-A1GT, text mode, in e:
F is lost for reads from 205|206 to 209|210 (the edge lies between the two e), FH from
151|152 to 155|156; on the HB-F700D F from 207|208 to 212|213, FH from 152|155 to 158|160.

- The windows are sharp: inside them the flag is always lost, outside never.
- The same window with the display off (3.25 Z80 cycles against 3.4 with it on) and with FH
  set in a border line (4.4 against 4.2): `slottest`, FS-A1GT.
- The same window on the NMS 8250, whose read pulse is one Z80 cycle shorter (no extra
  wait).
- `irqtest` (FS-A1GT, 614 frames, a main loop polling S#0 with interrupts on): 36 frames (6
  %) had no interrupt at all; in about 110 frames the main loop saw F just before the
  interrupt was taken, and the interrupt still came, with F already cleared.
- F is set after VR rises by 5.9 VDP cycles (FS-A1GT), 4.4 (NMS 8250), about 10 ± 3
  (HB-F700D): the edge of mode 2 against the end of the window of mode 0, which cancels
  any drift of the program.

## Conclusions for the hardware

A model that fits all of the above: **a status read is two steps inside the VDP.**
1. At the start of the read the VDP latches the value the CPU gets.
2. A fixed time later it clears the flags the read clears: about 20 VDP cycles for F and 25
   to 28 for FH on the V9958.

A flag set between those two moments is not in the value the CPU gets, and is cleared
anyway. That explains:
- the sharp windows, and their width (the time between latch and clear);
- that the width doesn't depend on the length of the CPU's read pulse, the access slots or
  the line: a fixed internal delay;
- the lost interrupts (the interrupt line is active only between the setting and the clear,
  too short for the Z80 to see), and the interrupts that come with F already cleared (F set
  just before the read: the line is still active when the Z80 samples it at the end of the
  `IN`).

*This is a model of what the CPU sees, not a description of the chip's circuits.*

**V9938 and V9958.** Only one V9938 machine was measured (the HB-F700D), and its clocks
are locked, so its reads fall on a fixed grid of 6 VDP cycles: its window edges are known
only to within one read moment.
- **F: no difference shown.** The HB-F700D's F window (27 ± 4 cycles) is wider than the
  V9958s' (about 20) by about one read moment, at its start only, and not consistently:
  it starts one read moment earlier than the V9958's in text mode, but not in SCREEN 5.
  So one F window describes all three machines as well as two do.
- **FH: the window ends later on the HB-F700D.** In text mode and in SCREEN 5, the
  read moment right after where the V9958's FH window ends still loses FH in every trial
  (32 of 32, and 61 of 61): the window ends at least one read moment (6 VDP cycles) later,
  so the latch-to-clear time for FH is longer there.
- Whether that FH difference is the chip or the machine's read timing, one V9938 machine
  can't tell. The NMS 8250 shows that a different read pulse (one Z80 cycle shorter) on
  the same chip doesn't change the window.

Also measured: the VDP sets F a few VDP cycles after VR rises, not at the same moment.

## Files

- `vdprace.asm`/`.bas`, `slottest.asm`/`.bas`, `irqtest.asm`/`.bas`, `clktest.asm`/`.bas`:
  the tests; `autoexec.bas`, `autoexec-text.bas`: run them without input. Assemble with
  sjasmplus (`sjasmplus vdprace.asm` writes `VDPRACE.BIN`), put the `.BIN` and `.BAS` files
  on a disk (the `.bas` files need CR LF line ends, see `.gitattributes`) and `RUN` the
  `.BAS` file. On a turboR the programs switch to Z80 mode themselves.
- `compare.py`: compares a result file of a real machine with one of the same program in
  openMSX, e by e.
- The full results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/flag-race`. `<machine>-f.txt`, `-fh.txt`, `-vr.txt`: `vdprace` modes 0,
  1 and 2, a header line with the program's settings, then per e the number of trials
  lost, seen and before. `fs-a1gt-slot-*.txt`: `slottest`, the same format.
  `<machine>-clock.txt`: `clktest`. `fs-a1gt-irq.txt`: `irqtest`.

## Other measurements with this method

Each has its own document in this folder, in the same form:
- `output-delay.md`: the interrupt line and the picture lag the status flags.
