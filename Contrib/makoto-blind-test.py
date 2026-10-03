"""Make a private, offline blind ABX page from a completed Makoto filter capture.

The page contains anonymous audio only. The filter identities are stored in a
separate key file outside the page, and are not needed for scoring X versus A/B.
"""
import argparse
import base64
import io
import json
import secrets
import tempfile
import wave
from pathlib import Path

import numpy as np

ROOT=Path(__file__).resolve().parents[1]
PAGE=r'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Makoto · Blind listening test</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#10141c;color:#eef3fc}*{box-sizing:border-box}body{margin:0;padding:36px 20px}main{max-width:760px;margin:auto}small,.muted{color:#a9b8cd}h1{font-size:30px;margin:12px 0}p{line-height:1.6}section{background:#1a2331;border:1px solid #354359;border-radius:18px;padding:25px;margin:24px 0}button{font:inherit;border:1px solid #526480;background:#25344a;color:inherit;border-radius:10px;padding:13px 20px;cursor:pointer}button:hover{background:#344c70}button:disabled{opacity:.45;cursor:default}.players{display:flex;gap:12px;margin:24px 0}.players button{flex:1;font-size:22px}.players button.active{background:#276c69;border-color:#71d2c8}.bar{display:flex;gap:12px;align-items:center;flex-wrap:wrap}progress{width:100%;accent-color:#71d2c8}input[type=range]{flex:1;accent-color:#71d2c8}#question{font-size:21px;font-weight:650}.answer{background:#335681;min-width:135px}#result{white-space:pre-line}a{color:#9ccaff}.hidden{display:none}#position{font-variant-numeric:tabular-nums}
</style><main><small>MAKOTO / LISTENING EXPERIMENT</small><h1>Can you hear the difference?</h1>
<p>A and B are two recordings of the same passage. X is an exact copy of one of them. Decide whether X matches A or B. Neither the filter labels nor your score will be shown during the test.</p>
<p class="muted">Use the same headphones or speakers throughout. Listen as often as you like. Switching keeps the same playback position; Restart returns to the beginning. Avoid changing volume within a trial. If you cannot tell, make your best guess.</p>
<section id="test"><div class="bar"><strong id="trial"></strong><span id="clip" class="muted"></span></div><progress id="progress" value="0" max="12"></progress>
<div class="players"><button id="A" onclick="play('A')">Play A</button><button id="B" onclick="play('B')">Play B</button><button id="X" onclick="play('X')">Play X</button></div>
<div class="bar"><button onclick="stop()">Pause</button><button onclick="restart()">Restart</button><span id="position">0:00</span></div>
<p class="bar"><label for="volume">Volume</label><input id="volume" type="range" min="0" max="100" value="35" oninput="volumeChanged()"><span id="volumeText">35%</span></p>
<p id="status" class="muted">Press Play A, B or X to begin.</p><hr style="border:0;border-top:1px solid #354359;margin:25px 0"><p id="question">Does X match A or B?</p>
<div class="bar"><button id="answerA" class="answer" onclick="answer('A')" disabled>X matches A</button><button id="answerB" class="answer" onclick="answer('B')" disabled>X matches B</button></div></section>
<section id="finished" class="hidden"><h2>Listening complete</h2><p id="result"></p><button onclick="download()">Save results</button><p class="muted">Send the score or downloaded result file back. The filter identities remain outside this page so you can discuss what you heard before they are revealed.</p></section>
<p class="muted">12 trials, four for each passage. No per-trial feedback. This page works offline; it sends no data anywhere. Both versions received the same gain within each passage, with no time adjustment or separate loudness normalization.</p></main>
<script>
const clips=__CLIPS__, sessionId='__SESSION__', storageKey='makoto-abx-'+sessionId;
function random(n){let a=new Uint32Array(1),v,limit=Math.floor(4294967296/n)*n;do{crypto.getRandomValues(a);v=a[0]}while(v>=limit);return v%n}
function shuffle(a){for(let i=a.length-1;i>0;i--){const j=random(i+1);[a[i],a[j]]=[a[j],a[i]]}return a}
let session;try{session=JSON.parse(localStorage.getItem(storageKey))}catch{}
if(!session){session={id:sessionId,started:new Date().toISOString(),order:shuffle(clips.flatMap((c,i)=>Array.from({length:4},()=>({clip:i,x:random(2)?'A':'B'})))),answers:[]};save()}
function save(){try{localStorage.setItem(storageKey,JSON.stringify(session))}catch{}}
let context,master,source,fade,startedAt=0,offset=0,active='',heard=new Set(),buffers=new Map(),loading=null,loadingKey='',requestId=0;
function trial(){return session.order[session.answers.length]}
async function ready(){if(!context){context=new AudioContext();master=context.createGain();master.connect(context.destination);volumeChanged()}await context.resume();const i=trial().clip;if(!buffers.has(i)){if(loadingKey!==i){loadingKey=i;loading=Promise.all(['a','b'].map(async k=>{const raw=atob(clips[i][k]);const bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));return context.decodeAudioData(bytes.buffer)})).then(b=>buffers.set(i,b))}await loading}}
function position(){return source?(context.currentTime-startedAt)%buffers.get(trial().clip)[0].duration:offset}
function stopSource(){if(source){const now=context.currentTime;fade.gain.cancelScheduledValues(now);fade.gain.setValueAtTime(fade.gain.value,now);fade.gain.linearRampToValueAtTime(0,now+.008);source.stop(now+.009);source=null}}
async function play(which){const request=++requestId;document.getElementById('status').textContent='Loading audio…';try{await ready();if(request!==requestId)return;const t=trial();offset=position();stopSource();const actual=which==='X'?t.x:which;source=context.createBufferSource();source.buffer=buffers.get(t.clip)[actual==='A'?0:1];source.loop=true;fade=context.createGain();fade.gain.setValueAtTime(0,context.currentTime);fade.gain.linearRampToValueAtTime(1,context.currentTime+.008);source.connect(fade).connect(master);startedAt=context.currentTime-offset;source.start(0,offset);active=which;heard.add(which);document.getElementById('status').textContent='Playing '+which+' · switching preserves position';buttons()}catch(e){document.getElementById('status').textContent='Could not start audio: '+e.message}}
function buttons(){for(const k of ['A','B','X'])document.getElementById(k).classList.toggle('active',k===active&&!!source);for(const k of ['answerA','answerB'])document.getElementById(k).disabled=heard.size<3}
function stop(){requestId++;offset=position();stopSource();buttons();document.getElementById('status').textContent='Paused'}
function restart(){const which=active||'A';stop();offset=0;play(which)}
function volumeChanged(){const v=+document.getElementById('volume').value;document.getElementById('volumeText').textContent=v+'%';if(master)master.gain.setTargetAtTime(v/100,context.currentTime,.01)}
function answer(choice){if(heard.size<3)return;const t=trial();stop();session.answers.push({trial:session.answers.length+1,clip:clips[t.clip].name,answer:choice,correct:choice===t.x});save();offset=0;active='';heard.clear();draw()}
function draw(){const n=session.answers.length;if(n===12){document.getElementById('test').classList.add('hidden');document.getElementById('finished').classList.remove('hidden');const correct=session.answers.filter(a=>a.correct).length;let tail=0,comb=1;for(let k=0;k<=12;k++){if(k>=correct)tail+=comb/4096;comb=comb*(12-k)/(k+1)}session.score={correct,total:12,chance_tail_probability:tail};save();document.getElementById('result').textContent=`You matched ${correct} of 12 trials.\n\nA random guess averages 6 of 12. The chance of guessing at least this many correctly is ${(tail*100).toFixed(2)}%.\n\nThis is one short test, not proof that the recordings are identical or different for every listener.`;return}document.getElementById('trial').textContent=`Trial ${n+1} / 12`;document.getElementById('clip').textContent=clips[trial().clip].name;document.getElementById('progress').value=n;document.getElementById('status').textContent='Listen to A, B and X before answering.';buttons()}
function download(){const blob=new Blob([JSON.stringify({id:session.id,started:session.started,answers:session.answers,score:session.score},null,2)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='makoto-blind-results.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}
setInterval(()=>{if(session.answers.length<12){const s=Math.floor(position());document.getElementById('position').textContent=Math.floor(s/60)+':'+String(s%60).padStart(2,'0')}},100);draw();
</script></html>'''

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('capture',type=Path);a=p.parse_args()
    capture=a.capture.resolve(strict=True);results=json.loads((capture/'results.json').read_text())
    assert results['summary'], 'Capture/benchmark must complete first'
    out=Path(tempfile.mkdtemp(prefix='makoto-blind-',dir=ROOT/'derived'))
    clips=[];key={}
    for name,title in [('bustling-town','Bustling Town'),('cymbal-hihat','Cymbal and hi-hat'),('bright-ssg','Bright PSG tone')]:
        pair=[]
        for kind in ('baseline','candidate'):
            with wave.open(str(capture/kind/(name+'.wav'))) as f:
                rate=f.getframerate();data=np.frombuffer(f.readframes(f.getnframes()),'<i2').reshape(-1,2).astype(float)
            # Identical crop and gain; retain level and phase differences.
            pair.append(data[int(rate*.1):int(rate*12.1)])
        assert pair[0].shape==pair[1].shape
        peak=max(float(np.max(abs(q))) for q in pair)
        rms=max(float(np.sqrt(np.mean(q*q))) for q in pair)
        gain=min(32768*.7/max(peak,1),32768*.1/max(rms,1))
        encoded=[]
        for data in pair:
            data=data.copy()
            fade=min(int(rate*.01),len(data)//2)
            data[:fade]*=np.linspace(0,1,fade)[:,None]
            data[-fade:]*=np.linspace(1,0,fade)[:,None]
            stream=io.BytesIO()
            with wave.open(stream,'wb') as f:
                f.setnchannels(2);f.setsampwidth(2);f.setframerate(rate)
                f.writeframes(np.rint(data*gain).astype('<i2').tobytes())
            encoded.append(base64.b64encode(stream.getvalue()).decode())
        order=[0,1] if secrets.randbits(1) else [1,0]
        clips.append({'name':title,'a':encoded[order[0]],'b':encoded[order[1]]})
        key[title]={'A':'filter-on' if order[0]==0 else 'filter-off','B':'filter-on' if order[1]==0 else 'filter-off','common_gain':gain}
    session=secrets.token_hex(12)
    (out/'index.html').write_text(PAGE.replace('__CLIPS__',json.dumps(clips)).replace('__SESSION__',session),encoding='utf-8')
    (out/'identity-key.json').write_text(json.dumps({'session':session,'capture':str(capture),'key':key},indent=2),encoding='utf-8')
    print(out/'index.html')

if __name__=='__main__':main()
