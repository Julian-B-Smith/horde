# b126-orbital-render-wins — the four free render wins, with the simulation held bit-identical

- **Queue item:** B126 (ORBITAL, `reference/gravity-modulator.html`, ADR-165 CANDIDATE).
  The ratified half of `traces/2026-09-16-b126-orbital-profile.md` §7: **A1** cache the CSS
  custom-property lookups, **A2** batch the trail strokes into alpha buckets, **A3** square
  the max-speed comparison, **B4** redraw the per-body strips lazily. **B2 and B3 are NOT
  in this change set** — both move the trajectory and the human's ruling excluded them.
- **Why:** the profile measured the lab's render path at **13,533 canvas calls and 65
  forced style resolutions per frame at N = 8** (§4) against a physics step costing 0.043 %
  of one core (§1). The cost was in implementation choices, not in controls, so nothing the
  user can reach is removed here; the four changes are the ones the profile priced at zero
  user cost. The human's ruling (2026-09-17, verbatim): *"Your recommendations are ratified
  on Orbital decisions."*

## Headline

| | before (origin/main) | after (this change) |
|---|---|---|
| canvas ops per frame, N = 8, 60 fps, all rows on screen | **13,533** | **6,781** (−49.9 %) |
| of which the trails | **8,776** | **2,024** (−77.0 %) |
| `getComputedStyle()` per frame, steady state | **65** | **0** |
| `getComputedStyle()` for the life of the page | 65 × every frame (402 over load + 6 frames) | **10 — one per distinct custom property, ever** |
| raw `x,y,vx,vy` doubles differing over 48 configurations × 4,000 steps | — | **0 of 62,400** |

`getComputedStyle` is 0 in **steady state**; the **first** frame after load resolves the 4
palette properties the node list did not already need (`--sky-deep`, `--line`, `--ink-dim`,
`--ink`), and never again. The lifetime total of 10 is the whole set:
`--ink --ink-dim --line --node-a..--node-f --sky-deep`.

## 1. Bit-identity — 0 of 62,400 doubles, and the clamp made to fire

`orbital_bitident.mjs` (§Harness) loads `git show origin/main:reference/gravity-modulator.html`
and this branch's file into two separate `vm` contexts and drives them in lockstep through
their own top-level bindings, comparing raw `x, y, vx, vy` with `Object.is` (so −0 and NaN
are not laundered by `==`). Matrix: **4 presets × {thermostat off, on} × {band off, g > 0,
g < 0} × {seeded, bodies thrown above MAXV} = 48 configurations**, 4,000 steps each, sampled
every 40.

```
GREEN  0/62400 doubles differ over 48 configurations; 2290 clamp engagements exercised;
       negative control fires on 4/4 presets;
       clamp ceiling 6.4031242374328485 -> 2.207985116136415 (MAXV 3), pre-lab lands on the same double: true;
       clamp branch not instrumented (tracked labs)
```

**A3 is exact by construction only for the branch it does not take, so the branch it DOES
take is proved separately.** The rewrite is

```js
if(b.vx*b.vx+b.vy*b.vy>MAXV*MAXV){const s=Math.hypot(b.vx,b.vy); b.vx*=MAXV/s;b.vy*=MAXV/s;}
```

`Math.hypot` is **kept for the scaling** and only the *comparison* is squared. Substituting
`Math.sqrt(vx*vx+vy*vy)` for the root — the cheaper move, and the one control E in the
profile priced — would change the scale factor in the last ulp *whenever the clamp fires*,
which is precisely the case the brief asked to exercise. It is not done.

To prove the taken branch rather than assume it, scratch copies of **both** labs carry a
counter inside the clamp's own branch (`make_clampcount.py`, every anchor asserted to match
exactly once):

```
GREEN  0/62400 doubles differ over 48 configurations; 2290 clamp engagements exercised;
       negative control fires on 4/4 presets;
       clamp ceiling 6.4031242374328485 -> 2.207985116136415 (MAXV 3), pre-lab lands on the same double: true;
       clamp BRANCH taken pre=10269 post=10269, mismatched configs 0,
       taken on seeded regulators-off runs 0 (must be 0), on seeded regulated runs 42
```

**10,269 engagements, identical config by config**, and the trajectories still agree to the
double. The control has both halves (L0032): it must read **zero** where the profile's
control C says the clamp never engages, and non-zero where the bodies are thrown.

### A finding that extends the profile's control C boundary

The must-read-zero half **did not read zero on the first attempt**, and the reason is a
result rather than a harness bug. Recorded rather than quietly re-scoped (L0033):

```
sun | thermo off | band g>0 | clamp branch taken pre/post 42 42 | doubles differing 0
```

**The max-speed clamp DOES engage in seeded play, with no user interaction, on the `sun`
preset with the Van der Pol band at g > 0** — 42 times in 4,000 steps. The profile's control C
("the max-speed clamp never engages, peak 1.61 against MAXV 3.0") was measured with both
regulators off, and its stated boundary was *"this seeded configuration, no user
interaction"*. The boundary is narrower than that: it is **regulators off**. The band's core
leg pumps a body past the clamp. This strengthens the profile's own recommendation — the
clamp is load-bearing, not vestigial, and deleting it was correctly not recommended. The
must-read-zero assertion in the harness is therefore scoped to regulators-off seeded runs,
with the regulated count reported beside it rather than folded away.

### Negative control (the comparator can fire)

Post lab against itself with a single 1e-12 velocity kick, same comparator, all four presets:
`differ > 0` on 4/4. Without it, "0 of 62,400" would be a statement about the comparator.

### The two existing harnesses, unchanged

Both are re-extracted verbatim from their traces and run against this file.

```
=== B135 (traces/2026-09-16-b135-orbital-initial-conditions.md) ===
GREEN — 13/13 gates passed  [gravity-modulator.html]

=== B126 regulators (traces/2026-09-16-b126-orbital-regulators.md), vs the pre-B126 lab 2876005 ===
GREEN — 18/18 gates passed  [gravity-modulator.html]
```

Both produce output **line for line identical** to the same harnesses run against
`origin/main`'s lab, including every numeric detail string (`R1` 20,800 doubles over 4
presets × 4 scenarios; `R2a` worst 0.55 % off the closed form; `R3a` peak |v| 1.878 of
MAXV 3). Diffed, not eyeballed.

## 2. Canvas operation counts

`profile_draw2.mjs` is the profile's §4 counting harness with three extensions, each forced
by a ratified change the original could not see: it counts a **whole frame**
(`draw()` + `updateReadouts()`) rather than `draw()` plus one strip × N, because B4 makes
the strip count a variable; it counts a **steady-state** frame as well as a cold one,
because A1 makes the first frame unrepresentative by design; and it **advances the sim
between frames**, because B4's cadence gate is only visible when frames and strip samples
are not 1:1.

**The extension is calibrated against the number it replaces:** on `origin/main`'s lab it
reports 13,533 ops / 65 css at N = 8, 4,757 / 57 with trails off, and 13,533 with polar
strips — every figure in the profile's §4 table, reproduced through a different code path.

| scenario (N = 8) | ops before | ops after | Δ | field | readouts | `getComputedStyle` |
|---|---|---|---|---|---|---|
| 60 fps, ts = 1, 8 rows visible | 13533 | **6781** | −49.9 % | 9237 → 2485 | 4296 → 4296 | 65 → 0 |
| 120 Hz (4 steps/frame), 8 visible | 13533 | **5165** | −61.8 % | 9237 → 2485 | 4296 → 2680 | 65 → 0 |
| ts = 0.25 (2 steps/frame), 8 visible | 13533 | **3549** | −73.8 % | 9237 → 2485 | 4296 → 1064 | 65 → 0 |
| paused | 13565 | **2741** | −79.8 % | 9237 → 2485 | 4328 → 256 | 65 → 0 |
| panel scrolled, 2 of 8 rows visible | 13533 | **3751** | −72.3 % | 9237 → 2485 | 4296 → 1266 | 65 → 0 |
| panel scrolled, 0 of 8 rows visible | 13533 | **2741** | −79.7 % | 9237 → 2485 | 4296 → 256 | 65 → 0 |
| trails OFF, 8 visible | 4757 | **4757** | 0.0 % | 461 → 461 | 4296 → 4296 | 57 → 0 |
| polar strips, 8 visible | 13533 | **6781** | −49.9 % | 9237 → 2485 | 4296 → 4296 | 65 → 0 |

Two rows are controls in their own right. **Trails off is unchanged at 4,757**: A2 touches
only the trail loop, so a configuration that never enters it must not move, and does not.
**Paused drops the readouts to 256**, which is `drawVbar` alone — 0 of 8 strips redrawn,
the strongest form of B4's claim; the pre lab draws all 8 while nothing changes.

At 60 fps with ts = 1 the strips are **not** cheaper (4,296 → 4,296), and that is correct
rather than a miss: one strip sample is recorded per 1/60 s of sim, so at 60 fps with time
scale 1 every frame genuinely has new content. B4 pays on a 120 Hz display, at time scale
below 1, while paused, and when the panel is scrolled — the cases where the old code redrew
identical pixels.

### Where the trail saving comes from

Steady-state field-canvas ops, per op kind:

| op | before | after |
|---|---|---|
| `beginPath` | 1812 | **124** |
| `moveTo` | 1812 | **124** |
| `lineTo` | 1812 | **1812** |
| `stroke` | 1796 | **108** |
| `set:globalAlpha` | 1816 | **128** |

**`lineTo` is unchanged, and that is the point.** It is the geometry — 1,752 trail segments
plus 60 grid/force/tick segments — and the geometry is not what was being paid for. What
was being paid for is that a per-segment `globalAlpha` cannot share a path, so each of the
219 segments per body carried a full `beginPath`/`moveTo`/`lineTo`/`stroke`. With the fade
quantised to 8 buckets, each bucket is one path and one stroke: 8 bodies × 8 buckets ≈ 124
paths instead of 1,752.

## 3. Nothing the user sees changes

`trail_pixels.mjs` records the **ordered argument stream**, not just counts, and
reconstructs every stroked field segment as `(x1,y1)→(x2,y2)` at the `globalAlpha` in effect
at stroke time — the model both labs fit, since the old code set alpha before `beginPath`
and the new one before `stroke`, and a canvas resolves it at the stroke.

```
GREEN  1812 stroked field segments; geometry identical: true;
       1752 differ in alpha only, max |dalpha| 0.034375 (half a bucket = 0.034375);
       CONTROL halved ramp max |dalpha| 0.273750 — must exceed half a bucket
```

- **The drawn geometry is bit-identical** — same segments, same order, same endpoints, same
  wrap seams lifted. The trail is in the same place with the same length.
- **The only difference is alpha, bounded by half a bucket: 0.55/(2·8) = 0.034375**, on a
  1.2 px line over a `--sky-deep` field. The bucket's alpha is its **midpoint**, so the ramp
  keeps its mean and not merely its ends, and the bucket boundaries slide along the trail
  every frame as samples shift — there is no fixed seam to find.
- **The control fires:** the same comparator against a lab whose fade ramp is halved reports
  0.2738, eight times the bound. "Within half a bucket" is a claim about the labs, not about
  the comparator.
- SPEC-ORBITAL §9 (`specs/SPEC-ORBITAL.md:150`) specifies *"Trails (last ~220 steps, alpha
  ramp)"*. The ramp survives quantisation; the spec needs no change, and none is made.

**Strips, readouts and controls are untouched**: the per-op table above shows `strip.*` and
`vstrip.*` identical in every kind, and a redrawn strip runs the same `drawXYGraph` it always
did. B4 changes *when* it runs, never *what* it draws.

**Theme switching: there is nothing to wire, and it is stated rather than assumed.** The lab
has one `:root` block (`reference/gravity-modulator.html:8`), no `data-theme`, no
`matchMedia`, no `prefers-color-scheme`, no class toggle — `grep -n 'theme|prefers-color-scheme'`
returns only the `:root` line. The palette cannot change at runtime, which is why caching it
is sound. The obligation a future theme switch inherits is named at the cache itself:
`refreshPalette()` is defined next to `css()` and the comment says the cache is the only
thing that would not follow a theme change.

## 4. One hazard found and closed while implementing

The first cut keyed the lazy strip redraw on `b.hp`, the strip ring's write pointer — it
advances on every sample, so it looked like a free dirty flag. It is not: **`primeStrips`
resets `hp` to 0**, so a Reset or Return-home that landed while `hp` was already 0 would
leave the strip showing the old curve with no way for the gate to notice. Replaced with an
explicit `stripRev` counter bumped at **both** ring-write sites (the sampling tick in
`frame()` and `primeStrips`), which cannot alias. The op counts above are unchanged by the
swap — measured before and after, identical in all eight scenarios — so the fix cost
nothing.

The harness had to learn the same thing: `profile_draw2.mjs` re-implements the frame body,
and without bumping `stripRev` there it would have measured **zero** strip redraws — a lie
in our favour. The bump is guarded (`typeof stripRev!=='undefined'`) because the pre lab has
no such binding.

## Evidence consulted

- `traces/2026-09-16-b126-orbital-profile.md` — §4 (the 13,533/65 counts and the op
  breakdown), §5 (control C, the clamp that never fired), §7 A1/A2/A3/B4 and the B2/B3
  exclusions, and the §Harness copies of `profile_draw.mjs` / `drive_draw.mjs`.
- `traces/2026-09-16-b135-orbital-initial-conditions.md` — `orbital_b135_check.mjs`,
  re-extracted and run.
- `traces/2026-09-16-b126-orbital-regulators.md` — `orbital_b126_check.mjs`, re-extracted and
  run against `git show 2876005:` as its baseline.
- `reference/gravity-modulator.html` at `origin/main`
  (sha256 `8352db2c8c87ac2654b167ade3f70328b95992762fb2e47f8177c7fae011f2bd`, the same hash
  the profile recorded): `css` 211, `MAXV` 222, the clamp 411, `draw` 483, the trail loop
  514, `frame` 553 / `HIST_EVERY` 551, `renderNodeList` 722, `drawXYGraph` 776,
  `updateReadouts` 830, `#panel` 35 (the scroll container), `:root` 8.
- `specs/SPEC-ORBITAL.md` §9 line 150 (trails, alpha ramp), line 46 (max speed 3.0, "safety
  clamp, not user-facing"), §12.6 / line 43 (the band's core leg).
- LIBRARY L0016 (calibrate the detector on a known-clean signal), L0032 (a control that must
  read zero, paired with a corruption that must read non-zero; assert every mutation's
  anchor), L0033 (a plant that does not fire is a finding), L0049 (a function downstream of
  the state makes bit-identity the gate), L0052 (a correctness sandbox is not a timing
  harness — nothing here is timed).

## Alternatives rejected

- **`Math.sqrt(vx*vx+vy*vy)` for the clamp's root as well as its comparison.** Cheaper, and
  the profile's control E priced the same substitution at 66.2 ns elsewhere — but `hypot`
  and `sqrt` of squares are not the same double, so a clamp that *fires* would scale by a
  different factor. The brief's own wording ("take the root only when it fires") is the
  version implemented.
- **One path per body for the trails, with a single alpha.** The largest saving available
  and the one the profile's "~27×" figure implies, but the fade is what makes the trail
  readable as a direction. Eight buckets keep the ramp at a 0.034 step.
- **`getBoundingClientRect()` for row visibility.** A forced layout per row per frame — the
  same class of cost A1 just removed from the palette, arriving through a different door.
  `IntersectionObserver` costs nothing per frame.
- **`IntersectionObserver` with `root: $('panel')`.** The panel is the scroll container at
  desktop width, but below 820 px the body scrolls (`reference/gravity-modulator.html:25`).
  `root: null` is correct at both, because intersection against the viewport already applies
  ancestor overflow clips.
- **Resolving the palette into a plain object at load.** A `Map` memo is the same thing with
  no initialisation-order question, and it keeps `css(v)` as the single call site.
- **Putting any of these harnesses in `tools/labharness/`.** Same call, same reason, as B135
  and the profile: they know ORBITAL's `nodes`/`P`/`step`/`renderNodeList` by name. Embedded
  below instead (L0048).
- **Timing anything.** L0052 — and there is no rasteriser here regardless. Counts only; the
  browser confirmation of A1/A2 remains the lead's, as the profile's open question 1 says.

## Verify

`./verify fast` — **exit 0**, tree `6923b93` (`.harness/last-verify.json`:
`{"target":"fast","exit":0,"git":"6923b93","ts":"2026-09-17T10:56:03Z"}`).

**Two runs, and the first did not cover this change set.** The output below was first taken
at tree `aaab057` (`{"target":"fast","exit":0,"git":"aaab057","ts":"2026-09-17T10:51:40Z"}`)
— the lab edit was in the working tree but nothing was committed, so the recorded hash was
the *parent*, not the tree that was checked. Re-run against the committed change set at
`6923b93`: identical output, exit 0. Both hashes are recorded rather than the first one
quietly swapped, because which tree an oracle ran against is the whole content of the claim.
Then green a third time, byte-identical output, at `d1c122a`
(`{"target":"fast","exit":0,"git":"d1c122a","ts":"2026-09-17T10:57:00Z"}`), which is
`6923b93` plus this paragraph. The final commit hash necessarily post-dates the last
recorded run — a trace that records its own verify can never name its own commit — and the
only delta from `d1c122a` is these six lines of this file, which no gate reads.

```
verify: .leakcheck-names absent — private-name leak check SKIPPED (expected off this Mac)
mailbox_delivery: no sibling mailbox checked out — SKIPPED
presentation_check: GREEN (325 rows, scopes: global, osc1, osc2; 34 undesigned (no chunk named), 5 ungrouped)
  note  engine guards law==0 but no parameter declares it
  note  engine guards law==1 but no parameter declares it
depends_check: GREEN (121 declared dependencies, header current, 2 advisory)
gen_gui_controls: GREEN (197 generated control(s), gui2 markup current)
test_table_check: GREEN (189 tests — 104 agentic, 85 human; 14 awaiting an oracle)
  gui.html     reaches 102 / 243 params
  gui2.html    reaches 224 / 243 params
  exempt: inertiaCurve — dev-only, labelled (dev) in the param table, fxXfade — buried by ruling B117 (ADR-163 A2); id kept for state, fxXfadeMs — buried with 264; 80 ms is the behaviour
  patch-scope params (raw-id dispatch, must be data-fixed): 89
gui_reach: GREEN (every declared param is reachable in some GUI)
```

`node tools/labharness/lab_load_check.mjs reference/gravity-modulator.html`:

```
OK    gravity-modulator.html

GREEN — 1 labs loaded, 0 broken, 0 skipped
```

## Open questions

1. **Real frame time is still unmeasured**, exactly as the profile's open question 1 left it.
   Everything above is an operation count. A 50 % drop in canvas calls and 3,900 → 0 forced
   style resolutions per second is a strong proxy, and `getComputedStyle` in particular may
   be cheaper than feared if the browser caches within a frame — the browser measurement is
   the lead's, and neither A1 nor A2 should be called a *win* until it exists. What is
   established here is that they cost the user nothing.
2. **The trail fade quantisation is argued, not observed.** 0.034 of alpha on a 1.2 px line
   is below what a display resolves, and the geometry is proved identical — but nobody has
   looked at the two trails side by side. A screenshot pair at N = 8 would close it in
   seconds and has not been taken.
3. **B4's saving is zero in the most common configuration** (60 fps, time scale 1, panel not
   scrolled). It pays on 120 Hz displays, at time scale below 1, while paused, and when
   scrolled. Whether that is worth the `IntersectionObserver` and the revision counter is a
   judgement the numbers above support but do not settle; the lead may prefer to revert B4
   alone, and it is independent of A1/A2/A3.
4. **Control C's boundary is now narrower than the profile stated** (§1 above): the clamp
   engages 42 times in 4,000 seeded steps on `sun` with the band at g > 0. Nothing depends
   on it here — the clamp stays, and A3 is proved on the firing branch — but the profile's
   §5 sentence *"an unconditional `Math.hypot` per body per step guarding a threshold nothing
   reaches"* is true only with the regulators off, and a future reader should not inherit the
   stronger claim. The profile is append-only; this is the correcting entry.
5. **`refreshPalette()` has no caller.** It is defined because a theme switch would otherwise
   fail silently and invisibly, and the lab has no theme switch to call it from. If the lead
   would rather have no unreferenced function than a named obligation, deleting it and
   keeping the comment is a one-line change.

## Harness (the durable copies — the scratch originals are ephemeral, L0048)

Run order: `orbital_bitident.mjs <pre> <post>` for the identity; `make_clampcount.py` then
the same comparator on its output for the clamp-branch control; `sweep.mjs` (which shells
out to `profile_draw2.mjs`) for the counts; `trail_pixels.mjs` for the drawn geometry.
`<pre>` is `git show origin/main:reference/gravity-modulator.html`.

<details><summary>orbital_bitident.mjs — 48 configurations, the fired clamp, and two controls</summary>

```javascript
/*
 * orbital_bitident.mjs — is the ORBITAL simulation bit-identical across the
 * B126 render wins (A1 css cache, A2 trail batching, A3 squared max-speed
 * compare, B4 lazy strips)?
 *
 * Two lab files are loaded into two SEPARATE vm contexts and driven in
 * lockstep through their own top-level bindings (the B135 harness's approach,
 * traces/2026-09-16-b135-orbital-initial-conditions.md). Only raw state is
 * compared — x, y, vx, vy as doubles, Object.is, so -0 and NaN are not
 * laundered by ==.
 *
 * A vm CONTEXT is right for correctness and wrong for timing (LIBRARY L0052:
 * global lexical reads cost ~56x through the context interceptor). Nothing
 * here is timed.
 *
 * Matrix: every built-in preset x {thermostat off/on} x {band off, g>0, g<0},
 * 4000 steps, sampled every 40.
 *
 * Scenarios beyond the matrix:
 *   CLAMP  bodies are thrown ABOVE MAXV on a fixed schedule so the max-speed
 *          clamp DOES fire — the profile's control C found it never fires in
 *          seeded play, so an unexercised clamp is a vacuous proof (L0033).
 *   NEG    the same comparator against a 1-ULP-kicked copy of the POST lab,
 *          which must fire on every configuration, or "0 differ" means the
 *          comparator is broken rather than the labs agreeing.
 *
 * Usage: node orbital_bitident.mjs <pre.html> <post.html>
 */
import { readFileSync } from 'node:fs';
import { resolve, basename } from 'node:path';
import vm from 'node:vm';

const PRE = resolve(process.argv[2]);
const POST = resolve(process.argv[3]);

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

function loadLab(file) {
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
    // IntersectionObserver is deliberately ABSENT: the post lab must degrade to
    // "every row visible" without it, and the pre lab never had it.
  };
  sandbox.globalThis = sandbox; sandbox.self = sandbox; sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  for (let i = 0; i < blocks.length; i++)
    new vm.Script(blocks[i], { filename: `${basename(file)}#script${i + 1}` })
      .runInContext(ctx, { timeout: 20000 });
  vm.runInContext(`globalThis.__p = {
    load, step,
    DT(){ return DT; }, MAXV(){ return MAXV; },
    cfg(o){ Object.assign(P,o); },
    n(){ return nodes.length; },
    raw(){ return nodes.flatMap(b=>[b.x,b.y,b.vx,b.vy]); },
    speeds(){ return nodes.map(b=>Math.hypot(b.vx,b.vy)); },
    setV(i,vx,vy){ nodes[i].vx=vx; nodes[i].vy=vy; },
    kick(i,d){ nodes[i].vx+=d; },
    /* Only the instrumented scratch copies define __clampN; on the tracked
       labs this reads null and the branch-count control is reported as absent
       rather than silently as zero. */
    clampN(){ return typeof __clampN==='undefined' ? null : __clampN; },
    resetClamp(){ if(typeof __clampN!=='undefined') __clampN=0; },
  };`, ctx);
  return sandbox.__p;
}

const pre = loadLab(PRE), post = loadLab(POST);
const DT = pre.DT(), MAXV = pre.MAXV();
if (!Object.is(DT, post.DT()) || !Object.is(MAXV, post.MAXV()))
  throw new Error('DT/MAXV differ between labs — comparison would be meaningless');

const mulberry32 = a => () => {
  a |= 0; a = (a + 0x6D2B79F5) | 0;
  let t = Math.imul(a ^ (a >>> 15), 1 | a);
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};

/* One scenario, applied identically to whichever lab it is handed. `throws`
   re-throws every body above MAXV every `throws` steps from a seeded stream,
   which is how the clamp is made to fire. Returns the samples AND the number of
   observed clamp engagements, so a "0 differ" can never be a clamp that slept. */
function run(p, sc) {
  p.load(sc.preset);
  p.cfg(sc.P || {});
  p.resetClamp();
  const r = mulberry32(sc.seed === undefined ? 0x51DE : sc.seed);
  const raw = [];
  let fired = 0;
  for (let i = 0; i < sc.steps; i++) {
    if (sc.throws && i % sc.throws === 0) {
      for (let b = 0; b < p.n(); b++) {
        const th = 2 * Math.PI * r(), sp = MAXV * (1.5 + 2 * r());
        p.setV(b, sp * Math.cos(th), sp * Math.sin(th));
      }
    }
    if (sc.kick && i === 0) p.kick(0, sc.kick);
    const before = sc.throws ? p.speeds() : null;
    p.step(DT);
    if (before) {
      const after = p.speeds();
      for (let b = 0; b < after.length; b++)
        if (before[b] > MAXV && after[b] <= MAXV * (1 + 1e-9)) fired++;
    }
    if (i % sc.every === 0) raw.push(...p.raw());
  }
  return { raw, fired, branch: p.clampN() };
}

const ndiff = (a, b) => (a.length !== b.length ? Math.max(a.length, b.length)
  : a.reduce((n, v, i) => n + (Object.is(v, b[i]) ? 0 : 1), 0));

const PRESETS = ['binary', 'sun', 'figure8', 'chaos'];
const THERMO = [['thermo off', { thermoOn: false }],
                ['thermo on', { thermoOn: true, thermoV: 0.4, thermoTau: 0.5 }]];
const BAND = [['band off', { vdpOn: false, vdpG: 0 }],
              ['band g>0', { vdpOn: true, vdpG: 0.6, vdpR0: 0.2 }],
              ['band g<0', { vdpOn: true, vdpG: -0.6, vdpR0: 0.2 }]];

let rows = [], totalDoubles = 0, totalDiff = 0, totalFired = 0;
let branchMismatch = 0, branchPre = 0, branchPost = 0, branchSeededQuiet = 0, branchSeededRegulated = 0;
for (const preset of PRESETS)
  for (const [tn, tp] of THERMO)
    for (const [bn, bp] of BAND)
      for (const clamp of [false, true]) {
        const sc = {
          preset, steps: 4000, every: 40, seed: 0x51DE,
          P: { ...tp, ...bp }, throws: clamp ? 250 : 0,
        };
        const a = run(pre, sc), b = run(post, sc);
        const d = ndiff(a.raw, b.raw);
        totalDoubles += a.raw.length; totalDiff += d; totalFired += b.fired;
        if (a.fired !== b.fired) throw new Error('clamp engagement count differs — labs diverged structurally');
        if (!Object.is(a.branch, b.branch)) branchMismatch++;
        if (a.branch !== null) {
          branchPre += a.branch; branchPost += b.branch;
          /* The must-read-zero half of the control is scoped to what the
             profile's control C actually measured — seeded play with BOTH
             regulators off. It is NOT zero with the Van der Pol band on: see
             branchSeededRegulated, which is a finding, not a failure. */
          if (!clamp) { if (tn === 'thermo off' && bn === 'band off') branchSeededQuiet += b.branch;
                        else branchSeededRegulated += b.branch; }
        }
        rows.push({ preset, thermo: tn, band: bn, clamp: clamp ? 'THROWN>MAXV' : 'seeded',
                    doubles: a.raw.length, differ: d, clampFired: b.fired,
                    clampBranchPre: a.branch, clampBranchPost: b.branch });
      }

// ---------------------------------------------------------------- controls
// NEG: the same comparator, post vs post with one 1-ULP-class velocity kick.
const negRows = [];
for (const preset of PRESETS) {
  const sc = { preset, steps: 4000, every: 40, seed: 0x51DE, P: { thermoOn: false, vdpOn: false, vdpG: 0 }, throws: 0 };
  const a = run(post, sc);
  const b = run(post, { ...sc, kick: 1e-12 });
  negRows.push({ preset, doubles: a.raw.length, differ: ndiff(a.raw, b.raw) });
}

// CLAMP-CEILING: after a throw the clamp must actually bind the speed to MAXV.
function ceiling(p) {
  p.load('chaos'); p.cfg({ thermoOn: false, vdpOn: false, vdpG: 0 });
  p.setV(0, 5, 4);                          // |v| = 6.403..., MAXV = 3
  const before = p.speeds()[0];
  p.step(DT);
  return { before, after: p.speeds()[0] };
}
const CEIL = { pre: ceiling(pre), post: ceiling(post), MAXV };
const ceilingOK = CEIL.post.before > MAXV && CEIL.post.after <= MAXV * (1 + 1e-12)
  && Object.is(CEIL.pre.after, CEIL.post.after);

const out = {
  pre: basename(PRE), post: basename(POST), node: process.version,
  matrix: rows, totals: { configs: rows.length, doubles: totalDoubles, differ: totalDiff, clampEngagements: totalFired },
  control_clamp_branch: { instrumented: rows[0].clampBranchPre !== null, mismatchedConfigs: branchMismatch,
                          takenPre: branchPre, takenPost: branchPost,
                          takenOnSeededQuietRuns: branchSeededQuiet,        // must be 0 (profile control C)
                          takenOnSeededRegulatedRuns: branchSeededRegulated },
  control_negative: negRows,
  control_clamp_ceiling: CEIL,
};
console.log(JSON.stringify(out, null, 1));

const negFires = negRows.every(r => r.differ > 0);
const instrumented = rows[0].clampBranchPre !== null;
/* The branch control has BOTH halves: it must read zero where the profile's
   control C says the clamp never engages (seeded play), and non-zero where the
   bodies are thrown above MAXV — and the two labs must agree config by config. */
const branchOK = !instrumented || (branchMismatch === 0 && branchPost > 0 && branchSeededQuiet === 0);
const ok = totalDiff === 0 && totalFired > 0 && negFires && ceilingOK && branchOK;
console.error(`\n${ok ? 'GREEN' : 'RED'}  ${totalDiff}/${totalDoubles} doubles differ over ${rows.length} configurations; ` +
  `${totalFired} clamp engagements exercised; negative control fires on ${negRows.filter(r => r.differ > 0).length}/${negRows.length} presets; ` +
  `clamp ceiling ${CEIL.post.before} -> ${CEIL.post.after} (MAXV ${MAXV}), pre-lab lands on the same double: ${Object.is(CEIL.pre.after, CEIL.post.after)}` +
  (instrumented ? `; clamp BRANCH taken pre=${branchPre} post=${branchPost}, mismatched configs ${branchMismatch}, taken on seeded regulators-off runs ${branchSeededQuiet} (must be 0), on seeded regulated runs ${branchSeededRegulated}` : '; clamp branch not instrumented (tracked labs)'));
process.exit(ok ? 0 : 1);
```

</details>

<details><summary>make_clampcount.py — the counter inside the clamp's own branch, both labs</summary>

```python
#!/usr/bin/env python3
"""Scratch copies of both labs with a counter INSIDE the max-speed clamp branch.

The engagement counter in orbital_bitident.mjs is an outside observer (speed
before vs after a whole step), and the COM lock moves velocities after the
clamp, so it can only ever be a heuristic. This instruments the branch itself:
if the pre and post labs take the branch the same number of times on every
configuration, and the seeded no-throw runs take it ZERO times (the profile's
control C, which is the must-read-zero half), then A3's rewritten condition is
the same predicate and not merely a cheaper one.

The tracked lab is never touched; these are scratch copies (L0048).
Every anchor is asserted to match exactly once.
"""
import sys, io

def patch(src_path, dst_path, anchor, repl, tag):
    s = io.open(src_path, encoding='utf-8').read()
    n = s.count(anchor)
    assert n == 1, f'{tag}: anchor matched {n} times, expected 1'
    out = s.replace(anchor, repl, 1)
    assert out != s, f'{tag}: no change'
    # the counter needs a home; declare it beside MAXV
    m = 'const MAXV=3.0;'
    assert out.count(m) == 1, f'{tag}: MAXV anchor'
    out = out.replace(m, m + '\nvar __clampN=0;', 1)
    io.open(dst_path, 'w', encoding='utf-8').write(out)
    print(f'{tag}: wrote {dst_path}')


pre_src, post_src, pre_dst, post_dst = sys.argv[1:5]

patch(pre_src, pre_dst,
      "    const s=Math.hypot(b.vx,b.vy); if(s>MAXV){b.vx*=MAXV/s;b.vy*=MAXV/s;}",
      "    const s=Math.hypot(b.vx,b.vy); if(s>MAXV){__clampN++;b.vx*=MAXV/s;b.vy*=MAXV/s;}",
      'pre')

patch(post_src, post_dst,
      "    if(b.vx*b.vx+b.vy*b.vy>MAXV*MAXV){const s=Math.hypot(b.vx,b.vy); b.vx*=MAXV/s;b.vy*=MAXV/s;}",
      "    if(b.vx*b.vx+b.vy*b.vy>MAXV*MAXV){__clampN++;const s=Math.hypot(b.vx,b.vy); b.vx*=MAXV/s;b.vy*=MAXV/s;}",
      'post')
```

</details>

<details><summary>profile_draw2.mjs — whole-frame canvas op counts, steady state, with row visibility</summary>

```javascript
/*
 * profile_draw2.mjs — canvas-operation and getComputedStyle counts for a WHOLE
 * ORBITAL frame (draw() + updateReadouts()), before and after the B126 render
 * wins.
 *
 * This is profile_draw.mjs from traces/2026-09-16-b126-orbital-profile.md §4
 * with three deliberate extensions, because the original could not see two of
 * the four ratified changes:
 *
 *  1. It counted `draw()` and ONE `drawXYGraph()` and multiplied by N. B4 makes
 *     the number of strips drawn per frame a variable, so the strips have to be
 *     counted through the real `updateReadouts()` instead of assumed.
 *  2. It counted the FIRST frame after load. A1 makes the first frame
 *     unrepresentative by design (it fills the cache), so frames are counted
 *     cold AND in steady state and both are reported.
 *  3. It drives the sim between frames, because B4's cadence gate only shows
 *     up as a difference when frames and strip samples are not 1:1 (a 120 Hz
 *     display, time scale < 1, or paused).
 *
 * Counts, never timings: there is no rasteriser here, and a vm/stub realm is
 * not a timing harness (LIBRARY L0052).
 *
 * Usage: node profile_draw2.mjs --lab <file> --n <N> [--trails 0|1] [--polar 0|1]
 *                               [--steps-per-frame 8] [--visible <k|all>] [--paused 0|1]
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import vm from 'node:vm';

const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i < 0 ? d : process.argv[i + 1]; };
const LAB = resolve(arg('lab'));
const N = +arg('n', 8);
const TRAILS = arg('trails', '1') !== '0';
const POLAR = arg('polar', '0') !== '0';
const SPF = +arg('steps-per-frame', 8);
const VISIBLE = arg('visible', 'all');
const PAUSED = arg('paused', '0') !== '0';

// ------------------------------------------------------------ counting context
const counts = Object.create(null);
const bump = k => { counts[k] = (counts[k] || 0) + 1; };
const CTX_METHODS = ['beginPath', 'moveTo', 'lineTo', 'stroke', 'fill', 'arc', 'rect', 'fillRect',
  'clearRect', 'strokeRect', 'fillText', 'strokeText', 'save', 'restore', 'setLineDash',
  'closePath', 'translate', 'scale', 'rotate', 'createRadialGradient', 'createLinearGradient',
  'quadraticCurveTo', 'bezierCurveTo', 'drawImage', 'measureText', 'ellipse',
  'setTransform', 'resetTransform', 'transform', 'clip', 'createPattern', 'roundRect'];
const CTX_PROPS = ['fillStyle', 'strokeStyle', 'lineWidth', 'globalAlpha', 'font', 'textAlign',
  'textBaseline', 'lineCap', 'lineJoin', 'globalCompositeOperation'];
function makeCtx(tag) {
  const c = {};
  for (const m of CTX_METHODS) c[m] = (...a) => {
    bump(`${tag}.${m}`);
    if (m === 'createRadialGradient' || m === 'createLinearGradient')
      return { addColorStop: () => bump(`${tag}.addColorStop`) };
    if (m === 'measureText') return { width: 10 };
    return undefined;
  };
  for (const p of CTX_PROPS) {
    let v = '';
    Object.defineProperty(c, p, { get: () => v, set: x => { bump(`${tag}.set:${p}`); v = x; } });
  }
  c.canvas = { width: 600, height: 600 };
  return c;
}

// ------------------------------------------------------------------ mini DOM
function el(tag = 'div', id = '') {
  const e = {
    tagName: tag, id, dataset: {}, children: [], _ctx: null,
    style: { setProperty() {}, removeProperty() {}, getPropertyValue: () => '' },
    className: '',
    width: 240, height: 44, value: '0.5', checked: false, textContent: '',
    title: '', selectedIndex: 0, options: [],
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    addEventListener() {}, removeEventListener() {}, appendChild(c) { this.children.push(c); return c; },
    remove() {}, setAttribute() {}, getAttribute: () => null,
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 600, height: 600 }),
    setPointerCapture() {}, releasePointerCapture() {},
    getContext(kind) {
      const tag2 = id === 'field' ? 'field' : (id === 'vstrip' ? 'vstrip' : 'strip');
      return (this._ctx ||= makeCtx(tag2));
    },
    querySelector(sel) { return (this._q ||= {})[sel] ||= el('canvas', id + sel); },
    querySelectorAll() { return []; },
  };
  // innerHTML='' is how renderNodeList empties the list — it has to really empty it
  let _html = '';
  Object.defineProperty(e, 'innerHTML', { get: () => _html, set: v => { _html = v; if (v === '') e.children.length = 0; } });
  return e;
}
const registry = Object.create(null);
const byId = id => (registry[id] ||= el('div', id));

const g = globalThis;
const def = (k, v) => Object.defineProperty(g, k, { value: v, writable: true, configurable: true });
let cssCalls = 0;
def('document', {
  getElementById: byId,
  querySelectorAll: () => [],
  createElement: t => el(t),
  documentElement: el('html', 'html'),
  addEventListener() {},
});
const cssProps = new Set();
def('getComputedStyle', () => ({ getPropertyValue: v => { cssCalls++; cssProps.add(String(v)); return '#7fd3e6'; } }));
def('navigator', {}); def('location', {}); def('devicePixelRatio', 2);
def('AudioContext', function () { return { destination: {}, createGain: () => ({ connect() {}, gain: { value: 0 } }) }; });
def('webkitAudioContext', g.AudioContext);
def('requestAnimationFrame', () => 0); def('cancelAnimationFrame', () => {});
def('setInterval', () => 0); def('clearInterval', () => {});
def('setTimeout', () => 0); def('clearTimeout', () => {});
def('addEventListener', () => {}); def('alert', () => {});
def('Image', class {});
/* IntersectionObserver stub: the driver decides which rows are on screen.
   Default is "all visible", which is the WORST case for B4 — any saving
   measured there comes from the cadence gate alone, not from hiding rows. */
const observed = [];
def('IntersectionObserver', class {
  constructor(cb) { this.cb = cb; }
  observe(t) { observed.push(t); }
  disconnect() { observed.length = 0; }
});
def('window', g); def('self', g);

const html = readFileSync(LAB, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);

/* Same seeded ring layout as the profile's §4 harness, so the op counts are
   comparable to the 13,533 it recorded. renderNodeList() is called here and the
   original did not need to: updateReadouts() walks the real row list. */
const BENCH = `
const SEED = 0x51DE;
function __setup(N){
  const r = mulberry32(SEED);
  nodes = [];
  for(let i=0;i<N;i++){
    const th = 2*Math.PI*i/N, rad = 0.22 + 0.06*r(), sp = 0.18 + 0.10*r();
    nodes.push(mk({ name:'B'+i, m: 0.6 + 1.0*r(),
      x: 0.5 + rad*Math.cos(th), y: 0.5 + rad*Math.sin(th),
      vx: -sp*Math.sin(th), vy: sp*Math.cos(th), tx:'none', ty:'none' }));
  }
  P.G=0.025; P.eps=0.06; P.bounds='reflect'; P.com=true; P.scale=1;
  P.damp=0; P.cushK=0; P.cushW=0.1; P.thermoOn=false; P.vdpOn=false; P.vdpG=0;
  simTime=0; renderNodeList();
}
/* The frame body of frame(), minus the rAF plumbing and the wall clock: step
   the sim, record the strips on their own cadence, push the trails once. */
function __advance(k, isPaused){
  paused = !!isPaused;
  if(isPaused) return;
  for(let i=0;i<k;i++){
    step(DT);
    if((++histStep)>=HIST_EVERY){ histStep=0;
      for(const b of nodes){ b.hx[b.hp]=outX(b); b.hy[b.hp]=outY(b); b.ha[b.hp]=outAngle(b); b.hr[b.hp]=outRadius(b); b.hp=(b.hp+1)%b.hx.length; }
      vbarHist[vbarP]=meanSpeed(); vbarP=(vbarP+1)%vbarHist.length;
      /* The post lab bumps this at the same site; the pre lab has no such
         binding. Without it here the B4 gate would never see a new sample and
         the harness would measure zero strip redraws — a lie in our favour. */
      if(typeof stripRev!=='undefined') stripRev++; }
  }
  for(const b of nodes){ b.trail.push([b.x,b.y]); if(b.trail.length>220)b.trail.shift(); }
}
function __fill(N){
  __setup(N);
  for(let i=0;i<2500;i++) __advance(8,false);
  return { trail: nodes[0].trail.length, strip: nodes[0].hx.length };
}
globalThis.__api = { __setup, __fill, __advance, draw, updateReadouts,
  rows: () => $('nodes').children, P, nodes: () => nodes };
`;
vm.runInThisContext(blocks.join('\n') + '\n' + BENCH, { filename: 'lab+bench' });
const api = g.__api;

const filled = api.__fill(N);
api.P.trails = TRAILS;
api.P.polar = POLAR;

// row visibility: 'all' or a count of rows left on screen
if (VISIBLE !== 'all') {
  const k = +VISIBLE;
  observed.forEach((t, i) => { t._vis = i < k; if (i >= k) t._sk = null; });
}

function snapshot() { return { ...counts, __css: cssCalls }; }
function delta(a, b) {
  const out = {};
  for (const k of new Set([...Object.keys(a), ...Object.keys(b)])) {
    const d = (b[k] || 0) - (a[k] || 0);
    if (d) out[k] = d;
  }
  return out;
}
const sum = o => Object.entries(o).reduce((s, [k, v]) => k === '__css' ? s : s + v, 0);

/* One counted frame = advance + draw + updateReadouts, which is exactly what
   frame() does between two requestAnimationFrame callbacks. */
function frame() {
  const b0 = snapshot();
  api.__advance(SPF, PAUSED);
  const b1 = snapshot();
  api.draw();
  const b2 = snapshot();
  api.updateReadouts();
  const b3 = snapshot();
  return { field: delta(b1, b2), readouts: delta(b2, b3), all: delta(b0, b3) };
}

const frames = [];
for (let i = 0; i < 6; i++) {
  const f = frame();
  frames.push({
    field_ops: sum(f.field), field_css: f.field.__css || 0,
    readout_ops: sum(f.readouts), readout_css: f.readouts.__css || 0,
    total_ops: sum(f.all), total_css: f.all.__css || 0,
    trail_ops: Object.entries(f.field).filter(([k]) => k.startsWith('field.')).reduce((s, [, v]) => s + v, 0),
    detail: f.all,
  });
}
// frames[0] is cold (A1's cache is empty); the steady state is the mean of the rest
const steady = frames.slice(1);
const avg = k => steady.reduce((s, f) => s + f[k], 0) / steady.length;

console.log(JSON.stringify({
  lab: LAB.split('/').pop(), n: N, trails: TRAILS, polar: POLAR,
  steps_per_frame: SPF, visible: VISIBLE, paused: PAUSED,
  trail_len: filled.trail, strip_len: filled.strip,
  cold_frame: { ops: frames[0].total_ops, css: frames[0].total_css },
  steady_frame: { ops: avg('total_ops'), css: avg('total_css'),
                  field_ops: avg('field_ops'), readout_ops: avg('readout_ops') },
  per_frame_series: frames.map(f => ({ ops: f.total_ops, css: f.total_css, field: f.field_ops, readouts: f.readout_ops })),
  steady_detail: frames[frames.length - 1].detail,
  /* Lifetime, not per frame: how many forced style resolutions the page does in
     total, and over how many distinct custom properties. */
  css_lifetime_calls: cssCalls, css_distinct_properties: [...cssProps].sort(),
  node: process.version,
}, null, 1));
```

</details>

<details><summary>sweep.mjs — the scenario matrix, one child process per cell</summary>

```javascript
/* sweep.mjs — the per-frame op counts for both labs across the frame/visibility
 * scenarios B4 is supposed to distinguish. One child process per cell.
 *
 * Usage: node sweep.mjs <dir> <pre.html> <post.html>
 */
import { execFileSync } from 'node:child_process';
import { join, resolve } from 'node:path';

const DIR = resolve(process.argv[2]);
const LABS = [['PRE  (origin/main)', resolve(process.argv[3])],
              ['POST (this branch)', resolve(process.argv[4])]];

const CASES = [
  ['60 fps, ts=1, 8 rows visible',            ['--steps-per-frame', '8']],
  ['120 Hz (4 steps/frame), 8 visible',       ['--steps-per-frame', '4']],
  ['ts=0.25 (2 steps/frame), 8 visible',      ['--steps-per-frame', '2']],
  ['paused',                                  ['--steps-per-frame', '8', '--paused', '1']],
  ['60 fps, panel scrolled: 2 of 8 visible',  ['--steps-per-frame', '8', '--visible', '2']],
  ['60 fps, panel scrolled: 0 of 8 visible',  ['--steps-per-frame', '8', '--visible', '0']],
  ['60 fps, trails OFF, 8 visible',           ['--steps-per-frame', '8', '--trails', '0']],
  ['60 fps, polar strips, 8 visible',         ['--steps-per-frame', '8', '--polar', '1']],
];

const rows = [];
for (const [label, lab] of LABS)
  for (const [name, args] of CASES) {
    const r = JSON.parse(execFileSync('node',
      [join(DIR, 'profile_draw2.mjs'), '--lab', lab, '--n', '8', ...args],
      { encoding: 'utf8', maxBuffer: 1 << 28 }));
    const s = r.steady_frame;
    rows.push({ lab: label, scenario: name, ops: s.ops, field: s.field_ops, readouts: s.readout_ops,
                css: s.css, cold_css: r.cold_frame.css, cold_ops: r.cold_frame.ops });
    console.error(`${label}  ${name.padEnd(42)} ops ${String(s.ops).padStart(7)}  field ${String(s.field_ops).padStart(6)}  readouts ${String(s.readout_ops).padStart(6)}  css ${s.css}`);
  }
console.log(JSON.stringify(rows, null, 1));
```

</details>

<details><summary>trail_pixels.mjs — drawn segment geometry and alpha, with the halved-ramp control</summary>

```javascript
/*
 * trail_pixels.mjs — "nothing the user sees changes", as a comparison of the
 * drawn geometry rather than an assurance.
 *
 * The recording context keeps the ORDERED argument stream, not just counts, and
 * reconstructs every stroked line segment as
 *     (x1,y1) -> (x2,y2)  at  globalAlpha-in-effect-at-stroke-time
 * which is the model both labs fit: the pre lab sets the alpha before
 * beginPath, the post lab before stroke, and a canvas resolves it at stroke.
 *
 * Two claims are checked over a whole frame at N = 8:
 *   P1  the SEGMENT GEOMETRY is bit-identical (same multiset of endpoint
 *       quadruples, same count) — the trail is in the same place, the same
 *       length, with the same wrap seams lifted.
 *   P2  the only difference is alpha, and it is bounded by half a bucket,
 *       0.55/(2*8) = 0.034375.
 *   CONTROL  the same comparator against a lab whose trail alpha ramp is
 *            deliberately halved must FAIL P2 — otherwise "within a bucket"
 *            is a statement about the comparator, not about the labs (L0032).
 *
 * Usage: node trail_pixels.mjs <pre.html> <post.html> [ctrl.html]
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import vm from 'node:vm';

function record(LAB, N = 8) {
  const segs = [];
  let path = [], cur = null, alpha = 1;
  const ctxFor = tag => {
    const c = {
      beginPath() { path = []; },
      moveTo(x, y) { cur = [x, y]; },
      lineTo(x, y) { if (cur) path.push([cur[0], cur[1], x, y]); cur = [x, y]; },
      stroke() { if (tag === 'field') for (const s of path) segs.push([...s, alpha]); path = []; },
      closePath() {}, fill() {}, clearRect() {}, fillRect() {}, strokeRect() {}, rect() {},
      arc() {}, ellipse() {}, fillText() {}, strokeText() {}, save() {}, restore() {},
      setLineDash() {}, translate() {}, scale() {}, rotate() {}, setTransform() {}, clip() {},
      resetTransform() {}, transform() {}, drawImage() {}, roundRect() {},
      quadraticCurveTo() {}, bezierCurveTo() {},
      createRadialGradient: () => ({ addColorStop() {} }),
      createLinearGradient: () => ({ addColorStop() {} }),
      measureText: () => ({ width: 10 }),
      canvas: { width: 600, height: 600 },
    };
    for (const p of ['fillStyle', 'strokeStyle', 'lineWidth', 'font', 'textAlign', 'textBaseline',
                     'lineCap', 'lineJoin', 'globalCompositeOperation']) c[p] = '';
    Object.defineProperty(c, 'globalAlpha', { get: () => alpha, set: v => { alpha = v; } });
    return c;
  };

  function el(tag = 'div', id = '') {
    const e = {
      tagName: tag, id, dataset: {}, children: [], _ctx: null,
      style: { setProperty() {}, removeProperty() {}, getPropertyValue: () => '' },
      className: '', width: 240, height: 44, value: '0.5', checked: false, textContent: '',
      title: '', selectedIndex: 0, options: [],
      classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
      addEventListener() {}, removeEventListener() {}, appendChild(c) { this.children.push(c); return c; },
      remove() {}, setAttribute() {}, getAttribute: () => null,
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 600, height: 600 }),
      setPointerCapture() {}, releasePointerCapture() {},
      getContext() { return (this._ctx ||= ctxFor(id === 'field' ? 'field' : 'other')); },
      querySelector(sel) { return (this._q ||= {})[sel] ||= el('canvas', id + sel); },
      querySelectorAll() { return []; },
    };
    let _html = '';
    Object.defineProperty(e, 'innerHTML', { get: () => _html, set: v => { _html = v; if (v === '') e.children.length = 0; } });
    return e;
  }
  const registry = Object.create(null);
  const byId = id => (registry[id] ||= el('div', id));
  const g = globalThis;
  const def = (k, v) => Object.defineProperty(g, k, { value: v, writable: true, configurable: true });
  def('document', { getElementById: byId, querySelectorAll: () => [], createElement: t => el(t),
                    documentElement: el('html', 'html'), addEventListener() {} });
  def('getComputedStyle', () => ({ getPropertyValue: () => '#7fd3e6' }));
  def('navigator', {}); def('location', {}); def('devicePixelRatio', 2);
  def('AudioContext', function () { return { destination: {}, createGain: () => ({ connect() {}, gain: { value: 0 } }) }; });
  def('webkitAudioContext', g.AudioContext);
  def('requestAnimationFrame', () => 0); def('cancelAnimationFrame', () => {});
  def('setInterval', () => 0); def('clearInterval', () => {});
  def('setTimeout', () => 0); def('clearTimeout', () => {});
  def('addEventListener', () => {}); def('alert', () => {}); def('Image', class {});
  def('IntersectionObserver', class { observe() {} disconnect() {} });
  def('window', g); def('self', g);

  const html = readFileSync(LAB, 'utf8');
  const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
  const BENCH = `
  const SEED = 0x51DE;
  function __fill(N){
    const r = mulberry32(SEED);
    nodes = [];
    for(let i=0;i<N;i++){
      const th = 2*Math.PI*i/N, rad = 0.22 + 0.06*r(), sp = 0.18 + 0.10*r();
      nodes.push(mk({ name:'B'+i, m: 0.6 + 1.0*r(),
        x: 0.5 + rad*Math.cos(th), y: 0.5 + rad*Math.sin(th),
        vx: -sp*Math.sin(th), vy: sp*Math.cos(th), tx:'none', ty:'none' }));
    }
    P.G=0.025; P.eps=0.06; P.bounds='reflect'; P.com=true; P.scale=1;
    P.damp=0; P.cushK=0; P.cushW=0.1; P.thermoOn=false; P.vdpOn=false; P.vdpG=0;
    simTime=0;
    for(let i=0;i<20000;i++){ step(DT); if(i%4===0) for(const b of nodes){ b.trail.push([b.x,b.y]); if(b.trail.length>220)b.trail.shift(); } }
  }
  globalThis.__api = { __fill, draw };
  `;
  vm.runInThisContext(blocks.join('\n') + '\n' + BENCH, { filename: 'lab+bench' });
  g.__api.__fill(N);
  segs.length = 0;
  g.__api.draw();
  return segs;
}

const [PRE, POST, CTRL] = process.argv.slice(2).map(p => p && resolve(p));

/* One process per lab: the recorder installs itself on the real globalThis, so
   two labs in one process would share a DOM and a draw history. */
if (process.env.TP_ONE) {
  console.log(JSON.stringify(record(process.env.TP_ONE)));
  process.exit(0);
}
import { execFileSync } from 'node:child_process';
const one = lab => JSON.parse(execFileSync(process.execPath, [new URL(import.meta.url).pathname],
  { env: { ...process.env, TP_ONE: lab }, encoding: 'utf8', maxBuffer: 1 << 28 }));

const a = one(PRE), b = one(POST);
const key = s => `${s[0]},${s[1]},${s[2]},${s[3]}`;
function compare(x, y, label) {
  const gx = x.map(key), gy = y.map(key);
  const sameGeom = gx.length === gy.length && gx.every((v, i) => v === gy[i]);
  let maxDA = 0, nAlphaDiff = 0;
  if (sameGeom) for (let i = 0; i < x.length; i++) {
    const d = Math.abs(x[i][4] - y[i][4]);
    if (d > 0) nAlphaDiff++;
    if (d > maxDA) maxDA = d;
  }
  return { label, segments: x.length, sameGeometry: sameGeom, alphaDiffering: nAlphaDiff, maxAlphaDelta: maxDA };
}
const BUCKET_HALF = 0.55 / (2 * 8);
const r = compare(a, b, 'pre vs post');
const ctrl = CTRL ? compare(a, one(CTRL), 'CONTROL pre vs halved-ramp') : null;

console.log(JSON.stringify({ bucketHalf: BUCKET_HALF, result: r, control: ctrl }, null, 1));
const ok = r.sameGeometry && r.maxAlphaDelta <= BUCKET_HALF + 1e-15 && r.alphaDiffering > 0
  && (!ctrl || ctrl.maxAlphaDelta > BUCKET_HALF);
console.error(`\n${ok ? 'GREEN' : 'RED'}  ${r.segments} stroked field segments; geometry identical: ${r.sameGeometry}; ` +
  `${r.alphaDiffering} differ in alpha only, max |dalpha| ${r.maxAlphaDelta.toFixed(6)} (half a bucket = ${BUCKET_HALF})` +
  (ctrl ? `; CONTROL halved ramp max |dalpha| ${ctrl.maxAlphaDelta.toFixed(6)} — must exceed half a bucket` : ''));
process.exit(ok ? 0 : 1);
```

</details>

<details><summary>patch_lab.py / patch_rev.py — the edit itself, every anchor asserted to match exactly once</summary>

```python
#!/usr/bin/env python3
"""Apply the ratified B126 render wins (A1, A2, A3, B4) to the ORBITAL lab.

Every anchor is asserted to match EXACTLY ONCE and the output to differ from
the input: a replace that silently matches nothing produces a file that reads
as "already done" (LIBRARY L0032 corollary, and the B126 profile's own rule).
"""
import sys, io

path = sys.argv[1]
src = io.open(path, encoding='utf-8').read()
orig = src


def sub(anchor, repl, tag):
    global src
    n = src.count(anchor)
    assert n == 1, f'{tag}: anchor matched {n} times, expected 1'
    src = src.replace(anchor, repl, 1)
    assert src != orig, f'{tag}: no change'


# ---------------------------------------------------------------- A1: cache css()
sub(
    "const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim();",
    """/* The palette is resolved ONCE per custom property and memoised. Each
   getComputedStyle() here is a forced style resolution against the document
   element, and the uncached version cost 65 of them PER FRAME at N = 8 —
   inside the O(N^2) force-line loop, once per trail, three times per body
   (B126 profile section 4). The palette is static: one :root block, no theme
   switch anywhere in this file. If one is ever added, it must call
   refreshPalette() — the cache is the only thing that would not follow it. */
const cssCache=new Map();
const css=v=>{let c=cssCache.get(v); if(c===undefined){c=getComputedStyle(document.documentElement).getPropertyValue(v).trim(); cssCache.set(v,c);} return c;};
const refreshPalette=()=>cssCache.clear();""",
    'A1',
)

# ------------------------------------------------- A3: square the max-speed compare
sub(
    "    const s=Math.hypot(b.vx,b.vy); if(s>MAXV){b.vx*=MAXV/s;b.vy*=MAXV/s;}",
    """    /* Max-speed clamp. Compare SQUARED and take the root only when it fires:
       the root was an unconditional Math.hypot per body per step (15.0 % of the
       whole step at N = 8) guarding a threshold nothing reaches in normal play
       (B126 profile control C, peak 1.61 against MAXV 3.0). Math.hypot is kept
       for the scaling itself, NOT replaced by sqrt(x*x+y*y), so a clamp that
       DOES fire scales by the bit-identical factor. */
    if(b.vx*b.vx+b.vy*b.vy>MAXV*MAXV){const s=Math.hypot(b.vx,b.vy); b.vx*=MAXV/s;b.vy*=MAXV/s;}""",
    'A3',
)

# ------------------------------------------------------- A2: batch the trail strokes
sub(
    """  // trails
  if(P.trails) for(const b of nodes){
    const col=css(b.color);
    cx.strokeStyle=col; cx.lineWidth=1.2;
    const t=b.trail; if(t.length<2)continue;
    for(let i=1;i<t.length;i++){
      const p=t[i-1],q=t[i];
      if(Math.abs(p[0]-q[0])>0.5||Math.abs(p[1]-q[1])>0.5)continue;
      cx.globalAlpha=(i/t.length)*0.55;
      cx.beginPath();cx.moveTo(gu(p[0])*S,gu(p[1])*S);cx.lineTo(gu(q[0])*S,gu(q[1])*S);cx.stroke();
    }
  }""",
    """  /* Trails, batched by alpha bucket (B126 profile A2). The fade is what used
     to cost, not the trail: a per-SEGMENT globalAlpha cannot share a path, so
     220 segments x 8 bodies paid a full beginPath/moveTo/lineTo/stroke each —
     8,776 of 13,533 canvas calls per frame. Quantising the fade to
     TRAIL_FADE_STEPS buckets makes each bucket one path with one stroke. The
     alpha is monotone in i, so a bucket is a contiguous run and the polyline
     survives; the wrap seam still lifts the pen. */
  if(P.trails) for(const b of nodes){
    const col=css(b.color);
    cx.strokeStyle=col; cx.lineWidth=1.2;
    const t=b.trail; if(t.length<2)continue;
    let bk=-1, pen=false;
    for(let i=1;i<t.length;i++){
      const p=t[i-1],q=t[i];
      const k=(i*TRAIL_FADE_STEPS/t.length)|0;
      if(k!==bk){ if(bk>=0){cx.globalAlpha=trailAlpha(bk);cx.stroke();} cx.beginPath(); bk=k; pen=false; }
      if(Math.abs(p[0]-q[0])>0.5||Math.abs(p[1]-q[1])>0.5){pen=false;continue;}
      if(!pen){cx.moveTo(gu(p[0])*S,gu(p[1])*S);pen=true;}
      cx.lineTo(gu(q[0])*S,gu(q[1])*S);
    }
    if(bk>=0){cx.globalAlpha=trailAlpha(bk);cx.stroke();}
  }""",
    'A2',
)

# A2 constants, parked next to radius() so draw() reads top-down
sub(
    "function radius(b){return 4+Math.sqrt(b.m)*5}",
    """function radius(b){return 4+Math.sqrt(b.m)*5}

/* Trail fade quantisation (A2). 8 buckets over a 0..0.55 alpha ramp is a
   0.034 step — below the alpha resolution of a 1.2 px line against the field
   background, and the bucket boundary moves along the trail every frame as
   samples shift, so there is no fixed seam to see. The bucket's alpha is its
   MIDPOINT, so the ramp keeps its mean, not just its ends. */
const TRAIL_FADE_STEPS=8;
const trailAlpha=k=>((k+0.5)/TRAIL_FADE_STEPS)*0.55;""",
    'A2-const',
)

# ------------------------------------------- B4: draw the per-body strips lazily
sub(
    """function renderNodeList(){
  const host=$('nodes'); host.innerHTML='';""",
    """/* Row visibility for the lazy strips (B4). An off-screen strip is 505 canvas
   ops per frame drawn for nobody. IntersectionObserver rather than
   getBoundingClientRect: the rect read is a forced layout per row per frame,
   which is the same cost A1 just removed from the palette. Rows start assumed
   visible so the first frames are never blank; the observer corrects within a
   frame or two. */
let rowObserver=null;
function observeRow(el){
  if(typeof IntersectionObserver!=='function'){el._vis=true;return;}
  rowObserver||=new IntersectionObserver(es=>{for(const e of es){e.target._vis=e.isIntersecting; if(!e.isIntersecting)e.target._sk=null;}},
                                         {root:null,rootMargin:'160px'});
  el._vis=true; rowObserver.observe(el);
}
function renderNodeList(){
  const host=$('nodes'); host.innerHTML='';
  if(rowObserver){rowObserver.disconnect(); rowObserver=null;}""",
    'B4-observer',
)

sub(
    "    host.appendChild(el);\n  });\n}",
    "    host.appendChild(el); observeRow(el);\n  });\n}",
    'B4-observe-call',
)

sub(
    """    el.querySelector('[data-bar=y]').style.width=(y*100)+'%';
    drawXYGraph(el.querySelector('.xyg'), b);""",
    """    el.querySelector('[data-bar=y]').style.width=(y*100)+'%';
    /* The strip redraws on its OWN cadence, not the animation frame's: its
       content changes only when a new sample is recorded (b.hp advances once
       per HIST_EVERY physics steps = 60/s of SIM time) or when something the
       strip prints changes. On a 120 Hz display, at time scale < 1, or while
       paused, that is strictly fewer redraws and never a different picture.
       Hidden rows draw nothing and are marked dirty so they redraw on return. */
    if(el._vis===false){ el._sk=null; }
    else { const k=stripKey(b); if(k!==el._sk){ el._sk=k; drawXYGraph(el.querySelector('.xyg'), b); } }""",
    'B4-gate',
)

sub(
    """/* Per-body x/y position graphs (human 2026-09-15)""",
    """/* Everything drawXYGraph puts on the canvas that is NOT a strip sample: which
   pair of series is drawn, and the frozen-state caption. Folded into the redraw
   key so a polar/pause/time-scale/pin change repaints immediately even though no
   new sample arrived. */
const stripKey=b=>b.hp+'|'+(P.polar?1:0)+'|'+(paused?'p':(P.ts===0?'t':(b.pinned?'n':'')));
/* Per-body x/y position graphs (human 2026-09-15)""",
    'B4-key',
)

io.open(path, 'w', encoding='utf-8').write(src)
print('patched OK ->', path)
```

```python
import sys, io
p = sys.argv[1]
s = io.open(p, encoding='utf-8').read(); o = s
def sub(a, r, tag):
    global s
    n = s.count(a); assert n == 1, f'{tag}: {n} matches'
    s = s.replace(a, r, 1); assert s != o, tag

# the counter, declared with the other strip state
sub("const vbarHist=new Float32Array(240); let vbarP=0;",
    """const vbarHist=new Float32Array(240); let vbarP=0;
/* Bumped by EVERY write to a body's strip ring — the sampling tick below and
   primeStrips. The lazy redraw (B4) keys on it rather than on b.hp, because hp
   is a ring pointer: primeStrips resets it to 0, so a prime that happened to
   land while hp was already 0 would leave the strip showing the old curve with
   no way for the gate to know. A revision counter cannot alias that way. */
let stripRev=0;""", 'rev-decl')

sub("function primeStrips(b){ b.hx.fill(outX(b)); b.hy.fill(outY(b)); b.ha.fill(outAngle(b)); b.hr.fill(outRadius(b)); b.hp=0; }",
    "function primeStrips(b){ b.hx.fill(outX(b)); b.hy.fill(outY(b)); b.ha.fill(outAngle(b)); b.hr.fill(outRadius(b)); b.hp=0; stripRev++; }",
    'rev-prime')

sub("        vbarHist[vbarP]=meanSpeed(); vbarP=(vbarP+1)%vbarHist.length; }   // the field strip rides the same sim-time cadence (B126)",
    "        vbarHist[vbarP]=meanSpeed(); vbarP=(vbarP+1)%vbarHist.length; stripRev++; }   // the field strip rides the same sim-time cadence (B126)",
    'rev-tick')

sub("const stripKey=b=>b.hp+'|'+(P.polar?1:0)+'|'+(paused?'p':(P.ts===0?'t':(b.pinned?'n':'')));",
    "const stripKey=b=>stripRev+'|'+(P.polar?1:0)+'|'+(paused?'p':(P.ts===0?'t':(b.pinned?'n':'')));",
    'rev-key')
io.open(p, 'w', encoding='utf-8').write(s)
print('rev patch OK')
```

</details>
