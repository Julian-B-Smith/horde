# b360-noise-rough-labels — noisy and rough become LABELS, not failures; main merged; P3 regenerated

- **Queue item:** B360, read verbatim from `origin/lead-records-142:ROADMAP.md` (records PR #855)
  beside B351 (both on PR #853, `patchspace-alias-refit`). Also carries B351's main-merge
  conflicts, which the first agent on this branch stalled on (8-minute watchdog) before resolving.
- **Why:** the human's ruling (2026-09-29): "I think noisy and rough should just label; some
  patches want noisy or rough." B350/B351 had noiseDb and roughness reducing both `healthy` and
  `coherent` yield in `gauntlet.mjs`, same as aliasing and root presence. The human's ruling
  narrows that: aliasing (`aliasConvDb`) and an absent root (`rootPresence`) are genuine defects: a
  patch that fails them is broken. Noise and roughness are not — a noise blade or a rough,
  detuned swarm can be exactly what a patch wants — so they should tag the record for P4 to target
  or avoid, not count against yield.

## 1. The merge with `origin/main`

Checked out `origin/patchspace-alias-refit` (`382ccea`), ran `git submodule update --init
--recursive`, then `git merge origin/main` (`9529eae`: #847, #848, #849, #851). **The merge was
clean — no conflicts.** `git status --short` after the merge was empty; the only changes are the
merge commit itself (`eb2e462`), bringing in #849's ROADMAP/DECISIONS records, #851's
`tools/sluice_hold_check.py` (wired in `verify`) and #848's `docs/design/sluice-horde-lab.html`
round 3. Confirmed both B351 and B360's ROADMAP rows are present and correct after the merge
(`grep -n '^| B35[0-9] \|^| B36[0-9] ' ROADMAP.md`). No check or row from either side was dropped —
there was nothing to drop; the histories touched disjoint files (this branch: `tools/patchspace/*`,
`docs/patchspace/*`; main since the branch point: `ROADMAP.md`, `DECISIONS.md`,
`docs/design/sluice-horde-lab.html`, `tools/sluice_hold_check.py`, `verify`, some traces).

## 2. B360 in `tools/patchspace/gauntlet.mjs`

- `incoherence(r)` now returns only `rootAbsent` (rootPresence < 0.616, unmoved). The noiseDb and
  roughness lines are deleted from it, not commented out — reduce, not accumulate dead branches.
- New exported `labels(r)`: `noisy` when `noiseDb > THRESH.noiseDb` (−21.5), `rough` when
  `roughness > THRESH.roughness` (0.12) — the same two conditions, moved verbatim, so the CUTS
  themselves are unchanged (B350's fit stands; only what they DO changed).
- `failures(r)` is untouched: `aliasConvDb` (B351, −33.4) was already there and stays there.
- The worker loop (`isMainThread` guard) now sets `r.labels = labels(r)` before writing the record,
  so a NEW gauntlet run carries `labels` inline in its JSONL, same footing as every other derived
  field.
- THRESH's header comment and each of the four lines gets a one-line note naming which function
  reads it (FAILURE via `failures()`/`incoherence()`, or LABEL via `labels()`), so a future reader
  does not have to cross-reference the code to know which of the four still gates.

## 3. Consumers updated

- **`tools/patchspace/gauntlet_report.mjs`:** imports `labels`; the per-row loop now sets
  `r.labels = labels(r)` beside `r.fail`/`r.inco` (recomputed live from THRESH, same convention as
  the other two — never trusted from disk, so an OLD run's records, which predate this change and
  carry no `labels` field on disk, report correctly here). `INCO` is now `['rootAbsent']`; a new
  `LABELS = ['noisy', 'rough']` constant. The Yield table gained two columns (`noisy`, `rough`)
  that report their RATE without touching how `healthy`/`coherent` are computed (unchanged:
  `!r.fail.length` / `!r.fail.length && !r.inco.length` — automatically correct once `r.inco` no
  longer carries noisy/rough). The Thresholds table's two rows are relabelled `(LABEL, B360 — does
  not reduce healthy or coherent yield)`. The hotspot loop is `CLASSES.concat(INCO).concat(LABELS)`
  with `has()` checking `r.labels` too, so `noisy`/`rough` keep their hotspot rows. One open
  question and the failure-hotspots heading are reworded to say LABEL, not gate.
- **`docs/design/listening-pass.html`:** read in full for anywhere its reveal or detector text
  calls noise or roughness a *failure* (the brief's condition). Found none: the page's `noisyM`/
  `detectorControls`/`QUI` machinery asks the human "is this patch noisy/rough" and asserts its OWN
  detector reads correctly on constructed signals (`ok: noisyM(m)` for a seeded-white-noise
  control) — that is a DIFFERENT claim ("the noise detector detects noise") than "noise fails a
  patch", and B360 does not touch it. `listening_sample.mjs`'s `strataFor`'s `broken` bucket
  (`r[al] >= -15 && r.roughness >= 0.15 && (noisy(r) || r.rootPresence <= 0.3)`) is also unchanged:
  it selects which patches go INTO the human's calibration sample, a sampling-design choice
  distinct from the gauntlet's own yield gate, and the ROADMAP row does not name it. Left alone,
  both files.
- **`tools/patchspace/README.md`:** one line added to the `gauntlet.mjs` row naming B360.

## 4. New check rows (`tools/patchspace/metrics_check.mjs`, already `WIRED: ./verify fast`)

The brief's `grep -rl 'failures(' tools/patchspace/*check*` finds nothing — no existing checker
calls `failures()`/`incoherence()` at all (the DSP checkers test the DSP; nothing had tested
`gauntlet.mjs`'s own classification). `metrics_check.mjs` already imports `measure` from
`gauntlet.mjs` and has an "engine controls" section (E1–E3), so it is where the gauntlet's other
functions are exercised; three rows added there, on CONSTRUCTED records (not rendered patches —
`failures`/`incoherence`/`labels` are pure functions of the measured fields):

- **G1** a noisy-but-otherwise-clean record (`noiseDb −10 > −21.5`, everything else clean) is
  healthy (`failures` and `incoherence` both `[]`) and carries `labels` `['noisy']`.
- **G2** an aliased record (`aliasConvDb −10 > −33.4`) still fails (`['alias']`) — aliasing was not
  moved.
- **G1c CONTROL:** a planted `preB360Failures` (the SAME `failures()`, with a noise-gating line
  spliced back in) DOES flag G1's record with `'noisy'` — proving G1's "healthy" assertion is not
  vacuously true, and that a regression back to gating noise would be caught here, turning G1 red.

All three PASS (`node tools/patchspace/metrics_check.mjs`: GREEN, output below). No `WIRED:` note
needed — the file was already wired under ADR-180 §1 (B316); these are new rows in an existing
gate, not a new gate.

## 5. P3 report regenerated once

The run `p3b346`'s local data (2000 broad + 1000 edge, seed `0xb316`, from #847) was not on this
worktree or the main checkout's `local/` (git-ignored, so it travels with whichever worktree ran
it) — found instead in a sibling agent worktree
(`.claude/worktrees/agent-ac68172c10473e8c7/local/patchspace/p3b346/`) and copied (not moved,
read-only source) into this worktree's own `local/patchspace/p3b346/`. Regenerated with
`node tools/patchspace/gauntlet_report.mjs --run p3b346 --out docs/patchspace/2026-09-27-gauntlet-p3.md`
(~9 s; the "about 3 min" estimate covers a machine under load — this run was quiet). The committed
report now reflects the FINAL THRESH (B351's `aliasConvDb` gate, B360's labels), not B316's original
cuts.

**Yields, before vs. after B360** (same `p3b346` data throughout; `healthy`/`coherent` per
`gauntlet_report.mjs`'s own definitions; "before" = B351's incoherence(), which still gated
noisy/rough):

| mode | n | healthy | coherent BEFORE B360 | coherent AFTER B360 |
|---|---|---|---|---|
| broad | 2000 | 38.5% | 8.9% | 20.3% |
| edge | 1000 | 25.8% | 18.4% | 22.5% |

`healthy` is unchanged (noiseDb/roughness were never in `failures()`); `coherent` rises because it
no longer also requires `!noisy && !rough`. The broad figures match B351's trace exactly (38.5% /
8.9%) — confirming this report is the same run, re-summarised under the new rule, not a re-measure.

## Evidence consulted

- `origin/lead-records-142:ROADMAP.md` B350/B351/B360 (verbatim, via `git show`).
- `origin/patchspace-alias-refit:traces/2026-09-29-b351-alias-refit.md`.
- `tools/patchspace/gauntlet.mjs`, `gauntlet_report.mjs`, `metrics_check.mjs`,
  `listening_pass_check.mjs`, `listening_sample.mjs`, `docs/design/listening-pass.html` (read in
  full for the "calls noise or roughness a failure" condition).
- `verify` (the `fast`/`full` sections covering `tools/patchspace/*check*.mjs`).
- The `p3b346` run data (`local/patchspace/p3b346/*.jsonl`, `meta.json`), read-only from a sibling
  worktree.

## Alternatives rejected

- **Moving rootPresence into `failures()`:** B360's ROADMAP text says aliasing and rootPresence
  "stay as failures", which reads as "keep gating" in plain language, not as a literal
  code-location instruction — the existing `failures()`/`incoherence()` split (B351, unquestioned)
  already treats both as gates; moving rootPresence's *function* would be an uninstructed, unscoped
  refactor of B351's structure for no behavioural change. Left it in `incoherence()`.
- **Baking `labels` only into the on-disk record, never recomputed by the report:** would make an
  OLD run's report (like `p3b346`, collected before this change) silently omit `labels` unless
  re-run. `gauntlet_report.mjs` already recomputes `fail`/`inco` live from THRESH for exactly this
  reason; `labels` follows the same convention.
- **Testing G1/G2 through the real engine (`measure()`), as E1–E3 do:** unnecessary and slower —
  `failures`/`incoherence`/`labels` take a measurement record, not a patch, so the DSP is not in
  the code path being tested. Constructed records isolate the classification logic, the same
  reasoning the M-series metric rows use for `metrics.mjs`.

## Verify

- **VERIFIED** `./verify fast` exit 0 at `090ad1d` (`{"target":"fast","exit":0,"git":"090ad1d","ts":"2026-09-29T15:09:12Z"}`). `metrics_check.mjs: GREEN — 10 metric rows + 9 B345 rows + 9 B346 rows + 4 INFO rows + 3 engine controls + 3 B360 gauntlet-classification rows (noisy-but-clean is healthy and labelled, aliased still fails, a planted pre-B360 control)`.
- **VERIFIED** `./verify full` exit 0 at `090ad1d` (`{"target":"full","exit":0,"git":"090ad1d","ts":"2026-09-29T15:16:07Z"}`). No B362 (load-sensitive timeout) hit this run. Relevant lines: `dependency_tree_check.mjs: GREEN — 113 params, 3170 probe comparisons, 0 structural disagreements required; 36.8 s`; `fidelity_scan_check.mjs: GREEN — 21 rows, 0 failed`; `listening_pass_check.mjs: GREEN — listening pass: 60/60 checks, 20 must-fail controls, 36 patches, 64 s` (unaffected by B360 — it imports `THRESH`, not `failures`/`incoherence`).
- This trace's own commit is included in the verified tree (committed before either run).

