# SCALPEL port, phase 1a: the blade engine

ROADMAP B332. Rulings: ADR-186 (horde 2 is a new shell; cores are copied forward),
ADR-187 (parity strength, the divergence ledger, CPU as Layer-E). Written
2026-09-28.

Phase 1a is a **faithful** C++ port of SCALPEL's blade engine, proven by parity
against its oracle. Nothing has been diverged. The quirks found along the way are
listed below as candidates for the ADR-187 ledger. Every one of them is ported
exactly as the oracle has it.

## What is ported

`h2/cores/scalpel/razor_core.h`, `horde2::scalpel::RazorCore`, is a line-for-line
transcription of `reference/scalpel/prototype/razor-core.js` (v1.1 as ingested; the
file is protected and was only read). It keeps the oracle's structure, its order of
operations and its member-state layout. It includes:

- **Blade 1 and blade 2:**
  - modes sync, FM reset and FM free (phase and pitch FM), noise S&H, fold, ring
    and crush (with slew, and with serial hold);
  - every blade wave and base wave, including sine→saw with its per-member
    band-limit;
  - the cut-rate units: cycles, width-locked and Hz;
  - edges, depth, mirror (reflect), and the twins (inverted and plain).
- **v1.1 interplay:**
  - serial stacking with λ, in both stacking orders;
  - collision pitch (integrated carrier chirp) and collision bite;
  - the pair DC estimate.
- **The swarm:**
  - SCALPEL's own Kuramoto coupling law (`keff`, `couple`), including the higher
    harmonics under negative K;
  - `settle()`;
  - the member spread laws (gradient, random, alternate, swarm-linked, drift);
  - the cut rules 1–9, with the custom-ratio parser, and snapped spreads;
  - blade 2's own spreads.
- **Per-voice state:**
  - blade envelopes 1 and 2;
  - rotation: shared, per-member and blade 2's own clock, with the home glide;
  - frames 1 and 2;
  - cross-member modulation and feedback.
- **Anti-aliasing:**
  - PolyBLEP on every known discontinuity: base edges, blade edges, twin edges,
    and carrier wraps tracked by `scan()`;
  - the per-cycle DC estimators (closed-form and numeric);
  - the 2× (or n×) oversampling biquads and the DC blocker.
- **Voices and output:**
  - the poly allocator (same-note reuse, then the first free voice, then the
    oldest voice by age);
  - mono and legato, with the note stack and glide;
  - the tanh output stage;
  - the 16-sample and per-sample parameter smoothers.
- **Randomness:** the oracle's five `Math.random` call sites consume one
  mulberry32 stream, in the oracle's order:
  1. `rnd()` for noise;
  2. `startVoice`'s phase and modX draws;
  3. `startVoice`'s random-law draws;
  4. `settle()`;
  5. the drift law's Box–Muller pair.

  The JS harness installs the same generator over `Math.random`, following
  `composed_engine_check.mjs`'s convention.

The port follows these rules from ADR-187 item 6:

- **Parity build:** doubles, `-O2 -ffp-contract=off` (target
  `h2_scalpel_parity_check`).
- **Rounding:** `Math.round` is ported as `floor(x + 0.5)`.
- **NaN semantics:**
  - `Math.min` and `Math.max` propagate NaN and order ±0 as JS does (`js::min`,
    `js::max`);
  - JS truthiness of a number is `x != 0 && !isnan(x)`;
  - `switch` on a number is strict equality, so a non-integral selector takes
    `default`;
  - `Math.pow(±1, ±∞)` and `Math.pow(x, NaN)` return NaN.
- **Integer/float conversions:** `j < os` is compared as int against double, as
  JS does. `slice(0, poly)` follows `ToIntegerOrInfinity`, including negative
  ends.
- **Real-time safety:** everything is preallocated (8 voices × 9 members, the
  note stack, and the rule-list cache). `setString("kCustom")` copies into a fixed
  buffer.

**API differences** (the harness's, not the sound's):

- `set(key, value)` takes one key at a time, and `setString` handles `kCustom`.
- `snap()` is the `Object.assign(c.s, c.t)` that the oracle's own
  `render-goldens.js` performs.
- Note numbers are integers from 0 to 127.
- **Domain clamps.** The JS throws or indexes out of bounds in three cases, and
  the port guards each one:
  - N outside 1..9 (the JS reads `PANS[N-1]`);
  - a poly pool of 0 (the JS dereferences an undefined voice);
  - notes outside 0..127.

  Inside the domain, the clamps are inert.

## What is not ported (phase 1b and later)

- **The composed engine.** The composed engine is horde's swarm (SwarmSynth's
  coupling law, onset and dissolve, drift, the inertia taper, detune laws
  0/1/2/4/5) driving these blades, per ACCOUNTING §1.6. It also includes the B310
  voice law, the B323 cull, B325's first-tick frequencies and ADR-184 A2's sign
  rules. Its file is `docs/design/scalpel-horde-engine.js`, pinned at `c79be56`,
  and it is the parity target B332 names for SCALPEL (ADR-187 item 3). Phase 1a
  is the blade half that the composed engine inherits unchanged.
- **The bench's `viz` posting.** `render()` keeps the `vc` countdown but posts
  nothing. `v.r` uses `std::hypot`, which is visualisation-only in the oracle
  (never rendered). The measured V8/libm hypot agreement is 62%; see below.
- **The `msg()` dispatcher.** `noteOn`, `noteOff`, `retune` and `panic` are
  public methods instead.
- **The bench UI, wavetable export and any plugin shell.**

## Parity (the oracle: `tools/h2_scalpel_parity_check.cpp` + `tools/h2_scalpel_render.mjs`, in `./verify full`)

The harness covers 372 scenarios:

- **Preset scenarios:** all 83 bench presets × {held chord, repeated note,
  arpeggio}, plus a legato phrase for the 12 mono presets (261 scenarios).
- **Targeted rows (111)**, which cover:
  - each blade mode, blade wave, base wave, FM type and modulator shape;
  - the lock units, mirror, twins, frames and hard edges;
  - crush (hard, and slewed), and noise;
  - rotation in 5 variants;
  - blade 2 in 14 variants;
  - interplay in 9 variants;
  - the DC modes, oversampling ×1/×2/×4, and anti-aliasing off;
  - laws 0–4, rules 1–9 (including a malformed custom list) and snapped spreads;
  - the envelopes, cross-mod, feedback, splay and strong lock;
  - phase modes, pan order and bend;
  - parameter sweeps;
  - 44.1 kHz, voice stealing, and mono retrigger with glide-always.

The acceptance criteria (ADR-187 item 6) are ALL of the following:

- RMS < 1e-6;
- max-abs < 1e-6 on every sample (the stated bound);
- identical blade-event counts and times.

The events are every PolyBLEP correction (tryE, scan) and every blade-window
entry. Each is keyed by oversampled tick, member and kind, and hashed in order.
The JS side is counted by an in-memory scratch copy of the oracle with seven
literal insertions. A non-invasiveness row proves that copy renders the pristine
oracle's samples bit for bit on 8 scenarios that reach every insertion.

**Result at the committed hash (see the trace for the per-scenario table):**

- **369 of 372 scenarios at parity.**
  - Worst RMS is 2.22e-12 and worst max-abs is 1.90e-10, both on T/fm mshape 7,
    the S&H-noise FM modulator; see libm below.
  - The median max-abs is 1.6e-15. 36 scenarios exceed 1e-12, and 151 exceed
    1e-14.
- **Blade events are identical on all 372 scenarios, the excluded ones
  included.** The totals are:
  - 729,437 edge/base BLEPs;
  - 341,273 carrier BLEPs;
  - 230,120 blade-1 window entries;
  - 95,884 blade-2 window entries.
- **3 scenarios are excluded as chaotic, with evidence:** Cross-mod ring (watch)
  × {chord, repeat, arp}.
  - This patch (xm 0.7) feeds each member's phase from its neighbour's last
    output around a ring.
  - C++ vs JS: max 1.2e-5, 2.6e-5 and 1.5e-6.
  - The oracle against itself, with inputs 1 ULP apart: max 9.9e-4, 2.1e-3 and
    1.4e-2. That is 80–9,500× further than the C++ goes, which meets ADR-065's
    evidence rule.
  - The check re-measures this on every run. It turns red if the JS alone stops
    breaking the gate, or stops diverging comparably.

### libm (LIBRARY L0066's prediction, measured on this Mac: Node 24.10 vs Apple libm)

These are 20,000 probes per function, in the ranges the oracle calls each one with.
The check prints them on every run; they are informational, never judged.

| fn | bit-identical | worst gap |
|---|---|---|
| sin | 95.6% | 1 ULP |
| cos | 95.5% | 1 ULP |
| exp | 90.1% | 1 ULP |
| log | 93.6% | 1 ULP |
| pow | 100% | 0 |
| atan2 | 82.4% | 1 ULP |
| asin | 91.3% | 1 ULP |
| tanh | 86.3% | 2 ULP |
| sqrt | 100% | 0 |
| hypot | 62.2% | 2 ULP |

The differences are real. They break parity on no non-chaotic scenario, and the
mechanism of the largest one is known:

- The oracle's `hash()` takes `sin(i·127.1 + 311.7)` at arguments up to about
  10⁷ and multiplies the result by 43758.5453.
- That multiplication amplifies a 1-ULP sine gap by about 4×10⁴ before `frac()`.
  The FM index then amplifies it again.
- This is why the noise-FM rows (mshape 5 and 7) are the most sensitive
  non-chaotic scenarios: at most 1.9e-10.

Only 28.8% of samples are bit-identical on average, and no scenario is fully
bit-exact. Every output sample passes through tanh, whose V8 and Apple values
agree on 86% of probes; how much of the gap tanh accounts for was not separated
(hypothesis, not measured). Nothing was loosened to absorb it.

### Must-fail controls (each run, in the same binary)

Each control plants a fault into the core under `H2_SCALPEL_FAULTS`, which only the
check compiles. Each must turn a scenario red that is green without it.

| Fault | Scenario | Result |
|---|---|---|
| F1 `std::round` for `Math.round` | T/spread snapped −half (the one input where they differ; out of the UI's 0..24 range) | rms 1.5e-1, events disagree |
| F2 phase/modX draws swapped | T/phase random | rms 3.8e-1, events disagree |
| F3 blade-entry BLEP skipped | T/mode 0 | rms 1.0e-3, max 1.9e-2, events disagree |
| F4 FMA contraction on (`h2_scalpel_fma_control`: the same source at `-ffp-contract=fast`) | the 111 targeted rows | 2 rows red: T/fm mshape 7 at max 1.8e-5 and T/b2 own fm at max 3.2e-5. It fires. |

F4 is narrow. At the 1e-6 bounds, contraction is visible **only** where `hash()`
amplifies it. Elsewhere it shows only as a loss of bit-exact samples. The
control exits 0 when it fires, so a sanitizer run that executes every wired binary
reads it correctly. If a future change stops the noise-FM rows from amplifying
contraction, the control goes red. That is intended: it is a must-fail.

## CPU (Layer-E, ADR-187 item 8; this Mac, 2026-09-28, by hand)

One voice held for 4 s at 48 kHz, 2× oversampling. The C++ figure is the Release
build (`measure_h2_scalpel`, -O3 with default contraction), best of 5. The JS
figure is Node 24.10, best of 3 after a warm-up. Calibration loop (1e8 dependent
multiply-adds): JS 179.0 ms, C++ 111.2 ms.

| Preset | JS | C++ Release | Release vs oracle |
|---|---|---|---|
| Crushed bells (N 6, two blades; the heavy class) | 11.91% RT per voice | 4.95% RT per voice | rms 8.0e-15 |
| Quarter sync (N 1, one blade) | 2.13% | 0.60% | rms 3.5e-17 |

The literal port is only 2.4–3.6× faster than V8's JIT. That is expected of a
transcription that keeps every NaN-safe min/max, every DC re-estimate and every
double-precision branch. Optimisation is output-neutral work under ADR-187 item 5.
It must leave every digest unchanged; it is not a divergence.

## Divergence candidates (for the ledger; NOT diverged here)

B316's five, confirmed present in the port because the port is faithful:

1. **The modulator rate leaks into non-FM blades.** The per-cycle DC estimator's
   resolution counts `s.mEff·w` (razor-core.js:577), so the modulator rate is
   heard in non-FM blades.
2. **The PolyBLEP scanner reads the collision accumulator for Crush.** It reads
   `ns.cd` (:633) for Crush too.
3. **Reflected Crush jumps at blade exit.**
4. **The drift law shifts note-on draws.** It draws from the shared `Math.random`
   stream in `spread()` (:495), so every later note-on draw shifts.
5. **λ > 0 always selects the pair DC estimate.** It is selected even with one
   blade silent (:844).

Found during the port:

6. **Blade 2's FM modulator uses the wrong normaliser.** `stepM` sets
   `RazorCore.mr = bx.mr` around blade 2's `fmStep` but not `RazorCore.mn`
   (:676–678), so a sine→saw modulator (mshape 6) on blade 2 divides by blade 1's
   normaliser. The same mixing happens in `scan()` for blade 2 (:629–630, called
   at :711): the BLEP time estimate reads blade 1's mr and mn while the audio reads
   blade 2's. Latent: the bench offers modulator shapes 0/1/2/4/3/5/7 only
   (scalpel-bench.html:1334, :1438). The core accepts 6, and T/b2 sine-to-saw
   own mod exercises it.
7. **mshape 6 reuses the carrier's band-limit.** A modulator in sine→saw shape
   reads the carrier's per-member band-limit (`RazorCore.mr`, derived from the
   carrier's cut rate), not a limit of its own. It is latent for the same reason.
8. **The rotation on/off gates read the TARGET, not the smoothed value.**
   `rOn`, `sOn`, `rOn2` and `sOn2` read `t.rotRate` and the others (:786, :790–791).
   Switching rotation off starts the home glide at once while `s.rotRate` is still
   gliding down.
9. **The class statics are process-global.** `RazorCore.mr`, `RazorCore.mn` and
    the rule-list cache are class statics, shared by every instance in one JS
    realm. They are always set before use within one `render()`, so no current
    output depends on them. A second instance in the same AudioWorklet scope, or
    a future refactor, could make them matter. The C++ holds them per instance.
10. **The prime rule goes NaN from member 10** (`PR[j]/2`, :96). Already noted in
    B252. The port's N clamp (1..9) keeps it unreachable.
11. **The oracle's own voice law.** Same-note reuse and steal-oldest-by-age
    (:372–375) are superseded in the composed engine by B310's horde allocator,
    so this belongs to phase 1b, not to the ledger.

A caveat on the port rule itself: `floor(x + 0.5)` differs from V8's `Math.round`
at exactly one double, 0.49999999999999994, where it gives 1 and JS gives 0. No
oracle path can reach it, since every `Math.round` argument is a spread, a
rotation offset or a rule index. The rule is followed as ADR-187 states it.

## What phase 1b needs

- **The composed engine's swarm half, in C++.** Horde's `SwarmSynth` law drives
  the member phases, per `docs/design/scalpel-horde-engine.js` at `c79be56`, with
  φ_S = frac(φ_H + ½), B310's allocator, B323's cull, B325's first-tick
  frequencies and A2's snapped-spread sign rule.
- **A decision on where the swarm comes from.** Two options:
  - copy forward legacy's `src/swarm_core.h`. It is already parity-proven
    against `SwarmSynth` (`parity_check`), and a byte-identical lift carries that
    proof (ADR-187 consequence L3);
  - port the JS swarm fresh. Copy-forward is the "reduce" answer. It needs the
    inertia taper that lives in the legacy shell (`hypersaw_clap.cpp:7307-7313`),
    not in the core.
- **The harness extended to the composed engine.** The same stream protocol and
  C++ check would apply, with a renderer that loads the engine as
  `composed_engine_check.mjs` does, and scenarios seeded through the composed
  engine's own seeded wrapper (ADR-187 item 3).
- **The Linux platform question.** The sanitizer CI job runs this check against
  glibc's libm. Phase 1a measured Apple's libm only. If glibc breaks parity on a
  non-chaotic row, that is a platform finding for ADR-187 item 7 (one canonical
  golden platform), not a reason to loosen anything.
