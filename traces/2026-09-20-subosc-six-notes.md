# subosc-six-notes — the human's six Sub Osc notes, one commit each

- **Queue item:** ROADMAP B181 (the human's six notes after playing the
  installed build, 2026-09-20). Dispatched by the horde lead session with the
  protected-path edits for note 1 (and anything note 4 genuinely needed)
  sanctioned by the human's request.

- **Why:** Six field reports on the SUB OSC block, one of which (note 6) turned
  out to be a defect wider than the sub. Each note is a commit; the sixth is the
  one that changed behaviour for every existing preset and therefore carries the
  most evidence.

  1. **Three octaves below.** The octave floor moves −2 → −3 in the three places
     that are one fact (`SubOscCore::kParamTable`, the lab's `SUB_PARAMS` +
     `<select>`, SPEC-SUBOSC §4/§7) plus the shell row a `static_assert` already
     holds to the core table. **The default stays −1**, so the widening is
     bit-inert for every stored patch. Two new golden scenarios reach −3
     (including the absolute bottom, MIDI 0 → 1.021975 Hz) because a superset
     range the reference lacked is zero coverage (L0031 (B)), and a new
     subosc_check section asks about the floor in ABSOLUTES rather than by
     agreement — closed-form frequency, finiteness, no subnormals over seven
     shapes × 2 s with a release, peak still under the row's 1.425 headroom
     divisor, and a control that −3 is an octave below −2 rather than −2 clamped.
     Measured at the floor: 0 non-finite, 0 subnormal, worst peak 1.381775.
  2. **The sub's own mono, `lowest` bias, glide.** Shell-side, because that is
     where voice assignment already lives for the swarm (`voiceMono`,
     `voiceLegato`, the `glide` lane). The core gained ONE thing: a
     `pitchOffsetSt` input in semitones, deliberately not a §7 row — the block's
     id map is positional under a frozen gate at 4015, and at its default 0 the
     law reads `m + 0.0`, so parity is untouched (worst rms still 0.000e+00).
     New ids 4016 mono / 4017 bias / 4018 glide (seconds, ADR-009) / 4019
     pitchMod. `subOscRowsAgreeWithCore` generalised, not weakened.
  3. **A wave display, drawn by the ENGINE.** `SubOscCore::shapeAt` extracted out
     of render()'s inner loop and called by BOTH render and the shell's
     `subWaveJson`, so there is nothing for the picture to disagree with. This is
     the deliberate non-repeat of B177 (the GUI computing the LFO shape
     independently of the shell) and of ADR-110's scar.
  4. **A pitch-envelope destination.** `sub.fine` already routed — measured, not
     assumed — so the gap was RANGE (±100 c is one semitone). `sub.pitchMod`
     (±48 st, the instrument's own pitch range) is the dedicated offset, which
     leaves `fine`'s meaning alone and costs no `recalc()` on the mod path.
  5. **A MIX strip**, in B178's proxy idiom: the same ids the OSC page writes,
     `data-fixed` + `data-proxy`, no second parameter.
  6. **"A load is a load."** `applyStateJson` held two contradictory rules in one
     function: routes and the intent chunk treat an ABSENT key as "unbound, never
     whatever the previous patch had", while every parameter loop SKIPPED an
     absent key. An absent parameter now restores its `defaultFor()` default —
     instrument table, engine blocks and osc-2 twins alike.

- **Evidence consulted:** ROADMAP B181 (via the brief); `src/subosc_core.h`
  (the table and the parity contract in its header); `src/hypersaw_clap.cpp`
  (kSubOscParams + `subOscRowsAgreeWithCore`, `paramClassOf`'s engine branch,
  `morphInit`'s engine append, `cornerJson`'s layout-marker rule, `renderSubSpan`,
  `handleNoteOn/Off`'s mono paths, `applyStateJson`, `state_load` for the sibling
  path); `src/morph_core.h` (`reshuffle` draws gShared AFTER the per-parameter
  draws — the mechanism behind the intent_check pin move);
  `src/param_presentation.tsv` and `tools/gen_gui_controls.py` (the generator
  reads the shell's ParamDef arrays, so a widened range becomes a widened
  control); `src/gui/gui2.html` (B178's proxy idiom, `setControl`'s base
  derivation); `tools/golden/gen_subosc_goldens.mjs`, `tools/subosc_check.cpp`,
  `tools/bank_check.cpp`, `tools/intent_check.cpp`; LIBRARY L0023, L0029, L0031,
  L0032, L0033, L0036, L0051, L0053.

- **Alternatives rejected:**
  · *Note 2/4, a sixteenth core parameter row.* Rejected: `id − 4000` IS
    `SubOscCore::Param` and the gate is frozen at 4015, so a sixteenth row would
    collide with a shipped CLAP id. Moving a frozen id is the worse trade.
  · *Note 4, widening `fine` to pitch-envelope range.* Rejected as the brief
    predicted: it moves an existing parameter's meaning and every stored value of
    it. Also measured: modulating `fine` reruns the BUMP peak search on all
    sixteen instances per mod tick, where `pitchMod` is a single store.
  · *Note 3, drawing the shape in JS.* Rejected — that is B177's defect and
    ADR-110's scar. Extracted the one law instead; parity proves the extraction
    changed no arithmetic.
  · *Note 5, a second level id for the MIX strip.* Rejected by B166's rule.
  · *Note 6, leaving the engine-block loop's skip in place.* Rejected: its
    comment's premise ("the block ships off, so it stays inert") expires the
    moment a player switches the block on, which is exactly what the human heard.
  · *Classing `sub.glide`/`sub.pitchMod` Device to avoid the morph append.*
    Rejected: the instrument's own `glide` and `fine` are morphable by ADR-173's
    rule, and an override needs a reason, not a convenience.

- **Calibration that did NOT fire, recorded rather than retried (L0033):** the
  40-patch byte-identity assertion (bank_check E) was run against the PRE-FIX
  binary and PASSED 40/40. That is E's coverage boundary, not a failure of E —
  the factory bank was re-saved when the SUB block landed, so no factory patch
  has a hole, and skip-vs-default agree when nothing is absent. Row F punches 52
  holes (every 7th key) in a factory patch and asks the same question; F-ANCHOR
  strips every `sub.` key. Pre-fix both are RED (`F` first differs at byte 66,
  `"n":7` vs `"n":8`; `F-ANCHOR` reads on = 1, wave = 5), post-fix both GREEN.

- **Found on the way, fixed here, named in its commit:** `setControl()` derived
  `id % OSC_STRIDE` for EVERY id, which ALIASES an engine-block or routing id —
  4015 (SUB On) reduces to base 15, `mono`/Mono Fold, which is global, so the
  echo painted the MIX page's Mono Fold checkbox with the sub's power state;
  4011 and every routing cell did it too. Guarded with `if (id >= 3000) return;`
  after the fixed paint. Also: `subWaveJson`'s header went through a `char[32]`
  and truncated mid-key — caught by 11i on its first run.

- **A moved PIN, with its reason at the pin:** `intent_check`'s T4 CONTROL went
  red on the first `./verify full`. Cause is structural, not a defect: appending
  two morphable ids lengthens `morphIds`, and `MorphField::reshuffle` draws the
  SHARED gumbel AFTER every per-parameter draw, so a longer list shifts
  `gShared` and (with `morphCoup` > 0) re-decides ownership everywhere — seed
  1024 read corners 1/0 off-centre before and 0/0 after. The seed search now
  covers the control's own position, which is the control's PREMISE; the
  COLLAPSE half is not searched on, and a new `premiseFound` guard makes the row
  FAIL if no seed splits at both, where the old loop silently kept the last one.

- **Verify:** `./verify full`, exit 0, git `914bcbf`
  (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"914bcbf",...}`).
  Parity green after the golden regeneration: 60 goldens at 48 000 and 44 100 Hz,
  worst rms 0.000e+00. subosc_check GREEN with 21 new rows; bank_check 0
  failures over 40 patches; intent_check 32 fixtures 0 failures; state,
  statefix, presetstore, preset, undo, paramclass, morphlayout all green.

- **Open questions:**
  1. `state_load` — the DAW-session key=value path — has the SAME absent-key
     skip as `applyStateJson` did. It does not bite a fresh session restore (the
     instance starts at defaults) but has the same shape when a host re-uses an
     instance. NOT fixed: out of this brief's scope, which named
     `applyStateJson`. A ROADMAP row is owed.
  2. In SPECTRA mode `handleNoteOff` returns after `spectra.noteOff` and never
     reaches the sub, so a sub struck in that mode is never released. Pre-existing
     (B172); SPECTRA is parked. Not touched.
  3. Hard sync stays the pinned refusal (11e). The sub's mono retarget does NOT
     re-strike (it glides); the SWARM's mono retarget still re-strikes the sub,
     which is B172's recorded divergence and is unchanged.
  4. The glide advances once per `kMixChunk` (256 samples, 5.8 ms at 44.1 kHz)
     because the core recomputes its phase increment once per `render()` call.
     A per-sample glide would mean changing a parity-gated loop.
  5. `sub.mono` silences the block when the toggle crosses, by the same
     reasoning ADR-099 A1 applies to a power switch. That is a deliberate
     audible discontinuity, not a bug.
