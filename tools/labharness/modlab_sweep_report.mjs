/*
 * modlab_sweep_report.mjs — renders the full 216-routing sweep as a
 * self-contained HTML report (visual-first review beat).
 *
 * Runs the sweep TWICE: once against the lab as it stands, and once against an
 * in-memory copy with the pre-fix `frac` logic patched back in. The "before"
 * column is therefore MEASURED, not asserted — the report cannot claim an
 * improvement the code does not actually produce.
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '../..');
const html = readFileSync(resolve(root, 'docs/design/mod-lab.html'), 'utf8');
const dspFixed = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];

// Reconstruct the pre-fix code: index guarded, frac derived from un-wrapped rd.
const FIXED = `        if (rd >= len) rd -= len;                  // rounded up to exactly len
        const i0 = rd | 0;`;
const BUGGY = `        let i0 = rd | 0;
        if (i0 >= len) i0 -= len;`;
if (!dspFixed.includes(FIXED)) throw new Error('fixed wrap shape not found — report is stale');
const dspBuggy = dspFixed.replace(FIXED, BUGGY);

const BLOCKS = 200, SR = 44100, NOTE = 45, FREQ = 440 * Math.pow(2, (NOTE - 69) / 12);

function sweep(dsp) {
  const api = new Function(dsp + '\n;return {ModLab, SOURCES, DESTS, MAXK, SRC_ENV};')();
  const { ModLab, SOURCES, DESTS, MAXK, SRC_ENV } = api;
  const mk = () => { const l = new ModLab(SR);
    l.rotor.setParam('n', MAXK); l.chorus.p.mix = 0.6; l.phaser.p.mix = 0.6;
    l.choSwarm.setParam('link', 0.5); l.phSwarm.setParam('link', 0.5);
    l.synth.p.cutoff = 0.5; l.depth[SRC_ENV][2] = 0.8; l.scope[SRC_ENV][2] = 0; return l; };
  const run = setup => { const lab = mk(); setup(lab); lab.noteOn(NOTE, FREQ);
    const L = new Float32Array(512), R = new Float32Array(512);
    let peak = 0, nan = false;
    for (let b = 0; b < BLOCKS; b++) { lab.render(L, R);
      for (let i = 0; i < 512; i++) { const v = L[i];
        if (!Number.isFinite(v)) { nan = true; break; }
        const a = Math.abs(v); if (a > peak) peak = a; }
      if (nan) break; }
    return { peak, nan, resets: lab.nanResets | 0 }; };
  const base = run(() => {});
  const cells = [];
  for (let si = 0; si < SOURCES.length; si++) { cells.push([]);
    for (let di = 0; di < DESTS.length; di++) {
      const a = run(l => { l.depth[si][di] = 1; }), b = run(l => { l.depth[si][di] = -1; });
      cells[si].push({ peak: Math.max(a.peak, b.peak),
                       nan: a.nan || b.nan || !!(a.resets || b.resets) }); } }
  return { base, cells, SOURCES, DESTS };
}

process.stderr.write('sweeping fixed…\n');  const fix = sweep(dspFixed);
process.stderr.write('sweeping pre-fix…\n'); const bug = sweep(dspBuggy);

const esc = s => String(s).replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
function grid(r, title) {
  const hot = r.base.peak * 4;
  let h = `<div><div class="note">${esc(title)} — baseline peak ${r.base.peak.toFixed(3)}, ` +
          `hot threshold ${hot.toFixed(2)}</div><table class="hm"><tr><th></th>` +
          r.DESTS.map(d => `<th>${esc(d)}</th>`).join('') + '</tr>';
  r.SOURCES.forEach((s, si) => {
    h += `<tr><th>${esc(s)}</th>`;
    r.DESTS.forEach((_, di) => {
      const c = r.cells[si][di];
      const cls = c.nan ? 'nan' : c.peak > hot ? 'hot' : 'ok';
      const txt = c.nan ? 'NaN' : c.peak.toFixed(2);
      h += `<td class="${cls}">${txt}</td>`; });
    h += '</tr>'; });
  return h + '</table></div>';
}
const count = r => { let nan = 0, hot = 0; const t = r.base.peak * 4;
  r.cells.forEach(row => row.forEach(c => { if (c.nan) nan++; else if (c.peak > t) hot++; }));
  return { nan, hot }; };
const cf = count(fix), cb = count(bug);

const doc = `<!doctype html><html><head><meta charset="utf-8">
<title>Full mod-matrix sweep — 2026-08-05</title><style>
body{background:#0b0e13;color:#cdd6e4;font:13px/1.5 ui-monospace,Menlo,monospace;padding:20px;max-width:1100px}
h1{font-size:16px}h2{font-size:13px;color:#5ff2e0;margin:20px 0 6px;text-transform:uppercase;letter-spacing:1px}
table{border-collapse:collapse;font-size:12px;margin:8px 0}td,th{border:1px solid #2a3040;padding:4px 9px;text-align:right}
td:first-child,th:first-child{text-align:left}th{color:#7f8899;font-weight:normal}
.note{color:#7f8899;font-size:11.5px;max-width:980px}
.hm td{text-align:center;min-width:52px}
.hm .ok{color:#4a5568}.hm .hot{background:#4a1d1d;color:#ff8a8a;font-weight:bold}
.hm .nan{background:#5a1010;color:#ff5a5a;font-weight:bold}
.grid{display:flex;flex-wrap:wrap;gap:18px}
.big{font-size:22px;color:#7ddf7d}.bad{font-size:22px;color:#ff5a5a}
code{color:#ffc24b}</style></head><body>
<h1>Full mod-matrix sweep — every routing in the mod lab</h1>
<div class="note">12 sources × 9 destinations × 2 polarities = <b>216 routings</b>, each rendered from a
FRESH engine for ${BLOCKS} blocks (~2.3 s) at note A2. Cell = the larger peak of the two polarities.
Bench calibrated so every destination has somewhere to go: rotor n = 8, chorus/phaser mix 0.6,
cutoff parked mid-range, one corner-scoped routing present.
<b>The "before" grid is measured</b>, by patching the pre-fix <code>frac</code> logic back into an
in-memory copy of the lab — not asserted.</div>

<h2>Before → after</h2>
<div class="grid">
<div><div class="note">non-finite cells</div><div class="${cb.nan ? 'bad' : 'big'}">${cb.nan} &rarr; <span class="big">${cf.nan}</span></div></div>
<div><div class="note">level blow-up cells (&gt;12 dB over baseline)</div><div class="${cb.hot ? 'bad' : 'big'}">${cb.hot} &rarr; <span class="big">${cf.hot}</span></div></div>
</div>

<h2>Peak level per routing — pre-fix</h2>
<div class="grid">${grid(bug, 'before: index wrapped, frac not')}</div>
<h2>Peak level per routing — after the frac fix</h2>
<div class="grid">${grid(fix, 'after: rd wrapped before either use')}</div>

<h2>The defect</h2>
<div class="note">The earlier crash fix guarded the read <b>index</b> but still derived <code>frac</code>
from the un-wrapped <code>rd</code>. The exactly-<code>len</code> case then gave <code>i0 = 0</code> with
<code>frac = 8192</code> — no NaN, but an 8192× interpolator extrapolation. Captured live at the
failing sample of <code>K3 → choDep</code>: neighbours <code>-0.13589</code> and <code>-0.13212</code>
produced <code>v = 30.73</code>, and the chorus stage output <b>8.99</b> against a synth peak of
<b>0.49</b>. Every hot cell above is a <code>choDep</code> routing, which is what localized it.</div>

<h2>Still open — one dead routing (design question, not a bug)</h2>
<div class="note"><code>R → Kboost</code> at positive depth is bit-identical to no routing.
<code>Kboost</code> is half-wave rectified (<code>8 * max(0, kbMod)</code>) and <code>R</code> is mapped
bipolar (<code>R*2-1</code>); below the coupling knee the swarm never locks, so the source is always
negative and the rectifier zeroes it. It revives exactly where R crosses 0.5:</div>
<table><tr><th>rotor K</th><th>detune</th><th>max R</th><th>+1 depth</th></tr>
<tr><td>0.35 (default)</td><td>0.3</td><td>0.334</td><td style="color:#ff5a5a">DEAD</td></tr>
<tr><td>1.0</td><td>0.3</td><td>0.996</td><td style="color:#7ddf7d">alive</td></tr>
<tr><td>0.35</td><td>0.05</td><td>0.984</td><td style="color:#7ddf7d">alive</td></tr></table>
<div class="note">Raised as register item <b>A9</b> for a ruling — per-route unipolar/bipolar setting,
unrectified Kboost, or leave it as physics — rather than fixed unilaterally.</div>
</body></html>`;
writeFileSync(resolve(root, 'docs/reports/2026-08-05-mod-matrix-sweep.html'), doc);
console.log(`wrote docs/reports/2026-08-05-mod-matrix-sweep.html  (before ${cb.nan} NaN / ${cb.hot} hot -> after ${cf.nan} / ${cf.hot})`);
