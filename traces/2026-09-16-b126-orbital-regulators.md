# b126-orbital-regulators — two energy regulators in the ORBITAL lab, both switchable, both off

- **Queue item:** B126 (ORBITAL energy regulators: thermostat + Van der Pol band, both in
  the lab), lead-organ brief of 2026-09-16 on the human's ruling the same day — "we should
  probably test both and see how they feel".
- **Why:** the human asked for "some force or process counteracting drag … a consistent
  mean magnitude over a certain time scale". The lead proposed two regulators and the
  human ratified **testing both**, so this is deliberately two experiments and not one
  feature: each is a single named function called from the one damping site, guarded by
  its own `on` flag, so whichever loses the listening test is deleted by removing one
  function and one call. Both default OFF, and "off = the pre-B126 integrator" is proved
  against the previous file rather than asserted.

## What changed

`reference/gravity-modulator.html` (CANDIDATE lab, ADR-165 — this edit is the human's own
ruling, per the brief):

- **Thermostat** (`thermostat(dt)`) — Berendsen velocity rescaling, one scalar λ per step
  over the free bodies. `K* / K` is evaluated as `(v* / v̄)²` with `v̄` the arithmetic mean
  speed: the human asked for a *mean magnitude*, and an energy target pins the
  mass-weighted RMS instead, which sits above the mean whenever masses or speeds differ.
  Pinned and dragged bodies are excluded from the mean and from the scaling. τ is in
  seconds and enters only as `dt/τ` (ADR-009).
- **Van der Pol band** (`vdpBand(b,dt)`) — the cushion's sign flipped in the interior:
  wall drag `|g|·12·u²` over the cushion's own band, plus, at `g > 0` only, an anti-drag
  core `g·12·c²` inside `r0`. At `g ≤ 0` it is the cushion path bit for bit.
- **Integrator.** Both run at the damping site, after the drag/cushion multiplications and
  **before** the max-speed clamp. The thermostat is field-level, so the old single damping
  loop is split into a damping pass and a clamp/bounds pass with the thermostat between
  them. Nothing in that section couples one body to another, so the split is a no-op —
  gate R1 proves it against the pre-B126 file rather than arguing it.
- **Readouts.** The HUD's energy line already showed the *regulated* K (it is computed from
  post-step velocities) and now carries the free-body mean speed beside it; a field-level
  strip (`#vstrip`) plots `|v̄|` over the last few seconds of **sim** time — the body
  strips' cadence and ring-buffer shape — with the target and its ±5 % band drawn on it,
  so settling is visible rather than inferred.
- **Presets.** `load()` sets both regulators off, the way it sets `P.scale = 1` (B135): the
  lab has no preset serialisation, so that is what "recorded in the preset" means here.

`specs/SPEC-ORBITAL.md`: §3.1 gains the two regulator rows (formulae, units, ranges,
default off, and the measured facts); §4 notes that `energy` is the regulated K and that
the HUD carries `|v̄|`; §5 gains the integrator-placement bullet (damping site, before the
clamp, why the loop is split, ADR-009); §12 gains open question 6 — the choice between the
two is the human's ruling, with the measured trade-off stated.

## Evidence consulted

- The brief's verbatim B126 acceptance criteria and the ROADMAP proposal text quoted in it.
- `reference/gravity-modulator.html` in full; `specs/SPEC-ORBITAL.md` §§3–5, 12.
- Prior trace `traces/2026-09-16-b135-orbital-initial-conditions.md` — the lab's current
  structure (`gu(u)`, the per-body initial conditions, `bakeScale()`), its vm-harness
  approach, and its note that the repo's `lab_load_check.mjs` cannot load this file.
- LIBRARY L0026/L0041 (TDZ — every new binding is a hoisted `function` or is declared above
  its first executed use), L0046 (exact-anchor HTML edits, `count == 1` asserted before
  every write), L0016/L0032/L0033 (a detector needs a known-answer case and a control that
  must fire), L0024 (a result at its threshold means the detector is wrong), L0049 (the
  B135 rule this change mirrors: a physics change belongs in `step()`, a view change does
  not — these two are physics, and are gated as such).

## Alternatives rejected

- **An energy target for the thermostat** (`K* = ½(Σm)v*²`, the literal reading of "K the
  field's kinetic energy"). It fixes the mass-weighted RMS speed, which exceeds the
  arithmetic mean whenever bodies differ, so the knob would say "mean speed" and hold
  something else. Rejected in favour of the same Berendsen λ with `K* / K = (v* / v̄)²`;
  recorded in the spec row and in the code comment.
- **Adding integral action** (a Nosé–Hoover-style friction state) to null the proportional
  offset under drag. It is the textbook cure and it is what "a force counteracting drag"
  literally describes — the friction variable settles to −γ — but it changes the control
  law the human ratified, so it is §12.6's open question, not this change.
- **A separate width knob for the band's wall leg.** The brief specifies three controls and
  says "one bipolar knob on the cushion's own band structure": the band shares `cushW`.
  The consequence is that the limit-cycle size is set by the geometry (`w`, `r0`), not by
  the gain — stated in §12.6 so the listening test knows what it is hearing.
- **Making the band active in wrap mode.** "Distance to the nearest wall" is meaningless on
  a torus; the cushion is already inert there, and sharing that guard is also what keeps
  the −g identity exact in every edge mode.
- **Putting the harness in `tools/labharness/`.** It is specific to one lab and the brief
  scoped it to scratch; embedded below instead, because a scratch file is not durable
  evidence (L0048). `tools/labharness/lab_load_check.mjs` is explicitly out of scope —
  another PR edits it.

## Verify

`./verify fast` — exit 0, git af96339 (`.harness/last-verify.json`; that hash is the branch
point, the tree being uncommitted at run time):

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

`node tools/labharness/lab_load_check.mjs reference/gravity-modulator.html` — **RED, and
pre-existing**: the checker's sandbox has no `getComputedStyle`, which the lab has used at
line 56 since long before this change (B135 trace, open question 5). Proof it is not this
change: the identical run against the untouched pre-B126 copy fails on the same line.

```
FAIL  gravity-modulator.html  script block 1: ReferenceError: getComputedStyle is not defined
        gravity-modulator.html#script1:56 | const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim(); |                                                                             ^

RED — 1 labs loaded, 1 broken, 0 skipped

FAIL  baseline-af96339.html  script block 1: ReferenceError: getComputedStyle is not defined
        baseline-af96339.html#script1:56 | const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim(); |                                                                             ^

RED — 1 labs loaded, 1 broken, 0 skipped
```

B126 harness (`orbital_b126_check.mjs`, source embedded below), run against the edited lab
with the pre-B126 file as the baseline — 18/18. R0's load gate is the same check with the
missing global stubbed.

```
PASS  R0 edited lab loads  — 1 script block(s), no throw  [gravity-modulator.html]
PASS  R0 pre-B126 baseline loads  — 1 script block(s), no throw  [baseline-af96339.html]
PASS  R1 both regulators OFF: bit-identical to the pre-B126 lab  — 20800 doubles over 4 presets x 4 edge/damping scenarios
PASS  R1c CONTROL thermostat ON does change the trajectory  — 1592/1600 doubles differ
PASS  R1d CONTROL Van der Pol band ON does change the trajectory  — 1520/1600 doubles differ
PASS  R2a CALIBRATION thermostat fixed point = 1/sqrt(1+2τγ) with gravity off  — worst 0.55 % off the closed form · γ=0.3,τ=0.05: 0.986 vs 0.985 · γ=0.3,τ=0.2: 0.945 vs 0.945 · γ=0.3,τ=0.5: 0.878 vs 0.877 · γ=0.9,τ=0.05: 0.960 vs 0.958 · γ=0.9,τ=0.2: 0.859 vs 0.857 · γ=0.9,τ=0.5: 0.727 vs 0.725 · γ=3.0,τ=0.05: 0.882 vs 0.877 · γ=3.0,τ=0.2: 0.677 vs 0.674 · γ=3.0,τ=0.5: 0.502 vs 0.500
PASS  R2b mean speed within ±5 % of v* on every preset, drag on  — v* 0.4, τ 0.05s, drag 0.3 · measured/v* over 20 s: binary 0.964 (4τ window 0.967) · sun 0.981 (4τ window 1.301) · figure8 0.958 (4τ window 0.962) · chaos 0.966 (4τ window 1.024)
PASS  R2c the fixed point is the same at dt and dt/2 (τ in seconds, not steps)  — dt 0.3436 vs dt/2 0.3433 (0.08 %)
PASS  R2d thermostat preserves every body-speed ratio  — worst spread 4.44e-16 over 80 isolated thermostat steps
PASS  R2e CONTROL the ratio checker fires on a per-body scale  — one body nudged by 1e-9 -> spread 1.00e-9 (> 1e-12)
PASS  R2f pinned and dragged bodies are not scaled  — pinned (0.900000,-0.400000), dragged (-1.100000,0.700000)
PASS  R2f CONTROL pinned/dragged velocities do not enter the mean  — 0/4 free-body components differ when the excluded bodies are changed
PASS  R3a released FROM REST inside the core: self-sustaining, unclamped  — 60 s: per-second mean floor after 30 s 1.0698, last-10 s 1.1040 vs 30–40 s 1.1044 (-0.0 %), peak |v| 1.878 of MAXV 3
PASS  R3b CONTROL the same release with the band OFF decays  — band off: last-10 s mean 1.78e-9 vs band on 1.1040
PASS  R3c the limit cycle is the same amplitude at dt/2 (rates in seconds)  — dt 1.1040 vs dt/2 1.1050 (0.09 %)
PASS  R3d band at −g is bit-identical to the cushion path  — 1600 doubles, 0 differ
PASS  R3e CONTROL the same band at +g is NOT the cushion (a sign error cannot pass)  — 1536/1600 doubles differ
PASS  R3f CONTROL the cushion band was actually exercised in that scenario  — 1592/1600 doubles differ with the cushion removed

GREEN — 18/18 gates passed  [gravity-modulator.html]
```

Three findings the controls produced, each of which changed the work:

1. **R3f caught a vacuous identity.** The first −g-versus-cushion comparison used
   `cushW = 0.12`, and the `chaos` bodies never entered the band: two trajectories the
   cushion has never touched are trivially equal, so the sign gate would have passed on a
   file with no band in it at all. Widened to 0.3, where the cushion demonstrably fires
   (1592/1600 doubles move when it is removed).
2. **Leaked knob state made a known-answer case read 22 % low.** `load()` resets
   G/ε/bounds/COM and the regulator flags but *not* `damp`/`cushK`/`cushW`, so `cushK = 0.5`
   from an earlier scenario was still on during the closed-form calibration and the
   drifting bodies were sitting in the wall band. Every scenario now starts from the
   documented defaults. Until that was fixed, the "measurement" was of the previous
   experiment.
3. **R3a sat 1.5 % under the max-speed clamp** with the `sun` preset's mass-8 attractor —
   the gate was within touching distance of reading a trajectory the *clamp* was shaping,
   which is a different regulator than the one under test (L0024). Re-run with a mass-3
   attractor: peak 1.88 against a 3.0 clamp, and the limit cycle is flat to −0.0 % over
   30 s.

## Open questions

1. **Criterion 2 is not met as literally written, and the gate says so out loud.** The
   criterion is "the field's mean speed over a window of 4·τ settles within ±5 % of v* from
   any of the built-in presets". Two separate reasons it cannot hold in general, both
   measured, neither a defect in the implementation:
   - **Berendsen is proportional control.** Against a competing loss of rate γ it settles
     at `v̄/v* = 1/sqrt(1 + 2τγ)`, confirmed within 0.5 % across a (γ, τ) grid with gravity
     off (gate R2a). With drag 0.3 that is −4 % at τ = 0.05 s and **−27 % at τ = 0.5 s**,
     so ±5 % holds only at the fast end of the τ knob. Nulling it requires integral action
     — a design change (§12.6).
   - **The 4τ window is tied to the regulator's own bandwidth.** At τ = 0.05 s it is 0.2 s
     of sim, shorter than one close approach in the `sun` preset, so the "mean" over it
     reads a *phase* (1.301) where the 20 s mean reads 0.981. R2b prints both numbers for
     every preset and gates on the long window; it does not silently substitute one for the
     other. **The lead owns whether the criterion's window changes or the regulator does.**
2. **The band's gain and its wall drag are one knob** (per the brief's "one bipolar knob"),
   so the limit-cycle amplitude is set by `cushW` and `r0`, not by `g`. If the listening
   test wants amplitude on a knob, that is a second control and a spec change.
3. **A body at exactly rest with the band on and no attractor never starts** — every
   damping here is multiplicative. R3a uses a pinned attractor, which is what makes the
   "from rest" case move at all. Worth saying in the UI if the band survives.
4. **`lab_load_check.mjs` still does not cover this lab** (open question 5 of the B135
   trace, unchanged and still the lead's): the checker's sandbox lacks `getComputedStyle`,
   `devicePixelRatio`, `matchMedia` and `Option`, and `./verify fast` does not sweep
   `reference/` at all. Out of scope here — another PR edits that file.
5. **Two LIBRARY candidates for the lead** (INDEX/LIBRARY are out of scope for this
   dispatch, so they are reported, not written):
   - A block comment containing `*/` — which is what maths like `K*/K` or `v*/v̄` looks
     like — **terminates the comment**, and the prose after it parses as code. It cost one
     load failure here and is invisible in review; the cheap reflex is `node --check` on
     the extracted script block, which the lab load checker would also catch if it covered
     `reference/`.
   - A harness that drives ONE live instance through many scenarios inherits every knob the
     previous scenario set. Reset to documented defaults per scenario, or the known-answer
     case measures the previous experiment and agrees with nothing.

<details><summary>orbital_b126_check.mjs (the durable copy — the scratch original is ephemeral)</summary>

```javascript
/*
 * orbital_b126_check.mjs — B126 gates for the two ORBITAL energy regulators
 * (thermostat, Van der Pol band) in reference/gravity-modulator.html.
 *
 * Runs the lab's script for real in a vm — the lab_load_check.mjs approach and
 * its stub proxies, plus getComputedStyle, which that checker's sandbox lacks —
 * and drives the simulation through the lab's OWN top-level bindings (global
 * lexical declarations are shared between scripts in one vm context). Modelled
 * on orbital_b135_check.mjs (traces/2026-09-16-b135-orbital-initial-conditions.md).
 *
 * The BASELINE argument is the pre-B126 copy of the same file, so R1 is a real
 * bit-identity gate and not a self-comparison:
 *   git show <pre-B126 rev>:reference/gravity-modulator.html > baseline.html
 *
 * Gates:
 *   R0  load             — both files survive evaluation (L0026 class).
 *   R1  both OFF         — raw trajectory bit-identical to the pre-B126 lab across
 *                          every preset and all three edge modes, vacuum and
 *                          drag+cushion. Criterion 1, and what makes splitting
 *                          step()'s damping loop in two a provable no-op.
 *   R1c/R1d CONTROLS     — with either regulator ON the same comparison DIFFERS,
 *                          so R1 cannot pass vacuously.
 *   R2a CALIBRATION      — with gravity OFF the thermostat's fixed point must equal
 *                          the closed form for Berendsen against a drag of rate
 *                          γ = 3·damp:  v̄/v* = 1/sqrt(1 + 2τγ). A known-answer case
 *                          for the detector AND the proof that the implementation is
 *                          the ratified law rather than something that merely looks
 *                          settled (L0016/L0032).
 *   R2b criterion 2      — drag on, thermostat on, every built-in preset: the mean
 *                          speed within ±5 % of v*. Reported over BOTH the 4τ window
 *                          the criterion names and a 20 s window; see the trace —
 *                          the 4τ window at the fast end of τ is shorter than one
 *                          close approach, so on `sun` it reads a phase, not a mean.
 *   R2c dt independence  — τ is in seconds (ADR-009): the fixed point is the same at
 *                          dt and dt/2.
 *   R2d ratio invariance — |v_i|/|v_j| unchanged by the thermostat step to 1e-12.
 *   R2e CONTROL          — the same checker fires on a deliberately per-body scale.
 *   R2f exclusion        — pinned and dragged bodies are untouched by the scaling
 *                          AND absent from the mean: changing a pinned body's
 *                          velocity leaves every free body's result bit-identical.
 *   R3a VdP limit cycle  — one free body released FROM REST inside the core, drag
 *                          on, band at +g: no decay over 60 s of sim, never reaches
 *                          the max-speed clamp, amplitude settles.
 *   R3b CONTROL          — the identical run with the band OFF decays to ~zero, so
 *                          R3a is the band's doing and not gravity's.
 *   R3c sign control     — at −g the band is BIT-identical to the cushion path;
 *   R3d CONTROL          — at +g it is not (a sign error cannot pass);
 *   R3e CONTROL          — and the cushion was genuinely exercised in that scenario,
 *                          or R3c would be an identity between two untouched runs.
 *                          (It was not, at the first parameters tried — that is why
 *                          the band is wide here.)
 *
 * Usage: node orbital_b126_check.mjs <edited.html> <baseline.html>
 */
import { readFileSync } from 'node:fs';
import { resolve, basename } from 'node:path';
import vm from 'node:vm';

const file = resolve(process.argv[2] || 'reference/gravity-modulator.html');
const base = resolve(process.argv[3] || 'baseline.html');

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

function sandboxFor() {
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
    Float32Array,
  };
  sandbox.globalThis = sandbox; sandbox.self = sandbox; sandbox.window = sandbox;
  return sandbox;
}

let fails = 0, checks = 0;
function gate(name, ok, detail = '') {
  checks++; if (!ok) fails++;
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  — ' + detail : ''}`);
}

// Shared probe surface. `extra` names bindings only the edited file has.
const PROBE = extra => `globalThis.__p = {
  load, step, P, DT(){ return DT; },
  build(specs){ nodes=[]; nodes=specs.map(o=>mk(o)); },
  raw(){ return nodes.flatMap(b=>[b.x,b.y,b.vx,b.vy]); },
  speeds(){ return nodes.map(b=>Math.hypot(b.vx,b.vy)); },
  vel(){ return nodes.map(b=>[b.vx,b.vy]); },
  flag(i,k,v){ nodes[i][k]=v; },
  setVel(i,vx,vy){ nodes[i].vx=vx; nodes[i].vy=vy; },
  ${extra}
};`;

function loadLab(path, extra, label) {
  const html = readFileSync(path, 'utf8');
  const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
  const ctx = vm.createContext(sandboxFor());
  try {
    for (let i = 0; i < blocks.length; i++)
      new vm.Script(blocks[i], { filename: `${basename(path)}#script${i + 1}` })
        .runInContext(ctx, { timeout: 20000 });
  } catch (e) {
    gate(`R0 ${label} loads`, false, `${e.name}: ${e.message}`);
    console.log(`\nRED — ${fails}/${checks} gates failed`); process.exit(1);
  }
  gate(`R0 ${label} loads`, true, `${blocks.length} script block(s), no throw  [${basename(path)}]`);
  vm.runInContext(PROBE(extra), ctx);
  return ctx.__p;
}

const P = loadLab(file, 'thermostat, vdpBand, meanSpeed,', 'edited lab');
const B = loadLab(base, '', 'pre-B126 baseline');
const DT = P.DT();

const same = (a, b) => a.length === b.length && a.every((v, i) => Object.is(v, b[i]));
const ndiff = (a, b) => a.reduce((n, v, i) => n + (Object.is(v, b[i]) ? 0 : 1), 0);
const PRESETS = ['binary', 'sun', 'figure8', 'chaos'];

/* load() sets G/eps/bounds/com and the regulator flags, but NOT damp, cushK or
   cushW — those persist across presets by design, so a scenario that set them
   leaks into every later gate. It did: R2a's closed form read 22 % low until
   this reset existed, because R1's last scenario had left cushK at 0.5 and the
   drifting bodies were sitting in the wall band. Every scenario therefore starts
   from the lab's documented defaults and states what it changes. */
const KNOB_DEFAULTS = { damp: 0, cushK: 0, cushW: 0.1,
                        thermoOn: false, thermoV: 0.4, thermoTau: 0.5,
                        vdpOn: false, vdpG: 0, vdpR0: 0.2 };

/* Run one scenario in one probe. `knobs` is applied AFTER load(). */
function run(p, preset, knobs, steps, every) {
  p.load(preset);
  Object.assign(p.P, KNOB_DEFAULTS, knobs);
  const raw = [];
  for (let i = 0; i < steps; i++) { p.step(DT); if (i % every === 0) raw.push(...p.raw()); }
  return raw;
}

/* Settle for `settle` seconds of sim, then time-average the field's mean speed
   over `window` seconds. dt is a parameter so the same measurement can be made
   at half the step (ADR-009). */
function settledMean(preset, knobs, settle, window, dt = DT) {
  P.load(preset);
  Object.assign(P.P, KNOB_DEFAULTS, knobs);
  for (let i = 0, n = Math.round(settle / dt); i < n; i++) P.step(dt);
  let s = 0; const n = Math.round(window / dt);
  for (let i = 0; i < n; i++) { P.step(dt); s += P.meanSpeed(); }
  return s / n;
}

/* ------------------------------------------------------- R1 both regulators off */
{
  const KNOBS = [
    ['vacuum', {}],
    ['drag+cushion', { damp: 0.3, cushK: 0.45, cushW: 0.12 }],
    ['wrap edges', { bounds: 'wrap', damp: 0.2 }],
    ['open edges', { bounds: 'open', damp: 0.1, cushK: 0.5 }],
  ];
  let total = 0, bad = 0, worst = '';
  for (const preset of PRESETS) for (const [kn, knobs] of KNOBS) {
    const a = run(P, preset, knobs, 3000, 30), b = run(B, preset, knobs, 3000, 30);
    total += a.length;
    const d = ndiff(a, b);
    if (d) { bad += d; worst = `${preset}/${kn}`; }
  }
  gate('R1 both regulators OFF: bit-identical to the pre-B126 lab', bad === 0,
       `${total} doubles over 4 presets x 4 edge/damping scenarios${bad ? `, ${bad} differ, first at ${worst}` : ''}`);

  // CONTROLS: the comparison must be able to fail.
  const th = run(P, 'chaos', { damp: 0.3, thermoOn: true, thermoV: 0.4, thermoTau: 0.5 }, 3000, 30);
  const th0 = run(B, 'chaos', { damp: 0.3 }, 3000, 30);
  gate('R1c CONTROL thermostat ON does change the trajectory', !same(th, th0),
       `${ndiff(th, th0)}/${th.length} doubles differ`);
  const vd = run(P, 'chaos', { damp: 0.3, vdpOn: true, vdpG: 0.5, vdpR0: 0.2 }, 3000, 30);
  gate('R1d CONTROL Van der Pol band ON does change the trajectory', !same(vd, th0),
       `${ndiff(vd, th0)}/${vd.length} doubles differ`);
}

/* ------------------------------------------- R2a the thermostat's fixed point */
{
  /* KNOWN-ANSWER CASE. Switch gravity off and the field is pure first-order
     control: a drag of rate γ = 3·damp against a Berendsen thermostat of time
     constant τ. Berendsen is PROPORTIONAL control with finite gain, so it does
     not null a constant loss — it settles at
         v̄/v* = 1/sqrt(1 + 2τγ)
     which is closed form, and is the whole reason criterion 2 behaves the way
     R2b reports. If this drifts, the law in the lab is no longer the ratified
     one, whatever the field looks like. */
  const VSTAR = 0.4;
  let worst = 0; const rows = [];
  for (const damp of [0.1, 0.3, 1.0]) for (const tau of [0.05, 0.2, 0.5]) {
    const m = settledMean('binary', { G: 0, com: false, damp, thermoOn: true, thermoV: VSTAR, thermoTau: tau },
                          20 * tau, 4 * tau) / VSTAR;
    const pred = 1 / Math.sqrt(1 + 2 * tau * 3 * damp);
    worst = Math.max(worst, Math.abs(m / pred - 1));
    rows.push(`γ=${(3 * damp).toFixed(1)},τ=${tau}: ${m.toFixed(3)} vs ${pred.toFixed(3)}`);
  }
  gate('R2a CALIBRATION thermostat fixed point = 1/sqrt(1+2τγ) with gravity off', worst < 0.01,
       `worst ${(worst * 100).toFixed(2)} % off the closed form · ` + rows.join(' · '));
}

/* --------------------------------------------- R2b criterion 2: ±5 % of v*  */
{
  /* Criterion 2, measured at the fast end of the τ knob (0.05 s), where the
     closed form above puts the offset at 4 % for γ = 0.9 rather than the 27 %
     it reaches at τ = 0.5 s. Both windows are printed: the 4τ window the
     criterion names, and a 20 s one. They disagree on `sun` by 30 %, because
     4τ = 0.2 s is shorter than one close approach there, so the "mean" over it
     is a phase of the orbit rather than a mean — the window is tied to τ, the
     very parameter that sets the regulator's bandwidth. The gate is the long
     window; the short one is printed so the disagreement is visible instead of
     chosen silently. See the trace: the lead owns this. */
  const VSTAR = 0.4, TAU = 0.05, KNOBS = { damp: 0.3, thermoOn: true, thermoV: VSTAR, thermoTau: TAU };
  let worst = 0; const rows = [];
  for (const preset of PRESETS) {
    const long = settledMean(preset, KNOBS, 4 * TAU, 20) / VSTAR;
    const short = settledMean(preset, KNOBS, 4 * TAU, 4 * TAU) / VSTAR;
    worst = Math.max(worst, Math.abs(long - 1));
    rows.push(`${preset} ${long.toFixed(3)} (4τ window ${short.toFixed(3)})`);
  }
  gate('R2b mean speed within ±5 % of v* on every preset, drag on', worst < 0.05,
       `v* ${VSTAR}, τ ${TAU}s, drag 0.3 · measured/v* over 20 s: ` + rows.join(' · '));
}

/* ------------------------------------------------- R2c τ is in seconds (ADR-009) */
{
  const K = { G: 0, com: false, damp: 0.3, thermoOn: true, thermoV: 0.4, thermoTau: 0.2 };
  const a = settledMean('binary', K, 4, 1, DT), b = settledMean('binary', K, 4, 1, DT / 2);
  gate('R2c the fixed point is the same at dt and dt/2 (τ in seconds, not steps)',
       Math.abs(a / b - 1) < 0.01, `dt ${a.toFixed(4)} vs dt/2 ${b.toFixed(4)} (${((a / b - 1) * 100).toFixed(2)} %)`);
}

/* --------------------------------------- R2d/R2e the thermostat preserves ratios */
{
  /* The invariant: one scalar λ for every free body, so every pairwise speed
     ratio survives the step. Measured as the spread of the per-body scale
     factors, which is the pairwise ratio error to first order and needs no
     N² loop. */
  const spread = (before, after) => {
    const f = after.map((v, i) => v / before[i]).filter(Number.isFinite);
    return Math.max(...f) / Math.min(...f) - 1;
  };
  P.load('chaos');
  Object.assign(P.P, KNOB_DEFAULTS, { damp: 0.3, thermoV: 0.4, thermoTau: 0.5 });
  let worst = 0, n = 0;
  for (let i = 0; i < 4000; i++) {
    P.step(DT);
    if (i % 50) continue;
    const before = P.speeds();
    P.thermostat(DT);                   // the regulator step ALONE, not a whole step()
    worst = Math.max(worst, spread(before, P.speeds())); n++;
  }
  gate('R2d thermostat preserves every body-speed ratio', worst < 1e-12,
       `worst spread ${worst.toExponential(2)} over ${n} isolated thermostat steps`);

  // CONTROL: the same checker on a deliberately non-uniform scale must fire.
  const before = P.speeds();
  P.setVel(0, P.vel()[0][0] * (1 + 1e-9), P.vel()[0][1] * (1 + 1e-9));
  const s = spread(before, P.speeds());
  gate('R2e CONTROL the ratio checker fires on a per-body scale', s > 1e-12,
       `one body nudged by 1e-9 -> spread ${s.toExponential(2)} (> 1e-12)`);
}

/* ------------------------------- R2f pinned / dragged excluded from K and scaling */
{
  const field = () => {
    P.load('chaos');
    Object.assign(P.P, KNOB_DEFAULTS, { thermoOn: true, thermoV: 0.4, thermoTau: 0.5 });
    for (let i = 0; i < 300; i++) P.step(DT);
    P.flag(0, 'pinned', true); P.flag(1, 'drag', true);
    P.setVel(0, 0.9, -0.4); P.setVel(1, -1.1, 0.7);
    return P.vel();
  };
  const v0 = field();
  P.thermostat(DT);
  const after = P.vel();
  gate('R2f pinned and dragged bodies are not scaled',
       same(v0[0], after[0]) && same(v0[1], after[1]),
       `pinned (${after[0].map(v => v.toFixed(6))}), dragged (${after[1].map(v => v.toFixed(6))})`);

  // ...and they are absent from the mean: give them different velocities and
  // every FREE body must land on bit-identical numbers.
  const freeA = after.slice(2).flat();
  field();
  P.setVel(0, -2.2, 0.05); P.setVel(1, 0.3, 2.4);     // wildly different, same free state
  P.thermostat(DT);
  const freeB = P.vel().slice(2).flat();
  gate('R2f CONTROL pinned/dragged velocities do not enter the mean', same(freeA, freeB),
       `${ndiff(freeA, freeB)}/${freeA.length} free-body components differ when the excluded bodies are changed`);
}

/* ---------------------------------------------- R3a/R3b Van der Pol limit cycle */
{
  /* Sun mass 3, not the `sun` preset's 8. At m = 8 the dive through the softened
     well peaks at 2.96 of a 3.0 clamp — the gate would then be reading a
     trajectory the CLAMP was shaping, which is a different regulator than the
     one under test, and 1.5 % of margin is not a margin (L0024). */
  const SUN = { name: 'Sun', m: 3, x: 0.5, y: 0.5, pinned: true, tx: 'none', ty: 'none' };
  const BODY = { name: 'One', m: 0.4, x: 0.62, y: 0.5, vx: 0, vy: 0, tx: 'none', ty: 'none' };  // at rest, r = 0.12 < r0
  const MAXV = 3.0;
  function release(bandOn, dt = DT) {
    P.load('sun');                                   // sets G/eps/bounds/com; bodies replaced below
    Object.assign(P.P, KNOB_DEFAULTS, { damp: 0.25, vdpOn: bandOn, vdpG: 0.5, vdpR0: 0.2 });
    P.build([SUN, BODY]);
    const win = [];                                  // mean speed per second of sim
    let peak = 0, n = Math.round(1 / dt);
    for (let s = 0; s < 60; s++) {
      let sum = 0;
      for (let i = 0; i < n; i++) { P.step(dt); const v = P.speeds()[1]; sum += v; if (v > peak) peak = v; }
      win.push(sum / n);
    }
    return { win, peak };
  }
  const mean = (a, i, j) => a.slice(i, j).reduce((x, y) => x + y, 0) / (j - i);
  const on = release(true), off = release(false);
  const last10 = mean(on.win, 50, 60), mid10 = mean(on.win, 30, 40);
  const floor = Math.min(...on.win.slice(30));
  gate('R3a released FROM REST inside the core: self-sustaining, unclamped',
       floor > 0.02 && Math.abs(last10 / mid10 - 1) < 0.25 && on.peak < MAXV,
       `60 s: per-second mean floor after 30 s ${floor.toFixed(4)}, last-10 s ${last10.toFixed(4)} vs 30–40 s ${mid10.toFixed(4)} ` +
       `(${((last10 / mid10 - 1) * 100).toFixed(1)} %), peak |v| ${on.peak.toFixed(3)} of MAXV ${MAXV}`);
  const offLast = mean(off.win, 50, 60);
  gate('R3b CONTROL the same release with the band OFF decays', offLast < last10 * 0.1,
       `band off: last-10 s mean ${offLast.toExponential(2)} vs band on ${last10.toFixed(4)}`);

  // The band's rates are per second too (ADR-009): the attractor is the same at dt/2.
  const half = release(true, DT / 2);
  const halfLast = mean(half.win, 50, 60);
  gate('R3c the limit cycle is the same amplitude at dt/2 (rates in seconds)',
       Math.abs(halfLast / last10 - 1) < 0.05,
       `dt ${last10.toFixed(4)} vs dt/2 ${halfLast.toFixed(4)} (${((halfLast / last10 - 1) * 100).toFixed(2)} %)`);
}

/* ------------------------- R3d/R3e/R3f negative gain IS the cushion, bit for bit */
{
  /* cushW is 0.3 here on purpose. At 0.12 the `chaos` bodies never entered the
     band, so the −g comparison was an identity between two runs the cushion had
     never touched — R3f caught exactly that, which is what a must-differ control
     is for. */
  const K = 0.37, W = 0.3, STEPS = 4000, EVERY = 40;
  const cushion = run(P, 'chaos', { damp: 0.2, cushK: K, cushW: W, vdpOn: false }, STEPS, EVERY);
  const negBand = run(P, 'chaos', { damp: 0.2, cushK: 0, cushW: W, vdpOn: true, vdpG: -K, vdpR0: 0.2 }, STEPS, EVERY);
  gate('R3d band at −g is bit-identical to the cushion path', same(cushion, negBand),
       `${cushion.length} doubles, ${ndiff(cushion, negBand)} differ`);

  const posBand = run(P, 'chaos', { damp: 0.2, cushK: 0, cushW: W, vdpOn: true, vdpG: +K, vdpR0: 0.2 }, STEPS, EVERY);
  gate('R3e CONTROL the same band at +g is NOT the cushion (a sign error cannot pass)',
       !same(cushion, posBand), `${ndiff(cushion, posBand)}/${cushion.length} doubles differ`);

  const noCushion = run(P, 'chaos', { damp: 0.2, cushK: 0, cushW: W, vdpOn: false }, STEPS, EVERY);
  gate('R3f CONTROL the cushion band was actually exercised in that scenario',
       !same(cushion, noCushion), `${ndiff(cushion, noCushion)}/${cushion.length} doubles differ with the cushion removed`);
}

console.log(`\n${fails ? 'RED' : 'GREEN'} — ${checks - fails}/${checks} gates passed  [${basename(file)}]`);
process.exit(fails ? 1 : 0);
```

</details>
