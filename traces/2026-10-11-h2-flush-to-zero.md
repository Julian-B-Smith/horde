# h2-flush-to-zero — the self-digests pin the engine with flush-to-zero on; eight rows re-pinned once

- **Queue item:** ROADMAP B448 (Blind-Spot Armor), the flush-to-zero ruling. ADR-209 item 6
  and ADR-211 item 1 (DECISIONS.md). ADR-211 made this "its own dispatch".
- **Why:** the human ruled flush-to-zero on in the horde 2 shell from its first build. The
  shipped build is the tested build (ADR-187 A1), so the two self-digest references must pin
  the engine with the mode on, and the tools that take the digests must switch it on with the
  code the shell will use. Eight of 543 rows differ with the mode on; ADR-211 item 1 sanctions
  re-pinning those eight, once.

## What is in this change set

| File | What changed |
|---|---|
| `h2/flush_to_zero.h` (new) | The one switch: `underFlushToZero(f)`, `withoutFlushToZero(f)`, `flushToZeroState()`. arm64 measured; x86-64 written from the documentation. |
| `tools/h2_engine_selfdigest_check.cpp` | Every replay runs inside the switch, over the whole replay. A new must-fire control, `FZ`. The header records the one re-pin that is none of its cases a to c. |
| `h2/engine/selfdigest.parity.*.txt`, `h2/engine/selfdigest.product.*.txt` | Eight rows each, and the TOTAL line that sums the rows. Nothing else. |
| `tools/h2_engine_fz_probe.cpp`, `.py` | Uses the header's switch. Its baseline is now the pinned scope (mode on); it reports the rows that differ with the mode forced off, and whether narrower scopes agree. |
| `h2/README.md` | A paragraph: the digests pin the mode-on engine; where the switch is; what is open. No pinned file was edited, so no `path@blob` pin moved. |
| `docs/armor/goldens.json` | The `pins` and `evidence` text of the two self-digest entries. |
| `docs/proposals/h2-flush-to-zero.md` (new) | Stage 1's account: every gate and its verdict, the scope, the placement, measured against argued, open questions. |

## The re-pin, and the reason on record

The references are golden fixtures and code-owned ledgers. **This edit is sanctioned by
ADR-211 item 1 and by the lead's brief of 2026-10-11, for those eight rows only.** It is none
of the tool header's three standing reasons (a ledgered divergence, a scenario-set change, a
joint JS and C++ change). The header now records it as the one exception and adds no fourth
reason. Whether it should also be a ledger entry is an open question below.

The rows, the same in both files (old digest, new digest):

| Row | Old | New |
|---|---|---|
| E/Swell · cut at the key-off :: chord | d42d1dd12fb5fe87 | 68bc87760cd77e30 |
| E/Swell · cut at the key-off :: repeat | ead0c2f0d433e182 | 27d804b6acbc4266 |
| E/Swell · cut at the key-off :: arp | db9710bef6166a53 | 085e3c3b2456a786 |
| E/Gated stab · organ :: chord | c7f438b0fb280893 | 2b2a5949f3c59255 |
| E/Gated stab · organ :: repeat | c5db030555c78adf | 61197cc1ddf49007 |
| E/Gated stab · organ :: arp | 11fe7faa79fdcacf | 6b9861b968e38438 |
| T/restrike after full release :: restrike | f5cd40047e7beead | 6d4748f47b64a8ea |
| C/VL tier 1, a faded slot | 18ace7b618eef8cc | 080426d7718177af |

TOTAL moved from `4d1ad7f1ed674d88` to `894c3bc4b62bcab0` in both. `git diff --numstat` on
each file: 9 lines added, 9 removed (the eight rows and TOTAL).

How it was made: each tool's own `--repin` on the full 543-scenario stream. Each printed
`REPIN ... 8 changed, 0 added, 0 removed, 535 unchanged` and named the eight. `--repin` also
rewrites the file's `# ORACLE` header lines from the stream, and one of them was stale
(`docs/design/scalpel-interface-lab.html`, `6abf848e` in the file against `77ccee77` in the
stream and in `h2/README.md`). That line was put back as it is on the base branch, because
only the eight rows are sanctioned. It was stale before this change and is as stale after.

## Evidence consulted

- DECISIONS.md: ADR-187 with A1 and A2, ADR-209, ADR-211. `h2/README.md`.
  `docs/proposals/h2-shell-acceptance.md` row OG-4. `verify`'s `full()` lines for the engine.
- `tools/h2_engine_fz_probe.cpp` and `.py`, `tools/h2_engine_selfdigest_check.cpp`,
  `tools/h2_engine_parity_check.cpp`, `tools/h2_engine_stream.h`,
  `tools/h2_engine_hooked_replay.h`, `tools/h2_rt_lint_check.py`, `tools/h2_rules_check.py`,
  `tools/engine_enrolment_check.py`, `tools/golden_registry_check.py`,
  `tools/golden_pin_check.py`, `tools/weakening_check.py`, `tools/h2_libm_count.py`,
  `tools/labharness/divergence_ledger_check.mjs`.
- Prior trace: `traces/2026-10-10-armor2-p16-rt-probe.md` (the measurement this follows).

## Measured (dev Mac, darwin-arm64, Apple clang 16, Node 24, 2026-10-11)

- Before any edit: both self-digest tools, the parity check and its FMA control green on the
  branch's base.
- The parity check with the mode on around each of its 12 replay calls (a scratch copy, not
  committed): GREEN, 540 of 543 at parity, 0 red, mean bit-exact share 37.00%. Only the eight
  rows' bit-exact shares move, by 0.29 to 1.42 points. The same wrapper with the bit left
  alone reproduces the stock output line for line. The FMA control with the mode on: still
  fires, 24 of 543 rows miss parity.
- `h2_scalpel_parity_check` and its FMA control, `h2_swarm_parity_check`, `h2_swarm48_check`,
  each with the mode on for the whole thread (scratch): verdicts unchanged.
- The updated probe, both builds: the pinned scope reproduces 543 of 543 pinned rows; with the
  mode forced off 8 rows differ, the eight above, largest sample difference 4.09e-308, none in
  events or readouts only; the render scope and the calls scope differ on 0 rows.
- Two plants of the self-digest tool (scratch, not committed), against the new references:
  - the replays switched to forced-off: both builds RED, exactly the eight rows, TOTAL back at
    the old `4d1ad7f1ed674d88`, and `control FZ` RED ("0 of 543 clean replays ran with the
    bits set");
  - the header's setter made to do nothing: `control FZ` RED ("off inside the switch"), and
    the eight rows RED.
- The header as an x86-64 binary under Rosetta 2: bits read back, results and inputs flush,
  the word is restored. A translator, not an x86 CPU.

## Alternatives rejected

- **Pin a narrower scope** (inside render only, or around each engine call). They give the
  same bits today, but need a second replay path in a gate and cannot plant the FAULT
  control's fault. The whole-replay scope uses the one replay unedited.
- **Take the tool's `--repin` output whole**, including the refreshed ORACLE header line. That
  is a change to a ledger beyond the eight sanctioned rows.
- **Put the switch in `h2/engine/`, in `tools/`, or in a new directory under `h2/`.** See the
  proposal, section 6. A new directory needs an entry in `docs/armor/engines.json`, an owned
  ledger outside this brief.
- **A full forced-off replay inside the gate on every run.** On the keyed platform the pin
  itself fails if the mode is off, and `control FZ` covers every platform without a second
  render. The probe keeps the full measurement.

## Verify

- `./verify fast`: exit 0, on git `c7dc9ab` (the base, PR #1054's branch) plus this change
  set, staged (`.harness/last-verify.json`).
- The engine's `full()` lines, run by hand on the same tree and one render of the stream:
  - TRUNC: the parity check on a stream cut at 8 MB exits 1 with a `FAIL  STREAM` row; the
    FMA control on it exits 2.
  - `h2_engine_parity_check`: exit 0, "GREEN — 540/543 scenarios at parity (rms < 1e-06, max
    < 1e-06, events and readouts identical), 3 excluded as chaotic (onset window 384 frames,
    events, readouts, bounded and finite), 0 miss parity, 0 red"; mean bit-exact share 37.00%.
  - `parity_floor_check.py --stream`: exit 0, "GREEN (h2_engine_parity_check: 543 scenarios;
    4 controls red; the untouched stream is the gate's own green run)".
  - `h2_engine_fma_control`: exit 0, "FIRED ... 24 of 543 scenarios miss parity".
  - `h2_engine_selfdigest_check`: exit 0, "GREEN — parity build, 543 scenarios against
    darwin-arm64.appleclang-16.node24; 0 differ; 0 red"; COVERAGE, FZ, ULP, KEYSELF and FAULT
    all PASS; TOTAL `894c3bc4b62bcab0`.
  - `h2_engine_selfdigest_product`: exit 0, "GREEN — product build, 543 scenarios ...; 0
    differ; 0 red"; FZ, ULP and KEYSELF PASS; 543 of 543 rows share the parity digest.
- `./verify full` as a whole was NOT run by this dispatch (the brief leaves it to the lead).

## After the push: what CI's first run showed (run 38165894374, 2026-10-11)

Added in a second commit, with the wording about x86-64 in the header, the proposal and
`h2/README.md` brought in line. No code changed in that commit (one code comment did).

- **Linux x86-64, gcc 13, both sanitizer lanes:** `h2_engine_selfdigest_check: SKIP — unkeyed
  (linux-x86_64.gcc-13.node22), controls fired` and the same for `h2_engine_selfdigest_product`.
  The tool prints "controls fired" only when every control fired, `FZ` among them. This is
  the switch's first run on x86 hardware. The control's own line is not in the log (the lane
  prints a tool's full output only when it fails).
- **Windows, the Microsoft compiler:** both self-digest tools built. They are not run there.
- **Both sanitizer lanes ended red**, for a reason outside this change: `h2_harness_render`
  run with no arguments exits 2 ("usage: ..."). The base branch fixed that in a later commit
  (`2c6903b`), which arrived after this branch was cut; the brief says not to rebase or merge.
- `verify-fast` passed on both workflows.
- The second commit's own verify: `./verify fast` exit 0 on git `305ef70` plus that commit,
  staged; both self-digest tools rebuilt and re-run by hand, GREEN, 0 differ, `control FZ` PASS.

## Open questions

All are in `docs/proposals/h2-flush-to-zero.md` section 8. In short:

1. Is flush-to-zero a ledgered divergence (`js_limit`), or a recorded exception? The human's.
2. Should the parity check (or the shared replay) set the mode too? Today the parity gate
   judges the mode-off engine and the pins are mode-on; the link is the probe, which no gate
   runs.
3. The engine harness renders with the mode off.
4. `h2/flush_to_zero.h` sits directly in `h2/`, where `engine_enrolment_check` does not look.
5. The stale ORACLE header line in both references.
6. On Windows the switch is compiled and has never run (see "After the push").
