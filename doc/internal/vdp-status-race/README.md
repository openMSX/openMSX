# V99x8: status flags, interrupts and picture timing

Test programs and measurements on real machines of the V99x8's timing:
its status flags, its interrupts, and where in the picture a write takes
effect (see issue #2244). Each section below is one finding: how it was
measured, the results, and the conclusion about the chip.

## Files

Each section lists its own programs, result files and photos. The result
files of the real machines are in `results/`, the photos in `photos/`.

Assemble with sjasmplus (`sjasmplus vdprace.asm` writes `VDPRACE.BIN`).
Put the `.BIN` and `.BAS` files on a disk (the `.bas` files need CR LF
line ends, see `.gitattributes`; an `autoexec-*.bas` goes on the disk as
`AUTOEXEC.BAS`) and run the `.BAS` file from MSX BASIC. On a turboR the
programs switch to Z80 mode themselves.

## Test machines

| Machine | VDP | Extra VDP I/O wait | CPU clock vs VDP clock / 6 |
|---------|-----|--------------------|-----------------------------|
| Panasonic FS-A1GT | V9958 | yes (S1990) | +3 ppm |
| Philips NMS 8250, modified: a V9958 instead of its V9938 | V9958 | no | +116 ppm |
| Sony HB-F700D | V9938 | no | 0 (locked) |

The clock ratio comes from `clktest` (`clktest.asm`, `clktest.bas`;
results `results/<machine>-clock.txt`): two HR phase measurements (64
samples each) exactly 313 lines (71364 CPU cycles) apart; each cycle of
difference is 14 ppm. FS-A1GT: 0.19 cycles, NMS 8250 (V9958): 8.27,
HB-F700D: 0.

## Timing reference

A line is 1368 VDP cycles (21.48 MHz; one Z80 cycle is 6 VDP cycles, one
pixel 4, in 80-column text mode 2). Moments in a line are given relative to
the falling edge of HR in the graphics modes with R#18 = 00h, which the
CPU tests measure directly; a CPU access counts at the moment the VDP takes
it, as the emulation used for the fits (below) times it.

The CPU tests give e values: moments relative to an HR edge plus a constant
of the program. Converting them into VDP cycles needs the exact cycle count
of each program, which comes from running the same programs in an
emulation with exact CPU timing; values found that way are marked
"fitted" and come with the accuracy of the fit. Unless stated otherwise,
a fitted value reproduces the tests named with it on both machines to
within one Z80 cycle (6 VDP cycles) per edge. The CPU tests resolve one
Z80 cycle, the photos about one pixel (4 VDP cycles in SCREEN 5, 2 in
80-column text mode).

## Method

`vdprace` and the programs based on it do one trial per frame, with
interrupts disabled unless stated otherwise:

1. Sync on the falling edge of VR (S#2 bit 6). The polling loop leaves a
   jitter of about 30 Z80 cycles.
2. Measure that jitter: sample HR (S#2 bit 5) 48 times, one sample per
   line, each 229 cycles after the previous one (a line is 228 cycles), so
   each sample is one cycle later in the line. CNT, the number of samples
   before HR falls, gives the phase of this trial to within one Z80 cycle.
   The padding of that loop depends on whether VDP I/O has an extra wait
   cycle (`W`).
3. Wait a fixed time plus d cycles (d = 0..319, one-cycle steps), read the
   register once (A1), and again about 3600 cycles later (A2).
4. Count the outcome at e = d + 48 - CNT: the moment of the read in Z80
   cycles relative to HR's falling edge, plus a constant. Outcomes for a
   flag: seen (A1 has it), before (A1 doesn't, A2 does), lost (neither).
   For a bit that isn't cleared by the read (VR, HR): its value in A1.

Trials with CNT = 0 or 48 (HR's edge not among the samples) are counted as
invalid (0 to 12% per run here) and left out; they don't bias the other e
values, as d is chosen independently of the phase.

Result files list e followed by the counts per outcome. Neighbouring e
values are one Z80 cycle (6 VDP cycles) apart, so differences between e
values are given in Z80 cycles below. An edge is given as
the two neighbouring e values with trials between which the majority
outcome changes ("207|210": after 207, at or before 210). On a machine with
locked clocks (the HB-F700D) each e is one fixed moment and only some e
values get trials; on a drifting machine (the FS-A1GT) the moment within
one e varies over a whole Z80 cycle, so at an edge an e can have mixed
outcomes. e values of different machines can't be compared directly
(different VDP I/O timing); the same program on an emulation of the same
machine can be compared with them e by e, with `compare.py`:

    python3 compare.py results/hb-f700d-f.txt <the emulator's T4M0W0.TXT>

It prints both files as one character per e and lists the e values where
the majority outcome differs.

## A read that loses F or FH

**Files.** `vdprace.asm`, `vdprace.bas`: reads S#0, S#1 or S#2 at a known
moment around the setting of F, FH or VR. `autoexec.bas` (`clktest` and
`vdprace`) and `autoexec-text.bas` (`vdprace`) run them in text mode
without any input. `slottest.asm`, `slottest.bas`: `vdprace` with the
display switched off, and FH in a border line. Results:
`results/<machine>-{f,fh,vr}.txt` (columns e, lost, seen, before) and
`results/fs-a1gt-slot-*.txt`.

**How.** `vdprace` MODE 0 (F), 1 (FH: IE1 on, R#19 = 191, interrupts off
in the CPU) and 2 (VR) in SCREEN 0 WIDTH 80, 32 repetitions, `C` = 1075,
`H` = 21. The NMS 8250 (V9958)'s CPU runs 116 ppm fast; between the HR
samples and the read there are about 38000 cycles, so its reads land
about 26 VDP cycles early; its windows below are corrected for that.
`slottest`: the same for F with the display switched off, and for FH set
in a border line (R#19 = 200), on the FS-A1GT.

**Results.** A read loses the flag when it starts inside a window of:

| Flag | Machine (VDP) | Width (VDP cycles) |
|------|---------------|--------------------|
| F | FS-A1GT (V9958) | 20.6 |
| F | NMS 8250 (V9958) | 19.9 |
| F | HB-F700D (V9938) | 27 ± 4 |
| FH | FS-A1GT (V9958) | 25 |
| FH | NMS 8250 (V9958) | 28 |
| FH | HB-F700D (V9938) | 18 to 48 (few e values with trials) |

The windows are sharp: inside them the flag is always lost, outside never.
The NMS 8250 (V9958)'s read pulse is one Z80 cycle shorter than the
FS-A1GT's (no extra wait), the window is the same.

The edges, `RUN` typed in:

| Window, text mode | Machine | Edges (e) |
|-------------------|---------|-----------|
| F lost | FS-A1GT | 205\|206 - 209\|210 |
| FH lost | FS-A1GT | 151\|152 - 155\|156 |
| F lost | HB-F700D | 207\|208 - 212\|213 |
| FH lost | HB-F700D | 152\|155 - 158\|160 |

VR relative to F: in MODE 2 the edge is the read that latches VR exactly
when it rises, in MODE 0 the end of the window the read that latches F
exactly when it is set; the difference cancels any drift or delay of the
program. F is set after VR rises by 5.9 VDP cycles (FS-A1GT), 4.4 (NMS
8250 with V9958), about 10 ± 3 (HB-F700D).

`slottest`: the F window with the display off is 3.25 Z80 cycles wide
(3.4 with the display on), the FH window at line 200 4.4 (4.2 at line
191): the windows don't depend on the access slots.

**Conclusion.** Measured: a read loses a flag when it starts within a
fixed window before the first read that sees it: 20 VDP cycles on the
V9958 for F (20.6 and 19.9 on the two machines), 25 to 28 for FH, 27 ± 4
on the V9938 for F. The window is sharp, and the same with a read pulse
one Z80 cycle shorter (NMS 8250 against FS-A1GT), with the display off and
in a border line.

*Interpretation (an assumption, not measured):* a read is two steps inside
the VDP: it latches the value the CPU gets at the start of the read, and
clears the flags the read clears a fixed time later (the window's width).
A flag set between those two moments isn't in the value and is cleared
anyway. Then the end of the window is the moment the flag is set.
