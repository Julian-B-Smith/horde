/*
 * listening_pass_check.mjs — the blind listening pass plays what was measured, measures what it
 * plays, stays blind, keeps the human's v1 pass, and exports what calibrate.mjs reads.
 * HYPERSAW, 2026-09-28. v1: ROADMAP B324 (records PR #819, branch lead-records-126), "Help me
 * organize the listening pass, please". v2: ROADMAP B340 (records PR #833, branch
 * lead-records-135), "I completed the listening pass but I don't entirely trust it".
 *
 * WHAT IS PROVEN (the page's PURE block — docs/design/listening-pass.html between the
 * PURE-BEGIN / PURE-END markers — is sliced and run here, not re-implemented):
 *   T1 the committed sample is whole: its id is its content hash, its thresholds are
 *      gauntlet.mjs THRESH, every item carries every kept metric.
 *   T2 SAMPLER: for every seed, the page's samplePatch (over the lab table it slices and the
 *      dependency tree) equals gauntlet.mjs samplePatch, key for key, and hashes to the
 *      committed `ph` (the one identity the page can check exactly in any browser).
 *   T3 RENDER ROUTE: for every seed, the v2 program's A3 and E5 segments (fresh engines, the
 *      page's chunked renderSteps) START WITH the measured renders bit for bit — their
 *      prefixes fingerprint to the committed `fp`, as space.mjs render (the gauntlet's route)
 *      does. The page's own measurement of those windows (measureHeard) reproduces the
 *      committed numbers exactly here; its tolerance reads zero on a patch against itself and
 *      flags a patch against another stratum's numbers (must-fail).
 *   T4 DETERMINISM: every seed re-measured with gauntlet.mjs measure() reproduces every
 *      committed metric exactly (and the one-member roughness and its origin tag).
 *   T5 MUST-FAIL: a perturbed seed (index + 1, and run seed ^ 1) is caught — different
 *      metrics and a different fingerprint. A check that cannot fail proves nothing.
 *   T6 BLIND: the intro (fresh, with a v1 pass, resuming), the calibration step (checking and
 *      with verdicts), every rating view (empty and answered) and the finish view contain none
 *      of the reveal's vocabulary; CONTROL: the reveal itself contains every patch's values.
 *   T7 EXPORT: v1 exports validate (complete and partial) and keep their must-fail controls;
 *      the page's v1-pass export is byte-identical to the v1 page's own (pinned), for today's
 *      sample and for the earlier B324 sample the pass may have been rated on; v2 exports
 *      validate; CONTROLS: a v2 export missing segments, with a foreign segment field, a bad
 *      program id, or a timestamp is rejected.
 *   (B345: the sample was re-measured in place with the fixed metrics — same patches, seeds and
 *      order, a new id; T7 reaches the human's v1 pass through the file's `remeasured` ids and
 *      pins it as before; T8's v1-fit pins were recomputed with the same pre-B340 calibrate on
 *      the new numbers, and T8 proves calibrate's re-measure: new segments replace the heard
 *      ones, drift is named by segment and field, noiseDb is fitted beside flatness.)
 *   T8 CALIBRATE: v1 fits are byte-identical to the pre-B340 calibrate.mjs (pinned hashes);
 *      a planted rater is recovered, a coin flip is not; a drift-marked v1 rating is left out;
 *      a v2 export is fitted on the WORST heard segment (not the committed number); CONTROL:
 *      a v1 + v2 mix is refused.
 *   T9 ORDER: the presentation order is a fixed permutation of every key, repeats at least 8
 *      apart from their originals, and a different seed gives a different order.
 *   T10 DETECTOR CONTROLS (the detector-shares-assumption trap), the page's detectorControls()
 *      through the page's measureWindows() and metrics.mjs: the sine reads clean, root clear
 *      and not noisy on every note and clean on the sweep; a NAIVE saw reads aliased at E5
 *      (held, and with a 30-cent vibrato) and on the sweep; the additive saw reads clean in all
 *      three; a tritone-off sine reads root
 *      unclear; seeded noise reads noisy. A failing row means the METRIC is broken: it is
 *      reported, never tuned around. INFO rows (a semitone-sharp sine) are printed, not
 *      asserted: they measure the root metric's resolution limit.
 *   T11 THE HUMAN'S v1 PASS: no write in the page can reach its localStorage key — every
 *      setItem/removeItem names the v2 key, nothing calls clear(), the v1 key is only ever
 *      read; CONTROL: planted writes to it are caught.
 *   T12 MEASURE WHAT IS PLAYED: measureWindows() slides the gauntlet's own windows across
 *      each segment; restricted to the first window it IS the gauntlet's measurement (exact);
 *      the engine's bend really sweeps (the sweep's first chunk
 *      has its root at A1, its last at A6; CONTROL: not the other way round); two patches'
 *      whole programs measure into valid v2 segments.
 * WHY verify full, not fast: it re-renders 36 patches several ways (~1-2 min), and the sample
 * is a GOLDEN against the engine of the day — an engine edit that moves a patch reddens here
 * by design (regenerate: listening_sample.mjs; see its header), which belongs in the
 * human-paced leg, not in every agent's fast loop.
 *
 * Usage: node tools/patchspace/listening_pass_check.mjs        exit 1 on any failure
 */
// WIRED: ./verify full
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, SR, loadEngine, loadSpace, mulberry32 } from './space.mjs';
import { samplePatch, measure, THRESH } from './gauntlet.mjs';
import { KEEP, fingerprintOf, roughnessSolo, roughOrigin, sampleId, patchHash, RENDER_SALT, SAMPLE_FILE, loadPage } from './listening_sample.mjs';
import { hash32 } from './gen_dependency_tree.mjs';
import * as CAL from './calibrate.mjs';
import * as MET from './metrics.mjs';

const PAGE = 'docs/design/listening-pass.html';
const read = rel => readFileSync(join(ROOT, rel), 'utf8');
let fails = 0, checks = 0, controls = 0;
const ok = (cond, what, detail) => { checks++; if (!cond) fails++; console.log(`${cond ? 'OK  ' : 'FAIL'} ${what}${detail ? ' — ' + detail : ''}`); return cond; };
const caught = (cond, what, detail) => { controls++; return ok(cond, 'CONTROL ' + what, detail); };
const fnv = s => { let h = 0x811C9DC5; for (let k = 0; k < s.length; k++) { h ^= s.charCodeAt(k); h = Math.imul(h, 0x01000193) >>> 0; } return h.toString(16).padStart(8, '0'); };

const HTML = read(PAGE);
const P = loadPage(HTML);                                     // the page's PURE block (listening_sample.mjs, shared with calibrate --remeasure)
const sample = JSON.parse(read(SAMPLE_FILE));
sample.itemsByKey = Object.fromEntries(sample.items.map(it => [it.key, it]));
const run = sample.run.seed, { Composed } = loadEngine();
const pageCtx = { lab: P.labFrom(read('docs/design/scalpel-interface-lab.html')), tree: P.asEvalTree(JSON.parse(read('tools/patchspace/dependency_tree.json'))) };
pageCtx.space = P.spaceFrom(pageCtx.lab, new Composed(SR));
const pseed = it => hash32(run, it.i, RENDER_SALT);
const seg = (patch, it, k) => P.runSync(P.renderSegmentSteps(Composed, patch, pseed(it), P.SEGS[k], 4096));   // chunked, as the page renders
const t0 = Date.now();

/* T1 */
const { itemsByKey, ...plain } = sample;
ok(sampleId(plain.items) === sample.id, 'T1 sample id is the items\' content hash', sample.id);
ok(['aliasDb', 'roughness', 'rootPresence', 'flatness'].every(k => sample.thresholds[k] === THRESH[k]), 'T1 sample thresholds are gauntlet.mjs THRESH');
ok(sample.items.length >= 30 && sample.items.length <= 40, 'T1 30-40 patches', String(sample.items.length));
ok(sample.items.every(it => KEEP.every(k => k in it.metrics) && 'roughnessSolo' in it.metrics && /^[0-9a-f]{8}$/.test(it.fp) && /^[0-9a-f]{8}$/.test(it.ph)), 'T1 every item carries every kept metric, a patch hash and a fingerprint');
ok((sample.excluded || []).every(x => !sample.itemsByKey[x.key] && x.why), 'T1 no cross-runtime exclusion is in the sample, and each says why', (sample.excluded || []).map(x => x.key).join(', ') || 'none');
ok(P.pseedOf(run, 5) === hash32(run, 5, RENDER_SALT), 'T1 the page\'s render seed is the gauntlet\'s');

/* T2 */
ok(pageCtx.space.params.map(p => p.key).join() === loadSpace().params.map(p => p.key).join(), 'T2 the page\'s parameter list and order are space.mjs\'s', `${pageCtx.space.params.length} params`);
let sameP = 0;
for (const it of sample.items) if (JSON.stringify(P.samplePatch(pageCtx, run, it.i, it.mode).patch) === JSON.stringify(samplePatch(run, it.i, it.mode).patch)) sameP++;
ok(sameP === sample.items.length, 'T2 page sampler = gauntlet sampler for every seed', `${sameP}/${sample.items.length}`);
const hashOk = sample.items.filter(it => { const p = samplePatch(run, it.i, it.mode).patch; return P.patchHash(p) === it.ph && patchHash(p) === it.ph; }).length;
ok(hashOk === sample.items.length, 'T2 every regenerated patch hashes to the committed ph (page and tool agree)', `${hashOk}/${sample.items.length}`);

/* T3 */
let fpPage = 0, fpSpace = 0;
const segCache = new Map();
for (const it of sample.items) {
  const patch = samplePatch(run, it.i, it.mode).patch, rs = [];
  rs[P.SEG_A3] = seg(patch, it, P.SEG_A3); rs[P.SEG_E5] = seg(patch, it, P.SEG_E5);
  if (P.programFp(rs) === it.fp) fpPage++;
  if (fingerprintOf(patch, pseed(it)) === it.fp) fpSpace++;
  if (segCache.size < 6 && it.role !== 'repeat') segCache.set(it.key, { patch, a: rs[P.SEG_A3], b: rs[P.SEG_E5] });
}
ok(fpPage === sample.items.length, 'T3 the v2 program\'s A3 and E5 segments start with the measured renders, bit for bit', `${fpPage}/${sample.items.length}`);
ok(fpSpace === sample.items.length, 'T3 space.mjs render (the gauntlet\'s route) matches the committed fingerprints', `${fpSpace}/${sample.items.length}`);
{
  const bad = [];
  for (const [key, c] of segCache) {
    const it = sample.itemsByKey[key], h = P.measureHeard(MET, Composed, c.patch, pseed(it), c.a, c.b);
    for (const k of Object.keys(h)) if (!Object.is(h[k], it.metrics[k])) bad.push(`${it.key}.${k} ${h[k]} vs ${it.metrics[k]}`);
  }
  ok(bad.length === 0, 'T3 the page\'s own measurement of those windows (measureHeard over metrics.mjs) reproduces the committed numbers in Node', bad.length ? bad.slice(0, 3).join('; ') : `${segCache.size} patches × 5 metrics, exact`);
  ok(sample.items.every(it => P.heardDiff(it.metrics, it.metrics).ok), 'T3 the drift tolerance reads zero on every patch against itself');
  const a = sample.items.find(it => /aliasDb fine/.test(it.stratum)), z = sample.items.find(it => /flatness flag/.test(it.stratum));
  caught(!P.heardDiff(a.metrics, z.metrics).ok, 'T3 the drift tolerance flags one patch\'s numbers against another stratum\'s', `${a.key} vs ${z.key}`);
}

/* T4 */
const diffs = [];
for (const it of sample.items) {
  if (it.role === 'repeat') continue;                              // its original is measured
  const patch = samplePatch(run, it.i, it.mode).patch, m = measure(patch, pseed(it));
  for (const k of KEEP) if (!Object.is(m[k], it.metrics[k])) diffs.push(`${it.key}.${k} ${m[k]} vs ${it.metrics[k]}`);
  const solo = roughnessSolo(patch, pseed(it));
  if (!Object.is(solo, it.metrics.roughnessSolo)) diffs.push(`${it.key}.roughnessSolo ${solo} vs ${it.metrics.roughnessSolo}`);
  if (roughOrigin(it.metrics.roughness, solo, THRESH.roughness) !== it.roughOrigin) diffs.push(`${it.key}.roughOrigin`);
}
ok(diffs.length === 0, 'T4 every seed re-measures to the committed metrics exactly', diffs.length ? diffs.slice(0, 4).join('; ') : `${KEEP.length + 1} fields × ${sample.items.filter(i => i.role !== 'repeat').length} patches`);

/* T5 */
{
  const it = sample.items[0], near = samplePatch(run, it.i + 1, it.mode).patch, mNear = measure(near, hash32(run, it.i + 1, RENDER_SALT));
  caught(KEEP.some(k => !Object.is(mNear[k], it.metrics[k])) && fingerprintOf(near, hash32(run, it.i + 1, RENDER_SALT)) !== it.fp,
    'T5 a perturbed seed (index + 1) reads different metrics and a different fingerprint', `${it.key} → ${it.mode}#${it.i + 1}`);
  const it2 = sample.items[1], other = samplePatch(run ^ 1, it2.i, it2.mode).patch;
  caught(fingerprintOf(other, hash32(run ^ 1, it2.i, RENDER_SALT)) !== it2.fp, 'T5 a perturbed run seed (seed ^ 1) is a different sound', it2.key);
}

/* T6 */
const order = P.presentationOrder(sample.items, sample.orderSeed), n = order.length;
const tokens = P.blindTokens(sample), strip = h => h.replace(/style="[^"]*"/g, '');
const st = { order, cursor: 0, answers: {}, segs: {}, heard: {}, revealed: false, loop: false, refBefore: 'off', calibrated: false };
let leaks = [], views = 0;
const scan = (html, where) => { views++; const h = P.blindScan(strip(html), tokens); if (h.length) leaks.push(`${where}: ${h.slice(0, 3).join(', ')}`); };
const v1full = { n, total: n, complete: true, which: 'current', id: sample.id }, v1other = { n: 12, total: n, complete: false, which: 'unknown', id: 'deadbeef' }, v1early = { n, total: n, complete: true, which: 'earlier', id: '03c97d3e' };
scan(P.viewIntro(st, null), 'intro (fresh)'); scan(P.viewIntro(st, v1full), 'intro (v1 pass)'); scan(P.viewIntro(st, v1other), 'intro (v1, unknown sample)'); scan(P.viewIntro(st, v1early), 'intro (v1, earlier sample)');
scan(P.viewCalib(st, null, false), 'calib (checking)');
scan(P.viewCalib(st, { e5: { saw: 'clean', naive: 'aliased' }, sweep: { saw: 'clean', naive: 'aliased', sine: 'clean' } }, true), 'calib (verdicts)');
const playStates = [{ text: 'stopped — space plays the patch, S / W a reference on the same notes', sel: -1, nowSeg: -1 },
  { text: 'playing: the patch · SWEEP A1→A6 (looped)', sel: P.SEG_SWEEP, nowSeg: P.SEG_SWEEP, what: 'patch' }, { text: 'playing: SINE reference · A4', sel: 3, nowSeg: -1, what: 'sine' }];
order.forEach((key, pos) => scan(P.viewRate(st, key, pos, n, playStates[pos % 3], false), `rate ${pos} empty`));
const rng = mulberry32(0xB324);
for (const key of order) { const a = {}; for (const q of P.QUI) a[q.key] = rng() < 0.4 ? 1 : 0; a.note = 'a note'; st.answers[key] = a; }
order.forEach((key, pos) => scan(P.viewRate(Object.assign({}, st, { loop: true, refBefore: 'saw' }), key, pos, n,
  { text: 'DRIFT: this sound measures differently here than in the gauntlet — rate it anyway; your answer is fitted on what this page measured', warn: true, sel: pos % 7 }, true), `rate ${pos} answered`));
scan(P.viewIntro(st, v1full), 'intro (resume)'); scan(P.viewDone(st, 0), 'done'); scan(P.viewDone(st, 3), 'done (measuring)');
ok(leaks.length === 0, 'T6 no view before the reveal carries a metric, stratum, role or measured value', leaks.length ? leaks.slice(0, 3).join(' | ') : `${tokens.length} tokens × ${views} views`);
/* synthetic heard segments for every patch: the committed numbers, one segment made worse, so the
   reveal and the v2 fit have something to read (T7, T8) */
const synthSegs = it => { const m = it.metrics; return [{ seg: 'A3', aliasDb: m.aliasDb, roughness: m.roughness, rootPresence: m.rootPresence, rootInterval: m.rootInterval, flatness: m.flatness, rmsDb: m.rmsDb },
  { seg: 'sweep', aliasDb: Math.min(0, m.aliasDb + 20), aliasAtNote: 90.5, roughness: null, rootPresence: null, rootInterval: null, flatness: null, rmsDb: m.rmsDb }]; };
for (const key of order) st.segs[key] = synthSegs(sample.itemsByKey[key]);
const x2full = P.buildExportV2(st, sample, CAL.SCHEMA_V2);
const A2 = CAL.analyse(sample, [x2full]), W = Object.fromEntries(order.map(k => [k, CAL.worstOf(st.segs[k])]));
const rv = strip(P.viewReveal(st, sample, A2, W, null, false));
const shown = sample.items.filter(it => ['aliasDb', 'roughness', 'rootPresence', 'flatness'].every(m => rv.includes(P.fmtM(m, it.metrics[m])))).length;
caught(shown === sample.items.length && P.blindScan(rv, tokens).length > 0, 'T6 the reveal carries every patch\'s measurements (the scan can see them)', `${shown}/${sample.items.length}`);

/* T7 */
const full = P.buildExport(st, sample, CAL.SCHEMA), vFull = CAL.validateExport(full);
ok(vFull.ok && full.complete === true && full.ratings.length === n, 'T7 a complete v1 export validates', vFull.errors.slice(0, 2).join('; '));
const part = P.buildExport({ order, answers: { [order[0]]: st.answers[order[0]] } }, sample, CAL.SCHEMA), vPart = CAL.validateExport(part);
ok(vPart.ok && part.complete === false, 'T7 a partial v1 export validates as incomplete', vPart.errors.slice(0, 2).join('; '));
ok(P.QUI.map(q => q.key).join() === CAL.QUESTIONS.map(q => q.key).join(), 'T7 the page asks calibrate.mjs\'s questions, in order');
caught(!CAL.validateExport(Object.assign({}, full, { savedAt: '2026-09-28T12:00:00Z' })).ok, 'T7 a timestamped export is rejected');
const bad = JSON.parse(JSON.stringify(full)); bad.ratings[0].answers.aliased = 2;
caught(!CAL.validateExport(bad).ok, 'T7 an out-of-range answer is rejected');
const gap = JSON.parse(JSON.stringify(full)); gap.ratings[3].answers.noisy = null;
caught(!CAL.validateExport(gap).ok, 'T7 a "complete" export with an unanswered question is rejected');
{
  /* the human's v1 pass exported from its store: byte-identical to what the v1 page's own
     buildExport wrote for the same store, for EITHER sample the pass may have been rated on.
     Pins computed 2026-09-28 with the historical pages' own buildExport over a synthetic store
     (33 of 36 answered, notes, match/drift marks, seeded 0xB340, over that sample's order):
     today's sample cec81302 with origin/main 9860227's page -> 667f6b8e; the earlier B324
     sample 03c97d3e with 1726739's page and 1726739's sample file -> 5f02623f.
     B345 re-measured cec81302 IN PLACE (listening_sample.mjs --remeasure: the same patches, seeds
     and order, new committed numbers, so a new id). A pass saved on cec81302 was rated on these
     very sounds; the page reaches it through the file's `remeasured` list (sameSoundsAs), and its
     export must still be the v1 page's own, byte for byte: the pin is unchanged, only the id it
     is reached by is now the historical one. */
  const storeFor = (id, ord) => { const r = mulberry32(0xB340), answers = {}, heard = {};
    ord.slice(0, ord.length - 3).forEach((k, i) => { answers[k] = { aliased: r() < .4 ? 1 : 0, rough: r() < .4 ? 1 : 0, rootUnclear: r() < .4 ? 1 : 0, noisy: r() < .4 ? 1 : 0, unusable: r() < .4 ? 1 : 0 };
      if (i % 5 === 0) answers[k].note = 'note ' + i; if (i % 7 === 0) heard[k] = i % 2 ? 'drift' : 'match'; });
    return { sampleId: id, cursor: 33, answers, heard, revealed: true }; };
  const pin = (id, want) => {
    const smp = P.v1SampleOf(id, sample), x = P.exportV1FromStore(storeFor(id, P.presentationOrder(smp.items, smp.orderSeed)), sample, CAL.SCHEMA);
    const h = fnv(JSON.stringify(x, null, 1) + '\n');
    ok(h === want && CAL.validateExport(x).ok && x.sample.id === id, `T7 a v1 pass on sample ${id} exports byte-identical to the v1 page's own export (pinned) and validates as v1`, `${h} (want ${want})`);
  };
  pin('cec81302', '667f6b8e');
  pin('03c97d3e', '5f02623f');
  ok(P.sameSoundsAs(sample.id, sample) && (sample.remeasured || []).every(r => P.sameSoundsAs(r.from, sample)) && (sample.remeasured || []).some(r => r.from === 'cec81302'),
    'T7 the sample\'s earlier ids (re-measured in place) count as the same sounds', (sample.remeasured || []).map(r => `${r.from} -> ${r.to}`).join(', '));
  caught(!P.sameSoundsAs('03c97d3e', sample) && !P.sameSoundsAs('deadbeef', sample), 'T7 a different sample (03c97d3e: other patches) is not the same sounds');
  caught(P.exportV1FromStore({ sampleId: 'deadbeef', answers: { 'broad#1': {} } }, sample, CAL.SCHEMA) === null, 'T7 a v1 store on a sample this page does not know is not exported');
}
const v2ok = CAL.validateExport(x2full);
ok(v2ok.ok && x2full.complete && x2full.phrase.id === P.PHRASE_ID && x2full.ratings.every(r => Array.isArray(r.segs)), 'T7 a complete v2 export validates, with the program id and every rating\'s segments', v2ok.errors.slice(0, 2).join('; '));
{
  const miss = JSON.parse(JSON.stringify(x2full)); miss.ratings[4].segs = null;
  caught(!CAL.validateExport(miss).ok, 'T7 a "complete" v2 export with an unmeasured rating is rejected');
  const partial = Object.assign(JSON.parse(JSON.stringify(miss)), { complete: false }); partial.ratings[5].answers.noisy = null;
  ok(CAL.validateExport(partial).ok, 'T7 a partial v2 export with an unmeasured rating validates');
  const foreign = JSON.parse(JSON.stringify(x2full)); foreign.ratings[0].segs[0].loudness = 3;
  caught(!CAL.validateExport(foreign).ok, 'T7 a v2 segment with a field calibrate does not know is rejected');
  const badId = JSON.parse(JSON.stringify(x2full)); badId.phrase.id = 'v2';
  caught(!CAL.validateExport(badId).ok, 'T7 a v2 export without a program id is rejected');
  caught(!CAL.validateExport(Object.assign({}, x2full, { savedAt: 'wall clock' })).ok, 'T7 a timestamped v2 export is rejected');
}

/* T8 */
{
  /* v1 FITS UNCHANGED: analyse() of the complete v1 export (and of it with one drift and one
     match mark) hashed; the pins were computed with the pre-B340 calibrate.mjs (last changed in
     1726739, as of origin/main 9860227) on the same deterministic exports.
     B345: a v1 fit reads the COMMITTED numbers, which the re-measure changed (aliasDb, and the
     new noiseDb), so the pins were RECOMPUTED, again with 1726739's calibrate.mjs, on the
     re-measured file: 60ae017d 791cc92a 921c14fd -> 1726cdb7 8b35c25a 369a01c9. The same
     calibrate on the previous file (cec81302) still gives the old pins, so what the pin guards —
     the v1 code path is pre-B340's, byte for byte — is unchanged; the data under it moved. */
  const drift = P.buildExport(Object.assign({}, st, { heard: { [order[0]]: 'drift', [order[1]]: 'match' } }), sample, CAL.SCHEMA);
  const hA = fnv(JSON.stringify(CAL.analyse(sample, [full]))), hB = fnv(JSON.stringify(CAL.analyse(sample, [drift]))), hT = fnv(CAL.textReport(CAL.analyse(sample, [full])));
  ok(hA === '1726cdb7' && hB === '8b35c25a' && hT === '369a01c9', 'T8 v1 exports fit exactly as before B340 (analyse and report hashes pinned)', `${hA} ${hB} ${hT} (want 1726cdb7 8b35c25a 369a01c9)`);
}
const prim = sample.items.filter(it => it.role !== 'repeat');
const planted = CAL.fitCut(prim.map(it => it.metrics.aliasDb), prim.map(it => (it.metrics.aliasDb > -20 ? 1 : 0)), 'above');
ok(planted.auc === 1 && planted.bracket[0] <= -20 && planted.bracket[1] >= -20, 'T8 calibrate recovers a planted rater\'s cut (-20 dB)',
  `AUC ${planted.auc}, cut ${planted.cut && planted.cut.toFixed(2)}, bracket ${planted.bracket && planted.bracket.map(v => v.toFixed(2)).join(' .. ')}`);
const coin = mulberry32(0xC014);
const noise = CAL.fitCut(prim.map(it => it.metrics.aliasDb), prim.map(() => (coin() < 0.5 ? 1 : 0)), 'above');
caught(noise.insufficient || noise.auc < 0.9, 'T8 a coin-flip rater is not "recovered"', noise.insufficient ? 'insufficient' : `AUC ${noise.auc.toFixed(2)}`);
{
  const st2 = Object.assign({}, st, { heard: { [order[0]]: 'drift', [order[1]]: 'match' } }), x2 = P.buildExport(st2, sample, CAL.SCHEMA);
  const A = CAL.analyse(sample, [x2]);
  ok(CAL.validateExport(x2).ok && A.drifted.length === 1 && A.drifted[0] === order[0], 'T8 a v1 rating marked heard: drift is left out of the fit, and named', A.drifted.join(', '));
}
{
  /* v2 fits the WORST HEARD segment: a rater who calls "aliased" exactly when the synthetic
     sweep (committed + 20 dB) exceeds -10 dB is recovered on the heard numbers — and the same
     answers read against the committed numbers put the cut 20 dB lower (the two bases differ) */
  const stP = Object.assign({}, st, { answers: {} });
  for (const k of order) { const a = Object.assign({}, st.answers[k]); a.aliased = CAL.worstOf(st.segs[k]).values.aliasDb > -10 ? 1 : 0; stP.answers[k] = a; }
  const A = CAL.analyse(sample, [P.buildExportV2(stP, sample, CAL.SCHEMA_V2)]), m = A.metrics.find(x => x.metric === 'aliasDb'), mc = A.metricsCommitted.find(x => x.metric === 'aliasDb');
  ok(A.version === 2 && m.auc === 1 && m.bracket[0] <= -10 && m.bracket[1] >= -10 && mc.cut < m.cut - 15 && A.worstFrom.aliasDb.sweep > 0,
    'T8 a v2 export is fitted on the worst heard segment (planted -10 dB on the heard numbers recovered)', `AUC ${m.auc}, bracket ${m.bracket.map(v => v.toFixed(1)).join(' .. ')}; committed-basis cut ${mc.cut.toFixed(1)}; worst from ${JSON.stringify(A.worstFrom.aliasDb)}`);
  let threw = false; try { CAL.analyse(sample, [full, x2full]); } catch (_) { threw = true; }
  caught(threw, 'T8 a v1 + v2 mix is refused (two bases are never pooled)');
}
{
  /* B345 RE-MEASURE (calibrate.mjs remeasureExport): segments re-measured now replace the
     heard ones; a field whose METHOD did not change (DRIFT_FIELDS) that moves beyond the page's
     tolerance marks the rating drift and names it; aliasDb, whose method B345 changed, never does */
  const same = CAL.remeasureExport(x2full, r => r.segs.map(g => Object.assign({}, g, { aliasDb: g.aliasDb === null ? null : g.aliasDb - 12 })), P.heardDiff);
  ok(Object.keys(same.drift).length === 0 && same.export.ratings.every((r, k) => r.heard === 'match' && r.segs[0].aliasDb === (x2full.ratings[k].segs[0].aliasDb === null ? null : x2full.ratings[k].segs[0].aliasDb - 12)),
    'T8 a re-measure replaces the segments, and a changed-method field (aliasDb) is not drift', `${same.export.ratings.length} ratings`);
  const k0 = x2full.ratings[3].key;
  const moved = CAL.remeasureExport(x2full, r => r.segs.map(g => (r.key === k0 && g.seg === 'A3' ? Object.assign({}, g, { roughness: g.roughness + 0.5 }) : g)), P.heardDiff);
  caught(Object.keys(moved.drift).length === 1 && moved.drift[k0] && moved.drift[k0][0] === 'A3.roughness' && moved.export.ratings[3].heard === 'drift',
    'T8 a re-measured unchanged-method field outside tolerance marks that rating drift, by segment and field', JSON.stringify(moved.drift));
  const A = CAL.analyse(sample, [same.export], { pairs: CAL.PAIRS_B345, loo: true }), nz = A.metrics.find(m => m.metric === 'noiseDb');
  ok(nz && nz.provisional === null && nz.agreementProvisional === null && A.metrics.every(m => m.insufficient || (m.loo >= 0 && m.loo <= 1)),
    'T8 the B345 re-fit adds noiseDb beside flatness (no provisional threshold) and a leave-one-out accuracy per metric', nz ? `noiseDb n ${nz.n}` : 'no noiseDb row');
}

/* T9 */
const reps = sample.items.filter(it => it.role === 'repeat');
ok(order.length === sample.items.length && new Set(order).size === order.length && sample.items.every(it => order.includes(it.key)), 'T9 the order is a permutation of every patch');
ok(reps.every(r => Math.abs(order.indexOf(r.key) - order.indexOf(r.repeatOf)) >= 8), 'T9 each repeat sits at least 8 patches from its original',
  reps.map(r => `${r.repeatOf} ${Math.abs(order.indexOf(r.key) - order.indexOf(r.repeatOf))} apart`).join(', '));
ok(P.presentationOrder(sample.items, sample.orderSeed).join() === order.join() && P.presentationOrder(sample.items, sample.orderSeed + 1).join() !== order.join(),
  'T9 the order is fixed by its seed (and moves with it)');

/* T10 */
{
  const det = P.detectorControls(MET, THRESH);
  for (const r of det) console.log('     ' + P.detectorLine(r));
  const asserted = det.filter(r => r.ok !== null), badRows = asserted.filter(r => !r.ok);
  const must = ['naive saw E5', 'naive saw E5 vibrato', 'naive saw sweep', 'tritone-off sine A1', 'seeded white noise'];
  ok(badRows.length === 0, 'T10 every detector control reads as constructed (sine clean / root clear / not noisy on every note; clean saw clean; naive saw aliased; tritone root unclear; noise noisy)',
    badRows.length ? 'THE METRIC IS BROKEN AT: ' + badRows.map(r => r.id).join(', ') : `${asserted.length} rows`);
  caught(must.every(id => det.some(r => r.id === id && r.ok)), 'T10 the must-read-HIGH rows are present and read high (naive saw E5 and sweep, tritone, noise)');
  ok(det.filter(r => r.ok === null).length === P.SEGS.length - 1, 'T10 the root metric\'s semitone resolution is measured on every note (INFO rows above)');
}

/* T11 */
{
  /* in the page's script: every localStorage write names STORE_KEY and nothing calls clear();
     the v1 key's literal appears once (its const); in CODE lines (comment lines skipped) the
     identifier appears only as its declaration or inside localStorage.getItem(...) */
  function writesToV1(src) {
    const s = src.slice(src.indexOf('<script>\n"use strict"')), hits = [];
    for (const m of s.matchAll(/localStorage\s*\.\s*(setItem|removeItem|clear)\s*\(([^,)]*)/g)) if (m[1] === 'clear' || m[2].trim() !== 'STORE_KEY') hits.push(m[0]);
    const lit = (s.match(/hypersaw\.b324\.listening-pass/g) || []).length;
    if (lit !== 1) hits.push(`the v1 key literal appears ${lit} times in the script (want exactly its one const)`);
    for (const line of s.split('\n')) {
      const t = line.trim();
      if (!t.includes('V1_STORE_KEY') || /^(\/\*|\*|\/\/)/.test(t)) continue;
      if (t.replace(/localStorage\.getItem\(V1_STORE_KEY\)/g, '').replace(/^const V1_STORE_KEY = /, '').includes('V1_STORE_KEY')) hits.push(t.slice(0, 80));
    }
    return hits;
  }
  const hits = writesToV1(HTML);
  ok(hits.length === 0, 'T11 no write in the page can reach the v1 pass (every setItem/removeItem names STORE_KEY; the v1 key is only read)', hits.slice(0, 3).join(' | '));
  const planted1 = HTML.replace('function clearStore() {', 'function clearStore() { localStorage.removeItem(V1_STORE_KEY);');
  const planted2 = HTML.replace('function clearStore() {', "function clearStore() { localStorage.setItem('hypersaw.b324.listening-pass', '{}');");
  const planted3 = HTML.replace('function clearStore() {', 'function clearStore() { localStorage.clear();');
  caught(writesToV1(planted1).length > 0 && writesToV1(planted2).length > 0 && writesToV1(planted3).length > 0, 'T11 planted writes to the v1 key (removeItem, setItem by literal, clear) are caught');
  ok(STORE_KEY_OF(HTML) === 'hypersaw.b340.listening-pass' && STORE_KEY_OF(HTML) !== 'hypersaw.b324.listening-pass', 'T11 v2 writes its own key', STORE_KEY_OF(HTML));
}
function STORE_KEY_OF(src) { const m = src.match(/const STORE_KEY = '([^']+)'/); return m ? m[1] : null; }

/* T12 */
{
  /* measureWindows on the gauntlet's own windows IS the gauntlet's measurement */
  const it = sample.items.find(x => x.role === 'control-broken'), patch = samplePatch(run, it.i, it.mode).patch;
  const a = seg(patch, it, P.SEG_A3), b = seg(patch, it, P.SEG_E5), os = patch.os || 2;
  const tonalSeg = Object.assign({}, P.SEGS[P.SEG_A3], { held: P.MEAS.tonal.n }), aliasSeg = Object.assign({}, P.SEGS[P.SEG_E5], { held: P.MEAS.alias.n });
  const mt = P.measureWindows(MET, tonalSeg, { L: a.L, R: a.R, sr: SR }, { L: a.L, R: a.R, sr: SR });
  const ref = P.renderScript(Composed, Object.assign({}, patch, { os: os * 4 }), P.segScript(aliasSeg, false), { seed: pseed(it), block: 128 });
  const ma = P.measureWindows(MET, aliasSeg, { L: b.L, R: b.R, sr: SR }, { L: ref.L, R: ref.R, sr: SR });
  const same = ['roughness', 'rootPresence', 'flatness', 'rmsDb'].every(k => Object.is(mt[k], it.metrics[k])) && Object.is(ma.aliasDb, it.metrics.aliasDb);
  ok(same, 'T12 measureWindows on the gauntlet\'s windows reproduces the gauntlet\'s numbers exactly (one measurement, not two)',
    `${it.key}: alias ${ma.aliasDb} vs ${it.metrics.aliasDb}, root ${mt.rootPresence} vs ${it.metrics.rootPresence}, rough ${mt.roughness} vs ${it.metrics.roughness}`);
  /* the engine's bend really sweeps: a clean control patch's sweep has its root at A1 at the start, A6 at the end */
  const c = sample.items.find(x => x.role === 'control-clean' && x.key === 'broad#472') || sample.items.find(x => x.role === 'control-clean');
  const cp = samplePatch(run, c.i, c.mode).patch, sw = seg(cp, c, P.SEG_SWEEP), S = P.SEGS[P.SEG_SWEEP];
  const rootAt = (from, noteNum) => MET.root(MET.spectrum(MET.mono(sw.L.subarray(from, from + 8192), sw.R.subarray(from, from + 8192)), SR), 440 * Math.pow(2, (noteNum - 69) / 12)).rootPresence;
  const startA1 = rootAt(2400, 33), endA6 = rootAt(S.held - 8192, 93), startA6 = rootAt(2400, 93);
  ok(startA1 > 0.9 && endA6 > 0.9, 'T12 the engine\'s bend really sweeps the held note from A1 to A6', `${c.key}: root at A1 over the first chunk ${startA1.toFixed(3)}, at A6 over the last ${endA6.toFixed(3)}`);
  caught(startA6 < 0.5, 'T12 the sweep\'s first chunk does not read as A6', startA6.toFixed(3));
  /* two whole programs measured segment by segment, as the page does, into valid v2 segments */
  const segsOf = x => { const p = samplePatch(run, x.i, x.mode).patch; return P.SEGS.map((s, k) => P.runSync(P.measureSegmentSteps(MET, Composed, p, pseed(x), s, seg(p, x, k), 4096))); };
  const two = [c, it].map(x => ({ x, segs: segsOf(x) }));
  const xv = CAL.validateExport({ schema: CAL.SCHEMA_V2, sample: { file: SAMPLE_FILE, id: sample.id }, orderSeed: sample.orderSeed, phrase: { version: 2, id: P.PHRASE_ID }, complete: false,
    ratings: two.map((t, o) => ({ order: o, key: t.x.key, run, i: t.x.i, mode: t.x.mode, answers: Object.fromEntries(CAL.QUESTIONS.map(q => [q.key, null])), note: '', heard: null, segs: t.segs })) });
  const lines = two.map(t => `${t.x.key}: ` + t.segs.map(s => `${s.seg} ${s.aliasDb}${s.aliasAtNote ? '@' + s.aliasAtNote : ''}`).join(' · '));
  ok(xv.ok && two.every(t => t.segs.length === P.SEGS.length && t.segs[P.SEG_SWEEP].rootPresence === null && Number.isFinite(t.segs[P.SEG_SWEEP].aliasDb)),
    'T12 whole programs measure into valid v2 segments (the sweep: aliasing and its pitch, no root)', xv.ok ? lines.join(' | ') : xv.errors.slice(0, 2).join('; '));
  console.log(`     program: ${P.SEGS.length} segments, ${P.PROGRAM_SECONDS} s of sound per patch, id ${P.PHRASE_ID}`);
}

console.log(`\n${fails ? 'RED' : 'GREEN'} — listening pass: ${checks - fails}/${checks} checks, ${controls} must-fail controls, ${sample.items.length} patches, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
process.exit(fails ? 1 : 0);
