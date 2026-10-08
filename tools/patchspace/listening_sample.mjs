/*
 * listening_sample.mjs — B324: choose the blind listening pass's patches, offline, from a
 * gauntlet run, and write docs/design/listening-pass.json (seeds + measured metrics only).
 * HYPERSAW, 2026-09-28, ROADMAP B324 (records PR #819, branch lead-records-126). The human:
 * "Help me organize the listening pass, please".
 *
 *   node tools/patchspace/gauntlet.mjs run --run lp324 --n 900 --edge 300     (the pool, ~6 min)
 *   node tools/patchspace/listening_sample.mjs --run lp324 [--exclude KEY,KEY]   (this, ~30 s)
 *   then open docs/design/listening-pass.html?xverify=1 (served) and read its cross-runtime table
 *   node tools/patchspace/listening_sample.mjs --remeasure --note "why"   (B345: after a metrics.mjs
 *     change, re-measure the SAME items in place; no gauntlet run needed — see remeasure() below)
 *
 * WHAT IS COMMITTED, AND WHY NOTHING ELSE IS. A patch is its gauntlet SEED: (run seed, index,
 * mode) through gauntlet.mjs samplePatch, rendered with the gauntlet's own per-patch render
 * seed hash32(run seed, i, 77). So the JSON carries the seed triple, the metrics gauntlet.mjs
 * measure() wrote for it, a PATCH HASH (`ph`, exact in any runtime) and a FINGERPRINT (`fp`,
 * FNV-1a over the float32 bits of the two renders the metrics were taken from — exact in
 * Node only: Chrome's sin/exp/tanh differ from Node's in the last bits, so the page checks
 * that its sound MEASURES like these numbers instead). The page regenerates every sound from the seed; no audio and
 * nothing under local/ is committed. listening_pass_check.mjs re-measures every seed against
 * the committed numbers, so a sampler, table or engine change that moves a patch turns red
 * instead of silently asking the human to rate a different sound than the one measured.
 *
 * THE STRATA (bands are placed around gauntlet.mjs THRESH, the PROVISIONAL thresholds, so a
 * few ratings bracket each one; every rated patch still answers all five questions, so every
 * patch informs every metric):
 *   per metric: 2 FINE (clearly on the good side), 3 BORDER (one just below, one at, one just above the
 *   threshold), 2 FLAG (clearly on the flagged side) — for aliasDb, rootPresence and flatness;
 *   roughness: 2 fine, 3 border, and 4 flag split by ORIGIN — 2 whose roughness is mostly a
 *   swarm's own beating (the SAME patch rendered with one member, N 1, reads smooth:
 *   roughnessSolo ≤ 0.04) and 2 whose partials are discordant on their own (roughnessSolo ≥
 *   the threshold). B316's open question, asked of the ears.
 *   CONTROLS: 2 clearly clean (no metric near any threshold), 2 clearly broken (aliased AND
 *   rough AND noisy-or-rootless), and 2 REPEATS of borderline patches (test-retest). An
 *   inattentive pass shows up on the controls before it shows up in the fit.
 *   Pool filter: audible (TONAL rmsDb > -60 dBFS, not silent), finite everywhere, settled.
 * Selection within a stratum is a seeded draw (SELECT_SEED) over the candidates in key order,
 * never reusing a patch.
 * CROSS-RUNTIME EXCLUSIONS (--exclude). The metrics are Node's; the ear hears the browser's render,
 * and the two runtimes' sin/exp/tanh differ in their last bits. For most patches that changes
 * nothing measurable (identical to 6 digits); a chaotic patch amplifies it into a different
 * realisation that measures differently, so its rating would pair with numbers that are not its
 * sound's. The page's ?xverify=1 pass names those patches; they are excluded here BY KEY, with
 * the reason recorded in the JSON (`excluded`), and the seeded draw takes the next candidate (the
 * shuffles consume the same stream, so no other stratum's pick moves unless it shared the patch).
 * The bias is stated, not hidden: patches whose chaos amplifies last-bit differences are
 * under-represented. In any OTHER browser the page still marks such a patch at play time
 * (`heard: drift`) and calibrate.mjs leaves it out of the fit. The roughness-origin render (N 1) is only computed for the draws it
 * needs, in the same seeded order.
 */
import '../labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { writeFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { readRun, samplePatch, measure, THRESH, A_SCRIPT, B_SCRIPT } from './gauntlet.mjs';
import { ROOT, SR, mulberry32, render, mtof, loadEngine } from './space.mjs';
import { analyse } from './metrics.mjs';
import { hash32 } from './gen_dependency_tree.mjs';

export const SAMPLE_FILE = 'docs/design/listening-pass.json';
export const PAGE_FILE = 'docs/design/listening-pass.html';
/* THE LISTENING PAGE'S PURE BLOCK (between its PURE-BEGIN / PURE-END markers), sliced and run in
   Node: the page's sampler, render route, program and measurement, never re-implemented. One
   loader: listening_pass_check.mjs proves the block, calibrate.mjs --remeasure (B345) uses it to
   re-render and re-measure what the human heard. */
export function loadPage(html) {
  html = html || readFileSync(join(ROOT, PAGE_FILE), 'utf8');
  const a = html.indexOf('// PURE-BEGIN'), b = html.indexOf('// PURE-END');
  if (a < 0 || b < a) throw new Error(`${PAGE_FILE}: PURE-BEGIN / PURE-END markers not found`);
  return new Function('"use strict";\n' + html.slice(a, b) + '\nreturn { labFrom, spaceFrom, asEvalTree, samplePatch, renderScript, renderSteps, runSync, pseedOf, ' +
    'presentationOrder, QUI, viewIntro, viewCalib, viewRate, viewDone, viewReveal, fmtM, blindTokens, blindScan, buildExport, buildExportV2, exportV1FromStore, ' +
    'patchHash, measureHeard, heardDiff, TOL, SEGS, SEG_A3, SEG_E5, SEG_SWEEP, MEAS, PHRASE_ID, renderSegmentSteps, programFp, measureWindows, measureSegmentSteps, winStarts, ' +
    'detectorControls, detectorLine, refTone, programGain, assembleProgram, segScript, PROGRAM_SECONDS, v1SampleOf, sameSoundsAs, heardEngine };')();
}
/* B382, THE HEARD ENGINE (the page's heardEngine, on the gauntlet's route). The committed sample was
   measured, and rated, on the composed engine before B382, with none of SwarmSynth's divergences (the
   engine's SWARM_PATCHES, default ON since B382). heard(patch) writes each of them 0, and every render of
   the COMMITTED sample (remeasure below, listening_pass_check.mjs) goes through it, so the sample still
   reproduces bit for bit and the ratings stay paired with their sounds. A NEW selection (main, below)
   reads a gauntlet run of the day and is rendered on the engine of the day: it does not use this. */
export const HEARD = Object.fromEntries(loadEngine().Composed.swarmPatches.map(x => [x.flag, 0]));
export const heard = patch => Object.assign({}, patch, HEARD);
export const SELECT_SEED = 0xB324;
export const ORDER_SEED = 0x324B;
export const RENDER_SALT = 77;                                   // gauntlet.mjs: pseed = hash32(seed, i, 77)
/* the deterministic fields gauntlet.mjs measure() writes that the pass keeps (no CPU: it is noisy) */
export const KEEP = ['aliasDb', 'roughness', 'rootPresence', 'rootInterval', 'flatness', 'noiseDb', 'rmsDb', 'lufs', 'peakDb', 'crestDb',
  'dcRatio', 'clicks', 'clicksC', 'peakC'];                     // noiseDb: B345, flatness's proposed replacement
const r6 = x => (typeof x === 'number' && Number.isFinite(x) ? +x.toPrecision(6) : x);

/* FNV-1a over the float32 BITS: two renders agree here only if every sample agrees exactly */
export function fingerprint(...chs) {
  let h = 0x811C9DC5;
  for (const x of chs) {
    const u = new Uint32Array(x.buffer, x.byteOffset, x.length);
    for (let k = 0; k < u.length; k++) { h ^= u[k]; h = Math.imul(h, 0x01000193) >>> 0; }
  }
  return h.toString(16).padStart(8, '0');
}
/* the two renders the metrics came from, exactly as gauntlet.mjs measure() makes them */
export function measuredRenders(patch, pseed) {
  return { a: render(patch, A_SCRIPT, { seed: pseed, block: 512 }), b: render(patch, B_SCRIPT, { seed: pseed }) };
}
export function fingerprintOf(patch, pseed) { const { a, b } = measuredRenders(patch, pseed); return fingerprint(a.L, a.R, b.L, b.R); }
/* roughness of the SAME patch with one member: what is left is the partials' own discordance */
export function roughnessSolo(patch, pseed) {
  const a = render(Object.assign({}, patch, { N: 1 }), A_SCRIPT, { seed: pseed, block: 512 });
  const m = analyse(a.L.subarray(4800, 24000), a.R.subarray(4800, 24000), SR, mtof(57));
  return m.roughness === null ? null : r6(m.roughness);
}
export const ORIGIN = { beatingMax: 0.04 };                      // solo at or under: the swarm's beating
export function roughOrigin(roughness, solo, thr) {
  if (!(roughness > thr) || solo === null) return null;
  return solo <= ORIGIN.beatingMax ? 'beating' : solo >= thr ? 'partials' : 'mixed';
}
/* a patch's identity: FNV-1a over its JSON's UTF-16 code units. The sampler reads Math.pow alone
   (the lab's fromPos), which Chrome and Node compute identically, so this matches in the page
   too — unlike `fp`: the ENGINE reads sin/exp/tanh, whose last bits differ between the two
   runtimes (listening-pass.html measureHeard says what the page checks instead) */
export function patchHash(patch) {
  const s = JSON.stringify(patch); let h = 0x811C9DC5;
  for (let k = 0; k < s.length; k++) { h ^= s.charCodeAt(k); h = Math.imul(h, 0x01000193) >>> 0; }
  return h.toString(16).padStart(8, '0');
}
/* the sample's identity: FNV-1a over the items' JSON (an export names the sample it was rated on) */
export function sampleId(items) {
  let h = 0x811C9DC5;
  for (const x of new TextEncoder().encode(JSON.stringify(items))) { h ^= x; h = Math.imul(h, 0x01000193) >>> 0; }
  return h.toString(16).padStart(8, '0');
}

/* bands: [lo, hi) in metric units; `when` says which side of the threshold is the flag.
   B351 (2026-09-29): the bands are a function of the thresholds `T` they are drawn around, so a
   future draw follows the RULED gates (B350/B351: aliasing on aliasConvDb, noise on noiseDb)
   instead of reading gates that no longer exist (THRESH.flatness was undefined under B350, and so
   is THRESH.aliasDb under B351). A threshold object without the new keys (the committed sample's
   own `thresholds`, the B316 values) gives exactly the B316 bands: listening_pass_check T1 proves
   every committed stratum label is one of strataFor(its thresholds)'s, so the committed sample is
   reproducible from this code and is not redrawn. The rule that places the B316 bands, kept:
   a dB gate's BORDER is thr-6..-2, thr-2..+2, thr+2..+6, FINE under thr-15 and FLAG from thr+15
   (the B316 aliasing row at -30; the noiseDb row at its gate uses the same dB rule); roughness and
   root keep their B316 bands shifted with their thresholds (border offsets -0.03/-0.01/+0.01/+0.04
   and -0.12/-0.04/+0.04/+0.12), their fine and flag ends absolute (a clearly smooth or clearly
   rootless sound does not move with a cut). */
const dbBands = thr => ({ fine: [[-Infinity, thr - 15, 2]], border: [[thr - 6, thr - 2, 1], [thr - 2, thr + 2, 1], [thr + 2, thr + 6, 1]], flag: [[thr + 15, Infinity, 2]] });
const shifted = (thr, o) => [[thr + o[0], thr + o[1], 1], [thr + o[1], thr + o[2], 1], [thr + o[2], thr + o[3], 1]].map(b => [+b[0].toFixed(6), +b[1].toFixed(6), 1]);
export const aliasGateOf = T => (T.aliasConvDb !== undefined ? 'aliasConvDb' : 'aliasDb');      // the field the aliasing gate reads
export function strataFor(T) {
  const al = aliasGateOf(T);
  return [
    Object.assign({ metric: al, when: 'above', thr: T[al] }, dbBands(T[al])),
    { metric: 'roughness', when: 'above', thr: T.roughness, fine: [[-Infinity, 0.04, 2]], border: shifted(T.roughness, [-0.03, -0.01, 0.01, 0.04]),
      flag: [[0.2, Infinity, 2, 'beating'], [0.2, Infinity, 2, 'partials']] },
    { metric: 'rootPresence', when: 'below', thr: T.rootPresence, fine: [[0.98, Infinity, 2]], border: shifted(T.rootPresence, [-0.12, -0.04, 0.04, 0.12]), flag: [[-Infinity, 0.25, 2]] },
    T.noiseDb !== undefined ? Object.assign({ metric: 'noiseDb', when: 'above', thr: T.noiseDb }, dbBands(T.noiseDb))
      : { metric: 'flatness', when: 'above', thr: T.flatness, fine: [[-Infinity, 0.02, 2]], border: [[0.2, 0.27, 1], [0.27, 0.33, 1], [0.33, 0.42, 1]], flag: [[0.5, Infinity, 2]] },
  ];
}
/* the gates a draw records in the sample's `thresholds` (the ones its bands were placed around) */
export const gatesOf = T => { const al = aliasGateOf(T); return Object.assign({ [al]: T[al], roughness: T.roughness, rootPresence: T.rootPresence }, T.noiseDb !== undefined ? { noiseDb: T.noiseDb } : { flatness: T.flatness }); };
/* CONTROLS: clean reads clean on every measured column (flatness is still measured); broken is
   aliased AND rough AND noisy-or-rootless on the gates in `T` (at the B316 thresholds: exactly the
   B316 rule, aliasDb >= -15 and flatness >= 0.3) */
export function controlsFor(T) {
  const al = aliasGateOf(T), noisy = T.noiseDb !== undefined ? r => r.noiseDb > T.noiseDb : r => r.flatness >= T.flatness;
  return {
    clean: r => r[al] <= -90 && r.roughness <= 0.03 && r.rootPresence >= 0.99 && r.rootInterval === 0 && r.flatness <= 0.01 &&
      r.clicks === 0 && r.clicksC === 0 && r.dcRatio < 0.05 && r.rmsDb > -30,
    broken: r => r[al] >= -15 && r.roughness >= 0.15 && (noisy(r) || r.rootPresence <= 0.3),
  };
}
/* a stratum's label in the sample (`stratum`): the record of how an item was drawn */
export function stratumLabel(metric, band, lo, hi, want) {
  const range = lo === -Infinity ? `< ${hi}` : hi === Infinity ? `≥ ${lo}` : `[${lo}, ${hi})`;
  return `${metric} ${band} ${range}` + (want ? ' ' + want : '');
}
export const strataLabels = strata => strata.flatMap(s => ['fine', 'border', 'flag'].flatMap(band => s[band].map(([lo, hi, , want]) => stratumLabel(s.metric, band, lo, hi, want))));
const T = THRESH;
export const STRATA = strataFor(T);
export const CONTROLS = controlsFor(T);

/* RE-MEASURE IN PLACE (B345, 2026-09-28; the human: "Let's fix the metrics first, then re-fit").
   When metrics.mjs changes, the committed numbers must be the current metrics' numbers of the
   SAME sounds, or listening_pass_check T4 reddens. This rewrites each item's `metrics` with
   gauntlet.mjs measure() (and roughnessSolo) for the same seeds, and changes nothing else: the
   same items in the same order, the same strata labels (a record of how each patch was DRAWN,
   on the numbers of its day), the same exclusions, ph and fp (asserted unchanged: the sounds
   are the sounds). A fresh selection would draw different patches from the new numbers and
   orphan the human's ratings, so this never re-selects. The id changes with the numbers; the
   file records every id it replaced in `remeasured`, so the page and calibrate.mjs know an
   export on an earlier id rated the same sounds. */
export function remeasure(json, note) {
  const run = json.run.seed, items = json.items.map(it => {
    const patch = samplePatch(run, it.i, it.mode).patch, pseed = hash32(run, it.i, RENDER_SALT), hp = heard(patch);   // B382: the heard engine
    if (patchHash(patch) !== it.ph || fingerprintOf(hp, pseed) !== it.fp) throw new Error(`${it.key}: the sound changed (ph/fp) — a re-measure keeps the sounds; re-select instead`);
    const r = measure(hp, pseed), metrics = Object.fromEntries(KEEP.map(k => [k, r[k]]));
    metrics.roughnessSolo = roughnessSolo(hp, pseed);
    /* the sample's OWN roughness threshold (the one its labels were made with), not the live THRESH:
       B350 moved THRESH.roughness 0.10 -> 0.12, and a re-measure keeps the labels of the draw (T4) */
    return Object.assign({}, it, { roughOrigin: roughOrigin(metrics.roughness, metrics.roughnessSolo, json.thresholds.roughness), metrics });
  });
  const id = sampleId(items);
  const remeasured = (json.remeasured || []).concat(id === json.id ? [] : [{ from: json.id, to: id, note }]);
  const { id: _, items: __, remeasured: ___, ...rest } = json;
  return Object.assign(rest, { remeasured, id, items });
}

function main() {
  const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
  if (process.argv.includes('--remeasure')) {
    const note = arg('note', '');
    if (!note) throw new Error('--remeasure needs --note "why the metrics changed" (recorded in the file)');
    const file = join(ROOT, SAMPLE_FILE), old = JSON.parse(readFileSync(file, 'utf8')), json = remeasure(old, note);
    writeFileSync(file, JSON.stringify(json, null, 1) + '\n');
    console.error(`listening_sample: re-measured ${json.items.length} items in place, id ${old.id} -> ${json.id}`);
    return;
  }
  const run = arg('run', 'lp324'), exclude = new Set(String(arg('exclude', '')).split(',').filter(Boolean));
  const { rows, meta } = readRun(run);
  if (!meta) throw new Error(`no gauntlet run "${run}" under local/patchspace/ — run gauntlet.mjs first`);
  const pool = [...rows.values()].filter(r => r.settled && !r.silent && r.rmsDb > -60 && !r.nonFiniteA && !r.nonFiniteB && !r.nonFiniteC &&
    r.roughness !== null && r.flatness !== null && r.rootPresence !== null)
    .sort((a, b) => (a.mode === b.mode ? a.i - b.i : a.mode < b.mode ? -1 : 1));
  console.error(`listening_sample: run ${run} (seed 0x${meta.seed.toString(16)}), ${rows.size} measured, ${pool.length} in the pool`);
  const rng = mulberry32(SELECT_SEED), used = new Set(), items = [];
  const key = r => `${r.mode}#${r.i}`;
  const pseedOf = r => hash32(meta.seed, r.i, RENDER_SALT);
  const soloCache = new Map();
  const solo = r => { const k = key(r); if (!soloCache.has(k)) soloCache.set(k, roughnessSolo(samplePatch(meta.seed, r.i, r.mode).patch, pseedOf(r))); return soloCache.get(k); };
  const shuffled = list => { const a = list.slice(); for (let k = a.length - 1; k > 0; k--) { const j = Math.floor(rng() * (k + 1)); [a[k], a[j]] = [a[j], a[k]]; } return a; };
  const take = (cands, n, role, stratum, want) => {
    let got = 0;
    for (const r of shuffled(cands)) {
      if (got >= n) break;
      if (used.has(key(r)) || exclude.has(key(r))) continue;
      if (want && roughOrigin(r.roughness, solo(r), T.roughness) !== want) continue;
      used.add(key(r)); got++; items.push({ r, role, stratum });
    }
    console.error(`  ${stratum.padEnd(34)} ${got}/${n} from ${cands.length} candidates`);
    if (got < n) throw new Error(`stratum ${stratum}: only ${got} of ${n} — widen the pool (a bigger gauntlet run), never the band silently`);
  };
  for (const s of STRATA) for (const band of ['fine', 'border', 'flag']) for (const [lo, hi, n, want] of s[band]) {
    const cands = pool.filter(r => r[s.metric] >= lo && r[s.metric] < hi);
    take(cands, n, 'stratum', stratumLabel(s.metric, band, lo, hi, want), want);
  }
  take(pool.filter(CONTROLS.clean), 2, 'control-clean', 'control clean', null);
  take(pool.filter(CONTROLS.broken), 2, 'control-broken', 'control broken', null);
  const border = items.filter(x => /border/.test(x.stratum));
  const reps = shuffled(border).slice(0, 2);

  const out = items.map(({ r, role, stratum }) => {
    const patch = samplePatch(meta.seed, r.i, r.mode).patch, pseed = pseedOf(r);
    const metrics = Object.fromEntries(KEEP.map(k => [k, r[k]]));
    metrics.roughnessSolo = solo(r);
    return { key: key(r), i: r.i, mode: r.mode, role, stratum, roughOrigin: roughOrigin(r.roughness, metrics.roughnessSolo, T.roughness),
      metrics, ph: patchHash(patch), fp: fingerprintOf(patch, pseed) };
  });
  for (const x of reps) {
    const o = out.find(it => it.key === key(x.r));
    out.push(Object.assign({}, o, { key: o.key + '~r', role: 'repeat', stratum: 'repeat of ' + o.key, repeatOf: o.key }));
  }
  const json = {
    schema: 'hypersaw.listening-pass.sample/1',
    about: 'B324 blind listening pass: the patches are regenerated from these gauntlet seeds (tools/patchspace/gauntlet.mjs samplePatch, render seed hash32(run, i, 77)); metrics are gauntlet measure() values. Written by tools/patchspace/listening_sample.mjs; never hand-edit.',
    run: { seed: meta.seed, broad: meta.broad, edge: meta.edge, name: run },
    selectSeed: SELECT_SEED, orderSeed: ORDER_SEED,
    excluded: [...exclude].sort().map(k => ({ key: k, why: 'measured outside tolerance when rendered in the browser (Chrome 153, listening-pass.html?xverify=1): a chaotic patch whose render amplifies the runtimes\' last-bit libm differences' })),
    thresholds: gatesOf(T),
    roughOrigin: { beatingMax: ORIGIN.beatingMax, partialsMin: T.roughness, how: 'roughnessSolo = the same patch rendered with N 1 (one member), TONAL window' },
    phrase: 'the TONAL render (A3, vel 0.8, 0.5 s) then the ALIAS render (E5, 0.3 s), each followed by its release; the first samples are exactly the renders measured',
    id: sampleId(out),
    items: out,
  };
  writeFileSync(join(ROOT, SAMPLE_FILE), JSON.stringify(json, null, 1) + '\n');
  console.error(`listening_sample: wrote ${SAMPLE_FILE}: ${out.length} items, id ${json.id}`);
}
if (process.argv[1] && process.argv[1].endsWith('listening_sample.mjs')) main();
