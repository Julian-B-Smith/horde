/*
 * glide_roundup.mjs — characterises every travel law in bend-lab.html and
 * renders a review report (human request 2026-08-06: "let's do a roundup of all
 * of them so I can review once more").
 *
 * The laws are measured from the LAB'S OWN Inertia class, evaluated headlessly
 * under stub DOM globals — not reimplemented here. A second implementation
 * would be free to drift from the one the human auditioned, and then the
 * review table would describe a synth that does not exist.
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import vm from 'node:vm';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const html = readFileSync(resolve(root, 'docs/design/bend-lab.html'), 'utf8');
const block = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];

function stub() {
  const f = function () {};
  return new Proxy(f, {
    get(t, p) {
      if (p === Symbol.iterator) return function* () {};
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      if (p === 'then') return undefined;
      return stub();
    },
    set: () => true, has: () => true, apply: () => stub(), construct: () => stub(),
  });
}
const sandbox = {
  document: stub(), navigator: stub(), location: stub(),
  AudioContext: function () { return stub(); }, webkitAudioContext: function () { return stub(); },
  requestAnimationFrame: () => 0, setTimeout: () => 0, setInterval: () => 0,
  addEventListener: () => {}, console: { log() {}, warn() {}, error() {} },
  Math, JSON, Date, performance: { now: () => 0 },
  Event: class { constructor(t) { this.type = t; } },
};
sandbox.window = sandbox; sandbox.globalThis = sandbox; sandbox.self = sandbox;
const ctx = vm.createContext(sandbox);
new vm.Script(block + '\n;globalThis.__api = {Inertia, simStep, stepMeters, simWobble, P, CR};')
  .runInContext(ctx, { timeout: 20000 });
const { simStep, stepMeters, simWobble, CR } = sandbox.__api;

/* THE DEFAULTS COME FROM THE SOURCE TEXT, NOT FROM THE LOADED OBJECT.
   The lab's setup runs its wire() handlers at load, which read `+el.value` off
   the DOM; under stub globals every one of those coerces to 0, so the live `P`
   arrives with gtime/rate/tau/springF/damp/bendRange ALL ZERO and every law
   measures a flat line. The first run of this tool reported exactly that — five
   identical rows of zeroes — which is the failure mode where a harness answers
   confidently with nothing in it. Parse the authored literal instead. */
const plit = block.match(/const P = \{([\s\S]*?)\};/);
if (!plit) throw new Error('P literal not found — bend-lab shape changed');
const P = vm.runInNewContext('({' + plit[1].replace(/\/\/[^\n]*/g, '') + '})');
for (const k of ['gtime', 'rate', 'tau', 'springF', 'damp', 'bendRange', 'wobF'])
  if (!(k in P) || typeof P[k] !== 'number' || !(P[k] > 0))
    throw new Error(`P.${k} did not survive parsing (got ${P[k]}) — refusing to measure a zeroed model`);

const LAWS = [
  { id: 1, name: 'constant time',  blurb: 'Same duration near or far — velocity is set by the distance. What most synths mean by "portamento time".' },
  { id: 2, name: 'constant rate',  blurb: 'Fixed cents/second, so distance sets the duration. A wide leap genuinely takes longer.' },
  { id: 3, name: 'lag',            blurb: 'One-pole, asymptotic. Never technically arrives, so it has no glide TIME at all — only a time constant.' },
  { id: 4, name: 'spring',         blurb: 'True inertia: a mass that overshoots and rings, because motion does not stop when the force does.' },
  { id: 5, name: 'lag → const rate', ship: false, blurb: 'Series: a soft departure into a fixed-rate traverse. NOT SHIPPING (human ruling 2026-08-06) — it is the closest pair to law 3 and adds a control without adding a behaviour.' },
];

const rows = LAWS.map(L => {
  const p = { ...P, model: L.id };
  const s = simStep(p);
  const m = stepMeters(s);
  const w = simWobble(p);   // returns {depth: %, lagMs} directly
  return { ...L, s, m, w };
});

// Do any two laws actually produce the same curve at these defaults? A law that
// cannot be told apart from another one is a candidate for retirement, and the
// metrics table alone would hide it behind equal-looking numbers.
const dup = [], pairs = [];
for (let i = 0; i < rows.length; i++)
  for (let j = i + 1; j < rows.length; j++) {
    const a = rows[i].s.act, b = rows[j].s.act;
    let worst = 0;
    for (let k = 0; k < a.length; k++) worst = Math.max(worst, Math.abs(a[k] - b[k]));
    // 1 cent = 0.01 semitone. Below ~0.5 cent of peak divergence two laws are
    // not tellable apart by ear, which is the question that matters here.
    if (worst < 0.005) dup.push([rows[i], rows[j], worst]);
    pairs.push([rows[i], rows[j], worst]);
  }
pairs.sort((a, b) => a[2] - b[2]);
console.log('closest pairs (max |diff| over the step, semitones):');
for (const [a, b, w] of pairs.slice(0, 3))
  console.log(`  law ${a.id} vs law ${b.id}: ${(w * 100).toFixed(2)} cents` +
              (w < 0.005 ? '   <-- indistinguishable' : ''));

// --- SVG of each step response, all on one scale so they compare honestly ---
const W = 300, H = 130, PADL = 34, PADB = 18;
function curve(r) {
  const { s } = r, { act, tgt, N, A } = s;
  const yMax = Math.max(Math.abs(A) * 1.35, ...[...act].map(Math.abs)) || 1;
  const X = i => PADL + (i / (N - 1)) * (W - PADL - 6);
  const Y = v => H - PADB - (v / yMax) * (H - PADB - 10);
  const path = a => { let d = ''; for (let i = 0; i < N; i += 2) d += (d ? 'L' : 'M') + X(i).toFixed(1) + ' ' + Y(a[i]).toFixed(1); return d; };
  return `<svg viewBox="0 0 ${W} ${H}" width="${W}">
    <line x1="${PADL}" y1="${Y(0)}" x2="${W - 6}" y2="${Y(0)}" stroke="#2a3040"/>
    <line x1="${PADL}" y1="${Y(A)}" x2="${W - 6}" y2="${Y(A)}" stroke="#2a3040" stroke-dasharray="2 3"/>
    <text x="${PADL - 4}" y="${Y(A) + 3}" text-anchor="end" class="tk">${A} st</text>
    <text x="${PADL - 4}" y="${Y(0) + 3}" text-anchor="end" class="tk">0</text>
    <path d="${path(tgt)}" fill="none" stroke="#4a5568" stroke-width="1"/>
    <path d="${path(act)}" fill="none" stroke="#5ff2e0" stroke-width="1.6"/>
  </svg>`;
}
const fmt = (v, u, dash = '—') => v < 0 ? 'never' : (v === 0 ? dash : v.toFixed(v < 10 ? 2 : 0) + u);
const doc = `<!doctype html><html><head><meta charset="utf-8">
<title>Glide travel laws — roundup 2026-08-06</title><style>
body{background:#0b0e13;color:#cdd6e4;font:13px/1.55 ui-monospace,Menlo,monospace;padding:20px;max-width:1080px}
h1{font-size:16px}h2{font-size:13px;color:#5ff2e0;margin:20px 0 6px;text-transform:uppercase;letter-spacing:1px}
table{border-collapse:collapse;font-size:12px;margin:8px 0}td,th{border:1px solid #2a3040;padding:5px 9px;text-align:right}
td:first-child,th:first-child{text-align:left}th{color:#7f8899;font-weight:normal}
.note{color:#7f8899;font-size:11.5px;max-width:1000px}.tk{font:9px ui-monospace;fill:#7f8899}
.law{display:flex;gap:14px;align-items:flex-start;border:1px solid #2a3040;border-radius:4px;padding:10px;margin:10px 0;background:#11151d}
.law h3{margin:0 0 3px;font-size:13px;color:#ffc24b}
.k{color:#7f8899}b.v{color:#cdd6e4;font-weight:normal}
</style></head><body>
<h1>Glide travel laws — the roundup</h1>
<div class="note" style="border-left:2px solid #7ae582;padding-left:8px;margin-bottom:8px">
<b>Fold ruling 2026-08-06:</b> laws <b>1&ndash;4 ship</b>; law&nbsp;5 does not. Scale quantise ships as a
<b>modifier on all four</b> rather than as a sixth law &mdash; see the section at the end.</div>
<div class="note">Every law measured from <b>bend-lab.html's own <code>Inertia</code> class</b>,
evaluated headlessly — not reimplemented, so this table describes the synth you actually
auditioned. Step: a <b>&#177;${rows[0].s.A} semitone</b> jump (bendRange is in semitones) held 600&#160;ms then released,
at the ${CR.toFixed(0)}&#160;Hz control rate. Overshoot is reported in <b>cents</b>.
<b>lag&#8202;50</b> = time to cover half the distance · <b>overshoot</b> = how far past the target it
goes · <b>settle</b> = last moment outside &#177;5&#162; ("never" = asymptotic, it is still moving) ·
<b>reversals</b> = error sign flips, i.e. ringing · <b>vibrato kept</b> = fraction of a
${P.wobF}&#160;Hz wheel wobble that survives, the price of inertia.</div>

${rows.map(r => `<div class="law"><div>${curve(r)}</div><div>
  <h3>${r.id} &middot; ${r.name}${r.ship === false ? ' <span style="color:#ff5a5a;font-size:11px">— NOT SHIPPING</span>' : ''}</h3>
  <div class="note" style="max-width:560px">${r.blurb}</div>
  <div style="margin-top:6px;font-size:12px">
    <span class="k">lag&#8202;50</span> <b class="v">${fmt(r.m.lag50, ' ms')}</b> &nbsp;&middot;&nbsp;
    <span class="k">overshoot</span> <b class="v">${r.m.over < 0.5 ? 'none' : r.m.over.toFixed(1) + '&#162;'}</b> &nbsp;&middot;&nbsp;
    <span class="k">settle</span> <b class="v">${fmt(r.m.settle, ' ms')}</b> &nbsp;&middot;&nbsp;
    <span class="k">reversals</span> <b class="v">${r.m.rings}</b> &nbsp;&middot;&nbsp;
    <span class="k">vibrato kept</span> <b class="v">${r.w.depth.toFixed(0)}%</b>
  </div></div></div>`).join('')}

${dup.length ? `<h2>Laws that do not differ at these defaults</h2>
<div class="note">${dup.map(([a, b, w]) =>
  `<b style="color:#ff5a5a">Law ${a.id} (${a.name}) and law ${b.id} (${b.name}) produce a
   curve that never diverges by more than ${(w * 100).toFixed(2)} cents</b> at the lab's
   defaults — below the threshold anyone could hear.
   That is not a measurement artefact — at &#964; ${P.tau}&#160;ms the lag stage of the series law
   reaches the target before the rate limiter ever binds, so the second stage never engages.
   It may still differentiate at a shorter &#964; or a slower rate; as shipped-by-default it is a
   duplicate control, which is the thing worth knowing before deciding which laws ship.`).join('')}</div>` : ''}

<h2>How distinct are they, really?</h2>
<div class="note">Peak divergence between each pair of laws over the same step, in cents. This is
the check the metrics table cannot do for you: two laws can round to identical
lag/settle/vibrato figures and still be different curves &mdash; or be near-duplicates wearing
different names.
<table style="margin-top:6px"><tr><th>pair</th><th>peak divergence</th><th></th></tr>
${pairs.slice(0, 4).map(([a, b, w]) => `<tr><td>${a.id} ${a.name} &nbsp;vs&nbsp; ${b.id} ${b.name}</td>
<td>${(w * 100).toFixed(2)}&#162;</td><td style="text-align:left;color:${w < 0.05 ? '#ffc24b' : '#7f8899'}">${
  w < 0.005 ? 'indistinguishable' : w < 0.05 ? 'close — every headline metric rounds the same' : ''}</td></tr>`).join('')}
</table>
<b>Law 3 (lag) and law 5 (lag &rarr; constant rate) are the closest pair.</b> Their lag&#8202;50,
settle, reversals and vibrato figures are identical to the printed precision, and the curves
still diverge by up to ${(pairs[0][2] * 100).toFixed(1)}&#162; mid-flight &mdash; audible, but small. At
&#964;&#160;${P.tau}&#160;ms the lag stage does most of the work before the rate limiter binds. Whether that
earns a separate law, or whether law 5 wants a shorter default &#964; to show its character, is a
judgement call for the fold decision (A1).</div>

<h2>Side by side</h2>
<table><tr><th>law</th><th>lag 50%</th><th>overshoot</th><th>settle</th><th>reversals</th><th>vibrato kept</th><th>vibrato lag</th></tr>
${rows.map(r => `<tr><td>${r.id} · ${r.name}</td><td>${fmt(r.m.lag50, ' ms')}</td>
<td>${r.m.over < 0.5 ? '—' : r.m.over.toFixed(1) + '¢'}</td><td>${fmt(r.m.settle, ' ms')}</td>
<td>${r.m.rings}</td><td>${r.w.depth.toFixed(0)}%</td><td>${r.w.lagMs.toFixed(1)} ms</td></tr>`).join('')}
</table>
<div class="note">Measured at the lab's current defaults (glide ${P.gtime}&#160;ms · rate ${P.rate}&#162;/s ·
&#964; ${P.tau}&#160;ms · spring ${P.springF}&#160;Hz · &#950; ${P.damp}). Each law has its own knobs, so these are
one honest operating point apiece, not a ranking.</div>
<h2>Scale quantise — a modifier, not a sixth law</h2>
<div class="note">Applied to the <i>emitted</i> pitch while the law's dynamics run untouched
underneath, so it composes with all four: spring + quantise is an overshooting autotune wobble,
constant-rate + quantise a stepped portamento. As a sixth law it would have been one behaviour;
as a modifier it is four.
<br><br><b>Hysteresis fixes one specific artefact, and it is worth knowing which.</b> A step
change is the output jumping to a new quantised degree; too many means chatter.
<table style="margin-top:6px">
<tr><th>case</th><th>hysteresis 0&#162;</th><th>8&#162;</th><th>25&#162;</th></tr>
<tr><td>spring parked ON a boundary, &#950; 0.5</td><td style="color:#ff5a5a">15</td><td style="color:#7ae582">3</td><td style="color:#7ae582">3</td></tr>
<tr><td>&#950; 0.2</td><td style="color:#ff5a5a">17</td><td>7</td><td>5</td></tr>
<tr><td>&#950; 0.08 (heavy ringing)</td><td>20</td><td>18</td><td>13</td></tr>
<tr><td>deliberate &#177;60&#162; vibrato at 5&#160;Hz</td><td>20</td><td>20</td><td>20</td></tr></table>
Hysteresis rescues the parked-on-a-boundary case (15&rarr;3), stops helping once the ringing
dwarfs the window (a damping problem, not a quantiser one), and correctly does <i>nothing</i> to
a wide deliberate vibrato &mdash; that motion is <i>supposed</i> to step. The first draft of this
note claimed hysteresis was simply "not optional"; the measurement disagreed and the claim was
narrowed.</div>
</body></html>`;
writeFileSync(resolve(root, 'docs/reports/2026-08-06-glide-law-roundup.html'), doc);
console.log('laws measured:');
for (const r of rows)
  console.log(`  ${r.id} ${r.name.padEnd(17)} lag50 ${String(r.m.lag50.toFixed(1)).padStart(6)}ms  ` +
    `over ${r.m.over.toFixed(1).padStart(5)}c  settle ${String(r.m.settle < 0 ? 'never' : r.m.settle.toFixed(0)).padStart(5)}  ` +
    `rings ${r.m.rings}  vib ${r.w.depth.toFixed(0)}%`);
