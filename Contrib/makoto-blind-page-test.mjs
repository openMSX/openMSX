import fs from 'node:fs';
import vm from 'node:vm';
import {webcrypto} from 'node:crypto';
import assert from 'node:assert/strict';
const html=fs.readFileSync(process.argv[2],'utf8');
const script=html.match(/<script>([\s\S]*)<\/script>/)[1];
const nodes=new Map();
function node(id){if(!nodes.has(id))nodes.set(id,{value:id==='volume'?'0':'',disabled:true,textContent:'',classList:{toggle(){},add(){},remove(){}}});return nodes.get(id)}
const saved=new Map();
class AudioContext {
 currentTime=0; destination={};
 async resume(){}
 createGain(){return {gain:{value:0,cancelScheduledValues(){},setValueAtTime(){},linearRampToValueAtTime(){},setTargetAtTime(){}},connect(){return this}}}
 createBufferSource(){return {connect(){return this},start(){},stop(){}}}
 async decodeAudioData(raw){const b=Buffer.from(raw);assert.equal(b.toString('ascii',0,4),'RIFF');return {duration:b.readUInt32LE(40)/b.readUInt32LE(28)}}
}
const ctx=vm.createContext({crypto:webcrypto,Uint32Array,Uint8Array,AudioContext,
 document:{getElementById:node},localStorage:{getItem:k=>saved.get(k),setItem:(k,v)=>saved.set(k,v)},
 atob:s=>Buffer.from(s,'base64').toString('binary'),setInterval(){},setTimeout(){}});
vm.runInContext(script,ctx);
await vm.runInContext(`(async()=>{
 if(session.order.length!==12)throw Error('trial count');
 for(let i=0;i<3;i++)if(session.order.filter(t=>t.clip===i).length!==4)throw Error('unbalanced passages');
 for(let i=0;i<12;i++){
  if(!document.getElementById('answerA').disabled)throw Error('answer allowed before listening');
  await play('A');
  if(!document.getElementById('answerA').disabled)throw Error('answer allowed after A only');
  context.currentTime+=.5;
  await play('B');
  if(Math.abs(position()-.5)>.00001)throw Error('switch lost position');
  await play('X');
  if(document.getElementById('answerA').disabled)throw Error('answer remains disabled');
  const chosen=trial().x;
  answer(chosen);
 }
 if(session.score.correct!==12)throw Error('scoring');
 if(session.score.chance_tail_probability!==1/4096)throw Error('binomial tail');
})()`,ctx);
assert.match(node('result').textContent,/12 of 12/);
assert.equal(saved.size,1);
assert.ok(!script.includes('filter-on')&&!script.includes('filter-off'));
console.log('PASS: anonymous embedded WAVs, balanced 12 trials, listening gate, synchronized switching, delayed scoring and persistence.');
