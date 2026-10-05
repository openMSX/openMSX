"""Compare compiled old/new makoto-owned-core-test renderers and negative controls."""
import argparse
from array import array
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(__doc__)
p.add_argument('--reference', type=Path, required=True)
p.add_argument('--candidate', type=Path, required=True)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix='makoto-engines-', dir=ROOT / 'derived'))

def render(exe, label, block, control=None):
    target = out / (label + '.bin')
    args = [str(exe.resolve()), str(target), str(block)]
    if control:
        args.append(control)
    result = subprocess.run(args, capture_output=True, text=True, check=True,
                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    print(result.stdout.strip(), flush=True)
    return target.read_bytes()

reference = render(a.reference, 'reference', 127)
report = {'reference_sha256': hashlib.sha256(a.reference.read_bytes()).hexdigest(),
          'candidate_sha256': hashlib.sha256(a.candidate.read_bytes()).hexdigest(),
          'frames_per_run': 246940, 'voices': 13, 'blocks': [], 'negative_controls': {}}
for block in (1, 7, 127, 4096):
    candidate = render(a.candidate, f'candidate-{block}', block)
    if candidate != reference:
        x, y = array('i'), array('i')
        x.frombytes(reference); y.frombytes(candidate)
        differences = [(i, v, w) for i, (v, w) in enumerate(zip(x, y)) if v != w]
        raise AssertionError((block, len(reference), len(candidate), differences[:12], len(differences)))
    report['blocks'].append({'block_size': block, 'sha256': hashlib.sha256(candidate).hexdigest(), 'mismatches': 0})
for control in ('fm', 'rhythm', 'adpcm'):
    data = render(a.candidate, f'negative-{control}', 127, control)
    assert data != reference, f'Negative control {control} was not detected'
    x, y = array('i'), array('i')
    x.frombytes(reference); y.frombytes(data)
    report['negative_controls'][control] = sum(v != w for v, w in zip(x, y))
(out / 'results.json').write_text(json.dumps(report, indent=2))
print(out)
print(json.dumps(report, indent=2))
