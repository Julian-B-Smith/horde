# b135-orbital-initial-conditions — per-body home/throw, and a distance scale that is a readout, not physics

- **Queue item:** B135 (ORBITAL per-body initial conditions + distance scale), brief
  `briefs/2026-09-16-b135-orbital-initial-conditions.md` on the lead's branch.
- **Why:** the human asked for control over where a body starts and how it is thrown,
  and for a way to make a relative equilibrium fill more of the grid "without messing
  up the gravity dynamics". Those are two different requests and the answer keeps them
  apart. Initial conditions become *state a body owns* (`hx0/hy0`, `vx0/vy0`), so Reset
  restores what you found rather than re-instantiating the preset literal. The distance
  scale is a **readout transform** — the one place the scale exists is `gu(u) = 0.5 +
  (u-0.5)*scale`, downstream of the integrator — so "the dynamics are unchanged" is true
  by construction (bit-identical trajectory, gate G2) rather than by tolerance. The
  physical alternative the human's phrasing also permits (a period-preserving similarity)
  is real and useful, so it ships as a one-shot **Bake** button whose contract is that the
  first frame after baking reads exactly as the view did before it (gate G6).

## What changed

`reference/gravity-modulator.html` (CANDIDATE lab, ADR-165 — this edit is the human's
own request, per the brief):

- **Model.** `mk()` seeds `hx0/hy0/vx0/vy0` from the body's literal `x/y/vx/vy`, so every
  built-in preset behaves exactly as before. `primeStrips(b)` factored out of `mk`;
  `returnHome(b)` / `setHomeHere(b)` are the only paths that touch initial conditions.
- **Reset.** `resetAll()` returns every body to *its own* initial conditions and zeroes
  `simTime`; it no longer reloads the preset, so added bodies, re-homed bodies and the
  universe knobs survive a reset (SPEC-ORBITAL §6.3, amended).
- **Body card.** Editable home x/y; the throw entered as **speed + heading** (0° = right,
  90° = up — the same convention as the `angle` observable and the y flip) with the
  cartesian `vx₀, vy₀` shown beside it; **Set home = here** (captures live position AND
  velocity) and **Return home**.
- **Distance scale.** `P.scale` (0.25–4, log₂ slider so the ends land exactly), applied in
  `gu/gx/gy` and consumed by the observables, the strips, the drawing and the pointer
  hit-test; `toSim()` is the inverse, so grabs and throws stay in sim units at every
  scale. Nothing in `step()`, `accel()` or `energy()` reads it.
- **Bake scale into bodies.** `bakeScale()`: positions and velocities ×s about the field
  **centre**, `G` ×s³, `ε` ×s, then scale → 1 and strips re-primed.
- **Drawing.** When scale ≠ 1 the field outlines the simulation box (dashed): at scale < 1
  bodies bounce off an invisible inner wall and at scale > 1 they leave the view, and
  without the outline either reads as a bug.

`specs/SPEC-ORBITAL.md`: §3.1 gains `Distance scale` and `Bake scale into bodies` rows
(with the exact formula, the clamp and the walls-do-not-scale caveat); §3.2's initial
velocity row becomes speed + heading with the heading convention stated; §6.3 gains the
stored-initial-conditions reset rule and the two per-body actions; §7's Reset trigger and
§9's UI list updated.

## Evidence consulted

- The brief's verbatim B135 acceptance criteria (quoted in the dispatch).
- `reference/gravity-modulator.html` in full; `specs/SPEC-ORBITAL.md` §§3, 6.3, 7, 9.
- `tools/labharness/lab_load_check.mjs` (the vm/stub approach this harness reuses).
- LIBRARY L0026/L0041 (TDZ — every new binding is a hoisted `function` or is declared
  above its first *executed* use), L0046 (exact-anchor HTML edits with uniqueness
  asserts — every replacement in the patch scripts asserted `count == 1` before writing),
  L0032/L0033 (controls that must fire), L0048 (scratch namespaced per stream).
- Prior trace `traces/2026-09-15-orbital-lab-cushion-graphs.md` conventions (sim-time
  strips + rAF fallback, both left intact).

## Alternatives rejected

- **Scaling the physics with the slider** (positions ×s, velocities ×s, G ×s³ live). It
  is a genuine similarity, but the walls and the cushion band are anchored to the grid,
  so any wall contact changes the dynamics — the request was explicitly "without messing
  up the gravity dynamics". Kept as the Bake button, which is honest about being a
  one-way rewrite.
- **Scaling about the centroid** (the brief's phrasing for the bake). The acceptance
  requires the first frame after the bake to equal the pre-bake readout, and the readout
  map's fixed point is the field centre, so the bake must use the same fixed point. With
  COM lock on (the default) the two coincide. Flagged to the lead.
- **Clamping drawn bodies to the grid.** The literal reading of "the drawing … clamped to
  0..1" would pile bodies against a wall they are not touching. The observables clamp
  (that is their contract); the drawing lets them leave and marks where the walls are.
  Flagged to the lead.
- **Leaving `ε` alone in the bake.** Softening is a length; an unscaled `ε` changes the
  orbit shape the bake claims to preserve. Scaled, and clamped to the knob range — an
  off-knob value would be a lie about the patch.
- **Putting the harness in `tools/labharness/`.** It is specific to one lab and the brief
  scoped it to scratch unless reusable; embedded below instead, because a scratch file is
  not durable evidence (L0048).

## Verify

`./verify fast` — exit 0, git c914bfa (`.harness/last-verify.json`):

```
verify: .leakcheck-names absent — private-name leak check SKIPPED (expected off this Mac)
mailbox_delivery: no sibling mailbox checked out — SKIPPED
presentation_check: GREEN (325 rows, scopes: global, osc1, osc2; 34 undesigned (no chunk named), 5 ungrouped)
  note  engine guards law==0 but no parameter declares it
  note  engine guards law==1 but no parameter declares it
depends_check: GREEN (121 declared dependencies, header current, 2 advisory)
gen_gui_controls: GREEN (197 generated control(s), gui2 markup current)
test_table_check: GREEN (189 tests — 104 agentic, 85 human; 15 awaiting an oracle)
  gui.html     reaches 102 / 243 params
  gui2.html    reaches 226 / 243 params
  exempt: inertiaCurve — dev-only, labelled (dev) in the param table
  patch-scope params (raw-id dispatch, must be data-fixed): 89
gui_reach: GREEN (every declared param is reachable in some GUI)
```

B135 harness (`orbital_b135_check.mjs`, source embedded below), run against the edited
lab — 13/13:

```
PASS  G1 lab loads  — 1 script block(s), no throw
PASS  G2 raw trajectory identical at scale 1 and 2  — 1600 doubles compared (chaos, 4000 steps)
PASS  G3a observables differ at scale 2  — 1200/1600 samples differ
PASS  G3b observables match an independent 0.5+(u-0.5)*s, clamped  — 0 mismatches at scale 4; 2 bodies past the grid — 0 would mean the clamp was never exercised
PASS  G4 CONTROL 1-ULP-class kick DOES diverge  — 1592/1600 doubles differ
PASS  G5 CONTROL baked s=2 is NOT the s=1 trajectory  — 1600/1600 doubles differ
PASS  G6 first frame after bake == pre-bake readout  — scale 1, 0 readout diffs
PASS  G6b bake rewrote the initial conditions  — 15/16 initial-condition values changed
PASS  G8 CONTROL the run actually moved (return-home is not vacuous)  — 8/8 doubles differ after 801 more steps
PASS  G7a return home restores the state at the set
PASS  G7c strips primed to the new observables on return (SPEC §6.3)
PASS  G7b the frame after the return == the frame after the set
PASS  G9 Reset lands on the stored initial conditions, not the preset literal  — simTime 0

GREEN — 13/13 gates passed  [gravity-modulator.html]
```

Note on G1: the repo's own `lab_load_check.mjs` reports RED on this file for a reason
that predates this change and is the checker's, not the lab's — its sandbox has no
`getComputedStyle`. It is also not covered by `./verify fast`, whose sweep is
`docs/design/` + `src/gui/`; all 15 labs under `reference/` are outside it since the
2026-09-07 layout move (63f0c9d). Reported to the lead; `./verify` and its gates are
out of scope for this brief. G1 above is the same check with the missing global stubbed.

<details><summary>orbital_b135_check.mjs (the durable copy — the scratch original is ephemeral)</summary>

```javascript
/*
 * orbital_b135_check.mjs — B135 gates for reference/gravity-modulator.html.
 *
 * Runs the lab's script for real in a vm (the lab_load_check.mjs approach and
 * its stub proxies, plus getComputedStyle, which that checker's sandbox lacks),
 * then drives the simulation through the lab's OWN top-level bindings — global
 * lexical declarations are shared between scripts in one vm context, so
 * `nodes`, `P`, `step`, `returnHome` are all reachable from a probe script.
 *
 * Gates:
 *   G1 load           — the script survives evaluation (L0026 class).
 *   G2 scale identity — raw x,y,vx,vy bit-identical at scale 1 vs 2 ...
 *   G3               ... while the observables differ, and match an
 *                       INDEPENDENT re-implementation of 0.5+(u-0.5)*s clamped
 *                       (the detector must not borrow the lab's own gu()).
 *   G4 must-differ    — a 1-ULP kick to one velocity makes the comparator fire,
 *                       so G2's "identical" is not vacuous.
 *   G5 bake must-differ — the brief's named control: bake at s=2 then run is
 *                       NOT the s=1 trajectory (the similarity is real physics).
 *   G6 bake readout   — the first frame after bake equals the pre-bake readout.
 *   G7 set/return     — set home = here, run on, return home: state and the
 *                       next frame are bit-identical to the moment of the set.
 *   G8 must-differ    — the same run WITHOUT returning home is not equal, so
 *                       G7 cannot pass on a frozen sim.
 *
 * Usage: node orbital_b135_check.mjs [path/to/gravity-modulator.html]
 */
import { readFileSync } from 'node:fs';
import { resolve, basename } from 'node:path';
import vm from 'node:vm';

const file = resolve(process.argv[2] || 'reference/gravity-modulator.html');

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
    set() { return true; }, has() { return true; },
    apply() { return stub(); }, construct() { return stub(); },
  });
}

const html = readFileSync(file, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
const sandbox = {
  document: stub(), navigator: stub(), location: stub(),
  getComputedStyle: () => stub(),
  AudioContext: function () { return stub(); }, webkitAudioContext: function () { return stub(); },
  requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
  setTimeout: () => 0, setInterval: () => 0, clearTimeout: () => {}, clearInterval: () => {},
  addEventListener: () => {}, alert: () => {},
  console: { log() {}, warn() {}, error() {} },
  Math, JSON, Date, performance: { now: () => 0 },
  Event: class { constructor(t) { this.type = t; } },
  CustomEvent: class { constructor(t, o) { this.type = t; this.detail = o && o.detail; } },
  Image: class {}, Blob: class {}, URL: { createObjectURL: () => '', revokeObjectURL() {} },
};
sandbox.globalThis = sandbox; sandbox.self = sandbox; sandbox.window = sandbox;
const ctx = vm.createContext(sandbox);

let fails = 0, checks = 0;
function gate(name, ok, detail = '') {
  checks++; if (!ok) fails++;
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  — ' + detail : ''}`);
}

// ------------------------------------------------------------------ G1 load
try {
  for (let i = 0; i < blocks.length; i++)
    new vm.Script(blocks[i], { filename: `${basename(file)}#script${i + 1}` })
      .runInContext(ctx, { timeout: 20000 });
  gate('G1 lab loads', true, `${blocks.length} script block(s), no throw`);
} catch (e) {
  gate('G1 lab loads', false, `${e.name}: ${e.message}`);
  console.log(`\nRED — ${fails}/${checks} gates failed`); process.exit(1);
}

// Probe surface over the lab's own bindings.
vm.runInContext(`globalThis.__p = {
  load, step, returnHome, setHomeHere, resetAll, bakeScale,
  setScale(s){ P.scale=s; }, getScale(){ return P.scale; },
  DT(){ return DT; },
  raw(){ return nodes.flatMap(b=>[b.x,b.y,b.vx,b.vy]); },
  ic(){ return nodes.flatMap(b=>[b.hx0,b.hy0,b.vx0,b.vy0]); },
  obs(){ return nodes.flatMap(b=>[outX(b),outY(b),outAngle(b),outRadius(b)]); },
  primed(){ const f=Math.fround;   // hx/hy/ha are Float32Array: compare the fround, not the double
    return nodes.every(b=>Object.is(b.hx[0],f(outX(b)))&&Object.is(b.hy[0],f(outY(b)))
                         &&Object.is(b.ha[0],f(outAngle(b)))&&b.hp===0); },
  setHomeAll(){ for(const b of nodes) setHomeHere(b); },
  returnAll(){ for(const b of nodes) returnHome(b); },
  simTime(){ return simTime; },
  kick(i,d){ nodes[i].vx+=d; },
};`, ctx);
const p = sandbox.__p;

const DT = p.DT();
function run(preset, scale, steps, sampleEvery, kick) {
  p.load(preset); p.setScale(scale);
  if (kick) p.kick(kick[0], kick[1]);
  const raw = [], obs = [];
  for (let i = 0; i < steps; i++) {
    p.step(DT);
    if (i % sampleEvery === 0) { raw.push(...p.raw()); obs.push(...p.obs()); }
  }
  return { raw, obs };
}
const same = (a, b) => a.length === b.length && a.every((v, i) => Object.is(v, b[i]));
const ndiff = (a, b) => a.reduce((n, v, i) => n + (Object.is(v, b[i]) ? 0 : 1), 0);

// ------------------------------------------------ G2/G3 scale is a readout
const PRESET = 'chaos', STEPS = 4000, EVERY = 40;
const s1 = run(PRESET, 1, STEPS, EVERY);
const s2 = run(PRESET, 2, STEPS, EVERY);
gate('G2 raw trajectory identical at scale 1 and 2', same(s1.raw, s2.raw),
     `${s1.raw.length} doubles compared (${PRESET}, ${STEPS} steps)`);
gate('G3a observables differ at scale 2', ndiff(s1.obs, s2.obs) > 0,
     `${ndiff(s1.obs, s2.obs)}/${s1.obs.length} samples differ`);

// Independent re-implementation of the contract: x -> clamp(0.5+(x-0.5)*s).
{
  const SC = 4;   // far enough out that bodies leave the grid, so the clamp is covered
  p.load(PRESET); p.setScale(SC);
  for (let i = 0; i < 300; i++) p.step(DT);
  const raw = p.raw(), obs = p.obs(), c = v => Math.max(0, Math.min(1, v));
  let bad = 0, clamped = 0;
  for (let b = 0; b * 4 < raw.length; b++) {
    const x = raw[b * 4], y = raw[b * 4 + 1];
    const gx = 0.5 + (x - 0.5) * SC, gy = 0.5 + (y - 0.5) * SC;
    if (!Object.is(obs[b * 4], c(gx))) bad++;
    if (!Object.is(obs[b * 4 + 1], c(1 - gy))) bad++;
    if (gx !== c(gx) || gy !== c(gy)) clamped++;
  }
  gate('G3b observables match an independent 0.5+(u-0.5)*s, clamped', bad === 0 && clamped > 0,
       `${bad} mismatches at scale ${SC}; ${clamped} bodies past the grid — 0 would mean the clamp was never exercised`);
}

// ------------------------------------------------ G4 the comparator can fail
{
  const perturbed = run(PRESET, 1, STEPS, EVERY, [0, 1e-12]);
  gate('G4 CONTROL 1-ULP-class kick DOES diverge', !same(s1.raw, perturbed.raw),
       `${ndiff(s1.raw, perturbed.raw)}/${s1.raw.length} doubles differ`);
}

// ------------------------------------------------ G5 bake is real physics
{
  p.load(PRESET); p.setScale(2); p.bakeScale();
  const raw = [];
  for (let i = 0; i < STEPS; i++) { p.step(DT); if (i % EVERY === 0) raw.push(...p.raw()); }
  gate('G5 CONTROL baked s=2 is NOT the s=1 trajectory', !same(s1.raw, raw),
       `${ndiff(s1.raw, raw)}/${s1.raw.length} doubles differ`);
}

// ------------------------------------------------ G6 bake preserves the view
{
  p.load(PRESET); p.setScale(2);
  for (let i = 0; i < 600; i++) p.step(DT);
  const before = p.obs(), icBefore = p.ic();
  p.bakeScale();
  const after = p.obs();
  gate('G6 first frame after bake == pre-bake readout', p.getScale() === 1 && same(before, after),
       `scale ${p.getScale()}, ${ndiff(before, after)} readout diffs`);
  gate('G6b bake rewrote the initial conditions', ndiff(icBefore, p.ic()) > 0,
       `${ndiff(icBefore, p.ic())}/${icBefore.length} initial-condition values changed`);
}

// ------------------------------------------------ G7/G8 set home / return home
{
  p.load('binary'); p.setScale(1);
  for (let i = 0; i < 500; i++) p.step(DT);
  p.setHomeAll();
  const atSet = p.raw();
  p.step(DT); const frame1 = p.raw();

  for (let i = 0; i < 800; i++) p.step(DT);
  const drifted = p.raw();
  gate('G8 CONTROL the run actually moved (return-home is not vacuous)', !same(atSet, drifted),
       `${ndiff(atSet, drifted)}/${atSet.length} doubles differ after 801 more steps`);

  p.returnAll();
  const returned = p.raw();
  gate('G7a return home restores the state at the set', same(atSet, returned));
  gate('G7c strips primed to the new observables on return (SPEC §6.3)', p.primed());
  p.step(DT); const frame1b = p.raw();
  gate('G7b the frame after the return == the frame after the set', same(frame1, frame1b));
}

// ------------------------------------------------ G9 Reset uses stored ICs
{
  p.load('binary'); p.setScale(1);
  for (let i = 0; i < 400; i++) p.step(DT);
  p.setHomeAll();                       // a configuration the user found by hand
  const home = p.raw();
  for (let i = 0; i < 400; i++) p.step(DT);
  p.resetAll();
  gate('G9 Reset lands on the stored initial conditions, not the preset literal',
       same(home, p.raw()) && p.simTime() === 0, `simTime ${p.simTime()}`);
}

console.log(`\n${fails ? 'RED' : 'GREEN'} — ${checks - fails}/${checks} gates passed  [${basename(file)}]`);
process.exit(fails ? 1 : 0);

```

</details>

## Open questions

1. **Bake fixed point** — the brief says "about the centroid", the acceptance gate forces
   "about the field centre". Implemented as the centre; they coincide under COM lock.
   If the lead wants the centroid, the acceptance criterion needs rewording first.
2. **Drawing clamp** — bodies are drawn unclamped (they leave the view; the sim box is
   outlined) while the observables clamp. Deliberate; see above.
3. **`G`/`ε` clamping on bake** — an extreme bake (e.g. s = 4 from G = 0.025) would push
   `G` past its knob maximum. Clamped, so such a bake is *not* an exact similarity. The
   alternative is widening the knob ranges, which is a spec change.
4. **Presets and the scale** — every built-in loads at scale 1 (`load()` sets it). The lab
   has no preset serialisation, so "presets store the scale" is realised as "the scale is
   patch state that `load()` sets"; a real preset format is the plugin's problem.
5. **Coverage hole for the lead** — `lab_load_check.mjs` sweeps `docs/design` + `src/gui`
   only; adding `reference/` plus four sandbox stubs (`getComputedStyle`,
   `devicePixelRatio`, `matchMedia`, `Option`) makes all 15 reference labs load-checked
   (measured: 10 pass as-is, the other 5 fail only on those four missing globals).
