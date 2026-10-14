# adr221-output-headroom — the engine's default output 12 dB lower, so chords do not clip (round 3: re-pinned under A2)

- **Queue item:** ROADMAP B489. Rulings: ADR-221 (the human's), and A1 and A2 (the lead's readings). Records are on `origin/lead-records-258`. Rounds 1 and 2 are `traces/2026-10-14-adr221-output-headroom-stopped.md` and `-round2-stopped.md`, both in this branch.
- **Why:** At the default level 0.35, the one output stage `tanh(y * gain * 1.6)` clipped four-note chords on 71 of 83 bank presets (B486). ADR-221 lowers the default to 0.35 / 4. A1 makes the rows that compare the two engines, and every pre-ADR-221 pin, render at gain 0.35 written, and adds a row that holds the default. Round 2 left one piece undone: `--repin` refused, because the self-digest's one-ULP control was judged against the reference being replaced, and ADR-221 moves the control's own row.
- **What round 3 changed:**
  1. **One line of `tools/h2_engine_selfdigest_check.cpp`.** The control call is now `ulpControl(keyed && !repin ? ref : self, newly, first)`, with an ADR-221 A2 comment. Check mode is unchanged. The scenario, the "exactly one row, by name" rule and the refusal are untouched.
  2. **The two must-fail controls**, built as scratch copies of the tool and not committed, each run with `--repin` on the same stream:
     - Nudge disabled: `FAIL  control ULP  sample 14080 of 'T/mode 0 :: chord' nudged one ULP: 0 row(s) newly red`, then `REFUSING to re-pin: a control did not fire`.
     - Nudge on two scenarios (the second row's digest flipped): `FAIL  control ULP ... 2 row(s) newly red, first: P/Starting points / Quarter sync :: chord`, then `REFUSING to re-pin`.
     - In both, KEYSELF also reads `DOES NOT FIRE`. Neither wrote a reference.
  3. **The re-pin, by the tool, in both builds.** Each reported `PASS  control ULP ... 1 row(s) newly red, first: T/mode 0 :: chord` and `REPIN ...: 332 changed, 0 added, 0 removed, 327 unchanged`. TOTAL is `eb850588903b6ee7` in both. The 332 rows: 207 composed-only (C) rows and all 125 targeted (T) rows. Every P (preset) row is unchanged, because `presetCmds` keeps render-goldens' 0.35. Reason (c): a joint JS+C++ engine change under ADR-187 A2 item 3.
  4. **Check mode against the new reference:** `PASS  control ULP ... 1 row(s) newly red` and `GREEN — parity build, 659 scenarios ...; 0 differ`. `h2/README.md` now describes the re-pin basis.
- **Evidence consulted:** ADR-221, A1 and A2; `tools/h2_engine_selfdigest_check.cpp:515-540` (the ULP control and KEYSELF's `ulpControl(self, ...)`); the round-2 logs.
- **Carried from rounds 1 and 2 (measured there; the engine is unchanged since):**
  - **Bank scan** (C4 E4 G4 B4, velocity 0.85, os 1, frameHold 1, stealFade 1, poly 8). Before: 71 of 83 presets over 1 and 21 over 2, median 1.655, worst 3.258 (Two-formant vowel). After: 0 over 1, median 0.414, worst 0.815. Every preset falls by exactly 0.25.
  - **Migration:** the composed engine has no saved-state path on main. Its only door is `set("gain", v)`, and with gain 0.35 written the new engine renders main's samples bit for bit (84 of 84 digests).
  - **A/B** (`tools/h2_ab.mjs` from `origin/h2-ab-tool`, extracted). A is main's renderer, presets at 0.35. B is this branch's renderer with `--b-sets gain=0.0875`. Over 28 pairs:
    - Gain to match: 12.04 dB on the quiet lines, 10.75 to 11.2 dB on chords and steals.
    - Level-matched residual: -16.7 dB to -72.4 dB.
- **Left as written (measured records or separate renderers):** `h2/harness/README.md:111`, `docs/design/scalpel-envelope-conflicts.md:84`, `docs/design/envelope-hierarchy-lab.html`, `docs/scalpel/ACCOUNTING.md:506` (SCALPEL's own default), `tools/measure_h2_engine.cpp:53`.
- **Alternatives rejected:** see A2. Hand-written references are unprovable. A different control scenario would meet the same wall at the next change. A one-off exception does not generalise.
- **Verify:** `./verify fast` and `./verify full` on the staged tree. The exit codes and the `.harness/last-verify.json` record are in the PR body and the hand-back. The branch is pushed only on `full` exit 0.
- **Open questions:** (1) Two readings are the lead's and are PROPOSED for the human at merge: A1's equal-level rows and A2's re-pin basis. (2) The new check's SCALE tolerance, 1e-12, covers the bus's atanh reading (measured 3.3e-16); it was stated by me and has not been ruled. (3) `./verify` gained one added block, the wiring for `tools/h2_output_headroom_check.mjs`, under ADR-180 §1. Nothing in it was removed or relaxed.
