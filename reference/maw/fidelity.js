const d=require('./core.js');const C=d();const {MawCore,SHAPERS}=C;
const SR=48000,N=8192;
// ---- FFT (radix-2, in place) ----
function fft(re,im){const n=re.length;for(let i=1,j=0;i<n;i++){let bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){[re[i],re[j]]=[re[j],re[i]];[im[i],im[j]]=[im[j],im[i]];}}
 for(let len=2;len<=n;len<<=1){const ang=-2*Math.PI/len,wr=Math.cos(ang),wi=Math.sin(ang);for(let i=0;i<n;i+=len){let cr=1,ci=0;for(let j=0;j<len/2;j++){const a=i+j,b=a+len/2;const tr=re[b]*cr-im[b]*ci,ti=re[b]*ci+im[b]*cr;re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;const t=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=t;}}}}
function spectrum(x,rect){const re=new Float64Array(N),im=new Float64Array(N);for(let i=0;i<N;i++){const w=rect?1:0.5-0.5*Math.cos(2*Math.PI*i/N);re[i]=x[i]*w;}fft(re,im);const m=new Float64Array(N/2);for(let i=0;i<N/2;i++)m[i]=re[i]*re[i]+im[i]*im[i];return m;}
const dB=p=>10*Math.log10(Math.max(p,1e-30));
const rms=x=>Math.sqrt(x.reduce((a,v)=>a+v*v,0)/x.length);
// ---- harness: run a stereo buffer through a core, return output after settling ----
function run(cfg,inL,inR,sr=SR){const c=new MawCore(sr);const P=c.P;P.src='sample';P.out=-6;P.wet=1;P.auto=0;P.stages[1].on=false;P.stages[2].on=false;
  const apply=(o,t)=>{for(const k in o){if(k==='stages')o.stages.forEach((s,i)=>Object.assign(t.stages[i],s));else if(k==='mod')Object.assign(t.mod,o.mod);else t[k]=o[k];}};apply(cfg,P);
  c.msg({t:'smp',L:inL,R:inR||inL});const n=inL.length;const L=new Float32Array(n),R=new Float32Array(n);
  // warm-up pass (filters/springs/auto-gain settle), then the measured pass
  c.process(new Float32Array(n),new Float32Array(n),n);c.process(L,R,n);return {L,R,c};}
const sine=(bin,amp=0.5)=>Float32Array.from({length:N},(_,i)=>amp*Math.sin(2*Math.PI*bin*i/N));
let seed=7;const rnd=()=>{seed=(seed*1103515245+12345)&0x7fffffff;return seed/0x7fffffff*2-1;};
const noise=(amp=0.3)=>Float32Array.from({length:N},()=>amp*rnd());
const idx=n=>SHAPERS.findIndex(s=>s.name.startsWith(n));
const OG=Math.pow(10,-6/20);
const pass=(ok,label,detail)=>console.log((ok?'  PASS ':'  FAIL ')+label.padEnd(58)+(detail||''));
const row=(...a)=>console.log('    '+a.map(v=>String(v).padEnd(24)).join(''));

console.log('\n== 1. bypass / identity ==');
{const x=noise();const {L}=run({stages:[{on:false}]},x);let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(L[i]-OG*x[i]));pass(e<1e-6,'all stages off → out == in',`max err ${e.toExponential(2)}`);}
{const x=noise();const {L}=run({stages:[{on:true,mix:0}]},x);let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(L[i]-OG*x[i]));pass(e<1e-6,'stage on, mix=0 → dry path bit-exact',`max err ${e.toExponential(2)}`);}

console.log('\n== 2. routing sums (all stages on, mix=0 = pure topology) ==');
{const x=noise();const base={stages:[{on:true,mix:0},{on:true,mix:0},{on:true,mix:0}]};
 const ref=spectrum(x,true);
 for(const [name,route] of [['multiband (LR4 sum)',2],['mid/side (recombine)',3]]){
  const {L}=run(Object.assign({route},base),x,route===3?noise():x);
  if(route===3){/* mono input path only: compare L vs x */}
  const sp=spectrum(L,true);let mx=-99,mn=99;for(let b=8;b<N/2-8;b++){const dv=dB(sp[b])-dB(ref[b])+6.02;mx=Math.max(mx,dv);mn=Math.min(mn,dv);}
  if(route===2)pass(mx<0.3&&mn>-0.3,name+' magnitude flat',`dev ${mn.toFixed(2)}..${mx.toFixed(2)} dB`);}
 const xl=noise(),xr=noise();const {L,R}=run(Object.assign({route:3},base),xl,xr);let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(L[i]-OG*xl[i]),Math.abs(R[i]-OG*xr[i]));pass(e<1e-5,'mid/side stereo recombine identity',`max err ${e.toExponential(2)}`);
 const {L:pl}=run(Object.assign({route:1},base),xl);e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(pl[i]-3*OG*xl[i]));pass(e<1e-5,'parallel sums 3× dry',`max err ${e.toExponential(2)}`);}

console.log('\n== 3. harmonic purity / symmetry (sine @ bin 100, drive 6 dB → peak 1.0, auto 0, OS on) ==');
{const x=sine(100);const H=(sp,k)=>{let m=0;for(let b=100*k-2;b<=100*k+2;b++)m=Math.max(m,sp[b]);return m;};
 for(const [n,s] of [[2,0.25],[3,0.5],[4,0.75]]){const {L}=run({stages:[{a:idx('cheby'),morph:0,shape:s,drive:6.02}]},x);const sp=spectrum(L);
   let best=0,tot=0;for(let k=1;k<=8;k++){tot+=H(sp,k);}const ratio=H(sp,n)/tot;pass(ratio>0.98,`cheby n=${n} from sine → ${(ratio*100).toFixed(1)}% of harmonic energy in H${n}`);}
 {const {L}=run({stages:[{a:idx('rectify'),shape:1,drive:6.02}]},x);const sp=spectrum(L);pass(H(sp,2)>100*H(sp,1),'full-wave rectify → octave up, fundamental gone',`H1/H2 = ${dB(H(sp,1)/H(sp,2)).toFixed(1)} dB`);}
 console.log('  odd/even symmetry at bias=0 (even-harmonic energy relative to odd):');
 for(const sh of SHAPERS){if(sh.name.startsWith('noise'))continue;const {L}=run({stages:[{a:sh.i=SHAPERS.indexOf(sh),shape:0.3,drive:12}]},x);const sp=spectrum(L);let odd=0,even=0;for(let k=1;k<=12;k++){(k%2?odd+=H(sp,k):even+=H(sp,k));}
   row(sh.name,`even/odd ${dB(even/odd).toFixed(1)} dB`,dB(even/odd)<-60?'(pure odd)':'(has even)');}}

console.log('\n== 4. aliasing ladder: sine @ 4688 Hz, drive 24 dB, shape 0.5 — non-harmonic energy / total (lower = cleaner) ==');
const aliasOf=(L)=>{const sp=spectrum(L);let tot=0,harm=0;for(let b=10;b<N/2-10;b++){tot+=sp[b];}for(let k=1;k<=5;k++){for(let b=800*k-4;b<=800*k+4;b++)harm+=sp[b];}return dB((tot-harm)/tot);};
{const x=sine(800,0.4);row('curve','1×','2×','4×','8×','ADAA 1×','ADAA 2×');
 for(const sh of SHAPERS){if(sh.name.startsWith('noise'))continue;const i=SHAPERS.indexOf(sh);const cols=[1,2,4,8].map(f=>aliasOf(run({os:f,stages:[{a:i,shape:0.5,drive:24}]},x).L).toFixed(1));
  const ad=C.AD[i]?[1,2].map(f=>aliasOf(run({os:f,adaa:true,stages:[{a:i,shape:0.5,drive:24}]},x).L).toFixed(1)):['—','—'];row(sh.name,...cols,...ad);}
 console.log('  target: ≤ −40 dB for "clean", ≤ −30 dB acceptable for a distortion device');}

console.log('\n== 4a. aliasing at a bass fundamental: sine @ 234 Hz (bin 40), drive 24 dB, shape 0.5 ==');
{const x=sine(40,0.4);const al=(L)=>{const sp=spectrum(L);let tot=0,harm=0;for(let b=10;b<N/2-10;b++){tot+=sp[b];}for(let k=1;k<=100;k++){for(let b=40*k-2;b<=40*k+2;b++)harm+=sp[b];}return dB((tot-harm)/tot);};
 row('curve','1×','2×','4×','8×','ADAA 2×');
 for(const nm of ['soft','fold','clip→wrap','cheby','polynomial','shards']){const i=idx(nm);const cols=[1,2,4,8].map(f=>al(run({os:f,stages:[{a:i,shape:0.5,drive:24}]},x).L).toFixed(1));const ad=C.AD[i]?al(run({os:2,adaa:true,stages:[{a:i,shape:0.5,drive:24}]},x).L).toFixed(1):'—';row(nm,...cols,ad);}}

console.log('\n== 4b. aliasing inside the feedback route (delay = 1/f so comb peaks are harmonic; fbAmt 0.7, tap after stage 1) ==');
{const x=sine(800,0.4);const T=1024/800; // samples per cycle of bin 800 at N=8192
 row('curve','1× no fb','1× fb','4× fb','ADAA 2× fb');
 for(const nm of ['soft','fold','hard clip','overdrive']){const i=idx(nm);const base={stages:[{a:i,shape:0.5,drive:24}]};const fbc={route:4,fbAmt:0.7,fbMode:1,fbMs:T/SR*1000,fbTap:0,fbDamp:16000,fbHp:20};
  const a=aliasOf(run(Object.assign({os:1},base),x).L),b=aliasOf(run(Object.assign({os:1},base,fbc),x).L),c=aliasOf(run(Object.assign({os:4},base,fbc),x).L);
  const d=C.AD[i]?aliasOf(run(Object.assign({os:2,adaa:true},base,fbc),x).L).toFixed(1):'—';row(nm,a.toFixed(1),b.toFixed(1),c.toFixed(1),d);}}

console.log('\n== 5. auto-gain flatness: output RMS vs drive per curve (auto=1, sine bin 100 amp 0.5) ==');
{const x=sine(100);row('curve','0dB','12dB','24dB','48dB','spread');
 for(const sh of SHAPERS){if(sh.name.startsWith('noise'))continue;const i=SHAPERS.indexOf(sh);const v=[0,12,24,48].map(dr=>rms(run({auto:1,stages:[{a:i,shape:0.3,drive:dr}]},x).L));const l=v.map(r=>20*Math.log10(r));row(sh.name,...l.map(q=>q.toFixed(1)),(Math.max(...l)-Math.min(...l)).toFixed(1)+' dB');}}

console.log('\n== 5b. input-level dependence: gain (out/in, dB) vs input amplitude at drive 12 dB, auto=1 — the "programme material" question ==');
{row('curve','in 0.05','in 0.15','in 0.5','in 1.0','range');
 for(const sh of SHAPERS){if(sh.name.startsWith('noise'))continue;const i=SHAPERS.indexOf(sh);const g=[0.05,0.15,0.5,1.0].map(a=>{const x=sine(100,a);return 20*Math.log10(rms(run({auto:1,stages:[{a:i,shape:0.3,drive:12}]},x).L)/rms(x))+6.02;});
  const gt=[0.05,0.15,0.5,1.0].map(a=>{const x=sine(100,a);return 20*Math.log10(rms(run({auto:1,autoMode:1,stages:[{a:i,shape:0.3,drive:12}]},x).L)/rms(x))+6.02;});
  row(sh.name,...g.map(v=>v.toFixed(1)),(Math.max(...g)-Math.min(...g)).toFixed(1)+' dB','tracked:',(Math.max(...gt)-Math.min(...gt)).toFixed(1)+' dB');}
 console.log('  (positive = louder than input; "tracked" = range with envelope-tracked auto-gain reference)');}

console.log('\n== 5c. CPU: ms of compute per second of stereo audio, one stage, node/V8 ==');
{const x=noise();const meas=cfg=>{const c=new MawCore(SR);const P=c.P;P.src='sample';P.stages[1].on=false;P.stages[2].on=false;Object.assign(P.stages[0],cfg.st||{});Object.assign(P,cfg.g||{});c.msg({t:'smp',L:x,R:x});
   const L=new Float32Array(1024),R=new Float32Array(1024);for(let i=0;i<20;i++)c.process(L,R,1024);const t0=process.hrtime.bigint();const blocks=200;for(let i=0;i<blocks;i++)c.process(L,R,1024);return Number(process.hrtime.bigint()-t0)/1e6/(blocks*1024/SR);};
 row('curve','1×','2×','4×','8×','ADAA 1×');
 for(const nm of ['soft','fold','tube','shards','fractal','polynomial']){const i=idx(nm);const c=[1,2,4,8].map(f=>meas({st:{a:i,drive:12},g:{os:f}}).toFixed(0));const ad=C.AD[i]?meas({st:{a:i,drive:12},g:{os:1,adaa:true}}).toFixed(0):'—';row(nm,...c,ad);}
 row('(all 3 stages off)',meas({st:{on:false},g:{os:1}}).toFixed(0));}

console.log('\n== 6. DC / silence / determinism ==');
{const x=sine(100);const {L}=run({stages:[{a:idx('tube'),bias:0.6,drive:12}]},x);const mean=L.reduce((a,v)=>a+v,0)/N;pass(Math.abs(mean)<1e-3,'tube + bias 0.6 → DC removed',`mean ${mean.toExponential(2)}`);}
{const z=new Float32Array(N);const {L}=run({stages:[{a:idx('noise'),shape:1,drive:48}]},z);pass(Math.max(...L.map(Math.abs))===0,'noise curve, silence in → exact silence out');}
{const z=new Float32Array(N);const {L}=run({route:4,fbAmt:1.2,stages:[{a:idx('overdrive'),drive:30},{on:true,a:idx('fold'),drive:20},{on:true}]},z);pass(Math.max(...L.map(Math.abs))===0,'feedback 120%, silence in → silence out (no self-oscillation from nothing)');}
{const x=noise();const a=run({stages:[{a:idx('fractal'),shape:0.7,drive:20,ft:1,cut:1200}]},x).L,b=run({stages:[{a:idx('fractal'),shape:0.7,drive:20,ft:1,cut:1200}]},x).L;let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(a[i]-b[i]));pass(e===0,'deterministic: two runs bit-identical',`max diff ${e}`);}
{const x=noise();const a=run({flux:0.5,stages:[{a:idx('soft'),b:idx('fold'),morph:0.5,drive:12}]},x).L,b=run({flux:0.5,stages:[{a:idx('soft'),b:idx('fold'),morph:0.5,drive:12}]},x).L;let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(a[i]-b[i]));pass(e===0,'flux>0: two runs bit-identical (seeded RNG)',`max diff ${e.toExponential(2)}`);}

{const x=noise();const a=run({stages:[{a:idx('noise'),shape:0.8,drive:12}]},x).L,b=run({stages:[{a:idx('noise'),shape:0.8,drive:12}]},x).L;let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(a[i]-b[i]));pass(e===0,'noise curve: two runs bit-identical (seeded RNG)',`max diff ${e}`);}

console.log('\n== 7. control-rate staircase: 5 Hz sine (≈DC within a block), lfo→drive 24 dB @ 10 Hz, inertia 0 ==');
{for(const interp of [false,true]){const c=new MawCore(SR);const P=c.P;P.src='sample';P.out=-6;P.auto=0;P.stages[1].on=false;P.stages[2].on=false;P.stages[0].drive=6;P.mod.ld=1;P.lfoRate=10;P.os=false;
 if(!interp)for(const s of c.stages)s.tick=function(){this.g=this.g1;this.bs=this.bs1;this.m=this.m1;this.comp=this.comp1;};
 const n=SR;const x=Float32Array.from({length:n},(_,i)=>0.3*Math.sin(2*Math.PI*5*i/SR));c.msg({t:'smp',L:x,R:x});const L=new Float32Array(n),R=new Float32Array(n);c.process(L,R,n);c.process(L,R,n);
 let eb=0,nb=0,ei=0,ni=0;for(let i=1;i<n;i++){const dd=Math.abs(L[i]-L[i-1]);if(i%32===0){eb+=dd;nb++;}else{ei+=dd;ni++;}}
 const ratio=(eb/nb)/(ei/ni);pass(ratio<3,(interp?'per-sample interpolated':'block-held coefficients')+': |Δ| at block boundaries vs inside',`ratio ${ratio.toFixed(1)}× (1 = no staircase)`);}}

console.log('\n== 7b. morph linearity: morph 0.5 == ½(A) + ½(B) (post-shaper path is linear) ==');
{const x=noise();const A=run({stages:[{a:idx('soft'),b:idx('fold'),morph:0,drive:12,ft:1,cut:900,res:0.5}]},x).L,B=run({stages:[{a:idx('soft'),b:idx('fold'),morph:1,drive:12,ft:1,cut:900,res:0.5}]},x).L,M=run({stages:[{a:idx('soft'),b:idx('fold'),morph:0.5,drive:12,ft:1,cut:900,res:0.5}]},x).L;
 let e=0;for(let i=0;i<N;i++)e=Math.max(e,Math.abs(M[i]-0.5*(A[i]+B[i])));pass(e<1e-5,'morph is a linear blend of curve outputs',`max err ${e.toExponential(2)}`);}

console.log('\n== 7b2. every stage parameter must audibly change the output (guards against a parameter silently disconnected) ==');
{const x=noise();const base={stages:[{a:idx('soft'),b:idx('fold'),morph:0.3,shape:0.3,drive:12,bias:0.1,ft:1,fpos:1,cut:2000,res:0.3,level:0,mix:1}]};
 const ref=run(base,x).L;const rmsd=(a,b)=>Math.sqrt(a.reduce((s,v,i)=>s+(v-b[i])**2,0)/a.length);const r0=rms(ref);
 for(const [k,v] of [['morph',0.9],['shape',0.9],['drive',24],['bias',0.5],['cut',400],['res',0.9],['level',-6],['mix',0.3],['floor',0.05],['a',idx('tube')],['b',idx('cheby')],['ft',3],['fpos',0]]){
   const cfg={stages:[Object.assign({},base.stages[0],{[k]:v})]};const y=run(cfg,x).L;const dd=20*Math.log10(rmsd(ref,y)/r0);pass(dd>-30,`stage.${k} changes output`,`${dd.toFixed(1)} dB rel. to signal`);}}

console.log('\n== 7c. latency: impulse through 1 vs 3 oversampled stages (linear-ish curve, tiny drive) ==');
{const imp=new Float32Array(N);imp[100]=0.5;for(const [k,cfg] of [[1,{stages:[{a:idx('soft'),drive:-12}]}],[3,{stages:[{a:idx('soft'),drive:-12},{on:true,a:idx('soft'),drive:-12},{on:true,a:idx('soft'),drive:-12}]}]]){
  const {L}=run(cfg,imp);let at=0,mx=0;for(let i=0;i<N;i++)if(Math.abs(L[i])>mx){mx=Math.abs(L[i]);at=i;}row(`${k} stage(s) OS on`,`peak at ${at} → latency ${at-100} samples`);}
 const {L}=run({os:false,stages:[{a:idx('soft'),drive:-12}]},imp);let at=0,mx=0;for(let i=0;i<N;i++)if(Math.abs(L[i])>mx){mx=Math.abs(L[i]);at=i;}row('1 stage OS off',`latency ${at-100} samples`);}

console.log('\n== 8. inertia spring step response (control rate 32 samples) ==');
{const c=new MawCore(SR);const sp=c.stages[0].drive;for(const I of [0.1,0.3,0.6,0.9]){sp.x=0;sp.v=0;sp.set(1);let peak=0,settle=-1;for(let k=0;k<20000;k++){const v=sp.step(I,32/SR);peak=Math.max(peak,v);if(settle<0&&k>10&&Math.abs(v-1)<0.02&&Math.abs(sp.v)<0.5)settle=k;}row(`inertia ${I}`,`overshoot ${((peak-1)*100).toFixed(0)}%`,`~settle ${(settle*32/SR*1000).toFixed(0)} ms`);}}

console.log('\n== 9. feedback loop-gain normalisation: burst decay should not depend on stage drive (fbAmt 0.6, tap after stage 1) ==');
{for(const dr of [0,20,40]){const c=new MawCore(SR);const P=c.P;P.src='sample';P.out=0;P.wet=1;P.route=4;P.fbAmt=0.6;P.fbMode=1;P.fbMs=5;P.fbTap=0;P.fbDamp=8000;P.auto=0;P.stages[1].on=false;P.stages[2].on=false;P.stages[0].a=idx('soft');P.stages[0].drive=dr;
  const n=SR*0.6|0;const x=new Float32Array(n);for(let i=0;i<2400;i++)x[i]=0.5*Math.sin(2*Math.PI*200*i/SR)*Math.sin(Math.PI*i/2400);c.msg({t:'smp',L:x,R:x});
  const L=new Float32Array(n),R=new Float32Array(n);c.process(L,R,n);const seg=(t0,t1)=>rms(L.slice(t0*SR|0,t1*SR|0));
  const a=seg(0.06,0.10),b=seg(0.16,0.20),cc=seg(0.30,0.34);row(`drive ${dr} dB`,`60ms ${dB(a*a).toFixed(1)} dB`,`160ms ${dB(b*b).toFixed(1)} dB`,`300ms ${dB(cc*cc).toFixed(1)} dB`,`decay ${(dB(a*a)-dB(cc*cc)).toFixed(1)} dB/240ms`);}}

console.log('\n== 10. oversampling FIR (23-tap Hamming sinc) frequency response ==');
{const os=new (C.MawCore)(SR).stages[0].os[0];const h=os.h;const resp=f=>{let re=0,im=0;for(let i=0;i<h.length;i++){re+=h[i]*Math.cos(2*Math.PI*f*i);im-=h[i]*Math.sin(2*Math.PI*f*i);}return 20*Math.log10(Math.hypot(re,im));};
 row('f (of base fs)','gain');for(const f of [0.05,0.15,0.2,0.25,0.3,0.4,0.5,0.6,0.75,1.0])row(`${f}·fs (${(f*SR/1000).toFixed(1)} kHz)`,resp(f/2).toFixed(1)+' dB');
 console.log('    latency: 11 taps @2× ×2 stages = 11 base samples per oversampled stage');}

console.log('\n== 11. sample-rate invariance: tube harmonic amplitudes, sine 500 Hz ==');
{const r=[];for(const sr of [44100,48000,96000]){const n=8192;const bin=Math.round(500/sr*n),f=bin*sr/n;const x=Float32Array.from({length:n},(_,i)=>0.5*Math.sin(2*Math.PI*bin*i/n));const {L}=run({stages:[{a:idx('tube'),shape:0.5,drive:12}]},x,x,sr);
  const sp=spectrum(L);const hk=k=>{const b=bin*k;let m=0;for(let q=b-3;q<=b+3;q++)m=Math.max(m,sp[q]);return dB(m);};r.push([sr,hk(1),hk(2),hk(3),hk(4),hk(5)]);}
 row('sr','H1','H2','H3','H4','H5');for(const q of r)row(q[0],...q.slice(1).map(v=>v.toFixed(1)));
 const dev=Math.max(...[1,2,3,4,5].map(k=>Math.max(...r.map(q=>q[k]))-Math.min(...r.map(q=>q[k]))));pass(dev<0.5,'harmonic amplitudes invariant across 44.1/48/96k',`max dev ${dev.toFixed(2)} dB`);}
