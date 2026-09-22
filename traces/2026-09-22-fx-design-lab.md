# fx-design-lab — every FX module as a four-knob face over every control, in gui2's tokens

- **Queue item:** B210 (row carried in PR #720; acceptance read from that branch's ROADMAP.md)
- **Why:** The human, 2026-09-22: "New FX design lab (We now have the routing
  visualized, but I want to see a design prototype for the full reverb, Maw,
  Sluice, and expanded visuals/controls/functionality for comb, echo, drive,
  filter)". ADR-169 A2's rule — "distill its essential controls to four knobs,
  even while the rest of the parameters are accessible" — is applied to all
  seven modules: a FACE of four role slots over an EXPANDED view with every
  control, the face's bindings marked on the rows they drive, and the choice of
  four stated per card. ADR-169 A1 (Sluice declines roles, asks for order +
  label; unruled) is drawn both ways side by side, not settled.
- **Evidence consulted:** `docs/design/station-page-lab.html` (the standard's
  model; tokens and chrome copied from its :50-247 citation block),
  `src/gui/gui2.html` :26-212 (tokens) and :617-731 (§4 knob skin, copied),
  `docs/design/reverb-lab.html` (control list :718, tap table :206-209, FDN
  lengths :237, loop-gain law :350-376, `stepMod`), `reference/maw/core.js` and
  `presets.js` (loaded read-only by `<script src>`, never copied or edited),
  `reference/network-lab-v0.html` :181-209 (Sluice module specs), Sluice's own
  lab PRESETLIB (read-only; bode / scrumulator-ish macros, their D-046/D-048),
  `integrations/sluice/notice-preset-macros.md` (addendum 2, D-052),
  `src/fx_rack.h` (:40-100 types and contract, :210-250 comb notes, :650-835
  per-type DSP), `src/time_core.h`, `src/force_core.h` :160-222 (tap placement),
  `src/svf_core.h`, `src/delay_core.h`, `src/param_presentation.tsv` (FX
  addresses), DECISIONS ADR-166 A1-A6, ADR-169 + A1 (both of them) + A2,
  ADR-170 + A1-A3, ADR-172, ADR-177 §4; LIBRARY L0026, L0028.
- **What the page claims, and how it checks itself:** a load-time audit prints
  per module the control count and bound face slots, and ERRORS if a face
  binding names a row the panel lacks; the reverb panel is held to the reverb
  lab's own 25-control list; MAW's rows are checked both ways against MAW's
  `defaultParams()` (72/72, lab-only `src/spread/latch` excluded by name) and
  its 14 curve names against `SHAPERS`. **Must-fire control:** a scratch copy
  with `tailDecorr` dropped, MAW `inGain` dropped and comb's Motion binding
  renamed to `spreadd` printed `reverb-lab controls 24/25`, `MAW params 71/72`
  and all three ERRORS — the audit can go red. **Interaction probe** (scratch
  copy + appended script, headless Chrome): MAW Feedback IDLE 1 → 0 after the
  `pitch-tracked feedback` preset; Sluice role-keyed 0/4 as shipped → 2/4 with
  the horde role table, order-keyed 4/4; order M3 → 0.9 moved the role-keyed
  REGEN knob to 0.9 and `fb.gain` to 0.84 (= 0.3 + 0.6·0.9); reverb Decay knob
  at 1 → decay 9.00 s and size 0.35 (one knob, two internals).
- **Alternatives rejected:** (1) audio in the lab — each module's audition
  already lives elsewhere (reverb-lab, MAW's prototype, Sluice's lab) and the
  question here is what a module SHOWS; every picture is deterministic
  arithmetic from the parameters instead. (2) Copying MAW's curves into the
  lab — `<script src>` of the protected core draws them, so the pictures cannot
  drift from the device; the page says so when the file is unreachable.
  (3) Inferring Sluice roles from labels ("regen", "amount") — a third
  vocabulary; the role-keyed face uses Sluice's own ADR-166 A4 mapping by
  binding target, behind an explicit toggle, off by default.
  (4) Four live knobs on Drive — its Regen slot is drawn unbound on purpose
  (ADR-169: inert, never hidden); inventing a fake loop role would hide the
  finding.
- **Files:** `docs/design/fx-design-lab.html` (new; `<meta name="lab-review">`
  per the lead's addendum), `docs/design/fx-page-lab.html` (superseded header,
  visible pointer, `<meta name="lab-superseded-by">`). `docs/design/index.html`
  deliberately untouched (the lead regenerates it after all five labs land).
- **Verify:** `./verify fast` exit 0, `.harness/last-verify.json`
  `{"target":"fast","exit":0,"git":"3fd3a6b"}` (the working tree carried the two
  files above, uncommitted). The private-name leak gate SKIPPED in this worktree
  (`.leakcheck-names` is untracked and absent here); the same pattern was run by
  hand from the main checkout's list over both files, case-insensitively: 0 hits.
- **Screenshots:** Chrome headless, dpr 1, `file://`, 1600 px:
  faces light/dark, all-expanded light/dark — session scratchpad, named in the PR.
- **Open questions:** (a) whether the proposed rack controls (comb damping,
  pitch, spread, lines, release; echo's swarm motion; drive curve/bias/tone/
  bloom/output; filter SVF + motion) are wanted at all — each is marked NEW and
  is a question, not a claim; (b) Filter's face inverts today's amount (up =
  open), implying a patch migration a → 1 − a; (c) whether Drive survives beside
  MAW at 1.0 or becomes a one-stage MAW preset (ADR-172 one instance per type);
  (d) whether a face knob bound to a parameter the current topology ignores
  should be marked IDLE by rule (SPEC-MODULE-MACROS gains a line) — the lab does
  it for MAW's Feedback; (e) the ADR-169 A1 ruling itself, which this lab makes
  concrete and does not vote on; (f) the page is verified headless only — knob
  feel (drag, wheel, double-click) wants a human hand.
