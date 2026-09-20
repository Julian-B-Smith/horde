# mod-lfo-env — two LFOs and two envelopes as modulation sources (B171)

- **Queue item:** B171 (ROADMAP row written by the lead in parallel; this change
  set is its acceptance). ADR-181 is the forward reference the pins cite.
- **Why:** The human, 2026-09-19, verbatim: "Let's also add a couple LFOs and
  envelopes to the mod page; we can make them more robust later, but I'm tired
  of not having them." The deliberately simple version — two LFOs, two
  envelopes, as SOURCES only.

## What landed

**Source slots (frozen, appended — never inserted).** 18 = LFO 1, 19 = LFO 2,
20 = ENV 3, 21 = ENV 4. `ModCore::kMaxSources` stays 24 and `src/mod_core.h` is
UNCHANGED: the slots already existed reading 0, and the polarity table is the
shell's. LFO 1/2 declared `kSrcBipolar` in `makeModCore()`; ENV 3/4 inherit
unipolar.

**Parameters, ids 269–288, all GLOBAL and all Device class.**

| ids | what |
| --- | --- |
| 269–274 | `lfo1Rate` `lfo1Shape` `lfo1Sync` `lfo1Beats` `lfo1Retrig` `lfo1Phase` |
| 275–280 | the same six for LFO 2 |
| 281–284 | `env3A` `env3D` `env3S` `env3R` (exactly ENV 2's ranges/defaults) |
| 285–288 | `env4A` `env4D` `env4S` `env4R` |

Global + Device keeps all twenty out of `buildMorphOrder`'s frozen morph prefix
and satisfies `paramclass_check`'s "no morphIds member is device" cross-check.

**Pins that moved.** Exactly one: `tools/paramclass_check.cpp` T1a
`baseRows == 246` → `== 266`, with its reason at the pin. Every other pin held —
`polarity_check` gained rows (it did not move one), and `mod_check`'s
`kMaxSources` assertions are untouched because the constant did not change.

**Regenerated, not hand-edited.** `src/gui/gui2.html` `<!--GEN:MOD-->` block
(four clusters), `src/depends_graph.h`, and the 40 factory patch JSONs
(`gen_factory_bank`, whose own header says a parameter-table change is absorbed
by re-running it). The bank regeneration was verified STRUCTURALLY, not by
eyeball: 44 files parsed and compared key-by-key against HEAD — the only
difference is the 20 new keys at their defaults plus the `build` stamp (which
`state_load` explicitly never reads back). The four corner presets are
byte-untouched, which is the globals-are-not-in-the-morph-field claim showing up
as a fact rather than an assertion.

## Decisions taken inside the brief

1. **`lfoNBeats` is a KNOB, not a select over a division list.** The brief said
   "a division list matching the Delay's beats param (read ids 232–234 and reuse
   its label table)". THERE IS NO SUCH LABEL TABLE: `d1beats` (id 234) is
   continuous 0.0625–8 with `labels == nullptr`, and
   `src/param_presentation.tsv:346` renders it as `knob` with unit `/beat`.
   "Matching the Delay's beats param" and "reuse its label table" cannot both be
   satisfied; I took the former, which is the reduce-not-invent reading, and
   defaulted to 1 beat = a 1/4 note (bpm counts quarter notes, and the delay's
   law is `seconds = beats * 60/bpm`). **Open for the lead** — see below.
2. **ENV 2's law was EXTRACTED, not copied.** `advanceAdsr()` is now the single
   copy, called by ENV 2, ENV 3 and ENV 4. ENV 2 is bit-identical: same
   branches, same order, same constants, proven by `parity_check` 156/156,
   `state_check`, `statefix_check`, `bank_check` and `penv_check` all green.
3. **The retrig flag is consumed ONCE and shared.** `penv[s].retrig` is set at
   the three existing note-on sites; `modStep` reads it into a local `strike`
   and hands it to all three envelopes, and an `anyStrike` derived from it
   drives the LFOs' retrig mode. No new flag at any note-on site — that is
   L0029's shape (one signal, distributed from one place) applied one level
   down, and it is why ENV 3/4 cannot drift out of step with ENV 2.
4. **The S&H stream is seeded from the patch seed XOR a golden-ratio-scaled
   index**, not a bare `^ i`: a bare XOR hands LFO 1 the patch seed verbatim
   (the same stream every other consumer of that seed draws) and LFO 2 its
   immediate neighbour. Re-seeded on the `seed` param, the same breath
   `SwarmCore::rebuild()` re-rolls its own stream.
5. **The `lfo=` chunk key follows B149's rule exactly** — emitted ONLY once a
   S&H stream has drawn, and emitted AFTER `seed` (whose apply re-seeds it).
   That is what keeps every existing chunk, fixture and factory file byte-for-byte
   what it was, and it is why `state_check` / `statefix_check` / `bank_check`
   remain the regression proof rather than three fixtures to regenerate.
6. **NO SMOOTHING WAS ADDED, and this is a recorded limit.** The generic
   destination path (`modStep`, the `base + deltas[i] * span` apply) has no
   filter on it — I checked; `modPitchSm` is the pitch lane's own one-pole and
   does not reach the generic path. Per the brief I did not add one. **A square
   or sample & hold LFO into an audio-rate destination therefore STEPS at the
   172 Hz mod tick (256/44100 s) and will be audible as a zipper.** Recorded in
   the code at the LFO block, and as human test row B171-5.
7. **The refusal is pinned (L0036).** Ids 269–288 are refused as mod
   DESTINATIONS in `modAddRoute` and excluded in `modDestOptions()`, both with a
   comment naming "LFO modulates LFO" as a later feature needing B70's cycle
   rule. `lfoenv_check` section I is the must-read-zero control.

## Oracle

`tools/lfoenv_check.cpp`, **WIRED** into `./verify full` (`verify:236`) in this
PR — ADR-180 §1 inverted the default, and `test_table_check` enforces
wired-or-explained. Nine sections, 40 assertions, every section carrying a
control that must read the other way. One new debug export,
`hypersaw_debug_modsrc`, declared in `src/hypersaw_debug.h` FIRST (its own
header's rule) and returning NaN past the table rather than 0 — because 0 is a
legitimate reading for an unassigned slot, so a silent clamp would forge
section I.

**It was RED on arrival, three times, and each red was a finding about the
check, not the code** (L0055 — run and read a new check before calling its PR
green):

1. *"saw up is MONOTONE"* failed at 1 drop. A one-cycle window taken at an
   arbitrary phase contains exactly one wrap. The assertion was wrong; it now
   reads "rises monotonically with exactly ONE wrap", with the sine's 86 drops
   as the contrast.
2. *"ENV 3 reaches exactly 1 in the stated attack seconds"* (the brief's clause
   f) failed at 0.912. **The ADSR knobs are one-pole TIME CONSTANTS, not
   times-to-peak** — that is ENV 2's law, which decision 4 says to mirror
   exactly, so the level is `1-exp(-t/tau)` and the snap to 1 happens past 0.99,
   about 4.6 tau. Measured at 7 tau it is `== 1.0` at all three rates. The
   brief's clause described a law ENV 2 does not have; the law is right and the
   clause was wrong.
3. *"route added on slot 21"* failed because I aimed it at id 5 (Detune Law),
   which is STEPPED and correctly refused by `modAddRoute`. Re-aimed at Inertia
   (11).

## Evidence consulted

`src/mod_core.h` (kMaxSources, srcPol, mapPolarity), `src/hypersaw_clap.cpp`
(kParams tail 268, kGlobalIds, kParamClassOverrides, makeModCore's polarity
table, modStep's ENV 2 block, modAddRoute's 161–177 refusal, the `ens=` chunk
idiom at state_save/state_load, plug_activate's B149 carry-across),
`src/param_presentation.tsv:344-346` (the Delay's beats row — the finding in
decision 1), `src/gui/gui2.html` (MOD_SRC_NAMES, pg-MOD, modDestOptions),
`tools/gen_gui_controls.py`, `tools/test_table_check.py` (the wired-or-explained
rule and the (page, feature) coverage rule), `tools/polarity_check.cpp` section
C, `tools/penv_check.cpp` (the Rig scaffold), `tools/gen_factory_bank.cpp`'s
header, CLAUDE.md §Domain "Gate wiring" (ADR-180 §1). LIBRARY: L0003 (M_PI is
banned — `portability_gate` would have failed the file; used a local `kPi`),
L0023, L0029, L0032, L0036, L0055.

## Alternatives rejected

- **A second copy of the ADSR loop for ENV 3/4** — rejected; a second copy of an
  envelope is the repo's named failure and would have made ENV 2's bit-identity
  a thing to maintain rather than a thing that is structural.
- **A per-LFO retrig flag set at the three note-on sites** — rejected; three
  sites × three flags is nine chances to miss one. Derived from the existing
  flag instead.
- **Adding a one-pole to the generic destination apply** — rejected because the
  brief rules it out ("robust later"); recorded as a limit instead of silently
  fixed or silently ignored.
- **Editing `tools/notefuzz_scaffold.inc` to carry transport events** —
  rejected; `clap_process_t::transport` is the door `process()` already reads
  tempo through, so the sync branch under test is the shipped one and no shared
  scaffold moved.

## Verify

`./verify full` — exit 0, on the tree that became the commit below.
`lfoenv_check: 0 failure(s)` visible in the log; `parity_check: 156/156 within
eps=1e-06 (worst 4.262e-09)`, `state_check: GREEN (0 failures)`,
`statefix_check: GREEN (3 fixtures, 0 failures)`, `bank_check: 0 failure(s)`,
`penv_check: PASS`, `rtsafety_probe: GREEN (audio thread is allocation-free)`.
The first full run was RED at `bank_check: 41 failure(s)` — the factory bank was
stale against the new parameter table — and went green after regeneration.

## Open questions

1. **`lfoNBeats` shape (decision 1).** It ships as a continuous knob over
   0.0625–8 beats, matching `d1beats`. The brief asked for a stepped division
   list; no such list exists anywhere in the tree to reuse. If the human wants
   1/1 · 1/2 · 1/4 · 1/8T · 1/16 · … as named steps, that is a new label table
   and a new convention — and it should arguably land on the DELAY's four beats
   params in the same change, or the device will spell the same idea two ways.
   Additive within the same id; nothing about it is frozen.
2. **`lfoNPhase` is a START phase, not a running offset**, per the brief. A
   free-running LFO reads it once at `activate()`, so turning the knob does
   nothing audible until the plugin re-activates. That is literally what the
   brief specified and it is probably not what a player expects from a knob
   labelled "Start Phase" sitting on the panel. Worth a ruling.
3. **The 172 Hz stepping** (decision 6) is unmeasured as an audible defect — I
   recorded the arithmetic, not a listening result. Human test row B171-5 is
   where it gets judged.
4. **ADR-181 and ROADMAP B171 do not exist in the tree yet** at the time of
   writing; `paramclass_check`'s pin reason and four `feature_tests.tsv` rows
   cite them as forward references, on the lead's statement that they are being
   written in parallel. If they land under different numbers the citations need
   correcting.
5. **`lfoReseed()` fires for ANY oscillator's seed param** (`baseIdOf(id) == 3`),
   so automating osc 2's seed rewinds both LFOs. Defensible (one device, one
   seed) but it is a behaviour nobody explicitly ruled.
