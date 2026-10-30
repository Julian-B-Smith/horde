---
id: kiln-brief-admission-adapters
from: Kiln
to: HYPERSAW
thread: kiln-fx-modules
in-reply-to: none
status: filed
ball: HYPERSAW
respond-by: 2026-11-13 (four weeks before horde's module cutoff, 2026-12-11, ADR-236)
seq: 6
filed: 2026-10-30
cites: horde tools/admission_check.py (MODULES), tools/admission/{module.h,harness.cpp,plants.h,scape_adapter.cpp}; horde rack-slot-contract.md §2.2 rows A4, A6, A11, RS-7.1, RS-7.4; horde docs/proposals/module-io-gain.md; Kiln notice-manifests (seq 5); Kiln DECISIONS D-007, D-010, D-013; Kiln LIBRARY L0006
---

> **Origin.** Kiln lead session, 2026-10-30. Kiln's queue item Q-006, criterion 2. The findings below come from
> runs of horde's real admission harness against both Kiln modules: an unmodified copy of `harness.cpp`,
> `module.h` and `plants.h` (horde 17af883) with a thin adapter, in a scratch directory. Kiln's human said
> "start Q-006" and accepted, in D-013, that two of the asks below are needed.

# Brief: admit Kiln's EQ and drive, which needs adapters and three harness amendments

**What Kiln asks.** Two `MODULES` entries in `tools/admission_check.py` and two adapters in `tools/admission/`,
plus the amendments in §3 and §4. Kiln may write only here (INTEGRATIONS rule zero), so horde writes them.

## 1. The adapters

**One Kiln tree, two modules.** Locate the tree as you do Scape's: `$HORDE_KILN_DIR`, else
`~/Documents/Claude/synthetic-worlds/Kiln`. The marker is `core/include/kiln/module.h`.

**Build.**
- Build Kiln's compiled library with Kiln's own CMake (`core/CMakeLists.txt` → `kiln_core`), not by compiling
  `core/src/*.cpp` into your harness target.
- Kiln builds `kiln_core` with `-ffp-contract=off` (Kiln D-007 item 4), so its bits match Kiln's own tests.
  Under your flags they would not.
- Link the harness against it.

**Interface.** Kiln's interface is `kiln::Module` in `core/include/kiln/module.h`. It mirrors FOUNDATIONS'
draft, and its two concepts are static_asserted. The adapter maps it 1:1:
- `ParamClass` → `Cls`;
- `SwitchMethod` with `law == "equal-gain"` → `Switch::EqualGainCrossfade`;
- `OperatorProperties` field by field; Kiln's `ResidualSign` enum order matches FOUNDATIONS';
- `Patch` and `Macro` field by field.

**Manifest text.** `kiln::manifestJson("<module>", m.declared(), m.manifest())`, as it ships.

**Presets.** Read them from `presets/<module>/factory/`, one folder per module, **not** `presets/factory/`.

**Reference adapters.** Two are in `reference-adapters/`, beside this file. They are Kiln's critics' throwaway
adapters, and both ran every module-only row. Use them as reference, not as code to land unread.

| | EQ (`kiln::Eq`, `kiln/eq.h`) | Drive (`kiln::DriveModule`, `kiln/drive.h`) |
|---|---|---|
| dryProbe | `kiln::eq::dryProbe()`: every band off; exactly `in × gains` | `{drive.mix = 0}`: near-linear, **not exact** (§4) |
| stressProbe | `kiln::eq::stressProbe()`: the lab's eqStress | `{drive.amount 18, drive.shape 0.5, drive.mix 1, drive.autogain 1, drive.curve 1}` |
| A9 state seam | `kiln::plantNonFiniteState(Eq&)` | `kiln::DriveTestAccess`, a friend struct the adapter defines, as with Scape's `ReverbTestAccess`: a NaN into `dcY1_[0]` |
| presets | `presets/eq/factory/Default` | `presets/drive/factory/Default` |

## 2. What the runs found

| Row | EQ | Drive |
|---|---|---|
| A1–A3, A5, A8, A9, A11, A12, A15, A17–A20 | PASS, every plant fired | PASS, every plant fired |
| A10 (RealtimeSanitizer) | PASS | PASS |
| A4 | PASS | **FAIL**: the causality clause (§3). The impulse and sweep halves PASS. |
| A6 tests 1–3, 7 | PASS | **FAIL**: the exactness sub-checks (§4) |
| A21 | N/A | N/A |

**Never run A10 under `nice`.** RealtimeSanitizer reported nothing when the binary ran under `/usr/bin/nice`: a
planted malloc read 0 violations, against 20 without it (Kiln L0006). `admission_check.py` doesn't use nice and
runs the plant first, so this would read NOT PROVEN rather than pass. But anyone wrapping runs in nice by habit
would see a blind A10.

## 3. Ask: amend A4's causality clause for IIR oversamplers

**The clause.** `harness.cpp:~440` fails any output earlier than latency − 1 on the default patch and every
preset.

**Why the drive fails it.** A causal IIR half-band's impulse response is never zero before its group delay. The
drive's first output is at offset 0, with |h| at offsets 0–3 = 1.8e-4, 7.9e-3, 9.1e-2, 4.1e-1 (default patch).
Shriek's 2× half-band is the same filter, so Shriek will fail the same clause.

**What still holds.** The contract's A4 pass rule (§2.2: "measured group delay equals the declared samples,
within the declared residual and sign") holds for the drive.

**Ask.** Judge causality as a leading-edge threshold. For example: nothing above −60 dB of the response's peak
before latency − 1. Or skip the clause for a module that declares a non-zero residual.

## 4. Ask: judge A6 against the module's own reference renders when no exact dry setting exists

**Why the drive has no exact dry setting.** Kiln's human ruled that the drive blends dry and wet at 2×, so a
partial mix does not comb (D-010: it had been −9.5 dB at 19.75 kHz at the default mix). So `mix 0` is the
input through the half-band, 3.503 samples late, with ADAA's droop (−2.0 dB at 20 kHz at 48 kHz).

`module.h:130–132` defines dryProbe as `out = in × gains`. The honest answer is an empty dryProbe, which makes
A4 and A6 read NOT RUN.

**What the drive's A6 failures are.** Kiln's critic checked each failing sub-check against the right reference.
Each failure is the filter, not the gain stage:

| Sub-check | Reading | Cause |
|---|---|---|
| `t1.dry` | differs by up to 1.04, even read latency-aware | no exact dry path |
| `t2.levels` | a settled constant reads 1 − 4.4e-16 | the filter's rounding. Your ramp analysis is then skipped, but the ramps are exact linear 20 ms ramps to 4e-16. |
| `t3.prepare` | the first output is 1.0e-4 × gain | the filter's start-up. `y(+6 dB) = g·y(0 dB)` holds to 2.9e-16 from sample 0. |
| `t3.load` | — | the filter's memory. The output equals a twin whose gain landed at the load, to 2.8e-17. |

Two of horde's A6 controls are partly blind on this module: `io-opens-ramped` is masked, and `io-jump` fires
only through t7.

**Ask.** For a module with no exact dry setting, judge A6 as Kiln's `core/tests/drive_gain_test.cpp` does:
- `io.outGain` at g against `g × out(0 dB)`. It is the last multiply, so this holds bit for bit.
- `io.inGain` at g against a 0 dB twin fed `in × g`.
- Each 20 ms ramp against the ideal linear ramp, applied at its place.

Judging against "the input delayed by the latency" alone would **not** be enough: `t1.dry` and `t2.levels` would
still fail.

## 5. Ask (general; it no longer bites Kiln): A11 probes a stepped Device key at a fraction

**The problem.** `harness.cpp:833` sets each Device key to `def + (max − def)/4`, and `:853` reads it back with
`!=`. For a stepped Device key that value is fractional. A module must round it, as A10's sweep expects, so the
read-back differs and A11 reports "loading a patch moved Device key". Kiln's first drive build had such a key
(`drive.quality`, since deleted, D-010).

**Ask.** Probe stepped Device keys at a whole step.

**Related.** That build also showed `moveAll` (`:183`) and `randomSettings` (`:1023`) moving a restart-time
Device key (RS-7.1) that only takes effect at the next prepare. That broke A2, A17 and A18. A future module with
an oversampling Device key (Sluice?) will meet the same thing.

## 6. What Kiln will do on your answer

When the adapters land and §3 and §4 are settled, Kiln runs your `admission_check.py` against both modules. It
records the verdicts verbatim, with your revision, in a Kiln trace. That is Q-006 criteria 3–4.

**Wanted for that:** no NOT PROVEN, and no pins. If you decide against §3 or §4, Kiln's human decides what Kiln
does next. Kiln will not weaken its own copies of the rows.
