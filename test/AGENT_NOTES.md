# Agent notes: driving openMSX headlessly and its Tcl interpreter

Hard-won facts for anyone (human or agent) writing tests or tooling that
controls openMSX from outside, and for working with openMSX's Tcl scripting.
These were all verified against this repository's source and a real build.

## 1. The headless control protocol (`-control stdio`)

Start openMSX with `-control stdio` to speak the XML control protocol over
stdin/stdout with no window (the renderer stays uninitialized, effectively
the dummy renderer).  Session shape:

  * Send the literal string `<openmsx-control>` **once** to open the session.
  * Then send one `<command>…</command>` element per console command.
  * Read back `<reply result="ok|nok">content</reply>` elements, in order.
    `result="nok"` means the command errored; `content` carries the message.
  * `<log>` and `<update>` elements also arrive; skip anything that is not a
    complete `<reply>` and keep buffering until one is.
  * XML-escape outgoing command text (`&` `<` `>`); decode incoming entities
    (`&lt; &gt; &quot; &apos; &amp;`).  `nok` messages may contain numeric
    entities such as `&#x0a;` for a newline.

Useful console commands used here: `reg <name> [<value>]`, `debug disasm`,
`debug read memory`, `debug set_bp <addr> [<cond>] [<cmd>]`,
`debug remove_bp <id>`, `debug breaked`, `debug cont`, `reset`,
`set power on`, `reverse status/start`, and the `step_back` proc.

## 2. Timing in plain `tclsh` — `after` does NOT sleep

`after <ms>` only *schedules* a timer; it returns immediately unless an event
loop runs.  In a plain `tclsh` script there is no event loop, so `after` alone
never waits.  To really sleep, drive the event loop with `vwait`:

    after $ms { set ::done 1 }
    vwait ::done

The driver wraps this in `omsx::sleepms`.  (Inside openMSX's own interpreter
an event loop is running, so `after` behaves normally there.)

## 3. `info script` is empty/wrong inside a proc

`[info script]` returns the currently *executing* script file.  Inside a proc
called later (after `source` has returned) it no longer points at the file that
defined the proc.  If a library file needs its own location (e.g. to find the
repo root), capture it at load time:

    set ::ns::rootdir [file normalize [file join [file dirname \
                       [file normalize [info script]]] .. ..]]

## 4. openMSX overwrites `OPENMSX_USER_DATA` / `OPENMSX_SYSTEM_DATA`

openMSX reads these two variables from its **process** environment at startup
(`FileOperations::getUserDataDir()` / `getSystemDataDir()`), then injects them
into the Tcl interpreter's `env()` array (`Interpreter.cc`).  So:

  * Setting them in the *parent* process before spawning openMSX works — the
    child inherits them.  Setting them later via the console does not stick.
  * `OPENMSX_SYSTEM_DATA` unset → compiled-in `DATADIR` (the install prefix,
    e.g. `/opt/openMSX/share`).  `OPENMSX_USER_DATA` unset → `~/.openMSX/share`.

## 5. How Tcl scripts are loaded — and how to test the repo's copy

Console scripts are loaded *lazily* (`share/init.tcl`): `register_lazy` maps a
script to the proc names it defines, and `lazy_handler` sources it from
`[data_file scripts/<name>]` the first time one of those procs runs.

`data_file` resolves `OPENMSX_USER_DATA/<file>` **first**, then
`OPENMSX_SYSTEM_DATA/<file>`.  Therefore, to make openMSX run the repository's
`share/scripts/` instead of the installed copy, arrange for
`OPENMSX_USER_DATA/scripts/_disasm.tcl` to be the repo file and leave
`OPENMSX_SYSTEM_DATA` unset.  This is what `openmsx_driver.tcl::spawn` does.

**Pitfall — the user-data dir is writable.**  openMSX treats
`OPENMSX_USER_DATA` as its writable user area and drops `settings.xml`,
`.filecache`, `machines/`, etc. into it.  Pointing it straight at the repo's
`share/` pollutes tracked files (`share/settings.xml` becomes modified).  The
driver instead points `OPENMSX_USER_DATA` at a scratch dir (`test/.userdata`,
gitignored) holding only a `scripts` symlink to `<repo>/share/scripts`, so the
scripts load from the working tree while runtime writes stay in the scratch dir.

**Pitfall — the source tree is not a full data dir.**  It does *not* contain
every file the installed share has.  In particular the `C-BIOS_MSX1` machine
definition and the system ROMs are bundled by packaging and live only in the
installed system data dir.  That is why `OPENMSX_SYSTEM_DATA` must stay unset
(fallback) rather than also pointing at the repo — otherwise machine lookup
fails.  For the same reason, tests here only use the `C-BIOS_MSX1` machine.

## 6. Tcl 9 namespace variables

Under Tcl 9 a variable created with `set ::ns::var …` inside one proc is not
necessarily readable as `::ns::var` from another proc; declare namespace
variables explicitly:

    namespace eval ns { variable foo "" }

(Still set the initial value if you also want the global form.)  Plain Tcl 8.6
is more lax, but the `variable` form works on both.

## 7. Bare `<` / `>` in a Tcl command are redirection

In Tcl 9 (and as a general hazard) a bare `<` or `>` token is treated as an
I/O redirection operator.  When you need the literal characters — e.g. as the
keys/values of a `string map` pair list for XML escaping — wrap them in braces:

    string map [list {&} {&amp;} {<} {&lt;} {>} {&gt;}] $s

## 8. Breakpoints, `reset`, and reading registers

  * `debug set_bp <addr> [<cond>]` — `<cond>` is a Tcl expression evaluated in
    openMSX's context each time the address is reached, e.g.
    `debug set_bp 0x401b {[reg BC] == 0x1000}`.  Returns an id like `bp#1`;
    remove with `debug remove_bp bp#1`.
  * After `reset`, the CPU is left **stalled at `PC=0` in break state** in a
    control session.  You must issue `debug cont` to let it run again.
  * `reg <name>` returns the value as a **decimal** string.  Compare addresses
    numerically (the test framework's `eq_hex` normalises decimal/hex).
  * A plain (unconditional) breakpoint at the instruction right after a block
    fires on the *first* execution of that instruction, which is the clean way
    to observe the just-completed block.

## 9. Saved block history (`reverse blockstart`) and `step_back` semantics

The CPU keeps a small saved history of block-repeat executions (PC, SP,
start time and an active flag).  It contains suspended and completed runs,
is serialized in the CPU savestate, and is queried with
`reverse blockstart <pc> <sp>`.  The command returns the start time of the
latest matching run, or errors (`nok`) when the global
`debug_block_instructions` setting is disabled or no matching run is in the
history.  `step_back` uses this command and otherwise falls back to the
generic reverse search.

## 10. Block-repeat `step_back` semantics that the tests pin down

For a block repeat instruction, a correct `step_back` lands on the block PC with
the loop counter restored to its initial value, in all of:

  * current-instruction-is-the-block,
  * the block *immediately precedes* PC (the classic regression),
  * the block was IRQ-interrupted and resumed,
  * the block is inside a loop (land on the current pass, not an earlier one).

Note the "immediately precedes" requirement: if other instructions run between
the block and PC (e.g. `LDIR` … `POP BC` … `DJNZ` … `JR`), `step_back` from the
`JR` correctly rewinds only one instruction (to the `DJNZ`), because the block is
no longer the immediate predecessor.  Tests must place the probe accordingly.

The counter is `BC` for `LDIR/LDDR/CPIR/CPDR` and `B` alone (with `C` a fixed
I/O port) for `INIR/INDR/OTIR/OTDR`; the tests assert it is back at its
pre-block value after `step_back`.
