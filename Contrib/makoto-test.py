"""Makoto integration: synthetic CPU/IO tests and save-state continuation."""
import argparse,base64,gzip,json,os,re,shutil,subprocess,tempfile,time
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
def image(marker, size=0x40000):
    data = bytearray([255]) * size
    data[:16] = b'AB\x20\x40' + bytes(12)
    data[16:24] = b'MAKOTEST'
    # DI; repeatedly read a cartridge byte and copy it into RAM.
    data[32:42] = bytes.fromhex('f3 3a 00 50 32 00 c0 c3 21 40')
    data[0x1000] = marker
    return bytes(data)


def executable_path(value):
    """Accept an explicit native openMSX executable, never a shell/batch command."""
    path = Path(value).expanduser().resolve(strict=True)
    pattern = r'openmsx(?:[-_.][A-Za-z0-9_.-]+)?'
    if os.name == 'nt':
        pattern += r'\.exe'
    if not path.is_file() or not re.fullmatch(pattern, path.name, re.IGNORECASE):
        raise ValueError('Expected a native openMSX executable file')
    if os.name == 'nt':
        with path.open('rb') as stream:
            if stream.read(2) != b'MZ':
                raise ValueError('Expected a Windows executable, not a command script')
    return path


def tcl_path(path):
    # A command substitution that returns exactly one value, even for braces,
    # dollar signs, semicolons, brackets or newlines in a filesystem path.
    encoded = base64.b64encode(path.as_posix().encode('utf-8')).decode('ascii')
    return '[encoding convertfrom utf-8 [binary decode base64 {' + encoded + '}]]'


class Emulator:
    def __init__(self, exe, out, firmware, rom, mapper, ips=None):
        exe = executable_path(exe)
        if mapper not in ('ASCII16', 'ASCII16-X', 'Yamanooto'):
            raise ValueError('Unsupported synthetic-test mapper')
        out = out.resolve(strict=True)
        rom = rom.resolve(strict=True)
        firmware = firmware.resolve(strict=True)
        if ips is not None:
            ips = ips.resolve(strict=True)
        self.out = out
        self.request = out / 'request.tcl'
        self.response = out / 'response.txt'
        self.log = []
        user = out / 'home/share'
        (user / 'systemroms').mkdir(parents=True)
        for name in ('nms8250_basic-bios2.rom', 'nms8250_msx2sub.rom', 'nms8250_disk.rom'):
            shutil.copy2(firmware / name, user / 'systemroms' / name)
        script = out / 'start.tcl'
        script.write_text("""set renderer none
set throttle off
set save_settings_on_exit false
set mute on
proc poll_test {} {
    set request [file join $::env(ROM_TEST_DIR) request.tcl]
    if {[file exists $request]} {
        set f [open $request r];set script [read $f];close $f
        file delete $request
        set status [catch {uplevel #0 $script} value]
        set f [open [file join $::env(ROM_TEST_DIR) response.tmp] w]
        puts $f $status
        puts $f [binary encode base64 -maxlen 0 [encoding convertto utf-8 $value]]
        close $f
        file rename [file join $::env(ROM_TEST_DIR) response.tmp] [file join $::env(ROM_TEST_DIR) response.txt]
    }
    after realtime 0.01 poll_test
}
after realtime 0.01 poll_test
""")
        env = dict(os.environ, OPENMSX_HOME=str(out / 'home'), OPENMSX_USER_DATA=str(user),
                   OPENMSX_SYSTEM_DATA=str(ROOT / 'share'), ROM_TEST_DIR=str(out),
                   SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        args = [str(exe), '-machine', 'Philips_NMS_8250', '-cart', str(rom), '-romtype', mapper]
        if ips:
            args += ['-ips', str(ips)]
        args += ['-script', str(script)]
        self.stderr = (out / 'stderr.txt').open('w')
        self.process = subprocess.Popen(args, shell=False, env=env, stdout=self.stderr, stderr=self.stderr,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        try:
            self.command('set power on; set pause off')
            self.advance(3)
        except BaseException:
            self.process.kill(); self.process.wait(); self.stderr.close()
            (self.out / 'commands.json').write_text(json.dumps(self.log, indent=2))
            raise

    def command(self, script):
        temporary = self.out / 'request.tmp'
        temporary.write_text(script, encoding='utf-8')
        temporary.rename(self.request)
        until = time.monotonic() + 20
        while not self.response.exists():
            assert self.process.poll() is None, 'Emulator exited: ' + (self.out / 'stderr.txt').read_text()
            assert time.monotonic() < until, 'Command timed out: ' + script
            time.sleep(0.01)
        while True:
            try:
                response = self.response.read_text()
                self.response.unlink()
                break
            except PermissionError:
                assert time.monotonic() < until, 'Response file remained locked'
                time.sleep(0.01)
        status, value = response.splitlines()
        value = base64.b64decode(value).decode('utf-8')
        self.log.append((script, status, value))
        assert status == '0', (script, value)
        return value

    def advance(self, seconds=0.02):
        self.command(f'set ::test_tick 0; after time {seconds} {{set ::test_tick 1}}')
        until = time.monotonic() + 20
        while self.command('set ::test_tick') != '1':
            assert time.monotonic() < until, 'Emulation did not advance'
            time.sleep(0.005)

    def close(self):
        try:
            if self.process.poll() is None:
                self.request.write_text('exit')
                self.process.wait(timeout=10)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            self.stderr.close()
            (self.out / 'commands.json').write_text(json.dumps(self.log, indent=2), encoding='utf-8')



def main():
 p=argparse.ArgumentParser();p.add_argument('--openmsx',type=Path,required=True);p.add_argument('--firmware-dir',type=Path,required=True);a=p.parse_args()
 out=Path(tempfile.mkdtemp(prefix='makoto-check-',dir=ROOT/'derived'));rom=out/'test.rom';rom.write_bytes(image(0x37));d=out/'run';d.mkdir()
 e=Emulator(a.openmsx,d,a.firmware_dir,rom,'ASCII16');passed=[]
 def write(reg,value):
  port=0x16 if reg>=256 else 0x14
  return e.command(f'debug write ioports {port} {reg&255}; debug write ioports {port+1} {value}')
 def read(port):return int(e.command(f'debug read ioports {port}'))
 def step(seconds):
  e.command(f'after time {seconds} {{set pause on}}; set pause off')
  until=time.monotonic()+20
  while e.command('set pause')!='true':
   assert time.monotonic()<until
 def snapshot(name):
  path=out/(name+'.oms');e.command('store_machine [machine] '+tcl_path(path));return path
 def core(path):
  root=ET.fromstring(gzip.decompress(path.read_bytes()))
  return ET.tostring(root.find('.//device[@type="Makoto"]/sound'))
 try:
  e.command('set pause on; ext Makoto')
  assert e.command('set makoto_master_volume')=='50'
  assert e.command('set makoto_psg_volume')=='50'
  passed.append('Master defaults to 50 percent and SSG to 50 percent')
  write(7,63)
  assert read(0x14)&128 and read(0x16)&128
  step(.0001);assert not(read(0x14)&128 or read(0x16)&128);passed.append('BUSY appears on both status ports and expires')
  write(0x29,0x80);write(0x110,0x1c)
  write(0x24,0xfe);write(0x25,0);write(0x27,5) # 8*144/8MHz=144us
  step(.00010);assert read(0x16)&1==0
  step(.00006);assert read(0x16)&1==1
  write(0x27,0x15);assert read(0x16)&1==0
  step(.00016);assert read(0x16)&1==1;passed.append('Timer A period, sticky overflow, acknowledge and reload')
  write(0x27,0x30);write(0x26,0xf0);write(0x27,0x0a) # 16 timer-B steps, allowing its free-running divider phase
  step(.003);assert read(0x16)&2==0
  step(.002);assert read(0x16)&2;passed.append('Timer B period and overflow')
  write(0x27,0x30);step(.001);assert read(0x16)&3==0;passed.append('Stopping timers cancels scheduled overflows')
  write(0,0x40);write(1,0);write(7,0x3e);write(8,15)
  assert e.command('debug read {Makoto registers} 8')=='15'
  write(0x24,0xf0);write(0x25,0);write(0x27,5)
  state=snapshot('before'); before=core(state);assert before
  for _ in range(30):read(0x16)
  assert core(snapshot('after-peek'))==before;passed.append('Debugger peeks preserve the entire chip state')
  step(.123);expected=core(snapshot('expected'))
  e.command('set old [machine]; set new [restore_machine '+tcl_path(state)+']; delete_machine $old; activate_machine $new')
  step(.123);actual=core(snapshot('actual'))
  assert actual==expected,'Chip/timer state diverged after restore'
  passed.append('Save/restore resumes the same chip, RAM, BUSY and timer state')
  e.command('reset');step(.01);assert read(0x16)&3==0;passed.append('MSX reset stops OPNA timers')
 finally:e.close()
 (out/'results.json').write_text(json.dumps({'passed':passed},indent=2));print(out);print('\n'.join(passed))
if __name__=='__main__':main()

