# V99x8: the interrupt lines lag the status flags

## The question

The CPU sees F (S#0) and FH (S#1) by reading them, and the interrupt they cause through the
VDP's interrupt line, /INT. Does /INT change at the same moment as the flag, both when the
flag is set and when a read clears it? Or later? And is that the same on the V9938 and the
V9958?

`irqtest` (`README.md`) already hints at a lag: the interrupt sometimes comes after a read
that has seen *and cleared* F. If /INT followed F at once, that read would have dropped
/INT before the Z80 could sample it.

## The tests

Both use the method of `README.md`: sync on VR, measure HR's phase per trial, then one
read at a chosen moment, in steps of one Z80 cycle. Only the CPU is involved: no picture.

1. **`vdpirq`** (`vdpirq.asm`, `autoexec-vdpirq.bas`): the `vdprace` trial for F, with
   the interrupt enabled during the read A1 (IE0 on, `EI`, interrupt mode 2, a handler that
   reads S#0 and records that it ran and whether it saw F). Outcomes per read moment:
   - *before*: A1 doesn't see F, the handler does;
   - *lost*: neither sees F (the window of `README.md`);
   - *seen*: A1 sees F, no interrupt follows;
   - *pulse*: A1 sees F, and the interrupt still comes (the handler finds F cleared).

   Text mode and SCREEN 5, 32 trials per read moment.
2. **`vdpirqh`** (`vdpirqh.asm`, `autoexec-vdpirqh.bas`, `vdpirqh2.bas`): the same for FH
   and the line interrupt (line 191, IE1 on, IE0 off, the handler reads S#1).
   `vdpirqh2.bas` repeats it with 64 trials in text mode.

Machines: Panasonic FS-A1GT (V9958), Sony HB-F700D (V9938), and a Philips NMS 8250 with a
V9958 instead of its V9938.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The read moments, the "lost" window and how it maps to VDP cycles | `README.md` (`vdprace`) | measured, to one Z80 cycle |
| The Z80 samples /INT at the end of each instruction, and takes the interrupt there if it is active and interrupts are enabled | Z80 data sheet | data sheet |
| A read clears the flag a fixed time after it starts | the latch/clear model of `README.md` | a model that fits |

## The results

**`vdpirq`**, the read moments of the outcomes from the window on (in Z80 cycles, relative
to HR's falling edge plus a constant of the program; the result files call this column e):

| | lost | pulse | before again from |
|---|---|---|---|
| FS-A1GT, SCREEN 5 | 92-94 | 95-107 | 108 |
| FS-A1GT, text | 87-89 | 90-101 | 102 |
| HB-F700D, SCREEN 5 | 97-101 | 102-111 | 113 |
| HB-F700D, text | 92-96 | 97-106 | 107 |
| NMS 8250, SCREEN 5 | 103-106 | 107-117 | 118 |
| NMS 8250, text | 97-100 | 101-111 | 112 |

Right after the lost window, a read always sees F and clears it, and the interrupt *still*
comes. The pulse range ends where the interrupt comes before the read.

**`vdpirqh`**, the same for FH:

| | lost | pulse | before again from |
|---|---|---|---|
| FS-A1GT, SCREEN 5 | 42-45 | 46-57 | 58 |
| FS-A1GT, text | 32-35 | 36-47 | 48 |
| HB-F700D, SCREEN 5 | 47-51 | 52-62 | 63 |
| HB-F700D, SCREEN 5, second run | 48-52 | 53-63 | 64 |
| HB-F700D, text | 38-42 | 43-51 | 53 |
| HB-F700D, text, second run (64 trials) | 38-41 | 42-52 | 53 |
| NMS 8250, SCREEN 5 | 53-56 | 57-67 | 68 |
| NMS 8250, text (64 trials) | 43-46 | 47-57 | 58 |

The read moments of different machines can't be compared directly (`README.md`; the NMS
8250's are about 5 higher because of its fast CPU). The length of the lost and pulse ranges
together can: from where the lost window starts to where "before" comes back, both
interpolated over the trials, in Z80 cycles (SCREEN 5 / text):

| | F | FH |
|---|---|---|
| FS-A1GT (V9958) | 15.8 / 15.6 | 16.6 / 16.4 |
| HB-F700D (V9938) | 15.5 / 15.0 | 16.0 / 15.0 |
| NMS 8250 (V9958) | 14.9 / 15.0 | 15.3 / 15.3 |

The two V9958 machines differ as much as the V9958 and the V9938 do.

The raw results are in the data repository (see Files).

## Conclusions for the hardware

1. **/INT goes active later than the flag can be read.** The pulse range goes on until
   about 12 to 15 Z80 cycles after the flag is set (the lengths above, minus the part of the
   lost window before the flag). The read of an `IN A,(99h)` comes about 9 Z80 cycles after
   the instruction starts, so for the last reads of the range the
   flag was set before the `IN` started. Had /INT gone active with the flag, the Z80 would
   have taken the interrupt at the end of the instruction before, and the handler would
   have cleared the flag first ("before").
2. **/INT stays active for a while after the read that clears the flag**: the interrupt
   still comes after it.

   A delayed copy of the flag, for both edges, does both; one delay for both edges
   reproduces the lengths above (3). Why there is a delay is not known.
3. **The delay of F's line**, fitted: the same programs run in openMSX for each value,
   together with the values of `README.md` (the windows) and `line-latch.md` (the latch)
   that depend on them, three times each from a different start, and compared with the
   lengths above. 27 to 29 VDP cycles leave every machine within 0.5 Z80 cycles; 30 within
   0.7; 24 and 33 1.4 to 1.5 off. The V9938 and the V9958 show no difference beyond the one
   between the two V9958 machines.
4. **FH's line lags less: 22 VDP cycles.** The FH test alone allows 16 to 24, depending on
   where FH's lost window starts. `HINTTOBL` (`line-latch.md`) is reproduced on both the
   FS-A1GT and the HB-F700D with one latch only with 22 (20 and 24 fit no single latch),
   and with 22 the FH window that fits starts 17 to 18 VDP cycles before FH. Why FH's line
   would be faster than F's is not known.

Not measured on the TMS99xx.

## Files

- `vdpirq.asm`, `autoexec-vdpirq.bas`; `vdpirqh.asm`, `autoexec-vdpirqh.bas`,
  `vdpirqh2.bas`. Assemble and run as in `README.md`.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/output-delay`: `<machine>-irq-{text,screen5}.txt` (`vdpirq`: per read
  moment the number of trials lost, seen, before, pulse), `<machine>-irqh-*.txt` and
  `<machine>-irqh2-*.txt` (`vdpirqh`, the same columns).
