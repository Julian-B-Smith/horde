# The blade-cycle table generator (`blade-bake`): the spec

> **Origin.** Drafted for the horde lead session, 2026-11-09, on ROADMAP B663 and ADR-388 #1 (the human, by poll:
> "Bake blades into tables"). Every existing patch uses blades (all 83 factory presets, 14 of the 16 demos); under
> ADR-387 the wavetable member replaces the blades for 1.0, so an old patch must load as the nearest wavetable sound,
> not as a plain shape. This file specifies the generator that renders a patch's blade settings into tables.

**Status.** PROPOSED, for the lead's review and the human's. Nothing here is built. Its requirements are `BB-n`, its
gates `BB-Fn`; its open choices are in CONTRACT §7 group J, the one decision list.

**Why a file of its own, not a SPEC-WAVETABLE section.** It is a generator with its own law (the blade law, frozen),
its own inputs (retired rows, which no other part of the wavetable member reads), its own migration duty and its own
gates; folded into SPEC-WAVETABLE it would be the largest section of a spec whose subject is the playing oscillator.
SPEC-WAVETABLE WT-48 points here; WT-40 to WT-47 (recipes, goldens, the table gates) apply to it unchanged.

---

## 1. What it does

At load, for each oscillator and each morph corner of an old patch, the generator reads the corner's blade settings
and base wave, renders the cycle the old engine played, and turns it into a 16-frame table (WT-1). The migration
puts that table in the corner (`oscN.wt.table`), sets the wavetable rows that carry what a static table cannot, and
writes a **load report** naming every setting that was not carried.

---

## 2. What it reads

| id | requirement |
|---|---|
| BB-1 | **Per oscillator, per corner,** the corner's values of the retired rows (CONTRACT §2.4): both blades' rows (SPEC-ENGINE 5.4), `baseWave`, `quantize`, `stacking`, `upperHears`, `collisionPitch`, `collisionBite`, and the corner's `ratios` text (MO-13); the oscillator's `seed` (for the blade's noise draws, DT-4); and the patch's modulation routes whose destination is a retired blade row (B.1 writes the bank's blade envelopes as ENV 2 and ENV 3 routes to `blade1.cut`, `cutHz` and `width`). The retired rows are kept in the saved state for this purpose (ADR-388 #3). |
| BB-2 | **Nothing live.** Swarm, pitch, pan and envelope rows are not read and not changed: the swarm plays the baked table exactly as it played the base wave. |
| BB-3 | **Where the old values come from:** the version-1 engine document and the morph system's corners (IF-4, RC-11.1), or the test plug-in's chunks of versions 2 to 4 (`bridge_core.h`), read by the same migration. |

---

## 3. The law: what it renders

| id | requirement |
|---|---|
| BB-10 | **The blade law is SPEC-ENGINE's C.9 to C.11 and C.16** (the law horde 2's patches were saved under, ADR-224), not SCALPEL's prototype. `reference/scalpel/prototype/razor-core.js` (read-only) is the cross-check for the bank's origin: a reported comparison, never a parity target. |
| BB-11 | **One member, at rest** (the single-member cycle; whether that is enough for swarm presets is CONTRACT §7 J9). The cycle is one member's (U = 1): every spread's position is 0 (BL-13), so spreads and cut rules vanish; no drift, coupling, onset, inertia or pan; `bandLimit` 1; rotation 0 (rotation is carried by the frame axis, BB-20); blade noise from the oscillator's seed's first note stream (BL-18, DT-4), **its draws those of the cycle taken** (BB-14's 4th), so the bake is deterministic. |
| BB-12 | **The law, and how it is computed** (CONTRACT §7 J8). The law is the clean-room engine's single-member blade path (SPEC-ENGINE C.9 to C.11, C.16, with ED-1, ED-6, ED-7's corrections). **Recommended: the generator is a focused C++ port of that path**, one member, one voice, shipped in the plug-in; **the frozen clean-room engine is its oracle, run only in the build repo**, never shipped (BB-F12). **The oracle's tap:** `HordeEngine::prepare(192000, block)` called directly (`api/engine.cpp` line 19 accepts 8 to 768 kHz; the offline entry `horde_offline_render` refuses any rate but 44.1 and 48 kHz, `api/offline.cpp` line 120); osc 1 only, the sub, the noise and osc 2 off; **no routes** (B.1's blade-envelope routes are data beside the state, `offline.cpp` lines 82 to 112, and the oracle passes none); ENV 1 at its shortest attack (0.5 ms), sustain 1; one note held at 23.4375 Hz, **8,192 samples per cycle**, Standard quality; the output taken from the oscillator's mono member sum. The edge correction band-limits near 0.47 × 192 kHz, about 3,850 harmonics, so harmonics 1 to 1,024 lie well inside the pass band. |
| BB-13 | **Every pitch- or rate-dependent law is evaluated at f_ref = 261.63 Hz (C4) and 44.1 kHz** (PROPOSED), not at the render's 23.4 Hz and 192 kHz, so the baked cycle is the one the old engine played at C4. The laws: **Hz settings** (`cutIn` Hz, `modIn` Hz), converted to ratios at f_ref; **the carrier cap** (BL-4, C.9 item 8: 0.45 of the internal rate); **the sine→saw carrier's brightness** r_max = min(0.995, 0.01^(f_h/Nyq)) (C.9.3, SPEC-ENGINE line 1698; DT-3 already exempts these presets); **ED-7's taper weight c(T)**, T in samples at f_ref and 44.1 kHz; **SW-13's band-edge fade** (inactive at C4). Each is computed at f_ref and fed to the render as a fixed value. A fixed-Hz formant therefore tracks the pitch after the bake unless BB-31 carries it as Hz sync; the report says which. |
| BB-14 | **The steady cycle.** The render runs 6 cycles; the 4th is taken (its edge corrections and C.16's estimate have settled, SW-15), as one exact period of 8,192 samples. Its 8,192-point FFT gives harmonics 1 to 1,024; DC is dropped (WT-1); each frame is peak-normalised (WT-2). |
| BB-15 | **Polarity** follows the base wave as the engine plays it (`wave(base, θ + ½)`, WT-20), so a baked base wave and the factory Sine → saw agree in sign. |

---

## 4. The frame axis

| id | requirement |
|---|---|
| BB-20 | **The rule** (PROPOSED, CONTRACT §7 J1). **(a) Static blades** (no rotation, no route into a blade row, no FM free or noise motion): a **single-frame table**, the corner's cycle in all 16 frames, declared single-frame so WT-47's "frames differ" gate does not apply; or, if the corner has a member spread (J9's mapping), frames at spread positions s = −1 + 2k/15 and the spread mapped to `wt.frameSpread`. **(b) One moving parameter:** the frames sweep it. **Rotation** (`rotate` ≠ 0): frame k is the cycle with the window at `position` + k/16 of a turn; an LFO (a saw at \|`rotate`\| Hz, falling for negative rates) drives `wt.frame` at full depth: a per-note LFO for "restart per note", a global LFO for "free-running" (BL-8). **An envelope route** into `cut`, `cutHz`, `width`, `position` or `depth`: frame k is the cycle with that row at the route's start value plus k/15 of the route's whole swing (start to start + depth × 1), **ordered along the swing's sign**, so frame 15 is the swing's far end; the migration routes the same envelope to `wt.frame` with **depth 1** (positive polarity, the same curve) and sets `wt.frame`'s base to 0, so the envelope crosses all 16 frames as it crossed the swing. **(c) Several moving parameters:** the axis takes the one with the largest swing in v (ties: manifest order); the rest go to the report. **(d) No free slot:** if the patch has no free LFO or route slot for (b)'s LFO or route (E.5, MS-6's maxima), the corner bakes as (a) and the report names the motion "not carried: no free modulation slot". |
| BB-21 | **What the axis loses.** Sixteen frames are coarse for a full turn: between frames the table blends two window positions instead of moving one window, and the saw LFO's wrap from frame 15 to 0 steps by 1/16 of a turn once per turn. The report says "rotation approximated: 16 positions per turn". |

---

## 5. What maps, and what the report names

Per corner, each retired row away from its default is one of: **carried exactly** (in the baked cycle), **carried by
other rows** (approximately, named), or **not carried** (named with its value). By family, using B652's estimate of
SCALPEL's 83 presets (`traces/2026-11-08-b652-wavetable-lab-fm.md`):

| family (presets) | what the bake does | report |
|---|---|---|
| static sync, fold, ring, crush at × pitch, FM reset, serial pairs and static collisions (about 30) | baked into the cycle (BB-20 (a)) | carried exactly, at rest |
| blade envelopes (7) | frame axis over the swept row; the route moved to `wt.frame` (BB-20 (b)) | carried by the frame route (approximate: the frame axis samples the sweep at 16 points) |
| rotation, moving blades (part of 10) | frame axis plus an LFO (BB-20 (b), BB-21) | approximate |
| a whole-cycle Sync blade (`width` ≥ 0.999, `depth` 1) | **BB-31:** the table is the carrier wave; `wt.syncIn` takes the blade's unit (× pitch, or Hz), `wt.sync` its `cut` or `wt.syncHz` its `cutHz`; `wt.syncSoft` takes the blade's `edges` | carried by sync (exact at × pitch; at Hz the formant stays fixed, as it was) |
| FM free and pitch jitter over the whole cycle (10) | **BB-32:** the table is the carrier wave; the internal modulator (WT-70) takes the blade's mod shape, rate unit and rate, FM type and index, and the blade's `depth` as the modulator's mix, per B652's mapping (Trance, Operator, Jitter swarm) | carried by the internal modulator; **not carried while WT-70 is unruled** (CONTRACT §7 J4) |
| FM free inside a partial window, noise blades (part of 10, 2) | baked with the modulator and the noise frozen at the taken cycle's draws (BB-11) | approximate: "frozen into one cycle" |
| fixed-Hz formants in a partial window (part of 7) | baked at f_ref (BB-13) | approximate: "now tracks the pitch" |
| cross-mod and feedback (14) | the rows stay live and now push the table read (WT-31) | approximate: "the loop now moves the table read, not the blade's carrier" |
| per-member spreads and cut rules (`kRule`, `ratios`), random and drift spread laws | vanish in a one-member bake (BB-11); the spread of the frame axis's row maps to `wt.frameSpread` under the same spread law | approximate for that one; not carried for the rest |
| collision chirps under motion, crush in Hz | baked at rest | not carried: their motion |

Roughly, as B652 estimated: a third of the bank carried closely, a third approximately, the rest in part.

| id | requirement |
|---|---|
| BB-30 | **The report is complete.** Every retired row the old patch set away from its default **and that was live under PA-8 in the old patch** (a blade row with its blade off is not reported) appears in the report, per oscillator, under exactly one of the three verdicts, deduplicated across corners that agree. **`baseWave` is always considered**: it is retired, so a non-saw base wave is reported (carried exactly, in the baked cycle). A patch with every blade off and a saw base wave has an empty report. |
| BB-31 | **Whole-cycle Sync blades become sync**, as the table above says, because the wavetable member's sync on the carrier wave is the same sound (a Sync blade over the whole cycle at depth 1 restarts its carrier every member cycle). |
| BB-32 | **Whole-cycle FM free becomes the internal modulator** once WT-70 is ruled; until then it is baked at rest and reported. |
| BB-33 | **Between corners.** Each corner bakes alone. Where the old patch blended blade rows parametrically between corners (MO-9), the pad now blends the corners' tables (WT-11): the report says so once per patch. Under Quantum the corners flip as before. Corners whose recipes are identical share one id and pool (WT-53). |

---

## 6. The recipe

| id | requirement |
|---|---|
| BB-40 | **Form:** `{"v":1, "kind":"gen", "gen":"blade-bake", "genV":1, "set":{…}, "seed":<the oscillator's seed>}` (CONTRACT §3.1). `set` holds: the base wave; every retired row of the corner **away from its default**, keyed by its row key without the oscillator prefix, valued in normalised v (the canonical number form of §3.1); the `ratios` text if it is not the default; the frame axis (BB-20: none, or the row, its start and swing, or `rotate`); and `fRef`. Deterministic, self-contained: it rebuilds alone (WT-42). |
| BB-41 | **Size:** at most **3 KiB** of canonical text. **Keys are the rows' block-relative ids in hex** (frozen by RC-9.1), not their names: named keys average about 37 to 40 bytes a row and would reach about 2.9 to 3.1 KiB at worst (the critic's estimate). With id keys a row is at most about 32 bytes (a 3-digit key, quotes and colon, and a value of up to 22 characters in the canonical shortest form), so all 68 retired rows are at most about 2.2 KiB, a `ratios` text at most 256 bytes and the header and frame axis about 150 bytes: **about 2.6 KiB at worst**, under 3 KiB; S0 confirms it on the worst-case recipe (BB-F8). The patch bound becomes: 64 recipes, each at most 512 bytes, or 3 KiB for a `blade-bake` recipe, 32 KiB in all (CONTRACT §3.2). Old patches have two oscillators, so a migrated patch needs at most 8 bake recipes: at most 24 KiB. |
| BB-42 | **The mapped rows** (BB-31, BB-32, the frame routes) are written into the corners and the modulation state by the migration, as ordinary values; they are not part of the recipe. |

---

## 7. Where it runs

| id | requirement |
|---|---|
| BB-50 | **At load, in the migration** from IF-4 version 1 to 2 (and the test plug-in's chunk migration), on the main thread, inside WT-18's complete build before the load returns. **Never on the audio thread.** |
| BB-51 | **Cost** (PROPOSED, measured at S6c): per frame, one render of 6 × 8,192 samples of one member and one 8,192-point FFT, a few milliseconds; a worst-case patch (8 corner tables, 16 swept frames each) under one second. Identical recipes bake once. A cache keyed by recipe id is allowed (derived data, never state). |

---

## 8. The C++ port, versions and goldens

| id | requirement |
|---|---|
| BB-60 | **What ships** (CONTRACT §7 J8). Recommended: **only the focused port** (BB-12), in the wavetable core (`core/table/generators/blade_bake/` in the build repo, then in horde's tree with the rest of the wavetable code), in its own namespace so its symbols never meet the engine's (`namespace horde` would otherwise define the same class names twice), sized for one voice and one member, so a bake allocates kilobytes, not the engine's 43 MB of 64-voice maxima. **The frozen clean-room engine stays in the build repo** (`vendor/engine2/`, read-only) as the oracle. Not recommended: vendoring the whole engine into the plug-in, which was never ruled (ADR-388 #1 says "ported to C++"), would be a dependency under the charter's gate, would publish the clean-room engine's source in horde's public tree, and needs symbol isolation and a one-voice prepare. When the blade code leaves the engine (§7 I22), the port stays: old patches need it as long as `genV` 1 is buildable (WT-43). |
| BB-61 | **Versions.** A change of the frozen engine, f_ref, the axis rule or the render is a new `genV`; every old version stays buildable. |
| BB-62 | **Goldens:** the spectra hash, on the reference build, of every corner of the 83 bank presets and the 16 demos, and of a fixed grid of single-blade settings; coefficients within 10⁻⁶ of peak on any build (WT-43). |

---

## 9. Gates

Plants: **(D)** detector, proven engine-free at S0; **(M)** mutation, run at the stage's close. All PROPOSED. BB-F1, F2, F3 (a), F5 to F8, F10 and F12 close **S6c**; BB-F3 (b), F4, F9 and F11 run at **S8**, where IF-4's version 2 and the test plug-in exist.

| id | target | method | plants | control |
|---|---|---|---|---|
| BB-F1 | **Determinism:** one recipe, two runs, two threads: bit-identical spectra | compare | (M) a draw from an unseeded stream in the noise blade | two runs |
| BB-F2 | **Band-limit accuracy:** harmonics 1 to 1,024 at 8,192 samples per cycle within 10⁻⁵ of peak of a 16,384-sample render | compare | (M) 1,024 samples per cycle: the band edge falls near harmonic 480 | the render against itself |
| BB-F3 | **The base-wave control.** (a) At S6c: a patch with every blade off bakes to the base wave's band-limited Fourier series (with WT-20's polarity) within 10⁻⁵ of peak. (b) **At S8** (it needs IF-4's version 2 and the test plug-in): the migrated patch's held C4 against the old engine's render, by **DT-3's method: the first 16 harmonics below 18 kHz within 0.5 dB** (TEST-PLAN Q-10) | engine-free series; renders | (M) the base wave option read as the next in its list, so a triangle bakes as a saw: **harmonic 2** is the detector (a triangle has none, a saw's is −6 dB from its fundamental); harmonic 3 is only about 6 dB off | the bake against its own series |
| BB-F4 | **Static blades at rest** (at S8). For every bank corner classed static (BB-20 (a)), the migrated patch's held C4 at that corner (one member, the morph off) against the old engine's: DT-3's method, 0.5 dB, judged; every other corner reported | renders | (M) blade 2 dropped from the bake: two-blade static corners fail | a single-blade corner baked twice |
| BB-F5 | **Goldens** (BB-62) | hashes; coefficients | (M) one constant of the frozen copy nudged by an ulp | the golden build against itself |
| BB-F6 | **The report is complete** (BB-30) | every retired row away from default in the bank and the demos is checked against the report | (M) one mapping line deleted: its row must appear as not carried, never vanish | a blade-free patch: an empty report |
| BB-F7 | **Table gates** (WT-47), single-frame declared tables allowed | WT-47's checks | (D) a bake of a corner whose crush leaves a near-silent frame: refused and reported | the bank's bakes pass |
| BB-F8 | **Size** (BB-41) | the worst-case recipe, every row away from default | (D) the full row set written uncompacted (over 3 KiB) | the bank's recipes |
| BB-F9 | **Never on the audio thread** (BB-50; at S8) | RealtimeSanitizer over a load in the test plug-in | (M) a bake triggered from the process call: reported | a normal load: no report |
| BB-F10 | **A blade envelope reaches every frame** (BB-20 (b)): on Blade pluck (a bank preset with an ENV 2 route to `cut`), the migrated patch's `wt.frame` crosses 0 to 1 as the envelope runs its full swing, and its frame-by-frame spectra track the old engine's at matching envelope levels within 1 dB per harmonic (PROPOSED) | observation (IF-10); renders | (M) the route kept at its old depth: the frame stops near 0.3 | a static corner: the frame stays at 0 |
| BB-F11 | **Swarm presets, reported** (J9): the old engine at U = 9 (the bank's own unison) against the migrated patch at U = 9, by DT-3's method and by ear; reported, never judged until J9 is ruled | renders | — | U = 1 against U = 1 (BB-F4) |
| BB-F12 | **The port equals its oracle** (BB-12): the focused port's cycle against the frozen engine's (BB-12's tap), every corner of the bank and the demos: within 10⁻⁶ of peak per harmonic | compare, in the build repo | (M) one blade law constant changed in the port | the oracle against itself |
