# 2026-11-24 — B710: Scape's reverb after the engine in the test plug-in, test-only (ADR-408)

- **Queue item:** B710 (ADR-408; the human, 2026-11-24: "start FX now with Scape in its own session"). Built by the B710
  worker, an agent of the lead's session, in its own worktree; the lead reviews and records.
- **Why:** the first audible FX module, now, after the engine's stereo output (RC-5's hand-off), as B660 placed Weir's
  filter (ADR-311). Not the rack: slots, `fx/N` morphing, macros and the limiter stay with B398.

**Admission first, at the pin** (Scape `8350fb8`, its `main` since PR #82; the commit its notice seq 8 asks for).
`HORDE_SCAPE_DIR=<a git archive export of 8350fb8> HORDE_SCAPE_REV=8350fb8 nice -n 10 python3 tools/admission_check.py --module scape`,
on horde `6d0d6a15`, verbatim:

```
== admission_check: Scape at 8350fb8 (horde 6d0d6a15); A19 figure 4096 B per corner counting patch-text (ruled, ADR-401, tools/admission/state_budgets.json)
   A1   PASS        Declarations               controls: tail-undeclared fired, class-missing fired, json-class-removed fired
   A2   PASS        Reset identity             controls: reset-unsnapped fired
   A3   PASS        Block independence         controls: ramp-per-block fired
   A4   PASS        Latency truth              controls: latency-short fired, pre-echo fired
   A5   PASS        Silence and tails          controls: denormal-residue fired, tail-short fired, silence-leak fired
   A6   PASS        I/O gain, tests 1-3 and 7  controls: io-not-unity fired, io-jump fired, io-opens-ramped fired, latch-self-clears fired
   A8   PASS        Stereo image               controls: mono-sum fired
   A9   PASS        Non-finite                 controls: guard-zero-uncounted fired
   A10  PASS        Real-time                  controls: malloc-in-process fired
   A11  PASS        Parameter classes          controls: flip-crossfade-removed fired, flip-blended fired, device-in-preset fired, morph-landed fired
   A12  PASS        Macros                     controls: macro-reorder fired
   A15  PASS        Determinism                controls: shared-detector fired
   A17  PASS        State completeness         controls: state-drops-param fired
   A18  PASS        Restore exactness          controls: lossy-float fired
   A19  PASS        State size                 controls: oversize-copied fired, budget-over-figure fired, patch-text-over fired, preset-text-over fired, no-figure fired, no-measure fired
   A20  PASS        Live-state exclusion       controls: latch-in-state fired
   A21  N/A         Morph position             controls: -
   A6 tests 4-6 SLOT        I/O gain at the slot       waits for horde 2's shell (B398)
   A7   SLOT        Off                        waits for horde 2's shell (B398)
   A13  SLOT        Corner presets             waits for horde 2's shell (B398)
   A14  SLOT        Feedback                   waits for horde 2's shell (B398)
   A16  SLOT        Sleep and wake             waits for horde 2's shell (B398)
   A1 note: OperatorProperties at 48k: latency 0, tail 1210560 (may_change), min_block 1, LoadDecl 0
   A1 note: 29 parameters: 25 morph, 2 flip, 2 Device; changes_image true, changes_level true; morphPosition not declared
   A1 note: memory per instance (RS-2.6, the module's figure): 12.2 MiB to 27.2 MiB over 6 rates
   A4 note: causality: the loudest sample before latency (no early residual declared) reads absent over the worst patch (threshold -60 dB, ADR-351/ADR-402)
   A8 note: changes_image declared: the rule is met by declaration; lowest side level -17.3864 dB re the input's (all-wet-hall.scape-patch)
   A10 note: REPORTED (Layer-E, not judged): the first 128-sample block after a reset() whose parameters moved: min 0.904042 ms, median 1.15896 ms, max 2.76679 ms over 21; a steady block: median 0.068334 ms; the block lasts 2.90249 ms at 44.1k
   A11 note: click reading (REPORTED, ADR-218), verb.era over 20 patches x 2 probes x 2 directions: worst 5.79886 dB; ideal equal-gain crossfade worst 4.54771 dB; largest excess over its ideal 5.79886 dB (vintage-wash.scape-patch, 1 kHz, max->min)
   A19 note: ruled figure 4096 bytes per corner, counting patch-text; module budget 2048 bytes; largest factory patch text 655 bytes; largest factory state 664 bytes; worst-case state 912 bytes
   A19 note: the worst case here is a STATE; a worst-case patch text (every binding, the longest names and labels) is not built here: it is bounded at creation (B702) and by the module's parser refusing text over the figure (ADR-401's notices)
   report: build-admission/scape/admission-report.md
== admission_check: GREEN (every module-only row judged or reported; every control fired) [Scape]
```

It agrees with the lead's re-run of 2026-11-16 at the same commit (Scape's mailbox, `integrations/hypersaw/response-vintage-jitter.md`:
A11's worst click reading 5.80 dB, reported only). Nothing in Scape's tree was written; nothing needed a brief.

**What changed** (all in `tools/engine-preview/`):
- `scape.pin` (`8350fb8a…`); CMakeLists refuses any other HEAD at `HORDE_SCAPE_DIR` (default `$HOME/...`, never an
  absolute path in a tracked file) or any edit or untracked file in its `core/`, then adds Scape's own
  `core/CMakeLists.txt` as tools/admission does (EXCLUDE_FROM_ALL, binary dir in the build) and checks that `scape_core`
  keeps Scape's `-ffp-contract=fast`.
- `reverb_core.h` (pure: ids 0xA00016 "Scape on", 0xA00017 + Scape's stable id for its 29 parameters; the `scape`
  state lines, only differences from Scape's defaults); `post_reverb.h` (Scape's `FxModule` in a stereo float wrapper;
  every range, default, unit and enum name read from Scape's table, the count held to `scape::kParamCount` by a
  static_assert).
- `preview_plugin.cpp`: thirty host parameters after the filter's six, off by default; Scape after the filter in the
  render path and in the idle skip; a state version 6 written only when Scape differs from its defaults;
  the output guard factored into `guard_output`, also run on an idle block the reverb wrote.
- **Idle path:** with zeros in, Scape runs until its own declared tail (`FxModule::tail()`) has passed since the input
  last sounded, then is skipped; bridge_test shows the skip lands where the output was already exact zero.
- **Deployment target 11.0 → 13.3** for the preview: Scape's `numtext.h` uses `std::to_chars` for double, which libc++
  offers from macOS 13.3 (the first build stopped there). Raising only scape_core's would leave a bundle claiming 11.0.
- Three plants, one fault each: `preview-plant-noreverb` (no Scape compiled or linked: the bit-identity reference),
  `preview-plant-reverb-deaf` (values never reach Scape), `preview-plant-reverb-idle` (the idle skip cuts the reverb).
  `build.sh` builds them and passes them to smoke; bridge_test links Scape for the wrapper's DSP checks.
- README: the test reverb's section and the oracles.

**Alternatives rejected:** a curated subset of Scape's parameters (the brief says "its settings"; all 29 under Scape's
own keys keeps every range Scape's); Scape before the filter (ADR-408 says only "after the engine"; the reverb last, so it hears what the filter made, is the usual order and the lead can overrule it);
an output-is-zero early skip (a pre-delay can hold signal while the output reads zero; the declared tail is Scape's
own bound); a per-target 13.3 for scape_core only (above).

**A threshold set from a measurement, before commit:** bridge_test's first run (build 12) failed my own guess that
Scape's default tail sits within 30 dB of the burst; it measured -37 dB (mix 0.35, decay 2.2 s). The bound is now
-50 dB, with the control that the input is exactly silent in that window, so any energy there is the reverb's.

**Verify:** see the PR body for the verbatim lines (`build.sh 6cd98b5`, build 13, GUI 3's page from
`tools/gui3_inline.py`, Weir cloned at `078f1436`, Scape cloned at `8350fb8`, not installed) and `./verify fast`.

**Open:** installed and heard by the human (B710's last criterion; `build.sh --install` is theirs to run). A Scape
panel on GUI 3's page is the GUI session's (ADR-336). The committed GUI frame fixtures are from `1ad270a`, unchanged.

## The human's rulings, and the lead's finish (2026-11-24)

The worker's push was refused on `tolerance_registry_check` alone (four new inline compares), and two of its choices
reached past the test (the preview's minimum macOS 11.0 → 13.3, Scape's std::to_chars; 12 to 27 MiB at activation).
The lead put both to the human by poll: **"Approve, smoke at −40 dB"** and **"OK for the test build"**. The lead set
smoke's tail bound to −40 dB of the note's energy (`1e-4`, from `1e-3`), registered the four entries with
`tools/tolerance_registry_check.py --approve` naming that ruling (158 registered, 0 changed), and rebuilt the preview
on the same pins (Weir `078f1436`, Scape `8350fb8`, GUI 3's page, not installed): `bridge_test: GREEN (0 failed)`,
`engine-preview smoke: GREEN (0 failed)` (Blade pluck's tail 0.1966 against the note's 122.8, the idle-cut build 0;
the control fires), `engine-preview gui: GREEN (0 failed)`.

