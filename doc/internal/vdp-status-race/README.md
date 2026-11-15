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

## The interrupt lines and the picture follow the timing

**Files.**

- `irqtest.asm`, `irqtest.bas`: counts interrupts while the main loop
  polls S#0 (turboR only). Result: `results/fs-a1gt-irq.txt`.
- `vdpirq.asm`, `autoexec-vdpirq.bas`: the `vdprace` trial for F with the
  interrupt enabled: when the interrupt comes. Results:
  `results/<machine>-irq-{text,screen5}.txt`.
- `vdpirqh.asm`, `autoexec-vdpirqh.bas`, `vdpirqh2.bas`: the same for FH
  and the line interrupt. Results: `results/<machine>-irqh-{text,screen5}.txt`,
  `results/hb-f700d-irqh2-{text,screen5}.txt`.
- `vdpline1.asm`, `vdpline.asm`, `vdpline.bas`, `vdpline2.bas`: a backdrop
  colour stripe written at a fixed moment relative to HR, with markers,
  for photos (`vdpline1.asm` writes `VDPLIN1.BIN`). Photos:
  `photos/<machine>-vdpline*.jpg`.
- `vdpfh.asm`, `vdpfh.bas`: the same stripe, timed with the line
  interrupt. Photos: `photos/<machine>-vdpfh-screen5.jpg`.

### irqtest: interrupts while polling S#0

**How.** The main loop polls S#0 with interrupts enabled; an IM2 handler
counts the interrupts and whether its own read of S#0 sees F; the E6 timer
of the turboR (one tick per 14 Z80 cycles) gives the number of frames.

**Results**, FS-A1GT, 614.4 frames: 578 interrupts, in 468 of which the
handler saw F and in 110 found it already cleared; the main loop saw F 107
times; 36 frames had no interrupt. So an interrupt can come after a read
that has seen and cleared F (110 times here).

### vdpirq: when the interrupt comes

**How.** `vdpirq.asm` is the `vdprace` trial for F (MODE 0) with the
interrupt enabled during the read A1: IE0, `EI`, interrupt mode 2 with a
handler that reads S#0 and records that it ran and whether it saw F.
Outcomes per e:

- before: A1 doesn't see F, the handler does (F set later, or the
  interrupt came before A1);
- lost: neither sees F;
- seen: A1 sees F and no interrupt follows;
- pulse: A1 sees F and the interrupt still comes (the handler finds F
  cleared).

`autoexec-vdpirq.bas`: text mode and SCREEN 5, 32 repetitions. The result
files have the columns e, lost, seen, before, pulse.

**Results**, the e values of the outcomes from the window on:

| | lost | then | before again from |
|---|------|------|-------------------|
| FS-A1GT, SCREEN 5 | 92-94 | pulse 95-107 | 108 |
| FS-A1GT, text | 87-89 | pulse 90-101 | 102 |
| HB-F700D, SCREEN 5 | 97-101 | pulse 102-111 | 113 |
| HB-F700D, text | 92-96 | pulse 97-106 | 107 |

On both machines, a read right after the window always sees F and the
interrupt still follows; the range ends where the interrupt comes before
A1, 6 Z80 cycles (FS-A1GT) or 4 to 5 (HB-F700D) later than where a read
would clear an interrupt that follows the flag at once. Fitted: the
interrupt line follows F by 33 VDP cycles (V9958) and 24 (V9938).

### vdpirqh: when the line interrupt comes

**How.** `vdpirqh.asm` with `autoexec-vdpirqh.bas`: `vdpirq` for FH
instead of F (MODE 1, line 191, IE1 on and IE0 off, the handler reads
S#1). Outcomes as in `vdpirq`. TEXT2 and SCREEN 5, 32 repetitions.
`vdpirqh2.bas` runs it again on the HB-F700D: TEXT2 with 64 repetitions
and SCREEN 5 with 32.

**Results**, the e values of the outcomes from the window on:

| | lost | pulse | before again from |
|---|---|---|---|
| HB-F700D, SCREEN 5 | 47-51 | 52-62 | 63 |
| HB-F700D, SCREEN 5, `vdpirqh2` | 48-52 | 53-63 | 64 |
| HB-F700D, text | 38-42 | 43-51 | 53 |
| HB-F700D, text, `vdpirqh2` (64 repetitions) | 38-41 | 42-52 | 53 |
| FS-A1GT, SCREEN 5 | 42-45 | 46-57 | 58 |
| FS-A1GT, text | 32-35 | 36-47 | 48 |

The end of the "pulse" range is where the interrupt comes before the read.
On the FS-A1GT, whose clock drifts, the change from pulse to before falls
inside one Z80 cycle (a mix at e = 58 and 48). On the HB-F700D the pulse
range ends 13 to 14 Z80 cycles after the start of the lost window in TEXT2
and 15 in SCREEN 5, in both runs; between the runs everything moved by up
to 1 Z80 cycle.

Fitted: the ends of the ranges are reproduced with an interrupt line that
follows FH by 21 to 24 VDP cycles on the V9958 and 19 to 20 on the V9938
in SCREEN 5, a little earlier than F's (33 and 24); 33 / 24 puts the end 1
to 2 Z80 cycles too late. On the HB-F700D in text mode the range ends 1
Z80 cycle earlier than with any of these values (in both runs); a delay
below 19 removes the pulse range there.

### vdpline: a write relative to HR, in the picture

**How.** Each frame: sync on VR and measure the phase with 48 HR samples
(as in `vdprace`), then wait a fixed time plus the measured phase, so that
what follows starts at the same moment relative to HR's falling edge every
frame, to within one Z80 cycle. Then, for 64 lines, in a loop of exactly
228 Z80 cycles per line: write COLB to R#7 (the backdrop: the border, and
colour 0 pixels in SCREEN 5 or the background in text mode), and 114 cycles
later COLA. That gives a stripe whose edges show where the two writes land
in the line (a loop of the wrong length would make them slant). Above and
below the stripe the program draws markers in the picture itself, at the
columns where the stripe would start and end for a given timing model; the
VDP draws them at its own pixel positions, so a photo shows where the
stripe lies relative to each model, in the VDP's own pixel grid. The
models put HR where the CPU tests measure it, the display area at a fixed
place in the line, and differ in the delay between that and the picture:

- `vdpline1.asm` with `vdpline.bas`: SCREEN 5 (red marker = 33 VDP cycles
  of delay, green = none), SCREEN 0 WIDTH 80 (one marker: no delay).
- `vdpline.asm` with `vdpline2.bas` (started by `vdpline.bas`): SCREEN 0
  WIDTH 80 and 40, solid marker = no delay, dotted = 33 VDP cycles.

**Results** (the FS-A1GT's SCREEN 5 screen was read on the screen, not
photographed):

| Screen | FS-A1GT | HB-F700D |
|--------|---------|----------|
| SCREEN 5 | on the red marker, at most 1 pixel to the right | 2-3 pixels right of the red marker |
| WIDTH 80, one marker | about 15 text pixels left of it | about 14 left |
| WIDTH 80, two markers | on the dotted marker | about 3 text pixels right of the dotted marker |
| WIDTH 40, two markers | on the dotted marker | about 1.5 pixels right of the dotted marker |

So relative to HR the stripe lies where the model with 33 VDP cycles of
delay puts it on the V9958, and 21 to 27 on the V9938. These photos fix
only the sum of the display area's place and that delay; the delay of the
interrupt line is measured on its own (`vdpirq`).

### vdpfh: a write relative to the line interrupt, in the picture

**How.** `vdpfh.asm`: only the line interrupt is enabled (IE1, line 100);
the CPU waits with `HALT` in interrupt mode 2 with its own vector table (no
BIOS code involved), and the handler writes the `vdpline` stripe after a
fixed delay, in SCREEN 5. A `HALT` cycle is 5 Z80 cycles: on the HB-F700D
(locked clocks) the edge stays sharp, on the FS-A1GT it wobbles by a few
pixels. Markers: red = the picture and the interrupt line without delay,
green = the picture 33 VDP cycles later and the interrupt line without
delay.

**Results:** on both machines on the red markers (the FS-A1GT wobbling
around them), at display x 55 to 222 on the HB-F700D and 57 to 227 on the
FS-A1GT: relative to the line interrupt, a write lands where the model
without any delay puts it, so the interrupt line of FH and the picture are
delayed by about the same.

### Conclusion

**The interrupt lines** follow the flags with a delay:

- F (`vdpirq`, `irqtest`, measured): the interrupt still comes after a
  read that saw and cleared F, for reads up to 33 VDP cycles (V9958) or
  about 24 (V9938) after F (fitted); on the FS-A1GT, while polling, 110 of
  578 interrupts found F already cleared.
- FH (`vdpirqh`, fitted): 21 to 24 VDP cycles (V9958) and 19 to 20 (V9938)
  after FH, a little earlier than F's. On the HB-F700D the pulse range ends
  1 Z80 cycle earlier in text mode than that gives (two runs).

**Where a write lands in the picture.**

- Relative to HR (`vdpline`, fitted to the markers, about one pixel): a
  backdrop write made t VDP cycles after HR falls changes the picture from
  pixel x on, with x = (t - 32) / 4 in the graphics modes and x = (t - 37)
  / 2 in 80-column text mode (/ 4 in 40-column; t from HR's fall in the
  text mode) on the V9958. On the V9938 the stripe lies 2 to 3 pixels
  (SCREEN 5) and 3 text pixels (80 columns; 1.5 in 40 columns) further
  right: x = (t - 20 to - 24) / 4 and x = (t - 31) / 2.
- Relative to the line interrupt (`vdpfh`): where a model with the same
  delay for the interrupt line of FH and for the picture puts it: on both
  machines the picture and that interrupt line are delayed alike.

## HR and VR in S#2

**Files.**

- `vdpedge.asm`, `vdpedge.bas`, `autoexec-vdpedge.bas`: where VR and HR
  change. Results: `results/<machine>-edge-m<mode>-s<screen>.txt`.
- `autoexec-screen5.bas`: `vdprace` in SCREEN 5 (and `vdpedge` mode 4, see
  "Two quick VRAM writes"). Results:
  `results/<machine>-{f,fh,vr}-screen5.txt`.
- `vdpmode.asm`, `autoexec-vdpmode.bas` to `autoexec-vdpmode4.bas`:
  `vdprace` with a screen mode switch between the phase measurement and
  the read, and with R#18 and R#25 set. Results:
  `results/<machine>-mode-<mode>-<A><B>[-r18-<adjust>].txt` and
  `results/fs-a1gt-mode-<mode>-{s1,s2,s3,s4,s6,s7,s8,s12,text40}.txt`.

### vdpedge: the edges of VR and HR

**How.** The trial of `vdprace`, with at the end:

| Mode | What it does at e | Columns in the result file |
|------|-------------------|----------------------------|
| 0 | reads S#2 once: VR rising at the end of the display | -, bit 1, bit 0 |
| 1 | the same one frame later: VR falling | -, bit 1, bit 0 |
| 2 | reads S#2 once: HR, both edges | -, bit 1, bit 0 |

`vdpedge.bas` asks for the mode, the screen (0 = SCREEN 0 WIDTH 80, 5 =
SCREEN 5) and `W`, and saves `V6M<mode>S<screen>W<W>.TXT`. Parameters: `C` =
1075 (mode 0, text), 1227 (mode 0, SCREEN 5), 1987 or 1599 (mode 1, 50 or
60 Hz), 100 (mode 2); `H` = 21 (text), 20 (SCREEN 5). F, FH and VR in
SCREEN 5 come from `autoexec-screen5.bas` (`vdprace`, `C` = 1227 for F and
VR, 1075 for FH, `H` = 20).

**Results**, from the AUTOEXEC runs (HR falls at 76|77 and 304|305: the
reference):

| What | FS-A1GT | HB-F700D |
|------|---------|----------|
| F lost, text | 205\|206 - 209\|210 | 207\|208 - 212\|213 |
| FH lost, text | 151\|152 - 155\|156 | 152\|155 - 158\|160 |
| F lost, SCREEN 5 | 211\|212 - 214\|215 | 213\|214 - 217\|218 |
| FH lost, SCREEN 5 | 161\|162 - 165\|166 | 163\|164 - 168\|169 |
| VR rises (`vdprace`), text | 208\|209 | 210\|212 |
| VR rises (`vdprace`), SCREEN 5 | 213\|214 | 215\|217 |
| VR rises (`vdpedge` 0), text | 225\|226 | 225\|226 |
| VR rises (`vdpedge` 0), SCREEN 5 | 230\|231 | 230\|233 |
| VR falls (`vdpedge` 1), text | 236\|237 | 224\|225 |
| VR falls (`vdpedge` 1), SCREEN 5 | 242\|243 | 229\|231 |
| HR rises (`vdpedge` 2), text | 238\|239 | 237\|238 |
| HR rises (`vdpedge` 2), SCREEN 5 | 253\|254 | 252\|253 |

### vdpmode: a screen mode switch, all screen modes

**How.** `vdpmode.asm` is `vdprace` with two screen modes per trial: mode A
(set at the start of the trial: the sync on VR and the 48 HR samples) and
mode B (set right after the samples: the read). T is TEXT2 (SCREEN 0 WIDTH
80), G is GRAPHIC4 (SCREEN 5). Sprites are off. MODE 0 (F), 1 (FH, line
191), 2 (VR) or 3 (HR: only A1 counts, "seen" is HR=1). If R18 isn't FFh,
R#18 is set to it for the whole test (07h moves the picture 7 pixels to the
left, 08h 8 to the right), and likewise R25 (08h: YJK). Every e is relative
to HR's falling edge in mode A.

| AUTOEXEC | Runs (32 repetitions, `C` = 1075 or 100 for MODE 3, `H` = 21, unless noted) | Machines |
|----------|------------------|----------|
| `autoexec-vdpmode.bas` | MODE 0-3 with A/B = T/T, T/G, G/G, G/T; MODE 0, 2, 3 with T/T and G/G and R#18 = 07h, 08h (16 repetitions, `H` = 20 for G/G with 07h) | both |
| `autoexec-vdpmode2.bas` | MODE 1 with T/T and G/G and R#18 = 07h, 08h; MODE 3 T/T 07h again | both |
| `autoexec-vdpmode3.bas` | MODE 0-3 in SCREEN 8 and SCREEN 12 (SCREEN 8 with YJK); MODE 0 and 2 in SCREEN 5 with R#18 = 04h, 0Ch | FS-A1GT |
| `autoexec-vdpmode4.bas` | MODE 0, 1, 3 in SCREEN 0 WIDTH 40 and SCREEN 1, 2, 3, 4, 6, 7 (16 repetitions) | FS-A1GT |

**Results, mode switch** (e):

| FS-A1GT | T/T | T/G | G/G | G/T |
|---------|-----|-----|-----|-----|
| F lost, from | 109\|110 | 109\|110 | 115\|116 | 115\|116 |
| VR rises | 112\|113 | 112\|113 | 117\|118 | 117\|118 |
| HR rises | 124\|125 | 134\|135 | 140\|141 | 130\|131 |
| HR falls | 191\|192 | 186\|187 | 191\|192 | 196\|197 |

| HB-F700D | T/T | T/G | G/G | G/T |
|----------|-----|-----|-----|-----|
| F lost, from | 115\|118 | 115\|116 | 120\|121 | 120\|121 |
| VR rises | 115\|120 | 116\|122 | 121\|126 | 124\|125 |
| HR rises | 131\|132 | 141\|142 | 146\|147 | 136\|137 |
| HR falls | 198\|199 | 193\|194 | 198\|199 | 203\|204 |

F and VR depend on mode A only (T/G equals T/T, G/T equals G/G); HR
depends on mode B: in GRAPHIC4 HR rises 10 Z80 cycles later and falls 5
earlier than in TEXT2, relative to F.

**Results, more screen modes** (FS-A1GT): SCREEN 1, 2, 3, 4, 6, 7 and 8
give the same edges as SCREEN 5 (F lost from 114|115 or 115|116, HR
140|141 / 191|192), SCREEN 0 WIDTH 40 the same as WIDTH 80.

### Conclusion

- Measured (`vdprace`, `vdpmode`): F and VR don't depend on the screen
  mode; HR does. Relative to F, HR rises 10 Z80 cycles later and falls 5
  earlier in GRAPHIC4 than in TEXT2. The end of the F window comes 5.9 VDP
  cycles (FS-A1GT) and 4.4 (NMS 8250 with V9958) after the first read that
  sees VR rise, about 10 ± 3 on the HB-F700D. SCREEN 1 to 8 have the edges
  of SCREEN 5, SCREEN 0 WIDTH 40 those of WIDTH 80.
- Fitted (`vdprace`, `vdpedge`, `vdpmode`): in the graphics modes HR=1
  from -312 to 0 (312 VDP cycles); in the text modes HR=1 from -367 to +31
  (398 VDP cycles); VR changes (both edges) at -48 in all modes.
  Exception: VR's falling edge in text mode fits 3 to 6 VDP cycles
  earlier.
