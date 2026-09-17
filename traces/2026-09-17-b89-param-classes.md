# b89-param-classes — every parameter carries a class (morphable / structural / device)

- **Queue item:** B89 phase 1 (QM-4 intent bus, `specs/SPEC-INTENT-BUS.md` §9.1:
  "Classify every parameter (morphable / structural / device) in the engine
  definition. This is the first PR; nothing else can land before it.")
- **Why:** §9 makes classification the gate on every later intent-bus PR, and the
  class is the row grammar B97's morph table and B136's module macro tiers both
  read. Definition only: nothing is persisted, nothing reaches the host, no audio
  path moved.
- **Evidence consulted:** `specs/SPEC-INTENT-BUS.md` §3.2 (parameter classes), §6.1
  (scope rules), §8 (structural parameters), §9 (migration); `src/hypersaw_clap.cpp`
  `ParamDef`/`kParams` (243 rows), `kGlobalIds`, `findParam`, `morphInit` +
  `buildMorphOrder` (ADR-159 order, the B49 lead map); `tools/polarity_check.cpp`
  and `tools/morphlayout_check.cpp` as the standalone-tool pattern; ROADMAP B89 /
  B95 / B97 / B104 / B136 rows; LIBRARY L0032 (detector-shares-assumption) and
  L0036 (pin your refusals).

## The shape chosen: a derivation + an override table, NOT a 9th column

ROADMAP's phase-1 acceptance offers both and asks which and why. **Derivation
function (`paramClassOf`) plus `kParamClassOverrides` beside it.** Three reasons:

1. **A column means editing all 243 frozen rows.** Every one carries a frozen id,
   range and default; a diff that touches all of them to add a field none of their
   behaviour depends on is risk bought with nothing. The override table touches
   zero existing rows — the diff to `kParams` is empty.
2. **A column states the rule 243 times** and so has 243 chances to disagree with
   it, with nowhere the rule itself is written. §9 says *classify by rule*, so the
   rule is the artifact; the table is only its exceptions, ordered by id, each
   carrying its reason.
3. **The 244th parameter is classified the day it is appended**, instead of
   silently inheriting whatever enum value happens to be 0.

Cost accepted, and stated in the source: an exception is invisible at the row it
applies to. Mitigated by keeping `kParamClassOverrides` id-ordered and reasoned,
and by `paramclass_check` printing the resolved class next to every row.

## The rule, written once

1. an id in `kParamClassOverrides` takes the class stated there;
2. otherwise **stepped → structural**;
3. otherwise **continuous → morphable**.

Rule 2 is a description of shipped behaviour, not a new claim: a stepped value
cannot be blended and the morph already ARGMAX-jumps it (ADR-150's note on
octave/semi). It also catches every *stepped* §8 structure selector for free —
Voices (1), Engine (43), FX type (57/59/61/63), Osc On (150), Mute/Solo (104/105),
D*Sync (233/241/249/257), Taps (202/…), Oversample (88), Mono Fold (15). §8's
carve-out ("including continuous-valued ones that select structure") is therefore
exactly one entry (id 160, below).

Keyed on the **base id**, so an oscillator and its `+1000` twin share a class by
construction — they share one `ParamDef`, and a class that differed between twins
would be a property of the instance, not of the parameter.

**`paramClassOf` deliberately does not read `morphIds`.** The morph field is built
independently in `morphInit`, so "no `morphIds` member is device" is a real
cross-check between two separately authored lists. Derive the class from the field
and that assertion certifies nothing (L0032).

## Counts

| scope | morphable | structural | device |
|---|---|---|---|
| the 243 `kParams` rows (the definition) | **137** | **73** | **33** |
| all 325 host-exposed ids (twins too) | 191 | 101 | 33 |

## Judgement calls

Every override is a judgement; these are the ones that were not dictated by the
acceptance text and that a reviewer should rule on.

1. **160 `voiceCull` → STRUCTURAL, and it is the ONLY continuous §8 selector.**
   Continuous (dB), so rule 3 would make it morphable; it is the voice-retirement
   threshold, so it selects voice count — §8's "continuous-valued ones that select
   structure". Its own row already declares it non-morphable ("a voice-lifecycle
   policy that morphed would change how long notes ring as you move the pad"). I
   read that as *structural*, not *device*: structural still morphs, atomically,
   which is exactly what a voice count should do. **Claim to check:** I searched
   all 158 continuous rows and found no other structure selector; 23 `beatMult` and
   148 `bendQTimeSync` are the near misses — both snap to `kGridSteps`, but they
   select a RATE, not a topology, so both stay morphable.
2. **89 `polyGlide` → DEVICE.** Stepped, so rule 2 would make it structural. ADR-102
   took it out of the DSP; nothing reads it and it exists only so stored state
   loads. A parameter no code reads is in no corner's gift. Device = "not part of
   the morph at all" is the honest class for a dead id.
3. **178 `specimen` → DEVICE.** Stepped; a GUI renderer toggle (ADR-140) with no
   audio path at all. Not in `morphIds`.
4. **161 `modEnvPitch` → DEVICE** as a "mod-matrix/route parameter" per the
   acceptance rule. Note the tension for the lead: §6.2/§6.3 make *routings*
   corner-scope atom-bundle members with per-routing promotion. So route DEPTH is
   device here as a phase-1 placeholder; when routings become first-class corner
   objects (phase 2/3) this id is the one that must be revisited, not silently
   inherited. ADR candidate 1 below.
5. **162–165 `penv*` (ENV 2 ADSR) → DEVICE** on §3.2's "global mod sources" clause:
   ENV 2 is a source, not a destination. Judgement, because a per-corner pitch
   envelope is a musically reasonable thing to want later.
6. **100 `masterVol` → DEVICE** — §3.2 names master volume as its own device
   example. Per-osc `vol` (17) stays morphable; it is the swarm's output gain.
7. **159 `morphArm` → DEVICE** on "anything that drives the morph itself" — an
   edit-routing mode, per ADR-109's own note.
8. **174–177 / 179–180 (XY → macro assignment) → DEVICE.** These assign which
   *intent* an axis writes. Under §3.2 an intent value is device; so is the wiring
   that decides which one a control writes. ADR-137 had already ruled all twelve
   out of the morph field.
9. **70 `inertiaCurve` stays MORPHABLE despite its "(dev)" label.** It is in
   `morphIds` by ADR-109 A1's deliberate ruling ("a corner that changes the swarm's
   weight changes its character more than most timbre knobs"). The acceptance's
   "dev toggles" clause is read narrowly — 264/265, 89, 178 — precisely so it
   cannot quietly evict a parameter a prior ADR put in the field. This is the case
   where the two halves of the rule pull opposite ways, and the morph field wins.

## The oracle

`tools/paramclass_check.cpp` — standalone, NOT wired into `./verify` (wiring a gate
is the human's ruling; ADR-171 is the route). It PRINTS the full table, then:

```
OK   T1a the table is the 243 frozen kParams rows
OK   T1b every host-exposed id carries a class (no -1)
OK   T2a the morph field was read (non-empty)
OK   T2b no morphIds member is device
OK   T2c CONTROL: the same scan over a lookup that calls id 33 device reports exactly that one violation
OK   T3 ids 151-158 (morph position + controls) are device
OK   T4 every per-osc twin shares its base id's class (82 twins; true by construction — they share one ParamDef)
OK   T4r REFUSAL: a global's +1000 twin is not a parameter and gets no class
OK   T5 every B49 FX group is type=structural, amount=morphable, tone=morphable
OK   T6 no id appears twice in the table
OK   T7 class numbers anchored: 4 Detune=morphable(0), 1 Voices=structural(1), 152 Morph X=device(2)

PASSED (0 failures)
```

T2c is the calibration half (L0032): a scan that silently examines nothing passes
exactly as loudly as a correct one, so the same scan is re-run against a lookup
that lies about one morph-field id and must report exactly that violation. T4r
pins a refusal (L0036): a global has no `+1000` twin, so its twin has NO class
(-1), not a defaulted one.

**Named coverage limit:** T6 sees the printed table, not `kParamClassOverrides`, so
a duplicated override id — shadowed by the first match, classification still a
function — reads as consistent. Stated, not silently uncovered.

## Alternatives rejected

- **A `ParamClass` column on `ParamDef`** — rejected above, with reasons.
- **Deriving the class from `morphIds` membership** — would have made the
  classification agree with the field by construction and turned T2b into a
  tautology (L0032). Rejected on exactly that ground.
- **Duplicating the class into `src/param_presentation.tsv`** — out of scope by the
  brief and wrong anyway: presentation is not definition, and a second copy drifts
  (L0005).
- **A second export for the reason string** — folded into one export with optional
  out-params; `hypersaw_debug_paramclass(id, &key, &reason)` takes no plugin
  handle, because a class that could differ between instances would not be a
  definition.

## Verify

`./verify full` — **exit 0**, every gate GREEN (`.harness/last-verify.json`:
`{"target":"full","exit":0,"git":"aaab057","ts":"2026-09-17T10:50:53Z"}` — the hash
is the pre-commit HEAD `aaab057`, the branch point). Acceptance (d)/(e) hold by
inspection of the working tree after the run: only `CMakeLists.txt`,
`src/hypersaw_clap.cpp` and the new `tools/paramclass_check.cpp` changed — no
golden regenerated, `state_check` / `statefix_check` untouched and GREEN, and
`params_get_info` was not touched, so the CLAP param info (flags included) is
byte-for-byte what it was.

## Open questions

1. **Is `voiceCull` (160) structural or device?** Judgement call 1. Structural reads
   §8 correctly; device matches its row's own word "non-morphable". Lead/human ruling.
2. **Does route depth (161) stay device once §6 routings are first-class?** It must
   be revisited at phase 2, not inherited.
3. **Should the eight macros be `device` or become the intent VALUES themselves?**
   Phase 2 replaces "macro writes a parameter" with "macro writes an intent"; at
   that point 166–173 may stop being parameters in this sense. Classifying them
   device now is correct and forward-compatible, but it is not the end state.
4. **Not wired into `./verify`.** Per the charter, a human ruling. Proposed in the PR.
5. **ROADMAP's phase-1 acceptance text is not present at `origin/main`** (grep for
   "Phase 1 acceptance" in `ROADMAP.md` at `aaab057` returns nothing). This work was
   done against the brief's verbatim quotation of it. The lead owns reconciling the
   two; ROADMAP is out of scope here.

## ADR candidates for the lead

- **A1** — parameter classes exist and are DERIVED, not a column: the rule
  (override → stepped/structural → continuous/morphable), the base-id keying, and
  the standing constraint that the derivation must never read `morphIds`.
- **A2** — the device set as ratified (89, 100, 151–159, 161–180, 264, 265) and
  `voiceCull` (160) as the single continuous §8 structural selector.
- **A3** — `hypersaw_debug_paramclass` as a handle-free export: the precedent that
  DEFINITION queries take no plugin instance, unlike every other `hypersaw_debug_*`.
- **A4** — whether `paramclass_check` joins `./verify full` (ADR-171's route).

## The full table

```
PARAMETER CLASSIFICATION (B89 phase 1 — QM-4 §3.2)
id     key              name                         class       reason
------ ---------------- ---------------------------- ----------- ------
1      n                Voices                       structural  stepped: cannot blend, resolves atomically
2      dist             Distribution                 structural  stepped: cannot blend, resolves atomically
3      seed             Seed                         structural  stepped: cannot blend, resolves atomically
4      detune           Detune                       morphable   continuous DSP value: blends inside its corner
5      law              Detune Law                   structural  stepped: cannot blend, resolves atomically
6      K                Pull K                       morphable   continuous DSP value: blends inside its corner
7      onset            Onset Lock                   morphable   continuous DSP value: blends inside its corner
8      dissolve         Dissolve (s)                 morphable   continuous DSP value: blends inside its corner
9      driftDepth       Drift Depth (c)              morphable   continuous DSP value: blends inside its corner
10     driftRate        Drift Rate                   morphable   continuous DSP value: blends inside its corner
11     inertia          Inertia                      morphable   continuous DSP value: blends inside its corner
12     rtone            R->Tone                      morphable   continuous DSP value: blends inside its corner
13     normExp          Density Comp                 morphable   continuous DSP value: blends inside its corner
14     width            Width                        morphable   continuous DSP value: blends inside its corner
15     mono             Mono Fold                    structural  stepped: cannot blend, resolves atomically
16     digital          Digital                      morphable   continuous DSP value: blends inside its corner
17     vol              Volume                       morphable   continuous DSP value: blends inside its corner
18     retrig           Retrigger                    structural  stepped: cannot blend, resolves atomically
19     attack           Attack (s)                   morphable   continuous DSP value: blends inside its corner
20     decay            Decay (s)                    morphable   continuous DSP value: blends inside its corner
21     sustain          Sustain                      morphable   continuous DSP value: blends inside its corner
22     release          Release (s)                  morphable   continuous DSP value: blends inside its corner
23     beatMult         Grid Cycles/Beat             morphable   continuous DSP value: blends inside its corner
24     topo             Topology                     structural  stepped: cannot blend, resolves atomically
25     reach            Ring Reach                   structural  stepped: cannot blend, resolves atomically
26     mu               Cluster Link                 morphable   continuous DSP value: blends inside its corner
27     alpha            Phase Lag                    morphable   continuous DSP value: blends inside its corner
28     poles            Poles q                      structural  stepped: cannot blend, resolves atomically
29     grav             Gravity                      morphable   continuous DSP value: blends inside its corner
30     basin            Basin (c)                    morphable   continuous DSP value: blends inside its corner
31     absK             Absolute K                   structural  stepped: cannot blend, resolves atomically
32     voiceMono        Mono                         structural  stepped: cannot blend, resolves atomically
33     glide            Note Lag (s)                 morphable   continuous DSP value: blends inside its corner
34     voiceLegato      Legato                       structural  stepped: cannot blend, resolves atomically
35     octave           Octave                       structural  stepped: cannot blend, resolves atomically
36     semi             Semitones                    structural  stepped: cannot blend, resolves atomically
37     fineCents        Fine (c)                     morphable   continuous DSP value: blends inside its corner
38     pitchBend        Pitch                        morphable   continuous DSP value: blends inside its corner
39     scatter          Phase Scatter                morphable   continuous DSP value: blends inside its corner
40     bassMono         Bass Mono                    structural  stepped: cannot blend, resolves atomically
41     bassMonoHz       Bass XOver (Hz)              morphable   continuous DSP value: blends inside its corner
42     panScatter       Pan Scatter                  morphable   continuous DSP value: blends inside its corner
43     engine           Engine                       structural  stepped: cannot blend, resolves atomically
44     partials         Partials                     structural  stepped: cannot blend, resolves atomically
45     tilt             Amp Tilt                     morphable   continuous DSP value: blends inside its corner
46     stretch          Stretch                      morphable   continuous DSP value: blends inside its corner
47     cloud            Cloud Voices                 structural  stepped: cannot blend, resolves atomically
48     cwidth           Cloud Width                  morphable   continuous DSP value: blends inside its corner
49     wtilt            Width Tilt                   morphable   continuous DSP value: blends inside its corner
50     wlaw             Width Law                    structural  stepped: cannot blend, resolves atomically
51     cascade          Cascade                      morphable   continuous DSP value: blends inside its corner
52     subOn            Sub Osc                      structural  stepped: cannot blend, resolves atomically
53     subVol           Sub Level                    morphable   continuous DSP value: blends inside its corner
54     subWave          Sub Wave                     morphable   continuous DSP value: blends inside its corner
55     subOct           Sub Octave                   structural  stepped: cannot blend, resolves atomically
56     balance          A/B Balance                  morphable   continuous DSP value: blends inside its corner
57     fx1type          FX1 Type                     structural  stepped: cannot blend, resolves atomically
58     fx1amt           FX1 Amount                   morphable   continuous DSP value: blends inside its corner
59     fx2type          FX2 Type                     structural  stepped: cannot blend, resolves atomically
60     fx2amt           FX2 Amount                   morphable   continuous DSP value: blends inside its corner
61     fx3type          FX3 Type                     structural  stepped: cannot blend, resolves atomically
62     fx3amt           FX3 Amount                   morphable   continuous DSP value: blends inside its corner
63     fx4type          FX4 Type                     structural  stepped: cannot blend, resolves atomically
64     fx4amt           FX4 Amount                   morphable   continuous DSP value: blends inside its corner
65     sAttack          S.Attack (s)                 morphable   continuous DSP value: blends inside its corner
66     sDecay           S.Decay (s)                  morphable   continuous DSP value: blends inside its corner
67     sSustain         S.Sustain                    morphable   continuous DSP value: blends inside its corner
68     sRelease         S.Release (s)                morphable   continuous DSP value: blends inside its corner
69     shape            Squareness                   morphable   continuous DSP value: blends inside its corner
71     toneTilt         Tone Tilt                    morphable   continuous DSP value: blends inside its corner
72     hiTame           Hi Tame                      morphable   continuous DSP value: blends inside its corner
73     driftMode        Drift Mode                   structural  stepped: cannot blend, resolves atomically
74     keepPhase        Keep Phase                   structural  stepped: cannot blend, resolves atomically
75     freqGlide        Freq Glide (s)               morphable   continuous DSP value: blends inside its corner
76     panMotion        Pan Motion                   morphable   continuous DSP value: blends inside its corner
77     panMode          Pan Motion Mode              structural  stepped: cannot blend, resolves atomically
78     motionCenter     Centre Pin                   morphable   continuous DSP value: blends inside its corner
79     harmReach        Harmonic Reach               morphable   continuous DSP value: blends inside its corner
80     stretchB         Stretch B                    morphable   continuous DSP value: blends inside its corner
81     spread           Octave Spread                morphable   continuous DSP value: blends inside its corner
82     anchor           Root Anchor                  morphable   continuous DSP value: blends inside its corner
83     pivotMode        Pivot                        structural  stepped: cannot blend, resolves atomically
84     panLayout        Pan Image                    structural  stepped: cannot blend, resolves atomically
85     panCurve         Fan Curve                    morphable   continuous DSP value: blends inside its corner
86     panInvert        Fan Invert                   structural  stepped: cannot blend, resolves atomically
87     superMode        Super-Width Mode             structural  stepped: cannot blend, resolves atomically
88     oversample       Oversample 2x                structural  stepped: cannot blend, resolves atomically
89     polyGlide        Poly Glide (dev)             device      (dev) not read by the DSP (ADR-102); state compat only
90     glideMode        Glide From                   structural  stepped: cannot blend, resolves atomically
91     onsetScatter     Onset Scatter (ms)           morphable   continuous DSP value: blends inside its corner
92     onsetAlpha       Timing Correction            morphable   continuous DSP value: blends inside its corner
93     attackScatter    Attack Scatter               morphable   continuous DSP value: blends inside its corner
94     voiceEnv         Per-Partial Env              structural  stepped: cannot blend, resolves atomically
95     relScatter       Release Scatter              morphable   continuous DSP value: blends inside its corner
96     fx1tone          FX1 Tone                     morphable   continuous DSP value: blends inside its corner
97     fx2tone          FX2 Tone                     morphable   continuous DSP value: blends inside its corner
98     fx3tone          FX3 Tone                     morphable   continuous DSP value: blends inside its corner
99     fx4tone          FX4 Tone                     morphable   continuous DSP value: blends inside its corner
70     inertiaCurve     Inertia Curve (dev)          morphable   continuous DSP value: blends inside its corner
100    masterVol        Master Volume                device      master volume — §3.2's own device example
101    gSemi            Pitch                        structural  stepped: cannot blend, resolves atomically
102    gFine            Fine                         morphable   continuous DSP value: blends inside its corner
103    gOct             Master Octave                structural  stepped: cannot blend, resolves atomically
104    oscMute          Mute                         structural  stepped: cannot blend, resolves atomically
105    oscSolo          Solo                         structural  stepped: cannot blend, resolves atomically
106    bendLaw          Bend Law                     structural  stepped: cannot blend, resolves atomically
107    bendTime         Bend Time (ms)               morphable   continuous DSP value: blends inside its corner
108    bendRate         Bend Rate (st/s)             morphable   continuous DSP value: blends inside its corner
109    bendTau          Bend Lag (ms)                morphable   continuous DSP value: blends inside its corner
110    bendSpringF      Spring (Hz)                  morphable   continuous DSP value: blends inside its corner
111    bendDamp         Damping                      morphable   continuous DSP value: blends inside its corner
112    bendDistOver     Distance Curve               morphable   continuous DSP value: blends inside its corner
113    bendReturn       Return x                     morphable   continuous DSP value: blends inside its corner
114    bendQuant        Bend Quantise                structural  stepped: cannot blend, resolves atomically
115    bendHyst         Quantise Hyst (c)            morphable   continuous DSP value: blends inside its corner
116    scaleRoot        Scale Root                   structural  stepped: cannot blend, resolves atomically
117    scaleDeg0        Degree 1 (root)              structural  stepped: cannot blend, resolves atomically
118    scaleDeg1        Degree b2                    structural  stepped: cannot blend, resolves atomically
119    scaleDeg2        Degree 2                     structural  stepped: cannot blend, resolves atomically
120    scaleDeg3        Degree b3                    structural  stepped: cannot blend, resolves atomically
121    scaleDeg4        Degree 3                     structural  stepped: cannot blend, resolves atomically
122    scaleDeg5        Degree 4                     structural  stepped: cannot blend, resolves atomically
123    scaleDeg6        Degree b5                    structural  stepped: cannot blend, resolves atomically
124    scaleDeg7        Degree 5                     structural  stepped: cannot blend, resolves atomically
125    scaleDeg8        Degree b6                    structural  stepped: cannot blend, resolves atomically
126    scaleDeg9        Degree 6                     structural  stepped: cannot blend, resolves atomically
127    scaleDeg10       Degree b7                    structural  stepped: cannot blend, resolves atomically
128    scaleDeg11       Degree 7                     structural  stepped: cannot blend, resolves atomically
129    sawBase          Saw Base                     morphable   continuous DSP value: blends inside its corner
130    sawProfile       Roundness Shape              morphable   continuous DSP value: blends inside its corner
131    round            Roundness                    morphable   continuous DSP value: blends inside its corner
132    roundHi          Round x Pitch                morphable   continuous DSP value: blends inside its corner
133    fx1mix           FX1 Mix                      morphable   continuous DSP value: blends inside its corner
134    fx2mix           FX2 Mix                      morphable   continuous DSP value: blends inside its corner
135    fx3mix           FX3 Mix                      morphable   continuous DSP value: blends inside its corner
136    fx4mix           FX4 Mix                      morphable   continuous DSP value: blends inside its corner
137    noteLawLink      Note Law                     structural  stepped: cannot blend, resolves atomically
138    noteLaw          Note Travel                  structural  stepped: cannot blend, resolves atomically
139    noteTime         Note Time (ms)               morphable   continuous DSP value: blends inside its corner
140    noteRate         Note Rate (st/s)             morphable   continuous DSP value: blends inside its corner
141    noteSpringF      Note Spring (Hz)             morphable   continuous DSP value: blends inside its corner
142    noteDamp         Note Damping                 morphable   continuous DSP value: blends inside its corner
143    noteDistOver     Note Distance Curve          morphable   continuous DSP value: blends inside its corner
144    noteQuant        Note Quantise                structural  stepped: cannot blend, resolves atomically
145    noteHyst         Note Quant Hyst (c)          morphable   continuous DSP value: blends inside its corner
146    bendQTimeMode    Step Timing                  structural  stepped: cannot blend, resolves atomically
147    bendQTimeHz      Step Rate (Hz)               morphable   continuous DSP value: blends inside its corner
148    bendQTimeSync    Step Grid                    morphable   continuous DSP value: blends inside its corner
149    bendMpeLaw       MPE Bend                     structural  stepped: cannot blend, resolves atomically
150    enable           Osc On                       structural  stepped: cannot blend, resolves atomically
151    morphOn          Morph                        device      morph control — the field cannot morph itself
152    morphX           Morph X                      device      morph position — §3.2: a source, not a destination
153    morphY           Morph Y                      device      morph position — §3.2: a source, not a destination
154    morphTemp        Temperature                  device      morph control — shapes the resolver itself
155    morphCoup        Coupling                     device      morph control — shapes the resolver itself
156    morphSeed        Morph Seed                   device      morph control — the field's flip topology
157    morphMode        Morph Mode                   device      morph control — how the field resolves
158    morphGlide       Morph Glide (s)              device      morph control — the field's own rate
159    morphArm         Edit Corner                  device      drives the morph — corner-edit arming (ADR-109)
200    fx1size          FX1 Size                     morphable   continuous DSP value: blends inside its corner
201    fx1spread        FX1 Spread                   morphable   continuous DSP value: blends inside its corner
202    fx1taps          FX1 Taps/Lines               structural  stepped: cannot blend, resolves atomically
203    fx1damp          FX1 Damping                  morphable   continuous DSP value: blends inside its corner
204    fx1noise         FX1 Noise                    morphable   continuous DSP value: blends inside its corner
205    fx1stereo        FX1 Stereo                   morphable   continuous DSP value: blends inside its corner
206    fx1dist          FX1 Spacing                  structural  stepped: cannot blend, resolves atomically
208    fx2size          FX2 Size                     morphable   continuous DSP value: blends inside its corner
209    fx2spread        FX2 Spread                   morphable   continuous DSP value: blends inside its corner
210    fx2taps          FX2 Taps/Lines               structural  stepped: cannot blend, resolves atomically
211    fx2damp          FX2 Damping                  morphable   continuous DSP value: blends inside its corner
212    fx2noise         FX2 Noise                    morphable   continuous DSP value: blends inside its corner
213    fx2stereo        FX2 Stereo                   morphable   continuous DSP value: blends inside its corner
214    fx2dist          FX2 Spacing                  structural  stepped: cannot blend, resolves atomically
216    fx3size          FX3 Size                     morphable   continuous DSP value: blends inside its corner
217    fx3spread        FX3 Spread                   morphable   continuous DSP value: blends inside its corner
218    fx3taps          FX3 Taps/Lines               structural  stepped: cannot blend, resolves atomically
219    fx3damp          FX3 Damping                  morphable   continuous DSP value: blends inside its corner
220    fx3noise         FX3 Noise                    morphable   continuous DSP value: blends inside its corner
221    fx3stereo        FX3 Stereo                   morphable   continuous DSP value: blends inside its corner
222    fx3dist          FX3 Spacing                  structural  stepped: cannot blend, resolves atomically
224    fx4size          FX4 Size                     morphable   continuous DSP value: blends inside its corner
225    fx4spread        FX4 Spread                   morphable   continuous DSP value: blends inside its corner
226    fx4taps          FX4 Taps/Lines               structural  stepped: cannot blend, resolves atomically
227    fx4damp          FX4 Damping                  morphable   continuous DSP value: blends inside its corner
228    fx4noise         FX4 Noise                    morphable   continuous DSP value: blends inside its corner
229    fx4stereo        FX4 Stereo                   morphable   continuous DSP value: blends inside its corner
230    fx4dist          FX4 Spacing                  structural  stepped: cannot blend, resolves atomically
160    voiceCull        Voice Cull                   structural  selects VOICE COUNT (continuous §8 selector)
161    modEnvPitch      Env > Pitch                  device      mod-matrix route depth — tiered by §6, not a corner value
162    penvA            P.Env Attack (s)             device      ENV 2 is a global mod source (§3.2)
163    penvD            P.Env Decay (s)              device      ENV 2 is a global mod source (§3.2)
164    penvS            P.Env Sustain                device      ENV 2 is a global mod source (§3.2)
165    penvR            P.Env Release (s)            device      ENV 2 is a global mod source (§3.2)
166    macro1           Macro 1                      device      macro = intent value (§3.2; ADR-137)
167    macro2           Macro 2                      device      macro = intent value (§3.2; ADR-137)
168    macro3           Macro 3                      device      macro = intent value (§3.2; ADR-137)
169    macro4           Macro 4                      device      macro = intent value (§3.2; ADR-137)
170    macro5           Macro 5                      device      macro = intent value (§3.2; ADR-137)
171    macro6           Macro 6                      device      macro = intent value (§3.2; ADR-137)
172    macro7           Macro 7                      device      macro = intent value (§3.2; ADR-137)
173    macro8           Macro 8                      device      macro = intent value (§3.2; ADR-137)
174    xyAsn0X          XY1 X > Macro                device      intent assignment — which macro an axis writes (ADR-137)
175    xyAsn0Y          XY1 Y > Macro                device      intent assignment — which macro an axis writes (ADR-137)
176    xyAsn1X          XY2 X > Macro                device      intent assignment — which macro an axis writes (ADR-137)
177    xyAsn1Y          XY2 Y > Macro                device      intent assignment — which macro an axis writes (ADR-137)
178    specimen         Specimen (CHROME-002)        device      GUI renderer toggle (ADR-140) — outside the audio path
179    mainAsnX         Main X > Macro               device      intent assignment — MAIN pad's axes (ADR-150)
180    mainAsnY         Main Y > Macro               device      intent assignment — MAIN pad's axes (ADR-150)
181    oscPitch         Pitch (cont.)                morphable   continuous DSP value: blends inside its corner
232    d1time           D1 Time (ms)                 morphable   continuous DSP value: blends inside its corner
233    d1sync           D1 Sync                      structural  stepped: cannot blend, resolves atomically
234    d1beats          D1 Beats                     morphable   continuous DSP value: blends inside its corner
235    d1offR           D1 R Offset                  morphable   continuous DSP value: blends inside its corner
236    d1fb             D1 Feedback                  morphable   continuous DSP value: blends inside its corner
237    d1cross          D1 Crossfeed                 morphable   continuous DSP value: blends inside its corner
238    d1damp           D1 Damp                      morphable   continuous DSP value: blends inside its corner
239    d1hp             D1 Loop HP (Hz)              morphable   continuous DSP value: blends inside its corner
240    d2time           D2 Time (ms)                 morphable   continuous DSP value: blends inside its corner
241    d2sync           D2 Sync                      structural  stepped: cannot blend, resolves atomically
242    d2beats          D2 Beats                     morphable   continuous DSP value: blends inside its corner
243    d2offR           D2 R Offset                  morphable   continuous DSP value: blends inside its corner
244    d2fb             D2 Feedback                  morphable   continuous DSP value: blends inside its corner
245    d2cross          D2 Crossfeed                 morphable   continuous DSP value: blends inside its corner
246    d2damp           D2 Damp                      morphable   continuous DSP value: blends inside its corner
247    d2hp             D2 Loop HP (Hz)              morphable   continuous DSP value: blends inside its corner
248    d3time           D3 Time (ms)                 morphable   continuous DSP value: blends inside its corner
249    d3sync           D3 Sync                      structural  stepped: cannot blend, resolves atomically
250    d3beats          D3 Beats                     morphable   continuous DSP value: blends inside its corner
251    d3offR           D3 R Offset                  morphable   continuous DSP value: blends inside its corner
252    d3fb             D3 Feedback                  morphable   continuous DSP value: blends inside its corner
253    d3cross          D3 Crossfeed                 morphable   continuous DSP value: blends inside its corner
254    d3damp           D3 Damp                      morphable   continuous DSP value: blends inside its corner
255    d3hp             D3 Loop HP (Hz)              morphable   continuous DSP value: blends inside its corner
256    d4time           D4 Time (ms)                 morphable   continuous DSP value: blends inside its corner
257    d4sync           D4 Sync                      structural  stepped: cannot blend, resolves atomically
258    d4beats          D4 Beats                     morphable   continuous DSP value: blends inside its corner
259    d4offR           D4 R Offset                  morphable   continuous DSP value: blends inside its corner
260    d4fb             D4 Feedback                  morphable   continuous DSP value: blends inside its corner
261    d4cross          D4 Crossfeed                 morphable   continuous DSP value: blends inside its corner
262    d4damp           D4 Damp                      morphable   continuous DSP value: blends inside its corner
263    d4hp             D4 Loop HP (Hz)              morphable   continuous DSP value: blends inside its corner
264    fxXfade          FX Type Crossfade (dev)      device      (dev) buried rack policy (B117/ADR-163)
265    fxXfadeMs        FX Crossfade Time (dev)      device      (dev) buried rack policy (B117/ADR-163)

base rows printed: 243   host-exposed ids: 325
kParams rows (the definition) — morphable 137  structural 73  device 33
all host-exposed ids (twins too) — morphable 191  structural 101  device 33

OK   T1a the table is the 243 frozen kParams rows
OK   T1b every host-exposed id carries a class (no -1)
OK   T2a the morph field was read (non-empty)
OK   T2b no morphIds member is device
OK   T2c CONTROL: the same scan over a lookup that calls id 33 device reports exactly that one violation
OK   T3 ids 151-158 (morph position + controls) are device
OK   T4 every per-osc twin shares its base id's class (82 twins; true by construction — they share one ParamDef)
OK   T4r REFUSAL: a global's +1000 twin is not a parameter and gets no class
OK   T5 every B49 FX group is type=structural, amount=morphable, tone=morphable
OK   T6 no id appears twice in the table
OK   T7 class numbers anchored: 4 Detune=morphable(0), 1 Voices=structural(1), 152 Morph X=device(2)

PASSED (0 failures)
```
