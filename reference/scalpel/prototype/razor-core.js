/*
 * razor-core.js — the Scalpel bench DSP core, extracted verbatim from prototype/scalpel-bench.html.
 * This is the PARITY ORACLE for the HORDE port: same code the published bench runs in its AudioWorklet.
 * Usage (Node): const RazorCore = require('./razor-core.js'); const c = new RazorCore(48000);
 *   c.set({...params}); c.noteOn(note, freqHz, velocity); c.render(L, R)  // Float32Array blocks
 * Randomness: the core calls Math.random(). For reproducible renders, install a seeded generator first
 * (verify/rng.js does this). See SPEC.md §9 for which behaviours are bit-parity targets and which are statistical.
 */
class RazorCore {
  static frac(x){ return x - Math.floor(x); }
  static rnd(){ return Math.random()*2 - 1; }
  static hash(i){ const x = Math.sin(i*127.1 + 311.7)*43758.5453; return (x - Math.floor(x))*2 - 1; }
  static wave(s, x){
    switch(s){
      case 0: return Math.sin(6.283185307179586 * x);
      case 1: return x < 0.25 ? 4*x : (x < 0.75 ? 2 - 4*x : 4*x - 4);
      case 2: { const y = x + 0.5; return 2*(y - Math.floor(y)) - 1; }
      case 4: { const y = x + 0.5; return 1 - 2*(y - Math.floor(y)); }   // reversed (ramp-down) saw
      case 6: {
        // sine -> saw: sum of r^n sin(n th)/n in closed form. r = 0 is the sine, r -> 1 the saw.
        // r is capped per member so the harmonics die out before Nyquist (self band-limiting).
        const r = RazorCore.mr || 0;
        if (r < 1e-4) return Math.sin(6.283185307179586*x);
        const th = 6.283185307179586*(x + 0.5);
        return -Math.atan2(r*Math.sin(th), 1 - r*Math.cos(th))/RazorCore.mn;
      }
      default: return x < 0.5 ? 1 : -1;
    }
  }
  // modulator shapes; x is an unbounded phase so noise keeps moving in free mode
  static mod(shape, x, seed){
    if (shape === 7) return RazorCore.hash(Math.floor(x) + seed);   // sample-and-hold noise: a new random value each mod cycle
    if (shape === 5){
      const i = Math.floor(x), f = x - i, sm = f*f*(3 - 2*f);
      const a = RazorCore.hash(i + seed), b = RazorCore.hash(i + 1 + seed);
      return a + (b - a)*sm;
    }
    return RazorCore.wave(shape, x - Math.floor(x));
  }
  // periodic antiderivative of each base shape (all are zero-mean, so the integral over any interval is F(b) − F(a))
  static F(sh, x){
    switch(sh){
      case 0: return (1 - Math.cos(6.283185307179586*x))*0.15915494309189535;
      case 1: { const f = x - Math.floor(x); return f < 0.25 ? 2*f*f : f < 0.75 ? 2*f - 2*f*f - 0.25 : 2*f*f - 4*f + 2; }
      case 2: { let y = x + 0.5; y -= Math.floor(y); return y*y - y; }
      case 4: { let y = x + 0.5; y -= Math.floor(y); return y - y*y; }
      default: { const f = x - Math.floor(x); return f < 0.5 ? f : 1 - f; }
    }
  }
  static crushAvg(sh, st, kk, j){ return (RazorCore.F(sh, st + (j + 1)/kk) - RazorCore.F(sh, st + j/kk))*kk; }
  // held level at hold-phase hp; with slew, each step glides in from the previous level over its first `sl` fraction
  static crushLevel(sh, st, kk, hp, sl){
    if (hp < 0) return RazorCore.wave(sh, st - Math.floor(st));
    const j = Math.floor(hp), f = hp - j, a = RazorCore.crushAvg(sh, st, kk, j);
    if (sl > 0.001 && f < sl){
      const prev = j === 0 ? RazorCore.wave(sh, st - Math.floor(st)) : RazorCore.crushAvg(sh, st, kk, j - 1);
      return prev + (a - prev)*f/sl;
    }
    return a;
  }
  static isFM(p){ return p.mode === 1 || p.mode === 2; }
  // 'balanced' stereo slots per swarm size: the evenly spaced pan positions, permuted so pan is uncorrelated with
  // member order (and so with detune, cut-rate, width... gradients). Found by exhaustive search; 2 and 3 members
  // can't be fully balanced, 4–9 are exact.
  static panSlot(N, i){ return RazorCore.PANS[N - 1][i]; }
  // glide a wrapped offset back to 0 by the shortest way round (used when rotation is switched off)
  static home(x, k){ let d = x - Math.round(x); d -= d*k; return Math.abs(d) < 1e-6 ? 0 : d; }
  // per-member offsets under the chosen spread law: 0 gradient, 1 random per note, 2 alternate, 3 swarm-linked
  // cut-rate rules: the member-by-member ratio list a rule defines for N members (member 0 is the root, at k)
  static parseRatios(str){
    const out = [];
    for (const tok of String(str).split(/[\s,;]+/)){
      if (!tok) continue;
      const q = tok.split('/'), v = q.length === 2 ? parseFloat(q[0])/parseFloat(q[1]) : parseFloat(tok);
      if (isFinite(v) && v > 0) out.push(v);
    }
    return out.length ? out : [1];
  }
  static ruleList(rule, N, custom){
    const c = RazorCore._rlc || (RazorCore._rlc = {n:0});
    const key = rule + '|' + N + '|' + (rule === 9 ? custom : '');
    if (c[key]) return c[key];
    if (++c.n > 32){ RazorCore._rlc = {n:1}; }
    const PR = [2, 3, 5, 7, 11, 13, 17, 19, 23], CU = rule === 9 ? RazorCore.parseRatios(custom) : null;
    const chord = (set, j) => set[j % set.length]*Math.pow(2, Math.floor(j/set.length));
    const L = [];
    for (let j = 0; j < N; j++){
      switch (rule){
        case 1: L.push(j + 1); break;                              // harmonic series
        case 2: L.push(1/(j + 1)); break;                          // undertone series
        case 3: L.push(Math.pow(2, j)); break;                     // octaves
        case 4: L.push(chord([1, 5/4, 3/2], j)); break;            // just major triad, stacked
        case 5: L.push(chord([1, 6/5, 3/2], j)); break;            // just minor triad, stacked
        case 6: L.push(Math.pow(1.5, j)); break;                   // stacked fifths
        case 7: L.push(Math.pow(1.6180339887, j)); break;          // golden ratio: maximally inharmonic
        case 8: L.push(PR[j]/2); break;                            // primes over 2
        default: L.push(CU ? CU[j % CU.length]*Math.pow(2, Math.floor(j/CU.length)) : 1);  // custom, repeating by octaves
      }
    }
    RazorCore._rlc[key] = L;
    return L;
  }
  static spreadMember(m, i, N, s, law, fi, nyq){
    const g = N > 1 ? i/(N - 1) - 0.5 : 0, alt = N > 1 ? (i % 2 ? 0.5 : -0.5) : 0;
    const pn = j => N < 2 ? 0 : law === 0 ? g : (law === 1 || law === 4) ? m.rv[j] : law === 2 ? alt : m.lead;
    m.cOff = s.bspread*pn(0);
    const ks = RazorCore.kSpread(s.kRule, s.kRuleAmt, s.kspread, s.kq, s.kCustom, N, pn(1));
    const ko = ks[0];
    m.kAdd = ko; m.kMul = ks[1];
    m.rotOff = s.rotSpread*2*pn(2);
    m.rotOff2 = s.b2sp ? s.rotSpread2*2*pn(11) : m.rotOff;
    m.wMul = Math.pow(2, s.wspread*4*pn(3));
    m.dAdd = s.dspread*2*pn(4);
    m.iMul = Math.pow(2, s.ispread*4*pn(5));
    m.mor = Math.min(1, Math.max(0, s.morph + s.mspread*2*pn(6)));
    const ki = Math.min((s.lock === 2 ? Math.max(0.05, s.kHz/Math.max(1, fi) + ko) : Math.max(0.25, s.k + ko))*m.kMul, 0.9*nyq/Math.max(1, fi));
    m.kEff = ki;
    const wi = Math.min(1, s.w*m.wMul);
    const fh = (s.lock === 1 ? ki/Math.max(wi, 1e-3) : ki)*fi;
    const rmax = Math.min(0.995, Math.pow(0.01, fh/nyq));
    const r = 1 - Math.pow(1 - rmax, m.mor);
    m.mr = r; m.mn = r > 1e-4 ? Math.asin(r) : 1;
    // blade 2: follows blade 1's spreads, or runs its own amounts on independent random slots (7..10)
    if (!s.b2sp){ m.cOff2 = m.cOff; m.kAdd2 = m.kAdd; m.kMul2 = m.kMul; m.wMul2 = m.wMul; m.dAdd2 = m.dAdd; m.iMul2 = m.iMul; }
    else {
      m.iMul2 = Math.pow(2, s.ispread2*4*pn(13));
      m.cOff2 = s.bspread2*pn(7);
      const k2 = RazorCore.kSpread(s.kRule2, s.kRuleAmt2, s.kspread2, s.kq, s.kCustom, N, pn(8));
      m.kAdd2 = k2[0]; m.kMul2 = k2[1];
      m.wMul2 = Math.pow(2, s.wspread2*4*pn(9));
      m.dAdd2 = s.dspread2*2*pn(10);
    }
    {
      // blade 2's sine->saw shape, capped from blade 2's own carrier frequency (it used to borrow blade 1's)
      const mor2 = Math.min(1, Math.max(0, s.morph2 + (s.b2sp ? s.mspread2*2*pn(12) : s.mspread*2*pn(6))));
      const lock2 = s.lock2 < 0 ? s.lock : s.lock2;
      const k2 = Math.min((lock2 === 2 ? Math.max(0.05, s.kHz2/Math.max(1, fi) + m.kAdd2) : Math.max(0.25, s.k2 + m.kAdd2))*m.kMul2, 0.9*nyq/Math.max(1, fi));
      const w2 = Math.min(1, s.w2*m.wMul2);
      const fh2 = (lock2 === 1 ? k2/Math.max(w2, 1e-3) : k2)*fi;
      const rmax2 = Math.min(0.995, Math.pow(0.01, fh2/nyq)), r2 = 1 - Math.pow(1 - rmax2, mor2);
      m.mr2 = r2; m.mn2 = r2 > 1e-4 ? Math.asin(r2) : 1;
    }
  }
  static kSpread(rule, amt, spread, q, custom, N, pv){
    const out = RazorCore._ks || (RazorCore._ks = [0, 1]);
    out[0] = 0; out[1] = 1;
    if (!rule){
      let ko = (q ? Math.round(spread) : spread)*pv;
      if (q) ko = Math.sign(ko)*Math.round(Math.abs(ko));
      out[0] = ko;
    } else if (N > 1){
      // a rule defines one ratio per slot; the spread law picks each member's slot (snapped = exact slots only)
      const L = RazorCore.ruleList(rule, N, custom);
      const x = Math.min(N - 1, Math.max(0, (pv + 0.5)*(N - 1)));
      let r;
      if (q) r = L[Math.round(x)];
      else { const a = Math.floor(x), b = Math.min(N - 1, a + 1), f = x - a; r = Math.exp(Math.log(L[a])*(1 - f) + Math.log(L[b])*f); }
      out[1] = Math.pow(r, amt);
    }
    return out;
  }
  // pitch-FM integrator: advances ns.acc (cycles of carrier phase); returns the unwrapped increment
  static fmStep(p, ns, e, entered, kk, modX, dphi){
    if (p.fmType !== 1 || !RazorCore.isFM(p)) return 0;
    if (entered && p.mode === 1) ns.acc = 0;
    const mv = RazorCore.mod(p.mshape, p.mode === 1 ? p.mEff*e : modX, ns.seed);
    // exponential pitch deviation: ±(1 + I/10) as a frequency ratio at full modulator swing (symmetric in cents)
    const d = (Math.pow(1 + 0.1*p.I, mv) - 1)*kk*dphi;
    ns.acc += d; ns.acc -= Math.floor(ns.acc);
    return d;
  }
  static voice(p, phi, c, k, modX, ns){
    const base = RazorCore.wave(p.base, phi);
    const w = p.w;
    if (w < 0.004){ ns.g = 0; ns.inside = false; return base; }
    let st = c - w*0.5; st -= Math.floor(st);
    let e = phi - st; if (e < 0) e += 1;
    if (e >= w){ ns.g = 0; ns.inside = false; return base; }
    const kk = p.lock === 1 ? k / w : k;
    // reflect: the second half of the blade replays the first half backwards (a palindrome)
    const er = p.mirror === 1 && e > w*0.5 ? w - e : e;
    const hp = kk * er;
    let hot, x;
    switch (p.mode){
      case 0: { const cp = hp + ns.xin; hot = RazorCore.wave(p.hot, cp - Math.floor(cp)); break; }
      case 1: case 2: {
        const cp = ns.xin + (p.fmType === 1 ? hp + ns.acc
          : hp + p.I*0.15915494309189535*RazorCore.mod(p.mshape, p.mode === 1 ? p.mEff*er : modX, ns.seed));
        hot = RazorCore.wave(p.hot, cp - Math.floor(cp)); break;
      }
      case 3: {
        const idx = Math.floor(hp);
        if (!ns.inside || idx !== ns.idx){ ns.idx = idx; ns.val = ns.rnd(idx); }
        hot = ns.val; break;
      }
      case 4: hot = Math.sin(1.5707963267948966*(1 + (k - 1)*0.25)*base); break;          // fold
      case 5: { const cp = hp + ns.xin; hot = base*RazorCore.wave(p.hot, cp - Math.floor(cp)); break; }  // ring
      case 6: {                                                                           // crush
        // each hold level is the AVERAGE of the base over its hold interval (box-filtered decimation), so levels move
        // continuously as k changes instead of flipping when a sample point slides across the base wave's jump
        const wEff = p.mirror === 1 ? w*0.5 : w, hpEnd = kk*wEff, sl = p.hard;
        hot = RazorCore.crushLevel(p.base, st, kk, hp, sl);
        if (sl > 0.001){
          // land on the base wave at the blade exit, over the last `slew` hold intervals (independent of step layout)
          const rlE = Math.min(sl, hpEnd), h0 = hpEnd - rlE;
          if (rlE > 1e-9 && hp > h0){
            const from = RazorCore.crushLevel(p.base, st, kk, h0, sl);
            x = st + wEff; const tgt = RazorCore.wave(p.base, x - Math.floor(x));
            hot = from + (tgt - from)*(hp - h0)/rlE;
          }
        }
        ns.inside = true; ns.g = 1;
        return base + p.depth*(hot - base);
      }
      default: hot = base;
    }
    ns.inside = true;
    const t = p.hard * w * 0.5;
    let g = 1;
    if (t > 1e-9){
      if (e < t) g = 0.5 - 0.5*Math.cos(Math.PI*e/t);
      else if (e > w - t) g = 0.5 - 0.5*Math.cos(Math.PI*(w - e)/t);
    }
    ns.g = g;
    return base + g*p.depth*(hot - base);
  }
  // full output: the blade, plus an optional twin half a cycle later (inverted or not)
  static out(p, phi, c, k, modX, ns, ns2, bx){
    let y = RazorCore.voice(p, phi, c, k, modX, ns);
    let ph2 = -1;
    if (p.mirror >= 2){
      ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1;
      ns2.acc = ns.acc; ns2.xin = ns.xin;
      const d2 = RazorCore.voice(p, ph2, c, k, modX, ns2) - RazorCore.wave(p.base, ph2);
      y += p.mirror === 2 ? -d2 : d2;
    }
    if (bx){
      // second blade: its own parameter view (bx.g), centre, rate and state; contributions add on the same base
      const g = bx.g, mr0 = RazorCore.mr, mn0 = RazorCore.mn;
      RazorCore.mr = bx.mr; RazorCore.mn = bx.mn;
      y += RazorCore.voice(g, phi, bx.c, bx.k, bx.modX, bx.ns3) - RazorCore.wave(g.base, phi);
      if (g.mirror >= 2){
        if (ph2 < 0){ ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1; }
        bx.ns4.acc = bx.ns3.acc; bx.ns4.xin = bx.ns3.xin;
        const d4 = RazorCore.voice(g, ph2, bx.c, bx.k, bx.modX, bx.ns4) - RazorCore.wave(g.base, ph2);
        y += g.mirror === 2 ? -d4 : d4;
      }
      RazorCore.mr = mr0; RazorCore.mn = mn0;
    }
    return y;
  }
  // blade-2 view: every field voice() reads, so it can be passed in place of the main params
  static g2(){ return {mode:4,hot:2,base:0,w:.2,depth:1,hard:0,lock:0,mirror:0,fmType:0,I:2,mshape:0,mEff:3.37,m:3.37}; }
  // blade 2's settings: units and mirror can follow blade 1 (-1) or be its own; FM follows blade 1 unless b2fm
  static fillG2(g, s, w2e, dep2, I2e, mEff2){
    g.mode = s.mode2; g.hot = s.hot2; g.base = s.base; g.w = w2e; g.depth = dep2; g.hard = s.hard2;
    g.lock = s.lock2 < 0 ? s.lock : s.lock2; g.mirror = s.mirror2 < 0 ? s.mirror : s.mirror2;
    g.fmType = s.b2fm ? s.fmType2 : s.fmType; g.mshape = s.b2fm ? s.mshape2 : s.mshape;
    g.I = I2e; g.mEff = mEff2; g.m = s.b2fm ? s.m2 : s.m;
  }
  constructor(sr){
    this.sr = sr; this.age = 0; this.cnt = 0; this.vc = 0;
    this.t = {I2:2,m2:3.37,mHz2:660,morph2:.5,mspread2:0,ispread2:0,rotRate2:0,rotSpread2:0,kRuleAmt2:1,bspread2:0,kspread2:0,wspread2:0,dspread2:0,benvA2:2,benvD2:250,benvK2:0,benvW2:0,benvVel2:.5,kRuleAmt:1,w2:.2,k2:3,kHz2:800,c2:.35,depth2:1,hard2:0,xm:0,fb:0,benvA:2,benvD:250,benvK:0,benvW:0,benvVel:.5,glide:60,driftRate:.5,morph:.5,wspread:0,dspread:0,mspread:0,ispread:0,w:.25,k:6,kHz:1320,mHz:660,c:.875,rotRate:0,rotSpread:0,hard:0,depth:1,I:2,m:3.37,gain:.35,
      detune:14,K:.35,bspread:0,kspread:0,width:.7,A:4,D:400,S:.85,R:280,bend:0};
    // every field declared up front so the hot object keeps one fast shape
    this.s = {lock2:-1,mirror2:-1,b2fm:0,fmType2:0,mshape2:0,mUnit2:0,I2:2,m2:3.37,mHz2:660,morph2:.5,mspread2:0,ispread2:0,rotRate2:0,rotSpread2:0,b2sp:0,kRule2:0,kRuleAmt2:1,bspread2:0,kspread2:0,wspread2:0,dspread2:0,benvA2:2,benvD2:250,benvK2:0,benvW2:0,benvVel2:.5,kRule:0,kCustom:'1, 5/4, 3/2',kRuleAmt:1,mode2:4,hot2:2,b2on:0,w2:.2,k2:3,kHz2:800,c2:.35,depth2:1,hard2:0,xm:0,fb:0,benvA:2,benvD:250,benvK:0,benvW:0,benvVel:.5,aa:1,glide:60,driftRate:.5,morph:.5,wspread:0,dspread:0,mspread:0,ispread:0,kq:0,law:0,w:.25,k:6,kHz:1320,mHz:660,c:.875,rotRate:0,rotSpread:0,hard:0,depth:1,I:2,m:3.37,gain:.35,
      detune:14,K:.35,bspread:0,kspread:0,width:.7,A:4,D:400,S:.85,R:280,bend:0,
      mode:0,hot:2,base:0,lock:0,mshape:0,fmType:0,mirror:0,mEff:3.37};
    this.keys = Object.keys(this.t).filter(k => !['k','kHz','w','c','depth','I','hard','w2','k2','kHz2','c2'].includes(k));
    this.sm = 0;
    this.d = {mode:0,hot:2,base:0,lock:0,N:5,phaseMode:2,poly:6,mshape:0,kq:0,fmType:0,mUnit:0,mirror:0,dcMode:2,law:0,frame:0,frame2:-1,rot2Follow:1,lock2:-1,mirror2:-1,b2fm:0,fmType2:0,mshape2:0,mUnit2:0,cScale:0,rotSync:1,panOrder:0,aa:1,polyMode:0,glideAlways:0,b2on:0,mode2:4,hot2:2,kRule:0,kCustom:'1, 5/4, 3/2',b2sp:0,kRule2:0,b2env:0};
    this.voices = [];
    for (let i = 0; i < 8; i++) this.voices.push(this.newVoice());
    this.Rx = new Float64Array(8); this.Ry = new Float64Array(8);
    this.gl = new Float64Array(9); this.gr = new Float64Array(9);
    this.sc = {idx:0,val:0,inside:true,g:0,seed:0,acc:0,xin:0,rnd:function(){ return this.val; }};
    this.sc2 = {idx:0,val:0,inside:true,g:0,seed:0,acc:0,xin:0,rnd:function(){ return this.val; }};
    this.bxs = {g:null,c:0,k:1,modX:0,mr:0,mn:1,ns3:{idx:0,val:0,inside:true,g:0,seed:0,acc:0,xin:0,rnd:function(){ return this.val; }},
      ns4:{idx:0,val:0,inside:true,g:0,seed:0,acc:0,xin:0,rnd:function(){ return this.val; }}};
    this.dcCnt = 0; this.hx = [0, 0]; this.hy = [0, 0]; this.gRot = 0; this.gRot2 = 0;
    this.stack = []; this.nf = {};
    this._cc = 0; this._cp = 0;
    this.post = null;
    this.os = 0; this.setOS(2);
  }
  newVoice(){
    const ms = [];
    for (let i = 0; i < 9; i++) ms.push({phi:0,modX:0,inc:0,prev:0,dc:0,dcS:0,dcInit:true,
      rv:new Float64Array(14),rvT:new Float64Array(14),dcWait:0,lead:0,kMul:1,rot2:0,rotOff2:0,iMul2:1,mr2:0,mn2:1,cOff2:0,kAdd2:0,kMul2:1,wMul2:1,dAdd2:0,rot:0,cOff:0,kAdd:0,rotOff:0,wMul:1,dAdd:0,iMul:1,mor:0,kEff:6,mr:0,mn:1,
      ns:{idx:0,val:0,inside:false,g:0,seed:i*97,acc:0,xin:0,rnd:RazorCore.rnd},
      ns2:{idx:0,val:0,inside:false,g:0,seed:i*97 + 7,acc:0,xin:0,rnd:RazorCore.rnd},
      y1:0,y2:0,
      bx:{g:RazorCore.g2(),c:0,k:3,modX:0,mr:0,mn:1,ns3:{idx:0,val:0,inside:false,g:0,seed:i*97 + 13,acc:0,xin:0,rnd:RazorCore.rnd},ns4:{idx:0,val:0,inside:false,g:0,seed:i*97 + 19,acc:0,xin:0,rnd:RazorCore.rnd}}});
    return {active:false,note:null,freq:110,freqT:110,fc:110,vel:1,env:0,stage:0,gate:false,age:0,r:1,rot:0,m:ms,
      rot2:0,be:0,bst:0,bv:1,kE:1,wE:1,be2:0,bst2:0,bv2:1,kE2:1,wE2:1,gr:1,xb:new Float64Array(9)};
  }
  msg(o){
    if (o.t === 'p') this.set(o);
    else if (o.t === 'on') this.noteOn(o.note, o.freq, o.vel);
    else if (o.t === 'off') this.noteOff(o.note);
    else if (o.t === 're'){ for (const v of this.voices) if (v.active && v.note === o.note) v.freq = o.freq; }
    else if (o.t === 'panic'){ for (const v of this.voices){ v.gate = false; v.stage = 4; } }
  }
  set(m){
    if ('polyMode' in m && m.polyMode !== this.d.polyMode){
      for (const v of this.voices){ v.gate = false; v.stage = 4; }
      this.stack = [];
    }
    for (const k in m){
      if (k === 't') continue;
      if (k === 'os'){ if (m.os !== this.os) this.setOS(m.os); }
      else if (k in this.t) this.t[k] = m[k];
      else if (k in this.d) this.d[k] = m[k];
    }
  }
  noteOn(note, freq, vel){
    const d = this.d;
    if (d.polyMode) return this.monoOn(note, freq, vel);
    const pool = this.voices.slice(0, d.poly);
    let v = pool.find(x => x.active && x.note === note);
    const fresh = !v;
    if (!v) v = pool.find(x => !x.active);
    if (!v){ v = pool[0]; for (const x of pool) if (x.age < v.age) v = x; }
    this.startVoice(v, note, freq, vel, fresh, true);
    v.freq = v.freqT = freq;
    this.couple(v, this.s, this.d);
    this.spread(v);
  }
  // mono: one voice, a note stack with last-note priority. 'mono' retriggers the envelope on every new note,
  // 'legato' only when no other key is held. Glide slides pitch between overlapping notes (or always, if set).
  monoOn(note, freq, vel){
    const d = this.d, s = this.s, v = this.voices[0];
    this.stack = this.stack.filter(n => n !== note); this.stack.push(note); this.nf[note] = freq;
    const held = v.active && v.gate, fresh = !v.active;
    const retrig = !held || d.polyMode === 1;
    const glide = !fresh && (held || d.glideAlways) && s.glide > 1;
    this.startVoice(v, note, freq, vel, fresh, retrig);
    v.freqT = freq; if (!glide) v.freq = freq;
    this.couple(v, s, d);
    this.spread(v);
  }
  startVoice(v, note, freq, vel, fresh, retrig){
    const d = this.d;
    if (fresh){
      if (!v.active) v.env = 0;
      for (const m of v.m){
        m.phi = d.phaseMode === 1 ? 0 : Math.random(); m.modX = Math.random()*1000; m.bx.modX = m.modX + 317; m.prev = 0;
        m.ns.inside = false; m.ns.acc = 0; m.ns2.inside = false; m.inc = freq;
        m.dc = 0; m.dcS = 0; m.dcInit = true; m.y1 = 0; m.y2 = 0; m.bx.ns3.inside = false; m.bx.ns3.acc = 0; m.bx.ns4.inside = false;
      }
    }
    if (fresh) v.freq = v.freqT = freq;
    v.note = note; v.vel = vel; v.gate = true; v.active = true; v.age = ++this.age;
    if (retrig){
      v.stage = 1; v.be = 0; v.bst = 1; v.bv = 1 - this.s.benvVel + this.s.benvVel*vel;
      v.be2 = 0; v.bst2 = 1; v.bv2 = 1 - this.s.benvVel2 + this.s.benvVel2*vel;
    }
    if (!retrig) return;
    // 'note' rotation: every note starts with its blade exactly at Position
    if (d.rotSync || fresh){ v.rot = 0; v.rot2 = 0; for (const m of v.m){ m.rot = 0; m.rot2 = 0; } }
    // random law: every note-on (retriggers included) rolls a new distribution; a fresh voice jumps straight to it,
    // a retriggered one glides there so a sounding note doesn't click
    for (const m of v.m) for (let j = 0; j < 14; j++){ m.rvT[j] = Math.random() - 0.5; if (fresh) m.rv[j] = m.rvT[j]; }
    if (fresh && d.phaseMode === 2 && d.N > 1){ v.freq = freq; this.settle(v); }
    this.dcCnt = 0;
  }
  noteOff(note){
    const d = this.d;
    if (d.polyMode){
      this.stack = this.stack.filter(n => n !== note);
      const v = this.voices[0];
      if (!v.active || !v.gate || v.note !== note) return;
      if (this.stack.length){
        // fall back to the most recent key still held, gliding there without a retrigger
        const n = this.stack[this.stack.length - 1];
        v.note = n; v.freqT = this.nf[n];
        if (this.s.glide <= 1) v.freq = v.freqT;
      } else { v.gate = false; v.stage = 4; }
      return;
    }
    for (const v of this.voices) if (v.active && v.note === note && v.gate){ v.gate = false; v.stage = 4; }
  }
  keff(v, s){
    const f = v.freq*Math.pow(2, s.bend/12);
    const maxDev = 6.283185307179586*f*(Math.pow(2, s.detune/1200) - 1);
    // 'cycles' scales the whole coupling with pitch, so locked phase lags (and the summed shape) match on every note
    const floor = 6.283185307179586*3*(this.d.cScale ? f/110 : 1);
    return s.K*(1.5*maxDev + floor);
  }
  // fast-forward the swarm to its steady state so a new note starts where a held note would be
  settle(v){
    const s = this.s, d = this.d, N = d.N, Ke = Math.abs(this.keff(v, s));
    if (Ke < 1e-3) return;
    const dt = Math.min(0.01, 0.15/Ke), f = v.freq*Math.pow(2, s.bend/12);
    if (s.K > 0) for (let i = 0; i < N; i++) v.m[i].phi = Math.random()*0.15;
    for (let it = 0; it < 150; it++){
      this.couple(v, s, d);
      for (let i = 0; i < N; i++){ const m = v.m[i]; m.phi += (m.inc - f)*dt; m.phi -= Math.floor(m.phi); }
    }
  }
  couple(v, s, d){
    const TAU = 6.283185307179586, N = d.N;
    const f = v.freq*Math.pow(2, s.bend/12);
    v.fc = v.freq;
    const Keff = this.keff(v, s);
    const H = N < 2 ? 1 : (s.K < 0 ? Math.min(N - 1, 6) : 1);
    const Rx = this.Rx, Ry = this.Ry;
    for (let h = 1; h <= H; h++){
      let sx = 0, sy = 0;
      for (let i = 0; i < N; i++){ const a = TAU*h*v.m[i].phi; sx += Math.cos(a); sy += Math.sin(a); }
      Rx[h] = sx/N; Ry[h] = sy/N;
    }
    v.r = Math.hypot(Rx[1], Ry[1]);
    if (N > 1){
      // each member's signed phase lead on the mean field, in cycles (−½…½): locking shrinks it, splay maximizes it
      const psi = Math.atan2(Ry[1], Rx[1])/TAU;
      for (let i = 0; i < N; i++){ let l = v.m[i].phi - psi + 0.5; l -= Math.floor(l); v.m[i].lead = l - 0.5; }
    } else v.m[0].lead = 0;
    for (let i = 0; i < N; i++){
      const cents = N > 1 ? s.detune*(2*i/(N - 1) - 1) : 0;
      const fi = f*Math.pow(2, cents/1200);
      let coup = 0;
      if (N > 1) for (let h = 1; h <= H; h++){
        const a = TAU*h*v.m[i].phi;
        coup += (Ry[h]*Math.cos(a) - Rx[h]*Math.sin(a))/h;
      }
      v.m[i].inc = Math.max(0, fi + Keff*coup/TAU);
    }
  }
  spread(v){
    const s = this.s, d = this.d, N = d.N, nyq = this.sr*this.os*0.5, dt = 32/this.sr;
    s.kq = d.kq; s.lock = d.lock; s.kRule = d.kRule; s.kCustom = d.kCustom; s.b2sp = d.b2sp; s.kRule2 = d.kRule2;
    s.lock2 = d.lock2; s.mirror2 = d.mirror2;
    if (d.law === 1){
      const a = 1 - Math.exp(-dt/0.02);
      for (let i = 0; i < N; i++){ const m = v.m[i]; for (let j = 0; j < 14; j++) m.rv[j] += (m.rvT[j] - m.rv[j])*a; }
    } else if (d.law === 4){
      // drift: an Ornstein-Uhlenbeck walk per member and per parameter, same spread as the random law
      const th = 6.283185307179586*s.driftRate, sig = 0.289*Math.sqrt(2*th*dt), dec = Math.exp(-th*dt);
      for (let i = 0; i < N; i++){
        const m = v.m[i];
        for (let j = 0; j < 14; j++){
          const u = Math.max(1e-12, Math.random()), z = Math.sqrt(-2*Math.log(u))*Math.cos(6.283185307179586*Math.random());
          let x = m.rv[j]*dec + sig*z;
          m.rv[j] = x > 0.75 ? 0.75 : x < -0.75 ? -0.75 : x;
        }
      }
    }
    for (let i = 0; i < N; i++) RazorCore.spreadMember(v.m[i], i, N, s, d.law, v.m[i].inc, nyq);
  }
  mk(fs, fc, Q){
    const w0 = 6.283185307179586*fc/fs, cs = Math.cos(w0), al = Math.sin(w0)/(2*Q);
    const a0 = 1 + al, b0 = (1 - cs)/2;
    return {b0:b0/a0, b1:(1 - cs)/a0, b2:b0/a0, a1:(-2*cs)/a0, a2:(1 - al)/a0, z1:0, z2:0};
  }
  setOS(n){
    this.os = n;
    const fs = this.sr*n, fc = 0.45*this.sr;
    this.bqL = [this.mk(fs,fc,0.5412), this.mk(fs,fc,1.3066)];
    this.bqR = [this.mk(fs,fc,0.5412), this.mk(fs,fc,1.3066)];
  }
  bqf(f, x){
    const y = f.b0*x + f.z1;
    f.z1 = f.b1*x - f.a1*y + f.z2;
    f.z2 = f.b2*x - f.a2*y;
    return y;
  }
  hAt(m, E, c, k, s){
    const sc = this.sc, sc2 = this.sc2, F = RazorCore.frac;
    sc.seed = m.ns.seed; sc.acc = m.ns.acc; sc2.seed = m.ns2.seed;
    sc.idx = m.ns.idx; sc.val = m.ns.val; sc.inside = true;
    sc2.idx = m.ns2.idx; sc2.val = m.ns2.val; sc2.inside = true;
    sc.xin = m.ns.xin; sc2.xin = m.ns.xin;
    let bx = null;
    if (s.b2on){
      const src = m.bx, b = this.bxs; bx = b;
      b.g = src.g; b.c = src.c; b.k = src.k; b.modX = src.modX; b.mr = src.mr; b.mn = src.mn;
      b.ns3.seed = src.ns3.seed; b.ns3.acc = src.ns3.acc; b.ns3.idx = src.ns3.idx; b.ns3.val = src.ns3.val; b.ns3.inside = true; b.ns3.xin = src.ns3.xin;
      b.ns4.seed = src.ns4.seed; b.ns4.idx = src.ns4.idx; b.ns4.val = src.ns4.val; b.ns4.inside = true;
    }
    const a = RazorCore.out(s, F(E + 1e-7), c, k, m.modX, sc, sc2, bx);
    sc.idx = m.ns.idx; sc.inside = true; sc2.idx = m.ns2.idx; sc2.inside = true;
    if (bx){ bx.ns3.idx = m.bx.ns3.idx; bx.ns3.inside = true; bx.ns4.idx = m.bx.ns4.idx; bx.ns4.inside = true; }
    const b = RazorCore.out(s, F(E - 1e-7), c, k, m.modX, sc, sc2, bx);
    return a - b;
  }
  // mean of the blade's contribution over one cycle, for per-cycle DC correction
  dcEst(ns, modX, c, k, s){
    const w = s.w;
    this._dcJ = 0;
    if (w < 0.004) return 0;   // noise blades: estimated with the noise at its mean (zero), leaving the base it replaces
    const fac = s.mirror === 2 ? 0 : s.mirror === 3 ? 2 : 1;
    if (!fac) return 0;
    let st = c - w*0.5; st -= Math.floor(st);
    const kk = s.lock === 1 ? k/w : k, F = RazorCore.F;
    // sync blade of a closed-form wave: the hard-edged integral is exact; soft edges subtract what the
    // tapers remove, integrated numerically over the (short) taper regions only
    if (s.mode === 0 && s.hot !== 6){
      const hotInt = s.mirror === 1 ? 2*(F(s.hot, kk*w*0.5) - F(s.hot, 0))/kk : (F(s.hot, kk*w) - F(s.hot, 0))/kk;
      let total = hotInt - (F(s.base, st + w) - F(s.base, st));
      const t = s.hard*w*0.5;
      if (t > 1e-9){
        const Jt = Math.min(512, Math.max(32, Math.ceil(kk*t*32)));
        this._dcJ = 2*Jt;
        const sc = this.sc; sc.inside = true;
        let cut = 0;
        for (let side = 0; side < 2; side++) for (let j = 0; j < Jt; j++){
          const e = side === 0 ? (j + 0.5)/Jt*t : w - (j + 0.5)/Jt*t;
          const er = s.mirror === 1 && e > w*0.5 ? w - e : e;
          let hp = kk*er; hp -= Math.floor(hp);
          let phi = st + e; phi -= Math.floor(phi);
          const g = 0.5 - 0.5*Math.cos(Math.PI*(side === 0 ? e : w - e)/t);
          cut += (1 - g)*(RazorCore.wave(s.hot, hp) - RazorCore.wave(s.base, phi));
        }
        total -= cut*t/Jt;
      }
      return fac*s.depth*total;
    }
    // numeric: enough points to resolve every carrier cycle, crush step and modulator cycle, so the
    // sample grid can never lock onto the blade's own periodicity (that was the erratic-DC bug)
    let feats = kk*w;
    if (RazorCore.isFM(s)) feats *= 1 + 0.5*s.I;
    feats += s.mEff*w;
    const J = Math.min(1024, Math.max(48, Math.ceil(feats*32)));
    this._dcJ = J;
    const sc = this.sc;
    sc.seed = ns.seed; sc.acc = ns.acc; sc.idx = ns.idx; sc.val = s.mode === 3 ? 0 : ns.val; sc.xin = ns.xin;
    let sum = 0;
    for (let j = 0; j < J; j++){
      let phi = st + (j + 0.5)/J*w; phi -= Math.floor(phi);
      sc.inside = j > 0;
      sum += RazorCore.voice(s, phi, c, k, modX, sc) - RazorCore.wave(s.base, phi);
    }
    return fac*w*sum/J;
  }
  // PolyBLEP for discontinuities inside one blade (primary or twin), tracked in carrier phase
  scan(m, st, p0, dphi, c, k, s, dAcc, modX0, g, kB, nsB, modX1){
    const w = g.w, ns = nsB;
    let off = -1, step = 1;
    const carrier = g.mode === 0 || g.mode === 5 || RazorCore.isFM(g);
    if (carrier && (g.hot === 2 || g.hot === 4)){ off = 0.5; step = 1; }
    else if (carrier && g.hot === 3){ off = 0; step = 0.5; }
    else if (g.mode === 6 && g.hard <= 0.001){ off = 0; step = 1; }
    if (off < 0) return;
    let e0 = p0 - st; e0 -= Math.floor(e0);
    if (e0 >= w) return;
    const kk = g.lock === 1 ? kB/w : kB;
    const eEnd = Math.min(e0 + dphi, w), tmax = (eEnd - e0)/dphi;
    const hw = w*0.5, refl = g.mirror === 1;
    const r0 = refl && e0 > hw ? w - e0 : e0, r1 = refl && eEnd > hw ? w - eEnd : eEnd;
    let o0 = 0, o1 = 0;
    if (RazorCore.isFM(g)){
      if (g.fmType === 1){ o1 = ns.acc; o0 = o1 - dAcc; }
      else {
        const sc = 0.15915494309189535*g.I;
        o0 = sc*RazorCore.mod(g.mshape, g.mode === 1 ? g.mEff*r0 : modX0, ns.seed);
        o1 = sc*RazorCore.mod(g.mshape, g.mode === 1 ? g.mEff*r1 : modX1, ns.seed);
      }
    }
    const cp0 = kk*r0 + o0, cp1 = kk*r1 + o0 + (o1 - o0)*tmax;
    const lo = Math.min(cp0, cp1), hi = Math.max(cp0, cp1);
    if (!(hi - lo > 1e-12 && hi - lo < 8)) return;
    let j = Math.floor((lo - off)/step) + 1;
    for (let q = 0; q < 6; q++, j++){
      const pos = off + j*step; if (pos > hi) break;
      if (pos <= lo || pos === 0) continue;
      const tau = (pos - cp0)/(cp1 - cp0)*tmax;
      if (tau > 0 && tau <= 1) this.addE(m, p0 + tau*dphi, tau, c, k, s);
    }
  }
  addE(m, E, tau, c, k, s){
    const h = this.hAt(m, E, c, k, s);
    this._cc -= 0.5*h*tau*tau;
    this._cp += 0.5*h*(1 - tau)*(1 - tau);
  }
  tryE(m, E, p0, dphi, c, k, s){
    let dd = E - p0; dd -= Math.floor(dd);
    if (dd > 0 && dd <= dphi) this.addE(m, E, dd/dphi, c, k, s);
  }
  // one member, one (oversampled) tick, with PolyBLEP on every known discontinuity
  stepM(m, dphi, c, k, s){
    const w = s.w, p0 = m.phi, ns = m.ns;
    let p1 = p0 + dphi; p1 -= Math.floor(p1); m.phi = p1;
    const modX0 = m.modX;
    m.modX += s.mEff*dphi; if (m.modX > 65536) m.modX -= 65536;
    let st = c - w*0.5; st -= Math.floor(st);
    let e0 = p0 - st; e0 -= Math.floor(e0);
    let e1 = p1 - st; e1 -= Math.floor(e1);
    const on = w >= 0.004, kk = on ? (s.lock === 1 ? k/w : k) : k;
    const dAcc = on ? RazorCore.fmStep(s, ns, e1, e1 < e0, kk, m.modX, dphi) : 0;
    const bx = s.b2on ? m.bx : null;
    let dAcc2 = 0, st3 = 0, on2 = false, modX20 = 0;
    if (bx){
      // blade 2 shares blade 1's modulator unless it has FM of its own
      if (s.b2fm){ modX20 = bx.modX; bx.modX += bx.g.mEff*dphi; if (bx.modX > 65536) bx.modX -= 65536; }
      else { modX20 = modX0; bx.modX = m.modX; }
      const g = bx.g, w2 = g.w;
      on2 = w2 >= 0.004;
      if (on2){
        st3 = bx.c - w2*0.5; st3 -= Math.floor(st3);
        let a0 = p0 - st3; a0 -= Math.floor(a0); let a1 = p1 - st3; a1 -= Math.floor(a1);
        const mr0 = RazorCore.mr; RazorCore.mr = bx.mr;
        dAcc2 = RazorCore.fmStep(g, bx.ns3, a1, a1 < a0, g.lock === 1 ? bx.k/w2 : bx.k, bx.modX, dphi);
        RazorCore.mr = mr0;
      }
    }
    const x = RazorCore.out(s, p1, c, k, m.modX, ns, m.ns2, bx);
    this._cc = 0; this._cp = 0;
    if (s.aa && dphi > 0 && dphi < 0.5){
      const b = s.base;
      if (b === 2 || b === 4) this.tryE(m, 0.5, p0, dphi, c, k, s);
      else if (b === 3){ this.tryE(m, 0, p0, dphi, c, k, s); this.tryE(m, 0.5, p0, dphi, c, k, s); }
      if (on){
        this.tryE(m, st, p0, dphi, c, k, s);
        if (w < 0.9999) this.tryE(m, st + w, p0, dphi, c, k, s);
        this.scan(m, st, p0, dphi, c, k, s, dAcc, modX0, s, k, ns, m.modX);
        if (s.mirror >= 2){
          const st2 = st + 0.5;
          this.tryE(m, st2, p0, dphi, c, k, s);
          if (w < 0.9999) this.tryE(m, st2 + w, p0, dphi, c, k, s);
          this.scan(m, st2 - Math.floor(st2), p0, dphi, c, k, s, dAcc, modX0, s, k, ns, m.modX);
        }
      }
      if (on2){
        const g = bx.g, w2 = g.w;
        this.tryE(m, st3, p0, dphi, c, k, s);
        if (w2 < 0.9999) this.tryE(m, st3 + w2, p0, dphi, c, k, s);
        this.scan(m, st3, p0, dphi, c, k, s, dAcc2, modX20, g, bx.k, bx.ns3, bx.modX);
        if (g.mirror >= 2){
          const st4 = st3 + 0.5;
          this.tryE(m, st4, p0, dphi, c, k, s);
          if (w2 < 0.9999) this.tryE(m, st4 + w2, p0, dphi, c, k, s);
          this.scan(m, st4 - Math.floor(st4), p0, dphi, c, k, s, dAcc2, modX20, g, bx.k, bx.ns3, bx.modX);
        }
      }
    }
    const out = m.prev + this._cp;
    m.prev = x + this._cc;
    return out;
  }
  render(L, R){
    const n = L.length, os = this.os, sr = this.sr, s = this.s, t = this.t, d = this.d, keys = this.keys;
    const a = 1 - Math.exp(-16/(0.012*sr));
    const a1 = 1 - Math.exp(-1/(0.012*sr));
    s.mode = d.mode; s.hot = d.hot; s.base = d.base; s.lock = d.lock; s.mshape = d.mshape; s.fmType = d.fmType; s.mirror = d.mirror; s.aa = d.aa; s.mEff = s.m;
    s.b2on = d.b2on; s.mode2 = d.mode2; s.hot2 = d.hot2;
    s.lock2 = d.lock2; s.mirror2 = d.mirror2; s.b2fm = d.b2fm; s.fmType2 = d.fmType2; s.mshape2 = d.mshape2; s.mUnit2 = d.mUnit2;
    s.kq = d.kq; s.law = d.law; s.kRule = d.kRule; s.kCustom = d.kCustom; s.b2sp = d.b2sp; s.kRule2 = d.kRule2;
    const N = d.N, gl = this.gl, gr = this.gr;
    for (let i = 0; i < N; i++){
      const pn = N > 1 ? i/(N - 1) - 0.5 : 0;
      const pos = d.panOrder ? pn*2 : RazorCore.panSlot(N, i);
      const ang = (pos*s.width + 1)*Math.PI/4;
      gl[i] = Math.cos(ang); gr[i] = Math.sin(ang);
    }
    const norm = 1/Math.sqrt(N);
    const attInc = 1/Math.max(1, s.A*0.001*sr);
    const dC = 1 - Math.exp(-4/Math.max(1, s.D*0.001*sr));
    const rC = 1 - Math.exp(-4/Math.max(1, s.R*0.001*sr));
    const voices = this.voices, bqL = this.bqL, bqR = this.bqR;
    const dcOn = d.dcMode === 2, hpR = 1 - 6.283185307179586*8/sr, hx = this.hx, hy = this.hy;
    for (let i = 0; i < n; i++){
      if (--this.sm <= 0){ this.sm = 16; for (let q = 0; q < keys.length; q++){ const k = keys[q]; s[k] += (t[k] - s[k])*a; } }
      // audio-critical parameters glide every sample, so cut-rate and width moves don't step every 16 samples
      s.k += (t.k - s.k)*a1; s.kHz += (t.kHz - s.kHz)*a1; s.w += (t.w - s.w)*a1; s.c += (t.c - s.c)*a1;
      s.depth += (t.depth - s.depth)*a1; s.I += (t.I - s.I)*a1; s.hard += (t.hard - s.hard)*a1;
      s.w2 += (t.w2 - s.w2)*a1; s.k2 += (t.k2 - s.k2)*a1; s.kHz2 += (t.kHz2 - s.kHz2)*a1; s.c2 += (t.c2 - s.c2)*a1;
      if (--this.cnt <= 0){ this.cnt = 32; for (const v of voices) if (v.active){ this.couple(v, s, d); this.spread(v); } }
      const gk = 1 - Math.exp(-3/Math.max(1, s.glide*0.001*sr));
      const beA = 1/Math.max(1, s.benvA*0.001*sr), beD = 1 - Math.exp(-4/Math.max(1, s.benvD*0.001*sr));
      const beA2 = 1/Math.max(1, s.benvA2*0.001*sr), beD2 = 1 - Math.exp(-4/Math.max(1, s.benvD2*0.001*sr));
      for (const v of voices){
        if (!v.active) continue;
        if (v.freq !== v.freqT){
          const lr = Math.log(v.freqT/v.freq);
          v.freq = Math.abs(lr) < 1e-5 ? v.freqT : v.freq*Math.exp(lr*gk);
        }
        v.gr = v.freq/v.fc;
        // blade envelope: linear attack, exponential decay to zero; scales cut rate and width of both blades
        if (v.bst === 1){ v.be += beA; if (v.be >= 1){ v.be = 1; v.bst = 2; } }
        else if (v.bst === 2){ v.be -= v.be*beD; if (v.be < 1e-4){ v.be = 0; v.bst = 0; } }
        const bE = v.be*v.bv;
        v.kE = s.benvK ? Math.pow(2, s.benvK*4*bE) : 1;
        v.wE = s.benvW ? Math.pow(2, s.benvW*3*bE) : 1;
        if (d.b2env){
          // blade 2's own envelope
          if (v.bst2 === 1){ v.be2 += beA2; if (v.be2 >= 1){ v.be2 = 1; v.bst2 = 2; } }
          else if (v.bst2 === 2){ v.be2 -= v.be2*beD2; if (v.be2 < 1e-4){ v.be2 = 0; v.bst2 = 0; } }
          const bE2 = v.be2*v.bv2;
          v.kE2 = s.benvK2 ? Math.pow(2, s.benvK2*4*bE2) : 1;
          v.wE2 = s.benvW2 ? Math.pow(2, s.benvW2*3*bE2) : 1;
        } else { v.kE2 = v.kE; v.wE2 = v.wE; }
        if (v.stage === 1){ v.env += attInc; if (v.env >= 1){ v.env = 1; v.stage = 2; } }
        else if (v.stage === 2) v.env += (s.S - v.env)*dC;
        else if (v.stage === 4){ v.env -= v.env*rC; if (v.env < 1e-4){ v.env = 0; v.active = false; } }
      }
      let yl = 0, yr = 0;
      const dcTick = dcOn && --this.dcCnt <= 0;
      if (dcTick) this.dcCnt = 256;
      const w0 = s.w, d0 = s.depth, I0 = s.I, rs = s.rotRate/sr;
      // rotation is bipolar: negative rates run the blade backwards round the cycle
      const rOn = Math.abs(t.rotRate) > 0.004, sOn = t.rotSpread > 0.004, hk = 1 - Math.exp(-1/(0.03*sr));
      if (rOn){ this.gRot += rs; this.gRot -= Math.floor(this.gRot); } else this.gRot = RazorCore.home(this.gRot, hk);
      const rotBase = d.rotSync ? null : this.gRot;
      // blade 2 rotates with blade 1, or on a clock of its own
      const own2 = s.b2on && !d.rot2Follow, rs2 = s.rotRate2/sr, rOn2 = Math.abs(t.rotRate2) > 0.004;
      const sOn2 = d.b2sp ? t.rotSpread2 > 0.004 : sOn;
      if (own2){ if (rOn2){ this.gRot2 += rs2; this.gRot2 -= Math.floor(this.gRot2); } else this.gRot2 = RazorCore.home(this.gRot2, hk); }
      const rotBase2 = d.rotSync ? null : this.gRot2;
      for (const v of voices){
        if (!v.active) continue;
        if (rOn){ v.rot += rs; v.rot -= Math.floor(v.rot); } else v.rot = RazorCore.home(v.rot, hk);
        if (own2){ if (rOn2){ v.rot2 += rs2; v.rot2 -= Math.floor(v.rot2); } else v.rot2 = RazorCore.home(v.rot2, hk); }
        for (let q = 0; q < N; q++){
          const mm = v.m[q];
          if (sOn){ mm.rot += mm.rotOff/sr; mm.rot -= Math.floor(mm.rot); } else mm.rot = RazorCore.home(mm.rot, hk);
          if (own2){ if (sOn2){ mm.rot2 += mm.rotOff2/sr; mm.rot2 -= Math.floor(mm.rot2); } else mm.rot2 = RazorCore.home(mm.rot2, hk); }
        }
      }
      for (let j = 0; j < os; j++){
        let accL = 0, accR = 0;
        for (const v of voices){
          if (!v.active) continue;
          let vl = 0, vr = 0;
          const xOn = s.xm > 0.0005 || s.fb > 0.0005, xb = v.xb;
          if (xOn) for (let q = 0; q < N; q++) xb[q] = v.m[q].y1;
          for (let q = 0; q < N; q++){
            const mm = v.m[q];
            // cross-member modulation: phase push from the next member round the ring; feedback: from itself
            const xin = xOn ? 0.5*(s.xm*xb[(q + 1) % N] + s.fb*0.5*(mm.y1 + mm.y2)) : 0;
            mm.ns.xin = xin; mm.ns2.xin = xin; mm.bx.ns3.xin = xin; mm.bx.ns4.xin = xin;
            // per-member overrides of the shared parameters, restored after the voices
            s.w = w0 < 0.004 ? w0 : Math.min(1, w0*mm.wMul*v.wE);
            s.depth = Math.min(1, Math.max(0, d0 + mm.dAdd));
            s.I = I0*mm.iMul;
            RazorCore.mr = mm.mr; RazorCore.mn = mm.mn;
            const rotAll = (rotBase === null ? v.rot : rotBase) + mm.rot, fr2 = d.frame2 < 0 ? d.frame : d.frame2;
            let c = s.c + rotAll + mm.cOff + (d.frame ? mm.lead : 0); c -= Math.floor(c);
            const fi = Math.max(1, mm.inc*v.gr);
            // Hz units: the same absolute rate for every member and every note
            const kCap = 0.45*sr*os/fi;   // keep every blade carrier under the oversampled Nyquist
            const kq = Math.min((d.lock === 2 ? Math.max(0.05, s.kHz/fi + mm.kAdd) : Math.max(0.25, s.k + mm.kAdd))*mm.kMul*v.kE, kCap);
            if (s.b2on){
              const bx = mm.bx;
              const rot2All = own2 ? (rotBase2 === null ? v.rot2 : rotBase2) + mm.rot2 : rotAll;
              bx.c = s.c2 + rot2All + mm.cOff2 + (fr2 ? mm.lead : 0); bx.c -= Math.floor(bx.c);
              const lock2 = d.lock2 < 0 ? d.lock : d.lock2;
              bx.k = Math.min((lock2 === 2 ? Math.max(0.05, s.kHz2/fi + mm.kAdd2) : Math.max(0.25, s.k2 + mm.kAdd2))*mm.kMul2*v.kE2, kCap);
              const mEff2 = d.b2fm ? (d.mUnit2 ? s.mHz2/fi : s.m2) : (d.mUnit ? s.mHz/fi : s.m);
              RazorCore.fillG2(bx.g, s, s.w2 < 0.004 ? s.w2 : Math.min(1, s.w2*mm.wMul2*v.wE2), Math.min(1, Math.max(0, s.depth2 + mm.dAdd2)),
                (d.b2fm ? s.I2 : I0)*mm.iMul2, mEff2);
              bx.mr = mm.mr2; bx.mn = mm.mn2;
            }
            s.mEff = d.mUnit ? s.mHz/fi : s.m;
            let y = this.stepM(mm, mm.inc*v.gr/(sr*os), c, kq, s);
            if (xOn){ mm.y2 = mm.y1; mm.y1 = y; }
            if (dcOn){
              // big numeric estimates refresh less often, so the cost stays about constant
              if (dcTick && j === 0 && (--mm.dcWait <= 0 || mm.dcInit)){
                mm.dc = this.dcEst(mm.ns, mm.modX, c, kq, s);
                const j1 = this._dcJ;
                if (s.b2on){
                  const mr0 = RazorCore.mr, mn0 = RazorCore.mn; RazorCore.mr = mm.bx.mr; RazorCore.mn = mm.bx.mn;
                  mm.dc += this.dcEst(mm.bx.ns3, mm.bx.modX, mm.bx.c, mm.bx.k, mm.bx.g); this._dcJ += j1;
                  RazorCore.mr = mr0; RazorCore.mn = mn0;
                }
                mm.dcWait = Math.max(1, Math.ceil(this._dcJ/64));
                if (mm.dcInit){ mm.dcS = mm.dc; mm.dcInit = false; }
              }
              mm.dcS += (mm.dc - mm.dcS)*0.003;
              y -= mm.dcS;
            }
            vl += y*gl[q]; vr += y*gr[q];
          }
          const amp = v.env*v.vel*norm;
          accL += vl*amp; accR += vr*amp;
        }
        s.w = w0; s.depth = d0; s.I = I0;
        if (os > 1){
          accL = this.bqf(bqL[1], this.bqf(bqL[0], accL));
          accR = this.bqf(bqR[1], this.bqf(bqR[0], accR));
        }
        yl = accL; yr = accR;
      }
      // cross-mod and feedback make the cycle depend on the previous sample, which the per-cycle estimate can't see:
      // back it up with the blocker so any residual offset still drains away
      if (d.dcMode === 1 || (d.dcMode === 2 && (s.xm > 0.0005 || s.fb > 0.0005))){
        const ol = yl - hx[0] + hpR*hy[0]; hx[0] = yl; hy[0] = ol; yl = ol;
        const or = yr - hx[1] + hpR*hy[1]; hx[1] = yr; hy[1] = or; yr = or;
      }
      L[i] = Math.tanh(yl*s.gain*1.6);
      R[i] = Math.tanh(yr*s.gain*1.6);
      if (--this.vc <= 0){
        this.vc = Math.floor(sr/30);
        if (this.post){
          let lv = null, cntA = 0;
          for (const v of voices) if (v.active){ cntA++; if (!lv || v.age > lv.age) lv = v; }
          if (lv){
            const mem = {phi:[], c:[], k:[], w:[], dep:[], I:[], mr:[], mn:[], k2:[], w2:[], dep2:[], c2:[], I2:[], mr2:[], mn2:[]};
            for (let q = 0; q < N; q++){
              const mm = lv.m[q];
              mem.phi.push(mm.phi); mem.c.push((d.rotSync ? lv.rot : this.gRot) + mm.rot + mm.cOff + (d.frame ? mm.lead : 0)); mem.k.push(mm.kEff*lv.kE);
              mem.w.push(s.w < 0.004 ? s.w : Math.min(1, s.w*mm.wMul*lv.wE));
              mem.k2.push(mm.bx.k); mem.w2.push(mm.bx.g.w); mem.dep2.push(mm.bx.g.depth); mem.I2.push(mm.bx.g.I); mem.mr2.push(mm.mr2); mem.mn2.push(mm.mn2); const r1 = (d.rotSync ? lv.rot : this.gRot) + mm.rot, r2 = (s.b2on && !d.rot2Follow) ? (d.rotSync ? lv.rot2 : this.gRot2) + mm.rot2 : r1;
              mem.c2.push((r2 + mm.cOff2 + ((d.frame2 < 0 ? d.frame : d.frame2) ? mm.lead : 0)) - (r1 + mm.cOff + (d.frame ? mm.lead : 0)));
              mem.dep.push(Math.min(1, Math.max(0, s.depth + mm.dAdd))); mem.I.push(s.I*mm.iMul);
              mem.mr.push(mm.mr); mem.mn.push(mm.mn);
            }
            this.post({t:'viz', phases:mem.phi, mem, r:lv.r, freq:lv.freq*Math.pow(2, s.bend/12), count:cntA});
          } else this.post({t:'viz', count:0});
        }
      }
    }
  }
}

RazorCore.PANS = [[0.0],[-1.0,1.0],[-1.0,1.0,0.0],[-0.3333,1.0,-1.0,0.3333],[-0.5,1.0,0.0,-1.0,0.5],[-0.2,0.6,-1.0,1.0,-0.6,0.2],[-0.3333,1.0,-1.0,0.6667,-0.6667,0.3333,0.0],[-0.1429,0.4286,-0.7143,1.0,-1.0,0.7143,-0.4286,0.1429],[-0.25,1.0,-0.75,-0.5,0.0,0.5,0.75,-1.0,0.25]];

if (typeof module !== 'undefined') module.exports = RazorCore;
