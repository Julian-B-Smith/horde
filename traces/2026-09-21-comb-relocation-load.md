# comb-relocation-load — a state load may resolve a fade shadow; a knob move may not

- **Queue item:** B188 (B187 refused — see "Open questions")
- **Why:** A preset load that had to MOVE Comb between slots lost it entirely.
  Comb is the rack's only singleton (one shared KS bank), and B117 makes a
  slot's fade-out SHADOW hold its type until the fade ends. A load arrives as a
  whole patch in a single drain: B192 resets every slot to Off first, arming the
  outgoing slot's crossfade, and the arriving slot's write is refused
  microseconds later against that shadow with nobody to retry. The fix scopes
  the resolution to the one caller the rule is wrong for. `FxRack::claimType`
  ends a fade that is the ONLY thing blocking a claim; the shell calls it only
  when `loadingState` is set (load, DAW session reload, history restore) and
  keeps `typeAllowed` for every live edit, which is what `fxxfade_check` T4
  pins. The cap itself is untouched on both paths: a LIVE second Comb is still
  refused, so the shared bank still has exactly one writer at every sample.
  The first attempt used `claimType` unconditionally and turned T4 red
  (`FX2 reads 5 during the fade (want 0 = refused)`); that failure is what
  located the correct scope.
- **Evidence consulted:** `src/fx_rack.h` (the cap and the B117 shadow, the
  `kSlotMaxInstances` argument), `src/hypersaw_clap.cpp` (the type-write choke
  point; `initState`'s B192 reset-before-load; the three `loadingState` set
  sites — the queued kind-3 path, the direct path and `state_load`),
  `tools/undo_check.cpp` (the self-retiring exclusion and its Notch control),
  `tools/fxxfade_check.cpp` T4, ROADMAP B188, ROADMAP B117/ADR-163.
- **The corpus that made the gate red (L0059):** NOT the factory bank — the bank
  holds no relocation, so a bank-only corpus passes on the unfixed binary. The
  discriminating case is the save/move/reload sequence `fxRelocate` performs
  (Comb into slot 1, snapshot, move to slot 3, reload the slot-1 patch). On the
  unfixed binary it printed `leaves slot 1 holding type 0 (wanted 5)` and the
  exclusion row read OK; on the fixed binary the exclusion went RED —
  `FAIL known gap ... WHEN THIS ROW GOES RED THE LOAD PATH IS FIXED` — which is
  the mechanism working and is why the exclusion was deleted rather than
  outliving the bug. The reverse direction (slot 3 -> slot 1) is now gated too,
  because the shadow and the live holder swap places with the slot order.
- **Controls kept, so the gate is neither a tautology nor a licence:** the same
  relocation with the uncapped Notch (green on the unfixed binary too, so the
  loss was the cap and not the probe); a genuine SECOND live Comb, which must
  still be refused; the same second-instance write with Notch, which must
  succeed.
- **Corpus widened:** the gauntlet no longer holds anything back for Comb — the
  type draw covers all ten types again and the two factory presets that place
  Comb (BS - Growl Bass, FX - Comb Throat) are back in the load pool (39 -> 41
  global presets), because a load now resolves the stale shadow a headless
  instance can never decay.
- **Alternatives rejected:** (a) let the cap count only live instances — that IS
  the double-write the cap exists to prevent, and it would have made
  `fxxfade_check` T4 red by design; (b) resolve shadows on every claim — tried,
  measured T4 red, reverted to the load-scoped form; (c) apply a load's four FX
  types in freeing order — B192's reset already establishes that order, so the
  extra pass would have displaced nothing.
- **Verify:** `./verify full`, exit 0, git 855f8d6 (per `.harness/last-verify.json`).
  `state_check`, `statefix_check`, `bank_check`, `preset_check`, `undo_check`,
  `fxxfade_check` all green in that run.
- **Open questions:**
  - **B187 was NOT fixed — grounded refusal, per the brief's own stop clause.**
    The brief's premise (one parameter with two disagreeing range declarations)
    is false, and the code states the reason the two differ. Id 45 `tilt` is
    SPECTRA's Amp Tilt, a roll-off EXPONENT (`spectra_core.h`:
    `amp[k] = 1/pow(k+1, p.tilt)`), range [0.5, 2]; id 71 `toneTilt` is SWARM's
    bipolar tone tilt whose SIGN selects HP/LP (`swarm_core.h`), range [-1, 1].
    Two different parameters on two different cores. The actual defect is an
    asymmetry: `applyParam`'s SPECTRA branch tests the RAW id (`id >= 44 &&
    id <= 55`) while `readParam`'s tests the BASE id (`d->id`), so id 45 returns
    early on write but its +1000 twin 1045 falls through to
    `cores[1].setParam("tilt")` and writes SwarmCore's tone tilt as well.
    Measured: writing 1045 = 1.5 makes `get_value(1071)` read 1.5 (outside its
    declared range), saves `o1.toneTilt: 1.5`, and a reload clamps it to 1.
    Control: id 45 = 1.5 leaves `toneTilt` at 0. Fixing it changes what osc-2's
    Amp Tilt control DOES — `tools/undo_check.cpp` already calls it "an ADR-sized
    decision about the interface". The `kAliasGapId` exclusion therefore stays,
    still earned, still self-retiring.
  - **The B174 asterisk symptom is confirmed, and NOT cleared by this change.**
    `presetMatches` compares `readParam(1071)` against the preset's stored
    `o1.toneTilt`, so a preset storing a tilt above 1 can never match after the
    loader clamps it. Measured on a preset authored by moving id 1045 to 1.5:
    `presetMatches` after loading it = false. Control: the same shape at 0.9
    (inside [-1, 1]) = true. It clears when B187 does, and only then.
  - **A MORPH that relocates Comb still drops it.** Morph writes types at the
    same choke point with `loadingState` false, so it takes the `typeAllowed`
    path. Not in scope here and not gated by anything; B117/T3 territory.
