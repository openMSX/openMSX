"""Check native engine blobs, legacy conversion and failed sound registration."""
import argparse, base64, gzip, importlib.util, json, tempfile, zlib, re, shutil
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("makotest",ROOT/"Contrib/makoto-test.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=argparse.ArgumentParser();p.add_argument("--openmsx",type=Path,required=True);p.add_argument("--firmware-dir",type=Path,required=True);p.add_argument("--legacy-state",type=Path,action="append",default=[]);a=p.parse_args()
out=Path(tempfile.mkdtemp(prefix="makoto-state-layout-",dir=ROOT/"derived"));run=out/"run";run.mkdir();rom=out/"test.rom";rom.write_bytes(m.image(0x37))
e=m.Emulator(a.openmsx,run,a.firmware_dir,rom,"ASCII16");passed=[]
def snapshot(name):
 path=out/(name+".oms");e.command("store_machine [machine] "+m.tcl_path(path));return path
def sound(path):return ET.fromstring(gzip.decompress(path.read_bytes())).find('.//device[@type="Makoto"]/sound')
def blob(node):
 if node.get("encoding")=="hex":return bytes.fromhex(node.text)
 data=base64.b64decode(node.text)
 return zlib.decompress(data) if node.get("encoding")=="gz-base64" else data
def restore(path):e.command("set old [machine]; set restored [restore_machine "+m.tcl_path(path)+"]; activate_machine $restored; delete_machine $old")
try:
 e.command("set pause on")
 for filename in ('fs-a1gt_firmware.rom','fs-a1gt_kanjifont.rom'):
  if (a.firmware_dir/filename).is_file():
   shutil.copy2(a.firmware_dir/filename,run/'home/share/systemroms'/filename)
 extensions=run/"home/share/extensions";extensions.mkdir(exist_ok=True)
 xml=(ROOT/"share/extensions/Makoto.xml").read_text()
 for label,addition in [("BadFm",'<mode>invalid</mode>'),("BadSsg",'<balance channel="4">0</balance>')]:
  (extensions/(label+".xml")).write_text(xml.replace('</sound>',addition+'</sound>'))
  assert e.command('catch {ext '+label+'}')=='1'
  assert e.command('info exists Makoto_volume')=='0'
  assert e.command('info exists {Makoto SSG_volume}')=='0'
  e.command('set valid [ext Makoto]; remove_extension $valid')
 passed.append('Failed FM and SSG registration leaves no device settings; subsequent insertion/removal succeeds')
 e.command('ext Makoto')
 current=snapshot('current');node=sound(current);chip=node.find('chip')
 assert chip is not None and node.find('core') is None
 assert [x.tag for x in chip]==['address','irqEnable','flagControl','fm','ssg','adpcmA','adpcmB']
 sizes={name:len(blob(chip.find(name))) for name in ('fm','ssg','adpcmA','adpcmB')}
 for name in sizes:
  rawxml=gzip.decompress(current.read_bytes());tag=name.encode()
  replacement=b'<'+tag+b' encoding="hex">'+blob(chip.find(name))[:-1].hex().encode()+b'</'+tag+b'>'
  changed,count=re.subn(b'<'+tag+rb'\b[^>]*>.*?</'+tag+b'>',replacement,rawxml,count=1,flags=re.S)
  assert count==1
  bad=out/(name+'-truncated.oms');bad.write_bytes(gzip.compress(changed))
  result=e.command('catch {restore_machine '+m.tcl_path(bad)+'} message; set message')
  assert 'Length' in result or 'blob' in result,result
 passed.append('Every engine blob rejects a truncated payload')
 for i,path in enumerate(a.legacy_state):
  old=sound(path);raw=bytes(int(n.text) for n in old.find('core'))
  restore(path);new=sound(snapshot('converted-'+str(i)));chip=new.find('chip')
  assert int(chip.findtext('address'))==int.from_bytes(raw[:2],'little')
  assert int(chip.findtext('irqEnable'))==raw[2] and int(chip.findtext('flagControl'))==raw[3]
  offset=12
  for name in ('fm','ssg','adpcmA','adpcmB'):
   if name=='adpcmA':offset+=16
   expected=blob(chip.find(name));actual=raw[offset:offset+len(expected)]
   if name=='adpcmB' and len(actual)==len(expected)-1:actual+=bytes(1)
   assert actual==expected,(path,name)
   offset+=len(expected)
  for name in ('busyEnd','sampleRAM','irq','timerA','timerB'):
   assert ET.tostring(old.find(name))==ET.tostring(new.find(name)),name
  e.command('after time .02 {set pause on}; set pause off')
  while e.command('set pause')!='true':pass
  passed.append('Legacy sound version '+old.get('version','1')+' converts exact control/engine/RAM/timer values and resumes')
finally:e.close()
report={'passed':passed,'engine_bytes':sizes};(out/'results.json').write_text(json.dumps(report,indent=2));print(out);print(json.dumps(report,indent=2))
