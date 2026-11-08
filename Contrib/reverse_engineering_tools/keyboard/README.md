# Keyboard investigation tools

These tools find out what each key of an emulated machine types, so that its
keyboard can be described correctly in openMSX: the `keyboard_type` (which
keyboard_info file in `share/keyboard_info`, which names the unicodemap and
the character set) and the other keyboard settings in its machine config.
With them, you can:

- check which existing unicodemap fits a machine,
- generate a new unicodemap and keyboard_info file if none fits,
- check that typing on the host keyboard (and the `type` command) gives the
  right characters on the MSX.

They work for MSX machines, the Spectravideo SVI-318/328 and (with some
limitations) the Sega SC-3000.

## How it works

openMSX runs the machine (headless by default) with a Tcl script that:

1. boots the machine into BASIC, using a "recipe" for machines with a start
   menu or firmware (`kbdlib/recipes.py`);
2. **chars pass:** presses each key in the key matrix, with each modifier
   combination (none, SHIFT, GRAPH, SHIFT+GRAPH, CODE, SHIFT+CODE, and the
   CAPS combinations for Japanese machines), in the BASIC screen editor. It
   reads the character codes that appear on the top line of the screen from
   VRAM. Keys that type nothing are tested as dead keys: that key followed
   by the keys for a, e, i, o, u, c and n.
3. **special pass:** special keys and CTRL combinations don't show a
   character on the screen. So it types a small BASIC program, which reads
   the keyboard with INKEY$ and stores the codes in VRAM. The program is
   typed with the keys that the chars pass found for its characters.

Keys are only pressed in the key matrix (`keymatrixdown`/`keymatrixup`), and
results are only read from VRAM. So nothing depends on the unicodemap or the
host keyboard. The one exception: on MSX, `SCREEN 0:WIDTH 40` is typed via
the BIOS keyboard buffer first, because BASIC starts in different screen
modes depending on the machine. The script checks that this worked.

openMSX runs with its own user directory in the output directory. Only the
system ROMs are used from yours (via the file pool setting), so unicodemaps,
machines or scripts in your own openMSX user directory don't influence the
results.

The analysis (comparing, generating unicodemaps) is done in Python from the
scan results.

## Requirements

- Python 3 (only the standard library)
- openMSX: `openmsx` from the PATH, or give the executable with `--openmsx`
- the system ROMs of the machine, in your openMSX `share/systemroms` (or give
  the directory with `--systemroms`)

The tools use the machine configs and unicodemaps of the source tree they are
in (`share/` next to `Contrib/`), and make openMSX use them too. Use
`--share DIR` (before the subcommand) for another complete share directory
(with `machines/`, `extensions/`, `keyboard_info/` etc.).

All commands are run in this directory, as `python3 kbdtool.py ...` (on
Windows usually `py kbdtool.py ...`). Each subcommand has `--help`.

## Files

| File | What |
|------|------|
| `kbdtool.py` | the tool to run (subcommands below) |
| `kbdlib/recipes.py` | how to get each machine into BASIC (to adapt for new machines) |
| `kbdlib/matrices.py` | the key matrices (MSX, SVI, Sega), and defaults for their BIOS/BASIC |
| `kbdlib/cli.py` | the subcommands |
| `kbdlib/unicodemaps.py` | reading scans, unicodemaps and character sets; the analysis subcommands |
| `kbdlib/scan.tcl` | the openMSX script that does the scans |

## Step by step: investigating a machine

The example uses a machine `Brand_Model`. Its config must be in
`share/machines/Brand_Model.xml`, with a first guess for `keyboard_type`,
`code_kana_locks` and `graph_locks` (copy them from a similar machine).

### 1. Get the machine into BASIC

    python3 kbdtool.py boot Brand_Model

This takes screenshots during the boot (`scans/Brand_Model/shot-*.png`, at
5 to 90 seconds; see `--times`). Look at them:

- The machine shows the BASIC prompt by itself: nothing to do. The scan
  starts when "Ok" is on the screen, or when the screen hasn't changed for 4
  seconds.
- It shows a start menu, firmware, or MSX-DOS: add a recipe for it in
  `kbdlib/recipes.py` (the documentation is at the top of that file). There are
  examples for all of these: keys to press (`menu_keys`), a key to hold while
  booting (`boot_hold`, e.g. DEL to skip firmware), `BASIC` to type at the
  MSX-DOS prompt (`menu_type`), and `start_time` for when to start. Then run
  `boot` again to check it.
- Machines with modes (like the Arabic machines, see "Special cases") have a
  recipe per mode; select one with `Brand_Model:mode`.

### 2. Scan the keyboard

    python3 kbdtool.py scan Brand_Model

It takes a few seconds; several machines can be given at once. The output in
`scans/Brand_Model/`:

| File | Contents |
|------|----------|
| `status.txt` | `done`, or the error |
| `chars.txt` | per key: `row bit combination codes`, with `-1` for nothing |
| `dead.txt` | for the keys that typed nothing: what that key followed by a letter typed (a, e, i, o, u, c, n) |
| `special.txt` | the same as `chars.txt`, as INKEY$ codes, for all keys with none, SHIFT, CTRL, CTRL+SHIFT, CTRL+GRAPH and CTRL+CODE |
| `font.txt` | the font of the machine (8 bytes per character) |
| `meta.json` | machine, mode, character set, ... |

On an error, `shot-error.png` shows the screen at that moment. The most
likely causes:

- "can't clear the screen": the machine isn't in BASIC (go back to step 1),
  or the clear key doesn't work on this machine (`clear` in a recipe).
- "the BASIC program doesn't run": the program was typed wrongly. That
  usually means the character set mapping is wrong (see step 4); the
  screenshot shows what was typed.

### 3. Find the unicodemap that fits

    python3 kbdtool.py bestmap scans/Brand_Model

This compares the scan with all unicodemaps. A unicodemap with 0 wrong and 0
missing fits: put its name in `keyboard_type` in the machine config. For the
details of one unicodemap:

    python3 kbdtool.py compare -v scans/Brand_Model --map int

- `WRONG`: a unicodemap line whose key types something else (or nothing).
- `MISSING`: a character that a key types, but that's not in the unicodemap.
  This also covers control codes and dead keys.
- `GHOST`: a key that types the right character plus another one (key
  ghosting, see "Special cases").
- `unchecked`: lines that the scan can't check, e.g. when there is no
  character set mapping.

If the differences are small and explainable (e.g. one key that differs),
consider documenting them in the machine config instead of making a new
unicodemap; the existing configs have many examples of such comments.

### 4. Check the character set visually

The scan gives character codes, and a character set mapping
(`share/keyboard_info/*.TXT`, named in the `MSX-Video-Characterset:` line
of the keyboard_info files) tells which Unicode
character each code is. Comparing codes can't show whether that mapping fits
the font of the machine, so check it by eye:

    python3 kbdtool.py report scans/Brand_Model --map de

Open `scans/Brand_Model/report.html` in a browser: for each character of the
unicodemap (from step 3), it shows the glyph that its key types on the
machine (from its font) next to the character. Lines that don't match the
scan are shown in red, and the differences are listed at the top.

Do the glyphs show other characters than the unicodemap (or did no
unicodemap fit at all in step 3)? Then the font is a different one, and a new
character set mapping is needed. Draft one:

    python3 kbdtool.py charset scans/Brand_Model --known scans/Other_Machine ... --out NEWSET

Give scans of machines whose fonts are probably similar as `--known`.
`NEWSET.TXT` gets the codes whose glyph is the same as in one of these fonts
(also when shifted by some pixels: then marked "check"); `NEWSET.html` shows
all 256 glyphs. Fill in the rest of `NEWSET.TXT` by looking at the glyphs
(lines starting with `#0x` are still open), and put it in
`share/keyboard_info/`. If the BASIC program of the
special pass couldn't be typed, scan again with `--charset NEWSET.TXT`.

### 5. If none fits: generate a new unicodemap

    python3 kbdtool.py genmap scans/Brand_Model --base de --region "My region" --out ../../../share/keyboard_info/unicodemap.my

`--base` is the unicodemap to take the character set from (or give
`--charset`). The rules:

- **Characters:** each character the machine types gets the first key that
  types it, in the order none, SHIFT, GRAPH, SHIFT+GRAPH, CODE, SHIFT+CODE.
  Characters of the character set that no key types get a `--` line, with
  the reason.
- **Special lines:** generated from the special pass:
  - the special keys (BS, TAB, HOME, RETURN, INS, SELECT, ESC, the cursor
    keys, space, DEL) without modifiers;
  - the other control codes as CTRL with the key of the lowercase letter
    (^A = CTRL+a) or of the character code+0x40 (^@ = CTRL+@).

  Each of these is only used when the special pass confirms it.
- **Dead keys:** a dead key without modifiers becomes a `DEADKEY<n>` line.
- **The backslash:** on Japanese and Korean MSX machines, code 0x5C is shown
  as yen or won sign but used as backslash, so the backslash goes on the same
  key.

Machines that should share a unicodemap can be given together. The
unicodemap then gets what they have in common, and the header lists the keys
in which they differ. `--header FILE` adds your own text to the header,
e.g. about the machines and the sources. Review the result. Compare it with
the closest existing unicodemap (`diff`), and check the comments. Then
regenerate after any change to the scans, rather than editing by hand.

Then generate the keyboard_info file, which names the unicodemap, the
character set and the keys of the key matrix:

    python3 kbdtool.py geninfo my --base int_numpad --out ../../../share/keyboard_info/keyboard_info.my_numpad

`--base` is the keyboard_info of the closest existing keyboard (with
`_numpad` if the machine has a numeric keypad): the special keys (rows 6 and
up on MSX) and the character set come from there (or give `--charset`). The
names of the character keys come from the unicodemap: the character that the
key types without modifiers (for a key with a digit on SHIFT: that digit),
or DEADKEY<n>. A key that types nothing without modifiers keeps the name in
`--base`; these are listed, and so are characters that have no name yet in
`NAMES` in `kbdlib/keyinfo.py`: check them. To regenerate an existing file
(e.g. after changing the unicodemap), give that file as `--base`.

Put the name of the keyboard_info file (here `my_numpad`) in `keyboard_type`
in the machine config, and do step 4 again with it.

### 6. Check typing

    python3 kbdtool.py typetest Brand_Model

This boots the machine and types every character of its unicodemap (the
`keyboard_type` in its config) with openMSX's `type` command. It then checks
that the screen shows the same as the key gave in the scan, which tests the
whole path (unicodemap, locks, modifiers). Characters above U+FFFF are skipped
when the Tcl version of openMSX can't handle them (Tcl 8.6: `format %c` gives
U+FFFD).

### 7. Finish the machine config

- `keyboard_type`: the unicodemap.
- `code_kana_locks` / `graph_locks`: whether CODE/GRAPH are lock keys. The
  scan uses these settings (it taps lock keys instead of holding them), so
  it can't find them out by itself: when one is wrong, the results of the
  CODE (or GRAPH) combinations are wrong too. If you're not sure, scan with
  both settings (edit the config in between) and compare the two scans with
  photos of the keyboard or other documentation.
- Document what you found (sources, differences, shortcuts taken) in a
  comment.

## Adapting the tools

The tools know how to handle the existing machines. A new machine can need
an addition in one of these places:

| Situation | Where |
|-----------|-------|
| start menu, firmware, MSX-DOS, CAPS on at start, a mode (e.g. Arabic), a BASIC cartridge | a recipe in `kbdlib/recipes.py` (see the documentation at the top) |
| an extra key that must not be pressed (e.g. a second CAPS key) | `skip_keys` in the recipe |
| the BIOS/BASIC differs from the defaults in `kbdlib/matrices.py`: another clear key, other codes on the screen that aren't typed characters (cursor, space), other free VRAM, ... | that entry in the recipe (e.g. `clear`, `screen_ignore`) |
| a new kind of key matrix | `kbdlib/matrices.py`, and `matrix_for_type()` there |
| a font without character set mapping | `kbdtool.py charset` (step 4) |

`kbdlib/matrices.py` has two kinds of entries. The positions of the modifiers,
space, RETURN and STOP are defined by openMSX per type of key matrix, so they
are the same for all machines of that type. The defaults for the BIOS/BASIC
(clear key, screen codes, VRAM addresses, ...) held for all machines in
`share/machines` when these tools were made, but a new machine can differ:
then override them in its recipe.

Run `boot` and `scan` again after each change, and check `shot-error.png`
when something fails.

Machines without a keyboard or without BASIC (game consoles like the
ColecoVision, Sega SG-1000 and Zemmix, or the Aucnet NIA-2001) can't be
scanned.

## Special cases

- **Arabic machines** start in Arabic mode (BASIC from the start menu):
  there the screen editor translates the keys itself and chooses the form of
  each letter. The recipe presses SPACE after each key (`finish_key`), so
  that the final form is on the screen. With CTRL held at boot, they start
  in international mode instead (mode `international`), e.g.
  `Yamaha_AX500:international`.
- **Spectravideo SVI-318/328:** the character codes on the screen are not
  ASCII (see `SVI328.TXT`); the CLS key clears the screen.
- **Sega SC-3000:** needs a BASIC cartridge (`extension` in its recipe).
  Known limitations:
  - There is no character set mapping for its font yet, so `compare` can't
    check the characters and `genmap` doesn't work. `charset` can draft one.
  - INKEY$ has no keyboard buffer (it gives the key that is held), so the
    BASIC program of the special pass waits until a key is released.
  - In Sega BASIC, SHIFT+HOME only moves the cursor to the top left; the scan
    clears the screen with CTRL+L.
  - Only some CTRL combinations give a code with INKEY$.
- **Ghosting:** machines with key ghosting can type an extra character when
  three keys in a rectangle of the matrix are pressed (e.g. SHIFT+GRAPH with
  a key in column 0: SHIFT and GRAPH are both in row 6), unless the machine
  has diodes against it (`key_ghosting_sgc_protected`). That's how the real
  machine behaves too. `compare` reports such keys as `GHOST`, `genmap`
  takes the right character from them, and `typetest` accepts them. (The
  `type` command and typing on the host keyboard in CHARACTER mode give
  the extra character too: see issue #2272.)
- **STOP** is never pressed (it would stop the BASIC program).
