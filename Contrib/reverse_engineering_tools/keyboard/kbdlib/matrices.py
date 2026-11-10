"""The key matrices that openMSX emulates (MSX, SVI, Sega), and defaults for
the BIOS/BASIC of the machines with that matrix.

Two kinds of entries:

- The key matrix itself: the positions of the modifiers, space, RETURN, STOP
  and the number of rows. openMSX defines these per type of key matrix (see
  src/input/Keyboard.cc), so they are the same for all machines of a type.
  Positions are (row, bit).

- Defaults for the BIOS/BASIC: how to clear the screen, the codes on the
  screen that aren't typed characters, free VRAM for the BASIC program of
  the special keys scan, etc. These held for all machines in share/machines
  (November 2026), but a new machine can differ: a recipe (recipes.py) can
  override each of them for one machine.

The modifier names are the ones used in the unicodemaps: SHIFT, CTRL, GRAPH,
CODE and CAPSLOCK.
"""

# entries that a recipe can override (see recipes.py)
BASIC_DEFAULTS = ('clear', 'screen_ignore', 'buffer', 'counter', 'inkey_release',
                  'start_time', 'setup')

MSX = dict(
    name='msx',
    # --- the key matrix ---
    rows=0xC,  # rows 0-B (9-B: the numeric keypad and more)
    modifiers={'SHIFT': (6, 0), 'CTRL': (6, 1), 'GRAPH': (6, 2),
               'CAPSLOCK': (6, 3), 'CODE': (6, 4)},
    # keys that type characters
    char_keys=[(r, b) for r in range(6) for b in range(8)],
    # all keys for the scan of special keys and CTRL combinations
    special_keys=[(r, b) for r in range(9) for b in range(8)],
    # never press these in a scan: the modifiers themselves, and STOP (it
    # interrupts or pauses the BASIC program of the scan)
    skip=[(6, 0), (6, 1), (6, 2), (6, 3), (6, 4), (7, 4)],
    space=(8, 0),
    enter=(7, 7),
    # --- defaults for the BIOS/BASIC ---
    # BASIC starts in SCREEN 0 or 1, with 40 or 80 columns, depending on the
    # machine and its settings: set up the same screen on all of them (typed
    # via the keyboard buffer, see scan.tcl)
    setup='SCREEN 0:WIDTH 40\r',
    # start when BASIC shows "Ok" or the screen is still for 4 seconds
    start_time=None,
    # clears the screen: SHIFT+HOME
    clear=[(6, 0), (8, 1)],
    # codes on the screen that aren't typed characters: space, DEL, cursor
    screen_ignore=[0x20, 0x7F, 0xFF],
    # VRAM addresses for the BASIC program of the special keys scan (not
    # used in SCREEN 0)
    buffer=0x3E00, counter=0x3DFF,
    # INKEY$ reads a keyboard buffer
    inkey_release=False,
)

SVI = dict(
    name='svi',
    # --- the key matrix ---
    rows=0xC,  # (as in the unicodemaps of MSX)
    # GRAPH is the left GRAPH key, CODE the right GRAPH key
    modifiers={'SHIFT': (6, 0), 'CTRL': (6, 1), 'GRAPH': (6, 2),
               'CODE': (6, 3), 'CAPSLOCK': (8, 3)},
    char_keys=[(r, b) for r in range(6) for b in range(8)],
    special_keys=[(r, b) for r in range(9) for b in range(8)],
    # modifiers, STOP (6, 5) and CAPS
    skip=[(6, 0), (6, 1), (6, 2), (6, 3), (6, 5), (8, 3)],
    space=(8, 0),
    enter=(6, 6),
    # --- defaults for the BIOS/BASIC ---
    setup='',
    start_time=12,
    # the CLS key (with SHIFT it's HOME)
    clear=[(7, 5)],
    # space and cursor (the character codes on the screen aren't ASCII)
    screen_ignore=[0x00, 0xBF],
    buffer=0x3E00, counter=0x3DFF,
    inkey_release=False,
)

SEGA = dict(
    name='sega',
    # --- the key matrix ---
    rows=0xD,  # rows 0-C in the unicodemaps (row D: the modifiers)
    # CODE is the ENG/DIER'S key (international) or the kana key (Japanese)
    modifiers={'SHIFT': (13, 3), 'CTRL': (13, 2), 'GRAPH': (13, 1),
               'CODE': (0, 4)},
    # rows 0-6 (all bits) and rows 7-12 (bit 0), without the special keys:
    # HOME/CLR, INS/DEL, the cursor keys and CR
    char_keys=[(r, b) for r in range(7) for b in range(8)
               if (r, b) not in [(0, 4), (2, 4), (3, 4), (4, 5), (5, 5),
                                 (6, 5), (6, 6), (5, 6)]]
              + [(r, 0) for r in range(7, 13)],
    special_keys=[(r, b) for r in range(7) for b in range(8)]
                 + [(r, 0) for r in range(7, 13)],
    # the modifiers (row 13), BREAK (13, 0) and FUNC (12, 3)
    skip=[(0, 4), (13, 0), (13, 1), (13, 2), (13, 3), (12, 3)],
    space=(1, 4),
    enter=(5, 6),
    # --- defaults for the BIOS/BASIC (Sega BASIC) ---
    setup='',
    start_time=12,
    # CTRL+L (SHIFT+HOME only moves the cursor home in Sega BASIC; HOME
    # clears only in GRAPH mode)
    clear=[(13, 2), (1, 6)],
    screen_ignore=[0x20],
    # the text mode name table is at 0x3C00, the patterns at 0x1800
    buffer=0x1000, counter=0x0FFF,
    # INKEY$ has no keyboard buffer: it gives the key that is held now
    inkey_release=True,
)

MATRICES = {'msx': MSX, 'svi': SVI, 'sega': SEGA}


def matrix_for_type(machine_type):
    """The matrix for the <type> of a machine config."""
    t = machine_type.upper()
    if t.startswith('SVI'):
        return SVI
    if t.startswith('SC-3000') or t.startswith('SG-1000'):
        return SEGA
    return MSX
