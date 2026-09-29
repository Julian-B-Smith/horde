/*
 * fidelity_audit.mjs — B325: the OFFLINE half of the DSP fidelity audit. HYPERSAW, 2026-09-28,
 * ROADMAP B325 (records PR #821, branch lead-records-128). The human: "I'm also starting to
 * notice more noise and clicks that I'm not certain are supposed to be part of the
 * waveforms ... it would be worth running some tests to make sure I'm not crazy."
 * UNWIRED: an audit run (minutes of DSP across workers, writing git-ignored local data), not
 * a check; its standing distillate is fidelity_scan_check.mjs, which IS wired.
 *
 * Two hypotheses, separated FIRST (B325): (a) PLAYBACK underruns, heard as clicks while the
 * render is clean; (b) RENDER artefacts in the samples themselves. This file answers (b) for
 * the 83 presets and the gauntlet's hotspots, through BOTH engines, and attributes every
 * click by ablation. The browser half (what the lab's worklet actually played) is a capture
 * harness, reported in docs/patchspace/2026-09-28-fidelity-audit.md.
 *
 *   node tools/patchspace/fidelity_audit.mjs presets   [--workers W]   the 83 presets × 4 phrases × 2 engines
 *   node tools/patchspace/fidelity_audit.mjs find      [--workers W]   the gauntlet's 3000 patches, TONAL + POLY
 *                                                                     through the composed engine (B316's seeds)
 *   node tools/patchspace/fidelity_audit.mjs hotspots  [--workers W]   the click/noise hotspots found × 4 phrases × 2 engines
 *   node tools/patchspace/fidelity_audit.mjs attribute [--workers W]   every click event × every applicable ablation
 *   node tools/patchspace/fidelity_audit.mjs wavs                      the worst examples, as WAVs, for listening
 *   node tools/patchspace/fidelity_audit.mjs alias     [--workers W]   B346: the chord's aliasing, both engines, fixed metric + the
 *                                                                     os-convergence estimator + the output stage (regenerates §5)
 *   node tools/patchspace/fidelity_audit.mjs summary                   the report's tables, as markdown, on stdout
 * Output: local/patchspace/fidelity/ (git-ignored). Every pass is RESUMABLE (one JSON line per
 * job; a re-run skips jobs already on disk) and prints progress every 25 jobs.
 *
 * MEASURED PER RENDER (fidelity.mjs states the methods):
 *   clicks     transient events of the local-baseline detector (> 20 dB over ±85 ms, residual
 *              over -90 dBFS), each with the engine event it sits on (onset, steal, retrigger,
 *              voice end, note-off, or mid-note); clicks10 is the same detector at 10 dB, a
 *              SENSITIVE count reported beside it, never used to attribute.
 *   steals     every start on a slot still active: its env, gate, and whether it restarted
 *              the phases (fresh) or reused them (the oracle's same-note reuse).
 *   noise      (chord phrase only) spectral flatness over the sustain, 0.25..1.15 s, and
 *              aliasing against the same engine at 4× the oversampling over a 0.6 s hold
 *              (B316's method and its stated caveat: the blade caps move with the oversampling).
 *   subnormal  output samples with 0 < |x| < 2^-126 (float32 subnormals): what a C++ port
 *              without flush-to-zero would carry.
 */
import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync, appendFileSync } from 'node:fs';
import { join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { cpus } from 'node:os';
import { ROOT, SR, render as spaceRender, engineParams } from './space.mjs';
import { spectrum, aliasing, mono, flatness, analyse, clicks as wholeClicks, rms } from './metrics.mjs';
import { samplePatch, A_SCRIPT, C_SCRIPT } from './gauntlet.mjs';
import { hash32 } from './gen_dependency_tree.mjs';
import { ABLATIONS, engineClass, ablationApplies, PHRASES, PRESETS, renderPhrase, clickEvents, nearestCause, sameEvent, clickEnergyDb, CLICK, wav, mtof, NEUTRAL, neutralPair, diffStats, dBFS } from './fidelity.mjs';
import { estimate, withCapture, renderWith } from './alias_sources.mjs';

export const OUT = join(ROOT, 'local', 'patchspace', 'fidelity');
const ENGINES = ['oracle', 'composed'];
const P3 = { seed: 0xB316, broad: 2000, edge: 1000 };       // B316's run p3 (docs/patchspace/2026-09-27-gauntlet-p3.md)
const HOT = { tonalClicks: 1, polyClicksTop: 40, noisyTop: 30, flat: 0.3 };
const NOISE_EXCESS = 0.02;                                  // composed − oracle chord flatness that earns a noise attribution
const NOISE_ABL = ['coupling', 'splitPhase', 'tick1', 'settledStart', 'noise', 'dcJitter'];
/* MECHANISMS measured by SAMPLE DIFFERENCE (the `mech` pass): each ablation removes one candidate
   and nothing else, so render − ablated render IS that mechanism's contribution, however masked it
   is in the mix. The oracle's own mechanisms on the oracle; the steal on both engines (B310's law
   decides how often the composed engine steals). */
const MECH = { oracle: ['crushExit', 'scanCol', 'dcJitter', 'endRamp', 'noSteal'], composed: ['noSteal', 'splitPhase', 'tick1'] };
const MECH_PHRASES = ['chord', 'arp'];

/* ---------------------------------------------------------------- subjects */
export function subjectParams(subj) {
  if (subj.preset) { const p = PRESETS.find(x => x.name === subj.preset); if (!p) throw new Error('preset missing: ' + subj.preset); return Object.assign({}, p.params, subj.over || {}); }
  return engineParams(samplePatch(P3.seed, subj.i, subj.mode).patch);
}
const subjName = s => (s.preset ? s.preset + (s.over ? ' + ' + Object.entries(s.over).map(([k, v]) => k + ' ' + v).join(', ') : '') : `${s.mode}#${s.i}`);
/* CONSTRUCTED subjects for the mechanism pass: no preset plays reflected Crush or Crush under
   collision, so the two Crush mechanisms B316 found would act nowhere in the 83. Each is the
   nearest preset with the one switch that engages the path. */
const CONSTRUCTED = [
  { preset: 'Slewed crush', over: { mirror: 1 }, why: 'reflected Crush, slewed (the exit jump at any slew)' },
  { preset: 'Fixed crush', over: { mirror: 1 }, why: 'reflected Crush, hard steps' },
  { preset: 'Crushed burst', over: { hard2: 0, colK: 0.8 }, why: 'upper blade hard Crush under collision → pitch (the scanner reads the collision accumulator)' },
];
const n10 = r => (Array.isArray(r.clicks10) ? r.clicks10.length : r.clicks10 || 0);

/* ---------------------------------------------------------------- one render, measured */
const SUBN = 1.1754943508222875e-38;
export function measureRender(kind, params, phrase, ablation) {
  const A = ablation ? ABLATIONS[ablation] : null;
  const over = A && A.patch ? A.patch : null;
  const r = renderPhrase(engineClass(kind, ablation), params, PHRASES[phrase], { over });
  const ev = clickEvents(r.L, r.R);
  const pack = e => ({ t: e.t, cause: nearestCause(e, r.log), excessDb: +e.excessDb.toFixed(2), levelDb: +e.levelDb.toFixed(2), energy: e.energy });
  const out = { clicks: ev.map(pack) };
  const thr = CLICK.ratioDb; CLICK.ratioDb = 10; out.clicks10 = clickEvents(r.L, r.R).map(pack); CLICK.ratioDb = thr;
  out.steals = r.log.filter(e => e.kind === 'start' && e.wasActive).map(e => ({ t: e.t, env: +e.env.toPrecision(4), gate: e.wasGate, fresh: e.fresh, cut: +e.cut.toPrecision(4) }));
  let nf = 0, sub = 0;
  for (const x of [r.L, r.R]) for (let i = 0; i < x.length; i++) { const a = Math.abs(x[i]); if (!Number.isFinite(x[i])) nf++; else if (a > 0 && a < SUBN) sub++; }
  out.nonFinite = nf; out.subnormal = sub;
  out.rmsDb = +(20 * Math.log10(Math.max(rms(r.L), rms(r.R), 1e-12))).toFixed(2);
  if (phrase === 'chord') {
    const a = Math.round(0.25 * SR), b = Math.round(1.15 * SR);
    const S = spectrum(mono(r.L.subarray(a, b), r.R.subarray(a, b)), SR);
    out.flatness = +flatness(S).toPrecision(4);
  }
  if (phrase === 'chord' && !ablation) {
    const hold = { n: Math.round(0.6 * SR / 128) * 128, ev: PHRASES.chord.ev.filter(e => e[1] === 'on') };
    const os = params.os || 2;
    const t = renderPhrase(engineClass(kind), params, hold), ref = renderPhrase(engineClass(kind), Object.assign({}, params, { os: os * 4 }), hold);
    const c0 = Math.round(0.1 * SR), S1 = spectrum(mono(t.L.subarray(c0), t.R.subarray(c0)), SR), S2 = spectrum(mono(ref.L.subarray(c0), ref.R.subarray(c0)), SR);
    out.aliasDb = +aliasing(S1, S2).aliasDb.toFixed(2);
  }
  return out;
}

/* ---------------------------------------------------------------- jobs */
function jobsFor(pass) {
  const J = [];
  if (pass === 'presets') for (const p of PRESETS) for (const ph of Object.keys(PHRASES)) for (const k of ENGINES) J.push({ id: `${p.name}|${ph}|${k}`, subj: { preset: p.name }, phrase: ph, kind: k });
  if (pass === 'find') {
    for (let i = 0; i < P3.broad; i++) J.push({ id: 'broad#' + i, subj: { mode: 'broad', i } });
    for (let i = 0; i < P3.edge; i++) J.push({ id: 'edge#' + i, subj: { mode: 'edge', i } });
  }
  if (pass === 'neutral') for (const p of PRESETS) for (const ph of Object.keys(PHRASES)) J.push({ id: `${p.name}|${ph}`, subj: { preset: p.name }, phrase: ph });
  if (pass === 'alias') for (const p of PRESETS) for (const k of ENGINES) J.push({ id: `${p.name}|${k}`, subj: { preset: p.name }, kind: k });
  if (pass === 'mech') {
    const subs = PRESETS.map(p => ({ preset: p.name })).concat(CONSTRUCTED.map(c => ({ preset: c.preset, over: c.over })));
    for (const sb of subs) for (const ph of MECH_PHRASES) for (const k of ENGINES) J.push({ id: `${subjName(sb)}|${ph}|${k}`, subj: sb, phrase: ph, kind: k });
  }
  if (pass === 'hotspots') for (const s of hotspotSubjects()) for (const ph of Object.keys(PHRASES)) for (const k of ENGINES) J.push({ id: `${subjName(s)}|${ph}|${k}`, subj: s, phrase: ph, kind: k });
  if (pass === 'attribute') {
    const seen = new Set(), add = (r, ab) => { const id = `${r.id}|${ab}`; if (seen.has(id) || !ablationApplies(r.kind, ab)) return; seen.add(id); J.push({ id, base: r.id, subj: r.subj, phrase: r.phrase, kind: r.kind, ablation: ab }); };
    for (const f of ['presets', 'hotspots']) {
      const rows = readPass(f);
      for (const r of rows.values()) {
        /* 20 dB events, and the SENSITIVE 10 dB ones (attributed separately, flagged): the
           presets pass stored the 10 dB count only, so a 'none' job re-measures the list */
        if ((r.clicks && r.clicks.length) || n10(r)) { for (const ab of Object.keys(ABLATIONS)) add(r, ab); if (typeof r.clicks10 === 'number') add(r, 'none'); }
        /* NOISE: a chord render whose composed flatness exceeds the oracle's by more than
           NOISE_EXCESS gets the noise ablations, on BOTH engines (like against like) */
        if (r.phrase === 'chord') {
          const o = rows.get(r.id.replace(/\|composed$|\|oracle$/, '|oracle')), c = rows.get(r.id.replace(/\|composed$|\|oracle$/, '|composed'));
          if (o && c && c.flatness - o.flatness > NOISE_EXCESS) for (const ab of NOISE_ABL) add(r, ab);
        }
      }
    }
  }
  return J;
}
function runJob(pass, j) {
  if (pass === 'find') {
    /* the gauntlet's own TONAL and POLY segments and seed (gauntlet.mjs measure: pseed = hash32(seed, i, 77)) */
    const s = samplePatch(P3.seed, j.subj.i, j.subj.mode), pseed = hash32(P3.seed, j.subj.i, 77);
    const a = spaceRender(s.patch, A_SCRIPT, { seed: pseed, block: 512 });
    const m = analyse(a.L.subarray(4800, 24000), a.R.subarray(4800, 24000), SR, mtof(57));
    const c = spaceRender(s.patch, C_SCRIPT, { seed: pseed });
    const ck = wholeClicks(mono(c.L, c.R));
    return { clicks: m.clicks, flatness: m.flatness, silent: m.silent, clicksC: ck.clicks, clicksCWorstDb: +ck.worstDb.toFixed(2) };
  }
  if (pass === 'alias') {
    /* B346: the audit's own chord hold (the four note-ons, 0.6 s, window 0.1 s to the end), at the preset's
       os and 2x..16x it, through alias_sources.mjs estimate: the B345-fixed aliasing (b345Db, the same test
       and 4x reference as the pre-B345 number this replaces), the os-convergence estimator, the output
       stage and the total against the oversampled truth. Seeded as renderPhrase is (0xB325). */
    const params = subjectParams(j.subj), N = params.os || 2, C = capClass(j.kind);
    const hold = { n: Math.round(0.6 * SR / 128) * 128, ev: PHRASES.chord.ev.filter(e => e[1] === 'on') };
    const E = estimate((m, cap) => renderWith(C, Object.assign({}, params, { os: N * m }), hold, { seed: 0xB325, cap, raw: true }), N, [Math.round(0.1 * SR), hold.n], { truth: true });
    const r2 = x => Math.round(x * 100) / 100;
    return { os: N, aliasDb: r2(E.conv.b345Db), convDb: r2(E.conv.excessDb), cls: E.conv.cls, foldDb: r2(E.conv.foldDb), dynDb: r2(E.conv.dynDb), converge: r2(E.conv.convDb),
      decimLeakDb: r2(E.out.decimLeakDb), tanhFoldDb: r2(E.out.tanhFoldDb), totalDb: r2(E.totalDb) };
  }
  if (pass === 'neutral') {
    /* the sensitive layer (fidelity.mjs NEUTRAL): composed must equal the oracle, sample for sample */
    const P = neutralPair(), params = subjectParams(j.subj);
    const a = renderPhrase(P.oracle, params, PHRASES[j.phrase], { over: NEUTRAL }), b = renderPhrase(P.composed, params, PHRASES[j.phrase], { over: NEUTRAL });
    return diffStats(a, b);
  }
  if (pass === 'mech') {
    const params = subjectParams(j.subj), base = renderPhrase(engineClass(j.kind), params, PHRASES[j.phrase]), out = {};
    let peak = 0; for (const x of [base.L, base.R]) for (let i = 0; i < x.length; i++) peak = Math.max(peak, Math.abs(x[i]));
    out.peak = peak;
    for (const ab of MECH[j.kind]) {
      const A = ABLATIONS[ab], r = renderPhrase(engineClass(j.kind, ab), params, PHRASES[j.phrase], { over: A.patch || null });
      const d = diffStats(r, base);
      out[ab] = { max: d.max, step: d.step, at: d.at, n40: d.n40 };
    }
    out.steals = base.log.filter(e => e.kind === 'start' && e.wasActive && e.fresh).length;
    return out;
  }
  return measureRender(j.kind, subjectParams(j.subj), j.phrase, j.ablation && j.ablation !== 'none' ? j.ablation : null);
}

const CAPC = new Map();
const capClass = kind => { if (!CAPC.has(kind)) CAPC.set(kind, withCapture(engineClass(kind))); return CAPC.get(kind); };

/* ---------------------------------------------------------------- storage */
function dir() { mkdirSync(OUT, { recursive: true }); return OUT; }
export function readPass(pass) {
  const rows = new Map(), d = join(OUT, pass);
  if (!existsSync(d)) return rows;
  for (const f of readdirSync(d).filter(f => f.endsWith('.jsonl'))) for (const l of readFileSync(join(d, f), 'utf8').split('\n')) {
    if (!l) continue; let r; try { r = JSON.parse(l); } catch { continue; } rows.set(r.id, r);   // a torn last line is re-run
  }
  return rows;
}
export function hotspotSubjects() {
  const f = [...readPass('find').values()];
  if (!f.length) throw new Error('fidelity_audit: run the `find` pass first');
  const pick = new Map();
  for (const r of f) if (r.clicks >= HOT.tonalClicks) pick.set(r.id, 'tonal clicks');
  f.filter(r => r.clicksC > 0).sort((a, b) => b.clicksCWorstDb - a.clicksCWorstDb).slice(0, HOT.polyClicksTop).forEach(r => { if (!pick.has(r.id)) pick.set(r.id, 'poly clicks'); });
  f.filter(r => r.flatness !== null && r.flatness > HOT.flat).sort((a, b) => b.flatness - a.flatness).slice(0, HOT.noisyTop).forEach(r => { if (!pick.has(r.id)) pick.set(r.id, 'noisy'); });
  return [...pick.entries()].sort().map(([id, why]) => { const [mode, i] = id.split('#'); return { mode, i: +i, why }; });
}

/* ---------------------------------------------------------------- run */
function arg(name, def) { const i = process.argv.indexOf('--' + name); return i > 0 ? process.argv[i + 1] : def; }
if (!isMainThread && workerData && workerData.kind === 'fidelity') {
  for (const j of workerData.jobs) {
    const r = Object.assign({ id: j.id, subj: j.subj, phrase: j.phrase, kind: j.kind, ablation: j.ablation, base: j.base }, runJob(workerData.pass, j));
    parentPort.postMessage({ line: JSON.stringify(r) });
  }
  parentPort.postMessage({ done: true });
}
async function runPass(pass) {
  const workers = +arg('workers', Math.max(1, Math.min(5, cpus().length - 3)));
  const d = join(dir(), pass); mkdirSync(d, { recursive: true });
  const have = readPass(pass), jobs = jobsFor(pass).filter(j => !have.has(j.id));
  console.error(`fidelity ${pass}: ${have.size} on disk, ${jobs.length} to run on ${workers} workers`);
  if (!jobs.length) return;
  /* heavy jobs spread: interleave so each worker gets a share of every preset */
  const shares = Array.from({ length: workers }, (_, w) => jobs.filter((_, k) => k % workers === w));
  const t0 = Date.now(); let done = 0;
  await Promise.all(shares.map((share, w) => new Promise((res, rej) => {
    const file = join(d, `w${w}-${Date.now().toString(36)}.jsonl`);
    const wk = new Worker(new URL(import.meta.url), { workerData: { kind: 'fidelity', pass, jobs: share } });
    wk.on('message', m => {
      if (m.done) return res();
      appendFileSync(file, m.line + '\n'); done++;
      if (done % 25 === 0 || done === jobs.length) { const el = (Date.now() - t0) / 1000; console.error(`  ${done}/${jobs.length}  ${el.toFixed(0)} s, ETA ${(el / done * (jobs.length - done)).toFixed(0)} s`); }
    });
    wk.on('error', rej); wk.on('exit', c => { if (c) rej(new Error('worker exit ' + c)); });
  })));
  console.error(`fidelity ${pass}: done in ${((Date.now() - t0) / 1000).toFixed(0)} s`);
}

/* ---------------------------------------------------------------- attribution */
/* An original event is REMOVED by an ablation when the ablated render has no event within two
   hops of it. Its source is every ablation that removes it; an event nothing removes is
   'unattributed'. PATH ablations (patch edits) implicate a path; they are listed apart. */
export function attribution() {
  const base = new Map([...readPass('presets'), ...readPass('hotspots')]), abl = readPass('attribute'), out = [];
  for (const r of base.values()) {
    const l10 = Array.isArray(r.clicks10) ? r.clicks10 : (abl.get(`${r.id}|none`) || {}).clicks10 || [];
    const evs = r.clicks.map(e => [e, false]).concat(l10.filter(e => !r.clicks.some(x => sameEvent(x, e))).map(e => [e, true]));
    for (const [e, sens] of evs) {
      const removedBy = [];
      for (const ab of Object.keys(ABLATIONS)) {
        const a = abl.get(`${r.id}|${ab}`);
        if (a && !(sens ? a.clicks10 : a.clicks).some(x => sameEvent(x, e))) removedBy.push(ab);
      }
      out.push({ id: r.id, subj: subjName(r.subj), phrase: r.phrase, kind: r.kind, sens, t: e.t, cause: e.cause, excessDb: e.excessDb, levelDb: e.levelDb, energy: e.energy, removedBy });
    }
  }
  return out;
}

/* ---------------------------------------------------------------- WAVs */
/* The worst example of each RENDER-side source, as the audit found them (the playback WAVs, the
   captured tap and the modeled as-heard signal, come from the capture harness, not from here):
     steal     the fresh steal that cuts the most, composed vs oracle vs composed with a pool of 8;
     beating   the largest composed flatness excess (the coupling law), composed vs oracle, and
               both at K 0;
     mech:*    each oracle mechanism at its largest sample difference, with and without it;
     sens:*    the loudest sensitive (10 dB) event per attributed source, with and without it. */
function wavs() {
  const d = join(dir(), 'wav', 'render'); mkdirSync(d, { recursive: true });
  const P = readPass('presets'), written = [];
  const put = (tag, kind, ab, subj, phrase, over) => {
    const A = ab ? ABLATIONS[ab] : null, x = renderPhrase(engineClass(kind, ab && !A.patch ? ab : null), subjectParams(subj), PHRASES[phrase], { over: over || (A && A.patch) || null });
    const f = `${tag}__${subjName(subj).replace(/[^A-Za-z0-9#]+/g, '-')}_${phrase}__${kind}${ab ? '-without-' + ab : ''}${over && !ab ? '-' + Object.entries(over).map(([k, v]) => k + v).join('-') : ''}.wav`;
    writeFileSync(join(d, f), wav(x.L, x.R, SR)); written.push(f);
  };
  /* steal */
  let st = null;
  for (const r of P.values()) if (r.kind === 'composed') for (const x of r.steals) if (x.fresh && (!st || x.cut > st.x.cut)) st = { r, x };
  if (st) { put('steal', 'composed', null, st.r.subj, st.r.phrase); put('steal', 'oracle', null, st.r.subj, st.r.phrase); put('steal', 'composed', 'noSteal', st.r.subj, st.r.phrase); }
  /* beating */
  let fl = null;
  for (const r of P.values()) if (r.kind === 'composed' && r.phrase === 'chord') { const o = P.get(r.id.replace(/\|composed$/, '|oracle')); if (o && (!fl || r.flatness - o.flatness > fl.d)) fl = { r, d: r.flatness - o.flatness }; }
  if (fl) for (const k of ENGINES) { put('beating', k, null, fl.r.subj, 'chord'); put('beating', k, null, fl.r.subj, 'chord', { K: 0 }); }
  /* mechanisms */
  const M = [...readPass('mech').values()];
  for (const k of ENGINES) for (const ab of MECH[k]) {
    if (ab === 'noSteal') continue;
    const w = M.filter(r => r.kind === k && r[ab] && r[ab].max > 0).sort((a, b) => b[ab].max - a[ab].max)[0];
    if (w) { put('mech-' + ab, k, null, w.subj, w.phrase); put('mech-' + ab, k, ab, w.subj, w.phrase); }
  }
  /* sensitive events */
  const best = new Map();
  for (const e of attribution()) if (e.sens) { const src = e.removedBy[0] || 'unattributed'; if (!best.has(src) || e.energy > best.get(src).energy) best.set(src, e); }
  const all = new Map([...readPass('presets'), ...readPass('hotspots')]);
  for (const [src, e] of best) {
    const r = all.get(e.id); if (!r) continue;
    put('sens-' + src, e.kind, null, r.subj, e.phrase);
    if (src !== 'unattributed' && ablationApplies(e.kind, src)) put('sens-' + src, e.kind, src, r.subj, e.phrase);
  }
  console.log(written.join('\n'));
}

/* ---------------------------------------------------------------- summary */
function pct(a, b) { return b ? (100 * a / b).toFixed(1) + '%' : '—'; }
function summary() {
  const P = readPass('presets'), H = readPass('hotspots'), F = readPass('find'), att = attribution();
  const lines = [];
  const pair = rows => {
    const by = new Map();
    for (const r of rows.values()) { const k = subjName(r.subj) + '|' + r.phrase; if (!by.has(k)) by.set(k, {}); by.get(k)[r.kind] = r; }
    return by;
  };
  for (const [title, rows] of [['83 presets', P], ['gauntlet hotspots', H]]) {
    const by = pair(rows);
    lines.push(`### ${title}: clicks per phrase (renders with ≥ 1 event · events · Σ energy dB · sensitive 10 dB count)`, '',
      '| phrase | oracle renders | oracle events | oracle Σ dB | oracle @10 dB | composed renders | composed events | composed Σ dB | composed @10 dB | composed − oracle events |', '|---|---|---|---|---|---|---|---|---|---|');
    for (const ph of Object.keys(PHRASES)) {
      const o = [], c = [];
      for (const [k, v] of by) if (k.endsWith('|' + ph)) { if (v.oracle) o.push(v.oracle); if (v.composed) c.push(v.composed); }
      const s = xs => [xs.filter(r => r.clicks.length).length + '/' + xs.length, xs.reduce((a, r) => a + r.clicks.length, 0), clickEnergyDb(xs.flatMap(r => r.clicks)).toFixed(1), xs.reduce((a, r) => a + n10(r), 0)];
      const so = s(o), sc = s(c);
      lines.push(`| ${ph} | ${so.join(' | ')} | ${sc.join(' | ')} | ${sc[1] - so[1]} |`);
    }
    lines.push('');
    /* noise: chord phrase, composed vs oracle */
    const fl = [], al = [];
    for (const [k, v] of by) if (k.endsWith('|chord') && v.oracle && v.composed && v.oracle.flatness !== undefined) { fl.push([k.split('|')[0], v.composed.flatness - v.oracle.flatness, v.oracle.flatness, v.composed.flatness]); al.push([k.split('|')[0], v.composed.aliasDb - v.oracle.aliasDb, v.oracle.aliasDb, v.composed.aliasDb]); }
    const q = (xs, i) => { const s = xs.map(x => x[i]).sort((a, b) => a - b); return [0.05, 0.5, 0.95].map(p => s[Math.min(s.length - 1, Math.floor(p * s.length))]); };
    if (fl.length) {
      lines.push(`Noise (chord phrase; composed − oracle, p5 / p50 / p95): flatness Δ ${q(fl, 1).map(x => x.toFixed(4)).join(' / ')}; aliasDb Δ ${q(al, 1).map(x => x.toFixed(1)).join(' / ')} dB.`);
      lines.push('Largest composed excess, flatness: ' + fl.sort((a, b) => b[1] - a[1]).slice(0, 5).map(x => `${x[0]} ${x[2].toFixed(3)}→${x[3].toFixed(3)}`).join(' · '));
      lines.push('Largest composed excess, aliasing: ' + al.sort((a, b) => b[1] - a[1]).slice(0, 5).map(x => `${x[0]} ${x[2].toFixed(1)}→${x[3].toFixed(1)} dB`).join(' · '), '');
    }
    const st = { oracle: [], composed: [] };
    for (const r of rows.values()) st[r.kind].push(...r.steals);
    for (const k of ENGINES) {
      const s = st[k], fr = s.filter(x => x.fresh), loud = fr.filter(x => x.env > 0.01);
      const cuts = fr.filter(x => x.cut !== undefined).map(x => x.cut).sort((a, b) => a - b), dB = x => (x > 0 ? (20 * Math.log10(x)).toFixed(1) : '−∞');
      lines.push(`Steals (${k}): ${s.length} starts on an active slot; ${fr.length} restart the phases (fresh), ${s.length - fr.length} reuse them; fresh with env > 0.01: ${loud.length}, max env ${fr.length ? Math.max(...fr.map(x => x.env)).toFixed(3) : '—'}; of a held note: ${s.filter(x => x.gate).length}.` +
        (cuts.length ? ` Cut at a fresh steal (output units, dBFS): p50 ${dB(cuts[cuts.length >> 1])}, p95 ${dB(cuts[Math.floor(cuts.length * 0.95)])}, max ${dB(cuts[cuts.length - 1])}; > −40 dBFS: ${cuts.filter(x => x > 0.01).length}, > −20 dBFS: ${cuts.filter(x => x > 0.1).length}.` : ''));
    }
    const sub = [...rows.values()].reduce((a, r) => a + r.subnormal, 0), nf = [...rows.values()].reduce((a, r) => a + r.nonFinite, 0);
    lines.push(`Non-finite output samples: ${nf}. Float32 subnormal output samples: ${sub}.`, '');
  }
  if (F.size) {
    const f = [...F.values()];
    lines.push(`### find pass: ${f.length} gauntlet patches re-measured (composed engine, B316's scripts and seeds)`, '',
      `TONAL clicks > 0: ${f.filter(r => r.clicks > 0).length} (${pct(f.filter(r => r.clicks > 0).length, f.length)}); POLY clicks > 0: ${f.filter(r => r.clicksC > 0).length} (${pct(f.filter(r => r.clicksC > 0).length, f.length)}); noisy (flatness > ${HOT.flat}): ${f.filter(r => r.flatness > HOT.flat).length}. Hotspot subjects taken: ${H.size ? hotspotSubjects().length : '—'}.`, '');
  }
  /* the sensitive layer and the mechanisms */
  const N = [...readPass('neutral').values()];
  if (N.length) {
    const nz = N.filter(r => r.max > 0);
    lines.push(`### neutral case: composed (oracle's voice law) vs oracle (start moved by ½), K 0, aligned — ${N.length} renders`, '',
      `Exactly equal (max|Δ| = 0): ${N.length - nz.length} of ${N.length}. Non-zero: ${nz.length}` + (nz.length ? ` — worst ${nz.sort((a, b) => b.max - a.max).slice(0, 6).map(r => `${r.id} ${r.max.toExponential(1)} (step ${r.step.toExponential(1)})`).join(' · ')}` : '') + '.', '');
  }
  const M = [...readPass('mech').values()];
  if (M.length) {
    lines.push('### mechanisms, by sample difference (render − the same render with ONE mechanism removed)', '',
      '| mechanism | engine | renders where it acts | peak Δ, dBFS | largest one-sample step of Δ, dBFS | renders with a Δ step > −40 dBFS | worst |', '|---|---|---|---|---|---|---|');
    for (const k of ENGINES) for (const ab of MECH[k]) {
      const rs = M.filter(r => r.kind === k && r[ab]), act = rs.filter(r => r[ab].max > 0);
      if (!rs.length) continue;
      const w = act.slice().sort((a, b) => b[ab].step - a[ab].step)[0];
      lines.push(`| ${ab} | ${k} | ${act.length}/${rs.length} | ${act.length ? dBFS(Math.max(...act.map(r => r[ab].max))).toFixed(1) : '—'} | ${act.length ? dBFS(Math.max(...act.map(r => r[ab].step))).toFixed(1) : '—'} | ${act.filter(r => r[ab].n40 > 0).length} | ${w ? `${w.id} at ${(w[ab].at / SR).toFixed(3)} s` : '—'} |`);
    }
    lines.push('');
  }
  /* B346: the chord's aliasing, regenerated with the fixed metric and the os-convergence estimator */
  const AL = [...readPass('alias').values()];
  if (AL.length) {
    const byP = new Map(); for (const r of AL) { const k = subjName(r.subj); if (!byP.has(k)) byP.set(k, {}); byP.get(k)[r.kind] = r; }
    const q3 = xs => { const s = xs.slice().sort((a, b) => a - b); return [0.05, 0.5, 0.95].map(p => s[Math.min(s.length - 1, Math.floor(p * s.length))]); };
    lines.push(`### aliasing on the held chord (B346: ${byP.size} presets × both engines; window 0.1..0.6 s)`, '',
      '| engine | fixed aliasDb p5 / p50 / p95 | os-convergence excess p5 / p50 / p95 | total vs truth p5 / p50 / p95 | clean / folding / dynamics | decimator leak p95 | tanh fold p95 |', '|---|---|---|---|---|---|---|');
    for (const k of ENGINES) {
      const R = AL.filter(r => r.kind === k), cnt = c => R.filter(r => r.cls === c).length;
      lines.push(`| ${k} | ${q3(R.map(r => r.aliasDb)).map(x => x.toFixed(1)).join(' / ')} | ${q3(R.map(r => r.convDb)).map(x => x.toFixed(1)).join(' / ')} | ${q3(R.map(r => r.totalDb)).map(x => x.toFixed(1)).join(' / ')} | ${cnt('clean')} / ${cnt('folding')} / ${cnt('dynamics')} | ${q3(R.map(r => r.decimLeakDb))[2].toFixed(1)} | ${q3(R.map(r => r.tanhFoldDb))[2].toFixed(1)} |`);
    }
    const d = [...byP.entries()].filter(([, v]) => v.oracle && v.composed), dq = f => q3(d.map(([, v]) => f(v.composed) - f(v.oracle))).map(x => x.toFixed(1)).join(' / ');
    lines.push('', `Composed − oracle, p5 / p50 / p95: fixed aliasDb ${dq(r => r.aliasDb)} dB; os-convergence excess ${dq(r => r.convDb)} dB; total ${dq(r => r.totalDb)} dB.`);
    lines.push('Largest composed − oracle total: ' + d.map(([n, v]) => [n, v.composed.totalDb - v.oracle.totalDb, v.oracle.totalDb, v.composed.totalDb]).sort((a, b) => b[1] - a[1]).slice(0, 5).map(x => `${x[0]} ${x[2].toFixed(1)}→${x[3].toFixed(1)}`).join(' · '));
    lines.push('Worst composed total: ' + d.map(([n, v]) => [n, v.composed]).sort((a, b) => b[1].totalDb - a[1].totalDb).slice(0, 8).map(([n, r]) => `${n} ${r.totalDb.toFixed(1)} (${r.cls}, fixed ${r.aliasDb.toFixed(1)})`).join(' · '), '');
  }
  /* attribution table */
  const src = new Map();
  for (const e of att) {
    const k = (e.removedBy.length ? e.removedBy.join('+') : 'unattributed') + '|' + e.kind + (e.sens ? ' (10 dB)' : '');
    if (!src.has(k)) src.set(k, { n: 0, energy: 0, subj: new Set(), cause: new Map() });
    const s = src.get(k); s.n++; s.energy += e.energy; s.subj.add(e.subj + ' (' + e.phrase + ')'); s.cause.set(e.cause, (s.cause.get(e.cause) || 0) + 1);
  }
  lines.push('### attribution (every event; the ablations that remove it; "(10 dB)" = the sensitive level)', '', '| removed by | engine | events | Σ energy dB | at | subjects |', '|---|---|---|---|---|---|');
  for (const [k, s] of [...src.entries()].sort((a, b) => b[1].n - a[1].n)) {
    const [by, kind] = k.split('|');
    lines.push(`| ${by} | ${kind} | ${s.n} | ${(10 * Math.log10(s.energy)).toFixed(1)} | ${[...s.cause].map(([c, n]) => c + ' ' + n).join(', ')} | ${[...s.subj].slice(0, 6).join('; ')}${s.subj.size > 6 ? ` … (${s.subj.size})` : ''} |`);
  }
  console.log(lines.join('\n'));
}

if (isMainThread && process.argv[1] && process.argv[1].endsWith('fidelity_audit.mjs')) {
  const cmd = process.argv[2];
  if (['presets', 'find', 'hotspots', 'attribute', 'neutral', 'mech', 'alias'].includes(cmd)) await runPass(cmd);
  else if (cmd === 'wavs') wavs();
  else if (cmd === 'summary') summary();
  else { console.error('usage: fidelity_audit.mjs presets|neutral|mech|find|hotspots|attribute|wavs|summary [--workers W]'); process.exit(2); }
}
