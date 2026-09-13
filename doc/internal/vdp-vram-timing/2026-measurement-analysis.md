# V9938 command timing against the 2026 logic-analyzer measurements

In 2026 a second set of logic-analyzer captures of the V9938-VRAM bus was made, on a
Philips NMS 8280, and published in the `part2` subdirectory of
<https://github.com/m9710797/vdp-timing-measurements>. Unlike the 2013 set, it covers
all three access-slot patterns (display off, display on with sprites off, display on
with sprites on) with roughly equal weight, and it includes captures with the CPU
accessing VRAM at the same time.

This document fits openMSX's command-engine timing model to that data. It replaces the
earlier reasoning from the 2013 set plus aggregate `vdpcmdx` frame counts, discussed in
<https://github.com/openMSX/openMSX/issues/2057> and
<https://github.com/sndpl/openMSX/pull/5>; the fixes that came out of that discussion
are commits 592ce968c and 7dde62b3f. Everything section 4 and section 6 describe is
implemented, in commit 5b0327eaf; the "openMSX today" rows below are measured against
the state before it.

Two scripts in this directory produce every number here:

```
vcd-slot-grid.py <measurement-repo>/part2                  # where the slots are
2026-measurement-check.py <repo>/part2/5.slots             # command timing
2026-measurement-check.py <repo>/part2/5.slots --cpu       # under CPU contention
2026-measurement-check.py <repo>/part2/5.slots --plain     # without section 4's rules
cpu-arbitration-model.py <measurement-repo>                # the CPU's own slots
```

Cycle numbers throughout are openMSX's, which are based on /RAS. The `.txt` analyses in
the measurement repository are based on /CAS, which normally follows /RAS by one cycle —
but not for two slots per table, see section 2.

## 1. The model under test

openMSX implements exactly the model Wouter derived in 2013 (`slots3.txt`):

* VRAM access slots are at fixed positions in a display line; which positions depends
  on the screen mode and on whether display and sprites are enabled
  (`VDPAccessSlots.cc`).
* A slot is allocated ~16 cycles in advance. If a CPU request is pending at that
  moment the CPU gets the slot; otherwise the command engine gets it if it has a
  request pending; otherwise the slot goes unused.
* The engine posts its request for access *k+1* a fixed number of cycles after access
  *k* completed. That number depends on the command and on which step of the command
  it is.

openMSX folds the arbitration latency into the per-step number, so one `Delta` value
per command step covers both:

> next access = the first access slot at or after (previous access + delta)

For a single observed pair of consecutive engine accesses (at *p* and *n*), that model
holds for every delta in `[prevSlot(n) - p + 1, n - p]`. Intersecting those intervals
over every observation of one step gives the step's admissible band. An empty band
means no single value can reproduce the data — the interesting case.

## 2. The access slot tables are confirmed, measured from /RAS

`part2/README.md` proposes moving two slots per table one cycle later. openMSX's tables
need no change, and the apparent disagreement is a difference of convention, not an
error on either side — see the end of this section.

The published `.txt` analyses derive access times by interpolating between the eight
DRAM refresh accesses of a line, which is only eight anchors per line. `/RAS` is a much
better time base: it pulses once per potential access slot whether or not the VDP uses
the slot, and its pattern repeats every display line. `vcd-slot-grid.py` in this
directory measures the grid directly from the raw `part2/1.vcd` captures:

* **Line period.** Exactly one gap per line is larger than all the others, so the sample
  distance between successive occurrences of it is the line period. Fitted per capture
  this gives 5131.13 to 5131.18 samples, very stable. (Taken literally that makes a VDP
  cycle 46.89 ns where 21.477 MHz is 46.57 ns, so the analyser runs about 0.7 % above
  its nominal 80 MHz, or the machine's crystal is off by that much. It cancels, because
  everything is normalised by the measured period.)
* **Numbering.** The refresh accesses are 128 cycles apart with the row address
  incrementing by one and the bank alternating, and between the last of one line and the
  first of the next there are 472 cycles, so the chain has a unique start. That one is
  at cycle 284 — the slot openMSX omits between 276 and 292.
* **Precision.** Over 40 captures each slot is averaged over at least 249 RAS edges with
  a standard deviation of at most 0.31 cycles, so each mean is good to ±0.02.

Result:

| mode | slots per line | gap structure | worst deviation of an openMSX slot |
|---|---|---|---|
| display off | 166 | 163 × 8 + 2 × 10 + 1 × 44 = 1368 | 0.013 cycles |
| sprites off | 132 | 6 × 66 + 8 × 29 + 10 × 2 + 12 + 20 × 32 + 24 + 44 = 1368 | 0.025 cycles |
| sprites on | 126 | irregular (sprite fetches) | 0.055 cycles |

All 154 / 88 / 31 openMSX slots are confirmed. Two further checks that the numbering is
not off by one: openMSX's display-off table is *exactly* the measured 166-slot grid minus
the eight refresh slots (284 + 128k) and the four dummy-read slots (1236, 1244, 1252,
1260), and shifting the anchor by one cycle makes both of those holes stop lining up.

Of the 104 211 accesses in the published `5.slots` files, only 1 108 (1.06 %) sit at a
cycle that is not a /RAS slot, and they are exactly the two slots of the proposed
correction (display off CAS 1326 and 1336, sprites off CAS 1324 and 1334; sprites on has
none). So the retiming is otherwise correct to the cycle, including across the
1181 → 285 stretch where there are no refresh accesses to interpolate between.

**Why the two conventions disagree there.** /CAS normally follows /RAS by one cycle. For
exactly those slots it follows by two:

| mode | single-/CAS accesses | with a 1-cycle /RAS → /CAS delay | with 2 cycles |
|---|---|---|---|
| display off | 4 862 | 4 839 | 23, all at RAS 1324 and 1334 |
| sprites off | 4 382 | 4 336 | 46, all at RAS 1322 and 1332 |
| sprites on | 8 036 | 8 036 | none |

(Burst accesses — one /RAS with several /CAS — are excluded; those are the display
fetches.) So openMSX's /RAS-based 1324 / 1334 and the measurement repository's /CAS-based
1326 / 1336 describe the same two slots, and the only wrong assumption was that the
conversion between them is a uniform one cycle. `2026-measurement-check.py` subtracts two
rather than one for those slots.

That the extra /CAS delay lands on exactly the two slots that start the 10-cycle gaps is
not a coincidence: they are the stretched memory cycles of section 4.1.

## 3. Results

508 traces without CPU activity yield 76 766 engine-to-engine transitions. Under the
plain model — the delay counted in VDP cycles — six steps have no value that fits, and
the three sprites-on steps in bold are disjoint from the other two modes:

| command | step | display off | sprites off | sprites on |
|---|---|---|---|---|
| HMMV | W → W        | [45,48]  | [45,48]  | [33,52]  |
| HMMV | newline      | [101,104]| [103,104]| [97,116] |
| LMMV | R → W        | [20,24]  | [21,24]  | [19,26]  |
| LMMV | W → R        | [69,72]  | [71,72]  | [71,78]  |
| LMMV | newline      | [133,132]! | [133,132]! | [133,148] |
| YMMM | R → W        | [20,24]  | [21,24]  | [9,26]   |
| YMMM | W → R        | [37,40]  | [34,38]  | [33,52]  |
| YMMM | newline      | [101,104]| [103,104]| [97,118] |
| HMMM | R → W        | [20,24]  | [21,24]  | [9,26]   |
| HMMM | W → R        | [61,60]! | [61,60]! | [59,64]  |
| HMMM | newline      | [125,128]| [129,128]! | **[133,134]** |
| LMMM | Rsrc → Rdst  | [29,32]  | [28,32]  | **[33,52]** |
| LMMM | Rdst → W     | [20,24]  | [21,24]  | [9,26]   |
| LMMM | W → Rsrc     | [61,60]! | [61,60]! | [59,64]  |
| LMMM | newline      | [125,128]| [129,128]! | **[131,148]** |
| LINE | R → W        | [18,24]  | [21,24]  | [9,26]   |
| LINE | W → R        | [85,84]! | [85,84]! | [79,90]  |
| LINE | newline      | [117,120]| [119,122]| [117,128]|

`!` marks a band that is empty by one cycle. `newline` is the step from the last write
of one line of the rectangle to the first read of the next. Section 5 shows that all of
the `!` cases and all three of the bold ones come from two mechanisms, not from noise,
and that fixing the first collapses the second.

## 4. What the model needs

Two refinements to the plain model account for everything but 32 of the 76 766
transitions; the third, in section 6, accounts for those.

### 4.1 The delay is counted in memory cycles, not VDP cycles

The measured display-off grid is 8-cycle regular except for two gaps of 10 in the
blanking region — `... 1316 1324 1334 1344 ...` — i.e. **two memory cycles per line are
stretched by two cycles each**. Every one of the six `!` bands in section 3 is a pair of
observations that straddle that stretch differently. The clearest case, HMMM's write →
read, decoded straight from `scr5-dispOff-hmmm-noCpu-1.vcd` with no interpolation:

```
  80 R 1bd55    104 W 1fd55    164 R 1bd56      W -> R = +60
1268 R 1bd62   1292 W 1fd62   1360 R 1bd63      W -> R = +68
```

From 1292 there *is* a slot at +60 (1352) and the engine skipped it; from 104 the slot at
+60 (164) was taken. The difference is that 1292 → 1352 crosses both stretched memory
cycles and 104 → 164 crosses neither.

> If the engine's delay counter does not see the stretch — i.e. it counts memory cycles
> rather than VDP cycles — the conflict disappears. 1292 → 1352 is then 56 engine cycles,
> so a minimum of 57..60 skips it and takes 1360 (64), while 104 → 164 is 60 either way.

The 44-cycle gap is *not* a stretch: it is five memory cycles that offer no slot, and it
counts in full, which is what HMMV needs (from 120 a 45..48 minimum skips 164 at +44 and
takes 172 at +52).

Note the counter has to be read as restarted at every access: the correction loses four
cycles per line, so it is well defined for an interval but not as a periodic remapping of
the line. That is exactly how the engine behaves — it is a delay from the previous
access, not a position in the line.

### 4.2 With sprites on, every delay is a cycle or two longer

With 4.1 in place, every step becomes internally consistent in every mode, and the steps
that still need a sprites-on distinction all need the *same* extra amount. So one rule
replaces the three per-command special cases: **with sprite rendering active every
command-engine delay is one or two cycles longer.** It reads like the arbitration decision
for a slot being taken earlier while the sprite fetch logic is active — the CPU
arbitration wants something similar (section 8.2). It cannot be a slot-position effect:
shifting the sprites-on slots would leave intervals within that mode unchanged.

The measurements admit 1 and 2 and cannot separate them, because the sprites-on bands are
the widest of the three modes. openMSX uses 1. Either value forces LMMM's
source→destination read and the line transition to the round 32 and 128 that the other
two modes already want.

### 4.3 The resulting values

| command | step | admissible | chosen | openMSX before |
|---|---|---|---|---|
| HMMM, LMMM (dst), LMMV, YMMM, LINE | R → W | 21..24 | 24 | 24 |
| HMMM, LMMM | W → R(src) | 59..60 | **60** | 64 |
| LMMM | R(src) → R(dst) | 27..32 | **32** | 32, and 48 with sprites on |
| HMMV | W → W | 45..48 | **46** | 48 |
| LMMV | W → R | 71..72 | **72** | 72 |
| YMMM | W → R | 33..38 | **36** | 38 |
| LINE | W → R | 81..84 | **84** | 88 |
| HMMV | newline | 103..104 | **104** | 104 |
| YMMM | newline | 103..104 | **104** | 104 |
| LINE | newline | 119..120 | **120** | 120 |
| HMMM, LMMM | newline | 125..128 | **128** | 128, and 134 with sprites on |
| LMMV | newline | 129..132 | **130** | 134 |

The values in the `chosen` column are Wouter's, picked so that each command has one
per-line overhead that all of its steps share: HMMV 46 + 58 and 104 = 46 + 58, LMMV
24 + 72 + 58, YMMM 24 + 36 + 68, HMMM 24 + 60 + 68, LMMM 32 + 24 + 60 + 68, LINE
24 + 84 + 36. Those constraints have almost no freedom left in them: a shared *even*
overhead for the two fills forces 58 and with it HMMV 46 and LMMV 72, a shared even
overhead for the three copies forces 68 and with it YMMM 36 and HMMM/LMMM 60. Only
R → W ∈ {22, 24} and LINE ∈ {84 + 36, 82 + 38} are left, and both are settled by
preferring the multiple of 4. The fills and the copies cannot share one overhead --
[57,59] and [68,69] are disjoint -- and no assignment makes every value a multiple of 8.

| model | mispredicted of 76 766 |
|---|---|
| openMSX today | 456 (0.594 %) |
| 4.1 + 4.2, with the deltas above | 32 (0.042 %) |
| and section 6 | **0** |

The rules go together: the deltas above *without* 4.1 give 1970 (2.6 %). The same model
gets the 2013 set right as well -- 3965 transitions, 0 wrong -- once the truncation of
`screen5screenoffHMMVnocpu.txt` is taken into account.

Both rules are free at run time: they only change how `VDPAccessSlots.cc` builds its
per-cycle lookup tables — the stretch as a correction inside the table generator, the
sprites-on cycle as a per-table offset — and they remove the mode conditionals from the
command implementations entirely. See the `Timing` struct in `VDPAccessSlots.cc`. The character, text and MSX1 tables are left alone, having never
been measured this way.

`vdpcmdx` improves as well, which it did not have to, since none of this was fitted to
it. Active area, non-CPU columns, before → after (real hardware in brackets):

| cell | before | after | real |
|---|---|---|---|
| HMMM portrait `NO SPR`  | 2636 | **2658** | 2659 |
| HMMM landscape `NO SPR` | 2668 | **2672** | 2673 |
| LMMM landscape `NO SPR` | 1970 | **1971** | 1971 |
| LMMM portrait `NO SPR`  | 1954 | **1955** | 1955 |
| YMMM landscape `NO SPR` | 3796 | **3797** | 3797 |
| YMMM landscape `NO SCR` | 3994 | 3995 | 3996 |

Every non-CPU cell is now within 3 pixels of the reference. Nothing else in the non-CPU
columns changed.

### 4.4 Where the padding is, per mode

The stretch of 4.1 is the VDP padding its line out to 1368 cycles. Measured from /RAS over
the whole blanking region, not just the slots the command engine can use, the total is 4
cycles in every bitmap mode, but it is not distributed the same way:

| mode | ordinary spacing there | padded memory cycles | total |
|---|---|---|---|
| display off | 8 | two of 10, completing at 1334 and 1344 | +2 +2 |
| sprites off | 8 | two of 10, completing at 1332 and 1342 | +2 +2 |
| sprites on | 13, 6, 10 | 15, 7, 11, completing at 1330, 1337, 1348 | +2 +1 +1 |

The sprites-on row was guesswork until the R#9 S1/S0 captures arrived, because in that
mode the padded cycles are sprite fetches rather than command slots. Section 10 shows how
those captures settle both the positions and the total.

Getting it right matters more for the positions than for the total: with the sprites-on
padding assumed to sit at 1332 and 1342 like the other two modes, 25 of the 16416 entries
of the sprites-on lookup table come out differently. The command traces cannot separate
the two — both reproduce every transition — but the /RAS comb can.

## 5. History: the parameter-only fix

Superseded by section 4, kept because the `vdpcmdx` comparison below is the only
end-to-end check of the whole chain. Two of openMSX's values were outside the admissible
band of the plain model and were corrected first:

| step | was | now | plain-model band |
|---|---|---|---|
| YMMM `W → R` | 36 | 38 | [37,38] |
| LMMV newline | 136 (= 72 + 64) | 134 (= 72 + 62) | [133,134] |

and the sprites-on deviation known from LMMM's destination read applies to the
line-transition step of both HMMM and LMMM as well, so that step used 134 instead of 128
when sprite rendering was active. Under section 4 all three become consequences of the
two table properties rather than separate facts.

`vdpcmdx` barely notices, as expected. Measured on a Philips NMS 8255, the ACTIVE block,
non-CPU columns, before → after:

| cell | master | patched | real |
|---|---|---|---|
| LMMM landscape `NORMAL` | 1340 | **1339** | 1339 |
| LMMM portrait `NORMAL`  | 1333 | 1331 | 1332 |
| HMMV landscape `NORMAL` | 4003 | 4002 | 4005 |
| YMMM portrait `NORMAL`  | 2093 | **2094** | 2095 |

and every other non-CPU cell is unchanged. The line-transition step happens once per
rectangle line, so it is worth well under a pixel per frame; and shifting the engine by
one 32-cycle slot at a line transition mostly just moves it to another phase of the same
128-cycle pattern, at the same throughput. An offline simulation of openMSX's own model
over the 192-line active window gives the same answer: HMMM ±0, LMMM landscape −1. The
HMMV and YMMM cells above cannot be affected by any of the three changes, so those two
±1 differences are cross-talk: `vdpcmdx` runs its tests back to back, so a change in one
test shifts the phase at which the next one starts. Two runs of the same binary are
bit-identical, so this is not emulator non-determinism.

The `NORMAL+CPU` column moves by up to 190 pixels in one cell for the same reason,
amplified: with the CPU taking a slot every 72 cycles the engine's slot selection is
chaotically sensitive to its starting phase. That column cannot judge this change (nor,
as Wouter has pointed out, can its hardcoded `REAL` values judge anything at the
few-percent level).

So the justification for these changes is the bus captures, not the aggregate frame
counts.

## 6. Slots that follow another slot after only 6 cycles

The sprites-off display area has its slots in pairs 6 cycles apart, at offsets
{0, 6, 32, 38, 64, 70, 96} of every 128 cycles. Reading the raw captures, the pair is
the *freed sprite fetch* and the memory cycle that is spare in both modes: with sprites
on, the cycle at 182 + 32k fetches sprite data and the one at 188 + 32k is free; with
sprites off, both are free. 25 of the 88 sprites-off slots are the second of such a
pair. Neither the display-off nor the sprites-on table has any.

Two independent effects follow from that, and both are needed:

**The command engine pays one cycle.** A step whose *previous* access was in one of
those 25 slots takes one cycle longer, as if that memory cycle -- squeezed against its
predecessor -- had finished one cycle late. 1778 transitions in the 2026 set start from
such a slot: 32 of them (HMMM's and LMMM's line transition from cycle 188) need the extra
cycle, and 1281 of them rule out a third cycle. So the band is exactly [1, 2]; openMSX
adds 1. Without it those 32 are the only mispredictions left in the whole set.

**The CPU never gets one.** Of 4550 CPU accesses in the sprites-off captures of both
measurement sets, **zero** are in one of those 25 slots, while all 63 other slots are
used. That is not a coincidence of the request phase: with the requests arriving 71.5
cycles apart against a 32-cycle slot pattern, an unconstrained model puts 11 % of them
there.

What happens instead is visible in the traces. Every unclassified read of `0x1ffff`
outside the four-slot dummy-read block of a normal line -- 335 of them, in the
sprites-off CPU captures and nowhere else -- sits in one of those 25 slots and is
followed by a CPU access in the next slot (322 at +26, 10 at +54). So the VDP *does* give
the slot to the CPU; it just cannot carry a CPU access, spends the memory cycle on a
dummy read, and does the access one slot later.

That also explains the last of the engine mispredictions in the CPU captures: the engine
was not skipping a free slot, the slot was occupied by that dummy read.

## 7. Traces that were mislabelled

`scr5-dispOff-hmmv-noCpu-nx2-6d`, `scr5-dispOff-lmmv-noCpu-nx2-3d`,
`scr5-dispOff-ymmm-noCpu-nx12-1d`, `scr5-sprOff-lmmm-noCpu-nx2-3d` and
`scr5-sprOff-lmmv-noCpu-nx8-5d` were named `noCpu` but contained CPU accesses. Before
excluding them they were the *only* source of inconsistency in the middle of a display
line -- every other anomaly is at the blanking boundary or is the sprites-on effect of
section 4.2. They have since been fixed or renamed to `scr5-wrong-*` in the measurement
repository, and the `d` sets have been relabelled.

Two accesses in the CPU captures still carry the wrong code, and they are the only
remaining mispredictions of the engine model there:

* `scr5-sprOn-hmmm-rdCpu-1.txt` has no `R.r` at all: the CPU's reads (0x1849a upwards)
  are coded `R.s`, together with the command's own source reads (0x1a1c2 upwards).
* `scr5-sprOn-hmmv-wrCpu-3.txt` has one `R.s 19e91` at cycle 4452 which belongs to the
  CPU's write sequence (…19e90, 19e91, 19e92…) -- HMMV has no source read.

One labelling subtlety is not a mistake: the unclassified read of `0x1ffff` is the VDP's
dummy read in most traces, but in a few YMMM captures the command's own source pointer
walks through the top of VRAM and produces genuine command accesses at that address. The
check script distinguishes the two by looking at the rest of the trace.

## 8. The CPU side

### 8.1 The engine model under contention

The `rdCpu` / `wrCpu` traces give 12 302 engine transitions with CPU accesses about 71.5
cycles apart, and the CPU takes the slot the engine wanted in **30 %** of them. Applying
openMSX's rule

> the engine takes the first slot at or after its minimum that the CPU did not take
> (`VDPCmdEngine::stealAccessSlot`)

to the observed CPU access times:

| | wrong |
|---|---|
| openMSX today | 90 (0.732 %) |
| the model of sections 4 and 6 | **3 (0.024 %)** |
| the same, but the loser re-arms its full delay instead of taking the next slot | 3086 (25.1 %) |

and the remaining 3 are the two mislabelled accesses of section 7. So `stealAccessSlot`
is right, including the detail that a command engine which loses a slot takes the *next*
slot rather than re-arming its full delay. The hypothesis floated earlier in
[sndpl/openMSX#5](https://github.com/sndpl/openMSX/pull/5) -- that under contention real
hardware uses slots closer together than the command's own minimum spacing -- is
refuted.

### 8.2 The CPU's own model

`cpu-arbitration-model.py` fits the other half: which slot a CPU port access ends up in.
The two measurement sets are complementary. The 2013 machine (NMS 8250) has a single
clock, so the port accesses sit on an exact grid with one unknown phase per capture. The
2026 machine (NMS 8280) has separate CPU and VDP clocks, but /CSR and /CSW were recorded,
so the pace of the port accesses is measurable.

The model:

1. **A constant delay** between the port access and the moment the arbiter sees the
   request. This is wall-clock time -- it is a pin-to-register delay, not something the
   memory sequencer counts -- and it is not separable from the fitted phase, so it is
   simply absorbed into it.
2. **A lookahead of 16 memory cycles at the arbiter.** The CPU gets the first eligible
   slot at least that far away, counted in the VDP's memory cycles with exactly the same
   padding subtraction as the command engine's delays.
3. **One request register.** A port access arriving while it is occupied is lost -- no
   VRAM access happens for it, and the VDP's VRAM pointer does not advance, so the CPU
   sees the previous byte again. It stays occupied until 2 cycles after the access.
   openMSX models this (`VDP::scheduleCpuVramAccess`, `tooFastCallback`); what the
   measurements add is that the *old* request wins, which is also what openMSX does.
4. A slot that follows another slot after 6 cycles is decided 2 cycles earlier than the
   rest, and carries the dummy read of section 6 instead of the CPU's access.

The 2013 set constrains the lookahead and the dead window only through their sum: any
(lookahead, dead window) with lookahead + dead = 18 and lookahead ≤ 16 fits it exactly.
16 + 2 is the top of that band and matches the 16 cycles openMSX already uses
(`Delta::CPU_16`).

Fit:

| set | CPU accesses | not predicted | predicted but absent |
|---|---|---|---|
| 2013, exact grid, one phase per capture | 1141 | **0** | **0** |
| 2026, fitted pace, one offset per capture | 19733 | 158 (0.80 %) | 126 |

For the 2026 set the requests are not taken from the individual /CSx edges but from a
straight line fitted through the whole capture: the Z80's own crystal makes the port
accesses a perfectly regular sequence in Z80 time, and one line through ~115 of them has
a residual of 0.15 cycles, well under the analyzer's 0.27-cycle sampling step. The fitted
rate is **5.96113 ± 0.00004 VDP cycles per Z80 T-state**, where a single-clock machine
would give exactly 6 -- the Z80 of that NMS 8280 runs 0.65 % fast relative to its VDP.
Two different test programs (40 port accesses 12 T-states apart, and 20 of them 37
T-states apart) give the same rate to 1 part in 10^5.

The VDP can only act on an asynchronous /CSx at one of its own clock edges, so the request
times are rounded up to whole VDP cycles. That is worth something: rounding to 1 cycle
beats no rounding, rounding to 2, and rounding to 4, in that order. So the VDP samples
/CSx on a 1-cycle grid.

Per pace and mode, with the constant of point 1 fitted per capture. The four loops are
`IN`/`OUT` x40 (72 cycles apart), `IN ; NOP ; NOP` (132), `IN` x20 slow (222), and
`IN ; OUT` alternating (r/w):

| pace | mode | accesses | not predicted | lost requests | constant |
|---|---|---|---|---|---|
| 72 | display off | 7729 | 74 (0.96 %) | 0 of 7833 | 9.86 ± 0.12 |
| 72 | sprites off | 6378 | 24 (0.38 %) | 22 of 6443 | 9.86 ± 0.13 |
| 72 | sprites on | 3703 | 57 (1.54 %) | 305 of 4022 | 12.05 ± 2.01 |
| 132 | display off | 368 | 1 (0.27 %) | 0 of 374 | 9.97 ± 0.08 |
| 132 | sprites off | 374 | 1 (0.27 %) | 0 of 376 | 9.62 ± 0.35 |
| 132 | sprites on | 991 | 2 (0.20 %) | 0 of 1000 | 11.40 ± 0.55 |
| 222 | display off | 675 | 1 (0.15 %) | 0 of 685 | 9.75 ± 0.22 |
| 222 | sprites off | 608 | **0** | 0 of 610 | 9.72 ± 0.30 |
| 222 | sprites on | 642 | 2 (0.31 %) | 0 of 646 | 10.55 ± 1.12 |
| r/w | display off | 681 | 10 (1.47 %) | 0 of 691 | 9.83 ± 0.13 |
| r/w | sprites off | 568 | 1 (0.18 %) | 1 of 577 | 9.92 ± 0.20 |
| r/w | sprites on | 636 | 2 (0.31 %) | 50 of 690 | 11.86 ± 0.26 |

**The constant is a per-mode number**: 9.8 ± 0.1 with the display off and with sprites
off, across four loops and three CPU paces, and about 1.9 cycles more with sprites on.
That is the same direction and about the same size as the command engine's extra cycle
in that mode.

**Reads and writes are the same.** The `r/w` captures alternate `IN A,(#98)` and
`OUT (#98),A`, so a difference between the two would show up as every other request being
misplaced. Fitting a separate constant for writes puts the difference at **0.0 cycles**,
with 13 mispredictions of 1885 at 0.0 against 32 at −1 and 52 at +1: the same delay to
within about a third of a cycle.

The 222-cycle captures are the ones with a slow CPU loop, made specifically to remove the
lost requests, and they do: 3 of 1924 accesses mispredicted, and one whole mode exact.
The constant repeats to ±0.01 cycles between display-off and sprites-off and to ±0.15
between the two paces, which is a good check on the whole chain -- the analyzer time base,
the slot grid, the pace fit and the model. Sprites-on wants 1 to 2 cycles more, in the
same direction as the command engine's extra cycle in that mode, but with a much larger
scatter because that mode has the fewest slots per line.

Of what is left, about a sixth are the *first* access of their capture, some fifteen times
the rate the remaining positions show. That is the one thing the model cannot know: a
capture starts at an arbitrary point in the test program, so there may already be a
request in flight. Almost all the others are isolated single slots.

## 9. What would help next

* **Command startup cost.** Still never measured. It is invisible to `vdpcmdx` (one
  command per frame of 10240 pixels). `part2` contains three `*-start-*` captures that
  reach step 4 but not `5.slots`; without knowing what the capture was triggered on they
  cannot be turned into a number, but if that is recoverable they would be the first
  direct measurement of it. Otherwise: sync to the line interrupt, burn a programmable
  number of cycles, start a one- or two-pixel command, poll S#2 once. The same rig would
  test section 4.1 directly, since it predicts that a command whose next access lands
  just after the padding behaves differently from one that does not cross it.
* **Alternating read and write.** Nothing in either set can tell whether a read and a
  write reach the arbiter with the same delay: each capture is all reads or all writes,
  so any difference disappears into the fitted constant. A loop that alternates them
  would separate the two.
* **Sprites-on with a slow loop and more slots.** The sprites-on constant of section 8.2
  is still the loosest number in the model (±1.2 cycles even at the slow pace), simply
  because that mode has 31 slots per line. Splitting the lookahead from the constant
  delay -- the two are only separable where the padding falls inside the lookahead window
  -- would need captures with requests deliberately placed near cycle 1330.
* **The 1365-cycle modes.** Section 10 measures the line length and where the padding
  goes, but openMSX does not implement those modes at all. If it ever does, the
  prediction is that the command engine needs no padding correction there.

## 10. Where the padding comes from: R#9 S1/S0 and R#18, measured

The V9938 data book documents bits S1 and S0 of R#9 as selecting one of three "cycle
modes", and gives 1368 cycles per horizontal line for S1,S0 = 0,0 but 1365 for the other
two settings (section 5 "Cycle mode" and section 7-1 "Horizontal display parameters").
The `scr5-spr{On,Off}-hmmv-noCpu-s{0,16,32,48}-*e.vcd` captures test that directly. Taking
the line period and the refresh interval both in analyzer samples, so that nothing has to
be assumed about either, the line comes out as 128 x period / refresh:

| R#9 | sprites off | sprites on |
|---|---|---|
| s = 0 | 1368.14 ± 0.00 | 1367.97 ± 0.03 |
| s = 16 | 1365.03 ± 0.02 | 1364.98 ± 0.03 |
| s = 32 | 1365.04 ± 0.03 | 1364.95 ± 0.04 |
| s = 48 | — | 1365.01 ± 0.10 |

1368 and 1365 exactly, as documented, in both modes. And the three cycles come off
precisely where section 4.4 says the padding is:

```
sprites off, 1368:  ... 1314 -8- 1322 -10- 1332 -10- 1342 -8- 1350 ...
sprites off, 1365:  ... 1314 -8- 1322  -9- 1331  -8- 1339 -8- 1347 ...

sprites on,  1368:  ... 1315 -15- 1330  -7- 1337 -11- 1348 -6- 1354 ...
sprites on,  1365:  ... 1315 -14- 1329  -6- 1335 -10- 1345 -6- 1351 ...
```

**The 1365 line does not remove all the padding.** In sprites-off the pair goes from
10 + 10 = 20 to 9 + 8 = 17, where the unpadded value would be 8 + 8 = 16; in sprites-on
the trio goes from 15 + 7 + 11 = 33 to 14 + 6 + 10 = 30, where the rest of that part of
the line runs 13 + 6 + 10 = 29. So a 1365-cycle line still carries one cycle of padding,
and the 1368-cycle line carries four — in both modes, which is what makes it four in the
sprites-on model as well. (Reading the difference as "the whole padding" would put the
sprites-on total at 3, which is what an earlier version of this document did.)

**Horizontal set-adjust moves the padding but never its total.** All 16 values of R#18 in
all three modes: the line stays at 1368 to ±0.1 cycles, nothing before cycle ~1290 moves,
and the padding block slides at 4 cycles per unit — one screen-5 pixel — with R#18 0..7
shifting it later and 8..15 (i.e. −8..−1) earlier. Even values keep the two-of-+2 shape,
odd values sit half a memory cycle away and spread the same 4 cycles as +1, +2, +1:

```
R#18 = 0:  1334:+2  1344:+2          R#18 = 1:  1333:+1  1343:+2  1352:+1
R#18 = 2:  1342:+2  1352:+2          R#18 = 3:  1341:+1  1351:+2  1360:+1
R#18 = 8:  1302:+2  1312:+2          R#18 = 9:  1301:+1  1311:+2  1320:+1
```

The total is invariant at +4, which is an independent confirmation of the paragraph above.
openMSX has one table per mode with the padding at its R#18 = 0 position, so a program
that changes the set-adjust gets the padding in the wrong place — worth at most 4 cycles
on a command step that crosses it, and it would cost 16 tables per mode to fix.

## 11. The command start delay, all six commands

`scr5-{mode}-{cmd}-noCpu-nx4-ny2-*h` restarts a 4x2 command in a loop, so the interval
between the `OUT` that commits R#46 and the command's first VRAM access is visible. Each
launch gives an interval: the upper bound is the distance to the access that happened, the
lower bound the distance to the command slot before it, which the command did not take.
Distances are in memory cycles; the /CSW edge is rounded up to a whole cycle, as a CPU
port access is.

| command | first access | display off | sprites off | sprites on | value | launches |
|---|---|---|---|---|---|---|
| LMMM | R source | 46 | 46..47 | 42..48 | **46** | 49/49 |
| LMMV | R dest | 70 | 70..72 | 72..73 | **70** | 46/50 |
| HMMM | R source | 82 | 80..83 | 83..84 | **82** | 48/50 |
| YMMM | R source | 82 | 83..84 | 81..82 | **82** | 52/53 |
| HMMV | W dest | 94 | 93..95 | 91..96 | **94** | 54/54 |
| LINE | R dest | 94 | 93..95 | 92..99 | **94** | 52/52 |

Display-off pins each value to a single integer, as expected from its 8-cycle slot
spacing; the other two modes are consistent with it but looser. The handful of launches
that reject the chosen value are ones whose /CSW edge fell close enough to a cycle
boundary to round the other way. **The startup cost does not depend on the display mode.**

### A decomposition

The four distinct values are 46, 70, 82 and 94 — all **congruent to 10 modulo 12**, i.e.

```
S0 = 10 + 12 * N        N = 3 (LMMM), 5 (LMMV), 6 (HMMM, YMMM), 7 (HMMV, LINE)
```

Both parts connect to something already measured:

* **the 10** is the constant delay between a port access reaching the pin and the
  arbiter acting on it, which section 8.2 measures independently from CPU accesses as
  9.86 ± 0.12 cycles. So the command start pays the same pin-to-arbiter delay a CPU
  request does.
* **the 12** is the step the engine's own delays are built from: five of the seven
  inter-access delays in section 4.3 are multiples of 12 (R → W 24, YMMM 36, HMMM and
  LMMM 60, LMMV 72, LINE 84). The exceptions are HMMV's 46 and LMMM's 32.

What N counts is not clear. It is not the number of accesses per unit (1, 2, 2, 2, 3, 2
against N = 7, 5, 6, 6, 3, 7), nor the delay of the step that would precede the first
access in steady state. Four distinct values all landing on the same residue mod 12 is
unlikely by chance — 1 in 1728 for the three after the first — so the 12-cycle step is
real even if what it counts is not identified.

In an emulator's port-write coordinate, add the 3 T-states from the timestamp to the /CSx
rising edge (18 cycles):

```
LMMM 64   LMMV 88   HMMM 100   YMMM 100   HMMV 112   LINE 112
```

openMSX currently uses `Delta::D0` for all of them, i.e. it starts every command about
5 to 14 display-off slots too early.

## 12. The sprite-fetch line, and what openMSX gets wrong there

`scr5-dispOff-sprOn-stop-noCmd-1f` and `scr5-sprOn-dispOff-stop-noCmd-1f` capture the
vertical border boundary. Sprite attributes for a display line are fetched during the
*previous* line, so the access pattern cannot simply follow the display area. Counting
what each line actually does -- burst reads (real bitmap fetches), dummy reads of 0x1FFFF,
and ordinary single accesses:

| line | bursts | dummies | singles | |
|---|---|---|---|---|
| last border line | 8 | 34 | 44 | sprites-on comb, bitmap fetches dummied out |
| display lines | 48 | 2 | 48 | |
| last display line | 41 | 45 | 12 | bitmap still fetched, sprite slots dummied out |
| first border line | 0 | 16 | 1 | display-off comb |

So the line before the display area already runs the sprites-on access pattern, with its
32 bitmap slots spent on dummy reads, and the last display line still fetches bitmap while
dummying out every sprite slot.

**Where the grid changes.** An earlier version of this section read the comb as changing at
the line boundary. That is wrong, and per-line calibration (each line fitted on its own
eight refresh pulses, which the measurement repository's `lines.py` does) shows it plainly.
The line before the display area:

```
    6-  14-  22- ...  110-  118-   162-  170-  182a  188-  194.  214a  220- ...
```

-- the left horizontal blanking runs the *sprites-off* comb, and from 162 on the line is
identical to a display line except that its bitmap slots hold dummy reads. And the line
after the display area:

```
    4.  17.  30-  36.  46. ...  116.  126.   164-  172-  180-  188- ...
```

-- the sprites-on sprite-fetch comb up to 126, all of it dummy, and the display-off comb
from 164 on. So the access grid changes **at the same point in the line at both ends**,
somewhere in (126, 162]: the last left-comb access and the first slot of the new grid
bracket it, and the two tables happen to agree at 162 and 170, so any switch point in that
window behaves identically.

That is a different line origin for the access grid, not a different number of lines. In
slots available to the command engine and the CPU:

| line | hardware | openMSX | switching one whole line early | switching at 162 |
|---|---|---|---|---|
| the line before the display area | 44 | 154 | 31 | 44 |
| the line after the display area | 140 | 154 | 154 | 140 |

openMSX's `getTab()` keys on `isDisplayEnabled() = isDisplayArea && displayEnabled`, so it
returns `tabScreenOff` for both, and gives a command 110 slots too many on the line before
the display area -- for one line per frame, a command there runs several times too fast.
Switching a whole line early is most of the fix; switching at cycle 162 of the line before
and of the line after is the whole of it, and is no harder, because the sync point that
changes the table already exists (`VDPVRAM::updateDisplayEnabled()` syncs the command
engine) and only its moment has to move.

One detail even that does not capture: the left blanking of the line before the display
area runs the sprites-off comb (6, 14, ... 118), not the display-off one (0, 8, ... 120) --
the same number of slots six cycles later, because that line fetches no sprite patterns for
the border line above it. Not worth a fourth table.

## 13. How well the lookahead and the pin delay separate

The lead from the /CSx edge to the granted slot is about 26 cycles, made of a wall-clock
pin delay and a lookahead counted in memory cycles. Only the lookahead sees the padding,
so the two are separable only on requests whose window straddles it — which is what the
`IN ; NOP ; NOP` loop supplies: 22 T-states is near-coprime with the 128-cycle refresh
cell, so one capture creeps through every phase instead of visiting two or three.

Holding the sum at 26 and scanning the split (the constant is refitted per capture each
time):

| lookahead | 0 | 4 | 8 | 12 | 14 | 16 | 18 | 20 | 24 | 28 |
|---|---|---|---|---|---|---|---|---|---|---|
| the 132 loop, of 1733 | 11 | 6 | 6 | **4** | **4** | **4** | 6 | 6 | 6 | 10 |
| all display-off, of 4888 | 104 | 93 | 93 | 75 | 75 | 75 | **68** | **68** | **68** | 83 |

So the split is real: a lookahead of 0 (all of it wall-clock, no padding seen) and one of
28 (all of it counted in memory cycles) are both excluded, from both directions. The
optimum is broad and the two corpora disagree slightly about where it sits — 12..16 on the
designed experiment, 18..24 on the larger display-off set, a difference of 7 accesses in
4888. The 2013 set is the sharp constraint: there the lookahead must be at most 16, and 17
already costs 10 of 1141. 16 is the value that satisfies all three, and it is what openMSX
already uses.

**Horizontal set-adjust is very nearly invisible to the CPU.** Moving the padding with
R#18 should move the CPU's granted slot for the few requests whose window straddles it.
Fitting the `stop-rdCpu-adjust*` captures with the padding at its R#18 = 0 position and
with it shifted by 4 cycles per unit: 26 versus 22 mispredictions over 1837 display-off
accesses, and 22 versus 19 over 1830 sprites-off ones. The shifted version is better in
both, which is the direction the model predicts, but seven accesses out of 3667 is not a
measurement.

## 14. A trap in finding the refresh chain

Everything here is anchored on the refresh accesses: they are 128 cycles apart, the row
address increments and the bank alternates, and the gap before the first of a line is 472.
Row and bank alone are not enough. In screen 5 an ordinary access can supply a false link,
and one false link moves the anchor by a refresh cell or, in one capture, by a whole line —
which is invisible in slot arithmetic, because 128 is a multiple of the slot spacing, but
wrong for anything that compares VRAM accesses with the /CSx pins.

Requiring a single /CAS as well (bitmap fetches are bursts) fixes it: 16 of 1051 captures
are anchored differently with and without that test, and the CPU model improves from
0.93 % to 0.75 % mispredicted.

What does *not* work is requiring a constant column address along the chain. The column
is not constant: it carries when the row counter wraps, e.g.
```
row 7c col 7f   row 7d col 7f   row 7e col 7f   row 7f col 7f   row 80 col bf   row 81 col bf
```
so that test breaks the chain in the middle and makes matters worse. The VDS pin would be
the clean discriminator if one is needed.

## 15. Reconstructed requests: how much freedom the trellis really has

The residual of section 8 -- 175 of 23353 CPU accesses mispredicted -- is not a model
error. It is what is left of the uncertainty in *when* each request was made: `/CSx` is
sampled at 80 MHz, the time base is interpolated between refresh cycles, and the arbiter
acts on an integer VDP cycle, so an edge that falls near a cycle boundary can arm either
side of it. Section 13 removes most of that by fitting the Z80's own instruction grid
rather than each edge; the measurement repository removes the rest with a Viterbi-style
search over the per-edge assignments (`TRELLIS_CPU_REQUESTS.md`).

Such a search deserves to be treated with suspicion: enough per-edge freedom will fit
anything. These are the numbers that say it does not have enough.

**What it uses.** One real pin delay `phi` shared by every edge of every capture, plus a
per-edge tolerance `eps` that the search minimises. Over the 266 captures and 24597
recorded `/CSx` edges:

| `eps` | in analyzer samples | captures reconstructed exactly | edges moved | one shared `phi` |
|--|--|--|--|--|
| 0 | 0.00 | 174 / 266 | 0 / 24597 | 114 / 266 |
| 0.05 | 0.19 | 195 / 266 | 30 | 147 / 266 |
| 0.134 | 0.50 | 220 / 266 | 83 | 190 / 266 |
| 0.268 | 1.00 | 255 / 266 | 151 | 224 / 266 |
| 0.5 | 1.86 | 264 / 266 | 142 | 263 / 266 |

Each row is one run with `eps` held at that value. With *no* per-edge freedom at all, one
real number per capture already reproduces every grant in two thirds of the corpus. The
published run instead takes the smallest `eps` that works for each capture: 174 of the 264
need none at all, 78 need up to one analyzer sample, 10 up to one and a half, and 2 need
1.86 -- and over the whole corpus 242 of the 24597 edges (0.98 %) move. The shared `phi`
that covers all three modes is confined to `[10.746, 10.748)` -- two thousandths of a
cycle out of the one-cycle period it could have taken.

The tail is the one place where the published figure leans on more slack than the stated
one-sample uncertainty: twelve captures need up to 1.86 samples. That is not unreasonable
-- the sampling step is not the only error, since the interpolated time base contributes
too -- but the 263 of 266 should be read next to the 224 that a tolerance of exactly one
sample gives.

**That it discriminates.** The stronger evidence is that wrong models fail. Re-running the
same search with one part of the model perturbed, and counting captures reconstructed with
one shared `phi`:

| variant | exact | one shared `phi` | `phi` band |
|--|--|--|--|
| the model as published | 264 / 266 | **263 / 266** | 0.002 wide |
| no sub-slot classes (`NEED` 16/18 by mode only) | 242 | 170 | 0.002 |
| sub-slot classes assigned at random, same count (3 draws) | 174..183 | 126..134 | 0.02..0.16 |
| lookahead 15 instead of 16 | 246 | 227 | 0.030 |
| lookahead 17 instead of 16 | 263 | 255 | 0.014 |
| no threshold classes (uniform release margin) | 258 | 246 | 0.037 |
| release margin 0 instead of 1 | 259 | 254 | 0.002 |
| two-entry request buffer | 209 | **208** | 0.411 |
| padding as four single cycles (the derived positions) | 261 | 247 | 0.002 |

The permutation control is the decisive one: sub-slot classes of exactly the same size,
drawn at random, take sprites-off from 83 of 83 captures down to 15..22. So the search
cannot absorb an arbitrary lattice; it is testing the one that was derived from the die.
The two-entry buffer is excluded as well, which is worth noting because the buffer depth
was the part of the model I had said rested on fitting alone.

**What it cannot see.** Three parts of the model are invisible to this fit, and its score
should not be read as evidence for them:

* the packed-slot dummy read (`-19` versus `-18`). The CPU is never served in a packed
  slot in either formulation, so the boundary does not enter the grant sequence at all;
  changing it moves nothing. The dummy-read census of section 6 remains the only
  measurement that bears on it.
* the sprites-on addend of the command engine: 0, 1 and 2 all score identically here.
* the release margin 1 versus 2, which score identically.

**Independent replay.** The reconstruction is published as one `.cpureq` file of integer
request cycles per capture. Feeding those through the arbiter of
`cpu-arbitration-model.py` -- no fitting anywhere, the request times come from the
reconstruction and the accesses from the `.txt` -- gives:

| | captures exact | accesses | not predicted | spurious |
|--|--|--|--|--|
| display off | 100 / 100 | 9529 | **0** | 0 |
| sprites off | 79 / 83 | 7926 | **0** | 4 |
| sprites on | 60 / 64 | 4924 | **0** | 4 |

Every one of the 22379 observed CPU accesses is reproduced. The eight spurious grants are
one access each in eight captures: four are the same captures the repository's own replay
fails on (its checked-in fixtures predate its final threshold formulation), and the other
four flip to the other side if the release margin is 2 rather than 1 -- the parameter the
corpus does not settle. So the 0.75 % of section 8 was the reconstruction of the request
times, not the arbiter.

The same replay run with a *uniform* release margin -- what openMSX did before the slot
classes were implemented -- loses 31 of the 4924 sprites-on accesses at margin 1 and 67 at
margin 2, and none at all with the display off or sprites off.

## 16. What of this is now in openMSX

| rule | where |
|--|--|
| the three slot tables | `slotsScreenOff` / `slotsSpritesOff` / `slotsSpritesOn` (unchanged) |
| line padding, 4 cycles in measured positions per mode | `Timing::pad` |
| command steps counted in memory cycles | `CycleTable`, `dist()` |
| the sprites-on addend on every command step | `Timing::extra` |
| a step that starts in a tight slot costs one cycle more | `CycleTable`, post-pass |
| the CPU is never served in a tight slot | `CycleTable`, `ok()` |
| the CPU's lookahead counted in memory cycles | `CycleTable`, `ok()` |
| 18 cycles of lookahead for a 'late' slot | `Timing::late` / `Timing::plain` |
| the request is registered 29 cycles after the port access | `VDP::scheduleV99x8VramAccess` |
| one request buffer, released `threshold` cycles before its access | `VDP::scheduleV99x8VramAccess` |
| the command start delay, per command | `Delta::CMD_START_*` |

The generated tables were checked entry by entry against the model in this directory:
103968 table entries over the four V99x8 tables, 304 slot thresholds and 165424
memory-cycle distances all agree exactly.

Not emulated, and why:

* **the packed-slot dummy read.** openMSX keeps the CPU out of a tight slot, which is the
  visible half. The dummy read itself only shows up as a command slot the engine loses,
  in a 3-cycle-wide request window.
* **R#9 S1/S0 and R#18.** Both need a second set of tables per mode; R#18 needs sixteen.
  Worth at most 4 cycles on a step that crosses the padding.
* **the border/display switch one line early.** Needs the table to change inside a line.
* **text, character and MSX1 tables.** Only the G1/G2/G3 CPU slot classes are known; the
  step delays for those modes have never been measured.
* **the two clocks are asynchronous on real hardware** (5.96113 VDP cycles per T-state on
  the NMS 8280, section 8). openMSX runs them at exactly 6, so a request always lands on
  the same phase. Nothing to do about that here.

## 17. The command start for the other six commands

The `*i.vcd` captures add POINT, PSET, SRCH, LMCM, LMMC and HMMC, display off, fifteen
captures each. Same method as section 11 -- the last `/CSW` of the register burst is the
write to R#46, and the slot the first VRAM access lands in, against the slot before it that
the command did not take, brackets the delay -- but with one improvement that matters here.

Rounding each launch's `/CSW` edge to a whole cycle on its own makes a launch whose edge
sits close to a cycle boundary round the wrong way, which is what left the earlier six with
one or two dissenting launches each. Instead, all 328 launches of all twelve commands were
fitted with **one shared real pin delay**: the arbiter sees the write at
`floor(t_csw + phi)`, with the same `phi` for every launch, which is the constant the CPU's
own requests pay. That has a clear optimum -- 4 of 328 launches unsatisfied at the best
offset against 15 at the worst -- and at it every command pins to a single integer:

| command | `S0` from `/CSW` | from the port-write timestamp | first access | launches |
|---|---|---|---|---|
| POINT | **45** | 63 | R source | 26/26 |
| LMMM | **46** | 64 | R source | 21/21 |
| LMCM | **58** | 76 | R source | 28/28 |
| LMMV | **70** | 88 | R dest | 25/28 |
| LMMC | **70** | 88 | R dest | 29/29 |
| PSET | **70** | 88 | R dest | 28/28 |
| SRCH | **70** | 88 | R source | 27/28 |
| HMMM | **82** | 100 | R source | 27/27 |
| YMMM | **82** | 100 | R source | 29/29 |
| HMMV | **94** | 112 | W dest | 28/28 |
| LINE | **94** | 112 | R dest | 28/28 |
| HMMC | **94** | 112 | W dest | 28/28 |

Eleven of the twelve are `46 + 12k`, so section 11's `S0 = 10 + 12N` survives with
N = 3, 4, 5, 6, 7. **POINT is the exception**: 45, one cycle below LMMM, whose first access
is the same source read. It is not a rounding artefact -- at the fitted `phi` the admissible
band for POINT is the single value 45 over all 26 launches, and 46 is outside it.

The groupings are suggestive and still unexplained. The value does not follow the operand
(SRCH reads its source and sits with the three destination-read commands), nor the number of
accesses per unit, nor the byte/pixel distinction, nor whether the command writes at all
(POINT and SRCH both only ever read, and they are 25 cycles apart).

For LMCM, LMMC and HMMC this is the startup only. Their transfer timing is a separate
matter: openMSX performs the transfer access at the moment the byte arrives rather than at
the next slot, which is a TODO that predates this work.

## 18. Character and text modes

The `*i.vcd` set also adds character and text captures: `noCpu` with R#9 S1/S0 = 0 and = 16,
and `wrCpu` at 72 cycles, for screens 1, 2, 3 and 4 and for both text widths, each with the
display off, sprites off and sprites on.

Text mode has no refresh *chain*: `vdp-timing-2.html` measured that the text modes refresh
differently, seven reads per line instead of eight and clustered at cycles 74 to 122 rather
than spread over the line, so the row-incrementing 128-cycle chain the other modes give is
not there. Two anchors are available instead, and they agree:

* fold each capture on its own line marks, which leaves one unknown -- the offset of that
  mark from cycle 0 -- and fit it. That is a one-parameter search and it is sharp;
* or take the documented cluster directly: seven single accesses eight cycles apart, the
  first at cycle 74.

The captures confirm the cluster exactly -- 74, 82, 90, 98, 106, 114, 122, then the two
dummy reads at 230 and 238 and the rendering groups from 246 in steps of 48, each
`g, g+18, g+24, g+30, g+36, g+42` -- which is `vdp-timing-2.html` reproduced twelve years
later on a different machine. Anchoring on the cluster instead of on the fitted mark puts
90 to 95 % of the CPU writes on a `slotsText` slot and uses 46 of the 47, against the fit's
100 % and 45: the same answer, the difference being that a run of CPU writes in the
eight-apart slots can impersonate the cluster.

**The slot tables are confirmed, including the text phase.** Folding every CPU write onto
the line and sweeping the offset:

| capture set | writes | on `slotsChar` | on `slotsText` | on the display-off table | slots used |
|---|---|---|---|---|---|
| screen 2, display off | 636 | **100 %** | 6 % | 94 % | 30/31 |
| screen 2, sprites off | 626 | **100 %** | 6 % | 94 % | 30/31 |
| screen 2, sprites on | 635 | **100 %** | 5 % | 95 % | 30/31 |
| text 40, display off | 533 | 7 % | **100 %** | 0 % | 45/47 |
| text 40, sprites off | 179 | 6 % | **99 %** | 0 % | 42/47 |
| text 40, sprites on | 272 | 7 % | **100 %** | 0 % | 45/47 |
| text 80, display off | 549 | 7 % | **100 %** | 0 % | 45/47 |
| text 80, sprites off | 208 | 8 % | **100 %** | 0 % | 45/47 |
| text 80, sprites on | 267 | 7 % | **100 %** | 0 % | 44/47 |

The best offset is **166 in every one of the nine sets** -- the line mark sits at cycle 166,
which is the first slot after the widest gap in both tables, so the fit is self-consistent
as well as sharp. Pooled, 1896 of 1897 character-mode writes land on a `slotsChar` slot and
all 31 slots are used; 2008 of 2010 text writes land on a `slotsText` slot and 45 of the 47
are used (174 and 182 are never reached by a 72-cycle request train).

That settles several things at once:

* **`slotsText` is right as it stands, phase included.** The circuit model of the
  measurement repository confirms the 47-slot cyclic structure but keeps a global
  one-`phiL` phase ambiguity; this fixes the phase, and it is the one openMSX already has.
* **TEXT1 and TEXT2 share the table**, as openMSX assumes.
* **The character table does not change when sprites are disabled.** `vdp-timing-2.html`
  lists this as unmeasured and guesses that the sprite bandwidth becomes available to the
  CPU as it does in bitmap modes. It does not: sprites off scores exactly the same 100 % on
  the unchanged table.
* **Nor when the display is disabled**, in either character or text mode. For text that
  answers the other open question in `vdp-timing-2.html`: screen-off in text mode is *not*
  the common screen-off schema, it is the text schema (0 % of text writes land on the
  display-off table). Both confirm what openMSX does after
  [issue 1754](https://github.com/openMSX/openMSX/issues/1754).

**Screen 4 has the same lattice as screens 1, 2 and 3.** Their line-end combs are identical
except that screen 4 adds accesses at 1255 and 1319 -- the sprite-mode-2 attribute fetches
-- and both fall in gaps that are not CPU slots in any of them. That is the third
`vdp-timing-2.html` guess confirmed.

**R#9 S1/S0 shortens the line in these modes too.** The raw `/RAS` line period needs no
absolute scale to compare, and the ratio is the same everywhere:

| mode | screen 1 | screen 2 | screen 3 | screen 4 | text 40 | text 80 |
|---|---|---|---|---|---|---|
| line at S1/S0 = 16 | 1365.04 | 1365.01 | 1364.94 | 1365.05 | 1364.96 | 1365.02 |

In the character modes the positions are measurable, and they are not where the bitmap ones
are. Comparing the two settings:

```
S1/S0 = 0    ... 1300 -6- 1306 -35- 1341 -11- 1352 -6- 1358 -10- 1368
S1/S0 = 16   ... 1300 -6- 1306 -33- 1339 -10- 1349 -6- 1355 -10- 1365
```

so **+2 completing at 1341 and +1 at 1352**, identical in screens 1, 2, 3 and 4. As in the
bitmap modes the two settings differ by 3 while the total is probably 4, and the fourth
cycle cannot be located without an unpadded reference. Not implemented: `tabChar` has no
padding today, and the CPU's lookahead is the only thing in those modes that would notice.

For text the positions are narrowed but not pinned. Anchored on the refresh cluster, every
access from 74 through the last rendering read at 1200 sits at an **identical** position in
both settings, so all three cycles are removed between cycle 1200 and the next line's
cluster at 74 -- the right blanking region, which is where the bitmap and character ones
are too. Pinning them further needs an access inside that region: the CPU slots at 1206 to
1362 are there, so a text `wrCpu` capture with S1/S0 = 16 would do it, where the `noCpu`
captures that exist cannot.
