/*
 * gauntlet.mjs — B316 P3: seeded random patches through the composed engine, measured.
 * HYPERSAW, 2026-09-27, ROADMAP B316 (records PR #810, branch lead-records-122).
 * The human: "You could start with a gauntlet of tests on more or less random patches
 * ... and test against coherence metrics ... and then zero in on ranges."
 *
 *   node tools/patchspace/gauntlet.mjs run --run NAME --n 3000 --edge 1000 [--seed S] [--workers W]
 *   node tools/patchspace/gauntlet_report.mjs --run NAME [--out docs/patchspace/FILE.md]
 *   node tools/patchspace/gauntlet.mjs estimate [--n 24]        (times a pilot, prints the budget)
 * Results go to local/patchspace/NAME/ (git-ignored: `local/` in .gitignore); the run is
 * RESUMABLE — each measured patch is one JSON line keyed by its index, and a re-run skips
 * indices already on disk — and prints progress with an ETA every 25 patches.
 *
 * THE SAMPLER. Patch i draws from mulberry32(hash(seed, i)), one SUBSTREAM PER PARAMETER
 * (hash(patch seed, parameter index)), so a parameter the tree leaves inactive costs no
 * draw that could shift another's. Every parameter is drawn in TAPER space (the lab's own
 * toPos/fromPos: log rows log-uniform, Width's bypass zone included, bipolar spreads
 * mirrored), then the dependency tree (dependency_tree.json) is applied as a FIXPOINT:
 * a parameter whose active_when is false is reset to its default, and the tree is re-read
 * until nothing changes (≤ 10 passes; a patch that does not settle is flagged). So a patch
 * with blade 2 off carries blade 2's defaults, not random values it would never play.
 *   broad  uniform in taper position (enums and toggles uniform).
 *   edge   the fuzzer: each continuous row lands on an END of its taper w.p. 0.7 (either end,
 *          50/50), else uniform; enums/toggles uniform. Extremes are where the engine breaks.
 * Not sampled (fixed at the engine default, named in the tree's `unsampled`): dist, seed,
 * driftMode, motionCenter, freqGlide, keepPhase, pivotMode, poly, bend. kCustom stays at its
 * default string. The tree's `excluded` rows (pitch, tab power, x rows) are not patch values.
 *
 * THE MEASUREMENT, per patch (48 kHz, Math.random seeded per render):
 *   A  TONAL   one note, A3 (220 Hz) vel 0.8, 0.5 s held; analysed over 0.1..0.5 s:
 *              every metrics.mjs analyse() field; its render is timed per 512-sample block
 *              (process.hrtime around render() ONLY) -> cpuVoice, the per-voice real-time
 *              fraction (median of blocks).
 *   B  ALIAS   one note, E5 (659 Hz), 0.3 s, at the patch's own oversampling AND at 4x it
 *              (the reference); aliasing() over 0.05..0.3 s. CAVEAT, stated because it bites:
 *              the engine's blade caps scale with the oversampled Nyquist (razor-core.js kCap
 *              = 0.45·sr·os/fi, spreadMember's 0.9·nyq and the Sine→Saw r cap), so where a
 *              cap binds the reference is a slightly different (brighter) patch, and FM
 *              partials that move with the cap can read as aliasing. Best available, not pure.
 *              B345 (2026-09-28): aliasing() compares 2-frame units and floors the reference's
 *              local level by its noise floor (metrics.mjs header); this 0.25 s window is exactly
 *              one unit, so only the floor moved its numbers (noise-like patches read lower). The
 *              A leg also records noiseDb (analyse()). Runs and reports made before B345 (the P3
 *              report, docs/patchspace/2026-09-27-gauntlet-p3.md) carry the earlier aliasing.
 *              B346 (2026-09-29) adds, BESIDE aliasDb: aliasConvDb (THRESH's aliasing gate since B351) / aliasConvClass
 *              (clean | folding | dynamics) / aliasFoldDb / aliasDynDb / aliasConvergeDb, the os-convergence
 *              estimator on the same note and window (metrics.mjs aliasConvergence, rendered by
 *              alias_sources.mjs estimate at os x1..x16), decimLeakDb / tanhFoldDb, the aliasing of the
 *              output stage every os shares (the engine's decimator and its output-rate tanh), which neither
 *              aliasing metric can see, and aliasTotalDb, all of it against an oversampled truth. Only when
 *              measure() is asked (opt.conv; the run and its report ask): the leg then costs about 31
 *              renders' worth instead of 5.
 *   C  POLY    four overlapping notes (A2, A3, E4, A5 entering every 0.15 s), all off at 0.9 s,
 *              rendered to 1.4 s: nonFinite, peak, clicks and silence over the whole render,
 *              and its wall-clock load (cpuPoly, all voices together).
 * CPU IS NOISY and is reported apart from the deterministic metrics: workers share the
 * machine (and macOS may place a worker on an efficiency core), so each worker re-times a
 * REFERENCE patch (the engine defaults) every 8 patches and every cpu number is also given
 * normalised to it (cpuVoiceNorm = cpuVoice / ref × the reference's cost timed once before
 * the workers start). Rankings are meaningful; absolute numbers carry the machine.
 * DETERMINISM: same seed -> same patches -> same non-CPU metrics, bit for bit (checked by
 * the report: it re-measures a sample of patches and compares).
 *
 * PROVISIONAL THRESHOLDS (the yield), MEASURED NOT GATED — to be calibrated by the human's
 * blind listening pass (B316): see THRESH below; the report prints them beside every rate.
 */
import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync, appendFileSync } from 'node:fs';
import { join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { cpus } from 'node:os';
import { ROOT, SR, mulberry32, loadSpace, valueAt, render, evalCond, mtof } from './space.mjs';
import { analyse, spectrum, aliasing, mono, cpuFraction, nonFinite, clicks, silence, peak } from './metrics.mjs';
import { asEvalTree, hash32 } from './gen_dependency_tree.mjs';
import { estimate as aliasEstimate, baseEngine, renderWith } from './alias_sources.mjs';

/* THRESH v2 (B350, 2026-09-29, ROADMAP-ruled): the human's blind listening pass (B344/B345,
   n=34 ratings from one listener) fitted these four cuts against the worst-heard segment; the
   human: "I think I'm comfortable with it". Still PROVISIONAL — one listener, one sample.
   flatness is kept as a MEASURED COLUMN, not a gate: B345 found it does not track what the
   human calls noisy (agreement 0.50 at 0.3, no logistic trend), so the noise gate below is
   noiseDb (metrics.mjs, added by B345) instead.
   THE ALIASING GATE (B351, 2026-09-29, the lead's decision on the human's delegation, ROADMAP B351):
   B350's aliasDb -26.8 failed a must-read-high control (a naive saw at E5 with vibrato reads -27.2 on
   B345's metric, which under-reads moving partials), so the gate moved to B346's os-convergence
   estimator, aliasConvDb (measure()'s conv leg), re-fitted on the same v2 ratings re-measured in Node
   (calibrate.mjs --remeasure --conv, broad#383 left out as chaotic, n=33): AUC 0.88, cut -33.4 dB,
   agreement 0.79, leave-one-out 0.70, logistic 50% at -17.8. aliasDb fitted 0.92 / -26.8 / 0.88 /
   0.79 on the same answers, but its cut moves 13 dB with the basis (-39.8 on the gauntlet's own E5
   window) where aliasConvDb's moves 1.5 (-34.9), and it fails the vibrato control; aliasConvDb at
   -33.4 passes every detector control (listening_pass_check T10: the naive saw with vibrato and glide
   at E5 and A3 reads -19.1 to -26.1, the band-limited ones -120). aliasDb stays a MEASURED COLUMN.
   A run measured without opt.conv carries no aliasConvDb: failures() refuses it rather than pass it.
   LABELS, NOT GATES (B360, 2026-09-29, human: "I think noisy and rough should just label; some
   patches want noisy or rough"): noiseDb and roughness below tag a patch (labels(), on every
   record) that P4 can target or avoid; they no longer push into failures() or incoherence() and no
   longer reduce healthy or coherent yield. aliasConvDb and rootPresence are unmoved — aliasing
   (failures()) and an absent root (incoherence()) still gate. */
export const THRESH = {
  overloadPoly6: 1.0,   // projected load of the lab's 6-voice pool: 6 × cpuVoice > 100% of real time
  overloadVoice: 1.0,   // one voice over real time (the brief's literal "> 100% RT per voice")
  aliasConvDb: -33.4,   // B351: the os-convergence estimator, v2 fit, worst-heard segment, n=33 (was aliasDb -26.8, B350; -30, B316) — a FAILURE (failures())
  clicks: 0,            // any click frame in the TONAL window
  dcRatio: 0.1,         // |mean| above 10% of RMS
  rootPresence: 0.616,  // B350: v2 fit, worst-heard segment, n=34 (was 0.5) — a FAILURE (incoherence())
  noiseDb: -21.5,       // B350: the noise gate, moved from flatness 0.3 (v2 fit, n=34) — B360: a LABEL (labels()), not a gate
  roughness: 0.12,      // B350: v2 fit, worst-heard segment, n=34 (was 0.1; two pure tones a minor second apart read 0.090 in metrics_check) — B360: a LABEL (labels()), not a gate
};
const TREE_FILE = 'tools/patchspace/dependency_tree.json';
let TREE = null;
const tree = () => TREE || (TREE = asEvalTree(JSON.parse(readFileSync(join(ROOT, TREE_FILE), 'utf8'))));

/* ---------------------------------------------------------------- the sampler */
export function samplePatch(seed, i, mode) {
  const { params } = loadSpace(), T = tree(), ps = hash32(seed >>> 0, i, mode === 'edge' ? 2 : 1);
  const P = {};
  params.forEach((p, pi) => {
    const r = mulberry32(hash32(ps, pi)), u = r(), v = r();
    if (p.kind === 's') P[p.key] = p.default;
    else if (p.kind === 'c') P[p.key] = valueAt(p, mode === 'edge' && u < 0.7 ? (v < 0.5 ? 0 : 1) : v);
    else P[p.key] = valueAt(p, v);
  });
  /* the fixpoint: inactive rows go to their defaults until the tree reads the same twice */
  let pass = 0, changed = true, inactive = [];
  for (; changed && pass < 10; pass++) {
    changed = false; inactive = [];
    for (const p of params) {
      if (evalCond(T.params[p.key].active_when, P, T.predicates)) continue;
      inactive.push(p.key);
      if (P[p.key] !== p.default) { P[p.key] = p.default; changed = true; }
    }
  }
  return { patch: P, live: params.length - inactive.length, settled: !changed };
}

/* ---------------------------------------------------------------- the measurement */
/* exported for B324's listening pass: its phrase is these two renders plus their release tails */
export const A_SCRIPT = { n: 24000, ev: [[0, 'on', 57, 0.8]] };
export const B_SCRIPT = { n: 14400, ev: [[0, 'on', 76, 0.8]] };
export const C_SCRIPT = { n: 67200, ev: [[0, 'on', 45, 0.8], [7200, 'on', 57, 0.8], [14400, 'on', 64, 0.8], [21600, 'on', 81, 0.8],
  [43200, 'off', 45], [43200, 'off', 57], [43200, 'off', 64], [43200, 'off', 81]] };
const sl = (x, a, b) => x.subarray(a, b);
const r6 = x => (typeof x === 'number' && Number.isFinite(x) ? +x.toPrecision(6) : x);
export function measure(patch, seed, opt) {
  const out = {};
  const a = render(patch, A_SCRIPT, { seed, timing: true, block: 512 });
  const m = analyse(sl(a.L, 4800, 24000), sl(a.R, 4800, 24000), SR, mtof(57));
  delete m._S;
  for (const k in m) out[k] = r6(m[k]);
  out.nonFiniteA = nonFinite(a.L, a.R);
  out.cpuVoice = cpuFraction(a.timing, SR);
  const os = patch.os || 2;
  const b = render(patch, B_SCRIPT, { seed });
  const bRef = render(Object.assign({}, patch, { os: os * 4 }), B_SCRIPT, { seed });
  const al = aliasing(spectrum(mono(sl(b.L, 2400, 14400), sl(b.R, 2400, 14400)), SR), spectrum(mono(sl(bRef.L, 2400, 14400), sl(bRef.R, 2400, 14400)), SR));
  out.aliasDb = r6(al.aliasDb);
  out.nonFiniteB = nonFinite(b.L, b.R);
  /* B346: the os-convergence estimator on the same note and window (renders at os x1..x16, the x8 one
     captured; alias_sources.mjs estimate), and the output stage every os shares. BESIDE aliasDb, which
     is unchanged (its two renders above are untouched); THRESH's alias gate reads aliasConvDb (B351). Only when asked
     (opt.conv: the run and its report ask): it costs about 26 renders' worth, and the checks that
     re-measure through measure() (metrics_check E1/E2, listening_pass_check T4) read none of it. */
  if (opt && opt.conv) Object.assign(out, convLeg(patch, seed));
  const t0 = process.hrtime.bigint();
  const c = render(patch, C_SCRIPT, { seed });
  out.cpuPoly = Number(process.hrtime.bigint() - t0) / 1e9 / (C_SCRIPT.n / SR);
  const cm = mono(c.L, c.R);
  out.nonFiniteC = nonFinite(c.L, c.R);
  out.peakC = r6(Math.max(peak(c.L), peak(c.R)));
  const ck = clicks(cm); out.clicksC = ck.clicks; out.clicksCWorstDb = r6(ck.worstDb);
  out.silentC = silence(c.L, c.R).silent;
  return out;
}
/* measure()'s B346 leg on its own (B351: calibrate.mjs --remeasure reads the committed-basis estimator
   for the v1 ratings through it, so the two are one measurement, not two) */
export function convLeg(patch, seed) {
  const os = patch.os || 2, out = {};
  const E = aliasEstimate((m, cap) => renderWith(baseEngine(), Object.assign({}, patch, { os: os * m }), B_SCRIPT, { seed, cap }), os, [2400, 14400], { truth: true });
  out.aliasConvDb = r6(E.conv.excessDb); out.aliasConvClass = E.conv.cls; out.aliasFoldDb = r6(E.conv.foldDb); out.aliasDynDb = r6(E.conv.dynDb);
  out.aliasConvergeDb = r6(E.conv.convDb); out.decimLeakDb = r6(E.out.decimLeakDb); out.tanhFoldDb = r6(E.out.tanhFoldDb);
  out.aliasTotalDb = r6(E.totalDb);
  return out;
}
export function failures(r) {
  const f = [];
  if (r.nonFiniteA || r.nonFiniteB || r.nonFiniteC) f.push('nonfinite');
  if (r.silent) f.push('silent');
  if (r.cpuVoiceNorm * 6 > THRESH.overloadPoly6) f.push('overload6');
  if (r.cpuVoiceNorm > THRESH.overloadVoice) f.push('overloadVoice');
  if (r.aliasConvDb === undefined) throw new Error(`${r.mode}#${r.i}: no aliasConvDb (measured without opt.conv, or a run before B346): the aliasing gate reads the estimator (B351)`);
  if (r.aliasConvDb > THRESH.aliasConvDb) f.push('alias');
  if (r.clicks > THRESH.clicks) f.push('clicks');
  if (r.dcRatio > THRESH.dcRatio) f.push('dc');
  return f;
}
export function incoherence(r) {
  const f = [];
  if (r.rootPresence !== null && r.rootPresence < THRESH.rootPresence) f.push('rootAbsent');
  return f;
}
/* B360 (2026-09-29, human: "I think noisy and rough should just label; some patches want noisy or
   rough"): noiseDb and roughness no longer reduce healthy or coherent yield — they LABEL a patch
   that P4 can target or avoid. aliasConvDb (failures()) and rootPresence (incoherence()) are
   unmoved: aliasing and an absent root are still gates. */
export function labels(r) {
  const L = [];
  if (r.noiseDb !== null && r.noiseDb !== undefined && r.noiseDb > THRESH.noiseDb) L.push('noisy');
  if (r.roughness !== null && r.roughness > THRESH.roughness) L.push('rough');
  return L;
}

/* ---------------------------------------------------------------- run */
const REF_EVERY = 8;
function refTiming() { const { defaults } = loadSpace(); return cpuFraction(render(defaults, A_SCRIPT, { seed: 1, timing: true, block: 512 }).timing, SR); }
if (!isMainThread && workerData && workerData.kind === 'gauntlet') {
  const { jobs, seed, quietRef } = workerData;
  let ref = refTiming();
  jobs.forEach((j, n) => {
    if (n && n % REF_EVERY === 0) ref = refTiming();
    const s = samplePatch(seed, j.i, j.mode);
    const pseed = hash32(seed, j.i, 77);
    const r = Object.assign({ i: j.i, mode: j.mode, live: s.live, settled: s.settled }, measure(s.patch, pseed, { conv: true }));
    r.cpuRef = ref; r.cpuVoiceNorm = r.cpuVoice / ref * quietRef;
    r.labels = labels(r);   // B360: noisy/rough are labels on the record, not gates
    parentPort.postMessage({ line: JSON.stringify(r) });
  });
  parentPort.postMessage({ done: true });
}

function arg(name, def) { const i = process.argv.indexOf('--' + name); return i > 0 ? process.argv[i + 1] : def; }
function runDir(run) { const d = join(ROOT, 'local', 'patchspace', run); mkdirSync(d, { recursive: true }); return d; }
function readRun(run) {
  const d = join(ROOT, 'local', 'patchspace', run), rows = new Map();
  if (!existsSync(d)) return { rows, meta: null };
  /* a run killed mid-write can leave a torn last line: skip it (the patch is re-measured on resume) */
  for (const f of readdirSync(d).filter(f => f.endsWith('.jsonl'))) for (const l of readFileSync(join(d, f), 'utf8').split('\n')) {
    if (!l) continue; let r; try { r = JSON.parse(l); } catch { continue; } rows.set(r.mode + ':' + r.i, r); }
  const mf = join(d, 'meta.json');
  return { rows, meta: existsSync(mf) ? JSON.parse(readFileSync(mf, 'utf8')) : null };
}

async function run() {
  const name = arg('run', 'default'), seed = +arg('seed', 0xB316), nb = +arg('n', 2000), ne = +arg('edge', 1000);
  const workers = +arg('workers', Math.max(1, Math.min(4, cpus().length - 2)));
  const d = runDir(name), { rows } = readRun(name);
  const quietRef = [0, 1, 2, 3, 4].map(refTiming).sort((a, b) => a - b)[2];
  const metaFile = join(d, 'meta.json');
  const meta = existsSync(metaFile) ? JSON.parse(readFileSync(metaFile, 'utf8')) : { seed, broad: nb, edge: ne, workers, quietRef, started: null, wall: 0 };
  if (meta.seed !== seed) throw new Error(`run ${name} was started with seed ${meta.seed}; resume with the same seed`);
  const jobs = [];
  for (let i = 0; i < nb; i++) if (!rows.has('broad:' + i)) jobs.push({ i, mode: 'broad' });
  for (let i = 0; i < ne; i++) if (!rows.has('edge:' + i)) jobs.push({ i, mode: 'edge' });
  console.error(`gauntlet ${name}: ${rows.size} on disk, ${jobs.length} to measure, ${workers} workers, quiet reference ${(quietRef * 100).toFixed(1)}% RT/voice`);
  meta.broad = nb; meta.edge = ne; meta.workers = workers;   // the CURRENT targets: the report reads only indices below them
  writeFileSync(metaFile, JSON.stringify(meta, null, 1));
  if (!jobs.length) return;
  const t0 = Date.now(), wall0 = meta.wall; let done = 0;
  const shares = Array.from({ length: workers }, (_, w) => jobs.filter((_, k) => k % workers === w));
  await Promise.all(shares.map((share, w) => new Promise((res, rej) => {
    const file = join(d, `results-w${w}-${Date.now().toString(36)}.jsonl`);
    const wk = new Worker(new URL(import.meta.url), { workerData: { kind: 'gauntlet', jobs: share, seed, quietRef } });
    wk.on('message', m => {
      if (m.done) return res();
      appendFileSync(file, m.line + '\n'); done++;
      if (done % 25 === 0 || done === jobs.length) {
        const el = (Date.now() - t0) / 1000, eta = el / done * (jobs.length - done);
        console.error(`  ${done}/${jobs.length}  ${el.toFixed(0)} s elapsed, ETA ${eta.toFixed(0)} s`);
        meta.wall = wall0 + el; writeFileSync(metaFile, JSON.stringify(meta, null, 1));   // survives a kill: resumable wall time
      }
    });
    wk.on('error', rej); wk.on('exit', c => { if (c) rej(new Error('worker exit ' + c)); });
  })));
  meta.wall = wall0 + (Date.now() - t0) / 1000;
  writeFileSync(metaFile, JSON.stringify(meta, null, 1));
  console.error(`gauntlet ${name}: done, ${done} measured this pass, wall ${meta.wall.toFixed(0)} s total`);
}

async function estimate() {
  const n = +arg('n', 16), seed = 0xE57;
  const t0 = process.hrtime.bigint();
  for (let i = 0; i < n; i++) { const s = samplePatch(seed, i, i % 3 ? 'broad' : 'edge'); measure(s.patch, i, { conv: true }); }
  const per = Number(process.hrtime.bigint() - t0) / 1e9 / n;
  console.log(`estimate: ${per.toFixed(2)} s per patch single-threaded (${n} pilot patches, 1/3 edge)`);
  for (const w of [3, 4, 6]) for (const N of [1000, 2000, 3000, 4000]) console.log(`  N ${N} on ${w} workers ≈ ${(N * per / w / 60).toFixed(1)} min (if workers scale linearly)`);
}

if (isMainThread && process.argv[1] && process.argv[1].endsWith('gauntlet.mjs')) {
  const cmd = process.argv[2];
  if (cmd === 'run') await run();
  else if (cmd === 'estimate') await estimate();
  else { console.error('usage: gauntlet.mjs run|estimate [--run NAME] [--n N] [--edge N] [--seed S] [--workers W]; the report is gauntlet_report.mjs --run NAME'); process.exit(2); }
}
export { readRun };
