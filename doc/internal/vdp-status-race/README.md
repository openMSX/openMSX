# V99x8: reading a status flag at the moment it gets set

These are the test programs and measurements behind the `F_LOST_*` and
`FH_LOST` windows in `src/video/VDP.cc` (see issue #2244).

A read of a status register at about the moment the VDP sets a flag that the
read clears (F in S#0, and FH in S#1 when the line interrupt is enabled)
loses that flag: the read returns 0 for it, the flag stays 0, and there is no
interrupt. Software that polls S#0 for F misses frames because of this.

## Files

| File | What it is |
|------|------------|
| `vdprace.asm`, `vdprace.bas` | Measures where, relative to the moment a flag is set, a read loses it (F, FH, or VR as a reference). |
| `irqtest.asm`, `irqtest.bas` | Checks whether the interrupt is lost too (turboR only). |
| `clktest.asm`, `clktest.bas` | Measures the ratio between the CPU clock and the VDP clock. |
| `autoexec.bas` | Runs the clock test and the three `vdprace` modes without any input (used on the HB-F700D). |
| `slottest.asm`, `slottest.bas` | `vdprace` plus an option to switch the display off; runs the two tests of the last paragraph below. (In openMSX its FH and VR edges are one Z80 cycle later than those of `vdprace.asm`.) |
| `results/` | The result files of the real machines. |

Assemble with sjasmplus (`sjasmplus vdprace.asm` writes `VDPRACE.BIN`), put
the `.BIN` and `.BAS` files on a disk (the `.bas` files need CR LF line ends,
see `.gitattributes`) and `RUN` the `.BAS` file from MSX BASIC. On a turboR,
the programs switch to Z80 mode themselves.

Test machines:

| Machine | VDP | Extra VDP I/O wait | CPU clock vs VDP clock / 6 |
|---------|-----|--------------------|-----------------------------|
| Panasonic FS-A1GT | V9958 | yes (S1990) | +3 ppm (practically locked) |
| Philips NMS 8250, modified: a V9958 instead of its V9938 | V9958 | no | +116 ppm |
| Sony HB-F700D | V9938 | no | 0 (locked) |

## vdprace: how it works

One trial per frame, with interrupts disabled:

1. Sync on the falling edge of VR (S#2 bit 6). Reading S#2 clears nothing,
   but the polling loop leaves a jitter of about 30 cycles.
2. Measure that jitter: sample HR (S#2 bit 5) 48 times, one sample per line,
   each 229 cycles after the previous one (a line is 228 cycles), so each
   sample is one cycle later in the line. The number of samples before HR
   falls gives the phase of this trial to within one Z80 cycle. The padding of
   that loop depends on whether VDP I/O has an extra wait cycle (`W`).
3. Wait a fixed time plus d cycles (d = 0..319, in one-cycle steps), read the
   status register once (A1), and read it again about 3600 cycles later (A2):
   - flag set in A1: seen
   - flag clear in A1, set in A2: the read came before the flag was set
   - flag clear in both: lost
4. Count the outcome at e = d corrected with the measured phase: the time of
   the read in Z80 cycles relative to the VDP's line timing (plus a constant).

Modes: 0 = F in S#0, 1 = FH in S#1 (the program enables the line interrupt
and sets R#19 to 191; interrupts stay disabled in the CPU), 2 = VR in S#2. VR
can't be lost; it shows where the vertical blanking starts.

In openMSX (without the race) each mode gives a sharp edge: every trial below
some e is "before", every trial from the next e on is "seen". That shows that
the phase correction works.

The result files (`results/*-f.txt`, `*-fh.txt`, `*-vr.txt`) list e, lost,
seen and before. The e values of different machines can't be compared
directly: the machines differ in VDP I/O timing.

### Converting to openMSX's timing

openMSX runs each test with its CPU and VDP clocks locked, so each e
corresponds to one read time. With temporary debug output in `VDP.cc`, these
are the read times relative to the moment openMSX sets the flag:

| | e of the last read before the flag | openMSX's read times |
|---|---|---|
| F, FS-A1GT | 206 | 206: −4, 207: +2 VDP cycles |
| F, NMS 8250, HB-F700D | 209 | 209: −4, 210: +2 |
| FH, FS-A1GT | 154 | 154: −3, 155: +3 |
| FH, NMS 8250, HB-F700D | 156 | 156: −6, 157: 0, 158: +6 |

On a real machine whose clocks aren't locked, the read time within the bin of
one e varies over a whole cycle (6 VDP cycles). So the fraction of lost trials
in a bin at the edge of the window gives the position of that edge within the
cycle.

The NMS 8250's CPU runs 116 ppm fast. Between the HR samples and the read
there are about 38,000 CPU cycles, so its reads land about 26 VDP cycles
early. The windows below are corrected for that. The VR results confirm the
correction: relative to VR, the F windows of the FS-A1GT and the NMS 8250
agree to within 2 VDP cycles.

## Results

Lost for reads from 'before' VDP cycles before to 'after' VDP cycles after the
moment openMSX sets the flag:

| Flag | Machine (VDP) | Before | After | Width (VDP cycles) |
|------|---------------|--------|-------|--------------------|
| F | FS-A1GT (V9958) | 6.5 | 14.1 | 20.6 |
| F | modified NMS 8250 (V9958) | 6.7 | 13.2 | 19.9 |
| F | HB-F700D (V9938) | 13 ± 3 | 14 ± 3 | 27 |
| FH | FS-A1GT (V9958) | 20.3 | 5 | 25 |
| FH | modified NMS 8250 (V9958) | 21.2 | 6.8 | 28 |
| FH | HB-F700D (V9938) | 12..30 | 6..18 | |

The HB-F700D has locked clocks, so there each e is a fixed read time and only
some e values get trials: its edges are only known to within a cycle.

openMSX uses F: 7 before, 14 after (V9958) and 13 / 15 (V9938, also used for
the TMS99xx, which weren't measured; this method needs HR and VR, which they
don't have); FH: 21 before, 6 after. With these windows, `vdprace` in openMSX
gives the same lost positions as the real machines (to within one Z80 cycle).

The window doesn't depend on the length of the CPU's read pulse: the NMS 8250
has no extra wait cycle on VDP I/O, so its read pulse is one cycle shorter
than the FS-A1GT's, yet the window is the same.

Relative to HR, the real VR rises about 8 VDP cycles (FS-A1GT) to 14 (NMS
8250) after the moment openMSX sets VR and F.

## irqtest: the interrupt

The main loop polls S#0 with interrupts enabled, an IM2 handler counts the
interrupts, and the E6 timer of the turboR (one tick per 14 Z80 cycles) gives
the number of frames. On the FS-A1GT (`results/fs-a1gt-irq.txt`), over
614.4 frames:

- 578 interrupts, of which 468 saw F in their own read of S#0;
- the main loop saw F 107 times;
- 36 frames (6%) had no interrupt at all.

So the interrupt is lost together with the flag, about as often as expected
for this loop (a window of 3.4 out of about 65 cycles). In about 110 frames
the main loop read F just before the interrupt was accepted, and the
interrupt still came (the handler then found F cleared): the Z80 samples
/INT before the VDP drops it. openMSX doesn't emulate that: there the read
clears the interrupt at once.

## clktest: the clock ratio

Two HR phase measurements (64 samples each) exactly 313 lines (71364 CPU
cycles) apart; each cycle of difference is 14 ppm. In openMSX the difference
is 0. FS-A1GT: 0.19 cycles (3 ppm), NMS 8250: 8.27 cycles (116 ppm), HB-F700D:
0.

## A model that fits

All results so far fit this model: a read of a status register is two steps
inside the VDP.

1. **Latch:** at the start of the read, the VDP takes the value that the CPU
   gets.
2. **Clear:** a fixed time later, it clears the flags that the read clears:
   about 20 VDP cycles on the V9958 for F, 25 to 28 for FH, about 28 on the
   V9938 for F.

A flag set between those two moments isn't in the value the CPU gets, and is
cleared anyway. This explains:

- **Sharp windows:** inside it the flag is always lost, outside it never.
- **Width:** it doesn't depend on the CPU's read pulse (the NMS 8250's is one
  Z80 cycle shorter than the FS-A1GT's, the window is the same), so the clear
  isn't tied to the end of /RD. The VDP samples /CSx on its own clock (see
  section 8.2 of `../vdp-vram-timing/2026-measurement-analysis.md`), so a
  fixed internal delay gives the same window every time.
- **The interrupt:** if F was set just before the read, the interrupt line is
  still active when the Z80 samples /INT at the end of that IN instruction,
  because the clear only comes about 3.3 Z80 cycles after the read started.
  So the interrupt is accepted, and the handler finds F cleared (`irqtest`:
  about 110 times). If F is set inside the window, the line is only active
  between the setting and the clear, and the Z80 usually doesn't see it: the
  interrupt is lost too.

**A check with VR.** VR is latched by a read in the same way, so in the VR
results (mode 2) the edge is the read whose latch falls exactly when VR
rises. In the F results (mode 0), the end of the window is the read whose
latch falls exactly when F is set. Their difference is how long after VR the
VDP sets F, and any drift or delay of the test program cancels:

| Machine | F set after VR rises |
|---------|----------------------|
| FS-A1GT (V9958) | 5.9 VDP cycles |
| modified NMS 8250 (V9958) | 4.4 VDP cycles |
| HB-F700D (V9938) | about 10 ± 3 |

The two V9958 machines agree to within 1.5 VDP cycles, although one of them
drifts 116 ppm. (openMSX sets VR and F at the same moment.)

The delay doesn't depend on the VDP's access slots (see `../vdp-vram-timing/`)
at that moment: `slottest.bas` on the FS-A1GT (`results/fs-a1gt-slot-*.txt`)
gives the same F window with the display off (3.25 Z80 cycles, against 3.4
with the display on), and the same FH window when FH is set in a border line,
line 200 (4.4 against 4.2 for line 191). So it's a fixed delay per flag and
per chip.

This is a model of what is visible from the CPU, not a description of the
chip.
