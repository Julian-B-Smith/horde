/* feedback_scan — sweep the feedback lab's REAL DSP and find what produces
 * "swelling undertones" (human report, 2026-08-15).
 *
 * WHY IT EXTRACTS RATHER THAN REIMPLEMENTS. The processor source is lifted out of
 * `docs/design/feedback-lab.html` and run under a small AudioWorklet shim, so the
 * thing scanned is the thing that made the sound. A re-implementation would agree
 * with itself and certify nothing (the same reason bend-lab's quantiser was
 * verified by executing its own class).
 *
 * WHAT "UNDERTONE" MEANS HERE, stated so the number is falsifiable: energy below
 * 0.85 x the source fundamental. The source is a detuned saw swarm at a known
 * pitch, so anything under that band was MANUFACTURED BY THE LOOP — it is not in
 * the input. "Swelling" is that band's energy in the last quarter of the render
 * divided by its energy in the first quarter; > 1 means growing.
 *
 * CONTROL (L0032): every sweep includes loop gain 0. With no loop there can be no
 * loop-made undertone, so if that row ever reports one, the DETECTOR is wrong and
 * nothing else in the table can be trusted.
 */
import './labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SR = 44100;
const html = readFileSync(join(ROOT, 'docs/design/feedback-lab.html'), 'utf8');

// ---- extract the processor, brace-free: it lives between the PROC backticks ----
const m = html.match(/const PROC = `([\s\S]*?)`;/);
if (!m) { console.error('feedback_scan: could not find PROC in feedback-lab.html'); process.exit(1); }

let registered = null;
globalThis.sampleRate = SR;
globalThis.AudioWorkletProcessor = class { constructor(){ this.port = { postMessage(){}, onmessage: null }; } };
globalThis.registerProcessor = (_n, cls) => { registered = cls; };
new Function(m[1])();
if (!registered) { console.error('feedback_scan: processor did not register'); process.exit(1); }

const midiHz = n => 440 * Math.pow(2, (n - 69) / 12);

function render(params, seconds = 2.2){
  const p = registered;
  const proc = new p();
  Object.assign(proc.p, params);
  let auto = false;
  proc.port.postMessage = msg => { if (msg && msg.autokill) auto = true; };
  const N = 128, blocks = Math.floor(seconds * SR / N);
  const out = new Float32Array(blocks * N);
  const L = new Float32Array(N), R = new Float32Array(N);
  for (let b = 0; b < blocks; b++){
    proc.process([], [[L, R]]);
    out.set(L, b * N);
  }
  return { out, auto };
}

/* HANN-WINDOWED band energy, and the window is the whole measurement.
   The first version of this file was unwindowed, and its control — gain 0, no
   loop, therefore no loop-made undertone possible — reported 18.5% sub-band
   energy. That was pure spectral leakage: a rectangular window's sidelobes drag
   the 110 Hz fundamental down into the 15-94 Hz band being measured. Exactly the
   trap steal_check hit and fixed the same way; measuring a weak residue beside a
   strong signal is the case that has fooled this project repeatedly (L0016).
   A band sum rather than a single bin, because the loop's products are not at
   tidy bin centres and one bin would under-read them. */
function bandEnergy(x, lo, hi, step = 3){
  const N = x.length;
  const win = new Float64Array(N);
  let wsum = 0;
  for (let i = 0; i < N; i++){ win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1)); wsum += win[i]; }
  let e = 0;
  for (let f = lo; f <= hi; f += step){
    const w = 2 * Math.PI * f / SR, c = 2 * Math.cos(w);
    let s1 = 0, s2 = 0;
    for (let i = 0; i < N; i++){ const s0 = win[i] * x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const re = s1 - s2 * Math.cos(w), im = s2 * Math.sin(w);
    e += (re * re + im * im) / (wsum * wsum);
  }
  return Math.sqrt(e);
}

function analyse(out, f0){
  const q = Math.floor(out.length / 4);
  const first = out.subarray(0, q), last = out.subarray(out.length - q);
  const subLo = 15, subHi = 0.85 * f0;
  const sub = bandEnergy(out, subLo, subHi);
  const tot = bandEnergy(out, subLo, Math.min(SR / 2 - 100, 8 * f0), 7);
  const s1 = bandEnergy(first, subLo, subHi), s2 = bandEnergy(last, subLo, subHi);
  let peak = 0;
  for (let i = 0; i < out.length; i++){ const a = Math.abs(out[i]); if (a > peak) peak = a; }
  return {
    subFrac: tot > 1e-12 ? sub / tot : 0,
    swell: s1 > 1e-9 ? s2 / s1 : (s2 > 1e-9 ? Infinity : 1),
    peak
  };
}

const BASE = { gain:0, gran:16, extra:0, lpOn:1, lpF:6000, shOn:0, shHz:3, satOn:1,
               spOn:0, spF:40, spZ:1, nv:5, det:12, pit:45, src:0.25, kill:0, burst:0 };

const f0 = midiHz(BASE.pit);
const rows = [];
function run(label, over){
  const params = { ...BASE, ...over };
  const { out, auto } = render(params);
  const a = analyse(out, f0);
  rows.push({ label, ...a, auto });
  return a;
}

console.log(`feedback_scan — source f0 = ${f0.toFixed(1)} Hz; "undertone" = 15..${(0.85*f0).toFixed(0)} Hz\n`);

// ---- CONTROL FIRST: no loop, therefore no loop-made undertone ---------------
const ctl = run('CONTROL gain=0 (must read ~0 sub)', { gain: 0 });

// ---- factor sweeps ----------------------------------------------------------
for (const g of [0.3, 0.6, 0.85, 0.98]) run(`gain ${g}`, { gain: g });
for (const gr of [1, 16, 64, 256, 512]) run(`gran ${gr} @ gain .85`, { gain: .85, gran: gr });
for (const ex of [0, 5, 20, 50]) run(`extra ${ex}ms @ gain .85`, { gain: .85, extra: ex });
run('lowpass OFF @ gain .85', { gain: .85, lpOn: 0 });
run('saturator OFF @ gain .85', { gain: .85, satOn: 0 });
for (const hz of [-12, -3, 3, 12]) run(`shift ${hz}Hz @ gain .85`, { gain: .85, shOn: 1, shHz: hz });
for (const z of [0.15, 0.4, 1.0, 2.0]) run(`spring z=${z} f=40 @ gain .85`, { gain: .85, spOn: 1, spZ: z });
for (const f of [0.5, 5, 40, 200]) run(`spring f=${f} z=1 @ gain .85`, { gain: .85, spOn: 1, spF: f });
run('spring f=5 z=0.15 @ gain .85', { gain: .85, spOn: 1, spF: 5, spZ: 0.15 });

const pad = (s, n) => String(s).padEnd(n);
console.log(pad('config', 34) + pad('sub frac', 11) + pad('swell', 10) + pad('peak', 9) + 'autokill');
console.log('-'.repeat(74));
for (const r of rows){
  console.log(pad(r.label, 34) +
              pad((r.subFrac * 100).toFixed(1) + '%', 11) +
              pad(isFinite(r.swell) ? r.swell.toFixed(2) + '×' : '∞', 10) +
              pad(r.peak.toFixed(3), 9) + (r.auto ? 'TRIPPED' : ''));
}

console.log('');
if (ctl.subFrac > 0.05){
  console.log(`DETECTOR SUSPECT: control (no loop) reports ${(ctl.subFrac*100).toFixed(1)}% sub energy.`);
  console.log('Every row above is unreliable until that reads near zero.');
  process.exit(1);
}
const worst = rows.filter(r => r.label !== ctl.label)
                  .sort((a, b) => (b.subFrac * Math.min(b.swell, 50)) - (a.subFrac * Math.min(a.swell, 50)))
                  .slice(0, 5);
console.log('Strongest undertone producers (sub fraction x swell):');
for (const r of worst) console.log(`  ${pad(r.label, 34)} ${(r.subFrac*100).toFixed(1)}%  ${isFinite(r.swell)?r.swell.toFixed(2)+'×':'∞'}`);


/* =====================================================================
   EDGE MODE (`--edge`) — the measurement the survey actually asked for.
   =====================================================================
   Practitioners do not describe good feedback as "stable". They describe a
   WIDE, CONTROLLABLE REGION NEAR THE THRESHOLD — gain brought up "until you can
   just hear it", a resonance knob prized for approaching self-oscillation
   without committing. So the number worth optimising is not whether the loop is
   safe, it is how much of the dial sits between "just sustains" and "gone".

     sustain gain   lowest loop gain where a burst is still ringing at the end,
                    with the SOURCE OFF so nothing is propping it up.
     runaway gain   lowest loop gain that trips the auto-kill or pins the output.
     EDGE WIDTH     runaway - sustain. The playable region. Bigger is better,
                    and a stabiliser that only shifts both numbers up together
                    has bought nothing.

   This is also the honest test of Berdahl: if a small frequency shift buys gain
   margin, the SUSTAIN gain must RISE when it is switched on. */
function ringsAtEnd(params){
  const { out, auto } = render({ ...params, src: 0 }, 2.5);
  // Burst is 20 ms at the very start; the last fifth is pure loop, nothing else.
  const tail = out.subarray(Math.floor(out.length * 0.8));
  let e = 0;
  for (let i = 0; i < tail.length; i++) e += tail[i] * tail[i];
  const rms = Math.sqrt(e / tail.length);
  return { rms, auto };
}

function bisect(pred, lo, hi, iters = 9){
  if (!pred(hi)) return null;              // never happens even at max gain
  if (pred(lo)) return lo;
  for (let i = 0; i < iters; i++){
    const mid = (lo + hi) / 2;
    if (pred(mid)) hi = mid; else lo = mid;
  }
  return +((lo + hi) / 2).toFixed(3);
}

function edge(label, over){
  /* SOURCE OFF. The first version inherited BASE.src = 0.25, so the tail was full
     of source signal at every gain and `sustains(0)` came back TRUE — a loop with
     no loop, sustaining. The burst is the only excitation; what rings afterwards
     is the loop or nothing. */
  const base = { ...BASE, ...over, burst: 0, src: 0 };
  // burst is fired by the processor's own message path, so poke p.burst directly
  const withBurst = g => {
    const params = { ...base, gain: g };
    const proc = new registered();
    Object.assign(proc.p, params);
    proc.p.burst = Math.floor(SR * 0.02);
    let auto = false;
    proc.port.postMessage = m => { if (m && m.autokill) auto = true; };
    const N = 128, blocks = Math.floor(2.5 * SR / N);
    const L = new Float32Array(N), R = new Float32Array(N);
    let e = 0, n = 0, peak = 0;
    for (let b = 0; b < blocks; b++){
      proc.process([], [[L, R]]);
      if (b > blocks * 0.8){
        for (let i = 0; i < N; i++){ e += L[i] * L[i]; n++; const a = Math.abs(L[i]); if (a > peak) peak = a; }
      }
    }
    return { rms: Math.sqrt(e / Math.max(1, n)), auto, peak };
  };
  /* RUNAWAY IS READ FROM THE AUTO-KILL, NEVER FROM THE TAIL. The kill zeroes the
     output, so a post-trip tail reads 0.000 and looks like silence — the observer
     destroying the observable. A config that trips has runaway AT that gain and
     no measurable sustain above it, which is why sustain is searched only below
     the runaway point. */
  const runs = g => { const r = withBurst(g); return r.auto || r.peak >= 0.98; };
  const gr = bisect(runs, 0, 1.2);
  const ceiling = gr === null ? 1.2 : gr;
  const sustains = g => { const r = withBurst(g); return !r.auto && r.rms > 0.005; };
  const gs = bisect(sustains, 0, ceiling);
  return { label, sustain: gs, runaway: gr,
           width: (gs !== null && gr !== null) ? +(gr - gs).toFixed(3) : null };
}

if (process.argv.includes('--edge')){
  console.log('\nEDGE MODE — sustain gain, runaway gain, and the playable width between them\n');
  const cfgs = [
    ['bare loop (no stabilisers)',      { lpOn:0, satOn:0, shOn:0, spOn:0 }],
    ['+ saturator',                     { lpOn:0, satOn:1, shOn:0, spOn:0 }],
    ['+ lowpass 6k',                    { lpOn:1, satOn:0, shOn:0, spOn:0 }],
    ['+ lowpass + saturator',           { lpOn:1, satOn:1, shOn:0, spOn:0 }],
    ['+ shift 3 Hz',                    { lpOn:0, satOn:0, shOn:1, shHz:3 }],
    ['+ shift 12 Hz',                   { lpOn:0, satOn:0, shOn:1, shHz:12 }],
    ['+ shift 3 Hz + lowpass',          { lpOn:1, satOn:0, shOn:1, shHz:3 }],
    ['+ spring z=2.0 (overdamped)',     { lpOn:0, satOn:0, spOn:1, spF:40, spZ:2.0 }],
    ['+ spring z=1.0 (critical)',       { lpOn:0, satOn:0, spOn:1, spF:40, spZ:1.0 }],
    ['+ spring z=0.4',                  { lpOn:0, satOn:0, spOn:1, spF:40, spZ:0.4 }],
    ['+ spring z=0.15 (underdamped)',   { lpOn:0, satOn:0, spOn:1, spF:40, spZ:0.15 }],
    ['gran 1 sample',                   { lpOn:0, satOn:0, gran:1 }],
    ['gran 16 (our tick)',              { lpOn:0, satOn:0, gran:16 }],
    ['gran 64 (vector)',                { lpOn:0, satOn:0, gran:64 }],
    ['gran 256 (block)',                { lpOn:0, satOn:0, gran:256 }],
    ['gran 512 (buffer)',               { lpOn:0, satOn:0, gran:512 }],
  ];
  const res = cfgs.map(([l, o]) => edge(l, o));
  console.log(pad('config', 32) + pad('sustains at', 14) + pad('runs away at', 15) + 'EDGE WIDTH');
  console.log('-'.repeat(74));
  for (const r of res){
    console.log(pad(r.label, 32) +
      pad(r.sustain === null ? 'never' : r.sustain.toFixed(3), 14) +
      pad(r.runaway === null ? 'never' : r.runaway.toFixed(3), 15) +
      (r.width === null ? '—' : r.width.toFixed(3)));
  }
  const ranked = res.filter(r => r.width !== null).sort((a, b) => b.width - a.width);
  console.log('\nWidest playable region:');
  for (const r of ranked.slice(0, 4)) console.log(`  ${pad(r.label, 32)} ${r.width.toFixed(3)}`);
}
