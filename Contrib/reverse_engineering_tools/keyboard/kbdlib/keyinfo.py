"""The keyboard_info files (share/keyboard_info/keyboard_info.<type>): the
unicodemap and character set of a keyboard type, and the names of the keys in
the key matrix (used by e.g. the msx_key_position command). Subcommand:
geninfo.

geninfo starts from the keyboard_info file of the closest existing keyboard
(--base): that one gives the layout of the table, the names of the special
keys (rows 6 and up on MSX: modifiers, cursor keys, numeric keypad, ...) and
the character set. Only the names of the character keys of the key matrix
(see matrices.py) are taken from the unicodemap:
- the character the key types without modifiers (letters and digits as they
  are, other characters by name: see NAMES), or DEADKEY<n> for a dead key,
- the digit, if the key types a digit with SHIFT (e.g. the top row of a
  French keyboard),
- if the key types more than one character without modifiers (e.g. both
  backslash and yen), the one that the character set shows,
- if the key types nothing without modifiers: the name in --base (e.g. PI on
  the Sega keyboard, the cursor keys of the SVI; not a dead key, and not a
  name that another key gets), or else BLANK if the key types something with
  SHIFT. The kept names are reported: check them.
"""
import glob
import os
import re
import sys
import unicodedata
import xml.etree.ElementTree as ET

from .matrices import MATRICES, matrix_for_type
from .unicodemaps import KBDIR, Charset, Unicodemap, charset_path, keyboard_info, map_path

# names of the characters that aren't a letter or digit (as in the existing
# keyboard_info files)
NAMES = {
    '-': 'MINUS', '=': 'EQUALS', '\\': 'BACKSLASH', '{': 'LEFT_CURLY_BRACKET',
    '}': 'RIGHT_CURLY_BRACKET', '[': 'LEFT_SQUARE_BRACKET', ']': 'RIGHT_SQUARE_BRACKET',
    ';': 'SEMICOLON', "'": 'QUOTE', '`': 'BACKQUOTE', ',': 'COMMA', '.': 'PERIOD',
    '/': 'SLASH', '#': 'HASH', '$': 'DOLLAR', '*': 'ASTERISK', '(': 'LEFT_PAREN',
    ')': 'RIGHT_PAREN', '&': 'AMPERSAND', ':': 'COLON', '<': 'LESS', '>': 'GREATER',
    '?': 'QUESTION', '@': 'AT', '^': 'CARET', '_': 'UNDERSCORE', '+': 'PLUS',
    '%': 'PERCENT', '"': 'DOUBLE_QUOTE', '!': 'EXCLAIM', '|': 'PIPE', '~': 'TILDE',
    ' ': 'SPACE', '¤': 'CURRENCY', '§': 'SECTION', '£': 'POUND', '°': 'DEGREE',
    '¥': 'YEN', '₩': 'WON', 'π': 'PI', 'ß': 'SS',
    'Ç': 'C_CEDILLA', 'È': 'E_GRAVE', 'É': 'E_ACUTE', 'À': 'A_GRAVE', 'Ù': 'U_GRAVE',
    'Ä': 'A_UMLAUT', 'Ö': 'O_UMLAUT', 'Ü': 'U_UMLAUT', 'Å': 'A_RING', 'Ñ': 'N_TILDE',
    'Æ': 'AE', 'Ø': 'O_SLASH',
    '٪': 'ARABIC_PERCENT', '٧': 'ARABIC_7', 'ﹹ': 'DAMMA_MEDIAL',
}

def key_name(u, unnamed):
    """The name of the key that types unicode u (None for a control code)."""
    if u < 0x20 or u == 0x7F:
        return None
    c = chr(u)
    up = c.upper() if len(c.upper()) == 1 else c
    if up in NAMES:
        return NAMES[up]
    if c in NAMES:
        return NAMES[c]
    if up.isascii() and up.isalnum():
        return up
    # not in the table: make a name from the Unicode name, and report it
    n = unicodedata.name(up, 'U+%04X' % u)
    n = re.sub(r'^(LATIN|GREEK|CYRILLIC) (CAPITAL |SMALL )?LETTER ', '', n)
    n = re.sub(r'[^A-Z0-9]+', '_', n.replace(' WITH ', '_')).strip('_')
    unnamed.add('%s (U+%04X) -> %s' % (c, u, n))
    return n

def derive_names(m, cs, positions):
    """(row, bit) -> key name, for the character keys of unicodemap m."""
    plain, shifted, dead = {}, {}, {}
    for e in m.entries:
        if not e.typeable or (e.row, e.bit) not in positions:
            continue
        pos = (e.row, e.bit)
        if isinstance(e.key, str):
            dead[pos] = e.key
        elif not e.mods:
            plain.setdefault(pos, []).append(e.key)
        elif e.mods == {'SHIFT'}:
            shifted.setdefault(pos, []).append(e.key)
    names, unnamed = {}, set()
    for pos in positions:
        if pos in dead:
            names[pos] = dead[pos]
            continue
        ps = [u for u in plain.get(pos, []) if key_name(u, set())]
        ss = [u for u in shifted.get(pos, []) if key_name(u, set())]
        if ps:
            # more than one: the one that the character set shows (e.g. yen,
            # not backslash, on a Japanese keyboard)
            u = next((u for u in ps if u in cs.u2c and cs.c2u[cs.u2c[u]][0] == u), ps[0])
            digit = next((s for s in ss if chr(s).isdigit() and chr(s).isascii()), None)
            names[pos] = chr(digit) if digit is not None else key_name(u, unnamed)
        elif ss:
            names[pos] = 'BLANK'
        else:
            names[pos] = ''
    return names, unnamed

ROW_RE = re.compile(r'^row\s+(\d+)\s*\|(.*)\|\s*$')

def read_table(lines):
    """The rows of the key table: [(row, [name of bit 0, ..., bit 7])]."""
    rows = []
    for l in lines:
        m = ROW_RE.match(l)
        if m:
            cells = [c.strip() for c in m.group(2).split('|')]
            if len(cells) != 8:
                sys.exit('not 8 columns: %s' % l)
            rows.append((int(m.group(1)), list(reversed(cells))))
    return rows

def render(lines, rows, unicodemap, charset):
    """The keyboard_info file: lines with the header lines and the table
    replaced."""
    cols = [[cells[7 - i] for _, cells in rows] for i in range(8)]  # bit 7 first
    w = [max([7] + [len(c) for c in col]) for col in cols]
    def border(top):
        return '#------+' + '+'.join(
            (('- bit %d ' % (7 - i)) if top else '').ljust(w[i] + 2, '-')
            for i in range(8)) + '+'
    out, table, seen_border = [], iter(rows), False
    for l in lines:
        if l.startswith('UnicodeMap:'):
            l = 'UnicodeMap: ' + unicodemap
        elif l.startswith('MSX-Video-Characterset:'):
            l = 'MSX-Video-Characterset: ' + charset
        elif l.startswith('#------+'):
            l = border(not seen_border)
            seen_border = True
        elif ROW_RE.match(l):
            r, cells = next(table)
            l = 'row %2d | ' % r + ' | '.join(
                cells[7 - i].ljust(w[i]) for i in range(8)) + ' |'
        out.append(l)
    return '\n'.join(out) + '\n'

def info_path(share, name):
    if os.path.sep in name or name.startswith('.'):
        return name
    for d in share:
        p = os.path.join(d, KBDIR, 'keyboard_info.' + name)
        if os.path.exists(p):
            return p
    sys.exit('keyboard_info.%s not found' % name)

def matrix_of(share, keyboard_type, unicodemap):
    """The key matrix of the machines with this keyboard_type (or, if there
    are none, with a keyboard_type that has the same unicodemap)."""
    found, same_map = set(), set()
    for d in share:
        for p in glob.glob(os.path.join(d, 'machines', '*.xml')):
            try:
                root = ET.parse(p).getroot()
            except ET.ParseError:
                continue
            # (only machines that name it: a machine without keyboard_type
            # has no keyboard, or the default one of its matrix)
            t = root.findtext('.//keyboard_type', '').strip()
            if not t:
                continue
            matrix = matrix_for_type(root.findtext('info/type', '').strip())['name']
            if t == keyboard_type:
                found.add(matrix)
            info = os.path.join(d, KBDIR, 'keyboard_info.' + t)
            if os.path.exists(info) and keyboard_info(info).get('unicodemap') == unicodemap:
                same_map.add(matrix)
    return found or same_map

def cmd_geninfo(args):
    m = Unicodemap(map_path(args.share, args.map))
    base = args.base or m.name
    bpath = info_path(args.share, base)
    binfo = keyboard_info(bpath)
    if args.matrix:
        matrix = args.matrix
    else:
        found = matrix_of(args.share, os.path.basename(bpath)[len('keyboard_info.'):],
                          binfo.get('unicodemap'))
        if len(found) != 1:
            sys.exit('can\'t tell the key matrix from the machines with keyboard type %s '
                     '(%s): give --matrix' % (base, ', '.join(sorted(found)) or 'none'))
        matrix = found.pop()
    positions = set(MATRICES[matrix]['char_keys'])
    csname = args.charset or binfo.get('charset')
    cs = Charset(charset_path(args.share, csname)) if csname and charset_path(args.share, csname) else None
    if cs is None:
        sys.exit('character set %s not found: give one with --charset' % csname)
    names, unnamed = derive_names(m, cs, positions)
    lines = open(bpath, encoding='utf-8').read().splitlines()
    rows = read_table(lines)
    kept = []
    for r, cells in rows:
        for bit in range(8):
            pos = (r, bit)
            if pos not in positions:
                continue
            n = names.get(pos, '')
            b = cells[bit]
            if (n in ('', 'BLANK') and b and b != 'BLANK' and not b.startswith('DEADKEY')
                    and b not in names.values()):
                # the key types nothing without modifiers: keep the name
                kept.append('%X%d %s' % (r, bit, cells[bit]))
            else:
                cells[bit] = n
    text = render(lines, rows, m.name, cs.name)
    if args.out:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    for u in sorted(unnamed):
        print('check the name of: %s' % u, file=sys.stderr)
    if kept:
        print('kept from %s (no character without modifiers): %s'
              % (base, ', '.join(kept)), file=sys.stderr)

def add_subcommands(sub):
    p = sub.add_parser('geninfo', help='generate a keyboard_info file for a unicodemap')
    p.add_argument('map', help="unicodemap name (e.g. 'de') or file")
    p.add_argument('--base', help='the keyboard_info of the closest existing keyboard type '
                                  '(e.g. int_numpad); default: the one with the name of the map')
    p.add_argument('--charset', help='character set mapping (default: the one of --base)')
    p.add_argument('--matrix', choices=sorted(MATRICES),
                   help='key matrix (default: the one of the machines with the --base keyboard type)')
    p.add_argument('--out', help='output file (default: standard output)')
    p.set_defaults(func=cmd_geninfo)
