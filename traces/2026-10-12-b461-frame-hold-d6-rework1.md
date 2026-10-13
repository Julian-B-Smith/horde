# b461-frame-hold-d6-rework1 — D6 `frameHold` after the critic: the tracking fraction in seconds, joining members, two members, and an honest check

- **Queue item:** ROADMAP B461, ADR-216. Rework round 1, dispatched by the lead after an independent critic read the first commit. It continues and corrects `traces/2026-10-12-b461-frame-hold-d6.md`.
- **Why:** the critic's verdict was "rework, narrowly": the law is sound and flag OFF is today's path; the check, the parity set and the ledger had to change. Two questions are with the human and are NOT answered here: (Q1) the wording of acceptance (2) at C3, and (Q2) what the frame does after a crossing. So the judged JUMP rows' rule, the way the frame turns, and the absence of a tie rule are all as they were.

## The lead's rulings under ADR-216's delegation (an amendment to ADR-216 will carry them)

1. **The frame's advance is the MEASURED mean phase advance** (the first trace's point 1: accepted). Consequence, stated: with members waiting on onset scatter the mean includes their zero advance, so the frame runs slower than the sounding members until all have entered.
2. **Spread law 3 reads the lead under the frame, through `spread()`, as a step per tick** (point 4: accepted). It is an exception to "never as a step", and B460 owns it.
3. **The frame is assigned psi where the correction is whole** (point 2: accepted).
4. **g(R) is a fraction per REFERENCE tick** (M1, below): 32 samples at 48 kHz, 1/1500 s. A tick of length dt takes `1 - (1 - g)^(dt / dtRef)`; g 0 gives 0 and g 1 gives 1, exactly.
5. **A member that joins a held frame has its lead SET at the next tick, not ramped** (M2, below).

## What changed, item by item

- **M1, the tracking fraction in seconds.** g(R) was applied per tick, so the frame's tracking time constant halved at 96 kHz. Now as ruling 4, the same arithmetic in both engines; the ends (g 0, g 1) take no `pow`. At 48 kHz the exponent is exactly 1 and `1 - (1 - g)` is g to a rounding: 7 of the 19 flag-on rows at 48 kHz moved (those that enter 0 < g < 1), by rounding (the pad before and after, a scratch comparison: at most 1.8e-14); the 44.1 kHz row moved by the law; the 543 earlier rows did not move. libm call sites: `pow` 9 → 10, `nan` +1, in both builds.
- **M2, members that join a held frame.** The C++ ramped `lead` itself; the JS ramped `leadR`, which it never writes with the flag off. The row with the critic's history (flag off at N 8, on at N 4, N back to 8) was added first and run before any fix: RED, rms 7.2e-3, max-abs 0.53, events differing. The fix: each voice keeps the N its hold last ticked with (`fhN`); a member at or above it keeps the blade law's lead until the next tick, takes no share of a ramp, is read as it stands by spread law 3, and at that tick has its lead set. Two rows, both at parity (rms 3.7e-15 and 1.6e-14). That the second row (the join met by a mono retrigger before any tick) would be red without the non-tick half of the fix is ARGUED from the code, not measured.
- **H1, two members.** Two rows, the pad's pinned settings at N 2 with a static spread law, detune 14 and 30. Both hold parity as written (rms 2.8e-15 and 4.3e-15). They hold where the two libms happen to agree: with the note frequencies moved 1 to 15 ULP on both sides (`--nudge`), 2 of the 30 renders are RED (detune 30 at 2 ULP: rms 2.7e-2, max-abs 0.61; detune 14 at 13 ULP: rms 2.8e-2, max-abs 0.64; events differ). Probed on the C++: all 22 hold exits at N 2 were within 8.5e-15 of the half-cycle tie, either sign. The rows are committed because they are green by the suite's own criterion; their comment says what a red on them means. **They may read red on another platform, compiler or Node** (not run anywhere but this Mac).
- **H2, the LOCKED rows.** Both cases set N 6; a silent render is not counted; a COUPLED case (detune 14, K 1) is judged from 2 s on, with the earlier window printed. Ceilings re-measured, each about 40 times its measured worst: unison 3e-12 (worst 7.3e-14), coupled 6e-10 (worst 1.5e-11). Before 2 s the coupled rows read 4.2e-4 to 0.14: the ramp against the step while the lock forms.
- **M4, the check's honesty.** A LIVENESS row (30 of 30 within 3 dB, the largest 0.56 dB); the two excusable corner patches named, any other skip red; os 1 rows (the control judged, the flag-on rows NOTE lines); SPREAD notes at both os; a comment at `SEED`.
- **L1.** `stepMember` computes the per-sample phase step once. Output-neutral, proven: both self-digest builds 567 of 567 with no re-pin. `nan` call sites 584 → 567 and 566 → 549.
- **L6 and text.** `h2/README.md`'s "Last verified" line states no parity figure from a run that is not green; the D4 wording is restored; two comments cite ADR-217 and the lead's amendment to the brief.

## Corrections to the first trace

- **Open question 3 and the ledger's finding (3)** said the half-cycle tie "did not occur" on the cases measured, with margins of 0.01 to 0.05 cycles. That is true at N 6 only (the critic: 12 exits, |error| 0.300 to 0.490, least margin 0.00994). At N 2 every exit sits on the tie (H1 above). The ledger is corrected.
- **The locked-case figures** there (0 to 6.7e-14 on 8 patches) were of a unison in which two patches had one member and never engaged D6. The figures above replace them.
- **"The lead, 2026-10-12" cited as a ruling** in two comments was the lead's amendment to the brief; the bank edit itself is ADR-217's.
- A further fact the first trace did not have: `corner/continuous-max` is not locked at all under the coupled settings (its `spread` 24 multiplies the detune; a scratch probe read R down to 0.005 and 53 hold exits in 4 s). Its coupled-row difference (about 1.4) is D6 at work on an unlocked swarm. It is one of the two patches excused by name.

## The check's readings (2026-10-12, darwin-arm64; isolated jumps at C3 / G3 / C4)

| render | flag off | flag on |
|---|---|---|
| os 2, the pad as pinned (judged) | 5 / 1 / 2 | **1** / 0 / 0 |
| os 2, Position spread 0 | 3 / 1 / 2 | 0 / 0 / 0 |
| os 1, the pad as pinned (control judged) | 6 / 5 / 3 | 3 / 0 / 0 |
| os 1, Position spread 0 | 5 / 2 / 4 | 0 / 0 / 0 |

With the spread law's route to the blade's position closed the flag-on pad reads no isolated jump at either os; as pinned it reads 1 at os 2 and 3 at os 1, all at C3.

## Rows and pins

- Rows added in the rework (567 scenarios; floors 563 → 565 → 567 in `tools/h2_engine_parity_check.cpp` and `tools/h2_engine_rtsan_check.py`): `C/D6 members join a held frame`, `C/D6 members join before a mono retrigger`, `C/D6 two members, detune 14`, `C/D6 two members, detune 30`.
- Rows of this PR whose digests moved in the rework (none existed before this PR): under M1, `C/D6 :: Smear and strike`, `C/D6 :: Cross-mod shimmer`, `C/D6 the centre undefined, K 0`, `C/D6 a splayed swarm, K -.6`, `C/D6 spread law 3`, `C/D6 blade 2 on the frame, blade 1 off it`, `C/D6 at os 1` (rounding) and `C/D6 at 44.1 kHz` (the law); under M2, `C/D6 N changes mid-note`. The 543 rows that predate this PR are byte-identical throughout.
- The composed JS's pin: `28b5f79c…` → `0902e11f8dd8df6cc59eb45e80632ce7418c561f` (M1) → `ad8e679141c905b15563c17461bd9dd7fa35518b` (M2). The lab's pin is unchanged at `65d587e9…`.

- **Evidence consulted:** the lead's rework brief (the critic's findings as relayed); `h2/engine/engine.h`, `docs/design/scalpel-horde-engine.js`; the first trace; ADR-216 on main.
- **Alternatives rejected:** a special case at 48 kHz that keeps g exactly (the ruling expects rounding there, and a rate-keyed branch is one more thing to keep true); a tie rule for two members (Q2 is the human's); widening the coupled settings to force the loud corner patch to lock (the settings are the lead's); carrying the two-member rows as known-red (they are green as written, and the suite has no sanctioned way to carry a red row).
- **Verify:** `./verify fast` exit 0 on each commit's staged tree. `./verify full` on the final tree: red on `h2_frame_hold_check`'s `JUMP C3` row only, as expected in this round; verbatim output, `.harness/last-verify.json` and a labelled diagnostic run of the later gates are in the hand-back. Not pushed.
- **Open questions:**
  1. Q1 and Q2 above, with the human.
  2. The two N 2 rows are a known fragility committed on purpose. If the human rules a tie rule or a different turn, they become ordinary rows; until then a red on them is information, and the lead may prefer them out of the committed set before any push.
  3. Not done in this round, by the brief: an invariant check on `Engine::voice()` for "the frame never jumps"; the ledger check's control order and a wrong-default control; the armor catalogue entry; any listening artifact.
