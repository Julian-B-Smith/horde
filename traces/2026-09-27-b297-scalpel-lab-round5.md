# b297-scalpel-lab-round5 — members at their phase, the R-hued beam returns, Cut spread under a rule

- **Queue item:** B297. The row is carried in records PR #783 (branch `lead-records-107`), and I read it verbatim from `origin/lead-records-107:ROADMAP.md`. The human, 2026-09-27:
  1. "Could we add a view mode that shows the members at their relative phase position, or would that get too messy?"
  2. "There are a couple presets where I get the distinct impression that what I'm seeing is not what I'm hearing. Maybe we can bring back the waveform view as well (the one that changes color based on the value of R)"
  3. "When the cut rule is switched to something else (like harmonic), the cut rate spread knob stops working"
- **Why:** This is round 5 of B271's lab, `docs/design/scalpel-interface-lab.html`, and it answers exactly those three asks. Isolation holds: one file plus this trace. `docs/design/index.html` is not regenerated (B296 is in flight). Nothing in `src/`, `specs/` or `reference/` is touched, and the oracle's law is unchanged.
- **What changed** (one file; `lab-review` meta `B271 + B293–B297 · 2026-09-27`):
  1. **AT PHASE, a third cycle-view mode** (`phaseModel`, section D; `phaseHi`/`phaseTicks`, section F).
     - **What SUM draws.** Each member's heard cycle over its OWN phase, summed. That is the swarm as if it were locked.
     - **The offset.** AT PHASE measures each member against the mean field ψ = arg Σe^{2πiφ} (the ring's needle): δ_i = φ_i − ψ, wrapped to [−½, ½). The phases are the viz post's `mem.phi`, the same instant as the windows (`cycleFrom` now returns them as `phi`).
     - **The drawing.** Member i is drawn as y_i(x + δ_i), and its blade windows move with it. The heard trace is the shifted sum. At lock every δ is 0, so AT PHASE draws exactly SUM.
     - **Legibility at N = 9.**
       - Members are 1 px violet at 0.34 alpha. A comb of ticks on the top edge marks where each member's own cycle starts.
       - Windows are fills only, with faint edges while no member is lifted.
       - Hover lifts the nearest line within 14 px: it goes to 2 px, full ink, over the sum. Its windows get edges and a stronger fill, and its offset is named ("member 5 · −0.03 cycle from ψ"). A drag keeps the grabbed member lifted.
     - **Hit-testing.** The XY hit-test reads the shifted windows (`xyWindows`), so a grab targets the window as drawn.
     - **The row.** The cycle row's "CYCLE" label gave its 38 px to the third pill, and the caption was shortened, so the row stays one line.
  2. **The beam returns** (`paintBeam`, `beamModel`, section F), a port of gui2's `paintScopeInto` (`src/gui/gui2.html:5648-5745`).
     - **The R→hue mapping source:** gui2.html:5733-5735, `mixHex(TOK('--scr-value'), TOK('--scr-marker'), clamp(kSmR, 0, 1))`. That is ADR-119 A2, a per-channel RGB mix from value pink (drifting) to marker amber (locked). `mixHex` is copied verbatim from gui2.html:1920. The mapping's input R̃ is smoothed at 0.35 per stroboscope frame (gui2.html:4957 smooths at 0.35 per viz frame), so it is stepped by rendered frames, never the clock.
     - **The feed:** the monitor's RENDERED samples only. `monStep` now keeps the right channel too (`MON.bufR`). The left channel is drawn as the beam (a 22 % underlay 8 px wide, then a 2.2 px core), and the right as a dotted meter etch, as in gui2.
     - **One change from gui2, the alignment.** For members of one shape, the sum's fundamental has phase ψ + θ (θ fixed by the wave's shape). So gui2's low-pass trigger is ψ-alignment shifted by θ. The beam starts where the live ψ last crossed 0 with a whole period to draw, allowing for the oracle's one-sample `prev`. So x = 0 on the beam is x = 0 on the cycle view.
       - Verified at N = 1 on the drawn = heard voicing: the error is 0.068 with no shift, and best-shift alignment finds the same shift (0).
       - gui2's trigger stays as the fallback when there is no live voice.
     - **Placement:** directly under the cycle view on OSC, 56 px, the same width, and the quarter lines are repeated.
       - The left column is untouched: rounds 3–4 kept the ring at 250 px, the spectrum at 92 px, and the carpet and voice map.
       - The Controls column now scrolls 26 px (content 621 against 595), so the ADVANCED bar sits under the fold.
       - MAIN has no beam. It has no room without shrinking round 3's 150 px cycle view.
  3. **What you see vs what you hear, measured** (section K2, `misread*`). The measurement sits in a full-width notes box, with a MEASURE button and `?misread=1`. It takes about 16.5 s for all 76 presets.
     - **The setup.** Each preset is played as the monitor plays it (A2, velocity 0.9, seed 0xB271 + 45). The render runs in blocks that end exactly on a viz post (vc counts from 0, one post per 1600 samples).
     - **The comparison.** At 8 posts from 0.2 to 1.8 s, the drawn trace (SUM's and AT PHASE's) is compared with the rendered mid's last period.
     - **The metric.** A normalised RMS error at the best circular alignment: 0 = the same shape, 1.41 = unrelated. `self` is the heard period against the period before it, with no alignment.
     - **The causes, by ablation.** Each suspect is removed and the error re-measured:
       - as if locked (AT PHASE instead of SUM);
       - fb → 0;
       - xm → 0;
       - output stage (Stereo 0, gain 0.02, raw, 1×).

       Noise and S&H are flagged from the patch, and non-periodic from `self`. Two attributions are recorded: SUM's largest cause, then what AT PHASE still misses.
  4. **Cut spread under a rule** (`inertWhy`/`inertMark`, section C; `bladeRow`).
     - **The oracle's law:** `RazorCore.kSpread` (`razor-core.js:144-161`) reads `spread` only when the rule is 0 ("even"). Any other rule sets `ruleList(rule)[slot]^amt`, and `spread` is never read.
     - **How the lab draws it.** Under a rule, each blade's Cut spread (`kRule`/`kspread`; `kRule2`/`kspread2`, or blade 1's rule while `b2sp` follows) is marked `.inert.ruled`:
       - dimmed, with its label struck through and a dotted cap;
       - a tooltip leading with the reason and "Turn Rule depth, beside it";
       - on the face and in Advanced (wired for all three skins; the check builds skin A).
     - **Rule depth on the face.** Rule depth (`kRuleAmt`/`kRuleAmt2`) joins the SPREAD group on the face while a rule is on, and the caption names the rule ("SPREAD · HARMONIC").
     - **The mirror.** Rule depth is drawn inert under "even".
     - **Values are kept.** An inert value can still be edited, and it returns when the rule does.
  5. **The page text.** Round 5 open questions 31–36, the header comment, the badges and the side panel's reading guide. The side panel also had a stale round-3 sentence ("a grab alone moves nothing" for the ring, false since round 4), now corrected.
- **The misleading-presets table** (measured on the committed lab, 643b05e; the top by SUM's error):

  | Preset | N | R | SUM err | AT PHASE err | self | SUM's largest cause | What AT PHASE still misses |
  |---|---|---|---|---|---|---|---|
  | Clockwork | 7 | 0.02 | 1.23 | 0.23 | 0.29 | as if locked | output stage |
  | Splayed blades | 4 | 0.02 | 1.18 | 0.27 | 0.05 | as if locked | output stage |
  | Metal pair | 4 | 0.69 | 1.02 | 0.12 | 0.03 | as if locked | — |
  | Smear and strike | 7 | 0.39 | 1.01 | 0.20 | 0.46 | as if locked | non-periodic |
  | Cross-mod roar | 5 | 0.42 | 0.96 | 0.49 | 0.53 | as if locked | cross-mod estimate |
  | Swarm-linked spread | 6 | 0.34 | 0.93 | 0.20 | 0.36 | as if locked | non-periodic |
  | Formant over sync | 4 | 0.97 | 0.92 | 0.11 | 0.03 | as if locked | — |
  | Split envelopes | 4 | 0.97 | 0.90 | 0.14 | 0.15 | as if locked | — |
  | Cross-mod ring (watch) | 5 | 0.36 | 0.88 | 0.36 | 0.27 | as if locked | cross-mod estimate |
  | Jitter swarm | 5 | 0.19 | 0.84 | 0.69 | 0.26 | as if locked | noise / S&H |
  | Feedback screech | 1 | 1.00 | 0.78 | 0.78 | 0.27 | feedback not drawn | feedback not drawn |
  | Feedback snarl | 1 | 1.00 | 0.76 | 0.76 | 0.37 | feedback not drawn | feedback not drawn |
  | FM bell | 1 | 1.00 | 0.59 | 0.59 | 0.89 | non-periodic | non-periodic |

  - **Totals.** SUM's error is ≥ 0.5 on 41 of 76 presets, and AT PHASE's on 6. AT PHASE is the nearer on 62 of the 63 presets with more than one member. The median falls from 0.55 to 0.11.
  - **SUM's largest cause.** It is "as if locked" on 62 presets, even near lock: at R ≈ 0.97, offsets of ±0.03 cycle smear sync edges and high formants. The rest: output stage 5, noise 3, feedback 2, residual 2, non-periodic 1, cross-mod 1.
  - **Where AT PHASE is worst:**
    - Feedback screech 0.78 and Feedback snarl 0.76 (feedback is not drawn);
    - Jitter swarm 0.69 (noise);
    - FM bell 0.59 (non-periodic: self 0.89);
    - Wobble jaw 0.55 (noise, self 0.84);
    - Crunch (audio-rate PM) 0.51 (noise);
    - Cross-mod roar 0.49 (cross-mod estimate).
- **Evidence consulted:**
  - ROADMAP B294, B295, B297 and B298 (`origin/lead-records-107`).
  - The B294 and B295 traces.
  - The lab, sections A–L.
  - `reference/scalpel/prototype/razor-core.js:85-161` (ruleList, spreadMember, kSpread), `:262` (vc = 0), `:581-640` (stepM, `prev`), `:641-814` (render, the viz post).
  - `src/gui/gui2.html:1920` (mixHex), `:4950-4960` (kSmR), `:5630-5745` (paintScopeInto).
  - `DECISIONS.md` ADR-119 A2 ("The waveform's hue rides R").
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 38/38, 12 of them controls that must fail** (30/30 with 9 controls before). Read from headless Chrome over DevTools on the lab, in light, dark and `?page=main`. New:
    - **AT PHASE offsets = the oracle's.** At splay (R 0.016), max |δ_i − δ_0 − (mem.phi_i − mem.phi_0)| = 0, and the offsets are taken from ψ (residual angle 0).
    - **CONTROL:** SUM's all-zero offsets are off by 0.419 cycle.
    - **Against the audio.** AT PHASE is nearer the rendered period than SUM at splay: 0.131 against 1.093.
    - **The beam reads the rendered buffer, on the cycle's axis.** The live source is bit-identical to the monitor's last 436 rendered samples, L and R. At N = 1 the error against the cycle's trace is 0.068 with no shift.
    - **CONTROL:** a quarter period late reads 1.412.
    - **The hue rides R.** Light: R 0.999 → #c43a00 (18°), R 0.016 → #d60169 (331°). Dark: #ffb86b (31°) → #ff5fcd (319°).
    - **Cut spread under "harmonic".** The built face marks ① and ② inert and ruled, with Rule depth beside each, and the Advanced ② row is inert. Turning 0 → 12 through `write`, which the oracle received, leaves the output bit-identical and every member's cut rate (mem.k, mem.k2) identical, on both blades.
    - **CONTROL:** under "even", the same turn changes the output and the member cut rates, and the face draws the knob live with no Rule depth.
  - **VERIFIED, real mouse** (scratch CDP harness, `Input.dispatchMouseEvent`):
    - A hover lifted the targeted member (hi 2).
    - In AT PHASE, a drag grabbed blade 2 at its shifted window and moved Position ② 0.35 → 0.45 for a 0.1-width drag, and the oracle's `t.c2` agrees.
    - The MEASURE button filled the table, and a row click loaded the preset.
  - **`lab_load_check`:** GREEN (53 labs, 0 broken, 1 skipped).
  - **Tier audit:** 93 at their tier, 8 promoted, 0 demoted, 0 missing, at the default patch. While a rule is on, Rule depth counts as promoted to the face (open question 36).
  - **Not verified:**
    - Nobody has listened, or used AT PHASE or the beam by hand.
    - The causes are ablation attributions: the patch changes when a suspect is removed, so they are the best single explanation, not a proof.
    - The measurement compares the mid (L + R)/2, while the beam draws L. With a wide Stereo the L beam differs from the mid by the pan weights ("output stage").
- **Alternatives rejected:**
  - **The beam as a cycle-view mode.** The human's complaint needs both views visible at once.
  - **The beam in the left column.** No room, and it would be far from the cycle it is compared with.
  - **Shrinking the cycle view.** It would undo round 2's 132 px (and round 3's 150 on MAIN).
  - **gui2's low-pass trigger as the primary alignment.** It is ψ plus a shape-dependent constant, so the beam would not share the cycle view's x axis.
  - **Referencing AT PHASE to a member (the root ★).** It is steady, but not physical, and it favours one member.
  - **Edges on every member's window in AT PHASE.** At N = 9 they drew a picket fence (first screenshot pass).
  - **Hiding or disabling the inert knob.** Its value is still stored and returns under "even"; a dimmed, struck, still-editable knob says both.
  - **Changing the default view to AT PHASE.** The measurement argues for it, but a default is the human's call (question 31).
- **Verify:** `./verify fast`, exit 0, git 643b05e (`.harness/last-verify.json`, 2026-09-27T16:35:34Z). The log notes "private-name leak check SKIPPED" (`.leakcheck-names` is absent in this worktree). This trace is committed on top, and verify is re-run on that hash before the push.
- **Screenshots** (scratch, not committed; `scratchpad/b297/shots/`):
  - 00: before (OSC light, MAIN light, Harmonic stack).
  - 05–07: after (OSC, OSC in AT PHASE, MAIN; light and dark).
  - 10: AT PHASE at N 3 / 5 / 9, locked and splayed, light and dark.
  - 11: N 9 splayed with a member lifted.
  - 12: SUM at N 9 splayed, for comparison.
  - 13: the beam at lock and at splay, light and dark (2× crops).
  - 14: the inert-spread state on Harmonic stack, light and dark.
  - 15: the worst three presets (Clockwork, Splayed blades, Metal pair), SUM and AT PHASE, each with the beam under it, light and dark.
  - 16: the real-mouse hover.
  - 20: the measurement table.
- **Open questions:**
  1. Should AT PHASE be the default view? The lab recommends yes (62/63; median 0.55 → 0.11). If so, MAIN's cycle view, which has no view pills and stays SUM, should follow.
  2. Does MAIN want a beam? There is no room without shrinking its 150 px cycle view.
  3. Should spread MEAN something under a rule? One concrete proposal is to stack them. kSpread already returns [offset, multiplier], and render applies (k + offset)·multiplier. Computing the offset under a rule too gives (k + spread·slot)·ratio^depth: the rule's chord, with each member detuned by the spread law. That adds no parameter and leaves both knobs live. It is a protected-oracle change and needs an ADR.
  4. The beam replaces gui2's trigger with ψ-alignment (it is gui2's trigger minus a shape constant). Should the plugin's gui2 adopt it where a cycle view sits above the beam?
  5. At deep splay ψ is weakly defined, so AT PHASE and the beam can slide sideways as a whole. The lab accepts this; the offsets between members stay exact.
  6. The Controls column now scrolls 26 px. Is that acceptable, or should the beam be a fold-away?
  7. The in-page self-check is still not gated by `./verify`, as in B271/B293–B295.
