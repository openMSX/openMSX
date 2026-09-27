"""Makoto channel isolation, analogue filtering, RAM and legacy-state checks."""
import argparse, array, importlib.util, json, math, tempfile, time, wave
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("makotest", ROOT / "Contrib/makoto-test.py")
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
p = argparse.ArgumentParser()
p.add_argument("--openmsx", type=Path, required=True)
p.add_argument("--firmware-dir", type=Path, required=True)
p.add_argument("--legacy-state", type=Path)
p.add_argument("--baseline", type=Path)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix="makoto-integration-", dir=ROOT / "derived"))
rom = out / "test.rom"; rom.write_bytes(m.image(0x37))
results = {}; metrics = {}
variants = [("current", a.openmsx)]
if a.baseline: variants.append(("baseline", a.baseline))
for variant, exe in variants:
 directory = out / variant; directory.mkdir()
 e = m.Emulator(exe, directory, a.firmware_dir, rom, "ASCII16")
 def write(reg, value):
  port = 0x16 if reg >= 256 else 0x14
  e.command(f"debug write ioports {port} {reg & 255}; debug write ioports {port+1} {value}")
 def step(seconds):
  e.command(f"after time {seconds} {{set pause on}}; set pause off")
  until = time.monotonic()+20
  while e.command("set pause") != "true": assert time.monotonic() < until
 def record(label):
  step(.03)
  path = directory / (label + ".wav")
  e.command("soundlog start " + m.tcl_path(path)); step(.3); e.command("soundlog stop")
  with wave.open(str(path)) as f:
   assert f.getsampwidth() == 2 and f.getnchannels() == 2
   rate=f.getframerate(); data=np.frombuffer(f.readframes(f.getnframes()),dtype="<i2").reshape(-1,2).astype(float)
  data=data[2000:-2000]
  assert np.max(abs(data)) < 32767
  rms=np.sqrt(np.mean(data*data,axis=0))
  metrics[variant+"-"+label]={"rms":rms.tolist(),"peak":float(np.max(abs(data)))}
  return data,rate
 try:
  e.command("set pause on; ext Makoto; set mute off; set volume 50; set makoto_psg_volume 100")
  if variant == "baseline":
   e.command("set makoto_master_volume 100; set Makoto_volume 20")
  else:
   e.command("set Makoto_volume 20")
   assert e.command("llength [info vars Makoto_ch*_mute]") == "16"
  write(0,9);write(1,0);write(7,0x3e);write(8,15)
  x,rate=record("ssg-high")
  results[variant]={"high":x,"rate":rate}
  write(0,125)
  x,rate=record("ssg-low")
  results[variant]["low"]=x
  if variant == "baseline": continue
  e.command("set Makoto_ch1_record "+m.tcl_path(directory/"fm1-alone.wav"))
  separate,_=record("separate-buffers")
  e.command("set Makoto_ch1_record {}")
  ratio=float(np.sqrt(np.mean(separate*separate))/np.sqrt(np.mean(x*x)))
  assert abs(ratio-1)<.005,ratio
  metrics["channel_tools_output_ratio"]=ratio
  e.command("set Makoto_ch7_mute true")
  x,_=record("ssg-muted"); assert np.max(abs(x[-1024:])) <= 1
  e.command("set Makoto_ch7_mute false; set Makoto_ch1_mute true")
  x,_=record("other-muted"); assert np.sqrt(np.mean(x*x)) > 5
  e.command("set Makoto_ch1_mute false")
  write(8,0)
  # FM channel 1, left only.
  write(0x29,0x80);write(0xb0,7);write(0xb4,0x80)
  for off in (0,4,8,12):
   for base,value in ((0x30,1),(0x40,40),(0x50,31),(0x60,0),(0x70,0),(0x80,15)):write(base+off,value)
  write(0xa4,0x22);write(0xa0,0x69);write(0x28,0xf0)
  x,_=record("fm-left"); assert np.max(abs(x[:,0])) > 5 and np.max(abs(x[-1024:,1])) <= 1
  e.command("set Makoto_ch1_mute true")
  x,_=record("fm-muted"); assert np.max(abs(x[-1024:])) <= 1
  e.command("set Makoto_ch1_mute false; set Makoto_volume 0")
  x,_=record("master-zero"); assert np.max(abs(x[-1024:])) <= 1
  e.command("set Makoto_volume 20")
  write(0x28,0)
  # x1 DRAM transfers at all four 64KB quarters, using actual CPU IN.
  patterns={address:bytes((17*index+i*31)&255 for i in range(8)) for index,address in enumerate((0,0x10000,0x20000,0x3ffe0))}
  write(0x101,0xc0);write(0x104,0xff);write(0x105,0xff);write(0x10c,0xff);write(0x10d,0xff)
  def start(address,mode):
   write(0x100,1);write(0x102,(address>>2)&255);write(0x103,(address>>10)&255);write(0x100,mode)
  for address,data in patterns.items():
   start(address,0x60)
   for value in data:write(0x108,value)
  def read_ram(address):
   start(address,0x20); e.command("debug write ioports 22 8")
   code=bytearray([0xf3,0xdb,0x17,0xdb,0x17])
   for i in range(8): code.extend((0xdb,0x17,0x32,i,0xc3))
   loop=0xc100+len(code);code.extend((0xc3,loop&255,loop>>8))
   pc=e.command("reg PC")
   e.command("debug write_block memory 0xc100 [binary decode hex {"+code.hex()+"}]; reg PC 0xc100")
   step(.001)
   value=e.command("binary encode hex [debug read_block memory 0xc300 8]")
   e.command("reg PC "+pc)
   return bytes.fromhex(value)
  for address,expected in patterns.items(): assert read_ram(address)==expected,(hex(address),read_ram(address),expected)
  write(0x100,1)
  saved=out/"ram-state.oms"
  e.command("store_machine [machine] "+m.tcl_path(saved))
  for address in patterns:
   start(address,0x60)
   for _ in range(8):write(0x108,0)
  e.command("set old [machine]; set new [restore_machine "+m.tcl_path(saved)+"]; delete_machine $old; activate_machine $new")
  for address,expected in patterns.items(): assert read_ram(address)==expected
  import gzip
  unsupported=out/"unsupported-core.oms"
  xml=gzip.decompress(saved.read_bytes())
  assert b"<coreFormat>1</coreFormat>" in xml
  unsupported.write_bytes(gzip.compress(xml.replace(b"<coreFormat>1</coreFormat>",b"<coreFormat>2</coreFormat>")))
  error=e.command("catch {restore_machine "+m.tcl_path(unsupported)+"} reason; set reason")
  assert "Unsupported Makoto core state format" in error,error
  metrics["state_format"]="RAM restored; unknown core format explicitly rejected"
  if a.legacy_state:
   e.command("set old [machine]; set new [restore_machine "+m.tcl_path(a.legacy_state)+"]; delete_machine $old; activate_machine $new")
   step(.01)
   assert e.command("debug read {Makoto registers} 8") == "15"
   metrics["legacy_state"]="loaded and continued"
  metrics["channels"]="16 voices exposed; SSG/FM isolation and stereo pan passed"
  metrics["ram"]="CPU readback in each 64KB quarter of 256KB x1-mode RAM passed"
 finally:e.close()
if a.baseline:
 # Compare the fundamental, excluding square-wave harmonics and filter startup.
 def fundamental(x,rate,f):
  t=np.arange(len(x))/rate
  return abs(np.sum(x[:,0]*np.hanning(len(x))*np.exp(-2j*np.pi*f*t)))
 for label,freq in (("high",125000/9),("low",1000)):
  got=fundamental(results["current"][label],results["current"]["rate"],freq)/fundamental(results["baseline"][label],results["baseline"]["rate"],freq)
  alpha=-math.expm1(-1e-6/(100000*47e-12))
  expected=alpha/abs(1-(1-alpha)*np.exp(-2j*np.pi*freq/1e6))
  assert abs(got-expected)<.015,(label,got,expected)
  metrics[label+"_filter"]={"measured":got,"expected":float(expected)}
(out/"results.json").write_text(json.dumps(metrics,indent=2))
print(out);print(json.dumps(metrics,indent=2))
