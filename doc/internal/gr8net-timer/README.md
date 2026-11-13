# GR8NET: the controlled interrupt generator as a timer

These are the hardware probe and the photo behind the minimal GR8NET
device (`src/network/GR8NET.cc`). It emulates the adapter-ID ports and
the 24-bit counter of the controlled interrupt generator (GR8NET manual,
section 11.5.1), without the interrupt.

## Files

| File | What it is |
|------|------------|
| `gr8timer.z80` | GR8TIMER probe source. Its header describes every test. |
| `gr8timer_hotbit.jpg` | GR8TIMER on a real GR8NET. |
| `gr8timer_openmsx.png` | GR8TIMER in openMSX with `-ext gr8net`. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80, Nextor 2.01) with a GR8NET
(firmware 2017-5-9, hardware 0.5) in slot 2-0. It was photographed on a
Sony PVM monitor in black-and-white mode.

The openMSX screenshot comes from a Sharp HB-8000 with
`-ext Panasonic_FS-FD1A -ext gr8net`.

Build the probe with sjasmplus (`sjasmplus gr8timer.z80` writes
`GR8TIMER.COM`). It is an MSX-DOS .COM program and is MSX1-safe. It runs
once, prints its results and returns to DOS.

## Results

| Test | Expected | Real GR8NET | openMSX |
|------|----------|-------------|---------|
| ID | GR8N, date, slot, HW | 2017-5-9, slot 82h, HW 0.5 | 2023-3-19, slot 02h, HW 255.255 |
| READ PAIR | 335 | 335 | 335 |
| DELAY 10M | 625778 | 626461 | 625778 |
| DELAY 3M5 | 224000 | 224000 | 224000 |
| FRAME | ~166882 | 167033 | 166851 |
| STOP | two equal values | OK | OK |
| INT, CLEAR, AGAIN | OK | OK | OK |

In openMSX:

- the ID is the device's fixed build date;
- slot 02h is not expanded on that machine;
- hardware version registers 3 and 4 aren't emulated, so they read FFh.

## What this shows

- The counter works as a free-running timer with the interrupt disabled
  (IGE = 0). Its INT bit still sets and clears.
- **The 3.579545 MHz clock is locked to the Z80's clock on this board.**
  224000 Z80 clocks gave exactly 224000 ticks.
- **The 10 MHz clock runs 0.109% fast relative to the Z80** (626461
  against 625778). openMSX uses an exact 10 MHz.
- Two back-to-back reads (LSB, then the latched MID and MSB) take 335
  ticks at 10 MHz, as the Z80 clock count predicts.

For timing other devices (for instance V9990 commands), CLS = 1
(3.579545 MHz) counts in Z80 clocks.

## Not covered

- The generator's interrupt (IGE, HZP, WDE). It isn't emulated.
- Everything else on the GR8NET: mapper modes, network, SD card, audio.
  Those aren't emulated either.
