# Seam audit — 2026-09-26 (B276, phase 0 of B275)

**Audit date** 2026-09-26.
**Commit audited** `4b8346f` (`origin/main`, PR #766 merged). Every `file:line` below is at that hash.
**Cause** The lead's call, not the seven-day cadence: ROADMAP **B276** (the brief of record)
dispatched as phase 0 of **B275**, both carried in PR #767 (branch `lead-records-98`) and read
verbatim from `origin/lead-records-98:ROADMAP.md`.
**The human's words this serves** (B275): *"making variety bins explicit at all the seams; for
each seam in the system, there needs to be an explicit expectations contract. A new engine isn't
ready to integrate until it has: all its parameters sorted by morph status … its parameters
audited for integration with the mod matrix … (i.e. maximum viable modulation speed, etc.); its
relationship with the global parameters established, including the pitch bend system; its presets
system ready for integration; etc."*

**Oracle.** `./verify fast` GREEN at `4b8346f` in a clean worktree of this branch (target fast,
exit 0, `.harness/last-verify.json` `{"target":"fast","exit":0,"git":"4b8346f"}`), **with two gates
SKIPPED by construction in that worktree**: `private_name_gate` (no untracked `.leakcheck-names`
beside it) and `mailbox_delivery_check` (no sibling checkouts beside it). Run separately from the
main checkout, **`mailbox_delivery_check` is RED**: `FOUNDATIONS: brief-engine-manifest.md` — the
B275 brief exists in FOUNDATIONS' tree but not on their `origin/main` (their `git status` shows it
untracked). By this repo's own R9 rule that brief is a draft, not a filing (finding G20).

**Read-only.** Nothing outside this file and its trace was edited. No code was built, no audio
rendered, no probe run. Every claim is one of three kinds and says which:
**FROM CODE** (read at the cited line), **MEASURED** (a number some committed check or bench
produced, cited), or **PROPOSED** (this audit's recommendation). Nothing here proposes weakening a
gate. A change to a protected path (`./verify`, `specs/*`, `reference/*`) is labelled as a
sanction the human would have to give.

**Starting points.** `docs/playbooks/integrating-a-source.md` (B233; ten seams, written at
`e07acac`, amended at `6256301`), `docs/scalpel/ACCOUNTING.md` (B252), the code, `./verify`.
`playbook_check` reports **117 of 186** playbook citations drifted from their line (advisory) — so
the playbook's *anchors* hold but its *line numbers* mostly do not; this audit cites current lines.

---

## 0. Status vocabulary

| mark | meaning |
|---|---|
| **DECLARED** | The participant's behaviour at this seam is written down (a table, an override with a reason, a spec row, an ADR) **and** a check enforces it or the code provably agrees. |
| **IMPLICIT** | The behaviour exists and is consistent, but only by convention or by reading the code; nothing states it for this participant and nothing would fail if it changed. |
| **MISSING** | No behaviour, no decision. The participant is silent at this seam. |
| **VIOLATED** | A stated rule, ruling or ADR says one thing and the code does another. |
| **n/a** | The seam does not apply (e.g. pitch for a Gain slot). Used sparingly; always says why. |

For a contract **rule**, the three statuses of B276 are used: **ENFORCED** (a named check fails if
the rule breaks), **IMPLICIT** (convention; true today, unguarded), **UNSTATED** (nobody wrote it;
the code does *something*).

B199's **five limits** on modulation speed, used throughout: **L1** discontinuity (no smoothing, a
step clicks) · **L2** structurally uninterpolatable (a switch) · **L3** stability (fast coefficient
motion) · **L4** recompute cost per write · **L5** the control tick, `kGravGridSeconds = 256/44100`
= **5.805 ms = 172.27 Hz**, so nothing above **86.1 Hz** is representable (`src/swarm_core.h:136`).
B199 is a plan (ROADMAP B199, "REFRAMED DELIVERABLE"); **no per-parameter rate has been measured
for any destination**.

---

## 1. Headline

- **Seams: 19.** The playbook's ten, re-cut (morph and the matrix each split in two, "checks"
  folded into parity), plus **eight it missed**: the intent bus, global pitch, the voice/note
  model, tempo, the host automation surface, signal ports and routing, the FX slot contract, and
  inertia/interceptors (declared by two specs, built nowhere).
- **Scorecard, the human's four requirements, over the five ENGINES** (Swarm 1, Swarm 2, Sub,
  SCALPEL, STATION):

  | requirement | DECLARED | of which built | IMPLICIT | MISSING | VIOLATED |
  |---|---|---|---|---|---|
  | morph status | 4 | 3 (Swarm 1, Swarm 2, Sub) | 1 (STATION) | 0 | 0 |
  | mod-matrix readiness incl. max viable rate | 1 | **0** (SCALPEL on paper) | 3 | 0 | 1 (STATION spec) |
  | global parameters incl. pitch bend (and tempo) | 2 | 1 (Swarm 1) | 1 (STATION) | 0 | 2 (Swarm 2 tempo, Sub pitch) |
  | presets readiness | 2 | 2 (Swarm 1, Swarm 2) | 1 (SCALPEL) | 1 (STATION) | 1 (Sub, B255) |

  **No engine meets all four in built code.** Swarm 1 meets three (the rate half of the mod
  requirement is missing, as it is for everyone). **The Sub — the engine that exposed the problem —
  meets one (morph) and VIOLATES two** (global pitch; presets via the open B255). Over all 24
  participants (engines, nine FX modules, eight modulator rows, two filters): morph 11/23
  applicable DECLARED, mod incl. rate 3/24, global/pitch/tempo 7/12 applicable, presets 16/23
  applicable (full table §3).
- **Three defects the seam lens found that no prior row names** (all FROM CODE, not rendered):
  1. **Host tempo reaches oscillator 1 only.** `core.p.bpm = …` is written at
     `src/hypersaw_clap.cpp:8409` and `:8664`, and `core` is `cores[0]` (`:1701`). `cores[1].p.bpm`
     stays at its constructor default 120 (`src/swarm_core.h:169`), so Swarm 2's tempo-grid detune
     law (`law == 3`, `src/swarm_core.h:1809`) runs at 120 bpm whatever the host plays. (G3)
  2. **Global pitch is three private compositions, not a seam.** The swarm sums master octave/pitch/
     fine, bend, the matrix offset and `oscPitch` (`updateTune`, `:5283-5292`). The Sub takes the
     **raw MIDI key** (`subNoteOn` → `SubOscCore::noteOn(midi)`, `src/subosc_core.h:314-316`) plus its
     own offset (`:8600`). The Comb slot is tuned from the **raw key** too (`:8160-8163`). So master
     octave −1 drops the swarms and leaves the Sub and the Comb where they were. (G2)
  3. **The FX slot contract covers 7 of the rack's 10 type values.** `kSlotContract`
     (`src/fx_rack.h:64-72`) has rows Off..Notch (0..6); Echo 7, Room 8, Delay 9 (`:91-97`) have
     none, and `slotcontract_check` loops `for (int t = 1; t <= 6; t++)`
     (`tools/slotcontract_check.cpp:286`) — green because it never asks. (G6)
- **Two existing, known gaps the seam lens re-ranks upward**: a **preset load keeps the previous
  patch's routing matrix** (`initState` resets routing only for host chunks, `:6832`; pinned as
  deliberate-pending-ruling by `undo_check` S4, `tools/undo_check.cpp:2613-2619`; B193's ruling is
  still owed) (G7); and **68 non-Device globals are absent from the morph field with no recorded
  decision**, which `morphlayout_check` T10 cannot see because it scopes to per-osc and engine ids
  (`tools/morphlayout_check.cpp:406`) (G5).

---

## 2. The seam registry

Each seam: **Where** (file:line) · **(a)+(b)** the expectation contract as rules, each with its
enforcement status · **(c)** the variety bin: what it accepts and emits, and what happens outside.

### S1 — Identity: parameter ids, engine blocks, state keys

**Where.** `ParamDef` `src/hypersaw_clap.cpp:77`; `kParams` `:174` (266 rows); `defaultFor` `:784`;
`kOscStride = 1000` `:828`; `kGlobalIds` `:844`; `baseIdOf` `:919`; `EngineBlock` `:1204`;
`kSubOscParams` `:1243`; `subOscRowsAgreeWithCore` `:1326`; `kEngineBlocks` `:1352`;
`findEngineParamByKey` `:1377`; `findParam` `:1410`; routing ids `:976-1000`, `makeRoutingTable`
`:1088`; write/read dispatch `:7375`, `:7923`.

| # | rule (what a participant must provide / may assume) | status |
|---|---|---|
| 1.1 | Ids are append-only; never renumbered or reused; a dropped parameter is retired in place and still round-trips | **IMPLICIT** for renumbering (file header `:6`); **ENFORCED** only per retired id (`subosc_check` 11e for 4011). No check compares the id set against a frozen list. |
| 1.2 | A per-osc row's twin is `id + 1000`; a global is listed in `kGlobalIds` | **ENFORCED** (`paramscope_check` scope assertions; `paramclass_check` T4 twins share class) |
| 1.3 | An engine takes one thousand-id block in 3000..9999; `id − base` is the core's enum index below the gate | **ENFORCED** for the Sub (static_assert `:1326`, range static_assert `:1357`) |
| 1.4 | Range, step and default have ONE writer (the core table), mirrored by a compile-time proof | **ENFORCED** for the Sub (`:1326`); **n/a** for kParams (the row is the writer) |
| 1.5 | State keys are unique; engine keys carry a prefix (`sub.`) | **IMPLICIT** — no check for key collisions across `kParams`/cores (playbook §1(b) `toneTilt` incident) |
| 1.6 | Engine blocks stay under 35 rows until `baseIdOf` sites are guarded | **UNSTATED in code**; written only in the playbook §1(b). STATION (~100 rows by SPEC-STATION §10) would trip it. |
| 1.7 | Row count pinned | **ENFORCED** `paramclass_check` T1a = 266 (`tools/paramclass_check.cpp:170`); `subosc_check` 11d = 20 keys (`tools/subosc_check.cpp:1474`) |

**Variety bin.** Instrument ids 1..999 per osc (osc 1), 1001..1999 (osc 2); 2000..2999 reserved for
a third osc (`:836`); engines 3000..9999 (one block used, 4000..4019); routing 10000..22063
(29 exposed). `findParam` returns `nullptr` for any other id — **refuse, silently** (no log, no
error to the host). Keys: `<coreKey>`, `o1.<coreKey>`, `<prefix><coreKey>`, `rt.*`.

### S2 — Parameter class

**Where.** `ParamClass` rule `:1438-1490`; overrides `kParamClassOverrides` `:1500-1592` (base id,
ordered, each with a reason); `paramClassOf` `:1597`; engine gate branch `:1621-1650`.

| # | rule | status |
|---|---|---|
| 2.1 | Class is derived, never stored: override → Structural if stepped → else Morphable | **ENFORCED** `paramclass_check` (anchors one id per class) |
| 2.2 | Every Device row carries an override **with its reason** | **IMPLICIT** — the table has reasons; nothing fails on an empty reason |
| 2.3 | No Device row is a morph member | **ENFORCED** `paramclass_check` T2 with must-fire control T2c |
| 2.4 | An engine block's gate is Structural (B203) | **ENFORCED** by code `:1621`; **VIOLATED by a comment**: `EngineBlock::gateId` still reads "DEVICE class" (`:1209`) |
| 2.5 | Class ≠ membership | **IMPLICIT** — see S3 rule 3.4 for the 68-row consequence |

**Variety bin.** Three values {0 Morphable, 1 Structural, 2 Device}; 57 overrides today
(1 Structural, 56 Device), keyed on base ids ≤ 288. Routing cells: always Morphable. Anything
`findParam` rejects: `paramClassOf` returns false.

### S3 — Morph field: membership and positional order

**Where.** `kMorphL9OscIdEnd = 182` `:3061`; `kMorphTailIds` `:3063` (empty — the only append site);
`morphInit` `:3091`; hand-curated appends `:3103` (FX), `:3113` (bend/note laws), `:3124` (ADR-109 A1),
`:3132` (scale); engine passes `:3189-3236`; tail loop `:3239`; `morphSlotMap` `:6384`; layout marker
at `cornerJson` `:6222`, `:6346`, `morphJson` `:6510`, `tools/gen_factory_bank.cpp:456`;
`MorphCore::kMaxParams = 512` `src/morph_core.h:21`; frozen order `tests/morph_order.txt` (273 ids,
`layout 9`).

| # | rule | status |
|---|---|---|
| 3.1 | The whole layout-9 order is frozen; any insertion/removal/swap fails | **ENFORCED** `morphlayout_check` T13 (+T13e plants) — **but not in CI** (B256: does not link on Linux) |
| 3.2 | New members append through `kMorphTailIds` only, marker bumped at 4 writers, frozen into `tests/morph_order.txt` + `kLayoutPins` | **ENFORCED** T13/T13h; writers' agreement by `playbook_check` |
| 3.3 | Every non-Device **per-osc** and **engine** id is a member | **ENFORCED** T10 (`tools/morphlayout_check.cpp:406`) |
| 3.4 | Every non-Device **global** is a member, or its absence is a decision | **UNSTATED**. 68 non-Device globals are absent (computed from `kParams`, `kGlobalIds`, `kParamClassOverrides` and `tests/morph_order.txt`): 15, 40, 41, 88, 101, 102, 103, 160, the 28 time-engine rows 200–230, the 32 delay rows 232–263. None has a recorded reason. |
| 3.5 | An EXISTING parameter must not join without a load migration | **VIOLATED, known** — B255 (open): B195/B203 already did it; measured by the B240 critic |
| 3.6 | The quantum/intent draws are frozen at the layout-9 prefix | **ENFORCED** T14/T15 |
| 3.7 | The field fits the draw table | **ENFORCED** T13f (≤ 512) |

**Variety bin.** 273 slots today — 164 per-osc (82 rows × 2), 60 globals, 20 Sub, 29 routing
(computed from `tests/morph_order.txt` against `kGlobalIds`); capacity 512 (`kMaxParams`) — past it `pickCorner` indexes unguarded (playbook §3.1(b)),
T13f fails first. Layout markers accepted on load: absent = 1 (a 224-entry remap, ADR-159), ≥ 2
read 1:1 as a prefix (`:6384`); a **shorter** array fills the tail with defaults (B124), a
**longer** one — from a newer build — is truncated (UNSTATED; `morphSlotMap` reads only what the
live field holds).

### S4 — Morph resolution: slides, snaps, ramps; off-corner; hold; groups; exempt; adoption

**Where.** `morphStep` `:4498` (grid `kGravGridSeconds`); blend branch and pick branch
(`morphApplyTarget` `:4275`, stepped rounding `:4280`); gates `morphOnWeight` `:4342`,
`morphApplyOscEnable` `:4322`, `morphApplyGateEnable` `:4368`, `setEngineGateRamp` `:4289`; off-corner
rule `sourceGateOf` `:4408` and the law beneath it (ADR-183); ADR-108 hold `depLiveInCorner` `:3507`;
lead groups `:3256-3291`; exempt `morphToggleExempt` `:3335`; edit routing `morphRouteEdit` `:3646`;
adoption `morphAdoptUncontested` `:3618`, `cornerValuesAgree` `:3604`.

| # | rule | status |
|---|---|---|
| 4.1 | Continuous **slides** (bilinear in BLEND, picked-then-glided in QUANTUM); stepped **snaps** atomically; a source switch **ramps** as a level off the bilinear ON weight, flip deferred to weight 1e-3 | **ENFORCED**: ramp for the Sub gate by `morphlayout_check` T12; off-corner for swarms and Sub by `offcorner_check`; the osc-enable ramp's *audio continuity* (B48) has no dedicated row (playbook §3.4, still true) |
| 4.2 | A parameter's owning switch is derived: per-osc row → `150 + osc·1000`, engine row → its block's gate, global → none | **IMPLICIT** (`:4408-4415`); nothing declares it per parameter, so an FX slot's params (globals) have **no** owning switch — slot type Off does not gate its amount/tone/mix in a blend |
| 4.3 | Params that only mean something together share a lead group (scale; each FX slot's type+amount+tone; the routing block) | **IMPLICIT** — hand-built at `:3256-3291`; `routing_check` covers the routing group; nothing checks that FX **mix** (133–136) is deliberately outside its slot's group |
| 4.4 | ADR-108 hold: on the pick path a param not live in the winning corner keeps its value | **ENFORCED** `intent_check` T-G; **VIOLATED for engine rows by construction**: the depends tooling cannot name `sub.*` (playbook §3.8, still true), so the Sub's `shown_when` gates (`src/param_presentation.tsv:420-422`) never reach the hold |
| 4.5 | Exempt = member, live, all four corners hold the live value; absent ≠ exempt | **IMPLICIT** (ADR-109); no totality check for globals (S3 rule 3.4) |
| 4.6 | Editor morph-on adopts uncontested groups; host-automated morph-on keeps the old path | **ENFORCED** `undo_check` layer 5; the host path is pinned, not ruled (playbook §3.7) |
| 4.7 | A pure corner is bit-identical to its stored state; flag-off paths bit-identical to shipped | **ENFORCED** T12d, `routing_check` 11, `intent_check` S |
| 4.8 | Locks (pin a parameter to a corner) | **MISSING in the engine** — exists only in `docs/design/morph-editor-lab.html` (B269) |
| 4.9 | A circular parameter blends on the shortest arc | **MISSING** — no circular rule anywhere; `sub.bumpPhase` (±π) and `sub.phase` (a cycle) blend linearly today; SCALPEL Position needs it (ACCOUNTING §4.3, G3) |

**Variety bin.** Pad x, y ∈ [0,1] (152/153); temperature 0.02..4 (154); coupling 0..1 (155); morph
glide 0..5 s (158); tick 172.27 Hz; revision {1, 2} selects the blend law (`:6559`). Outside:
applyParam clamps (S7 rule on non-finite).

### S5 — Intent bus

**Where.** `kIntents = 10` `:4634`, stored order `:4640-4642`; storage `:4650-4700`; `intentInit`
`:4720`; `intentChunk` `:4822`; `intentStep` `:5010`; `intentApply` `:5100`; flag id 266 (dev, default
0, Device `:1556`); resolver `src/intent_core.h`.

| # | rule | status |
|---|---|---|
| 5.1 | Bindable = morph member (bindings are `[corner × intent × slot]`, keyed by id in the chunk) | **IMPLICIT** — membership inherited from S3; a non-member (the 68 globals) cannot be bound, and nothing says so |
| 5.2 | Bindings and ranges are **normalised** to the parameter's span | **IMPLICIT** (`:4630-4633`) — the same span-linear convention as mod depth; a log parameter gets a linear binding |
| 5.3 | Chunk keyed by id (not slot) and append-only intent order | **ENFORCED** by `intent_check`; the id keying makes S3 appends safe here |
| 5.4 | Atoms are lead groups; one seed per atom; draws frozen at layout 9 | **ENFORCED** T15 |
| 5.5 | Flag off ⇒ bit-identical to shipped | **ENFORCED** `intent_check` S |

**Variety bin.** 10 intents; bind/range ∈ [0,1] normalised (clamped at read, `src/intent_core.h`
`clamp01`); homes ∈ [0,1]²; captions sanitised (`intentSafeName`). The bus is **dev-flagged off**
(266 default 0), so today its bin is exercised by oracles only. Stale self-claim:
`src/intent_core.h:34-36` says "Not wired into the audio path … the shell seam is phase 2b" — it is
wired (`:4511`, `:5100`) (G16).

### S6 — Mod matrix: sources

**Where.** `ModCore::kMaxSources = 24`, `kMaxRoutes = 64`, scopes `src/mod_core.h:43-46`; polarity
`makeModCore` `:2726-2734`; per-tick writes `modStep` `:4040-4190`; GUI names
`src/gui/gui2.html:3174-3185`.

| # | rule | status |
|---|---|---|
| 6.1 | Slots append; retired slots (10–13) are never reused | **IMPLICIT** (comment at `:4169-4174`, gui2 `:3182-3184`) |
| 6.2 | A source declares polarity; unipolar is the default | **IMPLICIT** (`makeModCore`); `polarity_check` covers polarity *application*, not the table |
| 6.3 | The GUI's source list equals the shell's | **UNSTATED** — `MOD_SRC_NAMES` is a second copy no tool reads (`grep MOD_SRC_NAMES tools/` → nothing) |
| 6.4 | A per-note source is not projected globally | **IMPLICIT/partly VIOLATED** — ENV 1 is "max over voices" (`:4051-4058`), velocity/pressure are global projections (`:4175-4180`); B82 (per-note) is unbuilt |
| 6.5 | Sources are computed once per tick, deterministically | **ENFORCED** in part: `lfoenv_check` (B171); **VIOLATED, known**: B244 (S&H stream depends on the tick) |

**Variety bin.** 24 slots (22 used, 22–23 contested by B253/B180, cap full — ACCOUNTING §4.2a);
values: unipolar [0,1] or bipolar [−1,1] by declaration, **unclamped by the matrix** (a source that
emits 1.3 is summed as 1.3); rate: once per 172.27 Hz tick; scope: `kGlobal` only in the shell
(`kPerNote` exists in `mod_core.h:46`, unused). A route naming slot ≥ 24 (a newer build's patch) is
**refused and dropped on load** (ACCOUNTING §4.2a).

### S7 — Mod matrix: destinations, depth units, base readback, rate limits

**Where.** `modAddRoute` `:3723-3738`; destination loop `:4183-4226` (`want = base + delta·span`,
clamped, `:4204-4205`); `ModDest` `:2743`; `readParam` base-first `:7880-7896`; pitch route smoothing
`:4233` (8 ms one-pole); GUI mirror `modDestOptions` `src/gui/gui2.html:3335`, refusals `:3358`, `:3364`.

| # | rule | status |
|---|---|---|
| 7.1 | Any continuous parameter is a destination; stepped, 161–177 and 269–288 are refused | **ENFORCED** `mod_check` (refusals); the GUI's copy refuses **161–178** (`gui2.html:3358`), a disagreement harmless only because 178 is stepped |
| 7.2 | Depth is a fraction of the destination's full span, clamped to range | **ENFORCED** `mod_check` sum law; **UNSTATED per parameter** — log and circular rows get the same linear law (G11; B213 D6) |
| 7.3 | Readback, state, capture and adoption see the BASE, never the modulated value | **ENFORCED** `modreadback_check` §B (sweeps every destination the matrix accepts — enumerated, not listed) |
| 7.4 | A destination must have a GUI knob to be routable from gui2 | **IMPLICIT** (only `input[type=range]` controls are offered) |
| 7.5 | **Maximum viable modulation rate / smoothing class per destination** | **UNSTATED for every destination.** B199 is unbuilt. The only per-row smoothing record is ACCOUNTING §1.0/§1.1's reading (RB/KS/NO/U) for the swarm; STATION's core smooths 16 params at 5 ms (`src/station_core.h:181`); the Sub smooths none. |
| 7.6 | The same concept has the same smoothing (pitch) | **VIOLATED in spirit** (one rule, two implementations): the swarm's pitch route is 8 ms one-pole (`:4233`); the Sub's `pitchMod` (4019, whose comment says it exists so "the two pitch surfaces agree", `:1305-1311`) is applied as a raw step per 256-sample chunk (`:8600`) |
| 7.7 | A write's recompute cost is bounded | **UNSTATED** — the Sub re-runs `recalc()` (a 282-transcendental bump search, SPEC-SUBOSC §3) in **all 16** cores on **every** write (`:1977-1978`, `src/subosc_core.h:252`, `:528-545`) |

**Variety bin.** ≤ 64 routes; ~199 instrument destinations + 11 Sub + 29 routing (ACCOUNTING §4.1);
depth initial 0.25 (`:3738`), range per route unbounded by the core; applied value clamped to the
parameter's `[min, max]`; writes at ≤ 172.27 Hz, only when the value moves by > 1e-9 (`:4206`);
pitch route ±48 st (`:4231`). **Non-finite input: see S13 rule 13.4.**

### S8 — Global pitch

**Where.** Rows: per-osc `octave` 35, `semi` 36, `fineCents` 37, `oscPitch` 181; global `pitchBend`
38, `gSemi` 101, `gFine` 102, `gOct` 103, bend law 106–115, note law 137–145, quantise timing and
MPE bend law 146–149, scale 116–128, ENV 2 → pitch 161–165. State `:2514`; bend traveller
`bendGlide` `:2525-2535` (stepped at `:8735`); scale `Tet12ScaleState` `:2555`; composition
`updateTune` `:5283-5292`; per-voice note expression (ADR-162) via `noteExprSetPenv` `:4111`;
Sub pitch `SubOscCore::freqHz` `src/subosc_core.h:285` + `renderSubSpan` `:8563-8601`; Comb
`:8160-8163`.

| # | rule | status |
|---|---|---|
| 8.1 | "The wheel bends the patch" — bend, master transpose and the matrix pitch offset are global and reach every source | **DECLARED for the swarms** (ADR-082 list `:865`, `:2433`), **ENFORCED for both swarm cores** by `mpe_check` (fan-out, Goertzel on audio); **VIOLATED for the Sub** and **the Comb slot**, which read the raw key (`src/subosc_core.h:316`; `:8160`) |
| 8.2 | A new source states which pitch surfaces it follows | **UNSTATED in code**; written only in the playbook §4(a). The Sub states none; B196 (the human, 2026-09-21: "It should default to 'follow bend' like the others") is unbuilt |
| 8.3 | One scale ({root, mask}) is read by every consumer | **DECLARED** (`:2537-2554`); consumers today: bend lane and note lane only |
| 8.4 | Glide/travel laws are described once per lane (bend, note, MPE) | **VIOLATED by the Sub** — its private glide 4018 is the second copy B196 retires |
| 8.5 | Per-note pitch (MPE, ENV 2 route 0) is applied per voice through one composer (L0029, ADR-162) | **ENFORCED for swarms** (`penv_check`, `mpe_check`); the Sub's pitch has its own writer (`:8600`, "ONE writer" — of the Sub's two contributors, not of the instrument's) |
| 8.6 | Every pitch time constant is in seconds (ADR-009) | **ENFORCED** by `sr_check`/`samplerate_check` for the swarm; the Sub's glide granularity is the 256-sample chunk (`:8554-8557`), seconds-correct but not in any rate check beyond `subosc_check` |

**Variety bin.** Swarm pitch in semitones: 12·(oct+gOct) + semi + gSemi + bend + (fine+gFine)/100
+ matrix offset (±48) + oscPitch (±24) — unbounded sum, applied as a frequency ratio `2^(st/12)`
(`:5288`), capped downstream by the core. Sub: MIDI key + 12·octave(−3..0) + semis(±12) +
pitchMod(±48) + glide, then `fine` in cents; the phase increment is capped at 0.49·sr
(`src/subosc_core.h:355`). Keytrack off pins MIDI 36. Bend range ±12 st (38); scale 12-TET only
(named in the type).

### S9 — Voice and note model

**Where.** `kPoly = 16` `src/swarm_core.h:51`; `voiceMono` 32, `voiceLegato` 34, `glideMode` 90
(globals); mono stack and legato retarget `:8229-8292`, `retargetAll` `:2013`; Sub voices
`subs[kPoly]` `:1747`, own mono/bias `:1786`, `subMonoNoteOn` `:1817`; steal tiers (`steal_check`).

| # | rule | status |
|---|---|---|
| 9.1 | Every per-note and lifecycle operation reaches every oscillator | **ENFORCED** `mpe_check` (swarms), `notefuzz_check`, `kstuck_probe`, `steal_check` |
| 9.2 | Voicing (poly/mono/legato) is the shell's, global | **DECLARED** for swarms (ADR-082; ACCOUNTING §1.2 G1/G2); **VIOLATED by the Sub**, which has its own mono (4016) and bias (4017) and ignores 32/34 in poly; in swarm legato the Sub re-strikes (declared in a comment, `:2020-2029`) |
| 9.3 | Crossing a voicing boundary never leaves a stuck or orphaned note | **ENFORCED** for swarms (notefuzz); for the Sub, **by hard kill**: `if (v != subMono) subAllOff();` (`:1964`) → `allOff()` zeroes the envelope instantly (`src/subosc_core.h:334-340`) — a mid-note discontinuity, unmeasured |

**Variety bin.** 16 slots; notes 0..127; velocity clamped [0,1] (Sub, `src/subosc_core.h:317`);
velocity-0 note-on = note-off (`:8150-8158`); the Sub's mono stack is its own.

### S10 — Tempo and transport

**Where.** `core.p.bpm` from `CLAP_EVENT_TRANSPORT` `:8409` and `process()` `:8664`; consumers:
swarm tempo-grid law `src/swarm_core.h:1809`, LFO sync `:4135-4136`, bend quantise step grid
`:2610-2611`, rack `rack.setTempo(core.p.bpm)` `:8801`.

| # | rule | status |
|---|---|---|
| 10.1 | bpm is host-owned and global; every tempo consumer reads it (`:851-852`: "bpm stays host-owned and global; beatMult is the per-source ratio to it") | **VIOLATED** for Swarm 2: only `cores[0]` is written (G3) |
| 10.2 | No/zero tempo falls back to 120 | **IMPLICIT** (`bpm > 1 ? bpm : 120`, `:2610`, `:4135`) |
| 10.3 | Tempo sync names divisions | **MISSING** — LFO/delay "beats" are continuous 0.0625..8 (B213, B273) |

**Variety bin.** Any positive double the host sends, unclamped; a host with no transport leaves the
last value (default 120, `src/swarm_core.h:169`). Nothing covers tempo in any check (`mpe_check`
fans out note events, not transport).

### S11 — History (undo tree, branching)

**Where.** `historyJson` `:7090-7096` (lossless state + routing); `guiGesture` `:7133`;
`guiSetParam` `:7148`; `undoMarkParam` `:7154`; `undoService` `:7182`; `undoGoTo` → `applyStateJson`;
`UndoTree` caps `src/undo_tree.h:46` (200 nodes), `:56` (48 KiB reserve); gui2 `gestureFor`
`src/gui/gui2.html:3873`, `NO_HISTORY_IDS` `:3866`.

| # | rule | status |
|---|---|---|
| 11.1 | Every control marks exactly once | **ENFORCED** `gui_history_check` (874 scenarios) — enumerates `[data-p]` from the page, **but** runtime-built controls are covered only if their builder is on a hand list (`tools/labharness/gui_history_check.mjs:306-309`: `wireKnob`, `mxWire`, `buildOscSelectors`, `buildSubTab`); `buildScalePicker` (`gui2.html:7019`) is not on it (G19) |
| 11.2 | A restore is a load: node N gives N's state and N's audio by any path; a → b → a lands on a; b never mutates a | **ENFORCED** `undo_check` layer 5 (renders, L0063) |
| 11.3 | Host automation never marks | **ENFORCED** structurally (ADR-160 (3)) and by `undo_check` |
| 11.4 | Deliberately not restored: ensemble stream position, in-flight morph glide, master declick | **IMPLICIT** — named in B222, "still to be ruled" (playbook §5(b)) |

**Variety bin.** ≤ 200 nodes × a 48 KiB reserve each (≤ 9.4 MiB if full); worst snapshot measured
25,530 B (B222, MEASURED). Beyond 200 nodes the tree prunes (`kUndoCap`).

### S12 — Presets and state

**Where.** `stateJson` `:6678` (kParams, `o1.` twins `:6704-6711`, engine blocks `:6719-6725`,
morph chunk, `modRoutes`, `intent`, `presetName`); `initState` `:6787-6835`; `applyStateJson`
`:6842`; migrations keyed on JSON text (e.g. `:7029`); header `{schema, engine_revision, build}`
(`kEngineRevision = 2` `:6559`); host chunk `state_save` `:9220`, `state_load` `:9324`
(`initState(true)` `:9348`); queue `kQCap = 2048` `:2378`; oscillator-preset format
`src/osc_preset.h` (plugin wiring deliberately absent, `:9211-9218`); factory Init = empty table
(`tools/gen_factory_bank.cpp:131`).

| # | rule | status |
|---|---|---|
| 12.1 | A load is a load: absent key ⇒ default, via `initState` first, on every load path | **ENFORCED** for parameters (`bank_check` E, `statefix_check`, `state_check`); **VIOLATED for the routing matrix on the preset path**: `if (chunkOnlyState) applyRoutingChunk("");` (`:6832`) resets routing only for host chunks, and preset JSON carries no routing, so a preset load keeps the previous patch's matrix — **pinned as intended-pending-ruling** by `undo_check` S4 (`tools/undo_check.cpp:2613-2619`, B193/B222) (G7) |
| 12.2 | A sound-changing law lands behind `engine_revision` with the old law selectable | **ENFORCED** by `offcorner_check` (both revisions rendered) |
| 12.3 | Defaults are inert (parity-safe superset) | **ENFORCED** per feature (e.g. `subosc_check` 11g); **IMPLICIT** as a general rule |
| 12.4 | Positional data only appends (corner arrays, routes' source slots, enum labels) | **ENFORCED** for corners (T13); **IMPLICIT** for enums and source slots |
| 12.5 | Non-parameter state rides a chunk emitted only off-default, keyed by id, reset in `initState` | **IMPLICIT** (`ensembleChunk` `:3904`, `lfoChunk` `:3950`) |
| 12.6 | Every chunk writer fits its line | **UNSTATED** — `char line[80]` (`:9227`) truncates silently via `snprintf`; no check measures the longest key |
| 12.7 | Preset parts (oscillator / module / corner / patch) | **Oscillator tier: format only** (`preset_check` gates the format; no plugin wiring); **corner presets** exist; **module/engine presets: MISSING** for the Sub, FX modules (ADR-169 PROPOSED) and STATION (its spec asks, `specs/SPEC-STATION.md:162`) |
| 12.8 | An existing parameter joining the morph field loads old patches as saved | **VIOLATED, known** (B255) |
| 12.9 | Non-scalar state (tables, curves) has a carrier | **MISSING** — STATION's Wave RAM (`specs/SPEC-STATION.md` §5) has none; FOUNDATIONS' `kBlob`/`kCurve` (`registry.h`, their tree) is the natural carrier |

**Variety bin.** Preset JSON schema 3; host chunk `hypersaw-state 2`; revision {1,2} (absent ⇒ 1);
numbers `%.17g` in params, **`%.6g` in preset corners** (history is lossless, `:6512`); queue 2048
with a measured load peak of 1471 (playbook §6, MEASURED there, not re-measured here); a key the
loader does not know is **ignored**; a non-finite number parses (`std::atof`, `:9400`) and is then
clamped by `applyParam` to the parameter's **max** (S13 rule 13.4).

### S13 — Host automation surface

**Where.** `params_count` `:9032`, `params_get_info`, `params_value_to_text` `:9109`,
`params_text_to_value`; wrapper normalisation `libs/clap-wrapper/src/detail/vst3/parameter.cpp`
~123-130 (B254); `applyParam` clamp `:7301`.

| # | rule | status |
|---|---|---|
| 13.1 | A shipped range never changes (VST3 lanes normalise through min/max) | **VIOLATED, known**: 179/180 widened 0..8 → 0..10 on 2026-09-19 (B254); no check pins ranges |
| 13.2 | Host text shows the unit | **IMPLICIT** — a hand-written id list (`:9125-9174`) tests raw ids 8, 9, 10, 19, 20, 22, 23, 27, 33, 38, so **the osc-2 twins of the raw-id rows and every continuous engine row print bare `%.3f`**
(B219, B213) |
| 13.3 | Enumeration index is unstable, ids are stable | **IMPLICIT**; the comment at `params_count` says otherwise (playbook §12.8, still true) |
| 13.4 | Out-of-bin input is handled one way | **UNSTATED; two rules**: `applyParam` maps NaN to **maxV** (`std::max(min, std::min(max, NaN))` → `max`, `:7301`) — master volume 1.5, K +1 — while `SubOscCore::setParam` maps non-finite to the **default** (`src/subosc_core.h:248`), but never sees NaN because the shell clamps first. No gate feeds a non-finite parameter value (`robustness_matrix` checks non-finite *output*). (G10) |

**Variety bin.** Every declared id, `[min, max]` as declared; stepped values rounded (`:7330`);
enum text resolved through the label array.

### S14 — Presentation and GUI generation

**Where.** `src/param_presentation.tsv` (368 rows; header `:1-69`, column row `:70`); generator
`tools/gen_gui_controls.py`; `presentation_check`, `depends_check`, `gen_gui_controls --check`,
`gui_reach` (`tools/gui_reach.py:28`: ids < 1000 only), `compact_lab_table_check`.

| # | rule | status |
|---|---|---|
| 14.1 | One row per declared address | **ENFORCED** `presentation_check` — **except routing ids**, which have no rows and are not counted (B243) |
| 14.2 | Every declared param is reachable | **ENFORCED for ids < 1000 only** (B243) |
| 14.3 | `depends` is the single declaration for GUI and morph hold (ADR-108) | **VIOLATED, known**: engine keys cannot be named; `,` means AND to the GUI and OR to the engine (playbook §12.1–2, still true) |
| 14.4 | Units and tapers live in one place | **VIOLATED, known** (B213: three places) |
| 14.5 | The compact lab's embedded table matches | **ENFORCED** on 8 columns; ranges/defaults/enums not checked |

**Variety bin.** Scopes {global, osc1, osc2, sub}; 34 undesigned rows, 5 ungrouped (the gate's own
report); `scale log10` needs min > 0.

### S15 — Real-time safety, time constants and cost

**Where.** Charter invariants; `rtsafety_probe` (ids 1..99 `tools/rtsafety_probe.cpp:133`, Sub on
`:115`, no morph, no route); `playbook_check` clock/RNG scan; `sr_check`, `samplerate_check`,
`blocksize_check`, `subdiv_check`; benches `tools/measure_cpu.cpp`, `tools/measure_fx.cpp`
(B262), `tools/measure_modsources.cpp` (B236).

| # | rule | status |
|---|---|---|
| 15.1 | No allocation, lock, clock or provider call on the audio thread | **ENFORCED** inside the probe's window; the window excludes `morphStep`, `intentStep` and the destination path (B242, open) |
| 15.2 | Time constants are seconds, converted per rate (ADR-009) | **ENFORCED** for the swarm (`sr_check`, `samplerate_check`); **VIOLATED** in `DelayCore` (`src/delay_core.h:149-150`) and `TimeCore` (`src/time_core.h:127`): a hand-tuned `0.0015` per-sample retime slew, τ = 666 samples = 15.1 ms at 44.1 kHz and 7.6 ms at 88.2 kHz (arithmetic). TimeCore is a lab port (parity may require it; an ADR then); DelayCore is spec-as-contract (ADR-142) and reuses it "deliberately" (G13) |
| 15.3 | Only mulberry32, seeded from the patch | **ENFORCED** `playbook_check` scan (28 engine sources), `tseed_check` |
| 15.4 | Per-parameter write cost is known | **MISSING** (the Sub's recalc ×16 per write; G8) |
| 15.5 | A module's cost is measured | **MEASURED** for the nine FX types (B262: per slot at 44.1 kHz, % of one core, default/worst — Drive 0.036/0.037, Filter 0.009, Gain 0.002, Comp 0.020/0.021, Comb 0.023/0.064, Notch 0.214/0.221, Echo 0.337/0.405, Room 0.486/0.671, Delay 0.036/0.037) and the swarm (MEASUREMENTS §2); **estimated only** for SCALPEL (ACCOUNTING §5) |

**Variety bin.** Sample rate any (the Sub falls back to 44.1 kHz on a non-finite rate,
`src/subosc_core.h:232`); block any size; the tick is 256 samples at 44.1 kHz and seconds-derived
elsewhere; oversampling 2× is the swarm's (88); the Sub renders at the base rate (`renderSubSpan` reads
`sampleRate`, `:8565`).

### S16 — Parity, oracles and check wiring

**Where.** `./verify` fast/full; `tools/test_table_check.py` (wired-or-explained, ADR-180 §1: 68
checks WIRED, 1 UNWIRED, each verified); per-engine chains (`verify:580-582` Sub, `:568-570` STATION,
`:335-337` SCALPEL's reference battery).

| # | rule | status |
|---|---|---|
| 16.1 | Correctness = parity with the JS reference (ε 1e-6) plus invariant rows, each with a must-fail control | **ENFORCED** per ported engine (swarm, Sub, STATION core, SPECTRA, glide, time, filter, notch, swarmalator, intent) |
| 16.2 | A check is wired or states why not | **ENFORCED** `test_table_check` |
| 16.3 | A shell-path oracle exists beside the core oracle (L0031) | **ENFORCED** for the Sub (`subosc_check` §11 via the plugin); **MISSING** for STATION (no shell) and the placeholder FX (Drive/Filter/Gain have `slotcontract_check` only) |
| 16.4 | A file's claim about its own wiring is true | **VIOLATED** by four stale self-claims: `src/intent_core.h:34-36`, `src/glide_core.h:25`, `verify:459-461` (routing "NOT yet in the audio path") and `verify:586-588` (glide "NOT in the audio path yet") — all four cores run in `process()` (`:8824`, `:8735`, `:5100`). The `verify` lines are comments; editing them is a sanction, not a weakening (G16) |

### S17 — Signal ports and routing (sources → per-voice → matrix → FX → output)

**Where.** Source buffers `srcBufL/R[kRoutingNSrc − 1]` (`:2341`; buffer index = source − 1,
source 0 renders straight into the output buffers, `:8817-8822`), so the Sub's accumulate into `srcBufL[1]` (`:8620`) is source 2;
routing `RoutingMatrixT routing` `:2319`, `processBlock` `:8824`; layout `:920-1000` (rows 0..7
reserved for sources, 3 used: `kRoutingNSrc = 3` `:976`); bass-mono stage per source
(`bmIc1[kRoutingNSrc]`, `:5273`) and post; Sub headroom `kSubRowHeadroom = 0.7018` (−3.07 dB)
`:8538`; per-voice tap (ADR-148, `voicetap_check`); per-voice SVF core `src/svf_core.h` (B81
increment 2, not in the shell).

| # | rule | status |
|---|---|---|
| 17.1 | A source takes a reserved row; adding one moves no id | **ENFORCED** `routing_check` dispatch probe; ids of a new row must be tail-listed for morph (S3) |
| 17.2 | Acyclicity on the read side, always | **ENFORCED** `routing_check` (calibrated) |
| 17.3 | A source declares its headroom/peak | **IMPLICIT** — the Sub's is measured and named (`:8530-8538`); the swarms' is not stated as a number |
| 17.4 | Every load resets or restores the routing | **VIOLATED on the preset path** (S12 rule 12.1, G7) |
| 17.5 | Per-voice processing between source and matrix | **IMPLICIT** — the tap is bit-identical when bypassed (`voicetap_check`); the SVF has no shell seam yet |

**Variety bin.** Coefficients −2..2, outs 0..2, inits −1..1 (`:1135-1160`); 29 exposed forward
cells; `NSRC + NSLOT ≤ 32`; stereo float buffers of `kMixChunk = 256`. The `source − 1` buffer offset is a second
coordinate convention beside `routingIndexOfRow` (`:920-1000`); both are hand-maintained.

### S18 — FX slot contract (the module-level promise)

**Where.** `SlotContract` `src/fx_rack.h:54-60`; `kSlotContract` `:64-72`; `FxType` `:82-99`;
`kRackSlots = 4` `:74`; labels `src/hypersaw_clap.cpp:156-157`; `slotcontract_check`
`tools/slotcontract_check.cpp:283-300`.

| # | rule | status |
|---|---|---|
| 18.1 | Every slot type declares identity point, dry-blend meaning, image/level effects, latency | **ENFORCED for 6 types** (Drive..Notch); **MISSING for Echo, Room, Delay** — no rows; indexing `kSlotContract[7..9]` would read past the array (no caller does today) |
| 18.2 | `mix = 0` is a bit-exact bypass for every type | **ENFORCED for 6 types**, never asked of the other three (`t <= 6`, `:286`) |
| 18.3 | Coverage enumerates from the declaration | **VIOLATED** — a hand bound (`6`) and a hand name list (`:284`) beside an enum that grew to 9 |

**Variety bin.** Types 0..9 (`kFxTypeLabels`, 10 labels); amount/tone/mix 0..1; 4 slots.

### S19 — Inertia and interceptors (a house tenet with no seam)

**Where.** Declared by `specs/SPEC-SUBOSC.md:238` ("`fine` and `tone` are inertia-eligible") and
`specs/SPEC-STATION.md` §9.1 (RATIO/PITCH/FIXED and PW "mass-slewed"); FOUNDATIONS §3.1
"Interceptors … glide inertia (smoothing)". The shell has `GlideCore` for the bend and note lanes
(`:2525`) and the swarm's own `inertia` (11) — **no generic per-parameter interceptor**.

| # | rule | status |
|---|---|---|
| 19.1 | An inertia-eligible parameter is mass-slewed | **MISSING** — two specs declare eligibility; nothing implements or checks it |

---

## 3. The scorecard

Rows are every participant; columns are seams. **Table A carries the human's four requirements**;
Table B the rest. One line of evidence per cell; the seam sections above hold the detail.
SCALPEL is scored **from `docs/scalpel/ACCOUNTING.md`** (a proposal: "on paper"); STATION is
scored for completeness from `specs/SPEC-STATION.md` and `src/station_core.h` (parked, ADR-182).

### Table A — the four headline requirements

| participant | morph status (S3/S4) | mod-matrix readiness incl. max viable rate (S6/S7) | global params incl. pitch bend, tempo (S8/S9/S10) | presets readiness (S12) |
|---|---|---|---|---|
| **Swarm 1** | **DECLARED** — class derived `:1597`; every non-Device per-osc id a member (T10); order frozen (T13); switch 150 ramps + off-corner (`offcorner_check` §4) | **IMPLICIT** — continuous ⇒ destination `:3723`, base readback (`modreadback_check` §B); depth span-linear for all; **rate UNSTATED** (B199 unbuilt) | **DECLARED** — `updateTune` `:5283` sums 103/101/102/38/matrix/181; fan-out `mpe_check`; tempo reaches it (`:8409`) | **DECLARED** — every key in `stateJson` `:6678`; absent ⇒ default (`initState`); revision gate `:6559`; `bank_check` E, `statefix_check` |
| **Swarm 2** | **DECLARED** — twins +1000, gate 1150 (same checks) | **IMPLICIT** — as Swarm 1; B241 fixed twin readback | **VIOLATED** — host tempo never reaches `cores[1]` (`:8409`, `:8664`; `src/swarm_core.h:169`), so law 3 runs at 120 bpm; pitch otherwise as Swarm 1 | **DECLARED** — `o1.` keys `:6704-6711`; ships OFF by `defaultFor` `:786` |
| **Sub** | **DECLARED** — all 20 ids members (T10), gate 4015 ramps (T12), off-corner via its gate (`:4410`); its `shown_when` gates never reach the ADR-108 hold (S4 4.4) | **IMPLICIT** — 11 continuous rows accepted; **no smoothing on any row**, every write re-runs `recalc()` in 16 cores (`:1977-1978`); `pitchMod` a raw step (`:8600`) | **VIOLATED** — raw key at note-on (`src/subosc_core.h:316`); ignores 101–103, bend 38/106–115 (B196 ruled "follow bend", unbuilt), note law, scale, MPE, ENV 2; own voice model 4016/4017 | **VIOLATED** — B255 (open): pre-B203 morph-on patches load with `sub.on` 0 (MEASURED by the B240 critic); otherwise 20 keys (`subosc_check` 11d); no module preset |
| **SCALPEL** (candidate) | **DECLARED on paper** — ACCOUNTING §1.4 class + blends/snaps per row, tail-list append (§G1); open: circular Position (G3), blade switches as gates (G2) | **DECLARED on paper** — §4.1 45 destinations/osc, §4.3 octave and wrap units, smoothing class P/C/B per row, P rows −3 dB at 13 Hz (arithmetic) | **DECLARED on paper** — §1.2 G1–G4 fold voicing/glide into 32/33/34/90; rows inherit `updateTune` as per-osc rows | **IMPLICIT** — inert defaults required (§1.7); 76 packet presets need K re-voicing and a ½-cycle phase map (Q B3, C2 open) |
| **STATION** (parked) | **IMPLICIT** — spec §9.2 names cells + levels as the surface; no ids, no block, no class table; ~100 rows exceed the 35-row `baseIdOf` guard (playbook §1(b)) | **VIOLATED** — `op{n}.qnt` is a "stepped mod target" (`specs/SPEC-STATION.md:180`) but the shell refuses stepped destinations (`:3726`); 5 ms smoothing on 16 params declared and built (`src/station_core.h:181`) | **IMPLICIT** — mono/legato/glide deferred to horde (`specs/SPEC-STATION.md:152`); bend and master transpose unstated; its own pitch envelope `penv.amt` ±24 st | **MISSING** — per-op/engine scoped presets asked (`:162`), Wave RAM tables need an array carrier; no mechanism |
| FX **Drive** | **IMPLICIT** — type/amount/tone/mix members by hand list (`:3103-3105`); atomic group (B49); no owning switch for off-corner | **IMPLICIT** — amount/tone/mix destinations; rate unstated | n/a — no pitch or tempo input | **DECLARED** — patch keys + corner slots |
| FX **Filter** | **IMPLICIT** — as Drive | **IMPLICIT** | n/a | **DECLARED** |
| FX **Gain** | **IMPLICIT** — as Drive | **IMPLICIT** | n/a | **DECLARED** |
| FX **Comp** | **IMPLICIT** — as Drive | **IMPLICIT** | n/a | **DECLARED** |
| FX **Comb** | **IMPLICIT** — as Drive | **IMPLICIT** | **MISSING** — tuned from the raw key (`:8160-8163`); ignores transpose, bend, glide; nothing states it | **DECLARED** |
| FX **Notch** | **IMPLICIT** — as Drive | **IMPLICIT** | n/a | **DECLARED** |
| FX **Echo** | **MISSING** — its per-slot rows (200–230, shared with Room) are absent from the field, no ruling | **IMPLICIT** — destinations; retime slew `0.0015`/sample (`src/time_core.h:127`) | n/a (no sync) | **IMPLICIT** — patch keys only; corners cannot hold its settings |
| FX **Room** | **MISSING** — as Echo | **IMPLICIT** — as Echo | n/a | **IMPLICIT** — as Echo |
| FX **Delay** | **MISSING** — 32 rows 232–263 absent, no ruling | **IMPLICIT** — destinations; retime `0.0015`/sample (`src/delay_core.h:149-150`) | **DECLARED** — sync reads host tempo via `rack.setTempo` (`:8801`, ADR-142) | **IMPLICIT** — as Echo |
| **ENV 1** (slot 0) | **DECLARED** — its controls are the per-osc amp envelope 19–22, members | **IMPLICIT** — max over voices (`:4051-4058`), unipolar; B229 redesign pending | n/a | **DECLARED** |
| **ENV 2** (slot 1, pitch) | **DECLARED** — Device 162–165 (`:1524-1527`), never | **DECLARED** — ADR-162: per voice on route 0 (st), global max otherwise | **DECLARED** — per-voice pitch through the composer (`:4111`) — **swarms only** | **DECLARED** |
| **LFO 1** (slot 18) | **DECLARED** — Device 269–274 | **VIOLATED** — B244 (open): S&H stream depends on the tick | **DECLARED** — sync reads host bpm (`:4135`) | **DECLARED** — params + `lfoChunk` (`:3950`) |
| **LFO 2** (slot 19) | **DECLARED** — Device 275–280 | **VIOLATED** — B244 | **DECLARED** — as LFO 1 | **DECLARED** |
| **ENV 3** (slot 20) | **DECLARED** — Device 281–284 | **IMPLICIT** — unipolar, global | n/a | **DECLARED** |
| **ENV 4** (slot 21) | **DECLARED** — Device 285–288 | **IMPLICIT** — as ENV 3 | n/a | **DECLARED** |
| **Macros 1–8** (slots 2–9) + pad assignment 174–180 | **DECLARED** — Device (ADR-137), reasons in the table | **DECLARED** — slots 2–9, unipolar, captions in the intent chunk | n/a | **DECLARED** — values + captions |
| **MIDI sources** (slots 14–17) | n/a — not parameters | **IMPLICIT** — global projections (`:4175-4180`); per-note (B82) unbuilt | **DECLARED** — pitch wheel bipolar (`:2730`) | n/a — performance signals |
| **Per-voice SVF** (B81 inc. 2) | **MISSING** — no ids | **MISSING** | **MISSING** — key tracking unstated | **MISSING** |
| **Bass-mono stage** (40/41/267) | **MISSING** — 40 and 41 absent with no ruling; 267 Device with a reason (B146) | **IMPLICIT** — 41 is a destination | n/a | **DECLARED** — patch keys |

**Counts (Table A).** Morph: 11 DECLARED of 23 applicable (all four engines that are built or
proposed, all modulators; **no FX module**). Mod incl. rate: 3 of 24 (SCALPEL on paper, ENV 2,
Macros) — **zero participants have a measured maximum rate**. Global/pitch/tempo: 7 DECLARED of 12
applicable (Swarm 1, SCALPEL, Delay, ENV 2, LFO 1, LFO 2, MIDI). Presets: 16 of 23 applicable.

### Table B — the other seams

Codes: **D** DECLARED · **I** IMPLICIT · **M** MISSING · **V** VIOLATED · **–** n/a.

| participant | S1–2 ids/class | S5 intent | S11 history | S13–14 host + GUI | S15 RT & cost | S16 parity/oracle | S17 routing/ports | S18 slot contract |
|---|---|---|---|---|---|---|---|---|
| Swarm 1 | D — `kParams`, T1a 266 | I — bindable as members; bus dev-off | D — `gui_history_check` | I — hand text list raw ids (`:9125-9174`) | D — `rtsafety_probe` ids 1–99, MEASUREMENTS §2 | D — `parity_check` + SAW suite | D — row 0 | – |
| Swarm 2 | D | I | D | **V** — raw-id text branches skip twins (B219) | I — probe sweeps ids 1–99 only (`tools/rtsafety_probe.cpp:133`) | D | D — row 1 | – |
| Sub | D — static_assert `:1326`, 11.0 dispatch | I | D — `buildSubTab` lifted | I — engine ids outside `gui_reach` (`tools/gui_reach.py:28`); bare `%.3f` host text | I — probe toggles 4015 (`:115`) but no route or morph (B242); write cost unmeasured | D — goldens + `subosc_check` + lab check | D — row 2, −3.07 dB headroom | – |
| SCALPEL | D on paper — 300–399, `bl.` prefix | I | I — generated controls would be covered | D on paper — tiers T1–T3 (§2) | **M** — estimate only (§5) | D — reference battery wired (`verify:335-337`); port parity bounded by §1.6 | I — rides rows 0/1 | – |
| STATION | **M** — 3000 block reserved only | M | M | M | D (core) — smoothing, no alloc; CPU reported | D — `station_check` + lab check | **M** — no row | – |
| Drive, Filter, Gain | D | I — slot atoms of 3 | D | I | D — MEASURED (B262) | I — `slotcontract_check`, `fxxfade_*` only; placeholders have no reference | **V** — preset load keeps the previous matrix (G7) | D |
| Comp, Comb, Notch | D | I | D | I | D — MEASURED (B262) | D — Comb (lab port), Notch `notch_check` + `notchslot_check` | **V** — G7 | D (Comp, Notch carry pinned violations: no identity point) |
| Echo, Room | D | M — rows not members | D | I | D — MEASURED; **V** ADR-009 slew (G13) | D — `time_check` | **V** — G7 | **M** — no contract row |
| Delay | D | M — rows not members | D | I | D — MEASURED; **V** ADR-009 slew | D — `delay_check` (spec-as-contract) | **V** — G7 | **M** — no contract row |
| ENV 1–4, LFO 1–2 | D — Device overrides with reasons | – | D | I — LFO beats continuous (B213) | I — tick-bound (L5) | D — `lfoenv_check`, `penv_check` | – | – |
| Macros / pads | D | D — the intents themselves | D | **V** — 179/180 range widened after shipping (B254) | I | I — `intent_check` S | – | – |
| MIDI sources | – | – | – | – | I | D — `mpe_check` | – | – |
| Per-voice SVF | M | M | M | M | D (core) | D — `svf_check` | I — `voicetap_check` seam, unused | – |
| Bass-mono | D — 267 Device (B146) | M | D | I | D | I — `blocksize`/`sr` indirect | D — per source + post | – |

---

## 4. A worked manifest: the SUB (the first real one)

Every field is tagged **[C]** FROM CODE (cited) or **[P]** PROPOSED (this audit). Where a field
cannot be established, it says so. Column meanings: *morph* = slides (continuous blend) / snaps
(stepped, atomic) / ramps (level ramp on the switch) / never; *scope* = where the value lives
(per-corner member, exempt-able, or absent); *rate* = the binding limit among L1–L5 (§0) and the
proposed ceiling.

### 4.1 Engine-level declarations

| field | value | tag |
|---|---|---|
| block | ids 4000..4019, prefix `sub.`, module "SUB OSC", gate 4015 (`:1352`) | [C] |
| core table | `SubOscCore::kParamTable` is the only writer of range/step/default for 4000..4014 (`src/subosc_core.h:151`), proven at compile time (`:1326`) | [C] |
| shell rows | 4015 gate, 4016..4019 above it (`:1317`) | [C] |
| voice model | poly: one core per swarm voice slot, struck from the swarm's allocation (`:8174`, `:8288`); mono: its own held stack, bias lowest/highest/last (4016/4017); swarm legato ⇒ the Sub re-strikes (`:2020-2029`) | [C] |
| voice model, proposed | follow `voiceMono`/`voiceLegato` (32/34) like every other source; retire 4016/4017 in place, or keep them as an explicit *override* declared as such | [P] |
| pitch input | raw MIDI key at note-on + 12·octave + semis + `pitchMod` + mono glide, then `fine` (`src/subosc_core.h:285-292`, `:8600`); keytrack off ⇒ MIDI 36 | [C] |
| pitch, global surfaces followed | **none**: not 101/102/103, not 38 or its law 106–115, not 137–149, not the scale, not MPE per-note bend, not ENV 2's route | [C] |
| pitch, proposed | consume the one published per-voice pitch (G2): follow master transpose and the bend lane by default (B196's "follow bend"), per-note MPE and the scale when the lane quantises; keep octave/semis/fine as the Sub's own offset | [P] |
| tempo | none (no tempo-dependent row) | [C] |
| routing | source row 2; `srcBufL[1]` (`:8620`); headroom 0.7018 = −3.07 dB (`:8538`) | [C] |
| cost | render MEASURED as a reported (ungated) row in `subosc_check`; **per-write cost not measured**: every write to a core row runs `recalc()` — one `tan`, two divides and the BUMP peak search (282 transcendentals, SPEC-SUBOSC §3) — in all 16 cores (`:1977-1978`) | [C] |
| cost, estimated | one routed Sub row at the 172.27 Hz tick: 16 × ~283 × 172.27 ≈ 7.8·10⁵ transcendental calls/s. At an assumed 10–20 ns each (**not measured on this Mac**) that is 7.8–15.6 ms per second, **0.8–1.6 % of one core per routed row** — against B262's MEASURED ≈ 1.4 % for a whole worst-case 4-slot FX rack | [P] |
| adaptive state | none — declared absent (`specs/SPEC-SUBOSC.md` §8.4) | [C] |
| determinism | mulberry32 re-seeded from `seed` at every note-on (`src/subosc_core.h:322`) | [C] |
| parity | `gen_subosc_goldens` + `subosc_check` (both rates) + `subosc_check.mjs`; ADR-178 | [C] |
| module preset | none | [C] |
| module preset, proposed | a `sub` part preset: the 20 prefixed keys, the osc-preset idiom (`src/osc_preset.h`), loaded as a load (absent ⇒ default) | [P] |

### 4.2 Per parameter — identity, morph, units, preset scope, pitch

Preset scope for **every** row [C]: patch state (`sub.<key>` in `stateJson` `:6719-6725` and the host
chunk), the four morph corners (layout 9), and history (lossless). Absent key ⇒ default (B181).

| id | key | range (default) | units · taper | class → morph | scope | pitch relationship |
|---|---|---|---|---|---|---|
| 4000 | wave | 0..6 (3 saw) labels | enum [C] | Structural → **snaps** [C] | member [C] | – |
| 4001 | width | 0.05..0.95 (0.5) | duty, linear; live only when wave = pulse (TSV `:420`) [C] | Morphable → **slides**, off-corner via 4015 at rev ≥ 2 [C] | member [C] | – |
| 4002 | bumpAmt | 0..0.6 (0.35) | ratio, linear; bump only [C] | **slides** [C] | member | – |
| 4003 | bumpPhase | −π..π (−0.25) | rad, linear [C]; **circular** — should blend on the shortest arc [P] | **slides** (linear today) [C] | member | – |
| 4004 | octave | −3..0 (−1) | oct, stepped [C] | **snaps** [C] | member | the Sub's own offset; **not** summed with `gOct` 103 [C]; should be [P] |
| 4005 | semis | −12..12 (0) | st, stepped [C] | **snaps** [C] | member | own offset; not summed with `gSemi` 101 [C] |
| 4006 | fine | −100..100 (0) | c, linear [C] | **slides** [C] | member | own offset; not summed with `gFine` 102 [C] |
| 4007 | level | 0..1 (0.8) | gain, linear; 0 = exact silence [C] | **slides** [C] | member | – |
| 4008 | phase | 0..1 (0) | cycles, linear [C]; **latched at note-on** (`src/subosc_core.h:318`) — FOUNDATIONS `kLatched` [P]; circular [P] | **slides** — audible only at the next strike [C] | member | – |
| 4009 | keytrack | 0/1 (1) | off ⇒ MIDI 36 [C] | **snaps** [C] | member | decides whether the key reaches the pitch at all [C] |
| 4010 | tone | 30..20000 (20000) | Hz; GUI log10 (TSV `:429`), host `%.3f` [C] | **slides** (linear in Hz) [C]; should blend in log-frequency [P] | member | – (not key-tracked) [C] |
| 4011 | sync (retired) | 0/1 (0) | reaches nothing (B184; `subosc_check` 11e) [C] | **snaps** (slot kept for position) [C] | member | – |
| 4012 | attack | 0.0005..0.5 (0.005) | s; GUI log10 [C]; provisional — R4 strikes it when the voice envelope lands [C] | **slides** [C] | member | – |
| 4013 | release | 0.002..2 (0.08) | s; GUI log10 [C]; provisional (R4) [C] | **slides** [C] | member | – |
| 4014 | seed | 0..4294967295 (1) | integer; latched per note-on [C] | **snaps** [C]; a seed is not a timbre a corner blends — **never** (Device) is the better class, as `morphSeed` 156 is [P] | member [C] | – |
| 4015 | on (gate) | 0/1 (0) | switch [C] | Structural → **ramps** (~8 ms one-pole off the bilinear ON weight, flip at 1e-3; `:4368`, `:8583-8596`) [C] | member (B203) [C] | – |
| 4016 | mono | 0/1 (0) | switch [C] | **snaps** — and a flip **hard-kills** sounding notes (`:1964`) [C]; should defer the flip to silence or ramp like 4015 [P] | member | voice model (4.1) |
| 4017 | bias | 0..2 (0 lowest) | enum [C] | **snaps** [C] | member | chooses which held key sets the mono pitch [C] |
| 4018 | glide | 0..2 (0) | s, linear, chunk-granular (`:8554-8570`) [C] | **slides** [C] | member | the Sub's private glide; B196 retires it in place for the bend lane [C] |
| 4019 | pitchMod | −48..48 (0) | st, linear [C] | **slides** [C] | member | the Sub's only pitch-mod surface [C] |

### 4.3 Per parameter — the modulation contract

Rules that hold for every row [C]: stepped rows are refused (`:3726`); a continuous row is accepted
if it has a gui2 knob; depth = fraction of the declared span, applied value clamped (`:4204-4205`);
readback reports the base (`modreadback_check`); writes at ≤ 172.27 Hz. **The Sub core smooths
nothing**: `render()` reads `p_[]` once per call (`src/subosc_core.h:348-352`) and the shell calls it
per ≤ 256-sample chunk, so every modulated value is a staircase at the tick. No row below has a
measured ceiling; the ceilings marked [P] are what B199's sweep would test.

| id | key | eligible | depth unit today [C] | proposed depth unit [P] | binding limits (B199) | proposed smoothing / ceiling [P] |
|---|---|---|---|---|---|---|
| 4000 | wave | no (stepped) | – | – | L2 | switch at a zero crossing or crossfade (STATION measured an unsmoothed wave switch at 11.1× the natural slope, `specs/SPEC-STATION.md:106`) |
| 4001 | width | yes | 0.9 span | linear | L4, L1 (step in duty), L5 | 5 ms one-pole (STATION's law); ≤ 86 Hz by L5 |
| 4002 | bumpAmt | yes | 0.6 span | linear | **L4** (bump search each write), L1, L5 | coefficient recompute only on change, once for all 16 cores (G8); 5 ms smoothing |
| 4003 | bumpPhase | yes | 2π span, clamped | **circular (wrap)** | L4, L1, L5 | as 4002; wrap, never clamp |
| 4004 | octave | no | – | – | L2 | – |
| 4005 | semis | no | – | – | L2 | – |
| 4006 | fine | yes | 200 c span | cents (as today) | L1 (pitch step per chunk), L4, L5 | share the swarm's 8 ms pitch smoother (`:4233`) — −3 dB near 20 Hz |
| 4007 | level | yes | 1.0 span | linear (or dB) | **L1** (gain step per chunk; STATION's unsmoothed LVL measured 7.4×), L4, L5 | 5 ms one-pole, −3 dB near 32 Hz |
| 4008 | phase | yes | 1.0 span | circular | latched: effective rate = note rate | declare **latched**; remove from the destination menu or label it "at note-on" |
| 4009 | keytrack | no | – | – | L2 | – |
| 4010 | tone | yes | 19 970 Hz span | **octaves** | L4, L3 not binding (one-pole TPT `g/(1+g)` stays in (0,1) for any fc < 0.45·sr, `src/subosc_core.h:534-536`), L5 | octave depth; 5 ms smoothing on the coefficient |
| 4011 | sync (retired) | no | – | – | – | – |
| 4012 | attack | yes | 0.4995 s span | log (seconds ratio) | L4, L5 | none needed beyond the tick; remove if R4 strikes it |
| 4013 | release | yes | 1.998 s span | log | L4, L5 | as 4012 |
| 4014 | seed | no | – | – | latched | – |
| 4015 | on | no | – | – | L2 (ramped by the field, not by the matrix) | – |
| 4016 | mono | no | – | – | L2 | – |
| 4017 | bias | no | – | – | L2 | – |
| 4018 | glide | yes | 2 s span | log | L5 | – (retire per B196) |
| 4019 | pitchMod | yes | 96 st span | semitones (as today) | **L1** (raw pitch step per chunk, `:8600`), L5 | the swarm's 8 ms pitch smoother — one rule, one implementation |

---

## 5. The gap list — ranked, each written as a proposed ROADMAP row

Severity: **CRITICAL** (wrong audio or data loss, measured) · **HIGH** (wrong audio or a gate green
for the wrong reason, from code) · **MEDIUM** (a second copy, an unstated bin, a cost) · **LOW**
(drift). **No CRITICAL**: nothing below was measured shipping wrong audio in this run, and the one
measured loss (B255) already has a row. Each row names its class (the nine shapes: green for the
wrong reason · prose asserting a relationship · a hand list beside a glob · one rule with two
implementations · insertion into positional data · absent means keep · reachable but unrecorded ·
an exclusion with no expiry · a number nobody measured), the minimal delta, and the check that
would enforce it.

### G1 — HIGH — The readiness manifest and its gate (B275 phase 1, made concrete)
**Title.** *Engine manifests and `tools/readiness_check`: a source is integrable only when its
declaration is complete and agrees with the code.*
**Paragraph.** Today every seam's contract lives in three places — the code, the playbook's prose,
and ad-hoc accountings (the SCALPEL one sorts 107 parameters by hand). The Sub, the only engine
block, has no manifest at all, and this audit had to derive §4 from code. A manifest per engine
(one row per id, the §4 columns) plus a checker makes "ready" a verdict rather than an opinion.
**Class.** a number nobody measured · prose asserting a relationship.
**Minimal delta.** `docs/contracts/SEAMS.md` (the §2 registry, citing anchors not line numbers);
`docs/contracts/manifests/sub.tsv` (§4, with [P] fields resolved by the human); a PROVISIONAL schema
header naming its swap-in point for FOUNDATIONS' schema; `tools/readiness_check.py`, wired into
`verify fast`, that **enumerates from `kEngineBlocks` and the `kParams` scope** (never from a list)
and asserts per row: class = `paramClassOf`; morph behaviour ⇔ membership + gate; mod eligibility
⇔ `modAddRoute`'s rule; preset keys ⇔ `stateJson` keys; pitch surfaces ⇔ a declared list; every [P]
field either resolved or carrying `UNRESOLVED: <row>`. Must-fail controls: a planted class
disagreement, a missing row, an extra row.
**Enforced by.** `readiness_check` (new). **Retroactivity is the human's question** (B275 (b)).

### G2 — HIGH — Global pitch is three private compositions; publish one
**Title.** *One per-voice sounding pitch, consumed by every source and every pitch-tracking module.*
**Paragraph.** The swarm composes master transpose, bend (with its law), the matrix offset and
`oscPitch` in `updateTune` (`src/hypersaw_clap.cpp:5283-5292`). The Sub reads the raw key
(`src/subosc_core.h:316`) plus its own offset (`:8600`); the Comb slot reads the raw key
(`:8160-8163`). So master octave, master pitch, the bend wheel, the bend law, the scale quantiser
and MPE per-note bend move the swarms and leave the Sub and the Comb behind (FROM CODE; a render
probe would confirm it in one row). B196 ("follow bend") is the human's ruling on half of it; the
master-transpose half has no ruling at all.
**Class.** one rule with two implementations (three) · prose asserting a relationship (ADR-082's
"the wheel bends the patch").
**Minimal delta.** A shell function that returns the per-voice pitch in semitones *excluding* each
source's own offsets; the Sub adds its octave/semis/fine/pitchMod to it; the Comb tunes from it.
Subsumes B196 (retire 4018 in place). Sound-changing for any patch that uses master transpose or
bend with the Sub on ⇒ **behind `engine_revision`** (ADR-183's pattern).
**Enforced by.** A pitch-surface row (the `mpe_check` Goertzel pattern) that sweeps 101/102/103,
38, an MPE bend and a quantised bend with the Sub on and the Comb slot active, and asserts each
source's measured f0; must-fail control: revision 1.

### G3 — HIGH — Host tempo reaches oscillator 1 only
**Title.** *Swarm 2's tempo-grid law ignores the host tempo.*
**Paragraph.** `core.p.bpm = tr->tempo` (`:8409`) and `core.p.bpm = p->transport->tempo`
(`:8664`) write `cores[0]` only (`core` is `cores[0]`, `:1701`). `cores[1].p.bpm` keeps its
constructor 120 (`src/swarm_core.h:169`), and law 3 reads it (`:1809`), so osc 2 at law 3 snaps its
beat grid to 120 bpm in every session. FROM CODE; `law` and `beatMult` are per-osc since A12, so
the case is reachable from the GUI. The only prior record of this line (ROADMAP ~7834) predates the
second oscillator.
**Class.** green for the wrong reason (`mpe_check` fans out *note* events; nothing fans out
transport) · prose asserting a relationship (`:851-852`).
**Minimal delta.** Write bpm to every core (a two-line loop at both sites); arguably not
sound-changing for any patch a user heard as intended, but the lead should rule whether it needs a
revision gate.
**Enforced by.** A tempo fan-out row: render osc 2 at law 3 at 90 and 150 bpm and assert the beat
grids differ as `u = bpm/60 · beatMult` predicts; control: osc 1.

### G4 — HIGH — Maximum viable modulation rate: unmeasured everywhere, unsmoothed in the Sub
**Title.** *B199's sweep, scoped to produce the manifest's rate column — the Sub first.*
**Paragraph.** The human's named requirement has no data for any destination (S7 rule 7.5). The Sub
smooths nothing, so `level` and `pitchMod` step at 172 Hz; the swarm's pitch route is smoothed at
8 ms (`:4233`) while the Sub's `pitchMod` — built "so the two pitch surfaces agree" (`:1305-1311`) —
is not. STATION already paid for this lesson (unsmoothed LVL 7.4×, cells 13.9×, SPEC-STATION §4).
**Class.** a number nobody measured · one rule with two implementations.
**Minimal delta.** B199 as reframed, enumerated from `paramClassOf` (continuous rows), reporting
the smoothing each row needs; the Sub's `level`, `width`, `bump*`, `tone` get STATION's 5 ms law,
`fine`/`pitchMod` the swarm's pitch smoother. Smoothing is sound-changing for modulated patches ⇒
revision-gated.
**Enforced by.** The B199 suite (measured thresholds + margin + must-fail control per row).

### G5 — HIGH — 68 non-Device globals are absent from the morph field, and no check can see it
**Title.** *Absent from the field must be a decision: a declared exclusion list, and T10 over globals.*
**Paragraph.** Class ≠ membership (playbook §2(b)). `morphlayout_check` T10 requires membership for
per-osc and engine ids only (`tools/morphlayout_check.cpp:406`). Absent without a recorded reason:
mono 15, bass mono 40/41, oversample 88, master pitch 101/102/103, voice cull 160, the 28
time-engine rows 200–230 (Echo/Room) and the 32 delay rows 232–263. Consequences: a corner cannot
hold a delay time or a master transpose; right-click exempt does nothing on them (ADR-109 A1's
defect, for 68 more rows); the intent bus cannot bind them. Several may be right to exclude
(oversample, voice cull) — the gap is that nobody decided.
**Class.** reachable but unrecorded · an exclusion with no expiry · a hand list beside a glob.
**Minimal delta.** A `kMorphExcluded[] = {id, reason}` table beside `kParamClassOverrides`; T10
widened to "every non-Device row is a member **or** listed with a reason". Any that should JOIN are
EXISTING parameters, so each needs B255's migration first.
**Enforced by.** `morphlayout_check` T10 (widened — a widening, not a weakening).

### G6 — HIGH — The FX slot contract covers 7 of 10 types, and its check cannot notice
**Title.** *`kSlotContract` rows for Echo, Room and Delay; the table's size asserted against the enum.*
**Paragraph.** `kSlotContract` has 7 rows (`src/fx_rack.h:64-72`); `FxType` reaches 9 (`:91-97`).
`slotcontract_check` loops `t <= 6` over a hand name list (`tools/slotcontract_check.cpp:284-286`),
so the three newest modules have never been asked the universal guarantee ("`mix = 0` is a
bit-exact bypass"), and any future caller indexing `kSlotContract[type]` reads past the array.
**Class.** a hand list beside a glob · green for the wrong reason.
**Minimal delta.** A `kFxTypeCount` (or `static_assert(std::size(kSlotContract) == 10)`), three rows,
the loop bound derived from the table. Echo/Room/Delay carry wet-path latency, so
`latency_samples` must be stated, not defaulted to 0.
**Enforced by.** `slotcontract_check` (widened).

### G7 — HIGH (existing: B193's ruling, re-ranked) — A preset load keeps the previous routing matrix
**Title.** *Rule B193: the routing matrix on the preset path — reset, restore, or both.*
**Paragraph.** `initState` resets routing only for host chunks (`if (chunkOnlyState)
applyRoutingChunk("");`, `:6832`), and preset JSON carries no routing (`stateJson` `:6678-6740`),
so a preset load leaves the FX matrix — including the Sub's output row — as the previous patch had
it (morph off). `undo_check` S4 pins exactly this as the boundary "nobody has ruled on"
(`tools/undo_check.cpp:2613-2619`). Every other patch surface obeys "a load is a load" (B181).
**Class.** absent means keep · an exclusion with no expiry.
**Minimal delta.** The ruling first (key name, and whether a preset without routing resets it to
the default). If "reset", one line removes the `chunkOnlyState` condition — sound-changing for the
factory bank's loads, so the lead decides whether B257 (scrap the bank) makes that moot.
**Enforced by.** `bank_check` E extended to the 29 routing ids; `undo_check` S4 re-pinned to the
ruling.

### G8 — MEDIUM — Every Sub parameter write costs 16 bump-peak searches
**Title.** *Recompute Sub coefficients once per write and only when their inputs change.*
**Paragraph.** `subSetParam` calls `setParam` on all 16 cores (`:1977-1978`); each runs `recalc()`
(`src/subosc_core.h:252`), which always re-runs the BUMP peak search (282 transcendentals) even for
`level` or `fine`. Under one LFO route: ≈ 7.8·10⁵ transcendental calls/s, **estimated** 0.8–1.6 %
of a core per routed row (§4.1, method stated), comparable to B262's MEASURED whole-rack worst case.
**Class.** a number nobody measured (cost).
**Minimal delta (bit-identical).** Compute the coefficients once and copy them to the 16 cores (the
cores share `p_`); re-run `bumpPeak` only when `bumpAmt`/`bumpPhase` change. Same floats, same
order ⇒ `subosc_check`'s parity and bit-identity rows guard it.
**Enforced by.** `subosc_check` (unchanged, bit-identical); cost reported by an unwired bench row
(`measure_cpu` pattern) before and after.

### G9 — MEDIUM — Sub voicing switches hard-kill sounding notes
**Title.** *`sub.mono` (and the plain `sub.on` toggle) must not cut a sounding note to zero.*
**Paragraph.** `if (v != subMono) subAllOff();` (`:1964`) → `allOff()` sets `env = 0` in one sample
(`src/subosc_core.h:334-340`). A morph flip of 4016 at a corner boundary, host automation, or a GUI
click mid-note is a full-scale discontinuity (FROM CODE, not measured). The gate 4015 is protected
under morph by its ramp; outside morph its OFF is the same cut.
**Class.** one rule with two implementations (the gate ramps; mono does not).
**Minimal delta.** Release rather than kill (`noteOff` on crossing, then re-strike), or defer the
mono flip until the Sub is silent.
**Enforced by.** A discontinuity row in `subosc_check` §11 (its 11g.e slope method) flipping 4016
mid-note; control: the fixed build.

### G10 — MEDIUM — A non-finite parameter value becomes the parameter's MAXIMUM
**Title.** *Map non-finite input to the default, once, at `applyParam`.*
**Paragraph.** `std::max(minV, std::min(maxV, NaN))` evaluates to `maxV` (`:7301`), so NaN from a
host or from a corrupt chunk (`std::atof`, `:9400`, accepts "nan"/"inf") sets master volume to 1.5,
coupling to +1, voices to 32. The Sub core's own rule is "non-finite ⇒ default"
(`src/subosc_core.h:248`) but it never sees NaN. No gate feeds a non-finite parameter value.
**Class.** one rule with two implementations · an unstated variety bin.
**Minimal delta.** `if (!std::isfinite(value)) value = defaultFor(*d, osc);` before the clamp.
**Enforced by.** A `paramscope_check` row that writes NaN/±inf to every id it enumerates and
asserts the readback is the default; must-fail control: the current clamp.

### G11 — MEDIUM — Circular and logarithmic parameters are blended and modulated linearly
**Title.** *A per-parameter depth unit and blend law: linear, log (octaves), circular (wrap).*
**Paragraph.** `sub.bumpPhase` (±π) and `sub.phase` are cycles; `sub.tone`, every delay time, LFO
rates and envelope times are perceptually logarithmic; all are blended bilinearly (S4) and
modulated span-linearly with a clamp (`:4204-4205`). SCALPEL's accounting reaches the same
conclusion for Position and five log rows (ACCOUNTING §4.3). B213 D6 is the open plan for the log
half.
**Class.** one rule where the domain needs three.
**Minimal delta.** A manifest column (G1) and a per-row transform applied after the base is read
(B241's condition), for new rows first; existing rows only behind a revision gate.
**Enforced by.** `modreadback_check` (base unchanged under the transform) plus a wrap row.

### G12 — MEDIUM — STATION's spec contradicts the matrix's refusal of stepped destinations
**Title.** *Rule `op{n}.qnt` "stepped mod target" before STATION is un-parked.*
**Paragraph.** `specs/SPEC-STATION.md:180` marks phase-quant a modulation target; `modAddRoute`
refuses every stepped destination (`:3726`, ADR-136: "a zippered enum is not modulation").
**Class.** prose asserting a relationship.
**Minimal delta.** A human ruling; either amend the spec (a sanction on a protected path) or define
a stepped-destination law (quantise the modulated value) in the shell.
**Enforced by.** `readiness_check` (G1) once STATION has a manifest.

### G13 — MEDIUM — A hand-tuned per-sample retime slew in DelayCore and TimeCore
**Title.** *Derive the delay retime slew from seconds (ADR-009).*
**Paragraph.** `dSmL += (tgtL - dSmL) * 0.0015;` (`src/delay_core.h:149-150`) and the same constant
in `TimeCore` (`src/time_core.h:127`): τ = 666 samples, 15.1 ms at 44.1 kHz, 7.6 ms at 88.2 kHz
(arithmetic). TimeCore is a lab port (parity with `reference/swarmtime.html` may require the
literal; then it wants an ADR-recorded divergence); DelayCore is spec-as-contract (ADR-142) and has
no such reason. `playbook_check` scans for clocks and RNGs, not per-tick literals.
**Class.** a number nobody measured · an exclusion with no expiry.
**Minimal delta.** `1 − exp(−1/(τ·sr))` with τ = 15.106 ms for DelayCore. Even at 44.1 kHz that
evaluates to 0.00149999999999995, not the literal 0.0015 — so not bit-identical anywhere, and the
lead rules on a revision gate.
**Enforced by.** A `delay_check` row rendering a time change at 44.1 and 96 kHz and asserting equal
retime duration in seconds.

### G14 — MEDIUM — The GUI holds second copies of the mod source table and the destination rule
**Title.** *Publish the source table and destination eligibility from the shell (the bind idiom).*
**Paragraph.** `MOD_SRC_NAMES` (`src/gui/gui2.html:3174-3185`) restates the shell's slot table; no
tool reads it. `modDestOptions` restates `modAddRoute`'s refusals and already differs (161–178 vs
161–177, `gui2.html:3358` vs `:3730`). ACCOUNTING §4.2a counts five places that must move in step
when the cap grows.
**Class.** one rule with two implementations · a GUI decoder of a layout the shell owns.
**Minimal delta.** The shell exports `{slot, name, polarity}` and an `isDestination(id)` answer over
the existing bridge; the GUI reads them.
**Enforced by.** `gui_history_check`'s harness (it already executes gui2) asserting the GUI's list
equals the shell's; or a small python check until then.

### G15 — MEDIUM — The playbook's citations are 63 % drifted
**Title.** *`playbook_check --fix`: rewrite drifted line numbers in place.*
**Paragraph.** 117 of 186 citations are advisory drift today; the playbook is the de-facto seam
registry, and a reader following `src/hypersaw_clap.cpp:6284` lands 394 lines from `stateJson`.
**Class.** prose asserting a relationship (a line number is a claim).
**Minimal delta.** A `--fix` flag that rewrites only the line number when the anchor is found
elsewhere — the gate itself is untouched. SEAMS.md (G1) cites anchors, not lines.
**Enforced by.** `playbook_check` (unchanged); drift count printed trends to 0.

### G16 — LOW — Four files claim a core is not in the audio path; it is
`src/intent_core.h:34-36`, `src/glide_core.h:25`, `verify:459-461` (routing) and `verify:586-588`
(glide) say "not wired / not in the audio path"; `routing.processBlock` runs at `:8824`,
`bendGlide.step` at `:8735`, `intentStep` at `:4511`. Also stale: `EngineBlock::gateId` "DEVICE
class" (`:1209`; B203 made it Structural), "The law params do not exist yet" (`:2523`), `morphJson`'s
"2 = late per-osc rows" (`:6508`, writes 9). **Class:** prose asserting a relationship. **Delta:**
comment edits; the two `verify` lines are **a sanction the human would have to give** (a comment,
not a gate). **Check:** none needed.

### G17 — LOW — ROADMAP rows read "READY TO MERGE" for merged PRs
B232 (#744 merged `ac2e0ef`), B240 (#749 merged `158339d`), B241 (#743 merged `6256301`). Lead-owned
(the lead is ROADMAP's only writer).

### G18 — LOW — SPEC-SUBOSC disagrees with the code on addresses and inertia
The spec's addresses are `subosc.*` (`specs/SPEC-SUBOSC.md:207-221`); the code and TSV say `sub.*`
(`:1352`, TSV `:419-438`). §8.3 declares `fine` and `tone` inertia-eligible; no inertia seam exists
(S19). Protected spec ⇒ **a sanction the human would have to give**.

### G19 — LOW — `gui_history_check` lifts runtime builders from a hand list
`tools/labharness/gui_history_check.mjs:306-309` lifts four builders; `buildScalePicker`
(`gui2.html:7019`) and `buildMatrixPane` (`:7279`) are not lifted. **Class:** a hand list beside a
glob. **Delta:** discover `function build*` in gui2 and fail on any not lifted or explicitly
excluded.

### G20 — HIGH (operational, today) — The B275 brief to FOUNDATIONS is a draft by our own rule
`python3 tools/mailbox_delivery_check.py` (run in the main checkout) → `FAILED — filings that exist
here and NOT on the reader's origin/main: FOUNDATIONS: brief-engine-manifest.md`. `./verify fast`
is therefore RED on this Mac until the brief reaches FOUNDATIONS' `origin/main` (their R9, which we
proposed). Note the tension with CLAUDE.md §Mailbox ("an uncommitted brief is still filed") — that
clause governs briefs *to* us; the gate governs ours to them. **Delta:** the lead delivers it the
way R9 prescribes. **Check:** `mailbox_delivery_check` (unchanged).

---

## 6. FOUNDATIONS' generic schema vs horde's own seam laws

Read against the brief as filed (`FOUNDATIONS/integrations/hypersaw/brief-engine-manifest.md`,
2026-09-26, untracked in their tree at the time of this audit) and against FOUNDATIONS' current
`ParamDesc` (`core/include/foundations/registry.h:79-101`: address, id, `ValueType`
{scalar, stepped, curve, array, blob}, `ReadCadence` {continuous, latched}, min/max/default,
`mod_min`/`mod_max`).

### 6.1 The split, field by field

| field | owner | evidence |
|---|---|---|
| value shape (continuous / stepped / array / blob) | **FOUNDATIONS** — already `ValueType` | STATION's Wave RAM needs `kArray`/`kBlob` (S12 12.9) |
| read cadence (continuous / latched at note-on) | **FOUNDATIONS** — already `ReadCadence` | `sub.phase`, `sub.seed` are latched (§4.2); ACCOUNTING's NO class |
| owning switch ("belongs to gate X") | **FOUNDATIONS** (neutral fact) | B232 needs it per parameter; horde derives it from id arithmetic (`sourceGateOf` `:4408`), which cannot express SCALPEL's blade switches (ACCOUNTING §G2) |
| morph behaviour: slides / snaps / ramps / never | neutral tag **FOUNDATIONS**; the *law* **horde** | horde's class is derived from `stepped` + overrides (`:1597`); blend/quantum/off-corner/revision rules are horde's novelty |
| morph scope: per-corner member / exempt-able / absent | **horde** | positional corner arrays, the append site, the layout marker and draw freeze are horde's storage (S3) |
| combination law / depth unit (linear, log, circular) | **FOUNDATIONS** — §3.1 already has a "per-parameter combination law" and §9 defers its catalogue | S7 7.2, G11; ACCOUNTING §4.3 |
| smoothing class + recompute cost per write | **FOUNDATIONS** (the payload knows its own cost) | G8: the Sub's cost is a property of its core, not of horde |
| control tick and the derived maximum rate | **horde** (the host's property) | L5 is `kGravGridSeconds`, a horde constant; the ceiling = min(host tick, payload smoothing, stability) |
| units | taper and **range-freeze** → **FOUNDATIONS**; display unit and label → **horde** | FOUNDATIONS' own D1 ruling (quoted in `src/param_presentation.tsv:3-8`) keeps label/unit/page/widget OUT of `ParamDesc` |
| preset participation | **FOUNDATIONS** §3.3 (scoped cascade) | horde has patch / corner / (unwired) oscillator tiers only (S12 12.7) |
| voice/note model | **FOUNDATIONS** §3.4 + §4 ("the engine manifest wires them") | the Sub's private mono (S9 9.2); STATION defers to the host |
| pitch relationship | the *input* **FOUNDATIONS** §3.4 (per-voice trajectories); which lanes horde composes into it **horde** | G2 |
| adaptive state, inertia eligibility | **FOUNDATIONS** §3.8, §3.1 interceptors | SPEC-SUBOSC §8.3–8.4, SPEC-STATION §9 already declare them in prose |
| ids, engine blocks, positional append rules, the `sub.` prefix, retired-in-place | **horde** (FOUNDATIONS' `id` is append-only already) | S1 |
| parity chain, check wiring, readiness gate | **horde** (`./verify`) | S16; FOUNDATIONS supplies a conformance tool for the *declaration* only |

### 6.2 Where the evidence agrees with the brief

- The four per-parameter families (morph behaviour + scope, modulation contract, units/taper with
  range-freeze, preset participation) are the right ones; every seam in §2 that failed did so on
  one of them.
- Q2 ("number, class, or both"): **both** — the evidence says a class alone hides the cost axis
  (G8) and a number alone hides *why* (L1 vs L4 have different fixes).
- Q4: the core should carry only neutral facts. Agreed, with one addition below.
- The two-consumer argument holds: SCALPEL's accounting, STATION's spec §10 and the Sub's spec §7
  each invented the same columns independently (class, mod ✓, inertia, notes).

### 6.3 Where it disagrees or is incomplete

1. **"horde measured that 'a safe rate' is five different failures" is not accurate.** B199 is an
   analysis and a plan; its deliverable is unbuilt and no rate has been measured (ROADMAP B199,
   "REFRAMED DELIVERABLE"). The brief should say *reasoned*, not *measured* — worth a one-line
   correction notice, because FOUNDATIONS will weigh consumer evidence by its kind.
2. **The pitch relationship should be one input, not a menu of opt-ins** (Q3). The evidence is G2:
   three consumers opting into surfaces separately is exactly how the Sub and the Comb came to
   follow none. Recommend: FOUNDATIONS carries a per-voice pitch *input* (§3.4 already names pitch
   as a generic per-voice trajectory); a payload declares only **opt-outs** with reasons.
3. **The schema needs an "owning switch" field** the brief omits. "Ramps" is not a property of a
   parameter; it is the relation between a parameter and the switch it lives under. B232's rule,
   SCALPEL's blade gates (ACCOUNTING §G2) and the FX slots' missing gate (S4 4.2) all need it.
4. **Two fields the brief asks for already exist in FOUNDATIONS**: `ReadCadence` (continuous /
   latched) covers part of "maximum viable modulation rate", and §3.1's combination law is the
   depth unit. The brief should point at them rather than ask for new ones.
5. **"Display unit" belongs to horde**, by FOUNDATIONS' own D1 ruling (the TSV header quotes it).
   Taper and range-freeze are structure and belong in the schema.
6. **Missing from the brief:** non-scalar state (STATION's tables — `kBlob`/`kArray` exist), adaptive
   state (§3.8; the Sub declares absence), inertia eligibility (two horde specs declare it), and
   **cost per parameter write** alongside the per-payload cost budget.

---

## 7. Reduction budget

Estimates, stated as such; the point is copies, not lines.

| finding | removes | adds | net copies |
|---|---|---|---|
| G1 manifests + gate | playbook §§1–10 contract prose migrates to SEAMS.md (≈ −300 lines of the playbook's 1 080; the procedures stay) | SEAMS.md ≈ +350; `sub.tsv` ≈ +25; `readiness_check.py` ≈ +250; **+1 check** | contract statements 3 → 1 per seam |
| G2 one pitch | the Sub's and the Comb's private key-to-Hz (≈ −10) | composer accessor ≈ +40; +1 check row | pitch compositions 3 → 1 |
| G3 tempo fan-out | – | +2 lines; +1 check row | – |
| G4 B199 | – | the suite (B199's own estimate); smoothers ≈ +30 | smoothing laws per concept 2 → 1 (pitch) |
| G5 exclusion table | – | ≈ +75 (68 ids with reasons); T10 widened | undecided absences 68 → 0 |
| G6 slot contract | hand bound + name list (−2) | 3 rows, 1 static_assert | coverage list 2 → 1 (the table) |
| G7 routing on load | 1 condition | ruling-dependent | – |
| G8 Sub recalc | 15 redundant recalcs per write | ≈ +15 | – |
| G10 non-finite | – | +1 line; +1 row | non-finite rules 2 → 1 |
| G14 GUI copies | `MOD_SRC_NAMES` literal, the GUI refusal ranges (≈ −20) | bridge export ≈ +25 | copies 2 → 1 (twice) |
| G15 `--fix` | – | ≈ +30 | – |
| G16 stale comments | ≈ −15 wrong lines | ≈ +8 right ones | – |
| **total** | **≈ −360 lines, 2 hand lists, 5 second copies** | **≈ +850 lines, 1 new check, ~5 new rows in existing checks** | |

---

## 8. What changed since the last audit

The last repo-wide audit is `docs/audits/2026-09-19-repo-audit.md` at `5a496c1`; the playbook was
read at `e07acac` and amended at `6256301`. Since then: the Sub landed as the first engine block
(B172) and gained shell rows, a gate in the field and the off-corner rule (B181, B203, B232/ADR-183
→ revision 2); the morph field gained its only append site and a whole-order freeze (B240, T13–T15);
modulated readback was fixed across 172 rows with an enumerating check (B241,
`modreadback_check`); wiring became wired-or-explained with a gate (ADR-180 §1; 68 checks WIRED,
1 UNWIRED, all verified). The previous audit's M2 (the morph layout marker as a bare literal in
seven places) is now four writers held equal by `playbook_check`. **New since then and found
here:** G3 (tempo fan-out — the second oscillator's arrival made it reachable), G6 (Echo/Room/Delay
joined the rack after the contract was written), G2's master-transpose half and the Comb, G10, G13.

---

## 9. Refusals and limits, stated rather than guessed

- **Nothing was rendered or built.** Every "wrong audio" claim (G2, G3, G9) is FROM CODE; each gap
  names the render row that would measure it. The Sub's per-write cost (G8) is an estimate with its
  method stated; no timing was taken on this Mac.
- **The queue-headroom figure** (peak 1 471 of 2 048) is the playbook's measurement, not re-taken.
- **The swarm's per-row smoothing classes** are taken from ACCOUNTING §1.1's reading, not re-read
  row by row.
- **`private_name_gate` and `mailbox_delivery_check` did not run inside this audit's worktree** (no
  untracked names file and no sibling checkouts beside it); the mailbox check was run from the main
  checkout (G20). This file names only FOUNDATIONS, Sluice and MAW among siblings.
- **I did not score SPECTRA** (parked indefinitely; not in the brief's list) or the parked SWARM-FX
  shell.

---

## The five findings to act on first

1. **G20** — the B275 brief is a draft by R9; `verify fast` is red on this Mac until it reaches
   FOUNDATIONS' `origin/main`.
2. **G2** — one published per-voice pitch; the Sub and the Comb ignore master transpose and bend.
   It carries B196 and needs a revision gate.
3. **G3** — host tempo reaches oscillator 1 only; two lines and one fan-out row.
4. **G7** — rule B193: a preset load keeps the previous routing matrix, pinned as "nobody has ruled".
5. **G5 + G6** — the two coverage holes that let a green gate say "all": T10 over globals with a
   declared exclusion list, and the slot contract over the whole `FxType` enum.

**The one question for the human.** *When a source ignores a global — the Sub ignoring master
transpose, bend and the voice model today — should the readiness gate treat "follows every global
unless it declares an opt-out with a reason" as the rule (so the Sub fails until fixed or excused),
or is each global's reach a per-source choice the manifest merely records?* The answer decides
whether G1's gate is retroactive (B275 (b)) and whether G2 is a defect or a feature request.
