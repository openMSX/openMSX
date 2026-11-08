"""Reading scans, unicodemaps and character set mappings, and the
subcommands that work on them: compare, bestmap, genmap, charset, report.

Terms:
  code      a character code in VRAM (what the screen shows); the character
            set mapping (share/unicodemaps/character_set_mappings/*.TXT)
            gives the Unicode character(s) for each code
  combo     a modifier combination as in the scans: 'none', 'SHIFT',
            'SHIFT+GRAPH', 'CTRL+SHIFT', ...
"""
import glob
import html
import json
import os
import re
import sys
import textwrap
import unicodedata

from .matrices import MATRICES

# the order in which modifier combinations are tried for a character: the
# first key (in this order) that types it, gets it
COMBO_ORDER = ['none', 'SHIFT', 'GRAPH', 'SHIFT+GRAPH', 'CODE', 'SHIFT+CODE',
               'CODE+CAPSLOCK', 'SHIFT+CODE+CAPSLOCK']
SPECIAL_COMBO_ORDER = ['none', 'CTRL', 'CTRL+SHIFT', 'CTRL+GRAPH', 'CTRL+CODE', 'SHIFT']
# the order of the modifiers in a unicodemap line
MOD_ORDER = ['CTRL', 'SHIFT', 'GRAPH', 'CODE', 'CAPSLOCK']

def combo_set(combo):
    """'SHIFT+CODE' / 'CODE SHIFT' / 'none' -> frozenset({'SHIFT', 'CODE'})"""
    parts = re.split(r'[+\s]+', combo.strip())
    return frozenset(p for p in parts if p and p != 'none')

def mods_text(mods):
    """frozenset -> 'SHIFT CODE' (the order of the unicodemaps)"""
    return ' '.join(sorted(mods, key=MOD_ORDER.index))

def combo_name(mods):
    """frozenset -> 'SHIFT+CODE' (for messages)"""
    return '+'.join(sorted(mods, key=MOD_ORDER.index)) or 'none'

def keyname(row, bit, mods):
    k = '%X%d' % (row, bit)
    return k if not mods else '%s+%s' % (combo_name(mods), k)

# --- character set mappings -------------------------------------------------

class Charset:
    """A character set mapping: code <-> Unicode."""
    def __init__(self, path=None):
        self.path = path
        self.name = os.path.basename(path) if path else None
        self.c2u = {}     # code -> [unicode, ...]
        self.u2c = {}     # unicode -> first code
        self.names = {}   # unicode -> name (from the comment)
        if path:
            self.read(path)

    def read(self, path):
        pat = re.compile(r'^0x([0-9A-Fa-f]{2})\s+(?:<(\w+)>\+)?0x([0-9A-Fa-f]+)\s*(?:#\s*(.*))?')
        for line in open(path, encoding='utf-8'):
            m = pat.match(line)
            if not m:
                continue
            c, u = int(m.group(1), 16), int(m.group(3), 16)
            us = self.c2u.setdefault(c, [])
            if u not in us:
                us.append(u)
            self.u2c.setdefault(u, c)
            name = (m.group(4) or '').strip()
            name = re.sub(r', (left to right|right to left)$', '', name)
            self.names.setdefault(u, name)
        # Japanese and Korean MSX machines show character code 0x5C as yen
        # or won sign, but programs use it as backslash: the unicodemaps put
        # the backslash on that key too
        if 0x5c not in self.u2c:
            for alt in (0xa5, 0x20a9):
                code = self.u2c.get(alt)
                if code is not None and self.c2u[code][0] == alt:
                    self.c2u[code].append(0x5c)
                    self.u2c[0x5c] = code
                    self.names[0x5c] = 'REVERSE SOLIDUS, shown as %s' % chr(alt)
                    break

    def uname(self, u):
        n = self.names.get(u)
        if n:
            return n
        try:
            return unicodedata.name(chr(u))
        except ValueError:
            return ''

    def letters(self, code):
        """The Unicode characters of a code, with presentation forms (Arabic
        shaping) also counting as their base letter."""
        out = set()
        for u in self.c2u.get(code, []):
            out.add(u)
            b = base_letter(u)
            if b and len(b) == 1:
                out.add(ord(b))
        return out

def base_letter(u):
    """A presentation form (e.g. ARABIC LETTER SEEN INITIAL FORM): its base
    letter(s); otherwise None."""
    d = unicodedata.decomposition(chr(u)).split()
    if d and d[0] in ('<initial>', '<medial>', '<final>', '<isolated>'):
        return ''.join(chr(int(x, 16)) for x in d[1:])
    return None

def charset_path(share, name):
    for d in share:
        p = os.path.join(d, 'unicodemaps', 'character_set_mappings', name)
        if os.path.exists(p):
            return p
    return None

# --- unicodemaps --------------------------------------------------------------

class Entry:
    def __init__(self, key, row, bit, mods, comment, line):
        self.key = key          # unicode (int) or 'DEADKEY<n>'
        self.row, self.bit = row, bit   # None for '--'
        self.mods = mods        # frozenset
        self.comment = comment
        self.line = line

    @property
    def typeable(self):
        return self.row is not None

class Unicodemap:
    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path).replace('unicodemap.', '')
        self.lines = open(path, encoding='utf-8').read().splitlines()
        self.entries = []
        self.charset = None
        pat = re.compile(r'^(DEADKEY\d*|[0-9a-fA-F]{4,6}),\s*([0-9a-fA-F][0-7]|--)\s*,([^#]*)(#.*)?$')
        for line in self.lines:
            m = re.match(r'MSX-Video-Characterset:\s*(\S+)', line)
            if m:
                self.charset = m.group(1)
                continue
            m = pat.match(line)
            if not m:
                continue
            key = m.group(1) if m.group(1).startswith('DEADKEY') else int(m.group(1), 16)
            pos = m.group(2)
            row, bit = (None, None) if pos == '--' else (int(pos[0], 16), int(pos[1]))
            self.entries.append(Entry(key, row, bit, combo_set(m.group(3)),
                                      (m.group(4) or '')[1:].strip(), line))

    def keyed(self):
        """unicode -> Entry, for the entries with a key"""
        return {e.key: e for e in self.entries if e.typeable and isinstance(e.key, int)}

def map_path(share, name):
    if os.path.sep in name or name.startswith('.'):
        return name
    for d in share:
        p = os.path.join(d, 'unicodemaps', 'unicodemap.' + name)
        if os.path.exists(p):
            return p
    sys.exit('unicodemap %s not found' % name)

def all_maps(share):
    out = {}
    for d in share:
        for p in sorted(glob.glob(os.path.join(d, 'unicodemaps', 'unicodemap.*'))):
            out.setdefault(os.path.basename(p)[len('unicodemap.'):], p)
    return out

# --- scans -----------------------------------------------------------------------

def load_scanfile(path):
    """A scan file -> ({(row, bit, mods): [codes]}, {(row, bit, mods): [dead codes]})"""
    d, dead = {}, {}
    if not os.path.exists(path):
        return d, dead
    for line in open(path):
        p = line.split()
        if len(p) < 4:
            continue
        key = (int(p[0]), int(p[1]), combo_set(p[2]))
        d[key] = [] if p[3] == '-1' else [int(x) for x in p[3].split(',')]
        if len(p) > 4 and p[4].startswith('dead='):
            dead[key] = [int(x) for x in p[4][5:].split(',')]
    return d, dead

class Scan:
    """The results of 'kbdtool.py scan' for one machine (and mode)."""
    def __init__(self, d):
        self.dir = d
        self.meta = json.load(open(os.path.join(d, 'meta.json')))
        self.name = self.meta['machine'] + ('__' + self.meta['mode'] if self.meta.get('mode') else '')
        self.chars, _ = load_scanfile(os.path.join(d, 'chars.txt'))
        self.special, _ = load_scanfile(os.path.join(d, 'special.txt'))
        # dead keys: (row, bit, mods) -> [(letter, codes), ...]: what the key
        # followed by a letter typed, for the letters that it changed
        self.dead = {}
        letters = self.meta.get('dead_letters', {})
        dp = os.path.join(d, 'dead.txt')
        if os.path.exists(dp):
            for line in open(dp):
                p = line.split()
                if len(p) != 4 or p[3] == '-1':
                    continue
                codes = [int(x) for x in p[3].split(',')]
                if codes != [letters.get(p[2])]:
                    self.dead.setdefault((int(p[0]), int(p[1]), frozenset()), []).append((p[2], codes))
        self.matrix = MATRICES[self.meta['matrix']]
        self.font = None
        fp = os.path.join(d, 'font.txt')
        if os.path.exists(fp):
            self.font = [[int(x) for x in l.split()] for l in open(fp)]

def charset_for(args, scan=None, mapobj=None):
    """The character set: --charset, else the one of the unicodemap."""
    name = getattr(args, 'charset', None)
    if not name and mapobj is not None:
        name = mapobj.charset
    if not name and scan is not None:
        name = scan.meta.get('charset')
    if not name:
        return Charset()
    p = name if os.path.exists(name) else charset_path(args.share, name)
    if not p:
        sys.exit('character set mapping %s not found' % name)
    return Charset(p)

# --- compare ---------------------------------------------------------------------

def show_codes(codes, cs):
    if not codes:
        return 'nothing'
    if len(codes) == 1:
        us = [u for u in cs.c2u.get(codes[0], []) if u >= 0x20]
        return '0x%02x %s' % (codes[0], chr(us[0]) if us else '(no Unicode)')
    return '"%s"' % ''.join(chr(cs.c2u[c][0]) if cs.c2u.get(c) and cs.c2u[c][0] >= 0x20
                            else '?' for c in codes)

def show_dead(results, cs):
    """[(letter, codes)] -> 'a: à, e: è'"""
    return ', '.join('%s: %s' % (letter, show_codes(codes, cs)) for letter, codes in results)

def compare(scan, m, cs):
    """Compare a scan with a unicodemap. Returns (counts, messages)."""
    ok, wrong, missing, unchecked, ghost = 0, [], [], 0, []
    keyed = m.keyed()
    deadkeys = {(e.row, e.bit) for e in m.entries if e.typeable and not isinstance(e.key, int)}
    for e in m.entries:
        if not e.typeable:
            continue
        if not isinstance(e.key, int):   # DEADKEY
            k = (e.row, e.bit, frozenset())
            if k in scan.chars and (scan.chars[k] or k not in scan.dead):
                wrong.append('%s at %s: not a dead key, it types %s' % (
                    e.key, keyname(e.row, e.bit, ()), show_codes(scan.chars[k], cs)))
            else:
                ok += 1
            continue
        u = e.key
        k = (e.row, e.bit, e.mods)
        what = 'U+%04X %s at %s' % (u, chr(u) if u >= 0x20 else '', keyname(*k))
        if k in scan.chars and u >= 0x20 and u != 0x7f:
            got = scan.chars[k]
            if len(got) == 1 and u in cs.letters(got[0]):
                ok += 1
            elif not cs.c2u:
                unchecked += 1
            elif len(got) > 1 and any(u in cs.letters(c) for c in got):
                ghost.append('%s gives %s: the right character and another one (key '
                             'ghosting?)' % (what, show_codes(got, cs)))
            else:
                wrong.append('%s gives %s' % (what, show_codes(got, cs)))
        elif k in scan.special:
            got = scan.special[k]
            want = 0x20 if u == 0xa0 else u
            if got == [want]:
                ok += 1
            elif want >= 0x80:
                unchecked += 1
            else:
                wrong.append('%s gives %s (INKEY$ codes), expected %d' % (
                    what, ','.join(map(str, got)) or 'nothing', want))
        else:
            unchecked += 1
    # characters that the machine types, but that aren't in the map
    seen = set()
    for k, codes in sorted(scan.chars.items(), key=lambda kv: scan_order(kv[0])):
        if len(codes) != 1 or codes[0] in seen:
            continue
        us = [u for u in cs.c2u.get(codes[0], []) if u >= 0x20 and u != 0x7f]
        if us and not any(u in keyed for u in us):
            seen.add(codes[0])
            missing.append('%s (typed by %s)' % (show_codes(codes, cs), keyname(*k)))
    for k, codes in sorted(scan.special.items(), key=lambda kv: scan_order(kv[0])):
        if len(codes) == 1 and (codes[0] < 0x20 or codes[0] == 0x7f) and codes[0] not in keyed \
                and codes[0] not in seen:
            seen.add(codes[0])
            missing.append('control code %d (typed by %s)' % (codes[0], keyname(*k)))
    for k in sorted(scan.dead, key=scan_order):
        if not k[2] and (k[0], k[1]) not in deadkeys:
            missing.append('dead key %s (%s)' % (keyname(*k), show_dead(scan.dead[k], cs)))
    return dict(ok=ok, wrong=len(wrong), missing=len(missing), unchecked=unchecked,
                ghost=len(ghost)), \
        ['WRONG   ' + w for w in wrong] + ['MISSING ' + x for x in missing] + \
        ['GHOST   ' + x for x in ghost]

def scan_order(k):
    row, bit, mods = k
    name = combo_name(mods)
    order = COMBO_ORDER + [c for c in SPECIAL_COMBO_ORDER if c not in COMBO_ORDER]
    return (order.index(name) if name in order else 99, row, bit)

def cmd_compare(args):
    for d in args.scans:
        scan = Scan(d)
        m = Unicodemap(map_path(args.share, args.map or scan.meta['keyboard_type']))
        cs = charset_for(args, scan, m)
        counts, msgs = compare(scan, m, cs)
        print('%-36s %-16s ok %3d  wrong %3d  missing %3d  ghosting %3d  unchecked %3d' % (
            scan.name, m.name, counts['ok'], counts['wrong'], counts['missing'], counts['ghost'],
            counts['unchecked']))
        if args.verbose:
            for x in msgs:
                print('  ' + x)

def cmd_bestmap(args):
    for d in args.scans:
        scan = Scan(d)
        results = []
        for name, p in all_maps(args.share).items():
            m = Unicodemap(p)
            if not m.entries:
                continue
            if args.charset is None and not m.charset:
                cs = Charset()
            else:
                cs = charset_for(args, scan, m)
            counts, _ = compare(scan, m, cs)
            results.append((counts['wrong'] + counts['missing'] + counts['ghost'],
                            -counts['ok'], name, counts))
        # (a unicodemap with only a few characters, like cvjoy, has few
        # differences too: only consider those that recognise at least 80%
        # as many characters as the best one)
        best_ok = max(-r[1] for r in results)
        results = [r for r in results if -r[1] >= 0.8 * best_ok]
        print(scan.name)
        configured = scan.meta['keyboard_type']
        results.sort(key=lambda r: (r[0], r[1], r[2] != configured, r[2]))
        for bad, _, name, c in results[:args.top]:
            print('  %-18s wrong %3d  missing %3d  ok %3d%s' % (
                name, c['wrong'], c['missing'], c['ok'],
                '  (the keyboard_type of the machine)' if name == configured else ''))

# --- genmap ------------------------------------------------------------------------

# comments for the control codes and some special characters
SPECIAL_COMMENTS = {
    0x08: 'Backspace',
    0x09: 'Tab',
    0x0b: 'Home (is Home a unicode character?)',
    0x0d: 'Enter/CR',
    0x12: 'Insert (is Insert a unicode character?)',
    0x18: 'Select (is Select a unicode character?)',
    0x1b: 'Escape(SDL maps ESC and ^[ to this code)',
    0x1c: 'Right (SDL maps ^\\ to this code)',
    0x1d: 'Left  (SDL maps ^] to this code)',
    0x1e: 'Up    (SDL maps ^^ to this code)',
    0x1f: 'Down  (SDL maps ^/ to this code)',
    0x20: 'Space',
    0x7f: 'Delete',
    0xa0: 'No-break space (&nbsp;)',
}

def special_lines(scans, chars_pos):
    """The lines for the control codes, space, DEL and no-break space, from
    the special scans (INKEY$). Rules (as in the existing unicodemaps):
    1. the codes of special keys (BS, TAB, HOME, RETURN, INS, SELECT, ESC,
       the cursor keys) and space and DEL: that key, without modifiers;
    2. the other control codes: CTRL with the key of the lowercase letter
       (^A = CTRL+a), or of the character code+0x40 for the others (^@ =
       CTRL+@, where @ can be SHIFT+2 or CODE+2), if the scan confirms it;
    3. otherwise: a key without modifiers, or else with CTRL, CTRL+SHIFT,
       CTRL+GRAPH, CTRL+CODE or SHIFT (in that order)."""
    special = scans[0].special
    by_code = {}
    for k, codes in special.items():
        if len(codes) != 1:
            continue
        # all scanned machines must agree
        if any(s.special.get(k) != codes for s in scans[1:]):
            continue
        by_code.setdefault(codes[0], []).append(k)

    def order(k):
        c = combo_name(k[2])
        return (SPECIAL_COMBO_ORDER.index(c) if c in SPECIAL_COMBO_ORDER else 9,
                k[0] < 6, k)   # (rows 6-8 first)
    out = {}
    for code in list(range(0x20)) + [0x20, 0x7f]:
        keys = by_code.get(code, [])
        if not keys:
            continue
        best = None
        plain = [k for k in keys if not k[2]]
        if code in SPECIAL_COMMENTS and plain:
            best = min(plain, key=order)
        elif code < 0x20:
            for c in ((code + 0x60,) if 1 <= code <= 26 else (code + 0x40,)):
                p = chars_pos.get(c)
                if p and (p[0], p[1], p[2] | {'CTRL'}) in keys:
                    best = (p[0], p[1], p[2] | {'CTRL'})
                    break
        if best is None:
            best = min(keys, key=order)
        if 'CTRL' in best[2]:
            comment = '^' + chr(code + 0x40)
        else:
            comment = SPECIAL_COMMENTS.get(code, 'control code 0x%02x' % code)
        out[code] = (best[0], best[1], best[2], comment)
    if 0x20 in out:
        r, b, mods, _ = out[0x20]
        out[0xa0] = (r, b, mods, SPECIAL_COMMENTS[0xa0])
    return out

def genmap(scans, cs, region, extra_header, mode):
    """Generate a unicodemap from the scans of one or more machines that
    should share it. Returns (text, summary)."""
    names = [s.name for s in scans]
    many = len(scans) > 1
    keys = sorted({k for s in scans for k in s.chars}, key=scan_order)
    pos = {}            # unicode -> (row, bit, mods)
    pos_code = {}       # code -> (row, bit, mods), first key typing that code
    typed = set()       # codes that some key types
    partial, forms, notes, ghosting = [], [], [], []
    nothing_on, other_form = {}, {}

    def assign(code, k):
        typed.add(code)
        pos_code.setdefault(code, k)
        new = []
        for u in cs.c2u.get(code, []):
            if u >= 0x20 and u != 0x7f and u not in pos:
                pos[u] = k
                new.append(u)
        return new

    def describe(outs):
        groups = {}
        for n, v in outs.items():
            groups.setdefault(tuple(v), []).append(n)
        if len(groups) == 1:
            v = list(outs.values())[0]
            return ('all: %s' if many else '%s') % show_codes(v, cs)
        return '; '.join('%s: %s' % (', '.join(ns), show_codes(list(v), cs)) for v, ns in groups.items())

    todo = []
    for k in keys:
        outs = {s.name: s.chars.get(k, []) for s in scans}
        vals = list(outs.values())
        if all(v == vals[0] for v in vals):
            if len(vals[0]) == 1:
                assign(vals[0][0], k)
            elif vals[0]:
                notes.append('%s: %s' % (keyname(*k), describe(outs)))
            continue
        todo.append((k, outs))
    for k, outs in todo:
        nonempty = [v for v in outs.values() if v]
        # key ghosting: some machines type the character plus another one
        # (a ghost key), the others only the character
        singles = {tuple(v) for v in nonempty if len(v) == 1}
        if len(singles) == 1:
            c = list(singles)[0][0]
            ghosts = [n for n, v in outs.items() if len(v) > 1 and c in v]
            if ghosts and all(v == [c] or n in ghosts for n, v in outs.items()):
                assign(c, k)
                ghosting.append('%s: %s (key ghosting on %s)' % (
                    keyname(*k), describe(outs), ', '.join(ghosts)))
                continue
        # every machine types one character, in different forms of the same
        # letter: assign the common Unicode characters (the base letter)
        if all(len(v) == 1 for v in outs.values()):
            common = set.intersection(*[cs.letters(v[0]) for v in outs.values()])
            common = {u for u in common if u >= 0x20 and u not in pos}
            if common:
                for u in sorted(common):
                    pos[u] = k
                    other_form[u] = [n for n, v in outs.items() if u not in cs.c2u.get(v[0], [])]
                for v in outs.values():
                    typed.add(v[0])
                forms.append('%s: %s (same character, in another form)' % (keyname(*k), describe(outs)))
                continue
        # some machines type the same character, the others nothing
        if all(v == nonempty[0] for v in nonempty) and len(nonempty[0]) == 1:
            new = assign(nonempty[0][0], k)
            for u in new:
                nothing_on[u] = [n for n, v in outs.items() if not v]
            if new:
                partial.append('%s: %s' % (keyname(*k), describe(outs)))
            elif cs.c2u.get(nonempty[0][0]):
                notes.append('%s: %s (not needed: on another key)' % (keyname(*k), describe(outs)))
            else:
                notes.append('%s: %s (no Unicode character)' % (keyname(*k), describe(outs)))
        else:
            notes.append('%s: %s' % (keyname(*k), describe(outs)))

    # where can a character be typed at all (on any of the machines)?
    anywhere = {}
    for s in scans:
        for k in sorted(s.chars, key=scan_order):
            v = s.chars[k]
            if len(v) == 1:
                for u in cs.c2u.get(v[0], []):
                    anywhere.setdefault(u, {}).setdefault(s.name, keyname(*k))

    where = 'these machines' if many else 'this machine'
    if mode:
        where += ' in %s mode' % mode

    def why_not(u):
        w = anywhere.get(u)
        b = base_letter(u)
        if not w and b and all(ord(c) in pos for c in b):
            return 'the screen editor chooses this form itself (shaping): type %s' % b
        if not w:
            return "can't be typed on %s" % where
        return 'left out, only on %s (see the list at the top)' % '; '.join(
            '%s: %s' % (n, k) for n, k in w.items())

    def nothing_text(u):
        parts = []
        for n in nothing_on[u]:
            k = anywhere.get(u, {}).get(n)
            parts.append('%s (which has it on %s instead)' % (n, k) if k
                         else "%s (which can't type it)" % n)
        return ' - this key types nothing on: %s' % ', '.join(parts)

    chars_pos = {}
    for code, k in pos_code.items():
        for u in cs.c2u.get(code, []):
            chars_pos.setdefault(u, k)
    specials = special_lines(scans, chars_pos)
    all_mods = [p[2] for p in pos.values()] + [p[2] for p in specials.values()]
    width = max([11] + [len(mods_text(x)) for x in all_mods])

    def line(u, where_, mods, comment):
        return '%05x, %s, %-*s # %s' % (u, where_, width, mods_text(mods), comment)

    gen = {}
    for u, (r, b, mods) in pos.items():
        c = '%s (%s)' % (chr(u), cs.uname(u))
        if u in nothing_on:
            c += nothing_text(u)
        if other_form.get(u):
            c += ' - shown in another form on %s' % ', '.join(other_form[u])
        gen[u] = line(u, '%X%d' % (r, b), mods, c)
    for code, us in cs.c2u.items():
        if code in (0x00, 0xff):
            continue
        for u in us:
            if u not in gen and u >= 0x20 and u != 0x7f:
                gen[u] = line(u, '--', frozenset(), '%s (%s) - %s' % (chr(u), cs.uname(u), why_not(u)))
    for u, (r, b, mods, comment) in specials.items():
        gen[u] = line(u, '%X%d' % (r, b), mods, comment)

    # dead keys (only without modifiers: a DEADKEY can't have modifiers)
    dead = []
    for k in sorted(scans[0].dead, key=scan_order):
        if all(k in s.dead and not s.chars.get(k) for s in scans):
            if k[2]:
                notes.append('%s: dead key (%s), with modifiers: not in this unicodemap'
                             % (keyname(*k), show_dead(scans[0].dead[k], cs)))
            else:
                dead.append(k)

    rows = scans[0].matrix['rows'] - 1
    allmods = 'SHIFT CTRL GRAPH CODE' + (' CAPSLOCK' if any('CAPSLOCK' in x for x in all_mods) else '')
    hdr = ['#region: %s' % region,
           '#format: <UNICODE>, <ROW><COL>, <MODIFIERS>',
           '# <UNICODE>: hexadecimal unicode number or DEADKEY<N>',
           '# <ROW>: row in keyboard matrix (hexadecimal: 0-%X)' % rows,
           '# <COL>: column in keyboard matrix (0-7)',
           '# <MODIFIERS>: space separated list of modifiers:',
           '#              %s' % allmods,
           '#',
           '# Example characters in comments are encoded in UTF-8',
           '#']
    for l in extra_header:
        hdr.append(('# ' + l).rstrip() if not l.startswith('#') else l)
    if extra_header:
        hdr.append('#')

    def wrapped(text):
        return ['# ' + l for l in textwrap.wrap(text, 72)]

    def listing(items):
        out = []
        for n in items or ['(none)']:
            w = textwrap.wrap(n, 70)
            out += ['# - ' + w[0]] + ['#   ' + x for x in w[1:]]
        return out
    method = 'in the BASIC screen editor'
    if many:
        hdr += wrapped('Based on a scan of all key matrix positions %s of: %s. These machines '
                       'type the same characters, except for the keys below (modifiers + '
                       'row and column in the key matrix).' % (method, ', '.join(names)))
        hdr += ['#'] + wrapped('Keys that type a character on some of these machines, and nothing '
                               'on the others; they are in this unicodemap:')
        hdr += listing(partial)
        if forms:
            hdr += ['#'] + wrapped('Keys that type the same character in a different form on some of '
                                   'these machines; they are in this unicodemap (for the character):')
            hdr += listing(forms)
        if ghosting:
            hdr += ['#'] + wrapped('Keys that type an extra character on some of these machines, '
                                   'because of key ghosting (three keys pressed in a rectangle of '
                                   'the key matrix); they are in this unicodemap:')
            hdr += listing(ghosting)
        hdr += ['#'] + wrapped('Keys that type different things on these machines, or a BASIC '
                               'keyword (in quotes); they are left out of this unicodemap (a '
                               'unicodemap per machine could add the single characters among '
                               'them, but not the keywords):')
        hdr += listing(notes)
        hdr += ['#']
    else:
        hdr += wrapped('Based on a scan of all key matrix positions %s of: %s.' % (method, names[0]))
        if notes:
            hdr += ['#'] + wrapped('Keys that type more than one character (e.g. a BASIC keyword, '
                                   'in quotes), or something else that is not in this unicodemap:')
            hdr += listing(notes)
        hdr += ['#']
    body = ['']
    if cs.name:
        body += ['MSX-Video-Characterset: %s' % cs.name, '']
    body += ['DEADKEY%d, %X%d,' % (i + 1, k[0], k[1]) for i, k in enumerate(dead)]
    body += [gen[u] for u in sorted(gen)]
    summary = '%d keyed characters, %d not typeable, %d keys left out' % (
        len(pos), sum(1 for u in gen if ', --,' in gen[u]), len(notes))
    return '\n'.join(hdr + body) + '\n', summary

def cmd_genmap(args):
    scans = [Scan(d) for d in args.scans]
    if len({s.meta['matrix'] for s in scans}) > 1:
        sys.exit('the scans are of different key matrices')
    base = Unicodemap(map_path(args.share, args.base)) if args.base else None
    cs = charset_for(args, scans[0], base)
    if not cs.c2u:
        sys.exit('no character set mapping: give one with --charset (see the '
                 '"charset" subcommand to make one)')
    extra = []
    if args.header:
        extra = open(args.header, encoding='utf-8').read().rstrip('\n').split('\n')
    modes = {s.meta.get('mode') for s in scans}
    mode = modes.pop() if len(modes) == 1 else None
    region = args.region or (base and next((l[9:].strip() for l in base.lines
                                            if l.startswith('#region:')), None)) or 'TODO'
    text, summary = genmap(scans, cs, region, extra, mode)
    if args.out:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write(text)
        print('%s: %s' % (args.out, summary))
    else:
        sys.stdout.write(text)
        print(summary, file=sys.stderr)

# --- glyphs, HTML ------------------------------------------------------------------

def glyph_svg(font, code, scale=4):
    """A character of the font as inline SVG."""
    if font is None or code >= len(font):
        return ''
    d = []
    for y, byte in enumerate(font[code]):
        x = 0
        while x < 8:
            if byte & (0x80 >> x):
                x0 = x
                while x < 8 and byte & (0x80 >> x):
                    x += 1
                d.append('M%d %dh%dv1h-%dz' % (x0, y, x - x0, x - x0))
            else:
                x += 1
    return ('<svg class="g" viewBox="0 0 8 8" width="%d" height="%d"><path d="%s"/></svg>'
            % (8 * scale, 8 * scale, ''.join(d)))

HTML_HEAD = '''<!doctype html>
<html><head><meta charset="utf-8"><title>%s</title>
<style>
:root { --bg: #fff; --fg: #111; --muted: #666; --line: #ccc; --bad: #c00; --glyphbg: #eef; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #111; --fg: #eee; --muted: #999; --line: #444; --bad: #f66; --glyphbg: #223; }
}
body { background: var(--bg); color: var(--fg); font-family: sans-serif; margin: 16px; }
table { border-collapse: collapse; }
td, th { border: 1px solid var(--line); padding: 2px 6px; text-align: center; }
.g { background: var(--glyphbg); fill: var(--fg); vertical-align: middle; margin: 1px; }
.u { font-size: 24px; min-width: 32px; display: inline-block; }
.bad { color: var(--bad); font-weight: bold; }
.small { color: var(--muted); font-size: 11px; }
.grid td { width: 52px; }
</style></head><body>
'''

def cmd_report(args):
    """HTML page: for each character of the unicodemap, the glyph(s) that
    its key really types (from the machine's font) next to the character."""
    scan = Scan(args.scan)
    m = Unicodemap(map_path(args.share, args.map or scan.meta['keyboard_type']))
    cs = charset_for(args, scan, m)
    if scan.font is None:
        sys.exit('no font.txt in %s' % args.scan)
    if not args.out:
        args.out = os.path.join(args.scan, 'report.html')
    _, msgs = compare(scan, m, cs)
    out = [HTML_HEAD % html.escape('%s / %s' % (scan.name, m.name)),
           '<h1>%s: unicodemap.%s</h1>' % (html.escape(scan.name), html.escape(m.name)),
           '<p>Glyph(s) that the key types on the machine (from its font), next to the '
           'character in the unicodemap. Check that they are the same character.</p>']
    if msgs:
        out.append('<h2>Differences</h2><ul>%s</ul>' % ''.join(
            '<li>%s</li>' % html.escape(x) for x in msgs))
    out.append('<h2>Characters with a key</h2><table><tr><th>key</th><th>machine</th>'
               '<th>map</th><th>Unicode</th><th>name</th></tr>')
    for e in m.entries:
        if not (e.typeable and isinstance(e.key, int) and e.key >= 0x20 and e.key != 0x7f):
            continue
        k = (e.row, e.bit, e.mods)
        if k not in scan.chars:
            continue
        got = scan.chars[k]
        good = len(got) == 1 and e.key in cs.letters(got[0])
        out.append('<tr%s><td>%s</td><td>%s</td><td><span class="u">%s</span></td>'
                   '<td>U+%04X</td><td class="small">%s</td></tr>' % (
                       '' if good else ' class="bad"', keyname(*k),
                       ''.join(glyph_svg(scan.font, c) for c in got) or 'nothing',
                       html.escape(chr(e.key)), e.key, html.escape(cs.uname(e.key))))
    out.append('</table>')
    unt = [e for e in m.entries if not e.typeable and isinstance(e.key, int)]
    if unt:
        out.append('<h2>Characters without a key</h2><p>Glyph of the character set, if '
                   'the character set has it.</p><table><tr><th>code</th><th>glyph</th>'
                   '<th>map</th><th>Unicode</th><th>comment</th></tr>')
        for e in unt:
            c = cs.u2c.get(e.key)
            out.append('<tr><td>%s</td><td>%s</td><td><span class="u">%s</span></td><td>U+%04X</td>'
                       '<td class="small">%s</td></tr>' % (
                           '0x%02X' % c if c is not None else '', glyph_svg(scan.font, c) if c is not None else '',
                           html.escape(chr(e.key)), e.key, html.escape(e.comment)))
        out.append('</table>')
    out.append('</body></html>\n')
    with open(args.out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print(args.out)

# --- charset: make a character set mapping from a font -----------------------------

def cmd_charset(args):
    """Draft character set mapping for the font of a scan: codes whose glyph
    is the same as in the font of a scan with a known character set mapping
    get its Unicode character; the rest must be filled in by hand, by looking
    at the glyphs in the HTML page."""
    scan = Scan(args.scan)
    if scan.font is None:
        sys.exit('no font.txt in %s' % args.scan)
    known = {}   # glyph -> (unicode, source)
    known_left = {}   # the same, with the glyph shifted to the left edge
    for d in args.known:
        s = Scan(d)
        m = Unicodemap(map_path(args.share, s.meta['keyboard_type']))
        cs = charset_for(argparse_ns(args, charset=None), s, m)
        if s.font is None or not cs.c2u:
            print('%s: no font or no character set mapping, skipped' % d, file=sys.stderr)
            continue
        for code, us in cs.c2u.items():
            g = tuple(s.font[code])
            if any(g) and g not in known:
                known[g] = (us[0], '%s %s' % (s.name, cs.name))
                known_left.setdefault(left_aligned(g), known[g])
    lines, grid = [], []
    found = 0
    for code in range(256):
        g = tuple(scan.font[code])
        # (empty glyphs are left open: one of them is the space)
        hit = known.get(g) if any(g) else None
        shifted = False
        if not hit and any(g):
            # the same shape at another horizontal position (some fonts
            # have the same characters shifted by a pixel)
            hit = known_left.get(left_aligned(g))
            shifted = hit is not None
        if hit:
            found += 1
            u = hit[0]
            try:
                name = unicodedata.name(chr(u))
            except ValueError:
                name = ''
            lines.append('0x%02X\t0x%04X\t# %s%s' % (code, u, name,
                                                     ' (shifted glyph: check)' if shifted else ''))
        else:
            lines.append('#0x%02X\t0x????\t# TODO%s' % (code, ' (empty glyph)' if not any(g) else ''))
        grid.append((code, hit))
    with open(args.out + '.TXT', 'w', encoding='utf-8') as f:
        f.write('#\tName:   Map from the %s (video) character set to Unicode\n'
                '#\tDraft made with kbdtool.py charset; lines starting with #0x\n'
                '#\tstill need a Unicode character\n\n' % scan.name)
        f.write('\n'.join(lines) + '\n')
    out = [HTML_HEAD % html.escape('Character set of %s' % scan.name),
           '<h1>Character set of %s</h1>' % html.escape(scan.name),
           '<p>All 256 characters of the font, with the Unicode character from the '
           'draft mapping %s.TXT (from identical glyphs in known fonts), or ? where it '
           'must be filled in.</p><table class="grid">' % html.escape(os.path.basename(args.out))]
    out.append('<tr><th></th>%s</tr>' % ''.join('<th>x%X</th>' % i for i in range(16)))
    for r in range(16):
        cells = []
        for c in range(16):
            code, hit = grid[r * 16 + c]
            u = ('<span class="u">%s</span>' % html.escape(chr(hit[0])) if hit and hit[0] > 0x20
                 else ('' if hit else '<span class="bad">?</span>'))
            cells.append('<td>%s<br>%s<br><span class="small">%02X</span></td>' % (
                glyph_svg(scan.font, code), u, code))
        out.append('<tr><th>%Xx</th>%s</tr>' % (r, ''.join(cells)))
    out.append('</table></body></html>\n')
    with open(args.out + '.html', 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print('%s.TXT: %d of 256 codes found; %s.html shows the glyphs' % (args.out, found, args.out))

def left_aligned(g):
    """A glyph shifted to the left edge."""
    used = 0
    for row in g:
        used |= row
    shift = 0
    while used and not used & 0x80:
        used <<= 1
        shift += 1
    return tuple((row << shift) & 0xFF for row in g)

def argparse_ns(args, **kw):
    import argparse
    d = dict(vars(args))
    d.update(kw)
    return argparse.Namespace(**d)

# --- registration ------------------------------------------------------------------

def add_subcommands(sub):
    p = sub.add_parser('compare', help='compare scans with a unicodemap')
    p.add_argument('scans', nargs='+', help='scan directories')
    p.add_argument('--map', help="unicodemap name (e.g. 'int') or file; default: the "
                                 "keyboard_type of the machine")
    p.add_argument('--charset', help='character set mapping (default: the one of the unicodemap)')
    p.add_argument('-v', '--verbose', action='store_true', help='show the differences')
    p.set_defaults(func=cmd_compare)

    p = sub.add_parser('bestmap', help='find the unicodemaps that fit a scan best')
    p.add_argument('scans', nargs='+', help='scan directories')
    p.add_argument('--charset', help='character set mapping (default: the one of each unicodemap)')
    p.add_argument('--top', type=int, default=5)
    p.set_defaults(func=cmd_bestmap)

    p = sub.add_parser('genmap', help='generate a unicodemap from scans')
    p.add_argument('scans', nargs='+', help='scan directories (of machines that share the unicodemap)')
    p.add_argument('--charset', help='character set mapping (default: the one of --base)')
    p.add_argument('--base', help='an existing unicodemap to take the region and character set from')
    p.add_argument('--region', help='text for the #region: line')
    p.add_argument('--header', help='file with extra header text (comments about the machines)')
    p.add_argument('--out', help='output file (default: standard output)')
    p.set_defaults(func=cmd_genmap)

    p = sub.add_parser('charset', help='draft a character set mapping from the font of a scan')
    p.add_argument('scan', help='scan directory')
    p.add_argument('--known', nargs='*', default=[],
                   help='scan directories of machines whose character set mapping is known')
    p.add_argument('--out', required=True, help='output name (without extension): NAME.TXT and NAME.html')
    p.set_defaults(func=cmd_charset)

    p = sub.add_parser('report', help='HTML page to check a unicodemap visually against the font')
    p.add_argument('scan', help='scan directory')
    p.add_argument('--map', help='unicodemap (default: the keyboard_type of the machine)')
    p.add_argument('--charset', help='character set mapping (default: the one of the unicodemap)')
    p.add_argument('--out', help='output file (default: report.html in the scan directory)')
    p.set_defaults(func=cmd_report)
