"""Tools to investigate the keyboard of an MSX (or SVI, Sega) machine in
openMSX, and to make or check its unicodemap. See README.md.

Subcommands:
  machines   list machines with their keyboard settings
  boot       take screenshots while a machine boots (to find out how to get
             it into BASIC; see recipes.py)
  scan       press all keys with all modifier combinations, and record what
             the machine types
  compare    compare scans with a unicodemap
  bestmap    find the existing unicodemaps that fit a scan best
  genmap     generate a unicodemap from the scans of one or more machines
  geninfo    generate the keyboard_info file (key names, character set) for
             a unicodemap
  checkinfo  check the keyboard_info files (with scans: also the names of the
             keys that don't type a character)
  charset    draft a character set mapping (code -> Unicode) from the font
             of a scan, for a machine whose character set has none yet
  report     make an HTML page with the glyphs of the machine's font next to
             the characters of a unicodemap (for a visual check)
  typetest   type all characters of a unicodemap with openMSX's 'type'
             command, and check that the machine shows the same as in the scan

Only uses the Python 3 standard library; the scans run in openMSX (Tcl).
"""
import argparse
import concurrent.futures
import glob
import html
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

from . import keyinfo, unicodemaps
from .matrices import MATRICES, BASIC_DEFAULTS, matrix_for_type
from .recipes import recipe_for, key_positions
from .unicodemaps import Charset, Scan, Unicodemap, charset_path, map_path

HERE = os.path.dirname(os.path.abspath(__file__))

# --- finding openMSX and its data -----------------------------------------

def default_share():
    """The share directory of the openMSX source tree that these tools are
    in (with the machine configs and unicodemaps to investigate)."""
    d = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', 'share'))
    return d if os.path.isdir(os.path.join(d, 'machines')) else None

# --- machines --------------------------------------------------------------

def machine_info(path):
    """Keyboard related info of a machine config."""
    root = ET.parse(path).getroot()
    def text(tag, default=None):
        e = root.find('.//' + tag)
        return e.text.strip() if e is not None and e.text else default
    info = root.find('info')
    mtype = info.findtext('type', '').strip() if info is not None else ''
    vdp = root.find('.//VDP/version')
    return dict(
        name=os.path.splitext(os.path.basename(path))[0],
        type=mtype,
        matrix=matrix_for_type(mtype)['name'],
        keyboard_type=text('keyboard_type', 'int'),
        code_kana_locks=text('code_kana_locks', 'false') == 'true',
        graph_locks=text('graph_locks', 'false') == 'true',
        vdp=vdp.text.strip() if vdp is not None and vdp.text else 'TMS99X8',
    )

def find_machine(share, name):
    for d in share:
        p = os.path.join(d, 'machines', name + '.xml')
        if os.path.exists(p):
            return p
    sys.exit('machine %s not found in %s' % (name, ', '.join(share)))

def cmd_machines(args):
    for d in args.share:
        for p in sorted(glob.glob(os.path.join(d, 'machines', '*.xml'))):
            try:
                i = machine_info(p)
            except ET.ParseError as e:
                print('%s: %s' % (p, e), file=sys.stderr)
                continue
            if not i['type'] or i['type'] in ('NABU',):
                continue
            print('%-36s %-10s %-6s %-18s code_locks=%-5s graph_locks=%s' % (
                i['name'], i['type'], i['matrix'], i['keyboard_type'],
                str(i['code_kana_locks']).lower(), str(i['graph_locks']).lower()))

# --- running openMSX with a job -------------------------------------------

def tcl(value):
    """A Python value as a Tcl word."""
    if isinstance(value, bool):
        return '1' if value else '0'
    if isinstance(value, int):
        return str(value)
    if isinstance(value, (list, tuple)):
        return '[list %s]' % ' '.join(tcl(v) for v in value)
    s = str(value)
    for a, b in (('\\', '\\\\'), ('"', '\\"'), ('$', '\\$'), ('[', '\\['),
                 (']', '\\]'), ('\r', '\\r'), ('\n', '\\n')):
        s = s.replace(a, b)
    return '"%s"' % s

def keys_tcl(positions):
    """[(row, bit), ...] -> flat list, for the job file"""
    return [x for p in positions for x in p]

COMBOS = ['none', 'SHIFT', 'GRAPH', 'SHIFT+GRAPH', 'CODE', 'SHIFT+CODE']
SPECIAL_COMBOS = ['none', 'SHIFT', 'CTRL', 'CTRL+SHIFT', 'CTRL+GRAPH', 'CTRL+CODE']
# the characters of the BASIC program of the special keys scan
PROGRAM_CHARS = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ=$":+>(),<'

def machine_charset(args, info):
    """The character set mapping for a machine: --charset, else the one of
    the unicodemap of its keyboard_type (None if there is none)."""
    name = getattr(args, 'charset', None)
    if not name:
        try:
            name = Unicodemap(map_path(args.share, info['keyboard_type'])).charset
        except SystemExit:
            name = None
    if not name:
        return None
    p = name if os.path.exists(name) else charset_path(args.share, name)
    if not p:
        sys.exit('character set mapping %s not found' % name)
    return Charset(p)

def program_codes(cs):
    """The screen codes of the characters of the BASIC program. Without a
    character set mapping, the ASCII codes are tried (the program then only
    runs if they are right: the scan checks that)."""
    out = {}
    for ch in PROGRAM_CHARS:
        out[ch] = cs.u2c[ord(ch)] if cs and ord(ch) in cs.u2c else ord(ch)
    return out

def make_job(info, recipe, outdir, mode, args):
    m = MATRICES[info['matrix']]
    # the defaults for the BIOS/BASIC, or the ones of the recipe
    basic = {k: recipe.get(k, m[k]) for k in BASIC_DEFAULTS}
    job = dict(out=outdir, mode=mode, timeout=args.timeout)
    for name, pos in m['modifiers'].items():
        job['mod,' + name] = keys_tcl([pos])
    locks = ['CAPSLOCK']
    if recipe.get('code_kana_locks', info['code_kana_locks']):
        locks.append('CODE')
    if recipe.get('graph_locks', info['graph_locks']):
        locks.append('GRAPH')
    job['locks'] = locks
    job['skip'] = [list(p) for p in m['skip'] + list(recipe.get('skip_keys', []))]
    job['space'] = keys_tcl([m['space']])
    job['enter'] = keys_tcl([m['enter']])
    job['clear'] = keys_tcl(basic['clear'])
    job['buffer'], job['counter'] = basic['buffer'], basic['counter']
    job['inkey_release'] = int(basic['inkey_release'])
    hold = recipe.get('boot_hold')
    job['boot_hold'] = keys_tcl(key_positions(hold)) if hold else ''
    job['boot_hold_time'] = recipe.get('boot_hold_time', 8)
    job['menu_keys'] = [keys_tcl(key_positions(k)) for k in recipe.get('menu_keys', [])]
    job['menu_time'] = recipe.get('menu_time', 40)
    job['menu_interval'] = recipe.get('menu_interval', 2)
    job['menu_type'] = recipe.get('menu_type', '')
    job['start_time'] = '' if basic['start_time'] is None else basic['start_time']
    job['type_interval'] = recipe.get('type_interval', 0.4)
    job['caps_tap'] = keys_tcl([m['modifiers']['CAPSLOCK']]) if recipe.get('caps_tap') else ''
    fk = recipe.get('finish_key')
    job['finish_key'] = keys_tcl(key_positions(fk)) if fk else []
    job['screen_ignore'] = basic['screen_ignore']
    job['vdp_r4_mask'] = 0x3F if info['vdp'].startswith('V99') else 0x07
    job['shot_times'] = getattr(args, 'times', [])
    cs = machine_charset(args, info)
    pc = program_codes(cs)
    job['program_codes'] = [x for ch, c in pc.items() for x in (ch, c)]
    # the letters for the dead key test, with their screen codes
    letters = {ch: (cs.u2c.get(ord(ch), ord(ch)) if cs else ord(ch)) for ch in 'aeioucn'}
    job['dead_letters'] = [x for ch, c in letters.items() for x in (ch, c)]
    job['setup'] = basic['setup']
    combos = list(COMBOS)
    if info['keyboard_type'].startswith('jp'):
        combos += ['CODE+CAPSLOCK', 'SHIFT+CODE+CAPSLOCK']
    passes = [['chars', 'screen', combos, [list(p) for p in m['char_keys']]]]
    if not getattr(args, 'no_special', False):
        passes.append(['special', 'inkey', SPECIAL_COMBOS, [list(p) for p in m['special_keys']]])
    job['passes'] = passes
    job['typetest'] = []
    meta = dict(machine=info['name'], mode=recipe.get('mode'), matrix=info['matrix'],
                keyboard_type=info['keyboard_type'], charset=cs.name if cs else None,
                extension=recipe.get('extension'), locks=locks,
                dead_letters=letters)
    return job, meta

def write_job(job, path):
    with open(path, 'w', encoding='utf-8') as f:
        for k, v in job.items():
            if k == 'passes':
                f.write('set job(passes) [list %s]\n' % ' '.join(
                    '[list %s %s %s %s]' % (tcl(p[0]), tcl(p[1]), tcl(p[2]),
                                           tcl(p[3])) for p in v))
            else:
                f.write('set job(%s) %s\n' % (k, tcl(v)))

def default_systemroms():
    """The systemroms directory in the user directory of openMSX (the same
    rules as openMSX itself, see getUserDataDir() in
    src/file/FileOperations.cc)."""
    d = os.environ.get('OPENMSX_USER_DATA')
    if not d:
        home = os.environ.get('OPENMSX_HOME') or os.path.expanduser(
            '~/openMSX' if os.name == 'nt' else '~/.openMSX')
        d = os.path.join(home, 'share')
    return os.path.join(d, 'systemroms')

def tcl_word(s):
    """A string as a Tcl word, in braces (no substitutions)."""
    if '{' in s or '}' in s or s.endswith('\\'):
        sys.exit("can't use this path in a Tcl list: %s" % s)
    return '{%s}' % s

def private_home(outdir, args):
    """An openMSX user directory for one run: the user's own one may contain
    other (older) unicodemaps, machines or scripts, which must not influence
    the results. Only the system ROMs are used from there: via the file pool
    setting, in settings.xml."""
    home = os.path.join(outdir, 'home')
    share = os.path.join(home, 'share')
    os.makedirs(share, exist_ok=True)
    pool = '{-path %s -types system_rom}' % tcl_word(os.path.abspath(args.systemroms))
    with open(os.path.join(share, 'settings.xml'), 'w', encoding='utf-8') as f:
        f.write("<!DOCTYPE settings SYSTEM 'settings.dtd'>\n<settings>\n  <settings>\n"
                '    <setting id="__filepool">%s</setting>\n'
                '  </settings>\n</settings>\n' % html.escape(pool, quote=False))
    with open(os.path.join(share, 'imgui.ini'), 'w') as f:
        f.write('[openmsx][machine]\nshowQuickSetupEditor=0\n')
    return home, share

def run_openmsx(machine, ext, jobfile, outdir, args):
    cmd = [args.openmsx, '-machine', machine]
    if ext:
        cmd += ['-ext', ext]
    cmd += ['-script', jobfile, '-script', os.path.join(HERE, 'scan.tcl')]
    env = dict(os.environ)
    if not args.window:
        env['SDL_VIDEODRIVER'] = 'offscreen'
    env['OPENMSX_HOME'], env['OPENMSX_USER_DATA'] = private_home(outdir, args)
    # openMSX must use the same machine configs, extensions and unicodemaps
    # as these tools (the first --share directory)
    env['OPENMSX_SYSTEM_DATA'] = os.path.abspath(args.share[0])
    logname = os.path.join(outdir, 'log.txt')
    with open(logname, 'w') as log:
        proc = subprocess.Popen(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
        # an error in the scan script only gives a warning in the log (and the
        # script stops): then stop openMSX instead of waiting for the timeout
        error = None
        try:
            for _ in range(args.timeout + 30):
                try:
                    proc.wait(timeout=1)
                    break
                except subprocess.TimeoutExpired:
                    pass
                with open(logname, errors='replace') as f:
                    error = next((l.strip() for l in f
                                  if 'While executing' in l
                                  or 'Error executing delayed command' in l), None)
                if error:
                    break
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
        if error:
            return 'ERROR in the Tcl script: %s' % error
    try:
        with open(os.path.join(outdir, 'status.txt')) as f:
            return f.read().strip()
    except OSError:
        return 'no status (see %s)' % os.path.join(outdir, 'log.txt')

def outdir_for(base, machine, mode):
    return os.path.join(base, machine + ('__' + mode if mode else ''))

def run_jobs(args, mode_name, adapt=None):
    print('openMSX: %s' % args.openmsx, flush=True)
    share = args.share
    def one(spec):
        machine, _, mode = spec.partition(':')
        info = machine_info(find_machine(share, machine))
        recipe = recipe_for(machine, mode or None)
        out = os.path.abspath(outdir_for(args.out, machine, recipe.get('mode')))
        os.makedirs(out, exist_ok=True)
        job, meta = make_job(info, recipe, out, mode_name, args)
        if adapt:
            adapt(job, meta, out)
        jobfile = os.path.join(out, 'job.tcl')
        write_job(job, jobfile)
        with open(os.path.join(out, 'meta.json'), 'w') as f:
            json.dump(meta, f, indent=1)
        st = run_openmsx(machine, recipe.get('extension'), jobfile, out, args)
        return spec, st, out
    # (printed as soon as each machine is finished, with a counter, so that
    # it shows when something is stuck)
    n = len(args.machines)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        futures = [ex.submit(one, m) for m in args.machines]
        for i, f in enumerate(concurrent.futures.as_completed(futures), 1):
            spec, st, out = f.result()
            print('[%d/%d] %-40s %s  (%s)' % (i, n, spec, st, out), flush=True)

def cmd_scan(args):
    run_jobs(args, 'scan')

def cmd_boot(args):
    run_jobs(args, 'boot')

# --- typetest ---------------------------------------------------------------

def typetest_chars(scan, m):
    """The characters of a unicodemap to test: those with a key (without
    CTRL) that the scan pressed, with what that key typed in the scan."""
    out = []
    for e in m.entries:
        if not (e.typeable and isinstance(e.key, int)) or e.key <= 0x20 or e.key == 0x7f \
                or e.key == 0xa0 or 'CTRL' in e.mods:
            continue
        want = scan.chars.get((e.row, e.bit, e.mods))
        if want:
            out.append((e.key, want))
    return out

def cmd_typetest(args):
    """Boot the machine, type each character of the unicodemap with the
    'type' command in the screen editor, and compare the codes on the screen
    with what its key typed in the scan."""
    scans = {}
    def adapt(job, meta, out):
        scan = Scan(args.scan or out)
        # (the keyboard_type in the machine config now: openMSX types with
        # that one, and it can differ from the one at the time of the scan)
        m = Unicodemap(map_path(args.share, args.map or meta['keyboard_type']))
        chars = typetest_chars(scan, m)
        scans[out] = (scan, m, dict(chars))
        job['mode'] = 'typetest'
        job['passes'] = []
        job['typetest'] = [u for u, _ in chars]
        # (keep the meta.json of the scan)
        meta.clear()
        meta.update(scan.meta)
    run_jobs(args, 'typetest', adapt)
    for out, (scan, m, want) in scans.items():
        bad, ok, ghosted, skipped = [], 0, 0, 0
        cs = unicodemaps.charset_for(args, scan, m)
        p = os.path.join(out, 'typetest.txt')
        if not os.path.exists(p):
            print('%s: no results' % out)
            continue
        for line in open(p):
            f = line.split()
            u = int(f[0])
            if f[1:] == ['skipped']:
                skipped += 1
                continue
            got = [int(x) for x in f[1].split(',')] if len(f) > 1 else []
            if got == want[u]:
                ok += 1
            elif len(want[u]) > 1 and len(got) == 1 and got[0] in want[u] \
                    and u in cs.letters(got[0]):
                # the key ghosts when pressed in the scan, 'type' doesn't
                # ghost: only the right character
                ok += 1
                ghosted += 1
            else:
                bad.append('U+%04X %s: typed %s, the key gives %s' % (
                    u, chr(u), got or 'nothing', want[u]))
        print('%s with unicodemap.%s: %d ok%s, %d bad%s' % (
            scan.name, m.name, ok,
            ' (%d where the key ghosts in the scan)' % ghosted if ghosted else '', len(bad),
            ', %d skipped (above U+FFFF: not possible with the Tcl version of this openMSX)'
            % skipped if skipped else ''))
        for b in bad:
            print('  ' + b)

# --- main -------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--share', action='append',
                    help='openMSX share directory with machines/ and keyboard_info/ '
                         '(can be given more than once; default: the one of this '
                         'source tree)')
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('machines', help='list machines and their keyboard settings')
    p.set_defaults(func=cmd_machines)

    def run_options(p):
        p.add_argument('machines', nargs='+',
                       help='machine names (as in share/machines), optionally '
                            'with a mode: Yamaha_AX500:international')
        p.add_argument('--out', default='scans', help='output directory (default: scans)')
        p.add_argument('--openmsx', default='openmsx',
                       help='the openMSX executable (default: openmsx, from the PATH)')
        p.add_argument('--jobs', type=int, default=os.cpu_count() or 2,
                       help='number of openMSX instances at the same time')
        p.add_argument('--window', action='store_true',
                       help="show the openMSX windows (default: headless, with SDL's "
                            "offscreen video driver)")
        p.add_argument('--systemroms', default=default_systemroms(),
                       help='directory with the system ROMs (default: %(default)s)')
        p.add_argument('--timeout', type=int, default=900,
                       help='seconds (real time) per machine')

    p = sub.add_parser('boot', help='screenshots while booting')
    run_options(p)
    p.add_argument('--times', type=int, nargs='+', default=[5, 10, 20, 30, 45, 60, 90],
                   help='emulated seconds for the screenshots')
    p.set_defaults(func=cmd_boot)

    p = sub.add_parser('scan', help='scan the keyboard')
    run_options(p)
    p.add_argument('--charset', help='character set mapping, for typing the BASIC program '
                                     '(default: the one of the unicodemap of the machine)')
    p.add_argument('--no-special', action='store_true',
                   help='skip the scan of special keys and CTRL combinations')
    p.set_defaults(func=cmd_scan)

    p = sub.add_parser('typetest', help="test the 'type' command with a unicodemap")
    run_options(p)
    p.add_argument('--scan', help='scan directory to compare with (default: the one in --out)')
    p.add_argument('--map', help='unicodemap to compare with (default: the keyboard_type of '
                                 'the machine; openMSX always types with that one)')
    p.add_argument('--charset', help=argparse.SUPPRESS)
    p.set_defaults(func=cmd_typetest)

    unicodemaps.add_subcommands(sub)
    keyinfo.add_subcommands(sub)

    args = ap.parse_args()
    if not args.share:
        d = default_share()
        if not d:
            sys.exit('give the openMSX share directory with --share')
        args.share = [d]
    args.func(args)

