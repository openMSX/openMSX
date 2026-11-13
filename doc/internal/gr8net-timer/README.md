# GR8NET: the controlled interrupt generator as a timer

These are the hardware probe and the photo behind the GR8NET timer
device (`src/network/GR8NETTimer.cc`, `-ext gr8net-timer`). It is a
developer's tool, not a GR8NET: it emulates only the 24-bit counter of
the controlled interrupt generator (GR8NET manual, section 11.5.1),
without the interrupt.

## Files

| File | What it is |
|------|------------|
| `gr8timer.z80` | GR8TIMER probe source. Its header describes every test. |
| `gr8timer_hotbit.jpg` | An earlier GR8TIMER on a real GR8NET. |
| `gr8timer_openmsx.png` | GR8TIMER in openMSX with `-ext gr8net-timer`. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80, Nextor 2.01) with a GR8NET
(firmware 2017-5-9, hardware 0.5) in slot 2-0. It was photographed on a
Sony PVM monitor in black-and-white mode.

The openMSX screenshot comes from a Sharp HB-8000 with
`-ext Panasonic_FS-FD1A -ext gr8net-timer`.

Build the probe with sjasmplus (`sjasmplus gr8timer.z80` writes
`GR8TIMER.COM`). It is an MSX-DOS .COM program and is MSX1-safe. It runs
once, prints its results and returns to DOS.

## Finding the counter

The probe doesn't use the GR8NET's I/O ports (5Eh/5Fh), which the device
doesn't emulate. It puts every slot and subslot in page 1 in turn, starts
the counter at 3.579545 MHz and accepts the slot when:

- two reads in a row differ, and
- 224000 Z80 clocks count 224000 ticks, give or take 2.

A slot that fails gets its four bytes (5FEAh-5FEDh) written back, in case
it is RAM.

The photo was taken with an earlier version, which found the adapter
through the I/O ports and printed an ID line (build date, slot, hardware
version) in place of `TIMER IN SLOT`. The measurements are the same code.
**The slot search has not been run on a real GR8NET yet.**

## Results

| Test | Expected | Real GR8NET | openMSX |
|------|----------|-------------|---------|
| TIMER IN SLOT | the adapter's slot | (ID line: slot 82h) | 02h |
| READ PAIR | 335 | 335 | 335 |
| DELAY 10M | 625778 | 626461 | 625778 |
| DELAY 3M5 | 224000 | 224000 | 224000 |
| FRAME | ~166882 | 167033 | 166935 |
| STOP | two equal values | OK | OK |
| INT, CLEAR, AGAIN | OK | OK | OK |

In openMSX slot 2 is not expanded on that machine. With
`-ext slotexpander` before `-ext gr8net-timer` the probe finds it in
82h, and without the extension it prints `GR8NET timer not found`.

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
- Everything else on the GR8NET: the I/O ports (adapter identification),
  mapper modes, network, SD card, audio. Those aren't emulated either.
