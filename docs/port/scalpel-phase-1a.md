# SCALPEL port, phase 1a: the blade engine

ROADMAP B332. Rulings: ADR-186 (horde 2 is a new shell; cores are copied forward),
ADR-187 (parity strength, the divergence ledger, CPU as Layer-E). Written
2026-09-28, and revised the same day after an independent Opus critic's review
(verdict REWORK, narrow). The corrections are marked where they land; the first
version's numbers are kept in its trace, `traces/2026-09-28-b332-scalpel-port-1a.md`.

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
  public methods instead. The bench's `re` and `panic` messages are in the
  script grammar and have rows, so both paths are parity-checked.
- **The bench UI, wavetable export and any plugin shell.**

## Parity (the oracle: `tools/h2_scalpel_parity_check.cpp` + `tools/h2_scalpel_render.mjs`, in `./verify full`)

The parity target is pinned by content:
`reference/scalpel/prototype/razor-core.js@0ce6a713410d89c65bf55f761f1dc791fae61b16`
(git blob). The stream carries the hash, and `tools/h2_rules_check.py` fails
`verify fast` if `h2/README.md`'s pin and the file disagree (ADR-187 item 3).

The harness covers 386 scenarios:

- **Preset scenarios:** all 83 bench presets × {held chord, repeated note,
  arpeggio}, plus a legato phrase for the 12 mono presets (261 scenarios).
- **Targeted rows (125)**, which cover:
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
  - **Added in the rework (14 rows, from the critic's M2):**
    - a short R, then a re-strike after the voice has fully released (the freed
      slot is reused as a fresh voice);
    - w 0 and w2 0 (each blade off);
    - colK and colB swept to 0 mid-note (collision on → off);
    - blade 2's own clock at rest (rotRate2 0 from the start), and swept to 0
      mid-note (blade-2 homing);
    - mono with glide 1 ms (the `glide <= 1` paths);
    - `settle()` skipped (K 0, so Ke < 1e-3);
    - collision with both blades in Crush (`gate()`'s mode-6 branch);
    - blade 2 width-locked (lock2 1) with its own pitch FM and collision;
    - the bench's `retune` and `panic` messages (now in the script grammar as
      `re` and `panic`);
    - a mid-note `polyMode` switch (poly → legato while three notes sound, then a
      fourth);
    - the modulator's 65536 wrap: m 64 on a chord from note 100 for 0.72 s. Both
      `m.modX` and blade 2's own `bx.modX` wrap once (checked against the oracle
      by hand before the row was added).

The acceptance criteria (ADR-187 item 6) are ALL of the following:

- RMS < 1e-6;
- max-abs < 1e-6 on every sample (the stated bound);
- identical blade-event counts and times.

The events are every PolyBLEP correction (tryE, scan) and every blade-window
entry. Each is keyed by oversampled tick, member and kind, and hashed in order.
The samples compared are the PRISTINE oracle's. The events are counted by an
in-memory scratch copy of the oracle with seven literal insertions, rendered
alongside every scenario; its samples must equal the pristine ones bit for bit
on every scenario (the NONINV row: 386 of 386).

**Result at the committed hash (see the rework trace for the per-scenario table):**

- **383 of 386 scenarios at parity** (was 369 of 372 before the rework; all 14
  new rows pass).
  - Worst RMS is 2.22e-12 and worst max-abs is 1.90e-10, both on T/fm mshape 7,
    the S&H-noise FM modulator; see libm below.
  - The mean bit-exact sample share is 29.2%.
- **Blade events are identical on all 386 scenarios, the excluded ones
  included.** The totals are:
  - 847,983 edge/base BLEPs;
  - 446,727 carrier BLEPs;
  - 261,019 blade-1 window entries;
  - 124,283 blade-2 window entries.
- **3 scenarios are excluded as chaotic, with evidence:** Cross-mod ring (watch)
  × {chord, repeat, arp}.
  - This patch (xm 0.7) feeds each member's phase from its neighbour's last
    output around a ring.
  - **An exclusion exempts the max-abs bound ONLY** (tightened in the rework,
    critic M1). The excluded row must still hold RMS < 1e-6 and identical
    events, and the oracle against itself, with its inputs 1 ULP apart, must miss
    the max bound by at least as much as the C++ does (selfMax ≥ C++ max).
  - C++ vs JS: RMS 1.4e-7, 3.0e-7 and 1.8e-8 (all under 1e-6); max 1.2e-5,
    2.6e-5 and 1.5e-6; events identical.
  - The oracle against itself, inputs 1 ULP apart: max 9.9e-4, 2.1e-3 and
    1.4e-2, which is 80–9,500× further than the C++ goes.
  - **The count is pinned** (`kExpectedExclusions = 3` in the check): the list
    cannot grow or shrink without a deliberate edit that a reviewer sees.
  - `./verify full` prints the three EXCL rows and their evidence on green runs
    too, so what was exempted is always in view.

### libm (LIBRARY L0066's prediction, measured on this Mac: Node 24.10 vs Apple libm)

These are 20,000 probes per function, in the ranges the oracle calls each one with.
The check prints them on every run; they are informational, never judged.

| fn | bit-identical | worst gap | note |
|---|---|---|---|
| sin | 95.6% | 1 ULP | |
| cos | 95.5% | 1 ULP | |
| exp | 90.1% | 1 ULP | |
| log | 93.6% | 1 ULP | |
| pow (general base) | 100% | 0 | a runtime base, so `std::pow` really runs |
| pow(2, x) as compiled | 99.81% | 1 ULP | V8 `Math.pow(2,x)` vs `std::exp2(x)` |
| atan2 | 82.5% | 1 ULP | |
| asin | 91.1% | 1 ULP | |
| tanh | 86.4% | 2 ULP | |
| sqrt | 100% | 0 | |
| hypot | 62.2% | 2 ULP | |

**Correction (critic LOW item).** The first table's `pow 100%` row did not
describe the core. Clang lowers the core's constant-base-2 `pow(2, x)` sites
(spreads, envelopes, cents) to `exp2` for a non-integral x, and to `ldexp` for an
integral one. The parity binary's undefined symbols include `_exp2` and `_ldexp`
beside `_pow` (`nm -u`). The `pow2` row now measures what the core actually
calls, and it is not bit-identical (99.81%). Clang also fuses a `sin` and `cos`
of the same argument into `__sincos_stret` (present in the same symbol list); the
probes call them separately, so that variant is not measured separately.

The differences are real. They break parity on no non-chaotic scenario, and the
mechanism of the largest one is known:

- The oracle's `hash()` takes `sin(i·127.1 + 311.7)` at arguments up to about
  10⁷ and multiplies the result by 43758.5453.
- That multiplication amplifies a 1-ULP sine gap by about 4×10⁴ before `frac()`.
  The FM index then amplifies it again.
- This is why the noise-FM rows (mshape 5 and 7) are the most sensitive
  non-chaotic scenarios: at most 1.9e-10.

No scenario is fully bit-exact. Every output sample passes through tanh, whose
V8 and Apple values agree on 86% of probes; how much of the gap tanh accounts for
was not separated (hypothesis, not measured). Nothing was loosened to absorb it.

### Must-fail controls (each run, in the same binary)

Each control plants a fault into the core under `H2_SCALPEL_FAULTS`, which only the
check compiles. Each must turn a scenario red that is green without it.

| Fault | Scenario | What it must break | Result |
|---|---|---|---|
| F1 `std::round` for `Math.round` | T/spread snapped −half (the one input where they differ; out of the UI's 0..24 range) | parity | rms 1.5e-1, events disagree |
| F2 phase/modX draws swapped | T/phase random | parity | rms 3.8e-1, events disagree |
| F3 blade-entry BLEP skipped | T/mode 0 | the samples AND the events (both required since the rework) | rms 1.0e-3, max 1.9e-2, events disagree |
| F5 every event one tick late (new) | T/mode 0 | the events ALONE: the samples must stay bit-identical to the clean replay | samples bit-identical, events disagree |
| F4 FMA contraction on (`h2_scalpel_fma_control`: the same source at `-ffp-contract=fast`, over the FULL stream since the rework) | all 386 | parity on at least one row | **19 of 386 miss parity**; worst rms 2.18e-6, worst max 1.47e-4 (Crunch (audio-rate PM) :: repeat); bit-exact share falls to 6.0% |

F4's verdict now separates three outcomes: exit 0 means it fired (a genuine parity
miss on a complete, well-formed stream); exit 1 means the contracted build passed;
exit 2 means the run itself broke (renderer crash, truncated stream, instrumentation
mismatch), which is never counted as firing. A stream cut mid-scenario was fed to
it by hand and returned 2 with "INFRASTRUCTURE FAILURE".

### Detection floor (critic M3; printed every run, not judged)

One non-hash constant is perturbed: the swarm's pitch in `couple()` is scaled by
(1 + eps), for eps = 1e-12 … 1e-3, on T/mode 0 :: chord (N 3, a hard sync blade,
0.29 s). The smallest eps from which every larger one is red is the floor.

| eps | rms | max | events | verdict |
|---|---|---|---|---|
| 1e-12 | 4.8e-9 | 6.5e-8 | agree | green |
| 1e-11 | 4.8e-8 | 6.5e-7 | agree | green |
| 1e-10 | 4.8e-7 | 6.5e-6 | agree | RED (max-abs) |
| 1e-9 | 4.8e-6 | 6.5e-5 | agree | RED |
| 1e-8 … 1e-3 | ≥ 4.8e-5 | ≥ 6.5e-4 | disagree | RED |

**The floor is 1e-10 relative** on the swarm's pitch (about 1.7e-7 cents), and it is
the max-abs bound that sees it first: a sync blade turns a pitch error into a
phase error at each blade entry, which is amplified by the cut rate. A 1e-12
relative slip in that constant is invisible to this gate. The event criterion
alone sees from 1e-8. The check requires only that 1e-3 is red (a sanity row).

## Every h2 build is contraction-off (critic H1; PENDING HUMAN RULING)

The first draft of this document called F4 "narrow", fired on 2 targeted rows.
That was wrong: the control was filtered to the 111 targeted rows. Over the full
stream the contracted build misses parity on **19 of 386** scenarios:

- Crunch (audio-rate PM) × {chord, repeat, arp, legato}: worst rms 2.18e-6, max
  1.47e-4;
- Frozen noise FM × 3, Trance jitter × 3, Jitter swarm × 2, Wobble jaw × 4;
- T/fm mshape 7, T/b2 own fm;
- Cross-mod ring (watch) :: arp, whose RMS (1.2e-5) no longer meets the excluded
  rows' RMS rule.

Clang's DEFAULT (`-ffp-contract=on`, no flag) gives the same 19 at -O2 and at -O3:
the same set, row for row (measured with scratch builds of this same source). So
a Release build at the compiler's default flags is **not** the build that passed
parity. The first CPU table's "Release vs oracle" column, rms 8e-15 on the two
bench presets, was true of those two presets and misleading as a statement about
the Release build: neither preset is among the 19.

Rule (enforced by `tools/h2_rules_check.py` in `verify fast`; stated in
`h2/README.md` rule 7): **every CMake target that compiles an h2 core carries
`-ffp-contract=off`.** The one declared exception is `h2_scalpel_fma_control`,
which must carry `-ffp-contract=fast`. This is labelled PENDING HUMAN RULING: the
human is being asked whether the build that ships must be the build that passed
parity. Until then, every h2 build stays on the parity side.

## CPU (Layer-E, ADR-187 item 8; this Mac, 2026-09-28, by hand)

One voice held for 4 s at 48 kHz, 2× oversampling, best of 5 (C++) or best of 3
after a warm-up (JS, Node 24.10). The machine was loaded (load average 11–17 on 8
cores), so the two C++ builds were run interleaved three times each, and the
figures below were stable to ±0.01 points across the three.

| Preset | JS | C++ -O3, contraction off (`measure_h2_scalpel` now) | C++ -O3, clang default contraction | cost of the rule |
|---|---|---|---|---|
| Crushed bells (N 6, two blades; the heavy class) | 11.8–11.9% RT per voice | 5.21% | 4.91% | +6.2% |
| Quarter sync (N 1, one blade) | 2.1–2.3% | 0.54% | 0.58–0.59% | none (off was faster here) |

Calibration loop (1e8 dependent multiply-adds): JS 179–193 ms, C++ 111–119 ms.

Distance from the oracle on these two bench presets is rms 8e-15 (Crushed bells)
and 2e-17 to 3e-17 (Quarter sync) in both builds. The full-stream parity
difference between the builds is the 19 rows above, not these two.

The literal port is only 2.3–4× faster than V8's JIT. That is expected of a
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
12. **Two per-sample constants that ADR-009 would express in seconds** (critic
    M4). The per-cycle DC estimate is glided in with `mm.dcS += (mm.dc - mm.dcS)
    * 0.003` every sample (:857): a hand-tuned per-sample coefficient, so its time
    constant (about 333 samples: 6.9 ms at 48 kHz, 7.6 ms at 44.1 kHz) moves with
    the sample rate. The ADR-009 class. And `couple()`/`spread()` run every 32
    samples (`this.cnt = 32`, :752), a cadence in samples rather than seconds
    (PLAUSIBLE rather than certain: the swarm's integration step then depends on
    the rate, which the 44.1 kHz row exercises but no oracle judges).

A caveat on the port rule itself: `floor(x + 0.5)` differs from V8's `Math.round`
at exactly one double, 0.49999999999999994, where it gives 1 and JS gives 0. No
oracle path can reach it, since every `Math.round` argument is a spread, a
rotation offset or a rule index. The rule is followed as ADR-187 states it.

## What phase 1b needs

- **`razor_core.h` stays byte-stable** (critic M5). Phase 1b composes the blade
  engine with horde's swarm by COMPOSING or SUBCLASSING `horde2::scalpel::RazorCore`
  (the way `ComposedEngine extends RazorCore` in the JS), never by editing this
  file for 1b's sake. The 1a harness then remains a live regression check on the
  blade half for as long as the oracle file is pinned: any 1b edit that leaks into
  the blades turns it red.
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
- **The shipped-flags ruling** (above): if the human rules that the shipped build
  may contract, the 19 rows become a divergence to ledger or a golden platform
  question (ADR-187 items 5 and 7); if not, rule 7 stands.
- **The Linux platform question.** The sanitizer CI job runs this check against
  glibc's libm on x86-64. It was GREEN there on the first push (369/372, the same
  3 exclusions), so no platform finding yet.
