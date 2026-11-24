# The blade-cycle table generator (`blade-bake`): the spec

> **Origin.** Drafted for the horde lead session, 2026-11-09, on ROADMAP B663 and ADR-388 #1 (the human, by poll:
> "Bake blades into tables"). Every existing patch uses blades (all 83 factory presets, 14 of the 16 demos); under
> ADR-387 the wavetable member replaces the blades for 1.0, so the bank and the demos are baked once, offline, into
> their nearest wavetable sound, as raw material for the factory patch session; no old session loads (ADR-406). This
> file specifies the bake that renders their blade settings into tables.
> Amended 2026-11-09 (B666) on ADR-390: the recipe bound 2.5 KiB (BB-41, BB-F8; S0 item 4) and J9 (BB-11, BB-20,
> BB-F11).
> Amended 2026-11-24 (B706) on ADR-406: option 2, once offline, nothing blade-era loads.

**Status.** PROPOSED, for the lead's review and the human's, except where ADR-406 ruled: option 2 (BB-11, BB-20 (a))
and once, offline, with nothing shipped (BB-50, BB-60). Nothing here is built. Its requirements are `BB-n`, its gates
`BB-Fn`; its open choices are in CONTRACT §7 group J, the one decision list.

**Why a file of its own, not a SPEC-WAVETABLE section.** It is a generator with its own law (the blade law, frozen),
its own inputs (the bank and demo files, which no part of the wavetable member reads), its own run (once, offline)
and its own gates; folded into SPEC-WAVETABLE it would be the largest section of a spec whose subject is the
playing oscillator.
SPEC-WAVETABLE WT-48 points here. Of WT-40 to WT-47, the table gates (WT-47) and the goldens' freezing (BB-62, by
WT-43's tiers) apply to it; rebuilding from the recipe holds in the build repo (BB-40, BB-F1) but not in the plug-in, since no `blade-bake`
generator ships: a deleted baked library file, with no embedded spectra, falls to the corner's default factory table,
named in the load report (WT-45 and WT-F30's one exception, ADR-406).

---

## 1. What it does

Once, offline, in the build repo (ADR-406), for each oscillator and each morph corner of the 83 bank presets and the
16 demos, the generator reads the corner's blade settings, base wave and unison, renders on the frozen clean-room
engine the cycle each member played (BB-11), and turns them into a 16-frame table (WT-1). The bake's state writer
puts that table in the corner (`oscN.wt.table`), sets the wavetable rows that carry what a static table cannot, and
writes a **bake report** naming every setting that was not carried. The bake report is for the lead and the human
(the listening batch, the factory patch session); it is never shown to users. Nothing bakes at load: no bake code
ships in the plug-in, and no blade-era state loads in the wavetable plug-in. The baked presets are raw material for
the factory patch session (ADR-406).

---

## 2. What it reads

| id | requirement |
|---|---|
| BB-1 | **Per oscillator, per corner,** of each source preset (BB-3): the values of the rows CONTRACT §2.4 retires: both blades' rows (SPEC-ENGINE 5.4), `baseWave`, `quantize`, `stacking`, `upperHears`, `collisionPitch`, `collisionBite`, and the corner's `ratios` text (MO-13); the oscillator's `seed` (for the blade's noise draws, DT-4); the oscillator's unison U and the spread rows that place its members (C.10; option 2, BB-11); and the preset's modulation routes whose destination is a blade row (B.1 writes the bank's blade envelopes as ENV 2 and ENV 3 routes to `blade1.cut`, `cutHz` and `width`). They are read from the bank and demo files, not from saved state (ADR-406): the retired rows a version-2 state keeps are read by nothing. |
| BB-2 | **Nothing live is changed** but the frame rows and, for a swarm-framed blade, the spread law (BB-20; ADR-406); the wavetable rows the bake maps are BB-42's. Swarm, pitch, pan and envelope rows are not otherwise changed, and are read only for the member positions (BB-1, BB-11): at U = 1 the swarm plays the baked table as it played the base wave, and at U above 1 each member reads the frame at its position (BB-20 (a)). |
| BB-3 | **Where the old values come from** (ADR-406): the 83 bank presets, `engine2/data/bank/presets.json` (B.1, single states), and the 16 demos, `tools/engine-preview/demo_bank.json` (four-corner morph records: `common`, `corners`, `morph`), each as the frozen engine reads it. The demos need horde's demo loader (`tools/engine-preview/demo_bank.cpp`, `gen_bank.cpp`) to become states; the build repo's s0 tools render bank presets only. No DAW session, test plug-in chunk (versions 1 to 5, `bridge_core.h`) or version-1 document is an input; the version-0 states stay clean-room fixtures and are never baked. |

---

## 3. The law: what it renders

| id | requirement |
|---|---|
| BB-10 | **The blade law is SPEC-ENGINE's C.9 to C.11 and C.16** (the law horde 2's patches were saved under, ADR-224), not SCALPEL's prototype. `reference/scalpel/prototype/razor-core.js` (read-only) is the cross-check for the bank's origin: a reported comparison, never a parity target. |
| BB-11 | **One cycle per member, at the preset's own unison, at rest** (option 2, ADR-406; it replaces ADR-390's J9 answer, spreads as frames). For a corner whose unison U is above 1, the bake takes U cycles, one per grid position p = j/(U − 1) − ½ (j = 0 … U − 1): **member j rendered under the gradient law, whatever the preset's own law**, as the old engine played that member at the preset's U, its spread positions and its cut rule (C.10) included, so everything a member's position decides is in its cycle. BB-20 (a) places them in the frames: frame k0 + j holds position j's cycle. Rendering under the preset's own law instead would put the wrong cycle in an end frame (under alternate at odd U, member U − 1 sits at −½). **The frames fit only that U:** a user who changes a baked preset's unison hears the baked frames at the new count (the members read other members' cycles or blends of two, a sparser spread of the original texture), not what the old engine played at that count; there is no re-bake (ADR-406). At U = 1 the cycle is the single member's: every spread's position is 0 (BL-13), so spreads and cut rules vanish. For every member, at rest: no drift, coupling, onset, inertia or pan; `bandLimit` 1; rotation 0 (rotation is carried by the frame axis, BB-20); blade noise from the oscillator's seed's first note stream (BL-18, DT-4), **its draws those of the cycle taken** (BB-14's 4th), so the bake is deterministic. |
| BB-12 | **The law, and how it is computed** (CONTRACT §7 J8, as amended by ADR-406). The law is the clean-room engine's blade path (SPEC-ENGINE C.9 to C.11, C.16, with ED-1, ED-6, ED-7's corrections). **The generator is the frozen clean-room engine itself** (at `6cd98b5`, exported read-only by `git archive` into the build repo's work tree, as `tools/s0_1` does), run offline in the build repo; there is no port, and nothing ships (BB-60). **The tap at U = 1:** `HordeEngine::prepare(192000, block)` called directly (`api/engine.cpp` line 19 accepts 8 to 768 kHz; the offline entry `horde_offline_render` refuses any rate but 44.1 and 48 kHz, `api/offline.cpp` line 120); osc 1 only, the sub, the noise and osc 2 off; **no routes** (B.1's blade-envelope routes are data beside the state, `offline.cpp` lines 82 to 112, and the tap passes none); ENV 1 at its shortest attack (0.5 ms), sustain 1; one note held at 23.4375 Hz, **8,192 samples per cycle**, Standard quality; the output taken from the oscillator's mono member sum. **At U above 1 the tap isolates one member's cycle, the corner rendered under the gradient law** (option 2, BB-11). How is the bake build's first task, gated against the engine's own render of that member (BB-F13) before any table is baked. The edge correction band-limits near 0.47 × 192 kHz, about 3,850 harmonics, so harmonics 1 to 1,024 lie well inside the pass band. |
| BB-13 | **Every pitch- or rate-dependent law is evaluated at f_ref = 261.63 Hz (C4) and 44.1 kHz** (PROPOSED), not at the render's 23.4 Hz and 192 kHz, so the baked cycle is the one the old engine played at C4. The laws: **Hz settings** (`cutIn` Hz, `modIn` Hz), converted to ratios at f_ref; **the carrier cap** (BL-4, C.9 item 8: 0.45 of the internal rate); **the sine→saw carrier's brightness** r_max = min(0.995, 0.01^(f_h/Nyq)) (C.9.3, SPEC-ENGINE line 1698; DT-3 already exempts these presets); **ED-7's taper weight c(T)**, T in samples at f_ref and 44.1 kHz; **SW-13's band-edge fade** (inactive at C4). Each is computed at f_ref and fed to the render as a fixed value. A fixed-Hz formant therefore tracks the pitch after the bake unless BB-31 carries it as Hz sync; the report says which. |
| BB-14 | **The steady cycle.** The render runs 6 cycles; the 4th is taken (its edge corrections and C.16's estimate have settled, SW-15), as one exact period of 8,192 samples **at U = 1**. At U above 1 the cycle is taken per member, one period of that member; how a member's period is taken (its detune included) is BB-12's tap, gated by BB-F13. Its 8,192-point FFT gives harmonics 1 to 1,024; DC is dropped (WT-1); each frame is peak-normalised (WT-2). |
| BB-15 | **Polarity** follows the base wave as the engine plays it (`wave(base, θ + ½)`, WT-20), so a baked base wave and the factory Sine → saw agree in sign. |

---

## 4. The frame axis

| id | requirement |
|---|---|
| BB-20 | **The rule** (PROPOSED, CONTRACT §7 J1). **(a) Static blades** (no rotation, no route into a blade row, no FM free or noise motion): at **U = 1**, a **single-frame table**, the corner's cycle in all 16 frames, declared single-frame so WT-47's "frames differ" gate does not apply. **At U above 1** (option 2, ADR-406; it replaces ADR-390's spread positions s = −1 + 2k/15): **the frames are indexed by member position p.** A member's cycle depends only on p and on U (C.10: the spreads' offsets and the cut rule's x = (p + ½)(U − 1)), so frame k0 + j holds the cycle at p = j/(U − 1) − ½ (j = 0 … U − 1; member j's under the gradient law, BB-11), with k0 = ⌊(16 − U)/2⌋; the frames below k0 repeat frame k0, and those above k0 + U − 1 repeat that frame. The bake writes `wt.frameSpread` = (U − 1)/15 and `wt.frame` = (k0 + (U − 1)/2)/15, so a member at position sᵢ reads frame k0 + (sᵢ + ½)(U − 1) (WT-22). **By spread law:** under gradient every member lands exactly on its own frame, and under alternate (p = ±½) exactly on the two end frames; under random (one bank preset) the positions fall between frames and blade 2's draws are independent of blade 1's, and under drift (one demo, String Machine) they move and reach ±0.75, beyond the end frames, so under both the texture is approximate and reported. **The preset's spread law is kept**, except that a corner whose blade frame follows the swarm gets `spreadLaw` swarm (the swarm frame equals the member frame plus that blade's position spread of 1 under the swarm law, the lead's `s0-3` render); the swarm then moves its members across the frames. The texture holds under Linear, the default; under Spectral log frame spread is inert (WT-22). **(b) still takes precedence:** a corner whose frame axis carries one moving parameter keeps (b); the axis takes the motion, and the member texture is reported, not carried. **(b) One moving parameter:** the frames sweep it. **Rotation** (`rotate` ≠ 0): frame k is the cycle with the window at `position` + k/16 of a turn; an LFO (a saw at \|`rotate`\| Hz, falling for negative rates) drives `wt.frame` at full depth: a per-note LFO for "restart per note", a global LFO for "free-running" (BL-8). **An envelope route** into `cut`, `cutHz`, `width`, `position` or `depth`: frame k is the cycle with that row at the route's start value plus k/15 of the route's whole swing (start to start + depth × 1), **ordered along the swing's sign**, so frame 15 is the swing's far end; the bake's state writer routes the same envelope to `wt.frame` with **depth 1** (positive polarity, the same curve) and sets `wt.frame`'s base to 0, so the envelope crosses all 16 frames as it crossed the swing. **(c) Several moving parameters:** the axis takes the one with the largest swing in v (ties: manifest order); the rest go to the report. **(d) No free slot:** if the patch has no free LFO or route slot for (b)'s LFO or route (E.5, MS-6's maxima), the corner bakes as (a) and the report names the motion "not carried: no free modulation slot". |
| BB-21 | **What the axis loses.** Sixteen frames are coarse for a full turn: between frames the table blends two window positions instead of moving one window, and the saw LFO's wrap from frame 15 to 0 steps by 1/16 of a turn once per turn. The report says "rotation approximated: 16 positions per turn". |

---

## 5. What maps, and what the report names

The report is the bake report (§1): for the lead and the human, never shown to users (ADR-406). Per corner, each retired row away from its default is one of: **carried exactly** (in the baked cycle), **carried by
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
| per-member spreads and cut rules (`kRule`, `ratios`), random and drift spread laws | at U above 1 under BB-20 (a), each member's cycle at rest in its own frame (option 2, BB-11; ADR-406); under BB-20 (b), not baked | (a): carried at the preset's U, at rest (exactly under the gradient and alternate laws, approximately under random and drift); their motion not carried; (b): not carried |
| collision chirps under motion, crush in Hz | baked at rest | not carried: their motion |

Roughly, as B652 estimated: a third of the bank carried closely, a third approximately, the rest in part.

| id | requirement |
|---|---|
| BB-30 | **The bake report is complete.** Every retired row the source preset set away from its default **and that was live under PA-8 in the source preset** (a blade row with its blade off is not reported) appears in the report, per oscillator, under exactly one of the three verdicts, deduplicated across corners that agree. **`baseWave` is always considered**: it is retired, so a non-saw base wave is reported (carried exactly, in the baked cycle). A patch with every blade off and a saw base wave has an empty report. |
| BB-31 | **Whole-cycle Sync blades become sync**, as the table above says, because the wavetable member's sync on the carrier wave is the same sound (a Sync blade over the whole cycle at depth 1 restarts its carrier every member cycle). |
| BB-32 | **Whole-cycle FM free becomes the internal modulator** once WT-70 is ruled; until then it is baked at rest and reported. |
| BB-33 | **Between corners.** Each corner bakes alone. Where the old patch blended blade rows parametrically between corners (MO-9), the pad now blends the corners' tables (WT-11): the report says so once per patch. Under Quantum the corners flip as before. Corners whose recipes are identical share one id and pool (WT-53). |

---

## 6. The recipe

| id | requirement |
|---|---|
| BB-40 | **Form:** `{"v":1, "kind":"gen", "gen":"blade-bake", "genV":1, "set":{…}, "seed":<the oscillator's seed>}` (CONTRACT §3.1). `set` holds: the base wave; every retired row of the corner **away from its default**, keyed by its row key without the oscillator prefix, valued in normalised v (the canonical number form of §3.1); the `ratios` text if it is not the default; the unison U and the spread rows of BB-1 (option 2, ADR-406); the frame axis (BB-20: none, the member frames, or the row, its start and swing, or `rotate`); and `fRef`. **`set` keeps the source blade rows** (ADR-406): they define the table's id, and the recipe is the one place a baked preset carries its source blade values. Deterministic, self-contained: it rebuilds alone, in the build repo, on the frozen engine (WT-42). **The recipe keys the library file** (ADR-406): the table's id is the recipe's hash (CONTRACT §3.1), and the baked table lives as a library file (WT-45: recipe and spectra) on the human's machine, which the baked presets reference. No `blade-bake` generator ships, so the plug-in never rebuilds one: it plays the library copy by id (CONTRACT §3.3 item 2), a fallback not reported for `blade-bake` ids. If that library file is deleted, the corner falls to its default factory table, and the load report names it (WT-45 and WT-F30's one exception, ADR-406). No new table kind is added; what ships in 1.0, and in what form, is ruled at the factory patch session. |
| BB-41 | **Size:** at most **2.5 KiB** of canonical text (ADR-390, P6; it was 3 KiB). **Keys are the rows' block-relative ids in hex** (frozen by RC-9.1), not their names: named keys average about 37 to 40 bytes a row and would reach about 2.9 to 3.1 KiB at worst (the critic's estimate). With id keys a row is at most about 32 bytes (a 3-digit key, quotes and colon, and a value of up to 22 characters in the canonical shortest form), so all 68 retired rows are at most about 2.2 KiB, a `ratios` text at most 256 bytes and the header and frame axis about 150 bytes: **about 2.6 KiB at worst**. **S0 measured 2,081 bytes** on the real 68 rows (2,337 with the engine's 512-byte `ratios`), and named keys at 2,828, so 2.5 KiB holds the worst case and BB-F8's plant fires (S0-RESULTS §4). The patch bound becomes: 64 recipes, each at most 512 bytes, or 2.5 KiB for a `blade-bake` recipe, 32 KiB in all (CONTRACT §3.2). The source presets have two oscillators, so a baked preset needs at most 8 bake recipes: at most 20 KiB. S0's measure has none of option 2's live rows (U and the spread rows, BB-40); BB-F8 measures them (ADR-406). The recipe still carries the blade rows (BB-40), so the 2.5 KiB bound and BB-F8 stand. Open for the next round (CONTRACT §3.2): the `ratios` limit (256 bytes in the spec, 512 in the engine) and the number form ("up to 22 characters" above is the state writer's printf form, not §3.1's). |
| BB-42 | **The mapped rows** (BB-31, BB-32, the frame routes, and BB-20 (a)'s `wt.frame`, `wt.frameSpread` and `spreadLaw`) are written into the baked preset's corners and modulation state by the bake's state writer, as ordinary values; they are not part of the recipe. **The retired rows are not written as rows** (ADR-406): the bake does not write them, and a baked preset carries its source blade values only inside its tables' recipes (BB-40's `set`); the archive holds the source bank, and the bake report maps each baked preset to its source. The baked presets are written as version-2 states when IF-4's version 2 exists (S8). |

---

## 7. Where it runs

| id | requirement |
|---|---|
| BB-50 | **Once, offline, in the build repo** (ADR-406): the lead's bake, in the build repo's tools beside `s0-1` to `s0-3`, on the frozen clean-room engine, over the 83 bank presets and the 16 demos. Its outputs (the tables, the baked presets, the bake report) are frozen data (BB-62). **Never in the plug-in:** no load-time bake, no migration and no port ships, and no blade-era state loads in the wavetable plug-in. The cycles are rendered now; the baked presets are heard on the wavetable build as soon as it plays tables (S4) and written as version-2 states at S8 (BB-42). |
| BB-51 | **Cost:** the bake's run time is reported, never gated (ADR-406: it runs once, offline). Identical recipes bake once. |

---

## 8. What ships, versions and goldens

| id | requirement |
|---|---|
| BB-60 | **Nothing ships** (CONTRACT §7 J8, as amended by ADR-406; the focused C++ port it recommended is withdrawn). No bake code, port or load-time migration is in the plug-in. **The frozen clean-room engine stays in the build repo** (`vendor/engine2/`, read-only) as the generator (BB-12), and the bake tool beside it; the archive `blade-engine-final` (ADR-406) keeps the engine at `6cd98b5`. The baked tables are library files (BB-40); only what the factory patch session keeps ships in 1.0. The blade code in the new engine stays through S8 as WT-19's oracle and leaves at the retirement round (§7 I22); nothing here needs it then. |
| BB-61 | **Versions.** One bake, `genV` 1 (ADR-406). A re-bake (a change of the frozen engine, f_ref, the axis rule or the render) is a new `genV` and a human ruling; its tables are new ids, and the old ones stay as data. No `genV` needs to stay buildable in the plug-in: none is there. |
| BB-62 | **Goldens:** the bake's outputs are frozen data (ADR-406). The spectra hash, on the reference build, of every baked table (every oscillator and corner of the 83 bank presets and the 16 demos), and of a fixed grid of single-blade settings; coefficients within 10⁻⁶ of peak on any build (WT-43). The bake report records the frozen engine's commit, the bake tool's commit and these hashes. |

---

## 9. Gates

Plants: **(D)** detector, proven engine-free at S0; **(M)** mutation, run at the stage's close. All PROPOSED. Each gate closes where what it needs exists (ADR-406). The bake's own gates, BB-F1, F2, F3 (a), F5, F6, F7, F8 and F13, close **S6c**, the lead's stage (as S0 is), offline in the build repo on the bake's outputs, BB-F13 first. BB-F11 (by ear, reported) runs at **S7**, where the real swarm plays the tables at the preset's U. BB-F3 (b), F4 and F10 close at **S8**, where the baked presets are states with their routes. BB-F9 and BB-F12 are deleted (ADR-406).

| id | target | method | plants | control |
|---|---|---|---|---|
| BB-F1 | **Determinism:** one recipe, two runs, two threads: bit-identical spectra | compare | (M) a draw from an unseeded stream in the noise blade | two runs |
| BB-F2 | **Band-limit accuracy:** harmonics 1 to 1,024 at 8,192 samples per cycle within 10⁻⁵ of peak of a 16,384-sample render | compare | (M) 1,024 samples per cycle: the band edge falls near harmonic 480 | the render against itself |
| BB-F3 | **The base-wave control.** (a) At S6c: a patch with every blade off bakes to the base wave's band-limited Fourier series (with WT-20's polarity) within 10⁻⁵ of peak. (b) **At S8:** the baked preset's held C4 on the wavetable build against the frozen engine's render (ADR-406), by **DT-3's method: the first 16 harmonics below 18 kHz within 0.5 dB** (TEST-PLAN Q-10) | engine-free series; renders | (M) the base wave option read as the next in its list, so a triangle bakes as a saw: **harmonic 2** is the detector (a triangle has none, a saw's is −6 dB from its fundamental); harmonic 3 is only about 6 dB off | the bake against its own series |
| BB-F4 | **Static blades at rest** (at S8; re-aimed per frame, not narrowed, ADR-406). For every bank corner classed static (BB-20 (a)): each of frames k0 to k0 + U − 1 played alone (one member, U = 1, at that frame's position, frame spread 0, the morph off) on the wavetable build against the frozen engine's member at that frame's position (BB-F13's tap); at a U = 1 corner, as before, the baked preset's held C4 at that corner (one member, the morph off) against the frozen engine's. DT-3's method, 0.5 dB, judged; every other corner reported | renders | (M) blade 2 dropped from the bake: two-blade static corners fail; (M) at U above 1, member j's cycle written to frame k0 + j + 1: every corner whose members differ fails | a single-blade corner baked twice, one at U = 1 and one at U above 1: the gate against the frozen engine passes on both |
| BB-F5 | **Goldens** (BB-62) | hashes; coefficients | (M) one constant of the frozen copy nudged by an ulp | the golden build against itself |
| BB-F6 | **The bake report is complete** (BB-30) | every retired row away from default in the bank and the demos is checked against the bake report | (M) one mapping line deleted: its row must appear as not carried, never vanish | a blade-free patch: an empty report |
| BB-F7 | **Table gates** (WT-47), single-frame declared tables allowed | WT-47's checks | (D) a bake of a corner whose crush leaves a near-silent frame: refused and reported | the bank's bakes pass |
| BB-F8 | **Size** (BB-41) | the worst-case recipe, every row away from default | (D) the full row set written uncompacted (named keys: 2,828 bytes, over 2.5 KiB) | the bank's recipes |
| BB-F9 | Deleted (ADR-406): nothing bakes at load, so no bake can reach the audio thread. | — | — | — |
| BB-F10 | **A blade envelope reaches every frame** (BB-20 (b); at S8): on Blade pluck (a bank preset with an ENV 2 route to `cut`), the baked preset's `wt.frame` crosses 0 to 1 as the envelope runs its full swing, and its frame-by-frame spectra track the old engine's at matching envelope levels within 1 dB per harmonic (PROPOSED) | observation (IF-10); renders | (M) the route kept at its old depth: the frame stops near 0.3 | a static corner: the frame stays at 0 |
| BB-F11 | **Swarm presets, reported** (option 2, ADR-406; at S7, by ear): the frozen engine at the preset's own U against the baked tables at the same U on the wavetable build, with BB-20 (a)'s rows set through the S7 harness (the baked presets are states only at S8), by DT-3's method and by ear; reported | renders | — | a U = 1 corner's table against the frozen engine's U = 1 render |
| BB-F12 | Deleted (ADR-406): there is no port; the frozen engine is the generator (BB-12). | — | — | — |
| BB-F13 | **The tap isolates one member** (PROPOSED, its tolerance, plant and control listed in CONTRACT §7 C3k; BB-12; option 2, ADR-406; the bake build's first task, closed before any table is baked): at U above 1, the tap's cycle for member i against the engine's own render of member i at the preset's U under the gradient law (BB-11), every member of every bank and demo corner at U above 1: within 10⁻⁶ of peak per harmonic | compare, in the build repo | (M) member i's cycle taken from member i + 1 | a U = 1 corner: the isolated member against the mono member sum |
