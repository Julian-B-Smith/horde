# subosc-source-row — the SUB OSC reaches the shell as ADR-088's first engine block

- **Queue item:** B172 phase 2 (ROADMAP; the pathfinder for the source-row seam
  — STATION phase 2 / B162 reuses the mechanism built here).
- **Why:** phase 1 (PR #685) landed `src/subosc_core.h` with bit-identical
  parity and no way to reach it. ADR-088 reserved ids 3000–9999 for engines
  that are not the swarm and reserved eight routing SOURCE rows; neither
  reservation had a consumer, so neither was ever exercised. This is that
  consumer, built as a generic **engine block** rather than as a sub-oscillator
  special case, because the next one is already queued.

## What changed

**The mechanism (one table, every dispatch).** `EngineBlock` + `kEngineBlocks`
in `src/hypersaw_clap.cpp`. Every id path walks it: `findParam`,
`paramClassOf`, `applyParam`, `readParam`, `params_count`, `params_get_info`,
`paramsJson`, `defaultsJson`, `stateJson`/`applyStateJson`,
`state_save`/`state_load`, `morphInit`, `undoMarkParam`. STATION's 3000-block
is one more row in that table and no dispatch edit. Each block names a
`gateId` (device class, default off) and a `keyPrefix` for the state chunks.

**The id map (core enum → shell id).** Positional; `id - 4000` IS the
`SubOscCore::Param` index, which is why nothing maps it.

| id | core key | core enum | class | in morphIds |
|---|---|---|---|---|
| 4000 | `wave` | kWave | structural | no |
| 4001 | `width` | kWidth | morphable | yes |
| 4002 | `bumpAmt` | kBumpAmt | morphable | yes |
| 4003 | `bumpPhase` | kBumpPhase | morphable | yes |
| 4004 | `octave` | kOctave | structural | no |
| 4005 | `semis` | kSemis | structural | no |
| 4006 | `fine` | kFine | morphable | yes |
| 4007 | `level` | kLevel | morphable | yes |
| 4008 | `phase` | kPhase | morphable | yes |
| 4009 | `keytrack` | kKeytrack | structural | no |
| 4010 | `tone` | kTone | morphable | yes |
| 4011 | `sync` | kSync | structural | no |
| 4012 | `attack` | kAttack | morphable | yes |
| 4013 | `release` | kRelease | morphable | yes |
| 4014 | `seed` | kSeed | structural | no |
| 4015 | `on` | — (shell row) | **device** | no |

Range, step and default are read from `SubOscCore::kParamTable` and nowhere
else; `subOscRowsAgreeWithCore()` is a `constexpr` proof of that, so a retyped
literal is a build failure rather than a silent divergence. The gate's core key
is `on`, not `subOn`: id 52 already carries `subOn` (the SPECTRA
sub-oscillator, ADR-042), and the `sub.` address prefix is what disambiguates.

**Instancing.** One `SubOscCore` per voice slot, 16 of them, indexed by
oscillator 0's slot — the same index `tags`/`penv`/`slotOf` use. Released
through the existing `allOffAll`/`noteOffAll` seam (one path, not a second
hand-wired one — L0029). `subKey[16]` maps slot → sounding key, because the
swarm releases by key and `SubOscCore` releases the one note it holds.

**Routing.** `kRoutingNSrc` 2 → 3. No routing id moved — the reserved source
block did what the ADR-088 amendment promised. Row 2's caption is SUB.

**Headroom (item 3), measured not assumed.** The core's TPT tone stage
overshoots above unity at a near-Nyquist cutoff. Re-measured on this build over
{44.1, 48, 96} kHz × MIDI 12–96 × width {0.05, 0.27, 0.5, 0.95}, bumpAmt at its
0.6 ceiling, level 1, tone 20 kHz:

| shape | worst peak |
|---|---|
| sine | 1.0025 |
| triangle | 0.9998 |
| square | 1.1221 |
| saw | 1.1208 |
| pulse | 1.1883 |
| noise | **1.4012** |
| bump | 1.0029 |

SPEC-SUBOSC's phase-1 finding records 1.425 for noise over a wider sweep. The
row divides by **1.425** (the larger of the two), so the bound holds under both
measurements. Through the shell the worst row peak is **0.9311** (noise, MIDI
12) — `subosc_check` 11f gates it, and its calibration shows the undivided
render reads 1.3268.

**Pins moved, each with its reason at the pin.**

| pin | was | now | reason |
|---|---|---|---|
| `bank_check` marker assertion | `morphLayout 5` | `6` | the engine block's morphable ids append after the routing block |
| shell `cornerJson` / `liveCornerJson` / `morphJson` | `5` | `6` | same; the ladder is written out at `cornerJson` |
| `tools/gen_factory_bank.cpp` | `5` | `6` | same |
| `morphlayout_check` T1b | tail ids `>= 10000` | `>= 3000` | ADR-088's engine band is the second appendable block; the invariant (no insertion into the frozen prefix) is unchanged |
| `paramclass_check` T4 twin band | `1000 <= id < 10000` | `1000 <= id < 3000` | **TIGHTENED**, not relaxed: an engine id is not a twin and `id - 1000` inside the band names nothing. Paired with new T4e/T4f pinning that refusal |
| factory bank (44 files) | — | re-saved | a new id block changes the chunk |

**Pin NOT moved, against the brief.** `paramclass_check` T1a stays **266**. The
brief asked for 266 → 282; that would have been red on arrival.
`tools/paramclass_check.cpp:153` reads `if (info.id >= 1000) continue;` before
`baseRows++`, so `baseRows` counts only ids below 1000 and the block at 4000 is
never counted. Verified by running it: `base rows printed: 266   host-exposed
ids: 393`.

**Bank diff, key by key against HEAD (44 files).** The only differences are the
16 `sub.*` keys at their declared defaults, the marker 5 → 6, and — in every
positional array (`morphCorners`, `cornerPreset`, `morphExempt`) — pure
INSERTS: `difflib` reports no `replace` and no `delete`, with the identical
shape in every file: 4 values at old index 235 (routing row 2's crosspoints,
`coeff[2][0] = 1.0` from `setSerialChain`, the rest 0) and 10 at old index 248
(row 2's dry-path cell at 0, then the nine morphable engine rows at their
defaults). `build` changes because it is the provenance stamp.

**CPU (item 8), reported not gated.** 16 voices, 5 s at 48 kHz, min of 3, real
plugin through the CLAP path: sub OFF **3.41 %** of one core, sub ON **3.81 %**
— the sub's bill is **+0.41 percentage points (1.12×)**. The standalone-core
figure for the same 16 instances is 0.391 %. Phase 1 finding 4 (`recalc()`'s
282-transcendental peak search) does NOT appear in that number: it is a
control-path cost, and the shell's per-param write now runs it on all sixteen
instances. Not restructured — out of scope, and reported here instead.

## Evidence consulted

- `src/subosc_core.h` — the ONE-INSTANCE-PER-VOICE header note (:11–21), the
  `Param` enum (:108), `kParamTable` (:126), `setParam(const char*)` throwing
  on an unknown key (:199), `render`'s `syncOn = (p_[kSync] == 1 && master !=
  nullptr)` (:272).
- `src/hypersaw_clap.cpp` — the ADR-088 id-layout comment and its "the
  sub … takes row 2 when it lands — no id moves for it" promise;
  `routingIndexOfRow`; `findParam`'s routing-first ordering and the trap it
  records; `renderSpan`'s stack-buffer/chunk contract; `setSerialChain`.
- `specs/SPEC-SUBOSC.md` §2, §4, §5.3, §6, §7, §8, §11 (R4 and R7 are the two
  open rulings this touches without resolving).
- `tools/paramclass_check.cpp`, `tools/morphlayout_check.cpp`,
  `tools/bank_check.cpp`, `tools/gui_reach.py`, `tools/presentation_check.py`,
  `tools/gen_gui_controls.py`, `tools/depends_check.py`.
- `traces/2026-09-19-subosc-core-phase1.md` (phase 1), LIBRARY L0023, L0029,
  L0031, L0032, L0036, L0057.

## Alternatives rejected

- **Putting the sub's 16 rows in `kParams`.** They would gain `+1000` twins,
  enter `buildMorphOrder`'s frozen prefix, and be enumerated per oscillator —
  all wrong for a device-wide object, and it would have made the id space
  oscillator-shaped for every future engine.
- **Generating the `ParamDef` rows from `kParamTable` at compile time with no
  literals.** The rows carry a host-facing name and an optional label array the
  core has no notion of, and three Python generators parse these literals out
  of the shell. Literals + a `constexpr` agreement proof is the brief's own
  sanctioned fallback and is strictly stronger than "there is no second copy",
  because the copy is now *checked*.
- **Wiring hard sync from something.** See the open questions.
- **A per-shape headroom gain.** It would make the waveform selector a level
  control, which is the one place a player does not want one.
- **Emitting the sub's state keys only when non-default** (the `routing=` /
  `intent=` idiom, which would have left every existing chunk byte-identical).
  Rejected per the brief: those are chunks meaning "someone left the default",
  while these are ordinary parameters, and a patch that omits a parameter is a
  patch that does not say what it is. The cost is paid once, here.

## Verify

`./verify full` — exit 0, git `a38d533` (`.harness/last-verify.json`). The run
on `423b9ef` (layer 2 + the working tree's layer 3) was also exit 0; the
committed-hash run is the one reported.

## Open questions

1. **A layout-5 corner array is NOT remapped into layout 6.** `morphSlotMap`
   returns the identity for any `layout >= 2`, so a corner saved under layout 5
   loads positionally into layout 6 — and the routing block INSERTED row 2's
   cells in the middle of itself, so those values land on the wrong crosspoints.
   This is the same situation the 4 → 5 bump created two days ago and was
   accepted then on the "no user patch exists under the old layout" ruling
   (ADR-088 amendment). It is now a two-day window rather than a zero-day one.
   The marker exists so a remap CAN be written; writing one is a design call
   (and STATION's append will pose it again). Not touched here.
2. **Structural engine rows are classed but not in the morph field.** The brief
   said "the morphable ids are appended AFTER the routing block", so only the
   nine morphable rows were appended. SPEC-SUBOSC §8.1 says the structural rows
   "morph atomically at a corner boundary like every other structural
   parameter", and every stepped `kParams` row IS in `morphIds`. Appending the
   six later is append-only-safe, so nothing is foreclosed — but today a corner
   cannot hold the sub's waveform.
3. **Hard sync is inert.** `SwarmCore` publishes no per-sample fundamental
   phase; `voices[s].phase[rootIdx]` is private, advanced inside the render
   loop, and advanced twice per output sample under oversampling. Publishing it
   means writing a per-sample buffer from inside a parity-gated core. The
   refusal is pinned by `subosc_check` 11e (sync ON is bit-identical to sync
   OFF) so it cannot become an accidental presence, and the seam is a queue row.
4. **Mono legato re-strikes the sub.** `SubOscCore` has no retune seam, so
   `retargetAll` calls `noteOn` — the swarm glides and the sub restarts its AR.
   Recorded in `tests/feature_tests.tsv` B172-5 rather than fixed, because
   SPEC-SUBOSC R4 already expects that AR to be struck when the voice envelope
   is wired, and a retune seam built now would be built for an envelope that is
   leaving.
5. **The SUB cluster's visibility is evidenced, not gated.** `gen_gui_controls
   --check`, `presentation_check` and `gui_reach` are green and
   `lab_load_check` loads `gui2.html` clean, but no wired gate asserts that the
   matrix row hides with the gate. A scripted DOM check was run as evidence
   (all 10 rows green, including a must-not-touch control on the other four
   display rows); it lives in the scratchpad, not in `tools/`, because a new
   tracked check is a new wired gate and that decision was not in the brief.
6. **The sub is engine-independent by construction.** It renders in SPECTRA
   mode too (its own `renderSubSpan` call in that branch) and follows SPECTRA's
   note-on. That was a choice, not a requirement — a source row that existed in
   only one engine mode, with nothing saying so, seemed worse.
