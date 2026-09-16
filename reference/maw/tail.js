const d=require('./core.js');const C=d();const {MawCore,SHAPERS}=C;const SR=48000;const idx=n=>SHAPERS.findIndex(s=>s.name.startsWith(n));
// decaying 200 Hz sine (T60 = 300 ms) through one stage; report when the OUTPUT crosses -40/-60 dB relative to its peak
function tail(cfg){const c=new MawCore(SR);const P=c.P;P.src='sample';P.out=0;P.auto=0.7;P.stages[1].on=false;P.stages[2].on=false;Object.assign(P.stages[0],cfg);
 const n=SR;const x=Float32Array.from({length:n},(_,i)=>0.5*Math.sin(2*Math.PI*200*i/SR)*Math.pow(10,-3*i/(0.3*SR)));c.msg({t:'smp',L:x,R:x});
 const L=new Float32Array(n),R=new Float32Array(n);c.process(L,R,n);c.process(L,R,n);
 const env=[];for(let i=0;i<n;i+=480){let m=0;for(let j=i;j<i+480;j++)m=Math.max(m,Math.abs(L[j]));env.push(20*Math.log10(m+1e-9));}
 const pk=Math.max(...env);const t=th=>{const k=env.findIndex(v=>v<pk-th);return k<0?'>1000':(k*10)+' ms';};return `−40 dB at ${t(40)}, −60 dB at ${t(60)}`;}
console.log('input alone (T60 300 ms): −40 dB at 200 ms, −60 dB at 300 ms by construction');
const old=(x,s)=>{const c=Math.max(-1,Math.min(1,x)),a=Math.abs(c),sg=c<0?-1:1;const N=1+9*s,p=1-0.75*s;return Math.max(-1,Math.min(1,c+0.45*s*sg*Math.sin(Math.PI*N*Math.pow(a,p))*(1-a*a*a*a)));};
SHAPERS.push({name:'polynomial-OLD',f:old});
for(const [nm,cfg] of [['polynomial OLD, drive 24',{a:SHAPERS.length-1,shape:0.7,drive:24}],['polynomial tapered, drive 24',{a:idx('polynomial'),shape:0.7,drive:24}],['polynomial tapered + floor −40 dB',{a:idx('polynomial'),shape:0.7,drive:24,floor:0.01}],
  ['shards, drive 24',{a:idx('shards'),shape:0.8,drive:24}],['shards + floor −40 dB',{a:idx('shards'),shape:0.8,drive:24,floor:0.01}],['soft, drive 24',{a:idx('soft'),drive:24}],['soft + floor −40 dB',{a:idx('soft'),drive:24,floor:0.01}]])console.log('  '+nm.padEnd(36)+tail(cfg));
