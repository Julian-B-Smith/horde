#!/usr/bin/env node
// verify.js — headless verification battery for the Scalpel engine oracle.
// Usage: node verify.js            (all checks, ~30–60 s)
//        node verify.js --bench    (also report cost per member-tick)
// Exit code 0 = all PASS. Each line prints the measured value next to its threshold, so regressions surface as numbers.
const RazorCore = require('../prototype/razor-core.js');
const {seed} = require('./rng.js');
const SR = 48000, mtof = n => 440*Math.pow(2, (n - 69)/12);
let failures = 0;
function check(name, value, ok, want){
  const tag = ok ? 'PASS' : 'FAIL'; if (!ok) failures++;
  console.log(`${tag}  ${name.padEnd(58)} ${String(value).padStart(12)}   (${want})`);
}
function engine(p, notes = [45], vel = 0.9, s = 0x1234567){
  seed(s); const c = new RazorCore(SR); c.set(Object.assign({gain:.2}, p)); Object.assign(c.s, c.t);
  for (const n of notes) c.noteOn(n, mtof(n), vel); return c;
}
function render(c, secs){ const L = new Float32Array(128), R = new Float32Array(128), l = [], r = [];
  for (let b = 0; b < secs*SR/128; b++){ c.render(L, R); for (let i = 0; i < 128; i++){ l.push(L[i]); r.push(R[i]); } } return [l, r]; }
function mag(x, f){ const N = 16384, y = x.slice(-N); let re = 0, im = 0;
  for (let n = 0; n < N; n++){ const w = 0.5 - 0.5*Math.cos(2*Math.PI*n/N); re += y[n]*w*Math.cos(2*Math.PI*f*n/SR); im += y[n]*w*Math.sin(2*Math.PI*f*n/SR); }
  return 20*Math.log10(Math.hypot(re, im)/N*4 + 1e-12); }
const mean = a => a.reduce((p, q) => p + q, 0)/a.length;

console.log('Scalpel engine verification — oracle: prototype/razor-core.js\n');

// 1. Coupling: lock and splay
{ const c = engine({N:7, K:1, detune:40, phaseMode:2}); render(c, 0.5); const v = c.voices.find(x => x.active);
  check('Kuramoto lock: r at K=+1, 7 members, ±40 ct', v.r.toFixed(3), v.r >= 0.9, '≥ 0.90');
  const c2 = engine({N:4, K:-1, detune:3, phaseMode:2}); render(c2, 1.0); const v2 = c2.voices.find(x => x.active);
  check('Kuramoto splay: r at K=−1, 4 members', v2.r.toFixed(3), v2.r <= 0.1, '≤ 0.10'); }

// 2. Coupling time 'cycles': locked phase spread identical on every note
{ const spreads = [];
  for (const n of [33, 45, 57, 69, 81]){ const c = engine({N:6, detune:20, K:.7, cScale:1, phaseMode:2}, [n]); render(c, 1.5);
    const v = c.voices.find(x => x.active); const leads = v.m.slice(0, 6).map(m => m.lead); spreads.push(Math.max(...leads) - Math.min(...leads)); }
  const dev = Math.max(...spreads) - Math.min(...spreads);
  check('Coupling time = cycles: lead spread range across A1–A5', dev.toFixed(4), dev < 0.01, '< 0.01 cycles'); }

// 3. Swarm frame: every member's blade lands at the same swarm time
{ const c = engine({N:6, K:.3, detune:20, frame:1, phaseMode:2}); render(c, 0.8); const v = c.voices.find(x => x.active);
  const land = v.m.slice(0, 6).map(m => { let x = c.s.c + m.rot + m.cOff + m.lead - m.lead; return x; });
  const b2 = engine({N:6, K:.3, detune:20, frame:0, frame2:1, b2on:1, phaseMode:2}); render(b2, 0.8); const v2 = b2.voices.find(x => x.active);
  const rel = v2.m.slice(0, 6).map(m => { const x = m.bx.c - m.lead; return x - Math.floor(x); });
  const spread = Math.max(...rel) - Math.min(...rel);
  check('Blade 2 in swarm frame: centre − lead identical across members', spread.toExponential(1), spread < 1e-6, '< 1e-6'); }

// 4. Anti-aliasing (2× OS + PolyBLEP)
for (const [lab, p] of [['blade 1 saw sync', {N:1, w:.3, c:.5, k:7, mode:0, hot:2}],
                        ['blade 2 saw sync (blade 1 bypassed)', {N:1, w:.001, b2on:1, mode2:0, hot2:2, w2:.3, k2:7, c2:.5}],
                        // FM with an integer-ratio sine modulator stays harmonic, so the probe isolates carrier-wrap aliasing.
                        // (Noise-modulated FM puts energy between harmonics on purpose: that is the crunch, not aliasing.)
                        ['blade 1 rev-saw carrier under FM (reset, ratio 2)', {N:1, w:1, c:.5, k:1, mode:1, hot:4, fmType:0, mshape:0, I:.3, m:2}]]){
  const c = engine(Object.assign({os:2, aa:1}, p), [81]); const [l] = render(c, 0.6);
  const alias = mag(l, 880*2.5) - mag(l, 880*2);
  check(`Aliasing, A5, ${lab}: between-harmonic probe`, alias.toFixed(1) + ' dB', alias < -100, '< −100 dB rel. h2'); }

// 5. Per-cycle DC estimate vs brute force
{ const c = new RazorCore(SR), s = c.s, m = c.voices[0].m[0]; let worst = 0, cases = 0;
  const truth = (cc, k) => { const ns = {inside:false, seed:0, acc:0, xin:0, rnd:() => 0}; let sum = 0; const N = 100000;
    for (let n = 0; n < N; n++){ const phi = (n + .5)/N; sum += RazorCore.voice(s, phi, cc, k, 0, ns) - RazorCore.wave(s.base, phi); } return sum/N; };
  for (const mode of [0, 1, 4, 5, 6]) for (const hot of [0, 2, 3, 4]) for (const hard of [0, .4]) for (const mirror of [0, 1]) for (const [w, k] of [[.25, 6], [.8, 40], [.5, 64]]){
    Object.assign(s, {mode, hot, base:(mode + hot)%2 ? 2 : 0, lock:0, mirror, mEff:2, fmType:0, mshape:0, hard, depth:1, w, I:1});
    const err = Math.abs(c.dcEst(m.ns, 0, .5, k, s) - truth(.5, k)); worst = Math.max(worst, err); cases++; }
  check(`Per-cycle DC estimate, worst error over ${cases} cases`, worst.toFixed(4), worst < 0.01, '< 0.01'); }

// 6. Output DC residual with per-cycle fix, including while k moves
{ const c = engine({N:5, w:.8, c:.6, k:24, detune:14, K:.35, dcMode:2}); const out = [];
  for (let seg = 0; seg < 6; seg++){ c.set({k:24 + seg*3}); const [l] = render(c, 0.25); out.push(Math.abs(mean(l.slice(4000)))); }
  const worst = Math.max(...out); check('Output DC residual, per-cycle fix, k stepping 24→39', worst.toFixed(4), worst < 0.003, '< 0.003'); }

// 7. Crush continuity with slew
{ const p = {mode:6, w:.6, depth:1, lock:0, mirror:0, hot:2, fmType:0, mshape:0, I:0, m:1, mEff:1, hard:.5};
  let worst = 0;
  for (const base of [0, 1, 2, 4, 3]){ p.base = base;
    for (const phi of [0.31, 0.4, 0.62, 0.85]){ let prev = null; for (let k = 2; k < 60; k += 0.001){ const y = RazorCore.voice(p, phi, .6, k, 0, {inside:true}); if (prev !== null) worst = Math.max(worst, Math.abs(y - prev)); prev = y; } } }
  check('Crush with slew: largest jump as k sweeps 2→60', worst.toFixed(4), worst < 0.01, '< 0.01'); }

// 8. Balanced pan order: no brightness tilt from gradient spreads (4–9 members)
{ let worst = 0; const b = Math.exp(-2*Math.PI*3000/SR);
  for (const N of [4, 5, 6, 7, 8, 9]){ const c = engine({N, width:.7, w:.25, wspread:.5, dcMode:2}); const [l, r] = render(c, 1.2);
    const hf = x => { let e = 0, h = 0, hp = 0, pr = 0; for (let i = 4000; i < x.length; i++){ const v = x[i]; e += v*v; hp = b*(hp + v - pr); pr = v; h += hp*hp; } return h/e; };
    worst = Math.max(worst, Math.abs(10*Math.log10(hf(r)/hf(l)))); }
  check('Balanced pan: worst L/R brightness difference, width spread', worst.toFixed(2) + ' dB', worst < 1.5, '< 1.5 dB'); }

// 9. Cut rules
{ const s = {bspread:0, kspread:4, kq:1, rotSpread:0, wspread:0, dspread:0, ispread:0, morph:.5, mspread:0, lock:0, kHz:1000, k:6, w:.25, kRuleAmt:1, kCustom:'1, 7/6, 4/3, 13/8'};
  const want = {1:[6,12,18,24,30], 2:[6,3,2,1.5,1.2], 4:[6,7.5,9,12,15], 7:[6,9.708,15.708,25.416,41.125], 9:[6,7,8,9.75,12]};
  let worst = 0;
  for (const r in want){ s.kRule = +r; for (let i = 0; i < 5; i++){ const m = {rv:new Float64Array(14), lead:0, kMul:1}; RazorCore.spreadMember(m, i, 5, s, 0, 110, 48000); worst = Math.max(worst, Math.abs(m.kEff - want[r][i])); } }
  check('Cut rules (harmonic, undertone, major, golden, custom)', worst.toExponential(1), worst < 1e-2, '< 0.01 from spec lists'); }

// 10. Independent blade envelopes
{ const c = engine({b2on:1, N:1, benvK:.5, benvD:100, benvA:1, b2env:1, benvK2:.5, benvA2:400, benvD2:1000, benvVel:0, benvVel2:0}, [57], 1);
  const v = c.voices[0]; render(c, 0.2); const e1 = v.kE, e2a = v.kE2; render(c, 0.3); const e2b = v.kE2;
  check('Blade envelopes independent: env 1 done while env 2 rising', `${e1.toFixed(2)} / ${e2a.toFixed(2)}→${e2b.toFixed(2)}`, e1 < 1.1 && e2b > e2a, 'env1 ≈ 1, env2 rising'); }

// 11. Legato and glide
{ const c = engine({polyMode:2, glide:100, N:1}, [57]); render(c, 0.2); const v = c.voices[0];
  c.noteOn(60, mtof(60), 1); render(c, 0.02); const mid = v.freq, stage = v.stage; render(c, 0.3);
  check('Legato: no retrigger, glide passes between notes', `${mid.toFixed(1)} Hz, stage ${stage}`, stage === 2 && mid > 221 && mid < mtof(60) - 1, 'between 220 and 261.6, stage 2');
  check('Legato: glide settles on target', v.freq.toFixed(2), Math.abs(v.freq - mtof(60)) < 0.05, '261.63 ± 0.05'); }

// 12. Stability sweep
{ let bad = 0, peak = 0, n = 0;
  for (const mode of [0, 1, 2, 3, 4, 5, 6]) for (const mirror of [0, 1, 2, 3]) for (const extra of [{}, {b2on:1, mode2:5, hot2:3}, {xm:1, fb:1}, {b2on:1, b2sp:1, b2fm:1, mode2:2, fmType2:1, mshape2:7, I2:.4, law:1, kRule2:7}]){
    const c = engine(Object.assign({mode, mirror, N:4, mshape:7, hard:.3, hot:6, morph:.7}, extra), [57, 64]); const [l] = render(c, 0.15);
    for (const x of l){ if (!isFinite(x)) bad++; peak = Math.max(peak, Math.abs(x)); } n++; }
  check(`Stability: non-finite samples over ${n} configurations`, bad, bad === 0, '0');
  check('Stability: output bounded', peak.toFixed(3), peak <= 1, '≤ 1 (soft clip)'); }

// 13. Determinism under a fixed seed
{ const a = render(engine({N:5, law:4, b2on:1, xm:.4}, [48, 55], .9, 99), 0.3)[0], b = render(engine({N:5, law:4, b2on:1, xm:.4}, [48, 55], .9, 99), 0.3)[0];
  let md = 0; for (let i = 0; i < a.length; i++) md = Math.max(md, Math.abs(a[i] - b[i]));
  check('Determinism: same seed, same output', md, md === 0, 'identical'); }

if (process.argv.includes('--bench')){
  console.log('\nCost per member-tick (JS reference; see SPEC §8 for C++ expectations):');
  for (const [lab, p] of [['default sync blade', {}], ['two blades', {b2on:1}], ['two blades + twin + cross-mod + envelopes', {b2on:1, mirror:2, xm:.5, benvK:.5}]]){
    const c = new RazorCore(SR); c.set(Object.assign({N:9, os:2}, p)); for (let n = 0; n < 6; n++) c.noteOn(48 + n*4, 200, 1);
    const L = new Float32Array(128), R = new Float32Array(128); for (let b = 0; b < 60; b++) c.render(L, R);
    const t0 = process.hrtime.bigint(); for (let b = 0; b < SR/128; b++) c.render(L, R);
    const ns = Number(process.hrtime.bigint() - t0)/(6*9*SR*2);
    console.log(`  ${lab.padEnd(44)} ${ns.toFixed(1)} ns`); }
}
console.log(failures ? `\n${failures} check(s) FAILED` : '\nAll checks passed');
process.exit(failures ? 1 : 0);
