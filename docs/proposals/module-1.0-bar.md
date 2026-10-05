# The module 1.0 bar — the minimum every module meets to ship in horde 1.0 (B439)

**Status: RATIFIED by the human, 2026-10-04** ("bar ratified, propose a CPU budget"). The CPU budget below is PROPOSED. The human asked for
it: "Let's also set a standard for where each module needs to be to be ready for 1.0. We can
keep improving them independently beyond that, but let's set the minimum."

**Scope.** Every module horde ships in 1.0: the sources (the swarm and SCALPEL blades, the Sub),
the FX (MAW, Sluice, Scape, ECHO, the simpler FX-lab modules, OTT if it makes it), Bulwark's
master limiter, and the modulators. A sibling project (Scape, Bulwark, Sluice, MAW) meets the
bar in its own repo. horde checks it at admission, the moment the module is hosted in
horde's rack.

**The rule.** A module is in 1.0 only if every MUST row below holds by the feature freeze.
A module that misses the freeze does not ship in 1.0. This is the OTT rule the human gave:
"if its construction keeps up with other modules and it can be shipped in time". It keeps
improving after 1.0, on its own schedule. SHOULD rows are expected but may ship as known
gaps, listed in the release notes.

## 1. It is defined

| | requirement | evidence |
|---|---|---|
| MUST | A **scope list**: the controls and modes that ship, and what is post-1.0. Frozen at the feature freeze. | a scope section in the module's spec or README |
| MUST | **Declared in a manifest**: every parameter's key, unit, range, default, morph class and rate class (the B376 / FOUNDATIONS engine-manifest shape). Keys frozen from first release (B428). | the manifest, plus the lockfile diff gate |

## 2. It plugs in

| | requirement | evidence |
|---|---|---|
| MUST | **The rack slot contract** (B50 / B281): admission, bypass, the "off" state, latency and tail reporting (B429). | a rack admission test |
| MUST | **The I/O gain standard** (B435 + A1): `io.inGain` / `io.outGain` with the law, plus pre and post meter taps with clip latches. | the standard's acceptance tests 1–7 |
| MUST | **Macros, where the module offers them** (ADR-169 A4, 2026-10-05; replaces the retired four-role face): up to 8 macros, ordered, carrying the preset's own labels and optional curves (A3), round-tripping with presets. Nothing in the module is driven by horde's intents unless a preset's designer binds it. Modules with deep internals (Sluice, Shriek) are expected to offer macros so presets stay usable without the advanced controls. | a macro round-trip test |
| MUST | **Morph and presets** (ADR-188): the module's state is per corner; patches transfer as horde FX-slot presets; Device-class controls never morph. | a corner-flip test, plus preset save and load |

## 3. It is correct

| | requirement | evidence |
|---|---|---|
| MUST | **Deterministic.** Same input and seed give identical output. Seeded mulberry32 only, no wall clock. | a determinism check in the module's verify |
| MUST | **Sample-rate and block-size independent** across 44.1, 48, 96 and 192 kHz and blocks 1 … 4096, within a stated tolerance. | SR and block checks |
| MUST | **Real-time safe.** No allocation, locks or syscalls on the audio thread; FTZ/DAZ; finite output under NaN/Inf injection (B430). | an RT probe, plus a non-finite injection control |
| MUST | **Its own fidelity suite** with measured thresholds, chosen for what the module claims. A reverb proves RT60 against its setting; dynamics prove its transfer curve and attack/release times; a delay proves its time accuracy; a saturator proves its aliasing ceiling. Every detector has a must-fail control. | the module's oracle, green |
| MUST | **Silence in gives silence out**, and tails decay to exact zero (no denormal residue). | a check |
| MUST | **Mono-safe**, if it is stereo: the B408 metrics are measured and stated, with no unintended cancellation at width 0. | the stereo report |
| SHOULD | **The C++ matches its lab** within the parity tolerance, or a ruling has demoted the lab and the goldens are frozen (ADR-187). | a parity check, or the goldens |

## 4. It is affordable

| | requirement | evidence |
|---|---|---|
| MUST | **A measured CPU cost** in Release, as a ratio to the calibration loop (B262 / B236), at its default and worst settings. It fits the per-module budget the human sets for 1.0. | a cost-bench row |
| SHOULD | **A memory figure** per instance, worst case included. | measured |

## 5. It is usable

| | requirement | evidence |
|---|---|---|
| MUST | **A face and an expanded view** in GUI 3's design system, keyboard-reachable, with every control named for the host (B432). | the GUI review |
| MUST | **Undo** covers every control (B389). | an undo test |
| MUST | **Factory presets**: at least an init plus a small set that shows the module's range. The size is set by the presets lab (B395). | presets in the bank |
| SHOULD | **A manual page**: what each control does, in plain words (B413). | the page |

## 6. It is signed off

| | requirement | evidence |
|---|---|---|
| MUST | **An independent critic review** of the module's port and its oracles, sourced separately from the author, with no open HIGH findings. | the review |
| MUST | **The human's listening sign-off** on the module, on its factory presets and at least one stress case (maximum feedback, extreme settings). | recorded in ROADMAP |
| MUST | **Its name cleared** (B433): trademark clearance for any user-visible module name. | the clearance memo |

## What the bar is NOT

- Not a quality ceiling. Each module keeps improving after 1.0, independently.
- Not a feature list. Each module's scope list (row 1) says what it ships with.
- Not new machinery. Every row reuses an existing contract, check or row. The bar only
  collects them into one admission list.

## Appendix: CPU budget (PROPOSED 2026-10-04, for the human)

**The anchor is our own ratified envelope** (specs/ACCEPTANCE.md E-6). A patch holds
**< 50 % of one core on min-spec**: Apple M1 base, or a 4-core 2018-class Intel ultrabook;
Windows x64 AVX2; 44.1 kHz at a 128-sample buffer; 8-voice poly. The budget splits that
50 %, so that any 1.0 patch fits however its modules are combined.

| Slice of the 50 % | Budget on min-spec | What lives there |
|---|---|---|
| Sources: the swarm, blades, Sub, 8 voices | **≤ 34 %** | the composed engine |
| FX: both racks together | **≤ 12 %** | every hosted FX module |
| Modulation, morph, intent bus | **≤ 2 %** | the modulators |
| Master strip and mixer meters | **≤ 1 %** | the master limiter and the meter taps |
| GUI with the editor closed | **0 %** | feed gating (B432) |
| Headroom | 1 % | |

**Per FX module, one instance at its defaults:** ≤ 2 % of min-spec. At its worst settings:
≤ 4 %. This is what the bar's "fits the per-module budget" means.
- **Oversampling exception:** a module whose quality needs oversampling (MAW; any saturator)
  may use up to ≤ 6 % at default and ≤ 10 % at worst, but ONLY if it also offers a
  lower-oversampling mode within the standard 2 % / 4 %, and its defaults in factory presets
  stay within 2 %. A user may choose the expensive mode; a preset never imposes it.
- **Master limiter:** ≤ 0.5 %. **Each modulator:** ≤ 0.25 %.

**How it is measured.** Use the B262 / B236 pattern: Release build, ratio to the calibration
loop, at the module's default and worst settings. Measurement happens on this Mac (Apple M3).
To convert to min-spec, use a factor of **1.5** (M1 base single-core ≈ M3 ÷ 1.5). This factor
is an ASSUMPTION until it is measured once on an M1 or the Intel min-spec, so it is labelled
in every cost row. CPU stays Layer-E: measured and reported, never a verify gate (ADR-187 §8).

**What today's numbers say** (B262; docs/port/h2-engine.md, checkpoint 4):
- **Today's FX are well inside budget.** The heaviest legacy rack module is Room, at 0.67 %
  worst. MAW is projected at ~9.7 % per instance at 3 stages and 8× oversampling, which is
  inside the oversampling exception only with a cheaper mode.
- **The engine is the real risk, not the modules.** The composed engine measures 5.7–8.9 %
  of an M3 core per voice on heavy patches (Crushed bells, Glass horde pad). That is
  ~45–70 % for 8 voices on this Mac, and more on min-spec, above the 34 % slice. The B378
  output-neutral kernel work, and a polyphony/quality policy for heavy patches, must close
  that gap before 1.0. This is flagged as the critical CPU item.
