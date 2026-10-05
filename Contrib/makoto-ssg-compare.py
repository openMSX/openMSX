"""Compare native SSG channel waveforms after factoring out the volume tables."""
import argparse
import importlib.util
import json
import tempfile
import time
import wave
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('makotest', ROOT / 'Contrib/makoto-test.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
p = argparse.ArgumentParser(__doc__)
p.add_argument('--baseline', type=Path, required=True)
p.add_argument('--candidate', type=Path, required=True)
p.add_argument('--firmware-dir', type=Path, required=True)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix='makoto-ssg-', dir=ROOT / 'derived'))
rom = out / 'test.rom'; rom.write_bytes(m.image(0x43))
cases = [(f'shape-{shape}', [(7,63),(8,16),(11,113),(12,0),(13,shape)], None) for shape in range(16)]
cases += [
    ('tone', [(0,137),(1,0),(7,62),(8,15)], None),
    ('noise', [(6,7),(7,55),(8,15)], None),
    ('noise-zero', [(6,0),(7,55),(8,15)], None),
    ('noise-one', [(6,1),(7,55),(8,15)], None),
    ('tone-noise', [(0,137),(1,0),(6,7),(7,54),(8,15)], None),
    ('noise-after-silence', [(6,7),(7,55),(8,0)], None),
    ('envelope-after-silence', [(7,63),(8,0),(11,113),(12,0),(13,8)], None),
    ('period-zero-saw', [(7,63),(8,16),(11,0),(12,0),(13,8)], None),
    ('period-zero-triangle', [(7,63),(8,16),(11,0),(12,0),(13,10)], None),
    ('rewrite-same-shape', [(7,63),(8,16),(11,113),(12,0),(13,8)], 8),
    ('rewrite-new-shape', [(7,63),(8,16),(11,113),(12,0),(13,8)], 10),
]
data = {}

def step(e, seconds):
    e.command(f'after time {seconds} {{set pause on}}; set pause off')
    deadline=time.monotonic()+30
    while e.command('set pause')!='true':
        assert time.monotonic()<deadline

for label,exe in (('baseline',a.baseline),('candidate',a.candidate)):
    run=out/label;run.mkdir()
    e=m.Emulator(exe,run,a.firmware_dir,rom,'ASCII16')
    data[label]={}
    try:
        e.command('set pause on; set mute off; set volume 50')
        e.command('proc ssg_test_write {r v} {debug write ioports 20 $r; debug write ioports 21 $v}')
        for name,writes,rewrite in cases:
            e.command('set testext [ext Makoto]')
            for reg,value in writes:
                e.command(f'ssg_test_write {reg} {value}')
            if name.endswith('-after-silence'):
                step(e,.08)
                e.command(f'ssg_test_write 8 {15 if name.startswith("noise") else 16}')
            path=run/(name+'.wav')
            e.command('set {Makoto SSG_ch1_record} '+m.tcl_path(path))
            if rewrite is not None:
                e.command(f'after time .012344 {{ssg_test_write 13 {rewrite}}}')
            step(e,.12)
            e.command('set {Makoto SSG_ch1_record} {}; remove_extension $testext')
            with wave.open(str(path)) as w:
                assert w.getframerate()==250000 and w.getnchannels()==1
                x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2')
            levels,rank=np.unique(x,return_inverse=True)
            data[label][name]=(levels,rank)
    finally:e.close()

# AY8910 uses Galois LFSR state; YMFM uses Fibonacci state. Both reset their
# differently encoded state to 1, so the resulting noise starts at different
# phases of the same 131071-step sequence. Check the actual native output
# against that complete sequence rather than accepting statistical similarity.
state=1;cycle=[]
for _ in range(131071):
    cycle.append(state & 1)
    state=(state>>1)^(((state^(state>>3))&1)<<16)
assert state==1
noise_positions={}
for label in data:
    noise_positions[label]={}
    for name,ticks in (('noise',14),('noise-zero',2),('noise-one',2),('noise-after-silence',14)):
        stream=np.tile(np.repeat(np.array(cycle,dtype=np.uint8),ticks),2)
        values=data[label][name][1].astype(np.uint8)[128:-128]
        pos=stream.tobytes().find(values.tobytes())
        assert pos>=0,(label,name,'Not the expected complete noise sequence')
        noise_positions[label][name]=pos
    advance=noise_positions[label]['noise-after-silence']-noise_positions[label]['noise']
    assert abs(advance-20000)<=2,(label,'Silent noise failed to advance',advance)

    # Tone and noise retain independent phases; a single shift cannot align
    # their product between implementations. Verify that product directly.
    values=data[label]['tone-noise'][1].astype(np.uint8)[128:-128]
    stream=np.tile(np.repeat(np.array(cycle,dtype=np.uint8),14),2)
    pos=noise_positions[label]['noise']
    head=min(2048,len(values))
    tones=(((np.arange(head)[None,:]+np.arange(274)[:,None])//137)&1).astype(np.uint8)
    found=False
    for delta in range(-128,129):
        if pos+delta<0:
            continue
        partial=stream[pos+delta:pos+delta+head]
        phases=np.flatnonzero(np.all((tones&partial)==values[:head],axis=1))
        for phase in phases:
            tone=(((np.arange(len(values))+int(phase))//137)&1).astype(np.uint8)
            expected=tone&stream[pos+delta:pos+delta+len(values)]
            found=found or np.array_equal(expected,values)
    assert found,(label,'Tone/noise product differs from expected generators')

report=[]
for name,_,rewrite in cases:
    levels_x,x=data['baseline'][name];levels_y,y=data['candidate'][name]
    assert len(levels_x)==len(levels_y),(name,levels_x.tolist(),levels_y.tolist())
    # Native recordings can begin on adjacent sample edges. Search a bounded
    # phase offset; do not rescale time or discard interior differences.
    margin=128
    length=min(len(x),len(y))-2*margin
    counts=[int(np.count_nonzero(x[margin:margin+length] != y[margin+shift:margin+shift+length]))
            for shift in range(-margin,margin+1)]
    best=min(counts);shift=counts.index(best)-margin
    mismatches=np.flatnonzero(x[margin:margin+length] != y[margin+shift:margin+shift+length])+margin
    if name.startswith('noise') or name=='tone-noise':
        result='Exact expected LFSR sequence/product; different reset phase between implementations'
    elif rewrite is not None:
        # One native sample differs at the timed register write: the engines
        # use opposite clock/output edge conventions. Preserve this finding.
        assert best<=1 and all(abs(int(i)-3086)<=2 for i in mismatches),(name,mismatches.tolist())
        result='Same shape sequence; at most one 4-us sample differs at the rewrite edge'
    else:
        assert best==0,(name,best)
        result='Exact level-index sequence after phase alignment'
    row={'case':name,'levels':len(levels_x),'compared_samples':length,
         'best_shift_samples':shift,'level_index_mismatches':best,
         'mismatch_indices':mismatches.tolist() if best<=1 else [],'result':result,
         'baseline_levels':levels_x.tolist(),'candidate_levels':levels_y.tolist()}
    report.append(row)
    print(name, 'mismatches',best,'of',length,'shift',shift,flush=True)
(out/'results.json').write_text(json.dumps({'cases':report,'noise_positions':noise_positions},indent=2))
print(out)
