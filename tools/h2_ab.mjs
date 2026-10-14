/*
 * h2_ab.mjs — an A/B set between two builds of horde 2's engine, rendered offline, that the
 * human can play. ROADMAP B488 step (0), ADR-220 item 2: "Each PR carries an A/B set the
 * human can play: the same phrases on the same presets rendered by the engine before and
 * after, level-matched, with the difference measured beside each pair."
 *
 * WIRED: ./verify fast (the controls, `--self-test`; ./verify full runs them on the engine)
 *
 * It takes two h2_harness_render binaries (h2/harness/README.md, "Building"): the A/B is
 * between two ENGINE BUILDS (main and a branch), never two settings of one build.
 *
 *   node tools/h2_ab.mjs --a <binA> --b <binB> [--out <dir>] [--presets "Name,Name,defaults"]
 *                        [--os 1|2] [--a-sets k=v,k=v] [--b-sets k=v,k=v]
 *   node tools/h2_ab.mjs --self-test [--bin <h2_harness_render>]
 *
 * --out         where the set goes (default build-ab/ under the repo root, which is gitignored).
 *               Writes wav/*.wav, index.html (the page), summary.md, pairs.jsonl. Never commit them.
 * --a-sets,
 * --b-sets      extra settings written on ONE side only, after the fixed ones, e.g. a flag the
 *               B build adds and A does not have: --b-sets spreadRamp=1. A key the engine does
 *               not know stops the run (summary.unknownKeys), so a misspelt flag is never a
 *               pair of identical renders that reads as "no difference".
 *
 * THE PHRASES (fixed, here, PHRASES): 48 kHz, velocity 0.85, host seed 1, host blocks of 128,
 * a release tail of 3 s after the last key-up (the pads release over 3 s).
 *   note   one held note, C3, 4 s
 *   chord  a held chord, C3 E3 G3 B3, 4 s
 *   line   16 detached notes, 8 a second (0.125 s apart, each gate 0.09 s), around C4
 *   steal  a progression of four 5-note chords, each held 1.6 s and struck 0.1 s after the
 *          last was released (tools/h2_steal_fade_check.mjs's shape). At a pool of 8 the second
 *          chord already finds only 3 free voices, so it takes the first chord's tails
 *          wherever a preset's release is longer than 0.1 s; with a short release nothing is
 *          taken, and that is the preset's behaviour, not the phrase's.
 * THE PRESETS: the five heaviest of docs/port/cpu-ledger.md (Glass horde pad, Breathing pad,
 *   Crushed bells, Crush vs FM, Sync over fold), then Quarter sync and "defaults" (the oracle
 *   defaults, no preset) as light references. Read from the bank
 *   (reference/scalpel/data/presets.json, which is protected: read only) and played the way
 *   h2/harness/patches.json plays a ledger patch (params in the bank's order, then gain 0.35).
 * THE SETTINGS, written explicitly on every job: os 1, frameHold 1, stealFade 1 (set before
 *   `snap`) and poly 8 (after it, as the ledger's cell does): as the installed preview plays.
 *   --os 2 changes the first.
 *
 * WHAT IS MEASURED, per pair (pairMetrics; the page and summary.md only show it):
 *   peak and rms of A and of B in dBFS (both channels together); gainBDb, the gain B would need
 *   to match A's rms (the WAVs are written UNMATCHED; the page applies it at play time);
 *   diffDb, rms(B - A) / rms(A) in dB (-inf when the two are the same samples); diffPeakDb,
 *   max |B - A| in dBFS; identical (every float64 sample equal, not the quantised WAV) and
 *   firstDiffSample.
 *
 * THE PAGE plays the WAVs through plain <audio> elements, volume only, so it works opened from
 * file:// (a fetch() or a media-element source of a file:// URL is blocked or silent in the
 * browsers, and tools/serve_labs.py serves only the lab trees, never a build directory). A
 * media element's volume cannot go above 1, so with the level match on the louder side is
 * turned DOWN by the measured gain instead of B being turned up: the pair is matched, the
 * pair's absolute level is not.
 *
 * THE CONTROLS (--self-test; each prints PASS or FAIL, any FAIL exits 1; with a binary they run
 * on the engine, without one on a synthetic signal and the engine half prints SKIP):
 *   (a) the same binary as A and as B: every pair bit-identical, difference -inf, no first
 *       differing sample. Its must-read-nonzero: one sample moved by one ulp reads as different,
 *       at that sample.
 *   (b) a known difference: B = A x 0.5 through the same metrics code reads -6.02 dB (+- 0.01)
 *       and a level match of +6.02 dB (+- 0.01).
 *   (c) the WAV writer round-trips: write, read back with this file's reader, equal bit for bit.
 *   (d) the page's Switch moves the volume and nothing else (pageSwitchProbe: the page's script in a
 *       stub DOM with audio elements; no seek, play or pause on Switch, the new element already within
 *       20 ms of the other). Must-fail: the old Switch (a seek) and a planted seek to 0 read red.
 *
 * Deterministic but for the render time printed on stdout. No dependency but Node's own and
 * h2/harness/harness.mjs.
 */
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { basename, join, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { ROOT, binPath, jobFor, loadPatches, mtof, run } from '../h2/harness/harness.mjs';

const SR = 48000, SEED = 1, VEL = 0.85, BLOCK = 128, PRESET_GAIN = 0.35;
const BANK = 'reference/scalpel/data/presets.json';
const sec = s => Math.round(s * SR);
const on = n => ['on', n, mtof(n), VEL], off = n => ['off', n];
const TAIL = ['render', sec(3)];

/* ---------------------------------------------------------------- the fixed set */
const kLine = [60, 64, 67, 72, 71, 67, 64, 62, 60, 65, 69, 72, 69, 65, 62, 59];
const kProgression = [[48, 52, 55, 59, 62], [53, 57, 60, 64, 67], [57, 60, 64, 67, 71], [55, 59, 62, 65, 69]];
const held = (notes, seconds) => [...notes.map(on), ['render', sec(seconds)], ...notes.map(off), TAIL];
export const PHRASES = [
  { id: 'note', what: 'one held note, C3, 4 s', cmds: () => held([48], 4) },
  { id: 'chord', what: 'a held chord, C3 E3 G3 B3, 4 s', cmds: () => held([48, 52, 55, 59], 4) },
  { id: 'line', what: '16 detached notes, 8 a second, around C4', cmds: () => {
    const c = [];
    kLine.forEach((n, i) => { c.push(on(n), ['render', sec(0.09)], off(n)); if (i < kLine.length - 1) c.push(['render', sec(0.035)]); });
    return [...c, TAIL];
  } },
  { id: 'steal', what: 'four 5-note chords, each held 1.6 s, struck 0.1 s after the last release (the pool of 8 steals)', cmds: () => {
    const c = [];
    kProgression.forEach((ch, i) => { c.push(...ch.map(on), ['render', sec(1.6)], ...ch.map(off)); if (i < kProgression.length - 1) c.push(['render', sec(0.1)]); });
    return [...c, TAIL];
  } },
];
// the five heaviest of the frozen CPU ledger, then two light references
export const DEFAULT_PRESETS = ['Glass horde pad', 'Breathing pad', 'Crushed bells', 'Crush vs FM', 'Sync over fold', 'Quarter sync', 'defaults'];

/* A preset as `set` commands: the bank's params in its order, then gain 0.35; "defaults" is no preset.
   Where h2/harness/patches.json holds the same preset it must agree to the value (one source, no drift). */
let bankCache = null;
export function presetSets(name) {
  if (name === 'defaults') return [];
  bankCache = bankCache || JSON.parse(readFileSync(join(ROOT, BANK), 'utf8')).presets;
  const hits = bankCache.filter(p => p.name === name);
  if (hits.length !== 1) throw new Error(`h2_ab: preset "${name}" is in ${BANK} ${hits.length} times (want 1)`);
  const sets = [...Object.entries(hits[0].params), ['gain', PRESET_GAIN]];
  const ledger = loadPatches().patches.find(p => p.kind === 'ledger' && p.name === name);
  if (ledger && JSON.stringify(ledger.sets) !== JSON.stringify(sets)) throw new Error(`h2_ab: "${name}" differs between ${BANK} and h2/harness/patches.json (regenerate it)`);
  return sets;
}
const setCmd = ([k, v]) => (typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
const slug = s => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');

/* one side's jobs, in (preset, phrase) order */
function jobsFor(presets, settings) {
  const jobs = [];
  for (const preset of presets) for (const ph of PHRASES) {
    const cmds = [...presetSets(preset).map(setCmd), ...Object.entries(settings.before).map(setCmd), ['snap'], setCmd(['poly', settings.poly]), ...ph.cmds()];
    jobs.push(jobFor({ engine: 'composed', sr: SR, seed: SEED, block: BLOCK, cmds, name: `${preset} / ${ph.id}` }));
  }
  return jobs;
}

/* ---------------------------------------------------------------- the measurement */
const dB = ratio => (ratio > 0 ? 10 * Math.log10(ratio) : -Infinity);   // of a POWER ratio
const round3 = x => (Number.isFinite(x) ? Math.round(x * 1000) / 1000 : x);
/* A and B are {L, R} (Float64Array). Both channels count as one signal, as tools/h2_steal_fade_check.mjs's level(). */
export function pairMetrics(A, B) {
  const n = A.L.length;
  if (B.L.length !== n || A.R.length !== n || B.R.length !== n) throw new Error(`h2_ab: A has ${n} frames and B has ${B.L.length}: the two scripts differ`);
  let eA = 0, eB = 0, eD = 0, pA = 0, pB = 0, pD = 0, first = -1;
  for (let i = 0; i < n; i++) {
    for (const [a, b] of [[A.L[i], B.L[i]], [A.R[i], B.R[i]]]) {
      if (!Number.isFinite(a) || !Number.isFinite(b)) throw new Error(`h2_ab: a non-finite sample at frame ${i} (a render with a NaN is not a pair to listen to)`);
      const d = b - a;
      eA += a * a; eB += b * b; eD += d * d;
      if (Math.abs(a) > pA) pA = Math.abs(a);
      if (Math.abs(b) > pB) pB = Math.abs(b);
      if (Math.abs(d) > pD) pD = Math.abs(d);
      if (first < 0 && a !== b) first = i;
    }
  }
  const identical = first < 0, N2 = 2 * n;
  const gainBDb = eA > 0 && eB > 0 ? dB(eA / eB) : null;   // rms(A) / rms(B), in dB
  return {
    aPeakDb: 20 * Math.log10(pA), aRmsDb: dB(eA / N2), bPeakDb: 20 * Math.log10(pB), bRmsDb: dB(eB / N2),
    gainBDb, gainB: gainBDb === null ? null : Math.pow(10, gainBDb / 20),
    diffDb: identical ? -Infinity : (eA > 0 ? dB(eD / eA) : Infinity), diffPeakDb: 20 * Math.log10(pD),
    identical, firstDiffSample: first, frames: n,
  };
}

/* ---------------------------------------------------------------- WAV: 24-bit PCM, stereo */
const FULL = 8388608;   // 2^23
const q24 = x => { const q = Math.round(x * FULL); return q > FULL - 1 ? FULL - 1 : q < -FULL ? -FULL : q; };
export function encodeWav(L, R, sr) {
  const n = L.length, data = n * 6, buf = Buffer.alloc(44 + data);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + data, 4); buf.write('WAVEfmt ', 8); buf.writeUInt32LE(16, 16);
  buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22); buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 6, 28);
  buf.writeUInt16LE(6, 32); buf.writeUInt16LE(24, 34); buf.write('data', 36); buf.writeUInt32LE(data, 40);
  for (let i = 0, p = 44; i < n; i++, p += 6) { buf.writeIntLE(q24(L[i]), p, 3); buf.writeIntLE(q24(R[i]), p + 3, 3); }
  return buf;
}
export function decodeWav(buf) {
  if (buf.toString('latin1', 0, 4) !== 'RIFF' || buf.toString('latin1', 8, 12) !== 'WAVE') throw new Error('h2_ab: not a RIFF/WAVE file');
  let p = 12, fmt = null, data = null;
  while (p + 8 <= buf.length) {
    const id = buf.toString('latin1', p, p + 4), size = buf.readUInt32LE(p + 4);
    if (id === 'fmt ') fmt = { tag: buf.readUInt16LE(p + 8), ch: buf.readUInt16LE(p + 10), sr: buf.readUInt32LE(p + 12), bits: buf.readUInt16LE(p + 22) };
    else if (id === 'data') data = buf.subarray(p + 8, p + 8 + size);
    p += 8 + size + (size & 1);
  }
  if (!fmt || !data) throw new Error('h2_ab: a WAV with no fmt or data chunk');
  if (fmt.tag !== 1 || fmt.ch !== 2 || fmt.bits !== 24) throw new Error(`h2_ab: this reader takes 24-bit stereo PCM only (read tag ${fmt.tag}, ${fmt.ch} channels, ${fmt.bits} bits)`);
  const n = data.length / 6, L = new Float64Array(n), R = new Float64Array(n);
  for (let i = 0, q = 0; i < n; i++, q += 6) { L[i] = data.readIntLE(q, 3) / FULL; R[i] = data.readIntLE(q + 3, 3) / FULL; }
  return { sr: fmt.sr, L, R };
}

/* ---------------------------------------------------------------- building a set */
export function parseSets(text) {
  const out = {};
  for (const kv of (text || '').split(',').filter(Boolean)) {
    const i = kv.indexOf('=');
    if (i < 1) throw new Error(`h2_ab: "${kv}" is not key=value`);
    const v = kv.slice(i + 1);
    out[kv.slice(0, i)] = v !== '' && Number.isFinite(Number(v)) ? Number(v) : v;
  }
  return out;
}
const hashOf = path => createHash('sha256').update(readFileSync(path)).digest('hex');
const identity = path => ({ id: path.split(sep).slice(-2).join('/'), sha256: hashOf(path) });

/* Renders every (preset, phrase) on both builds -> { pairs: [{ preset, phrase, A, B, m }], ... } */
export function makeSet({ a, b, presets, os, aSets, bSets }) {
  for (const [side, bin] of [['A', a], ['B', b]]) if (!existsSync(bin)) throw new Error(`h2_ab: build ${side} (${bin}) is not built (h2/harness/README.md, "Building")`);
  const fixed = { os, frameHold: 1, stealFade: 1 };
  const side = extra => ({ before: { ...fixed, ...extra }, poly: 8 });
  const sA = side(aSets), sB = side(bSets);
  const ra = run(jobsFor(presets, sA), { bin: a }), rb = run(jobsFor(presets, sB), { bin: b });
  for (const [label, rs] of [['A', ra], ['B', rb]]) {
    const bad = rs.find(r => r.summary.unknownKeys);
    if (bad) throw new Error(`h2_ab: build ${label} did not know the key "${bad.summary.firstUnknown}" (job "${bad.name}"); a setting the build lacks would render the same sound twice`);
  }
  const pairs = [];
  let k = 0;
  for (const preset of presets) for (const ph of PHRASES) {
    const A = ra[k], B = rb[k]; k++;
    pairs.push({ preset, phrase: ph, A, B, m: pairMetrics(A, B) });
  }
  return { pairs, sets: { A: sA, B: sB }, builds: { A: identity(a), B: identity(b) } };
}

/* ---------------------------------------------------------------- the outputs */
const fmt = (x, d = 2) => (x === -Infinity ? '-inf' : x === Infinity ? '+inf' : x === null ? 'n/a' : x.toFixed(d));
// JSON has no infinities: a silent render's peak is -inf dBFS, and a null would read as 'not measured'
const jsonInf = (k, v) => (typeof v === 'number' && !Number.isFinite(v) ? (v > 0 ? '+inf' : v < 0 ? '-inf' : 'nan') : v);
const loudestFirst = pairs => [...pairs].sort((p, q) => q.m.diffDb - p.m.diffDb);   // -inf last; stable otherwise
const describeSets = s => `os ${s.before.os}, frameHold ${s.before.frameHold}, stealFade ${s.before.stealFade}, poly ${s.poly}` +
  Object.entries(s.before).filter(([k]) => !['os', 'frameHold', 'stealFade'].includes(k)).map(([k, v]) => `, ${k} ${v}`).join('');

export function summaryTable(pairs) {
  const head = '| preset | phrase | diff vs A (dB) | diff peak (dBFS) | A rms (dBFS) | B rms (dBFS) | B gain to match (dB) | first differing sample |\n|---|---|---:|---:|---:|---:|---:|---:|';
  const rows = loudestFirst(pairs).map(({ preset, phrase, m }) =>
    `| ${preset} | ${phrase.id} | ${fmt(m.diffDb)} | ${fmt(m.diffPeakDb)} | ${fmt(m.aRmsDb)} | ${fmt(m.bRmsDb)} | ${fmt(m.gainBDb)} | ${m.identical ? 'none (identical)' : m.firstDiffSample} |`);
  return `${head}\n${rows.join('\n')}`;
}

const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const PAGE_CSS = `:root{color-scheme:light dark;--bg:#fafafa;--fg:#1c1c1c;--dim:#666;--line:#d8d8d8;--acc:#1f6feb;--hot:#c2410c}
@media (prefers-color-scheme:dark){:root{--bg:#161616;--fg:#e8e8e8;--dim:#9a9a9a;--line:#333;--acc:#58a6ff;--hot:#fb923c}}
body{font:15px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg);margin:0 auto;padding:1rem 1.25rem;max-width:72rem}
h1{font-size:1.2rem;margin:.2rem 0}code{font-family:ui-monospace,Menlo,monospace;font-size:.9em}
.dim{color:var(--dim)}table{border-collapse:collapse;width:100%}th,td{border-bottom:1px solid var(--line);padding:.4rem .5rem;text-align:left;vertical-align:middle}
th{font-weight:600;color:var(--dim);font-size:.85rem}tr.grp td{font-weight:600;padding-top:1rem;border-bottom:2px solid var(--line)}
button{font:inherit;padding:.2rem .7rem;border:1px solid var(--line);background:transparent;color:var(--fg);border-radius:4px;cursor:pointer}
button:hover{border-color:var(--acc)}.which{display:inline-block;width:1.6rem;text-align:center;font-weight:700;color:var(--acc)}
.which.b{color:var(--hot)}.seek{width:11rem;vertical-align:middle}.num{font-variant-numeric:tabular-nums;white-space:nowrap}
.same{color:var(--dim)}.bar{display:flex;gap:1.5rem;align-items:center;flex-wrap:wrap;margin:.6rem 0}`;
/* THE PAGE'S SCRIPT is this function's source, shipped as `(pageMain)(document)`. It is a real function, not a
   string, so the Switch probe below can run it against a stub DOM without an evaluator (tools/labharness/
   sandbox_check.mjs flags any `new Function`/vm/eval/non-literal import() under tools/). */
export function pageMain(document) {
const match=document.getElementById('match');
const rows=[...document.querySelectorAll('tr.row')].map(tr=>{
  const r={tr,a:tr.querySelector('audio.a'),b:tr.querySelector('audio.b'),which:'A',gain:parseFloat(tr.dataset.gain)||1,
    pp:tr.querySelector('.pp'),sw:tr.querySelector('.sw'),seek:tr.querySelector('.seek'),t:tr.querySelector('.t'),lab:tr.querySelector('.which')};
  return r;});
const audible=r=>r.which==='A'?r.a:r.b;
const dur=r=>isFinite(r.a.duration)?r.a.duration:0;
// volume cannot exceed 1: with the match on, the louder side is turned down by the measured gain (B is matched to A)
function levels(r){const g=match.checked?r.gain:1;let va=1,vb=g;if(vb>1){va=1/vb;vb=1;}
  r.a.volume=r.which==='A'?va:0;r.b.volume=r.which==='B'?vb:0;}
function show(r){const d=dur(r),c=audible(r).currentTime;r.t.textContent=c.toFixed(1)+' / '+d.toFixed(1)+' s';r.seek.value=d?Math.round(1000*c/d):0;}
// the silent element follows the audible one, never the other way round; a seek on the silent one cannot be heard
function align(r){const au=audible(r),hid=au===r.a?r.b:r.a;if(!au.paused&&Math.abs(hid.currentTime-au.currentTime)>0.02)hid.currentTime=au.currentTime;}
function stop(r){r.a.pause();r.b.pause();r.pp.textContent='Play';}
rows.forEach(r=>{
  levels(r);
  r.pp.onclick=()=>{
    if(!r.a.paused){stop(r);return;}
    rows.forEach(o=>{if(o!==r)stop(o);});
    levels(r);{const au=audible(r);(au===r.a?r.b:r.a).currentTime=au.currentTime;}   // both paused: line the silent one up with the audible one
    Promise.all([r.a.play(),r.b.play()]).then(()=>{r.pp.textContent='Pause';}).catch(e=>{r.t.textContent=String(e);});};
  // SWITCH ONLY MOVES THE VOLUME. Both elements play in step all the time; a seek on a playing element is
  // asynchronous and landed at a stale time (the lead's browser run: A 1.23 s, B 0.45 s after one click).
  r.sw.onclick=()=>{
    r.which=r.which==='A'?'B':'A';
    levels(r);r.lab.textContent=r.which;r.lab.classList.toggle('b',r.which==='B');};
  r.seek.oninput=()=>{const t=dur(r)*r.seek.value/1000;r.a.currentTime=t;r.b.currentTime=t;show(r);};
  r.a.ontimeupdate=r.b.ontimeupdate=()=>{show(r);align(r);};
  r.a.onended=r.b.onended=()=>{stop(r);r.a.currentTime=0;r.b.currentTime=0;show(r);};
  r.a.onloadedmetadata=()=>show(r);
});
match.onchange=()=>rows.forEach(levels);
document.getElementById('stop').onclick=()=>rows.forEach(stop);
}
export const PAGE_JS = `(${pageMain.toString()})(document);`;

export function pageHtml(set, pairs, files, osText) {
  const builds = ['A', 'B'].map(s => `<tr><td><b>${s}</b></td><td><code>${esc(set.builds[s].id)}</code></td><td><code>${set.builds[s].sha256.slice(0, 16)}</code></td><td class="dim">${esc(describeSets(set.sets[s]))}</td></tr>`).join('');
  let lastPreset = null;
  const body = pairs.map(({ preset, phrase, m }, i) => {
    const grp = preset !== lastPreset ? `<tr class="grp"><td colspan="3">${esc(preset)}</td></tr>` : '';
    lastPreset = preset;
    const diff = m.identical ? '<span class="same">identical, bit for bit</span>'
      : `<b>${fmt(m.diffDb)} dB</b> <span class="dim">against A; peak ${fmt(m.diffPeakDb)} dBFS; B ${fmt(m.gainBDb)} dB to match</span>`;
    return `${grp}<tr class="row" data-gain="${m.gainB === null ? 1 : m.gainB}"><td>${esc(phrase.id)}<div class="dim">${esc(phrase.what)}</div></td>` +
      `<td>${diff}</td><td class="num"><button class="pp">Play</button> <button class="sw">Switch</button> <span class="which">A</span> ` +
      `<input type="range" class="seek" min="0" max="1000" value="0"> <span class="t dim">0.0 / 0.0 s</span>` +
      `<audio class="a" preload="metadata" src="${esc(files[i].a)}"></audio><audio class="b" preload="metadata" src="${esc(files[i].b)}"></audio></td></tr>`;
  }).join('\n');
  return `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>horde 2 engine A/B</title><style>${PAGE_CSS}</style></head><body>
<h1>horde 2 engine A/B</h1>
<p class="dim">Rendered offline by the C++ engine (tools/h2_ab.mjs): 48 kHz, velocity 0.85, host seed 1, ${esc(osText)}. Each row plays A and B at one position; Switch flips between them without moving.
Difference = rms(B - A) / rms(A), in dB: the lower, the nearer. The WAV files are unmatched; the level match is applied when playing.</p>
<table><tr><th></th><th>build</th><th>sha-256</th><th>settings</th></tr>${builds}</table>
<div class="bar"><label><input type="checkbox" id="match" checked> match levels (B to A, by the measured gain)</label><button id="stop">Stop all</button></div>
<table><tr><th>phrase</th><th>difference</th><th>play</th></tr>
${body}
</table>
<script>${PAGE_JS}</script></body></html>
`;
}

export function writeSet(set, outDir) {
  const wavDir = join(outDir, 'wav');
  mkdirSync(wavDir, { recursive: true });
  const files = set.pairs.map(({ preset, phrase, A, B }) => {
    const stem = `wav/${slug(preset)}__${phrase.id}`;
    writeFileSync(join(outDir, `${stem}__A.wav`), encodeWav(A.L, A.R, SR));
    writeFileSync(join(outDir, `${stem}__B.wav`), encodeWav(B.L, B.R, SR));
    return { a: `${stem}__A.wav`, b: `${stem}__B.wav` };
  });
  const lines = set.pairs.map(({ preset, phrase, m }) => JSON.stringify({
    preset, phrase: phrase.id, aPeakDb: round3(m.aPeakDb), aRmsDb: round3(m.aRmsDb), bPeakDb: round3(m.bPeakDb), bRmsDb: round3(m.bRmsDb),
    gainBDb: round3(m.gainBDb), diffDb: round3(m.diffDb), diffPeakDb: round3(m.diffPeakDb), identical: m.identical, firstDiffSample: m.identical ? null : m.firstDiffSample,
  }, jsonInf));
  writeFileSync(join(outDir, 'pairs.jsonl'), lines.join('\n') + '\n');
  const osText = `os ${set.sets.A.before.os}`;
  writeFileSync(join(outDir, 'index.html'), pageHtml(set, set.pairs, files, osText));
  const n = set.pairs.length, same = set.pairs.filter(p => p.m.identical).length;
  const md = `# horde 2 engine A/B\n\n- A: \`${set.builds.A.id}\` sha-256 \`${set.builds.A.sha256}\`; ${describeSets(set.sets.A)}\n- B: \`${set.builds.B.id}\` sha-256 \`${set.builds.B.sha256}\`; ${describeSets(set.sets.B)}\n` +
    `- ${n} pairs, ${same} bit-identical. Difference = rms(B - A) / rms(A); loudest difference first.\n\n${summaryTable(set.pairs)}\n`;
  writeFileSync(join(outDir, 'summary.md'), md);
  return md;
}

/* ---------------------------------------------------------------- the controls */
const SELF_PRESETS = ['Breathing pad'];   // a heavy pad whose noise blade draws from the shared stream: all four phrases
function synthetic() {
  const n = SR, L = new Float64Array(n), R = new Float64Array(n);
  for (let i = 0; i < n; i++) { const t = i / SR; L[i] = 0.3 * Math.sin(2 * Math.PI * 220 * t) + 0.1 * Math.sin(2 * Math.PI * 331 * t); R[i] = 0.25 * Math.sin(2 * Math.PI * 261.6 * t + 1); }
  return { L, R };
}
/* The page's script (pageMain) against a stub DOM (no browser here): Play, let time pass, let the silent element
   drift, Switch. Returns { problems, text }. Switch must move the volumes and nothing else: no seek, play or
   pause (a seek on a playing element is asynchronous, and landed at a stale time in the browser), and the
   newly audible element must already be where the other one is, within 20 ms. */
export function pageSwitchProbe(main, afterSwitch) {
  const log = [];
  const audio = name => {
    let v = 1;
    const el = { name, paused: true, duration: 7, t: 0, onended: null, ontimeupdate: null, onloadedmetadata: null,
      get currentTime() { return el.t; }, set currentTime(x) { el.t = x; log.push(`seek ${name} to ${x.toFixed(3)}`); },
      get volume() { return v; }, set volume(x) { v = x; },
      play() { el.paused = false; log.push(`play ${name}`); return Promise.resolve(); }, pause() { el.paused = true; log.push(`pause ${name}`); } };
    return el;
  };
  const A = audio('A'), B = audio('B');
  const plain = () => ({ textContent: '', value: 0, onclick: null, oninput: null, classList: { toggle() {} } });
  const pp = plain(), sw = plain(), seek = plain(), t = plain(), lab = plain();
  const byClass = { 'audio.a': A, 'audio.b': B, '.pp': pp, '.sw': sw, '.seek': seek, '.t': t, '.which': lab };
  const tr = { dataset: { gain: '1.5' }, querySelector: q => byClass[q] };
  const match = { checked: true, onchange: null }, stop = plain();
  const doc = { getElementById: id => (id === 'match' ? match : stop), querySelectorAll: () => [tr] };
  main(doc);
  const advance = dt => { for (const el of [A, B]) if (!el.paused) el.t += dt; };   // the clock, not a seek
  pp.onclick();                         // Play
  advance(0.74); A.ontimeupdate();
  B.t += 0.1;                           // the silent one drifts ahead
  A.ontimeupdate();                     // the page may line the SILENT one up
  const drifted = Math.abs(A.t - B.t);
  advance(0.5);
  const mark = log.length;
  sw.onclick();                         // Switch
  if (afterSwitch) afterSwitch(A, B);   // a planted fault: what a wrong handler would have done next
  const ops = log.slice(mark), problems = [];
  if (ops.length) problems.push(`Switch issued ${ops.length} media call(s): ${ops.join('; ')}`);
  if (Math.abs(A.t - B.t) > 0.02) problems.push(`after Switch B is at ${B.t.toFixed(3)} s and A at ${A.t.toFixed(3)} s`);
  if (!(B.volume > 0 && A.volume === 0)) problems.push(`after Switch the volumes are A ${A.volume}, B ${B.volume} (B should be the audible one)`);
  if (drifted > 0.02) problems.push(`the silent element was left ${drifted.toFixed(3)} s out of step`);
  if (A.paused || B.paused) problems.push('Switch left an element paused');
  return { problems, text: `A ${A.t.toFixed(3)} s, B ${B.t.toFixed(3)} s after Switch; ${ops.length} media calls on Switch` };
}

export function selfTest(binOpt) {
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag} ${text}`); if (!ok) red++; };
  const bin = binPath(binOpt ? { bin: binOpt } : undefined);
  const haveBin = existsSync(bin);
  if (binOpt && !haveBin) throw new Error(`h2_ab: ${bin} is not built`);
  let base;   // the signal controls (b) and (c) work on
  if (haveBin) {
    const t0 = Date.now();
    const set = makeSet({ a: bin, b: bin, presets: SELF_PRESETS, os: 1, aSets: {}, bSets: {} });
    const same = set.pairs.every(p => p.m.identical && p.m.diffDb === -Infinity && p.m.firstDiffSample === -1);
    row(same && set.builds.A.sha256 === set.builds.B.sha256, '(a)', `the same binary as A and B (${basename(bin)}): ${set.pairs.filter(p => p.m.identical).length} of ${set.pairs.length} pairs bit-identical, ` +
      `difference ${fmt(Math.max(...set.pairs.map(p => p.m.diffDb)))} dB, first differing sample ${same ? 'none' : set.pairs.find(p => !p.m.identical).m.firstDiffSample} (${((Date.now() - t0) / 1000).toFixed(1)} s)`);
    base = set.pairs[0].A;
  } else {
    base = synthetic();
    const m = pairMetrics(base, synthetic());
    console.log(`SKIP  (a) on the engine: no h2_harness_render at ${bin.split(sep).slice(-2).join('/')}; ./verify full runs it. On a synthetic signal:`);
    row(m.identical && m.diffDb === -Infinity && m.firstDiffSample === -1, '(a)', `the same signal as A and B reads identical, difference ${fmt(m.diffDb)} dB`);
  }
  // (a)'s must-read-nonzero: one sample moved by one ulp must read as different, AT that sample
  const k = Math.floor(base.L.length / 2);
  const bumped = { L: Float64Array.from(base.L), R: base.R };
  bumped.L[k] = bumped.L[k] === 0 ? Number.MIN_VALUE : bumped.L[k] * (1 + Number.EPSILON);
  const mb = pairMetrics(base, bumped);
  row(!mb.identical && mb.firstDiffSample === k, '(a)', `its must-read-nonzero: one sample moved by one ulp at frame ${k} reads different, first differing sample ${mb.firstDiffSample}`);

  // (b) B = A x 0.5, through pairMetrics, the code every pair goes through
  const half = { L: base.L.map(x => x * 0.5), R: base.R.map(x => x * 0.5) };
  const mh = pairMetrics(base, half);
  row(Math.abs(mh.diffDb - -6.02) <= 0.01 && mh.gainBDb !== null && Math.abs(mh.gainBDb - 6.02) <= 0.01 && !mh.identical, '(b)',
    `B = A x 0.5 reads a difference of ${fmt(mh.diffDb, 3)} dB (required -6.02 +- 0.01) and a level match of ${fmt(mh.gainBDb, 3)} dB (required +6.02 +- 0.01)`);

  // (c) the WAV writer round-trips: samples on the 24-bit grid come back bit for bit
  const n = 4096, gl = new Float64Array(n), gr = new Float64Array(n);
  let s = 0x9e3779b9;
  const next = () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s; };
  for (let i = 0; i < n; i++) { gl[i] = (next() % (2 * FULL) - FULL) / FULL; gr[i] = (next() % (2 * FULL) - FULL) / FULL; }
  [gl[0], gl[1], gl[2], gl[3], gr[0], gr[1]] = [-1, (FULL - 1) / FULL, 0, 1 / FULL, -1 / FULL, 0];   // the rails, zero and one LSB
  const wav = encodeWav(gl, gr, SR), back = decodeWav(wav);
  const bits = (a, b) => a.length === b.length && a.every((x, i) => Object.is(x, b[i]));
  const again = encodeWav(back.L, back.R, SR);
  row(back.sr === SR && bits(gl, back.L) && bits(gr, back.R) && wav.equals(again), '(c)', `${n} frames written and read back: sample rate ${back.sr}, every sample bit for bit, and the re-written file byte for byte`);
  const rt = decodeWav(encodeWav(base.L, base.R, SR));
  row(base.L.every((x, i) => Math.abs(rt.L[i] - x) <= 0.5 / FULL + 1e-15), '(c)', `a ${haveBin ? 'rendered' : 'synthetic'} signal (${base.L.length} frames) comes back within half a 24-bit step`);
  // (d) the page's Switch moves the volume and nothing else. Its must-fail, on every run: what the old Switch did
  // (seek the new element to the other one's time) and a seek to 0, done right after the real handler, must each
  // read red. They are planted around the handler, not by editing it, so the probe's eyes are what is tested.
  const good = pageSwitchProbe(pageMain);
  row(!good.problems.length, '(d)', `the page's Switch, in a stub DOM: ${good.problems.length ? good.problems.join(' | ') : good.text}`);
  const plants = [['the old Switch (a seek of the new element to the other one\'s time)', (A, B) => { B.currentTime = A.currentTime; }],
    ['a seek to 0', (A, B) => { B.currentTime = 0; }]];
  for (const [what, plant] of plants) {
    const bad = pageSwitchProbe(pageMain, plant);
    row(bad.problems.length > 0, '(d)', `CONTROL ${what} reads: ${bad.problems.join(' | ') || 'GREEN (the probe is blind)'}`);
  }
  console.log(`h2_ab --self-test: ${red ? 'FAIL' : 'PASS'}`);
  return red ? 1 : 0;
}

/* ---------------------------------------------------------------- the command line */
function arg(argv, name) { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : undefined; }
export function main(argv) {
  if (argv.includes('--self-test')) return selfTest(arg(argv, '--bin'));
  const a = arg(argv, '--a'), b = arg(argv, '--b');
  if (!a || !b) { console.error('usage: node tools/h2_ab.mjs --a <binA> --b <binB> [--out <dir>] [--presets "A,B"] [--os 1|2] [--a-sets k=v,..] [--b-sets k=v,..]\n       node tools/h2_ab.mjs --self-test [--bin <h2_harness_render>]'); return 2; }
  const os = Number(arg(argv, '--os') || 1);
  if (os !== 1 && os !== 2) throw new Error('h2_ab: --os is 1 or 2');
  const presets = arg(argv, '--presets') ? arg(argv, '--presets').split(',') : DEFAULT_PRESETS;
  const out = resolve(arg(argv, '--out') || join(ROOT, 'build-ab'));
  const t0 = Date.now();
  const set = makeSet({ a: resolve(a), b: resolve(b), presets, os, aSets: parseSets(arg(argv, '--a-sets')), bSets: parseSets(arg(argv, '--b-sets')) });
  const md = writeSet(set, out);
  console.log(md);
  console.log(`rendered ${set.pairs.length} pairs (${2 * set.pairs.length} renders) in ${((Date.now() - t0) / 1000).toFixed(1)} s; written to ${out}: open index.html`);
  return 0;
}
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); } catch (e) { console.error(`h2_ab: ${e.message}`); process.exitCode = 2; }
}
