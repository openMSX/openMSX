"""Exercise real SDL -> ImGui -> MSX mouse routing in an isolated profile.

The test driver is linked only into a separate test executable. --prepare-msvc
generates a project that reuses a completed Release/x64 build's object files,
replacing only main.cc with a copy that installs mouse-capture-test.hh.
No production source files or objects are modified.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]


def prepare_msvc():
    out = ROOT / 'derived/mouse-capture-test'
    out.mkdir(parents=True, exist_ok=True)
    original = (ROOT / 'src/main.cc').read_text(encoding='utf-8')
    anchor = '\t\t\treactor.runStartupScripts(parser);'
    assert original.count(anchor) == 1
    source = '#include "' + (ROOT / 'Contrib/mouse-capture-test.hh').as_posix() + '"\n' + original
    source = source.replace(anchor, '\t\t\tMouseCaptureTestDriver mouseTest(reactor);\n' + anchor)
    (out / 'mouse-capture-main.cc').write_text(source, encoding='utf-8')
    p = lambda path: escape(str(path), {'"': '&quot;'})
    project = f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="{p(ROOT / 'build/msvc/openmsx.vcxproj')}" />
  <PropertyGroup><TargetName>openmsx</TargetName></PropertyGroup>
  <ItemGroup>
    <ClCompile Remove="@(ClCompile)" />
    <ResourceCompile Remove="@(ResourceCompile)" />
    <ClCompile Include="{p(out / 'mouse-capture-main.cc')}" />
    <Object Include="{p(ROOT / 'derived/x64-VC-Release/build/*.obj')}"
            Exclude="{p(ROOT / 'derived/x64-VC-Release/build/main.obj')}" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <Manifest><AdditionalManifestFiles>{p(ROOT / 'build/msvc/ActiveCodePageUTF8.manifest')}</AdditionalManifestFiles></Manifest>
  </ItemDefinitionGroup>
</Project>
'''
    (out / 'mouse-capture-test.vcxproj').write_text(project, encoding='utf-8')
    runtime = ROOT / 'derived/x64-VC-Release/install/ogg.dll'
    if runtime.exists():
        (out / 'install').mkdir(exist_ok=True)
        shutil.copy2(runtime, out / 'install/ogg.dll')
    print(out / 'mouse-capture-test.vcxproj')
    print('Build with the same toolset and third-party properties as the production build, plus:')
    print(f'/p:OpenMSXRootDir={ROOT}')
    print(f'/p:OpenMSXIntDir={out / "build"}')
    print(f'/p:OpenMSXOutDir={out / "install"}')
    print(f'/p:OpenMSXConfigDir={ROOT / "derived/x64-VC-Release/config"}')


class Emulator:
    def __init__(self, exe, out, synthetic_focus=False):
        self.out, self.log = out, []
        user = out / 'home/share'
        shutil.copytree(ROOT / 'Contrib/cbios', user / 'machines')
        (user / 'imgui.ini').write_text('[openmsx][machine]\nshowQuickSetupEditor=0\n', encoding='utf-8')
        start = out / 'start.tcl'
        start.write_text('''set save_settings_on_exit false
set renderer SDLGL-PP
set mute on
set pointer_hide_delay 0
proc poll_mouse_test {} {
    after realtime 0.01 poll_mouse_test
    set request [file join $::env(MOUSE_TEST_DIR) request.tcl]
    if {![file exists $request]} {return}
    set f [open $request r]; set script [read $f]; close $f
    file delete $request
    set status [catch {uplevel #0 $script} value]
    set f [open [file join $::env(MOUSE_TEST_DIR) response.tmp] w]
    puts $f $status
    puts $f [binary encode base64 -maxlen 0 [encoding convertto utf-8 $value]]
    close $f
    file rename [file join $::env(MOUSE_TEST_DIR) response.tmp] [file join $::env(MOUSE_TEST_DIR) response.txt]
}
after realtime 0.01 poll_mouse_test
''', encoding='utf-8')
        env = dict(os.environ, OPENMSX_HOME=str(out / 'home'), OPENMSX_USER_DATA=str(user),
                   OPENMSX_SYSTEM_DATA=str(ROOT / 'share'), MOUSE_TEST_DIR=str(out), SDL_AUDIODRIVER='dummy',
                   SDL_HINT_FORCE_RAISEWINDOW='1')
        env.pop('SDL_VIDEODRIVER', None)
        if synthetic_focus:
            env['MOUSE_TEST_SYNTHETIC_FOCUS'] = '1'
        self.stderr = (out / 'stderr.txt').open('w')
        self.process = subprocess.Popen([str(exe), '-machine', 'C-BIOS_MSX2+', '-script', str(start)],
            env=env, stdout=self.stderr, stderr=self.stderr,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))

    def command(self, script):
        temporary = self.out / 'request.tmp'
        temporary.write_text(script, encoding='utf-8')
        temporary.rename(self.out / 'request.tcl')
        response = self.out / 'response.txt'
        until = time.monotonic() + 20
        while not response.exists():
            assert self.process.poll() is None, 'Emulator exited; see ' + str(self.out / 'stderr.txt')
            assert time.monotonic() < until, 'Timed out: ' + script
            time.sleep(0.01)
        while True:
            try:
                data = response.read_text()
                response.unlink()
                break
            except PermissionError:
                assert time.monotonic() < until
                time.sleep(0.01)
        status, encoded = data.splitlines()
        value = base64.b64decode(encoded).decode('utf-8')
        self.log.append([script, status, value])
        assert status == '0', (script, value)
        return value

    def test(self, args):
        return self.command('mouse_capture_test ' + args)

    def settle(self, seconds=0.12):
        time.sleep(seconds)

    def state(self):
        value = self.command('set s [mouse_capture_test state]; set pairs {}; '
            'dict for {k v} $s {lappend pairs [format {%s:%s} $k '
            '[binary encode base64 -maxlen 0 [encoding convertto utf-8 $v]]]}; join $pairs "\\n"')
        return {key: base64.b64decode(encoded).decode('utf-8')
                for key, encoded in (line.split(':', 1) for line in value.splitlines())}

    def assert_state(self, capture, relative, cursor):
        self.settle()
        state = self.state()
        assert (state['capture'], state['relative'], state['cursor']) == (capture, str(relative), str(cursor)), state
        return state

    def click(self, button=1, x=None, y=None):
        if x is None:
            x, y = map(int, self.test('point').split())
        self.test(f'button 1 {button} {x} {y}')
        self.settle(0.04)
        self.test(f'button 0 {button} {x} {y}')
        self.settle()

    def capture(self):
        self.test('focus')
        self.settle()
        x, y = map(int, self.test('point').split())
        self.test(f'hover {x} {y}')
        self.settle()
        self.click(x=x, y=y)
        return self.assert_state('captured', 1, 0)

    def key(self, code, mods=0):
        self.test(f'key 1 {code} {mods}')
        self.settle(0.04)
        self.test(f'key 0 {code} {mods}')
        self.settle()

    def close(self):
        if self.process.poll() is None:
            try:
                self.command('after realtime 0.05 exit')
                self.process.wait(timeout=5)
            except (Exception, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait()
        self.stderr.close()
        (self.out / 'commands.json').write_text(json.dumps(self.log, indent=2), encoding='utf-8')


def run(exe, synthetic_focus=False):
    out = Path(tempfile.mkdtemp(prefix='mouse-capture-', dir=ROOT / 'derived'))
    e = Emulator(exe, out, synthetic_focus)
    passed = []
    try:
        e.command('set power on; set pause off')
        e.settle(0.5)
        assert e.state()['capture'] == 'disabled'
        e.command('plug joyporta mouse')
        e.test('focus')
        e.assert_state('released', 0, 1)
        passed.append('Connected mouse starts released; pointer_hide_delay=0 cannot hide its GUI pointer')

        e.test('hover 50 10')
        e.settle()
        e.click(x=50, y=10)
        s = e.assert_state('released', 0, 1)
        assert s['popup'] == '1', s
        e.key(27)  # Escape closes the menu, without using an MSX hotkey
        e.key(27)  # A submenu may have opened under the injected cursor.
        passed.append('A menu click opens the real GUI menu and never captures or reaches the MSX')

        assert 'type_clipboard' in e.command('bind "mouse button2 down"')
        # Exercise the real default binding and type_clipboard procedure without
        # reading the host clipboard or typing into the running C-BIOS machine.
        e.command('set saved_type_proc $default_type_proc; set paste_count 0; '
                  'proc ::type::get_clipboard_text {} {return "capture\\npaste"}; '
                  'proc ::capture_test_type {text} {incr ::paste_count; set ::paste_text $text}; '
                  'set default_type_proc ::capture_test_type')
        try:
            e.capture()
            e.click(2)
            e.assert_state('released', 0, 1)
            assert e.command('set paste_count') == '0'  # Release must not paste.
            e.click(2)
            e.assert_state('released', 0, 1)
            assert e.command('set paste_count') == '1'
            assert e.command('set paste_text') == 'capture\rpaste'

            # A menu click must not paste, even with stale hover coordinates.
            e.click(2, x=50, y=10)
            assert e.command('set paste_count') == '1'
            e.test('hover 50 10')
            e.settle()
            e.click(2)  # Actual event is on the display, despite menu hover.
            assert e.command('set paste_count') == '2'

            # Keep user bindings working instead of hardcoding type_clipboard.
            e.command('set custom_middle 0; bind -msx "mouse button2 down" {incr ::custom_middle}')
            e.click(2)
            assert e.command('set custom_middle') == '1'
            assert e.command('set paste_count') == '2'
            e.command('set pause on')
            e.click(2)
            e.assert_state('released', 0, 1)
            assert e.command('set custom_middle') == '2'
            e.command('set pause off')
        finally:
            e.command('bind -msx "mouse button2 down" type_clipboard; '
                      'set default_type_proc $saved_type_proc; '
                      'rename ::type::get_clipboard_text {}; rename ::capture_test_type {}')
        passed.append('Middle-click releases without pasting; released display clicks retain clipboard and custom bindings, not menu clicks')

        e.test('reset')
        e.test('motion 20 10 300 240')
        e.settle()
        assert e.state()['motions'] == '0'
        e.capture()
        s = e.state()
        assert s['downs'] == s['ups'] == '0', s
        assert s['buttons'] == '48', s
        passed.append('Uncaptured motion is blocked; the activating click does not reach the MSX')

        e.test('motion 30 20 2 2')
        e.settle()
        s = e.assert_state('captured', 1, 0)
        assert int(s['motions']) > 0, s
        e.test('button 1 1 320 240')
        e.settle()
        assert e.state()['buttons'] == '32'
        e.click(2)
        s = e.assert_state('released', 0, 1)
        assert s['buttons'] == '48', s
        e.test('button 0 1 320 240')
        passed.append('Capture survives menu-edge motion; middle release clears a held MSX button')

        e.command('set inputdelay 0.25')
        e.capture()
        e.test('button 1 3 320 240')
        e.click(2)
        e.settle(0.5)
        s = e.assert_state('released', 0, 1)
        assert s['buttons'] == '48', s
        e.test('button 0 3 320 240')
        e.command('set inputdelay 0')
        passed.append('Delayed button-down events cannot leave the MSX button stuck after release')

        e.capture()
        e.key(1073741891, 192)  # SDL F10, KMOD_CTRL
        e.assert_state('captured', 1, 0)
        e.click(2)
        e.assert_state('released', 0, 1)
        e.test('motion 3 4 300 240')
        e.assert_state('released', 0, 1)
        passed.append('No default Ctrl+F10 release; middle-click releases without hover recapture')

        e.command('bind "keyb CTRL+F8" escape_grab')
        e.capture()
        e.key(1073741889, 192)
        e.assert_state('released', 0, 1)
        passed.append('An optional user binding can release capture through escape_grab')

        e.capture()
        e.key(1073741891)  # F10 opens console
        e.assert_state('released', 0, 1)
        e.command('set console off')
        e.capture()
        e.command('debug break')
        e.assert_state('released', 0, 1)
        e.command('debug cont')
        passed.append('Opening the console or breaking into the debugger releases capture')

        e.capture()
        e.command('set pause on')
        e.assert_state('released', 0, 1)
        e.command('set pause off')
        e.capture()
        e.test('lost_focus')
        e.assert_state('released', 0, 1)
        passed.append('Pause and loss of window focus release capture')

        e.capture()
        e.command('set auto_mouse_capture off')
        e.settle()
        assert e.state()['capture'] == 'disabled'
        assert e.state()['relative'] == '0'
        e.command('set auto_mouse_capture on')
        e.assert_state('released', 0, 1)
        passed.append('Disabling automatic capture restores legacy mode; re-enabling starts released')

        e.test('layout 1')
        e.test('statusbar 1')
        e.settle()
        e.capture()
        e.test('motion 2000 2000 600 479')
        e.assert_state('captured', 1, 0)
        e.click(2)
        e.assert_state('released', 0, 1)
        e.test('layout 0')
        passed.append('Undocked menu and visible status bar do not steal captured motion')

        e.command('set fullscreen on')
        e.settle(0.3)
        e.capture()
        e.click(2)
        e.assert_state('released', 0, 1)
        e.command('set fullscreen off')
        passed.append('Capture and release work in fullscreen')

        e.capture()
        e.command('escape_grab')
        e.assert_state('released', 0, 1)
        passed.append('The existing escape_grab command releases automatic capture')

        e.capture()
        e.test('button 1 1 320 240')
        e.settle()
        assert e.state()['buttons'] == '32'
        e.command('set oldID [activate_machine]; set newID [create_machine]; ${newID}::load_machine C-BIOS_MSX2+; activate_machine $newID')
        e.settle()
        assert e.state()['capture'] == 'disabled'
        assert e.state()['relative'] == '0'
        e.command('activate_machine $oldID; delete_machine $newID')
        s = e.assert_state('released', 0, 1)
        assert s['buttons'] == '48', s
        e.test('button 0 1 320 240')
        passed.append('Switching machines releases capture and clears the old machine mouse button')

        e.capture()
        e.command('unplug joyporta')
        e.settle()
        assert e.state()['capture'] == 'disabled'
        assert e.state()['relative'] == '0'
        passed.append('Disconnecting the MSX mouse releases native capture')

        print(json.dumps(passed, indent=2))
    finally:
        e.close()
        (out / 'results.json').write_text(json.dumps({'synthetic_focus': synthetic_focus, 'passed': passed}, indent=2), encoding='utf-8')
        print('Results:', out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-msvc', action='store_true')
    parser.add_argument('--openmsx', type=Path, help='Executable containing the test-only driver')
    parser.add_argument('--synthetic-focus', action='store_true', help='Inject SDL focus for noninteractive build desktops; does not validate native OS focus behavior')
    args = parser.parse_args()
    if args.prepare_msvc:
        prepare_msvc()
    elif args.openmsx:
        run(args.openmsx.resolve(strict=True), args.synthetic_focus)
    else:
        parser.error('Specify --prepare-msvc or --openmsx')
