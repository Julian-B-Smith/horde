# SCALPEL × horde — the full accounting (B252)

> **Origin.** Written 2026-09-24 by an implementer session dispatched by the horde lead, on ROADMAP
> **B252** (PR #746, branch `lead-records-86`) and **ADR-184** (PR #745, branch `ingest-scalpel`).
> This is the working document for the structured discussion the human asked for: *"I want it to be
> an extension, but we should probably consider it more of an overhaul. It's worth a thorough
> discussion and accounting."* It proposes; it rules nothing. No code, spec, reference or ROADMAP
> file was changed to write it.
>
> **Read against.** `origin/main` at `6256301` for every existing-parameter claim (cited
> `file:line`); the packet as ingested on `origin/ingest-scalpel` at `aaa2e0e`
> (`specs/SPEC-SCALPEL.md`, `reference/scalpel/**`). Packet paths below are relative to
> `reference/scalpel/`. Two facts this document leans on are **not on `main` yet**: B232's
> off-corner rule (PR #744, open) and B240's morph tail list (dispatched, not merged). Where a row
> depends on them it says so.
>
> **Evidence classes.** *Code* = read at the cited line. *Arithmetic* = computed from the two
> formulas in scratch, not rendered. *Hypothesis* = labelled as one. Nothing here was measured by
> rendering audio.

---

## 0. Summary

**Counts.** 82 existing per-oscillator parameters (164 host ids, osc 1 + osc 2), 12 global
parameters SCALPEL overlaps, and all 107 SCALPEL parameters, each with exactly one fate:

| table | SURVIVES | MERGES | RETIRED | NEW | DEFERRED | DROPPED | total |
|---|---|---|---|---|---|---|---|
| horde per-osc (§1.1) | 64 | 13 | 5 | — | — | — | 82 |
| horde global overlaps (§1.2) | 7 | 5 | — | — | — | — | 12 |
| SCALPEL (§1.4) | — | 17 | — | 80 | 4 | 6 | 107 |

The 17 SCALPEL MERGES land on 18 horde rows (13 per-osc + 5 global). `polyMode` is the one that
needs two (32 + 34). The 80 NEW become **82 new per-oscillator rows**, 164 host ids. That is 79
numeric NEW rows, plus three rows horde needs that the packet does not name: `b1on`, `settle` and
`panBalanced` (§1.7). The 80th NEW, `kCustom`, is a string, and a string cannot be a CLAP
parameter (Q F4). **51 of the NEW rows are continuous**, and 46 of those are pad-assignable in the
packet.

**Headline findings, each argued below:**

1. **The two coupling laws are different laws, not one law normalised two ways** (§1.6). Horde
   pulls with `4K|K|·σ` Hz: quadratic in K, and proportional to the standard deviation of the
   member frequencies (`src/swarm_core.h:1885-1896`). SCALPEL pulls with
   `K·(1.5·Δf_max + 3·s)` Hz: linear in K, with a fixed 3 Hz floor (`prototype/razor-core.js:383-389`).
   At the packet presets' typical K (0.3–0.6), SCALPEL couples **≈ 2–9× harder at K 0.35 and ≈ 1–5× at K 0.6** than horde does (the gap shrinks with K because horde is quadratic).
   At zero detune it couples **9–27× harder**. SPEC §11 says to use horde's law. If that holds,
   exact parity against the JS oracle is possible only where coupling is off (Q B1, B2).
2. **Horde's default coupling is already what SCALPEL calls *cycles***. σ scales with pitch under
   the cents law, so locked lags are already pitch-independent. Horde's `absK` is the *seconds*
   option. Under horde's law, D5 goes away (§1.6.2).
3. **The member limit bites the factory bank hard.** **31 of 41** factory patches run more than 9
   members, and **5 of 41** run more than 16 (§3). Narrowing `n`'s declared range would re-map
   stored values *and* every VST3 automation lane (`libs/clap-wrapper/src/detail/vst3/parameter.cpp:123-130`).
   The proposal is a revision-gated clamp that keeps the range.
4. **The packet's defaults are not horde's legacy defaults.** Blade 1 is audible by default
   (`w` 0.25, `depth` 1). The base wave defaults to **Sine**, where horde's swarm is a saw. Start
   phases default to *settled*. SCALPEL's saw is also **half a cycle out of phase with horde's**
   (`razor-core.js:17` against `swarm_core.h:1058`). Each needs a horde default or a phase map
   before any existing patch can stay bit-identical (§1.7).
5. **Position is circular.** The morph blend and the mod matrix's clamp-to-range
   (`src/hypersaw_clap.cpp:4021-4023`) both treat it as a line, so 0.95 ↔ 0.05 blends through 0.5.
   Log-range blade rows (k, kHz, m, mHz, w) need a log-domain depth, or a route's reach depends on
   where the knob sits (§4.3).
6. **The blade envelopes and the swarm lead ψ are weak matrix sources.** Horde's matrix is global
   scope. The blade envelopes are per voice, and ψ rotates at the note frequency. The proposal:
   keep the blade envelopes internal for v1, and use R (B253) as the swarm's source instead of ψ.
   `kMaxSources` (24) is full once B253 takes slots 22/23. Growing it costs no per-tick work and
   no state-format change, and appended slots keep every saved route valid. The proposed final
   list runs to slot 29 under a cap of 32 (§4.2a).
7. **Several packet facts disagree with the packet.** `parameters.json` lists choice options in
   *display* order, not by stored value (e.g. `phaseMode` default 2 = *settled*). Several enums
   have non-contiguous values. SPEC §7 says blade 2's depth, FM depth and edges are smoothed per
   sample, but the oracle smooths them at control rate. The balanced pan tables and the *primes*
   rule stop at N = 9 (§1.8).

---

## 1. The accounting table

### 1.0 Legend

- **Fate.** SURVIVES (as is) · MERGES (with its counterpart: whose law wins, and what a revision-1
  patch hears) · RETIRED (behind the revision gate: the id stays declared and round-trips; it is
  **live at revisions below R_s and inert at R_s and above**) · NEW · DEFERRED · DROPPED.
- **R_s.** The engine revision SCALPEL's laws arrive in: the next free one after B232's
  revision 2 (ADR-183), so **3** if nothing else lands first. B229 and B213 are also queued to
  need a revision (ADR-183, *Why gate*).
- **Class** (ADR-173, `src/hypersaw_clap.cpp:1455-1461`). **M** Morphable (continuous), **S**
  Structural (stepped), **D** Device. No per-oscillator row carries an override today; the
  override table is globals only (`:1494-1587`). The class follows from `stepped`.
- **Morph.** *blends* means continuous: interpolated in BLEND, corner-picked then glided in
  QUANTUM. *snaps* means stepped: the winner's value in full (`:4087-4089`). *ramps* is the
  oscillator gate's level ramp (B48). **· B232** means that at revision ≥ 2 the off-corner rule
  applies through the oscillator gate. On PR #744, `sourceGateOf` maps **every** non-global
  per-osc row to `150 + osc·1000`. Any SCALPEL row declared per-osc inherits the rule by
  construction; nothing has to list it.
- **Mod dest.** The shell accepts any **continuous** row except 161–177 and 269–288
  (`:3541-3556`). A route's depth is a **fraction of the row's declared span**, and the result is
  clamped to the range (`:4013-4023`). *span X* = what depth 1.0 moves. The SCALPEL column gives
  the **proposed** unit where span-linear is wrong (§4.3).
- **Smoothing.** Existing rows (horde has no generic parameter smoother):
  - **RB**: step, then a swarm rebuild (`swarm_core.h:476-478`).
  - **KS**: the coupling smoother, τ 4.35 ms (`:152`, `:1896`).
  - **NO**: read at note-on (`initVoice`, `:622-702`).
  - **U**: read unsmoothed at the control tick or render segment.

  SCALPEL rows, from `data/parameters.json` `engine`:
  - **P**: one-pole 12 ms, per sample.
  - **C**: 12 ms at the 16-sample control rate.
  - **B**: discrete, at block rate.
- **Id plan.** Existing rows keep their ids; ids are frozen and append-only
  (`hypersaw_clap.cpp:75`). NEW rows take a reserved per-osc block **300–399**, with twins at
  **1300–1399** (`kOscStride` 1000, `:828`). The block is clear of every numeric range special-case
  in the shell (`:7031-7654`) and of the playbook's `baseIdOf` traps
  (`docs/playbooks/integrating-a-source.md` §1(b)). Rows are declared in `kParams` and appended to
  the morph field **through B240's tail list** in id order, each twin beside its base, with a
  layout-marker bump. Keys carry a **`bl.`** prefix because of the key collisions in §1.8.4.
  Numbers are *proposed*; they are allocated when each port phase lands.
- **Tier.** The interface priority of §2: **T1** on the face, **T2** one level down, **T3**
  Advanced.

### 1.1 Existing per-oscillator parameters (osc 1 and osc 2)

Every row not in `kGlobalIds` (`src/hypersaw_clap.cpp:844-899`) is per-oscillator, and its osc-2
twin is `id + 1000`. The table covers the SWARM panels, the saw-shape panel, the SPECTRA-only rows
(untouched; SPECTRA is parked) and the mix/pitch rows.

| # | id (osc 1 / osc 2) | key · label | range (default) | panel (tsv line) | fate | counterpart · whose law · what an old patch hears | class | morph | mod dest (depth unit) | smoothing | id plan | tier |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1 / 1001 · `hypersaw_clap.cpp:175` | `n` · Voices | 1..32 (7) | The swarm (:73) | **MERGES** | `N` (Members). Horde id, horde range 1..32 kept; the SCALPEL revision clamps the member count (§3). Rev-1/2 patch hears the same. | S | snaps | no (stepped) | RB | unchanged | T1 |
| 2 | 2 / 1002 · `hypersaw_clap.cpp:176` | `dist` · Distribution | 0..4 (1) | The swarm (:75) | **SURVIVES** | SCALPEL has no distribution (gradient only). Its *gradient* spread law ranks by member INDEX, which is pitch order only for dist 0/1 — Q F3. | S | snaps | no (stepped) | RB | unchanged | T3 |
| 3 | 3 / 1003 · `hypersaw_clap.cpp:177` | `seed` · Seed | 0..999999 (1234) | The swarm (:77) | **SURVIVES** | Every SCALPEL draw (random law, drift law, noise, settle) seeds from this, as named streams (H3). | S | snaps | no (stepped) | RB | unchanged | T3 |
| 4 | 4 / 1004 · `hypersaw_clap.cpp:184` | `detune` · Detune | 0..1 (0.28) | The swarm (:79) | **MERGES** | `detune` (cents). Horde law: knob × 100 c × x(dist). SCALPEL `detune` d c ≡ knob d/100 at dist 0 (even), law 0. Old patch hears the same. | M | blends · B232 | yes · span 1 | U | unchanged | T1 (pad) |
| 5 | 5 / 1005 · `hypersaw_clap.cpp:185` | `law` · Detune Law | 0..5 (0) | The swarm (:81) | **SURVIVES** | SCALPEL is cents-only; the other laws only move member frequencies, which the blades read, so they compose. | S | snaps | no (stepped) | RB | unchanged | T3 |
| 6 | 6 / 1006 · `hypersaw_clap.cpp:186` | `K` · Coupling | -1..1 (0) | The coupling (:83) | **MERGES** | `K`. **Horde law wins** (SPEC §11): 4K\|K\|·σ Hz pull, not SCALPEL's K·(1.5Δω_max+6π·s). At the presets' K the two differ ≈ 1–9×, up to 27× near zero detune (§1.6). Old patch hears the same. | M | blends · B232 | yes · span 2 | KS | unchanged | T1 (pad) |
| 7 | 7 / 1007 · `hypersaw_clap.cpp:187` | `onset` · Onset Lock | -1..1 (0) | The coupling (:85) | **SURVIVES** | No SCALPEL counterpart. Composes with *settle* (Q B4). | M | blends · B232 | yes · span 2 | NO | unchanged | T2 |
| 8 | 8 / 1008 · `hypersaw_clap.cpp:188` | `dissolve` · Dissolve (s) | 0.05..7.94 (0.63) | The coupling (:87) | **SURVIVES** | No SCALPEL counterpart. | M | blends · B232 | yes · span 7.89 | U | unchanged | T2 |
| 9 | 9 / 1009 · `hypersaw_clap.cpp:189` | `driftDepth` · Drift Depth (c) | 0..100 (0) | Drift (:89) | **SURVIVES** | Frequency drift. Not SCALPEL's *drift* spread law (that walks blade offsets, not pitch). | M | blends · B232 | yes · span 100 | U | unchanged | T2 |
| 10 | 10 / 1010 · `hypersaw_clap.cpp:191` | `driftRate` · Drift Rate | 0..1 (0.4) | Drift (:91) | **SURVIVES** | Key `driftRate` COLLIDES with SCALPEL's spread-drift key (§1.8). | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 11 | 12 / 1012 · `hypersaw_clap.cpp:193` | `rtone` · Coherence -> Tone | -1..1 (0) | Image & tone (:94) | **SURVIVES** | No counterpart; R also becomes a mod source (B253). | M | blends · B232 | yes · span 2 | U | unchanged | T2 |
| 12 | 13 / 1013 · `hypersaw_clap.cpp:194` | `normExp` · Density Comp | 0.5..1 (0.75) | Image & tone (:96) | **SURVIVES** | SCALPEL sums /√N = normExp 0.5; horde default 0.75 wins (D11). | M | blends · B232 | yes · span 0.5 | U | unchanged | T3 |
| 13 | 14 / 1014 · `hypersaw_clap.cpp:195` | `width` · Width | 0..1.5 (0.8) | Image & tone (:98) | **MERGES** | `width` (Stereo 0..1). Horde law (0..1.5, super-width) wins. | M | blends · B232 | yes · span 1.5 | RB | unchanged | T2 |
| 14 | 16 / 1016 · `hypersaw_clap.cpp:197` | `digital` · Digital | 0..1 (1) | Image & tone (:101) | **MERGES** | `aa` (clean/raw). Horde's continuous BLEP amount: clean = 1, raw = 0; proposed to scale every BLEP correction the blades add. | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 15 | 17 / 1017 · `hypersaw_clap.cpp:198` | `vol` · Volume | 0..1 (0.4) | Mix (:103) | **MERGES** | `gain`. Horde law (0.9·vol/n^normExp into the core's tanh, swarm_core.h:943, :1331) wins; SCALPEL's tanh(1.6·gain·y) is dropped (D11). | M | blends · B232 | yes · span 1 | U | unchanged | T1 (mix) |
| 16 | 18 / 1018 · `hypersaw_clap.cpp:199` | `retrig` · Retrigger | 0..1 (1) | The coupling (:105) | **MERGES** | `phaseMode`: *random* ≡ retrig off, *aligned* ≡ retrig on (horde phase origin, §1.6.6). *settled* is a NEW row (`settle`). | S | snaps | no (stepped) | NO | unchanged | T2 |
| 17 | 19 / 1019 · `hypersaw_clap.cpp:201` | `attack` · Attack (s) | 0.001..2 (0.003) | Envelope (:107) | **MERGES** | `A` (ms, linear attack). Horde's one-pole seconds law wins. | M | blends · B232 | yes · span 1.999 | U | unchanged | T2 |
| 18 | 20 / 1020 · `hypersaw_clap.cpp:202` | `decay` · Decay (s) | 0.005..4 (0.16) | Envelope (:109) | **MERGES** | `D` (ms). Horde law wins. | M | blends · B232 | yes · span 3.995 | U | unchanged | T2 |
| 19 | 21 / 1021 · `hypersaw_clap.cpp:203` | `sustain` · Sustain | 0..1 (1) | Envelope (:111) | **MERGES** | `S`. Same meaning. | M | blends · B232 | yes · span 1 | U | unchanged | T2 |
| 20 | 22 / 1022 · `hypersaw_clap.cpp:204` | `release` · Release (s) | 0.005..8 (0.16) | Envelope (:113) | **MERGES** | `R` (ms). Horde law wins. | M | blends · B232 | yes · span 7.995 | U | unchanged | T2 |
| 21 | 23 / 1023 · `hypersaw_clap.cpp:206` | `beatMult` · Grid Cycles/Beat | 0.25..8 (1) | The swarm (:115) | **SURVIVES** | Tempo-grid law only. | M | blends · B232 | yes · span 7.75 | U | unchanged | T3 |
| 22 | 24 / 1024 · `hypersaw_clap.cpp:208` | `topo` · Topology | 0..2 (0) | Dynamics (:117) | **SURVIVES** | Topologies compose: ψ (the lead) is computed before the topology branch (swarm_core.h:1911-1921). | S | snaps | no (stepped) | RB | unchanged | T3 |
| 23 | 25 / 1025 · `hypersaw_clap.cpp:209` | `reach` · Ring Reach | 1..8 (5) | Dynamics (:119) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T3 |
| 24 | 26 / 1026 · `hypersaw_clap.cpp:210` | `mu` · Cluster Link | 0..1 (0.6) | Dynamics (:121) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 25 | 27 / 1027 · `hypersaw_clap.cpp:211` | `alpha` · Phase Lag | -90..90 (0) | Dynamics (:123) | **SURVIVES** | Sakaguchi α composes with *swarm frame* (SPEC §11). | M | blends · B232 | yes · span 180 | U | unchanged | T3 |
| 26 | 28 / 1028 · `hypersaw_clap.cpp:212` | `poles` · Poles q | 1..4 (1) | Dynamics (:125) | **SURVIVES** | Daido poles q ≠ SCALPEL's H harmonics (§1.6.4). | S | snaps | no (stepped) | U | unchanged | T3 |
| 27 | 29 / 1029 · `hypersaw_clap.cpp:213` | `grav` · Gravity | 0..1 (0) | Dynamics (:127) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 28 | 30 / 1030 · `hypersaw_clap.cpp:214` | `basin` · Basin (c) | 10..50 (35) | Dynamics (:129) | **SURVIVES** |  | M | blends · B232 | yes · span 40 | U | unchanged | T3 |
| 29 | 31 / 1031 · `hypersaw_clap.cpp:215` | `absK` · Absolute Coupling | 0..1 (0) | Dynamics (:131) | **MERGES** | `cScale`: absK on (fixed 2.5 Hz units) ≈ *seconds*; horde's default σ-scaling is already pitch-proportional ≈ *cycles* (§1.6.2). | S | snaps | no (stepped) | U | unchanged | T3 |
| 30 | 35 / 1035 · `hypersaw_clap.cpp:221` | `octave` · Octave | -2..2 (0) | Pitch (:136) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T2 |
| 31 | 36 / 1036 · `hypersaw_clap.cpp:224` | `semi` · Semitones | -12..12 (0) | Pitch (:138) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T2 |
| 32 | 37 / 1037 · `hypersaw_clap.cpp:225` | `fineCents` · Fine (c) | -100..100 (0) | Pitch (:140) | **SURVIVES** |  | M | blends · B232 | yes · span 200 | U | unchanged | T2 |
| 33 | 39 / 1039 · `hypersaw_clap.cpp:227` | `scatter` · Phase Scatter | 0..1 (0) | The coupling (:143) | **SURVIVES** | Partial random start; *settle* runs after it. | M | blends · B232 | yes · span 1 | NO | unchanged | T3 |
| 34 | 42 / 1042 · `hypersaw_clap.cpp:232` | `panScatter` · Pan Scatter | 0..1 (0) | Image & tone (:148) | **SURVIVES** | Composes with the balanced order (permutes its slots). | M | blends · B232 | yes · span 1 | RB | unchanged | T3 |
| 35 | 43 / 1043 · `hypersaw_clap.cpp:236` | `engine` · Engine | 0..1 (0) | The swarm (:150) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 36 | 44 / 1044 · `hypersaw_clap.cpp:237` | `partials` · Partials | 1..32 (12) | Spectra (:152) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 37 | 45 / 1045 · `hypersaw_clap.cpp:238` | `tilt` · Amp Tilt | 0.5..2 (1) | Spectra (:154) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1.5 | U | unchanged | n/a |
| 38 | 46 / 1046 · `hypersaw_clap.cpp:239` | `stretch` · Stretch | 0..1 (0) | Spectra (:156) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 39 | 47 / 1047 · `hypersaw_clap.cpp:240` | `cloud` · Cloud Voices | 1..7 (5) | Spectra (:158) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 40 | 48 / 1048 · `hypersaw_clap.cpp:241` | `cwidth` · Cloud Width | 0..1 (0.25) | Spectra (:160) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 41 | 49 / 1049 · `hypersaw_clap.cpp:242` | `wtilt` · Width Tilt | -1..1 (0) | Spectra (:162) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 2 | U | unchanged | n/a |
| 42 | 50 / 1050 · `hypersaw_clap.cpp:243` | `wlaw` · Width Law | 0..1 (0) | Spectra (:164) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 43 | 51 / 1051 · `hypersaw_clap.cpp:244` | `cascade` · Cascade | 0..1 (0) | Spectra (:166) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 44 | 52 / 1052 · `hypersaw_clap.cpp:246` | `subOn` · Sub Osc | 0..1 (0) | Spectra (:168) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 45 | 53 / 1053 · `hypersaw_clap.cpp:247` | `subVol` · Sub Level | 0..1 (0) | Spectra (:170) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 46 | 54 / 1054 · `hypersaw_clap.cpp:248` | `subWave` · Sub Wave | 0..1 (0) | Spectra (:172) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 47 | 55 / 1055 · `hypersaw_clap.cpp:249` | `subOct` · Sub Octave | -3..-1 (-1) | Spectra (:174) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | S | snaps | no (stepped) | U | unchanged | n/a |
| 48 | 56 / 1056 · `hypersaw_clap.cpp:252` | `balance` · A/B Balance | 0..1 (0) | Dynamics (:176) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 49 | 65 / 1065 · `hypersaw_clap.cpp:271` | `sAttack` · S.Attack (s) | 0.001..2 (0.004) | Envelope (:186) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1.999 | U | unchanged | n/a |
| 50 | 66 / 1066 · `hypersaw_clap.cpp:272` | `sDecay` · S.Decay (s) | 0.005..4 (0.18) | Envelope (:188) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 3.995 | U | unchanged | n/a |
| 51 | 67 / 1067 · `hypersaw_clap.cpp:273` | `sSustain` · S.Sustain | 0..1 (1) | Envelope (:190) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 1 | U | unchanged | n/a |
| 52 | 68 / 1068 · `hypersaw_clap.cpp:274` | `sRelease` · S.Release (s) | 0.005..8 (0.18) | Envelope (:192) | **SURVIVES** | SPECTRA-only (engine 1, parked); untouched by SCALPEL. | M | blends · B232 | yes · span 7.995 | U | unchanged | n/a |
| 53 | 69 / 1069 · `hypersaw_clap.cpp:280` | `shape` · Squareness | 0..1 (0) | Saw shape (:194) | **RETIRED** | Ratified (H2). Base wave *Square* is the nearest successor, not a mapping. | M | blends · B232 | rev ≥ R_s: withdraw | U | declared, inert at rev ≥ R_s; live at rev < R_s | — |
| 54 | 71 / 1071 · `hypersaw_clap.cpp:293` | `toneTilt` · Tone Tilt | -1..1 (0) | Image & tone (:196) | **SURVIVES** |  | M | blends · B232 | yes · span 2 | U | unchanged | T3 |
| 55 | 72 / 1072 · `hypersaw_clap.cpp:294` | `hiTame` · Hi Tame | 0..1 (0) | Image & tone (:198) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 56 | 73 / 1073 · `hypersaw_clap.cpp:295` | `driftMode` · Drift Mode | 0..2 (0) | Drift (:200) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T3 |
| 57 | 74 / 1074 · `hypersaw_clap.cpp:296` | `keepPhase` · Keep Phase | 0..1 (0) | Drift (:202) | **SURVIVES** |  | S | snaps | no (stepped) | NO | unchanged | T3 |
| 58 | 76 / 1076 · `hypersaw_clap.cpp:298` | `panMotion` · Pan Motion | 0..1 (0) | Image & tone (:205) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 59 | 77 / 1077 · `hypersaw_clap.cpp:299` | `panMode` · Pan Motion Mode | 0..1 (0) | Image & tone (:207) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T3 |
| 60 | 78 / 1078 · `hypersaw_clap.cpp:300` | `motionCenter` · Centre Pin | 0..1 (0) | Drift (:209) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 61 | 79 / 1079 · `hypersaw_clap.cpp:301` | `harmReach` · Harmonic Reach | 0.25..4 (1) | The swarm (:211) | **SURVIVES** |  | M | blends · B232 | yes · span 3.75 | U | unchanged | T3 |
| 62 | 80 / 1080 · `hypersaw_clap.cpp:302` | `stretchB` · Stretch B | 0..6 (0) | The swarm (:213) | **SURVIVES** |  | M | blends · B232 | yes · span 6 | U | unchanged | T3 |
| 63 | 81 / 1081 · `hypersaw_clap.cpp:303` | `spread` · Octave Spread | 1..24 (1) | The swarm (:215) | **SURVIVES** |  | M | blends · B232 | yes · span 23 | U | unchanged | T3 |
| 64 | 82 / 1082 · `hypersaw_clap.cpp:304` | `anchor` · Root Anchor | 0..1 (0) | The swarm (:217) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | U | unchanged | T3 |
| 65 | 83 / 1083 · `hypersaw_clap.cpp:305` | `pivotMode` · Pivot | 0..1 (0) | Dynamics (:219) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T3 |
| 66 | 84 / 1084 · `hypersaw_clap.cpp:306` | `panLayout` · Pan Image | 0..1 (0) | Image & tone (:221) | **MERGES** | `panOrder`. Horde's pitch-ranked alternating fan stays the rev-1 law; *balanced* arrives as a NEW row (`panBalanced`), not a widened 84 (§1.8.5). | S | snaps | no (stepped) | RB | unchanged | T3 |
| 67 | 85 / 1085 · `hypersaw_clap.cpp:307` | `panCurve` · Fan Curve | 0..1 (0.5) | Image & tone (:223) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | RB | unchanged | T3 |
| 68 | 86 / 1086 · `hypersaw_clap.cpp:308` | `panInvert` · Fan Invert | 0..1 (0) | Image & tone (:225) | **SURVIVES** |  | S | snaps | no (stepped) | RB | unchanged | T3 |
| 69 | 87 / 1087 · `hypersaw_clap.cpp:312` | `superMode` · Super-Width Mode | 0..2 (0) | Image & tone (:227) | **SURVIVES** |  | S | snaps | no (stepped) | RB | unchanged | T3 |
| 70 | 91 / 1091 · `hypersaw_clap.cpp:327` | `onsetScatter` · Onset Scatter (ms) | 0..80 (0) | Onset & scatter (:232) | **SURVIVES** |  | M | blends · B232 | yes · span 80 | NO | unchanged | T3 |
| 71 | 92 / 1092 · `hypersaw_clap.cpp:328` | `onsetAlpha` · Timing Correction | 0..1.5 (0.25) | Onset & scatter (:234) | **SURVIVES** |  | M | blends · B232 | yes · span 1.5 | NO | unchanged | T3 |
| 72 | 93 / 1093 · `hypersaw_clap.cpp:329` | `attackScatter` · Attack Scatter | 0..1 (0) | Onset & scatter (:236) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | NO | unchanged | T3 |
| 73 | 94 / 1094 · `hypersaw_clap.cpp:331` | `voiceEnv` · Per-Partial Env | 0..1 (0) | Onset & scatter (:238) | **SURVIVES** |  | S | snaps | no (stepped) | NO | unchanged | T3 |
| 74 | 95 / 1095 · `hypersaw_clap.cpp:332` | `relScatter` · Release Scatter | 0..1 (0) | Onset & scatter (:240) | **SURVIVES** |  | M | blends · B232 | yes · span 1 | NO | unchanged | T3 |
| 75 | 104 / 1104 · `hypersaw_clap.cpp:374` | `oscMute` · Mute | 0..1 (0) | (ungrouped) (:251) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T2 (mix) |
| 76 | 105 / 1105 · `hypersaw_clap.cpp:375` | `oscSolo` · Solo | 0..1 (0) | (ungrouped) (:253) | **SURVIVES** |  | S | snaps | no (stepped) | U | unchanged | T2 (mix) |
| 77 | 129 / 1129 · `hypersaw_clap.cpp:424` | `sawBase` · Saw Base | 0..1 (0) | Saw shape (:278) | **RETIRED** | Ratified (H2). | M | blends · B232 | rev ≥ R_s: withdraw | U | declared, inert at rev ≥ R_s; live at rev < R_s | — |
| 78 | 130 / 1130 · `hypersaw_clap.cpp:425` | `sawProfile` · Roundness Shape | 0..1 (0) | Saw shape (:279) | **RETIRED** | Ratified (H2). | M | blends · B232 | rev ≥ R_s: withdraw | U | declared, inert at rev ≥ R_s; live at rev < R_s | — |
| 79 | 131 / 1131 · `hypersaw_clap.cpp:426` | `round` · Roundness | 0..1 (0) | Saw shape (:280) | **RETIRED** | Ratified (H2). | M | blends · B232 | rev ≥ R_s: withdraw | U | declared, inert at rev ≥ R_s; live at rev < R_s | — |
| 80 | 132 / 1132 · `hypersaw_clap.cpp:435` | `roundHi` · Round x Pitch | -1..1 (0) | Saw shape (:281) | **RETIRED** | Ratified (H2). | M | blends · B232 | rev ≥ R_s: withdraw | U | declared, inert at rev ≥ R_s; live at rev < R_s | — |
| 81 | 150 / 1150 · `hypersaw_clap.cpp:497` | `enable` · Osc On | 0..1 (1) | The swarm (:71) | **SURVIVES** | The oscillator's gate: B232's off-corner rule keys every SCALPEL per-osc row to it. | S | ramps (B48 level ramp) | no (stepped) | U | unchanged | T1 (tab power) |
| 82 | 181 / 1181 · `hypersaw_clap.cpp:655` | `oscPitch` · Pitch (cont.) | -24..24 (0) | Pitch (:393) | **SURVIVES** |  | M | blends · B232 | yes · span 48 | U | unchanged | T2 |

### 1.2 Global parameters SCALPEL overlaps

SCALPEL's Voice group is per-engine. Horde's voicing, glide and oversampling are **global**
(`kGlobalIds` 32, 33, 34, 88, 90), so the merged rows govern both oscillators.

| # | id | key · label | range (default) | panel (tsv line) | fate | counterpart · whose law | class | morph field | mod dest | smoothing |
|---|---|---|---|---|---|---|---|---|---|---|
| G1 | 32 · `hypersaw_clap.cpp:218` | `voiceMono` · Mono | 0..1 (0) | Bend (:133) | **MERGES** | `polyMode` (with 34): poly = Mono off; mono = Mono on + Legato off; legato = Mono on + Legato on. Horde's shell voicing wins; it is GLOBAL, so both oscillators share it. | S | in field (ADR-109 A1 append) | no (stepped) | U |
| G2 | 34 · `hypersaw_clap.cpp:220` | `voiceLegato` · Legato | 0..1 (1) | Bend (:135) | **MERGES** | `polyMode` (with 32). | S | in field (ADR-109 A1 append) | no (stepped) | U |
| G3 | 33 · `hypersaw_clap.cpp:219` | `glide` · Note Lag (s) | 0..2 (0) | Bend (:134) | **MERGES** | `glide`: SCALPEL's exponential-in-log-f glide IS horde's lag law in semitones; SCALPEL time-to-95 % T ms ≡ horde τ = T/3000 s (razor-core.js:669, `gk = 1−exp(−3/(glide·sr))`). Horde steps it on the 16-sample tick; SCALPEL per sample. | M | in field (note-lane append) | yes · span 2 | U |
| G4 | 90 · `hypersaw_clap.cpp:323` | `glideMode` · Glide From | 0..2 (0) | Bend (:231) | **MERGES** | `glideAlways`: *overlapping* ≡ 0 held note; *always* ≡ 2. Horde's third mode (1, last note ringing) has no SCALPEL twin. | S | in field (ADR-109 A1 append) | no (stepped) | U |
| G5 | 88 · `hypersaw_clap.cpp:316` | `oversample` · Oversample 2x | 0..1 (0) | Output & perception (:229) | **MERGES** | `os` (bench-only in the packet). Horde's 2× (halfband decimator) is GLOBAL and ships OFF; SCALPEL's alias claims assume 2× with a Butterworth pair. Q D2. | S | not in field | no (stepped) | RB |
| G6 | 11 · `hypersaw_clap.cpp:192` | `inertia` · Inertia | 0..1 (0) | The coupling (:93) | **SURVIVES** | Second-order member inertia; composes with coupling. No counterpart. | M | in field | yes · span 1 | U |
| G7 | 70 · `hypersaw_clap.cpp:353` | `inertiaCurve` · Inertia Curve (dev) | 0.3..5 (2.5) | The coupling (:246) | **SURVIVES** | (dev) inertia taper. | M | in field | yes · span 4.7 | U |
| G8 | 15 · `hypersaw_clap.cpp:196` | `mono` · Mono Fold | 0..1 (0) | Output & perception (:100) | **SURVIVES** | Mono fold. No counterpart. | S | not in field | no (stepped) | U |
| G9 | 75 · `hypersaw_clap.cpp:297` | `freqGlide` · Freq Glide (s) | 0..0.1 (0) | Drift (:204) | **SURVIVES** | Per-member frequency glide. No counterpart. | M | in field | yes · span 0.1 | U |
| G10 | 89 · `hypersaw_clap.cpp:322` | `polyGlide` · Poly Glide (dev) | 0..1 (1) | Bend (:230) | **SURVIVES** | (dev) vestigial, Device. | D | not in field | no (stepped) | U |
| G11 | 100 · `hypersaw_clap.cpp:359` | `masterVol` · Master Volume | 0..1.5 (1) | Mix (:247) | **SURVIVES** | Master volume. No counterpart. | D | not in field (Device) | yes · span 1.5 | U |
| G12 | 160 · `hypersaw_clap.cpp:582` | `voiceCull` · Voice Cull | -80..-40 (-80) | Performance (:312) | **SURVIVES** | Voice cull. No counterpart. | S | not in field | yes · span 40 | U |

The note-travel lane (137–145, global) SURVIVES untouched. SCALPEL's glide is one law: the lag,
which is `noteLaw` 3 and `glide` 33's meaning (`hypersaw_clap.cpp:444-460`).

### 1.3 Sync

**No swarm hard-sync parameter exists to retire.** B228's sync lab was superseded by B238, and no
sync row was ever ported into the swarm. The sub's `sub.sync` (4011) is already RETIRED in place
(B184, `hypersaw_clap.cpp:1261`) and is outside this accounting. SCALPEL covers classic hard sync
as a special case: Mode *Sync*, Width 1, Cut rate in × f₀ per cycle.

### 1.4 SCALPEL parameters (all 107, `data/parameters.json`)

The *stored values* column reads the bench's own `seg(...)` definitions
(`prototype/scalpel-bench.html:1238-1376`), because `parameters.json` lists options in display
order (§1.8.1).

| # | key · label | group | kind: range or stored values (bench default) | curve | fate | counterpart · whose law · note | class | morph | mod dest (proposed unit) | smoothing | id plan (proposed base; osc 2 = +1000) | tier |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `w` · Width | Blade 1 | continuous: 0..1 (0.25) | log with bypass zone | **NEW** | Bench default 0.25 = an audible blade; horde needs blade 1 inert by default (§1.7). | M | blends · B232 | yes · octaves (log; the spec's own 2^(±3·level) law) | P | 303 / 1303 · key `bl.w` | T1 |
| 2 | `k` · Cut rate k | Blade 1 | continuous: 1..64 (6) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | P | 304 / 1304 · key `bl.k` | T1 |
| 3 | `kHz` · Cut rate Hz | Blade 1 | continuous: 20..12000 (1320) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | P | 305 / 1305 · key `bl.kHz` | T1 |
| 4 | `c` · Position | Blade 1 | continuous: 0..1 (0.875) | linear | **NEW** | Circular: morph and matrix must wrap (§4.3). | M | blends SHORTEST ARC (needs a circular rule) · B232 | yes · cycles, WRAPPING (circular; the linear clamp is wrong, §4.3) | P | 307 / 1307 · key `bl.c` | T1 |
| 5 | `rotRate` · Rotate | Blade 1 | continuous: -4..4 (0) | linear | **NEW** |  | M | blends · B232 | yes · Hz, linear | C | 312 / 1312 · key `bl.rotRate` | T2 |
| 6 | `rotSync` · Rotation | Both blades | choice: 1=restart per note, 0=free-running (1) |  | **NEW** |  | S | snaps | no (stepped) | B | 313 / 1313 · key `bl.rotSync` | T3 |
| 7 | `hard` · Edges | Blade 1 | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | P | 308 / 1308 · key `bl.hard` | T1 |
| 8 | `depth` · Depth | Blade 1 | continuous: 0..1 (1) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | P | 309 / 1309 · key `bl.depth` | T1 |
| 9 | `mirror` · Mirror | Blade 1 | choice: 0=off, 1=reflect, 2=twin −, 3=twin + (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 310 / 1310 · key `bl.mirror` | T2 |
| 10 | `mode` · Mode | Blade 1 | choice: 0=Sync, 1=FM reset, 2=FM free, 3=Noise, 4=Fold, 5=Ring, 6=Crush (0) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 301 / 1301 · key `bl.mode` | T1 |
| 11 | `hot` · Blade wave | Blade 1 | choice: 0=Sine, 1=Tri, 2=Saw, 4=Rev saw, 3=Square, 6=Sine→Saw (2) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 302 / 1302 · key `bl.hot` | T1 |
| 12 | `morph` · Shape | Blade 1 | continuous: 0..1 (0.5) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 311 / 1311 · key `bl.morph` | T2 |
| 13 | `fmType` · FM type | Blade 1 FM | choice: 0=phase, 1=pitch (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 315 / 1315 · key `bl.fmType` | T2 |
| 14 | `mshape` · Mod shape | Blade 1 FM | choice: 0=Sine, 1=Tri, 2=Saw, 4=Rev saw, 3=Square, 5=Smooth noise, 7=S&H noise (0) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 316 / 1316 · key `bl.mshape` | T2 |
| 15 | `I` · FM depth | Blade 1 FM | continuous: 0..10 (2) | power | **NEW** |  | M | blends · B232 | yes · span 10 (index) | P | 317 / 1317 · key `bl.I` | T1 |
| 16 | `fb` · Feedback | Both blades | continuous: 0..1 (0) | power | **NEW** |  | M | blends · B232 | yes · span 1 | C | 314 / 1314 · key `bl.fb` | T2 |
| 17 | `m` · Mod rate | Blade 1 FM | continuous: 0.25..24 (3.37) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | C | 319 / 1319 · key `bl.m` | T1 |
| 18 | `mHz` · Mod rate Hz | Blade 1 FM | continuous: 1..20000 (660) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | C | 320 / 1320 · key `bl.mHz` | T1 |
| 19 | `mUnit` · Mod rate in | Blade 1 FM | choice: 0=× f₀, 1=Hz (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 318 / 1318 · key `bl.mUnit` | T2 |
| 20 | `lock` · Cut rate in | Blade 1 | choice: 0=× f₀ per cycle, 1=per blade, 2=Hz (fixed) (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 306 / 1306 · key `bl.lock` | T2 |
| 21 | `N` · Members | Swarm | continuous: 1..9 (5) | linear | **MERGES** | → `n` (1/1001). Horde id and law; revision-gated cap (§3). | as horde row | as horde row | as horde row | B | — | T1 |
| 22 | `detune` · Detune | Swarm | continuous: 0..100 (14) | linear | **MERGES** | → `detune` (4). Horde law; d c ≡ knob d/100 at dist 0. | as horde row | as horde row | as horde row | C | — | T1 (pad) |
| 23 | `K` · Coupling K | Swarm | continuous: -1..1 (0.35) | linear | **MERGES** | → `K` (6). Horde law (§1.6); SCALPEL presets need re-voicing. | as horde row | as horde row | as horde row | C | — | T1 (pad) |
| 24 | `width` · Stereo | Swarm | continuous: 0..1 (0.7) | linear | **MERGES** | → `width` (14). Horde law. | as horde row | as horde row | as horde row | C | — | T2 |
| 25 | `panOrder` · Pan order | Swarm | choice: 0=balanced, 1=fan (0) |  | **MERGES** | → pan family (84). *balanced* ships as NEW row `panBalanced` (tables exist for N ≤ 9 only). | as horde row | as horde row | as horde row | B | new row 331 | T3 |
| 26 | `xm` · Cross-mod | Swarm | continuous: 0..1 (0) | power | **NEW** |  | M | blends · B232 | yes · span 1 | C | 328 / 1328 · key `bl.xm` | T2 |
| 27 | `phaseMode` · Start phases | Swarm | choice: 2=settled, 0=random, 1=aligned (2) |  | **MERGES** | → `retrig` (18) for random/aligned; *settled* ships as NEW row `settle`. | as horde row | as horde row | as horde row | B | new row 330 | T2 |
| 28 | `frame` · Blade frame | Swarm | choice: 0=member, 1=swarm (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 329 / 1329 · key `bl.frame` | T2 |
| 29 | `frame2` · Blade 2 frame | Swarm | choice: -1=same as blade 1, 0=member, 1=swarm (-1) |  | **NEW** | −1 = *same as blade 1* sentinel; range −1..max. | S | snaps | no (stepped) | B | 358 / 1358 · key `bl.frame2` | T3 |
| 30 | `cScale` · Coupling time | Swarm | choice: 0=seconds, 1=cycles (0) |  | **MERGES** | → `absK` (31) under horde's law (§1.6.2). Reopens if SCALPEL's law is chosen (Q B1). | as horde row | as horde row | as horde row | B | — | T3 |
| 31 | `law` · Law | Spread across members | choice: 0=gradient, 1=random, 4=drift, 2=alternate, 3=swarm (0) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 332 / 1332 · key `bl.law` | T2 |
| 32 | `driftRate` · Drift rate | Spread across members | continuous: 0.02..12 (0.5) | log | **NEW** | Spread-drift rate. Key collides with horde `driftRate` (10) → prefix. | M | blends · B232 | yes · octaves (log) | C | 333 / 1333 · key `bl.driftRate` | T3 |
| 33 | `bspread` · Position | Spread across members | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 334 / 1334 · key `bl.bspread` | T2 |
| 34 | `kspread` · Cut rate | Spread across members | continuous: 0..24 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 24 (harmonics) | C | 335 / 1335 · key `bl.kspread` | T2 |
| 35 | `kRule` · Cut rule | Spread across members | choice: 0=even, 1=harmonic, 2=undertone, 3=octaves, 4=major, 5=minor, 6=fifths, 7=golden, 8=primes, 9=custom (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 336 / 1336 · key `bl.kRule` | T2 |
| 36 | `kRuleAmt` · Rule depth | Spread across members | continuous: 0..1 (1) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 337 / 1337 · key `bl.kRuleAmt` | T2 |
| 37 | `kCustom` · Ratios | Spread across members | other: text (1, 5/4, 3/2) |  | **NEW** | A string cannot be a CLAP parameter. | state | snaps with its corner (proposed) | no | B | NOT a CLAP param: a per-osc state string `bl.ratios` (Q F4) | T3 |
| 38 | `kRuleOut` · Members | Spread across members | other: display only |  | **DROPPED** | read-only display of the rule's member ratios (engine: "UI / bench only"); the GUI can recompute it from kRule/N. | — | — | — | UI | none | — |
| 39 | `kq` · Quantize | Spread across members | choice: 0=free, 1=whole (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 338 / 1338 · key `bl.kq` | T3 |
| 40 | `wspread` · Width | Spread across members | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 339 / 1339 · key `bl.wspread` | T3 |
| 41 | `dspread` · Depth | Spread across members | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 340 / 1340 · key `bl.dspread` | T3 |
| 42 | `mspread` · Shape | Spread across members | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 341 / 1341 · key `bl.mspread` | T3 |
| 43 | `ispread` · FM index | Spread across members | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 342 / 1342 · key `bl.ispread` | T3 |
| 44 | `rotSpread` · Rotate | Spread across members | continuous: 0..2 (0) | power | **NEW** |  | M | blends · B232 | yes · span 2 (Hz) | C | 343 / 1343 · key `bl.rotSpread` | T3 |
| 45 | `base` · Base wave | Voice | choice: 0=Sine, 1=Tri, 2=Saw, 4=Rev saw, 3=Square (0) |  | **NEW** | Horde default must be **Saw** (value 2), not the bench's Sine (§1.7). | S | snaps | no (stepped) | B | 326 / 1326 · key `bl.base` | T1 |
| 46 | `A` · Attack | Voice | continuous: 1..2000 (4) | log | **MERGES** | → `attack` (19). Horde law (s, one-pole). | as horde row | as horde row | as horde row | C | — | T2 |
| 47 | `D` · Decay | Voice | continuous: 10..4000 (400) | log | **MERGES** | → `decay` (20). Horde law. | as horde row | as horde row | as horde row | C | — | T2 |
| 48 | `S` · Sustain | Voice | continuous: 0..1 (0.85) | linear | **MERGES** | → `sustain` (21). | as horde row | as horde row | as horde row | C | — | T2 |
| 49 | `R` · Release | Voice | continuous: 10..5000 (280) | log | **MERGES** | → `release` (22). Horde law. | as horde row | as horde row | as horde row | C | — | T2 |
| 50 | `f0Mode` · Drone pitch in | Drone and output | choice: 0=Hz, 1=notes (0) |  | **DROPPED** | the bench's drone test-tone; horde is played from MIDI. | — | — | — | UI | none | — |
| 51 | `f0` · Drone pitch | Drone and output | continuous: 27.5..880 (110) | log | **DROPPED** | drone pitch (bench test-tone). | — | — | — | UI | none | — |
| 52 | `f0Note` · Drone note | Drone and output | continuous: 12..108 (45) | linear | **DROPPED** | drone note (bench test-tone). | — | — | — | UI | none | — |
| 53 | `nameAbleton` · Note names | Drone and output | choice: 1=C3 = middle C, 0=C4 = middle C (1) |  | **DROPPED** | note-name display convention of the bench; horde's GUI owns note naming. | — | — | — | UI | none | — |
| 54 | `gain` · Volume | Voice | continuous: 0..1 (0.35) | linear | **MERGES** | → `vol` (17). Horde output stage (D11). | as horde row | as horde row | as horde row | C | — | T1 (mix) |
| 55 | `polyMode` · Voicing | Voice | choice: 0=poly, 1=mono, 2=legato (0) |  | **MERGES** | → `voiceMono` (32) + `voiceLegato` (34), GLOBAL. | as horde row | as horde row | as horde row | B | — | T3 |
| 56 | `glide` · Glide | Voice | continuous: 0..2000 (60) | power | **MERGES** | → `glide` (33): τ = T/3000 s. | as horde row | as horde row | as horde row | C | — | T3 |
| 57 | `glideAlways` · Glide when | Voice | choice: 0=overlapping, 1=always (0) |  | **MERGES** | → `glideMode` (90). | as horde row | as horde row | as horde row | B | — | T3 |
| 58 | `os` · Oversample | Drone and output | choice: 1=1×, 2=2× (2) |  | **MERGES** | → `oversample` (88, global). Q D2. | as horde row | as horde row | as horde row | UI | — | T3 |
| 59 | `aa` · Band-limit | Drone and output | choice: 1=clean, 0=raw (1) |  | **MERGES** | → `digital` (16). | as horde row | as horde row | as horde row | B | — | T3 |
| 60 | `b2on` · Blade 2 | Blade 2 | choice: 0=off, 1=on (0) |  | **NEW** |  | S | snaps — proposed RAMP like 150 (Q G2) | no (stepped) | B | 344 / 1344 · key `bl.b2on` | T1 |
| 61 | `mode2` · Mode | Blade 2 | choice: 0=Sync, 1=FM reset, 2=FM free, 3=Noise, 4=Fold, 5=Ring, 6=Crush (4) |  | **NEW** |  | S | snaps | no (stepped) | B | 345 / 1345 · key `bl.mode2` | T1 |
| 62 | `hot2` · Blade wave | Blade 2 | choice: 0=Sine, 1=Tri, 2=Saw, 4=Rev saw, 3=Square, 6=Sine→Saw (2) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 346 / 1346 · key `bl.hot2` | T1 |
| 63 | `w2` · Width | Blade 2 | continuous: 0..1 (0.2) | log with bypass zone | **NEW** |  | M | blends · B232 | yes · octaves (log; the spec's own 2^(±3·level) law) | P | 347 / 1347 · key `bl.w2` | T1 |
| 64 | `k2` · Cut rate k | Blade 2 | continuous: 1..64 (3) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | P | 348 / 1348 · key `bl.k2` | T1 |
| 65 | `kHz2` · Cut rate Hz | Blade 2 | continuous: 20..12000 (800) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | P | 349 / 1349 · key `bl.kHz2` | T1 |
| 66 | `lock2` · Cut rate in | Blade 2 | choice: -1=same as blade 1, 0=× f₀, 1=per blade, 2=Hz (-1) |  | **NEW** | −1 = *same as blade 1* sentinel; range −1..max. | S | snaps | no (stepped) | B | 350 / 1350 · key `bl.lock2` | T2 |
| 67 | `mirror2` · Mirror | Blade 2 | choice: -1=same as blade 1, 0=off, 1=reflect, 2=twin −, 3=twin + (-1) |  | **NEW** | −1 = *same as blade 1* sentinel; range −1..max. | S | snaps | no (stepped) | B | 354 / 1354 · key `bl.mirror2` | T2 |
| 68 | `morph2` · Shape | Blade 2 | continuous: 0..1 (0.5) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 355 / 1355 · key `bl.morph2` | T2 |
| 69 | `rot2Follow` · Rotation | Blade 2 | choice: 1=with blade 1, 0=its own (1) |  | **NEW** |  | S | snaps | no (stepped) | B | 356 / 1356 · key `bl.rot2Follow` | T3 |
| 70 | `rotRate2` · Rotate | Blade 2 | continuous: -4..4 (0) | linear | **NEW** |  | M | blends · B232 | yes · Hz, linear | C | 357 / 1357 · key `bl.rotRate2` | T2 |
| 71 | `c2` · Position | Blade 2 | continuous: 0..1 (0.35) | linear | **NEW** | Circular: morph and matrix must wrap (§4.3). | M | blends SHORTEST ARC (needs a circular rule) · B232 | yes · cycles, WRAPPING (circular; the linear clamp is wrong, §4.3) | P | 351 / 1351 · key `bl.c2` | T1 |
| 72 | `hard2` · Edges | Blade 2 | continuous: 0..1 (0) | linear | **NEW** | Oracle smooths at CONTROL rate; SPEC §7 says per sample (§1.8.3). | M | blends · B232 | yes · span 1 | C | 352 / 1352 · key `bl.hard2` | T1 |
| 73 | `depth2` · Depth | Blade 2 | continuous: 0..1 (1) | linear | **NEW** | Oracle smooths at CONTROL rate; SPEC §7 says per sample (§1.8.3). | M | blends · B232 | yes · span 1 | C | 353 / 1353 · key `bl.depth2` | T1 |
| 74 | `b2sp` · Blade 2 uses | Blade 2 spread | choice: 0=blade 1 spreads, 1=its own (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 359 / 1359 · key `bl.b2sp` | T3 |
| 75 | `bspread2` · Position | Blade 2 spread | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 360 / 1360 · key `bl.bspread2` | T3 |
| 76 | `kRule2` · Cut rule | Blade 2 spread | choice: 0=even, 1=harmonic, 2=undertone, 3=octaves, 4=major, 5=minor, 6=fifths, 7=golden, 8=primes, 9=custom (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 362 / 1362 · key `bl.kRule2` | T3 |
| 77 | `kspread2` · Cut rate | Blade 2 spread | continuous: 0..24 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 24 (harmonics) | C | 361 / 1361 · key `bl.kspread2` | T3 |
| 78 | `kRuleAmt2` · Rule depth | Blade 2 spread | continuous: 0..1 (1) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 363 / 1363 · key `bl.kRuleAmt2` | T3 |
| 79 | `wspread2` · Width | Blade 2 spread | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 364 / 1364 · key `bl.wspread2` | T3 |
| 80 | `mspread2` · Shape | Blade 2 spread | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 366 / 1366 · key `bl.mspread2` | T3 |
| 81 | `ispread2` · FM index | Blade 2 spread | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 367 / 1367 · key `bl.ispread2` | T3 |
| 82 | `rotSpread2` · Rotate | Blade 2 spread | continuous: 0..2 (0) | power | **NEW** |  | M | blends · B232 | yes · span 2 (Hz) | C | 368 / 1368 · key `bl.rotSpread2` | T3 |
| 83 | `dspread2` · Depth | Blade 2 spread | continuous: 0..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 1 | C | 365 / 1365 · key `bl.dspread2` | T3 |
| 84 | `kRuleOut2` · Members | Blade 2 spread | other: display only |  | **DROPPED** | same, blade 2. | — | — | — | UI | none | — |
| 85 | `b2fm` · Blade 2 uses | Blade 2 FM | choice: 0=blade 1 FM, 1=its own (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 369 / 1369 · key `bl.b2fm` | T3 |
| 86 | `fmType2` · FM type | Blade 2 FM | choice: 0=phase, 1=pitch (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 370 / 1370 · key `bl.fmType2` | T2 |
| 87 | `mshape2` · Mod shape | Blade 2 FM | choice: 0=Sine, 1=Tri, 2=Saw, 4=Rev saw, 3=Square, 5=Smooth noise, 7=S&H noise (0) |  | **NEW** | Stored values are non-contiguous or out of display order; re-index (§1.8.2). | S | snaps | no (stepped) | B | 371 / 1371 · key `bl.mshape2` | T2 |
| 88 | `I2` · FM depth | Blade 2 FM | continuous: 0..10 (2) | power | **NEW** | Oracle smooths at CONTROL rate; SPEC §7 says per sample (§1.8.3). | M | blends · B232 | yes · span 10 (index) | C | 372 / 1372 · key `bl.I2` | T1 |
| 89 | `mUnit2` · Mod rate in | Blade 2 FM | choice: 0=× f₀, 1=Hz (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 373 / 1373 · key `bl.mUnit2` | T2 |
| 90 | `m2` · Mod rate | Blade 2 FM | continuous: 0.25..24 (3.37) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | C | 374 / 1374 · key `bl.m2` | T1 |
| 91 | `mHz2` · Mod rate Hz | Blade 2 FM | continuous: 1..20000 (660) | log | **NEW** |  | M | blends · B232 | yes · octaves (log) | C | 375 / 1375 · key `bl.mHz2` | T1 |
| 92 | `b2env` · Blade 2 uses | Blade 2 envelope | choice: 0=blade 1 envelope, 1=its own (0) |  | **NEW** |  | S | snaps | no (stepped) | B | 376 / 1376 · key `bl.b2env` | T2 |
| 93 | `benvK2` · Cut rate | Blade 2 envelope | continuous: -1..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 2 | C | 377 / 1377 · key `bl.benvK2` | T2 |
| 94 | `benvW2` · Width | Blade 2 envelope | continuous: -1..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 2 | C | 378 / 1378 · key `bl.benvW2` | T2 |
| 95 | `benvA2` · Attack | Blade 2 envelope | continuous: 0.5..2000 (2) | log | **NEW** |  | M | blends · B232 | yes · NO (modulator time; B70 cycle rule if blade env becomes a source) | C | 379 / 1379 · key `bl.benvA2` | T2 |
| 96 | `benvD2` · Decay | Blade 2 envelope | continuous: 10..5000 (250) | log | **NEW** |  | M | blends · B232 | yes · NO (same) | C | 380 / 1380 · key `bl.benvD2` | T2 |
| 97 | `benvVel2` · Velocity | Blade 2 envelope | continuous: 0..1 (0.5) | linear | **NEW** |  | M | blends · B232 | yes · NO (note-on only) | C | 381 / 1381 · key `bl.benvVel2` | T3 |
| 98 | `benvK` · Cut rate | Blade 1 envelope | continuous: -1..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 2 | C | 321 / 1321 · key `bl.benvK` | T2 |
| 99 | `benvW` · Width | Blade 1 envelope | continuous: -1..1 (0) | linear | **NEW** |  | M | blends · B232 | yes · span 2 | C | 322 / 1322 · key `bl.benvW` | T2 |
| 100 | `benvA` · Attack | Blade 1 envelope | continuous: 0.5..2000 (2) | log | **NEW** |  | M | blends · B232 | yes · NO (modulator time; B70 cycle rule if blade env becomes a source) | C | 323 / 1323 · key `bl.benvA` | T2 |
| 101 | `benvD` · Decay | Blade 1 envelope | continuous: 10..5000 (250) | log | **NEW** |  | M | blends · B232 | yes · NO (same) | C | 324 / 1324 · key `bl.benvD` | T2 |
| 102 | `benvVel` · Velocity | Blade 1 envelope | continuous: 0..1 (0.5) | linear | **NEW** |  | M | blends · B232 | yes · NO (note-on only) | C | 325 / 1325 · key `bl.benvVel` | T3 |
| 103 | `wtMode` · Source | Serum wavetable | choice: 0=snapshot, 1=sweep X, 2=sweep Y, 3=record (0) |  | **DEFERRED** | Serum wavetable export (D9; SPEC §1, §10). | — | — | — | UI | none | — |
| 104 | `wtFrames` · Frames | Serum wavetable | choice: 16=16, 64=64, 128=128, 256=256 (64) |  | **DEFERRED** | Serum wavetable export (D9; SPEC §1, §10). | — | — | — | UI | none | — |
| 105 | `wtDur` · Record for | Serum wavetable | continuous: 0.5..16 (4) | log | **DEFERRED** | Serum wavetable export (D9; SPEC §1, §10). | — | — | — | UI | none | — |
| 106 | `wtNorm` · Level | Serum wavetable | choice: 1=normalize, 0=as is (1) |  | **DEFERRED** | Serum wavetable export (D9; SPEC §1, §10). | — | — | — | UI | none | — |
| 107 | `dcMode` · DC fix | Drone and output | choice: 0=off, 1=blocker, 2=per cycle (2) |  | **NEW** |  | S | snaps | no (stepped) | B | 327 / 1327 · key `bl.dcMode` | T3 |

### 1.5 The three overlapping groups: whose law, and why

The rule applied throughout is **horde's law wins where horde already has the behaviour**. There
are three reasons:

1. A revision-1 patch must render bit-identically (ADR-183 §2).
2. SPEC §11 says so for coupling and for the output stage.
3. The horde law is already the golden that shipped goldens encode.

SCALPEL's law wins only where horde has nothing: the blades, spreads, rules, cross-mod, feedback,
frames and DC.

**Swarm (10).**

| SCALPEL | horde | whose law | why · what differs |
|---|---|---|---|
| `N` 1–9 | `n` 1–32 | horde id, revision-gated cap | §3. |
| `detune` 0–100 c, gradient | `detune` 0–1 × 100 c × x(dist) (`swarm_core.h:1773-1801`) | horde | Identical at dist 0, law 0: d c = knob d/100. Horde's default dist is 1 (JP-8000), so a *default* horde patch spreads differently from a SCALPEL preset at the same cents. |
| `K` −1..1 | `K` −1..1 | **horde** (SPEC §11) | The laws differ in shape and strength (§1.6.1). |
| `width` 0–1 | `width` 0–1.5 | horde | Horde's super-width (ADR-025/074) is a superset. |
| `panOrder` balanced/fan | `panLayout` pitch fan/legacy (+ curve, invert, scatter) | horde family; *balanced* as a new row | §1.6.5. SCALPEL's *fan* ≈ horde's *legacy* (pan ∝ x). Horde's default is neither. |
| `xm` | — | SCALPEL | NEW. |
| `phaseMode` | `retrig`, `scatter`, `keepPhase` | horde for random/aligned; *settled* NEW | §1.6.6: the two phase origins differ by ½ cycle. |
| `frame`, `frame2` | — | SCALPEL | NEW. ψ exists in horde already (`s.psi`, `swarm_core.h:1921`). |
| `cScale` | `absK` (and the σ law) | horde | §1.6.2. |

**Voice (9).**

| SCALPEL | horde | whose law | why |
|---|---|---|---|
| `base` | none (always saw) | SCALPEL | NEW. Default Saw. Squareness (69) is retired, and base = Square is its nearest successor. |
| `A` `D` `S` `R` (ms, linear attack) | 19–22 (s, one-pole, ADR-021) | horde | The amp envelope is the voice's, and rev-1 identity needs it. SCALPEL presets translate ms → s; the linear-vs-one-pole attack shape differs (listen, not map). |
| `gain` | `vol` 17 | horde | D11: horde's output stage (`gain = vol·0.9/n^normExp`, `swarm_core.h:943`; `tanh`, `:1331`). SCALPEL's `tanh(1.6·gain·y)` (`razor-core.js:791`) is dropped. |
| `polyMode` | `voiceMono` 32 + `voiceLegato` 34 (global) | horde | Horde's allocator and shell voicing. |
| `glide` | `glide` 33 (lag τ, s) | horde | Same law family: τ = T/3000 (`razor-core.js:669`). |
| `glideAlways` | `glideMode` 90 | horde | *overlapping* = 0, *always* = 2. |

**Drone and output (7).**

| SCALPEL | horde | fate | why |
|---|---|---|---|
| `f0Mode`, `f0`, `f0Note`, `nameAbleton` | — | DROPPED | The bench's drone test-tone and its note-naming convention (`engine`: "UI / bench only"). Horde is played from MIDI. |
| `os` | `oversample` 88 (global, ships off) | MERGES | SCALPEL's AA claims (SPEC §1, §7) assume 2×. Q D2. |
| `aa` | `digital` 16 | MERGES | clean = 1, raw = 0. `digital` becomes the scalar on every BLEP correction. |
| `dcMode` | — | NEW | Inert without blades: the per-cycle estimate is of blade contributions only (SPEC §7), and the blocker runs only with `xm`/`fb` active (`razor-core.js:787-790`). The default can stay *per cycle*. |

### 1.6 The coupling law, checked against `src/swarm_core.h`

SPEC §11 says to use horde's Kuramoto and K normalisation. SPEC §3 is written in SCALPEL's. This
section records where the two actually differ.

**1.6.1 Normalised K.**

| | horde (`swarm_core.h:1885-1897`, `:1987-1992`) | SCALPEL (`razor-core.js:383-389`, `:401-428`) |
|---|---|---|
| strength | `KsmS → (4K\|K\| + Kenv)·σ` Hz, where σ = std-dev of member freqs, floor 0.08 Hz; `absK` replaces σ with 2.5 Hz | `Keff/2π = K·(1.5·Δf_max + 3·s)` Hz, Δf_max = f·(2^(detune/1200) − 1) |
| shape in K | quadratic, signed | linear |
| pull | `couple = KsmS·R·sin(ψ − θᵢ)` Hz, added to the member frequency | `Keff/2π · Σₕ(1/h)(Im Rₕ cos − Re Rₕ sin)` Hz; the same form at H = 1 |
| smoothing | τ 4.35 ms on KsmS (`:152`, `:1896`) | none beyond K's own 12 ms control-rate smoother |
| update | every **16** samples (`kTick`, `:54`) | every **32** samples (`razor-core.js:668`) |
| transient | onset lock `Kenv` decays over `dissolve` (`:635`, `:1740`) | none (settled start is its answer) |
| second order | `inertia` (`:2055-2085`) | none |

Peak pull at R = 1, in Hz (*arithmetic*, even spread, horde dist 0 / law 0):

| N · detune | f | K | horde | SCALPEL *seconds* | SCALPEL *cycles* |
|---|---|---|---|---|---|
| 5 · 14 c | 110 | 0.35 | 0.31 | 1.52 | 1.52 |
| 5 · 14 c | 440 | 0.35 | 1.23 | 2.93 | 6.08 |
| 5 · 14 c | 440 | 1.0 | 10.06 | 8.36 | 17.36 |
| 7 · 40 c | 110 | 1.0 | 6.78 | 6.86 | 6.86 |
| 7 · 40 c | 880 | 0.35 | 6.64 | 11.85 | 19.20 |
| 5 · 0 c | any | 0.35 | 0.04 | 1.05 (×f/110 in *cycles*) | — |

The verifier's own lock case (7 members, ±40 c, K = 1, A2) happens to land where the two agree.
Everywhere the packet's presets live (K 0.3–0.6, detune 3–22 c: `data/presets.json`, all 76
presets), SCALPEL couples ≈ 2–9× harder at K 0.35 and ≈ 1–5× at K 0.6 (the ratio falls as 1/K), and 9–27× harder near zero detune. **A SCALPEL preset's K is
not a horde K.** Porting the 76 presets means re-voicing their K by ear or by a measured map,
never by copying the number (Q B3).

**1.6.2 Coupling time (*seconds* / *cycles*).** Under horde's law and the cents detune law, σ ∝ f.
Pull and detuning both scale with pitch, so the locked lag `sin Δθ = Δω/(K·r)` is **already
pitch-independent**. That is SCALPEL's *cycles*. It breaks only where σ hits its 0.08 Hz floor
(tiny detune), and under the Hz-constant and ERB laws (1, 2), where σ is not ∝ f. `absK`
(`:1886-1889`) is the fixed-Hz coupling, i.e. SCALPEL's *seconds* without the detune term. So
`cScale` MERGES into horde's existing behaviour, and D5 needs no new parameter. *Hypothesis to
verify:* SCALPEL's own criterion (lead-spread range < 0.01 cycles across A1–A5) should pass under
horde's law at law 0, once detune is above the floor.

**1.6.3 Settled start.** Horde has no fast-forward. Its start phases are all 0 (`retrig` on, the
default), a seeded random draw (`retrig` off), a partial draw (`scatter`), or the last phases
(`keepPhase`) (`swarm_core.h:693-702`). SCALPEL's *settled* re-draws the phases, then runs 150
coupling steps (`razor-core.js:391-400`). K > 0 draws uniform [0, 0.15); K ≤ 0 draws uniform
[0, 1). Under horde's law the port would settle with horde's coupling function, at horde's
16-sample grid spacing in simulated time. Two things are new:

- a **new named RNG stream**. The per-note phase stream (`seed + age·7919 + 1`, `:692`) also feeds
  drift, and drawing from it would shift rev-1 draws;
- a note-on CPU spike (SPEC §8.5).

It also has to be sequenced with onset lock, which is horde's transient. The two answer the same
complaint in opposite ways (Q B4).

**1.6.4 Negative K.** SCALPEL repels with H = min(N−1, 6) harmonic mean fields (`:406`). That
cancels the first N−1 moments only up to **N = 7**; at 8 and 9 members two moments are left, and
at 16 members nine are. Horde reaches splay differently. The mean-field path adds a pacemaker toward
an explicit rank lattice, `KsmP·sin(2π(θ_c0 + (i−c0)/n − θᵢ))` with gain 3·4K²·σ (`:1895`,
`:1990-1991`), so its splay is equidistant by construction at any n. Horde's `poles` (Daido q) is
unrelated: an attractive q-cluster law at K > 0 (`:1945-1960`). Under horde's law, SCALPEL's
"r ≤ 0.10 at K = −1" criterion needs re-measuring. *Hypothesis:* it passes.

**1.6.5 Balanced pan order.** SCALPEL's tables (`razor-core.js:817`) permute evenly spaced slots so
that pan does not correlate with member index. Horde's default (ADR-070,
`swarm_core.h:1585-1632`) is an **alternating pitch-ranked fan**: rank 0 in the centre, then out
alternately left and right. That already breaks the monotone tilt SCALPEL measured under *fan*,
but it keeps |pan| correlated with pitch rank. *Hypothesis:* under a gradient spread its residual
L/R brightness tilt sits between SCALPEL's *fan* (+3.2 dB) and *balanced* (< 1.5 dB); measure it.
The tables stop at N = 9, and N = 2 cannot be balanced. A 16-member cap needs tables that do not
exist, and 16! is too large for the packet's exhaustive search (Q C3).

**1.6.6 Phase origin.** SCALPEL's saw is `2·frac(x + ½) − 1` (`razor-core.js:17`): zero-crossing
rising at x = 0, jump at ½, sine-aligned like every SCALPEL wave. Horde's is `2·ph − 1`
(`swarm_core.h:1058`), with the jump at 0. So φ_SCALPEL = frac(φ_horde + ½) maps one saw onto the
other exactly. **Position** and **aligned** therefore mean different things in the two frames. A
SCALPEL preset's Position 0.875 is horde phase 0.375. Horde's `retrig` start (φ_horde = 0) is
φ_SCALPEL = ½, not SCALPEL's *aligned* 0. Recommendation (Q B5): keep horde's phase state and
start, and evaluate every SCALPEL formula at φ_SCALPEL. Rev-1 is then untouched, and presets
translate by the fixed offset.

### 1.7 Defaults horde must set, and the three rows the packet does not name

With blades off, SPEC §11's "legacy defaults" must reproduce a rev-1 patch bit-for-bit (SPEC §9's
acceptance line *"existing HORDE sets load and render unchanged"*). The packet's defaults do not:

| row | packet default | horde default (proposed) | why |
|---|---|---|---|
| blade 1 | on: `w` 0.25, `depth` 1, `mode` Sync, `k` 6 | **off** | The bench always has blade 1. Horde's existing patches have none. |
| `base` | 0 = Sine | **2 = Saw** | The swarm is a saw today. |
| `phaseMode` | 2 = settled | `settle` off (retrig governs) | Rev-1 phases (§1.6.3). |
| `panOrder` | 0 = balanced | `panBalanced` off | Rev-1 pan image. |
| `dcMode` | 2 = per cycle | 2 = per cycle | Inert without blades (§1.5). |
| `gain` | 0.35 | `vol` 0.4 | Horde's own. |

**Three horde rows the packet does not have** (proposed ids, all per-osc, all Structural, all
default off):

- **300 `bl.b1on`**, *Blade 1*. A switch, so blade 1 can be inert by default without zeroing
  `w`/`depth`. Its other rows keep the bench's values ("Quarter sync"), so switching it on lands
  on a musical starting point. Morph: snaps, or ramps like blade 2 (Q G2).
- **330 `bl.settle`**, *Settle start*. The *settled* half of `phaseMode`. A new row rather than a
  third `retrig` value: widening a shipped stepped range re-maps VST3 automation (§1.8.5).
- **331 `bl.panBalanced`**, *Balanced pan*. The *balanced* half of `panOrder`, for the same reason.
  It overrides `panLayout` while on.

### 1.8 Port-data findings (packet vs packet, packet vs horde)

1. **`parameters.json` options are in display order, not value order.** `phaseMode` lists
   *settled|random|aligned* with default 2. The stored values are 2 = settled, 0 = random,
   1 = aligned (`scalpel-bench.html:1261`; `razor-core.js:347`, `:364`). `rotSync` default 1 is
   *restart per note* (`:1238`). Reading `default` as an index into `options` gives the wrong
   answer for `phaseMode`, `rotSync`, `law`, `hot`/`hot2`, `mshape`/`mshape2`, `wtNorm` and
   `nameAbleton`. The table in §1.4 prints the real value→label pairs.
2. **Some enum values are non-contiguous.** `hot`: 0 Sine, 1 Tri, 2 Saw, 3 Square, 4 Rev saw,
   6 Sine→Saw (no 5). `mshape`: …, 5 Smooth noise, 7 S&H (no 6). A horde stepped row spans
   min..max, so a hole would be a dead host value. Proposal: contiguous horde indices in display
   order, with the mapping to oracle values held by the parity harness (Q E4).
3. **SPEC §7 vs the oracle on smoothing.** SPEC §7 says per-sample smoothing applies to "cut rate
   (k, Hz), width, position, depth, FM depth, edges — for both blades". The oracle smooths
   `depth2`, `I2` and `hard2` at the 16-sample rate (`razor-core.js:665-667`), as
   `parameters.json` also says. Parity follows the oracle; the spec line needs a ruling (Q E5).
4. **Key collisions.** SCALPEL keys that already exist as horde core keys: `law` (5), `width` (14),
   `detune` (4), `K` (6), `driftRate` (10), `glide` (33). SCALPEL keys that are dangerously short:
   `c`, `k`, `m`, `I`, `R`, `S`, `A`, `D`, `w`, `depth`, `mode`. The shell's fall-through writes
   every core by key (`hypersaw_clap.cpp:290-292`, "a new id named 'tilt' would write both").
   Hence the `bl.` prefix.
5. **Widening a stepped range re-maps VST3 automation.** clap-wrapper sets
   `stepCount = max − min` and normalises by `(v − min)/(max − min)`
   (`libs/clap-wrapper/src/detail/vst3/parameter.cpp:123-130`). CLAP stores plain values, so a
   CLAP host and a saved patch are safe. A VST3 lane stores normalised values and is not.
   `mainAsnX/Y` (179/180) were widened 0..8 → 0..10 on 2026-09-19 without addressing this
   (`hypersaw_clap.cpp:637-648`). That shipped case is worth a separate look.
6. **N ≤ 9 is baked into the oracle.** Member arrays are fixed at 9 (`razor-core.js:275`, `:288`,
   `:295`), `PANS` has 9 rows (`:817`), and the *primes* rule has 9 primes (`:84`), so a 10th
   member reads `undefined` → NaN. A 16-member cap has no oracle to be measured against above 9.
7. **`N` is typed `continuous` in `parameters.json`** but is discrete (block rate).
8. **Blade 2 has no `kq2` and no `kCustom2`.** Blade 2's *custom* rule reads blade 1's ratio list
   (`razor-core.js` `spread`, `s.kCustom`).

---

## 2. Priority order for the interface

The human: *"Much of what's in Scalpel will take priority over even existing parameters that
survive — this will be the sound design powerhouse of the synth, and the interface needs to feel
really intuitive and clear."*

**Evidence for the ranking.** How often each parameter is moved off its default across the 76
packet presets (*arithmetic*, `data/presets.json`): Position 73, Width 70, K 65, Members 61,
Detune 59, Edges 58, Cut rate 57, FM depth 51, Mod rate 48, Mode 37, Blade wave 37, Blade 2 on 35,
Base wave 32, … Mirror 7, Frame 2, every spread except Position and Cut rate ≤ 3. The preset
authors reached for the blade core and the swarm pad; the spreads are specialist.

**The frame it lands in.** B212's six decisions hold. Decision 6 is the invariant: a compact
control is a *view* of an existing address. B227's reading of the human's wireframes is the
layout:

- **MAIN**'s *Osc Controls* has Sub / Swarm 1 / Swarm 2 sub-tabs. Today they hold a K × Detune
  XY, Voices, Width, R → Tone, Onset/Dissolve, Drift, Squareness and an Envelope.
- The **OSC** page is a K × Detune XY, a visualizer column, and one large Controls area with its
  own *Advanced*.

The proposal changes the **contents** of the Swarm tabs, not the frames.

**T1: the face** (the MAIN Swarm tab and the top of the OSC Controls area), per oscillator:

1. **K × Detune pad** + **Members**. The swarm's identity. Unchanged.
2. **Blade selector [1 | 2]**, each with its power switch (`b1on`, `b2on`). One layout serves both
   blades; SCALPEL's "blade 2 mirrors blade 1" becomes something you *see*.
3. For the selected blade: **Mode**, **Blade wave**, **Width**, **Cut rate** (the knob shows Hz
   when *Cut rate in* is Hz: `k`/`kHz` share one slot), **Position**, **Edges**, **Depth**.
   Seven controls.
4. Contextual: **FM depth** + **Mod rate** when Mode is FM reset or FM free (the `shown_when`
   mechanism, `param_presentation.tsv` header).
5. **Base wave**. It takes Squareness's place in the human's list, since Squareness is retired.
6. **Osc on** (tab power) and **Volume**.

**T2: one level down** (the OSC Controls area below the fold, or its first *Advanced* step):

- *Blade detail:* Cut rate in, Mirror, Shape (when a wave is Sine→Saw), Rotate, FM type, Mod shape,
  Mod rate in, Feedback, Blade frame, and the blade envelope (Cut rate amt, Width amt, Attack,
  Decay).
- *Swarm detail:* Cross-mod, Settle start, Spread law + Position spread + Cut rate spread + Cut
  rule + Rule depth.
- *Surviving swarm controls demoted from the face:* Width (stereo), R → Tone, Onset, Dissolve,
  Drift depth, the amp Envelope (ADSR), Pitch.

**T3: Advanced.** The remaining spreads (width, depth, shape, FM index, rotate), Drift rate
(spread), Quantize, Custom ratios, DC fix, Balanced pan, Rotation (restart/free), blade 2's
*same as blade 1* / *uses its own* switches (frame2, lock2, mirror2, rot2Follow, b2sp, b2fm,
b2env) and blade 2's own spread/FM/envelope sets, envelope velocity. Also every surviving
specialist control: distribution, detune law and its sub-parameters, dynamics/topology, the pan
image family, super-width, tone tilt, hi tame, density comp, digital, onset & scatter, phase
scatter, keep phase, seed, absolute coupling.

**What this asks of the compact lab.** Round 1's spine was `n, detune, K, width, attack, release`
(`docs/design/compact-lab.html:1185`). Under this ranking it becomes
`n, detune, K, b1on/b2on, mode, blade wave, width, cut rate, position, edges, depth, base`, with
the envelope and stereo width one level down. SCALPEL's own monitor visuals map to the visualizer
names B227 lists (*hypothesis* on the mapping):

- the cycle view (sum/members) → *Waveform*;
- the phase ring with cross-mod arrows → *Swarm Circle*;
- the spectrum with the predicted formant band → a new visualizer.

Whether the blade selector is a tab or two stacked rows depends on the 980×720 conflict B227
recorded (Q I2).

---

## 3. The member limit

**Today.** `n` is declared 1..32, default 7 (`hypersaw_clap.cpp:175`). The core clamps to
`kMaxV` = 32 (`swarm_core.h:41`, `:446`, `:1355`), and `applyParam` clamps every write to the
declared range (`hypersaw_clap.cpp:6910`). The GUI, CLAP hosts and saved patches carry **plain**
values. VST3 hosts carry values **normalised through min/max**, with `stepCount = 31`
(`parameter.cpp:123-130`).

**What narrowing the declared range to 9 (or 16) would do:**

- Every stored `n` above the cap silently clamps on load (`:6910`). That is a sound change with no
  revision gate.
- Every VST3 automation lane on ids 1/1001 re-maps. Today normalised 0.5 → n ≈ 16. At max 9 it
  → n = 5; at max 16 it → n = 8.
- For AU, clap-wrapper passes min/max as the AU range; whether hosts store AU automation as plain
  values is not checked here (*hypothesis*).

**The factory bank** (`docs/presets/factory/*/*.json`, 41 patches; *arithmetic* on the files):

| | n ≤ 9 | n > 9 | n > 16 | n = 16 exactly |
|---|---|---|---|---|
| osc 1 (41 patches) | 10 | **31** | **5** (18, 20, 20, 22, 24) | 10 |

Sorted, the osc 1 values are 2, 3, 4, 5, 7, 7, 7, 8, 8, 9, 10, 10, 10, 10, 12 ×8, 14 ×3, 15, 16 ×10,
18, 20, 20, 22, 24. Osc 2 is disabled in all 41 (n = 7 default). The three morph patches hold
n = 16 in all four corners. The four corner presets (`factory/corners/`) hold osc-1 n = 16. The
pads are the heavy users (16–24), then the FX demos (10–16). Elsewhere: the Serum parity
reference holds n = 16 (`docs/presets/serum-parity-reference.json`), and the state fixtures hold
n = 9 (`tests/state_fixtures/*.txt`). By contrast, the 76 SCALPEL presets use N 1–9 only (13 at
N = 1, one each at 8 and 9).

**The options.**

- **(A) Revision-gated clamp, range kept (recommended).** Ids 1/1001 keep 1..32 forever. At
  engine revision ≥ R_s the engine plays `min(n, cap)` members. At R_s, `applyParam` also clamps
  the *stored* value to the cap, so readback never shows a count the engine is not running (the
  B241 lesson: readback = what the engine holds). Below R_s nothing changes, including 32 members.
  The GUI draws the knob 1..cap for an R_s patch. A VST3 lane keeps its mapping and plateaus above
  the cap. Opting an old patch forward (B239) with n > cap must *say* "Members 16 → 9"; it must
  never clamp silently.
- **(B) Clamp only when a blade is on.** This keeps 32-member pads available at R_s. It is also
  two laws for one knob, and a pad that loses members the moment a blade is switched on.
- **(C) A new Members parameter (1..cap), with `n` retired at R_s.** Clean host range. But hosts
  then show two voice-count parameters, and every existing lane on id 1 goes inert for new
  patches.

**The cap value.** The ruling says "9 is the target, test up to 16". Recommendation: **cap 9** at
R_s, the human's stated ceiling. Size the port's member arrays and the parity tests for **16**, so
raising the cap later is a constant plus a re-measurement, not a layout change. Two things block a
cap of 16 today: the pan tables and the primes rule stop at 9 (§1.8.6), and the cost roughly
doubles (§5).

**Consequence for the bank.** Under (A) the bank *cannot* move to R_s as-is: 31 patches would lose
members. Either the bank stays at its current revision (it keeps sounding as it does, with no
blades), or it is deliberately re-voiced patch by patch (Q C2). ADR-183 §3's "the bank moves to
the new revision" rule meets its first counter-case here.

---

## 4. The mod-matrix surface

The human's condition, from ADR-184: *"so long as we can … maintain the integrity of the mod
matrix."*

### 4.1 Destinations

- **Today.** The shell accepts ≈ 199 instrument destination ids: 54 per-osc continuous rows × 2,
  plus 91 global continuous rows (*arithmetic* over `kParams` with `modAddRoute`'s exclusions,
  `:3541-3556`), plus the sub's rows and the routing cells.
- **The GUI's menu is flat.** `modDestOptions()` walks every range slider on every page and
  fans each out to both oscillators (`src/gui/gui2.html:3335-3372`).
- **What SCALPEL adds.** By construction, every continuous row is a destination the moment it is
  declared. 51 continuous NEW rows per oscillator would add **102** entries, and nothing would opt
  them in or out. The 10 retired continuous ids (69, 129–132 and twins) should leave the menu at
  R_s: they are inert there. They stay destinations below R_s, where they are live.

**Proposed destination set per oscillator:** all 51 continuous NEW rows, **except**:

- the blade envelopes' `A`, `D` and `Vel` (×2 blades = 6). These are a modulator's own controls
  (B171's refusal of 269–288 for the same reason), and velocity is read at note-on.

That leaves **45 per osc, 90 in all**.

**How the list stays navigable:**

1. **Hierarchical picker.** Oscillator → section (Swarm · Blade 1 · Blade 2 · Spread · Blade env)
   → parameter, replacing the flat list.
2. **The picker follows the tiers.** T1 and T2 destinations are offered by default; T3 behind
   *all*. That is 20-odd entries per oscillator on first open, not 45.
3. **Eligibility stays in the shell.** Presentation data must not carry dispatch facts
   (`param_presentation.tsv` header, FOUNDATIONS D1/D2). So eligibility is the shell's rule
   (continuous ∧ not in a *non-destination* override list, the ADR-173 pattern), and the GUI
   mirrors it, as `modDestOptions` already mirrors `modAddRoute`.
4. **Destinations are per osc**, like every other twin. The picker should never offer a blade
   destination on a SPECTRA oscillator (`engine` = 1).

### 4.2 New sources

| candidate | nature | proposal |
|---|---|---|
| Blade 1 / Blade 2 envelope (×2 osc = 4) | per **voice** (AD, velocity-scaled; `razor-core.js:678-690`) | **Internal for v1.** They drive their fixed targets (cut rate ×2^(±4·level), width ×2^(±3·level)) per voice, as the oracle does. Horde's matrix is global scope (`mod_core.h:46`, routes evaluated with `kGlobal`, `hypersaw_clap.cpp:4001`), so exposing them now means a loudest-voice projection, and the mod lab's finding #2 says per-note sources must not be projected globally (ROADMAP, Modulation lab status). Expose them as sources when per-note scope (B82) exists. |
| Swarm lead ψ | ψ = arg R₁ **rotates at the note frequency** | **Not a source.** Sampled at the 16-sample control tick, it aliases. What SCALPEL uses internally is the per-member *lead* θᵢ − ψ, and that stays internal (swarm frame, swarm spread law). The swarm's control-rate observable is **R**, which B253 adds as Coherence 1/2. |
| (for completeness) spread positions `pn` | per member | Not a source (internal). |

### 4.2a The source cap is full: what growing it costs, and the proposed final list

This subsection was added at the lead's request, mid-task. **The situation.** `kMaxSources` = 24
(`src/mod_core.h:43`). Slots 0–21 are shipped (the B134 table, `hypersaw_clap.cpp:2696-2716`). Two
rows want 22/23:

- B253's lab (PR #747) puts Coherence 1/2 there;
- B180's companion proposed `Gain ENV OSC 1/2` for "the last two free slots (22, 23)", and said
  "after this the source array is FULL".

Whichever lands, the array is full, and no SCALPEL source can arrive without growing the cap.

**What growing it costs. Read from the code, not measured:**

- **Per-tick work: nothing that scales with the cap.** `evaluate()` walks the *route* table (≤ 64)
  with an O(routes²) destination compaction (`mod_core.h:121-141`). It never iterates over
  sources. It reads `src[q.src]` for each route. The shell fills `mod.src[]` slot by slot in
  `modStep` (`hypersaw_clap.cpp:3858-3998`), so the per-tick cost is the cost of **each source
  actually computed** (for Coherence, a gain-weighted reduction of each voice's `s.R`, which
  `controlTick` already computes at `swarm_core.h:1920`), not of the cap.
- **Memory: negligible.** `src[]` (double) and `srcPol[]` (int) grow by 12 bytes per slot, in one
  `ModCore` per plugin instance, not per voice.
- **State: none at the format level.** A route persists as the text `src:dest:depth[:pol];` with
  `src` printed `%u` (`hypersaw_clap.cpp:3811-3827`). There is no width, no bitfield and no count
  field to widen. The B222 history uses the same chunk (`lossless` form). No state-format change,
  no `schema` bump.

**Do appended slots keep every saved route valid?** **Yes, by construction.** A slot index never
moves when the cap grows. Every saved route names the same source before and after. The only
exposure is forward compatibility:

- the loader passes each route through `modAddRoute` → `ModCore::addRoute`, which refuses
  `source >= kMaxSources` (`mod_core.h:100`), and the loader then `continue`s
  (`hypersaw_clap.cpp:3851-3852`);
- so a patch saved by a newer build with a route on slot ≥ 24, opened in an older build, **loses
  that route silently**, and re-saving there erases it.

That is a downgrade loss, not corruption of anything the older build understands. It is worth one
line in the release notes, and no mechanism.

**What must grow in step** (the checklist for the row that raises it):

1. `ModCore::kMaxSources`.
2. `MOD_SRC_NAMES` in `src/gui/gui2.html:3174-3185`. The source pickers are built from its
   *length* (`:3260`, `:3384`), so a slot without a name is unreachable from the GUI.
3. The polarity table (`makeModCore`, `hypersaw_clap.cpp:2717-2726`). New slots default to
   `kSrcUnipolar`, which is right for every candidate below, so only a bipolar addition needs a
   line. Update its comment "22+ unassigned".
4. The `modStep` writes for each new slot.
5. The mod-matrix lab's source list (B207/B253).

The checks already use the constant symbolically (`tools/mod_check.cpp:93`,
`hypersaw_clap.cpp:9177`), so there is no numeric pin to move. **Never reuse** 10–13: they are
retired but reserved, and reusing them would silently re-aim every saved route that names them
(`hypersaw_clap.cpp:3987-3992`).

**Proposed final source list** (append-only; slots 0–21 exactly as shipped):

| slot | source | polarity | status |
|---|---|---|---|
| 0 | ENV 1 (amp, max over voices; renamed per B179) | uni | shipped |
| 1 | ENV 2 (pitch envelope) | uni | shipped |
| 2–9 | Macro 1–8 | uni | shipped |
| 10–13 | XY aliases (retired, ADR-156; read 0) | bi | reserved: never reuse |
| 14–17 | Velocity · Mod wheel · Pressure · Pitch wheel | uni ×3 · bi | shipped |
| 18–21 | LFO 1 · LFO 2 · ENV 3 · ENV 4 | bi · bi · uni · uni | shipped |
| **22** | **Coherence 1** (osc 1's R; gain-weighted over voices when read globally) | uni | B253 (PR #747) |
| **23** | **Coherence 2** | uni | B253 |
| **24** | **Gain ENV OSC 1** | uni | B180's companion, moved off 22 |
| **25** | **Gain ENV OSC 2** | uni | B180, moved off 23; may be subsumed by B229's envelope redesign, in which case 24/25 stay reserved |
| **26** | **Blade 1 ENV · OSC 1** | uni | SCALPEL, allocated now, shipped with per-note scope (§4.2) |
| **27** | **Blade 2 ENV · OSC 1** | uni | SCALPEL |
| **28** | **Blade 1 ENV · OSC 2** | uni | SCALPEL |
| **29** | **Blade 2 ENV · OSC 2** | uni | SCALPEL |
| 30–31 | spare | — | — |
| — | swarm lead ψ | — | **no slot** (§4.2: it rotates at the note frequency; R is its control-rate observable) |

**Proposed cap: 32.** 30 named slots plus 2 spare. The cost of any cap is the downgrade exposure
and the length of the source picker, not memory or CPU. The picker needs the same hierarchical
treatment as destinations (§4.1): Envelopes · LFOs · Macros · MIDI · Swarm (Coherence) ·
Blades.

**One consistency point.** The lead reports the lab's rule for Coherence: a per-voice source,
*gain-weighted when read globally*. The blade envelopes are also per voice, so the same
projection rule would apply to them. It does not fix what the mod lab's finding #2 measured
(ROADMAP, Modulation lab status): a globally projected per-note source re-surges on every new
note. That is why 26–29 are allocated here and shipped with per-note scope (B82), while
Coherence, which moves slowly and does not retrigger, can ship projected now.

### 4.3 Depth units and B241

- **Span-linear is wrong for the log rows.** A route's reach is `depth × (max − min)`, clamped
  (`:4021-4023`). On `kHz` (20..12000), a 10 % route moves ±1198 Hz whether the knob sits at 40 Hz
  or 8 kHz. SCALPEL's own modulators are all log: envelope ×2^(±4 oct), spreads 2^(4·spread).
  Proposal: `k`, `kHz`, `m`, `mHz`, `w` and `driftRate` (both blades) take depth in **octaves**.
  That is a per-row transform in the destination law, the same place B241 found the base intercept
  must live.
- **Position is circular.** `c`/`c2` must **wrap**, not clamp. At base 0.9, a +0.2 route must land
  at 0.1, not 1.0. The same rule applies to the morph blend: 0.95 and 0.05 must meet at 0.0, not
  0.5 (§1.4, morph column). Neither the matrix nor the morph field has a circular parameter today.
- **B241 is on `main` (#743) and covers new rows by construction.** `readParam`'s ADR-136 base
  lookup now runs first, on the full id, so a routed blade row reads back and saves its base.
  `modreadback_check` §B sweeps "every parameter the matrix accepts as a destination"
  (`tools/modreadback_check.cpp:36`), so new rows join the sweep automatically. Two conditions:
  - no SCALPEL row may be intercepted in `readParam` before the base lookup;
  - any per-row transform (the octave or wrap law above) must be applied *after* the base is
    read, never stored.
- **Smoothing absorbs the matrix's steps.** The matrix writes on the control grid; SCALPEL's 12 ms
  per-sample smoother (P rows) then de-zippers them. The cost: a 12 ms one-pole attenuates fast
  modulation (≈ −3 dB at 13 Hz). An LFO at 40 Hz on Cut rate will be audibly softened (*arithmetic*
  on τ = 12 ms).

---

## 5. Cost framing (frame only; the measurement is its own row)

**Horde today** (`docs/MEASUREMENTS.md` §2, build 319a758, 44.1 kHz, 8 held notes, 1×, polyBLEP
on): ≈ 2.1 ms CPU per second per unit of n, which is **≈ 6 ns per member-tick**. The worst cell,
32 members × 8 notes × 2 oscillators, is 14 % of one core. The stated budget is **50 %**
(`tools/cpu_bench.cpp`, per MEASUREMENTS §2). Oversampling adds ≈ 2.5× (`hypersaw_clap.cpp:313-315`).

**SCALPEL's estimate** (SPEC §8): a scalar C++ default path of 10–25 ns per member-tick at 2×, an
*estimate, not a measurement*. The JS costs are 145 ns default, 285 ns with two blades, and 550 ns
with two blades + twin + cross-mod + envelopes. If C++ scales like JS (*hypothesis*), the full path
lands at ~40–95 ns.

% of one core at 44.1 kHz, 2× oversampled, member-ticks = notes × members × 2 × 44 100 (*arithmetic*):

| notes · members · oscs | member-ticks/s | 10 ns | 25 ns | 50 ns | 95 ns |
|---|---|---|---|---|---|
| 8 · 9 · 1 | 6.35 M | 6.4 % | 15.9 % | 31.8 % | 60.3 % |
| 8 · 9 · 2 | 12.70 M | 12.7 % | 31.8 % | 63.5 % | 120.7 % |
| 8 · 16 · 1 | 11.29 M | 11.3 % | 28.2 % | 56.4 % | 107.3 % |
| 8 · 16 · 2 | 22.58 M | 22.6 % | 56.4 % | 112.9 % | 214.5 % |
| 16 · 9 · 2 (horde's `kPoly` = 16, `swarm_core.h:51`) | 25.40 M | 25.4 % | 63.5 % | 127.0 % | 241.3 % |

**Reading it:**

- SPEC §8's 7–17 % is **one oscillator** at 8 notes × 9 members.
- Horde has two oscillators and 16 notes of polyphony. At the default path's upper estimate, two
  full 9-member swarms already exceed the 50 % budget at 16 notes. At 16 members they exceed it at
  8 notes.
- The full-feature path is the real question.
- SIMD (SPEC §8.9): 8 members fill 4-wide lanes exactly and 9 leave the last group ¾ empty. With
  a cap of 9, a SIMD port pays for 12 lanes. That is D2's question, for after the scalar bench
  (B236 pattern).

**What the bench must measure** (the row after the mod-matrix design pass):

- the scalar default path;
- two blades;
- the full path;
- each at 9 and 16 members, and 1 and 2 oscillators;
- settled-start note-on spikes;
- the cost of blade-off (SPEC §8.8 promises "zero-cost when off"; horde's rev-1 path must be
  measured unchanged).

---

## 6. Questions for the structured discussion

Each question has options and a recommendation. **[settled]** means this accounting or an existing
ruling closes it; **[ruled]** means the human already ruled it.

### A. Shape and scope

- **A1 (H1) Engine block or extension?** **[ruled]** B252: an extension that is an overhaul. SCALPEL
  becomes a blade stage inside each SWARM oscillator, as per-osc rows (§1). An engine block would
  have duplicated ~30 rows and a second coupling law.
- **A2 (H2) What it replaces.** **[ruled]** Hard sync (none shipped, §1.3) and the saw-shape panel
  (69, 129–132) are RETIRED behind the revision gate.
- **A3 (D3) Blade 2 in v1?** Options: yes / defer. Recommendation: **yes**. 35 of 76 presets use it,
  and the spec makes it zero-cost when off (§8.8).
- **A4 Blade 1 switch.** Options:
  - (a) new `b1on` row (§1.7);
  - (b) default `w` 0 (bypass zone);
  - (c) default `depth` 0.

  Recommendation: **(a)**. It is honest on the face, it gives B232 a secondary gate if wanted
  (G2), and the bench's values stay as the "switched on" starting point.
- **A5 (D1) The name.** SCALPEL / BLADE / a horde-family name (not RAZOR). Open; the lead has no
  recommendation on taste. Practical note: the state-key prefix (`bl.`) is chosen so it does not
  depend on this.

### B. Coupling law and parity

- **B1 Whose coupling law?** Options:
  - (a) horde's, per SPEC §11;
  - (b) SCALPEL's normalised Keff;
  - (c) both, as a new structural *coupling law* row.

  Recommendation: **(a)**. Rev-1 identity needs it, horde's goldens encode it, and it already
  behaves as *cycles* (§1.6.2). Cost: the 76 presets need their K re-voiced (B3), and SCALPEL's
  coupling criteria get re-measured under horde's law. (c) is the fallback if the human hears
  SCALPEL's feel as essential. It adds a law to maintain, which *reduce, never invent* argues
  against.
- **B2 (H3) Parity scope.** **[ruled in part]**: the C++ port is the golden, measured against the
  JS oracle. Open: *what* is measured exactly, given B1(a). Recommendation, in three layers:
  1. The blade stage is ε-exact against the oracle with the laws aligned: K = 0, aligned phases in
     the SCALPEL frame, dist 0 / law 0, the oracle's decimator, output tapped before the output
     stage.
  2. Coupling is covered by horde's existing goldens.
  3. SCALPEL's coupling criteria (r ≥ 0.90, r ≤ 0.10, lead spread) are re-measured under horde's
     law as trajectory criteria.

  The seed edit (the ratified order's step 4) routes the oracle's 11 `Math.random` sites through
  named streams, so layer 1 can include the random laws.
- **B3 Re-voicing SCALPEL presets.** Options: by ear / by a measured K map (match R or lead
  spread at a reference pitch) / not ported. Recommendation: a **measured map as the starting
  point, finished by ear**. The laws differ in shape, so no single factor exists (§1.6.1).
- **B4 (D4) Settled start.** Options: default for new patches / opt-in / not ported.
  Recommendation: a **NEW row, default on for new (R_s) patches and off for rev < R_s**, with its
  own named RNG stream. Open sub-question: with onset lock also on, does settle run before the
  onset transient, or is onset suppressed? Recommendation: settle first, then onset. Onset is an
  authored *departure* from steady state.
- **B5 Phase origin (§1.6.6).** Options: (a) keep horde's frame and map φ_S = frac(φ_H + ½) in the
  blade maths; (b) move horde to SCALPEL's origin at R_s. Recommendation: **(a)**. Rev-1 is
  untouched, and presets translate Position by a fixed ½.
- **B6 (D5) Coupling time.** **[settled if B1 = (a)]** `cScale` MERGES into horde's law, and `absK`
  is *seconds* (§1.6.2). No new row. It reopens only under B1(b)/(c).
- **B7 Negative K at N ≥ 8.** SCALPEL's H = min(N−1, 6) leaves moments uncancelled above 7 members.
  **[settled if B1 = (a)]** Horde's rank-lattice splay has no such limit (§1.6.4).

### C. Members

- **C1 (H6/D2) The cap and its rule.** **[ruled: 9 target, test to 16]** Open: the rule and the
  final cap. Recommendation: **option (A)** of §3, a revision-gated clamp with the range kept.
  Cap **9**, arrays and tests sized to 16, SIMD 8-vs-9 decided after the scalar bench.
- **C2 The factory bank.** 31/41 patches exceed 9. Options: (a) the bank stays at its current
  revision (no blades, unchanged sound); (b) re-voice each over-cap patch at R_s; (c) split,
  keeping classic swarm patches old and adding new SCALPEL patches at R_s. Recommendation:
  **(c)**. It is also the first real test of ADR-183 §3's "bank moves forward" rule.
- **C3 Balanced pan above 9.** If the cap ever exceeds 9, tables for 10–16 do not exist and cannot
  be found exhaustively. Recommendation: defer with the cap. At cap 9, `panBalanced` is honest for
  N ≤ 9, and a heuristic search belongs to the row that raises the cap.

### D. Voice and output

- **D1 (D11) Output stage.** **[settled]** Horde's: `vol`, `normExp`, the output pole, `tanh`.
  Open: a per-engine *trim*? The bench's pads "lean on" its `tanh(1.6·gain)` (SPEC §10).
  Recommendation: no new trim row, since `vol` is the per-osc trim. Measure blade-on loudness
  against blade-off and set the bench presets' `vol` when re-voicing.
- **D2 Oversampling.** SCALPEL's AA claims assume 2×; horde's 2× is global and ships off. Options:
  - (a) an oscillator renders 2× whenever any blade is on (no new row; the CPU follows the sound);
  - (b) leave it to global 88;
  - (c) a per-osc oversample row.

  Recommendation: **(a)**. Open sub-question: whose decimator? Horde's halfband or the oracle's
  Butterworth pair. Parity layer 1 (B2) needs the oracle's, and the shipped path should use
  horde's, measured.
- **D3 (D7) Crush by box-average.** Recommendation: **yes** (normative in SPEC §4.4, verified
  continuous).
- **D4 (D8) Per-cycle DC, blocker fallback.** Recommendation: **yes**. Inert without blades
  (§1.5).
- **D5 Voicing is global.** SCALPEL's poly/mono/legato is per engine; horde's is global
  (32/34/90). Recommendation: **accept global.** Per-osc voicing is a separate feature.

### E. Blades (port data)

- **E1 `digital` scope.** Should `digital` (16) scale *every* BLEP correction, including blade
  edges, carriers and crush steps, or only the base wave? Recommendation: **every**, because it is
  SCALPEL's `aa`.
- **E2 Envelope class.** Blade-envelope rows: Morphable (per-osc timbre, like the amp ADSR) or
  Device (the precedent for global mod-source controls, 162–165, 269–288)? Recommendation:
  **Morphable** while they are internal (§4.2). Revisit if they become matrix sources.
- **E3 Sentinel rows.** `lock2`/`mirror2`/`frame2` use −1 = *same as blade 1*. Keep the sentinel as
  a stepped value (range −1..max)? Recommendation: **yes**. It is what the verifier's
  "follow-defaults" guard tests (SPEC §9).
- **E4 Enum re-indexing (§1.8.2).** Recommendation: **contiguous horde indices in display order**,
  with the oracle mapping kept in the parity harness only.
- **E5 SPEC §7 vs oracle smoothing (§1.8.3).** Recommendation: **follow the oracle**
  (depth2/I2/hard2 at control rate), and record a spec erratum. The alternative is a behaviour
  change against the golden.

### F. Spreads

- **F1 `random` and `drift` spread laws.** Both draw per note or per tick. **[settled by H3's seed
  edit]** Named streams from the osc `seed`.
- **F2 Label collision.** SCALPEL's spread *Drift rate* against horde's *Drift Rate* (10, frequency
  drift). Recommendation: label it **"Spread drift"**, key `bl.driftRate`.
- **F3 Gradient over index or pitch?** SCALPEL's *gradient* ranks by member index, which is pitch
  order in SCALPEL. In horde, index = pitch order only for dist 0/1 (and harmonic law). Gaussian,
  cauchy and golden scatter the index. Options: (a) index; (b) pitch rank (horde already sorts it
  for the fan, `swarm_core.h:1603-1612`). Recommendation: **(b)**, so "gradient" keeps meaning
  "low to high" under every distribution.
- **F4 Custom ratios (`kCustom`).** A string. Options:
  - (a) a per-osc state string, not a parameter: stored, recalled, not automatable, snaps with its
    corner;
  - (b) nine numeric ratio rows per blade;
  - (c) drop *custom*.

  Recommendation: **(a)**, parsed off the audio thread. Nine rows would add 36 host ids for a
  specialist feature.

### G. Morph

- **G1 (H5) Membership.** **[settled]** Every NEW row joins through B240's tail list. Stepped rows
  snap, continuous rows blend. B232 applies through gate 150/1150 by construction (PR #744's
  `sourceGateOf`). **Blocked on B240 landing.**
- **G2 Blade switches as gates.** Should `b1on`/`b2on` be *secondary gates*, i.e. level-ramped like
  osc enable (B48) and carrying B232's off-corner rule for their blade's rows? Without it, a blend
  between a corner with blade 2 on and one with it off snaps (a click) and pulls the on-corner's
  blade toward the off-corner's irrelevant settings. Recommendation: **yes, by declaration**. The
  B232 critic note applies: a gate whose flip does not coincide with the −60 dB floor must not
  inherit the rule as-is. So the ramp comes first.
- **G3 Circular blend for Position.** Recommendation: a **shortest-arc blend** for `c`/`c2` (a
  per-row flag in the morph law), and the same for the matrix (§4.3). It is the first circular
  parameter either system has.

### H. Mod matrix

- **H1 (H4) Destination set.** Recommendation: 45 per osc (§4.1), a hierarchical picker, tiered
  offering, and an eligibility override list in the shell.
- **H2 Depth units.** Octave-depth for the log rows and wrap for Position (§4.3). This changes the
  destination law for those rows only, and the rows are new, so no existing route changes.
  Recommendation: **yes**.
- **H3 New sources.** Blade envelopes internal for v1, ψ not a source, R via B253. Recommendation
  as stated.
- **H4 The source cap and slot order (§4.2a).** Options: (a) raise `kMaxSources` to 32 now, with
  the slot list fixed (Coherence 22/23, Gain ENVs 24/25, blade envelopes 26–29); (b) raise it only
  when each source ships, allocating first-come. Recommendation: **(a) for the list, (b) for the
  constant.** Fix the slot numbers in the ROADMAP now, so B180 and B253 stop contending for 22/23.
  Raise the constant in the first PR that needs a slot ≥ 24. Open for the lead: B180 or B253 on
  22/23? The table follows the lab (B253).

### I. Interface

- **I1 The ranking (§2).** Is the blade core above the surviving swarm controls on the face, as
  proposed? This is the human's call by their own words.
- **I2 The blade selector at 980×720.** A tab pair, or both blades stacked? It interacts with
  B227's open window-size conflict.

### J. Cross-repo

- **J1 (H7) FOUNDATIONS utilities.** Blade geometry, BLEP event scanning, per-cycle DC and the
  balanced pan table (SPEC §11). Open, and later: a brief after the scalar port proves the
  interfaces.
- **J2 (D10) Pads → intents.** **[settled by ADR-176]** The packet's pad-assignable flag (46 of
  51 continuous NEW rows) becomes the candidate list for intent bindings. Pads are not ported
  (SPEC §1, *Do not port*).
- **J3 (D9) Wavetable export.** **[settled]** DEFERRED (§1.4).

### Cross-reference: the packet's D1–D11 and ADR-184's H1–H7

| id | topic | here | status |
|---|---|---|---|
| D1 | name | A5 | open |
| D2 | members 9 or 8 | C1 | ruled 9; SIMD after bench |
| D3 | blade 2 in v1 | A3 | recommend yes |
| D4 | settled start default | B4 | recommend new-patch default |
| D5 | coupling time | B6 | settled if B1(a) |
| D6 | balanced pan engine-wide | §1.7 `panBalanced`, C3 | proposed as a per-osc row; ≤ 9 |
| D7 | crush box-average | D3 | recommend yes |
| D8 | per-cycle DC | D4 | recommend yes |
| D9 | wavetable export | J3 | settled: deferred |
| D10 | pads → intents | J2 | settled by ADR-176 |
| D11 | horde output stage | D1 | settled; trim open |
| H1 | shape | A1 | ruled |
| H2 | replaces | A2 | ruled |
| H3 | parity | B2 | ruled in part |
| H4 | mod matrix | §4, H1–H3 | proposed |
| H5 | morph | G1 | settled; blocked on B240 |
| H6 | cost / members | C1, §5 | ruled 9/16; bench owed |
| H7 | FOUNDATIONS | J1 | later |

---

## Appendix: evidence index

- Horde parameter table: `src/hypersaw_clap.cpp:174-776`. Globals: `:844-899`. Classes:
  `:1455-1656`. Morph order: `:2936-3099`. Destinations: `:3541-3556`, `:4013-4031`. Load clamp:
  `:6910`.
- Presentation: `src/param_presentation.tsv` (osc-1 rows :71-393).
- Swarm core: `src/swarm_core.h`:
  - member laws `:1773-1838`;
  - σ `:1847-1853`;
  - coupling `:1885-2092`;
  - phases `:693-702`;
  - pan `:1558-1661`;
  - saw `:1058-1066`;
  - output `:943`, `:1331`;
  - rebuild keys `:476-478`;
  - `kMaxV` `:41`, `kPoly` `:51`.
- clap-wrapper normalisation: `libs/clap-wrapper/src/detail/vst3/parameter.cpp:123-130`.
- Packet (`origin/ingest-scalpel`, `aaa2e0e`): `specs/SPEC-SCALPEL.md`;
  `reference/scalpel/prototype/razor-core.js` (the lines cited inline);
  `reference/scalpel/prototype/scalpel-bench.html:1238-1376` (stored values);
  `reference/scalpel/data/parameters.json`, `data/presets.json`.
- Factory bank: `docs/presets/factory/**` (41 patches + 4 corner presets). CPU:
  `docs/MEASUREMENTS.md` §2.
- Rows: B232 (PR #744, open), B240 (dispatched), B241 (on `main`, #743), B253, B227, B212
  (`origin/lead-records-86` ROADMAP). ADR-173, ADR-183 (`DECISIONS.md` on `main`). ADR-184
  (PR #745).
