"""Measure no-device, idle, real playback and channel-tool host CPU costs.

Uses disposable profiles and dummy audio. Supply your own test ROM and matching
37-track manifest; no copyrighted music assets are distributed with this tool.
"""
import argparse, importlib.util, json, statistics, tempfile, time, hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,file):
 spec=importlib.util.spec_from_file_location(name,ROOT/'Contrib'/file)
 obj=importlib.util.module_from_spec(spec);spec.loader.exec_module(obj);return obj
m=module('makoto_test','makoto-test.py');b=module('makoto_bench','makoto-benchmark.py')
def step(e,seconds):
 e.command(f'after time {seconds} {{set pause on}}; set pause off')
 deadline=time.monotonic()+180
 while e.command('set pause')!='true':
  assert time.monotonic()<deadline,'Emulation timed out'
def main():
 p=argparse.ArgumentParser(__doc__)
 p.add_argument('--build',action='append',required=True,help='label=executable')
 p.add_argument('--firmware-dir',type=Path,required=True)
 p.add_argument('--rom',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True)
 p.add_argument('--seconds',type=float,default=20);p.add_argument('--repeats',type=int,default=3)
 a=p.parse_args();builds=[x.split('=',1) for x in a.build]
 songs=json.loads(a.manifest.read_text())['songs']
 track=next((i for i,s in enumerate(songs) if 'BUSTLING' in s.get('display_name',s['name']).upper()),None)
 assert track is not None,'Bustling Town not found in supplied manifest'
 out=Path(tempfile.mkdtemp(prefix='makoto-workload-',dir=ROOT/'derived'))
 stub=out/'idle.rom';stub.write_bytes(m.image(0x40))
 report={'seconds':a.seconds,'rom_sha256':hashlib.sha256(a.rom.read_bytes()).hexdigest(),'track':track+1,'runs':[]}
 with (out/'results.json').open('x',encoding='utf-8') as stream:
  try:
   for repeat in range(a.repeats):
    order=builds if repeat%2==0 else list(reversed(builds))
    for label,exe in order:
     for case in ('no-makoto','silent-basic','music','channel-tools'):
      run=out/f'{label}-{case}-{repeat}';run.mkdir()
      active=case in ('music','channel-tools')
      e=m.Emulator(Path(exe),run,a.firmware_dir,a.rom if active else stub,'ASCII16',machine='Panasonic_FS-A1GT')
      try:
       e.command('set pause on; set mute off; set volume 50')
       if not active:e.command('carta eject')
       if case!='no-makoto':
        e.command('ext Makoto; set Makoto_volume 20; if {[llength [info vars ?akoto_psg_volume]]} {set {Makoto SSG_volume} 20; set [lindex [info vars ?akoto_psg_volume] 0] 50} else {set {Makoto SSG_volume} 10}')
       e.command('reset');step(e,15)
       if active:
        e.command(f"debug write memory 0xD200 {track}; debug write memory 0xD201 {songs[track]['default_bank']-1}; keymatrixdown 8 1")
        step(e,.1);e.command('keymatrixup 8 1');step(e,1)
        assert e.command('debug read memory 0xD202')=='1','Music did not start'
        assert e.command('debug read memory 0xD203')=='0','Player hardware error'
       if case=='channel-tools':
        e.command('set Makoto_ch1_mute true')
        e.command('set {Makoto SSG_ch3_mute} true' if e.command('info exists {Makoto SSG_volume}')=='1' else 'set Makoto_ch9_mute true')
       cpu_mode=e.command('get_active_cpu');start=b.cpu_time(e.process);wall=time.perf_counter()
       step(e,a.seconds);elapsed=time.perf_counter()-wall;end=b.cpu_time(e.process)
       row=dict(build=label,case=case,repeat=repeat,cpu_mode=cpu_mode,cpu_seconds=None if start is None else end-start,wall_seconds=elapsed)
       report['runs'].append(row);print(json.dumps(row),flush=True)
      finally:e.close()
   summary={}
   for label,_ in builds:
    values={case:statistics.median(r['cpu_seconds'] if r['cpu_seconds'] is not None else r['wall_seconds'] for r in report['runs'] if r['build']==label and r['case']==case) for case in ('no-makoto','silent-basic','music','channel-tools')}
    summary[label]={'median_seconds':values,'relative_to_no_makoto':{c:v/values['no-makoto'] for c,v in values.items()}}
   report['summary']=summary
  finally:json.dump(report,stream,indent=2)
 print(out);print(json.dumps(report.get('summary',{}),indent=2))
if __name__=='__main__':main()
