"""Makoto channel isolation, RAM and legacy-state checks."""
import argparse, array, importlib.util, json, math, tempfile, time, wave
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("makotest", ROOT / "Contrib/makoto-test.py")
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
p = argparse.ArgumentParser()
p.add_argument("--openmsx", type=Path, required=True)
p.add_argument("--firmware-dir", type=Path, required=True)
p.add_argument("--legacy-state", type=Path, action="append", default=[])
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
  # Peek ADPCM transfer data without consuming dummy reads or RAM bytes.
  start(0x10000,0x20); e.command("debug write ioports 22 8")
  assert e.command("debug read ioports 23") == "0"
  assert e.command("debug read ioports 23") == "0"
  pc=e.command("reg PC")
  e.command("debug write_block memory 0xc100 [binary decode hex {f3db17db17c305c1}]; reg PC 0xc100")
  step(.001)
  peek_state=out/"before-data-peeks.oms"
  e.command("store_machine [machine] "+m.tcl_path(peek_state))
  for _ in range(30):
   assert int(e.command("debug read ioports 23")) == patterns[0x10000][0]
   e.command("debug read ioports 22")
  after_peeks=out/"after-data-peeks.oms"
  e.command("store_machine [machine] "+m.tcl_path(after_peeks))
  import gzip, xml.etree.ElementTree as ET
  def chip_xml(path):
   return ET.tostring(ET.fromstring(gzip.decompress(path.read_bytes())).find('.//device[@type="Makoto"]/sound'))
  assert chip_xml(peek_state)==chip_xml(after_peeks)
  e.command("reg PC "+pc)
  metrics["data_peeks"]="ADPCM dummy reads, next RAM byte, IRQ and complete saved chip state preserved"
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
  import re
  assert b"<coreFormat>" not in xml
  changed,n = re.subn(rb'(<sound\b[^>]*\bversion=")[0-9]+(")', rb'\g<1>999\2', xml)
  assert n == 1
  unsupported.write_bytes(gzip.compress(changed))
  error=e.command("catch {restore_machine "+m.tcl_path(unsupported)+"} reason; set reason")
  assert "version" in error.lower(),error
  metrics["state_format"]="RAM restored; future sound-state version rejected by the native serializer"
  for legacy_path in a.legacy_state:
   import xml.etree.ElementTree as ET
   def sound_state(path):
    root=ET.fromstring(gzip.decompress(path.read_bytes()))
    return root,root.find('.//device[@type="Makoto"]/sound')
   legacy_root,legacy_sound=sound_state(legacy_path)
   assert legacy_sound.get("version","1") in ("1","2","3","4")
   deadlines=[int(x.text) for x in legacy_sound.findall("deadlines/item/time")]
   if legacy_sound.get("version") in ("3","4"):
    deadlines=[]
    for name in ("timerA","timerB"):
     node=legacy_sound.find(name+"/Schedulable/syncPoints/item/time/time")
     deadlines.append(int(node.text) if node is not None else 2**64-1)
   assert len(deadlines)==2
   def payload(node):
    return tuple(text.strip() for text in node.itertext() if text.strip())
   variants=[("released-v"+legacy_sound.get("version","1"),legacy_path)]
   if legacy_sound.get("version")=="2":
    # Version 1 used this same core/deadline layout without the v2 caches.
    legacy_sound.set("version","1")
    for name in ("coreFormat","channelOutput","filterState"):
     legacy_sound.remove(legacy_sound.find(name))
    v1=out/"legacy-v1-layout.oms"
    v1_xml=gzip.decompress(legacy_path.read_bytes())
    v1_xml,n=re.subn(rb'(<sound\b[^>]*\bversion=")2(")',rb'\g<1>1\2',v1_xml)
    assert n==1
    for name in (b"coreFormat",b"channelOutput",b"filterState"):
     v1_xml,n=re.subn(b"<"+name+b">.*?</"+name+b">",b"",v1_xml,flags=re.S)
     assert n==1
    v1.write_bytes(gzip.compress(v1_xml))
    variants.append(("v1-layout",v1))
   for label,path in variants:
    e.command("set old [machine]; set new [restore_machine "+m.tcl_path(path)+"]; delete_machine $old; activate_machine $new")
    migrated=out/("migrated-"+label+".oms")
    e.command("store_machine [machine] "+m.tcl_path(migrated))
    _,current_sound=sound_state(migrated)
    assert current_sound.find("regs") is None and current_sound.find("latch") is None
    for index,name in enumerate(("timerA","timerB")):
     times=[int(x.text) for x in current_sound.findall(name+"/Schedulable/syncPoints/item/time/time")]
     expected=[] if deadlines[index]==2**64-1 else [deadlines[index]]
     assert times==expected,(name,times,expected)
    for name in ("core","busyEnd","sampleRAM","irq","sampleClock"):
     assert payload(current_sound.find(name))==payload(legacy_sound.find(name)),(label,name)
    step(.01)
    assert e.command("debug read {Makoto registers} 8") == "15"
   # Old explicit format tags must still be validated, not ignored.
   if len(variants)==2:
    xml=gzip.decompress(legacy_path.read_bytes())
    unsupported=out/"unsupported-legacy-core.oms"
    unsupported.write_bytes(gzip.compress(xml.replace(b"<coreFormat>1</coreFormat>",b"<coreFormat>2</coreFormat>")))
    error=e.command("catch {restore_machine "+m.tcl_path(unsupported)+"} reason; set reason")
    assert "Unsupported Makoto core state format" in error,error
   metrics["legacy_state_"+legacy_path.parent.name]="core, deadlines, BUSY, RAM and clock preserved; resumed; no duplicate registers in new save"
  metrics["channels"]="16 voices exposed; SSG/FM isolation and stereo pan passed"
  metrics["ram"]="CPU readback in each 64KB quarter of 256KB x1-mode RAM passed"
 finally:e.close()
if a.baseline:
 # Compare removal against a filtered baseline, excluding harmonics/startup.
 def fundamental(x,rate,f):
  t=np.arange(len(x))/rate
  return abs(np.sum(x[:,0]*np.hanning(len(x))*np.exp(-2j*np.pi*f*t)))
 for label,freq in (("high",125000/9),("low",1000)):
  got=fundamental(results["current"][label],results["current"]["rate"],freq)/fundamental(results["baseline"][label],results["baseline"]["rate"],freq)
  alpha=-math.expm1(-1e-6/(100000*47e-12))
  expected=abs(1-(1-alpha)*np.exp(-2j*np.pi*freq/1e6))/alpha
  assert abs(got-expected)<.015,(label,got,expected)
  metrics[label+"_filter"]={"measured":got,"expected":float(expected)}
(out/"results.json").write_text(json.dumps(metrics,indent=2))
print(out);print(json.dumps(metrics,indent=2))
