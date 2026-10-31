# openMSX `step_back` test suite

Automated tests for the block-repeat handling in `disasm::step_back`
(`share/scripts/_disasm.tcl`).  A *block repeat
instruction* is one of `LDIR LDDR CPIR CPDR INIR INDR OTIR OTDR`.

`step_back` normally rewinds by exactly one instruction.  For a block repeat
instruction it instead rewinds to the point **before the first iteration** of
the current block execution.  This must also hold when:

  * the block has *just completed* and the instruction about to run is the one
    immediately after it (e.g. a `RET`/`JR` right after an `LDIR`), and
  * the block was *interrupted by an IRQ* and resumed, and
  * the block sits inside a loop and the same PC is executed on many passes.

These tests guard all of the above.

To do so, `step_back` asks the CPU for the start of the latest block run
entered with the current PC and SP (`reverse blockstart <pc> <sp>`), then
jumps to that time.  The history contains both suspended and completed runs
and is saved with the CPU state.

## Layout

    test/
      common/
        tcltest.tcl              tiny dependency-free assertion framework
        openmsx_driver.tcl       headless openMSX driver (XML stdio control)
      integration/
        step_back_integration_test.tcl   step_back test cases
        run_integration.tcl              entry point (runs the suite)
      roms/
        *.asm  *.rom             MSX cartridge fixtures (16 KiB, page 1)
        build.sh                 rebuild the .rom files with SjASMPlus

## Running

Run from the repository root.

Integration suite (needs an openMSX binary and the `C-BIOS_MSX1` machine):

    tclsh test/integration/run_integration.tcl

The launcher runs each suite as a separate `tclsh` subprocess (each test file
ends with its own summary + non-zero exit on failure) and aggregates the
results: 8 tests total.

## Saved block history

`step_back` queries the CPU's saved block history with
`reverse blockstart <pc> <sp>`.  The history contains both suspended and
completed runs, is part of the savestate, and therefore also works after
loading a snapshot.  When no matching run is in the history, `step_back`
falls back to a plain single-instruction step.

### Choosing the openMSX binary

`openmsx_driver.tcl` locates the binary in this order:

  1. `$OPENMSX_BIN` if set;
  2. the in-tree build `./derived/openmsx` (so you test what you just built);
  3. the first `openmsx` on `$PATH`;
  4. common install prefixes (`/opt/openMSX/bin`, `/usr/local/bin`, ...).

### Which Tcl scripts are tested

openMSX normally loads its console scripts from its installed *system data
dir*, so testing a freshly edited `share/scripts/_disasm.tcl` would silently
exercise the installed copy instead.  The driver prevents that: before
spawning openMSX it points `OPENMSX_USER_DATA` at a scratch directory
(`test/.userdata`, gitignored) that contains a `scripts` symlink to the repo's
`share/scripts`.  openMSX's `data_file` helper looks in `OPENMSX_USER_DATA`
first, so `scripts/_disasm.tcl` (and the rest of `share/scripts/`) is loaded
from the working tree.  Using a scratch dir (rather than pointing
`OPENMSX_USER_DATA` at the repo's `share/` itself) keeps openMSX's runtime
writes — `settings.xml`, `.filecache` — out of the repository.
`OPENMSX_SYSTEM_DATA` is deliberately left unset so that data the source tree
does not ship — machine definitions such as `C-BIOS_MSX1.xml` and the system
ROMs — still resolve from the installed system data dir.

## The fixtures

Each fixture is a 16 KiB MSX1 cartridge (`AB` header with the standard `INIT`
vector) that, on reset, sets up registers and runs one block repeat
instruction, then spins in a `JR` loop.  They are built from the `.asm` sources
with SjASMPlus (`sh test/roms/build.sh`); the `.rom` files are checked in so
the tests run without an assembler.

  * `ldir_irq.rom` — `LDIR` at `0x401B`, `BC=$2000`, source `HL=$4000`,
    dest `DE=$C000`; a 60 Hz video IRQ is arranged to hit mid-`LDIR`.
  * `otir_irq.rom` — `OTIR` at `0x4019`, `B=0` (256 iterations),
    `C=$A0`; a 60 Hz video IRQ is arranged to hit mid-`OTIR` (exercises
    the B-only counter case).
  * `ldir_djnz_loop.rom` — `LDIR` at `0x4019` inside a `DJNZ` outer
    loop (4 passes, `DI` so no IRQ); same block PC on every pass.
  * `ldir_varying_bc.rom` — shared `LDIR` subroutine at `0x4027`
    called with a different `BC` from three sites (5, then 3, then 4;
    `DI` so no IRQ); same block PC with varying initial counters.
  * `ldir_nested_inir.rom` — outer `LDIR` at `0x4026`
    (`BC=$2000`) interrupted by video IRQs whose `H.TIMI` hook runs its own
    `INIR` (`B=4` reads of the VDP status port) at `0x4034`; nested blocks
    at different PCs, suspended/resumed outer execution.
  * `ldir_random_bc.rom` — 48 unrolled `LDIR`s whose `BC` values were
    generated with a fixed seed and include single digits, `0` (65536
    iterations), and values up to `$FFFF`. The integration test scans the
    image for the `LD BC,nn; LDIR` pairs.

## What the tests assert

After arming a breakpoint and running to it, each block test calls `step_back`
and asserts that the CPU lands on the block instruction with
the registers restored to their **pre-block** values (the initial counter
value).  For example, the `LDIR` after-block test asserts `PC=0x401B`,
`BC=$2000`, `HL=$4000`, `DE=$C000`.
