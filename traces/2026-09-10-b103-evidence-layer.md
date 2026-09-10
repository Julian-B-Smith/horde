# b103-evidence-layer — measurements, writeup draft, README landing page

- **Queue item:** B103 (the evidence layer). Implementer slice only: measurements, the
  writeup draft, the README structure. Video, clips, screenshots, listening note left as
  marked `TODO(human)` slots — they need ears and a DAW.
- **Why:** the 1.0 definition of done (ROADMAP §"1.0 — DEFINITION OF DONE", ratified
  2026-09-10) wants "parity isn't correctness" answered in numbers before the goldens-v2
  workshop, and the audit called this "the part your instincts will tell you to skip".
- **What changed:** `tools/measure_alias.cpp` and `tools/measure_cpu.cpp` (new; standalone;
  registered in `CMakeLists.txt` beside `svf_check`; not gates; both drive the shipped plugin
  through the CLAP factory and print `HYPERSAW_BUILD_STAMP`); `docs/MEASUREMENTS.md` (the two
  tables verbatim, exact commands, build 319a758, machine as CPU model only, reading, listening
  slot); `docs/ENGINEERING.md` (marked DRAFT, four sections in the brief's order, every claim
  cites a file, B100 quoted verbatim); `README.md` top restructured as a landing page with
  `TODO(human)` slots, existing content unchanged below the rule, dated "last verified" line
  untouched (nothing below it was re-verified).
- **Evidence consulted:** ROADMAP B103/B100 and the 1.0 section; `specs/ACCEPTANCE.md` L0-1
  domain limit; `tests/feature_tests.tsv` header (RULING/ENCODING); ROADMAP §"FEATURE TEST
  TABLE"; `CLAUDE.md` charter; `DECISIONS.md` ADR-003/009/014/020/065/082/094/100; the param
  table in `src/hypersaw_clap.cpp` (ids 1/4/6/9/14/16/88/130/131/132/150 confirmed;
  `kOscStride = 1000` so osc-2 twins are 1001/1150/1017); `tools/combguard_check.cpp`,
  `tools/notefuzz_scaffold.inc`, `tools/shell_bench.cpp`, `tools/cpu_bench.cpp`,
  `tools/blep_alias_incommensurate_probe.cpp` (the FFT and midpoint protocol, reused by copy);
  `tools/build_stamp.cmake`; LIBRARY L0016/L0017/L0020/L0031/L0032; `docs/img/README.md`.
- **Detector calibration (L0016/L0032), recorded here because the numbers are only as good
  as this:** the tool's clean control (in-tool additive band-limited saw) reads a flat
  −159 dB floor at every note/rate; its naive control matches closed form (Σ_{k>K}1/k² /
  Σ_{k≤K}1/k²: −12.1 dB predicted and read at MIDI 96 / 44.1 k); the plugin's naive mode
  matches the synthetic naive saw within 0.8 dB in all 16 rows. The aliasing table is
  bit-identical across two runs; the CPU table moved ≤ 3 % between runs (timing, min of 3).
- **Findings for the lead:** (1) the ADR-094 saw-shape section adds discrete fold-back at
  MIDI 84/96 (worst midpoint −186 → −172 dB at 44.1 k, −172 → −149 dB at MIDI 96 / 96 k),
  invisible in the energy integral, mostly recovered by 2× OS; the JS reference shares it, so
  parity certifies it (L0031). (2) CPU is linear in swarm size: ≈ 0.26 ms per oscillator-second
  over a ≈ 3.3 ms/s floor; 512 oscillators = 14 % of one M3 core against the 50 % E-6 budget.
- **Alternatives rejected:** a Hann-window "energy in all non-harmonic bins" ratio (the brief's
  literal phrasing) — L0016 records it reading −27.7 dB on a clean polyBLEP saw; replaced by
  the same ratio under a Kaiser β 19 window with ±16-bin harmonic exclusion, calibrated in-tool,
  plus the tree's midpoint protocol as the second figure. Core-direct rendering — rejected
  because it skips the shell (L0031 B). Sweeping polyphony as "voice count" — param 1
  "Voices" is swarm size with range 1–32, exactly the brief's 1/4/8/16/32; polyphony
  (`kPoly` = 16) left unswept and said so.
- **Verify:** `./verify fast` exit 0 (git 319a758, 2026-09-10T13:06:01Z); `./verify full`
  exit 0 (git 319a758, 2026-09-10T13:09:07Z — `.harness/last-verify.json`), last lines
  `glide_check: GREEN (0 failures; worst parity rms 3.51308e-08)` and `time_check: GREEN
  (0 failures; worst parity rms 5.5853e-12)`. Private-name leak check SKIPPED on this Mac
  (`.leakcheck-names` absent — expected, per the gate's own message).
- **Process notes:** the fresh worktree had no submodules; `git submodule update --init
  --recursive` checked out the pinned commits from the main repo's modules (no network, no
  history change). The session scratchpad turned out to be shared with another agent (my PR
  body file was overwritten by theirs); rewritten under a worktree-unique name.
- **Open questions:** whether the saw-shape fold-back is audible — the listening note's
  question, not the table's; whether the lead wants the shape-section finding filed as an
  ADR/B-item ahead of the goldens-v2 workshop; the writeup quotes README's dated gate counts
  (31 gates / 156 scenarios, 2026-08-28) as dated and the live test-table counts
  (187 rows: 117 RULING / 70 ENCODING) as of today — the human's rewrite should refresh both.
