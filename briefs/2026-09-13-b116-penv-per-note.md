# Dispatch brief — B116: ENV 2 (pitch envelope) becomes per-note

**Provenance.** HYPERSAW lead organ, 2026-09-13, for a scoped subagent with zero
conversation history. Motivating decision: ADR-162 (human ruling 2026-09-13
"Pitch envelope should be per-note"), superseding ADR-161's every-strike
restart of the SHARED envelope (merged 2026-09-13, PR #553). Read ADR-135,
ADR-161, ADR-162 and ROADMAP row B116 first.

## Acceptance criteria (verbatim from ROADMAP B116)

> (1) ENV 2 becomes PER-SLOT — `penv[kPoly]` `{level, stage}` advanced at the mod grid on the audio thread, preallocated: attack on that slot's note-on (fresh strike, retarget, steal — from the current level), decay to sustain while that slot is gated, release on its note-off. (2) Route 0 (knob 161, Env > Pitch) applies PER VOICE: `depth × penv[slot]` in semitones, composed with the per-note bend into ONE `setNoteExprAll(slot, bend + penv)` through a single composer (`noteExpr[slot] = {bend, penv}`); the MPE lane writes `bend`, the grid writes `penv`, nothing else calls `setNoteExprAll` directly; route 0 no longer contributes to the global `modPitchSt`. (3) Mod source slot 1 (ENV 2 for every OTHER route) = max over gated voices' `penv` — ENV 1's convention — so an ENV 2 → filter route behaves as before with one note. (4) `penv_check` grows: T4 hold 60, strike 64 → 60's `noteTune` stays 1.0 (no blip) while 64's rises to the peak; T5 release 64 with 60 held → 64 releases, 60 untouched; T6 source slot 1 still peaks on every strike; T1–T3 stay; `hypersaw_debug_voices` reports `noteTune`. (5) Bit-identical with Env > Pitch at 0 (`statefix_check`, goldens untouched). (6) No new ids; the behaviour change for patches with 161 ≠ 0 is recorded in ADR-162, not revision-gated (the shared blip was the bug).

## Where things are (so you do not re-derive them)

- The shared ENV 2 today: `src/hypersaw_clap.cpp` — search `env2Stage`
  (the stage block inside the mod-grid step, right after `mod.src[0] = envMax`),
  `env2Retrig` (set at the three `lastNoteKey = n->key;` note-on sites),
  `env2A/D/S/R` (params 162–165, handler + reader). Route 0's depth is
  param 161 (`modEnvPitch`); find how route 0 reaches `modPitchSt` (search
  `modPitchSt`, ADR-137/B69) and remove route 0 from that global sum.
- Per-note pitch: `setNoteExprAll(slot, semis)` → `SwarmCore::setNoteExpr`
  (`noteTune` multiplier, `src/swarm_core.h`; a fresh strike resets it to 1.0
  in `initVoice`, and the shell re-applies the latched MPE bend after a strike
  — search `seedNoteBend` / `ADR-038`). The MPE per-note bend lanes are
  `noteBend[kPoly]` (`stepNoteBends`, `setNoteBendTarget`, `seedNoteBend`).
  Build the composer there: one struct per slot holding `bend` and `penv`,
  one function that calls `setNoteExprAll(slot, bend + penv)`.
- Slot ↔ voice: `tags[kPoly]` (`NoteTag`: active/key), `slotOf[slot][osc]`.
  The gate of a slot is `cores[k].voiceAt(slotOf[slot][k]).gate`.
- Headless exports for the check: `hypersaw_debug_penv` (extend or replace —
  it is diagnostic-only), `hypersaw_debug_voices` (add `noteTune`).
  `tools/penv_check.cpp` is the check to grow; `tools/notefuzz_scaffold.inc`
  is the rig (include `<algorithm>` for MSVC).
- Do NOT touch `SwarmCore`'s render path; `setNoteExpr` already exists.

## Files in scope

`src/hypersaw_clap.cpp` (ENV 2 block, note-on/off sites, the per-note
composer, route-0 exclusion, the two exports), `tools/penv_check.cpp`,
`traces/2026-09-13-b116-penv-per-note.md`. `src/swarm_core.h` ONLY if
`setNoteExpr` needs a companion accessor (say so).

**OUT of scope:** `ROADMAP.md` / `DECISIONS.md` (lead-only — report text);
`./verify` and gates; the FX rack (another stream is in `src/fx_rack.h` and
the FX type choke point in `hypersaw_clap.cpp` — you will both touch
`hypersaw_clap.cpp` in different regions; REBASE onto main before opening
your PR); `reference/**`, `specs/**`; `src/gui/**` (no GUI change);
untracked root files.

## Constraints

Branch from `main` (pull first); build with absolute paths
(`cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`,
`cmake --build "<abs>/build-release" -j8`); `./verify fast` after each change
set and gate every scripted commit on its exit code; `./verify full` before
done (it includes `rtsafety_probe`, `state_check`, `undo_check`); paste
oracle output verbatim; red halts you; no allocation on the audio thread;
no machine identity in tracked files; MSVC builds this in CI (no
`__attribute__`, no VLAs).

## Deliverable

Branch `b116-penv-per-note`, pushed, PR via `gh pr create --base main` whose
body leads with `penv_check` output (before/after for T4) and the
`./verify full` tail pasted from the run. **Never merge.** Final report: PR
URL, the check output, verify tail verbatim, the ROADMAP/DECISIONS text you
would add.
