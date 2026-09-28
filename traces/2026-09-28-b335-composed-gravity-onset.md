# b335-composed-gravity-onset — gravity and the ensemble timing correction in the composed engine

- **Queue item:** B335 (records PR #831, branch `lead-records-133`), dispatched by the horde lead 2026-09-28. The human: "Have we ported gravity over from the original engine? Or the ensemble voice lag correction behavior?" Before this change, neither was in the composed engine.
- **Why:** Both SURVIVE in the accounting: row 27/28 are `grav` and `basin`, and rows 70-74 are onset scatter, timing correction, attack scatter, per-partial env and release scatter. The composed engine (B298) was built on SwarmSynth, which has neither. Under ADR-187 they join SCALPEL's parity target.

## What landed

- **`docs/design/scalpel-horde-engine.js`.**
  - **Gravity.** DynSynth's `gravityStep` (`reference/swarmdynamics.html:291-321`) is copied, cited, over each held voice's swarm pitch f0cur.
    - It is stepped on the fixed-time grid, `round(sr·256/44100)` samples (279 at 48 kHz), between render segments. This follows ADR-086 and its Amendment 1, and uses DynSynth's own loop.
    - f0cur rides the played pitch multiplicatively, as `swarm_core.h` keeps it.
    - A note-on resets f0cur to ET (ADR-008).
    - With grav < 0.005, nothing is segmented and the swarm reads the played pitch unchanged.
    - I copied rather than called because calling DynSynth would change the factory's two-argument signature, and every caller (lab, patch space, listening page) passes two arguments.
  - **The ensemble timing** is transcribed from `src/swarm_core.h`. The code says the C++ is the reference:
    - `ensembleSeed`, mulberry32 `rngT`, the Box-Muller `gaussT`;
    - the persistent `tOff`, reseeded on a seed change only;
    - `armMembers` (initVoice: the jitter draws, then the Vorberg/Wing correction, the re-centre and the earliest-at-zero shift);
    - `memberStep` (renderSeg: each member waits with its phase frozen, then the onsE entry ramp, or with voiceEnv each member runs its own envelope).
  - **Per-member gain.** It rides RazorCore's pan gains `gl/gr`, which are snapshotted per render call. So the gain scales the member's DC-corrected output, and nothing is written when no voice uses it.
  - **New `d` keys:** `grav` 0, `basin` 35, `onsetScatter` 0, `onsetAlpha` 0.25, `attackScatter` 0, `voiceEnv` 0, `relScatter` 0.
  - **`viz.horde` gains** `f0`, `grav` (the pairs gravity holds: ratio, octave, cents), `onsetMs` and `gain`.
- **`tools/onset_ref_check.cpp`** (new, `WIRED: ./verify full`). It drives SwarmCore through `swarm_core.h` only (the ncap_check pattern) and emits `tools/labharness/onset_ref_cpp.json` (new, 109 KB, generated):
  - 40-note serial draws at four correction gains and two seeds;
  - 44.1 kHz renders: `ens`, `venv`, `coupled`, and the `os2` finding.
  - Its check mode re-derives the JSON and fails if the committed copy is stale. It calibrates with a planted one-byte change.
- **`CMakeLists.txt`**: the target. **`verify`**: one invocation beside `tseed_check`. It also corrects the composed check's cost prose (~10 s → ~30 s).
- **`tools/labharness/composed_engine_check.mjs`**: 35 new rows (GRAV, ZERO, ONS, VENV, API), 90 → 125.
- **`tools/patchspace/dependency_tree.json`** is regenerated. The diff covers two things:
  - seven engine anchor line numbers moved (the file grew);
  - the seven new keys joined `unsampled` at their defaults.

  They are not lab rows (`HORDE_T3` names them, the lab draws none), so they do not enter the sampled space. There are 0 structural disagreements.

## Evidence (`node tools/labharness/composed_engine_check.mjs`: GREEN, 125 rows, 0 failed)

**GRAV.** Composed engine vs DynSynth, every sample of the first 4096 and every 32nd after, 1 s. Every note's member phases, member frequencies and f0cur matched EXACTLY (Δ 0) in five scenarios:
- a fifth 13 c sharp (702.491 c at 1 s in both engines);
- K .35 from random phases;
- a late triad;
- an octave-folded twelfth;
- a pair outside a 10 c basin, which stays at 713.000 c.

A 3 s settle reaches 701.9563 c; 3/2 is 701.9550 c.

Controls:
- DynSynth without gravity: Δφ 0.5000.
- Per-call gravity (the pre-ADR-086 law): Δφ 1.12e-3.
- A 256-sample grid at 48 kHz: Δφ 2.20e-4.

**ZERO.** Three renders fingerprinted against main at c79be56 (SHA-256 of the float32 output) are equal, with the new keys absent and with them written explicitly at grav 0.004:
- Glass horde pad `acfdf284a132da01`;
- Two blades `7dbb7b19c2412b11`;
- horde rows `8aecd8e01b3da445`.

The twins with grav .5, onsetScatter 10, and voiceEnv + relScatter each differ.

**ONS**, against the C++ fixture:
- **Draws.** Five runs of 40 notes × 7 members:
  - max|ΔtOff| ≤ 2.8e-17 s;
  - wait ≤ 3.6e-12 samples, with 0 whole-sample waits differing;
  - onsC exact, relC 5.1e-13 relative (the last ulp of exp/log, amplified by 1 − exp(−x)).
- **ADR-077's structure law**, lag-1 of the asynchrony over 300 notes at α 0 / .25 / 1 / 1.5: 0.987 / 0.760 / 0.021 / −0.486. The ADR measured +0.985 / +0.679 / −0.072 / −0.550. Over the fixture's 40 notes, JS = C++ (0.829 / 0.553 / −0.091 / −0.533).
- **Rendered entries** are exact:
  - `ens` [1544 1438 1372 0 1390 2749 827 | 1768 641 2102 2 1417 2999 1239];
  - `coupled` [2058 1917 1830 0 1854 3665 1103].
- onsE Δ 1.1e-16. Phase Δ 0 (uncoupled) and 8.7e-19 cycles (K .35).
- Controls:
  - wrong α sign: ΔtOff 400 s;
  - the pre-B149 literal seed: 0.157 s;
  - i.i.d. jitter with no memory: lag-1 0.021 < 0.4;
  - a waiting member whose phase runs: entries all 0.

**VENV.**
- Entries are exact.
- Per member, the JS/C++ half-time ratio is one constant per stage: attack 0.7213 within 1.8e-3, release 0.25 within 3.3e-4 (tolerance ratio·(1/t_JS + 1/t_C++)).
- Unscattered voiceEnv ≡ voiceEnv off, max|Δ| 0.
- A voice rings 304 ms with release scatter 1 against 229 ms without, so liveness follows the loudest member.
- Control: no drawn factors, ratio off by 0.906.

**Kept intact.** B310 VL rows, B323 CULL rows and the B325 rows are all PASS. O1, O2 and O3 are unchanged.

**`listening_pass_check`**: GREEN, 31/31, 8 controls, 36 patches. The new parameters at their defaults move no sampled patch.

**`onset_ref_check`** on the committed fixture: OK. A copy with one appended byte fails as STALE.

## Findings

- **(1) The C++ counts a member's wait and steps its entry ramp per SUB-sample** (inside the ADR-075 loop). At its 2x setting, a 15 ms scatter waits 7.5 ms (os2 entries [772 719 686 0 695 1374 413] vs 1x [1544 …]). The composed engine counts output samples at every os, which is the C++'s 1x law. This is a C++ defect for the divergence ledger, not fixed here (`src/` is out of scope).
- **(2) Per-partial env uses this engine's envelope law** (RazorCore's ADSR per member, with the C++'s drawn time factors and 2 ms floor). ADR-078 defines it as "the same arithmetic as the shared envelope", and here the shared envelope is RazorCore's (D11 is not composed). The draws and timing are the C++'s exactly; the shape is the composed engine's. The check compares the laws, not the samples.

## Alternatives rejected

- Calling DynSynth instead of copying gravityStep: rejected because it changes the factory signature (see above).
- Transcribing the C++'s one-pole per-voice ADSR verbatim: rejected because voiceEnv would then change the envelope shape even with nothing scattered, contradicting ADR-078's uniform-when-unscattered property.
- Counting the wait per sub-sample like the C++: rejected because it is ADR-009's defect (time in seconds).
- A live C++ call from the node check: rejected because the check runs before the build in `verify full`. A committed fixture plus a staleness check covers both legs.
- Segmenting every render on the grid: rejected because RazorCore re-reads per-call state, so it could move gravity-off samples while controls glide. Segmenting happens only with gravity on.

## Verify

`./verify fast` and `./verify full` on the committed hash are in the PR body (`.harness/last-verify.json`).

## Open questions

- A non-fresh (mono) retrigger keeps the draws (the C++ re-strikes and draws again). This is the same open question as B298's swarm restart.
- A member that waits on a STOLEN voice drops to silence until it enters (the C++'s onsE = 0). This is a possible click of the same class as B310's stolen-slot note.
- Switching onset scatter or voiceEnv mid-note takes effect on the next note: members are armed at note-on. The C++ reads stale per-voice state there.
- The C++ OS finding (1) goes to the ADR-187 divergence ledger.
