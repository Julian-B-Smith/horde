---
id: kiln-notice-manifests
from: Kiln
to: HYPERSAW
thread: kiln-fx-modules
in-reply-to: none
status: filed
ball: none
respond-by: none (information; the asks are in kiln-brief-admission-adapters, seq 6)
seq: 5
filed: 2026-10-30
cites: Kiln DECISIONS D-004, D-008 to D-013; Kiln docs/manifests/{eq,drive}-manifest-draft.md (RATIFIED, D-013); Kiln core/include/kiln/{eq.h,drive.h}; horde rack-slot-contract.md RS-2.1, RS-2.4, RS-7.1, RS-7.4, §12.7 (ADR-309); horde ADR-313
---

> **Origin.** Kiln lead session, 2026-10-30. Kiln's queue item Q-006, criterion 1: once the human ratified both
> manifest drafts (Kiln D-013), a notice carries them to horde. Kiln's human said "start Q-006".

# Notice: Kiln's two modules, their ratified manifests

Kiln (horde ADR-313) holds two hosted FX modules, built to the rack-slot contract.

**1. The EQ (`eq`).**
- **Manifest:** `manifest-eq.json`, beside this file.
- **Keys:** 38. Per band n = 1…6: `eq.bN.on`, `type` (LC, LS, PK, HS, HC), `freq`, `gain`, `q` and `slope`
  (12, 24 or 48 dB/oct). Plus `io.inGain` and `io.outGain`.
  - Flip keys switch by an equal-gain crossfade of 20 ms, declared in ADR-309 form.
  - Morph keys ramp over 20 ms.
- **Declarations:**
  - latency 0;
  - `min_block` 1;
  - a finite tail that follows the settings (`may_change` true);
  - `changes_image` false, `changes_level` true (Kiln D-011).
- **Audible-tail constant for the DAW (RS-7.4): 120 s, provisional.** The lab's stacked cut-Q law (+109 dB at
  Q 18 on a 48 dB cut) sets it. Kiln's Phase 2 listening batch on cut Q rules on that law, and the constant
  follows. If Q is capped at 2, it would be about 6 s.
- **Factory preset:** `presets/eq/factory/Default`.

**2. The drive (`drive`).**
- **Manifest:** `manifest-drive.json`.
- **Keys:** 8.
  - `drive.curve` (soft, hard, tube, diode, in Maw's order): flip, equal-gain crossfade, 20 ms.
  - `drive.amount` (0–24 dB), `drive.shape`, `drive.mix`.
  - `drive.autogain` (flip, default off).
  - `drive.output`.
  - `io.inGain`, `io.outGain`.
- **Path:** one fixed 2× polyphase IIR half-band with first-order ADAA, ported from Maw. Mix is blended at 2×
  (Kiln D-010).
- **Declarations:**
  - **latency 4**, residual 0.497 early: measured 3.503 = half-band 3.253 + ADAA 0.25 (Kiln D-009; your RS-7.1 row
    as corrected);
  - `min_block` 1;
  - a fixed finite tail of `ceil(0.55 s·rate) + 1028` samples (0.571 s at 48 kHz). That is also its audible
    bound;
  - `changes_image` true and `changes_level` true (Kiln D-012).
- **Factory preset:** `presets/drive/factory/Default`.

**Frozen from the first release (RS-2.1).** Both manifests are ratified (Kiln D-013), and Kiln will change one
only through its human gate, with a notice to horde. The Phase 2 A/B candidates live behind a dev-only selector
outside the manifest (Kiln D-005). A candidate reaches the manifest only if the human rules it the winner.

**Source.** Kiln `main` at `cc72596`. The JSON files here are the modules' own `kiln::manifestJson(…)`
output at that revision, byte for byte.
