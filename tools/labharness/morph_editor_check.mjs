/*
 * morph_editor_check.mjs — the morph editor lab's in-page audits, as a gate.
 * WIRED: ./verify fast (beside fxlab_check).
 *
 *   node tools/labharness/morph_editor_check.mjs [lab.html]
 *        (default: docs/design/morph-editor-lab.html; ~1-2 s)
 *
 * WHY THIS EXISTS (B269). docs/design/morph-editor-lab.html audits itself at
 * load and PRINTS the verdict — four audit lines under its tagline:
 *   field audit      (B211) ownership parity against the engine's own
 *                    morphOwnersJson, 1092/1092 at the time of writing;
 *   waypoint audit   (B235) six properties of the four waypoint laws and four
 *                    must-fail controls;
 *   interaction audit (B268) the page opens editable, and five pad gestures run
 *                    through the one pin write path, with four controls;
 *   round-3 audit    (B269) the puck stays still under pin gestures, the hover
 *                    readout is the colour under it, a LOCKED quantum row
 *                    survives reseed / temperature / Cohesion and an unlocked
 *                    one does not, a boundary drag lands on the pointer, and
 *                    STEP 0 is BLEND bit for bit — each with a control.
 * A verdict printed on a page is read only by whoever opens it. B268's trace
 * asked for this file (open question 1) and the human said "Go for it" (B269).
 *
 * HOW IT RUNS THE PAGE. Every inline <script> is executed in a vm context
 * under the same universal-proxy DOM stub as lab_load_check.mjs (copied, not
 * imported: that file is a CLI whose top level sweeps every lab on import).
 * The audits are pure — no DOM, no clock, mulberry32 only — so what they
 * compute under the stub is what they compute in a browser. The page leaves
 * its results on `window.__morphEditorAudit`; this file reads them back.
 *
 * THE COUNTS ARE PINNED, not just the verdicts. A page that quietly dropped a
 * property would still report "all green"; so the number of properties and
 * controls each audit ran must equal the number below. Adding a check to the
 * page means raising a number here in the same change — never lowering one
 * (a lowered count is a weakened gate: human decision, ROADMAP).
 *
 * ITS OWN MUST-FAIL CONTROL (L0016/L0032). A gate that has only ever said
 * GREEN has not been shown able to say anything else. Every run therefore also
 * loads SCRATCH COPIES of the page, in memory, each with one fault planted by
 * anchored substitution (the anchor must occur exactly once, or the plant
 * itself fails — a plant that silently misses would make the control pass for
 * the wrong reason), and requires each copy to come back RED. The planting
 * mechanism is checked first with an anchor that must not be found.
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join } from 'node:path';
import vm from 'node:vm';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const labPath = process.argv[2] ? resolve(process.argv[2]) : join(root, 'docs/design/morph-editor-lab.html');
const html = isMainThread ? readFileSync(labPath, 'utf8') : '';

// What each audit must have RUN (not merely passed). See the header.
const EXPECT = {
  waypoints: { nProps: 6, nControls: 4 },
  interaction: { nG: 5, nControls: 4 },
  b269: { nProps: 9, nControls: 9 },
};

// lab_load_check.mjs's stub, verbatim in spirit: a value that can be called,
// constructed, indexed, iterated and coerced without ever throwing.
function stub() {
  const f = function () {};
  return new Proxy(f, {
    get(t, p) {
      if (p === Symbol.iterator) return function* () {};
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      if (p === 'then') return undefined;
      return stub();
    },
    set() { return true; },
    has() { return true; },
    apply() { return stub(); },
    construct() { return stub(); },
  });
}

function runPage(src) {
  const blocks = [...src.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
  if (!blocks.length) return { error: 'no inline <script> in the lab' };
  const sandbox = {
    document: stub(), location: stub(), navigator: stub(),
    requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
    setTimeout: () => 0, setInterval: () => 0, clearTimeout: () => {}, clearInterval: () => {},
    addEventListener: () => {}, alert: () => {},
    console: { log() {}, warn() {}, error() {} },
    Math, JSON, performance: { now: () => 0 },
    Event: class { constructor(t) { this.type = t; } },
    getComputedStyle: () => stub(), devicePixelRatio: 1,
    matchMedia: () => ({ matches: false, addEventListener() {}, addListener() {} }),
  };
  sandbox.globalThis = sandbox; sandbox.self = sandbox; sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  for (let i = 0; i < blocks.length; i++) {
    try {
      new vm.Script(blocks[i], { filename: `morph-editor-lab#script${i + 1}` }).runInContext(ctx, { timeout: 30000 });
    } catch (e) {
      return { error: `script ${i + 1} threw ${e && e.name}: ${e && e.message}` };
    }
  }
  return { A: sandbox.__morphEditorAudit };
}

/* The verdict on one load: a list of failures (empty = green) and one summary
   line per audit. Pure over the audit object, so the plants go through exactly
   the judgement the real page does. */
function judge(res) {
  const bad = [], lines = [];
  if (res.error) return { bad: [res.error], lines };
  const A = res.A;
  if (!A) return { bad: ['the page left no window.__morphEditorAudit'], lines };
  const P = A.parity;
  if (!P || !(P.total > 0) || P.agree !== P.total) bad.push(`field audit: parity ${P ? P.agree + '/' + P.total : 'missing'}${P && P.bad && P.bad.length ? ' — ' + P.bad.join(', ') : ''}`);
  if (P) lines.push(`field audit        parity ${P.agree}/${P.total}`);
  const W = A.waypoints;
  if (!W) bad.push('waypoint audit: missing');
  else {
    if (W.nProps !== EXPECT.waypoints.nProps || W.nControls !== EXPECT.waypoints.nControls)
      bad.push(`waypoint audit ran ${W.nProps} properties / ${W.nControls} controls; expected ${EXPECT.waypoints.nProps} / ${EXPECT.waypoints.nControls}`);
    if (W.props !== W.nProps) bad.push(`waypoint audit: properties ${W.props}/${W.nProps} — ${JSON.stringify(W.fails)}`);
    if (W.controls !== W.nControls) bad.push(`waypoint audit: controls ${W.controls}/${W.nControls}`);
    lines.push(`waypoint audit     properties ${W.props}/${W.nProps} · controls ${W.controls}/${W.nControls}`);
  }
  const I = A.interaction;
  if (!I) bad.push('interaction audit: missing');
  else {
    if (I.nG !== EXPECT.interaction.nG || I.nControls !== EXPECT.interaction.nControls)
      bad.push(`interaction audit ran ${I.nG} gestures / ${I.nControls} controls; expected ${EXPECT.interaction.nG} / ${EXPECT.interaction.nControls}`);
    if (!I.ok) {
      const f = Object.keys(I.G).filter(k => !I.G[k]).concat(Object.keys(I.R).filter(k => !I.R[k]).map(k => 'G5.' + k));
      bad.push(`interaction audit: open ${I.open.ok ? 'ok' : 'FAILS'} · gestures ${I.gOk}/${I.nG} · controls ${I.controls}/${I.nControls}${f.length ? ' — ' + f.join(', ') : ''}`);
    }
    lines.push(`interaction audit  open ${I.open.ok ? 'ok' : 'FAILS'} · gestures ${I.gOk}/${I.nG} · controls ${I.controls}/${I.nControls}`);
  }
  const B = A.b269;
  if (!B) bad.push('round-3 audit (B269): missing');
  else {
    if (B.nProps !== EXPECT.b269.nProps || B.nControls !== EXPECT.b269.nControls)
      bad.push(`round-3 audit ran ${B.nProps} properties / ${B.nControls} controls; expected ${EXPECT.b269.nProps} / ${EXPECT.b269.nControls}`);
    const fp = Object.keys(B.props).filter(k => !B.props[k]), fc = Object.keys(B.controls).filter(k => !B.controls[k]);
    if (fp.length || fc.length) bad.push(`round-3 audit: failing ${fp.concat(fc.map(k => 'control ' + k)).join(', ')}`);
    lines.push(`round-3 audit      properties ${B.nProps - fp.length}/${B.nProps} · controls ${B.nControls - fc.length}/${B.nControls}`);
  }
  return { bad, lines };
}

/* THE PLANTS. Each is one fault a real regression could introduce, and each
   names the audit that must go red for it — one plant per audit, so the control
   covers the gate's whole reach and a plant caught only by a neighbouring audit
   does not count (a 1% Cohesion drift, the first parity plant tried, flips no
   owner among the 1092 and was caught only by the lock audit: a finding about
   the parity audit's resolution, and the reason `audit` is checked here). */
const PLANTS = [
  { audit: 'field audit', name: 'the resolver drifts from morph_core.h (the shared draw read mirrored)',
    anchor: 'const s = lw[k] + (1 - coup) * G.g[i * 4 + k] + coup * G.gs[k];',
    with: 'const s = lw[k] + (1 - coup) * G.g[i * 4 + k] + coup * G.gs[3 - k];' },
  { audit: 'waypoint audit', name: 'the corner guard is gone (a law computes the corner instead of reading it)',
    anchor: 'if (isCornerXY(x, y)) { const v = s.base(x, y); return { v, raw: v, clip: 0 }; }',
    with: '' },
  { audit: 'interaction audit', name: 'the page opens in QUANTUM again (B235\'s opening)',
    anchor: "mode: 1,                                   // 0 quantum, 1 blend, 2 stepped",
    with: "mode: 0,                                   // 0 quantum, 1 blend, 2 stepped" },
  { audit: 'round-3 audit', name: 'B268\'s puck: a click on empty pad moves the puck again',
    anchor: "if (!g) { g = { kind: 'none' };",
    with: "if (!g) { st.x = p.x; st.y = p.y; g = { kind: 'none' };" },
  { audit: 'round-3 audit', name: 'a lock that is only a flag: pickFor ignores the stored offsets',
    anchor: 'const L = st.qlock[LEAD[i]] || null;',
    with: 'const L = null;' },
];
function plant(src, a, b) {
  const n = src.split(a).length - 1;
  if (n !== 1) throw new Error(`anchor found ${n} times (must be exactly 1): ${a}`);
  return src.replace(a, () => b);
}

/* EACH LOAD IS INDEPENDENT, so the real page and every plant load in parallel,
   one worker thread each (node's built-in worker_threads — no dependency): ~0.8 s
   per load, and six in sequence was 5.3 s of the fast leg. A worker gets the
   source text and returns only judge()'s strings, so nothing but a verdict
   crosses the thread boundary. */
function judgeInWorker(src) {
  return new Promise(res => {
    const w = new Worker(new URL(import.meta.url), { workerData: { src } });
    w.once('message', res);
    w.once('error', e => res({ bad: [`worker threw: ${e && e.message}`], lines: [] }));
  });
}
if (!isMainThread) {
  parentPort.postMessage(judge(runPage(workerData.src)));
} else {
  let red = 0;
  // The planting mechanism first: an anchor that is not in the page must throw.
  let mechOk = false;
  try { plant(html, '/* no such anchor: morph_editor_check self-test */', ''); } catch (_) { mechOk = true; }
  const jobs = [judgeInWorker(html)];
  const planted = PLANTS.map(p => { try { return { p, src: plant(html, p.anchor, p.with) }; } catch (e) { return { p, err: e.message }; } });
  for (const x of planted) jobs.push(x.src !== undefined ? judgeInWorker(x.src) : Promise.resolve(null));
  const [real, ...verdicts] = await Promise.all(jobs);
  for (const l of real.lines) console.log(l);
  for (const b of real.bad) { console.log(`FAIL ${b}`); red++; }
  if (!mechOk) { console.log('FAIL control: the planting mechanism accepted an anchor that is not in the page'); red++; }
  let caught = 0;
  planted.forEach(({ p, err }, k) => {
    if (err) { console.log(`FAIL control "${p.name}": ${err}`); red++; return; }
    const v = verdicts[k], hit = v.bad.find(b => b.startsWith(p.audit));
    if (hit) { caught++; console.log(`OK   control: planted "${p.name}" → ${p.audit} RED (${hit.slice(p.audit.length).replace(/^[\s:(]+/, '').slice(0, 90)})`); }
    else { console.log(`FAIL control: planted "${p.name}" and the ${p.audit} stayed GREEN${v.bad.length ? ' (only: ' + v.bad[0].slice(0, 80) + ')' : ''}`); red++; }
  });
  console.log(`morph_editor_check: ${red ? 'RED' : 'GREEN'} — ${real.bad.length} audit failure(s); controls ${caught}/${PLANTS.length} planted faults caught`);
  process.exit(red ? 1 : 0);
}
