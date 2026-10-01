# SCALPEL port, phase 1b: step 1 (the swarm lift) and what stops the composed layer

ROADMAP B379 (B332 phase 1b). Rulings: ADR-186 item 4 (cores are copied forward),
ADR-187 (parity strength, the divergence ledger, L3: a byte-identical lift is the
core's parity proof). Written 2026-09-30.

The brief had five steps: lift the swarm core, answer the allocator question, write
the composed layer, extend the parity harness, and measure CPU. The implementer
stopped after the first two, because the brief's own constraints and the measured
code conflict (below). The lead then narrowed this PR to **step 1 only**: the lift,
its one edit, the checks that hold it, and this record. Steps 3 to 5 wait on the
decisions listed at the end. Nothing here is decided; each open item is the
human's.

## What landed

- **The lift.** `h2/cores/swarm/swarm_core.h`, `force_core.h` and `glide_core.h`
  are `src/`'s files at `1c421b5` (origin/main, 2026-09-30), byte for byte, except
  for four lines, all listed in `h2/cores/swarm/lift-ledger.json`:
  - **NS1–NS3, the namespace lines.** `namespace hypersaw` becomes
    `namespace horde2::swarm::hypersaw` (in swarm_core.h and glide_core.h), and
    `namespace forcecore` becomes `namespace horde2::swarm::forcecore`. The legacy
    name stays the innermost component on purpose. The files qualify their own
    names (`hypersaw::GlideCore` on 8 lines, `forcecore::` on 4), and with the
    legacy name nested those references resolve to the lifted copies by ordinary
    lookup, so no other line changes. The closing `}  // namespace hypersaw` is
    unchanged: one brace closes a C++17 nested namespace definition.
  - **E1, the one edit:** a public forwarder, `tickVoice(Voice&, bool)`, to the
    private `controlTick`. It is access only (see "The allocator question").
- **The legacy parity chain, duplicated under h2 flags** (critic H1; ADR-186 item 4:
  "re-pointed or duplicated at the lift"). Legacy `parity_check` is built at clang's
  default contraction, so it never measured the flags horde 2 ships.
  `tools/h2_swarm_parity_check.cpp` is a COPY of `tools/parity_check.cpp`. Only its
  include and its one namespace-qualified line change (ledger entries T1 and T2).
  The copy is itself held by the byte gate, against its source blob. It is built
  as an h2 target at `-O2 -ffp-contract=off` and run in `./verify full` beside
  `parity_check`, on the same goldens: **156/156 within 1e-6, worst 2.485e-9 @
  saw-glass.seed1234**.
- **`tools/h2_lift_check.py`, in `./verify fast`.** It rebuilds each lifted file
  from its CURRENT `src/` original by applying the ledger's hunks in order, AS
  BYTES, and demands byte equality. Red cases:
  - an unledgered difference, including a CRLF or a stray `\r`;
  - a stale entry (a ledgered edit the tree no longer carries);
  - an anchor or block that is not unique;
  - a `*.h` in the core directory that the ledger does not list;
  - a divergence hunk without its `divergences.json` id.

  A source whose git blob no longer equals the ledger's `src_blob` gets its own
  verdict, "legacy moved since the lift; re-lift deliberately", and the copy is
  not blamed. Thirteen self-cases run on every run. By hand, a planted trailing
  space, a stray `\r`, an unlisted header and a wrong pinned blob were each
  caught.
- **One path for every later edit** (the lead's decision, 2026-09-30; `h2/README.md`
  rule 8). Every change to a lifted copy is a hunk in `lift-ledger.json`:
  - lift edits are `E<n>`;
  - divergences (the B378 fixes when they come) are `kind: "divergence"` hunks,
    each carrying its ADR-187 id from a `divergences.json` beside the ledger;
  - multi-line changes use the `patch` op (an exact old block becomes a new block).

  So the gate stays exact after the first divergence instead of being loosened by
  it.

  The non-divergence entries are FROZEN per file in `tools/h2_lift_check.py`
  (critic N1): anything outside NS1–NS3, E1, T1 and T2 must be a divergence, and
  a new lift edit is a deliberate re-lift.
- **`h2_rules_check` rule 4** (critic L5): no TU in `tools/`, `h2/` or `src/`
  includes both a legacy core and an h2 core, with a self-case. `h2/README.md` now
  records two things: that callers spell `horde2::swarm::hypersaw::` in full, and
  that the macro `HZ_CULL_ENV` is un-namespaced and shared by both copies.
- **`tools/h2_swarm_lift_check.cpp`, in `./verify full`** (target
  `h2_swarm_lift_check`, `-O2 -ffp-contract=off`, so `h2_rules_check` covers it).
  It re-proves E1 (see "The allocator question").
- **The contraction rule is RULED** (the human, 2026-09-30: the shipped build is
  arithmetically the tested one). The "pending" labels are gone from
  `h2/README.md` rule 7, `tools/h2_rules_check.py`, the `CMakeLists.txt` comment and
  the `verify` comment. The rule itself is unchanged.
- Not in this PR: the B378 fixes (F1–F16, later, one ledgered divergence each), the
  F3 table edges (B380), B376's manifest decisions, and the plugin shell. `src/`,
  `reference/` and `specs/` are untouched.

## The allocator question: can SwarmCore's per-voice dynamics run without its allocator?

**Not without one edit, and E1 is that edit.** (Line numbers are `src/swarm_core.h`
at `1c421b5`.)

- The per-voice dynamics are `controlTick(Voice&, bool lastOfSeg)` (:1698). They
  are **private**: the `private:` at :1342 covers them.
- Their only caller is `renderSeg` (:1036). It iterates the core's own
  `voices[kPoly]` (:2159, private), which are filled by the private `alloc()`
  (:1664, ADR-083's three tiers) through `noteOn` (:534).
- `renderSeg` also retires voices by the core's OWN envelope (:1008) and renders
  its own saw. So the core's render path cannot drive voices that follow SCALPEL's
  voice law (B310) and RazorCore's envelope.
- `initVoice(Voice&, midi, f)` (:622) is already **public** (the `public:` at :340
  runs to :883), and `Voice` (:341) is a public type. So a caller can already own
  voices and start them. The only thing missing is ticking them.

**E1** adds, after `voiceAt()`:

```cpp
void tickVoice(Voice &s, bool lastOfSeg) { controlTick(s, lastOfSeg); }
```

The proof has two halves:

- **Codegen: by construction.** An unused non-virtual inline member is never
  emitted, so code that does not call `tickVoice()` compiles exactly as before.
  (A with/without object comparison agreed once, by hand, at the lift. It is not a
  reproducible gate, so it is not cited as one.)
- **Behaviour** (`h2_swarm_lift_check`). Core A plays a note the ordinary way. Core
  B never allocates: it copies a fresh slot as its own `Voice`, starts it with
  `initVoice`, and replays `renderSeg`'s schedule by hand. That schedule is:
  - the gravity-grid segmentation;
  - a tick where the shared 16-sample counter is 0, with `lastOfSeg` on each
    segment's final tick;
  - the per-sample phase advance, :1053-1057.

  Every dynamics observable matches bit for bit over 40 blocks at 44.1 and 48 kHz,
  with K 0.45, drift 14 ct, inertia 0.3 and onset 0.4. The observables are eff,
  vf, phase, driftS, KsmS, KsmP, KsmD, Kenv, R, RN, psi, sigma and rngState. The
  must-fail control is the same replay with every tick one sample late; it is
  caught at block 0.

  **Scope of that proof (critic L4):** it covers the DEFAULT render path only, the
  osSub == 1, no-glide, no-onset phase advance. It does NOT exercise:
  - the `fRun` frequency glide (`freqGlide` > 0);
  - the `onsD` wait-and-skip (onset scatter or per-voice envelopes);
  - 2× oversampling;
  - note travel (`glideActive`);
  - release and the envelope cull.

  Those paths belong to the core's render, which the composed layer replaces with
  its own, and are proven there when it exists.

**What the composed layer must own (not the core), even with E1:**

- **One core instance per swarm source, used for both halves.** The same `SwarmCore`
  object must `initVoice` and `tickVoice` its voices:
  - `initVoice` reads that instance's `noteCounter`, `p`, `lastPhase` and the
    ensemble stream;
  - `controlTick` reads its `x[]`, `xmin` and `centerIdx`, which that instance's
    `rebuild()` wrote.

  A voice started by one core and ticked by another would mix two swarms'
  geometry.
- **`tickVoice` is not per-voice-pure, and not thread-safe.** Besides the `Voice`, it
  writes core-shared state: `tiltHP` (the tone-tilt sign, :1857) and the law-0
  detune-ratio cache (`lawRatio`, `lawN`, `lawGen`, `lawDep`, `lawAnchor`,
  :1786-1794). The cache is keyed by value, so interleaving voices is exact on one
  thread, but two threads ticking voices of one core concurrently race on it.

- **Keep-phase.** `lastPhase` is private and is written only by `renderSeg`, from the
  core's own focus voice (:1334-1335). With keepPhase on, `initVoice` copies that
  never-updated snapshot (zeros). The composed layer overwrites the start phases
  with its own snapshot after `initVoice`. There is no draw to protect: the
  keep-phase branch draws nothing.
- **Gravity.** `gravityStep` (:802) walks the core's own `voices[]`. The composed
  engine's gravity pulls RazorCore's held voices (B335: DynSynth's law on the
  fixed-time grid), so the composed layer runs it on its own voices, as the JS
  does. The ratio set `kRatios` is reusable from the lifted core.
- **The ensemble onset stream.** The composed per-member envelope needs each
  member's drawn attack and release time factors (`max(0.15, jitter)`). The core
  keeps only the resulting coefficients (`onsC`, `relC`, :660-663), and those do
  not invert. So the core's onset path stays OFF (`onsetScatter` and `voiceEnv` at
  0 in the core), and the composed layer runs the stream itself, as the JS
  transcription does. `forcecore::rngNext` is reusable for it.
- **Everything the JS composed engine already owns beside SwarmSynth:**
  - the voice law, cap and cull (B310, B323, B375);
  - the first-tick look-ahead (B325), which snapshots a whole `Voice` (about 39 KB,
    most of it the ITD ring; preallocated per voice);
  - the ADR-184 A2 sign rules;
  - ADR-189 D1–D3;
  - φ_S = frac(φ_H + ½), and the phase advance at 1× per output sample.

## Why the composed layer is not built (decisions pending the human)

### Blocker: `razor_core.h` has no extension points

The brief requires the composed layer to sit over `h2/cores/scalpel/razor_core.h`
kept byte-stable (phase 1a's M5). The C++ RazorCore cannot be composed or
subclassed the way the JS `ComposedEngine extends RazorCore`:

- The file contains 0 `virtual`.
- Everything below `private:` at `razor_core.h:325` is private. That includes
  `startVoice`, `couple`, `settle`, `spread`, `stepM`, `scan`, `voiceOut` and
  `setOS`.
- `render()` (:1100) calls `couple`/`spread` (:1129) and `stepM` (:1215) statically,
  so an override would never be called.
- The public `noteOn` (:284-298) hard-codes the oracle's same-note reuse, which B310
  replaces.
- Voices are reachable only read-only (`voice(i) const`, :321).

The composed JS overrides 11 sites: noteOn, startVoice, couple, settle, spread,
stepM, scan, the static voice (D1), setOS, render and set. Several reach deep
into the render path:

- D1 needs a hook inside `voiceOut`;
- D2 needs one inside `scan`;
- D3 needs the loop input before `stepM`;
- the swarm drive and the per-member gains belong inside `stepM`.

Without editing the file, there are only two ways in. Both are rejected here
because they duplicate the blade path, which would take it out of the 1a harness's
protection (the purpose of M5):

- copy about 365 lines of it into the composed layer (render, stepM, out, outSerial,
  scan);
- use the explicit specialisation of the member template `render<T>`, which is
  legal C++ with private access, as a backdoor.

Options, for the human:

- (i) A sanctioned edit that makes `razor_core.h` extensible, re-proven by the 1a
  harness. Either `protected:` plus `virtual` on the hook methods (the cost of a
  virtual `stepM` per member per internal sample is to be measured), or a CRTP
  self-call.
- (ii) A separate composed port with its own parity chain.

### M1: the coupling smoother is rate-dependent in the lift, a literal in SwarmSynth (B150)

**RULED 2026-09-30 (the human: "Sounds good"), built in B382 (branch `composed-m1-ksm`):** the
composed JS mirrors B150, default ON, ledgered as `docs/port/divergences.json` M1 (flag
`ksmPerRate`; 0 is SwarmSynth's 0.08).

`swarm_core.h:389-391` sets the per-tick coupling-smoother coefficient `ksmC` from a
time constant in seconds (B150, ADR-009). It returns the literal 0.08 at 44.1 kHz
and 0.073746064208033535 at 48 kHz. SwarmSynth, which the composed engine calls for
the swarm, hard-codes `s.KsmS += (syncT - s.KsmS) * 0.08` at every rate (the same
for KsmP). The legacy parity chain never saw this: it renders at 44.1 kHz only (B378
audit §2.9).

Measured 2026-09-30 (scratch, not tracked; the method is below): `src/swarm_core.h`
(identical to the lift in arithmetic) against swarmsaw.html's SwarmSynth, one note
(57, 220 Hz), 1 s, float32 output.

| rate | K 0 | K 0.35 (SCALPEL's default K) | onset +0.5 | onset −0.5 |
|---|---|---|---|---|
| 44.1 kHz | bit-exact | bit-exact | bit-exact | rms 1.16e-1, max 3.6e-1 |
| 48 kHz | bit-exact | rms 1.44e-4, max 2.42e-3 (0.1% bit-exact) | rms 9.7e-4, max 1.6e-2 | rms 1.16e-1, max 3.6e-1 |

So at 48 kHz, the rate the 1a harness and the lab use, every patch with K ≠ 0 or
onset ≠ 0 misses the 1e-6 RMS bound by about 144× through the lifted core. Options:

- regress the lift to 0.08 per tick at every rate (a ledgered edit; it re-introduces
  the ADR-009 violation B150 removed);
- run parity at 44.1 kHz only and ledger M1 as a divergence, mirrored into the
  composed JS;
- have the composed JS mirror B150.

### M2: bipolar onset (ADR-056)

The lift's onset lock is bipolar. `Kenv = 8·onset·|onset|` (:635), and a negative
Kenv adds to the splay target (:1894-1895): a splay burst. SwarmSynth squares the
sign away (`8·onset·onset`), so onset −0.5 is a SYNC burst there. This mismatch holds
at every rate (the table's last column: rms 1.16e-1). It is reachable:
`docs/design/composed-engine-check.html:192` exposes onset −1..1. Decision: is
negative onset in scope for the composed engine's parity, and on whose law?

### M3: law 3 (a code fact)

In the lift, law 3 is the tempo grid (:1803, ADR-022). SwarmSynth has no law 3: its
chain falls through to ERB. The composed engine documents `h.law` as 0, 1, 2, 4
and 5.

**The lab UI does not offer it** (critic L6):
- `docs/design/composed-engine-check.html:199`'s select lists 0, 1, 2, 4 and 5;
- `docs/design/scalpel-interface-lab.html:1595` lists 3, marked not offered, with
  the reason at :1590 and :1716.

Saved state, host automation and `setParam` can still carry a 3, so the law for
it is still open.

### How far M1 and M2 reach (a hypothesis)

**TESTED 2026-09-30 (B382, the 48 kHz golden set: `tools/golden/gen_goldens_sr.mjs`,
`swarm48_check`, `h2_swarm48_check`).** On the whole L0-1 matrix at 48 kHz, with M1 mirrored,
every SwarmSynth scenario is within 1e-6 of both copies of the core (worst 2.474e-9); the only
other differences are negative onset (M2) and law 3 (M3). One more was found: DynSynth
(`reference/swarmdynamics.html`) carries the same per-tick 0.08 in its own smoother, and with
it replaced by M1's coefficient every dyn scenario is within 1e-6 too. The text below is the
hypothesis as it stood.

"M1 and M2 are the only 48 kHz differences between the lift and SwarmSynth" is a
HYPOTHESIS. It rests on the four scenarios in the table above, which cover one
note with K and onset only. It does not rest on the legacy chain's 156, which run
at 44.1 kHz. Drift, inertia, the laws, pivot and keep-phase at 48 kHz are
unmeasured. A 48 kHz golden set would settle it.

### The method (for re-measurement)

- The JS side extracts SwarmSynth with `tools/golden/extract_core.mjs`, calls
  `setParam('K' | 'onset', x)` then `noteOn(57, 220)`, and renders round(sr/1024)
  blocks of 1024 into Float32Arrays.
- The C++ side includes `src/swarm_core.h`, makes the same calls on `SwarmCore`,
  renders the same blocks into floats, and compares sample by sample (RMS, max,
  bit-exact share).
- Build: Apple clang `-O2 -ffp-contract=off`, Node 24.

## Decisions pending (the human's, via the lead)

1. How the composed layer attaches to the blade engine: (i) a sanctioned
   `razor_core.h` edit (the end of M5's byte-stability), or (ii) a separate composed
   port.
2. M1: parity at 44.1 kHz with M1 ledgered, the JS mirroring B150, or the lift
   regressed to 0.08 per tick.
3. M2: negative onset in or out of parity scope, and whose law.
4. M3: the lab UI never sends law 3, but state, automation and `setParam` can. Rule
   what a 3 means in the composed engine.

Until these are ruled, the swarm core in `h2/` is the legacy core, proven as such,
and not yet the composed engine's swarm.
