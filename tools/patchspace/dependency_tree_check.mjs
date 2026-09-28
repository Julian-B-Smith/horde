/*
 * dependency_tree_check.mjs — the oracle for tools/patchspace/dependency_tree.json (B316 P1).
 * WIRED: ./verify full.
 * HYPERSAW, 2026-09-27, ROADMAP B316 (records PR #810, branch lead-records-122).
 *
 * ASSERTS (exit 1 on any red row):
 *   T1  FRESH: re-running tools/patchspace/gen_dependency_tree.mjs reproduces the
 *       committed JSON BYTE FOR BYTE. A changed engine, oracle guard, lab table or
 *       taper that moves any condition or probe count makes it stale; regenerate
 *       (node tools/patchspace/gen_dependency_tree.mjs) and review the diff. The one
 *       exception: a difference confined to anchors' `line` numbers is printed as DRIFT
 *       and passes (an unrelated edit above a cited line; playbook_check.py's rule).
 *   T2  AGREEMENT: the regenerated tree has ZERO structural disagreements (a parameter
 *       heard where neither its role nor a declared side channel says it can be, or a
 *       role declared live in >= 2 contexts and never heard there). Regime-inert rows
 *       and lab greys-but-live rows are REPORTED in the JSON, not failed here: the
 *       first are value corners the grammar does not model, the second are the lab's
 *       UI findings (docs/design/** is not this tool's to change).
 *   T3  MUST-READ-ZERO: the same patch rendered twice differs by exactly 0 (the probe's
 *       own determinism), and cScale — never heard in the composed engine — reads
 *       inert, exactly zero, in every context it was probed in.
 * MUST-FAIL CONTROLS (L0032: a probe that cannot fail proves nothing), each a planted
 * wrong condition run through the SAME targeted probe and compare() the generator
 * uses, each required to come back with a structural disagreement:
 *   C1  harmReach declared under the STRETCH law (h.law 5) instead of harmonic (4);
 *   C2  kHz declared under lock != 2 (Cut rate's condition) instead of lock == 2;
 *   C3  gain declared never live (false);
 *   C4  a vanished source anchor must THROW (the stale-engine guard);
 *   C5  T1's own comparator: a planted condition change must read STALE and a moved
 *       anchor line only DRIFT, so the drift exception cannot swallow a real change.
 * Why in full, not fast: the generator's probe is ~480 seeded renders on worker threads,
 * 35 s on an idle machine and ~115 s measured under load — station_check's reason.
 * By hand:  node tools/patchspace/dependency_tree_check.mjs   (--workers N)
 */
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, render, loadSpace } from './space.mjs';
import { generate, serialize, OUT, asEvalTree, buildTasks, runTasks, compare, SCRIPT, locate } from './gen_dependency_tree.mjs';

let red = 0;
const row = (ok, id, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${id.padEnd(4)} ${text}`); if (!ok) red++; };
const w = process.argv.indexOf('--workers'), workers = w > 0 ? +process.argv[w + 1] : undefined;
const t0 = process.uptime();

/* T1 + T2 */
const json = await generate({ workers });
const fresh = serialize(json), committed = readFileSync(join(ROOT, OUT), 'utf8');
/* An anchor's LINE NUMBER is evidence, not substance: an unrelated edit above a cited line
   (the lab gets a round a day) moves it without changing any condition. So a difference
   confined to `line` fields is printed as drift, not failed — tools/playbook_check.py's
   rule for its own citations. A vanished anchor still fails (the generator throws). */
const noLines = t => t.replace(/"line": \d+/g, '"line": 0');
const freshness = (a, b) => (a === b ? 'fresh' : noLines(a) === noLines(b) ? 'drift' : 'stale');
if (fresh === committed) row(true, 'T1', `${OUT} is fresh (${fresh.length} bytes reproduced)`);
else if (noLines(fresh) === noLines(committed)) {
  const n = (committed.match(/"line": \d+/g) || []).filter((l, i) => l !== (fresh.match(/"line": \d+/g) || [])[i]).length;
  row(true, 'T1', `${OUT} is fresh in substance; ${n} anchor line number(s) drifted (regenerate to refresh the citations)`);
} else {
  const a = fresh.split('\n'), b = committed.split('\n');
  let i = 0; while (i < a.length && a[i] === b[i]) i++;
  row(false, 'T1', `${OUT} is STALE: first difference at line ${i + 1}\n        committed: ${(b[i] || '<eof>').slice(0, 160)}\n        generated: ${(a[i] || '<eof>').slice(0, 160)}\n        regenerate: node tools/patchspace/gen_dependency_tree.mjs`);
}
const s = json.summary;
row(s.structuralDisagreements === 0, 'T2', `static tree vs probe: ${s.structuralDisagreements} structural disagreements over ${json.probe.comparisons} comparisons` +
  ` (reported, not failed: ${s.regimeInert} regime-inert, ${s.sideChannelHits} side-channel hits, ${s.labGreysButLive} lab greys-but-live)`);
for (const d of json.disagreements) console.log('        ' + JSON.stringify(d));

/* T3 */
{
  const C = Object.assign({}, loadSpace().defaults, { b2on: 1, mode: 2, mode2: 3, law: 4, N: 6 });
  const a = render(C, SCRIPT, { seed: 99 }), b = render(C, SCRIPT, { seed: 99 });
  let m = 0; for (let i = 0; i < a.L.length; i++) m = Math.max(m, Math.abs(a.L[i] - b.L[i]), Math.abs(a.R[i] - b.R[i]));
  const cs = json.params.cScale.probe;
  row(m === 0 && cs.live === 0 && cs.inertExactlyZero && cs.contexts > 0, 'T3',
    `same patch twice: max |Δ| ${m} (must be 0); cScale: live in ${cs.live} of ${cs.contexts} contexts, inert deltas exactly 0: ${cs.inertExactlyZero}`);
}

/* controls: plant a wrong condition, probe that parameter in its own targeted contexts */
const base = asEvalTree(json);
async function plant(id, key, cond, why) {
  const tree = { predicates: base.predicates, params: Object.assign({}, base.params, { [key]: { active_when: cond, side: base.params[key].side } }) };
  const tasks = buildTasks(tree, [key]);
  const res = await runTasks(tasks, workers);
  const { dis } = compare(tree, tasks, res, null);
  const hit = dis.filter(d => d.key === key);
  row(hit.length > 0, id, `planted ${why}: ${hit.length ? 'CAUGHT — ' + [...new Set(hit.map(d => d.kind))].join(', ') + ` (${hit.length} rows over ${tasks.length} contexts)` : 'NOT caught over ' + tasks.length + ' contexts'}`);
}
await plant('C1', 'harmReach', { all: [{ pred: 'DETUNED' }, { eq: ['h.law', 5] }] }, 'harmReach live under h.law 5 (stretch) instead of 4');
await plant('C2', 'kHz', { all: [{ pred: 'B1' }, { ne: ['lock', 2] }] }, 'kHz live under lock != 2');
await plant('C3', 'gain', false, 'gain never live');
{
  let threw = '';
  try { locate(['oracle', 'a guard text that is not in razor-core.js']); }
  catch (e) { threw = e.message; }
  row(/anchor not found/.test(threw), 'C4', `a vanished anchor throws: ${threw ? threw.slice(0, 90) + '…' : 'DID NOT THROW'}`);
}
{
  /* T1's comparator on planted files: one condition constant changed must read STALE; one
     anchor line moved must read only DRIFT (the exception must not swallow a real change) */
  const cond = committed.replace('"ge": [\n     "N",\n     2', '"ge": [\n     "N",\n     3');
  const line = committed.replace(/"line": (\d+)/, (_, n) => `"line": ${+n + 1}`);
  const a = freshness(cond, committed), b = freshness(line, committed);
  row(cond !== committed && a === 'stale' && b === 'drift', 'C5', `T1's comparator: a planted condition change reads ${cond === committed ? 'NOT PLANTED (anchor text missing)' : a} (must be stale); a moved anchor line reads ${b} (must be drift)`);
}
console.log(`dependency_tree_check: ${red ? red + ' RED' : 'GREEN'} — ${s.params} params, ${json.probe.comparisons} probe comparisons, 0 structural disagreements required; ${(process.uptime() - t0).toFixed(1)} s`);
process.exit(red ? 1 : 0);
