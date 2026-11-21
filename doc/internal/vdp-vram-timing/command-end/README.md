# V99x8: when CE clears after a command

## The question

S#2 bit 0, CE, is set while the command engine executes a command. Software polls it to
know when it can start the next command. When does CE clear, relative to the command's
last VRAM access? Is that the same for every command and every size? Does the engine
itself stay busy that long, or only the flag? And what do STOP, HMMC, LMMC and LMCM do at
their end?

## The tests

Five programs, all in SCREEN 5. Per trial: the command's registers, a sync on HR's falling
edge, then the `OUT (99h),A` that completes the R#46 write and starts the command, a delay
built as code (any number of Z80 cycles from a minimum on, in 1-cycle steps), and one read.
The *read moment* T is the start of that `IN`, in Z80 cycles after the start of the `OUT`.
20 trials per read moment.

1. **`vdpce`** (`vdpce.asm`, `vdpce.bas`), the display off: CE read at T = 19 to 338.
   HMMV of 1, 16 and 4 × 4 bytes, LMMV of 1 and 16 dots, HMMM of 1 and 16 bytes, LMMM of 1
   dot, PSET, LINE of 8 dots, POINT, YMMM of 2 bytes. And one more (`RH`): R#46 written
   again while CE is still set after a 16-byte HMMV, with CE read 27 Z80 cycles later.
   (The FS-A1GT ran a version of `vdpce.asm` that waited for that restarted command,
   which can be 1024 lines long, instead of stopping it; the same measurement, slower.)
2. **`vdpce2`** (`vdpce2.asm`, `vdpce2.bas`), the display off, T from 12 on (13 to 16 and
   18 can't be built). Three modes:
   - CE, for LINE of 2 and 32 dots along X and 8 along Y, LMMM of 16 dots, YMMM of 16
     bytes, HMMM of 4 × 4 bytes, SRCH (its colour 4 dots away), HMMC and LMMC of one byte
     or dot, and the 1-byte HMMV of `vdpce` again;
   - when the command's last write is in VRAM: the byte it writes is cleared first and the
     CPU's read address set to the byte before it; then `IN A,(98h)` at T starts the
     read-ahead of that byte, and the next `IN` after CE = 0 gives what it read. For HMMV
     and HMMM of 2 bytes, LMMV and LMMM of 3 dots, PSET, LINE of 7 dots and YMMM of 2
     bytes, each with its last write alone in its byte;
   - POINT: S#7 read at T, after a POINT of another colour.
3. **`vdpce3`** (`vdpce3.asm`, `vdpce3.bas`): `vdpce2` with the display and sprites on, and
   a sync on VR first (16 trials per frame from its falling edge on, so all trials fall in
   the display lines). The slots are far apart there: a command waits up to about 128 VDP
   cycles for each access.
4. **`vdpce4`** (`vdpce4.asm`, `vdpce4.bas`), the display off, as `vdpce2`:
   - STOP (R#46 = 00h) written during a 16-byte HMMV; T then counts from the STOP's `OUT`;
   - HMMC and LMMC of 2 bytes or dots without the CPU sending any: CE; and TR (S#2 bit 7)
     for HMMC of 1 and 2 bytes and LMMC of 2 dots;
   - when R#44's byte of a 1-byte HMMC and a 1-dot LMMC lands in VRAM (as in `vdpce2`);
   - LMCM of 1 dot without the CPU reading S#7: CE and TR;
   - the 1-byte HMMV of `vdpce` again, to tie the runs together.
5. **`vdpce5`** (`vdpce5.asm`, `vdpce5.bas`), the display off, as `vdpce4`:
   - STOP with no command running, and STOP during a 16-byte HMMV at four moments 18 Z80
     cycles apart; T counts from the STOP's `OUT`;
   - a 1-byte HMMC started with R#44 = F0h and a 1-dot LMMC started with R#44 = 3, then
     R#44 = 0Fh written by an `OUT` that starts 26 + i Z80 cycles after the start (the
     earliest the program can), then STOP; which byte the command wrote;
   - the 1-byte HMMV again.

Machines: Panasonic FS-A1GT (V9958; its S1990 adds a wait to VDP I/O), Sony HB-F700D
(V9938), Philips NMS 8250 with a V9958 (no extra wait).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The read moments relative to the start of the command | the programs' cycle counts | exact, to one Z80 cycle (6 VDP cycles) |
| With the display off the slots are regular, so a command's length hardly depends on where in the line it starts | the slot tables, sections 2 and 8 | measured (captures) |
| Where a command's accesses are: its start delay, its steps, the slots | logic-analyser captures, sections 2 to 4, 11 and 17; the last write checked directly (test 2, and test 3 for PSET) | measured |
| Where those slots are relative to HR (with the display and sprites on) | `../command-slots` | fitted, to about one Z80 cycle |
| When an S#2 read samples CE, relative to the `IN` | not known | a delay common to every read would be in all the values below; at most about 30 VDP cycles (the smallest ones) |
| The CE times are found by simulating the slot model (openMSX, which implements it) with CE held a given time after the command's last access | a model that fits | fitted: most runs within 1 Z80 cycle, a few up to 5 (below) |

## The results

**When CE clears** (tests 1 and 2), Z80 cycles after the start of the command; the three
machines give the same to within a few Z80 cycles (`summary.py`):

| command | CE clear from |
|---|---|
| HMMV 1 / 16 / 4 × 4 bytes | 32-33 / 159-163 / 186-189 |
| LMMV 1 / 16 dots | 32-34 / 278-279 |
| HMMM 1 / 16 bytes / 4 × 4 | 38-39 / 265-268 / 302-303 |
| LMMM 1 / 16 dots | 38-39 / 362-364 |
| YMMM 2 / 16 bytes | 45-46 / 199-203 |
| PSET | 40-42 |
| POINT | 39-41 |
| LINE 2 / 8 / 32 dots along X, 8 along Y | 50-51 / 166-173 / 628-629 / 169-170 |
| SRCH | 82-89 |
| HMMC, LMMC 1 byte or dot | 32-33 |

**The last write** (test 2) lands where the slot model has it: within 2 to 5 Z80 cycles for
HMMV, LMMV, HMMM, LMMM, PSET and YMMM, within 5 either way for LINE (where a command
crosses the line's slot gap depends on its start). POINT's colour shows in S#7 9 to 39 VDP
cycles after the model's read (the program can't read at the delays in between).

**So CE clears well after the last access.** The model with CE held a fixed time after the
last access, per command, gives the edges of most runs to within 1 Z80 cycle on each
machine. The exceptions are HMMV and HMMM of 16 bytes, LINE and SRCH, up to 5 either way,
about as far as the machines differ from each other there (HMMV of 16 bytes: 159 to
163):

| command | CE clears after the last access (VDP cycles) |
|---|---|
| HMMV, LMMV | 78 |
| YMMM | 80 |
| HMMM | 100 |
| LMMM | 104 |
| PSET | 131 |
| POINT | 170 (after the model's read) |
| LINE | 30 |
| SRCH | 29 |

The same value fits every size of a command that was measured, so it is a constant at the
end, not a time per byte or dot. PSET and a one-dot LMMV do the same VRAM work (a read, a
write), yet clear CE 131 and 78 cycles after it.

**A new command starts at once** (`RH`): R#46 written while CE is still set starts the new
command at 318 to 319 of 320 write moments on all three machines (CE set again 27 Z80
cycles later; the exceptions are right at the end).

**With the display and sprites on** (test 3), compared as the fraction of trials with CE
set at every read moment (the edges spread over several Z80 cycles there), the best CE
times after the last access (VDP cycles, steps of 5; FS-A1GT, HB-F700D, NMS 8250):

| command | sprites on | display off |
|---|---|---|
| HMMV 1 byte | 90, 85, 80 | 78 |
| LMMV 1 dot | 80, 90, 80 | 78 |
| HMMM 1 byte | 105, 105, 100 | 100 |
| LMMM 1 dot | 115, 115, 120 | 104 |
| PSET | 135, 135, 140 | 131 |
| POINT | 185, 175, 180 | 170 |
| LINE 8 dots | 30, 40, 25 | 30 |
| YMMM 2 bytes | 85, 85, 85 | 80 |
| SRCH | 35, 50, 45 | 29 |

On average about 6 VDP cycles more than with the display off (LMMM and SRCH about 13),
while the waits for the slots differ by up to about 128 cycles. The last write of HMMV,
LMMM and PSET lands where the model has it (within 1 Z80 cycle), and POINT's colour shows
in S#7 2 to 4 Z80 cycles after the model's read.

**STOP, HMMC, LMMC, LMCM** (test 4; `summary.py` on the `vdpce4` files):

- STOP during a 16-byte HMMV: CE clears 40 to 41 Z80 cycles (about 237 VDP cycles) after
  the STOP's `OUT`. A whole 1-byte HMMV clears CE 32 to 33 Z80 cycles after its start.
- HMMC of 2 bytes and LMMC of 2 dots, with no byte from the CPU: CE stays set over the
  whole range (to 331 Z80 cycles). With 1 byte or dot they end on their own (test 2).
- R#44's byte of a 1-byte HMMC, and its dot of a 1-dot LMMC, is in VRAM by 12 or 17 Z80
  cycles after the start (the first two read moments that can be built).
- TR: set from 17 Z80 cycles after the start for HMMC (1 and 2 bytes), already at the
  first read (12) for LMMC.
- LMCM of 1 dot: CE clears 33 to 35 Z80 cycles after the start, without S#7 being read;
  TR is set from 17 to 19.
- The 1-byte HMMV again: CE clear from 32 to 33, as in test 1. (On the HB-F700D one read
  moment after the edge, 37, has CE set in 19 of 20 trials; unexplained.)

**STOP and R#44** (test 5; `summary.py` on the `vdpce5` files):

- STOP with no command running sets CE too, and it clears 40 to 41 Z80 cycles after the
  STOP's `OUT` (FS-A1GT 40, the others 41).
- STOP during the 16-byte HMMV, at four moments 18 Z80 cycles apart: CE clears 39 to 41
  Z80 cycles after the STOP every time, the same as with no command running.
- R#44 written from 26 Z80 cycles after the start of a 1-byte HMMC or a 1-dot LMMC on:
  the command writes the start value of R#44 at every one of the 320 moments, on all
  three machines.
- The 1-byte HMMV: CE clear from 32 to 33 (the NMS 8250 33 to 36 in this run, a few
  trials late).

The full results are in the data repository (see Files).

## Independent measurements: igmar, Philips VG-8235 (V9958)

igmar's test ROM (PR #2299; the ROM, its sources and his measurements are attached there),
test C04 step 10, run twice with the same result: commands started with colour 2, and
right after the start R#44 = 7 written (or for one HMMV, NX = 8); then 16 dots across each
row read back.

| command | after the start | rows read back |
|---|---|---|
| LMMV 256 × 16 | R#44 = 7 | first dot 2, then 7 (row 700); all 7 (row 715) |
| LINE 255 dots | R#44 = 7 | first dot 2, then 7 |
| HMMV 256 × 16 | R#44 = 77h | all 2, in every row |
| HMMV 256 × 16 | NX = 8 | row 750 all 2; rows 751 and 765: 2 at X 0, 0 from X 16 on |

So HMMV takes its colour from R#44 once, at the start, while LMMV and LINE take R#44 for
every dot; and HMMV takes NX again for every row (8 dots from the second row on). So the
engine reads R#44 at different moments per command. For HMMC and LMMC, whose first byte
or dot comes from R#44 (above), it doesn't say when they take it: at the start, as HMMV,
or at their first write.

## Conclusions for the hardware

1. **CE stays set a fixed time after the command's last VRAM access**, per command: from
   about 30 VDP cycles (LINE, SRCH) to 131 (PSET) and 170 after POINT's read. The same for
   every size, on the V9938 and the V9958, and nearly the same with sprites on.
   - *Fitted:* the values come from the slot model fitted to the CE edges. The
     accesses themselves are where the model has them (the last write checked).
   - Not the engine finishing a step that waits for a slot: with sprites on those waits
     differ by up to 128 cycles, the times by about 6.
   - *Not known:* why the time differs per command (PSET and a one-dot LMMV do the same
     VRAM work), and how much of it is a delay common to every S#2 read (at most about
     30 cycles).
2. **Only the flag is held, not the engine:** a new R#46 write starts a new command at
   once, also while CE is still set.
3. **HMMC and LMMC take their first byte or dot from R#44**: with one byte or dot they end
   on their own, like HMMV and LMMV; with two they wait for the CPU's next one. R#44 is
   taken at or before the first write (at most 12 to 17 Z80 cycles after the start): a
   change from 26 Z80 cycles on comes too late. Whether it is taken at the start or at
   the first write, these tests can't tell.
4. **A one-dot LMCM ends on its own**, without the CPU reading S#7.
5. **STOP holds CE for a fixed time of its own**: about 237 VDP cycles (39.5 Z80 cycles)
   after it is written, with or without a command running, and wherever that command
   is. So STOP acts like a command without accesses, with a longer end than a 1-byte
   HMMV has from its start (about 195). *Not known why it takes this long.*

## Files

- `vdpce.asm`, `vdpce.bas`, `vdpce2.asm`, `vdpce2.bas`, `vdpce3.asm`, `vdpce3.bas`,
  `vdpce4.asm`, `vdpce4.bas`, `vdpce5.asm`, `vdpce5.bas`: the tests (assemble and run as
  `../../vdp-status-race/README.md` describes; each `.bas` runs all its commands and
  writes a result file per command). `summary.py DATA/<machine>-vdpce<n>.txt`: the edge
  per run.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-vram-timing/command-end`, `<machine>-vdpce<n>.txt`: per run a header line (`run=`
  the command, R#8 and R#9 read back after it), then per read moment i (T = 19 + i in
  `vdpce`, 12 + i in the others, plus the coarse delay `summary.py` lists; see its header
  for the runs that count from a STOP or write R#44) the number of trials per class (`0=`
  CE clear, or the old value; `1=` CE set, the new value, or TR set).
