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

## FH follows the screen mode in which it is set

**Files.** `vdpmode` (see "HR and VR in S#2"), MODE 1 (FH). Results:
`results/<machine>-mode-1-<A><B>.txt` and
`results/fs-a1gt-mode-1-{s1,s2,s3,s4,s6,s7,s8,text40}.txt`.

**Results, mode switch** (e; mode A for the phase measurement, mode B for
the read):

| FH lost, from | T/T | T/G | G/G | G/T |
|---------------|-----|-----|-----|-----|
| FS-A1GT | 55\|56 | 59\|60 | 65\|66 | 60\|61 |
| HB-F700D | 61\|64 | 65\|66 | 70\|71 | 66\|67 |

T/G and G/T lie between T/T and G/G: FH depends on both. Corrected for F,
which follows mode A ("HR and VR in S#2": F 5 to 6 Z80 cycles later in
G/x than in T/x), FH comes 4 to 5 Z80 cycles later relative to F when
mode B is GRAPHIC4 than when it is TEXT2, with either mode A.

**Results, more screen modes** (FS-A1GT): SCREEN 1, 2, 3, 4, 6, 7 and 8
lose FH from 64|65 or 65|66, as SCREEN 5; SCREEN 0 WIDTH 40 as WIDTH 80.

### Conclusion

- Measured (`vdpmode`): FH depends on the screen mode at the moment it is
  set: relative to F it comes 4 to 5 Z80 cycles later in GRAPHIC4 than in
  TEXT2.
- Fitted (`vdprace`, `vdpedge`, `vdpmode`): FH is set at -345 in the
  graphics modes (33 VDP cycles before HR rises) and at -373 in the text
  modes (6 before HR rises).

## F, FH, VR and HR all move with the horizontal adjust

**Files.** `vdpmode` with R#18 set (see "HR and VR in S#2"). Results:
`results/<machine>-mode-<mode>-<A><B>-r18-<adjust>.txt`
(`fs-a1gt-mode-3-tt-r18-7-2.txt` is a second run of that test; on the
HB-F700D its file couldn't be read back from the floppy, the second run
replaces it). `vdpadj.asm`, `autoexec-vdpadj.bas`: `vdpmode` with R#18
changed between the HR measurement and the read. Results:
`results/<machine>-adj-<f|vr>-<screen5|text>-r18-<b>.txt`.

### vdpmode with the horizontal adjust

**Results** (R#18 = 00h / 07h / 08h, e):

| What | FS-A1GT | HB-F700D |
|------|---------|----------|
| F lost, T/T, from | 109\|110 / 109\|110 / 109\|110 | 115\|118 / (*) / 116\|117 |
| F lost, G/G, from | 115\|116 / 115\|116 / 115\|116 | 120\|121 / 121\|122 / 120\|123 |
| VR rises, T/T | 112\|113 / 112\|113 / 112\|113 | 115\|120 / 117\|122 / 119\|120 |
| VR rises, G/G | 117\|118 / 117\|118 / 117\|118 | 121\|126 / 123\|125 / 122\|125 |
| FH lost, T/T, from | 55\|56 / 55\|56 / 55\|56 | 61\|64 / 61\|62 / 61\|62 |
| FH lost, G/G, from | 65\|66 / 65\|66 / 65\|66 | 70\|71 / 71\|73 / 71\|72 |

(*) No e with trials inside the window: e 115 is "before", e 121 "seen".
MODE 3 gives the same HR edges with every adjust (the phase comes from HR
itself). R#18 = 04h and 0Ch (FS-A1GT, SCREEN 5, F and VR): the same. So,
relative to HR, F, FH and VR stay where they are with any adjust.

### vdpadj: R#18 changed between the HR measurement and the read

**How.** `vdpadj.asm` with `autoexec-vdpadj.bas`: `vdpmode` MODE 0 (F) and
MODE 2 (VR), GRAPHIC4 and TEXT2, with R#18 = 00h during the sync and the HR
samples and R#18 = B (00h, 02h, 07h, 08h, 0Fh) from right after the samples
on; 8 repetitions. e stays relative to HR with R#18 = 00h, so the edges show
how far F and VR move with the adjust.

**Results**, the shift of the edge relative to B = 00h, in Z80 cycles
(-4 VDP cycles per adjust step would be -1.3, -4.7, +5.3 and +0.7):

| | B = 02h | 07h | 08h | 0Fh |
|---|---|---|---|---|
| FS-A1GT, VR and F, GRAPHIC4 and TEXT2 | -1 to -1.5 | -4 to -5 | +5 to +5.5 | 0 to +0.5 |
| HB-F700D, VR and F, GRAPHIC4 and TEXT2 | -1 | -4 to -4.5 | +5.5 to +6 | +1 |

### Conclusion

Measured (`vdpmode`, `vdpadj`): HR, F, FH and VR keep their positions
relative to each other with any horizontal adjust (R#18); F and VR move
with it by -4 VDP cycles per step, HR and FH along with them.

## Two quick VRAM writes

**Files.** `vdpedge` modes 3 and 4 (see "HR and VR in S#2"): mode 3 is
among the runs of `autoexec-vdpedge.bas`, mode 4 runs from
`autoexec-vdpedge4.bas` (text mode and SCREEN 5) and
`autoexec-screen5.bas` (SCREEN 5). Results:
`results/<machine>-edge-m3-s0.txt` and
`results/<machine>-edge-m4-s<screen>.txt` (`fs-a1gt-edge-m4-s5-2.txt` is
a second run, with the same result).

**How.** The trial of `vdprace`, with at the end:

| Mode | What it does at e | Columns in the result file |
|------|-------------------|----------------------------|
| 3 | `OUT (C),A` / `OUT (C),B` (AAh, then 55h, 14 Z80 cycles apart) to VRAM 7000h, reads back 7000h-7001h | second write lost (AAh, 00h), both written, other |
| 4 | the same writes, reads back 7000h-7002h | (55h, 00h, 00h), (AAh, 55h, 00h), other; plus all triples with their counts (`T` lines) |

Parameters: `C` = 100, `H` = 21 (text), 20 (SCREEN 5).

**Results, text mode** (`results/*-edge-m4-s0.txt`, 10240 trials each):

| Read back from 7000h-7002h | FS-A1GT | HB-F700D |
|----------------------------|---------|----------|
| AAh, 55h, 00h (both written) | 6112 | 5895 |
| 55h, 00h, 00h | 1725 | 3091 |
| 55h, 55h, 00h | 2233 | 534 |
| AAh, AAh, 00h | 0 | 720 |
| AAh, 00h, 00h (second write lost) | 0 | 0 |
| 00h, 55h, 00h | 168 | 0 |
| other | 2 | 0 |

The bad trials come in windows of 8 Z80 cycles in every 16 (the text mode
access slot pattern) on both machines; the part of the window where only
the second byte arrives (55h, 00h, 00h) is FS-A1GT e 14-16, HB-F700D e
17-21.

**Results, SCREEN 5** (`results/*-edge-m4-s5*.txt`): 3 to 4 Z80 cycles out
of about every 21 (128 VDP cycles) are too fast: FS-A1GT 1205 and 1233
times (55h, 55h, 00h) and 134 and 118 times (00h, 55h, 00h) in two runs;
HB-F700D 725 times (55h, 55h, 00h), 505 times (AAh, AAh, 00h) and 10 times
(55h, 00h, 00h).

### Conclusion

Measured: when a second `OUT` to VRAM comes too soon after the first, the
result is never a lost second write: both addresses get the second byte
(55h, 55h, 00h), or the first address gets the second byte and the second
address nothing (55h, 00h, 00h), or the second address gets its byte and
the first nothing (00h, 55h, 00h, FS-A1GT), or the first byte is written
twice (AAh, AAh, 00h, HB-F700D). The too-fast windows are 8 Z80 cycles in
every 16 in text mode and 3 to 4 in about every 21 in SCREEN 5.

*Interpretation (an assumption, not measured):* the VDP keeps the data of a
CPU VRAM write in one register, not with the request: a write that comes
too soon overwrites the byte of the write that is still waiting, and a
second request accepted while the first still waits gets the same byte.

## A mid-line R#23 write takes effect per 8-pixel group

**Files.** `vdpline4.asm`, `genstair.py`, `vdpline4.bas`: a staircase of
R#23 writes relative to HR, 1 Z80 cycle later every 8 lines, for photos.
`vdpline4.asm` includes lines generated by `genstair.py` (`stair.inc`).
Photos: `photos/hb-f700d-vdpline4-stair.jpg`,
`photos/fs-a1gt-vdpline4-stair-1.jpg` and `-2.jpg`.

**How.** `vdpline4.asm` with `vdpline4.bas`, SCREEN 5: the `vdpline` loop
(synced to HR as in `vdpline`), writing R#23 (vertical scroll) instead of
R#7: 8 at a fixed moment in the line and 0 later in the line, over a picture
of blocks of 8 white and 8 black lines, so each write shows as a seam where
the blocks swap. The 64 lines are unrolled (`genstair.py`) in 8 groups of 8
lines; in each next group the first write comes 1 Z80 cycle later. A ruler
above and below the picture, drawn by the VDP, has a tick every 8 pixels.

**Results.** The seam doesn't move 1.5 pixels per group, but jumps by 8
pixels, always to a ruler tick. The seam at display x 136, 144 or 152, from
which group on:

| | at 144 from group | at 152 from group |
|---|---|---|
| FS-A1GT | 0 or 1 (group 0 flips) | 5 or 6 (group 5 flips) |
| HB-F700D | 1 | 6 |

On the FS-A1GT (clocks not locked) group 0 and group 5 flip between two
positions: those writes are right at a group boundary. One group is one Z80
cycle (6 VDP cycles).

### Conclusion

Measured: a mid-line R#23 write (bitmap modes) takes effect from an
8-pixel group of the picture on: the seam jumps by 8 pixels, on the
picture's own 8-pixel grid, 1 Z80 cycle per group moving it to the next
group after about every 5 groups.

## The display mode and enable are latched once per line

**Files.** `hintz80.asm`: runs grauw's `HINTTOBL` from issue #1310 in Z80
mode (on a turboR).

**How.** `HINTTOBL` is the test program from issue #1310 by grauw (download
link in `hintz80.asm`), an MSX-DOS program, SCREEN 1. In 4-line rows, each
row waits with `HALT` for a line interrupt, then waits a fixed time that is
one Z80 cycle longer in every next row, and writes R#1 with the display
disabled, followed by a write that enables it again 38 Z80 cycles later. As
long as the disable comes before the moment the VDP latches the display
enable for the next line, that line is blank. The rows are labelled with
their wait (the number of Z80 cycles from the interrupt to the 11th cycle of
the `OUT`); the reading is the last row that still has a blank line, even if
it only flickers (`HALT` adds 0 to 4 cycles). `hintz80.asm` wraps it into
`HINTZ80.COM`, which first switches a turboR to Z80 mode (on a disk with an
MSX-DOS 1 boot sector the FS-A1GT already starts in Z80 mode).

**Results:**

| Machine | Z80 cycles |
|---------|------------|
| Yamaha CX5MII (V9938), grauw in #1310 | 242 |
| Sony HB-F700D (V9938) | 242 (photo: the line through the lower half of the `241/242` label) |
| Panasonic FS-A1GT (V9958) | 241 (photo: through the upper half) |

### Software with writes near this moment

On the real machines:

- **Woei** (issue #470): its split (R#1, display off, after polling FH)
  is stable on both machines.
- **Dawn of Time**, the copper part (issue #1313): the top line of the
  copper area is missing now and then: on the HB-F700D 1 or 2 times per
  second (by eye). On the FS-A1GT its bottom line flickers, and the bars in
  this part are uneven.

### Conclusion

Measured (`HINTTOBL`, issue #1310): the VDP latches the display mode, the
display enable and the sprite disable once per line; `HINTTOBL` gives 242
Z80 cycles from the line interrupt on the V9938 and 241 on the V9958.
Fitted (`HINTTOBL` run in an emulation with the timing and the interrupt
line delays above): the latch is 91 to 96 VDP cycles (V9938) and 79 to 84
(V9958) before HR falls at the start of the line it is for.

## The line counter reset

**Files.** `vdplc.asm`, `autoexec-vdplc.bas`: writes LN (R#9) at a known
moment around the line counter reset: is it taken for the next frame?
Results: `results/<machine>-lc-<screen5|text>-r18-<0|7|8>.txt` (columns
e, -, seen, before).

**How.** The `vdpmode` trial (sync on VR, HR samples), then a wait into the
line count reset line of the next frame (line 8 + vertical adjust), where
R#9 is written with LN = 0 (192 lines) at a moment d, in 1-cycle steps; the
value byte is written before the wait, the register byte at the measured
moment. Then S#2 is read about 5 lines after the display start with 212
lines: the reset fixes the display start from LN as it is then, and with
192 lines the display starts 10 lines later, so VR is still 1 there if the
write came before the reset ("seen") and 0 if it came after it ("before").
(The display end compares the line counter with LN as it is at that
moment, so it can't show when LN was taken.) Then LN = 1 again, before the
next frame's reset; each trial takes two frames. The waits differ for 50
and 60 Hz. GRAPHIC4 and TEXT2, each with R#18 = 00h, 07h and 08h, 8
repetitions.

**Results**, the last e at which the write is taken and the first at which
it isn't (the reset lies in between):

| | GRAPHIC4 | TEXT2 |
|---|---|---|
| HB-F700D, R#18 = 00h | 157\|160 | 152\|154 |
| HB-F700D, R#18 = 07h | 158\|162 | 154\|156 |
| HB-F700D, R#18 = 08h | 159\|160 | 153\|158 |
| FS-A1GT, R#18 = 00h | 140\|141 | 135\|136 |
| FS-A1GT, R#18 = 07h | 140\|142 | 135\|136 |
| FS-A1GT, R#18 = 08h | 140\|141 | 135\|136 |

e is relative to HR's falling edge in the screen mode of the test, which
comes 31 VDP cycles (5 Z80 cycles) later in TEXT2: the reset is at the same
moment in both modes. On the HB-F700D (locked clocks) only some e values get
trials, so its edges are wider.

### Conclusion

The line counter reset (where NT and LN are taken for the next frame) is
a different moment from the line latch. Measured (`vdplc`): it is the same
moment in GRAPHIC4 and TEXT2 (5 apart in e, relative to each mode's HR, as
HR falls 5 to 6 Z80 cycles later in TEXT2), and it moves with R#18 as HR
does. Fitted: 101 VDP cycles before HR falls, on both chips (4 VDP cycles
either way fits worse), 5 to 22 VDP cycles before the latch.

## YJK moves the picture, not the timing

V9958 only (SCREEN 10 to 12), measured on the FS-A1GT and the NMS 8250
with V9958.

**Files.**

- `vdpyjk.asm`, `autoexec-vdpyjk.bas`: SCREEN 8 above, SCREEN 12 below,
  the same columns drawn in both: does YJK move the picture? Photo:
  `photos/fs-a1gt-vdpyjk.jpg`.
- `vdpline5.asm`, `genyjk.py`, `vdpline5.bas`: a backdrop stripe edge in
  the border, timed relative to HR, next to pixel columns, in SCREEN 8
  and SCREEN 12 (`vdpline5.asm` includes `yjk.inc` from `genyjk.py`).
  Photos: `photos/<machine>-vdpline5-screen{8,12}-left.jpg`.
- `vdpmode` in SCREEN 8 and SCREEN 12 (`autoexec-vdpmode3.bas`, see "HR
  and VR in S#2"). Results: `results/fs-a1gt-mode-<mode>-s12.txt`.
- `vdpyjkl.asm`, `vdpyjkm.asm`, `vdpsw.asm`, `vdpyjk2.bas`, `vdpyjk3.bas`,
  `vdpyjk5.bas`, `autoexec-vdpyjk5.bas`: a YJK switch in the middle of a
  line (and more, see the later sections). These include `vdpline5.asm`
  (with `DEFINE SWEEP`: `vdpyjkl.asm` with `yjkl.inc` from
  `genyjk.py 22 36 16 8 yjkl.inc`, `vdpyjkm.asm` with `yjkm.inc` from
  `genyjk.py 28 36 16 8 yjkm.inc`) or `vdpline4.asm` (`vdpsw.asm`, with
  `stairsw.inc` from `genstair.py 29 stairsw.inc`). Photos:
  `photos/<machine>-vdpyjk3-switch-*.jpg`,
  `photos/<machine>-vdpyjk5-switch-374.jpg`,
  `photos/fs-a1gt-nms-8250-v9958-vdpyjk2-switch.jpg`.
- `vdpyjkn.asm`, `vdpyjk4.bas`, `vdplc2.bas`: a mid-line R#23 write, the
  line latch and the line counter reset in YJK (`vdpyjkn.asm` with
  `yjkn.inc` from `genyjk.py 22 38 16 8 yjkn.inc`). Photos:
  `photos/<machine>-vdpyjk4-*.jpg`. Results:
  `results/<machine>-lc-<screen8|screen12>-r18-<0|8>.txt`.

### vdpyjk: does YJK move the picture?

**How.** `vdpyjk.asm` with `autoexec-vdpyjk.bas`: SCREEN 8 at the top,
SCREEN 12 (R#25 = 08h) from line 106 on, switched by the line interrupt.
Vertical lines of pixel value F8h every 16 pixels, drawn once for both
halves: yellow in SCREEN 8, white in SCREEN 12 (Y = 31, J = K = 0).

**Result:** the white lines are 4 pixels right of the yellow ones.

### vdpline5: does YJK move the timing?

**How.** `vdpline5.asm` with `vdpline5.bas`: the `vdpline` frame sync (VR,
then HR samples), then 64 lines of exactly 228 Z80 cycles in which R#7 gets
COLB in the right border and 19 cycles later COLA, in the left border of
the next line, so that a backdrop stripe edge lies in each border; 8 groups
of 8 lines, the writes 1 cycle later per group. Columns of pixel value F8h
every 16 pixels and at x = 255. Four screens: SCREEN 8 and SCREEN 12 (R#25
= 08h), the HR samples taken in that mode, with R#18 = 08h (picture 8
pixels right, for the left border) and 00h (for the right border). A
backdrop change shows at a fixed place on the screen for a fixed moment of
writing; the columns are 4 pixels further right in SCREEN 12 (`vdpyjk`).
If YJK moves the timing (HR) along, the writes move along and the edges
keep their distance to the columns; if not, the edges are 4 pixels further
from the first column in SCREEN 12.

**Results** (FS-A1GT, left border), the distance from the stripe edge to
the first column per group, in pixels (scale: 16 pixels between the first
two columns):

| group | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| SCREEN 8 | 10.4 | 8.4 | 7.1 | 5.1 | 3.9 | 1.8 | 0.6 | 0 |
| SCREEN 12 | 14.2 | 11.9 | 10.2 | 8.8 | 7.5 | 6.2 | 5.2 | 0 |

In SCREEN 12 the edges of groups 0 to 6 are 4 to 5 pixels further from the
first column than in SCREEN 8: HR doesn't move with YJK. (In group 7 the
stripe reaches the first column in both screens, in SCREEN 12 without the
4 pixels: those 4 pixels don't show the backdrop of each pixel's moment.
That is measured separately, with `vdpyjk3` and `vdpyjk5`.)

The modified NMS 8250 with a V9958 instead of its V9938 (no extra VDP I/O
wait, so other group positions; on a monitor whose horizontal position
reaches far enough to show the whole left border) gives the same result.
The photo values are measured per row, against the first column in that
row (the camera is slightly tilted):

| group | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| SCREEN 8 | 16.6 | 15.5 | 14.4 | 13.3 | 11.5 | 10.3 | 9.2 | 8.2 |
| SCREEN 12 | 22.2 | 20.7 | 19.1 | 18.0 | 17.3 | 16.2 | 14.6 | 13.3 |

SCREEN 12 is 4.7 to 5.9 pixels further from the first column than SCREEN
8 in every group. The bars moved by about a pixel on the screen, which
blurs the edges in the photos. The steps are about 1.2 pixels instead of
the 1.5 of one Z80 cycle: the CPU clock of this machine runs 116 ppm fast
(`clktest`), so the 8 lines (1824 cycles) of a group take 0.21 Z80 cycles
less than 8 VDP lines, and each next group moves 0.32 pixels less than one
cycle (1.5 - 0.32 = 1.18). Here the last group stays 8 pixels from the
first column.

On the right border the stripe starts right after the last column in both
screens: there the display end cuts it off, so it doesn't show the
difference.

### vdpmode in SCREEN 12

**Results** (FS-A1GT): SCREEN 12 gives F, FH, VR and HR at the same
positions relative to each other as SCREEN 8.

### YJK switched in the middle of a line

**How.** Each screen waits for a photo, space goes on.

- `vdpyjk2.bas` screen 4: `VDPLIN4.BIN` (`vdpline4`, R#25 instead of
  R#23): SCREEN 8 with columns every 8 pixels (and x + 1 every 32), R#25
  = 08h (YJK) written at a moment in the line and 00h 112 Z80 cycles
  later.
- `vdpyjk3.bas` screens 3 to 6: `VDPSW.BIN` (`vdpsw.asm`: `vdpline4` with
  the second write 57 Z80 cycles after the first), columns every 8 pixels,
  at two moments (`C` = 374 and 375, 30 Z80 cycles apart): SCREEN 5 with
  R#7 (colour 0 shows the backdrop, so a red stripe shows where the writes
  land) and then SCREEN 8 with R#25 = 08h and 00h at the same moments.
- `vdpyjk5.bas` screens 3 to 6 (`autoexec-vdpyjk5.bas` runs it on a
  V9958): the same at `C` = 374 and 375, over columns of F8h every 2
  pixels (yellow in SCREEN 8, white in YJK), with a ruler of F8h (a tick
  every 8 pixels, longer every 32) above and below.

**Results, columns every 8 pixels** (`vdpyjk3`, each the R#7 screen above
the R#25 screen; `fs-a1gt-nms-8250-v9958-vdpyjk2-switch.jpg`: a close-up
of the switch point of `vdpyjk2` screen 4). YJK switches on and off in
the middle of the line, at the place in the picture where an R#7 write
made at the same moment lands, to within the 8-pixel spacing of the
columns (x, top group to bottom group):

| | R#7 stripe | YJK part (R#25) |
|---|---|---|
| FS-A1GT, `C` = 374 | 40 to 48 - 125 to 135 | 40 to 52 - 132 to 140 |
| NMS 8250 (V9958), `C` = 374 | 34 to 42 - 120 to 125 | 33 to 41 - 116 to 124 |
| FS-A1GT, `C` = 375 | from 85 - 174 (top group) | from 83 - 172 (top group) |

From the switch point on, the columns are white and lie halfway between
the yellow ones: the YJK pixels come out 4 pixels later from there on, in
that line. The column just before the switch point shows red or orange for
a group or two.

**Results, columns every 2 pixels** (`vdpyjk5`, R#7 left, R#25 right), x
of the edges in pixels, read against the nearest ruler, top group to
bottom group:

| | R#7 stripe | YJK part (R#25) |
|---|---|---|
| FS-A1GT, `C` = 374 | 41.2 to 48.6 - 129.1 to 137.1 | 38.7 to 47.7 - 129.1 to 135.4 |
| NMS 8250 (V9958), `C` = 374, top group | 34.8 - 120 | 33.6 - 120.1 |

So YJK switches on and off where the R#7 write lands, to within 1 to 2.5
pixels, and not in steps of 8 pixels: `vdpyjk3`'s columns every 8 pixels
showed it only where a column was.

### vdpyjk4 and vdplc2: R#23, the line latch and the line counter reset in YJK

**How.** `vdpyjk4.bas` (each screen waits for a photo, space goes on;
SCREEN 12 is SCREEN 8 with R#25 = 08h, written by machine code, so that it
also works with an MSX2 BIOS):

1. and 2. `VDPLIN4.BIN` (`vdpline4`, the R#23 staircase) in SCREEN 8 and in
   SCREEN 12, the HR samples taken in that mode, over blocks of 8 lines of
   F8h and 0 with a ruler of F8h (a tick every 8 pixels).
3. and 4. `VDPYJKN.BIN` (`vdpyjkn.asm`: `vdpline5` with 16 groups of 8
   lines, the first write 1 Z80 cycle later per group) writing R#1: the
   display off, and on again 50 Z80 cycles later, over a picture of F8h, in
   SCREEN 8 and SCREEN 12. A line stays black when the latch for it comes
   while the display is off; the groups in which that happens show where
   the latch is.

Then `vdplc2.bas` runs `vdplc` (`VDPLC.BIN`) in SCREEN 8 and SCREEN 12
(GRAPHIC7, and GRAPHIC7 with R#25 = 08h), R#18 = 00h and 08h, 8
repetitions.

**Results, R#23** (each SCREEN 8 above SCREEN 12, and videos): in both
modes the seam lies on a tick of the ruler, so in SCREEN 12 on the moved
(YJK) picture's 8-pixel grid, 4 pixels further right on the screen. It
moves to the next tick at the same group in both modes (on the FS-A1GT
varying between group 5 and 7 from frame to frame, the same range in both
modes).

**Results, the latch** (left SCREEN 8, right SCREEN 12), the number of
black lines, measured in every frame of a video (6 frames per second; it
alternates between two values, one group apart):

| | SCREEN 8 | SCREEN 12 |
|---|---|---|
| FS-A1GT | 33 or 41 | 33 or 41 |
| NMS 8250 (V9958) | 66 or 74 | 66 or 74 |

A latch 16 VDP cycles (4 pixels) later in SCREEN 12 would make the black
band 2.7 groups (21 lines) longer: the latch doesn't move with YJK.

**Results, the line counter reset**, the last e at which the write is taken
and the first at which it isn't:

| | SCREEN 8, R#18 = 00h | SCREEN 12, 00h | SCREEN 8, 08h | SCREEN 12, 08h |
|---|---|---|---|---|
| FS-A1GT | 141\|142 | 141\|142 | 141\|142 | 141\|142 |
| NMS 8250 (V9958) | 166\|167 | 166\|167 | 166\|167 | 166\|167 |

The reset doesn't move with YJK (16 VDP cycles would be 2.7 Z80 cycles). The
FS-A1GT gives the edge of GRAPHIC4 (`vdplc`: 140\|141) to within one Z80
cycle. The NMS 8250's e values are larger by about 7 because its CPU clock
runs 116 ppm fast: the write comes about 60000 CPU cycles after the HR
samples, so it lands 6 to 8 Z80 cycles early.

### Conclusion

Measured on the V9958:

- The picture moves 4 pixels to the right (`vdpyjk`).
- The timing doesn't move: relative to the picture of SCREEN 8, a backdrop
  write lands at the same place on the screen (4 to 6 pixels further from
  the moved columns, `vdpline5`), and F, FH, VR and HR keep their positions
  relative to each other (`vdpmode`).
- A mid-line R#23 write takes effect from an 8-pixel group of the moved
  picture, at the same group for the same write moment as in SCREEN 8
  (`vdpyjk4`).
- The line latch and the line counter reset don't move (`vdpyjk4`,
  `vdplc2`).
- An R#25 write in the middle of a line switches YJK on or off from the
  point in the picture where an R#7 write at the same moment lands, to
  within 1 to 2.5 pixels (switching on perhaps that much earlier), and the
  4-pixel shift starts and ends there too, in that line (`vdpyjk3`,
  `vdpyjk5`). The column right before the switch point shows red or
  orange for a group or two.

## The 4 pixels before a YJK picture

**Files.** `vdpline5` (see "YJK moves the picture, not the timing"),
screens 2 and 3 of `vdpyjk2.bas`, and screens 1 and 2 of `vdpyjk3.bas` and
`vdpyjk5.bas`. Photos:
`photos/fs-a1gt-vdpline5-screen12-flicker.jpg`,
`photos/<machine>-vdpyjk3-strip.jpg`, `photos/fs-a1gt-vdpyjk5-gap.jpg`.

**How.**

- `vdpline5` in SCREEN 12 (see there): the stripe edges of groups 6 and 7
  are the ones whose second write comes at or after the display start of
  SCREEN 8 (where the stripe ends at the first column there).
- `vdpyjk2.bas` screens 2 and 3: `VDPYJKL.BIN` (`vdpyjkl.asm`: `vdpline5`
  with 16 groups of 8 lines and the second write 22 cycles after the
  first) in SCREEN 8 and SCREEN 12, R#18 = 08h, TP set (colour 0 is black,
  not the backdrop): the backdrop stripe's edge at the left moves 1 Z80
  cycle (1.5 pixels) per group, through the display start and the 4 pixels
  before the first YJK pixel.
- `vdpyjk3.bas` screens 1 and 2: the same, with `VDPYJKL.BIN` on a turboR
  and `VDPYJKM.BIN` (the second write 28 cycles after the first, 6 later)
  on other machines.
- `vdpyjk5.bas` screens 1 and 2: `vdpyjk3` screens 1 and 2 again, screen
  2 also as a video.

**Results, `vdpline5`.** On the FS-A1GT in SCREEN 12 the stripe of the last
group reaches the first column, and the 4-pixel gap of group 6 flickers
between black and the stripe colour, more often black (two frames of a
video: group 6 with its gap, and filled). On the FS-A1GT, whose clock
drifts, group 6 lands on either side of the display start of SCREEN 8.

**Results, `vdpyjk3`** (left SCREEN 8, right SCREEN 12). In SCREEN 8 the
stripe edge moves on by about 1.5 pixels per group until it reaches the
first column (the display start). In SCREEN 12 it never stops inside the 4
pixels before the first column: a few groups before the display start the
stripe colour fills the whole border up to the first column at once: on
the NMS 8250 after an edge at about 4.5 pixels before the first column, on
the FS-A1GT at about 7 (both about the display start of SCREEN 8, give or
take a group). On the FS-A1GT this was seen in both runs and in the
flicker of `vdpline5`.

**Results, `vdpyjk5`** (FS-A1GT; left SCREEN 8, right SCREEN 12; the
groups are 1 Z80 cycle apart). In SCREEN 8 the stripe edge reaches the
display start (the first column) at group 4.6; in SCREEN 12 the edge
stops 1.7 pixels before the display start and the stripe colour fills the
4 pixels up to the first YJK pixel from group 4.7 on. The pale band below
that in the photo is the transition flickering between the two (the
FS-A1GT's clock drifts).

### Conclusion

Measured: the 4 pixels between the display start and the first YJK pixel
show one colour: the backdrop as it is at the display start (of SCREEN 8),
to within one Z80 cycle, not the backdrop at each pixel (`vdpline5`,
`vdpyjk3`, `vdpyjk5`). Not measured with a masked border (R#25 bit 1),
which covers those pixels.

*Interpretation (an assumption):* the display window opens at its usual
moment and the YJK pixels come out 4 pixels later; in between the VDP
outputs the backdrop it took when the window opened.

## An NT write changes the length of the field it is made in

**Files.** `vdpnt.asm`, `vdpnt.bas`, `vdpnt2.bas`, `vdpnt3.bas`,
`vdpnt4.asm`, `vdpnt4.bas`, `autoexec-vdpnt.bas`: NT (R#9 bit 1) written N
lines after the display start, then the lines counted to VR's next four
edges. Results: `results/<machine>-nt-<a|b>.txt` (`vdpnt`),
`results/<machine>-nt2-<a|b>.txt` (`vdpnt2`), `results/<machine>-nt3.txt`
(`vdpnt3`), `results/<machine>-nt4-<a|b>.txt` (`vdpnt4`).

**How.** `vdpnt.asm`, per call: R#9 = NTA, 3 frames to settle; sync on VR's
fall (the display start), count N lines (HR pulses), wait DLY times 14 Z80
cycles, write R#9 = NTB, then count the lines to VR's next four edges
(rise, fall, rise, fall). The counts are lines from the write to the edge,
to within one line. Interrupts are off during the call. SCREEN 0 WIDTH 80,
212 lines; direction A is 50 to 60 Hz (NTA = 82h, NTB = 80h), B is 60 to
50 Hz (80h, 82h). Each N is run twice.

- `vdpnt.bas`: N = 0 to 340 in steps of 2 to 50, DLY 0. On the FS-A1GT and
  the HB-F700D it ran with a version of `vdpnt.asm` without DLY, which
  writes 32 Z80 cycles earlier than this one with DLY = 0; on the NMS 8250
  (V9958) with this one.
- `vdpnt2.bas`: around the end of the field: N = 249 to 257 (A) and 224 to
  232 (B), DLY = 0 to 16 in steps of 2 (DLY 16 is 224 Z80 cycles, about a
  line).
- `vdpnt3.bas`: direction A around the end of a 60 Hz field: N = 199 to
  206, DLY = 0 to 16.
- `vdpnt4.bas` with `VDPNT4.BIN` (`vdpnt4.asm`: `vdpnt.asm` with a second
  delay FINE of 1 Z80 cycle per step, through a NOP sled and stubs of 14 to
  18 cycles as in `vdpedge`): around the switch of `vdpnt2`, N = 253 (A)
  and 229 (B), a delay of 65 to 125 Z80 cycles in steps of one (DLY =
  delay / 14, FINE = the rest), each 6 times, with R#18 = 00h, 07h and
  08h. The result files list R#18, N and the delay, then the four counts.

`autoexec-vdpnt.bas` runs whichever of the four hasn't saved its result
file yet; each comes back to it when it's done.

The result files list N (and DLY), then the four counts. Below, D = N + the
first two counts: the lines from the display start to the next display
start (the field's own length, plus the difference between the two
fields' top borders). The machines agree to within one line, apart from
N = 312 in A: there the NMS 8250's write (32 Z80 cycles later, see above)
lands after the next field's display start (D = 546).

**Results, `vdpnt`**, D:

| A (50 to 60 Hz), N | D | then (blanking lines) |
|---|---|---|
| 0 to 200 | 234 | 48 |
| 205 to 252 | 484 | 48 |
| 254 to 266 | 285 | 48 |
| 270 to 312 | 312 | 21 |
| 315 to 340 | 547 | 48 |

| B (60 to 50 Hz), N | D | then (blanking lines) |
|---|---|---|
| 0 to 225 | 339 | 99 |
| 230 to 245 | 288 | 99 |
| 248 to 260 | 261 | 126 |
| 262 to 340 | 601 | 99 |

Read as fields: with A, a write up to N = 200 gives the field the 60 Hz
length; from N = 205 on the field runs on much longer, 250 lines more
than a 60 Hz field; from N = 254 on the write is too late for this field,
which keeps the 50 Hz length. From N = 270 on the write lands in the next
field, which then gets the 60 Hz length (21 blanking lines after its
display) but keeps the 50 Hz top border; from N = 315 on it lands after
that field's display start. With B, a write up to N = 225 gives the field
the 50 Hz length, from N = 230 on it is too late; from N = 248 on it
lands in the next field, which gets the 50 Hz length (126 blanking lines)
and keeps the 60 Hz top border; from N = 262 on it lands after that
field's display start.

**Results, `vdpnt2`**, the switch, as N and the first DLY with the new
outcome (each DLY twice):

| | FS-A1GT | HB-F700D | NMS 8250 (V9958) |
|---|---|---|---|
| A: from D = 484 to 285 | N = 253, DLY 12 (mixed) to 14 | N = 253, DLY 14 | N = 253, DLY 14 |
| B: from D = 339 to 288 | N = 229, DLY 14 | N = 229, DLY 14 | N = 229, DLY 14 |

**Results, `vdpnt3`**: from D = 234 to 484 at N = 202, DLY 14, on all
three machines.

**Results, `vdpnt4`**, the switch (A: from D = 484 to 285, B: from D = 339
to 288) as the delay in Z80 cycles at which half of the trials have the new
outcome:

| R#18 | FS-A1GT, A / B | HB-F700D, A / B | NMS 8250 (V9958), A / B |
|---|---|---|---|
| 00h | 101.0 / 100.8 | 102.5 / 103.0 | 105.3 / 102.2 |
| 07h | 105.2 / 106.7 | 108.5 / 105.8 | 109.2 / 108.7 |
| 08h | 94.8 / 96.3 | 94.7 / 98.7 | 97.7 / 97.5 |

From R#18 = 07h to 08h the switch comes 7 to 14 Z80 cycles earlier
(about 10 to 11 on average per machine), from 00h to 07h 3 to 7 later.
The delay counts from an HR edge, and HR moves with the horizontal adjust
(see "F, FH, VR and HR all move with the horizontal adjust"): 60 VDP
cycles (10 Z80 cycles) later from 07h to 08h, 28 (about 5) earlier from
00h to 07h. The switch moves by the same amount the other way: the moment
that decides doesn't move with the adjust.

### Conclusion

Measured on the V9938 and the V9958 (the HB-F700D, the FS-A1GT and the
NMS 8250 with V9958): an NT write changes the length of the field it is
made in. In time, the field gets the length of the rate written; when the
line counter is already past that length, the field runs on for a long
time (A: 250 lines more than a 60 Hz field); too late, the field keeps
its length and the next field gets the new rate. A write in the next
field's first lines changes its length, but not its top border.

Fitted (`vdpnt`, `vdpnt2`, `vdpnt3` and `vdpnt4` of all three machines,
run in an emulation with the timing above): the field lengths are 262 and
313 lines, and 512 when the counter is past the new length (the counter
runs on to its wrap); the top border follows NT at the line counter reset.
The write is in time if it comes at least 400 ± 10 VDP cycles before the
end of the line in which the field would end, i.e. before 709 ± 10 VDP
cycles after HR falls at the start of that line (R#18 = 00h); unlike HR, F
and VR, this moment doesn't move with the horizontal adjust. `vdpnt2` and
`vdpnt3` alone allow 360 to 520 (they reproduce all switches with those,
apart from single trials right at a switch; 300 and 600 don't). `vdpnt4`
narrows it down: with 400 the emulated switch lies within 0.6 Z80 cycles
of the real one on average on each machine (single settings within 2.5),
with 380 and 420 it is 2.5 to 5 cycles off. The same moment holds for the
end of the old rate (`vdpnt2`, `vdpnt4`) and for the new rate's end that
would already have passed (`vdpnt3`).
