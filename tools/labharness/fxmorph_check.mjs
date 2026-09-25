/*
 * fxmorph_check.mjs — property-based fuzzer for the FX-chain morph (B266).
 * WIRED: ./verify full
 *
 * WHAT IT TESTS. docs/design/fx-chain-morph-lab.html races nine ways to morph
 * an FX chain between two patches (B265's A-D, B266's E, F, G, H and the GC
 * hybrid) against seven invariants — I7 in two readings, I3 in two. The lab
 * checks five hand-made pairs. This checks thousands of SEEDED random ones,
 * across temperatures and seeds, and SHRINKS every failure to a minimal
 * counterexample, so the design sees cases, not percentages.
 *
 * WHICH PARADIGMS ARE ASSERTED AND WHICH ARE ONLY REPORTED — stated plainly:
 *   ASSERTED  E, F, G, H, GC. Each claims a list of invariants BY CONSTRUCTION
 *             (the lab's CLAIMS table). Whenever one of them hosts a pair, every
 *             claimed invariant must hold at every x, or this exits non-zero.
 *             Whether it CAN host a pair (coverage) is reported, never failed:
 *             "cannot host" is an honest answer, a broken claim is a bug.
 *   REPORTED  A, B, C, D — the paradigms under study (D is the control). Their
 *             failure rates per invariant are printed and never fail the gate.
 *   ASSERTED  too: the lab's own self-check (every claim holds, every planted
 *             fault is caught — L0032's two halves), and the committed gallery:
 *             every counterexample in docs/design/fx-chain-morph-gallery.json
 *             must still fail exactly as recorded, so the lab never shows a case
 *             the current law no longer produces.
 *
 * IT EXTRACTS, IT DOES NOT REIMPLEMENT. The lab's first <script> block is the
 * pure model (no DOM, no clock, mulberry32 only); it is sliced out and run in
 * THIS context with `new Function` — the modlab_* idiom, and not node:vm,
 * whose globals cost ~56x (L0052). One law, not two. Only the random endpoint
 * generator and the shrinker live here, because the lab never needs them.
 *
 * MODES
 *   node tools/labharness/fxmorph_check.mjs
 *        the gate: self-check, the five lab pairs plus GATE_PAIRS random pairs
 *        x four temperatures, gallery replay. Prints the rates. ~20-40 s.
 *   node tools/labharness/fxmorph_check.mjs --sweep --pairs 2000 --chunk 3/8 --out DIR
 *        one chunk of the big sweep (run chunks in parallel; each prints
 *        progress every 50 pairs, so no watchdog sees silence).
 *   node tools/labharness/fxmorph_check.mjs --merge DIR [--write]
 *        merge the chunks, shrink one witness per (paradigm, failure), print the
 *        tables, and with --write replace the committed gallery.
 *
 * Provenance: ROADMAP B265 (I1-I7, the pared world) and B266 (this fuzzer);
 * traces/2026-09-25-b266-fxmorph-fuzz.md.
 */
import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '../..');
/* --lab PATH runs the gate against a MUTATED COPY of the lab (scratch only) —
   how the gate's own must-fail was calibrated: a copy whose planner ignores I7
   must turn it red (see the B266 trace). The tracked lab is never written. */
const labArg = process.argv.indexOf('--lab');
const LAB = labArg > 0 ? process.argv[labArg + 1] : join(ROOT, 'docs/design/fx-chain-morph-lab.html');
const GALLERY = join(ROOT, 'docs/design/fx-chain-morph-gallery.json');

// ---------------------------------------------------------------- the core --
const html = readFileSync(LAB, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
if (!blocks.length) throw new Error('fxmorph_check: no <script> block in ' + LAB);
const core = new Function(blocks[0][1] + `
;return { build, evaluate, runSelfCheck, claimsOf, mulberry32, fnv, kendall, PAIRS, ALGS, TYPES, LANES, APPROVED,
          APPROVED_FULL, NGRID, W, SHORT, NAME, I6_FLOOR, I7_FLOOR, TAIL_X };`)();
const { build, evaluate, runSelfCheck, claimsOf, mulberry32, fnv, kendall, PAIRS, ALGS, TYPES, LANES, APPROVED, SHORT } = core;

const ALG_IDS = ALGS.map(a => a.id);
const ASSERTED = ALGS.filter(a => a.planned).map(a => a.id);   // E F G H GC
const STUDIED = ALG_IDS.filter(a => !ASSERTED.includes(a));      // A B C D
const KEYS = ['I1', 'I2', 'I3', 'I3p', 'I4', 'I5', 'I6', 'I7', 'I7t'];
const LABEL = { I1: 'I1', I2: 'I2', I3: 'I3 edge', I3p: 'I3 path', I4: 'I4', I5: 'I5', I6: 'I6', I7: 'I7 STRICT', I7t: 'I7 TAIL' };
const TEMPS = [0, 0.3, 0.7, 1];
const GATE_PAIRS = 40, GATE_SEED = 266;

// ------------------------------------------------------- random endpoints --
/* A patch in the lab's world: an ordered subset of the four types, the modules
   (or OUT) each lane enters, per-module parameters, at most one return edge.
   VALID AT ITS END by construction: every lane enters something (so reaches OUT
   down the serial chain), the chain's first module is entered by a lane (I7:
   no module sits upstream of every input), and a return lands on a Delay or
   Reverb that comes BEFORE its source, gain <= 0.5 (feedback rules F1-F3). */
const PARAMS = {
  DRV: r => ({ drive: +(1 + 9 * r()).toFixed(2) }),
  FLT: r => ({ cut: Math.round(200 * Math.pow(40, r())), res: +(0.1 + 0.8 * r()).toFixed(2) }),
  DLY: r => ({ time: +(0.08 + 0.32 * r()).toFixed(3), fbk: +(0.1 + 0.4 * r()).toFixed(2), mix: +(0.2 + 0.3 * r()).toFixed(2) }),
  REV: r => ({ size: +(0.3 + 0.6 * r()).toFixed(2), mix: +(0.15 + 0.45 * r()).toFixed(2) }),
};
const DEFAULT_P = { DRV: { drive: 4 }, FLT: { cut: 1500, res: 0.4 }, DLY: { time: 0.2, fbk: 0.3, mix: 0.3 }, REV: { size: 0.5, mix: 0.3 } };
const clone = o => JSON.parse(JSON.stringify(o));
const pick = (r, a) => a[Math.floor(r() * a.length)];
function shuffle(r, a) { a = a.slice(); for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); const t = a[i]; a[i] = a[j]; a[j] = t; } return a; }

function repair(P, r) {
  const ok = new Set(P.order.concat(['OUT']));
  for (const l of LANES) {
    P.lanes[l] = [...new Set((P.lanes[l] || []).filter(m => ok.has(m)))];
    if (!P.lanes[l].length) P.lanes[l] = ['OUT'];
    if (P.lanes[l].length > 1) P.lanes[l] = P.lanes[l].filter(m => m !== 'OUT');   // OUT plus a module is two paths to OUT; keep the patch simple
  }
  if (P.order.length && !LANES.some(l => P.lanes[l].includes(P.order[0]))) {
    const l = r ? pick(r, LANES) : 'BODY';
    P.lanes[l] = P.lanes[l][0] === 'OUT' ? [P.order[0]] : P.lanes[l].concat([P.order[0]]);
  }
  if (P.fb) {
    const s = P.order.indexOf(P.fb.src), d = P.order.indexOf(P.fb.dst);
    if (s < 0 || d < 0 || d >= s || !(P.fb.dst === 'DLY' || P.fb.dst === 'REV') || !(P.fb.g > 0 && P.fb.g <= 0.5)) P.fb = null;
  }
  const p = {}; for (const m of P.order) p[m] = (P.p && P.p[m]) || clone(DEFAULT_P[m]); P.p = p;
  return P;
}
function genPatch(r) {
  const n = pick(r, [0, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4]);
  const order = shuffle(r, TYPES).slice(0, n);
  const lanes = {};
  for (const l of LANES) {
    const opts = order.concat(['OUT']);
    lanes[l] = r() < 0.75 ? [pick(r, opts)] : shuffle(r, order).slice(0, 2);
  }
  let fb = null;
  if (r() < 0.35) {
    const c = [];
    order.forEach((d, i) => { if (d === 'DLY' || d === 'REV') for (let j = i + 1; j < order.length; j++) c.push({ src: order[j], dst: d }); });
    if (c.length) { const q = pick(r, c); fb = { src: q.src, dst: q.dst, g: +(0.1 + 0.4 * r()).toFixed(2) }; }
  }
  const p = {}; for (const m of order) p[m] = PARAMS[m](r);
  return repair({ order, lanes, fb, p }, r);
}
/* Half the pairs are independent draws (mostly order CONFLICTS — the hard
   case), half are a patch and a local edit of it (one to three moves: swap,
   insert, remove, re-route a lane, toggle the return), which is what a patch
   author's A/B actually looks like and what E and G can host. */
function genPair(r) {
  const L = genPatch(r);
  if (r() < 0.5) return { L, R: genPatch(r), kind: 'independent' };
  const R = clone(L), k = 1 + Math.floor(r() * 3);
  for (let i = 0; i < k; i++) {
    const e = Math.floor(r() * 6);
    if (e === 0 && R.order.length >= 2) { const a = Math.floor(r() * (R.order.length - 1)); const t = R.order[a]; R.order[a] = R.order[a + 1]; R.order[a + 1] = t; }
    else if (e === 1 && R.order.length < 4) { const m = pick(r, TYPES.filter(t => !R.order.includes(t))); R.order.splice(Math.floor(r() * (R.order.length + 1)), 0, m); R.p[m] = PARAMS[m](r); }
    else if (e === 2 && R.order.length) { R.order.splice(Math.floor(r() * R.order.length), 1); }
    else if (e === 3) { const l = pick(r, LANES); R.lanes[l] = [pick(r, R.order.concat(['OUT']))]; }
    else if (e === 4) { R.fb = null; if (r() < 0.6) { const c = []; R.order.forEach((d, i) => { if (d === 'DLY' || d === 'REV') for (let j = i + 1; j < R.order.length; j++) c.push({ src: R.order[j], dst: d }); }); if (c.length) { const q = pick(r, c); R.fb = { src: q.src, dst: q.dst, g: +(0.1 + 0.4 * r()).toFixed(2) }; } } }
    else { for (const m of R.order) R.p[m] = PARAMS[m](r); }
    repair(R, r);
  }
  return { L, R, kind: 'edit' };
}
const pairOf = c => ({ id: 'fuzz', name: 'fuzz', L: c.L, R: c.R });
const size = c => {
  const ends = [c.L, c.R];
  const common = c.L.order.filter(t => c.R.order.includes(t));
  return ends.reduce((a, P) => a + P.order.length + LANES.reduce((q, l) => q + P.lanes[l].filter(m => m !== 'OUT').length, 0) + (P.fb ? 1 : 0), 0)
       + kendall(common, c.R.order.filter(t => common.includes(t)))
       + (ends.some(P => P.order.some(m => JSON.stringify(P.p[m]) !== JSON.stringify(DEFAULT_P[m]))) ? 0.5 : 0);
};

// ------------------------------------------------------------------- a run --
/* One (case, paradigm) → the failure keys it shows ('unhosted:<why>' if it
   cannot host), the claims it breaks, and the objective metrics. */
function runOne(c, alg, grid) {
  const o = { seed: c.seed, T: c.T, coup: c.coup || 0, fb: true, grid };
  const b = build(alg, pairOf(c), o), s = evaluate(b, o);
  if (s.unhosted) return { alg, hosted: false, plan: b.info.plan, why: s.unhosted, keys: ['unhosted:' + b.info.plan] };
  const fails = KEYS.filter(k => k === 'I5' ? !s.I5ok : s.inv[k] > 0);
  const claims = b.alg in { E: 1, F: 1, G: 1, H: 1, GC: 1 } ? claimsOf(b) : [];
  return { alg, hosted: true, keys: fails, broken: fails.filter(k => claims.includes(k)), first: s.first, whyOf: s.why,
           events: b.events.length, steps: b.flips.length, states: s.states, maxLive: s.maxLive, wp: !!(b.info && b.info.waypoints),
           minT: s.minT, close: s.close, maxStep: s.maxStep };
}
function newAgg() {
  const a = {};
  for (const id of ALG_IDS) a[id] = { runs: 0, hosted: 0, fail: Object.fromEntries(KEYS.map(k => [k, 0])), unhosted: {}, events: 0, steps: 0,
                                      states: 0, maxLive: 0, wp: 0, broken: 0, pairsHosted: 0, pairsAll: 0 };
  return a;
}
function tally(agg, wit, c, r) {
  const A = agg[r.alg];
  A.runs++;
  if (!r.hosted) { const k = r.keys[0]; A.unhosted[k] = (A.unhosted[k] || 0) + 1; }
  else {
    A.hosted++; A.events += r.events; A.steps += r.steps; A.states += r.states; A.maxLive = Math.max(A.maxLive, r.maxLive); if (r.wp) A.wp++;
    for (const k of r.keys) A.fail[k]++;
    if (r.broken.length) A.broken++;
  }
  for (const k of r.keys) {
    const key = r.alg + '|' + k, sz = size(c), w = wit[key];
    if (!w || sz < w.size) wit[key] = { alg: r.alg, key: k, size: sz, c: clone(c), broken: (r.broken || []).includes(k) };
  }
}
function* cases(nPairs, seed, chunk) {
  const [ci, cn] = chunk || [0, 1];
  for (let i = 0; i < nPairs; i++) {
    const r = mulberry32((seed * 7919 + i * 104729) >>> 0);
    const p = genPair(r), caseSeed = 1 + Math.floor(r() * 1e6);
    if (i % cn !== ci) continue;
    for (const T of TEMPS) for (const sd of [caseSeed, caseSeed + 1]) yield { i, L: p.L, R: p.R, kind: p.kind, seed: sd, T, coup: 0 };
  }
}
function* labCases() {
  for (const P of PAIRS) for (const fb of [false, true]) for (const T of TEMPS)
    yield { i: 'lab:' + P.id, L: fb ? P.L : Object.assign({}, P.L, { fb: null }), R: fb ? P.R : Object.assign({}, P.R, { fb: null }), kind: 'lab', seed: 265, T, coup: 0 };
}

// --------------------------------------------------------------- shrinking --
/* Greedy delta-debugging. A candidate is kept when the SAME paradigm still
   shows the SAME failure (at the same seed and temperature) and the case got
   smaller: fewer modules, fewer lane entries, no return, fewer inversions,
   default parameters. Every candidate is repaired back to a valid end first,
   so a shrunk case is always a legal pair of patches. */
function candidates(c) {
  const out = [], push = f => { const d = clone(c); if (f(d) !== false) { repair(d.L); repair(d.R); out.push(d); } };
  for (const s of ['L', 'R']) {
    if (c[s].fb) push(d => { d[s].fb = null; });
    c[s].order.forEach((m, i) => push(d => { d[s].order.splice(i, 1); for (const l of LANES) d[s].lanes[l] = d[s].lanes[l].filter(q => q !== m); }));
    for (const l of LANES) {
      if (c[s].lanes[l].length > 1) c[s].lanes[l].forEach((m, i) => push(d => { d[s].lanes[l].splice(i, 1); }));
      if (c[s].lanes[l][0] !== 'OUT') push(d => { d[s].lanes[l] = ['OUT']; });
      const o = s === 'L' ? 'R' : 'L';
      if (JSON.stringify(c[s].lanes[l]) !== JSON.stringify(c[o].lanes[l])) push(d => { d[s].lanes[l] = c[o].lanes[l].slice(); });
    }
    for (let i = 0; i + 1 < c[s].order.length; i++) {
      const o = c[s === 'L' ? 'R' : 'L'].order;
      const a = o.indexOf(c[s].order[i]), b = o.indexOf(c[s].order[i + 1]);
      if (a >= 0 && b >= 0 && a > b) push(d => { const t = d[s].order[i]; d[s].order[i] = d[s].order[i + 1]; d[s].order[i + 1] = t; });
    }
    if (c[s].order.some(m => JSON.stringify(c[s].p[m]) !== JSON.stringify(DEFAULT_P[m]))) push(d => { for (const m of d[s].order) d[s].p[m] = clone(DEFAULT_P[m]); });
  }
  return out;
}
function stillFails(c, alg, key) { const r = runOne(c, alg); return r.keys.includes(key); }
function shrink(w) {
  let c = w.c, sz = size(c), rounds = 0, tried = 0;
  for (let improved = true; improved && rounds < 40; rounds++) {
    improved = false;
    for (const d of candidates(c)) {
      const s2 = size(d); tried++;
      if (s2 < sz && stillFails(d, w.alg, w.key)) { c = d; sz = s2; improved = true; break; }
    }
  }
  return { c, size: sz, tried };
}

// ------------------------------------------------------------------ report --
const pct = (n, d) => d ? (100 * n / d < 0.1 && n ? '<0.1' : (100 * n / d).toFixed(1)) + '%' : '—';
function table(agg) {
  const lines = [];
  lines.push('paradigm  hosted   ' + KEYS.map(k => LABEL[k].padEnd(10)).join('') + ' mean xf  steps  states  max live  waypoints  no plan (proved)  budget  broken claims');
  for (const id of ALG_IDS) {
    const A = agg[id], h = A.hosted;
    lines.push((id + (ASSERTED.includes(id) ? '*' : ' ')).padEnd(10) + pct(h, A.runs).padEnd(9) +
      KEYS.map(k => pct(A.fail[k], h).padEnd(10)).join('') + ' ' + (h ? (A.events / h).toFixed(1) : '—').padEnd(8) +
      (h ? (A.steps / h).toFixed(1) : '—').padEnd(7) + (h ? (A.states / h).toFixed(1) : '—').padEnd(8) + String(A.maxLive).padEnd(10) + (h ? pct(A.wp, h) : '—').padEnd(11) +
      pct(A.unhosted['unhosted:impossible'] || 0, A.runs).padEnd(18) + pct(A.unhosted['unhosted:budget'] || 0, A.runs).padEnd(8) +
      (ASSERTED.includes(id) ? String(A.broken) : 'n/a'));
  }
  lines.push('* ASSERTED by construction on the invariants it claims (a broken claim fails the gate); unmarked = REPORTED, under study.');
  lines.push('  Rates are the share of HOSTED runs showing the failure at one x or more; "hosted" is the share of runs the paradigm can morph at all');
  lines.push('  (not hosted = the ends do not fit the architecture, or the planner proved no legal plan exists, or ran out of budget).');
  return lines.join('\n');
}
function summaryOf(agg) {
  const out = {};
  for (const id of ALG_IDS) {
    const A = agg[id], h = A.hosted;
    out[id] = { runs: A.runs, hosted: h, fail: A.fail, unhosted: A.unhosted, meanEvents: h ? +(A.events / h).toFixed(2) : null,
                meanSteps: h ? +(A.steps / h).toFixed(2) : null, meanStates: h ? +(A.states / h).toFixed(2) : null, maxLive: A.maxLive,
                waypointShare: h ? +(A.wp / h).toFixed(4) : null, brokenClaims: A.broken };
  }
  return out;
}
const say = s => process.stdout.write(s + '\n');
function describe(c) {
  const P = x => `${x.order.map(t => SHORT[t]).join(' ') || '(dry)'} · Bo→${x.lanes.BODY.map(m => SHORT[m] || m).join('+')} Su→${x.lanes.SUB.map(m => SHORT[m] || m).join('+')}${x.fb ? ` · fb ${SHORT[x.fb.src]}→${SHORT[x.fb.dst]}` : ''}`;
  return `L ${P(c.L)}  ⇒  R ${P(c.R)}  (seed ${c.seed}, T ${c.T})`;
}

// ------------------------------------------------------------------- modes --
const args = process.argv.slice(2), arg = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };

if (args.includes('--sweep')) {
  const n = +arg('--pairs', 2000), [ci, cn] = arg('--chunk', '0/1').split('/').map(Number), out = arg('--out', '.');
  mkdirSync(out, { recursive: true });
  const agg = newAgg(), wit = {}, t0 = Date.now();
  let k = 0, lastI = -1, pairs = 0;
  for (const c of cases(n, GATE_SEED + 1, [ci, cn])) {
    if (c.i !== lastI) { lastI = c.i; pairs++; if (pairs % 50 === 0) say(`chunk ${ci}/${cn}: ${pairs} pairs, ${((Date.now() - t0) / 1000).toFixed(0)} s`); }
    for (const a of ALG_IDS) tally(agg, wit, c, runOne(c, a));
    k++;
  }
  writeFileSync(join(out, `chunk-${ci}-of-${cn}.json`), JSON.stringify({ n, chunk: [ci, cn], runsPerParadigm: k, agg, wit }));
  say(`chunk ${ci}/${cn}: done — ${pairs} pairs, ${k} runs per paradigm, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
  process.exit(0);
}

if (args.includes('--merge')) {
  const dir = arg('--merge'), files = readdirSync(dir).filter(f => /^chunk-.*\.json$/.test(f)).sort();
  const agg = newAgg(), wit = {};
  let n = 0, runs = 0;
  for (const f of files) {
    const J = JSON.parse(readFileSync(join(dir, f), 'utf8'));
    n = J.n; runs += J.runsPerParadigm;
    for (const id of ALG_IDS) {
      const A = agg[id], B = J.agg[id];
      for (const k of ['runs', 'hosted', 'events', 'steps', 'states', 'wp', 'broken']) A[k] += B[k];
      A.maxLive = Math.max(A.maxLive, B.maxLive);
      for (const k of KEYS) A.fail[k] += B.fail[k];
      for (const k in B.unhosted) A.unhosted[k] = (A.unhosted[k] || 0) + B.unhosted[k];
    }
    for (const key in J.wit) if (!wit[key] || J.wit[key].size < wit[key].size) wit[key] = J.wit[key];
  }
  say(`merged ${files.length} chunks: ${n} pairs × ${TEMPS.length} temperatures × 2 seeds = ${runs} runs per paradigm\n`);
  say(table(agg));
  const gallery = [];
  const keys = Object.keys(wit).sort((a, b) => ALG_IDS.indexOf(a.split('|')[0]) - ALG_IDS.indexOf(b.split('|')[0]) || (a < b ? -1 : 1));
  say('\nSHRINKING one witness per (paradigm, failure)…');
  for (const key of keys) {
    const w = wit[key], t0 = Date.now(), s = shrink(w), r = runOne(s.c, w.alg);
    const g = { alg: w.alg, key: w.key, label: LABEL[w.key] || w.key, broken: w.broken, size: s.size, case: s.c,
                rate: w.key.startsWith('unhosted:') ? pct(agg[w.alg].unhosted[w.key] || 0, agg[w.alg].runs) + ' of runs' : pct(agg[w.alg].fail[w.key], agg[w.alg].hosted) + ' of hosted runs',
                firstX: r.first && r.first[w.key] !== undefined ? +r.first[w.key].toFixed(4) : null,
                why: r.hosted ? (r.whyOf && r.whyOf[w.key]) || (w.key === 'I5' ? `${r.close} crowded, max step ${r.maxStep.toFixed(2)}` : '') : r.why,
                text: describe(s.c) };
    gallery.push(g);
    say(`  ${w.alg.padEnd(3)} ${(LABEL[w.key] || w.key).padEnd(22)} size ${String(w.size).padStart(4)} → ${String(s.size).padStart(4)}  ${describe(s.c)}  ${g.why || ''}  (${s.tried} tried, ${((Date.now() - t0) / 1000).toFixed(1)} s)`);
  }
  if (args.includes('--write')) {
    const doc = { generated: 'tools/labharness/fxmorph_check.mjs --merge (B266)', pairs: n, temperatures: TEMPS, seedsPerTemperature: 2, runsPerParadigm: runs,
                  asserted: ASSERTED, studied: STUDIED, summary: summaryOf(agg), gallery };
    writeFileSync(GALLERY, JSON.stringify(doc, null, 1) + '\n');
    say(`\nwrote ${GALLERY.slice(ROOT.length + 1)} (${gallery.length} counterexamples)`);
  }
  process.exit(0);
}

// ---------------------------------------------------------------- the gate --
let bad = 0;
const t0 = Date.now();
const fail = s => { bad++; say('FAIL  ' + s); };

// 1. The lab's own self-check: every claim holds, every planted fault is caught.
const rows = runSelfCheck();
rows.forEach((r, i) => { if (!r.pass) fail(`self-check ${i + 1} claim does not hold: ${r.claim} — ${r.detail}`); if (!r.caught) fail(`self-check ${i + 1} plant NOT caught: ${r.control} — ${r.cdetail}`); });
say(`self-check: ${rows.filter(r => r.pass).length}/${rows.length} claims hold · ${rows.filter(r => r.caught).length}/${rows.length} planted faults caught`);

// 2. The five lab pairs (both return settings) plus GATE_PAIRS random pairs, four temperatures, two seeds.
const agg = newAgg(), wit = {};
const brokenList = [];
const runAll = c => { for (const a of ALG_IDS) { const r = runOne(c, a); tally(agg, wit, c, r); if (r.broken && r.broken.length) brokenList.push(`${a} breaks its claimed ${r.broken.join(', ')} on ${describe(c)}: ${r.broken.map(k => r.whyOf[k] || k).join('; ')}`); } };
for (const c of labCases()) runAll(c);
for (const c of cases(GATE_PAIRS, GATE_SEED)) runAll(c);
for (const s of brokenList.slice(0, 10)) fail(s);
if (brokenList.length > 10) fail(`… and ${brokenList.length - 10} more broken claims`);
say(`\nsample: the five lab pairs × 2 return settings + ${GATE_PAIRS} random pairs, × temperatures ${TEMPS.join(', ')} (random pairs × 2 seeds)`);
say(table(agg));

// 3. The committed gallery still reproduces.
if (existsSync(GALLERY)) {
  const G = JSON.parse(readFileSync(GALLERY, 'utf8'));
  let ok = 0;
  for (const g of G.gallery) { const r = runOne(g.case, g.alg); if (r.keys.includes(g.key)) ok++; else fail(`gallery: ${g.alg} ${g.label} no longer fails on ${g.text} — regenerate with --sweep/--merge --write`); }
  say(`\ngallery: ${ok}/${G.gallery.length} committed counterexamples still fail as recorded (${G.pairs} pairs × ${G.temperatures.length} temperatures × ${G.seedsPerTemperature} seeds behind them)`);
} else fail('gallery missing: ' + GALLERY.slice(ROOT.length + 1));

say(`\nASSERTED: ${ASSERTED.join(' ')} hold every claimed invariant whenever they host a pair; the self-check and gallery replay.`);
say(`REPORTED: ${STUDIED.join(' ')} failure rates (under study; D is the control), and every paradigm's coverage.`);
say(bad ? `fxmorph_check: FAILED (${bad}) in ${((Date.now() - t0) / 1000).toFixed(1)} s` : `fxmorph_check: GREEN in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
process.exit(bad ? 1 : 0);
