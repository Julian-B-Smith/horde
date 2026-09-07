# Proposal — set modules + a routing matrix: what it fixes, what it forces

**Status: groundwork, awaiting the human.** Written 2026-08-26 at the human's
direction: *"We are going to have to come up with an elegant rework of the FX
system since we're replacing the variable slots with set slots and a routing
matrix that allows for feedback and bypass; fortunately I haven't made a single
preset that uses the same module twice, so we could write an algorithm that
ports over to all existing patches nondestructively."*

The direction is right, and it is more right than it looks: it **dissolves**
two open problems instead of patching them. It also forces one architecture
decision that must be made before any code is written. Both are below.

---

## 1. What the rework dissolves for free

### B49's chimera stops being possible

Yesterday's defect: slot `type` is a stepped param in the morph field, drawn
independently of `amount`, so a sweep between a Drive corner and a Gain corner
spent its middle third at `type=Drive, amount=0.10` — a state neither corner
authored (measured: 3 of 9 positions). ADR-124 patched it with an atomic group.

**With set modules there is no `type` param at all.** A module's identity is
structural, not a value, so there is nothing stepped to draw and no chimera to
assemble. ADR-124's group becomes vestigial for FX the day this lands.

### Bypass becomes continuous, so the ramp is free

B49's other half wanted slot presence to *ramp* rather than snap. In a
crosspoint matrix, presence **is** a coefficient, and ADR-088's founding
rationale is exactly this property:

> a coefficient of 0 *is* "not connected" — so connecting and disconnecting are
> one continuous motion.

So "bypass" needs no special case: it is a gain going to zero, and it morphs
continuously by construction. The same is true of ADR-123's osc on/off ramp —
this is the third instance of one principle, and the matrix is its general form.

### It subsumes the FX slot contract proposal

`docs/proposals/fx-slot-contract.md` (2026-08-15, still awaiting the human)
documents the real mess: `amount` has **three different identity points**
across six modules (0, 0.5, and *none*), and **two modules cannot be bypassed at
any setting** — Comp's 0.98 brickwall is always on, Notch measured −5.4 dB and
mono at `amount = 0`.

Under the matrix, bypass stops being the module's job. A module with no live
input edge contributes nothing regardless of its own parameters, so
"is there a no-op value?" stops being a question anyone has to answer per
module. **The rework retires that proposal rather than resolving it** — worth
recording, because otherwise it sits open forever.

---

## 2. The decision the rework forces: feedback is not free

**`routing_core.h` as built cannot do feedback, by design, in one line:**

```cpp
static constexpr bool edgeLive(int from, int to)
{
  return from < NSRC ? true : (from - NSRC) < to;   // slots read EARLIER slots only
}
```

Strict acyclicity is what makes a single forward pass always correct and lets
the audio thread skip cycle detection entirely. Feedback breaks that, and the
two ways out are not equivalent:

| | mechanism | latency in the loop | verdict |
|---|---|---|---|
| **A. Block-rate delay** | one buffer of delay on any backwards edge | **2.9 ms** at 128 @ 44.1k, and it **changes with the host's buffer size** | A 2.9 ms loop is a flanger, not a routing primitive — and a patch that sounds different at 64 vs 512 samples violates our own determinism rule |
| **B. Per-sample matrix** | one-sample delay on backwards edges; matrix runs inside the sample loop | 22.7 µs, buffer-independent | Musically correct; costs a per-sample crosspoint sum instead of a per-block one |

FOUNDATIONS' OQ-23 ruling (*"a cycle is legal, every feedback edge carries a
unit delay at block rate"*) was made for the **modulation** graph, where
block-rate is fine. Inheriting it for audio would be a category error.

**Recommendation: B, and scope it.** Per-sample crosspoint summing over N
modules is N² multiply-adds per sample in the worst case; at N=6 and all
crosspoints live that is 36 MACs/sample ≈ 1.6 M/s per channel — negligible.
It stops being negligible if the roster grows (below), so the honest form is
**per-sample summing over LIVE edges only**, with the live set recomputed per
block.

---

## 3. The sizing question nobody has asked yet

`RoutingMatrix` carries `static_assert(NSRC + NSLOT <= 32)`. Today's rack is 6
real modules. **B45's filter roster adds five more** (SVF, ladder, comb,
formant pair, tilt) — that is 11 modules, 121 crosspoints, and with four morph
corners **484 morphable values for routing alone**.

So the roster and the matrix are one decision, not two:

- every module always present → idle cost scales with the roster, where today
  `FxType::Off` costs *literally nothing* (`case FxType::Off: break;`);
- therefore the renderer **must** skip modules with no live path — the exact
  analogue of `fx_rack.h:272`'s `mix <= 0` guaranteed bypass, promoted from a
  per-slot check to a graph property;
- and B47's finding applies here too: a feedback comb can ring indefinitely, so
  the FX graph needs its own analogue of B38's voice cull, or CPU ratchets the
  same way release tails do.

---

## 4. The migration algorithm — and the hazard in it

The human's premise (*"I haven't made a single preset that uses the same module
twice"*) makes the port injective and therefore exact. Two corrections:

**(a) The unit is the CORNER, not the patch.** Each of the four morph corners
stores its own four slot types. The no-duplicate property must hold **within
each corner**, which is four times as many chances to violate it as the patch
view suggests. The algorithm must *detect* the collision, never assume it away.

**(b) Cross-corner duplicates are fine and strictly better.** Corner A holding
Drive@0.9 in slot 1 and corner B holding Drive@0.3 in slot 2 is *not* a
collision: both become the one Drive module, whose `amount` then morphs
continuously between 0.9 and 0.3 — which is an improvement on today, where they
were two unrelated slots.

```
for each corner c in A..D:
    seen = {}
    for slot s in 0..3:
        t = type[c][s]
        if t == Off: continue
        if t in seen: REFUSE — report (patch, corner, slots) and stop
        seen[t] = s
        module[t].amount[c] = amount[c][s]
        module[t].tone[c]   = tone[c][s]
    # order becomes topology: chain the non-Off modules in slot order
    prev = SOURCE
    for s in slot order where type[c][s] != Off:
        edge[c][prev -> module[type[c][s]]] = 1.0
        prev = module[type[c][s]]
    edge[c][prev -> OUT] = 1.0
    # mix: today's per-slot dry/wet becomes the module's own send depth
    for s: module[type[c][s]].send[c] = mix[c][s]
```

**Nondestructive** means: write the new keys **alongside** the old ones and
bump the chunk version, so a patch saved by the new build still loads in the
old one until the old one is retired. Never rewrite the v-old keys in place.

### The order hazard, which is a ruling not a bug

Corner A may be `Drive → Comb` while corner B is `Comb → Drive`. The matrix
expresses both. But morphing between them passes through a state where **both
edges are partially open** — which is a *parallel blend* of the two modules,
not either serial order, and it does not sound like either endpoint.

That is the same "what does a flip between structures *mean*" question
FOUNDATIONS has open (their OQ #2), now in its most concrete form. Two answers:

1. **Accept it.** Parallel-in-the-middle is a legitimate and arguably musical
   interpretation of morphing between two orderings.
2. **Argmax the topology.** Route edges follow QM-0's `QUANTUM` law — pick a
   corner's whole topology rather than blending coefficients — so order flips
   discretely while module *parameters* still blend.

Our own ROADMAP already names both laws (`BLEND` vs `ARGMAX`) and records they
are *not* two settings of one dial. **This ruling should be made before the
migration is written**, because it decides whether ported patches need their
route edges marked as one atomic group per corner.

---

## 5. Suggested order

1. **Rule** the feedback mechanism (§2) and the topology-morph law (§4). Both
   are human calls and both gate design, not just implementation.
2. **Fix the seam debt first if it is cheap** — unrelated, but it is the only
   open cross-repo obligation and it touches note handling the FX rework does
   not.
3. Extend `routing_core.h` with the ruled feedback mechanism, keeping
   `routing_check`'s 7 invariants green and adding cycle cases.
4. Port `fx_rack.h`'s six modules to a module interface with no bypass duty.
5. Write the migration with the collision detector as a **gate**, not a
   warning, and a golden corpus of real presets as its oracle.
6. Retire `docs/proposals/fx-slot-contract.md` as subsumed (§1).

---

# Part 2 — The recommendation: how the FX should be shaped (2026-08-28)

**Written at the human's request before anything locks:** *"what would you say
is the optimal way to set up FX so they work with quantum morph but also allow
for the greatest degree of flexibility... 1. CPU-affordable 2. robust/modular/
flexible 3. morph-friendly."*

Most of this is composition, not invention — the rulings already exist
(ADR-088/123/124/125/128, B49/B63/B65). What follows is the shape they add up
to, plus the two trade-offs that are genuinely still open.

## The one-sentence shape

**A fixed roster of always-present module instances behind one dense crosspoint
matrix, where presence is a coefficient, structure flips atomically, cost is
charged only for audible signal, and every module exposes a small morphable
macro face with its depth behind it.**

## 1. CPU-affordable: existence is free, audibility costs

The bar is already set by today's rack: `FxType::Off` is `break;` — a slot that
does nothing costs nothing. Set modules must clear the same bar, and the
mechanism generalises:

- **Reachability skip.** A module with no live input path from any source, or
  no live path onward to the output, is not processed at all. This is
  `mix <= 0`'s guaranteed bypass promoted from a per-slot check to a graph
  property, computed on the control thread when edges change — never per
  sample.
- **Quiesce.** Reachable is not audible: a reverb whose input path just closed
  is still ringing, and a reverb that finished ringing is still reachable.
  Each module answers "am I silent?" (input silent AND internal energy below a
  floor) and sleeps when it is. **This is B47's lesson arriving in the FX
  domain** — release tails cost full price until a threshold retires them, and
  an FX graph accumulates ringing state exactly the way the voice pool
  accumulates tails. voiceCull's shape (a floor, default conservative) is the
  precedent. Neither TimeCore nor the rack has this today; it is new work and
  it is the *load-bearing* half of "affordable".
- **Live-edge summing** (already an ADR-128 constraint): the matrix sums over
  the live edge set recomputed per block, never the full N² table.

Worst-case cost is then bounded by *modules carrying audible signal*, not by
roster size — which is what makes a generous roster safe to ship.

## 2. Morph-friendly: three laws, all already ruled, applied uniformly

The morph question is settled in pieces; the design just has to apply them to
every axis of the FX consistently:

| axis | law | already ruled by |
|---|---|---|
| module **parameters** | morph as values (pick or blend, per `morphMode`) | ADR-104 |
| **structure** (which edges) | ARGMAX — route edges draw ONE corner, atomically | ADR-125 + ADR-124's lead map |
| **presence** (how much) | continuous — a coefficient of zero IS disconnection, and flips glide *through* zero rather than cutting | ADR-088 A1, ADR-123's shape |
| **inaudible params** (module has no live path in a corner) | the B65 rule: AUTHOR / HOLD / ADOPT toggle, ADOPT = weighted average of ON corners (ratified 2026-08-27) | B65 |

Two consequences worth stating plainly:

- **Set modules dissolve the chimera class.** No `type` param → nothing stepped
  for the field to draw independently → ADR-124's FX atomic group becomes
  vestigial. The B49 defect cannot recur *by construction*.
- **B65 must be stated in terms of live paths, not oscillators.** "This
  module's parameters, in a corner where no path reaches it, are held / adopted
  / authored" — same toggle, same rule, no special case for oscillators.

## 3. Robust/modular: the facility owns safety; the module owns sound

The charter's "safety by construction, not by vigilance," applied at the
boundary:

- **ADR-031's laws live in the MATRIX, not in modules.** Feedback-edge
  normalisation (/N, worst-case correlation), a DC blocker on cycle edges, and
  the loop-gain ceiling are facility guarantees — a module cannot know it is
  inside a loop, so it must not be responsible for surviving one. The
  morph-law lab's resonant-damper failure (a default-Q lowpass quietly adding
  loop gain) is the cautionary instance: the facility should also own its own
  in-loop filters' Q.
- **NaN watchdog at the module boundary** (ADR-032's law: a NaN must never
  silence the instrument for more than a block).
- **The module contract**, kept small: stereo block process in place, declared
  latency, the quiesce query, and — only for modules that opt in — a per-sample
  tap. **Cycle membership requires the tap** (ADR-128 option B): at edit time
  the control thread finds cycles and switches exactly those members to
  sample-wise processing. A module without a tap (a lookahead compressor, any
  future FFT module) is simply *refused membership in a cycle*, surfaced in the
  UI as a rule rather than discovered as a glitch. That is the principled
  answer to "a compressor detecting over a block is not naturally per-sample":
  it is not per-sample, so it does not go in loops.
- **Latency is declared but v1 keeps a zero-latency roster.** The moment a
  latent module lands in a *parallel* path, the rack needs internal delay
  compensation — real work that should arrive with the first module that needs
  it, not speculatively.

## 4. Flexible: the macro face / deep set split

The flexibility ceiling is not ids (768 free) and not really
`kMaxParams = 512` (a constexpr, raisable) — it is **automation-lane clutter
and corner-chunk size**, which grow with every exposed param (188 declared
today; B63's OTT alone wants ~21).

So every module ships two surfaces:

- **The macro face: ≤ 8 morphable params**, in the field, in the corner chunks,
  on the slot page. For OTT that is *depth* (+ maybe time); for Echo/Room it is
  roughly what ADR-131 already picked. Eight is not the old stride embarrassed
  into a rule — it is about what a corner can meaningfully author and a pad
  meaningfully morph.
- **The deep set: everything else**, exposed on per-module pages as params that
  default to **morph-exempt** (the exempt machinery already exists, ADR-109) so
  they are automatable and tweakable but do not bloat every corner or the
  Gumbel field by default. Un-exempting one is the player's deliberate act.

This keeps "fully modifiable" true without making every corner carry 21 OTT
values it will never author.

## The two genuinely open trade-offs (human decisions, both cheap to defer)

1. **One instance per type, or N?** The migration premise (no preset uses a
   module twice) makes one-per-type exact today, and it is the cheap, morphable
   shape — module identity is structural, so "two Drives" is not expressible.
   The flexible answer is instance slots (any type in any of N sockets), which
   reintroduces a stepped identity param and with it the chimera problem the
   set-module design exists to kill. **Recommendation: one instance per type
   for 1.0**, revisit only if a real patch wants two of something — and note
   the workaround (Comb and Notch are different types; the roster can grow a
   second flavour of a popular module more safely than it can grow sockets).
2. **How much of the deep set exists at 1.0.** Macro faces + reachability +
   quiesce is a shippable, affordable, morphable system. Deep pages can arrive
   module-by-module afterwards without touching the architecture. This is
   B63's staging generalised to the whole rack.

## What this rules out, explicitly

- **Dynamic module instantiation** (allocation, and "which modules exist" as
  morphable state — a structure chimera by construction).
- **Dry/wet inside modules** — bypass is an edge property; the module-level
  identity-point mess is the fx-slot-contract proposal's whole finding, and
  this retires it.
- **Every deep param as a day-one automation lane** — the clutter cost is the
  player's, and it is the one cost that cannot be optimised later without
  breaking saved automation.

---

# Part 3 — The module roster — REVISION A, the human's edits applied (2026-08-28)

The human went through the list module by module. This revision applies every
ruling; the original is in git history. Standing rulings carried forward:
one-instance-per-type **except 2–3 filter sockets** (fixed sockets, so the
chimera stays dead), macro face / deep set split, and — new this revision —
**every surviving module gets a visualizer** (the human asked for one on
almost every line; it is now a roster-wide requirement, not a per-module note).

## Dynamics

| module | ruling | face notes |
|---|---|---|
| **Comp** | KEEP, **promote** — *"practically useless"* today (one `strength` knob over a hidden brickwall). Gets its own controls + a visual interface | threshold · ratio · attack · release · makeup, **gain-reduction meter** as the visual |
| **OTT** | KEEP (B63) | *"no notes other than I want to make sure it has its own visualizer"* — 3-band GR display is the natural one |
| **Gain** | **BURIED** — *"I have never used the gain module."* Goes read-only with the old surface in the rebuild; the matrix's edge coefficients are its replacement |

## Distortion / colour

| module | ruling | face notes |
|---|---|---|
| **Saturator** (Drive's successor) | **REBUILD** — *"a proper saturator/overdrive module with a color control and different algorithms"*, plus a visualizer (transfer-curve display is the obvious one) | drive · **color** · algorithm (fixed per-socket list) · output |
| **WARP** | KEEP as the deep distortion, **slimmed**: *"lose a handful of its subtler parameters; particularly anything related to movement, which could just be replaced by modulation"* — i.e. WARP's internal LFO/walk motion goes, and the mod matrix drives those targets instead. Needs a visualizer (transfer curve + hysteresis trace) | shape-morph · hysteresis · dispersion · drive |
| *Brainstorm requested* | The human asked for other distortion candidates. Offered for striking: **wavefolder** (through-zero, the West-coast bend) · **decimator/redux** (bitdepth + rate, the digital colour) · **rectifier/octave fuzz** · **hard/soft clipper with waveshape view** · **Chebyshev harmonic shaper** (dial harmonics in directly — pairs beautifully with a spectrum visual) · **ring mod against an internal swarm osc** (swarm-native colour nothing else has) | — |

## Filters (2–3 fixed sockets)

Unchanged from the original: **SVF** · **Ladder** · **Tilt** · **Formant pair**
as candidates for the 2–3 sockets. The human's phaser note below adds a
**standard phaser** to this neighbourhood.

## Swarm-native

| module | ruling | face notes |
|---|---|---|
| **Comb** | KEEP — *"great"*. More controls + *"maybe we can get creative with"* the visualizer. Creative candidate: the strings themselves — one line per active note, displacement as brightness, sympathetic ringing visible | wet · feedback · damp · keytrack · + new controls TBD |
| **Notch swarm** | **NIXED** as a module — *"K-coupling frequencies along a spectrum doesn't tend to be a very satisfying effect unless the K-value is being modulated like a spring."* Replaced by standard modules below |
| **Phaser swarm** | **NIXED** — same ruling. A **standard phaser** joins the roster instead |
| **Filter swarm** | **NIXED** by the same logic (not explicitly named, struck as the third of the family — flag if wrong) |

**The discussion hook the nix opened, worth keeping:** *"unless the K-value is
being modulated like a spring (though that does give some interesting ideas
worth a discussion)."* This is B57's silent spring pointed at FX — a
note-driven mass-spring modulating a *coupling*, so the swarm effect only
blooms when the playing moves. The swarm CORES survive their module burial
(they are oracle-covered and reachable in SWARM-FX); if the spring-modulated
version proves out, the module returns with a reason to exist.

## Time

| module | ruling | face notes |
|---|---|---|
| **Delay** (NEW) | **BUILD for an A/B** — a standard, boring, excellent delay: *"Hz/sync rate, feedback, L/R offsets, standard vs M/S vs ping-pong, in-line filter module."* The timing trio reuses ADR-128/B62's continuous·Hz·sync vocabulary | rate (Hz/sync) · feedback · L/R offset · mode (std/MS/pingpong) · filter cutoff · mix |
| **Echo** (tap-swarm) | ON TRIAL — *"sound nice but controls may add an unnecessary level of complexity with little payoff (random distribution laws are barely audibly different)."* The A/B against Delay decides; consolidation into one module is on the table |
| **Room** (FDN swarm) | ON TRIAL, leaning keep — *"I do like how Room sounds"* — but may be covered by Reverb below |
| **Reverb** (NEW) | **BUILD — the reverb lab's robust design**, which the original roster missed entirely: `docs/design/reverb-lab.html` carries a full pre-delay · ER · diffusion · FDN · swarm-modulation surface (~24 controls: RT60, damping, width, ER spin, tail decorrelation, mixing time…). Macro face from its headline controls; the rest is the deep set's first big customer | mix · pre-delay · size · decay · damping · width |

## The smear family (NEW — the human's reference list)

*"a feedback/allpass chain/phaser/dispersion filter/freeverb/Serum 2 Bode/
Scrumulator style module"* — that list names a family (phase and frequency
smearing) more than one module. Proposed as **two** modules, offered for
striking:

| module | covers | face |
|---|---|---|
| **Disperser** | allpass cascade with feedback — the chirp/laser/watery smear (WARP's dispersion stage, promoted to a standalone with resonance and feedback; freeverb-style allpass colour lives here too) | amount · frequency · resonance · feedback · stages |
| **Shifter** | Bode-style frequency shifter (Hilbert pair) — the Serum-2-Bode half; single-sideband shift + feedback gets the barberpole/Scrumulator territory | shift (Hz/sync) · feedback · mix |

*Prior-art note: the Scrumulator reference (frequent.audio) is recorded as the
human's pointer and has NOT been researched yet — the family description above
is from the named techniques, not from that product. Research before any public
claim (docs/PRIOR-ART.md discipline).*

## Modulation-class (unchanged)

**Chorus/ensemble** and **Kuro-phaser** remain blocked on B17's `link`
contract. The standard phaser ruled in above is a separate, simpler thing.

## Net roster after Revision A

**Kept/promoted:** Comp⁺ · OTT · Saturator · WARP⁻ · SVF · Ladder · Tilt ·
Formant · Comb⁺ · Delay · Reverb · Disperser · Shifter · (Echo, Room on trial)
· chorus-class later. **Buried:** Gain, notch/phaser/filter swarms (cores
survive in SWARM-FX). That is ~13 modules plus trials — a real instrument's
rack, and the reachability-skip + quiesce architecture from Part 2 is what
makes a roster this size affordable.
