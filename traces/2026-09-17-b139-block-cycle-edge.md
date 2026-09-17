# b139-block-cycle-edge — processBlock honours the one-sample delay on a cycle edge

- **Queue item:** B139 — "Must be fixed BEFORE any feedback cell is exposed (the
  rework's routing_core extension): make processBlock honour zPrev per sample for
  cycle edges and add the routing_check assertion that scalar and block paths
  agree on a cycle."

- **Why:** `process()` has read `zPrev` per sample since ADR-128, but
  `processBlock` — the path the shell actually calls
  (`src/hypersaw_clap.cpp:5379`) — gathered a whole block per slot, so a
  backwards edge read `slotL[f − NSRC]` for the *current* block, i.e. the
  previous BLOCK's samples. That is a delay of the host's buffer size: the
  flanger-not-a-routing-primitive failure ADR-128 rejected by name, and a
  guaranteed disagreement between the two paths at the first exposed cycle edge.
  Latent today only because B50 phase 1 exposes the acyclic cell subset.

## What changed

**`src/routing_core.h` — `processBlock` grows a second regime.** A live cycle
edge anywhere in the topology (asked once per block, via the existing
`connected()` / `edgeForward()` predicates — no third copy of "which edges are
live") switches the whole pass to sample-by-sample, calling `proc` with `n = 1`
so the loop closes at sample rate. Without a cycle edge the block-wise gather is
kept **verbatim**, which is what keeps every golden, the 156 parity scenarios and
routing assertion 7's rack comparison as this change's regression proof.

Why the whole pass and not just the gather: slot 1's input at sample *i* needs
slot 2's output at sample *i − 1*, and slot 2 is not computed until after slot 1.
A block-wise gather cannot express that ordering for any slot downstream of a
cycle edge, so interleaving the `proc` calls is not an optimisation choice — it
is the only shape in which the rule is expressible.

Two consequential details:

- **Gathering accumulates in `double` in the cycle regime**, matching
  `process()` term for term. That is what lets assertion 17 ask the two paths for
  bit-identity and have the answer be about the routing rule rather than about
  two accumulation orders. The acyclic branch keeps its `float` accumulation
  because changing it would move every golden.
- **`zPrevR[NSLOT]` is added** beside `zPrev`, and `resetFeedback()` clears both.
  The block pass is stereo and the scalar one is mono, so one carry array cannot
  serve both channels. `zPrev` is now the scalar path's state *and* the block
  path's LEFT channel — a shared meaning, not a shared owner: a matrix is driven
  by one path at a time.

**Interface note (flagged, not assumed):** `processBlock` loses its `const`
qualifier, because the per-sample carry lives in the matrix. No caller is
affected — the shell's `routing` member is non-const (`hypersaw_clap.cpp:1547`)
and the only `const RoutingMatrixT` in the tree (`hypersaw_clap.cpp:920`) reads
defaults and never renders. Nothing else in the signature moved.

**`tools/routing_check.cpp` — assertions 17-19** (the brief numbers them 19-21;
the offset the phase-1c block records continues). Plus a `LoopSlots` stand-in: a
gain and a ring, so what is measured stays the routing rule and not an effect.
Its scalar ring holds doubles and its block ring floats *on purpose* — that is
the real asymmetry between the paths, and hiding it behind one type would let
assertion 17 pass on a similarity it invented.

## What 17-19 measure

| # | claim | result |
|---|---|---|
| 17a | block sizes 1 / 7 / 64 / 256 agree, bit for bit, on a sine through a 0.5 loop | 0 mismatched samples of 1024 × 3 comparisons |
| 17b | `process()` sample-by-sample == `processBlock` at all four block sizes | 0 mismatched samples of 1024 × 4 |
| 17c | *control:* the loop is audible at all (vs the same chain without the edge) | 1022 / 1024 samples differ |
| 17d | *control:* the cycle BRANCH at coefficient 0 renders the serial chain bit-exactly | yes |
| 18 | silence in → **exact** silence out at loop gain 1.2 | 0 non-zero samples of 4096 |
| 18c | *must-read-nonzero control:* one impulse into the same loop runs away | reaches 1.55e+20 |
| 19 | a 5 ms loop (220 samples + the edge's own) at 0.6 decays ≥ 100 dB inside 240 ms | −208.5 dB (tail 3.742e-11 against a peak of 1) |
| 19c | *must-not-decay control:* the same loop at gain 1.0 still holds full scale in the same window | 0.0 dB |

17b is asked on an **impulse through power-of-two coefficients**, and that choice
is load-bearing: `process()` accumulates in double and stores doubles, the block
path stores each slot's output as a float, so bit-identity between them is only a
statement about *which sample each edge reads* on data where both types are
exact. Every value in that loop is 2^−k. Any other input would measure
float-vs-double accumulation and report it as a routing disagreement. 17a carries
the data-independent half of the claim: it compares the float path against
itself, so it is asked on a real sine.

### Calibration (both plants fired; one boundary recorded)

- **Plant A — the pre-B139 engine restored** (`cyclic` forced false, so a cycle
  topology takes the block-wise gather). Assertion 17 RED: *"block 1/7/64/256
  disagree on 3071 samples; scalar vs block 3121"*. Assertion 18 RED: *"4096
  samples of silence: 3712 non-zero output samples"*.
- **Plant B — the brief's literal plant** (the cycle edge reads
  `slotL[f − NSRC][i]`, the current block, instead of `zPrev`). **Identical**
  numbers to plant A, and that is not a stale object: both plants make the cycle
  edge read the same thing — the previous block's content still sitting in the
  scratch buffer at index *i*. At a block size of 1 that content happens to *be*
  the previous sample, which is why the failure is "every block size > 1", as the
  brief predicted. The binaries differ (`e2e672b2…` planted vs `4cfad5e3…`
  clean) and removing the plant returns the suite to GREEN, so the source does
  reach the binary (L0032's stale-object case, checked rather than assumed).
- **Assertion 19 did NOT fire under either plant** (it reported −164.2 dB, still
  inside its threshold). Recorded rather than retried until something fired
  (L0033): a decay measurement cannot see a wrong-sample read, because a loop
  that reads the wrong sample still loses 0.6 per trip. 19 covers stability, 17
  covers the delay, and that division is the coverage map — not an accident.
- Objects were deleted before each plant build: CMake does not track
  `src/routing_core.h` as a dependency of these targets (the file's own note).

## Cost (brief item c — reported, no gate)

`processBlock` alone, one-pole stand-in slots, 512-sample blocks, 10.24 M
samples per run, `-O3`, timed in the main context (L0052). Scratch:
`scratchpad/b139/bench.cpp`, built against the pre-change header
(`origin/main:src/routing_core.h`) and the new one.

- **Acyclic default, before vs after: unchanged within noise.** Interleaved
  ×10: min 8.665 vs 8.698 ns/sample, medians 8.91 vs 8.86 — a 0.4 % spread
  against ±5 % run-to-run variance. Entailed, not merely measured: the acyclic
  branch is byte-identical code, and all the change adds is one topology scan per
  block (16 `connected()` tests per 512 samples).
- **One cycle edge: 12.45 ns/sample** (min of 7; range 12.45–12.87), i.e. **≈ 1.44×**
  the acyclic path. The extra is one `proc` call per sample instead of per block
  plus the per-sample gather, which is O(live edges) per sample.

## Evidence consulted

- ROADMAP B139; `src/routing_core.h` (ADR-128's header note on why block rate was
  rejected, `edgeForward`, `process()`'s sample loop, the aliasing note on
  `outL/outR`); `src/hypersaw_clap.cpp:1547, 5379, 920`;
  `tools/routing_check.cpp` (assertions 3 and 7 and their calibration notes);
  PR #617's `srcOut` as the "both paths agree" pattern.
- LIBRARY L0032 (a detector sharing an assumption confirms what you expect; the
  stale-object case), L0033 (a plant that does not fire measured a coverage
  boundary), L0051, L0052, L0053.

## Alternatives rejected

- **Per-sample gather but block-wise `proc`.** Rejected because it is not
  expressible: the slot downstream of the cycle edge must be advanced one sample
  at a time for the carry to be its own previous output.
- **Latching `zPrev` in the acyclic branch too**, so the carry is always live.
  Rejected on scope and on evidence: it is output-neutral there (nothing reads
  it), so no oracle in this brief could observe it, and an unobservable claim
  rots. See open question 1.
- **A `hasCycleEdge()` member** instead of the inline scan. Rejected: one more
  name on the public surface for a five-line loop used once.
- **Making the block path float-accumulate in the cycle regime** for symmetry
  with the acyclic branch. Rejected: it would make assertion 17b's bit-identity
  unachievable for a reason that has nothing to do with routing.

## Verify

`./verify full` — **exit 0, GREEN**, on the tree of this commit. Gate summary
lines verbatim, in order:

```
presentation_check: GREEN (325 rows, scopes: global, osc1, osc2; 34 undesigned (no chunk named), 5 ungrouped)
depends_check: GREEN (121 declared dependencies, header current, 2 advisory)
gen_gui_controls: GREEN (197 generated control(s), gui2 markup current)
test_table_check: GREEN (189 tests — 104 agentic, 85 human; 14 awaiting an oracle)
include_check: GREEN (98 files, every used std symbol has its header in the file)
gui_reach: GREEN (every declared param is reachable in some GUI)
parity_check: 156/156 scenarios within eps=1e-06 (worst 4.262e-09 @ dyn-ring.seed42)
trajectory_check: GREEN (0 failures)
state_check: GREEN (0 failures)
undo_check: GREEN (0 failures)
statefix_check: GREEN (3 fixtures, 0 failures)
presetstore_check … PASS — 55 checks, 0 failure(s)
bank_check: 0 failure(s)
anchor_check: PASS
penv_check: PASS
twocluster_check: PASS
morphlayout_check: PASS
fxxfade_check: GREEN (0 failures)
polarity … PASSED (0 failures)
notefuzz_check: GREEN (0 hangs)
trace_check: GREEN (0 failures)
steal_check: GREEN (0 failures)
endprobe: PASS
kstuck_probe: GREEN (0 failures)
rtsafety_probe: GREEN (audio thread is allocation-free)
paramscope_check: GREEN (0 failures)
samplerate_check: GREEN (0 failures)
routing_check: GREEN (0 failures)
notchslot_check: GREEN (0 failures)
slotcontract_check: GREEN (0 failure(s), 1 pinned)
subdiv_check: GREEN (0 failures; pan motion excluded pending a ruling)
mpe_check: GREEN (0 failures)
preset_check: GREEN (0 failures)
waveshape_check: GREEN (0 failures)
force_check: GREEN (0 failures)
spectra_check: GREEN (0 failures; worst parity rms 0)
filter_check: GREEN (0 failures; worst parity rms 0)
notch_check: GREEN (0 failures; worst parity rms 0)
swarmalator_check: GREEN (0 failures; worst parity rms 0)
glide_check: GREEN (0 failures; worst parity rms 3.51308e-08)
time_check: GREEN (0 failures; worst parity rms 5.5853e-12)
```

Two gates skipped by `./verify`'s own conditions, unchanged from prior runs:
`verify: .leakcheck-names absent — private-name leak check SKIPPED (expected off
this Mac)` and `verify: FOUNDATIONS headers absent — conformance_check SKIPPED
(expected off this Mac)`.

The bit-identity gate is `parity_check: 156/156` plus `routing_check`'s
assertion 7 (`0/1024 samples differ`) and 14 — all unchanged, no golden
regenerated, no fixture touched. `rtsafety_probe` green: the cycle regime
allocates nothing (`zPrev`/`zPrevR` are members).

## Open questions

1. **The acyclic branch does not latch `zPrev`; `process()` does.** So the first
   sample after a cycle edge goes live reads a stale carry (zeros, in the shell,
   since it never calls `process()`). Whether that matters is phase 2's to rule
   on: it depends on whether the shell sets the `inFrom` presence bit at the
   instant the coefficient leaves 0 (in which case the stale value is multiplied
   by ≈ 0 and is inaudible) or at some other moment. Recorded in the header
   comment as well, so it cannot be discovered as a surprise.
2. **The regime switch is per block, so a cycle edge that goes live mid-block
   takes effect at the next block boundary.** Consistent with every other
   topology write (parameters are applied per block), but not asserted anywhere.
3. **No `proc`-reentrancy claim.** The cycle regime calls a slot's `proc` with
   `n = 1` up to 44 100 times a second per slot; `FxRack::processSlot` is
   sample-loop-based and correct under that, but nothing gates a future slot type
   that assumes a minimum block length. Phase 2 should pin it.
4. **ADR candidates** (not filed — `DECISIONS.md` is out of scope):
   *(a)* a routing topology carrying a cycle edge is processed sample-by-sample,
   `proc` included — the one-sample delay of ADR-128 is a promise about the
   ENGINE, not about the scalar path, and block-wise processing of a feedback
   graph is not an optimisation but a different instrument;
   *(b)* `processBlock` is no longer `const`, because feedback state is matrix
   state — any future path that carries state across blocks inherits this;
   *(c)* the cycle regime accumulates in double while the acyclic one accumulates
   in float, and the boundary is deliberate: bit-identity with the existing
   goldens on one side, agreement with `process()` on the other.
