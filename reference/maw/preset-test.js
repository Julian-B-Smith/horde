const d=require('./core.js');const C=d();const {MawCore,SHAPERS}=C;const {PRESETS,applyPresetTo}=require('./presets.js');const SR=48000;
let ok=true;
for(const name in PRESETS){const P=applyPresetTo(C.defaultParams(),PRESETS[name],SHAPERS);
 for(const s of P.stages)if(s.a<0||s.b<0){console.log('  BAD CURVE NAME in',name);ok=false;}
 const c=new MawCore(SR);c.setParams(P);c.noteOn(45,1);c.noteOn(52,1);const L=new Float32Array(128),R=new Float32Array(128);let peak=0,sum=0,n=0,bad=0,t1=0,t2=0;
 for(let b=0;b<600;b++){c.process(L,R,128);for(let i=0;i<128;i++){const v=L[i];if(!isFinite(v))bad++;if(b>=100&&b<300){peak=Math.max(peak,Math.abs(v));sum+=v*v;n++;}if(b>=470&&b<500)t1=Math.max(t1,Math.abs(v));if(b>=570)t2=Math.max(t2,Math.abs(v));}if(b===300){c.noteOff(45);c.noteOff(52);}}
 const rms=Math.sqrt(sum/n);const good=bad===0&&rms>0.02&&peak<1.0001&&t2<0.7*t1&&t2<0.1;if(!good)ok=false;
 console.log((good?'  ok   ':'  FAIL ')+name.padEnd(48)+`peak ${peak.toFixed(2)}  rms ${rms.toFixed(3)}  tail ${t1.toExponential(1)}→${t2.toExponential(1)}  nonfinite ${bad}`);}
console.log(ok?'all presets audible, finite, and decay after release':'PRESET PROBLEMS');
