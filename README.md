# horde

**A coupled-oscillator synthesizer — the supersaw taken seriously as physics.**

Every supersaw you have used picks a fixed detune recipe and hides it. horde makes the swarm
itself the instrument: its voices are Kuramoto-coupled oscillators you can herd into lock,
dissolve into cloud, splay into harmonic multiplication, or erase by interference. Everything
is deterministic and seeded — the same patch and the same notes produce the same samples.

One knob — **coupling** — turns a stack of detuned voices into a dynamical system: low, they
drift as a loose cloud; raised, they herd, shimmer, then lock; negative, they push apart and
multiply harmonics. **Inertia** gives the swarm weight, so lock is arrived at rather than set.
**Gravity** lets a held chord settle into just intonation — you hear the beating slow and stop.

**Two products live in this repository.** The one you can download is the **legacy plugin**:
frozen, and released as [`v0.9-legacy`](https://github.com/Julian-B-Smith/horde/releases/tag/v0.9-legacy)
on 2026-10-17. It is a CLAP-native instrument (VST3 and AU through clap-wrapper) with two
oscillators, a sub, an FX rack and a morph grid, and its C++ engine is held to parity (1e-6 RMS)
against the browser prototypes it was designed in — every run, by the oracle, not by ear.
**horde 2** is in development: a new plugin with its own identity, around a new engine being
rebuilt from a ratified spec in a clean room, with FX modules developed as sibling projects.
Nothing of horde 2 is released yet. [Where it is going](#where-it-is-going--horde-2) has the plan.

### Hear it

> **TODO(human) — three audio clips, 20–30 s each.** Nothing here can be rendered by the tree;
> the clips need a DAW and ears, and none must be fabricated. Wanted, matching the video plan
> in `ROADMAP.md` B103: (1) a **coupling sweep** on a held chord, K from −1 through 0 to +1;
> (2) **gravity settling a chord** — strike, hold, and let the beating stop; (3) a **morph
> patchwork** across the grid. Drop them under `docs/audio/`, name each with its patch and the
> build stamp that rendered it (the corner of every GUI shot says which code drew it — the same
> rule for sound), and link them here.

### See it

The two shots directly below the rule are real shots of the legacy plugin — MAIN on both
chassis, from the 2026-08-26 set in `docs/img/` — and OSC and the morph grid appear further
down. The interface kept changing after that date (the MOD page's LFOs, for one), so a detail
in a shot may differ from the released build.

> **TODO(human) — the rest of the screenshot set.** `docs/img/README.md` lists what is still
> owed (labs gallery, chassis/screen schemes, a green `./verify`, FX and MIX) and notes that
> none of the four existing shots has the build stamp in frame. Same width for every shot.

### Download

**The legacy plugin, `v0.9-legacy`** —
[the release page](https://github.com/Julian-B-Smith/horde/releases/tag/v0.9-legacy), built and
drafted by CI from the tag and published by the human on 2026-10-17. Its assets:

| asset | what it is |
|---|---|
| `horde-macos-universal-0.9-legacy.pkg` | macOS installer: VST3, AU and CLAP, into `/Library/Audio/Plug-Ins` |
| `horde-macos-universal-0.9-legacy.zip` | the same three macOS bundles, to copy by hand |
| `horde-windows-x64-0.9-legacy.zip` | Windows x64: VST3 and CLAP |
| `horde-0.9-legacy.cdx.json` | the SBOM: every pinned dependency |
| `SHA256SUMS` | check every download against it |

The macOS bundles are **ad-hoc signed, not notarized** (no Apple Developer ID is configured), so
macOS may refuse to open them until you allow them. This is the legacy plugin of record: old
projects depend on this archive, not on a future rebuild (ADR-186). There is no horde 2 download.

### Read the engineering

- **[docs/ENGINEERING.md](docs/ENGINEERING.md)** — how correctness is defined here, the
  RULING / ENCODING taxonomy, how an agentic process was governed, and the goldens problem
  with its v2 plan. Five minutes. A draft, written about the legacy engine.
- **[docs/MEASUREMENTS.md](docs/MEASUREMENTS.md)** — aliasing at four notes with the saw-shape
  section engaged, CPU per voice count, and the listening note. The direct answer to "parity
  isn't correctness".
- **[docs/H2-PLAN.md](docs/H2-PLAN.md)** — horde 2's plan of record: every part, the rows and
  decisions that govern it, and what 1.0 is.

---

*Names: the **device** is **horde**; its founding **engine** is **SWARM SAW**; the maker is
**Mindlathe**. "HYPERSAW" is the frozen legacy plugin id hosts use to re-find saved sessions
(`com.lifted-truck.hypersaw`), the legacy preset folder's name, and the name this project signs
in sibling projects' mailboxes — so that id will keep saying `hypersaw` long after nothing on
screen does. horde 2 has an id of its own, `com.mindlathe.horde` (ADR-235).*

![horde — MAIN, dark chassis](docs/img/gui-hero.png)
*MAIN on the **dark** chassis with the **TUBE** screen: the XY pad, patch storage, the bend
laws with their step-response and vibrato-retention meters, the scale mask, and the swarm's
phase circle mid-note — the ring is the swarm, the violet vector is the order parameter, and
the green arc is where the voices have collected. The wordmark is not a graphic; it is being
warped live by the engine. The interface carries its build stamp bottom-right, which says which
code drew it; this set was framed without it.*

![horde — the same page, light chassis](docs/img/gui-light.png)
*The **same page** on the **light** chassis with the **EMBER** screen. Chassis and screen are
two independent axes, which is why this is a pair rather than a variant: the layout, the
signal colours and their meanings are identical — only the ground and the phosphor moved.*

---

## What it does today: the legacy plugin

Everything in this section describes the **legacy plugin**, the one you can download
(`v0.9-legacy`). It is frozen: from here it receives only crash, real-time-safety, state and
host-validation fixes, display metadata the human asks for, and build fixes for toolchain drift
(ADR-186 item 1). horde 2 is described under [Where it is going](#where-it-is-going--horde-2).

Two **oscillators**, each a full swarm, and a **sub**, summed through a shared FX rack and a
master stage. The interface has six playing pages — MAIN, OSC, MIX, FX, MOD and MORPH — plus a
settings page and an undo-history page.

### OSC — where the sound is built

![horde — the OSC page](docs/img/gui-osc.png)
*OSC carries the density: nine clusters of controls against five live visualisers. The
coloured bars above the knob labels are **corner ownership** — each one says which morph
corner currently owns that parameter. Hovering a spectrum bar reads out its frequency band.*

A swarm of up to 32 voices per oscillator with **six detune laws** and seeded distributions,
plus the parts that make it a *dynamical* system rather than a chorus: **coupling K** (positive
pulls voices into lock, negative splays them apart), **drift**, **gravity** toward consonance,
**topology** (mean-field, non-local ring, two-cluster), **onset scatter**, and a **saw shape**
section with squareness, roundness and pitch-dependent rounding. The visualizers are not
decoration — the phase circle shows the swarm's coherence, the voice map shows pan × pitch
(target against actual), and the phase carpet shows each voice's phase over time.

### MIX — balance and output

Per-oscillator power, level, tuning, mute/solo and meters; master volume and transposition;
and the output stage — mono fold, bass-mono with an adjustable crossover, and 2× oversampling.
Disabled means it does not run, not that it runs quietly: with both oscillators switched off
the shell measured **0.03% of a core** (`docs/research/2026-08-22-two-osc-cpu-measurement.md`).

### FX — four slots in series

Four slots, order fixed by slot index, each with type / amount / tone. An `Off` slot is a
**bit-exact passthrough** that does not touch the buffer. Comp, Comb, Notch, Echo, Room and a
standard stereo Delay are real cores; Drive, Filter and Gain are honest placeholders, labelled
as such, kept because slot *order* is audible even with them.

### MAIN — performance

Bend behaviour is a first-class surface: **five travel laws** (off, constant time, constant
rate, lag, mass-spring), quantisation to chromatic or to a scale in three flavours, a step
grid, and an on-screen wheel so every one of them is auditionable without hardware. Glide
sources, mono/legato, and MPE per-note bend live here too.

**Eight macro knobs, MAIN's own XY pad, a morph mini, and wheels:** MAIN carries its own
assignable pad (each axis picks a macro, or None — and lists what it actually reaches through
the matrix), a light morph position pad that appears while morph is on, and on-screen
pitch-bend + mod wheels. The OSC pages keep their per-oscillator pads.

### MOD — the mod matrix

Macros reach any continuous parameter through the **mod matrix** — right-click a knob to send
it; modulation draws as an offset (a reach arc and a live tick around a stationary knob) — the
knob always shows the value you authored. Routes persist with the patch. The sources: four
envelopes, two LFOs, the eight macros, velocity, mod wheel, pressure and pitch wheel (ADR-149,
B171). While morph is ON, the macro family suspends so corners keep their authored identity
(ADR-152).

---

## Eight things that work differently

Where one of these resembles a category that already exists, the comparison is named in italics
rather than skipped; `docs/PRIOR-ART.md` carries the full accounting.

### Voices that listen to each other

Turn one knob — **coupling** — and a stack of detuned voices stops being a stack. Low, they
drift as a loose cloud. Raise it and they begin to herd, then shimmer, then lock into what
sounds like a single fat oscillator; the sound thins and focuses as they agree. Turn it
*negative* and they push apart instead, multiplying harmonics, and far enough out they start
cancelling each other into hollow, combed shapes.

Nothing is being crossfaded or morphed between those states. The voices are **coupled
oscillators**, each one nudged by what the others are doing, and the coupling knob is the
strength of that nudge. Every texture along the way is the same physical system finding a
different equilibrium — which is why the transitions sound continuous and slightly alive
rather than like a blend between presets.

*Coupled oscillators in audio are not unheard of — Chiral Audio's Foxfire ships a Kuramoto
chorus built on coupled LFOs, and there is a research lineage worth reading. What has no
shipping analogue found is using them as the synthesis engine itself.*

### Inertia — the swarm has weight

Add **inertia** and the voices stop responding instantly. Reach for lock and they swing past it
and settle back; let go and they coast before they scatter. The sound gains a lag between your
hand and the result, and lock becomes something the swarm *arrives at* rather than something it
is set to — earned on the way in, and lost reluctantly on the way out.

Technically the oscillators gain momentum, which turns the whole thing from a first-order
system into a second-order one. Practically it is the difference between a control and a thing
with weight.

### Detune as two decisions instead of one

Most instruments give you a detune amount and one built-in idea of where the voices go. Here
that splits into two menus you set independently, which is a much larger space than one slider.

**Where the voices sit** — spread evenly, along the classic JP-8000 curve, or scattered by a
seeded random shape (a gentle Gaussian bunching, or a Cauchy one with occasional far outliers)
— or placed at golden-ratio spacings that never repeat.

**What the spacing means** — the same amount can hold constant in *cents* (musically even),
constant in *Hz* (so low notes spread wider), even to the **ear** rather than to the maths, or
snapped to the **session tempo**, so the beating between voices lands on the beat. The last two
are the unusual ones: perceptually flat detune, and detune you can dial into rhythm.

*Serum 2's unison tuning modes prove players already read "tuning mode as a menu"; the
statistical, perceptual and metrical axes are the extension.*

### Consonance gravity — chords that settle into tune

Play a chord and the notes **drift into just intonation** — you hear the beating slow down and
stop, over a settling time you control. A *basin* sets how far out of tune a note can be and
still be pulled in; a *strength* sets how hard the pull is. Turn it up and the instrument
tunes itself as you play; turn it down and equal temperament stays exactly where you put it.

The pull is not a retuning rule applied to your notes before they reach the synth — it is a
force acting on the oscillators themselves, which is why the approach to tune is something you
*hear happening* rather than something that has already happened.

*Adaptive just intonation is a mature category — Hermode, Pivotuner, Alt-tuner. Those are
MIDI-domain rule engines that rewrite notes upstream of the synth; this is the same
`sin(error)` law that runs the unison swarm, operating one level up.*

### A pitch wheel with weight on it

Move the wheel and the pitch does not simply follow it. You choose **how** it travels: straight
there, taking the same *time* regardless of distance, moving at a constant *speed* so far bends
take longer, easing in like a slider with friction — or hanging off a **spring**, where the
pitch overshoots the target, swings back, and settles, with the spring's stiffness, damping and
return strength under your hands.

The spring setting is the one that changes how you play. A fast flick and a slow lean produce
genuinely different gestures from the same wheel, because you are moving a thing with mass
rather than addressing a number.

The same five behaviours drive **note-to-note glide**, so a legato slide and a wheel bend feel
like the same instrument rather than two unrelated features.

### Bend that stays in key

Set a scale and the pitch wheel stops being continuous: bends land **on the degrees of that
scale** instead of gliding through everything between. Bend up a tone and you arrive at the
next note *of the key*, not at whatever pitch the wheel happened to reach — so a bend is a
musical interval rather than a smear, and you can play expressively in a key without listening
for the edges of it.

Four modes cover the useful cases: **chromatic** (every semitone), **scale** (only the degrees
you have enabled), **scale (drag)** — which quantises the *destination* while letting the
journey there stay smooth — and **scale (offset)**, which keeps the interval you played rather
than the absolute pitch, so the same gesture transposes with the note.

Two details do the work that makes it feel solid rather than fussy. Quantising is **anchored to
the note you started from**, so releasing a bend returns you to exactly the pitch you played,
never a neighbouring degree. And a **hysteresis** control (in cents) keeps a bend parked between
two degrees from flickering between them — it has to travel meaningfully past the boundary
before it commits.

### The quantum morph — every parameter picks a side

Put four complete patches in the corners of a pad and move between them. **Each parameter is
assigned whole to one corner patch by a deterministic Gumbel-max law rather than crossfaded**,
so what you hear in between is a *patchwork* of the four sounds rather than an average of them.

That matters most for the parameters that cannot be averaged. A filter type, a waveform, an
oscillator being on — crossfading those means a discrete control snapping at the halfway point
and a state that belongs to no patch at all. Here they **flip coherently** instead, and
parameters that only make sense together flip together.

Three controls shape the patchwork: **temperature** loosens the borders so a corner's influence
reaches further, **coupling** makes parameters flip in blocs rather than independently, and the
**seed** fixes the whole arrangement — the same seed and position give you the same patchwork
every time, on any machine, so an accident you liked is somewhere you can return to.

*Morph pads themselves are not new — NI Super\*Saw ships X/Y state morphing. The assignment law
is what differs.* **Mechanics: §The morph grid below.**

### Humanization that behaves like an ensemble

Voices enter slightly apart, the way a real section does — but the interesting control is not
*how far* apart. It is how hard the voices try to correct back toward each other.

Leave them uncorrected and they wander: each entry drifts from the last, and the ensemble comes
apart over time like players who cannot hear one another. Correct too hard and they snap onto
the grid, which is tight but lifeless. In between there is a setting where voices pull toward
the group without ever quite arriving — always slightly early or slightly late, always
recovering — and that is what a real ensemble sounds like.

The reason it works is that listeners judge togetherness from the *pattern* of the timing
errors rather than their size. Each voice nudges itself toward the group's average and adds a
little motor noise of its own, which is the model measured from actual string quartets
(Vorberg/Wing): `offset ← offset − α·(offset − group average) + noise`.

Measured from the legacy engine when the feature landed (ADR-077) — the **lag-1** column is the
pattern, and it is the thing conventional humanize cannot produce:

| α | onset SD | lag-1 autocorrelation | what it is |
|---|---|---|---|
| 0 | 202 ms | +0.985 | a random walk — drifts without bound |
| **0.25** | **35.8 ms** | **+0.679** | bounded, *with structure* — real quartets |
| 1.0 | 26.4 ms | −0.072 | i.i.d. — **exactly what conventional humanize produces** |
| 1.5 | 31.6 ms | −0.550 | over-correction: alternating early/late |

Read the bottom rows: the familiar "humanize" sound is not an alternative to this control, it
is **one specific value of it** — α = 1.0, where every voice corrects fully and the errors stop
relating to each other at all. It is available here, and it is not the musical setting.

### Routing you can morph through

Connections between modules are **amounts, not switches**. A connection at zero is simply a
connection turned all the way down, so patching something in or out is a move you can make
gradually — and, more to the point, a move the morph pad can make *for* you as you travel
between corners. Signal flow becomes something that changes with the sound rather than a layout
you set beforehand and leave alone.

---

## The morph grid

![horde — the MORPH page](docs/img/gui-morph.png)
*The morph field, with the cursor parked off-centre. The four corner colours in the gradient
are the same four that mark parameter ownership everywhere else in the interface — that is the
whole idea in one picture: a patch is a position in a field, and every parameter belongs to
whichever corner won it.*

The feature that makes horde a *patch explorer* rather than a synth with a lot of knobs.

Four corners each hold a complete snapshot of the patch. The field **starts at 100% corner A**,
not in the middle: at a corner every parameter is owned by one snapshot, so the morph behaves
like an ordinary patch until you choose to move. Drag the XY field and every parameter
independently decides which corner it follows — this is **not** a crossfade. Each parameter
runs its own weighted draw, so moving across the field re-assembles the patch out of the four
corners rather than averaging them into mush. The controls that shape it:

- **Temperature** — how sharply a parameter commits to the nearest corner. Low is decisive;
  high loosens the borders so parameters start disagreeing near the middle.
- **Coupling** — whether parameters flip independently or move in blocs.
- **Seed** — the identity of the patchwork. The same seed always assembles the same way, so a
  field you like is a number you can write down.
- **Morph Glide** — how long a flip takes to travel.
- **Mode** — quantum (flip) or blend.

And the authoring side:

- **Corner colour coding.** Every parameter row on every page wears a stripe in the colour of
  the corner currently driving it, so you can see the patchwork rather than infer it.
- **Arm a corner** (the four colour boxes in the tab bar) and the interface shows *that
  corner's* stored settings instead of the live sound — you edit what you are looking at.
  Rows it owns but is not currently voicing go dotted.
- **Exempt a parameter** (right-click → *Exempt from morph*) to pull it out of the field
  entirely and hold it steady while everything else moves.
- Corners save and load as presets, and the whole field rides the DAW session.

---

## What is deliberately not here

Other engines and modules exist in the tree as parked, parity-gated candidates. None of them
is reachable from the released legacy interface, and none is a feature of this instrument until
the human re-opens it explicitly — `docs/PARKED.md` is the register, with a revisit trigger per
entry. The sections above describe what the legacy plugin ships.

## Where it is going — horde 2

`ROADMAP.md` is the single source of truth for status. **[docs/H2-PLAN.md](docs/H2-PLAN.md)** is
the README-level map of it for horde 2 — every part, its governing rows and decisions, its
status, what it needs first, and what 1.0 is — and
**[docs/design/h2-plan-map.html](docs/design/h2-plan-map.html)** draws the same plan as a graph
you can click (serve the repo with `python3 tools/serve_labs.py` and open it). The short version:

- **A new plugin, not a new version** (ADR-186). The legacy plugin keeps its id, its state format
  and its sound. horde 2 is a separate product with all of its identity new — CLAP id
  `com.mindlathe.horde` (ADR-235), a new AU subtype, its own bundle names and preset store — and it owes
  nothing to legacy DAW state. **1.0 is horde 2's first human-cut release tag: the stability
  line.** Before it, laws, parameters and ranges change freely; after it, they are locked and a
  sound-law change is selected by the state's version.
- **A new engine, rebuilt from a ratified spec** (ADR-223 to ADR-231). The engine that combined
  the swarm with SCALPEL's blades (windows that cut each cycle and replace their span) carried too
  much of its own history, so the human ruled a rebuild: a spec written from the user's side
  ([`specs/SPEC-ENGINE.md`](specs/SPEC-ENGINE.md)), acceptance tests written from it by a different
  agent before the engine exists, then a build from the spec and the tests alone, in a clean room
  (`engine2/`). The instrument's laws — the coupling law, member placement, gravity, the
  ensemble, every blade law — are copied into the spec as normative math (ADR-224 A1). One
  instrument-wide voice per played note holds both swarm oscillators with their blades, the sub
  and the noise oscillator. The labs will run it as WebAssembly.
- **FX modules developed as sibling projects** — **Shriek** (formerly MAW, a three-stage
  saturator), **Sluice**, **Scape** (reverb) and **Bulwark** (the compressor, and the master
  limiter) — hosted in horde's rack through one slot contract, beside horde's own ECHO, EQ, FX
  filter and drive. **The module cutoff is 2026-12-11** (ADR-236): each module's 1.0 MUST rows
  are judged on that date, and a module that misses it ships after 1.0.
- **The filters, in a sibling project of their own** — **Weir**, a C++ filter library
  (ADR-307, B567). It provides the new engine's per-voice filter slot and the FX filter module.
  horde's filter lab is its starting point, not its reference.

**The rebuild, stage by stage** (ROADMAP B494; the build's stages are the spec's BD-8):

| stage | status |
|---|---|
| The rebuild audit (B493) | Done 2026-10-16 (PR #1096) |
| The spec, `specs/SPEC-ENGINE.md` (B494 step 1) | **Ratified** by the human's merge of PR #1097, 2026-10-16; amended since by ADR-230 and ADR-231 |
| The clean-room tree `engine2/` and the 380 reference renders (B497) | Done 2026-10-17: the tree builds an empty library and an empty suite; the renders are release asset `engine2-reference-renders-v1` |
| Two history-free repositories, one for the test author and one for the builder (B502) | Made 2026-10-17; both sessions running, each repo with a private off-machine remote |
| The acceptance tests, by a different agent from the spec alone (B498) | Running: the test author delivers each stage's items ahead of the build, and each spec round's changes to both repos the same day (spec round 47 as of 2026-10-28, ADR-304) |
| The build, in eight stages: manifest and state; the voice, ENV 1, the pedal and the output; the swarm; the edge correction; the blades; the sub and noise; modulation, morph and state; WebAssembly — then the bank | The eight stages are built and the bank stage began 2026-10-26 (ADR-284). Spec rounds 42 to 47 are built in the builder's repo (2026-10-29): of 295 suite items, two still fail, DT-3's Crushed burst and four presets' key-off clicks found before these rounds. The soak is next. *Row checked 2026-10-29 against DECISIONS.md and the builder's report.* |
| Judgement and cutover (B494 step 4) | The human's ears on the whole bank A/B, the CPU target (B488), the invariant suite; then it replaces the current engine |

---

## The look

The interface is a designed object, not a control dump, and its rules are small enough to
state.

**One ink, one accent, and signals that are never borrowed.** Every outline on the page is
the same ink (`#191521`); every colour beyond that has exactly one job and is never reused
for a second one:

| token | value | its one job |
|---|---|---|
| `--value` | `#F5169C` | the value **you** authored — nothing the machine decided |
| `--physics` | `#6431F0` | the system acting: order parameter, K vector, rotor lead |
| `--meter` | `#00D5C8` | analysers and the R rim arc |
| `--marker` | `#FFD702` | attention: root note, overshoot, lock front |
| `--celebrate` | `#A6F219` | meters in the green |

That discipline is why the panels stay readable at density: if a mark is pink you know a
human put it there, and if it is violet you know the swarm did.

**Two independent axes, not one theme switch.** The *chassis* is light (cream `#F2EDE2`) or
dark (`#151220`) and carries the whole page. The *screens* — the phosphor wells where data
lives — are themed separately, because a data surface and a control surface want different
grounds: **TUBE**, **ORCHID**, **FROST**, **EMBER**, **DUSK**. You can run a dark screen on a
light chassis, which is the combination the design actually favours.

Signal hues survive the chassis flip; only their *emission* moves (`--value` becomes
`#FF3DAD` on the dark ground). A hue that changed meaning between modes would make the
"one job each" rule a lie in half the product.

**Alpha is ground-dependent, and the tokens know it.** Low-alpha glow is a dark-ground
technique — it disappears on cream. A `--scr-alpha` multiplier lets a painter ask for
"faint" and lets the scheme decide what faint costs on its own ground, so no painter carries
per-theme special cases.

**The wordmark is played, not drawn.** `horde` is rendered live through a backward-remap
warp field — wave, tremor, blobs and swirls sampled per pixel — and when an engine is
running the synth drives it: the order parameter **R** becomes ripple, detune becomes wiggle,
and the morph position becomes colour, blended bilinearly from the same four corner hues the
pad uses. In light mode an ink outline carries the contrast so the fill can stay at full
saturation, which is the only way corner A's `#FFD702` is legible on cream at all
(measured 1.34:1 as a fill, which is why the outline exists).

Design sources: `docs/design-system/`, the labs in `docs/design/`, and the tokens themselves
at the top of `src/gui/gui2.html` — which is the authority, since the interface generates
its controls from them.

---

## How correctness is defined

Not "it sounds plausible". What it is depends on which engine you mean, and the records say so
(ADR-187, ADR-224).

**The legacy plugin: parity with the prototypes.** Its C++ engines are **statement-level ports
of browser prototypes** that you can open and hear, and the oracle re-derives the goldens from
those prototypes on every run and compares:

- **Parity, in `./verify full`: 156/156 scenarios within 1e-6 RMS** (worst 4.262e-09; most
  match to the bit).
- Alongside parity sit the probes parity structurally **cannot** see: subdivision invariance,
  sample-rate independence, RT-safety (no allocation on the audio thread), note-lifecycle fuzz,
  voice-steal priority, GUI reachability, presentation-table totality, and the dependency graph.
- **`tests/feature_tests.tsv`** — 204 tests (116 an oracle runs, 88 that need ears or a host),
  each declaring whether it pins a **RULING** (a decision that must not silently change) or an
  **ENCODING** (how it happens to be done today).
- Note-lifecycle **conformance** against a sibling project's ruled behaviours, in `./verify full`
  on the maintainer's Mac only: the sibling's headers are private, so everywhere else the check
  announces that it skipped.

**The new engine: its spec, its invariant suite and the human's ears.** It is held to no JS
parity at all (ADR-224 item 8). Its correctness is the ratified spec, `specs/SPEC-ENGINE.md`; a
Layer-0 invariant suite written from that spec (determinism, real-time safety, aliasing, clicks,
CPU); and the human's ears on the whole bank. It need not sound the same as today's engine at
the same settings; it must do what a player expects of the waveform manipulation, and sound good
(ADR-223). Its suite is empty today (`suite: 0 items, 0 failed`): the tests come next, written
before the engine exists. The engine it replaces (`h2/engine/`) stays under parity until the
cutover.

**For both:** every `tools/*_check` is wired into `./verify` or says why not in its header
(ADR-180 §1); `test_table_check` prints the wired and `UNWIRED:` counts on every run. CI runs
`./verify fast`, the oracles under ASan and UBSan, a thread-race test, and the Windows build with
pluginval at strictness 10 on every pull request that is not docs-only — about 22 minutes (B505).
auval and host loads on the Mac are run by the human.

## Build

This builds the **legacy plugin**. CLAP-native, wrapped to VST3 and AUv2 by clap-wrapper.

```bash
cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

Pass `-DHYPERSAW_GUI2=OFF` for the legacy single-column interface (GUI1), which predates the
second oscillator, the mixer, the FX rack and the morph grid and cannot show any of them. It
is kept building so the escape hatch stays real.

The new engine builds on its own, from its clean-room tree:
`engine2/build.sh /absolute/path/to/a/build/dir` configures, builds and runs its suite (today an
empty library and `suite: 0 items, 0 failed`).

### Trying unmerged changes in a host: the test identity

A host finds a plugin by its identity, not its name, so a build of unmerged code under the
real identity *replaces* the installed plugin that real projects load. `-DHORDE_TEST_IDENTITY=ON`
(default OFF) builds the same sources as a second plugin that installs beside it (B456):

| | real (default, frozen) | test (`-DHORDE_TEST_IDENTITY=ON`) |
|---|---|---|
| Name in the host | `horde` | `horde TEST` |
| CLAP id | `com.lifted-truck.hypersaw` | `com.lifted-truck.hypersaw.test` |
| VST3 class id (derived from the CLAP id) | `F730E1CE68C657DB87D2452E272BD28F` | `B97E86E35E255030BEA8313DFA385F26` |
| AU type / subtype / manufacturer | `aumu` / `Hsaw` / `LfTk` | `aumu` / `HsTs` / `LfTk` |
| Bundle identifier | `com.lifted-truck.hypersaw.{clap,vst3,auv2}` | `com.lifted-truck.hypersaw.test.{clap,vst3,auv2}` |
| Files | `horde.vst3` · `horde.component` · `horde.clap` | `horde-test.vst3` · `horde-test.component` · `horde-test.clap` |
| Preset folder (under `~/Library/Application Support/LiftedTruck/`) | `HYPERSAW` | `HYPERSAW-TEST` |

Use a separate build directory, and build only the plugin and its store check there (the other
oracles create the plugin by its real id, and that tree's SWARM-FX bundles carry SWARM-FX's
real identity, so neither belongs in a test-identity tree):

```bash
cmake -S . -B build-test-identity -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DHORDE_TEST_IDENTITY=ON
cmake --build build-test-identity --target HYPERSAW_all identity_store_check -j
python3 tools/test_identity_check.py --built build-test-identity test   # the bundles carry the test ids, sealed
build-test-identity/identity_store_check test                           # the store is HYPERSAW-TEST only
```

The two can be installed side by side: no id, file name or folder is shared, and
`tools/test_identity_check.py` (in `./verify fast`) keeps it so. The test build never reads or
writes the real preset folder. It starts with the factory bank, which is embedded in the binary
and copied into `HYPERSAW-TEST` the first time its interface opens; user presets come across
only if someone copies them. A session saved with one identity does not open with the other.
The test bundles are signed by their own build; the real ones are still signed by `./install`.

**Installing is the lead's step, on the human's say-so, never an agent's.** The test bundles
are copied by hand (`./install` installs the real plugin only):

```bash
A=build-test-identity/HYPERSAW_assets
rm -rf ~/Library/Audio/Plug-Ins/VST3/horde-test.vst3 ~/Library/Audio/Plug-Ins/Components/horde-test.component ~/Library/Audio/Plug-Ins/CLAP/horde-test.clap
cp -R "$A/horde-test.vst3"      ~/Library/Audio/Plug-Ins/VST3/
cp -R "$A/horde-test.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "$A/horde-test.clap"      ~/Library/Audio/Plug-Ins/CLAP/
codesign --verify --deep --strict ~/Library/Audio/Plug-Ins/Components/horde-test.component
killall -9 AudioComponentRegistrar
auval -v aumu HsTs LfTk
```

To remove it, delete those three bundles and, if its presets are not wanted, the
`HYPERSAW-TEST` folder.

## Map

| Path | Purpose |
|---|---|
| `ROADMAP.md` | Phase-gated plan — **the single source of truth for status** |
| `DECISIONS.md` | ADR log, append-only |
| `docs/H2-PLAN.md` · `docs/design/h2-plan-map.html` | horde 2's plan of record, and the same plan as a clickable map |
| `specs/` — `SPEC.md`, `SPEC-<NAME>.md`, `ACCEPTANCE.md` | One spec per engine-family member; measured acceptance criteria. Moved 2026-09-07 (ADR-155) |
| `specs/SPEC-ENGINE.md` | The new engine's ratified spec — the only reference its builder reads |
| `engine2/` | The new engine's clean-room tree: an empty library and an empty suite until the tests and the staged build land (`engine2/README.md`, `engine2/CLEANROOM.md`) |
| `h2/` | horde 2's current engine (`h2/engine/`, the composed engine the rebuild replaces), its test harness, and the lifted cores it is checked against |
| `reference/` | The spec-in-code prototypes the parity oracles extract from — protected; an edit there is a spec change. Design labs stay in `docs/design/` |
| `src/*_core.h` | The legacy plugin's engines: header-only, pure, framework-free |
| `src/hypersaw_clap.cpp` | The legacy CLAP shell: params, state, notes/MPE, the morph field, viz feed |
| `src/gui/gui2.html` · `src/param_presentation.tsv` | The legacy interface, and the table 238 of its controls are generated from |
| `tools/` | The oracle: golden generator, parity/trajectory/invariant checks |
| `docs/design/` | 36 labs, plus review pages such as the plan map — where behaviour is auditioned before it becomes code |
| `docs/playbooks/` | How to wire a new source or feature through every seam (ids, class, morph, mod matrix, history, presets, GUI, RT safety, parity, checks), with a PR checklist — its citations are re-read by `tools/playbook_check.py` |
| `integrations/` | The inbound mailbox: briefs other projects file to this one, one folder per sender |
| `traces/` | Provenance log — one entry per merged change set |

## Status and known gaps

*Last verified: **2026-10-17**, against main `5613885` — every claim in this file re-checked
against the records, the code and the gates on that date. If this date is old, trust
`ROADMAP.md` over this file.*

Stated rather than omitted:

- **The legacy plugin is frozen, so its known render defects stay.** Changes that would alter its
  sound (ROADMAP B278, B279, B241) go to horde 2, not into legacy (ADR-186 item 1). Its interface
  runs in the system's web view, so it can still drift with WebKit; that is stated rather than
  hidden. Legacy leaves `main` only when the human has moved their own work to horde 2 (ADR-186
  item 6).
- **The macOS release is ad-hoc signed, not notarized.** CI notarizes only when Apple Developer
  ID credentials exist, and none are configured.
- **horde 2 has no plugin shell on `main` yet.** A playable preview around its current engine has
  been built and played in a DAW on the maintainer's machine (ROADMAP B458); it is not on `main`
  and is not distributed.
- **A parked engine's parameters exist in the parameter table but not in the shipped
  interface** — by ruling, not by accident (`docs/PARKED.md`; the engine source carries a
  STATUS header saying so). `gui_reach` is an either-GUI check and stays green across that
  gap; recorded here because a green gate is exactly where a gap like this hides.
- **Sixteen test-table rows have no oracle yet** — counted by the gate rather than quietly
  carried (`test_table_check` prints the number every run, which is why this line can be
  trusted to be current rather than remembered).
- **A check that is not wired carries `UNWIRED: <reason>` in its header**, and `test_table_check`
  fails the build otherwise (ADR-180 §1, 2026-09-19). The list of exemptions is the enforcement's
  own output, not this file.
- **In-page WebAudio health readouts are untrustworthy:** an analyser taps the graph rather
  than the device, and `ctx.currentTime` advances straight through an underrun. Both reported
  healthy audio through three rounds while a human heard the output cutting out.
- **Windows had its first native run on 2026-09-09** (builds, every oracle green, loads and
  plays in Live), and the release carries a CI-built Windows binary. One residual is open in
  ROADMAP B99: computer-keyboard MIDI still dies while a control is dragged. Reaper and Bitwig
  loads have not been tested.

## Licence

**All rights reserved until 1.0.** horde is public so that it can be read; until 1.0 there is no
licence to use, copy, modify or redistribute it. At 1.0 it will be released under the GPL
(ADR-219, 2026-10-13). The repository's author holds the rights in it.
