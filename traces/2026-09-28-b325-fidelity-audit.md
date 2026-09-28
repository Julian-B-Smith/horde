# 2026-09-28 — B325: DSP fidelity audit (salvaged and merged by the lead)

**What changed.**
- **The report:** `docs/patchspace/2026-09-28-fidelity-audit.md`.
- **Audit tooling:**
  - `tools/patchspace/fidelity.mjs` (shared phrases and detectors);
  - `tools/patchspace/fidelity_audit.mjs` (the passes);
  - `tools/patchspace/fidelity_scan_check.mjs` (a standing check, wired into `verify full`, 21 rows).
- **One composed-engine fix:** the first swarm tick is taken at the strike, so the blades never read the undetuned pitch as the members' frequency. It comes with five `composed_engine_check` rows, two of them must-fail controls. One existing VOICE row now reads the re-strike directly, which is stricter than before.
- **A regenerated `dependency_tree.json`.** Only evidence anchors and probe counts moved; there are 0 structural disagreements.

**Why.** The human asked (2026-09-28): "I'm also starting to notice more noise and clicks that I'm not certain are supposed to be part of the waveforms … it would be worth running some tests to make sure I'm not crazy."

**Findings.**
- **The audible clicks are PLAYBACK UNDERRUNS.** 33 of 83 presets underran in the lab, adding up to 47 s of inserted silence. Meanwhile the worklet's rendered samples are bit-identical to an offline replay for 65 presets and within 1e-5 for 18, with 0 transients in the tap or the replay.
- **B323 corroborates this independently:** its in-graph capture reads 0 render clicks against ~1000 device underruns.
- **Real render artefacts:**
  - the first-tick pitch placeholder, the one defect the composed engine introduced itself (FIXED);
  - stolen voices restarting from their current envelope, an oracle-inherent mechanism made more frequent by B310's law (proposed, not fixed);
  - beating from horde's coupling law (by design).

**How it landed.** The implementer stalled on the stream watchdog after finishing the report. The lead:
1. committed the work-in-progress (`fa4b37e`);
2. ran `composed_engine_check` (GREEN) and `verify full`, which failed only on the stale dependency tree, then regenerated the tree (evidence only);
3. merged the result onto B323's branch (`lab-scalpel-10`), rebuilding `composed_engine_check.mjs` from B323's version plus B325's four hunks. The textual conflict resolution had broken a block, so the file was rebuilt rather than resolved; the result is 90 rows GREEN;
4. merged `origin/main`, which brought in B324: the gauntlet exports, and both `verify` blocks kept;
5. re-selected the B324 listening sample on the fixed engine, because its pinned measurements are a golden against the engine of the day, as that check's own header says. The gitignored pool was rebuilt, and nine chaotic keys are excluded after Chrome `?xverify=1` read 36/36 within tolerance.

**Verify.** `./verify fast` and `./verify full` exit 0 at `d20082e`: composed engine 90 rows, dependency tree GREEN, fidelity scan 21 rows, listening pass 31/31.

**Evidence consulted.** The audit report; B323's measurements (PR #828); `.harness/last-verify.json`; the Chrome cross-runtime readout on the rebuilt sample.
