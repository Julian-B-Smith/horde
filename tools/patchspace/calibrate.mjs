/*
 * calibrate.mjs — B324: fit the gauntlet's coherence thresholds to the human's BLIND ratings.
 * HYPERSAW, 2026-09-28, ROADMAP B324 (records PR #819, branch lead-records-126). The human:
 * "Help me organize the listening pass, please". B316 P3 left four thresholds PROVISIONAL
 * (gauntlet.mjs THRESH: aliasing, roughness, root presence, noise); P4 fits the bounded
 * distributions only after the human's ears have set them.
 *
 *   node tools/patchspace/calibrate.mjs [ratings.json …] [--sample docs/design/listening-pass.json] [--json]
 * With no ratings files named, every *.json in local/patchspace/ratings/ (git-ignored) is read.
 * The ratings come from docs/design/listening-pass.html's EXPORT button.
 *
 * A PROPOSAL, NEVER APPLIED. This prints suggested thresholds and how well each metric agrees
 * with the ears; it writes nothing and edits nothing. THRESH changes only by the human's
 * decision recorded in ROADMAP (oracle discipline: a threshold is a gate's number).
 *
 * THE METHOD, per metric (each is paired with ONE question, and the direction is fixed):
 *   aliasDb      ↔ "aliased"       flag when the metric is ABOVE the cut
 *   roughness    ↔ "rough"         flag when ABOVE
 *   rootPresence ↔ "root unclear"  flag when BELOW
 *   flatness     ↔ "noisy"         flag when ABOVE
 *   ROC best cut: every midpoint between adjacent distinct metric values is a candidate; the
 *   cut maximising Youden's J (TPR − FPR) wins; a run of tied cuts takes its middle one, so the
 *   answer is deterministic and never sits on an edge by accident. AUC is the Mann-Whitney
 *   statistic (ties count ½): 0.5 = the metric knows nothing the ears said, 1 = it separates
 *   them perfectly. Beside it, a 1-D LOGISTIC fit (IRLS on the standardised metric, a small
 *   ridge so perfectly separated data still converges) gives the 50% point: where the ears
 *   would say yes half the time (reported only when it falls inside the rated range; a weak
 *   trend extrapolates it anywhere). With ~36 ratings both are COARSE — the cut is known to within
 *   the gap between neighbouring samples, which the report prints as the bracket.
 *   A metric with fewer than 3 flagged or 3 unflagged ratings gets NO suggestion (said so).
 *   A rating the page marked `heard: drift` (its sound measured outside tolerance of the
 *   committed numbers in the rater's browser) is left out of every fit, and named.
 * Also reported: agreement at the provisional threshold vs at the best cut; the CONTROLS
 *   (clearly clean / clearly broken patches: an inattentive pass shows up there first);
 *   test-retest agreement on the REPEATED patches; the roughness-ORIGIN question (is a swarm's
 *   own beating heard as rough, vs partials that are genuinely discordant? the sample tags
 *   each rough patch by its one-member render, `roughnessSolo`); and each metric's AUC
 *   against "not usable".
 *
 * BROWSER-IMPORTABLE on purpose: every export above the CLI is pure (no Node module), so the
 * listening page imports THIS file to validate its own export and to draw its reveal — one
 * validator and one fit, not a copy in the page. The CLI loads node:fs lazily, below.
 *
 * TWO EXPORT VERSIONS (B340, 2026-09-28, records PR #833). The human, after the v1 pass: "I
 * don't entirely trust it". v2 of the page plays a longer program (six notes over five
 * octaves, 1.5 s holds, and a pitch sweep through the engine's bend) and MEASURES every
 * segment it plays with the same metrics.mjs code; its export (SCHEMA_V2) carries those
 * per-segment numbers (`segs`) and the program's id (`phrase`).
 *   v1 exports: fitted against the COMMITTED gauntlet numbers, exactly as before (the
 *     returned object, key for key, is pinned by listening_pass_check.mjs against the
 *     pre-B340 file).
 *   v2 exports: fitted against the WORST HEARD SEGMENT per metric (worstOf: the direction
 *     that raises the flag, from PAIRS), because the ear answers for the worst thing it
 *     heard. `heard: drift` is then informational, not an exclusion: the numbers used ARE
 *     the ones measured where the rating was made. The committed-number fit is reported
 *     beside it (metricsCommitted) so the two bases can be compared, never merged.
 *   Mixing v1 and v2 exports in one analyse() call is refused with a warning per group:
 *   the CLI analyses each version separately (one person's two passes are not two raters).
 *
 * RE-MEASURE, THEN RE-FIT (B345, 2026-09-28; the human: "Let's fix the metrics first, then
 * re-fit"). metrics.mjs's aliasing was fixed and noiseDb added.
 *   node tools/patchspace/calibrate.mjs --remeasure [ratings.json …]      (~4 min for 34 patches)
 * v2 exports are re-rendered in Node through the listening page's own PURE block (loaded by
 * listening_sample.mjs loadPage, the loader listening_pass_check uses) and measured by its
 * measureWindows with the fixed metrics (remeasureExport); the answers are then fitted on the
 * worst re-measured segment, printed BEFORE (the export as heard) -> AFTER, with noiseDb fitted
 * beside flatness (PAIRS_B345) and each row's leave-one-out accuracy (looAccuracy). Chrome-vs-
 * Node drift is found on the fields whose method did not change and named; drifted ratings are
 * kept (the v2 rule) and a sensitivity fit without them follows. v1 exports fit the committed
 * numbers, which listening_sample.mjs --remeasure re-measured in place. The default mode and
 * analyse()'s default output are unchanged (listening_pass_check T8 pins v1's).
 *
 * THE ESTIMATOR RE-FIT (B351, 2026-09-29; the lead, on the human's delegation: "You can decide
 * whether we re-fit the aliasing cut").
 *   node tools/patchspace/calibrate.mjs --remeasure --conv [ratings.json …]   (~45 min for 34 patches)
 * adds B346's os-convergence estimator to every re-measured segment (aliasConvDb, aliasTotalDb:
 * five renders per segment through the page's own render route, the page's worst-window rule; see
 * convSeg below) and to the committed basis (gauntlet.mjs convLeg, the gauntlet's own E5 leg), and
 * fits the aliasing answers on all three measures (PAIRS_B351). In --remeasure the CHAOTIC patches
 * (broad#383, ratified 2026-09-29, evidence in CHAOTIC) are left out and named, and a sensitivity
 * fit with them kept follows. The engine rendered is the one the human HEARD: ADR-189's anti-
 * aliasing flags default off in docs/design/scalpel-horde-engine.js.
 */

export const SCHEMA = 'hypersaw.listening-pass.ratings/1';
export const SCHEMA_V2 = 'hypersaw.listening-pass.ratings/2';
/* the per-segment fields a v2 rating may carry (all numbers or null; `seg` names the segment) */
export const SEG_FIELDS = ['aliasDb', 'roughness', 'rootPresence', 'rootInterval', 'flatness', 'noiseDb', 'rmsDb', 'aliasAtNote'];   // noiseDb: B345
/* the five questions, flag-side keys: 1 = the "bad" answer, 0 = the "fine" answer */
export const QUESTIONS = [
  { key: 'aliased', fine: 'clean', flag: 'aliased' },
  { key: 'rough', fine: 'smooth', flag: 'rough' },
  { key: 'rootUnclear', fine: 'root clear', flag: 'root unclear' },
  { key: 'noisy', fine: 'not noisy', flag: 'noisy' },
  { key: 'unusable', fine: 'usable', flag: 'not usable' },
];
export const PAIRS = [
  { metric: 'aliasDb', question: 'aliased', when: 'above' },
  { metric: 'roughness', question: 'rough', when: 'above' },
  { metric: 'rootPresence', question: 'rootUnclear', when: 'below' },
  { metric: 'flatness', question: 'noisy', when: 'above' },
];
/* B345's re-fit asks one more pairing: noiseDb (metrics.mjs, the proposed replacement for
   flatness) against the same "noisy" answers. PAIRS itself is unchanged, so the default fits,
   v1's pinned output included, are exactly what they were. */
export const PAIRS_B345 = PAIRS.concat([{ metric: 'noiseDb', question: 'noisy', when: 'above' }]);
/* B351's re-fit asks the aliasing question of B346's os-convergence estimator too: aliasConvDb (the
   excess over the 4N/8N references) and aliasTotalDb (the excess over the oversampled truth, the
   output stage included). Beside aliasDb, never instead of it, so the three are compared on the
   same answers. */
export const PAIRS_B351 = PAIRS_B345.concat([{ metric: 'aliasConvDb', question: 'aliased', when: 'above' }, { metric: 'aliasTotalDb', question: 'aliased', when: 'above' }]);
/* CHAOTIC patches, left out of the re-fit by key (a repeat `~r` with its original), each with its
   evidence. Unlike `heard: drift` (the rater's browser measured the sound differently), these are
   patches whose sound is not a function of the patch at the precision two runtimes share: a 1-ULP
   change moves the measurement as far as the browser did, so no Node number is the sound heard.
   The human ratified the exclusion 2026-09-29 (ROADMAP B346). Used only by --remeasure; the default
   analyse() output (v1's pinned) is unchanged. */
export const CHAOTIC = {
  'broad#383': 'B346 (alias_sources.mjs ulp): 1-ULP nudges of the played frequency, fb, detune or w swing its E5 aliasDb over 4.15 dB through the page\'s own render, the size of its Chrome/Node drift (4.1 dB against TOL 3); the must-read-zero twin broad#511 moves 0.00',
};
const TOP_KEYS = ['schema', 'sample', 'orderSeed', 'complete', 'ratings'];
const RATING_KEYS = ['order', 'key', 'run', 'i', 'mode', 'answers', 'note', 'heard'];
const TOP_KEYS_V2 = TOP_KEYS.concat(['phrase']);
const RATING_KEYS_V2 = RATING_KEYS.concat(['segs']);

/* The export's contract. Timestamp-free BY SCHEMA: an unknown key is an error, so a
   wall-clock field cannot ride in unnoticed (the order index is the only sequence).
   A v2 export adds `phrase` {version 2, id} and per rating `segs` (null until measured; a
   COMPLETE v2 export must carry them on every rating, since its fit reads nothing else). */
export function validateExport(x) {
  const e = [];
  if (!x || typeof x !== 'object' || Array.isArray(x)) return { ok: false, errors: ['not an object'] };
  const v2 = x.schema === SCHEMA_V2, top = v2 ? TOP_KEYS_V2 : TOP_KEYS, rkeys = v2 ? RATING_KEYS_V2 : RATING_KEYS;
  for (const k of Object.keys(x)) if (!top.includes(k)) e.push(`unknown top-level key "${k}"`);
  if (x.schema !== SCHEMA && !v2) e.push(`schema must be "${SCHEMA}"`);
  if (v2 && !(x.phrase && x.phrase.version === 2 && typeof x.phrase.id === 'string' && /^[0-9a-f]{8}$/.test(x.phrase.id) && Object.keys(x.phrase).length === 2))
    e.push('phrase must be {version: 2, id: 8 hex digits}');
  if (!x.sample || typeof x.sample.id !== 'string' || typeof x.sample.file !== 'string') e.push('sample.{file,id} missing');
  if (!Number.isInteger(x.orderSeed)) e.push('orderSeed must be an integer');
  if (typeof x.complete !== 'boolean') e.push('complete must be a boolean');
  if (!Array.isArray(x.ratings)) { e.push('ratings must be an array'); return { ok: false, errors: e }; }
  const orders = new Set(), keys = new Set();
  x.ratings.forEach((r, n) => {
    const at = `ratings[${n}]`;
    if (!r || typeof r !== 'object') { e.push(`${at} not an object`); return; }
    for (const k of Object.keys(r)) if (!rkeys.includes(k)) e.push(`${at}: unknown key "${k}"`);
    if (v2) {
      if (r.segs === null || r.segs === undefined) { if (x.complete) e.push(`${at}.segs missing in a complete v2 export`); }
      else if (!Array.isArray(r.segs) || !r.segs.length) e.push(`${at}.segs must be a non-empty array or null`);
      else r.segs.forEach((s, j) => {
        if (!s || typeof s !== 'object' || typeof s.seg !== 'string' || !/^[A-Za-z0-9]{1,12}$/.test(s.seg)) { e.push(`${at}.segs[${j}].seg must name the segment`); return; }
        for (const k of Object.keys(s)) if (k !== 'seg' && !SEG_FIELDS.includes(k)) e.push(`${at}.segs[${j}]: unknown key "${k}"`);
        for (const k of SEG_FIELDS) if (!(s[k] === undefined || s[k] === null || Number.isFinite(s[k]))) e.push(`${at}.segs[${j}].${k} must be a finite number or null`);
      });
    }
    if (!Number.isInteger(r.order) || r.order < 0) e.push(`${at}.order must be an integer ≥ 0`);
    else if (orders.has(r.order)) e.push(`${at}.order ${r.order} repeated`); else orders.add(r.order);
    if (typeof r.key !== 'string' || !/^(broad|edge)#\d+(~r)?$/.test(r.key)) e.push(`${at}.key malformed`);
    else if (keys.has(r.key)) e.push(`${at}.key ${r.key} repeated`); else keys.add(r.key);
    if (!Number.isInteger(r.run)) e.push(`${at}.run must be an integer`);
    if (!Number.isInteger(r.i) || r.i < 0) e.push(`${at}.i must be an integer ≥ 0`);
    if (r.mode !== 'broad' && r.mode !== 'edge') e.push(`${at}.mode must be broad|edge`);
    if (typeof r.note !== 'string' || r.note.length > 2000) e.push(`${at}.note must be a string ≤ 2000 chars`);
    if (!(r.heard === undefined || r.heard === null || r.heard === 'match' || r.heard === 'drift')) e.push(`${at}.heard must be match, drift or null`);
    const a = r.answers;
    if (!a || typeof a !== 'object') { e.push(`${at}.answers missing`); return; }
    for (const k of Object.keys(a)) if (!QUESTIONS.some(q => q.key === k)) e.push(`${at}.answers: unknown key "${k}"`);
    for (const q of QUESTIONS) {
      const v = a[q.key];
      if (!(v === 0 || v === 1 || v === null)) e.push(`${at}.answers.${q.key} must be 0, 1 or null`);
      else if (v === null && x.complete) e.push(`${at}.answers.${q.key} unanswered in a complete export`);
    }
  });
  return { ok: e.length === 0, errors: e };
}

/* ROC best cut + AUC + logistic midpoint. xs: the metric; ys: 0/1 flags; when: 'above'|'below'. */
export function fitCut(xs, ys, when) {
  const s = when === 'below' ? -1 : 1;                       // score: higher => predicts the flag
  const pts = xs.map((x, k) => ({ x: s * x, y: ys[k] })).filter(p => Number.isFinite(p.x) && (p.y === 0 || p.y === 1));
  const n1 = pts.filter(p => p.y).length, n0 = pts.length - n1;
  const out = { n: pts.length, flagged: n1, unflagged: n0, when };
  if (n1 < 3 || n0 < 3) return Object.assign(out, { insufficient: true });
  pts.sort((a, b) => a.x - b.x);
  const vals = [...new Set(pts.map(p => p.x))];
  const cuts = [vals[0] - 1];
  for (let k = 0; k + 1 < vals.length; k++) cuts.push(0.5 * (vals[k] + vals[k + 1]));
  cuts.push(vals[vals.length - 1] + 1);
  let bestJ = -Infinity, tied = [];
  for (let c = 0; c < cuts.length; c++) {
    let tp = 0, fp = 0;
    for (const p of pts) if (p.x > cuts[c]) { if (p.y) tp++; else fp++; }
    const J = tp / n1 - fp / n0;
    if (J > bestJ + 1e-12) { bestJ = J; tied = [c]; } else if (Math.abs(J - bestJ) <= 1e-12) tied.push(c);
  }
  const ci = tied[(tied.length - 1) >> 1], cut = cuts[ci];
  const below = vals.filter(v => v < cut), above = vals.filter(v => v > cut);
  const bracket = [below.length ? below[below.length - 1] : null, above.length ? above[0] : null];
  let auc = 0;
  for (const p of pts) if (p.y) for (const q of pts) if (!q.y) auc += p.x > q.x ? 1 : p.x === q.x ? 0.5 : 0;
  auc /= n1 * n0;
  const back = v => (v === null ? null : s * v);
  return Object.assign(out, {
    cut: s * cut, J: bestJ, auc, accuracyAtCut: accuracy(pts, cut),
    bracket: s > 0 ? bracket.map(back) : bracket.map(back).reverse(),
    logistic50: (l => (l ? { at: s * l.score, slope: l.slope } : null))(logistic50(pts)),   // back in metric units
  });
}
/* LEAVE-ONE-OUT accuracy of the ROC-cut rule (B345): each rating is predicted by the cut fitted
   on the others. With ~34 ratings the in-sample agreement at the best cut is optimistic by
   construction; this is the honest one. null when too few ratings remain on one side. */
export function looAccuracy(xs, ys, when) {
  const idx = xs.map((_, k) => k).filter(k => Number.isFinite(xs[k]) && (ys[k] === 0 || ys[k] === 1));
  let hit = 0, n = 0;
  for (const k of idx) {
    const rest = idx.filter(j => j !== k), f = fitCut(rest.map(j => xs[j]), rest.map(j => ys[j]), when);
    if (f.insufficient) continue;
    n++; if (((when === 'below' ? xs[k] < f.cut : xs[k] > f.cut) ? 1 : 0) === ys[k]) hit++;
  }
  return n ? hit / n : null;
}
function accuracy(pts, cut) { let a = 0; for (const p of pts) if ((p.x > cut ? 1 : 0) === p.y) a++; return a / pts.length; }
/* agreement of the provisional threshold, in the metric's own units and direction */
export function agreementAt(xs, ys, when, thr) {
  let a = 0, n = 0;
  xs.forEach((x, k) => { if (!Number.isFinite(x) || !(ys[k] === 0 || ys[k] === 1)) return; n++; if (((when === 'below' ? x < thr : x > thr) ? 1 : 0) === ys[k]) a++; });
  return n ? a / n : null;
}
/* 1-D logistic regression by IRLS (Newton) on the standardised score; ridge 1e-2 on the slope
   keeps a perfectly separated set finite. Returns the score's 50% point in METRIC units. */
function logistic50(pts) {
  const n = pts.length, mu = pts.reduce((a, p) => a + p.x, 0) / n;
  const sd = Math.sqrt(pts.reduce((a, p) => a + (p.x - mu) ** 2, 0) / n) || 1;
  const z = pts.map(p => (p.x - mu) / sd);
  let a = 0, b = 0;
  for (let it = 0; it < 50; it++) {
    let ga = 0, gb = -1e-2 * b, haa = 0, hab = 0, hbb = 1e-2;
    for (let k = 0; k < n; k++) {
      const p = 1 / (1 + Math.exp(-(a + b * z[k]))), w = Math.max(p * (1 - p), 1e-9), r = pts[k].y - p;
      ga += r; gb += r * z[k]; haa += w; hab += w * z[k]; hbb += w * z[k] * z[k];
    }
    const det = haa * hbb - hab * hab; if (!(det > 0)) break;
    const da = (hbb * ga - hab * gb) / det, db = (haa * gb - hab * ga) / det;
    a += da; b += db;
    if (Math.abs(da) + Math.abs(db) < 1e-10) break;
  }
  if (!(b > 1e-9)) return null;                             // no increasing trend: no midpoint
  /* a weak trend puts the 50% point far outside the rated range (a random rater read 62 on a
     0..0.7 roughness scale): an extrapolation, not a threshold, so it is not reported */
  const at = mu + sd * (-a / b);
  if (at < pts[0].x || at > pts[n - 1].x) return null;       // pts is sorted by score here
  return { score: at, slope: b / sd };
}

/* The worst a v2 rating HEARD, per metric: over its measured segments, the value on the side
   that raises the flag (PAIRS.when: above -> the largest, below -> the smallest); a null
   (root on the sweep, where no single pitch is held) is skipped. `from` names the segment. */
export function worstOf(segs, pairs) {
  const values = {}, from = {};
  for (const pr of pairs || PAIRS) {
    let best = null, at = null;
    for (const s of segs || []) {
      const v = s[pr.metric];
      if (v === null || v === undefined || !Number.isFinite(v)) continue;
      if (best === null || (pr.when === 'above' ? v > best : v < best)) { best = v; at = s.seg; }
    }
    values[pr.metric] = best; from[pr.metric] = at;
  }
  return { values, from };
}

/* The whole analysis: `sample` is docs/design/listening-pass.json, `exports` validated exports.
   Primary observations are each item's own key; a repeat (`~r`) is used for test-retest only.
   All exports must be ONE version (see the header): v1 fits the committed numbers and returns
   exactly the pre-B340 object; v2 fits the worst heard segment and adds its own fields. */
export function analyse(sample, exports, opt) {
  opt = opt || {};
  const pairs = opt.pairs || PAIRS;
  const nV2 = exports.filter(x => x.schema === SCHEMA_V2).length;
  if (nV2 && nV2 < exports.length) throw new Error('analyse: v1 and v2 exports cannot be fitted together (different bases); analyse each version separately');
  const v2 = nV2 > 0;
  const byKey = Object.fromEntries(sample.items.map(it => [it.key, it]));
  const warnings = [], obs = [], repeats = [], drifted = [], unmeasured = [], phrases = new Set(), chaotic = [];
  for (const x of exports) {
    if (x.sample.id !== sample.id) warnings.push((sample.remeasured || []).some(r => r.from === x.sample.id)
      ? `an export was made against sample ${x.sample.id}, since re-measured in place as ${sample.id} (the same patches and sounds; only the committed numbers changed): its ratings are matched by key`
      : `an export was made against sample ${x.sample.id}, this sample is ${sample.id}: its ratings are matched by key, but the sounds may differ`);
    if (v2) phrases.add(x.phrase.id);
    for (const r of x.ratings) {
      const it = byKey[r.key];
      if (!it) { warnings.push(`rating for unknown key ${r.key} skipped`); continue; }
      /* opt.exclude (B351): { key: why } — CHAOTIC above; a repeat goes with its original */
      if (opt.exclude && opt.exclude[r.key.replace(/~r$/, '')]) { chaotic.push(r.key); continue; }
      if (!v2) {
        if (r.heard === 'drift') { drifted.push(r.key); continue; }
        (it.role === 'repeat' ? repeats : obs).push({ it, r, m: it.metrics });
        continue;
      }
      if (r.heard === 'drift') drifted.push(r.key);       // KEPT in v2: the numbers fitted are the ones heard
      if (!Array.isArray(r.segs) || !r.segs.length) { unmeasured.push(r.key); continue; }
      const w = worstOf(r.segs, pairs);
      (it.role === 'repeat' ? repeats : obs).push({ it, r, m: w.values, from: w.from });
    }
  }
  if (chaotic.length) warnings.push(`${chaotic.length} rating(s) left out as CHAOTIC: ` + [...new Set(chaotic.map(k => k.replace(/~r$/, '')))].map(k => `${k} (${opt.exclude[k]})`).join('; '));
  const T = sample.thresholds;
  const fitOn = pick => pairs.map(pr => {
    const xs = obs.map(o => pick(o)[pr.metric]), ys = obs.map(o => o.r.answers[pr.question]);
    const f = fitCut(xs, ys, pr.when);
    /* a metric with no provisional threshold (B345's noiseDb) reports none, not an agreement with `undefined` */
    const thr = T[pr.metric] === undefined ? null : T[pr.metric];
    const row = Object.assign({ metric: pr.metric, question: pr.question, provisional: thr, agreementProvisional: thr === null ? null : agreementAt(xs, ys, pr.when, thr) }, f);
    if (opt.loo) row.loo = looAccuracy(xs, ys, pr.when);
    return row;
  });
  const metrics = fitOn(o => o.m);
  const usable = pairs.map(pr => {
    const f = fitCut(obs.map(o => o.m[pr.metric]), obs.map(o => o.r.answers.unusable), pr.when);
    return { metric: pr.metric, auc: f.insufficient ? null : f.auc, n: f.n };
  });
  const controls = obs.filter(o => /^control/.test(o.it.role)).map(o => {
    const flags = QUESTIONS.filter(q => o.r.answers[q.key] === 1).map(q => q.key);
    const pass = o.it.role === 'control-clean' ? flags.length === 0 : o.r.answers.unusable === 1;
    return { key: o.it.key, role: o.it.role, flags, pass };
  });
  const retest = repeats.map(rp => {
    const orig = obs.find(o => o.it.key === rp.it.repeatOf);
    if (!orig) return null;
    const same = QUESTIONS.filter(q => rp.r.answers[q.key] !== null && rp.r.answers[q.key] === orig.r.answers[q.key]).length;
    return { key: rp.it.repeatOf, agree: same, of: QUESTIONS.length };
  }).filter(Boolean);
  const origin = {};
  for (const o of obs) {
    const g = o.it.roughOrigin; if (!g) continue;
    const s = origin[g] || (origin[g] = { n: 0, rough: 0 });
    if (o.r.answers.rough === 0 || o.r.answers.rough === 1) { s.n++; s.rough += o.r.answers.rough; }
  }
  if (!v2) {
    if (drifted.length) warnings.push(`${drifted.length} rating(s) left out: the page measured their sound outside tolerance of the committed numbers (heard: drift): ${drifted.join(', ')}`);
    return { sampleId: sample.id, observations: obs.length, drifted, warnings, metrics, usable, controls, retest, roughOrigin: origin };
  }
  if (phrases.size > 1) warnings.push(`the exports were made on ${phrases.size} different programs (${[...phrases].join(', ')}): their segments differ`);
  if (drifted.length) warnings.push(`${drifted.length} rating(s) measured differently in the rater's browser than in the gauntlet (heard: drift); KEPT, fitted on the numbers heard: ${drifted.join(', ')}`);
  if (unmeasured.length) warnings.push(`${unmeasured.length} rating(s) left out: no heard segments (not measured when exported): ${unmeasured.join(', ')}`);
  const worstFrom = {};
  for (const pr of pairs) { const c = worstFrom[pr.metric] = {}; for (const o of obs) { const s = o.from[pr.metric]; if (s) c[s] = (c[s] || 0) + 1; } }
  return { sampleId: sample.id, observations: obs.length, drifted, warnings, metrics, usable, controls, retest, roughOrigin: origin,
    version: 2, basis: 'the worst heard segment per metric', phrase: [...phrases], unmeasured, worstFrom, metricsCommitted: fitOn(o => o.it.metrics) };
}

/* RE-MEASURE WITHOUT RE-LISTENING (B345; the human: "Let's fix the metrics first, then re-fit").
   A v2 rating is keyed by its patch, and the patch's program renders deterministically from its
   seed, so after a metrics.mjs fix the answers can be fitted on the FIXED metrics' numbers of the
   same sounds. `segsOf(rating)` returns the segments measured now (the CLI renders the page's own
   program in Node through its PURE block and measures it with measureWindows; see below); `diff`
   is the page's tolerance test (heardDiff). The rater's browser measured the sound with the
   metrics of the day, so the comparison that can reveal Chrome-vs-Node DRIFT uses only the fields
   whose method B345 did not change (DRIFT_FIELDS): a rating whose sound measures differently in
   Node on any of them is marked `heard: drift` (informational in v2, as before) and named. */
export const DRIFT_FIELDS = ['roughness', 'rootPresence', 'flatness', 'rmsDb'];
export function remeasureExport(x, segsOf, diff) {
  const drift = {};
  const pick = o => Object.fromEntries(DRIFT_FIELDS.map(k => [k, o[k]]));
  const ratings = x.ratings.map(r => {
    const now = segsOf(r), over = [];
    for (const old of r.segs || []) {
      const nw = now.find(s => s.seg === old.seg);
      if (nw) for (const o of diff(pick(nw), pick(old)).over) over.push(`${old.seg}.${o.k}`);
    }
    if (over.length) drift[r.key] = over;
    return Object.assign({}, r, { segs: now, heard: over.length ? 'drift' : 'match' });
  });
  return { export: Object.assign({}, x, { ratings }), drift };
}

/* ---------------------------------------------------------------- the CLI (Node only) */
const fmt = (v, d) => (v === null || v === undefined ? '—' : typeof v === 'number' ? v.toFixed(d === undefined ? 3 : d) : String(v));
export function textReport(A) {
  const L = [];
  L.push(`calibrate — B324 listening pass, sample ${A.sampleId}, ${A.observations} rated patches (repeats excluded)`);
  L.push('A PROPOSAL: nothing is applied. THRESH (tools/patchspace/gauntlet.mjs) changes only by a human decision recorded in ROADMAP.');
  if (A.version === 2) L.push(`v2 export(s), program ${A.phrase.join(', ')}: fitted against ${A.basis} (the committed-number fit follows, for comparison only)`);
  for (const w of A.warnings) L.push('WARNING: ' + w);
  L.push('');
  const table = ms => {
    L.push('metric        question      flag when  n   yes  AUC    provisional (agree)   suggested cut (agree)   bracket                logistic 50%');
    for (const m of ms) {
      const d = /Db$/.test(m.metric) ? 1 : 3;                   // dB metrics (aliasDb, noiseDb, B351's aliasConvDb/aliasTotalDb) to 0.1
      const row = [m.metric.padEnd(13), m.question.padEnd(13), m.when.padEnd(10), String(m.n).padEnd(3), String(m.flagged).padEnd(4)];
      if (m.insufficient) row.push(`—      ${fmt(m.provisional, d)} (${fmt(m.agreementProvisional, 2)})`.padEnd(29), 'no suggestion: fewer than 3 answers on one side');
      else row.push(fmt(m.auc, 2).padEnd(6), `${fmt(m.provisional, d)} (${fmt(m.agreementProvisional, 2)})`.padEnd(21),
        `${fmt(m.cut, d)} (${fmt(m.accuracyAtCut, 2)})`.padEnd(23), `${fmt(m.bracket[0], d)} .. ${fmt(m.bracket[1], d)}`.padEnd(22),
        m.logistic50 ? fmt(m.logistic50.at, d) : '— (no trend)');
      L.push(row.join(' '));
    }
  };
  table(A.metrics);
  if (A.version === 2) {
    L.push('');
    L.push('where the worst heard value came from: ' + Object.entries(A.worstFrom).map(([m, c]) => `${m} ${Object.entries(c).map(([s, n]) => `${s}×${n}`).join(' ') || '—'}`).join(' · '));
    L.push('');
    L.push('COMPARISON ONLY — the same answers against the committed gauntlet numbers (the A3/E5 windows the gauntlet measured):');
    table(A.metricsCommitted);
  }
  L.push('');
  L.push('AUC against "not usable": ' + A.usable.map(u => `${u.metric} ${fmt(u.auc, 2)}`).join(' · '));
  L.push('controls: ' + (A.controls.length ? A.controls.map(c => `${c.key} ${c.role} ${c.pass ? 'as expected' : 'NOT as expected (' + (c.flags.join(',') || 'no flags') + ')'}`).join(' · ') : 'none rated'));
  L.push('test-retest: ' + (A.retest.length ? A.retest.map(r => `${r.key} ${r.agree}/${r.of}`).join(' · ') : 'no repeat rated'));
  L.push('roughness origin (share answered "rough"): ' + (Object.keys(A.roughOrigin).length ? Object.entries(A.roughOrigin).map(([g, s]) => `${g} ${s.rough}/${s.n}`).join(' · ') : 'none rated'));
  return L.join('\n');
}

const isCli = typeof process !== 'undefined' && process.argv && process.argv[1] && process.argv[1].endsWith('calibrate.mjs');
if (isCli) {
  const { readFileSync, readdirSync, existsSync } = await import('node:fs');
  const { join, dirname } = await import('node:path');
  const { fileURLToPath } = await import('node:url');
  const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
  const argv = process.argv.slice(2), flag = n => argv.indexOf('--' + n);
  const si = flag('sample'), samplePath = si >= 0 ? argv[si + 1] : join(ROOT, 'docs/design/listening-pass.json');
  let files = argv.filter((a, k) => !a.startsWith('--') && !(si >= 0 && k === si + 1));
  if (!files.length) {
    const d = join(ROOT, 'local/patchspace/ratings');
    files = existsSync(d) ? readdirSync(d).filter(f => f.endsWith('.json')).sort().map(f => join(d, f)) : [];
  }
  if (!files.length) { console.error('calibrate: no ratings — export them from docs/design/listening-pass.html into local/patchspace/ratings/'); process.exit(2); }
  const sample = JSON.parse(readFileSync(samplePath, 'utf8'));
  const exps = files.map(f => {
    const x = JSON.parse(readFileSync(f, 'utf8')), v = validateExport(x);
    if (!v.ok) { console.error(`calibrate: ${f} fails the export schema:\n  ` + v.errors.join('\n  ')); process.exit(1); }
    return x;
  });
  /* one analysis per export version: a v1 and a v2 pass by the same person are two hearings
     on two bases, never pooled (analyse() refuses a mix) */
  const groups = [exps.filter(x => x.schema !== SCHEMA_V2), exps.filter(x => x.schema === SCHEMA_V2)].filter(g => g.length);
  if (flag('remeasure') < 0) {
    const out = groups.map(g => analyse(sample, g));
    if (flag('json') >= 0) console.log(JSON.stringify(out.length === 1 ? out[0] : out, null, 1));
    else console.log(out.map(textReport).join('\n\n' + '-'.repeat(100) + '\n\n'));
  } else {
    /* --remeasure (B345): v1 exports are fitted on the committed numbers, which listening_sample.mjs
       --remeasure has re-measured in place with the fixed metrics (so they need no render); v2
       exports are re-rendered and re-measured here, patch by patch (~5 s each), and fitted on the
       worst re-measured segment. BEFORE = the export as it was (the metrics of the day, measured in
       the rater's browser); AFTER = the fixed metrics, in Node. noiseDb is fitted beside flatness,
       and every row carries its leave-one-out accuracy. Writes nothing. */
    const { loadPage, RENDER_SALT } = await import('./listening_sample.mjs');
    const { loadEngine } = await import('./space.mjs');
    const { samplePatch } = await import('./gauntlet.mjs');
    const { hash32 } = await import('./gen_dependency_tree.mjs');
    const MET = await import('./metrics.mjs');
    const P = loadPage(), { Composed } = loadEngine(), cache = new Map(), t0 = Date.now();
    const conv = flag('conv') >= 0, AS = conv ? await import('./alias_sources.mjs') : null, G = conv ? await import('./gauntlet.mjs') : null;
    const r6 = x => (typeof x === 'number' && Number.isFinite(x) ? +x.toPrecision(6) : null);
    /* --conv (B351): B346's estimator on every segment, the page's window rule (every aliasing window,
       the WORST one): the segment's held part rendered once at os N, 2N, 4N, 8N and 16N through the
       page's own render (P.renderScript, so the sweep's bend is the page's), by the estimator's engine
       (alias_sources.mjs baseEngine: the composed engine with a read-only capture of the internal
       stream), then alias_sources.mjs estimate() per window: aliasConvDb (metrics.mjs aliasConvergence's
       excess) and aliasTotalDb (the excess over the 8N/16N truth). The truth is decimated once, for the
       LAST window (the largest end), and handed to every earlier one (estimate's truthOf). */
    const convSeg = (patch, pseed, seg) => {
      const N = patch.os || 2, memo = {}, Cc = AS.baseEngine(), n = seg.held;
      const renderAt = (m, cap) => {
        if (memo[m]) return memo[m];
        let core = null;
        const K = class extends Cc {
          constructor(sr) { super(sr); core = this; }
          bqf(f, x) {                                  // allocate the capture once os is known (set() runs after construction)
            if (cap && !this._cap) { const L = n * this.os; this._cap = { n: L, i: 0, preL: new Float64Array(L), preR: new Float64Array(L), postL: new Float64Array(L), postR: new Float64Array(L) }; }
            return super.bqf(f, x);
          }
        };
        const r = P.renderScript(K, Object.assign({}, patch, { os: N * m }), P.segScript(seg, false), { seed: pseed, block: seg.block });
        return (memo[m] = { L: r.L, R: r.R, cap: core._cap || null, core });
      };
      const W = P.MEAS.alias.n - P.MEAS.alias.from, starts = P.winStarts(P.MEAS.alias.from, W, n).reverse();
      let TT = null, c = null, t = null, b = null;
      for (const a of starts) {
        const E = AS.estimate(renderAt, N, [a, a + W], { truth: true, output: false, truthOf: TT || undefined });
        if (!TT) TT = E._T;
        if (c === null || E.conv.excessDb > c) c = E.conv.excessDb;
        if (t === null || E.totalDb > t) t = E.totalDb;
        if (b === null || E.conv.b345Db > b) b = E.conv.b345Db;
      }
      return { aliasConvDb: r6(c), aliasTotalDb: r6(t), b345: r6(b) };
    };
    const segsOf = r => {
      const base = r.key.replace(/~r$/, '');
      if (cache.has(base)) return cache.get(base);
      const patch = samplePatch(r.run, r.i, r.mode).patch, pseed = hash32(r.run, r.i, RENDER_SALT);
      const segs = P.SEGS.map(seg => P.runSync(P.measureSegmentSteps(MET, Composed, patch, pseed, seg, P.runSync(P.renderSegmentSteps(Composed, patch, pseed, seg)))));
      if (conv) segs.forEach((g, k) => {
        const e = convSeg(patch, pseed, P.SEGS[k]);
        /* the estimator's renders at N and 4N ARE the page's test and reference: its B345 reading must be the page's, or the two renders differ */
        if (e.b345 !== g.aliasDb) throw new Error(`${base} ${g.seg}: the estimator's renders read aliasDb ${e.b345}, the page's ${g.aliasDb}: not the same sound`);
        g.aliasConvDb = e.aliasConvDb; g.aliasTotalDb = e.aliasTotalDb;
      });
      cache.set(base, segs);
      console.error(`  re-measured ${base} (${cache.size}, ${((Date.now() - t0) / 1000).toFixed(0)} s)`);
      return segs;
    };
    /* the re-fit leaves CHAOTIC patches out (B351; ratified in B346); a sensitivity fit with them kept follows */
    const opt = { pairs: conv ? PAIRS_B351 : PAIRS_B345, loo: true, exclude: CHAOTIC }, optKept = Object.assign({}, opt, { exclude: null }), L = [];
    /* the committed basis (v1's fit, and v2's comparison fit) carries no estimator numbers: with --conv each item
       gets gauntlet.mjs convLeg (measure()'s own B346 leg, E5 0.3 s) in memory; the sample file is not written */
    let fitSample = sample;
    if (conv) {
      const legs = new Map();
      fitSample = Object.assign({}, sample, { items: sample.items.map(it => {
        const base = it.key.replace(/~r$/, '');
        if (!legs.has(base)) legs.set(base, G.convLeg(samplePatch(sample.run.seed, it.i, it.mode).patch, hash32(sample.run.seed, it.i, RENDER_SALT)));
        const g = legs.get(base);
        return Object.assign({}, it, { metrics: Object.assign({}, it.metrics, { aliasConvDb: g.aliasConvDb, aliasTotalDb: g.aliasTotalDb }) });
      }) });
      console.error(`  committed-basis estimator on ${legs.size} patches (${((Date.now() - t0) / 1000).toFixed(0)} s)`);
    }
    const cmp = (B, A) => {
      L.push('metric        question      AUC before -> after   cut before -> after       agree@cut before -> after   LOO before -> after   provisional agree before -> after');
      for (const a of A.metrics) {
        const b = B.metrics.find(m => m.metric === a.metric), d = /Db$/.test(a.metric) ? 1 : 3;
        const two = (f, dd) => `${b ? fmt(f(b), dd) : '—'} -> ${fmt(f(a), dd)}`;
        L.push([a.metric.padEnd(13), a.question.padEnd(13), two(m => (m.insufficient ? null : m.auc), 2).padEnd(21), two(m => (m.insufficient ? null : m.cut), d).padEnd(25),
          two(m => (m.insufficient ? null : m.accuracyAtCut), 2).padEnd(27), two(m => m.loo, 2).padEnd(21), two(m => m.agreementProvisional, 2)].join(' '));
      }
      L.push('AUC against "not usable", before -> after: ' + A.usable.map(u => { const b = B.usable.find(x => x.metric === u.metric); return `${u.metric} ${b ? fmt(b.auc, 2) : '—'} -> ${fmt(u.auc, 2)}`; }).join(' · '));
    };
    for (const g of groups) {
      if (g[0].schema !== SCHEMA_V2) {
        L.push('v1 export(s): fitted on the committed numbers, re-measured in place with the fixed metrics (listening_sample.mjs --remeasure). For BEFORE, run the default mode with --sample on the earlier file (its id is in the sample file under remeasured).');
        L.push(textReport(analyse(fitSample, g, opt)), '', 'SENSITIVITY — the same v1 fit with the CHAOTIC patches KEPT:', textReport(analyse(fitSample, g, optKept)), '', '-'.repeat(100), '');
        continue;
      }
      const before = analyse(sample, g, { loo: true }), redone = g.map(x => remeasureExport(x, segsOf, P.heardDiff));
      const after = analyse(fitSample, redone.map(r => r.export), opt), drift = Object.assign({}, ...redone.map(r => r.drift));
      L.push(`v2 export(s) RE-MEASURED in Node with the fixed metrics (${cache.size} patches, ${((Date.now() - t0) / 1000).toFixed(0)} s). BEFORE = the export's own numbers (the rater's browser, the metrics of the day); AFTER = the fixed metrics.`);
      cmp(before, after);
      L.push('', `CHROME-vs-NODE DRIFT (${Object.keys(drift).length} rating(s): a field whose method did not change measures outside the page's tolerance in Node; KEPT, as v2 keeps drift): ` +
        (Object.entries(drift).map(([k, f]) => `${k} [${f.join(', ')}]`).join(' · ') || 'none'));
      if (Object.keys(drift).length) {
        const kept = redone.map(r => Object.assign({}, r.export, { ratings: r.export.ratings.filter(x => !drift[x.key]) }));
        L.push('', 'SENSITIVITY — the same fit with the drifted ratings LEFT OUT (the v1 rule):');
        cmp(before, analyse(fitSample, kept, opt));
      }
      L.push('', 'SENSITIVITY — the same fit with the CHAOTIC patches KEPT:');
      cmp(before, analyse(fitSample, redone.map(r => r.export), optKept));
      L.push('', 'AFTER, in full:', textReport(after), '', '-'.repeat(100), '');
    }
    console.log(L.join('\n'));
  }
}
