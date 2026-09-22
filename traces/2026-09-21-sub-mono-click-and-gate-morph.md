# sub-mono-click-and-gate-morph — the mono overlap click is a phase reset, and the block's gate joins the morph field

- **Queue item:** B202 (the click) and B203 (the gate). Named in the dispatch
  brief; **neither id exists in ROADMAP.md at the hash this branched from**
  (`git grep -n "B202\|B203" ROADMAP.md` → nothing, on `origin/main` and on the
  lead's working tree). The brief carried its own acceptance criteria verbatim,
  which is what this was built against; recording the gap rather than treating
  the brief's text as if it had been quoted from the roadmap.
- **Why:** The human, playing the instrument, 2026-09-21: (a) "When the Sub Osc
  is set to mono, there's a click artifact when notes overlap." (b) "Sub on/off
  is still exempt from morph and it ought to be wired in the way the other two
  oscs are." On (b) the lead's B195 ruling — that an engine block's GATE is
  Device and therefore out of the field — is **overturned**: the oscillators'
  enable (150) is stepped, carries no class override, is Structural, and has
  been in the field with B48's ramp since the field existed. Two power switches
  answering to two different rules was tidy in the code and audible as an
  inconsistency at the pad.

## B202 — WHICH discontinuity, measured before anything was changed

Mono has exactly ONE re-strike path. `subStruckKey` is cleared when the last
key lifts (`subMonoNoteOff`, src/hypersaw_clap.cpp), so the next press STRIKES
instead of gliding, and `SubOscCore::noteOn` REPLACES the sounding note
(subosc_core.h's header). `release` ships at 0.08 s, so in ordinary playing the
previous note's tail is still running when that strike lands, and the phase
reset underneath a live envelope is a full-scale sample-to-sample jump.

**Ruled out, not assumed.** At the strike instant three things could move:
phase, envelope, glide target. The envelope does NOT restart — `noteOn` sets
`stage_ = kStageAttack` and leaves `e` where the release left it, so it is
continuous. The glide target is reset to 0 with `subStruckKey`, which changes
the phase INCREMENT and therefore the slope, never the value. `ph_` is the only
quantity that jumps, and the measurement with velocity held constant isolates
exactly it.

**The detector is station_check 3.12/3.15's, in its unit** — |step at the
event| over the LARGEST natural inter-sample step in the cycle before it, gated
at 2x — over 16 phases, median. Sixteen because at 48 kHz a 256-sample block
advances MIDI 60's 183-sample cycle by 73 samples, so varying the sustain
length sweeps where the event falls; station_check's own note records what a
single reading is worth ("luck, not safety").

Verbatim, same row, before and after the one-line change:

```
   overlap re-strike into a live release tail: 18.18x the natural step (worst of 16 phases 21.50x)
   control  mono TOGGLE crossing (deliberate, ADR-099 A1, unruled): 20.87x (gate: > 2x — must step)
FAIL 11g.e mono overlap is click-free: ...  overlap 18.18x
```
```
   overlap re-strike into a live release tail: 1.44x the natural step (worst of 16 phases 1.80x; 18.18x before B202)
   same re-strike at velocity 1.0 -> 0.3: 11.91x   REPORTED, NOT GATED — see the coverage note
   control  mono TOGGLE crossing (deliberate, ADR-099 A1, unruled): 20.87x (gate: > 2x — must step)
PASS 11g.e mono overlap is click-free: ...  overlap 1.44x
```

**The toggle is a DIFFERENT discontinuity and now cannot be confused with it.**
Crossing `sub.mono` under a sounding note calls `subAllOff`, which zeroes the
envelope and the filter state in one sample; that is deliberate (ADR-099 A1's
"off must silence, not freeze") and awaits its own ruling. It reads 20.87x
through the IDENTICAL detector, which is why it is the row's must-step control:
a detector that had quietly stopped measuring would pass the overlap half by
reading zero everywhere.

**The fix reuses a word this repo already owns.** `SwarmCore::retargetNote`
takes `keepPhase`; `SubOscCore::noteOn` now does too, defaulting FALSE, so every
golden, every parity render (worst parity rms 0.000e+00, unchanged) and every
strike from silence is bit-identical. The shell passes true only where the
alternative is a click (`env > 0`). The `phase` parameter is still obeyed on
every note that starts from silence, which is the only place a player can hear
it. The RNG is re-seeded either way — audit A3's "pure function of (seed, note)"
is a determinism property, phase continuity is an audio one, and neither is
traded for the other.

**Mono and not the poly path beside it**, because that is the structural
difference the sub has from an oscillator: poly hands the sub the SWARM
allocator's slot and `alloc()`'s tiers 1 and 2 read `env`, so a fresh note
lands on an idle or quietest voice and a live tail is the case that allocator
exists to avoid. Mono has one slot and no such choice.

**The coverage boundary, measured and named (L0033).** A re-strike whose
VELOCITY differs still steps: 11.91x at 1.0 → 0.3, REPORTED and not gated. Its
size is the product of the INHERITED envelope level and the velocity change,
and the inheritance is SPEC-SUBOSC §5.3's PROVISIONAL per-module AR that open
ruling R4 already expects the voice envelope to replace. Closing it here means
either cutting the tail (a bigger step) or the shell writing the core's `env`,
which that member's own comment forbids. Left visible rather than paid for
against a stage that is leaving.

## B203 — the gate joins the field, as a LEVEL RAMP

`paramClassOf`'s gate branch returns **Structural** now, with no override —
rule 2 applied, exactly as it is to id 150. The reason lives at the branch.

**The append is a THIRD pass, and that is the whole risk.** The gate is
Structural, so B195's second pass would have taken it IN BLOCK ORDER, putting
4015 between 4014 (seed) and 4016 (mono) and shifting every slot after it —
silently re-reading every corner ever saved against the wrong parameter. So the
two existing passes `continue` past `b.gateId` (they otherwise produce the
order they produced before, textually unchanged) and the gates are appended
after both.

*Proven three ways, not asserted:*
1. **Positionally, by the gate that already existed.** `T10b the 9 Structural
   engine ids are the TAIL of the order (first at slot 264 of 273) — no
   previously stored slot moved` (it read `8 ... 264 of 272` before).
2. **By the regenerated bank.** Comparing `BS - Reese.json` before and after:
   `morphCorners lengths [272,272,272,272] -> [273,273,273,273]`, `prefix
   identical: True`, `appended tail: [[0],[0],[0],[0]]`. Every stored value is
   where it was; the new slot holds 0, the gate's default, so every shipped
   patch keeps its sound.
3. **By the class test, not a list.** T10 asserts both directions over ADR-088's
   whole engine span off the shell's own enumeration, so STATION inherits it.

**The ramp is B48's, and every property its comment claims is claimed here.**
`morphApplyGateEnable` takes the plain bilinear `w[]` — not the Gumbel draw,
not the resolver's sharpened weights — so the ramp is deterministic in the pad
position under all three laws; it is the same helper in `morphStep` and in
`intentApply`; the stepped flip is deferred to the weight floor (1e-3, ~−60 dB)
where `subAllOff`'s kill and the re-strike still run but inaudibly. The law
itself was EXTRACTED (`morphOnWeight`) rather than copied into the sibling —
ADR-110's reason.

*Where the sub differs from an oscillator, and why the handling is the same
anyway.* An oscillator's ramp lands in `applyOscGainAndMeter`, which already
existed for the mixer's mute/solo faders. The sub has no strip of that kind, so
`renderSubSpan` carries the ramp itself, through the SAME `gainSmoothCoef()`
one-pole, computed ONCE PER CHUNK and OUTSIDE the sixteen-slot loop — the gate
is the ROW's switch, and advancing the smoother inside the accumulation would
run it sixteen times per sample and make the ramp 16x too fast. And the sub's
kill is TOTAL (phase, envelope and filter state) where an oscillator's is a
voice kill, which is why the deferral to the weight floor matters more here.

**Two behaviours the ramp forced, both found by building it and neither in the
brief:**
- *The smoother must resume from the WEIGHT.* `subOnGainSm` ships at 1.0 and the
  row renders nothing while the gate is off, so a gate flipping ON at the weight
  floor would open at FULL LEVEL and fade down to 0.001 — a full-scale burst at
  exactly the pad position the ramp exists to make silent. Set from `subOnW` on
  the ON transition; with morph off that assignment is 1.0 = 1.0 and the plain
  toggle is unchanged.
- *ON re-strikes what is held*, which is ADR-100 Amendment 1's rule for an
  oscillator's enable applied to the gate that now shares its field. Without it
  a corner that turns the sub ON is silent until the next fresh note (the
  "sometimes osc 2 doesn't work" report, one engine over), and a member a pad
  sweep cannot make audible is a member in name only.

**The gate rows, and the must-fail control run against them.** `morphlayout_check`
T12 asserts BOTH halves, each the other's control: the AUDIO must move
continuously while the gate's own VALUE must only ever read 0 or 1 (a row that
read only the parameter could not tell a ramp from a snap; a row that read only
the audio could not tell a deferred flip from an interpolated gate). T12d is
B48's pure-corner claim: two instances taking the IDENTICAL morph code path,
differing only in whether the weight 1.0 came from a pure corner or from all
four corners agreeing, must render bit-identically — and the OFF corner must be
EXACTLY silent, not merely quiet. Green:

```
  ok    T12b the gate RAMPS across the pad: over 21 positions the sub's level rises 0.0000 -> 0.3746 with 17 strictly between and no adjacent step above 25% of the span (worst 16.9%)
  ok    T12c CONTROL/paired: the gate's own VALUE only ever reads 0 or 1 across that same sweep — the flip is deferred, never interpolated (stray -1)
  ok    T12d a PURE CORNER is exact: parked on the ON corner the render is BIT-IDENTICAL to the same patch with all four corners holding the gate ON (4096 samples, first difference none), and the OFF corner is exactly silent (peak 0.000e+00)
```

Measured RED on a planted binary (the ramp removed from `renderSubSpan`'s
accumulate, everything else identical), so the row is known to be able to fail:

```
  FAIL  T12b the gate RAMPS across the pad: over 21 positions the sub's level rises 0.0000 -> 0.3746 with 0 strictly between and no adjacent step above 25% of the span (worst 107.9%)
```

**The layout marker moved 8 → 9, and why.** Not required to READ: `morphSlotMap`
treats every layout ≥ 2 as a 1:1 prefix and `resetCorner` defaults every slot
first, so a stored 272-entry layout-8 array loads into the 273-entry order with
the new slot at its default either way. Taken because the marker's job is to
NAME an order and the repo's own rule at `cornerJson` is one bump per appending
change — the same reasoning, and the same bank re-save, as B195's 7 → 8. Pins
moved with it: `bank_check` (with the reason at the pin), `subosc_check` 11d,
`state_check`'s hand-authored corner array, `morphlayout_check` T11's,
`gen_factory_bank`.

**One check was EDITED and it was not a weakening.** `morphlayout_check`'s T10
anchor asserted `device > 0` over the engine span. The gate was the ONLY Device
id in that whole span, so after this ruling the clause asserts something untrue
of the product. The count is still computed and PRINTED (`device is 0 since
B203`), so a future engine block that does class a row Device is visible the day
it lands, and T10's Device clause says out loud that it is vacuous rather than
reading as coverage it does not have. The scan's teeth are unchanged: T10c
plants a lie in the membership set and requires exactly one violation.

- **Evidence consulted:** `src/subosc_core.h` (header, `noteOn`, `render`,
  `kParamTable`); `src/hypersaw_clap.cpp` (`subMonoNoteOn`/`subMonoNoteOff`/
  `subNoteOn`/`subAllOff`/`subSetParam` ~1740-1960, `retargetAll`,
  `handleNoteOff`/`handleEvent` ~7250-7450, `renderSubSpan`, `morphInit`,
  `paramClassOf`, `morphApplyOscEnable`/`morphStep`/`intentApply`,
  `applyOscGainAndMeter`/`gainSmoothCoef`, `cornerJson`/`morphJson`,
  `subWaveJson`); `tools/subosc_check.cpp` (the shell rig, 11g),
  `tools/station_check.cpp` 3.12/3.15 (the step detector and its unit),
  `tools/morphlayout_check.cpp` T10/T10b/T10c/T11, `tools/paramclass_check.cpp`,
  `tools/bank_check.cpp`, `tools/gen_factory_bank.cpp`, `tools/state_check.cpp`;
  `traces/2026-09-21-subosc-morph-and-init.md` (B195's two-pass shape, and its
  ruling on whether the marker must move).
- **Alternatives rejected:**
  - *(B202) Giving mono's re-strike a FREE SLOT instead of slot 0*, which is
    what the swarm's own mono does for a ringing tail ("a ringing release tail
    alone gets a fresh strike on a new slot, overlapping the tail naturally").
    It would fix the velocity step too. Rejected for this dispatch: it rewrites
    B181's documented "ONE SOUNDING SLOT, AND IT IS SLOT 0" invariant, moves the
    glide/pitch write off `s == 0`, and silently changes what `subWaveJson`'s
    instance-0 `freqHz()` reading means (the wave display's header Hz). That is
    an architectural change to a shipped design, which the charter routes to a
    critic and a human, not to an implementer with a one-line cause in hand.
    **Recommended to the lead as the follow-up that closes the velocity
    residual.**
  - *(B202) Smoothing `vel_` inside the core.* Rejected — it puts a new
    mechanism in the parity-gated render loop to serve a surface (§5.3) that
    ruling R4 is already retiring.
  - *(B203) Adding 4015 to `morphIds` while leaving its class Device.* Rejected:
    it breaks `paramclass_check` T2b ("no morphIds member is device"), which is
    the load-bearing cross-check between two independently authored lists.
  - *(B203) Widening pass 2 in place instead of a third pass.* Rejected — it
    interleaves 4015 in block order and shifts every stored slot after it. This
    is the exact defect B195's two passes exist to prevent.
  - *(B203) Leaving the layout marker at 8.* Defensible (it is not needed to
    read) and would have spared 45 files of bank churn, but it makes one marker
    name two orders, which is precisely what B175's cross-layout remap will have
    to ask. Rejected on the same grounds B195 rejected it.
- **Verify:** `./verify full`, exit 0, git `f798f48` per
  `.harness/last-verify.json` — the B203 commit itself, re-run after committing
  rather than before. (This line is the one thing a trace cannot state in the
  commit it describes; the follow-up commit that writes it is re-verified the
  same way, which is the B195 trace's precedent.) Named gates green in that run: `subosc_check: GREEN (0 failures; worst parity rms 0.000e+00)`,
  `morphlayout_check: PASS`, `bank_check: 0 failure(s)`,
  `state_check: GREEN (0 failures)`, `undo_check: GREEN (0 failures)`,
  `fxxfade_check: GREEN (0 failures)`, `paramclass_check: PASSED (0 failures)`.
- **Open questions:**
  1. **B202/B203 are not ROADMAP rows.** The brief names them; `ROADMAP.md` does
     not contain either id. Acceptance was taken from the brief's own text. The
     lead owns closing that.
  2. **The velocity residual (11.91x) is reported, not gated.** It is real and a
     player with dynamics will hear it. The free-slot change above is the fix
     that closes it; it needs a ruling on B181's slot-0 invariant first.
  3. **The mono TOGGLE discontinuity (20.87x) is still unruled** and is now
     load-bearing: it is 11g.e's must-step control. If it is ever smoothed, that
     control needs a different positive case.
  4. **`subWaveJson` reads instance 0's `freqHz()`** for the wave display's
     header. Correct today because mono always sounds slot 0 — and it is one of
     the things the free-slot change would quietly break. Noted here so the next
     session does not have to rediscover it.
  5. **No TESTING.md row was added.** `test_table_check` is green (no new
     (page, feature) pair, no new check file), but the coverage table does not
     record B202's or B203's rows. Out of this brief's file scope.
