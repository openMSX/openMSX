"""How to get each machine into BASIC for a scan ("recipes").

Most MSX machines start in BASIC by themselves: they need no recipe. The
others have firmware, a start menu, a disk with MSX-DOS, etc. A recipe is a
dict with these (all optional) entries:

  menu_keys     keys to press one after another, e.g. ['DOWN', 'RETURN'] or
                ['SHIFT+4'], starting at menu_time, every menu_interval
                seconds (emulated time)
  menu_time     default 40
  menu_interval default 2
  menu_type     text to type after the menu keys, e.g. 'BASIC\\r' at the
                MSX-DOS prompt
  boot_hold     key to hold while booting (e.g. 'DEL' skips the firmware of
                some machines), released after boot_hold_time seconds
                (default 8)
  start_time    when to start the scan (default: see matrices.py)
  type_interval seconds between the keys when typing the BASIC program of
                the special keys scan (default 0.4)
  caps_tap      True: tap CAPS before scanning (machines that start with CAPS
                on)
  skip_keys     extra keys not to press, as (row, bit), e.g. a second CAPS
                LOCK key
  extension     openMSX extension to add (e.g. a BASIC cartridge)
  finish_key    key to press after each key in the chars pass (e.g. 'SPACE',
                so that the Arabic screen editor shows the final form)
  setup, clear, screen_ignore, buffer, counter, inkey_release
                replace the default for the BIOS/BASIC of the key matrix
                (see matrices.py), e.g. screen_ignore: the character codes
                on the screen that aren't typed characters (spaces, cursor)
  code_kana_locks, graph_locks
                override these settings of the machine config (for a mode
                in which they're different)

Machines with several modes (the Arabic machines) have a 'modes' dict with a
recipe per mode, and a 'default_mode'.

Key names: see KEYS below; 'A+B' presses keys together.
"""

# MSX key names, as (row, bit)
KEYS = {str(d): (0, d) for d in range(8)}
KEYS.update({
    '8': (1, 0), '9': (1, 1),
    'SHIFT': (6, 0), 'CTRL': (6, 1), 'GRAPH': (6, 2), 'CAPS': (6, 3),
    'CODE': (6, 4), 'F1': (6, 5), 'F2': (6, 6), 'F3': (6, 7),
    'F4': (7, 0), 'F5': (7, 1), 'ESC': (7, 2), 'TAB': (7, 3), 'STOP': (7, 4),
    'BS': (7, 5), 'SELECT': (7, 6), 'RETURN': (7, 7),
    'SPACE': (8, 0), 'HOME': (8, 1), 'INS': (8, 2), 'DEL': (8, 3),
    'LEFT': (8, 4), 'UP': (8, 5), 'DOWN': (8, 6), 'RIGHT': (8, 7),
})
# F6-F10 are SHIFT+F1-F5
KEYS.update({'F%d' % (n + 5): None for n in range(1, 6)})


def key_positions(name):
    """'SHIFT+F3' -> [(6, 0), (6, 7)]"""
    out = []
    for part in name.split('+'):
        if part in ('F6', 'F7', 'F8', 'F9', 'F10'):
            out += [KEYS['SHIFT'], KEYS['F%d' % (int(part[1:]) - 5)]]
        else:
            out.append(KEYS[part])
    return out


_DOS = dict(menu_keys=['RETURN', 'RETURN'], menu_type='BASIC\r', menu_time=40,
            start_time=60)
_MENU = dict(menu_time=40, start_time=60)
_SLOW = dict(menu_time=70, start_time=90)


def _arabic(menu_key, ctrl_time=8):
    """Arabic machines: Arabic mode (default, BASIC from the start menu) or
    international mode (CTRL held while booting)."""
    return dict(default_mode='arabic', modes={
        'arabic': dict(menu_keys=[menu_key], menu_time=40, start_time=70,
                       finish_key='SPACE',
                       # the MSX default, and 0x80: the space of the AX200
                       # (and others) in Arabic mode
                       screen_ignore=[0x20, 0x7F, 0xFF, 0x80]),
        # (in international mode, CODE isn't a lock key)
        'international': dict(boot_hold='CTRL', boot_hold_time=ctrl_time,
                              start_time=ctrl_time + 20, code_kana_locks=False),
    })


RECIPES = {
    # start menus / firmware
    'Hitachi_MB-H1':       dict(_MENU, menu_keys=['F1']),
    'Hitachi_MB-H2':       dict(_MENU, menu_keys=['F1']),
    'Hitachi_MB-H21':      dict(_MENU, menu_keys=['F1']),
    'Hitachi_MB-H3':       dict(menu_keys=['F1'], menu_time=25, start_time=40),
    'Mitsubishi_ML-F120':  dict(_MENU, menu_keys=['9', 'RETURN']),
    'National_FS-4000':    dict(_MENU, menu_keys=['3']),
    'National_FS-4500':    dict(_MENU, menu_keys=['4']),
    'National_FS-4600F':   dict(_MENU, menu_keys=['4']),
    'National_FS-4700F':   dict(_MENU, menu_keys=['4']),
    'Panasonic_FS-A1':     dict(_MENU, menu_keys=['F8']),
    'Toshiba_HX-22I':      dict(_MENU, menu_keys=['1']),
    'Toshiba_HX-20E':      dict(menu_keys=['RETURN', '1'], menu_time=16,
                                menu_interval=6, start_time=30),
    'Sony_HB-55P':         dict(_SLOW, menu_keys=['DOWN'] * 4 + ['RETURN']),
    'Sony_HB-75':          dict(_SLOW, menu_keys=['DOWN'] * 5 + ['RETURN']),
    'Sony_HB-75D':         dict(_SLOW, menu_keys=['DOWN'] * 4 + ['RETURN']),
    'Sony_HB-75P':         dict(_SLOW, menu_keys=['DOWN'] * 4 + ['RETURN']),
    'Sony_HB-701':         dict(menu_keys=['F1'], menu_time=20, start_time=30),
    'Sanyo_PHC-77':        dict(menu_keys=['SPACE', 'DOWN', 'DOWN', 'RETURN'],
                                menu_time=20, start_time=35),
    'Daewoo_CPC-300':      dict(menu_keys=['SELECT'], menu_time=20, start_time=30),
    'Frael_Bruc_100_2':    dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-11':          dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-101':         dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-101P':        dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-201':         dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-201P':        dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-F1':          dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-F1II':        dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Sony_HB-F5':          dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Victor_HC-7':         dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    'Victor_HC-80':        dict(menu_keys=['RETURN'], menu_time=20, start_time=30),
    # 1: MSX BASIC + P-BASIC, 2: MSX BASIC
    'Pioneer_PX-7':        dict(menu_keys=['2'], menu_time=20, start_time=30),
    'Pioneer_PX-7UK':      dict(menu_keys=['2'], menu_time=20, start_time=30),
    'Pioneer_PX-V60':      dict(menu_keys=['2'], menu_time=20, start_time=30),
    'Sanyo_MPC-10mkII':    dict(menu_keys=['1'], menu_time=20, start_time=30),
    'Gradiente_Expert_Plus': dict(menu_keys=['1'], menu_time=20, start_time=30),
    # keys held while booting skip the firmware
    'Panasonic_FS-A1F':    dict(boot_hold='DEL', start_time=90),
    'Panasonic_FS-A1MK2':  dict(boot_hold='DEL', start_time=90),
    'Mitsubishi_ML-G10':   dict(boot_hold='DEL', start_time=90),
    'Mitsubishi_ML-G1_ES': dict(boot_hold='DEL', start_time=90),
    'Toshiba_FS-TM1':      dict(boot_hold='DEL'),
    'Sanyo_MPC-2300':      dict(boot_hold='DEL', start_time=30),  # skips the network ROM
    'Sony_HB-F9P':         dict(boot_hold='GRAPH', start_time=30),
    'Sony_HB-F9S':         dict(boot_hold='GRAPH', start_time=30),
    'Philips_NMS_8220':    dict(boot_hold='ESC', start_time=30),  # skips the painter
    # MSX-DOS from disk: go to BASIC
    'Sony_HB-701FD':       _DOS,
    'Spectravideo_SVI-738':    _DOS,
    'Spectravideo_SVI-738_DE': _DOS,
    'Spectravideo_SVI-738_DK': _DOS,
    'Spectravideo_SVI-738_PL': _DOS,
    'Spectravideo_SVI-738_SE': _DOS,
    'Gradiente_Expert_DD_Plus': _DOS,
    'National_CF-3300':    _DOS,
    # other particularities
    'Sharp_HB-8000_1.1':   dict(caps_tap=True),    # starts with CAPS on
    'Sharp_HB-8000_1.2':   dict(caps_tap=True),
    'Philips_NMS_8250-19': dict(skip_keys=[(2, 5)]),  # a second CAPS LOCK key
    'Yamaha_CX5MII-128':   dict(start_time=100),
    # Arabic machines
    'Al_Alamiah_AX170':    _arabic('5'),
    'Al_Alamiah_AX230':    _arabic('7'),
    'Al_Alamiah_AX370':    _arabic('5'),
    'Bawareth_Perfect_MSX1': _arabic('0'),
    'Yamaha_AX150':        _arabic('5'),
    'Yamaha_AX200':        _arabic('5', ctrl_time=25),
    'Yamaha_AX350II':      _arabic('4'),
    'Yamaha_AX350IIF':     _arabic('SHIFT+4'),  # AZERTY: the digits need SHIFT
    'Yamaha_AX500':        _arabic('5', ctrl_time=25),
    # The screen editor shows a Hangul vowel without consonant as an empty
    # cell (code 0, or 252 when typed with the type command) and the vowel
    'Daewoo_CPC-61_Zemmix_Super_V': dict(screen_ignore=[0x20, 0x7F, 0xFF, 0x00, 0xFC]),
    # Sega SC-3000: needs a BASIC cartridge
    'Sega_SC-3000':        dict(extension='Sega_BASIC_Level_IIIA_PAL'),
    'Sega_SC-3000_JP':     dict(extension='Sega_BASIC_Level_IIIA'),
}


def recipe_for(machine, mode=None):
    """The recipe for a machine (in the given mode, or its default mode)."""
    r = RECIPES.get(machine, {})
    if 'modes' in r:
        mode = mode or r['default_mode']
        if mode not in r['modes']:
            raise ValueError('%s has no mode %r (modes: %s)' % (
                machine, mode, ', '.join(r['modes'])))
        return dict(r['modes'][mode], mode=mode)
    if mode:
        raise ValueError('%s has no modes' % machine)
    return dict(r)
