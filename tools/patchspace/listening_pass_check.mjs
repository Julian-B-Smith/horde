/*
 * listening_pass_check.mjs — B324: the blind listening pass plays what was measured, stays
 * blind, and exports what calibrate.mjs reads. HYPERSAW, 2026-09-28, ROADMAP B324 (records
 * PR #819, branch lead-records-126). The human: "Help me organize the listening pass, please".
 *
 * WHAT IS PROVEN (the page's PURE block — docs/design/listening-pass.html between the
 * PURE-BEGIN / PURE-END markers — is sliced and run here, not re-implemented):
 *   T1 the committed sample is whole: its id is its content hash, its thresholds are
 *      gauntlet.mjs THRESH, every item carries every kept metric.
 *   T2 SAMPLER: for every seed, the page's samplePatch (over the lab table it slices and the
 *      dependency tree) equals gauntlet.mjs samplePatch, key for key, and hashes to the
 *      committed `ph` (the one identity the page can check exactly in any browser).
 *   T3 RENDER ROUTE: for every seed, the page's phrase fingerprints to the committed `fp`,
 *      and space.mjs render (the gauntlet's route) fingerprints to the same: IN NODE the page
 *      plays the samples that were measured, bit for bit. The page's own measurement
 *      (measureHeard, what it checks in the browser, whose libm is not Node's) reproduces the
 *      committed numbers exactly here; its tolerance reads zero on a patch against itself and
 *      flags a patch against another stratum's numbers (must-fail).
 *   T4 DETERMINISM: every seed re-measured with gauntlet.mjs measure() reproduces every
 *      committed metric exactly (and the one-member roughness and its origin tag).
 *   T5 MUST-FAIL: a perturbed seed (index + 1, and run seed ^ 1) is caught — different
 *      metrics and a different fingerprint. A check that cannot fail proves nothing.
 *   T6 BLIND: the intro, every rating view (empty and answered) and the finish view contain
 *      none of the reveal's vocabulary (metric names, strata, roles, every formatted value);
 *      CONTROL: the reveal itself contains every patch's values (must-read-high).
 *   T7 EXPORT: the page's export passes calibrate.mjs validateExport, complete and partial;
 *      CONTROLS: a timestamped export, an out-of-range answer and a "complete" export with a
 *      gap are rejected. The page's questions are calibrate's, in order.
 *   T8 CALIBRATE: a planted rater (aliased iff aliasDb > -20) is recovered — AUC 1 and a
 *      bracket containing -20; CONTROL: a seeded coin-flip rater is not (AUC < 0.9).
 *   T9 ORDER: the presentation order is a fixed permutation of every key, repeats at least 8
 *      apart from their originals, and a different seed gives a different order.
 * WHY verify full, not fast: T4 re-renders 36 patches four ways (~30-60 s under load), and
 * the sample is a GOLDEN against the engine of the day — an engine edit that moves a patch
 * reddens here by design (regenerate: listening_sample.mjs; see its header), which belongs in
 * the human-paced leg, not in every agent's fast loop.
 *
 * Usage: node tools/patchspace/listening_pass_check.mjs        exit 1 on any failure
 */
// WIRED: ./verify full
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, SR, loadEngine, loadSpace, mulberry32 } from './space.mjs';
import { samplePatch, measure, THRESH } from './gauntlet.mjs';
import { KEEP, fingerprintOf, roughnessSolo, roughOrigin, sampleId, patchHash, RENDER_SALT, SAMPLE_FILE } from './listening_sample.mjs';
import { hash32 } from './gen_dependency_tree.mjs';
import * as CAL from './calibrate.mjs';
import * as MET from './metrics.mjs';

const PAGE = 'docs/design/listening-pass.html';
const read = rel => readFileSync(join(ROOT, rel), 'utf8');
let fails = 0, checks = 0, controls = 0;
const ok = (cond, what, detail) => { checks++; if (!cond) fails++; console.log(`${cond ? 'OK  ' : 'FAIL'} ${what}${detail ? ' — ' + detail : ''}`); return cond; };
const caught = (cond, what, detail) => { controls++; return ok(cond, 'CONTROL ' + what, detail); };

function pagePure() {
  const html = read(PAGE), a = html.indexOf('// PURE-BEGIN'), b = html.indexOf('// PURE-END');
  if (a < 0 || b < a) throw new Error(`${PAGE}: PURE-BEGIN / PURE-END markers not found`);
  return new Function('"use strict";\n' + html.slice(a, b) + '\nreturn { labFrom, spaceFrom, asEvalTree, samplePatch, renderPhrase, pseedOf, ' +
    'presentationOrder, QUI, viewIntro, viewRate, viewDone, viewReveal, fmtM, blindTokens, blindScan, buildExport, patchHash, measureHeard, heardDiff };')();
}

const P = pagePure();
const sample = JSON.parse(read(SAMPLE_FILE));
sample.itemsByKey = Object.fromEntries(sample.items.map(it => [it.key, it]));
const run = sample.run.seed, { Composed } = loadEngine();
const pageCtx = { lab: P.labFrom(read('docs/design/scalpel-interface-lab.html')), tree: P.asEvalTree(JSON.parse(read('tools/patchspace/dependency_tree.json'))) };
pageCtx.space = P.spaceFrom(pageCtx.lab, new Composed(SR));
const pseed = it => hash32(run, it.i, RENDER_SALT);
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
for (const it of sample.items) {
  const patch = samplePatch(run, it.i, it.mode).patch;
  if (P.renderPhrase(Composed, patch, pseed(it)).fp === it.fp) fpPage++;
  if (fingerprintOf(patch, pseed(it)) === it.fp) fpSpace++;
}
ok(fpPage === sample.items.length, 'T3 the page\'s phrase starts with the measured renders, bit for bit', `${fpPage}/${sample.items.length}`);
ok(fpSpace === sample.items.length, 'T3 space.mjs render (the gauntlet\'s route) matches the committed fingerprints', `${fpSpace}/${sample.items.length}`);
{
  const few = sample.items.filter(it => it.role !== 'repeat').slice(0, 6), bad = [];
  for (const it of few) {
    const patch = samplePatch(run, it.i, it.mode).patch, h = P.measureHeard(MET, Composed, patch, pseed(it), P.renderPhrase(Composed, patch, pseed(it)).segs);
    for (const k of Object.keys(h)) if (!Object.is(h[k], it.metrics[k])) bad.push(`${it.key}.${k} ${h[k]} vs ${it.metrics[k]}`);
  }
  ok(bad.length === 0, 'T3 the page\'s own measurement (measureHeard over metrics.mjs) reproduces the committed numbers in Node', bad.length ? bad.slice(0, 3).join('; ') : `${few.length} patches × 5 metrics, exact`);
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
const st = { order, cursor: 0, answers: {}, revealed: false, loop: true };
let leaks = [];
const scan = (html, where) => { const h = P.blindScan(strip(html), tokens); if (h.length) leaks.push(`${where}: ${h.slice(0, 3).join(', ')}`); };
scan(P.viewIntro(st), 'intro (fresh)');
order.forEach((key, pos) => scan(P.viewRate(st, key, pos, n, { text: 'playing, looped', warn: false }, false), `rate ${pos} empty`));
const rng = mulberry32(0xB324);
for (const key of order) { const a = {}; for (const q of P.QUI) a[q.key] = rng() < 0.4 ? 1 : 0; a.note = 'a note'; st.answers[key] = a; }
order.forEach((key, pos) => scan(P.viewRate(st, key, pos, n, { text: 'DRIFT: this sound no longer matches the render that was measured — tell the lead', warn: true }, true), `rate ${pos} answered`));
scan(P.viewIntro(st), 'intro (resume)'); scan(P.viewDone(st), 'done');
ok(leaks.length === 0, 'T6 no view before the reveal carries a metric, stratum, role or measured value', leaks.length ? leaks.slice(0, 3).join(' | ') : `${tokens.length} tokens × ${2 * n + 3} views`);
const A = CAL.analyse(sample, [P.buildExport(st, sample, CAL.SCHEMA)]);
const rv = strip(P.viewReveal(st, sample, A, false));
const shown = sample.items.filter(it => ['aliasDb', 'roughness', 'rootPresence', 'flatness'].every(m => rv.includes(P.fmtM(m, it.metrics[m])))).length;
caught(shown === sample.items.length && P.blindScan(rv, tokens).length > 0, 'T6 the reveal carries every patch\'s measurements (the scan can see them)', `${shown}/${sample.items.length}`);

/* T7 */
const full = P.buildExport(st, sample, CAL.SCHEMA), vFull = CAL.validateExport(full);
ok(vFull.ok && full.complete === true && full.ratings.length === n, 'T7 a complete export validates', vFull.errors.slice(0, 2).join('; '));
const part = P.buildExport({ order, answers: { [order[0]]: st.answers[order[0]] } }, sample, CAL.SCHEMA), vPart = CAL.validateExport(part);
ok(vPart.ok && part.complete === false, 'T7 a partial export validates as incomplete', vPart.errors.slice(0, 2).join('; '));
ok(P.QUI.map(q => q.key).join() === CAL.QUESTIONS.map(q => q.key).join(), 'T7 the page asks calibrate.mjs\'s questions, in order');
caught(!CAL.validateExport(Object.assign({}, full, { savedAt: '2026-09-28T12:00:00Z' })).ok, 'T7 a timestamped export is rejected');
const bad = JSON.parse(JSON.stringify(full)); bad.ratings[0].answers.aliased = 2;
caught(!CAL.validateExport(bad).ok, 'T7 an out-of-range answer is rejected');
const gap = JSON.parse(JSON.stringify(full)); gap.ratings[3].answers.noisy = null;
caught(!CAL.validateExport(gap).ok, 'T7 a "complete" export with an unanswered question is rejected');

/* T8 */
const prim = sample.items.filter(it => it.role !== 'repeat');
const planted = CAL.fitCut(prim.map(it => it.metrics.aliasDb), prim.map(it => (it.metrics.aliasDb > -20 ? 1 : 0)), 'above');
ok(planted.auc === 1 && planted.bracket[0] <= -20 && planted.bracket[1] >= -20, 'T8 calibrate recovers a planted rater\'s cut (-20 dB)',
  `AUC ${planted.auc}, cut ${planted.cut && planted.cut.toFixed(2)}, bracket ${planted.bracket && planted.bracket.map(v => v.toFixed(2)).join(' .. ')}`);
const coin = mulberry32(0xC014);
const noise = CAL.fitCut(prim.map(it => it.metrics.aliasDb), prim.map(() => (coin() < 0.5 ? 1 : 0)), 'above');
caught(noise.insufficient || noise.auc < 0.9, 'T8 a coin-flip rater is not "recovered"', noise.insufficient ? 'insufficient' : `AUC ${noise.auc.toFixed(2)}`);

{
  const st2 = Object.assign({}, st, { heard: { [order[0]]: 'drift', [order[1]]: 'match' } }), x2 = P.buildExport(st2, sample, CAL.SCHEMA);
  const A2 = CAL.analyse(sample, [x2]);
  ok(CAL.validateExport(x2).ok && A2.drifted.length === 1 && A2.drifted[0] === order[0], 'T8 a rating marked heard: drift is left out of the fit, and named', A2.drifted.join(', '));
}

/* T9 */
const reps = sample.items.filter(it => it.role === 'repeat');
ok(order.length === sample.items.length && new Set(order).size === order.length && sample.items.every(it => order.includes(it.key)), 'T9 the order is a permutation of every patch');
ok(reps.every(r => Math.abs(order.indexOf(r.key) - order.indexOf(r.repeatOf)) >= 8), 'T9 each repeat sits at least 8 patches from its original',
  reps.map(r => `${r.repeatOf} ${Math.abs(order.indexOf(r.key) - order.indexOf(r.repeatOf))} apart`).join(', '));
ok(P.presentationOrder(sample.items, sample.orderSeed).join() === order.join() && P.presentationOrder(sample.items, sample.orderSeed + 1).join() !== order.join(),
  'T9 the order is fixed by its seed (and moves with it)');

console.log(`\n${fails ? 'RED' : 'GREEN'} — listening pass: ${checks - fails}/${checks} checks, ${controls} must-fail controls, ${sample.items.length} patches, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
process.exit(fails ? 1 : 0);
