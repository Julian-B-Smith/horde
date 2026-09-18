# b148-ncap-bitident — cap `n` at the core, plus the audit's three bit-identical optimisations

- **Queue item:** B148 (ROADMAP row on `lead-records-38`), bundling audit
  items O1 (cap `n`), O2 (`RN`), O3 (law-0 `pow` cache), O4 (`rebuild()` on
  change). Motivating record: `docs/audits/2026-09-18-saw-engine-audit.md`
  §1.2, §2.2, §2.5 and the O1–O4 rows of §5.
- **Why:** the core had no cap on `n` (`kMaxV` = 32 is the extent of `x[]`,
  `panL[]`, `panR[]`, `panBase[]`, `itdSamp[]` and every per-oscillator Voice
  buffer, while `p.n` is a public double). The shell's param row was the only
  cap in the system and every tool in `tools/` drives the core under it —
  `setParam("n",33)` wrote one double past `x[32]` into `panL[0]` and rendered
  the corruption silently; `n >= 40` segfaulted. That is a correctness defect,
  and the audit's three proven-bit-identical CPU items ride the same file, so
  they land in one PR against one bit-identity witness.

## Evidence consulted

- `docs/audits/2026-09-18-saw-engine-audit.md` (branch `lead-records-38`),
  headline A2/A4/A8, §1.2, §2.2, §2.4, §2.5, §5 O1–O4.
- ROADMAP rows B147 / B148 / B149 / B150 (same branch).
- `src/swarm_core.h` — the five `const int n = (int)p.n` reads, `setParam`'s
  rebuild trigger list, `rebuild()`/`finishRebuild()`'s complete `p` read set,
  `focus()`, the RN block in `controlTick`, the law-0 `pow`.
- `src/hypersaw_clap.cpp:3281` (viz snapshot), `:6150` (`hypersaw_debug_viz`),
  `:6162` (`hypersaw_debug_phases`), `:5485-5488` (`plug_activate`'s reseat).
- `tools/trajectory_check.cpp`, `tools/bank_check.cpp:208` — the RN consumers.
- `tools/polarity_check.cpp` + its CMake block, as the standalone-target model.

## What changed

1. **O1 — `src/swarm_core.h`.** One private `voiceCount()` =
   `std::min(kMaxV, std::max(1, (int)p.n))` at the five `const int n` reads,
   plus a clamp (NOT a truncation — 7.5 stays 7.5) in `setParam` so
   `getParam("n")` and the state chunk report the size that actually runs.
2. **O2 — `src/swarm_core.h`.** `RN` computed on the segment's FINAL control
   tick instead of every tick.
3. **O3 — `src/swarm_core.h`.** Law-0's `pow(2, xv*dep*100/1200)` cached per
   `(rebuildGen, dep, anchor, n)`, invalidated by VALUE (not by `setParam`,
   because `p` is public).
4. **O4 — `src/swarm_core.h`.** `rebuild()` skipped when `rebuild()`'s own
   twelve inputs are bit-unchanged, loaded through one helper that IS the
   trigger list.
5. **`tools/ncap_check.cpp` + one `CMakeLists.txt` target.** Standalone, NOT
   wired into `./verify` (wiring a gate is the human's ruling — charter).

## Alternatives rejected

- **O2 as the audit proposed it — RN for the focus voice only.** Built,
  measured, rejected: `focus()` at read time is not `focus()` at tick time. An
  adversarial probe forcing eight BACKWARD handovers (a held note regaining
  focus when a newer stab dies) makes the focus-only variant report the held
  voice's RN from the last block it HAD focus — handover hash
  `0bfa58f2a09f1eb3` baseline vs `cb91e56c96cdf5d6` focus-only, at n = 1/7/32.
  The last-tick form is exact instead (RN is write-only inside the core, so
  only the last write per `render()` is ever observable) and still takes 7/8
  of the win at a 128-frame block, 63/64 at 1024.
- **O4 keyed on "did this `setParam` move its own slot".** Written first,
  committed, and RED: `plug_activate` reseats a core by assigning the whole
  `Params` struct and then calling `setParam("seed", saved.seed)` purely to
  force the rebuild. The slot-delta form dropped that call and every restored
  session rendered with the DEFAULT distribution — `statefix_check` RED on all
  three fixtures, max |diff| 0.29–0.31 at frame 0. Replaced by keying on
  `rebuild()`'s real inputs; the commit was amended so no red commit ships.
- **`exp2` for `pow`, and double bus accumulation** — the audit already
  measured and rejected both; not attempted.
- **Making `rebuild()` public** so `plug_activate` could call it directly
  instead of the same-value-`setParam` idiom — a public-interface change, and
  `src/hypersaw_clap.cpp` is out of this brief's scope. Left for the lead.

## Bit-identity witness

A 164-item corpus, hashed FNV-1a-64 over (a) the interleaved float32 audio and
(b) the focus voice's `(R, RN, psi)` sampled once per block:

- **A, 156 items** — every parity scenario the golden generator drives
  (`build-golden/manifest.tsv`), replayed under `parity_check`'s exact
  protocol (A3 = midi 57, 4 s, 1024-frame blocks, note-off before the first
  block at >= 3 s).
- **B, 8 items** — 8 notes x n in {1,7,16,32} x law in {0,4}, 128-frame
  blocks, with mid-render changes to detune / anchor / spread / width / n, a
  no-op `setParam("n", same)` and a real n change.

**All 164 hashes are byte-identical to `origin/main` after every one of the
four items.** Corpus B tail, for the record (unchanged throughout):

| item | audio | obs |
|---|---|---|
| B/chord8.law0.n1 | 3a5f479893eb7beb | 5ff8d3780edd65c6 |
| B/chord8.law0.n7 | 00cb594b386a20d8 | 6c80dd70fb4af529 |
| B/chord8.law0.n16 | 2524a6c0fea71fcd | 0b64a8594978154c |
| B/chord8.law0.n32 | 26116fba92003eb4 | fd7bbd6c35d9dd1a |
| B/chord8.law4.n1 | 3a5f479893eb7beb | 5ff8d3780edd65c6 |
| B/chord8.law4.n7 | 932535eed89c2068 | 88d6ec7494343d04 |
| B/chord8.law4.n16 | 561625acee49a23a | 8ba53176c4bc31e1 |
| B/chord8.law4.n32 | 6f4798e0afa7afc2 | 6c48f4fbcc55d2a9 |

The three state fixtures are witnessed by `statefix_check` itself, which
already asserts a bit-identical render against the frozen `.f32` goldens —
a stronger claim than a hash I compute myself, and the gate that caught the
first O4 form.

Three further probes, each run against `origin/main` and against the branch,
each identical, each with a must-fire plant:

| probe | what it forces | plant that fires |
|---|---|---|
| handover | 8 backward focus handovers per case, n {1,7,32} x release {0.02,0.16} | focus-only RN (all n) |
| chunk | blocks {1,3,7,15,16,17,31,64,127,333,1024}, incl. segments straddling no tick | — (boundary probe for O2's `lastTick`) |
| lawcache | every O3 key driven mid-render; detune/spread/anchor driven BOTH via `setParam` and by DIRECT writes to `p`; seed/dist rebuilds that move `x[]` without moving a key; n down and back; dist {1,2,4} x n {1,7,32} | dropping the two value keys (fires on every n > 1 row) |
| rebuildskip | 128 no-op writes/block across all twelve trigger keys + real changes, n {1,7,32} x panLayout {0,1}, PLUS a core-direct twin of `plug_activate`'s reseat idiom | `if (false) rebuild()` (all 8 rows); the slot-delta form (ONLY the 2 reseat rows) |

**Recorded coverage boundaries (L0033).** The lawcache plant does not fire on
the n = 1 rows — at n = 1, `x[0] = 0` and `anchor*xmin = 0`, so the cached
ratio is 1 whatever the key. And the rebuildskip storm rows alone do NOT
distinguish the correct O4 from the slot-delta form; only the reseat row does,
which is why it exists.

## `ncap_check` and its sanitizer witness

`tools/ncap_check.cpp` asserts EQUALITY, not absence-of-crash: `setParam("n",
33)`, `setParam("n", 200)` and a direct `p.n = 200` write all render exactly
the n = 32 hash; `setParam("n", 0)` renders exactly n = 1; `getParam("n")`
reports the clamped size; a legal fractional n (7.5) is passed through
untouched (a pinned refusal — the setter clamps, it does not truncate, which
is what keeps the goldens bit-identical). Two must-DIFFER controls (31 != 32,
2 != 1) keep the equalities from being vacuous.

```
ncap_check — n clamped to [1, 32] at the core (B148)
  reference hashes: n=32 77c341020f5eb6f3  n=31 b9cd6c0ecadc1a61  n=1 3a087b9f2965f7e1  n=2 1c5665e006576606
  ok    T1 setParam(n,33)  renders exactly n=32
  ok    T2 setParam(n,200) renders exactly n=32
  ok    T3 setParam(n,0)   renders exactly n=1
  ok    T4 p.n = 200 written DIRECTLY renders exactly n=32
  readback: n=200 -> 32.0000   n=-5 -> 1.0000   n=7.5 -> 7.5000
  ok    T5 getParam('n') reports the clamped size, not the asked one
  ok    T5b a legal fractional n (7.5) is passed through untouched
  ok    T6 CONTROL n=31 differs from n=32 (the equalities above are not vacuous)
  ok    T7 CONTROL n=2 differs from n=1 (ditto at the low end)
ncap_check: PASS
```

**The brief asked for ASan as the second witness. It is UBSan, for two
independent reasons, and this is a finding about the machine, not a shortcut.**

1. The x[32] -> panL[0] write is INTRA-object. ASan does not instrument
   member-to-member overflow inside a struct at all; UBSan's array-bounds sees
   the static extent and does.
2. **The ASan runtime does not start on this machine.** `clang++
   -fsanitize=address` on a four-line `int a[4]; a[5]` smoke test:

```
AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)" (0x0, 0x0) (tid=10039164)
    <empty stack>
```

   Apple clang 16.0.0 (clang-1600.0.26.6), arm64-apple-darwin25.6.0.
   `MallocNanoZone=0` and `ASAN_OPTIONS=detect_leaks=0` do not change it. So an
   absent ASan report here would have been evidence about the sanitizer, not
   about the code — exactly the must-fire-control failure L0032 warns about.
   The CI `sanitize` job (`tools/sanitize_oracles.sh address,undefined`) is
   unaffected; it runs elsewhere.

UBSan, both arms, against a scratch copy of `swarm_core.h` with both clamps
planted out and against the shipped header:

```
PLANTED, -fsanitize=undefined -fno-sanitize-recover=all, exit 134:
plantsrc/swarm_core.h:1322:7: runtime error: index 32 out of bounds for type 'double[32]'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior plantsrc/swarm_core.h:1322:7 in

SHIPPED header, same flags, exit 0:
ncap_check: PASS
```

(`plantsrc/swarm_core.h:1322` is `x[i] = xv;` in `rebuild()`.)

## CPU

Core-direct, 44.1 kHz, 2 s, min of 5 passes, best of 3 interleaved A/B runs
(interleaved deliberately: a first non-interleaved O3 pass read **+8 %**, which
was thermal drift — the same trap the audit hit measuring `exp2`). Apple M3.
`origin/main` vs this branch, all four items, 128-frame blocks:

| case | origin/main | branch | delta | audit predicted |
|---|---|---|---|---|
| 8 notes x n=7 | 435.2 ns/sample | 410.0 | **−5.8 %** | O2 −6.6 % |
| 8 notes x n=32 | 1744.8 | 1605.3 | **−8.0 %** | O2 −7.4 %, O3 −2.0 % |
| 16 notes x n=32 | 3656.3 | 3382.0 | **−7.5 %** | O2 −7.3 % |
| 8x32 + 128 no-op events/block | 2249.3 | 1637.1 | **−27.2 %** | O4 −65 us/block |

Automation overhead falls from **+28.9 %** of the block's own cost to
**+2.0 %**; 61.3 us/block saved at 128 frames, against the audit's
−65 us/block. Per-item, isolated: O2 −5.4 to −6.6 % (−6.6 % at 1024-frame
blocks, where 63/64 of the win is available), O3 −1.6 to −2.8 %, O4 nil
outside automation by construction. **Every number agrees with the audit's
within the run-to-run noise it declared (±12 %).**

## Verify

`./verify full`, exit **0**, git `448c2f9` (`.harness/last-verify.json`).
`parity_check: 156/156 scenarios within eps=1e-06 (worst 4.262e-09 @
dyn-ring.seed42)`; `trajectory_check: GREEN`; `statefix_check: GREEN (3
fixtures, 0 failures)`; `bank_check: 0 failure(s)`; `rtsafety_probe: GREEN
(audio thread is allocation-free)`; `include_check: GREEN (99 files)`;
`subdiv_check`, `samplerate_check`, `waveshape_check`, `notefuzz_check` all
GREEN. No golden regenerated into the tree, no fixture changed (`git status`
shows only `src/swarm_core.h`, `tools/ncap_check.cpp`, `CMakeLists.txt`).

`./verify fast` exit 0 at each of the four commits.

## Open questions

1. **`ncap_check` is not wired into `./verify`.** Standing charter ruling —
   wiring a gate is the human's decision, proposed here. Without it the cap is
   pinned by a binary nobody runs.
2. **The SHELL's own `(int)core.p.n` reads are still unclamped.**
   `src/hypersaw_clap.cpp:6162` does `std::min(cap, (int)core.p.n)` and then
   reads `s->phase[i]` for `i < n` against `phase[kMaxV]`; `:3184`, `:3278`,
   `:6147` report it. In the shipped plugin `p.n` comes from the 1..32 param
   row so none is reachable today, but the defect class the audit found is
   "the core trusted a cap that lived somewhere else". Out of this brief's
   scope (only the viz snapshot site was in scope) — flagged for the lead.
3. **`plug_activate` forces a rebuild with a same-value `setParam`**
   (`hypersaw_clap.cpp:5488`). It works, and O4 is keyed so that it keeps
   working, but the honest fix is a public `rebuild()` (or a named
   `reseat()`), which is a public-interface change and a human gate.
   `statefix_check` is currently the only thing standing between that idiom
   and a silent regression.
4. **The audit's O2 figure (−6.6 to −7.4 %) is only fully available at large
   block sizes.** At a 128-frame block this form leaves 1/8 of the RN cost;
   closing that needs the focus-only variant, which is NOT bit-identical (see
   above). Whether the last ~1 % is worth a divergence in a viz observable is
   a human call, not one I took.
5. **ASan is unavailable on this machine** (transcript above). Anything that
   relies on ASan locally — including the brief's "ASan in the sanitize CI job
   is the second witness" — has to come from CI, not from here.
6. Not touched, per the brief: B149 (`tRng`), B150 (the `0.08` smoother).
