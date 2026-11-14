# Proposal: correct the Layer-0 wording in specs/ACCEPTANCE.md (B689)

> **Origin.** Written by an agent the horde lead dispatched, 2026-11-14, for ADR-398 item 6 (the human's B676
> rulings on the 2026-11-10 audit, `docs/audits/2026-11-10-fable-audit.md` item 16), ROADMAP row B689. Revised
> the same day after an adversarial review.

## What is proposed

Two parts, kept apart because only the first is inside the ruling.

- **Ruled deliverable (ADR-398 item 6): sections (c) and (d).** The exact replacement for `specs/ACCEPTANCE.md`
  line 3, which says Layer-0 runs in `./verify fast` and is CI-blocking, and the exact replacement for L0-18, which
  points at what checks it, which is nothing.
- **Beyond the ruling: section (e).** Appended "enforced in" sentences on L0-16, L0-17, L0-19, L0-20 and L0-21.
  ADR-398 item 6 does not name them. Each needs its own sanction; approving (c) and (d) approves none of them.

No acceptance number changes (acceptance numbers change only with a re-measurement).

`specs/ACCEPTANCE.md` is a protected file. **Nothing changes until the human sanctions this text.** The lead edits
the file only with that sanction, word for word.

## (b) Current line 3, exactly

```
Two layers, per ATTEST discipline. **Layer-0**: deterministic, CI-blocking, runs in `./verify fast`. **Layer-E**: behavioral/listening checks, `./verify full` + human sign-off. Every number below was measured on the JS reference implementations in this design cycle; tolerances are stated where drift is acceptable.
```

## (c) Proposed replacement for line 3 (ruled deliverable)

```
Two layers, per ATTEST discipline. **Layer-0**: deterministic, no model calls; blocking when run (a red `./verify full` fails the run, and the sanitize CI job fails on a red oracle). Its parity and trajectory oracles run in `./verify full`, not `./verify fast`. GitHub CI never runs `./verify full`; it runs these oracles through the sanitize job on pull requests and pushes to main (the compiled oracles `./verify` names, rebuilt with address and undefined-behaviour sanitizers, less those pinned in `tools/sanitize_unbuilt.pin` as not linking on Linux, `subosc_check` among them; a change that touches only `.md` files, `docs/` other than the preset bank's JSON, or `traces/` skips it), and of these oracles the nightly runs only `stability_check` (L0-13), at 3600 s. `./verify fast` runs none of the L0 oracles. One item has no tool at all (L0-18); L0-16, L0-17 and L0-19 to L0-21 are checked only in part. **Layer-E**: behavioral/listening checks, `./verify full` + human sign-off. Every number below was measured on the JS reference implementations in this design cycle; tolerances are stated where drift is acceptable.
```

Note for the human: the middle sentences are longer than a plain fix needs. A shorter alternative that is still true:
"**Layer-0**: deterministic, no model calls. Run by `./verify full` (never by `./verify fast`); GitHub CI reaches it through the sanitize job, and of its oracles the nightly runs only `stability_check` (L0-13)."

The line points at no proposal file; the per-item map below is evidence for the human, not part of the spec.

## (d) Proposed L0-18 change (ruled deliverable)

Current text:

```
## L0-18 · Filter-family stability
Bank: K + inertia 0.7 + drift 150¢ + Q ceiling simultaneously — bounded, NaN-clean, 4 s. Notch swarm: feedback 0.85 + K 0.8 + drift 150¢ + stage Q 3 — peak ≤ 0.7 post-tanh, NaN-clean, 4 s.
```

Evidence for "unbuilt": searching for "L0-18" finds it only in this file, in `docs/armor/tolerances.json:168-173` (the number
`≤ 0.7` is pinned there), in DECISIONS/ROADMAP prose, one trace, and the 2026-11-10 audit that raised it. No file under `tools/` sets feedback 0.85 with K 0.8
and drift 150 on `NotchCore`, or K, inertia 0.7, drift 150 and a Q ceiling on `FilterCore` (`tools/filter_check.cpp` and
`tools/notch_check.cpp` set neither; `notch_check.cpp:148` sets feedback 0). ROADMAP row B4 ("E1 remainder", whose
title reads "L0-17/18") is PARKED, and the E1 section's REMAINING line lists "L0-17 audio (tuned harmonic rejection) + L0-18
family-stability long-runs as oracle rows". This read was of the `b676-rulings` worktree (origin/main `e4b0cebc` plus uncommitted
ROADMAP, DECISIONS and design-ledger edits), so rows and anchors are cited, not line numbers.

Proposed text, exactly (one sentence appended):

```
## L0-18 · Filter-family stability
Bank: K + inertia 0.7 + drift 150¢ + Q ceiling simultaneously — bounded, NaN-clean, 4 s. Notch swarm: feedback 0.85 + K 0.8 + drift 150¢ + stage Q 3 — peak ≤ 0.7 post-tanh, NaN-clean, 4 s. **Not yet built (2026-11-14): no tool checks this item; ROADMAP B4 holds it (parked with SWARM-FX). The notch peak bound (0.7) is pinned in docs/armor/tolerances.json, but nothing measures it.**
```

## (e) Beyond ADR-398 item 6: optional, each needs its own sanction

None of this is in the ruling. Each paragraph is a separate yes or no; dropping any of them leaves (c) and (d)
unchanged. Each is one appended sentence; no number is touched, and none adds a comparison sign: `tools/tolerance_registry_check.py` reads every sign in ACCEPTANCE.md as a tolerance, so a new one would turn `./verify fast` red until approved.

**L0-16** (current text ends "...the allpass-topology defect has returned."). Evidence: `tools/notch_check.cpp:142-161` checks
that the on-notch attenuation is at least 20 dB deeper than a probe a quarter-octave away, for one notch (band 0). It does
not measure "≥ 60 dB below the adjacent ridge at each swarm frequency" (the ACCEPTANCE reference for that is about 150 dB;
60 dB is the criterion). `notchslot_check` is not a notch-exactness check: it tests that FX rack slot type 6 reaches
`NotchCore` through the plugin's param path (`tools/notchslot_check.cpp:1-14`). Append:

```
Enforced in `notch_check` as a looser relative check (one notch, band 0, at least 20 dB deeper on the notch than a probe a quarter-octave away); the 60 dB criterion below the ridge at every swarm frequency is not yet a gate.
```

**L0-17** (current text ends "...gather σ and comb CV match L0-14 within tolerance."). Evidence: `tools/force_check.cpp:286-296` checks only the
shared-core half (phaser sigma ratio and gap CV match the bank). No tool checks the ≥ 15 dB rejection at gravity-captured
notches (searched `tools/force_check.cpp`, `notch_check.cpp`, `notchslot_check.cpp` for "reject", "15 dB", "90"; none).
The E1 section's REMAINING line names "L0-17 audio" as remaining. Append:

```
Enforced today only for the shared-core check, in `force_check`; the rejection half (the 15 dB criterion at captured notches) is not yet built.
```

**L0-19, L0-20, L0-21** (lower confidence; the human may drop any of these). Evidence: `tools/force_check.cpp:306-328` and
`tools/time_check.cpp:135-181` check the gather σ, the grid CV, rhythmic gravity, the sympathetic equilibrium ratio, the
echo tail mean |amplitude| ≤ 0.15 (last half of the render, no flatness test), "room resonates" (RMS > 1e-4, although the
block comment at `time_check.cpp:151-152` promises an on-comb versus off-comb Goertzel the code does not do) and room
boundedness with mean |DC| ≤ 0.05. Not found in any tool: tap echo detection at ±2 ms ("impulse honesty"), the IR comb
≥ 10 dB over an off-comb probe, the Householder-sign regression guard, the sympathetic f0-vs-off ≥ 2.5 dB, per-line
DC ≤ 0.01, and the DC-blocker corner and 26 Hz cost. Append to each:

```
L0-19: Enforced in `force_check` (σ, grid CV, rhythmic gravity) and `time_check` (tail mean |amplitude| no more than 0.15 only; flatness is not checked); impulse honesty is not yet built.
L0-20: Enforced in `force_check` (sympathetic equilibrium) and `time_check` (a sustained-energy check only); the 10 dB IR-comb criterion, the 2.5 dB improvement and the Householder-sign regression guard are not yet built.
L0-21: Enforced in `time_check` as boundedness plus a mean-DC bound of 0.05 (looser than, and different from, the criterion); the per-line DC criterion and the DC-blocker figures are not yet built.
```

Items with no wording problem found: L0-1 to L0-15 and L0-22 to L0-24 (each has a named oracle in `./verify full`;
see the table). L0-24's text names `trajectory_check`'s "ADR-056" block (`tools/trajectory_check.cpp:991`, the
"ADR-056 bipolar onset lock" section), which is the right tool, although that file does not use the label "L0-24".

## (f) The map (evidence, not proposed text)

Verify target: `fast` = `fast()` at `verify:152-841`; `full` = `full()` at `verify:843-1736` (it also runs
`fast`). `fast()` calls no oracle in this table (its only mentions are comments, e.g. `verify:155`). The Stop gate
accepts a `./verify fast` record (`.kit/stop-gate.sh:56`), so nothing blocks automatically on the L0 oracles.
CI reach: **sanitize** = `.github/workflows/ci.yml:150-173` runs `tools/sanitize_oracles.sh`, which parses every
`"$build_dir/<name>"` line out of `./verify` and runs it under address+undefined sanitizers
(`tools/sanitize_oracles.sh:10-14, 32-42`). It keeps only a build-golden argument and drops every other argument
(`sanitize_oracles.sh:35`), so `stability_check` runs there at its 600 s default, not `full`'s `--seconds=60`, and
`h2_scalpel_parity_check` runs without `--full-from`; `rtsafety_probe` is never run under a sanitizer
(`sanitize_oracles.sh:224`). It runs on push to main and on pull requests, except when the diff touches only `.md`
files, `docs/` other than the preset bank's JSON, or `traces/` (`ci.yml:79-116`, `150-152`).
**verify-fast** = `ci.yml:117-129` runs `./verify fast` only. No workflow runs `./verify full` (`grep full .github/workflows/*.yml` finds nothing
that calls it). Nightly = `nightly.yml:60` runs `./verify nightly`, whose job list is `tools/nightly/jobs.json`.
The only L0-related nightly job is `stability-legacy-saw` (the 3600 s `stability_check`).
`tools/sanitize_unbuilt.pin:10-11` lists the oracles that do not link on Linux; none in this table except `subosc_check`
(the L0-1 parity of SUB OSC), which the sanitize job skips.

| L0 id | tool | verify target | CI reach | evidence |
|---|---|---|---|---|
| L0-1 parity | `parity_check` (legacy SwarmCore), `h2_swarm_parity_check` (lifted core), `h2_scalpel_parity_check`, `h2_engine_parity_check` (the current composed engine, ADR-187; `verify:1169`; under sanitize it runs without `--full-from`), plus per-core `spectra_check`, `filter_check`, `notch_check`, `time_check`, `swarmalator_check`, `station_check`, `subosc_check` | full | sanitize (all but `subosc_check`, which is skipped on Linux) | `verify:1019,1026,1118,1656,1662,1668,1674,1684,1696,1711`; `tools/parity_check.cpp:2`; `tools/sanitize_unbuilt.pin:10` |
| L0-2 sync transition | `trajectory_check` | full | sanitize | `tools/trajectory_check.cpp:181-216`; `verify:1042` |
| L0-3 splay + regime table | `trajectory_check` | full | sanitize | `trajectory_check.cpp:218-264` |
| L0-4 inertia | `trajectory_check` | full | sanitize | `trajectory_check.cpp:267-290` |
| L0-5 R→tone | `trajectory_check` | full | sanitize | `trajectory_check.cpp:293-302` |
| L0-6 cascade zipper | `spectra_check` | full | sanitize | `tools/spectra_check.cpp:140-176`; `verify:1656` |
| L0-7 interference gate | `spectra_check` | full | sanitize | `spectra_check.cpp:179-192` |
| L0-8 consonance gravity | `trajectory_check` | full | sanitize | `trajectory_check.cpp:426-466` |
| L0-9 Sakaguchi lag | `trajectory_check` | full | sanitize | `trajectory_check.cpp:469-494` |
| L0-10 two-cluster | `trajectory_check` | full | sanitize | `trajectory_check.cpp:497-548` |
| L0-11 ring coexistence | `trajectory_check` | full | sanitize | `trajectory_check.cpp:552-590` |
| L0-12 tempo-grid law | `trajectory_check` | full | sanitize | `trajectory_check.cpp:305-410` |
| L0-13 determinism and stability | `trajectory_check` (bit-identical, fuzz grid), `stability_check --seconds=60`, `denormal_check`, `tseed_check` | full; `stability_check` 3600 s also nightly | sanitize (`stability_check` at its 600 s default); nightly (Linux, `stability-legacy-saw`) | `trajectory_check.cpp:700-771`; `verify:1396,1397,1404`; `tools/stability_check.cpp:182`; `tools/nightly/jobs.json` (job `stability-legacy-saw`) |
| L0-14 filter bank | `force_check` (population), `filter_check` (collapse→Q audio half) | full | sanitize | `tools/force_check.cpp:250-266`; `tools/filter_check.cpp:124-138`; `verify:1650,1662` |
| L0-15 harmonic gravity | `force_check` | full | sanitize | `force_check.cpp:273-279` |
| L0-16 notch exactness | `notch_check` (looser: ≥ 20 dB relative, one notch) | full | sanitize | `tools/notch_check.cpp:142-161`; `verify:1668` |
| L0-17 tuned rejection | `force_check` for the shared-core half only; rejection half: none found (searched `tools/force_check.cpp`, `notch_check.cpp`, `notchslot_check.cpp`) | full | sanitize (half) | `force_check.cpp:286-296`; ROADMAP E1 section, REMAINING line |
| L0-18 filter-family stability | none found (searched for "L0-18", and tools/ for feedback 0.85, stage Q, Q ceiling, drift 150 on the filter and notch cores) | none | none | `docs/armor/tolerances.json:168-173` (number only); ROADMAP B4 and the E1 section's REMAINING line |
| L0-19 tap swarm | `force_check` (σ, CV, rhythmic gravity), `time_check` (LF stability, tail mean only); impulse honesty: none found | full | sanitize (partial) | `force_check.cpp:306-321`; `tools/time_check.cpp:135-149` |
| L0-20 FDN room | `force_check` (sympathetic), `time_check` (sustained-energy check); IR comb ≥ 10 dB, +2.5 dB, Householder-sign guard: none found | full | sanitize (partial) | `force_check.cpp:328`; `time_check.cpp:151-165` |
| L0-21 time-family stability and DC | `time_check` (bounded, mean DC ≤ 0.05); per-line DC, blocker corner: none found | full | sanitize (partial) | `time_check.cpp:167-181` |
| L0-22 Daido q-clusters | `trajectory_check` | full | sanitize | `trajectory_check.cpp:594-668` (q ∈ {2,3} R_q and bistability at 594-627, split-as-timbre at 629-668) |
| L0-23 A/B balance | `trajectory_check`, `twocluster_check` (K=+0.8 corner) | full | sanitize | `trajectory_check.cpp:670-696`; `tools/twocluster_check.cpp:9`; `verify:1058` |
| L0-24 bipolar onset | `trajectory_check` (ADR-056 block); mirrored in the lab JS by `tools/labharness/composed_engine_check.mjs` | full | sanitize (`trajectory_check`) | `trajectory_check.cpp:991-1003`; `composed_engine_check.mjs:306`; `verify:892` |

### What the map says in one paragraph

Every L0 item with a tool is run by `./verify full` only. CI never runs `full`; the sanitize job runs most of the same
compiled oracles under sanitizers (some with different arguments, some skipped) and is therefore the main CI path to them,
skipped on docs-only diffs; the nightly adds `stability_check` at 3600 s. `./verify fast` runs none of them. L0-18 has no
tool; L0-16, L0-17 and L0-19 to L0-21 are checked more loosely than their text says (the unbuilt halves are listed in (e)).

### Caveats on this map

- The sanitize job builds `RelWithDebInfo` with `-fsanitize`, not the Release flags `./verify full` uses, so it is a
  second run of the oracle, not a copy of `full`'s.
- I did not run `./verify`, CI, or any oracle; every row is read from files in the `b676-rulings` worktree (origin/main
  `e4b0cebc` plus uncommitted ROADMAP, DECISIONS and design-ledger edits). Citations to ROADMAP use rows and anchors
  because line numbers move.
- Whether the sanitize job is a required check on main is a GitHub setting this worktree cannot show.
