# b116-penv-per-note — ENV 2 (the pitch envelope) becomes one envelope per note slot

- **Queue item:** B116 (dispatch brief `briefs/2026-09-13-b116-penv-per-note.md`,
  HYPERSAW lead organ 2026-09-13, under ADR-162).
- **Why:** ADR-135 made ENV 2 one shared ADSR; ADR-161 restarted it on every
  strike, which fixed the shrinking spike and gave every HELD note a blip at
  the same time. The human ruled 2026-09-13 "Pitch envelope should be
  per-note" (ADR-162), which subsumes ADR-161: per note, every strike IS a
  fresh envelope, and a strike can no longer reach a note it did not strike.

## What changed

- `penv[kPoly]` `{level, stage, retrig}`, preallocated, advanced at the mod
  grid on the audio thread (`src/hypersaw_clap.cpp` `modStep`). `retrig` is set
  by the three note-on sites (spectra / mono incl. retarget / poly — the same
  sites that used to set `env2Retrig`), so a fresh strike, a retarget and a
  steal all restart that slot's attack from its CURRENT level.
- **One composer for the per-note pitch multiplier.** `noteTune` is a single
  multiplier per voice and there are now two independent per-note offsets (the
  MPE bend lane, ENV 2). `noteExpr[slot] = {bend, penv, emitted}`; the bend
  lane calls `noteExprSetBend`, the grid calls `noteExprSetPenv`, and only
  `emitNoteExpr()` calls `setNoteExprAll` — which now has exactly one caller,
  so the rule is checkable rather than remembered (L0029).
  `resetNoteExpr(slot)` at each note-on mirrors the core's own
  `noteTune = 1.0` reset (ADR-036/038); without it the cache claims a value
  the core has thrown away.
- Route 0 no longer feeds the global `modPitchSt`/`modPitchSm` lane. The lane
  stays as the generic global-semitone seam and settles to exactly 0.
- Mod source slot 1 (ENV 2 for every OTHER route) = max over GATED slots'
  `penv` — the literal ROADMAP wording; see Open questions.
- Retired: `env2Gate`, `env2Retrig`, `anyGate` — all three were properties of
  the sharing, not of an envelope.
- Exports: `hypersaw_debug_penv_slot(p, slot, &level, &stage, &semis)` added;
  `hypersaw_debug_voices` appends `noteTune` (appended, never inserted —
  `tools/preset_probe.cpp` prints the row positionally).
- `tools/penv_check.cpp` grows T4 (held note does not blip · struck note rides
  the per-note lane · global lane reads 0), T5 (per-note release, held note
  untouched), T6 (source slot 1 still peaks at depth 0, and no voice's
  `noteTune` moves there).

## Evidence consulted

- Brief `briefs/2026-09-13-b116-penv-per-note.md` (branch `lead-records-7`),
  ROADMAP row B116, ADR-135 / ADR-137 / ADR-161 / ADR-162.
- `src/hypersaw_clap.cpp`: `modStep` (mod grid), `setNoteExprAll` + `slotOf`
  (the constructed slot↔voice map), `NoteBendLane` / `seedNoteBend`
  (ADR-038/097), `modPitchRouteIdx` (ADR-138: the pitch route is found by
  DEST, never by index), the three note-on sites.
- `src/swarm_core.h`: `setNoteExpr` (`noteTune`), `initVoice`'s reset.
- INDEX.md L0024, L0029, L0032, L0033.

## Calibration (the part that nearly went wrong)

The first draft of T4 asserted the held note's `noteTune` stayed 1.0. Run
against the shared-envelope build (origin/main + the two diagnostic-only
exports, so the probe could read it at all) that assertion **passed** — a plant
that did not fire (L0033). It passed vacuously: before ADR-162 the shared blip
was applied through the GLOBAL lane (`modPitchSm` → `updateTuneAll`), where
`noteTune` structurally cannot see it. A detector built on the lane I was
adding shared its subject's assumption (L0032). T4/T5 now measure the TOTAL
applied offset — `modPitchSm + 12·log2(noteTune)` — which is what the player
hears. Re-run against the shared build:

    FAIL  T4 the STRUCK note bends to the peak (11.0773 st >= 11.9 at depth +12)
    FAIL  T4 and it rides the PER-NOTE lane (its own noteTune 1.0000 >= 1.99)
    FAIL  T4 the HELD note does not blip (max total offset 11.077324 st < 1e-3; shared env gave +12)
    FAIL  T4 route 0 left the GLOBAL lane (max |modPitchSm| 1.108e+01 st == 0)
    FAIL  T5 control: the still-held note is untouched (largest rise -1.29e-04 <= 0, max total offset 6.016809 st < 1e-3)

and against this build all five read `0.000000` / `0.000e+00`. Five orders
between pass and fail, so the thresholds are not load-bearing (L0024).
Observation anchors were added too: a "max deviation 0.000000" computed from a
voice the probe never FOUND is indistinguishable from a pass, so T4/T5/T6
assert the number of blocks in which the voice was actually read.

## Alternatives rejected

- **Deleting the global pitch lane** now that nothing feeds it. Rejected: it is
  the generic "global semitone offset" seam `updateTuneAll` already sums, and
  removing it would touch tuning code the brief does not scope. It is inert,
  not wrong, and `penv_check` T4 now ASSERTS it reads 0.
- **Writing the strike's pitch offset at note-on** as well as at the grid, to
  remove the ≤1 mod tick (16 samples, 0.36 ms) of lag. Rejected: the ROADMAP
  says the grid writes `penv`, and a second writer is precisely the hazard the
  composer exists to prevent.
- **Gating the grid's per-slot writes on `tags[slot].active`**. Rejected: the
  tag is retired when the NOTE_END is pushed at gate-fall, while the release
  tail is still sounding — the envelope's RELEASE stage would have stopped
  being applied exactly where it is audible.

## Verify

`./verify full` → exit 0, `.harness/last-verify.json`
`{"target":"full","exit":0,"git":"b16fcd4","ts":"2026-09-13T21:28:12Z"}`.
`statefix_check tests/state_fixtures` (not in `./verify` — protected path) →
GREEN, 3 fixtures, 0 failures, planted-control fires: criterion 5's
bit-identity at depth 0. `penv_check` → PASS, T1–T6.

## Open questions

1. **Source slot 1 is max over GATED slots, the ROADMAP's literal wording — and
   that wording is in tension with its own justification.** The criterion says
   "max over gated voices' `penv` — ENV 1's convention — so an ENV 2 → filter
   route behaves as before with one note". ENV 1 (`mod.src[0]`) is the max over
   ALL voices' amp envelope, gated or not, so it decays smoothly through a
   release; max-over-GATED makes slot 1 SNAP to 0 the instant the last key
   lifts, where the shared envelope used to release over `env2R`. With the
   default sustain 0 the two are indistinguishable (the envelope is already at
   ~0 by key-up); they differ only with sustain > 0. Implemented literally.
   One line to change if the lead wants "as before" to win over "gated".
2. `tools/preset_probe.cpp` prints the voice row under the label
   `voices[slot,midi,gate,f0,f0cur,glide]`, which is now one field short. The
   file is out of scope; the appended field does not move any other column.
3. `penv_check` remains unwired from `./verify` (human gate, `./verify` is a
   protected path). It is the only oracle for ADR-162's claim.
