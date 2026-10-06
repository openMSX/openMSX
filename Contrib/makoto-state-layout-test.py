"""Native YM2608 state layout, malformed collections and registration cleanup."""
import argparse, copy, gzip, importlib.util, json, tempfile
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("makotest",ROOT/"Contrib/makoto-test.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=argparse.ArgumentParser(__doc__)
p.add_argument("--openmsx",type=Path,required=True)
p.add_argument("--firmware-dir",type=Path,required=True)
a=p.parse_args()
out=Path(tempfile.mkdtemp(prefix="makoto-state-layout-",dir=ROOT/"derived"))
run=out/"run";run.mkdir();rom=out/"test.rom";rom.write_bytes(m.image(0x37))
e=m.Emulator(a.openmsx,run,a.firmware_dir,rom,"ASCII16");passed=[]
try:
 e.command("set pause on")
 extensions=run/"home/share/extensions";extensions.mkdir(exist_ok=True)
 xml=(ROOT/"share/extensions/Makoto.xml").read_text()
 for label,addition in [("BadFm",'<mode>invalid</mode>'),("BadSsg",'<balance channel="4">0</balance>')]:
  (extensions/(label+".xml")).write_text(xml.replace('</sound>',addition+'</sound>'))
  assert e.command('catch {ext '+label+'}')=='1'
  assert e.command('info exists Makoto_volume')=='0'
  assert e.command('info exists {Makoto SSG_volume}')=='0'
  e.command('set valid [ext Makoto]; remove_extension $valid')
 passed.append('Failed FM and SSG registration cleans up settings; reinsertion succeeds')
 e.command('ext Makoto')
 current=out/'current.oms'
 e.command('store_machine [machine] '+m.tcl_path(current))
 root=ET.fromstring(gzip.decompress(current.read_bytes()))
 node=m.sound_node(root)
 assert node.tag=='ym2608' and node.find('core') is None and node.find('chip') is None
 for tag in ('timers','fm','adpcmA','adpcmB','ssg'):
  assert node.find(tag) is not None,tag
 assert node.find('adpcmB/sampleRAM') is not None
 assert node.find('fm/irq') is not None
 sizes={'fm/operators':24,'fm/channels':6,'adpcmA/channels':6}
 for path,size in sizes.items():
  assert len(node.find(path))==size,(path,len(node.find(path)))
  broken=copy.deepcopy(root)
  collection=m.sound_node(broken).find(path)
  collection.remove(collection[-1])
  bad=out/(path.replace('/','-')+'-truncated.oms')
  bad.write_bytes(gzip.compress(ET.tostring(broken,encoding='utf-8',xml_declaration=True)))
  failed=e.command('catch {restore_machine '+m.tcl_path(bad)+'} message')
  assert failed=='1',(path,e.command('set message'))
  passed.append(path+': native serializer rejects truncated fixed-size collection')
 passed.append('Native control/engine fields are present; packed core blobs are absent')
finally:e.close()
report={'passed':passed,'collection_sizes':sizes}
(out/'results.json').write_text(json.dumps(report,indent=2))
print(out);print(json.dumps(report,indent=2))
