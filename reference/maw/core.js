function defineCore(){
'use strict';
const PI=Math.PI, TAU=2*Math.PI;
const clamp=(x,a,b)=>x<a?a:(x>b?b:x);
const dB2g=d=>Math.pow(10,d/20);
const wrap=x=>{x=(x+1)%2;if(x<0)x+=2;return x-1;};
class RNG{constructor(seed){this.s=(seed>>>0)||0x9e3779b9;}
  next(){let x=this.s;x^=x<<13;x>>>=0;x^=x>>>17;x^=x<<5;x>>>=0;this.s=x;return x/4294967296;} // xorshift32, [0,1)
  bi(){return this.next()*2-1;}
  gauss(){let u=0,v=0;while(u===0)u=this.next();v=this.next();return Math.sqrt(-2*Math.log(u))*Math.cos(TAU*v);}}

// ---------- primitives ----------
class SVF{ // Zavalishin TPT state-variable filter
  constructor(){this.ic1=0;this.ic2=0;this.lp=0;this.bp=0;this.hp=0;this.k=1;this.a1=0;this.a2=0;this.a3=0;}
  set(fc,q,sr){const g=Math.tan(PI*clamp(fc,10,sr*0.45)/sr);const k=1/Math.max(q,0.1);this.k=k;this.a1=1/(1+g*(g+k));this.a2=g*this.a1;this.a3=g*this.a2;}
  p(v0){const v3=v0-this.ic2;const v1=this.a1*this.ic1+this.a2*v3;const v2=this.ic2+this.a2*this.ic1+this.a3*v3;this.ic1=2*v1-this.ic1;this.ic2=2*v2-this.ic2;this.lp=v2;this.bp=v1;this.hp=v0-this.k*v1-v2;return v2;}
}
class LR4{ // Linkwitz-Riley 4th-order crossover: lo+hi is allpass
  constructor(){this.l1=new SVF();this.l2=new SVF();this.h1=new SVF();this.h2=new SVF();this.lo=0;this.hi=0;}
  set(fc,sr){this.l1.set(fc,0.7071,sr);this.l2.set(fc,0.7071,sr);this.h1.set(fc,0.7071,sr);this.h2.set(fc,0.7071,sr);}
  split(x){this.l1.p(x);this.lo=this.l2.p(this.l1.lp);this.h1.p(x);this.h2.p(this.h1.hp);this.hi=this.h2.hp;}
}
class DCB{constructor(sr){this.R=1-TAU*10/sr;this.x1=0;this.y1=0;} p(x){const y=x-this.x1+this.R*this.y1;this.x1=x;this.y1=y;return y;}}
class OnePole{constructor(){this.y=0;this.a=0.1;} set(fc,sr){this.a=1-Math.exp(-TAU*clamp(fc,5,sr*0.45)/sr);} p(x){this.y+=this.a*(x-this.y);return this.y;}}
class Delay{
  constructor(n){this.b=new Float32Array(n);this.n=n;this.w=0;}
  write(x){this.b[this.w]=x;this.w++;if(this.w>=this.n)this.w=0;}
  read(t){const n=this.n;t=clamp(t,2,n-3);const r=this.w-t;const i=Math.floor(r),fr=r-i;const b=this.b;
    const i0=((i-1)%n+n)%n,i1=(i0+1)%n,i2=(i1+1)%n,i3=(i2+1)%n;const y0=b[i0],y1=b[i1],y2=b[i2],y3=b[i3];
    const c1=0.5*(y2-y0),c2=y0-2.5*y1+2*y2-0.5*y3,c3=0.5*(y3-y0)+1.5*(y1-y2);return ((c3*fr+c2)*fr+c1)*fr+y1;}
}
// Inertia: every knob is a mass on a spring. inertia 0 = instant, 1 = heavy, ringing.
class Spring{
  constructor(v){this.x=v;this.v=0;this.t=v;}
  set(t){this.t=t;}
  step(I,dt){ if(I<=0.001){this.x=this.t;this.v=0;return this.x;}
    const w=TAU*(30*Math.pow(0.02,I)); const z=1-0.85*I;
    const a=w*w*(this.t-this.x)-2*z*w*this.v; this.v+=a*dt; this.x+=this.v*dt; return this.x; }
}
// N× oversampling wrapper for a memoryless nonlinearity (windowed-sinc FIR up/down, direct form — fine for a prototype)
const OS_TAPS={};
function osTaps(f){if(OS_TAPS[f])return OS_TAPS[f];const N=12*f-1,M=(N-1)/2,fc=0.5/f;const h=new Float64Array(N);let s=0;
  for(let i=0;i<N;i++){const k=i-M;const sinc=k===0?1:Math.sin(2*PI*fc*k)/(2*PI*fc*k);const w=0.54-0.46*Math.cos(TAU*i/(N-1));h[i]=2*fc*sinc*w;s+=h[i];}
  for(let i=0;i<N;i++)h[i]/=s;return OS_TAPS[f]=h;}
class OSn{
  constructor(){this.f=0;this.setFactor(2);}
  setFactor(f){if(f===this.f)return;this.f=f;this.h=osTaps(f);this.N=this.h.length;this.b1=new Float64Array(this.N);this.b2=new Float64Array(this.N);this.i1=0;this.i2=0;}
  fir(buf,idx){const h=this.h,N=this.N;let acc=0,j=idx;for(let i=0;i<N;i++){acc+=h[i]*buf[j];j--;if(j<0)j=N-1;}return acc;}
  run(x,fn){const f=this.f,N=this.N;let out=0;
    for(let k=0;k<f;k++){this.i1++;if(this.i1>=N)this.i1=0;this.b1[this.i1]=k===0?f*x:0;const u=this.fir(this.b1,this.i1);
      this.i2++;if(this.i2>=N)this.i2=0;this.b2[this.i2]=fn(u);}
    return this.fir(this.b2,this.i2);}
}

// ---------- transfer curves ----------  x = driven+biased input, s = per-stage "shape" 0..1
// classic overdrive (Doidic piecewise), small-signal slope 1
function od(x){const u=x*0.5,a=Math.abs(u),sg=u<0?-1:1;if(a<1/3)return 2*u;if(a<2/3)return sg*(3-(2-3*a)*(2-3*a))/3;return sg;}
function trifold(x){let u=(x+1)%4;if(u<0)u+=4;return 1-Math.abs(u-2);} // continuous fold of R onto [-1,1]
function tri(u){u=u-Math.floor(u);return 4*Math.abs(u-0.5)-1;}
// deterministic shard table (same on main thread and in the worklet)
const SHARDS=(()=>{let seed=1337;const r=()=>{seed=(seed*1103515245+12345)&0x7fffffff;return seed/0x7fffffff;};const t=[];
  for(let j=0;j<32;j++)t.push({p:r()*2-1,w:0.015+0.08*r()*r(),h:(r()<0.5?-1:1)*(0.15+0.3*r())});return t;})();
const logcosh=x=>{const a=Math.abs(x);return a>20?a-Math.LN2:Math.log(Math.cosh(x));};
const clipAD=x=>{const a=Math.abs(x);return a<=1?0.5*x*x:a-0.5;};
const wrapAD=x=>{const w=wrap(x);return 0.5*w*w;};
const AD={ // antiderivative F(x,s) with F' = f; only for curves where it is closed-form
  0:(x,s)=>(1-s)*logcosh(x)+s*clipAD(x),
  5:(x,s)=>{let u=(x+1)%4;if(u<0)u+=4;const triAD=u<=2?0.5*(u-1)*(u-1):1-0.5*(3-u)*(3-u);return (1-s)*(-2/PI)*Math.cos(0.5*PI*x)+s*triAD;},
  8:(x,s)=>(1-s)*clipAD(x)+s*wrapAD(x),
};
const SHAPERS=[
 {name:'soft (tanh→clip)', f:(x,s)=>(1-s)*Math.tanh(x)+s*clamp(x,-1,1)},
 {name:'hard clip (knee↑)', f:(x,s)=>{const k=0.9*s,a=Math.abs(x),sg=x<0?-1:1;if(k<1e-4||a<=1-k)return clamp(x,-1,1);if(a>=1+k)return sg;const d=a-(1-k);return sg*(a-d*d/(4*k));}},
 {name:'overdrive (bite↑)', f:(x,s)=>{const y=od(x);return y*(1+1.5*s*(1-y*y));}},
 {name:'tube (cubic+even)', f:(x,s)=>{const c=clamp(x,-1,1);return 1.5*(c-c*c*c/3)+s*0.5*c*c;}},
 {name:'diode (asym ceiling)', f:(x,s)=>{const k=1+6*s;return x>0?Math.tanh(x):Math.tanh(x*k)/k;}},
 {name:'fold (sine→triangle)', f:(x,s)=>(1-s)*Math.sin(0.5*PI*x)+s*trifold(x)},
 {name:'rectify (half→full)', f:(x,s)=>{const y=(1-s)*x+s*Math.abs(x);return clamp(y*(1+s),-1,1);}},
 {name:'crush (bits↓)', f:(x,s)=>{const L=Math.pow(2,8-7*s);return clamp(Math.round(x*L)/L,-1,1);}},
 {name:'clip→wrap', f:(x,s)=>(1-s)*clamp(x,-1,1)+s*wrap(x)},
 {name:'cheby (harmonic n)', f:(x,s)=>Math.cos((1+4*s)*Math.acos(trifold(x)))},
 // wiggle whose phase advances fastest near zero, so ridges crowd toward the centre and go chaotic as s rises
 {name:'polynomial (wiggle↑)', f:(x,s)=>{const c=clamp(x,-1,1),a=Math.abs(c),sg=c<0?-1:1;const N=1+9*s,p=1-0.75*s;return clamp(c+0.45*s*sg*Math.sin(PI*N*Math.pow(a,p))*(1-a*a*a*a)*Math.tanh(6*a),-1,1);}},
 // triangle-wave octaves: shape adds octaves AND raises ridge amplitude; octaves decay slowly (0.8) so the fine ridges stay strong
 {name:'fractal (ridges↑)', f:(x,s)=>{const c=clamp(x,-1,1);const K=1+7*s,kf=Math.floor(K),fr=K-kf;let y=0,amp=1,norm=0;
   for(let k=1;k<=kf+1;k++){const g=k<=kf?1:fr;y+=g*amp*tri(Math.pow(2,k-1)*c*0.5+0.25);norm+=g*amp;amp*=0.8;}
   return trifold((1-0.5*s)*c+1.6*s*y/norm);}},
 // fixed pseudorandom smattering of narrow pulse offsets along the curve; s adds more of them
 {name:'shards (count↑)', f:(x,s)=>{const c=clamp(x,-1,1);const J=Math.min(32,Math.round(s*32));let y=c;for(let j=0;j<J;j++){const sh=SHARDS[j];const d=(c-sh.p)/sh.w;if(d>-1&&d<1)y+=sh.h*(0.5+0.5*Math.cos(PI*d));}return clamp(y,-1,1);}},
 // signal-dependent white noise disturbance (silent when silent)
 {name:'noise (amount↑)', f:(x,s,r)=>{const c=clamp(x,-1,1);return clamp(c+s*(r?r.bi():0)*Math.abs(c),-1,1);}},
];

const ROUTES=['series','parallel','multiband','mid/side','feedback'];

function defaultStage(){return {on:true,drive:12,bias:0,a:0,b:5,morph:0,shape:0.3,floor:0,ft:0,fpos:1,cut:2000,res:0.15,level:0,mix:1};}
function defaultParams(){return {
  src:'super',spread:14,inGain:0,latch:false,
  route:0,os:2,adaa:false,auto:0.7,autoMode:0,driveRef:0,inertia:0,eco:0,flux:0,wet:1,out:-6,
  xo1:200,xo2:2000,
  fbAmt:0.3,fbMode:0,fbSemi:0,fbMs:8,fbDamp:5000,fbHp:80,fbTap:2,
  envA:5,envR:80,lfoRate:2,
  mod:{ed:0,ec:0,em:0,ld:0,lc:0,lb:0},
  stages:[defaultStage(),defaultStage(),defaultStage()],
};}

// ---------- one distortion stage ----------
class Stage{
  constructor(sr,G,p){this.sr=sr;this.G=G;this.p=p;
    this.f=[new SVF(),new SVF()];this.dc=[new DCB(sr),new DCB(sr)];this.os=[new OSn(),new OSn()];this.u1=[0,0];this.F1=[0,0];this.adaaOK=false;
    this.drive=new Spring(p.drive);this.bias=new Spring(p.bias);this.cut=new Spring(Math.log2(p.cut));this.morph=new Spring(p.morph);
    this.g=1;this.bs=0;this.m=0;this.walk=0;this.env=0;this.envIn=0;this.lvl=1;this.comp=1;this.g1=1;this.bs1=0;this.m1=0;this.comp1=1;this.g0=1;this.bs0=0;this.m0=0;this.comp0=1;this.pk=1;this.pk0=1;this.pk1=1;this.fa=SHAPERS[0].f;this.fb=SHAPERS[1].f;
    this.sd=this.shapeDriven.bind(this);this.sdA=(v)=>this.shapeADAA(v,this._ch);this._ch=0;this.tmp=new Float64Array(32);}
  control(mod){const p=this.p,G=this.G,dt=32/this.sr,I=G.inertia;
    this.drive.set(p.drive+mod.drive);this.bias.set(p.bias+mod.bias);this.cut.set(Math.log2(p.cut)+mod.cut);this.morph.set(p.morph+mod.morph+this.walk);
    this.g0=this.g1;this.bs0=this.bs1;this.m0=this.m1;this.comp0=this.comp1;this.pk0=this.pk1;
    this.g1=dB2g(clamp(this.drive.step(I,dt),-12,60));const pk=clamp(this.envIn*0.5*PI,0.02,1.5);this.pk1=G.driveRef?pk:1;if(G.driveRef)this.g1/=pk; /* drive-ref: y = pk·f(g·x/pk) — position on the curve, dynamics preserved */
    this.bs1=clamp(this.bias.step(I,dt),-1,1);this.m1=clamp(this.morph.step(I,dt),0,1);
    this.g=this.g1;this.bs=this.bs1;this.m=this.m1; // used by the reference-sine measurement below
    const fc=Math.pow(2,clamp(this.cut.step(I,dt),4.3,14.3));const q=0.7071*Math.pow(20,p.res);
    this.f[0].set(fc,q,this.sr);this.f[1].set(fc,q,this.sr);
    this.lvl=dB2g(p.level);this.fa=SHAPERS[p.a].f;this.fb=SHAPERS[p.b].f;
    const osf=G.os===true?2:(G.os|0)||1;if(osf>1){this.os[0].setFactor(osf);this.os[1].setFactor(osf);}
    this.adaaOK=!!G.adaa&&p.floor<=0&&((this.m1<=0&&!!AD[p.a])||(this.m1>=1&&!!AD[p.b])||(!!AD[p.a]&&!!AD[p.b]));this.FA=AD[p.a];this.FB=AD[p.b];
    // measured auto-gain: RMS of a 0.5-amplitude reference sine through the current curve vs its input RMS
    const N=32,t=this.tmp;const A=(G.autoMode||G.driveRef)?clamp(this.envIn*0.5*PI,0.02,1.2):0.5;let mean=0,si=0,so=0;for(let i=0;i<N;i++){const u=A*Math.sin(TAU*i/N);const y=this.shape(u*this.g+this.bs)*this.pk1;t[i]=y;mean+=y;si+=u*u;}
    mean/=N;for(let i=0;i<N;i++){const d=t[i]-mean;so+=d*d;}
    const target=clamp(Math.pow(Math.sqrt(si/Math.max(so,1e-9)),G.auto),0.05,20);this.comp1+=0.25*(target-this.comp1);this.g=this.g0;this.bs=this.bs0;this.m=this.m0;this.comp=this.comp0;}
  tick(t){this.g=this.g0+(this.g1-this.g0)*t;this.bs=this.bs0+(this.bs1-this.bs0)*t;this.m=this.m0+(this.m1-this.m0)*t;this.comp=this.comp0+(this.comp1-this.comp0)*t;this.pk=this.pk0+(this.pk1-this.pk0)*t;}
  shape(u){const s=this.p.shape,m=this.m,r=this.rnd;if(m<=0)return this.fa(u,s,r);if(m>=1)return this.fb(u,s,r);return (1-m)*this.fa(u,s,r)+m*this.fb(u,s,r);}
  shapeDriven(v){const fl=this.p.floor;const y=this.shape(v*this.g+this.bs);if(fl<=0)return y;
    const t=clamp((Math.abs(v)-fl)/fl,0,1),w=t*t*(3-2*t);return v*this.g+w*(y-v*this.g);} // below floor: clean gain, smoothstep crossfade over [floor, 2·floor]
  shapeAD(u){const s=this.p.shape,m=this.m;if(m<=0)return this.FA(u,s);if(m>=1)return this.FB(u,s);return (1-m)*this.FA(u,s)+m*this.FB(u,s);}
  // first-order antiderivative anti-aliasing: y = (F(u)-F(u1))/(u-u1), falling back to f(midpoint) when the step is tiny
  shapeADAA(v,ch){const u=v*this.g+this.bs;const u1=this.u1[ch];const F=this.shapeAD(u);const d=u-u1;let y;
    if(Math.abs(d)<1e-4)y=this.shape(0.5*(u+u1));else y=(F-this.F1[ch])/d;this.u1[ch]=u;this.F1[ch]=F;return y;}
  filt(x,ch){const f=this.f[ch];f.p(x);const t=this.p.ft;return t===1?f.lp:t===2?f.bp:f.hp;}
  process(x,ch){const p=this.p;if(!p.on)return x;const dry=x;if(ch===0){const ax=Math.abs(x);this.envIn+=0.0003*(ax-this.envIn);}
    if(p.ft&&p.fpos===0)x=this.filt(x,ch);
    const osf=this.G.os===true?2:(this.G.os|0)||1;let y;
    if(this.adaaOK){this._ch=ch;y=osf>1?this.os[ch].run(x,this.sdA):this.shapeADAA(x,ch);}
    else y=osf>1?this.os[ch].run(x,this.sd):this.shapeDriven(x);
    y=this.dc[ch].p(y)*this.comp*this.pk;
    if(p.ft&&p.fpos===1)y=this.filt(y,ch);
    y=(dry*(1-p.mix)+y*p.mix)*this.lvl;
    const a=Math.abs(y);this.env+=(a>this.env?0.02:0.0015)*(a-this.env);
    return y;}
}

// ---------- the device ----------
class MawCore{
  constructor(sr){this.sr=sr;this.P=defaultParams();
    this.stages=this.P.stages.map(p=>new Stage(sr,this.P,p));
    this.xo=[[new LR4(),new LR4()],[new LR4(),new LR4()]]; // [ch][xo]
    this.dl=[new Delay(Math.ceil(sr)),new Delay(Math.ceil(sr))];this.fbLP=[new OnePole(),new OnePole()];this.fbHP=[new SVF(),new SVF()];this.fbT=100;this.fbTarget=100;this.fbNorm=1;
    this.voices=[];for(let i=0;i<8;i++)this.voices.push({on:false,gate:false,note:0,f:110,ph:[0,0,0],env:0,age:0});
    this.lastF=110;this.smp=null;this.spos=0;this.k=0;this.envF=0;this.aA=0.1;this.aR=0.01;this.lfoPh=0;this.lfo=0;
    this.sL=0;this.sR=0;this.age=0;this.mods=[{},{},{}];this.ig=1;this.og=1;this.wet=1;this.rng=new RNG(0x51ED);for(const s of this.stages)s.rnd=this.rng;}
  msg(d){if(d.t==='p')this.setParams(d.P);else if(d.t==='on')this.noteOn(d.n,d.v);else if(d.t==='off')this.noteOff(d.n);
    else if(d.t==='smp'){this.smp={L:d.L,R:d.R,n:d.L.length};this.spos=0;}else if(d.t==='panic'){for(const v of this.voices){v.on=false;v.gate=false;}}}
  setParams(P){const latchWas=this.P.latch;Object.assign(this.P,P);for(let i=0;i<3;i++){Object.assign(this.P.stages[i],P.stages[i]);this.stages[i].p=this.P.stages[i];}
    if(latchWas&&!this.P.latch)for(const v of this.voices)v.gate=false;}
  noteOn(n,vel){let v=this.voices.find(v=>v.on&&v.note===n)||this.voices.find(v=>!v.on);
    if(!v){v=this.voices.reduce((a,b)=>a.age<b.age?a:b);}
    v.on=true;v.gate=true;v.note=n;v.f=440*Math.pow(2,(n-69)/12);v.age=++this.age;v.vel=vel||1;this.lastF=v.f;}
  noteOff(n){if(this.P.latch)return;for(const v of this.voices)if(v.on&&v.note===n)v.gate=false;}
  control(){const P=this.P,sr=this.sr,dt=32/sr;
    this.ig=dB2g(P.inGain);this.og=dB2g(P.out);this.wet=P.wet;
    this.aA=1-Math.exp(-1/(P.envA*0.001*sr));this.aR=1-Math.exp(-1/(P.envR*0.001*sr));
    this.lfoPh=(this.lfoPh+P.lfoRate*dt)%1;this.lfo=Math.sin(TAU*this.lfoPh);
    const envs=this.stages.map(s=>s.env),sum=envs[0]+envs[1]+envs[2];
    for(let i=0;i<3;i++){const s=this.stages[i];
      s.walk+=2*(0-s.walk)*dt+P.flux*0.8*Math.sqrt(dt)*this.rng.gauss();s.walk=clamp(s.walk,-0.6,0.6);
      const other=(sum-envs[i])*0.5; const eco=-P.eco*36*other;
      const m=this.mods[i];m.drive=P.mod.ed*24*this.envF+P.mod.ld*24*this.lfo+eco;m.cut=P.mod.ec*4*this.envF+P.mod.lc*3*this.lfo;
      m.morph=P.mod.em*this.envF;m.bias=P.mod.lb*0.6*this.lfo;s.control(m);}
    if(P.route===2){for(const c of this.xo){c[0].set(P.xo1,sr);c[1].set(Math.max(P.xo2,P.xo1*1.5),sr);}}
    if(P.route===4){this.fbTarget=P.fbMode===0?sr/(this.lastF*Math.pow(2,P.fbSemi/12)):P.fbMs*0.001*sr;
      this.fbLP[0].set(P.fbDamp,sr);this.fbLP[1].set(P.fbDamp,sr);this.fbHP[0].set(P.fbHp,0.7071,sr);this.fbHP[1].set(P.fbHp,0.7071,sr);
      // normalise the loop so fbAmt=100% ≈ unity small-signal loop gain regardless of how hot the stages are
      let gain=1;for(let i=0;i<=P.fbTap;i++){const s=this.stages[i];if(!s.p.on)continue;
        const sl=Math.max(0.25,Math.abs((s.shape(s.bs+1e-3)-s.shape(s.bs-1e-3))*500));gain*=s.p.mix*sl*s.g*s.comp*s.lvl+(1-s.p.mix)*s.lvl;}
      this.fbNorm=1/clamp(gain,0.05,400);}}
  source(){const P=this.P,sr=this.sr;let L=0,R=0;
    if(P.src==='sample'){if(this.smp){const s=this.smp;L=s.L[this.spos];R=s.R[this.spos];this.spos++;if(this.spos>=s.n)this.spos=0;}this.sL=L;this.sR=R;return;}
    const attA=1-Math.exp(-1/(0.003*sr)),relA=1-Math.exp(-1/(0.15*sr));
    for(const v of this.voices){if(!v.on)continue;
      v.env+=(v.gate?attA:relA)*((v.gate?1:0)-v.env);if(!v.gate&&v.env<1e-4){v.on=false;continue;}
      const dt=v.f/sr;let y=0,l=0,r=0;
      if(P.src==='sine'){v.ph[0]=(v.ph[0]+dt)%1;y=Math.sin(TAU*v.ph[0]);l=r=y;}
      else if(P.src==='saw'){v.ph[0]=(v.ph[0]+dt)%1;y=saw(v.ph[0],dt);l=r=y;}
      else if(P.src==='square'){v.ph[0]=(v.ph[0]+dt)%1;y=sqr(v.ph[0],dt);l=r=y;}
      else if(P.src==='pulse'){v.ph[0]=(v.ph[0]+dt)%1;y=0.5*(saw(v.ph[0],dt)-saw((v.ph[0]+0.12)%1,dt));l=r=y;}
      else{const d=Math.pow(2,P.spread/1200);const d0=dt,d1=dt/d,d2=dt*d;
        v.ph[0]=(v.ph[0]+d0)%1;v.ph[1]=(v.ph[1]+d1)%1;v.ph[2]=(v.ph[2]+d2)%1;
        const c=saw(v.ph[0],d0),a=saw(v.ph[1],d1),b=saw(v.ph[2],d2);
        l=0.45*c+0.6*a+0.2*b;r=0.45*c+0.2*a+0.6*b;}
      L+=l*v.env*0.35*v.vel;R+=r*v.env*0.35*v.vel;}
    this.sL=L;this.sR=R;}
  process(outL,outR,n){const S=this.stages,P=this.P;
    for(let i=0;i<n;i++){
      if((this.k&31)===0)this.control();const tt=(this.k&31)/32;S[0].tick(tt);S[1].tick(tt);S[2].tick(tt);this.k++;
      this.source();const xL=this.sL*this.ig,xR=this.sR*this.ig;
      const a=Math.abs(xL+xR)*0.5;this.envF+=(a>this.envF?this.aA:this.aR)*(a-this.envF);
      let yL,yR;
      switch(P.route){
        case 0:yL=S[2].process(S[1].process(S[0].process(xL,0),0),0);yR=S[2].process(S[1].process(S[0].process(xR,1),1),1);break;
        case 1:yL=S[0].process(xL,0)+S[1].process(xL,0)+S[2].process(xL,0);yR=S[0].process(xR,1)+S[1].process(xR,1)+S[2].process(xR,1);break;
        case 2:{const c=this.xo[0];c[0].split(xL);c[1].split(c[0].hi);yL=S[0].process(c[0].lo,0)+S[1].process(c[1].lo,0)+S[2].process(c[1].hi,0);
                const d=this.xo[1];d[0].split(xR);d[1].split(d[0].hi);yR=S[0].process(d[0].lo,1)+S[1].process(d[1].lo,1)+S[2].process(d[1].hi,1);break;}
        case 3:{let m=S[0].process((xL+xR)*0.5,0),s=S[1].process((xL-xR)*0.5,1);yL=S[2].process(m+s,0);yR=S[2].process(m-s,1);break;}
        default:{this.fbT+=0.003*(this.fbTarget-this.fbT);const k=P.fbAmt*this.fbNorm;
                this.fbHP[0].p(this.fbLP[0].p(this.dl[0].read(this.fbT)));this.fbHP[1].p(this.fbLP[1].p(this.dl[1].read(this.fbT)));
                const fL=this.fbHP[0].hp,fR=this.fbHP[1].hp;
                let tL=S[0].process(xL+k*fL,0),tR=S[0].process(xR+k*fR,1);if(P.fbTap===0){this.dl[0].write(Math.tanh(tL));this.dl[1].write(Math.tanh(tR));}
                tL=S[1].process(tL,0);tR=S[1].process(tR,1);if(P.fbTap===1){this.dl[0].write(Math.tanh(tL));this.dl[1].write(Math.tanh(tR));}
                yL=S[2].process(tL,0);yR=S[2].process(tR,1);if(P.fbTap===2){this.dl[0].write(Math.tanh(yL));this.dl[1].write(Math.tanh(yR));}}}
      const w=this.wet;
      outL[i]=clamp((xL*(1-w)+yL*w)*this.og,-1,1);outR[i]=clamp((xR*(1-w)+yR*w)*this.og,-1,1);}}
}
function blep(t,dt){if(t<dt){t/=dt;return t+t-t*t-1;}if(t>1-dt){t=(t-1)/dt;return t*t+t+t+1;}return 0;}
function saw(t,dt){return 2*t-1-blep(t,dt);}
function sqr(t,dt){let y=t<0.5?1:-1;y+=blep(t,dt);y-=blep((t+0.5)%1,dt);return y;}
return {MawCore,SHAPERS,AD,ROUTES,defaultParams,defaultStage,clamp,dB2g,RNG};
}
if(typeof module!=='undefined')module.exports=defineCore;
