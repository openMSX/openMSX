# V99x8: where in the line HR, VR, F and FH change

## The question

The status flags tell the CPU where the VDP is: HR (S#2 bit 5, horizontal blanking), VR
(S#2 bit 6, vertical blanking), F (S#0 bit 7, set at the end of the display) and FH (S#1
bit 0, the line interrupt). Where in the line does each one change, relative to the
others? Does that depend on the screen mode, and on the horizontal adjust (R#18)?

## The tests

All use the method of `README.md`. Every read moment (in the result files: column e) is
relative to HR's falling edge, measured per trial, plus a constant of the program;
neighbouring read moments are one Z80 cycle (6 VDP cycles) apart. The read moments of
different programs have different constants and can't be
compared directly.

1. **`vdprace`** (`README.md`) in text mode and SCREEN 5 (`autoexec-screen5.bas`): where
   F and FH are lost and where VR rises.
2. **`vdpedge`** (`vdpedge.asm`, `vdpedge.bas`, `autoexec-vdpedge.bas`): reads S#2 once
   at a read moment: mode 0 VR rising, mode 1 VR falling (one frame later), mode 2 HR, both edges.
   Text mode and SCREEN 5.
3. **`vdpmode`** (`vdpmode.asm`, `autoexec-vdpmode.bas` to `autoexec-vdpmode4.bas`):
   `vdprace` with **two screen modes per trial**: mode A while the trial syncs on VR and
   measures HR's phase, mode B from right after that, for the read. T = TEXT2 (SCREEN 0
   WIDTH 80), G = GRAPHIC4 (SCREEN 5). This separates "the flag moves with the mode" from
   "HR, the reference, moves with the mode": a flag that depends on mode A only moves
   with the reference. MODE 0 F, 1 FH (line 191), 2 VR, 3 HR. Also with R#18 set for the
   whole trial (00h, 04h, 07h, 08h, 0Ch), and on the FS-A1GT in SCREEN 0 WIDTH 40 and
   SCREEN 1 to 8.
4. **`vdpadj`** (`vdpadj.asm`, `autoexec-vdpadj.bas`): `vdpmode` MODE 0 (F) and 2 (VR),
   with R#18 = 00h while HR's phase is measured and R#18 = B (00h, 02h, 07h, 08h, 0Fh)
   from right after that. Since the read moment stays relative to HR with R#18 = 00h, the
   edges show how far F and VR move with the adjust.

Machines: Panasonic FS-A1GT (V9958) and Sony HB-F700D (V9938); 32 trials per read moment (16 or 8 in
some runs, see the result files).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The read moments relative to HR's falling edge | `README.md` | measured, to one Z80 cycle |
| F and FH are where a read starts to lose them; VR and HR where A1's value changes | `README.md` (the end of the lost window is the moment the flag is set) | a model that fits; it applies to all flags alike, so it cancels in the differences between flags |
| Converting read moments to VDP cycles from one fixed point | running the same programs in openMSX, whose CPU timing is exact | derived ("fitted" below: reproduces the edges of the tests named to within one Z80 cycle) |
| The reference 0: HR's falling edge in the graphics modes with R#18 = 00h | chosen; it's what the CPU tests measure directly | definition |

## The results

**The F, FH, VR and HR edges in one mode** (read moments, `vdprace` and `vdpedge` in their own
runs; "205|206" means the outcome changes between these two read moments):

| | FS-A1GT | HB-F700D |
|---|---|---|
| F lost, text | 205\|206 - 209\|210 | 207\|208 - 212\|213 |
| F lost, SCREEN 5 | 211\|212 - 214\|215 | 213\|214 - 217\|218 |
| FH lost, text | 151\|152 - 155\|156 | 152\|155 - 158\|160 |
| FH lost, SCREEN 5 | 161\|162 - 165\|166 | 163\|164 - 168\|169 |
| VR rises (`vdprace`), text / SCREEN 5 | 208\|209 / 213\|214 | 210\|212 / 215\|217 |
| VR rises (`vdpedge` 0), text / SCREEN 5 | 225\|226 / 230\|231 | 225\|226 / 230\|233 |
| VR falls (`vdpedge` 1), text / SCREEN 5 | 236\|237 / 242\|243 | 224\|225 / 229\|231 |
| HR rises (`vdpedge` 2), text / SCREEN 5 | 238\|239 / 253\|254 | 237\|238 / 252\|253 |

Relative to HR's falling edge, everything lies 5 to 6 Z80 cycles later in SCREEN 5 than in
text mode, and HR's rising edge 15 later. That alone doesn't tell whether F, FH and VR
move with the mode or HR does.

**`vdpmode`, a mode switch** (read moments; the phase measurement in mode A, the read in
mode B):

| FS-A1GT | T/T | T/G | G/G | G/T |
|---|---|---|---|---|
| F lost, from | 109\|110 | 109\|110 | 115\|116 | 115\|116 |
| VR rises | 112\|113 | 112\|113 | 117\|118 | 117\|118 |
| FH lost, from | 55\|56 | 59\|60 | 65\|66 | 60\|61 |
| HR rises | 124\|125 | 134\|135 | 140\|141 | 130\|131 |
| HR falls | 191\|192 | 186\|187 | 191\|192 | 196\|197 |

| HB-F700D | T/T | T/G | G/G | G/T |
|---|---|---|---|---|
| F lost, from | 115\|118 | 115\|116 | 120\|121 | 120\|121 |
| VR rises | 115\|120 | 116\|122 | 121\|126 | 124\|125 |
| FH lost, from | 61\|64 | 65\|66 | 70\|71 | 66\|67 |
| HR rises | 131\|132 | 141\|142 | 146\|147 | 136\|137 |
| HR falls | 198\|199 | 193\|194 | 198\|199 | 203\|204 |

- F and VR depend on mode A only (T/G = T/T, G/T = G/G): they don't move with the mode;
  what moved in the first table was the reference, HR.
- HR depends on mode B: in GRAPHIC4 it rises 10 Z80 cycles later and falls 5 earlier
  than in TEXT2, relative to F.
- FH depends on both: corrected for the reference (mode A), FH comes 4 to 5 Z80 cycles
  later relative to F when mode B, the mode at the moment FH is set, is GRAPHIC4.
- SCREEN 1, 2, 3, 4, 6, 7 and 8 give the edges of SCREEN 5; SCREEN 0 WIDTH 40 those
  of WIDTH 80 (FS-A1GT).

**`vdpmode` with R#18** (R#18 = 00h / 07h / 08h, read moments):

| | FS-A1GT | HB-F700D |
|---|---|---|
| F lost, T/T, from | 109\|110 / 109\|110 / 109\|110 | 115\|118 / (*) / 116\|117 |
| F lost, G/G, from | 115\|116 / 115\|116 / 115\|116 | 120\|121 / 121\|122 / 120\|123 |
| VR rises, T/T | 112\|113 / 112\|113 / 112\|113 | 115\|120 / 117\|122 / 119\|120 |
| VR rises, G/G | 117\|118 / 117\|118 / 117\|118 | 121\|126 / 123\|125 / 122\|125 |
| FH lost, T/T, from | 55\|56 / 55\|56 / 55\|56 | 61\|64 / 61\|62 / 61\|62 |
| FH lost, G/G, from | 65\|66 / 65\|66 / 65\|66 | 70\|71 / 71\|73 / 71\|72 |

(*) no read moment with trials inside the window. With R#18 = 04h and 0Ch (FS-A1GT,
SCREEN 5, F and VR): the same. So relative to HR, F, FH and VR don't move with R#18: when
R#18 moves HR, it moves them by the same amount. `vdpadj` below shows that they do move.

**`vdpadj`**, R#18 changed after the HR measurement: the shift of the F and VR edges
relative to B = 00h, in Z80 cycles. -4 VDP cycles per adjust step would give -1.3, -4.7,
+5.3 and +0.7:

| | B = 02h | 07h | 08h | 0Fh |
|---|---|---|---|---|
| FS-A1GT, F and VR, GRAPHIC4 and TEXT2 | -1 to -1.5 | -4 to -5 | +5 to +5.5 | 0 to +0.5 |
| HB-F700D, F and VR, GRAPHIC4 and TEXT2 | -1 | -4 to -4.5 | +5.5 to +6 | +1 |

## Independent measurements: igmar, Philips VG-8235 (V9958)

igmar measured the same edges with his own test ROM (PR #2299, test M13 steps 3 to 5; the
ROM and its sources are attached there). The Z80 runs locked to the VDP (228 Z80 cycles a
line) from RAM with interrupts off; it locks on HR near line 212, then reads S#2 VR (and
S#0 F) three times per Z80 cycle over a window, one sample a frame. Z80 cycles (T) from
the lock:

| R#18 | VR rise G4 | VR rise T1 | VR fall G4 | VR fall T1 | F rise G4 | F rise T1 |
|---|---|---|---|---|---|---|
| 0 | 44 | 54 | -185 | -175 | 45 | 55 |
| 2 | 42 | 52 | -187 | -177 | 43 | 53 |
| 7 | 39 | 49 | -190 | -180 | 40 | 50 |
| 8 | 49 | 59 | -180 | -170 | 50 | 60 |
| F | 44 | 54 | -185 | -175 | 45 | 55 |

(G4 = GRAPHIC4, T1 = TEXT1. A second power-up gave the same, except 1 T earlier for the
rises at nibble 0.) Against the tests above:
- **R#18:** VR's two edges and F move together relative to his HR lock by 0, -2, -5, +5,
  0 T at nibbles 0, 2, 7, 8, F: -4 VDP cycles per step, rounded to T, the same as
  `vdpadj` on both machines here.
- **F 1 T after VR,** in all rows: here 5.9 VDP cycles (FS-A1GT), 4.4 (NMS 8250), about
  10 (HB-F700D) (`README.md`).
- **TEXT1 10 T later than GRAPHIC4:** that is what `vdpmode` shows for HR's rising edge
  (10 Z80 cycles later in GRAPHIC4 relative to F), *if* his lock is on HR's rising edge.
  That is an assumption: his description doesn't say which edge.

So a third machine, measured with a different program by someone else, gives the same
behaviour with the adjust and the same order of VR and F.

## Conclusions for the hardware

Two separate questions: what the screen mode does to the flags, and what the horizontal
adjust (R#18) does to them.

**The screen mode** (`vdpmode`):

1. **F and VR don't move with the screen mode; HR does.** Relative to F and VR, HR is
   longer in the text modes: it rises earlier and falls later.
2. **FH moves with the screen mode at the moment it is set**: 4 to 5 Z80 cycles later in
   the graphics modes. After a mode switch it follows the new mode.

**The horizontal adjust** (`vdpadj`, `vdpmode` with R#18):

3. **All four flags, F, FH, VR and HR, move together with R#18**, by -4 VDP cycles per
   step. Nothing stays at a fixed place in the line when R#18 changes. Two tests show it:
   - `vdpadj` measures HR's phase with R#18 = 00h and sets R#18 to the value under test
     right after that, before the read. Its read moments stay relative to HR at R#18 =
     00h, so the F and VR edges show how far they moved: -4 VDP cycles per step (the
     `vdpadj` table).
   - `vdpmode` with R#18 set for the whole trial: relative to HR, F, FH and VR don't move,
     so HR moves by the same amount.

   igmar's M13, on a third machine, agrees.

**Where they are:**

4. **The positions, fitted** (VDP cycles from HR's falling edge in the graphics modes,
   R#18 = 00h; a line is 1368):
   - HR = 1 from -312 to 0 in the graphics modes (312 cycles), from -367 to +31 in the text
     modes (398 cycles);
   - FH is set at -345 in the graphics modes (33 before HR rises), -373 in the text modes
     (6 before HR rises);
   - VR changes, both edges, at -48 in all modes, about 4 to 10 VDP cycles before F is set
     (`README.md`). Exception: VR's falling edge in text mode fits 3 to 6 VDP cycles
     earlier.

   **How accurate:** the edges they rest on are known to one read moment (one Z80 cycle, 6
   VDP cycles), on the FS-A1GT more finely through its drift (`README.md`). With these
   positions openMSX reproduces every edge of these tests to within one read moment. They
   weren't scanned for the range that fits, as the windows of `README.md` were, so what is
   shown is: within 6 VDP cycles.

A mechanism that fits 1 to 3: one horizontal counter, which R#18 offsets, from which all
four flags are decoded; HR and FH with decode points per mode, F and VR with one decode
point. **IKA9958** (https://github.com/ika-musume/IKA9958, a V9958 rebuilt in HDL from die
shots) is built that way: its horizontal counter counts once per 4 VDP cycles, and its
decoder counter is reloaded when the horizontal counter plus R#18 reaches 23, so everything
decoded from it moves with the adjust. IKA9958 doesn't implement S#2, so which signals
drive HR and VR isn't known; but with one offset for its counter, chosen to fit, its decode
points fall on the fitted positions to within one counter step:

| signal | IKA9958 decode point | IKA9958 | fitted |
|---|---|---|---|
| HR rises, graphics modes | `0x108` (`bpla[37]`, resets `gt063`) | -313 | -312 |
| HR falls, graphics modes | `0x000` (`bpla[36]`, sets `gt062`) | -1 | 0 |
| HR rises, text modes | `0x140` (`bpla[33]`, resets `gt061`) | -377 | -367 |
| HR falls, text modes | `0x000` (`bpla[36]`) | +31 | +31 |
| VR changes | vertical tick (`vapla_tick`, latch `gc105`) | -53 | -48 |

The exception is HR's rise in text mode (10 VDP cycles). In IKA9958, too, the vertical
tick is at the same place in all screen modes and the HR decode points differ per mode.

**Not shown by these tests:** whether the display start, line zero and the switch of the
VRAM access slot grid move with the adjust.

## Files

- `vdpedge.asm`, `vdpedge.bas`, `autoexec-vdpedge.bas`, `autoexec-screen5.bas`;
  `vdpmode.asm`, `autoexec-vdpmode.bas` to `autoexec-vdpmode4.bas`; `vdpadj.asm`,
  `autoexec-vdpadj.bas`. Assemble and run as in `README.md`; `compare.py` compares a
  result file with the same program in openMSX.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/flag-positions`, per machine:
  - `<machine>-{f,fh,vr}-screen5.txt`: `vdprace` in SCREEN 5;
  - `<machine>-edge-m<mode>-s<screen>.txt`: `vdpedge` (columns: read moment, unused, bit 1, bit 0);
  - `<machine>-mode-<MODE>-<A><B>[-r18-<R#18>].txt` and `fs-a1gt-mode-<MODE>-<screen>.txt`:
    `vdpmode` (MODE 3: columns read moment, unused, HR = 1, HR = 0);
    `fs-a1gt-mode-3-tt-r18-7-2.txt` is a second run (the HB-F700D's first run of that test
    couldn't be read back from the floppy);
  - `<machine>-adj-{f,vr}-{screen5,text}-r18-<B>.txt`: `vdpadj`.
