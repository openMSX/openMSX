"""Two Makoto instances: independent settings, RAM, registers, IRQ and state."""
import argparse
import importlib.util
import json
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("makotest", ROOT / "Contrib/makoto-test.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
p = argparse.ArgumentParser()
p.add_argument("--openmsx", type=Path, required=True)
p.add_argument("--firmware-dir", type=Path, required=True)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix="makoto-instances-", dir=ROOT / "derived"))
rom = out / "test.rom"
rom.write_bytes(m.image(0x37))
run = out / "run"
run.mkdir()
e = m.Emulator(a.openmsx, run, a.firmware_dir, rom, "ASCII16")
passed = []

def step(seconds):
    e.command(f"after time {seconds} {{set pause on}}; set pause off")
    until = time.monotonic() + 20
    while e.command("set pause") != "true":
        assert time.monotonic() < until

def write(base, reg, value):
    port = base + (2 if reg >= 256 else 0)
    e.command(f"debug write ioports {port} {reg & 255}; debug write ioports {port+1} {value}")

try:
    e.command("set pause on; set first [ext Makoto]; set second [ext Makoto]")
    for name in ("Makoto", "Makoto (1)"):
        assert e.command("set {" + name + " SSG_volume}") == "75"
        assert e.command("debug size {" + name + " ADPCM RAM}") == "262144"
        assert e.command("debug size {" + name + " registers}") == "512"
        assert e.command("debug probe read {" + name + ".IRQ}") == "0"
    e.command("set {Makoto SSG_volume} 31; set {Makoto (1) SSG_volume} 67")
    assert e.command("set {Makoto SSG_volume}") == "31"
    assert e.command("set {Makoto (1) SSG_volume}") == "67"
    passed.append("Two identical extensions coexist with unique settings, debuggers and IRQ probes")
    e.command("remove_extension $second; remove_extension $first")
    extensions = run / "home/share/extensions"
    extensions.mkdir(exist_ok=True)
    xml = (ROOT / "share/extensions/Makoto.xml").read_text()
    (extensions / "MakotoAlt.xml").write_text(xml.replace('base="0x14"', 'base="0x18"'))
    e.command("set first [ext Makoto]; set second [ext MakotoAlt]")
    write(0x14, 0, 37)
    write(0x18, 0, 91)
    assert e.command("debug read {Makoto registers} 0") == "37"
    assert e.command("debug read {Makoto (1) registers} 0") == "91"
    e.command("debug write {Makoto ADPCM RAM} 123 165; debug write {Makoto (1) ADPCM RAM} 123 90")
    assert e.command("debug read {Makoto ADPCM RAM} 123") == "165"
    assert e.command("debug read {Makoto (1) ADPCM RAM} 123") == "90"
    pending = int(e.command("debug probe read z80.pendingIRQ"))
    for base in (0x14, 0x18):
        write(base, 0x29, 0x83)
        write(base, 0x110, 0x1c)
        write(base, 0x24, 0xfe)
        write(base, 0x25, 0)
        write(base, 0x27, 5)
    step(.001)
    assert int(e.command("debug probe read z80.pendingIRQ")) == pending + 2
    write(0x14, 0x27, 0x10)
    assert e.command("debug probe read Makoto.IRQ") == "0"
    assert e.command("debug probe read {Makoto (1).IRQ}") == "1"
    assert int(e.command("debug probe read z80.pendingIRQ")) == pending + 1
    passed.append("Separate I/O ranges preserve independent registers, RAM and simultaneous IRQs")
    state = out / "two.oms"
    e.command("store_machine [machine] " + m.tcl_path(state))
    e.command("set old [machine]; set restored [restore_machine " + m.tcl_path(state) + "]; activate_machine $restored; delete_machine $old")
    assert e.command("debug read {Makoto registers} 0") == "37"
    assert e.command("debug read {Makoto (1) registers} 0") == "91"
    assert e.command("debug read {Makoto ADPCM RAM} 123") == "165"
    assert e.command("debug read {Makoto (1) ADPCM RAM} 123") == "90"
    assert e.command("debug probe read Makoto.IRQ") == "0"
    assert e.command("debug probe read {Makoto (1).IRQ}") == "1"
    passed.append("Two-instance state restores both independent devices")
    # Extension names are serialized, and Tcl variables still refer to them.
    surviving_volume = e.command("set {Makoto (1) SSG_volume}")
    e.command("remove_extension $first")
    assert e.command("set {Makoto (1) SSG_volume}") == surviving_volume
    assert e.command("debug read {Makoto (1) ADPCM RAM} 123") == "90"
    assert e.command("debug probe read {Makoto (1).IRQ}") == "1"
    passed.append("Removing one cartridge leaves the other usable")
finally:
    e.close()
(out / "results.json").write_text(json.dumps(passed, indent=2))
print(json.dumps({"directory": str(out), "passed": passed}, indent=2))


