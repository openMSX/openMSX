"""Native-stream pitch, prescaler, state and recording checks. Uses synthetic tones."""
import argparse,gzip,importlib.util,json,tempfile,time,wave,xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('makotest',ROOT/'Contrib/makoto-test.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=argparse.ArgumentParser(__doc__)
p.add_argument('--openmsx',type=Path,required=True)
p.add_argument('--firmware-dir',type=Path,required=True)
a=p.parse_args()
out=Path(tempfile.mkdtemp(prefix='makoto-native-stream-',dir=ROOT/'derived'))
rom=out/'test.rom';rom.write_bytes(m.image(0x41))
e=m.Emulator(a.openmsx,out,a.firmware_dir,rom,'ASCII16')
report={}
def step(seconds):
 e.command(f'after time {seconds} {{set pause on}}; set pause off')
 deadline=time.monotonic()+30
 while e.command('set pause')!='true':assert time.monotonic()<deadline

def write(reg,value):
 port=22 if reg>=256 else 20
 e.command(f'debug write ioports {port} {reg&255}; debug write ioports {port+1} {value}')

def snapshot(name):
 path=out/(name+'.oms');e.command('store_machine [machine] '+m.tcl_path(path))
 return path

def sound(path):
 return ET.tostring(ET.fromstring(gzip.decompress(path.read_bytes())).find('.//device[@type="Makoto"]/sound'))

def record(label,seconds=1):
 path=out/(label+'.wav');e.command('soundlog start '+m.tcl_path(path));step(seconds);e.command('soundlog stop')
 with wave.open(str(path)) as f:
  rate=f.getframerate();data=np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').reshape(-1,2).astype(float)
 assert np.max(abs(data))<32000
 return data,rate

def pitch(data,rate):
 x=data[4000:-4000,0];s=np.abs(np.fft.rfft((x-np.mean(x))*np.hanning(len(x))))
 k=int(np.argmax(s[1:])+1);v=np.log(s[k-1:k+2]+1e-20)
 offset=.5*(v[0]-v[2])/(v[0]-2*v[1]+v[2])
 return (k+offset)*rate/len(x)

try:
 e.command('set pause on; ext Makoto; set mute off; set volume 50; set Makoto_volume 20; set [lindex [info vars ?akoto_psg_volume] 0] 100')
 native=e.command('info exists {Makoto SSG_volume}')=='1'
 if native:e.command('set {Makoto SSG_volume} 20')
 write(0,128);write(1,0);write(7,0x3e);write(8,15)
 for prescale,address,expected in [(6,0x2d,976.5625),(3,0x2e,1953.125),(2,0x2f,3906.25),(6,0x2d,976.5625)]:
  label=str(prescale)+'-'+str(len(report))
  e.command(f'debug write ioports 20 {address}')
  data,rate=record(label)
  freq=pitch(data,rate);assert abs(freq-expected)<2,(prescale,freq,expected)
  before=snapshot(label+'-before');step(.1234);expected_state=sound(snapshot(label+'-expected'))
  e.command('set old [machine]; set new [restore_machine '+m.tcl_path(before)+']; delete_machine $old; activate_machine $new')
  step(.1234);assert sound(snapshot(label+'-actual'))==expected_state,'State diverged at prescaler '+str(prescale)
  report[label]={'prescaler':prescale,'frequency_hz':freq,'expected_hz':expected,'peak':float(np.max(abs(data))),'save_restore':'exact chip/clock continuation'}
 # Rewind across a prescaler change with an active tone.
 e.command('reverse start');step(.2);saved_time=e.command('machine_info time');before=sound(snapshot('rewind-before'))
 step(.02) # Keep the recorded debugger write strictly after the rewind target.
 e.command('debug write ioports 20 46');step(.3)
 e.command('reverse goto '+saved_time)
 assert sound(snapshot('rewind-after'))==before,'Prescaler rewind diverged'
 e.command('reverse stop')
 report['rewind']='exact chip/clock restore across live prescaler change'
finally:e.close()
(out/'results.json').write_text(json.dumps(report,indent=2))
print(out);print(json.dumps(report,indent=2))
